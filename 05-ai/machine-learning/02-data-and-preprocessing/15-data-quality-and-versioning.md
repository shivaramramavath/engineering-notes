---
title: "Data Quality, Testing, and Versioning (DVC)"
description: "Production methodologies for asserting tabular data quality, continuous validation with Pandera/Great Expectations, detecting covariate shift, and tracking immutable data with DVC."
track: "machine-learning"
module: "02-data-and-preprocessing"
topic: "Data Quality and Versioning"
order: 15
format: "concept"
level: "intermediate"
tags: ["data-quality", "pandera", "great-expectations", "dvc", "data-versioning", "data-contracts", "schema-validation"]
prerequisites: [
  "05-ai/machine-learning/02-data-and-preprocessing/03-exploratory-data-analysis.md",
  "05-ai/machine-learning/02-data-and-preprocessing/04-data-cleaning.md"
]
key_takeaways:
  - "Data quality must be guarded by executable data contracts at ingestion boundaries, failing fast before corrupted records enter feature pipelines."
  - "The 6 core dimensions of data quality: Completeness, Uniqueness, Validity, Consistency, Timeliness, and Accuracy."
  - "Git cannot version multi-gigabyte datasets without repository bloat; DVC versions metadata pointers in Git while storing binary artifacts in remote object stores."
  - "Pandera provides runtime, type-safe schema validation for pandas DataFrames with native statistical hypothesis assertions."
---

# Data Quality, Testing, and Versioning (DVC)

## 1. Context & Problem Statement

In production machine learning systems, code is rarely the primary source of failure; data is. Upstream database migrations, mobile app SDK updates, third-party API changes, and silent network outages introduce subtle corruptions into data pipelines:
1. **Silent Schema Drift**: An upstream engineer changes a currency column from USD cents (`10500`) to dollars (`105.00`), causing feature values to drop by two orders of magnitude without raising an exception.
2. **Untracked Model Provenance**: An auditor asks which exact training records produced a deployed medical diagnosis model from 18 months ago. Because raw CSVs were overwritten in an S3 bucket, the experiment cannot be reproduced.
3. **Downstream Pipeline Contamination**: Corrupted null records or negative age values pass into feature stores, corrupting production embeddings and degrading live model accuracy.

Ensuring ML reliability requires **executable data contracts** at pipeline boundaries coupled with **content-addressed, immutable data versioning**.

---

## 2. Conceptual Architecture

The data quality and versioning architecture deploys validation gates across the ingestion boundary and establishes strict Git-DVC linkage for immutable lineage.

```
+-----------------------------------------------------------------------------------+
|                        Raw Data Ingestion / Production Logs                       |
+-----------------------------------------------------------------------------------+
                                          |
                                          v
+-----------------------------------------------------------------------------------+
|                     Data Quality Gate (Pandera / Data Contracts)                  |
|  - Schema Validation: Column Types, Nullability, Uniqueness                       |
|  - Range Bounds: Age in [0, 120], Amount >= 0.0                                   |
|  - Statistical Invariants: Missingness < 2%, Category set in {'US', 'EU', 'APAC'} |
+-----------------------------------------------------------------------------------+
           |                                                      |
      [Passes]                                                [Fails]
           v                                                      v
+------------------------------------+              +-------------------------------+
|     DVC Immutable Data Tracking    |              | Dead Letter Queue / Alerting  |
|  - Calculate hash: dvc add data/   |              | - Page On-Call Data Engineer  |
|  - Write data.dvc pointer to Git   |              | - Quarantine corrupted batch  |
|  - Push binary chunks to S3 / GCS  |              +-------------------------------+
+------------------------------------+
           |
           v
+------------------------------------+
|  Downstream ML Training Pipeline   |
+------------------------------------+
```

---

## 3. Core Theory & Mathematical Formulation

### 3.1 The 6 Dimensions of Data Quality

| Dimension | Formal Mathematical Definition | Production Metric / Test |
| :--- | :--- | :--- |
| **Completeness** | $\frac{1}{N} \sum_{i=1}^N \mathbb{I}(X_{ij} \neq \text{NULL})$ | Null percentage $\le \tau_{\text{null}}$ |
| **Uniqueness** | $\frac{|\text{Unique}(K)|}{N} = 1.0$ | Primary key collision count $= 0$ |
| **Validity** | $\forall i: X_{ij} \in \Omega_j$ (Domain Bounds) | Value satisfies regex or $[a_j, b_j]$ bounds |
| **Consistency** | $X_{A} + X_{B} = X_{\text{Total}}$ | Cross-column business logic invariant |
| **Timeliness** | $t_{\text{current}} - t_{\text{event}} \le \Delta T_{\max}$ | Ingestion latency / freshness SLA |
| **Accuracy** | $\|X_{\text{recorded}} - X_{\text{ground\_truth}}\| \le \epsilon$ | Comparison against verified audit log |

### 3.2 Content-Addressed Storage & Merkle Hash Verification

DVC (Data Version Control) solves large-scale data versioning by decoupling metadata pointers from binary storage using content-addressed hashing (MD5):

Given a directory of data files $D = \{f_1, f_2, \dots, f_k\}$:
1. Compute MD5 checksum for each file: $h_i = \text{MD5}(f_i)$.
2. Construct directory hash $H_D = \text{MD5}(\sum h_i)$.
3. Write small metadata pointer `data.dvc` (typically $< 1\text{ KB}$):
   ```yaml
   outs:
     - md5: a8f5c88b90123ef...
       size: 532891048
       path: data/raw/
   ```
4. Commit `data.dvc` directly into Git. Push the $500\text{ MB}$ raw data file to remote object storage (`s3://my-bucket/dvc-cache/`).
5. Any modification to a single byte in `data/` alters the MD5 hash, forcing Git to track the version transition deterministically.

---

## 4. Practical Implementation

Below is a complete, executable Python module demonstrating programmatic schema enforcement and statistical data contracts using Pandera-style assertions, followed by a simulated DVC content-addressed hashing engine.

```python
"""
Data Quality Validation Contracts & Content-Addressed Versioning Engine.
Tested on Python 3.11, pandas 3.x, NumPy 2.x.
"""

from typing import Dict, List, Optional
import hashlib
import json
import numpy as np
import pandas as pd


class DataContractViolation(Exception):
    """Raised when incoming dataset violates data contract assertions."""
    pass


class ProductionDataContract:
    """
    Defines and executes strict data contracts on tabular DataFrames:
    - Expected column dtypes
    - Non-null constraints
    - Allowed categorical values
    - Range bound checks
    """

    def __init__(self):
        self.required_columns: Dict[str, str] = {
            "customer_id": "int64",
            "age": "float64",
            "credit_score": "float64",
            "country": "object"
        }
        self.allowed_countries = {"US", "CA", "UK", "DE"}
        self.max_null_pct = 0.05

    def validate(self, df: pd.DataFrame) -> bool:
        # 1. Column Existence Check
        missing_cols = set(self.required_columns.keys()) - set(df.columns)
        if missing_cols:
            raise DataContractViolation(f"Missing required columns: {missing_cols}")

        # 2. Dtype Verification
        for col, expected_dtype in self.required_columns.items():
            actual_dtype = str(df[col].dtype)
            if actual_dtype != expected_dtype and not (expected_dtype == "float64" and actual_dtype == "int64"):
                raise DataContractViolation(f"Column '{col}' expected {expected_dtype}, got {actual_dtype}")

        # 3. Completeness Invariant
        for col in self.required_columns.keys():
            null_pct = df[col].isna().mean()
            if null_pct > self.max_null_pct:
                raise DataContractViolation(f"Column '{col}' null rate {null_pct:.2%} exceeds max allowed {self.max_null_pct:.2%}")

        # 4. Validity & Domain Range Bounds
        valid_age = df["age"].dropna().between(18, 120).all()
        if not valid_age:
            raise DataContractViolation("Age violates domain boundaries [18, 120].")

        valid_credit = df["credit_score"].dropna().between(300, 850).all()
        if not valid_credit:
            raise DataContractViolation("Credit score violates boundaries [300, 850].")

        # 5. Categorical Domain Enforcement
        unrecognized_countries = set(df["country"].dropna().unique()) - self.allowed_countries
        if unrecognized_countries:
            raise DataContractViolation(f"Unrecognized countries detected: {unrecognized_countries}")

        return True


def compute_dvc_style_hash(df: pd.DataFrame) -> str:
    """Computes deterministic MD5 hash of tabular content for versioning."""
    # Serialize deterministic JSON byte string
    content_bytes = df.to_json(orient="records", date_format="iso").encode("utf-8")
    return hashlib.md5(content_bytes).hexdigest()


# Verification Script
if __name__ == "__main__":
    contract = ProductionDataContract()

    # 1. Test Valid Ingestion Batch
    valid_batch = pd.DataFrame({
        "customer_id": [1001, 1002, 1003],
        "age": [28.0, 45.0, 34.0],
        "credit_score": [720.0, 680.0, 810.0],
        "country": ["US", "CA", "DE"]
    })

    assert contract.validate(valid_batch) is True
    hash_v1 = compute_dvc_style_hash(valid_batch)
    print(f"Batch 1 Passed Data Contract. Content Hash: {hash_v1}")

    # 2. Test Invalid Batch (Catches Corrupted Range & Category)
    corrupted_batch = pd.DataFrame({
        "customer_id": [1004, 1005],
        "age": [25.0, 150.0], # 150 exceeds boundary!
        "credit_score": [700.0, 710.0],
        "country": ["US", "MARS"] # Invalid category!
    })

    try:
        contract.validate(corrupted_batch)
        raise AssertionError("Contract should have failed on corrupted batch!")
    except DataContractViolation as e:
        print(f"Contract Successfully Blocked Ingestion: {e}")

    # 3. Test Immutability / Version Detection
    tampered_batch = valid_batch.copy()
    tampered_batch.loc[0, "credit_score"] = 721.0 # 1 point change
    hash_v2 = compute_dvc_style_hash(tampered_batch)
    print(f"Tampered Batch Hash: {hash_v2}")
    assert hash_v1 != hash_v2, "DVC hash must detect single-cell mutations."

    print("\n[SUCCESS] Data Quality contracts and content versioning verified.")
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

| Failure Mode | Root Cause | Observable Symptom | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **Non-Deterministic Hashing** | Storing JSON / CSV with arbitrary row order | Identical data generates different DVC hashes | Sort by primary key before computing content hash. |
| **Silent Type Promotion** | Integer column containing a single `NaN` coerced to `float64` | Downstream SQL join failure or hash map miss | Use nullable integer types (`pd.Int64Dtype()`). |
| **Pipeline Failure Swallowing** | Data validation script logs error as warning and proceeds | Model trains on corrupted batch; production metrics degrade | Configure validation gate to exit with non-zero status code (`sys.exit(1)`). |
| **Timezone Ambiguity** | Timestamps recorded without UTC offset | Group aggregations shift by 5–8 hours depending on server locale | Enforce strict ISO-8601 strings with explicit `+00:00` UTC offsets. |
| **DVC Cache Bloat** | Pushing high-volume temporary scratch datasets to remote DVC cache | Cloud storage costs explode | Use `.dvcignore` to exclude temporary outputs and intermediate checkpoints. |

---

## 6. Production Engineering & Performance

1. **Integration with CI/CD Pipelines**:
   Execute data contract validation inside GitHub Actions or GitLab CI before retraining jobs kick off. If an ingestion batch fails validation, the pipeline halts immediately, paging on-call engineers.
2. **Standard DVC Workflow CLI Commands**:
   - Initialize DVC in git repo: `dvc init`
   - Configure S3 remote: `dvc remote add -d myremote s3://my-bucket/dvc-storage`
   - Track large data directory: `dvc add data/raw/` (generates `data/raw.dvc`)
   - Commit pointer: `git add data/raw.dvc .gitignore && git commit -m "track v1.2 dataset"`
   - Push binary data to remote: `dvc push`
   - Checkout exact data on another machine: `git checkout <commit> && dvc pull`
3. **Partitioned Ingestion Checks**:
   For multi-terabyte datasets in Spark or Snowflake, execute contract checks across partitioned micro-batches ($100,000$ rows) rather than full table scans to keep validation latency under 3 seconds.

---

## 7. Comparative Analysis: Data Validation Frameworks

| Feature | Manual Assertions | Pandera | Great Expectations | Pydantic |
| :--- | :--- | :--- | :--- | :--- |
| **Target Paradigm** | Ad-hoc scripts | Pandas / Polars native | Enterprise batch suites | Single-row API payloads |
| **Performance Overhead**| Low | Very Low (Vectorized) | Moderate (Suite overhead) | Ultra-Fast (C-Rust backend) |
| **Statistical Hypothesis**| Manual code | Native (`hypothesis=...`) | Extensive built-in expectations | No (Type/Range only) |
| **HTML Profiling Reports**| No | Minimal | Rich interactive docs | No |
| **Best Used For** | Quick unit tests | ML training pipelines | Data warehouse / ETL pipelines | FastAPI inference endpoints |

---

## 8. Verification & Diagnostics Test Harness

```python
import pytest
import pandas as pd

def test_data_contract_completeness_pass():
    contract = ProductionDataContract()
    df = pd.DataFrame({
        "customer_id": [1, 2],
        "age": [30.0, 40.0],
        "credit_score": [700.0, 750.0],
        "country": ["US", "CA"]
    })
    assert contract.validate(df) is True

def test_data_contract_missing_column():
    contract = ProductionDataContract()
    df = pd.DataFrame({"customer_id": [1], "age": [30.0]})
    with pytest.raises(DataContractViolation):
        contract.validate(df)
```

---

## 9. Cross-Track Connections

- **04-devops/git-github/01-git-fundamentals**: Git commit hashes, object storage, and `.gitignore` conventions.
- **05-ai/machine-learning/15-mlops-basics/01-model-tracking-and-mlflow.md**: Linking DVC data hashes to MLflow run metadata.
- **09-sql/08-data-integrity-and-constraints**: SQL table constraints (`CHECK`, `FOREIGN KEY`, `NOT NULL`).

---

## 10. Real-World Case Study: FinTech Automated Underwriting Failure

A peer-to-peer lending startup evaluated loan applications automatically.
- **Incident**: An upstream core-banking vendor updated an API endpoint, changing the formatting of `annual_income` from integer cents (`$5,000,000` cents = `$50,000`) to raw dollars (`$50,000`).
- **Consequence**: The underwriting model observed incomes that were suddenly 100 times smaller. It rejected 94% of qualified applicants over a 48-hour weekend, costing \$3.2M in lost origination revenue.
- **Remediation**:
  1. Deployed an automated Pandera contract gate verifying that the median `annual_income` of incoming batches remained within $[45,000, 65,000]$.
  2. Integrated DVC tracking to log the exact dataset hashes used in daily model audits.
  3. Deployed an automated circuit breaker that shuts down automated decisions and falls back to manual review if contract validation fails.

---

## 11. Technical Interview Questions & Solutions

### Question 1: How does DVC track 100 GB datasets in Git without storing the large files in the Git repository?
**Answer**:
DVC works on the principle of **content-addressed storage and metadata decoupling**:
1. When you run `dvc add data.csv`, DVC computes the MD5 checksum of `data.csv` (e.g., `ec5f79...`).
2. DVC moves the actual binary file into its local cache directory: `.dvc/cache/ec/5f79...`.
3. In the workspace, DVC creates a lightweight pointer file: `data.csv.dvc` containing the hash, file size, and relative path.
4. DVC automatically adds `data.csv` to `.gitignore` so Git never tracks the 100 GB binary file.
5. The developer commits only `data.csv.dvc` (a 100-byte text file) to Git.
6. When another team member runs `git pull` followed by `dvc pull`, DVC reads the hash from the pointer file and fetches the exact 100 GB binary chunk from remote cloud object storage (S3/GCS).

### Question 2: What is the difference between Data Drift and Concept Drift?
**Answer**:
Let $X$ denote the features and $Y$ denote the target label. Under Bayes' theorem:
$$P(X, Y) = P(Y \mid X) \cdot P(X)$$
- **Data Drift (Covariate Shift)**:
  The marginal distribution of features changes, but the conditional probability of the target given the features remains constant:
  $$P(X) \text{ changes, while } P(Y \mid X) \text{ is invariant.}$$
  *Example*: A loan app's user demographic shifts from ages 20–30 to ages 50–60, but an individual with a 750 credit score still has the same probability of default.
- **Concept Drift**:
  The conditional relationship between features and target changes, regardless of feature distribution:
  $$P(Y \mid X) \text{ changes, while } P(X) \text{ may or may not change.}$$
  *Example*: Macroeconomic inflation occurs. A debt-to-income ratio that was considered safe three years ago now leads to rapid loan default. The mathematical meaning of the features has evolved.

### Question 3: Write a Python function implementing a Population Stability Index (PSI) calculation to detect feature distribution drift.
```python
def compute_psi(expected: np.ndarray, actual: np.ndarray, num_bins: int = 10) -> float:
    """
    Computes Population Stability Index (PSI) between baseline and production feature.
    PSI < 0.1: No significant shift.
    0.1 <= PSI < 0.2: Moderate shift.
    PSI >= 0.2: Significant drift (retraining required).
    """
    # Define bin edges on expected baseline distribution
    percentiles = np.linspace(0, 100, num_bins + 1)
    bin_edges = np.percentile(expected, percentiles)
    bin_edges[0] -= 1e-5
    bin_edges[-1] += 1e-5

    # Compute empirical proportions per bin
    expected_counts, _ = np.histogram(expected, bins=bin_edges)
    actual_counts, _ = np.histogram(actual, bins=bin_edges)

    expected_pct = expected_counts / len(expected)
    actual_pct = actual_counts / len(actual)

    # Prevent division by zero / log(0) with small smoothing epsilon
    eps = 1e-4
    expected_pct = np.clip(expected_pct, eps, 1.0)
    actual_pct = np.clip(actual_pct, eps, 1.0)

    # PSI formula: sum((actual% - expected%) * ln(actual% / expected%))
    psi_value = np.sum((actual_pct - expected_pct) * np.log(actual_pct / expected_pct))
    return float(psi_value)
```

---

## 12. Guided Practice & Hands-On Exercises

### Exercise 1: Pandera Schema Builder
- Build a complete Pandera `DataFrameSchema` for an e-commerce transaction dataset enforcing: `price > 0`, `quantity in [1, 50]`, `email` matching a regex pattern, and `timestamp` strictly in the past.

### Exercise 2: Simulated DVC Pipeline DAG
- Write a Python script that simulates a multi-stage DVC pipeline: Stage 1 (Clean Data), Stage 2 (Engineer Features), Stage 3 (Train Model). Use content hashing to verify that modifying Stage 1's output automatically invalidates and triggers Stages 2 and 3.

### Exercise 3: Automated PSI Drift Detector
- Generate a baseline dataset from $\mathcal{N}(0, 1)$. Generate production batches with mean shifts $\mu \in [0.1, 1.0]$. Plot the computed PSI as a function of mean shift and identify the exact drift magnitude where $\text{PSI} \ge 0.2$.
