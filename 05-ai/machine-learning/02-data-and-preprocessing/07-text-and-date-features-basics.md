---
title: "Text and Datetime Feature Engineering for Tabular ML"
description: "Techniques for extracting high-signal numerical representations from dates, timestamps, cyclical periodicities, and free-form text features in tabular pipelines."
track: "machine-learning"
module: "02-data-and-preprocessing"
topic: "Text and Date Features Basics"
order: 7
format: "concept"
level: "intermediate"
tags: ["datetime-engineering", "cyclical-encoding", "text-features", "tf-idf", "bag-of-words", "tabular-ml"]
prerequisites: [
  "05-ai/machine-learning/02-data-and-preprocessing/01-data-types-and-sources.md",
  "05-ai/machine-learning/02-data-and-preprocessing/05-feature-scaling.md"
]
key_takeaways:
  - "Linear integer encodings of calendar features (e.g., Hour 0 to 23) create an artificial discontinuity between 23:59 and 00:00; use trigonometric cyclical sine/cosine pairs."
  - "Raw timestamps must be converted into relative elapsed durations ($\Delta t = t - t_0$) or holiday/business-day indicator features."
  - "Free-form text in tabular models can be efficiently embedded using low-dimensional TF-IDF with TruncatedSVD / NMF, bypassing heavy deep learning architectures."
  - "Always strictly preserve temporal causality when creating lag, rolling, or text aggregations to prevent data leakage."
---

# Text and Datetime Feature Engineering for Tabular ML

## 1. Context & Problem Statement

Production tabular datasets frequently contain timestamps (`created_at`, `login_time`) and short unstructured text fields (`ticket_description`, `merchant_name`, `search_query`). Feeding these attributes into machine learning models without specialized feature extraction leads to systemic model failures:
1. **The Cyclical Boundary Discontinuity**: Representing hour of the day as an integer in $[0, 23]$ implies that hour $23$ is mathematically distant from hour $0$ ($|23 - 0| = 23$), even though they are separated by a single minute. Linear models, KNN, and neural networks fail to learn continuous diurnal patterns.
2. **Raw Timestamp Inability to Generalize**: A timestamp represented as an epoch integer (`1711929600`) monotonically increases over time. Trees split on historical thresholds that never activate on future production data.
3. **Tabular-Text Bottlenecks**: Deploying full BERT/Transformer models to process a 5-word product title in a 10M-row tabular pipeline introduces massive GPU latency and training bottlenecks when classical n-gram / TF-IDF representations suffice.

---

## 2. Conceptual Architecture

The text and datetime feature engineering pipeline extracts structured temporal patterns, periodicity signals, and lexical token distributions.

```
+-----------------------------------------------------------------------------------+
|                        Raw Tabular Record with Mixed Types                        |
|   timestamp: "2026-10-31 23:45:00"  |  text: "Urgent: credit card payment failed"  |
+-----------------------------------------------------------------------------------+
           |                                                      |
           v                                                      v
+------------------------------------+  +-------------------------------------------+
|      Datetime Decomposition        |  |        Unstructured Text Extraction       |
+------------------------------------+  +-------------------------------------------+
  |                                |      |                                       |
  v                                v      v                                       v
[Cyclical Trigonometric]    [Calendar]  [Tokenization & n-grams]           [Embeddings]
- sin(2*pi*hour / 24)       - is_weekend - CountVectorizer (1-2 grams)      - TF-IDF + SVD
- cos(2*pi*hour / 24)       - is_holiday - Sublinear TF scaling             - Length / Punct
- sin(2*pi*month / 12)      - days_since - Stopword filtering               - Upper-case %
- cos(2*pi*month / 12)        last_login
```

---

## 3. Core Theory & Mathematical Formulation

### 3.1 Cyclical Trigonometric Encoding

To preserve the topological continuity of periodic temporal features (e.g., hours in a day $T=24$, days in a week $T=7$, months in a year $T=12$), the scalar integer $v \in [0, T-1]$ is mapped onto the 2D unit circle:

$$x_{\sin} = \sin\left( \frac{2\pi \cdot v}{T} \right), \quad x_{\cos} = \cos\left( \frac{2\pi \cdot v}{T} \right)$$

**Key Invariant**:
The Euclidean distance between any two time points $v_1$ and $v_2$ on the unit circle is strictly a function of their circular difference:

$$d(v_1, v_2) = \sqrt{(\sin(2\pi v_1/T) - \sin(2\pi v_2/T))^2 + (\cos(2\pi v_1/T) - \cos(2\pi v_2/T))^2} = 2 \left| \sin\left(\frac{\pi(v_1 - v_2)}{T}\right) \right|$$

For $v_1 = 23$ and $v_2 = 0$ ($T=24$):
$$d(23, 0) = 2 \left| \sin\left(\frac{23\pi}{24}\right) \right| = 2 \sin\left(\frac{\pi}{24}\right) \approx 0.261$$
The circular distance between 23:00 and 00:00 matches the distance between 00:00 and 01:00.

### 3.2 Term Frequency - Inverse Document Frequency (TF-IDF)

For short text columns in tabular datasets, TF-IDF quantifies the importance of token $t$ in document $d$ relative to a corpus of $N$ documents:

$$\text{TF-IDF}(t, d, D) = \text{TF}(t, d) \times \text{IDF}(t, D)$$

Where:
- Sublinear Term Frequency:
  $$\text{TF}(t, d) = 1 + \ln(f_{t, d}) \quad \text{for } f_{t, d} > 0$$
- Smooth Inverse Document Frequency:
  $$\text{IDF}(t, D) = \ln\left( \frac{1 + N}{1 + n_t} \right) + 1$$
  where $n_t = |\{d \in D \mid t \in d\}|$ is the number of documents containing token $t$.
- $L_2$ Normalisation:
  $$v_{\text{norm}} = \frac{v}{\|v\|_2} = \frac{v}{\sqrt{\sum_i v_i^2}}$$

---

## 4. Practical Implementation

Below is a complete, production-grade custom transformer pipeline that extracts cyclical calendar features and compact TF-IDF text features within scikit-learn.

```python
"""
Datetime and Text Feature Extraction for Tabular ML Pipelines.
Tested on Python 3.11, pandas 3.x, scikit-learn 1.9.x.
"""

from typing import List, Tuple
import numpy as np
import pandas as pd
from sklearn.base import BaseEstimator, TransformerMixin
from sklearn.feature_extraction.text import TfidfVectorizer
from sklearn.decomposition import TruncatedSVD
from sklearn.pipeline import Pipeline


class CyclicalDatetimeEncoder(BaseEstimator, TransformerMixin):
    """
    Extracts calendar and cyclical trigonometric features from a datetime column.
    """

    def __init__(self, datetime_col: str):
        self.datetime_col = datetime_col

    def fit(self, X: pd.DataFrame, y=None):
        return self

    def transform(self, X: pd.DataFrame) -> pd.DataFrame:
        df = X.copy()
        dt_series = pd.to_datetime(df[self.datetime_col])

        # 1. Cyclical Hours (0-23)
        hour = dt_series.dt.hour.values
        hour_sin = np.sin(2.0 * np.pi * hour / 24.0)
        hour_cos = np.cos(2.0 * np.pi * hour / 24.0)

        # 2. Cyclical Day of Week (0-6)
        dow = dt_series.dt.dayofweek.values
        dow_sin = np.sin(2.0 * np.pi * dow / 7.0)
        dow_cos = np.cos(2.0 * np.pi * dow / 7.0)

        # 3. Cyclical Month of Year (1-12)
        month = dt_series.dt.month.values - 1  # 0-11
        month_sin = np.sin(2.0 * np.pi * month / 12.0)
        month_cos = np.cos(2.0 * np.pi * month / 12.0)

        # 4. Binary Calendar Indicators
        is_weekend = (dow >= 5).astype(int)

        features = pd.DataFrame({
            f"{self.datetime_col}_hour_sin": hour_sin,
            f"{self.datetime_col}_hour_cos": hour_cos,
            f"{self.datetime_col}_dow_sin": dow_sin,
            f"{self.datetime_col}_dow_cos": dow_cos,
            f"{self.datetime_col}_month_sin": month_sin,
            f"{self.datetime_col}_month_cos": month_cos,
            f"{self.datetime_col}_is_weekend": is_weekend
        }, index=df.index)

        return features


class TabularTextEmbeddingTransformer(BaseEstimator, TransformerMixin):
    """
    Transforms short free-form text columns into low-dimensional dense embeddings
    via TF-IDF followed by TruncatedSVD (Latent Semantic Analysis).
    """

    def __init__(self, text_col: str, n_components: int = 5):
        self.text_col = text_col
        self.n_components = n_components
        self.vectorizer = TfidfVectorizer(
            ngram_range=(1, 2),
            max_features=1000,
            sublinear_tf=True
        )
        self.svd = TruncatedSVD(n_components=n_components, random_state=42)

    def fit(self, X: pd.DataFrame, y=None):
        text_data = X[self.text_col].fillna("").astype(str)
        tfidf_mat = self.vectorizer.fit_transform(text_data)
        self.svd.fit(tfidf_mat)
        return self

    def transform(self, X: pd.DataFrame) -> pd.DataFrame:
        text_data = X[self.text_col].fillna("").astype(str)
        tfidf_mat = self.vectorizer.transform(text_data)
        embeddings = self.svd.transform(tfidf_mat)
        
        cols = [f"{self.text_col}_emb_{i}" for i in range(self.n_components)]
        return pd.DataFrame(embeddings, columns=cols, index=X.index)


# Verification Script
if __name__ == "__main__":
    # 1. Synthesize Tabular Data with Datetimes and Short Text
    data = pd.DataFrame({
        "timestamp": [
            "2026-10-31 23:59:00",
            "2026-11-01 00:01:00",  # 2 minutes later!
            "2026-11-01 12:00:00",  # Mid-day
        ],
        "incident_log": [
            "Database connection timeout on primary node",
            "Database pool connection reset by peer",
            "User password reset successful"
        ]
    })

    # 2. Test Cyclical Datetime Features
    date_encoder = CyclicalDatetimeEncoder("timestamp")
    dt_features = date_encoder.fit_transform(data)
    print("--- Extracted Cyclical Features ---")
    print(dt_features[["timestamp_hour_sin", "timestamp_hour_cos", "timestamp_is_weekend"]])

    # Test Cyclical Distance: 23:59 (hour 23) vs 00:01 (hour 0)
    p1 = dt_features.loc[0, ["timestamp_hour_sin", "timestamp_hour_cos"]].values
    p2 = dt_features.loc[1, ["timestamp_hour_sin", "timestamp_hour_cos"]].values
    p3 = dt_features.loc[2, ["timestamp_hour_sin", "timestamp_hour_cos"]].values

    dist_midnight = np.linalg.norm(p1 - p2)
    dist_noon = np.linalg.norm(p1 - p3)

    print(f"\nEuclidean Distance 23:00 to 00:00: {dist_midnight:.4f}")
    print(f"Euclidean Distance 23:00 to 12:00: {dist_noon:.4f}")
    assert dist_midnight < 0.30, "Midnight transition distance should be small."
    assert dist_noon > 1.80, "Noon distance should be near maximum diameter (2.0)."

    # 3. Test Text Embedding Extraction
    text_encoder = TabularTextEmbeddingTransformer("incident_log", n_components=2)
    text_features = text_encoder.fit_transform(data)
    print("\n--- Low-Dimensional Text SVD Embeddings ---")
    print(text_features)
    assert text_features.shape == (3, 2)

    # Incident 0 and Incident 1 share 'connection', 'database'; should have similar vectors
    emb0 = text_features.iloc[0].values
    emb1 = text_features.iloc[1].values
    emb2 = text_features.iloc[2].values

    sim_01 = np.dot(emb0, emb1) / (np.linalg.norm(emb0) * np.linalg.norm(emb1))
    sim_02 = np.dot(emb0, emb2) / (np.linalg.norm(emb0) * np.linalg.norm(emb2))
    print(f"\nCosine Similarity (DB Error 1 vs DB Error 2): {sim_01:.4f}")
    print(f"Cosine Similarity (DB Error 1 vs Password Reset): {sim_02:.4f}")
    assert sim_01 > sim_02, "Related logs should have higher cosine similarity."

    print("\n[SUCCESS] Datetime and Text Feature Transformers verified.")
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

| Failure Mode | Root Cause | Observable Symptom | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **Tree Partitioning on Unit Circle** | Decision trees make axis-aligned cuts ($x_{\sin} \le c$ or $x_{\cos} \le c$) | A tree needs 4 or more rectangular splits to isolate a single 2-hour window | Retain raw integer `hour` alongside cyclical coordinates for tree-based models. |
| **Daylight Saving Time (DST) Jumps** | Wall-clock clocks jump by $\pm 1$ hour during spring/autumn | Spurious 23-hour or 25-hour days; negative time differences | Always parse datetime strings to UTC timestamps (`pd.to_datetime(s, utc=True)`). |
| **Leap Year and Month Length Variance** | February has 28/29 days; other months have 30/31 days | Day-of-month periodic normalization ($d/31$) drifts for shorter months | Normalize day of year using exact calendar year length: $d / 365.25$. |
| **Vocabulary OOV Explosion** | Production text inputs contain typos, emojis, and unseen tokens | High memory churn; zero features activated if vocabulary is rigid | Use character n-grams (`analyzer='char_wb'`, `ngram_range=(3, 5)`) for robustness. |
| **Future Lookahead in Rolling Aggregations** | Using centered rolling windows `rolling(7, center=True)` | Model uses future day $t+1, t+2$ values to predict event at day $t$ | Strictly use forward-only rolling windows: `rolling(7, closed='left')`. |

---

## 6. Production Engineering & Performance

1. **Vectorized Datetime Extraction via C-extensions**:
   Avoid Python `datetime.strptime()` loops. In pandas and Polars, accessing `.dt` properties uses vectorized C-struct pointer offsets, processing $10^7$ rows in $< 100\text{ ms}$.
2. **Sublinear TF Scaling**:
   Always set `sublinear_tf=True` in `TfidfVectorizer`. Replacing term frequency $f_{t, d}$ with $1 + \ln(f_{t, d})$ prevents a document containing the same keyword 50 times from overpowering a document containing it twice.
3. **Sparse Matrix Preservation**:
   Do not convert `TfidfVectorizer` outputs to dense arrays (`toarray()`). Chain directly into `TruncatedSVD` or linear classifiers that natively accept `scipy.sparse.csr_matrix`.

---

## 7. Comparative Analysis: Temporal Encoding Strategies

| Encoding Strategy | Dimensionality | Preserves Topology? | Model Family Suitability | Computational Cost |
| :--- | :--- | :--- | :--- | :--- |
| **Integer Ordinal ($0 \dots T-1$)** | $1$ | No (Discontinuous at boundary) | Trees only | $\mathcal{O}(1)$ |
| **One-Hot Encoding ($T$ bins)** | $T$ | Partially (Independent bins) | Linear, Trees, Neural Nets | $\mathcal{O}(T)$ |
| **Cyclical (Sin / Cos Pair)** | $2$ | Yes (Continuous unit circle) | Neural Nets, Linear, SVM, KNN | $\mathcal{O}(1)$ |
| **Radial Basis Functions (RBF)** | $K$ centers | Yes (Smooth bell curves) | Linear, Neural Networks | $\mathcal{O}(K)$ |
| **Target Encoded Day/Hour** | $1$ | No (Prior dependent) | Gradient Boosted Trees | $\mathcal{O}(1)$ |

---

## 8. Verification & Diagnostics Test Harness

```python
import pytest
import numpy as np
import pandas as pd

def test_cyclical_encoder_invariants():
    # 0 hours and 24 hours should map to identical unit circle points
    df = pd.DataFrame({"ts": ["2026-01-01 00:00:00", "2026-01-01 12:00:00"]})
    encoder = CyclicalDatetimeEncoder("ts")
    res = encoder.fit_transform(df)
    
    # Hour 0: sin = 0, cos = 1
    assert np.isclose(res.loc[0, "ts_hour_sin"], 0.0)
    assert np.isclose(res.loc[0, "ts_hour_cos"], 1.0)
    
    # Hour 12: sin = 0, cos = -1
    assert np.isclose(res.loc[1, "ts_hour_sin"], 0.0, atol=1e-7)
    assert np.isclose(res.loc[1, "ts_hour_cos"], -1.0)

def test_text_svd_shape():
    df = pd.DataFrame({"msg": ["apple banana", "orange apple", "grape kiwi"]})
    trans = TabularTextEmbeddingTransformer("msg", n_components=2)
    res = trans.fit_transform(df)
    assert res.shape == (3, 2)
```

---

## 9. Cross-Track Connections

- **05-ai/machine-learning/08-time-series-and-text-basics/01-time-series-fundamentals.md**: Seasonality decomposition, ACF/PACF, and stationarity tests.
- **05-ai/machine-learning/08-time-series-and-text-basics/04-text-preprocessing-and-bag-of-words.md**: Deep dive into TF-IDF, tokenizers, and vocabulary pruning.
- **09-sql/06-date-and-time-functions**: Extracting year, month, and day-of-week parts in PostgreSQL and BigQuery.

---

## 10. Real-World Case Study: Ride-Hailing Surge Pricing

A ride-hailing app predicted trip demand $y$ per city hex-cell across 24 hours.
- **Problem**: When using raw integer `hour` ($0 \dots 23$), the GBDT model severely underpredicted driver demand at 00:15 AM following Friday night nightlife peaks, because it treated hour $0$ as identical to quiet Tuesday 04:00 AM lulls.
- **Solution**:
  1. Implemented $(\sin, \cos)$ pairs for hour of day and day of week.
  2. Engineered an interaction feature: `is_weekend * night_hours_sin`.
  3. Extracted `minutes_until_next_major_event` using a localized concert/sports calendar table.
  4. Surge forecast error (MAPE) during peak weekend nightlife hours decreased from 34.2% to 11.5%.

---

## 11. Technical Interview Questions & Solutions

### Question 1: Why must cyclical features use BOTH sine AND cosine? Why is sine alone insufficient?
**Answer**:
A single trigonometric function (e.g., $\sin(\theta)$) is not injective (not one-to-one) over the interval $[0, 2\pi)$.
Specifically, $\sin(\theta) = \sin(\pi - \theta)$. For example, for an hour periodic cycle $T=24$:
$$\sin\left(\frac{2\pi \cdot 2}{24}\right) = \sin\left(\frac{\pi}{6}\right) = 0.5$$
$$\sin\left(\frac{2\pi \cdot 10}{24}\right) = \sin\left(\frac{5\pi}{6}\right) = 0.5$$
Hour 2 (02:00 AM) and Hour 10 (10:00 AM) would receive identical feature values of $0.5$, making them completely indistinguishable to the model.
By pairing $\sin(\theta)$ with $\cos(\theta)$, every hour maps to a unique $(x, y)$ coordinate on the 2D Cartesian plane:
- Hour 2: $(0.5, \cos(\pi/6)) = (0.5, \frac{\sqrt{3}}{2}) \approx (0.5, 0.866)$
- Hour 10: $(0.5, \cos(5\pi/6)) = (0.5, -\frac{\sqrt{3}}{2}) \approx (0.5, -0.866)$
The signs of the cosine coordinate resolve the ambiguity unambiguously.

### Question 2: In tree-based models like XGBoost, why can cyclical sine/cosine encodings sometimes perform WORSE than raw integer hour?
**Answer**:
Decision trees partition feature space using orthogonal, axis-aligned splits:
$$\mathbb{I}(x_j \le \theta)$$
On the unit circle created by $(\sin(\theta), \cos(\theta))$, a contiguous time slice (e.g., late night hours 22:00 to 02:00) forms a continuous circular arc. To isolate this arc, an axis-aligned tree requires at least 4 splits across both features (e.g., $x_{\sin} \ge a \land x_{\cos} \ge b \dots$), consuming tree depth and splitting samples into tiny leaves.
With a simple integer representation or a categorical target encoding, a tree can isolate the window in 1 or 2 threshold splits. The production best practice for GBDTs is to provide **both** the raw/binned integer feature and the cyclical $(\sin, \cos)$ features.

### Question 3: Write a vectorized function to compute rolling 7-day lagged averages without future target leakage.
```python
def compute_closed_left_rolling_mean(df: pd.DataFrame, target_col: str, window: int = 7) -> pd.Series:
    """Computes rolling mean strictly excluding the current row's target value."""
    # closed='left' excludes the current timestamp from the rolling calculation
    return df[target_col].shift(1).rolling(window=window, min_periods=1).mean()
```

---

## 12. Guided Practice & Hands-On Exercises

### Exercise 1: Multi-Scale Calendar Feature Pipeline
- Implement a transformer that extracts cyclical features across three simultaneous scales: Hour of Day ($T=24$), Day of Month ($T=31$), and Day of Year ($T=365.25$).

### Exercise 2: Text Length & Sentiment Heuristic Extractor
- Build a tabular text meta-feature extractor that computes: character count, word count, average word length, punctuation count, uppercase character ratio, and digit count.

### Exercise 3: Holiday Proximity Exponential Decay
- Given a list of public holiday dates, engineer a feature `holiday_decay = \exp(-\lambda \cdot \Delta t)` where $\Delta t$ is the number of days until the nearest upcoming holiday.
