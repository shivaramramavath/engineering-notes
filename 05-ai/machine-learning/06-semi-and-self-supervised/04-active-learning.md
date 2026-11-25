# Active Learning: Query Strategies, Human-in-the-Loop & Pool-Based Sampling

> **Track:** `05-ai/machine-learning` | **Module:** `06-semi-and-self-supervised` | **Status:** `Production Standard`  
> **Metadata:** `scikit-learn: >=1.4.0` | `NumPy: >=1.26.0` | `SciPy: >=1.12.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Active Learning (Pool-Based Selective Sampling & Query-by-Committee) |
| **Family** | Human-in-the-Loop Machine Learning / Sequential Experimental Design / Optimal Sampling |
| **Input Feature Types** | Labeled seed set $\mathcal{D}_L = \{(\mathbf{x}_i, y_i)\}_{i=1}^{n_l}$ and a massive unlabeled candidate pool $\mathcal{U} = \{\mathbf{x}_j\}_{j=1}^{n_u}$ where human annotation incurs monetary/temporal cost $C$. |
| **Target Output** | Ranked subset of informative query instances $\mathcal{Q} \subset \mathcal{U}$ ($|\mathcal{Q}| = B$) submitted to a Human Oracle for ground-truth annotation. |
| **Optimization Target** | Maximizing expected model generalization gain $\mathbb{E}[-\Delta \mathcal{L}_{\text{test}}]$ per dollar/hour of annotation effort spent. |
| **Key Metrics** | Learning Curve Area (ALC), Data Efficiency Ratio (samples saved vs random sampling to reach target F1), Annotation Budget Burn Rate, Oracle Disagreement Rate. |
| **Key Hyperparameters** | Query strategy (`'least_confidence'`, `'margin_sampling'`, `'entropy'`, `'qbc'`, `'core_set'`), batch acquisition size ($B$), committee size ($C$), uncertainty-diversity trade-off ($\beta$). |
| **Inference Mechanism** | Model predicts class posteriors $\hat{p}(y \mid \mathbf{x})$; acquisition function scores every pool candidate in parallel; top-$B$ samples are dispatched to annotation pipeline. |
| **Primary Failure Modes** | **Sampling Bias / Region Starvation**: Model relentlessly queries noisy outliers on decision boundaries while ignoring completely undiscovered dense clusters; **cold-start failure** on unrepresentative seed sets; oracle label noise. |

---

## 2. Architecture & Intuition

### The Core Premise: Not All Data Points Are Equal
In standard passive supervised learning, data points are sampled uniformly at random from the population. In many domains (medical diagnosis, fraud, rare event detection), $99\%$ of randomly sampled data points provide redundant, trivial information that teaches the model nothing new.

**Active Learning** transforms the learner from a passive recipient into an active agent that interrogates the environment:
> *"I understand patients with clear chest X-rays. But I am deeply confused by this specific scan—it looks $51\%$ like pneumonia and $49\%$ like fibrosis. Human doctor, please label this one scan."*

```
                              The Active Learning Feedback Loop
                              
                                   ┌──────────────────────┐
                                   │  Unlabeled Pool (U)  │
                                   └──────────┬───────────┘
                                              │
                                              ▼
    ┌──────────────────────┐       ┌──────────────────────┐
    │     Human Oracle     │◄──────┤ Acquisition Strategy │
    │   (Doctor / Expert)  │       │ (Uncertainty/Entropy)│
    └──────────┬───────────┘       └──────────▲───────────┘
               │ (Labeled Queries)            │ (Model Predictions)
               ▼                              │
    ┌──────────────────────┐       ┌──────────┴───────────┐
    │   Labeled Set (L)    │──────►│   Current Model f_θ  │
    └──────────────────────┘       └──────────────────────┘
```

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $\mathcal{D}_L$ | Set of pairs | Currently labeled training dataset of size $n_l$ |
| $\mathcal{U}$ | Set of feature vectors | Unlabeled pool of candidate instances of size $n_u$ |
| $\mathcal{Q}^*$ | Subset of $\mathcal{U}$ | Optimal batch of query instances to send to oracle ($|\mathcal{Q}^*| = B$) |
| $f_\theta$ | Predictive model | Current model trained on $\mathcal{D}_L$ |
| $P(y = c \mid \mathbf{x})$ | Continuous probability | Model posterior probability for class $c \in \{1, \dots, C\}$ |
| $\hat{y}_1, \hat{y}_2$ | Class labels | Most probable and second-most probable predicted classes |
| $\alpha(\mathbf{x})$ | $\mathbb{R}^d \to \mathbb{R}$ | Acquisition score function (higher = more informative) |

### The Core Uncertainty Acquisition Strategies

#### 1. Least Confidence (Lewis & Gale, 1994)
Queries the instance whose most confident class prediction is lowest:

$$\alpha_{\text{LC}}(\mathbf{x}) = 1 - \max_{c} P(y = c \mid \mathbf{x})$$

For binary classification, this prioritizes points nearest $0.50$ probability.

#### 2. Margin Sampling (Scheffer et al., 2001)
Queries the instance with the smallest margin between its top two most probable classes:

$$\alpha_{\text{Margin}}(\mathbf{x}) = P(y = \hat{y}_1 \mid \mathbf{x}) - P(y = \hat{y}_2 \mid \mathbf{x})$$

The most informative query minimizes the margin: $\mathbf{x}^* = \arg\min_{\mathbf{x} \in \mathcal{U}} \alpha_{\text{Margin}}(\mathbf{x})$. Margin sampling directly pinpoints samples straddling the decision boundary in multi-class problems.

#### 3. Shannon Entropy Sampling (Shannon, 1948; Dagan & Engelson, 1995)
Evaluates uncertainty across the entire class distribution:

$$\alpha_{\text{Entropy}}(\mathbf{x}) = -\sum_{c=1}^C P(y = c \mid \mathbf{x}) \ln P(y = c \mid \mathbf{x})$$

Maximal entropy occurs when the model predicts a uniform distribution $P(y=c) = \frac{1}{C}$.

#### 4. Query-by-Committee (QBC; Seung, Opper, Sompolinsky, 1992)
Maintains a committee of $M$ diverse models $\mathcal{C} = \{f^{(1)}, \dots, f^{(M)}\}$ trained on $\mathcal{D}_L$ with bootstrap aggregation or diverse architectures.
Queries the sample that provokes maximal disagreement among committee members, measured via **Vote Entropy**:

$$\alpha_{\text{QBC}}(\mathbf{x}) = -\sum_{c=1}^C \frac{V(c, \mathbf{x})}{M} \ln\left( \frac{V(c, \mathbf{x})}{M} \right)$$

where $V(c, \mathbf{x}) = \sum_{m=1}^M \mathbb{I}(\arg\max f^{(m)}(\mathbf{x}) = c)$ is the number of committee votes cast for class $c$.

#### 5. Diversity / Core-Set Selection (Sener & Savarese, 2018)
Uncertainty sampling alone tends to query clustered duplicates of the same ambiguous point. **Core-Set** minimizes the maximum distance from any unlabeled point to its nearest labeled neighbor:

$$\min_{\mathcal{Q} \subset \mathcal{U}, |\mathcal{Q}|=B} \max_{i \in \mathcal{U}} \min_{j \in \mathcal{D}_L \cup \mathcal{Q}} \|\mathbf{x}_i - \mathbf{x}_j\|_2$$

---

## 4. Computational Complexity

| Acquisition Strategy | Per-Query Scoring Complexity | Memory Complexity | Scalability to $|\mathcal{U}| = 1,000,000$ |
| :--- | :--- | :--- | :--- |
| **Least Confidence / Margin** | $\mathcal{O}(|\mathcal{U}| \cdot C)$ | $\mathcal{O}(|\mathcal{U}|)$ | **Extreme** (vectorized NumPy takes $< 10\text{ ms}$) |
| **Entropy Sampling** | $\mathcal{O}(|\mathcal{U}| \cdot C)$ | $\mathcal{O}(|\mathcal{U}|)$ | **Extreme** |
| **Query-by-Committee (QBC)** | $\mathcal{O}(M \cdot |\mathcal{U}| \cdot C)$ | $\mathcal{O}(M \cdot |\mathcal{U}|)$ | High |
| **Core-Set (k-Center-Greedy)** | $\mathcal{O}(B \cdot |\mathcal{U}| \cdot d)$ | $\mathcal{O}(|\mathcal{U}| \cdot d)$ | Moderate (requires spatial indexing) |
| **Expected Error Reduction** | $\mathcal{O}(|\mathcal{U}|^2 \cdot C \cdot \mathcal{T}_{\text{fit}})$ | $\mathcal{O}(|\mathcal{U}|)$ | **Intractable** (retrains model for every pool point) |

---

## 5. Inductive Bias & Statistical Assumptions

1. **Informativeness-Uncertainty Equivalence**: Assumes points near the decision boundary provide the highest gradient updates for parameter optimization.
2. **Oracle Consistency & Competence**: Assumes the Human Oracle provides correct ground truth without systemic annotator drift or noise.
3. **Representativeness**: Assumes uncertain samples are representative of the true underlying data manifold and not isolated, corrupted sensory outliers.

---

## 6. From-Scratch Implementation (NumPy)

A complete, self-contained Active Learning loop implementing Least Confidence, Margin, Entropy, and QBC strategies:

```python
"""
Pure NumPy implementation of Active Learning acquisition strategies and pool-based loops.
"""

from __future__ import annotations
import numpy as np
from typing import Literal

StrategyType = Literal["least_confidence", "margin", "entropy", "random"]

class PureNumPyActiveLearner:
    """
    Pool-Based Active Learning Coordinator.
    """
    def __init__(
        self,
        base_estimator,
        strategy: StrategyType = "entropy",
        batch_size: int = 10,
        random_state: int | None = 42
    ):
        self.base_estimator = base_estimator
        self.strategy = strategy
        self.batch_size = batch_size
        self.random_state = random_state

    def score_candidates(self, probs: np.ndarray) -> np.ndarray:
        """
        Computes acquisition score for each candidate in the unlabeled pool.
        Higher score = more valuable to label.
        
        Parameters
        ----------
        probs : np.ndarray of shape (n_pool, C)
            Predicted class probabilities.
        """
        n_pool, C = probs.shape

        if self.strategy == "least_confidence":
            # 1 - max P(y | x)
            max_p = np.max(probs, axis=1)
            return 1.0 - max_p

        elif self.strategy == "margin":
            # Difference between top 2 probabilities. Lower difference = higher uncertainty.
            # We invert so higher score = smaller margin
            sorted_p = np.sort(probs, axis=1)[:, ::-1] # Descending
            margin = sorted_p[:, 0] - sorted_p[:, 1]
            return 1.0 - margin

        elif self.strategy == "entropy":
            # - sum p * log(p)
            eps = 1e-12
            clipped = np.clip(probs, eps, 1.0 - eps)
            return -np.sum(clipped * np.log(clipped), axis=1)

        elif self.strategy == "random":
            rng = np.random.default_rng(self.random_state)
            return rng.uniform(0.0, 1.0, size=n_pool)

        else:
            raise ValueError(f"Unknown acquisition strategy: {self.strategy}")

    def query_next_batch(self, X_pool: np.ndarray) -> np.ndarray:
        """
        Selects top-B indices from the unlabeled pool to request labels.
        """
        probs = self.base_estimator.predict_proba(X_pool)
        scores = self.score_candidates(probs)
        
        # Sort descending by score
        ranked_indices = np.argsort(scores)[::-1]
        return ranked_indices[: self.batch_size]
```

---

## 7. Production Scikit-Learn Active Learning Pipeline

An enterprise-ready Human-in-the-Loop simulation pipeline benchmarking Active Learning against Random Passive Sampling:

```python
"""
Simulation Benchmarking Active Learning vs Random Passive Sampling.
"""

import numpy as np
from sklearn.datasets import make_classification
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import accuracy_score

def run_active_learning_simulation(
    n_rounds: int = 15,
    batch_size: int = 10,
    strategy: str = "entropy"
) -> dict:
    # 1. Generate imbalanced synthetic dataset (n=1200)
    rng = np.random.default_rng(42)
    X, y = make_classification(
        n_samples=1200, n_features=10, n_informative=6, n_classes=3, random_state=42
    )

    # Separate holdout test set (200 samples)
    test_idx = rng.choice(1200, size=200, replace=False)
    X_test, y_test = X[test_idx], y[test_idx]

    train_mask = np.ones(1200, dtype=bool)
    train_mask[test_idx] = False
    X_train_full = X[train_mask]
    y_train_full = y[train_mask] # Ground truth Oracle

    # 2. Seed initial labeled pool (3 samples per class = 9 total)
    n_train = len(X_train_full)
    pool_mask = np.ones(n_train, dtype=bool)
    
    initial_labeled_idx = []
    for c in np.unique(y_train_full):
        c_idx = np.where(y_train_full == c)[0][:3]
        initial_labeled_idx.extend(c_idx)
        
    pool_mask[initial_labeled_idx] = False

    # Track labeled indices
    labeled_indices = list(initial_labeled_idx)

    accuracies = []

    for round_num in range(n_rounds):
        # 1. Train model on currently labeled set
        X_labeled = X_train_full[labeled_indices]
        y_labeled = y_train_full[labeled_indices]
        
        model = LogisticRegression(max_iter=500, random_state=42).fit(X_labeled, y_labeled)
        acc = accuracy_score(y_test, model.predict(X_test))
        accuracies.append(acc)

        # 2. Check remaining pool
        pool_indices = np.where(pool_mask)[0]
        if len(pool_indices) == 0:
            break

        # 3. Score pool candidates
        X_pool = X_train_full[pool_indices]
        probs = model.predict_proba(X_pool)
        
        if strategy == "entropy":
            scores = -np.sum(probs * np.log(np.maximum(probs, 1e-12)), axis=1)
        elif strategy == "random":
            scores = rng.uniform(0, 1, size=len(pool_indices))
            
        # 4. Acquire top batch
        top_k_pool_idx = np.argsort(scores)[::-1][:batch_size]
        new_labeled_global = pool_indices[top_k_pool_idx]

        # 5. Move from pool to labeled
        labeled_indices.extend(new_labeled_global)
        pool_mask[new_labeled_global] = False

    return {
        "strategy": strategy,
        "final_labeled_count": len(labeled_indices),
        "accuracy_curve": accuracies
    }
```

---

## 8. Model Persistence & SHA-256 Integrity

```python
import json
import hashlib
import joblib
from pathlib import Path
from typing import Any, Dict

def save_active_learning_state(learner: Any, pool_indices: list, out_path: Path, metadata: Dict[str, Any]) -> Path:
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    
    payload = {
        "model": getattr(learner, "base_estimator", learner),
        "labeled_indices": getattr(learner, "labeled_indices_", []),
        "pool_indices": pool_indices
    }
    joblib.dump(payload, out_path, compress=3)
    
    sha = hashlib.sha256()
    with open(out_path, "rb") as f:
        while chunk := f.read(65536):
            sha.update(chunk)
            
    sidecar = {
        "file": out_path.name,
        "sha256": sha.hexdigest(),
        "labeled_count": len(payload["labeled_indices"]),
        "metadata": metadata
    }
    with open(out_path.with_suffix(".json"), "w", encoding="utf-8") as f:
        json.dump(sidecar, f, indent=2)
        
    return out_path
```

---

## 9. Hyperparameter Tuning & Diagnostics

### Calibrating Batch Size ($B$) vs Iteration Latency
Acquiring samples in batches ($B > 1$) is necessary in production to keep annotators busy. However, large $B$ causes **Information Redundancy**: querying 100 points based on model state $f_{\theta_t}$ means point 100 is selected using stale information before learning from point 1.

```
       Trade-Off: Acquisition Batch Size (B)
       
       Small Batch (B = 1)                   Large Batch (B = 500)
       -------------------                   ---------------------
       [+] Maximum statistical efficiency    [-] Highly redundant queries (batch overlap)
       [-] Human annotator idle latency      [+] High throughput annotator utilization
       [-] Excessive model retraining time   [+] Fast retraining cycles
       
       Standard Production Compromise: B in [10, 50] with Core-Set diversity filtering.
```

---

## 10. Residuals, Diagnostics & Validation

The primary diagnostic metric is the **Area Under the Learning Curve (ALC)**:

$$\text{ALC} = \frac{1}{K} \sum_{k=1}^K \text{Metric}(k)$$

Plot accuracy vs labeled sample count: if Active Learning's curve stays strictly above Random Sampling, the active acquisition strategy is successful.

---

## 11. Uncertainty Estimation & Confidence Intervals

Model calibration is paramount in active learning. If a neural network is uncalibrated (overconfident on out-of-distribution points), its predicted softmax probabilities will not reflect true epistemic uncertainty.
- **Monte Carlo Dropout (Gal & Ghahramani, 2016)**: Leave dropout active at inference time. Compute $T=20$ forward passes per candidate point $\mathbf{x}$. The variance of predictions across passes quantifies epistemic uncertainty.

---

## 12. Failure Modes & Edge Cases

| Failure Mode | Root Cause | Behavioral Symptom | Detection Strategy | Mitigation |
| :--- | :--- | :--- | :--- | :--- |
| **Outlier Trapping** | Pure uncertainty queries isolated noisy outliers (e.g. corrupted sensor data) that sit near 0.5. | Model spends budget labeling garbage samples; zero improvement on test data. | Density check: candidate has low neighbor density in feature space. | Combine uncertainty with density weighting: $\alpha(\mathbf{x}) = \alpha_{\text{unc}}(\mathbf{x}) \cdot \text{Density}(\mathbf{x})^\beta$. |
| **Cluster Starvation / Blind Spots** | Initial seed set failed to sample from an entire class cluster. | Model is 100% confident on unobserved cluster, never querying it. | t-SNE inspection of pool vs query points. | Periodically inject $\epsilon$-greedy random exploratory queries ($10\%$). |
| **Oracle Noise / Fatigue** | Human annotator makes mistakes on difficult ambiguous boundary cases. | Contaminated training set leads to boundary jitter. | Compute inter-annotator agreement ($\kappa < 0.70$). | Submit ambiguous samples to multi-annotator consensus voting. |
| **Batch Query Duplication** | Top-$B$ uncertain candidates are all nearly identical points clustered together. | Budget wasted on redundant information. | Compute pairwise cosine distance between batch candidates ($< 0.1$). | Apply greedy diversity filtering (Core-Set / K-Means on candidate embeddings). |
| **Model Invalidation on Architecture Change** | Active samples gathered using Logistic Regression are reused to train a deep Transformer. | Supervised Transformer suffers from sampling bias. | Train standard random baseline on the collected set. | Use Importance Weighting to debias the active sampling distribution. |

---

## 13. Comparative Trade-offs & When to Use

| Dimension | Least Confidence | Margin Sampling | Shannon Entropy | Core-Set (Diversity) |
| :--- | :--- | :--- | :--- | :--- |
| **Complexity** | $\mathcal{O}(|\mathcal{U}|)$ | $\mathcal{O}(|\mathcal{U}|)$ | $\mathcal{O}(|\mathcal{U}| \cdot C)$ | $\mathcal{O}(B \cdot |\mathcal{U}| \cdot d)$ |
| **Multi-Class Sensitivity** | Weak (only looks at max) | **Excellent** (checks top 2) | **Comprehensive** (all classes) | Ignores class predictions |
| **Outlier Robustness** | Poor | Poor | Poor | **High** (covers space) |
| **Best Used For** | Quick binary classification | Multi-class with confusing pairs | High-class cardinality ($C \ge 10$) | High batch sizes ($B > 100$) |

---

## 14. Production Engineering & Latency SLAs

- **Fast Candidate Filtering**: When $|\mathcal{U}| > 10,000,000$, scoring every sample takes too long. Run uncertainty scoring on a sub-sampled random candidate pool of $100,000$ points per round.
- **Asynchronous Task Queue**: Decouple query acquisition from human labeling via Redis / Celery. When the annotator finishes a batch, an async worker updates the model in the background.

---

## 15. Interpretability & Feature Attribution

To explain why a sample was selected for annotation, present the human annotator with:
1. The predicted probability distribution across classes (e.g. Class A: $48\%$, Class B: $46\%$).
2. The nearest labeled neighbor from Class A and the nearest labeled neighbor from Class B.

---

## 16. Hardware Acceleration, Scaling & Distributed Notes

- **GPU Batch Forward Passes**: Compute softmax probabilities for millions of candidates using PyTorch/TensorRT tensor batches.
- **Distributed Core-Set with FAISS**: Core-set $k$-center greedy searches accelerate $100\times$ by querying FAISS vector indices on GPU.

---

## 17. Pytest Verification Suite

```python
"""
Pytest validation suite for Active Learning acquisition scoring.
Save as: tests/test_active_learning.py
Run: pytest -v tests/test_active_learning.py
"""

import pytest
import numpy as np

def test_entropy_acquisition_scoring():
    """Verify Shannon entropy is maximized for uniform distribution and minimized for deterministic."""
    probs_uniform = np.array([[0.25, 0.25, 0.25, 0.25]])
    probs_deterministic = np.array([[1.0, 0.0, 0.0, 0.0]])
    
    eps = 1e-12
    h_uniform = -np.sum(probs_uniform * np.log(probs_uniform))
    h_det = -np.sum(probs_deterministic * np.log(np.maximum(probs_deterministic, eps)))
    
    assert np.isclose(h_uniform, np.log(4.0), atol=1e-5)
    assert np.isclose(h_det, 0.0, atol=1e-5)

def test_margin_sampling_scoring():
    """Verify margin sampling assigns highest uncertainty to closest top-2 probabilities."""
    # Sample A: margin is 0.02 (very uncertain)
    p_A = np.array([0.49, 0.47, 0.04])
    # Sample B: margin is 0.40 (confident)
    p_B = np.array([0.65, 0.25, 0.10])
    
    margin_A = p_A[0] - p_A[1]
    margin_B = p_B[0] - p_B[1]
    
    assert margin_A < margin_B
```

---

## 18. Cross-Track Integration

- **`03-backend/fastapi/16-ai-and-ml-serving`**: Building an Active Learning annotation API with batch candidate dispatch.
- **`09-sql`**: Querying candidate pools from PostgreSQL using table partitioning on `status = 'unlabeled'`.
- **`04-devops/docker`**: Containerizing worker pools executing continuous retraining cycles.

---

## 19. Real-World Enterprise Case Study

### Active Learning for Rare Financial Transaction Fraud
- **Context**: A global payment processor reviews 40 million transactions daily. True fraud accounts for only $0.03\%$ of transactions.
- **Problem**: Compliance investigators had capacity to inspect only 2,000 flagged alerts daily. Random sampling found almost zero fraud, and standard supervised models had high false alarm rates.
- **Solution Architecture**:
  1. Deployed an Active Learning pipeline using Margin Sampling combined with Isolation Forest density weighting.
  2. Routed the top 2,000 daily maximum-margin transactions to senior fraud analysts.
  3. Re-trained the fraud detection model nightly on the new expert annotations.
- **Outcome**: Fraud detection recall increased from $44\%$ to $89\%$ within 6 weeks using the exact same human investigator headcount.

---

## 20. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Why does pure Uncertainty Sampling fail in the presence of sensory outliers, and how does Information Density resolve it?</strong></summary>

**Answer:**  
Uncertainty sampling prioritizes points near the model's decision boundary: $P(y \mid \mathbf{x}) \approx 0.5$.
1. If a point is a corrupt, unrepresentative sensory outlier (e.g. a dead sensor reporting random Gaussian noise or an illegible blurred image), it naturally sits in empty space between classes where the model has received no data.
2. The model outputs maximum entropy / $0.5$ probability for this outlier.
3. Pure uncertainty sampling will prioritize labeling this outlier. Even after it is labeled, it provides **zero generalization benefit** for normal data points because it sits in an unpopulated region. The active learner gets "trapped" wasting its budget querying garbage points.
**Resolution (Information Density - Settles & Craven, 2008)**:
Weight uncertainty by the candidate's average similarity to all other unlabeled points:
$$\alpha_{\text{Density}}(\mathbf{x}) = \alpha_{\text{uncertainty}}(\mathbf{x}) \cdot \left( \frac{1}{|\mathcal{U}|} \sum_{u \in \mathcal{U}} \text{sim}(\mathbf{x}, \mathbf{x}_u) \right)^\beta$$
This ensures points are only selected if they are both highly uncertain **and** representative of a dense cluster of real data.
</details>

<details>
<summary><strong>Q2: What is Sampling Bias in Active Learning and why does it invalidate standard offline test sets?</strong></summary>

**Answer:**  
In passive supervised learning, data points are drawn i.i.d. from the true population distribution $p(\mathbf{x})$.
Active learning deliberately violates the i.i.d. assumption: it selects points based on current model uncertainty. Consequently:
1. The labeled set $\mathcal{D}_L$ is heavily biased toward complex decision boundaries and low-margin zones, with near-zero coverage of easy, high-density regions.
2. **Offline Evaluation Invalidation**: Evaluating a model on a test set constructed from leftover active learning samples produces a completely distorted accuracy metric (dramatically underestimating real-world performance because easy samples are absent).
3. **Model Invalidation**: If you later replace the model (e.g. switching from Logistic Regression to a ResNet), training the ResNet on the actively acquired set can perform **worse** than random sampling because the data distribution was tailored to the specific blind spots of the old model.
**Fix**: Maintain a pristine, randomly sampled holdout test set that is never touched by active acquisition.
</details>

<details>
<summary><strong>Q3: Explain the Query-by-Committee (QBC) strategy and derive the Kullback-Leibler divergence disagreement metric.</strong></summary>

**Answer:**  
Query-by-Committee (Seung et al., 1992) represents the "version space" of candidate hypotheses by maintaining a committee of $M$ diverse classifiers $\mathcal{C} = \{f^{(1)}, \dots, f^{(M)}\}$.
To measure the disagreement of the committee on sample $\mathbf{x}$, compute the consensus probability distribution:
$$P_C(y \mid \mathbf{x}) = \frac{1}{M} \sum_{m=1}^M P^{(m)}(y \mid \mathbf{x})$$
The informativeness of $\mathbf{x}$ is the average Kullback-Leibler (KL) divergence between each committee member's prediction and the consensus:
$$\alpha_{\text{QBC}}(\mathbf{x}) = \frac{1}{M} \sum_{m=1}^M \mathcal{D}_{\text{KL}}\left( P^{(m)}(y \mid \mathbf{x}) \,\|\, P_C(y \mid \mathbf{x}) \right) = \frac{1}{M} \sum_{m=1}^M \sum_{c=1}^C P^{(m)}(c \mid \mathbf{x}) \ln \frac{P^{(m)}(c \mid \mathbf{x})}{P_C(c \mid \mathbf{x})}$$
This equals the Jensen-Shannon Divergence of the committee. Samples where committee members disagree strongly maximize this score, indicating a region of version space that needs reduction.
</details>

<details>
<summary><strong>Q4: How does the Core-Set approach (Sener & Savarese, 2018) frame active learning as a geometry problem?</strong></summary>

**Answer:**  
Rather than relying on model probabilities (which may be uncalibrated), Core-Set frames active learning as finding a subset $\mathcal{Q} \subset \mathcal{U}$ such that a model trained on $\mathcal{D}_L \cup \mathcal{Q}$ achieves generalization error competitive with a model trained on the entire pool $\mathcal{U}$.
Mathematically, it bounds the difference between empirical loss over the entire pool and empirical loss over the chosen core-set using the Lipschitz continuity of the loss function:
$$|\mathcal{L}(\mathcal{U}) - \mathcal{L}(\mathcal{D}_L \cup \mathcal{Q})| \le L_f \cdot \delta_{\mathcal{Q}}$$
where $\delta_{\mathcal{Q}}$ is the **Covering Radius**:
$$\delta_{\mathcal{Q}} = \max_{u \in \mathcal{U}} \min_{v \in \mathcal{D}_L \cup \mathcal{Q}} \|\mathbf{x}_u - \mathbf{x}_v\|_2$$
Minimizing this covering radius is the classical NP-hard **$k$-Center Problem**. Sener & Savarese solve it using a greedy 2-approximation: iteratively select the unlabeled point that has the maximum distance to its nearest labeled neighbor.
</details>

<details>
<summary><strong>Q5: What is the Cold-Start problem in active learning and how do you initialize the seed labeled set?</strong></summary>

**Answer:**  
In active learning, uncertainty acquisition strategies rely on model class posteriors $P(y \mid \mathbf{x})$.
- If the model is uninitialized ($|\mathcal{D}_L| = 0$), posteriors cannot be computed.
- If initialized with a tiny random sample that misses entire minority classes, the model will be overconfident in those missing regions and will never query them.
**Production Initialization Strategies**:
1. **$K$-Means Clustering on Unsupervised Embeddings**: Cluster the unlabeled pool into $K$ clusters (e.g. using PCA or self-supervised embeddings). Request annotations for the $K$ points closest to the cluster medoids. This guarantees broad geographic representation across the entire feature space before active learning begins.
2. **Zero-Shot Heuristics**: Use weak rules (regular expressions, keywords) to seed initial candidates across all known classes.
</details>

<details>
<summary><strong>Q6: Contrast Expected Error Reduction (EER) with Uncertainty Sampling.</strong></summary>

**Answer:**  
- **Uncertainty Sampling**: A heuristic proxy. It assumes that labeling the most ambiguous point will improve the model, but cannot guarantee that labeling that point will reduce overall error on the rest of the pool.
- **Expected Error Reduction (Roy & McCallum, 2001)**: An optimal decision-theoretic objective. It directly estimates how much the validation loss over the entire pool will drop if candidate $\mathbf{x}^*$ is labeled:
  $$\alpha_{\text{EER}}(\mathbf{x}) = \sum_{c=1}^C P(y=c \mid \mathbf{x}) \left[ \sum_{u \in \mathcal{U}} \text{Loss}\left( f_{\mathcal{D}_L \cup \{(\mathbf{x}, c)\}}(\mathbf{x}_u) \right) \right]$$
  For every candidate point $\mathbf{x}$, it simulates adding $(\mathbf{x}, c)$ to the training set, retrains the model for every hypothetical label $c$, and computes pool loss.
  - *Advantage*: Directly targets global generalization error; immune to outlier traps.
  - *Disadvantage*: Computationally intractable: retraining the model $|\mathcal{U}| \times C$ times per query requires millions of training cycles.
</details>

<details>
<summary><strong>Q7: What is Stream-Based Selective Sampling and when is it preferred over Pool-Based Active Learning?</strong></summary>

**Answer:**  
- **Pool-Based Active Learning**: Assumes a large static collection of unlabeled data $\mathcal{U}$ is accessible in storage. The algorithm evaluates and ranks all candidates simultaneously.
  - *Limitation*: Requires holding or scanning the entire pool; unsuitable for continuous streaming data.
- **Stream-Based Selective Sampling (Atlas et al., Cohn et al.)**: Data arrives sequentially one sample at a time $\mathbf{x}_t \sim p(\mathbf{x})$.
  - For each arriving sample, the model evaluates an acquisition criterion (e.g. margin $\alpha(\mathbf{x}_t)$).
  - A decision rule decides whether to **query** the oracle or **discard** the sample:
    $$\text{If } \alpha(\mathbf{x}_t) \ge \tau \implies \text{Query Oracle, else Discard}$$
  - *Advantage*: Minimal memory footprint; ideal for real-time sensor streams, financial tickers, and high-velocity network packet inspection.
</details>

<details>
<summary><strong>Q8: How do you handle non-expert or noisy oracles in active learning?</strong></summary>

**Answer:**  
Real-world human annotators make errors, especially on ambiguous boundary cases where active learning focuses.
**Strategies**:
1. **Repeated Labeling (Ipeirotis et al.)**: Dispatch the same sample to multiple independent annotators; apply Dawid-Skene Expectation-Maximization to jointly estimate annotator reliability and true latent consensus labels.
2. **Proactive Learning (Donmez & Carbonell, 2008)**: Model oracles with varying expertise and costs (e.g. Expert Doctor @ \$150/hr vs Medical Student @ \$20/hr). The acquisition function optimizes the trade-off: select which data point to query **and** which specific annotator to assign based on required confidence.
</details>

---

## 21. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Computing Uncertainty Acquisition Scores</strong></summary>

**Task**: Given a NumPy array of predicted class probabilities for 5 candidates across 3 classes, compute Least Confidence, Margin, and Entropy scores.

**Solution**:
```python
import numpy as np

probs = np.array([
    [0.90, 0.05, 0.05],  # Very confident
    [0.40, 0.35, 0.25],  # Highly ambiguous
    [0.51, 0.49, 0.00],  # Minimal margin
    [0.34, 0.33, 0.33],  # Maximal entropy
    [0.70, 0.20, 0.10]
])

# 1. Least Confidence: 1 - max(p)
lc_score = 1.0 - np.max(probs, axis=1)

# 2. Margin: sorted top 2 difference
sorted_p = np.sort(probs, axis=1)[:, ::-1]
margin = sorted_p[:, 0] - sorted_p[:, 1]
margin_score = 1.0 - margin # Higher = smaller margin

# 3. Shannon Entropy: - sum p log p
eps = 1e-12
entropy_score = -np.sum(probs * np.log(np.maximum(probs, eps)), axis=1)

for i in range(5):
    print(f"Sample {i}: LC = {lc_score[i]:.3f} | MarginScore = {margin_score[i]:.3f} | Entropy = {entropy_score[i]:.3f}")
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Simulated Oracle Labeling</strong></summary>

**Task**: Write an Oracle simulator function that accepts queried candidate indices, retrieves their true labels from a hidden array, and adds synthetic label noise at a $5\%$ error rate.

**Solution**:
```python
import numpy as np

def simulate_noisy_oracle(queried_indices: list[int], true_labels: np.ndarray, noise_rate: float = 0.05, random_state: int = 42) -> np.ndarray:
    rng = np.random.default_rng(random_state)
    labels = np.copy(true_labels[queried_indices])
    
    # Inject random label flips
    n_queries = len(queried_indices)
    noise_mask = rng.uniform(0, 1, size=n_queries) < noise_rate
    
    # Flip binary labels for contaminated entries
    labels[noise_mask] = 1 - labels[noise_mask]
    return labels

# Test
y_ground = np.array([1, 1, 1, 0, 0, 1, 0, 0, 1, 1])
retrieved = simulate_noisy_oracle([0, 1, 2, 3, 4], y_ground, noise_rate=0.20)
print("Ground truth:", y_ground[:5])
print("Oracle returned:", retrieved)
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Query-by-Committee Vote Entropy Implementation</strong></summary>

**Task**: Implement a 3-model committee (Random Forest, Logistic Regression, GaussianNB) that scores an unlabeled pool using vote entropy.

**Solution**:
```python
import numpy as np
from sklearn.linear_model import LogisticRegression
from sklearn.ensemble import RandomForestClassifier
from sklearn.naive_bayes import GaussianNB

def compute_qbc_vote_entropy(models, X_pool, n_classes=2):
    # Collect predictions from each committee member: shape (M, N)
    preds = np.array([clf.predict(X_pool) for clf in models])
    M, N = preds.shape
    
    # Compute vote entropy per sample
    vote_entropies = np.zeros(N)
    eps = 1e-12
    for i in range(N):
        votes = preds[:, i]
        counts = np.bincount(votes, minlength=n_classes)
        probs = counts / M
        vote_entropies[i] = -np.sum(probs * np.log(np.maximum(probs, eps)))
        
    return vote_entropies

# Test
rng = np.random.default_rng(42)
X_train = rng.normal(0, 1, (20, 2))
y_train = rng.choice([0, 1], size=20)
X_pool = rng.normal(0, 1, (10, 2))

c1 = LogisticRegression().fit(X_train, y_train)
c2 = RandomForestClassifier(random_state=42).fit(X_train, y_train)
c3 = GaussianNB().fit(X_train, y_train)

entropies = compute_qbc_vote_entropy([c1, c2, c3], X_pool)
print("QBC Disagreement Entropies for 10 candidates:", np.round(entropies, 3))
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Information Density Weighting</strong></summary>

**Task**: Combine margin uncertainty scoring with average pairwise cosine similarity to prevent querying isolated outliers.

**Solution**:
```python
import numpy as np
from sklearn.metrics.pairwise import cosine_similarity

def information_density_acquisition(probs: np.ndarray, X_pool: np.ndarray, beta: float = 1.0) -> np.ndarray:
    # 1. Margin uncertainty: 1 - (p1 - p2)
    sorted_p = np.sort(probs, axis=1)[:, ::-1]
    margin = sorted_p[:, 0] - sorted_p[:, 1]
    unc_score = 1.0 - margin
    
    # 2. Density: average similarity to all other pool candidates
    sim_matrix = cosine_similarity(X_pool)
    density_score = np.mean(sim_matrix, axis=1)
    
    # Combine: unc * (density^beta)
    return unc_score * (density_score ** beta)

# Test
probs_dummy = np.array([[0.51, 0.49], [0.52, 0.48]])
# Sample 0 is in dense cluster, Sample 1 is far outlier
X_dummy = np.array([[1.0, 1.0], [100.0, -50.0]])

scores = information_density_acquisition(probs_dummy, X_dummy, beta=1.0)
print(f"Dense ambiguous sample score:   {scores[0]:.4f}")
print(f"Isolated ambiguous sample score:{scores[1]:.4f}")
assert scores[0] > scores[1]
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Greedy Core-Set Selection (k-Center)</strong></summary>

**Task**: Implement the greedy $k$-Center Core-Set acquisition algorithm that iteratively selects $B$ candidate points that minimize the maximum distance to any existing labeled point.

**Solution**:
```python
import numpy as np
from scipy.spatial.distance import cdist

def greedy_k_center_core_set(X_labeled: np.ndarray, X_pool: np.ndarray, batch_size: int = 5) -> list[int]:
    """
    Returns indices within X_pool chosen by k-center greedy covering.
    """
    n_pool = len(X_pool)
    # Compute initial distance from each pool point to its nearest labeled neighbor
    dist_to_labeled = np.min(cdist(X_pool, X_labeled, metric="euclidean"), axis=1)
    
    selected_indices = []
    current_min_dists = np.copy(dist_to_labeled)
    
    for _ in range(batch_size):
        # Pick the pool sample furthest from its nearest neighbor
        furthest_idx = int(np.argmax(current_min_dists))
        selected_indices.append(furthest_idx)
        
        # Update distances to include the newly selected point
        dist_to_new = cdist(X_pool, X_pool[furthest_idx : furthest_idx + 1]).squeeze()
        current_min_dists = np.minimum(current_min_dists, dist_to_new)
        # Prevent re-picking
        current_min_dists[furthest_idx] = -1.0
        
    return selected_indices

# Verification
rng = np.random.default_rng(42)
X_l = rng.normal(0, 1, (5, 2))
X_p = rng.normal(5, 2, (50, 2))

chosen = greedy_k_center_core_set(X_l, X_p, batch_size=4)
print(f"Selected {len(chosen)} diverse core-set indices: {chosen}")
assert len(chosen) == 4
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Area Under the Learning Curve (ALC) Benchmark Harness</strong></summary>

**Task**: Build a complete comparative benchmark running 10 rounds of Active Learning (Entropy) vs Random Sampling over 5 independent random trials, computing the mean Area Under the Learning Curve (ALC).

**Solution**:
```python
import numpy as np
from sklearn.datasets import make_classification
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import accuracy_score

def compute_alc(accuracies: list[float]) -> float:
    # Normalized trapezoidal area under the learning curve
    return float(np.trapezoid(accuracies) / (len(accuracies) - 1))

def benchmark_alc_trial(strategy="entropy", n_rounds=8, batch_size=5, seed=42):
    rng = np.random.default_rng(seed)
    X, y = make_classification(n_samples=400, n_features=6, random_state=seed)
    
    # 50 test samples
    X_test, y_test = X[:50], y[:50]
    X_train_pool, y_train_pool = X[50:], y[50:]
    
    # 6 initial seeds
    labeled_idx = list(range(6))
    pool_idx = list(range(6, len(X_train_pool)))
    
    acc_history = []
    
    for _ in range(n_rounds):
        clf = LogisticRegression().fit(X_train_pool[labeled_idx], y_train_pool[labeled_idx])
        acc_history.append(accuracy_score(y_test, clf.predict(X_test)))
        
        if strategy == "entropy":
            probs = clf.predict_proba(X_train_pool[pool_idx])
            scores = -np.sum(probs * np.log(np.maximum(probs, 1e-12)), axis=1)
        else:
            scores = rng.uniform(0, 1, size=len(pool_idx))
            
        top_idx = np.argsort(scores)[::-1][:batch_size]
        new_picks = [pool_idx[i] for i in top_idx]
        
        labeled_idx.extend(new_picks)
        pool_idx = [idx for idx in pool_idx if idx not in new_picks]
        
    return compute_alc(acc_history)

# Run benchmark
alc_active = np.mean([benchmark_alc_trial("entropy", seed=i) for i in range(5)])
alc_random = np.mean([benchmark_alc_trial("random", seed=i) for i in range(5)])

print(f"Mean ALC Active Learning (Entropy): {alc_active:.4f}")
print(f"Mean ALC Random Passive Sampling:   {alc_random:.4f}")
print(f"Active Learning Gain:              +{(alc_active - alc_random):.4f}")
assert alc_active > alc_random
```
</details>
