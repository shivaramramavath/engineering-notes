# Multiclass & Multilabel Classification Strategies

> **Track:** `05-ai/machine-learning` | **Module:** `04-supervised-classification` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `NumPy: >=1.26.0` | `Python: >=3.11`  
> **Type:** `Core Concept Architecture & Engineering Guide`

---

## 1. Concept Definition & Purpose

Standard binary classification maps input features $\mathbf{x} \in \mathbb{R}^d$ to a single binary label $y \in \{0, 1\}$. Real-world enterprise systems frequently require classification across non-binary target topologies:

1. **Multiclass Classification (Single-Label, Multi-Category)**:
   Each instance belongs to **exactly one** mutually exclusive class among $K > 2$ categories (e.g., classifying a vehicle as `[Sedan, SUV, Truck, Coupe]`).
   $$y \in \{1, 2, \dots, K\}, \quad \sum_{k=1}^K \mathbb{I}(y = k) = 1$$
2. **Multilabel Classification (Multi-Label, Non-Exclusive)**:
   Each instance may simultaneously belong to **zero, one, or multiple** categories among $K$ labels (e.g., tagging a customer support ticket with `['Billing', 'Technical Support', 'Urgent']`).
   $$\mathbf{y} \in \{0, 1\}^K$$
3. **Multioutput-Multiclass Classification**:
   Each instance is simultaneously assigned labels across multiple distinct multiclass targets (e.g., predicting an image's fruit type `[Apple, Orange, Banana]` AND its ripeness `[Unripe, Ripe, Overripe]`).

---

## 2. Theoretical Mechanics & Math Formulations

### 1. One-vs-Rest (OvR / One-vs-All)
Decomposes a $K$-class problem into $K$ independent binary classification tasks. For each class $k \in \{1, \dots, K\}$, a dedicated binary classifier $f_k(\mathbf{x})$ is trained to separate class $k$ (positive class) from all other $K-1$ classes combined (negative class).

```
Training Data (Classes 0, 1, 2)
  ├── Classifier 0: Class 0 vs {Class 1, Class 2}  --> f_0(x)
  ├── Classifier 1: Class 1 vs {Class 0, Class 2}  --> f_1(x)
  └── Classifier 2: Class 2 vs {Class 0, Class 1}  --> f_2(x)
```

At inference time, evaluate all $K$ classifiers and select the class with the highest raw confidence score or decision margin:

$$\hat{y} = \arg\max_{k \in \{1, \dots, K\}} f_k(\mathbf{x})$$

- **Total Binary Classifiers**: $K$
- **Sample Imbalance**: High (in each sub-problem, negative instances vastly outnumber positive instances by a ratio of $K-1 : 1$).

### 2. One-vs-One (OvO / Pairwise Decomposition)
Decomposes a $K$-class problem into binary classifiers for every unique pair of classes $(j, k)$ where $j < k$. Each classifier $f_{jk}(\mathbf{x})$ is trained strictly on the subset of data belonging to classes $j$ and $k$, completely ignoring data from all other $K-2$ classes.

- **Total Binary Classifiers**: 
  $$\binom{K}{2} = \frac{K(K - 1)}{2}$$
- **Inference Voting**: Every classifier votes for one of its two classes. The class with the highest total vote count wins:
  $$\hat{y} = \arg\max_{k} \sum_{j \neq k} \mathbb{I}(\text{Classifier } (j, k) \text{ votes for } k)$$
- **Computational Advantage**: While it trains $\approx \frac{K^2}{2}$ models, each model is trained on a small fraction of the total dataset ($2/K$), which is advantageous for algorithms with super-linear scaling like non-linear SVMs ($O(n^3)$).

### 3. Native Multinomial Softmax
Certain algorithm families (Multinomial Logistic Regression, Neural Networks) handle multiclass targets natively without binary decomposition by producing a logit vector $\mathbf{z} = [z_1, \dots, z_K]^T \in \mathbb{R}^K$ and applying the Softmax function:

$$P(y = k \mid \mathbf{x}) = \frac{e^{\mathbf{w}_k^T \mathbf{x} + b_k}}{\sum_{j=1}^K e^{\mathbf{w}_j^T \mathbf{x} + b_j}}$$

Optimization minimizes the Categorical Cross-Entropy loss directly across all $K$ classes simultaneously.

### 4. Multilabel Strategies: Binary Relevance vs Classifier Chains vs Label Powerset
For multilabel problems with target vector $\mathbf{y} \in \{0, 1\}^K$:

1. **Binary Relevance**:
   Fits $K$ independent binary classifiers, one for each label.
   - *Limitation*: Strictly assumes label independence ($P(y_1, y_2 \mid \mathbf{x}) = P(y_1 \mid \mathbf{x}) P(y_2 \mid \mathbf{x})$), completely ignoring correlations between co-occurring labels.
2. **Classifier Chains**:
   Models label correlations by chaining $K$ binary classifiers in a specified sequence. For classifier $j \in \{1, \dots, K\}$, the input feature space is augmented with the ground-truth (or predicted) states of all preceding labels:
   $$\hat{y}_j = f_j([\mathbf{x}, y_1, y_2, \dots, y_{j-1}])$$
3. **Label Powerset**:
   Transforms the multilabel task into a single multiclass task where each unique binary label combination is treated as an atomic class.
   - *Limitation*: Can create up to $2^K$ classes, resulting in extreme class sparsity and poor generalization on unseen label combinations.

---

## 3. Architectural Topology & Flow Diagrams

```
+--------------------------------------------------------------------------------------------------+
|                            MULTICLASS & MULTILABEL DECISION FLOWCHART                            |
|                                                                                                  |
|                      Are classes mutually exclusive for each sample?                             |
|                                     /               \                                            |
|                                  [YES]              [NO]                                         |
|                                   /                    \                                         |
|                         Multiclass Task                 Multilabel Task (y in {0, 1}^K)          |
|                                 |                                      |                         |
|             Does base model support native multiclass?          Are labels correlated?           |
|                 (Tree ensembles, Neural Nets)                  (e.g., Illness & Symptoms)        |
|                       /              \                               /             \             |
|                    [YES]             [NO]                         [YES]            [NO]          |
|                     /                  \                           /                 \           |
|            Native Softmax / CART   Does model scale           Classifier Chains    Binary        |
|                                   quadratically with n?     (MultiOutput)        Relevance       |
|                                       /            \                                             |
|                                    [YES]           [NO]                                          |
|                                     /                \                                           |
|                            One-vs-One (OvO)    One-vs-Rest (OvR)                                 |
|                                 (SVC)            (LinearSVC)                                     |
+--------------------------------------------------------------------------------------------------+
```

---

## 4. Production Scikit-Learn Implementation

```python
"""
Production implementation of Multiclass and Multilabel Classification strategies.
Demonstrates OneVsRest, OneVsOne, and Classifier Chains with rigorous evaluation.
"""

from __future__ import annotations
import logging
from typing import Dict, Any
import numpy as np
from sklearn.datasets import make_multilabel_classification, load_iris
from sklearn.model_selection import train_test_split
from sklearn.multiclass import OneVsRestClassifier, OneVsOneClassifier
from sklearn.multioutput import ClassifierChain
from sklearn.linear_model import LogisticRegression
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import (
    classification_report,
    hamming_loss,
    f1_score,
    jaccard_score
)

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("MultiStrategy_Pipeline")


def run_multilabel_chain_pipeline() -> Dict[str, float]:
    """Execute Multilabel Classification via Classifier Chains to model label dependencies."""
    logger.info("Synthesizing multilabel dataset (K=5 correlated labels).")
    X, Y = make_multilabel_classification(
        n_samples=2000,
        n_features=20,
        n_classes=5,
        n_labels=2,
        length=50,
        allow_unlabeled=False,
        random_state=42
    )

    X_train, X_test, Y_train, Y_test = train_test_split(
        X, Y, test_size=0.25, random_state=42
    )

    # 1. Baseline: Independent Binary Relevance (One-vs-Rest)
    logger.info("Evaluating baseline independent Binary Relevance.")
    base_ovr = OneVsRestClassifier(LogisticRegression(max_iter=500), n_jobs=-1)
    base_ovr.fit(X_train, Y_train)
    y_pred_ovr = base_ovr.predict(X_test)

    # 2. Advanced: Classifier Chain (Chained dependency modeling)
    logger.info("Evaluating Classifier Chain with Logistic Regression base.")
    base_lr = LogisticRegression(max_iter=500)
    # Order specifies sequential dependency: 0 -> 1 -> 2 -> 3 -> 4
    chain = ClassifierChain(base_lr, order="random", random_state=42)
    chain.fit(X_train, Y_train)
    y_pred_chain = chain.predict(X_test)

    # 3. Metrics Comparison
    hl_ovr = float(hamming_loss(Y_test, y_pred_ovr))
    hl_chain = float(hamming_loss(Y_test, y_pred_chain))
    f1_ovr = float(f1_score(Y_test, y_pred_ovr, average="micro"))
    f1_chain = float(f1_score(Y_test, y_pred_chain, average="micro"))
    jaccard_chain = float(jaccard_score(Y_test, y_pred_chain, average="samples"))

    logger.info(f"Binary Relevance Hamming Loss: {hl_ovr:.4f} | Micro F1: {f1_ovr:.4f}")
    logger.info(f"Classifier Chain Hamming Loss: {hl_chain:.4f} | Micro F1: {f1_chain:.4f} | Sample Jaccard: {jaccard_chain:.4f}")

    return {
        "ovr_hamming_loss": hl_ovr,
        "chain_hamming_loss": hl_chain,
        "chain_micro_f1": f1_chain,
        "chain_jaccard": jaccard_chain,
    }


def run_multiclass_ovo_vs_ovr_comparison() -> Dict[str, float]:
    """Compare OvR vs OvO on Iris dataset using Support Vector Classifier."""
    X, y = load_iris(return_X_y=True)
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.3, stratify=y, random_state=42)

    from sklearn.svm import SVC
    # One-vs-Rest
    ovr_svc = OneVsRestClassifier(SVC(kernel="rbf", C=1.0)).fit(X_train, y_train)
    # One-vs-One
    ovo_svc = OneVsOneClassifier(SVC(kernel="rbf", C=1.0)).fit(X_train, y_train)

    acc_ovr = float(ovr_svc.score(X_test, y_test))
    acc_ovo = float(ovo_svc.score(X_test, y_test))

    logger.info(f"Multiclass OvR Accuracy: {acc_ovr:.4f} | OvO Accuracy: {acc_ovo:.4f}")
    return {"ovr_acc": acc_ovr, "ovo_acc": acc_ovo}


if __name__ == "__main__":
    ml_res = run_multilabel_chain_pipeline()
    mc_res = run_multiclass_ovo_vs_ovr_comparison()
```

---

## 5. Edge Cases & Failure Modes

1. **Ambiguous Geometric Regions in OvR**:
   In OvR, classifiers are trained independently. At inference time, geometric regions may arise where all $K$ binary classifiers output negative decision values ($f_k(\mathbf{x}) < 0$), or where multiple classifiers output conflicting high positive values ($f_1(\mathbf{x}) = 0.9, f_2(\mathbf{x}) = 0.95$). Selecting $\arg\max f_k(\mathbf{x})$ resolves the tie, but calibrated confidence is destroyed.
2. **Chain Error Propagation in Classifier Chains**:
   If an early classifier in the chain makes an error on label $y_1$, this corrupt prediction is passed as an input feature into all downstream classifiers ($f_2, \dots, f_K$), compounding errors down the chain.
   - *Mitigation*: Train an **Ensemble of Classifier Chains** (`order='random'`) and average their predictions.
3. **Severe Sample Imbalance in OvR**:
   For $K=50$ classes, each binary OvR subproblem faces a $49:1$ class imbalance. Classifiers without proper balancing (`class_weight='balanced'`) degenerate into predicting the majority negative class.

---

## 6. Comparative Analysis Table

| Dimension | One-vs-Rest (OvR) | One-vs-One (OvO) | Native Multinomial (Softmax) | Binary Relevance (Multilabel) | Classifier Chains (Multilabel) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Number of Models** | $K$ | $\frac{K(K-1)}{2}$ | $1$ | $K$ | $K$ |
| **Training Speed** | Fast ($K$ runs on full data) | Fast for super-linear models | Fastest (single optimization) | Fast | Moderate (sequential dependencies) |
| **Inference Latency** | $O(K \cdot \text{infer})$ | $O(K^2 \cdot \text{infer})$ | $O(1 \cdot \text{infer})$ | $O(K \cdot \text{infer})$ | $O(K \cdot \text{infer})$ (Strictly sequential) |
| **Label Correlation** | Completely Ignored | Ignored | Jointly Modeled | Completely Ignored | Explicitly Modeled |
| **Class Imbalance** | Severe ($K-1 : 1$) | Balanced (Pairwise) | Handled natively | Depends on label sparsity | Handled along chain |

---

## 7. Hyperparameter & Configuration Matrix

| Strategy / Estimator | Parameter | Optimal Values | Description |
| :--- | :--- | :--- | :--- |
| `OneVsRestClassifier` | `n_jobs` | `-1` | Parallelizes fitting of the $K$ binary models across CPU cores. |
| `OneVsOneClassifier` | `n_jobs` | `-1` | Parallelizes fitting of the $K(K-1)/2$ models. |
| `ClassifierChain` | `order` | `'random'` or custom array | Sequence of label chaining. Using `'random'` within an ensemble mitigates error propagation. |
| `LogisticRegression` | `multi_class` | `'multinomial'` | Enables native Softmax cross-entropy optimization instead of heuristic OvR. |

---

## 8. Diagnostics, Monitoring & Error Metrics

Multilabel classification requires specialized metrics that account for partial label correctness:

1. **Hamming Loss**:
   The fraction of label predictions that are incorrect across all samples and all labels:
   $$\text{HammingLoss} = \frac{1}{n \cdot K} \sum_{i=1}^n \sum_{k=1}^K \mathbb{I}(\hat{y}_{ik} \neq y_{ik})$$
   (Optimal value is $0.0$).
2. **Subset Accuracy (Exact Match Ratio)**:
   Measures the fraction of samples where the predicted label vector $\hat{\mathbf{y}}_i$ matches the ground-truth vector $\mathbf{y}_i$ across **all** $K$ labels exactly:
   $$\text{SubsetAccuracy} = \frac{1}{n} \sum_{i=1}^n \mathbb{I}(\hat{\mathbf{y}}_i == \mathbf{y}_i)$$
   (Highly stringent; fails if even one label among 50 is wrong).
3. **Macro vs Micro vs Weighted F1**:
   - **Micro F1**: Aggregates total true positives, false negatives, and false positives globally. Emphasizes majority classes.
   - **Macro F1**: Calculates unweighted F1 independently per class, then takes the average. Gives equal voice to rare minority classes.
   - **Weighted F1**: Calculates F1 per class, weighted by class support (frequency).

---

## 9. Production Engineering & Serving Considerations

- **Asynchronous Execution for OvR**: Because OvR binary models are completely decoupled, they can be deployed as independent microservice containers and evaluated asynchronously via `asyncio.gather()`.
- **Chain Latency Bottleneck**: Classifier chains cannot be parallelized during inference because model $k$ strictly requires the output of model $k-1$ as an input feature.
- **Threshold Tuning per Label**: In multilabel classification, applying a single static threshold ($0.5$) across all $K$ labels is suboptimal. Optimize individual decision thresholds $\tau_k \in [0.1, 0.9]$ independently per label on validation data to maximize label-specific F1.

---

## 10. Automated Verification Harness

```python
"""
Pytest automated verification harness for Multiclass and Multilabel strategies.
Asserts correct output shapes, probability matrix dimensions, and metric bounds.
"""

import numpy as np
import pytest
from sklearn.datasets import make_multilabel_classification, make_classification
from sklearn.multiclass import OneVsRestClassifier
from sklearn.linear_model import LogisticRegression
from sklearn.multioutput import ClassifierChain
from sklearn.metrics import hamming_loss


def test_multilabel_output_shapes():
    """Verify that multilabel models output binary 2D arrays matching target dimensions."""
    X, Y = make_multilabel_classification(n_samples=100, n_features=10, n_classes=4, random_state=42)
    chain = ClassifierChain(LogisticRegression()).fit(X, Y)

    preds = chain.predict(X)
    assert preds.shape == (100, 4), f"Expected shape (100, 4), got {preds.shape}"
    assert set(np.unique(preds)).issubset({0, 1}), "Predictions must be binary {0, 1}."


def test_hamming_loss_bounds():
    """Hamming loss must be bounded within [0.0, 1.0]."""
    X, Y = make_multilabel_classification(n_samples=50, n_features=8, n_classes=3, random_state=42)
    clf = OneVsRestClassifier(LogisticRegression()).fit(X, Y)
    preds = clf.predict(X)

    hl = hamming_loss(Y, preds)
    assert 0.0 <= hl <= 1.0, f"Hamming loss {hl} outside valid range [0, 1]."
```

---

## 11. Cross-Track Learning & Ecosystem Links

- **Mathematical Foundations**: Extends probability formulas in `01-math-foundations/03-probability-and-distributions`.
- **Metric Definitions**: Detailed metric explanations in `05-ai/machine-learning/10-evaluation-and-metrics`.
- **Backend Serving**: Deploy multi-label prediction pipelines in `03-backend/fastapi/16-ai-and-ml-serving`.

---

## 12. Enterprise Case Study

### Context: Automated Customer Support Ticket Routing & Tagging
- **Domain**: Cloud SaaS Platform ($50\text{K}$ support tickets/month).
- **Task**: Incoming tickets require simultaneous multiclass routing to a specialized engineering team ($K=8$ teams) AND multilabel issue tagging ($M=12$ possible tags, e.g., `['Database', 'Billing', 'P0-Outage', 'Security']`).
- **Implementation**:
  - Routing: Native multinomial Logistic Regression with TF-IDF embeddings.
  - Tagging: Ensemble of 10 Classifier Chains using `HistGradientBoostingClassifier` base estimators.
- **Outcome**: Improved exact match subset accuracy by $18.4\%$ over independent binary relevance, reducing manual ticket re-routing by $42\%$.

---

## 13. Technical Interview Preparation

### Q1: What is the primary difference between Multiclass and Multilabel classification?
**Answer:** In multiclass classification, the target classes are mutually exclusive; each instance belongs to exactly one class ($y \in \{1, \dots, K\}$). In multilabel classification, target classes are non-exclusive; an instance can simultaneously possess any combination of labels ($\mathbf{y} \in \{0, 1\}^K$), including having all labels or none.

### Q2: Why is One-vs-One (OvO) preferred over One-vs-Rest (OvR) for Support Vector Machines with non-linear kernels?
**Answer:** The training complexity of kernel SVM scales between $O(n^2)$ and $O(n^3)$ with respect to sample size $n$. In OvR, each of the $K$ models is trained on the entire dataset ($n$ samples), yielding total training time $O(K \cdot n^3)$. In OvO, each of the $\frac{K(K-1)}{2}$ models is trained strictly on data from two classes ($\approx \frac{2n}{K}$ samples). Total OvO training time scales as $\frac{K(K-1)}{2} \cdot O\left(\left(\frac{2n}{K}\right)^3\right) \approx O\left(\frac{4n^3}{K}\right)$, which is substantially faster when $K$ is large.

### Q3: What is the main drawback of the Binary Relevance method in multilabel learning, and how does Classifier Chains address it?
**Answer:** Binary Relevance trains an independent binary classifier for each label, implicitly assuming label conditional independence ($P(\mathbf{y} \mid \mathbf{x}) = \prod P(y_k \mid \mathbf{x})$). It fails to model label correlations (e.g., if "Hospital" is positive, the probability of "Doctor" is greatly increased). Classifier Chains addresses this by ordering labels into a sequence and passing previous label predictions as auxiliary input features into downstream classifiers, modeling conditional dependencies via the probability chain rule.

### Q4: Explain the difference between Micro F1 and Macro F1 in multiclass evaluation.
**Answer:** Macro F1 computes the F1 score independently for each class and takes their unweighted arithmetic mean, treating all classes with equal importance regardless of frequency (making it highly sensitive to performance on rare minority classes). Micro F1 aggregates all true positives, false positives, and false negatives globally across all classes to compute a single overall precision and recall, effectively weighting each sample equally and thus reflecting majority class performance.

### Q5: How do you handle tie-breaks in One-vs-One (OvO) voting?
**Answer:** In OvO, if two or more classes receive the identical number of pairwise votes, simple majority voting results in a tie. Standard production implementations (such as LIBSVM and Scikit-Learn) resolve ties by summing the continuous decision values (signed geometric distances or posterior probabilities) of all pairwise classifiers involving the tied classes and selecting the class with the highest cumulative confidence.

### Q6: What is the Label Powerset method, and when does it fail?
**Answer:** Label Powerset maps each unique combination of labels in the multilabel training set to a unique single multiclass identifier, converting the task into an ordinary multiclass problem. It fails when the number of labels $K$ is large or when labels are weakly correlated, causing combinatorial explosion (up to $2^K$ classes), severe data sparsity (many classes having only 1 training sample), and an absolute inability to predict unseen label combinations at test time.

### Q7: Why is Subset Accuracy often an overly pessimistic metric for multilabel tasks?
**Answer:** Subset Accuracy (Exact Match Ratio) requires every single element of the predicted binary vector to match the ground truth. If a sample has 20 candidate labels, and the model correctly predicts 19 of them but misses one, subset accuracy scores the entire prediction as $0.0$. For high-dimensional label spaces, it severely underestimates model utility; Hamming Loss or Micro/Macro F1 provide more granular diagnostic insight.

### Q8: Under what conditions does One-vs-Rest (OvR) produce uncalibrated probabilities?
**Answer:** In OvR, binary models are trained completely independently without joint normalization. The sum of predicted probabilities across all $K$ binary models rarely equals $1.0$ ($\sum_{k=1}^K P_k(\mathbf{x}) \neq 1.0$). If raw outputs are normalized post-hoc via $P_k' = \frac{P_k}{\sum P_j}$, the resulting distribution can be severely distorted if some classifiers are systematically more overconfident than others. Native multinomial softmax cross-entropy ensures true joint probability calibration.
