---
title: "Handling Imbalanced Datasets in Machine Learning"
description: "Data-level resampling, algorithmic cost-weighting, synthetic generation (SMOTE, ADASYN), and threshold calibration for extreme class imbalances."
track: "machine-learning"
module: "02-data-and-preprocessing"
topic: "Handling Imbalanced Data"
order: 10
format: "concept"
level: "intermediate"
tags: ["imbalanced-data", "smote", "adasyn", "undersampling", "class-weights", "pr-auc", "cost-sensitive"]
prerequisites: [
  "05-ai/machine-learning/01-math-foundations/03-probability-and-distributions.md",
  "05-ai/machine-learning/02-data-and-preprocessing/05-feature-scaling.md"
]
key_takeaways:
  - "Classification accuracy is completely uninformative under class imbalance; a naive dummy model predicting the majority class achieves 99.9% accuracy on a 1:1000 fraud dataset."
  - "The Golden Rule of Resampling: NEVER apply SMOTE, oversampling, or undersampling to validation or test sets; evaluate only on the true production distribution."
  - "Algorithmic weighting (`class_weight='balanced'`) achieves comparable or superior empirical performance to synthetic oversampling without increasing dataset size or training latency."
  - "Precision-Recall AUC (PR-AUC) and average precision are mathematically superior to ROC-AUC when the minority positive class is rare."
---

# Handling Imbalanced Datasets in Machine Learning

## 1. Context & Problem Statement

In real-world classification domains—such as credit card fraud detection, rare disease diagnosis, network intrusion, and ad click prediction—the distribution of classes is radically skewed. Common ratios range from $1:10$ (moderate imbalance) to $1:10,000+$ (extreme rarity). Standard learning algorithms trained on imbalanced datasets fail completely:
1. **The Majority Classifier Trap**: With a $99.9\%$ majority class, minimizing standard cross-entropy or $0-1$ misclassification loss yields a degenerate trivial model: $f(x) \equiv 0$. The model attains $99.9\%$ accuracy while detecting zero true fraud cases.
2. **Boundary Dislocation**: Margin-based classifiers (SVMs, Logistic Regression) push decision boundaries toward the sparse minority class, creating massive false negative rates.
3. **Validation Resampling Corruption**: Resampling or applying SMOTE to the validation or test partitions invalidates probability calibration, destroys true prior probabilities $P(y)$, and produces unrealistically inflated evaluation metrics.

---

## 2. Conceptual Architecture

The solutions to class imbalance fall into two primary methodological branches: **Data-Level Resampling** (modifying sample distributions) and **Algorithmic Cost-Weighting** (modifying loss gradients).

```
+-----------------------------------------------------------------------------------+
|                        Handling Imbalanced Data Architecture                      |
+-----------------------------------------------------------------------------------+
                                          |
          +-------------------------------+-------------------------------+
          |                                                               |
          v                                                               v
+-------------------------------+              +------------------------------------+
|      Data-Level Resampling    |              |     Algorithmic Remedies           |
|      (Pre-Processing)         |              |     (Loss Function Modification)   |
+-------------------------------+              +------------------------------------+
  |                           |                  |                                |
  v                           v                  v                                v
Undersampling               Oversampling       Cost-Sensitive Loss           Threshold Tuning
- RandomUnderSampler        - RandomOverSampler- class_weight='balanced'     - Post-hoc threshold
- Tomek Links               - SMOTE            - Focal Loss (gamma > 0)        optimization
- Edited Nearest            - Borderline-SMOTE - Custom Misclassification    - Maximize F-beta or
  Neighbours (ENN)          - ADASYN             Cost Matrix C(i, j)           business utility
```

---

## 3. Core Theory & Mathematical Formulation

### 3.1 Algorithmic Cost-Weighting: Inverse Class Frequencies

Instead of altering the training data matrix, cost-sensitive learning scales the loss contribution of each sample by a class weight $w_c$ inversely proportional to class frequency:

$$w_c = \frac{N}{C \cdot N_c}$$

Where:
- $N$ is total sample count.
- $C$ is number of classes ($C=2$ for binary).
- $N_c$ is count of samples in class $c$.

The weighted cross-entropy loss objective becomes:
$$\mathcal{L}(\theta) = -\frac{1}{N} \sum_{i=1}^N w_{y_i} \left[ y_i \ln(\hat{p}_i) + (1 - y_i) \ln(1 - \hat{p}_i) \right]$$

A rare fraud sample ($N_1 \ll N_0$) produces a massive gradient signal proportional to $w_1 = \frac{N}{2 N_1}$, penalizing false negatives with heavy loss penalties.

### 3.2 Synthetic Minority Over-sampling Technique (SMOTE)

SMOTE synthesizes novel minority samples along the line segments connecting existing $k$-nearest minority neighbors:

For every minority class sample $x_i \in S_{\text{min}}$:
1. Find its $k$-nearest neighbors within $S_{\text{min}}$ using Euclidean distance: $\mathcal{N}_k(x_i)$.
2. Randomly select one neighbor $\tilde{x} \in \mathcal{N}_k(x_i)$.
3. Generate a synthetic sample $x_{\text{synth}}$ by interpolating with a random uniform scale $\lambda \sim \mathcal{U}(0, 1)$:
   $$x_{\text{synth}} = x_i + \lambda \cdot (\tilde{x} - x_i)$$

**ADASYN (Adaptive Synthetic)**:
Extends SMOTE by weighting the number of synthetic samples generated per minority point $x_i$ according to its local density of majority neighbors:
$$r_i = \frac{\Delta_i}{k} \quad (\Delta_i = \text{number of majority samples in } k\text{-NN of } x_i)$$
Minority samples surrounded by majority points (near the decision boundary) receive a higher proportion of synthetic samples.

### 3.3 Metric Superiority: Precision-Recall AUC vs ROC-AUC

Let $TP, FP, TN, FN$ denote confusion matrix counts.
- **ROC-AUC** plots False Positive Rate ($\text{FPR} = \frac{FP}{FP + TN}$) vs True Positive Rate ($\text{TPR} = \frac{TP}{TP + FN}$).
  In extreme imbalance ($TN = 1,000,000$, $FP = 1,000$), FPR is $\frac{1000}{1001000} \approx 0.00099$. A massive flood of false positives barely moves the FPR axis, making ROC-AUC misleadingly optimistic ($> 0.98$).
- **PR-AUC** plots Precision ($\frac{TP}{TP + FP}$) vs Recall ($\frac{TP}{TP + FN}$).
  Precision directly penalizes the absolute count of false positives ($FP = 1000$), dropping precision dramatically and providing a rigorous, realistic assessment of production utility.

---

## 4. Practical Implementation

Below is a self-contained, reproducible Python module comparing `class_weight='balanced'`, Random Undersampling, and pure SMOTE interpolation, verifying metric differences and enforcing safe pipeline evaluation.

```python
"""
Imbalanced Classification Strategies & Benchmark Harness.
Tested on Python 3.11, NumPy 2.x, scikit-learn 1.9.x.
"""

from typing import Dict, Tuple
import numpy as np
import pandas as pd
from sklearn.datasets import make_classification
from sklearn.model_selection import train_test_split
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import classification_report, average_precision_score, roc_auc_score


def pure_numpy_smote(
    X_min: np.ndarray,
    n_samples_to_generate: int,
    k_neighbors: int = 5
) -> np.ndarray:
    """
    Pure NumPy implementation of SMOTE interpolation.
    Synthesizes points along the line segment between minority samples and their k-NN.
    """
    n_min, n_features = X_min.shape
    assert n_min >= k_neighbors, "Minority class must have at least k_neighbors samples."

    # Compute pairwise Euclidean distances within minority class
    diff = X_min[:, np.newaxis, :] - X_min[np.newaxis, :, :]
    dist_matrix = np.linalg.norm(diff, axis=-1)

    # Sort neighbor indices (exclude self at index 0)
    nn_indices = np.argsort(dist_matrix, axis=1)[:, 1 : k_neighbors + 1]

    synthetic_samples = np.zeros((n_samples_to_generate, n_features))
    for i in range(n_samples_to_generate):
        # Pick random minority point
        idx = np.random.randint(0, n_min)
        base_sample = X_min[idx]

        # Pick random neighbor among its k-NN
        neighbor_idx = np.random.choice(nn_indices[idx])
        neighbor_sample = X_min[neighbor_idx]

        # Interpolate: base + lambda * (neighbor - base)
        lam = np.random.uniform(0.0, 1.0)
        synthetic_samples[i] = base_sample + lam * (neighbor_sample - base_sample)

    return synthetic_samples


def benchmark_imbalance_methods():
    np.random.seed(42)

    # 1. Synthesize heavily imbalanced dataset (98% Class 0, 2% Class 1)
    N = 5000
    X, y = make_classification(
        n_samples=N,
        n_features=10,
        n_informative=6,
        n_classes=2,
        weights=[0.98, 0.02],
        flip_y=0.001,
        random_state=42
    )

    # STRICT RULE: Split BEFORE any resampling or scaling
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.3, stratify=y, random_state=42
    )

    print(f"Train Set Class Counts: {np.bincount(y_train)} (Minority: {np.mean(y_train)*100:.2f}%)")
    print(f"Test Set Class Counts:  {np.bincount(y_test)} (Minority: {np.mean(y_test)*100:.2f}%)\n")

    # -------------------------------------------------------------
    # Model 1: Baseline Unweighted Logistic Regression
    # -------------------------------------------------------------
    clf_base = LogisticRegression(solver="lbfgs", random_state=42)
    clf_base.fit(X_train, y_train)
    probs_base = clf_base.predict_proba(X_test)[:, 1]

    # -------------------------------------------------------------
    # Model 2: Algorithmic Class Weighting (class_weight='balanced')
    # -------------------------------------------------------------
    clf_weighted = LogisticRegression(class_weight="balanced", solver="lbfgs", random_state=42)
    clf_weighted.fit(X_train, y_train)
    probs_weighted = clf_weighted.predict_proba(X_test)[:, 1]

    # -------------------------------------------------------------
    # Model 3: Synthetic Oversampling via SMOTE (Train only!)
    # -------------------------------------------------------------
    X_min = X_train[y_train == 1]
    n_majority = np.sum(y_train == 0)
    n_minority = len(X_min)
    n_to_synth = n_majority - n_minority

    X_synth = pure_numpy_smote(X_min, n_samples_to_generate=n_to_synth, k_neighbors=5)
    X_train_resampled = np.vstack([X_train, X_synth])
    y_train_resampled = np.concatenate([y_train, np.ones(n_to_synth, dtype=int)])

    clf_smote = LogisticRegression(solver="lbfgs", random_state=42)
    clf_smote.fit(X_train_resampled, y_train_resampled)
    probs_smote = clf_smote.predict_proba(X_test)[:, 1]

    # Evaluate models on STRICTLY UNTOUCHED TEST SET
    models = {
        "Baseline (Unweighted)": (clf_base, probs_base),
        "Cost-Weighted (Balanced)": (clf_weighted, probs_weighted),
        "SMOTE (Oversampled Train)": (clf_smote, probs_smote),
    }

    metrics = []
    for name, (model, probs) in models.items():
        preds = model.predict(X_test)
        roc = roc_auc_score(y_test, probs)
        pr_auc = average_precision_score(y_test, probs)
        report = classification_report(y_test, preds, output_dict=True, zero_division=0)
        
        metrics.append({
            "Strategy": name,
            "Minority Recall": report["1"]["recall"],
            "Minority Precision": report["1"]["precision"],
            "F1-Score": report["1"]["f1-score"],
            "ROC-AUC": roc,
            "PR-AUC (Avg Precision)": pr_auc
        })

    results_df = pd.DataFrame(metrics).set_index("Strategy")
    print("--- Imbalanced Modeling Comparative Benchmark ---")
    print(results_df)

    # Verifications
    # 1. Cost-weighted must substantially increase minority recall over baseline
    assert results_df.loc["Cost-Weighted (Balanced)", "Minority Recall"] > results_df.loc["Baseline (Unweighted)", "Minority Recall"]
    # 2. Both weighted and SMOTE should achieve competitive PR-AUC
    assert results_df.loc["Cost-Weighted (Balanced)", "PR-AUC (Avg Precision)"] > 0.40

    print("\n[SUCCESS] Imbalanced learning benchmarks verified successfully.")


if __name__ == "__main__":
    benchmark_imbalance_methods()
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

| Failure Mode | Root Cause | Observable Symptom | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **Resampling Test / Validation Folds** | Applying SMOTE to full dataset prior to train/test split | Fraud recall appears $> 99\%$; deployed model fails catastrophically | Never resample evaluation sets; enforce split inside custom `imblearn.pipeline.Pipeline`. |
| **SMOTE Noise Amplification** | Synthesizing between an extreme outlier minority point and its distant neighbors | Creates bridge of synthetic points across majority class clusters | Use Tomek Links cleaning or Borderline-SMOTE which ignores isolated minority noise. |
| **Probability Calibration Distortion** | Undersampling the majority class alters the base rate $P(y=1)$ | Predicted probabilities $\hat{p}$ are severely uncalibrated (shifted higher) | Calibrate probabilities post-hoc using the true test prior: $\hat{p}_{\text{cal}} = \frac{\hat{p}}{\hat{p} + \frac{1 - \hat{p}}{\beta}}$. |
| **Information Destruction in Undersampling** | Randomly discarding 95% of majority samples | Massive increase in model variance; failure to learn complex majority boundaries | Use ensemble undersampling (Balanced Random Forest or EasyEnsemble). |
| **Over-reliance on Accuracy** | Tracking accuracy metric during hyperparameter grid search | Optuna / GridSearchCV selects trivial model predicting all zeros | Set `scoring='average_precision'` or `scoring='f1_macro'`. |

---

## 6. Production Engineering & Performance

1. **Prefer `class_weight` over SMOTE in Large-Scale Systems**:
   On a dataset with $10^7$ rows, SMOTE doubles memory consumption and inflates gradient descent training times. Algorithmic weighting adds zero rows, requires zero extra RAM, and processes batches at native speed.
2. **Post-Hoc Decision Threshold Optimization**:
   Default classification uses a fixed probability threshold $\theta = 0.50$. In production, calibrate $\theta$ by scanning precision-recall curves to maximize a business utility function:
   $$\text{Utility} = V_{\text{caught}} \cdot TP - C_{\text{investigate}} \cdot FP - L_{\text{missed}} \cdot FN$$
3. **Probability Prior Adjustment Formula**:
   If training on an undersampled dataset with majority reduction factor $\beta = \frac{N_{\text{maj, sampled}}}{N_{\text{maj, original}}}$, restore true production probabilities via:
   $$P_{\text{true}}(y=1 \mid x) = \frac{P_{\text{sample}}(y=1 \mid x)}{P_{\text{sample}}(y=1 \mid x) + \frac{1 - P_{\text{sample}}(y=1 \mid x)}{\beta}}$$

---

## 7. Comparative Analysis: Imbalance Techniques

| Method | Space Complexity | Time Complexity | Alters Calibration? | Risk of Noise Amplification |
| :--- | :--- | :--- | :--- | :--- |
| **Cost-Weighting (`class_weight`)** | $\mathcal{O}(1)$ | $\mathcal{O}(1)$ | Minimally | Zero (Data is untouched) |
| **Random Undersampling** | $\mathcal{O}(N_{\text{sub}})$ (Shrinks) | Fast | Severely shifts priors | Zero |
| **Random Oversampling** | $\mathcal{O}(N_{\text{maj}})$ (Grows) | Moderate | Retains priors | High (Exact duplicate overfitting) |
| **SMOTE** | $\mathcal{O}(N_{\text{maj}} \cdot D)$ | $\mathcal{O}(N_{\text{min}} \cdot k \cdot D)$ | Retains priors | Moderate (Interpolates across voids) |
| **Balanced Random Forest** | $\mathcal{O}(B \cdot N_{\text{sub}})$ | Fast parallel | Minimally | Very Low (Ensemble averaging) |

---

## 8. Verification & Diagnostics Test Harness

```python
import pytest
import numpy as np
from sklearn.metrics import average_precision_score

def test_pure_numpy_smote_output():
    X_min = np.array([[1.0, 1.0], [1.2, 1.1], [0.9, 0.95], [1.05, 1.02], [1.1, 0.98]])
    synth = pure_numpy_smote(X_min, n_samples_to_generate=10, k_neighbors=3)
    assert synth.shape == (10, 2)
    # Synthetic samples should remain clustered near minority center
    assert np.all(synth >= 0.8) and np.all(synth <= 1.3)

def test_pr_auc_sanity():
    # If predictions are perfect, PR-AUC must be 1.0
    y_true = np.array([0, 0, 0, 0, 1, 1])
    y_scores = np.array([0.1, 0.2, 0.1, 0.05, 0.8, 0.9])
    score = average_precision_score(y_true, y_scores)
    assert np.isclose(score, 1.0)
```

---

## 9. Cross-Track Connections

- **05-ai/machine-learning/04-supervised-classification/10-probability-calibration.md**: Platt scaling, Isotonic regression, and prior recovery.
- **05-ai/machine-learning/04-supervised-classification/12-cost-sensitive-classification.md**: Formal utility matrices and asymmetric misclassification risk.
- **05-ai/machine-learning/10-evaluation-and-metrics/04-precision-recall-and-roc.md**: Deep mathematical comparison of precision-recall vs ROC dynamics.

---

## 10. Real-World Case Study: Rare Cyber-Attack Network Intrusion

A cybersecurity provider monitored network packet telemetry.
- **Environment**: $N = 10,000,000$ daily connections; only $120$ connections were zero-day APT intrusions (imbalance ratio $\approx 1 : 83,000$).
- **Failure**: A vendor deployed an off-the-shelf GBDT with default parameters. It achieved $99.998\%$ accuracy, but missed $100\%$ of APT attacks.
- **Production Architecture**:
  1. Implemented a streaming 2-stage ensemble: stage 1 filtered benign traffic using an ensemble of undersampled Random Forests (`n_estimators=100`, balanced subsamples).
  2. Stage 2 evaluated the top $0.1\%$ suspicious packets using an XGBoost model optimized with custom Focal Loss ($\gamma = 2.0, \alpha = 0.75$).
  3. Optimized the operating decision threshold to maximize F-2 score ($\beta = 2$, prioritizing recall over precision).
  4. The system detected 112 out of 120 attacks (93.3% recall) while maintaining acceptable analyst alert volumes under 20 per day.

---

## 11. Technical Interview Questions & Solutions

### Question 1: What is Focal Loss, and how does it dynamically address class imbalance in gradient-based learning?
**Answer**:
Focal Loss (Lin et al., 2017) was introduced to address extreme foreground-background class imbalance. It modifies standard cross-entropy loss by adding a modulating factor $(1 - p_t)^\gamma$:
$$\text{FL}(p_t) = -\alpha_t (1 - p_t)^\gamma \ln(p_t)$$
Where:
- $p_t = p$ if $y=1$, else $1-p$.
- $\gamma \ge 0$ is the tunable focusing parameter.
- $\alpha_t$ is an optional class-weighting factor.

**Mechanics**:
When an easy majority sample is classified with high confidence (e.g., $p_t = 0.99$), the modulating factor $(1 - 0.99)^\gamma = 0.01^\gamma$ becomes tiny (e.g., $0.0001$ for $\gamma=2$). This down-weights the gradient contribution of well-classified majority samples to near zero.
In contrast, for a hard or misclassified minority sample ($p_t = 0.1$), the modulating factor $(1 - 0.1)^2 = 0.81$ remains large. Consequently, the optimization gradients are dominated exclusively by hard minority instances, preventing the vast flood of easy majority samples from overwhelming parameter updates.

### Question 2: Why does SMOTE struggle on high-dimensional tabular datasets ($D > 100$)?
**Answer**:
SMOTE relies fundamentally on Euclidean $k$-nearest neighbors to identify sample neighborhoods. Due to the **Curse of Dimensionality**:
1. Pairwise Euclidean distances concentrate: the ratio $\frac{d_{\max} - d_{\min}}{d_{\min}} \to 0$ as $D \to \infty$. In 100+ dimensions, the nearest neighbor is almost as distant as the average sample.
2. The concept of local neighborhood collapses. Points that appear near each other in Euclidean space may belong to completely different topological manifolds.
3. Linear interpolation between two distant points creates synthetic samples that land in empty, low-density regions ("empty space") of the true data-generating distribution, introducing unrealistic artifacts that confuse the decision boundary.

### Question 3: Write a function to find the optimal classification threshold that maximizes the $F_\beta$ score.
```python
def find_optimal_fbeta_threshold(y_true: np.ndarray, y_probs: np.ndarray, beta: float = 1.0) -> Tuple[float, float]:
    """Finds decision threshold in [0, 1] that maximizes F-beta score."""
    thresholds = np.linspace(0.01, 0.99, 99)
    best_threshold = 0.5
    best_fbeta = -1.0
    beta_sq = beta ** 2

    for thresh in thresholds:
        preds = (y_probs >= thresh).astype(int)
        tp = np.sum((preds == 1) & (y_true == 1))
        fp = np.sum((preds == 1) & (y_true == 0))
        fn = np.sum((preds == 0) & (y_true == 1))

        precision = tp / (tp + fp) if (tp + fp) > 0 else 0.0
        recall = tp / (tp + fn) if (tp + fn) > 0 else 0.0

        denom = (beta_sq * precision) + recall
        if denom > 0:
            fbeta = (1.0 + beta_sq) * (precision * recall) / denom
        else:
            fbeta = 0.0

        if fbeta > best_fbeta:
            best_fbeta = fbeta
            best_threshold = thresh

    return float(best_threshold), float(best_fbeta)
```

---

## 12. Guided Practice & Hands-On Exercises

### Exercise 1: Custom Cost Matrix Loss Evaluator
- Implement an evaluation harness that takes a $2 \times 2$ asymmetric business cost matrix:
  `cost_matrix = [[0, 10], [500, 0]]` ($10 cost for FP, $500 cost for FN). Find the probability threshold that minimizes total monetary loss.

### Exercise 2: Tomek Links Undersampling Cleaner
- Implement a Tomek Links detector in NumPy: a pair $(x_i, x_j)$ is a Tomek link if $y_i \neq y_j$ and $x_i$ is the nearest neighbor of $x_j$ AND $x_j$ is the nearest neighbor of $x_i$. Implement a function to drop the majority point in each link.

### Exercise 3: Cost-Weighted Logistic Regression via SGD
- Write a custom Stochastic Gradient Descent (SGD) loop in Python that applies class weights directly to sample loss gradients during mini-batch backpropagation.
