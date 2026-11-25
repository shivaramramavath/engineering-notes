# Module 02: Data Preparation & Preprocessing

Welcome to **Module 02: Data Preparation & Preprocessing** of the Machine Learning Curriculum track.

Data quality sets the upper bound on machine learning performance. While model architectures receive significant academic attention, real-world machine learning engineering failures originate predominantly in data collection ambiguities, undetected collinearity, leakage, or unhandled distribution shifts. This module provides a complete, mathematically grounded, and production-tested guide to preparing, cleaning, scaling, transforming, and validating tabular, text, and datetime features.

---

## Module Notes Overview

| Order | Note | Format | Level | Focus & Key Concepts |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Data Types and Sources](01-data-types-and-sources.md) | Concept | Beginner | Tabular, text, image, time-series, categorical scales (Stevens taxonomy), memory downcasting. |
| **02** | [Data Collection and Labelling](02-data-collection-and-labelling.md) | Concept | Intermediate | Annotation protocols, Cohen's & Fleiss' Kappa, programmatic weak supervision, Dawid-Skene consensus. |
| **03** | [Exploratory Data Analysis (EDA)](03-exploratory-data-analysis.md) | Concept | Beginner | Skewness, excess kurtosis, Pearson vs Spearman vs Kendall correlation, Variance Inflation Factor (VIF). |
| **04** | [Data Cleaning and Sanitisation](04-data-cleaning.md) | Concept | Beginner | Rubin missingness taxonomy (MCAR, MAR, MNAR), sentinel mapping, Levenshtein fuzzy deduplication. |
| **05** | [Feature Scaling and Normalisation](05-feature-scaling.md) | Concept | Beginner | StandardScaler, MinMaxScaler, RobustScaler, Yeo-Johnson PowerTransformer, QuantileTransformer. |
| **06** | [Encoding Categorical Features](06-encoding-categorical-features.md) | Concept | Intermediate | OneHotEncoder, OrdinalEncoder, TargetEncoder with cross-fitting, Weight of Evidence, FeatureHasher. |
| **07** | [Text and Datetime Feature Engineering](07-text-and-date-features-basics.md) | Concept | Intermediate | Cyclical trigonometric sin/cos encodings, calendar flags, TF-IDF and SVD for short tabular text. |
| **08** | [Feature Engineering Strategies](08-feature-engineering.md) | Concept | Intermediate | Combinatorial polynomial expansions, group-level contextual z-scores, KBinsDiscretizer strategies. |
| **09** | [Feature Selection Methods](09-feature-selection.md) | Concept | Intermediate | Filter (Mutual Information, ANOVA), Wrapper (RFE), Embedded (Lasso L1), Permutation Importance. |
| **10** | [Handling Imbalanced Datasets](10-handling-imbalanced-data.md) | Concept | Intermediate | Cost-weighting (`class_weight='balanced'`), pure SMOTE interpolation, ADASYN, PR-AUC vs ROC-AUC. |
| **11** | [Data Splitting and Cross-Validation](11-train-validation-test-splits-and-cross-validation.md) | Concept | Beginner | StratifiedKFold, GroupKFold for entity isolation, TimeSeriesSplit with gap, Nested Cross-Validation. |
| **12** | [Data Leakage Prevention](12-data-leakage.md) | Concept | Intermediate | Target leakage, train-test contamination, temporal lookahead, reverse causality, automated audit tests. |
| **13** | [Pipelines and ColumnTransformer](13-pipelines-and-columntransformer.md) | Concept | Intermediate | Scikit-learn Pipeline DAGs, ColumnTransformer routers, custom transformers, `set_output('pandas')`. |
| **14** | [Data Augmentation & Synthetic Data](14-data-augmentation-and-synthetic-data.md) | Concept | Intermediate | Tabular Mixup regularisation, Gaussian Copulas, Sklar's theorem, Train on Synthetic Test on Real (TSTR). |
| **15** | [Data Quality, Testing, and Versioning](15-data-quality-and-versioning.md) | Concept | Intermediate | The 6 quality dimensions, Pandera runtime schema contracts, content-addressed DVC versioning. |
| **16** | [Advanced Missing Data Imputation](16-handling-missing-data-in-depth.md) | Concept | Intermediate | SimpleImputer, MissingIndicator flags, KNNImputer with NaN-distance, IterativeImputer (MICE). |
| **17** | [Outlier Detection and Remediation](17-outlier-detection-for-preprocessing.md) | Concept | Intermediate | Tukey IQR, Median Absolute Deviation (MAD), Isolation Forest, Local Outlier Factor, Winsorization. |

---

## Key Engineering Invariants

1. **Strict Fit / Transform Segregation**:
   Never call `fit()` or `fit_transform()` on validation or test sets. All summary statistics (means, standard deviations, imputation values, quantiles, target encodings) must be learned strictly on $X_{\text{train}}$ and applied to $X_{\text{val}}$ and $X_{\text{test}}$ via `transform()`.
2. **Encapsulate in Pipelines**:
   Loose procedural scripts leak data. Encapsulate all feature transformations and estimators inside an `sklearn.pipeline.Pipeline` or `ColumnTransformer`.
3. **No Resampling of Test Partitions**:
   Never apply SMOTE, RandomUnderSampler, or oversampling to validation or test sets. Real-world evaluation must reflect the true, unmanipulated operational class distribution.
4. **Preserve DataFrame Column Traceability**:
   Standardize on `pipeline.set_output(transform="pandas")` to ensure transformed intermediate representations maintain named columns for interpretability and debugging.

---

## Cross-Track Connections

- **01-languages/python/21-data-and-scientific-basics**: Vectorized pandas operations, nullable types, and NumPy array broadcasting.
- **03-backend/fastapi/16-ai-and-ml-serving**: Serializing and serving scikit-learn `Pipeline` DAGs inside production REST microservices.
- **04-devops/git-github/01-git-fundamentals**: Data version control (DVC) integration with Git repositories.
- **05-ai/machine-learning/01-math-foundations**: Mathematical underpinnings of optimization, statistics, linear algebra, and information theory.
- **09-sql/05-grouping-and-aggregation**: Pre-aggregating entity-level features and window functions in SQL databases.
