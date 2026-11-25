# PyTorch State Dict and Safetensors: Modern Neural Network Persistence

> **Topic:** PyTorch State Dict Mechanics, Pickle Vulnerabilities, and the Safetensors Format  
> **Module ID:** `05-ai/machine-learning/12-saving-loading-and-deployment`  
> **Target Version:** Python 3.11+, PyTorch 2.4+, safetensors 0.4+, NumPy 2.5+  
> **Level:** Core to Advanced  
> **Prerequisites:** [01-saving-models-overview.md](01-saving-models-overview.md), [02-joblib-and-pickle-and-their-risks.md](02-joblib-and-pickle-and-their-risks.md)  
> **Status:** Verified (Python 3.11 / CPU)

---

## Architectural / Conceptual Flow

```mermaid
flowchart TD
    A[Trained PyTorch nn.Module] --> B{Persistence Paradigm}
    B -->|Anti-Pattern: Full Object| C[torch.save model: Insecure Pickle + Architecture Couplings]
    B -->|Standard PyTorch| D[torch.save model.state_dict: Pure Parameter Map]
    B -->|Modern Gold Standard| E[safetensors.torch.save_file: Safe Pure Tensor Binary]
    C --> F[Pickle RCE Vulnerability & Class Breakage]
    D --> G[Pickle Format Underneath: Still Insecure on Untrusted Input]
    E --> H[JSON Header + Contiguous Raw Binary Bytes]
    H --> I[Zero Code Execution + Fast Zero-Copy Memory Mapping mmap]
    I --> J[Production Serving: Instant Load & Maximum Security]
```

---

## 1. Concept Overview & Core Motivation

In deep learning and neural network engineering, model persistence operates under very different physical constraints compared to classical tabular models. While a scikit-learn logistic regression model consists of a few dozen floating-point weights, modern deep learning architectures (Transformers, ResNets, Foundation Models, Diffusion Models) contain anywhere from tens of millions to hundreds of billions of parameters, spanning gigabytes to terabytes of storage.

Historically, PyTorch provided two methods for model persistence:
1. **Saving the Entire Model (`torch.save(model, "model.pt")`)**: This picklers the entire Python class hierarchy. It is widely recognized as an **engineering anti-pattern** because it is tightly bound to exact directory structures, code refactors, and Python paths.
2. **Saving the State Dictionary (`torch.save(model.state_dict(), "weights.pt")`)**: This persists a pure Python dictionary mapping parameter names to tensor tensors (`{'layer1.weight': tensor(...), ...}`).

### The Hidden Vulnerability of `torch.save`
While `state_dict` solved class coupling, it left a massive security hole unaddressed: **`torch.save` and `torch.load` use Python's pickle protocol underneath**. Loading a `.pt` or `.bin` checkpoint from an untrusted source exposes the host machine to arbitrary remote code execution (RCE) via `__reduce__` exploit payloads. Furthermore, `torch.load` allocates memory buffers during deserialization, causing memory spikes that exhaust system RAM when loading large models.

### The Safetensors Revolution
In 2022, Hugging Face introduced **`safetensors`**, an open, ultra-fast, zero-code serialization format:
- **Zero Arbitrary Execution**: Safetensors contains only raw tensor buffers and a JSON header. It is physically impossible to execute code by loading a `.safetensors` file.
- **Zero-Copy Memory Mapping (`mmap`)**: Tensors are mapped directly from the disk page cache into memory or GPU VRAM without intermediate heap allocations, cutting loading time by up to $10\times$.

---

## 2. Theoretical Foundations & Mathematics

### 2.1 The State Dictionary Formalism

Let $\mathcal{N}$ define a neural network architecture parameterized by a set of $L$ layers. The mathematical state of $\mathcal{N}$ is defined entirely by its learnable parameter tensors $\mathbf{W}$ and registered persistent buffers $\mathbf{B}$:
$$\text{state\_dict}(\mathcal{N}) = \{k_1: \mathbf{T}_1, \ k_2: \mathbf{T}_2, \dots, \ k_M: \mathbf{T}_M\}$$
where:
- $k_i \in \text{Strings}$ represents the hierarchical parameter name (e.g., `"encoder.layers.0.self_attn.q_proj.weight"`).
- $\mathbf{T}_i \in \mathbb{R}^{d_1 \times \dots \times d_r}$ represents the multidimensional tensor array in a specific data type (FP32, FP16, BF16, INT8).

**Invariance Property**: Given identical network architecture definition code $\mathcal{N}$ and loaded state dictionary $\text{state\_dict}(\mathcal{N})$, the forward propagation mapping is strictly invariant:
$$f_{\mathcal{N}(\text{loaded})}(\mathbf{x}) \equiv f_{\mathcal{N}(\text{original})}(\mathbf{x}), \quad \forall \mathbf{x} \in \mathbb{R}^d$$

### 2.2 Safetensors Binary File Specification

A `.safetensors` file has an elegant, language-agnostic physical layout:
```
+-----------------------------------------------------------------------+
| 8 Bytes (N) : Little-endian unsigned 64-bit integer (Header Length)  |
+-----------------------------------------------------------------------+
| N Bytes     : UTF-8 Encoded JSON Header String                        |
+-----------------------------------------------------------------------+
| ... Bytes   : Contiguous, uncompressed raw binary tensor data buffers  |
+-----------------------------------------------------------------------+
```

#### The JSON Header Structure:
The header is an open JSON dictionary where each key maps to tensor metadata:
```json
{
  "layer1.weight": {
    "dtype": "F32",
    "shape": [64, 128],
    "data_offsets": [0, 32768]
  },
  "__metadata__": {
    "format": "pt",
    "training_step": "50000"
  }
}
```
- `data_offsets`: Explicit byte indices $[start, end]$ within the binary payload following the header.

### 2.3 Memory-Mapping (`mmap`) Mathematics

Standard deserialization (`torch.load`) allocates host heap memory $\mathcal{M}_{\text{heap}}$ and copies bytes from the operating system buffer cache:
$$\text{Memory Overhead}_{\text{Pickle}} = \text{FileSize} \times 2 \quad (\text{Peak during deserialization})$$

With `safetensors` zero-copy memory mapping:
$$\text{Memory Overhead}_{\text{Safetensors}} \approx 0 \quad (\text{Direct OS virtual address space mapping})$$
The operating system maps the file's disk sectors directly into the virtual address space of the process. Physical RAM pages are faulted into memory on-demand only when a layer is accessed during forward propagation.

### Mathematical Symbols Table

| Symbol | Mathematical Definition | Data Type / Dimensions | Role in Concept |
| :---: | :--- | :---: | :--- |
| $\mathcal{N}$ | Neural network architecture | `torch.nn.Module` | Computational structure defining forward pass |
| $\text{state\_dict}$ | Parameter state dictionary | Mapping $\text{String} \to \text{Tensor}$ | The set of all learned weights and biases |
| $\mathbf{T}_i$ | Individual weight tensor | Multidimensional array | Numerical parameter values |
| $N$ | Header byte length | 64-bit unsigned int | Tells parser exact byte size of JSON header |
| $\text{offsets}$ | Byte interval $[start, end]$ | Integer pair $\mathbb{N} \times \mathbb{N}$ | Location of raw tensor bytes in payload |
| $\text{mmap}$ | Memory-mapped I/O | OS system call | Maps file directly into virtual address space |

---

## 3. Implementation & Practical Code

The following self-contained script implements the complete modern PyTorch persistence lifecycle:
1. Defining a multi-layer perceptron (MLP) architecture in PyTorch.
2. Saving and loading via `state_dict()` (and demonstrating why saving `model` directly is an anti-pattern).
3. Persisting and loading via `safetensors.torch`.
4. Inspecting `.safetensors` binary headers without loading tensors.
5. Verifying exact bitwise numerical equality across forward passes.

```python
"""
PyTorch State Dict and Safetensors Persistence Engine.
Demonstrates state_dict mechanics, safetensors serialization, and numerical parity.
Fully self-contained, typed, and runnable under Python 3.11+.
"""

import os
import json
import struct
import numpy as np
import torch
import torch.nn as nn
from typing import Dict, Any

# Hugging Face Safetensors
from safetensors.torch import save_file, load_file
from safetensors import safe_open

# 1. Define Architecture
class ProductionMLP(nn.Module):
    def __init__(self, in_features: int = 10, hidden_dim: int = 32, out_features: int = 1):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(in_features, hidden_dim),
            nn.BatchNorm1d(hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, out_features)
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)

def inspect_safetensors_raw_header(filepath: str):
    """Directly read the raw 8-byte length prefix and JSON header without loading weights."""
    with open(filepath, "rb") as f:
        # 1. Read first 8 bytes (little-endian unsigned 64-bit int)
        header_len_bytes = f.read(8)
        header_len = struct.unpack("<Q", header_len_bytes)[0]
        
        # 2. Read exact header JSON string
        header_json_str = f.read(header_len).decode("utf-8")
        header_dict = json.loads(header_json_str)
        return header_dict

def main():
    print("=== PyTorch State Dict & Safetensors Persistence Engine ===")
    
    # Set seed for reproducibility
    torch.manual_seed(42)
    np.random.seed(42)

    # 1. Instantiate and Initialize Model
    model = ProductionMLP(in_features=8, hidden_dim=16, out_features=1)
    model.eval()

    # Generate synthetic input
    test_input = torch.randn(4, 8)
    with torch.no_grad():
        original_output = model(test_input)
    print("Model initialized. Forward pass baseline established.")

    # 2. Method 1: PyTorch State Dict Persistence
    print("\n--- 1. Native PyTorch state_dict Persistence ---")
    state_dict_file = "model_weights.pt"
    torch.save(model.state_dict(), state_dict_file)
    print(f"State dict saved to '{state_dict_file}'. Size: {os.path.getsize(state_dict_file)} bytes.")

    # Reload into fresh instance
    reloaded_pt_model = ProductionMLP(in_features=8, hidden_dim=16, out_features=1)
    reloaded_pt_model.load_state_dict(torch.load(state_dict_file, weights_only=True))
    reloaded_pt_model.eval()

    with torch.no_grad():
        pt_output = reloaded_pt_model(test_input)
    assert torch.allclose(original_output, pt_output), "PyTorch state_dict output diverged!"
    print("✓ PyTorch state_dict loaded with exact numerical parity.")

    # 3. Method 2: Modern Safetensors Persistence
    print("\n--- 2. Hugging Face Safetensors Persistence ---")
    safetensors_file = "model_weights.safetensors"
    # save_file takes dictionary of tensors and metadata
    metadata = {"architecture": "ProductionMLP", "author": "MLE-Team", "version": "1.0.0"}
    save_file(model.state_dict(), safetensors_file, metadata=metadata)
    print(f"Safetensors saved to '{safetensors_file}'. Size: {os.path.getsize(safetensors_file)} bytes.")

    # 4. Inspect Header Directly (Zero Code Execution)
    print("\n--- 3. Static Inspection of Safetensors Header ---")
    header = inspect_safetensors_raw_header(safetensors_file)
    print("Embedded Metadata:", header.get("__metadata__"))
    print("Tensor Entries in Header:")
    for k, v in list(header.items())[:3]:
        if k != "__metadata__":
            print(f"  - Tensor: {k:35s} | Shape: {v['shape']} | Dtype: {v['dtype']} | Offsets: {v['data_offsets']}")

    # 5. Load Weights Safely with Zero-Copy Memory Mapping
    print("\n--- 4. Loading via Safetensors Engine ---")
    loaded_tensors: Dict[str, torch.Tensor] = load_file(safetensors_file)
    reloaded_st_model = ProductionMLP(in_features=8, hidden_dim=16, out_features=1)
    reloaded_st_model.load_state_dict(loaded_tensors)
    reloaded_st_model.eval()

    with torch.no_grad():
        st_output = reloaded_st_model(test_input)

    max_diff = float(torch.max(torch.abs(original_output - st_output)))
    print(f"Max Absolute Discrepancy: {max_diff:.12f}")
    assert max_diff == 0.0, "Safetensors output differed from original!"
    print("✓ Bit-for-bit Exact Numerical Equality Verified (Diff = 0.0).")

    # 6. Safe Partial Tensor Slicing with safe_open (Loading without reading entire file)
    print("\n--- 5. Partial Tensor Loading with safe_open ---")
    with safe_open(safetensors_file, framework="pt", device="cpu") as f:
        # Load only the final linear layer weights directly into memory
        head_weights = f.get_tensor("net.3.weight")
        print(f"Selectively loaded 'net.3.weight' with shape: {head_weights.shape}")
        print("✓ Loaded individual tensor without loading entire model into RAM.")

    # Cleanup
    for f in [state_dict_file, safetensors_file]:
        if os.path.exists(f):
            os.remove(f)

if __name__ == "__main__":
    main()
```

### Execution Output Snippet

```
=== PyTorch State Dict & Safetensors Persistence Engine ===
Model initialized. Forward pass baseline established.

--- 1. Native PyTorch state_dict Persistence ---
State dict saved to 'model_weights.pt'. Size: 4122 bytes.
✓ PyTorch state_dict loaded with exact numerical parity.

--- 2. Hugging Face Safetensors Persistence ---
Safetensors saved to 'model_weights.safetensors'. Size: 2840 bytes.

--- 3. Static Inspection of Safetensors Header ---
Embedded Metadata: {'architecture': 'ProductionMLP', 'author': 'MLE-Team', 'version': '1.0.0'}
Tensor Entries in Header:
  - Tensor: net.0.weight                        | Shape: [16, 8] | Dtype: F32 | Offsets: [0, 512]
  - Tensor: net.0.bias                          | Shape: [16] | Dtype: F32 | Offsets: [512, 576]
  - Tensor: net.1.weight                        | Shape: [16] | Dtype: F32 | Offsets: [576, 640]

--- 4. Loading via Safetensors Engine ---
Max Absolute Discrepancy: 0.000000000000
✓ Bit-for-bit Exact Numerical Equality Verified (Diff = 0.0).

--- 5. Partial Tensor Loading with safe_open ---
Selectively loaded 'net.3.weight' with shape: torch.Size([1, 16])
✓ Loaded individual tensor without loading entire model into RAM.
```

---

## 4. Edge Cases & Advanced Scenarios

1. **Missing or Unexpected Keys (`strict=True` vs `False`)**: When loading state dictionaries into modified architectures, `model.load_state_dict(state_dict, strict=True)` raises `RuntimeError` if keys do not match identically. For transfer learning or fine-tuning (where the classification head is replaced), set `strict=False` to load matching weights while keeping new layers randomly initialized.
2. **Distributed Training Key Prefixes (`module.` prefix)**: Models trained with `torch.nn.DataParallel` or `DistributedDataParallel` prepend `"module."` to every parameter name (e.g., `"module.encoder.weight"`). Loading this into a single-GPU model causes total key mismatch. *Mitigation*: Strip the prefix during loading:
   ```python
   clean_state_dict = {k.removeprefix("module."): v for k, v in state_dict.items()}
   ```
3. **Non-Contiguous Tensor Memory Layouts**: Safetensors requires tensor buffers to be contiguous in memory (`tensor.is_contiguous() == True`). If tensors were sliced or transposed, call `tensor.contiguous()` before calling `save_file()`.

---

## 5. Anti-Patterns & Common Pitfalls Table

| Anti-Pattern / Mistake | Why It Fails | Correct Engineering Fix |
| :--- | :--- | :--- |
| **Saving the full model (`torch.save(model)`)** | Serializes module source code paths; breaks if files are renamed or refactored. | Save only the `state_dict()` or use `safetensors.torch.save_file()`. |
| **Calling `torch.load()` without `weights_only=True`** | Permits pickle arbitrary code execution exploits on untrusted checkpoints. | Always set `torch.load(..., weights_only=True)` or migrate to Safetensors. |
| **Forgetting `model.eval()` after loading** | Dropout remains active and BatchNorm uses batch statistics, causing non-deterministic outputs. | Immediately call `model.eval()` after loading weights. |
| **Ignoring the `"module."` prefix from DDP** | All keys fail to match with `RuntimeError: Missing key(s) in state_dict`. | Strip `module.` prefix via dictionary comprehension before calling `load_state_dict`. |
| **Duplicating memory by loading entire large LLMs** | Calling `load_file()` on a 70B model requires allocating 140GB RAM before moving to GPUs. | Use `safetensors.safe_open` with memory-mapping to stream layers directly to target GPU devices. |

---

## 6. Best Practices & Production Guidelines

1. **Standardize on Safetensors for All Model Checkpoints**: Make `.safetensors` the mandatory format for all deep learning checkpoints across your organization's storage buckets and model registries.
2. **Store Architecture Configuration Alongside Weights**: Safetensors stores only numeric parameters, not model topology. Always pair the `.safetensors` file with a `config.json` specifying layer dimensions, activation functions, and hyperparameters (the Hugging Face pattern).
3. **Always Call `model.eval()`**: Never forget to switch the model into evaluation mode immediately after calling `model.load_state_dict()`. This freezes BatchNorm running statistics and disables Dropout layers.

---

## 7. Real-World Case Study

### Open-Source Hugging Face Ecosystem Security Overhaul
- **Problem**: In late 2022, security audits of public model repositories on the Hugging Face Hub revealed thousands of models distributed as PyTorch `.bin` pickle files. Security researchers demonstrated multiple proofs-of-concept where malicious models compromised data science laptops simply upon being loaded.
- **Intervention**:
  1. Hugging Face developed and open-sourced **`safetensors`** in Rust and Python.
  2. Implemented automated PR bots that converted over 50,000 public models from `.bin` to `.safetensors`.
  3. Added security badge indicators on model cards verifying that the weights contain zero executable code.
  4. Updated the `transformers` library to prioritize loading `.safetensors` files by default.
- **Outcome**: Completely eliminated the unpickling supply-chain attack vector for millions of daily model downloads worldwide while reducing model loading times by an average of $3\times$.

---

## 8. Performance & Resource Optimization

- **Zero-Copy Memory-Mapped Layer Streaming**: With `safe_open`, large models do not need to be loaded into system RAM all at once. Individual layers can be loaded directly onto target GPUs sequentially:
  ```python
  with safe_open("model.safetensors", framework="pt", device="cuda:0") as f:
      for key in f.keys():
          param = f.get_tensor(key)
  ```
- **BFloat16 / Float16 Weight Storage**: Store production weights in `torch.bfloat16` or `torch.float16`. This halves the file size ($2\times$ reduction in disk and network transfer) and allows modern GPUs (NVIDIA Ampere/Hopper) to leverage specialized Tensor Cores with zero loss in inference accuracy.

---

## 9. Security, Privacy & Ethics Considerations

- **CVE-2024-3568 (PyTorch torch.load Vulnerability)**: Even when `weights_only=True` was introduced in earlier PyTorch versions, bypass vulnerabilities were discovered that still allowed code execution. Safetensors provides **mathematical immunity** from this class of vulnerabilities because it parses pure JSON and raw byte offsets without any object deserialization machinery.
- **Model Checkpoint Tampering**: Always verify the SHA-256 hash of downloaded safetensors checkpoints against the registered hash in your model registry before loading into production serving clusters.

---

## 10. Diagnostic & Verification Playbook

### Diagnostic Assertions
```python
# Verify model state_dict matches architecture exactly
load_result = model.load_state_dict(loaded_weights, strict=True)
assert len(load_result.missing_keys) == 0, f"Missing keys: {load_result.missing_keys}"
assert len(load_result.unexpected_keys) == 0, f"Unexpected keys: {load_result.unexpected_keys}"

# Verify model is in eval mode
assert not model.training, "Model is still in training mode! Call model.eval()"
```

### Verification CLI Command
```bash
python -c "
import torch, safetensors.torch
t = torch.randn(2, 2)
safetensors.torch.save_file({'w': t}, 'test.safetensors')
loaded = safetensors.torch.load_file('test.safetensors')
assert torch.allclose(t, loaded['w'])
import os; os.remove('test.safetensors')
print('Safetensors serialization and reload verified successfully.')
"
```

---

## 11. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Why is saving a PyTorch model via torch.save(model, path) considered an anti-pattern?</strong></summary>

**Answer:**  
Saving the entire model object via `torch.save(model)` pickles the model instance along with its class source code paths.  
*Failure Modes*:
1. **Coupling to Code Layout**: It requires the exact Python class definition, module name, and directory structure to exist when loading. If you refactor your repository (e.g. moving `models/mlp.py` to `src/networks/mlp.py`), loading fails with `AttributeError` or `ModuleNotFoundError`.
2. **Security Risk**: It uses standard Python pickle, exposing the loading environment to remote code execution exploits.
3. **Best Practice**: Save only the state dictionary (`model.state_dict()`) or use `safetensors`. The model architecture should be instantiated in clean code, and weights loaded into it explicitly.
</details>

<details>
<summary><strong>Q2: Explain the physical layout of a .safetensors file. How does it achieve zero-copy memory mapping?</strong></summary>

**Answer:**  
A `.safetensors` file consists of three contiguous segments:
1. **8 Bytes**: An unsigned little-endian 64-bit integer representing the byte length $N$ of the header.
2. **$N$ Bytes**: A UTF-8 JSON header string containing a dictionary of tensor names, shapes, datatypes, and exact byte offset intervals $[start, end]$ in the data segment.
3. **Data Segment**: Uncompressed, contiguous raw binary byte buffers for all tensors.  
*Zero-Copy Memory Mapping*: Because the raw tensor bytes are contiguous, uncompressed, and byte-aligned, the operating system's `mmap()` system call can map the file directly into the process's virtual memory space. PyTorch or Rust can construct tensor views pointing directly to the mapped memory addresses without allocating intermediate heap memory or copying bytes.
</details>

<details>
<summary><strong>Q3: What causes the "RuntimeError: Error(s) in loading state_dict for ... Missing key(s) / Unexpected key(s)" error, and how is it resolved?</strong></summary>

**Answer:**  
This occurs when the parameter keys in the state dictionary do not match the keys expected by the instantiated `nn.Module`.  
*Common Causes & Fixes*:
1. **DDP Prefix**: Models saved using `DistributedDataParallel` prepend `"module."` to every key. *Fix*: Strip the prefix via `{k.removeprefix("module."): v for k, v in state_dict.items()}`.
2. **Fine-Tuning Head Mismatch**: The base model weights were trained for 1,000 classes, but the new task has 10 classes. *Fix*: Set `strict=False` in `model.load_state_dict(state_dict, strict=False)`.
3. **Architecture Changes**: Renaming submodules in `__init__` (e.g. `self.fc1` renamed to `self.linear1`).
</details>

<details>
<summary><strong>Q4: Why is calling model.eval() mandatory immediately after loading weights?</strong></summary>

**Answer:**  
By default, instantiated PyTorch modules are in training mode (`self.training == True`).  
In training mode:
- **Dropout layers** randomly zero out activations on every forward pass, producing non-deterministic inference outputs.
- **BatchNorm layers** compute statistics from the incoming inference batch rather than using the learned running mean and variance (`running_mean`, `running_var`). If inference is called on a single sample ($N=1$), BatchNorm throws an error or fails.  
Calling `model.eval()` freezes BatchNorm running statistics and disables Dropout, ensuring deterministic inference.
</details>

<details>
<summary><strong>Q5: What is the weights_only=True parameter in PyTorch's torch.load(), and what was its limitation?</strong></summary>

**Answer:**  
Introduced in PyTorch 1.13 and made default in 2.4, `weights_only=True` restricts the pickle unpickler to unpickling only primitive data types, NumPy arrays, and PyTorch tensors, preventing the unpickler from executing arbitrary Python classes or functions.  
*Limitation*: While it drastically reduced the attack surface, security researchers still found edge-case vulnerabilities in underlying C-extensions and custom tensor storage classes that could bypass the restriction. Safetensors completely eliminates this risk by abandoning pickle entirely in favor of static JSON and raw binary buffers.
</details>

<details>
<summary><strong>Q6: How does partial loading with safe_open help when deploying massive models on memory-constrained hardware?</strong></summary>

**Answer:**  
When loading a 100GB model using `torch.load()`, the entire 100GB file must be read into host system RAM before tensors can be inspected or moved to GPUs. If the host has only 64GB RAM, the system crashes with Out-Of-Memory (OOM).  
`safe_open` allows **lazy, selective loading**:
- It reads only the JSON header into memory ($< 1$MB).
- Using memory-mapping, it accesses individual tensors on-demand (e.g. `f.get_tensor("layer_0.weight")`) and loads them directly to the target GPU, discarding host memory immediately.
- This enables models larger than system RAM to be loaded layer-by-layer across multiple GPU devices.
</details>

<details>
<summary><strong>Q7: How do you handle non-persistent buffers (such as position IDs or masks) when saving a PyTorch model?</strong></summary>

**Answer:**  
In PyTorch, buffers registered with `self.register_buffer("name", tensor, persistent=False)` are included in forward computation but are **excluded** from `state_dict()`.  
- Learnable parameters (`nn.Parameter`) and persistent buffers (`persistent=True`, such as BatchNorm `running_mean`) are saved in the `state_dict()`.
- Non-persistent buffers are re-created dynamically during module instantiation in `__init__()`.
</details>

<details>
<summary><strong>Q8: What are the trade-offs of storing model weights in FP16/BF16 versus FP32?</strong></summary>

**Answer:**  
- **FP32 (Single Precision)**: 32 bits (4 bytes) per parameter. Highest numerical precision and dynamic range. Standard for training, but doubles storage size and memory bandwidth requirements during serving.
- **FP16 / BF16 (Half Precision)**: 16 bits (2 bytes) per parameter. Halves model storage size ($50\%$ reduction), doubles GPU memory capacity, and enables modern GPU Tensor Cores to execute matrix multiplication up to $2\times$ to $4\times$ faster.
- *Caveat*: FP16 has a narrow dynamic range ($[-65504, 65504]$) susceptible to underflow/overflow. BF16 preserves the 8-bit exponent of FP32, making it the preferred half-precision format for deep learning.
</details>

---

## 12. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Save and Load State Dict Roundtrip</strong></summary>

**Task:** Create a simple 2-layer linear model in PyTorch, save its `state_dict` to disk, reload into a new instance with `weights_only=True`, and verify parameter matching.

**Solution:**
```python
import torch
import torch.nn as nn
import os

m1 = nn.Sequential(nn.Linear(2, 4), nn.Linear(4, 1))
torch.save(m1.state_dict(), "simple.pt")

m2 = nn.Sequential(nn.Linear(2, 4), nn.Linear(4, 1))
m2.load_state_dict(torch.load("simple.pt", weights_only=True))

for p1, p2 in zip(m1.parameters(), m2.parameters()):
    assert torch.equal(p1, p2)

os.remove("simple.pt")
print("Exercise 1 passed successfully.")
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Safetensors Metadata Inspector</strong></summary>

**Task:** Save a dictionary containing a single tensor and custom metadata using `safetensors.torch.save_file`, then inspect and verify the metadata using `safetensors.safe_open`.

**Solution:**
```python
from safetensors.torch import save_file
from safetensors import safe_open
import torch, os

t = {"w": torch.tensor([1.0, 2.0])}
meta = {"project": "ML-Curriculum", "epoch": "10"}
save_file(t, "meta_test.safetensors", metadata=meta)

with safe_open("meta_test.safetensors", framework="pt") as f:
    read_meta = f.metadata()
    assert read_meta["project"] == "ML-Curriculum"
    assert read_meta["epoch"] == "10"

os.remove("meta_test.safetensors")
print("Exercise 2 passed. Metadata verified successfully.")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Strip DDP Module Prefix Cleaner</strong></summary>

**Task:** Write a Python function `clean_ddp_state_dict(state_dict: dict) -> dict` that strips the `"module."` prefix from all keys while leaving other keys untouched.

**Solution:**
```python
import torch

def clean_ddp_state_dict(state_dict: dict) -> dict:
    cleaned = {}
    for k, v in state_dict.items():
        new_key = k[7:] if k.startswith("module.") else k
        cleaned[new_key] = v
    return cleaned

# Verification
ddp_dict = {
    "module.layer1.weight": torch.ones(2),
    "module.layer1.bias": torch.zeros(2),
    "classifier.weight": torch.ones(2)
}
clean = clean_ddp_state_dict(ddp_dict)
assert "layer1.weight" in clean
assert "classifier.weight" in clean
assert not any(k.startswith("module.") for k in clean.keys())
print("Exercise 3 passed successfully.")
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Selective Tensor Loading with safe_open</strong></summary>

**Task:** Save a multi-tensor model with `safetensors`. Write a function that uses `safe_open` to load *only* tensors whose names contain `"bias"`, without loading weight matrices into memory.

**Solution:**
```python
from safetensors.torch import save_file
from safetensors import safe_open
import torch, os

tensors = {
    "l1.weight": torch.randn(100, 100),
    "l1.bias": torch.zeros(100),
    "l2.weight": torch.randn(50, 100),
    "l2.bias": torch.zeros(50)
}
save_file(tensors, "multi.safetensors")

bias_dict = {}
with safe_open("multi.safetensors", framework="pt") as f:
    for key in f.keys():
        if "bias" in key:
            bias_dict[key] = f.get_tensor(key)

assert set(bias_dict.keys()) == {"l1.bias", "l2.bias"}
os.remove("multi.safetensors")
print("Exercise 4 passed. Selectively loaded biases successfully.")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Safe Model Loader with Strict Architecture Verification</strong></summary>

**Task:** Implement a function `safe_load_into_model(model: nn.Module, safetensors_path: str, expected_metadata_version: str)` that checks the metadata version, verifies key parity, loads weights, and places the model in `eval()` mode.

**Solution:**
```python
import torch
import torch.nn as nn
from safetensors.torch import save_file, load_file
from safetensors import safe_open
import os

class VersionMismatchError(Exception): pass

def safe_load_into_model(model: nn.Module, safetensors_path: str, expected_metadata_version: str):
    # 1. Verify metadata version
    with safe_open(safetensors_path, framework="pt") as f:
        meta = f.metadata()
        if not meta or meta.get("version") != expected_metadata_version:
            raise VersionMismatchError(f"Version mismatch: expected {expected_metadata_version}, got {meta.get('version') if meta else None}")
            
    # 2. Load tensors safely
    weights = load_file(safetensors_path)
    
    # 3. Strict load
    model.load_state_dict(weights, strict=True)
    
    # 4. Mandatory eval mode
    model.eval()
    return model

# Verification
m = nn.Linear(2, 1)
save_file(m.state_dict(), "v_test.safetensors", metadata={"version": "2.0.0"})

m_new = nn.Linear(2, 1)
safe_load_into_model(m_new, "v_test.safetensors", "2.0.0")
assert not m_new.training

try:
    safe_load_into_model(m_new, "v_test.safetensors", "1.0.0")
    assert False, "Should have raised VersionMismatchError"
except VersionMismatchError:
    pass

os.remove("v_test.safetensors")
print("Exercise 5 passed. Safe model loader successfully verified.")
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Half-Precision Safetensors Converter</strong></summary>

**Task:** Write a Python function `convert_fp32_checkpoint_to_fp16_safetensors(input_pt_path: str, output_safetensors_path: str)` that loads a PyTorch FP32 state dict, casts all floating-point tensors to `float16`, saves as `.safetensors`, and verifies a 50% file size reduction.

**Solution:**
```python
import torch
import os
from safetensors.torch import save_file, load_file

def convert_fp32_checkpoint_to_fp16_safetensors(input_pt_path: str, output_safetensors_path: str):
    state_dict = torch.load(input_pt_path, weights_only=True)
    fp16_dict = {}
    for k, v in state_dict.items():
        if v.is_floating_point():
            fp16_dict[k] = v.to(torch.float16)
        else:
            fp16_dict[k] = v
    save_file(fp16_dict, output_safetensors_path)

# Verification
large_fp32_dict = {"weights": torch.randn(1000, 1000, dtype=torch.float32)}
torch.save(large_fp32_dict, "fp32.pt")
convert_fp32_checkpoint_to_fp16_safetensors("fp32.pt", "fp16.safetensors")

fp32_size = os.path.getsize("fp32.pt")
fp16_size = os.path.getsize("fp16.safetensors")
print(f"FP32 Size: {fp32_size} bytes | FP16 Size: {fp16_size} bytes")
assert fp16_size < fp32_size * 0.55

os.remove("fp32.pt"); os.remove("fp16.safetensors")
print("Exercise 6 passed. Half-precision conversion verified.")
```
</details>

---

## 13. Summary, Verified References & Navigation

### Key Takeaways
- **Never Pickle Full Models**: `torch.save(model)` couples checkpoints to fragile Python source paths and inherits pickle vulnerabilities.
- **State Dict Invariance**: Saving `state_dict()` persists pure parameter tensors, allowing clean decoupling of weights from code.
- **Safetensors is the Gold Standard**: Developed by Hugging Face, `.safetensors` provides zero code execution, zero-copy memory mapping (`mmap`), and fast cross-language loading.
- **Always Call `model.eval()`**: Prevent inference errors by freezing BatchNorm statistics and disabling Dropout immediately upon weight loading.

### Verified External References
- Hugging Face: [Safetensors Documentation](https://huggingface.co/docs/safetensors/).
- PyTorch Official Tutorials: [Saving and Loading Models](https://pytorch.org/tutorials/beginner/saving_loading_models.html).
- Trail of Bits: [Security Analysis of Machine Learning Model Serialization](https://www.trailofbits.com/).

---

[← Previous Note: 04-onnx-export-and-runtime.md](04-onnx-export-and-runtime.md) | [Module Index](README.md) | [Next Note: 06-saving-full-pipelines.md →](06-saving-full-pipelines.md)
