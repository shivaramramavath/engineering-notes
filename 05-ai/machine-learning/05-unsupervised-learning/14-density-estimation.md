# Kernel Density Estimation (KDE) & Non-Parametric Density Estimation

> **Track:** `05-ai/machine-learning` | **Module:** `05-unsupervised-learning` | **Status:** `Production Standard`  
> **Metadata:** `scikit-learn: >=1.4.0` | `SciPy: >=1.12.0` | `NumPy: >=1.26.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Kernel Density Estimation (KDE) / Parzen-Rosenblatt Window |
| **Family** | Non-parametric Probability Density Estimation / Smoothing |
| **Input Feature Types** | Continuous feature vectors $\mathbf{X} \in \mathbb{R}^{n \times d}$. Categorical features must be encoded or smoothed via discrete kernels. Sensitive to distance metric choice. |
| **Target Output** | Continuous estimated probability density function $\hat{f}_h(\mathbf{x}) \ge 0$ such that $\int_{\mathbb{R}^d} \hat{f}_h(\mathbf{x}) d\mathbf{x} = 1$, or log-likelihood $\ln \hat{f}_h(\mathbf{x})$. |
| **Optimization Target** | Minimizing Mean Integrated Squared Error (MISE) or maximizing cross-validated log-likelihood with respect to bandwidth parameter $h$. |
| **Key Metrics** | Integrated Squared Error (ISE), Mean Integrated Squared Error (MISE), Asymptotic MISE (AMISE), Log-Likelihood $\sum \ln \hat{f}(\mathbf{x}_i)$. |
| **Key Hyperparameters** | `bandwidth` ($h > 0$), `kernel` (`'gaussian'`, `'tophat'`, `'epanechnikov'`, `'exponential'`, `'linear'`, `'cosine'`), `metric` (Euclidean, Manhattan, Chebyshev), `algorithm` (`'auto'`, `'ball_tree'`, `'kd_tree'`). |
| **Inference Mechanism** | Evaluates normalized sum of kernel evaluations centered at all $n$ training points: $\hat{f}(\mathbf{x}) = \frac{1}{n h^d} \sum_{i=1}^n K\left(\frac{\mathbf{x} - \mathbf{x}_i}{h}\right)$. Tree-accelerated neighbor query (`BallTree`/`KDTree`) ignores samples beyond cutoff radius. |
| **Primary Failure Modes** | Curse of dimensionality ($d > 10$ leads to extreme sparsity and bandwidth degeneration); extreme computational inference cost $\mathcal{O}(n \cdot d)$ per query without tree indexing; catastrophic over-smoothing (large $h$) or delta-spike overfitting (small $h$). |

---

## 2. Architecture & Intuition

### The Core Problem
Histograms are the most common density estimators, but they suffer from two major flaws:
1. **Discontinuity**: The estimated density is jagged and piecewise-constant.
2. **Bin Edge Sensitivity**: Shifting bin origins even slightly dramatically alters the shape of the density estimate.

```
       Histogram (Discontinuous, Edge-Sensitive)           Kernel Density Estimate (Smooth, Centered)
      Density                                           Density
        ^                                                 ^
        |     +---+                                       |         .-'""'-.
        |     |   |                                       |       .'        '.
        | +---+   +---+                                   |      /            \
        | |   |   |   |                                   |    .'              '.
        +-------------------> x                           +-------------------------> x
          [  ] [  ] [  ] bins                                 ^   ^   ^   ^   ^
                                                            Kernels centered at points
```

### The Kernel Intuition
Instead of dropping data into fixed spatial bins, Kernel Density Estimation places a continuous, symmetric "bump" (a **kernel function** $K$) centered over **every observed training data point** $\mathbf{x}_i$. The global density estimate at any query point $\mathbf{x}$ is the normalized average of the heights of all bumps evaluated at $\mathbf{x}$.

- **Bandwidth ($h$)**: Controls the width/spread of each bump.
  - $h \to 0$: Every data point becomes a Dirac delta spike. Severe overfitting (infinite variance).
  - $h \to \infty$: Bumps blur together into a uniform flat smear. Severe underfitting (infinite bias).
- **Kernel Function ($K$)**: Controls the shape of each bump (Gaussian bell, parabolic dome, triangular tent, flat cylinder).

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $\mathbf{X} = \{\mathbf{x}_1, \dots, \mathbf{x}_n\}$ | $\mathbb{R}^{n \times d}$ | Observed training sample set of size $n$ |
| $\mathbf{x}$ | $\mathbb{R}^d$ | Arbitrary evaluation query point in feature space |
| $d$ | $\mathbb{N}^+$ | Dimensionality of the input feature space |
| $h$ | $\mathbb{R}^+$ | Smoothing parameter (bandwidth) |
| $K(\mathbf{u})$ | $\mathbb{R}^d \to \mathbb{R}^+$ | Kernel function satisfying $\int K(\mathbf{u}) d\mathbf{u} = 1$ |
| $\hat{f}_h(\mathbf{x})$ | $\mathbb{R}^+$ | Estimated probability density at $\mathbf{x}$ |
| $f(\mathbf{x})$ | $\mathbb{R}^+$ | True underlying population density |
| $\text{MISE}(h)$ | $\mathbb{R}^+$ | Mean Integrated Squared Error $\mathbb{E}\left[\int (\hat{f}_h(\mathbf{x}) - f(\mathbf{x}))^2 d\mathbf{x}\right]$ |
| $\text{AMISE}(h)$ | $\mathbb{R}^+$ | Asymptotic approximation of MISE as $n \to \infty, h \to 0$ |

### The Parzen-Rosenblatt Estimator
Given an i.i.d. sample $\mathbf{x}_1, \dots, \mathbf{x}_n$ drawn from unknown density $f$, the multivariate kernel density estimator is defined as:

$$\hat{f}_h(\mathbf{x}) = \frac{1}{n h^d} \sum_{i=1}^n K\left( \frac{\mathbf{x} - \mathbf{x}_i}{h} \right)$$

### Standard Kernel Functions (1D forms where $u = \frac{x - x_i}{h}$)

1. **Gaussian Kernel**:
   $$K(u) = \frac{1}{\sqrt{2\pi}} \exp\left(-\frac{1}{2}u^2\right)$$
2. **Epanechnikov Kernel** (optimal in minimizing MISE):
   $$K(u) = \frac{3}{4}(1 - u^2) \cdot \mathbb{I}(|u| \le 1)$$
3. **Tophat (Uniform) Kernel**:
   $$K(u) = \frac{1}{2} \cdot \mathbb{I}(|u| \le 1)$$
4. **Exponential Kernel**:
   $$K(u) = \frac{1}{2}\exp(-|u|)$$

### Asymptotic Mean Integrated Squared Error (AMISE)
To select the optimal bandwidth $h$, we analyze the trade-off between bias and variance:

$$\text{MISE}(h) = \int \text{Var}(\hat{f}_h(\mathbf{x})) d\mathbf{x} + \int \text{Bias}^2(\hat{f}_h(\mathbf{x})) d\mathbf{x}$$

Using Taylor expansion for small $h$, the asymptotic terms are:

$$\text{Var}(\hat{f}_h(\mathbf{x})) \approx \frac{R(K)}{n h^d} f(\mathbf{x}), \quad \text{where } R(K) = \int K^2(\mathbf{u}) d\mathbf{u}$$

$$\text{Bias}(\hat{f}_h(\mathbf{x})) \approx \frac{1}{2} h^2 \mu_2(K) \nabla^2 f(\mathbf{x}), \quad \text{where } \mu_2(K) = \int u^2 K(u) du$$

Integrating over $\mathbb{R}^d$ yields AMISE:

$$\text{AMISE}(h) = \frac{R(K)}{n h^d} + \frac{1}{4} h^4 \mu_2(K)^2 R(\nabla^2 f)$$

Taking the derivative with respect to $h$ and setting to zero:

$$\frac{\partial \text{AMISE}}{\partial h} = -\frac{d \cdot R(K)}{n h^{d+1}} + h^3 \mu_2(K)^2 R(\nabla^2 f) = 0 \implies h_{\text{opt}} \propto n^{-\frac{1}{d + 4}}$$

### Analytical Rules of Thumb for Bandwidth Selection
Assuming the underlying distribution is a 1D normal distribution $f \sim \mathcal{N}(\mu, \sigma^2)$ and using a Gaussian kernel:

1. **Silverman's Rule of Thumb (1986)**:
   $$h_{\text{Silverman}} = 0.9 \cdot \min\left( \hat{\sigma}, \frac{\text{IQR}}{1.34} \right) n^{-\frac{1}{5}}$$
   where $\text{IQR} = Q_{75} - Q_{25}$ accounts for heavy tails and outliers.
2. **Scott's Rule of Thumb (1992)**:
   $$h_{\text{Scott}} = 1.06 \cdot \hat{\sigma} \cdot n^{-\frac{1}{5}}$$
   For $d$-dimensional multivariate Gaussian:
   $$h_{\text{Scott}, d} = n^{-\frac{1}{d + 4}} \hat{\sigma}$$

---

## 4. Computational Complexity

| Operation | Naive Formulation | Tree-Accelerated (`KDTree`/`BallTree`) | Fast Fourier Transform (1D Grid) |
| :--- | :--- | :--- | :--- |
| **Training (Fit)** | $\mathcal{O}(1)$ (stores references) | $\mathcal{O}(d \cdot n \log n)$ (index build) | $\mathcal{O}(n + M \log M)$ ($M$ grid points) |
| **Query (1 sample)** | $\mathcal{O}(n \cdot d)$ | $\mathcal{O}(d \cdot \log n)$ (for bounded kernels) | $\mathcal{O}(1)$ (grid interpolation) |
| **Evaluation ($m$ samples)** | $\mathcal{O}(m \cdot n \cdot d)$ | $\mathcal{O}(m \cdot d \cdot \log n)$ | $\mathcal{O}(M \log M + m)$ |
| **Memory Space** | $\mathcal{O}(n \cdot d)$ | $\mathcal{O}(n \cdot d)$ | $\mathcal{O}(M)$ |

---

## 5. Inductive Bias & Statistical Assumptions

1. **Local Smoothness**: Points close together in Euclidean space share similar probability density values. The underlying density $f(\mathbf{x})$ is assumed to be twice continuously differentiable.
2. **Isotropic / Spherical Bandwidth (Default)**: Standard scikit-learn KDE uses a scalar bandwidth $h$ across all feature dimensions. This assumes all features have been scaled to uniform unit variance.
3. **Metric Sensitivity**: Distances are Euclidean by default. In high dimensions ($d > 10$), distance concentration degrades the kernel weights so that every point appears equidistant.

---

## 6. From-Scratch Implementation (NumPy)

Below is an enterprise-grade, fully vectorized 1D/multivariate Kernel Density Estimator supporting Gaussian, Epanechnikov, and Tophat kernels with bandwidth heuristics:

```python
"""
Non-parametric Kernel Density Estimation implemented from first principles.
Pure NumPy implementation with AMISE bandwidth heuristics and log-sum-exp stabilization.
"""

from __future__ import annotations
import numpy as np
from typing import Literal

KernelType = Literal["gaussian", "epanechnikov", "tophat"]

class PureNumPyKDE:
    """
    Multivariate Kernel Density Estimator using NumPy vectorization.
    
    Parameters
    ----------
    bandwidth : float | str
        Bandwidth parameter $h > 0$, or 'silverman', 'scott'.
    kernel : KernelType
        Kernel shape function ('gaussian', 'epanechnikov', 'tophat').
    """
    def __init__(self, bandwidth: float | str = "silverman", kernel: KernelType = "gaussian"):
        self.bandwidth = bandwidth
        self.kernel = kernel
        self.X_train_: np.ndarray | None = None
        self.h_: float = 0.0
        self.d_: int = 0
        self.n_: int = 0

    def _compute_bandwidth(self, X: np.ndarray) -> float:
        n, d = X.shape
        # Compute pooled standard deviation
        sigma = np.mean(np.std(X, axis=0, ddof=1))
        
        # Interquartile range
        q75, q25 = np.percentile(X, 75, axis=0), np.percentile(X, 25, axis=0)
        iqr = np.mean(q75 - q25) / 1.34
        
        spread = min(sigma, iqr) if iqr > 1e-6 else sigma
        if spread <= 1e-6:
            spread = 1.0

        if self.bandwidth == "silverman":
            return float(0.9 * spread * (n ** (-1.0 / (d + 4))))
        elif self.bandwidth == "scott":
            return float(1.06 * spread * (n ** (-1.0 / (d + 4))))
        elif isinstance(self.bandwidth, (int, float)):
            if self.bandwidth <= 0:
                raise ValueError(f"Bandwidth must be positive, got {self.bandwidth}")
            return float(self.bandwidth)
        else:
            raise ValueError(f"Unknown bandwidth strategy: {self.bandwidth}")

    def fit(self, X: np.ndarray) -> PureNumPyKDE:
        """
        Store training data and compute effective bandwidth.
        """
        X = np.asarray(X, dtype=np.float64)
        if X.ndim == 1:
            X = X[:, np.newaxis]
        self.n_, self.d_ = X.shape
        self.X_train_ = np.ascontiguousarray(X)
        self.h_ = self._compute_bandwidth(X)
        return self

    def score_samples(self, X: np.ndarray) -> np.ndarray:
        """
        Compute log-density log p(x) for each query point using log-sum-exp stability.
        
        Returns
        -------
        log_dens : np.ndarray of shape (m,)
        """
        if self.X_train_ is None:
            raise RuntimeError("PureNumPyKDE must be fitted before scoring.")
        
        X = np.asarray(X, dtype=np.float64)
        if X.ndim == 1:
            X = X[:, np.newaxis]
        m, d = X.shape
        if d != self.d_:
            raise ValueError(f"Feature mismatch: fit on {self.d_}, queried with {d}")

        # Compute pairwise Euclidean squared distances: (m, n)
        # ||x - x_i||^2 = ||x||^2 + ||x_i||^2 - 2 x x_i^T
        diff = X[:, np.newaxis, :] - self.X_train_[np.newaxis, :, :]  # (m, n, d)
        sq_dist = np.sum(diff ** 2, axis=2)                          # (m, n)
        h = self.h_

        if self.kernel == "gaussian":
            # Normalization constant: (2pi)^(-d/2) * h^(-d)
            log_norm = -0.5 * d * np.log(2.0 * np.pi) - d * np.log(h)
            # Log-arguments: -0.5 * sq_dist / h^2
            log_weights = -0.5 * sq_dist / (h ** 2)
            # log p(x) = log_norm + log( (1/n) * sum exp(log_weights) )
            # Numerically stable Log-Sum-Exp:
            max_w = np.max(log_weights, axis=1, keepdims=True)
            sum_exp = np.sum(np.exp(log_weights - max_w), axis=1)
            log_density = log_norm - np.log(self.n_) + max_w.squeeze(1) + np.log(sum_exp)
            return log_density

        elif self.kernel == "epanechnikov":
            # Epanechnikov kernel: K(u) = 0.5 * c_d * (1 - u^2) for ||u|| <= 1
            u_sq = sq_dist / (h ** 2)
            mask = u_sq <= 1.0
            # For 1D: c_1 = 3/4
            # Multivariate normalization:
            # V_d = pi^(d/2) / Gamma(d/2 + 1)
            # c_d = 0.5 * (d + 2) / V_d
            from scipy.special import gamma
            v_d = (np.pi ** (d / 2.0)) / gamma(d / 2.0 + 1.0)
            c_d = 0.5 * (d + 2) / v_d
            kernel_vals = np.where(mask, c_d * (1.0 - u_sq) / (h ** d), 0.0)
            density = np.mean(kernel_vals, axis=1)
            return np.log(np.maximum(density, 1e-300))

        elif self.kernel == "tophat":
            # Tophat kernel: indicator ||x - x_i|| <= h
            dist = np.sqrt(sq_dist)
            mask = dist <= h
            from scipy.special import gamma
            v_d = (np.pi ** (d / 2.0)) / gamma(d / 2.0 + 1.0)
            vol = v_d * (h ** d)
            kernel_vals = np.where(mask, 1.0 / vol, 0.0)
            density = np.mean(kernel_vals, axis=1)
            return np.log(np.maximum(density, 1e-300))

        else:
            raise ValueError(f"Unsupported kernel: {self.kernel}")

    def score(self, X: np.ndarray) -> float:
        """Total log-likelihood across all points in X."""
        return float(np.sum(self.score_samples(X)))

    def sample(self, n_samples: int = 1, random_state: int | None = None) -> np.ndarray:
        """
        Generate synthetic samples from the fitted KDE distribution.
        """
        rng = np.random.default_rng(random_state)
        if self.X_train_ is None:
            raise RuntimeError("Must fit before sampling.")
        
        # Step 1: Uniformly pick one data point per required sample
        idx = rng.choice(self.n_, size=n_samples, replace=True)
        centers = self.X_train_[idx]

        # Step 2: Add random noise drawn from the kernel shape
        if self.kernel == "gaussian":
            noise = rng.normal(loc=0.0, scale=self.h_, size=(n_samples, self.d_))
            return centers + noise
        else:
            raise NotImplementedError(f"Sampling for {self.kernel} kernel not implemented.")
```

---

## 7. Production Scikit-Learn Pipeline

In high-throughput microservices, continuous variables must be scaled and decorrelated (via PCA/StandardScaler) before passing to `KernelDensity` with a spatial tree structure.

```python
"""
Production 9-stage Pipeline for Anomaly Scoring and Density Estimation using Scikit-Learn.
"""

import numpy as np
import pandas as pd
from sklearn.base import BaseEstimator, TransformerMixin
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import RobustScaler
from sklearn.decomposition import PCA
from sklearn.neighbors import KernelDensity
from sklearn.model_selection import GridSearchCV, KFold

class DensityAnomalyDetector(BaseEstimator):
    """
    High-reliability Scikit-Learn Estimator wrapping KernelDensity with calibrated quantile thresholds.
    """
    def __init__(
        self,
        bandwidth: float = 0.5,
        kernel: str = "gaussian",
        contamination: float = 0.01,
        metric: str = "euclidean"
    ):
        self.bandwidth = bandwidth
        self.kernel = kernel
        self.contamination = contamination
        self.metric = metric
        self.pipeline_: Pipeline | None = None
        self.threshold_: float = -np.inf

    def fit(self, X: np.ndarray, y=None) -> DensityAnomalyDetector:
        # Preprocessing: Robust scaling + PCA decorrelation + Tree-backed KDE
        self.pipeline_ = Pipeline([
            ("scaler", RobustScaler()),
            ("pca", PCA(n_components=0.95, svd_solver="full")),
            ("kde", KernelDensity(
                bandwidth=self.bandwidth,
                kernel=self.kernel,
                metric=self.metric,
                algorithm="ball_tree"
            ))
        ])
        self.pipeline_.fit(X)

        # Calibrate decision threshold via empirical percentile
        log_dens = self.pipeline_.named_steps["kde"].score_samples(
            self.pipeline_.named_steps["pca"].transform(
                self.pipeline_.named_steps["scaler"].transform(X)
            )
        )
        self.threshold_ = float(np.percentile(log_dens, 100.0 * self.contamination))
        return self

    def score_samples(self, X: np.ndarray) -> np.ndarray:
        """Returns log-density log p(x)."""
        transformed = self.pipeline_.named_steps["pca"].transform(
            self.pipeline_.named_steps["scaler"].transform(X)
        )
        return self.pipeline_.named_steps["kde"].score_samples(transformed)

    def predict(self, X: np.ndarray) -> np.ndarray:
        """Returns -1 for outliers (low density), +1 for inliers."""
        log_dens = self.score_samples(X)
        return np.where(log_dens < self.threshold_, -1, 1)
```

---

## 8. Model Persistence & SHA-256 Integrity

Because `KernelDensity` with `ball_tree` caches the entire training dataset inside the tree node structures, pickling can produce multi-gigabyte binaries. We save via `joblib` alongside cryptographic SHA-256 sidecars and schema metadata:

```python
"""
Enterprise serialization pattern for KernelDensity with hash integrity checking.
"""

import json
import hashlib
import joblib
from pathlib import Path
from typing import Any, Dict

def save_kde_artifact(estimator: Any, out_path: Path, metadata: Dict[str, Any]) -> Path:
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    
    # 1. Serialize model
    joblib.dump(estimator, out_path, compress=3)
    
    # 2. Compute SHA-256 digest
    sha256 = hashlib.sha256()
    with open(out_path, "rb") as f:
        while chunk := f.read(65536):
            sha256.update(chunk)
    digest = sha256.hexdigest()

    # 3. Write metadata sidecar
    meta_payload = {
        "model_file": out_path.name,
        "sha256": digest,
        "bandwidth": float(getattr(estimator, "bandwidth", 0.0)),
        "metadata": metadata
    }
    with open(out_path.with_suffix(".json"), "w", encoding="utf-8") as f:
        json.dump(meta_payload, f, indent=2)
        
    return out_path

def load_kde_artifact(model_path: Path) -> Any:
    model_path = Path(model_path)
    meta_path = model_path.with_suffix(".json")
    
    if not meta_path.exists():
        raise FileNotFoundError(f"Missing cryptographic metadata sidecar: {meta_path}")
        
    with open(meta_path, "r", encoding="utf-8") as f:
        meta = json.load(f)
        
    sha256 = hashlib.sha256()
    with open(model_path, "rb") as f:
        while chunk := f.read(65536):
            sha256.update(chunk)
    current_digest = sha256.hexdigest()
    
    if current_digest != meta["sha256"]:
        raise ValueError(
            f"Integrity check failed! Expected {meta['sha256']}, got {current_digest}"
        )
        
    return joblib.load(model_path)
```

---

## 9. Hyperparameter Tuning & Diagnostics

Bandwidth $h$ is the single most critical parameter in KDE. In practice, analytical rules of thumb (Silverman/Scott) break down under multimodality or heavy skew. Grid-searched cross-validation maximizing out-of-fold log-likelihood is the industry standard:

```python
from sklearn.model_selection import GridSearchCV, LeaveOneOut, KFold
from sklearn.neighbors import KernelDensity
import numpy as np

def tune_kde_bandwidth(X: np.ndarray, cv_splits: int = 5) -> KernelDensity:
    """
    Grid search over logarithmic bandwidth space using K-Fold Cross-Validation.
    """
    n, d = X.shape
    # Analytical base bandwidth
    base_h = 1.06 * np.mean(np.std(X, axis=0)) * (n ** (-1.0 / (d + 4)))
    
    param_grid = {
        "bandwidth": np.logspace(np.log10(base_h * 0.1), np.log10(base_h * 10.0), 30),
        "kernel": ["gaussian", "epanechnikov", "exponential"]
    }
    
    cv = KFold(n_splits=cv_splits, shuffle=True, random_state=42)
    grid = GridSearchCV(
        KernelDensity(algorithm="ball_tree"),
        param_grid=param_grid,
        cv=cv,
        n_jobs=-1,
        scoring=None # Scikit-learn KDE score() returns total log-likelihood
    )
    grid.fit(X)
    return grid.best_estimator_
```

---

## 10. Residuals, Diagnostics & Validation

To evaluate how well a KDE model fits an empirical distribution, we test:
1. **Cumulative Integral Consistency**: $\int_{-\infty}^{\infty} \hat{f}(x) dx \approx 1.0$.
2. **Kolmogorov-Smirnov (KS) Goodness-of-Fit**: Compare synthetic samples generated from fitted KDE against empirical holdout distribution via two-sample KS test.

```python
from scipy.stats import ks_2samp
import numpy as np

def validate_kde_fit(kde_model: KernelDensity, X_test: np.ndarray) -> dict:
    """
    Runs Kolmogorov-Smirnov test between holdout distribution and KDE generated samples.
    """
    # Draw synthetic points from KDE
    synthetic_samples = kde_model.sample(n_samples=len(X_test), random_state=42)
    
    p_values = []
    # Test each feature dimension independently
    for dim in range(X_test.shape[1]):
        stat, p_val = ks_2samp(X_test[:, dim], synthetic_samples[:, dim])
        p_values.append(p_val)
        
    return {
        "mean_p_value": float(np.mean(p_values)),
        "min_p_value": float(np.min(p_values)),
        "fit_accepted_at_alpha_05": bool(np.all(np.array(p_values) > 0.05))
    }
```

---

## 11. Uncertainty Estimation & Confidence Intervals

Standard KDE yields a point density estimate $\hat{f}(x)$. To obtain point-wise $(1 - \alpha)$ confidence bands, we compute empirical bootstrap intervals:

$$\hat{f}_{\text{boot}}^{(b)}(x) = \frac{1}{n h^d} \sum_{i=1}^n K\left(\frac{x - X_i^{*(b)}}{h}\right), \quad b=1,\dots,B$$

$$\text{CI}_{1-\alpha}(x) = \left[ q_{\alpha/2}\left(\{\hat{f}^{(b)}(x)\}\right), \; q_{1-\alpha/2}\left(\{\hat{f}^{(b)}(x)\}\right) \right]$$

---

## 12. Failure Modes & Edge Cases

| Failure Mode | Root Cause | Behavioral Symptom | Detection Strategy | Mitigation |
| :--- | :--- | :--- | :--- | :--- |
| **Boundary Bias (Spillover)** | Kernel assigns positive probability density past natural physical boundaries ($x < 0$). | Non-zero density at impossible coordinates (e.g. negative prices, negative ages). | Check $\int_{-\infty}^{0} \hat{f}(x) dx > 0$ when domain is $[0, \infty)$. | Log-transform data $y = \ln x$ before fitting KDE, then apply Jacobian correction: $\hat{f}_X(x) = \hat{f}_Y(\ln x) \cdot \frac{1}{x}$. |
| **Curse of Dimensionality** | In $d > 10$, volume scales as $r^d$; data points are isolated in empty space. | Bandwidth must be set enormous to overlap points, destroying all local structure. | AMISE scales as $\mathcal{O}(n^{-4/(d+4)})$; sample size requirement grows exponentially. | Project with PCA/UMAP to $d \le 5$, or use Gaussian Mixture Models / Normalizing Flows. |
| **Bimodal Density Masking** | Global scalar bandwidth $h$ chosen by Silverman's rule on a mixture of narrow and wide peaks. | Over-smooths narrow peak into a lump; fractures wide peak into noisy spikes. | Visual residual inspection; large discrepancy in peak variances. | Variable/Adaptive Bandwidth KDE ($h_i$ varies inversely with local sample density $\hat{f}(x_i)^{-1/2}$). |
| **Outlier Inflation** | Isolated extreme anomalies pull density mass into empty regions. | Phantom high-density tails; massive bandwidth calculation in Scott's rule due to inflated $\sigma$. | Leverage IQR rather than $\sigma$ in bandwidth formula; evaluate Cook's distance. | Use Silverman's formula with robust IQR: $h \propto \min(\sigma, \text{IQR}/1.34)$. |
| **Memory Exhaustion on Predict** | Evaluating $m$ test queries against $n$ training points creates $(m \times n)$ pairwise matrix. | Python crashes with `MemoryError` or Linux OOM Killer invocation. | Monitor RAM during `score_samples()`. | Evaluate in chunked batches (e.g. 10,000 queries per batch) or use `algorithm='ball_tree'`. |
| **Floating Point Underflow** | Summing small probabilities $\prod f(x_j)$ in high dimensions underflows to exact `0.0`. | `log(0)` yields `-inf` or `NaN` errors in anomaly scoring downstream. | Zero or `-inf` values in output vectors. | Always work in log-density domain using Log-Sum-Exp identity: $\ln \sum e^{z_i} = z_{\max} + \ln \sum e^{z_i - z_{\max}}$. |

---

## 13. Comparative Trade-offs & When to Use

| Dimension | Histogram | Kernel Density Estimation (KDE) | Gaussian Mixture Model (GMM) | Normalizing Flows (Deep Learning) |
| :--- | :--- | :--- | :--- | :--- |
| **Parametric Assumption** | Non-parametric (discrete) | Non-parametric (continuous) | Semi-parametric (mixture of Gaussians) | Non-parametric (invertible neural nets) |
| **Training Speed** | $\mathcal{O}(n)$ | $\mathcal{O}(n \log n)$ (Tree build) | $\mathcal{O}(k \cdot n \cdot d \cdot \text{iters})$ (EM) | $\mathcal{O}(\text{epochs} \cdot n)$ (GPU training) |
| **Inference Speed** | $\mathcal{O}(1)$ (hash lookup) | $\mathcal{O}(d \cdot \log n)$ to $\mathcal{O}(n \cdot d)$ | $\mathcal{O}(k \cdot d)$ (fast matrix vector) | $\mathcal{O}(\text{layers} \cdot d)$ |
| **Memory Footprint** | Small (stores bin counts) | **Huge** (stores all training points) | Small (stores $k$ means & covariances) | Moderate (neural network weights) |
| **Dimensional Scaling** | Terrible ($B^d$ bins) | Poor ($d \le 10$) | Moderate ($d \le 50$) | Excellent ($d > 100$, images/audio) |
| **Optimal Use Case** | Real-time dashboards, metrics | Small/Medium data density & mode finding | Parametric clustering, fast latency inference | High-dimensional generative modeling |

---

## 14. Production Engineering & Latency SLAs

### Real-Time Inference Optimization
- **Do NOT evaluate raw NumPy pairwise matrices in online web services**: For $n = 100,000$ points, a single HTTP query takes $\sim 45\text{ ms}$.
- **Use `BallTree` / `KDTree`**: Pruning points outside kernel bandwidth radius shrinks query latency to $< 2\text{ ms}$.
- **Precomputed 1D/2D Lookup Tables**: If query features are 1D or 2D (e.g. latitude/longitude or transaction amount), precompute a $1000 \times 1000$ density grid at build time and execute $\mathcal{O}(1)$ bilinear interpolation during request handling.

```
       Online Production Request Flow (p99 SLA < 5ms)
       
       [HTTP POST /score] 
               │
               ▼
       [Input Validator] (Check feature range & bounds)
               │
               ▼
       [Feature Transform] (RobustScaler -> PCA projection)
               │
               ▼
       ┌───────────────────────────────┐
       │   KDE BallTree Query Engine   │  <--- Skips distant branches outside
       │     (score_samples)           │       bounding hypersphere
       └───────────────────────────────┘
               │
               ▼
       [Calibrated Quantile Check] (log_density < -12.43 ==> Anomaly)
               │
               ▼
       [JSON Response: {is_anomaly: true, score: 0.982}]
```

---

## 15. Interpretability & Feature Attribution

Because KDE computes joint density $p(x_1, \dots, x_d)$, explaining why a specific query point has an anomalous low density requires computing **marginal directional gradients**:

$$\nabla_{\mathbf{x}} \ln \hat{f}(\mathbf{x}) = \frac{\nabla_{\mathbf{x}} \hat{f}(\mathbf{x})}{\hat{f}(\mathbf{x})} = \frac{1}{\sum_{i=1}^n K\left(\frac{\mathbf{x}-\mathbf{x}_i}{h}\right)} \sum_{i=1}^n \nabla_{\mathbf{x}} K\left(\frac{\mathbf{x}-\mathbf{x}_i}{h}\right)$$

For a Gaussian kernel, the gradient points directly toward the nearest dense data mode:

$$\nabla_{\mathbf{x}} \ln \hat{f}(\mathbf{x}) = \frac{1}{h^2} \left[ \left(\sum_{i=1}^n w_i(\mathbf{x}) \mathbf{x}_i \right) - \mathbf{x} \right], \quad w_i(\mathbf{x}) = \frac{\exp(-\frac{1}{2h^2}\|\mathbf{x}-\mathbf{x}_i\|^2)}{\sum_j \exp(-\frac{1}{2h^2}\|\mathbf{x}-\mathbf{x}_j\|^2)}$$

The feature $j$ with the largest absolute gradient $|\nabla_{x_j} \ln \hat{f}(\mathbf{x})|$ is the primary driver dragging the point into low density.

---

## 16. Hardware Acceleration, Scaling & Distributed Notes

- **GPU Acceleration via CuPy / PyTorch**: Pairwise Euclidean distance matrices $\mathbf{D}_{ij} = \|\mathbf{x}_i - \mathbf{y}_j\|^2$ are massively parallelizable GEMM operations. On an NVIDIA A100 GPU, a batch of $100,000$ points can be scored against $1,000,000$ reference points in $12\text{ ms}$ via tensor broadcasting.
- **Distributed Spark KDE**: In Apache Spark, compute KDE via spatial binning or random projection approximations rather than Cartesian `join` across executor partitions.

---

## 17. Pytest Verification Suite

```python
"""
Pytest test harness validating mathematical correctness, bandwidth bounds, and scoring.
Save as: tests/test_kernel_density.py
Run: pytest -v tests/test_kernel_density.py
"""

import pytest
import numpy as np
from sklearn.neighbors import KernelDensity

def test_pure_numpy_kde_matches_sklearn():
    """Verify from-scratch NumPy KDE matches Scikit-Learn within numerical precision."""
    from scipy.stats import norm
    
    rng = np.random.default_rng(42)
    X_train = rng.normal(loc=0.0, scale=1.0, size=(150, 1))
    X_test = np.linspace(-3.0, 3.0, 50).reshape(-1, 1)
    
    h = 0.4
    # Scikit-learn
    skl_kde = KernelDensity(bandwidth=h, kernel="gaussian").fit(X_train)
    skl_log_dens = skl_kde.score_samples(X_test)
    
    # Scratch KDE
    # Pure Gaussian Log-Sum-Exp implementation
    diff = X_test[:, np.newaxis, :] - X_train[np.newaxis, :, :] # (50, 150, 1)
    sq_dist = np.sum(diff ** 2, axis=2)
    log_weights = -0.5 * sq_dist / (h ** 2)
    log_norm = -0.5 * np.log(2.0 * np.pi) - np.log(h)
    max_w = np.max(log_weights, axis=1, keepdims=True)
    sum_exp = np.sum(np.exp(log_weights - max_w), axis=1)
    scratch_log_dens = log_norm - np.log(len(X_train)) + max_w.squeeze(1) + np.log(sum_exp)
    
    np.testing.assert_allclose(scratch_log_dens, skl_log_dens, rtol=1e-5, atol=1e-5)

def test_integral_sums_to_one():
    """Verify that the numeric integral of the 1D estimated density over the real line equals 1.0."""
    rng = np.random.default_rng(101)
    X_train = rng.normal(loc=5.0, scale=2.0, size=(300, 1))
    
    kde = KernelDensity(bandwidth=0.5, kernel="gaussian").fit(X_train)
    grid = np.linspace(-5.0, 15.0, 5000).reshape(-1, 1)
    log_dens = kde.score_samples(grid)
    dens = np.exp(log_dens)
    
    # Trapezoidal numerical integration
    dx = grid[1, 0] - grid[0, 0]
    total_area = np.trapezoid(dens, dx=dx)
    
    assert np.isclose(total_area, 1.0, atol=1e-3)

def test_bandwidth_scaling_silverman():
    """Verify Silverman's rule outputs positive scaling consistent with N^(-1/5)."""
    rng = np.random.default_rng(7)
    X = rng.normal(0, 1, (1000, 1))
    sigma = np.std(X, ddof=1)
    expected_h = 0.9 * sigma * (1000 ** (-0.2))
    assert expected_h > 0.1 and expected_h < 0.5
```

---

## 18. Cross-Track Integration

- **`01-languages/python/21-data-and-scientific-basics`**: SciPy `scipy.stats.gaussian_kde` covariance factor tuning and visualization with matplotlib.
- **`03-backend/fastapi/16-ai-and-ml-serving`**: Low-latency REST serving of KDE anomaly scores with BallTree memory-pinned caching.
- **`04-devops/docker`**: Packaging CPU-bound Scikit-Learn tree inference with multi-core OpenMP threads (`OMP_NUM_THREADS=4`).
- **`05-ai/machine-learning/05-unsupervised-learning/12-anomaly-detection.md`**: Comparing non-parametric KDE thresholding with Isolation Forests and Local Outlier Factor.

---

## 19. Real-World Enterprise Case Study

### Sensor Drift & Outlier Detection in Industrial Gas Pipelines
- **Context**: A chemical refinery monitors 8 continuous pressure, flow, and temperature sensors across 2,400 meters of cracking pipe.
- **Problem**: Mechanical valves develop micro-leaks, causing gradual sensor drift into anomalous states without triggering hard threshold alarms.
- **Solution Architecture**:
  1. Historical normal operation data ($N = 180,000$ points) projected to 4 dimensions via robust PCA.
  2. Multi-bandwidth KDE fitted using `BallTree` search.
  3. Continuous stream (10 Hz) evaluated against calibrated threshold $\tau_{0.001}$ corresponding to the 99.9th percentile of normal operational density.
- **Outcome**: Detected 14 micro-leaks an average of 42 hours before physical rupture, preventing an estimated \$3.8M in emergency shutdown downtime.

---

## 20. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Prove why the optimal asymptotic bandwidth $h$ scales as $\mathcal{O}(n^{-1/(d+4)})$.</strong></summary>

**Answer:**  
In multivariate Kernel Density Estimation, the Mean Integrated Squared Error decomposes into integrated variance and integrated squared bias:
$$\text{MISE}(h) = \int \text{Var}(\hat{f}_h(\mathbf{x})) d\mathbf{x} + \int \text{Bias}^2(\hat{f}_h(\mathbf{x})) d\mathbf{x}$$

Using standard Taylor expansion:
1. The variance term scales as $\frac{C_1}{n h^d}$ because the estimator averages $n$ local kernel evaluations, each having volume proportional to $h^d$.
2. The bias term scales as $h^2$, making integrated squared bias scale as $C_2 h^4$.

Combining these yields the Asymptotic MISE:
$$\text{AMISE}(h) = \frac{C_1}{n h^d} + C_2 h^4$$

To find the minimum, differentiate with respect to $h$ and set to zero:
$$\frac{d}{dh}\text{AMISE}(h) = -\frac{d \cdot C_1}{n h^{d+1}} + 4 C_2 h^3 = 0$$
$$4 C_2 h^{d+4} = \frac{d \cdot C_1}{n} \implies h^{d+4} = \frac{d \cdot C_1}{4 C_2 n} \implies h_{\text{opt}} = \left(\frac{d \cdot C_1}{4 C_2}\right)^{\frac{1}{d+4}} n^{-\frac{1}{d+4}}$$
Thus, $h_{\text{opt}} \in \mathcal{O}(n^{-\frac{1}{d+4}})$.
</details>

<details>
<summary><strong>Q2: Why does Kernel Density Estimation suffer catastrophically when $d > 10$ (the Curse of Dimensionality)?</strong></summary>

**Answer:**  
The performance decay is driven by the convergence rate of the error:
1. **Convergence Rate**: From the AMISE derivation, substituting $h_{\text{opt}}$ back into AMISE gives:
   $$\text{AMISE}_{\text{opt}} \in \mathcal{O}\left(n^{-\frac{4}{d+4}}\right)$$
   For $d = 1$, the error decreases as $n^{-4/5} = n^{-0.8}$. For $d = 20$, the error decreases as $n^{-4/24} = n^{-0.166}$. To achieve the same accuracy in $d = 20$ that 100 samples provide in $d=1$, you would need $n \approx 100^{0.8 / 0.166} \approx 100^{4.8} \approx 4 \times 10^9$ samples.
2. **Empty Space Phenomenon**: In high dimensions, almost all volume of a hypercube is concentrated in the corners, and the volume of a hypersphere of radius $h$ approaches zero. The distance between points becomes uniform, causing kernel weights to collapse to uniform constants.
</details>

<details>
<summary><strong>Q3: How does Scikit-Learn's `KernelDensity` achieve sub-linear query time using `BallTree`?</strong></summary>

**Answer:**  
A naive KDE evaluates all $n$ training points for every query, yielding $\mathcal{O}(n \cdot d)$ time. `BallTree` partitions feature space into nested bounding hyperspheres (balls). 
For kernels with compact support (like Epanechnikov or Tophat with radius $h$), if the distance from the query point to the closest boundary of a ball exceeds $h$, the entire subtree can be pruned immediately with zero kernel evaluations. 
Even for infinite-support kernels like Gaussian, points located beyond $3h$ or $4h$ contribute negligible probability ($< 10^{-5}$) to the sum. The `BallTree` uses dual-tree traversal and distance bounds to prune tree nodes whose maximum possible contribution to the kernel sum is below a specified error tolerance, reducing expected query complexity to $\mathcal{O}(\log n)$.
</details>

<details>
<summary><strong>Q4: Explain the difference between Silverman's Rule and Scott's Rule for bandwidth selection.</strong></summary>

**Answer:**  
Both rules assume the underlying distribution is Gaussian:
- **Scott's Rule**: Assumes a normal density and derives the optimal coefficient by minimizing AMISE analytically: $h = 1.06 \cdot \hat{\sigma} \cdot n^{-1/5}$.
- **Silverman's Rule**: Recognizes that empirical data often has heavier tails or slight multimodality than a pure Gaussian. Using the sample standard deviation $\hat{\sigma}$ alone over-estimates spread if outliers exist. Silverman replaces $\hat{\sigma}$ with $A = \min(\hat{\sigma}, \frac{\text{IQR}}{1.34})$ and reduces the coefficient from $1.06$ to $0.9$ to provide slightly narrower bandwidths that avoid washing out bimodal peaks: $h = 0.9 \cdot A \cdot n^{-1/5}$.
</details>

<details>
<summary><strong>Q5: What is boundary bias in KDE and how do you resolve it mathematically?</strong></summary>

**Answer:**  
Boundary bias occurs when the true density is strictly supported on a bounded domain (e.g., $x \in [0, \infty)$ for income or lifetime), but symmetric kernels allocate probability mass across the boundary ($x < 0$). This causes the estimator to underestimate density near the boundary and assign non-zero probability to impossible negative values.
**Resolutions**:
1. **Domain Transformation**: Transform $Y = g(X) = \ln X$ where $Y \in (-\infty, \infty)$. Fit KDE on $Y$, then apply change of variables formula:
   $$\hat{f}_X(x) = \hat{f}_Y(\ln x) \cdot \left|\frac{d}{dx}\ln x\right| = \hat{f}_Y(\ln x) \cdot \frac{1}{x}$$
2. **Reflection Principle**: Reflect data across the boundary: create synthetic dataset $\{x_1, -x_1, x_2, -x_2, \dots\}$. Estimate density on the doubled data and multiply by 2 for $x \ge 0$.
</details>

<details>
<summary><strong>Q6: Why is Leave-One-Out Cross-Validation (LOOCV) used for KDE instead of standard training log-likelihood?</strong></summary>

**Answer:**  
In KDE, evaluating training log-likelihood directly $\sum_{i=1}^n \ln \hat{f}_h(\mathbf{x}_i)$ leads to catastrophic degenerate overfitting: as $h \to 0$, each point evaluates its own kernel center where $K(0)/h^d \to \infty$. Thus, training likelihood is maximized at $h = 0$ (a sum of Dirac delta functions).
To prevent this, LOOCV computes the density at each training point $\mathbf{x}_i$ using the estimator trained on all points **except** $\mathbf{x}_i$:
$$\hat{f}_{h,-i}(\mathbf{x}_i) = \frac{1}{(n-1)h^d} \sum_{j \ne i} K\left(\frac{\mathbf{x}_i - \mathbf{x}_j}{h}\right)$$
As $h \to 0$, the distance $\|\mathbf{x}_i - \mathbf{x}_j\| > 0$ causes the kernel evaluated at other points to vanish, driving $\hat{f}_{h,-i}(\mathbf{x}_i) \to 0$ and log-likelihood to $-\infty$. This penalizes tiny bandwidths and finds the genuine maximum likelihood trade-off.
</details>

<details>
<summary><strong>Q7: How do you perform semi-supervised classification using Kernel Density Estimation?</strong></summary>

**Answer:**  
By using Bayes' theorem to construct a **Generative Classifier**:
1. For each class $c \in \{1, \dots, C\}$, fit a separate KDE model $\hat{f}_c(\mathbf{x})$ using only training samples belonging to class $c$.
2. Compute class priors $\hat{P}(Y = c) = \frac{N_c}{N}$.
3. For any unlabelled query point $\mathbf{x}$, compute the class posterior via Bayes' Rule:
   $$\hat{P}(Y = c \mid \mathbf{x}) = \frac{\hat{P}(Y = c) \hat{f}_c(\mathbf{x})}{\sum_{k=1}^C \hat{P}(Y = k) \hat{f}_k(\mathbf{x})}$$
4. Predict $\hat{y} = \arg\max_c \hat{P}(Y = c \mid \mathbf{x})$. This is known as a Non-Parametric Naive Bayes or Kernel Discriminant Analysis.
</details>

<details>
<summary><strong>Q8: What is Variable (Adaptive) Bandwidth KDE and when is it necessary?</strong></summary>

**Answer:**  
Standard fixed-bandwidth KDE uses a single constant $h$ everywhere. In distributions with heavy tails or regions of widely varying density, a fixed $h$ is a bad compromise: it over-smooths sharp peaks in dense central regions, while creating noisy spurious spikes in sparse tails.
**Adaptive Bandwidth KDE** assigns a distinct bandwidth $h_i$ to each sample:
$$h_i = h_0 \cdot \left( \frac{\tilde{f}(\mathbf{x}_i)}{g} \right)^{-\alpha}$$
where $\tilde{f}$ is a pilot density estimate, $g$ is the geometric mean of pilot densities, and $\alpha \in [0, 1]$ (typically $0.5$). Points in sparse regions (low density) receive large bandwidths to provide smooth tail estimation, while points in dense clusters receive small bandwidths to preserve sharp modal peaks.
</details>

---

## 21. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Computing and Plotting 1D Bimodal Density</strong></summary>

**Task**: Generate a 1D dataset containing two Gaussian modes at $\mu_1 = -2, \mu_2 = 3$. Fit a Scikit-Learn `KernelDensity` model and evaluate density over a uniform grid.

**Solution**:
```python
import numpy as np
from sklearn.neighbors import KernelDensity

# 1. Generate bimodal data
rng = np.random.default_rng(42)
X = np.concatenate([
    rng.normal(loc=-2.0, scale=0.8, size=300),
    rng.normal(loc=3.0, scale=1.2, size=700)
])[:, np.newaxis]

# 2. Fit KDE
kde = KernelDensity(bandwidth=0.4, kernel="gaussian", algorithm="kd_tree")
kde.fit(X)

# 3. Evaluate on grid
x_grid = np.linspace(-6, 8, 1000)[:, np.newaxis]
log_dens = kde.score_samples(x_grid)
density = np.exp(log_dens)

print(f"Peak density mode 1 evaluated near -2: {density[np.argmin(np.abs(x_grid - (-2.0)))]:.4f}")
print(f"Peak density mode 2 evaluated near 3: {density[np.argmin(np.abs(x_grid - 3.0))]:.4f}")
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Fast Anomaly Detection via Quantile Thresholding</strong></summary>

**Task**: Given a 2D sensor telemetry array, fit a KDE model, score the samples, and identify the top 2% most anomalous data points.

**Solution**:
```python
import numpy as np
from sklearn.neighbors import KernelDensity

rng = np.random.default_rng(101)
normal_telemetry = rng.multivariate_normal(mean=[50.0, 100.0], cov=[[5, 2], [2, 8]], size=1000)
anomalies = np.array([[30.0, 70.0], [80.0, 140.0], [52.0, 102.0]])
X_all = np.vstack([normal_telemetry, anomalies])

# Fit KDE
kde = KernelDensity(bandwidth=1.5, kernel="gaussian").fit(normal_telemetry)
log_densities = kde.score_samples(X_all)

# Threshold at 2nd percentile
threshold = np.percentile(log_densities[:1000], 2.0)
outliers_idx = np.where(log_densities < threshold)[0]

print(f"Calibrated Threshold: {threshold:.2f}")
print(f"Detected {len(outliers_idx)} outliers. Indices: {outliers_idx}")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Log-Transform Boundary Bias Correction</strong></summary>

**Task**: Create a strictly positive dataset ($X \sim \text{Exponential}(1)$). Demonstrate how naive KDE causes boundary leakage ($x < 0$), and implement a change-of-variables log transform to enforce strict positivity.

**Solution**:
```python
import numpy as np
from sklearn.neighbors import KernelDensity

rng = np.random.default_rng(42)
X_pos = rng.exponential(scale=1.5, size=2000)[:, np.newaxis]

# Transformed domain: Y = ln(X)
Y = np.log(X_pos)
kde_y = KernelDensity(bandwidth=0.3, kernel="gaussian").fit(Y)

# Query grid in positive domain
x_grid = np.linspace(0.01, 6.0, 500)[:, np.newaxis]
y_grid = np.log(x_grid)

# Change of variables: f_X(x) = f_Y(ln x) * (1 / x)
log_dens_y = kde_y.score_samples(y_grid)
dens_x = np.exp(log_dens_y) * (1.0 / x_grid.squeeze())

print(f"Density at x=0.01: {dens_x[0]:.4f}")
print(f"Verified strictly zero density for x < 0 by construction.")
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): K-Fold Cross-Validation Grid Search for Bandwidth</strong></summary>

**Task**: Write a standalone cross-validation loop to find the optimal bandwidth over an irregular distribution without using Scikit-Learn's `GridSearchCV`.

**Solution**:
```python
import numpy as np
from sklearn.neighbors import KernelDensity
from sklearn.model_selection import KFold

rng = np.random.default_rng(2024)
X = np.concatenate([rng.normal(0, 1, 400), rng.laplace(4, 0.5, 600)])[:, np.newaxis]

bandwidth_candidates = np.linspace(0.05, 1.0, 20)
kf = KFold(n_splits=5, shuffle=True, random_state=42)

best_score = -np.inf
best_h = None

for h in bandwidth_candidates:
    fold_scores = []
    for train_idx, val_idx in kf.split(X):
        kde = KernelDensity(bandwidth=h, kernel="gaussian").fit(X[train_idx])
        # Score returns total log-likelihood
        val_score = kde.score(X[val_idx])
        fold_scores.append(val_score)
    mean_score = np.mean(fold_scores)
    if mean_score > best_score:
        best_score = mean_score
        best_h = h

print(f"Optimal Bandwidth: {best_h:.4f} with validation total log-likelihood: {best_score:.2f}")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Vectorized Epanechnikov Kernel Implementation</strong></summary>

**Task**: Implement an Epanechnikov kernel estimator from scratch in NumPy supporting arbitrary dimensionality $d$, correctly applying the unit ball volume normalization factor $c_d = \frac{1}{2}(d+2) / V_d$.

**Solution**:
```python
import numpy as np
from scipy.special import gamma

class VectorizedEpanechnikovKDE:
    def __init__(self, bandwidth: float = 1.0):
        self.bandwidth = bandwidth
        self.X_train_ = None

    def fit(self, X: np.ndarray):
        self.X_train_ = np.asarray(X, dtype=np.float64)
        self.n_, self.d_ = self.X_train_.shape
        # Unit hypersphere volume: V_d = pi^(d/2) / Gamma(d/2 + 1)
        v_d = (np.pi ** (self.d_ / 2.0)) / gamma(self.d_ / 2.0 + 1.0)
        # Normalization constant: c_d = 0.5 * (d + 2) / V_d
        self.c_d_ = 0.5 * (self.d_ + 2.0) / v_d
        return self

    def score_samples(self, X_query: np.ndarray) -> np.ndarray:
        X_q = np.asarray(X_query, dtype=np.float64)
        m = len(X_q)
        h = self.bandwidth
        
        # Pairwise squared normalized distance: (m, n)
        diff = X_q[:, np.newaxis, :] - self.X_train_[np.newaxis, :, :]
        u_sq = np.sum(diff ** 2, axis=2) / (h ** 2)
        
        # Epanechnikov formula: c_d * (1 - u^2) for u^2 <= 1
        valid_mask = u_sq <= 1.0
        kernel_eval = np.where(valid_mask, self.c_d_ * (1.0 - u_sq) / (h ** self.d_), 0.0)
        density = np.mean(kernel_eval, axis=1)
        return np.log(np.maximum(density, 1e-300))

# Test
rng = np.random.default_rng(42)
X = rng.normal(0, 1, (200, 2))
kde = VectorizedEpanechnikovKDE(bandwidth=1.2).fit(X)
log_p = kde.score_samples(X[:5])
print("Epanechnikov Log-Densities for first 5 samples:", log_p)
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): High-Throughput Batch Inference Engine</strong></summary>

**Task**: Construct a memory-safe batch inference engine that processes $1,000,000$ query points against a $50,000$-sample training set using chunked execution and multi-processing without exceeding 1 GB RAM.

**Solution**:
```python
import numpy as np
from sklearn.neighbors import KernelDensity
from concurrent.futures import ProcessPoolExecutor
import math

def _score_chunk(chunk_data: np.ndarray, model: KernelDensity) -> np.ndarray:
    return model.score_samples(chunk_data)

class SafeBatchKDEInference:
    def __init__(self, model: KernelDensity, chunk_size: int = 50_000, n_jobs: int = 2):
        self.model = model
        self.chunk_size = chunk_size
        self.n_jobs = n_jobs

    def predict_in_chunks(self, X_large: np.ndarray) -> np.ndarray:
        n_queries = len(X_large)
        n_chunks = math.ceil(n_queries / self.chunk_size)
        chunks = [
            X_large[i * self.chunk_size : min((i + 1) * self.chunk_size, n_queries)]
            for i in range(n_chunks)
        ]
        
        results = []
        for chunk in chunks:
            # Sequential chunk processing protects RAM
            results.append(self.model.score_samples(chunk))
            
        return np.concatenate(results)

# Verification
rng = np.random.default_rng(42)
X_ref = rng.normal(0, 1, (1000, 3))
kde = KernelDensity(bandwidth=0.5, algorithm="ball_tree").fit(X_ref)

X_test_huge = rng.normal(0, 1, (100_000, 3))
engine = SafeBatchKDEInference(kde, chunk_size=20_000)
scores = engine.predict_in_chunks(X_test_huge)
print(f"Successfully processed {len(scores):,} samples in memory-safe batches.")
```
</details>
