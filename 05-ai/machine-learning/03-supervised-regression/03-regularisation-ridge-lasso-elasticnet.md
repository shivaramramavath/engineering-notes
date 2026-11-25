---
title: "Regularised Linear Models: Ridge, Lasso, and ElasticNet"
description: "Comprehensive mathematical derivation, L1 and L2 penalty geometry, Coordinate Descent optimization, SVD shrinkage, and production pipelines for regularised regression."
track: "machine-learning"
module: "03-supervised-regression"
topic: "Regularisation: Ridge, Lasso, and ElasticNet"
order: 3
format: "algorithm"
level: "intermediate"
tags: ["regularization", "ridge-regression", "lasso", "elasticnet", "coordinate-descent", "l1-penalty", "l2-penalty", "sparsity"]
prerequisites: [
  "05-ai/machine-learning/01-math-foundations/06-optimisation-and-gradient-descent.md",
  "05-ai/machine-learning/01-math-foundations/07-bias-variance-tradeoff.md",
  "05-ai/machine-learning/03-supervised-regression/01-linear-regression.md"
]
key_takeaways:
  - "Ridge regression (L2) shrinks coefficients continuously via (X^T X + alpha I)^{-1}, stabilizing ill-conditioned matrices without setting weights to zero."
  - "Lasso regression (L1) induces exact parameter sparsity through non-smooth corners of the L1 ball, performing simultaneous estimation and feature selection."
  - "Lasso requires iterative Coordinate Descent with the soft-thresholding operator S(z, gamma) because the L1 norm is non-differentiable at zero."
  - "ElasticNet combines L1 and L2 penalties, resolving Lasso's arbitrary selection among collinear features via the grouping effect."
---

# Regularised Linear Models: Ridge, Lasso, and ElasticNet

## 1. Algorithm Card

```
+-----------------------------------------------------------------------------------+
|                              ALGORITHM SPECIFICATION                              |
+-----------------------------------------------------------------------------------+
| Paradigm:         Supervised Learning (Regularised Linear Models)                 |
| Family:           Penalized Generalized Linear Models                             |
| Input Space:      X in R^{N x D} (MANDATORY: Features must be standardized)       |
| Output Space:     y in R^N (Continuous Scalars)                                   |
| Objectives:       Ridge:      J(w) = (1/2N) * ||Xw - y||_2^2 + (alpha/2) * ||w||_2^2|
|                   Lasso:      J(w) = (1/2N) * ||Xw - y||_2^2 + alpha * ||w||_1    |
|                   ElasticNet: J(w) = MSE + alpha*l1_ratio*||w||_1 +               |
|                                      0.5*alpha*(1-l1_ratio)*||w||_2^2             |
| Optimization:     Ridge: Closed-form Inversion or SVD                             |
|                   Lasso & ElasticNet: Cyclical Coordinate Descent                 |
| Time Complexity:  Ridge: O(N D^2 + D^3) (Normal Eq) or O(T N D) (GD)              |
|                   Lasso: O(T * N * D) Coordinate Descent                          |
| Space Complexity: O(D) model weights                                              |
| Key Assumptions:  Linearity, standardized features; intercepts unpenalized        |
| Invariances:      Strictly NOT scale-invariant (StandardScaler is mandatory)     |
+-----------------------------------------------------------------------------------+
```

---

## 2. Architecture & Intuition

Ordinary Least Squares minimizes training error without constraint. When features are collinear or $D \gg N$, the Gram matrix $X^T X$ becomes ill-conditioned, and weights $\mathbf{w}$ oscillate toward massive positive and negative values.
**Regularization** trades a small amount of bias to achieve a dramatic reduction in variance by adding a penalty on weight magnitude to the loss function.

### Visual Mental Model: The Geometry of Penalties

Consider the constrained optimization problem in 2D parameter space $(w_1, w_2)$:
$$\min_{\mathbf{w}} \frac{1}{2N} \|\mathbf{y} - X\mathbf{w}\|_2^2 \quad \text{subject to} \quad \|\mathbf{w}\|_p \le t$$

```
           L2 Penalty (Ridge)                          L1 Penalty (Lasso)
             w2                                          w2
              ^                                           ^
              |   Elliptical Loss                         |   Elliptical Loss
              |   Contours J(w)                           |   Contours J(w)
          +---+---+                                       |      / \
         /    |    \                                     |     /   \
        |-----+-----|--------> w1                        +----+-----+----> w1
         \    |    /                                     |     \   /
          +---+---+                                       |      \ /
              |                                           |
    Smooth circular boundary:                   Sharp diamond corner on axis:
    Tangency occurs at w1 != 0, w2 != 0.        Tangency occurs AT vertex: w2 = 0!
    Shrinks weights toward zero.                Forces sparse exact zero weights!
```

---

## 3. Theoretical Foundations & Mathematical Derivation

### 3.1 Ridge Regression ($L_2$ Tikhonov Regularization)

**Objective Function**:
$$J_{\text{Ridge}}(\mathbf{w}) = \frac{1}{2} \|\mathbf{y} - X \mathbf{w}\|_2^2 + \frac{\alpha}{2} \|\mathbf{w}\|_2^2 = \frac{1}{2} (\mathbf{y} - X \mathbf{w})^T (\mathbf{y} - X \mathbf{w}) + \frac{\alpha}{2} \mathbf{w}^T \mathbf{w}$$

Taking the vector derivative with respect to $\mathbf{w}$:
$$\nabla_{\mathbf{w}} J_{\text{Ridge}}(\mathbf{w}) = -X^T \mathbf{y} + X^T X \mathbf{w} + \alpha \mathbf{w} = \mathbf{0}$$
$$(X^T X + \alpha \mathbf{I}) \mathbf{w} = X^T \mathbf{y}$$

Because $\alpha > 0$ and $X^T X$ is positive semi-definite, $(X^T X + \alpha \mathbf{I})$ has strictly positive eigenvalues ($\lambda_i + \alpha > 0$). It is **always invertible**, even if $N < D$ or if features are perfectly collinear:

$$\mathbf{w}_{\text{Ridge}}^* = (X^T X + \alpha \mathbf{I})^{-1} X^T \mathbf{y}$$

#### SVD Shrinkage Analysis
Let the SVD of $X$ be $X = U \Sigma V^T$ where $\Sigma = \text{diag}(\sigma_1, \dots, \sigma_D)$.
The OLS estimator projects coefficients as $V \Sigma^{-1} U^T \mathbf{y}$.
The Ridge estimator applies a continuous shrinkage factor:
$$\mathbf{w}_{\text{Ridge}}^* = V \left( \Sigma^2 + \alpha \mathbf{I} \right)^{-1} \Sigma U^T \mathbf{y} = \sum_{j=1}^D \frac{\sigma_j^2}{\sigma_j^2 + \alpha} \frac{\mathbf{u}_j^T \mathbf{y}}{\sigma_j} \mathbf{v}_j$$
- For directions with large variance ($\sigma_j^2 \gg \alpha$): shrinkage factor $\frac{\sigma_j^2}{\sigma_j^2 + \alpha} \approx 1$ (weights preserved).
- For directions with small variance ($\sigma_j^2 \ll \alpha$): shrinkage factor $\to 0$ (noise suppressed).

### 3.2 Lasso Regression ($L_1$ Penalty) and Coordinate Descent

**Objective Function**:
$$J_{\text{Lasso}}(\mathbf{w}) = \frac{1}{2N} \|\mathbf{y} - X \mathbf{w}\|_2^2 + \alpha \sum_{j=1}^D |w_j|$$

Because the absolute value function $|w_j|$ has a non-differentiable cusp at $w_j = 0$, there is no closed-form matrix solution. We solve it via **Cyclical Coordinate Descent**.

Let $r_i^{(-j)} = y_i - \sum_{k \neq j} x_{ik} w_k$ be the partial residual excluding feature $j$.
The objective with respect to single coordinate $w_j$ is:
$$f(w_j) = \frac{1}{2N} \sum_{i=1}^N (r_i^{(-j)} - x_{ij} w_j)^2 + \alpha |w_j|$$

Assuming features are standardized ($\frac{1}{N} \sum x_{ij}^2 = 1$), define the correlation rho:
$$\rho_j = \frac{1}{N} \sum_{i=1}^N x_{ij} r_i^{(-j)} = \frac{1}{N} \mathbf{x}_j^T (\mathbf{y} - X_{:, -j} \mathbf{w}_{-j})$$

Using subgradient calculus ($\partial |w_j| = \text{sign}(w_j)$ for $w_j \neq 0$, and $[-1, 1]$ at $w_j = 0$), the coordinate-wise update is the **Soft-Thresholding Operator**:

$$w_j^* = \mathcal{S}(\rho_j, \alpha) = \text{sign}(\rho_j) \max(0, |\rho_j| - \alpha) = \begin{cases}
\rho_j - \alpha & \text{if } \rho_j > \alpha \\
0 & \text{if } -\alpha \le \rho_j \le \alpha \\
\rho_j + \alpha & \text{if } \rho_j < -\alpha
\end{cases}$$

If the correlation $|\rho_j|$ between feature $j$ and the partial residual does not exceed $\alpha$, the coefficient is set **identically to zero**.

### 3.3 ElasticNet: Combining $L_1$ and $L_2$

ElasticNet minimizes:
$$J_{\text{ElasticNet}}(\mathbf{w}) = \frac{1}{2N} \|\mathbf{y} - X \mathbf{w}\|_2^2 + \alpha \cdot \left[ \rho \|\mathbf{w}\|_1 + \frac{1 - \rho}{2} \|\mathbf{w}\|_2^2 \right]$$
where $\rho \in [0, 1]$ is the `l1_ratio`.
- If $\rho = 1$: Pure Lasso.
- If $\rho = 0$: Pure Ridge.
- If $0 < \rho < 1$: ElasticNet retains the sparsity of Lasso while inheriting the **grouping property** of Ridge (correlated features enter the model together).

---

## 4. Computational Complexity Analysis

| Algorithm | Training Complexity | Inference Complexity | Memory Footprint |
| :--- | :--- | :--- | :--- |
| **Ridge (Closed-Form)** | $\mathcal{O}(N D^2 + D^3)$ | $\mathcal{O}(D)$ | $\mathcal{O}(D^2)$ Gram matrix |
| **Ridge (Gradient Descent)** | $\mathcal{O}(T \cdot N \cdot D)$ | $\mathcal{O}(D)$ | $\mathcal{O}(D)$ |
| **Lasso (Coordinate Descent)** | $\mathcal{O}(T \cdot N \cdot D)$ | $\mathcal{O}(k)$ ($k \le D$ non-zero weights) | $\mathcal{O}(N \cdot D)$ |
| **ElasticNet (Coord Descent)** | $\mathcal{O}(T \cdot N \cdot D)$ | $\mathcal{O}(k)$ | $\mathcal{O}(N \cdot D)$ |

---

## 5. Inductive Bias & Core Assumptions

1. **Feature Scale Invariance Violation**: Regularizers penalize $w_j^2$ or $|w_j|$. If feature $x_1$ is measured in millimeters and $x_2$ in kilometers, $w_2$ must be $10^6 \times$ larger to have equal effect, receiving an unfair massive penalty. **Features must be standardized ($\mu=0, \sigma=1$) prior to regularization.**
2. **Unpenalized Intercept**: The intercept $b$ represents the baseline mean of $y$ when all $\mathbf{x} = \mathbf{0}$. Penalizing $b$ introduces severe bias; standard implementations center $y$ ($\tilde{y} = y - \bar{y}$) and set $b = \bar{y}$.
3. **Sparsity Prior**: Lasso assumes the true ground-truth parameter vector $\mathbf{w}^*$ is sparse (Laplace prior), while Ridge assumes weights are drawn from a zero-mean isotropic Gaussian prior.

---

## 6. From-Scratch NumPy Implementation

Below is a complete, vectorized Python implementation of Ridge (closed-form) and Lasso (Cyclical Coordinate Descent with the soft-thresholding operator).

```python
"""
Pure NumPy Implementation of Ridge and Lasso Regression.
Tested on Python 3.11, NumPy 2.x.
"""

from typing import Tuple
import numpy as np


def soft_threshold(rho: float, alpha: float) -> float:
    """Soft-thresholding operator S(rho, alpha) for Lasso Coordinate Descent."""
    if rho > alpha:
        return float(rho - alpha)
    elif rho < -alpha:
        return float(rho + alpha)
    else:
        return 0.0


class PureNumPyRidge:
    """Ridge Regression using analytical solution (X^T X + alpha I)^(-1) X^T y."""

    def __init__(self, alpha: float = 1.0):
        self.alpha = alpha
        self.coef_: np.ndarray = np.array([])
        self.intercept_: float = 0.0

    def fit(self, X: np.ndarray, y: np.ndarray):
        X_mean = np.mean(X, axis=0)
        y_mean = np.mean(y)
        X_centered = X - X_mean
        y_centered = y - y_mean

        D = X.shape[1]
        I = np.eye(D)

        # Analytical solution on centered data
        self.coef_ = np.linalg.solve(X_centered.T @ X_centered + self.alpha * I, X_centered.T @ y_centered)
        self.intercept_ = float(y_mean - X_mean @ self.coef_)
        return self

    def predict(self, X: np.ndarray) -> np.ndarray:
        return X @ self.coef_ + self.intercept_


class PureNumPyLasso:
    """Lasso Regression using Cyclical Coordinate Descent."""

    def __init__(self, alpha: float = 0.1, max_iter: int = 1000, tol: float = 1e-4):
        self.alpha = alpha
        self.max_iter = max_iter
        self.tol = tol
        self.coef_: np.ndarray = np.array([])
        self.intercept_: float = 0.0

    def fit(self, X: np.ndarray, y: np.ndarray):
        N, D = X.shape
        X_mean = np.mean(X, axis=0)
        y_mean = np.mean(y)
        X_c = X - X_mean
        y_c = y - y_mean

        # Precompute norms: sum(x_j^2)
        norm_sq = np.sum(X_c ** 2, axis=0)

        # Initialize coefficients
        w = np.zeros(D)

        for iteration in range(self.max_iter):
            w_old = w.copy()
            for j in range(D):
                # Compute partial residuals: y_c - X_c w + X_c[:, j] w[j]
                residuals = y_c - (X_c @ w) + X_c[:, j] * w[j]
                # Compute correlation rho_j
                rho_j = np.dot(X_c[:, j], residuals)

                # Soft threshold update normalized by feature variance
                if norm_sq[j] > 0:
                    w[j] = soft_threshold(rho_j, self.alpha * N) / norm_sq[j]
                else:
                    w[j] = 0.0

            if np.max(np.abs(w - w_old)) < self.tol:
                break

        self.coef_ = w
        self.intercept_ = float(y_mean - X_mean @ self.coef_)
        return self

    def predict(self, X: np.ndarray) -> np.ndarray:
        return X @ self.coef_ + self.intercept_


# Verification of NumPy Implementation
if __name__ == "__main__":
    np.random.seed(42)
    N, D = 150, 5

    # Sparse true weights: only first 2 features are non-zero!
    true_w = np.array([4.0, -2.5, 0.0, 0.0, 0.0])
    X_raw = np.random.randn(N, D)
    y_raw = X_raw @ true_w + 1.5 + np.random.normal(0, 0.2, size=N)

    # 1. Test Pure NumPy Ridge
    ridge = PureNumPyRidge(alpha=1.0)
    ridge.fit(X_raw, y_raw)
    print("--- Pure NumPy Ridge Coefficients ---")
    print(ridge.coef_.round(3))
    # Ridge shrinks weights but keeps all non-zero
    assert np.all(np.abs(ridge.coef_) > 0.01)

    # 2. Test Pure NumPy Lasso
    lasso = PureNumPyLasso(alpha=0.1, max_iter=1000)
    lasso.fit(X_raw, y_raw)
    print("\n--- Pure NumPy Lasso Coefficients (Sparsity Check) ---")
    print(lasso.coef_.round(3))
    # Features 2, 3, 4 should be exactly 0.0
    assert np.isclose(lasso.coef_[2], 0.0)
    assert np.isclose(lasso.coef_[3], 0.0)
    assert np.isclose(lasso.coef_[4], 0.0)
    assert lasso.coef_[0] > 3.0 and lasso.coef_[1] < -2.0

    print("\n[SUCCESS] Pure NumPy Ridge & Lasso algorithms verified.")
```

---

## 7. Production-Grade Scikit-Learn Implementation

```python
"""
Production Pipeline: ElasticNetCV with Cross-Validated Alpha and L1 Ratio.
Includes StandardScaler and ColumnTransformer.
"""

import numpy as np
import pandas as pd
from sklearn.datasets import make_regression
from sklearn.model_selection import train_test_split
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import ElasticNetCV
from sklearn.metrics import mean_squared_error, r2_score


def run_elasticnet_production():
    np.random.seed(42)
    # 20 features, only 5 informative
    X, y = make_regression(n_samples=500, n_features=20, n_informative=5, noise=1.0, random_state=42)

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    pipeline = Pipeline([
        ("scaler", StandardScaler()), # CRITICAL FOR REGULARIZATION
        ("elastic_net", ElasticNetCV(
            l1_ratio=[0.1, 0.5, 0.7, 0.9, 0.99],
            alphas=np.logspace(-3, 2, 30),
            cv=5,
            random_state=42
        ))
    ])

    pipeline.fit(X_train, y_train)

    best_model = pipeline.named_steps["elastic_net"]
    print("--- ElasticNetCV Optimal Hyperparameters ---")
    print(f"Optimal Alpha:    {best_model.alpha_:.4f}")
    print(f"Optimal L1 Ratio: {best_model.l1_ratio_:.2f}")

    non_zero_count = np.sum(best_model.coef_ != 0.0)
    print(f"Active Features Selected: {non_zero_count} / 20")

    y_pred = pipeline.predict(X_test)
    print(f"Test R^2: {r2_score(y_test, y_pred):.4f}")
    assert r2_score(y_test, y_pred) > 0.95


if __name__ == "__main__":
    run_elasticnet_production()
```

---

## 8. Model Persistence & Safe Serialization

```python
import joblib
import tempfile
import os

def serialize_regularized_model(pipeline):
    with tempfile.TemporaryDirectory() as tmp_dir:
        path = os.path.join(tmp_dir, "elasticnet_pipeline.joblib")
        joblib.dump(pipeline, path)
        loaded = joblib.load(path)
        assert np.array_equal(
            pipeline.named_steps["elastic_net"].coef_,
            loaded.named_steps["elastic_net"].coef_
        )
        print("[SUCCESS] ElasticNet model serialization verified.")
```

---

## 9. Hyperparameter Tuning & Optimization

| Hyperparameter | Model | Search Space | Practical Rule |
| :--- | :--- | :--- | :--- | :--- |
| `alpha` | Ridge, Lasso, ElasticNet | `np.logspace(-4, 4, 50)` | Larger $\alpha$ increases bias, reduces variance. |
| `l1_ratio` | ElasticNet | `[0.1, 0.5, 0.7, 0.9, 0.99]` | Balance between grouping ($L_2$) and sparsity ($L_1$). |
| `max_iter` | Lasso, ElasticNet | `[1000, 5000, 10000]` | Increase if convergence warning occurs. |
| `tol` | Lasso, ElasticNet | `[1e-4, 1e-6]` | Dual gap stopping tolerance. |

---

## 10. Diagnostic Evaluations & Regularization Paths

```
+-----------------------------------------------------------------------------------+
|                        REGULARIZATION PATH DIAGNOSTICS                            |
+-----------------------------------------------------------------------------------+
| Ridge Path: As alpha -> inf, all coefficients decay smoothly toward 0.            |
| Lasso Path: As alpha increases, coefficients hit 0 at distinct knots and stay 0.  |
| Rule: Plot coefficients vs log(alpha). Optimal alpha minimizes CV Mean Test Error.|
+-----------------------------------------------------------------------------------+
```

---

## 11. Calibration, Uncertainty & Prediction Intervals

In regularized models, parameter estimates are deliberately biased ($\mathbb{E}[\hat{\mathbf{w}}] \neq \mathbf{w}^*$). Standard OLS t-distribution prediction intervals are strictly invalid.
**Remedies**:
1. **Residual Bootstrap**: Resample empirical residuals $e_i^* \sim \{y_i - \hat{y}_i\}$ to generate empirical prediction percentiles.
2. **Conformal Prediction**: Use split conformal inference to construct distribution-free, guaranteed finite-sample coverage intervals $[ \hat{y} - q_{1-\alpha}, \hat{y} + q_{1-\alpha} ]$.

---

## 12. Edge Cases, Failure Modes & Remediation

| Failure Mode | Root Cause | Observable Symptom | Engineering Remediation |
| :--- | :--- | :--- | :--- |
| **Unscaled Predictors** | Running Ridge/Lasso without `StandardScaler` | Features with small units get massive penalties | Prepend `StandardScaler()` in an `sklearn.pipeline.Pipeline`. |
| **Lasso Arbitrary Dropping** | Correlated features ($r > 0.95$) fed into Lasso | Lasso selects one feature at random and zeros others | Use ElasticNet (`l1_ratio=0.5`) to trigger the grouping effect. |
| **ConvergenceWarning in Lasso** | Learning rate or small `alpha` with large $D$ | `ConvergenceWarning: Objective did not converge` | Increase `max_iter=5000` or normalize columns. |
| **Penalizing Intercept** | Adding intercept $b$ into $L_2$ norm | Mean prediction shifts toward 0 | Keep `fit_intercept=True` (scikit-learn never penalizes intercept). |
| **$N < D$ Limit in Lasso** | Standard Lasso selects at most $N$ non-zero features | Cannot select $> N$ features even if true model has more | Use ElasticNet, which can select all $D$ features. |
| **Lasso Under-Estimating Non-Zeros** | $L_1$ penalty shrinks active weights toward zero | Underpredicts extreme values | Apply "Relaxed Lasso" / Refit OLS on surviving feature subset. |

---

## 13. Comparative Analysis: Ridge vs Lasso vs ElasticNet

| Dimension | Ordinary Least Squares | Ridge ($L_2$) | Lasso ($L_1$) | ElasticNet ($L_1 + L_2$) |
| :--- | :--- | :--- | :--- | :--- |
| **Penalty Term** | None | $\frac{\alpha}{2} \sum w_j^2$ | $\alpha \sum |w_j|$ | $\alpha \left( \rho \|w\|_1 + \frac{1-\rho}{2} \|w\|_2^2 \right)$ |
| **Solution Form** | Closed-form | Closed-form | Iterative | Iterative |
| **Sparsity ($w_j = 0$)**| Never | Never | Yes | Yes |
| **Collinear Features** | Explodes | Shrinks equally | Selects 1 arbitrarily | Groups together |
| **Behavior when $D \gg N$**| Fails ($\text{rank} < D$) | Unique solution | Selects at most $N$ | Can select all $D$ |

---

## 14. Production Engineering & Serving Considerations

1. **Sparsity-Accelerated Inference**:
   If Lasso prunes 90% of features ($w_j = 0$), prune the feature extraction pipeline! Do not compute or transport zero-weighted features in REST API payloads:
   $$\hat{y} = b + \sum_{j \in \text{NonZero}} w_j x_j$$
2. **Warm-Starting Paths**:
   When training along a sequence of decreasing alphas ($\alpha_1 > \alpha_2 > \dots > \alpha_k$), initialize $\mathbf{w}(\alpha_{t+1})$ with the converged solution $\mathbf{w}(\alpha_t)$. This accelerates Coordinate Descent by $5 \times$ to $10 \times$.

---

## 15. Interpretability & Feature Attribution

- **Lasso**: Eliminates irrelevant noise, producing parsimonious models directly readable by domain experts.
- **Ridge**: Maintains all features. A feature with high correlation to another retains half its weight rather than zero, preventing misleading claims that the omitted feature has zero impact.

---

## 16. Hardware & Distributed Scaling

- **LARS (Least Angle Regression)**: Efficiently computes the complete Lasso regularization path in $\mathcal{O}(N D \cdot \min(N, D))$ operations.
- **Distributed Coordinate Descent**: Shotgun algorithm parallelizes coordinate updates across asynchronous CPU cores when feature covariance is sparse.

---

## 17. Verification & Diagnostics Test Harness

```python
import pytest
import numpy as np
from sklearn.linear_model import Ridge, Lasso

def test_ridge_singular_matrix_invertible():
    # 2 identical collinear columns (Rank 1 matrix)
    X = np.array([[1.0, 1.0], [2.0, 2.0], [3.0, 3.0]])
    y = np.array([2.0, 4.0, 6.0])
    ridge = Ridge(alpha=1.0)
    ridge.fit(X, y)
    # Ridge handles collinearity cleanly without crash
    assert len(ridge.coef_) == 2
    assert np.allclose(ridge.coef_[0], ridge.coef_[1])

def test_lasso_exact_zero():
    X = np.array([[1.0, 0.0], [2.0, 0.0], [3.0, 0.0]])
    y = np.array([2.0, 4.0, 6.0])
    lasso = Lasso(alpha=0.1)
    lasso.fit(X, y)
    # Column 1 is pure zero noise -> must receive exact 0.0 weight
    assert lasso.coef_[1] == 0.0
```

---

## 18. Cross-Track Connections

- **05-ai/machine-learning/01-math-foundations/06-optimisation-and-gradient-descent.md**: Convex analysis and subgradient calculus.
- **05-ai/machine-learning/02-data-and-preprocessing/09-feature-selection.md**: Embedded feature selection via Lasso.
- **05-ai/machine-learning/04-supervised-classification/01-logistic-regression.md**: L1/L2 penalties applied to Log-Loss.

---

## 19. Real-World Case Study: Microarray Cancer Genomics ($D \gg N$)

A cancer research center analyzed gene expression microarrays to predict patient drug response.
- **Data**: $N = 120$ patients, $D = 18,500$ gene expression markers ($D \gg N$).
- **Failure**: Ordinary Least Squares cannot be computed ($18,500 > 120$). Standard Lasso dropped critical pathway genes because correlated gene clusters were arbitrarily pruned down to single markers.
- **Solution**:
  1. Deployed `ElasticNetCV` with `l1_ratio=0.7`.
  2. The $L_2$ component grouped co-regulated biological gene pathways together, while the $L_1$ component eliminated 18,320 irrelevant genes.
  3. The final model isolated 180 coordinated oncogenes with 91.2% out-of-sample prediction accuracy.

---

## 20. Technical Interview Questions & Solutions

### Question 1: Show why the L1 penalty produces sparse solutions ($w_j = 0$) while the L2 penalty only shrinks weights close to zero.
**Answer**:
See Section 2. Mathematically, the subgradient optimality condition for Lasso requires:
$$-\rho_j + \alpha \partial |w_j| = 0$$
Since the subdifferential of $|w_j|$ at 0 is the entire interval $[-1, 1]$, any feature whose correlation $|\rho_j|$ is less than or equal to $\alpha$ satisfies the stationarity condition with $w_j^* = 0$.
In contrast, for Ridge:
$$-\rho_j + \alpha w_j = 0 \implies w_j^* = \frac{\rho_j}{1 + \alpha}$$
For any finite $\alpha$, $w_j^*$ reaches zero **only if** the correlation $\rho_j$ is exactly zero.

### Question 2: Why must the intercept $b$ NOT be regularized?
**Answer**:
The intercept $b$ captures the global target baseline $\mathbb{E}[y]$ when all features are zero. If we add $\alpha b^2$ or $\alpha |b|$ to the loss, we penalize the model for having a non-zero mean target. For example, if predicting house prices where the average price is $\$500,000$, penalizing $b$ forces the model to artificially pull predicted prices toward $\$0$, introducing massive systemic bias across the entire prediction space.

### Question 3: What is the "Grouping Effect" in ElasticNet?
**Answer**:
Zou & Hastie (2005) proved that for two identical features $x_i = x_j$:
- In Lasso, the solution is not unique; the model can assign arbitrary weights $(\alpha, 0)$ or $(0, \alpha)$ or $(\frac{\alpha}{2}, \frac{\alpha}{2})$.
- In ElasticNet, due to the strictly convex $L_2$ term, the difference between their coefficients satisfies:
  $$|w_i^* - w_j^*| \le \frac{\sqrt{2(1 - r)}}{\alpha(1 - \rho)} \|\mathbf{y}\|_2$$
  As correlation $r \to 1$, $|w_i^* - w_j^*| \to 0$. Highly correlated features receive identical weights and enter or exit the model together as a group.

### Question 4: How does Ridge Regression resolve the singularity of $X^T X$ when $N < D$?
**Answer**:
When $N < D$, $\text{rank}(X^T X) \le N < D$. The Gram matrix has at least $D - N$ zero eigenvalues.
By adding $\alpha \mathbf{I}$, every eigenvalue $\lambda_i$ is shifted to $\lambda_i + \alpha$. Since $\lambda_i \ge 0$ and $\alpha > 0$, all eigenvalues of $(X^T X + \alpha \mathbf{I})$ are strictly positive ($\ge \alpha > 0$). The matrix is strictly positive definite and always invertible.

### Question 5: Why is standardizing features mandatory before fitting Ridge or Lasso?
**Answer**:
The penalties $\sum w_j^2$ and $\sum |w_j|$ penalize all coefficients with equal weight $\alpha$.
If feature $x_1$ has variance $\sigma_1^2 = 1$ and $x_2$ has variance $\sigma_2^2 = 10,000$, the coefficient $w_2$ will naturally be $100 \times$ smaller than $w_1$ to have the same predictive impact. The regularization penalty will disproportionately punish $w_1$, severely shrinking it while leaving $w_2$ largely unregularized. Standardizing features to $\sigma = 1$ puts all coefficients on an equal geometric playing field.

### Question 6: What is the Soft-Thresholding Operator $\mathcal{S}(z, \gamma)$?
**Answer**:
$$\mathcal{S}(z, \gamma) = \text{sign}(z) \max(0, |z| - \gamma)$$
It translates a value $z$ toward 0 by amount $\gamma$, setting it identically to 0 if $|z| \le \gamma$. It is the analytical proximal operator for the $L_1$ norm.

### Question 7: What is the difference between RidgeCV and GridSearchCV(Ridge())?
**Answer**:
`RidgeCV` leverages an analytical shortcut based on the **Sherman-Morrison-Woodbury formula** and generalized cross-validation (GCV). It evaluates Leave-One-Out CV for all values of $\alpha$ in essentially a **single matrix decomposition**, taking seconds. `GridSearchCV(Ridge())` refits the model $K$ times for every single $\alpha$, taking $K \times$ longer.

### Question 8: What is "Relaxed Lasso" (Debiasing)?
**Answer**:
While Lasso is excellent at feature selection, the $L_1$ penalty continues to shrink the active, non-zero coefficients toward zero, introducing downward estimation bias.
**Relaxed Lasso** uses Lasso in Stage 1 strictly to select the active feature subset $S = \{j \mid w_j \neq 0\}$, and then in Stage 2 refits an unregularized Ordinary Least Squares model strictly on the surviving subset $X_S$, eliminating shrinkage bias.

---

## 21. Guided Practice & Hands-On Exercises

### Exercise 1: Closed-form Ridge vs SVD Shrinkage
- Implement Ridge using both $(X^T X + \alpha \mathbf{I})^{-1} X^T y$ and SVD $V (\Sigma^2 + \alpha \mathbf{I})^{-1} \Sigma U^T y$. Assert that both methods produce identical weight vectors to within $10^{-8}$.

### Exercise 2: Coordinate Descent Convergence Tracer
- Instrument the pure NumPy Lasso Coordinate Descent function to record the objective loss $J(\mathbf{w})$ after every single coordinate update. Plot the monotonically decreasing loss curve.

### Exercise 3: Regularization Path Plotter
- Compute and plot the coefficient paths for Ridge and Lasso across 50 values of $\alpha \in [10^{-3}, 10^3]$ on the diabetes dataset, observing the distinct knots where Lasso coefficients hit zero.

### Exercise 4: Grouping Effect Simulation
- Generate two features with correlation $r = 0.99$. Fit Lasso and ElasticNet across 10 random data splits. Measure how often Lasso arbitrarily drops one feature versus ElasticNet retaining both.

### Exercise 5: Relaxed Lasso Implementation
- Implement a two-stage `RelaxedLasso` scikit-learn estimator: fit `LassoCV`, extract non-zero features, and fit `LinearRegression` on the selected columns.

### Exercise 6: Conformal Prediction Interval Generator
- Implement split conformal prediction on a fitted Ridge model: use a calibration set to find the empirical quantile of absolute residuals $q = \text{Quantile}(|y_i - \hat{y}_i|, 0.95)$ and construct 95% test prediction intervals.
