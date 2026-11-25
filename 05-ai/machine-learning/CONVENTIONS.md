# Machine Learning Track: Conventions & Editorial Guidelines

> **Area:** `05-ai`  
> **Track:** `machine-learning`  
> **Target Python:** 3.11 / 3.12 Baseline  
> **Status:** Active Standard

---

## 1. Naming & Directory Structure

* **Directory & File Names:** Strict `NN-kebab-case` starting from `01-` for files, `00-` for modules.
* **Module Folder Pattern:** `00-setup/`, `01-math-foundations/`, `02-data-and-preprocessing/`, etc.
* **Note Suffix:** All files must end with `.md`. Never commit Python scripts (`.py`), Jupyter notebooks (`.ipynb`), datasets (`.csv`), or binary models (`.pkl`, `.onnx`) directly into the repository.
* **Code Containment:** All code, configs, test outputs, and system units reside exclusively in fenced code blocks inside markdown notes under path comments or section headers.
* **Navigation Links:** Every note ends with a 3-way navigation bar: `[← Previous](relative-link) | [Module Index](README.md) | [Next →](relative-link)`.

---

## 2. Metadata Schema

Every note must feature a standardized metadata block immediately below the H1 heading:

### 2.1 Algorithm Note Metadata Header
```markdown
> **Topic:** Algorithm Common Name  
> **Module ID:** `05-ai/machine-learning/NN-module-name`  
> **Target Version:** Python 3.11+, scikit-learn 1.9+, NumPy 2.5+, pandas 3.1+  
> **Dataset:** Dataset Name / Synthetic (`make_classification`, `load_diabetes`, etc.)  
> **Random Seed:** `42`  
> **Level:** Beginner | Intermediate | Advanced  
> **Prerequisites:** [Relative Link to Prerequisite Note](../path/to/note.md)  
> **Status:** Verified (Python 3.11 / CPU)
```

### 2.2 Concept Note Metadata Header
```markdown
> **Topic:** Concept Name  
> **Module ID:** `05-ai/machine-learning/NN-module-name`  
> **Target Version:** Python 3.11+, scikit-learn 1.9+, NumPy 2.5+, pandas 3.1+  
> **Level:** Fundamental | Core | Advanced  
> **Prerequisites:** [Relative Link to Prerequisite Note](../path/to/note.md)  
> **Status:** Verified (Python 3.11 / CPU)
```

---

## 3. Mathematical Notation Standards

To ensure rendering across GitHub, IDEs, and documentation generators:
* Use standard LaTeX math: inline `$formula$` or `\(formula\)`; block `$$formula$$` or `\[formula\]`.
* Matrices: Uppercase bold or standard uppercase (e.g. $\mathbf{X} \in \mathbb{R}^{n \times d}$ or $X \in \mathbb{R}^{n \times d}$).
* Vectors: Lowercase bold or standard lowercase (e.g. $\mathbf{y} \in \mathbb{R}^n$, $\mathbf{w} \in \mathbb{R}^d$, $\mathbf{x}_i$).
* Scalar parameters & errors: Lowercase Greek or Latin (e.g. $\epsilon_i$, $\alpha$, $\lambda$, $\sigma^2$).
* Loss functions: $L(y, \hat{y})$ or $J(\mathbf{w})$.
* Every formula must be accompanied by an explicit symbol explanation table defining every variable, dimension, and operator.

---

## 4. Code & Workflow Standards

### 4.1 Strict Leakage-Safe Workflow
Every supervised and unsupervised learning example must demonstrate a rigorous train-validation-test split before transformer fitting:
```python
# Rule: Transformers fit ONLY on training data, never on validation/test sets!
scaler = StandardScaler()
X_train_scaled = scaler.fit_transform(X_train)
X_test_scaled = scaler.transform(X_test)
```
Where applicable, use `Pipeline` and `ColumnTransformer` with `set_output(transform="pandas")`.

### 4.2 From-Scratch Algorithm Implementation
Every algorithm note includes a readable from-scratch NumPy implementation checked numerically against the reference library (scikit-learn, PyTorch, or Gymnasium) on the same dataset and random seed.

### 4.3 Safe Model Serialization
* Do not use unvalidated `pickle.dump()` in production snippets.
* Always highlight the Arbitrary Code Execution (RCE) risk of unpickling untrusted files.
* Provide secure persistence examples using `skops.io` or `safetensors` alongside standard `joblib`.

### 4.4 Reinforcement Learning Guidelines
* Use Gymnasium 1.x API: `reset(seed=...) -> (obs, info)` and `step(action) -> (obs, reward, terminated, truncated, info)`.
* Report multi-seed evaluation results (mean and standard deviation) to account for RL variance.
* Clearly differentiate `terminated` (absorbing goal/failure state) from `truncated` (time limit expiration).

---

## 5. Visual Artifacts: Diagrams Only
* Zero external image files (`.png`, `.jpg`, `.svg`).
* All diagrams must be authored as Mermaid diagrams (`mermaid`) or structured ASCII drawings.
* Every algorithm note requires at least 2 diagrams:
  1. Pipeline/Flow diagram (Mermaid)
  2. Geometric/Conceptual diagram (ASCII or Mermaid)
