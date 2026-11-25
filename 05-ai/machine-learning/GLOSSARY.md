# Machine Learning Curriculum Glossary of Terms

> **Track ID:** `05-ai/machine-learning/GLOSSARY`  
> **Status:** Production Verified  
> **Coverage:** Comprehensive Definitions from A to Z across Statistical Learning, Systems & Optimization

---

### A
* **Activation Function**: Non-linear mathematical mapping applied to a neuron's linear combination (e.g., ReLU, GELU, Sigmoid) enabling neural networks to learn non-linear functions (Universal Approximation Theorem).
* **Actor-Critic**: Reinforcement learning architecture featuring two components: an Actor that parameterizes the policy $\pi_\theta(a \mid s)$, and a Critic that estimates value $V_\phi(s)$ or advantage $A(s, a)$.
* **Adjusted Rand Index (ARI)**: A chance-corrected measure of similarity between two cluster assignments; ranges from $-1.0$ (complete disagreement) to $1.0$ (perfect clustering), with expected value $0.0$ for random partitions.
* **Approximate Nearest Neighbors (ANN)**: High-speed heuristic retrieval algorithms (e.g., HNSW, ScaNN, Faiss) that find nearest neighbor vectors in sub-linear time ($O(\log N)$) with small loss in recall.
* **Autoregressive Model**: Time-series model where the current value $y_t$ is predicted as a linear combination of its own historical lags: $y_t = c + \sum \phi_i y_{t-i} + \epsilon_t$.

---

### B
* **Backpropagation**: Reverse-mode automatic differentiation algorithm that applies the multivariable chain rule to compute loss gradients with respect to all layer parameters in a single backward pass.
* **Bagging (Bootstrap Aggregating)**: Ensemble technique that trains multiple high-capacity estimators on independent bootstrap resamples in parallel, averaging predictions to reduce model variance.
* **Bayes Optimal Classifier**: Theoretical upper bound on classification performance that assigns each query $\mathbf{x}$ to the class maximizing posterior probability $\arg\max_k P(Y=k \mid X=\mathbf{x})$; achieves the irreducible Bayes Error Rate.
* **Beam Search**: Heuristic graph search algorithm for sequence generation that retains the top $B$ most promising partial candidate hypotheses at each time step.
* **Bellman Optimality Equation**: Non-linear recursive dynamic programming equation stating that the value of a state under an optimal policy equals the expected immediate reward plus the discounted value of the optimal successor state.
* **Bird's Eye View (BEV)**: Top-down 2D spatial representation constructed by projecting multi-camera surround perspective views into unified metric ground coordinates.
* **Brier Score**: Proper scoring rule evaluating probabilistic forecasts defined as mean squared error between predicted probabilities and binary ground truth: $\frac{1}{N} \sum (p_i - y_i)^2$.

---

### C
* **CatBoost**: High-performance gradient boosted decision tree library specialized in handling categorical features using target statistics computed over random data permutations to eliminate target leakage.
* **Category Cardinality**: The number of unique discrete levels or values in a categorical feature column.
* **ColumnTransformer**: Scikit-Learn composition utility that applies distinct preprocessing pipelines to specified column subsets in parallel and concatenates the resulting arrays.
* **Conformal Prediction**: Non-parametric framework that wraps any point-prediction model to generate mathematically guaranteed prediction sets or intervals satisfying $\mathbb{P}(y \in C(\mathbf{x})) \ge 1 - \alpha$ under zero distributional assumptions.
* **Confusion Matrix**: Contingency table reporting True Positives, False Positives, True Negatives, and False Negatives produced by a classification decision boundary.
* **Covariate Shift**: Distribution shift where input feature distribution changes ($P_{\text{train}}(X) \ne P_{\text{test}}(X)$) while the underlying conditional causal mechanism $P(Y \mid X)$ remains unchanged.
* **Curse of Dimensionality**: Phenomenon where the volume of feature space grows exponentially with dimension $d$, causing data points to become sparse, isolated in hypersphere corners, and equidistant under Euclidean metrics.

---

### D
* **Deadly Triad**: The dangerous combination in Reinforcement Learning of (1) Function Approximation, (2) Bootstrapping, and (3) Off-Policy Learning, which can cause value estimates to diverge to infinity.
* **Decision Boundary**: Hyperplane or surface partitioning feature space into distinct predicted class regions.
* **Differential Privacy**: Mathematical standard guaranteeing that an algorithm's output distribution changes by at most $e^\epsilon$ whether any single individual's record is present or absent in the training dataset.
* **DBSCAN**: Density-Based Spatial Clustering of Applications with Noise; clusters continuous dense regions separated by sparse noise using neighborhood radius $\epsilon$ and point threshold $\text{MinPts}$.
* **Double DQN**: Deep Q-Network enhancement that decouples action selection ($\arg\max Q_\theta$) from action value evaluation ($Q_{\theta^-}$) to eliminate statistical maximization bias.
* **DVC (Data Version Control)**: Git-compatible tool that versions multi-gigabyte dataset binaries via content-addressable storage pointers (`.dvc` files) on remote cloud object storage.

---

### E
* **Early Stopping**: Regularization technique that halts iterative gradient optimization when validation error fails to improve after a predetermined patience window, acting implicitly as L2 weight decay.
* **Eckart-Young-Mirsky Theorem**: Fundamental linear algebra theorem proving that truncated SVD provides the globally optimal low-rank matrix approximation under both Frobenius and spectral norms.
* **ElasticNet**: Regularized linear model combining both L1 (Lasso) and L2 (Ridge) penalties to produce sparse feature selection while preserving correlated feature groups.
* **Empirical Risk Minimization (ERM)**: Optimization paradigm that seeks model parameters minimizing the average loss over the observed training dataset.
* **Expected Calibration Error (ECE)**: Weighted average difference between predicted confidence probabilities and observed accuracy across binned reliability intervals.

---

### F
* **F-Score ($F_\beta$)**: Weighted harmonic mean of Precision and Recall, where parameter $\beta$ weights Recall $\beta$ times as heavily as Precision.
* **Feature Store**: Centralized data management system (e.g., Feast) that manages feature definitions, computes offline features for training via point-in-time joins, and serves online features for low-latency inference.
* **Friedman's GBDT**: Gradient Boosted Decision Tree algorithm where base learners are sequentially fit to pseudo-residuals (negative loss gradients).

---

### G
* **Gaussian Mixture Model (GMM)**: Probabilistic generative model representing the data distribution as a weighted sum of $K$ multivariate Gaussian components, optimized via Expectation-Maximization (EM).
* **Generalization Gap**: The difference between a model's empirical training error and its expected error on unseen test data drawn from the same underlying distribution.
* **Gini Impurity**: Impurity criterion used in CART classification trees measuring the probability of misclassifying a randomly chosen element if labeled according to the node's class distribution: $1 - \sum p_k^2$.
* **Gradient Accumulation**: Technique in deep learning where loss gradients from multiple micro-batches are summed before calling `optimizer.step()`, effectively emulating massive batch sizes on memory-constrained GPUs.

---

### H
* **Hessian Matrix**: Square $d \times d$ matrix of second-order partial derivatives describing the local curvature of a scalar loss function: $H_{ij} = \frac{\partial^2 f}{\partial x_i \partial x_j}$.
* **HNSW (Hierarchical Navigable Small World)**: Multi-layer graph data structure enabling logarithmic-time approximate nearest neighbor vector search.
* **Huber Loss**: Smooth loss function that is quadratic for small residuals ($|u| \le \delta$) and linear for large residuals ($|u| > \delta$), combining the convergence speed of MSE with the outlier robustness of MAE.

---

### I
* **Inductive Bias**: The set of assumptions an algorithm uses to predict outputs for unseen inputs outside its training dataset.
* **Information Gain**: Expected reduction in information entropy achieved by partitioning a dataset on a given feature split: $H(\text{Parent}) - \sum \frac{N_j}{N} H(\text{Child}_j)$.
* **Isolation Forest**: Unsupervised tree-based anomaly detection algorithm that isolates outliers using few random orthogonal partition cuts, scoring anomaly by average leaf path length $h(x)$.
* **Iteratively Reweighted Least Squares (IRLS)**: Newton-Raphson optimization method for Generalized Linear Models (e.g., Logistic Regression) that solves a weighted least squares problem at each step.

---

### K
* **K-Means++**: Initialization algorithm that selects subsequent cluster centroids with probability proportional to their squared distance to the closest existing centroid, guaranteeing an $O(\log K)$ approximation bound.
* **KKT (Karush-Kuhn-Tucker) Conditions**: First-order necessary conditions for a non-linear optimization problem to be optimal under inequality and equality constraints.
* **Kullback-Leibler (KL) Divergence**: Asymmetric information-theoretic measure of relative entropy quantifying the excess surprise incurred by using distribution $Q$ to approximate true distribution $P$: $\mathbb{E}_P[\ln(P/Q)]$.

---

### L
* **LambdaMART**: State-of-the-art Learning-to-Rank algorithm that trains boosted decision trees on virtual gradients ($\lambda$-gradients) scaled by pairwise changes in NDCG.
* **Lasso (L1 Regularization)**: Regularization technique adding an $L_1$ penalty $\|\mathbf{w}\|_1$ to the loss, producing geometric sparsity by setting non-informative feature weights identically to zero.
* **Lift**: In marketing and response modeling, the ratio of target responders captured in a given top decile of model-ranked prospects compared to random selection.
* **Lipschitz Continuity**: Bound ensuring that a function does not change arbitrarily fast: $\|f(\mathbf{x}) - f(\mathbf{y})\| \le L \|\mathbf{x} - \mathbf{y}\|$.

---

### M
* **Markov Decision Process (MDP)**: 5-tuple $\langle \mathcal{S}, \mathcal{A}, \mathcal{P}, \mathcal{R}, \gamma \rangle$ formalizing sequential decision making where next state transitions satisfy the memoryless Markov property.
* **Matthews Correlation Coefficient (MCC)**: Pearson correlation coefficient between predicted and true binary labels; symmetrical across all four confusion matrix quadrants.
* **Maximum A Posteriori (MAP)**: Bayesian parameter estimation technique that maximizes the product of the data likelihood and parameter prior distribution: $\arg\max_{\mathbf{w}} P(\mathcal{D} \mid \mathbf{w}) P(\mathbf{w})$.
* **Mean Absolute Scaled Error (MASE)**: Scale-independent forecasting metric comparing forecast MAE to in-sample naive persistence MAE; values $< 1.0$ indicate superiority over naive baselines.
* **Model Card**: Standardized technical fact sheet documenting a model's architecture, intended use, evaluated demographic slices, performance metrics, and ethical limitations.

---

### N
* **Nesterov Accelerated Gradient (NAG)**: Momentum optimization enhancement that computes gradients "ahead" at the predicted lookahead position $\mathbf{w} - \gamma \mathbf{v}_{t-1}$, dampening oscillations.
* **Normalized Discounted Cumulative Gain (NDCG)**: Graded relevance ranking metric that rewards placing highly relevant items at top ranks using a logarithmic position discount: $\frac{\text{DCG}}{\text{IDCG}}$.

---

### O
* **One-Hot Encoding**: Categorical representation that converts nominal categories into mutually exclusive binary vectors.
* **ONNX (Open Neural Network Exchange)**: Open standard format representing machine learning computational graphs independently of the authoring framework, optimized for C++ inference.
* **Out-of-Bag (OOB) Error**: Cross-validation proxy in bagging ensembles computed by evaluating each tree strictly on the $\approx 36.8\%$ of training samples excluded from its bootstrap sample.

---

### P
* **PAC (Probably Approximately Correct) Learning**: Theoretical framework bounding the sample size $N$ required for an algorithm to find a hypothesis with error $\le \epsilon$ with probability $\ge 1 - \delta$.
* **Permutation Feature Importance (PFI)**: Model-agnostic inspection technique measuring the drop in held-out test score when a single feature column is randomly shuffled.
* **Population Stability Index (PSI)**: Metric measuring the divergence between a reference feature distribution and a live production inference distribution to detect covariate shift.
* **Proximal Policy Optimization (PPO)**: Actor-critic reinforcement learning algorithm utilizing a clipped surrogate objective to enforce trust-region policy updates with first-order gradient descent.

---

### Q
* **Q-Learning**: Off-policy temporal difference algorithm that learns optimal action values $Q^*(s, a)$ by bootstrapping against the maximum estimated value of the next state: $r + \gamma \max_{a'} Q(s', a')$.
* **Quantization**: Mapping high-precision 32-bit floating point weights and activations to lower-precision 8-bit integers (INT8) to accelerate inference throughput and reduce memory bandwidth.

---

### R
* **Rademacher Complexity**: Statistical capacity measure quantifying a function class's ability to fit random binary noise vectors on a specific dataset distribution.
* **Receiver Operating Characteristic (ROC)**: Curve plotting True Positive Rate against False Positive Rate across all continuous classification thresholds.
* **Replay Buffer**: FIFO memory queue in off-policy deep reinforcement learning that stores historical transitions $(s, a, r, s', d)$ to break temporal correlation during mini-batch gradient updates.
* **Ridge Regression (L2 Regularization)**: Linear model penalized by squared L2 norm $\|\mathbf{w}\|_2^2$, which shrinks parameter weights along low-variance principal components.
* **Robbins-Monro Conditions**: Mathematical conditions ($\sum \alpha_t = \infty, \sum \alpha_t^2 < \infty$) on learning rate schedules guaranteeing the asymptotic convergence of stochastic approximation algorithms.

---

### S
* **Safetensors**: Fast, zero-copy, memory-mapped tensor serialization format designed to eliminate arbitrary code execution vulnerabilities in machine learning weight storage.
* **SHAP (Shapley Additive Explanations)**: Game-theoretic framework that allocates additive feature attribution values satisfying four foundational axioms: Efficiency, Symmetry, Dummy, and Additivity.
* **Single Linkage**: Hierarchical clustering metric that defines distance between two clusters as the minimum distance between any pair of points; prone to chaining artifacts.
* **Softmax**: Multi-class logistic generalization mapping an arbitrary logit vector $\mathbf{z} \in \mathbb{R}^K$ to a normalized probability simplex: $\frac{e^{z_k}}{\sum e^{z_j}}$.
* **Spectral Clustering**: Non-linear clustering technique that performs K-Means on the eigenvectors of the normalized Graph Laplacian matrix, segmenting complex non-convex manifolds.
* **Support Vectors**: The critical subset of training data points that lie directly on or violate the margin boundaries in an SVM, uniquely defining the separating hyperplane.

---

### T
* **Target Encoding**: Preprocessing method that replaces categorical values with the conditional expected target value $\mathbb{E}[y \mid x = c]$, requiring Bayesian smoothing to prevent target leakage.
* **Thompson Sampling**: Bayesian multi-armed bandit algorithm that selects arms randomly proportional to their posterior probability of being optimal by sampling from conjugate priors (e.g., $\text{Beta}(\alpha, \beta)$).
* **Treelite**: Universal tree compiler that translates Random Forests and Gradient Boosted Decision Trees directly into branch-free, parallelized C code for microsecond inference.
* **Truncated SVD**: Factorization algorithm that computes the top $K$ singular vectors directly on sparse Compressed Sparse Row (CSR) matrices without centering.

---

### U
* **UMAP (Uniform Manifold Approximation and Projection)**: Non-linear dimensionality reduction algorithm based on Riemannian geometry and fuzzy simplicial sets that preserves both local and global manifold structure.
* **Universal Approximation Theorem**: Fundamental theorem proving that a feedforward neural network with a single hidden layer and non-linear activation functions can approximate any continuous function on a compact domain to arbitrary precision.

---

### V
* **Vapnik-Chervonenkis (VC) Dimension**: Measure of the capacity of a binary hypothesis class defined as the cardinality of the largest set of points that the class can shatter.
* **Variance Inflation Factor (VIF)**: Collinearity diagnostic measuring the inflation of a coefficient's variance due to linear correlation with other predictors: $\frac{1}{1 - R_j^2}$.

---

### W
* **Weighted Absolute Percentage Error (WAPE)**: Supply chain demand forecasting error metric defined as $\frac{\sum |y - \hat{y}|}{\sum y} \times 100\%$, robust to intermittent zero sales.
* **Winsorization**: Outlier management strategy that clamps extreme continuous values at specified lower and upper percentiles (e.g., 1st and 99th percentiles) rather than discarding rows.

---

### X
* **XGBoost (eXtreme Gradient Boosting)**: Distributed gradient boosting library that applies a second-order Taylor expansion to the objective function, optimizing split gains using exact first-order ($g_i$) and second-order ($h_i$) loss derivatives.

---

### Z
* **Zero-Shot Learning**: Learning setting where a model predicts classes that were completely absent from its training set, typically achieved by mapping instances and class descriptors into a shared semantic embedding space.
