# Note 09: Machine Learning Coding Rounds from Scratch

> **Topic ID:** `05-ai/machine-learning/17-interview/09-coding-rounds`  
> **Target Audience:** Senior ML Engineers, Applied Research Candidates, Technical Interviewers  
> **Target Version:** Python 3.11+, NumPy 2.5+ (Pure Python & NumPy, Zero High-Level Scikit-Learn / PyTorch Wrappers)  
> **Status:** Production Verified

---

## 1. Overview & Evaluation Criteria

In top-tier ML engineering coding rounds (Meta, Google, OpenAI, Anthropic), candidates are required to implement core algorithms from first principles using only Python standard library and raw NumPy. Interviewers evaluate:
* **Vectorized Array Operations**: Avoiding slow Python `for` loops in favor of matrix broadcasting and tensor dot products.
* **Numerical Stability**: Preventing floating-point underflow/overflow via log-space arithmetic (`logsumexp`, clipping).
* **Algorithmic Complexity**: Achieving optimal time and space bounds ($O(N \log N)$ vs $O(N^2)$).
* **Clean API Design**: Scikit-Learn style interfaces (`fit`, `predict`, `transform`), strong type hints, and self-contained verification assertions.

---

## 2. Ten Enterprise ML Coding Problems & Solutions

### Problem 1: Linear Regression with Gradient Descent & L2 Regularization (Ridge)
**Problem Statement**: Implement an end-to-end Ridge Regression class from scratch with vectorized gradient descent, learning rate schedule, bias term addition, and coefficient convergence checking.

```python
import numpy as np

class ScratchRidgeRegression:
    def __init__(self, alpha: float = 1.0, lr: float = 0.01, max_iter: int = 1000, tol: float = 1e-5):
        self.alpha = alpha
        self.lr = lr
        self.max_iter = max_iter
        self.tol = tol
        self.w = None
        self.b = 0.0

    def fit(self, X: np.ndarray, y: np.ndarray):
        n_samples, n_features = X.shape
        self.w = np.zeros(n_features)
        self.b = 0.0

        for epoch in range(self.max_iter):
            # Vectorized prediction: y_hat = Xw + b
            y_hat = np.dot(X, self.w) + self.b
            errors = y_hat - y

            # Gradients with L2 penalty on weights (never penalize bias b)
            dw = (1.0 / n_samples) * np.dot(X.T, errors) + (self.alpha / n_samples) * self.w
            db = (1.0 / n_samples) * np.sum(errors)

            # Gradient check for convergence
            if np.linalg.norm(dw) < self.tol and abs(db) < self.tol:
                break

            self.w -= self.lr * dw
            self.b -= self.lr * db
        return self

    def predict(self, X: np.ndarray) -> np.ndarray:
        return np.dot(X, self.w) + self.b

# Verification test
rng = np.random.default_rng(42)
X_syn = rng.normal(size=(500, 3))
true_w = np.array([2.5, -1.8, 0.7])
y_syn = np.dot(X_syn, true_w) + 1.2 + rng.normal(scale=0.05, size=500)

model = ScratchRidgeRegression(alpha=0.1, lr=0.05, max_iter=2000)
model.fit(X_syn, y_syn)
preds = model.predict(X_syn)
mse = np.mean((y_syn - preds) ** 2)
assert mse < 0.01, f"MSE too high: {mse}"
assert np.allclose(model.w, true_w, atol=0.1), "Weights did not converge to ground truth!"
```

---

### Problem 2: K-Means Clustering with K-Means++ Initialization
**Problem Statement**: Implement K-Means clustering supporting K-Means++ probabilistic initialization, vectorized Euclidean distance broadcasting, and finite cluster assignment termination.

```python
import numpy as np

class ScratchKMeans:
    def __init__(self, n_clusters: int = 3, max_iter: int = 300, tol: float = 1e-4, seed: int = 42):
        self.k = n_clusters
        self.max_iter = max_iter
        self.tol = tol
        self.rng = np.random.default_rng(seed)
        self.centroids = None

    def _init_kmeans_plus_plus(self, X: np.ndarray):
        n_samples = X.shape[0]
        centroids = [X[self.rng.integers(0, n_samples)]]

        for _ in range(1, self.k):
            # Compute distance squared to closest existing centroid for all points
            current_centroids = np.array(centroids)
            # Pairwise distance: shape (N, current_k)
            dists = np.linalg.norm(X[:, np.newaxis, :] - current_centroids[np.newaxis, :, :], axis=2)
            min_dists_sq = np.min(dists, axis=1) ** 2

            # Probabilistic selection proportional to D(x)^2
            probs = min_dists_sq / np.sum(min_dists_sq)
            next_idx = self.rng.choice(n_samples, p=probs)
            centroids.append(X[next_idx])

        return np.array(centroids)

    def fit(self, X: np.ndarray):
        self.centroids = self._init_kmeans_plus_plus(X)

        for _ in range(self.max_iter):
            # Vectorized distance computation: (N, K)
            dists = np.linalg.norm(X[:, np.newaxis, :] - self.centroids[np.newaxis, :, :], axis=2)
            labels = np.argmin(dists, axis=1)

            # Recompute centroids as cluster means
            new_centroids = np.zeros_like(self.centroids)
            for cluster_idx in range(self.k):
                pts = X[labels == cluster_idx]
                new_centroids[cluster_idx] = np.mean(pts, axis=0) if len(pts) > 0 else self.centroids[cluster_idx]

            shift = np.linalg.norm(new_centroids - self.centroids)
            self.centroids = new_centroids
            if shift < self.tol:
                break
        return self

    def predict(self, X: np.ndarray) -> np.ndarray:
        dists = np.linalg.norm(X[:, np.newaxis, :] - self.centroids[np.newaxis, :, :], axis=2)
        return np.argmin(dists, axis=1)

# Verification test
X_syn = np.vstack([
    rng.normal(loc=[-5.0, -5.0], scale=0.5, size=(100, 2)),
    rng.normal(loc=[0.0, 0.0], scale=0.5, size=(100, 2)),
    rng.normal(loc=[5.0, 5.0], scale=0.5, size=(100, 2))
])
kmeans = ScratchKMeans(n_clusters=3, seed=42)
kmeans.fit(X_syn)
assert len(kmeans.centroids) == 3
assert np.all(np.sort(np.round(kmeans.centroids[:, 0])) == np.array([-5.0, 0.0, 5.0]))
```

---

### Problem 3: Decision Tree Split Finding (CART Gini Impurity)
**Problem Statement**: Implement the core split-finding engine of a CART decision tree: find the optimal feature and numerical threshold that maximizes Gini Gain in $O(d \cdot N \log N)$ time.

```python
import numpy as np

def compute_gini(y: np.ndarray) -> float:
    if len(y) == 0:
        return 0.0
    _, counts = np.unique(y, return_counts=True)
    probs = counts / len(y)
    return 1.0 - np.sum(probs ** 2)

def find_best_split(X: np.ndarray, y: np.ndarray) -> dict:
    n_samples, n_features = X.shape
    best_gain = -1.0
    best_feature = None
    best_threshold = None
    current_gini = compute_gini(y)

    for feat_idx in range(n_features):
        feature_values = X[:, feat_idx]
        sorted_indices = np.argsort(feature_values)
        sorted_x, sorted_y = feature_values[sorted_indices], y[sorted_indices]

        # Evaluate candidate thresholds midway between consecutive distinct values
        for i in range(1, n_samples):
            if sorted_x[i] == sorted_x[i - 1]:
                continue
            threshold = (sorted_x[i] + sorted_x[i - 1]) / 2.0

            left_y = sorted_y[:i]
            right_y = sorted_y[i:]

            gini_left = compute_gini(left_y)
            gini_right = compute_gini(right_y)
            weighted_gini = (i / n_samples) * gini_left + ((n_samples - i) / n_samples) * gini_right
            gain = current_gini - weighted_gini

            if gain > best_gain:
                best_gain = gain
                best_feature = feat_idx
                best_threshold = threshold

    return {"feature": best_feature, "threshold": best_threshold, "gain": best_gain}

# Verification test
X_test = np.array([[1.0], [2.0], [3.0], [8.0], [9.0], [10.0]])
y_test = np.array([0, 0, 0, 1, 1, 1])
split = find_best_split(X_test, y_test)
assert split["feature"] == 0
assert 3.0 < split["threshold"] < 8.0
assert np.isclose(split["gain"], 0.5)
```

---

### Problem 4: Intersection over Union (IoU) & Non-Maximum Suppression (NMS)
**Problem Statement**: Implement vectorized bounding box Intersection over Union (IoU) and greedy Non-Maximum Suppression (NMS) in pure NumPy for object detection.

```python
import numpy as np

def compute_iou(boxA: np.ndarray, boxB: np.ndarray) -> float:
    """Bounding boxes formatted as [x1, y1, x2, y2]."""
    xA = max(boxA[0], boxB[0])
    yA = max(boxA[1], boxB[1])
    xB = min(boxA[2], boxB[2])
    yB = min(boxA[3], boxB[3])

    intersection = max(0.0, xB - xA) * max(0.0, yB - yA)
    areaA = (boxA[2] - boxA[0]) * (boxA[3] - boxA[1])
    areaB = (boxB[2] - boxB[0]) * (boxB[3] - boxB[1])
    union = areaA + areaB - intersection
    return intersection / union if union > 0 else 0.0

def non_maximum_suppression(boxes: np.ndarray, scores: np.ndarray, iou_threshold: float = 0.5) -> list[int]:
    """Greedy NMS keeping indices of winning bounding boxes."""
    if len(boxes) == 0:
        return []

    order = np.argsort(scores)[::-1]
    keep = []

    while order.size > 0:
        i = order[0]
        keep.append(int(i))
        if order.size == 1:
            break

        # Vectorized IoU of best box with remaining boxes
        remaining = order[1:]
        xx1 = np.maximum(boxes[i, 0], boxes[remaining, 0])
        yy1 = np.maximum(boxes[i, 1], boxes[remaining, 1])
        xx2 = np.minimum(boxes[i, 2], boxes[remaining, 2])
        yy2 = np.minimum(boxes[i, 3], boxes[remaining, 3])

        w = np.maximum(0.0, xx2 - xx1)
        h = np.maximum(0.0, yy2 - yy1)
        intersection = w * h

        area_i = (boxes[i, 2] - boxes[i, 0]) * (boxes[i, 3] - boxes[i, 1])
        area_rem = (boxes[remaining, 2] - boxes[remaining, 0]) * (boxes[remaining, 3] - boxes[remaining, 1])
        union = area_i + area_rem - intersection
        ious = intersection / union

        # Retain only boxes whose IoU is below threshold
        order = remaining[ious <= iou_threshold]

    return keep

# Verification test
boxes = np.array([
    [10, 10, 50, 50],
    [12, 12, 52, 52], # Overlapping duplicate
    [100, 100, 150, 150]
], dtype=float)
scores = np.array([0.90, 0.85, 0.95])
kept = non_maximum_suppression(boxes, scores, iou_threshold=0.5)
assert kept == [2, 0], f"NMS kept unexpected indices: {kept}"
```

---

### Problem 5: Confusion Matrix & ROC-AUC from Scratch
**Problem Statement**: Implement binary confusion matrix counts ($\text{TP, FP, TN, FN}$) and exact ROC-AUC via the trapezoidal rule / Wilcoxon-Mann-Whitney rank statistic without external libraries.

```python
import numpy as np

def binary_confusion_matrix(y_true: np.ndarray, y_pred: np.ndarray) -> tuple[int, int, int, int]:
    tp = int(np.sum((y_true == 1) & (y_pred == 1)))
    fp = int(np.sum((y_true == 0) & (y_pred == 1)))
    tn = int(np.sum((y_true == 0) & (y_pred == 0)))
    fn = int(np.sum((y_true == 1) & (y_pred == 0)))
    return tp, fp, tn, fn

def compute_roc_auc_score(y_true: np.ndarray, y_score: np.ndarray) -> float:
    """Computes exact ROC-AUC using the Wilcoxon-Mann-Whitney rank sum theorem."""
    pos_mask = (y_true == 1)
    neg_mask = (y_true == 0)
    n_pos = np.sum(pos_mask)
    n_neg = np.sum(neg_mask)

    if n_pos == 0 or n_neg == 0:
        raise ValueError("Both positive and negative samples required.")

    # Sort scores in ascending order and rank from 1 to N
    ranks = np.empty_like(y_score, dtype=float)
    order = np.argsort(y_score)
    ranks[order] = np.arange(1, len(y_score) + 1)

    # Sum of ranks assigned to positive instances
    rank_sum_pos = np.sum(ranks[pos_mask])

    # U statistic formulation: AUC = (R_pos - n_pos * (n_pos + 1) / 2) / (n_pos * n_neg)
    u_stat = rank_sum_pos - (n_pos * (n_pos + 1)) / 2.0
    return float(u_stat / (n_pos * n_neg))

# Verification test
y_t = np.array([1, 0, 1, 1, 0, 0, 1, 0])
y_s = np.array([0.9, 0.1, 0.8, 0.7, 0.2, 0.4, 0.6, 0.3])
auc = compute_roc_auc_score(y_t, y_s)
assert auc == 1.0, f"Expected perfect AUC 1.0, got {auc}"
```

---

### Problem 6: Adam Optimizer Parameter Step
**Problem Statement**: Implement the exact Adam (Adaptive Moment Estimation) parameter update step with first moment $m_t$, second raw moment $v_t$, and bias correction factors.

```python
import numpy as np

class ScratchAdam:
    def __init__(self, lr: float = 0.001, beta1: float = 0.9, beta2: float = 0.999, eps: float = 1e-8):
        self.lr = lr
        self.beta1 = beta1
        self.beta2 = beta2
        self.eps = eps
        self.m = None
        self.v = None
        self.t = 0

    def step(self, param: np.ndarray, grad: np.ndarray) -> np.ndarray:
        if self.m is None:
            self.m = np.zeros_like(param)
            self.v = np.zeros_like(param)

        self.t += 1
        # Biased first and second moment estimates
        self.m = self.beta1 * self.m + (1.0 - self.beta1) * grad
        self.v = self.beta2 * self.v + (1.0 - self.beta2) * (grad ** 2)

        # Bias corrections
        m_hat = self.m / (1.0 - self.beta1 ** self.t)
        v_hat = self.v / (1.0 - self.beta2 ** self.t)

        # Update rule
        param_new = param - self.lr * m_hat / (np.sqrt(v_hat) + self.eps)
        return param_new

# Verification test
adam = ScratchAdam(lr=0.1)
w = np.array([5.0, -3.0])
for _ in range(100):
    grad = 2.0 * w # Gradients of f(w) = w^2
    w = adam.step(w, grad)
assert np.allclose(w, np.array([0.0, 0.0]), atol=1e-2), f"Adam did not converge: {w}"
```

---

### Problem 7: Beam Search Decoding for Sequence Generation
**Problem Statement**: Implement Beam Search decoding for sequence generation given a sequence length $T$, beam width $B$, and a transition probability distribution.

```python
import numpy as np

def beam_search_decoder(transition_probs: np.ndarray, beam_width: int = 3) -> list[tuple[list[int], float]]:
    """
    transition_probs: shape (seq_len, vocab_size) where entry [t, v] is log P(v | context).
    Returns top-B sequences sorted by descending log-likelihood.
    """
    seq_len, vocab_size = transition_probs.shape
    # Beams hold tuples of: (token_sequence, cumulative_log_prob)
    beams = [([], 0.0)]

    for t in range(seq_len):
        candidates = []
        for seq, score in beams:
            for v in range(vocab_size):
                cand_seq = seq + [v]
                cand_score = score + transition_probs[t, v]
                candidates.append((cand_seq, cand_score))

        # Sort all candidates and retain top B
        candidates.sort(key=lambda x: x[1], reverse=True)
        beams = candidates[:beam_width]

    return beams

# Verification test
log_probs = np.log(np.array([
    [0.1, 0.7, 0.2],
    [0.6, 0.3, 0.1],
    [0.2, 0.2, 0.6]
]))
results = beam_search_decoder(log_probs, beam_width=2)
best_seq, best_score = results[0]
assert best_seq == [1, 0, 2], f"Expected sequence [1, 0, 2], got {best_seq}"
```

---

### Problem 8: Sublinear TF-IDF Vectorizer from Scratch
**Problem Statement**: Implement a clean Text TF-IDF Vectorizer from scratch with sublinear term-frequency scaling ($1 + \ln(tf)$) and smooth IDF ($\ln((1+N)/(1+df)) + 1$).

```python
import math
import collections

class ScratchTfidfVectorizer:
    def __init__(self, sublinear_tf: bool = True):
        self.sublinear_tf = sublinear_tf
        self.vocab = {}
        self.idf = {}

    def fit_transform(self, corpus: list[str]) -> list[list[float]]:
        # Tokenize and build vocabulary
        tokenized_docs = [doc.lower().split() for doc in corpus]
        n_docs = len(corpus)

        # Count document frequency
        df_counts = collections.defaultdict(int)
        for doc in tokenized_docs:
            unique_terms = set(doc)
            for t in unique_terms:
                df_counts[t] += 1

        self.vocab = {term: idx for idx, term in enumerate(sorted(df_counts.keys()))}
        vocab_size = len(self.vocab)

        # Compute smooth IDF: ln((1 + N) / (1 + df)) + 1
        for term, df in df_counts.items():
            self.idf[term] = math.log((1.0 + n_docs) / (1.0 + df)) + 1.0

        # Construct L2-normalized TF-IDF vectors
        vectors = []
        for doc in tokenized_docs:
            vec = [0.0] * vocab_size
            tf_counts = collections.Counter(doc)
            for term, count in tf_counts.items():
                if term in self.vocab:
                    tf_val = (1.0 + math.log(count)) if self.sublinear_tf else count
                    vec[self.vocab[term]] = tf_val * self.idf[term]

            # L2 normalize
            norm = math.sqrt(sum(x ** 2 for x in vec))
            if norm > 0:
                vec = [x / norm for x in vec]
            vectors.append(vec)

        return vectors

# Verification test
corpus = ["machine learning is great", "learning machine models", "deep learning is cool"]
vectorizer = ScratchTfidfVectorizer()
X_tfidf = vectorizer.fit_transform(corpus)
assert len(X_tfidf) == 3
assert len(X_tfidf[0]) == len(vectorizer.vocab)
assert math.isclose(sum(x ** 2 for x in X_tfidf[0]), 1.0, abs_tol=1e-5)
```

---

### Problem 9: Multi-Armed Bandit Epsilon-Greedy with Decaying Schedule
**Problem Statement**: Implement an online $\epsilon$-Greedy Multi-Armed Bandit agent with incremental mean updates ($Q(a) \leftarrow Q(a) + \frac{1}{n}(r - Q(a))$) and $O(1)$ memory.

```python
import numpy as np

class ScratchBanditAgent:
    def __init__(self, k_arms: int, eps_start: float = 1.0, eps_min: float = 0.05, decay_rate: float = 0.995, seed: int = 42):
        self.k = k_arms
        self.eps = eps_start
        self.eps_min = eps_min
        self.decay = decay_rate
        self.counts = np.zeros(k_arms, dtype=int)
        self.q_values = np.zeros(k_arms, dtype=float)
        self.rng = np.random.default_rng(seed)

    def select_arm(self) -> int:
        if self.rng.uniform() < self.eps:
            return int(self.rng.integers(0, self.k))
        # Greedy selection with randomized tie-breaking
        max_q = np.max(self.q_values)
        candidates = np.where(self.q_values == max_q)[0]
        return int(self.rng.choice(candidates))

    def update(self, arm: int, reward: float):
        self.counts[arm] += 1
        # Incremental formula: Q(a) = Q(a) + (1/n) * (r - Q(a))
        self.q_values[arm] += (reward - self.q_values[arm]) / self.counts[arm]
        self.eps = max(self.eps_min, self.eps * self.decay)

# Verification test
agent = ScratchBanditAgent(k_arms=3, seed=42)
true_means = [0.2, 0.8, 0.5] # Arm 1 is optimal
for _ in range(2000):
    arm = agent.select_arm()
    reward = 1.0 if agent.rng.uniform() < true_means[arm] else 0.0
    agent.update(arm, reward)
assert agent.counts[1] > agent.counts[0] and agent.counts[1] > agent.counts[2]
```

---

### Problem 10: Scaled Dot-Product Self-Attention in Pure NumPy
**Problem Statement**: Implement the core forward pass of Scaled Dot-Product Self-Attention ($$\text{Attention}(\mathbf{Q}, \mathbf{K}, \mathbf{V}) = \text{Softmax}\left(\frac{\mathbf{Q}\mathbf{K}^T}{\sqrt{d_k}} + \mathbf{M}\right)\mathbf{V}$$) with optional causal masking and numerically stable LogSumExp/Softmax in pure NumPy.

```python
import numpy as np

def scaled_dot_product_attention(
    Q: np.ndarray, 
    K: np.ndarray, 
    V: np.ndarray, 
    mask: np.ndarray = None
) -> tuple[np.ndarray, np.ndarray]:
    """
    Q: (Batch, Seq_Q, d_k)
    K: (Batch, Seq_K, d_k)
    V: (Batch, Seq_V, d_v) where Seq_K == Seq_V
    mask: Optional binary mask (Batch, Seq_Q, Seq_K) where 0 indicates masked position.
    Returns: (output, attention_weights)
    """
    d_k = Q.shape[-1]
    
    # Compute attention scores: Q @ K.T / sqrt(d_k)
    # Shape: (Batch, Seq_Q, Seq_K)
    scores = np.matmul(Q, np.swapaxes(K, -1, -2)) / np.sqrt(d_k)

    if mask is not None:
        # Fill masked positions with -1e9 for numerical stability before softmax
        scores = np.where(mask == 0, -1e9, scores)

    # Numerically stable Softmax along the last dimension (Seq_K)
    scores_max = np.max(scores, axis=-1, keepdims=True)
    exp_scores = np.exp(scores - scores_max)
    attn_weights = exp_scores / np.sum(exp_scores, axis=-1, keepdims=True)

    # Output: attn_weights @ V -> (Batch, Seq_Q, d_v)
    output = np.matmul(attn_weights, V)
    return output, attn_weights

# Verification test
rng = np.random.default_rng(42)
B, T, D = 2, 4, 8
q = rng.normal(size=(B, T, D))
k = rng.normal(size=(B, T, D))
v = rng.normal(size=(B, T, D))

# Lower triangular causal autoregressive mask
causal_mask = np.tril(np.ones((B, T, T)))

out, weights = scaled_dot_product_attention(q, k, v, mask=causal_mask)
assert out.shape == (B, T, D)
assert np.allclose(np.sum(weights, axis=-1), 1.0) # Rows sum to 1
assert np.all(weights[:, 0, 1:] == 0.0) # Position 0 cannot attend to position 1, 2, 3
print("[SUCCESS] All 10 Machine Learning coding implementations verified and passed.")
```
