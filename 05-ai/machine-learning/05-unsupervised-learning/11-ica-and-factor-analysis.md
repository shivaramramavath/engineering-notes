# Independent Component Analysis (ICA) & Factor Analysis

> **Track:** `05-ai/machine-learning` | **Module:** `05-unsupervised-learning` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `NumPy: >=1.26.0` | `SciPy: >=1.12.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Independent Component Analysis (FastICA) & Exploratory Factor Analysis (FA) |
| **Family** | Blind Source Separation (BSS) / Latent Variable Generative Modeling |
| **Input Feature Types** | Continuous numerical signals or features ($X \in \mathbb{R}^{n \times d}$). Strictly sensitive to feature scaling and zero-mean centering. |
| **Target Output** | Statistically independent latent source signals $\mathbf{S} \in \mathbb{R}^{n \times k}$, unmixing matrix $\mathbf{W} \in \mathbb{R}^{k \times d}$, mixing matrix $\mathbf{A} \in \mathbb{R}^{d \times k}$, and Factor Analysis uniqueness variances $\mathbf{\Psi} \in \mathbb{R}^{d \times d}$. |
| **Optimization Target** | **FastICA**: Maximizing statistical non-Gaussianity of projected components via Negentropy: $\max_{\mathbf{w}} J(\mathbf{w}^T \mathbf{x}) \propto \left[ \mathbb{E}[G(\mathbf{w}^T \mathbf{x})] - \mathbb{E}[G(\nu)] \right]^2$ subject to $\|\mathbf{w}\|=1$. **Factor Analysis**: Maximum Likelihood Estimation of factor loadings via Expectation-Maximization (EM): $\mathbf{\Sigma} = \mathbf{\Lambda} \mathbf{\Lambda}^T + \mathbf{\Psi}$. |
| **Core Assumptions** | **ICA**: Latent sources are mutually independent ($P(\mathbf{s}) = \prod P(s_j)$) and **strictly non-Gaussian** (at most one Gaussian source can be separated). **FA**: Latent factors are standard Gaussian ($\mathbf{f} \sim \mathcal{N}(\mathbf{0}, \mathbf{I})$) with feature-specific independent Gaussian noise ($\boldsymbol{\epsilon} \sim \mathcal{N}(\mathbf{0}, \mathbf{\Psi})$). |
| **Key Hyperparameters** | `n_components` ($k$), `algorithm` (`'parallel'`, `'deflation'`), `fun` (`'logcosh'`, `'exp'`, `'cube'`), `rotation` (for FA: `'varimax'`, `'quartimax'`). |
| **Inference Mechanism** | Deterministic linear unmixing: $\mathbf{s} = \mathbf{W} (\mathbf{x} - \boldsymbol{\mu})$. |
| **Primary Failure Modes** | Failure when multiple sources are Gaussian (rotational ambiguity); scale and sign ambiguity (cannot recover true amplitude or polarity); failure on non-linear or convoluted mixtures. |

---

## 2. Architecture & Intuition

### The Cocktail Party Problem
Imagine a crowded room with two people speaking simultaneously ($s_1(t)$ and $s_2(t)$) and two microphones placed at different locations recording mixed acoustic audio signals ($x_1(t)$ and $x_2(t)$). Each microphone records a distinct linear combination of both voices:

$$\mathbf{x}(t) = \mathbf{A} \mathbf{s}(t)$$

```
     True Sources (Independent)            Acoustic Mixing                Recorded Signals               FastICA Unmixing              Recovered Sources
     [ Speaker A: s_1(t) ] ──────\      /──> [ Mic 1: x_1(t) ] ────────\      /──> [ Unmixed Source s_1' ]
                                  \    /                                \    /
                                   \  /                                  \  /
                                    \/                                    \/
                                    /\                                    /\
                                   /  \                                  /  \
                                  /    \                                /    \
     [ Speaker B: s_2(t) ] ──────/      \──> [ Mic 2: x_2(t) ] ────────/      \──> [ Unmixed Source s_2' ]
                                      Mixing Matrix A                       Unmixing Matrix W = A^{-1}
```

### Why PCA Fails at Source Separation
PCA finds axes that are **mutually uncorrelated** ($\text{Cov}(z_i, z_j) = 0$). However:
$$\text{Independence} \implies \text{Uncorrelated}, \quad \text{but} \quad \text{Uncorrelated} \centernot\implies \text{Independence}$$
Uncorrelatedness only eliminates second-order linear covariance. **Statistical Independence** requires zero mutual information across all higher-order statistical moments:

$$P(s_1, s_2) = P(s_1) P(s_2)$$

PCA rotates the coordinate frame along the axes of maximum global variance, which blends the two speakers together. ICA searches for an unmixing matrix $\mathbf{W} = \mathbf{A}^{-1}$ that maximizes the statistical independence of the recovered signals.

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $\mathbf{s} \in \mathbb{R}^k$ | Vector | Latent independent source signals (unobserved) |
| $\mathbf{A} \in \mathbb{R}^{d \times k}$ | Matrix | Mixing matrix (full rank, unobserved) |
| $\mathbf{x} \in \mathbb{R}^d$ | Vector | Observed mixed feature vector: $\mathbf{x} = \mathbf{A} \mathbf{s}$ |
| $\mathbf{W} \in \mathbb{R}^{k \times d}$ | Matrix | Unmixing matrix: $\mathbf{W} \approx \mathbf{A}^{-1}$ such that $\hat{\mathbf{s}} = \mathbf{W} \mathbf{x}$ |
| $\nu \sim \mathcal{N}(0, 1)$ | Random Variable | Standard Gaussian random variable |
| $J(y)$ | $\mathbb{R}_{\ge 0}$ | Negentropy metric quantifying distance from Gaussianity |
| $G(\cdot)$ | Function $\mathbb{R} \to \mathbb{R}$ | Non-quadratic contrast function ($\log \cosh$, Gaussian exponential) |
| $\mathbf{\Lambda} \in \mathbb{R}^{d \times k}$| Matrix | Factor loading matrix in Factor Analysis |
| $\mathbf{\Psi} \in \mathbb{R}^{d \times d}$| Diagonal Matrix | Specific uniqueness error variances in Factor Analysis |

### 1. The Central Limit Theorem Justification
Why does maximizing non-Gaussianity recover independent sources?
The **Central Limit Theorem (CLT)** states that the sum of independent random variables is **always more Gaussian** than any of the individual variables:

$$x_1 = a_{11} s_1 + a_{12} s_2$$

The mixed signal $x_1$ is closer to a Gaussian distribution than either original source $s_1$ or $s_2$. 
Conversely, to unmix the signals, we seek a linear projection $y = \mathbf{w}^T \mathbf{x} = \mathbf{w}^T \mathbf{A} \mathbf{s}$ that is as **far from Gaussian as possible**. When $y$ reaches its maximum non-Gaussianity, it must isolate an individual pure source!

> [!IMPORTANT]
> **The Gaussian Impossibility**: If the original sources $\mathbf{s}$ are Gaussian, their joint distribution is spherically symmetric ($\exp(-\frac{1}{2}\|\mathbf{s}\|^2)$). Any orthogonal rotation of a Gaussian mixture is completely indistinguishable from any other rotation. ICA can separate at most **one** Gaussian source; all other sources must be strictly non-Gaussian!

### 2. FastICA: Contrast Functions & Fixed-Point Iteration (Hyvärinen, 1999)

#### Step 1: Pre-Whitening (Sphering)
Before searching for non-Gaussian directions, pre-process the centered data $\mathbf{X}_c$ using PCA/SVD so that features are mutually uncorrelated with unit variance:

$$\tilde{\mathbf{x}} = \mathbf{D}^{-1/2} \mathbf{E}^T \mathbf{x}_c$$
$$\mathbb{E}[\tilde{\mathbf{x}} \tilde{\mathbf{x}}^T] = \mathbf{I}$$

Where $\mathbf{E} \mathbf{D} \mathbf{E}^T$ is the EVD of $\mathbb{E}[\mathbf{x}_c \mathbf{x}_c^T]$. Whitening reduces the problem of finding an arbitrary unmixing matrix $\mathbf{W}$ to finding an **orthonormal rotation matrix** ($\mathbf{W} \mathbf{W}^T = \mathbf{I}$), halving the number of parameters.

#### Step 2: Negentropy Maximization via Contrast Functions
Negentropy $J(y)$ measures non-Gaussianity using differential entropy $H(y)$:
$$J(y) = H(y_{\text{gauss}}) - H(y) \ge 0$$
Hyvärinen approximated Negentropy using smooth, robust non-quadratic contrast functions $G(u)$:

$$J(y) \propto \left[ \mathbb{E}[G(y)] - \mathbb{E}[G(\nu)] \right]^2$$

Standard Contrast Functions:
1. **Log-Cosh**: $G(u) = \frac{1}{a} \log \cosh(a u)$ with derivative $g(u) = \tanh(a u)$ (robust default).
2. **Exponential**: $G(u) = -\exp(-u^2 / 2)$ with derivative $g(u) = u \exp(-u^2 / 2)$.

#### Step 3: FastICA Fixed-Point Newton Update
For a single weight vector $\mathbf{w}$ on whitened data $\tilde{\mathbf{x}}$:
Using the Kuhn-Tucker condition on the unit sphere $\|\mathbf{w}\|^2 = 1$, the Newton-Raphson update step is:

$$\mathbf{w}^{(t+1)} = \mathbb{E}\left[ \tilde{\mathbf{x}} \, g(\mathbf{w}^{(t)T} \tilde{\mathbf{x}}) \right] - \mathbb{E}\left[ g'(\mathbf{w}^{(t)T} \tilde{\mathbf{x}}) \right] \mathbf{w}^{(t)}$$
$$\mathbf{w}^{(t+1)} \leftarrow \frac{\mathbf{w}^{(t+1)}}{\|\mathbf{w}^{(t+1)}\|_2}$$

For $g(u) = \tanh(u)$, the second derivative is $g'(u) = 1 - \tanh^2(u)$.

#### Step 4: Symmetric Decorrelation (For $K$ Components)
To extract all $k$ independent components simultaneously without deflation bias, apply symmetric orthogonalization:

$$\mathbf{W} \leftarrow (\mathbf{W} \mathbf{W}^T)^{-1/2} \mathbf{W}$$

---

### 3. Factor Analysis: Modeling Shared vs Unique Variance
Factor Analysis (Spearman, 1904) is a generative linear Gaussian model that separates total variance into **shared common factors** and **variable-specific uniquenesses**:

$$\mathbf{x} = \boldsymbol{\mu} + \mathbf{\Lambda} \mathbf{f} + \boldsymbol{\epsilon}$$

Where:
- $\mathbf{f} \sim \mathcal{N}(\mathbf{0}, \mathbf{I})$ are $k$ latent common factors.
- $\boldsymbol{\epsilon} \sim \mathcal{N}(\mathbf{0}, \mathbf{\Psi})$ is independent feature-specific uniqueness noise, where $\mathbf{\Psi} = \text{diag}(\psi_1, \dots, \psi_d)$ is diagonal.
- The implied covariance matrix of $\mathbf{x}$ decomposes as:
  $$\mathbf{\Sigma}_{\mathbf{x}} = \mathbf{\Lambda} \mathbf{\Lambda}^T + \mathbf{\Psi}$$

#### Fundamental Difference from PCA:
- **PCA** models total variance along orthogonal axes: $\mathbf{\Sigma} = \mathbf{V} \mathbf{\Lambda} \mathbf{V}^T$, forcing uniqueness noise into the components.
- **Factor Analysis** explicitly discards independent diagonal noise $\mathbf{\Psi}$, modeling strictly the off-diagonal shared covariance $\mathbf{\Lambda} \mathbf{\Lambda}^T$.
- **Varimax Rotation**: Because factor loadings $\mathbf{\Lambda}$ have rotational ambiguity ($\mathbf{\Lambda} \mathbf{R} (\mathbf{\Lambda} \mathbf{R})^T = \mathbf{\Lambda} \mathbf{\Lambda}^T$), Varimax rotates factor axes to maximize loading sparsity (each variable loads heavily on only 1 factor).

---

## 4. Computational Complexity

| Algorithm | Iteration Complexity | Convergence Rate | Space Complexity |
| :--- | :--- | :--- | :--- |
| **FastICA (Parallel)** | $O(n \cdot d \cdot k)$ per iteration | Cubic / Quadratic Newton | $O(n \cdot d + d^2)$ |
| **Factor Analysis (EM)**| $O(n \cdot d \cdot k + d \cdot k^2 + k^3)$ per EM step | Linear convergence | $O(n \cdot d + d^2)$ |
| **Inference / Unmixing**| $O(d \cdot k)$ matrix-vector product | Deterministic | $O(k)$ |

---

## 5. Inductive Bias & Learning Guarantees

1. **Non-Gaussianity Equals Independence**: ICA assumes that the underlying physical generative mechanisms are non-Gaussian. Under the Darmois-Skitovich Theorem, if linear combinations of independent variables are independent, the variables must be Gaussian. Thus, non-Gaussianity uniquely guarantees statistical independence.
2. **Indeterminacies of ICA**:
   - **Scale Ambiguity**: The variance of sources cannot be recovered; both $\mathbf{s}$ and $\mathbf{A}$ are unknown. Standard practice normalizes sources to unit variance ($\mathbb{E}[s_j^2] = 1$).
   - **Sign Ambiguity**: Reversing the sign of source $s_j$ and multiplying column $a_j$ by $-1$ leaves $\mathbf{x}$ unchanged.
   - **Permutation Ambiguity**: The order of components is arbitrary; there is no natural ranking like eigenvalues in PCA.

---

## 6. Scratch NumPy Implementation

```python
"""
Scratch NumPy implementation of FastICA with Symmetric Orthogonalization.
Implements pre-whitening via SVD, fixed-point updates with tanh contrast function,
and symmetric matrix decorrelation.
"""

from __future__ import annotations
import numpy as np


class ScratchFastICA:
    """Production reference FastICA implementation in pure NumPy."""

    def __init__(
        self,
        n_components: int = 2,
        max_iter: int = 200,
        tol: float = 1e-4,
        random_state: int | None = 42,
    ) -> None:
        self.n_components = n_components
        self.max_iter = max_iter
        self.tol = tol
        self.random_state = random_state

        self.mean_: np.ndarray | None = None
        self.components_: np.ndarray | None = None  # Unmixing matrix W
        self.mixing_: np.ndarray | None = None      # Mixing matrix A = W^{-1}

    def _whiten(self, X: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """Pre-whiten centered data: cov(X_white) = I."""
        # Economy SVD of X_centered: X_c = U S V^T
        U, S, Vt = np.linalg.svd(X, full_matrices=False)
        # Whitened data: X_white = sqrt(n - 1) * U[:, :k]
        k = self.n_components
        X_white = np.sqrt(len(X) - 1) * U[:, :k]
        # Whitening projection matrix K = sqrt(n-1) * V_k @ S_k^{-1}
        K = np.sqrt(len(X) - 1) * (Vt[:k].T / S[:k])
        return X_white, K

    def fit_transform(self, X: np.ndarray) -> np.ndarray:
        rng = np.random.default_rng(self.random_state)
        X = np.asarray(X, dtype=np.float64)
        n_samples, n_features = X.shape

        # 1. Center Data
        self.mean_ = np.mean(X, axis=0)
        X_centered = X - self.mean_

        # 2. Pre-Whitening
        X_white, K = self._whiten(X_centered)  # shape (n_samples, k)

        # 3. Initialize Random Orthonormal Rotation Matrix W_white in R^{k x k}
        W_init = rng.normal(size=(self.n_components, self.n_components))
        # Orthogonalize via SVD
        u, _, vt = np.linalg.svd(W_init)
        W_white = u @ vt

        # 4. FastICA Fixed-Point Newton Loop (Symmetric Decorrelation)
        for iteration in range(self.max_iter):
            # Compute projections: Y = X_white @ W_white.T -> shape (n_samples, k)
            Y = np.dot(X_white, W_white.T)

            # Contrast function g(u) = tanh(u), g'(u) = 1 - tanh^2(u)
            g_Y = np.tanh(Y)
            g_prime_Y = 1.0 - g_Y ** 2

            # Newton-Raphson Step:
            # W_new = (g(Y).T @ X_white) / n - diag(mean(g'(Y))) @ W_white
            term1 = np.dot(g_Y.T, X_white) / n_samples
            term2 = np.mean(g_prime_Y, axis=0)[:, None] * W_white
            W_new = term1 - term2

            # Symmetric Orthogonalization: W_new = (W_new @ W_new.T)^{-1/2} @ W_new
            u_step, s_step, vt_step = np.linalg.svd(W_new)
            W_new = np.dot(u_step, vt_step)

            # Check convergence: max absolute inner product shift
            # For converged W, diag(abs(W_new @ W_white.T)) should be 1.0
            shift = np.max(np.abs(np.abs(np.diag(np.dot(W_new, W_white.T))) - 1.0))
            W_white = W_new

            if shift <= self.tol:
                break

        # 5. Full Unmixing Matrix W = W_white @ K.T in R^{k x d}
        self.components_ = np.dot(W_white, K.T)
        self.mixing_ = np.linalg.pinv(self.components_)

        # Recover Independent Sources S = X_centered @ W.T
        return np.dot(X_centered, self.components_.T)

    def transform(self, X: np.ndarray) -> np.ndarray:
        if self.components_ is None or self.mean_ is None:
            raise RuntimeError("ScratchFastICA is not fitted.")
        X = np.asarray(X, dtype=np.float64)
        return np.dot(X - self.mean_, self.components_.T)
```

---

## 7. Production Scikit-Learn 9-Stage Pipeline

```python
"""
Production Blind Source Separation pipeline comparing FastICA against Factor Analysis.
Separates synthetic non-Gaussian audio/sensor signals from linear mixtures.
"""

from __future__ import annotations
import logging
from typing import Dict, Tuple
import numpy as np
from sklearn.decomposition import FastICA, FactorAnalysis, PCA
from scipy.signal import square, sawtooth

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("ICA_Pipeline")


def run_blind_source_separation_suite() -> Tuple[FastICA, Dict[str, float]]:
    logger.info("Stage 1: Synthesizing 3 independent non-Gaussian signals (Sine, Square, Sawtooth).")
    time_steps = np.linspace(0, 8, 2000)
    s1 = np.sin(2 * time_steps)                    # Sine wave
    s2 = square(3 * time_steps)                     # Square wave
    s3 = sawtooth(5 * time_steps)                   # Sawtooth wave
    S_true = np.column_stack([s1, s2, s3])

    # Stage 2: Linear Mixing Matrix A (3x3 invertible matrix)
    logger.info("Stage 2: Mixing signals via linear mixing matrix A.")
    A = np.array([[1.0, 1.0, 0.5], [0.5, 2.0, 1.0], [1.5, 1.0, 2.0]])
    X_mixed = np.dot(S_true, A.T)
    # Add minor Gaussian noise
    X_mixed += 0.05 * np.random.default_rng(42).normal(size=X_mixed.shape)

    # Stage 3: Baseline PCA (Demonstrating Failure to Unmix)
    pca = PCA(n_components=3, random_state=42)
    S_pca = pca.fit_transform(X_mixed)

    # Stage 4: FastICA Production Fitting
    logger.info("Stage 4: Fitting FastICA with log-cosh contrast function.")
    ica = FastICA(n_components=3, algorithm="parallel", fun="logcosh", max_iter=500, random_state=42)
    S_ica = ica.fit_transform(X_mixed)

    # Stage 5: Factor Analysis with Varimax Rotation
    logger.info("Stage 5: Fitting FactorAnalysis with Varimax rotation.")
    fa = FactorAnalysis(n_components=3, rotation="varimax", random_state=42)
    S_fa = fa.fit_transform(X_mixed)

    # Stage 6: Source Correlation Evaluation
    # Measure maximum absolute correlation between recovered signals and true square wave (s2)
    pca_corr = float(np.max(np.abs([np.corrcoef(s2, S_pca[:, j])[0, 1] for j in range(3)])))
    ica_corr = float(np.max(np.abs([np.corrcoef(s2, S_ica[:, j])[0, 1] for j in range(3)])))
    fa_corr = float(np.max(np.abs([np.corrcoef(s2, S_fa[:, j])[0, 1] for j in range(3)])))

    logger.info(f"Square Wave Recovery Correlation -> PCA: {pca_corr:.4f} | FA: {fa_corr:.4f} | ICA: {ica_corr:.4f}")

    metrics = {
        "pca_correlation": pca_corr,
        "fa_correlation": fa_corr,
        "ica_correlation": ica_corr,
    }
    return ica, metrics


if __name__ == "__main__":
    model, perf = run_blind_source_separation_suite()
    assert perf["ica_correlation"] > 0.95, "FastICA failed blind source separation SLA."
```

---

## 8. Model Persistence & Serialization

```python
"""
Serializing FastICA and Factor Analysis models.
"""

from pathlib import Path
import json
import joblib
from sklearn.decomposition import FastICA


def export_ica_bundle(model: FastICA, path: Path) -> Path:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    joblib.dump(model, path)

    meta = {
        "n_components": int(model.n_components),
        "unmixing_matrix": model.components_.tolist(),  # shape (k, d)
        "mixing_matrix": model.mixing_.tolist(),        # shape (d, k)
        "mean": model.mean_.tolist(),
    }
    meta_path = path.with_suffix(".ica.json")
    with open(meta_path, "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)
    return meta_path
```

---

## 9. Hyperparameter Tuning Guide

| Parameter | Options | Default | Engineering Guidance |
| :--- | :--- | :--- | :--- |
| `fun` (FastICA) | `'logcosh'`, `'exp'`, `'cube'` | `'logcosh'` | **`'logcosh'` is the robust universal default**. `'exp'` is superior when sources are highly super-Gaussian (heavy-tailed spikes). Avoid `'cube'` on noisy data as kurtosis is overly sensitive to outliers. |
| `algorithm` | `'parallel'`, `'deflation'` | `'parallel'` | `'parallel'` extracts all components simultaneously with symmetric decorrelation. Use `'deflation'` (one-by-one) only when specifically interested in the single most non-Gaussian component. |
| `rotation` (FA) | `None`, `'varimax'`, `'quartimax'` | `None` | **Always use `'varimax'` in Factor Analysis**. It rotates factor axes to maximize loading variance, forcing variables to load strongly on only one factor for clean human interpretability. |

---

## 10. Residuals, Diagnostics & Failure Modes

1. **Kurtosis Audit of Recovered Sources**:
   Compute excess kurtosis for each extracted source: $\kappa(s_j) = \frac{\mathbb{E}[s_j^4]}{(\mathbb{E}[s_j^2])^2} - 3$.
   - If $\kappa(s_j) \approx 0$: The source is Gaussian and cannot be reliably separated.
   - If $\kappa > 0$: Super-Gaussian (spiky, sparse, speech-like).
   - If $\kappa < 0$: Sub-Gaussian (bimodal, sinusoidal).
2. **Signal-to-Interference Ratio (SIR)**:
   Measures leakage between recovered channels. Higher values ($> 20\text{dB}$) indicate clean physical separation.
3. **Factor Analysis Uniqueness Variances ($\mathbf{\Psi}$)**:
   Inspect `fa.noise_variance_`. Variables with high uniqueness ($\psi_j \approx 1.0$) share no common variance with other features.

---

## 11. Prediction Uncertainty & Calibration

ICA produces uncalibrated linear coordinates. Source uncertainty is quantified via **crosstalk leakage**:

$$\mathbf{P} = \mathbf{W} \mathbf{A}$$

In an ideal separation, the performance matrix $\mathbf{P}$ is a permutation matrix with exactly one non-zero entry per row and column. The off-diagonal energy in $\mathbf{P}$ quantifies residual source contamination.

---

## 12. Comprehensive Failure Modes & Edge Cases Table

| Failure Mode | Root Cause | Observable Symptom | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **All-Gaussian Component Collapse** | Two or more underlying sources are normally distributed. | Rotational ambiguity; recovered sources remain completely mixed. | Mathematically impossible to separate via linear ICA. Requires non-linear or time-delayed ICA. |
| **Scale & Amplitude Invariance** | Scale is absorbed between $\mathbf{A}$ and $\mathbf{s}$. | Cannot know whether original speaker was loud or quiet. | Standardize recovered sources to unit variance ($\sigma = 1.0$). |
| **Sign Flip / Inversion** | Negating both $\mathbf{s}_j$ and column $a_j$ preserves mixture. | Audio playback is inverted; ECG peaks point downward. | Apply domain post-processing (e.g., force maximum peak of ECG signal to be positive). |
| **Permutation Indeterminacy** | Order of components is arbitrary. | Rerunning code produces identical sources in scrambled column orders. | Sort components post-hoc by Kurtosis magnitude or energy. |
| **Outlier Sensitivity (Kurtosis Collapse)** | Extreme outlier inflates 4th statistical moment. | Contrast function aligns entirely with 1 bad data point. | Use robust `'logcosh'` contrast function instead of `'cube'`. |
| **More Sources Than Sensors ($k > d$)** | Underdetermined Blind Source Separation. | Cannot invert non-square mixing matrix. | Requires Sparse Dictionary Learning or Deep Autoencoders. |

---

## 13. Comparative Trade-off Table

| Dimension | Independent Component Analysis (ICA) | Principal Component Analysis (PCA) | Exploratory Factor Analysis (FA) | Non-Negative Matrix Factorization (NMF) |
| :--- | :--- | :--- | :--- | :--- |
| **Target Property** | **Statistical Independence** ($P(\mathbf{s}) = \prod P(s_j)$) | **Uncorrelatedness** ($\text{Cov} = 0$) | **Shared Covariance** ($\mathbf{\Sigma} = \mathbf{\Lambda} \mathbf{\Lambda}^T + \mathbf{\Psi}$) | **Non-Negative Parts** ($W, H \ge 0$) |
| **Distribution** | **Strictly Non-Gaussian** | Gaussian / Orthogonal | Latent Gaussian + Specific Noise | Arbitrary Non-Negative |
| **Noise Model** | Noise-free mixing ($\mathbf{x} = \mathbf{A}\mathbf{s}$) | Noise absorbed into PCs | **Explicit diagonal noise $\mathbf{\Psi}$** | Residual Frobenius noise |
| **Rotational Invariance**| Fixed by non-Gaussianity | Fixed by orthogonal variance | Rotational ambiguity (Varimax) | Fixed by non-negativity simplicial cone |

---

## 14. Production Engineering, Serving & Latency

### Real-Time Embedded Audio / EEG Filtering
Because the unmixing matrix $\mathbf{W} \in \mathbb{R}^{k \times d}$ is static once trained, unmixing incoming streaming sensor buffers takes $< 20\mu\text{s}$ via BLAS:

```c
// Real-time DSP audio unmixing loop
void unmix_audio_frame(const float* x_mixed, const float* W, float* s_clean, int d, int k) {
    for (int i = 0; i < k; i++) {
        s_clean[i] = 0.0f;
        for (int j = 0; j < d; j++) {
            s_clean[i] += W[i * d + j] * x_mixed[j];
        }
    }
}
```

---

## 15. Interpretability & Explainability

1. **Topographic Scalp Maps (EEG/MEG)**:
   In neuroscience, columns of the mixing matrix $\mathbf{A}$ correspond to spatial electric field projections across scalp electrodes, identifying whether a component originates from the visual cortex or is an ocular blink artifact.
2. **Factor Analysis Loading Matrix**:
   Plotting heatmaps of Varimax-rotated loadings $\mathbf{\Lambda}$ shows which psychological test questions or survey items measure the same underlying latent personality trait.

---

## 16. Hardware, Parallelism & Scaling

- **Parallel FastICA**: In Scikit-Learn, `algorithm='parallel'` calculates Newton updates simultaneously across all $k$ components via BLAS matrix operations, scaling across all physical CPU cores.
- **CuPy / PyTorch**: For multi-channel MEG arrays with millions of time samples, fast-ICA matrix multiplications can be executed in PyTorch on CUDA GPUs in milliseconds.

---

## 17. Automated Verification Harness

```python
"""
Pytest automated verification harness for FastICA.
Asserts signal separation correlation, unmixing inverse consistency, and whitening.
"""

import numpy as np
import pytest
from sklearn.decomposition import FastICA
from scipy.signal import square


@pytest.fixture
def mixed_signals():
    t = np.linspace(0, 10, 1000)
    s1 = np.sin(2 * t)
    s2 = square(3 * t)
    S = np.column_stack([s1, s2])
    A = np.array([[1.0, 0.8], [0.5, 1.5]])
    X = np.dot(S, A.T)
    return S, X


def test_fastica_unmixing_recovers_sources(mixed_signals):
    """FastICA must recover independent sources with correlation > 0.90 to true signals."""
    S_true, X_mixed = mixed_signals
    ica = FastICA(n_components=2, algorithm="parallel", random_state=42)
    S_recovered = ica.fit_transform(X_mixed)

    # Check correlation to true sources up to sign
    corr_0 = np.max(np.abs([np.corrcoef(S_true[:, 0], S_recovered[:, j])[0, 1] for j in range(2)]))
    corr_1 = np.max(np.abs([np.corrcoef(S_true[:, 1], S_recovered[:, j])[0, 1] for j in range(2)]))

    assert corr_0 > 0.90, f"Failed to recover source 0: correlation={corr_0}"
    assert corr_1 > 0.90, f"Failed to recover source 1: correlation={corr_1}"


def test_mixing_unmixing_pseudo_inverse(mixed_signals):
    """Mixing matrix A and unmixing matrix W must satisfy W @ A = I."""
    _, X_mixed = mixed_signals
    ica = FastICA(n_components=2, random_state=42).fit(X_mixed)

    product = np.dot(ica.components_, ica.mixing_)
    np.testing.assert_allclose(product, np.eye(2), atol=1e-5)
```

---

## 18. Cross-Track Learning & Ecosystem Links

- **Linear Algebra**: Matrix factorizations in `01-math-foundations/01-linear-algebra-for-ml`.
- **Precursor PCA**: Pre-whitening steps in `05-unsupervised-learning/08-pca`.
- **Deep Learning Generative**: Bridge to Variational Autoencoders in `05-ai/machine-learning/14-deep-learning-bridge`.

---

## 19. Real-World Enterprise Case Study

### Context: Clinical Electroencephalogram (EEG) Ocular Artifact Removal
- **Domain**: Medical Neuro-Diagnostics & Brain-Computer Interfaces (BCI).
- **Problem**: 64-channel scalp EEG recordings are severely corrupted by eye blinks and retinal micro-saccades, which generate electrical voltages $50\times$ larger than cerebral brain waves.
- **Why Filtering Fails**: Eye blinks share overlapping frequency spectrums ($1\text{Hz}$–$10\text{Hz}$) with cortical theta waves. Frequency filters destroy the underlying neural signal.
- **Implementation**:
  - Model: `FastICA(n_components=64, fun='logcosh', random_state=42)`.
  - FastICA isolated eye blinks into exactly 1 independent component showing high frontal lobe kurtosis ($\kappa > 15.0$).
  - Set the blink component's column in mixing matrix $\mathbf{A}$ to zero and reconstructed cleaned signal: $\mathbf{X}_{\text{clean}} = \mathbf{S}_{\text{cleaned}} \mathbf{A}^T$.
- **Outcome**: Restored clean cortical brain rhythms with zero distortion of cerebral frequencies, automating clinical artifact rejection across 12,000 patient sleep studies.

---

## 20. Technical Interview Preparation

### Q1: Why can ICA not separate two independent signals if both are Gaussian?
**Answer:** The joint probability density of two independent zero-mean unit-variance Gaussian variables is:
$$P(s_1, s_2) = \frac{1}{2\pi} \exp\left( -\frac{s_1^2 + s_2^2}{2} \right) = \frac{1}{2\pi} \exp\left( -\frac{\|\mathbf{s}\|^2}{2} \right)$$
This distribution is **spherically symmetric**. If we multiply $\mathbf{s}$ by any arbitrary orthogonal rotation matrix $\mathbf{R}$ ($\mathbf{R} \mathbf{R}^T = \mathbf{I}$), the norm $\|\mathbf{R} \mathbf{s}\|^2 = \mathbf{s}^T \mathbf{R}^T \mathbf{R} \mathbf{s} = \|\mathbf{s}\|^2$. The joint density is completely invariant to rotation: $P(\mathbf{R} \mathbf{s}) = P(\mathbf{s})$. Consequently, every orthogonal rotation produces identical statistical independence, making it impossible to identify the true mixing direction $\mathbf{A}$. Non-Gaussian distributions have distinct directional higher-order moments (kurtosis/skewness) that break this rotational symmetry.

### Q2: Explain the Central Limit Theorem justification for maximizing non-Gaussianity.
**Answer:** By the Central Limit Theorem, the sum of independent random variables tends toward a Gaussian distribution. If observed mixtures are linear combinations of independent sources ($x_1 = a_{11} s_1 + a_{12} s_2$), $x_1$ is strictly more Gaussian than either original source $s_1$ or $s_2$. When searching for an unmixing vector $\mathbf{w}$, the linear combination $y = \mathbf{w}^T \mathbf{x}$ will be maximally Gaussian when it contains an equal mixture of both sources, and **maximally non-Gaussian** when it contains strictly one source. Thus, maximizing non-Gaussianity directly isolates the pure independent components.

### Q3: What is Negentropy, and why is it preferred over Kurtosis in FastICA?
**Answer:**
- **Kurtosis** is based on the 4th statistical moment: $\text{kurt}(y) = \mathbb{E}[y^4] - 3(\mathbb{E}[y^2])^2$. While mathematically simple, sample estimates of kurtosis are powered to the 4th degree, making them extremely sensitive to outliers in the data. A single distant outlier point can completely dominate the kurtosis calculation.
- **Negentropy** is information-theoretically grounded: $J(y) = H(y_{\text{gauss}}) - H(y)$, measuring differential entropy deficit. Hyvärinen approximated negentropy using non-quadratic functions $G(u) = \frac{1}{a} \log \cosh(a u)$ that grow much more slowly than $u^4$. This provides robust, outlier-resilient estimators of non-Gaussianity.

### Q4: What is the purpose of Pre-Whitening in FastICA?
**Answer:** Pre-whitening centers the data and transforms it such that its covariance matrix is an identity matrix ($\mathbb{E}[\tilde{\mathbf{x}} \tilde{\mathbf{x}}^T] = \mathbf{I}$).
1. It eliminates all second-order linear correlations up-front.
2. It proves that the unmixing matrix $\mathbf{W}$ on whitened data must be an **orthogonal matrix** ($\mathbf{W} \mathbf{W}^T = \mathbf{I}$).
This reduces the search space from $d^2$ free parameters to $\frac{d(d-1)}{2}$ orthogonal degrees of freedom, dramatically accelerating convergence.

### Q5: State the three fundamental indeterminacies of Independent Component Analysis.
**Answer:**
1. **Variance / Scale Ambiguity**: Because both sources $\mathbf{s}$ and mixing matrix $\mathbf{A}$ are unknown, any scalar multiplier $\alpha$ applied to source $s_j$ can be cancelled by dividing column $a_j$ by $\alpha$: $\mathbf{x} = (\mathbf{a}_j / \alpha)(\alpha s_j)$. True absolute physical amplitudes cannot be recovered.
2. **Sign Ambiguity**: Multiplying both $s_j$ and $a_j$ by $-1$ leaves the observed mixture unchanged. The polarity of signals cannot be determined.
3. **Permutation Ambiguity**: The ordering of components is arbitrary; there is no inherent ranking (unlike eigenvalues in PCA).

### Q6: How does Factor Analysis differ fundamentally from PCA?
**Answer:**
- **PCA** is a descriptive linear transformation that decomposes total variance along orthogonal axes: $\mathbf{\Sigma} = \mathbf{V} \mathbf{S}^2 \mathbf{V}^T / (n-1)$. It makes zero distinction between common signal variance and feature-specific noise.
- **Factor Analysis** is a generative probabilistic model that decomposes the covariance matrix into shared common variance and unique noise:
  $$\mathbf{\Sigma} = \mathbf{\Lambda} \mathbf{\Lambda}^T + \mathbf{\Psi}$$
  Where $\mathbf{\Psi}$ is a diagonal matrix of unique variances. Factor Analysis isolates the off-diagonal covariances, assuming that diagonal noise does not belong to the latent common factors.

### Q7: What is Varimax Rotation in Factor Analysis?
**Answer:** In Factor Analysis, factor loadings $\mathbf{\Lambda}$ have rotational ambiguity: for any orthogonal matrix $\mathbf{R}$, $\mathbf{\Lambda}^* = \mathbf{\Lambda} \mathbf{R}$ satisfies the covariance model identically. **Varimax Rotation** searches for an orthogonal rotation matrix $\mathbf{R}$ that maximizes the variance of the squared factor loadings:
$$\max_{\mathbf{R}} \sum_{j=1}^k \sum_{i=1}^d \left( \lambda_{ij}^4 - \frac{1}{d} \left( \sum_{i=1}^d \lambda_{ij}^2 \right)^2 \right)$$
This drives loadings to be either close to $1.0$ or close to $0.0$ (**Simple Structure**), ensuring each observed variable associates with only one latent factor, maximizing human interpretability.

### Q8: Under what conditions does FastICA fail to converge?
**Answer:** FastICA fails to converge when:
1. The dataset contains multiple Gaussian sources (causing the fixed-point gradient to oscillate on the spherical manifold).
2. The contrast function derivative explodes due to extreme unscaled feature outliers.
3. The sample size $n$ is too small relative to dimensionality $d$, causing empirical sample expectations $\mathbb{E}[g(\mathbf{w}^T \mathbf{x})]$ to have high variance.
4. Symmetric orthogonalization encounters singular matrices due to collinearity.

---

## 21. Hands-On Exercises & Mini-Projects

1. **Synthetic Acoustic Blind Source Separation**: Load two distinct WAV audio files (e.g., speech and music). Mix them mathematically using a $2 \times 2$ matrix. Run `ScratchFastICA` to separate the audio streams. Play back the separated WAV files and verify that clean speech is isolated.
2. **EEG Blink Removal Pipeline**: Simulate a 16-channel EEG array with alpha rhythms and periodic high-amplitude eye-blink spikes. Use `FastICA` to isolate the blink component, zero its column in the mixing matrix, and plot before-and-after signal traces.
3. **Varimax Rotation from Scratch**: Implement the Kaiser Varimax orthogonal rotation algorithm in pure NumPy. Fit Factor Analysis on a psychological survey dataset, apply Varimax rotation, and print the simplified factor loading matrix.
4. **Kurtosis vs Negentropy Outlier Sensitivity**: Create a synthetic non-Gaussian signal and inject 3 extreme outliers. Implement ICA using raw kurtosis ($u^3$) and compare its convergence against FastICA's `logcosh` contrast function to prove negentropy's robustness.
5. **Permutation & Sign Alignment Harness**: Write a utility function `match_recovered_sources(S_true, S_recovered)` that computes the absolute correlation matrix, solves the linear sum assignment problem via the Hungarian algorithm, and flips signs to align recovered components with ground truth.
6. **Factor Analysis vs PCA on High-Noise Features**: Generate a dataset where 5 features share a common latent factor, but 2 features have $10\times$ higher independent noise variance. Fit PCA and Factor Analysis. Show that PCA aligns with the high-noise features, while Factor Analysis correctly isolates the common latent factor.
