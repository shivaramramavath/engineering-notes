# Cheatsheet 04: Machine Learning Metrics & Mathematical Formulas

> **Cheatsheet ID:** `05-ai/machine-learning/18-cheatsheets/04-metrics-formulas`  
> **Topic:** Comprehensive Mathematical Formulation of ML Evaluation Metrics  
> **Status:** Production Verified

---

## 1. Classification Metrics (Confusion Matrix Derived)

| Metric | Mathematical Formula | Target Goal | Notes / Behavior under Imbalance |
| :--- | :---: | :---: | :--- |
| **Accuracy** | $\frac{\text{TP} + \text{TN}}{\text{TP} + \text{TN} + \text{FP} + \text{FN}}$ | $\to 1.0$ | Misleading on imbalanced data ($99\%$ negative base rate gives $99\%$ accuracy for random guesser). |
| **Precision** (PPV) | $\frac{\text{TP}}{\text{TP} + \text{FP}}$ | $\to 1.0$ | Fraction of declared positives that are actually positive. Penalizes false alarms. |
| **Recall / Sensitivity** (TPR) | $\frac{\text{TP}}{\text{TP} + \text{FN}}$ | $\to 1.0$ | Fraction of actual ground truth positives captured. Penalizes missed detections. |
| **Specificity** (TNR) | $\frac{\text{TN}}{\text{TN} + \text{FP}}$ | $\to 1.0$ | True negative capture rate. $1 - \text{FPR}$. |
| **$F_1$ Score** | $2 \cdot \frac{\text{Precision} \cdot \text{Recall}}{\text{Precision} + \text{Recall}} = \frac{2\text{TP}}{2\text{TP} + \text{FP} + \text{FN}}$ | $\to 1.0$ | Harmonic mean. Completely ignores TNs. |
| **$F_\beta$ Score** | $(1 + \beta^2) \frac{\text{Precision} \cdot \text{Recall}}{\beta^2 \text{Precision} + \text{Recall}}$ | $\to 1.0$ | $\beta=2$ weights Recall twice as heavily; $\beta=0.5$ weights Precision twice. |
| **Matthews Corr (MCC)** | $\frac{\text{TP} \times \text{TN} - \text{FP} \times \text{FN}}{\sqrt{(\text{TP}+\text{FP})(\text{TP}+\text{FN})(\text{TN}+\text{FP})(\text{TN}+\text{FN})}}$ | $\to +1.0$ | Pearson correlation between true and predicted labels $\in [-1, +1]$. Symmetrical across all 4 quadrants. |
| **Balanced Accuracy** | $\frac{1}{2} \left( \frac{\text{TP}}{\text{TP} + \text{FN}} + \frac{\text{TN}}{\text{TN} + \text{FP}} \right)$ | $\to 1.0$ | Average of Sensitivity and Specificity; drops to $0.50$ for random guessers under imbalance. |
| **Cohen's Kappa ($\kappa$)** | $\frac{p_o - p_e}{1 - p_e}$ | $\to 1.0$ | Agreement adjusted for chance probability $p_e$. |

---

## 2. Probabilistic & Calibration Metrics

* **Log-Loss (Binary Cross-Entropy)**:
  $$\text{Log-Loss} = -\frac{1}{N} \sum_{i=1}^N \left[ y_i \ln p_i + (1 - y_i) \ln(1 - p_i) \right]$$
* **Brier Score (Mean Squared Probability Error)**:
  $$\text{BS} = \frac{1}{N} \sum_{i=1}^N (p_i - y_i)^2 = \text{Reliability} - \text{Resolution} + \text{Uncertainty}$$
* **Expected Calibration Error (ECE)**:
  $$\text{ECE} = \sum_{m=1}^M \frac{|B_m|}{N} \left| \text{acc}(B_m) - \text{conf}(B_m) \right|$$
* **Maximum Calibration Error (MCE)**:
  $$\text{MCE} = \max_{m \in \{1, \dots, M\}} \left| \text{acc}(B_m) - \text{conf}(B_m) \right|$$

---

## 3. Regression Metrics

| Metric | Mathematical Formula | Optimal Prediction Function | Outlier Sensitivity |
| :--- | :---: | :---: | :---: |
| **Mean Squared Error (MSE)** | $\frac{1}{N} \sum_{i=1}^N (y_i - \hat{y}_i)^2$ | Conditional Mean $\mathbb{E}[y \mid \mathbf{x}]$ | **Extreme** (Quadratic penalty) |
| **Root Mean Squared Error (RMSE)** | $\sqrt{\frac{1}{N} \sum_{i=1}^N (y_i - \hat{y}_i)^2}$ | Conditional Mean $\mathbb{E}[y \mid \mathbf{x}]$ | **High** |
| **Mean Absolute Error (MAE)** | $\frac{1}{N} \sum_{i=1}^N \|y_i - \hat{y}_i\|$ | Conditional Median $\text{Med}(y \mid \mathbf{x})$ | **Low** (Robust) |
| **Median Absolute Error (MedAE)**| $\text{median}(\|y_1 - \hat{y}_1\|, \dots, \|y_N - \hat{y}_N\|)$ | Conditional Median | **Zero** |
| **Coefficient of Determination ($R^2$)**| $1 - \frac{\sum (y_i - \hat{y}_i)^2}{\sum (y_i - \bar{y})^2}$ | Scale-free $\in (-\infty, 1]$ | High |
| **Adjusted $R^2$** | $1 - (1 - R^2) \frac{N - 1}{N - p - 1}$ | Penalizes feature count $p$ | High |
| **Weighted Absolute Pct Error (WAPE)**| $\frac{\sum \|y_i - \hat{y}_i\|}{\sum \|y_i\|} \times 100\%$ | Supply-chain standard | Handles zeros gracefully |
| **Mean Absolute Scaled Error (MASE)**| $\frac{\frac{1}{h} \sum_{t=1}^h \|y_{N+t} - \hat{y}_{N+t}\|}{\frac{1}{N-1} \sum_{t=2}^N \|y_t - y_{t-1}\|}$ | Scale-free benchmark ($<1$ beats naive) | Symmetric and robust |
| **Pinball Loss (Quantile $\tau$)** | $\max(\tau(y - \hat{y}), (\tau - 1)(y - \hat{y}))$ | $\tau$-th Conditional Quantile | Linear asymmetric |

---

## 4. Clustering & Unsupervised Metrics

* **Silhouette Coefficient**:
  $$s(i) = \frac{b(i) - a(i)}{\max(a(i), b(i))} \in [-1, +1]$$
  where $a(i)$ is mean intra-cluster distance, $b(i)$ is nearest-cluster distance.
* **Calinski-Harabasz Index (Variance Ratio Criterion)**:
  $$CH = \frac{\text{Tr}(B_k)}{\text{Tr}(W_k)} \times \frac{N - K}{K - 1}$$
  where $B_k$ is between-cluster dispersion, $W_k$ is within-cluster dispersion. (Higher is better).
* **Davies-Bouldin Index**:
  $$DB = \frac{1}{K} \sum_{i=1}^K \max_{j \ne i} \left( \frac{\sigma_i + \sigma_j}{d(c_i, c_j)} \right)$$
  (Lower is better; measures average similarity between clusters).
* **Adjusted Rand Index (ARI)**:
  $$\text{ARI} = \frac{\text{Index} - \mathbb{E}[\text{Index}]}{\text{MaxIndex} - \mathbb{E}[\text{Index}]} \in [-1, +1]$$

---

## 5. Ranking & Information Retrieval Metrics

* **Mean Reciprocal Rank (MRR)**:
  $$\text{MRR} = \frac{1}{|Q|} \sum_{q \in Q} \frac{1}{\text{rank}_q}$$
* **Discounted Cumulative Gain (DCG@K)**:
  $$\text{DCG}@K = \sum_{i=1}^K \frac{2^{r_i} - 1}{\log_2(i + 1)}$$
* **Normalized DCG@K (NDCG@K)**:
  $$\text{NDCG}@K = \frac{\text{DCG}@K}{\text{IDCG}@K} \in [0, 1]$$
* **Mean Average Precision (MAP@K)**:
  $$\text{MAP}@K = \frac{1}{|Q|} \sum_{q \in Q} \frac{1}{\min(K, R_q)} \sum_{k=1}^K P(k) \times \text{rel}(k)$$

---

## 6. Distribution Shift & Drift Metrics

* **Population Stability Index (PSI)**:
  $$\text{PSI} = \sum_{k=1}^K (T_k - B_k) \times \ln\left( \frac{T_k}{B_k} \right)$$
  Thresholds: $\text{PSI} < 0.10$ (stable), $0.10 \le \text{PSI} < 0.25$ (moderate drift), $\ge 0.25$ (critical drift).
* **Kolmogorov-Smirnov (KS) Distance**:
  $$D_{\text{KS}} = \sup_x |F_{\text{prod}}(x) - F_{\text{ref}}(x)|$$
* **Wasserstein-1 (Earth Mover's) Distance**:
  $$\mathcal{W}_1(P, Q) = \int_{-\infty}^\infty |F_P(x) - F_Q(x)| dx$$
