---
title: "Logistic Regression: Binary and Multinomial Classification"
description: "Comprehensive mathematical derivation of the sigmoid function, log-loss cross-entropy, Newton-Raphson optimization, Softmax, and production classification pipelines."
track: "machine-learning"
module: "04-supervised-classification"
topic: "Logistic Regression"
order: 1
format: "algorithm"
level: "beginner"
tags: ["logistic-regression", "sigmoid", "cross-entropy", "log-loss", "newton-raphson", "softmax", "classification"]
prerequisites: [
  "05-ai/machine-learning/01-math-foundations/02-calculus-and-gradients.md",
  "05-ai/machine-learning/01-math-foundations/03-probability-and-distributions.md",
  "05-ai/machine-learning/03-supervised-regression/01-linear-regression.md"
]
key_takeaways:
  - "Logistic Regression models the log-odds of a binary event as a linear combination of features, mapping real-valued logits to probabilities via the sigmoid function."
  - "The Cross-Entropy (Log-Loss) objective function is globally convex with respect to model weights w, guaranteeing convergence to a global minimum."
  - "Coefficients represent multiplicative changes in the Odds Ratio: a 1-unit increase in x_j multiplies the odds of y=1 by exp(w_j)."
  - "Multinomial Logistic Regression generalizes binary classification to K mutually exclusive classes using the Softmax activation function."
---

# Logistic Regression: Binary and Multinomial Classification

## 1. Algorithm Card

```
+-----------------------------------------------------------------------------------+
|                              ALGORITHM SPECIFICATION                              |
+-----------------------------------------------------------------------------------+
| Paradigm:         Supervised Learning (Parametric Probabilistic Classification)   |
| Family:           Generalized Linear Models (Bernoulli / Categorical Distribution)|
| Input Space:      X in R^{N x D} (Continuous, Categorical, Standardized)          |
| Output Space:     Binary: y in {0, 1} or p in [0, 1]; Multiclass: y in {0..K-1}   |
| Activation:       Binary: Sigmoid sigma(z); Multiclass: Softmax                   |
| Objective:        Binary Cross-Entropy (Log-Loss): -1/N * sum [y*ln(p) + (1-y)*ln(1-p)]|
| Optimization:     Quasi-Newton (L-BFGS), Newton-CG, or LibLinear (Coordinate Desc)|
| Time Complexity:  Training: O(T * N * D) (First-order) or O(T * N * D^2) (Second) |
|                   Inference: O(D) dot product + scalar sigmoid                    |
| Space Complexity: O(D) for binary; O(K * D) for K-class multinomial               |
| Key Assumptions:  Linearity of log-odds; independent observations; no separation  |
| Invariances:      NOT scale-invariant when regularized; requires StandardScaler   |
+-----------------------------------------------------------------------------------+
```

---

## 2. Architecture & Intuition

Ordinary linear regression predicts unbounded continuous numbers ($y \in (-\infty, \infty)$). When predicting probabilities, this creates critical failure modes: linear predictions can exceed $1.0$ (e.g., $1.42$) or drop below $0.0$ (e.g., $-0.25$), violating probability axioms.
**Logistic Regression** models the posterior probability $P(Y = 1 \mid \mathbf{x})$ by passing the linear predictor $z = \mathbf{w}^T \mathbf{x} + b$ through the non-linear **Sigmoid (logistic) function** $\sigma(z)$:

$$p = P(Y = 1 \mid \mathbf{x}) = \sigma(\mathbf{w}^T \mathbf{x} + b) = \frac{1}{1 + e^{-(\mathbf{w}^T \mathbf{x} + b)}}$$

### Visual Mental Model: Linear Log-Odds to S-Curve Probabilities

```
          Probability p = P(y=1)
               ^
           1.0 |                              +--------+ (Saturated p ~ 1)
               |                            /
               |                           /
           0.5 |--------------------------+ (Decision Threshold at z = 0)
               |                         /
               |                        /
           0.0 +--------+--------------+----------------------> Logit z = w^T x + b
                       -4             0             +4
```

The decision boundary is the hyperplane in feature space where the predicted probability equals $0.50$, which corresponds precisely to the linear condition:
$$\mathbf{w}^T \mathbf{x} + b = 0$$

---

## 3. Theoretical Foundations & Mathematical Derivation

### 3.1 The Logit Function and Odds Ratio

The **Odds** of an event is the ratio of the probability of occurrence to the probability of non-occurrence:

$$\text{Odds} = \frac{p}{1 - p} = \frac{P(Y = 1 \mid \mathbf{x})}{P(Y = 0 \mid \mathbf{x})}$$

Taking the natural logarithm of the odds yields the **Logit** function:

$$\text{logit}(p) = \ln\left( \frac{p}{1 - p} \right) = \mathbf{w}^T \mathbf{x} + b$$

Inverting the logit recovers the Sigmoid function:
$$\frac{p}{1 - p} = e^{\mathbf{w}^T \mathbf{x} + b} \implies p = \frac{e^{\mathbf{w}^T \mathbf{x} + b}}{1 + e^{\mathbf{w}^T \mathbf{x} + b}} = \frac{1}{1 + e^{-(\mathbf{w}^T \mathbf{x} + b)}}$$

#### Mathematical Property: Derivative of the Sigmoid
$$\sigma'(z) = \frac{d}{dz} \left( \frac{1}{1 + e^{-z}} \right) = \frac{e^{-z}}{(1 + e^{-z})^2} = \left( \frac{1}{1 + e^{-z}} \right) \left( 1 - \frac{1}{1 + e^{-z}} \right) = \sigma(z) (1 - \sigma(z))$$

### 3.2 Maximum Likelihood Estimation & Log-Loss

Given $N$ independent training pairs $\{(\mathbf{x}_i, y_i)\}_{i=1}^N$ with $y_i \in \{0, 1\}$.
Under the Bernoulli distribution, the probability of observing label $y_i$ given prediction $p_i = \sigma(\mathbf{w}^T \mathbf{x}_i)$ is:

$$P(y_i \mid \mathbf{x}_i; \mathbf{w}) = p_i^{y_i} (1 - p_i)^{1 - y_i}$$

The likelihood function over the entire dataset is:
$$L(\mathbf{w}) = \prod_{i=1}^N p_i^{y_i} (1 - p_i)^{1 - y_i}$$

Taking the natural logarithm yields the Log-Likelihood:
$$\ell(\mathbf{w}) = \sum_{i=1}^N \left[ y_i \ln(p_i) + (1 - y_i) \ln(1 - p_i) \right]$$

To formulate this as a minimization problem, we define the **Binary Cross-Entropy Loss (Log-Loss)** as the negative average log-likelihood:

$$J(\mathbf{w}) = -\frac{1}{N} \ell(\mathbf{w}) = -\frac{1}{N} \sum_{i=1}^N \left[ y_i \ln(p_i) + (1 - y_i) \ln(1 - p_i) \right]$$

### 3.3 Gradient and Hessian Derivation

To compute $\nabla_{\mathbf{w}} J(\mathbf{w})$, apply the chain rule:
$$\frac{\partial J}{\partial w_j} = -\frac{1}{N} \sum_{i=1}^N \left[ \frac{y_i}{p_i} \frac{\partial p_i}{\partial w_j} - \frac{1 - y_i}{1 - p_i} \frac{\partial p_i}{\partial w_j} \right]$$
Since $\frac{\partial p_i}{\partial w_j} = \sigma'(z_i) x_{ij} = p_i (1 - p_i) x_{ij}$:
$$\frac{\partial J}{\partial w_j} = -\frac{1}{N} \sum_{i=1}^N \left[ \frac{y_i}{p_i} p_i (1 - p_i) x_{ij} - \frac{1 - y_i}{1 - p_i} p_i (1 - p_i) x_{ij} \right] = \frac{1}{N} \sum_{i=1}^N (p_i - y_i) x_{ij}$$

In compact matrix notation:
$$\nabla_{\mathbf{w}} J(\mathbf{w}) = \frac{1}{N} X^T (\mathbf{p} - \mathbf{y})$$

Computing the second derivative yields the **Hessian Matrix** $H \in \mathbb{R}^{D \times D}$:
$$H = \nabla_{\mathbf{w}}^2 J(\mathbf{w}) = \frac{1}{N} X^T W X$$
where $W = \text{diag}(p_1(1 - p_1), \dots, p_N(1 - p_N))$.
Since $p_i(1 - p_i) > 0$ for all valid probabilities, $W$ is strictly positive definite. Consequently, the Hessian $H$ is positive semi-definite, proving that the **Log-Loss surface is strictly convex with a unique global minimum**.

### 3.4 Multinomial Logistic Regression (Softmax)

For $K > 2$ mutually exclusive classes $c \in \{0, 1, \dots, K-1\}$, we assign a parameter vector $\mathbf{w}_k$ to each class:

$$P(Y = k \mid \mathbf{x}) = \frac{e^{\mathbf{w}_k^T \mathbf{x}}}{\sum_{j=0}^{K-1} e^{\mathbf{w}_j^T \mathbf{x}}}$$

The Multiclass Categorical Cross-Entropy objective is:
$$J(W) = -\frac{1}{N} \sum_{i=1}^N \sum_{k=0}^{K-1} \mathbb{I}(y_i = k) \ln(P(Y = k \mid \mathbf{x}_i))$$

---

## 4. Computational Complexity Analysis

| Stage | First-Order (GD / L-BFGS) | Second-Order (Newton-Raphson) | Inference (Single Sample) |
| :--- | :--- | :--- | :--- |
| **Gradient Evaluation** | $\mathcal{O}(N \cdot D)$ | $\mathcal{O}(N \cdot D)$ | N/A |
| **Hessian Evaluation** | None ($\mathcal{O}(m D)$ rank-2 updates) | $\mathcal{O}(N \cdot D^2)$ | N/A |
| **Parameter Update Step** | $\mathcal{O}(D)$ | $\mathcal{O}(D^3)$ (Matrix inversion) | N/A |
| **Total per Iteration** | $\mathcal{O}(N \cdot D)$ | $\mathcal{O}(N D^2 + D^3)$ | $\mathcal{O}(D)$ dot product |

---

## 5. Inductive Bias & Core Assumptions

1. **Linear Decision Boundary**: Assumes classes are separated by a linear hyperplane in feature space. Non-linear separations require polynomial features or kernel extensions.
2. **Strict Independence**: Observations are assumed to be conditionally independent given $\mathbf{x}$.
3. **Absence of Perfect Separation**: If a linear hyperplane can separate the classes with 100% accuracy, the likelihood function has no finite maximum: weights $\|\mathbf{w}\|_2 \to \infty$ as the algorithm tries to force predicted probabilities to exactly $0.0$ and $1.0$.
   *Remedy*: $L_2$ regularization (Ridge penalty $\frac{1}{2C} \|\mathbf{w}\|_2^2$) is strictly required to keep weights bounded.

---

## 6. From-Scratch NumPy Implementation

```python
"""
Pure NumPy Implementation of Binary Logistic Regression.
Supports Gradient Descent, L2 Regularization, and Probability Calibration.
Tested on Python 3.11, NumPy 2.x.
"""

from typing import Tuple
import numpy as np


class PureNumPyLogisticRegression:
    """Binary Logistic Regression with L2 regularization."""

    def __init__(self, lr: float = 0.1, max_iter: int = 1000, C: float = 1.0, tol: float = 1e-5):
        self.lr = lr
        self.max_iter = max_iter
        self.C = C # C = 1 / lambda (Inverse regularization strength)
        self.tol = tol
        self.coef_: np.ndarray = np.array([])
        self.intercept_: float = 0.0
        self.loss_history_: list = []

    def _sigmoid(self, z: np.ndarray) -> np.ndarray:
        # Numerically stable sigmoid avoiding exp overflow
        return np.where(
            z >= 0,
            1.0 / (1.0 + np.exp(-z)),
            np.exp(z) / (1.0 + np.exp(z))
        )

    def fit(self, X: np.ndarray, y: np.ndarray):
        assert X.ndim == 2, "X must be 2D array (N, D)."
        assert set(np.unique(y)).issubset({0, 1}), "y must be binary {0, 1}."
        N, D = X.shape

        # Initialize weights
        w = np.zeros(D)
        b = 0.0
        reg_lambda = 1.0 / self.C if self.C > 0 else 0.0

        for epoch in range(self.max_iter):
            z = X @ w + b
            p = self._sigmoid(z)

            # Compute cross-entropy loss + L2 regularization penalty
            # Clip probabilities to prevent log(0)
            p_clipped = np.clip(p, 1e-15, 1.0 - 1e-15)
            loss = -np.mean(y * np.log(p_clipped) + (1.0 - y) * np.log(1.0 - p_clipped))
            loss += 0.5 * reg_lambda * np.sum(w ** 2)
            self.loss_history_.append(float(loss))

            # Gradients
            errors = p - y
            grad_w = (X.T @ errors) / N + reg_lambda * w
            grad_b = np.mean(errors)

            w_prev = w.copy()
            w -= self.lr * grad_w
            b -= self.lr * grad_b

            if np.linalg.norm(w - w_prev) < self.tol:
                break

        self.coef_ = w
        self.intercept_ = b
        return self

    def predict_proba(self, X: np.ndarray) -> np.ndarray:
        z = X @ self.coef_ + self.intercept_
        p1 = self._sigmoid(z)
        p0 = 1.0 - p1
        return np.column_stack([p0, p1])

    def predict(self, X: np.ndarray, threshold: float = 0.5) -> np.ndarray:
        p1 = self.predict_proba(X)[:, 1]
        return (p1 >= threshold).astype(int)


# Verification Run
if __name__ == "__main__":
    np.random.seed(42)
    N = 400
    X = np.random.randn(N, 2)
    # Linearly separable with small noise
    logits = 2.5 * X[:, 0] - 1.8 * X[:, 1] + 0.5
    probs = 1.0 / (1.0 + np.exp(-logits))
    y = np.random.binomial(1, probs)

    clf = PureNumPyLogisticRegression(lr=0.5, max_iter=1500, C=10.0)
    clf.fit(X, y)

    preds = clf.predict(X)
    accuracy = np.mean(preds == y)

    print("--- Pure NumPy Logistic Regression ---")
    print(f"Learned Weights:   {clf.coef_.round(4)} (Expected ~ [2.5, -1.8])")
    print(f"Learned Intercept: {clf.intercept_:.4f} (Expected ~ 0.5)")
    print(f"Training Accuracy: {accuracy * 100:.2f}%")

    assert accuracy > 0.85
    assert np.all(clf.predict_proba(X) >= 0.0)
    assert np.allclose(np.sum(clf.predict_proba(X), axis=1), 1.0)
    print("[SUCCESS] Pure NumPy Logistic Regression verified.")
```

---

## 7. Production-Grade Scikit-Learn Implementation

```python
"""
Production Pipeline: LogisticRegression with ColumnTransformer and StandardScaler.
Demonstrates Hyperparameter Search and Evaluation Metrics.
"""

import numpy as np
import pandas as pd
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import LogisticRegression
from sklearn.model_selection import train_test_split, GridSearchCV
from sklearn.metrics import classification_report, roc_auc_score


def run_production_logistic():
    np.random.seed(42)
    N = 800
    X = np.random.randn(N, 4)
    y = (X[:, 0] + 1.5 * X[:, 1] - 0.8 * X[:, 2] > 0).astype(int)

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    pipeline = Pipeline([
        ("scaler", StandardScaler()), # MANDATORY FOR REGULARIZED LOGISTIC REGRESSION
        ("classifier", LogisticRegression(solver="lbfgs", max_iter=500, random_state=42))
    ])

    param_grid = {
        "classifier__C": [0.01, 0.1, 1.0, 10.0],
        "classifier__class_weight": [None, "balanced"]
    }

    grid = GridSearchCV(pipeline, param_grid=param_grid, cv=5, scoring="roc_auc")
    grid.fit(X_train, y_train)

    best_model = grid.best_estimator_
    print("--- Logistic Regression Production Report ---")
    print(f"Best Hyperparameters: {grid.best_params_}")

    y_probs = best_model.predict_proba(X_test)[:, 1]
    auc_score = roc_auc_score(y_test, y_probs)
    print(f"Test ROC-AUC: {auc_score:.4f}")
    assert auc_score > 0.90


if __name__ == "__main__":
    run_production_logistic()
```

---

## 8. Model Persistence & Safe Serialization

```python
import joblib
import tempfile
import os

def persist_logistic(pipeline):
    with tempfile.TemporaryDirectory() as tmp_dir:
        path = os.path.join(tmp_dir, "logistic_pipeline.joblib")
        joblib.dump(pipeline, path)
        loaded = joblib.load(path)
        assert np.array_equal(
            pipeline.named_steps["classifier"].coef_,
            loaded.named_steps["classifier"].coef_
        )
        print("[SUCCESS] Logistic Regression serialized safely.")
```

---

## 9. Hyperparameter Tuning & Optimization

| Hyperparameter | Search Space | Default | Practical Guidance |
| :--- | :--- | :--- | :--- |
| `C` | `np.logspace(-4, 4, 20)` | `1.0` | Inverse regularization strength ($C = 1/\lambda$). Smaller $C$ prevents overfitting. |
| `penalty` | `['l1', 'l2', 'elasticnet']`| `'l2'` | $L_1$ for feature selection; $L_2$ for standard stability. |
| `solver` | `['lbfgs', 'liblinear', 'saga']`| `'lbfgs'` | `'lbfgs'` for dense $L_2$; `'saga'` for large $N$ and ElasticNet. |
| `class_weight` | `[None, 'balanced']` | `None` | Use `'balanced'` if dataset exhibits class imbalance. |

---

## 10. Diagnostic Evaluations & Calibration Curves

```
+-----------------------------------------------------------------------------------+
|                        LOGISTIC DIAGNOSTIC SIGNALS                                |
+-----------------------------------------------------------------------------------+
| 1. Calibration Curve (Reliability Diagram): Binned predicted probabilities vs     |
|    empirical observed positive frequencies. Should follow the 45-degree diagonal. |
| 2. Brier Score: Mean squared error of probabilities: (1/N) * sum (p_i - y_i)^2.   |
| 3. ROC-AUC vs PR-AUC: Check if high ROC-AUC masks poor minority precision.        |
+-----------------------------------------------------------------------------------+
```

---

## 11. Calibration, Uncertainty & Prediction Intervals

Logistic Regression natively outputs calibrated posterior probabilities:
$$P(Y = 1 \mid \mathbf{x}) = \sigma(\mathbf{w}^T \mathbf{x} + b)$$
Because Log-Loss is a strictly proper scoring rule, unregularized maximum likelihood estimates are asymptotically well-calibrated.
Under strong regularization (small $C$), probabilities are slightly shrunk toward 0.50; apply **Platt Scaling** or **Isotonic Regression** to recalibrate.

---

## 12. Edge Cases, Failure Modes & Remediation

| Failure Mode | Root Cause | Observable Symptom | Engineering Remediation |
| :--- | :--- | :--- | :--- |
| **Complete Separation** | Features linearly separate classes perfectly | Coefficients explode to $\pm \infty$; solver fails to converge | Add $L_2$ regularization (`C=1.0` or smaller). |
| **Unscaled Features** | Continuous feature scale discrepancy | Small features get over-regularized; slow convergence | Prepend `StandardScaler()` in an `sklearn.pipeline.Pipeline`. |
| **Multicollinear Redundancy** | High VIF among predictors | Massive standard errors; coefficient signs invert | Apply $L_1$ penalty (`penalty='l1'`, `solver='saga'`) or ElasticNet. |
| **Non-Linear Decision Boundary** | Target separated by circular manifold ($x_1^2 + x_2^2 \le r^2$) | Logistic regression accuracy collapses to $\approx 50\%$ | Add `PolynomialFeatures(degree=2)` or use Kernel SVM. |
| **Default Threshold Inappropriateness** | Extreme class imbalance (1:1000 fraud) | Threshold 0.50 predicts all zeros | Calibrate decision threshold $\theta$ to maximize F-1 / utility. |

---

## 13. Comparative Analysis: Logistic Regression vs Alternatives

| Dimension | Logistic Regression | Support Vector Machine (SVC) | Random Forest Classifier | Neural Network (MLP) |
| :--- | :--- | :--- | :--- | :--- |
| **Output Type** | Direct Probabilities | Raw Margins (Uncalibrated) | Ensemble Tree Vote | Probabilities via Softmax |
| **Interpretability** | Exact Odds Ratios | Moderate (Support vectors) | Low (Tree ensemble) | Black-box weights |
| **Inference Latency** | Ultra-Fast ($\sim \text{ns}$) | Fast ($\mathcal{O}(N_{\text{SV}} \cdot D)$)| Fast ($\mathcal{O}(B \cdot \text{depth})$)| Moderate |
| **Non-Linearity** | Linear only | Via Kernel Trick | Native Non-linear | Native Non-linear |

---

## 14. Production Engineering & Serving Considerations

1. **Ultra-High-Throughput Scoring ($< 50\text{ nanoseconds}$)**:
   In ad click bidding engines handling $1,000,000\text{ req/sec}$, compute the logit via dot product and look up $\sigma(z)$ in a precomputed $10,000$-entry float array, avoiding expensive `exp()` math calls.
2. **Probability Threshold Shifting**:
   Store the operating threshold $\theta$ as a dynamic configuration variable in Redis / ZooKeeper:
   $$\text{Decision} = \mathbb{I}(p \ge \theta_{\text{dynamic}})$$
   This allows fraud teams to tighten or loosen detection sensitivity in real time without redeploying code.

---

## 15. Interpretability & Feature Attribution

The coefficient $w_j$ represents the change in **log-odds** per unit increase in $x_j$:
$$\text{Odds Ratio (OR)} = \exp(w_j)$$
- If $w_j = +0.693 \implies \text{OR} = e^{0.693} \approx 2.0$: A 1-unit increase in $x_j$ **doubles the odds** of the positive outcome.
- If $w_j = -0.693 \implies \text{OR} = e^{-0.693} \approx 0.5$: A 1-unit increase in $x_j$ **halves the odds**.

---

## 16. Hardware & Distributed Scaling

In Apache Spark MLlib, `LogisticRegression` uses distributed L-BFGS: worker nodes compute gradient vectors $\sum x_i(p_i - y_i)$ across local partitions, and the driver updates parameter vectors via tree aggregation.

---

## 17. Verification & Diagnostics Test Harness

```python
import pytest
import numpy as np
from sklearn.linear_model import LogisticRegression

def test_logistic_odds_ratio_invariance():
    # Synthetic dataset with known single feature impact
    X = np.array([[0.0], [1.0], [2.0], [3.0], [4.0]])
    y = np.array([0, 0, 1, 1, 1])
    clf = LogisticRegression(penalty=None, solver="lbfgs")
    clf.fit(X, y)
    p0 = clf.predict_proba([[0.0]])[0, 1]
    p1 = clf.predict_proba([[1.0]])[0, 1]
    odds0 = p0 / (1.0 - p0)
    odds1 = p1 / (1.0 - p1)
    # odds1 / odds0 must equal exp(coef)
    assert np.isclose(odds1 / odds0, np.exp(clf.coef_[0, 0]), atol=1e-4)
```

---

## 18. Cross-Track Connections

- **05-ai/machine-learning/01-math-foundations/05-information-theory.md**: Cross-entropy and Kullback-Leibler divergence.
- **05-ai/machine-learning/03-supervised-regression/09-generalised-linear-models.md**: Logistic regression as a Binomial GLM with Logit link.
- **05-ai/machine-learning/04-supervised-classification/10-probability-calibration.md**: Brier scores and calibration curves.

---

## 19. Real-World Case Study: Hospital Sepsis Alert Triage

An intensive care unit deployed a real-time sepsis alert system.
- **Requirement**: Clinical staff required exact probability outputs ($P(\text{sepsis}) \in [0, 1]$) with interpretable risk factors to justify invasive antibiotic administration.
- **Solution**:
  1. Trained a `LogisticRegression(class_weight='balanced', C=0.5)` on standardized vital signs.
  2. Verified calibration using Brier score ($0.082$) and reliability diagrams.
  3. Deployed the model in the hospital EHR system.
  4. Physicians received alerts with exact Odds Ratios (*"Heart rate > 110 bpm increases sepsis odds by 2.4x"*), achieving an AUROC of 0.88 and reducing septic shock mortality by 18%.

---

## 20. Technical Interview Questions & Solutions

### Question 1: Derive the gradient of Binary Cross-Entropy Loss with respect to parameter vector $\mathbf{w}$.
**Answer**:
See Section 3.3 for the complete step-by-step derivation.
1. The loss is $J(\mathbf{w}) = -\frac{1}{N} \sum [y_i \ln(p_i) + (1 - y_i) \ln(1 - p_i)]$.
2. Note that $\frac{\partial p_i}{\partial z_i} = p_i (1 - p_i)$ and $\frac{\partial z_i}{\partial w_j} = x_{ij}$.
3. Differentiating via chain rule: $\frac{\partial J}{\partial w_j} = \frac{1}{N} \sum_{i=1}^N (p_i - y_i) x_{ij}$.
4. In vector form: $\nabla_{\mathbf{w}} J = \frac{1}{N} X^T (\mathbf{p} - \mathbf{y})$.

### Question 2: Why can't we use Mean Squared Error (MSE) loss for Logistic Regression?
**Answer**:
If we pair the non-linear Sigmoid activation $p = \sigma(\mathbf{w}^T \mathbf{x})$ with Mean Squared Error loss $J(\mathbf{w}) = \frac{1}{2N} \sum (y_i - \sigma(\mathbf{w}^T \mathbf{x}_i))^2$:
1. **Non-Convexity**: The derivative contains the term $\sigma'(z) = \sigma(z)(1 - \sigma(z))$. The resulting loss surface is **non-convex with multiple local minima and saddle points**, preventing gradient descent from finding the global optimum.
2. **Gradient Vanishing**: When the model makes a confident wrong prediction (e.g., $y=1$ but $z = -10 \implies p \approx 0$), the term $\sigma'(z) \to 0$. The gradient vanishes, and the model ceases learning precisely when it is most wrong! In contrast, under Cross-Entropy loss, the gradient is $(p - y) x$, which is large when the error is large.

### Question 3: What is "Complete Separation", and why does unregularized Logistic Regression fail on separable data?
**Answer**:
Complete separation occurs when there exists a linear hyperplane that classifies 100% of training instances correctly:
$$\mathbf{w}^T \mathbf{x}_i > 0 \quad (\forall y_i = 1) \quad \text{and} \quad \mathbf{w}^T \mathbf{x}_i < 0 \quad (\forall y_i = 0)$$
The cross-entropy loss is strictly positive and approaches zero **only as** $p_i \to 1$ (for $y=1$) and $p_i \to 0$ (for $y=0$).
Because $p = \sigma(\mathbf{w}^T \mathbf{x})$, achieving $p \to 1$ requires $\mathbf{w}^T \mathbf{x} \to \infty$.
Gradient descent will continuously push the magnitude of the weights $\|\mathbf{w}\|_2 \to \infty$, causing parameter values to explode without ever reaching a finite minimum.
*Solution*: Add an $L_2$ regularization penalty $\frac{1}{2C} \|\mathbf{w}\|_2^2$, which bounds weight magnitudes.

### Question 4: How does Multinomial Logistic Regression (Softmax) ensure probabilities sum to $1.0$?
**Answer**:
The Softmax function normalizes the exponentials of the linear scores across all $K$ classes:
$$P(Y = k \mid \mathbf{x}) = \frac{e^{z_k}}{\sum_{j=0}^{K-1} e^{z_j}} \quad \text{where } z_k = \mathbf{w}_k^T \mathbf{x}$$
Since $e^{z_k} > 0$ for all real $z_k$, every probability is strictly positive.
Summing across all $K$ classes:
$$\sum_{k=0}^{K-1} P(Y = k \mid \mathbf{x}) = \frac{\sum_{k=0}^{K-1} e^{z_k}}{\sum_{j=0}^{K-1} e^{z_j}} = 1.0$$
It forms a mathematically valid discrete probability distribution over the simplex $\Delta^{K-1}$.

### Question 5: How do you interpret an Odds Ratio of $1.0$, $2.5$, and $0.40$?
**Answer**:
The Odds Ratio is $\text{OR} = e^{w_j}$:
- $\text{OR} = 1.0$ ($w_j = 0$): Feature $x_j$ has **zero effect** on the odds of the outcome.
- $\text{OR} = 2.5$ ($w_j \approx +0.916$): A 1-unit increase in $x_j$ increases the odds of $Y=1$ by $150\%$ (multiplies odds by $2.5$).
- $\text{OR} = 0.40$ ($w_j \approx -0.916$): A 1-unit increase in $x_j$ reduces the odds of $Y=1$ by $60\%$ (multiplies odds by $0.40$).

### Question 6: What is the Brier Score?
**Answer**:
The Brier score measures the accuracy of probabilistic predictions:
$$\text{Brier} = \frac{1}{N} \sum_{i=1}^N (p_i - y_i)^2$$
It is the Mean Squared Error between predicted probabilities $p_i \in [0, 1]$ and actual binary outcomes $y_i \in \{0, 1\}$.
Lower Brier score indicates better calibrated and sharper probabilities. A dummy model predicting 0.5 achieves a Brier score of 0.25.

### Question 7: Why is L-BFGS the preferred solver for dense Logistic Regression in scikit-learn?
**Answer**:
Full Newton-Raphson requires computing and inverting the $D \times D$ Hessian matrix, taking $\mathcal{O}(D^3)$ time and $\mathcal{O}(D^2)$ memory.
**L-BFGS (Limited-memory Broyden-Fletcher-Goldfarb-Shanno)** is a Quasi-Newton method that approximates the inverse Hessian using only the last $m$ (typically $m=10$) gradient vectors. It achieves super-linear convergence in $\mathcal{O}(m \cdot D)$ space and time per iteration, scaling effectively to thousands of features.

### Question 8: How do you handle non-linear decision boundaries with Logistic Regression?
**Answer**:
Apply non-linear basis expansions:
1. Construct polynomial features (`PolynomialFeatures(degree=2)`).
2. Construct radial basis function features using `RBFSampler` or `Nystroem`.
3. Discretize continuous features into bins using `KBinsDiscretizer(encode='onehot')`.

---

## 21. Guided Practice & Hands-On Exercises

### Exercise 1: Stable Sigmoid Evaluator
- Implement a numerically stable sigmoid function in NumPy that prevents overflow warnings for inputs $z \in [-1000, 1000]$.

### Exercise 2: Binary Log-Loss Surface Plotter
- Generate a 2-sample dataset. Plot the 3D surface of Binary Cross-Entropy loss vs weights $(w_1, w_2)$, verifying its convex bowl geometry.

### Exercise 3: Complete Separation Weight Explosion Experiment
- Generate a 100% linearly separable dataset. Fit `LogisticRegression(penalty=None)` and record how weights grow as `max_iter` increases from 10 to 500. Then fit with `C=1.0` and demonstrate weight stabilization.

### Exercise 4: Reliability Calibration Curve Generator
- Implement a function `plot_calibration_curve(y_true, y_probs, n_bins=10)` in pure NumPy and compute the Brier score.

### Exercise 5: Odds Ratio Confidence Interval Extractor
- Extract the covariance matrix of estimated parameters $(X^T W X)^{-1}$ and compute 95% Wald confidence intervals for the Odds Ratios $\exp(w_j \pm 1.96 \cdot \text{SE}(w_j))$.

### Exercise 6: Softmax Multi-Class Classifier from Scratch
- Implement a 3-class Multinomial Logistic Regression model using Softmax and vectorized Categorical Cross-Entropy in pure NumPy.
