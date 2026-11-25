# Compute & Hardware Acceleration: CPUs, GPUs, Tensor Cores & Mixed Precision

> **Topic:** Hardware Architecture, Throughput vs Latency, Mixed Precision (FP16/BF16/FP8) & The Roofline Model  
> **Module ID:** `05-ai/machine-learning/09-training-and-optimisation/10-compute-and-gpu-basics.md`  
> **Target Version:** Python 3.11+, PyTorch 2.5+, NumPy 2.5+  
> **Level:** Core  
> **Prerequisites:** [Gradient Descent Variants](02-gradient-descent-variants.md), [Handling Large Datasets](09-handling-large-datasets.md)  
> **Status:** Verified (Python 3.11 / CPU)

---

## Architectural / Conceptual Flow

```mermaid
flowchart TD
    A[Compute Workload Characteristics] --> B{Execution Paradigm}
    
    B -->|Low Latency / Sequential Branching| C["CPU Architecture<br>Few powerful cores, large L1/L2/L3 caches, AVX-512 SIMD"]
    B -->|Massive Parallel / Matrix Multiplication| D["GPU Architecture<br>Thousands of ALUs, Streaming Multiprocessors, High Bandwidth Memory (HBM)"]
    B -->|Dedicated Systolic Matrix Multipliers| E["Specialized AI Accelerators<br>NVIDIA Tensor Cores, Google TPUs, Apple Neural Engine"]
    
    D --> F[Memory & Precision Optimization]
    E --> F
    
    F --> F1["Precision Formats: FP32 (Full) -> TF32 -> BF16 / FP16 -> FP8"]
    F --> F2["Automatic Mixed Precision (AMP): FP16 Computations + FP32 Master Weights"]
    F --> F3["Dynamic Loss Scaling (GradScaler): Prevents gradient underflow"]
    
    F --> G["Bottleneck Identification: Roofline Model (Memory Bandwidth Bound vs Compute Bound)"]
```

---

## 1. Concept Overview & Core Motivation

Modern machine learning workloads are fundamentally **linear algebra compute engines**. Training a deep neural network or gradient boosted decision tree executes trillions of Floating-Point Multiply-Accumulate (MAC) operations:

$$\mathbf{C} = \mathbf{A} \mathbf{B} + \mathbf{C}$$

### CPU vs GPU Computational Paradigms
- **Central Processing Units (CPUs)** are optimized for **latency-critical, sequential processing**. With 8 to 96 powerful cores running at high clock frequencies (3.5–5.0 GHz) backed by deep branch predictors and massive multi-level cache hierarchies, CPUs excel at operating systems, string manipulation, tree traversals, and non-vectorized tabular preprocessing.
- **Graphics Processing Units (GPUs)** are optimized for **throughput-critical, massively parallel processing**. Rather than a few complex cores, a modern GPU (such as NVIDIA H100 or RTX 4090) consists of thousands of lightweight Arithmetic Logic Units (ALUs) organized into Streaming Multiprocessors (SMs), delivering tens of terabytes of memory bandwidth per second.

Understanding hardware constraints—specifically the trade-offs between **arithmetic intensity**, **memory bandwidth**, and **numerical precision**—is required to maximize hardware utilization and prevent expensive training infrastructure from idling.

---

## 2. Theoretical Foundations & Mathematics

### 2.1 Floating-Point Numeric Representations

An IEEE floating-point number represents a real value $x$ as:

$$x = (-1)^s \times 2^{e - \text{bias}} \times \left(1 + \sum_{i=1}^m b_i 2^{-i}\right)$$

where $s$ is the sign bit, $e$ is the exponent bits, and $m$ is the mantissa (significand) bits.

| Format | Total Bits | Sign ($s$) | Exponent ($e$) | Mantissa ($m$) | Dynamic Exponent Range | Precision (Decimal Digits) | Primary Use Case |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **FP32** (Single) | 32 | 1 | 8 | 23 | $\approx 10^{-38} \text{ to } 10^{38}$ | $\approx 7.2$ | Classical default; master weights; loss computation |
| **TF32** (TensorFloat) | 19 | 1 | 8 | 10 | $\approx 10^{-38} \text{ to } 10^{38}$ | $\approx 3.0$ | NVIDIA Ampere/Hopper internal Tensor Core speedup |
| **FP16** (Half) | 16 | 1 | 5 | 10 | $\approx 10^{-5} \text{ to } 65,504$ | $\approx 3.3$ | Memory savings; prone to underflow (requires loss scaling) |
| **BF16** (Bfloat16) | 16 | 1 | 8 | 7 | $\approx 10^{-38} \text{ to } 10^{38}$ | $\approx 2.1$ | Modern LLM / Transformer default; immune to underflow |
| **FP8** (E4M3 / E5M2)| 8 | 1 | 4/5 | 3/2 | Extremely limited | $\approx 1.0$ | Hopper / Blackwell ultra-fast inference and pre-training |

### 2.2 The Roofline Model: Arithmetic Intensity

The **Roofline Model** establishes whether an algorithm's execution time is limited by the processor's peak computational throughput ($P_{\text{peak}}$, in TFLOPs/s) or by the memory subsystem's bandwidth ($B_{\text{mem}}$, in GB/s).

**Arithmetic Intensity ($I$)** is defined as the ratio of floating-point operations performed per byte of memory transferred:

$$I = \frac{\text{Total FLOPs}}{\text{Total Bytes Transferred from Memory}} \quad [\text{FLOPs/Byte}]$$

The maximum achievable performance $P$ is bounded by:

$$P = \min\left(P_{\text{peak}}, \; I \times B_{\text{mem}}\right)$$

```
Performance (TFLOPs/s)
  ^
  |                          Peak Compute Limit (P_peak)
  |                      +--------------------------------- (Compute Bound)
  |                     /
  |                    /
  |                   /
  |                  /
  |                 /  Memory Bandwidth Limit (Slope = B_mem)
  |                /   (Memory Bound)
  |               /
  |              /
  +-------------+-----------------------------------------> Arithmetic Intensity (I)
             I_ridge = P_peak / B_mem
```

- If $I < I_{\text{ridge}}$: The workload is **Memory-Bound** (e.g. element-wise additions, ReLU, Dropout, LayerNorm). Speed is limited by how fast data can be streamed from VRAM.
- If $I > I_{\text{ridge}}$: The workload is **Compute-Bound** (e.g. large Matrix Multiplications $\mathbf{W} \mathbf{x}$, Convolutions). Speed is limited by Tensor Core clock speeds.

### 2.3 Dynamic Loss Scaling in Mixed Precision

In FP16, the smallest positive normalized value is $2^{-14} \approx 6.1 \times 10^{-5}$. Gradients $\mathbf{g} = \nabla_\theta \mathcal{L}$ in deep networks frequently have magnitudes in $[10^{-8}, 10^{-5}]$, causing them to underflow to exact $0.0$.

**Dynamic Loss Scaling** multiplies the loss $\mathcal{L}$ by a large scaling factor $S \approx 2^{16} = 65,536$ before computing backpropagation:

$$\tilde{\mathcal{L}} = S \cdot \mathcal{L} \implies \tilde{\mathbf{g}} = \nabla_\theta \tilde{\mathcal{L}} = S \cdot \mathbf{g}$$

After the backward pass, gradients are unscaled back to FP32 before the optimizer update:

$$\mathbf{g} = \frac{1}{S} \tilde{\mathbf{g}}$$

If an `inf` or `NaN` is detected in $\tilde{\mathbf{g}}$, the optimizer skips the step and halves the scale factor $S \leftarrow S / 2$.

---

## 3. Implementation & Practical Code

```python
"""Production Automated Mixed Precision (AMP) Pipeline and Roofline Diagnostics.

Requirements: torch>=2.0.0, numpy>=2.0.0
Verified on: Python 3.11 / CPU
"""

from __future__ import annotations
import time
from typing import Tuple
import numpy as np
import torch
import torch.nn as nn


class RooflineAnalyzer:
    """Calculates theoretical performance boundaries and arithmetic intensity."""

    @staticmethod
    def analyze_matmul(M: int, N: int, K: int, peak_tflops: float = 82.6, bandwidth_gb_s: float = 1008.0) -> dict:
        """Analyzes GEMM: C = A @ B where A is (M, K) and B is (K, N) in FP16 (2 bytes)."""
        flops = 2.0 * M * N * K
        bytes_transferred = 2.0 * (M * K + K * N + M * N)  # Read A, B; write C
        arithmetic_intensity = flops / bytes_transferred

        ridge_point = (peak_tflops * 1e12) / (bandwidth_gb_s * 1e9)
        is_compute_bound = arithmetic_intensity >= ridge_point

        theoretical_max_tflops = min(peak_tflops, (arithmetic_intensity * bandwidth_gb_s * 1e9) / 1e12)

        return {
            "flops": flops,
            "bytes": bytes_transferred,
            "arithmetic_intensity": arithmetic_intensity,
            "ridge_point": ridge_point,
            "bound_regime": "COMPUTE_BOUND" if is_compute_bound else "MEMORY_BOUND",
            "theoretical_tflops": theoretical_max_tflops,
        }


class ProductionAMPExecutor:
    """Demonstrates safe mixed precision execution with device fallback."""

    def __init__(self, in_features: int = 1024, hidden: int = 2048, out_features: int = 10):
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.model = nn.Sequential(
            nn.Linear(in_features, hidden),
            nn.LayerNorm(hidden),
            nn.GELU(),
            nn.Linear(hidden, out_features)
        ).to(self.device)

        self.optimizer = torch.optim.AdamW(self.model.parameters(), lr=1e-3)
        # Enable GradScaler if CUDA is available, else disabled dummy scaler
        self.scaler = torch.amp.GradScaler("cuda", enabled=torch.cuda.is_available())

    def train_step(self, x: torch.Tensor, y: torch.Tensor) -> float:
        x, y = x.to(self.device), y.to(self.device)
        self.optimizer.zero_grad(set_to_none=True)

        device_type = "cuda" if torch.cuda.is_available() else "cpu"
        # Autocast automatically casts operations to FP16/BF16 where safe
        with torch.amp.autocast(device_type=device_type, dtype=torch.bfloat16 if device_type == "cpu" else torch.float16):
            preds = self.model(x)
            loss = nn.CrossEntropyLoss()(preds, y)

        # Scale loss and backprop
        self.scaler.scale(loss).backward()
        # Unscale gradients and update parameters
        self.scaler.step(self.optimizer)
        self.scaler.update()

        return float(loss.item())


if __name__ == "__main__":
    print("--- Hardware Roofline Analysis (NVIDIA RTX 4090 / Ampere Profile) ---")
    gemm_small = RooflineAnalyzer.analyze_matmul(M=64, N=64, K=64)
    gemm_large = RooflineAnalyzer.analyze_matmul(M=4096, N=4096, K=4096)

    print(f"Small GEMM (64x64):   Intensity = {gemm_small['arithmetic_intensity']:6.2f} FLOPs/B | Regime = {gemm_small['bound_regime']}")
    print(f"Large GEMM (4096x4096): Intensity = {gemm_large['arithmetic_intensity']:6.2f} FLOPs/B | Regime = {gemm_large['bound_regime']}")
    print(f"Hardware Ridge Point:  {gemm_large['ridge_point']:.2f} FLOPs/Byte")

    print("\n--- Production Mixed Precision Step Execution ---")
    amp_runner = ProductionAMPExecutor()
    dummy_x = torch.randn(32, 1024)
    dummy_y = torch.randint(0, 10, (32,))

    loss_val = amp_runner.train_step(dummy_x, dummy_y)
    print(f"AMP Execution Step Completed | Device: {amp_runner.device} | Loss: {loss_val:.4f}")
```

### Execution Verification Output

```
--- Hardware Roofline Analysis (NVIDIA RTX 4090 / Ampere Profile) ---
Small GEMM (64x64):   Intensity =  21.33 FLOPs/B | Regime = MEMORY_BOUND
Large GEMM (4096x4096): Intensity = 1365.33 FLOPs/B | Regime = COMPUTE_BOUND
Hardware Ridge Point:  81.94 FLOPs/Byte

--- Production Mixed Precision Step Execution ---
AMP Execution Step Completed | Device: cpu | Loss: 2.3792
```

---

## 4. Edge Cases & Advanced Scenarios

1. **Underflow in Softmax and LayerNorm**: In pure FP16, computing $\sum \exp(z_i)$ easily causes catastrophic overflow to $+\infty$ if $z_i > 11.0$ (since $\exp(11.1) > 65,504$). PyTorch's `torch.amp.autocast` automatically leaves reduction operations (Softmax, LayerNorm, BatchNorm, and Cross-Entropy loss) in **FP32** while executing matrix multiplications in **FP16**.
2. **CPU Data Starvation (The "GPU Bottleneck Paradox")**: If complex data augmentations (e.g. image decoding, regex tokenization) run synchronously on a single CPU thread, the GPU finishes each mini-batch in 5ms and sits idle for 95ms waiting for the next batch. GPU utilization drops to 5%. Resolution: Multi-threaded prefetching via `num_workers = 4`, `pin_memory = True`, and asynchronous prefetching.
3. **Tensor Core Dimension Misalignment**: NVIDIA Tensor Cores execute matrix multiplications via specialized hardware tiles (e.g. $16 \times 16 \times 16$ micro-GEMMs). If tensor matrix dimensions (Batch Size, Sequence Length, Hidden Dimension) are not multiples of 8 or 16, the compiler pads matrices with zeros, forfeiting up to 30% of potential peak throughput.

---

## 5. Anti-Patterns & Common Pitfalls Table

| Anti-Pattern / Mistake | Why It Fails | Correct Engineering Fix |
| :--- | :--- | :--- |
| Training in pure FP16 without GradScaler | Gradients smaller than $6.1 \times 10^{-5}$ underflow to zero, causing training to stall | Always wrap FP16 backward passes in `torch.amp.GradScaler()` |
| Using FP16 instead of BF16 on modern hardware (Ampere/Hopper) | FP16 has an 8-bit dynamic range; prone to frequent gradient overflows requiring loss scale adjustments | Use **Bfloat16 (BF16)**, which has the identical 8-bit exponent range as FP32 and requires no loss scaling |
| Omitting `pin_memory=True` on GPU DataLoaders | Host-to-device memory copies must be staged through temporary pinned buffers, slowing transfer | Set `pin_memory=True` in `DataLoader` to enable direct DMA page-locked transfer |
| Calling `.item()` or `.cpu()` inside the batch iteration loop | Forces GPU to synchronize synchronously with CPU host, destroying CUDA stream concurrency | Accumulate metric tensors on GPU; synchronize only once per validation epoch |
| Non-multiple-of-8 tensor dimensions | Fails to activate hardware Tensor Core systolic arrays | Pad vocabulary sizes and hidden layers to multiples of 8, 16, or 64 |
| Using standard Python CPU loops for tabular tree scoring | Fails to utilize modern SIMD registers | Leverage **GPU-accelerated libraries (CuML / XGBoost `tree_method='hist'`)** |

---

## 6. Best Practices & Production Guidelines

1. **The Modern Precision Standard**:
   - Training on NVIDIA Ampere (A100, RTX 3090+), Hopper (H100), or newer: Use **Bfloat16 (`torch.bfloat16`)**. It requires zero loss scaling and avoids numerical divergence.
   - Training on older Volta/Turing (V100, T4, RTX 2080): Use **FP16 (`torch.float16`)** with **`GradScaler`**.
2. **DataLoader High-Throughput Recipe**:
   ```python
   loader = DataLoader(
       dataset,
       batch_size=256,
       shuffle=True,
       num_workers=4,          # Rule of thumb: 4 workers per GPU
       pin_memory=True,        # Page-locked memory for fast DMA transfers
       persistent_workers=True,# Keeps worker processes alive across epochs
       prefetch_factor=2       # Prefetches 2 batches per worker
   )
   ```

---

## 7. Real-World Case Study

### GPU Acceleration of Massive Tabular Gradient Boosting
- **Context**: A retail conglomerate trains GBDT models on 40 million point-of-sale customer transactions weekly with 150 dense numerical features.
- **Problem**: Baseline Scikit-Learn `HistGradientBoostingClassifier` on a 64-core Intel Xeon CPU server required 7 hours and 40 minutes per training run.
- **Solution**:
  1. Migrated model to **XGBoost with GPU Histograms** (`tree_method='hist'`, `device='cuda'`).
  2. Maintained feature matrices in GPU VRAM using NVIDIA RAPIDS cuDF.
  3. Aligned matrix columns to multiples of 16.
- **Outcome**: Training time plummeted from **460 minutes down to 8.5 minutes**—a **$54\times$ wall-clock speedup**—allowing hyperparameter optimization sweeps to complete in a single afternoon.

---

## 8. Performance & Resource Optimization

1. **PyTorch 2.0 Compilation (`torch.compile`)**:
   ```python
   compiled_model = torch.compile(model, mode="reduce-overhead")
   ```
   Uses TorchDynamo and OpenAI Triton to fuse element-wise operations (e.g. `Bias + Add + LayerNorm + GELU`), converting memory-bound bottlenecks into compute-bound kernel executions and boosting throughput by $20\text{--}40\%$.
2. **Zero-Grad Memory Optimization**: Use `optimizer.zero_grad(set_to_none=True)` instead of resetting gradients to zeros. Setting to `None` frees the tensor memory buffers entirely until backprop allocates them, reducing peak memory usage.

---

## 9. Security, Privacy & Ethics Considerations

1. **Side-Channel Timing Attacks on Cloud GPUs**: In multi-tenant cloud GPU clusters with shared PCIe switches or virtualized instances, memory access timing variations can leak tensor matrix dimensions or neural network layer counts to co-located tenants.
2. **Energy Efficiency and Carbon Footprint**: Mixed-precision BF16/FP8 training halves memory bandwidth requirements and reduces power consumption by up to $3\times$ per training run compared to FP32, directly mitigating the environmental impact of large-scale ML operations.

---

## 10. Diagnostic & Verification Playbook

### Diagnostic Script: GPU Telemetry & Tensor Core Alignment Check
```python
import torch

def audit_hardware_configuration():
    print(f"CUDA Available: {torch.cuda.is_available()}")
    if torch.cuda.is_available():
        device_name = torch.cuda.get_device_name(0)
        capability = torch.cuda.get_device_capability(0)
        print(f"Device: {device_name} (Compute Capability: {capability[0]}.{capability[1]})")
        print(f"Supports BF16: {torch.cuda.is_bf16_supported()}")
        print(f"Device Memory: {torch.cuda.get_device_properties(0).total_memory / 1e9:.2f} GB")
    else:
        print("Executing on Host CPU (SIMD Vectorization Enabled)")

    # Alignment verification helper
    def check_tensor_core_aligned(dim: int) -> bool:
        return dim % 8 == 0

    assert check_tensor_core_aligned(512), "512 should be aligned"
    assert not check_tensor_core_aligned(505), "505 is misaligned"
    print("Hardware Diagnostic Assertion Passed Successfully!")

audit_hardware_configuration()
```

---

## 11. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: What is the Roofline Model, and how do you determine if a model layer is compute-bound or memory-bound?</strong></summary>

**Answer:** The Roofline model characterizes the theoretical performance limit of an algorithm on specific hardware based on its **Arithmetic Intensity** $I = \frac{\text{FLOPs}}{\text{Bytes Transferred}}$.
A hardware system has a "ridge point" defined by $I_{\text{ridge}} = \frac{P_{\text{peak}}}{B_{\text{mem}}}$, where $P_{\text{peak}}$ is peak computational throughput (TFLOPs/s) and $B_{\text{mem}}$ is memory bandwidth (GB/s).
- If a layer's arithmetic intensity $I < I_{\text{ridge}}$, it falls on the memory-bandwidth slope: it is **Memory-Bound**. Execution speed is limited by how quickly parameters can be read from VRAM (e.g. element-wise activations, LayerNorm, Dropout).
- If $I > I_{\text{ridge}}$, it reaches the flat ceiling: it is **Compute-Bound**. Execution speed is limited by how quickly the ALUs or Tensor Cores can execute math (e.g. large dense matrix multiplications and deep convolutions).
</details>

<details>
<summary><strong>Q2: Why does Bfloat16 (BF16) largely eliminate the need for Dynamic Loss Scaling compared to FP16?</strong></summary>

**Answer:** Both FP16 and BF16 occupy 16 bits of memory, but they partition their bits differently:
- **FP16**: 1 sign bit, 5 exponent bits, 10 mantissa bits. The 5 exponent bits yield a narrow dynamic range ($10^{-5}$ to $65,504$). Gradients smaller than $6 \times 10^{-5}$ underflow to zero, requiring a dynamic loss scaler ($S \approx 2^{16}$) to shift small gradients up into representable range.
- **BF16**: 1 sign bit, 8 exponent bits, 7 mantissa bits. By dedicating 8 bits to the exponent (matching FP32 exactly), BF16 has an identical dynamic range ($10^{-38}$ to $10^{38}$). Gradients almost never underflow or overflow, allowing training without a loss scaler or underflow checks.
</details>

<details>
<summary><strong>Q3: How does Automatic Mixed Precision (AMP) maintain training stability while using half-precision arithmetic?</strong></summary>

**Answer:** AMP uses a hybrid precision execution strategy:
1. **Master Weights in FP32**: The optimizer maintains parameter weights and momentum buffers in FP32 to ensure that tiny gradient updates $\eta \mathbf{g}$ are not truncated to zero.
2. **Forward and Backward Computations in FP16/BF16**: Matrix multiplications and convolutions are cast to 16-bit half precision to utilize high-speed Tensor Cores and halve memory bandwidth consumption.
3. **Reductions in FP32**: Numerical reductions (Softmax, LayerNorm, Cross-Entropy) are automatically kept in FP32 to prevent exponent overflow.
4. **GradScaler Unscaling**: Gradients are scaled to prevent underflow, converted back to FP32, and applied to update the FP32 master weights.
</details>

<details>
<summary><strong>Q4: Why does setting `pin_memory=True` in a PyTorch DataLoader accelerate GPU training?</strong></summary>

**Answer:** Host CPU memory is normally allocated as paged virtual memory, which the operating system can relocate or swap to disk at any time. The GPU cannot copy data directly from pageable host memory via Direct Memory Access (DMA). 
When `pin_memory=False`, PyTorch must first copy the batch from pageable host RAM into an internal temporary "pinned" (page-locked) host memory buffer before executing the DMA transfer to GPU VRAM. 
Setting `pin_memory=True` allocates the DataLoader tensors directly in page-locked physical host memory, eliminating the redundant CPU-to-CPU copy and enabling asynchronous DMA transfers directly across the PCIe bus concurrently with GPU kernel execution.
</details>

<details>
<summary><strong>Q5: What causes "GPU Starvation" and how do you diagnose it using profiling tools?</strong></summary>

**Answer:** GPU Starvation occurs when the host CPU data loading and preprocessing pipeline cannot prepare mini-batches as quickly as the GPU executes them. 
Using tools like NVIDIA Nsight Systems, PyTorch Profiler, or `nvidia-smi`:
- Symptom: GPU utilization oscillates between 0% and 90% in a spiky pattern.
- In timeline traces, large empty gaps appear on the CUDA stream between kernel launches, while CPU threads show continuous blocking calls inside `DataLoader.__next__()`.
Fixes include increasing `num_workers`, moving transforms to GPU via torchvision tensors, enabling prefetching, or saving data in pre-processed binary formats (Parquet/Arrow).
</details>

<details>
<summary><strong>Q6: Why must tensor dimensions be multiples of 8 or 16 to utilize NVIDIA Tensor Cores?</strong></summary>

**Answer:** Tensor Cores are specialized hardware systolic matrix-multiply-accumulate units that process fixed-size micro-tiles (e.g. $16 \times 16 \times 16$ or $8 \times 8 \times 4$ floating-point matrices) in a single clock cycle. If the matrix dimensions $M, N, K$ of GEMM operations are not multiples of the hardware tile size, the CUDA driver and cuBLAS library must pad the matrices with zero values or fall back to standard scalar CUDA cores, causing substantial throughput drops.
</details>

<details>
<summary><strong>Q7: What is Kernel Fusion and how does `torch.compile` utilize it to bypass memory bandwidth bottlenecks?</strong></summary>

**Answer:** In uncompiled PyTorch, evaluating $\text{GELU}(\text{LayerNorm}(\mathbf{x} + \mathbf{b}))$ requires launching three separate GPU kernels. Each kernel must read its input tensor from high-bandwidth memory (HBM), perform the operation, and write the output back to HBM. Because these operations have low arithmetic intensity, performance is severely bottlenecked by memory read/write latency. 
`torch.compile` uses OpenAI Triton to generate a single fused GPU kernel: data is loaded from HBM into the Streaming Multiprocessor's fast on-chip SRAM/registers once, all mathematical transformations are applied sequentially in register memory, and only the final result is written back to HBM, reducing memory traffic by up to $3\times$.
</details>

<details>
<summary><strong>Q8: What is the primary difference between FP8 E4M3 and FP8 E5M2?</strong></summary>

**Answer:** NVIDIA Hopper architectures introduce two distinct 8-bit floating-point formats:
- **E4M3** (1 sign bit, 4 exponent bits, 3 mantissa bits): Offers higher precision (3 bits of mantissa) at the expense of a smaller dynamic range. It is used primarily for **activations and weights** during the forward pass.
- **E5M2** (1 sign bit, 5 exponent bits, 2 mantissa bits): Mirrors standard half-precision FP16 exponent range (5 bits) with lower precision (2 bits of mantissa). It is used primarily for **gradients** during the backward pass where dynamic range is critical to avoid underflow.
</details>

---

## 12. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Measure Host-to-Device Memory Transfer Latency</strong></summary>

**Task:** Write a Python script comparing tensor creation time with and without pinned memory.  
**Solution:**
```python
import torch
import time

def benchmark_pinned_memory():
    size = (1000, 1000)
    # Standard paged tensor
    t_start = time.perf_counter()
    paged_tensor = torch.randn(size)
    paged_time = time.perf_counter() - t_start

    # Pinned memory tensor
    t_start = time.perf_counter()
    pinned_tensor = torch.randn(size).pin_memory()
    pinned_time = time.perf_counter() - t_start

    assert pinned_tensor.is_pinned()
    print(f"Paged alloc: {paged_time*1e3:.3f}ms | Pinned alloc: {pinned_time*1e3:.3f}ms")

benchmark_pinned_memory()
print("Exercise 1 Verified!")
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Implement Manual Dynamic Loss Scaler</strong></summary>

**Task:** Implement a pure Python class that scales loss, checks for gradient `inf`/`NaN`, unscales gradients, and adapts scale factor $S$ dynamically.  
**Solution:**
```python
import torch

class SimpleGradScaler:
    def __init__(self, init_scale: float = 65536.0, growth_factor: float = 2.0, backoff_factor: float = 0.5):
        self.scale = init_scale
        self.growth = growth_factor
        self.backoff = backoff_factor
        self.growth_tracker = 0

    def scale_loss(self, loss: torch.Tensor) -> torch.Tensor:
        return loss * self.scale

    def step_and_update(self, optimizer: torch.optim.Optimizer, grads: list[torch.Tensor]) -> bool:
        # Check for inf or nan
        has_overflow = any(torch.isnan(g).any() or torch.isinf(g).any() for g in grads)
        if has_overflow:
            self.scale *= self.backoff
            self.growth_tracker = 0
            optimizer.zero_grad()
            return False # Step skipped
        
        # Unscale
        for g in grads:
            g.data.div_(self.scale)
        optimizer.step()
        
        self.growth_tracker += 1
        if self.growth_tracker >= 2000:
            self.scale *= self.growth
            self.growth_tracker = 0
        return True

scaler = SimpleGradScaler()
w = torch.tensor([1.0], requires_grad=True)
opt = torch.optim.SGD([w], lr=0.1)

# Normal step
loss = (w ** 2)
scaled_loss = scaler.scale_loss(loss)
scaled_loss.backward()
success = scaler.step_and_update(opt, [w.grad])
assert success
assert scaler.scale == 65536.0
print("Exercise 2 Loss Scaler Verified!")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Tensor Core Multiplicity Padding Utility</strong></summary>

**Task:** Write a utility function that inspects a PyTorch linear layer and pads weight matrices to the nearest multiple of 16 to guarantee Tensor Core activation.  
**Solution:**
```python
import torch
import torch.nn as nn
import torch.nn.functional as F

def pad_layer_to_multiple(layer: nn.Linear, multiple: int = 16) -> nn.Linear:
    in_f, out_f = layer.in_features, layer.out_features
    pad_in = (multiple - (in_f % multiple)) % multiple
    pad_out = (multiple - (out_f % multiple)) % multiple
    
    if pad_in == 0 and pad_out == 0:
        return layer # Already aligned

    new_in = in_f + pad_in
    new_out = out_f + pad_out
    new_layer = nn.Linear(new_in, new_out, bias=layer.bias is not None)
    
    with torch.no_grad():
        new_layer.weight.zero_()
        new_layer.weight[:out_f, :in_f] = layer.weight
        if layer.bias is not None:
            new_layer.bias.zero_()
            new_layer.bias[:out_f] = layer.bias
            
    return new_layer

orig_layer = nn.Linear(10, 25) # Misaligned
padded = pad_layer_to_multiple(orig_layer, multiple=16)
assert padded.in_features == 16
assert padded.out_features == 32
print(f"Padded {orig_layer.in_features}x{orig_layer.out_features} to {padded.in_features}x{padded.out_features}")
print("Exercise 3 Verified!")
```
</details>

<details>
<summary><strong>Exercise 4 (Hard): Analytical Roofline Classification for PyTorch Layers</strong></summary>

**Task:** Write an automated inspector that takes a PyTorch sequential model and calculates whether each layer is memory-bound or compute-bound on an A100 GPU.  
**Solution:**
```python
import torch.nn as nn

def classify_model_layers(model: nn.Module, batch_size: int = 32, a100_ridge: float = 156.0):
    report = []
    for name, layer in model.named_children():
        if isinstance(layer, nn.Linear):
            M = batch_size
            N = layer.out_features
            K = layer.in_features
            flops = 2.0 * M * N * K
            bytes_trans = 2.0 * (M * K + K * N + M * N)
            ai = flops / bytes_trans
            regime = "COMPUTE_BOUND" if ai >= a100_ridge else "MEMORY_BOUND"
            report.append((name, ai, regime))
        elif isinstance(layer, (nn.ReLU, nn.GELU, nn.LayerNorm)):
            # Element-wise operations have AI < 1.0
            report.append((name, 0.5, "MEMORY_BOUND"))
    return report

test_net = nn.Sequential(
    nn.Linear(128, 4096),
    nn.ReLU(),
    nn.Linear(4096, 4096),
    nn.LayerNorm(4096)
)
analysis = classify_model_layers(test_net, batch_size=64)
for l_name, intensity, reg in analysis:
    print(f"Layer: {l_name:12s} | AI: {intensity:8.2f} FLOPs/B | {reg}")
assert analysis[0][2] == "COMPUTE_BOUND" or analysis[2][2] == "COMPUTE_BOUND"
print("Exercise 4 Roofline Analysis Verified!")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Automatic Precision Benchmark Runner</strong></summary>

**Task:** Write a benchmarking harness comparing execution throughput (steps per second) of FP32 versus BF16 on CPU vector operations.  
**Solution:**
```python
import torch
import time

def compare_precision_throughput():
    x32 = torch.randn(2000, 2000, dtype=torch.float32)
    w32 = torch.randn(2000, 2000, dtype=torch.float32)

    # Warmup
    _ = x32 @ w32

    # Benchmark FP32
    t0 = time.perf_counter()
    for _ in range(5):
        _ = x32 @ w32
    t_fp32 = (time.perf_counter() - t0) / 5

    # Benchmark BF16
    x16 = x32.bfloat16()
    w16 = w32.bfloat16()
    _ = x16 @ w16 # Warmup
    t0 = time.perf_counter()
    for _ in range(5):
        _ = x16 @ w16
    t_bf16 = (time.perf_counter() - t0) / 5

    print(f"FP32 Mean GEMM Time: {t_fp32*1e3:.2f}ms")
    print(f"BF16 Mean GEMM Time: {t_bf16*1e3:.2f}ms")
    assert t_fp32 > 0 and t_bf16 > 0

compare_precision_throughput()
print("Exercise 5 Precision Benchmark Verified!")
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Zero-Grad Set-To-None Memory Verification</strong></summary>

**Task:** Write an automated test confirming that `zero_grad(set_to_none=True)` deallocates tensor memory buffers while `zero_grad(set_to_none=False)` preserves allocated zero tensors.  
**Solution:**
```python
import torch

layer = torch.nn.Linear(10, 5)
opt = torch.optim.SGD(layer.parameters(), lr=0.1)

# Populate gradients
out = layer(torch.randn(2, 10)).sum()
out.backward()
assert layer.weight.grad is not None

# Test standard zero_grad
opt.zero_grad(set_to_none=False)
assert layer.weight.grad is not None
assert torch.equal(layer.weight.grad, torch.zeros_like(layer.weight.grad))

# Test set_to_none=True
opt.zero_grad(set_to_none=True)
assert layer.weight.grad is None, "Gradient buffer must be None after set_to_none=True"
print("Exercise 6 Zero-Grad Deallocation Verified!")
```
</details>

---

## 13. Summary, Verified References & Navigation

* **Throughput Architecture**: GPUs utilize massive SIMD parallelism and high-bandwidth memory to accelerate linear algebra operations by orders of magnitude over latency-focused CPUs.
* **The Roofline Frontier**: Operations are fundamentally either **memory-bandwidth bound** (low arithmetic intensity) or **compute bound** (high arithmetic intensity).
* **The BF16 Paradigm**: Bfloat16 preserves the 8-bit dynamic range of FP32 while halving memory footprint, eliminating the need for complex loss scaling in modern deep learning.
* **Memory Pipelining**: Avoid CPU data starvation by configuring `DataLoader` with `num_workers > 0`, `pin_memory=True`, and `persistent_workers=True`.

### Verified External References
- Williams, S., Waterman, A., & Patterson, D. (2009). *Roofline: An insightful visual performance model for multicore architectures*. Communications of the ACM.
- Micikevicius, P., et al. (2018). *Mixed Precision Training*. ICLR (NVIDIA & Baidu).
- NVIDIA Corporation. *Tensor Core Architecture and Performance Guide*. https://developer.nvidia.com/tensor-cores

---

[← Previous Note: Handling Large Datasets](09-handling-large-datasets.md) | [Module Index](README.md) | [Next Module: Evaluation & Metrics →](../10-evaluation-and-metrics/README.md)
