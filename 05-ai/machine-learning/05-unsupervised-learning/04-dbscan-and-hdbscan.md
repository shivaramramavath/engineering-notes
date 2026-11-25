# Density-Based Clustering: DBSCAN & HDBSCAN

> **Track:** `05-ai/machine-learning` | **Module:** `05-unsupervised-learning` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `HDBSCAN: >=0.8.33` | `NumPy: >=1.26.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Density-Based Spatial Clustering of Applications with Noise (DBSCAN) & Hierarchical DBSCAN (HDBSCAN) |
| **Family** | Non-Parametric Density-Based Clustering / Graph Connectivity of Dense Manifolds |
| **Input Feature Types** | Continuous numerical features ($X \in \mathbb{R}^{n \times d}$) or custom metric distance matrices. Sensitive to distance metric selection. |
| **Target Output** | Discrete cluster labels $c_i \in \{0, 1, \dots, K-1\}$ where label `-1` explicitly designates **Noise / Outliers**. Continuous outlier scores and cluster membership probabilities (HDBSCAN). |
| **Optimization Target** | Maximal density-connected graph components. In HDBSCAN: Maximizing cluster persistence (Excess of Mass) across a condensed hierarchy of Mutual Reachability distances: $\max \sum (\lambda_{\text{death}} - \lambda_{\text{birth}})$. |
| **Core Primitives** | $\epsilon$-neighborhood $N_\epsilon(\mathbf{p})$, Core Points ($|N_\epsilon(\mathbf{p})| \ge \text{MinPts}$), Border Points, Noise Points, Density-Reachability, Mutual Reachability Distance. |
| **Key Hyperparameters** | **DBSCAN**: `eps` ($\epsilon$), `min_samples` ($\text{MinPts}$), `metric`. **HDBSCAN**: `min_cluster_size`, `min_samples`, `cluster_selection_epsilon`, `cluster_selection_method` (`'eom'`, `'leaf'`). |
| **Inference Mechanism** | Transductive for DBSCAN (can assign test points to nearest core point within $\epsilon$ via `approximate_predict`). Native approximate prediction in HDBSCAN. |
| **Primary Failure Modes** | DBSCAN: Inability to handle clusters with varying densities (fixed $\epsilon$ dilemma); distance concentration in high dimensions ($d > 50$); severe sensitivity to slight perturbations in $\epsilon$. |

---

## 2. Architecture & Intuition

Unlike K-Means (which assumes convex spherical balls) or Hierarchical clustering (which forces every point into a tree regardless of noise), **DBSCAN** defines clusters as continuous regions of **high sample density** separated by sparse regions of **noise**.

```
           Feature x2
               ^          . . . . . .               .  (Noise: -1)
               |        .  (Core: +)  .
               |       .  +   +   +    .  <-- High Density Cluster 1
               |        .   +   +     .
               |          . . . . . .
               |                                      +  +  +
               |              . (Noise: -1)          +  +  +  +  <-- Cluster 2
               |                                      +  +  +
               +----------------------------------------------------> Feature x1
```

### The Three Topological Point Types in DBSCAN
1. **Core Point**: Any point $\mathbf{p}$ that contains at least $\text{MinPts}$ points within its radius $\epsilon$:
   $$|N_\epsilon(\mathbf{p})| \ge \text{MinPts}$$
2. **Border Point**: A point that has fewer than $\text{MinPts}$ neighbors within $\epsilon$, but falls within the $\epsilon$-neighborhood of a Core Point.
3. **Noise Point**: Any point that is neither a Core Point nor a Border Point. Natively tagged with label `-1`.

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $\epsilon$ | $\mathbb{R}_{> 0}$ | Neighborhood search radius distance cutoff |
| $\text{MinPts}$ / $m_{\text{pts}}$ | $\mathbb{N}_{\ge 1}$ | Minimum number of samples required to form a dense core region |
| $N_\epsilon(\mathbf{p})$ | Set of points | $\epsilon$-neighborhood of point $\mathbf{p}$: $\{\mathbf{q} \in \mathcal{D} \mid d(\mathbf{p}, \mathbf{q}) \le \epsilon\}$ |
| $\text{core}_k(\mathbf{p})$ | $\mathbb{R}_{\ge 0}$ | Core distance: Distance from $\mathbf{p}$ to its $k$-th nearest neighbor ($k = \text{MinPts}$) |
| $d_{\text{mreach}-k}(\mathbf{p}, \mathbf{q})$ | $\mathbb{R}_{\ge 0}$ | Mutual reachability distance between $\mathbf{p}$ and $\mathbf{q}$ |
| $\lambda$ | $\mathbb{R}_{\ge 0}$ | Inverse density scale parameter in HDBSCAN: $\lambda = \frac{1}{\epsilon}$ |
| $S(\mathcal{C})$ | $\mathbb{R}_{\ge 0}$ | Stability of cluster $\mathcal{C}$ in the condensed tree |

### 1. Topological Density Connectivity (DBSCAN)
- **Directly Density-Reachable**: Point $\mathbf{q}$ is directly density-reachable from $\mathbf{p}$ with respect to $(\epsilon, \text{MinPts})$ if:
  1. $\mathbf{p}$ is a Core Point: $|N_\epsilon(\mathbf{p})| \ge \text{MinPts}$.
  2. $\mathbf{q} \in N_\epsilon(\mathbf{p})$.
- **Density-Reachable**: Point $\mathbf{q}$ is density-reachable from $\mathbf{p}$ if there exists a chain of points $\mathbf{p}_1, \mathbf{p}_2, \dots, \mathbf{p}_m$ with $\mathbf{p}_1 = \mathbf{p}$ and $\mathbf{p}_m = \mathbf{q}$ such that $\mathbf{p}_{i+1}$ is directly density-reachable from $\mathbf{p}_i$. (Asymmetric: border points cannot reach other points).
- **Density-Connected**: Points $\mathbf{p}$ and $\mathbf{q}$ are density-connected if there exists an intermediate point $\mathbf{o}$ such that both $\mathbf{p}$ and $\mathbf{q}$ are density-reachable from $\mathbf{o}$. (Symmetric and reflexive).

A **Density-Based Cluster** is a maximal subset of density-connected points.

### 2. The Multi-Density Dilemma & HDBSCAN (Campello et al., 2013)
DBSCAN requires a single global radius $\epsilon$. If a dataset contains both a dense cluster (points separated by $0.1$) and a sparse cluster (points separated by $1.0$):
- Setting $\epsilon = 0.1$ correctly detects the dense cluster, but marks the entire sparse cluster as noise.
- Setting $\epsilon = 1.0$ detects the sparse cluster, but merges the dense cluster with adjacent noise.

**HDBSCAN** resolves this by converting DBSCAN into a hierarchical algorithm across all continuous density levels $\epsilon \in (0, \infty)$:

#### Step 1: Transform Space via Mutual Reachability Distance
Define the **Core Distance** $\text{core}_k(\mathbf{p})$ as the Euclidean distance from $\mathbf{p}$ to its $k$-th nearest neighbor ($k = \text{min\_samples}$).
The **Mutual Reachability Distance** between points $\mathbf{p}$ and $\mathbf{q}$ is:

$$d_{\text{mreach}-k}(\mathbf{p}, \mathbf{q}) = \max\left( \text{core}_k(\mathbf{p}), \, \text{core}_k(\mathbf{q}), \, d(\mathbf{p}, \mathbf{q}) \right)$$

This metric pushes low-density points (large core distance) away from each other while preserving distances between dense points.

#### Step 2: Build the Minimum Spanning Tree (MST)
Construct the complete graph with edge weights $d_{\text{mreach}-k}(\mathbf{p}, \mathbf{q})$ and find its Minimum Spanning Tree via Prim's or Kruskal's algorithm.

#### Step 3: Construct & Condense the Cluster Tree
Sort MST edges by distance ascending (corresponding to $\lambda = \frac{1}{\epsilon}$ increasing). As $\lambda$ increases, components split.
**Condensation**: At each split, if a branch has fewer than `min_cluster_size` points, do not create a new cluster; mark those points as having fallen out of the parent cluster at density $\lambda_{\text{drop}}$. If both branches have $\ge \text{min\_cluster_size}$, declare a true cluster split.

#### Step 4: Extract Optimal Clusters via Excess of Mass (EOM)
For each candidate cluster $\mathcal{C}_i$ in the condensed tree, compute its **Stability**:

$$S(\mathcal{C}_i) = \sum_{\mathbf{p} \in \mathcal{C}_i} \left( \lambda_{\text{drop}}(\mathbf{p}) - \lambda_{\text{birth}}(\mathcal{C}_i) \right)$$

Using dynamic programming from leaves to root: if the sum of stabilities of child clusters exceeds the parent's stability ($\sum S(\text{children}) > S(\text{parent})$), keep the children; otherwise, select the parent and unselect its descendants.

---

## 4. Computational Complexity

| Algorithm | Indexing Data Structure | Time Complexity | Space Complexity |
| :--- | :--- | :--- | :--- |
| **DBSCAN (Naive Brute-Force)** | None (Flat distance scanning) | $O(n^2 \cdot d)$ | $O(n)$ |
| **DBSCAN (Spatial Tree)** | KD-Tree / Ball-Tree ($d < 20$) | $O(n \log n \cdot d)$ | $O(n \cdot d)$ |
| **DBSCAN (Worst-Case / High-d)**| Dimensionality collapse ($d > 50$) | $O(n^2 \cdot d)$ | $O(n \cdot d)$ |
| **HDBSCAN (Dual Tree Boruvka)** | Fast KD-Tree MST Construction | $O(d \cdot n \log n)$ | $O(n \cdot d)$ |
| **Inference (Single Query Sample)**| Nearest Core Point search | $O(\log n \cdot d)$ | $O(1)$ |

---

## 5. Inductive Bias & Learning Guarantees

1. **Topological Invariance to Geometric Shape**: Density-based clustering makes zero assumptions about cluster convexity. It correctly isolates concentric rings, intertwined spirals, crescents, and complex manifold shapes.
2. **Noise Immunity**: Points located in low-density transition corridors are excluded from cluster assignment (`label = -1`), preventing boundary distortion of cohesive clusters.
3. **Density Stability (HDBSCAN)**: HDBSCAN guarantees that clusters persist over a wide dynamic range of density scales, providing robustness against arbitrary threshold tuning.

---

## 6. Scratch NumPy Implementation

```python
"""
Scratch NumPy implementation of DBSCAN Clustering from first principles.
Implements region queries, core/border/noise classification, and cluster expansion.
"""

from __future__ import annotations
import numpy as np
from typing import List


class ScratchDBSCAN:
    """Production reference implementation of DBSCAN written in pure NumPy."""

    def __init__(self, eps: float = 0.5, min_samples: int = 5) -> None:
        self.eps = float(eps)
        self.min_samples = int(min_samples)
        self.labels_: np.ndarray | None = None
        self.core_sample_indices_: np.ndarray | None = None

    def _region_query(self, X: np.ndarray, point_idx: int) -> List[int]:
        """Find all sample indices within epsilon radius of X[point_idx]."""
        # Vectorized Euclidean distance from point to all samples
        diff = X - X[point_idx]
        dist = np.sqrt(np.sum(diff ** 2, axis=1))
        return list(np.where(dist <= self.eps)[0])

    def fit(self, X: np.ndarray) -> ScratchDBSCAN:
        X = np.asarray(X, dtype=np.float64)
        n_samples = len(X)

        # Labels: -1 = unclassified / noise, >= 0 = cluster ID
        labels = np.full(n_samples, -1, dtype=np.int64)
        visited = np.zeros(n_samples, dtype=bool)
        core_indices = []
        cluster_id = 0

        for i in range(n_samples):
            if visited[i]:
                continue
            visited[i] = True

            neighbors = self._region_query(X, i)

            # If fewer than min_samples, mark as noise (may become border later)
            if len(neighbors) < self.min_samples:
                labels[i] = -1
            else:
                # Point i is a Core Point! Start a new cluster
                core_indices.append(i)
                labels[i] = cluster_id

                # Expand cluster using breadth-first queue
                queue = list(neighbors)
                # Remove i itself from search queue
                queue = [q for q in queue if q != i]

                head = 0
                while head < len(queue):
                    neighbor_idx = queue[head]
                    head += 1

                    if not visited[neighbor_idx]:
                        visited[neighbor_idx] = True
                        n_neighbors = self._region_query(X, neighbor_idx)
                        if len(n_neighbors) >= self.min_samples:
                            core_indices.append(neighbor_idx)
                            # Add new neighbors to expansion queue
                            for n_idx in n_neighbors:
                                if n_idx not in queue:
                                    queue.append(n_idx)

                    # If not yet assigned to any cluster, assign to current cluster
                    if labels[neighbor_idx] == -1:
                        labels[neighbor_idx] = cluster_id

                cluster_id += 1

        self.labels_ = labels
        self.core_sample_indices_ = np.array(list(set(core_indices)), dtype=np.int64)
        return self

    def fit_predict(self, X: np.ndarray) -> np.ndarray:
        self.fit(X)
        return self.labels_
```

---

## 7. Production Scikit-Learn & HDBSCAN 9-Stage Pipeline

```python
"""
Production Density-Based Clustering pipeline comparing DBSCAN with HDBSCAN.
Implements k-distance graph elbow estimation, DBCV validation, and noise handling.
"""

from __future__ import annotations
import logging
from typing import Dict, Tuple
import numpy as np
from sklearn.datasets import make_moons, make_blobs
from sklearn.preprocessing import StandardScaler
from sklearn.cluster import DBSCAN, HDBSCAN
from sklearn.neighbors import NearestNeighbors
from sklearn.metrics import silhouette_score

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("Density_Pipeline")


def estimate_optimal_eps(X_scaled: np.ndarray, k: int = 5) -> float:
    """Compute optimal epsilon using k-nearest neighbor distance knee/elbow heuristic."""
    nbrs = NearestNeighbors(n_neighbors=k).fit(X_scaled)
    distances, _ = nbrs.kneighbors(X_scaled)
    k_distances = np.sort(distances[:, k - 1])
    # The 90th percentile of k-distance curve serves as an automated knee estimate
    return float(np.percentile(k_distances, 90))


def run_density_clustering_pipeline() -> Tuple[HDBSCAN, Dict[str, float]]:
    logger.info("Stage 1: Synthesizing non-convex interlocking moons with Gaussian noise.")
    X_moons, _ = make_moons(n_samples=1500, noise=0.08, random_state=42)
    X_noise = np.random.default_rng(42).uniform(low=-2.0, high=3.0, size=(100, 2))
    X = np.vstack([X_moons, X_noise])

    # Stage 2: Standardization
    X_scaled = StandardScaler().fit_transform(X)

    # Stage 3: DBSCAN with Estimated Epsilon
    min_samples = 6
    estimated_eps = estimate_optimal_eps(X_scaled, k=min_samples)
    logger.info(f"Stage 3: Estimated DBSCAN eps = {estimated_eps:.4f} with min_samples = {min_samples}")

    dbscan = DBSCAN(eps=estimated_eps, min_samples=min_samples)
    db_labels = dbscan.fit_predict(X_scaled)
    db_n_clusters = len(set(db_labels)) - (1 if -1 in db_labels else 0)
    db_noise_pct = float(np.mean(db_labels == -1) * 100.0)
    logger.info(f"DBSCAN -> Clusters Found: {db_n_clusters} | Noise: {db_noise_pct:.1f}%")

    # Stage 4: Production HDBSCAN (Native in Scikit-Learn >= 1.3)
    logger.info("Stage 4: Fitting HDBSCAN with Excess of Mass extraction.")
    hdb = HDBSCAN(
        min_cluster_size=30,
        min_samples=10,
        cluster_selection_method="eom",
        store_centers="centroid"
    )
    hdb_labels = hdb.fit_predict(X_scaled)
    hdb_n_clusters = len(set(hdb_labels)) - (1 if -1 in hdb_labels else 0)
    hdb_noise_pct = float(np.mean(hdb_labels == -1) * 100.0)
    logger.info(f"HDBSCAN -> Clusters Found: {hdb_n_clusters} | Noise: {hdb_noise_pct:.1f}%")

    # Stage 5: Evaluation on Non-Noise Core Points
    non_noise_mask = hdb_labels != -1
    hdb_sil = float(silhouette_score(X_scaled[non_noise_mask], hdb_labels[non_noise_mask]))
    logger.info(f"HDBSCAN (Non-Noise Samples) Silhouette Score: {hdb_sil:.4f}")

    metrics = {
        "dbscan_clusters": float(db_n_clusters),
        "dbscan_noise_pct": db_noise_pct,
        "hdbscan_clusters": float(hdb_n_clusters),
        "hdbscan_noise_pct": hdb_noise_pct,
        "hdbscan_core_silhouette": hdb_sil,
    }
    return hdb, metrics


if __name__ == "__main__":
    model, perf = run_density_clustering_pipeline()
    assert perf["hdbscan_clusters"] == 2, f"HDBSCAN failed to discover the 2 primary moon manifolds: {perf['hdbscan_clusters']}"
```

---

## 8. Model Persistence & Serialization

```python
"""
Serializing HDBSCAN models with outlier scoring tables.
"""

from pathlib import Path
import joblib
from sklearn.cluster import HDBSCAN


def persist_density_model(model: HDBSCAN, path: Path) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    joblib.dump(model, path, compress=("gzip", 3))
```

---

## 9. Hyperparameter Tuning Guide

| Parameter | Recommended Range | Selection Protocol & Impact |
| :--- | :--- | :--- |
| `eps` (DBSCAN) | $[0.1, 2.0]$ | Plot sorted $k$-nearest neighbor distance curve with $k = \text{min\_samples}$. The optimal $\epsilon$ lies at the knee/elbow where distances shoot upward. |
| `min_samples` | $[2 \cdot d, 5 \cdot d]$ | Rule of thumb: set $\text{min\_samples} \ge 2 \cdot d$. For noisy datasets, increase to $10$–$25$ to prevent noise bridges. |
| `min_cluster_size` (HDBSCAN)| $[10, 200]$ | The smallest group of samples you are willing to consider a distinct cluster. Primary control knob for cluster granularity. |
| `cluster_selection_method` | `'eom'`, `'leaf'` | `'eom'` (Excess of Mass) extracts persistent global clusters of varying shapes. `'leaf'` extracts smaller, tighter sub-clusters from the fine branches of the tree. |

---

## 10. Residuals, Diagnostics & Failure Modes

1. **K-Distance Knee Graph**:
   Plot the distance to the $k$-th nearest neighbor sorted in ascending order. If the curve is smooth with no discernible inflection point, the dataset lacks distinct density boundaries.
2. **Noise Percentage Audit**:
   - If Noise $> 40\%$: $\epsilon$ is too small or `min_samples` is too large.
   - If Noise $< 1\%$ and 1 giant cluster forms: $\epsilon$ is too large, causing the entire dataset to merge into one component.
3. **HDBSCAN Probability Spectrum**:
   Inspect `model.probabilities_`. Samples with probabilities near $0.0$ lie on the diffuse outer margins of clusters; samples near $1.0$ reside in the dense core.

---

## 11. Prediction Uncertainty & Calibration

HDBSCAN provides native **probabilistic membership and outlier metrics**:
1. **Cluster Membership Probability** (`hdb.probabilities_`):
   $$\hat{P}(\mathbf{x}_i \in \mathcal{C}) = \frac{\lambda_{\text{drop}}(\mathbf{x}_i) - \lambda_{\text{birth}}(\mathcal{C})}{\lambda_{\text{death}}(\mathcal{C}) - \lambda_{\text{birth}}(\mathcal{C})} \in [0, 1]$$
   Measures how deeply embedded sample $i$ is within the persistence lifespan of its cluster.
2. **GLOSH (Global-Local Outlier Score from Hierarchies)** (`hdb.outlier_scores_`):
   A continuous outlier metric in $[0, 1]$. Values $> 0.85$ indicate true topological anomalies.

---

## 12. Comprehensive Failure Modes & Edge Cases Table

| Failure Mode | Root Cause | Observable Symptom | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **Varying Density Failure (DBSCAN)** | Fixed global $\epsilon$ cannot simultaneously resolve high-density and low-density clusters.| One cluster is completely marked as noise, or two distinct clusters merge together.| Upgrade from DBSCAN to **HDBSCAN**. |
| **High Dimensionality Collapse ($d > 50$)**| Euclidean distance concentration causes all pairwise distances to converge.| Every sample has either 0 neighbors or $n$ neighbors; algorithm outputs 100% noise.| Pre-reduce feature space using **PCA** or **UMAP** before running density clustering. |
| **Out-of-Memory on KD-Tree Build** | Large $n > 200,000$ with high $d > 20$. | Tree search falls back to brute force; memory explodes. | Use HDBSCAN with `approx_min_span_tree=True` or subsample data. |
| **Border Point Non-Determinism** | A border point lies within $\epsilon$ of two distinct core points belonging to different clusters.| Assignment depends strictly on the arbitrary order of input rows in the array. | Pre-sort data deterministically or use HDBSCAN (which treats border points as low-probability members). |
| **Stringy Noise Bridge** | Single noisy outlier connects two large clusters. | Two clearly distinct categories merge into one massive cluster. | Increase `min_samples` to require denser core connectivity. |
| **Uniform Random Distribution** | Data has no clusters; density is uniform throughout. | Arbitrary single cluster containing all data, or completely random fragments. | Check Hopkins Statistic ($H < 0.5$ indicates uniform data; abort clustering). |

---

## 13. Comparative Trade-off Table

| Dimension | DBSCAN | HDBSCAN | K-Means | Agglomerative Clustering |
| :--- | :--- | :--- | :--- | :--- |
| **Cluster Geometry** | Arbitrary non-convex | Arbitrary non-convex | Convex spherical balls | Depends on linkage (Ward is spherical) |
| **Handles Variable Densities?** | **No** (Single fixed $\epsilon$) | **Yes** (Mutual reachability) | No | Moderate |
| **Requires $K$ Up-Front?** | **No** | **No** | **Yes** | No (cut at height) |
| **Outlier Isolation?** | Native (`-1` noise) | Native (`-1` + GLOSH scores)| No (forces all points into balls)| No (forces all points into tree) |
| **Execution Time** | $O(n \log n)$ to $O(n^2)$ | $O(n \log n)$ | $O(n \cdot K \cdot d)$ (Fastest)| $O(n^2 \log n)$ |

---

## 14. Production Engineering, Serving & Latency

### Approximate Test-Time Inference
While standard DBSCAN has no native out-of-sample prediction function, modern Scikit-Learn HDBSCAN supports fast approximate prediction:

```python
from sklearn.cluster import HDBSCAN

model = HDBSCAN(min_cluster_size=20, prediction_data=True).fit(X_train)

# Assign new streaming data points to the nearest persistent cluster core in O(log N)
labels, probs = hdbscan.approximate_predict(model, X_test)
```

---

## 15. Interpretability & Explainability

1. **Condensed Cluster Tree**:
   In HDBSCAN, plot the condensed tree (`model.condensed_tree_.plot()`). Branch widths represent cluster sample size, and vertical lengths represent density persistence ($\lambda = 1/\epsilon$), illustrating cluster stability.
2. **Core vs Border Profiling**:
   Filter data into Core points (`dist to k-NN < eps`), Border points, and Noise points (`label == -1`). Plot separate feature distributions to identify what characteristics make an instance an outlier.

---

## 16. Hardware, Parallelism & Scaling

- **Dual-Tree Boruvka Algorithm**: Scikit-Learn's HDBSCAN leverages the Dual-Tree Boruvka algorithm over spatial KD-Trees, constructing the Minimum Spanning Tree in $O(n \log n)$ time without calculating the full $n \times n$ distance matrix.
- **Multithreading**: Distance queries utilize OpenMP multithreading across all CPU cores when `n_jobs=-1`.

---

## 17. Automated Verification Harness

```python
"""
Pytest automated verification harness for DBSCAN and HDBSCAN.
Asserts non-convex moon separation, noise label representation, and probability bounds.
"""

import numpy as np
import pytest
from sklearn.datasets import make_moons
from sklearn.cluster import DBSCAN, HDBSCAN


@pytest.fixture
def non_convex_data():
    X, _ = make_moons(n_samples=250, noise=0.05, random_state=42)
    return X


def test_dbscan_discovers_interlocking_moons(non_convex_data):
    """DBSCAN must separate two interlocking non-convex moons into exactly 2 clusters."""
    db = DBSCAN(eps=0.25, min_samples=5).fit(non_convex_data)
    unique_labels = set(db.labels_)
    # Must contain 0 and 1, plus optionally -1 (noise)
    non_noise = unique_labels - {-1}
    assert len(non_noise) == 2, f"DBSCAN failed non-convex clustering: found {len(non_noise)} clusters."


def test_hdbscan_probability_bounds(non_convex_data):
    """HDBSCAN membership probabilities must lie strictly in [0.0, 1.0]."""
    hdb = HDBSCAN(min_cluster_size=15).fit(non_convex_data)
    probs = hdb.probabilities_

    assert np.all(probs >= 0.0), "Negative probability found."
    assert np.all(probs <= 1.0 + 1e-6), "Probability exceeded 1.0."
```

---

## 18. Cross-Track Learning & Ecosystem Links

- **Mathematical Foundations**: Connects to Metric Spaces in `01-math-foundations/01-linear-algebra-for-ml`.
- **Precursor Steps**: Outlier detection in `02-data-and-preprocessing/17-outlier-detection-for-preprocessing`.
- **Alternative Density**: Probabilistic density comparison in `05-unsupervised-learning/05-gaussian-mixture-models-and-em`.

---

## 19. Real-World Enterprise Case Study

### Context: Ride-Hailing GPS Trajectory Pickup Zone Discovery & Anomaly Detection
- **Domain**: Urban Mobility Platform ($5\text{M}$ GPS pings daily).
- **Problem**: Automatically identifying high-density passenger pickup hotspots while ignoring erratic GPS noise and highway drop-offs.
- **Why K-Means Fails**: City road networks, airport terminal curbsides, and transit hubs form long, curving non-convex corridors. K-Means creates circular bubbles that place centroids inside rivers or buildings.
- **Implementation**:
  - Algorithm: `HDBSCAN(min_cluster_size=50, metric='haversine')` applied to latitude/longitude coordinates converted to radians.
  - Automatically isolated $142$ pickup hotspot zones while flagging $18.4\%$ of GPS pings as road-transit noise.
- **Outcome**: Sited designated driver staging bays with zero manual cartography, cutting passenger curbside wait times by $3.4\text{ minutes}$.

---

## 20. Technical Interview Preparation

### Q1: Why is DBSCAN preferred over K-Means for real-world geospatial and trajectory clustering?
**Answer:**
1. **Arbitrary Geometry**: Geospatial features (roads, shorelines, train tracks, pedestrian pathways) are non-convex and curved. K-Means forces clusters into convex spherical Voronoi cells, splitting long linear corridors into unnatural segments. DBSCAN follows density-connected paths regardless of shape.
2. **Native Outlier Handling**: Geospatial telemetry is noisy (GPS multipath reflections, drivers passing through on highways). K-Means forces every outlier into a cluster, pulling centroids off-target. DBSCAN natively flags low-density points as noise (`-1`).
3. **No Prior $K$ Required**: The number of pickup hotspots or crime clusters in a city is unknown up-front; DBSCAN discovers $K$ dynamically based on density thresholds.

### Q2: What is the difference between a Core Point, Border Point, and Noise Point in DBSCAN?
**Answer:**
- **Core Point**: A point $\mathbf{p}$ with at least $\text{MinPts}$ neighbors within radius $\epsilon$ ($|N_\epsilon(\mathbf{p})| \ge \text{MinPts}$). It forms the interior dense foundation of a cluster.
- **Border Point**: A point that has fewer than $\text{MinPts}$ neighbors within $\epsilon$, but falls within the $\epsilon$-neighborhood of a Core Point. It lies on the outer boundary of a cluster.
- **Noise Point**: A point that is neither a Core Point nor within $\epsilon$ of any Core Point. It is assigned label `-1`.

### Q3: What is the "Variable Density Dilemma" in standard DBSCAN, and how does HDBSCAN solve it?
**Answer:** Standard DBSCAN uses a single global radius $\epsilon$. If a dataset contains clusters with vastly differing densities, choosing a small $\epsilon$ will correctly isolate dense clusters but classify all sparse clusters as noise. Choosing a large $\epsilon$ will detect sparse clusters but merge all adjacent dense clusters and noise together. **HDBSCAN** solves this by converting the problem into a hierarchy across all possible density values $\lambda = 1/\epsilon$, computing a Minimum Spanning Tree of **Mutual Reachability distances**, condensing the tree, and extracting clusters that maximize total persistence (Excess of Mass) across density scales.

### Q4: How should an engineer systematically select `eps` and `min_samples` for DBSCAN?
**Answer:**
1. **`min_samples`**: Set $\text{min\_samples} \ge 2 \cdot d$ as a baseline rule of thumb. For noisy or massive datasets, increase to $10$–$50$ to avoid noise bridging.
2. **`eps`**: Fit a K-Nearest Neighbors model with $k = \text{min\_samples}$. Calculate the distance from every point to its $k$-th nearest neighbor and plot these distances sorted in ascending order (the **k-distance graph**). The optimal $\epsilon$ corresponds to the "knee" or "elbow" of the curve where distances transition from flat core values to steep noise values.

### Q5: What is the Mutual Reachability Distance in HDBSCAN?
**Answer:** Given core distance $\text{core}_k(\mathbf{p})$ (the distance from $\mathbf{p}$ to its $k$-th nearest neighbor), the mutual reachability distance between $\mathbf{p}$ and $\mathbf{q}$ is:
$$d_{\text{mreach}-k}(\mathbf{p}, \mathbf{q}) = \max\left( \text{core}_k(\mathbf{p}), \, \text{core}_k(\mathbf{q}), \, d(\mathbf{p}, \mathbf{q}) \right)$$
If either point is in a low-density region (large core distance), the mutual reachability distance is artificially boosted to that large core distance, effectively pushing sparse noise points away from each other and away from dense clusters while preserving true distances between dense points.

### Q6: Why is border point assignment in standard DBSCAN technically non-deterministic?
**Answer:** If a border point $\mathbf{b}$ lies within distance $\epsilon$ of two different Core Points belonging to two distinct clusters ($\mathcal{C}_1$ and $\mathcal{C}_2$), $\mathbf{b}$ will be assigned to whichever cluster happens to explore and claim it first in the array loop. The assignment depends strictly on the arbitrary indexing order of rows in the input matrix $X$. Core points and noise points are $100\%$ deterministic; only shared border points exhibit order-dependence.

### Q7: What is the computational bottleneck of DBSCAN in high dimensions ($d > 50$)?
**Answer:** In low dimensions ($d < 20$), spatial indexing trees (KD-Tree, Ball-Tree) execute $\epsilon$-region queries in $O(\log n)$ time, yielding overall training time $O(n \log n)$. In high dimensions, the **Curse of Dimensionality** causes the bounding boxes of spatial trees to overlap completely, forcing spatial trees to fall back to a linear scan of all $n$ samples. The time complexity collapses to brute-force $O(n^2 \cdot d)$, making the algorithm intractable for large high-dimensional datasets.

### Q8: What does a label of `-1` mean in HDBSCAN, and how should it be handled downstream?
**Answer:** A label of `-1` designates an unclustered **outlier / noise** instance that does not belong to any persistent cluster. In downstream business workflows:
- In customer segmentation, noise instances are routed to a generic fallback persona.
- In fraud or system monitoring, `-1` instances are flagged as high-priority investigation anomalies.
- In semi-supervised classification, noise points are excluded from pseudo-labeling.

---

## 21. Hands-On Exercises & Mini-Projects

1. **Automated K-Distance Knee Detector**: Write a Python function using `scipy.optimize` or `kneed` that computes the $k$-nearest neighbor distance curve, automatically detects the knee coordinate via maximum curvature, and passes it as `eps` to DBSCAN.
2. **Concentric Circles & Crescent Showdown**: Generate synthetic concentric circles, intertwined moons, and an anisotropic elongated blob. Run K-Means, Agglomerative (Ward), and DBSCAN side-by-side. Plot their 2D cluster partitions to visually demonstrate density vs centroid boundaries.
3. **HDBSCAN Condensed Tree Inspector**: Train an `HDBSCAN` model on a multi-density dataset. Extract `model.condensed_tree_` and plot the hierarchical tree with highlighted cluster selections, verifying how Excess of Mass selects persistent nodes over transient sub-branches.
4. **GPS Anomaly Filter for Fleet Telematics**: Load 5,000 real or simulated delivery vehicle coordinates. Use HDBSCAN with Haversine metric to cluster delivery stops and extract the top $5\%$ most extreme anomalies using `outlier_scores_`, verifying that they correspond to off-route GPS glitches.
5. **DBSCAN Border Point Non-Determinism Verification**: Create a dataset with two dense blobs connected by a single border point. Run `ScratchDBSCAN` 20 times with randomly shuffled row orders. Count how often the border point flips between Cluster 0 and Cluster 1.
6. **High-Dimensional PCA + HDBSCAN Pipeline**: Take a 100-dimensional noisy dataset. Demonstrate that running HDBSCAN directly on raw 100D data assigns $95\%$ of points to noise. Build a Scikit-Learn `Pipeline` combining `StandardScaler` + `PCA(n_components=10)` + `HDBSCAN` and show that clean clusters are recovered.
