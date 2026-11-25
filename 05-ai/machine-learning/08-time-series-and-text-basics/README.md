# Module 08: Time Series & Text Basics

Welcome to **Module 08: Time Series & Text Basics** of the Machine Learning Curriculum track.

Sequential and unstructured textual data present unique structural challenges to classical machine learning algorithms. While tabular data assumes independent and identically distributed ($i.i.d.$) samples, time series data violates independence through temporal autocorrelation, seasonality, and non-stationarity, while natural language violates vector-space assumptions through variable length, compositional syntax, and polysemy.

This module establishes the mathematical principles, statistical transformations, feature engineering pipelines, and diagnostic frameworks required to model temporal sequences and textual corpora using classical machine learning estimators.

---

## Module Notes Overview

| Order | Note | Format | Level | Focus & Key Algorithms |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Time Series Fundamentals](01-time-series-fundamentals.md) | Concept | Core | Stationarity, Autocorrelation (ACF/PACF), Additive/Multiplicative Decomposition, Augmented Dickey-Fuller (ADF) & KPSS tests, Differencing, Temporal Train-Test Splits (`TimeSeriesSplit`). |
| **02** | [Classical Forecasting Models](02-classical-forecasting.md) | Algorithm | Core | Moving Averages, Exponential Smoothing (SES, Holt's Linear, Holt-Winters Triple), ARMA, ARIMA, SARIMA, Box-Jenkins methodology, AIC/BIC model selection. |
| **03** | [Machine Learning for Time Series](03-ml-for-time-series.md) | Algorithm | Advanced | Tabularizing time series; Lag features, Rolling/Expanding window statistics, Calendar/Fourier cyclical encoding; Target leakage avoidance; GBDT multi-step forecasting (Recursive vs Direct vs Multi-Output). |
| **04** | [Text Preprocessing & Bag-of-Words](04-text-preprocessing-and-bag-of-words.md) | Concept | Core | Text normalization, Regex tokenization, Lemmatization vs Stemming, Stopword filtering, Vocabulary building, N-grams, CountVectorizer, Sparse matrix representations. |
| **05** | [TF-IDF & Text Classification](05-tf-idf-and-text-classification.md) | Algorithm | Core | Term Frequency-Inverse Document Frequency mathematical derivation, Sublinear TF scaling, Smooth IDF, Multinomial / Complement Naive Bayes, Linear SVM text classifiers, Benchmarks. |
| **06** | [Word Embeddings Introduction](06-word-embeddings-intro.md) | Concept | Intermediate | Distributed representations; One-hot vs Dense embeddings; Word2Vec (CBOW vs Skip-Gram with Negative Sampling); GloVe co-occurrence factorisation; FastText subwords; Semantic vector arithmetic. |

---

## Architectural Decision Flowchart

```
+---------------------------------------------------------------------------------------------------------+
|                               SEQUENTIAL & UNSTRUCTURED DATA WORKFLOW                                   |
|                                                                                                         |
|                                     What type of data are you modeling?                                 |
|                                     /                                  \                                |
|                                    /                                    \                               |
|                     [Time Series / Temporal Sequence]                [Natural Language Text]            |
|                                    |                                            |                       |
|                          Is the series Stationary?                     Vocabulary & Corpus Scale:       |
|                            /               \                            /                     \         |
|                          [No]             [Yes]                   [Small/Medium]           [Massive]    |
|                           |                 |                           |                      |        |
|                    Apply Differencing   Check ACF / PACF        Sparse Count Matrix    Dense Distributed|
|                    or Log Transform         |                   & Lexical Features:    Embeddings:      |
|                           |                 ▼                           |                      |        |
|                           +--------> Forecast Approach:                 ▼                      ▼        |
|                                     /              \               TF-IDF + Linear       Word2Vec /     |
|                          [Univariate /       [Multivariate /        SVM or Complement    FastText /     |
|                           Pure Autoreg]       Exogenous Covariates]  Naive Bayes         Sentence-Trans |
|                                 |                    |                                                  |
|                                 ▼                    ▼                                                  |
|                             ARIMA /             Lag & Rolling                                           |
|                             SARIMA /            Features +                                              |
|                             Holt-Winters        LightGBM / CatBoost                                     |
+---------------------------------------------------------------------------------------------------------+
```

---

## Cross-Module Synergies

- **Preceding Modules**: Builds upon Linear Regression ([Module 03](../03-supervised-regression/01-linear-regression.md)), Regularization ([Module 03](../03-supervised-regression/04-ridge-lasso-elasticnet.md)), and Naive Bayes ([Module 04](../04-supervised-classification/05-naive-bayes.md)).
- **Subsequent Modules**: Directly feeds into Loss Functions & Optimizers ([Module 09](../09-training-and-optimisation/01-loss-functions.md)), Evaluation Metrics ([Module 10](../10-evaluation-and-metrics/README.md)), and the Deep Learning Bridge ([Module 14](../14-deep-learning-bridge/README.md)) for Recurrent and Transformer architectures.
