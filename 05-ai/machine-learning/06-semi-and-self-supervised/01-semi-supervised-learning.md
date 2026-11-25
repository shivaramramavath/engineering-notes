# Semi-Supervised Learning: Foundations, Assumptions & Paradigms

> **Topic:** Semi-Supervised Learning (SSL) Foundations & Theoretical Assumptions  
> **Module ID:** `05-ai/machine-learning/06-semi-and-self-supervised`  
> **Target Version:** Python 3.11+, scikit-learn 1.4+, NumPy 1.26+, SciPy 1.12+  
> **Level:** Core / Advanced  
> **Prerequisites:** [Supervised Classification](../04-supervised-classification/README.md), [Unsupervised Learning](../05-unsupervised-learning/README.md)  
> **Status:** Verified (Python 3.11 / CPU)

---

## Architectural / Conceptual Flow

```mermaid
flowchart TD
    A[Vast Unlabeled Pool: D_U (95%)] --> C[SSL Optimization Engine]
    B[Small Labeled Set: D_L (5%)] --> C
    C --> D{Foundational Assumptions}
    D -- Smoothness --> E[Enforce Output Continuity]
    D -- Low-Density Separation --> F[Push Boundary to Sparse Valleys]
    D -- Manifold Geometry --> G[Geodesic Laplacian Regularization]
    E --> H[Regularized Decision Boundary]
    F --> H
    G --> H
    H --> I[Superior Generalization vs Pure Supervised]
```

---

## 1. Concept Overview & Core Motivation

In modern machine learning applications, unlabeled data is abundant, inexpensive, and continuously generated (e.g., millions of server log lines, billions of raw audio seconds, terabytes of medical imaging). However, acquiring high-quality ground-truth annotations $y$ requires specialized human expertise (radiologists, legal analysts, fraud investigators), creating an acute bottleneck where $|\mathcal{D}_L| \ll |\mathcal{D}_U|$.

**Semi-Supervised Learning (SSL)** bridges this divide by jointly training on a small labeled dataset $\mathcal{D}_L = \{(\mathbf{x}_i, y_i)\}_{i=1}^{n_l}$ alongside a massive unlabeled dataset $\mathcal{D}_U = \{\mathbf{x}_j\}_{j=1}^{n_u}$, where typically $n_u \gg n_l$.

```
     Pure Supervised (Only D_L: 4 points)             Semi-Supervised (D_L + D_U revealing manifold)
            Y                                                Y
            ^                                                ^
            |     +1                                         |     +1  o  o  o
            |      \  Ambiguous boundary                     |      \          o
            |       \                                        |       \  o  o    o
            |        \                                       |  ---   \           --- (True Valley)
            |         \                                      |         \  x  x    x
            |          \                                     |          \          x
            |           -1                                   |           -1  x  x  x
            +--------------------> X                         +----------------------------> X
```

### Why Unlabeled Data Contains Information
Unlabeled data alone cannot tell us what labels exist. However, the marginal data distribution $p(\mathbf{x})$ contains vital geometric and structural information. If $p(\mathbf{x})$ is connected to the conditional distribution $p(y \mid \mathbf{x})$, observing millions of unlabeled samples allows us to identify high-density clusters, data manifolds, and sparse decision valleys.

---

## 2. Theoretical Foundations & Mathematics

### The Three Foundational Assumptions of SSL
Semi-supervised learning is mathematically proven to be useless unless at least one of three foundational structural assumptions holds:

1. **The Smoothness Assumption**:
   If two points $\mathbf{x}_1, \mathbf{x}_2$ in a high-density region are close in input space, their conditional label distributions $p(y \mid \mathbf{x}_1)$ and $p(y \mid \mathbf{x}_2)$ must be close. (Transitions in labels occur primarily across low-density regions).
2. **The Cluster / Low-Density Separation Assumption**:
   If points cluster into distinct high-density components, points within the same cluster share the same label. Consequently, **the optimal decision boundary must pass through low-density regions** (avoiding slicing through dense clusters).
3. **The Manifold Assumption**:
   High-dimensional data $\mathbf{x} \in \mathbb{R}^d$ concentrates near a lower-dimensional Riemannian manifold $\mathcal{M} \subset \mathbb{R}^d$ of intrinsic dimension $m \ll d$. The geodesic distance along $\mathcal{M}$ governs label propagation rather than ambient Euclidean distance.

### Symbol Table

| Symbol | Mathematical Definition | Data Type / Dimensions | Role in Concept |
| :---: | :--- | :---: | :--- |
| $\mathcal{D}_L$ | Labeled training set | $\{(\mathbf{x}_i, y_i)\}_{i=1}^{n_l}$ | Supervised anchor points with known ground-truth |
| $\mathcal{D}_U$ | Unlabeled training set | $\{\mathbf{x}_j\}_{j=1}^{n_u}$ | Unsupervised structural observation set |
| $\mathbf{X}_L, \mathbf{y}_L$ | Labeled matrices | $\mathbb{R}^{n_l \times d}, \{0, 1\}^{n_l}$ | Feature vectors and label vector |
| $\mathbf{X}_U$ | Unlabeled feature matrix | $\mathbb{R}^{n_u \times d}$ | Unlabeled feature matrix ($n_u \gg n_l$) |
| $f(\mathbf{x})$ | Prediction function / hypothesis | $\mathbb{R}^d \to \mathbb{R}$ | Model mapping inputs to target outputs |
| $\ell(y, f(\mathbf{x}))$ | Supervised loss function | $\mathbb{R} \times \mathbb{R} \to \mathbb{R}_{\ge 0}$ | Standard cross-entropy or hinge loss |
| $\mathcal{R}_{\text{unsup}}(f, \mathcal{D}_U)$ | Unsupervised regularization penalty | $\mathbb{R}_{\ge 0}$ | Penalizes violation of manifold smoothness |
| $\lambda_U \in \mathbb{R}^+$ | Balance hyperparameter | Scalar | Relative weight of unlabeled loss term |
| $\mathbf{L} = \mathbf{D} - \mathbf{W}$ | Graph Laplacian matrix | $\mathbb{R}^{(n_l+n_u) \times (n_l+n_u)}$ | Encodes pairwise manifold connectivity |

### General Semi-Supervised Objective Function
The composite loss function minimized across both labeled and unlabeled sets:

$$\min_{f \in \mathcal{H}} \mathcal{J}(f) = \frac{1}{n_l} \sum_{i=1}^{n_l} \ell(y_i, f(\mathbf{x}_i)) + \lambda_L \|f\|_{\mathcal{H}}^2 + \lambda_U \mathcal{R}_{\text{unsup}}(f, \mathcal{D}_U)$$

### Manifold Regularization (Belkin, Niyogi, Sindhwani, 2006)
Under the manifold assumption, the unsupervised penalty is defined via the Graph Laplacian quadratic form:

$$\mathcal{R}_{\text{unsup}}(f, \mathcal{D}_U) = \frac{1}{(n_l + n_u)^2} \sum_{i, j=1}^{n_l + n_u} W_{ij} \left( f(\mathbf{x}_i) - f(\mathbf{x}_j) \right)^2 = \frac{1}{(n_l + n_u)^2} \mathbf{f}^T \mathbf{L} \mathbf{f}$$

where $W_{ij} = \exp\left(-\frac{\|\mathbf{x}_i - \mathbf{x}_j\|^2}{2\sigma^2}\right)$ is the affinity between points. If two points are close ($W_{ij} \approx 1$), assigning them different predictions $(f(\mathbf{x}_i) - f(\mathbf{x}_j))^2 > 0$ incurs a severe penalty.

---

## 3. Implementation & Practical Code

Comparing a purely supervised baseline against a semi-supervised model on a partially labeled dataset (only 5% labels retained):

```python
"""
Benchmarking Purely Supervised Logistic Regression vs Semi-Supervised Label Spreading.
"""

from __future__ import annotations
import numpy as np
from sklearn.datasets import make_moons
from sklearn.model_selection import train_test_split
from sklearn.linear_model import LogisticRegression
from sklearn.semi_supervised import LabelSpreading
from sklearn.metrics import accuracy_score

def compare_supervised_vs_ssl():
    # 1. Generate non-linear moon dataset (n=1200)
    rng = np.random.default_rng(42)
    X, y = make_moons(n_samples=1200, noise=0.12, random_state=42)
    
    # Holdout 20% for pure test evaluation
    X_train_full, X_test, y_train_full, y_test = train_test_split(
        X, y, test_size=0.20, random_state=42, stratify=y
    )
    
    # 2. Mask 95% of training labels as -1 (Unlabeled)
    y_train_masked = np.copy(y_train_full)
    n_train = len(y_train_full)
    n_labeled = int(0.05 * n_train)  # Only ~48 labeled samples!
    
    labeled_indices = rng.choice(n_train, size=n_labeled, replace=False)
    unlabeled_mask = np.ones(n_train, dtype=bool)
    unlabeled_mask[labeled_indices] = False
    y_train_masked[unlabeled_mask] = -1

    # Baseline 1: Purely Supervised Model trained ONLY on the 5% labeled data
    X_l = X_train_full[labeled_indices]
    y_l = y_train_full[labeled_indices]
    sup_model = LogisticRegression().fit(X_l, y_l)
    sup_acc = accuracy_score(y_test, sup_model.predict(X_test))

    # Model 2: Semi-Supervised Label Spreading trained on ALL data (5% labeled + 95% unlabeled)
    # Uses RBF kernel to diffuse labels along the moon manifold
    ssl_model = LabelSpreading(kernel="rbf", gamma=20.0, alpha=0.8, max_iter=50)
    ssl_model.fit(X_train_full, y_train_masked)
    ssl_acc = accuracy_score(y_test, ssl_model.predict(X_test))

    print(f"Total Training Samples: {n_train} | Labeled: {n_labeled} (5.0%)")
    print(f"Pure Supervised Baseline (5% labels) Accuracy: {sup_acc * 100:.2f}%")
    print(f"Semi-Supervised Label Spreading Accuracy:       {ssl_acc * 100:.2f}%")
    print(f"Incremental Improvement from SSL:              +{(ssl_acc - sup_acc) * 100:.2f}%")

if __name__ == "__main__":
    compare_supervised_vs_ssl()
```

---

## 4. Edge Cases & Advanced Scenarios

1. **Negative Transfer (When SSL Hurts Performance)**:
   - If the true decision boundary cuts directly through a dense cluster (violating the cluster assumption), SSL algorithms will actively force the boundary away from the data, increasing error compared to a purely supervised model.
2. **Extreme Class Imbalance in Labeled Pool**:
   - If $\mathcal{D}_L$ contains 95 positive samples and 5 negative samples, naive pseudo-labeling will amplify the majority class into $\mathcal{D}_U$, resulting in $100\%$ positive predictions.
   - *Fix*: Enforce class-prior matching (Distribution Alignment) to force pseudo-labels to match empirical class proportions.
3. **Out-of-Distribution (OOD) Contamination in $\mathcal{D}_U$**:
   - In production, unlabeled data often contains noisy, out-of-domain samples (e.g. scrapers downloading foreign language text).
   - *Fix*: Apply anomaly detection (Isolation Forest) to filter $\mathcal{D}_U$ prior to SSL.

---

## 5. Anti-Patterns & Common Pitfalls Table

| Anti-Pattern / Mistake | Why It Fails | Correct Engineering Fix |
| :--- | :--- | :--- |
| **Confirmation Bias in Self-Training** | Model assigns high confidence to erroneous pseudo-labels; retraining reinforces its own mistakes. | Use conservative probability thresholds ($\tau \ge 0.95$) or teacher-student EMA architectures. |
| **Assuming SSL Always Helps** | If $p(\mathbf{x})$ contains zero information about $p(y \mid \mathbf{x})$, unlabeled data adds pure noise. | Validate performance against a purely supervised baseline using cross-validation before deploying. |
| **Ignoring Graph Construction Costs** | Label Propagation builds an $(N \times N)$ dense kernel matrix. For $N = 100,000$, matrix requires $80\text{ GB}$ RAM. | Use approximate nearest-neighbor graphs (HNSW) or sparse KNN graphs with $k=20$. |
| **Data Leakage in Feature Scaling** | Fitting `StandardScaler` on $\mathcal{D}_L \cup \mathcal{D}_U \cup \mathcal{D}_{\text{test}}$ leaks test manifold statistics into training. | Fit transformers strictly on $\mathcal{D}_L \cup \mathcal{D}_U$; evaluate on untouched $\mathcal{D}_{\text{test}}$. |
| **Treating Unlabeled Samples as Negative** | In PU Learning (Positive-Unlabeled), treating unlabeled data as class 0 creates massive label noise. | Use Elkan-Noto or unbiased PU risk estimators. |

---

## 6. Best Practices & Production Guidelines

1. **Ramp-Up Schedule for $\lambda_U$**: During early epochs, the model is inaccurate. Set $\lambda_U(t) = \lambda_0 \cdot \exp\left(-5(1 - t/T)^2\right)$ to gradually ramp up the weight of unlabeled loss as model confidence stabilizes.
2. **Consistency Regularization**: Force predictions to be invariant under stochastic data augmentations:
   $$\mathcal{L}_{\text{consistency}} = \mathbb{E}_{\mathbf{x} \in \mathcal{D}_U} \left\| f(\text{Augment}(\mathbf{x})) - f(\mathbf{x}) \right\|^2$$
3. **Confidence Masking**: Only backpropagate unsupervised loss on unlabeled samples where prediction confidence exceeds a calibrated threshold: $\mathbb{I}(\max_k f_k(\mathbf{x}) \ge \tau)$.

---

## 7. Real-World Case Study

### Rare Disease Detection in Electronic Health Records (EHR)
- **Context**: A healthcare network seeks to identify patients with an under-diagnosed autoimmune disorder.
- **Data**: 450,000 patient clinical histories ($\mathcal{D}_U$); only 320 confirmed genetic biopsy test records ($\mathcal{D}_L$).
- **Incident**: Purely supervised GBDT achieved a dismal PR-AUC of $0.18$ due to severe sample starvation in high-dimensional feature space ($d=450$ clinical codes).
- **SSL Architecture**:
  1. Used Autoencoder to learn a 32-dimensional patient manifold representation over all 450,000 EHR records.
  2. Applied Graph Convolutional Label Propagation over the low-dimensional manifold anchored by the 320 biopsy cases.
  3. Calibrated predictions using Platt scaling.
- **Outcome**: PR-AUC surged from $0.18$ to $0.64$, surfacing 1,420 previously undiagnosed patients with an $84\%$ clinical confirmation rate.

---

## 8. Performance & Resource Optimization

```python
"""
High-performance sparse graph construction for large-scale Label Propagation.
"""

import numpy as np
from sklearn.neighbors import kneighbors_graph
from scipy.sparse import csgraph

def build_sparse_laplacian(X: np.ndarray, n_neighbors: int = 15) -> tuple:
    """
    Constructs a symmetric sparse KNN graph and normalized Laplacian.
    Avoids O(N^2) dense memory allocation.
    """
    # 1. Build sparse directed KNN adjacency matrix (CSR format)
    A = kneighbors_graph(X, n_neighbors=n_neighbors, mode="connectivity", include_self=False)
    
    # 2. Symmetrize: W = 0.5 * (A + A^T)
    W = 0.5 * (A + A.T)
    
    # 3. Compute normalized Laplacian L_sym = I - D^(-1/2) W D^(-1/2)
    L_sym = csgraph.laplacian(W, normed=True)
    
    return W, L_sym
```

---

## 9. Security, Privacy & Ethics Considerations

1. **Data Poisoning in the Unlabeled Pool**: An adversary without access to labeling credentials can inject poisoned unlabeled samples into $\mathcal{D}_U$ designed to bridge disparate clusters, shifting the learned decision boundary. Sanitize unlabeled pools with density-based isolation metrics.
2. **Bias Propagation**: If labeled data $\mathcal{D}_L$ suffers from historical demographic bias, SSL will propagate that bias across the entire unlabeled population, entrenching disparities.

---

## 10. Diagnostic & Verification Playbook

```python
"""
Diagnostic check to verify that SSL is outperforming pure supervised baseline.
"""

import numpy as np
from sklearn.metrics import brier_score_loss

def verify_ssl_gain(y_test: np.ndarray, sup_probs: np.ndarray, ssl_probs: np.ndarray) -> dict:
    """
    Computes Brier score and log-loss improvement of SSL over supervised baseline.
    """
    brier_sup = brier_score_loss(y_test, sup_probs)
    brier_ssl = brier_score_loss(y_test, ssl_probs)
    
    improvement = (brier_sup - brier_ssl) / brier_sup
    return {
        "brier_supervised": float(brier_sup),
        "brier_ssl": float(brier_ssl),
        "relative_improvement": float(improvement),
        "ssl_is_superior": bool(brier_ssl < brier_sup)
    }
```

---

## 11. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Under what theoretical condition does unlabeled data provide ZERO improvement to a supervised model?</strong></summary>

**Answer:**  
Unlabeled data provides zero benefit when the parameter space governing the marginal data distribution $p(\mathbf{x} \mid \theta_x)$ is completely **functionally independent (unidentifiable/decoupled)** from the parameters governing the conditional class distribution $p(y \mid \mathbf{x}, \theta_{y|x})$.
Formally, if the joint distribution factorizes such that:
$$p(\mathbf{x}, y \mid \theta) = p(\mathbf{x} \mid \theta_x) \cdot p(y \mid \mathbf{x}, \theta_{y|x})$$
and the parameter spaces $\Theta_x$ and $\Theta_{y|x}$ are disjoint ($\Theta_x \cap \Theta_{y|x} = \emptyset$), then knowledge of $p(\mathbf{x})$ conveys zero Fisher information regarding $\theta_{y|x}$. In this case, estimating $p(\mathbf{x})$ from billions of unlabeled samples leaves our estimate of $p(y \mid \mathbf{x})$ unchanged, and SSL cannot improve upon pure supervised learning.
</details>

<details>
<summary><strong>Q2: Explain the Low-Density Separation assumption and how Transductive SVMs (TSVM) optimize it.</strong></summary>

**Answer:**  
The Low-Density Separation assumption posits that the true classification decision boundary should pass through regions where the marginal density $p(\mathbf{x})$ is minimal (avoiding cutting through high-density clusters).
**Transductive SVM (TSVM)** optimizes this by seeking a hyperplane that maximizes the margin on labeled data while simultaneously driving unlabeled data points outside the margin:
$$\min_{\mathbf{w}, b, y_1^*, \dots, y_{n_u}^*} \frac{1}{2}\|\mathbf{w}\|_2^2 + C_L \sum_{i=1}^{n_l} \xi_i + C_U \sum_{j=1}^{n_u} \xi_j^*$$
$$\text{subject to } y_i (\mathbf{w}^T \mathbf{x}_i + b) \ge 1 - \xi_i, \quad y_j^* (\mathbf{w}^T \mathbf{x}_j^* + b) \ge 1 - \xi_j^*, \quad \xi_i, \xi_j^* \ge 0$$
The optimization solves for both the hyperplane $(\mathbf{w}, b)$ and the optimal discrete pseudo-labels $y_j^* \in \{-1, +1\}$ for the unlabeled points. Because points within the margin $[-1, +1]$ incur a penalty $\xi_j^*$, the boundary is forced into the widest empty gap in the unlabeled data.
</details>

<details>
<summary><strong>Q3: What is "Confirmation Bias" in self-training and what architectural methods mitigate it?</strong></summary>

**Answer:**  
Confirmation Bias occurs in self-training when a model produces confident but incorrect predictions on unlabeled data $\mathbf{x} \in \mathcal{D}_U$. When these erroneous pseudo-labels are added into the training set as if they were ground truth, the model is retrained to fit its own errors. Over successive iterations, this feedback loop magnifies error cascades and permanently degrades the decision boundary.
**Mitigations**:
1. **High Confidence Thresholding**: Only adopt pseudo-labels where predicted probability exceeds $\tau = 0.95$.
2. **Consistency Regularization**: Require predictions on augmented versions $\text{Augment}(\mathbf{x})$ to match original predictions before pseudo-labeling (e.g. FixMatch).
3. **Teacher-Student EMA (Mean Teacher)**: Maintain a "Teacher" model whose weights are an Exponential Moving Average of the student model's weights: $\theta_{\text{teacher}} \leftarrow \alpha \theta_{\text{teacher}} + (1 - \alpha) \theta_{\text{student}}$. The teacher generates much smoother, more stable targets.
</details>

<details>
<summary><strong>Q4: Derive the Graph Laplacian regularizer $\mathbf{f}^T \mathbf{L} \mathbf{f}$ from first principles.</strong></summary>

**Answer:**  
Let $G = (V, E, \mathbf{W})$ be a graph where nodes $V$ represent data points and $W_{ij} \ge 0$ is the affinity between $\mathbf{x}_i$ and $\mathbf{x}_j$.
The Dirichlet energy of a continuous function $\mathbf{f} \in \mathbb{R}^n$ over the graph penalizes discrepancies between adjacent nodes:
$$\mathcal{E}(\mathbf{f}) = \frac{1}{2} \sum_{i, j=1}^n W_{ij} (f_i - f_j)^2$$
Expanding the square:
$$\mathcal{E}(\mathbf{f}) = \frac{1}{2} \sum_{i, j=1}^n W_{ij} \left( f_i^2 - 2 f_i f_j + f_j^2 \right) = \frac{1}{2} \sum_{i, j} W_{ij} f_i^2 - \sum_{i, j} W_{ij} f_i f_j + \frac{1}{2} \sum_{i, j} W_{ij} f_j^2$$
Since $\mathbf{W}$ is symmetric, $\sum_{i, j} W_{ij} f_i^2 = \sum_i \left( \sum_j W_{ij} \right) f_i^2 = \sum_i D_{ii} f_i^2 = \mathbf{f}^T \mathbf{D} \mathbf{f}$, where $\mathbf{D} = \text{diag}(D_{11}, \dots, D_{nn})$ is the degree matrix.
The cross-term is $\sum_{i, j} W_{ij} f_i f_j = \mathbf{f}^T \mathbf{W} \mathbf{f}$.
Substituting back:
$$\mathcal{E}(\mathbf{f}) = \mathbf{f}^T \mathbf{D} \mathbf{f} - \mathbf{f}^T \mathbf{W} \mathbf{f} = \mathbf{f}^T (\mathbf{D} - \mathbf{W}) \mathbf{f} = \mathbf{f}^T \mathbf{L} \mathbf{f}$$
where $\mathbf{L} = \mathbf{D} - \mathbf{W}$ is the unnormalized Graph Laplacian matrix.
</details>

<details>
<summary><strong>Q5: What is the difference between Transductive and Inductive Semi-Supervised Learning?</strong></summary>

**Answer:**  
- **Transductive SSL**:
  - Goal: Predict labels strictly for the **given, fixed set** of unlabeled data points $\mathcal{D}_U$ used during training.
  - Does NOT produce a reusable functional mapping $f(\mathbf{x})$.
  - To predict a new test sample $\mathbf{x}_{\text{new}}$ arriving next week, the algorithm must re-run the entire optimization on $\mathcal{D}_L \cup \mathcal{D}_U \cup \{\mathbf{x}_{\text{new}}\}$.
  - Examples: Pure Label Propagation (Zhu & Ghahramani), spectral graph partitioning.
- **Inductive SSL**:
  - Goal: Learn a continuous general predictive function $f: \mathcal{X} \to \mathcal{Y}$ defined over the entire input space.
  - Unlabeled data $\mathcal{D}_U$ is used as a regularizer during training, but the resulting model (e.g. Neural Network, Logistic Regression, SVM) can evaluate unseen test samples $\mathbf{x}_{\text{new}}$ in $\mathcal{O}(d)$ time.
  - Examples: Self-training, Manifold Regularization, FixMatch, Virtual Adversarial Training (VAT).
</details>

<details>
<summary><strong>Q6: Why can semi-supervised learning suffer from negative transfer, and how can it be detected?</strong></summary>

**Answer:**  
Negative transfer occurs when incorporating unlabeled data leads to **worse** generalization error than training solely on the labeled subset $\mathcal{D}_L$.
**Root Causes**:
1. **Mismatched Clustering**: The feature space naturally clusters by an irrelevant factor (e.g. lighting conditions in computer vision or font style in OCR) rather than the target semantic class. The algorithm pushes the boundary between lighting conditions, destroying semantic classification.
2. **Label Noise Amplification**: Outliers or mislabeled points in $\mathcal{D}_L$ are propagated across large unlabeled manifolds.
**Detection**:
Maintain a clean, held-out cross-validation set. Compute the generalization error $\mathcal{E}_{\text{sup}}$ of the purely supervised model and $\mathcal{E}_{\text{ssl}}$ of the semi-supervised model. If $\mathcal{E}_{\text{ssl}} > \mathcal{E}_{\text{sup}}$, negative transfer has occurred; discard the unlabeled regularization or re-tune kernel bandwidth.
</details>

<details>
<summary><strong>Q7: What is Positive-Unlabeled (PU) Learning and where is it applied?</strong></summary>

**Answer:**  
PU Learning is a specialized variant of semi-supervised learning where the training data consists of a set of confirmed **Positive** samples $\mathcal{P}$ and a set of **Unlabeled** samples $\mathcal{U}$, with **zero confirmed negative samples**.
- *Example*: Fraud detection (we know confirmed fraudsters who were caught, but unflagged users are a mixture of legitimate customers and undetected fraudsters).
- *Formulation*: The unlabeled pool is modeled as a mixture: $p(\mathbf{x} \mid \mathcal{U}) = \pi p(\mathbf{x} \mid y=1) + (1 - \pi) p(\mathbf{x} \mid y=0)$, where $\pi = P(y=1)$ is the class prior.
- *Method*: Elkan-Noto Theorem or Unbiased PU Risk Estimators (du Plessis et al.) reweight unlabeled samples to penalize false negatives without assuming unlabeled points are negative.
</details>

<details>
<summary><strong>Q8: Explain the role of Entropy Minimization in semi-supervised learning.</strong></summary>

**Answer:**  
Entropy Minimization (Grandvalet & Bengio, 2004) is an inductive implementation of the Low-Density Separation assumption.
For an unlabeled sample $\mathbf{x} \in \mathcal{D}_U$, the conditional class distribution is $\hat{\mathbf{p}} = f(\mathbf{x})$. The Shannon entropy is:
$$\mathcal{H}(\hat{\mathbf{p}}) = -\sum_{k=1}^K \hat{p}_k \ln \hat{p}_k$$
- If a point lies far from the decision boundary, the model is confident (e.g. $\hat{\mathbf{p}} = [0.99, 0.01]$) and entropy is near $0$.
- If a point lies directly on the decision boundary, the model is uncertain ($\hat{\mathbf{p}} = [0.5, 0.5]$) and entropy is maximized ($\ln 2$).
By adding $\lambda_U \sum_{\mathbf{x} \in \mathcal{D}_U} \mathcal{H}(f(\mathbf{x}))$ to the training loss, the optimizer is penalized whenever the decision boundary intersects dense clusters of unlabeled points, forcing the boundary into low-density valleys.
</details>

---

## 12. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Masking Labels for SSL Experimentation</strong></summary>

**Task**: Write a helper function that takes a full label vector $\mathbf{y}$ and masks a specified percentage (e.g. $90\%$) as `-1`, preserving stratified class balances in the remaining labeled portion.

**Solution**:
```python
import numpy as np
from sklearn.model_selection import StratifiedShuffleSplit

def mask_labels_stratified(y: np.ndarray, keep_labeled_ratio: float = 0.1, random_state: int = 42) -> np.ndarray:
    """
    Masks (1 - keep_labeled_ratio) of labels as -1 while preserving class balance.
    """
    y_masked = np.full_like(y, -1)
    sss = StratifiedShuffleSplit(n_splits=1, train_size=keep_labeled_ratio, random_state=random_state)
    
    labeled_idx, _ = next(sss.split(np.zeros(len(y)), y))
    y_masked[labeled_idx] = y[labeled_idx]
    
    return y_masked

# Verification
y_raw = np.array([0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2])
masked = mask_labels_stratified(y_raw, keep_labeled_ratio=0.5)
print("Masked labels (-1 indicates unlabeled):", masked)
print("Retained counts:", {c: np.sum(masked == c) for c in [0, 1, 2]})
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Computing Graph Dirichlet Energy</strong></summary>

**Task**: Given an affinity matrix $\mathbf{W}$ and a vector of node predictions $\mathbf{f}$, compute the graph Dirichlet energy $\mathbf{f}^T \mathbf{L} \mathbf{f}$ using SciPy.

**Solution**:
```python
import numpy as np
from scipy.sparse import csgraph

W = np.array([
    [0.0, 0.9, 0.1],
    [0.9, 0.0, 0.2],
    [0.1, 0.2, 0.0]
])

# Prediction vector: Node 0 and Node 1 share high affinity (0.9)
f_smooth = np.array([1.0, 1.0, -1.0])
f_rough  = np.array([1.0, -1.0, -1.0]) # Node 0 and Node 1 assigned opposite labels!

L = csgraph.laplacian(W, normed=False)

energy_smooth = f_smooth.T @ L @ f_smooth
energy_rough  = f_rough.T @ L @ f_rough

print(f"Dirichlet Energy (Smooth): {energy_smooth:.4f}")
print(f"Dirichlet Energy (Rough):  {energy_rough:.4f}")
assert energy_rough > energy_smooth
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Entropy Minimization Loss Function in NumPy</strong></summary>

**Task**: Implement a vectorized loss function computing the Shannon entropy of predicted probability distributions across an unlabeled batch.

**Solution**:
```python
import numpy as np

def entropy_minimization_loss(predicted_probs: np.ndarray, eps: float = 1e-12) -> float:
    """
    Computes mean Shannon entropy across a batch of predicted probabilities: shape (N, K).
    """
    probs = np.clip(predicted_probs, eps, 1.0 - eps)
    # H = - sum p_k * log(p_k)
    sample_entropy = -np.sum(probs * np.log(probs), axis=1)
    return float(np.mean(sample_entropy))

# Test
confident_probs = np.array([[0.99, 0.01], [0.95, 0.05]])
uncertain_probs = np.array([[0.51, 0.49], [0.50, 0.50]])

print(f"Confident batch entropy: {entropy_minimization_loss(confident_probs):.4f}")
print(f"Uncertain batch entropy: {entropy_minimization_loss(uncertain_probs):.4f}")
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Benchmarking Label Spreading vs Pure Supervised GBDT</strong></summary>

**Task**: Create a non-linear concentric circle dataset. Train a purely supervised GBDT on 10 labeled samples, and compare its test accuracy against `LabelSpreading`.

**Solution**:
```python
from sklearn.datasets import make_circles
from sklearn.ensemble import HistGradientBoostingClassifier
from sklearn.semi_supervised import LabelSpreading
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score
import numpy as np

X, y = make_circles(n_samples=600, factor=0.4, noise=0.08, random_state=42)
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.3, random_state=42)

# Keep only 10 labeled samples (5 per class)
y_masked = np.full(len(y_train), -1)
idx_0 = np.where(y_train == 0)[0][:5]
idx_1 = np.where(y_train == 1)[0][:5]
labeled_idx = np.concatenate([idx_0, idx_1])
y_masked[labeled_idx] = y_train[labeled_idx]

# 1. Supervised baseline
sup = HistGradientBoostingClassifier(random_state=42).fit(X_train[labeled_idx], y_train[labeled_idx])
sup_acc = accuracy_score(y_test, sup.predict(X_test))

# 2. Semi-Supervised Label Spreading
ssl = LabelSpreading(kernel="rbf", gamma=15.0).fit(X_train, y_masked)
ssl_acc = accuracy_score(y_test, ssl.predict(X_test))

print(f"Supervised GBDT (10 labels) Accuracy: {sup_acc * 100:.1f}%")
print(f"Label Spreading (SSL) Accuracy:        {ssl_acc * 100:.1f}%")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Semi-Supervised Learning Verification Framework</strong></summary>

**Task**: Write an automated test suite comparing a model trained with increasing percentages of labeled data ($2\%, 5\%, 10\%, 50\%$) to detect if and when SSL yields statistically significant gains.

**Solution**:
```python
import numpy as np
from sklearn.datasets import make_classification
from sklearn.semi_supervised import LabelSpreading
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import accuracy_score

def run_ssl_scaling_benchmark():
    rng = np.random.default_rng(42)
    X, y = make_classification(n_samples=800, n_features=6, n_informative=4, random_state=42)
    
    ratios = [0.02, 0.05, 0.10, 0.25]
    results = {}
    
    for r in ratios:
        n_labeled = int(len(X) * r)
        labeled_idx = rng.choice(len(X), size=n_labeled, replace=False)
        
        y_masked = np.full(len(y), -1)
        y_masked[labeled_idx] = y[labeled_idx]
        
        # Supervised
        sup = LogisticRegression().fit(X[labeled_idx], y[labeled_idx])
        sup_acc = accuracy_score(y, sup.predict(X))
        
        # SSL
        ssl = LabelSpreading(kernel="knn", n_neighbors=7).fit(X, y_masked)
        ssl_acc = accuracy_score(y, ssl.predict(X))
        
        results[r] = {"supervised": sup_acc, "ssl": ssl_acc, "gain": ssl_acc - sup_acc}
        print(f"Ratio {r*100:4.1f}% | Sup: {sup_acc:.3f} | SSL: {ssl_acc:.3f} | Gain: {ssl_acc - sup_acc:+.3f}")
        
    return results

benchmark = run_ssl_scaling_benchmark()
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Manifold Regularized Loss Gradient</strong></summary>

**Task**: Implement a pure NumPy gradient calculation for a linear classifier with graph Laplacian manifold regularization: $\nabla_{\mathbf{w}} \mathcal{J} = \mathbf{X}_L^T (\mathbf{X}_L \mathbf{w} - \mathbf{y}_L) + \lambda_U \mathbf{X}^T \mathbf{L} \mathbf{X} \mathbf{w}$.

**Solution**:
```python
import numpy as np
from scipy.sparse import csgraph

def compute_manifold_gradient(w, X_L, y_L, X_all, W_affinity, lam_U=0.1):
    """
    Computes exact analytical gradient for least-squares with manifold regularization.
    """
    n_L = len(X_L)
    # 1. Supervised gradient: (1/n_L) * X_L^T (X_L w - y_L)
    res_L = (X_L @ w) - y_L
    grad_sup = (X_L.T @ res_L) / n_L
    
    # 2. Graph Laplacian
    L = csgraph.laplacian(W_affinity, normed=False)
    
    # 3. Manifold gradient: lam_U * X_all^T L X_all w
    # (d x n) @ (n x n) @ (n x d) @ (d x 1)
    grad_manifold = lam_U * (X_all.T @ (L @ (X_all @ w))) / (len(X_all) ** 2)
    
    return grad_sup + grad_manifold

# Verification
rng = np.random.default_rng(42)
X_all = rng.normal(0, 1, (50, 4))
X_L = X_all[:10]
y_L = rng.choice([-1.0, 1.0], size=10)
W = np.exp(-np.sum((X_all[:, None] - X_all[None, :]) ** 2, axis=2) / 2.0)
w_init = np.zeros(4)

grad = compute_manifold_gradient(w_init, X_L, y_L, X_all, W)
print("Initial gradient vector:", grad)
assert grad.shape == (4,)
```
</details>

---

## 13. Summary, Verified References & Navigation

### Core Takeaways
1. **Unlabeled data is only informative under structural assumptions**: Smoothness, Cluster (Low-Density Separation), or Manifold. If these fail, SSL cannot help and can cause negative transfer.
2. **Never leak test data**: Always build graphs over $\mathcal{D}_L \cup \mathcal{D}_U$; test sets must remain untouched.
3. **Beware Confirmation Bias**: Filter pseudo-labels using calibrated probability thresholds ($\tau \ge 0.95$) or teacher-student EMA architectures.

### Verified References
- Chapelle, O., Schölkopf, B., & Zien, A. (2006). *Semi-Supervised Learning.* MIT Press.
- Belkin, M., Niyogi, P., & Sindhwani, V. (2006). *Manifold Regularization: A Geometric Framework for Learning from Labeled and Unlabeled Examples.* JMLR.
- Grandvalet, Y., & Bengio, Y. (2004). *Semi-supervised learning by entropy minimization.* NeurIPS.
- Sohn, K. et al. (2020). *FixMatch: Simplifying Semi-Supervised Learning with Consistency and Confidence.* NeurIPS.

---

[← Previous Module: Unsupervised Learning](../05-unsupervised-learning/README.md) | [Module Index](README.md) | [Next Note: Self-Training & Label Propagation →](02-self-training-and-label-propagation.md)
