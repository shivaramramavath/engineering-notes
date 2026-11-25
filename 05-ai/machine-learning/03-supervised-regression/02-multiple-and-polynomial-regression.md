---
title: "Multiple and Polynomial Regression: Non-Linear Expansions"
description: "Mathematical formulation, polynomial basis functions, Runge's phenomenon, centering mitigations, and production pipelines for Polynomial Regression."
track: "machine-learning"
module: "03-supervised-regression"
topic: "Multiple and Polynomial Regression"
order: 2
format: "algorithm"
level: "beginner"
tags: ["polynomial-regression", "multiple-regression", "basis-expansion", "runge-phenomenon", "overfitting", "supervised-learning"]
prerequisites: [
  "05-ai/machine-learning/03-supervised-regression/01-linear-regression.md",
  "05-ai/machine-learning/02-data-and-preprocessing/08-feature-engineering.md"
]
key_takeaways:
  - "Polynomial regression models non-linear relationships by applying linear models to non-linear polynomial basis expansions phi(x)."
  - "Higher-degree polynomials (d > 4) suffer from Runge's phenomenon: catastrophic wild oscillations near domain boundaries."
  - "Powers of uncentered features (x, x^2, x^3) induce extreme structural multicollinearity; mean-centering before polynomial expansion is essential."
  - "Model complexity grows combinatorially with degree d and feature count D: M = binom(D+d, d)."
---

# Multiple and Polynomial Regression: Non-Linear Expansions

## 1. Algorithm Card

```
+-----------------------------------------------------------------------------------+
|                              ALGORITHM SPECIFICATION                              |
+-----------------------------------------------------------------------------------+
| Paradigm:         Supervised Learning                                             |
| Family:           Linear Models with Non-Linear Basis Expansions                  |
| Input Space:      x in R^D -> Expanded to phi(x) in R^M where M = binom(D+d, d)  |
| Output Space:     y in R (Continuous Scalars)                                     |
| Objective:        MSE over expanded features: J(w) = (1 / 2N) * ||Phi w - y||_2^2 |
| Optimization:     Normal Equation on Phi or Ridge Regularization                  |
| Time Complexity:  Training: O(N M^2 + M^3) where M grows combinatorially          |
|                   Inference: O(M) per sample                                      |
| Space Complexity: O(M) model parameters                                           |
| Key Assumptions:  Linearity in transformed feature space, additive Gaussian noise |
| Invariances:      Sensitive to feature scale; uncentered features inflate VIF     |
+-----------------------------------------------------------------------------------+
```

---

## 2. Architecture & Intuition

When the underlying data-generating phenomenon is non-linear (e.g., projectile trajectories, enzymatic reactions, diminishing economic returns), fitting a straight line $y = w x + b$ induces severe underfitting (high bias).
**Polynomial Regression** retains the linear optimization machinery of Ordinary Least Squares by mapping the original input vector $\mathbf{x} \in \mathbb{R}^D$ into a higher-dimensional feature space via a fixed, non-linear basis expansion function $\phi(\mathbf{x})$:

$$\hat{y} = \mathbf{w}^T \phi(\mathbf{x})$$

For a single feature $x \in \mathbb{R}$ expanded to degree $d$:
$$\phi_d(x) = [1, x, x^2, x^3, \dots, x^d]^T \in \mathbb{R}^{d+1}$$
$$\hat{y} = w_0 + w_1 x + w_2 x^2 + \dots + w_d x^d$$

Even though the relationship between $\hat{y}$ and $x$ is non-linear, the relationship between $\hat{y}$ and the parameter vector $\mathbf{w}$ is **strictly linear**. Therefore, the optimization problem remains globally convex with an exact closed-form analytical solution.

```
+-----------------------------------------------------------------------------------+
|                          POLYNOMIAL REGRESSION FLOW                               |
+-----------------------------------------------------------------------------------+
|  [Raw Scalar x] ---> [Polynomial Feature Map phi_d(x)] ---> [Standard OLS Solver] |
|                      [1, x, x^2, ..., x^d]                  (Phi^T Phi)^(-1) Phi^Ty
+-----------------------------------------------------------------------------------+
```

---

## 3. Theoretical Foundations & Mathematical Derivation

### 3.1 Multivariate Polynomial Expansion

For a multivariate feature vector $\mathbf{x} = [x_1, x_2, \dots, x_D]^T$, a complete polynomial expansion of degree $d$ includes all monomials of the form:

$$x_1^{k_1} x_2^{k_2} \dots x_D^{k_D} \quad \text{such that} \quad \sum_{j=1}^D k_j \le d, \quad k_j \in \mathbb{N}_0$$

For $D=2$ and degree $d=2$:
$$\phi(\mathbf{x}) = [1, x_1, x_2, x_1^2, x_1 x_2, x_2^2]^T$$

The total number of transformed features $M$ is given by:
$$M = \binom{D + d}{d} = \frac{(D + d)!}{D! \, d!}$$

### 3.2 The Closed-Form Normal Equation on Basis Matrix $\Phi$

Let $\Phi \in \mathbb{R}^{N \times M}$ be the transformed design matrix where row $i$ is $\phi(\mathbf{x}_i)^T$.
The objective function is:
$$J(\mathbf{w}) = \frac{1}{2N} \|\mathbf{y} - \Phi \mathbf{w}\|_2^2$$

Setting the gradient $\nabla_{\mathbf{w}} J(\mathbf{w}) = \mathbf{0}$ yields the basis Normal Equation:
$$\mathbf{w}^* = (\Phi^T \Phi)^{-1} \Phi^T \mathbf{y}$$

### 3.3 Runge's Phenomenon

In numerical analysis, **Runge's phenomenon** describes the problem of severe oscillation at the edges of an interval when polynomial interpolation or unregularized high-degree polynomial regression is fitted using equidistant points.
As degree $d \to \infty$:
$$\lim_{d \to \infty} \max_{x \in [-1, 1]} |f(x) - \hat{f}_d(x)| = \infty$$
At the boundaries $x \to \pm 1$, the polynomial develops violent sinusoidal ripples, creating catastrophic generalization failure.

---

## 4. Computational Complexity Analysis

| Stage | Complexity | Practical Consideration |
| :--- | :--- | :--- |
| **Feature Expansion** | $\mathcal{O}(N \cdot M)$ | Memory intensive for large $M$ |
| **Gram Matrix ($\Phi^T \Phi$)** | $\mathcal{O}(N \cdot M^2)$ | Quadratic in expanded dimension $M$ |
| **Matrix Inversion** | $\mathcal{O}(M^3)$ | Impractical when $M > 10,000$ |
| **Inference per sample** | $\mathcal{O}(M)$ | Requires computing all monomial combinations |

---

## 5. Inductive Bias & Core Assumptions

1. **Continuous Smoothness**: Polynomial regression assumes the underlying target function is infinitely differentiable ($C^\infty$).
2. **Global Influence**: A single distant data point exerts influence over the entire global fitted curve, unlike localized splines or decision trees.
3. **Severe Structural Multicollinearity**: Successive powers $x, x^2, x^3$ are inherently correlated. If $x \in [10, 20]$, $\text{corr}(x, x^2) \approx 0.99$.
   *Remedy*: Center features around their sample mean ($\tilde{x} = x - \bar{x}$) before computing powers, which renders odd and even powers approximately orthogonal.

---

## 6. From-Scratch NumPy Implementation

```python
"""
Pure NumPy Implementation of Polynomial Regression with Feature Centering.
Tested on Python 3.11, NumPy 2.x.
"""

from typing import List
import numpy as np


class PureNumPyPolynomialRegression:
    """Polynomial Regression using OLS on Vandermonde / Basis Matrix."""

    def __init__(self, degree: int = 2, center_features: bool = True):
        self.degree = degree
        self.center_features = center_features
        self.mean_: float = 0.0
        self.coef_: np.ndarray = np.array([])

    def _expand(self, x: np.ndarray) -> np.ndarray:
        # x is 1D array of shape (N,)
        x_centered = x - self.mean_ if self.center_features else x
        # Construct Vandermonde-style basis matrix: [1, x, x^2, ..., x^d]
        columns = [np.ones_like(x_centered)]
        for p in range(1, self.degree + 1):
            columns.append(x_centered ** p)
        return np.column_stack(columns)

    def fit(self, x: np.ndarray, y: np.ndarray):
        assert x.ndim == 1, "Currently supporting 1D input series."
        if self.center_features:
            self.mean_ = float(np.mean(x))
        else:
            self.mean_ = 0.0

        Phi = self._expand(x)
        # Solve Normal Equation via pseudo-inverse
        self.coef_ = np.linalg.pinv(Phi) @ y
        return self

    def predict(self, x: np.ndarray) -> np.ndarray:
        assert self.coef_.size > 0, "Model has not been fitted."
        Phi = self._expand(x)
        return Phi @ self.coef_


# Verification Run
if __name__ == "__main__":
    np.random.seed(42)
    N = 100
    # True relationship: y = 2.0 - 1.5*x + 3.0*x^2 + noise
    x_true = np.linspace(-2.0, 2.0, N)
    y_true = 2.0 - 1.5 * x_true + 3.0 * (x_true ** 2) + np.random.normal(0, 0.2, size=N)

    poly_reg = PureNumPyPolynomialRegression(degree=2, center_features=False)
    poly_reg.fit(x_true, y_true)

    print("--- Pure NumPy Polynomial Fit ---")
    print(f"Recovered Coefficients: {poly_reg.coef_.round(4)} (Expected: [2.0, -1.5, 3.0])")
    assert np.allclose(poly_reg.coef_, [2.0, -1.5, 3.0], atol=0.1)
    print("[SUCCESS] Pure NumPy Polynomial Regression verified.")
```

---

## 7. Production-Grade Scikit-Learn Implementation

```python
"""
Production Pipeline: PolynomialFeatures with Ridge Regularization.
Avoids Runge's phenomenon by penalizing high-degree coefficients.
"""

import numpy as np
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import PolynomialFeatures, StandardScaler
from sklearn.linear_model import RidgeCV
from sklearn.metrics import mean_squared_error, r2_score


def build_production_poly_pipeline(max_degree: int = 3):
    pipeline = Pipeline([
        ("poly", PolynomialFeatures(degree=max_degree, include_bias=False)),
        ("scaler", StandardScaler()), # MANDATORY: Scale polynomial powers
        ("ridge", RidgeCV(alphas=np.logspace(-3, 3, 20), cv=5))
    ])
    return pipeline


if __name__ == "__main__":
    np.random.seed(42)
    x = np.random.uniform(-3, 3, size=(200, 1))
    y = np.sin(x).flatten() + np.random.normal(0, 0.1, size=200)

    pipe = build_production_poly_pipeline(max_degree=5)
    pipe.fit(x, y)
    y_pred = pipe.predict(x)

    print("--- Polynomial + Ridge Pipeline Performance ---")
    print(f"Optimal Alpha: {pipe.named_steps['ridge'].alpha_:.4f}")
    print(f"R^2 Score:     {r2_score(y, y_pred):.4f}")
    assert r2_score(y, y_pred) > 0.90
```

---

## 8. Model Persistence & Safe Serialization

```python
import joblib
import tempfile
import os

def persist_pipeline(pipeline):
    with tempfile.TemporaryDirectory() as tmp_dir:
        path = os.path.join(tmp_dir, "poly_pipeline.joblib")
        joblib.dump(pipeline, path)
        loaded = joblib.load(path)
        assert len(loaded.named_steps) == len(pipeline.named_steps)
        print("[SUCCESS] Polynomial Pipeline persisted safely.")
```

---

## 9. Hyperparameter Tuning & Optimization

| Hyperparameter | Search Space | Default | Optimization Rule |
| :--- | :--- | :--- | :--- |
| `degree` | `[2, 3, 4, 5]` | `2` | Use CV to find degree where validation error is minimized. |
| `interaction_only`| `[True, False]` | `False` | Set `True` if pure powers ($x^2, x^3$) are unphysical. |
| `include_bias` | `[True, False]` | `True` | Set `False` if downstream regressor includes intercept. |

---

## 10. Diagnostic Evaluations & Residual Analysis

```
+-----------------------------------------------------------------------------------+
|                        DEGREE SELECTION VIA RESIDUALS                             |
+-----------------------------------------------------------------------------------+
| Degree d = 1: Residual plot shows distinct parabolic U-curve (Underfitting).      |
| Degree d = 2: Residuals become uniformly distributed horizontal noise (Optimal).  |
| Degree d = 10: Training MSE -> 0, but validation MSE explodes (Overfitting).      |
+-----------------------------------------------------------------------------------+
```

---

## 11. Calibration, Uncertainty & Prediction Intervals

Uncertainty explodes at domain extremes $x > x_{\max}$:
$$\sigma_{\text{pred}}^2(\mathbf{x}_0) = \sigma^2 \left( 1 + \phi(\mathbf{x}_0)^T (\Phi^T \Phi)^{-1} \phi(\mathbf{x}_0) \right)$$
Because monomial components $x_0^d$ grow exponentially, prediction intervals flare out dramatically outside the training range.

---

## 12. Edge Cases, Failure Modes & Remediation

| Failure Mode | Root Cause | Observable Symptom | Engineering Remediation |
| :--- | :--- | :--- | :--- |
| **Runge's Phenomenon** | High polynomial degree ($d \ge 5$) | Massive error spikes at boundary points | Restrict $d \le 3$; switch to Splines or RBF kernels. |
| **Combinatorial Explosion** | $D=50$ features expanded to degree $d=3$ | Generates 23,426 features; out-of-memory | Set `interaction_only=True` or perform feature selection first. |
| **Numerical Overflow** | Calculating $x^{10}$ on unscaled features ($x=100$) | $100^{10} = 10^{20}$, float overflow | Prepend `StandardScaler()` before `PolynomialFeatures`. |
| **Structural Collinearity** | $x, x^2, x^3$ have VIF $> 1000$ | Gram matrix ill-conditioned | Center predictors ($x - \bar{x}$) or use orthogonal polynomials. |
| **Extrapolation Disaster** | Predicting on inputs outside training bounds | Predictions diverge to $\pm \infty$ | Clip inference inputs to $[x_{\min}^{\text{train}}, x_{\max}^{\text{train}}]$. |
| **Boundary Oscillation** | High-order fit on noisy data | Derivatives fluctuate wildly | Apply $L_2$ Ridge penalty ($\alpha > 1.0$). |

---

## 13. Comparative Analysis: Polynomial Regression vs Alternatives

| Dimension | Polynomial OLS | Polynomial Ridge | B-Spline Regression | Decision Tree Regressor |
| :--- | :--- | :--- | :--- | :--- |
| **Boundary Stability** | Very Poor (Oscillates) | Moderate | Excellent (Local knots) | Step constant |
| **Feature Dimensionality** | $\mathcal{O}(D^d)$ (Explosive) | $\mathcal{O}(D^d)$ | $\mathcal{O}(\text{knots} \cdot D)$ | $\mathcal{O}(D)$ |
| **Global vs Local** | Global basis | Global basis | Local basis | Local partition |
| **Differentiability** | Infinitely smooth | Infinitely smooth | Piecewise $C^2$ | Discontinuous |

---

## 14. Production Engineering & Serving Considerations

1. **Inference Latency Optimization**:
   Pre-compute expanded monomial values during streaming ingest. If scoring in C++, implement Horner's Rule to evaluate 1D polynomials in $\mathcal{O}(d)$ operations without exponentiation:
   $$P(x) = w_0 + x(w_1 + x(w_2 + \dots + x(w_d)))$$
2. **Memory Footprint**:
   Expanded basis matrices densify rapidly. Keep `PolynomialFeatures` inside a streaming pipeline to avoid persisting multi-gigabyte expanded matrices to disk.

---

## 15. Interpretability & Feature Attribution

Marginal effects in polynomial regression vary with the feature value itself:
$$\frac{\partial \hat{y}}{\partial x} = w_1 + 2 w_2 x + 3 w_3 x^2 + \dots + d w_d x^{d-1}$$
The coefficient $w_2$ governs the **curvature** (acceleration/deceleration), while $w_1$ is the instantaneous slope strictly at $x=0$.

---

## 16. Hardware & Distributed Scaling

Matrix multiplication of basis matrix $\Phi^T \Phi$ is parallelized across multi-threaded BLAS/LAPACK. On clusters (PySpark), use `VectorPolynomialExpansion` followed by distributed Ridge regression.

---

## 17. Verification & Diagnostics Test Harness

```python
import pytest
import numpy as np
from sklearn.preprocessing import PolynomialFeatures

def test_polynomial_expansion_feature_count():
    # 2 features, degree 2: 1 + 2 + 3 = 6 features
    X = np.array([[2.0, 3.0]])
    poly = PolynomialFeatures(degree=2, include_bias=True)
    out = poly.fit_transform(X)
    assert out.shape == (1, 6)
    # Expected: [1, x1, x2, x1^2, x1*x2, x2^2] = [1, 2, 3, 4, 6, 9]
    assert np.allclose(out[0], [1.0, 2.0, 3.0, 4.0, 6.0, 9.0])
```

---

## 18. Cross-Track Connections

- **05-ai/machine-learning/01-math-foundations/08-curse-of-dimensionality.md**: Combinatorial explosion in basis expansions.
- **05-ai/machine-learning/03-supervised-regression/03-regularisation-ridge-lasso-elasticnet.md**: Regularizing polynomial weights.
- **05-ai/machine-learning/14-deep-learning-bridge/01-neural-networks-as-function-approximators.md**: Multilayer Perceptrons as learned non-linear basis expansions.

---

## 19. Real-World Case Study: Chemical Reactor Yield Optimization

A specialty chemicals plant modeled polymer output yield as a function of temperature ($T$) and pressure ($P$).
- **Physics**: Yield follows an inverted parabolic response surface with a unique optimal operating temperature.
- **Problem**: Linear regression had $R^2 = 0.22$. A 6th-degree polynomial overfitted violently, recommending dangerous pressure setpoints ($P > 200\text{ bar}$) where the model oscillated upwards.
- **Solution**:
  1. Centered $T$ and $P$ around nominal operating baselines ($150^\circ\text{C}, 50\text{ bar}$).
  2. Restricted expansion to degree $d=2$ with interaction: $\phi(T, P) = [T, P, T^2, P^2, T \cdot P]$.
  3. Fit a Ridge model ($\alpha = 0.5$).
  4. Response surface achieved $R^2 = 0.94$, accurately locating the global maximum yield at $168^\circ\text{C}$ and $52\text{ bar}$ without dangerous boundary oscillations.

---

## 20. Technical Interview Questions & Solutions

### Question 1: Why is Polynomial Regression classified as a Linear Model despite predicting non-linear curves?
**Answer**:
A model is classified as a "linear model" based on whether it is **linear in its parameters** $\mathbf{w}$, not whether it is linear in its input variables $\mathbf{x}$.
The prediction function is:
$$\hat{y}(\mathbf{x}) = \sum_{j=0}^M w_j \phi_j(\mathbf{x})$$
The gradient of $\hat{y}$ with respect to parameter $w_j$ is $\frac{\partial \hat{y}}{\partial w_j} = \phi_j(\mathbf{x})$, which does not depend on $\mathbf{w}$.
Consequently, the MSE loss function $J(\mathbf{w})$ remains a quadratic convex paraboloid in parameter space with a single unique global minimum that can be solved analytically via linear algebra.

### Question 2: What is Horner's Rule, and why is it preferred for polynomial evaluation in production?
**Answer**:
Horner's rule rewrites a polynomial of degree $d$:
$$P(x) = w_0 + w_1 x + w_2 x^2 + \dots + w_d x^d$$
into nested form:
$$P(x) = w_0 + x(w_1 + x(w_2 + \dots + x(w_{d-1} + x w_d)\dots))$$
- **Naive evaluation**: Computing $x^k$ directly requires $k-1$ multiplications, totaling $\mathcal{O}(d^2)$ operations.
- **Horner's Rule**: Requires strictly $d$ multiplications and $d$ additions ($\mathcal{O}(d)$ operations), eliminating numerical instability from large power exponentiation.

### Question 3: How does feature centering mitigate structural multicollinearity in polynomial regression?
**Answer**:
If a feature $x$ is positive ($x \in [10, 20]$), $x$ and $x^2$ are monotonically increasing together, producing correlation $r(x, x^2) \approx 0.99$.
By centering $x$ around its sample mean ($\tilde{x} = x - \bar{x}$), $\tilde{x}$ spans $[-5, +5]$.
For symmetric distributions centered at 0:
$$\text{Cov}(\tilde{x}, \tilde{x}^2) = \mathbb{E}[\tilde{x}^3] - \mathbb{E}[\tilde{x}] \mathbb{E}[\tilde{x}^2] = 0 - 0 = 0$$
Odd and even powers become completely uncorrelated, reducing the Condition Number of $\Phi^T \Phi$ from $10^8$ to $< 10^2$.

### Question 4: Derive the total number of terms in a polynomial expansion of degree $d$ across $D$ features.
**Answer**:
See Section 3.1. The number of non-negative integer solutions to $\sum_{j=1}^D k_j \le d$ equals the number of ways to distribute $d$ indistinguishable tokens among $D+1$ bins (using the stars and bars combinatorial theorem):
$$M = \binom{D + d}{d} = \frac{(D + d)!}{D! \, d!}$$

### Question 5: Why does unregularized Polynomial Regression extrapolate poorly?
**Answer**:
As $|x| \to \infty$, the highest-order term $w_d x^d$ dominates all lower-order terms. The prediction asymptotic behavior is:
$$\lim_{x \to \pm \infty} \hat{y}(x) \approx w_d x^d \to \pm \infty$$
Even if the true natural phenomenon plateaus (asymptotes) at high values, the polynomial is mathematically forced to accelerate toward $\pm \infty$.

### Question 6: What is the difference between `interaction_only=True` and `interaction_only=False` in scikit-learn?
**Answer**:
- `interaction_only=False`: Generates all combinations including powers: $x_1, x_2, x_1^2, x_1 x_2, x_2^2$.
- `interaction_only=True`: Excludes pure powers $x_j^k$ for $k \ge 2$, generating only interaction terms: $x_1, x_2, x_1 x_2$.

### Question 7: Can you apply Gradient Descent to Polynomial Regression?
**Answer**:
Yes. Once the basis matrix $\Phi$ is computed, standard mini-batch gradient descent can be applied directly:
$$\mathbf{w}^{(t+1)} = \mathbf{w}^{(t)} - \eta \cdot \frac{1}{N} \Phi^T (\Phi \mathbf{w}^{(t)} - \mathbf{y})$$
Feature scaling of all columns of $\Phi$ is strictly mandatory to prevent gradient explosion.

### Question 8: How do Splines resolve the primary weakness of Polynomial Regression?
**Answer**:
Polynomial regression enforces a single global polynomial equation across the entire domain.
**Splines** partition the domain into local intervals separated by "knots", fitting independent low-degree polynomials (typically cubic $d=3$) within each interval, constrained to match value and first/second derivatives at the knots. This provides local flexibility without boundary Runge oscillations.

---

## 21. Guided Practice & Hands-On Exercises

### Exercise 1: Horner's Rule Evaluator
- Implement an optimized polynomial inference function in pure Python using Horner's method and benchmark its speed against naive `sum(w[i] * x**i)`.

### Exercise 2: Runge's Phenomenon Simulation
- Sample 20 equidistant points from Runge's function $f(x) = \frac{1}{1 + 25 x^2}$ on $[-1, 1]$. Fit unregularized polynomials of degree $d \in [2, 15]$. Plot the exploding edge errors.

### Exercise 3: Cross-Validated Degree Selection
- Implement an automated degree selector using 5-fold cross-validation that tests degrees $d \in [1, 8]$ and returns the optimal degree based on minimum validation MSE.

### Exercise 4: Centering VIF Comparison
- Generate a positive continuous feature $x \in [50, 100]$. Compute the VIF between $x, x^2, x^3$ before and after mean-centering, verifying the reduction in collinearity.

### Exercise 5: 2D Response Surface Plotter
- Fit a degree-2 polynomial with interaction on two features $(x_1, x_2)$. Compute and print the coordinates of the critical stationary point $\nabla_{\mathbf{x}} \hat{y} = \mathbf{0}$.

### Exercise 6: Orthogonal Legendre Polynomial Basis
- Implement a custom transformer using NumPy's `numpy.polynomial.legendre.legvander` to generate orthogonal Legendre polynomial bases on $[-1, 1]$.
