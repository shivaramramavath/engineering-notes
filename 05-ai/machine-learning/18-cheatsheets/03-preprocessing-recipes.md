# Cheatsheet 03: Machine Learning Preprocessing Recipes

> **Cheatsheet ID:** `05-ai/machine-learning/18-cheatsheets/03-preprocessing-recipes`  
> **Topic:** Copy-Paste Production Recipes for Data Transformation & Feature Engineering  
> **Target Version:** scikit-learn 1.9+, Python 3.11+  
> **Status:** Production Verified

---

## 1. Missing Value Imputation Recipes

### 1.1 Univariate Imputation with Missingness Indicator
```python
from sklearn.impute import SimpleImputer, MissingIndicator
from sklearn.pipeline import FeatureUnion, Pipeline

# Preserves both the median-imputed values AND binary flags indicating where missingness occurred
imputer_with_indicator = FeatureUnion(transformer_list=[
    ("median_impute", SimpleImputer(strategy="median")),
    ("missing_flag", MissingIndicator())
])
```

### 1.2 Multivariate Imputation (KNN & MICE)
```python
from sklearn.impute import KNNImputer
from sklearn.experimental import enable_iterative_imputer
from sklearn.impute import IterativeImputer
from sklearn.linear_model import BayesianRidge

# 1. K-Nearest Neighbors Imputer (Euclidean distance weighted)
knn_imputer = KNNImputer(n_neighbors=5, weights="distance")

# 2. Multivariate Imputation by Chained Equations (MICE)
mice_imputer = IterativeImputer(
    estimator=BayesianRidge(),
    max_iter=10,
    random_state=42,
    initial_strategy="median"
)
```

---

## 2. Feature Scaling & Distribution Normalization Recipes

### 2.1 Outlier-Robust Scaling vs Gaussian Quantile Mapping
```python
from sklearn.preprocessing import RobustScaler, QuantileTransformer, PowerTransformer

# Outlier-resistant scaling using median and Interquartile Range (IQR)
robust_scaler = RobustScaler(quantile_range=(25.0, 75.0), with_centering=True)

# Non-linear Yeo-Johnson Power Transform (stabilizes variance, handles negatives)
yeo_johnson = PowerTransformer(method="yeo-johnson", standardize=True)

# Non-parametric mapping directly to standard normal distribution
quantile_normalizer = QuantileTransformer(
    n_quantiles=1000, 
    output_distribution="normal", 
    random_state=42
)
```

---

## 3. Categorical Encoding Recipes

### 3.1 Low-Cardinality One-Hot vs High-Cardinality Target Encoding
```python
from sklearn.preprocessing import OneHotEncoder, TargetEncoder

# 1. Nominal Low-Cardinality: Drops first level to prevent collinearity, ignores unseen
ohe_encoder = OneHotEncoder(drop="first", handle_unknown="ignore", sparse_output=False)

# 2. High-Cardinality Target Encoding (Out-of-fold cross-validated to prevent leakage)
target_encoder = TargetEncoder(
    categories="auto",
    target_type="continuous", # or 'binary'
    smooth="auto",            # Empirical Bayes smoothing
    cv=5,                     # 5-fold cross-fitting
    random_state=42
)
```

### 3.2 Hashing Trick for Unbounded Cardinality
```python
from sklearn.feature_extraction import FeatureHasher

# Hashes arbitrary string dictionaries into a fixed 2^18 bucket vector
hasher = FeatureHasher(n_features=262144, input_type="string")
# Usage: hashed_matrix = hasher.transform([["user_1234", "merchant_88"], ["user_5678"]])
```

---

## 4. Text, Temporal & Cyclical Recipes

### 4.1 Sublinear TF-IDF Text Vectorizer
```python
from sklearn.feature_extraction.text import TfidfVectorizer

tfidf = TfidfVectorizer(
    sublinear_tf=True,        # 1 + log(tf) dampening
    min_df=5,                 # Prune words appearing in < 5 documents
    max_df=0.85,              # Prune corpus stop-words appearing in > 85% of documents
    ngram_range=(1, 2),       # Unigrams and bigrams
    norm="l2"
)
```

### 4.2 Cyclical Trigonometric Feature Generator
```python
import numpy as np
from sklearn.preprocessing import FunctionTransformer

def encode_cyclical_features(df_time_series, period=24.0):
    """Encodes hours [0, 23] into 2D smooth cyclical unit circle."""
    sin_feat = np.sin(2.0 * np.pi * df_time_series / period)
    cos_feat = np.cos(2.0 * np.pi * df_time_series / period)
    return np.column_stack([sin_feat, cos_feat])

cyclical_transformer = FunctionTransformer(
    func=encode_cyclical_features,
    validate=True
)
```

---

## 5. Outlier Winsorization & Capping Recipe

```python
import numpy as np
from sklearn.base import BaseEstimator, TransformerMixin

class OutlierCapper(BaseEstimator, TransformerMixin):
    """Caps continuous features at empirical percentile bounds (Winsorization)."""
    def __init__(self, lower_percentile=1.0, upper_percentile=99.0):
        self.lower_percentile = lower_percentile
        self.upper_percentile = upper_percentile
        self.lower_bounds_ = None
        self.upper_bounds_ = None

    def fit(self, X, y=None):
        X_arr = np.asarray(X)
        self.lower_bounds_ = np.percentile(X_arr, self.lower_percentile, axis=0)
        self.upper_bounds_ = np.percentile(X_arr, self.upper_percentile, axis=0)
        return self

    def transform(self, X):
        X_arr = np.asarray(X)
        return np.clip(X_arr, self.lower_bounds_, self.upper_bounds_)

    def get_feature_names_out(self, input_features=None):
        return input_features
```
