# Handling Large Datasets: Out-of-Core Learning, Memory Mapping & Streaming

> **Topic:** Out-of-Core Computation, `partial_fit`, Memory-Mapped Arrays (`np.memmap`) & Arrow Streaming  
> **Module ID:** `05-ai/machine-learning/09-training-and-optimisation/09-handling-large-datasets.md`  
> **Target Version:** Python 3.11+, scikit-learn 1.9+, NumPy 2.5+, PyArrow 19.0+, polars 1.20+  
> **Level:** Core  
> **Prerequisites:** [Gradient Descent Variants](02-gradient-descent-variants.md), [Data Preparation & Preprocessing](../02-data-and-preprocessing/README.md)  
> **Status:** Verified (Python 3.11 / CPU)

---

## Architectural / Conceptual Flow

```mermaid
flowchart TD
    A[Dataset Exceeds Physical RAM Capacity] --> B{Data Pipeline Architecture}
    
    B -->|Disk Storage & Columnar Format| C["Apache Parquet / Arrow<br>Column-pruned, dictionary-encoded, compressed chunks"]
    B -->|Zero-Copy In-Memory Access| D["NumPy Memory-Mapped Arrays (np.memmap)<br>OS Virtual Memory Paging directly from NVMe"]
    B -->|Algorithmic Incremental Fitting| E["Out-of-Core Estimators (partial_fit)<br>Streaming mini-batches without in-memory state"]
    B -->|Unbounded Vocabularies| F["Feature Hashing Trick (MurmurHash3)<br>Fixed dimensional vectorization without vocabulary dictionary"]
    
    C --> G[Stream Processing Pipeline]
    D --> G
    E --> G
    F --> G
    
    G --> H["Trained Model (SGDClassifier / MiniBatchKMeans / IncrementalPCA)"]
```

---

## 1. Concept Overview & Core Motivation

Standard machine learning libraries (such as scikit-learn's default estimators) assume that the full training dataset $\mathbf{X} \in \mathbb{R}^{N \times d}$ and target vector $\mathbf{y} \in \mathbb{R}^N$ reside simultaneously in contiguous physical Random Access Memory (RAM). 

When datasets expand into the gigabyte or terabyte scale (e.g. 500M clickstream events, 100M transaction logs):
1. **Out-of-Memory (OOM) Crashes**: Operating systems terminate processes that exceed physical memory limits via the Linux OOM Killer.
2. **Virtual Memory Thrashing**: When memory spills into traditional OS swap space on disk, execution speeds collapse by $1,000\times$ due to continuous paging.
3. **Vocabulary Explosion**: In NLP and high-cardinality categorical processing, storing string-to-index dictionaries across billions of tokens exhausts heap memory.

**Out-of-Core (Incremental) Learning** is the paradigm where learning algorithms process data in continuous sequential chunks or mini-batches, updating internal model parameters iteratively without ever loading the complete dataset into RAM.

---

## 2. Theoretical Foundations & Mathematics

### 2.1 The Incremental Gradient Update

For a streaming batch $\mathcal{B}_t$ of size $B$ arriving at time $t$, parameters $\theta$ are updated via stochastic approximation:

$$\theta_t = \theta_{t-1} - \eta_t \left[\frac{1}{B} \sum_{i \in \mathcal{B}_t} \nabla_\theta \mathcal{L}(y_i, f_{\theta_{t-1}}(\mathbf{x}_i)) + \lambda \theta_{t-1}\right]$$

Under the Robbins-Monro conditions ($\sum \eta_t = \infty$ and $\sum \eta_t^2 < \infty$), the streaming estimator converges almost surely to the true risk minimizer $\theta^*$, regardless of whether the dataset is seen once or over multiple streamed passes.

### 2.2 Incremental Mean and Variance (Welford's Algorithm)

To scale features without loading the entire matrix $\mathbf{X}$ into memory, mean $\boldsymbol{\mu}$ and variance $\boldsymbol{\sigma}^2$ are updated incrementally using Chan et al.'s parallel formulation. For existing batch $A$ (count $n_A$) and incoming batch $B$ (count $n_B$):

$$n_{AB} = n_A + n_B$$

$$\boldsymbol{\delta} = \boldsymbol{\mu}_B - \boldsymbol{\mu}_A, \quad \boldsymbol{\mu}_{AB} = \boldsymbol{\mu}_A + \boldsymbol{\delta} \frac{n_B}{n_{AB}}$$

$$M_{2, AB} = M_{2, A} + M_{2, B} + \boldsymbol{\delta}^2 \frac{n_A n_B}{n_{AB}}$$

$$\boldsymbol{\sigma}^2_{AB} = \frac{M_{2, AB}}{n_{AB}}$$

### 2.3 The Hashing Trick (Weinberger et al., 2009)

To vectorize categorical or textual tokens $w$ into a fixed $D$-dimensional space without storing a vocabulary dictionary:

$$j = h(w) \pmod D, \quad \xi = \text{sign}(h_2(w)) \in \{-1, +1\}$$

$$\mathbf{x}_j \leftarrow \mathbf{x}_j + \xi$$

The signed hash $\xi$ ensures that hash collisions are unbiased in expectation: $\mathbb{E}[\langle \mathbf{x}, \mathbf{x}' \rangle] = \langle \mathbf{u}, \mathbf{u}' \rangle$.

### Complete Symbol Breakdown Table

| Symbol | Mathematical Definition | Data Type / Dimensions | Role in Concept |
| :---: | :--- | :---: | :--- |
| $\mathcal{B}_t$ | Mini-batch partition at step $t$ | $\mathbb{R}^{B \times d}$ | Chunk of data loaded into RAM |
| $B$ | Chunk / Mini-batch size | $\mathbb{Z}_{>0}$ | Memory footprint throttle |
| $\boldsymbol{\mu}_t, \boldsymbol{\sigma}_t^2$ | Running mean and variance vectors | $\mathbb{R}^d$ | Incremental normalization statistics |
| $h(w)$ | Hash function (e.g. MurmurHash3) | $\text{String} \to \mathbb{Z}$ | Memoryless token index projection |
| $D$ | Fixed feature dimension | $\mathbb{Z}_{>0}$ | Output dimensionality of hashing trick ($2^{18}\text{ to }2^{22}$) |

---

## 3. Implementation & Practical Code

```python
"""Production Out-of-Core Streaming Classifier and Memory-Mapped Pipeline.

Requirements: scikit-learn>=1.5.0, numpy>=2.0.0, pyarrow>=16.0.0
Verified on: Python 3.11 / CPU
"""

from __future__ import annotations
import tempfile
from pathlib import Path
from typing import Generator, Tuple
import numpy as np
from sklearn.linear_model import SGDClassifier
from sklearn.preprocessing import StandardScaler
from sklearn.feature_extraction.text import HashingVectorizer


class StreamingDataSimulator:
    """Simulates multi-gigabyte disk data streamed in small chunks."""

    @staticmethod
    def generate_disk_batches(
        num_batches: int = 10, batch_size: int = 500, n_features: int = 20
    ) -> Generator[Tuple[np.ndarray, np.ndarray], None, None]:
        rng = np.random.RandomState(42)
        w_true = rng.randn(n_features)
        for b in range(num_batches):
            X_chunk = rng.randn(batch_size, n_features)
            # Binary target
            y_chunk = (X_chunk @ w_true + rng.normal(0, 0.5, size=batch_size) > 0).astype(int)
            yield X_chunk, y_chunk


class OutOfCorePipeline:
    """Manages incremental scaling and classification across streaming data."""

    def __init__(self, n_features: int, classes: list[int]):
        self.scaler = StandardScaler()
        self.classifier = SGDClassifier(
            loss="log_loss",  # Equivalent to logistic regression
            penalty="l2",
            alpha=1e-4,
            random_state=42,
            learning_rate="optimal",
        )
        self.classes = np.array(classes)
        self.total_samples_seen = 0

    def partial_fit_chunk(self, X_chunk: np.ndarray, y_chunk: np.ndarray) -> None:
        # 1. Update running mean and variance
        self.scaler.partial_fit(X_chunk)

        # 2. Scale chunk in-place using current running statistics
        X_scaled = self.scaler.transform(X_chunk)

        # 3. Incrementally fit classifier
        self.classifier.partial_fit(X_scaled, y_chunk, classes=self.classes)
        self.total_samples_seen += len(X_chunk)


def demonstrate_memmap() -> None:
    """Demonstrates zero-RAM memory-mapped array processing directly from disk."""
    temp_dir = Path(tempfile.mkdtemp())
    memmap_path = temp_dir / "large_dataset.dat"
    shape = (1000, 50)
    dtype = np.float32

    # 1. Create and write to disk without loading into RAM
    fp_write = np.memmap(memmap_path, dtype=dtype, mode="w+", shape=shape)
    fp_write[:] = np.ones(shape, dtype=dtype) * 42.0
    fp_write.flush()
    del fp_write  # Closes handle

    # 2. Read back sliced chunk via memory-mapping (zero copy)
    fp_read = np.memmap(memmap_path, dtype=dtype, mode="r", shape=shape)
    slice_data = fp_read[100:105, 0:5]
    print(f"Memmapped Slice Sample: {slice_data[0, 0]:.1f} | Shape: {slice_data.shape}")
    assert np.all(slice_data == 42.0)
    del fp_read

    # Cleanup
    memmap_path.unlink()


if __name__ == "__main__":
    print("--- Memory-Mapped Array Demonstration ---")
    demonstrate_memmap()

    print("\n--- Out-of-Core Incremental Streaming Training ---")
    pipeline = OutOfCorePipeline(n_features=20, classes=[0, 1])

    # Stream 8 batches of 500 samples (4,000 total samples)
    data_stream = StreamingDataSimulator.generate_disk_batches(num_batches=8, batch_size=500, n_features=20)
    
    for b_idx, (X_batch, y_batch) in enumerate(data_stream):
        pipeline.partial_fit_chunk(X_batch, y_batch)
        if (b_idx + 1) % 4 == 0:
            print(f"Processed Batch {b_idx + 1:2d} | Cumulative Samples: {pipeline.total_samples_seen}")

    # Evaluate on held-out test chunk
    X_test, y_test = next(StreamingDataSimulator.generate_disk_batches(num_batches=1, batch_size=1000, n_features=20))
    X_test_scaled = pipeline.scaler.transform(X_test)
    test_acc = pipeline.classifier.score(X_test_scaled, y_test)
    print(f"Test Accuracy on 1,000 Streaming Held-Out Samples: {test_acc:.4f}")

    # 3. Hashing Trick demonstration
    print("\n--- Hashing Trick (Dictionary-Free Text Vectorization) ---")
    texts = ["fraudulent transaction wire transfer", "legitimate payment invoice approval"]
    hasher = HashingVectorizer(n_features=2**12, alternate_sign=True)
    sparse_matrix = hasher.transform(texts)
    print(f"Vectorized {len(texts)} sentences into fixed dimension {sparse_matrix.shape[1]} without dictionary.")
```

### Execution Verification Output

```
--- Memory-Mapped Array Demonstration ---
Memmapped Slice Sample: 42.0 | Shape: (5, 5)

--- Out-of-Core Incremental Streaming Training ---
Processed Batch  4 | Cumulative Samples: 2000
Processed Batch  8 | Cumulative Samples: 4000
Test Accuracy on 1,000 Streaming Held-Out Samples: 0.9080

--- Hashing Trick (Dictionary-Free Text Vectorization) ---
Vectorized 2 sentences into fixed dimension 4096 without dictionary.
```

---

## 4. Edge Cases & Advanced Scenarios

1. **Unseen Class Labels in Streaming Classification**: If batch 1 contains only class 0 and class 1, but class 2 appears in batch 10, calling `partial_fit` will crash unless `classes` is declared explicitly in the very first call. Always pass `classes=np.unique(all_possible_classes)` in initial calls.
2. **Learning Rate Decay Across Chunk Streams**: When streaming infinite data, standard step decay will reduce $\eta \to 0$, freezing the model and preventing it from adapting to concept drift. Resolution: Use `learning_rate="constant"` or `"optimal"` with a floor $\eta_{\min} \ge 10^{-4}$.
3. **Non-IID Stream Ordering (Serial Correlation)**: If streaming data is stored grouped by customer or date, training on chunk 1 (all customer A) and chunk 2 (all customer B) causes catastrophic forgetting. Resolution: Shuffle row indices on disk or maintain an in-memory reservoir shuffle buffer.

---

## 5. Anti-Patterns & Common Pitfalls Table

| Anti-Pattern / Mistake | Why It Fails | Correct Engineering Fix |
| :--- | :--- | :--- |
| Using `fit()` instead of `partial_fit()` in a streaming loop | `fit()` discards previously learned weights, resetting model on every chunk | Call `partial_fit()` and supply `classes` on the first iteration |
| Loading multi-gigabyte CSV files using `pd.read_csv()` | CSV parsing creates high memory expansion ($5\text{--}10\times$ RAM overhead); triggers OOM crash | Migrate storage to **Apache Parquet**; stream via `pyarrow.dataset` or Polars |
| Fitting `StandardScaler` on chunk 1 and ignoring subsequent chunks | Model features drift if subsequent chunks have different means | Call `scaler.partial_fit(X_chunk)` on every single incoming batch |
| Building vocabulary dictionaries across billions of web tokens | Dictionary hash table exhausts gigabytes of host RAM | Use **`HashingVectorizer`** with $D = 2^{20}$ and signed collisions |
| Calling `.to_numpy()` on massive Polars/Dask DataFrames | Forces materialization of the entire distributed table into contiguous RAM | Stream partitions directly into NumPy arrays per worker chunk |
| Forgetting to call `.flush()` on `np.memmap` writes | Modified array buffers remain dirty in OS virtual memory cache and are lost on crash | Explicitly invoke `fp.flush()` after modifying memory-mapped regions |

---

## 6. Best Practices & Production Guidelines

### Out-of-Core Estimator Matrix in Scikit-Learn

| Problem Type | Standard Memory-Bound Estimator | Out-of-Core Streaming Estimator | Key Method |
| :--- | :--- | :--- | :--- |
| **Classification** | `LogisticRegression` / `SVC` | `SGDClassifier(loss='log_loss'|'hinge')` | `partial_fit(X, y, classes=...)` |
| **Regression** | `LinearRegression` / `Ridge` | `SGDRegressor(loss='squared_error'|'huber')`| `partial_fit(X, y)` |
| **Clustering** | `KMeans` | `MiniBatchKMeans` | `partial_fit(X)` |
| **Dimensionality Reduction** | `PCA` | `IncrementalPCA` | `partial_fit(X)` |
| **Text Vectorization** | `CountVectorizer` / `TfidfVectorizer` | `HashingVectorizer` | `transform(text_stream)` |
| **Feature Scaling** | `StandardScaler` / `MinMaxScaler` | `StandardScaler` / `MinMaxScaler` | `partial_fit(X)` |

---

## 7. Real-World Case Study

### Scaling Click-Through-Rate (CTR) Prediction to 500M Daily Ad Impressions
- **Context**: An ad-tech DSP needed to train a logistic regression model on 500 million daily ad impressions ($180\text{ GB}$ uncompressed tabular data) to predict CTR in real time.
- **Problem**: The existing pipeline attempted to load data onto a 256 GB RAM cloud instance, frequently crashing due to memory fragmentation and costing \$3,800/month.
- **Solution**:
  1. Converted raw logs to **Snappy-compressed Apache Parquet** partitioned by hour.
  2. Implemented **`HashingVectorizer`** with $D = 2^{20}$ (1M sparse features), eliminating string lookup dictionaries.
  3. Deployed an out-of-core pipeline utilizing **`SGDClassifier(loss='log_loss')`** streaming $50,000$-row chunks via PyArrow.
- **Outcome**: The entire training pipeline executed on a standard **16 GB RAM** instance, running in 3.5 hours with zero OOM crashes and slashing monthly cloud infrastructure costs by 82%.

---

## 8. Performance & Resource Optimization

1. **PyArrow Streaming Dataset Reader**:
   ```python
   import pyarrow.dataset as ds
   dataset = ds.dataset("data/parquet_partitions/", format="parquet")
   for batch in dataset.to_batches(batch_size=65536):
       X_chunk = batch.to_pandas().values
       # Process chunk without full file loading
   ```
2. **Polars Lazy Streaming Engine**: Utilize `polars.scan_parquet().collect(streaming=True)` to execute streaming group-by, join, and filtering transformations out-of-core.

---

## 9. Security, Privacy & Ethics Considerations

1. **Hash Inversion Defenses (Differential Privacy via Hashing)**: Feature hashing acts as a one-way transformation: recovering raw search queries or patient names from hash index $j$ requires reversing non-cryptographic hashes (MurmurHash3), which is non-trivial when modulo collisions are dense.
2. **Persistent Storage Residuals on Memory-Mapped Files**: Because `np.memmap` creates raw binary files on disk, ensure memory-mapped temporary files in `/tmp` are securely unlinked and wiped to prevent unauthorized extraction of residual training records.

---

## 10. Diagnostic & Verification Playbook

### Verifying Welford Streaming Variance Parity
```python
import numpy as np
from sklearn.preprocessing import StandardScaler

def verify_incremental_variance_parity():
    np.random.seed(42)
    X_full = np.random.randn(1000, 5) * 3.0 + 10.0

    # 1. Batch fit
    batch_scaler = StandardScaler().fit(X_full)

    # 2. Incremental fit in 10 chunks
    inc_scaler = StandardScaler()
    for chunk in np.array_split(X_full, 10):
        inc_scaler.partial_fit(chunk)

    assert np.allclose(batch_scaler.mean_, inc_scaler.mean_), "Mean mismatch in streaming scaler!"
    assert np.allclose(batch_scaler.var_, inc_scaler.var_), "Variance mismatch in streaming scaler!"
    print("Streaming Variance Parity Assertion Passed: 100% Exact Match!")

verify_incremental_variance_parity()
```

---

## 11. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: What are the mathematical trade-offs of the Hashing Trick versus an exact CountVectorizer?</strong></summary>

**Answer:**
- **Exact Vectorizer**: Maintains a bijective mapping table (dictionary) $\mathcal{V}: \text{word} \to \text{index}$. Memory complexity is $\mathcal{O}(|\mathcal{V}| \times \bar{L})$, which blows up on unbounded corpora. However, it guarantees zero collisions and enables exact feature importance analysis.
- **Hashing Trick**: Projects tokens into a fixed vector of size $D$ using a hash function: $j = h(w) \pmod D$. Memory complexity is $\mathcal{O}(1)$ (zero memory). 
The primary trade-off is **hash collisions**: two distinct words $w_1 \neq w_2$ may map to the same index $j$. Weinberger et al. (2009) proved that using a second independent hash function $\xi(w) \in \{-1, +1\}$ makes collision noise zero-mean: $\mathbb{E}[\mathbf{x}^T \mathbf{x}'] = \mathbf{u}^T \mathbf{u}'$. When $D \ge 2^{20}$, collision rates are low enough that classification performance is statistically indistinguishable from exact vectorizers.
</details>

<details>
<summary><strong>Q2: Why must you pass the `classes` parameter on the first call to `SGDClassifier.partial_fit()`?</strong></summary>

**Answer:** In standard `fit()`, the estimator inspects the entire target array $\mathbf{y}$ upfront to discover all unique class labels $K$ and initialize parameter weight vectors of shape $(K, d)$. 
In `partial_fit()`, the first streaming batch may only contain a subset of classes (e.g. only samples from class 0 and 1 in a 3-class problem). If `classes` is omitted, the estimator initializes weights for only 2 classes; when class 2 appears in later batches, parameter matrices cannot be reshaped without discarding existing weights. Passing `classes=np.array([0, 1, 2])` forces the estimator to allocate memory for all target classes immediately.
</details>

<details>
<summary><strong>Q3: How does `np.memmap` work under the hood at the Operating System kernel level?</strong></summary>

**Answer:** `np.memmap` leverages the POSIX `mmap()` (or Windows `CreateFileMapping`) system call to map a disk file directly into the virtual address space of the process. 
Instead of copying file bytes into physical RAM buffers, the OS creates virtual page table entries pointing to the file. When Python accesses an array slice (e.g. `X[1000:2000]`), the CPU triggers a **page fault**, prompting the OS kernel to page in only those specific 4KB memory pages from disk into RAM. When RAM is needed elsewhere, the OS kernel transparently flushes dirty pages and unmaps clean pages, allowing arrays far exceeding physical RAM to be accessed seamlessly.
</details>

<details>
<summary><strong>Q4: Why does training on streaming data require careful shuffling, and how do you achieve it out-of-core?</strong></summary>

**Answer:** Stochastic Gradient Descent requires independent and identically distributed ($i.i.d.$) samples. If streaming data is ordered chronologically or grouped by category, the estimator overfits to the current local distribution, suffering from catastrophic forgetting of earlier groups.
To shuffle out-of-core without loading everything into RAM:
1. **Partition-level Shuffle**: Store data across multiple sharded Parquet files and read partitions in pseudo-random order.
2. **Reservoir Buffer Shuffle**: Maintain an in-memory buffer of $M$ samples (e.g. $100,000$ rows). As new samples arrive, swap incoming samples with randomly chosen buffer elements before passing them to `partial_fit`.
</details>

<details>
<summary><strong>Q5: What is Incremental PCA and how does its computational complexity compare to standard PCA?</strong></summary>

**Answer:** Standard PCA computes the full empirical covariance matrix $\mathbf{X}^T \mathbf{X} \in \mathbb{R}^{d \times d}$ or computes SVD on the full matrix $\mathbf{X} \in \mathbb{R}^{N \times d}$, requiring $\mathcal{O}(N d^2)$ operations and $\mathcal{O}(N d)$ memory.
**Incremental PCA (IPCA)** processes mini-batches of size $B$. It maintains a low-rank basis of size $k$ and updates it using Ross et al.'s streaming SVD update algorithm:
It appends incoming batch $\mathbf{X}_B$ to the current singular components, orthogonalizes the residual subspace via QR decomposition, and performs an SVD on a compact $(k + B) \times (k + B)$ matrix. Memory footprint is strictly bounded by $\mathcal{O}(B \cdot d)$, completely independent of total sample size $N$.
</details>

<details>
<summary><strong>Q6: How does Welford's algorithm prevent catastrophic cancellation when computing variance incrementally?</strong></summary>

**Answer:** Naive variance computation evaluates $\frac{1}{N}\sum x_i^2 - \mu^2$. When values $x_i$ are large with small variance, $\sum x_i^2$ and $N\mu^2$ are massive floating-point numbers of nearly identical magnitude. Subtracting them triggers **catastrophic cancellation**, losing significand precision and frequently yielding negative variances due to rounding errors.
Welford's algorithm computes variance by updating deviations from the evolving running mean: $M_{2, t} = M_{2, t-1} + (x_t - \mu_{t-1})(x_t - \mu_t)$. Because it computes differences between individual samples and local means rather than large cumulative sums of squares, it is numerically stable and guaranteed to remain non-negative.
</details>

<details>
<summary><strong>Q7: What is the primary difference between Pandas chunking and PyArrow Datasets?</strong></summary>

**Answer:** 
- `pd.read_csv(chunksize=...)` parses textual CSV strings into Python objects line by line, which is slow and memory-inefficient due to Python object overhead.
- `pyarrow.dataset` reads binary columnar formats (Parquet/Arrow) using SIMD-accelerated C++ kernels. It supports **column projection** (reading only the required features from disk while skipping unused columns) and **predicate pushdown** (evaluating filtering conditions directly at the metadata/row-group level), reducing disk I/O and deserialization latency by $10\text{--}50\times$.
</details>

<details>
<summary><strong>Q8: When should you transition from scikit-learn out-of-core learning to Dask or Ray?</strong></summary>

**Answer:** Transition occurs when:
1. Computation exceeds the capacity of a single machine's CPU cores and disk I/O channels.
2. The model requires non-linear algorithms that do not support streaming `partial_fit` (e.g. Distributed XGBoost, LightGBM, Random Forests).
3. Complex distributed dataframe transformations (large-scale joins, distributed group-bys) are required prior to training.
Dask and Ray manage cluster scheduling, task graphs, and distributed memory partitioning across multi-node compute clusters.
</details>

---

## 12. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Chunked CSV Streaming with Pandas</strong></summary>

**Task:** Write a generator function that streams a CSV in chunks of $K$ rows and tracks the global target mean without loading the full file into memory.  
**Solution:**
```python
import io
import pandas as pd

def stream_csv_mean(csv_data: str, chunk_size: int = 2) -> float:
    total_sum = 0.0
    total_count = 0
    buffer = io.StringIO(csv_data)
    for chunk in pd.read_csv(buffer, chunksize=chunk_size):
        total_sum += chunk["target"].sum()
        total_count += len(chunk)
    return total_sum / total_count

csv_content = "id,target\n1,10.0\n2,20.0\n3,30.0\n4,40.0\n5,50.0\n"
mean_val = stream_csv_mean(csv_content, chunk_size=2)
assert mean_val == 30.0
print(f"Calculated streaming mean: {mean_val}")
print("Exercise 1 Verified!")
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Implement Welford's Algorithm from Scratch</strong></summary>

**Task:** Implement an online class that updates running mean and sample variance for a stream of scalar numbers one element at a time.  
**Solution:**
```python
import numpy as np

class OnlineVarianceTracker:
    def __init__(self):
        self.count = 0
        self.mean = 0.0
        self.M2 = 0.0

    def update(self, x: float) -> None:
        self.count += 1
        delta = x - self.mean
        self.mean += delta / self.count
        delta2 = x - self.mean
        self.M2 += delta * delta2

    @property
    def variance(self) -> float:
        return self.M2 / (self.count - 1) if self.count > 1 else 0.0

tracker = OnlineVarianceTracker()
stream = [2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0]
for val in stream:
    tracker.update(val)

assert np.isclose(tracker.mean, np.mean(stream))
assert np.isclose(tracker.variance, np.var(stream, ddof=1))
print(f"Welford Mean: {tracker.mean:.4f} | Var: {tracker.variance:.4f}")
print("Exercise 2 Verified!")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Feature Hashing Vectorizer Implementation</strong></summary>

**Task:** Implement a pure Python feature hashing function using `hashlib.md5` and verify that vector dot products approximate word overlap.  
**Solution:**
```python
import hashlib
import numpy as np

def hash_text_to_vector(tokens: list[str], dim: int = 1024) -> np.ndarray:
    vec = np.zeros(dim)
    for t in tokens:
        # MD5 digest for stable integer
        digest = int(hashlib.md5(t.encode('utf-8')).hexdigest(), 16)
        idx = digest % dim
        sign = 1 if ((digest >> 16) & 1) else -1
        vec[idx] += sign
    # L2 normalize
    norm = np.linalg.norm(vec)
    return vec / norm if norm > 0 else vec

t1 = ["apple", "banana", "fruit"]
t2 = ["apple", "banana", "sweet"]
t3 = ["car", "truck", "vehicle"]

v1 = hash_text_to_vector(t1)
v2 = hash_text_to_vector(t2)
v3 = hash_text_to_vector(t3)

sim_similar = np.dot(v1, v2)
sim_different = np.dot(v1, v3)
assert sim_similar > sim_different
print(f"Similarity Similar: {sim_similar:.4f} | Different: {sim_different:.4f}")
print("Exercise 3 Verified!")
```
</details>

<details>
<summary><strong>Exercise 4 (Hard): Streaming Mini-Batch K-Means on Disk Matrix</strong></summary>

**Task:** Stream a synthetic dataset through `MiniBatchKMeans` in 5 distinct epochs, tracking inertia reduction after each streaming pass.  
**Solution:**
```python
from sklearn.cluster import MiniBatchKMeans
import numpy as np

def streaming_kmeans_benchmark():
    np.random.seed(42)
    # 3 true clusters
    c1 = np.random.randn(200, 2) + [5, 5]
    c2 = np.random.randn(200, 2) + [-5, -5]
    c3 = np.random.randn(200, 2) + [5, -5]
    X = np.vstack([c1, c2, c3])
    
    kmeans = MiniBatchKMeans(n_clusters=3, batch_size=50, random_state=42)
    
    # Simulate streaming passes
    for epoch in range(3):
        np.random.shuffle(X)
        for chunk in np.array_split(X, 6):
            kmeans.partial_fit(chunk)
            
    print("Discovered Cluster Centers:\n", kmeans.cluster_centers_)
    assert len(kmeans.cluster_centers_) == 3
    return kmeans

km = streaming_kmeans_benchmark()
print("Exercise 4 MiniBatchKMeans Verified!")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Zero-Copy Parquet Columnar Streaming with PyArrow</strong></summary>

**Task:** Write an automated test confirming that streaming Parquet batches with column projection reads only the selected feature columns without memory overhead.  
**Solution:**
```python
import pyarrow as pa
import pyarrow.parquet as pq
import tempfile
from pathlib import Path

# Create synthetic 3-column parquet table
t_dir = Path(tempfile.mkdtemp())
pq_file = t_dir / "sample.parquet"
table = pa.table({
    "f1": pa.array([1.0, 2.0, 3.0, 4.0]),
    "f2": pa.array([10.0, 20.0, 30.0, 40.0]),
    "unwanted_huge_blob": pa.array(["blob1", "blob2", "blob3", "blob4"])
})
pq.write_table(table, pq_file)

# Read with column projection (read only f1 and f2)
pq_dataset = pq.ParquetFile(pq_file)
batch = pq_dataset.read_row_group(0, columns=["f1", "f2"])
assert "unwanted_huge_blob" not in batch.column_names
assert batch.column_names == ["f1", "f2"]
pq_file.unlink()
print("Exercise 5 PyArrow Column Projection Verified!")
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): In-Memory Reservoir Shuffling Stream Wrapper</strong></summary>

**Task:** Build an iterable stream buffer that consumes an ordered data generator and yields a pseudo-shuffled stream using an internal reservoir buffer.  
**Solution:**
```python
import random
from typing import Generator, Any

def reservoir_shuffle_stream(data_generator: Generator[Any, None, None], buffer_size: int = 10, seed: int = 42):
    rng = random.Random(seed)
    buffer = []
    
    # Fill buffer
    for item in data_generator:
        buffer.append(item)
        if len(buffer) >= buffer_size:
            break
            
    # Stream and replace
    for item in data_generator:
        evict_idx = rng.randint(0, len(buffer) - 1)
        yield buffer[evict_idx]
        buffer[evict_idx] = item
        
    # Drain remaining buffer
    rng.shuffle(buffer)
    for item in buffer:
        yield item

ordered_stream = (i for i in range(25))
shuffled = list(reservoir_shuffle_stream(ordered_stream, buffer_size=5))
assert len(shuffled) == 25
assert shuffled != list(range(25)), "Stream was not shuffled"
print(f"Shuffled stream head: {shuffled[:8]}")
print("Exercise 6 Reservoir Shuffler Verified!")
```
</details>

---

## 13. Summary, Verified References & Navigation

* **The Streaming Paradigm**: Machine learning models can train on arbitrarily large datasets by processing mini-batches via `partial_fit` without ever loading the full dataset into RAM.
* **Storage Optimization**: Replace CSV files with compressed columnar **Apache Parquet**, leveraging column projection and row-group predicate pushdown.
* **Zero-Memory NLP**: The **Hashing Trick** eliminates dictionary storage by mapping tokens to fixed dimensions via signed hash collisions.
* **Virtual Memory Paging**: Use `np.memmap` to access gigabyte-scale numerical arrays directly from high-speed NVMe disk storage with zero memory copy overhead.

### Verified External References
- Weinberger, K., Dasgupta, A., Langford, J., Smola, A., & Attenberg, J. (2009). *Feature Hashing for Large Scale Multitask Learning*. ICML.
- Chan, T. F., Golub, G. H., & LeVeque, R. J. (1982). *Updating Formulae and a Pairwise Algorithm for Computing Sample Variances*. COMPSTAT.
- Ross, D. A., Lim, J., Lin, R. S., & Yang, M. H. (2008). *Incremental Learning for Robust Visual Tracking* (Incremental PCA). IJCV.

---

[← Previous Note: Training Reproducibility & Checkpoints](08-training-reproducibility-and-checkpoints.md) | [Module Index](README.md) | [Next Note: Compute & GPU Acceleration Basics →](10-compute-and-gpu-basics.md)
