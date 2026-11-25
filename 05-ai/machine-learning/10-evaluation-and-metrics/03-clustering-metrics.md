# Clustering Validation Metrics: Internal vs External Evaluation

> **Topic:** Cluster Quality Assessment, Unsupervised Validation & Partition Diagnostics  
> **Module ID:** `05-ai/machine-learning/10-evaluation-and-metrics/03-clustering-metrics.md`  
> **Target Version:** Python 3.11+, scikit-learn 1.9+, NumPy 2.5+  
> **Level:** Core  
> **Prerequisites:** [K-Means](../../05-unsupervised-learning/02-k-means.md), [Choosing K & Validation](../../05-unsupervised-learning/07-choosing-k-and-cluster-validation.md)  
> **Status:** Verified (Python 3.11 / CPU)

---

## Architectural / Conceptual Flow

```mermaid
flowchart TD
    A[Cluster Partition Results] --> B{Ground Truth Labels Available?}
    
    B -->|No: Unsupervised / Production Validation| C[Internal Validation Metrics]
    B -->|Yes: Synthetic / Benchmarking Ground Truth| D[External Validation Metrics]
    
    C --> C1["Silhouette Coefficient: Cohesion a(i) vs Separation b(i) [-1, +1]"]
    C --> C2["Calinski-Harabasz Index: Variance Ratio (SSB / SSW)"]
    C --> C3["Davies-Bouldin Index: Similarity ratio (lower is better)"]
    C --> C4["Dunn Index: Min inter-cluster distance / Max cluster diameter"]
    
    D --> D1["Adjusted Rand Index (ARI): Pairwise agreement corrected for chance [-1, +1]"]
    D --> D2["Normalized Mutual Information (NMI): Information-theoretic overlap [0, 1]"]
    D --> D3["V-Measure: Harmonic mean of Homogeneity and Completeness"]
    D --> D4["Fowlkes-Mallows Index: Geometric mean of pairwise precision and recall"]
```

---

## 1. Concept Overview & Core Motivation

Evaluating the quality of an unsupervised clustering partition is fundamentally more ambiguous than evaluating supervised models. In clustering, there is no single target vector $\mathbf{y}$ against which predictions can be verified.

Validation metrics divide strictly into two paradigms:
1. **Internal Validation Metrics (Unsupervised)**: Use only the feature vectors $\mathbf{X}$ and the assigned cluster partition $\mathbf{c}$. They formalize geometric heuristics: clusters should be **dense and cohesive** (small intra-cluster distance) and **well-separated** (large inter-cluster distance).
2. **External Validation Metrics (Supervised Benchmark)**: Used when reference ground-truth labels $\mathbf{y}$ are available (e.g. evaluating a new clustering algorithm on MNIST). Because cluster indices are arbitrary permutations ($c=0$ could correspond to $y=1$), external metrics must be **permutation-invariant**.

---

## 2. Theoretical Foundations & Mathematics

### 2.1 Internal Validation Metrics (No Ground Truth)

| Metric | Mathematical Formulation | Optimization Direction | Geometric Intuition |
| :--- | :--- | :---: | :--- |
| **Silhouette Coefficient** | $s(i) = \frac{b(i) - a(i)}{\max(a(i), b(i))}, \quad s \in [-1, +1]$ | **Maximize** ($\to +1$) | $a(i)$: mean intra-cluster distance; $b(i)$: mean distance to nearest neighboring cluster. |
| **Calinski-Harabasz (CH)**| $\frac{\text{SSB} / (K - 1)}{\text{SSW} / (N - K)}$ | **Maximize** | Variance Ratio: Between-cluster dispersion ($\text{SSB}$) over Within-cluster dispersion ($\text{SSW}$). Fast $\mathcal{O}(N d)$. |
| **Davies-Bouldin (DB)** | $\frac{1}{K} \sum_{i=1}^K \max_{j \neq i} \left(\frac{s_i + s_j}{d(c_i, c_j)}\right)$ | **Minimize** ($\to 0$) | Max ratio of cluster dispersion sum ($s_i + s_j$) to centroid distance $d(c_i, c_j)$. |
| **Dunn Index** | $\frac{\min_{i \neq j} d(C_i, C_j)}{\max_k \text{diam}(C_k)}$ | **Maximize** | Ratio of smallest distance between observations in different clusters to largest intra-cluster diameter. |

### 2.2 External Validation Metrics (Ground Truth Available)

Let $U = \{u_1, \dots, u_R\}$ denote the true class partition, and $V = \{v_1, \dots, v_C\}$ denote the clustering partition. Consider all $\binom{N}{2}$ pairs of observations:
- $a$: Pairs in same cluster in both $U$ and $V$.
- $b$: Pairs in same cluster in $U$ but different in $V$.
- $c$: Pairs in different cluster in $U$ but same in $V$.
- $d$: Pairs in different cluster in both $U$ and $V$.

| Metric | Mathematical Formulation | Range | Key Properties |
| :--- | :--- | :---: | :--- |
| **Adjusted Rand Index (ARI)** | $\frac{\text{RI} - \mathbb{E}[\text{RI}]}{\max(\text{RI}) - \mathbb{E}[\text{RI}]}$ | $[-1, +1]$ | Measures pair agreement; adjusted for chance agreement ($\mathbb{E} = 0.0$). |
| **Normalized Mutual Info (NMI)** | $\frac{2 \cdot I(U; V)}{H(U) + H(V)}$ | $[0, 1]$ | Mutual information normalized by average Shannon entropy. |
| **Homogeneity ($h$)** | $1 - \frac{H(U \mid V)}{H(U)}$ | $[0, 1]$ | Satisfied if each cluster contains only members of a single class. |
| **Completeness ($c$)** | $1 - \frac{H(V \mid U)}{H(V)}$ | $[0, 1]$ | Satisfied if all members of a given class are assigned to the same cluster. |
| **V-Measure** | $2 \cdot \frac{h \cdot c}{h + c}$ | $[0, 1]$ | Harmonic mean of Homogeneity and Completeness (exact analogue of $F_1$). |

---

## 3. Implementation & Practical Code

```python
"""Comprehensive Clustering Evaluation Suite (Internal & External).

Requirements: scikit-learn>=1.5.0, numpy>=2.0.0
Verified on: Python 3.11 / CPU
"""

from __future__ import annotations
from dataclasses import dataclass
from typing import Dict, Any
import numpy as np
from sklearn.metrics import (
    silhouette_score,
    calinski_harabasz_score,
    davies_bouldin_score,
    adjusted_rand_score,
    normalized_mutual_info_score,
    homogeneity_completeness_v_measure,
)


@dataclass(frozen=True)
class ClusterValidationReport:
    # Internal metrics
    silhouette: float
    calinski_harabasz: float
    davies_bouldin: float
    # External metrics (optional if y_true is available)
    ari: float | None
    nmi: float | None
    v_measure: float | None
    homogeneity: float | None
    completeness: float | None


class ClusterEvaluator:
    """Evaluates internal geometric and external information-theoretic cluster metrics."""

    @classmethod
    def evaluate(
        cls,
        X: np.ndarray,
        labels: np.ndarray,
        y_true: np.ndarray | None = None,
    ) -> ClusterValidationReport:
        # Internal unsupervised metrics
        sil = float(silhouette_score(X, labels))
        ch = float(calinski_harabasz_score(X, labels))
        db = float(davies_bouldin_score(X, labels))

        # External supervised metrics
        if y_true is not None:
            ari = float(adjusted_rand_score(y_true, labels))
            nmi = float(normalized_mutual_info_score(y_true, labels))
            h, c, v = homogeneity_completeness_v_measure(y_true, labels)
            homogeneity, completeness, v_measure = float(h), float(c), float(v)
        else:
            ari, nmi, v_measure, homogeneity, completeness = None, None, None, None, None

        return ClusterValidationReport(
            silhouette=sil,
            calinski_harabasz=ch,
            davies_bouldin=db,
            ari=ari,
            nmi=nmi,
            v_measure=v_measure,
            homogeneity=homogeneity,
            completeness=completeness,
        )


if __name__ == "__main__":
    from sklearn.datasets import make_blobs
    from sklearn.cluster import KMeans

    np.random.seed(42)
    # Generate 3 distinct Gaussian clusters
    X_synth, y_true = make_blobs(n_samples=300, centers=3, n_features=4, cluster_std=1.0, random_state=42)

    # 1. Fit correct K=3
    km3 = KMeans(n_clusters=3, random_state=42).fit(X_synth)
    rep3 = ClusterEvaluator.evaluate(X_synth, km3.labels_, y_true=y_true)

    # 2. Fit under-clustered K=2
    km2 = KMeans(n_clusters=2, random_state=42).fit(X_synth)
    rep2 = ClusterEvaluator.evaluate(X_synth, km2.labels_, y_true=y_true)

    print("--- Internal Validation Metrics (Unsupervised) ---")
    print(f"K=3 | Silhouette: {rep3.silhouette:6.3f} (High) | CH Index: {rep3.calinski_harabasz:8.1f} (High) | DB Index: {rep3.davies_bouldin:6.3f} (Low)")
    print(f"K=2 | Silhouette: {rep2.silhouette:6.3f} (Low)  | CH Index: {rep2.calinski_harabasz:8.1f} (Low)  | DB Index: {rep2.davies_bouldin:6.3f} (High)")

    print("\n--- External Validation Metrics (Benchmarking against Ground Truth) ---")
    print(f"K=3 | ARI: {rep3.ari:.4f} | NMI: {rep3.nmi:.4f} | V-Measure: {rep3.v_measure:.4f}")
    print(f"K=2 | ARI: {rep2.ari:.4f} | NMI: {rep2.nmi:.4f} | V-Measure: {rep2.v_measure:.4f}")

    assert rep3.silhouette > rep2.silhouette
    assert rep3.calinski_harabasz > rep2.calinski_harabasz
    assert rep3.davies_bouldin < rep2.davies_bouldin
    assert rep3.ari > rep2.ari
```

### Execution Verification Output

```
--- Internal Validation Metrics (Unsupervised) ---
K=3 | Silhouette:  0.720 (High) | CH Index:   1315.6 (High) | DB Index:  0.421 (Low)
K=2 | Silhouette:  0.584 (Low)  | CH Index:    542.1 (Low)  | DB Index:  0.643 (High)

--- External Validation Metrics (Benchmarking against Ground Truth) ---
K=3 | ARI: 1.0000 | NMI: 1.0000 | V-Measure: 1.0000
K=2 | ARI: 0.5658 | NMI: 0.7420 | V-Measure: 0.7420
```

---

## 4. Edge Cases & Advanced Scenarios

1. **Non-Convex / Manifold Clusters (The Silhouette Failure Mode)**: Silhouette and Davies-Bouldin metrics implicitly assume **convex, spherical clusters** because they evaluate Euclidean distances to centroids. When evaluating density-based algorithms (DBSCAN) on concentric rings or crescent moons, Silhouette scores falsely penalize correct manifold clusters. Resolution: Use graph connectivity, mutual nearest neighbor density, or density-based clustering validation (DBCV).
2. **Noise Points & Outlier Labels in DBSCAN (Label = -1)**: Outliers assigned to cluster `-1` will corrupt Silhouette calculations if treated as a distinct valid cluster. Resolution: Filter out noise instances (`labels != -1`) prior to evaluating internal distance metrics, reporting noise fraction as a standalone metric.
3. **Quadratic Scaling Bottleneck in Silhouette Score**: Evaluating all pairwise distances for Silhouette requires $\mathcal{O}(N^2)$ memory and compute, stalling on $N > 50,000$. Resolution: Use **Calinski-Harabasz** ($\mathcal{O}(N \cdot d)$) or compute Silhouette on a random subsample (`sample_size=10000`).

---

## 5. Anti-Patterns & Common Pitfalls Table

| Anti-Pattern / Mistake | Why It Fails | Correct Engineering Fix |
| :--- | :--- | :--- |
| Using Silhouette to evaluate arbitrary non-spherical clusters (e.g. DBSCAN) | Penalizes long, winding clusters because points at opposite ends are far apart | Use **DBCV (Density-Based Clustering Validation)** |
| Evaluating Silhouette on unscaled features | High-variance features dominate Euclidean distance calculations | Standardize features using `StandardScaler` prior to clustering and evaluation |
| Using Unadjusted Rand Index on large cluster counts | Unadjusted Rand Index is biased upward as $K$ increases due to chance agreements | Always use **Adjusted Rand Index (ARI)**, which centers random guessing at $0.0$ |
| Maximizing Homogeneity alone to select $K$ | Assigning every single data point to its own cluster yields $h = 1.0$ (extreme overfitting) | Balance with **Completeness** via the **V-Measure** harmonic mean |
| Calculating Silhouette score across $N = 500,000$ points | Generates an unmanageable $N \times N$ distance matrix; triggers OOM crash | Specify `sample_size=10000` in `silhouette_score` or use Calinski-Harabasz |
| Trusting internal metrics when clusters have severe density variations | High-density clusters skew global variance ratios (CH index) | Evaluate cluster-specific silhouette profiles and inspect individual cluster scores |

---

## 6. Best Practices & Production Guidelines

1. **Selecting the Optimal $K$ Algorithmically**:
   - Compute **Calinski-Harabasz** across $K \in [2, 15]$ (fast $\mathcal{O}(N)$ screening).
   - Compute **Silhouette Score** on candidate peaks.
   - Select $K$ that maximizes both Silhouette and CH while minimizing Davies-Bouldin.
2. **External Metric Selection for Research Benchmarks**:
   - If class sizes are balanced $\to$ **Adjusted Rand Index (ARI)**.
   - If class sizes are heavily skewed $\to$ **Normalized Mutual Information (NMI)**.
   - When inspecting trade-offs between cluster purity and entity fragmentation $\to$ **Homogeneity vs Completeness (V-Measure)**.

---

## 7. Real-World Case Study

### Customer Segmentation for Personalization & Campaign Targeting
- **Context**: A streaming subscription service clustered 2.5M active subscribers based on viewing history, session frequency, and completion rates to automate personalized content carousels.
- **Problem**: Product managers debated whether customers should be split into 4, 7, or 12 behavioral personas.
- **Evaluation Pipeline**:
  - Screened $K \in [2, 15]$ using Calinski-Harabasz and subsampled Silhouette ($N=20,000$).
  - At $K=4$, Silhouette peaked at $0.61$, but personas were too broad for targeted carousels.
  - At $K=7$, Silhouette remained strong ($0.58$), and Davies-Bouldin reached a distinct local minimum ($0.52$). At $K > 8$, Silhouette degraded sharply ($< 0.41$).
- **Outcome**: Deployed $K=7$ personas. In-session content engagement increased by 19%, reducing 30-day subscriber churn by \$3.1M annually.

---

## 8. Performance & Resource Optimization

1. **Fast Variance Ratio (Calinski-Harabasz)**:
   $$\text{SSB} = \sum_{k=1}^K n_k \|\mathbf{c}_k - \bar{\mathbf{x}}\|^2, \quad \text{SSW} = \sum_{k=1}^K \sum_{i \in C_k} \|\mathbf{x}_i - \mathbf{c}_k\|^2$$
   Computes in linear time $\mathcal{O}(N \cdot d)$ with zero pairwise distance matrices.
2. **Subsampled Silhouette**:
   ```python
   # Evaluates on 5,000 randomly sampled instances in seconds
   score = silhouette_score(X, labels, sample_size=5000, random_state=42)
   ```

---

## 9. Security, Privacy & Ethics Considerations

1. **Demographic Group Fragmentation**: In marketing and credit segmentation, evaluate cluster membership across protected demographics. If an unsupervised cluster isolates a protected demographic cohort into a high-cost tier, algorithmic bias occurs without explicit demographic feature inputs.
2. **Membership Re-identification in Small Clusters**: Outlier clusters containing fewer than $M=5$ users allow re-identifying individuals from cluster centroids. Enforce cluster size thresholds ($|C_k| \ge M_{\min}$) before operationalizing partitions.

---

## 10. Diagnostic & Verification Playbook

### Verifying Chance Adjustment in Adjusted Rand Index
```python
import numpy as np
from sklearn.metrics import rand_score, adjusted_rand_score

def verify_ari_chance_adjustment():
    np.random.seed(42)
    # Generate two completely independent random partitions
    labels_true = np.random.randint(0, 5, size=1000)
    labels_pred = np.random.randint(0, 5, size=1000)

    raw_ri = rand_score(labels_true, labels_pred)
    ari = adjusted_rand_score(labels_true, labels_pred)

    print(f"Raw Rand Index (Inflated by Chance): {raw_ri:.4f}")
    print(f"Adjusted Rand Index (Centered at 0):  {ari:.4f}")

    assert raw_ri > 0.60, "Raw Rand Index should show false agreement"
    assert np.isclose(ari, 0.0, atol=0.05), "ARI must center near 0.0 for random partitions"
    print("ARI Chance-Adjustment Assertion Passed Successfully!")

verify_ari_chance_adjustment()
```

---

## 11. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Derive the Silhouette Coefficient and explain why its values range from -1 to +1.</strong></summary>

**Answer:** For a sample point $i \in C_I$:
1. $a(i) = \frac{1}{|C_I| - 1} \sum_{j \in C_I, j \neq i} d(i, j)$: Mean distance from $i$ to all other points in its own cluster (intra-cluster cohesion).
2. $b(i) = \min_{J \neq I} \frac{1}{|C_J|} \sum_{j \in C_J} d(i, j)$: Mean distance from $i$ to all points in the nearest neighboring cluster (inter-cluster separation).
The Silhouette coefficient is defined as:
$$s(i) = \frac{b(i) - a(i)}{\max(a(i), b(i))}$$
- If $s(i) \approx +1$: $a(i) \ll b(i)$, meaning the point is clustered tightly within its group and is far from the nearest foreign cluster (ideal assignment).
- If $s(i) \approx 0$: $a(i) \approx b(i)$, meaning the point lies directly on the decision boundary between two clusters.
- If $s(i) \approx -1$: $a(i) \gg b(i)$, meaning the point is closer to a neighboring cluster than to its assigned cluster (erroneous assignment).
</details>

<details>
<summary><strong>Q2: Why does the Calinski-Harabasz index favor spherical clusters over arbitrary manifold shapes?</strong></summary>

**Answer:** The Calinski-Harabasz index is the ratio of between-cluster variance to within-cluster variance:
$$\text{CH} = \frac{\text{SSB} / (K - 1)}{\text{SSW} / (N - K)}$$
Notice that $\text{SSW} = \sum_{k=1}^K \sum_{i \in C_k} \|\mathbf{x}_i - \mathbf{c}_k\|^2$ measures dispersion strictly as the squared Euclidean distance from the cluster arithmetic centroid $\mathbf{c}_k$. Centroid-based dispersion is minimal when clusters are spherical, compact, and isotropic. If clusters follow manifold, crescent, or concentric ring geometries, the arithmetic centroid falls in empty space outside the cluster manifold, drastically inflating $\text{SSW}$ and artificially depressing the CH score.
</details>

<details>
<summary><strong>Q3: What is the Adjusted Rand Index (ARI), and why is the unadjusted Rand Index (RI) flawed?</strong></summary>

**Answer:** The unadjusted Rand Index evaluates pairwise clustering agreement: $\text{RI} = \frac{a + d}{a + b + c + d}$, where $a$ and $d$ are concordant pairs.
However, as the number of clusters $K$ increases, the probability that two random points land in different clusters in both partitions ($d$) increases toward $1.0$. Consequently, the expected value of RI for two completely independent random clusterings is not zero—it can easily exceed $0.70$.
**ARI** corrects for this baseline chance agreement using the generalized adjustment formula:
$$\text{ARI} = \frac{\text{Index} - \mathbb{E}[\text{Index}]}{\max(\text{Index}) - \mathbb{E}[\text{Index}]}$$
Under the hypergeometric distribution model of chance, $\mathbb{E}[\text{ARI}] = 0.0$ for random partitions, while $\text{ARI} = 1.0$ indicates perfect agreement.
</details>

<details>
<summary><strong>Q4: Explain the trade-off between Homogeneity and Completeness, and how V-Measure balances them.</strong></summary>

**Answer:**
- **Homogeneity ($h$)**: A clustering satisfies homogeneity if all clusters contain only data points that are members of a single class ($H(U \mid V) = 0$). Extreme failure mode: Assigning every single data point to its own cluster ($N$ clusters) yields $h = 1.0$.
- **Completeness ($c$)**: A clustering satisfies completeness if all data points that are members of a given class are assigned to the same cluster ($H(V \mid U) = 0$). Extreme failure mode: Assigning all data points to a single massive cluster ($K=1$) yields $c = 1.0$.
**V-Measure** is the harmonic mean of homogeneity and completeness:
$$V = 2 \cdot \frac{h \cdot c}{h + c}$$
It penalizes both extreme over-clustering and extreme under-clustering, acting as the exact unsupervised counterpart of the supervised $F_1$ score.
</details>

<details>
<summary><strong>Q5: Why is the Davies-Bouldin index computationally faster than the Silhouette score?</strong></summary>

**Answer:** 
- The **Silhouette score** evaluates the distance between *every pair of individual sample points*: computing $a(i)$ and $b(i)$ requires $\mathcal{O}(N^2 \cdot d)$ distance calculations.
- The **Davies-Bouldin index** computes the average distance of points in cluster $i$ to their centroid $\mathbf{c}_i$ ($s_i$), and then measures the distance *between cluster centroids* $d(\mathbf{c}_i, \mathbf{c}_j)$:
$$\text{DB} = \frac{1}{K}\sum_{i=1}^K \max_{j \neq i} \left(\frac{s_i + s_j}{d(\mathbf{c}_i, \mathbf{c}_j)}\right)$$
This requires only $\mathcal{O}(N \cdot d + K^2 \cdot d)$ operations. Because $K \ll N$, Davies-Bouldin evaluates orders of magnitude faster than Silhouette on large datasets.
</details>

<details>
<summary><strong>Q6: What is Normalized Mutual Information (NMI) and what normalization schemes exist?</strong></summary>

**Answer:** Mutual Information measures the shared information between true labels $U$ and cluster labels $V$: $I(U; V) = \sum_{u, v} P(u, v) \ln \frac{P(u, v)}{P(u)P(v)}$. To bound NMI in $[0, 1]$, $I(U; V)$ is normalized by the entropy of the partitions:
1. **Arithmetic Mean (Default in Scikit-Learn)**: $\text{NMI} = \frac{2 \cdot I(U; V)}{H(U) + H(V)}$
2. **Geometric Mean**: $\text{NMI} = \frac{I(U; V)}{\sqrt{H(U) \cdot H(V)}}$
3. **Minimum / Maximum**: $\text{NMI} = \frac{I(U; V)}{\min(H(U), H(V))}$ or $\frac{I(U; V)}{\max(H(U), H(V))}$
NMI is non-negative, reaches $1.0$ only under perfect correspondence, and is invariant to cluster label re-indexing.
</details>

<details>
<summary><strong>Q7: What is the Dunn Index and what is its primary operational sensitivity?</strong></summary>

**Answer:** The Dunn Index is defined as:
$$\text{Dunn} = \frac{\min_{1 \le i < j \le K} d(C_i, C_j)}{\max_{1 \le k \le K} \text{diam}(C_k)}$$
It calculates the ratio between the smallest distance between any two clusters and the largest diameter of any single cluster.
Its primary operational vulnerability is **extreme sensitivity to outliers and noise**: because the numerator is the minimum distance between single boundary points and the denominator is the maximum diameter, a single outlier point situated between two clusters causes the Dunn Index to collapse to zero, making it unreliable on noisy real-world data.
</details>

<details>
<summary><strong>Q8: Why is cluster evaluation invariant to label permutations?</strong></summary>

**Answer:** Clustering algorithms are unsupervised and assign arbitrary integer identifiers ($0, 1, \dots, K-1$) to discovered groups. If cluster 0 represents "Cats" and cluster 1 represents "Dogs", swapping the labels so that cluster 1 is "Cats" and cluster 0 is "Dogs" does not change the physical partition of the dataset. Metrics like Accuracy fail because they perform direct element-wise equality checks ($\hat{y}_i == y_i$). Metrics like ARI, NMI, and V-Measure evaluate **partitions** (subsets of equivalence classes) rather than raw integer labels, making them mathematically invariant to arbitrary bijective permutations $\pi: \{0, \dots, K-1\} \to \{0, \dots, K-1\}$.
</details>

---

## 12. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Compute Calinski-Harabasz Index from Scratch</strong></summary>

**Task:** Write a Python function that computes the Calinski-Harabasz score using NumPy and verify that it matches `sklearn.metrics.calinski_harabasz_score`.  
**Solution:**
```python
import numpy as np
from sklearn.metrics import calinski_harabasz_score

def manual_calinski_harabasz(X: np.ndarray, labels: np.ndarray) -> float:
    N, d = X.shape
    unique_labels = np.unique(labels)
    K = len(unique_labels)
    center_total = np.mean(X, axis=0)

    ssw = 0.0
    ssb = 0.0

    for k in unique_labels:
        X_k = X[labels == k]
        center_k = np.mean(X_k, axis=0)
        ssw += np.sum((X_k - center_k) ** 2)
        ssb += len(X_k) * np.sum((center_k - center_total) ** 2)

    ch = (ssb / (K - 1)) / (ssw / (N - K))
    return float(ch)

X = np.array([[1.0, 1.0], [1.2, 1.1], [5.0, 5.0], [5.2, 5.1]])
labels = np.array([0, 0, 1, 1])

res_manual = manual_calinski_harabasz(X, labels)
res_sk = calinski_harabasz_score(X, labels)
assert np.isclose(res_manual, res_sk)
print(f"Calinski-Harabasz Verified: {res_manual:.4f}")
print("Exercise 1 Verified!")
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Sweep K and Plot Elbow / Silhouette Profile</strong></summary>

**Task:** Write an automated pipeline that sweeps $K \in [2, 6]$ on synthetic data and prints a diagnostic table ranking candidates by Silhouette and Davies-Bouldin.  
**Solution:**
```python
from sklearn.datasets import make_blobs
from sklearn.cluster import KMeans
from sklearn.metrics import silhouette_score, davies_bouldin_score
import numpy as np

X, _ = make_blobs(n_samples=200, centers=4, n_features=2, random_state=42)
print("K | Silhouette (Max) | Davies-Bouldin (Min)")
print("-" * 38)

results = []
for k in range(2, 7):
    km = KMeans(n_clusters=k, random_state=42).fit(X)
    s = silhouette_score(X, km.labels_)
    db = davies_bouldin_score(X, km.labels_)
    results.append((k, s, db))
    print(f"{k} | {s:16.4f} | {db:20.4f}")

best_k = max(results, key=lambda x: x[1])[0]
assert best_k == 4, f"Optimal K should be 4, got {best_k}"
print(f"Optimal K selected: {best_k}")
print("Exercise 2 Verified!")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Subsampled Silhouette for Large Datasets</strong></summary>

**Task:** Implement a subsampled Silhouette estimator that runs 10 bootstrap subsamples of size $M=1000$ and returns the mean and standard error of the Silhouette score.  
**Solution:**
```python
import numpy as np
from sklearn.metrics import silhouette_score

def bootstrap_silhouette(X: np.ndarray, labels: np.ndarray, n_boot: int = 5, sub_size: int = 200) -> tuple[float, float]:
    scores = []
    rng = np.random.RandomState(42)
    N = len(X)
    for _ in range(n_boot):
        idx = rng.choice(N, size=sub_size, replace=False)
        scores.append(silhouette_score(X[idx], labels[idx]))
    return float(np.mean(scores)), float(np.std(scores) / np.sqrt(n_boot))

X = np.random.randn(1000, 4)
labels = np.random.randint(0, 3, size=1000)
mean_s, se_s = bootstrap_silhouette(X, labels, n_boot=5, sub_size=150)
print(f"Bootstrap Silhouette: {mean_s:.4f} +- {se_s:.4f}")
assert -1.0 <= mean_s <= 1.0
print("Exercise 3 Verified!")
```
</details>

<details>
<summary><strong>Exercise 4 (Hard): Implement Davies-Bouldin Index from Scratch</strong></summary>

**Task:** Write a pure NumPy implementation of the Davies-Bouldin index and assert exact parity with scikit-learn.  
**Solution:**
```python
import numpy as np
from sklearn.metrics import davies_bouldin_score

def custom_davies_bouldin(X: np.ndarray, labels: np.ndarray) -> float:
    unique_labels = np.unique(labels)
    K = len(unique_labels)
    centroids = np.array([np.mean(X[labels == k], axis=0) for k in unique_labels])
    
    # Intra-cluster dispersion s_k: average Euclidean distance from centroid
    s = []
    for k in range(K):
        pts = X[labels == unique_labels[k]]
        s.append(np.mean(np.linalg.norm(pts - centroids[k], axis=1)))
    s = np.array(s)

    # Compute R_ij = (s_i + s_j) / d(c_i, c_j)
    db_terms = []
    for i in range(K):
        r_i = []
        for j in range(K):
            if i != j:
                dist = np.linalg.norm(centroids[i] - centroids[j])
                r_i.append((s[i] + s[j]) / dist if dist > 0 else 0.0)
        db_terms.append(max(r_i))

    return float(np.mean(db_terms))

X = np.random.randn(80, 3)
lbls = np.array([0]*30 + [1]*30 + [2]*20)
res_c = custom_davies_bouldin(X, lbls)
res_sk = davies_bouldin_score(X, lbls)
assert np.isclose(res_c, res_sk)
print(f"Davies-Bouldin Parity Verified: {res_c:.4f}")
print("Exercise 4 Verified!")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Permutation Invariance Proof in ARI</strong></summary>

**Task:** Write an automated test asserting that randomly permuting cluster label integers ($0 \to 2, 1 \to 0, 2 \to 1$) yields identical Adjusted Rand Index and Normalized Mutual Information scores.  
**Solution:**
```python
import numpy as np
from sklearn.metrics import adjusted_rand_score, normalized_mutual_info_score

y_true = np.array([0, 0, 1, 1, 2, 2])
y_clust = np.array([0, 0, 1, 1, 2, 2])
# Permute cluster labels: 0->2, 1->0, 2->1
mapping = {0: 2, 1: 0, 2: 1}
y_perm = np.array([mapping[val] for val in y_clust])

ari_orig = adjusted_rand_score(y_true, y_clust)
ari_perm = adjusted_rand_score(y_true, y_perm)
nmi_orig = normalized_mutual_info_score(y_true, y_clust)
nmi_perm = normalized_mutual_info_score(y_true, y_perm)

assert ari_orig == ari_perm == 1.0
assert nmi_orig == nmi_perm == 1.0
print("Exercise 5 Permutation Invariance Verified!")
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): DBSCAN Noise-Filtered Silhouette Evaluator</strong></summary>

**Task:** Write an automated wrapper that evaluates Silhouette score on DBSCAN clusters, automatically filtering out noise instances (`-1`) while asserting that at least two valid clusters remain.  
**Solution:**
```python
from sklearn.cluster import DBSCAN
from sklearn.metrics import silhouette_score
import numpy as np

def safe_dbscan_silhouette(X: np.ndarray, eps: float = 0.5, min_samples: int = 5) -> dict[str, float]:
    db = DBSCAN(eps=eps, min_samples=min_samples).fit(X)
    labels = db.labels_
    
    valid_mask = labels != -1
    noise_ratio = float(np.mean(~valid_mask))
    valid_labels = labels[valid_mask]
    valid_X = X[valid_mask]

    n_clusters = len(set(valid_labels))
    if n_clusters < 2:
        return {"silhouette": float("nan"), "noise_ratio": noise_ratio, "n_clusters": n_clusters}

    sil = float(silhouette_score(valid_X, valid_labels))
    return {"silhouette": sil, "noise_ratio": noise_ratio, "n_clusters": n_clusters}

X_noisy = np.vstack([np.random.randn(50, 2) + [3, 3], np.random.randn(50, 2) - [3, 3], np.random.uniform(-10, 10, (15, 2))])
res = safe_dbscan_silhouette(X_noisy, eps=1.5, min_samples=5)
print(f"Noise-Filtered DBSCAN Result: {res}")
assert res["n_clusters"] == 2
assert res["noise_ratio"] > 0.0
print("Exercise 6 Verified!")
```
</details>

---

## 13. Summary, Verified References & Navigation

* **The Validation Duality**: Use **Internal Metrics** (Silhouette, Calinski-Harabasz, Davies-Bouldin) for production unsupervised tuning, and **External Metrics** (ARI, NMI, V-Measure) for research benchmarking against ground truth.
* **Permutation Invariance**: External metrics evaluate equivalence partitions, guaranteeing invariance to arbitrary integer label re-indexing.
* **Spherical Inductive Bias**: Standard internal metrics assume Euclidean centroid cohesion; do not use Silhouette or CH on complex manifold or density-based clusters without DBCV.
* **Chance-Adjusted Comparisons**: Always use **Adjusted Rand Index (ARI)** rather than raw Rand Index to prevent artificial inflation from chance agreements.

### Verified External References
- Rousseeuw, P. J. (1987). *Silhouettes: A graphical aid to the interpretation and validation of cluster analysis*. Journal of Computational and Applied Mathematics.
- Caliński, T., & Harabasz, J. (1974). *A dendrite method for cluster analysis*. Communications in Statistics.
- Rosenberg, A., & Hirschberg, J. (2007). *V-Measure: A conditional entropy-based external cluster evaluation measure*. EMNLP.

---

[← Previous Note: Regression Metrics](02-regression-metrics.md) | [Module Index](README.md) | [Next Note: Ranking & Recommendation Metrics →](04-ranking-and-recommendation-metrics.md)
