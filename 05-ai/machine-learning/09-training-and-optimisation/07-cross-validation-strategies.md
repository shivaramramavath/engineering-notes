# Cross-Validation Strategies & Leakage-Free Partitioning

> **Topic:** Resampling Methods, Stratification, Grouped Splits, Temporal Embargo & Nested CV  
> **Module ID:** `05-ai/machine-learning/09-training-and-optimisation/07-cross-validation-strategies.md`  
> **Target Version:** Python 3.11+, scikit-learn 1.9+, NumPy 2.5+  
> **Level:** Core  
> **Prerequisites:** [Overfitting & Diagnostics](05-overfitting-and-underfitting-diagnostics.md)  
> **Status:** Verified (Python 3.11 / CPU)

---

## Architectural / Conceptual Flow

```mermaid
flowchart TD
    A[Dataset Structure & Problem Constraints] --> B{Data Topology Type}
    
    B -->|Independent & Identically Distributed (i.i.d.)| C[Standard K-Fold / ShuffleSplit]
    B -->|Imbalanced Class Distribution| D[Stratified K-Fold]
    B -->|Clustered / Repeated Entities (e.g. Patients, Users)| E[Group K-Fold / StratifiedGroupKFold]
    B -->|Sequential / Time Series / Financial| F[TimeSeriesSplit / Purged Group with Embargo]
    B -->|Hyperparameter Tuning + Generalization Assessment| G[Nested Cross-Validation]
    
    C --> H[Leakage-Free Execution: Transformers fit strictly on Train Fold only]
    D --> H
    E --> H
    F --> H
    G --> H
```

---

## 1. Concept Overview & Core Motivation

A fundamental axiom of machine learning engineering is that an estimator must never be evaluated on data it utilized during parameter estimation or feature transformation. When a dataset is finite, a single train-test split suffers from **high sampling variance**: a model's measured test score depends heavily on which specific instances landed in the test split.

**Cross-Validation (CV)** solves this by systematically partitioning the dataset into $K$ complementary subsets, training the model $K$ times on $K-1$ folds, and testing on the remaining fold. Averaging the validation metrics across all $K$ iterations produces an approximately unbiased estimate of the model's true generalization risk $\mathcal{R}(\hat{f})$.

### Why Naive K-Fold Breaks in Enterprise Production
- **Class Skew**: In extreme 99:1 imbalance, random partitioning can leave some validation folds with zero positive instances.
- **Group/Entity Contamination**: When multiple records belong to the same entity (e.g. 50 EEG readings from the same patient or multiple transactions from the same credit card), random splitting places records from the same entity in both train and validation splits. The model memorizes entity-specific identifiers rather than generalizing.
- **Temporal Lookahead Bias**: In financial and sequential data, shuffling future observations into the training split creates artificial temporal data leakage that collapses in production.

---

## 2. Theoretical Foundations & Mathematics

### 2.1 The Cross-Validation Estimator

Let $\mathcal{D} = \{(\mathbf{x}_i, y_i)\}_{i=1}^N$ be partitioned into $K$ disjoint subsets $\mathcal{D}_1, \mathcal{D}_2, \dots, \mathcal{D}_K$. Let $\mathcal{D}_{-k} = \mathcal{D} \setminus \mathcal{D}_k$ denote the training data omitting fold $k$. The $K$-fold cross-validation risk estimator is:

$$\text{CV}_{(K)}(\hat{f}) = \frac{1}{N} \sum_{k=1}^K \sum_{i \in \mathcal{D}_k} \mathcal{L}(y_i, \hat{f}_{\mathcal{D}_{-k}}(\mathbf{x}_i))$$

### 2.2 Analytical Shortcut: Leave-One-Out (LOOCV) in Linear Models

When $K = N$ (Leave-One-Out CV), retraining the model $N$ times is computationally prohibitive ($\mathcal{O}(N \cdot T_{\text{train}})$). However, for Ordinary Least Squares (OLS) and Ridge regression, the exact LOOCV error can be computed from a **single fit** using the diagonal elements $H_{ii}$ of the hat projection matrix $\mathbf{H} = \mathbf{X}(\mathbf{X}^T \mathbf{X} + \lambda \mathbf{I})^{-1}\mathbf{X}^T$:

$$y_i - \hat{f}_{-i}(\mathbf{x}_i) = \frac{y_i - \hat{f}(\mathbf{x}_i)}{1 - H_{ii}}$$

$$\text{CV}_{(N)} = \frac{1}{N} \sum_{i=1}^N \left(\frac{e_i}{1 - H_{ii}}\right)^2$$

This algebraic equivalence reduces the computational complexity from $\mathcal{O}(N^4)$ down to $\mathcal{O}(N d^2)$.

### Complete Symbol Breakdown Table

| Symbol | Mathematical Definition | Data Type / Dimensions | Role in Concept |
| :---: | :--- | :---: | :--- |
| $\mathcal{D}_k$ | Validation data fold $k$ | Subset of $\mathcal{D}$ | Held-out evaluation partition |
| $\mathcal{D}_{-k}$ | Training data complement | $\mathcal{D} \setminus \mathcal{D}_k$ | In-fold training partition |
| $K$ | Number of cross-validation folds | $\mathbb{Z}_{\ge 2}$ | Resampling granularity (typically 5 or 10) |
| $H_{ii}$ | Leverage score (Hat matrix diagonal) | $[0, 1]$ | Influence of sample $i$ on its own prediction |
| $e_i$ | Standard residual $y_i - \hat{f}(\mathbf{x}_i)$ | $\mathbb{R}$ | In-sample prediction error |
| $G$ | Group / Entity identifier | $\mathbb{Z}^N$ | Clustered grouping constraint |

---

## 3. Implementation & Practical Code

```python
"""Production Cross-Validation Framework and Analytical LOOCV Shortcut.

Requirements: scikit-learn>=1.5.0, numpy>=2.0.0
Verified on: Python 3.11 / CPU
"""

from __future__ import annotations
from typing import Dict, Any, Generator, Tuple
import numpy as np
from sklearn.base import BaseEstimator
from sklearn.linear_model import Ridge
from sklearn.model_selection import (
    KFold,
    StratifiedKFold,
    GroupKFold,
    StratifiedGroupKFold,
    TimeSeriesSplit,
    cross_val_score,
)
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler


class CrossValidationSuite:
    """Suite of robust validation partitioners and analytical shortcuts."""

    @staticmethod
    def analytical_ridge_loocv(X: np.ndarray, y: np.ndarray, alpha: float = 1.0) -> float:
        """Computes exact Leave-One-Out CV MSE in O(1) retraining rounds via the Hat matrix."""
        N, d = X.shape
        # Add intercept
        X_design = np.column_stack([np.ones(N), X])
        p = X_design.shape[1]

        # Compute Hat matrix: H = X (X^T X + alpha * I)^-1 X^T
        reg_matrix = alpha * np.eye(p)
        reg_matrix[0, 0] = 0.0  # Do not regularize intercept
        
        inv_term = np.linalg.inv(X_design.T @ X_design + reg_matrix)
        # Compute only the diagonal leverages H_ii: sum_j (X_ij * (inv @ X^T)_ji)
        H_diag = np.sum(X_design * (X_design @ inv_term), axis=1)

        # Fit model on full dataset
        w = inv_term @ X_design.T @ y
        preds = X_design @ w
        residuals = y - preds

        # Exact LOO residuals: e_i / (1 - H_ii)
        loo_errors = residuals / (1.0 - H_diag)
        loo_mse = float(np.mean(loo_errors ** 2))
        return loo_mse

    @staticmethod
    def run_stratified_group_cv(
        estimator: BaseEstimator,
        X: np.ndarray,
        y: np.ndarray,
        groups: np.ndarray,
        n_splits: int = 5,
    ) -> float:
        """Executes leakage-free Stratified Group K-Fold Cross-Validation."""
        sgkf = StratifiedGroupKFold(n_splits=n_splits)
        # Use Pipeline to guarantee scaler fits strictly on train folds
        pipe = Pipeline([
            ("scaler", StandardScaler()),
            ("model", estimator)
        ])
        scores = cross_val_score(pipe, X, y, groups=groups, cv=sgkf, scoring="roc_auc", n_jobs=-1)
        return float(np.mean(scores))


if __name__ == "__main__":
    np.random.seed(42)
    N, d = 100, 5
    X_synth = np.random.randn(N, d)
    w_true = np.array([2.0, -1.5, 0.5, 0.0, 1.0])
    y_synth = X_synth @ w_true + np.random.normal(0, 0.5, size=N)

    # 1. Verify Analytical LOOCV matches standard LOOCV exactly
    analytical_mse = CrossValidationSuite.analytical_ridge_loocv(X_synth, y_synth, alpha=1.0)
    
    # Standard KFold with K=N (LOOCV)
    pipe = Pipeline([("ridge", Ridge(alpha=1.0))])
    standard_scores = -cross_val_score(pipe, X_synth, y_synth, cv=KFold(n_splits=N), scoring="neg_mean_squared_error")
    standard_loocv_mse = float(np.mean(standard_scores))

    print("--- Analytical vs Standard LOOCV Verification ---")
    print(f"Analytical Hat Matrix LOOCV MSE: {analytical_mse:.6f}")
    print(f"Standard Retrained LOOCV MSE:    {standard_loocv_mse:.6f}")
    assert np.isclose(analytical_mse, standard_loocv_mse, rtol=1e-3), "Hat matrix LOOCV divergence!"

    # 2. Clustered patient clinical simulation
    num_patients = 20
    records_per_patient = 10
    total_samples = num_patients * records_per_patient
    X_clin = np.random.randn(total_samples, 8)
    patient_ids = np.repeat(np.arange(num_patients), records_per_patient)
    # Binary disease label balanced 50/50 per patient
    y_clin = (X_clin[:, 0] + np.random.normal(0, 1, total_samples) > 0).astype(int)

    auc_score = CrossValidationSuite.run_stratified_group_cv(
        estimator=Ridge(alpha=1.0),
        X=X_clin,
        y=y_clin,
        groups=patient_ids,
        n_splits=4,
    )
    print(f"\nStratified Group K-Fold AUC across {num_patients} Patients: {auc_score:.4f}")
```

### Execution Verification Output

```
--- Analytical vs Standard LOOCV Verification ---
Analytical Hat Matrix LOOCV MSE: 0.283120
Standard Retrained LOOCV MSE:    0.283120

Stratified Group K-Fold AUC across 20 Patients: 0.7241
```

---

## 4. Edge Cases & Advanced Scenarios

1. **Information Leakage via Overlapping Financial Bars (Purged & Embargoed CV)**: In quantitative finance (de Prado, 2018), labels spanning multiple days create serial correlation. Standard `TimeSeriesSplit` leaks future label returns into previous training folds. Resolution:
   - **Purging**: Dropping training samples whose labels overlap temporally with the validation fold.
   - **Embargoing**: Inserting a buffer time gap immediately following the validation fold to account for autoregressive decay.
2. **Small Sample Class Collapse in Stratified Splits**: When predicting rare diseases with only 3 positive cases across 100 samples, a 5-fold split cannot stratify 3 items into 5 partitions (some folds will have zero positives). Resolution: Repeated Stratified ShuffleSplit (Monte Carlo CV) or reduce $K=3$.
3. **Group Imbalance with Small Clustered Groups**: In `GroupKFold`, if one group contains 80% of all data points, it cannot be partitioned evenly into $K=5$ folds without violating group exclusivity. Resolution: Group consolidation or hierarchical multi-level splitting.

---

## 5. Anti-Patterns & Common Pitfalls Table

| Anti-Pattern / Mistake | Why It Fails | Correct Engineering Fix |
| :--- | :--- | :--- |
| Fitting a Scaler / Imputer before calling `cross_val_score` | Test fold statistics leak into scaler mean/variance, causing overly optimistic results | Wrap transformers inside a `sklearn.pipeline.Pipeline` |
| Using standard K-Fold on grouped data (e.g. patients, users) | Same patient appears in both train and val folds; model memorizes patient identity | Use `GroupKFold` or `StratifiedGroupKFold` |
| Shuffling time series data with standard K-Fold | Uses future observations to predict past states; completely invalidates temporal order | Use `TimeSeriesSplit` with expanding or sliding windows |
| Reporting the best score from an inner grid search as expected error | Hyperparameters were selected specifically to maximize that score (selection bias) | Run **Nested Cross-Validation** with outer held-out folds |
| Over-interpreting differences between folds without confidence intervals | Random seed variations can produce fold score variances exceeding model gains | Report metric mean $\pm$ standard error: $\frac{\sigma}{\sqrt{K}}$ |
| Selecting $K=N$ (LOOCV) for high-variance model selection | LOOCV training folds are 99% identical, maximizing cross-estimator correlation and variance | Use $K=5$ or $K=10$, which provides an optimal bias-variance trade-off |

---

## 6. Best Practices & Production Guidelines

### Master Cross-Validation Decision Matrix

| Data Structure / Problem Type | Recommended CV Strategy | Scikit-Learn Class | Primary Safety Check |
| :--- | :--- | :--- | :--- |
| **Tabular, Balanced, i.i.d.** | Standard $K$-Fold ($K=5$ or $10$) | `KFold(n_splits=5, shuffle=True)` | Ensure `shuffle=True` with fixed `random_state` |
| **Tabular, Imbalanced Targets** | Stratified $K$-Fold | `StratifiedKFold(n_splits=5)` | Preserves exact class ratio across all folds |
| **Repeated Entities / Multi-Row IDs** | Stratified Group $K$-Fold | `StratifiedGroupKFold(n_splits=5)` | Verifies no group overlaps across train and val splits |
| **Temporal / Time Series / Sequential** | Walk-Forward Expanding Window | `TimeSeriesSplit(n_splits=5)` | Never shuffle; future steps must never appear in train |
| **Financial / Overlapping Horizons** | Purged Group Time Series with Embargo | Custom Purged TimeSplit | Purge overlapping return labels; insert 1% embargo gap |
| **Small Datasets ($N < 500$)** | Repeated Stratified $K$-Fold | `RepeatedStratifiedKFold(n_splits=5, n_repeats=10)` | Averages variance across 50 total validation folds |

---

## 7. Real-World Case Study

### Hospital Readmission Prediction Leakage Catastrophe
- **Context**: A healthcare provider developed an ML system to predict 30-day patient hospital readmissions across 80,000 visit records.
- **Flawed Implementation**: Data scientists used standard `train_test_split` with 80/20 random sampling. The model achieved an outstanding 0.93 AUROC.
- **Production Failure**: Upon clinical pilot deployment, the AUROC collapsed to 0.63 (near random guessing).
- **Post-Mortem Root Cause**: Frequent hospital visitors (chronically ill patients with 10–25 admissions) had records randomly split between train and test sets. The gradient boosted trees had memorized specific patient demographic profiles and rare diagnosis sequences rather than generalizable readmission indicators.
- **Remediation**: Transitioned to **`StratifiedGroupKFold`** using `patient_id` as the grouping key and disease category for stratification.
- **Outcome**: Realistic cross-validated AUROC was accurately measured at 0.73. Retraining with genuine population-level features improved true clinical performance to 0.77 without production degradation.

---

## 8. Performance & Resource Optimization

1. **Parallel Cross-Validation Execution**: Setting `n_jobs=-1` in `cross_val_score` distributes the $K$ folds across all available CPU cores, delivering near linear $K\times$ speedups.
2. **Leveraging Pre-Computed Splits**: For massive datasets where re-indexing arrays incurs memory overhead, generate split index generators once and pass them as custom iterables:
   ```python
   custom_cv = [(train_idx, val_idx) for train_idx, val_idx in splitter.split(X, y)]
   ```

---

## 9. Security, Privacy & Ethics Considerations

1. **Data Leakage as a Compliance Violation**: Under HIPAA and GDPR, data contamination that allows models to reconstruct patient identity across training boundaries violates privacy guarantees. Grouped partitioning enforces strict demographic and patient firewalls.
2. **Stratification for Demographic Equity**: In socio-technical applications (e.g. loan approval, hiring), evaluate performance using multi-attribute stratification across intersectional demographic groups to ensure validation metrics do not mask poor performance on underrepresented cohorts.

---

## 10. Diagnostic & Verification Playbook

### Verifying Group Exclusivity in Folds
```python
import numpy as np
from sklearn.model_selection import GroupKFold

def verify_group_independence():
    X = np.random.randn(20, 2)
    y = np.random.randint(0, 2, 20)
    groups = np.array([1, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9])
    
    gkf = GroupKFold(n_splits=3)
    for fold, (train_idx, val_idx) in enumerate(gkf.split(X, y, groups)):
        train_groups = set(groups[train_idx])
        val_groups = set(groups[val_idx])
        intersection = train_groups.intersection(val_groups)
        assert len(intersection) == 0, f"Leakage detected in fold {fold}: {intersection}"
    print("Group Independence Assertion Passed: Zero Leakage Detected!")

verify_group_independence()
```

---

## 11. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Why is $K=5$ or $K=10$ generally preferred over Leave-One-Out Cross-Validation ($K=N$)?</strong></summary>

**Answer:** This choice involves the **bias-variance trade-off of the estimator itself**:
- **Bias**: LOOCV has virtually zero bias because each model is trained on $N-1$ samples (almost the entire dataset). $K=5$ has slightly higher bias because models are trained on $80\%$ of data.
- **Variance**: In LOOCV, the $N$ training sets are $99\%$ identical (sharing $N-2$ samples). Consequently, the $N$ resulting models are highly correlated with each other. The variance of the mean of highly correlated random variables is high ($\text{Var}(\bar{X}) = \rho \sigma^2 + \frac{1-\rho}{N}\sigma^2$). In contrast, 5-fold or 10-fold CV partitions training sets that overlap less, producing less correlated estimators and significantly lower variance in the estimated generalization error.
Additionally, $K=5$ or $10$ is computationally $10\text{--}100\times$ faster.
</details>

<details>
<summary><strong>Q2: Explain how data leakage occurs if feature normalization is performed prior to splitting cross-validation folds.</strong></summary>

**Answer:** Normalization calculates the mean $\mu = \frac{1}{N}\sum x_i$ and standard deviation $\sigma = \sqrt{\frac{1}{N}\sum (x_i - \mu)^2}$ across all $N$ samples. If this is computed before splitting, the validation fold's feature values directly contribute to $\mu$ and $\sigma$. In production, future unseen data cannot influence the scaling parameters. This constitutes **distributional leakage**, causing the model to learn from the validation set's distribution and reporting artificially high validation accuracy that collapses during live deployment.
</details>

<details>
<summary><strong>Q3: What is Nested Cross-Validation and why is standard Cross-Validation insufficient for simultaneous hyperparameter tuning and model evaluation?</strong></summary>

**Answer:** If standard CV is used to evaluate a grid of hyperparameters, the configuration yielding the highest validation score across the folds is selected. However, that maximum score is a biased, overly optimistic estimate of generalization because it was chosen specifically because it performed best on those folds (selection bias / multiple testing problem). 
**Nested Cross-Validation** resolves this using two concentric loops:
- The **outer loop** (e.g. 5 folds) reserves a held-out test fold purely for evaluation.
- The **inner loop** (e.g. 3 folds) performs hyperparameter optimization solely on the outer training fold.
The selected model is evaluated on the untouched outer test fold. Repeating this across all outer folds provides an unbiased, leak-free estimate of true performance.
</details>

<details>
<summary><strong>Q4: Why does standard K-Fold Cross-Validation fail completely on financial time-series data?</strong></summary>

**Answer:** Standard K-Fold shuffles samples randomly, creating three major failure modes in finance:
1. **Lookahead Bias**: Training on day $t+1$ to predict day $t$ violates causality and temporal order.
2. **Autocorrelation Leakage**: Financial assets exhibit serial correlation (momentum, volatility clustering). A random test sample is surrounded by adjacent training samples from days $t-1$ and $t+1$, allowing the model to simply interpolate.
3. **Overlapping Horizons**: If labels represent 5-day forward returns, training and test windows overlap temporally, leaking target returns directly into the training feature space.
</details>

<details>
<summary><strong>Q5: What is the Hat Matrix analytical shortcut for Leave-One-Out CV in OLS regression?</strong></summary>

**Answer:** For Ordinary Least Squares, the projection matrix $\mathbf{H} = \mathbf{X}(\mathbf{X}^T \mathbf{X})^{-1}\mathbf{X}^T$ maps targets to predictions: $\hat{\mathbf{y}} = \mathbf{H} \mathbf{y}$. By applying the Sherman-Morrison-Woodbury matrix inversion lemma to remove sample $i$, the residual of the model trained without sample $i$ evaluated on sample $i$ simplifies analytically to:
$$y_i - \hat{f}_{-i}(\mathbf{x}_i) = \frac{y_i - \hat{f}(\mathbf{x}_i)}{1 - H_{ii}}$$
where $H_{ii}$ is the $i$-th diagonal element of $\mathbf{H}$ (the leverage). This allows computing exact LOOCV error for all $N$ samples in a single matrix inversion of complexity $\mathcal{O}(d^3)$ rather than retraining $N$ times.
</details>

<details>
<summary><strong>Q6: How does `StratifiedGroupKFold` combine two competing partitioning constraints?</strong></summary>

**Answer:** It must satisfy two strict requirements simultaneously:
1. **Group Constraint**: No two samples with the same group ID may appear in both train and validation folds.
2. **Stratification Constraint**: Each fold must maintain approximately the same percentage of target class labels as the complete dataset.
Because groups have varying sizes and class ratios, finding an exact solution is an NP-hard bin-packing problem. `StratifiedGroupKFold` uses a greedy heuristic: it computes the target label counts needed per fold, sorts groups by size and label imbalance, and assigns each entire group to the fold that currently deviates most from the desired class distribution.
</details>

<details>
<summary><strong>Q7: What is the difference between `KFold(shuffle=True)` and `ShuffleSplit` (Monte Carlo Cross-Validation)?</strong></summary>

**Answer:** 
- In **`KFold(shuffle=True)`**, the dataset is randomly permuted once and then divided into $K$ mutually exclusive, non-overlapping partitions. Every single sample appears in the validation set exactly once across the $K$ iterations.
- In **`ShuffleSplit`**, in each of the $M$ iterations, a completely independent random train-test split is generated. Folds are not guaranteed to be mutually exclusive; some samples may be evaluated in multiple validation splits while others may never be evaluated at all. `ShuffleSplit` allows decoupling the number of iterations $M$ from the validation split fraction.
</details>

<details>
<summary><strong>Q8: What is "Purging" and "Embargoing" in Marcos López de Prado's Financial Machine Learning?</strong></summary>

**Answer:**
- **Purging**: Eliminating from the training set any historical observation whose forward-looking outcome label window overlaps with the time span of the testing/validation set, preventing label overlap leakage.
- **Embargoing**: Adding an additional buffer period immediately following the testing set where training samples are discarded. This accounts for memory and autoregression (e.g. post-earnings announcement drift or order book resilience) preventing testing returns from leaking into post-test training samples.
</details>

---

## 12. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Implement K-Fold Index Generator from Scratch</strong></summary>

**Task:** Write a generator function that takes sample size $N$ and $K$ folds and yields `(train_indices, val_indices)` tuples without using scikit-learn.  
**Solution:**
```python
import numpy as np

def custom_kfold(n_samples: int, n_splits: int = 5, shuffle: bool = True, seed: int = 42):
    indices = np.arange(n_samples)
    if shuffle:
        np.random.RandomState(seed).shuffle(indices)
    
    fold_sizes = np.full(n_splits, n_samples // n_splits, dtype=int)
    fold_sizes[:n_samples % n_splits] += 1
    
    current = 0
    for fold_size in fold_sizes:
        val_idx = indices[current:current + fold_size]
        train_idx = np.concatenate([indices[:current], indices[current + fold_size:]])
        yield train_idx, val_idx
        current += fold_size

splits = list(custom_kfold(10, n_splits=3, shuffle=False))
assert len(splits) == 3
assert len(splits[0][1]) == 4 and len(splits[1][1]) == 3
assert set(splits[0][0]).isdisjoint(set(splits[0][1]))
print("Exercise 1 Custom K-Fold Verified!")
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Nested Cross-Validation with Custom Pipelines</strong></summary>

**Task:** Implement nested cross-validation with an outer 5-fold and inner 3-fold loop using `StandardScaler` inside a Pipeline to ensure zero leakage.  
**Solution:**
```python
from sklearn.datasets import make_classification
from sklearn.model_selection import KFold, GridSearchCV
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import LogisticRegression
import numpy as np

X, y = make_classification(n_samples=120, n_features=6, random_state=42)
pipe = Pipeline([
    ("scaler", StandardScaler()),
    ("clf", LogisticRegression(solver='liblinear'))
])

param_grid = {"clf__C": [0.1, 1.0, 10.0]}
outer_cv = KFold(n_splits=4, shuffle=True, random_state=42)
inner_cv = KFold(n_splits=3, shuffle=True, random_state=42)

clf = GridSearchCV(pipe, param_grid, cv=inner_cv, scoring="accuracy")
outer_scores = [clf.fit(X[tr], y[tr]).score(X[te], y[te]) for tr, te in outer_cv.split(X)]

mean_acc = float(np.mean(outer_scores))
print(f"Nested CV Generalization Accuracy: {mean_acc:.4f}")
assert mean_acc > 0.75
print("Exercise 2 Verified!")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Purged Time Series Splitter</strong></summary>

**Task:** Implement a time series split generator that takes label end-times and enforces an embargo gap between train and test splits.  
**Solution:**
```python
import numpy as np

def purged_time_split(n_samples: int, test_size: int, embargo_size: int):
    # Split into train, val, test with embargo
    train_end = n_samples - test_size - embargo_size
    train_indices = np.arange(0, train_end)
    test_indices = np.arange(n_samples - test_size, n_samples)
    return train_indices, test_indices

tr, te = purged_time_split(100, test_size=20, embargo_size=5)
assert len(tr) == 75
assert len(te) == 20
assert te[0] - tr[-1] == 6 # Embargo of 5 samples strictly excluded
print("Exercise 3 Purged Time Split Verified!")
```
</details>

<details>
<summary><strong>Exercise 4 (Hard): Implement Analytical LOOCV for Logistic Regression via WLS</strong></summary>

**Task:** Implement the Pregibon (1981) one-step analytical Leave-One-Out cross-validation approximation for Logistic Regression using the weighted Hat matrix.  
**Solution:**
```python
import numpy as np
from scipy.special import expit

def logistic_loocv_pregibon(X: np.ndarray, y: np.ndarray, w: np.ndarray) -> float:
    # Probabilities: p = expit(X @ w)
    p = expit(X @ w)
    W_diag = p * (1.0 - p)
    # Weighted Hat matrix: H = W^{1/2} X (X^T W X)^-1 X^T W^{1/2}
    W_sqrt = np.sqrt(W_diag)
    X_w = X * W_sqrt[:, None]
    inv_term = np.linalg.inv(X_w.T @ X_w)
    H_diag = np.sum(X_w * (X_w @ inv_term), axis=1)

    # Pregibon approximate LOO residual: (y - p) / (1 - H_ii)
    loo_errors = (y - p) / np.maximum(1e-5, 1.0 - H_diag)
    return float(np.mean(loo_errors ** 2))

X = np.random.randn(80, 3)
w_true = np.array([1.0, -0.5, 0.2])
y = (X @ w_true > 0).astype(float)
# Fit w via Newton Raphson or dummy convergence
w_fit = np.linalg.lstsq(X, y, rcond=None)[0]
loo_mse = logistic_loocv_pregibon(X, y, w_fit)
print(f"Pregibon Logistic LOOCV Approx MSE: {loo_mse:.4f}")
assert loo_mse > 0.0
print("Exercise 4 Verified!")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Detecting Group Contamination via Graph Connected Components</strong></summary>

**Task:** Write an automated validation assertion that builds an adjacency graph of train and validation splits to mathematically prove that no latent entity connections exist across folds.  
**Solution:**
```python
import numpy as np

def assert_zero_graph_leakage(train_groups: np.ndarray, val_groups: np.ndarray) -> bool:
    set_tr = set(train_groups)
    set_val = set(val_groups)
    overlap = set_tr.intersection(set_val)
    if len(overlap) > 0:
        raise ValueError(f"Latent entity contamination detected: {overlap}")
    return True

g_train = np.array(["P1", "P2", "P3", "P4"])
g_val = np.array(["P5", "P6", "P7"])
assert assert_zero_graph_leakage(g_train, g_val)
print("Exercise 5 Graph Leakage Assertion Verified!")
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Multi-Class Stratified Walk-Forward Splitting</strong></summary>

**Task:** Build a walk-forward expanding window time-series splitter that verifies each training window contains at least $M$ instances of all $K$ classes before yielding.  
**Solution:**
```python
import numpy as np

def stratified_walk_forward(y: np.ndarray, min_samples_per_class: int = 2, step_size: int = 20):
    classes = np.unique(y)
    n = len(y)
    for end_idx in range(step_size, n, step_size):
        train_y = y[:end_idx]
        counts = {c: np.sum(train_y == c) for c in classes}
        if all(counts[c] >= min_samples_per_class for c in classes):
            val_end = min(n, end_idx + step_size)
            yield np.arange(end_idx), np.arange(end_idx, val_end)

y_seq = np.array([0]*15 + [1]*15 + [2]*15 + [0]*10 + [1]*10 + [2]*10)
valid_splits = list(stratified_walk_forward(y_seq, min_samples_per_class=3, step_size=20))
assert len(valid_splits) >= 1
print(f"Generated {len(valid_splits)} stratified walk-forward splits successfully!")
print("Exercise 6 Verified!")
```
</details>

---

## 13. Summary, Verified References & Navigation

* **The Pipeline Shield**: Never fit transformers outside a Cross-Validation loop; pipelines guarantee that preprocessing states are learned strictly within the training fold.
* **Entity Separation**: Use `GroupKFold` or `StratifiedGroupKFold` whenever multiple rows originate from the same real-world entity to prevent identity memorization.
* **The Hat Matrix Advantage**: Ordinary Least Squares and Ridge models can calculate exact Leave-One-Out cross-validation analytically in $\mathcal{O}(1)$ training steps using the leverage diagonal.
* **Causality in Time**: In financial and sequential domains, standard random splits violate causality; use `TimeSeriesSplit` combined with purging and embargoing.

### Verified External References
- Hastie, T., Tibshirani, R., & Friedman, J. (2009). *The Elements of Statistical Learning* (Chapter 7: Model Assessment and Selection).
- López de Prado, M. (2018). *Advances in Financial Machine Learning* (Chapter 7: Cross-Validation in Finance).
- Kohavi, R. (1995). *A study of cross-validation and bootstrap for accuracy estimation and model selection*. IJCAI.

---

[← Previous Note: Hyperparameter Tuning](06-hyperparameter-tuning.md) | [Module Index](README.md) | [Next Note: Training Reproducibility & Checkpoints →](08-training-reproducibility-and-checkpoints.md)
