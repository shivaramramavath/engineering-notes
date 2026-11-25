# Linear & Quadratic Discriminant Analysis (LDA & QDA)

> **Track:** `05-ai/machine-learning` | **Module:** `04-supervised-classification` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `NumPy: >=1.26.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Linear Discriminant Analysis (LDA / Fisher's Discriminant) & Quadratic Discriminant Analysis (QDA) |
| **Family** | Generative Classifiers / Maximum Fisher Separation / Gaussian Mixture Modeling |
| **Input Feature Types** | Continuous numerical features ($X \in \mathbb{R}^{n \times d}$). Assumes multivariate normality within each class. |
| **Target Output** | Multiclass labels $y \in \{1, \dots, K\}$ and exact Bayesian posterior probabilities $P(y = k \mid \mathbf{x})$. Supervised dimensional projection $\mathbf{z} \in \mathbb{R}^{\min(d, K-1)}$. |
| **Optimization Target** | Maximizing Fisher's criterion (ratio of between-class variance to within-class variance): $J(\mathbf{w}) = \frac{\mathbf{w}^T S_B \mathbf{w}}{\mathbf{w}^T S_W \mathbf{w}}$. |
| **Generative Assumption** | **LDA**: Each class is normally distributed $\mathcal{N}(\boldsymbol{\mu}_k, \boldsymbol{\Sigma})$ with class-specific mean $\boldsymbol{\mu}_k$ and a **shared pooled covariance matrix** $\boldsymbol{\Sigma}$. **QDA**: Each class has its own individual covariance matrix $\boldsymbol{\Sigma}_k$. |
| **Optimization Solvers** | Singular Value Decomposition (SVD), Eigenvalue Decomposition (EVD), Least Squares (`'lsqr'`) with Ledoit-Wolf / OAS covariance shrinkage. Closed-form analytic solution (zero iterative gradient descent required). |
| **Key Hyperparameters** | `solver` (`'svd'`, `'lsqr'`, `'eigen'`), `shrinkage` (`None`, `'auto'`, float $[0, 1]$), `priors`, `tol`. |
| **Inference Mechanism** | Deterministic linear or quadratic scoring function: $\delta_k(\mathbf{x}) = \mathbf{x}^T \boldsymbol{\Sigma}^{-1} \boldsymbol{\mu}_k - \frac{1}{2} \boldsymbol{\mu}_k^T \boldsymbol{\Sigma}^{-1} \boldsymbol{\mu}_k + \log(\pi_k)$. |
| **Primary Failure Modes** | Singular covariance matrix when $d > n$ (requires shrinkage); breakdown when feature distributions are severely non-Gaussian or multimodal; extreme vulnerability to heavy-tailed outliers. |

---

## 2. Architecture & Intuition

Linear Discriminant Analysis operates under two complementary mathematical paradigms:
1. **Fisher's Geometric Perspective (Supervised Dimensionality Reduction)**:
   Finds a linear projection vector $\mathbf{w}$ that simultaneously maximizes the separation between class centroids while minimizing the spread (variance) of data points within each class.
2. **Bayesian Generative Perspective (Maximum A Posteriori)**:
   Models the conditional likelihood of features $P(\mathbf{x} \mid y = k)$ as a multivariate normal distribution $\mathcal{N}(\boldsymbol{\mu}_k, \boldsymbol{\Sigma})$. By Bayes' theorem, the posterior class probability is computed directly.

```
+--------------------------------------------------------------------------------------------------+
|                            LDA vs QDA DECISION BOUNDARY TOPOLOGY                                 |
|                                                                                                  |
|              LDA: Shared Pooled Covariance                  QDA: Class-Specific Covariance       |
|              Produces Linear Boundaries                     Produces Quadratic / Curved Margins  |
|                                                                                                  |
|          x2 ^           Class 2                         x2 ^          Class 2 (Wide Variance)    |
|             |          (   )                               |         (       )                   |
|             |           \                                  |        (         )                  |
|             |     Linear \ Boundary                        |         \       /                   |
|             |             \                                |          \     /  Parabolic         |
|             |              \                               |           \   /   Boundary          |
|             |           (   )                              |            ( )                      |
|             |          Class 1                             |          Class 1 (Tight Variance)   |
|             +------------------------> x1                  +------------------------> x1         |
+--------------------------------------------------------------------------------------------------+
```

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $K$ | $\mathbb{N}$ | Total number of discrete target classes |
| $n$ | $\mathbb{N}$ | Total number of training samples ($n = \sum n_k$) |
| $d$ | $\mathbb{N}$ | Feature space dimensionality |
| $\pi_k$ | $(0, 1]$ | Prior probability of class $k$: $\pi_k = \frac{n_k}{n}$ |
| $\boldsymbol{\mu}_k$ | $\mathbb{R}^d$ | Empirical mean vector of class $k$: $\boldsymbol{\mu}_k = \frac{1}{n_k} \sum_{i \in \mathcal{C}_k} \mathbf{x}_i$ |
| $\boldsymbol{\mu}$ | $\mathbb{R}^d$ | Global empirical mean vector: $\boldsymbol{\mu} = \frac{1}{n} \sum_{i=1}^n \mathbf{x}_i$ |
| $S_W$ | $\mathbb{R}^{d \times d}$ | Within-class scatter matrix: $\sum_{k=1}^K \sum_{i \in \mathcal{C}_k} (\mathbf{x}_i - \boldsymbol{\mu}_k)(\mathbf{x}_i - \boldsymbol{\mu}_k)^T$ |
| $S_B$ | $\mathbb{R}^{d \times d}$ | Between-class scatter matrix: $\sum_{k=1}^K n_k (\boldsymbol{\mu}_k - \boldsymbol{\mu})(\boldsymbol{\mu}_k - \boldsymbol{\mu})^T$ |
| $\boldsymbol{\Sigma}$ | $\mathbb{R}^{d \times d}$ | Pooled sample covariance matrix: $\boldsymbol{\Sigma} = \frac{1}{n - K} S_W$ |
| $\boldsymbol{\Sigma}_k$ | $\mathbb{R}^{d \times d}$ | Class-specific sample covariance matrix (for QDA) |

### 1. Geometric Derivation: Fisher's Criterion
For binary classification ($K=2$), we project input $\mathbf{x} \in \mathbb{R}^d$ onto a 1D scalar $z = \mathbf{w}^T \mathbf{x}$.
- Projected means: $m_1 = \mathbf{w}^T \boldsymbol{\mu}_1$, $m_2 = \mathbf{w}^T \boldsymbol{\mu}_2$.
- Between-class centroid distance: $(m_1 - m_2)^2 = \mathbf{w}^T (\boldsymbol{\mu}_1 - \boldsymbol{\mu}_2)(\boldsymbol{\mu}_1 - \boldsymbol{\mu}_2)^T \mathbf{w} = \mathbf{w}^T S_B \mathbf{w}$.
- Projected within-class variance: $s_1^2 + s_2^2 = \mathbf{w}^T S_W \mathbf{w}$.

Fisher's objective maximizes the ratio of between-class spread to within-class spread:

$$J(\mathbf{w}) = \frac{\mathbf{w}^T S_B \mathbf{w}}{\mathbf{w}^T S_W \mathbf{w}}$$

To maximize $J(\mathbf{w})$, take the derivative with respect to $\mathbf{w}$ and set to zero:

$$\frac{\partial J}{\partial \mathbf{w}} = \frac{2 S_B \mathbf{w} (\mathbf{w}^T S_W \mathbf{w}) - 2 S_W \mathbf{w} (\mathbf{w}^T S_B \mathbf{w})}{(\mathbf{w}^T S_W \mathbf{w})^2} = 0$$
$$S_B \mathbf{w} = \frac{\mathbf{w}^T S_B \mathbf{w}}{\mathbf{w}^T S_W \mathbf{w}} S_W \mathbf{w} = \lambda S_W \mathbf{w}$$

This is the **Generalized Eigenvalue Problem**:
$$S_W^{-1} S_B \mathbf{w} = \lambda \mathbf{w}$$

Since $S_B \mathbf{w} = (\boldsymbol{\mu}_1 - \boldsymbol{\mu}_2)(\boldsymbol{\mu}_1 - \boldsymbol{\mu}_2)^T \mathbf{w}$ points in the direction of $(\boldsymbol{\mu}_1 - \boldsymbol{\mu}_2)$, the scalar factor can be dropped, yielding the exact closed-form solution:

$$\mathbf{w}^* = S_W^{-1} (\boldsymbol{\mu}_1 - \boldsymbol{\mu}_2) \propto \boldsymbol{\Sigma}^{-1} (\boldsymbol{\mu}_1 - \boldsymbol{\mu}_2)$$

### 2. Generative Bayesian Derivation of the Discriminant Function
By Bayes' theorem, the posterior probability of class $k$ is:

$$P(y = k \mid \mathbf{x}) = \frac{P(\mathbf{x} \mid y = k) \pi_k}{\sum_{j=1}^K P(\mathbf{x} \mid y = j) \pi_j}$$

Assuming multivariate normality:
$$P(\mathbf{x} \mid y = k) = \frac{1}{(2\pi)^{d/2} |\boldsymbol{\Sigma}_k|^{1/2}} \exp\left( -\frac{1}{2} (\mathbf{x} - \boldsymbol{\mu}_k)^T \boldsymbol{\Sigma}_k^{-1} (\mathbf{x} - \boldsymbol{\mu}_k) \right)$$

Taking the natural logarithm of $P(\mathbf{x} \mid y = k) \pi_k$:

$$\log P(\mathbf{x}, y = k) = -\frac{d}{2} \log(2\pi) - \frac{1}{2} \log |\boldsymbol{\Sigma}_k| - \frac{1}{2} (\mathbf{x} - \boldsymbol{\mu}_k)^T \boldsymbol{\Sigma}_k^{-1} (\mathbf{x} - \boldsymbol{\mu}_k) + \log(\pi_k)$$

#### A. Linear Discriminant Analysis (Equal Covariance: $\boldsymbol{\Sigma}_k = \boldsymbol{\Sigma}$)
When all classes share an identical covariance matrix $\boldsymbol{\Sigma}$, terms independent of $k$ ($-\frac{d}{2} \log(2\pi)$ and $-\frac{1}{2} \log |\boldsymbol{\Sigma}|$) can be discarded. Expanding the quadratic term:

$$(\mathbf{x} - \boldsymbol{\mu}_k)^T \boldsymbol{\Sigma}^{-1} (\mathbf{x} - \boldsymbol{\mu}_k) = \mathbf{x}^T \boldsymbol{\Sigma}^{-1} \mathbf{x} - 2 \mathbf{x}^T \boldsymbol{\Sigma}^{-1} \boldsymbol{\mu}_k + \boldsymbol{\mu}_k^T \boldsymbol{\Sigma}^{-1} \boldsymbol{\mu}_k$$

Since $\mathbf{x}^T \boldsymbol{\Sigma}^{-1} \mathbf{x}$ is constant across all candidate classes $k$, it cancels out! The resulting **Linear Discriminant Function** is strictly affine:

$$\delta_k(\mathbf{x}) = \mathbf{x}^T \boldsymbol{\Sigma}^{-1} \boldsymbol{\mu}_k - \frac{1}{2} \boldsymbol{\mu}_k^T \boldsymbol{\Sigma}^{-1} \boldsymbol{\mu}_k + \log(\pi_k)$$

The decision boundary between class $j$ and class $k$ occurs where $\delta_j(\mathbf{x}) = \delta_k(\mathbf{x})$, forming a **strictly linear hyperplane**:

$$\mathbf{x}^T \boldsymbol{\Sigma}^{-1} (\boldsymbol{\mu}_j - \boldsymbol{\mu}_k) - \frac{1}{2} \boldsymbol{\mu}_j^T \boldsymbol{\Sigma}^{-1} \boldsymbol{\mu}_j + \frac{1}{2} \boldsymbol{\mu}_k^T \boldsymbol{\Sigma}^{-1} \boldsymbol{\mu}_k + \log\left(\frac{\pi_j}{\pi_k}\right) = 0$$

#### B. Quadratic Discriminant Analysis (Unequal Covariance: $\boldsymbol{\Sigma}_k \neq \boldsymbol{\Sigma}_j$)
When each class retains its own covariance matrix $\boldsymbol{\Sigma}_k$, the quadratic term $\mathbf{x}^T \boldsymbol{\Sigma}_k^{-1} \mathbf{x}$ and determinant $\log |\boldsymbol{\Sigma}_k|$ do not cancel:

$$\delta_k^{\text{QDA}}(\mathbf{x}) = -\frac{1}{2} \log |\boldsymbol{\Sigma}_k| - \frac{1}{2} (\mathbf{x} - \boldsymbol{\mu}_k)^T \boldsymbol{\Sigma}_k^{-1} (\mathbf{x} - \boldsymbol{\mu}_k) + \log(\pi_k)$$

The decision boundary $\delta_j^{\text{QDA}}(\mathbf{x}) = \delta_k^{\text{QDA}}(\mathbf{x})$ is a **second-order quadratic surface** (ellipsoid, paraboloid, or hyperboloid).

---

## 4. Computational Complexity

| Phase | Linear Discriminant Analysis (LDA) | Quadratic Discriminant Analysis (QDA) |
| :--- | :--- | :--- |
| **Training (Covariance Estimation)** | $O(n \cdot d^2) + O(d^3)$ (One $d \times d$ pooled inversion) | $O(n \cdot d^2) + K \cdot O(d^3)$ ($K$ individual inversions) |
| **Inference (Single Sample)** | $O(K \cdot d)$ (Precomputed vector dot products) | $O(K \cdot d^2)$ (Matrix-vector quadratic products) |
| **Memory Footprint** | Storing $\boldsymbol{\Sigma}^{-1} \boldsymbol{\mu}_k \in \mathbb{R}^{d \times K}$ + bias scalars | Storing $K$ dense inverse covariance matrices $\boldsymbol{\Sigma}_k^{-1} \in \mathbb{R}^{K \times d \times d}$ |
| **Rank Constraint (Dimensionality)** | Projects into at most $\min(d, K - 1)$ subspace | Cannot project (operates directly in original space) |

---

## 5. Inductive Bias & Learning Guarantees

1. **Gaussian Feature Normality**: Assumes features are generated by multivariate Gaussian distributions. When normality holds and covariances are equal, LDA is the **asymptotically optimal Bayes classifier**.
2. **Homoscedasticity vs Heteroscedasticity**: LDA enforces homoscedasticity (shared variance), providing heavy regularisation that prevents overfitting on small datasets. QDA models heteroscedasticity, allowing variance to differ by class at the expense of estimating $K \times \frac{d(d+1)}{2}$ parameters.
3. **Subspace Constraint**: Between-class scatter $S_B$ has rank at most $K - 1$. Therefore, LDA can project data into at most $K - 1$ orthogonal dimensions regardless of how large $d$ is.

---

## 6. Scratch NumPy Implementation

```python
"""
Scratch NumPy implementation of Linear & Quadratic Discriminant Analysis.
Implements exact pooled/class covariance inversion, log-determinants, and shrinkage.
"""

from __future__ import annotations
import numpy as np
from typing import Optional, Tuple


class ScratchLDA:
    """Linear Discriminant Analysis with Ledoit-Wolf-style covariance shrinkage."""

    def __init__(self, shrinkage: Optional[float] = None) -> None:
        self.shrinkage = shrinkage
        self.priors_: np.ndarray | None = None
        self.means_: np.ndarray | None = None
        self.coef_: np.ndarray | None = None
        self.intercept_: np.ndarray | None = None
        self.classes_: np.ndarray | None = None

    def fit(self, X: np.ndarray, y: np.ndarray) -> ScratchLDA:
        X = np.asarray(X, dtype=np.float64)
        y = np.asarray(y)
        n_samples, n_features = X.shape
        self.classes_ = np.unique(y)
        n_classes = len(self.classes_)

        self.priors_ = np.zeros(n_classes, dtype=np.float64)
        self.means_ = np.zeros((n_classes, n_features), dtype=np.float64)
        pooled_cov = np.zeros((n_features, n_features), dtype=np.float64)

        # 1. Compute empirical means and class priors
        for k, c in enumerate(self.classes_):
            X_k = X[y == c]
            n_k = len(X_k)
            self.priors_[k] = n_k / n_samples
            self.means_[k] = np.mean(X_k, axis=0)
            diff = X_k - self.means_[k]
            pooled_cov += np.dot(diff.T, diff)

        # Unbiased pooled sample covariance
        pooled_cov /= (n_samples - n_classes)

        # 2. Apply Shrinkage if specified: Sigma_shrunk = (1 - gamma)*Sigma + gamma*nu*I
        if self.shrinkage is not None and self.shrinkage > 0.0:
            gamma = float(self.shrinkage)
            nu = np.trace(pooled_cov) / n_features
            pooled_cov = (1.0 - gamma) * pooled_cov + gamma * nu * np.eye(n_features)

        # 3. Invert pooled covariance via pseudo-inverse for numerical stability
        inv_cov = np.linalg.pinv(pooled_cov)

        # 4. Precompute linear weights and intercepts
        # W_k = Sigma^{-1} mu_k, b_k = -0.5 * mu_k^T Sigma^{-1} mu_k + log(pi_k)
        self.coef_ = np.dot(self.means_, inv_cov)  # shape (K, d)
        self.intercept_ = -0.5 * np.sum(self.coef_ * self.means_, axis=1) + np.log(self.priors_)

        return self

    def decision_function(self, X: np.ndarray) -> np.ndarray:
        """Compute linear discriminant scores delta_k(x) = W_k x + b_k."""
        X = np.asarray(X, dtype=np.float64)
        return np.dot(X, self.coef_.T) + self.intercept_

    def predict_proba(self, X: np.ndarray) -> np.ndarray:
        """Compute posterior probabilities via Softmax over discriminant scores."""
        scores = self.decision_function(X)
        # Numerically stable softmax
        exp_scores = np.exp(scores - np.max(scores, axis=1, keepdims=True))
        return exp_scores / np.sum(exp_scores, axis=1, keepdims=True)

    def predict(self, X: np.ndarray) -> np.ndarray:
        probs = self.predict_proba(X)
        return self.classes_[np.argmax(probs, axis=1)]


class ScratchQDA:
    """Quadratic Discriminant Analysis with class-specific covariance matrices."""

    def __init__(self, reg_param: float = 0.0) -> None:
        self.reg_param = reg_param
        self.classes_: np.ndarray | None = None
        self.priors_: np.ndarray | None = None
        self.means_: np.ndarray | None = None
        self.inv_covs_: list[np.ndarray] = []
        self.log_dets_: list[float] = []

    def fit(self, X: np.ndarray, y: np.ndarray) -> ScratchQDA:
        X = np.asarray(X, dtype=np.float64)
        y = np.asarray(y)
        n_samples, n_features = X.shape
        self.classes_ = np.unique(y)

        self.priors_ = []
        self.means_ = []
        self.inv_covs_ = []
        self.log_dets_ = []

        for c in self.classes_:
            X_k = X[y == c]
            n_k = len(X_k)
            self.priors_.append(n_k / n_samples)
            mean_k = np.mean(X_k, axis=0)
            self.means_.append(mean_k)

            diff = X_k - mean_k
            cov_k = np.dot(diff.T, diff) / (n_k - 1)

            # Regularisation: (1 - reg)*cov + reg*I
            if self.reg_param > 0.0:
                cov_k = (1.0 - self.reg_param) * cov_k + self.reg_param * np.eye(n_features)

            # SVD decomposition for stable inversion and log-determinant
            U, S, Vt = np.linalg.svd(cov_k)
            inv_cov = np.dot(Vt.T / S, U.T)
            log_det = float(np.sum(np.log(S)))

            self.inv_covs_.append(inv_cov)
            self.log_dets_.append(log_det)

        return self

    def predict_proba(self, X: np.ndarray) -> np.ndarray:
        X = np.asarray(X, dtype=np.float64)
        n_samples = len(X)
        scores = np.zeros((n_samples, len(self.classes_)), dtype=np.float64)

        for k in range(len(self.classes_)):
            diff = X - self.means_[k]
            # Vectorized quadratic form: sum((diff @ inv_cov) * diff, axis=1)
            quad = np.sum(np.dot(diff, self.inv_covs_[k]) * diff, axis=1)
            scores[:, k] = -0.5 * self.log_dets_[k] - 0.5 * quad + np.log(self.priors_[k])

        exp_scores = np.exp(scores - np.max(scores, axis=1, keepdims=True))
        return exp_scores / np.sum(exp_scores, axis=1, keepdims=True)

    def predict(self, X: np.ndarray) -> np.ndarray:
        return self.classes_[np.argmax(self.predict_proba(X), axis=1)]
```

---

## 7. Production Scikit-Learn 9-Stage Pipeline

```python
"""
Production pipeline comparing LDA with automated Ledoit-Wolf shrinkage against QDA.
"""

from __future__ import annotations
import logging
from typing import Dict, Tuple
import numpy as np
from sklearn.datasets import load_wine
from sklearn.model_selection import train_test_split, StratifiedKFold, cross_val_score
from sklearn.discriminant_analysis import LinearDiscriminantAnalysis, QuadraticDiscriminantAnalysis
from sklearn.preprocessing import StandardScaler
from sklearn.pipeline import Pipeline
from sklearn.metrics import classification_report, accuracy_score

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("LDA_Pipeline")


def run_discriminant_pipeline() -> Tuple[Pipeline, Dict[str, float]]:
    logger.info("Stage 1: Loading 13-feature multi-class Wine dataset.")
    data = load_wine()
    X, y = data.data, data.target

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.30, stratify=y, random_state=42
    )

    # Stage 2: LDA with Automatic Ledoit-Wolf Shrinkage
    # solver='lsqr' or 'eigen' is required to enable automated shrinkage
    lda_pipeline = Pipeline([
        ("scale", StandardScaler()),
        ("lda", LinearDiscriminantAnalysis(solver="lsqr", shrinkage="auto"))
    ])
    lda_pipeline.fit(X_train, y_train)

    # Stage 3: QDA Pipeline (Class-specific covariance)
    qda_pipeline = Pipeline([
        ("scale", StandardScaler()),
        ("qda", QuadraticDiscriminantAnalysis(reg_param=0.05))
    ])
    qda_pipeline.fit(X_train, y_train)

    # Stage 4: Out-of-sample Evaluation
    y_pred_lda = lda_pipeline.predict(X_test)
    y_pred_qda = qda_pipeline.predict(X_test)

    acc_lda = float(accuracy_score(y_test, y_pred_lda))
    acc_qda = float(accuracy_score(y_test, y_pred_qda))

    logger.info(f"LDA (Auto Shrinkage) Accuracy: {acc_lda:.4f}")
    logger.info(f"QDA (Reg=0.05) Accuracy: {acc_qda:.4f}")
    logger.info("\nLDA Classification Report:\n" + classification_report(y_test, y_pred_lda))

    return lda_pipeline, {"lda_acc": acc_lda, "qda_acc": acc_qda}


if __name__ == "__main__":
    pipeline, metrics = run_discriminant_pipeline()
    assert metrics["lda_acc"] >= 0.95, "LDA benchmark failed SLA."
```

---

## 8. Model Persistence & Serialization

```python
"""
Serializing LDA pipelines with projection matrices.
"""

from pathlib import Path
import joblib
from sklearn.pipeline import Pipeline


def persist_lda_pipeline(pipeline: Pipeline, path: Path) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    joblib.dump(pipeline, path, compress=("gzip", 3))
```

---

## 9. Hyperparameter Tuning Guide

| Parameter | Options | Default | Engineering Guidance |
| :--- | :--- | :--- | :--- |
| `solver` | `'svd'`, `'lsqr'`, `'eigen'` | `'svd'` | `'svd'` is fast and avoids explicit matrix inversion (does not support shrinkage). Use `'lsqr'` or `'eigen'` when covariance regularisation (`shrinkage`) is required. |
| `shrinkage` | `None`, `'auto'`, float $[0, 1]$ | `None` | Set to `'auto'` (Ledoit-Wolf shrinkage) whenever $d > n/2$ to prevent ill-conditioned covariance matrix inversion. |
| `reg_param` (QDA) | float $[0, 1]$ | `0.0` | Regularizes individual covariance matrices: $\hat{\boldsymbol{\Sigma}}_k = (1 - \alpha)\boldsymbol{\Sigma}_k + \alpha \mathbf{I}$. Prevents singular covariance collapse. |

---

## 10. Residuals, Diagnostics & Failure Modes

1. **Condition Number of Covariance Matrix**:
   Check $\kappa = \frac{\lambda_{\max}(\boldsymbol{\Sigma})}{\lambda_{\min}(\boldsymbol{\Sigma})}$. If $\kappa > 10^5$, the covariance matrix is ill-conditioned, leading to numerical divergence in inversion. Immediately enable `shrinkage='auto'`.
2. **Normality Violation (Shapiro-Wilk Test)**:
   Apply the Shapiro-Wilk test to features grouped by class. If $p < 0.01$, data is non-Gaussian; consider Logistic Regression or Tree ensembles instead.
3. **Multimodal Class Distributions**:
   If class 1 contains two distinct clusters (e.g., mixtures of Gaussians), LDA's single mean vector $\boldsymbol{\mu}_1$ will place the centroid in the empty space between clusters, causing misclassifications.

---

## 11. Prediction Uncertainty & Calibration

Because LDA is a generative probabilistic model, its posterior probabilities are derived directly from Bayes' rule using the multivariate normal density:

$$P(y = k \mid \mathbf{x}) = \frac{\exp(\delta_k(\mathbf{x}))}{\sum_{j=1}^K \exp(\delta_j(\mathbf{x}))}$$

When the Gaussian assumption holds, LDA probabilities are **natively well-calibrated**, requiring zero post-hoc Platt scaling.

---

## 12. Comprehensive Failure Modes & Edge Cases Table

| Failure Mode | Root Cause | Observable Symptom | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **Singular Matrix / Small Sample Size ($d > n$)** | Dimensionality exceeds sample size; rank of $\boldsymbol{\Sigma} \le n - K < d$. | `LinAlgError: Matrix is singular` during training. | Set `solver='lsqr'` and `shrinkage='auto'` (Ledoit-Wolf shrinkage). |
| **Multimodal Distribution Collapse** | Subclasses exist within a single class label. | Centroid placed in sparse space; boundary cuts through cluster center. | Use Gaussian Mixture Models (GMM) or non-linear classifiers. |
| **Extreme Outlier Sensitivity** | Sample covariance and mean are sensitive to extreme squared Euclidean distances. | Single distant outlier pulls entire hyperplane toward it. | Standardize with `RobustScaler` or use Minimum Covariance Determinant (MCD). |
| **High Dimensional Projection Bottleneck** | LDA projection rank is strictly capped at $\min(d, K - 1)$. | Cannot project $K=2$ binary classes to more than 1 dimension. | Use PCA or UMAP if multi-dimensional unsupervised projection is needed. |
| **QDA Overfitting ($K \cdot d^2$ parameters)** | Estimating $K$ separate covariance matrices when $n_k$ is small. | Perfect training score, collapsed test score on minority classes. | Add `reg_param=0.1` to regularize toward pooled covariance or fall back to LDA. |
| **Collinear Features** | Perfectly correlated feature pairs ($X_1 = 2 X_2$). | Zero eigenvalues in covariance matrix. | Remove redundant features via VarianceInflationFactor or PCA preprocessing. |

---

## 13. Comparative Trade-off Table

| Dimension | Linear Discriminant Analysis (LDA) | Quadratic Discriminant Analysis (QDA) | Multinomial Logistic Regression | Principal Component Analysis (PCA) |
| :--- | :--- | :--- | :--- | :--- |
| **Paradigm** | Generative ($P(\mathbf{x} \mid y)$) | Generative ($P(\mathbf{x} \mid y)$) | Discriminative ($P(y \mid \mathbf{x})$) | Unsupervised Reconstruction |
| **Decision Boundary**| Strictly Linear | Second-Order Quadratic | Strictly Linear | N/A (Dimensionality Reduction) |
| **Optimization** | Closed-form analytic (SVD/EVD)| Closed-form analytic | Iterative (L-BFGS / Newton) | Closed-form EVD on Covariance |
| **Assumptions** | Multivariate Gaussian + Equal Covariance | Multivariate Gaussian + Unequal Covariance | Linear log-odds (Distribution-free) | Orthogonal variance maximization |
| **Sample Efficiency**| High on small $n$ (if Gaussian)| Low (requires large $n_k$)| Moderate | High |

---

## 14. Production Engineering, Serving & Latency

### Sub-Microsecond Embedded Inference
Because LDA's decision rule is a simple linear product:
$$\hat{y} = \arg\max_k \left( \mathbf{w}_k^T \mathbf{x} + b_k \right)$$
Inference requires exactly **one matrix-vector multiplication** and **one vector addition**:
```c
// Ultra-low latency C serving loop (< 50 nanoseconds per sample)
float best_score = -1e9;
int best_class = 0;
for (int k = 0; k < K; k++) {
    float score = intercept[k];
    for (int j = 0; j < d; j++) {
        score += x[j] * coef[k][j];
    }
    if (score > best_score) {
        best_score = score;
        best_class = k;
    }
}
```

---

## 15. Interpretability & Explainability

1. **Discriminant Coefficients ($\mathbf{w}_k$)**:
   The magnitude and sign of coefficients $\boldsymbol{\Sigma}^{-1} \boldsymbol{\mu}_k$ indicate how strongly each standardized feature pushes an instance toward class $k$.
2. **Supervised 2D Projection**:
   Transform high-dimensional features into the first 2 Fisher canonical discriminant coordinates (`lda.transform(X)[:, :2]`) to visualize optimal supervised cluster separation.

---

## 16. Hardware, Parallelism & Scaling

- **Covariance Inversion Bottleneck**: Training requires computing $S_W$ and inverting it ($O(d^3)$). For $d > 10,000$ features (e.g., genomics), SVD/EVD can be distributed across multi-node GPUs using cuML.
- **Inference Scaling**: Scoring is embarrassingly parallel and vectorizable with AVX-512 BLAS routines.

---

## 17. Automated Verification Harness

```python
"""
Pytest automated verification harness for LDA and QDA.
Verifies Fisher projection rank bounds and discriminant score equivalence.
"""

import numpy as np
import pytest
from sklearn.datasets import load_iris
from sklearn.discriminant_analysis import LinearDiscriminantAnalysis


@pytest.fixture
def iris_data():
    X, y = load_iris(return_X_y=True)
    return X, y


def test_lda_subspace_rank_constraint(iris_data):
    """LDA projection dimension cannot exceed min(d, K - 1)."""
    X, y = iris_data
    # Iris has d=4 features, K=3 classes. Max components = 3 - 1 = 2
    lda = LinearDiscriminantAnalysis(n_components=2)
    X_trans = lda.fit_transform(X, y)

    assert X_trans.shape == (150, 2), f"Expected shape (150, 2), got {X_trans.shape}"


def test_lda_analytic_prediction_consistency(iris_data):
    """argmax of decision function must strictly match class predictions."""
    X, y = iris_data
    lda = LinearDiscriminantAnalysis().fit(X, y)

    df = lda.decision_function(X)
    preds = lda.predict(X)
    expected_preds = np.argmax(df, axis=1)

    np.testing.assert_array_equal(preds, expected_preds)
```

---

## 18. Cross-Track Learning & Ecosystem Links

- **Linear Algebra**: Connects to `01-math-foundations/01-linear-algebra-for-ml` (Eigenvalue decomposition and matrix inversion).
- **Bayesian Probability**: Extends `01-math-foundations/09-maximum-likelihood-and-bayes`.
- **Dimensionality Reduction**: Compare with unsupervised PCA in `05-unsupervised-learning/07-pca`.

---

## 19. Real-World Enterprise Case Study

### Context: Real-Time High-Frequency Quantitative Trading Signal Classification
- **Domain**: Algorithmic Market-Making Firm.
- **Problem**: Classifying incoming limit order book flow imbalances into 3 directional regimes (`[Buy-Liquidity, Neutral, Sell-Liquidity]`).
- **Hard SLA Constraint**: Inference latency must strictly execute in **under 400 nanoseconds** per tick; neural nets and tree ensembles are too slow.
- **Implementation**:
  - Trained `LinearDiscriminantAnalysis(solver='lsqr', shrinkage='auto')` on 8 micro-structural order-book features ($d=8$).
  - Precomputed weights $\mathbf{W} \in \mathbb{R}^{3 \times 8}$ and intercepts $\mathbf{b} \in \mathbb{R}^3$, compiled directly into FPGA / AVX-512 C++ firmware.
- **Outcome**: Achieved $74.2\%$ directional accuracy at an average execution latency of $120\text{ns}$, comfortably satisfying low-latency execution requirements.

---

## 20. Technical Interview Preparation

### Q1: Compare LDA and Logistic Regression. When would you prefer LDA over Logistic Regression?
**Answer:** Both are linear classifiers producing linear decision boundaries:
- **LDA** is a **generative model**: it models $P(\mathbf{x} \mid y)$ as Gaussian and uses Bayes' rule to find $P(y \mid \mathbf{x})$. When features are genuinely normally distributed, LDA is asymptotically efficient, requiring $\approx 30\%$ fewer training samples than Logistic Regression to reach optimal error rates. Furthermore, LDA has a closed-form analytic solution.
- **Logistic Regression** is a **discriminative model**: it models $P(y \mid \mathbf{x})$ directly with zero distributional assumptions on features $\mathbf{x}$. It is significantly more robust to non-Gaussian distributions, categorical variables, and heavy-tailed outliers.

### Q2: Why is the maximum number of projection dimensions in LDA capped at $\min(d, K - 1)$?
**Answer:** The between-class scatter matrix is $S_B = \sum_{k=1}^K n_k (\boldsymbol{\mu}_k - \boldsymbol{\mu})(\boldsymbol{\mu}_k - \boldsymbol{\mu})^T$. This is a sum of $K$ rank-1 matrices. Because the overall centroid is a linear combination of class centroids ($\boldsymbol{\mu} = \sum \pi_k \boldsymbol{\mu}_k$), only $K - 1$ of these vectors are linearly independent. Thus, the rank of $S_B$ is at most $K - 1$. The generalized eigenvalue problem $S_W^{-1} S_B \mathbf{w} = \lambda \mathbf{w}$ can yield at most $K - 1$ non-zero eigenvalues, restricting the projection subspace to $\min(d, K - 1)$.

### Q3: What is the purpose of Covariance Shrinkage (Ledoit-Wolf) in LDA?
**Answer:** When the sample size $n$ is small relative to feature dimensionality $d$ ($n \approx d$ or $n < d$), the sample covariance matrix $\boldsymbol{\Sigma}$ is rank-deficient or ill-conditioned, leading to wild, unstable eigenvalue estimates and singular matrix inversion errors. Shrinkage regularizes $\boldsymbol{\Sigma}$ by convexly interpolating between the empirical covariance and an isotropic identity matrix target:
$$\hat{\boldsymbol{\Sigma}}_{\text{shrunk}} = (1 - \gamma) \hat{\boldsymbol{\Sigma}} + \gamma \nu \mathbf{I}$$
The Ledoit-Wolf formula analytically computes the optimal shrinkage intensity $\gamma \in [0, 1]$ that minimizes mean squared error without cross-validation.

### Q4: Why does LDA produce a linear decision boundary while QDA produces a quadratic boundary?
**Answer:** In the log-posterior formulation:
$$\log P(y = k \mid \mathbf{x}) \propto -\frac{1}{2} \log |\boldsymbol{\Sigma}_k| - \frac{1}{2} (\mathbf{x} - \boldsymbol{\mu}_k)^T \boldsymbol{\Sigma}_k^{-1} (\mathbf{x} - \boldsymbol{\mu}_k) + \log(\pi_k)$$
In LDA, because all classes share an identical covariance matrix ($\boldsymbol{\Sigma}_k = \boldsymbol{\Sigma}$), the quadratic term $\mathbf{x}^T \boldsymbol{\Sigma}^{-1} \mathbf{x}$ is identical across all classes and cancels out in pairwise comparisons, leaving strictly linear terms. In QDA, each class has a distinct $\boldsymbol{\Sigma}_k$, so the second-order term $\mathbf{x}^T \boldsymbol{\Sigma}_k^{-1} \mathbf{x}$ does not cancel, generating a quadratic decision surface.

### Q5: How many parameters are estimated in QDA compared to LDA?
**Answer:** For $K$ classes and $d$ features:
- **LDA**: Estimates 1 shared covariance matrix ($\frac{d(d+1)}{2}$ unique entries), $K$ mean vectors ($K \cdot d$), and $K-1$ priors. Total parameters scale as $O(d^2 + K \cdot d)$.
- **QDA**: Estimates $K$ independent covariance matrices ($K \times \frac{d(d+1)}{2}$ entries), $K$ mean vectors ($K \cdot d$), and $K-1$ priors. Total parameters scale as $O(K \cdot d^2)$. QDA requires substantially more training data to avoid parameter overfitting.

### Q6: Can LDA be used for unsupervised dimensionality reduction like PCA?
**Answer:** No. PCA is strictly unsupervised; it finds orthogonal axes that maximize total variance $\mathbf{w}^T S_T \mathbf{w}$ without knowledge of target labels. LDA is strictly **supervised**; it finds axes that maximize class separation ($\frac{\mathbf{w}^T S_B \mathbf{w}}{\mathbf{w}^T S_W \mathbf{w}}$). An unsupervised dataset has no $S_B$ or $S_W$ matrices.

### Q7: What happens to LDA when two classes have identical means ($\boldsymbol{\mu}_1 = \boldsymbol{\mu}_2$) but different variances?
**Answer:** LDA completely fails. If $\boldsymbol{\mu}_1 = \boldsymbol{\mu}_2$, the between-class scatter matrix $S_B = 0$, meaning the Fisher projection vector $\mathbf{w}^* = \boldsymbol{\Sigma}^{-1}(\boldsymbol{\mu}_1 - \boldsymbol{\mu}_2) = \mathbf{0}$. LDA cannot distinguish classes by variance alone. QDA, however, models $\boldsymbol{\Sigma}_1 \neq \boldsymbol{\Sigma}_2$ and successfully separates concentric distributions using an elliptical boundary.

### Q8: What solver should you choose in Scikit-Learn if your dataset has 50,000 features and 500 samples?
**Answer:** Neither standard SVD nor EVD will work cleanly without regularisation due to extreme singularity ($d \gg n$). You must choose `solver='lsqr'` combined with `shrinkage='auto'`. The least-squares solver avoids computing explicit $50,000 \times 50,000$ covariance matrix inverses, while Ledoit-Wolf shrinkage ensures numerical stability.

---

## 21. Hands-On Exercises & Mini-Projects

1. **Fisher's Linear Projection from Scratch**: Using `ScratchLDA`, train on the first two classes of the Wine dataset. Compute the 1D projection vector $\mathbf{w}^*$ and plot a histogram of projected scores for Class 0 vs Class 1, illustrating Fisher separation.
2. **LDA vs PCA Supervised Visualization Benchmark**: Load the 64-feature Digits dataset ($K=10$). Project into 2D using PCA, and separately project into 2D using LDA. Plot the resulting scatter plots side-by-side to visualize how supervised LDA produces vastly tighter cluster separation than unsupervised PCA.
3. **Shrinkage Intensity Analysis**: Generate an ill-conditioned dataset with $n=50$ and $d=40$. Train LDA with shrinkage values $\gamma \in [0.0, 0.1, 0.5, 0.9, \text{'auto'}]$. Plot test accuracy as a function of shrinkage to identify the optimal regularization level.
4. **Concentric Circles Failure Case**: Generate a dataset of two concentric circles (`sklearn.datasets.make_circles`). Fit both LDA and QDA. Plot their decision boundaries to observe LDA's complete failure (linear boundary) versus QDA's quadratic capture.
5. **C++ Real-Time Inference Generator**: Write a Python script that exports the `coef_` and `intercept_` arrays of a fitted Scikit-Learn LDA model into a static `.h` C header file with a self-contained prediction function.
6. **Robustness to Extreme Outliers**: Add 3 extreme outliers to an otherwise Gaussian dataset. Compare how the decision boundary shifts for LDA versus Logistic Regression to demonstrate generative sensitivity to sample covariance corruption.
