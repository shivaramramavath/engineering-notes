# Cost-Sensitive Classification

> **Track:** `05-ai/machine-learning` | **Module:** `04-supervised-classification` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `NumPy: >=1.26.0` | `Python: >=3.11`  
> **Type:** `Core Concept Architecture & Engineering Guide`

---

## 1. Concept Definition & Purpose

Traditional machine learning algorithms optimize symmetric loss functions (0-1 loss, cross-entropy, mean squared error) under the implicit assumption that all classification errors carry equal real-world consequence. In production engineering, this assumption is almost universally false.

In **Cost-Sensitive Classification**, the optimization objective directly incorporates an asymmetric **Cost Matrix** $C$, where the economic, operational, or safety penalty of misclassifying an instance of class $i$ as class $j$ ($C(i, j)$) is explicitly modeled.

```
                     Actual Positive (y=1)       Actual Negative (y=0)
                 +---------------------------+---------------------------+
Predict Pos (1)  | C(1, 1) = $0              | C(0, 1) = $50             |
                 | (True Positive / Benefit) | (False Alarm Cost)        |
                 +---------------------------+---------------------------+
Predict Neg (0)  | C(1, 0) = $5,000          | C(0, 0) = $0              |
                 | (Catastrophic Miss Cost)  | (True Negative / Normal)  |
                 +---------------------------+---------------------------+
```

---

## 2. Theoretical Mechanics & Math Formulations

### 1. Cost Matrix & Bayes Risk Minimization
Let $C(i, j)$ denote the cost incurred when the true label is $y = i$ and the model predicts $\hat{y} = j$.
The **Conditional Expected Cost (Bayes Risk)** of predicting class $k$ given feature vector $\mathbf{x}$ is:

$$R(k \mid \mathbf{x}) = \sum_{i} P(y = i \mid \mathbf{x}) C(i, k)$$

By the **Bayes Decision Rule**, the optimal decision $\hat{y}^*$ chooses the action minimizing the expected conditional risk:

$$\hat{y}^*(\mathbf{x}) = \arg\min_k R(k \mid \mathbf{x})$$

For binary classification ($y \in \{0, 1\}$):
- Expected cost of predicting Positive ($\hat{y} = 1$):
  $$R(1 \mid \mathbf{x}) = P(y = 0 \mid \mathbf{x}) C(0, 1) + P(y = 1 \mid \mathbf{x}) C(1, 1)$$
- Expected cost of predicting Negative ($\hat{y} = 0$):
  $$R(0 \mid \mathbf{x}) = P(y = 0 \mid \mathbf{x}) C(0, 0) + P(y = 1 \mid \mathbf{x}) C(1, 0)$$

Assuming standard zero costs for correct classifications ($C(0, 0) = C(1, 1) = 0$):
$$\text{Predict } \hat{y} = 1 \iff R(1 \mid \mathbf{x}) \le R(0 \mid \mathbf{x})$$
$$(1 - P(y = 1 \mid \mathbf{x})) C(0, 1) \le P(y = 1 \mid \mathbf{x}) C(1, 0)$$

Solving for the theoretical optimal probability cutoff $\tau^*$:
$$P(y = 1 \mid \mathbf{x}) \ge \tau^* = \frac{C(0, 1)}{C(0, 1) + C(1, 0)} = \frac{C_{\text{FP}}}{C_{\text{FP}} + C_{\text{FN}}}$$

### 2. The Three Architectural Paradigms of Cost-Sensitivity

```
+--------------------------------------------------------------------------------------------------+
|                            THREE PARADIGMS OF COST-SENSITIVE LEARNING                            |
|                                                                                                  |
| 1. DATA-LEVEL (Resampling / Weighting)                                                           |
|    Translate asymmetric costs into sample weights w_i or resample data.                          |
|    - Advantage: Works with any black-box estimator.                                              |
|    - Mechanics: w_i = C(y_i, 1 - y_i).                                                           |
|                                                                                                  |
| 2. ALGORITHM-LEVEL (Cost-Weighted Loss Functions)                                                |
|    Modify the internal optimization objective directly.                                          |
|    - Mechanics: Loss = sum w_{y_i} * L(y_i, f(x_i)) via class_weight='balanced' or custom.      |
|    - Pushes optimization gradients toward high-cost samples.                                     |
|                                                                                                  |
| 3. POST-PROCESSING (Bayesian Threshold Shifting)                                                 |
|    Train a well-calibrated probabilistic model; shift decision threshold tau post-hoc.          |
|    - Advantage: Cleanest separation of concerns; costs can change dynamically at runtime         |
|      without retraining the model!                                                               |
+--------------------------------------------------------------------------------------------------+
```

### 3. Zadrozny & Elkan Theorem (Cost-Proportionate Rejection Sampling)
Zadrozny, Langford, and Elkan (2003) established that any standard cost-insensitive learning algorithm can be converted into an optimal cost-sensitive learner by drawing training instances with probability proportional to their misclassification cost:

$$P(\text{Sample } i \text{ is selected}) \propto C(y_i, 1 - y_i)$$

Alternatively, supplying sample weights $w_i = C(y_i, 1 - y_i)$ during standard empirical risk minimization achieves exact mathematical equivalence.

---

## 3. Architectural Topology & Flow Diagrams

```
+--------------------------------------------------------------------------------------------------+
|                          COST-SENSITIVE DECISION IMPLEMENTATION PIPELINE                         |
|                                                                                                  |
|                            Raw Ingestion Features & Labels (X, y)                                |
|                                                |                                                 |
|                                 Are business costs dynamic or static?                            |
|                                     /                      \                                     |
|                                [DYNAMIC]                 [STATIC]                                |
|                                   /                          \                                   |
|                      Train Standard Estimator          Are sample weights supported?             |
|                      with Probability Calibration             /             \                    |
|                                   |                        [YES]            [NO]                 |
|                            Calibrated Model                 /                 \                  |
|                                   |                 class_weight=C       Cost-Proportionate      |
|                      Evaluate at Runtime with       sample_weight=w      Rejection Resampling    |
|                      tau* = C_FP / (C_FP + C_FN)           \                 /                   |
|                                                              v              v                    |
|                                                      Cost-Weighted Estimator Model               |
+--------------------------------------------------------------------------------------------------+
```

---

## 4. Production Scikit-Learn Implementation

```python
"""
Production cost-sensitive classification framework.
Implements custom cost-matrix evaluation, sample-weighting, and threshold moving.
"""

from __future__ import annotations
import logging
from typing import Dict, Tuple
import numpy as np
from sklearn.datasets import make_classification
from sklearn.model_selection import train_test_split
from sklearn.ensemble import HistGradientBoostingClassifier
from sklearn.calibration import CalibratedClassifierCV
from sklearn.metrics import confusion_matrix

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("CostSensitive_Engine")


class CostMatrix:
    """Explicit business cost structure."""
    def __init__(self, cost_tp: float = 0.0, cost_fp: float = 50.0, cost_fn: float = 2000.0, cost_tn: float = 0.0) -> None:
        self.c_tp = cost_tp
        self.c_fp = cost_fp
        self.c_fn = cost_fn
        self.c_tn = cost_tn

    @property
    def theoretical_threshold(self) -> float:
        """Bayesian cost-optimal probability cutoff."""
        return self.c_fp / (self.c_fp + self.c_fn)

    def evaluate_cost(self, y_true: np.ndarray, y_pred: np.ndarray) -> float:
        """Compute cumulative dollar cost across all predictions."""
        tn, fp, fn, tp = confusion_matrix(y_true, y_pred).ravel()
        total_dollars = (tp * self.c_tp) + (fp * self.c_fp) + (fn * self.c_fn) + (tn * self.c_tn)
        return float(total_dollars)


def run_cost_sensitive_experiment() -> Dict[str, float]:
    # Cost structure: False Alarm = $50, Missed Severe Malfunction = $2,000
    costs = CostMatrix(cost_tp=0.0, cost_fp=50.0, cost_fn=2000.0, cost_tn=0.0)
    logger.info(f"Theoretical Cost-Optimal Cutoff: tau = {costs.theoretical_threshold:.4f}")

    # Synthesize industrial telemetry data (3% failure rate)
    X, y = make_classification(
        n_samples=10000,
        n_features=20,
        n_informative=10,
        weights=[0.97, 0.03],
        random_state=42
    )

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.30, stratify=y, random_state=42
    )

    # Strategy 1: Naive Symmetric Classifier (Default tau = 0.50)
    naive_clf = HistGradientBoostingClassifier(random_state=42).fit(X_train, y_train)
    y_pred_naive = naive_clf.predict(X_test)
    cost_naive = costs.evaluate_cost(y_test, y_pred_naive)
    logger.info(f"Strategy 1 (Default tau=0.50) Total Cost: ${cost_naive:,.2f}")

    # Strategy 2: Algorithm-Level Loss Weighting via class_weight
    # Ratio: C_FN / C_FP = 2000 / 50 = 40.0
    weighted_clf = HistGradientBoostingClassifier(
        class_weight={0: 1.0, 1: 40.0},
        random_state=42
    ).fit(X_train, y_train)
    y_pred_weighted = weighted_clf.predict(X_test)
    cost_weighted = costs.evaluate_cost(y_test, y_pred_weighted)
    logger.info(f"Strategy 2 (Algorithm class_weight=40) Total Cost: ${cost_weighted:,.2f}")

    # Strategy 3: Post-Hoc Bayesian Threshold Shifting with Calibrated Probabilities
    calibrated_clf = CalibratedClassifierCV(naive_clf, method="sigmoid", cv=5)
    calibrated_clf.fit(X_train, y_train)
    y_prob_calibrated = calibrated_clf.predict_proba(X_test)[:, 1]

    # Apply theoretical Bayesian threshold tau*
    tau_star = costs.theoretical_threshold
    y_pred_shifted = (y_prob_calibrated >= tau_star).astype(int)
    cost_shifted = costs.evaluate_cost(y_test, y_pred_shifted)
    logger.info(f"Strategy 3 (Calibrated Threshold Shifting tau={tau_star:.4f}) Total Cost: ${cost_shifted:,.2f}")

    cost_savings = cost_naive - cost_shifted
    pct_savings = (cost_savings / cost_naive) * 100.0
    logger.info(f"Cost Reduction Achieved: ${cost_savings:,.2f} ({pct_savings:.1f}%)")

    return {
        "cost_naive": cost_naive,
        "cost_weighted": cost_weighted,
        "cost_shifted": cost_shifted,
        "cost_savings_pct": pct_savings,
    }


if __name__ == "__main__":
    results = run_cost_sensitive_experiment()
    assert results["cost_shifted"] < results["cost_naive"], "Cost-sensitive shifting failed to reduce cost."
```

---

## 5. Edge Cases & Failure Modes

1. **Uncalibrated Probabilities in Threshold Shifting**:
   The mathematical identity $\tau^* = \frac{C_{\text{FP}}}{C_{\text{FP}} + C_{\text{FN}}}$ holds **strictly and exclusively** when predicted probabilities $\hat{p}(\mathbf{x})$ match true empirical posterior frequencies. If probabilities are uncalibrated (e.g., standard SVM or naive tree leaves), shifting to $\tau^*$ produces suboptimal, unpredictable operational costs.
2. **Extreme Class Weight Numerical Instability**:
   Setting `class_weight={1: 10000.0}` in gradient-based algorithms (logistic regression, neural networks) multiplies the loss gradient by $10,000$, causing exploding gradients, numerical overflow (`NaN`), and boundary collapse.
   - *Mitigation*: Cap class weights at reasonable bounds ($\le 100$) and handle residual asymmetry via post-hoc threshold shifting.
3. **Negative Costs (Revenue / Net Profit Formulations)**:
   When true positives generate profit ($C(1, 1) = -\$100$) while false alarms incur costs ($C(0, 1) = +\$20$), the objective switches from cost minimization to **Net Present Value (NPV) Maximization**. Ensure signs and decision rules are adjusted accordingly:
   $$\hat{y}^* = 1 \iff P(y = 1 \mid \mathbf{x}) \ge \frac{C_{\text{FP}}}{C_{\text{FP}} + \text{Profit}_{\text{TP}}}$$

---

## 6. Comparative Analysis Table

| Paradigm | Architectural Layer | Retraining Needed When Costs Change? | Calibration Dependent? | Scalability & Robustness |
| :--- | :--- | :--- | :--- | :--- |
| **Bayesian Threshold Shifting** | Post-Processing | **No** (Zero retraining; instantaneous runtime updates) | **Yes** (Strictly requires calibrated probabilities) | Maximum (Clean separation of concerns) |
| **Loss Function Reweighting** (`class_weight`) | Algorithm-Level | **Yes** (Must retrain entire model for every cost shift) | No | Moderate (Extreme weights destabilize gradients) |
| **Cost-Proportionate Resampling** | Data-Level | **Yes** (Must re-draw bootstrap sets and retrain) | No | Moderate (Increases sample size or discards data) |

---

## 7. Hyperparameter & Configuration Matrix

| Tool / Strategy | Parameter | Standard Configuration | Impact |
| :--- | :--- | :--- | :--- |
| `Estimator.fit()` | `sample_weight` | Array of float weights: $w_i = C(y_i, 1 - y_i)$ | Scales individual sample loss contributions during gradient updates. |
| `Estimator` | `class_weight` | Dictionary `{0: C_FP, 1: C_FN}` | Multiplies class loss penalty without modifying input dataset. |
| `CalibratedClassifierCV` | `method` | `'sigmoid'` or `'isotonic'` | Essential prerequisite before applying Bayesian threshold shifting. |

---

## 8. Diagnostics, Monitoring & Error Metrics

### Production Cost Tracking
1. **Total Realized Cost (TRC)**:
   $$\text{TRC} = \sum_{i=1}^n C(y_i, \hat{y}_i(\mathbf{x}_i))$$
2. **Normalized Cost Savings (NCS)**:
   Measures cost reduction relative to the trivial baseline (predicting all negative or all positive):
   $$\text{NCS} = \frac{\text{Cost}_{\text{baseline}} - \text{Cost}_{\text{model}}}{\text{Cost}_{\text{baseline}}}$$
3. **Cost Curves (Drummond & Holte)**:
   Plots expected cost against the probability-cost factor $P(+) C(1, 0) / (P(+) C(1, 0) + P(-) C(0, 1))$, demonstrating the operating range over which the model outperforms trivial policies.

---

## 9. Production Engineering & Serving Considerations

- **Dynamic Cost Injectors**: In fraud detection, the cost of a missed fraud $C_{\text{FN}}$ is not static—it equals the transaction amount ($C_{\text{FN}} = \text{Amount}$). A static model cannot handle transaction-varying costs via `class_weight`. Instead, evaluate individual transactions dynamically:
  $$\tau^*(\text{transaction}) = \frac{C_{\text{review}}}{\text{Transaction Amount} + C_{\text{review}}}$$
  Transactions for $\$10,000$ trigger alerts at $\tau^* = 0.005$, while $\$5$ transactions require $\tau^* = 0.80$.

---

## 10. Automated Verification Harness

```python
"""
Pytest automated verification harness for Cost-Sensitive classification.
Verifies cost monotonicity and Bayesian threshold optimality.
"""

import numpy as np
import pytest
from sklearn.datasets import make_classification
from sklearn.linear_model import LogisticRegression


def test_bayesian_threshold_optimality():
    """Bayesian threshold shifting on well-calibrated probabilities must yield minimal cost."""
    X, y = make_classification(n_samples=1000, n_features=6, weights=[0.90, 0.10], random_state=42)
    clf = LogisticRegression(random_state=42).fit(X, y)
    probs = clf.predict_proba(X)[:, 1]

    c_fp = 10.0
    c_fn = 100.0
    optimal_tau = c_fp / (c_fp + c_fn)  # 10 / 110 = 0.0909

    def calc_cost(tau):
        preds = (probs >= tau).astype(int)
        fp = np.sum((preds == 1) & (y == 0))
        fn = np.sum((preds == 0) & (y == 1))
        return fp * c_fp + fn * c_fn

    optimal_cost = calc_cost(optimal_tau)
    naive_cost = calc_cost(0.50)

    assert optimal_cost < naive_cost, f"Optimal threshold failed to beat naive threshold: {optimal_cost} vs {naive_cost}"
```

---

## 11. Cross-Track Learning & Ecosystem Links

- **Mathematical Optimization**: Connects to Bayes decision theory in `01-math-foundations/09-maximum-likelihood-and-bayes`.
- **Threshold Mechanics**: Detailed curve analysis in `04-supervised-classification/11-decision-thresholds-roc-and-pr`.
- **Business Architecture**: Dynamic pricing and transaction-level risk engines in `07-system-design`.

---

## 12. Enterprise Case Study

### Context: Offshore Wind Turbine Bearing Failure Early Warning System
- **Domain**: Renewable Energy Utility ($450$ offshore wind turbines).
- **Asymmetric Cost Structure**:
  - False Alarm ($C_{\text{FP}}$): $\$4,500$ (offshore technician boat dispatch + inspection crew).
  - Missed Failure ($C_{\text{FN}}$): $\$380,000$ (catastrophic catastrophic gearbox destruction + 6-week downtime).
  - Cost Ratio: $\approx 84 : 1$.
- **Implementation**:
  - Model: Vibration telemetry GBDT classifier.
  - Deployed `CalibratedClassifierCV(method='isotonic')` to generate strictly calibrated failure probabilities.
  - Set dynamic alert threshold $\tau^* = \frac{4,500}{4,500 + 380,000} \approx 0.0117$ ($1.17\%$).
- **Outcome**: The system dispatched 38 inspection crews that year, identifying 14 early micro-fractures before catastrophic seizure. Reduced annual fleet catastrophic maintenance expenses from $\$4.2\text{M}$ to $\$420\text{K}$.

---

## 13. Technical Interview Preparation

### Q1: Why is Post-Hoc Bayesian Threshold Shifting often preferred over training-time `class_weight`?
**Answer:** Bayesian threshold shifting cleanly decouples statistical modeling from business operational policy:
1. Business costs change frequently (e.g., changes in review staffing costs, shifting merchandise margins). With threshold shifting, the decision threshold $\tau^* = \frac{C_{\text{FP}}}{C_{\text{FP}} + C_{\text{FN}}}$ is updated instantaneously in runtime configuration without expensive model retraining.
2. It supports **sample-varying costs** (e.g., transaction fraud where cost equals transaction dollar amount).
3. Extreme `class_weight` values can cause severe numerical instability and gradient explosion during training.

### Q2: What is the primary prerequisite for Post-Hoc Threshold Shifting to be mathematically valid?
**Answer:** The predicted continuous output $\hat{p}(\mathbf{x})$ must be **strictly well-calibrated posterior probabilities**: $P(Y = 1 \mid \hat{p}(\mathbf{x}) = p) = p$. If the model outputs uncalibrated margins (e.g., SVM decision values) or overconfident probabilities (unpruned trees, Naive Bayes), the Bayesian risk formula $E[\text{Cost}] = (1-p) C_{\text{FP}} + p C_{\text{FN}}$ fails to reflect true expected loss, leading to erratic, suboptimal decision boundaries.

### Q3: Explain the Zadrozny & Elkan Theorem on Cost-Proportionate Rejection Sampling.
**Answer:** Zadrozny, Langford, and Elkan proved that minimizing expected cost under an asymmetric cost matrix $C$ can be converted into standard cost-insensitive 0-1 loss minimization. By resampling training instances such that each instance $i$ is drawn with probability proportional to its misclassification penalty $w_i = C(y_i, 1 - y_i)$, the distribution of the resampled training set forces any standard learning algorithm to discover the Bayes cost-optimal decision boundary.

### Q4: How do you handle cost-sensitive classification when the costs vary per individual sample?
**Answer:** When costs are instance-specific (e.g., each loan has a different requested principal $L_i$), training-time `class_weight` cannot be used since it applies uniformly across the class. The two correct approaches are:
1. Pass individual `sample_weight` arrays to `.fit(X, y, sample_weight=w)` where $w_i = C_i(y_i, 1 - y_i)$.
2. Train a well-calibrated probabilistic model $P(y=1 \mid \mathbf{x})$, and evaluate each individual sample at test time against its own customized threshold: $\tau_i^* = \frac{C_{\text{FP}, i}}{C_{\text{FP}, i} + C_{\text{FN}, i}}$.

### Q5: How does cost-sensitive learning differ from techniques for handling imbalanced data (SMOTE, downsampling)?
**Answer:** Class imbalance refers strictly to differences in **marginal prior class frequencies** ($P(y=1) \ll P(y=0)$). Cost sensitivity refers strictly to asymmetric **utility penalties in the loss domain** ($C_{\text{FN}} \gg C_{\text{FP}}$). While they frequently co-occur (e.g., rare diseases are both imbalanced and costly to miss), they are theoretically orthogonal. A dataset can be perfectly balanced ($50:50$) yet have severe cost asymmetry ($100:1$), requiring cost-sensitive optimization.

### Q6: What are Cost Curves, and how do they differ from ROC curves?
**Answer:** ROC curves plot TPR vs FPR, which isolates performance from both class prevalence and misclassification costs. However, ROC curves make it difficult for business stakeholders to see the expected financial loss of an operational model. **Cost Curves** (Drummond & Holte) plot normalized expected cost against the probability-cost factor $P(+) C_{\text{FN}} / (P(+) C_{\text{FN}} + P(-) C_{\text{FP}})$, directly visualizing operational dollar loss across various operating conditions.

### Q7: Can a cost matrix incorporate benefits or negative costs?
**Answer:** Yes. In commercial applications (e.g., targeted marketing), correct predictions yield profit ($C(1, 1) = -\text{Profit}$). In such formulations, the objective is converted into **Net Present Value / Profit Maximization**. The optimal Bayesian condition becomes predicting positive whenever the expected profit exceeds the expected cost:
$$P(y = 1 \mid \mathbf{x}) \cdot \text{Revenue}_{\text{TP}} \ge (1 - P(y = 1 \mid \mathbf{x})) \cdot \text{Cost}_{\text{FP}}$$

### Q8: What failure occurs when training a Gradient Boosted Tree with an extreme `class_weight` (e.g., 1 : 5000)?
**Answer:** The initial pseudo-residuals of minority instances are scaled by $5,000$. The very first shallow tree will allocate an isolated terminal leaf specifically to isolate that single massive gradient sample. Subsequent trees will experience extreme oscillation, leaf values $\gamma_{jm}$ will hit clipping bounds, and the ensemble will drastically overfit noisy minority instances, destroying precision across the entire population.
