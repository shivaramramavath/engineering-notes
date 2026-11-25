---
title: "Regression Diagnostics: Residuals, Leverage, and Hypothesis Testing"
description: "Comprehensive statistical diagnostic suite for regression models: residual analysis, Breusch-Pagan heteroscedasticity, Cook's distance, VIF, and Durbin-Watson."
track: "machine-learning"
module: "03-supervised-regression"
topic: "Regression Diagnostics"
order: 11
format: "concept"
level: "intermediate"
tags: ["regression-diagnostics", "residuals", "cooks-distance", "heteroscedasticity", "breusch-pagan", "durbin-watson", "vif"]
prerequisites: [
  "05-ai/machine-learning/01-math-foundations/04-statistics-and-hypothesis-testing.md",
  "05-ai/machine-learning/03-supervised-regression/01-linear-regression.md"
]
key_takeaways:
  - "Regression models cannot be evaluated solely on R^2 or RMSE; residual diagnostics verify whether mathematical assumptions are satisfied."
  - "The Breusch-Pagan and White tests formally test for heteroscedasticity; non-constant variance invalidates standard errors and p-values."
  - "Cook's distance combines residual magnitude with leverage (h_ii) to identify single points that exert disproportionate pull on regression coefficients."
  - "The Durbin-Watson test detects first-order autocorrelation in residuals, identifying temporal dependency violations."
---

# Regression Diagnostics: Residuals, Leverage, and Hypothesis Testing

## 1. Context & Problem Statement

In applied machine learning, high $R^2$ or low RMSE does not guarantee a valid, reliable model. A linear model can report an $R^2$ of $0.92$ while completely violating the underlying Gauss-Markov assumptions:
1. **Unidentified Non-Linearity**: A straight line fitted to a parabolic curve can have high $R^2$, but systematic positive residuals at the boundaries and negative residuals in the center prove the functional form is misspecified.
2. **Spurious Precision via Heteroscedasticity**: If residual variance expands with target magnitude, standard error formulas underestimate uncertainty in volatile regimes, yielding dangerously overconfident predictions.
3. **Fragility from Influential Outliers**: A single rogue sensor measurement with high leverage can completely invert the slope of a regression hyperplane.

Regression diagnostics provide a rigorous statistical battery to verify model assumptions, detect data pathologies, and guide structural remediation.

---

## 2. Conceptual Architecture

The diagnostic protocol progresses through four fundamental statistical checks: Functional Form, Residual Sphericality, Multicollinearity, and Outlier Leverage.

```
+-----------------------------------------------------------------------------------+
|                        REGRESSION DIAGNOSTIC SUITE                                |
+-----------------------------------------------------------------------------------+
                                          |
          +-------------------------------+-------------------------------+
          |                                                               |
          v                                                               v
[Residual Structure & Sphericality]                             [Influential Points & Geometry]
  - Residuals vs Fitted (Linearity check)                         - Hat Matrix Diagonal h_ii (Leverage)
  - Scale-Location Plot (Homoscedasticity)                        - Studentized / Standardized Residuals
  - Breusch-Pagan & White Tests                                   - Cook's Distance D_i (Pull on w)
  - Normal Q-Q & Jarque-Bera (Normality)                          - Durbin-Watson (Autocorrelation)
  - Variance Inflation Factor (VIF)
```

---

## 3. Core Theory & Mathematical Formulation

### 3.1 Leverage and the Hat Matrix

The Hat matrix $H = X (X^T X)^{-1} X^T$ projects target $\mathbf{y}$ onto predictions $\hat{\mathbf{y}} = H \mathbf{y}$.
The diagonal elements $h_{ii} = H_{ii} = \mathbf{x}_i^T (X^T X)^{-1} \mathbf{x}_i$ quantify the **leverage** of observation $i$:
- $h_{ii}$ measures the Euclidean/Mahalanobis distance of $\mathbf{x}_i$ from the centroid $\bar{\mathbf{x}}$ of the feature space.
- $\sum_{i=1}^N h_{ii} = \text{trace}(H) = D + 1$ (Degrees of Freedom).
- Average leverage is $\bar{h} = \frac{D + 1}{N}$.
- An observation is conventionally flagged as a **high-leverage point** if:
  $$h_{ii} > 2 \cdot \frac{D + 1}{N} \quad \text{or} \quad h_{ii} > 3 \cdot \frac{D + 1}{N}$$

### 3.2 Studentized Residuals

Raw residuals $e_i = y_i - \hat{y}_i$ do not have equal variance; their variance depends on leverage:
$$\text{Var}(e_i) = \sigma^2 (1 - h_{ii})$$
Points with high leverage naturally have smaller raw residuals because they pull the line toward themselves.
The **Internally Studentized Residual** $r_i$ normalizes for this variance:

$$r_i = \frac{e_i}{\hat{\sigma} \sqrt{1 - h_{ii}}} \quad \text{where } \hat{\sigma}^2 = \frac{\sum e_i^2}{N - D - 1}$$

Points with $|r_i| > 3.0$ are statistically significant outliers.

### 3.3 Cook's Distance ($D_i$)

Cook's Distance measures the aggregate shift in all $N$ fitted values when observation $i$ is deleted from the training set:

$$D_i = \frac{\sum_{j=1}^N (\hat{y}_j - \hat{y}_{j(-i)})^2}{(D + 1) \hat{\sigma}^2} = \frac{r_i^2}{D + 1} \left( \frac{h_{ii}}{1 - h_{ii}} \right)$$

Cook's distance is the product of two components:
1. **Residual Error ($r_i^2$)**: How poorly the model predicts observation $i$.
2. **Leverage Ratio ($\frac{h_{ii}}{1 - h_{ii}}$)**: How isolated $\mathbf{x}_i$ is in feature space.

**Threshold Rule of Thumb**: An observation is highly influential if $D_i > \frac{4}{N}$ or $D_i > 1.0$.

### 3.4 Formal Hypothesis Tests

| Pathology | Statistical Test | Null Hypothesis $H_0$ | Test Statistic & Distribution | Action on Rejection ($p < 0.05$) |
| :--- | :--- | :--- | :--- | :--- |
| **Heteroscedasticity** | **Breusch-Pagan** | Homoscedastic error variance ($\sigma_i^2 = \sigma^2$) | $LM = N \cdot R^2_{e^2 \sim X} \sim \chi^2_D$ | Apply Box-Cox / Log on $y$; use Huber/WLS |
| **Autocorrelation** | **Durbin-Watson** | No first-order serial correlation ($e_t \perp e_{t-1}$) | $d = \frac{\sum (e_t - e_{t-1})^2}{\sum e_t^2} \approx 2(1 - r_1)$ | Add lag features; use TimeSeriesSplit |
| **Non-Normality** | **Jarque-Bera** | Normal skewness ($0$) and excess kurtosis ($0$) | $JB = \frac{N}{6} \left( g_1^2 + \frac{1}{4} g_2^2 \right) \sim \chi^2_2$ | Use non-parametric quantiles or bootstrap |

---

## 4. Practical Implementation

Below is a complete, production-grade statistical diagnostic engine in Python that computes leverage, Studentized residuals, Cook's distance, Breusch-Pagan, and Durbin-Watson tests without relying on statsmodels.

```python
"""
Comprehensive Regression Diagnostics Suite in Pure NumPy/SciPy.
Tested on Python 3.11, NumPy 2.x, SciPy 1.x.
"""

from typing import Dict, List, Tuple
import numpy as np
import pandas as pd
from scipy import stats


class RegressionDiagnosticsEngine:
    """Computes leverage, studentized residuals, Cook's distance, and tests."""

    def __init__(self, X: np.ndarray, y: np.ndarray, coef: np.ndarray, intercept: float):
        self.N, self.D = X.shape
        self.X_design = np.hstack([np.ones((self.N, 1)), X])
        self.y = y
        self.w = np.hstack([intercept, coef])
        self.p = self.D + 1  # Parameters count including intercept

        # Predictions and raw residuals
        self.y_hat = self.X_design @ self.w
        self.residuals = self.y - self.y_hat

        # Residual variance estimate
        self.rss = np.sum(self.residuals ** 2)
        self.df_e = self.N - self.p
        self.sigma_sq = self.rss / self.df_e
        self.sigma = np.sqrt(self.sigma_sq)

        # Hat Matrix Diagonals (Leverage)
        # h_ii = diag(X (X^T X)^(-1) X^T)
        XtX_inv = np.linalg.pinv(self.X_design.T @ self.X_design)
        self.leverage = np.sum((self.X_design @ XtX_inv) * self.X_design, axis=1)

    def compute_studentized_residuals(self) -> np.ndarray:
        """Computes internally studentized residuals: e_i / (sigma * sqrt(1 - h_ii))."""
        denom = self.sigma * np.sqrt(np.clip(1.0 - self.leverage, 1e-7, 1.0))
        return self.residuals / denom

    def compute_cooks_distance(self) -> np.ndarray:
        """Computes Cook's distance D_i = (r_i^2 / p) * (h_ii / (1 - h_ii))."""
        stud_res = self.compute_studentized_residuals()
        leverage_factor = self.leverage / np.clip(1.0 - self.leverage, 1e-7, 1.0)
        cooks_d = (stud_res ** 2 / self.p) * leverage_factor
        return cooks_d

    def test_breusch_pagan(self) -> Tuple[float, float]:
        """Breusch-Pagan test for heteroscedasticity: regresses e^2 on X."""
        e_sq = self.residuals ** 2
        # Fit OLS of e^2 on X_design
        w_aux = np.linalg.pinv(self.X_design) @ e_sq
        e_sq_pred = self.X_design @ w_aux
        
        # R^2 of auxiliary regression
        r2_aux = 1.0 - (np.sum((e_sq - e_sq_pred)**2) / np.sum((e_sq - np.mean(e_sq))**2))
        lm_stat = self.N * r2_aux
        p_value = 1.0 - stats.chi2.cdf(lm_stat, df=self.D)
        return float(lm_stat), float(p_value)

    def test_durbin_watson(self) -> float:
        """Computes Durbin-Watson statistic for autocorrelation: sum((e_t - e_{t-1})^2) / sum(e_t^2)."""
        diff = np.diff(self.residuals)
        dw_stat = np.sum(diff ** 2) / np.sum(self.residuals ** 2)
        return float(dw_stat)


# Verification Run
if __name__ == "__main__":
    np.random.seed(42)
    N = 200
    X = np.random.randn(N, 2)
    
    # Inject heteroscedasticity: error variance grows with X[:, 0]
    errors = np.random.normal(0, 0.5 + np.abs(X[:, 0]) * 1.5, size=N)
    y = 3.0 * X[:, 0] - 2.0 * X[:, 1] + 1.0 + errors

    # Add 1 high-leverage extreme outlier
    X[0] = [10.0, 10.0]
    y[0] = -50.0 # Extreme negative outlier at high leverage!

    # Fit simple OLS
    X_d = np.hstack([np.ones((N, 1)), X])
    w = np.linalg.pinv(X_d) @ y

    diag = RegressionDiagnosticsEngine(X, y, coef=w[1:], intercept=w[0])

    cooks_d = diag.compute_cooks_distance()
    bp_stat, bp_p = diag.test_breusch_pagan()
    dw_stat = diag.test_durbin_watson()

    print("--- Regression Diagnostic Report ---")
    print(f"Breusch-Pagan Stat:  {bp_stat:.4f} (p-value: {bp_p:.4e})")
    print(f"Durbin-Watson Stat:   {dw_stat:.4f} (Ideal: ~2.0)")
    print(f"Sample 0 Leverage:    {diag.leverage[0]:.4f} (Mean Leverage: {diag.p/N:.4f})")
    print(f"Sample 0 Cook's Dist: {cooks_d[0]:.4f} (Threshold 4/N: {4.0/N:.4f})")

    # Verifications
    # 1. Breusch-Pagan should reject homoscedasticity (p < 0.05)
    assert bp_p < 0.05, "Breusch-Pagan should detect heteroscedasticity."
    # 2. Sample 0 must be flagged as a severe Cook's distance outlier
    assert cooks_d[0] > 4.0 / N, "Sample 0 should exceed Cook's threshold."
    # 3. Durbin-Watson on independent samples should be near 2.0
    assert 1.7 < dw_stat < 2.3

    print("\n[SUCCESS] Regression diagnostics test harness passed.")
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

| Failure Mode | Root Cause | Observable Symptom | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **Masking of Influential Clusters** | Two adjacent high-leverage outliers pull the line together | Individual Cook's distances appear moderate; neither is flagged | Compute group Cook's distance or use robust estimators (RANSAC). |
| **False Non-Linearity in Q-Q Plot** | Heavy-tailed t-distributed errors treated as non-linear target | S-shaped Q-Q plot; Shapiro-Wilk rejects normality | Use Huber loss or Quantile Regression rather than transforming $y$. |
| **Autocorrelation Confusion** | Shuffled time-series data destroys temporal order | Durbin-Watson reports $d \approx 2.0$ despite severe underlying serial correlation | Strictly evaluate Durbin-Watson on chronologically sorted residuals. |
| **VIF Singular Inversion** | Two features have correlation $r = 1.000$ | Division by zero yields $\text{VIF} = \infty$ | Drop one duplicate feature via `VarianceThreshold` or correlation audit. |
| **Heteroscedasticity False Alarm** | Single extreme residual inflates auxiliary regression | Breusch-Pagan p-value $< 0.001$ due to single point | Inspect residual scatter plot before concluding true heteroscedasticity. |

---

## 6. Production Engineering & Performance

1. **Automated Continuous Retraining Gates**:
   Do not deploy newly trained regression pipelines based solely on test $R^2$. Establish automated gating assertions:
   - $\text{Breusch-Pagan } p > 0.01$ (Homoscedasticity check).
   - $\max(D_i) < 1.0$ (Zero catastrophic leverage outliers).
   - $\text{VIF}_{\max} < 5.0$ (Multicollinearity check).
   If any assertion fails, halt deployment and alert data engineers.
2. **Computational Scaling of Hat Matrix**:
   Computing the exact diagonal leverage $h_{ii}$ takes $\mathcal{O}(N D^2)$. For $N > 10^7$, subsample $100,000$ rows to evaluate diagnostic distributions.

---

## 7. Comparative Analysis: Diagnostic Tests

| Test | Evaluates | Null Hypothesis | Typical Failure Cause |
| :--- | :--- | :--- | :--- |
| **Breusch-Pagan** | Error variance | Homoscedasticity | Target spans multiple orders of magnitude |
| **White Test** | Non-linear variance | Homoscedasticity + No cross-term variance | Interaction terms in errors |
| **Durbin-Watson** | Serial correlation | Residuals are un-autocorrelated ($r_1 = 0$) | Time-series trend omitted |
| **Jarque-Bera** | Error distribution | Normality (Skew=0, Kurt=3) | Outliers, skewed real-world target |
| **Cook's Distance** | Point influence | Point has negligible effect on $\mathbf{w}$ | Data entry error, rare extreme event |

---

## 8. Verification & Diagnostics Test Harness

```python
import pytest
import numpy as np

def test_durbin_watson_perfect_anticorrelation():
    # Alternating residuals: [1, -1, 1, -1] -> d should approach 4.0
    residuals = np.array([1.0, -1.0, 1.0, -1.0, 1.0, -1.0])
    diff = np.diff(residuals)
    dw = np.sum(diff ** 2) / np.sum(residuals ** 2)
    assert dw > 3.0

def test_leverage_sum_equals_d_plus_one():
    # Sum of leverage values must mathematically equal p = D + 1
    X = np.random.randn(50, 3)
    X_d = np.hstack([np.ones((50, 1)), X])
    XtX_inv = np.linalg.pinv(X_d.T @ X_d)
    leverage = np.sum((X_d @ XtX_inv) * X_d, axis=1)
    assert np.isclose(np.sum(leverage), 4.0)
```

---

## 9. Cross-Track Connections

- **05-ai/machine-learning/01-math-foundations/04-statistics-and-hypothesis-testing.md**: Chi-square tests, t-tests, and p-values.
- **05-ai/machine-learning/02-data-and-preprocessing/03-exploratory-data-analysis.md**: Variance Inflation Factor (VIF).
- **05-ai/machine-learning/03-supervised-regression/01-linear-regression.md**: Gauss-Markov assumptions.

---

## 10. Real-World Case Study: Commercial Airline Fuel Consumption Model

An airline modeled fuel burn ($y$ in tons) per transatlantic flight based on payload, cruising altitude, and headwind velocity.
- **Problem**: The model had $R^2 = 0.94$. However, on long flights with heavy cargo, fuel estimates were systematically short by $12\%$, risking emergency diversions.
- **Diagnostic Discovery**:
  1. The Breusch-Pagan test failed decisively ($p = 10^{-14}$); residual variance expanded quadratically with payload.
  2. Cook's distance flagged 3 flights that flew through Hurricane Fiona as extreme leverage points that had distorted the headwind coefficient.
- **Remediation**:
  1. Removed the 3 hurricane outlier flights from the baseline training set.
  2. Replaced OLS with a Gamma GLM (`GammaRegressor(power=2)`), natively modeling the proportional variance.
  3. Prediction bias on heavy flights dropped to $< 0.4\%$, eliminating emergency fuel stops.

---

## 11. Technical Interview Questions & Solutions

### Question 1: What is the exact mathematical difference between an "Outlier", a "High Leverage Point", and an "Influential Point"?
**Answer**:
- **Outlier**: A data point whose response $y_i$ deviates substantially from the fitted model prediction (large studentized residual $|r_i| \gg 3$). An outlier can occur at a normal feature position.
- **High Leverage Point**: A data point whose feature coordinates $\mathbf{x}_i$ are extreme/isolated in feature space (large leverage $h_{ii} \gg 2p/N$). A high-leverage point does **not** have to be an outlier if its $y_i$ falls naturally along the regression plane.
- **Influential Point**: A point that is **simultaneously an outlier AND has high leverage**. Its presence significantly alters the estimated regression parameters $\mathbf{w}$ (measured by Cook's Distance $D_i > 4/N$).

### Question 2: Why do high-leverage points naturally produce smaller raw residuals $e_i$?
**Answer**:
The raw residual is $e_i = y_i - \hat{y}_i$.
Since $\hat{\mathbf{y}} = H \mathbf{y}$, the residual vector is $\mathbf{e} = (\mathbf{I} - H) \mathbf{y}$.
The variance of the $i$-th residual is:
$$\text{Var}(e_i) = \sigma^2 (1 - h_{ii})$$
As leverage $h_{ii} \to 1$, the variance $\text{Var}(e_i) \to 0$. A point with leverage close to 1 forces the regression hyperplane to pass directly through itself, resulting in a raw residual near zero. This is why raw residuals cannot be used to detect outliers; one must use **Studentized residuals** $r_i = \frac{e_i}{\sigma \sqrt{1 - h_{ii}}}$.

### Question 3: How does the Durbin-Watson statistic detect autocorrelation, and what do values of 0, 2, and 4 mean?
**Answer**:
The Durbin-Watson statistic is:
$$d = \frac{\sum_{t=2}^N (e_t - e_{t-1})^2}{\sum_{t=1}^N e_t^2} \approx \frac{\sum e_t^2 + \sum e_{t-1}^2 - 2 \sum e_t e_{t-1}}{\sum e_t^2} \approx 2(1 - r_1)$$
where $r_1$ is the lag-1 sample autocorrelation of residuals.
- If $r_1 = 0$ (no autocorrelation): $d \approx 2(1 - 0) = 2.0$.
- If $r_1 = +1$ (perfect positive autocorrelation): $d \approx 2(1 - 1) = 0.0$.
- If $r_1 = -1$ (perfect negative autocorrelation): $d \approx 2(1 - (-1)) = 4.0$.
Values between $1.5$ and $2.5$ generally indicate acceptable independence.

---

## 12. Guided Practice & Hands-On Exercises

### Exercise 1: 4-Panel Diagnostic Dashboard
- Write a plotting script that takes a fitted scikit-learn model and outputs the standard 4-panel diagnostic dashboard: Residuals vs Fitted, Normal Q-Q, Scale-Location, and Residuals vs Leverage with Cook's distance contours.

### Exercise 2: Breusch-Pagan Test from Scratch
- Implement the Breusch-Pagan test in pure Python and verify that it rejects homoscedasticity on $y = x + x \cdot \epsilon$ while accepting it on $y = x + \epsilon$.

### Exercise 3: Automated High-Influence Pruning Pipeline
- Build a custom scikit-learn transformer `InfluenceFilter` that calculates Cook's distance during `fit()` and automatically drops training samples with $D_i > 4/N$ before fitting the final estimator.
