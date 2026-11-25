# Cheatsheet 08: Production PyTorch Snippets & Patterns

> **Cheatsheet ID:** `05-ai/machine-learning/18-cheatsheets/08-pytorch-snippets`  
> **Target Version:** PyTorch 2.4+, Python 3.11+  
> **Status:** Production Verified

---

## 1. Tensor Manipulation & Memory Essentials

```python
import torch

# 1. Device Placement & Memory Contiguity
device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
t = torch.randn(32, 128, device=device, dtype=torch.float32)

# Reshape vs View: view() requires contiguous memory; reshape() copies if non-contiguous
t_transposed = t.transpose(0, 1) # Non-contiguous view
t_reshaped = t_transposed.contiguous().view(128, 32) # Safe view
# Equivalent shorthand: t_transposed.reshape(128, 32)

# 2. Squeeze & Unsqueeze
x = torch.randn(128)          # Shape: [128]
x_expanded = x.unsqueeze(0)   # Shape: [1, 128] (Add batch dimension)
x_original = x_expanded.squeeze(0) # Shape: [128]

# 3. Gather & Scatter (Standard in Multi-Task Classification & DQN)
# Gather action Q-values: Q(s, a) where actions has shape [Batch, 1]
q_values = torch.randn(32, 4) # 32 samples, 4 discrete actions
actions = torch.randint(0, 4, (32, 1))
chosen_q = q_values.gather(dim=1, index=actions) # Shape: [32, 1]
```

---

## 2. Custom Dataset & Collate Function

```python
import torch
from torch.utils.data import Dataset, DataLoader

class TabularDataset(Dataset):
    def __init__(self, X: torch.Tensor, y: torch.Tensor):
        self.X = X.float()
        self.y = y.float()

    def __len__(self) -> int:
        return self.X.shape[0]

    def __getitem__(self, idx: int) -> tuple[torch.Tensor, torch.Tensor]:
        return self.X[idx], self.y[idx]

def custom_collate_fn(batch):
    """Custom batch assembly handling variable length or dictionary inputs."""
    xs = torch.stack([item[0] for item in batch], dim=0)
    ys = torch.stack([item[1] for item in batch], dim=0)
    return {"features": xs, "targets": ys}

# Production DataLoader with hardware pin_memory for fast Host-to-Device DMA transfers
loader = DataLoader(
    dataset=TabularDataset(torch.randn(1000, 10), torch.randint(0, 2, (1000, 1))),
    batch_size=64,
    shuffle=True,
    num_workers=4,
    pin_memory=True if torch.cuda.is_available() else False,
    collate_fn=custom_collate_fn
)
```

---

## 3. High-Performance Training Loop (AMP + GradScaler + Clipping)

```python
import torch
import torch.nn as nn
from torch.amp import autocast, GradScaler

def train_one_epoch(
    model: nn.Module,
    dataloader: DataLoader,
    optimizer: torch.optim.Optimizer,
    loss_fn: nn.Module,
    scaler: GradScaler,
    device: torch.device,
    grad_accum_steps: int = 2
) -> float:
    model.train()
    running_loss = 0.0
    optimizer.zero_grad(set_to_none=True) # set_to_none=True avoids zeroing memory writes

    for step, batch in enumerate(dataloader):
        features = batch["features"].to(device, non_blocking=True)
        targets = batch["targets"].to(device, non_blocking=True)

        # Automatic Mixed Precision (AMP) using FP16 / BF16
        with autocast(device_type=device.type, dtype=torch.bfloat16 if torch.cuda.is_bf16_supported() else torch.float16):
            outputs = model(features)
            loss = loss_fn(outputs, targets)
            loss = loss / grad_accum_steps # Normalize for gradient accumulation

        # Scale loss and backpropagate
        scaler.scale(loss).backward()

        if (step + 1) % grad_accum_steps == 0 or (step + 1) == len(dataloader):
            # Unscale gradients before clipping
            scaler.unscale_(optimizer)
            torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)

            # Step optimizer & update scaler
            scaler.step(optimizer)
            scaler.update()
            optimizer.zero_grad(set_to_none=True)

        running_loss += loss.item() * grad_accum_steps

    return running_loss / len(dataloader)
```

---

## 4. Atomic Model Checkpointing & Resumption

```python
import os
import torch

def save_checkpoint(state: dict, filepath: str):
    """Atomically writes checkpoint using a temporary file to prevent corruption."""
    temp_path = f"{filepath}.tmp"
    torch.save(state, temp_path)
    os.replace(temp_path, filepath)

def load_checkpoint(filepath: str, model: torch.nn.Module, optimizer=None, scheduler=None, device="cpu"):
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"Checkpoint {filepath} not found.")

    checkpoint = torch.load(filepath, map_location=device, weights_only=True)
    model.load_state_dict(checkpoint["model_state_dict"])
    if optimizer and "optimizer_state_dict" in checkpoint:
        optimizer.load_state_dict(checkpoint["optimizer_state_dict"])
    if scheduler and "scheduler_state_dict" in checkpoint:
        scheduler.load_state_dict(checkpoint["scheduler_state_dict"])
    return checkpoint.get("epoch", 0), checkpoint.get("best_score", None)
```

---

## 5. Custom Module with He/Kaiming Initialization & Buffers

```python
import torch
import torch.nn as nn

class CustomMLP(nn.Module):
    def __init__(self, in_features: int, hidden_dim: int, out_features: int):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(in_features, hidden_dim),
            nn.LayerNorm(hidden_dim),
            nn.GELU(),
            nn.Linear(hidden_dim, out_features)
        )
        # Register persistent non-parameter tensor (e.g. running step or temperature)
        self.register_buffer("temperature", torch.tensor(1.0))
        self._init_weights()

    def _init_weights(self):
        for m in self.modules():
            if isinstance(m, nn.Linear):
                # Kaiming / He normal initialization for ReLU/GELU activations
                nn.init.kaiming_normal_(m.weight, mode="fan_in", nonlinearity="relu")
                if m.bias is not None:
                    nn.init.zeros_(m.bias)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        logits = self.net(x)
        return logits / self.temperature
```
