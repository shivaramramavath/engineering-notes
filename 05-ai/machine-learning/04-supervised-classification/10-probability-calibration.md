# Probability Calibration in Classification

> **Track:** `05-ai/machine-learning` | **Module:** `04-supervised-classification` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `NumPy: >=1.26.0` | `Python: >=3.11`  
> **Type:** `Core Concept Architecture & Engineering Guide`

---

## 1. Concept Definition & Purpose

In classification systems, predictive models often output a continuous confidence score $s \in [0, 1]$ or raw decision margin $f(\mathbf{x}) \in (-\infty, +\infty)$. However, a model with high discrimination (e.g., high ROC-AUC) is not necessarily **calibrated**.

A classifier is **well-calibrated** if its predicted probability reflects the true empirical likelihood of the event occurring:

$$P(Y = 1 \mid \hat{P} = p) = p, \quad \forall p \in [0, 1]$$

*Concrete Interpretation*: Out of 100 loan applicants assigned a default probability of $0.80$, exactly 80 should genuinely default. If only 40 default, the model is severely overconfident.

```
       Perfect Calibration (Diagonal)         Uncalibrated S-Curve (Sigmoid)
     1.0 |               /                  1.0 |                  .---
         |              /                       |                .'
True     |             /             True       |              .'
Fraction |            /              Fraction   |             /
of       |           /               of         |            /
Positives|          /                Positives  |          .'
         |         /                            |       _.'
     0.0 +-----------------> 1.0            0.0 +-----------------> 1.0
          Predicted Probability                  Predicted Probability
```

---

## 2. Theoretical Mechanics & Math Formulations

### 1. Brier Score Decomposition
The Brier Score measures the mean squared difference between predicted probabilities $\hat{p}_i$ and true binary outcomes $y_i \in \{0, 1\}$:

$$\text{BS} = \frac{1}{n} \sum_{i=1}^n (\hat{p}_i - y_i)^2$$

Murphy (1973) proved that the Brier score can be partitioned into three orthogonal components:

$$\text{BS} = \text{Reliability} - \text{Resolution} + \text{Uncertainty}$$

$$\text{BS} = \frac{1}{n} \sum_{k=1}^K n_k (\bar{p}_k - \bar{y}_k)^2 - \frac{1}{n} \sum_{k=1}^K n_k (\bar{y}_k - \bar{y})^2 + \bar{y}(1 - \bar{y})$$

- **Reliability (Calibration Error)**: Measures distance between predicted probability bin mean $\bar{p}_k$ and true empirical frequency $\bar{y}_k$. (Lower is better; $0$ indicates perfect calibration).
- **Resolution**: Measures how much the predictions in bins differ from the global base rate $\bar{y}$. (Higher is better; quantifies discriminatory power).
- **Uncertainty**: The intrinsic entropy of the dataset $\bar{y}(1 - \bar{y})$. (Constant for a fixed dataset).

### 2. Expected Calibration Error (ECE) & Maximum Calibration Error (MCE)
Partition the prediction interval $[0, 1]$ into $M$ equally spaced bins $B_1, \dots, B_M$:

$$\text{ECE} = \sum_{m=1}^M \frac{|B_m|}{n} \left| \text{acc}(B_m) - \text{conf}(B_m) \right|$$

$$\text{MCE} = \max_{m \in \{1, \dots, M\}} \left| \text{acc}(B_m) - \text{conf}(B_m) \right|$$

Where $\text{acc}(B_m) = \frac{1}{|B_m|} \sum_{i \in B_m} y_i$ is true accuracy, and $\text{conf}(B_m) = \frac{1}{|B_m|} \sum_{i \in B_m} \hat{p}_i$ is average model confidence.

### 3. Platt Scaling (Sigmoid Calibration)
Fits a univariate parametric logistic regression model over the uncalibrated decision function or log-odds $f(\mathbf{x})$:

$$\hat{P}(y = 1 \mid \mathbf{x}) = \frac{1}{1 + \exp(A \cdot f(\mathbf{x}) + B)}$$

Scalar parameters $A$ and $B$ are estimated via maximum likelihood on a held-out calibration set. To prevent overfitting, Platt proposed using regularized Bayesian target probabilities:
$$t_i = \begin{cases} \frac{N_+ + 1}{N_+ + 2} & \text{if } y_i = 1 \\ \frac{1}{N_- + 2} & \text{if } y_i = 0 \end{cases}$$

### 4. Isotonic Regression (Non-Parametric Calibration)
Fits a free-form, non-decreasing piece-wise constant step function $m(\hat{y})$ using the **Pair Adjacent Violators (PAV) algorithm**:

$$\min_{m} \sum_{i=1}^n (y_i - m(\hat{y}_i))^2 \quad \text{subject to } m(\hat{y}_i) \le m(\hat{y}_j) \text{ whenever } \hat{y}_i \le \hat{y}_j$$

- *Advantage*: Completely non-parametric; can correct complex distortions (e.g., S-curves, asymmetric skews).
- *Limitation*: Prone to severe overfitting when validation sample size $n_{\text{cal}} < 1,000$.

### 5. Temperature Scaling (For Neural Networks & Softmax)
A single-parameter variant of Platt scaling applied to logit vectors $\mathbf{z} \in \mathbb{R}^K$:

$$P(y = k \mid \mathbf{z}) = \frac{\exp(z_k / T)}{\sum_{j=1}^K \exp(z_j / T)}$$

Where $T > 0$ is the learned temperature scalar. If $T > 1$, the softmax distribution softens, reducing overconfidence without altering the top-1 prediction rank.

---

## 3. Architectural Topology & Flow Diagrams

```
+--------------------------------------------------------------------------------------------------+
|                            CALIBRATION WORKFLOW & SELECTION FLOWCHART                            |
|                                                                                                  |
|                      Raw Estimator Outputs Predictions / Decision Margins                        |
|                                                |                                                 |
|                           Evaluate Initial Reliability Diagram & ECE                             |
|                                                |                                                 |
|                                  Is the model well-calibrated?                                   |
|                                  (ECE < 0.03, Reliability on diagonal)                           |
|                                     /                     \                                      |
|                                  [YES]                    [NO]                                   |
|                                   /                          \                                   |
|                           Deploy Directly           Is calibration dataset size                  |
|                                                         n_cal >= 1,000 samples?                  |
|                                                              /            \                      |
|                                                           [YES]           [NO]                   |
|                                                            /                \                    |
|                                                  Is distortion monotonic?  Platt Scaling        |
|                                                        /         \         ('sigmoid')           |
|                                                     [YES]        [NO]                            |
|                                                      /             \                             |
|                                            Isotonic Regression   Beta Calibration /              |
|                                               ('isotonic')       Spline Calibration              |
+--------------------------------------------------------------------------------------------------+
```

---

## 4. Production Scikit-Learn Implementation

```python
"""
Production calibration pipeline implementing reliability diagrams,
Brier score auditing, and CalibratedClassifierCV.
"""

from __future__ import annotations
import logging
from typing import Dict, Any, Tuple
import numpy as np
from sklearn.datasets import make_classification
from sklearn.model_selection import train_test_split
from sklearn.svm import SVC
from sklearn.calibration import CalibratedClassifierCV, calibration_curve
from sklearn.metrics import brier_score_loss, roc_auc_score

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("Calibration_Pipeline")


def compute_expected_calibration_error(y_true: np.ndarray, y_prob: np.ndarray, n_bins: int = 10) -> float:
    """Calculate Expected Calibration Error (ECE) across uniform probability bins."""
    bin_edges = np.linspace(0.0, 1.0, n_bins + 1)
    ece = 0.0
    n = len(y_true)

    for i in range(n_bins):
        in_bin = (y_prob >= bin_edges[i]) & (y_prob < bin_edges[i + 1])
        bin_count = np.sum(in_bin)
        if bin_count > 0:
            bin_acc = np.mean(y_true[in_bin])
            bin_conf = np.mean(y_prob[in_bin])
            ece += (bin_count / n) * np.abs(bin_acc - bin_conf)

    return float(ece)


def run_calibration_pipeline() -> Dict[str, float]:
    logger.info("Generating uncalibrated classification dataset.")
    X, y = make_classification(
        n_samples=5000,
        n_features=20,
        n_informative=10,
        weights=[0.80, 0.20],
        random_state=42
    )

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.25, stratify=y, random_state=42
    )

    # 1. Train Uncalibrated Base Model (SVM with RBF kernel produces raw margins)
    logger.info("Fitting raw uncalibrated Support Vector Classifier.")
    base_svm = SVC(kernel="rbf", C=1.0, random_state=42)
    base_svm.fit(X_train, y_train)

    # Scikit-Learn SVC doesn't predict probabilities unless calibrated or probability=True
    # Decision function represents raw margin
    df_test = base_svm.decision_function(X_test)
    # Min-max scale decision function to [0, 1] as an uncalibrated heuristic
    raw_probs = (df_test - df_test.min()) / (df_test.max() - df_test.min())

    uncal_brier = float(brier_score_loss(y_test, raw_probs))
    uncal_ece = compute_expected_calibration_error(y_test, raw_probs)
    logger.info(f"Uncalibrated Model -> Brier Score: {uncal_brier:.4f} | ECE: {uncal_ece:.4f}")

    # 2. Calibrate via Platt Scaling (Sigmoid)
    logger.info("Calibrating via Platt Scaling (method='sigmoid').")
    cal_sigmoid = CalibratedClassifierCV(estimator=base_svm, method="sigmoid", cv=5)
    cal_sigmoid.fit(X_train, y_train)
    prob_sigmoid = cal_sigmoid.predict_proba(X_test)[:, 1]
    sig_brier = float(brier_score_loss(y_test, prob_sigmoid))
    sig_ece = compute_expected_calibration_error(y_test, prob_sigmoid)
    logger.info(f"Platt Scaling -> Brier Score: {sig_brier:.4f} | ECE: {sig_ece:.4f}")

    # 3. Calibrate via Isotonic Regression
    logger.info("Calibrating via Isotonic Regression (method='isotonic').")
    cal_iso = CalibratedClassifierCV(estimator=base_svm, method="isotonic", cv=5)
    cal_iso.fit(X_train, y_train)
    prob_iso = cal_iso.predict_proba(X_test)[:, 1]
    iso_brier = float(brier_score_loss(y_test, prob_iso))
    iso_ece = compute_expected_calibration_error(y_test, prob_iso)
    logger.info(f"Isotonic Regression -> Brier Score: {iso_brier:.4f} | ECE: {iso_ece:.4f}")

    return {
        "uncalibrated_brier": uncal_brier,
        "sigmoid_brier": sig_brier,
        "isotonic_brier": iso_brier,
        "sigmoid_ece": sig_ece,
        "isotonic_ece": iso_ece,
    }


if __name__ == "__main__":
    metrics = run_calibration_pipeline()
    assert metrics["sigmoid_brier"] < metrics["uncalibrated_brier"], "Calibration failed to improve Brier score."
```

---

## 5. Edge Cases & Failure Modes

1. **Overfitting in Isotonic Regression on Small Validation Sets**:
   When $n_{\text{cal}} < 500$, the Pair Adjacent Violators algorithm creates coarse, stair-stepped probability bins that memorize individual labels in the calibration set. Test-time predictions exhibit discrete steps with high variance.
   - *Mitigation*: Fall back strictly to parametric Platt scaling (`method='sigmoid'`).
2. **Post-Calibration Distribution Drift**:
   A model calibrated under training class prevalence $P(y=1) = 0.20$ becomes uncalibrated if deployed in an environment where class prevalence shifts to $P(y=1) = 0.50$, even if feature conditional likelihoods $P(\mathbf{x} \mid y)$ remain unchanged.
   - *Mitigation*: Apply **Bayesian Prior Shift Correction**:
     $$P_{\text{target}}(y=1 \mid \mathbf{x}) = \frac{P_{\text{source}}(y=1 \mid \mathbf{x}) \cdot \frac{P_{\text{target}}(y=1)}{P_{\text{source}}(y=1)}}{P_{\text{source}}(y=1 \mid \mathbf{x}) \frac{P_{\text{target}}(y=1)}{P_{\text{source}}(y=1)} + P_{\text{source}}(y=0 \mid \mathbf{x}) \frac{P_{\text{target}}(y=0)}{P_{\text{source}}(y=0)}}$$
3. **Severe Underconfidence in Naive Bayes**:
   Due to the violated conditional independence assumption, Naive Bayes pushes probabilities to extreme values ($0.0$ or $1.0$). Platt scaling can pull these extreme probabilities back toward calibrated ranges.

---

## 6. Comparative Analysis Table

| Method | Type | Parameters | Data Requirement | Non-Monotonic Capacity | Overfitting Risk |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Platt Scaling** | Parametric (Sigmoid) | 2 ($A, B$) | Low ($n \sim 100$) | None (strictly monotonic) | Minimal |
| **Isotonic Regression** | Non-parametric (Step) | Variable (PAV steps) | High ($n \ge 1,000$) | None (strictly monotonic) | High on small data |
| **Temperature Scaling** | Parametric (Logit scalar)| 1 ($T$) | Low ($n \sim 100$) | None | Minimal |
| **Beta Calibration** | Parametric (Beta dist) | 3 ($\alpha, \beta, \gamma$) | Moderate ($n \ge 500$) | Low | Low |

---

## 7. Hyperparameter & Configuration Matrix

| Parameter (`CalibratedClassifierCV`) | Options | Recommended | Impact |
| :--- | :--- | :--- | :--- |
| `method` | `'sigmoid'`, `'isotonic'` | `'sigmoid'` if $n < 1000$; `'isotonic'` if $n \ge 1000$ | Selects parametric vs non-parametric calibration algorithm. |
| `cv` | integer, cross-validation generator, or `'prefit'` | `5` (or `'prefit'`) | If base model is already trained on external data, pass `'prefit'` to prevent retraining. |
| `ensemble` | `True`, `False` | `True` | When `True`, averages probability predictions across all CV folds during inference. |

---

## 8. Diagnostics, Monitoring & Error Metrics

### Constructing Reliability Diagrams
1. Sort test samples by predicted probability $\hat{p}_i$.
2. Divide into 10 deciles (bins).
3. Compute mean predicted probability $\bar{p}_m$ and true fraction of positives $\bar{y}_m$ for each bin.
4. Plot $\bar{y}_m$ against $\bar{p}_m$. A perfectly calibrated model aligns exactly on the $45^\circ$ diagonal line ($y = x$).

```python
from sklearn.calibration import calibration_curve

prob_true, prob_pred = calibration_curve(y_test, y_prob, n_bins=10, strategy="uniform")
# Plotting prob_true vs prob_pred generates the empirical reliability diagram
```

---

## 9. Production Engineering & Serving Considerations

- **Inference Latency Impact**: Platt scaling adds negligible latency ($O(1)$ scalar addition and exponentiation: $\sigma(A x + B)$). Isotonic regression requires binary search over PAV step thresholds ($O(\log N_{\text{steps}})$), which is still sub-millisecond.
- **Data Leakage Prohibition**: Never fit calibration models on the training set used to fit the base estimator. Overfitted base models output extreme margins on training data, causing calibrators to misjudge real generalization margins. Always use an isolated validation split or cross-validation (`cv=5`).

---

## 10. Automated Verification Harness

```python
"""
Pytest automated verification harness for probability calibration.
Verifies Brier score monotonicity, ECE bounds, and probability ranges.
"""

import numpy as np
import pytest
from sklearn.datasets import make_classification
from sklearn.ensemble import RandomForestClassifier
from sklearn.calibration import CalibratedClassifierCV
from sklearn.metrics import brier_score_loss


@pytest.fixture
def test_dataset():
    return make_classification(n_samples=500, n_features=10, random_state=42)


def test_calibrated_probabilities_valid_distribution(test_dataset):
    """Calibrated predictions must form a valid probability distribution."""
    X, y = test_dataset
    base_rf = RandomForestClassifier(n_estimators=20, random_state=42)
    cal_model = CalibratedClassifierCV(base_rf, method="sigmoid", cv=3)
    cal_model.fit(X, y)

    probs = cal_model.predict_proba(X)
    assert np.all(probs >= 0.0), "Negative probability detected."
    assert np.all(probs <= 1.0), "Probability exceeded 1.0."
    np.testing.assert_allclose(np.sum(probs, axis=1), 1.0, atol=1e-6)


def test_calibration_brier_improvement(test_dataset):
    """Calibration must improve or maintain Brier Score compared to raw scaled outputs."""
    X, y = test_dataset
    X_tr, X_te, y_tr, y_te = X[:350], X[350:], y[:350], y[350:]

    base_rf = RandomForestClassifier(n_estimators=15, max_depth=3, random_state=42).fit(X_tr, y_tr)
    raw_brier = brier_score_loss(y_te, base_rf.predict_proba(X_te)[:, 1])

    cal_model = CalibratedClassifierCV(base_rf, method="sigmoid", cv="prefit")
    cal_model.fit(X_tr, y_tr)
    cal_brier = brier_score_loss(y_te, cal_model.predict_proba(X_te)[:, 1])

    assert cal_brier <= raw_brier + 0.02, f"Calibration significantly degraded Brier score: {cal_brier} vs {raw_brier}"
```

---

## 11. Cross-Track Learning & Ecosystem Links

- **Mathematical Foundations**: Extends Bayes theorem and log-loss in `01-math-foundations/09-maximum-likelihood-and-bayes`.
- **Base Algorithms**: Calibrates margins for SVM in `04-support-vector-machines` and trees in `05-decision-trees`.
- **System Design & Pricing**: Integral for expected value calculations in production financial engines in `07-system-design`.

---

## 12. Enterprise Case Study

### Context: Automated Consumer Credit Underwriting & Interest Rate Pricing
- **Domain**: Tier-1 Consumer Lending Fintech.
- **Problem**: Loan interest rates are determined dynamically by expected loss:
  $$\text{Expected Loss} = P(\text{Default} \mid \mathbf{x}) \times \text{Exposure at Default} \times \text{Loss Given Default}$$
  An uncalibrated GBDT model had an outstanding ROC-AUC ($0.88$), but was severely overconfident: predicted default probabilities in the $0.10$–$0.20$ decile had an actual default frequency of $0.03$.
- **Implementation**:
  Applied Isotonic Regression via `CalibratedClassifierCV(estimator=best_lgbm, method='isotonic', cv=5)` over a validation pool of $40\text{K}$ loans.
- **Outcome**: Reduced Expected Calibration Error (ECE) from $0.092$ to $0.011$. Aligned loan pricing with actual default risk, capturing $\$8.4\text{M}$ in high-margin lending previously rejected due to overconfident risk estimates.

---

## 13. Technical Interview Preparation

### Q1: Can a model with high ROC-AUC still have poor probability calibration?
**Answer:** Yes, absolutely. ROC-AUC evaluates strictly the **ranking ability** (discrimination) of a model—whether positive instances receive higher predicted scores than negative instances. If every positive sample receives score $0.999$ and every negative sample receives $0.998$, the ROC-AUC is a perfect $1.0$. However, the predicted probabilities are completely useless for expected value calculations because a score of $0.998$ corresponds to a $0\%$ empirical positive rate.

### Q2: Explain the Brier Score decomposition into Reliability, Resolution, and Uncertainty.
**Answer:** The Brier score $\text{BS} = \frac{1}{n} \sum (\hat{p}_i - y_i)^2$ decomposes into:
$$\text{BS} = \text{Reliability} - \text{Resolution} + \text{Uncertainty}$$
- **Reliability**: Measures calibration error—the weighted squared distance between predicted probability bin means and actual bin event frequencies.
- **Resolution**: Measures discrimination—how much empirical event frequencies within prediction bins vary from the overall global sample base rate.
- **Uncertainty**: The variance of the true labels $\bar{y}(1-\bar{y})$, representing intrinsic irreducible dataset noise.

### Q3: When should you choose Platt Scaling over Isotonic Regression?
**Answer:**
- Choose **Platt Scaling** when the calibration dataset is small ($n_{\text{cal}} < 1,000$) or when the calibration curve exhibits a simple sigmoidal S-shape (typical for SVMs and Logistic Regression). Platt scaling's parametric formulation (2 parameters) prevents overfitting.
- Choose **Isotonic Regression** when the dataset is large ($n_{\text{cal}} > 1,000$) and the model's calibration distortions are non-sigmoidal or asymmetric, as its non-parametric piece-wise constant formulation can fit arbitrary monotonic curves.

### Q4: Why does Naive Bayes systematically produce poorly calibrated, overconfident probabilities?
**Answer:** Naive Bayes computes posterior probabilities by multiplying individual feature conditional likelihoods: $P(y \mid \mathbf{x}) \propto P(y) \prod P(x_j \mid y)$. It assumes all features are conditionally independent given the class. In practice, real-world features are almost always correlated. Multiplying correlated evidence repeatedly causes the model to double-count supporting signals, pushing predicted probabilities asymptotically toward $0.0$ or $1.0$.

### Q5: What is Temperature Scaling, and why is it preferred for deep neural networks?
**Answer:** Temperature Scaling is a single-parameter post-processing calibration technique that divides the logit vector $\mathbf{z}$ by a learned positive scalar $T$ before the Softmax function: $\sigma(\mathbf{z} / T)$. Because $T$ scales all logits equally, it leaves the $\arg\max$ unchanged, preserving classification accuracy and top-1 ranking. It softens overconfident distributions when $T > 1$, minimizing validation cross-entropy without adding significant parameters.

### Q6: Why must calibration models be fit on a held-out dataset rather than the training set?
**Answer:** Base classification algorithms minimize training loss, often achieving near-zero error on training data through capacity memorization. If a calibration model is fit on these overfitted training predictions, it learns that high confidence scores correspond to $100\%$ accuracy, completely failing to detect that test-time predictions will be much less certain. Using a held-out split or cross-validation ensures the calibrator observes realistic out-of-sample error distributions.

### Q7: What is Expected Calibration Error (ECE), and what is its primary limitation?
**Answer:** ECE partitions predicted probabilities into $M$ bins and computes the weighted average absolute difference between average bin confidence and empirical bin accuracy: $\sum \frac{|B_m|}{n} |\text{acc}(B_m) - \text{conf}(B_m)|$. Its primary limitation is **bin sensitivity**: varying the number of bins $M$ or choosing uniform width versus equal frequency (quantile) binning produces drastically different ECE values. Furthermore, errors within a bin can cancel each other out.

### Q8: How does covariate shift or label shift affect a calibrated model in production?
**Answer:** If the target class prior shifts from $P_{\text{train}}(y=1) = 0.10$ to $P_{\text{prod}}(y=1) = 0.40$ (prior probability shift), a previously calibrated probability $\hat{p} = 0.20$ will now systematically underestimate true risk. Calibration guarantees hold only under the assumption that the joint distribution $P(\mathbf{x}, y)$ remains stationary. If class balance shifts, probabilities must be mathematically re-weighted using Bayes' rule and the new prior ratio.
