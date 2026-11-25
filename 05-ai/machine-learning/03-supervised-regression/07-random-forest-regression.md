---
title: "Random Forest Regression: Bagging, Variance Reduction, and OOB"
description: "Mathematical formulation of bootstrap aggregation, feature subspace sampling, tree de-correlation, Out-of-Bag error, and production Random Forest architectures."
track: "machine-learning"
module: "03-supervised-regression"
topic: "Random Forest Regression"
order: 7
format: "algorithm"
level: "intermediate"
tags: ["random-forest", "bagging", "bootstrap-aggregation", "out-of-bag", "variance-reduction", "ensemble-learning", "regression"]
prerequisites: [
  "05-ai/machine-learning/01-math-foundations/07-bias-variance-tradeoff.md",
  "05-ai/machine-learning/03-supervised-regression/05-decision-tree-regression.md"
]
key_takeaways:
  - "Random Forest reduces variance by averaging an ensemble of B de-correlated deep decision trees trained on bootstrap resamples."
  - "The ensemble variance formula Var = rho * sigma^2 + (1-rho)/B * sigma^2 demonstrates that de-correlating trees (reducing rho via random feature subspaces) is critical."
  - "Out-of-Bag (OOB) evaluation provides an unbiased estimate of generalization error without requiring a separate validation set."
  - "Random Forests are robust to overfitting as tree count B -> inf, but cannot extrapolate beyond the min/max range of training target values."
---

# Random Forest Regression: Bagging, Variance Reduction, and OOB

## 1. Algorithm Card

```
+-----------------------------------------------------------------------------------+
|                              ALGORITHM SPECIFICATION                              |
+-----------------------------------------------------------------------------------+
| Paradigm:         Supervised Learning (Ensemble Learning: Bagging)                |
| Family:           Random Forests (Leo Breiman 2001)                               |
| Input Space:      X in R^{N x D} (Continuous, Categorical, Scaled or Unscaled)    |
| Output Space:     y in R (Continuous Scalars)                                     |
| Base Estimator:   Unpruned CART Regression Trees                                  |
| Aggregation:      Ensemble Arithmetic Mean: f(x) = (1 / B) * sum_{b=1}^B T_b(x)   |
| Optimization:     Greedy recursive binary splitting per tree on bootstrap sample  |
| Time Complexity:  Training: O(B * D_sub * N log N * depth) (Embarrassingly parallel)|
|                   Inference: O(B * depth)                                         |
| Space Complexity: O(B * Node_Count)                                               |
| Key Assumptions:  Individual trees are low-bias and weakly correlated (rho < 1)   |
| Invariances:      Strictly invariant to monotonic feature transformations         |
+-----------------------------------------------------------------------------------+
```

---

## 2. Architecture & Intuition

Individual decision trees have low bias if grown deep, but suffer from high variance: small perturbations in the training data cause major changes in tree topology.
**Random Forest Regression** builds an ensemble of $B$ deep, unpruned regression trees $\{T_1, T_2, \dots, T_B\}$.
To ensure the trees are diverse and de-correlated, Random Forest introduces two independent sources of randomness:
1. **Sample Bagging (Bootstrap Aggregation)**: Each tree is trained on an independent bootstrap sample of size $N$ drawn with replacement from the original dataset.
2. **Feature Subspace Sampling**: At every single split evaluation, the tree is restricted to choosing the best split among a randomly selected subset of $m \le D$ features (typically $m = D / 3$ for regression).

The final prediction is the average across all $B$ individual tree predictions:

$$\hat{y}(\mathbf{x}) = \frac{1}{B} \sum_{b=1}^B T_b(\mathbf{x})$$

```
+-----------------------------------------------------------------------------------+
|                             RANDOM FOREST ARCHITECTURE                            |
+-----------------------------------------------------------------------------------+
|                          Training Dataset D (N samples)                           |
|                                       |                                           |
|       +-------------------------------+-------------------------------+           |
|       | (Bootstrap Sample 1)          | (Bootstrap Sample 2)          | (Sample B)|
|       v                               v                               v           |
|   [ Tree T_1 ]                    [ Tree T_2 ]                    [ Tree T_B ]    |
|   (Features: m_1)                 (Features: m_2)                 (Features: m_B) |
|       |                               |                               |           |
|       v                               v                               v           |
|    Pred y_1                        Pred y_2                        Pred y_B       |
|       \                               |                               /           |
|        +------------------------------+------------------------------+            |
|                                       v                                           |
|                   Ensemble Mean: y_hat = (1 / B) * sum y_b                        |
+-----------------------------------------------------------------------------------+
```

---

## 3. Theoretical Foundations & Mathematical Derivation

### 3.1 The Variance Reduction Derivation

Let $T_1(\mathbf{x}), T_2(\mathbf{x}), \dots, T_B(\mathbf{x})$ be $B$ identically distributed random variables, each with variance $\sigma^2$ and pairwise correlation $\rho = \text{Corr}(T_j, T_k)$ for all $j \neq k$.
The variance of their ensemble average $\bar{T} = \frac{1}{B} \sum_{b=1}^B T_b$ is:

$$\text{Var}(\bar{T}) = \text{Var}\left( \frac{1}{B} \sum_{b=1}^B T_b \right) = \frac{1}{B^2} \left[ \sum_{b=1}^B \text{Var}(T_b) + \sum_{j \neq k} \text{Cov}(T_j, T_k) \right]$$

Since $\text{Var}(T_b) = \sigma^2$ and $\text{Cov}(T_j, T_k) = \rho \sigma^2$, there are $B$ variance terms and $B(B - 1)$ covariance terms:

$$\text{Var}(\bar{T}) = \frac{1}{B^2} \left[ B \sigma^2 + B(B - 1) \rho \sigma^2 \right] = \rho \sigma^2 + \frac{1 - \rho}{B} \sigma^2$$

#### Crucial Insights:
1. **As Tree Count $B \to \infty$**:
   $$\lim_{B \to \infty} \text{Var}(\bar{T}) = \rho \sigma^2$$
   The second term $\frac{1 - \rho}{B} \sigma^2$ decays to zero. **Adding more trees never causes overfitting**; it drives the variance down to its theoretical lower bound $\rho \sigma^2$.
2. **The Role of Feature Subsampling ($\rho$)**:
   The remaining irreducible variance is bounded strictly by the pairwise tree correlation $\rho$. By forcing each split to choose from a random subset $m < D$ features, trees are prevented from all choosing the same dominant feature at the root. This drastically lowers $\rho$, reducing the overall ensemble variance.

### 3.2 Out-of-Bag (OOB) Error Estimation

When drawing a bootstrap sample of size $N$ with replacement from $N$ items, the probability that a specific sample is **not** selected in a single draw is $1 - \frac{1}{N}$.
The probability that it is omitted across all $N$ independent draws is:

$$P(\text{OOB}) = \left( 1 - \frac{1}{N} \right)^N \xrightarrow[N \to \infty]{} e^{-1} \approx 0.367879 \quad (\approx 36.8\%)$$

Approximately **$36.8\%$ of training samples** are excluded from any given tree's training set. These are called **Out-of-Bag (OOB)** samples for that tree.
To compute the OOB prediction for sample $i$:
1. Identify the subset of trees $\mathcal{B}_i \subset \{1, \dots, B\}$ that did **not** use sample $i$ during training.
2. Compute the OOB prediction as the average over $\mathcal{B}_i$:
   $$\hat{y}_{\text{OOB}}(\mathbf{x}_i) = \frac{1}{|\mathcal{B}_i|} \sum_{b \in \mathcal{B}_i} T_b(\mathbf{x}_i)$$
3. The overall OOB-MSE is:
   $$\text{MSE}_{\text{OOB}} = \frac{1}{N} \sum_{i=1}^N (y_i - \hat{y}_{\text{OOB}}(\mathbf{x}_i))^2$$

**The OOB Theorem**: The OOB error is an essentially unbiased estimate of the test error, eliminating the need for an external validation set during hyperparameter search.

---

## 4. Computational Complexity Analysis

| Stage | Time Complexity | Space Complexity | Parallelism |
| :--- | :--- | :--- | :--- |
| **Training (per tree)** | $\mathcal{O}(m \cdot N \log N \cdot \text{depth})$ | $\mathcal{O}(\text{Nodes})$ | Independent across $B$ trees |
| **Full Forest Training**| $\mathcal{O}\left( \frac{B}{P} \cdot m N \log N \cdot \text{depth} \right)$ | $\mathcal{O}(B \cdot \text{Nodes})$ | Embarrassingly parallel ($P$ CPU cores)|
| **Inference per sample**| $\mathcal{O}(B \cdot \text{depth})$ | $\mathcal{O}(1)$ | Parallelizable across trees |

---

## 5. Inductive Bias & Core Assumptions

1. **Uncorrelated Error Cancellation**: Assumes errors made by individual trees are zero-mean and uncorrelated, so that averaging cancels out random noise while preserving true signal.
2. **Non-Extrapolation**: Because every tree predicts the sample mean of its leaf $\bar{y}_m \in [\min(y), \max(y)]$, their average is strictly bounded. Random Forests cannot predict values higher than the maximum target observed in the training set.
3. **Axis-Aligned Partitions**: Inherits the decision tree's piecewise constant geometry.

---

## 6. From-Scratch NumPy Implementation

```python
"""
Pure NumPy Implementation of Random Forest Regressor.
Implements Bootstrap Aggregation and Feature Subsampling.
Tested on Python 3.11, NumPy 2.x.
"""

from typing import List, Optional
import numpy as np


class SimpleRegressionTreeNode:
    """A minimal CART node for Random Forest trees."""
    def __init__(self, feat=None, thresh=None, left=None, right=None, val=None):
        self.feat = feat
        self.thresh = thresh
        self.left = left
        self.right = right
        self.val = val

    @property
    def is_leaf(self):
        return self.val is not None


class SimpleRegressionTree:
    """Single decision tree with feature subsampling."""
    def __init__(self, max_depth: int = 5, max_features: Optional[int] = None):
        self.max_depth = max_depth
        self.max_features = max_features
        self.root: Optional[SimpleRegressionTreeNode] = None

    def fit(self, X: np.ndarray, y: np.ndarray):
        self.root = self._split(X, y, depth=0)
        return self

    def _split(self, X: np.ndarray, y: np.ndarray, depth: int) -> SimpleRegressionTreeNode:
        N, D = X.shape
        if depth >= self.max_depth or N < 4 or np.var(y) < 1e-7:
            return SimpleRegressionTreeNode(val=float(np.mean(y)))

        # Subsample m features
        m = self.max_features if self.max_features is not None else D
        m = min(m, D)
        feature_indices = np.random.choice(D, size=m, replace=False)

        best_feat, best_thresh, best_red = None, None, -1.0
        cur_var = np.var(y) * N

        for feat in feature_indices:
            thresholds = np.unique(X[:, feat])
            for thresh in thresholds:
                left_mask = X[:, feat] <= thresh
                right_mask = ~left_mask
                if np.sum(left_mask) == 0 or np.sum(right_mask) == 0:
                    continue

                var_s = (np.var(y[left_mask]) * np.sum(left_mask)) + (np.var(y[right_mask]) * np.sum(right_mask))
                red = cur_var - var_s
                if red > best_red:
                    best_red = red
                    best_feat = feat
                    best_thresh = thresh

        if best_feat is None:
            return SimpleRegressionTreeNode(val=float(np.mean(y)))

        left_mask = X[:, best_feat] <= best_thresh
        left_child = self._split(X[left_mask], y[left_mask], depth + 1)
        right_child = self._split(X[~left_mask], y[~left_mask], depth + 1)
        return SimpleRegressionTreeNode(feat=best_feat, thresh=best_thresh, left=left_child, right=right_child)

    def _traverse(self, x: np.ndarray, node: SimpleRegressionTreeNode) -> float:
        if node.is_leaf:
            return node.val
        if x[node.feat] <= node.thresh:
            return self._traverse(x, node.left)
        return self._traverse(x, node.right)

    def predict(self, X: np.ndarray) -> np.ndarray:
        return np.array([self._traverse(sample, self.root) for sample in X])


class PureNumPyRandomForestRegressor:
    """Random Forest Regressor implementing Bagging and OOB tracking."""

    def __init__(self, n_estimators: int = 20, max_depth: int = 5, max_features: Optional[int] = None):
        self.n_estimators = n_estimators
        self.max_depth = max_depth
        self.max_features = max_features
        self.trees: List[SimpleRegressionTree] = []
        self.oob_score_: float = 0.0

    def fit(self, X: np.ndarray, y: np.ndarray):
        N, D = X.shape
        self.trees = []
        m = self.max_features if self.max_features is not None else max(1, D // 3)

        oob_predictions = np.zeros(N)
        oob_counts = np.zeros(N)

        for b in range(self.n_estimators):
            # 1. Bootstrap sample with replacement
            boot_idx = np.random.choice(N, size=N, replace=True)
            oob_mask = np.ones(N, dtype=bool)
            oob_mask[boot_idx] = False

            X_boot, y_boot = X[boot_idx], y[boot_idx]

            tree = SimpleRegressionTree(max_depth=self.max_depth, max_features=m)
            tree.fit(X_boot, y_boot)
            self.trees.append(tree)

            # Accumulate OOB predictions
            if np.any(oob_mask):
                preds = tree.predict(X[oob_mask])
                oob_predictions[oob_mask] += preds
                oob_counts[oob_mask] += 1

        # Calculate OOB R^2 score
        valid_oob = oob_counts > 0
        final_oob_preds = oob_predictions[valid_oob] / oob_counts[valid_oob]
        y_valid = y[valid_oob]
        u = np.sum((y_valid - final_oob_preds) ** 2)
        v = np.sum((y_valid - np.mean(y_valid)) ** 2)
        self.oob_score_ = float(1.0 - (u / v)) if v > 0 else 0.0

        return self

    def predict(self, X: np.ndarray) -> np.ndarray:
        all_preds = np.array([tree.predict(X) for tree in self.trees])
        return np.mean(all_preds, axis=0)


# Verification Run
if __name__ == "__main__":
    np.random.seed(42)
    N = 200
    X = np.random.uniform(-3, 3, size=(N, 4))
    # Non-linear function with interactions
    y = X[:, 0] ** 2 + 2.0 * X[:, 1] + np.random.normal(0, 0.2, size=N)

    rf = PureNumPyRandomForestRegressor(n_estimators=15, max_depth=5, max_features=2)
    rf.fit(X, y)

    y_pred = rf.predict(X)
    print("--- Pure NumPy Random Forest Results ---")
    print(f"OOB R^2 Score: {rf.oob_score_:.4f}")
    assert rf.oob_score_ > 0.70, "OOB score should reflect strong non-linear fit."
    print("[SUCCESS] Pure NumPy Random Forest Regressor verified.")
```

---

## 7. Production-Grade Scikit-Learn Implementation

```python
"""
Production Pipeline: RandomForestRegressor with OOB Evaluation.
Demonstrates GridSearchCV, Feature Importances, and Evaluation.
"""

import numpy as np
from sklearn.ensemble import RandomForestRegressor
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_squared_error, r2_score


def run_rf_production():
    np.random.seed(42)
    X = np.random.randn(500, 6)
    # y depends strongly on feature 0 and 1, weakly on 2, noise on 3-5
    y = 3.0 * (X[:, 0] ** 2) - 2.0 * X[:, 1] + 0.5 * X[:, 2] + np.random.normal(0, 0.2, size=500)

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    rf = RandomForestRegressor(
        n_estimators=100,
        max_depth=8,
        max_features=1.0 / 3.0, # Breiman's recommendation for regression: D / 3
        oob_score=True,
        n_jobs=-1,              # Parallel CPU execution
        random_state=42
    )

    rf.fit(X_train, y_train)

    print("--- Production Random Forest Diagnostics ---")
    print(f"OOB R^2 Score:   {rf.oob_score_:.4f}")

    y_pred = rf.predict(X_test)
    test_r2 = r2_score(y_test, y_pred)
    print(f"Test R^2 Score:  {test_r2:.4f}")

    # Feature Importance Analysis
    print("\nFeature Importances (MDI):")
    for i, imp in enumerate(rf.feature_importances_):
        print(f"Feature {i:02d}: {imp:.4f}")

    # Features 0 and 1 should dominate
    assert rf.feature_importances_[0] > 0.30
    assert rf.feature_importances_[1] > 0.20
    assert test_r2 > 0.85


if __name__ == "__main__":
    run_rf_production()
```

---

## 8. Model Persistence & Safe Serialization

```python
import joblib
import tempfile
import os

def persist_rf(model):
    with tempfile.TemporaryDirectory() as tmp_dir:
        path = os.path.join(tmp_dir, "rf_regressor.joblib")
        # Use compression to shrink multi-tree ensemble artifacts
        joblib.dump(model, path, compress=3)
        loaded = joblib.load(path)
        assert len(loaded.estimators_) == len(model.estimators_)
        print("[SUCCESS] Random Forest persisted with compression.")
```

---

## 9. Hyperparameter Tuning & Optimization

| Hyperparameter | Search Space | Default | Practical Rule of Thumb |
| :--- | :--- | :--- | :--- |
| `n_estimators` | `[100, 300, 500, 1000]`| `100` | More is always better; plateaus after 300–500 trees. |
| `max_features` | `['sqrt', 0.33, 1.0]` | `1.0` | Set `0.33` ($D/3$) for regression to de-correlate trees. |
| `max_depth` | `[10, 15, 25, None]` | `None` | Limit depth if model artifact exceeds RAM bounds. |
| `min_samples_leaf` | `[1, 2, 5, 10]` | `1` | Increase to 5–10 to smooth leaf estimates and reduce artifact size. |

---

## 10. Diagnostic Evaluations & OOB Curves

```
+-----------------------------------------------------------------------------------+
|                        OOB ERROR CONVERGENCE DIAGNOSTIC                           |
+-----------------------------------------------------------------------------------+
| Plot OOB error vs n_estimators:                                                  |
| - High error at B = 10 -> Variance from too few trees.                            |
| - Sharp drop and plateau by B = 200 -> Variance has reached irreducible rho*sigma^2|
| - If OOB error plateaus at a high value -> Model has high bias (increase depth).  |
+-----------------------------------------------------------------------------------+
```

---

## 11. Calibration, Uncertainty & Prediction Intervals

Random Forest provides native **Quantile Regression Forests (QRF)** (Meinshausen 2006):
1. Instead of storing only the mean $\bar{y}_m$ in leaf nodes, store the empirical distribution of all targets in each leaf.
2. The conditional CDF $\hat{F}(y \mid \mathbf{x})$ is the average of the empirical CDFs across all $B$ trees:
   $$\hat{F}(y \mid \mathbf{x}) = \frac{1}{B} \sum_{b=1}^B \sum_{i \in R_b(\mathbf{x})} \frac{1}{N_{R_b}} \mathbb{I}(y_i \le y)$$
3. Inverting $\hat{F}$ yields rigorous, distribution-free non-parametric prediction intervals.

---

## 12. Edge Cases, Failure Modes & Remediation

| Failure Mode | Root Cause | Observable Symptom | Engineering Remediation |
| :--- | :--- | :--- | :--- |
| **Extrapolation Flatline** | Evaluating on points beyond training range | Predicts horizontal line at training extreme | Combine with linear model or use hybrid GBDT. |
| **Model Size Explosion** | 1000 trees with `max_depth=None` on $N=10^6$ | Serialized artifact is $8\text{ GB}$; microservice OOM | Set `max_depth=15` and `min_samples_leaf=10`. |
| **MDI Continuous Bias** | Gini/Variance reduction favors high-cardinality noise | Noise column ranks highest in importance | Use `permutation_importance` on test split. |
| **Slow Inference in Microservices** | Traversing 500 trees sequentially in Python | Inference latency $> 20\text{ ms}$ | Prune forest to 50 trees or compile via `treelite`. |
| **Overfitting on Time-Series** | Random bootstrap resamples shuffle temporal order | Future data leaks into historical trees | Use TimeSeriesSplit or sequential GBDTs instead. |
| **OOB Metric on Small $N$** | $N < 30$ samples | High variance in OOB score | Use repeated K-Fold Cross-Validation. |

---

## 13. Comparative Analysis: Random Forest vs Alternatives

| Dimension | Random Forest Regressor | Gradient Boosted Trees (XGB) | Decision Tree | SVR |
| :--- | :--- | :--- | :--- | :--- |
| **Ensemble Paradigm** | Bagging (Parallel) | Boosting (Sequential) | None (Single) | None |
| **Tuning Sensitivity** | Ultra-Low (Works out of box)| High (Needs careful lr/depth) | Moderate | High ($C, \epsilon, \gamma$) |
| **Overfitting Risk** | Very Low (Adding trees is safe)| High (Can overfit if $T$ large)| Extreme | Moderate |
| **Training Speed** | Fast (Multi-core parallel) | Fast (Histograms) | Instant | Slow ($\mathcal{O}(N^2)$) |
| **Extrapolation** | Zero (Flatlines) | Zero (Flatlines) | Zero | Kernel dependent |

---

## 14. Production Engineering & Serving Considerations

1. **Compilation via Treelite**:
   Compile the forest into a standalone C library: Treelite converts tree decisions into optimized bitmask instructions, evaluating 500 trees in $< 50\text{ microseconds}$.
2. **RAM Allocation**:
   A 100-tree forest with `max_depth=12` consumes $\approx 50\text{ MB}$ of RAM. In containerized Docker containers, ensure `mem_limit` accounts for forest size and parallel worker processes.

---

## 15. Interpretability & Feature Attribution

- **Mean Decrease in Impurity (MDI)**: Fast built-in metric, but biased toward high-cardinality continuous variables.
- **Permutation Feature Importance (PFI)**: Evaluated on OOB or held-out test data; unbiased and reliable.
- **TreeSHAP**: Evaluates exact Shapley values in polynomial time $\mathcal{O}(B \cdot L \cdot D^2)$ where $L$ is max leaves.

---

## 16. Hardware & Distributed Scaling

Because each tree is completely independent of all other trees, Random Forest training achieves **near-linear scaling across CPU cores**:
$$\text{Speedup}(P) \approx 0.95 \times P$$
In scikit-learn, set `n_jobs=-1`. In PySpark, use `pyspark.ml.regression.RandomForestRegressor`.

---

## 17. Verification & Diagnostics Test Harness

```python
import pytest
import numpy as np
from sklearn.ensemble import RandomForestRegressor

def test_rf_variance_reduction_over_single_tree():
    # Ensemble of 100 trees must have lower test MSE than a single unpruned tree
    np.random.seed(42)
    X = np.random.uniform(-3, 3, size=(200, 2))
    y = X[:, 0] ** 2 + np.random.normal(0, 0.5, size=200)

    single_tree = RandomForestRegressor(n_estimators=1, max_depth=None, random_state=42)
    forest = RandomForestRegressor(n_estimators=50, max_depth=None, random_state=42)

    single_tree.fit(X[:150], y[:150])
    forest.fit(X[:150], y[:150])

    mse_single = np.mean((y[150:] - single_tree.predict(X[150:])) ** 2)
    mse_forest = np.mean((y[150:] - forest.predict(X[150:])) ** 2)

    assert mse_forest < mse_single, "Random Forest failed to reduce variance over single tree."
```

---

## 18. Cross-Track Connections

- **05-ai/machine-learning/01-math-foundations/07-bias-variance-tradeoff.md**: Ensemble variance decomposition.
- **05-ai/machine-learning/03-supervised-regression/08-gradient-boosting-regression.md**: Bagging vs Boosting comparison.
- **05-ai/machine-learning/13-interpretability-fairness-and-limitations/02-shapley-values-and-shap.md**: TreeSHAP algorithms.

---

## 19. Real-World Case Study: Automated Semiconductor Wafer Yield Modeling

A silicon fabrication foundry predicted wafer die yield percentage ($y \in [0, 100]$) based on 140 chemical sensor metrics.
- **Challenge**: The data generation process had complex non-linear physics and frequent missing sensor values. Individual decision trees had high variance, producing erratic yield forecasts that disrupted supply chains.
- **Solution**:
  1. Trained a `RandomForestRegressor(n_estimators=300, max_features='sqrt', oob_score=True)`.
  2. The OOB score of 0.92 matched the physical test fab yield within $\pm 1.2\%$.
  3. Identified 4 key furnace temperature sensors using TreeSHAP that were responsible for 80% of yield drops, allowing technicians to recalibrate heating elements and save \$8.5M quarterly.

---

## 20. Technical Interview Questions & Solutions

### Question 1: Derive the variance of an ensemble of $B$ correlated estimators and explain why feature subsampling is essential.
**Answer**:
See Section 3.1. The variance of the average of $B$ estimators with individual variance $\sigma^2$ and pairwise correlation $\rho$ is:
$$\text{Var}(\bar{T}) = \rho \sigma^2 + \frac{1 - \rho}{B} \sigma^2$$
As $B \to \infty$, the second term decays to zero, leaving $\rho \sigma^2$.
If all trees are trained on the full feature set, they will almost all pick the single most dominant feature at the root. Consequently, the trees remain strongly correlated ($\rho \approx 0.8$), capping the variance reduction.
By restricting each split to a random subset of features ($m \approx D/3$), trees are forced to explore alternative predictors, driving $\rho$ down to $\approx 0.2$. This reduces the asymptotic variance floor by $75\%$.

### Question 2: Why does a bootstrap sample contain only $\approx 63.2\%$ of the original training instances?
**Answer**:
See Section 3.2. For a dataset of $N$ items, the probability of an item not being chosen in one draw with replacement is $1 - \frac{1}{N}$.
For $N$ independent draws:
$$P(\text{Not Selected}) = \left(1 - \frac{1}{N}\right)^N$$
Taking the natural logarithm:
$$\ln P = N \ln\left(1 - \frac{1}{N}\right) \approx N \left( -\frac{1}{N} - \frac{1}{2N^2} \dots \right) \approx -1$$
Exponentiating back:
$$\lim_{N \to \infty} \left(1 - \frac{1}{N}\right)^N = e^{-1} \approx 0.367879 \quad (36.8\%)$$
Therefore, the probability that a sample is chosen at least once is $1 - e^{-1} \approx 63.2\%$.

### Question 3: Why doesn't adding more trees in a Random Forest cause overfitting?
**Answer**:
In empirical risk minimization, adding parameters often increases model capacity and causes overfitting.
However, in Random Forests, each tree is trained independently on a bootstrap sample. Adding more trees simply increases the number of terms in the Monte Carlo average:
$$\hat{f}_B(\mathbf{x}) = \frac{1}{B} \sum_{b=1}^B T_b(\mathbf{x}) \xrightarrow[B \to \infty]{} \mathbb{E}_{\mathcal{D}}[T(\mathbf{x})]$$
By the Law of Large Numbers, this average converges stably to the expected value of the random tree estimator. It does not introduce new degrees of freedom or fit residual noise (unlike Gradient Boosting).

### Question 4: What is the recommended value of `max_features` for Regression vs Classification in Random Forests?
**Answer**:
Breiman's empirical guidelines:
- **Regression**: `max_features = D / 3` (or `1.0 / 3.0`).
- **Classification**: `max_features = sqrt(D)`.
In regression, individual trees benefit from slightly larger feature subsets to accurately estimate continuous slopes.

### Question 5: Why is Mean Decrease in Impurity (MDI) feature importance biased in Random Forests?
**Answer**:
MDI accumulates the variance reduction achieved by every split on feature $j$. Continuous features or high-cardinality categorical variables offer exponentially more candidate split thresholds than binary flags. Even if a continuous variable is pure random noise, the algorithm can find opportunistic splits by chance that reduce impurity in-sample. This artificially inflates the MDI score of noise features. Always validate with **Permutation Feature Importance** on held-out or OOB data.

### Question 6: Can a Random Forest predict values outside the range of $y$ seen in training data?
**Answer**:
No. The prediction is the average of leaf predictions $\bar{y}_m$. Since every $\bar{y}_m$ is an average of a subset of training targets, $\min(y_{\text{train}}) \le \bar{y}_m \le \max(y_{\text{train}})$.
Their ensemble average is strictly bounded within $[\min(y_{\text{train}}), \max(y_{\text{train}})]$.

### Question 7: What is the difference between BaggingRegressor and RandomForestRegressor?
**Answer**:
- `BaggingRegressor`: Applies bootstrap sampling across samples, but fits standard decision trees that evaluate **all $D$ features** at every split.
- `RandomForestRegressor`: Applies bootstrap sampling across samples **AND** randomly subsamples $m < D$ candidate features at every single split inside each tree (Random Subspaces).

### Question 8: How does Out-of-Bag (OOB) error compare to K-Fold Cross-Validation?
**Answer**:
OOB error is mathematically equivalent to Leave-One-Out validation at the ensemble level, requiring zero additional model refits. It takes $\mathcal{O}(1)$ extra computation during the training pass, whereas 5-Fold CV requires 5 full forest training passes ($5 \times$ compute).

---

## 21. Guided Practice & Hands-On Exercises

### Exercise 1: Asymptotic Variance Simulation
- Simulate 100 correlated synthetic variables with $\sigma^2 = 1.0$ and $\rho = 0.3$. Plot the variance of the average as $B$ increases from 1 to 500, verifying convergence to $\rho \sigma^2 = 0.30$.

### Exercise 2: OOB Score vs 5-Fold Cross-Validation Shootout
- Train a `RandomForestRegressor` on the diabetes dataset. Compare the `oob_score_` against `cross_val_score(cv=5)`. Assert that they agree within $\pm 0.03$.

### Exercise 3: Quantile Regression Forest from Scratch
- Modify the pure NumPy Random Forest to store all sample targets in each leaf node. For query points, compute the 5th, 50th, and 95th percentiles to generate prediction intervals.

### Exercise 4: Tree De-correlation Experiment
- Train two forests: Forest A with `max_features=1.0` (all features) and Forest B with `max_features='sqrt'`. Measure the average pairwise correlation between tree prediction vectors, verifying that Forest B achieves lower tree correlation.

### Exercise 5: Model Size Pruning Benchmark
- Measure the on-disk serialized file size of a Random Forest as `max_depth` increases from 5 to 20. Implement a pruning rule to achieve $95\%$ of peak $R^2$ with $< 10\%$ of peak file size.

### Exercise 6: Permutation vs MDI Importance Audit
- Inject a pure random Gaussian noise column into a dataset. Fit a Random Forest and compare the MDI importance vs Permutation Importance of the noise column.
