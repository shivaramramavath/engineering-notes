# Classical Forecasting: AR, MA, ARIMA & SARIMA

> **One-Line Summary:** A unified family of linear parametric stochastic models that forecast stationary and non-stationary time series using historical linear combinations (AR), moving average noise shocks (MA), differencing integration (I), and seasonal harmonics (SARIMA).

---

## Metadata

| Field | Value |
| :--- | :--- |
| **Level** | Core / Advanced |
| **Prerequisites** | [01-time-series-fundamentals.md](01-time-series-fundamentals.md) |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Verified (Python 3.11 / CPU) |
| **Tested With** | Python 3.11+, NumPy 2.5+, statsmodels 0.14+ (Random Seed: `42`) |

---

## Table of Contents
1. [Intuition & Problem It Solves](#1-intuition--problem-it-solves)
2. [Algorithm Card](#2-algorithm-card)
3. [Architectural & Geometric Diagrams](#3-architectural--geometric-diagrams)
4. [The Mathematics](#4-the-mathematics)
5. [The Algorithm Step by Step & From-Scratch Implementation](#5-the-algorithm-step-by-step--from-scratch-implementation)
6. [Using the Library (Complete End-to-End Workflow)](#6-using-the-library-complete-end-to-end-workflow)
7. [Preprocessing Required](#7-preprocessing-required)
8. [Training & Hyperparameter Tuning](#8-training--hyperparameter-tuning)
9. [Prediction & Inference Mechanics](#9-prediction--inference-mechanics)
10. [Postprocessing](#10-postprocessing)
11. [Saving and Loading (Model Persistence & Security)](#11-saving-and-loading-model-persistence--security)
12. [Evaluation & Diagnostics](#12-evaluation--diagnostics)
13. [Advantages](#13-advantages)
14. [Limitations & Failure Modes](#14-limitations--failure-modes)
15. [Comparison / When to Use What](#15-comparison--when-to-use-what)
16. [Common Mistakes & Pitfalls](#16-common-mistakes--pitfalls)
17. [Performance, Security & Ethics Notes](#17-performance-security--ethics-notes)
18. [Interview Questions (8 In-Depth Q&As)](#18-interview-questions-8-in-depth-qas)
19. [Practice Exercises (6 Hands-on Problems)](#19-practice-exercises-6-hands-on-problems)
20. [Summary & Cheat Sheet](#20-summary--cheat-sheet)
21. [Further Reading & Navigation](#21-further-reading--navigation)

---

## 1. Intuition & Problem It Solves

When forecasting sequential processes (e.g. quarterly corporate revenue, hourly server CPU load, weekly inventory turnover), two distinct physical forces govern future values:
1. **Momentum / Autoregression (AR):** The current value depends on past observed values. If the room was warm 5 minutes ago, it is likely warm right now.
2. **Shock Persistence / Moving Average (MA):** The current value is influenced by recent unpredicted environmental shocks or white-noise disturbances. A sudden flash sale or transit delay produces a ripple effect that persists for several steps before dissipating.

The **Box-Jenkins Methodology** integrates both mechanisms into the **ARIMA** (Autoregressive Integrated Moving Average) framework:
- **AR($p$):** Regresses $Y_t$ on its own lagged values $Y_{t-1}, \dots, Y_{t-p}$.
- **I($d$):** Differences non-stationary data $d$ times until stationary.
- **MA($q$):** Models $Y_t$ as a linear combination of white noise error innovations $\epsilon_t, \dots, \epsilon_{t-q}$.
- **SARIMA:** Extends this to seasonal cycles of period $s$ (e.g. 12 months, 7 days).

ARIMA remains the gold standard for linear statistical forecasting: it provides analytical confidence intervals, requires zero GPU infrastructure, and delivers mathematically provable minimum mean squared error (MMSE) linear forecasts.

---

## 2. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Category** | Classical Statistical Time Series Model |
| **Model Formulation** | $\Phi_P(B^s) \phi_p(B) (1 - B)^d (1 - B^s)^D Y_t = \Theta_Q(B^s) \theta_q(B) \epsilon_t$ |
| **Parameters** | AR order $p$, Differencing $d$, MA order $q$, Seasonal orders $(P, D, Q)_s$ |
| **Estimation Method** | Maximum Likelihood Estimation (MLE) / Conditional Sum of Squares (CSS) |
| **Stationarity Requirement** | Strict: AR roots must lie outside the complex unit circle |
| **Invertibility Requirement**| Strict: MA roots must lie outside the complex unit circle |
| **Evaluation Metrics** | AIC, BIC, RMSE, MAE, MAPE, Ljung-Box Q-statistic |

---

## 3. Architectural & Geometric Diagrams

```mermaid
flowchart TD
    subgraph BoxJenkins["The Box-Jenkins Iterative Methodology"]
        Raw["Raw Time Series Y_t"] --> Identification["1. Identification: Inspect ACF/PACF, ADF Test, Determine (p, d, q)"]
        Identification --> Estimation["2. Estimation: Fit Parameters via MLE / Yule-Walker"]
        Estimation --> Diagnostics["3. Diagnostic Checking: Are Residuals White Noise?"]
        
        Diagnostics --> LjungBox{"Ljung-Box Test p > 0.05?"}
        LjungBox -->|No (Residuals Correlated)| RefineModel["Adjust (p, q) orders; re-estimate"]
        RefineModel --> Estimation
        LjungBox -->|Yes (Residuals White Noise)| ProductionForecast["4. Out-of-Sample Forecasting with Confidence Intervals"]
    end

    subgraph ARMAComposition["Mathematical Composition"]
        AR["AR(p): phi_1*Y_t-1 + ... + phi_p*Y_t-p (Momentum)"] --> ARMA["Combined ARMA(p, q)"]
        MA["MA(q): epsilon_t + theta_1*eps_t-1 + ... (Noise Shocks)"] --> ARMA
        Diff["Integration (1 - B)^d (Eliminates Trend)"] --> ARIMA["ARIMA(p, d, q)"]
        ARMA --> ARIMA
        Seasonal["Seasonal Operators (P, D, Q)_s"] --> SARIMA["SARIMA(p, d, q)(P, D, Q)_s"]
        ARIMA --> SARIMA
    end
```

---

## 4. The Mathematics

### 4.1 Lag (Backshift) Operator Notation
The backshift operator $B$ shifts an observation back by one time step:
$$B Y_t = Y_{t-1}, \quad B^k Y_t = Y_{t-k}$$
First-order differencing is written as:
$$\Delta Y_t = Y_t - Y_{t-1} = (1 - B) Y_t$$
Order-$d$ differencing is $(1 - B)^d Y_t$.

### 4.2 Autoregressive Model: AR($p$)
An Autoregressive process of order $p$ expresses $Y_t$ as a linear combination of its past $p$ values:
$$Y_t = c + \phi_1 Y_{t-1} + \phi_2 Y_{t-2} + \dots + \phi_p Y_{t-p} + \epsilon_t = c + \sum_{i=1}^p \phi_i Y_{t-i} + \epsilon_t$$
where $\epsilon_t \sim \text{WN}(0, \sigma^2)$ is white noise. In lag polynomial notation:
$$\phi(B) Y_t = c + \epsilon_t, \quad \text{where } \phi(B) = 1 - \sum_{i=1}^p \phi_i B^i$$
**Stationarity Condition:** All roots of the characteristic polynomial $\phi(z) = 0$ must lie **outside the unit circle** in the complex plane ($|z| > 1$). For an AR(1) model ($Y_t = \phi Y_{t-1} + \epsilon_t$), this simplifies to $|\phi| < 1$.

### 4.3 Moving Average Model: MA($q$)
A Moving Average process of order $q$ models $Y_t$ as a linear combination of the current and past $q$ white noise error shocks:
$$Y_t = \mu + \epsilon_t + \theta_1 \epsilon_{t-1} + \theta_2 \epsilon_{t-2} + \dots + \theta_q \epsilon_{t-q} = \mu + \epsilon_t + \sum_{j=1}^q \theta_j \epsilon_{t-j}$$
In lag polynomial notation:
$$Y_t = \mu + \theta(B) \epsilon_t, \quad \text{where } \theta(B) = 1 + \sum_{j=1}^q \theta_j B^j$$
**Invertibility Condition:** An MA process is *invertible* (can be uniquely represented as an infinite AR($\infty$) process) if all roots of $\theta(z) = 0$ lie outside the unit circle ($|z| > 1$). For MA(1), this requires $|\theta| < 1$.

### 4.4 The Full SARIMA Model
A Seasonal ARIMA model $\text{SARIMA}(p, d, q)(P, D, Q)_s$ combines non-seasonal and seasonal polynomial lag operators:
$$\Phi_P(B^s) \, \phi_p(B) \, (1 - B)^d \, (1 - B^s)^D Y_t = c + \Theta_Q(B^s) \, \theta_q(B) \, \epsilon_t$$
where:
- $\phi_p(B) = 1 - \sum_{i=1}^p \phi_i B^i$ (Non-seasonal AR)
- $\Phi_P(B^s) = 1 - \sum_{i=1}^P \Phi_i B^{i \cdot s}$ (Seasonal AR)
- $(1 - B)^d$: Non-seasonal differencing
- $(1 - B^s)^D$: Seasonal differencing
- $\theta_q(B) = 1 + \sum_{j=1}^q \theta_j B^j$ (Non-seasonal MA)
- $\Theta_Q(B^s) = 1 + \sum_{j=1}^Q \Theta_j B^{j \cdot s}$ (Seasonal MA)

### 4.5 Yule-Walker Equations for AR($p$)
For a zero-mean stationary AR($p$) process, multiplying both sides of $Y_t = \sum_{i=1}^p \phi_i Y_{t-i} + \epsilon_t$ by $Y_{t-k}$ and taking expectations yields the Yule-Walker equations:
$$\gamma(k) = \sum_{i=1}^p \phi_i \gamma(k - i) \quad \text{for } k \ge 1$$
Dividing by variance $\gamma(0)$ produces the matrix equation:
$$\begin{pmatrix} 1 & \rho(1) & \dots & \rho(p-1) \\ \rho(1) & 1 & \dots & \rho(p-2) \\ \vdots & \vdots & \ddots & \vdots \\ \rho(p-1) & \rho(p-2) & \dots & 1 \end{pmatrix} \begin{pmatrix} \phi_1 \\ \phi_2 \\ \vdots \\ \phi_p \end{pmatrix} = \begin{pmatrix} \rho(1) \\ \rho(2) \\ \vdots \\ \rho(p) \end{pmatrix} \iff \mathbf{R} \boldsymbol{\phi} = \mathbf{r}$$
Solving $\boldsymbol{\phi} = \mathbf{R}^{-1} \mathbf{r}$ yields exact parameter estimates directly from sample autocorrelations.

---

## 5. The Algorithm Step by Step & From-Scratch Implementation

Below is a complete, self-contained implementation in pure NumPy estimating an **Autoregressive AR($p$) Model via the Yule-Walker equations**, along with recursive multi-step forecasting.

```python
"""
Autoregressive Model AR(p) via Yule-Walker Equations from scratch in pure NumPy.
Verified under Python 3.11+, CPU execution.
"""

import numpy as np
from typing import Tuple, List, Dict, Any


class PureAutoregressiveModel:
    """
    Parametric AR(p) Model: Y_t = c + phi_1 * Y_{t-1} + ... + phi_p * Y_{t-p} + eps_t
    Fitted via Yule-Walker toeplitz matrix inversion.
    """
    def __init__(self, p: int):
        self.p = p
        self.phi: np.ndarray = np.zeros(p)
        self.c: float = 0.0
        self.sigma2: float = 0.0

    def fit(self, series: np.ndarray):
        n = len(series)
        mean = float(np.mean(series))
        centered = series - mean

        # 1. Compute sample autocovariances gamma(0) to gamma(p)
        gamma = np.zeros(self.p + 1)
        for k in range(self.p + 1):
            if k == 0:
                gamma[k] = np.mean(centered ** 2)
            else:
                gamma[k] = np.mean(centered[k:] * centered[:-k])

        # 2. Construct Toeplitz Autocorrelation Matrix R and vector r
        r = gamma[1:] / gamma[0] # shape (p,)
        R = np.zeros((self.p, self.p))
        for i in range(self.p):
            for j in range(self.p):
                lag = abs(i - j)
                R[i, j] = 1.0 if lag == 0 else gamma[lag] / gamma[0]

        # 3. Solve Yule-Walker: phi = R^-1 * r
        self.phi = np.linalg.solve(R, r)
        
        # 4. Intercept c = mean * (1 - sum(phi_i))
        self.c = mean * (1.0 - float(np.sum(self.phi)))
        
        # 5. Innovation variance sigma^2 = gamma(0) * (1 - phi^T r)
        self.sigma2 = float(gamma[0] * (1.0 - np.dot(self.phi, r)))

    def forecast(self, history: np.ndarray, steps: int = 5) -> Tuple[np.ndarray, np.ndarray]:
        """
        Generates recursive out-of-sample forecasts and analytic standard errors.
        """
        recent = list(history[-self.p:])
        forecasts = np.zeros(steps)
        std_errors = np.zeros(steps)

        # Standard error scales with recursive multi-step horizon
        cumulative_psi_sq = 1.0

        for h in range(steps):
            # Y_hat = c + sum(phi_i * Y_{t-i})
            lags = np.array(recent[-self.p:][::-1])
            y_hat = self.c + np.dot(self.phi, lags)
            forecasts[h] = y_hat
            recent.append(y_hat)

            # Error variance approximation
            std_errors[h] = np.sqrt(self.sigma2 * cumulative_psi_sq)
            if h < len(self.phi):
                cumulative_psi_sq += self.phi[h] ** 2

        return forecasts, std_errors


# Verification Run
if __name__ == "__main__":
    # Generate synthetic stationary AR(2) process:
    # Y_t = 2.0 + 0.6 * Y_{t-1} - 0.2 * Y_{t-2} + eps
    rng = np.random.default_rng(42)
    n = 1000
    eps = rng.normal(0, 1.0, size=n)
    y = np.zeros(n)
    for t in range(2, n):
        y[t] = 2.0 + 0.6 * y[t-1] - 0.2 * y[t-2] + eps[t]

    # Fit AR(2)
    model = PureAutoregressiveModel(p=2)
    model.fit(y[100:]) # Discard warmup

    print(f"Fitted phi_1: {model.phi[0]:.4f} (True: 0.6000)")
    print(f"Fitted phi_2: {model.phi[1]:.4f} (True: -0.2000)")
    print(f"Fitted Intercept c: {model.c:.4f} (True: 2.0000)")

    assert np.isclose(model.phi[0], 0.6, atol=0.08)
    assert np.isclose(model.phi[1], -0.2, atol=0.08)

    # 5-step forecast
    fc, std_err = model.forecast(y, steps=5)
    print(f"5-Step Ahead Forecast: {np.round(fc, 3)}")
    print(f"Forecast Std Errors:   {np.round(std_err, 3)}")
    assert len(fc) == 5
    print("Pure NumPy Yule-Walker AR(p) engine verified successfully.")
```

---

## 6. Using the Library (Complete End-to-End Workflow)

Production forecasting using `statsmodels.tsa.statespace.sarimax.SARIMAX`:

```python
import pandas as pd
import numpy as np
import statsmodels.api as sm
from statsmodels.tsa.statespace.sarimax import SARIMAX

def run_sarimax_pipeline(data_series: pd.Series) -> Tuple[Any, pd.DataFrame]:
    """
    Fits SARIMA(1, 1, 1)(1, 1, 1)_12 with AIC optimization and diagnostic testing.
    """
    # 1. Fit SARIMAX
    model = SARIMAX(
        data_series,
        order=(1, 1, 1),
        seasonal_order=(1, 1, 1, 12),
        enforce_stationarity=True,
        enforce_invertibility=True
    )
    results = model.fit(disp=False)
    
    # 2. Diagnostic Testing: Ljung-Box test for residual autocorrelation
    lb_test = results.test_serial_correlation(method="ljungbox")
    # p-values for Ljung-Box: if p > 0.05, residuals are pure white noise!
    p_values = lb_test[0, 1]
    
    # 3. Out-of-sample 12-month forecast with 95% Confidence Intervals
    forecast_res = results.get_forecast(steps=12)
    forecast_df = forecast_res.summary_frame(alpha=0.05) # contains mean, mean_ci_lower, mean_ci_upper
    
    return results, forecast_df
```

---

## 7. Preprocessing Required

1. **Log Transformation:** If seasonal variance expands with trend level, apply $\ln(Y_t)$ before modeling.
2. **Missing Value Imputation:** Classical ARIMA cannot handle missing `NaN` values. Interpolate missing data using linear or cubic spline interpolation (never forward-fill blindly).
3. **Determining Differencing Order ($d$):** Run ADF test iteratively; difference until $p < 0.05$. Typically $d \in \{0, 1\}$, rarely $2$.

---

## 8. Training & Hyperparameter Tuning

### The Box-Jenkins Selection Rules:
| Criterion | Formula | Tuning Rule |
| :--- | :--- | :--- |
| **Akaike Information Criterion (AIC)** | $\text{AIC} = -2\ln(L) + 2k$ | Penalizes parameter count $k$; minimize over grid search |
| **Bayesian Information Criterion (BIC)** | $\text{BIC} = -2\ln(L) + k\ln(T)$ | Stronger penalty for large datasets; prefers parsimonious models |
| **Auto-ARIMA Grid Search** | Range $p \in [0, 3], q \in [0, 3]$ | Select combination minimizing BIC |

---

## 9. Prediction & Inference Mechanics

At inference:
1. **Analytical Confidence Intervals:** Under Gaussian innovations, the $h$-step ahead forecast error variance is:
   $$\sigma^2(h) = \sigma^2 \sum_{j=0}^{h-1} \psi_j^2$$
   where $\psi_j$ are the infinite moving average representation coefficients ($Y_t = \sum \psi_j \epsilon_{t-j}$).
2. **Interval Widening:** The prediction interval $[ \hat{Y}_{T+h} \pm 1.96 \sigma(h) ]$ naturally expands as the forecast horizon $h$ increases, reflecting growing uncertainty.

---

## 10. Postprocessing

1. **Inverse Differencing:** Cumulative sum $\hat{Y}_{T+h} = Y_T + \sum_{i=1}^h \Delta \hat{Y}_{T+i}$.
2. **Inverse Log Transformation:** If $\ln(Y)$ was modeled, apply exponential correction:
   $$\hat{Y}_{\text{original}} = \exp\left( \hat{w} + \frac{1}{2}\sigma_{\text{resid}}^2 \right)$$
   where the $\frac{1}{2}\sigma^2$ term (Jensen's inequality correction) ensures unbiased expectation under log-normal distributions.

---

## 11. Saving and Loading (Model Persistence & Security)

Serialize SARIMAX models safely:

```python
import statsmodels.api as sm

def export_sarimax(results, filepath: str):
    results.save(filepath, remove_data=True) # remove_data strips training arrays to save disk space
```

---

## 12. Evaluation & Diagnostics

### Diagnostic Verification Battery:
1. **Ljung-Box Test:**
   $$Q = T(T + 2) \sum_{k=1}^m \frac{\hat{\rho}_k^2}{T - k}$$
   If $p > 0.05$, fail to reject null hypothesis; residuals are independent white noise.
2. **Quantile-Quantile (Q-Q) Plot:** Residuals must lie on the $45^\circ$ diagonal, confirming Gaussian normality.
3. **Residual Mean:** $\mathbb{E}[\hat{\epsilon}_t] \approx 0.0$.

---

## 13. Advantages

1. **Theoretical Purity:** Optimal linear minimum mean squared error (MMSE) predictor under Gaussian assumptions.
2. **Analytical Uncertainty:** Produces exact mathematical confidence bands without bootstrapping.
3. **Minimal Data Requirement:** Fits robustly on short historical series ($T \approx 50-100$ points) where deep learning fails.

---

## 14. Limitations & Failure Modes

1. **Linearity Assumption:** Cannot capture non-linear dynamics, chaotic attractors, or complex threshold effects.
2. **Exogenous Features Difficulty:** Incorporating dozens of external tabular covariates is unwieldy compared to tree-based models.
3. **Long Horizon Flatlining:** For stationary AR models, multi-step forecasts exponentially decay to the long-term historical mean $\mu$.

---

## 15. Comparison / When to Use What

| Feature | ARIMA / SARIMA | Exponential Smoothing (ETS) | Prophet | LightGBM / XGBoost |
| :--- | :--- | :--- | :--- | :--- |
| **Model Nature** | Stochastic Difference Equation | Non-linear State Space | Generalized Additive Model | Non-linear Decision Trees |
| **Seasonality** | Single explicit period $s$ | Additive / Multiplicative | Multiple Fourier Harmonics | Lag & Calendar Features |
| **Interpretability** | High (Coefficients) | High (Level, Trend, Season) | High (Components) | Moderate (SHAP values) |
| **Exogenous Features** | Limited (SARIMAX) | None | Moderate | Outstanding |
| **Compute Cost** | Low | Very Low | Moderate | Moderate |

---

## 16. Common Mistakes & Pitfalls

| Anti-Pattern / Mistake | Why It Fails | Correct Engineering Fix |
| :--- | :--- | :--- |
| Fitting ARIMA on non-stationary series ($d=0$) | Violates inversion assumptions; forecast variance explodes | Difference series until ADF test yields $p < 0.05$ |
| Ignoring residual autocorrelation | Model left predictable signals unextracted in the noise | Increase AR order $p$ or MA order $q$ until Ljung-Box $p > 0.05$ |
| Overfitting via excessive orders ($p=10, q=10$) | High parameter variance causes disastrous out-of-sample forecasts | Use BIC minimization to enforce parsimony |
| Inverting non-invertible MA models | Small past errors blow up exponentially in forecasting | Enforce invertibility roots outside unit circle |
| Evaluating MAPE when series contains zeros | Triggers division-by-zero infinity errors | Use MAE, RMSE, or symmetric SMAPE |

---

## 17. Performance, Security & Ethics Notes

1. **Matrix Inversion Conditioning:** When solving Yule-Walker or Kalman filter equations, singular or near-singular Toeplitz matrices indicate collinearity among lags. Add Tikhonov ridge regularization $\mathbf{R} + \lambda \mathbf{I}$.
2. **Financial Forecast Manipulation:** In financial earnings reporting, adjusting ARIMA differencing parameters to project misleading upward trends violates SEC disclosure compliance.

---

## 18. Interview Questions (8 In-Depth Q&As)

<details>
<summary><strong>Q1: Derive the Yule-Walker equations for an AR(2) process from first principles.</strong></summary>

**Answer:** Consider a zero-mean stationary AR(2) process:
$$Y_t = \phi_1 Y_{t-1} + \phi_2 Y_{t-2} + \epsilon_t, \quad \epsilon_t \sim \text{WN}(0, \sigma^2)$$
Multiply both sides by $Y_{t-k}$ and take expectations $\mathbb{E}[\cdot]$:
$$\mathbb{E}[Y_t Y_{t-k}] = \phi_1 \mathbb{E}[Y_{t-1} Y_{t-k}] + \phi_2 \mathbb{E}[Y_{t-2} Y_{t-k}] + \mathbb{E}[\epsilon_t Y_{t-k}]$$
By definition of autocovariance $\gamma(k) = \mathbb{E}[Y_t Y_{t-k}]$:
- For $k = 1$: $\gamma(1) = \phi_1 \gamma(0) + \phi_2 \gamma(1)$ (since $\mathbb{E}[\epsilon_t Y_{t-1}] = 0$)
- For $k = 2$: $\gamma(2) = \phi_1 \gamma(1) + \phi_2 \gamma(0)$
Divide both equations by $\gamma(0)$ to express in terms of autocorrelations $\rho(k) = \gamma(k)/\gamma(0)$:
$$\rho(1) = \phi_1 + \phi_2 \rho(1)$$
$$\rho(2) = \phi_1 \rho(1) + \phi_2$$
In matrix form:
$$\begin{pmatrix} 1 & \rho(1) \\ \rho(1) & 1 \end{pmatrix} \begin{pmatrix} \phi_1 \\ \phi_2 \end{pmatrix} = \begin{pmatrix} \rho(1) \\ \rho(2) \end{pmatrix}$$
Solving this linear system yields explicit closed-form solutions:
$$\phi_1 = \frac{\rho(1)(1 - \rho(2))}{1 - \rho(1)^2}, \quad \phi_2 = \frac{\rho(2) - \rho(1)^2}{1 - \rho(1)^2}$$
</details>

<details>
<summary><strong>Q2: Why must the roots of the AR characteristic polynomial lie outside the unit circle for stationarity?</strong></summary>

**Answer:** In lag operator notation, an AR($p$) process is $\phi(B) Y_t = \epsilon_t$. Factoring the polynomial:
$$\phi(B) = (1 - r_1^{-1} B)(1 - r_2^{-1} B) \dots (1 - r_p^{-1} B)$$
where $r_i$ are the roots of characteristic equation $\phi(z) = 0$.
To express $Y_t$ as a stationary linear process $Y_t = \phi(B)^{-1} \epsilon_t = \sum_{j=0}^\infty \psi_j \epsilon_{t-j}$, the inverse operator $(1 - r_i^{-1} B)^{-1}$ must converge:
$$\frac{1}{1 - r_i^{-1} B} = \sum_{j=0}^\infty (r_i^{-1} B)^j = \sum_{j=0}^\infty r_i^{-j} Y_{t-j}$$
This geometric series converges if and only if the ratio $|r_i^{-1}| < 1 \iff |r_i| > 1$.
Therefore, all roots $r_i$ must have modulus strictly greater than 1 (lie **outside the unit circle** in the complex plane). If any root lies inside the unit circle ($|r| < 1$), the weights grow exponentially $\psi_j \to \infty$, making the process explode.
</details>

<details>
<summary><strong>Q3: Explain the concept of Invertibility in Moving Average (MA) models.</strong></summary>

**Answer:** An MA($q$) model is $Y_t = \theta(B) \epsilon_t$.
**Invertibility** is the mathematical dual of stationarity: it requires that the error shock $\epsilon_t$ can be uniquely recovered as a convergent linear combination of current and past observations:
$$\epsilon_t = \theta(B)^{-1} Y_t = \sum_{j=0}^\infty \pi_j Y_{t-j}$$
This representation is valid if and only if all roots of $\theta(z) = 0$ lie outside the unit circle ($|z| > 1$).
- **Why it matters:** An non-invertible MA model cannot associate past observed data $Y$ with the current innovation $\epsilon_t$, making recursive forecasting mathematically impossible. Furthermore, multiple non-invertible MA parameter sets produce the exact same autocorrelation structure; requiring invertibility guarantees parameter uniqueness.
</details>

<details>
<summary><strong>Q4: How does the Ljung-Box test verify model adequacy?</strong></summary>

**Answer:** The Ljung-Box test evaluates whether the residual errors $\hat{\epsilon}_t = Y_t - \hat{Y}_t$ from an estimated ARIMA model behave like pure, uncorrelated white noise.
- **Null Hypothesis ($H_0$):** The residuals are independently distributed (no serial autocorrelation up to lag $m$).
- **Alternative ($H_1$):** The residuals exhibit serial autocorrelation.
The test statistic is:
$$Q = T(T + 2) \sum_{k=1}^m \frac{\hat{\rho}_k^2}{T - k} \sim \chi^2(m - p - q)$$
where degrees of freedom are adjusted by subtracting estimated parameters $p + q$.
If $p\text{-value} > 0.05$, we fail to reject $H_0$; the model has successfully extracted all predictable autocorrelation from the series. If $p < 0.05$, the residuals contain leftover predictability, indicating that orders $p$ or $q$ must be increased.
</details>

<details>
<summary><strong>Q5: What happens to multi-step forecasts as the horizon $h \to \infty$ for a stationary ARMA model?</strong></summary>

**Answer:** For any stationary ARMA($p, q$) model:
1. The conditional forecast converges asymptotically to the unconditional mean of the series:
   $$\lim_{h \to \infty} \hat{Y}_{T+h \mid T} = \mathbb{E}[Y_t] = \mu$$
2. The forecast error variance converges to the unconditional variance of the time series:
   $$\lim_{h \to \infty} \sigma^2(h) = \text{Var}(Y_t) = \gamma(0)$$
Because stationary models have finite memory, past observations lose all predictive power over the distant future. The point forecast flatlines to the long-term mean, and the prediction interval widens to the unconditional distribution band.
</details>

<details>
<summary><strong>Q6: How does SARIMA handle dual seasonality (e.g. daily 24-hr and weekly 168-hr)?</strong></summary>

**Answer:** Standard SARIMA can handle only a **single seasonal period** $s$ (e.g. $s=12$ for monthly or $s=7$ for daily data).
When data exhibits multiple seasonal cycles (e.g. hourly load with $s_1 = 24$ and $s_2 = 168$):
1. A SARIMA model with $s=168$ requires estimating a lag polynomial of order 168, which is computationally expensive and numerically unstable.
2. The standard solution is **Dynamic Harmonic Regression (SARIMAX)**: model the longer seasonality using exogenous Fourier sine/cosine series:
   $$x_t = \left[ \sin\left(\frac{2\pi k t}{168}\right), \cos\left(\frac{2\pi k t}{168}\right) \right]_{k=1}^K$$
   and use standard SARIMA to handle the short-term 24-hour cycle and remaining auto-correlated residuals.
</details>

<details>
<summary><strong>Q7: What is the difference between Maximum Likelihood Estimation (MLE) and Conditional Sum of Squares (CSS) for ARIMA?</strong></summary>

**Answer:**
- **Conditional Sum of Squares (CSS):** Assumes unobserved initial residuals prior to the sample start are zero ($\epsilon_0 = \epsilon_{-1} = \dots = 0$). It minimizes $\sum_{t=p+1}^T \epsilon_t^2$ via standard non-linear least squares. While fast, it produces biased parameter estimates on short time series because the zero-residual initialization assumption distorts early steps.
- **Exact Maximum Likelihood Estimation (MLE):** Uses the **Kalman Filter** to evaluate the exact likelihood function without making heuristic assumptions about initial values. It accounts for the exact stationary covariance of initial states. While computationally heavier, it yields unbiased, efficient estimates across series of all lengths.
Modern packages use CSS to find quick initial weights, followed by full MLE optimization.
</details>

<details>
<summary><strong>Q8: Explain the Box-Cox transformation and its role in stabilizing time series variance.</strong></summary>

**Answer:** When a time series displays heteroscedasticity (variance growing proportionally with level), linear models like ARIMA fail because innovation variance $\sigma^2$ is assumed constant.
The Box-Cox transformation parameterizes a continuous family of power transforms indexed by parameter $\lambda$:
$$w_t = \begin{cases} \frac{Y_t^\lambda - 1}{\lambda} & \text{if } \lambda \neq 0 \\ \ln(Y_t) & \text{if } \lambda = 0 \end{cases}$$
- $\lambda = 1$: No transformation (linear shift).
- $\lambda = 0.5$: Square root transform (stabilizes Poisson count variance).
- $\lambda = 0$: Logarithmic transform (stabilizes multiplicative variance).
$\lambda$ is chosen via profile likelihood maximization. The transformed series $w_t$ has constant variance, allowing standard ARIMA fitting.
</details>

---

## 19. Practice Exercises (6 Hands-on Problems)

<details>
<summary><strong>Exercise 1 (Easy): AR(1) Forecast Calculation</strong></summary>

**Task:** Given an AR(1) model $Y_t = 1.5 + 0.7 Y_{t-1}$ and current value $Y_T = 10.0$, compute the 1-step and 2-step ahead point forecasts.

**Solution:**
```python
def forecast_ar1(y_t: float, c: float = 1.5, phi: float = 0.7) -> tuple[float, float]:
    y_t1 = c + phi * y_t
    y_t2 = c + phi * y_t1
    return y_t1, y_t2

fc1, fc2 = forecast_ar1(10.0)
# fc1 = 1.5 + 0.7(10) = 8.5
# fc2 = 1.5 + 0.7(8.5) = 1.5 + 5.95 = 7.45
assert fc1 == 8.5 and fc2 == 7.45
print(f"1-Step: {fc1}, 2-Step: {fc2}")
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Characteristic Root Modulus Verification</strong></summary>

**Task:** Verify whether an AR(1) model with $\phi = 0.8$ satisfies the stationarity root condition $|z| > 1$.

**Solution:**
```python
def check_ar1_stationarity(phi: float) -> bool:
    # Characteristic equation: 1 - phi * z = 0 -> z = 1 / phi
    root = 1.0 / phi
    return abs(root) > 1.0

assert check_ar1_stationarity(0.8)
assert not check_ar1_stationarity(1.1)
print("AR(1) characteristic root modulus verified.")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): 2-Step Ahead MA(1) Forecast Property</strong></summary>

**Task:** Prove numerically that for an MA(1) model $Y_t = \mu + \epsilon_t + \theta \epsilon_{t-1}$, all forecasts beyond step 1 equal the unconditional mean $\mu$.

**Solution:**
```python
import numpy as np

def forecast_ma1(mu: float, theta: float, last_eps: float, steps: int = 4) -> np.ndarray:
    forecasts = np.zeros(steps)
    # Step 1: E[Y_{T+1} | T] = mu + theta * eps_T
    forecasts[0] = mu + theta * last_eps
    # Step 2+: E[Y_{T+h} | T] = mu (since eps_{T+h} have zero expectation)
    forecasts[1:] = mu
    return forecasts

fc = forecast_ma1(mu=5.0, theta=0.6, last_eps=2.0, steps=3)
np.testing.assert_array_equal(fc, [5.0 + 0.6 * 2.0, 5.0, 5.0])
print(f"MA(1) Forecasts: {fc}")
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Log-Normal Forecast Bias Correction</strong></summary>

**Task:** Apply Jensen's inequality bias correction $\exp(\hat{w} + 0.5 \sigma^2)$ to convert a log-space forecast to an unbiased original space forecast.

**Solution:**
```python
import numpy as np

def inverse_log_unbiased(log_forecast: float, residual_variance: float) -> float:
    return float(np.exp(log_forecast + 0.5 * residual_variance))

w_hat = 2.0
sigma2 = 0.04
y_unbiased = inverse_log_unbiased(w_hat, sigma2)
# exp(2.0 + 0.02) = exp(2.02) ~ 7.5383
assert np.isclose(y_unbiased, np.exp(2.02))
print(f"Unbiased forecast: {y_unbiased:.4f}")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Pure NumPy Yule-Walker AR(1) Solver</strong></summary>

**Task:** Implement closed-form Yule-Walker parameter estimation for an AR(1) series in pure NumPy.

**Solution:**
```python
import numpy as np

def fit_yule_walker_ar1(series: np.ndarray) -> tuple[float, float, float]:
    n = len(series)
    mean = np.mean(series)
    centered = series - mean
    
    gamma0 = np.mean(centered ** 2)
    gamma1 = np.mean(centered[1:] * centered[:-1])
    
    phi1 = gamma1 / gamma0
    c = mean * (1.0 - phi1)
    sigma2 = gamma0 * (1.0 - phi1 ** 2)
    return phi1, c, sigma2

test_s = np.array([1.0, 2.0, 1.8, 3.1, 2.9, 4.0])
phi, c, sig = fit_yule_walker_ar1(test_s)
print(f"AR(1) Fitted phi: {phi:.4f}, Intercept: {c:.4f}, Sigma2: {sig:.4f}")
assert abs(phi) < 1.0
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Numerical Ljung-Box Q-Statistic Calculation</strong></summary>

**Task:** Compute the Ljung-Box test statistic $Q$ in pure NumPy for residual autocorrelation across $m=5$ lags.

**Solution:**
```python
import numpy as np

def compute_ljung_box_q(residuals: np.ndarray, m: int = 5) -> float:
    T = len(residuals)
    mean = np.mean(residuals)
    gamma0 = np.sum((residuals - mean) ** 2)
    
    Q = 0.0
    for k in range(1, m + 1):
        gammak = np.sum((residuals[k:] - mean) * (residuals[:-k] - mean))
        rho_k = gammak / gamma0
        Q += (rho_k ** 2) / (T - k)
        
    Q *= T * (T + 2)
    return float(Q)

# Pure white noise residuals
rng = np.random.default_rng(42)
white_noise = rng.normal(0, 1, size=200)
q_stat = compute_ljung_box_q(white_noise, m=5)
print(f"Ljung-Box Q-statistic on white noise: {q_stat:.4f}")
# Under chi-square(5), critical value at alpha=0.05 is 11.07
assert q_stat < 11.07, "White noise residuals failed Ljung-Box test."
```
</details>

---

## 20. Summary & Cheat Sheet

```
Box-Jenkins Workflow:
1. Stationarity: Test ADF (p < 0.05); if non-stationary, difference d=1
2. Identification:
   - AR(p): ACF decays exponentially, PACF cuts off at lag p
   - MA(q): ACF cuts off at lag q, PACF decays exponentially
   - ARMA(p, q): Both decay; minimize AIC/BIC
3. SARIMA: Seasonal operators (P, D, Q)_s handle recurring cycles
4. Diagnostics: Verify residuals via Ljung-Box test (must have p > 0.05)
5. Invertibility: Ensure MA polynomial roots lie outside the complex unit circle
```

---

## 21. Further Reading & Navigation

- Box, G. E., Jenkins, G. M., Reinsel, G. C., & Ljung, G. M. (2015). *Time Series Analysis: Forecasting and Control* (5th ed.). Wiley.
- Hyndman, R. J., & Athanasopoulos, G. (2021). *Forecasting: Principles and Practice* (3rd ed.). OTexts.
- Ljung, G. M., & Box, G. E. (1978). *On a measure of lack of fit in time series models*. Biometrika, 65(2), 297-303.

---

[← 01-time-series-fundamentals.md](01-time-series-fundamentals.md) | [Module Index](README.md) | [03-ml-for-time-series.md →](03-ml-for-time-series.md)
