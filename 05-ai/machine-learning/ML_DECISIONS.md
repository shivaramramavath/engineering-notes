# Machine Learning Track: Architectural Decision Records (ADRs)

> **Area:** `05-ai`  
> **Track:** `machine-learning`  
> **Date:** 2026-11-25

---

### ADR-001: 19-Module Curriculum Taxonomy
* **Context:** The machine learning domain encompasses foundational mathematics, data wrangling, classical supervised/unsupervised algorithms, reinforcement learning, time series/text, MLOps, deployment, and ethical safety.
* **Decision:** Establish an explicit 19-module numbered taxonomy (`00-setup` through `18-cheatsheets`). Deep learning (transformers, LLMs, computer vision) is maintained in separate tracks under `05-ai/`, with Module 14 providing a dedicated bridge to prevent content duplication.
* **Status:** Accepted.

---

### ADR-002: Authoritative Tri-Template Specification
* **Context:** Algorithm-specific notes require deep mathematical derivations, algorithm cards, from-scratch NumPy implementations, and full lifecycle workflows, whereas conceptual notes (e.g., Information Theory, Bias-Variance, MLOps) require architectural analysis and best-practice frameworks. Reference projects require end-to-end reproducible pipelines.
* **Decision:**
  1. All algorithm notes must follow the **21-Section Algorithm Template** (`algorithm-template.md`).
  2. All conceptual, foundational, preprocessing, and evaluation notes must follow the **13-Section Concept Template** (`concept-template.md`).
  3. All reference projects must follow the **5-Section Project Template** (`project-template.md`).
* **Status:** Accepted.

---

### ADR-003: Modern scikit-learn 1.x Baseline & Deprecations
* **Context:** Scikit-learn has undergone major modernization, including `set_output(transform="pandas")`, metadata routing, native missing/categorical handling via `HistGradientBoosting`, and deprecation of positional arguments in estimator initializers.
* **Decision:** Enforce modern scikit-learn conventions throughout all code examples. Deprecated patterns (such as `get_feature_names`, positional `__init__` parameters, and manual one-hot encoding where native histograms are preferred) are documented explicitly in anti-patterns tables.
* **Status:** Accepted.

---

### ADR-004: Gymnasium 1.x Standard Decoupled Signatures
* **Context:** Gymnasium decoupled the legacy Gym `done` flag into `terminated` (true task failure/success) and `truncated` (time limit reached).
* **Decision:** All reinforcement learning code and explanations must use Gymnasium 1.x signatures: `obs, info = env.reset(seed=...)` and `obs, reward, terminated, truncated, info = env.step(action)`. Value function bootstrapping and absorbing state distinctions must be explained mathematically.
* **Status:** Accepted.

---

### ADR-005: Model Persistence Security Standard
* **Context:** Loading arbitrary `pickle` or `joblib` model files exposes systems to arbitrary code execution (RCE) via `__reduce__` exploit payloads.
* **Decision:** While `joblib` is documented as the historical standard, every serialization note must feature a prominent security warning and demonstrate secure alternatives: `skops` for scikit-learn pipelines, `safetensors` for PyTorch tensors, and `ONNX` for cross-platform runtime deployment.
* **Status:** Accepted.

---

### ADR-006: Markdown-Only File Invariant
* **Context:** The repository enforces a strict markdown-only standard for notes and projects.
* **Decision:** No raw `.py`, `.ipynb`, `.pkl`, or dataset files will be committed inside the repository. All code, configuration files, and verified execution logs reside inside fenced code blocks within `.md` notes.
* **Status:** Accepted.

---

### ADR-007: Dataset Selection for Offline Reproducibility
* **Context:** External dataset downloads are prone to network timeouts, firewall blocks, link rot, and API rate limits.
* **Decision:** Use scikit-learn built-in toy datasets (`load_iris`, `load_breast_cancer`, `load_wine`, `load_diabetes`) and synthetic generators (`make_classification`, `make_regression`, `make_blobs`). Real dataset downloads are provided with fallback synthetic generators. Every note explicitly records random seeds (`seed=42`).
* **Status:** Accepted.
