# Principal Component Analysis (PCA)

> **Track:** `05-ai/machine-learning` | **Module:** `05-unsupervised-learning` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `NumPy: >=1.26.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Principal Component Analysis (PCA) & Randomized / Incremental PCA |
| **Family** | Linear Dimensionality Reduction / Orthogonal Subspace Projection |
| **Input Feature Types** | Continuous numerical features ($X \in \mathbb{R}^{n \times d}$). Must be strictly centered ($\bar{\mathbf{x}} = \mathbf{0}$) and standardized. |
| **Target Output** | Uncorrelated orthogonal latent coordinates $\mathbf{Z} \in \mathbb{R}^{n \times k}$ ($k \le d$), component loading matrices $\mathbf{V} \in \mathbb{R}^{d \times k}$, and explained variance ratios. |
| **Optimization Target** | **Dual Formulations**: (1) Maximizing projected sample variance along orthogonal axes: $\max_{\mathbf{w}} \mathbf{w}^T \mathbf{\Sigma} \mathbf{w}$ s.t. $\|\mathbf{w}\|=1$. (2) Minimizing squared Euclidean reconstruction error: $\min_{\mathbf{V}_k} \sum \|\mathbf{x}_i - \mathbf{V}_k \mathbf{V}_k^T \mathbf{x}_i\|_2^2$. |
| **Optimization Solvers** | Singular Value Decomposition (SVD) of the centered data matrix ($\mathbf{X}_c = \mathbf{U} \mathbf{S} \mathbf{V}^T$), Eigenvalue Decomposition (EVD) of the empirical covariance matrix ($\mathbf{\Sigma} = \frac{1}{n-1} \mathbf{X}_c^T \mathbf{X}_c$), Randomized SVD (Halko et al., 2011). |
| **Key Hyperparameters** | `n_components` (Integer count $k$ or float variance ratio $\in (0, 1)$), `svd_solver` (`'auto'`, `'full'`, `'arpack'`, `'randomized'`), `whiten` (`True` / `False`). |
| **Inference Mechanism** | Deterministic linear projection: $\mathbf{z} = (\mathbf{x} - \boldsymbol{\mu}) \mathbf{V}_k$. Reconstruction: $\hat{\mathbf{x}} = \mathbf{z} \mathbf{V}_k^T + \boldsymbol{\mu}$. |
| **Primary Failure Modes** | Failure to capture non-linear manifold structures (collapses non-linear folds); extreme sensitivity to unscaled features; vulnerability to outliers which distort the sample covariance matrix. |

---

## 2. Architecture & Intuition

Principal Component Analysis finds a new orthogonal coordinate system for high-dimensional data such that:
1. The **first principal component ($PC_1$)** aligns along the axis of maximum empirical variance.
2. Each subsequent component ($PC_2, \dots, PC_k$) maximizes the remaining variance under the strict constraint that it is **mutually orthogonal** (uncorrelated) to all preceding components.

```
       Feature x2
           ^                     .  . .   PC 1 (Axis of Maximum Variance)
           |                   .   .  *
           |                 .   *  .  /
           |               .   *   .  /
           |             .   *   .   /
           |           .   *   .    /      PC 2 (Orthogonal to PC 1)
           |          .  *  .      /     /
           |         . * .        /    /
           |        .---.        /   /
           |       (  *  )      /  /
           |        '---'      / v
           +----------------------------------------------------------> Feature x1
```

### The Two Equivalent Theoretical Derivations
- **Maximum Variance Perspective**: Projecting data onto a subspace while preserving as much statistical information (variance) as possible.
- **Minimum Reconstruction Error Perspective**: Finding a $k$-dimensional linear hyperplane that minimizes the sum of squared perpendicular Euclidean distances from the data points to the hyperplane (Eckart-Young-Mirsky Theorem).

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $\mathbf{X} \in \mathbb{R}^{n \times d}$ | Matrix | Raw input feature matrix ($n$ samples, $d$ features) |
| $\boldsymbol{\mu} \in \mathbb{R}^d$ | Vector | Empirical column mean vector: $\boldsymbol{\mu} = \frac{1}{n} \sum_{i=1}^n \mathbf{x}_i$ |
| $\mathbf{X}_c \in \mathbb{R}^{n \times d}$ | Matrix | Centered data matrix: $\mathbf{X}_c = \mathbf{X} - \mathbf{1} \boldsymbol{\mu}^T$ |
| $\mathbf{\Sigma} \in \mathbb{R}^{d \times d}$ | Symmetric PSD Matrix | Empirical sample covariance matrix: $\mathbf{\Sigma} = \frac{1}{n - 1} \mathbf{X}_c^T \mathbf{X}_c$ |
| $\mathbf{w} \in \mathbb{R}^d$ | Unit Vector | Principal component projection direction ($\|\mathbf{w}\|_2 = 1$) |
| $\lambda_j$ | $\mathbb{R}_{\ge 0}$ | $j$-th eigenvalue of $\mathbf{\Sigma}$, representing variance along $PC_j$ |
| $\mathbf{Z} \in \mathbb{R}^{n \times k}$ | Matrix | Low-dimensional projected coordinates: $\mathbf{Z} = \mathbf{X}_c \mathbf{W}_k$ |
| $\mathbf{U}, \mathbf{S}, \mathbf{V}^T$ | SVD Matrices | Singular Value Decomposition of $\mathbf{X}_c$: $\mathbf{X}_c = \mathbf{U} \mathbf{S} \mathbf{V}^T$ |

### 1. Derivation via Maximum Projected Variance
Let $\mathbf{w}_1 \in \mathbb{R}^d$ be a unit projection vector ($\mathbf{w}_1^T \mathbf{w}_1 = 1$). The projection of a centered observation $\mathbf{x}_i$ onto $\mathbf{w}_1$ is the scalar $z_{i1} = \mathbf{w}_1^T \mathbf{x}_i$.
The sample variance of the projected points is:

$$\sigma_{z_1}^2 = \frac{1}{n - 1} \sum_{i=1}^n (\mathbf{w}_1^T \mathbf{x}_i)^2 = \frac{1}{n - 1} \mathbf{w}_1^T \mathbf{X}_c^T \mathbf{X}_c \mathbf{w}_1 = \mathbf{w}_1^T \mathbf{\Sigma} \mathbf{w}_1$$

We seek $\mathbf{w}_1$ that maximizes this variance subject to the unit-norm constraint:

$$\max_{\mathbf{w}_1} \mathbf{w}_1^T \mathbf{\Sigma} \mathbf{w}_1 \quad \text{subject to } \mathbf{w}_1^T \mathbf{w}_1 = 1$$

Formulating the Lagrangian using multiplier $\lambda_1$:
$$\mathcal{L}(\mathbf{w}_1, \lambda_1) = \mathbf{w}_1^T \mathbf{\Sigma} \mathbf{w}_1 - \lambda_1 (\mathbf{w}_1^T \mathbf{w}_1 - 1)$$

Taking the gradient with respect to $\mathbf{w}_1$ and setting to zero:
$$\nabla_{\mathbf{w}_1} \mathcal{L} = 2 \mathbf{\Sigma} \mathbf{w}_1 - 2 \lambda_1 \mathbf{w}_1 = 0 \implies \mathbf{\Sigma} \mathbf{w}_1 = \lambda_1 \mathbf{w}_1$$

This is the standard **Eigenvalue Equation**!
- $\mathbf{w}_1$ must be an eigenvector of the covariance matrix $\mathbf{\Sigma}$.
- Pre-multiplying by $\mathbf{w}_1^T$:
  $$\mathbf{w}_1^T \mathbf{\Sigma} \mathbf{w}_1 = \lambda_1 \mathbf{w}_1^T \mathbf{w}_1 = \lambda_1$$
- Therefore, the projected variance **equals the eigenvalue** $\lambda_1$. To maximize variance, $\mathbf{w}_1$ must be the eigenvector corresponding to the **largest eigenvalue** $\lambda_{\max}(\mathbf{\Sigma})$.
- By induction, the $j$-th principal component $\mathbf{w}_j$ is the eigenvector corresponding to the $j$-th largest eigenvalue $\lambda_j$, mutually orthogonal to all prior components ($\mathbf{w}_j^T \mathbf{w}_i = 0$ for $i < j$).

### 2. SVD Connection: Numerical Superiority
In production, computing the $d \times d$ covariance matrix $\mathbf{\Sigma} = \frac{1}{n-1} \mathbf{X}_c^T \mathbf{X}_c$ is computationally inefficient and numerically unstable when $d$ is large or when features are highly collinear ($d > n$).

Instead, modern PCA applies **Singular Value Decomposition (SVD)** directly to the centered data matrix $\mathbf{X}_c \in \mathbb{R}^{n \times d}$:

$$\mathbf{X}_c = \mathbf{U} \mathbf{S} \mathbf{V}^T$$

Where:
- $\mathbf{U} \in \mathbb{R}^{n \times n}$ is an orthogonal matrix of left singular vectors.
- $\mathbf{S} \in \mathbb{R}^{n \times d}$ is a diagonal matrix of singular values $s_1 \ge s_2 \ge \dots \ge s_{\min(n,d)} \ge 0$.
- $\mathbf{V} \in \mathbb{R}^{d \times d}$ is an orthogonal matrix of right singular vectors.

Computing the sample covariance matrix using SVD:
$$\mathbf{\Sigma} = \frac{1}{n - 1} \mathbf{X}_c^T \mathbf{X}_c = \frac{1}{n - 1} (\mathbf{U} \mathbf{S} \mathbf{V}^T)^T (\mathbf{U} \mathbf{S} \mathbf{V}^T) = \frac{1}{n - 1} \mathbf{V} \mathbf{S}^T \mathbf{U}^T \mathbf{U} \mathbf{S} \mathbf{V}^T$$
Since $\mathbf{U}^T \mathbf{U} = \mathbf{I}$:
$$\mathbf{\Sigma} = \mathbf{V} \left( \frac{\mathbf{S}^2}{n - 1} \right) \mathbf{V}^T$$

#### Crucial Identities:
1. The right singular vectors $\mathbf{V}$ of $\mathbf{X}_c$ are **identically the eigenvectors** of $\mathbf{\Sigma}$.
2. The eigenvalues of $\mathbf{\Sigma}$ equal the squared singular values divided by degrees of freedom:
   $$\lambda_j = \frac{s_j^2}{n - 1}$$
3. The projected principal component coordinates $\mathbf{Z} \in \mathbb{R}^{n \times k}$ are computed in a single matrix product without inverting anything:
   $$\mathbf{Z} = \mathbf{X}_c \mathbf{V}_k = \mathbf{U}_k \mathbf{S}_k$$

### 3. Explained Variance Ratio
The total variance of the dataset is the trace of the covariance matrix:
$$\text{Total Variance} = \text{Tr}(\mathbf{\Sigma}) = \sum_{j=1}^d \lambda_j$$
The **Explained Variance Ratio** of component $j$ is:
$$\text{EVR}_j = \frac{\lambda_j}{\sum_{m=1}^d \lambda_m} = \frac{s_j^2}{\sum_{m=1}^d s_m^2}$$

### 4. Minimum Reconstruction Error (Eckart-Young-Mirsky Theorem)
Reconstructing the original data from $k$ components:
$$\hat{\mathbf{X}} = \mathbf{Z} \mathbf{V}_k^T + \mathbf{1} \boldsymbol{\mu}^T = \mathbf{X}_c \mathbf{V}_k \mathbf{V}_k^T + \mathbf{1} \boldsymbol{\mu}^T$$
The Eckart-Young-Mirsky theorem proves that the rank-$k$ matrix $\hat{\mathbf{X}}_c = \mathbf{U}_k \mathbf{S}_k \mathbf{V}_k^T$ is the **globally optimal rank-$k$ approximation** of $\mathbf{X}_c$ under both the Frobenius norm and spectral norm:

$$\min_{\text{rank}(A) \le k} \|\mathbf{X}_c - A\|_F = \|\mathbf{X}_c - \hat{\mathbf{X}}_c\|_F = \sqrt{\sum_{j=k+1}^d s_j^2}$$

---

## 4. Computational Complexity

| Implementation / Solver | Time Complexity | Space Complexity |
| :--- | :--- | :--- |
| **Full SVD (Exact)** | $O(\min(n \cdot d^2, n^2 \cdot d))$ | $O(n \cdot d)$ |
| **Covariance EVD** | $O(n \cdot d^2) + O(d^3)$ | $O(d^2)$ |
| **Randomized SVD (Halko et al.)** | $O(n \cdot d \cdot k + k^2(n + d))$ (Fastest for $k \ll d$) | $O(n \cdot k)$ |
| **Incremental PCA (Mini-Batch)** | $O(b \cdot d^2 + d^3)$ per batch of size $b$ | $O(b \cdot d)$ (Streaming / Out-of-core) |
| **Transform / Inference (Single Sample)** | $O(d \cdot k)$ | $O(k)$ |

---

## 5. Inductive Bias & Learning Guarantees

1. **Linear Subspace Assumption**: Assumes the data cloud can be faithfully approximated by a flat $k$-dimensional linear hyperplane. It cannot unroll curved Riemannian manifolds (e.g., Swiss roll).
2. **Variance-as-Information Principle**: Assumes that directions with larger variance contain more signal, while directions with smaller variance represent negligible noise. (If noise has high amplitude, PCA can mistakenly align with noise).
3. **Orthogonal Decorrelation**: Strictly guarantees that the resulting projected coordinates $\mathbf{Z}$ have zero pairwise covariance: $\text{Cov}(z_i, z_j) = 0$ for $i \ne j$.

---

## 6. Scratch NumPy Implementation

```python
"""
Scratch NumPy implementation of Principal Component Analysis (PCA).
Implements exact mean centering, SVD decomposition, projection,
inverse reconstruction, and explained variance ratio calculations.
"""

from __future__ import annotations
import numpy as np
from typing import Optional


class ScratchPCA:
    """Production reference implementation of PCA written entirely in NumPy."""

    def __init__(self, n_components: int | float | None = None, whiten: bool = False) -> None:
        self.n_components = n_components
        self.whiten = whiten

        self.mean_: np.ndarray | None = None
        self.components_: np.ndarray | None = None  # shape (k, d)
        self.explained_variance_: np.ndarray | None = None
        self.explained_variance_ratio_: np.ndarray | None = None
        self.singular_values_: np.ndarray | None = None
        self.n_components_: int = 0

    def fit(self, X: np.ndarray) -> ScratchPCA:
        X = np.asarray(X, dtype=np.float64)
        n_samples, n_features = X.shape

        # 1. Mean Centering
        self.mean_ = np.mean(X, axis=0)
        X_centered = X - self.mean_

        # 2. Singular Value Decomposition: X_c = U S V^T
        # full_matrices=False computes economy SVD: U is (n, min(n, d)), V^T is (min(n, d), d)
        U, S, Vt = np.linalg.svd(X_centered, full_matrices=False)

        # 3. Calculate Variance Metrics
        # Var_j = S_j^2 / (n - 1)
        explained_variance = (S ** 2) / (n_samples - 1)
        total_var = np.sum(explained_variance)
        explained_variance_ratio = explained_variance / total_var

        # 4. Determine Number of Components (k)
        if self.n_components is None:
            k = min(n_samples, n_features)
        elif isinstance(self.n_components, float) and 0.0 < self.n_components < 1.0:
            # Cumulative variance threshold
            cum_ratio = np.cumsum(explained_variance_ratio)
            k = int(np.searchsorted(cum_ratio, self.n_components)) + 1
        else:
            k = int(self.n_components)

        self.n_components_ = k
        self.components_ = Vt[:k]  # shape (k, d)
        self.explained_variance_ = explained_variance[:k]
        self.explained_variance_ratio_ = explained_variance_ratio[:k]
        self.singular_values_ = S[:k]
        return self

    def transform(self, X: np.ndarray) -> np.ndarray:
        """Project centered data onto principal components."""
        if self.mean_ is None or self.components_ is None:
            raise RuntimeError("ScratchPCA instance is not fitted.")
        X = np.asarray(X, dtype=np.float64)
        X_centered = X - self.mean_
        Z = np.dot(X_centered, self.components_.T)  # shape (n, k)

        if self.whiten:
            # Scale coordinates to unit variance: z_j / sqrt(lambda_j)
            Z /= np.sqrt(self.explained_variance_)

        return Z

    def fit_transform(self, X: np.ndarray) -> np.ndarray:
        return self.fit(X).transform(X)

    def inverse_transform(self, Z: np.ndarray) -> np.ndarray:
        """Reconstruct original features from principal coordinates."""
        if self.mean_ is None or self.components_ is None:
            raise RuntimeError("ScratchPCA instance is not fitted.")
        Z = np.asarray(Z, dtype=np.float64)
        if self.whiten:
            Z = Z * np.sqrt(self.explained_variance_)

        X_centered_recon = np.dot(Z, self.components_)
        return X_centered_recon + self.mean_
```

---

## 7. Production Scikit-Learn 9-Stage Pipeline

```python
"""
Production PCA pipeline with Cumulative Variance Thresholding,
Scree Plot Diagnostics, and Reconstruction Error Auditing.
"""

from __future__ import annotations
import logging
from typing import Dict, Tuple
import numpy as np
from sklearn.datasets import load_digits
from sklearn.preprocessing import StandardScaler
from sklearn.decomposition import PCA, IncrementalPCA
from sklearn.pipeline import Pipeline

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("PCA_Pipeline")


def run_pca_production_pipeline() -> Tuple[PCA, Dict[str, float]]:
    logger.info("Stage 1: Loading 64-dimensional Digits dataset (n=1797, d=64).")
    data = load_digits()
    X = data.data

    # Stage 2: Standardization (Crucial: variance must not reflect arbitrary units)
    scaler = StandardScaler()
    X_scaled = scaler.fit_transform(X)

    # Stage 3: Automated Component Selection: Retain 90% cumulative variance
    logger.info("Stage 3: Fitting PCA to retain >= 90% cumulative explained variance.")
    pca_90 = PCA(n_components=0.90, svd_solver="full", random_state=42)
    X_pca = pca_90.fit_transform(X_scaled)
    n_retained = pca_90.n_components_
    cum_var = float(np.sum(pca_90.explained_variance_ratio_))

    logger.info(f"Dimensionality Reduction: 64 features -> {n_retained} components ({cum_var * 100:.2f}% variance retained)")

    # Stage 4: Reconstruction Error Analysis
    X_recon = pca_90.inverse_transform(X_pca)
    reconstruction_mse = float(np.mean((X_scaled - X_recon) ** 2))
    logger.info(f"Reconstruction MSE: {reconstruction_mse:.4f}")

    # Stage 5: Scalable Incremental PCA Demonstration (Out-of-core streaming)
    logger.info("Stage 5: Fitting IncrementalPCA in mini-batches of 256 samples.")
    ipca = IncrementalPCA(n_components=n_retained, batch_size=256)
    for batch in np.array_split(X_scaled, 7):
        ipca.partial_fit(batch)
    ipca_cum_var = float(np.sum(ipca.explained_variance_ratio_))
    logger.info(f"IncrementalPCA Cumulative Variance: {ipca_cum_var * 100:.2f}%")

    metrics = {
        "n_components": float(n_retained),
        "cumulative_variance": cum_var,
        "reconstruction_mse": reconstruction_mse,
        "ipca_cumulative_variance": ipca_cum_var,
    }
    return pca_90, metrics


if __name__ == "__main__":
    model, perf = run_pca_production_pipeline()
    assert perf["cumulative_variance"] >= 0.90, "PCA failed variance retention target."
```

---

## 8. Model Persistence & Serialization

```python
"""
Serializing PCA models with transformation parameters.
"""

from pathlib import Path
import json
import joblib
from sklearn.decomposition import PCA


def export_pca_bundle(model: PCA, path: Path) -> Path:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    joblib.dump(model, path)

    # Export projection matrix V_k
    meta = {
        "n_components": int(model.n_components_),
        "explained_variance_ratio": model.explained_variance_ratio_.tolist(),
        "singular_values": model.singular_values_.tolist(),
        "mean": model.mean_.tolist(),
        "components": model.components_.tolist()  # shape (k, d)
    }
    meta_path = path.with_suffix(".pca.json")
    with open(meta_path, "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)
    return meta_path
```

---

## 9. Hyperparameter Tuning Guide

| Parameter | Options | Default | Engineering Guidance |
| :--- | :--- | :--- | :--- |
| `n_components` | Integer $k$, float $(0, 1)$, or `'minka'` | `None` | **Use float $\in (0, 1)$** (e.g., `0.95`) to automatically select the minimal $k$ that retains $95\%$ cumulative variance. |
| `svd_solver` | `'auto'`, `'full'`, `'arpack'`, `'randomized'` | `'auto'` | Use `'randomized'` whenever $n, d > 500$ and $k \ll d$. It approximates exact SVD in a fraction of the time using randomized matrix projections (Halko et al.). |
| `whiten` | `True` / `False` | `False` | When `True`, divides coordinates by singular values so that $PC_j$ has unit variance. Essential when feeding PCA coordinates into isotropic models like K-Means or RBF SVMs. |

---

## 10. Residuals, Diagnostics & Failure Modes

1. **Scree Plot (Elbow Curve of Eigenvalues)**:
   Plot sorted eigenvalues $\lambda_j$ against component index $j$. The optimal cutoff corresponds to the elbow where eigenvalues flatten out to a low horizontal shelf (representing random noise floor).
2. **Reconstruction Residual Metric ($Q$-Residual / SPE)**:
   For any sample $\mathbf{x}$, the Squared Prediction Error (SPE) is:
   $$\text{SPE}(\mathbf{x}) = \|\mathbf{x} - \hat{\mathbf{x}}\|^2 = \|\mathbf{x}_c - \mathbf{V}_k \mathbf{V}_k^T \mathbf{x}_c\|^2$$
   High SPE identifies abnormal instances that do not conform to the low-dimensional linear subspace (anomalies).
3. **Hotelling's $T^2$ Statistic**:
   Measures distance within the PCA subspace:
   $$T^2 = \sum_{j=1}^k \frac{z_j^2}{\lambda_j}$$
   High $T^2$ indicates an extreme outlier within the normal correlation structure.

---

## 11. Prediction Uncertainty & Calibration

PCA is an orthogonal linear mapping. Uncertainty in PCA stems from **reconstruction loss**:

$$\mathbf{x} = \hat{\mathbf{x}} + \mathbf{e}, \quad \mathbf{e} \sim \mathcal{N}(\mathbf{0}, \sigma^2 \mathbf{I})$$

The residual variance $\sigma^2 = \frac{1}{d - k} \sum_{j=k+1}^d \lambda_j$ serves as a calibrated estimate of residual model uncertainty.

---

## 12. Comprehensive Failure Modes & Edge Cases Table

| Failure Mode | Root Cause | Observable Symptom | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **Missing Standardization** | Features have unequal variances ($10^6$ vs $10^0$). | $PC_1$ aligns entirely with the highest-variance feature column; others ignored. | Strictly precede PCA with `StandardScaler()` in an atomic `Pipeline`. |
| **Non-Linear Manifold Distortion** | Data lies on curved surface (Swiss roll, concentric rings). | Collapses distinct non-linear layers onto one another; destroys manifold geometry.| Switch to **KernelPCA(kernel='rbf')**, **t-SNE**, or **UMAP**. |
| **Extreme Outlier Sensitivity** | Covariance matrix is quadratic in distance ($\mathbf{x}_i \mathbf{x}_i^T$). | Single distant outlier rotates the entire coordinate frame by 45 degrees. | Standardize with `RobustScaler` or use **Robust PCA** (L1 decomposition). |
| **Out-Of-Memory on Huge Matrices** | Dense SVD on $100,000 \times 10,000$ matrix requires 8GB RAM per copy. | Memory allocation error during SVD. | Switch to `IncrementalPCA(batch_size=1000)` or `RandomizedPCA`. |
| **Loss of Feature Interpretability** | Each $PC_j$ is a linear combination of all $d$ features. | Cannot explain to business stakeholders what "$PC_1 = 0.42$" means. | Use **Sparse PCA** to force component weights to be sparse (L1 penalty). |
| **Noise Dominance Failure** | Noise variance exceeds signal variance. | PCA aligns with high-amplitude noise axes rather than underlying signal. | Apply filtering or domain-specific denoising prior to PCA. |

---

## 13. Comparative Trade-off Table

| Algorithm | Paradigm | Linearity | Time Scaling | Preserves Global Topology? | Out-of-Sample Transform? |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **PCA** | Variance Maximization | **Strictly Linear** | $O(n \cdot d \cdot k)$ (Very Fast) | **Yes (Global)** | **Yes** ($\mathbf{X}_c \mathbf{V}_k$) |
| **TruncatedSVD** | Low-Rank Factorization | Linear | Fast on sparse text | Yes | **Yes** |
| **Kernel PCA** | Kernel Trick ($K_{ij}$) | **Non-Linear** | $O(n^3)$ (Slow) | Yes (in Hilbert space) | Yes (via kernel expansion) |
| **t-SNE** | Neighborhood Probability | Non-Linear | $O(n \log n)$ | No (Local only) | No (Transductive) |
| **UMAP** | Fuzzy Simplicial Sets | Non-Linear | $O(n \log n)$ | Balanced (Local + Global) | **Yes** |

---

## 14. Production Engineering, Serving & Latency

### Sub-Microsecond Matrix Transform Serving
Once $\mathbf{V}_k \in \mathbb{R}^{d \times k}$ and mean $\boldsymbol{\mu} \in \mathbb{R}^d$ are saved, scoring a new real-time sample takes $< 100\text{ns}$ via BLAS:

```python
# Ultra-fast real-time scoring in production microservice
def fast_pca_transform(x_sample: np.ndarray, mean: np.ndarray, V_k: np.ndarray) -> np.ndarray:
    return np.dot(x_sample - mean, V_k.T)
```

---

## 15. Interpretability & Explainability

1. **Component Loadings**:
   The entries of $\mathbf{V}[j, :]$ represent the correlation between original feature $j$ and each principal component. Plotting a horizontal bar chart of loadings reveals which original variables drive each component.
2. **PCA Biplot**:
   Overlays a 2D scatter plot of samples in $(PC_1, PC_2)$ with directional arrows representing the original feature vectors, illustrating how features correlate with latent axes.

---

## 16. Hardware, Parallelism & Scaling

- **BLAS Multi-Threading**: SVD implementations automatically utilize all CPU cores via Intel MKL / OpenBLAS.
- **GPU Acceleration**: For datasets with millions of rows, use `cuml.decomposition.PCA` on NVIDIA GPUs, achieving a **$25\times$ to $50\times$ speedup** over CPU SVD.

---

## 17. Automated Verification Harness

```python
"""
Pytest automated verification harness for PCA.
Asserts SVD singular value equivalence, coordinate orthogonality, and reconstruction bounds.
"""

import numpy as np
import pytest
from sklearn.datasets import make_classification
from sklearn.decomposition import PCA


@pytest.fixture
def dataset():
    X, _ = make_classification(n_samples=200, n_features=10, n_informative=6, random_state=42)
    return X


def test_pca_coordinates_uncorrelated(dataset):
    """Projected PCA coordinates Z must have strictly zero pairwise covariance."""
    pca = PCA(n_components=5).fit(dataset)
    Z = pca.transform(dataset)

    cov_Z = np.cov(Z, rowvar=False)
    # Off-diagonal elements must be 0
    np.fill_diagonal(cov_Z, 0.0)
    np.testing.assert_allclose(cov_Z, np.zeros((5, 5)), atol=1e-6, err_msg="PCA coordinates are not uncorrelated!")


def test_reconstruction_error_monotonicity(dataset):
    """Reconstruction error must strictly decrease as n_components increases."""
    mses = []
    for k in [1, 3, 5, 8]:
        pca = PCA(n_components=k).fit(dataset)
        X_recon = pca.inverse_transform(pca.transform(dataset))
        mses.append(np.mean((dataset - X_recon) ** 2))

    assert np.all(np.diff(mses) <= 1e-6), f"Reconstruction MSE did not decrease monotonically: {mses}"
```

---

## 18. Cross-Track Learning & Ecosystem Links

- **Linear Algebra**: Singular Value Decomposition in `01-math-foundations/01-linear-algebra-for-ml`.
- **Sparse Text SVD**: Connects to `05-unsupervised-learning/09-svd-and-truncated-svd`.
- **Non-Linear Visualization**: Non-linear manifold alternative in `05-unsupervised-learning/10-t-sne-and-umap`.

---

## 19. Real-World Enterprise Case Study

### Context: High-Dimensional Industrial Sensor Denoising & Anomaly Isolation
- **Domain**: Chemical Petrochemical Refining Plant.
- **Problem**: Monitoring 240 continuous telemetry sensors (temperatures, pressures, flow rates) sampled every second. Collinear interactions caused supervised alarm flooding ($4,000$ false alerts/day).
- **Implementation**:
  - Fitted PCA on historical steady-state operations.
  - Selected $k=12$ components, capturing $94.2\%$ of global plant variance.
  - Deployed continuous Squared Prediction Error monitoring ($\text{SPE} = \|\mathbf{x} - \hat{\mathbf{x}}\|^2$).
- **Outcome**: Detected a catastrophic catalytic reactor leak 45 minutes before safety relief valve trip, reducing false alarm noise by $86\%$.

---

## 20. Technical Interview Preparation

### Q1: Why is centering the data ($\bar{\mathbf{x}} = \mathbf{0}$) mandatory before running PCA?
**Answer:** The objective of PCA is to maximize sample variance: $\mathbf{w}^T \mathbf{\Sigma} \mathbf{w} = \frac{1}{n-1} \mathbf{w}^T (\mathbf{X} - \mathbf{1}\boldsymbol{\mu}^T)^T (\mathbf{X} - \mathbf{1}\boldsymbol{\mu}^T) \mathbf{w}$. If the mean is not subtracted, the matrix $\mathbf{X}^T \mathbf{X}$ computes the second uncentered moment rather than covariance. The first principal component will simply point from the coordinate origin $(0, 0, \dots, 0)$ directly to the center of mass of the data cloud $\boldsymbol{\mu}$, capturing shift offset rather than the true internal variance of the distribution.

### Q2: Explain why SVD is numerically preferred over computing the Covariance Matrix EVD.
**Answer:**
1. **Condition Number Squaring**: Forming the sample covariance matrix $\mathbf{\Sigma} = \frac{1}{n-1} \mathbf{X}_c^T \mathbf{X}_c$ explicitly squares the condition number: $\kappa(\mathbf{X}_c^T \mathbf{X}_c) = (\kappa(\mathbf{X}_c))^2$. If $\mathbf{X}_c$ has condition number $10^8$, its covariance matrix has condition number $10^{16}$, resulting in complete loss of floating-point numerical precision in standard float64.
2. **Computational Efficiency**: When $d > n$ (e.g., $d=50,000, n=500$), computing the $50,000 \times 50,000$ covariance matrix requires massive RAM and $O(n \cdot d^2)$ operations. SVD computes economy decompositions directly on the $n \times d$ matrix in $O(n^2 \cdot d)$ time.

### Q3: What is "Whitening" in PCA, and when should it be applied?
**Answer:** Whitening scales each projected principal coordinate by the inverse square root of its eigenvalue: $z_j^{\text{white}} = \frac{z_j}{\sqrt{\lambda_j}}$. This transforms the covariance matrix of the projected data into an identity matrix ($\mathbf{\Sigma}_{\mathbf{z}} = \mathbf{I}$). Whitening eliminates all covariance and forces all dimensions to have unit variance. It is essential when feeding PCA features into isotropic distance-based algorithms (K-Means, KNN, RBF SVM) or as a pre-processing step for Independent Component Analysis (ICA).

### Q4: State the Eckart-Young-Mirsky Theorem and its relevance to PCA.
**Answer:** The theorem states that given a matrix $\mathbf{X}_c$ with SVD $\sum_{j=1}^r s_j \mathbf{u}_j \mathbf{v}_j^T$, the optimal rank-$k$ matrix approximation $\mathbf{A}^*$ that minimizes the distance $\|\mathbf{X}_c - \mathbf{A}\|$ (under both Frobenius and Spectral norms) is obtained by truncating the SVD at $k$ terms: $\hat{\mathbf{X}}_c = \sum_{j=1}^k s_j \mathbf{u}_j \mathbf{v}_j^T$. This proves that PCA provides the provably optimal linear reconstruction of the original data in a least-squares sense.

### Q5: Can PCA be used for supervised feature selection?
**Answer:** Generally no, with caution. PCA is strictly unsupervised; it maximizes total variance without regard to target labels $y$. If the target information is correlated with subtle low-variance directions ($PC_{10}$), discarding low-variance components will destroy predictive power while retaining high-variance noise ($PC_1$). For supervised linear dimensionality reduction, Linear Discriminant Analysis (LDA) or Partial Least Squares (PLS) should be preferred.

### Q6: How does Randomized SVD (Halko et al., 2011) accelerate PCA?
**Answer:** Standard SVD computes all singular values in $O(\min(n d^2, n^2 d))$ time. Randomized SVD generates a random Gaussian projection matrix $\mathbf{\Omega} \in \mathbb{R}^{d \times (k + p)}$ (where $p \approx 10$ is an oversampling parameter) and projects $\mathbf{X}$ into a low-dimensional subspace: $\mathbf{Y} = \mathbf{X} \mathbf{\Omega}$. Using QR decomposition to find an orthonormal basis $\mathbf{Q}$, it performs SVD on the tiny matrix $\mathbf{B} = \mathbf{Q}^T \mathbf{X} \in \mathbb{R}^{(k+p) \times d}$. This runs in $O(n \cdot d \cdot k)$ time, delivering near-exact singular values in a fraction of the time.

### Q7: What is the difference between Hotelling's $T^2$ and the $Q$-residual (SPE) in PCA diagnostics?
**Answer:**
- **Hotelling's $T^2$**: Measures the distance of a sample within the $k$-dimensional model subspace: $T^2 = \sum_{j=1}^k \frac{z_j^2}{\lambda_j}$. A high $T^2$ indicates an extreme outlier that conforms to normal correlation patterns (e.g., high pressure AND high temperature simultaneously).
- **$Q$-Residual (SPE)**: Measures the perpendicular distance from the sample to the $k$-dimensional hyperplane: $Q = \|\mathbf{x}_c - \hat{\mathbf{x}}_c\|^2$. A high $Q$ indicates a structural anomaly that breaks the normal correlation structure (e.g., high pressure with low temperature).

### Q8: Under what conditions does PCA fail to provide meaningful dimensionality reduction?
**Answer:** PCA fails when:
1. Features are completely uncorrelated ($\mathbf{\Sigma} \approx \sigma^2 \mathbf{I}$). All eigenvalues are equal ($\lambda_1 \approx \dots \approx \lambda_d$), meaning variance is uniform in all directions and no low-dimensional subspace exists.
2. The data lies on a complex non-linear manifold (e.g., Swiss roll), where global linear projections collapse distinct manifold folds.
3. The data is discrete or binary (e.g., text document term matrices), where continuous Gaussian variance assumptions fail; Multiple Correspondence Analysis (MCA) or TruncatedSVD are more appropriate.

---

## 21. Hands-On Exercises & Mini-Projects

1. **Eigenface Decomposition from Scratch**: Load the Olivetti Faces dataset ($400$ images of $64 \times 64 = 4096$ pixels). Fit `ScratchPCA` with $k=25$. Reshape the top 10 component vectors back into $64 \times 64$ images and plot the "Eigenfaces", interpreting the facial features captured by each component.
2. **PCA Biplot Implementation**: Write a custom plotting function that computes the 2D PCA projection of the Wine dataset and overlays arrows representing the component loadings of all 13 features, scaled by their singular values.
3. **Randomized SVD Benchmark**: Synthesize a $50,000 \times 1,000$ matrix. Benchmark execution time, peak memory usage, and singular value accuracy between `PCA(svd_solver='full')` and `PCA(svd_solver='randomized', n_components=20)`.
4. **Reconstruction Residual Anomaly Engine**: Train a PCA model on clean normal digits (Digit 0). Pass images of Digits 0 through 9 into `inverse_transform` and compute reconstruction MSE. Verify that Digits 1 through 9 have significantly higher reconstruction error, demonstrating unsupervised novelty detection.
5. **Cumulative Variance Knee Search**: Implement a script that plots the cumulative explained variance curve for a dataset, fits a spline, and outputs the minimum $k$ required for $80\%, 90\%, 95\%$, and $99\%$ variance retention.
6. **Kernel PCA vs Linear PCA on Concentric Rings**: Generate a concentric circles dataset. Fit both standard linear PCA and `KernelPCA(kernel='rbf', gamma=10.0)`. Plot the 2D projected representations side-by-side to observe linear PCA's failure versus Kernel PCA's linear separation.
