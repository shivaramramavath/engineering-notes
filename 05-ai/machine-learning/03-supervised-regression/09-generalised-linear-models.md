---
title: "Generalised Linear Models (GLMs): Poisson, Gamma, and Tweedie"
description: "Mathematical theory of the Exponential Dispersion Family, link functions, Iteratively Reweighted Least Squares (IRLS), Poisson counts, and Tweedie compound distributions."
track: "machine-learning"
module: "03-supervised-regression"
topic: "Generalised Linear Models (GLMs)"
order: 9
format: "algorithm"
level: "intermediate"
tags: ["glm", "generalized-linear-models", "poisson-regression", "gamma-regression", "tweedie", "irls", "exponential-family"]
prerequisites: [
  "05-ai/machine-learning/01-math-foundations/03-probability-and-distributions.md",
  "05-ai/machine-learning/01-math-foundations/09-maximum-likelihood-and-bayes.md",
  "05-ai/machine-learning/03-supervised-regression/01-linear-regression.md"
]
key_takeaways:
  - "GLMs unify linear, logistic, count, and skewed positive continuous regressions under the single framework of the Exponential Dispersion Family."
  - "A GLM consists of three components: a random distribution component, a linear systematic predictor eta = Xw, and a monotonic link function g(mu) = eta."
  - "Poisson regression models discrete non-negative counts with variance equal to mean (V(mu) = mu); exposure time is incorporated via an offset term log(time)."
  - "Gamma regression models strictly positive continuous data with constant coefficient of variation (V(mu) = mu^2); Tweedie models zero-inflated continuous claims."
---

# Generalised Linear Models (GLMs): Poisson, Gamma, and Tweedie

## 1. Algorithm Card

```
+-----------------------------------------------------------------------------------+
|                              ALGORITHM SPECIFICATION                              |
+-----------------------------------------------------------------------------------+
| Paradigm:         Supervised Learning (Parametric Maximum Likelihood Estimation)  |
| Family:           Generalized Linear Models (Nelder & Wedderburn 1972)            |
| Input Space:      X in R^{N x D} (Continuous, Categorical, with Offset Vector)    |
| Output Space:     Poisson: y in N_0 (Counts); Gamma: y in R^+; Tweedie: y >= 0    |
| Likelihood:       Exponential Dispersion Family: f(y; theta, phi)                 |
| Optimization:     Iteratively Reweighted Least Squares (IRLS) via Fisher Scoring  |
| Time Complexity:  Training: O(T * N * D^2 + D^3) (IRLS matrix inversions)         |
|                   Inference: O(D) (Compute eta = w^T x, then invert link g^-1(eta))|
| Space Complexity: O(D) model parameters                                           |
| Key Assumptions:  y follows specified exponential family; g(E[y|X]) = Xw         |
| Invariances:      Equivariant to linear feature transformations; needs scaling    |
+-----------------------------------------------------------------------------------+
```

---

## 2. Architecture & Intuition

Ordinary Least Squares assumes that targets $y$ are continuous and that error residuals are normally distributed with constant variance $\sigma^2$ across all predictions.
In real-world domains, this assumption is fundamentally violated:
1. **Count Data (Website clicks, accident counts)**: Targets are discrete non-negative integers ($y \in \{0, 1, 2, \dots\}$). A linear model can predict nonsensical negative counts ($-3.4$ clicks). Furthermore, count variance naturally increases with the expected count ($\text{Var}(y) = \mu$).
2. **Positively Skewed Continuous Data (Hospital costs, server response latency)**: Targets are strictly positive ($y > 0$) with long right tails. The standard deviation is proportional to the mean ($\text{Var}(y) = \sigma^2 \mu^2$), exhibiting severe heteroscedasticity.
3. **Zero-Inflated Continuous Data (Insurance claim totals)**: Most customers have exactly $\$0.00$ in claims, but a small fraction have continuous positive payouts.

**Generalized Linear Models (GLMs)** resolve these pathologies by allowing the target distribution to belong to the **Exponential Dispersion Family** and connecting the expected mean $\mu = \mathbb{E}[y \mid \mathbf{x}]$ to the linear predictor $\eta = \mathbf{w}^T \mathbf{x}$ through a non-linear **link function** $g(\mu) = \eta$.

```
+-----------------------------------------------------------------------------------+
|                                 GLM ARCHITECTURE                                  |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|   [Features x] ---> [Linear Predictor eta = w^T x + offset]                       |
|                                     |                                             |
|                                     v                                             |
|                     [Inverse Link Function: mu = g^-1(eta)]                       |
|                                     |                                             |
|                                     v                                             |
|                      Expected Mean mu = E[y | x]                                  |
|                                     |                                             |
|                                     v                                             |
|             Variance Function: Var(y) = phi * V(mu)                               |
|             (Poisson: V=mu | Gamma: V=mu^2 | Tweedie: V=mu^p)                     |
+-----------------------------------------------------------------------------------+
```

---

## 3. Theoretical Foundations & Mathematical Derivation

### 3.1 The Exponential Dispersion Family (EDF)

A random variable $Y$ belongs to the Exponential Dispersion Family if its probability density (or mass) function can be expressed in canonical form:

$$f(y; \theta, \phi) = \exp\left( \frac{y \theta - b(\theta)}{a(\phi)} + c(y, \phi) \right)$$

Where:
- $\theta$ is the **natural (canonical) parameter**.
- $\phi$ is the **dispersion parameter** ($a(\phi) = \phi / w_i$).
- $b(\theta)$ is the **log-partition function** (cumulant function).

#### Key Properties via Cumulant Derivatives:
1. **Expected Value**:
   $$\mu = \mathbb{E}[Y] = b'(\theta)$$
2. **Variance**:
   $$\text{Var}(Y) = b''(\theta) a(\phi) = V(\mu) \cdot \frac{\phi}{w_i}$$
   where $V(\mu) = b''(\theta)$ is the **variance function**, defining how variance changes with the mean.

### 3.2 Canonical Link Functions

The link function $g(\mu)$ relates the conditional mean $\mu$ to the linear systematic predictor $\eta = \mathbf{w}^T \mathbf{x}$:

$$\eta = g(\mu) \iff \mu = g^{-1}(\eta)$$

The **canonical link** occurs when the linear predictor directly models the natural parameter: $g(\mu) = \theta$.

| Distribution | Support of $y$ | Variance Function $V(\mu)$ | Canonical Link $g(\mu)$ | Inverse Link $\mu = g^{-1}(\eta)$ | Common Applications |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Gaussian** | $(-\infty, \infty)$ | $1$ (Constant) | $\text{Identity}: \mu$ | $\eta$ | Classic OLS Regression |
| **Poisson** | $\{0, 1, 2, \dots\}$ | $\mu$ | $\text{Log}: \ln(\mu)$ | $\exp(\eta)$ | Count rates, web traffic |
| **Gamma** | $(0, \infty)$ | $\mu^2$ | $\text{Negative Inverse}: -\frac{1}{\mu}$ | $-\frac{1}{\eta}$ (or $\exp(\eta)$)| Claim severity, durations |
| **Inverse Gaussian**| $(0, \infty)$ | $\mu^3$ | $\frac{1}{\mu^2}$ | $\frac{1}{\sqrt{\eta}}$ | Highly skewed degradation |
| **Tweedie ($1 < p < 2$)**| $[0, \infty)$ | $\mu^p$ | $\text{Log}: \ln(\mu)$ | $\exp(\eta)$ | Insurance claims (zero mass + positive)|

### 3.3 Poisson Regression and Exposure Offsets

For event counts occurring over differing observation time intervals or exposure windows (e.g., car accidents per vehicle-mile, server errors per million requests), the expected count $Y_i$ over exposure $t_i$ is:

$$\mathbb{E}[Y_i \mid \mathbf{x}_i] = \mu_i = t_i \cdot \lambda(\mathbf{x}_i)$$
$$\ln(\mu_i) = \ln(t_i) + \ln(\lambda(\mathbf{x}_i)) = \ln(t_i) + \mathbf{w}^T \mathbf{x}_i$$

The term $\ln(t_i)$ is called the **Offset variable**. It is included in the linear predictor with a **fixed coefficient of exactly $1.0$** (never estimated).

### 3.4 Optimization: Iteratively Reweighted Least Squares (IRLS)

The log-likelihood of a GLM is:
$$\ell(\mathbf{w}) = \sum_{i=1}^N \frac{y_i \theta_i - b(\theta_i)}{\phi} + c(y_i, \phi)$$

Because the Hessian matrix is negative definite, GLMs are solved via **Fisher Scoring / IRLS**:
In each iteration $t$:
1. Compute the working response $z_i$:
   $$z_i = \eta_i^{(t)} + (y_i - \mu_i^{(t)}) g'(\mu_i^{(t)})$$
2. Compute the diagonal weight matrix $W \in \mathbb{R}^{N \times N}$:
   $$W_{ii} = \frac{1}{V(\mu_i^{(t)}) \cdot [g'(\mu_i^{(t)})]^2}$$
3. Update parameter vector $\mathbf{w}$ via weighted least squares:
   $$\mathbf{w}^{(t+1)} = (X^T W X)^{-1} X^T W \mathbf{z}$$
Convergence is typically achieved in 5 to 15 iterations.

---

## 4. Computational Complexity Analysis

| Stage | Complexity | Practical Consideration |
| :--- | :--- | :--- |
| **Weight Matrix Evaluation ($W$)** | $\mathcal{O}(N)$ | Vectorized evaluation of $g'(\mu)$ and $V(\mu)$ |
| **Weighted Gram Matrix ($X^T W X$)**| $\mathcal{O}(N D^2)$ | Parallelizable across samples |
| **Inversion per iteration** | $\mathcal{O}(D^3)$ | Solved via Cholesky decomposition |
| **Total IRLS Convergence** | $\mathcal{O}(T_{\text{iter}} \cdot (N D^2 + D^3))$ | Typically $T_{\text{iter}} < 15$ |
| **Inference per sample** | $\mathcal{O}(D)$ | Linear dot product followed by scalar exponentiation |

---

## 5. Inductive Bias & Core Assumptions

1. **Mean-Variance Relationship**: Assumes the conditional variance of $y$ strictly adheres to $V(\mu)$ (e.g., $\text{Var}(y) = \mu$ for Poisson).
2. **Independence**: Observations are mutually independent conditioned on features $X$.
3. **No Overdispersion**: In pure Poisson regression, if the true empirical variance exceeds the mean ($\text{Var}(y) \gg \mu$), the data suffers from **Overdispersion**. Standard errors computed by Poisson will be artificially small, causing severe false positives.
   *Remedy*: Use **Negative Binomial** regression or **Quasi-Poisson** models.

---

## 6. From-Scratch NumPy Implementation

```python
"""
Pure NumPy Implementation of Poisson Regression via IRLS (Log Link).
Tested on Python 3.11, NumPy 2.x.
"""

from typing import Optional
import numpy as np


class PureNumPyPoissonRegression:
    """Poisson Regression with Log Link: mu = exp(X w), solved via IRLS."""

    def __init__(self, max_iter: int = 30, tol: float = 1e-6):
        self.max_iter = max_iter
        self.tol = tol
        self.coef_: np.ndarray = np.array([])
        self.intercept_: float = 0.0

    def fit(self, X: np.ndarray, y: np.ndarray, offset: Optional[np.ndarray] = None):
        assert X.ndim == 2, "X must be 2D array (N, D)."
        assert np.all(y >= 0), "Poisson target y must be strictly non-negative integers."
        N, D = X.shape

        # Add intercept column
        X_design = np.hstack([np.ones((N, 1)), X])
        w = np.zeros(D + 1)
        
        # Initialize offset
        base_offset = offset if offset is not None else np.zeros(N)

        for iteration in range(self.max_iter):
            eta = X_design @ w + base_offset
            # Clip eta to prevent exp overflow
            eta = np.clip(eta, -20.0, 20.0)
            mu = np.exp(eta)

            # For Log Link and Poisson:
            # g(mu) = ln(mu) -> g'(mu) = 1 / mu
            # V(mu) = mu
            # Weight W_ii = 1 / (V * (g')^2) = 1 / (mu * (1/mu^2)) = mu
            W = mu

            # Working response: z = eta + (y - mu) * g'(mu) = eta + (y - mu) / mu
            z = eta - base_offset + (y - mu) / mu

            # Weighted Least Squares: (X^T W X) w_new = X^T W z
            # Vectorized weighted product
            WX = X_design * W[:, np.newaxis]
            A = X_design.T @ WX
            b = X_design.T @ (W * z)

            # Solve system with small ridge stabilizer for numerical safety
            A += 1e-8 * np.eye(D + 1)
            w_new = np.linalg.solve(A, b)

            if np.max(np.abs(w_new - w)) < self.tol:
                break
            w = w_new

        self.intercept_ = float(w[0])
        self.coef_ = w[1:]
        return self

    def predict(self, X: np.ndarray, offset: Optional[np.ndarray] = None) -> np.ndarray:
        N = len(X)
        base_offset = offset if offset is not None else np.zeros(N)
        eta = X @ self.coef_ + self.intercept_ + base_offset
        return np.exp(np.clip(eta, -20.0, 20.0))


# Verification Run
if __name__ == "__main__":
    np.random.seed(42)
    N = 300
    X = np.random.normal(0, 1, size=(N, 2))
    # True relationship: rate = exp(1.2 + 0.8*x0 - 0.5*x1)
    true_w = np.array([0.8, -0.5])
    true_b = 1.2
    rate = np.exp(true_b + X @ true_w)
    y_counts = np.random.poisson(lam=rate)

    glm = PureNumPyPoissonRegression()
    glm.fit(X, y_counts)

    print("--- Pure NumPy Poisson Regression Fit ---")
    print(f"Recovered Weights:   {glm.coef_.round(4)} (Expected: {true_w})")
    print(f"Recovered Intercept: {glm.intercept_:.4f} (Expected: {true_b})")

    assert np.allclose(glm.coef_, true_w, atol=0.1)
    assert np.isclose(glm.intercept_, true_b, atol=0.1)
    print("[SUCCESS] Pure NumPy Poisson Regression verified.")
```

---

## 7. Production-Grade Scikit-Learn Implementation

```python
"""
Production Pipeline: Poisson, Gamma, and Tweedie Regressors.
Demonstrates D^2-Deviance evaluation and exposure offsets.
"""

import numpy as np
import pandas as pd
from sklearn.linear_model import PoissonRegressor, GammaRegressor, TweedieRegressor
from sklearn.model_selection import train_test_split
from sklearn.metrics import d2_tweedie_score


def run_glm_production_suite():
    np.random.seed(42)
    N = 1000

    # 1. Synthesize Count Telemetry Data with Exposure Time
    X = np.random.randn(N, 3)
    exposure_hours = np.random.uniform(1.0, 24.0, size=N)
    log_exposure = np.log(exposure_hours)

    # True Poisson rate
    lambda_rate = np.exp(0.5 + 0.4 * X[:, 0] - 0.3 * X[:, 1])
    expected_counts = lambda_rate * exposure_hours
    y_counts = np.random.poisson(lam=expected_counts)

    X_train, X_test, y_train, y_test, exp_train, exp_test = train_test_split(
        X, y_counts, exposure_hours, test_size=0.2, random_state=42
    )

    # 2. Fit Poisson Regressor with sample weights as exposure
    # Note: In scikit-learn, Tweedie/PoissonRegressor accommodates exposure via sample_weight
    poisson_model = PoissonRegressor(alpha=1e-3, max_iter=300)
    poisson_model.fit(X_train, y_train / exp_train, sample_weight=exp_train)

    # 3. Evaluate Predictions
    y_pred_rate = poisson_model.predict(X_test)
    y_pred_counts = y_pred_rate * exp_test

    d2_score = d2_tweedie_score(y_test, y_pred_counts, power=1) # power=1 is Poisson
    print("--- Production Poisson Model Evaluation ---")
    print(f"D^2 Deviance Score: {d2_score:.4f}")
    assert d2_score > 0.80

    # 4. Fit Gamma Regressor for Strictly Positive Severities
    y_severities = np.random.gamma(shape=2.0, scale=100.0, size=N)
    gamma_model = GammaRegressor(alpha=1e-3).fit(X, y_severities)
    gamma_d2 = d2_tweedie_score(y_severities, gamma_model.predict(X), power=2) # power=2 is Gamma
    print(f"Gamma D^2 Deviance Score: {gamma_d2:.4f}")
    assert gamma_d2 > -0.1


if __name__ == "__main__":
    run_glm_production_suite()
```

---

## 8. Model Persistence & Safe Serialization

```python
import joblib
import tempfile
import os

def persist_glm(model):
    with tempfile.TemporaryDirectory() as tmp_dir:
        path = os.path.join(tmp_dir, "poisson_model.joblib")
        joblib.dump(model, path)
        loaded = joblib.load(path)
        assert np.array_equal(model.coef_, loaded.coef_)
        print("[SUCCESS] GLM model persisted safely.")
```

---

## 9. Hyperparameter Tuning & Optimization

| Hyperparameter | Model | Search Space | Practical Guidance |
| :--- | :--- | :--- | :--- |
| `alpha` | All GLMs | `[1e-4, 1e-2, 1.0, 10.0]` | $L_2$ regularization penalty on coefficients. |
| `power` | `TweedieRegressor` | `[1.0, 1.5, 2.0, 3.0]` | $p=1$ (Poisson), $p=2$ (Gamma), $p \in (1, 2)$ (Compound). |
| `max_iter` | All GLMs | `[100, 300, 1000]` | Increase if L-BFGS convergence warning occurs. |

---

## 10. Diagnostic Evaluations & Deviance Residuals

```
+-----------------------------------------------------------------------------------+
|                        GLM DEVIANCE DIAGNOSTICS                                   |
+-----------------------------------------------------------------------------------+
| 1. Deviance Residuals: Check if deviance residuals e_D follow N(0, 1).            |
| 2. Overdispersion Ratio: phi_hat = Pearson Chi-Square / (N - D - 1).              |
|    - If phi_hat >> 1.5 in Poisson -> Severe Overdispersion (Switch to NegBin).     |
| 3. D^2 Score (Explained Deviance): Analogous to R^2 for non-Gaussian likelihoods. |
+-----------------------------------------------------------------------------------+
```

---

## 11. Calibration, Uncertainty & Prediction Intervals

For Poisson regression, the conditional distribution is explicitly known:
$$Y \mid \mathbf{x} \sim \text{Poisson}(\lambda = \exp(\mathbf{w}^T \mathbf{x}))$$
Exact prediction intervals are derived from the Poisson Cumulative Distribution Function:
$$[ \text{Poisson.ppf}(\alpha/2, \lambda), \, \text{Poisson.ppf}(1 - \alpha/2, \lambda) ]$$

---

## 12. Edge Cases, Failure Modes & Remediation

| Failure Mode | Root Cause | Observable Symptom | Engineering Remediation |
| :--- | :--- | :--- | :--- |
| **Poisson Overdispersion** | Unobserved heterogeneity; true variance $\gg \mu$ | p-values artificially tiny; confidence intervals too narrow | Use Negative Binomial model or Quasi-Poisson with robust covariance. |
| **Zero Values in Gamma GLM** | Target contains $y = 0$ | Throws `ValueError: Some value(s) of y are negative or zero` | Switch to `TweedieRegressor(power=1.5)` which supports exact zeros. |
| **Exp Overflow in Log Link** | Large feature weights cause $\mathbf{w}^T \mathbf{x} > 700$ | `OverflowError: math range error` | Standardize features; add $L_2$ regularization `alpha > 0`. |
| **Missing Offset Exposure** | Units evaluated across differing time windows | Observations with longer exposure falsely appear higher rate | Provide `sample_weight` as exposure duration. |
| **Complete Separation** | Count is strictly 0 for an entire categorical level | Coefficients diverge to $-\infty$ | Apply $L_2$ penalty (`alpha=1.0`). |

---

## 13. Comparative Analysis: GLMs vs Standard Models

| Dimension | Ordinary Least Squares | Poisson GLM | Gamma GLM | Tweedie GLM ($p=1.5$) |
| :--- | :--- | :--- | :--- | :--- |
| **Target Distribution** | Gaussian $\mathcal{N}(\mu, \sigma^2)$ | Poisson $\text{Pois}(\mu)$ | Gamma $\text{Gam}(\alpha, \beta)$| Compound Poisson-Gamma |
| **Target Domain** | $(-\infty, \infty)$ | $\{0, 1, 2, \dots\}$ | $(0, \infty)$ strictly positive | $[0, \infty)$ (Zeros + Positive) |
| **Variance Function** | $V(\mu) = 1$ | $V(\mu) = \mu$ | $V(\mu) = \mu^2$ | $V(\mu) = \mu^{1.5}$ |
| **Link Function** | Identity | Log | Negative Inverse / Log | Log |
| **Evaluation Metric** | MSE / $R^2$ | Poisson Deviance | Gamma Deviance | Tweedie Deviance |

---

## 14. Production Engineering & Serving Considerations

1. **Inference Latency**:
   GLM inference requires a single dot product and scalar transcendental exponentiation:
   $$\hat{\mu} = \exp\left(b + \sum_{j=1}^D w_j x_j\right)$$
   Execution takes $< 100\text{ nanoseconds}$, making GLMs ideal for ultra-high-throughput ad auction bidding engines ($1,000,000\text{ req/sec}$).
2. **Offset Injection at Serving**:
   Ensure the inference payload includes the exposure duration: $\hat{y}_{\text{total}} = \text{duration} \times \exp(\mathbf{w}^T \mathbf{x} + b)$.

---

## 15. Interpretability & Feature Attribution

Because Poisson regression uses a log link ($\ln(\mu) = w_0 + w_1 x_1$), coefficients have a **multiplicative rate ratio interpretation**:
$$\mu = e^{w_0} \cdot e^{w_1 x_1} \dots e^{w_D x_D}$$
- A 1-unit increase in $x_j$ multiplies the expected count rate by a factor of $e^{w_j}$:
  $$\text{Percentage Change} = (e^{w_j} - 1) \times 100\%$$
  *Example*: If $w_1 = 0.08$, a 1-unit increase in $x_1$ increases the expected event count by $(e^{0.08} - 1) \approx +8.3\%$.

---

## 16. Hardware & Distributed Scaling

In Apache Spark MLlib, `GeneralizedLinearRegression` computes the weighted Gram matrix $X^T W X$ across distributed partitions using `treeAggregate`, allowing GLMs to scale across billions of web telemetry logs.

---

## 17. Verification & Diagnostics Test Harness

```python
import pytest
import numpy as np
from sklearn.linear_model import PoissonRegressor

def test_poisson_positive_predictions():
    # Predictions of a log-link Poisson model must be strictly positive
    X = np.random.randn(50, 2)
    y = np.random.poisson(lam=5.0, size=50)
    model = PoissonRegressor(alpha=0.1)
    model.fit(X, y)
    preds = model.predict(X)
    assert np.all(preds > 0.0)

def test_poisson_rate_ratio_property():
    # If feature increases by 1, prediction multiplies by exp(coef)
    X = np.array([[0.0], [1.0]])
    y = np.array([10, 20])
    model = PoissonRegressor(alpha=0.0, fit_intercept=True)
    model.fit(X, y)
    p0 = model.predict([[0.0]])[0]
    p1 = model.predict([[1.0]])[0]
    expected_ratio = np.exp(model.coef_[0])
    assert np.isclose(p1 / p0, expected_ratio)
```

---

## 18. Cross-Track Connections

- **05-ai/machine-learning/01-math-foundations/09-maximum-likelihood-and-bayes.md**: Maximum Likelihood Estimation and Fisher Information.
- **05-ai/machine-learning/04-supervised-classification/01-logistic-regression.md**: Logistic regression as a Bernoulli GLM with Logit link.
- **09-sql/05-grouping-and-aggregation**: Aggregating counts and exposures in database tables.

---

## 19. Real-World Case Study: Auto Insurance Actuarial Pure Premium Engine

A national automotive insurer calculated the actuarial "Pure Premium" for auto policies: $\text{Pure Premium} = \text{Frequency} \times \text{Severity}$.
- **Challenge**:
  1. Claim Frequency is count data (most drivers have 0 accidents; some have 1 or 2).
  2. Claim Severity is continuous, strictly positive, and heavily skewed (most payouts are $\$1,500$; rare crashes cost $\$150,000$).
- **Solution**:
  1. Modeled Claim Frequency using a **Poisson GLM** with `offset = log(policy_exposure_years)`.
  2. Modeled Claim Severity using a **Gamma GLM** strictly on historical claims ($y > 0$).
  3. Combined both into a single **Tweedie GLM ($p=1.65$)** modeling pure premium directly.
  4. The pricing engine complied strictly with insurance regulatory audits (coefficients were transparent rate factors) while reducing loss ratios by $4.2\%$.

---

## 20. Technical Interview Questions & Solutions

### Question 1: What are the three components of any Generalized Linear Model?
**Answer**:
1. **Random Component**: The response variable $Y$ follows a probability distribution from the Exponential Dispersion Family:
   $$f(y; \theta, \phi) = \exp\left(\frac{y \theta - b(\theta)}{a(\phi)} + c(y, \phi)\right)$$
2. **Systematic Component**: A linear combination of predictors $\eta = \mathbf{w}^T \mathbf{x} + \text{offset}$.
3. **Link Function $g(\cdot)$**: A monotonic, differentiable function connecting the expected mean $\mu = \mathbb{E}[Y \mid \mathbf{x}]$ to the linear predictor:
   $$\eta = g(\mu) \iff \mu = g^{-1}(\eta)$$

### Question 2: Why is Ordinary Least Squares inappropriate for modeling event counts?
**Answer**:
1. **Invalid Domain Support**: OLS predicts continuous values in $(-\infty, \infty)$. For small counts, OLS often predicts negative values, which are physically impossible.
2. **Heteroscedasticity**: Count data naturally has non-constant variance. In Poisson processes, $\text{Var}(Y) = \mu$. As the expected rate increases, the variance expands. OLS assumes constant homoscedastic variance $\sigma^2$, violating Gauss-Markov assumptions and corrupting standard errors.
3. **Non-Gaussian Error Distribution**: Small counts are heavily skewed and discrete, severely violating the Gaussian error assumption required for hypothesis tests and confidence intervals.

### Question 3: What is the Offset variable in Poisson Regression, and why is its coefficient fixed at $1.0$?
**Answer**:
See Section 3.3. When counts $Y$ are observed over varying exposure times $t$ (e.g., car crashes over $t$ vehicle-years), the rate is $\lambda = \mu / t$.
Taking the log:
$$\ln(\mu / t) = \mathbf{w}^T \mathbf{x} \implies \ln(\mu) = 1.0 \cdot \ln(t) + \mathbf{w}^T \mathbf{x}$$
The term $\ln(t)$ enters the linear predictor with a fixed coefficient of $1.0$. If its coefficient were estimated freely ($c \ln(t)$), the model would violate the physical dimensional definition of a rate (e.g., doubling the exposure time would not double the expected count).

### Question 4: What is Overdispersion in Poisson Regression, and how do you detect it?
**Answer**:
A theoretical Poisson distribution requires $\text{Var}(Y) = \mathbb{E}[Y] = \mu$ (equidispersion).
**Overdispersion** occurs when empirical variance significantly exceeds the mean: $\text{Var}(Y) > \mu$, typically caused by unobserved subgroup heterogeneity or clustering.
**Detection**: Compute the Pearson dispersion statistic:
$$\hat{\phi} = \frac{\chi^2}{N - D - 1} = \frac{1}{N - D - 1} \sum_{i=1}^N \frac{(y_i - \hat{\mu}_i)^2}{\hat{\mu}_i}$$
If $\hat{\phi} \approx 1.0$, the Poisson assumption holds. If $\hat{\phi} > 1.5$ to $2.0$, data is overdispersed.

### Question 5: What is the Tweedie distribution, and why is it uniquely suited for insurance pricing?
**Answer**:
The Tweedie distribution has variance function $V(\mu) = \mu^p$.
For $1 < p < 2$, it represents a **Compound Poisson-Gamma process**:
$$Y = \sum_{j=1}^N X_j \quad \text{where } N \sim \text{Poisson}(\lambda), \ X_j \sim \text{Gamma}(\alpha, \beta)$$
In insurance:
- $N$ is the number of claims (Poisson).
- $X_j$ is the payout amount per claim (Gamma).
- $Y$ is the total loss.
It natively generates a discrete probability mass at $y=0$ (when $N=0$) and a smooth continuous positive distribution for $y > 0$, allowing insurance loss costs to be modeled in a single unified regression.

### Question 6: What is Deviance in GLMs?
**Answer**:
Deviance measures the goodness of fit of a GLM relative to a **Saturated Model** (a theoretical perfect model with $N$ parameters that achieves $\hat{\mu}_i = y_i$):
$$D(y, \hat{\mu}) = 2 \left( \ell(\text{Saturated}) - \ell(\text{Fitted Model}) \right)$$
It is the GLM generalization of the Residual Sum of Squares (RSS). Lower deviance indicates a better fit.

### Question 7: Explain how the Iteratively Reweighted Least Squares (IRLS) algorithm works.
**Answer**:
See Section 3.4. IRLS uses the Newton-Raphson / Fisher Scoring method to find the maximum likelihood weights $\mathbf{w}$. In each step:
1. It linearizes the link function around the current estimate $\mu^{(t)}$, constructing a synthetic working response $z_i = \eta_i + (y_i - \mu_i) g'(\mu_i)$.
2. It constructs a diagonal weight matrix $W_{ii} = \frac{1}{V(\mu_i) [g'(\mu_i)]^2}$.
3. It solves a weighted least squares problem $\mathbf{w}^{(t+1)} = (X^T W X)^{-1} X^T W \mathbf{z}$.
4. It iterates until parameter change falls below tolerance.

### Question 8: How do you interpret a coefficient $w_j = -0.30$ in a Gamma Regression with a Log link?
**Answer**:
Because the link is log ($\ln(\mu) = \mathbf{w}^T \mathbf{x}$), $\mu = \exp(w_0 + \dots + w_j x_j)$.
A 1-unit increase in $x_j$ multiplies the expected positive target by $e^{-0.30} \approx 0.7408$.
This represents a $(0.7408 - 1) \times 100\% \approx -25.9\%$ decrease in the expected target value, holding all other features constant.

---

## 21. Guided Practice & Hands-On Exercises

### Exercise 1: Pearson Dispersion Statistic Evaluator
- Write a function `compute_dispersion_ratio(y_true, y_pred)` for a Poisson model. Generate synthetic data with Negative Binomial overdispersion and assert that the statistic detects $\hat{\phi} > 2.0$.

### Exercise 2: IRLS vs Scikit-Learn Poisson Shootout
- Benchmark the weights and convergence speed of the from-scratch PureNumPy Poisson IRLS against `sklearn.linear_model.PoissonRegressor`.

### Exercise 3: Auto Insurance Claim Exposure Simulation
- Synthesize an insurance dataset where customer exposure varies from 0.1 to 1.0 years. Fit a Poisson model with and without exposure weighting, demonstrating the parameter bias induced by ignoring exposure.

### Exercise 4: Tweedie Power Parameter Grid Search
- Use `TweedieRegressor` on insurance claim data across power parameters $p \in [1.1, 1.3, 1.5, 1.7, 1.9]$. Identify the optimal $p$ that maximizes explained deviance.

### Exercise 5: Rate Ratio Confidence Interval Calculator
- Implement a function that takes a fitted Poisson GLM, extracts the parameter covariance matrix $(X^T W X)^{-1}$, and computes 95% confidence intervals for the multiplicative rate ratios $e^{w_j}$.

### Exercise 6: Zero-Inflated Count Comparison
- Generate a zero-inflated Poisson dataset where 50% of zeros come from a structural Bernoulli process. Compare standard Poisson GLM predictions against true conditional means.
