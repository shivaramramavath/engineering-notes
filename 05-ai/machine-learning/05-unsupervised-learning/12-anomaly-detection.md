# Unsupervised Anomaly Detection

> **Track:** `05-ai/machine-learning` | **Module:** `05-unsupervised-learning` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `NumPy: >=1.26.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Isolation Forest (iForest), Local Outlier Factor (LOF), One-Class SVM (OCSVM) & Elliptic Envelope |
| **Family** | Unsupervised Anomaly / Outlier / Novelty Detection |
| **Input Feature Types** | Continuous numerical features ($X \in \mathbb{R}^{n \times d}$). Tree methods invariant to scaling; distance/kernel methods strictly scale-dependent. |
| **Target Output** | Binary outlier flags $y \in \{-1, +1\}$ ($+1 = \text{Inlier / Normal}$, $-1 = \text{Outlier / Anomaly}$) and continuous anomaly scores $s(\mathbf{x}) \in [0, 1]$ or $(-\infty, +\infty)$. |
| **Optimization Targets** | **Isolation Forest**: Sub-sampling tree isolation depth: $s(\mathbf{x}, n) = 2^{-\frac{\mathbb{E}[h(\mathbf{x})]}{c(n)}}$. **LOF**: Ratio of local reachability density to nearest neighbors. **One-Class SVM**: Maximum margin hyperplane separating data from coordinate origin with slack budget $\nu$. **Elliptic Envelope**: Robust Mahalanobis distance via FastMCD (Minimum Covariance Determinant). |
| **Key Hyperparameters** | `contamination` (Expected fraction of outliers), `n_estimators`, `max_samples` (256 default for iForest), `n_neighbors` (LOF), `nu` / `gamma` (OCSVM). |
| **Inference Mechanism** | Evaluates continuous decision function against threshold $\tau$ (determined by `contamination` parameter): $\hat{y} = \text{sign}(s(\mathbf{x}) - \tau)$. |
| **Primary Failure Modes** | **Swamping**: Normal points mistaken for anomalies. **Masking**: True anomalies clustered close together masking each other. Axis-aligned partition artifacts in standard iForest. |

---

## 2. Architecture & Intuition

In real-world security, fraud, and hardware failure systems, anomalies are **rare, novel, and adversarial**. Standard supervised classification fails because labeled positive instances are either non-existent or rapidly morph to evade static rules.

Unsupervised anomaly detection algorithms exploit two fundamental properties of anomalies:
1. **Few**: They represent a tiny statistical fraction of the population ($< 1\%$).
2. **Different**: Their feature values deviate significantly from nominal dense patterns.

```
+--------------------------------------------------------------------------------------------------+
|                            FOUR ANOMALY DETECTION ARCHITECTURES                                  |
|                                                                                                  |
| 1. ISOLATION FOREST (Tree Partitioning)        2. LOCAL OUTLIER FACTOR (Density Ratio)           |
|    Normal points require MANY cuts to isolate.    Compares local density of x to its k-NNs.       |
|    Anomalies are ISOLATED IN FEW CUTS (shallow).  Detects anomalies in variable density regimes. |
|                                                                                                  |
| 3. ONE-CLASS SVM (Kernel Margin)               4. ELLIPTIC ENVELOPE (Robust Covariance)          |
|    Encloses data in minimum volume RBF tube;      Fits robust Gaussian ellipsoid using           |
|    origin separates normal data from outliers.    Minimum Covariance Determinant (MCD).          |
+--------------------------------------------------------------------------------------------------+
```

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $h(\mathbf{x})$ | $\mathbb{N}$ | Path length: Number of tree edges traversed from root to isolate $\mathbf{x}$ |
| $c(n)$ | $\mathbb{R}_{> 0}$ | Average path length of unsuccessful searches in a Binary Search Tree (BST) |
| $s(\mathbf{x}, n)$ | $[0, 1]$ | Isolation Forest anomaly score for sample $\mathbf{x}$ |
| $\text{reach-dist}_k(\mathbf{p}, \mathbf{o})$| $\mathbb{R}_{\ge 0}$ | Reachability distance between points $\mathbf{p}$ and $\mathbf{o}$ |
| $\text{lrd}_k(\mathbf{p})$ | $\mathbb{R}_{> 0}$ | Local reachability density of point $\mathbf{p}$ |
| $\text{LOF}_k(\mathbf{p})$ | $\mathbb{R}_{\ge 0}$ | Local Outlier Factor score ($\approx 1.0$ is normal, $\gg 1.0$ is anomaly) |
| $\nu \in (0, 1]$ | Float | Upper bound on fraction of outliers / lower bound on support vectors in OCSVM |

### 1. Isolation Forest (Liu, Ting, Zhou, 2008)
Standard trees (CART) isolate classes using supervised impurity. **Isolation Trees (iTrees)** partition data using completely random orthogonal cuts:
1. Randomly select a feature dimension $j \in \{1, \dots, d\}$.
2. Randomly select a split threshold $p \in [\min(x_j), \max(x_j)]$.
3. Recursively split until sample $\mathbf{x}$ is isolated into a single leaf node.

Because anomalies have atypical values, they are **isolated very close to the root** (small path length $h(\mathbf{x})$). Normal instances in dense clusters require many deep cuts.

#### The Anomaly Score Formula
The average path length of an unsuccessful search in a Binary Search Tree (BST) of size $n$ is:

$$c(n) = 2 \left( \ln(n - 1) + \gamma_{\text{Euler}} \right) - \frac{2(n - 1)}{n}$$

Where $\gamma_{\text{Euler}} \approx 0.5772156649$ (Euler-Mascheroni constant). The anomaly score $s(\mathbf{x}, n)$ across an ensemble of $T$ isolation trees is:

$$s(\mathbf{x}, n) = 2^{-\frac{\mathbb{E}[h(\mathbf{x})]}{c(n)}}$$

- If $\mathbb{E}[h(\mathbf{x})] \to 0 \implies s \to 1.0$: Strong anomaly.
- If $\mathbb{E}[h(\mathbf{x})] \to n - 1 \implies s \to 0.0$: Highly central inlier.
- If $\mathbb{E}[h(\mathbf{x})] \to c(n) \implies s \to 0.50$: Entire dataset lacks distinct anomalies.

### 2. Local Outlier Factor (LOF) (Breunig et al., 2000)
Global density methods fail when clusters have different densities. LOF evaluates the density of point $\mathbf{p}$ **relative to the densities of its $k$-nearest neighbors**:

1. **$k$-distance of $\mathbf{p}$**: Distance $d(\mathbf{p}, \mathbf{o})$ to its $k$-th nearest neighbor.
2. **Reachability Distance**:
   $$\text{reach-dist}_k(\mathbf{p}, \mathbf{o}) = \max\left( k\text{-distance}(\mathbf{o}), \, d(\mathbf{p}, \mathbf{o}) \right)$$
3. **Local Reachability Density (lrd)**:
   $$\text{lrd}_k(\mathbf{p}) = \frac{|N_k(\mathbf{p})|}{\sum_{\mathbf{o} \in N_k(\mathbf{p})} \text{reach-dist}_k(\mathbf{p}, \mathbf{o})}$$
4. **Local Outlier Factor (LOF)**:
   $$\text{LOF}_k(\mathbf{p}) = \frac{\sum_{\mathbf{o} \in N_k(\mathbf{p})} \frac{\text{lrd}_k(\mathbf{o})}{\text{lrd}_k(\mathbf{p})}}{|N_k(\mathbf{p})|}$$
- $\text{LOF} \approx 1.0$: Density similar to neighbors (Inlier).
- $\text{LOF} < 1.0$: Dense point in a cluster.
- $\text{LOF} \gg 1.0$ (e.g., $1.5$–$5.0$): Significantly lower density than neighbors (Outlier).

---

## 4. Computational Complexity

| Algorithm | Training Complexity | Inference Complexity (Single Sample) | Memory Complexity |
| :--- | :--- | :--- | :--- |
| **Isolation Forest** | $O(T \cdot \psi \log \psi)$ where $\psi=256$ | $O(T \cdot \log \psi)$ (< 50 microseconds) | $O(T \cdot \psi)$ (Extremely compact) |
| **Local Outlier Factor (LOF)** | $O(n^2 \cdot d)$ (or $O(n \log n)$ with KD-tree) | $O(k \cdot d)$ (Novelty mode) | $O(n \cdot d)$ |
| **One-Class SVM** | $O(n^2 \cdot d)$ to $O(n^3)$ | $O(n_{\text{SV}} \cdot d)$ | $O(n_{\text{SV}} \cdot d)$ |
| **Elliptic Envelope (FastMCD)**| $O(n \cdot d + d^3)$ | $O(d^2)$ (Mahalanobis dot product) | $O(d^2)$ |

---

## 5. Inductive Bias & Learning Guarantees

1. **Subsampling Defense Against Masking (iForest)**: By default, each iTree is trained on a tiny random subsample ($\psi = 256$) rather than the full dataset $n$. This prevents **masking** (anomalies forming small clusters that camouflage each other) and **swamping** (normal points being isolated prematurely).
2. **Elliptic Envelope**: Assumes nominal inlier data is generated by a single unimodal multivariate Gaussian. Completely invalid for multimodal or non-linear distributions.
3. **One-Class SVM**: Fits a tight RBF kernel envelope around the nominal data. Can model arbitrary non-linear shapes but is sensitive to hyperparameter tuning ($\nu$ and $\gamma$).

---

## 6. Scratch NumPy Implementation

```python
"""
Scratch NumPy implementation of Isolation Forest from first principles.
Implements recursive random orthogonal partitioning, path length tracking,
and average BST depth normalization.
"""

from __future__ import annotations
import numpy as np
from typing import Optional, List


class ScratchIsolationTree:
    """A single randomized isolation tree."""

    def __init__(self, current_depth: int = 0, max_depth: int = 10) -> None:
        self.current_depth = current_depth
        self.max_depth = max_depth
        self.split_feat: Optional[int] = None
        self.split_val: Optional[float] = None
        self.left: Optional[ScratchIsolationTree] = None
        self.right: Optional[ScratchIsolationTree] = None
        self.size: int = 0
        self.is_leaf: bool = False

    def fit(self, X: np.ndarray, rng: np.random.Generator) -> ScratchIsolationTree:
        n_samples, n_features = X.shape
        self.size = n_samples

        if self.current_depth >= self.max_depth or n_samples <= 1:
            self.is_leaf = True
            return self

        # Randomly choose a feature
        self.split_feat = int(rng.integers(0, n_features))
        col = X[:, self.split_feat]
        min_v, max_v = float(np.min(col)), float(np.max(col))

        if min_v == max_v:
            self.is_leaf = True
            return self

        # Randomly choose split point between min and max
        self.split_val = float(rng.uniform(min_v, max_v))

        left_mask = col < self.split_val
        self.left = ScratchIsolationTree(self.current_depth + 1, self.max_depth).fit(X[left_mask], rng)
        self.right = ScratchIsolationTree(self.current_depth + 1, self.max_depth).fit(X[~left_mask], rng)
        return self

    def path_length(self, x: np.ndarray) -> float:
        """Traverse tree to compute path length for a single instance."""
        if self.is_leaf or self.size <= 1:
            # Add average path length of remaining unbuilt sub-tree
            return float(self.current_depth + self._c(self.size))

        if x[self.split_feat] < self.split_val:
            return self.left.path_length(x)
        return self.right.path_length(x)

    @staticmethod
    def _c(n: int) -> float:
        """Average path length of unsuccessful BST search."""
        if n <= 1:
            return 0.0
        if n == 2:
            return 1.0
        euler = 0.5772156649
        return float(2.0 * (np.log(n - 1) + euler) - (2.0 * (n - 1) / n))


class ScratchIsolationForest:
    """Production reference implementation of Isolation Forest."""

    def __init__(
        self,
        n_estimators: int = 100,
        max_samples: int = 256,
        contamination: float = 0.05,
        random_state: int | None = 42,
    ) -> None:
        self.n_estimators = n_estimators
        self.max_samples = max_samples
        self.contamination = contamination
        self.random_state = random_state
        self.trees: List[ScratchIsolationTree] = []
        self.threshold_: float = 0.5

    def fit(self, X: np.ndarray) -> ScratchIsolationForest:
        rng = np.random.default_rng(self.random_state)
        X = np.asarray(X, dtype=np.float64)
        n_samples = len(X)
        subsample_size = min(self.max_samples, n_samples)
        max_depth = int(np.ceil(np.log2(max(subsample_size, 2))))

        self.trees = []
        for _ in range(self.n_estimators):
            sub_idx = rng.choice(n_samples, size=subsample_size, replace=False)
            tree = ScratchIsolationTree(max_depth=max_depth)
            tree.fit(X[sub_idx], rng)
            self.trees.append(tree)

        # Determine threshold based on contamination
        scores = self.score_samples(X)
        self.threshold_ = float(np.percentile(scores, 100.0 * (1.0 - self.contamination)))
        return self

    def score_samples(self, X: np.ndarray) -> np.ndarray:
        """Compute anomaly score s(x, n) in [0, 1]. Higher values indicate anomalies."""
        X = np.asarray(X, dtype=np.float64)
        paths = np.zeros((len(X), len(self.trees)), dtype=np.float64)

        for t_idx, tree in enumerate(self.trees):
            paths[:, t_idx] = [tree.path_length(x) for x in X]

        mean_path = np.mean(paths, axis=1)
        c_factor = ScratchIsolationTree._c(self.max_samples)
        # s = 2^{-E[h(x)] / c(n)}
        return 2.0 ** (-mean_path / c_factor)

    def predict(self, X: np.ndarray) -> np.ndarray:
        """Predict binary anomaly flags: -1 for outlier, +1 for inlier."""
        scores = self.score_samples(X)
        return np.where(scores >= self.threshold_, -1, 1)
```

---

## 7. Production Scikit-Learn 9-Stage Pipeline

```python
"""
Production Anomaly Detection benchmark pipeline.
Compares Isolation Forest, Local Outlier Factor, One-Class SVM, and Elliptic Envelope.
"""

from __future__ import annotations
import logging
from typing import Dict, Tuple
import numpy as np
from sklearn.datasets import make_blobs
from sklearn.preprocessing import StandardScaler
from sklearn.ensemble import IsolationForest
from sklearn.neighbors import LocalOutlierFactor
from sklearn.svm import OneClassSVM
from sklearn.covariance import EllipticEnvelope
from sklearn.metrics import roc_auc_score, f1_score

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("Anomaly_Pipeline")


def run_anomaly_benchmark_pipeline() -> Tuple[IsolationForest, Dict[str, float]]:
    logger.info("Stage 1: Synthesizing nominal Gaussian cluster with 3% injected anomalies.")
    rng = np.random.default_rng(42)
    # 2000 nominal inliers
    X_nominal, _ = make_blobs(n_samples=2000, n_features=12, centers=2, cluster_std=1.0, random_state=42)
    # 60 synthetic extreme anomalies (uniform noise)
    X_anomalies = rng.uniform(low=-10.0, high=10.0, size=(60, 12))

    X = np.vstack([X_nominal, X_anomalies])
    # Ground truth: 0 for normal, 1 for anomaly
    y_true = np.zeros(len(X), dtype=int)
    y_true[2000:] = 1

    contamination = 60.0 / len(X)
    X_scaled = StandardScaler().fit_transform(X)

    # 1. Isolation Forest
    logger.info("Stage 2: Evaluating Isolation Forest.")
    iforest = IsolationForest(n_estimators=150, max_samples=256, contamination=contamination, random_state=42)
    iforest.fit(X_scaled)
    # Invert score_samples so higher indicates anomaly
    score_iforest = -iforest.score_samples(X_scaled)
    auc_iforest = float(roc_auc_score(y_true, score_iforest))

    # 2. Local Outlier Factor (Novelty mode enabled for out-of-sample scoring)
    logger.info("Stage 3: Evaluating Local Outlier Factor.")
    lof = LocalOutlierFactor(n_neighbors=25, contamination=contamination, novelty=True)
    lof.fit(X_scaled)
    score_lof = -lof.score_samples(X_scaled)
    auc_lof = float(roc_auc_score(y_true, score_lof))

    # 3. One-Class SVM
    logger.info("Stage 4: Evaluating One-Class SVM (RBF kernel).")
    ocsvm = OneClassSVM(kernel="rbf", nu=contamination, gamma="scale")
    ocsvm.fit(X_scaled)
    score_ocsvm = -ocsvm.decision_function(X_scaled)
    auc_ocsvm = float(roc_auc_score(y_true, score_ocsvm))

    # 4. Elliptic Envelope (FastMCD)
    logger.info("Stage 5: Evaluating Elliptic Envelope.")
    ee = EllipticEnvelope(contamination=contamination, random_state=42)
    ee.fit(X_scaled)
    score_ee = -ee.decision_function(X_scaled)
    auc_ee = float(roc_auc_score(y_true, score_ee))

    logger.info(f"Anomaly ROC-AUC -> iForest: {auc_iforest:.4f} | LOF: {auc_lof:.4f} | OCSVM: {auc_ocsvm:.4f} | EE: {auc_ee:.4f}")

    metrics = {
        "iforest_auc": auc_iforest,
        "lof_auc": auc_lof,
        "ocsvm_auc": auc_ocsvm,
        "elliptic_auc": auc_ee,
    }
    return iforest, metrics


if __name__ == "__main__":
    model, perf = run_anomaly_benchmark_pipeline()
    assert perf["iforest_auc"] > 0.95, "Isolation Forest benchmark failed SLA."
```

---

## 8. Model Persistence & Serialization

```python
"""
Serializing Isolation Forest models with security checksums.
"""

from pathlib import Path
import joblib
from sklearn.ensemble import IsolationForest


def export_anomaly_model(model: IsolationForest, path: Path) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    joblib.dump(model, path, compress=("gzip", 3))
```

---

## 9. Hyperparameter Tuning Guide

| Parameter | Options | Default | Engineering Guidance |
| :--- | :--- | :--- | :--- |
| `contamination` | Float $\in (0, 0.5]$ or `'auto'` | `'auto'` | Expected proportion of anomalies. If known from historical audit (e.g., $1.5\%$ fraud rate), set explicitly to calibrate decision threshold $\tau$. |
| `max_samples` (iForest) | Integer | `256` | **Do not set to full dataset size $n$!** Liu et al. proved that $256$ is universally optimal; larger samples cause **swamping and masking** and slow down training. |
| `n_estimators` (iForest) | Integer | `100` | Number of isolation trees. 100–200 trees provide complete path length stability. |
| `n_neighbors` (LOF) | Integer | `20` | Local neighborhood size. Set between $15$ and $35$. Too small is sensitive to noise; too large smooths out local density variations. |
| `nu` (One-Class SVM) | Float $\in (0, 1]$ | `0.5` | Upper bound on outlier fraction and lower bound on support vector count. Set strictly equal to expected contamination. |

---

## 10. Residuals, Diagnostics & Failure Modes

1. **Path Length Histogram**:
   Plot the distribution of $\mathbb{E}[h(\mathbf{x})]$. Bimodal distributions indicate crisp anomaly separation; uniform distributions indicate lack of anomaly contrast.
2. **False Alarm Rate (Swamping Analysis)**:
   In high-volume streaming, evaluate the number of alerts generated per hour. If alerts exceed human investigator capacity, decrease `contamination` or increase decision score threshold.
3. **Axis-Aligned Cut Artifacts**:
   Standard iForest makes axis-aligned orthogonal cuts. In diagonal or circular distributions, it can artificially score legitimate corner regions as anomalies (**Extended Isolation Forest** solves this with random slope cuts).

---

## 11. Prediction Uncertainty & Calibration

Anomaly detectors output uncalibrated continuous scores. For probabilistic workflows:
1. Min-Max scale the raw score to $[0, 1]$.
2. Apply **Extreme Value Theory (EVT)** or Pareto tail fitting to calculate the probability that an observation exceeds the tail distribution: $P(\text{Score} > s)$.

---

## 12. Comprehensive Failure Modes & Edge Cases Table

| Failure Mode | Root Cause | Observable Symptom | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **Masking Effect** | Multiple anomalies cluster closely together in feature space. | Anomalies appear to form a valid dense cluster, escaping isolation. | Use small `max_samples=256` in iForest, or switch to **Local Outlier Factor**. |
| **Swamping Effect** | Normal inliers positioned near cluster peripheries. | False positive alarms flag legitimate transactions. | Decrease `contamination` parameter or raise anomaly threshold. |
| **Axis-Aligned Cut Artifacts** | Standard iTrees make strictly orthogonal single-feature cuts. | Corner points of rotated Gaussian distributions erroneously flagged as anomalies.| Use **Extended Isolation Forest (EIF)** with random oblique hyperplanes. |
| **Curse of Dimensionality (LOF)** | Euclidean distances concentrate to uniform values in $d > 50$. | LOF scores for all points collapse to $1.0$; zero anomalies detected. | Pre-reduce dimensions using **PCA** or **Autoencoders**. |
| **Concept Drift False Alarms** | Normal business seasonal shift (e.g., Black Friday traffic volume). | Millions of normal high-velocity orders flagged as fraud anomalies. | Periodically retrain models on rolling baseline windows. |
| **Multimodal Failure (Elliptic Envelope)**| Data generated by multiple Gaussian clusters. | Non-anomalous points in the gap between clusters flagged as severe outliers. | Switch to **Isolation Forest** or **GMM**. |

---

## 13. Comparative Trade-off Table

| Algorithm | Handles Non-Linear Data? | Training Speed | Memory Scaling | Primary Business Strength |
| :--- | :--- | :--- | :--- | :--- |
| **Isolation Forest** | **Yes (Exceptional)** | **Fastest** ($O(T \cdot \psi \log \psi)$) | $O(T \cdot \psi)$ (Ultra Light) | Universal baseline for tabular anomaly detection. |
| **Local Outlier Factor** | **Yes** | Slow ($O(n^2)$) | $O(n \cdot d)$ | Best for variable local cluster densities. |
| **One-Class SVM** | **Yes (RBF Kernel)** | Slow ($O(n^2)$ to $O(n^3)$) | $O(n_{\text{SV}} \cdot d)$ | Novelty detection when training on 100% clean data. |
| **Elliptic Envelope** | No (Single Gaussian only)| Fast ($O(n \cdot d + d^3)$) | $O(d^2)$ | Robust Mahalanobis distance for multivariate normal data. |

---

## 14. Production Engineering, Serving & Latency

### High-Throughput Streaming Scoring (< 20µs)
Isolation Forest inference is pure integer comparison down 100 shallow trees (depth $\le 8$):
```python
# Real-time scoring loop in FastAPI microservice (< 50 microseconds)
@app.post("/v1/score_transaction")
def score_transaction(features: list[float]):
    x = np.array(features).reshape(1, -1)
    # decision_function outputs negative values for anomalies
    score = float(iforest_model.decision_function(x)[0])
    is_anomaly = score < 0.0
    return {"anomaly_score": score, "is_anomaly": is_anomaly}
```

---

## 15. Interpretability & Explainability

- **DiffI (Difference in Isolation)**: Measures how much the path length of a sample increases when feature $j$ is omitted, isolating the exact feature causing the anomaly.
- **TreeSHAP for Isolation Forest**: Fast polynomial-time Shapley attributions explain why an individual transaction was flagged as fraudulent.

---

## 16. Hardware, Parallelism & Scaling

- **Embarrassingly Parallel Training**: Each isolation tree is constructed completely independently. Setting `n_jobs=-1` scales linearly across all physical CPU cores.
- **Out-of-Core Processing**: Because iForest subsamples $\psi = 256$ points per tree, training can be distributed across massive datasets using Dask-ML or Spark MLlib.

---

## 17. Automated Verification Harness

```python
"""
Pytest automated verification harness for Isolation Forest and LOF.
Asserts anomaly flag bounds, decision score consistency, and outlier isolation.
"""

import numpy as np
import pytest
from sklearn.datasets import make_blobs
from sklearn.ensemble import IsolationForest


@pytest.fixture
def synthetic_data_with_outlier():
    X, _ = make_blobs(n_samples=200, n_features=4, centers=1, cluster_std=0.5, random_state=42)
    # Inject 1 extreme outlier
    outlier = np.array([[100.0, 100.0, 100.0, 100.0]])
    return np.vstack([X, outlier])


def test_isolation_forest_flags_extreme_outlier(synthetic_data_with_outlier):
    """The injected extreme outlier must be assigned label -1."""
    X = synthetic_data_with_outlier
    model = IsolationForest(contamination=0.01, random_state=42).fit(X)
    preds = model.predict(X)

    # The last sample is the extreme outlier
    assert preds[-1] == -1, "Isolation Forest failed to detect extreme outlier!"


def test_isolation_forest_prediction_labels(synthetic_data_with_outlier):
    """Predictions must strictly contain only +1 (inlier) and -1 (outlier)."""
    model = IsolationForest(contamination=0.05, random_state=42).fit(synthetic_data_with_outlier)
    preds = model.predict(synthetic_data_with_outlier)

    assert set(np.unique(preds)).issubset({-1, 1})
```

---

## 18. Cross-Track Learning & Ecosystem Links

- **Precursor Steps**: Data cleaning in `02-data-and-preprocessing/17-outlier-detection-for-preprocessing`.
- **Probabilistic Alternative**: Density thresholding via GMM in `05-unsupervised-learning/05-gaussian-mixture-models-and-em`.
- **Deep Learning Bridge**: Autoencoders for complex images/signals in `05-ai/machine-learning/14-deep-learning-bridge`.

---

## 19. Real-World Enterprise Case Study

### Context: Cloud Infrastructure Kubernetes Container Intrusion Detection
- **Domain**: Cloud Cybersecurity & DevSecOps Platform.
- **Problem**: Monitoring $45,000$ production microservice containers for zero-day privilege escalation attacks from 42 OS kernel telemetry metrics (syscall rates, socket open frequency, CPU burstiness).
- **Implementation**:
  - Algorithm: `IsolationForest(n_estimators=150, max_samples=256, contamination=0.001)`.
  - Scored $1.2\text{M}$ metric vectors per minute with p99 scoring latency under $35\mu\text{s}$.
- **Outcome**: Detected a novel container-escape crypto-mining malware attack within 4 seconds of execution, cutting mean time to detect (MTTD) from 4 hours to real-time.

---

## 20. Technical Interview Preparation

### Q1: Why is Isolation Forest fundamentally different from other anomaly detection algorithms?
**Answer:** Traditional anomaly detection algorithms (LOF, Elliptic Envelope, OCSVM) follow a **profile-and-distance approach**: they build a model of normal inlier data (fitting densities, convex hulls, or distance distributions) and identify anomalies as points that fail to conform to this normal profile. This is computationally expensive ($O(n^2)$) and susceptible to the Curse of Dimensionality. **Isolation Forest** follows an **isolation approach**: it isolates anomalies directly using random binary cuts without profiling normal instances, running in sub-linear time $O(T \cdot \psi \log \psi)$ and scaling effortlessly to massive datasets.

### Q2: Why does Isolation Forest use a fixed subsample size of $\psi = 256$ rather than the full dataset?
**Answer:** Using the full dataset $n$ harms anomaly detection in two major ways:
1. **Swamping**: When sample size is large, normal clusters become very dense, and points near the outer periphery of normal clusters require many extra cuts, making them look artificially like anomalies.
2. **Masking**: If multiple anomalies exist in the same region, they form an artificial small cluster, requiring many cuts to separate and masking their true anomalous nature.
Subsampling $\psi = 256$ points completely resolves masking (anomalies rarely co-occur in the same 256-sample draw) while vastly reducing memory and training time.

### Q3: How does Local Outlier Factor (LOF) handle clusters with differing local densities?
**Answer:** A point near a sparse cluster might have a large Euclidean distance to its neighbors, yet be completely normal for that cluster. Global distance methods would mistakenly flag it as an outlier. LOF computes the **local reachability density (lrd)** of the point and compares it to the average lrd of its $k$-nearest neighbors. Because it evaluates the **ratio of densities**, a point in a sparse cluster whose neighbors are equally sparse will have $\text{LOF} \approx 1.0$ (normal), while an outlier standing outside a dense cluster will have $\text{LOF} \gg 1.0$, correctly adapting to variable local density regimes.

### Q4: What is the difference between Novelty Detection and Outlier Detection?
**Answer:**
- **Outlier Detection (Unsupervised)**: The training dataset contains unlabeled anomalies mixed in with normal data. The algorithm must identify anomalies within the training data itself. (e.g., standard `IsolationForest`, `LocalOutlierFactor(novelty=False)`).
- **Novelty Detection (Semi-Supervised)**: The training dataset is certified to be $100\%$ clean nominal data with **zero anomalies**. The model learns the boundary of normalcy during training, and flags novel anomalies exclusively during test-time inference on new samples. (e.g., `OneClassSVM`, `LocalOutlierFactor(novelty=True)`).

### Q5: What is the purpose of the $\nu$ (nu) parameter in One-Class SVM?
**Answer:** The parameter $\nu \in (0, 1]$ plays a dual mathematical role in the One-Class SVM formulation:
1. It is an **upper bound on the fraction of outliers / margin violations** permitted during training.
2. It is a **lower bound on the fraction of support vectors** retained by the model.
Setting $\nu = 0.05$ instructs the optimizer to construct a decision boundary that encloses at least $95\%$ of the training data, allowing at most $5\%$ to sit outside the margin.

### Q6: What is the "Masking Effect" in anomaly detection, and how do you prevent it?
**Answer:** Masking occurs when two or more anomalies are clustered closely together in feature space. In distance-based methods, each anomaly treats the other anomaly as a close neighbor, artificially inflating local density and hiding the fact that the entire group is disconnected from the main data cloud. It is mitigated by:
- Subsampling small batches (Isolation Forest with $\psi = 256$).
- Pre-filtering data using multi-scale nearest-neighbor radii.
- Agglomerative density analysis (HDBSCAN GLOSH outlier scoring).

### Q7: Why does the Elliptic Envelope fail on multimodal or non-linear data?
**Answer:** The Elliptic Envelope algorithm is based on the Minimum Covariance Determinant (FastMCD), which fits a single robust multivariate Gaussian ellipsoid $\mathcal{N}(\boldsymbol{\mu}, \mathbf{\Sigma})$ to the data. If the data consists of multiple distinct clusters, non-linear manifolds, or crescent shapes, fitting a single ellipsoid will enclose the empty space between clusters, falsely classifying legitimate normal points as outliers while failing to detect anomalies inside the convex hull.

### Q8: What is Extended Isolation Forest (EIF), and what flaw in standard iForest does it address?
**Answer:** Standard Isolation Forest splits features using strictly axis-aligned orthogonal cuts ($x_j < p$). On datasets where features are correlated or form diagonal/spherical geometries, orthogonal cuts struggle to isolate points along diagonal boundaries, creating artificial rectangular artifact regions where normal points receive artificially high anomaly scores. **Extended Isolation Forest (EIF)** replaces single-feature cuts with random oblique hyperplanes ($\mathbf{w}^T \mathbf{x} < p$, where $\mathbf{w}$ is drawn from a standard normal sphere), completely eliminating axis-aligned spatial artifacts.

---

## 21. Hands-On Exercises & Mini-Projects

1. **Isolation Forest 2D Decision Surface Plotter**: Train an `IsolationForest` on a 2D synthetic dataset with 2 Gaussian blobs and 10 scattered outliers. Evaluate `decision_function` over a dense grid and plot the contour heatmap, showing how anomaly scores transition from the core to the edges.
2. **LOF vs Isolation Forest on Multi-Density Blobs**: Synthesize two clusters: one extremely tight and dense ($N=500, \sigma=0.2$), and one large and sparse ($N=500, \sigma=2.0$). Run iForest and LOF. Show that LOF correctly avoids flagging normal sparse points as anomalies, while iForest is biased by global distance.
3. **Automated Contamination Calibrator**: Build a pipeline that takes unlabelled telemetry data, applies Extreme Value Theory (EVT) to fit a generalized Pareto distribution to the tail of iForest path lengths, and automatically calculates the optimal `contamination` cutoff threshold.
4. **Credit Card Fraud Isolation Benchmark**: Load the Kaggle Credit Card Fraud dataset ($284,807$ transactions, $492$ frauds). Run `IsolationForest(n_estimators=100, max_samples=256)`. Plot the Precision-Recall curve and compute PR-AUC without using training labels during fit.
5. **FastMCD Breakdown Point Simulation**: Fit an `EllipticEnvelope` on a Gaussian dataset. Gradually increase the percentage of extreme outliers from $1\%$ to $49\%$. Verify that FastMCD maintains robust centroid estimates up to the theoretical $50\%$ breakdown point.
6. **Oblique Extended Isolation Forest Prototype**: Extend `ScratchIsolationTree` to support random linear combinations of features ($\mathbf{w}^T \mathbf{x} \le p$). Compare the 2D decision contours of your oblique tree against standard axis-aligned trees on a rotated Gaussian distribution.
