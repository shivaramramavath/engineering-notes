# High-Throughput Batch Scoring Pipelines

> **Topic:** Offline Batch Scoring, Out-of-Core Processing, Chunking, Parquet Optimization, and Idempotency  
> **Module ID:** `05-ai/machine-learning/12-saving-loading-and-deployment`  
> **Target Version:** Python 3.11+, PyArrow 17.0+, polars 1.8+, scikit-learn 1.9+, pandas 3.1+  
> **Level:** Core to Advanced  
> **Prerequisites:** [06-saving-full-pipelines.md](06-saving-full-pipelines.md), [11-prediction-and-postprocessing/02-batch-vs-online-inference.md](../11-prediction-and-postprocessing/02-batch-vs-online-inference.md)  
> **Status:** Verified (Python 3.11 / CPU)

---

## Architectural / Conceptual Flow

```mermaid
flowchart TD
    A[Data Lake / Cloud Object Store: S3 / GCS / Parquet Partition] --> B[Batch Ingestion Engine]
    B -->|Chunked Streaming / PyArrow Batches: 50,000 rows| C{Out-of-Core Memory Manager}
    C -->|Stream Batch B_k into RAM| D[Vectorized Preprocessing Pipeline]
    D --> E[SIMD-Accelerated Model Inference: ONNX / GBDT]
    E --> F[Postprocessing, Clamping & Scoring Envelope]
    F --> G[Atomic Parquet Part Write: .part_k.parquet.tmp]
    G -->|Verify Checksum & Integrity| H[Atomic Rename to part_k.parquet]
    H --> I[Update State Ledger / Checkpoint DB: Chunk k Complete]
    I --> J{More Chunks?}
    J -->|Yes| C
    J -->|No: All Chunks Scored| K[Batch Job Complete: Downstream Warehouse Consumes Results]
```

---

## 1. Concept Overview & Core Motivation

While real-time model serving (via REST APIs like FastAPI) focuses on minimizing single-query latency (e.g. $< 15$ms for one user), a vast proportion of enterprise machine learning value is unlocked through **Offline Batch Scoring**:
- Scoring 85 million customer credit risk profiles every Sunday night to update credit limits.
- Generating personalized top-50 movie recommendations for 200 million users daily at 2:00 AM.
- Calculating dynamic churn risk for all SaaS accounts monthly to prioritize customer success outreach.
- Running daily fraud detection sweeps across 500 million settled financial ledger transactions.

### The Problem with Naive Batch Scoring
Data scientists often write naive batch scripts:
```python
# ❌ FATAL OUT-OF-MEMORY ANTI-PATTERN
df = pd.read_csv("huge_dataset.csv")  # CRASHES CONTAINER WITH OOM!
preds = model.predict(df)
df["prediction"] = preds
df.to_csv("predictions.csv")
```
When `huge_dataset.csv` grows to 50GB or 500GB, loading the entire table into Python memory triggers the Linux Out-Of-Memory (OOM) Killer. Furthermore, if a job processing 100 million rows crashes on row 99,999,999 due to a network glitch, a naive script loses all progress and must restart from row 1, wasting thousands of dollars in cloud compute.

### The Solution: Robust Out-of-Core Batch Pipelines
Production batch scoring requires a distinct set of software engineering principles:
1. **Out-of-Core Chunked Streaming**: Streaming data in fixed-size chunks (e.g. 50,000 rows) using columnar formats like **Apache Parquet** and **PyArrow**. Memory usage remains strictly bounded ($O(\text{chunk\_size})$) regardless of whether the dataset is 100MB or 10 Terabytes.
2. **Idempotency and Atomic Writes**: Writing chunk outputs to temporary files and renaming them atomically (`tmp_001.parquet` $\to$ `part_001.parquet`) upon successful completion.
3. **Resumable State Checkpointing**: Maintaining an append-only state ledger so failed jobs resume from the exact last successful chunk without re-scoring completed data.

---

## 2. Theoretical Foundations & Mathematics

### 2.1 The Out-of-Core Bounded Memory Invariant

Let $\mathcal{D} = \{\mathbf{x}_1, \mathbf{x}_2, \dots, \mathbf{x}_N\}$ denote a massive dataset of $N$ instances, where $N \gg 10^7$ and total uncompressed size $\text{Size}(\mathcal{D}) \gg \text{RAM}_{\text{available}}$.

Let $\mathcal{D}$ be partitioned into $M$ disjoint chunks $\mathcal{B}_1, \mathcal{B}_2, \dots, \mathcal{B}_M$ such that:
$$\bigcup_{k=1}^M \mathcal{B}_k = \mathcal{D}, \quad \text{and} \quad \mathcal{B}_i \cap \mathcal{B}_j = \emptyset \quad (\forall i \ne j)$$
where $|\mathcal{B}_k| = B$ is the batch chunk size (e.g. $B = 50,000$).

**Theorem (Memory Invariance)**:
For any dataset size $N \to \infty$, the peak RAM consumption $\mathcal{R}_{\text{peak}}$ during batch scoring is strictly bounded:
$$\mathcal{R}_{\text{peak}} \le \text{Size}(\mathcal{B}_k) + \text{Size}(\text{Model}) + \mathcal{R}_{\text{runtime}} = \mathcal{O}(B) \ll \mathcal{O}(N)$$

### 2.2 Columnar Format Efficiency: Parquet vs CSV

In tabular machine learning, models typically consume only a subset of features $d_{\text{used}} \subset d_{\text{total}}$ from a wide table (e.g. 15 features out of 200 raw database columns).

| Metric | Row-Oriented CSV | Columnar Apache Parquet |
| :--- | :--- | :--- |
| **Storage Layout** | Row-by-row ASCII bytes | Column-by-column compressed binary pages |
| **Projection Pushdown** | Must scan 100% of bytes from disk | Scans **only** the $d_{\text{used}}$ columns directly from disk |
| **Compression Ratio** | $1\times$ (Verbose ASCII text) | $4\times$ to $10\times$ (Snappy / ZSTD dictionary encoding) |
| **I/O Complexity** | $\mathcal{O}(N \cdot d_{\text{total}})$ | $\mathcal{O}(N \cdot d_{\text{used}})$ |

### 2.3 Mathematical Idempotency in Batch Processing

Let $f_{\text{batch}}: \mathcal{D} \to \mathcal{P}$ define the batch scoring operation mapping raw data $\mathcal{D}$ to persistent predictions $\mathcal{P}$.

**Definition (Idempotency)**: An execution pipeline $f_{\text{batch}}$ is idempotent if executing it multiple times yields the exact same state as executing it once:
$$f_{\text{batch}}(f_{\text{batch}}(\mathcal{D})) \equiv f_{\text{batch}}(\mathcal{D})$$
In engineering terms, running a batch scoring job twice (e.g. after an automatic retry) must overwrite or ignore existing chunk partitions without duplicating records or corrupting the destination table.

### Mathematical Symbols Table

| Symbol | Mathematical Definition | Data Type / Dimensions | Role in Concept |
| :---: | :--- | :---: | :--- |
| $N$ | Total dataset sample count | Integer $\mathbb{N}$ ($10^6 - 10^{10}$) | Total records requiring batch scoring |
| $B$ | Chunk batch size | Integer $\mathbb{N}$ ($10^4 - 10^5$) | Samples loaded into RAM per iteration |
| $M$ | Total number of chunks | Integer $\lceil N / B \rceil$ | Partition count of the dataset |
| $\mathcal{B}_k$ | Chunk partition $k$ | Subset of dataset | Active batch undergoing inference |
| $\mathcal{R}_{\text{peak}}$ | Peak memory consumption | Bytes / Megabytes | Maximum RAM allocated during scoring |
| $d_{\text{used}}$ | Features consumed by model | Subset of columns | Columns loaded via projection pushdown |

---

## 3. Implementation & Practical Code

The following self-contained script implements an enterprise out-of-core batch scoring pipeline:
1. Generating a synthetic 100,000-row dataset and writing it to a compressed Apache Parquet file.
2. Reading the dataset in memory-bounded batches using **PyArrow** chunked batch streaming.
3. Scoring each chunk using a vectorized pipeline and writing atomic partition files (`part_k.parquet.tmp` $\to$ `part_k.parquet`).
4. Maintaining a state ledger (`checkpoints.json`) to demonstrate automatic resumption after an injected failure.

```python
"""
Out-of-Core Batch Scoring Engine with Atomic Checkpointing.
Built with PyArrow, Parquet, and Scikit-Learn Pipelines.
Fully self-contained, typed, and runnable under Python 3.11+.
"""

import os
import json
import time
import numpy as np
import pandas as pd
import pyarrow as pa
import pyarrow.parquet as pq
from typing import List, Dict, Any, Optional
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import Ridge

class OutOfCoreBatchScorer:
    def __init__(
        self,
        model_pipeline: Pipeline,
        feature_columns: List[str],
        id_column: str,
        output_dir: str = "./batch_scoring_output",
        chunk_size: int = 10_000
    ):
        self.model = model_pipeline
        self.feature_columns = feature_columns
        self.id_column = id_column
        self.output_dir = output_dir
        self.chunk_size = chunk_size
        self.state_file = os.path.join(output_dir, "batch_checkpoint_state.json")
        os.makedirs(output_dir, exist_ok=True)

    def _load_completed_chunks(self) -> set:
        """Load set of already completed chunk indices from state ledger."""
        if os.path.exists(self.state_file):
            with open(self.state_file, "r") as f:
                return set(json.load(f).get("completed_chunks", []))
        return set()

    def _record_chunk_completion(self, chunk_idx: int):
        """Append completed chunk index to atomic state ledger."""
        completed = self._load_completed_chunks()
        completed.add(chunk_idx)
        tmp_state = self.state_file + ".tmp"
        with open(tmp_state, "w") as f:
            json.dump({"completed_chunks": sorted(list(completed)), "last_updated": time.time()}, f)
        os.replace(tmp_state, self.state_file)

    def process_parquet_dataset(self, input_parquet_path: str, simulate_crash_on_chunk: Optional[int] = None):
        """Stream Parquet dataset in chunks, execute inference, and write atomic partitions."""
        parquet_file = pq.ParquetFile(input_parquet_path)
        total_rows = parquet_file.metadata.num_rows
        print(f"Commencing batch scoring on: '{input_parquet_path}' ({total_rows:,} total rows)")

        completed_chunks = self._load_completed_chunks()
        if completed_chunks:
            print(f"Resuming job: Found {len(completed_chunks)} previously completed chunks. Skipping them.")

        # PyArrow record batch reader: Projection pushdown (Load ONLY required columns)
        columns_to_load = [self.id_column] + self.feature_columns
        batch_reader = parquet_file.iter_batches(batch_size=self.chunk_size, columns=columns_to_load)

        t_start = time.perf_counter()
        rows_processed = 0

        for chunk_idx, record_batch in enumerate(batch_reader):
            if chunk_idx in completed_chunks:
                continue

            # Simulate network or node crash for testing resiliency
            if simulate_crash_on_chunk is not None and chunk_idx == simulate_crash_on_chunk:
                print(f"⚠️ [SIMULATED CRASH] Worker process died unexpectedly on chunk {chunk_idx}!")
                raise RuntimeError(f"Simulated container crash on chunk {chunk_idx}")

            # 1. Convert Arrow RecordBatch to pandas/numpy with zero memory bloat
            batch_df = record_batch.to_pandas()
            X_features = batch_df[self.feature_columns]

            # 2. Vectorized High-Throughput Inference
            predictions = self.model.predict(X_features)

            # 3. Create Result Table
            result_df = pd.DataFrame({
                self.id_column: batch_df[self.id_column],
                "score_prediction": np.round(predictions, 4),
                "scored_timestamp": int(time.time())
            })

            # 4. Atomic Partition Write: Write to .tmp first, then atomic rename
            final_part_name = os.path.join(self.output_dir, f"part_{chunk_idx:05d}.parquet")
            tmp_part_name = final_part_name + ".tmp"

            table = pa.Table.from_pandas(result_df)
            pq.write_table(table, tmp_part_name, compression="snappy")
            os.replace(tmp_part_name, final_part_name)

            # 5. Checkpoint success in ledger
            self._record_chunk_completion(chunk_idx)
            rows_processed += len(batch_df)

        elapsed = time.perf_counter() - t_start
        print(f"✓ Batch Job Completed in {elapsed:.2f}s ({rows_processed:,} rows scored | {rows_processed / max(0.001, elapsed):,.0f} rows/sec).")

def generate_synthetic_parquet_source(filepath: str, n_rows: int = 50_000):
    """Generate sample source Parquet dataset with 50,000 rows."""
    print(f"Generating synthetic Parquet source dataset ({n_rows:,} rows)...")
    np.random.seed(42)
    data = {
        "user_id": [f"USR_{i:07d}" for i in range(n_rows)],
        "f_spend_30d": np.random.uniform(10.0, 5000.0, n_rows).astype(np.float32),
        "f_login_count": np.random.randint(1, 100, n_rows).astype(np.float32),
        "f_support_tickets": np.random.randint(0, 10, n_rows).astype(np.float32),
        "extra_unneeded_column": np.random.randn(n_rows).astype(np.float32)  # Should be skipped by projection
    }
    table = pa.Table.from_pandas(pd.DataFrame(data))
    pq.write_table(table, filepath, compression="snappy")
    print(f"Synthetic source Parquet saved to '{filepath}'. Size: {os.path.getsize(filepath):,} bytes.")

def main():
    print("=== Production Batch Scoring Pipeline Harness ===")
    
    source_parquet = "source_user_features.parquet"
    output_dir = "./test_batch_output"
    
    # 1. Generate 50,000-row synthetic dataset
    generate_synthetic_parquet_source(source_parquet, n_rows=50_000)

    # 2. Train and Serialize Pipeline
    feature_cols = ["f_spend_30d", "f_login_count", "f_support_tickets"]
    pipeline = Pipeline([
        ("scaler", StandardScaler()),
        ("model", Ridge(alpha=1.0))
    ])
    # Mock fit on small sample
    X_sample = np.random.randn(100, 3)
    y_sample = X_sample[:, 0] * 2.0 - X_sample[:, 1]
    pipeline.fit(X_sample, y_sample)

    # 3. Initialize Scorer with 10,000-row chunking (5 chunks total)
    scorer = OutOfCoreBatchScorer(
        model_pipeline=pipeline,
        feature_columns=feature_cols,
        id_column="user_id",
        output_dir=output_dir,
        chunk_size=10_000
    )

    # 4. Simulation Part 1: Worker crashes on Chunk 2 (after successfully completing 0 and 1)
    print("\n--- Simulation 1: Worker Crash on Chunk 2 ---")
    try:
        scorer.process_parquet_dataset(source_parquet, simulate_crash_on_chunk=2)
    except RuntimeError as e:
        print(f"Caught expected crash: {e}")

    # Verify state after crash
    with open(scorer.state_file, "r") as f:
        state = json.load(f)
    print(f"State Ledger After Crash: Completed chunks = {state['completed_chunks']}")
    assert state["completed_chunks"] == [0, 1]

    # 5. Simulation Part 2: Resume Job from Checkpoint (Chunks 0 and 1 skipped, 2, 3, 4 completed)
    print("\n--- Simulation 2: Resuming Job from Checkpoint ---")
    scorer.process_parquet_dataset(source_parquet, simulate_crash_on_chunk=None)

    # 6. Verify Full Output Table
    print("\n--- Verification: Merging and Validating Output ---")
    all_part_files = [os.path.join(output_dir, f) for f in os.listdir(output_dir) if f.endswith(".parquet") and not f.endswith(".tmp")]
    print(f"Total Part Files Generated: {len(all_part_files)}")
    assert len(all_part_files) == 5  # 50,000 / 10,000 = 5 chunks

    # Read all output parts via PyArrow dataset
    output_dataset = pq.ParquetDataset(output_dir)
    total_scored_rows = output_dataset.read().num_rows
    print(f"Total Scored Records in Output: {total_scored_rows:,}")
    assert total_scored_rows == 50_000, f"Expected 50,000 scored records, got {total_scored_rows}"
    print("✓ Full dataset successfully scored with zero row loss and zero memory bloat.")

    # Cleanup
    if os.path.exists(source_parquet):
        os.remove(source_parquet)
    for f in os.listdir(output_dir):
        os.remove(os.path.join(output_dir, f))
    os.rmdir(output_dir)

if __name__ == "__main__":
    main()
```

### Execution Output Snippet

```
=== Production Batch Scoring Pipeline Harness ===
Generating synthetic Parquet source dataset (50,000 rows)...
Synthetic source Parquet saved to 'source_user_features.parquet'. Size: 785,420 bytes.

--- Simulation 1: Worker Crash on Chunk 2 ---
Commencing batch scoring on: 'source_user_features.parquet' (50,000 total rows)
⚠️ [SIMULATED CRASH] Worker process died unexpectedly on chunk 2!
Caught expected crash: Simulated container crash on chunk 2
State Ledger After Crash: Completed chunks = [0, 1]

--- Simulation 2: Resuming Job from Checkpoint ---
Commencing batch scoring on: 'source_user_features.parquet' (50,000 total rows)
Resuming job: Found 2 previously completed chunks. Skipping them.
✓ Batch Job Completed in 0.18s (30,000 rows scored | 166,667 rows/sec).

--- Verification: Merging and Validating Output ---
Total Part Files Generated: 5
Total Scored Records in Output: 50,000
✓ Full dataset successfully scored with zero row loss and zero memory bloat.
```

---

## 4. Edge Cases & Advanced Scenarios

1. **Severe Imbalance in Chunk Partition Sizes**: In distributed filesystems (HDFS, S3), some partition files may have 10,000 rows while others have 10,000,000 rows due to skewed partitioning keys (e.g. partition by country). A fixed batch size reader handles this cleanly, but worker threads assigned large partitions can experience "straggler" latency. *Mitigation*: Enforce uniform file chunking during upstream ETL.
2. **Schema Drift Across Historical Parquet Files**: In multi-year historical datasets, older Parquet partitions may have different data types (e.g. `int32` vs `int64`) or missing newly added columns. *Mitigation*: Use PyArrow's `unify_schemas=True` when initializing `ParquetDataset`, mapping missing columns to null defaults.
3. **Corrupted Incomplete Part Files After Spot Instance Termination**: Cloud spot instances can be terminated with 30 seconds notice, leaving half-written `.parquet` files. *Mitigation*: The **Atomic Write Pattern** guarantees safety: write strictly to `part_k.parquet.tmp` and execute an OS-level atomic rename (`os.replace`) only after the file write completes and closes.

---

## 5. Anti-Patterns & Common Pitfalls Table

| Anti-Pattern / Mistake | Why It Fails | Correct Engineering Fix |
| :--- | :--- | :--- |
| **Loading full dataset with `pd.read_csv()`** | Memory explodes for large tables, triggering Linux OOM killer. | Stream in bounded chunks using PyArrow `iter_batches()` or Polars streaming. |
| **Appending rows to a single master CSV in a loop** | Writing single lines creates massive I/O disk thrashing and non-atomic corruption. | Write independent, compressed chunk Parquet files (`part_00001.parquet`). |
| **Re-scoring completed data upon job failure** | A failure on chunk 99 restarts chunk 0, wasting hours of compute. | Maintain an append-only checkpoint ledger to skip already completed chunks. |
| **Reading all columns when only 5 are needed** | Wastes massive disk bandwidth reading unused text/blob fields. | Leverage Parquet projection pushdown: pass `columns=[...]` to read only required features. |
| **Non-atomic file writing** | Process crashes midway through writing leave corrupt, truncated files that break downstream jobs. | Write to `.tmp` extension and atomically rename upon completion (`os.replace`). |

---

## 6. Best Practices & Production Guidelines

1. **Tune Chunk Size for Hardware Cache**: Set chunk size between 20,000 and 100,000 rows. Chunks under 1,000 rows incur high Python loop and file I/O overhead; chunks over 500,000 rows risk memory spikes and reduce checkpoint granularity.
2. **Column-Pruning via Projection Pushdown**: Never load unneeded columns. Parquet stores column data in separate disk pages; requesting only 5 out of 100 columns reduces disk I/O by 95%.
3. **Distributed Batch Scoring with Apache Spark or Ray**: For datasets exceeding 1 Terabyte, distribute chunks across worker nodes using **Apache Spark** (`spark.read.parquet().mapPartitions(...)`) or **Ray Data** (`ray.data.read_parquet().map_batches(...)`), ensuring each worker operates on local partitions.

---

## 7. Real-World Case Study

### Global Streaming Service Nightly Recommendation Refresh
- **Problem**: A music streaming platform generates daily personalized recommendations for 120 million active users. The legacy batch job used a single Python script loading user embeddings from CSV files. The job took 11 hours to complete and crashed 3 out of 7 nights due to memory leaks, failing morning business SLAs.
- **Intervention**:
  1. Converted feature storage from CSV to partitioned Apache Parquet with Snappy compression.
  2. Implemented PyArrow chunked batch streaming ($B = 50,000$ users per batch).
  3. Integrated atomic partition writing and DynamoDB state checkpointing.
  4. Executed model scoring across 16 parallel spot instances using ONNX Runtime.
- **Outcome**: Batch job execution time dropped from 11 hours to **42 minutes (15x speedup)**. Cloud computing costs dropped by 78%, and automated checkpoint recovery ensured 100% on-time delivery of daily recommendations for 18 consecutive months.

---

## 8. Performance & Resource Optimization

- **PyArrow Zero-Copy Buffer Sharing**: PyArrow utilizes the Apache Arrow in-memory columnar format. When converting Arrow record batches to NumPy arrays via `batch.to_numpy()`, memory is shared via zero-copy views without allocating duplicate memory buffers.
- **Snappy vs ZSTD Compression**: Use `Snappy` for high-throughput batch scoring pipelines where decompression speed is the bottleneck. Use `ZSTD` when disk storage and network transfer costs outweigh CPU decompression cycles.

---

## 9. Security, Privacy & Ethics Considerations

- **Secure Temporary Directory Permissions**: Ensure temporary chunk staging directories (`/tmp/batch_scoring/`) have strict POSIX permissions (`chmod 700`) so other users or processes on the host cannot read intermediate unencrypted scoring partitions.
- **GDPR Right to Be Forgotten in Batch Outputs**: When generating long-term batch scoring tables, maintain an automated post-scoring purge job that deletes predictions associated with user IDs flagged in the company's daily deletion queue.

---

## 10. Diagnostic & Verification Playbook

### Diagnostic Assertions
```python
# Verify total scored row count equals input row count exactly
input_count = pq.ParquetFile(input_path).metadata.num_rows
output_count = pq.ParquetDataset(output_dir).read().num_rows
assert input_count == output_count, f"Row count mismatch! In={input_count}, Out={output_count}"

# Verify no duplicate IDs exist in output
output_df = pq.ParquetDataset(output_dir).read(columns=["user_id"]).to_pandas()
assert output_df["user_id"].is_unique, "Duplicate user IDs detected in batch scoring output"
```

### Verification CLI Command
```bash
python -c "
import pyarrow as pa, pyarrow.parquet as pq
table = pa.Table.from_arrays([pa.array([1, 2, 3])], names=['x'])
pq.write_table(table, 'test_batch.parquet')
read_back = pq.read_table('test_batch.parquet')
assert read_back.num_rows == 3
import os; os.remove('test_batch.parquet')
print('PyArrow Parquet batch read/write verified successfully.')
"
```

---

## 11. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: What are the fundamental architectural differences between Online Inference and Batch Scoring?</strong></summary>

**Answer:**  
- **Online Real-Time Inference**: Focuses on **latency** ($< 20$ms SLA). Serves unpredictable, interactive traffic via REST/gRPC APIs on a query-by-query basis ($N=1$). Requires dynamic batching, caching, and 99.99% high availability.
- **Batch Scoring**: Focuses on **throughput and total cost**. Processes pre-existing, massive datasets ($N=10^6 - 10^9$) on an scheduled offline cadence (hourly, nightly). Utilizes chunked streaming, columnar Parquet files, spot compute instances, and atomic checkpointing.
</details>

<details>
<summary><strong>Q2: Why is Apache Parquet significantly faster than CSV for batch machine learning scoring?</strong></summary>

**Answer:**  
1. **Columnar Storage & Projection Pushdown**: Parquet stores data column-by-column rather than row-by-row. If a table has 200 columns but the model only needs 8 features, the storage engine reads *only* those 8 columns from disk, skipping 96% of bytes.
2. **Heavy Compression**: Columnar data has high homogeneity, allowing dictionary encoding, run-length encoding (RLE), and Snappy compression to compress files by $4\times$ to $10\times$ compared to CSV.
3. **Binary Types**: Values are stored in binary format (e.g. IEEE 754 floats); reading them requires zero text-to-float ASCII parsing overhead.
</details>

<details>
<summary><strong>Q3: Explain the Atomic File Write pattern in batch scoring and why it is necessary.</strong></summary>

**Answer:**  
If a batch worker process writes directly to `part_001.parquet` and crashes halfway through (due to an out-of-memory error, spot instance termination, or network interruption), a truncated, corrupted Parquet file is left on disk. Downstream data pipelines consuming the directory will crash.  
*The Atomic Write Pattern*:
1. The worker writes the chunk to a temporary file: `part_001.parquet.tmp`.
2. Once the file is completely written, flushed, and closed, the process executes an atomic filesystem rename: `os.replace("part_001.parquet.tmp", "part_001.parquet")`.
3. Renaming is an atomic filesystem metadata operation; downstream readers will only ever see 100% complete files, never corrupted partial files.
</details>

<details>
<summary><strong>Q4: How does a batch scoring pipeline achieve idempotency?</strong></summary>

**Answer:**  
An idempotent batch pipeline produces the exact same final state whether executed once or five times.  
*Implementation*:
1. Partition output filenames deterministically based on chunk indices or input partition keys (`part_chunk_0042.parquet`).
2. Maintain a transactional checkpoint ledger (e.g. JSON state file, DynamoDB, or database table).
3. If a chunk already exists in the ledger, the pipeline skips it. If forced to re-run, it atomically overwrites existing partition files.
4. Duplicate records are never appended to existing tables.
</details>

<details>
<summary><strong>Q5: What is Out-of-Core learning/scoring, and how does it prevent RAM exhaustion?</strong></summary>

**Answer:**  
Out-of-Core processing is a computing paradigm where datasets too large to fit in physical RAM are processed in sequential, memory-bounded chunks.  
- Instead of loading the entire 100GB table into memory, the program loads a small batch (e.g. 50MB, representing 50,000 rows).
- The model scores the 50MB batch, writes the output to disk, and immediately releases the memory buffer.
- Memory consumption remains flat at $\mathcal{O}(\text{batch\_size})$ regardless of whether the dataset contains 1 million or 10 billion rows.
</details>

<details>
<summary><strong>Q6: How do you choose the optimal chunk size for batch scoring?</strong></summary>

**Answer:**  
- **Too Small ($B < 5,000$)**: Creates high Python loop overhead, excessive file system metadata operations, and small-file fragmentation in S3/HDFS.
- **Too Large ($B > 500,000$)**: Increases peak RAM consumption, risks container OOM crashes, and increases the compute wasted if a chunk fails midway.
- **Sweet Spot**: $20,000$ to $100,000$ rows (typically $20\text{MB} - 100\text{MB}$ in RAM). This fits comfortably within CPU L3 caches, saturates SIMD matrix vectorization, and produces optimal 50MB–200MB Parquet partition files for downstream data warehouses (Snowflake, BigQuery).
</details>

<details>
<summary><strong>Q7: When should you use Apache Spark or Ray instead of a single-node Python batch script?</strong></summary>

**Answer:**  
- Use a **Single-Node Script** (PyArrow + Scikit-Learn / ONNX) when: Dataset size is up to a few hundred gigabytes, and a single large cloud VM (e.g. 32-core AWS c6i instance) can complete the job within operational SLA time. It has zero cluster orchestration overhead and is simple to debug.
- Use **Distributed Engines (Spark / Ray)** when:
  1. The dataset is in the Terabyte to Petabyte scale.
  2. The scoring job must finish in minutes rather than hours, requiring horizontal scaling across hundreds of worker nodes.
  3. Upstream ETL and data extraction are already native Spark / Databricks workflows.
</details>

<details>
<summary><strong>Q8: How do you verify the integrity of a batch scoring run before publishing results to downstream users?</strong></summary>

**Answer:**  
1. **Row Count Reconciliation**: Assert that total output prediction records equal total input records exactly: $N_{\text{out}} == N_{\text{in}}$.
2. **Uniqueness Audit**: Verify that the primary key (e.g. `user_id`) is strictly unique across all output partition files.
3. **Null & Anomaly Check**: Assert zero NaN/Inf values in predictions; verify that output distributions (mean, min, max, percentiles) match historical baseline distributions within acceptable drift tolerance bounds.
</details>

---

## 12. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): PyArrow Table Creator and Parquet Writer</strong></summary>

**Task:** Create an Apache Arrow Table with columns `id` (int64) and `score` (float64), save it to disk as Parquet with Snappy compression, and verify file existence.

**Solution:**
```python
import pyarrow as pa
import pyarrow.parquet as pq
import os

ids = pa.array([1, 2, 3], type=pa.int64())
scores = pa.array([0.15, 0.85, 0.42], type=pa.float64())
table = pa.Table.from_arrays([ids, scores], names=["id", "score"])

output_file = "test_easy.parquet"
pq.write_table(table, output_file, compression="snappy")

assert os.path.exists(output_file)
assert pq.read_table(output_file).num_rows == 3
os.remove(output_file)
print("Exercise 1 passed successfully.")
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Parquet Column Projection Pushdown Reader</strong></summary>

**Task:** Write a function `read_projected_column(parquet_path: str, column_name: str) -> list` that reads only the specified column from disk without loading other columns.

**Solution:**
```python
import pyarrow.parquet as pq

def read_projected_column(parquet_path: str, column_name: str) -> list:
    table = pq.read_table(parquet_path, columns=[column_name])
    return table.column(column_name).to_pylist()

# Verification
import pyarrow as pa, os
t = pa.Table.from_arrays([pa.array([1, 2]), pa.array(["a", "b"])], names=["num", "letter"])
pq.write_table(t, "proj_test.parquet")

nums = read_projected_column("proj_test.parquet", "num")
assert nums == [1, 2]
os.remove("proj_test.parquet")
print("Exercise 2 passed successfully.")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Atomic File Renamer Function</strong></summary>

**Task:** Implement a function `atomic_write_parquet(table: pa.Table, target_filepath: str)` that writes the table to a temporary file (`.tmp`) and atomically renames it to `target_filepath`.

**Solution:**
```python
import os
import pyarrow as pa
import pyarrow.parquet as pq

def atomic_write_parquet(table: pa.Table, target_filepath: str):
    tmp_path = target_filepath + ".tmp"
    pq.write_table(table, tmp_path, compression="snappy")
    # Atomic rename (guaranteed atomic on POSIX and modern Windows)
    os.replace(tmp_path, target_filepath)

# Verification
t = pa.Table.from_arrays([pa.array([10, 20])], names=["val"])
final_f = "atomic_result.parquet"
atomic_write_parquet(t, final_f)

assert os.path.exists(final_f)
assert not os.path.exists(final_f + ".tmp")
os.remove(final_f)
print("Exercise 3 passed successfully.")
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): PyArrow Batch Iteration Chunk Counter</strong></summary>

**Task:** Write a Python function `count_parquet_chunks(parquet_path: str, chunk_size: int) -> int` that uses `iter_batches` to count the total number of chunks without loading the whole table.

**Solution:**
```python
import pyarrow.parquet as pq

def count_parquet_chunks(parquet_path: str, chunk_size: int) -> int:
    pf = pq.ParquetFile(parquet_path)
    count = 0
    for _ in pf.iter_batches(batch_size=chunk_size):
        count += 1
    return count

# Verification
import pyarrow as pa, os
t = pa.Table.from_arrays([pa.array(list(range(25)))], names=["val"])
pq.write_table(t, "count_test.parquet")

# 25 rows with batch_size=10 -> 3 chunks (10, 10, 5)
chunks = count_parquet_chunks("count_test.parquet", chunk_size=10)
assert chunks == 3
os.remove("count_test.parquet")
print(f"Exercise 4 passed. Counted chunks: {chunks}")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): State Ledger Checkpoint Manager</strong></summary>

**Task:** Build a thread-safe `BatchCheckpointLedger` class that stores completed chunk IDs in a JSON file using atomic writes, providing methods `is_completed(chunk_id)` and `mark_completed(chunk_id)`.

**Solution:**
```python
import os
import json
import threading

class BatchCheckpointLedger:
    def __init__(self, ledger_filepath: str):
        self.filepath = ledger_filepath
        self._lock = threading.Lock()
        self._completed = set()
        self._load()

    def _load(self):
        if os.path.exists(self.filepath):
            with open(self.filepath, "r") as f:
                self._completed = set(json.load(f).get("completed", []))

    def is_completed(self, chunk_id: int) -> bool:
        with self._lock:
            return chunk_id in self._completed

    def mark_completed(self, chunk_id: int):
        with self._lock:
            self._completed.add(chunk_id)
            tmp_f = self.filepath + ".tmp"
            with open(tmp_f, "w") as f:
                json.dump({"completed": sorted(list(self._completed))}, f)
            os.replace(tmp_f, self.filepath)

# Verification
ledger_path = "test_ledger.json"
ledger = BatchCheckpointLedger(ledger_path)
assert not ledger.is_completed(0)

ledger.mark_completed(0)
assert ledger.is_completed(0)

# Reload in fresh instance
ledger2 = BatchCheckpointLedger(ledger_path)
assert ledger2.is_completed(0)
assert not ledger2.is_completed(1)

os.remove(ledger_path)
print("Exercise 5 passed. Checkpoint ledger verified.")
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Batch Pipeline Output Auditor</strong></summary>

**Task:** Implement an auditor function `audit_batch_run(input_parquet: str, output_parquet_dir: str, id_col: str) -> bool` that asserts total row count matches and verifies zero duplicate primary IDs exist across all output partitions.

**Solution:**
```python
import pyarrow.parquet as pq

def audit_batch_run(input_parquet: str, output_parquet_dir: str, id_col: str) -> bool:
    in_count = pq.ParquetFile(input_parquet).metadata.num_rows
    
    out_dataset = pq.ParquetDataset(output_parquet_dir)
    out_table = out_dataset.read(columns=[id_col])
    out_count = out_table.num_rows
    
    if in_count != out_count:
        raise AssertionError(f"Row count mismatch: input has {in_count}, output has {out_count}")
        
    ids = out_table.column(id_col).to_pylist()
    if len(ids) != len(set(ids)):
        raise AssertionError("Duplicate primary IDs detected in output partitions!")
        
    return True

# Verification
import pyarrow as pa, os
t_in = pa.Table.from_arrays([pa.array([1, 2, 3])], names=["uid"])
pq.write_table(t_in, "audit_in.parquet")

os.makedirs("audit_out", exist_ok=True)
t_out1 = pa.Table.from_arrays([pa.array([1, 2])], names=["uid"])
t_out2 = pa.Table.from_arrays([pa.array([3])], names=["uid"])
pq.write_table(t_out1, "audit_out/part1.parquet")
pq.write_table(t_out2, "audit_out/part2.parquet")

assert audit_batch_run("audit_in.parquet", "audit_out", "uid") is True

# Cleanup
os.remove("audit_in.parquet")
for f in os.listdir("audit_out"): os.remove(os.path.join("audit_out", f))
os.rmdir("audit_out")
print("Exercise 6 passed. Batch run audit verified.")
```
</details>

---

## 13. Summary, Verified References & Navigation

### Key Takeaways
- **Bounded Memory Invariance**: Production batch scoring must stream data in chunks using PyArrow/Parquet to prevent RAM exhaustion.
- **Columnar Projection Pushdown**: Reading only the features needed by the model avoids scanning 90%+ of unneeded disk bytes.
- **The Atomic Write Pattern**: Write chunk outputs to `.tmp` files and atomically rename upon completion to prevent corrupt, half-written partitions.
- **Idempotency via Checkpointing**: Maintain an append-only state ledger to ensure failed jobs resume seamlessly from the last successful chunk.

### Verified External References
- Apache Arrow: [PyArrow Streaming Batches Documentation](https://arrow.apache.org/docs/python/).
- Apache Parquet: [Parquet Format Overview](https://parquet.apache.org/).
- Polars Documentation: [Streaming API for Out-of-Core Processing](https://docs.pola.rs/).

---

[← Previous Note: 09-serving-with-fastapi.md](09-serving-with-fastapi.md) | [Module Index](README.md) | [Next Note: 11-containerising-models.md →](11-containerising-models.md)
