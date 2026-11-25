# Module 05: Unsupervised Learning

Welcome to **Module 05: Unsupervised Learning** of the Machine Learning Curriculum track.

Unsupervised learning algorithms uncover latent structural patterns, clusters, density manifolds, and compressed representations in unlabeled data $\mathbf{X} \in \mathbb{R}^{n \times d}$ without guidance from target labels $y$. This module covers the foundational theory, mathematical derivations, NumPy implementations from first principles, production Scikit-Learn pipelines, diagnostic evaluation frameworks, failure modes, and technical interview preparation across the entire unsupervised landscape.

---

## Module Notes Overview

| Order | Note | Format | Level | Focus & Key Algorithms |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [What is Unsupervised Learning?](01-what-is-unsupervised-learning.md) | Concept | Beginner | Taxonomy: Clustering, Dimensionality Reduction, Density Estimation, Association Rules; Inductive biases; Information-theoretic foundations. |
| **02** | [K-Means & Mini-Batch K-Means](02-k-means.md) | Algorithm | Beginner | Lloyd's algorithm, Voronoi tessellation, K-Means++ initialization, MiniBatch streaming, Inertia, failure on non-convex manifolds. |
| **03** | [Hierarchical Clustering](03-hierarchical-clustering.md) | Algorithm | Intermediate | Agglomerative vs Divisive, Ward's minimum variance, Single/Complete/Average linkage, Cophenetic correlation, Dendrogram slicing. |
| **04** | [DBSCAN & HDBSCAN](04-dbscan-and-hdbscan.md) | Algorithm | Intermediate | Density reachability, core/border/noise points, spatial index acceleration, HDBSCAN condensed cluster hierarchy, DBCV validation. |
| **05** | [Gaussian Mixture Models (GMM) & EM](05-gaussian-mixture-models-and-em.md) | Algorithm | Intermediate | Soft probabilistic clustering, Expectation-Maximization (EM), complete data log-likelihood, covariance types (full, diag, tied, spherical). |
| **06** | [Spectral Clustering](06-spectral-clustering.md) | Algorithm | Advanced | Graph Laplacians (unnormalized, symmetric, random walk), Cheeger's inequality, Normalized Cut (NCut), manifold embedding. |
| **07** | [Choosing K & Cluster Validation](07-choosing-k-and-cluster-validation.md) | Concept | Intermediate | Internal vs External metrics: Silhouette score, Davies-Bouldin, Calinski-Harabasz, Adjusted Rand Index (ARI), Normalized Mutual Info (NMI). |
| **08** | [Principal Component Analysis (PCA)](08-pca.md) | Algorithm | Beginner | Variance maximization, orthogonal projection, spectral decomposition of covariance matrix, SVD formulation, scree plots, whitening. |
| **09** | [SVD & Truncated SVD](09-svd-and-truncated-svd.md) | Algorithm | Intermediate | Singular Value Decomposition, Eckart-Young-Mirsky low-rank theorem, Latent Semantic Analysis (LSA) on sparse matrices without centering. |
| **10** | [t-SNE & UMAP](10-t-sne-and-umap.md) | Algorithm | Advanced | Manifold learning: Student-t distribution, Kullback-Leibler divergence gradient, fuzzy simplicial sets, Riemannian geometry, local vs global topology. |
| **11** | [ICA & Factor Analysis](11-ica-and-factor-analysis.md) | Algorithm | Advanced | Blind source separation, non-Gaussianity maximization (kurtosis, negentropy), FastICA, latent common factors, noise covariance. |
| **12** | [Anomaly Detection](12-anomaly-detection.md) | Algorithm | Intermediate | Isolation Forest (iForest path length), Local Outlier Factor (LOF local reachability), One-Class SVM (hyperplane margin), threshold calibration. |
| **13** | [Association Rule Mining](13-association-rules.md) | Algorithm | Intermediate | Market Basket Analysis, Apriori principle (downward-closure lattice pruning), FP-Growth prefix-trees, Support, Confidence, Lift, Conviction. |
| **14** | [Kernel Density Estimation (KDE)](14-density-estimation.md) | Algorithm | Intermediate | Parzen-Rosenblatt window, MISE/AMISE bias-variance trade-off, Gaussian/Epanechnikov kernels, Silverman/Scott bandwidth heuristics, BallTree speedup. |
| **15** | [Topic Modelling: Latent Dirichlet Allocation (LDA)](15-topic-modelling-lda.md) | Algorithm | Advanced | Generative hierarchical Bayesian models, Dirichlet priors ($\alpha, \beta$), Collapsed Gibbs Sampling, Online Variational Bayes, Perplexity, $U_{\text{mass}}$ coherence. |
| **16** | [Recommender Systems Basics](16-recommender-systems-basics.md) | Algorithm | Intermediate | Collaborative Filtering (User/Item-based), Matrix Factorization (ALS with biases), implicit feedback confidence weighting, NDCG@K, two-stage retrieval. |
| **17** | [Unsupervised Preprocessing & Pitfalls](17-unsupervised-preprocessing-and-pitfalls.md) | Concept | Core | Distance concentration in high dimensions, sparsity-centering memory blowup, scaling pitfalls, metric distortions, leakage-safe pipelines. |

---

## Architectural Decision Flowchart

```
+---------------------------------------------------------------------------------------------------------+
|                               UNSUPERVISED LEARNING MASTER TAXONOMY                                     |
|                                                                                                         |
|                                  What is your primary analytical goal?                                  |
|                 /                   |                       |                      \                    |
|                /                    |                       |                       \                   |
|      [Group Data Points]   [Reduce Dimensions]    [Estimate Density / Anomaly]   [Discover Rules]       |
|              |                      |                       |                           |               |
|      Are clusters convex?    Is data linear?         Is goal binary anomaly?    Frequent Itemsets:      |
|         /           \           /         \               /              \       Apriori / FP-Growth    |
|      [YES]          [NO]      [YES]       [NO]          [YES]            [NO]                           |
|       /               \        /            \            /                 \                            |
|    K-Means /      DBSCAN /    PCA /       UMAP /     Isolation Forest /  Kernel Density                 |
|    GMM (Soft)     HDBSCAN     SVD (Sparse) t-SNE     LOF / One-Class SVM Estimation (KDE)               |
+---------------------------------------------------------------------------------------------------------+
```

---

## Module Standards & Verification

- **Pure NumPy Implementations**: Every algorithm note contains a self-contained, mathematically rigorous implementation written from first principles.
- **Production Pipelines**: Demonstrates leakage-safe Scikit-Learn pipelines integrating robust scaling, spatial trees, and cross-validated tuning.
- **Cryptographic Model Integrity**: SHA-256 sidecar generation for all serialized unsupervised artifacts.
- **Interview & Practice Mastery**: 8 advanced interview questions with formal mathematical proofs and 6 hands-on coding exercises per note.
