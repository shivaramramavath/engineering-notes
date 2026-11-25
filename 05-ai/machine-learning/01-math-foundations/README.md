# Module 01: Math Foundations for Machine Learning

> **Track:** Machine Learning Curriculum  
> **Module ID:** `05-ai/machine-learning/01-math-foundations`  
> **Status:** Complete (9 of 9 notes authored and verified)

---

## 1. Overview & Purpose

Module 01 provides the rigorous mathematical foundations underpinning all machine learning algorithms. It covers linear algebra (vector spaces, matrix factorizations, SVD), multivariate calculus (gradients, Jacobians, Hessians, curvature), probability theory (distributions, Bayes' theorem, CLT), statistical hypothesis testing (p-values, t-tests, ANOVA, FDR, drift detection), information theory (entropy, mutual information, KL divergence), mathematical optimization (convexity, momentum, Adam, second-order methods), the bias-variance tradeoff and modern double descent, the curse of dimensionality, and Maximum Likelihood vs Bayesian inference.

---

## 2. Note Catalog & Learning Sequence

| Note | Topic & Key Concepts | Prerequisites | Link |
| :---: | :--- | :--- | :---: |
| **01** | **Linear Algebra for ML**<br>Vector spaces, dot products, matrix rank, invertibility, eigendecomposition, SVD geometry, condition numbers. | High school algebra | [01-linear-algebra-for-ml.md](./01-linear-algebra-for-ml.md) |
| **02** | **Calculus & Gradients**<br>Partial derivatives, gradients, Jacobians, Hessians, multivariate chain rule, curvature, Taylor series, gradient checks. | Note 01 | [02-calculus-and-gradients.md](./02-calculus-and-gradients.md) |
| **03** | **Probability & Distributions**<br>Kolmogorov axioms, Bayes' theorem, expectation, covariance, Gaussian, Bernoulli, Poisson, Beta, Central Limit Theorem. | Note 02 | [03-probability-and-distributions.md](./03-probability-and-distributions.md) |
| **04** | **Statistics & Hypothesis Testing**<br>Standard errors, confidence intervals, p-values, t-tests, ANOVA, Chi-Square, Mann-Whitney U, Benjamini-Hochberg FDR, drift tests. | Note 03 | [04-statistics-and-hypothesis-testing.md](./04-statistics-and-hypothesis-testing.md) |
| **05** | **Information Theory**<br>Self-information, Shannon entropy, conditional entropy, mutual information, KL divergence, cross-entropy, decision tree splits. | Note 03 | [05-information-theory.md](./05-information-theory.md) |
| **06** | **Optimization & Gradient Descent**<br>Convexity, Batch/SGD/Mini-batch GD, Polyak momentum, Adam, AdamW, L-BFGS, saddle points, learning rate bounds. | Note 02 | [06-optimisation-and-gradient-descent.md](./06-optimisation-and-gradient-descent.md) |
| **07** | **Bias-Variance Tradeoff**<br>Mathematical decomposition of MSE, underfitting vs overfitting, validation curves, double descent phenomenon, ensemble variance reduction. | Notes 03, 06 | [07-bias-variance-tradeoff.md](./07-bias-variance-tradeoff.md) |
| **08** | **Curse of Dimensionality**<br>Hypersphere volume collapse, outer shell concentration, distance concentration theorem, manifold hypothesis, JL lemma. | Note 01 | [08-curse-of-dimensionality.md](./08-curse-of-dimensionality.md) |
| **09** | **Maximum Likelihood & Bayes**<br>MLE derivations, biased variance, MAP estimation, Gaussian prior $\equiv$ Ridge, Laplace prior $\equiv$ Lasso, full Bayesian inference. | Notes 03, 06 | [09-maximum-likelihood-and-bayes.md](./09-maximum-likelihood-and-bayes.md) |

---

## 3. Recommended Study Workflow

1. Study **Notes 01 & 02** to master matrix vectorization, projection geometry, and gradient calculus before implementing optimization routines.
2. Review **Notes 03, 04 & 05** to build an intuitive grasp of uncertainty, statistical hypothesis testing, and information-theoretic loss functions.
3. Dive into **Note 06** to understand how optimizers navigate loss surfaces, and **Note 07** to understand generalization bounds.
4. Conclude with **Notes 08 & 09** to understand high-dimensional geometry and the formal equivalence between Bayesian priors and regularized empirical loss functions.
5. Proceed to [Module 02: Data Preparation & Preprocessing](../02-data-and-preprocessing/README.md).
