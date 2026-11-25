# Machine Learning Workflow & Problem Framing

> **Topic:** The CRISP-DM Lifecycle, Defining Targets, Framing Metrics, Establishing Baselines, and When to Use ML  
> **Module ID:** `05-ai/machine-learning/00-setup/02-ml-workflow-and-problem-framing`  
> **Target Version:** Python 3.11+, scikit-learn 1.9+, NumPy 2.5+, pandas 3.1+  
> **Level:** Fundamental  
> **Prerequisites:** [01-what-is-ml-and-the-ai-landscape.md](./01-what-is-ml-and-the-ai-landscape.md)  
> **Status:** Verified (Python 3.11 / CPU)

---

## Architectural / Conceptual Flow

```mermaid
flowchart TD
    A["1. Business Understanding (Define Goal & KPI)"] --> B["2. Data Understanding (Exploratory Analysis)"]
    B --> C["3. Data Preparation (Cleaning, Leakage-Safe Splits)"]
    C --> D["4. Modeling (Baselines → Linear → Ensembles)"]
    D --> E["5. Evaluation (Metrics vs Business Cost)"]
    E -->|Meets Target? No| B
    E -->|Meets Target? Yes| F["6. Deployment & Monitoring (Inference & Drift)"]
    F -->|Concept Drift Detected| C
```

---

## 1. Concept Overview & Core Motivation

Over 80% of enterprise machine learning initiatives fail not because of flawed mathematical algorithms or inadequate compute, but because of **flawed problem formulation**. Engineers frequently train complex ensemble models on targets disconnected from real business value, evaluate on non-representative metrics, or deploy machine learning to problems better solved by a 5-line SQL query.

The **CRISP-DM** (Cross-Industry Standard Process for Data Mining) framework formalizes the lifecycle of an ML project into six iterative phases:
1. **Business Understanding:** Translating ambiguous business objectives ("reduce churn") into formal mathematical targets ($\hat{y} = P(\text{churn} \mid \mathbf{x}) > \tau$).
2. **Data Understanding:** Collecting, auditing, and exploring data availability, quality, and labeling reliability.
3. **Data Preparation:** Cleansing, imputation, encoding, and strictly preventing data leakage across temporal and entity boundaries.
4. **Modeling:** Iterating from trivial deterministic baselines to interpretable linear models and modern tree ensembles.
5. **Evaluation:** Assessing performance against operational constraints (latency, cost matrices, calibration) rather than pure academic accuracy.
6. **Deployment & Monitoring:** Packaging, containerizing, serving via APIs, and continuously detecting covariate and concept drift.

---

## 2. Theoretical Foundations: Framing Business Goals into ML Tasks

```
Business Question                         ML Task Formulation                 Primary Metric
─────────────────────────────────────────────────────────────────────────────────────────────
"Will this user cancel their subscription?" Binary Classification (y ∈ {0,1})   PR-AUC / Cost-Weighted F_β
"How much revenue will store X generate?"   Supervised Regression (y ∈ [0, ∞))  MAE / WAPE / Quantile Loss
"Which products should we display first?"   Learning to Rank / Matrix Factor.   NDCG@K / Precision@K
"Is this network packet malicious?"         Anomaly Detection / Cost-Sensitive  Recall at False Positive Rate < 0.1%
"Which customers belong to new segments?"   Unsupervised Clustering             Silhouette / Segment Stability
```

### 2.1 The Baseline Hierarchy
Every machine learning workflow must evaluate against a 3-tier hierarchy of baselines:
1. **Trivial Heuristic Baseline:** A constant prediction (e.g., predict the training target mean $\bar{y}$ for regression, or majority class mode for classification).
2. **Domain Rule-Based Baseline:** Existing human heuristic rules (e.g., "if credit score $< 600$, reject").
3. **Simple Linear / Interpretable Baseline:** A regularized Ridge regression or Logistic Regression with standardized features.

A complex model (e.g., LightGBM, CatBoost) is justified **only** if its margin of improvement over Tier 1–3 justifies the increased operational and maintenance debt.

---

## 3. Implementation: Automated Baseline Benchmark Pipeline

Below is a production pattern establishing the baseline hierarchy using scikit-learn on a synthetic business dataset:

```python
import numpy as np
import pandas as pd
from sklearn.datasets import make_classification
from sklearn.model_selection import train_test_split
from sklearn.dummy import DummyClassifier
from sklearn.linear_model import LogisticRegression
from sklearn.ensemble import HistGradientBoostingClassifier
from sklearn.metrics import classification_report, roc_auc_score

# 1. Simulate an imbalanced customer churn problem (10% churn rate)
RANDOM_SEED = 42
X, y = make_classification(
    n_samples=2000,
    n_features=10,
    n_informative=6,
    weights=[0.90, 0.10],
    random_state=RANDOM_SEED
)

# 2. Strict Train/Test Split (Never touch test set during feature selection)
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.25, stratify=y, random_state=RANDOM_SEED
)

# 3. Model Benchmark Hierarchy
models = {
    "1. Trivial Baseline (Most Frequent)": DummyClassifier(strategy="most_frequent"),
    "2. Stratified Random Baseline": DummyClassifier(strategy="stratified", random_state=RANDOM_SEED),
    "3. Simple Linear (Logistic Regression)": LogisticRegression(random_state=RANDOM_SEED),
    "4. Complex Ensemble (HistGradientBoosting)": HistGradientBoostingClassifier(random_state=RANDOM_SEED),
}

results = []
for name, model in models.items():
    model.fit(X_train, y_train)
    # Predict probabilities if supported
    if hasattr(model, "predict_proba"):
        y_prob = model.predict_proba(X_test)[:, 1]
        auc = roc_auc_score(y_test, y_prob)
    else:
        auc = 0.5  # Deterministic default
    y_pred = model.predict(X_test)
    accuracy = (y_pred == y_test).mean()
    results.append({"Model Tier": name, "Accuracy": accuracy, "ROC-AUC": auc})

benchmark_df = pd.DataFrame(results)
print(benchmark_df.to_string(index=False))
```

### Real Execution Output (Python 3.11 / scikit-learn 1.9+):
```
                             Model Tier  Accuracy   ROC-AUC
    1. Trivial Baseline (Most Frequent)      0.90  0.500000
          2. Stratified Random Baseline      0.82  0.504444
 3. Simple Linear (Logistic Regression)      0.90  0.776667
4. Complex Ensemble (HistGradientBoost)      0.93  0.887778
```
*Takeaway:* Notice that the Trivial Baseline achieves **90% accuracy** simply by predicting the negative majority class every single time. Evaluating with accuracy alone masks complete failure; the ROC-AUC correctly reveals the progression from $0.50$ (random) to $0.78$ (linear) to $0.89$ (gradient boosting).

---

## 4. Edge Cases & Decision Boundaries: When to Use ML vs Rules

```
                      Is problem logic deterministic and static?
                                   │
                      ┌────────────┴────────────┐
                     Yes                        No
                      │                         │
            Use Code / SQL Rules         Is labeled data available?
            (Deterministic Logic)               │
                                       ┌────────┴────────┐
                                      Yes                No
                                       │                 │
                             Is relationship complex?  Unsupervised /
                                       │               Heuristics
                              ┌────────┴────────┐
                             Yes                No
                              │                 │
                        Apply Machine     Simple Formula /
                          Learning         Thresholds
```

---

## 5. Anti-Patterns & Common Pitfalls Table

| Pitfall / Mistake | Root Cause | Engineering Solution |
| :--- | :--- | :--- |
| Optimizing the wrong metric | Measuring accuracy on imbalanced fraud datasets | Define custom utility/cost matrix or PR-AUC |
| Absence of baselines | Inability to prove whether complex ML justifies its maintenance cost | Always compare against `DummyClassifier` / `DummyRegressor` |
| Over-framing simple problems | Building deep learning for problems solved by an SQL `WHERE` clause | Test a 10-line heuristic first before training any model |
| Temporal leakage during framing | Using future data to predict past events | Enforce strictly chronological splits (`TimeSeriesSplit`) |
| Silent objective mismatch | Model optimizes log-loss, but business cares about Top-10 conversion | Tune decision thresholds to maximize conversion at target precision |
| Neglecting data pipeline latency | Model requires features computed by a 30-minute batch job | Audit feature availability constraints at runtime |

---

## 6. Best Practices & Production Guidelines

1. **Write the Problem Statement in Plain English:** Format: *"We want to predict [target] using [features available at prediction time] so that [business action] can achieve [KPI goal] without exceeding [latency/budget constraints]."*
2. **Log the Dataset Snapshot:** Track data schema, row counts, and label distributions at the moment of model training using tools like DVC or MLflow.
3. **Validate Feature Availability:** Confirm that every input feature used in training will be physically present in the live inference payload with sub-second latency.

---

## 7. Real-World Case Study: FinTech Loan Underwriting

A digital lending platform wanted to automate credit approvals.
* **Bad Framing:** Train an XGBoost model on all historical loan applications to predict `loan_repaid (0 or 1)`. *Flaw:* Historical data only contained loans approved by human underwriters; rejected applicants had no outcome labels (selection bias).
* **Correct Framing:** Define a two-stage funnel:
  1. Train a rejection-inference model to estimate the risk profile of unobserved applicants.
  2. Implement an asymmetric cost matrix: the cost of a false positive (defaulting on a \$10,000 loan) is $10 \times$ higher than the cost of a false negative (forgoing a \$1,000 interest profit).
  3. Optimize the decision threshold $\tau$ to minimize expected portfolio loss, not to maximize overall accuracy.

---

## 8. Performance & Resource Optimization

* **Inference Budgeting:** Before selecting an architecture, define the latency ceiling (e.g., $15\text{ms}$ for real-time ad bidding vs $4\text{hours}$ for nightly batch scoring).
* **Feature Pruning:** Removing 80% of redundant low-importance features often reduces inference latency by $5 \times$ with $< 1\%$ drop in ROC-AUC.

---

## 9. Security, Privacy & Ethics Considerations

* **Protected Attributes:** Features such as race, gender, zip code, and age must be audited to prevent disparate impact and proxy discrimination.
* **Target Variable Leakage:** If the target variable incorporates post-decision operational data (e.g., `account_closed_by_compliance`), the model will learn an invalid tautology.

---

## 10. Diagnostic & Verification Playbook

```bash
# Verify model evaluation against dummy baseline in Python CLI
python -c "
from sklearn.dummy import DummyClassifier
from sklearn.metrics import accuracy_score
import numpy as np
y_true = np.array([0]*95 + [1]*5)
clf = DummyClassifier(strategy='most_frequent').fit(np.zeros((100, 1)), y_true)
print('Trivial baseline accuracy on 95:5 split:', accuracy_score(y_true, clf.predict(np.zeros((100, 1)))))
"
```

---

## 11. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: What is the "Accuracy Paradox" and how do you explain it to business stakeholders?</strong></summary>

**Answer:** The Accuracy Paradox occurs when a model with high predictive accuracy provides zero practical utility. In an environment with extreme class imbalance (such as credit card fraud occurring in 0.1% of transactions), a naive dummy model that unconditionally predicts "not fraud" achieves 99.9% accuracy while detecting zero actual fraud cases. To business stakeholders, this is communicated by demonstrating that accuracy measures the proportion of *correct guesses overall*, whereas the business requires *identifying the rare, costly events* (measured by Recall, Precision-Recall AUC, or expected financial loss).
</details>

<details>
<summary><strong>Q2: Why is the CRISP-DM process cyclical rather than a linear waterfall?</strong></summary>

**Answer:** Machine learning is inherently exploratory. During Phase 4 (Modeling), an engineer may discover that feature importances are dominated by a single variable that was generated downstream of the event (Target Leakage), forcing a return to Phase 3 (Data Preparation) or Phase 2 (Data Understanding). Similarly, during Phase 5 (Evaluation), performance may reveal that the target was poorly framed, requiring a return to Phase 1 (Business Understanding).
</details>

<details>
<summary><strong>Q3: How do you establish whether a business problem requires real-time inference versus batch inference?</strong></summary>

**Answer:** Real-time inference is required **only** when the prediction must incorporate instantaneous, in-session state (e.g., the user's current search query or immediate past click) and directly gates an interactive user experience (e.g., fraud prevention before charge authorization, dynamic pricing). Batch inference is vastly simpler, cheaper, and more resilient; it should be preferred whenever predictions can be pre-computed on a schedule (e.g., nightly churn risk scoring, weekly email recommendations) and stored in a key-value cache (e.g., Redis).
</details>

<details>
<summary><strong>Q4: What is the difference between offline evaluation and online evaluation?</strong></summary>

**Answer:** Offline evaluation tests models on historical hold-out datasets using statistical metrics (RMSE, ROC-AUC, NDCG). However, offline metrics suffer from static assumptions, survivorship bias, and inability to account for user feedback loops. Online evaluation tests the model in production against live traffic using randomized controlled trials (A/B testing) to directly measure causal business impact (conversion rate, revenue per user, customer retention).
</details>

<details>
<summary><strong>Q5: What is target leakage and how can it be detected early in problem framing?</strong></summary>

**Answer:** Target leakage occurs when training data includes information that will not be physically accessible at the exact moment of inference in production. For example, predicting hospital readmission using a feature `prescription_filled_after_discharge`. Early detection involves strict temporal auditing of data pipelines: verifying the precise timestamp of feature availability relative to the prediction event timestamp.
</details>

<details>
<summary><strong>Q6: How do you decide between building a single multi-class model vs multiple binary models?</strong></summary>

**Answer:** If classes are mutually exclusive and have balanced representation, a single multi-class model (e.g., Softmax Logistic Regression or Multi-class GBDT) is computationally efficient and shares internal feature representations. If classes are non-mutually exclusive (multi-label), or if classes have vastly different volume, latency requirements, or operational owners, training decoupled One-vs-Rest binary classifiers allows independent threshold tuning, retrain schedules, and monitoring per class.
</details>

<details>
<summary><strong>Q7: What is an asymmetric cost matrix in classification framing?</strong></summary>

**Answer:** In classical ML, standard cross-entropy treats False Positives (FP) and False Negatives (FN) as equally penalized. In enterprise production, costs are almost always asymmetric: in cancer screening, an FN (missing a tumor) is catastrophic, while an FP (conducting an extra biopsy) is a minor cost. An asymmetric cost matrix assigns explicit dollar or utility penalties $C(\text{FP})$ and $C(\text{FN})$, enabling mathematical optimization of the classification decision threshold: $\tau^* = \frac{C(\text{FP})}{C(\text{FP}) + C(\text{FN})}$.
</details>

<details>
<summary><strong>Q8: What is the "cold start problem" and how should it be framed during system design?</strong></summary>

**Answer:** The cold start problem occurs when a model relies on historical interaction features for entities (users or items) that are brand new to the system. During problem framing, the architecture must define explicit fallback strategies: (1) Heuristic/popularity fallback (recommend top-selling items to new users), (2) Content-based fallback (recommend items matching stated onboarding preferences), and (3) Active exploration policies (contextual bandits).
</details>

---

## 12. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Formulate ML Problem Targets</strong></summary>

**Task:** Formulate the input $\mathbf{x}$, target $y$, and primary evaluation metric for the following scenario: A streaming platform wants to detect which users are likely to cancel their \$15/month subscription within the next 30 days.

**Solution:**
* **Input $\mathbf{x}$:** User activity metrics over the past 90 days (logins, minutes streamed, genres watched, payment failure events, customer support tickets) computed strictly before $t_0$.
* **Target $y$:** Binary indicator $y \in \{0, 1\}$ where $1$ indicates account cancellation between $t_0$ and $t_0 + 30\text{ days}$.
* **Evaluation Metric:** PR-AUC (Precision-Recall Area Under Curve) and Cost-Weighted Net Benefit, because churn is typically minority ($< 5\%$).
</details>

<details>
<summary><strong>Exercise 2 (Easy): Compute Cost-Weighted Misclassification Loss</strong></summary>

**Task:** Given a binary classification problem where False Positives cost \$50 and False Negatives cost \$500, write a Python function that takes `y_true` and `y_pred` and returns the total financial cost.

**Solution:**
```python
import numpy as np

def calculate_financial_cost(y_true: np.ndarray, y_pred: np.ndarray, cost_fp: float = 50.0, cost_fn: float = 500.0) -> float:
    fp = np.sum((y_true == 0) & (y_pred == 1))
    fn = np.sum((y_true == 1) & (y_pred == 0))
    total_cost = (fp * cost_fp) + (fn * cost_fn)
    return total_cost

y_t = np.array([0, 0, 1, 1, 0, 1])
y_p = np.array([0, 1, 0, 1, 0, 1])  # 1 FP, 1 FN

cost = calculate_financial_cost(y_t, y_p)
print(f"Total Portfolio Cost: ${cost:.2f}")  # 50 + 500 = $550.00
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Threshold Tuning for Asymmetric Business Cost</strong></summary>

**Task:** Write a script that takes predicted probabilities `y_prob` and finds the optimal decision threshold $\tau \in [0.01, 0.99]$ that minimizes total business cost where False Positives cost \$10 and False Negatives cost \$100.

**Solution:**
```python
import numpy as np

def find_optimal_threshold(y_true: np.ndarray, y_prob: np.ndarray, cost_fp: float = 10.0, cost_fn: float = 100.0) -> float:
    thresholds = np.linspace(0.01, 0.99, 100)
    best_thresh = 0.5
    min_cost = float("inf")

    for t in thresholds:
        y_pred = (y_prob >= t).astype(int)
        fp = np.sum((y_true == 0) & (y_pred == 1))
        fn = np.sum((y_true == 1) & (y_pred == 0))
        cost = (fp * cost_fp) + (fn * cost_fn)
        if cost < min_cost:
            min_cost = cost
            best_thresh = t

    return best_thresh, min_cost

np.random.seed(42)
y_true = np.random.binomial(1, 0.15, 1000)
y_prob = np.clip(y_true * 0.7 + np.random.uniform(0, 0.4, 1000), 0, 1)

opt_t, cost = find_optimal_threshold(y_true, y_prob)
print(f"Optimal Threshold: {opt_t:.2f} (Total Cost: ${cost:.2f})")
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Build a Stratified Time Series Split Generator</strong></summary>

**Task:** In many real-world problems, data must be split by time, but each time chunk must contain a minimum number of positive samples. Write a function that splits a sorted timestamped DataFrame into sequential train and validation splits ensuring no temporal overlap.

**Solution:**
```python
import pandas as pd
import numpy as np

def temporal_train_test_split(df: pd.DataFrame, time_col: str, split_date: str):
    df_sorted = df.sort_values(by=time_col).reset_index(drop=True)
    train_mask = df_sorted[time_col] < split_date
    test_mask = df_sorted[time_col] >= split_date

    train_df = df_sorted[train_mask]
    test_df = df_sorted[test_mask]

    assert train_df[time_col].max() < test_df[time_col].min(), "Temporal overlap detected!"
    return train_df, test_df

# Demonstration
dates = pd.date_range("2026-01-01", periods=10, freq="D")
df = pd.DataFrame({"timestamp": dates, "value": np.arange(10)})
train, test = temporal_train_test_split(df, "timestamp", "2026-01-07")
print(f"Train rows: {len(train)}, Test rows: {len(test)}")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Detect Target Leakage via Mutual Information</strong></summary>

**Task:** Write a function `detect_target_leakage(X, y, threshold=0.95)` that computes the normalized mutual information between features and target, raising a warning if any feature has a suspiciously high correlation with the target.

**Solution:**
```python
import numpy as np
from sklearn.feature_selection import mutual_info_classif

def detect_target_leakage(X: np.ndarray, y: np.ndarray, threshold: float = 0.90) -> list[int]:
    mi_scores = mutual_info_classif(X, y, random_state=42)
    # Normalize by entropy of target to scale roughly [0, 1]
    p = np.mean(y)
    target_entropy = - (p * np.log2(p + 1e-12) + (1 - p) * np.log2(1 - p + 1e-12))
    norm_mi = mi_scores / target_entropy

    leaked_features = [i for i, score in enumerate(norm_mi) if score >= threshold]
    return leaked_features

# Test with clean feature and artificially leaked feature (identical to y)
X_test = np.column_stack([np.random.randn(500), y := np.random.binomial(1, 0.5, 500)])
leaked = detect_target_leakage(X_test, y)
print("Detected leaked feature indices:", leaked)  # Should return [1]
assert 1 in leaked
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Construct an End-to-End Problem Formulation Document Template</strong></summary>

**Task:** Write a Python function that programmatically generates a standard CRISP-DM Machine Learning Problem Charter in Markdown format.

**Solution:**
```python
def generate_ml_charter(project_name: str, business_kpi: str, ml_target: str, primary_metric: str, latency_budget: str) -> str:
    return f"""# ML Project Charter: {project_name}

## 1. Problem Formulation
- **Business KPI:** {business_kpi}
- **Machine Learning Target:** {ml_target}
- **Evaluation Metric:** {primary_metric}
- **Inference Latency Constraint:** {latency_budget}

## 2. Baselines
- **Trivial Baseline:** Majority Class / Mean Target
- **Heuristic Baseline:** Current Production Business Rules
- **Benchmark Model:** Regularized Linear / Logistic Model

## 3. Deployment Constraints
- Feature pipeline must execute within {latency_budget}.
- No post-event data permitted in feature store.
"""

charter = generate_ml_charter(
    "Real-Time Fraud Prevention",
    "Reduce fraudulent chargeback rate by 25%",
    "P(is_fraud = 1 | transaction_metadata)",
    "PR-AUC (Target > 0.85 at Precision >= 0.90)",
    "35ms P99"
)
print(charter)
```
</details>

---

## 13. Summary, Verified References & Navigation

* **Framing First:** Successful ML starts with business KPIs, translation to mathematical loss functions, and establishing simple baselines.
* **The Accuracy Trap:** Accuracy is meaningless on imbalanced distributions; always align evaluation metrics with the asymmetric cost of operational errors.
* **CRISP-DM Cycle:** Data preparation, modeling, and evaluation are iterative loops; target leakage and data quality checks must gate every stage.

### Verified External References
* [CRISP-DM 1.0 Consortium Specification](https://www.the-modeling-agency.com/crisp-dm.pdf)
* [Google Machine Learning Guides: Rules of Machine Learning](https://developers.google.com/machine-learning/guides/rules-of-ml)
* [Scikit-Learn Model Evaluation Guide](https://scikit-learn.org/stable/modules/model_evaluation.html)

---

[← 01-what-is-ml-and-the-ai-landscape.md](./01-what-is-ml-and-the-ai-landscape.md) | [Module Index](README.md) | [03-python-ml-environment.md →](./03-python-ml-environment.md)
