# Module 10: Evaluation & Metrics

Welcome to **Module 10: Evaluation & Metrics** of the Machine Learning Curriculum track.

Evaluation is the cornerstone of machine learning engineering. A model's measured metric is not merely a benchmark score—it defines the exact objective landscape that the optimization algorithm navigates. Choosing the wrong metric (such as using Accuracy on 99:1 imbalanced data, or optimizing raw MAPE on intermittent demand) directly misguides optimization and guarantees production failure.

This module provides a comprehensive, mathematically rigorous, and production-hardened guide to offline and online evaluation architectures across classification, regression, clustering, information retrieval, and reinforcement learning.

---

## Module Notes Overview

| Order | Note | Format | Level | Focus & Key Algorithms |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Classification Metrics](01-classification-metrics.md) | Concept | Core | Confusion Matrix, Accuracy Paradox, Precision/Recall, $F_\beta$ score, Matthews Correlation Coefficient (MCC), ROC-AUC, PR-AUC (Average Precision), Brier score, and Expected Calibration Error (ECE). |
| **02** | [Regression Metrics](02-regression-metrics.md) | Concept | Core | MSE, RMSE, MAE, Median Absolute Error (MedAE), MAPE, SMAPE, WAPE, $R^2$, Adjusted $R^2$, Explained Variance Score, Max Error ($L_\infty$), and Quantile Pinball loss. |
| **03** | [Clustering Validation Metrics](03-clustering-metrics.md) | Concept | Core | Internal validation (Silhouette Coefficient, Calinski-Harabasz Index, Davies-Bouldin Index, Dunn Index) vs External validation (Adjusted Rand Index [ARI], Normalized Mutual Information [NMI], Homogeneity, Completeness, V-Measure). |
| **04** | [Ranking & Recommendation Metrics](04-ranking-and-recommendation-metrics.md) | Concept | Core | Hit Rate@K, Precision@K, Recall@K, Mean Reciprocal Rank (MRR), Mean Average Precision (MAP@K), Cumulative Gain (CG), Discounted Cumulative Gain (DCG@K), NDCG@K, Diversity, Novelty, and Coverage. |
| **05** | [Reinforcement Learning Evaluation](05-rl-metrics-and-learning-curves.md) | Concept | Core | Episodic return, sample efficiency, value estimation error; Agarwal et al. (NeurIPS 2021) standard: Interquartile Mean (IQM), Stratified Bootstrap 95% Confidence Intervals, and Performance Profiles across 10–20+ seeds. |
| **06** | [Baselines, Sanity Checks & Behavioral Testing](06-baselines-and-sanity-checks.md) | Concept | Core | The baseline hierarchy (`DummyClassifier`, `DummyRegressor`, simple linear models); Pipeline sanity checks (overfit single batch, label permutation test); CheckList behavioral auditing (MFT, INV, DIR). |
| **07** | [Error Analysis & Sliced Evaluation](07-error-analysis.md) | Concept | Core | Andrew Ng's qualitative error taxonomies; Residuals vs Fitted plots (homoscedasticity vs non-linearity); Q-Q plots; Cook's Distance; Sliced Evaluation and worst-case slice auditing; Disparate impact four-fifths compliance. |
| **08** | [Statistical Significance & Confidence Intervals](08-statistical-significance-and-confidence-intervals.md) | Concept | Core | Cross-validation fold correlation pitfall (Dietterich); Nadeau & Bengio corrected resampled t-test; Dietterich 5x2cv paired t-test; McNemar's test for paired classifiers; Wilcoxon signed-rank test; Percentile Bootstrap CIs; Benjamini-Hochberg FDR. |
| **09** | [A/B Testing & Online Experimentation](09-a-b-testing-for-models.md) | Concept | Core | The offline-online metric disconnect; Power analysis and sample size determination; CUPED variance reduction (using pre-experiment covariates to slash variance by 50%); Team Draft Interleaving; Sample Ratio Mismatch (SRM). |
| **10** | [Evaluation Pitfalls & Anti-Patterns](10-evaluation-pitfalls.md) | Concept | Core | Goodhart's Law ("When a measure becomes a target, it ceases to be a good measure"); Metric gaming; Heckman selection / survivorship bias; Benchmark data contamination in foundation models (Canary GUIDs); Simpson's Paradox; Defensive Metric Triad. |

---

## Architectural Decision Flowchart

```
+---------------------------------------------------------------------------------------------------------+
|                                    EVALUATION METRIC MASTER TAXONOMY                                    |
|                                                                                                         |
|                                     What type of task are you evaluating?                               |
|                     /                     |                       |                      \              |
|                    /                      |                       |                       \             |
|          [Classification]            [Regression]            [Clustering]           [Ranking / Recs]    |
|                 |                         |                       |                         |           |
|        Class Distribution:         Error Penalty Type:     Ground Truth Labels?      Target Presentation|
|        /              \            /              \        /              \          /               \  |
|   [Balanced]     [Imbalanced]   [Quadratic]    [Robust]  [Yes]            [No]    [Binary]        [Graded]
|       |               |              |            |        |                |        |               |  |
|    ROC-AUC &       PR-AUC &        RMSE &       MAE &    ARI &         Silhouette,  MAP@K &       NDCG@K|
|    Accuracy         MCC            R^2          MedAE    NMI & V-Meas    CH, DB      MRR         & Diversity|
|                                                                                                         |
|                                                                                                         |
|   +-------------------------------------------------------------------------------------------------+   |
|   |                      ONLINE EXPERIMENTATION & DEPLOYMENT VERIFICATION GATES                     |   |
|   |                                                                                                 |   |
|   |   1. Power Sizing: Sample size N calculated from Minimum Detectable Effect (MDE)                 |   |
|   |   2. Pre-Test Variance Reduction: CUPED covariate adjustment (removes up to 50% variance)       |   |
|   |   3. Systemic Integrity Check: Sample Ratio Mismatch (SRM) Chi-Squared Gate (p > 0.001)        |   |
|   |   4. Defensive Metric Triad: Primary Driver + Counter-Balancing Guard + Health Constraints       |   |
|   +-------------------------------------------------------------------------------------------------+   |
+---------------------------------------------------------------------------------------------------------+
```

---

## Module Standards & Verification

- **Statistical Rigor**: Includes full implementations of the Nadeau-Bengio corrected t-test, McNemar's test, and Stratified Bootstrap Confidence Intervals, demonstrating the critical flaw of standard uncorrected t-tests on cross-validation folds.
- **Enterprise Experimentation**: Features production implementations of CUPED variance reduction, Team Draft Interleaving, and Sample Ratio Mismatch (SRM) alerting.
- **Data-Centric Diagnostics**: Integrates Andrew Ng's qualitative error taxonomy framework, Cook's distance leverage metrics, and automated Sliced Evaluation engines.
