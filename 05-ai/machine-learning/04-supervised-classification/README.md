# Module 04: Supervised Learning — Classification

Welcome to **Module 04: Supervised Learning — Classification** of the Machine Learning Curriculum track.

Classification algorithms model the functional mapping from feature space to discrete categorical class assignments: $f: \mathbb{R}^d \to \{1, \dots, K\}$. This module provides an exhaustive, mathematically rigorous, and production-ready journey through linear and kernel decision boundaries, distance-based voting, probabilistic generative modeling, recursive hierarchical partitioning, bagging and boosting ensembles, multiclass/multilabel strategies, probability calibration, decision thresholds, cost sensitivity, and neural networks.

---

## Module Notes Overview

| Order | Note | Format | Level | Focus & Key Algorithms |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Logistic Regression](01-logistic-regression.md) | Algorithm | Beginner | Sigmoid log-odds, Bernoulli cross-entropy, Newton-Raphson / IRLS, L1/L2/ElasticNet regularization, Odds Ratios. |
| **02** | [K-Nearest Neighbors (KNN) Classifier](02-knn-classifier.md) | Algorithm | Beginner | Distance metrics (Minkowski, Mahalanobis), KD-Trees / Ball-Trees, Curse of dimensionality, uniform vs distance weighting. |
| **03** | [Naive Bayes Classifiers](03-naive-bayes.md) | Algorithm | Beginner | Bayes theorem, conditional independence, GaussianNB, MultinomialNB, BernoulliNB, Laplace smoothing, text classification. |
| **04** | [Support Vector Machines (SVM)](04-support-vector-machines.md) | Algorithm | Intermediate | Maximum margin hyperplanes, primal and dual quadratic programs, KKT conditions, RBF/Poly kernels, Mercer's theorem. |
| **05** | [Decision Tree Classifier (CART)](05-decision-trees.md) | Algorithm | Intermediate | Recursive binary splitting, Gini impurity, Shannon entropy, Cost-Complexity Pruning (`ccp_alpha`), rule extraction. |
| **06** | [Random Forest Classifier](06-random-forest.md) | Algorithm | Intermediate | Bootstrap aggregation (Bagging), random feature subspaces ($m=\sqrt{d}$), Out-of-Bag (OOB) error estimation, TreeSHAP. |
| **07** | [Gradient Boosted Decision Trees (GBDT)](07-gradient-boosting.md) | Algorithm | Advanced | Functional gradient descent, pseudo-residuals, Newton-Raphson leaf updates, shrinkage, HistGradientBoosting, LightGBM. |
| **08** | [Bagging, Boosting, Stacking & Voting Ensembles](08-bagging-boosting-stacking-voting.md) | Algorithm | Advanced | Condorcet Jury Theorem, Hard vs Soft voting, Out-of-fold Stacking meta-learning, Blending, sub-ensemble distillation. |
| **09** | [Multiclass & Multilabel Strategies](09-multiclass-and-multilabel-strategies.md) | Concept | Intermediate | One-vs-Rest (OvR), One-vs-One (OvO), Multinomial Softmax, Binary Relevance, Classifier Chains, Hamming Loss. |
| **10** | [Probability Calibration](10-probability-calibration.md) | Concept | Intermediate | Reliability diagrams, Expected Calibration Error (ECE), Brier score decomposition, Platt scaling, Isotonic regression. |
| **11** | [Decision Thresholds, ROC & PR Curves](11-decision-thresholds-roc-and-pr.md) | Concept | Intermediate | Threshold tuning $\tau$, Confusion matrix rates, ROC-AUC, PR-AUC / Average Precision, Youden's J statistic, F1 maximization. |
| **12** | [Cost-Sensitive Classification](12-cost-sensitive-classification.md) | Concept | Advanced | Asymmetric cost matrices, Bayes risk minimization, Zadrozny-Elkan theorem, cost-weighted loss, Bayesian threshold shifting. |
| **13** | [Linear & Quadratic Discriminant Analysis (LDA & QDA)](13-linear-discriminant-analysis.md) | Algorithm | Intermediate | Fisher's criterion, within/between-class scatter, pooled covariance, Ledoit-Wolf shrinkage, sub-microsecond serving. |
| **14** | [Perceptron & Multilayer Perceptron (MLP)](14-perceptron-and-simple-neural-nets.md) | Algorithm | Intermediate | Rosenblatt's perceptron, Novikoff convergence theorem, XOR non-separability, Universal Approximation, Backpropagation. |
| **15** | [Choosing a Classifier: Architectural Guide](15-choosing-a-classifier-guide.md) | Concept | Advanced | No Free Lunch theorem, operational trade-off space, automated multi-classifier tournament framework, decision matrix. |

---

## Architectural Decision Flowchart

```
+--------------------------------------------------------------------------------------------------+
|                            SUPERVISED CLASSIFICATION MASTER TAXONOMY                             |
|                                                                                                  |
|                                   What is your primary design goal?                              |
|                                     /             |              \                               |
|                                    /              |               \                              |
|                   [Maximum Tabular Accuracy]   [Glass-Box Compliance]   [Ultra-Low Latency < 1µs]|
|                           |                           |                         |                |
|                  Are features dense?           Are rules required?       Linear Discriminant     |
|                      /          \                     /          \       Analysis (LDA) /        |
|                   [YES]         [NO: Text]         [YES]         [NO]    Logistic Regression     |
|                    /              \                 /              \                             |
|            HistGradientBoosting  LinearSVC /    Decision Tree   Logistic                        |
|            (or Random Forest)    Logistic Reg   (CART)          Regression                       |
+--------------------------------------------------------------------------------------------------+
```

---

## Testing & Verification Harnesses

All notes in this module contain automated `pytest` verification suites. To run the comprehensive classification verification suite across all algorithms:

```bash
# Run all automated verification tests within Module 04
pytest 05-ai/machine-learning/04-supervised-classification/ -v
```

---

## Cross-Track Learning Pathways

- **Mathematical Foundations**: Extends Linear Algebra, Calculus, and Maximum Likelihood in `05-ai/machine-learning/01-math-foundations`.
- **Data Engineering**: Data cleaning, scaling, and categorical encoding in `05-ai/machine-learning/02-data-and-preprocessing`.
- **Evaluation & Diagnostics**: Deep dive into classification metrics in `05-ai/machine-learning/10-evaluation-and-metrics`.
- **Serving Architecture**: Microservice deployment via FastAPI in `03-backend/fastapi/16-ai-and-ml-serving`.
