# Cheatsheet 10: Machine Learning Mathematical Foundations Reference

> **Cheatsheet ID:** `05-ai/machine-learning/18-cheatsheets/10-math-formulas`  
> **Topic:** Comprehensive Formula Reference: Linear Algebra, Calculus, Probability & Information Theory  
> **Status:** Production Verified

---

## 1. Linear Algebra & Matrix Calculus

### 1.1 Vector & Matrix Norms
* **$L_p$ Vector Norm**: $\|\mathbf{x}\|_p = \left( \sum_{i=1}^d |x_i|^p \right)^{1/p}$. ($L_1 = \sum |x_i|$, $L_2 = \sqrt{\sum x_i^2}$, $L_\infty = \max |x_i|$).
* **Frobenius Matrix Norm**: $\|\mathbf{A}\|_F = \sqrt{\sum_{i=1}^m \sum_{j=1}^n A_{ij}^2} = \sqrt{\text{Tr}(\mathbf{A}^T \mathbf{A})}$.
* **Spectral Norm**: $\|\mathbf{A}\|_2 = \sigma_{\max}(\mathbf{A}) = \sqrt{\lambda_{\max}(\mathbf{A}^T \mathbf{A})}$.

### 1.2 Matrix Calculus Derivative Rules
$$\nabla_{\mathbf{x}} (\mathbf{x}^T \mathbf{a}) = \mathbf{a}, \quad \nabla_{\mathbf{x}} (\mathbf{a}^T \mathbf{x}) = \mathbf{a}$$
$$\nabla_{\mathbf{x}} (\mathbf{x}^T \mathbf{A} \mathbf{x}) = (\mathbf{A} + \mathbf{A}^T) \mathbf{x} \quad (\text{if } \mathbf{A} \text{ symmetric: } 2\mathbf{A}\mathbf{x})$$
$$\nabla_{\mathbf{X}} \text{Tr}(\mathbf{A}\mathbf{X}\mathbf{B}) = \mathbf{A}^T \mathbf{B}^T, \quad \nabla_{\mathbf{X}} \ln |\det(\mathbf{X})| = (\mathbf{X}^{-1})^T$$

### 1.3 Spectral Decompositions
* **Eigendecomposition (Symmetric Matrix $\mathbf{A} \in \mathbb{R}^{d \times d}$)**:
  $$\mathbf{A} = \mathbf{Q} \mathbf{\Lambda} \mathbf{Q}^T = \sum_{i=1}^d \lambda_i \mathbf{q}_i \mathbf{q}_i^T \quad \text{where } \mathbf{Q}^T \mathbf{Q} = \mathbf{I}$$
* **Singular Value Decomposition (SVD for any $\mathbf{X} \in \mathbb{R}^{N \times d}$)**:
  $$\mathbf{X} = \mathbf{U} \mathbf{\Sigma} \mathbf{V}^T = \sum_{i=1}^{\min(N, d)} \sigma_i \mathbf{u}_i \mathbf{v}_i^T$$
* **Positive Semi-Definiteness (PSD)**:
  $\mathbf{A} \succeq 0 \iff \mathbf{x}^T \mathbf{A} \mathbf{x} \ge 0 \quad \forall \mathbf{x} \ne \mathbf{0} \iff \text{all eigenvalues } \lambda_i \ge 0$.

---

## 2. Multivariable Calculus & Optimization Geometry

### 2.1 Multivariable Taylor Series Expansion
For twice-differentiable scalar field $f: \mathbb{R}^d \to \mathbb{R}$ around point $\mathbf{x}_0$:
$$f(\mathbf{x}) \approx f(\mathbf{x}_0) + \nabla f(\mathbf{x}_0)^T (\mathbf{x} - \mathbf{x}_0) + \frac{1}{2} (\mathbf{x} - \mathbf{x}_0)^T \nabla^2 f(\mathbf{x}_0) (\mathbf{x} - \mathbf{x}_0)$$
where $\nabla f \in \mathbb{R}^d$ is the gradient vector and $\nabla^2 f \in \mathbb{R}^{d \times d}$ is the symmetric Hessian matrix:
$$H_{ij} = \frac{\partial^2 f}{\partial x_i \partial x_j}$$

### 2.2 Convexity & Condition Number Bounds
* **$\mu$-Strong Convexity**: $\nabla^2 f(\mathbf{x}) \succeq \mu \mathbf{I} \implies f(\mathbf{y}) \ge f(\mathbf{x}) + \nabla f(\mathbf{x})^T(\mathbf{y}-\mathbf{x}) + \frac{\mu}{2}\|\mathbf{y}-\mathbf{x}\|_2^2$.
* **$L$-Lipschitz Smoothness**: $\nabla^2 f(\mathbf{x}) \preceq L \mathbf{I} \implies \|\nabla f(\mathbf{x}) - \nabla f(\mathbf{y})\|_2 \le L \|\mathbf{x} - \mathbf{y}\|_2$.
* **Hessian Condition Number**: $\kappa = \frac{\lambda_{\max}(\mathbf{H})}{\lambda_{\min}(\mathbf{H})} = \frac{L}{\mu}$.
* **Gradient Descent Convergence Rate**:
  $$\|\mathbf{x}_t - \mathbf{x}^*\|_2 \le \left( \frac{\kappa - 1}{\kappa + 1} \right)^t \|\mathbf{x}_0 - \mathbf{x}^*\|_2$$

---

## 3. Probability & Continuous Distributions

| Distribution | Probability Density Function $p(x)$ | Mean $\mathbb{E}[X]$ | Variance $\text{Var}(X)$ | Core Use in ML |
| :--- | :---: | :---: | :---: | :--- |
| **Multivariate Normal** $\mathcal{N}(\boldsymbol{\mu}, \boldsymbol{\Sigma})$ | $\frac{1}{(2\pi)^{d/2} \|\boldsymbol{\Sigma}\|^{1/2}} \exp\left(-\frac{1}{2}(\mathbf{x}-\boldsymbol{\mu})^T \boldsymbol{\Sigma}^{-1} (\mathbf{x}-\boldsymbol{\mu})\right)$ | $\boldsymbol{\mu}$ | $\boldsymbol{\Sigma}$ | Gaussian processes, VAEs, GMMs, Linear regression noise. |
| **Bernoulli** | $p^x (1 - p)^{1-x} \quad \text{for } x \in \{0, 1\}$ | $p$ | $p(1 - p)$ | Binary classification ground truth, Dropout mask. |
| **Binomial** | $\binom{n}{k} p^k (1 - p)^{n - k}$ | $n p$ | $n p (1 - p)$ | Conversion modeling, A/B test sample sizing. |
| **Poisson** | $\frac{\lambda^k e^{-\lambda}}{k!}$ | $\lambda$ | $\lambda$ | Retail store demand counts, server request arrivals. |
| **Beta** $\text{Beta}(\alpha, \beta)$ | $\frac{1}{\text{B}(\alpha, \beta)} x^{\alpha - 1} (1 - x)^{\beta - 1}$ | $\frac{\alpha}{\alpha + \beta}$ | $\frac{\alpha \beta}{(\alpha + \beta)^2 (\alpha + \beta + 1)}$ | Conjugate prior for Bernoulli bandits, Mixup interpolation. |
| **Dirichlet** $\text{Dir}(\boldsymbol{\alpha})$ | $\frac{1}{\text{B}(\boldsymbol{\alpha})} \prod_{k=1}^K x_k^{\alpha_k - 1}$ | $\frac{\alpha_k}{\sum \alpha_j}$ | Covariance matrix | Conjugate prior for multinomial text topic models (LDA). |

---

## 4. Information Theory & Entropy

* **Shannon Entropy**:
  $$H(X) = -\sum_{x \in \mathcal{X}} P(x) \log_2 P(x) = \mathbb{E}[-\log_2 P(X)]$$
* **Joint Entropy & Conditional Entropy**:
  $$H(X, Y) = -\sum \sum P(x, y) \log_2 P(x, y), \quad H(Y \mid X) = H(X, Y) - H(X)$$
* **Mutual Information**:
  $$I(X; Y) = H(X) - H(X \mid Y) = \text{KL}(P(X, Y) \parallel P(X)P(Y))$$
* **Kullback-Leibler (KL) Divergence**:
  $$\text{KL}(P \parallel Q) = \sum_{x \in \mathcal{X}} P(x) \ln \frac{P(x)}{Q(x)} \ge 0 \quad (\text{Gibbs' Inequality})$$
* **Cross-Entropy**:
  $$H(P, Q) = H(P) + \text{KL}(P \parallel Q) = -\sum_{x \in \mathcal{X}} P(x) \ln Q(x)$$

---

## 5. Optimization Update Rules

* **Momentum**:
  $$\mathbf{v}_t = \gamma \mathbf{v}_{t-1} + \eta \nabla L(\mathbf{w}_t), \quad \mathbf{w}_{t+1} = \mathbf{w}_t - \mathbf{v}_t$$
* **Nesterov Accelerated Gradient (NAG)**:
  $$\mathbf{v}_t = \gamma \mathbf{v}_{t-1} + \eta \nabla L(\mathbf{w}_t - \gamma \mathbf{v}_{t-1}), \quad \mathbf{w}_{t+1} = \mathbf{w}_t - \mathbf{v}_t$$
* **AdamW (Decoupled Weight Decay)**:
  $$\mathbf{m}_t = \beta_1 \mathbf{m}_{t-1} + (1 - \beta_1) \mathbf{g}_t, \quad \mathbf{v}_t = \beta_2 \mathbf{v}_{t-1} + (1 - \beta_2) \mathbf{g}_t^2$$
  $$\hat{\mathbf{m}}_t = \frac{\mathbf{m}_t}{1 - \beta_1^t}, \quad \hat{\mathbf{v}}_t = \frac{\mathbf{v}_t}{1 - \beta_2^t}$$
  $$\mathbf{w}_{t+1} = (1 - \eta \lambda) \mathbf{w}_t - \frac{\eta}{\sqrt{\hat{\mathbf{v}}_t} + \epsilon} \hat{\mathbf{m}}_t$$
