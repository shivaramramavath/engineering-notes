# Non-Linear Manifold Visualization: t-SNE & UMAP

> **Track:** `05-ai/machine-learning` | **Module:** `05-unsupervised-learning` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `UMAP-Learn: >=0.5.5` | `NumPy: >=1.26.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | t-Distributed Stochastic Neighbor Embedding (t-SNE) & Uniform Manifold Approximation and Projection (UMAP) |
| **Family** | Non-Linear Manifold Learning / Topological Dimension Reduction / Probabilistic Embedding |
| **Input Feature Types** | Continuous high-dimensional features ($X \in \mathbb{R}^{n \times d}$) or custom distance metrics. Sensitive to distance metric choice. |
| **Target Output** | Low-dimensional non-linear spatial embeddings (strictly $\mathbb{R}^2$ or $\mathbb{R}^3$ for human visual exploration). |
| **Optimization Target** | **t-SNE**: Minimizing Kullback-Leibler (KL) divergence between high-dimensional Gaussian joint probabilities $P$ and low-dimensional Student-t probabilities $Q$: $\min \text{KL}(P \parallel Q) = \sum_{i \ne j} p_{ij} \log \frac{p_{ij}}{q_{ij}}$. **UMAP**: Minimizing Fuzzy Set Cross-Entropy between high-dimensional and low-dimensional fuzzy simplicial sets: $\min \sum p_{ij} \log \frac{p_{ij}}{q_{ij}} + (1 - p_{ij}) \log \frac{1 - p_{ij}}{1 - q_{ij}}$. |
| **Key Hyperparameters** | **t-SNE**: `perplexity` ($\approx 30$), `learning_rate` (`'auto'`), `early_exaggeration` ($12.0$). **UMAP**: `n_neighbors` ($15$), `min_dist` ($0.1$), `metric` (`'euclidean'`, `'cosine'`). |
| **Inference Mechanism** | t-SNE is **strictly transductive** (cannot transform new test instances without retraining). UMAP provides a **parametric / out-of-sample projection formula**. |
| **Primary Failure Modes** | Over-interpreting inter-cluster distances (global distance distortion); cluster size distortion (t-SNE expands dense clusters and compresses sparse ones); extreme sensitivity to perplexity; high computational cost on large $n$. |

---

## 2. Architecture & Intuition

Linear methods like PCA fail to visualize non-linear structures because they force high-dimensional data onto a flat hyperplane. Manifold learning algorithms unroll complex, non-linear Riemannian manifolds (such as Swiss rolls, spheres, and intertwined clusters) into human-interpretable 2D or 3D visual coordinates.

```
       High-Dimensional Space (R^d)                     Low-Dimensional Embedding (R^2)
        P_ij = Gaussian Neighborhood Probability          Q_ij = Student-t Probability (Heavy-Tailed)
             .---.                                            .---.
            (  *  )  Cluster A                               (  *  )  Cluster A
             '---'       .                                    '---'
                        .  Inter-manifold path                          (Large White Space Separation)
             .---.     .                                      .---.
            (  *  )  Cluster B                               (  *  )  Cluster B
             '---'                                            '---'
```

### The "Crowding Problem" & The Student-t Resolution
In a $d$-dimensional space, the volume of a sphere of radius $r$ scales exponentially: $V \propto r^d$. At high dimensions, there is vastly more volume available to accommodate moderate-distance neighbors than in 2D space ($V \propto r^2$). 

If we map high-dimensional Gaussian distances directly into a 2D Gaussian space, points located at moderate distances in $\mathbb{R}^d$ are forced to crowd together in the center of the 2D plot, crushing distinct clusters into an amorphous blob.

Van der Maaten & Hinton (2008) solved this via **t-SNE**:
- Use a **Gaussian distribution** in high-dimensional space (modeling local neighborhoods).
- Use a **heavy-tailed Student-t distribution (1 degree of freedom / Cauchy)** in low-dimensional space:
  $$q_{ij} \propto \frac{1}{1 + \|\mathbf{y}_i - \mathbf{y}_j\|^2}$$
Because the Student-t distribution has much heavier polynomial tails ($\frac{1}{d^2}$) than a Gaussian ($\exp(-d^2)$), moderate distances in $\mathbb{R}^d$ map to **substantially larger distances** in 2D, pushing distinct clusters far apart and completely resolving the crowding problem.

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $\mathbf{x}_i \in \mathbb{R}^d$ | Vector | High-dimensional input feature observation |
| $\mathbf{y}_i \in \mathbb{R}^2$ | Vector | Low-dimensional output visual coordinate |
| $p_{j \mid i}$ | $[0, 1]$ | Conditional probability that $\mathbf{x}_i$ picks $\mathbf{x}_j$ as its neighbor under a Gaussian |
| $\sigma_i$ | $\mathbb{R}_{> 0}$ | Bandwidth of the Gaussian centered at sample $\mathbf{x}_i$ |
| $\text{Perp}(P_i)$ | $\mathbb{R}_{> 0}$ | User-defined perplexity: $\text{Perp}(P_i) = 2^{H(P_i)}$ where $H(P_i)$ is Shannon entropy |
| $p_{ij}$ | $[0, 1]$ | Symmetric high-dimensional joint probability: $p_{ij} = \frac{p_{j \mid i} + p_{i \mid j}}{2n}$ |
| $q_{ij}$ | $[0, 1]$ | Low-dimensional Cauchy / Student-t joint probability |
| $\text{KL}(P \parallel Q)$| $\mathbb{R}_{\ge 0}$ | Kullback-Leibler divergence objective function |

### 1. The t-SNE Mathematical Formulation

#### Step A: High-Dimensional Affinities
The conditional probability that point $\mathbf{x}_i$ picks $\mathbf{x}_j$ as its neighbor is:

$$p_{j \mid i} = \frac{\exp(-\|\mathbf{x}_i - \mathbf{x}_j\|^2 / 2\sigma_i^2)}{\sum_{k \ne i} \exp(-\|\mathbf{x}_i - \mathbf{x}_k\|^2 / 2\sigma_i^2)}, \quad p_{i \mid i} = 0$$

The bandwidth $\sigma_i$ is determined via binary search such that the Shannon entropy of $P_i$ matches the user-specified **Perplexity**:

$$\text{Perp}(P_i) = 2^{H(P_i)} = 2^{-\sum_{j} p_{j \mid i} \log_2 p_{j \mid i}}$$

Perplexity functions as a continuous smooth proxy for the effective number of nearest neighbors.
To ensure robustness to outliers, symmetrize the joint probabilities:
$$p_{ij} = \frac{p_{j \mid i} + p_{i \mid j}}{2n}$$

#### Step B: Low-Dimensional Student-t Affinities
In the 2D visual embedding space $\mathbf{y}_i \in \mathbb{R}^2$:

$$q_{ij} = \frac{(1 + \|\mathbf{y}_i - \mathbf{y}_j\|^2)^{-1}}{\sum_{k} \sum_{l \ne k} (1 + \|\mathbf{y}_k - \mathbf{y}_l\|^2)^{-1}}, \quad q_{ii} = 0$$

#### Step C: The KL Divergence Gradient
The objective minimizes the asymmetric Kullback-Leibler divergence:

$$\mathcal{L}_{\text{t-SNE}} = \text{KL}(P \parallel Q) = \sum_{i=1}^n \sum_{j \ne i} p_{ij} \log\left(\frac{p_{ij}}{q_{ij}}\right)$$

Taking the analytical gradient with respect to coordinate $\mathbf{y}_i$:

$$\frac{\partial \mathcal{L}}{\partial \mathbf{y}_i} = 4 \sum_{j=1}^n (p_{ij} - q_{ij}) q_{ij} Z (\mathbf{y}_i - \mathbf{y}_j) = 4 \sum_{j=1}^n (p_{ij} - q_{ij})(1 + \|\mathbf{y}_i - \mathbf{y}_j\|^2)^{-1} (\mathbf{y}_i - \mathbf{y}_j)$$

#### Physical Spring Analogy:
The gradient acts as a system of physical springs between all pairs $(i, j)$:
- **Attractive Force**: $p_{ij} (1 + \|\mathbf{y}_i - \mathbf{y}_j\|^2)^{-1} (\mathbf{y}_i - \mathbf{y}_j)$ pulls nearby high-dimensional points together.
- **Repulsive Force**: $q_{ij} (1 + \|\mathbf{y}_i - \mathbf{y}_j\|^2)^{-1} (\mathbf{y}_i - \mathbf{y}_j)$ pushes all points apart in 2D space.

---

### 2. The UMAP Mathematical Formulation (McInnes et al., 2018)
UMAP builds upon Riemannian geometry and algebraic topology (Fuzzy Simplicial Sets):

1. **High-Dimensional Fuzzy Simplicial Set**:
   $$p_{i \mid j} = \exp\left( -\frac{\max(0, d(\mathbf{x}_i, \mathbf{x}_j) - \rho_i)}{\sigma_i} \right)$$
   Where $\rho_i$ is the distance to the single nearest neighbor (guaranteeing local graph connectivity), and $\sigma_i$ is solved such that $\sum_j p_{i \mid j} = \log_2(\text{n\_neighbors})$.
   Symmetrized via algebraic t-conorm:
   $$p_{ij} = p_{i \mid j} + p_{j \mid i} - p_{i \mid j} p_{j \mid i}$$
2. **Low-Dimensional Curve**:
   $$q_{ij} = \left( 1 + a \|\mathbf{y}_i - \mathbf{y}_j\|^{2b} \right)^{-1}$$
   Where $a$ and $b$ are fitted to approximate the step-function controlled by `min_dist`.
3. **Fuzzy Set Cross-Entropy Loss**:
   $$\mathcal{L}_{\text{UMAP}} = \sum_{i \ne j} \left[ p_{ij} \log\left(\frac{p_{ij}}{q_{ij}}\right) + (1 - p_{ij}) \log\left(\frac{1 - p_{ij}}{1 - q_{ij}}\right) \right]$$

#### Why UMAP Preserves More Global Structure Than t-SNE:
The second term $(1 - p_{ij}) \log \frac{1 - p_{ij}}{1 - q_{ij}}$ applies a strong **repulsive penalty** when $p_{ij} \approx 0$ but $q_{ij} > 0$. In t-SNE, when $p_{ij} \approx 0$, the gradient $p_{ij} \log \frac{p_{ij}}{q_{ij}} \to 0$, providing zero incentive to organize distant clusters relative to each other. UMAP's cross-entropy enforces both local attraction AND global repulsion.

---

## 4. Computational Complexity

| Algorithm | Algorithm Variant | Time Complexity | Memory Complexity |
| :--- | :--- | :--- | :--- |
| **Exact t-SNE** | Brute-force pair evaluations | $O(n^2 \cdot d + \text{iter} \cdot n^2)$ | $O(n^2)$ |
| **Barnes-Hut t-SNE** | Quadtree / Octree spatial approximation | $O(n \log n \cdot d + \text{iter} \cdot n \log n)$ | $O(n \cdot \text{perplexity})$ |
| **FIt-SNE (Fast Fourier)**| 1D/2D Fast Fourier Transform interpolation | $O(n \cdot d + \text{iter} \cdot n)$ (Linear in $n$) | $O(n)$ |
| **UMAP** | Nearest Neighbor Descent (NN-Descent) + SGD | $O(d \cdot n^{1.14} + \text{iter} \cdot n \cdot k)$ | $O(n \cdot k)$ sparse graph |

---

## 5. Inductive Bias & Learning Guarantees

1. **Local Neighborhood Priority**: Both algorithms strictly prioritize preserving local 1st- and 2nd-order neighbor affinities over long-range Euclidean distances.
2. **Uniform Manifold Assumption (UMAP)**: Assumes data lies on a Riemannian manifold with local metric tensor that makes the manifold locally connected and uniformly distributed.
3. **Non-Metric Invariance**: Cluster sizes, shapes, and inter-cluster distances in t-SNE plots **do not reflect real-world distances**. An expanded cluster in 2D may simply be a dense cluster that t-SNE dilated to equalize entropy.

---

## 6. Scratch NumPy Implementation

```python
"""
Scratch NumPy implementation of t-Distributed Stochastic Neighbor Embedding (t-SNE).
Implements binary search for Gaussian sigmas matching target Perplexity,
Student-t low-dimensional affinities, and vectorized gradient descent with momentum.
"""

from __future__ import annotations
import numpy as np


class ScratchTSNE:
    """Educational reference implementation of t-SNE in pure NumPy."""

    def __init__(
        self,
        n_components: int = 2,
        perplexity: float = 30.0,
        n_iter: int = 300,
        lr: float = 200.0,
        random_state: int | None = 42,
    ) -> None:
        self.n_components = n_components
        self.perplexity = perplexity
        self.n_iter = n_iter
        self.lr = lr
        self.random_state = random_state
        self.embedding_: np.ndarray | None = None

    def _binary_search_sigmas(self, dist_sq: np.ndarray, tol: float = 1e-5, max_iter: int = 50) -> np.ndarray:
        """Find Gaussian bandwidth sigma_i for each row such that entropy matches log2(perplexity)."""
        n_samples = len(dist_sq)
        target_entropy = np.log2(self.perplexity)
        P = np.zeros((n_samples, n_samples), dtype=np.float64)

        for i in range(n_samples):
            beta_min = -np.inf
            beta_max = np.inf
            beta = 1.0  # beta = 1 / (2 * sigma^2)

            for _ in range(max_iter):
                # Compute conditional probabilities excluding diagonal
                dists = dist_sq[i].copy()
                dists[i] = np.inf
                exp_dists = np.exp(-dists * beta)
                sum_exp = np.sum(exp_dists)
                if sum_exp < 1e-12:
                    sum_exp = 1e-12

                p_row = exp_dists / sum_exp
                # Shannon entropy: -sum(p * log2(p))
                p_safe = p_row[p_row > 1e-12]
                entropy = -np.sum(p_safe * np.log2(p_safe))

                diff = entropy - target_entropy
                if abs(diff) < tol:
                    break

                if diff > 0:  # Entropy too high -> increase precision beta
                    beta_min = beta
                    beta = beta * 2.0 if beta_max == np.inf else (beta + beta_max) / 2.0
                else:  # Entropy too low -> decrease precision beta
                    beta_max = beta
                    beta = beta / 2.0 if beta_min == -np.inf else (beta + beta_min) / 2.0

            dists[i] = 0.0
            p_final = np.exp(-dists * beta)
            p_final[i] = 0.0
            P[i] = p_final / np.sum(p_final)

        return P

    def fit_transform(self, X: np.ndarray) -> np.ndarray:
        rng = np.random.default_rng(self.random_state)
        X = np.asarray(X, dtype=np.float64)
        n_samples = len(X)

        # 1. Compute High-Dimensional Squared Euclidean Distances
        diff = X[:, None, :] - X[None, :, :]
        dist_sq = np.sum(diff ** 2, axis=2)

        # 2. Compute Conditional P and Symmetrize Joint Probabilities
        P_cond = self._binary_search_sigmas(dist_sq)
        P = (P_cond + P_cond.T) / (2.0 * n_samples)
        # Early Exaggeration: multiply P by 4.0 in early rounds
        P = np.maximum(P, 1e-12)

        # 3. Initialize 2D coordinates Y randomly from small Gaussian
        Y = rng.normal(0.0, 1e-4, size=(n_samples, self.n_components))
        Y_prev = Y.copy()

        # 4. Gradient Descent Optimization Loop
        for iteration in range(self.n_iter):
            # Compute Low-Dimensional Student-t Affinities: q_ij = (1 + ||y_i - y_j||^2)^{-1} / sum
            y_diff = Y[:, None, :] - Y[None, :, :]
            y_dist_sq = np.sum(y_diff ** 2, axis=2)
            inv_dist = 1.0 / (1.0 + y_dist_sq)
            np.fill_diagonal(inv_dist, 0.0)

            Q_num = inv_dist
            Q = Q_num / np.sum(Q_num)
            Q = np.maximum(Q, 1e-12)

            # Compute Gradients: dL/dy_i = 4 * sum_j (p_ij - q_ij) * (1 + ||y_i - y_j||^2)^{-1} * (y_i - y_j)
            PQ_diff = P - Q
            # shape (n, n, 1) * (n, n, 1) * (n, n, 2)
            mult = (PQ_diff * inv_dist)[:, :, None]
            grad = 4.0 * np.sum(mult * y_diff, axis=1)

            # Gradient Step with Momentum
            momentum = 0.8 if iteration > 50 else 0.5
            Y_step = Y - Y_prev
            Y_new = Y - self.lr * grad + momentum * Y_step

            # Zero-center coordinates to prevent drift
            Y_new -= np.mean(Y_new, axis=0)

            Y_prev = Y.copy()
            Y = Y_new

        self.embedding_ = Y
        return Y
```

---

## 7. Production Scikit-Learn & UMAP 9-Stage Pipeline

```python
"""
Production Non-Linear Manifold Pipeline comparing Barnes-Hut t-SNE with UMAP.
Demonstrates Trustworthiness metrics and UMAP out-of-sample projection.
"""

from __future__ import annotations
import logging
from typing import Dict, Tuple
import numpy as np
from sklearn.datasets import load_digits
from sklearn.preprocessing import StandardScaler
from sklearn.manifold import TSNE, trustworthiness

# Import UMAP if available
try:
    import umap
except ImportError:
    umap = None

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("Manifold_Pipeline")


def run_manifold_comparison_suite() -> Dict[str, float]:
    logger.info("Stage 1: Ingesting 64-dimensional Digits dataset (n=1797, d=64).")
    data = load_digits()
    X, y = data.data, data.target

    X_scaled = StandardScaler().fit_transform(X)

    # Stage 2: Barnes-Hut t-SNE (Scikit-Learn Standard)
    logger.info("Stage 2: Running Barnes-Hut t-SNE (perplexity=30).")
    tsne = TSNE(
        n_components=2,
        perplexity=30.0,
        learning_rate="auto",
        init="pca",          # Initializing with PCA preserves more global topology
        random_state=42,
        n_jobs=-1
    )
    X_tsne = tsne.fit_transform(X_scaled)
    tsne_trust = float(trustworthiness(X_scaled, X_tsne, n_neighbors=15))
    logger.info(f"t-SNE Embedding Generated | Neighborhood Trustworthiness: {tsne_trust:.4f}")

    metrics = {"tsne_trustworthiness": tsne_trust}

    # Stage 3: UMAP (if installed)
    if umap is not None:
        logger.info("Stage 3: Running UMAP (n_neighbors=15, min_dist=0.1).")
        reducer = umap.UMAP(
            n_components=2,
            n_neighbors=15,
            min_dist=0.1,
            metric="euclidean",
            random_state=42
        )
        X_umap = reducer.fit_transform(X_scaled)
        umap_trust = float(trustworthiness(X_scaled, X_umap, n_neighbors=15))
        logger.info(f"UMAP Embedding Generated | Neighborhood Trustworthiness: {umap_trust:.4f}")

        # Stage 4: Test Out-of-Sample Transform (UMAP Native Feature)
        test_samples = X_scaled[:5]
        test_proj = reducer.transform(test_samples)
        assert test_proj.shape == (5, 2), "UMAP transform failed."
        logger.info("Stage 4: Successfully projected out-of-sample query vectors using UMAP.")

        metrics["umap_trustworthiness"] = umap_trust

    return metrics


if __name__ == "__main__":
    results = run_manifold_comparison_suite()
    assert results["tsne_trustworthiness"] > 0.90, "t-SNE failed baseline manifold fidelity SLA."
```

---

## 8. Model Persistence & Serialization

```python
"""
Serializing UMAP models (UMAP supports native joblib persistence and out-of-sample transform).
"""

from pathlib import Path
import joblib


def save_umap_model(model: Any, path: Path) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    joblib.dump(model, path, compress=("gzip", 3))
```

> [!WARNING]
> Standard Scikit-Learn `TSNE` instances **cannot be serialized for inference** because t-SNE has no `.transform()` method. Scoring new samples requires using UMAP or training a neural parametric t-SNE model.

---

## 9. Hyperparameter Tuning Guide

| Parameter | Standard Value | Range | Impact & Diagnostics |
| :--- | :--- | :--- | :--- |
| `perplexity` (t-SNE) | `30.0` | $[5.0, 50.0]$ | Controls balance between local and global structure. Low perplexity ($5$) creates tight, fragmented micro-clusters; high perplexity ($50$) merges clusters into a global blob. |
| `init` (t-SNE) | `'pca'` | `['pca', 'random']` | Initializing with PCA preserves significantly more global cluster layout and speeds up convergence compared to random initialization. |
| `n_neighbors` (UMAP) | `15` | $[5, 100]$ | UMAP's equivalent to perplexity. Low values preserve fine manifold detail; high values capture broad global topology. |
| `min_dist` (UMAP) | `0.1` | $[0.001, 0.8]$ | Minimum Euclidean distance between points in 2D space. Small values ($0.01$) pack clusters into tight clumps; large values ($0.5$) spread points out. |

---

## 10. Residuals, Diagnostics & Failure Modes

### How to Read t-SNE & UMAP Plots Safely (Wattenberg et al., 2016)
1. **Cluster Sizes Mean Nothing**:
   t-SNE adapts its local bandwidth $\sigma_i$ to equalize point entropy. A dense cluster of 100 points will expand to the same visual radius as a sparse cluster of 100 points. Do not conclude that two visual clusters have equal variance.
2. **Distances Between Distant Clusters Mean Nothing in t-SNE**:
   Once points are far enough apart that high-dimensional $p_{ij} \approx 0$, the gradient drops to zero. Clusters 1 and 2 being separated by $10\text{cm}$ while Clusters 1 and 3 are separated by $15\text{cm}$ does **not** imply Cluster 1 is closer to 2 than 3.
3. **Random Noise Can Form Visual Clusters**:
   Running t-SNE on pure Gaussian random noise with perplexity $\le 5$ will generate visually compelling false clusters due to random nearest-neighbor fluctuations!

---

## 11. Prediction Uncertainty & Calibration

### Trustworthiness & Continuity Metrics (Venna & Kaski, 2006)
1. **Trustworthiness**: Measures whether the nearest neighbors in the 2D visual embedding were genuinely true neighbors in the high-dimensional space:
   $$T(k) = 1 - \frac{2}{n k (2n - 3k - 1)} \sum_{i=1}^n \sum_{j \in U_i^{(k)}} (r(i, j) - k)$$
   (Values $> 0.95$ certify that visual clusters are trustworthy and not projection hallucinations).
2. **Continuity**: Measures whether true high-dimensional neighbors were preserved in 2D.

---

## 12. Comprehensive Failure Modes & Edge Cases Table

| Failure Mode | Root Cause | Observable Symptom | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **Hallucinated Clusters on Uniform Noise** | Perplexity set too low ($< 5$) on noise data. | Scatter plot shows dozens of tiny tight clusters where zero structure exists. | Check Hopkins statistic before running; verify clusters with cross-validation. |
| **Crushed Uninformative Blob** | Perplexity set too high relative to sample size ($> n/3$). | All clusters merge into a uniform round ball; no structure visible. | Reduce `perplexity` to range $[15, 40]$. |
| **Shattered Manifold** | `min_dist` in UMAP set to `0.0`. | Points collapse into infinitely dense zero-dimensional dots. | Set `min_dist` between $0.1$ and $0.25$. |
| **Out-Of-Sample Inference Impossibility** | t-SNE has no parametric mapping function $f(\mathbf{x})$. | Cannot score real-time production query vectors without retraining on all data. | Switch to **UMAP** (supports `.transform()`) or train a **Parametric Autoencoder**. |
| **Extreme Outlier Distortion** | High-dimensional extreme outlier with huge distance. | Entire dataset is crushed into a $1\text{mm}$ dot while outlier sits at the edge. | Standardize with `RobustScaler` and clip extreme feature outliers before embedding. |
| **Non-Deterministic Layout Drift** | Stochastic initialization without random seed. | Plot layout rotates 180 degrees or clusters swap quadrants on every run. | Fix `random_state` and initialize with `init='pca'`. |

---

## 13. Comparative Trade-off Table

| Dimension | t-SNE (Barnes-Hut / FIt-SNE) | UMAP | PCA |
| :--- | :--- | :--- | :--- |
| **Linearity** | Non-Linear | Non-Linear | **Strictly Linear** |
| **Preserves Global Structure?** | Very Poorly (Local only) | **Moderate to Strong** | **Strictly Global** |
| **Out-of-Sample Transform?** | **No** (Transductive only) | **Yes** (`.transform()`) | **Yes** ($\mathbf{X}_c \mathbf{V}_k$) |
| **Computation Speed** | Moderate ($O(n \log n)$) | **Fastest Non-Linear** ($O(n^{1.14})$) | **Instantaneous** |
| **Mathematical Foundation** | Information Theory (KL Divergence) | Category Theory / Fuzzy Topology | Linear Algebra (SVD / Covariance) |
| **Embedding Dimensions** | Strictly $\le 3$ (Visual exploration) | Flexible ($\mathbb{R}^2$ to $\mathbb{R}^{50}$ for preprocessing)| Flexible ($1 \dots d$) |

---

## 14. Production Engineering, Serving & Latency

### Serving UMAP Embeddings in Search Engines
Unlike t-SNE, UMAP can be deployed as an online dimensionality reducer:
```python
# Save UMAP transformer
import joblib
joblib.dump(umap_reducer, "umap_embedder.joblib")

# Real-time microservice endpoint (< 5ms latency)
def embed_query(query_features):
    return umap_reducer.transform(query_features)
```
The resulting 2D or 10D embeddings can be indexed in FAISS for fast nearest-neighbor retrieval.

---

## 15. Interpretability & Explainability

- **Color Overlays**: Never inspect a t-SNE or UMAP scatter plot without coloring points by external metadata (e.g., ground-truth class, customer lifetime value, churn status, sensor failure label).
- **Misconception Warning**: Never train a linear regression or linear SVM directly on t-SNE 2D coordinates; t-SNE non-linearly distorts coordinate space, violating linear assumptions.

---

## 16. Hardware, Parallelism & Scaling

- **FIt-SNE (Fast Fourier Transform t-SNE)**: Accelerates the repulsive force calculation using non-equispaced FFTs, scaling linearly ($O(n)$) to millions of single-cell RNA sequences.
- **GPU UMAP (cuML)**: NVIDIA cuML executes UMAP on GPUs, reducing processing time for $1,000,000$ points from $45\text{ minutes}$ (CPU) to **$12\text{ seconds}$**.

---

## 17. Automated Verification Harness

```python
"""
Pytest automated verification harness for t-SNE and UMAP.
Asserts correct 2D output shapes, trustworthiness bounds, and reproducible random states.
"""

import numpy as np
import pytest
from sklearn.datasets import load_iris
from sklearn.manifold import TSNE, trustworthiness


@pytest.fixture
def iris_data():
    X, y = load_iris(return_X_y=True)
    return X, y


def test_tsne_output_dimensions(iris_data):
    """t-SNE must output an array of shape (n_samples, 2)."""
    X, _ = iris_data
    tsne = TSNE(n_components=2, perplexity=15, random_state=42)
    X_emb = tsne.fit_transform(X)

    assert X_emb.shape == (150, 2), f"Expected shape (150, 2), got {X_emb.shape}"


def test_tsne_trustworthiness_quality(iris_data):
    """Trustworthiness of Iris t-SNE embedding must exceed 0.90."""
    X, _ = iris_data
    tsne = TSNE(n_components=2, perplexity=20, random_state=42)
    X_emb = tsne.fit_transform(X)

    score = trustworthiness(X, X_emb, n_neighbors=10)
    assert score >= 0.90, f"Trustworthiness score {score} fell below quality SLA."
```

---

## 18. Cross-Track Learning & Ecosystem Links

- **Linear Foundations**: Linear baseline comparison in `05-unsupervised-learning/08-pca`.
- **Clustering Pair**: Often used to visually audit DBSCAN clusters in `05-unsupervised-learning/04-dbscan-and-hdbscan`.
- **Deep Learning Embeddings**: Visualizing high-dimensional transformer embeddings from `05-ai/machine-learning/14-deep-learning-bridge`.

---

## 19. Real-World Enterprise Case Study

### Context: Single-Cell RNA Sequencing (scRNA-seq) Immune Cell Atlas
- **Domain**: Pharmaceutical Genomics / Immuno-Oncology.
- **Problem**: Visualizing gene expression profiles for $120,000$ patient T-cells across $20,000$ sequenced genes ($d = 20,000$) to identify rare drug-resistant sub-populations.
- **Why Linear PCA Fails**: Cellular differentiation is a continuous, non-linear branching process; PCA collapses developmental trajectories into a dense cloud.
- **Implementation**:
  - Filtered top 2,000 highly variable genes; pre-reduced to 50 dimensions using TruncatedSVD.
  - Deployed GPU UMAP (`n_neighbors=30, min_dist=0.3`).
- **Outcome**: Successfully resolved 14 distinct functional T-cell subtypes, isolating an uncharacterized exhausted exhaustion phenotype responsible for immunotherapy failure.

---

## 20. Technical Interview Preparation

### Q1: What is the "Crowding Problem" in dimensionality reduction, and how does t-SNE resolve it?
**Answer:** In high dimensions, the volume of a sphere scales as $r^d$. Consequently, the available volume to accommodate moderate-distance points is vastly larger than in a 2D plane ($r^2$). If we project high-dimensional distances into a 2D Gaussian space, there is insufficient area to place moderate neighbors at their proportional distances, forcing distinct clusters to crowd together into the center. t-SNE resolves this by using an **asymmetric probability distribution**: a Gaussian in the high-dimensional space and a heavy-tailed **Student-t distribution (1 DOF / Cauchy)** in the 2D space ($q_{ij} \propto \frac{1}{1 + d^2}$). Because the Student-t tail drops off as an inverse square rather than an exponential, moderate high-dimensional distances map to much larger physical separations in 2D, pushing clusters apart.

### Q2: Why does UMAP preserve more global structure than t-SNE?
**Answer:** The objective function of t-SNE is the Kullback-Leibler divergence $\sum p_{ij} \log \frac{p_{ij}}{q_{ij}}$. When two points are far apart in high-dimensional space ($p_{ij} \approx 0$), the term $p_{ij} \log \frac{p_{ij}}{q_{ij}} \to 0$, producing zero gradient. t-SNE exerts no repulsive force between distant clusters, organizing only local neighborhoods. UMAP optimizes **Fuzzy Set Cross-Entropy**: $\sum p_{ij} \log \frac{p_{ij}}{q_{ij}} + (1 - p_{ij}) \log \frac{1 - p_{ij}}{1 - q_{ij}}$. The second term exerts an active repulsive force whenever $p_{ij} \approx 0$ but $q_{ij} > 0$, forcing distant clusters to position themselves relative to all other clusters globally.

### Q3: Why can you NOT interpret the distances between distant clusters in a t-SNE plot?
**Answer:** Because t-SNE's cost function only penalizes $q_{ij}$ when $p_{ij}$ is large (nearby points). For points where $p_{ij} \approx 0$, the gradient is essentially zero. Distant clusters simply drift to whatever open space is available in the 2D layout without their relative distances reflecting high-dimensional Euclidean proximity. Furthermore, t-SNE's early exaggeration phase artificially warps global distances to untangle clusters.

### Q4: What does the `perplexity` hyperparameter in t-SNE physically represent?
**Answer:** Perplexity is defined as $\text{Perp}(P_i) = 2^{H(P_i)}$, where $H(P_i)$ is the Shannon entropy of the conditional Gaussian neighborhood distribution centered at sample $i$. It functions as a continuous, smooth measure of the **effective number of nearest neighbors** that influence the placement of each point. A perplexity of 30 means each point balances its position primarily to satisfy its ~30 closest neighbors.

### Q5: Can you use t-SNE as a feature engineering step before training a classifier?
**Answer:** Generally **no**.
1. t-SNE is strictly transductive; it has no out-of-sample mapping formula ($f(\mathbf{x}_{\text{new}})$). You cannot project new test instances in production without appending them and re-running the entire $O(n^2)$ optimization loop.
2. t-SNE non-linearly distorts Euclidean distances and densities; training a linear model on t-SNE coordinates yields unstable boundaries.
*Exception*: **UMAP** supports native `.transform()` and preserves more metric continuity, making low-dimensional UMAP embeddings (e.g., $k=10$) valid as input features.

### Q6: What is the difference between Trustworthiness and Continuity in manifold evaluation?
**Answer:**
- **Trustworthiness**: Evaluates whether points that appear close in the 2D visual embedding were genuinely close in the high-dimensional space. It penalizes **false neighbors** (hallucinated clusters).
- **Continuity**: Evaluates whether points that were close in the high-dimensional space remain close in the 2D visual embedding. It penalizes **missing neighbors** (torn manifolds).

### Q7: Why should high-dimensional data ($d > 500$) be pre-reduced with PCA before running t-SNE?
**Answer:**
1. **Computational Speed**: Calculating high-dimensional pairwise Euclidean distances across thousands of features scales as $O(n^2 \cdot d)$. Pre-reducing to 50 PCA components slashes distance matrix computation time by $90\%$.
2. **Noise Suppression**: High-dimensional datasets contain many noisy, uninformative features. Pre-filtering with PCA isolates the dominant signal variance, preventing noise from distorting local Gaussian neighborhood probabilities.

### Q8: What occurs if you run t-SNE with a perplexity of 2 on a large dataset?
**Answer:** A perplexity of 2 forces each point to only care about its single closest neighbor. The resulting 2D plot will exhibit severe **shattering**: the dataset will fragment into dozens of isolated, artificial 2-point or 3-point micro-clumps spread uniformly across the canvas, completely destroying true macro-cluster geometry.

---

## 21. Hands-On Exercises & Mini-Projects

1. **"How to Use t-SNE Effectively" Interactive Replication**: Generate 4 synthetic shapes (linked rings, two clusters with equal points but $10\times$ different densities, random uniform noise). Run t-SNE with perplexities $\in [2, 5, 30, 100]$. Observe how t-SNE equalizes cluster sizes and hallucinates structure in noise.
2. **t-SNE Initialization Benchmark (Random vs PCA)**: Run `TSNE(init='random')` versus `TSNE(init='pca')` on the Digits dataset across 10 random seeds. Compute the Trustworthiness score for both and show that PCA initialization produces higher trustworthiness and consistent global cluster orientations.
3. **UMAP Hyperparameter Grid (`n_neighbors` vs `min_dist`)**: Train a $3 \times 3$ grid of UMAP models varying $n_{\text{neighbors}} \in [5, 15, 50]$ and $\text{min\_dist} \in [0.01, 0.1, 0.5]$. Render the 9 scatter plots side-by-side to visualize how $n_{\text{neighbors}}$ controls global layout and $\text{min\_dist}$ controls cluster tightness.
4. **Out-of-Sample Query Projection via UMAP**: Train a UMAP model on $80\%$ of the Fashion-MNIST dataset. Save the model. Load 5 unseen test images and use `reducer.transform()` to plot their coordinates over the existing embedding, verifying that test items land inside their correct fashion categories.
5. **Trustworthiness Metric from Scratch**: Implement the mathematical formula for $T(k)$ in pure NumPy using `scipy.spatial.distance.pdist`. Verify that your scratch calculation exactly matches `sklearn.manifold.trustworthiness`.
6. **FIt-SNE vs Standard Barnes-Hut Benchmark**: On a dataset of $50,000$ samples, benchmark the execution time and memory usage of standard Scikit-Learn `TSNE` versus `openTSNE` / FIt-SNE, demonstrating linear scaling.
