---
title: "K-Nearest Neighbors (KNN) Regression"
description: "Non-parametric regression via localized neighborhood averaging, distance metrics, KD-Trees, Ball-Trees, and the curse of dimensionality."
track: "machine-learning"
module: "03-supervised-regression"
topic: "KNN Regression"
order: 4
format: "algorithm"
level: "beginner"
tags: ["knn", "k-nearest-neighbors", "non-parametric", "kd-tree", "ball-tree", "lazy-learning", "regression"]
prerequisites: [
  "05-ai/machine-learning/01-math-foundations/01-linear-algebra-for-ml.md",
  "05-ai/machine-learning/01-math-foundations/08-curse-of-dimensionality.md",
  "05-ai/machine-learning/02-data-and-preprocessing/05-feature-scaling.md"
]
key_takeaways:
  - "KNN is an instance-based, non-parametric 'lazy learner': training is O(1) storage, while inference requires searching neighbor distances across the training set."
  - "Predictions are computed as localized neighborhood averages (uniform or inverse-distance weighted: w_i = 1 / d(x, x_i))."
  - "Spatial indexing trees (KD-Tree, Ball-Tree) reduce search time to O(D log N) for low dimensions (D < 20), but collapse to brute force O(N D) in high dimensions."
  - "KNN fails catastrophically outside the bounding box of the training data: predictions flatline at the target value of the nearest edge sample."
---

# K-Nearest Neighbors (KNN) Regression

## 1. Algorithm Card

```
+-----------------------------------------------------------------------------------+
|                              ALGORITHM SPECIFICATION                              |
+-----------------------------------------------------------------------------------+
| Paradigm:         Supervised Learning (Instance-Based / Non-Parametric)           |
| Family:           Localized Kernel Smoothing / Neighborhood Averaging             |
| Input Space:      x in R^D (MANDATORY: Continuous, strictly standardized)         |
| Output Space:     y in R (Continuous Scalars)                                     |
| Objective:        Minimize localized MSE over k-nearest neighbors                 |
| Optimization:     None (Lazy learner: stores points during fit)                   |
| Time Complexity:  Training: O(1) (Brute) or O(D * N log N) (KD-Tree build)        |
|                   Inference: O(N D) (Brute) or O(D log N) (Tree, D < 20)          |
| Space Complexity: O(N * D) (Must keep all training data in memory)                |
| Key Assumptions:  Local smoothness: instances close in feature space have         |
|                   similar target values; all features have equal relevance        |
| Invariances:      Strictly NOT scale-invariant; sensitive to irrelevant features  |
+-----------------------------------------------------------------------------------+
```

---

## 2. Architecture & Intuition

Unlike linear models that fit a single global equation $\hat{y} = \mathbf{w}^T \mathbf{x} + b$, K-Nearest Neighbors (KNN) Regression makes no parametric assumptions about the functional form of the target.
To predict the target for a query point $\mathbf{x}_q$:
1. Search the stored training dataset to find the $k$ training points $\{\mathbf{x}_{(1)}, \dots, \mathbf{x}_{(k)}\}$ closest to $\mathbf{x}_q$ under distance metric $d(\mathbf{x}_q, \mathbf{x}_i)$.
2. Compute the prediction as the average (or distance-weighted average) of their targets $\{y_{(1)}, \dots, y_{(k)}\}$:

$$\hat{y}(\mathbf{x}_q) = \frac{\sum_{i=1}^k w_i y_{(i)}}{\sum_{i=1}^k w_i}$$

```
+-----------------------------------------------------------------------------------+
|                           KNN REGRESSION INTUITION                                |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|           Feature 2                                                               |
|               ^                                                                   |
|               |       (x2, y=14.2)                                                |
|               |            \                                                      |
|               |             *  <- Query Point x_q                                 |
|               |            / \    Prediction = (14.2 + 15.0 + 13.8) / 3 = 14.33   |
|               |   (x1, y=15.0)   (x3, y=13.8)                                     |
|               |                                                                   |
|               +----------------------------------------> Feature 1                |
+-----------------------------------------------------------------------------------+
```

---

## 3. Theoretical Foundations & Mathematical Derivation

### 3.1 Distance Metrics

The definition of "nearness" is governed by the metric $d(\mathbf{u}, \mathbf{v})$:

1. **Minkowski Distance ($L_p$ Norm)**:
   $$d(\mathbf{u}, \mathbf{v}) = \left( \sum_{j=1}^D |u_j - v_j|^p \right)^{1/p}$$
   - $p = 1$: Manhattan ($L_1$) distance (city-block, robust to extreme feature deviations).
   - $p = 2$: Euclidean ($L_2$) distance (standard shortest geometric path).
2. **Mahalanobis Distance (Covariance-Normalized)**:
   $$d_M(\mathbf{u}, \mathbf{v}) = \sqrt{(\mathbf{u} - \mathbf{v})^T \boldsymbol{\Sigma}^{-1} (\mathbf{u} - \mathbf{v})}$$
   Normalizes distances by the feature covariance matrix $\boldsymbol{\Sigma}$, making KNN invariant to linear feature correlations.

### 3.2 Weighting Functions

- **Uniform Weighting**:
  $$w_i = \frac{1}{k} \implies \hat{y} = \frac{1}{k} \sum_{i=1}^k y_{(i)}$$
- **Inverse Distance Weighting**:
  $$w_i = \frac{1}{d(\mathbf{x}_q, \mathbf{x}_{(i)}) + \epsilon}$$
  Closer neighbors exert higher gravitational pull on the predicted output.

### 3.3 Bias-Variance Trade-off in $k$

The hyperparameter $k$ directly controls the bias-variance trade-off:
- **$k = 1$**: Minimum Bias, Maximum Variance. The model memorizes training points, creating discontinuous Voronoi step boundaries. Training MSE is identically $0$, but the model severely overfits noise.
- **$k = N$**: Maximum Bias, Minimum Variance. The prediction for every query point becomes the global sample mean $\bar{y}$, completely underfitting the data.

Under localized regression theory, the optimal $k$ scales as:
$$k^* \propto N^{\frac{4}{D + 4}}$$
As dimension $D$ increases, the optimal neighborhood size approaches $N$, demonstrating the degradation of local estimation in high dimensions.

---

## 4. Computational Complexity Analysis

| Spatial Indexing Method | Training / Build Time | Query / Inference Time | Suitable Dimensionality |
| :--- | :--- | :--- | :--- |
| **Brute Force** | $\mathcal{O}(1)$ (Instant) | $\mathcal{O}(N \cdot D)$ (Full Scan) | Any $D$; ideal for small $N$ |
| **KD-Tree** | $\mathcal{O}(D \cdot N \log N)$ | $\mathcal{O}(D \log N)$ (Best) to $\mathcal{O}(N D)$ | Low dimensions: $D < 20$ |
| **Ball-Tree** | $\mathcal{O}(D \cdot N \log N)$ | $\mathcal{O}(D \log N)$ | Moderate dimensions: $D \approx 20 - 50$ |
| **Hierarchical NSW (HNSW)**| $\mathcal{O}(N \log N)$ | $\mathcal{O}(\log N)$ (Approximate) | Ultra-high dimensions: $D > 100$ |

---

## 5. Inductive Bias & Core Assumptions

1. **Local Smoothness (Continuity)**: The conditional expectation function $m(\mathbf{x}) = \mathbb{E}[y \mid \mathbf{x}]$ is Lipschitz continuous: $\|\mathbf{x}_1 - \mathbf{x}_2\| \le \delta \implies |m(\mathbf{x}_1) - m(\mathbf{x}_2)| \le L \delta$.
2. **Isotropic Feature Importance**: Standard Euclidean KNN assumes all $D$ features contribute equally to the distance. If 90 out of 100 features are irrelevant noise, the Euclidean distance is completely dominated by noise.
3. **No Extrapolation Capability**: KNN can never predict a value outside $[\min(y_{\text{train}}), \max(y_{\text{train}})]$. When predicting on a point far outside the training cluster, the nearest neighbors are simply the edge points of the training cloud, causing predictions to flatline into a horizontal constant.

---

## 6. From-Scratch NumPy Implementation

```python
"""
Pure NumPy Implementation of KNN Regressor.
Supports Uniform and Distance-Weighted Neighbor Averaging.
Tested on Python 3.11, NumPy 2.x.
"""

from typing import Optional
import numpy as np


class PureNumPyKNNRegressor:
    """K-Nearest Neighbors Regressor using brute-force distance matrix."""

    def __init__(self, n_neighbors: int = 5, weights: str = "uniform", p: int = 2):
        self.n_neighbors = n_neighbors
        self.weights = weights
        self.p = p
        self.X_train_: np.ndarray = np.array([])
        self.y_train_: np.ndarray = np.array([])

    def fit(self, X: np.ndarray, y: np.ndarray):
        assert X.ndim == 2, "X must be 2D array (N, D)."
        assert y.ndim == 1, "y must be 1D array (N,)."
        assert len(X) == len(y), "X and y length mismatch."
        self.X_train_ = X.copy()
        self.y_train_ = y.copy()
        return self

    def predict(self, X_test: np.ndarray) -> np.ndarray:
        assert self.X_train_.size > 0, "Model has not been fitted."
        N_test = len(X_test)
        predictions = np.zeros(N_test)

        for i in range(N_test):
            query = X_test[i]
            # Compute Minkowski distance to all training points
            if self.p == 2:
                distances = np.linalg.norm(self.X_train_ - query, axis=1)
            else:
                distances = np.sum(np.abs(self.X_train_ - query) ** self.p, axis=1) ** (1.0 / self.p)

            # Find k nearest neighbors
            k_indices = np.argsort(distances)[: self.n_neighbors]
            k_distances = distances[k_indices]
            k_targets = self.y_train_[k_indices]

            if self.weights == "uniform":
                predictions[i] = np.mean(k_targets)
            elif self.weights == "distance":
                # Handle exact zero distance (coincident point)
                if np.any(k_distances == 0.0):
                    zero_mask = k_distances == 0.0
                    predictions[i] = np.mean(k_targets[zero_mask])
                else:
                    weights = 1.0 / k_distances
                    predictions[i] = np.sum(weights * k_targets) / np.sum(weights)
            else:
                raise ValueError(f"Unknown weights: {self.weights}")

        return predictions


# Verification Script
if __name__ == "__main__":
    np.random.seed(42)
    # Target: y = sin(x)
    X = np.linspace(0, 2 * np.pi, 50).reshape(-1, 1)
    y = np.sin(X).flatten() + np.random.normal(0, 0.05, size=50)

    knn = PureNumPyKNNRegressor(n_neighbors=3, weights="distance")
    knn.fit(X, y)

    X_query = np.array([[np.pi / 2], [np.pi]])
    preds = knn.predict(X_query)

    print("--- Pure NumPy KNN Predictions ---")
    print(f"Prediction at pi/2 (Expected ~ 1.0): {preds[0]:.4f}")
    print(f"Prediction at pi   (Expected ~ 0.0): {preds[1]:.4f}")

    assert np.isclose(preds[0], 1.0, atol=0.1)
    assert np.isclose(preds[1], 0.0, atol=0.1)
    print("[SUCCESS] Pure NumPy KNN Regressor verified.")
```

---

## 7. Production-Grade Scikit-Learn Implementation

```python
"""
Production Pipeline: KNeighborsRegressor with Standardized Features.
Tuned via GridSearchCV over k and distance weighting.
"""

import numpy as np
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.neighbors import KNeighborsRegressor
from sklearn.model_selection import GridSearchCV, train_test_split
from sklearn.metrics import mean_squared_error, r2_score


def run_knn_pipeline():
    np.random.seed(42)
    X = np.random.uniform(-3, 3, size=(300, 3))
    y = X[:, 0] ** 2 + np.cos(X[:, 1]) + np.random.normal(0, 0.1, size=300)

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    pipeline = Pipeline([
        ("scaler", StandardScaler()), # MANDATORY FOR KNN
        ("knn", KNeighborsRegressor(algorithm="auto"))
    ])

    param_grid = {
        "knn__n_neighbors": [3, 5, 7, 11, 15],
        "knn__weights": ["uniform", "distance"],
        "knn__p": [1, 2] # Manhattan vs Euclidean
    }

    grid = GridSearchCV(pipeline, param_grid=param_grid, cv=5, scoring="neg_mean_squared_error")
    grid.fit(X_train, y_train)

    best_pipe = grid.best_estimator_
    print("--- KNN Hyperparameter Optimization ---")
    print(f"Best Params: {grid.best_params_}")

    y_pred = best_pipe.predict(X_test)
    print(f"Test R^2: {r2_score(y_test, y_pred):.4f}")
    assert r2_score(y_test, y_pred) > 0.85


if __name__ == "__main__":
    run_knn_pipeline()
```

---

## 8. Model Persistence & Safe Serialization

```python
import joblib
import tempfile
import os

def persist_knn(pipeline):
    with tempfile.TemporaryDirectory() as tmp_dir:
        path = os.path.join(tmp_dir, "knn_pipeline.joblib")
        joblib.dump(pipeline, path)
        loaded = joblib.load(path)
        # Verify the entire training dataset was stored inside KNN
        assert loaded.named_steps["knn"]._fit_X.shape == pipeline.named_steps["knn"]._fit_X.shape
        print("[SUCCESS] KNN Regressor serialized safely.")
```

---

## 9. Hyperparameter Tuning & Optimization

| Hyperparameter | Search Space | Default | Practical Guidance |
| :--- | :--- | :--- | :--- |
| `n_neighbors` | `[3, 5, 7, 11, 15, 25]` | `5` | Smaller $k$ fits complex non-linear curves; larger $k$ resists noise. |
| `weights` | `['uniform', 'distance']` | `'uniform'` | Distance weighting almost always outperforms uniform on uneven densities. |
| `p` | `[1, 2]` | `2` | $p=1$ (Manhattan) is often superior when $D > 10$. |
| `algorithm` | `['auto', 'ball_tree', 'kd_tree', 'brute']` | `'auto'` | Let scikit-learn select based on $N$ and $D$. |

---

## 10. Diagnostic Evaluations & Residual Analysis

```
+-----------------------------------------------------------------------------------+
|                        KNN RESIDUAL DIAGNOSTICS                                   |
+-----------------------------------------------------------------------------------+
| 1. High Residuals in Low-Density Regions: KNN error correlates with distance to   |
|    the k-th neighbor. Plot residual magnitude vs mean neighbor distance.          |
| 2. Edge Degradation: Residuals show strong directional bias at extreme boundaries |
|    due to flatline extrapolation failure.                                         |
+-----------------------------------------------------------------------------------+
```

---

## 11. Calibration, Uncertainty & Prediction Intervals

Unlike linear models with parametric normality, KNN provides **empirical neighborhood prediction intervals**:
1. For query point $\mathbf{x}_q$, extract the $k$ neighbor targets: $Y_k = [y_{(1)}, \dots, y_{(k)}]$.
2. Compute the empirical variance: $\hat{\sigma}^2(\mathbf{x}_q) = \frac{1}{k-1} \sum (y_{(i)} - \hat{y})^2$.
3. Construct non-parametric intervals using empirical percentiles:
   $$[ \text{Quantile}(Y_k, \alpha/2), \, \text{Quantile}(Y_k, 1 - \alpha/2) ]$$

---

## 12. Edge Cases, Failure Modes & Remediation

| Failure Mode | Root Cause | Observable Symptom | Engineering Remediation |
| :--- | :--- | :--- | :--- |
| **Extrapolation Flatline** | Query point lies far outside training domain | Predicts constant edge value; error $\to \infty$ | Fall back to linear model or alert on out-of-distribution input. |
| **Scale Disparity** | One feature has scale $10^5$, another has scale $1.0$ | Large feature completely dominates distance | Prepend `StandardScaler()` in an `sklearn.pipeline.Pipeline`. |
| **Curse of Dimensionality** | Dataset has $D > 50$ features | Distance to nearest neighbor $\approx$ distance to average | Run PCA or feature selection prior to KNN. |
| **High Inference Latency** | Querying $N=1,000,000$ points with brute force | Single prediction takes $> 1\text{ second}$ | Subsample data or use Approximate Nearest Neighbors (Faiss/HNSW). |
| **High Memory Footprint** | Storing all $N \times D$ training records in RAM | Microservice OOM on large datasets | Use tree-based models (Random Forest) which store only split rules. |
| **Tie / Distance Zero Glitch** | Query point is identical to training point | Division by zero in $1 / d$ | Add small stabilizer: $1 / (d + 10^{-6})$. |

---

## 13. Comparative Analysis: KNN vs Alternatives

| Dimension | KNN Regressor | Decision Tree Regressor | Linear Regression | Random Forest |
| :--- | :--- | :--- | :--- | :--- |
| **Model Type** | Non-parametric (Lazy) | Non-parametric (Eager) | Parametric (Eager) | Ensemble (Eager) |
| **Training Speed** | Instant ($\mathcal{O}(1)$) | Fast ($\mathcal{O}(D N \log N)$) | Fast ($\mathcal{O}(N D^2)$) | Moderate |
| **Inference Speed** | Slow ($\mathcal{O}(N D)$) | Ultra-Fast ($\mathcal{O}(\text{depth})$)| Instant ($\mathcal{O}(D)$) | Fast |
| **Memory in Serving**| Huge (Stores all data) | Minimal (Tree nodes) | Tiny (Weight vector) | Moderate |
| **Extrapolation** | Zero (Flatlines) | Zero (Flatlines) | Infinite (Linear) | Zero (Flatlines) |

---

## 14. Production Engineering & Serving Considerations

1. **Approximate Nearest Neighbors (ANN) in High-Throughput APIs**:
   When serving KNN at scale ($N > 100,000$), standard KD-Trees fail. Export vectors to vector search engines like **FAISS** or **HNSWLib**, achieving sub-millisecond retrieval with $> 98\%$ recall.
2. **Serving RAM Sizing**:
   Calculate exact memory requirements before deployment:
   $$\text{RAM Bytes} \approx N \times D \times 8\text{ bytes (float64)}$$
   A dataset of $10^7$ rows and 100 features requires $8\text{ GB}$ of RAM strictly to hold the KNN lookup table in memory.

---

## 15. Interpretability & Feature Attribution

KNN offers intuitive **example-based interpretability**:
- To explain why $\hat{y} = 42.5$, return the exact $k$ historical records that were nearest neighbors to the customer, displaying their IDs and target values: *"Your estimated home value is $\$450\text{k}$ because 3 houses on your street sold for $\$440\text{k}, \$455\text{k}, \$452\text{k}$ last month."*

---

## 16. Hardware & Distributed Scaling

Pairwise distance computation is embarrassingly parallel. In scikit-learn, set `n_jobs=-1` to parallelize queries across CPU cores. For GPU acceleration, use RAPIDS cuML `cuml.neighbors.KNeighborsRegressor`.

---

## 17. Verification & Diagnostics Test Harness

```python
import pytest
import numpy as np
from sklearn.neighbors import KNeighborsRegressor

def test_knn_exact_memorization():
    # k=1 must achieve exact 0 training error
    X = np.array([[1.0], [2.0], [3.0], [4.0]])
    y = np.array([10.0, 20.0, 30.0, 40.0])
    knn = KNeighborsRegressor(n_neighbors=1)
    knn.fit(X, y)
    assert np.allclose(knn.predict(X), y)

def test_knn_extrapolation_flatline():
    X = np.array([[0.0], [1.0]])
    y = np.array([0.0, 1.0])
    knn = KNeighborsRegressor(n_neighbors=1)
    knn.fit(X, y)
    # Query at x=100.0 (far outside domain)
    pred = knn.predict([[100.0]])
    # Must flatline at the boundary value 1.0, NOT extrapolate linearly to 100.0
    assert pred[0] == 1.0
```

---

## 18. Cross-Track Connections

- **05-ai/machine-learning/01-math-foundations/08-curse-of-dimensionality.md**: Concentration of distances in high dimensions.
- **05-ai/machine-learning/02-data-and-preprocessing/16-handling-missing-data-in-depth.md**: NaN-Euclidean distance in `KNNImputer`.
- **05-ai/machine-learning/04-supervised-classification/02-knn-classifier.md**: Classification via majority voting.

---

## 19. Real-World Case Study: Real-Time Hydroelectric Dam Flow Forecasting

A hydroelectric authority predicted water discharge volume ($y$ in $\text{m}^3/\text{s}$) 30 minutes ahead based on upstream sensor telemetry.
- **Environment**: Complex riverbed topography where non-linear hydraulic dynamics made physical differential equations too slow for real-time valve control.
- **Solution**:
  1. Built a historical database of 50,000 hourly river telemetry snapshots.
  2. Deployed a `KNeighborsRegressor(n_neighbors=5, weights='distance')` indexed via a Ball-Tree.
  3. Prediction latency was $< 2\text{ ms}$, achieving an $R^2$ of 0.96.
  4. Example-based explanations allowed dam engineers to inspect the 5 historical analog events to verify flood safety before opening dam spillway gates.

---

## 20. Technical Interview Questions & Solutions

### Question 1: Why does KNN completely fail at extrapolation?
**Answer**:
KNN computes predictions strictly as a convex combination of training targets:
$$\hat{y}(\mathbf{x}_q) = \sum_{i=1}^k w_i y_{(i)} \quad \text{where } \sum w_i = 1, \ w_i \ge 0$$
This implies that for any query point $\mathbf{x}_q$, the prediction is strictly bounded:
$$\min(y_{\text{train}}) \le \hat{y}(\mathbf{x}_q) \le \max(y_{\text{train}})$$
If the true underlying function is $y = 2x$, and the training set spans $x \in [0, 10]$, then for query point $x = 100$, the true value is $200$. However, KNN finds the nearest neighbors among the training set, which are clustered near $x=10$ with target $y \approx 20$. The prediction flatlines at $20$, producing an error of $180$.

### Question 2: Explain the difference between KD-Trees and Ball-Trees.
**Answer**:
- **KD-Tree**: Recursively partitions feature space using **orthogonal axis-aligned hyperplanes** passing through the median of a single alternating coordinate dimension. Highly efficient when $D < 20$. However, when $D$ is large, hyperspheres intersect nearly all hypercubes, requiring the tree to backtrack and check almost all leaves ($\mathcal{O}(N)$).
- **Ball-Tree**: Recursively partitions space into nested **$D$-dimensional hyperspheres (balls)** defined by a centroid and radius. Points do not have to align with coordinate axes. Ball-Trees significantly outperform KD-Trees on high-dimensional or clustered manifolds ($D \approx 20 - 50$).

### Question 3: How does the Curse of Dimensionality affect distance metrics in KNN?
**Answer**:
Beyer et al. (1999) proved that as dimension $D \to \infty$:
$$\lim_{D \to \infty} \frac{d_{\max} - d_{\min}}{d_{\min}} = 0$$
The relative distance between the nearest neighbor $d_{\min}$ and the farthest neighbor $d_{\max}$ collapses to zero. In high dimensions, every point is essentially equidistant from the query point. Consequently, the concept of a "localized neighborhood" disappears, and KNN degrades to taking the global average of random points.

### Question 4: How can you incorporate categorical variables into KNN?
**Answer**:
Standard Euclidean distance is undefined for raw categories. Options:
1. One-Hot Encoding followed by scaling.
2. Target Encoding to continuous values.
3. Compute mixed-type distances using **Gower's Distance**, which evaluates Manhattan distance for numeric variables and Dice / Jaccard similarity for categorical variables.

### Question 5: Why is KNN called a "Lazy Learner"?
**Answer**:
Parametric models (like Linear Regression or Neural Networks) are "eager learners": they process training data during `fit()`, construct a compact abstraction (weights $\mathbf{w}$), and can discard the training data.
KNN is "lazy" because `fit()` performs zero mathematical work beyond storing the data in memory. All computation is deferred until query time (`predict()`), shifting the computational burden entirely to inference.

### Question 6: What happens if you run KNN without feature scaling?
**Answer**:
Euclidean distance is $d = \sqrt{\sum (u_j - v_j)^2}$. A feature with large numerical values (e.g., `income` spanning $\$10,000$ to $\$1,000,000$) will have squared differences on the order of $10^{10}$, while a feature with small scale (e.g., `age` spanning $18$ to $80$) will have differences $< 3600$. The distance is completely dominated by income; age is effectively erased from the model.

### Question 7: How do you choose the optimal $k$ without data leakage?
**Answer**:
Perform Grid Search with Cross-Validation (`GridSearchCV(KNeighborsRegressor(), param_grid={'n_neighbors': ...}, cv=5)`). Always encapsulate the scaler inside the pipeline so scaling statistics are recomputed within each training fold.

### Question 8: How does Mahalanobis distance improve KNN over Euclidean distance?
**Answer**:
If two features are highly correlated (e.g., height and weight), Euclidean distance counts the same variance twice. The Mahalanobis distance $d_M = \sqrt{(\mathbf{x} - \mathbf{y})^T \boldsymbol{\Sigma}^{-1} (\mathbf{x} - \mathbf{y})}$ decorrelates the features and scales each direction by its variance, measuring true statistical distance.

---

## 21. Guided Practice & Hands-On Exercises

### Exercise 1: KD-Tree vs Brute Force Benchmark
- Benchmark query latency of `algorithm='brute'` vs `algorithm='kd_tree'` across sample sizes $N \in [10^3, 10^5]$ and dimensions $D \in [2, 100]$. Identify the exact dimension $D$ where KD-Tree becomes slower than brute force.

### Exercise 2: Out-of-Distribution Extrapolation Audit
- Fit a KNN regressor and a Linear regressor on $y = 3x$ for $x \in [0, 10]$. Evaluate both models on test points $x \in [10, 50]$ and plot the diverging prediction curves.

### Exercise 3: Neighborhood Percentile Uncertainty Estimator
- Implement a custom KNN regressor that outputs both the point prediction $\hat{y}$ and the 10th and 90th percentiles of the $k$ neighbor targets as an empirical prediction interval.

### Exercise 4: Mahalanobis Distance KNN
- Implement a KNN regressor in NumPy that computes the sample covariance matrix $\boldsymbol{\Sigma}$, inverts it, and calculates Mahalanobis neighbor distances.

### Exercise 5: Gower Distance for Mixed Data
- Write a Python distance function implementing Gower's distance metric for a DataFrame containing both numeric and categorical columns.

### Exercise 6: Approximate Nearest Neighbors with Annoy / Faiss
- Integrate Spotify's Annoy or Facebook's Faiss library with a scikit-learn regressor wrapper to perform sub-millisecond approximate regression on $N = 100,000$ samples.
