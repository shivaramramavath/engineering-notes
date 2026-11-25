# Support Vector Machines (SVM) Classifier

> **Track:** `05-ai/machine-learning` | **Module:** `04-supervised-classification` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `NumPy: >=1.26.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Support Vector Machine (C-Support Vector Classifier / C-SVC) |
| **Family** | Maximum Margin Linear & Kernel Classifiers / Convex Quadratic Programming |
| **Input Feature Types** | Continuous, scaled numerical features ($X \in \mathbb{R}^{n \times d}$). Categorical features must be one-hot or target encoded and standardised. |
| **Target Output** | Binary class labels $y \in \{-1, +1\}$ (or multiclass via One-vs-One / One-vs-Rest heuristics). Signed geometric distance / uncalibrated decision values $f(x) \in (-\infty, +\infty)$. |
| **Optimization Target** | Convex Quadratic Programming (QP): $\min_{\mathbf{w}, b, \boldsymbol{\xi}} \frac{1}{2}\|\mathbf{w}\|^2 + C \sum_{i=1}^n \xi_i$ subject to $y_i(\mathbf{w}^T \phi(\mathbf{x}_i) + b) \ge 1 - \xi_i, \, \xi_i \ge 0$. |
| **Loss Function** | Hinge Loss: $L(y, f(x)) = \max(0, 1 - y \cdot f(x))$ with $L_2$ weight regularisation $\frac{1}{2}\|\mathbf{w}\|^2$. |
| **Optimization Solvers** | Sequential Minimal Optimization (SMO) algorithm (implemented via LIBSVM in C++), Dual Coordinate Descent (LIBLINEAR for linear SVM). |
| **Key Hyperparameters** | Regularisation parameter $C$, Kernel function (Linear, Polynomial, RBF, Sigmoid), Kernel coefficient $\gamma$, Degree $d$, Intercept term $c_0$, Class weight balance. |
| **Inference Mechanism** | Deterministic boundary evaluation: $f(\mathbf{x}) = \text{sign}\left(\sum_{i \in \text{SV}} \alpha_i y_i K(\mathbf{x}_i, \mathbf{x}) + b\right)$. Only depends on support vectors $\alpha_i > 0$. |
| **Primary Failure Modes** | Quadratic-to-cubic training time scaling ($O(n^2 \cdot d)$ to $O(n^3)$), poor performance on unscaled/heterogeneous tabular data, sensitivity to noisy label flipping near margin boundaries. |

---

## 2. Architecture & Intuition

Support Vector Machines construct optimal geometric decision boundaries by maximising the functional margin between opposing classes in a feature space. While logistic regression models conditional log-odds across all data points globally, SVM isolates the single most informative subset of training exemplars—the **Support Vectors**—which lie directly on or within the geometric boundary margins.

```
       Feature x2
           ^
           |       Class +1          Positive Margin: w^T x + b = +1
           |         (+)     (+)
           |            (+)   [+]*  <-- Support Vector
           |-------------------/----------------------
           |                  /  Separating Hyperplane: w^T x + b = 0
           |                 /   Margin Width = 2 / ||w||
           |----------------/-------------------------
           |        [-]*   /         Negative Margin: w^T x + b = -1
           |      (-)     /  <-- Support Vector
           |   (-)     (-)
           |  Class -1
           +------------------------------------------> Feature x1
```

### The Three Core Structural Tenets
1. **Geometric Margin Maximisation**: Minimising $\frac{1}{2}\|\mathbf{w}\|^2$ is geometrically equivalent to maximising the separation margin width $M = \frac{2}{\|\mathbf{w}\|}$ between support planes.
2. **Soft-Margin Relaxation ($\xi_i$)**: Real-world data is rarely linearly separable without overfitting. Slack variables $\xi_i \ge 0$ permit controlled violations, trading off geometric margin width against cumulative margin penetration penalty weighted by $C$.
3. **The Kernel Trick (Mercer's Theorem)**: By operating entirely within the dual optimization space, SVM maps non-linear input features into an arbitrary (or infinite-dimensional) Hilbert space $\mathcal{H}$ using an inner-product kernel $K(\mathbf{x}_i, \mathbf{x}_j) = \langle \phi(\mathbf{x}_i), \phi(\mathbf{x}_j) \rangle$ without explicitly computing the coordinates of $\phi(\mathbf{x})$.

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $\mathbf{x}_i$ | $\mathbb{R}^d$ | Feature vector of the $i$-th training observation |
| $y_i$ | $\{-1, +1\}$ | Binary ground-truth target classification label |
| $\mathbf{w}$ | $\mathbb{R}^d$ | Normal vector perpendicular to the separating hyperplane |
| $b$ | $\mathbb{R}$ | Hyperplane bias (intercept offset from coordinate origin) |
| $\gamma_i$ | $\mathbb{R}$ | Functional margin: $\hat{\gamma}_i = y_i(\mathbf{w}^T \mathbf{x}_i + b)$; Geometric margin: $\gamma_i = \frac{y_i(\mathbf{w}^T \mathbf{x}_i + b)}{\|\mathbf{w}\|}$ |
| $\xi_i$ | $\mathbb{R}_{\ge 0}$ | Slack variable quantifying boundary violation distance for sample $i$ |
| $C$ | $\mathbb{R}_{> 0}$ | Regularisation penalty hyperparameter balancing margin width vs slack |
| $\alpha_i$ | $\mathbb{R}_{\ge 0}$ | Lagrange multiplier associated with the inequality constraint of sample $i$ |
| $\mu_i$ | $\mathbb{R}_{\ge 0}$ | Lagrange multiplier associated with non-negativity constraint $\xi_i \ge 0$ |
| $K(\mathbf{x}_i, \mathbf{x}_j)$| $\mathbb{R}$ | Kernel function computing inner product $\langle \phi(\mathbf{x}_i), \phi(\mathbf{x}_j) \rangle$ |
| $\gamma$ | $\mathbb{R}_{> 0}$ | Radial Basis Function (RBF) kernel bandwidth scale hyperparameter |

### 1. Primal Soft-Margin Formulation
We seek a separating hyperplane $\mathbf{w}^T \mathbf{x} + b = 0$ that solves the constrained quadratic optimisation problem:

$$\min_{\mathbf{w}, b, \boldsymbol{\xi}} \mathcal{P}(\mathbf{w}, b, \boldsymbol{\xi}) = \frac{1}{2} \|\mathbf{w}\|^2 + C \sum_{i=1}^n \xi_i$$

Subject to the linear inequality constraints:
$$y_i(\mathbf{w}^T \mathbf{x}_i + b) \ge 1 - \xi_i, \quad \forall i \in \{1, \dots, n\}$$
$$\xi_i \ge 0, \quad \forall i \in \{1, \dots, n\}$$

### 2. The Lagrangian Dual Function
Formulate the generalised Lagrangian $\mathcal{L}(\mathbf{w}, b, \boldsymbol{\xi}, \boldsymbol{\alpha}, \boldsymbol{\mu})$ by introducing Lagrange multipliers $\alpha_i \ge 0$ and $\mu_i \ge 0$:

$$\mathcal{L} = \frac{1}{2} \|\mathbf{w}\|^2 + C \sum_{i=1}^n \xi_i - \sum_{i=1}^n \alpha_i \left[ y_i(\mathbf{w}^T \mathbf{x}_i + b) - 1 + \xi_i \right] - \sum_{i=1}^n \mu_i \xi_i$$

To find the dual formulation, set the partial derivatives with respect to primal variables $(\mathbf{w}, b, \xi_i)$ to zero:

$$\frac{\partial \mathcal{L}}{\partial \mathbf{w}} = \mathbf{w} - \sum_{i=1}^n \alpha_i y_i \mathbf{x}_i = 0 \implies \mathbf{w} = \sum_{i=1}^n \alpha_i y_i \mathbf{x}_i$$

$$\frac{\partial \mathcal{L}}{\partial b} = - \sum_{i=1}^n \alpha_i y_i = 0 \implies \sum_{i=1}^n \alpha_i y_i = 0$$

$$\frac{\partial \mathcal{L}}{\partial \xi_i} = C - \alpha_i - \mu_i = 0 \implies C = \alpha_i + \mu_i$$

Since $\mu_i \ge 0$, the condition $C = \alpha_i + \mu_i$ strictly constrains $\alpha_i \in [0, C]$.

Substituting $\mathbf{w} = \sum_{i=1}^n \alpha_i y_i \mathbf{x}_i$, $\sum_{i=1}^n \alpha_i y_i = 0$, and $\mu_i = C - \alpha_i$ back into $\mathcal{L}$ yields the **Wolfe Dual**:

$$\max_{\boldsymbol{\alpha}} \mathcal{D}(\boldsymbol{\alpha}) = \sum_{i=1}^n \alpha_i - \frac{1}{2} \sum_{i=1}^n \sum_{j=1}^n \alpha_i \alpha_j y_i y_j \mathbf{x}_i^T \mathbf{x}_j$$

Subject to:
$$0 \le \alpha_i \le C, \quad \forall i \in \{1, \dots, n\}$$
$$\sum_{i=1}^n \alpha_i y_i = 0$$

### 3. Karush-Kuhn-Tucker (KKT) Complementarity Conditions
At the global optimal solution $(\mathbf{w}^*, b^*, \boldsymbol{\xi}^*, \boldsymbol{\alpha}^*, \boldsymbol{\mu}^*)$, KKT conditions dictate:

1. **Primal Feasibility**: $y_i(\mathbf{w}^{*T} \mathbf{x}_i + b^*) - 1 + \xi_i^* \ge 0$ and $\xi_i^* \ge 0$.
2. **Dual Feasibility**: $0 \le \alpha_i^* \le C$ and $\mu_i^* \ge 0$.
3. **Complementary Slackness**:
   $$\alpha_i^* \left[ y_i(\mathbf{w}^{*T} \mathbf{x}_i + b^*) - 1 + \xi_i^* \right] = 0$$
   $$\mu_i^* \xi_i^* = (C - \alpha_i^*) \xi_i^* = 0$$

#### The Geometric Role of Support Vectors
- **Case 1: $\alpha_i^* = 0$ (Non-Support Vectors)**:
  Then $\mu_i^* = C > 0$, implying $\xi_i^* = 0$. The sample strictly satisfies $y_i(\mathbf{w}^T \mathbf{x}_i + b) > 1$. These points lie outside the margin and have zero influence on the decision boundary.
- **Case 2: $0 < \alpha_i^* < C$ (Free / Unbounded Support Vectors)**:
  Then $\mu_i^* = C - \alpha_i^* > 0 \implies \xi_i^* = 0$. The sample lies precisely on the canonical margin boundary:
  $$y_i(\mathbf{w}^T \mathbf{x}_i + b) = 1 \implies b^* = y_i - \mathbf{w}^T \mathbf{x}_i$$
  Any free support vector can be used to solve for the optimal bias $b^*$. In numerical implementations, $b^*$ is computed as the average over all free support vectors:
  $$b^* = \frac{1}{|\mathcal{S}_{\text{free}}|} \sum_{i \in \mathcal{S}_{\text{free}}} \left( y_i - \sum_{j \in \mathcal{S}} \alpha_j y_j K(\mathbf{x}_j, \mathbf{x}_i) \right)$$
- **Case 3: $\alpha_i^* = C$ (Bounded Support Vectors / Margin Violations)**:
  Then $\mu_i^* = 0$, meaning $\xi_i^* \ge 0$. These points violate the strict margin ($y_i(\mathbf{w}^T \mathbf{x}_i + b) \le 1$). If $0 < \xi_i^* \le 1$, they lie within the margin but are correctly classified. If $\xi_i^* > 1$, they are misclassified.

### 4. Mercer's Theorem & Kernel Formulations
Replacing the Euclidean dot product $\mathbf{x}_i^T \mathbf{x}_j$ with a positive semi-definite continuous symmetric kernel function $K(\mathbf{x}_i, \mathbf{x}_j) = \langle \phi(\mathbf{x}_i), \phi(\mathbf{x}_j) \rangle$:

1. **Linear**: $K(\mathbf{x}, \mathbf{z}) = \mathbf{x}^T \mathbf{z}$
2. **Radial Basis Function (RBF / Gaussian)**: $K(\mathbf{x}, \mathbf{z}) = \exp\left(-\gamma \|\mathbf{x} - \mathbf{z}\|^2\right)$, where $\gamma = \frac{1}{2\sigma^2}$ (corresponds to an infinite-dimensional feature space).
3. **Polynomial**: $K(\mathbf{x}, \mathbf{z}) = (\gamma \mathbf{x}^T \mathbf{z} + c_0)^d$
4. **Sigmoid**: $K(\mathbf{x}, \mathbf{z}) = \tanh(\gamma \mathbf{x}^T \mathbf{z} + c_0)$

---

## 4. Computational Complexity

| Phase | Linear Kernel (LIBLINEAR / Primal) | Non-Linear Kernel (LIBSVM / Dual SMO) | Space Complexity |
| :--- | :--- | :--- | :--- |
| **Training Time** | $O(n \cdot d)$ (converges in linear time) | $O(n^2 \cdot d)$ to $O(n^3)$ (depends strongly on cache hit rate and $C$) | $O(n \cdot d)$ or $O(n_{\text{cache}}^2)$ kernel cache |
| **Inference Time (Single Sample)** | $O(d)$ via precomputed $\mathbf{w} \in \mathbb{R}^d$ | $O(n_{\text{SV}} \cdot d)$ where $n_{\text{SV}} \le n$ is number of support vectors | $O(n_{\text{SV}} \cdot d)$ stored support vector weights |
| **Memory Footprint** | Extremely compact: storing single vector $\mathbf{w} \in \mathbb{R}^d$ and bias $b \in \mathbb{R}$. | High: must retain all support vector feature matrices $X_{\text{SV}} \in \mathbb{R}^{n_{\text{SV}} \times d}$ and dual multipliers $\alpha_{\text{SV}}$. |

---

## 5. Inductive Bias & Learning Guarantees

1. **Maximum Margin Principle**: SVM biases towards the hyperplane that maximizes the geometric distance to the closest data points. Under Vapnik-Chervonenkis (VC) theory, the VC dimension of linear classifiers with margin $\gamma$ is bounded by $h \le \min\left(\lceil \frac{R^2}{\gamma^2} \rceil, d\right) + 1$, where $R$ is the radius of the smallest sphere enclosing the data. This provides strong distribution-free generalization bounds independent of raw feature dimensionality $d$.
2. **Sparsity in Dual Representation**: Only samples with non-zero dual coefficients ($\alpha_i > 0$) determine the decision boundary. All other training observations can be discarded without altering the solution.
3. **Strict Convexity**: The optimization problem is a convex quadratic program with linear constraints. It is guaranteed to converge to a unique global optimum; there are zero local minima.

---

## 6. Scratch NumPy Implementation

The following implementation implements a simplified Sequential Minimal Optimization (SMO) algorithm solving the dual quadratic program with an RBF or Linear kernel.

```python
"""
Scratch NumPy implementation of Support Vector Classifier (C-SVC) using SMO.
Provides an exact educational reference matching LIBSVM formulation.
"""

from __future__ import annotations
import numpy as np
from typing import Literal, Tuple, Callable


class ScratchSVC:
    """Support Vector Classifier implemented from first principles via SMO."""

    def __init__(
        self,
        C: float = 1.0,
        kernel: Literal["linear", "rbf"] = "rbf",
        gamma: float | str = "scale",
        tol: float = 1e-3,
        max_iter: int = 100,
        random_state: int | None = 42,
    ) -> None:
        self.C = float(C)
        self.kernel = kernel
        self.gamma_param = gamma
        self.tol = tol
        self.max_iter = max_iter
        self.random_state = random_state

        # Fitted attributes
        self.gamma: float = 1.0
        self.alpha: np.ndarray | None = None
        self.b: float = 0.0
        self.X_train: np.ndarray | None = None
        self.y_train: np.ndarray | None = None
        self.support_indices_: np.ndarray | None = None
        self.support_vectors_: np.ndarray | None = None
        self.dual_coef_: np.ndarray | None = None

    def _compute_kernel(self, X1: np.ndarray, X2: np.ndarray) -> np.ndarray:
        """Compute the kernel matrix between X1 (n, d) and X2 (m, d)."""
        if self.kernel == "linear":
            return np.dot(X1, X2.T)
        elif self.kernel == "rbf":
            # Efficient vectorized squared Euclidean distance
            # ||x - z||^2 = ||x||^2 + ||z||^2 - 2 x^T z
            x1_sq = np.sum(X1 ** 2, axis=1, keepdims=True)
            x2_sq = np.sum(X2 ** 2, axis=1, keepdims=True)
            dist_sq = x1_sq + x2_sq.T - 2.0 * np.dot(X1, X2.T)
            # Clip numerical noise below zero
            dist_sq = np.maximum(dist_sq, 0.0)
            return np.exp(-self.gamma * dist_sq)
        else:
            raise ValueError(f"Unsupported kernel: {self.kernel}")

    def fit(self, X: np.ndarray, y: np.ndarray) -> ScratchSVC:
        """
        Fit the SVM classifier using simplified Sequential Minimal Optimization (SMO).
        
        Args:
            X: Input feature matrix of shape (n_samples, n_features).
            y: Target binary labels of shape (n_samples,). Must be encoded as {-1, +1}.
        """
        rng = np.random.default_rng(self.random_state)
        X = np.asarray(X, dtype=np.float64)
        y = np.asarray(y, dtype=np.float64)
        n_samples, n_features = X.shape

        # Verify binary classes in {-1, +1}
        unique_y = np.unique(y)
        if not np.array_equal(np.sort(unique_y), np.array([-1.0, 1.0])):
            raise ValueError("Target y must contain exactly binary classes {-1, +1}.")

        # Determine gamma scale
        if isinstance(self.gamma_param, str):
            if self.gamma_param == "scale":
                var_x = float(X.var())
                self.gamma = 1.0 / (n_features * var_x) if var_x > 0 else 1.0
            else:
                self.gamma = 1.0 / n_features
        else:
            self.gamma = float(self.gamma_param)

        self.X_train = X
        self.y_train = y
        self.alpha = np.zeros(n_samples, dtype=np.float64)
        self.b = 0.0

        # Precompute kernel matrix for training
        K = self._compute_kernel(X, X)

        # Simplified SMO loop
        iters = 0
        while iters < self.max_iter:
            num_changed_alphas = 0
            for i in range(n_samples):
                # Calculate margin error E_i = f(x_i) - y_i
                u_i = float(np.sum(self.alpha * y * K[:, i]) + self.b)
                E_i = u_i - y[i]

                # Check KKT conditions violation within tolerance
                # r_i = y_i * E_i
                # If (r_i < -tol and alpha_i < C) or (r_i > tol and alpha_i > 0)
                if (y[i] * E_i < -self.tol and self.alpha[i] < self.C) or \
                   (y[i] * E_i > self.tol and self.alpha[i] > 0.0):
                    
                    # Randomly select j != i
                    j_choices = [k for k in range(n_samples) if k != i]
                    j = int(rng.choice(j_choices))

                    u_j = float(np.sum(self.alpha * y * K[:, j]) + self.b)
                    E_j = u_j - y[j]

                    alpha_i_old = self.alpha[i]
                    alpha_j_old = self.alpha[j]

                    # Compute bounding box [L, H] for alpha_j
                    if y[i] != y[j]:
                        L = max(0.0, alpha_j_old - alpha_i_old)
                        H = min(self.C, self.C + alpha_j_old - alpha_i_old)
                    else:
                        L = max(0.0, alpha_i_old + alpha_j_old - self.C)
                        H = min(self.C, alpha_i_old + alpha_j_old)

                    if L == H:
                        continue

                    # Second derivative eta = 2*K(x_i, x_j) - K(x_i, x_i) - K(x_j, x_j)
                    eta = 2.0 * K[i, j] - K[i, i] - K[j, j]
                    if eta >= 0:
                        continue

                    # Update alpha_j and clip to [L, H]
                    alpha_j_new = alpha_j_old - (y[j] * (E_i - E_j)) / eta
                    alpha_j_new = np.clip(alpha_j_new, L, H)

                    if abs(alpha_j_new - alpha_j_old) < 1e-5:
                        continue

                    # Update alpha_i accordingly
                    alpha_i_new = alpha_i_old + y[i] * y[j] * (alpha_j_old - alpha_j_new)

                    # Update threshold b to satisfy KKT
                    b1 = self.b - E_i - y[i] * (alpha_i_new - alpha_i_old) * K[i, i] \
                         - y[j] * (alpha_j_new - alpha_j_old) * K[i, j]
                    b2 = self.b - E_j - y[i] * (alpha_i_new - alpha_i_old) * K[i, j] \
                         - y[j] * (alpha_j_new - alpha_j_old) * K[j, j]

                    if 0 < alpha_i_new < self.C:
                        self.b = b1
                    elif 0 < alpha_j_new < self.C:
                        self.b = b2
                    else:
                        self.b = (b1 + b2) / 2.0

                    self.alpha[i] = alpha_i_new
                    self.alpha[j] = alpha_j_new
                    num_changed_alphas += 1

            if num_changed_alphas == 0:
                iters += 1
            else:
                iters = 0

        # Store support vectors
        sv_mask = self.alpha > 1e-5
        self.support_indices_ = np.where(sv_mask)[0]
        self.support_vectors_ = X[sv_mask]
        self.dual_coef_ = (self.alpha[sv_mask] * y[sv_mask]).reshape(1, -1)
        return self

    def decision_function(self, X: np.ndarray) -> np.ndarray:
        """Compute the signed geometric distance to the separating boundary."""
        if self.support_vectors_ is None or self.dual_coef_ is None:
            raise RuntimeError("ScratchSVC instance is not fitted.")
        X = np.asarray(X, dtype=np.float64)
        K_sv = self._compute_kernel(X, self.support_vectors_)
        return np.dot(K_sv, self.dual_coef_.T).flatten() + self.b

    def predict(self, X: np.ndarray) -> np.ndarray:
        """Predict binary labels in {-1, +1}."""
        df = self.decision_function(X)
        return np.where(df >= 0.0, 1.0, -1.0)
```

---

## 7. Production Scikit-Learn 9-Stage Pipeline

```python
"""
End-to-end production Support Vector Machine classification pipeline.
Implements calibrated probabilities, scaling, tuning, and rigorous validation.
"""

from __future__ import annotations
import logging
from typing import Dict, Any
import numpy as np
from sklearn.base import BaseEstimator
from sklearn.datasets import make_classification
from sklearn.model_selection import train_test_split, StratifiedKFold, HalvingGridSearchCV
from sklearn.preprocessing import StandardScaler, RobustScaler
from sklearn.pipeline import Pipeline
from sklearn.svm import SVC
from sklearn.calibration import CalibratedClassifierCV
from sklearn.metrics import classification_report, roc_auc_score, brier_score_loss

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("SVM_Pipeline")


def build_and_evaluate_svm_pipeline() -> Tuple[BaseEstimator, Dict[str, float]]:
    # Stage 1: Data Ingestion & Synthesis
    logger.info("Stage 1: Synthesizing non-linear classification dataset.")
    X, y = make_classification(
        n_samples=2500,
        n_features=20,
        n_informative=12,
        n_redundant=4,
        n_clusters_per_class=2,
        weights=[0.75, 0.25],  # Imbalanced ratio
        flip_y=0.03,            # 3% label noise
        random_state=42,
    )

    # Stage 2: Train/Test Split
    logger.info("Stage 2: Partitioning stratified train/test datasets.")
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.20, stratify=y, random_state=42
    )

    # Stage 3: Base Pipeline Formulation (Scaling + Estimator)
    # SVM is mathematically strictly sensitive to unscaled variance!
    raw_pipeline = Pipeline([
        ("scaler", RobustScaler(with_centering=True, with_scaling=True)),
        ("svm", SVC(
            kernel="rbf",
            probability=False,     # Internal Platt scaling is disabled during grid search for speed
            class_weight="balanced",
            random_state=42,
            cache_size=500        # MB allocated for kernel matrix cache
        ))
    ])

    # Stage 4: Hyperparameter Optimization Strategy
    logger.info("Stage 4: Executing HalvingGridSearchCV for C and gamma tuning.")
    param_grid = {
        "svm__C": [0.1, 1.0, 10.0, 100.0],
        "svm__gamma": ["scale", "auto", 0.01, 0.1],
    }
    cv = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)
    search = HalvingGridSearchCV(
        raw_pipeline,
        param_grid=param_grid,
        cv=cv,
        scoring="roc_auc",
        factor=2,
        n_jobs=-1,
        random_state=42
    )
    search.fit(X_train, y_train)
    best_estimator = search.best_estimator_
    logger.info(f"Best Hyperparameters: {search.best_params_}")

    # Stage 5: Rigorous Probability Calibration
    # SVM decision values are uncalibrated margins; wrap in CalibratedClassifierCV
    logger.info("Stage 5: Calibrating margins into true posterior probabilities.")
    calibrated_model = CalibratedClassifierCV(
        estimator=best_estimator,
        method="sigmoid",  # Platt scaling
        cv=5
    )
    calibrated_model.fit(X_train, y_train)

    # Stage 6: Inference on Test Set
    logger.info("Stage 6: Running out-of-sample inference.")
    y_pred = calibrated_model.predict(X_test)
    y_prob = calibrated_model.predict_proba(X_test)[:, 1]

    # Stage 7: Comprehensive Evaluation
    roc_auc = float(roc_auc_score(y_test, y_prob))
    brier = float(brier_score_loss(y_test, y_prob))
    logger.info(f"Test ROC-AUC: {roc_auc:.4f} | Brier Calibration Loss: {brier:.4f}")
    logger.info("\n" + classification_report(y_test, y_pred))

    metrics = {"roc_auc": roc_auc, "brier_score": brier}
    return calibrated_model, metrics


if __name__ == "__main__":
    model, perf = build_and_evaluate_svm_pipeline()
    assert perf["roc_auc"] > 0.85, "Pipeline ROC-AUC failed SLA target."
```

---

## 8. Model Persistence & Serialization

```python
"""
Enterprise serialization pattern for Support Vector Classifiers.
Ensures SHA-256 integrity, runtime metadata verification, and tamper detection.
"""

import json
import hashlib
from pathlib import Path
import joblib
from sklearn.base import BaseEstimator


def serialize_svm_model(model: BaseEstimator, output_path: Path, metadata: dict) -> Path:
    """Save trained SVM model along with cryptographic checksum and provenance sidecar."""
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    # 1. Save binary payload via joblib
    joblib.dump(model, output_path, compress=("gzip", 3))

    # 2. Calculate SHA-256 hash
    hasher = hashlib.sha256()
    with open(output_path, "rb") as f:
        while chunk := f.read(65536):
            hasher.update(chunk)
    digest = hasher.hexdigest()

    # 3. Write provenance sidecar
    sidecar_path = output_path.with_suffix(".json")
    sidecar_data = {
        "model_file": output_path.name,
        "sha256": digest,
        "framework": "scikit-learn",
        "algorithm": "SupportVectorClassifier",
        "metadata": metadata
    }
    with open(sidecar_path, "w", encoding="utf-8") as f:
        json.dump(sidecar_data, f, indent=2)

    return sidecar_path


def load_svm_model(model_path: Path) -> BaseEstimator:
    """Load model securely, validating checksum against sidecar metadata."""
    model_path = Path(model_path)
    sidecar_path = model_path.with_suffix(".json")
    if not sidecar_path.exists():
        raise FileNotFoundError(f"Missing security metadata sidecar: {sidecar_path}")

    with open(sidecar_path, "r", encoding="utf-8") as f:
        meta = json.load(f)

    # Cryptographic integrity check
    hasher = hashlib.sha256()
    with open(model_path, "rb") as f:
        while chunk := f.read(65536):
            hasher.update(chunk)
    current_digest = hasher.hexdigest()

    if current_digest != meta["sha256"]:
        raise ValueError("Cryptographic checksum mismatch! Model binary has been tampered with or corrupted.")

    return joblib.load(model_path)
```

> [!WARNING]
> Serializing non-linear SVM models stores every single support vector in memory. For large or noisy datasets where $n_{\text{SV}} \to n$, model files can exceed hundreds of megabytes. Verify model size via `len(model.named_steps['svm'].support_)` before committing to edge deployments.

---

## 9. Hyperparameter Tuning Guide

```
+---------------------------------------------------------------------------------+
|                       SVM REGULARISATION DYNAMICS (C vs GAMMA)                  |
|                                                                                 |
|                   Small gamma (Wide bell curve)       Large gamma (Narrow peak) |
|               +-----------------------------------+-----------------------------+
| Small C       | High Regularisation / Underfit    | Localised islands / high    |
| (Soft margin) | Flat boundary, large margin       | variance around points      |
|               +-----------------------------------+-----------------------------+
| Large C       | Precise boundary, low variance,   | Severe Overfitting!         |
| (Hard margin) | sensitive to outliers             | Isolates individual points  |
+---------------+-----------------------------------+-----------------------------+
```

| Hyperparameter | Default | Exploration Range | Impact & Diagnostics |
| :--- | :--- | :--- | :--- |
| `C` | `1.0` | $[10^{-3}, 10^{3}]$ (log-scale) | Inverse regularisation strength. Large $C$ penalizes violations heavily, narrowing the margin and risking overfitting. Small $C$ creates wider margins with more violations (higher bias). |
| `kernel` | `'rbf'` | `['linear', 'poly', 'rbf', 'sigmoid']` | Linear is preferred when $d \gg n$ (e.g., text TF-IDF). RBF is the universal default when $n > d$ and relationships are non-linear. |
| `gamma` | `'scale'` | `['scale', 'auto', 10^{-4}, 10^1]` | Kernel coefficient for RBF/Poly. Defines radius of influence. $\gamma = \frac{1}{2\sigma^2}$. Large $\gamma$ creates complex, tight boundary "islands" around training points. |
| `class_weight` | `None` | `[None, 'balanced']` or custom dict | Multiplies $C$ by class inverse frequencies. Critical for handling severe class imbalance without manual resampling. |
| `cache_size` | `200` (MB) | $[500, 2000]$ (MB) | RAM allocated to LIBSVM's internal kernel matrix cache. Increasing to 1000MB drastically reduces training times on datasets with $n > 10,000$. |

---

## 10. Residuals, Diagnostics & Failure Modes

1. **Fraction of Support Vectors (SV Ratio)**:
   $$\rho_{\text{SV}} = \frac{n_{\text{SV}}}{n_{\text{train}}}$$
   - If $\rho_{\text{SV}} > 0.6$, the model is memorizing the dataset or the data has high overlap/noise. The model will suffer slow inference latency and high memory consumption. Increase $C$ or tune $\gamma$.
2. **Boundary Sparsity Audit**:
   Compare bounded support vectors ($\alpha_i = C$, margin violations) against free support vectors ($0 < \alpha_i < C$). A predominance of bounded SVs indicates either an overly soft margin ($C$ too small) or non-separable noise.
3. **Conditioning of Scalers**:
   SVM relies on isotropic Euclidean distances. If feature scaling (e.g., `StandardScaler`) is omitted or distorted by outliers, the margin will be dominated entirely by the feature with the largest absolute dynamic range.

---

## 11. Prediction Uncertainty & Calibration

Standard SVM outputs an unconstrained continuous decision value $f(\mathbf{x}) = \mathbf{w}^T \phi(\mathbf{x}) + b$. This represents signed geometric distance, not a calibrated probability.

### Platt Scaling
Platt scaling maps the margin distance $f(\mathbf{x})$ into a posterior probability via a parametric sigmoid:

$$P(y = 1 \mid \mathbf{x}) = \frac{1}{1 + \exp(A \cdot f(\mathbf{x}) + B)}$$

Parameters $A$ and $B$ are estimated by maximum likelihood on an internal cross-validation loop. In Scikit-Learn:
- `SVC(probability=True)` runs 5-fold cross-validation internally during `fit()`, increasing training time by $5\times$.
- **Best Practice**: Train `SVC(probability=False)` and wrap the best-tuned model in `CalibratedClassifierCV(estimator=model, cv=5, method='sigmoid')` or `'isotonic'`.

---

## 12. Comprehensive Failure Modes & Edge Cases Table

| Failure Mode | Root Cause | Observable Symptom | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **Exploding Training Time** | Sample size $n > 50,000$ with non-linear kernel. | Training runs for hours; CPU pinned at 100% on one core. | Switch to `LinearSVC` with Nystroem or SGDClassifier (`loss='hinge'`), or use LightGBM. |
| **Oversaturated Boundary (RBF Islanding)** | $\gamma$ set excessively high ($> 10.0$). | Perfect training accuracy ($1.0$), test accuracy collapses to majority class baseline. | Bound $\gamma \in [10^{-4}, 1.0]$. Inspect cross-validation curve over log-scale grid. |
| **Dominant Feature Bias** | Missing or improper feature scaling. | Margin parallel to unscaled feature axis; weights on other features vanish. | Enforce `StandardScaler` or `RobustScaler` strictly within an atomic `Pipeline`. |
| **Memory Exhaustion on Deployment** | Massive number of support vectors stored in pickled object. | Docker container crashes with `OOMKilled` on cold start. | Monitor `model.support_vectors_.nbytes`. Prune near-zero dual coefficients or retrain with higher $C$. |
| **Extreme Latency on Inference** | Millions of predictions required per second; kernel must evaluate across $10,000$ SVs. | Real-time p99 latency exceeds 500ms SLA. | If linear, extract explicit $\mathbf{w}$ and $b$ weights. If non-linear, distill into a lightweight neural net or decision forest. |
| **Class Imbalance Collapse** | Majority class dominates margin objective; minority class completely ignored. | Specificity = 1.0, Recall = 0.0 for minority class. | Set `class_weight='balanced'` to scale $C_i$ inversely proportional to class frequencies. |

---

## 13. Comparative Trade-off Table

| Algorithm | Training Scaling | Inference Latency | Non-Linear Capacity | Interpretability | Outlier Sensitivity |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **SVM (RBF Kernel)** | $O(n^2 \cdot d)$ to $O(n^3)$ | $O(n_{\text{SV}} \cdot d)$ (Moderate to Slow) | High (infinite-dimensional feature space) | Low (black-box dual combination) | High (if outliers lie near margin) |
| **Linear SVM** | $O(n \cdot d)$ via dual coordinate descent | $O(d)$ (Sub-millisecond) | None (purely linear) | High (feature weights directly observable) | Moderate |
| **Logistic Regression** | $O(n \cdot d)$ via L-BFGS | $O(d)$ (Sub-millisecond) | None (without manual feature engineering) | Very High (odds ratios) | Moderate |
| **Random Forest** | $O(M \cdot n \log n \cdot d)$ | $O(M \cdot \text{depth})$ (Fast) | High (orthogonal hyperplanes) | Moderate (Feature importance, SHAP) | Low (invariant to monotonic shifts) |
| **LightGBM / XGBoost** | $O(M \cdot n \cdot d)$ (Histogram binning) | $O(M \cdot \text{depth})$ (Extremely Fast) | Very High | Moderate | Low |

---

## 14. Production Engineering, Serving & Latency

### Real-Time Inference Optimization for Linear SVM
For a linear SVM ($K(\mathbf{x}, \mathbf{z}) = \mathbf{x}^T \mathbf{z}$), storing support vectors is completely redundant. Precompute the explicit primal weight vector:

$$\mathbf{w} = \sum_{i \in \text{SV}} \alpha_i y_i \mathbf{x}_i \in \mathbb{R}^d$$

```python
# Extract weights for ultra-low latency C/Go/Rust embedded serving
linear_svm = best_pipeline.named_steps["svm"]
if linear_svm.kernel == "linear":
    w = linear_svm.coef_[0]  # shape (d,)
    b = linear_svm.intercept_[0]
    # Prediction: dot(w, x) + b >= 0 -> class 1
```

For non-linear RBF models, inference requires calculating $n_{\text{SV}}$ pairwise distances. Keep $n_{\text{SV}} < 1,000$ by increasing $C$ or pruning data to satisfy sub-10ms latency SLAs.

---

## 15. Interpretability & Explainability

1. **Linear SVM**: Weight magnitude $|w_j|$ directly reflects feature contribution to the log-margin. Standardised features allow direct ranking of relative feature importance.
2. **Kernel SVM**: Individual weights do not exist in input space.
   - **Kernel SHAP**: Perturbs inputs to calculate Shapley values by treating the SVM as a black-box function.
   - **Support Vector Inspection**: Visualise the most influential support vectors (the training samples with largest $\alpha_i$). In image or text tasks, displaying these boundary archetypes provides human-interpretable reasoning for why a decision boundary sits where it does.

---

## 16. Hardware, Parallelism & Scaling

- **LIBSVM Single-Thread Constraint**: The standard C-SVC SMO implementation in Scikit-Learn wraps LIBSVM, which is fundamentally single-threaded for fitting a single binary problem. Setting `n_jobs=-1` parallelizes across cross-validation folds or One-vs-One pairs, but **not** within a single binary fit.
- **Large-Scale Alternative**: For datasets exceeding $100,000$ rows:
  - Use `LinearSVC` (based on LIBLINEAR, which executes in $O(n)$ time).
  - Use `SGDClassifier(loss='hinge', penalty='l2')` for out-of-core online mini-batch gradient descent.
  - Approximate RBF kernels using `sklearn.kernel_approximation.Nystroem` or `RBFSampler` followed by a linear classifier.

---

## 17. Automated Verification Harness

```python
"""
Pytest verification harness for SVM classification models.
Asserts mathematical consistency, shape integrity, and numerical stability.
"""

import numpy as np
import pytest
from sklearn.datasets import make_blobs
from sklearn.svm import SVC


@pytest.fixture
def separable_data() -> Tuple[np.ndarray, np.ndarray]:
    X, y = make_blobs(n_samples=200, centers=2, cluster_std=1.0, random_state=42)
    # Map {0, 1} to {-1, +1}
    y_signed = np.where(y == 0, -1, 1)
    return X, y_signed


def test_linear_svm_margin_orthogonality(separable_data):
    """Verify that primal weight vector w matches the dual combination sum(alpha * y * x)."""
    X, y = separable_data
    clf = SVC(kernel="linear", C=10.0)
    clf.fit(X, y)

    # Reconstruct w from support vectors and dual coefficients
    w_dual = np.sum(clf.dual_coef_.T * clf.support_vectors_, axis=0)
    w_primal = clf.coef_[0]

    np.testing.assert_allclose(
        w_dual, w_primal, rtol=1e-4, atol=1e-5,
        err_msg="Dual reconstructed weight w does not match primal coef_."
    )


def test_decision_function_sign_consistency(separable_data):
    """Ensure sign of decision function exactly matches binary predictions."""
    X, y = separable_data
    clf = SVC(kernel="rbf", C=1.0)
    clf.fit(X, y)

    df = clf.decision_function(X)
    pred = clf.predict(X)

    expected_pred = np.where(df >= 0.0, 1, -1)
    np.testing.assert_array_equal(pred, expected_pred)


def test_support_vector_kkt_bounds():
    """Verify that dual coefficients alpha are strictly bounded within [0, C]."""
    C_val = 5.0
    X, y = make_blobs(n_samples=150, centers=2, random_state=42)
    clf = SVC(kernel="rbf", C=C_val)
    clf.fit(X, y)

    alphas = np.abs(clf.dual_coef_[0])
    assert np.all(alphas >= 0.0), "Found negative dual coefficients."
    assert np.all(alphas <= C_val + 1e-6), f"Dual coefficients exceeded C={C_val}."
```

---

## 18. Cross-Track Learning & Ecosystem Links

- **Python Foundations**: Connects to `01-languages/python/21-data-and-scientific-basics` (Vectorized distance broadcasting).
- **Mathematical Background**: Links to `01-math-foundations/06-optimisation-and-gradient-descent` (Quadratic Programming and KKT conditions).
- **Data Engineering**: Connects to `02-data-and-preprocessing/05-feature-scaling` (Essential role of isotropic scaling in distance-based classifiers).
- **Backend Serving**: Serves models via FastAPI in `03-backend/fastapi/16-ai-and-ml-serving`.
- **System Design**: Integrates into high-throughput low-latency inference architectures in `07-system-design`.

---

## 19. Real-World Enterprise Case Study

### Context: High-Value Financial Transaction Fraud Detection
- **Domain**: Tier-1 Payment Processing Network.
- **Problem**: Detecting synthetic identity fraud in card-not-present cross-border transactions where feature space has high interaction non-linearity ($d = 45$ numerical features).
- **Constraints**: Dataset has high feature-to-noise ratio; ultra-low false positive rate required ($< 0.05\%$).
- **Implementation Strategy**:
  1. Pipeline: Standard scaling + RBF-kernel SVC.
  2. Cost-sensitive weighting: `class_weight={0: 1.0, 1: 15.0}`.
  3. Calibration: Sigmoid Platt scaling via `CalibratedClassifierCV(method='sigmoid', cv=5)`.
  4. Optimization: Halving search identified $C = 25.0, \gamma = 0.04$, yielding $n_{\text{SV}} = 1,420$ vectors from $n_{\text{train}} = 40,000$.
- **Result**: Reduced fraud leakage by $22\%$ compared to baseline Logistic Regression, maintaining p95 inference latency under $4.2\text{ms}$ per transaction.

---

## 20. Technical Interview Preparation

### Q1: Why does SVM maximize the geometric margin rather than simply finding any separating hyperplane?
**Answer:** Per Vapnik's Statistical Learning Theory, maximizing the geometric margin $M = \frac{2}{\|\mathbf{w}\|}$ minimizes the VC dimension of the hypothesis class. By maximizing separation, the model minimizes expected risk rather than empirical risk alone. Hyperplanes that pass too close to data points exhibit high variance and are fragile to small test-time perturbations.

### Q2: What happens to the SVM decision boundary as $C \to \infty$ and $C \to 0$?
**Answer:** As $C \to \infty$, the penalty for slack violations $\sum \xi_i$ becomes infinite, forcing all $\xi_i \to 0$. This recovers the hard-margin SVM. If the data is linearly non-separable, the optimizer fails to find a feasible solution; if separable, it fits a tight boundary susceptible to overfitting. As $C \to 0$, the penalty on margin violations vanishes, prioritizing the minimization of $\|\mathbf{w}\|^2$ (maximizing margin width) at the expense of misclassifying points, leading to underfitting.

### Q3: Explain Mercer's Condition in the context of kernel functions.
**Answer:** A function $K(\mathbf{x}, \mathbf{z})$ is a valid Mercer kernel if and only if for any square-integrable function $g(\mathbf{x})$, it satisfies $\iint K(\mathbf{x}, \mathbf{z}) g(\mathbf{x}) g(\mathbf{z}) d\mathbf{x} d\mathbf{z} \ge 0$. In matrix terms, for any finite set of points, the Gram matrix $K_{ij} = K(\mathbf{x}_i, \mathbf{x}_j)$ must be symmetric positive semi-definite (PSD). This ensures the dual quadratic optimization problem remains convex with a unique global minimum.

### Q4: Why is LIBSVM inefficient for datasets with $n > 100,000$ samples?
**Answer:** LIBSVM solves the dual quadratic program using the Sequential Minimal Optimization (SMO) algorithm. Each step optimizes a pair $(\alpha_i, \alpha_j)$. The number of iterations scales between $O(n^2)$ and $O(n^3)$ depending on the kernel cache hit rate and number of support vectors. Computing and maintaining an $n \times n$ Gram matrix in memory is intractable for large $n$.

### Q5: How do you extract feature importances from a non-linear RBF SVM?
**Answer:** Non-linear kernels project features into an implicit infinite-dimensional Hilbert space where features are non-linearly coupled, so no constant 1D weight vector $w_j$ exists in input space. To interpret feature influence, one must employ post-hoc model-agnostic explainability methods such as Kernel SHAP (estimating Shapley values) or Permutation Feature Importance (measuring validation loss degradation when feature columns are shuffled).

### Q6: What is the relationship between Hinge Loss and the SVM optimization objective?
**Answer:** The primal soft-margin objective can be rewritten directly as an unconstrained empirical risk minimization problem with $L_2$ weight decay:
$$\min_{\mathbf{w}, b} \sum_{i=1}^n \max\left(0, 1 - y_i(\mathbf{w}^T \mathbf{x}_i + b)\right) + \frac{1}{2C} \|\mathbf{w}\|^2$$
Here, $\max(0, 1 - y \cdot f(x))$ is the exact Hinge Loss function, which is zero when points are correctly classified beyond the margin ($y f(x) \ge 1$) and increases linearly for violations.

### Q7: Under what conditions is a Linear SVM mathematically equivalent to Logistic Regression?
**Answer:** Both are linear classifiers computing $f(\mathbf{x}) = \mathbf{w}^T \mathbf{x} + b$ with $L_2$ regularisation. They differ solely in their loss function: SVM optimizes Hinge Loss (producing a sparse dual solution where non-support vectors have zero loss), while Logistic Regression optimizes Log-Loss / Cross-Entropy (which assigns asymptotic non-zero gradient to every training sample). When points are far from the decision boundary, SVM assigns zero gradient while logistic regression still slightly updates weights.

### Q8: How does the RBF kernel bandwidth parameter $\gamma$ influence the bias-variance tradeoff?
**Answer:** The RBF kernel is $K(\mathbf{x}, \mathbf{z}) = \exp(-\gamma \|\mathbf{x} - \mathbf{z}\|^2)$ where $\gamma = \frac{1}{2\sigma^2}$. As $\gamma \to \infty$, $\sigma \to 0$, meaning the Gaussian bell curve becomes infinitely narrow. In this regime, $K(\mathbf{x}_i, \mathbf{x}_j) \to 0$ for all $i \neq j$, causing each training sample to become its own support vector. This maximizes variance (severe overfitting). As $\gamma \to 0$, the kernel approaches a constant value $1$, causing the model to lose discriminatory power and underfit (high bias).

---

## 21. Hands-On Exercises & Mini-Projects

1. **Custom Polynomial Kernel from Scratch**: Extend `ScratchSVC` to support polynomial kernels $K(\mathbf{x}, \mathbf{z}) = (\gamma \mathbf{x}^T \mathbf{z} + c_0)^d$ with arbitrary degree $d$. Verify that $d=1$ exactly matches the linear kernel output.
2. **Support Vector Pruning Experiment**: Train an `SVC` on a synthetic dataset. Write a script that progressively prunes support vectors with the smallest $\alpha_i$ and measure how degradation in accuracy correlates with inference latency speedup.
3. **Linear SVM Weight Visualisation**: Train a `LinearSVC` on the classic Wine or Iris dataset. Plot a horizontal bar chart of the primal coefficients $\mathbf{w}$ to identify which botanical dimensions contribute most positively and negatively to class separation.
4. **Nystroem Kernel Approximation Benchmark**: Compare training time and ROC-AUC of an exact `SVC(kernel='rbf')` against a pipeline combining `sklearn.kernel_approximation.Nystroem` + `SGDClassifier(loss='hinge')` across sample sizes $n \in \{1000, 5000, 20000, 50000\}$.
5. **Platt Scaling vs Isotonic Regression**: Fit an uncalibrated SVM on an imbalanced classification problem. Compare calibration curves (reliability diagrams) between Platt scaling (`CalibratedClassifierCV(method='sigmoid')`) and non-parametric isotonic regression (`method='isotonic'`).
6. **Robustness to Extreme Outliers**: Add 5 extreme mislabeled outliers to a clean dataset. Fit both an $L_2$-regularized Logistic Regression and an SVM with varying $C$ values. Plot how the decision boundaries shift to demonstrate SVM's localized margin insensitivity compared to global log-loss shifting.
