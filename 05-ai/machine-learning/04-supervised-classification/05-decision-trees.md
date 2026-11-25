# Decision Tree Classifier (CART)

> **Track:** `05-ai/machine-learning` | **Module:** `04-supervised-classification` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `NumPy: >=1.26.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Classification and Regression Trees (CART) — Classification Variant |
| **Family** | Non-parametric Supervised Learning / Hierarchical Axis-Aligned Space Partitioning |
| **Input Feature Types** | Continuous, discrete, and categorical features ($X \in \mathbb{R}^{n \times d}$). Invariant to monotonic feature scalings. |
| **Target Output** | Discrete class labels $\hat{y} \in \{1, \dots, K\}$ and class empirical probability distributions $P(y = k \mid \mathbf{x})$. |
| **Optimization Target** | Recursive binary splitting greedily maximizing criterion purity gain: $\Delta I(s, \mathcal{D}) = I(\mathcal{D}) - \frac{|\mathcal{D}_L|}{|\mathcal{D}|} I(\mathcal{D}_L) - \frac{|\mathcal{D}_R|}{|\mathcal{D}|} I(\mathcal{D}_R)$. |
| **Loss / Purity Metrics** | Gini Impurity: $I_G(p) = 1 - \sum_{k=1}^K p_k^2$; Shannon Cross-Entropy: $I_H(p) = -\sum_{k=1}^K p_k \log_2 p_k$; Log-Loss. |
| **Optimization Solvers** | Top-down greedy recursive partitioning (heuristic search); exact globally optimal tree search is NP-complete. |
| **Key Hyperparameters** | `criterion`, `max_depth`, `min_samples_split`, `min_samples_leaf`, `min_impurity_decrease`, `ccp_alpha` (Cost-Complexity Pruning). |
| **Inference Mechanism** | Deterministic binary traversal down axis-aligned orthogonal decision rules from root to leaf: $O(\text{depth})$. |
| **Primary Failure Modes** | Extreme high variance and instability (small input perturbations alter top splits); inability to model non-axis-aligned boundaries (diagonal cuts produce staircase artifacts); over-confident leaf probabilities. |

---

## 2. Architecture & Intuition

A decision tree constructs an explicit, hierarchical directed acyclic graph (specifically a binary tree) that recursively partitions the input space $\mathbb{R}^d$ into a set of non-overlapping, axis-aligned hyper-rectangles (boxes).

```
                      [ Root Node: Feature X1 <= 4.5 ]
                               /            \
                             Yes             No
                             /                \
        [ Node A: Feature X2 <= 1.2 ]      [ Class 2 (Leaf) ]
                /             \             P=[0.05, 0.95]
              Yes              No
              /                 \
       [ Class 0 (Leaf) ]  [ Class 1 (Leaf) ]
        P=[0.90, 0.10]      P=[0.15, 0.85]
```

### Geometric Partitioning
Every internal decision node tests an axis-aligned predicate $[x_j \le \theta]$. This partitions the region $\mathcal{R}$ into two half-spaces $\mathcal{R}_L$ and $\mathcal{R}_R$. At each terminal leaf node, the model assigns a constant prediction: the majority class or empirical class probability distribution of the training observations trapped within that leaf's geometric box.

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $\mathcal{D}_m$ | Set of pairs $(\mathbf{x}_i, y_i)$ | Subset of training observations reaching node $m$ |
| $N_m$ | $\mathbb{N}$ | Total number of training samples in node $m$ ($|\mathcal{D}_m|$) |
| $N_{mk}$ | $\mathbb{N}$ | Number of samples of class $k$ in node $m$ |
| $p_{mk}$ | $[0, 1]$ | Proportion of class $k$ samples in node $m$: $p_{mk} = \frac{N_{mk}}{N_m}$ |
| $j$ | $\{1, \dots, d\}$ | Candidate splitting feature dimension |
| $\theta$ | $\mathbb{R}$ | Candidate splitting scalar threshold |
| $I(m)$ | $\mathbb{R}_{\ge 0}$ | Impurity measure of node $m$ (Gini Impurity or Shannon Entropy) |
| $\Delta I(j, \theta, m)$ | $\mathbb{R}_{\ge 0}$ | Impurity reduction (information gain) achieved by split $(j, \theta)$ at node $m$ |
| $T$ | Graph | Tree structure consisting of internal nodes and terminal leaves $\widetilde{T}$ |
| $|T|$ | $\mathbb{N}$ | Number of terminal leaf nodes in tree $T$ |
| $R(T)$ | $\mathbb{R}_{\ge 0}$ | Total misclassification risk or impurity error of tree $T$ |
| $\alpha$ | $\mathbb{R}_{\ge 0}$ | Complexity parameter in Minimal Cost-Complexity Pruning |

### 1. Impurity Measures
Let $p_{mk}$ denote the empirical probability that an observation in node $m$ belongs to class $k \in \{1, \dots, K\}$:

$$p_{mk} = \frac{1}{N_m} \sum_{\mathbf{x}_i \in \mathcal{D}_m} \mathbb{I}(y_i = k)$$

1. **Gini Impurity**:
   Measures the probability of misclassifying a randomly chosen element from the set if it were randomly labeled according to the class distribution:
   $$I_G(m) = \sum_{k=1}^K p_{mk} (1 - p_{mk}) = 1 - \sum_{k=1}^K p_{mk}^2$$
   - Minimum value is $0.0$ when the node is completely pure (all samples belong to a single class).
   - Maximum value for $K$ balanced classes is $1 - \frac{1}{K}$.
2. **Shannon Entropy**:
   Derived from information theory, measuring the average surprise/entropy of the node:
   $$I_H(m) = - \sum_{k=1}^K p_{mk} \log_2 (p_{mk}) \quad \text{with } 0 \log_2(0) \equiv 0$$
   - Minimum value is $0.0$ for pure nodes.
   - Maximum value for $K$ balanced classes is $\log_2(K)$.

### 2. Splitting Objective (The CART Criterion)
At node $m$, the algorithm searches over all possible feature dimensions $j \in \{1, \dots, d\}$ and all unique threshold values $\theta \in \mathbb{R}$ to split $\mathcal{D}_m$ into:

$$\mathcal{D}_L(j, \theta) = \{(\mathbf{x}, y) \in \mathcal{D}_m \mid x_j \le \theta\}$$
$$\mathcal{D}_R(j, \theta) = \{(\mathbf{x}, y) \in \mathcal{D}_m \mid x_j > \theta\}$$

The optimal split $(j^*, \theta^*)$ minimizes the weighted child impurity (or equivalently, maximizes impurity decrease):

$$\arg\min_{j, \theta} \mathcal{G}(j, \theta, m) = \frac{N_L}{N_m} I(\mathcal{D}_L) + \frac{N_R}{N_m} I(\mathcal{D}_R)$$

The **Impurity Decrease** (Information Gain) is:
$$\Delta I(j, \theta, m) = I(\mathcal{D}_m) - \left[ \frac{N_L}{N_m} I(\mathcal{D}_L) + \frac{N_R}{N_m} I(\mathcal{D}_R) \right]$$

### 3. Stopping Conditions
Recursive partitioning terminates at node $m$ if any of the following hold:
1. Node purity: $I(\mathcal{D}_m) = 0$ (all samples belong to one class).
2. Tree depth reaches predefined limit: $\text{depth}(m) \ge \text{max\_depth}$.
3. Insufficient samples: $N_m < \text{min\_samples\_split}$.
4. Child node would violate size constraints: $\min(N_L, N_R) < \text{min\_samples\_leaf}$.
5. Improvement is negligible: $\Delta I(j^*, \theta^*, m) < \text{min\_impurity\_decrease}$.

### 4. Minimal Cost-Complexity Pruning (Breiman's CART)
Fully grown trees overfit training noise. Cost-complexity pruning defines a cost-complexity measure parameterized by $\alpha \ge 0$:

$$R_\alpha(T) = R(T) + \alpha |T|$$

Where:
- $R(T) = \sum_{t \in \widetilde{T}} \frac{N_t}{N} I(t)$ is total leaf impurity/error.
- $|T|$ is the number of terminal leaves in tree $T$.
- $\alpha \ge 0$ is the regularisation penalty per additional leaf.

For each internal node $t$, collapsing its entire subtree $T_t$ into a single leaf increases tree error by:
$$\Delta R(t) = R(t) - R(T_t)$$
The cost-complexity parity point occurs at:
$$g(t) = \frac{R(t) - R(T_t)}{|T_t| - 1}$$
The weakest link algorithm identifies the internal node $t$ minimizing $g(t)$, prunes it, and steps $\alpha$ up, yielding a nested sequence of subtrees $T_0 \supset T_1 \supset T_2 \dots \supset \{root\}$. Optimal $\alpha$ is selected via cross-validation.

---

## 4. Computational Complexity

| Phase | Time Complexity | Space Complexity |
| :--- | :--- | :--- |
| **Training (Building Tree)** | $O(d \cdot n \log n \cdot \text{depth})$ using pre-sorted feature columns | $O(n \cdot d)$ for data storage + $O(|T|)$ for node pointers |
| **Inference (Single Sample)** | $O(\text{depth})$ where $\text{depth} \le \log_2(n)$ for balanced trees | $O(1)$ stack memory during traversal |
| **Inference (Worst Case)** | $O(n)$ if tree degenerates into a single deep linear branch | $O(n)$ recursion depth |
| **Cost-Complexity Pruning** | $O(|T|^2)$ to generate sequence of candidate pruned subtrees | $O(|T|)$ to store pruned subtree representations |

---

## 5. Inductive Bias & Learning Guarantees

1. **Orthogonal Axis-Aligned Splits**: CART assumes the true underlying decision boundaries can be approximated by a combination of hyperplanes that are strictly perpendicular to feature axes. Rotated or linear combinations (e.g., $x_1 + x_2 > 5$) require a deep, stair-stepped staircase of orthogonal splits.
2. **Greedy Myopic Optimality**: At each step, CART chooses the locally optimal split without lookahead. It cannot guarantee finding the globally optimal tree (a known NP-complete problem).
3. **Monotonic Invariance**: Splitting thresholds depend solely on feature ranking, making decision trees completely invariant to strictly monotonic transformations of input features (scaling, normalization, log-transforms).

---

## 6. Scratch NumPy Implementation

```python
"""
Scratch NumPy implementation of Classification and Regression Tree (CART).
Implements recursive greedy binary splitting with Gini Impurity and Shannon Entropy.
"""

from __future__ import annotations
import numpy as np
from typing import Literal, Optional


class Node:
    """Represents a single node within the decision tree graph."""

    def __init__(
        self,
        feature_idx: Optional[int] = None,
        threshold: Optional[float] = None,
        left: Optional[Node] = None,
        right: Optional[Node] = None,
        *,
        value: Optional[int] = None,
        probabilities: Optional[np.ndarray] = None,
    ) -> None:
        self.feature_idx = feature_idx
        self.threshold = threshold
        self.left = left
        self.right = right
        self.value = value
        self.probabilities = probabilities

    @property
    def is_leaf(self) -> bool:
        return self.value is not None


class ScratchDecisionTreeClassifier:
    """Production-grade reference decision tree classifier written purely in NumPy."""

    def __init__(
        self,
        criterion: Literal["gini", "entropy"] = "gini",
        max_depth: Optional[int] = 10,
        min_samples_split: int = 2,
        min_samples_leaf: int = 1,
        min_impurity_decrease: float = 0.0,
    ) -> None:
        self.criterion = criterion
        self.max_depth = max_depth
        self.min_samples_split = min_samples_split
        self.min_samples_leaf = min_samples_leaf
        self.min_impurity_decrease = min_impurity_decrease
        self.root: Optional[Node] = None
        self.n_classes_: int = 0

    def _impurity(self, y: np.ndarray) -> float:
        """Compute node impurity (Gini or Entropy)."""
        if len(y) == 0:
            return 0.0
        counts = np.bincount(y, minlength=self.n_classes_)
        p = counts / len(y)
        p = p[p > 0.0]  # Exclude zero probabilities for numerical stability

        if self.criterion == "gini":
            return float(1.0 - np.sum(p ** 2))
        elif self.criterion == "entropy":
            return float(-np.sum(p * np.log2(p)))
        else:
            raise ValueError(f"Unknown criterion: {self.criterion}")

    def _best_split(self, X: np.ndarray, y: np.ndarray) -> tuple[Optional[int], Optional[float], float]:
        """Find the optimal feature and threshold maximizing impurity decrease."""
        n_samples, n_features = X.shape
        best_gain = 0.0
        best_feat = None
        best_thresh = None

        current_impurity = self._impurity(y)

        for feat_idx in range(n_features):
            col = X[:, feat_idx]
            thresholds = np.unique(col)
            if len(thresholds) <= 1:
                continue

            # Evaluate midpoints between consecutive sorted unique values
            sorted_thresh = np.sort(thresholds)
            midpoints = (sorted_thresh[:-1] + sorted_thresh[1:]) / 2.0

            for thresh in midpoints:
                left_mask = col <= thresh
                right_mask = ~left_mask

                n_l, n_r = np.sum(left_mask), np.sum(right_mask)
                if n_l < self.min_samples_leaf or n_r < self.min_samples_leaf:
                    continue

                imp_l = self._impurity(y[left_mask])
                imp_r = self._impurity(y[right_mask])
                gain = current_impurity - ((n_l / n_samples) * imp_l + (n_r / n_samples) * imp_r)

                if gain > best_gain:
                    best_gain = gain
                    best_feat = feat_idx
                    best_thresh = thresh

        return best_feat, best_thresh, best_gain

    def _build_tree(self, X: np.ndarray, y: np.ndarray, depth: int = 0) -> Node:
        """Recursively construct tree nodes."""
        n_samples, _ = X.shape
        counts = np.bincount(y, minlength=self.n_classes_)
        majority_class = int(np.argmax(counts))
        probs = counts / n_samples

        # Check stopping criteria
        is_pure = (counts[majority_class] == n_samples)
        depth_exceeded = (self.max_depth is not None and depth >= self.max_depth)
        split_too_small = (n_samples < self.min_samples_split)

        if is_pure or depth_exceeded or split_too_small:
            return Node(value=majority_class, probabilities=probs)

        # Greedily search for best split
        feat_idx, thresh, gain = self._best_split(X, y)
        if feat_idx is None or gain <= self.min_impurity_decrease:
            return Node(value=majority_class, probabilities=probs)

        # Split dataset and recurse
        left_mask = X[:, feat_idx] <= thresh
        right_mask = ~left_mask

        left_child = self._build_tree(X[left_mask], y[left_mask], depth + 1)
        right_child = self._build_tree(X[right_mask], y[right_mask], depth + 1)

        return Node(
            feature_idx=feat_idx,
            threshold=thresh,
            left=left_child,
            right=right_child,
        )

    def fit(self, X: np.ndarray, y: np.ndarray) -> ScratchDecisionTreeClassifier:
        """Fit decision tree to training data."""
        X = np.asarray(X, dtype=np.float64)
        y = np.asarray(y, dtype=np.int64)
        self.n_classes_ = int(np.max(y)) + 1
        self.root = self._build_tree(X, y, depth=0)
        return self

    def _traverse(self, x: np.ndarray, node: Node) -> Node:
        """Traverse tree down to leaf for a single instance."""
        if node.is_leaf:
            return node
        if x[node.feature_idx] <= node.threshold:
            return self._traverse(x, node.left)
        return self._traverse(x, node.right)

    def predict(self, X: np.ndarray) -> np.ndarray:
        """Predict majority class labels."""
        X = np.asarray(X, dtype=np.float64)
        return np.array([self._traverse(x, self.root).value for x in X])

    def predict_proba(self, X: np.ndarray) -> np.ndarray:
        """Predict empirical class probability distributions."""
        X = np.asarray(X, dtype=np.float64)
        return np.array([self._traverse(x, self.root).probabilities for x in X])
```

---

## 7. Production Scikit-Learn 9-Stage Pipeline

```python
"""
Production decision tree classification pipeline with Cost-Complexity Pruning.
Implements automated tree path extraction and evaluation metrics.
"""

from __future__ import annotations
import logging
from typing import Tuple, Dict
import numpy as np
from sklearn.datasets import load_breast_cancer
from sklearn.model_selection import train_test_split, StratifiedKFold, GridSearchCV
from sklearn.tree import DecisionTreeClassifier
from sklearn.pipeline import Pipeline
from sklearn.metrics import classification_report, roc_auc_score, brier_score_loss

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("DecisionTree_Pipeline")


def run_decision_tree_production_pipeline() -> Tuple[Pipeline, Dict[str, float]]:
    # Stage 1: Ingestion
    logger.info("Stage 1: Loading diagnostic dataset.")
    data = load_breast_cancer()
    X, y = data.data, data.target

    # Stage 2: Train/Test Split
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.25, stratify=y, random_state=42
    )

    # Stage 3: Minimal Cost-Complexity Pruning Alpha Path Discovery
    logger.info("Stage 3: Computing cost-complexity pruning path.")
    base_clf = DecisionTreeClassifier(random_state=42)
    path = base_clf.cost_complexity_pruning_path(X_train, y_train)
    ccp_alphas = path.ccp_alphas
    # Subsample alphas to avoid excessive grid search size
    selected_alphas = ccp_alphas[::max(1, len(ccp_alphas) // 15)]

    # Stage 4: Pipeline Construction
    pipeline = Pipeline([
        ("dt", DecisionTreeClassifier(
            criterion="gini",
            min_samples_leaf=3,
            random_state=42
        ))
    ])

    # Stage 5: Systematic Hyperparameter Optimization
    param_grid = {
        "dt__ccp_alpha": selected_alphas,
        "dt__max_depth": [3, 5, 8, 12, None],
        "dt__min_samples_split": [2, 5, 10],
    }
    cv = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)
    search = GridSearchCV(
        pipeline,
        param_grid=param_grid,
        cv=cv,
        scoring="roc_auc",
        n_jobs=-1
    )
    search.fit(X_train, y_train)
    best_model = search.best_estimator_
    logger.info(f"Optimal Alpha & Depth: {search.best_params_}")

    # Stage 6: Inference
    y_pred = best_model.predict(X_test)
    y_prob = best_model.predict_proba(X_test)[:, 1]

    # Stage 7: Evaluation
    auc = float(roc_auc_score(y_test, y_prob))
    brier = float(brier_score_loss(y_test, y_prob))
    logger.info(f"Test ROC-AUC: {auc:.4f} | Brier Calibration Loss: {brier:.4f}")
    logger.info("\n" + classification_report(y_test, y_pred))

    metrics = {"roc_auc": auc, "brier_score": brier}
    return best_model, metrics


if __name__ == "__main__":
    model, perf = run_decision_tree_production_pipeline()
    assert perf["roc_auc"] > 0.90, "Pipeline failed quality SLA."
```

---

## 8. Model Persistence & Serialization

Decision trees are stored natively as flat parallel C arrays (`children_left`, `children_right`, `feature`, `threshold`, `value`, `impurity`, `n_node_samples`).

```python
"""
Zero-dependency model persistence and rule extraction for Decision Trees.
Extracts rules to human-readable JSON alongside standard joblib serialization.
"""

import json
from pathlib import Path
import joblib
from sklearn.tree import DecisionTreeClassifier


def export_decision_tree_bundle(model: DecisionTreeClassifier, feature_names: list[str], path: Path) -> Path:
    """Save model binary and JSON representation of all decision rules."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    
    # 1. Binary dump
    joblib.dump(model, path)

    # 2. Extract rules to JSON
    tree_ = model.tree_
    def recurse(node: int) -> dict:
        if tree_.feature[node] != -2:  # Not leaf
            name = feature_names[tree_.feature[node]]
            thresh = float(tree_.threshold[node])
            return {
                "type": "split",
                "feature": name,
                "threshold": thresh,
                "left": recurse(tree_.children_left[node]),
                "right": recurse(tree_.children_right[node]),
            }
        else:
            return {
                "type": "leaf",
                "samples": int(tree_.n_node_samples[node]),
                "distribution": tree_.value[node][0].tolist(),
                "class": int(tree_.value[node][0].argmax()),
            }

    rules_json = recurse(0)
    rules_path = path.with_suffix(".rules.json")
    with open(rules_path, "w", encoding="utf-8") as f:
        json.dump(rules_json, f, indent=2)

    return rules_path
```

---

## 9. Hyperparameter Tuning Guide

| Hyperparameter | Default | Tuning Range | Production Recommendation & Impact |
| :--- | :--- | :--- | :--- |
| `max_depth` | `None` | $[3, 15]$ | Fundamental defense against overfitting. An unconstrained tree (`None`) memorizes noise down to zero leaf impurity. |
| `min_samples_split` | `2` | $[10, 100]$ | Minimum samples required to attempt an internal split. Scale with sample size ($0.5\%$ to $1\%$ of $n$). |
| `min_samples_leaf` | `1` | $[5, 50]$ | Critical smoothing parameter. Setting $\ge 5$ prevents isolated leaf predictions based on single anomalous samples. |
| `ccp_alpha` | `0.0` | $[10^{-5}, 10^{-1}]$ | Optimal regularizer. Use `cost_complexity_pruning_path` to find exact candidate alphas and select via cross-validation. |
| `criterion` | `'gini'` | `['gini', 'entropy']` | Gini is faster (no logarithmic computation). Entropy penalizes mixed leaves slightly more aggressively. Performance rarely differs by $>1\%$. |

---

## 10. Residuals, Diagnostics & Failure Modes

1. **Tree Depth Profile & Branching Imbalance**:
   Inspect `tree_.max_depth`. If the depth is 25 while $n=500$, the tree is memorizing unique samples. Check ratio of leaf count $|T|$ to total samples $n$.
2. **Feature Dominance (Masking Effect)**:
   If one continuous feature with high cardinality is chosen at the root, it may mask other correlated features throughout subtrees.
3. **Staircase Artifacts**:
   When separating classes divided by a diagonal boundary ($x_1 + x_2 > 1$), an axis-aligned tree must execute dozens of splits, wasting model capacity and producing poor generalization near edges.

---

## 11. Prediction Uncertainty & Calibration

Decision tree leaf probability estimates $P(y = k \mid \mathbf{x}) = \frac{N_{mk}}{N_m}$ suffer from severe miscalibration:
1. **Extreme Quantization**: In small leaves (e.g., $N_m = 5$), probabilities can only take values from $\{0.0, 0.2, 0.4, 0.6, 0.8, 1.0\}$.
2. **Empirical Overconfidence**: Leaves often reach $100\%$ purity on training data due to overfitting, yielding uncalibrated $0.0$ or $1.0$ probabilities.
3. **Remedy**: Always wrap a decision tree in `CalibratedClassifierCV(method='sigmoid', cv=5)` or use an ensemble (Random Forest) before relying on probabilities for risk calculations.

---

## 12. Comprehensive Failure Modes & Edge Cases Table

| Failure Mode | Root Cause | Observable Symptom | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **High Variance / Structural Instability** | Hierarchical top-down nature; split error cascades down all subtrees. | Minor perturbation in training data completely alters root split and entire tree structure. | Use bagging ensembles (Random Forest) or prune aggressively via `ccp_alpha`. |
| **Diagonal Boundary Failure** | Axis-aligned splitting constraint cannot represent linear combinations. | Staircase decision boundaries; high leaf count required for simple 45-degree line. | Pre-rotate data using PCA or use a linear model / logistic regression. |
| **Cardinality Bias** | Features with many distinct values have more candidate split thresholds. | Continuous or high-cardinality IDs chosen preferentially over strong binary features. | Use `min_samples_leaf` constraints, bin continuous features, or use permutation importance. |
| **Leaf Probability Overconfidence** | Pure leaves produce hard 0.0 / 1.0 probabilities. | Extremely high Brier calibration loss; catastrophic failure when computing expected loss. | Apply Laplace smoothing: $\hat{p}_k = \frac{N_k + 1}{N + K}$ or apply Platt scaling. |
| **Extreme Imbalance Degeneracy** | Minority class contains fewer samples than `min_samples_split`. | Tree completely ignores minority class; all leaves predict majority label. | Set `class_weight='balanced'` to weight impurity gain by inverse class frequencies. |
| **Extrapolation Failure** | Step-function nature beyond bounding box of training features. | Unseen values outside training range receive constant leaf prediction of nearest edge. | Constrain input feature ranges with strict validation gates at ingestion. |

---

## 13. Comparative Trade-off Table

| Dimension | Decision Tree (Single CART) | Logistic Regression | Random Forest | Support Vector Machine (RBF) |
| :--- | :--- | :--- | :--- | :--- |
| **Interpretability** | Maximum (Direct visual flowchart) | High (Odds ratios) | Low (Averaged forest) | Very Low (Dual kernel combination) |
| **Inference Speed** | $O(\text{depth})$ (< 10 microseconds) | $O(d)$ (< 1 microsecond) | $O(M \cdot \text{depth})$ (Moderate) | $O(n_{\text{SV}} \cdot d)$ (Slow) |
| **Scaling Invariance** | Strictly invariant to monotonic scaling | Highly sensitive (requires scaling) | Strictly invariant | Highly sensitive |
| **Handling Outliers** | Highly robust in feature space | Moderate | Highly robust | Sensitive |
| **Generalization Error** | High (High variance / prone to overfit) | Low variance / High bias | Very Low (Variance reduction via bagging) | Low |

---

## 14. Production Engineering, Serving & Latency

### Zero-Dependency C / SQL Code Generation
Because a decision tree is an explicit set of `if-else` rules, it can be compiled directly into raw C code or pure SQL `CASE WHEN` statements, eliminating Python runtime dependencies:

```sql
-- Generated SQL scoring query from exported CART rules
SELECT 
    transaction_id,
    CASE 
        WHEN feature_amount <= 45.2 THEN 
            CASE 
                WHEN feature_velocity <= 2.0 THEN 0.02
                ELSE 0.18
            END
        ELSE 
            CASE 
                WHEN feature_foreign_risk <= 0.8 THEN 0.12
                ELSE 0.89
            END
    END AS fraud_probability
FROM transactions;
```

---

## 15. Interpretability & Explainability

1. **Direct Graph Visualization**: Use `sklearn.tree.export_graphviz` or `dtreeviz` to render the tree structure with node sample counts, purity, and split criteria.
2. **Mean Decrease in Impurity (MDI) Feature Importance**:
   $$I_{\text{MDI}}(j) = \frac{\sum_{t \in T: v(t)=j} \frac{N_t}{N} \Delta I(t)}{\sum_{t \in T} \frac{N_t}{N} \Delta I(t)}$$
   > [!WARNING]
   > MDI is known to be biased toward high-cardinality features. Always cross-validate feature importance against **Permutation Feature Importance**!

---

## 16. Hardware, Parallelism & Scaling

- **Pre-Sorting vs Histogram Binning**: Scikit-Learn's `DecisionTreeClassifier` sorts each feature column in $O(n \log n)$ time. For $n > 10^6$, single tree construction becomes memory-bandwidth bound.
- **Single-Core Nature**: Construction of a single decision tree is inherently sequential (each node requires the parent's data split). Parallelism is achieved at the ensemble level (e.g., across trees in Random Forest) rather than within a single CART tree.

---

## 17. Automated Verification Harness

```python
"""
Pytest verification harness for Decision Tree models.
Verifies recursive split bounds, invariant feature scaling, and leaf purity.
"""

import numpy as np
import pytest
from sklearn.datasets import make_classification
from sklearn.tree import DecisionTreeClassifier


@pytest.fixture
def dataset() -> tuple[np.ndarray, np.ndarray]:
    return make_classification(n_samples=200, n_features=6, n_informative=4, random_state=42)


def test_monotonic_scale_invariance(dataset):
    """Decision tree splits must be invariant to monotonic non-linear feature transforms."""
    X, y = dataset
    clf_original = DecisionTreeClassifier(max_depth=4, random_state=42)
    clf_original.fit(X, y)
    preds_orig = clf_original.predict(X)

    # Apply strictly monotonic transform: exp(x) and log(x + offset)
    X_transformed = X.copy()
    X_transformed[:, 0] = np.exp(X[:, 0])
    X_transformed[:, 1] = 10.0 * X[:, 1] + 100.0

    clf_trans = DecisionTreeClassifier(max_depth=4, random_state=42)
    clf_trans.fit(X_transformed, y)
    preds_trans = clf_trans.predict(X_transformed)

    np.testing.assert_array_equal(
        preds_orig, preds_trans,
        err_msg="Decision tree predictions changed under monotonic feature transformation!"
    )


def test_leaf_purity_at_unbounded_depth():
    """An unconstrained decision tree without duplicate features must achieve 100% training accuracy."""
    X, y = make_classification(n_samples=100, n_features=10, n_classes=2, flip_y=0.0, random_state=42)
    clf = DecisionTreeClassifier(max_depth=None, min_samples_split=2, min_samples_leaf=1)
    clf.fit(X, y)

    train_acc = float(clf.score(X, y))
    assert train_acc == 1.0, f"Unconstrained tree failed to achieve zero training error: {train_acc}"
```

---

## 18. Cross-Track Learning & Ecosystem Links

- **Python Optimization**: See `01-languages/python/21-data-and-scientific-basics` for recursive structure handling.
- **Ensemble Upgrades**: Direct predecessor to `04-supervised-classification/06-random-forest` and `07-gradient-boosting`.
- **Interpretability**: See `13-interpretability-and-fairness` for SHAP and surrogate decision tree models.
- **Inference Systems**: Compiles to ultra-fast SQL pipelines in `09-sql`.

---

## 19. Real-World Enterprise Case Study

### Context: Healthcare Emergency Room Triage Protocol
- **Domain**: Hospital Emergency Department Clinical Decision Support.
- **Problem**: Classifying arriving patients into Immediate Resuscitation vs Stable Triage ($K=2$) based on vitals (heart rate, blood pressure, oxygen saturation, GCS score).
- **Hard Constraint**: System must be 100% legally explainable; clinicians must be able to verify every single decision rule on a laminated flowchart within 5 seconds.
- **Solution**:
  1. Fit `DecisionTreeClassifier(max_depth=4, min_samples_leaf=20, ccp_alpha=0.008)`.
  2. The resulting tree contains exactly 7 decision nodes and 8 terminal leaves.
  3. Exported directly to clinical guidelines and verified by an expert panel of medical triage officers.
- **Outcome**: Achieved $96.4\%$ sensitivity for critical resuscitation cases while maintaining zero black-box opacity.

---

## 20. Technical Interview Preparation

### Q1: Why is finding the globally optimal decision tree NP-complete?
**Answer:** Finding a minimal decision tree consistent with training data requires searching over the combinatorial space of all possible hierarchical feature-threshold permutations across all subsets of data. The number of possible binary trees grows super-exponentially with both samples $n$ and features $d$. Therefore, all practical implementations (CART, ID3, C4.5) rely on greedy top-down heuristic search, optimizing impurity reduction locally at each node without backtracking.

### Q2: Compare Gini Impurity vs Shannon Entropy mathematically and practically.
**Answer:**
- Gini: $I_G = 1 - \sum p_k^2$; Entropy: $I_H = - \sum p_k \log_2(p_k)$.
- For binary classification with probability $p$: Gini is $2p(1-p)$ and peaks at $0.5$; Entropy peaks at $1.0$. Scaled by $\frac{1}{2}$, entropy closely mirrors Gini.
- Computationally, Gini is significantly faster as it avoids costly logarithmic operations. In practice, they select the identical split over $98\%$ of the time.

### Q3: How does Cost-Complexity Pruning (CCP) prevent overfitting?
**Answer:** CCP minimizes $R_\alpha(T) = R(T) + \alpha |T|$, where $R(T)$ is total tree error and $|T|$ is number of terminal leaves. By progressively increasing $\alpha$, the algorithm identifies the subtree whose collapse into a single leaf yields the smallest increase in error per eliminated leaf: $g(t) = \frac{R(t) - R(T_t)}{|T_t| - 1}$. Cross-validation then selects the optimal $\alpha$, eliminating noisy branches while retaining primary structure.

### Q4: Why are decision trees immune to monotonic feature transformations?
**Answer:** CART split evaluations depend solely on the relative ordering of feature values, searching for split boundaries between sorted unique thresholds. Any strictly monotonic transformation $f(x)$ preserves order ($x_a < x_b \iff f(x_a) < f(x_b)$). Consequently, the rank-ordered partition of samples on either side of the threshold remains completely identical.

### Q5: What is the "masking effect" in decision tree induction?
**Answer:** If two features $X_1$ and $X_2$ are strongly correlated, and $X_1$ is chosen at the root node because it provides a fractionally higher impurity gain, $X_2$ may never be selected anywhere in the subtree. This masks the predictive utility of $X_2$, resulting in zero MDI importance for an otherwise highly predictive feature.

### Q6: How does CART handle missing values during training and inference?
**Answer:** While Scikit-Learn natively requires imputation or routes NaNs to the child with greatest sample count (in modern versions), Breiman's original CART uses **Surrogate Splits**. When evaluating a split on $X_j^*$, the algorithm identifies alternative features that produce the most similar binary split of the data. During inference, if $X_j^*$ is missing for a sample, the primary surrogate feature is evaluated instead.

### Q7: Why are decision trees considered high-variance estimators?
**Answer:** Because tree construction is hierarchical and greedy, an error or shift in the root node split cascades to every subsequent partition in the subtree. A slight perturbation in training data can cause a different feature or threshold to be chosen at the top, generating a radically different tree topology with completely different leaf boundaries.

### Q8: How can a decision tree be used to detect non-linear feature interactions?
**Answer:** In a decision tree, any split occurring at depth $k \ge 1$ is explicitly conditioned on the sequence of antecedent splits from the root. If feature $X_2$ is split within the subtree governed by $X_1 \le \theta$, the model is modeling a non-linear interaction effect $f(X_1, X_2) = \mathbb{I}(X_1 \le \theta) \cdot \mathbb{I}(X_2 \le \phi)$, which linear models cannot capture without explicit interaction terms.

---

## 21. Hands-On Exercises & Mini-Projects

1. **Surrogate Split Engine**: Enhance `ScratchDecisionTreeClassifier` to compute and store a surrogate feature-threshold split for each node to handle missing feature inputs at test time without imputation.
2. **Interactive Decision Boundary Plotter**: Train decision trees with depths $d \in \{1, 2, 3, 5, 10\}$ on a 2D synthetic dataset. Plot the orthogonal partitioning boxes over the scatter plot to visualize how depth controls spatial resolution.
3. **MDI vs Permutation Importance Showdown**: Create a dataset with 1 continuous random noise feature, 1 high-cardinality categorical noise feature, and 2 genuinely informative binary features. Fit an unpruned CART tree and show how MDI falsely crowns the noise feature as most important, whereas permutation importance correctly identifies the informative features.
4. **SQL Decision Tree Compiler**: Write a Python function `tree_to_sql(model, feature_names)` that traverses an arbitrary Scikit-Learn `DecisionTreeClassifier` and compiles it into an executable, nested SQL `CASE WHEN` statement.
5. **Cost-Complexity Pruning Curve Analysis**: Compute and plot the training and cross-validation accuracy curves as a function of `ccp_alpha`. Identify the exact sweet spot that balances tree complexity against test generalization.
6. **Oblique Decision Tree Prototype**: Write a prototype classifier that tests linear combinations $(\mathbf{w}^T \mathbf{x} \le \theta)$ at each node using a small linear SVM or logistic regression instead of single axis-aligned features, eliminating staircase artifacts on rotated data.
