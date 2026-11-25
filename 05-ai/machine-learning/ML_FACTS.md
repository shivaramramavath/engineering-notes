# Machine Learning Track: Verified Facts & Version Standards

> **Area:** `05-ai`  
> **Track:** `machine-learning`  
> **Date Verified:** 2026-11-25  
> **Baseline Python:** Python 3.11 / 3.12 (Supported through 3.14/3.15)  
> **Source Rule:** Every stated version, default, parameter, and API signature is verified against authoritative documentation and releases.

---

## 1. Verified Core Package Versions (As of November 2026)

| Package | Latest Stable Version | Release Milestone / Highlights | Source URL |
| :--- | :---: | :--- | :--- |
| **Python** | `3.11` / `3.12` / `3.13` | Python 3.11 is the production baseline for ML binaries. Python 3.13 introduces experimental free-threading (PEP 703). | [python.org](https://www.python.org/downloads/) |
| **scikit-learn** | `1.9.1` | Callback API (`set_callbacks`), `set_output` with Narwhals (pandas & polars), metadata routing, `sparse_interface` config. | [scikit-learn.org](https://scikit-learn.org/stable/release_history.html) |
| **NumPy** | `2.5.5` | NumPy 2.x standard: `StringDType`, Array API compliance, C-API compatibility layer, SIMD vectorization. | [numpy.org](https://numpy.org/doc/stable/release.html) |
| **pandas** | `3.1.1` | Pandas 3.x: Copy-on-Write enabled by default, PyArrow string backend integration, performance optimizations. | [pandas.pydata.org](https://pandas.pydata.org/docs/whatsnew/index.html) |
| **SciPy** | `1.16.x` | `sparray` replacing `spmatrix`, `scipy.stats.yeojohnson` numerical stability. | [scipy.org](https://scipy.org) |
| **XGBoost** | `3.4.2` | Native categorical support, `hist` tree method by default, GPU acceleration, PyArrow integration. | [xgboost.readthedocs.io](https://xgboost.readthedocs.io/en/stable/) |
| **LightGBM** | `4.7.0` | Hist-based gradient boosting, integer categorical handling, multi-threading scaling. | [lightgbm.readthedocs.io](https://lightgbm.readthedocs.io) |
| **CatBoost** | `1.2.10` | Ordered boosting, target statistics for categorical variables, symmetric trees. | [catboost.ai](https://catboost.ai) |
| **PyTorch (CPU)** | `2.15.0` | CPU inductor optimization, C++ TorchScript/AOTInductor, `torch.compile` speedups. | [pytorch.org](https://pytorch.org) |
| **Gymnasium** | `1.4.0` | Standardized `reset()` -> `(obs, info)` and `step()` -> `(obs, reward, terminated, truncated, info)`. | [gymnasium.farama.org](https://gymnasium.farama.org) |
| **Stable-Baselines3** | `2.9.2` | Gymnasium 1.x compatibility, PyTorch 2.x support, vector environments (`VecEnv`). | [stable-baselines3.readthedocs.io](https://stable-baselines3.readthedocs.io) |
| **Optuna** | `5.0.0` | Tree-structured Parzen Estimator (TPE), Hyperband pruning, async distributed trials. | [optuna.org](https://optuna.org) |
| **MLflow** | `3.18.0` | Experiment tracking, model registry, autologging for scikit-learn, PyTorch, XGBoost. | [mlflow.org](https://mlflow.org) |
| **skops** | `0.16.0` | Secure persistence for scikit-learn models without arbitrary code execution vulnerabilities. | [skops.readthedocs.io](https://skops.readthedocs.io) |
| **safetensors** | `0.8.0` | Fast zero-copy tensor storage format preventing code execution attacks. | [github.com/huggingface/safetensors](https://github.com/huggingface/safetensors) |
| **imbalanced-learn**| `0.14.2` | Resampling algorithms (SMOTE, ADASYN, RandomUnderSampler, BalancedRandomForest). | [imbalanced-learn.org](https://imbalanced-learn.org) |
| **umap-learn** | `0.6.0` | Uniform Manifold Approximation and Projection for non-linear dimensionality reduction. | [umap-learn.readthedocs.io](https://umap-learn.readthedocs.io) |

---

## 2. API Signatures & Critical Architectural Changes

### 2.1 scikit-learn 1.x / Modern Paradigm
* **`set_output` API:** Introduced in 1.2 and expanded in 1.4-1.9. Transformers can now natively return pandas DataFrames or polars DataFrames:
  ```python
  pipe.set_output(transform="pandas")
  ```
* **Metadata Routing:** Explicit parameter routing across meta-estimators (e.g. sample weights passing into `cross_val_score` or nested `Pipeline`):
  ```python
  sklearn.set_config(enable_metadata_routing=True)
  regressor.set_fit_request(sample_weight=True)
  ```
* **HistGradientBoosting:** `HistGradientBoostingClassifier` and `HistGradientBoostingRegressor` natively handle missing values (`NaN` routed to child node with maximum gain) and categorical features (`categorical_features=...`) without manual one-hot encoding.
* **`get_feature_names_out`:** Replaced the deprecated `get_feature_names` across all transformers (`OneHotEncoder`, `ColumnTransformer`, `CountVectorizer`).
* **Positional Arguments Deprecation:** Estimator `__init__` requires keyword-only arguments to enforce readable configs and future compatibility.

### 2.2 Gymnasium 1.x Standard Interaction Signatures
Gymnasium strictly decouples episode termination (agent failure/goal) from episode truncation (time limit exceeded):
* **`env.reset(seed=None, options=None)`** returns a tuple:
  ```python
  observation, info = env.reset(seed=42)
  ```
* **`env.step(action)`** returns a 5-element tuple:
  ```python
  observation, reward, terminated, truncated, info = env.step(action)
  ```
  - `terminated=True`: Absorbing state reached (falling over in CartPole, goal reached). Value of next state is 0.
  - `truncated=True`: Step count exceeded max episode limit (TimeLimit wrapper). State is not terminal; value bootstrapping applies.

### 2.3 Model Persistence & Security Protocols
* **Pickle / Joblib Hazard:** Inherent vulnerability to arbitrary code execution via Python object unpickling (`__reduce__`). Never load untrusted `.pkl` or `.joblib` files.
* **Secure Alternatives:**
  - `skops.io`: Scans zip structure against an allowlist of primitive types and scikit-learn classes (`skops.io.dump` and `skops.io.load(..., trusted=...)`).
  - `safetensors`: Pure tensor serialization without executable code.
  - `ONNX` (`onnxruntime`): Portable inference format standard across C++, Python, and browser runtimes.
