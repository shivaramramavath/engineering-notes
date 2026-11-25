# Module 03: Supervised Learning — Regression

Welcome to **Module 03: Supervised Learning — Regression** of the Machine Learning Curriculum track.

Regression algorithms model the functional mapping from feature space to continuous scalar targets: $f: \mathbb{R}^D \to \mathbb{R}$. This module provides an exhaustive, mathematically rigorous, and production-ready journey through linear models, regularization geometries, instance-based smoothers, decision trees, ensemble bagging, gradient boosting in function space, non-Gaussian Generalized Linear Models (GLMs), and non-parametric quantile estimation.

---

## Module Notes Overview

| Order | Note | Format | Level | Focus & Key Algorithms |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Linear Regression: OLS and Normal Equation](01-linear-regression.md) | Algorithm | Beginner | Ordinary Least Squares, Normal Equation derivation, Gauss-Markov theorem, Condition Number, pure NumPy solver. |
| **02** | [Multiple & Polynomial Regression](02-multiple-and-polynomial-regression.md) | Algorithm | Beginner | Non-linear basis expansions $\phi(x)$, Runge's phenomenon, combinatorial dimensionality, feature centering. |
| **03** | [Regularisation: Ridge, Lasso, and ElasticNet](03-regularisation-ridge-lasso-elasticnet.md) | Algorithm | Intermediate | $L_1$ and $L_2$ penalty geometry, SVD shrinkage, soft-thresholding operator, Coordinate Descent, ElasticNet grouping. |
| **04** | [K-Nearest Neighbors (KNN) Regression](04-knn-regression.md) | Algorithm | Beginner | Localized neighborhood smoothing, distance weighting, KD-Trees, Ball-Trees, curse of dimensionality, flatline limits. |
| **05** | [Decision Tree Regression: CART](05-decision-tree-regression.md) | Algorithm | Intermediate | Variance reduction splitting, recursive partitioning, Minimal Cost-Complexity Pruning (`ccp_alpha`), staircase artifacts. |
| **06** | [Support Vector Regression (SVR)](06-svr.md) | Algorithm | Intermediate | $\epsilon$-insensitive loss tube, slack variables, dual quadratic programming, RBF kernel trick, support vector sparsity. |
| **07** | [Random Forest Regression](07-random-forest-regression.md) | Algorithm | Intermediate | Bootstrap Aggregation (Bagging), tree de-correlation via random feature subspaces, Out-of-Bag (OOB) variance bounds. |
| **08** | [Gradient Boosting Regression (GBDT)](08-gradient-boosting-regression.md) | Algorithm | Intermediate | Functional gradient descent, pseudo-residuals, shrinkage $\eta$, Huber loss, modern architectures (XGBoost, LightGBM, CatBoost). |
| **09** | [Generalised Linear Models (GLMs)](09-generalised-linear-models.md) | Algorithm | Intermediate | Exponential Dispersion Family, link functions, IRLS optimization, Poisson count regression, Gamma severities, Tweedie. |
| **10** | [Quantile Regression](10-quantile-regression.md) | Algorithm | Intermediate | Pinball / check loss, median regression ($L_1$), linear programming formulation, heteroscedastic fans, non-crossing sorting. |
| **11** | [Regression Diagnostics](11-regression-diagnostics.md) | Concept | Intermediate | Residual plots, Breusch-Pagan heteroscedasticity test, Cook's distance, Hat matrix leverage $h_{ii}$, Durbin-Watson. |
| **12** | [Regression Metrics and Baselines](12-regression-metrics-and-baselines.md) | Concept | Beginner | Mathematical analysis of MAE, MSE, RMSE, RMSLE, $R^2$, MAPE, sMAPE, and `DummyRegressor` naive baselines. |

---

## Algorithm Selection Flowchart

Use the following architectural decision tree to select the optimal regression algorithm for your operational problem:

```
+-----------------------------------------------------------------------------------+
|                         REGRESSION ALGORITHM SELECTION                            |
+-----------------------------------------------------------------------------------+
                                          |
        +---------------------------------+---------------------------------+
        |                                                                   |
        v                                                                   v
 [Is Target Continuous Real?]                                 [Is Target Counts or Zero-Inflated?]
        |                                                                   |
        |                                             +---------------------+---------------------+
        |                                             |                                           |
        |                                             v                                           v
        |                                      [Discrete Counts N_0]                    [Zero-Mass + Positive]
        |                                      Poisson GLM (Log link)                  Tweedie GLM (p in 1.5)
        |
        +---------------------------------+
        |                                 |
        v                                 v
 [Need Full Distribution?]          [Need Point Estimate]
        |                                 |
 Quantile Regression                      +---------------------+---------------------+
 (Pinball Loss at tau)                    |                                           |
                                          v                                           v
                                   [Data Size Small-Medium]                     [Data Size Massive]
                                   [N < 50,000 | Needs Explainability]         [N > 100,000 | Max Performance]
                                          |                                           |
                         +----------------+----------------+                          v
                         |                                 |                   HistGradientBoosting
                         v                                 v                   (or XGBoost / LightGBM)
                  [Linear Trends?]                 [Complex Non-Linear?]
                         |                                 |
           +-------------+-------------+             +-----+-----+
           |                           |             |           |
           v                           v             v           v
    [Few Features]             [High Dim D > N]    Random Forest  SVR (RBF)
     Ordinary Least               Ridge / Lasso     (Fast Tuning) (N < 20k)
     Squares (OLS)                  ElasticNet
```

---

## Key Engineering Invariants

1. **Standardization is Mandatory for Regularization and Distances**:
   `StandardScaler` or `RobustScaler` must be applied before fitting Ridge, Lasso, ElasticNet, SVR, and KNN. Monotonic tree models (DecisionTree, RandomForest, GBDT) do not require scaling.
2. **Never Extrapolate with Tree Models**:
   Decision trees and random forests cannot predict values higher than the maximum target observed during training. If trend continuation is required, use Linear Regression, Splines, or hybrid residual ensembles.
3. **Always Benchmark Against Naive Baselines**:
   Every regression model must be evaluated against a `DummyRegressor(strategy='mean')` or `DummyRegressor(strategy='median')`. If test $R^2 \le 0$, the model performs worse than a simple horizontal line.
4. **Quantile Monotonicity**:
   When training multiple quantiles independently, apply post-hoc monotonizing rearrangement (`np.sort`) to prevent quantile crossing.

---

## Cross-Track Connections

- **01-languages/python/21-data-and-scientific-basics**: NumPy matrix algebra, BLAS bindings, and vectorization.
- **03-backend/fastapi/16-ai-and-ml-serving**: Serializing and serving regression pipelines in low-latency REST microservices.
- **05-ai/machine-learning/01-math-foundations**: Mathematical underpinnings of convex optimization, quadratic programming, and calculus.
- **05-ai/machine-learning/02-data-and-preprocessing**: Preprocessing pipelines, scaling, and feature selection.
- **05-ai/machine-learning/10-evaluation-and-metrics**: Deep dive into regression scoring and model validation.
