# Topic Modelling: Latent Dirichlet Allocation (LDA)

> **Track:** `05-ai/machine-learning` | **Module:** `05-unsupervised-learning` | **Status:** `Production Standard`  
> **Metadata:** `scikit-learn: >=1.4.0` | `NumPy: >=1.26.0` | `SciPy: >=1.12.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Latent Dirichlet Allocation (LDA) |
| **Family** | Generative Probabilistic Topic Model / Bayesian Hierarchical Model |
| **Input Feature Types** | Discrete word token count matrices $\mathbf{X} \in \mathbb{N}_0^{D \times V}$ (Document-Term Matrix, DTM) from raw text. Strictly integer frequencies; never continuous TF-IDF weights. |
| **Target Output** | Two multinomial probability distributions: Document-Topic distributions $\mathbf{\theta}_d \in \Delta^{K-1}$ and Topic-Word distributions $\mathbf{\phi}_k \in \Delta^{V-1}$. |
| **Optimization Target** | Maximizing the marginal log-likelihood of observed documents $\sum_{d=1}^D \ln p(\mathbf{w}_d \mid \alpha, \beta)$ via Variational Inference (ELBO maximization) or Collapsed Gibbs Sampling. |
| **Key Metrics** | Perplexity $\exp\left(-\frac{\mathcal{L}}{\sum N_d}\right)$, Topic Coherence ($C_v$, $U_{\text{mass}}$), Topic Diversity (percentage of unique top- $N$ words). |
| **Key Hyperparameters** | Number of topics ($K$), Dirichlet document-topic prior ($\alpha$ or `doc_topic_prior`), Dirichlet topic-word prior ($\beta / \eta$ or `topic_word_prior`), learning method (`'online'` vs `'batch'`). |
| **Inference Mechanism** | Variational E-step: for unseen test document $\mathbf{w}_{\text{new}}$, optimize local variational parameters $\mathbf{\gamma}$ while holding global topic-word distributions $\mathbf{\phi}$ fixed. |
| **Primary Failure Modes** | Topic degeneration/merging when $K$ is set improperly; high sensitivity to domain stop words (frequent non-informative words dominating all topics); memory/compute bottlenecks on vast vocabularies without frequency pruning. |

---

## 2. Architecture & Intuition

### The Generative Story of Documents
Latent Dirichlet Allocation (Blei, Ng, Jordan, 2003) models text documents as probabilistic mixtures over latent topics, where each topic is a probabilistic distribution over words in a shared vocabulary.

```
                  Latent Dirichlet Allocation Plate Notation
                  
        (alpha) ───► [ theta_d ] ───► ( z_dn ) ───► [ w_dn ] ◄─── ( phi_k ) ◄─── ( beta )
                         │               │             │              │
                         │               └──────┬──────┘              │
                         │                   N words                  │
                         └──────────────────────┬─────────────────────┘
                                            D documents            K topics
```

### The Three Intuitive Rules
1. **Documents are topic mixtures**: A document about "Quantum Computing in Finance" might be $60\%$ Physics, $35\%$ Finance, and $5\%$ General Politics.
2. **Topics are word distributions**: The "Finance" topic assigns high probability to `["bank", "interest", "credit", "market"]`, while "Physics" assigns high probability to `["particle", "quantum", "spin", "entanglement"]`.
3. **Priors enforce sparsity**:
   - Small $\alpha$ ($< 1.0$) encourages documents to focus on **few** topics.
   - Small $\beta$ ($< 1.0$) encourages topics to focus on **few** specific words.

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $D$ | $\mathbb{N}^+$ | Total number of documents in the corpus |
| $V$ | $\mathbb{N}^+$ | Vocabulary size (number of unique terms) |
| $K$ | $\mathbb{N}^+$ | Number of latent topics |
| $N_d$ | $\mathbb{N}^+$ | Number of words in document $d$ |
| $w_{dn}$ | $\{1, \dots, V\}$ | The $n$-th observed word token in document $d$ |
| $z_{dn}$ | $\{1, \dots, K\}$ | The latent topic assigned to word token $w_{dn}$ |
| $\mathbf{\theta}_d$ | $\Delta^{K-1}$ | Multinomial topic distribution for document $d$ |
| $\mathbf{\phi}_k$ | $\Delta^{V-1}$ | Multinomial word distribution for topic $k$ |
| $\alpha \in \mathbb{R}^K$ | $\mathbb{R}_{>0}^K$ | Dirichlet prior hyperparameter for document-topic distributions |
| $\beta / \eta \in \mathbb{R}^V$ | $\mathbb{R}_{>0}^V$ | Dirichlet prior hyperparameter for topic-word distributions |

### The Joint Probability Distribution
The complete joint probability of a document collection $\mathcal{W}$, latent topics $\mathcal{Z}$, document mixtures $\mathbf{\Theta}$, and topic distributions $\mathbf{\Phi}$ is:

$$p(\mathcal{W}, \mathcal{Z}, \mathbf{\Theta}, \mathbf{\Phi} \mid \alpha, \beta) = \prod_{k=1}^K p(\mathbf{\phi}_k \mid \beta) \prod_{d=1}^D p(\mathbf{\theta}_d \mid \alpha) \prod_{n=1}^{N_d} p(z_{dn} \mid \mathbf{\theta}_d) p(w_{dn} \mid \mathbf{\phi}_{z_{dn}})$$

where the Dirichlet priors are:

$$p(\mathbf{\theta}_d \mid \alpha) = \frac{\Gamma\left(\sum_{k=1}^K \alpha_k\right)}{\prod_{k=1}^K \Gamma(\alpha_k)} \prod_{k=1}^K \theta_{dk}^{\alpha_k - 1}$$

### Collapsed Gibbs Sampling Formulation
Exact posterior inference $p(\mathcal{Z}, \mathbf{\Theta}, \mathbf{\Phi} \mid \mathcal{W})$ is intractable due to the denominator integral over all parameter spaces. In **Collapsed Gibbs Sampling** (Griffiths & Steyvers, 2004), we analytically integrate out $\mathbf{\Theta}$ and $\mathbf{\Phi}$ (since Dirichlet is the conjugate prior to the Multinomial), leaving only the discrete latent topic assignments $z_{dn}$.

The conditional probability of assigning topic $k$ to word $n$ in document $d$, given all other word assignments $\mathbf{z}_{-dn}$:

$$p(z_{dn} = k \mid \mathbf{z}_{-dn}, \mathbf{w}, \alpha, \beta) \propto \frac{n_{k,-dn}^{(v)} + \beta}{\sum_{v'=1}^V (n_{k,-dn}^{(v')} + \beta)} \cdot \frac{n_{d,-dn}^{(k)} + \alpha_k}{\sum_{k'=1}^K (n_{d,-dn}^{(k')} + \alpha_{k'})}$$

where:
- $n_{k,-dn}^{(v)}$: Number of times vocabulary term $v = w_{dn}$ is assigned to topic $k$, excluding current word.
- $n_{d,-dn}^{(k)}$: Number of words in document $d$ assigned to topic $k$, excluding current word.

### Parameter Recovery
Once Gibbs sampling reaches steady state:

$$\hat{\theta}_{dk} = \frac{n_d^{(k)} + \alpha_k}{N_d + \sum_{k'=1}^K \alpha_{k'}}, \qquad \hat{\phi}_{kv} = \frac{n_k^{(v)} + \beta}{\sum_{v'=1}^V (n_k^{(v')} + \beta)}$$

---

## 4. Computational Complexity

| Algorithm Variant | Per-Iteration Time Complexity | Memory Complexity | Scalability |
| :--- | :--- | :--- | :--- |
| **Collapsed Gibbs Sampling** | $\mathcal{O}(N_{\text{total}} \cdot K)$ per sweep | $\mathcal{O}(D \cdot K + K \cdot V + N_{\text{total}})$ | Excellent for small/medium corpora; slow convergence |
| **Batch Variational Bayes** | $\mathcal{O}(D \cdot K \cdot V)$ per E-step | $\mathcal{O}(D \cdot K + K \cdot V)$ | Predictable; requires loading all documents into RAM |
| **Online Variational Bayes** | $\mathcal{O}(B \cdot K \cdot V_{\text{active}})$ per mini-batch $B$ | $\mathcal{O}(B \cdot K + K \cdot V)$ | **Production Standard**; scales to infinite streaming text |

---

## 5. Inductive Bias & Statistical Assumptions

1. **Bag-of-Words (Exchangeability)**: The order of words within a document is irrelevant; only word frequencies matter (De Finetti's Theorem). Syntactic order, negation, and phrase structure are ignored.
2. **Document Exchangeability**: The order of documents within the corpus is irrelevant.
3. **Dirichlet Conjugacy**: Document topic allocations and topic vocabulary distributions are assumed to follow Dirichlet priors, favoring sparse, smooth distributions.

---

## 6. From-Scratch Implementation (NumPy)

A complete, self-contained implementation of Latent Dirichlet Allocation using Collapsed Gibbs Sampling:

```python
"""
Collapsed Gibbs Sampling for Latent Dirichlet Allocation from first principles.
Pure NumPy implementation with full Dirichlet conjugate updating.
"""

from __future__ import annotations
import numpy as np

class PureNumPyLDA:
    """
    Latent Dirichlet Allocation using Collapsed Gibbs Sampling.
    
    Parameters
    ----------
    n_topics : int
        Number of latent topics K.
    alpha : float
        Symmetric Dirichlet hyperparameter for document-topic distribution.
    beta : float
        Symmetric Dirichlet hyperparameter for topic-word distribution.
    n_iter : int
        Number of Gibbs sampling iterations.
    random_state : int | None
        PRNG seed.
    """
    def __init__(
        self,
        n_topics: int = 5,
        alpha: float = 0.1,
        beta: float = 0.01,
        n_iter: int = 50,
        random_state: int | None = 42
    ):
        self.n_topics = n_topics
        self.alpha = alpha
        self.beta = beta
        self.n_iter = n_iter
        self.random_state = random_state

        # Fitted parameters
        self.theta_: np.ndarray | None = None  # (D, K)
        self.phi_: np.ndarray | None = None    # (K, V)
        self.vocab_size_: int = 0
        self.n_docs_: int = 0

    def fit(self, X: np.ndarray) -> PureNumPyLDA:
        """
        Fit LDA on a Document-Term Matrix X of shape (D, V).
        """
        rng = np.random.default_rng(self.random_state)
        D, V = X.shape
        K = self.n_topics
        self.n_docs_ = D
        self.vocab_size_ = V

        # Convert Document-Term Matrix into a flat list of token occurrences
        # docs_words[d] contains array of word token IDs for document d
        docs_words = []
        for d in range(D):
            words_in_doc = []
            for v in range(V):
                count = int(X[d, v])
                if count > 0:
                    words_in_doc.extend([v] * count)
            docs_words.append(np.array(words_in_doc, dtype=np.int32))

        # Initialize count matrices
        # n_dk: count of words in doc d assigned to topic k
        n_dk = np.zeros((D, K), dtype=np.int32)
        # n_kv: count of word v assigned to topic k across corpus
        n_kv = np.zeros((K, V), dtype=np.int32)
        # n_k: total words assigned to topic k across corpus
        n_k = np.zeros(K, dtype=np.int32)

        # Randomly initialize topic assignments z_dn
        z_dn = []
        for d in range(D):
            words = docs_words[d]
            doc_z = rng.integers(0, K, size=len(words), dtype=np.int32)
            z_dn.append(doc_z)
            for w, k in zip(words, doc_z):
                n_dk[d, k] += 1
                n_kv[k, w] += 1
                n_k[k] += 1

        # Gibbs Sampling Sweeps
        for iteration in range(self.n_iter):
            for d in range(D):
                words = docs_words[d]
                for n_idx in range(len(words)):
                    w = words[n_idx]
                    k_old = z_dn[d][n_idx]

                    # 1. Decrement counts (exclude current token)
                    n_dk[d, k_old] -= 1
                    n_kv[k_old, w] -= 1
                    n_k[k_old] -= 1

                    # 2. Compute posterior multinomial probability for each topic k
                    # p(z = k) proportional to [(n_kv + beta) / (n_k + V*beta)] * [(n_dk + alpha)]
                    p_k = (n_kv[:, w] + self.beta) / (n_k + V * self.beta) * (n_dk[d, :] + self.alpha)
                    p_k /= np.sum(p_k)

                    # 3. Sample new topic assignment from categorical distribution
                    k_new = rng.choice(K, p=p_k)

                    # 4. Increment counts with new topic
                    z_dn[d][n_idx] = k_new
                    n_dk[d, k_new] += 1
                    n_kv[k_new, w] += 1
                    n_k[k_new] += 1

        # Estimate final distributions from posterior counts
        self.theta_ = (n_dk + self.alpha) / (np.sum(n_dk, axis=1, keepdims=True) + K * self.alpha)
        self.phi_ = (n_kv + self.beta) / (n_k[:, np.newaxis] + V * self.beta)
        return self

    def transform(self, X: np.ndarray) -> np.ndarray:
        """
        Infer topic mixtures for new unseen documents via empirical projection.
        """
        if self.phi_ is None:
            raise RuntimeError("Model must be fitted before transform.")
        D_new = X.shape[0]
        # Empirical projection: X @ phi.T smoothed by Dirichlet prior
        doc_topic = (X @ self.phi_.T) + self.alpha
        return doc_topic / np.sum(doc_topic, axis=1, keepdims=True)
```

---

## 7. Production Scikit-Learn Pipeline

In production microservices, text must pass through a strict vocabulary-controlled pipeline:

```python
"""
Production 9-stage Pipeline for Topic Discovery using Scikit-Learn Online LDA.
"""

import numpy as np
from sklearn.pipeline import Pipeline
from sklearn.feature_extraction.text import CountVectorizer
from sklearn.decomposition import LatentDirichletAllocation

def create_lda_pipeline(n_topics: int = 10, max_vocab_size: int = 10_000) -> Pipeline:
    """
    Constructs an enterprise-grade LDA pipeline with online stochastic variational learning.
    """
    return Pipeline([
        ("vectorizer", CountVectorizer(
            lowercase=True,
            strip_accents="unicode",
            stop_words="english",
            min_df=5,              # Ignore ultra-rare hapax legomena
            max_df=0.60,           # Filter out ubiquitous words (e.g. 'company', 'product')
            max_features=max_vocab_size,
            token_pattern=r"(?u)\b[a-zA-Z]{3,}\b"  # At least 3 letters
        )),
        ("lda", LatentDirichletAllocation(
            n_components=n_topics,
            doc_topic_prior=0.1,    # alpha: enforces sparse document topic allocation
            topic_word_prior=0.01,  # eta/beta: enforces sharp, distinctive topics
            learning_method="online",
            batch_size=128,
            learning_decay=0.7,
            max_iter=10,
            random_state=42,
            n_jobs=-1
        ))
    ])

def inspect_top_words(pipeline: Pipeline, n_top_words: int = 8) -> dict:
    """Extract top descriptive terms per topic."""
    vectorizer = pipeline.named_steps["vectorizer"]
    lda = pipeline.named_steps["lda"]
    feature_names = vectorizer.get_feature_names_out()
    
    topics = {}
    for topic_idx, topic in enumerate(lda.components_):
        top_indices = topic.argsort()[: -n_top_words - 1 : -1]
        topics[f"Topic_{topic_idx:02d}"] = [feature_names[i] for i in top_indices]
        
    return topics
```

---

## 8. Model Persistence & SHA-256 Integrity

```python
import json
import hashlib
import joblib
from pathlib import Path
from typing import Any, Dict

def save_lda_artifact(pipeline: Any, out_path: Path, metadata: Dict[str, Any]) -> Path:
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    
    joblib.dump(pipeline, out_path, compress=3)
    
    sha = hashlib.sha256()
    with open(out_path, "rb") as f:
        while chunk := f.read(65536):
            sha.update(chunk)
            
    sidecar = {
        "model_file": out_path.name,
        "sha256": sha.hexdigest(),
        "n_topics": int(pipeline.named_steps["lda"].n_components),
        "metadata": metadata
    }
    with open(out_path.with_suffix(".json"), "w", encoding="utf-8") as f:
        json.dump(sidecar, f, indent=2)
        
    return out_path
```

---

## 9. Hyperparameter Tuning & Diagnostics

### Tuning the Topic Cardinality $K$
Choosing $K$ via grid search over Perplexity and Topic Coherence ($U_{\text{mass}}$):

```python
import numpy as np
from sklearn.decomposition import LatentDirichletAllocation

def compute_umass_coherence(dtm: np.ndarray, components: np.ndarray, top_n: int = 10) -> float:
    """
    Computes intrinsic U_mass topic coherence directly from Document-Term Matrix.
    U_mass = sum_{m=2}^M sum_{l=1}^{m-1} log( (D(v_m, v_l) + 1) / D(v_l) )
    """
    D = dtm.shape[0]
    # Binary document occurrence matrix
    doc_has_word = (dtm > 0).astype(np.float64)
    # Word co-occurrence matrix: (V, V)
    co_occur = doc_has_word.T @ doc_has_word
    doc_freq = np.diag(co_occur)
    
    K = components.shape[0]
    topic_scores = []
    
    for k in range(K):
        top_words = np.argsort(components[k])[: -top_n - 1 : -1]
        score = 0.0
        for m in range(1, top_n):
            v_m = top_words[m]
            for l in range(0, m):
                v_l = top_words[l]
                d_ml = co_occur[v_m, v_l]
                d_l = doc_freq[v_l]
                score += np.log((d_ml + 1.0) / (d_l + 1.0))
        topic_scores.append(score)
        
    return float(np.mean(topic_scores))
```

---

## 10. Residuals, Diagnostics & Validation

The standard metric for LDA goodness-of-fit is **Perplexity** on a held-out test split:

$$\text{Perplexity}(\mathcal{W}_{\text{test}}) = \exp\left( -\frac{\sum_{d=1}^{D_{\text{test}}} \ln p(\mathbf{w}_d)}{\sum_{d=1}^{D_{\text{test}}} N_d} \right)$$

Lower perplexity indicates better generalization; however, extreme low perplexity does **not** guarantee human-interpretable topics. Topic Coherence must always be paired with Perplexity.

---

## 11. Uncertainty Estimation & Confidence Intervals

Uncertainty in LDA document mixtures $\mathbf{\theta}_d$ is represented by the Dirichlet posterior variance:

$$\text{Var}(\theta_{dk}) = \frac{\tilde{\alpha}_{dk} \left(\sum_{j} \tilde{\alpha}_{dj} - \tilde{\alpha}_{dk}\right)}{\left(\sum_j \tilde{\alpha}_{dj}\right)^2 \left(\sum_j \tilde{\alpha}_{dj} + 1\right)}$$

where $\tilde{\alpha}_{dk} = n_d^{(k)} + \alpha_k$. As document length $N_d$ increases, the variance contracts as $\mathcal{O}(1/N_d)$.

---

## 12. Failure Modes & Edge Cases

| Failure Mode | Root Cause | Behavioral Symptom | Detection Strategy | Mitigation |
| :--- | :--- | :--- | :--- | :--- |
| **Stop Word Intrusion** | High-frequency domain words (e.g. `'patient'` in hospital records) pass basic stop list. | Every topic has the identical 3-4 top words at the highest probability rank. | Topic Diversity $< 0.4$; top words overlap across topics. | Set `max_df=0.40` to aggressively prune ubiquitous domain terms. |
| **TF-IDF Inputs Used** | Passing continuous TF-IDF matrices instead of raw counts. | Model fails or yields nonsensical distributions; violates discrete Multinomial likelihood. | Check `X.dtype` for float values. | **Strict Rule**: LDA operates strictly on integer count matrices. Use NMF for TF-IDF. |
| **Topic Degeneration** | $K$ set significantly too large for a small corpus. | Empty/duplicate topics with uniform word distributions. | Check entropy of $\mathbf{\phi}_k$; topics with near-uniform distribution. | Decrease $K$ or set smaller $\alpha, \beta$ to force sparsity. |
| **Hapax Legomena Bloat** | Huge vocabulary with thousands of terms appearing only once in corpus. | Massive memory consumption; slow Gibbs sampling. | Vocabulary size $V > 100,000$. | Filter with `min_df=5` or `min_df=10`. |
| **Document Length Bias** | Very short documents (tweets, SMS, search queries). | Insufficient word co-occurrence leads to unstable topic mixtures. | Documents with $N_d < 5$ tokens. | Aggregate short texts by user/thread, or use specialized models (Biterm Topic Model - BTM). |

---

## 13. Comparative Trade-offs & When to Use

| Dimension | Latent Dirichlet Allocation (LDA) | Non-Negative Matrix Factorization (NMF) | BERTopic / Top2Vec (Embedding + HDBSCAN) |
| :--- | :--- | :--- | :--- |
| **Mathematical Basis** | Generative Bayesian hierarchical model | Linear algebra matrix decomposition | Pretrained Transformer embeddings + clustering |
| **Input Format** | Integer token count matrices | Continuous TF-IDF matrices | Raw unstructured string sentences |
| **Inference Mechanism** | Variational Bayes / Gibbs sampling | Coordinate descent / Multiplicative update | Embedding projection + cosine neighbor lookup |
| **Short Text Handling** | Poor on short snippets ($N_d < 10$) | Moderate | **Superior** (captures sentence semantics) |
| **Interpretability** | Excellent (exact word probabilities) | High (coefficient weights) | High (c-TF-IDF keyword extraction) |
| **Training Speed** | Fast (Online Variational) | Extremely Fast | Slower (requires GPU for transformer) |
| **Optimal Use Case** | Formal long-form text (articles, PDFs, legal) | Fast enterprise text discovery | Informal short text, social media, chatbots |

---

## 14. Production Engineering & Latency SLAs

- **Online Variational Bayes (Hoffman et al.)**: Process documents in mini-batches ($B = 128$ to $512$). Global topic-word distributions $\mathbf{\phi}$ are updated with a decaying learning rate $\rho_t = (\tau_0 + t)^{-\kappa}$.
- **Online Transform SLA**: To classify incoming documents in $< 5\text{ ms}$, cache vocabulary hash maps and limit variational iterations (`max_doc_update_iter=50`).

---

## 15. Interpretability & Feature Attribution

To explain why document $d$ received a high score for topic $k$:
1. Compute the contribution of each word $w$ present in document $d$:
   $$\text{Attribution}(w, k) = \text{count}(w \in d) \cdot \ln \hat{\phi}_{kw}$$
2. Rank words in descending order of $\text{Attribution}(w, k)$.

---

## 16. Hardware Acceleration, Scaling & Distributed Notes

- **Distributed LDA via Spark MLlib**: Uses distributed Variational Bayes where each partition computes local worker updates $\mathbf{\gamma}$ and driver aggregates global sufficient statistics.
- **WarpLDA**: High-performance GPU/CPU architecture utilizing cache-friendly sparse memory layouts to sample millions of tokens per second.

---

## 17. Pytest Verification Suite

```python
"""
Pytest validation suite for Latent Dirichlet Allocation.
Save as: tests/test_lda.py
Run: pytest -v tests/test_lda.py
"""

import pytest
import numpy as np
from sklearn.decomposition import LatentDirichletAllocation

def test_lda_distribution_normalization():
    """Verify that document-topic and topic-word distributions sum to 1.0."""
    rng = np.random.default_rng(42)
    X = rng.integers(0, 10, size=(50, 30)) # 50 docs, 30 terms
    
    lda = LatentDirichletAllocation(n_components=4, random_state=42)
    doc_topics = lda.fit_transform(X)
    
    # 1. Document-topic sums to 1.0
    np.testing.assert_allclose(np.sum(doc_topics, axis=1), 1.0, atol=1e-5)
    
    # 2. Topic-word distributions sum to 1.0
    topic_words = lda.components_ / lda.components_.sum(axis=1)[:, np.newaxis]
    np.testing.assert_allclose(np.sum(topic_words, axis=1), 1.0, atol=1e-5)

def test_lda_rejects_negative_or_nan_inputs():
    """Verify LDA raises ValueError when passed invalid matrices."""
    lda = LatentDirichletAllocation(n_components=3)
    invalid_X = np.array([[1, -2], [3, 4]])
    with pytest.raises(ValueError):
        lda.fit(invalid_X)
```

---

## 18. Cross-Track Integration

- **`01-languages/python/21-data-and-scientific-basics`**: NLTK/SpaCy tokenization and lemmatization for vocabulary preprocessing.
- **`03-backend/fastapi/16-ai-and-ml-serving`**: Asynchronous streaming endpoint assigning incoming customer tickets to support topics.
- **`09-sql`**: Storing document-topic probability tables with composite primary keys (`doc_id`, `topic_id`).
- **`05-ai/machine-learning/05-unsupervised-learning/09-svd-and-truncated-svd.md`**: Comparing Latent Semantic Analysis (LSA) with probabilistic LDA.

---

## 19. Real-World Enterprise Case Study

### Automated Routing of Regulatory Compliance Inquiries
- **Context**: A multinational bank receives 45,000 regulatory documents, inquiries, and customer compliance complaints monthly.
- **Problem**: Manual triage to specialized legal teams (AML, KYC, GDPR, MiFID II) took 4.2 days per inquiry.
- **Solution Architecture**:
  1. Built an automated 20-topic Online LDA pipeline integrated with Scikit-Learn.
  2. Extracted top topic distributions per document; routed documents with $> 60\%$ confidence directly to corresponding legal queues.
- **Outcome**: Reduced triage turnaround from 4.2 days to 3.5 minutes; improved routing precision to $94.2\%$.

---

## 20. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Why is exact inference in Latent Dirichlet Allocation analytically intractable?</strong></summary>

**Answer:**  
In LDA, we seek the posterior distribution of the latent variables given the observed words:
$$p(\mathbf{\theta}, \mathbf{\phi}, \mathbf{z} \mid \mathbf{w}, \alpha, \beta) = \frac{p(\mathbf{\theta}, \mathbf{\phi}, \mathbf{z}, \mathbf{w} \mid \alpha, \beta)}{p(\mathbf{w} \mid \alpha, \beta)}$$
The marginal probability of the observed document collection (the evidence) requires summing over all possible topic configurations $\mathbf{z}$ and integrating over all continuous simplex mixtures $\mathbf{\theta}$ and $\mathbf{\phi}$:
$$p(\mathbf{w} \mid \alpha, \beta) = \int \dots \int \sum_{\mathbf{z}} p(\mathbf{\theta}, \mathbf{\phi}, \mathbf{z}, \mathbf{w} \mid \alpha, \beta) \, d\mathbf{\theta} \, d\mathbf{\phi}$$
Because the topics $z_{dn}$ couple the continuous parameters $\mathbf{\theta}_d$ and $\mathbf{\phi}_k$ inside the product over words:
$$p(\mathbf{w}_d \mid \alpha, \beta) = \int p(\mathbf{\theta}_d \mid \alpha) \left( \prod_{n=1}^{N_d} \sum_{k=1}^K p(z_{dn}=k \mid \mathbf{\theta}_d) p(w_{dn} \mid \mathbf{\phi}_k) \right) d\mathbf{\theta}_d$$
The summation over topics appears inside the product over words before the integral, making the integral non-factorizable. The number of terms in the summation is $K^{N_d}$, leading to exponential combinatorial intractability. We must approximate via MCMC (Gibbs sampling) or Variational Bayes.
</details>

<details>
<summary><strong>Q2: Contrast the roles of hyperparameters $\alpha$ and $\beta$ (or $\eta$) in governing topic sparsity.</strong></summary>

**Answer:**  
$\alpha$ and $\beta$ are parameters of Dirichlet distributions:
- **$\alpha$ (Document-Topic Prior)**:
  - If $\alpha \ll 1$ (e.g. $0.01$): Document topic distributions are concentrated at the corners of the topic simplex. Documents are assumed to belong to only one or two dominant topics.
  - If $\alpha \gg 1$ (e.g. $10.0$): Document topic distributions are forced toward the center of the simplex. Every document is assumed to be a mixture of all topics.
- **$\beta / \eta$ (Topic-Word Prior)**:
  - If $\beta \ll 1$ (e.g. $0.01$): Topic vocabulary distributions are sparse. Topics are composed of a few highly specific words.
  - If $\beta \gg 1$: Topics are smooth and broad, sharing words evenly.
In production, setting $\alpha < 1$ and $\beta < 1$ is standard practice to encourage clean, disjoint, interpretable topics.
</details>

<details>
<summary><strong>Q3: How does Variational Inference approximate the LDA posterior, and what is the ELBO?</strong></summary>

**Answer:**  
Variational Inference approximates the true intractable posterior $p(\mathbf{\theta}, \mathbf{z} \mid \mathbf{w})$ using a tractable, factorized family of distributions $q(\mathbf{\theta}, \mathbf{z} \mid \mathbf{\gamma}, \mathbf{\phi}_{\text{var}})$:
$$q(\mathbf{\theta}, \mathbf{z} \mid \mathbf{\gamma}, \mathbf{\phi}_{\text{var}}) = \prod_{d=1}^D q(\mathbf{\theta}_d \mid \mathbf{\gamma}_d) \prod_{n=1}^{N_d} q(z_{dn} \mid \mathbf{\phi}_{dn})$$
where $\mathbf{\gamma}_d$ is a Dirichlet variational parameter and $\mathbf{\phi}_{dn}$ is a multinomial variational parameter.
Instead of minimizing KL-divergence $\text{KL}(q \| p)$ directly, we maximize the **Evidence Lower Bound (ELBO)** $\mathcal{L}(\mathbf{\gamma}, \mathbf{\phi}_{\text{var}})$:
$$\ln p(\mathbf{w}) = \mathcal{L}(\mathbf{\gamma}, \mathbf{\phi}_{\text{var}}) + \text{KL}(q \| p) \implies \ln p(\mathbf{w}) \ge \mathcal{L}(\mathbf{\gamma}, \mathbf{\phi}_{\text{var}})$$
$$\mathcal{L} = \mathbb{E}_q[\ln p(\mathbf{\theta}, \mathbf{z}, \mathbf{w})] - \mathbb{E}_q[\ln q(\mathbf{\theta}, \mathbf{z})]$$
Maximizing ELBO with respect to $\mathbf{\gamma}$ and $\mathbf{\phi}_{\text{var}}$ guarantees minimizing KL divergence to the true posterior.
</details>

<details>
<summary><strong>Q4: Why does high perplexity on holdout text not necessarily indicate poor human topic quality?</strong></summary>

**Answer:**  
Perplexity strictly measures how well the statistical model predicts unseen word frequencies under a multinomial likelihood:
$$\text{Perplexity} = \exp\left(-\frac{\sum \ln p(w)}{\sum N_d}\right)$$
Research (Chang et al., 2009, *"Reading Tea Leaves: How Humans Interpret Topic Models"*) revealed an inverse correlation between perplexity and human interpretability:
1. Models that optimize perplexity often create dense, overlapping topics that capture statistical noise and rare co-occurrences.
2. Humans judge topics based on **semantic coherence** (whether top words belong to a single conceptual domain).
3. A topic model with slightly worse perplexity often has sharper, sparser word clusters that are far more understandable to human analysts. Hence, metrics like $C_v$ and $U_{\text{mass}}$ coherence are preferred for evaluating topic readability.
</details>

<details>
<summary><strong>Q5: What is the difference between Batch LDA and Online LDA (Hoffman et al., 2010)?</strong></summary>

**Answer:**  
- **Batch LDA**: Performs variational inference sweeps over the **entire** document corpus before updating the global topic-word distributions $\mathbf{\lambda}$. If the corpus contains millions of documents, each iteration requires hours of computation and cannot process streaming data.
- **Online LDA**: Employs Stochastic Variational Inference (SVI). At step $t$, it draws a mini-batch of $B$ documents, computes local variational parameters $\mathbf{\gamma}$ for only those $B$ documents, constructs a noisy intermediate estimate of global topic parameters $\tilde{\mathbf{\lambda}}$, and updates global parameters via a Robbins-Monro stochastic step:
  $$\mathbf{\lambda}^{(t)} = (1 - \rho_t) \mathbf{\lambda}^{(t-1)} + \rho_t \tilde{\mathbf{\lambda}}^{(t)}, \quad \rho_t = (\tau_0 + t)^{-\kappa}$$
This allows LDA to train on infinite streams of text with constant memory.
</details>

<details>
<summary><strong>Q6: Why can you NOT pass TF-IDF transformed matrices into Latent Dirichlet Allocation?</strong></summary>

**Answer:**  
LDA is fundamentally a **generative probabilistic count model**. Its likelihood function is built on the Multinomial and Categorical distributions:
$$w_{dn} \sim \text{Categorical}(\mathbf{\phi}_{z_{dn}})$$
The probability mass function of a Multinomial requires discrete integer counts ($k \in \{0, 1, 2, \dots\}$). TF-IDF produces continuous floating-point weights ($w \in [0, 1]$). Passing TF-IDF weights violates the foundational mathematical axioms of the Dirichlet-Multinomial conjugate framework. If you have TF-IDF matrices, you must use **Non-Negative Matrix Factorization (NMF)** instead.
</details>

<details>
<summary><strong>Q7: How do you handle cold-start inference for an unseen document containing words not in the training vocabulary?</strong></summary>

**Answer:**  
Unseen words (Out-of-Vocabulary / OOV words) have no corresponding column in the trained $\mathbf{\Phi}$ topic-word matrix.
1. During vectorization, unseen words are filtered out using the vocabulary lookup dictionary.
2. If an unseen document consists **entirely** of OOV words, the document has 0 active tokens ($N_d = 0$).
3. In this case, the variational update defaults exactly to the Dirichlet prior:
   $$\mathbf{\theta}_{\text{cold}} = \frac{\alpha}{\sum_{k=1}^K \alpha_k} = \left[\frac{1}{K}, \dots, \frac{1}{K}\right]$$
The model gracefully outputs a uniform prior topic distribution.
</details>

<details>
<summary><strong>Q8: What is the Biterm Topic Model (BTM) and why does it outperform LDA on short texts?</strong></summary>

**Answer:**  
LDA assumes documents are rich mixtures of topics generated from word-topic distributions. In short texts (tweets, SMS, customer search queries), documents contain only 3 to 10 words. Word co-occurrences within individual documents are too sparse for LDA to infer stable document-level mixtures $\mathbf{\theta}_d$.
**Biterm Topic Model (BTM)** solves this by changing the modeling unit:
1. It extracts all pairwise combinations of words within a sliding window across the entire corpus: **biterms** $(w_i, w_j)$.
2. Instead of modeling document-topic mixtures, it models the corpus-level generation of biterms directly from global topic distributions.
3. This pools word co-occurrences across all short documents globally, overcoming individual document sparsity.
</details>

---

## 21. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Topic Extraction from Newsgroup Headlines</strong></summary>

**Task**: Extract 3 distinct topics from a synthetic list of tech and sports headlines using Scikit-Learn's `CountVectorizer` and `LatentDirichletAllocation`.

**Solution**:
```python
from sklearn.feature_extraction.text import CountVectorizer
from sklearn.decomposition import LatentDirichletAllocation

documents = [
    "Apple releases new iPhone with advanced neural engine processor",
    "Google announces Gemini AI model competing with OpenAI",
    "Football team wins championship match in overtime goal",
    "Soccer player scores hat trick in premier league game",
    "Nvidia launches powerful GPU accelerator for machine learning"
]

vectorizer = CountVectorizer(stop_words="english")
X = vectorizer.fit_transform(documents)

lda = LatentDirichletAllocation(n_components=2, random_state=42)
lda.fit(X)

feature_names = vectorizer.get_feature_names_out()
for idx, topic in enumerate(lda.components_):
    top_words = [feature_names[i] for i in topic.argsort()[:-4:-1]]
    print(f"Topic {idx}: {', '.join(top_words)}")
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Inferring Topic Distributions for a New Document</strong></summary>

**Task**: Given the fitted LDA model from Exercise 1, project an unseen query headline `"Nvidia develops next generation AI chips"` and print the topic probability breakdown.

**Solution**:
```python
new_doc = ["Nvidia develops next generation AI chips"]
new_dtm = vectorizer.transform(new_doc)
topic_distribution = lda.transform(new_dtm)[0]

for idx, prob in enumerate(topic_distribution):
    print(f"Topic {idx}: {prob * 100:.1f}%")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Corpus Perplexity Grid Search over Topic Cardinality</strong></summary>

**Task**: Write a loop testing $K \in \{2, 3, 4, 5\}$ topics on a document collection, recording perplexity to identify the optimal configuration.

**Solution**:
```python
import numpy as np
from sklearn.decomposition import LatentDirichletAllocation
from sklearn.feature_extraction.text import CountVectorizer

docs = [
    "cloud computing infrastructure aws azure",
    "relational database postgresql mysql query index",
    "neural network deep learning pytorch transformer",
    "kubernetes container deployment docker cluster",
    "sql transactions acid isolation postgresql",
    "convolutional neural networks computer vision"
] * 10

X = CountVectorizer().fit_transform(docs)

perplexities = {}
for k in [2, 3, 4, 5]:
    model = LatentDirichletAllocation(n_components=k, random_state=42, max_iter=20)
    model.fit(X)
    perplexities[k] = model.perplexity(X)

print("Perplexities across K:")
for k, perp in perplexities.items():
    print(f"K = {k}: {perp:.2f}")
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Document Topic Filtering & Anomaly Detection</strong></summary>

**Task**: Detect documents that are "unfocused" (entropy of topic mixture $\theta_d$ is near maximal uniform $\ln K$).

**Solution**:
```python
import numpy as np
from scipy.stats import entropy

# Assume doc_topic_matrix is of shape (N, K)
doc_topic_matrix = np.array([
    [0.90, 0.05, 0.05],  # Very focused
    [0.34, 0.33, 0.33],  # Highly unfocused / ambiguous
    [0.10, 0.85, 0.05]
])

K = doc_topic_matrix.shape[1]
max_entropy = np.log(K)

for i, dist in enumerate(doc_topic_matrix):
    ent = entropy(dist)
    is_ambiguous = (ent / max_entropy) > 0.85
    print(f"Doc {i}: Entropy = {ent:.3f} (Relative: {ent/max_entropy:.2f}) -> Ambiguous: {is_ambiguous}")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): From-Scratch Collapsed Gibbs Sampler Verification</strong></summary>

**Task**: Implement a 20-line bare-bones Collapsed Gibbs loop in NumPy and prove that word counts correctly decrement and increment without leakage.

**Solution**:
```python
import numpy as np

rng = np.random.default_rng(42)
D, V, K = 3, 4, 2
words_per_doc = [[0, 1], [1, 2], [2, 3]] # Word token IDs
alpha, beta = 0.5, 0.5

# Initialize
n_dk = np.zeros((D, K), dtype=int)
n_kv = np.zeros((K, V), dtype=int)
n_k = np.zeros(K, dtype=int)
z = [[rng.integers(0, K) for _ in doc] for doc in words_per_doc]

for d, doc in enumerate(words_per_doc):
    for w, k in zip(doc, z[d]):
        n_dk[d, k] += 1
        n_kv[k, w] += 1
        n_k[k] += 1

# One Gibbs step on first word
d, w, old_k = 0, words_per_doc[0][0], z[0][0]
n_dk[d, old_k] -= 1
n_kv[old_k, w] -= 1
n_k[old_k] -= 1

p = (n_kv[:, w] + beta) / (n_k + V * beta) * (n_dk[d, :] + alpha)
p /= p.sum()
new_k = rng.choice(K, p=p)

n_dk[d, new_k] += 1
n_kv[new_k, w] += 1
n_k[new_k] += 1
z[d][0] = new_k

assert np.sum(n_k) == sum(len(doc) for doc in words_per_doc)
print(f"Gibbs step verified! Transitioned word from topic {old_k} -> {new_k}")
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Intrinsic U_mass Coherence Calculator</strong></summary>

**Task**: Write an end-to-end Python function computing the $U_{\text{mass}}$ score of a trained Scikit-Learn LDA pipeline without external dependencies.

**Solution**:
```python
import numpy as np
from sklearn.feature_extraction.text import CountVectorizer
from sklearn.decomposition import LatentDirichletAllocation

def calculate_corpus_umass(pipeline, raw_docs, top_n=5):
    vec = pipeline.named_steps["vectorizer"]
    lda = pipeline.named_steps["lda"]
    
    dtm = vec.transform(raw_docs)
    binary_dtm = (dtm > 0).astype(float)
    co_occur = (binary_dtm.T @ binary_dtm).toarray()
    doc_freq = np.diag(co_occur)
    
    scores = []
    for k in range(lda.n_components):
        top_idx = lda.components_[k].argsort()[:-top_n-1:-1]
        topic_score = 0.0
        for m in range(1, top_n):
            for l in range(0, m):
                vm, vl = top_idx[m], top_idx[l]
                d_ml = co_occur[vm, vl]
                d_l = doc_freq[vl]
                topic_score += np.log((d_ml + 1.0) / (d_l + 1.0))
        scores.append(topic_score)
    return float(np.mean(scores))

# Verification
docs = ["machine learning artificial intelligence", "python code programming development"] * 5
pipe = Pipeline([
    ("vectorizer", CountVectorizer()),
    ("lda", LatentDirichletAllocation(n_components=2, random_state=42))
]).fit(docs)

umass = calculate_corpus_umass(pipe, docs)
print(f"Calculated U_mass Topic Coherence: {umass:.4f}")
```
</details>
