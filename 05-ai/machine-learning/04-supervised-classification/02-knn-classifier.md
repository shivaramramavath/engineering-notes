---
title: "K-Nearest Neighbors (KNN) Classifier: Majority Voting and Metrics"
description: "Non-parametric classification via localized neighborhood voting, Voronoi tessellations, distance metrics, probability estimation, and KD-Trees."
track: "machine-learning"
module: "04-supervised-classification"
topic: "KNN Classifier"
order: 2
format: "algorithm"
level: "beginner"
tags: ["knn", "knn-classifier", "majority-voting", "voronoi", "instance-based", "classification", "kd-tree"]
prerequisites: [
  "05-ai/machine-learning/01-math-foundations/01-linear-algebra-for-ml.md",
  "05-ai/machine-learning/01-math-foundations/08-curse-of-dimensionality.md",
  "05-ai/machine-learning/02-data-and-preprocessing/05-feature-scaling.md"
]
key_takeaways:
  - "KNN classification assigns query points to the majority class among the k-nearest neighbors in standardized feature space."
  - "When k=1, the feature space is partitioned into a Voronoi diagram, achieving 0% training error but maximal sensitivity to noise."
  - "Class posterior probabilities are directly estimated as empirical neighbor frequencies: P(y=c|x) = (1/k) * sum I(y_i = c)."
  - "Cover & Hart (1967) theorem proves that as N -> inf, the error of a 1-NN classifier is bounded by at most twice the optimal Bayes error rate."
---

# K-Nearest Neighbors (KNN) Classifier: Majority Voting and Metrics

## 1. Algorithm Card

```
+-----------------------------------------------------------------------------------+
|                              ALGORITHM SPECIFICATION                              |
+-----------------------------------------------------------------------------------+
| Paradigm:         Supervised Learning (Instance-Based / Non-Parametric)           |
| Family:           Localized Density Estimation / Majority Voting                  |
| Input Space:      X in R^{N x D} (MANDATORY: Strictly standardized)               |
| Output Space:     y in {0, 1, ..., C-1} (Discrete Categorical Classes)            |
| Probability:      P(y=c|x) = sum_{i in N_k(x)} w_i * I(y_i = c) / sum w_i         |
| Optimization:     None (Lazy Learner: stores training points during fit)          |
| Time Complexity:  Training: O(1) (Brute) or O(D * N log N) (KD-Tree / Ball-Tree)  |
|                   Inference: O(N D) (Brute) or O(D log N) (Tree, D < 20)          |
| Space Complexity: O(N * D) (Entire dataset stored in memory)                      |
| Key Assumptions:  Smooth local manifolds; metric distance correlates with class   |
| Invariances:      Strictly NOT scale-invariant; sensitive to redundant features   |
+-----------------------------------------------------------------------------------+
```

---

## 2. Architecture & Intuition

KNN Classification is grounded in the intuitive principle: *"Instances with similar attributes belong to the same class."*
To classify query point $\mathbf{x}_q$:
1. Compute distances to all training samples using metric $d(\mathbf{x}_q, \mathbf{x}_i)$.
2. Identify the index set of the $k$-nearest neighbors: $\mathcal{N}_k(\mathbf{x}_q)$.
3. Assign $\mathbf{x}_q$ to the class with the highest vote:

$$\hat{y}(\mathbf{x}_q) = \arg\max_{c \in \{0, \dots, C-1\}} \sum_{i \in \mathcal{N}_k(\mathbf{x}_q)} w_i \cdot \mathbb{I}(y_i = c)$$

### Visual Mental Model: Voronoi Diagram ($k=1$)

When $k=1$, the decision boundary is the set of perpendicular bisectors between points of different classes, partitioning space into a **Voronoi Tessellation**. Each training sample controls its own polygonal cell:

```
          Feature 2
              ^
              |        +-----------------+
              |       /  Class A (o)     |
              |      /                   |   Class B (x)
              |     +---------+----------+
              |     |         |          |
              |     |  * x_q  |          |
              |     +---------+----------+
              |               |
              +----------------------------------> Feature 1
```

---

## 3. Theoretical Foundations & Mathematical Derivation

### 3.1 Probability Estimation

The posterior class probability is estimated directly from the neighborhood composition:

$$P(Y = c \mid \mathbf{x}_q) = \frac{\sum_{i \in \mathcal{N}_k(\mathbf{x}_q)} w_i \cdot \mathbb{I}(y_i = c)}{\sum_{i \in \mathcal{N}_k(\mathbf{x}_q)} w_i}$$

For uniform weighting ($w_i = 1$):
$$P(Y = c \mid \mathbf{x}_q) = \frac{k_c}{k}$$
where $k_c$ is the count of neighbors belonging to class $c$.

### 3.2 The Cover-Hart Theorem (1967)

Let $R^*$ be the theoretical **Bayes Error Rate** (the minimum achievable misclassification error under the true data-generating distribution).
Thomas Cover and Peter Hart proved that as sample size $N \to \infty$, the asymptotic error rate $R_{\text{1-NN}}$ of a 1-Nearest Neighbor classifier is bounded by:

$$R^* \le R_{\text{1-NN}} \le 2 R^* \left( 1 - R^* \right) \le 2 R^*$$

**Implication**: Even with zero knowledge of the underlying probability distributions and using only a single nearest neighbor, the asymptotic error rate is at most **twice the optimal Bayes error**.

### 3.3 Choice of $k$ and Tie-Breaking

- For binary classification ($C=2$), choosing an **odd $k$** (e.g., $k=3, 5, 7$) mathematically prevents tie votes.
- For multi-class classification ($C > 2$), ties can occur even with odd $k$.
  *Tie-breaking rules*:
  1. Pick the class with the smallest average distance to the query point.
  2. Fall back to class prior frequencies in the training set.

---

## 4. Computational Complexity Analysis

| Spatial Structure | Build Time | Inference Time ($D < 20$) | High Dimensions ($D > 50$) |
| :--- | :--- | :--- | :--- |
| **Brute Force** | $\mathcal{O}(1)$ | $\mathcal{O}(N \cdot D)$ | $\mathcal{O}(N \cdot D)$ |
| **KD-Tree** | $\mathcal{O}(D \cdot N \log N)$ | $\mathcal{O}(D \log N)$ | Collapses to $\mathcal{O}(N D)$ |
| **Ball-Tree** | $\mathcal{O}(D \cdot N \log N)$ | $\mathcal{O}(D \log N)$ | Outperforms KD-Tree |

---

## 5. Inductive Bias & Core Assumptions

1. **Local Compactness**: Assumes decision boundaries are smooth and continuous.
2. **Equal Feature Weighting**: Unweighted Euclidean distance assumes all features are equally relevant. If 95% of features are irrelevant noise, Euclidean distances concentrate, completely breaking neighbor rankings.

---

## 6. From-Scratch NumPy Implementation

```python
"""
Pure NumPy Implementation of K-Nearest Neighbors Classifier.
Tested on Python 3.11, NumPy 2.x.
"""

from typing import Tuple
import numpy as np


class PureNumPyKNNClassifier:
    """K-Nearest Neighbors Classifier supporting probability output."""

    def __init__(self, n_neighbors: int = 5, weights: str = "uniform"):
        self.n_neighbors = n_neighbors
        self.weights = weights
        self.X_train_: np.ndarray = np.array([])
        self.y_train_: np.ndarray = np.array([])
        self.classes_: np.ndarray = np.array([])

    def fit(self, X: np.ndarray, y: np.ndarray):
        assert X.ndim == 2, "X must be 2D array (N, D)."
        assert y.ndim == 1, "y must be 1D array (N,)."
        self.X_train_ = X.copy()
        self.y_train_ = y.copy()
        self.classes_ = np.unique(y)
        return self

    def predict_proba(self, X_test: np.ndarray) -> np.ndarray:
        N_test = len(X_test)
        n_classes = len(self.classes_)
        probabilities = np.zeros((N_test, n_classes))

        for i in range(N_test):
            query = X_test[i]
            distances = np.linalg.norm(self.X_train_ - query, axis=1)

            # Find k nearest neighbors
            k_indices = np.argsort(distances)[: self.n_neighbors]
            k_dists = distances[k_indices]
            k_labels = self.y_train_[k_indices]

            if self.weights == "uniform":
                for c_idx, c in enumerate(self.classes_):
                    probabilities[i, c_idx] = np.mean(k_labels == c)
            elif self.weights == "distance":
                weights = 1.0 / np.clip(k_dists, 1e-7, None)
                total_w = np.sum(weights)
                for c_idx, c in enumerate(self.classes_):
                    probabilities[i, c_idx] = np.sum(weights[k_labels == c]) / total_w

        return probabilities

    def predict(self, X_test: np.ndarray) -> np.ndarray:
        probs = self.predict_proba(X_test)
        best_indices = np.argmax(probs, axis=1)
        return self.classes_[best_indices]


# Verification Run
if __name__ == "__main__":
    np.random.seed(42)
    # 2D Clusters
    c0 = np.random.normal(loc=[-2.0, -2.0], scale=0.8, size=(50, 2))
    c1 = np.random.normal(loc=[+2.0, +2.0], scale=0.8, size=(50, 2))
    X = np.vstack([c0, c1])
    y = np.array([0] * 50 + [1] * 50)

    knn = PureNumPyKNNClassifier(n_neighbors=5, weights="distance")
    knn.fit(X, y)

    test_points = np.array([[-2.5, -2.5], [+2.5, +2.5]])
    preds = knn.predict(test_points)
    probs = knn.predict_proba(test_points)

    print("--- Pure NumPy KNN Classifier Output ---")
    print(f"Point [-2.5, -2.5] Predicted: {preds[0]} (Prob: {probs[0]})")
    print(f"Point [+2.5, +2.5] Predicted: {preds[1]} (Prob: {probs[1]})")

    assert preds[0] == 0 and preds[1] == 1
    assert probs[0, 0] > 0.90 and probs[1, 1] > 0.90
    print("[SUCCESS] Pure NumPy KNN Classifier verified.")
```

---

## 7. Production-Grade Scikit-Learn Implementation

```python
"""
Production Pipeline: KNeighborsClassifier with StandardScaler and GridSearchCV.
"""

import numpy as np
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.neighbors import KNeighborsClassifier
from sklearn.model_selection import GridSearchCV, train_test_split
from sklearn.metrics import classification_report, roc_auc_score


def run_production_knn_classifier():
    np.random.seed(42)
    N = 400
    X = np.random.randn(N, 4)
    # Circular non-linear boundary
    y = (X[:, 0]**2 + X[:, 1]**2 < 1.5).astype(int)

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.25, random_state=42)

    pipeline = Pipeline([
        ("scaler", StandardScaler()), # STRICTLY MANDATORY FOR KNN
        ("knn", KNeighborsClassifier())
    ])

    param_grid = {
        "knn__n_neighbors": [3, 5, 7, 11],
        "knn__weights": ["uniform", "distance"],
        "knn__metric": ["euclidean", "manhattan"]
    }

    grid = GridSearchCV(pipeline, param_grid=param_grid, cv=5, scoring="roc_auc")
    grid.fit(X_train, y_train)

    best_pipe = grid.best_estimator_
    print("--- Production KNN Classifier Tuning ---")
    print(f"Optimal Hyperparameters: {grid.best_params_}")

    y_probs = best_pipe.predict_proba(X_test)[:, 1]
    auc = roc_auc_score(y_test, y_probs)
    print(f"Test ROC-AUC: {auc:.4f}")
    assert auc > 0.85


if __name__ == "__main__":
    run_production_knn_classifier()
```

---

## 8. Model Persistence & Safe Serialization

```python
import joblib
import tempfile
import os

def persist_knn_classifier(pipeline):
    with tempfile.TemporaryDirectory() as tmp_dir:
        path = os.path.join(tmp_dir, "knn_clf.joblib")
        joblib.dump(pipeline, path)
        loaded = joblib.load(path)
        assert len(loaded.named_steps["knn"].classes_) == len(pipeline.named_steps["knn"].classes_)
        print("[SUCCESS] KNN Classifier persisted safely.")
```

---

## 9. Hyperparameter Tuning & Optimization

| Hyperparameter | Search Space | Default | Practical Guidance |
| :--- | :--- | :--- | :--- |
| `n_neighbors` | `[3, 5, 7, 11, 15]` | `5` | Odd numbers avoid ties in binary classification. |
| `weights` | `['uniform', 'distance']` | `'uniform'` | Distance weighting excels when class density varies. |
| `metric` | `['euclidean', 'manhattan', 'minkowski']` | `'minkowski'` | Manhattan ($p=1$) is preferred in moderate dimensions ($D > 10$). |

---

## 10. Diagnostic Evaluations & Boundary Analysis

```
+-----------------------------------------------------------------------------------+
|                        KNN CLASSIFIER DIAGNOSTICS                                 |
+-----------------------------------------------------------------------------------+
| 1. High k (e.g. k=50): Smooth linear boundary; ignores local minority clusters.   |
| 2. Low k (k=1): Islands of noise Voronoi cells form around training outliers.     |
| 3. Neighbor Distance Histogram: Checks if test samples lie within normal density. |
+-----------------------------------------------------------------------------------+
```

---

## 11. Calibration, Uncertainty & Prediction Intervals

KNN probability estimates are discrete steps ($0/k, 1/k, \dots, k/k$). For small $k$ (e.g., $k=5$), probabilities can only take values $\{0.0, 0.2, 0.4, 0.6, 0.8, 1.0\}$, which are poorly calibrated.
Apply **Platt Scaling** or **CalibratedClassifierCV** to smooth discrete neighbor counts into calibrated posterior probabilities.

---

## 12. Edge Cases, Failure Modes & Remediation

| Failure Mode | Root Cause | Observable Symptom | Engineering Remediation |
| :--- | :--- | :--- | :--- |
| **Class Imbalance Domination** | Majority class comprises 98% of data | Nearest neighbors are always majority; 0% minority recall | Use `weights='distance'` or balance training samples via undersampling. |
| **High Query Latency** | Scanning 500,000 samples per query | REST API latency $> 500\text{ ms}$ | Use Approximate Nearest Neighbors (Faiss/HNSW). |
| **Curse of Dimensionality** | Data has $D > 100$ features | Nearest neighbor distance $\approx$ average neighbor distance | Run PCA or feature selection prior to KNN. |
| **Scale Disparity** | Unscaled features | Large feature completely controls class prediction | Always prepend `StandardScaler()`. |
| **Memory Exhaustion** | Deploying large dataset in container | Microservice crashes due to RAM exhaustion | Switch to eager parametric model (Logistic Regression, Random Forest). |

---

## 13. Comparative Analysis: KNN vs Alternatives

| Dimension | KNN Classifier | Logistic Regression | Random Forest Classifier | Support Vector Machine |
| :--- | :--- | :--- | :--- | :--- |
| **Model Type** | Lazy Instance-based | Eager Parametric | Eager Ensemble | Eager Kernel-based |
| **Decision Boundary**| Highly Non-linear | Strictly Linear | Axis-aligned Steps | Smooth Non-linear |
| **Training Speed** | $\mathcal{O}(1)$ | Fast | Moderate | Slow ($\mathcal{O}(N^2)$) |
| **Inference Speed** | Slow | Ultra-Fast ($\sim \text{ns}$) | Fast | Moderate |

---

## 14. Production Engineering & Serving Considerations

1. **Approximate Nearest Neighbors (ANN)**:
   For enterprise recommendation and visual search systems with millions of embeddings, replace exact scikit-learn KNN with **FAISS (Facebook AI Similarity Search)** or **HNSW**, achieving 99% accuracy in $< 1\text{ ms}$.
2. **Batch Query Parallelism**:
   Query all test samples simultaneously via matrix-matrix multiplication ($X_{\text{test}} X_{\text{train}}^T$) to leverage BLAS level-3 hardware parallelism.

---

## 15. Interpretability & Feature Attribution

KNN offers direct **case-based justification**:
To explain why a loan was rejected, output the exact $k$ historical default records that were most similar to the applicant.

---

## 16. Hardware & Distributed Scaling

For large-scale vector similarity classification, use GPU tensor core acceleration via RAPIDS cuML `cuml.neighbors.KNeighborsClassifier`.

---

## 17. Verification & Diagnostics Test Harness

```python
import pytest
import numpy as np
from sklearn.neighbors import KNeighborsClassifier

def test_knn_memorization_k1():
    X = np.array([[1.0], [2.0], [3.0], [4.0]])
    y = np.array([0, 0, 1, 1])
    knn = KNeighborsClassifier(n_neighbors=1)
    knn.fit(X, y)
    assert np.all(knn.predict(X) == y)
```

---

## 18. Cross-Track Connections

- **05-ai/machine-learning/01-math-foundations/08-curse-of-dimensionality.md**: Distance metric concentration.
- **05-ai/machine-learning/03-supervised-regression/04-knn-regression.md**: KNN regression analog.
- **05-ai/machine-learning/05-unsupervised-learning/02-k-means.md**: Centroid-based neighborhood clustering.

---

## 19. Real-World Case Study: Clinical Pathology Biopsy Matching

A digital pathology startup matched optical biopsy images of skin lesions to a reference database of 20,000 verified melanoma cases.
- **Solution**:
  1. Extracted 64-dimensional feature embeddings from lesion images using a self-supervised visual backbone.
  2. Built a `KNeighborsClassifier(n_neighbors=7, weights='distance')` indexed via a Ball-Tree.
  3. Dermatologists received not only a melanoma probability score ($0.86$), but also direct visual access to the 7 nearest verified patient biopsy images from the historical archive, providing clinical trust.

---

## 20. Technical Interview Questions & Solutions

### Question 1: What is the Cover-Hart Theorem, and what are its implications for 1-NN classification?
**Answer**:
See Section 3.2. Thomas Cover and Peter Hart (1967) proved that under mild smoothness conditions, as the number of training samples $N \to \infty$, the probability of error $R_{\text{1-NN}}$ for a 1-Nearest Neighbor classifier satisfies:
$$R^* \le R_{\text{1-NN}} \le 2 R^* (1 - R^*) \le 2 R^*$$
where $R^*$ is the Bayes error rate (the theoretical minimum achievable error by an omniscient observer with access to the true class distributions).
This implies that half of the available information in an infinite dataset is contained in the single nearest neighbor.

### Question 2: Why should $k$ always be an odd number in binary classification?
**Answer**:
In binary classification with $C=2$ classes, if $k$ is even (e.g., $k=4$), the neighborhood can contain exactly 2 samples of Class 0 and 2 samples of Class 1. This results in a $50/50$ tie vote.
Choosing an odd number ($k=3, 5, 7$) guarantees that a strict majority class always exists, eliminating tie-breaking ambiguity.

### Question 3: How does class imbalance affect KNN, and how do you mitigate it?
**Answer**:
If Class 0 has 9,900 samples and Class 1 has 100 samples, the average Euclidean distance between Class 0 points is much smaller than between Class 1 points. For almost any query point, the $k$ nearest neighbors will be dominated by Class 0 purely due to higher sample density.
**Mitigations**:
1. Use distance-weighted voting: $w_i = \frac{1}{d(x_q, x_i)}$, heavily weighting rare points that are very close.
2. Undersample the majority class prior to indexing.
3. Scale votes by the inverse class prior: $w_i \cdot \frac{N}{N_c}$.

### Question 4: How can KNN estimate class posterior probabilities?
**Answer**:
By computing the proportion of neighbors belonging to class $c$:
$$P(Y = c \mid \mathbf{x}) = \frac{\sum_{i \in \mathcal{N}_k} \mathbb{I}(y_i = c)}{k}$$
Because these probabilities are discrete multiples of $1/k$, apply Platt scaling / calibration if continuous probabilities are required.

### Question 5: Why does KNN suffer from the Curse of Dimensionality?
**Answer**:
In high dimensions, the volume of feature space grows exponentially ($V \propto r^D$). To capture a fixed fraction (e.g., $10\%$) of data points, the radius of the hypercube neighborhood must be $r = 0.1^{1/D}$.
For $D=100$, $r = 0.1^{0.01} \approx 0.977$.
To capture just 10% of the data, the neighborhood must span $98\%$ of the entire feature range along every axis! The neighborhood is no longer "local"; it spans essentially the entire dataset.

### Question 6: What distance metric should be used if features are binary flags?
**Answer**:
Use **Hamming Distance** (proportion of discordant bits: $\frac{1}{D} \sum \mathbb{I}(u_j \neq v_j)$) or **Jaccard Distance** (for sparse binary features, ignoring co-absences: $1 - \frac{|A \cap B|}{|A \cup B|}$).

### Question 7: How do you choose the optimal $k$ without data leakage?
**Answer**:
Use $K$-Fold Cross-Validation inside an `sklearn.pipeline.Pipeline` with `StandardScaler` to evaluate classification accuracy or ROC-AUC across candidate values of $k \in [3, 25]$.

### Question 8: Can KNN be used for anomaly detection?
**Answer**:
Yes. The average distance to the $k$-nearest neighbors $D_k(\mathbf{x}) = \frac{1}{k} \sum_{i=1}^k d(\mathbf{x}, \mathbf{x}_{(i)})$ measures local sparsity. Points with large $D_k$ reside in empty regions of feature space and are flagged as anomalies.

---

## 21. Guided Practice & Hands-On Exercises

### Exercise 1: Voronoi Decision Boundary Visualizer
- Generate 20 random 2D points from two classes. Plot the Voronoi tessellation and color each cell by its 1-NN class assignment.

### Exercise 2: Distance Metric Shootout
- Compare classification accuracy of Euclidean ($p=2$), Manhattan ($p=1$), and Chebyshev ($p=\infty$) metrics across dimensions $D \in [2, 50]$.

### Exercise 3: Calibrated KNN Classifier
- Train a `KNeighborsClassifier(n_neighbors=5)` and calibrate its probabilities using `CalibratedClassifierCV(method='sigmoid')`. Plot the reliability diagram before and after calibration.

### Exercise 4: Distance-Weighted Majority Voter
- Implement a custom distance-weighted voting scheme in pure NumPy that applies exponential decay weights: $w_i = \exp(-d_i / \sigma)$.

### Exercise 5: Curse of Dimensionality Neighborhood Radius
- Compute the radius of a hypersphere needed to enclose the $k=5$ nearest neighbors as feature dimension $D$ increases from 2 to 100 on uniformly distributed points.

### Exercise 6: Approximate Nearest Neighbors with FAISS
- Index 50,000 synthetic embeddings using FAISS IndexFlatL2 and evaluate search speed vs scikit-learn's exact `KNeighborsClassifier`.
