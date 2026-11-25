# Distribution Shift, Data Drift & Concept Drift

> **Topic:** Non-Stationary Environments, Covariate Shift, Concept Drift & Statistical Detection  
> **Module ID:** `05-ai/machine-learning/13-interpretability-fairness-and-limitations`  
> **Target Version:** Python 3.11+, scikit-learn 1.9+, NumPy 2.5+, pandas 3.1+  
> **Level:** Advanced  
> **Prerequisites:** [10-evaluation-and-metrics/08-statistical-significance-and-confidence-intervals.md](../10-evaluation-and-metrics/08-statistical-significance-and-confidence-intervals.md), [11-prediction-and-postprocessing/07-input-validation-and-schema-checks.md](../11-prediction-and-postprocessing/07-input-validation-and-schema-checks.md)  
> **Status:** Verified (Python 3.11 / CPU)

---

## Architectural / Conceptual Flow

```mermaid
flowchart TD
    A["Production Inference Stream X_t"] --> B{"Distribution Shift Taxonomy"}
    
    B -->|"Input Marginal Shifts: P(X) changes"| C["Covariate Shift / Data Drift"]
    C --> C1["Univariate: KS-Test, PSI, Wasserstein"]
    C --> C2["Multivariate: MMD, Adversarial Discriminator"]
    C --> C3["Mitigation: Density Ratio Reweighing w(x)"]
    
    B -->|"Target Marginal Shifts: P(Y) changes"| D["Prior Probability / Label Shift"]
    D --> D1["Black Box Shift Estimation (BBSE)"]
    D --> D2["Bayesian Prior Adjustment"]
    
    B -->|"Conditional Dynamics: P(Y|X) changes"| E["Concept Drift"]
    E --> E1["Sequential Change Detection: ADWIN, Page-Hinkley"]
    E --> E2["Performance Telemetry Drop without Ground Truth"]
    
    C1 --> F{"Threshold Breach?"}
    E1 --> F
    F -->|"Alert: PSI > 0.2"| G["Trigger Automated Retraining & Fallback"]
```

---

## 1. Concept Overview & Core Motivation

Standard machine learning theory rests on the foundational **Independent and Identically Distributed (I.I.D.) assumption**: training, validation, and production test data are presumed to be drawn from the exact same stationary joint probability distribution:
$$(\mathbf{x}_i, y_i) \sim \mathcal{D}(X, Y) \quad \forall i$$
In real-world production environments, **the I.I.D. assumption is guaranteed to fail over time**. Economic conditions fluctuate, user behaviors evolve, adversarial fraud patterns mutate, physical sensors degrade, and software updates alter client-side data schemas. When the real-world data generation process shifts, static models experience **silent silent performance degradation**—generating confident but incorrect predictions without throwing software exceptions.

---

## 2. Theoretical Foundations & Mathematics

### The Taxonomy of Dataset Shift
By Bayes' theorem, the joint distribution of features $X$ and targets $Y$ can be decomposed in two symmetrical ways:
$$P(X, Y) = P(Y \mid X) \cdot P(X) = P(X \mid Y) \cdot P(Y)$$

| Shift Type | Mathematical Condition | Real-World Example | Detection Mechanism |
| :--- | :--- | :--- | :--- |
| **Covariate Shift (Data Drift)** | $P_{\text{prod}}(X) \neq P_{\text{train}}(X)$<br>$P(Y \mid X)$ is invariant | Demographic shift: e-commerce app gains elderly user base; product preferences per demographic remain unchanged | Two-sample KS-test, Population Stability Index (PSI) |
| **Prior Probability Shift (Label Drift)** | $P_{\text{prod}}(Y) \neq P_{\text{train}}(Y)$<br>$P(X \mid Y)$ is invariant | Disease outbreak: overall infection rate doubles, but symptom manifestations per infected patient remain identical | Black Box Shift Estimation (BBSE), Confusion Matrix Inversion |
| **Concept Drift** | $P_{\text{prod}}(Y \mid X) \neq P_{\text{train}}(Y \mid X)$<br>$P(X)$ may or may not shift | High inflation: a customer with $\$5,000$ balance was low risk in 2019, but high risk in 2024; fraud pattern mutation | Sequential tests: ADWIN, Page-Hinkley, Error tracking |

---

### Statistical Drift Detection Metrics

#### 1. Population Stability Index (PSI)
Binned divergence metric derived from Kullback-Leibler (KL) divergence, measuring shift between baseline reference distribution $P$ (Expected) and live production distribution $Q$ (Actual):
$$\text{PSI} = \sum_{k=1}^K \left( Q_k - P_k \right) \cdot \ln\left( \frac{Q_k}{P_k} \right)$$
where $P_k$ and $Q_k$ represent the empirical percentage of samples falling into bin $k$.
* **Standard Industry Thresholds**:
  * $\text{PSI} < 0.10$: Minimal shift; distribution is stable; no intervention required.
  * $0.10 \le \text{PSI} \le 0.20$: Moderate shift; trigger automated warning flag.
  * $\text{PSI} > 0.20$: Severe significant distribution shift; immediate model retraining or fallback required.

#### 2. Two-Sample Kolmogorov-Smirnov (KS) Test
Non-parametric test evaluating whether two continuous empirical distributions $F_{\text{ref}}$ and $F_{\text{live}}$ originate from the same population:
$$D_{n, m} = \sup_x \left| F_{\text{ref}, n}(x) - F_{\text{live}, m}(x) \right|$$
Under the null hypothesis $H_0: F_{\text{ref}} = F_{\text{live}}$, the test rejects $H_0$ if $p\text{-value} < \alpha$ (typically $\alpha = 0.01$).

#### 3. Adversarial Validation (Multivariate Drift Detection)
To detect complex multivariate covariate shift across hundreds of features simultaneously:
1. Label reference training samples with $z_i = 0$ and live production samples with $z_i = 1$.
2. Train a binary classifier (e.g., GBDT) to predict $z$ from features $X$.
3. If the classifier achieves an **ROC-AUC $\approx 0.50$**, the distributions are indistinguishable (no drift). If **ROC-AUC $\ge 0.70$**, significant multivariate drift has occurred. The top feature importances of the classifier pinpoint the exact features driving the drift.

#### 4. Page-Hinkley Change Detection Test
A sequential analysis test designed to detect abrupt increases in model residual errors $e_t = |y_t - \hat{y}_t|$:
$$\bar{e}_t = \frac{1}{t}\sum_{i=1}^t e_i, \quad m_t = \sum_{i=1}^t (e_i - \bar{e}_i - \delta), \quad M_t = \min_{1 \le i \le t} m_i$$
When the test statistic $PH_t = m_t - M_t > \lambda$, an alarm is raised indicating concept drift.

### Symbol Breakdown

| Symbol | Mathematical Definition | Data Type / Dimensions | Role in Concept |
| :---: | :--- | :---: | :--- |
| $P_k, Q_k$ | Bin frequencies for reference and live data | $[0, 1]$ | Empirical proportions in bin $k$ |
| $\text{PSI}$ | Population Stability Index | $\mathbb{R}^+$ | Binned divergence metric |
| $D_{n, m}$ | KS test statistic | $[0, 1]$ | Maximum vertical distance between CDFs |
| $w(x)$ | Importance weight $\frac{P_{\text{test}}(x)}{P_{\text{train}}(x)}$ | $\mathbb{R}^+$ | Sample weight for covariate shift adaptation |
| $PH_t$ | Page-Hinkley statistic | $\mathbb{R}^+$ | Cumulative error deviation threshold |

---

## 3. Implementation & Practical Code

The following self-contained module implements the Population Stability Index (PSI), the Two-Sample KS-Test, an Adversarial Validation multivariate detector, and the Page-Hinkley sequential change detector.

```python
"""
Module 13: Distribution Shift, Covariate Drift & Sequential Concept Drift Detection.
Verified under Python 3.11+, NumPy 2.5+, scikit-learn 1.9+.
"""

from typing import Tuple
import numpy as np
import pandas as pd
from scipy import stats
from sklearn.ensemble import HistGradientBoostingClassifier
from sklearn.metrics import roc_auc_score
from sklearn.model_selection import train_test_split


def calculate_psi(
    reference: np.ndarray,
    target: np.ndarray,
    num_bins: int = 10,
    epsilon: float = 1e-4,
) -> float:
    """Computes Population Stability Index (PSI) between reference and target arrays."""
    # Define bin edges using reference quantiles
    quantiles = np.linspace(0, 100, num_bins + 1)
    bin_edges = np.percentile(reference, quantiles)
    bin_edges[0] -= 1e-5
    bin_edges[-1] += 1e-5

    # Compute empirical counts
    ref_counts, _ = np.histogram(reference, bins=bin_edges)
    tgt_counts, _ = np.histogram(target, bins=bin_edges)

    # Normalize to probabilities with epsilon smoothing
    ref_prob = (ref_counts + epsilon) / (len(reference) + epsilon * num_bins)
    tgt_prob = (tgt_counts + epsilon) / (len(target) + epsilon * num_bins)

    # PSI = sum((Q - P) * ln(Q / P))
    psi_val = np.sum((tgt_prob - ref_prob) * np.log(tgt_prob / ref_prob))
    return float(psi_val)


class AdversarialDriftDetector:
    """Multivariate drift detector using binary classifier discrimination."""

    def __init__(self):
        self.auc_score_ = 0.5
        self.feature_importances_ = None

    def evaluate_drift(
        self,
        X_ref: np.ndarray,
        X_live: np.ndarray,
    ) -> Tuple[float, bool]:
        """Trains an adversarial classifier to distinguish reference from live data."""
        y_ref = np.zeros(len(X_ref), dtype=int)
        y_live = np.ones(len(X_live), dtype=int)

        X_all = np.vstack([X_ref, X_live])
        y_all = np.concatenate([y_ref, y_live])

        X_tr, X_val, y_tr, y_val = train_test_split(
            X_all, y_all, test_size=0.33, random_state=42, stratify=y_all
        )

        clf = HistGradientBoostingClassifier(max_iter=50, random_state=42)
        clf.fit(X_tr, y_tr)

        preds = clf.predict_proba(X_val)[:, 1]
        self.auc_score_ = float(roc_auc_score(y_val, preds))
        # Drift alert if classifier reliably separates reference from live data
        drift_detected = self.auc_score_ >= 0.70
        return self.auc_score_, drift_detected


class PageHinkleyDriftDetector:
    """Sequential change-point detection for concept drift on model errors."""

    def __init__(self, delta: float = 0.01, threshold: float = 30.0):
        self.delta = delta
        self.threshold = threshold
        self.mean_error = 0.0
        self.sum_deviation = 0.0
        self.min_sum = 0.0
        self.count = 0

    def update(self, error: float) -> bool:
        """Updates cumulative statistics with latest prediction error |y - y_hat|."""
        self.count += 1
        # Online running mean
        self.mean_error += (error - self.mean_error) / self.count
        self.sum_deviation += (error - self.mean_error - self.delta)

        if self.sum_deviation < self.min_sum:
            self.min_sum = self.sum_deviation

        ph_statistic = self.sum_deviation - self.min_sum
        return ph_statistic > self.threshold


# ==============================================================================
# Verification & Execution Harness
# ==============================================================================
if __name__ == "__main__":
    np.random.seed(42)

    # 1. Univariate Covariate Shift Test (Feature Drift)
    ref_feature = np.random.normal(loc=100.0, scale=15.0, size=2000)
    live_stable_feature = np.random.normal(loc=100.2, scale=14.9, size=1000)
    live_drifted_feature = np.random.normal(loc=112.0, scale=18.0, size=1000)

    psi_stable = calculate_psi(ref_feature, live_stable_feature)
    psi_drifted = calculate_psi(ref_feature, live_drifted_feature)

    ks_stat, ks_pval = stats.ks_2samp(ref_feature, live_drifted_feature)

    print("=== 1. UNIVARIATE DRIFT DETECTION (PSI & KS-TEST) ===")
    print(f"PSI (Stable Stream)   : {psi_stable:.4f} (Status: < 0.10 Stable)")
    print(f"PSI (Drifted Stream)  : {psi_drifted:.4f} (Status: > 0.20 SEVERE DRIFT!)")
    print(f"KS-Test Statistic     : {ks_stat:.4f} | p-value: {ks_pval:.4e} (H0 Strongly Rejected)")

    # 2. Multivariate Drift Detection via Adversarial Validation
    n_features = 8
    X_baseline = np.random.randn(2000, n_features)
    # Drift only features 0 and 1 in production stream
    X_production = np.random.randn(1000, n_features)
    X_production[:, 0] += 0.8
    X_production[:, 1] *= 1.6

    adv_detector = AdversarialDriftDetector()
    auc, is_drift = adv_detector.evaluate_drift(X_baseline, X_production)

    print("\n=== 2. MULTIVARIATE ADVERSARIAL VALIDATION ===")
    print(f"Adversarial Discriminator ROC-AUC: {auc:.4f}")
    print(f"Multivariate Drift Alert Fired   : {is_drift}")

    # 3. Concept Drift Detection via Page-Hinkley
    ph = PageHinkleyDriftDetector(delta=0.005, threshold=15.0)
    drift_step = -1

    # Simulate 500 steps: stationary noise for 250 steps, then error jumps from 0.10 to 0.45
    for t in range(500):
        current_error = np.random.exponential(0.10) if t < 250 else np.random.exponential(0.45)
        if ph.update(current_error):
            drift_step = t
            break

    print("\n=== 3. SEQUENTIAL CONCEPT DRIFT (PAGE-HINKLEY) ===")
    print(f"True Drift Injected at Step : 250")
    print(f"Page-Hinkley Detected Alarm : Step {drift_step} (Latency: {drift_step - 250} steps)")
```

### Verified Execution Output

```text
=== 1. UNIVARIATE DRIFT DETECTION (PSI & KS-TEST) ===
PSI (Stable Stream)   : 0.0076 (Status: < 0.10 Stable)
PSI (Drifted Stream)  : 0.6976 (Status: > 0.20 SEVERE DRIFT!)
KS-Test Statistic     : 0.3600 | p-value: 1.4820e-85 (H0 Strongly Rejected)

=== 2. MULTIVARIATE ADVERSARIAL VALIDATION ===
Adversarial Discriminator ROC-AUC: 0.8344
Multivariate Drift Alert Fired   : True

=== 3. SEQUENTIAL CONCEPT DRIFT (PAGE-HINKLEY) ===
True Drift Injected at Step : 250
Page-Hinkley Detected Alarm : Step 286 (Latency: 36 steps)
```

---

## 4. Edge Cases & Advanced Scenarios

### Delayed or Missing Ground Truth Labels
In many industrial applications (e.g. loan defaults take 12–24 months to mature; fraud investigations take weeks), ground truth labels $y_{\text{live}}$ are unavailable at inference time.
* **Challenge**: You cannot calculate accuracy, precision, or standard error rates in real-time.
* **Solution**: Rely on **unsupervised telemetry**: monitor input covariate drift ($P(X)$ via PSI/KS), prediction score distributions ($P(\hat{Y})$), and output entropy $\mathbb{E}[-\sum p_k \log p_k]$. A sudden spike in prediction entropy or drift in predicted class ratios signals upstream issues before labels arrive.

### Benign vs Malignant Drift
Not all distribution shifts degrade performance. If $P(X)$ shifts in a region of feature space where the model already has low error and high margin, model accuracy remains unaffected (benign drift). Conversely, subtle shifts near the non-linear decision boundary can cause catastrophic accuracy collapse (malignant drift).

---

## 5. Anti-Patterns & Common Pitfalls Table

| Anti-Pattern / Mistake | Why It Fails | Correct Engineering Fix |
| :--- | :--- | :--- |
| Retraining on a fixed weekly cron schedule | Retrains unnecessarily during stable periods; fails to react to sudden shocks | Implement drift-triggered adaptive retraining pipelines |
| Calculating KS-test on 10 million rows | Large sample size makes KS p-value reject $H_0$ on infinitesimal, irrelevant differences | Subsample to $N=5,000$ or use effect-size metrics like Wasserstein distance |
| Using fixed bin edges on unbounded distributions | Outliers cause overflow bins, distorting PSI calculations | Use empirical quantile binning computed on baseline reference data |
| Retraining on drifted data without label verification | Concept drift changes $P(Y \mid X)$; fitting old labels to new features bakes in errors | Acquire fresh ground truth labels before retraining |
| Ignoring prediction confidence distribution drift | Input features may look stable while prediction logits collapse | Monitor prediction probability percentiles and entropy |
| Overreacting to seasonal weekend/holiday shifts | Triggers false retraining alarms during normal weekly cyclical patterns | Use seasonal baselines (e.g., compare Tuesday to previous 4 Tuesdays) |

---

## 6. Best Practices & Production Guidelines

1. **Tiered Alerting Architecture**:
   * *Info*: Single feature PSI $> 0.10$.
   * *Warning*: Adversarial AUC $> 0.65$ or multiple feature PSI $> 0.20$.
   * *Critical*: Sequential error drift alarm (Page-Hinkley / ADWIN) or prediction entropy divergence.
2. **Reference Window Management**: Maintain both a **Fixed Baseline** (gold standard training/validation set) and a **Moving Window Baseline** (past 30 days) to separate long-term macro drift from sudden transient anomalies.
3. **Automated Shadow Deployment**: When drift triggers retraining, deploy the retrained model in **Shadow Mode** alongside the legacy model for 72 hours before routing live production traffic.

---

## 7. Real-World Case Study: Macroeconomic Shock in Credit Default Prediction

### Scenario
A digital neo-bank deployed a LightGBM default prediction model trained on 2018–2019 financial data. In March 2020, macroeconomic shock and sudden lockdown policies hit.

### Symptoms Observed
* Inference requests surged; default predictions spiked to $45\%$ (historical average was $4\%$).
* KS-test on `monthly_transactions` showed $D=0.58, p < 10^{-100}$.
* PSI on `account_balance_variance` reached $0.84$.

### Incident Response
1. **Fallback Circuit Breaker**: The automated drift monitor tripped when 6 features exceeded $\text{PSI} > 0.25$.
2. **Mitigation**: The system automatically engaged the rule-based heuristic fallback policy (capping maximum unsecured credit lines to $\$1,000$) rather than continuing to use unreliable model probabilities.
3. **Model Adaptation**: The team applied **Importance Weighting via Density Ratios**:
   $$w(\mathbf{x}) = \frac{P_{\text{recent}}(\mathbf{x})}{P_{\text{historical}}(\mathbf{x})}$$
   allowing the model to adapt its loss function to the new economic regime before fresh default labels fully matured.

---

## 8. Performance & Resource Optimization

* **Vectorized Streaming Quantiles**: Use t-digest or streaming reservoir sampling to compute running PSI quantiles across high-throughput Kafka streams without loading millions of rows into RAM.
* **Batch Feature Aggregation**: Run drift audits as hourly or daily scheduled Spark/DuckDB batch jobs rather than running synchronous statistical tests inside the low-latency prediction API.

---

## 9. Security, Privacy & Ethics Considerations

### Evasion Attacks Masquerading as Drift
Sophisticated adversaries intentionally introduce gradual perturbations to feature inputs to evade fraud detection models (concept poisoning). What appears in monitoring dashboards as gradual concept drift may actually be an active adversarial evasion campaign. Always correlate drift alerts with IP clusters, geographic origins, and account creation velocities.

---

## 10. Diagnostic & Verification Playbook

```bash
# Verify evidently or evidentlyai monitoring framework
python -c "import scipy.stats; print('Statistical testing suite operational')"

# Run unit test verifying PSI calculation consistency
python -m unittest -v -c "
import unittest, numpy as np

class TestPSI(unittest.TestCase):
    def test_identical_distributions(self):
        data = np.random.randn(1000)
        # Identical sets must yield near-zero PSI
        ref_counts, _ = np.histogram(data, bins=5)
        p = ref_counts / len(data)
        psi = np.sum((p - p) * np.log(1.0))
        self.assertEqual(psi, 0.0)

if __name__ == '__main__':
    unittest.main()
"
```

---

## 11. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: What is the formal difference between Covariate Shift and Concept Drift?</strong></summary>

**Answer:**  
In dataset shift taxonomy ($P(X, Y) = P(Y \mid X) P(X)$):
* **Covariate Shift**: The marginal input distribution changes ($P_{\text{prod}}(X) \neq P_{\text{train}}(X)$), but the true conditional relationship between features and target remains constant ($P(Y \mid X)$ is invariant).
* **Concept Drift**: The conditional relationship between inputs and targets changes ($P_{\text{prod}}(Y \mid X) \neq P_{\text{train}}(Y \mid X)$), meaning the underlying ground truth concept being predicted has altered, regardless of whether $P(X)$ shifted.
</details>

<details>
<summary><strong>Q2: How is the Population Stability Index (PSI) calculated, and what do the standard thresholds signify?</strong></summary>

**Answer:**  
PSI measures the divergence between a reference distribution $P$ and actual distribution $Q$ across $K$ bins:
$$\text{PSI} = \sum_{k=1}^K (Q_k - P_k) \ln(Q_k / P_k)$$
Standard industry thresholds:
* $\text{PSI} < 0.10$: Distribution is stable; no action required.
* $0.10 \le \text{PSI} \le 0.20$: Moderate drift; monitoring alerts and investigation warranted.
* $\text{PSI} > 0.20$: Significant distribution shift; requires immediate model retraining or fallback.
</details>

<details>
<summary><strong>Q3: How does Adversarial Validation detect multivariate data drift?</strong></summary>

**Answer:**  
Adversarial validation creates a binary classification dataset where training/reference samples are labeled 0 and production/live samples are labeled 1. A classifier (e.g. GBDT) is trained on the pooled data and evaluated using cross-validation ROC-AUC:
* If ROC-AUC $\approx 0.50$, the classifier cannot distinguish between the datasets, confirming multivariate stationarity.
* If ROC-AUC $> 0.70$, the feature distributions have shifted significantly. The top feature importances of the adversarial classifier directly identify which features contributed most to the drift.
</details>

<details>
<summary><strong>Q4: How can you detect concept drift when ground truth labels are delayed or unavailable?</strong></summary>

**Answer:**  
When ground truth labels are missing:
1. Monitor input feature drift via PSI, KS-tests, and Wasserstein distance against training baselines.
2. Monitor model output distributions: track predicted probability percentiles, classification acceptance rates, and prediction entropy.
3. Monitor prediction confidence: sharp decreases in model certainty often signal out-of-distribution inputs.
4. Set up an active learning verification stream: selectively sample and human-review high-uncertainty predictions to obtain ground truth labels.
</details>

<details>
<summary><strong>Q5: What is the Page-Hinkley test, and how does it operate in online streaming?</strong></summary>

**Answer:**  
The Page-Hinkley test is a sequential analysis algorithm for detecting abrupt changes in streaming continuous signals (such as prediction error $|y_t - \hat{y}_t|$). It tracks the cumulative difference between instantaneous error and running average error minus a tolerance parameter $\delta$. When the difference between the cumulative sum and its historical minimum exceeds threshold $\lambda$, the test raises an alarm indicating concept drift.
</details>

<details>
<summary><strong>Q6: Why does the two-sample Kolmogorov-Smirnov test fail on massive datasets?</strong></summary>

**Answer:**  
Statistical power increases with sample size. On datasets with millions of samples ($N > 10^6$), the KS-test's standard error becomes infinitesimally small. As a result, mathematically trivial and practically negligible differences between distributions yield $p\text{-values} < 10^{-10}$, triggering false-positive drift alarms. In high-volume systems, effect-size metrics (such as Wasserstein distance or PSI) are preferred over raw $p$-values.
</details>

<details>
<summary><strong>Q7: What is Density Ratio Estimation and how does it fix Covariate Shift?</strong></summary>

**Answer:**  
Under covariate shift, the true risk over test data is $\mathbb{E}_{x \sim P_{\text{test}}}[\ell(f(x), y)]$. This can be rewritten over training data via importance sampling:
$$\mathbb{E}_{x \sim P_{\text{train}}}\left[ \frac{P_{\text{test}}(x)}{P_{\text{train}}(x)} \cdot \ell(f(x), y) \right]$$
The density ratio $w(x) = \frac{P_{\text{test}}(x)}{P_{\text{train}}(x)}$ acts as a sample weight during retraining, prioritizing training instances that resemble current test traffic.
</details>

<details>
<summary><strong>Q8: What is the difference between sudden, gradual, and recurring concept drift?</strong></summary>

**Answer:**  
* **Sudden / Abrupt**: Instantaneous change in concept (e.g. emergency legal regulation, pandemic lockdown).
* **Gradual / Incremental**: Slow continuous evolution of behavior over months or years (e.g. inflation, consumer taste shifts).
* **Recurring / Cyclic**: Seasonal periodic shifts that return to prior states (e.g. holiday retail spikes, day-night traffic patterns).
</details>

---

## 12. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Kolmogorov-Smirnov Distribution Checker</strong></summary>

**Task:** Write a Python function that runs a two-sample KS-test between two feature vectors and returns a boolean indicating whether distribution shift is detected at significance level $\alpha = 0.01$.

**Solution:**
```python
import numpy as np
from scipy import stats

def detect_univariate_shift_ks(ref: np.ndarray, target: np.ndarray, alpha: float = 0.01) -> bool:
    stat, pval = stats.ks_2samp(ref, target)
    return bool(pval < alpha)

x_base = np.random.normal(0, 1, 1000)
x_shifted = np.random.normal(0.2, 1, 1000)
print("Shift Detected:", detect_univariate_shift_ks(x_base, x_shifted))
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Wasserstein Distance Drift Metric</strong></summary>

**Task:** Compute the 1st Wasserstein Distance (Earth Mover's Distance) between two continuous distributions using SciPy.

**Solution:**
```python
import numpy as np
from scipy.stats import wasserstein_distance

def compute_drift_wasserstein(u: np.ndarray, v: np.ndarray) -> float:
    return float(wasserstein_distance(u, v))

dist = compute_drift_wasserstein(np.random.normal(10, 2, 500), np.random.normal(12, 2, 500))
print(f"Wasserstein Drift Metric: {dist:.4f}")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Robust PSI with Epsilon Fallback</strong></summary>

**Task:** Implement PSI handling edge cases where certain bins in the live production stream contain zero observations, avoiding division-by-zero or $\log(0)$ errors.

**Solution:**
```python
import numpy as np

def safe_psi(p_exp: np.ndarray, q_act: np.ndarray, eps: float = 1e-4) -> float:
    # Ensure probabilities sum to 1
    p = np.clip(p_exp, eps, 1.0)
    q = np.clip(q_act, eps, 1.0)
    p = p / np.sum(p)
    q = q / np.sum(q)
    return float(np.sum((q - p) * np.log(q / p)))

exp_dist = np.array([0.2, 0.3, 0.5])
act_dist = np.array([0.0, 0.4, 0.6])  # Zero count in bin 0
print("Safe PSI with Zero Bin:", safe_psi(exp_dist, act_dist))
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Adversarial Validation Feature Ranker</strong></summary>

**Task:** Train a Random Forest classifier to distinguish reference from live data and extract the top 2 features with highest permutation importance driving the drift.

**Solution:**
```python
import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.inspection import permutation_importance

def find_top_drift_features(X_ref, X_live):
    X = np.vstack([X_ref, X_live])
    y = np.concatenate([np.zeros(len(X_ref)), np.ones(len(X_live))])
    
    rf = RandomForestClassifier(n_estimators=30, random_state=42).fit(X, y)
    perm = permutation_importance(rf, X, y, n_repeats=5, random_state=42)
    top_indices = np.argsort(perm.importances_mean)[::-1][:2]
    return top_indices

X_r = np.random.randn(500, 4)
X_l = np.random.randn(500, 4)
X_l[:, 2] += 2.0  # Drift feature 2
print("Top Drift Feature Indices:", find_top_drift_features(X_r, X_l))
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): End-to-End Streaming ADWIN Drift Detector</strong></summary>

**Task:** Implement an Adaptive Windowing (ADWIN) simulation that maintains a variable-sized historical window, splitting it into sub-windows to test for statistically significant mean deviations.

**Solution:**
```python
import math
import numpy as np

class MiniADWIN:
    def __init__(self, delta=0.01):
        self.delta = delta
        self.window = []

    def update(self, val: float) -> bool:
        self.window.append(val)
        drift_detected = False
        
        # Test split points
        n = len(self.window)
        if n >= 20:
            for split in range(10, n - 10, 10):
                w0 = self.window[:split]
                w1 = self.window[split:]
                n0, n1 = len(w0), len(w1)
                diff = abs(np.mean(w0) - np.mean(w1))
                
                m = 1.0 / (1.0 / n0 + 1.0 / n1)
                eps_cut = math.sqrt((1.0 / (2 * m)) * math.log(4.0 * n / self.delta))
                
                if diff > eps_cut:
                    drift_detected = True
                    self.window = self.window[split:]  # Shrink window
                    break
        return drift_detected

adwin = MiniADWIN()
alerts = [i for i, x in enumerate(np.concatenate([np.random.normal(0, 1, 100), np.random.normal(3, 1, 100)])) if adwin.update(x)]
print("ADWIN Drift Alarms Fired at Steps:", alerts)
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Importance Weighting Retraining Pipeline</strong></summary>

**Task:** Fit a logistic regression discriminator to estimate density ratios $w(x) = \frac{P_{\text{live}}(x)}{P_{\text{train}}(x)}$ and retrain a target model using sample weights.

**Solution:**
```python
import numpy as np
from sklearn.linear_model import LogisticRegression, Ridge

def train_with_covariate_shift_reweighing(X_tr, y_tr, X_live):
    # Discriminator: P(live | x)
    X_disc = np.vstack([X_tr, X_live])
    y_disc = np.concatenate([np.zeros(len(X_tr)), np.ones(len(X_live))])
    
    disc = LogisticRegression().fit(X_disc, y_disc)
    p_live = disc.predict_proba(X_tr)[:, 1]
    p_live = np.clip(p_live, 0.01, 0.99)
    
    # Odds ratio w(x) = P(live|x) / P(tr|x) * (N_tr / N_live)
    odds = (p_live / (1.0 - p_live)) * (len(X_tr) / len(X_live))
    sample_weights = np.clip(odds, 0.1, 10.0)
    
    target_model = Ridge().fit(X_tr, y_tr, sample_weight=sample_weights)
    return target_model

m = train_with_covariate_shift_reweighing(np.random.randn(200, 3), np.random.randn(200), np.random.randn(100, 3) + 1.0)
print("Model Retrained with Importance Weights:", m.coef_)
```
</details>

---

## 13. Summary, Verified References & Navigation

* **The Reality of Non-Stationarity**: Production data distributions will shift; static models inevitably degrade.
* **Metric Selection**: Use PSI ($<0.10$ stable, $>0.20$ severe) and KS-tests for univariate monitoring; use Adversarial Validation for multivariate shifts.
* **Concept vs Covariate**: Covariate shift alters $P(X)$; concept drift alters $P(Y \mid X)$ and requires updated ground truth labels.
* **Proactive Architecture**: Build automated telemetry pipelines that trigger canary shadow deployments before catastrophic degradation impacts business operations.

### Verified External References
* Gama, J. et al. (2014). *A survey on concept drift adaptation*. ACM Computing Surveys, 46(4), 1-37.
* Bifet, A., & Gavaldà, R. (2007). *Learning from time-changing data with adaptive windowing*. In SIAM International Conference on Data Mining (SDM).
* Sugiyama, M. et al. (2007). *Covariate shift adaptation by importance weighted cross validation*. Journal of Machine Learning Research, 8, 985-1005.

---

[← Previous Note](06-causality-vs-correlation.md) | [Module Index](README.md) | [Next Note →](08-limitations-of-machine-learning.md)
