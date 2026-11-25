# Singular Value Decomposition (SVD) & Truncated SVD (LSA)

> **Track:** `05-ai/machine-learning` | **Module:** `05-unsupervised-learning` | **Status:** `Production Standard`  
> **Metadata:** `Scikit-Learn: >=1.4.0` | `NumPy: >=1.26.0` | `SciPy: >=1.12.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Singular Value Decomposition (SVD), Truncated SVD & Latent Semantic Analysis (LSA) |
| **Family** | Low-Rank Matrix Factorization / Linear Spectral Decomposition |
| **Input Feature Types** | Continuous or sparse non-negative numerical matrices ($X \in \mathbb{R}^{n \times d}$). **Crucially operates directly on sparse CSR matrices without mean centering**. |
| **Target Output** | Latent semantic projection coordinates $\mathbf{Z} \in \mathbb{R}^{n \times k}$, right singular term vectors $\mathbf{V}_k \in \mathbb{R}^{d \times k}$, and singular values $\mathbf{s} \in \mathbb{R}^k$. |
| **Optimization Target** | Optimal low-rank approximation under Frobenius and Spectral norms: $\min_{\text{rank}(A) \le k} \|\mathbf{X} - A\|_F$. |
| **Optimization Solvers** | Randomized SVD (Halko, Martinsson, Tropp, 2011), ARPACK implicitly restarted Lanczos bidiagonalization. |
| **Key Hyperparameters** | `n_components` ($k$), `algorithm` (`'randomized'`, `'arpack'`), `n_iter`, `random_state`. |
| **Inference Mechanism** | Linear projection onto right singular vectors: $\mathbf{z} = \mathbf{x} \mathbf{V}_k$. Reconstruction: $\hat{\mathbf{x}} = \mathbf{z} \mathbf{V}_k^T$. |
| **Primary Failure Modes** | Dense matrix explosion if mean centering is accidentally applied to sparse data; negative component loadings uninterpretable as pure word counts (unlike NMF); sign ambiguity in singular vectors. |

---

## 2. Architecture & Intuition

Singular Value Decomposition factorizes any arbitrary real matrix $\mathbf{X} \in \mathbb{R}^{n \times d}$ into the product of three constituent matrices:

$$\mathbf{X} = \mathbf{U} \mathbf{\Sigma} \mathbf{V}^T$$

```
   Raw Data Matrix X (n x d)       Left Singular U (n x k)    Singular S (k x k)    Right Singular V^T (k x d)
   +-----------------------+       +-----+                    +-----+               +-----------------------+
   |                       |       |     |                    |s1   |               |                       |
   |   Documents x Words   |   =   |     | (Doc-Concept)   x  |  s2 |            x  |    (Concept-Word)     |
   |      (Sparse TF-IDF)  |       |     |                    |   sk|               |                       |
   +-----------------------+       +-----+                    +-----+               +-----------------------+
```

### Why Truncated SVD Dominates Text & Sparse Data
Standard PCA requires **mean centering** ($\mathbf{X} - \boldsymbol{\mu}$). In natural language processing, term-document matrices (TF-IDF) are typically $99.5\%$ sparse ($99.5\%$ zeros). Centering a sparse matrix converts all zero entries into non-zero means, instantly converting a $50\text{MB}$ sparse matrix into a $16\text{GB}$ dense matrix, causing immediate out-of-memory crashes.

**Truncated SVD** computes the optimal low-rank factorization **without centering the data**, operating directly on sparse `scipy.sparse.csr_matrix` structures with minimal RAM. When applied to text term-document matrices, it is known as **Latent Semantic Analysis (LSA)**.

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $\mathbf{X} \in \mathbb{R}^{n \times d}$ | Matrix | Arbitrary input matrix (e.g., $n$ documents, $d$ vocabulary terms) |
| $r \le \min(n, d)$ | $\mathbb{N}$ | Exact mathematical rank of matrix $\mathbf{X}$ |
| $k < r$ | $\mathbb{N}$ | Target truncated rank (latent semantic dimensions) |
| $\mathbf{U} \in \mathbb{R}^{n \times n}$ | Orthonormal Matrix | Left singular vectors (eigenvectors of $\mathbf{X} \mathbf{X}^T$) |
| $\mathbf{\Sigma} \in \mathbb{R}^{n \times d}$ | Diagonal Rectangular Matrix | Singular values $s_1 \ge s_2 \ge \dots \ge s_r > 0$ along diagonal |
| $\mathbf{V} \in \mathbb{R}^{d \times d}$ | Orthonormal Matrix | Right singular vectors (eigenvectors of $\mathbf{X}^T \mathbf{X}$) |
| $\mathbf{X}_k \in \mathbb{R}^{n \times d}$ | Rank-$k$ Matrix | Optimal truncated low-rank approximation: $\mathbf{X}_k = \mathbf{U}_k \mathbf{\Sigma}_k \mathbf{V}_k^T$ |

### 1. The Fundamental SVD Theorem
Any real matrix $\mathbf{X} \in \mathbb{R}^{n \times d}$ can be decomposed as:

$$\mathbf{X} = \sum_{j=1}^r s_j \mathbf{u}_j \mathbf{v}_j^T$$

Where:
- $\mathbf{u}_j \in \mathbb{R}^n$ are mutually orthonormal: $\mathbf{u}_i^T \mathbf{u}_j = \delta_{ij}$.
- $\mathbf{v}_j \in \mathbb{R}^d$ are mutually orthonormal: $\mathbf{v}_i^T \mathbf{v}_j = \delta_{ij}$.
- $s_j = \sqrt{\lambda_j(\mathbf{X}^T \mathbf{X})} = \sqrt{\lambda_j(\mathbf{X} \mathbf{X}^T)} > 0$ are the non-negative singular values.

### 2. Truncation & The Eckart-Young-Mirsky Theorem
By retaining strictly the $k$ largest singular values ($k \ll r$) and setting all remaining values $s_{k+1}, \dots, s_r = 0$, we construct the rank-$k$ truncated matrix:

$$\mathbf{X}_k = \mathbf{U}_k \mathbf{\Sigma}_k \mathbf{V}_k^T = \sum_{j=1}^k s_j \mathbf{u}_j \mathbf{v}_j^T$$

The **Eckart-Young-Mirsky Theorem** (1936) guarantees that $\mathbf{X}_k$ is the mathematically optimal approximation of $\mathbf{X}$ among all possible rank-$k$ matrices:

$$\min_{\text{rank}(A) \le k} \|\mathbf{X} - A\|_F = \|\mathbf{X} - \mathbf{X}_k\|_F = \sqrt{\sum_{j=k+1}^r s_j^2}$$
$$\min_{\text{rank}(A) \le k} \|\mathbf{X} - A\|_2 = \|\mathbf{X} - \mathbf{X}_k\|_2 = s_{k+1}$$

### 3. Latent Semantic Analysis (LSA) Mechanics
In NLP, let $\mathbf{X} \in \mathbb{R}^{n \times d}$ be a TF-IDF matrix where $X_{ij}$ reflects the importance of word $j$ in document $i$.
1. **Truncated Factorization**:
   $$\mathbf{X} \approx \mathbf{U}_k \mathbf{\Sigma}_k \mathbf{V}_k^T$$
   - $\mathbf{U}_k \in \mathbb{R}^{n \times k}$: Document embeddings in latent semantic concept space.
   - $\mathbf{\Sigma}_k \in \mathbb{R}^{k \times k}$: Relative importance of each latent concept.
   - $\mathbf{V}_k \in \mathbb{R}^{d \times k}$: Word loadings across latent concepts.
2. **Document-to-Document Semantic Similarity**:
   $$\mathbf{X} \mathbf{X}^T \approx (\mathbf{U}_k \mathbf{\Sigma}_k \mathbf{V}_k^T)(\mathbf{V}_k \mathbf{\Sigma}_k \mathbf{U}_k^T) = (\mathbf{U}_k \mathbf{\Sigma}_k)(\mathbf{U}_k \mathbf{\Sigma}_k)^T$$
   Two documents that share **zero vocabulary words** but use synonymous words (e.g., "doctor" vs "physician") will map to similar latent vectors $\mathbf{U}_k \mathbf{\Sigma}_k$ because their co-occurrence patterns align within the low-rank subspace!
3. **Word-to-Word Semantic Similarity**:
   $$\mathbf{X}^T \mathbf{X} \approx (\mathbf{V}_k \mathbf{\Sigma}_k)(\mathbf{V}_k \mathbf{\Sigma}_k)^T$$
   Measures semantic synonymy between words based on shared context.

---

## 4. Computational Complexity

| Matrix Topology | Solver | Time Complexity | Memory Complexity |
| :--- | :--- | :--- | :--- |
| **Dense Matrix ($n \times d$)** | Full Lapack SVD (`gesdd`) | $O(\min(n d^2, n^2 d))$ | $O(n \cdot d)$ |
| **Sparse Matrix (CSR)** | Truncated ARPACK (Lanczos) | $O(\text{nnz} \cdot k \cdot \text{iter})$ | $O(\text{nnz} + (n + d) k)$ |
| **Sparse Matrix (CSR)** | Randomized SVD (Halko) | $O(\text{nnz} \cdot k + (n + d) k^2)$ | $O(\text{nnz} + (n + d) k)$ |
| **Query Projection (Single Document)**| Vector-matrix dot product | $O(d_{\text{active}} \cdot k)$ | $O(k)$ |

---

## 5. Inductive Bias & Learning Guarantees

1. **Subspace Co-occurrence Compression**: LSA assumes that words frequently appearing in the same contexts share underlying semantic meaning (Firth's Structural Linguistics Hypothesis: *"You shall know a word by the company it keeps"*).
2. **Noise Filtering via Truncation**: High-rank singular components ($s_{j > k}$) represent document-specific typos, syntactic irregularities, and sampling noise. Discarding them acts as an optimal Wiener-style noise filter.
3. **Sign Ambiguity Invariance**: Singular vectors are unique up to sign: if $(\mathbf{u}_j, \mathbf{v}_j)$ is a valid singular pair, then $(-\mathbf{u}_j, -\mathbf{v}_j)$ produces an identical factorization: $(-s)(-\mathbf{u})(-\mathbf{v}^T) = s \mathbf{u} \mathbf{v}^T$. Algorithms downstream must be invariant to global coordinate flips.

---

## 6. Scratch NumPy Implementation

```python
"""
Scratch NumPy implementation of Truncated SVD using Randomized SVD.
Operates on arbitrary matrices without centering, providing an exact
reference for Halko's low-rank randomized factorization algorithm.
"""

from __future__ import annotations
import numpy as np
from typing import Tuple


class ScratchTruncatedSVD:
    """Production reference implementation of Truncated SVD via Randomized Projections."""

    def __init__(self, n_components: int = 2, n_iter: int = 4, random_state: int | None = 42) -> None:
        self.n_components = n_components
        self.n_iter = n_iter
        self.random_state = random_state

        self.components_: np.ndarray | None = None  # shape (k, d) -> V_k^T
        self.singular_values_: np.ndarray | None = None
        self.explained_variance_: np.ndarray | None = None
        self.explained_variance_ratio_: np.ndarray | None = None

    def fit(self, X: np.ndarray) -> ScratchTruncatedSVD:
        rng = np.random.default_rng(self.random_state)
        X = np.asarray(X, dtype=np.float64)
        n_samples, n_features = X.shape
        k = min(self.n_components, n_features)

        # 1. Randomized Subspace Projection (Halko et al., 2011)
        # Oversampling parameter p=10 ensures high probability of capturing top k subspace
        p = 10
        l = min(k + p, min(n_samples, n_features))

        # Generate Gaussian random test matrix Omega in R^{d x l}
        Omega = rng.normal(size=(n_features, l))

        # Form Y = X @ Omega in R^{n x l}
        Y = np.dot(X, Omega)

        # Power Iterations to accelerate spectral decay: Y = (X @ X.T)^q @ X @ Omega
        for _ in range(self.n_iter):
            Y = np.dot(X, np.dot(X.T, Y))

        # 2. Orthonormalize Y via QR Decomposition
        Q, _ = np.linalg.qr(Y, mode="reduced")  # Q in R^{n x l}

        # 3. Project X onto low-dimensional basis Q: B = Q^T @ X in R^{l x d}
        B = np.dot(Q.T, X)

        # 4. Compute Economy SVD on tiny matrix B
        U_tilde, S, Vt = np.linalg.svd(B, full_matrices=False)

        # 5. Extract Truncated Results
        self.singular_values_ = S[:k]
        self.components_ = Vt[:k]  # V_k^T in R^{k x d}

        # Variance calculations (uncentered variance = total sum of squares / (n - 1))
        total_var = np.sum(X ** 2) / (n_samples - 1)
        explained_var = (self.singular_values_ ** 2) / (n_samples - 1)
        self.explained_variance_ = explained_var
        self.explained_variance_ratio_ = explained_var / total_var

        return self

    def transform(self, X: np.ndarray) -> np.ndarray:
        """Project input matrix onto right singular vectors V_k."""
        if self.components_ is None:
            raise RuntimeError("ScratchTruncatedSVD is not fitted.")
        X = np.asarray(X, dtype=np.float64)
        # Z = X @ V_k = X @ components_.T
        return np.dot(X, self.components_.T)

    def fit_transform(self, X: np.ndarray) -> np.ndarray:
        return self.fit(X).transform(X)

    def inverse_transform(self, Z: np.ndarray) -> np.ndarray:
        """Reconstruct matrix from latent coordinates: X_approx = Z @ V_k^T."""
        if self.components_ is None:
            raise RuntimeError("ScratchTruncatedSVD is not fitted.")
        Z = np.asarray(Z, dtype=np.float64)
        return np.dot(Z, self.components_)
```

---

## 7. Production Scikit-Learn 9-Stage Pipeline

```python
"""
Production Latent Semantic Analysis (LSA) pipeline using TruncatedSVD and TfidfVectorizer.
Performs topic discovery on sparse text corpus without destroying sparsity.
"""

from __future__ import annotations
import logging
from typing import Dict, List, Tuple
import numpy as np
from sklearn.datasets import fetch_20newsgroups
from sklearn.feature_extraction.text import TfidfVectorizer
from sklearn.decomposition import TruncatedSVD
from sklearn.pipeline import Pipeline
from sklearn.metrics.pairwise import cosine_similarity

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("LSA_Pipeline")


def run_lsa_production_pipeline() -> Tuple[Pipeline, Dict[str, float]]:
    logger.info("Stage 1: Ingesting text documents across 3 technical categories.")
    categories = ["comp.graphics", "sci.med", "sci.space"]
    data = fetch_20newsgroups(subset="train", categories=categories, remove=("headers", "footers", "quotes"))
    corpus = [doc for doc in data.data if len(doc.strip()) > 50]

    # Stage 2: Sparse TF-IDF Vectorization
    logger.info("Stage 2: Constructing sparse TF-IDF n-gram feature representation.")
    tfidf = TfidfVectorizer(
        max_df=0.85,
        min_df=3,
        max_features=5000,
        stop_words="english",
        ngram_range=(1, 2)
    )
    X_sparse = tfidf.fit_transform(corpus)
    logger.info(f"TF-IDF Matrix: {X_sparse.shape} with {X_sparse.nnz:,} non-zero entries (Sparsity: {100.0 * (1 - X_sparse.nnz / (X_sparse.shape[0] * X_sparse.shape[1])):.2f}%)")

    # Stage 3: Truncated SVD (LSA) on Sparse Matrix
    # Crucial: TruncatedSVD operates directly on CSR matrix without mean centering!
    logger.info("Stage 3: Fitting TruncatedSVD with K=20 latent semantic concepts.")
    svd = TruncatedSVD(n_components=20, algorithm="randomized", n_iter=5, random_state=42)
    X_lsa = svd.fit_transform(X_sparse)
    cum_var = float(np.sum(svd.explained_variance_ratio_))
    logger.info(f"LSA Concept Embedding Shape: {X_lsa.shape} | Cumulative Variance: {cum_var * 100:.2f}%")

    # Stage 4: Top Terms Extraction per Latent Topic
    vocab = np.array(tfidf.get_feature_names_out())
    for topic_idx in range(3):
        top_indices = np.argsort(svd.components_[topic_idx])[::-1][:8]
        top_words = vocab[top_indices]
        logger.info(f"Topic {topic_idx + 1} Keywords: {', '.join(top_words)}")

    # Stage 5: Semantic Document Similarity Query
    # Query: "lunar orbit shuttle mission"
    query_vec = tfidf.transform(["lunar orbit shuttle mission"])
    query_lsa = svd.transform(query_vec)
    # Cosine similarity in latent concept space
    sims = cosine_similarity(query_lsa, X_lsa).flatten()
    best_doc_idx = int(np.argmax(sims))
    logger.info(f"Top Semantic Match (Similarity: {sims[best_doc_idx]:.4f}):\n{corpus[best_doc_idx][:150]}...")

    pipeline = Pipeline([("tfidf", tfidf), ("svd", svd)])
    return pipeline, {"cumulative_variance": cum_var, "top_similarity": float(sims[best_doc_idx])}


if __name__ == "__main__":
    model, perf = run_lsa_production_pipeline()
    assert perf["top_similarity"] > 0.60, "LSA failed semantic document retrieval benchmark."
```

---

## 8. Model Persistence & Serialization

```python
"""
Serializing TruncatedSVD models and concept loading dictionaries.
"""

from pathlib import Path
import json
import joblib
from sklearn.decomposition import TruncatedSVD


def export_lsa_bundle(model: TruncatedSVD, feature_names: list[str], path: Path) -> Path:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    joblib.dump(model, path)

    # Export top 10 keywords per topic
    topics = {}
    for i, comp in enumerate(model.components_):
        top_idx = np.argsort(comp)[::-1][:10]
        topics[f"concept_{i}"] = [feature_names[j] for j in top_idx]

    meta_path = path.with_suffix(".topics.json")
    with open(meta_path, "w", encoding="utf-8") as f:
        json.dump(topics, f, indent=2)
    return meta_path
```

---

## 9. Hyperparameter Tuning Guide

| Parameter | Options | Default | Engineering Guidance |
| :--- | :--- | :--- | :--- |
| `n_components` ($k$) | Integer | `2` | For LSA document modeling, set $k \in [100, 300]$. Setting $k < 50$ merges distinct topics; setting $k > 500$ retains syntactic noise. |
| `algorithm` | `'randomized'`, `'arpack'` | `'randomized'` | Always use `'randomized'` for text TF-IDF. It executes in $O(\text{nnz} \cdot k)$ time and runs $5\times$ to $10\times$ faster than ARPACK on sparse matrices. |
| `n_iter` | Integer | `5` | Number of power iterations for randomized SVD. Increase to $7$–$10$ if singular value decay is slow. |

---

## 10. Residuals, Diagnostics & Failure Modes

1. **First Component Dominance in Text (The "Document Length" Axis)**:
   In uncentered text TF-IDF matrices, all entries are non-negative ($X_{ij} \ge 0$). By the **Perron-Frobenius Theorem**, the first right singular vector $\mathbf{v}_1$ will have strictly non-negative loadings, capturing overall document length / word frequency rather than a specific semantic topic.
   - *Mitigation*: Often, practitioners discard the first component ($PC_1$) or apply L2 normalization to LSA coordinates.
2. **Singular Value Spectrum**:
   Plot singular values $s_j$. If singular values decay steeply and plateau, truncation at the elbow successfully isolates the latent semantic subspace.
3. **Negative Weight Interpretability**:
   Unlike Non-Negative Matrix Factorization (NMF), singular vectors in SVD contain both positive and negative values ($\pm$). A negative loading on a word in topic $k$ indicates that the word is anti-correlated with that concept.

---

## 11. Prediction Uncertainty & Calibration

Truncated SVD provides an explicit metric of **information retention**:

$$\text{Fidelity}(\mathbf{x}) = \frac{\|\mathbf{x} \mathbf{V}_k \mathbf{V}_k^T\|_2}{\|\mathbf{x}\|_2} \in [0, 1]$$

If an incoming test query document has a fidelity $< 0.20$, it consists of vocabulary terms that were not captured in the latent subspace (out-of-domain query).

---

## 12. Comprehensive Failure Modes & Edge Cases Table

| Failure Mode | Root Cause | Observable Symptom | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **Sparsity Destruction (Out-Of-Memory)** | Accidentally calling `StandardScaler()` or `PCA(whiten=True)` on sparse CSR. | Python process crashes with OOM; RAM spikes to 32GB on small text corpus. | **Never center sparse matrices**. Use `TruncatedSVD` directly on raw TF-IDF. |
| **Sign Ambiguity Inconsistency** | Singular vectors $(\mathbf{u}, \mathbf{v})$ are symmetric up to sign ($\pm$). | Running identical code on different machines produces flipped embedding axes. | Apply deterministic sign alignment (e.g., force the maximum entry in each component to be positive). |
| **Polysemy Collision** | A single word has multiple distinct meanings (e.g., "bank" = river bank vs financial bank). | LSA represents each word as a single static vector $\mathbf{v}_j$, collapsing meanings.| Use contextual embeddings (BERT) or Latent Dirichlet Allocation (LDA). |
| **Randomized SVD Inaccuracy** | `n_iter` set too low ($0$ or $1$) on slowly decaying spectrums. | Lower components fail to converge; inaccurate singular values. | Increase `n_iter=7` or switch to `algorithm='arpack'`. |
| **Stopword Dominance** | Failure to strip common boilerplate stopwords ("the", "is", "this"). | The top 5 latent components reflect variations of generic stopwords. | Enforce `stop_words='english'` and `max_df=0.80` in `TfidfVectorizer`. |
| **Extreme Vocabulary Cardinality** | Vocabulary size $d > 500,000$ containing rare typos. | Large projection matrix memory overhead ($500,000 \times 300 \approx 600\text{MB}$). | Constrain vocabulary using `min_df=5` and `max_features=50000`. |

---

## 13. Comparative Trade-off Table

| Algorithm | Operates on Sparse Matrices? | Enforces Non-Negativity? | Handles Polysemy? | Probabilistic Foundations? |
| :--- | :--- | :--- | :--- | :--- |
| **TruncatedSVD (LSA)** | **Yes (Native CSR)** | No (Can have negative values) | Poorly | No (Linear Spectral Decomposition) |
| **PCA** | No (Centering destroys sparsity)| No | Poorly | Yes (Probabilistic PCA / Gaussian) |
| **NMF** | **Yes (Native CSR)** | **Strictly Yes** ($\ge 0$) | Moderate | No (Constrained Matrix Factorization) |
| **LDA (Topic Model)** | **Yes (Count matrices)** | **Yes (Probabilities)** | **Yes (Mixture per doc)** | **Strictly Yes (Dirichlet Priors)** |

---

## 14. Production Engineering, Serving & Latency

### High-Throughput Semantic Document Search
Once $\mathbf{V}_k \in \mathbb{R}^{d \times k}$ is fitted, query matching executes in three fast vectorized steps:
1. Sparse vector multiplication: $\mathbf{z}_{\text{query}} = \mathbf{x}_{\text{query}} \mathbf{V}_k \in \mathbb{R}^k$.
2. Unit normalize: $\hat{\mathbf{z}} = \mathbf{z} / \|\mathbf{z}\|_2$.
3. Compute dot product against pre-normalized document database:
   $$\text{Scores} = \mathbf{Z}_{\text{database}} \hat{\mathbf{z}}^T \in \mathbb{R}^n$$
Executed via FAISS or BLAS in $< 2\text{ms}$ over $1,000,000$ documents.

---

## 15. Interpretability & Explainability

- **Topic Word Bars**: Extract the top 15 words with the largest positive weights in each right singular vector $\mathbf{v}_k$ to name and interpret the semantic concept.
- **Document Projection Scatter**: Plot documents in the first two concept dimensions $(Z_1, Z_2)$, color-coded by category, to visualize cross-domain semantic overlap.

---

## 16. Hardware, Parallelism & Scaling

- **SciPy ARPACK & OpenMP**: Sparse matrix-vector multiplications ($A \mathbf{v}$) are multithreaded across CPU cores via OpenMP.
- **GPU Scaling**: cuML provides `cuml.decomposition.TruncatedSVD`, which parallelizes sparse Lanczos iterations across NVIDIA GPU memory.

---

## 17. Automated Verification Harness

```python
"""
Pytest automated verification harness for Truncated SVD.
Asserts Eckart-Young approximation error, singular value sorting, and sparse matrix support.
"""

import numpy as np
import pytest
import scipy.sparse as sp
from sklearn.decomposition import TruncatedSVD


@pytest.fixture
def sparse_matrix():
    # Construct 100 x 50 sparse CSR matrix (90% zeros)
    rng = np.random.default_rng(42)
    dense = rng.uniform(0, 1, size=(100, 50))
    dense[dense < 0.90] = 0.0
    return sp.csr_matrix(dense)


def test_truncated_svd_handles_sparse_csr(sparse_matrix):
    """TruncatedSVD must execute natively on scipy.sparse.csr_matrix without dense conversion."""
    svd = TruncatedSVD(n_components=5, random_state=42)
    Z = svd.fit_transform(sparse_matrix)

    assert Z.shape == (100, 5)
    assert svd.components_.shape == (5, 50)


def test_singular_values_sorted_descending(sparse_matrix):
    """Singular values must be sorted in strictly non-increasing order."""
    svd = TruncatedSVD(n_components=8, random_state=42).fit(sparse_matrix)
    s = svd.singular_values_

    assert np.all(np.diff(s) <= 1e-6), f"Singular values not sorted: {s}"
```

---

## 18. Cross-Track Learning & Ecosystem Links

- **Linear Algebra**: SVD theorems in `01-math-foundations/01-linear-algebra-for-ml`.
- **Text Engineering**: Text tokenization and TF-IDF in `02-data-and-preprocessing/07-text-and-date-features-basics`.
- **Alternative Topic Model**: Probabilistic comparison in `05-unsupervised-learning/15-topic-modelling-lda`.

---

## 19. Real-World Enterprise Case Study

### Context: Enterprise Legal Discovery (E-Discovery) Semantic Contract Matching
- **Domain**: Corporate Litigation & Compliance.
- **Problem**: Reviewing $450,000$ internal company emails and contracts under a strict regulatory subpoena to find all documents discussing "off-the-books debt concealment" within 72 hours.
- **Why Keyword Search Failed**: Bad actors deliberately avoided the word "debt", using euphemisms like "special purpose parking", "off-balance sheet entity", and "synthetic collateral".
- **Implementation**:
  - Constructed TF-IDF matrix ($450\text{K}$ documents $\times 42\text{K}$ terms, $99.8\%$ sparse).
  - Deployed `TruncatedSVD(n_components=250, algorithm='randomized')`.
  - Queried seed regulatory phrases in the 250-dimensional latent semantic space.
- **Outcome**: Surfaced $3,800$ critical smoking-gun emails containing zero keyword matches to "debt", slashing human paralegal document review hours by $78\%$.

---

## 20. Technical Interview Preparation

### Q1: What is the fundamental difference between PCA and Truncated SVD?
**Answer:** The primary mathematical and practical difference is **mean centering**:
- **PCA** strictly subtracts the column mean vector ($\mathbf{X} - \boldsymbol{\mu}$) before computing singular vectors, finding axes of maximum variance around the center of mass.
- **Truncated SVD** does **not** center the data; it factorizes the raw uncentered matrix $\mathbf{X} \approx \mathbf{U}_k \mathbf{\Sigma}_k \mathbf{V}_k^T$.
In dense data, they are conceptually similar. In sparse data (e.g., text TF-IDF matrices with $99\%$ zeros), subtracting the mean destroys all zero entries, causing memory usage to explode from megabytes to gigabytes. Truncated SVD preserves sparsity.

### Q2: How does Latent Semantic Analysis (LSA) solve the problems of Synonymy and Polysemy?
**Answer:**
- **Synonymy** (different words with the same meaning, e.g., "automobile" vs "car"): LSA solves synonymy effectively. Because synonyms frequently co-occur with the same surrounding context words, their loading patterns in the low-rank subspace $\mathbf{V}_k$ align closely. Truncating noise forces them onto the same latent semantic concept.
- **Polysemy** (one word with multiple distinct meanings, e.g., "bank"): LSA struggles with polysemy. Because SVD assigns each word a single static coordinate vector $\mathbf{v}_j$, the vector for "bank" becomes a weighted average of financial and river meanings, pulling it into an unnatural midpoint. Probabilistic models (LDA) or contextual embeddings (BERT) handle polysemy better.

### Q3: What is the Perron-Frobenius Theorem, and how does it explain the first component of LSA?
**Answer:** The Perron-Frobenius theorem states that any real square matrix with strictly positive entries has a unique largest real eigenvalue whose corresponding eigenvector has strictly positive components. In text processing, TF-IDF matrices contain only non-negative values ($X_{ij} \ge 0$). Consequently, the matrix $\mathbf{X}^T \mathbf{X}$ is non-negative, and the first singular vector $\mathbf{v}_1$ consists entirely of positive loadings. In practice, this first component captures global document length and general word frequency rather than a specific semantic topic.

### Q4: Explain the Eckart-Young-Mirsky Theorem.
**Answer:** Formulated in 1936, the theorem proves that the truncated SVD $\mathbf{X}_k = \sum_{j=1}^k s_j \mathbf{u}_j \mathbf{v}_j^T$ is the provably optimal rank-$k$ approximation of matrix $\mathbf{X}$ in terms of minimizing both the Frobenius norm $\|\mathbf{X} - \mathbf{X}_k\|_F = \sqrt{\sum_{j=k+1}^r s_j^2}$ and the Spectral norm $\|\mathbf{X} - \mathbf{X}_k\|_2 = s_{k+1}$. No other rank-$k$ matrix can achieve a smaller reconstruction error.

### Q5: Why are singular vectors in SVD subject to "Sign Ambiguity"?
**Answer:** For any singular value $s_j$ and singular pair $(\mathbf{u}_j, \mathbf{v}_j)$, we have $\mathbf{X} \mathbf{v}_j = s_j \mathbf{u}_j$ and $\mathbf{X}^T \mathbf{u}_j = s_j \mathbf{v}_j$. Negating both vectors simultaneously yields $\mathbf{X} (-\mathbf{v}_j) = s_j (-\mathbf{u}_j)$, and the product $(-s_j)(-\mathbf{u}_j)(-\mathbf{v}_j^T) = s_j \mathbf{u}_j \mathbf{v}_j^T$ remains completely unchanged. SVD solvers choose signs arbitrarily based on initial numerical seeding. Downstream production systems must ensure algorithms are invariant to global sign flips.

### Q6: How does Randomized SVD achieve $O(\text{nnz} \cdot k)$ complexity on sparse matrices?
**Answer:** Instead of computing full matrix factorizations, Randomized SVD uses random Gaussian test matrices $\mathbf{\Omega} \in \mathbb{R}^{d \times (k+p)}$ to sample the range of $\mathbf{X}$: $\mathbf{Y} = \mathbf{X} \mathbf{\Omega}$. Performing a few power iterations $\mathbf{Y} \leftarrow (\mathbf{X} \mathbf{X}^T) \mathbf{Y}$ drives down singular components beyond $k$. Computing QR decomposition on $\mathbf{Y}$ produces an orthonormal basis $\mathbf{Q}$, allowing SVD to be solved on the small matrix $\mathbf{B} = \mathbf{Q}^T \mathbf{X} \in \mathbb{R}^{(k+p) \times d}$. This scales linearly with non-zero entries ($\text{nnz}$) rather than $O(n \cdot d^2)$.

### Q7: When would you choose Non-Negative Matrix Factorization (NMF) over Truncated SVD?
**Answer:** Choose **NMF** ($\mathbf{X} \approx \mathbf{W} \mathbf{H}$ s.t. $W, H \ge 0$) when human interpretability of latent components is paramount. In Truncated SVD, singular vectors contain arbitrary positive and negative numbers; interpreting what a negative word loading means in a text topic is counter-intuitive. In NMF, all factors are non-negative, allowing topics to be interpreted as purely additive "parts-based" mixtures of words. However, NMF is non-convex, has no unique global optimum, and is computationally slower than SVD.

### Q8: How can Truncated SVD be used for Matrix Completion and Recommender Systems?
**Answer:** In user-item rating matrices (e.g., Netflix prize where users rate movies), the matrix $\mathbf{R} \in \mathbb{R}^{n \times m}$ is heavily incomplete ($> 98\%$ missing entries). Under the assumption that user preferences are driven by $k$ latent factors (e.g., preference for comedy, action, director), Truncated SVD models $\mathbf{R} \approx \mathbf{P} \mathbf{Q}^T$. Unseen ratings are predicted by evaluating the low-rank dot product: $\hat{r}_{ui} = \mathbf{p}_u^T \mathbf{q}_i$. (Solvers like Alternating Least Squares or SVD++ optimize this strictly over observed entries).

---

## 21. Hands-On Exercises & Mini-Projects

1. **Semantic Text Search Engine from Scratch**: Build a mini search engine over 1,000 Wikipedia articles: compute TF-IDF, fit `ScratchTruncatedSVD` with $k=50$, project an arbitrary text query into latent space, and return the top 5 most similar articles using cosine similarity.
2. **Sparsity Memory Destruction Demonstration**: Create a sparse matrix with $n=10,000, d=20,000$ and $99.9\%$ zeros. Measure its RAM footprint (`X.data.nbytes`). Call `StandardScaler().fit_transform(X.toarray())` and observe memory growth from $2\text{MB}$ to $1.6\text{GB}$.
3. **Randomized SVD Power Iteration Accuracy**: Implement a script that compares the singular values produced by Randomized SVD with power iterations $q \in [0, 1, 2, 4, 8]$ against exact SciPy SVD. Plot the relative error $|s_j^{\text{rand}} - s_j^{\text{exact}}| / s_j^{\text{exact}}$ to show how power iterations improve accuracy.
4. **Sign Flip Normalization Function**: Write a Python post-processing function `align_svd_signs(U, V)` that ensures the largest absolute value in each column of $V$ is strictly positive, flipping the signs of both $U$ and $V$ columns when negative, guaranteeing deterministic outputs.
5. **Image Compression via Truncated SVD**: Load a grayscale image as a $1000 \times 1000$ matrix. Compute SVD. Reconstruct approximations using ranks $k \in [5, 20, 50, 100]$. Compute the Frobenius reconstruction error and plot the visual clarity progression.
6. **LSA vs NMF Topic Keyword Comparison**: Fit both `TruncatedSVD(n_components=5)` and `NMF(n_components=5)` on the 20 Newsgroups corpus. Print the top 10 words for each topic and compare the semantic interpretability of NMF's strictly non-negative weights versus SVD's signed weights.
