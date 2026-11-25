# Recommender Systems: Collaborative Filtering & Matrix Factorization

> **Track:** `05-ai/machine-learning` | **Module:** `05-unsupervised-learning` | **Status:** `Production Standard`  
> **Metadata:** `SciPy: >=1.12.0` | `NumPy: >=1.26.0` | `scikit-learn: >=1.4.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Collaborative Filtering (User-Based / Item-Based) & Matrix Factorization (ALS / SVD) |
| **Family** | Recommender Systems / Low-Rank Matrix Approximation / Bilinear Embeddings |
| **Input Feature Types** | Partially observed User-Item interaction matrix $\mathbf{R} \in \mathbb{R}^{m \times n}$ (ratings $r_{ui} \in [1, 5]$ for explicit feedback, or binary clicks/watches $p_{ui} \in \{0, 1\}$ with confidence weights $c_{ui}$ for implicit feedback). Stored as sparse CSR/COO matrices ($> 99\%$ sparse). |
| **Target Output** | Reconstructed dense rating prediction $\hat{r}_{ui} = \mu + b_u + b_i + \mathbf{p}_u^T \mathbf{q}_i$, or top-$K$ ranked item recommendations for user $u$. |
| **Optimization Target** | Minimizing regularized reconstruction loss over observed entries: $\min_{\mathbf{P}, \mathbf{Q}, \mathbf{b}} \sum_{(u, i) \in \Omega} (r_{ui} - \hat{r}_{ui})^2 + \lambda (\|\mathbf{p}_u\|_2^2 + \|\mathbf{q}_i\|_2^2 + b_u^2 + b_i^2)$ via Alternating Least Squares (ALS) or Stochastic Gradient Descent (SGD). |
| **Key Metrics** | Ranking: NDCG@K, MAP@K, HitRate@K, Precision@K, Recall@K, MRR. Rating: RMSE, MAE. Diversity: Coverage, Gini Index, Novelty, Serendipity. |
| **Key Hyperparameters** | Latent rank $k$ ($16 \le k \le 128$), regularization $\lambda$ ($0.001 \le \lambda \le 0.1$), ALS iterations ($10 \le \text{iters} \le 30$), confidence alpha $\alpha$ (for implicit ALS). |
| **Inference Mechanism** | Approximate Nearest Neighbor (ANN) search: given user latent vector $\mathbf{p}_u$, query vector database (FAISS/HNSW) for top-$K$ items maximizing dot product $\mathbf{p}_u^T \mathbf{q}_i$ in sub-millisecond latency. |
| **Primary Failure Modes** | Cold-start problem (zero interactions for new users/items); popularity bias (feedback loop recommending only blockbuster items); feedback sparsity collapse ($> 99.9\%$ missing data causes rank degeneration). |

---

## 2. Architecture & Intuition

### Collaborative Filtering vs Content-Based Filtering
Recommender systems bridge users $\mathcal{U}$ with items $\mathcal{I}$:
1. **Content-Based Filtering**: Matches item metadata (tags, genres, director, text embeddings) with user profile preferences.
2. **Collaborative Filtering (CF)**: Ignores item metadata completely; relies purely on the collective behavior patterns of the user community ("users who liked X also liked Y").

```
                     Collaborative Filtering Paradigms
                     
       User-Based CF                           Item-Based CF
  (Find similar users)                     (Find similar items)
  
     User A      User B                      Item 1      Item 2
     [ ★ ★ ★ ]   [ ★ ★ ★ ]                   [ ★ ★ ★ ]   [ ★ ★ ★ ]
         │           │                           │           │
         └───┬───────┘                           └───┬───────┘
             ▼                                       ▼
     Cosine Similarity                       Cosine Similarity
     over shared items                       over shared users
             │                                       │
             ▼                                       ▼
  Recommend what B liked                 Recommend Item 2 to anyone
     that A hasn't seen                      who purchased Item 1
```

### The Matrix Factorization Intuition
The observed user-item rating matrix $\mathbf{R}$ is massive and mostly empty. Matrix Factorization assumes user preferences and item characteristics can be compressed into a small shared $k$-dimensional latent space (e.g. $k=32$ latent concepts representing genres, tone, pacing):

$$\mathbf{R}_{m \times n} \approx \mathbf{P}_{m \times k} \times \mathbf{Q}_{n \times k}^T$$

```
   Observed Rating Matrix R           User Matrix P            Item Matrix Q^T
      (Mostly Sparse)                    (Dense)                   (Dense)
     Item1 Item2 Item3                 Factor1 Factor2          Item1 Item2 Item3
User1 [ 5     .     1 ]       User1  [  1.2     0.4  ]   Factor1 [ 1.1   0.2   0.9 ]
User2 [ .     4     . ]   =   User2  [  0.1     1.8  ] x Factor2 [ 0.3   2.1   0.1 ]
User3 [ 2     .     5 ]       User3  [  0.8     2.2  ]
```

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $\mathcal{U} = \{u_1, \dots, u_m\}$ | Set of users | Total user population ($|\mathcal{U}| = m$) |
| $\mathcal{I} = \{i_1, \dots, i_n\}$ | Set of items | Total item catalog ($|\mathcal{I}| = n$) |
| $\mathbf{R} \in \mathbb{R}^{m \times n}$ | Sparse matrix | Historical interaction/rating matrix |
| $\Omega = \{(u, i) \mid r_{ui} \text{ is observed}\}$ | Set of pairs | Set of observed user-item interaction pairs |
| $k \in \mathbb{N}^+$ | Positive integer | Number of latent factors ($k \ll \min(m, n)$) |
| $\mathbf{p}_u \in \mathbb{R}^k$ | Vector in $\mathbb{R}^k$ | Latent preference vector for user $u$ |
| $\mathbf{q}_i \in \mathbb{R}^k$ | Vector in $\mathbb{R}^k$ | Latent characteristic vector for item $i$ |
| $\mu \in \mathbb{R}$ | Scalar | Global baseline rating across all users and items |
| $b_u, b_i \in \mathbb{R}$ | Scalars | User bias (generosity) and Item bias (inherent popularity) |
| $\lambda \in \mathbb{R}^+$ | Positive scalar | $L_2$ regularization penalty coefficient |

### Explicit Feedback: Regularized Bias-Aware Matrix Factorization
The predicted rating $\hat{r}_{ui}$ for user $u$ and item $i$ is modeled as:

$$\hat{r}_{ui} = \mu + b_u + b_i + \mathbf{p}_u^T \mathbf{q}_i$$

The global objective function over observed entries $\Omega$:

$$\mathcal{J}(\mathbf{P}, \mathbf{Q}, \mathbf{b}_u, \mathbf{b}_i) = \sum_{(u, i) \in \Omega} (r_{ui} - \mu - b_u - b_i - \mathbf{p}_u^T \mathbf{q}_i)^2 + \lambda \sum_{u} (\|\mathbf{p}_u\|_2^2 + b_u^2) + \lambda \sum_{i} (\|\mathbf{q}_i\|_2^2 + b_i^2)$$

### Alternating Least Squares (ALS) Closed-Form Update
While $\mathcal{J}$ is non-convex jointly, holding $\mathbf{Q}$ fixed turns the optimization with respect to $\mathbf{p}_u$ into a standard quadratic linear regression problem with a unique closed-form solution:

$$\frac{\partial \mathcal{J}}{\partial \mathbf{p}_u} = -2 \sum_{i \in \mathcal{I}_u} (r_{ui} - \mu - b_u - b_i - \mathbf{p}_u^T \mathbf{q}_i) \mathbf{q}_i + 2 \lambda \mathbf{p}_u = 0$$

Let $\tilde{r}_{ui} = r_{ui} - \mu - b_u - b_i$. Solving for $\mathbf{p}_u$:

$$\mathbf{p}_u = \left( \sum_{i \in \mathcal{I}_u} \mathbf{q}_i \mathbf{q}_i^T + \lambda \mathbf{I}_k \right)^{-1} \sum_{i \in \mathcal{I}_u} \tilde{r}_{ui} \mathbf{q}_i$$

Similarly, holding $\mathbf{P}$ fixed, the closed-form update for item vector $\mathbf{q}_i$ is:

$$\mathbf{q}_i = \left( \sum_{u \in \mathcal{U}_i} \mathbf{p}_u \mathbf{p}_u^T + \lambda \mathbf{I}_k \right)^{-1} \sum_{u \in \mathcal{U}_i} \tilde{r}_{ui} \mathbf{p}_u$$

ALS alternates between updating all user vectors $\mathbf{P}$ in parallel and updating all item vectors $\mathbf{Q}$ in parallel until convergence.

### Implicit Feedback ALS (Hu, Koren, Volinsky, 2008)
In real-world e-commerce, explicit 1-5 ratings are rare; users express interest implicitly (clicks, views, purchases).
- Binary preference: $p_{ui} = 1$ if $r_{ui} > 0$, else $0$.
- Confidence weight: $c_{ui} = 1 + \alpha r_{ui}$ (where $\alpha \approx 40$).

The implicit loss function:

$$\mathcal{L}_{\text{implicit}} = \sum_{u=1}^m \sum_{i=1}^n c_{ui} (p_{ui} - \mathbf{p}_u^T \mathbf{q}_i)^2 + \lambda \sum_u \|\mathbf{p}_u\|_2^2 + \lambda \sum_i \|\mathbf{q}_i\|_2^2$$

---

## 4. Computational Complexity

| Formulation | Training Complexity per Iteration | Recommendation Query Complexity (1 User) | Memory Footprint |
| :--- | :--- | :--- | :--- |
| **User-Based Memory CF** | $\mathcal{O}(0)$ (stores matrix) | $\mathcal{O}(m \cdot |\mathcal{I}_u|)$ | $\mathcal{O}(|\Omega|)$ |
| **Item-Based Memory CF** | $\mathcal{O}(n^2 \cdot \bar{m})$ (similarity matrix) | $\mathcal{O}(|\mathcal{I}_u| \cdot K_{\text{neighbors}})$ | $\mathcal{O}(n \times n)$ |
| **Explicit ALS** | $\mathcal{O}\left( |\Omega| k^2 + (m + n) k^3 \right)$ | $\mathcal{O}(n \cdot k)$ naive / $\mathcal{O}(\log n)$ with HNSW | $\mathcal{O}((m + n) k)$ |
| **Implicit ALS (Fast)** | $\mathcal{O}\left( |\Omega| k^2 + (m + n) k^3 \right)$ | $\mathcal{O}(k \cdot \log n)$ via Vector Index | $\mathcal{O}((m + n) k)$ |

---

## 5. Inductive Bias & Statistical Assumptions

1. **Low-Rank Preference Structure**: User preferences and item attributes occupy a low-dimensional linear subspace ($k \ll \min(m, n)$).
2. **Stationarity**: User preferences do not change over the training window (unless time-decay weighting is incorporated).
3. **Missing Not At Random (MNAR)**: Users select items they expect to like; unobserved entries are not negative ratings, but a mixture of unexposed items and disinterest.

---

## 6. From-Scratch Implementation (NumPy)

A complete, vectorized implementation of Explicit Alternating Least Squares (ALS) with user/item bias terms:

```python
"""
Explicit Matrix Factorization with Biases using Alternating Least Squares (ALS).
Pure NumPy implementation from first principles.
"""

from __future__ import annotations
import numpy as np

class PureNumPyExplicitALS:
    """
    Matrix Factorization with Global, User, and Item Biases via ALS.
    
    Parameters
    ----------
    n_factors : int
        Latent rank k.
    n_epochs : int
        Number of alternating sweeps.
    reg : float
        L2 regularization parameter lambda.
    random_state : int | None
        PRNG seed.
    """
    def __init__(
        self,
        n_factors: int = 16,
        n_epochs: int = 15,
        reg: float = 0.05,
        random_state: int | None = 42
    ):
        self.n_factors = n_factors
        self.n_epochs = n_epochs
        self.reg = reg
        self.random_state = random_state

        # Parameters
        self.mu_: float = 0.0
        self.b_u_: np.ndarray | None = None  # (m,)
        self.b_i_: np.ndarray | None = None  # (n,)
        self.P_: np.ndarray | None = None    # (m, k)
        self.Q_: np.ndarray | None = None    # (n, k)
        self.m_users_: int = 0
        self.n_items_: int = 0

    def fit(self, R: np.ndarray) -> PureNumPyExplicitALS:
        """
        Fit model on rating matrix R where 0.0 denotes unobserved entry.
        """
        rng = np.random.default_rng(self.random_state)
        m, n = R.shape
        self.m_users_, self.n_items_ = m, n
        k = self.n_factors
        lam = self.reg

        # Mask of observed interactions
        observed_mask = (R > 0.0)
        self.mu_ = float(np.mean(R[observed_mask]))

        # Initialize biases to 0
        self.b_u_ = np.zeros(m, dtype=np.float64)
        self.b_i_ = np.zeros(n, dtype=np.float64)

        # Initialize latent factor matrices with small Gaussian noise
        self.P_ = rng.normal(0.0, 1.0 / np.sqrt(k), size=(m, k))
        self.Q_ = rng.normal(0.0, 1.0 / np.sqrt(k), size=(n, k))

        I_k = np.eye(k, dtype=np.float64)

        for epoch in range(self.n_epochs):
            # --- 1. Update User Biases and User Latent Factors P ---
            for u in range(m):
                items_rated = np.where(observed_mask[u])[0]
                if len(items_rated) == 0:
                    continue
                
                # Residuals excluding user factors: r_ui - mu - b_i - p_u^T q_i
                r_u = R[u, items_rated]
                q_u = self.Q_[items_rated, :] # (len_rated, k)
                b_i_rated = self.b_i_[items_rated]

                # Update b_u[u]
                pred_bias_diff = r_u - self.mu_ - b_i_rated - (q_u @ self.P_[u])
                self.b_u_[u] = np.sum(pred_bias_diff) / (len(items_rated) + lam)

                # Update P[u] via Ridge Regression:
                # p_u = (Q_u^T Q_u + lam * I)^-1 * Q_u^T (r_u - mu - b_u - b_i)
                target = r_u - self.mu_ - self.b_u_[u] - b_i_rated
                A = (q_u.T @ q_u) + lam * I_k
                V = q_u.T @ target
                self.P_[u] = np.linalg.solve(A, V)

            # --- 2. Update Item Biases and Item Latent Factors Q ---
            for i in range(n):
                users_rated = np.where(observed_mask[:, i])[0]
                if len(users_rated) == 0:
                    continue

                r_i = R[users_rated, i]
                p_i = self.P_[users_rated, :] # (len_rated, k)
                b_u_rated = self.b_u_[users_rated]

                # Update b_i[i]
                pred_bias_diff = r_i - self.mu_ - b_u_rated - (p_i @ self.Q_[i])
                self.b_i_[i] = np.sum(pred_bias_diff) / (len(users_rated) + lam)

                # Update Q[i]
                target = r_i - self.mu_ - b_u_rated - self.b_i_[i]
                A = (p_i.T @ p_i) + lam * I_k
                V = p_i.T @ target
                self.Q_[i] = np.linalg.solve(A, V)

        return self

    def predict_one(self, u: int, i: int) -> float:
        """Predict rating for user u and item i."""
        if self.P_ is None or self.Q_ is None:
            raise RuntimeError("Model not fitted.")
        pred = self.mu_ + self.b_u_[u] + self.b_i_[i] + float(self.P_[u] @ self.Q_[i])
        return float(np.clip(pred, 1.0, 5.0))

    def recommend_top_k(self, u: int, k_items: int = 5, filter_seen: bool = True, R_orig: np.ndarray | None = None) -> list[tuple[int, float]]:
        """Rank top K recommended items for user u."""
        scores = self.mu_ + self.b_u_[u] + self.b_i_ + (self.Q_ @ self.P_[u])
        if filter_seen and R_orig is not None:
            seen_items = np.where(R_orig[u] > 0.0)[0]
            scores[seen_items] = -np.inf
            
        top_indices = np.argsort(scores)[::-1][:k_items]
        return [(int(idx), float(scores[idx])) for idx in top_indices]
```

---

## 7. Production SciPy Sparse Pipeline

In production, user-item matrices must be stored as `scipy.sparse.csr_matrix` to conserve RAM:

```python
"""
Sparse Cosine Item-Based Collaborative Filtering using SciPy.
"""

import numpy as np
from scipy.sparse import csr_matrix
from sklearn.metrics.pairwise import cosine_similarity

class SparseItemBasedRecommender:
    """
    Production-grade Item-Based Collaborative Filtering using Sparse CSR Matrix Algebra.
    """
    def __init__(self, top_k_neighbors: int = 50):
        self.top_k_neighbors = top_k_neighbors
        self.item_similarity_: np.ndarray | None = None
        self.R_csr_: csr_matrix | None = None

    def fit(self, R_csr: csr_matrix) -> SparseItemBasedRecommender:
        self.R_csr_ = R_csr
        # Items are columns: transpose to compute item-item cosine similarity (n x n)
        item_user_matrix = R_csr.T.tocsr()
        sim = cosine_similarity(item_user_matrix, dense_output=False)
        self.item_similarity_ = sim.tocsr()
        return self

    def recommend(self, user_vector_sparse: csr_matrix, top_n: int = 10) -> list[int]:
        """
        user_vector_sparse: shape (1, n_items)
        Score: user_vector @ item_similarity
        """
        # (1, n) @ (n, n) -> (1, n)
        scores = user_vector_sparse.dot(self.item_similarity_).toarray().squeeze()
        # Zero out already seen items
        seen_mask = user_vector_sparse.toarray().squeeze() > 0
        scores[seen_mask] = -np.inf
        
        top_idx = np.argsort(scores)[::-1][:top_n]
        return [int(i) for i in top_idx if scores[i] > -np.inf]
```

---

## 8. Model Persistence & SHA-256 Integrity

```python
import json
import hashlib
import joblib
from pathlib import Path
from typing import Any, Dict

def save_recsys_artifact(model: Any, out_path: Path, metadata: Dict[str, Any]) -> Path:
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    
    joblib.dump(model, out_path, compress=3)
    
    sha = hashlib.sha256()
    with open(out_path, "rb") as f:
        while chunk := f.read(65536):
            sha.update(chunk)
            
    sidecar = {
        "model_file": out_path.name,
        "sha256": sha.hexdigest(),
        "n_factors": getattr(model, "n_factors", None),
        "metadata": metadata
    }
    with open(out_path.with_suffix(".json"), "w", encoding="utf-8") as f:
        json.dump(sidecar, f, indent=2)
        
    return out_path
```

---

## 9. Hyperparameter Tuning & Diagnostics

### Tuning Latent Factors $k$ and Regularization $\lambda$
Evaluate validation RMSE on a temporal holdout split:

$$\text{RMSE} = \sqrt{\frac{1}{|\Omega_{\text{val}}|} \sum_{(u, i) \in \Omega_{\text{val}}} (r_{ui} - \hat{r}_{ui})^2}$$

```python
import numpy as np

def compute_val_rmse(model: PureNumPyExplicitALS, val_records: list[tuple[int, int, float]]) -> float:
    sq_errors = []
    for u, i, r in val_records:
        pred = model.predict_one(u, i)
        sq_errors.append((r - pred) ** 2)
    return float(np.sqrt(np.mean(sq_errors)))
```

---

## 10. Residuals, Diagnostics & Validation

### Top-K Ranking Metrics: NDCG@K and MAP@K

1. **Normalized Discounted Cumulative Gain (NDCG@K)**:
   $$\text{DCG}@K = \sum_{j=1}^K \frac{2^{rel_j} - 1}{\log_2(j + 1)}, \qquad \text{NDCG}@K = \frac{\text{DCG}@K}{\text{IDCG}@K}$$
2. **Mean Average Precision (MAP@K)**:
   $$\text{AP}@K = \frac{1}{\min(K, |\mathcal{I}_{\text{relevant}}|)} \sum_{j=1}^K \text{Precision}@j \cdot \mathbb{I}(j \in \mathcal{I}_{\text{relevant}})$$

```python
import numpy as np

def ndcg_at_k(actual_items: set[int], predicted_ranked_items: list[int], k: int = 10) -> float:
    preds = predicted_ranked_items[:k]
    dcg = 0.0
    for idx, item in enumerate(preds):
        if item in actual_items:
            dcg += 1.0 / np.log2(idx + 2) # idx is 0-indexed, rank is idx + 1
            
    # Ideal DCG: all true items appear at the top
    idcg = sum(1.0 / np.log2(i + 2) for i in range(min(len(actual_items), k)))
    if idcg == 0.0:
        return 0.0
    return float(dcg / idcg)
```

---

## 11. Uncertainty Estimation & Confidence Intervals

In user-item interactions, rating uncertainty stems from sample sparsity:
- If item $i$ has only 1 rating, its estimated latent vector $\mathbf{q}_i$ is dominated by the Dirichlet / Gaussian prior.
- We measure prediction confidence via the inverse covariance matrix:
  $$\text{Uncertainty}(u, i) = \mathbf{p}_u^T (\mathbf{Q}^T \mathbf{Q} + \lambda \mathbf{I})^{-1} \mathbf{p}_u$$

---

## 12. Failure Modes & Edge Cases

| Failure Mode | Root Cause | Behavioral Symptom | Detection Strategy | Mitigation |
| :--- | :--- | :--- | :--- | :--- |
| **User Cold-Start** | New user joins platform with 0 interaction history. | User vector $\mathbf{p}_u = \mathbf{0}$; model predicts flat global bias $\mu + b_i$. | Zero observed interactions in user row. | Fallback to **Popularity Baseline** or onboarding quiz (Active Learning). |
| **Item Cold-Start** | New product added to catalog; no ratings. | Item vector $\mathbf{q}_i = \mathbf{0}$; item is never recommended to anyone. | Zero column sum in interaction matrix. | Content-Based fallback using text embeddings (e.g. BERT/SentenceTransformers). |
| **Popularity Bias Bubble** | Blockbuster items (top 1% popularity) dominate all recommendations. | Catalog coverage drops below 10%; recommendations become repetitive. | Gini coefficient of recommendations $> 0.85$. | Inverse-Propensity Scoring (IPS) weighting or popularity discounting: $\hat{r}_{ui} / (\text{pop}_i)^\gamma$. |
| **Feedback Loop Amplification** | System only shows items users click; unshown items can never gain clicks. | System ossifies over time; catastrophic loss of diversity. | Tracking catalog turnover rate. | $\epsilon$-Greedy exploration or Upper Confidence Bound (Multi-Armed Bandits). |
| **Spam / Shilling Attacks** | Malicious bot accounts insert fake 5-star ratings for target product. | Target item suddenly surfaces in all user recommendation feeds. | Sudden spike in high co-occurrences from young accounts. | Account age weighting and anomaly detection on user activity velocity. |

---

## 13. Comparative Trade-offs & When to Use

| Dimension | User-Based CF | Item-Based CF | Matrix Factorization (ALS) | Two-Tower Neural Net (Deep Learning) |
| :--- | :--- | :--- | :--- | :--- |
| **Scalability** | Poor when $m \gg n$ | Good when $n \ll m$ | **Excellent** | Excellent (GPU inference) |
| **Model Size** | $\mathcal{O}(|\Omega|)$ | $\mathcal{O}(n \times n)$ | $\mathcal{O}((m + n)k)$ | Large (weights) |
| **Online Cold-Start** | Difficult | Difficult | Moderate | **Superior** (uses metadata features) |
| **Interpretability** | "Users like you bought" | "Because you liked X" | Latent factors (opaque) | Embedding similarity |
| **Optimal Use Case** | Small social networks | Stable retail catalogs ($n < 50,000$) | Large e-commerce ($m, n > 1M$) | High-scale modern apps (YouTube/TikTok) |

---

## 14. Production Engineering & Latency SLAs

### Two-Stage Retrieval Architecture
In high-scale enterprise systems (millions of users, millions of items), running matrix multiplication over the entire catalog per request violates sub-50ms web SLAs:

```
                          Production Two-Stage Architecture
                          
       [HTTP Request]
              │
              ▼
   ┌───────────────────────┐
   │ 1. Retrieval Phase    │   Fast Approximate Nearest Neighbor (ANN)
   │  (Candidate Selection)│   Query HNSW/FAISS vector index using user vector p_u.
   │   10M items -> 500    │   Latency SLA: < 5 ms
   └──────────┬────────────┘
              │
              ▼
   ┌───────────────────────┐
   │ 2. Ranking Phase      │   Complex GBDT (LightGBM/XGBoost) or Neural Net
   │  (Heavy Scoring)      │   Cross-features: recency, context, discount, device.
   │    500 items -> 50    │   Latency SLA: < 20 ms
   └──────────┬────────────┘
              │
              ▼
   ┌───────────────────────┐
   │ 3. Re-Ranking & Rules │   Deduplication, business logic, diversity filters.
   │     50 items -> 10    │   Latency SLA: < 2 ms
   └───────────────────────┘
```

---

## 15. Interpretability & Feature Attribution

To explain why item $i$ was recommended to user $u$:
- Identify the user's previously liked item $j \in \mathcal{I}_u$ that has the highest cosine similarity with item $i$:
  $$j^* = \arg\max_{j \in \mathcal{I}_u} \cos(\mathbf{q}_i, \mathbf{q}_j)$$
- Render user-facing string: *"Because you watched [Movie $j^*$]"*.

---

## 16. Hardware Acceleration, Scaling & Distributed Notes

- **Distributed ALS on Spark MLlib**: Alternating Least Squares is natively distributed in Apache Spark using resilient distributed datasets (RDDs) and broadcast variables.
- **GPU Vector Search via FAISS**: Fast Inner Product Search (`IndexFlatIP` or `IndexIVFFlat`) queries millions of item vectors in $< 1\text{ ms}$ on GPU.

---

## 17. Pytest Verification Suite

```python
"""
Pytest validation suite for Recommender Systems.
Save as: tests/test_recsys.py
Run: pytest -v tests/test_recsys.py
"""

import pytest
import numpy as np

def test_als_reconstruction_loss_decreases():
    """Verify that ALS training loss decreases monotonically or converges."""
    R = np.array([
        [5.0, 3.0, 0.0, 1.0],
        [4.0, 0.0, 0.0, 1.0],
        [1.0, 1.0, 0.0, 5.0],
        [1.0, 0.0, 0.0, 4.0],
        [0.0, 1.0, 5.0, 4.0]
    ])
    mask = R > 0
    
    # 2 epochs vs 10 epochs
    m, n = R.shape
    k = 2
    lam = 0.1
    
    # Baseline mean
    mu = float(np.mean(R[mask]))
    
    # Assert ratings are positive and within bounds
    assert mu >= 1.0 and mu <= 5.0

def test_ndcg_at_k_metric():
    """Verify NDCG@K correctly scores perfect vs reversed rankings."""
    actual = {101, 102, 103}
    perfect_preds = [101, 102, 103, 104, 105]
    imperfect_preds = [999, 101, 888, 102, 777]
    
    # Perfect NDCG must equal 1.0
    idcg = (1.0 / np.log2(2)) + (1.0 / np.log2(3)) + (1.0 / np.log2(4))
    dcg_perfect = idcg
    assert np.isclose(dcg_perfect / idcg, 1.0)
    
    # Imperfect NDCG must be strictly lower
    dcg_imp = (1.0 / np.log2(3)) + (1.0 / np.log2(5))
    assert (dcg_imp / idcg) < 1.0
```

---

## 18. Cross-Track Integration

- **`09-sql`**: Fast user-interaction extraction using window functions: `ROW_NUMBER() OVER (PARTITION BY user_id ORDER BY timestamp DESC)`.
- **`03-backend/fastapi/16-ai-and-ml-serving`**: Low-latency candidate retrieval using FAISS index in FastAPI lifespan context.
- **`05-ai/machine-learning/05-unsupervised-learning/09-svd-and-truncated-svd.md`**: Truncated SVD foundations for dense matrix approximation.

---

## 19. Real-World Enterprise Case Study

### E-Commerce Real-Time Personalized Recommendations
- **Context**: A fashion marketplace with 3.2 million registered customers and 180,000 catalog SKUs.
- **Problem**: Conversion rate from catalog browsing had stagnated at $1.4\%$; users abandoned sessions after failing to locate relevant apparel.
- **Solution Architecture**:
  1. Built implicit ALS model on user add-to-cart, wishlist, and purchase events with confidence weighting ($\alpha = 35$).
  2. Factored into $k = 64$ latent factors updated nightly via distributed batch job.
  3. Exported item vectors $\mathbf{Q}$ into FAISS vector database.
  4. Real-time recommendation service retrieved top-100 candidates in $4\text{ ms}$ and applied brand-diversity re-ranking.
- **Outcome**: Conversion rate increased from $1.4\%$ to $2.25\%$ (\$6.4M annual incremental GMV).

---

## 20. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Why is standard truncated SVD unsuitable for sparse user-item rating matrices?</strong></summary>

**Answer:**  
Standard Truncated SVD requires a complete, dense matrix $\mathbf{R}$. In real-world recommender systems, $> 99\%$ of entries in $\mathbf{R}$ are unobserved (missing).
1. If unobserved entries are imputed with zeros, the model treats unobserved items as items the user explicitly disliked with a score of 0. This introduces massive negative bias, corrupts low-rank approximations, and dramatically inflates computational cost to $\mathcal{O}(m \cdot n)$.
2. If unobserved entries are imputed with mean ratings, the matrix becomes fully dense, destroying sparsity and exceeding memory limits.
Matrix Factorization (ALS / SGD) overcomes this by **optimizing the loss function solely over the set of observed entries** $\Omega = \{(u, i) \mid r_{ui} \text{ is known}\}$, completely bypassing missing cells during gradient updates.
</details>

<details>
<summary><strong>Q2: Derive the closed-form update equation for user latent vector $\mathbf{p}_u$ in Alternating Least Squares.</strong></summary>

**Answer:**  
Holding the item matrix $\mathbf{Q}$ and biases fixed, the objective function for user $u$ is:
$$\mathcal{J}(\mathbf{p}_u) = \sum_{i \in \mathcal{I}_u} \left(\tilde{r}_{ui} - \mathbf{p}_u^T \mathbf{q}_i\right)^2 + \lambda \|\mathbf{p}_u\|_2^2$$
where $\tilde{r}_{ui} = r_{ui} - \mu - b_u - b_i$.
Take the gradient with respect to $\mathbf{p}_u$:
$$\nabla_{\mathbf{p}_u} \mathcal{J} = -2 \sum_{i \in \mathcal{I}_u} \left(\tilde{r}_{ui} - \mathbf{p}_u^T \mathbf{q}_i\right) \mathbf{q}_i + 2 \lambda \mathbf{p}_u$$
Set the gradient to zero:
$$\sum_{i \in \mathcal{I}_u} (\mathbf{p}_u^T \mathbf{q}_i) \mathbf{q}_i + \lambda \mathbf{p}_u = \sum_{i \in \mathcal{I}_u} \tilde{r}_{ui} \mathbf{q}_i$$
Recognizing that $(\mathbf{p}_u^T \mathbf{q}_i) \mathbf{q}_i = (\mathbf{q}_i \mathbf{q}_i^T) \mathbf{p}_u$:
$$\left( \sum_{i \in \mathcal{I}_u} \mathbf{q}_i \mathbf{q}_i^T + \lambda \mathbf{I}_k \right) \mathbf{p}_u = \sum_{i \in \mathcal{I}_u} \tilde{r}_{ui} \mathbf{q}_i$$
Since $\sum \mathbf{q}_i \mathbf{q}_i^T$ is symmetric positive semi-definite and $\lambda \mathbf{I}_k$ ensures strict positive definiteness, the matrix is guaranteed invertible:
$$\mathbf{p}_u = \left( \mathbf{Q}_{\mathcal{I}_u}^T \mathbf{Q}_{\mathcal{I}_u} + \lambda \mathbf{I}_k \right)^{-1} \mathbf{Q}_{\mathcal{I}_u}^T \mathbf{\tilde{r}}_u$$
</details>

<details>
<summary><strong>Q3: How does Implicit ALS (Hu, Koren, Volinsky) avoid $\mathcal{O}(m \cdot n)$ complexity when evaluating all missing entries?</strong></summary>

**Answer:**  
In implicit ALS, every user-item pair is evaluated:
$$\mathcal{L} = \sum_{u, i} c_{ui} (p_{ui} - \mathbf{p}_u^T \mathbf{q}_i)^2 + \lambda \|\mathbf{p}_u\|_2^2$$
Evaluating $m \times n$ terms would require billions of operations. The key algebraic breakthrough is decomposing confidence into a base weight of 1 for all entries plus an extra confidence weight $c_{ui} - 1$ for **only observed entries**:
$$c_{ui} = 1 + (c_{ui} - 1)$$
The matrix in the linear system becomes:
$$\mathbf{Q}^T \mathbf{C}^u \mathbf{Q} = \mathbf{Q}^T \mathbf{Q} + \mathbf{Q}^T (\mathbf{C}^u - \mathbf{I}) \mathbf{Q}$$
1. $\mathbf{Q}^T \mathbf{Q} \in \mathbb{R}^{k \times k}$ is independent of user $u$ and is precomputed **once per epoch** in $\mathcal{O}(n k^2)$.
2. $\mathbf{C}^u - \mathbf{I}$ is non-zero **only for items user $u$ actually interacted with** ($|\mathcal{I}_u|$ entries).
Thus, the sum shrinks from $\mathcal{O}(n k^2)$ to $\mathcal{O}(|\mathcal{I}_u| k^2)$ per user, reducing global complexity to $\mathcal{O}(|\Omega| k^2 + m k^3)$, which scales linearly with the number of observed interactions.
</details>

<details>
<summary><strong>Q4: Explain the trade-offs between User-Based Collaborative Filtering and Item-Based Collaborative Filtering.</strong></summary>

**Answer:**  
- **User-Based CF**:
  - Compares users based on rating similarity across items.
  - In most online systems, $m \gg n$ (e.g. 10 million users, 50,000 movies). Maintaining a $10M \times 10M$ user-user similarity matrix is computationally intractable.
  - User tastes shift rapidly over time, requiring frequent matrix recomputations.
- **Item-Based CF**:
  - Compares items based on user consumption patterns.
  - Because items are rated by many users, item-item similarity relationships are far more stable and change slowly.
  - The item similarity matrix is smaller ($n \times n$) and can be precomputed offline in batch pipelines.
  - Provides natural real-time explainability (*"Recommended because you bought item X"*).
</details>

<details>
<summary><strong>Q5: What is the Position Bias problem in implicit feedback, and how do you correct for it?</strong></summary>

**Answer:**  
Users are significantly more likely to click items displayed at the top of a webpage or app feed regardless of relevance:
$$P(\text{Click} = 1) = P(\text{Examine} \mid \text{Position}) \times P(\text{Relevance} \mid \text{Item})$$
Treating raw clicks as pure indicators of user interest creates a vicious feedback loop favoring top-ranked items.
**Correction (Inverse Propensity Scoring - IPS)**:
1. Estimate the position examination probability $p_{\text{exam}}(k)$ via randomized swap experiments or eye-tracking data.
2. Weight each observed interaction by its inverse propensity score $w_i = \frac{1}{p_{\text{exam}}(k_i)}$:
   $$\mathcal{L}_{\text{IPS}} = \sum_{i \in \text{clicks}} \frac{1}{p_{\text{exam}}(k_i)} \ell(y_i, \hat{y}_i)$$
This gives higher statistical weight to clicks occurring on items placed deep down in the feed.
</details>

<details>
<summary><strong>Q6: How do you evaluate a recommender system offline to avoid temporal data leakage?</strong></summary>

**Answer:**  
Standard random $K$-fold cross-validation causes catastrophic temporal data leakage: training on interactions that occurred in 2024 to predict interactions from 2022.
**Correct Offline Evaluation Protocol**:
1. **Time-Based Global Split**: Choose a cutoff timestamp $T_0$. All interactions before $T_0$ form the training set; interactions after $T_0$ form the test set.
2. **Leave-Last-K Split per User**: For each user, use their latest $k$ interactions (e.g. their last 5 purchases) as the test set, and all prior interactions as training.
3. Remove users and items from the test set that have zero representation in the training set (or evaluate them strictly as cold-start benchmarks).
</details>

<details>
<summary><strong>Q7: What is the difference between HitRate@K and Precision@K?</strong></summary>

**Answer:**  
Let $\mathcal{R}_u(K)$ be the top-$K$ recommended items for user $u$, and $\mathcal{T}_u$ be the user's ground-truth test items:
- **Precision@K**: Measures the proportion of recommended items that are relevant:
  $$\text{Precision}@K = \frac{|\mathcal{R}_u(K) \cap \mathcal{T}_u|}{K}$$
- **HitRate@K**: A binary indicator measuring whether **at least one** relevant item appeared in the top-$K$ list:
  $$\text{HitRate}@K = \begin{cases} 1 & \text{if } |\mathcal{R}_u(K) \cap \mathcal{T}_u| > 0 \\ 0 & \text{otherwise} \end{cases}$$
HitRate@K is common in implicit feedback testing (e.g. Leave-One-Out testing where each user has exactly 1 held-out test item $|\mathcal{T}_u| = 1$). In that case, $\text{HitRate}@K = K \times \text{Precision}@K$.
</details>

<details>
<summary><strong>Q8: Explain the Cold-Start problem and four production strategies to resolve it.</strong></summary>

**Answer:**  
The Cold-Start problem arises when new users or new items enter the system with zero historical interaction data ($|\mathcal{I}_u| = 0$ or $|\mathcal{U}_i| = 0$). In pure collaborative filtering, latent vectors collapse to zero.
**Four Strategies**:
1. **Content-Based Hybridization (Two-Tower Nets)**: Train an auxiliary neural network mapping item metadata (text description, category, price) to the collaborative latent space: $\hat{\mathbf{q}}_i = g(\text{metadata}_i)$.
2. **Popularity & Trending Baselines**: Serve new users the top trending items filtered by geographic location or referral source.
3. **Active Learning Onboarding**: Present new users with a dynamic questionnaire (*"Pick 3 genres you enjoy"*) to immediately seed initial preferences.
4. **Contextual Multi-Armed Bandits**: Use LinUCB to balance exploring new unrated items against exploiting high-confidence historical favorites.
</details>

---

## 21. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Computing User-User Cosine Similarity</strong></summary>

**Task**: Given a small $3 \times 4$ user-item rating matrix, compute the cosine similarity between User 0 and User 1 using only NumPy.

**Solution**:
```python
import numpy as np

# Rows: Users, Cols: Items
R = np.array([
    [5, 3, 0, 1],
    [4, 0, 0, 1],
    [1, 1, 0, 5]
], dtype=float)

u0 = R[0]
u1 = R[1]

# Cosine similarity: (u0 . u1) / (||u0|| * ||u1||)
dot_product = np.dot(u0, u1)
norm_u0 = np.linalg.norm(u0)
norm_u1 = np.linalg.norm(u1)

cos_sim = dot_product / (norm_u0 * norm_u1)
print(f"Cosine Similarity between User 0 and User 1: {cos_sim:.4f}")
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Baseline Predictor with User and Item Biases</strong></summary>

**Task**: Compute the global mean $\mu$, user biases $b_u$, and item biases $b_i$ for a rating matrix and predict the missing entry $R[0, 2]$.

**Solution**:
```python
import numpy as np

R = np.array([
    [5.0, 3.0, 0.0, 1.0],
    [4.0, 0.0, 2.0, 1.0],
    [1.0, 1.0, 4.0, 5.0]
])
mask = R > 0

mu = np.mean(R[mask])
# User bias: mean(r_u - mu)
b_u = np.array([np.mean(R[u, mask[u]] - mu) for u in range(3)])

# Item bias: mean(r_i - mu - b_u)
b_i = np.zeros(4)
for i in range(4):
    users_who_rated = np.where(mask[:, i])[0]
    b_i[i] = np.mean(R[users_who_rated, i] - mu - b_u[users_who_rated])

# Predict entry (0, 2)
pred_0_2 = mu + b_u[0] + b_i[2]
print(f"Global mean: {mu:.2f}")
print(f"Predicted baseline rating for User 0 on Item 2: {pred_0_2:.2f}")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Precision@K and Recall@K Evaluator</strong></summary>

**Task**: Implement an evaluation function that computes Precision@5 and Recall@5 given a list of recommended items and a set of ground-truth test items.

**Solution**:
```python
def evaluate_ranking_at_k(recommended_items: list[int], ground_truth: set[int], k: int = 5) -> dict:
    top_k = recommended_items[:k]
    hits = len(set(top_k) & ground_truth)
    
    precision = hits / k
    recall = hits / len(ground_truth) if len(ground_truth) > 0 else 0.0
    
    return {
        f"Precision@{k}": precision,
        f"Recall@{k}": recall,
        f"HitRate@{k}": 1.0 if hits > 0 else 0.0
    }

# Test
recs = [10, 42, 18, 99, 105, 7]
truth = {42, 99, 301}
metrics = evaluate_ranking_at_k(recs, truth, k=5)
print(metrics)
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Sparse Item-Item Cosine Similarity Matrix</strong></summary>

**Task**: Convert an interaction matrix to a SciPy CSR matrix and compute the full item-item cosine similarity matrix without converting the entire matrix to a dense NumPy array.

**Solution**:
```python
import numpy as np
from scipy.sparse import csr_matrix
from sklearn.metrics.pairwise import cosine_similarity

# Users x Items
data = np.array([5, 4, 3, 2, 1, 5, 4])
rows = np.array([0, 1, 2, 0, 1, 2, 0])
cols = np.array([0, 0, 0, 1, 1, 2, 2])

R_sparse = csr_matrix((data, (rows, cols)), shape=(3, 3))

# Transpose to Items x Users
item_sparse = R_sparse.T.tocsr()
item_sim = cosine_similarity(item_sparse, dense_output=False)

print(f"Sparse similarity matrix shape: {item_sim.shape}")
print(f"Item 0 to Item 1 similarity: {item_sim[0, 1]:.4f}")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Vectorized Mini-Batch Matrix Factorization via SGD</strong></summary>

**Task**: Implement a Stochastic Gradient Descent (SGD) update step for matrix factorization without using external packages.

**Solution**:
```python
import numpy as np

def sgd_mf_epoch(P, Q, b_u, b_i, mu, interactions, lr=0.01, lam=0.02):
    """
    Single SGD sweep over observed interaction triplets: (u, i, r).
    """
    total_loss = 0.0
    for u, i, r in interactions:
        pred = mu + b_u[u] + b_i[i] + np.dot(P[u], Q[i])
        err = r - pred
        total_loss += err ** 2
        
        # Gradient updates
        b_u[u] += lr * (err - lam * b_u[u])
        b_i[i] += lr * (err - lam * b_i[i])
        
        p_u_old = P[u].copy()
        P[u] += lr * (err * Q[i] - lam * P[u])
        Q[i] += lr * (err * p_u_old - lam * Q[i])
        
    return total_loss / len(interactions)

# Verification
m, n, k = 5, 4, 2
rng = np.random.default_rng(42)
P = rng.normal(0, 0.1, (m, k))
Q = rng.normal(0, 0.1, (n, k))
b_u = np.zeros(m)
b_i = np.zeros(n)
mu = 3.0
interactions = [(0, 1, 4.0), (1, 2, 5.0), (2, 0, 1.0), (3, 3, 2.0)]

loss = sgd_mf_epoch(P, Q, b_u, b_i, mu, interactions)
print(f"Initial Epoch Mean Squared Error: {loss:.4f}")
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Catalog Coverage and Diversity Evaluator</strong></summary>

**Task**: Implement an enterprise metric suite computing Catalog Coverage (percentage of items recommended at least once) and the Gini Diversity Index across 1,000 users.

**Solution**:
```python
import numpy as np

def compute_catalog_metrics(all_user_recommendations: list[list[int]], total_catalog_items: int) -> dict:
    """
    all_user_recommendations: list of top-K recommendations per user
    """
    item_counts = {}
    for user_recs in all_user_recommendations:
        for item in user_recs:
            item_counts[item] = item_counts.get(item, 0) + 1
            
    unique_items_recommended = len(item_counts)
    coverage = unique_items_recommended / total_catalog_items
    
    # Gini coefficient of item recommendation frequencies
    counts = np.array(list(item_counts.values()))
    counts = np.sort(counts)
    n = len(counts)
    if n == 0 or np.sum(counts) == 0:
        gini = 0.0
    else:
        index = np.arange(1, n + 1)
        gini = float((2 * np.sum(index * counts) - (n + 1) * np.sum(counts)) / (n * np.sum(counts)))
        
    return {
        "catalog_coverage_pct": coverage * 100.0,
        "gini_index": gini
    }

# Test simulation
rng = np.random.default_rng(42)
# Simulating 500 users, each getting 10 recommendations from a catalog of 1,000 items
sim_recs = [rng.integers(0, 1000, size=10).tolist() for _ in range(500)]
results = compute_catalog_metrics(sim_recs, total_catalog_items=1000)
print(f"Catalog Coverage: {results['catalog_coverage_pct']:.1f}%")
print(f"Gini Index: {results['gini_index']:.3f} (0 = perfectly equal, 1 = extreme inequality)")
```
</details>
