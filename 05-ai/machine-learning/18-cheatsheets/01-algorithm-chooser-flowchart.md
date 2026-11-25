# Cheatsheet 01: Machine Learning Algorithm Chooser Flowchart

> **Cheatsheet ID:** `05-ai/machine-learning/18-cheatsheets/01-algorithm-chooser-flowchart`  
> **Topic:** Comprehensive Machine Learning Algorithm Selection Flowcharts & Decision Matrices  
> **Status:** Production Verified

---

## 1. Master Decision Flowchart

```mermaid
flowchart TD
    Start(["START: Machine Learning Problem"]) --> HasLabels{"Do you have labeled target data y?"}
    
    %% SUPERVISED LEARNING
    HasLabels -->|YES: Supervised Learning| TargetType{"What is the target data type?"}
    
    TargetType -->|Continuous Real Value| RegTask["Regression Task"]
    TargetType -->|Discrete Categories| ClfTask["Classification Task"]
    TargetType -->|Ordered Sequences / Lags| TimeTask["Time Series Forecasting"]
    
    %% REGRESSION BRANCH
    RegTask --> RegInterp{"Is 100% strict interpretability required?"}
    RegInterp -->|YES| RegLin["Ordinary Least Squares / Ridge / Lasso / EBM"]
    RegInterp -->|NO| RegData{"Dataset Size & Structure"}
    RegData -->|Tabular N < 100k| RegTree["HistGradientBoosting / LightGBM / XGBoost"]
    RegData -->|Tabular N >= 100k| RegHist["LightGBM / CatBoost / HistGBDT"]
    RegData -->|High-Dimensional Text/Sparse| RegSGD["Ridge / LinearSVR / SGDRegressor"]
    RegData -->|Unstructured Images/Audio| RegDL["PyTorch ResNet / ConvNeXt / Transformer Head"]
    
    %% CLASSIFICATION BRANCH
    ClfTask --> ClfInterp{"Is glass-box interpretability required?"}
    ClfInterp -->|YES| ClfGlass["Logistic Regression / Decision Tree (Depth <= 4) / EBM"]
    ClfInterp -->|NO| ClfType{"Data Domain"}
    ClfType -->|Tabular Data| ClfTab{"Sample Size & Speed"}
    ClfTab -->|N < 10k & Noisy| ClfRF["Random Forest / Extra Trees"]
    ClfTab -->|N >= 10k & Top Accuracy| ClfGBDT["HistGradientBoosting / LightGBM / CatBoost"]
    ClfType -->|Text / NLP Sparse| ClfText["LinearSVC / MultinomialNB / RoBERTa"]
    ClfType -->|Images / Video / Audio| ClfVision["CNN / Vision Transformer (ViT)"]
    
    %% TIME SERIES BRANCH
    TimeTask --> MultiSeries{"Single Series or Multi-Store Hierarchical?"}
    MultiSeries -->|Single Classical| ARIMA["ARIMA / SARIMAX / Prophet"]
    MultiSeries -->|Hierarchical Multi-Store| GBDT_Lag["HistGBDT with Lag/Rolling Features"]
    MultiSeries -->|Long-Horizon Complex| DeepTS["PatchTST / Chronos / Temporal Fusion Transformer"]

    %% UNSUPERVISED LEARNING
    HasLabels -->|NO: Unsupervised Learning| UnsupGoal{"What is the unsupervised objective?"}
    
    UnsupGoal -->|Group similar samples| ClusterTask["Clustering Task"]
    UnsupGoal -->|Compress / Visualize high-D| DimRedTask["Dimensionality Reduction"]
    UnsupGoal -->|Detect rare outliers| AnomalyTask["Anomaly Detection"]
    
    %% CLUSTERING BRANCH
    ClusterTask --> KnowK{"Do you know number of clusters K?"}
    KnowK -->|YES & Spherical| KMeans["K-Means++ / MiniBatchKMeans"]
    KnowK -->|YES & Non-Spherical Ellipsoids| GMM["Gaussian Mixture Models (GMM)"]
    KnowK -->|NO & Arbitrary Shapes / Density| DBSCAN["DBSCAN / HDBSCAN"]
    KnowK -->|NO & Hierarchical Tree Needed| AggHier["Agglomerative Clustering (Ward Linkage)"]
    
    %% DIMENSIONALITY REDUCTION BRANCH
    DimRedTask --> DimGoal{"Goal: Latent Features or 2D/3D Visualization?"}
    DimGoal -->|Speed & Dense Features| PCA["PCA / IncrementalPCA"]
    DimGoal -->|Sparse Text / User-Item| SVD["TruncatedSVD (LSA)"]
    DimGoal -->|Additive Topic Parts| NMF["Non-Negative Matrix Factorization (NMF)"]
    DimGoal -->|2D/3D Exploratory Viz| Manifold{"Manifold Type"}
    Manifold -->|Cluster Separation Focus| TSNE["t-SNE (Student-t)"]
    Manifold -->|Global Topology & Speed| UMAP["UMAP"]
    
    %% ANOMALY DETECTION BRANCH
    AnomalyTask --> AnomType{"Data Structure"}
    AnomType -->|Multivariate Tabular| IFo["Isolation Forest (Fast, Tree-based)"]
    AnomType -->|Local Density Fluctuation| LOF["Local Outlier Factor (LOF)"]
    AnomType -->|Clean High-D Inliers Only| OCSVM["One-Class SVM / Elliptic Envelope"]

    %% REINFORCEMENT LEARNING
    HasLabels -->|INTERACTIVE: Agent & Reward| RLAction{"Action Space Type"}
    RLAction -->|Bandit Exploration (Static S)| Bandit["Thompson Sampling / UCB1"]
    RLAction -->|Discrete Dynamic Control| DiscRL["Deep Q-Network (Double DQN / Dueling)"]
    RLAction -->|Continuous Robotics / Actuators| ContRL["PPO (Actor-Critic) / SAC (Soft Actor-Critic)"]
```

---

## 2. Comprehensive Algorithm Selection Decision Matrix

| Problem Type | Best Algorithm | Key Strengths | Critical Limitations | Primary Hyperparameters |
| :--- | :--- | :--- | :--- | :--- |
| **Linear Regression** | Ridge Regression (`Ridge`) | Convex, closed-form, handles collinearity. | Assumes strict linearity and additivity. | `alpha` (L2 strength) |
| **Sparse Linear** | Lasso / ElasticNet | Automated feature selection (L1 zeros). | Biased toward dropping correlated groups. | `alpha`, `l1_ratio` |
| **Tabular Regression** | `HistGradientBoostingRegressor` | Outperforms deep models, native NaN support. | Non-extrapolative outside training bounds. | `max_iter`, `learning_rate`, `min_samples_leaf` |
| **Binary Classification** | `HistGradientBoostingClassifier` | State-of-the-art accuracy on tabular data. | Requires careful calibration on imbalanced data. | `learning_rate`, `max_iter`, `l2_regularization` |
| **Noisy Tabular** | `RandomForestClassifier` | Resists label noise, trivially parallel. | High memory footprint, slow inference on CPU. | `n_estimators`, `max_depth`, `max_features` |
| **Sparse Text Classification** | `CalibratedClassifierCV(LinearSVC)` | Extremely fast, high dimensional ($d > 10^5$). | Linear decision boundary only. | `C`, `method='sigmoid'` |
| **Interpretable Complex** | Explainable Boosting Machine (`EBM`) | Full glass-box accuracy comparable to GBDT. | Slow training on large feature counts ($d > 500$). | `max_bins`, `outer_bags`, `learning_rate` |
| **Unsupervised Spherical** | `KMeans(init='k-means++')` | Fast $O(N \cdot k \cdot d)$, predictable. | Fails on non-convex or unequal variance shapes. | `n_clusters`, `n_init` |
| **Density Clustering** | `HDBSCAN` | Finds arbitrary shapes, auto-detects noise. | Sensitive to distance metric and `min_cluster_size`. | `min_cluster_size`, `min_samples` |
| **Linear Dim Reduction** | `PCA` | Globally optimal variance capture via SVD. | Orthogonal linear projections only. | `n_components` |
| **Sparse Dim Reduction** | `TruncatedSVD` | Operates directly on sparse CSR matrices. | Skips centering (not identical to true PCA). | `n_components` |
| **Manifold Visualization** | `UMAP` | Preserves global structure, $10\times$ faster than t-SNE. | Stochastic initialization, non-metric. | `n_neighbors`, `min_dist`, `metric` |
| **Tabular Anomaly** | `IsolationForest` | Sub-linear $O(N \log N)$ complexity, robust. | Masking effects under extreme clustering. | `contamination`, `n_estimators` |
| **Continuous Control RL** | `PPO` (Actor-Critic) | Stable monotonic improvement, clipped loss. | Sample inefficient compared to off-policy RL. | `clip_range`, `gamma`, `gae_lambda` |
| **Maximum Sample RL** | `Soft Actor-Critic` (SAC) | Off-policy replay buffer, maximum entropy. | Sensitive to temperature hyperparameter $\alpha$. | `alpha`, `target_update_interval` |

---

## 3. Latency vs Accuracy vs Hardware Pareto Rules

1. **Sub-Millisecond Inference SLA ($< 1\text{ms}$)**:
   * *Selection*: Linear models (`Ridge`, `LogisticRegression`) or Treelite-compiled Decision Trees (`HistGradientBoosting`).
   * *Prohibited*: Large Deep Transformers, Ensembles of Ensembles, KNN ($O(N)$ query overhead).
2. **Strict Interpretability SLA (Regulated Banking / Medicine)**:
   * *Selection*: Regularized Generalized Linear Models (GLMs) or Explainable Boosting Machines (EBMs).
   * *Prohibited*: Deep Neural Networks, Kernel SVMs, Deep Random Forests.
3. **Extreme Cardinality ($> 100,000$ categories)**:
   * *Selection*: CatBoost (ordered target encoding) or LightGBM with integer binning.
   * *Prohibited*: One-Hot Encoding (memory crash).
