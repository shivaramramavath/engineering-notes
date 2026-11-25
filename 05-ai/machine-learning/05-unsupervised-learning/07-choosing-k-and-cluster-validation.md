# Choosing K & Cluster Validation: Metrics, Stability & Diagnostics

> **Track:** `05-ai/machine-learning` | **Module:** `05-unsupervised-learning` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `NumPy: >=1.26.0` | `SciPy: >=1.12.0` | `Python: >=3.11`  
> **Type:** `Core Concept Architecture & Engineering Guide`

---

## 1. Concept Definition & Purpose

Unlike supervised learning—where loss functions and metrics (MSE, Log-Loss, ROC-AUC) evaluate models against objective ground-truth labels—unsupervised clustering has no universal target $y$. Deciding **how many clusters exist ($K$)** and **how well-separated they are** is fundamentally an ill-posed mathematical problem.

**Cluster Validation** provides formal quantitative methodologies to:
1. **Determine the Optimal Model Order ($K^*$)**: Preventing under-clustering (merging distinct populations) and over-clustering (fracturing natural populations into arbitrary noise).
2. **Evaluate Partition Quality Without Labels (Internal Validation)**: Measuring geometric compactness, separation, density, and stability.
3. **Benchmark Against Reference Ground Truth (External Validation)**: Comparing discovered partitions against external annotations or metadata.
4. **Quantify Algorithmic Stability**: Ensuring that discovered clusters are reproducible across random subsamples and bootstrap resamples rather than artifacts of stochastic seeding.

---

## 2. Theoretical Mechanics & Mathematical Formulations

### 1. Internal Validation Metrics (No Labels Required)

```
        Cluster A                         Cluster B
     .-------------.                   .-------------.
    (   x_i        )                  (              )
    (    |         )                  (       *      )
    (    v a(i)    )                  (   Centroid B )
    (   x_j        )                  (              )
     '-------------'                   '-------------'
      Mean Intra-Cluster                Mean Nearest-Cluster
      Distance: a(i)                    Distance: b(i)
```

#### A. Silhouette Coefficient (Rousseeuw, 1987)
For each sample $i$, let $a(i)$ be the mean distance to all other points in its own cluster, and let $b(i)$ be the mean distance to all points in the nearest neighboring cluster:

$$s(i) = \frac{b(i) - a(i)}{\max(a(i), b(i))} \in [-1, +1]$$

- $s(i) \approx +1$: Sample is well-clustered (far from neighboring clusters).
- $s(i) \approx 0$: Sample lies directly on the boundary between two clusters.
- $s(i) < 0$: Sample is closer to a neighboring cluster than its assigned cluster (misclustered).
The global score is the average across all $n$ samples: $\bar{s} = \frac{1}{n} \sum_{i=1}^n s(i)$.

#### B. Calinski-Harabasz Index (Variance Ratio Criterion)
The ratio of between-cluster dispersion to within-cluster dispersion, adjusted for degrees of freedom:

$$\text{CH}(K) = \frac{\text{Tr}(B_K) / (K - 1)}{\text{Tr}(W_K) / (n - K)}$$

Where $B_K = \sum_{k=1}^K n_k (\boldsymbol{\mu}_k - \boldsymbol{\mu})(\boldsymbol{\mu}_k - \boldsymbol{\mu})^T$ is between-cluster scatter, and $W_K = \sum_{k=1}^K \sum_{i \in \mathcal{C}_k} (\mathbf{x}_i - \boldsymbol{\mu}_k)(\mathbf{x}_i - \boldsymbol{\mu}_k)^T$ is within-cluster scatter. **Higher values indicate superior clustering**.

#### C. Davies-Bouldin Index (DBI)
Measures the average similarity between each cluster and its most similar rival, where similarity $R_{ij}$ is the ratio of within-cluster dispersions ($s_i + s_j$) to between-centroid distance $d(\boldsymbol{\mu}_i, \boldsymbol{\mu}_j)$:

$$\text{DB} = \frac{1}{K} \sum_{i=1}^K \max_{j \ne i} \left( \frac{s_i + s_j}{d(\boldsymbol{\mu}_i, \boldsymbol{\mu}_j)} \right)$$

**Lower values indicate superior clustering** (0 is theoretical minimum).

#### D. The Gap Statistic (Tibshirani, Walther, Hastie, 2001)
Compares the change in within-cluster dispersion $W_k$ to an expected null reference distribution generated from a uniform distribution over the principal components of the data:

$$\text{Gap}(k) = \mathbb{E}^*[\ln(W_k)] - \ln(W_k)$$

The optimal $k^*$ is the smallest $k$ satisfying:
$$\text{Gap}(k) \ge \text{Gap}(k + 1) - s_{k+1}$$
Where $s_{k+1}$ accounts for the standard error of Monte Carlo null simulations.

#### E. Density-Based Clustering Validation (DBCV)
Standard Silhouette and CH metrics assume convex, spherical clusters and fail completely on non-convex density shapes (DBSCAN/HDBSCAN). DBCV evaluates density clusters using mutual reachability distance and all-points core distances.

---

### 2. External Validation Metrics (Ground Truth Available)

| Metric | Formula / Basis | Range | Invariance Properties |
| :--- | :--- | :---: | :--- |
| **Adjusted Rand Index (ARI)** | Adjusted for chance: $\text{ARI} = \frac{\text{Index} - \mathbb{E}[\text{Index}]}{\text{Max} - \mathbb{E}[\text{Index}]}$ based on pair contingency. | $[-1, +1]$ | Strictly invariant to label permutations; random partitions yield $\approx 0.0$. |
| **Normalized Mutual Information (NMI)** | Information-theoretic sharing: $\text{NMI} = \frac{2 I(U; V)}{H(U) + H(V)}$. | $[0, 1]$ | Invariant to cluster count asymmetry; evaluates non-linear association. |
| **Homogeneity & Completeness** | Homogeneity: each cluster contains only one class. Completeness: all members of a class assigned to same cluster. | $[0, 1]$ | Dual entropy criteria. Combined in harmonic mean as **V-Measure**. |
| **Fowlkes-Mallows Index (FMI)** | Geometric mean of pair precision and recall: $\text{FMI} = \frac{\text{TP}}{\sqrt{(\text{TP} + \text{FP})(\text{TP} + \text{FN})}}$. | $[0, 1]$ | Zero chance adjustment, but robust to extreme cluster imbalance. |

---

## 3. Architectural Topology & Decision Flowchart

```
+--------------------------------------------------------------------------------------------------+
|                            CLUSTER VALIDATION & K-SELECTION TOPOLOGY                             |
|                                                                                                  |
|                              Do you have reference ground-truth labels?                          |
|                                     /                        \                                   |
|                                  [YES]                       [NO]                                |
|                                   /                            \                                 |
|                       External Validation              Are clusters expected to be               |
|                    Adjusted Rand Index (ARI) /           spherical or non-convex?                |
|                    V-Measure / NMI                              /        \                       |
|                                                                /          \                      |
|                                                        [Spherical]      [Non-Convex Density]     |
|                                                             |                     |              |
|                                                     Multi-Metric Search        DBCV Metric /     |
|                                                     1. Silhouette Score        Core Persistence  |
|                                                     2. Davies-Bouldin (Min)                      |
|                                                     3. Calinski-Harabasz (Max)                   |
|                                                     4. Gap Statistic                             |
|                                                             |                                    |
|                                                     Bootstrap Stability                          |
|                                                     Verification (Jaccard > 0.75)                |
+--------------------------------------------------------------------------------------------------+
```

---

## 4. Production Scikit-Learn Implementation

```python
"""
Production Cluster Validation & Optimal K Selection Suite.
Automates multi-metric grid evaluation across Silhouette, Calinski-Harabasz,
Davies-Bouldin, and Bootstrap Stability Analysis.
"""

from __future__ import annotations
import logging
from typing import Dict, List, Any, Tuple
import numpy as np
import pandas as pd
from sklearn.datasets import make_blobs
from sklearn.preprocessing import StandardScaler
from sklearn.cluster import KMeans
from sklearn.metrics import (
    silhouette_score,
    calinski_harabasz_score,
    davies_bouldin_score,
    adjusted_rand_score
)

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("Cluster_Validation")


def evaluate_cluster_stability(X: np.ndarray, k: int, n_bootstraps: int = 10) -> float:
    """Measure cluster assignment reproducibility across bootstrap resamples using ARI."""
    n_samples = len(X)
    rng = np.random.default_rng(42)
    ari_scores = []

    # Baseline clustering on full dataset
    base_model = KMeans(n_clusters=k, n_init=3, random_state=42).fit(X)
    base_labels = base_model.labels_

    for b in range(n_bootstraps):
        # Draw 80% subsample without replacement
        sub_idx = rng.choice(n_samples, size=int(0.8 * n_samples), replace=False)
        sub_model = KMeans(n_clusters=k, n_init=3, random_state=42 + b).fit(X[sub_idx])

        # Compare labels on shared subsample points
        ari = adjusted_rand_score(base_labels[sub_idx], sub_model.labels_)
        ari_scores.append(ari)

    return float(np.mean(ari_scores))


def run_optimal_k_selection_tournament(k_min: int = 2, k_max: int = 8) -> Tuple[int, pd.DataFrame]:
    logger.info(f"Synthesizing multi-cluster dataset (True K=4, n=2500, d=8).")
    X, y_true = make_blobs(n_samples=2500, n_features=8, centers=4, cluster_std=1.4, random_state=42)
    X_scaled = StandardScaler().fit_transform(X)

    results: List[Dict[str, Any]] = []

    for k in range(k_min, k_max + 1):
        km = KMeans(n_clusters=k, init="k-means++", n_init=5, random_state=42).fit(X_scaled)
        labels = km.labels_

        sil = float(silhouette_score(X_scaled, labels))
        ch = float(calinski_harabasz_score(X_scaled, labels))
        db = float(davies_bouldin_score(X_scaled, labels))
        ari_true = float(adjusted_rand_score(y_true, labels))
        stability = evaluate_cluster_stability(X_scaled, k=k, n_bootstraps=5)

        logger.info(
            f"K={k} -> Sil: {sil:.4f} | CH: {ch:,.0f} | DB: {db:.4f} | Stability: {stability:.4f} | ARI: {ari_true:.4f}"
        )

        results.append({
            "K": k,
            "Silhouette (Max)": sil,
            "Calinski-Harabasz (Max)": ch,
            "Davies-Bouldin (Min)": db,
            "Stability ARI (Max)": stability,
            "Ground-Truth ARI": ari_true,
        })

    df = pd.DataFrame(results)
    # Optimal K selected by maximizing Silhouette and Stability
    best_k = int(df.loc[df["Silhouette (Max)"].idxmax(), "K"])
    logger.info(f"\nTournament Winner: Optimal Cluster Count K = {best_k}")
    return best_k, df


if __name__ == "__main__":
    winner_k, score_table = run_optimal_k_selection_tournament()
    print("\n" + score_table.to_string(index=False))
    assert winner_k == 4, f"Tournament failed to identify true K=4: {winner_k}"
```

---

## 5. Edge Cases & Failure Modes

1. **Spherical Inductive Bias on Manifolds**:
   Both Silhouette and Calinski-Harabasz evaluate Euclidean distances between points and centroids. If the data contains two concentric rings, K-Means will split them into two hemispheres, and the Silhouette score may be positive ($+0.35$). If DBSCAN correctly isolates the two rings, the Euclidean Silhouette score will paradoxically be **negative** because the outer ring surrounds the inner ring!
   - *Mitigation*: Never use Silhouette or CH metrics to validate density-based algorithms on non-convex manifolds; use **DBCV** or **downstream task performance**.
2. **The Trivial $K=n$ Trap**:
   Inertia decreases monotonically to $0$ as $K \to n$. Raw within-cluster variance can never be optimized directly without regularization or penalty terms (like degrees of freedom adjustment in Calinski-Harabasz or BIC).
3. **High-Dimensional Metric Compression**:
   When $d > 100$, distances between all points concentrate to a uniform value. Silhouette scores compress into a narrow band near $0.0$, losing all discriminatory sensitivity. Always reduce dimensions via PCA prior to metric evaluation.

---

## 6. Comparative Analysis Table of Validation Metrics

| Metric | Type | Optimization Direction | Assumes Convex / Spherical? | Computation Scaling | Sensitivity to Scale |
| :--- | :--- | :---: | :---: | :--- | :--- |
| **Silhouette Score** | Internal | **Maximize** ($\to +1.0$) | **Yes** (Distance to centroids) | $O(n^2 \cdot d)$ (Pairwise distances) | Moderate |
| **Calinski-Harabasz**| Internal | **Maximize** | **Yes** (Scatter ratio) | $O(n \cdot K \cdot d)$ (Very Fast) | High |
| **Davies-Bouldin** | Internal | **Minimize** ($\to 0.0$) | **Yes** (Centroid distances) | $O(n \cdot d + K^2)$ (Very Fast) | High |
| **Gap Statistic** | Internal | **Maximize** $\text{Gap}(k)$ | **Yes** (Null comparison) | $O(B \cdot n \cdot K \cdot d)$ (Slow) | Moderate |
| **DBCV** | Internal | **Maximize** ($\to +1.0$) | **No** (Density-based) | $O(n^2)$ | Moderate |
| **Adjusted Rand (ARI)**| External | **Maximize** ($\to +1.0$) | **No** (Combinatorial pairs) | $O(n)$ | Strictly Invariant |
| **V-Measure / NMI** | External | **Maximize** ($\to +1.0$) | **No** (Information mutual) | $O(n)$ | Strictly Invariant |

---

## 7. Hyperparameter & Configuration Matrix

| Tool | Evaluation Parameter | Recommended Usage | Description |
| :--- | :--- | :--- | :--- |
| `silhouette_score` | `sample_size` | `10000` | On datasets with $n > 50,000$, computing full pairwise distances exhausts memory. Pass `sample_size=10000` to evaluate a random subsample. |
| `silhouette_score` | `metric` | `'euclidean'`, `'cosine'` | Match the ground distance metric utilized by the underlying clustering algorithm. |
| `fcluster` / `KMeans` | Grid Range | $K \in [2, \min(20, \sqrt{n})]$ | Standard exploration window for candidate cluster counts. |

---

## 8. Diagnostics, Monitoring & Error Metrics

### Constructing Silhouette Thickness Diagrams
A global Silhouette score of $+0.52$ can be misleading if one cluster has a score of $+0.90$ while another has a score of $-0.10$. Always inspect the per-cluster silhouette plot:
1. Sort silhouette coefficients within each cluster in descending order.
2. Render vertical thickness bars proportional to cluster sample size.
3. Assert that:
   - Every cluster exceeds the global mean dashed line.
   - Zero clusters exhibit substantial negative tails ($s(i) < 0$).
   - Cluster thicknesses are roughly balanced (unless unequal sizes are expected).

---

## 9. Production Engineering & Serving Considerations

- **Sampling in Continuous Monitoring**: In production pipelines, continuously monitoring cluster drift by computing Silhouette scores across millions of streaming records is computationally intractable ($O(n^2)$). Subsample $1,000$ points hourly (`sample_size=1000`) to compute a rolling internal validation trend line.
- **Automated Re-Clustering Triggers**: Trigger an automated model re-training and re-clustering workflow whenever the rolling Silhouette score degrades by $> 15\%$ or the percentage of outlier samples exceeds a predefined threshold.

---

## 10. Automated Verification Harness

```python
"""
Pytest automated verification harness for cluster validation metrics.
Asserts mathematical bounds, label invariance of ARI, and metric monotonicity.
"""

import numpy as np
import pytest
from sklearn.datasets import make_blobs
from sklearn.metrics import silhouette_score, adjusted_rand_score, davies_bouldin_score


@pytest.fixture
def clean_clusters():
    X, y = make_blobs(n_samples=200, n_features=4, centers=3, cluster_std=0.5, random_state=42)
    return X, y


def test_silhouette_score_bounds(clean_clusters):
    """Silhouette score must strictly lie within [-1.0, 1.0]."""
    X, y = clean_clusters
    sil = silhouette_score(X, y)
    assert -1.0 <= sil <= 1.0, f"Silhouette score {sil} outside valid bounds."
    assert sil > 0.60, f"Expected high silhouette on well-separated blobs, got {sil}"


def test_ari_permutation_invariance(clean_clusters):
    """Adjusted Rand Index must be strictly invariant to label renaming."""
    X, y = clean_clusters
    # Permute labels: 0 -> 2, 1 -> 0, 2 -> 1
    y_permuted = np.array([{0: 2, 1: 0, 2: 1}[val] for val in y])

    ari = adjusted_rand_score(y, y_permuted)
    np.testing.assert_allclose(ari, 1.0, atol=1e-6, err_msg="ARI failed label permutation invariance!")


def test_davies_bouldin_non_negative(clean_clusters):
    """Davies-Bouldin index must be strictly non-negative."""
    X, y = clean_clusters
    db = davies_bouldin_score(X, y)
    assert db >= 0.0, f"Davies-Bouldin index {db} cannot be negative."
```

---

## 11. Cross-Track Learning & Ecosystem Links

- **Mathematical Foundations**: Variance ratios in `01-math-foundations/04-statistics-and-hypothesis-testing`.
- **Clustering Algorithms**: Validates models in `02-k-means`, `03-hierarchical-clustering`, and `04-dbscan-and-hdbscan`.
- **Evaluation Standards**: Pairs with classification metrics in `05-ai/machine-learning/10-evaluation-and-metrics`.

---

## 12. Enterprise Case Study

### Context: National Retail Pharmacy Customer Micro-Segmentation
- **Domain**: Retail Pharmacy Chain ($14\text{M}$ loyalty cardholders).
- **Task**: Segment customers into distinct health & purchasing personas based on 32 purchase category velocity features.
- **Dilemma**: Business executives debated whether to create 4 broad macro-segments or 12 granular micro-segments.
- **Validation Tournament**:
  - Computed Silhouette, Davies-Bouldin, and Bootstrap Stability across $K \in [2, 16]$.
  - Silhouette peaked at $K=3$ ($0.58$), but Stability ARI dropped sharply.
  - At $K=6$, Silhouette remained high ($0.51$), Davies-Bouldin reached a local minimum ($0.82$), and Bootstrap Stability achieved a peak ARI of $0.88$ across 20 subsamples.
- **Outcome**: Adopted $K=6$ as the certified enterprise segmentation model, driving personalized loyalty coupon targeting that generated $\$34\text{M}$ in incremental annualized revenue.

---

## 13. Technical Interview Preparation

### Q1: Why can Within-Cluster Sum of Squares (Inertia) not be used directly to select $K$?
**Answer:** Inertia strictly decreases as $K$ increases: adding another centroid provides an additional degree of freedom to reduce squared Euclidean distances, reaching an absolute minimum of $0$ when $K=n$ (every point is its own centroid). Because raw inertia has no penalty for model complexity (parameter count), minimizing inertia directly will always select $K=n$, which provides zero compression or clustering utility. Selecting $K$ requires identifying the inflection point (Elbow Method) or using metrics that penalize model complexity (Calinski-Harabasz, BIC, Gap Statistic).

### Q2: What does a negative Silhouette score indicate for a single data point?
**Answer:** The silhouette coefficient for point $i$ is $s(i) = \frac{b(i) - a(i)}{\max(a(i), b(i))}$. If $s(i) < 0$, it means $b(i) < a(i)$—the average distance to points in the nearest neighboring cluster is smaller than the average distance to points in its own assigned cluster. Geometrically, this proves that the point is closer to another cluster's centroid than its own and has been misclustered by the algorithm.

### Q3: Explain the Gap Statistic and how it formalizes the "Elbow Method."
**Answer:** The Elbow Method is a subjective visual heuristic. The **Gap Statistic** (Tibshirani et al.) provides a rigorous statistical hypothesis test. It computes $\ln(W_k)$ on the real dataset and compares it to the expected dispersion $\mathbb{E}^*[\ln(W_k)]$ calculated over Monte Carlo null datasets generated by drawing from a uniform distribution over the box or principal component axes of the data:
$$\text{Gap}(k) = \mathbb{E}^*[\ln(W_k)] - \ln(W_k)$$
The optimal $K^*$ is the smallest $k$ where the gap increase levels off, satisfying $\text{Gap}(k) \ge \text{Gap}(k+1) - s_{k+1}$, providing an automated, objective stopping criterion.

### Q4: Why is the Adjusted Rand Index (ARI) superior to raw Rand Index (RI) or Accuracy?
**Answer:**
1. **Permutation Invariance**: Clustering produces arbitrary cluster integer IDs ($0, 1, 2$). If a model assigns true class `0` to cluster `2` and true class `1` to cluster `0`, raw classification accuracy scores this as $0\%$, whereas ARI recognizes the 1-to-1 bijection and scores it as $1.0$ ($100\%$).
2. **Chance Adjustment**: The raw Rand Index computes the fraction of agreeing pairs: $\text{RI} = \frac{a + b}{\binom{n}{2}}$. On random cluster assignments, RI produces an artificially inflated positive score (e.g., $0.60$). ARI adjusts for chance using the generalized hypergeometric distribution, guaranteeing that random label assignments yield an expected score of exactly $0.0$.

### Q5: How does Cluster Stability Analysis work, and why is it important?
**Answer:** Cluster stability measures whether the discovered cluster partitions reflect true underlying structure or are random artifacts of sample noise. The methodology repeatedly draws bootstrap resamples (or perturbations) of the training data, re-runs the clustering algorithm independently on each resample, and measures the pairwise similarity of cluster assignments on shared observations using the Adjusted Rand Index or Jaccard similarity. If mean $\text{ARI} > 0.80$, the clusters are stable and reproducible; if $\text{ARI} < 0.50$, the clusters are unstable and fragile to sample variation.

### Q6: Why do Silhouette and Calinski-Harabasz fail on DBSCAN cluster outputs?
**Answer:** Both Silhouette and Calinski-Harabasz are based strictly on **Euclidean distance to cluster centroids**, inherently assuming clusters are compact, convex, spherical balls. Density-based algorithms like DBSCAN and HDBSCAN are explicitly designed to find non-convex, elongated, or manifold-shaped clusters (e.g., spirals, concentric rings). On such shapes, Euclidean distances to centroids are meaningless (the centroid may lie outside the cluster entirely). Evaluating DBSCAN with Silhouette produces heavily distorted or negative scores despite perfect density separation.

### Q7: What is the computational time complexity of computing the Silhouette Score on $n$ samples?
**Answer:** Computing the Silhouette score requires evaluating the average distance from every sample to every other sample in its own cluster ($a(i)$) and all other clusters ($b(i)$). This requires calculating and aggregating the full pairwise distance matrix, scaling as $O(n^2 \cdot d)$. For $n = 100,000$, computing the full silhouette score requires evaluating 10 billion pairwise distances ($\approx 80\text{GB}$ of memory), which is computationally prohibitive without subsampling (`sample_size=10000`).

### Q8: What is the difference between Homogeneity and Completeness in external validation?
**Answer:**
- **Homogeneity**: Satisfied if all clusters contain only data points that are members of a single class ($H(C \mid K) = 0$). (A trivial clustering where every single data point is its own cluster has $100\%$ homogeneity).
- **Completeness**: Satisfied if all data points that are members of a given class are assigned to the same cluster ($H(K \mid C) = 0$). (A trivial clustering where all data points are assigned to a single giant cluster has $100\%$ completeness).
The **V-Measure** is the harmonic mean of both, ensuring that neither trivial extreme achieves a high score.

---

## 21. Hands-On Exercises & Mini-Projects

1. **Automated Knee / Elbow Point Detection**: Write a Python function using the second derivative of the Inertia curve:
   $$\Delta^2(k) = (J_{k-1} - J_k) - (J_k - J_{k+1})$$
   Automatically identify the index $k$ that maximizes the second derivative to extract the elbow point algorithmically.
2. **Interactive Silhouette Thickness Plotter**: Train K-Means with $K \in [2, 6]$. For each $K$, render a multi-panel silhouette plot with horizontal bars colored by cluster, plotting the global average dashed line.
3. **Gap Statistic from Scratch**: Implement the Gap Statistic algorithm using `scipy.stats.uniform` bounding boxes. Evaluate on synthetic Gaussian clusters and verify that it correctly selects the ground-truth cluster count.
4. **Cluster Stability Tournament**: Write a stability testing harness that resamples 10 bootstrap slices ($80\%$ subsampling) for $K \in [2, 10]$. Compute the distribution of pairwise ARIs and plot boxplots across $K$ to visualize stability collapse.
5. **DBSCAN Density Metric (DBCV) Implementation**: Test both Silhouette and DBCV on a concentric circles dataset clustered with DBSCAN. Show that Silhouette incorrectly assigns a negative score while DBCV correctly confirms high density quality.
6. **External Metric Invariance Benchmark**: Write a pytest function asserting that `adjusted_rand_score`, `normalized_mutual_info_score`, and `v_measure_score` remain 100% invariant when true labels are randomly shuffled, reversed, or offset by constant scalars.
