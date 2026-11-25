# Cheatsheet 05: Hyperparameter Tuning Reference by Algorithm

> **Cheatsheet ID:** `05-ai/machine-learning/18-cheatsheets/05-hyperparameters-by-algorithm`  
> **Topic:** Practical Tuning Ranges, Default Values & Search Distributions across Core ML Models  
> **Target Libraries:** scikit-learn 1.9+, XGBoost 2.1+, LightGBM 4.5+, CatBoost 1.2+  
> **Status:** Production Verified

---

## 1. Linear & Logistic Models

| Model & Parameter | Default Value | Recommended Search Range | Optuna / SciPy Distribution | What It Controls |
| :--- | :---: | :---: | :---: | :--- |
| **`Ridge.alpha`** | `1.0` | $[10^{-4}, 10^{4}]$ | `loguniform(1e-4, 1e4)` | L2 penalty strength; shrinks coefficients toward zero. |
| **`Lasso.alpha`** | `1.0` | $[10^{-5}, 10^{1}]$ | `loguniform(1e-5, 1e1)` | L1 penalty strength; drives non-informative weights to zero. |
| **`ElasticNet.l1_ratio`** | `0.5` | $[0.01, 0.99]$ | `uniform(0.01, 0.98)` | Balance between L1 (sparsity) and L2 (grouping). |
| **`LogisticRegression.C`**| `1.0` | $[10^{-4}, 10^{3}]$ | `loguniform(1e-4, 1e3)` | Inverse regularization parameter ($C = 1/\lambda$). |

---

## 2. Tree-Based Bagging Ensembles (Random Forest)

| Parameter | Default Value | Recommended Search Range | Distribution | Tuning Impact & Guidance |
| :--- | :---: | :---: | :---: | :--- |
| **`n_estimators`** | `100` | $[100, 500]$ | Integer | Higher is strictly better for variance reduction; plateaus after 300. |
| **`max_depth`** | `None` | $[8, 32]$ or `None` | Integer | Prevents individual trees from memorizing rare outlier instances. |
| **`min_samples_split`** | `2` | $[2, 20]$ | Integer | Higher values regularize the tree by requiring more evidence to split. |
| **`min_samples_leaf`** | `1` | $[1, 20]$ | Integer | Critical smoothing parameter; set $\ge 5$ on noisy tabular data. |
| **`max_features`** | `"sqrt"` | $["sqrt", "log2", 0.3, 0.5]$ | Categorical / Float | Random feature subset size per split; smaller decorrelates trees. |
| **`bootstrap`** | `True` | `[True, False]` | Categorical | Keep `True` to enable OOB evaluation and bagging variance reduction. |

---

## 3. Gradient Boosted Decision Trees (HistGBDT / LightGBM)

| Parameter (Scikit / LightGBM) | Default | Recommended Search Range | Distribution | Tuning Impact & Guidance |
| :--- | :---: | :---: | :---: | :--- |
| **`learning_rate`** | `0.10` | $[0.01, 0.20]$ | `loguniform(0.01, 0.20)` | Step size shrinkage. Lower rates require higher `max_iter`. |
| **`max_iter` / `n_estimators`**| `100` | $[100, 1000]$ | Integer (with early stopping)| Number of boosting iterations. Tune with early stopping (`patience=10`). |
| **`max_leaf_nodes` / `num_leaves`**| `31` | $[15, 127]$ | Integer | Primary model capacity control for leaf-wise tree growth. |
| **`min_samples_leaf`** | `20` | $[10, 100]$ | Integer | Prevents leaf overfitting on small cohorts. |
| **`l2_regularization` / `reg_lambda`**| `0.0` | $[10^{-3}, 10.0]$ | `loguniform(1e-3, 10.0)` | L2 penalty on optimal leaf weights $w_j^* = -\frac{G}{H + \lambda}$. |
| **`subsample` (bagging)** | `1.0` | $[0.6, 1.0]$ | `uniform(0.6, 0.4)` | Fraction of samples used per tree; stochastic gradient boosting. |

---

## 4. XGBoost & CatBoost Specific Parameters

| Algorithm & Parameter | Default | Recommended Range | Distribution | Role |
| :--- | :---: | :---: | :---: | :--- |
| **`xgboost.colsample_bytree`** | `1.0` | $[0.5, 1.0]$ | `uniform(0.5, 0.5)` | Subsampling ratio of columns per tree. |
| **`xgboost.gamma` (min_split_loss)**| `0.0` | $[0.0, 5.0]$ | `uniform(0.0, 5.0)` | Minimum gain required to split a leaf. |
| **`xgboost.scale_pos_weight`**| `1.0` | $\frac{N_{\text{neg}}}{N_{\text{pos}}}$ | Calculated / Fixed | Weights positive class instances to counteract severe imbalance. |
| **`catboost.depth`** | `6` | $[4, 10]$ | Integer | Oblivious symmetric tree depth ($2^d$ leaves). |
| **`catboost.l2_leaf_reg`** | `3.0` | $[1.0, 10.0]$ | `uniform(1.0, 9.0)` | L2 penalty coefficient on leaf values. |
| **`catboost.border_count`** | `254` | $[32, 254]$ | Integer | Number of splits/bins for numerical feature quantization. |

---

## 5. Support Vector Machines & Kernel Methods

| Parameter | Default | Recommended Search Range | Distribution | What It Controls |
| :--- | :---: | :---: | :---: | :--- |
| **`SVC.C`** | `1.0` | $[10^{-2}, 10^{3}]$ | `loguniform(1e-2, 1e3)` | Penalty for margin slack violations $\xi_i$. |
| **`SVC.gamma` (RBF)** | `"scale"` | $[10^{-4}, 10^{1}]$ | `loguniform(1e-4, 1e1)` | RBF kernel inverse radius ($\gamma = \frac{1}{2\sigma^2}$). |
| **`SVR.epsilon`** | `0.1` | $[10^{-3}, 1.0]$ | `loguniform(1e-3, 1.0)` | Radius of $\epsilon$-insensitive loss tube around target. |

---

## 6. Deep Learning & Neural Network Optimizers

| Parameter | Default | Recommended Range | What It Controls & Operational Guidance |
| :--- | :---: | :---: | :--- |
| **`AdamW.lr`** | `1e-3` | $[10^{-5}, 10^{-2}]$ | Peak learning rate. Use cosine decay with warm-up. |
| **`AdamW.weight_decay`** | `1e-2` | $[10^{-4}, 10^{-1}]$ | True decoupled L2 decay on non-bias parameters. |
| **`Dropout.p`** | `0.5` (dense) / `0.1` (transf.)| $[0.0, 0.5]$ | Probability of zeroing units during training. |
| **Batch Size $B$** | `32` / `64` | $[32, 512]$ | Trade-off between GPU throughput and generalization noise. |
