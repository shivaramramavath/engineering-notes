# Cheatsheet 09: Model Saving, Loading & Export Recipes

> **Cheatsheet ID:** `05-ai/machine-learning/18-cheatsheets/09-saving-and-loading-recipes`  
> **Topic:** Production Serialization Patterns, Security Protocols & Cross-Platform Export Engines  
> **Target Libraries:** joblib 1.4+, skops 0.10+, onnx 1.17+, onnxruntime 1.20+, safetensors 0.5+  
> **Status:** Production Verified

---

## 1. Serialization Security & Performance Matrix

| Serialization Format | Primary Target | Arbitrary Code Exec Risk? | Memory Mapped? | Zero-Copy? | Recommended Production Use |
| :--- | :--- | :---: | :---: | :---: | :--- |
| **`joblib` / `pickle`** | Scikit-Learn | **CRITICAL HIGH** (Executes `__reduce__`) | No | No | Internal offline caching only; never load untrusted inputs. |
| **`skops`** | Scikit-Learn | **SAFE** (Zero code execution) | No | No | Secure Scikit-Learn model sharing across untrusted networks. |
| **`safetensors`** | PyTorch / LLMs | **SAFE** (Pure binary tensor payload) | **YES (`mmap`)** | **YES** | SOTA standard for PyTorch neural weights & Hugging Face. |
| **`ONNX`** | Multi-Framework | **SAFE** (Deterministic Protocol Buffers)| **YES** | **YES** | High-throughput, cross-platform microservice deployment. |
| **`TorchScript (JIT)`** | PyTorch | Low (Embedded C++ IR) | No | No | Deploying PyTorch models into native C++ environments without Python. |

---

## 2. Joblib: Atomic Compression Recipe

```python
import os
import joblib

def atomic_joblib_dump(obj, filepath: str, compress=("lz4", 3)):
    """Atomically serializes model using temporary file replacement."""
    os.makedirs(os.path.dirname(os.path.abspath(filepath)), exist_ok=True)
    temp_file = f"{filepath}.tmp_{os.getpid()}"
    joblib.dump(obj, temp_file, compress=compress)
    os.replace(temp_file, filepath)

def safe_joblib_load(filepath: str):
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"Model file not found: {filepath}")
    return joblib.load(filepath)
```

---

## 3. Skops: Zero-Code Execution Secure Serialization

```python
import skops.io as sio

# 1. Safe Serialization
def export_secure_skops(pipeline, filepath: str):
    sio.dump(pipeline, filepath)

# 2. Safe Loading with Type Inspection & Trusted Types
def load_secure_skops(filepath: str):
    # Untrusted loading: inspect required types first
    unknown_types = sio.get_untrusted_types(file=filepath)
    print(f"[AUDIT] Types requested by model: {unknown_types}")

    # Load strictly with explicit allowlist (or trusted=True if verified)
    model = sio.load(filepath, trusted=unknown_types)
    return model
```

---

## 4. Safetensors: Zero-Copy Memory-Mapped Neural Tensors

```python
import torch
import torch.nn as nn
from safetensors.torch import save_file, load_file

class SimpleNet(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc = nn.Linear(10, 2)

model = SimpleNet()

# 1. Save state_dict directly to safetensors (Zero-copy binary format)
save_file(model.state_dict(), "artifacts/model.safetensors")

# 2. Instant Memory-Mapped Load
loaded_tensors = load_file("artifacts/model.safetensors", device="cpu")
model.load_state_dict(loaded_tensors)
```

---

## 5. ONNX Export & Multi-Platform Inference Engine

### 5.1 PyTorch to ONNX Export
```python
import torch

def export_pytorch_to_onnx(model: torch.nn.Module, dummy_input: torch.Tensor, onnx_path: str):
    model.eval()
    torch.onnx.export(
        model=model,
        args=dummy_input,
        f=onnx_path,
        export_params=True,
        opset_version=17,
        do_constant_folding=True,
        input_names=["input_features"],
        output_names=["logits"],
        dynamic_axes={
            "input_features": {0: "batch_size"},
            "logits": {0: "batch_size"}
        }
    )
    print(f"[SUCCESS] Exported PyTorch graph to {onnx_path}")
```

### 5.2 Scikit-Learn to ONNX Export (via skl2onnx)
```python
from skl2onnx import convert_sklearn
from skl2onnx.common.data_types import FloatTensorType

def export_sklearn_to_onnx(sklearn_pipeline, n_features: int, onnx_path: str):
    initial_type = [("float_input", FloatTensorType([None, n_features]))]
    onx = convert_sklearn(sklearn_pipeline, initial_types=initial_type, target_opset=17)
    with open(onnx_path, "wb") as f:
        f.write(onx.SerializeToString())
```

### 5.3 High-Throughput ONNX Runtime Inference Client
```python
import onnxruntime as ort
import numpy as np

class OnnxInferenceClient:
    def __init__(self, onnx_path: str):
        # Configure thread pool and execution providers
        opts = ort.SessionOptions()
        opts.intra_op_num_threads = 4
        opts.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL

        providers = ["CUDAExecutionProvider", "CPUExecutionProvider"]
        self.session = ort.InferenceSession(onnx_path, sess_options=opts, providers=providers)
        self.input_name = self.session.get_inputs()[0].name
        self.output_name = self.session.get_outputs()[0].name

    def predict(self, X: np.ndarray) -> np.ndarray:
        # Vectorized C++ execution outside Python GIL
        return self.session.run([self.output_name], {self.input_name: X.astype(np.float32)})[0]
```

---

## 6. TorchScript JIT Tracing for C++ Deployment

```python
import torch

def trace_and_save_torchscript(model: torch.nn.Module, dummy_input: torch.Tensor, filepath: str):
    model.eval()
    with torch.no_grad():
        traced_cell = torch.jit.trace(model, dummy_input)
    traced_cell.save(filepath)
    print(f"[SUCCESS] Saved standalone TorchScript model to {filepath}")

# In C++ LibTorch:
# torch::jit::script::Module module = torch::jit::load("filepath.pt");
```
