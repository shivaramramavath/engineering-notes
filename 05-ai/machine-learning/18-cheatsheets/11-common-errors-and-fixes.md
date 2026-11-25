# Cheatsheet 11: Top 30 Machine Learning Errors, Stack Traces & Production Fixes

> **Cheatsheet ID:** `05-ai/machine-learning/18-cheatsheets/11-common-errors-and-fixes`  
> **Topic:** Exhaustive Diagnostics Runbook for Machine Learning Exceptions, Numerical Bugs & Pipeline Failures  
> **Target Version:** Python 3.11+, scikit-learn 1.9+, PyTorch 2.4+, NumPy 2.5+  
> **Status:** Production Verified

---

## 1. Top 30 ML Exceptions, Root Causes & Code Fixes

### Error 01: `ValueError: Found unknown categories ['X'] in column 0 during transform`
* **Root Cause**: `OneHotEncoder` encountered a new categorical level in test/production data not present during training `fit()`.
* **Fix**: Set `handle_unknown="ignore"` in `OneHotEncoder`:
  ```python
  encoder = OneHotEncoder(drop="first", handle_unknown="ignore", sparse_output=False)
  ```

---

### Error 02: `ValueError: Input X contains NaN` during Estimator `.fit()` or `.predict()`
* **Root Cause**: Upstream missing values were unhandled before passing to an algorithm that does not natively support NaNs (e.g., LogisticRegression, LinearSVC, RandomForest).
* **Fix**: Wrap estimator in a `Pipeline` containing `SimpleImputer(strategy="median")` or switch to `HistGradientBoostingClassifier` which handles NaNs natively.

---

### Error 03: `RuntimeError: view size is not compatible with input tensor's size and stride`
* **Root Cause**: Attempting to call `.view()` on a transposed or sliced PyTorch tensor whose underlying memory buffer is non-contiguous.
* **Fix**: Call `.contiguous()` before calling `.view()`, or call `.reshape()` directly:
  ```python
  # INCORRECT: x.transpose(0, 1).view(-1, 64)
  # CORRECT:
  x = x.transpose(0, 1).contiguous().view(-1, 64)
  # OR:
  x = x.transpose(0, 1).reshape(-1, 64)
  ```

---

### Error 04: Loss outputs `NaN` during PyTorch Neural Network Training
* **Root Cause**: Unbounded logits passed into explicit `torch.sigmoid()` followed by `nn.BCELoss()`, causing saturation and $\log(0) = -\infty \to \text{NaN}$.
* **Fix**: Remove explicit `nn.Sigmoid()` from model output head and use numerically stable `nn.BCEWithLogitsLoss()` (or `nn.CrossEntropyLoss()` for multiclass), which utilizes the stable LogSumExp trick.

---

### Error 05: Data Leakage: Artificially 100% Cross-Validation Accuracy Collapsing to Random Guessing on Test Set
* **Root Cause**: Preprocessing scalers, imputers, or target encoders were fit on the entire dataset `df` *before* invoking `train_test_split()` or `cross_val_score()`.
* **Fix**: Fit transformers strictly inside the fold using Scikit-Learn `Pipeline`. Never call `.fit()` or `.fit_transform()` on test data.

---

### Error 06: `ValueError: X has 15 features, but HistGradientBoosting is expecting 12 features`
* **Root Cause**: `OneHotEncoder` expanded differently between train and test sets because distinct category sets were present in each dataframe.
* **Fix**: Fit `OneHotEncoder` inside a `ColumnTransformer` once during training, and save the fitted transformer artifact alongside the model.

---

### Error 07: `RuntimeError: CUDA out of memory. Tried to allocate X GiB`
* **Root Cause**: GPU VRAM exhaustion caused by excessively large batch sizes or accumulating autograd computational graphs across iterations.
* **Fix**:
  1. Reduce `batch_size` and enable gradient accumulation (`loss = loss / accum_steps`).
  2. Ensure loss tracking detaches tensors: `running_loss += loss.item()` (NOT `running_loss += loss`).
  3. Wrap validation loops in `with torch.no_grad():`.
  4. Call `torch.cuda.empty_cache()`.

---

### Error 08: `ValueError: target is multiclass but average='binary'`
* **Root Cause**: Calling `f1_score(y_true, y_pred)` on multiclass targets ($K > 2$) without specifying an averaging method.
* **Fix**: Pass `average="macro"`, `average="micro"`, or `average="weighted"`.

---

### Error 09: Python Multiprocessing Freeze / Hang with `n_jobs=-1` on Windows / MacOS
* **Root Cause**: OpenMP / BLAS thread oversubscription deadlock between PyTorch/NumPy multi-threading and joblib process forking.
* **Fix**: Set environment variables before importing libraries:
  ```python
  import os
  os.environ["OMP_NUM_THREADS"] = "1"
  os.environ["MKL_NUM_THREADS"] = "1"
  # In __main__ guard:
  if __name__ == "__main__":
      pass
  ```

---

### Error 10: `ZeroDivisionError: float division by zero` in Custom Metrics (e.g., MAPE)
* **Root Cause**: Ground truth target $y_i = 0$ in $\frac{|y - \hat{y}|}{y}$.
* **Fix**: Migrate to Weighted Absolute Percentage Error (WAPE):
  ```python
  wape = np.sum(np.abs(y_true - y_pred)) / (np.sum(y_true) + 1e-9) * 100.0
  ```

---

### Error 11: `UserWarning: LightGBM / XGBoost: categorical_feature keyword has been set`
* **Root Cause**: Categorical columns passed as standard `object` strings rather than Pandas `category` dtype.
* **Fix**: Convert columns: `df[col] = df[col].astype("category")`.

---

### Error 12: `RuntimeError: Expected all tensors to be on the same device`
* **Root Cause**: Inputs are on CPU while neural network parameters are on CUDA GPU (or vice-versa).
* **Fix**: Explicitly match devices: `inputs = inputs.to(model.device)` or pass `device=device`.

---

### Error 13: `ValueError: The number of classes in y is 1, but minimum is 2`
* **Root Cause**: Mini-batch sampling under extreme class imbalance sampled zero positive instances.
* **Fix**: Use `StratifiedKFold` or `StratifiedSampler`, or increase batch size $B$.

---

### Error 14: Silent Model Stalling: Gradient Descent Loss Does Not Decrease Across Epochs
* **Root Cause**: Forgetting `optimizer.zero_grad()`, leading to accumulated historical gradients; or learning rate set too high causing divergence.
* **Fix**: Call `optimizer.zero_grad(set_to_none=True)` at the top of each loop; verify learning rate with learning rate range test.

---

### Error 15: `PicklingError: Can't pickle local object 'lambda'`
* **Root Cause**: Lambda functions cannot be serialized by `pickle` or `joblib`.
* **Fix**: Replace inline lambdas with top-level named functions.

---

### Error 16: `ValueError: y contains new labels` in `LabelEncoder` during `.transform()`
* **Root Cause**: `LabelEncoder` is strictly intended for targets, not features. Unseen test levels crash it.
* **Fix**: Use `OrdinalEncoder(handle_unknown="use_encoded_value", unknown_value=-1)` for features.

---

### Error 17: Inverted Metric Values: `neg_mean_squared_error` Returning Negative Numbers
* **Root Cause**: Scikit-Learn follows the convention that higher is always better; loss functions are negated.
* **Fix**: Negate when reporting: `rmse = np.sqrt(-cv_scores)`.

---

### Error 18: Tree Extrapolation Failure: Model Predicts Flat Line on Future Time-Series
* **Root Cause**: Decision trees cannot extrapolate slopes outside the min/max bounding box of training features.
* **Fix**: Train trees on differenced targets ($y_t - y_{t-1}$) and lag features, or use hybrid linear-tree models.

---

### Error 19: `ConvergenceWarning: Liblinear failed to converge, increase the number of iterations`
* **Root Cause**: Features unscaled in `LogisticRegression(solver='liblinear')` or `LinearSVC`.
* **Fix**: Scale features with `StandardScaler()` or `RobustScaler()`, and increase `max_iter=1000`.

---

### Error 20: Broadcast Shape Mismatch: Target Shape `(N, 1)` vs Prediction Shape `(N,)`
* **Root Cause**: Computing loss $(y - \hat{y})^2$ where $y \in \mathbb{R}^{N \times 1}$ and $\hat{y} \in \mathbb{R}^N$ triggers NumPy broadcasting to produce an $(N \times N)$ matrix!
* **Fix**: Explicitly flatten targets: `y = y.ravel()` or `y = y.squeeze()`.

---

### Error 21: `RuntimeError: Trying to backward through the graph a second time`
* **Root Cause**: Retaining computational graph references across iterations without calling `.detach()` on states in recurrent models (RNNs/RL).
* **Fix**: Detach hidden states: `hidden = hidden.detach()`.

---

### Error 22: Target Encoding Target Leakage During Cross-Validation
* **Root Cause**: Computing target means on the full training set without out-of-fold splitting.
* **Fix**: Use `TargetEncoder(cv=5)` in scikit-learn 1.9+.

---

### Error 23: Collinear Matrix Inversion Failure: `numpy.linalg.LinAlgError: Singular matrix`
* **Root Cause**: Perfectly correlated features in Ordinary Least Squares Normal Equation $(\mathbf{X}^T \mathbf{X})^{-1} \mathbf{X}^T \mathbf{y}$.
* **Fix**: Switch to Ridge Regression (`Ridge(alpha=1.0)`) or compute pseudoinverse `np.linalg.pinv()`.

---

### Error 24: `IndexError: Target -1 is out of bounds` in PyTorch `CrossEntropyLoss`
* **Root Cause**: Target labels contain negative numbers or have not been mapped to $[0, K-1]$.
* **Fix**: Remap labels to continuous integers starting at 0.

---

### Error 25: Silent Training Freeze: PyTorch DataLoader Worker OOM (Exit code 137)
* **Root Cause**: Multi-worker DataLoader copies large Python objects into shared `/dev/shm` memory, exceeding Docker shared memory limits.
* **Fix**: Increase `--shm-size=8g` in Docker or set `num_workers=0` for in-memory debugging.

---

### Error 26: `AttributeError: 'Pipeline' object has no attribute 'coef_'`
* **Root Cause**: Attempting to access estimator weights on the outer pipeline rather than the named final step.
* **Fix**: Access named step: `pipeline.named_steps["classifier"].coef_`.

---

### Error 27: Overconfident Predictions: Deep Neural Network Softmax Outputting 0.9999 for Wrong Classes
* **Root Cause**: Overfitting to hard one-hot targets driving logit norms to infinity.
* **Fix**: Apply Label Smoothing (`CrossEntropyLoss(label_smoothing=0.1)`) and Temperature Scaling calibration.

---

### Error 28: Infinite Loop / Regression Explosion in Feature Scaling
* **Root Cause**: Fitting a scaler on training data, then calling `.fit_transform()` on test data (destroying test alignment).
* **Fix**: Call `.fit()` on training data; call `.transform()` strictly on test data.

---

### Error 29: `TypeError: can't convert np.ndarray of type numpy.object_` to PyTorch Tensor
* **Root Cause**: NumPy array contains mixed types (strings, None, ints).
* **Fix**: Clean types explicitly: `X = X.astype(np.float32)`.

---

### Error 30: Model Deserialization Security Failure / Incompatibility
* **Root Cause**: Loading untrusted `.pkl` models using `pickle.load()` allowing arbitrary remote code execution via `__reduce__`.
* **Fix**: Migrate to `skops` (`skops.io.load`) for Scikit-Learn or `safetensors` for PyTorch weights.
