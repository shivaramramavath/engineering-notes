---
title: "Gradient Boosting Regression: Residual Fitting, Loss Functions, and GBDT"
description: "Mathematical formulation of Gradient Boosted Decision Trees (GBDT), pseudo-residuals, shrinkage, Huber loss, and modern implementations (XGBoost, LightGBM, CatBoost)."
track: "machine-learning"
module: "03-supervised-regression"
topic: "Gradient Boosting Regression"
order: 8
format: "algorithm"
level: "intermediate"
tags: ["gradient-boosting", "gbdt", "xgboost", "lightgbm", "catboost", "pseudo-residuals", "huber-loss", "regression"]
prerequisites: [
  "05-ai/machine-learning/01-math-foundations/06-optimisation-and-gradient-descent.md",
  "05-ai/machine-learning/03-supervised-regression/05-decision-tree-regression.md",
  "05-ai/machine-learning/03-supervised-regression/07-random-forest-regression.md"
]
key_takeaways:
  - "Gradient Boosting trains an ensemble of weak trees sequentially; each new tree fits the negative gradient (pseudo-residuals) of the loss function."
  - "Learning rate shrinkage eta in (0, 1] acts as a critical regularizer, preventing early trees from dominating the ensemble."
  - "Different loss functions change the target of the trees: L2 fits raw residuals; L1 fits the sign of residuals (median); Huber loss transitions smoothly between L2 and L1."
  - "Modern implementations optimize efficiency: LightGBM uses histogram binning (GOSS), XGBoost uses exact second-order Taylor expansion, and CatBoost uses symmetric oblivious trees."
---

# Gradient Boosting Regression: Residual Fitting, Loss Functions, and GBDT

## 1. Algorithm Card

```
+-----------------------------------------------------------------------------------+
|                              ALGORITHM SPECIFICATION                              |
+-----------------------------------------------------------------------------------+
| Paradigm:         Supervised Learning (Ensemble Learning: Sequential Boosting)    |
| Family:           Gradient Boosted Decision Trees (Jerome Friedman 1999, 2001)    |
| Input Space:      X in R^{N x D} (Continuous, Categorical, Native NaN support)    |
| Output Space:     y in R (Continuous Scalars)                                     |
| Base Estimator:   Shallow Decision Trees (typically depth 3 to 6)                 |
| Ensemble Form:    F_M(x) = F_0(x) + sum_{m=1}^M eta * h_m(x)                      |
| Optimization:     Functional Gradient Descent in Function Space                   |
| Time Complexity:  Training: O(M * N * D_sub) with HistGradientBoosting            |
|                   Inference: O(M * depth)                                         |
| Space Complexity: O(M * Nodes)                                                    |
| Key Assumptions:  Additive model structure; sequential residuals are minimizable  |
| Invariances:      Strictly invariant to monotonic feature scaling                 |
+-----------------------------------------------------------------------------------+
```

---

## 2. Architecture & Intuition

While Random Forest builds independent trees in parallel to reduce variance, **Gradient Boosting** builds trees sequentially to reduce bias.
Each new tree $h_m(\mathbf{x})$ is trained to correct the remaining errors made by the existing ensemble $F_{m-1}(\mathbf{x})$.

### Visual Mental Model: Gradient Descent in Function Space

In standard parameter optimization, gradient descent updates weight vectors in parameter space:
$$\mathbf{w}^{(m)} = \mathbf{w}^{(m-1)} - \eta \cdot \nabla_{\mathbf{w}} L$$

In Gradient Boosting, optimization occurs **directly in function space**. The predictions $\mathbf{f} = [F(\mathbf{x}_1), \dots, F(\mathbf{x}_N)]^T$ are the variables.
To minimize loss $\sum_{i=1}^N L(y_i, F(\mathbf{x}_i))$, we compute the gradient of the loss with respect to the current predictions:

$$r_{im} = -\left[ \frac{\partial L(y_i, F(\mathbf{x}_i))}{\partial F(\mathbf{x}_i)} \right]_{F = F_{m-1}}$$

These $r_{im}$ values are called **pseudo-residuals**. We fit a shallow regression tree $h_m(\mathbf{x})$ to predict these pseudo-residuals, moving the ensemble function in the direction of steepest descent.

```
+-----------------------------------------------------------------------------------+
|                           GRADIENT BOOSTING ARCHITECTURE                          |
+-----------------------------------------------------------------------------------+
|  [Target y]                                                                       |
|      |                                                                            |
|      v                                                                            |
|  [Base F_0 = mean(y)] ---> Prediction F_0                                         |
|      |                                                                            |
|      v                                                                            |
|  [Residuals r_1 = y - F_0] ---> [Tree h_1] ---> Update: F_1 = F_0 + eta * h_1     |
|                                     |                                             |
|                                     v                                             |
|  [Residuals r_2 = y - F_1] ---> [Tree h_2] ---> Update: F_2 = F_1 + eta * h_2     |
|                                     |                                             |
|                                     v                                             |
|                                [Tree h_M] ---> Final: F_M = F_0 + sum eta * h_m   |
+-----------------------------------------------------------------------------------+
```

---

## 3. Theoretical Foundations & Mathematical Derivation

### 3.1 The Generic Gradient Boosting Algorithm (Friedman)

Given training dataset $\{(\mathbf{x}_i, y_i)\}_{i=1}^N$ and a differentiable loss function $L(y, F(\mathbf{x}))$:

1. **Initialize Base Model with a Constant Value**:
   $$F_0(\mathbf{x}) = \arg\min_\gamma \sum_{i=1}^N L(y_i, \gamma)$$
   - For Squared Error ($L_2$): $F_0(\mathbf{x}) = \bar{y}$ (Sample Mean).
   - For Absolute Error ($L_1$): $F_0(\mathbf{x}) = \text{median}(y)$ (Sample Median).
2. **For iteration $m = 1, 2, \dots, M$**:
   a. **Compute Pseudo-Residuals** for all $i \in \{1, \dots, N\}$:
      $$r_{im} = -\left[ \frac{\partial L(y_i, F(\mathbf{x}_i))}{\partial F(\mathbf{x}_i)} \right]_{F(\mathbf{x}_i) = F_{m-1}(\mathbf{x}_i)}$$
   b. **Fit a Regression Tree** to the pseudo-residuals $\{(\mathbf{x}_i, r_{im})\}_{i=1}^N$, creating terminal leaf regions $R_{jm}$ for $j \in \{1, \dots, J_m\}$.
   c. **Compute Optimal Leaf Output Values** $\gamma_{jm}$ (Line Search):
      $$\gamma_{jm} = \arg\min_\gamma \sum_{\mathbf{x}_i \in R_{jm}} L(y_i, F_{m-1}(\mathbf{x}_i) + \gamma)$$
   d. **Update Ensemble with Shrinkage** $\eta \in (0, 1]$:
      $$F_m(\mathbf{x}) = F_{m-1}(\mathbf{x}) + \eta \sum_{j=1}^{J_m} \gamma_{jm} \mathbb{I}(\mathbf{x} \in R_{jm})$$
3. **Output Final Model**: $F_M(\mathbf{x})$.

### 3.2 Mathematical Analysis of Loss Functions

| Loss Function | Mathematical Objective $L(y, F)$ | Pseudo-Residual $r = -\frac{\partial L}{\partial F}$ | Optimal Leaf Value $\gamma_j$ | Robustness |
| :--- | :--- | :--- | :--- | :--- |
| **Squared Error ($L_2$)** | $\frac{1}{2}(y - F)^2$ | $y - F$ (Standard Residual) | $\text{mean}(r_i)$ in leaf | Low (Outlier sensitive) |
| **Absolute Error ($L_1$)** | $|y - F|$ | $\text{sign}(y - F) \in \{-1, +1\}$ | $\text{median}(y_i - F_{m-1})$ | High (Median regression) |
| **Huber Loss** | $\begin{cases} \frac{1}{2}(y-F)^2 & \text{if } |y-F| \le \delta \\ \delta(|y-F| - \frac{\delta}{2}) & \text{otherwise} \end{cases}$ | $\begin{cases} y - F & \text{if } |y-F| \le \delta \\ \delta \cdot \text{sign}(y - F) & \text{otherwise} \end{cases}$ | Approximation via median/mean | Maximum (Best for noisy tabular data) |

### 3.3 Second-Order Taylor Expansion (XGBoost Formulation)

Chen & Guestrin (2016) extended Friedman's formulation by approximating any convex loss using a second-order Taylor expansion:

$$\mathcal{L}^{(m)} \approx \sum_{i=1}^N \left[ L(y_i, F_{m-1}(\mathbf{x}_i)) + g_i f_m(\mathbf{x}_i) + \frac{1}{2} h_i f_m^2(\mathbf{x}_i) \right] + \Omega(f_m)$$

Where:
- $g_i = \frac{\partial L(y_i, F_{m-1}(\mathbf{x}_i))}{\partial F_{m-1}(\mathbf{x}_i)}$ is the first-order gradient.
- $h_i = \frac{\partial^2 L(y_i, F_{m-1}(\mathbf{x}_i))}{\partial F_{m-1}^2(\mathbf{x}_i)}$ is the second-order Hessian.
- $\Omega(f_m) = \gamma T + \frac{1}{2} \lambda \sum_{j=1}^T w_j^2$ is an explicit regularization penalty on the number of leaves $T$ and leaf weights $w_j$.

The optimal leaf weight for leaf $j$ has a closed-form solution:
$$w_j^* = -\frac{\sum_{i \in I_j} g_i}{\sum_{i \in I_j} h_i + \lambda}$$
The split quality score is:
$$\text{Gain} = \frac{1}{2} \left[ \frac{(\sum_{i \in I_L} g_i)^2}{\sum_{i \in I_L} h_i + \lambda} + \frac{(\sum_{i \in I_R} g_i)^2}{\sum_{i \in I_R} h_i + \lambda} - \frac{(\sum_{i \in I} g_i)^2}{\sum_{i \in I} h_i + \lambda} \right] - \gamma$$

---

## 4. Computational Complexity Analysis

| Algorithm | Split Finding Algorithm | Complexity per Tree | High-Dimensional Speed |
| :--- | :--- | :--- | :--- |
| **Classic Friedman GBDT**| Exact Pre-sorted Split | $\mathcal{O}(D \cdot N \log N)$ | Slow for $N > 100,000$ |
| **XGBoost (Histogram)** | Quantile Sketching Bins | $\mathcal{O}(D \cdot N)$ | Very Fast |
| **LightGBM (GOSS)** | Histogram Binning (256 bins) + Gradient Downsampling | $\mathcal{O}(D \cdot N_{\text{sub}})$ | Ultra-Fast ($\sim 10\times$ faster) |
| **CatBoost** | Oblivious (Symmetric) Trees | $\mathcal{O}(D \cdot N)$ | Fast + Best on Categoricals |

---

## 5. Inductive Bias & Core Assumptions

1. **Additive Hypothesis Space**: Assumes the target function can be represented as an additive sum of bounded base learners: $F(\mathbf{x}) = \sum \eta h_m(\mathbf{x})$.
2. **Local Feature Interactions**: The tree depth (typically 3 to 6) bounds the order of feature interactions: a tree of depth $d$ can model interactions of at most $d$ variables.
3. **Sequential Dependency**: Tree $m$ strictly depends on Tree $m-1$. Training cannot be parallelized across trees (unlike Random Forest); parallelization must occur across features and samples within a tree.

---

## 6. From-Scratch NumPy Implementation

```python
"""
Pure NumPy Implementation of Gradient Boosting Regressor (L2 Loss).
Tested on Python 3.11, NumPy 2.x.
"""

from typing import List, Optional
import numpy as np


class SimpleStump:
    """A shallow decision tree stump for residual fitting."""
    def __init__(self, max_depth: int = 3):
        self.max_depth = max_depth
        self.feat: Optional[int] = None
        self.thresh: Optional[float] = None
        self.left_val: Optional[float] = None
        self.right_val: Optional[float] = None

    def fit(self, X: np.ndarray, residuals: np.ndarray):
        N, D = X.shape
        best_loss = float("inf")

        for feat in range(D):
            thresholds = np.unique(X[:, feat])
            for thresh in thresholds:
                left_mask = X[:, feat] <= thresh
                right_mask = ~left_mask

                if np.sum(left_mask) == 0 or np.sum(right_mask) == 0:
                    continue

                val_l = np.mean(residuals[left_mask])
                val_r = np.mean(residuals[right_mask])

                loss = np.sum((residuals[left_mask] - val_l) ** 2) + np.sum((residuals[right_mask] - val_r) ** 2)

                if loss < best_loss:
                    best_loss = loss
                    self.feat = feat
                    self.thresh = thresh
                    self.left_val = float(val_l)
                    self.right_val = float(val_r)

        return self

    def predict(self, X: np.ndarray) -> np.ndarray:
        left_mask = X[:, self.feat] <= self.thresh
        preds = np.zeros(len(X))
        preds[left_mask] = self.left_val
        preds[~left_mask] = self.right_val
        return preds


class PureNumPyGradientBoostingRegressor:
    """Gradient Boosting Regressor for L2 Squared Error Loss."""

    def __init__(self, n_estimators: int = 50, learning_rate: float = 0.1, max_depth: int = 3):
        self.n_estimators = n_estimators
        self.learning_rate = learning_rate
        self.max_depth = max_depth
        self.base_pred_: float = 0.0
        self.trees: List[SimpleStump] = []

    def fit(self, X: np.ndarray, y: np.ndarray):
        # 1. Initialize with constant mean: F_0 = mean(y)
        self.base_pred_ = float(np.mean(y))
        current_preds = np.full_like(y, fill_value=self.base_pred_, dtype=float)
        self.trees = []

        for m in range(self.n_estimators):
            # 2. Compute pseudo-residuals: r = y - F_{m-1}
            residuals = y - current_preds

            # 3. Fit base learner to pseudo-residuals
            tree = SimpleStump(max_depth=self.max_depth)
            tree.fit(X, residuals)
            self.trees.append(tree)

            # 4. Update predictions with shrinkage: F_m = F_{m-1} + lr * h_m
            update = tree.predict(X)
            current_preds += self.learning_rate * update

        return self

    def predict(self, X: np.ndarray) -> np.ndarray:
        preds = np.full(len(X), fill_value=self.base_pred_, dtype=float)
        for tree in self.trees:
            preds += self.learning_rate * tree.predict(X)
        return preds


# Verification Run
if __name__ == "__main__":
    np.random.seed(42)
    N = 150
    X = np.linspace(-3, 3, N).reshape(-1, 1)
    y = np.sin(X).flatten() + np.random.normal(0, 0.1, size=N)

    gbr = PureNumPyGradientBoostingRegressor(n_estimators=40, learning_rate=0.1)
    gbr.fit(X, y)

    y_pred = gbr.predict(X)
    r2 = 1.0 - (np.sum((y - y_pred) ** 2) / np.sum((y - np.mean(y)) ** 2))
    print("--- Pure NumPy Gradient Boosting Regressor ---")
    print(f"R^2 Score on Non-Linear Sine Wave: {r2:.4f}")
    assert r2 > 0.85
    print("[SUCCESS] Pure NumPy Gradient Boosting Regressor verified.")
```

---

## 7. Production-Grade Scikit-Learn Implementation

```python
"""
Production Pipeline: HistGradientBoostingRegressor (LightGBM-style).
Native support for missing values, categorical features, and early stopping.
"""

import numpy as np
import pandas as pd
from sklearn.ensemble import HistGradientBoostingRegressor
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_squared_error, r2_score


def run_hist_gbdt_production():
    np.random.seed(42)
    N = 1000
    X = np.random.randn(N, 6)
    # y with complex interactions and non-linearity
    y = 2.0 * X[:, 0] * X[:, 1] + np.sin(X[:, 2]) + (X[:, 3] > 0) * 3.0 + np.random.normal(0, 0.2, size=N)

    # Inject missing values (HistGradientBoosting handles NaNs natively)
    X[::20, 0] = np.nan

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    model = HistGradientBoostingRegressor(
        loss="squared_error",
        learning_rate=0.08,
        max_iter=300,
        max_leaf_nodes=31,
        early_stopping=True,   # Halts when validation loss plateaus
        n_iter_no_change=10,
        random_state=42
    )

    model.fit(X_train, y_train)

    print("--- HistGradientBoosting Production Performance ---")
    print(f"Iterations Completed (Early Stopping): {model.n_iter_}")

    y_pred = model.predict(X_test)
    test_r2 = r2_score(y_test, y_pred)
    print(f"Test R^2: {test_r2:.4f}")
    assert test_r2 > 0.90


if __name__ == "__main__":
    run_hist_gbdt_production()
```

---

## 8. Model Persistence & Safe Serialization

```python
import joblib
import tempfile
import os

def persist_gbdt(model):
    with tempfile.TemporaryDirectory() as tmp_dir:
        path = os.path.join(tmp_dir, "gbdt_model.joblib")
        joblib.dump(model, path)
        loaded = joblib.load(path)
        assert loaded.n_iter_ == model.n_iter_
        print("[SUCCESS] GBDT model serialized safely.")
```

---

## 9. Hyperparameter Tuning & Optimization

| Hyperparameter | Search Space | Default | Practical Tuning Strategy |
| :--- | :--- | :--- | :--- |
| `learning_rate` ($\eta$) | `[0.01, 0.05, 0.1]` | `0.1` | Halve `learning_rate` and double `max_iter` for higher accuracy. |
| `max_iter` ($M$) | `[100, 500, 1000]` | `100` | Control via `early_stopping=True`. |
| `max_leaf_nodes` | `[15, 31, 63, 127]` | `31` | Controls tree capacity; $31 \approx$ depth 5. |
| `min_samples_leaf` | `[10, 20, 50]` | `20` | Guards against fitting leaf-level noise. |
| `l2_regularization` | `[0.0, 1e-3, 1.0, 10.0]`| `0.0` | $L_2$ leaf weight penalty (XGBoost $\lambda$). |

---

## 10. Diagnostic Evaluations & Learning Curves

```
+-----------------------------------------------------------------------------------+
|                        EARLY STOPPING DIAGNOSTIC PROTOCOL                         |
+-----------------------------------------------------------------------------------+
| Training Loss: Monotonically decreases toward 0 as M -> inf.                      |
| Validation Loss: Decreases, hits global minimum M*, then rises (Overfitting).     |
| Rule: Stop training when validation loss fails to improve for 10 iterations.     |
+-----------------------------------------------------------------------------------+
```

---

## 11. Calibration, Uncertainty & Prediction Intervals

To generate prediction intervals with GBDT, fit **dual Quantile Regressors**:
```python
# Lower 5% bound
lower_model = HistGradientBoostingRegressor(loss="quantile", quantile=0.05).fit(X, y)
# Median prediction
median_model = HistGradientBoostingRegressor(loss="quantile", quantile=0.50).fit(X, y)
# Upper 95% bound
upper_model = HistGradientBoostingRegressor(loss="quantile", quantile=0.95).fit(X, y)
```
The interval $[\hat{y}_{0.05}(\mathbf{x}), \hat{y}_{0.95}(\mathbf{x})]$ provides exact 90% non-parametric coverage.

---

## 12. Edge Cases, Failure Modes & Remediation

| Failure Mode | Root Cause | Observable Symptom | Engineering Remediation |
| :--- | :--- | :--- | :--- |
| **Overfitting via Tree Count** | Setting `n_estimators=5000` without early stopping | Training $R^2 = 0.999$, Test $R^2 = 0.40$ | Enable `early_stopping=True` with `n_iter_no_change=10`. |
| **Learning Rate Too High** | Setting $\eta = 0.5$ | Gradient descent oscillates wildly; fails to converge | Lower learning rate to $\eta \le 0.05$. |
| **Outlier Sensitivity in L2** | Severe outliers in target $y$ | Residuals explode, pulling all subsequent trees | Switch to `loss='huber'` or `loss='absolute_error'`. |
| **Extrapolation Failure** | Querying outside training domain | Flatlines at edge tree leaf value | Combine with linear base model (Residual Stacking). |
| **Unbounded Tree Depth** | Setting `max_depth > 12` | Memory explosion; slow training; overfitting | Limit `max_depth <= 6` or `max_leaf_nodes <= 31`. |
| **Target Leakage Amplification** | Leaked feature present in data | First 3 trees capture 100% of importance | Audit feature importance and temporal ordering. |

---

## 13. Comparative Analysis: GBDT Ecosystem

| Framework | Histogram Algorithm | Tree Architecture | Categorical Handling | GPU Speed |
| :--- | :--- | :--- | :--- | :--- |
| **Scikit-Learn (HistGBDT)** | 256-bin Integer Histograms | Depth-wise / Leaf-wise | Native (Integer codes) | No (CPU only) |
| **XGBoost** | Approximate Quantile Sketch | Depth-wise | Partition-based | CUDA native |
| **LightGBM** | GOSS + Histogram | Leaf-wise (Best-First) | Native sorting | OpenCL / CUDA |
| **CatBoost** | Quantile Bins | Oblivious (Symmetric) | Ordered Target Statistics| Ultra-Fast GPU |

---

## 14. Production Engineering & Serving Considerations

1. **Compilation via Treelite**:
   Compile 500 gradient boosted trees into native C/C++ shared libraries (`.so` / `.dll`), evaluating predictions in $< 10\text{ microseconds}$ without Python runtime overhead.
2. **Serving Latency vs Accuracy Slicing**:
   In low-latency microservices, evaluate early exits: if the first 50 trees predict an outcome with high confidence, skip the remaining 450 trees.

---

## 15. Interpretability & Feature Attribution

- **TreeSHAP**: Evaluates exact local feature attributions in $\mathcal{O}(M \cdot L \cdot D^2)$ time.
- **Partial Dependence Plots (PDP)**: Visualizes the marginal effect of feature $x_j$ on $\hat{y}$ while averaging out all other features.

---

## 16. Hardware & Distributed Scaling

- **Histogram Parallelism**: Data is partitioned by rows across CPU cores. Each core computes a local $256$-bin histogram; cores reduce histograms via `AllReduce` in $\mathcal{O}(256 \cdot D)$ time.
- **GPU Acceleration**: In XGBoost, set `tree_method='hist'`, `device='cuda'` to accelerate histogram construction on GPU VRAM.

---

## 17. Verification & Diagnostics Test Harness

```python
import pytest
import numpy as np
from sklearn.ensemble import HistGradientBoostingRegressor

def test_gbdt_residual_reduction():
    # Adding trees must monotonically decrease training loss
    X = np.random.randn(100, 2)
    y = 2.0 * X[:, 0] + np.random.normal(0, 0.1, size=100)

    m1 = HistGradientBoostingRegressor(max_iter=5, random_state=42).fit(X, y)
    m2 = HistGradientBoostingRegressor(max_iter=30, random_state=42).fit(X, y)

    mse1 = np.mean((y - m1.predict(X)) ** 2)
    mse2 = np.mean((y - m2.predict(X)) ** 2)
    assert mse2 < mse1, "More boosting iterations should reduce training error."
```

---

## 18. Cross-Track Connections

- **05-ai/machine-learning/01-math-foundations/06-optimisation-and-gradient-descent.md**: Functional gradient descent and Taylor series.
- **05-ai/machine-learning/03-supervised-regression/07-random-forest-regression.md**: Bagging vs Boosting comparison.
- **05-ai/machine-learning/04-supervised-classification/07-gradient-boosting.md**: GBDT applied to binary and multi-class cross-entropy.

---

## 19. Real-World Case Study: Electricity Grid Day-Ahead Load Forecasting

A regional power transmission operator forecasted hourly megawatt (MW) load demand 24 hours in advance.
- **Challenge**: Demand exhibited non-linear interactions between temperature, humidity, day-of-week, and industrial operating schedules. Extreme heatwaves created severe demand spikes that standard linear models underpredicted.
- **Solution**:
  1. Deployed `HistGradientBoostingRegressor(loss='huber', learning_rate=0.04, max_iter=500)` with early stopping.
  2. Huber loss resisted sensor outlier glitches while accurately fitting non-linear summer air conditioning spikes.
  3. Forecast Mean Absolute Percentage Error (MAPE) dropped from 8.4% to 2.1%, saving the utility \$22M in grid reserve peaking plant fuel costs.

---

## 20. Technical Interview Questions & Solutions

### Question 1: Derive why the pseudo-residuals in Gradient Boosting with Squared Error ($L_2$) loss equal standard residuals $y_i - F(\mathbf{x}_i)$.
**Answer**:
The Squared Error loss function is:
$$L(y_i, F(\mathbf{x}_i)) = \frac{1}{2} (y_i - F(\mathbf{x}_i))^2$$
Taking the partial derivative with respect to the prediction $F(\mathbf{x}_i)$:
$$\frac{\partial L}{\partial F(\mathbf{x}_i)} = \frac{1}{2} \cdot 2(y_i - F(\mathbf{x}_i)) \cdot (-1) = -(y_i - F(\mathbf{x}_i))$$
The pseudo-residual is the negative gradient:
$$r_{im} = -\left[ \frac{\partial L}{\partial F(\mathbf{x}_i)} \right] = -\left( -(y_i - F(\mathbf{x}_i)) \right) = y_i - F(\mathbf{x}_i)$$
Thus, under $L_2$ loss, gradient descent in function space is mathematically identical to recursively fitting trees to the raw residual errors.

### Question 2: What are pseudo-residuals under Absolute Error ($L_1$) loss, and why does $L_1$ boosting predict the median?
**Answer**:
The $L_1$ loss function is $L(y_i, F(\mathbf{x}_i)) = |y_i - F(\mathbf{x}_i)|$.
The derivative with respect to $F$ is $-\text{sign}(y_i - F(\mathbf{x}_i))$.
The pseudo-residual is:
$$r_{im} = -\left[ -\text{sign}(y_i - F(\mathbf{x}_i)) \right] = \text{sign}(y_i - F(\mathbf{x}_i)) \in \{-1, +1\}$$
Trees fit the direction of the error, not its magnitude.
In the leaf line search:
$$\gamma_{jm} = \arg\min_\gamma \sum_{\mathbf{x}_i \in R_{jm}} |y_i - (F_{m-1}(\mathbf{x}_i) + \gamma)|$$
The value $\gamma$ that minimizes the sum of absolute deviations is the **median** of the residuals $y_i - F_{m-1}(\mathbf{x}_i)$.

### Question 3: Why is shrinkage ($\eta < 1$) necessary in Gradient Boosting?
**Answer**:
Friedman demonstrated that without shrinkage ($\eta = 1.0$), the first few trees greedily fit large steps, quickly getting trapped in poor local optima and overfitting training noise.
Shrinkage scales each step by $\eta \in [0.01, 0.1]$:
$$F_m(\mathbf{x}) = F_{m-1}(\mathbf{x}) + \eta \cdot h_m(\mathbf{x})$$
This forces the ensemble to take small, conservative steps in function space, allowing subsequent trees to correct errors from diverse angles and significantly improving generalization.

### Question 4: How does LightGBM's Gradient-based One-Side Sampling (GOSS) work?
**Answer**:
GOSS recognizes that samples with large gradients (large residuals) contribute more to information gain.
To reduce sample size from $N$ to a smaller subset:
1. Sort samples descending by absolute gradient $|g_i|$.
2. Keep the top $a \times 100\%$ samples with the largest gradients.
3. Randomly sample a fraction $b \times 100\%$ from the remaining small-gradient samples.
4. Multiply the small-gradient samples by a weight $\frac{1 - a}{b}$ to preserve the original data distribution without altering the expectation of the gradient histogram.

### Question 5: Why does XGBoost require second-order derivatives (Hessians)?
**Answer**:
Second-order Taylor expansion provides curvature information about the loss surface. The optimal leaf weight:
$$w_j^* = -\frac{\sum g_i}{\sum h_i + \lambda}$$
automatically scales the step size by the curvature $\sum h_i$: in regions where the loss surface is steep and flat ($h_i \approx 0$), it takes larger steps; in regions with high curvature ($h_i \gg 0$), it takes cautious small steps. This eliminates the need for expensive manual line searches.

### Question 6: What is the primary difference between Bagging and Boosting regarding the Bias-Variance Trade-off?
**Answer**:
- **Bagging (Random Forest)**: Combines **high-variance, low-bias** estimators (deep unpruned trees) in parallel to **reduce variance** without changing bias.
- **Boosting (GBDT)**: Combines **high-bias, low-variance** estimators (shallow weak trees) sequentially to **reduce bias** while controlling variance through shrinkage and early stopping.

### Question 7: How do modern GBDTs handle missing values natively?
**Answer**:
During split evaluation, the algorithm evaluates two candidate directions for samples with missing values:
1. Route all `NaN` samples to the Left child node.
2. Route all `NaN` samples to the Right child node.
The algorithm calculates the split gain for both options and permanently records the winning direction as the **default path** for that split. During inference, if a sample has a `NaN`, it follows the default path.

### Question 8: Why are oblivious (symmetric) trees used in CatBoost?
**Answer**:
In standard trees, left and right branches evaluate different features.
In **Oblivious Trees**, every node at the same tree depth evaluates the **exact same feature and threshold**.
- Benefit 1: An oblivious tree of depth $d$ has exactly $2^d$ leaves addressed by a $d$-bit integer index.
- Benefit 2: Inference reduces to a single bitmask SIMD lookup: $\text{index} = \sum_{k=0}^{d-1} \mathbb{I}(x_{j_k} > s_k) \cdot 2^k$, executing in nanoseconds on CPU/GPU.

---

## 21. Guided Practice & Hands-On Exercises

### Exercise 1: Step-by-Step Residual Tracer
- Write a Python script that fits a 3-tree Gradient Boosting Regressor by hand. Print the initial prediction $F_0$, the pseudo-residuals after tree 1, and the updated predictions $F_1$, verifying the manual steps against scikit-learn.

### Exercise 2: Huber Loss vs L2 Outlier Shootout
- Train two GBDT models on a dataset containing 5 extreme target spikes: Model A with `loss='squared_error'` and Model B with `loss='huber'`. Show that Model B's predictions on clean test samples are unaffected by the training outliers.

### Exercise 3: Early Stopping Patience Study
- Train `HistGradientBoostingRegressor` with `early_stopping=True` across learning rates $\eta \in [0.01, 0.05, 0.1, 0.3]$. Plot the optimal stopping iteration $M^*$ vs learning rate.

### Exercise 4: TreeSHAP Feature Attribution
- Train a GBDT model on the California Housing dataset. Use the `shap` library to compute TreeSHAP values and plot the global beeswarm summary.

### Exercise 5: Quantile Fan Chart Visualizer
- Train three GBDT models for quantiles $\tau \in [0.1, 0.5, 0.9]$ on heteroscedastic noise data. Plot the predicted median line and the expanding 80% prediction interval fan.

### Exercise 6: Treelite C-Code Compiler Benchmark
- Train a 100-tree GBDT model, compile it to C code using `treelite`, and benchmark inference latency against native Python `model.predict()`.
