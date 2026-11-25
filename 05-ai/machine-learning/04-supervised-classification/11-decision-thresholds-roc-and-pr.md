# Decision Thresholds, ROC & Precision-Recall Curves

> **Track:** `05-ai/machine-learning` | **Module:** `04-supervised-classification` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `NumPy: >=1.26.0` | `Python: >=3.11`  
> **Type:** `Core Concept Architecture & Engineering Guide`

---

## 1. Concept Definition & Purpose

In binary classification, models do not natively output binary decisions $\hat{y} \in \{0, 1\}$. Rather, they output continuous scores or posterior probabilities $\hat{p}(\mathbf{x}) \in [0, 1]$. Mapping this continuous score to an operational business decision requires selecting a **Decision Threshold** $\tau \in [0, 1]$:

$$\hat{y}(\mathbf{x}; \tau) = \begin{cases} 1 & \text{if } \hat{p}(\mathbf{x}) \ge \tau \\ 0 & \text{if } \hat{p}(\mathbf{x}) < \tau \end{cases}$$

The default threshold $\tau = 0.50$ is strictly arbitrary. In real-world enterprise applications—such as cancer screening, credit card fraud detection, and hardware failure alerting—the costs of False Positives and False Negatives are severely asymmetric. Tuning the decision threshold $\tau$ allows engineers to directly optimize business trade-offs without retraining the underlying model.

```
       Threshold tau = 0.20 (High Recall)        Threshold tau = 0.80 (High Precision)
       Low false negatives, high false alarms     Few false alarms, many missed positives
      0.0 [--- Negatives ---|------- Positives -----------------------------------] 1.0
                            ^ tau=0.20
      0.0 [----------------------------------- Negatives -------|-- Positives ----] 1.0
                                                                ^ tau=0.80
```

---

## 2. Theoretical Mechanics & Math Formulations

### 1. Fundamental Confusion Matrix Rates

| Metric | Mathematical Formula | Intuition |
| :--- | :--- | :--- |
| **True Positive Rate (TPR) / Recall / Sensitivity** | $\text{TPR} = \frac{\text{TP}}{\text{TP} + \text{FN}} = \frac{\text{TP}}{P}$ | Fraction of actual positive instances correctly identified. |
| **False Positive Rate (FPR) / Fall-out** | $\text{FPR} = \frac{\text{FP}}{\text{FP} + \text{TN}} = \frac{\text{FP}}{N}$ | Fraction of actual negative instances mistakenly flagged as positive. |
| **Precision / Positive Predictive Value (PPV)** | $\text{Precision} = \frac{\text{TP}}{\text{TP} + \text{FP}}$ | Fraction of flagged instances that are genuinely positive. |
| **Specificity / True Negative Rate (TNR)** | $\text{Specificity} = \frac{\text{TN}}{\text{TN} + \text{FP}} = 1 - \text{FPR}$ | Fraction of actual negative instances correctly identified. |

### 2. Receiver Operating Characteristic (ROC) Curve & ROC-AUC
The ROC curve plots the **True Positive Rate (TPR)** against the **False Positive Rate (FPR)** across all possible threshold values $\tau \in [0, 1]$:

$$\text{ROC}(\tau) = (\text{FPR}(\tau), \text{TPR}(\tau))$$

- **Area Under the ROC Curve (ROC-AUC)**:
  $$\text{ROC-AUC} = \int_{0}^{1} \text{TPR}(\tau) \, d(\text{FPR}(\tau))$$
- **Probabilistic Equivalence**:
  The ROC-AUC is mathematically equivalent to the **Wilcoxon-Mann-Whitney U statistic**: the probability that a randomly chosen positive instance receives a higher predicted score than a randomly chosen negative instance:
  $$\text{ROC-AUC} = P(\hat{p}(\mathbf{x}^+) > \hat{p}(\mathbf{x}^-))$$
- **Baseline for Random Guessing**: $\text{ROC-AUC} = 0.50$ (diagonal line from $(0,0)$ to $(1,1)$).

### 3. Precision-Recall (PR) Curve & PR-AUC (Average Precision)
The Precision-Recall curve plots **Precision** against **Recall (TPR)** across all possible threshold values $\tau$:

$$\text{PR}(\tau) = (\text{Recall}(\tau), \text{Precision}(\tau))$$

- **Average Precision (PR-AUC)**:
  Approximated via the trapezoidal rule or interpolated step-function:
  $$\text{PR-AUC} = \sum_{k} (R_k - R_{k-1}) P_k$$
- **Baseline for Random Guessing**: $\text{PR-AUC} = \frac{P}{P + N} = \pi$ (the base rate prevalence of the positive class).

> [!IMPORTANT]
> **Why PR-AUC Dominates on Imbalanced Data**:
> The ROC-AUC false positive rate $\text{FPR} = \frac{\text{FP}}{\text{TN} + \text{FP}}$ includes total negatives $N$ in the denominator. When negatives vastly outnumber positives ($N = 1,000,000$ and $P = 1,000$), generating $10,000$ false positives results in an $\text{FPR} = \frac{10,000}{1,000,000} = 0.01$ ($1\%$), making the ROC curve look outstanding. However, the Precision is $\frac{1,000}{1,000 + 10,000} \approx 9\%$, which is immediately exposed by the Precision-Recall curve.

### 4. Threshold Selection Objectives
1. **Youden's J Statistic (Balanced Sensitivity & Specificity)**:
   $$J(\tau) = \text{TPR}(\tau) - \text{FPR}(\tau) = \text{Sensitivity}(\tau) + \text{Specificity}(\tau) - 1$$
   $$\tau^* = \arg\max_\tau J(\tau)$$
   (Geometrically finds the point on the ROC curve farthest from the diagonal).
2. **$F_\beta$ Maximization**:
   $$F_\beta(\tau) = (1 + \beta^2) \frac{\text{Precision}(\tau) \cdot \text{Recall}(\tau)}{(\beta^2 \cdot \text{Precision}(\tau)) + \text{Recall}(\tau)}$$
   - $\beta = 1.0$: Balanced harmonic mean.
   - $\beta = 2.0$: Emphasizes Recall ($2\times$ more important than Precision).
   - $\beta = 0.5$: Emphasizes Precision ($2\times$ more important than Recall).
3. **Cost-Optimal Thresholding (Bayesian Decision Theory)**:
   Given a cost matrix with $C_{\text{FP}}$ (cost of false alarm) and $C_{\text{FN}}$ (cost of missed incident):
   $$\text{Total Cost}(\tau) = C_{\text{FP}} \cdot \text{FP}(\tau) + C_{\text{FN}} \cdot \text{FN}(\tau)$$
   $$\tau^* = \arg\min_\tau \text{Total Cost}(\tau)$$
   For perfectly calibrated probabilities, the theoretical optimal threshold is:
   $$\tau^* = \frac{C_{\text{FP}}}{C_{\text{FP}} + C_{\text{FN}}}$$

---

## 3. Architectural Topology & Flow Diagrams

```
+--------------------------------------------------------------------------------------------------+
|                             THRESHOLD SELECTION & METRIC TOPOLOGY                                |
|                                                                                                  |
|                                       Model Validation Data                                      |
|                                                |                                                 |
|                                 Is the dataset class-imbalanced?                                 |
|                                     /                      \                                     |
|                                  [YES]                     [NO]                                  |
|                                   /                          \                                   |
|                      Precision-Recall Curve                ROC Curve                             |
|                                   |                              |                               |
|                  Are specific business costs known?     Balance Sensitivity/Specificity          |
|                         /                  \                     |                               |
|                      [YES]                 [NO]           Youden's J Statistic                   |
|                       /                      \            tau = argmax(TPR - FPR)                |
|             Cost-Optimal Search         Maximize F_beta                                          |
|             tau = argmin(C_FP*FP        tau = argmax F1                                          |
|                     + C_FN*FN)                                                                   |
+--------------------------------------------------------------------------------------------------+
```

---

## 4. Production Scikit-Learn Implementation

```python
"""
Production decision threshold optimization pipeline.
Demonstrates Scikit-Learn 1.4+ TunedThresholdClassifierCV,
precision-recall threshold analysis, and cost-curve search.
"""

from __future__ import annotations
import logging
from typing import Dict, Any, Tuple
import numpy as np
from sklearn.datasets import make_classification
from sklearn.model_selection import train_test_split
from sklearn.ensemble import HistGradientBoostingClassifier
from sklearn.metrics import (
    roc_curve,
    precision_recall_curve,
    f1_score,
    roc_auc_score,
    average_precision_score,
    confusion_matrix
)

# TunedThresholdClassifierCV was introduced in scikit-learn 1.4
try:
    from sklearn.model_selection import TunedThresholdClassifierCV
except ImportError:
    TunedThresholdClassifierCV = None

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("Threshold_Optimization")


def find_cost_optimal_threshold(
    y_true: np.ndarray,
    y_prob: np.ndarray,
    cost_fp: float = 10.0,
    cost_fn: float = 200.0
) -> Tuple[float, float]:
    """Find decision threshold minimizing asymmetric business operational cost."""
    thresholds = np.linspace(0.01, 0.99, 200)
    best_cost = float("inf")
    best_tau = 0.50

    for tau in thresholds:
        y_pred = (y_prob >= tau).astype(int)
        tn, fp, fn, tp = confusion_matrix(y_true, y_pred).ravel()
        total_cost = (fp * cost_fp) + (fn * cost_fn)

        if total_cost < best_cost:
            best_cost = total_cost
            best_tau = float(tau)

    return best_tau, best_cost


def run_threshold_optimization_suite() -> Dict[str, float]:
    # Stage 1: Ingestion of highly imbalanced dataset (98% negative, 2% positive)
    logger.info("Stage 1: Synthesizing imbalanced fraud dataset (2% positive).")
    X, y = make_classification(
        n_samples=10000,
        n_features=25,
        n_informative=12,
        weights=[0.98, 0.02],
        random_state=42
    )

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.30, stratify=y, random_state=42
    )

    # Stage 2: Fit Estimator
    clf = HistGradientBoostingClassifier(random_state=42)
    clf.fit(X_train, y_train)

    y_prob = clf.predict_proba(X_test)[:, 1]

    # Stage 3: Baseline Evaluation at Default tau = 0.50
    y_pred_default = (y_prob >= 0.50).astype(int)
    f1_default = float(f1_score(y_test, y_pred_default))
    roc_auc = float(roc_auc_score(y_test, y_prob))
    pr_auc = float(average_precision_score(y_test, y_prob))

    logger.info(f"Baseline (tau=0.50) -> F1: {f1_default:.4f} | ROC-AUC: {roc_auc:.4f} | PR-AUC: {pr_auc:.4f}")

    # Stage 4: Optimal Threshold Search via Precision-Recall Curve (Maximizing F1)
    precisions, recalls, pr_thresholds = precision_recall_curve(y_test, y_prob)
    # Avoid division by zero
    f1_scores = 2 * (precisions * recalls) / (precisions + recalls + 1e-10)
    best_idx = np.argmax(f1_scores)
    # pr_thresholds has length len(precisions) - 1
    optimal_tau_f1 = float(pr_thresholds[min(best_idx, len(pr_thresholds) - 1)])
    max_f1 = float(f1_scores[best_idx])

    logger.info(f"Optimal F1 Threshold: tau = {optimal_tau_f1:.4f} (Max F1: {max_f1:.4f})")

    # Stage 5: Business Cost Optimization ($10 per false alert, $200 per missed fraud)
    cost_tau, min_cost = find_cost_optimal_threshold(
        y_test, y_prob, cost_fp=10.0, cost_fn=200.0
    )
    logger.info(f"Cost-Optimal Threshold: tau = {cost_tau:.4f} (Total Cost: ${min_cost:,.2f})")

    # Stage 6: Modern Scikit-Learn TunedThresholdClassifierCV (if available)
    if TunedThresholdClassifierCV is not None:
        tuned_clf = TunedThresholdClassifierCV(
            clf,
            scoring="f1",
            cv=5,
            random_state=42
        )
        tuned_clf.fit(X_train, y_train)
        logger.info(f"TunedThresholdClassifierCV Selected Threshold: {tuned_clf.best_threshold_:.4f}")

    return {
        "roc_auc": roc_auc,
        "pr_auc": pr_auc,
        "default_f1": f1_default,
        "optimized_f1": max_f1,
        "optimal_tau_f1": optimal_tau_f1,
        "cost_optimal_tau": cost_tau,
    }


if __name__ == "__main__":
    results = run_threshold_optimization_suite()
    assert results["optimized_f1"] > results["default_f1"], "Threshold tuning failed to improve F1."
```

---

## 5. Edge Cases & Failure Modes

1. **Threshold Overfitting on Small Validation Slices**:
   Searching over hundreds of threshold candidates on a tiny validation set ($n_{\text{pos}} < 50$) risks selecting an overfitted threshold $\tau^*$ that exploits random spacing between adjacent validation scores.
   - *Mitigation*: Apply **Cross-Validated Threshold Tuning** (`TunedThresholdClassifierCV`) or constrain candidate grid size.
2. **Threshold Collapse on Extreme Imbalance**:
   When maximizing metrics like accuracy or weighted F1 on severe imbalance ($99.9 : 0.1$), the optimal threshold search may collapse to $\tau = 1.0$ (never predict positive).
   - *Mitigation*: Strictly optimize unweighted Macro F1, $F_\beta$ ($\beta \ge 2$), or enforce minimum recall constraints ($\text{Recall} \ge 0.80$).
3. **Score Distribution Shift in Production**:
   If model feature drift causes the predicted score distribution to shift downward, a static threshold $\tau = 0.35$ will suddenly produce catastrophic false negatives.
   - *Mitigation*: Continuously track the percentage of predictions above threshold $\tau$ (alert rate) as a live operational KPI.

---

## 6. Comparative Analysis Table

| Metric / Method | Invariance to Class Imbalance | Reflects High-Probability Confidence | Primary Business Use Case |
| :--- | :--- | :--- | :--- |
| **ROC-AUC** | Yes (Evaluates TPR vs FPR; insensitive to prevalence) | No (Purely evaluates rank ordering) | Global model benchmark across balanced datasets. |
| **PR-AUC (Average Precision)** | No (Directly influenced by class prevalence $\pi$) | Yes (Focuses exclusively on minority positive class) | Anomaly detection, fraud, rare disease screening ($< 5\%$ prevalence). |
| **Youden's J Threshold** | High (Balances sensitivity and specificity equally) | Moderate | Diagnostic medical tests where FN and FP have symmetric gravity. |
| **Cost-Optimal Threshold** | Dependent on business cost matrix | Maximum | Financial underwriting, fraud detection, commercial bidding. |

---

## 7. Hyperparameter & Configuration Matrix

| Tool / Method | Parameter | Optimal Values | Description |
| :--- | :--- | :--- | :--- |
| `TunedThresholdClassifierCV` | `scoring` | `'f1'`, `'roc_auc'`, custom cost scorer | Target objective function to optimize during threshold search. |
| `TunedThresholdClassifierCV` | `thresholds` | `100` | Number of candidate threshold cutoffs evaluated over $[0, 1]$. |
| `precision_recall_curve` | `probas_pred` | Continuous probabilities | Input probability array corresponding to the positive class. |

---

## 8. Diagnostics, Monitoring & Error Metrics

### Production Monitoring Metrics
1. **Positive Prediction Rate (PPR / Alert Rate)**:
   $$\text{PPR} = \frac{1}{n} \sum_{i=1}^n \mathbb{I}(\hat{p}_i \ge \tau)$$
   Monitors operational capacity. If fraud investigators can review at most $500$ alerts/day, threshold $\tau$ must be dynamically constrained to satisfy $\text{PPR} \times \text{Volume} \le 500$.
2. **Precision at $K$ (P@K)**:
   Sorts all predictions by confidence score descending and computes precision strictly within the top $K$ highest-ranked samples.

---

## 9. Production Engineering & Serving Considerations

- **Decoupled Architecture**: Never hard-code decision thresholds inside ML model weights or pipeline objects. Store $\tau$ in dynamic runtime configuration (Redis, Consul, or environment variables) so business operators can adjust alert sensitivity in real-time without redeploying code.
- **Latency Impact**: Decision threshold comparison is a single vectorized float comparison: `predictions = (probs >= tau).astype(np.int8)`. Latency is negligible ($< 1\mu\text{s}$).

---

## 10. Automated Verification Harness

```python
"""
Pytest automated verification harness for decision thresholds and ROC/PR curves.
Verifies ROC-AUC equivalence, monotonic rate relationships, and threshold validity.
"""

import numpy as np
import pytest
from sklearn.datasets import make_classification
from sklearn.metrics import roc_curve, precision_recall_curve, roc_auc_score


@pytest.fixture
def test_data():
    return make_classification(n_samples=300, n_features=8, random_state=42)


def test_roc_curve_monotonicity(test_data):
    """FPR and TPR must both decrease monotonically as threshold increases."""
    X, y = test_data
    # Synthetic random probabilities
    y_prob = np.random.default_rng(42).uniform(0, 1, size=len(y))

    fpr, tpr, thresholds = roc_curve(y, y_prob)
    # Thresholds in roc_curve are sorted descending
    # As threshold decreases, FPR and TPR must be non-decreasing
    assert np.all(np.diff(fpr) >= -1e-6), "FPR is not monotonically non-decreasing."
    assert np.all(np.diff(tpr) >= -1e-6), "TPR is not monotonically non-decreasing."


def test_wilcoxon_mann_whitney_equivalence(test_data):
    """ROC-AUC must exactly equal the Wilcoxon-Mann-Whitney ranking probability."""
    X, y = test_data
    rng = np.random.default_rng(42)
    y_prob = rng.uniform(0, 1, size=len(y))

    sklearn_auc = roc_auc_score(y, y_prob)

    # Compute Wilcoxon-Mann-Whitney manually: P(score_pos > score_neg)
    pos_scores = y_prob[y == 1]
    neg_scores = y_prob[y == 0]

    # Pairwise comparison
    comparisons = pos_scores[:, None] > neg_scores[None, :]
    ties = pos_scores[:, None] == neg_scores[None, :]
    wmw_auc = (np.sum(comparisons) + 0.5 * np.sum(ties)) / (len(pos_scores) * len(neg_scores))

    np.testing.assert_allclose(sklearn_auc, wmw_auc, atol=1e-5)
```

---

## 11. Cross-Track Learning & Ecosystem Links

- **Mathematical Foundations**: Connects to `01-math-foundations/04-statistics-and-hypothesis-testing` (Type I vs Type II errors).
- **Cost Sensitivity**: Integrates into `04-supervised-classification/12-cost-sensitive-classification`.
- **System Architecture**: Dynamic threshold governance in `07-system-design`.

---

## 12. Enterprise Case Study

### Context: Payment Gateway Chargeback Prevention & Fraud Alerting
- **Domain**: High-Volume Payment Processor ($2\text{M}$ transactions/day).
- **Constraints**:
  - False Positive Cost: $C_{\text{FP}} = \$12$ (friction, customer checkout abandonment).
  - False Negative Cost: $C_{\text{FN}} = \$350$ (chargeback fee + merchandise loss).
  - Human review capacity: Max $2,000$ manual review tickets per day ($0.1\%$ capacity).
- **Implementation**:
  - Model: GBDT with PR-AUC $0.68$ on $0.2\%$ fraud prevalence.
  - Default $\tau = 0.50$ caught only $32\%$ of fraud (too conservative).
  - Multi-objective search: Tuned $\tau^* = 0.18$, which minimized operational cost while capping daily review volume at $1,850$ cases.
- **Outcome**: Fraud prevention capture increased from $32\%$ to $81\%$, saving $\$3.2\text{M}$ annually while staying within investigator headcount constraints.

---

## 13. Technical Interview Preparation

### Q1: Why is the default threshold $\tau = 0.50$ often suboptimal in production systems?
**Answer:** The threshold $\tau = 0.50$ is optimal only under two strict, rarely met assumptions:
1. The class distribution is perfectly balanced ($50:50$).
2. The real-world costs of False Positives and False Negatives are completely symmetric ($C_{\text{FP}} = C_{\text{FN}}$).
In enterprise environments (e.g., fraud, healthcare, churn), classes are heavily imbalanced and the cost of a missed incident (FN) is typically orders of magnitude higher than a false alarm (FP). Setting $\tau = 0.50$ ignores these operational realities, resulting in massive business cost.

### Q2: Why is the Precision-Recall curve preferred over the ROC curve for highly imbalanced datasets?
**Answer:** The ROC curve plots TPR vs FPR, where $\text{FPR} = \frac{\text{FP}}{\text{TN} + \text{FP}}$. In highly imbalanced data where true negatives (TN) are vast, a large influx of false positives produces only a minuscule change in FPR, causing the ROC curve to present an overly optimistic picture of model utility. The Precision-Recall curve plots Precision vs Recall; Precision ($\frac{\text{TP}}{\text{TP} + \text{FP}}$) directly captures the dilution caused by false positives without being cushioned by true negatives.

### Q3: What is the theoretical relationship between ROC-AUC and the Wilcoxon-Mann-Whitney U statistic?
**Answer:** They are mathematically identical. Given a random positive instance $\mathbf{x}^+$ and a random negative instance $\mathbf{x}^-$, the ROC-AUC equals the probability that the classifier assigns a higher prediction score to the positive instance than to the negative instance:
$$\text{ROC-AUC} = P(f(\mathbf{x}^+) > f(\mathbf{x}^-)) + \frac{1}{2} P(f(\mathbf{x}^+) = f(\mathbf{x}^-))$$
This non-parametric equivalence demonstrates that ROC-AUC evaluates purely the global ranking quality of the model across all thresholds.

### Q4: How is Youden's J statistic defined, and what geometric property does it possess?
**Answer:** Youden's J statistic is defined as:
$$J(\tau) = \text{TPR}(\tau) - \text{FPR}(\tau) = \text{Sensitivity}(\tau) + \text{Specificity}(\tau) - 1$$
Geometrically, $J$ represents the vertical distance between the point on the ROC curve $(\text{FPR}(\tau), \text{TPR}(\tau))$ and the diagonal chance line ($y = x$). The threshold that maximizes $J(\tau)$ represents the point of maximum discriminatory divergence between the two classes under equal class and cost weighting.

### Q5: If a model's predicted probabilities are perfectly calibrated, what is the mathematically optimal decision threshold given costs $C_{\text{FP}}$ and $C_{\text{FN}}$?
**Answer:** By Bayesian Decision Theory, an instance should be classified as positive if the expected cost of predicting positive is less than or equal to the expected cost of predicting negative:
$$E[\text{Cost} \mid \hat{y}=1] \le E[\text{Cost} \mid \hat{y}=0]$$
$$(1 - p) \cdot C_{\text{FP}} \le p \cdot C_{\text{FN}}$$
Solving for $p$:
$$C_{\text{FP}} \le p (C_{\text{FP}} + C_{\text{FN}}) \implies p \ge \frac{C_{\text{FP}}}{C_{\text{FP}} + C_{\text{FN}}}$$
Thus, the theoretical optimal threshold is $\tau^* = \frac{C_{\text{FP}}}{C_{\text{FP}} + C_{\text{FN}}}$. If $C_{\text{FN}} = 9 \cdot C_{\text{FP}}$, $\tau^* = \frac{1}{1 + 9} = 0.10$.

### Q6: Can a classifier have an ROC-AUC of 0.95 and yet have a Precision of only 10%?
**Answer:** Yes. If the positive class prevalence is $0.1\%$ ($1$ in $1,000$), an outstanding ROC-AUC of $0.95$ might correspond to a point where $\text{TPR} = 0.90$ and $\text{FPR} = 0.01$ ($1\%$). In a population of $100,000$, there are $100$ true positives and $99,900$ true negatives. The model catches $90$ true positives but generates $0.01 \times 99,900 \approx 999$ false positives. Precision is $\frac{90}{90 + 999} \approx 8.26\%$.

### Q7: What is the primary risk of using cross-validation to select an optimal threshold $\tau$?
**Answer:** Threshold tuning adds an additional optimization step. If the validation fold is too small or if threshold selection is performed over thousands of candidates without regularization, the search will overfit the validation sample distribution (threshold overfitting). This results in an over-optimistic validation F1 score that degrades when applied to out-of-sample production traffic.

### Q8: How should decision thresholds be handled in Multi-Label classification?
**Answer:** In multi-label classification ($\mathbf{y} \in \{0, 1\}^K$), applying a single global threshold $\tau$ across all $K$ labels is suboptimal because different labels have different base rates, noise levels, and operational costs. The optimal strategy is to tune $K$ independent decision thresholds $\boldsymbol{\tau} = [\tau_1, \dots, \tau_K]$, optimizing each label's decision boundary independently to maximize label-specific F1 or target utility.
