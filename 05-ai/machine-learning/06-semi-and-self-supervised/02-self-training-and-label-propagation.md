# Self-Training, Label Propagation & Label Spreading

> **Track:** `05-ai/machine-learning` | **Module:** `06-semi-and-self-supervised` | **Status:** `Production Standard`  
> **Metadata:** `scikit-learn: >=1.4.0` | `NumPy: >=1.26.0` | `SciPy: >=1.12.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Self-Training (Pseudo-Labeling), Label Propagation & Label Spreading |
| **Family** | Semi-Supervised Classification / Graph-Based Transductive Learning / Wrapper Methods |
| **Input Feature Types** | Partially labeled dataset $\mathbf{X} \in \mathbb{R}^{(n_l + n_u) \times d}$ with label vector $\mathbf{y} \in \{1, \dots, C, -1\}^{n_l + n_u}$, where $-1$ denotes an unlabeled instance. |
| **Target Output** | Complete label assignment vector $\hat{\mathbf{y}} \in \{1, \dots, C\}^{n_l + n_u}$ or class posterior distributions $\hat{\mathbf{P}} \in \Delta^{C-1}$. |
| **Optimization Target** | Label Propagation: Harmonic energy minimization over graph Laplacian with clamped labeled nodes: $\min_{\mathbf{f}_u} \sum_{i, j} W_{ij} (f_i - f_j)^2$. Label Spreading: Normalized Laplacian with soft clamping ($\alpha \in (0, 1)$). |
| **Key Metrics** | Transductive accuracy on $\mathcal{D}_U$, Brier score, Macro-F1, Pseudo-label acceptance rate, Confirmation bias drift. |
| **Key Hyperparameters** | **Self-Training**: `threshold` ($\tau \ge 0.85$), `criterion` (`'threshold'` vs `'k_best'`), `max_iter`. **Label Propagation/Spreading**: `kernel` (`'rbf'`, `'knn'`), `gamma` ($\gamma > 0$), `n_neighbors` ($k \approx 7$), `alpha` ($\alpha \in (0, 1)$ clamping factor). |
| **Inference Mechanism** | Transductive for graph algorithms (evaluates directly on graph nodes); Inductive for Self-Training (base estimator scores new unseen points). |
| **Primary Failure Modes** | **Confirmation bias cascade** in self-training (reinforcing misclassifications); **label swallowing** in label propagation when class imbalance exists; massive memory blowup on dense $N \times N$ graph affinity matrices. |

---

## 2. Architecture & Intuition

### The Two Paradigms of Semi-Supervised Classification
1. **Self-Training (Inductive Wrapper)**:
   - A supervised base classifier (e.g. Logistic Regression, GBDT, SVM) trains on labeled data $\mathcal{D}_L$.
   - It predicts class probabilities on unlabeled data $\mathcal{D}_U$.
   - High-confidence predictions ($p > \tau$) are converted to "pseudo-labels" and added to $\mathcal{D}_L$.
   - The model retrains on the expanded labeled set. The process repeats iteratively until convergence.
2. **Label Propagation & Spreading (Transductive Graph Diffusion)**:
   - Constructs a graph where every sample (labeled and unlabeled) is a node, and edge weights $W_{ij}$ represent pairwise geometric similarity.
   - Known labels act as sources of "heat" or "electric potential".
   - Labels diffuse across graph edges in parallel until the entire graph reaches a steady-state harmonic distribution.

```
       Self-Training Loop                                Label Propagation (Graph Diffusion)
       
       ┌─────────────────────┐                                     (Node 1: y=+1)
       │ Train Base Model on │                                         ● (Source)
       │    Labeled Set D_L  │                                        / \
       └──────────┬──────────┘                                   W_12/   \ W_13
                  │                                                 /     \
                  ▼                                                ●       ●
       ┌─────────────────────┐                                (Unlabeled) (Unlabeled)
       │ Predict on D_U; Pick│                                      \     /
       │ Confident (p > tau) │                                   W_24\   / W_34
       └──────────┬──────────┘                                        \ /
                  │                                                    ● (Sink)
                  ▼                                                (Node 4: y=-1)
       ┌─────────────────────┐
       │ Add Pseudo-Labels   │
       │ to D_L; Repeat      │
       └─────────────────────┘
```

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $N = n_l + n_u$ | $\mathbb{N}^+$ | Total sample count ($n_l$ labeled, $n_u$ unlabeled) |
| $\mathbf{X} = [\mathbf{X}_L; \mathbf{X}_U]$ | $\mathbb{R}^{N \times d}$ | Concatenated feature matrix |
| $\mathbf{Y}_L \in \{0, 1\}^{n_l \times C}$ | Binary matrix | One-hot ground truth label indicators for labeled nodes |
| $\mathbf{F} = [\mathbf{F}_L; \mathbf{F}_U]$ | $\mathbb{R}^{N \times C}$ | Continuous node soft label distribution |
| $\mathbf{W} \in \mathbb{R}^{N \times N}$ | Symmetric non-negative | Graph affinity matrix ($W_{ij} = \exp(-\gamma \|\mathbf{x}_i - \mathbf{x}_j\|^2)$) |
| $\mathbf{D} = \text{diag}(\sum_j W_{ij})$ | Diagonal matrix | Degree matrix |
| $\mathbf{P} = \mathbf{D}^{-1} \mathbf{W}$ | Row-stochastic matrix | Random walk transition probability matrix |
| $\mathbf{S} = \mathbf{D}^{-1/2} \mathbf{W} \mathbf{D}^{-1/2}$ | Symmetric normalized | Normalized graph affinity matrix |
| $\alpha \in (0, 1)$ | Real scalar | Label spreading retention / clamping factor |

### Label Propagation: The Harmonic Function Solution (Zhu & Ghahramani, 2002)
Partition the affinity and degree matrices into labeled ($L$) and unlabeled ($U$) blocks:

$$\mathbf{W} = \begin{bmatrix} \mathbf{W}_{LL} & \mathbf{W}_{LU} \\ \mathbf{W}_{UL} & \mathbf{W}_{UU} \end{bmatrix}, \qquad \mathbf{P} = \mathbf{D}^{-1}\mathbf{W} = \begin{bmatrix} \mathbf{P}_{LL} & \mathbf{P}_{LU} \\ \mathbf{P}_{UL} & \mathbf{P}_{UU} \end{bmatrix}$$

In pure Label Propagation, labeled nodes are strictly clamped to their true values: $\mathbf{F}_L = \mathbf{Y}_L$.
At each iteration, unlabeled nodes update as the weighted average of their neighbors:

$$\mathbf{F}_U^{(t+1)} = \mathbf{P}_{UU} \mathbf{F}_U^{(t)} + \mathbf{P}_{UL} \mathbf{Y}_L$$

Because the spectral radius $\rho(\mathbf{P}_{UU}) < 1$ (assuming every connected component has at least one labeled node), the Neumann series converges to a unique closed-form steady state:

$$\lim_{t \to \infty} \mathbf{F}_U^{(t)} = (\mathbf{I} - \mathbf{P}_{UU})^{-1} \mathbf{P}_{UL} \mathbf{Y}_L$$

Equivalently, expressed via unnormalized Graph Laplacian blocks ($\mathbf{L} = \mathbf{D} - \mathbf{W}$):

$$\mathbf{F}_U = (\mathbf{D}_{UU} - \mathbf{W}_{UU})^{-1} \mathbf{W}_{UL} \mathbf{Y}_L$$

### Label Spreading: Soft Clamping (Zhou et al., 2004)
Pure Label Propagation can overfit if labeled samples contain label noise. **Label Spreading** introduces a normalized Laplacian and allows labeled nodes to change:

$$\mathbf{F}^{(t+1)} = \alpha \mathbf{S} \mathbf{F}^{(t)} + (1 - \alpha) \mathbf{Y}$$

where $\mathbf{S} = \mathbf{D}^{-1/2} \mathbf{W} \mathbf{D}^{-1/2}$ and $\alpha \in (0, 1)$ controls how much a node absorbs from neighbors vs retaining its initial label. The closed-form analytical solution is:

$$\mathbf{F}^* = (1 - \alpha) (\mathbf{I} - \alpha \mathbf{S})^{-1} \mathbf{Y}$$

---

## 4. Computational Complexity

| Algorithm | Iterative Time Complexity | Exact Closed-Form Complexity | Memory Footprint |
| :--- | :--- | :--- | :--- |
| **Self-Training (Wrapper)** | $\mathcal{O}(T \cdot \mathcal{T}_{\text{base}}(N))$ | N/A (iterative heuristic) | $\mathcal{O}(N \cdot d)$ |
| **Label Propagation (Iterative)** | $\mathcal{O}(T \cdot \text{nnz}(\mathbf{W}) \cdot C)$ | N/A | $\mathcal{O}(N^2)$ dense / $\mathcal{O}(N \cdot k)$ sparse |
| **Label Propagation (Closed-Form)**| N/A | $\mathcal{O}(n_u^3)$ (matrix inversion) | $\mathcal{O}(n_u^2)$ |
| **Label Spreading (Normalized)** | $\mathcal{O}(T \cdot \text{nnz}(\mathbf{S}) \cdot C)$ | $\mathcal{O}(N^3)$ | $\mathcal{O}(N^2)$ dense / $\mathcal{O}(N \cdot k)$ sparse |

---

## 5. Inductive Bias & Statistical Assumptions

1. **Graph Homophily**: Points strongly connected by graph edges share identical class assignments.
2. **Harmonic Smoothness**: The function $f$ minimizes Dirichlet energy; there are no sharp discontinuities along dense connected subgraphs.
3. **Pseudo-Label Invariance**: Self-training assumes the base classifier's high-confidence predictions are statistically representative of the true label distribution.

---

## 6. From-Scratch Implementation (NumPy)

A pure NumPy implementation featuring both an Inductive Self-Training Classifier and Transductive Harmonic Label Propagation:

```python
"""
Pure NumPy implementations of Self-Training and Closed-Form Harmonic Label Propagation.
"""

from __future__ import annotations
import numpy as np
from scipy.spatial.distance import cdist

class PureNumPySelfTraining:
    """
    Inductive Self-Training Classifier wrapper using iterative thresholded pseudo-labeling.
    """
    def __init__(self, base_estimator, threshold: float = 0.90, max_iter: int = 15):
        self.base_estimator = base_estimator
        self.threshold = threshold
        self.max_iter = max_iter
        self.classes_: np.ndarray | None = None

    def fit(self, X: np.ndarray, y: np.ndarray) -> PureNumPySelfTraining:
        """
        y contains class labels {0, 1, ...} and -1 for unlabeled instances.
        """
        X = np.asarray(X, dtype=np.float64)
        y = np.copy(y)
        
        labeled_mask = (y != -1)
        self.classes_ = np.unique(y[labeled_mask])

        for iteration in range(self.max_iter):
            unlabeled_mask = (y == -1)
            n_unlabeled = np.sum(unlabeled_mask)
            if n_unlabeled == 0:
                break

            # 1. Fit base estimator on currently labeled pool
            X_curr = X[~unlabeled_mask]
            y_curr = y[~unlabeled_mask]
            self.base_estimator.fit(X_curr, y_curr)

            # 2. Predict class probabilities on remaining unlabeled pool
            X_u = X[unlabeled_mask]
            probs = self.base_estimator.predict_proba(X_u)
            max_probs = np.max(probs, axis=1)
            pred_classes = self.classes_[np.argmax(probs, axis=1)]

            # 3. Filter by confidence threshold
            confident_idx = np.where(max_probs >= self.threshold)[0]
            if len(confident_idx) == 0:
                # No new points exceed threshold; stop
                break

            # 4. Map back to global indices and assign pseudo-labels
            global_unlabeled_idx = np.where(unlabeled_mask)[0]
            adopted_global_idx = global_unlabeled_idx[confident_idx]
            y[adopted_global_idx] = pred_classes[confident_idx]

        # Final fit on complete expanded dataset
        X_final = X[y != -1]
        y_final = y[y != -1]
        self.base_estimator.fit(X_final, y_final)
        return self

    def predict(self, X: np.ndarray) -> np.ndarray:
        return self.base_estimator.predict(X)

    def predict_proba(self, X: np.ndarray) -> np.ndarray:
        return self.base_estimator.predict_proba(X)


class PureNumPyLabelPropagation:
    """
    Transductive Harmonic Function Label Propagation (Zhu & Ghahramani).
    Solves for steady-state soft labels via closed-form matrix inversion.
    """
    def __init__(self, gamma: float = 1.0):
        self.gamma = gamma
        self.F_: np.ndarray | None = None
        self.classes_: np.ndarray | None = None

    def fit(self, X: np.ndarray, y: np.ndarray) -> PureNumPyLabelPropagation:
        X = np.asarray(X, dtype=np.float64)
        y = np.asarray(y)
        N = len(X)
        
        labeled_mask = (y != -1)
        unlabeled_mask = ~labeled_mask
        
        self.classes_ = np.unique(y[labeled_mask])
        C = len(self.classes_)
        class_to_idx = {c: idx for idx, c in enumerate(self.classes_)}

        # Construct One-Hot Ground Truth for labeled nodes: (n_l, C)
        n_l = np.sum(labeled_mask)
        n_u = np.sum(unlabeled_mask)
        
        Y_L = np.zeros((n_l, C), dtype=np.float64)
        for i, label in enumerate(y[labeled_mask]):
            Y_L[i, class_to_idx[label]] = 1.0

        # Construct RBF Affinity Matrix W: (N, N)
        sq_dist = cdist(X, X, metric="sqeuclidean")
        W = np.exp(-self.gamma * sq_dist)
        np.fill_diagonal(W, 0.0) # Zero self-loops

        # Degree Matrix
        d = np.sum(W, axis=1)
        D = np.diag(d)

        # Reorder into Labeled and Unlabeled blocks
        L_idx = np.where(labeled_mask)[0]
        U_idx = np.where(unlabeled_mask)[0]

        W_UU = W[np.ix_(U_idx, U_idx)]
        W_UL = W[np.ix_(U_idx, L_idx)]
        D_UU = np.diag(d[U_idx])

        # Closed-form Harmonic Solution:
        # F_U = (D_UU - W_UU)^-1 * (W_UL * Y_L)
        A = D_UU - W_UU
        B = W_UL @ Y_L
        
        # Add tiny ridge for numerical stability in singular components
        A += np.eye(n_u) * 1e-9
        F_U = np.linalg.solve(A, B)

        # Normalize rows to valid probability simplex
        row_sums = np.sum(F_U, axis=1, keepdims=True)
        row_sums[row_sums == 0] = 1.0
        F_U /= row_sums

        # Assemble full probability matrix
        self.F_ = np.zeros((N, C), dtype=np.float64)
        self.F_[L_idx] = Y_L
        self.F_[U_idx] = F_U
        return self

    def predict(self) -> np.ndarray:
        """Returns transductive predictions for all N nodes."""
        if self.F_ is None:
            raise RuntimeError("Model must be fitted first.")
        return self.classes_[np.argmax(self.F_, axis=1)]
```

---

## 7. Production Scikit-Learn Pipeline

```python
"""
Production Pipeline: Semi-Supervised Self-Training with GBDT base estimator.
"""

import numpy as np
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.ensemble import HistGradientBoostingClassifier
from sklearn.semi_supervised import SelfTrainingClassifier, LabelSpreading

def create_selftraining_pipeline(confidence_threshold: float = 0.90) -> Pipeline:
    """
    Constructs an enterprise-grade SelfTrainingClassifier with HistGradientBoosting.
    """
    base_model = HistGradientBoostingClassifier(
        max_iter=50,
        random_state=42,
        early_stopping=True
    )
    
    return Pipeline([
        ("scaler", StandardScaler()),
        ("self_training", SelfTrainingClassifier(
            base_estimator=base_model,
            criterion="threshold",
            threshold=confidence_threshold,
            max_iter=10,
            verbose=False
        ))
    ])

def create_label_spreading_pipeline(gamma: float = 20.0, alpha: float = 0.8) -> Pipeline:
    """
    Constructs an enterprise-grade LabelSpreading pipeline with soft clamping.
    """
    return Pipeline([
        ("scaler", StandardScaler()),
        ("label_spreading", LabelSpreading(
            kernel="rbf",
            gamma=gamma,
            alpha=alpha,
            max_iter=50,
            n_jobs=-1
        ))
    ])
```

---

## 8. Model Persistence & SHA-256 Integrity

```python
import json
import hashlib
import joblib
from pathlib import Path
from typing import Any, Dict

def save_ssl_artifact(model: Any, out_path: Path, metadata: Dict[str, Any]) -> Path:
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    
    joblib.dump(model, out_path, compress=3)
    
    sha = hashlib.sha256()
    with open(out_path, "rb") as f:
        while chunk := f.read(65536):
            sha.update(chunk)
            
    sidecar = {
        "model_file": out_path.name,
        "sha256": sha.hexdigest(),
        "model_type": type(model).__name__,
        "metadata": metadata
    }
    with open(out_path.with_suffix(".json"), "w", encoding="utf-8") as f:
        json.dump(sidecar, f, indent=2)
        
    return out_path
```

---

## 9. Hyperparameter Tuning & Diagnostics

### Tuning Gamma and Alpha in Label Spreading
Tuning using transductive cross-validation over masked labeled points:

```python
from sklearn.model_selection import GridSearchCV
from sklearn.semi_supervised import LabelSpreading

def tune_label_spreading(X: np.ndarray, y_train_masked: np.ndarray) -> LabelSpreading:
    param_grid = {
        "gamma": [0.1, 1.0, 5.0, 20.0, 50.0],
        "alpha": [0.2, 0.5, 0.8, 0.95]
    }
    # Using default scoring (accuracy on known labeled samples)
    grid = GridSearchCV(LabelSpreading(kernel="rbf"), param_grid=param_grid, cv=3, n_jobs=-1)
    grid.fit(X, y_train_masked)
    return grid.best_estimator_
```

---

## 10. Residuals, Diagnostics & Validation

Track the **Pseudo-Label Acceptance Rate** and **Entropy Decay** across self-training epochs:

```python
def inspect_selftraining_convergence(st_model: SelfTrainingClassifier) -> dict:
    """
    Extracts iterations run and final labeled count from Scikit-Learn SelfTrainingClassifier.
    """
    history = st_model.labeled_iter_
    n_total = len(history)
    n_initially_labeled = np.sum(history == 0)
    n_final_labeled = np.sum(history >= 0)
    
    return {
        "total_samples": n_total,
        "initial_labeled": int(n_initially_labeled),
        "pseudo_labeled_added": int(n_final_labeled - n_initially_labeled),
        "remaining_unlabeled": int(np.sum(history == -1)),
        "iterations_completed": int(st_model.n_iter_)
    }
```

---

## 11. Uncertainty Estimation & Confidence Intervals

In Label Spreading, uncertainty for unlabeled node $u$ is directly quantified by the normalized Shannon entropy of its continuous soft label vector $\mathbf{F}_u^* \in \Delta^{C-1}$:

$$\mathcal{H}(u) = -\sum_{c=1}^C F_{uc}^* \ln F_{uc}^*$$

Nodes with $\mathcal{H}(u) \approx \ln C$ reside equidistant from competing label sources along the graph manifold.

---

## 12. Failure Modes & Edge Cases

| Failure Mode | Root Cause | Behavioral Symptom | Detection Strategy | Mitigation |
| :--- | :--- | :--- | :--- | :--- |
| **Error Propagation Cascade** | Low self-training threshold ($\tau < 0.70$) allows erroneous pseudo-labels into train. | Model accuracy drops monotonically on validation set with each epoch. | Validation loss increases during self-training loop. | Enforce high threshold ($\tau \ge 0.95$) or Curriculum Pseudo-Labeling (gradually lower $\tau$). |
| **Class Swallowing (Label Imbalance)**| Majority class has more seeds in $\mathcal{D}_L$; random walks hit majority seeds with higher probability. | Minority class vanishes entirely from pseudo-labels; 100% predictions to majority. | Track pseudo-label class frequencies against expected prior. | Class-balanced self-training (adopt top-$K$ confident samples per class rather than fixed global threshold). |
| **Graph Disconnection** | RBF kernel bandwidth $\gamma$ set too large; distant points receive affinity $W_{ij} \approx 0$. | Matrix $(D_{UU} - W_{UU})$ becomes singular; isolated nodes remain unassigned. | Check number of connected components in graph ($> 1$). | Decrease $\gamma$ or switch to $k$-NN graph with guaranteed connectivity ($k \ge 5$). |
| **Over-smoothing ($\alpha \to 1$)** | Soft clamping parameter set to extreme limit; graph diffusion washes out labeled signals. | Every node converges to identical uniform distribution. | Soft predictions $\mathbf{F}^*$ have zero variance across nodes. | Restrict $\alpha \in [0.2, 0.8]$. |
| **Out-of-Memory Graph Build** | Constructing dense $(N \times N)$ affinity matrix for $N = 200,000$. | Process killed by OS (`OOM Killed`). | RAM consumption scales as $8 \cdot N^2$ bytes. | Use sparse $k$-NN graphs (`kernel='knn'`, `n_neighbors=10`). |

---

## 13. Comparative Trade-offs & When to Use

| Dimension | Self-Training (Wrapper) | Label Propagation | Label Spreading | Deep SSL (FixMatch / FlexMatch) |
| :--- | :--- | :--- | :--- | :--- |
| **Inductive vs Transductive**| **Inductive** (predicts new test data) | Transductive (fixed graph) | Transductive (fixed graph) | **Inductive** |
| **Base Classifier Freedom** | Any model (GBDT, SVM, Net) | Graph-only | Graph-only | Neural networks only |
| **Noise Robustness** | Moderate | Poor (hard clamped) | Good (soft clamped) | Excellent |
| **Scalability** | High ($\mathcal{O}(N \log N)$) | Poor without sparse graph | Poor without sparse graph | High (GPU batching) |
| **Optimal Use Case** | Tabular data with GBDT base | Small clean network graphs | Noisy small/medium graphs | Images, audio, NLP text |

---

## 14. Production Engineering & Latency SLAs

- **Serving Latency**: Self-Training produces a standard model (e.g. `HistGradientBoostingClassifier`) that evaluates single HTTP queries in $< 1\text{ ms}$.
- **Transductive Serving Workaround**: To classify a new query $\mathbf{x}_{\text{new}}$ with Label Spreading without rebuilding the entire graph:
  $$\mathbf{f}(\mathbf{x}_{\text{new}}) = \frac{\sum_{i=1}^N W(\mathbf{x}_{\text{new}}, \mathbf{x}_i) \mathbf{F}_i^*}{\sum_{i=1}^N W(\mathbf{x}_{\text{new}}, \mathbf{x}_i)}$$
This Nadaraya-Watson kernel regression evaluates in $\mathcal{O}(N \cdot d)$ time.

---

## 15. Interpretability & Feature Attribution

For Self-Training with a GBDT base estimator, use **TreeSHAP** directly on the final fitted model. For Label Propagation, explainability is structural: identify the $k$ labeled seed nodes that contributed the largest random walk transition probability to the target node.

---

## 16. Hardware Acceleration, Scaling & Distributed Notes

- **GraphBLAS / cuGraph**: For graphs with millions of nodes, compute sparse matrix-vector multiplications $\mathbf{S} \mathbf{F}$ using NVIDIA GPU graph libraries.
- **Faiss Graph Construction**: Build approximate $k$-NN graphs for 10M samples in $< 30\text{ seconds}$ using HNSW indices on GPU.

---

## 17. Pytest Verification Suite

```python
"""
Pytest validation suite for Self-Training and Label Propagation.
Save as: tests/test_self_training.py
Run: pytest -v tests/test_self_training.py
"""

import pytest
import numpy as np
from sklearn.datasets import make_classification
from sklearn.semi_supervised import SelfTrainingClassifier, LabelPropagation, LabelSpreading
from sklearn.linear_model import LogisticRegression

def test_selftraining_classifier_convergence():
    """Verify SelfTrainingClassifier runs and reduces the number of unlabeled instances."""
    X, y = make_classification(n_samples=100, n_features=4, random_state=42)
    # Mask 80% as unlabeled
    y_masked = np.copy(y)
    y_masked[20:] = -1
    
    st = SelfTrainingClassifier(base_estimator=LogisticRegression(), threshold=0.75)
    st.fit(X, y_masked)
    
    # Assert some pseudo-labels were added
    final_unlabeled = np.sum(st.labeled_iter_ == -1)
    assert final_unlabeled < 80

def test_label_spreading_preserves_labeled_classes():
    """Verify LabelSpreading correctly assigns true labels to clean seeds."""
    X = np.array([[0.0, 0.0], [0.1, 0.1], [10.0, 10.0], [10.1, 10.1]])
    y = np.array([0, -1, 1, -1])
    
    ls = LabelSpreading(kernel="rbf", gamma=1.0, alpha=0.5).fit(X, y)
    preds = ls.predict(X)
    
    assert preds[0] == 0
    assert preds[1] == 0  # Close neighbor of seed 0
    assert preds[2] == 1
    assert preds[3] == 1  # Close neighbor of seed 1
```

---

## 18. Cross-Track Integration

- **`03-backend/fastapi/16-ai-and-ml-serving`**: Serving inductive SelfTraining pipelines with zero-downtime hot reloading.
- **`09-sql`**: Storing confidence-scored pseudo-labels in staging tables for human-in-the-loop audit.
- **`05-ai/machine-learning/05-unsupervised-learning/06-spectral-clustering.md`**: Graph Laplacian foundations shared between Spectral Clustering and Label Propagation.

---

## 19. Real-World Enterprise Case Study

### Customer Support Intent Classification with 1% Labeled Seed Data
- **Context**: A SaaS enterprise handles 250,000 customer support tickets per week across 40 intent categories.
- **Problem**: Support agents had only hand-annotated 1,200 tickets ($\sim 30$ per class). Training a supervised text classifier yielded a poor Macro-F1 of $0.51$.
- **Solution Architecture**:
  1. Extracted 384-dimensional dense sentence embeddings from raw tickets using an open-source encoder.
  2. Applied `SelfTrainingClassifier` wrapping a calibrated Logistic Regression model with a strict threshold $\tau = 0.94$.
  3. Adopted 85,000 high-confidence pseudo-labeled tickets over 4 iterative epochs.
- **Outcome**: Macro-F1 increased from $0.51$ to $0.83$, saving \$420,000 in human annotation outsourcing costs.

---

## 20. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Prove that the steady-state solution of Label Propagation minimizes the graph Dirichlet energy with Dirichlet boundary conditions.</strong></summary>

**Answer:**  
In Label Propagation with clamped labels, the objective is to minimize the Dirichlet energy of the continuous label vector $\mathbf{f} \in \mathbb{R}^N$:
$$\min_{\mathbf{f}_U} \mathcal{E}(\mathbf{f}) = \frac{1}{2} \sum_{i, j=1}^N W_{ij} (f_i - f_j)^2 \quad \text{subject to } \mathbf{f}_L = \mathbf{y}_L$$
Using the Graph Laplacian $\mathbf{L} = \mathbf{D} - \mathbf{W}$:
$$\mathcal{E}(\mathbf{f}) = \mathbf{f}^T \mathbf{L} \mathbf{f} = \begin{bmatrix} \mathbf{f}_L^T & \mathbf{f}_U^T \end{bmatrix} \begin{bmatrix} \mathbf{L}_{LL} & \mathbf{L}_{LU} \\ \mathbf{L}_{UL} & \mathbf{L}_{UU} \end{bmatrix} \begin{bmatrix} \mathbf{f}_L \\ \mathbf{f}_U \end{bmatrix}$$
Expanding the quadratic form:
$$\mathcal{E}(\mathbf{f}) = \mathbf{f}_L^T \mathbf{L}_{LL} \mathbf{f}_L + 2 \mathbf{f}_U^T \mathbf{L}_{UL} \mathbf{f}_L + \mathbf{f}_U^T \mathbf{L}_{UU} \mathbf{f}_U$$
To find the minimum, take the gradient with respect to the unlabeled vector $\mathbf{f}_U$ and set to zero:
$$\nabla_{\mathbf{f}_U} \mathcal{E}(\mathbf{f}) = 2 \mathbf{L}_{UL} \mathbf{f}_L + 2 \mathbf{L}_{UU} \mathbf{f}_U = \mathbf{0}$$
$$\mathbf{L}_{UU} \mathbf{f}_U = -\mathbf{L}_{UL} \mathbf{f}_L$$
Since $\mathbf{L} = \mathbf{D} - \mathbf{W}$, for disjoint blocks: $\mathbf{L}_{UU} = \mathbf{D}_{UU} - \mathbf{W}_{UU}$, and $\mathbf{L}_{UL} = -\mathbf{W}_{UL}$.
Substituting gives:
$$(\mathbf{D}_{UU} - \mathbf{W}_{UU}) \mathbf{f}_U = \mathbf{W}_{UL} \mathbf{y}_L \implies \mathbf{f}_U = (\mathbf{D}_{UU} - \mathbf{W}_{UU})^{-1} \mathbf{W}_{UL} \mathbf{y}_L$$
This matches the exact steady-state harmonic function.
</details>

<details>
<summary><strong>Q2: Why does pure Label Propagation fail when labeled data contains label noise, and how does Label Spreading solve this?</strong></summary>

**Answer:**  
In pure Label Propagation (Zhu & Ghahramani), labeled nodes are strictly clamped: $\mathbf{F}_L^{(t)} = \mathbf{Y}_L$ for all $t$.
1. If a single sample in $\mathcal{D}_L$ is mislabeled (e.g. an outlier labeled positive that actually sits in a negative cluster), that clamped node acts as an infinite heat source. It will continuously force positive probability into all its neighboring nodes, corrupting the entire cluster.
2. **Label Spreading (Zhou et al.)** replaces hard clamping with soft clamping:
   $$\mathbf{F}^{(t+1)} = \alpha \mathbf{S} \mathbf{F}^{(t)} + (1 - \alpha) \mathbf{Y}$$
   Here, $\alpha \in (0, 1)$ governs the diffusion rate. Labeled nodes are permitted to update their predictions based on the consensus of their surrounding neighbors. If an isolated seed has label $+1$ but is surrounded by 50 neighbors voting $-1$, the diffusion process will overpower the noisy seed, allowing it to flip and self-correct.
</details>

<details>
<summary><strong>Q3: How does Curriculum Pseudo-Labeling improve upon standard fixed-threshold self-training?</strong></summary>

**Answer:**  
Standard self-training uses a single fixed threshold (e.g. $\tau = 0.95$) across all classes throughout all training epochs.
**Problems with fixed thresholding**:
1. Easy classes (distinct clusters) achieve $\ge 0.95$ probability immediately; hard classes never exceed $0.95$. Over time, the model only adopts pseudo-labels for easy classes, resulting in extreme class imbalance.
2. In early epochs, the model is poorly calibrated; any point exceeding $0.95$ may be a false overconfident hallucination.
**Curriculum Pseudo-Labeling (Cascante-Bonilla et al., 2021; FlexMatch)**:
1. Dynamically adjusts thresholds per class based on the model's learning status for that specific class: $\tau_c(t) = \tau \cdot \beta_c(t)$.
2. For hard classes with lower average confidence, $\tau_c$ is lowered proportionally to allow balanced pseudo-label adoption.
3. Gradually ramps up thresholds as global training progresses, ensuring balanced, risk-controlled pseudo-labeling.
</details>

<details>
<summary><strong>Q4: Explain how an absorbing Markov chain models Label Propagation.</strong></summary>

**Answer:**  
Consider a random walk on the graph where a particle hops between nodes with transition probability $P_{ij} = W_{ij} / D_{ii}$.
- Labeled nodes are designated as **absorbing states**: once the particle reaches a labeled node, it is absorbed and never leaves ($P_{ii} = 1$).
- Unlabeled nodes are **transient states**.
The transition matrix in canonical form is:
$$\mathbf{P} = \begin{bmatrix} \mathbf{I} & \mathbf{0} \\ \mathbf{R} & \mathbf{Q} \end{bmatrix}$$
where $\mathbf{R} = \mathbf{P}_{UL}$ (transitions from transient to absorbing) and $\mathbf{Q} = \mathbf{P}_{UU}$ (transitions between transient nodes).
The probability that a random walk starting at unlabeled node $u$ eventually gets absorbed by labeled node $l$ is given by the entry $(u, l)$ of the matrix:
$$\mathbf{B} = (\mathbf{I} - \mathbf{Q})^{-1} \mathbf{R} = (\mathbf{I} - \mathbf{P}_{UU})^{-1} \mathbf{P}_{UL}$$
Multiplying by the labels $\mathbf{Y}_L$ gives $\mathbf{f}_U = \mathbf{B} \mathbf{Y}_L$, proving that the predicted class probability for node $u$ is the probability that a random walk starting at $u$ hits a seed of that class before hitting any other seed.
</details>

<details>
<summary><strong>Q5: Why is Self-Training considered an "Inductive" wrapper method while Label Propagation is "Transductive"?</strong></summary>

**Answer:**  
- **Self-Training is Inductive**: It wraps an underlying parametric or non-parametric supervised classifier (like a Decision Tree, Logistic Regression, or Neural Network). The classifier learns an explicit decision surface $f: \mathbb{R}^d \to \mathcal{Y}$. When a completely new sample $\mathbf{x}_{\text{new}}$ arrives at inference time, it is evaluated directly through the trained decision boundary in $\mathcal{O}(d)$ time without needing the training set.
- **Label Propagation is Transductive**: It does not learn an analytical functional mapping $f(\mathbf{x})$. Instead, it computes an equilibrium vector $\mathbf{f}^* \in \mathbb{R}^N$ solely for the specific nodes present in the graph. If a new point $\mathbf{x}_{\text{new}}$ arrives, it cannot be classified directly; it must be added as a new node, and the entire graph must be re-solved.
</details>

<details>
<summary><strong>Q6: What causes "Label Swallowing" in graph-based semi-supervised learning and how do you prevent it?</strong></summary>

**Answer:**  
Label swallowing occurs when one class has substantially more labeled seeds (e.g. 100 positive seeds vs 5 negative seeds) or a denser sub-network.
Because random walk transition probabilities depend on path density, random walks from almost all unlabeled nodes have a much higher likelihood of hitting a majority seed first. The majority class "swallows" the entire graph, resulting in $100\%$ majority predictions.
**Prevention**:
1. **Class-Mass Normalization (Zhu et al.)**: Adjust the continuous predicted distributions $\mathbf{F}$ so that their column sums match the empirical class priors $p(y=c)$:
   $$\tilde{F}_{uc} = F_{uc} \cdot \frac{p(y=c)}{\sum_{i} F_{ic}}$$
2. **Degree-Weighted Priors**: Normalize seed initial voltages inversely proportional to class frequency.
</details>

<details>
<summary><strong>Q7: Contrast the convergence speed of Power Iteration vs Closed-Form Inversion in Label Propagation.</strong></summary>

**Answer:**  
- **Closed-Form Inversion**: Directly computes $\mathbf{f}_U = (\mathbf{D}_{UU} - \mathbf{W}_{UU})^{-1} \mathbf{W}_{UL} \mathbf{y}_L$. It requires $\mathcal{O}(n_u^3)$ operations and $\mathcal{O}(n_u^2)$ memory. While exact, it is completely intractable when $n_u > 20,000$.
- **Power Iteration (Jacobi Iteration)**: Iteratively executes $\mathbf{F}^{(t+1)} = \mathbf{P}_{UU} \mathbf{F}^{(t)} + \mathbf{P}_{UL} \mathbf{Y}_L$. If $\mathbf{W}$ is stored as a sparse matrix with average node degree $k \ll N$, each iteration requires only $\mathcal{O}(k \cdot n_u \cdot C)$ operations. It converges exponentially fast at rate $\mathcal{O}(\lambda_2^t)$ (where $\lambda_2$ is the second largest eigenvalue of $\mathbf{P}_{UU}$), reaching 4 decimal digits of precision in 20-40 iterations, scaling easily to millions of samples.
</details>

<details>
<summary><strong>Q8: How does the choice of graph construction method ($k$-NN graph vs RBF $\epsilon$-ball) affect Label Propagation?</strong></summary>

**Answer:**  
- **RBF / Fully-Connected $\epsilon$-Ball ($W_{ij} = \exp(-\gamma d_{ij}^2)$)**:
  - Produces a dense $N \times N$ matrix.
  - Highly sensitive to bandwidth $\gamma$: if $\gamma$ is too small, everything connects to everything (no clustering structure); if $\gamma$ is too large, the graph shatters into disconnected isolated nodes that cannot receive propagated labels.
- **$k$-Nearest Neighbors ($k$-NN Graph)**:
  - Connects each point only to its $k$ closest neighbors ($k \approx 5-15$).
  - Produces a sparse matrix with only $k \cdot N$ non-zero entries, reducing memory from $\mathcal{O}(N^2)$ to $\mathcal{O}(k N)$.
  - Adapts naturally to varying data density: sparse regions form longer edges while dense regions form short edges, ensuring the graph remains globally connected without shattering.
</details>

---

## 21. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Self-Training Pipeline with Logistic Regression</strong></summary>

**Task**: Create a partially labeled dataset using Scikit-Learn. Fit a `SelfTrainingClassifier` wrapping `LogisticRegression` and print the number of pseudo-labels added.

**Solution**:
```python
from sklearn.datasets import make_classification
from sklearn.semi_supervised import SelfTrainingClassifier
from sklearn.linear_model import LogisticRegression
import numpy as np

X, y = make_classification(n_samples=200, n_features=5, random_state=42)
y_masked = np.copy(y)
y_masked[30:] = -1 # 30 labeled, 170 unlabeled

st = SelfTrainingClassifier(base_estimator=LogisticRegression(), threshold=0.85)
st.fit(X, y_masked)

pseudo_added = np.sum((st.labeled_iter_ > 0))
print(f"Initial labeled samples: 30")
print(f"Pseudo-labeled samples added: {pseudo_added}")
print(f"Iterations completed: {st.n_iter_}")
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Classifying Two Moons with Label Propagation</strong></summary>

**Task**: Generate a Two Moons dataset with only 2 labeled points (1 positive, 1 negative). Classify all remaining points using `LabelPropagation` and evaluate accuracy.

**Solution**:
```python
from sklearn.datasets import make_moons
from sklearn.semi_supervised import LabelPropagation
from sklearn.metrics import accuracy_score
import numpy as np

X, y = make_moons(n_samples=300, noise=0.08, random_state=42)
y_masked = np.full(300, -1)

# Pick 1 seed for each class
idx_0 = np.where(y == 0)[0][0]
idx_1 = np.where(y == 1)[0][0]
y_masked[idx_0] = 0
y_masked[idx_1] = 1

lp = LabelPropagation(kernel="rbf", gamma=20.0).fit(X, y_masked)
preds = lp.predict(X)

print(f"Transductive Accuracy from only 2 labeled seeds: {accuracy_score(y, preds) * 100:.2f}%")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Analyzing the Effect of the Alpha Clamping Factor</strong></summary>

**Task**: Contaminate a labeled seed with incorrect noise. Compare predictions of `LabelPropagation` ($\alpha=1$, hard clamp) vs `LabelSpreading` ($\alpha=0.2$, soft clamp).

**Solution**:
```python
import numpy as np
from sklearn.semi_supervised import LabelPropagation, LabelSpreading

# 4 points in two distinct clusters
X = np.array([[0, 0], [0.1, 0.1], [10, 10], [10.1, 10.1]])
# Intentionally mislabel point 0 as class 1 (it sits next to point 1 which should be 0)
y = np.array([1, -1, 1, -1])

lp = LabelPropagation(kernel="rbf", gamma=1.0).fit(X, y)
ls = LabelSpreading(kernel="rbf", gamma=1.0, alpha=0.2).fit(X, y)

print("Label Propagation predictions (Hard Clamped):", lp.predict(X))
print("Label Spreading predictions   (Soft Clamped):", ls.predict(X))
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Iterative Power Iteration for Label Propagation</strong></summary>

**Task**: Implement power iteration diffusion for label propagation in pure NumPy and verify convergence against the closed-form matrix solution.

**Solution**:
```python
import numpy as np
from scipy.spatial.distance import cdist

# Setup 10 points
rng = np.random.default_rng(42)
X = rng.normal(0, 1, (10, 2))
y = np.array([0, 1, -1, -1, -1, -1, -1, -1, -1, -1])

# Affinity
W = np.exp(-cdist(X, X)**2)
np.fill_diagonal(W, 0.0)
P = W / np.sum(W, axis=1, keepdims=True)

# One-hot initialization
Y_init = np.zeros((10, 2))
Y_init[0, 0] = 1.0
Y_init[1, 1] = 1.0

# Power iteration
F = np.copy(Y_init)
for _ in range(50):
    F = P @ F
    # Clamp labeled
    F[0] = [1.0, 0.0]
    F[1] = [0.0, 1.0]

print("Converged soft probabilities for first 4 unlabeled nodes:")
print(np.round(F[2:6], 3))
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Class-Balanced Threshold Selection for Self-Training</strong></summary>

**Task**: Write a custom self-training loop that adopts the top-$K$ most confident pseudo-labels **per class** in each iteration to prevent majority class swallowing.

**Solution**:
```python
import numpy as np
from sklearn.linear_model import LogisticRegression

def class_balanced_self_training(X, y, k_per_class=5, max_iter=5):
    y = np.copy(y)
    classes = np.unique(y[y != -1])
    
    for iteration in range(max_iter):
        unlabeled = np.where(y == -1)[0]
        if len(unlabeled) == 0:
            break
            
        clf = LogisticRegression().fit(X[y != -1], y[y != -1])
        probs = clf.predict_proba(X[unlabeled])
        
        adopted_this_round = []
        for c_idx, c in enumerate(classes):
            c_probs = probs[:, c_idx]
            # Rank top-K within this class
            top_k_sub_idx = np.argsort(c_probs)[::-1][:k_per_class]
            
            # Filter by baseline confidence >= 0.70
            valid = top_k_sub_idx[c_probs[top_k_sub_idx] >= 0.70]
            for idx in valid:
                global_idx = unlabeled[idx]
                y[global_idx] = c
                adopted_this_round.append(global_idx)
                
        if len(adopted_this_round) == 0:
            break
            
    return y

# Verification
X_sim = np.random.normal(0, 1, (100, 3))
y_sim = np.full(100, -1)
y_sim[0:3] = 0
y_sim[3:6] = 1

y_final = class_balanced_self_training(X_sim, y_sim, k_per_class=4)
print(f"Total labeled count post-balanced-training: {np.sum(y_final != -1)}")
print(f"Class counts: Class 0 = {np.sum(y_final == 0)}, Class 1 = {np.sum(y_final == 1)}")
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Sparse Graph Label Spreading with Scipy</strong></summary>

**Task**: Implement memory-efficient Label Spreading on a $10,000$-sample dataset using a SciPy sparse $k$-NN graph without creating an $(N \times N)$ dense matrix.

**Solution**:
```python
import numpy as np
import scipy.sparse as sp
from sklearn.neighbors import kneighbors_graph

def sparse_label_spreading(X, y_masked, n_neighbors=10, alpha=0.8, max_iter=30):
    N = len(X)
    classes = np.unique(y_masked[y_masked != -1])
    C = len(classes)
    class_map = {c: i for i, c in enumerate(classes)}
    
    # 1. Sparse KNN adjacency (CSR)
    A = kneighbors_graph(X, n_neighbors=n_neighbors, mode="connectivity", include_self=False)
    # Symmetrize
    W = 0.5 * (A + A.T)
    
    # Degrees and normalized Laplacian S = D^-1/2 W D^-1/2
    degrees = np.array(W.sum(axis=1)).squeeze()
    deg_inv_sqrt = np.power(degrees, -0.5, where=degrees > 0)
    deg_inv_sqrt[degrees == 0] = 0.0
    D_inv_sqrt = sp.diags(deg_inv_sqrt)
    S = D_inv_sqrt @ W @ D_inv_sqrt
    
    # 2. One-hot initial label matrix
    Y = np.zeros((N, C), dtype=np.float32)
    for idx, label in enumerate(y_masked):
        if label != -1:
            Y[idx, class_map[label]] = 1.0
            
    # 3. Iterative sparse updates: F^(t+1) = alpha * S @ F^t + (1 - alpha) * Y
    F = np.copy(Y)
    for _ in range(max_iter):
        F = alpha * S.dot(F) + (1.0 - alpha) * Y
        
    return classes[np.argmax(F, axis=1)]

# Verification on synthetic data
X_large = np.random.normal(0, 1, (2000, 4))
y_large = np.full(2000, -1)
y_large[0:10] = 0
y_large[10:20] = 1

preds = sparse_label_spreading(X_large, y_large, n_neighbors=8)
print(f"Successfully ran sparse label spreading on {len(preds):,} nodes.")
```
</details>
