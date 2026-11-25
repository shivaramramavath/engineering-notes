---
title: "Naive Bayes Classifiers: Gaussian, Multinomial, and Bernoulli"
description: "Mathematical formulation of Bayes' theorem, conditional independence, Laplace smoothing, Gaussian, Multinomial, and Bernoulli Naive Bayes."
track: "machine-learning"
module: "04-supervised-classification"
topic: "Naive Bayes"
order: 3
format: "algorithm"
level: "beginner"
tags: ["naive-bayes", "gaussian-nb", "multinomial-nb", "bernoulli-nb", "laplace-smoothing", "bayes-theorem", "text-classification"]
prerequisites: [
  "05-ai/machine-learning/01-math-foundations/03-probability-and-distributions.md",
  "05-ai/machine-learning/01-math-foundations/09-maximum-likelihood-and-bayes.md"
]
key_takeaways:
  - "Naive Bayes applies Bayes' theorem under the 'naive' assumption that all features are mutually conditionally independent given the class label."
  - "Despite the unrealistic independence assumption, Naive Bayes often produces optimal 0-1 classification decisions even when posterior probabilities are poorly calibrated."
  - "Laplace / Lidstone smoothing (alpha > 0) prevents zero-frequency zero-probability collapse on unseen test words."
  - "MultinomialNB models token counts; BernoulliNB models binary token presence/absence; GaussianNB models continuous features via normal likelihoods."
---

# Naive Bayes Classifiers: Gaussian, Multinomial, and Bernoulli

## 1. Algorithm Card

```
+-----------------------------------------------------------------------------------+
|                              ALGORITHM SPECIFICATION                              |
+-----------------------------------------------------------------------------------+
| Paradigm:         Supervised Learning (Generative Probabilistic Classification)   |
| Family:           Bayesian Maximum A Posteriori (MAP) Estimator                   |
| Input Space:      Gaussian: X in R^{N x D}; Multinomial: X in N_0^{N x D};        |
|                   Bernoulli: X in {0, 1}^{N x D}                                  |
| Output Space:     y in {0, 1, ..., C-1}                                           |
| Decision Rule:    y_hat = argmax_c [ ln P(Y = c) + sum_{j=1}^D ln P(x_j | Y = c) ]|
| Optimization:     Closed-form Maximum Likelihood / Empirical Frequency Counting   |
| Time Complexity:  Training: O(N * D) (Single pass counting)                       |
|                   Inference: O(C * D) (Fast vector additions in log space)        |
| Space Complexity: O(C * D) parameters (class conditional means, variances, or probs)|
| Key Assumptions:  Conditional Independence: P(x_1, ..., x_D | y) = prod P(x_j | y) |
| Invariances:      Gaussian: Invariant to linear scaling; Text: Invariant to docs  |
+-----------------------------------------------------------------------------------+
```

---

## 2. Architecture & Intuition

Given a feature vector $\mathbf{x} = [x_1, x_2, \dots, x_D]^T$, we wish to find the class $c \in \{0, \dots, C-1\}$ that maximizes the posterior probability $P(Y = c \mid \mathbf{x})$.
By **Bayes' Theorem**:

$$P(Y = c \mid \mathbf{x}) = \frac{P(\mathbf{x} \mid Y = c) \cdot P(Y = c)}{P(\mathbf{x})}$$

The denominator $P(\mathbf{x}) = \sum_j P(\mathbf{x} \mid Y = j) P(Y = j)$ is independent of class $c$ and acts as a constant normalizing factor.
However, estimating the joint likelihood $P(x_1, \dots, x_D \mid Y = c)$ directly requires estimating $2^D - 1$ parameters for binary features—impossible for real datasets.

### The "Naive" Conditional Independence Assumption

Naive Bayes assumes that **all features are mutually conditionally independent given the class label**:

$$P(\mathbf{x} \mid Y = c) = P(x_1, x_2, \dots, x_D \mid Y = c) = \prod_{j=1}^D P(x_j \mid Y = c)$$

The Maximum A Posteriori (MAP) classification rule reduces to:

$$\hat{y} = \arg\max_{c} \left[ P(Y = c) \prod_{j=1}^D P(x_j \mid Y = c) \right]$$

To prevent numerical underflow when multiplying dozens of tiny probabilities, we compute in **log-probability space**:

$$\hat{y} = \arg\max_{c} \left[ \ln P(Y = c) + \sum_{j=1}^D \ln P(x_j \mid Y = c) \right]$$

```
+-----------------------------------------------------------------------------------+
|                            NAIVE BAYES ARCHITECTURE                               |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|                   [Class Prior ln P(Y = c)]                                       |
|                               +                                                   |
|  [ln P(x_1 | c)] + [ln P(x_2 | c)] + ... + [ln P(x_D | c)]                        |
|                               =                                                   |
|                      Log Posterior Score S_c                                      |
|                               |                                                   |
|                               v                                                   |
|                 y_hat = argmax_c [ S_c ]                                          |
+-----------------------------------------------------------------------------------+
```

---

## 3. Theoretical Foundations & Mathematical Derivation

### 3.1 The Three Core Naive Bayes Variants

#### 1. Gaussian Naive Bayes (Continuous Features)
Assumes that continuous feature $x_j$ conditioned on class $c$ follows a normal distribution:
$$P(x_j \mid Y = c) = \frac{1}{\sqrt{2\pi \sigma_{jc}^2}} \exp\left( -\frac{(x_j - \mu_{jc})^2}{2\sigma_{jc}^2} \right)$$
Parameters are estimated via empirical mean $\mu_{jc}$ and sample variance $\sigma_{jc}^2$ within class $c$.

#### 2. Multinomial Naive Bayes (Count Data / Text BoW)
Models integer feature counts $\mathbf{x} = [x_1, \dots, x_D]$ (e.g., word frequencies in document):
$$P(\mathbf{x} \mid Y = c) = \frac{(\sum x_j)!}{\prod x_j!} \prod_{j=1}^D \theta_{jc}^{x_j}$$
The parameters $\theta_{jc}$ represent the probability of token $j$ appearing in a document of class $c$.

#### 3. Bernoulli Naive Bayes (Binary Indicators)
Models binary feature indicators $x_j \in \{0, 1\}$ (e.g., word presence/absence):
$$P(\mathbf{x} \mid Y = c) = \prod_{j=1}^D \left[ p_{jc}^{x_j} (1 - p_{jc})^{1 - x_j} \right]$$
Crucially, Bernoulli NB explicitly penalizes the **non-occurrence** of words ($1 - p_{jc}$).

### 3.2 Laplace / Lidstone Smoothing

In Multinomial NB, the maximum likelihood estimate of word probability is:
$$\theta_{jc} = \frac{N_{jc}}{N_c}$$
where $N_{jc}$ is the count of word $j$ in class $c$, and $N_c = \sum_k N_{kc}$ is total words in class $c$.
**The Zero-Frequency Problem**: If a word in the test document never appeared in class $c$ in the training set ($N_{jc} = 0$), then $\theta_{jc} = 0$.
The entire product becomes:
$$P(\mathbf{x} \mid Y = c) = \prod P(x_k \mid Y = c) = 0$$
A single novel word completely erases all positive evidence from thousands of other words!

**Laplace (Add-One) Smoothing ($\alpha = 1.0$)**:
$$\hat{\theta}_{jc} = \frac{N_{jc} + \alpha}{N_c + \alpha \cdot D}$$
where $D$ is the total vocabulary size.
Even if $N_{jc} = 0$, $\hat{\theta}_{jc} = \frac{\alpha}{N_c + \alpha D} > 0$, preventing zero-probability collapse.

---

## 4. Computational Complexity Analysis

| Stage | Gaussian NB | Multinomial NB | Inference per Sample |
| :--- | :--- | :--- | :--- |
| **Training Time** | $\mathcal{O}(N \cdot D)$ (Compute $\mu, \sigma^2$) | $\mathcal{O}(N \cdot D)$ (Count frequencies) | N/A |
| **Inference Time** | $\mathcal{O}(C \cdot D)$ | $\mathcal{O}(C \cdot \text{NonZero}(D))$ | Ultra-Fast ($\sim \mu\text{s}$) |
| **Space Complexity**| $\mathcal{O}(C \cdot D)$ ($2 \times C \times D$ floats) | $\mathcal{O}(C \cdot D)$ log-prob table | Tiny |

---

## 5. Inductive Bias & Core Assumptions

1. **Conditional Independence**: The core assumption $\text{Cov}(x_i, x_j \mid y) = 0$ is almost always violated in reality (e.g., in text, "New" and "York" are strongly correlated).
   *Domingos & Pazzani (1997) Theorem*: Naive Bayes can be **strictly optimal under 0-1 misclassification loss** even when the independence assumption is completely violated, as long as the rank ordering of the log-posteriors remains correct.
2. **Poor Probability Calibration**: Because correlated features double-count evidence, Naive Bayes pushes predicted probabilities toward extreme edges ($0.00001$ or $0.99999$). Probabilities are poorly calibrated despite accurate classification decisions.

---

## 6. From-Scratch NumPy Implementation

```python
"""
Pure NumPy Implementation of Gaussian Naive Bayes and Multinomial Naive Bayes.
Tested on Python 3.11, NumPy 2.x.
"""

from typing import Dict
import numpy as np


class PureNumPyGaussianNB:
    """Gaussian Naive Bayes for continuous features."""

    def __init__(self, var_smoothing: float = 1e-9):
        self.var_smoothing = var_smoothing
        self.classes_: np.ndarray = np.array([])
        self.class_priors_: np.ndarray = np.array([])
        self.means_: np.ndarray = np.array([])
        self.variances_: np.ndarray = np.array([])

    def fit(self, X: np.ndarray, y: np.ndarray):
        N, D = X.shape
        self.classes_ = np.unique(y)
        C = len(self.classes_)

        self.class_priors_ = np.zeros(C)
        self.means_ = np.zeros((C, D))
        self.variances_ = np.zeros((C, D))

        for c_idx, c in enumerate(self.classes_):
            X_c = X[y == c]
            self.class_priors_[c_idx] = len(X_c) / N
            self.means_[c_idx] = np.mean(X_c, axis=0)
            # Add variance smoothing for numerical stability
            self.variances_[c_idx] = np.var(X_c, axis=0) + self.var_smoothing

        return self

    def predict_log_proba(self, X: np.ndarray) -> np.ndarray:
        N = len(X)
        C = len(self.classes_)
        log_posteriors = np.zeros((N, C))

        for c_idx in range(C):
            # Prior log P(Y = c)
            log_prior = np.log(self.class_priors_[c_idx])
            mu = self.means_[c_idx]
            var = self.variances_[c_idx]

            # Log Gaussian PDF: -0.5 * ln(2*pi*var) - ((x - mu)^2 / (2*var))
            log_likelihood = -0.5 * np.sum(np.log(2.0 * np.pi * var)) - 0.5 * np.sum(((X - mu) ** 2) / var, axis=1)
            log_posteriors[:, c_idx] = log_prior + log_likelihood

        return log_posteriors

    def predict(self, X: np.ndarray) -> np.ndarray:
        log_posteriors = self.predict_log_proba(X)
        best_indices = np.argmax(log_posteriors, axis=1)
        return self.classes_[best_indices]


class PureNumPyMultinomialNB:
    """Multinomial Naive Bayes with Laplace Smoothing for Count Data."""

    def __init__(self, alpha: float = 1.0):
        self.alpha = alpha
        self.classes_: np.ndarray = np.array([])
        self.class_log_priors_: np.ndarray = np.array([])
        self.feature_log_prob_: np.ndarray = np.array([])

    def fit(self, X: np.ndarray, y: np.ndarray):
        N, D = X.shape
        self.classes_ = np.unique(y)
        C = len(self.classes_)

        self.class_log_priors_ = np.zeros(C)
        self.feature_log_prob_ = np.zeros((C, D))

        for c_idx, c in enumerate(self.classes_):
            X_c = X[y == c]
            self.class_log_priors_[c_idx] = np.log(len(X_c) / N)

            # Total counts of each feature in class c + Laplace smoothing
            feature_counts = np.sum(X_c, axis=0) + self.alpha
            total_tokens = np.sum(feature_counts)
            self.feature_log_prob_[c_idx] = np.log(feature_counts / total_tokens)

        return self

    def predict(self, X: np.ndarray) -> np.ndarray:
        # X is (N, D). log_prob is (C, D)
        # Log likelihood is matrix multiply: X @ feature_log_prob_.T
        log_posteriors = X @ self.feature_log_prob_.T + self.class_log_priors_
        best_c = np.argmax(log_posteriors, axis=1)
        return self.classes_[best_c]


# Verification Run
if __name__ == "__main__":
    np.random.seed(42)
    # 1. Test Gaussian NB
    c0 = np.random.normal(loc=[-2.0, -2.0], scale=1.0, size=(100, 2))
    c1 = np.random.normal(loc=[+2.0, +2.0], scale=1.0, size=(100, 2))
    X_gauss = np.vstack([c0, c1])
    y_gauss = np.array([0] * 100 + [1] * 100)

    gnb = PureNumPyGaussianNB().fit(X_gauss, y_gauss)
    acc_g = np.mean(gnb.predict(X_gauss) == y_gauss)
    print(f"Gaussian NB Accuracy: {acc_g * 100:.2f}%")
    assert acc_g > 0.95

    # 2. Test Multinomial NB
    # Toy Document Count Matrix: [word_spam_free, word_urgent, word_meeting, word_report]
    X_counts = np.array([
        [5, 4, 0, 0], # Spam 1
        [6, 3, 0, 1], # Spam 2
        [0, 1, 4, 5], # Ham 1
        [0, 0, 6, 4], # Ham 2
    ])
    y_text = np.array([1, 1, 0, 0]) # 1=Spam, 0=Ham

    mnb = PureNumPyMultinomialNB(alpha=1.0).fit(X_counts, y_text)
    test_doc = np.array([[4, 2, 0, 0]]) # New spam doc
    pred = mnb.predict(test_doc)
    print(f"Multinomial NB Spam Detection: {pred[0]} (Expected: 1)")
    assert pred[0] == 1
    print("[SUCCESS] Pure NumPy Naive Bayes algorithms verified.")
```

---

## 7. Production-Grade Scikit-Learn Implementation

```python
"""
Production Pipeline: Text Classification with TfidfVectorizer and MultinomialNB.
"""

import numpy as np
from sklearn.pipeline import Pipeline
from sklearn.feature_extraction.text import TfidfVectorizer
from sklearn.naive_bayes import MultinomialNB
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, accuracy_score


def run_production_text_classifier():
    corpus = [
        "Win free money now click prize",
        "Urgent claim your prize cash bonus",
        "Free coupons gift card lottery winner",
        "Quarterly budget meeting agenda notes",
        "Please review the attached project report",
        "Weekly engineering standup update sync"
    ] * 50
    labels = [1, 1, 1, 0, 0, 0] * 50 # 1=Spam, 0=Ham

    X_train, X_test, y_train, y_test = train_test_split(corpus, labels, test_size=0.25, random_state=42)

    pipeline = Pipeline([
        ("tfidf", TfidfVectorizer(ngram_range=(1, 2), max_features=500)),
        ("nb", MultinomialNB(alpha=0.5))
    ])

    pipeline.fit(X_train, y_train)
    y_pred = pipeline.predict(X_test)

    acc = accuracy_score(y_test, y_pred)
    print("--- Production Naive Bayes Text Classification ---")
    print(f"Test Accuracy: {acc * 100:.2f}%")
    assert acc == 1.0


if __name__ == "__main__":
    run_production_text_classifier()
```

---

## 8. Model Persistence & Safe Serialization

```python
import joblib
import tempfile
import os

def persist_nb(pipeline):
    with tempfile.TemporaryDirectory() as tmp_dir:
        path = os.path.join(tmp_dir, "nb_pipeline.joblib")
        joblib.dump(pipeline, path)
        loaded = joblib.load(path)
        assert np.array_equal(loaded.named_steps["nb"].classes_, pipeline.named_steps["nb"].classes_)
        print("[SUCCESS] Naive Bayes pipeline persisted safely.")
```

---

## 9. Hyperparameter Tuning & Optimization

| Hyperparameter | Model | Search Space | Practical Guidance |
| :--- | :--- | :--- | :--- |
| `alpha` | MultinomialNB, BernoulliNB | `[0.01, 0.1, 0.5, 1.0, 2.0]` | Laplace smoothing parameter; smaller $\alpha$ fits rare tokens. |
| `var_smoothing`| GaussianNB | `np.logspace(-11, -7, 10)` | Portion of largest variance added to all features for stability. |
| `fit_prior` | All | `[True, False]` | If `False`, assumes uniform class priors ($P(Y=c) = 1/C$). |

---

## 10. Diagnostic Evaluations & Calibration Distortion

```
+-----------------------------------------------------------------------------------+
|                        NAIVE BAYES CALIBRATION WARNING                            |
+-----------------------------------------------------------------------------------+
| Because features are assumed independent, multiplying 50 weakly correlated tokens |
| pushes posteriors to p = 0.9999999 or p = 0.0000001.                              |
| Rule: Never trust raw Naive Bayes probabilities in high dimensions without        |
| applying CalibratedClassifierCV(method='isotonic').                               |
+-----------------------------------------------------------------------------------+
```

---

## 11. Calibration, Uncertainty & Prediction Intervals

Naive Bayes probabilities are notoriously **uncalibrated** (overconfident).
To restore calibration for risk scoring:
```python
from sklearn.calibration import CalibratedClassifierCV
calibrated_nb = CalibratedClassifierCV(MultinomialNB(), method="isotonic", cv=5)
calibrated_nb.fit(X_train, y_train)
```

---

## 12. Edge Cases, Failure Modes & Remediation

| Failure Mode | Root Cause | Observable Symptom | Engineering Remediation |
| :--- | :--- | :--- | :--- |
| **Zero Frequency Probability** | Word appears in test but never in training class | Entire product probability collapses to 0.0 | Set `alpha=1.0` (Laplace Smoothing). |
| **Numerical Underflow** | Multiplying hundreds of probabilities $p < 0.01$ | Floating point collapses to 0.0 | Sum in log space: $\ln P(c) + \sum \ln P(x_j \mid c)$. |
| **Zero Variance in Gaussian NB** | Constant continuous feature within a class | Division by zero in normal exponent | Configure `var_smoothing=1e-9`. |
| **Severe Correlated Features** | 10 identical redundant features added to model | Feature is weighted $10\times$ higher than other evidence | Use feature selection (prune collinear tokens via mutual info). |
| **Class Imbalance Distortion** | Majority class comprises 99% of data | Prior $P(c)$ overwhelms feature likelihood | Set `fit_prior=False` or use `ComplementNB`. |

---

## 13. Comparative Analysis: Naive Bayes vs Alternatives

| Dimension | Naive Bayes | Logistic Regression | Support Vector Machine | Random Forest |
| :--- | :--- | :--- | :--- | :--- |
| **Training Speed** | Ultra-Fast (Single Pass) | Fast (Iterative) | Moderate | Moderate |
| **Inference Speed** | Instant ($\sim \mu\text{s}$) | Instant | Fast | Fast |
| **Streaming Support** | Native `partial_fit()` | Native `partial_fit()` | Difficult | Difficult |
| **Handling Correlated Data**| Poor (Distorts probs) | Excellent (Weights adapt) | Excellent | Excellent |

---

## 14. Production Engineering & Serving Considerations

1. **Incremental Out-of-Core Learning**:
   Naive Bayes supports `partial_fit()`, allowing continuous streaming training on unbounded data streams without reloading historical records:
   ```python
   clf.partial_fit(X_stream_batch, y_stream_batch, classes=[0, 1])
   ```
2. **Precomputed Log-Odds Lookup**:
   In binary text classification, precompute the log-odds table for all vocabulary words:
   $$w_j = \ln \left( \frac{\hat{\theta}_{j, \text{spam}}}{\hat{\theta}_{j, \text{ham}}} \right)$$
   Inference reduces to summing scalar weights for active words: $\text{Score} = b + \sum_{j \in \text{Words}} w_j$. Execution takes $< 5\text{ microseconds}$ in pure C.

---

## 15. Interpretability & Feature Attribution

Naive Bayes is fully transparent:
The log-likelihood ratio (Information Value) for token $j$ is:
$$\Delta_j = \ln P(x_j \mid \text{Spam}) - \ln P(x_j \mid \text{Ham})$$
Tokens with large positive $\Delta_j$ ("free", "winner", "crypto") push toward Spam; tokens with large negative $\Delta_j$ ("meeting", "schedule") push toward Ham.

---

## 16. Hardware & Distributed Scaling

Because Naive Bayes requires only summing feature counts ($\sum x_{ij}$), it is trivially map-reducible across thousands of nodes in Apache Spark or Hadoop without iterative communication loops.

---

## 17. Verification & Diagnostics Test Harness

```python
import pytest
import numpy as np
from sklearn.naive_bayes import MultinomialNB

def test_laplace_smoothing_prevents_zero():
    # Token 1 never appears in Class 0
    X = np.array([[10, 0], [0, 10]])
    y = np.array([0, 1])
    mnb = MultinomialNB(alpha=1.0)
    mnb.fit(X, y)
    # The probability of Token 1 in Class 0 must be strictly positive
    token_prob = np.exp(mnb.feature_log_prob_)
    assert token_prob[0, 1] > 0.0
```

---

## 18. Cross-Track Connections

- **05-ai/machine-learning/01-math-foundations/03-probability-and-distributions.md**: Multinomial and Gaussian distributions.
- **05-ai/machine-learning/08-time-series-and-text-basics/04-text-preprocessing-and-bag-of-words.md**: Text tokenization and CountVectorizer.

---

## 19. Real-World Case Study: Real-Time Email Spam Filtering

An enterprise email security gateway processed 40,000 incoming emails per minute.
- **Challenge**: The mail transfer agent required spam classification within $2\text{ milliseconds}$ to prevent mail server queue backpressure.
- **Solution**:
  1. Tokenized emails using character 4-grams and unigrams.
  2. Deployed a `ComplementNB(alpha=0.1)` with precomputed log-odds tables.
  3. Memory consumption was under 4 MB, and single-email scoring latency was $80\text{ microseconds}$.
  4. Blocked 99.4% of inbound phishing campaigns with zero queue delays.

---

## 20. Technical Interview Questions & Solutions

### Question 1: What is the "Naive" assumption in Naive Bayes, and when does it fail?
**Answer**:
The naive assumption is that all features are **mutually conditionally independent given the class label**:
$$P(x_1, x_2, \dots, x_D \mid Y = c) = \prod_{j=1}^D P(x_j \mid Y = c)$$
It fails when features are correlated. For example, in text classification, the words "Hong" and "Kong" occur together almost deterministically. Naive Bayes treats them as two independent pieces of evidence, squaring their joint impact and drastically exaggerating the posterior probability.

### Question 2: Why can Naive Bayes perform well on classification even when its probability estimates are terribly uncalibrated?
**Answer**:
A 0-1 classification decision depends only on which class has the **maximum posterior score**:
$$\hat{y} = \arg\max_c P(Y = c \mid \mathbf{x})$$
It does **not** depend on the actual numerical value of $P(Y = c \mid \mathbf{x})$.
Even if correlated features cause the true probability $P(\text{Spam} \mid \mathbf{x}) = 0.75$ to be exaggerated to $0.99999$, the model still chooses class "Spam". As long as the rank ordering between competing classes is preserved, the classification boundary is identical to the optimal Bayes decision boundary.

### Question 3: What is Laplace Smoothing, and what happens if $\alpha = 0$?
**Answer**:
See Section 3.2. Laplace smoothing adds a constant $\alpha > 0$ to feature counts:
$$\hat{\theta}_{jc} = \frac{N_{jc} + \alpha}{N_c + \alpha D}$$
If $\alpha = 0$, any feature that did not appear in class $c$ in the training set receives zero probability ($\theta_{jc} = 0$). When a test sample contains this feature, multiplying by zero wipes out the entire joint probability ($\prod P = 0$), preventing the model from classifying the sample into class $c$ regardless of all other features.

### Question 4: How does Multinomial Naive Bayes differ from Bernoulli Naive Bayes?
**Answer**:
- **Multinomial NB**: Takes integer feature counts (e.g., word frequencies). It models the number of times each word appears.
- **Bernoulli NB**: Takes binary indicators $\{0, 1\}$ (word present vs absent).
  *Crucial Difference*: Bernoulli NB explicitly models the **absence** of words: it multiplies by $(1 - p_{jc})$ for every vocabulary word that does *not* appear in the document.

### Question 5: When should you use Complement Naive Bayes (CNB)?
**Answer**:
`ComplementNB` is designed specifically for **imbalanced datasets**. Instead of calculating statistics using the training instances of class $c$, it computes probabilities using the instances of **all classes except $c$** (the complement). This prevents majority classes from biasing parameter estimates.

### Question 6: Why is Gaussian Naive Bayes sensitive to feature scaling if linear models are?
**Answer**:
Gaussian Naive Bayes estimates an independent mean $\mu_{jc}$ and variance $\sigma_{jc}^2$ for every feature $j$. Because the term in the exponent is normalized by the feature's own variance:
$$\frac{(x_j - \mu_{jc})^2}{2\sigma_{jc}^2}$$
Multiplying feature $j$ by a constant $k$ scales $\mu$ by $k$ and $\sigma^2$ by $k^2$. The constant $k^2$ cancels out in the numerator and denominator! Gaussian NB is **strictly invariant to linear scaling of individual features**.

### Question 7: How do you handle continuous features with multimodal distributions in Naive Bayes?
**Answer**:
Gaussian NB assumes a single unimodal normal distribution. If a continuous feature is bimodal (e.g., two distinct peaks), Gaussian NB fits a wide variance curve that fails.
*Remedy*: Discretize the feature into $B$ bins using `KBinsDiscretizer` and fit a Multinomial NB, or model the density using **Kernel Density Estimation (KDE)**.

### Question 8: How does Naive Bayes support online / streaming learning?
**Answer**:
Because parameters are simple counts ($\sum x_{ij}$ and $N_c$), updating the model on a new data stream requires only adding the incoming feature counts to the existing accumulators:
$$N_{jc}^{(\text{new})} = N_{jc}^{(\text{old})} + \sum_{\text{batch}} x_{ij}$$
No optimization loops or matrix inversions are needed.

---

## 21. Guided Practice & Hands-On Exercises

### Exercise 1: Log-Sum-Exp Probability Normalizer
- Write a function that takes unnormalized log-posteriors and uses the `logsumexp` trick to output normalized probabilities in $[0, 1]$ without numerical underflow.

### Exercise 2: Text Spam Filter from Scratch
- Implement a complete Multinomial Naive Bayes spam classifier in pure Python without scikit-learn. Train it on 20 toy emails and assert that it correctly flags unseen test emails containing spam keywords.

### Exercise 3: Laplace Alpha Sensitivity Study
- Train a `MultinomialNB` on the 20 Newsgroups dataset across $\alpha \in [10^{-4}, 10.0]$. Plot the test accuracy vs $\alpha$ and identify the optimal smoothing value.

### Exercise 4: Gaussian NB vs Logistic Regression Boundary Plotter
- Generate 2D Gaussian data with correlated features ($r = 0.85$). Plot the decision boundary of Gaussian NB (which assumes independence) vs Logistic Regression.

### Exercise 5: Incremental Streaming Learning with `partial_fit`
- Train a `MultinomialNB` in streaming fashion on 10 sequential batches of 1,000 samples each, recording accuracy after each batch.

### Exercise 6: Information Value / Log-Odds Word Cloud
- Extract `feature_log_prob_` from a fitted `MultinomialNB` on spam data. Compute the top 20 words with the highest log-odds ratio $\ln(\theta_{\text{spam}} / \theta_{\text{ham}})$ and display them.
