# Contributing & Verification Standards

> **Track ID:** `05-ai/machine-learning/CONTRIBUTING`  
> **Status:** Production Standard  
> **Scope:** Authoring Guidelines, Template Adherence, Code Testing & Pull Request Review Rules

---

## 1. Core Architectural Constraints

All contributions to the `05-ai/machine-learning` curriculum must comply with these foundational invariants:

1. **Markdown-Only Constraint**:
   * **Never commit raw executable scripts** (`.py`), Jupyter Notebooks (`.ipynb`), or serialized binary weights (`.pkl`, `.joblib`, `.pt`, `.onnx`, `.bin`) directly into the repository tree.
   * All code, configuration dictionaries, test fixtures, and execution traces must reside strictly inside fenced code blocks within `.md` files.
2. **Template Fidelity**:
   * **Concept Notes** must adhere strictly to `_templates/concept-template.md` (13 mandatory numbered sections).
   * **Algorithm Notes** must adhere strictly to `_templates/algorithm-template.md` (21 mandatory numbered sections).
   * **Project Notes** must adhere strictly to `_templates/project-template.md` (5 mandatory numbered sections).
3. **Naming & Directory Conventions**:
   * Module folders must follow the `NN-kebab-case` pattern starting from `00-setup` through `18-cheatsheets`.
   * Note filenames within modules must be numbered `NN-kebab-case.md` starting at `01-` (e.g., `01-what-is-ml-and-the-ai-landscape.md`).
   * Every module directory must contain an authoritative, standalone `README.md`.
4. **Reproducibility & Hardware Standards**:
   * All stochastic code examples must use explicit random seeds (`seed=42` or `np.random.default_rng(42)`).
   * Code must run self-contained and verified on standard Python 3.11+ / CPU architectures without requiring specialized external cloud hardware.

---

## 2. Note Structure Standards

### 2.1 Concept Note Standard (13 Sections)
Every concept note must contain:
1. Conceptual Definition & Core Mental Model
2. Theoretical Foundations & Mathematical Formulations
3. Visual Architecture Diagram (`mermaid`)
4. Data Flow & Tensor Shape Lifecycle
5. Complete Production Python Implementation (Typed & Self-Contained)
6. Edge Cases, Failure Modes & Boundary Conditions
7. Hyperparameter Tuning & Diagnostic Guidance
8. Verification, Real Execution Logs & Assertions
9. Complexity & Computational Profiling ($O(\cdot)$ Time & Space)
10. Model Comparison & Trade-Off Matrix
11. Security, Data Leakage & Regulatory Constraints
12. Production Troubleshooting Runbook & Failure Signatures
13. Cross-References & Next Steps

### 2.2 Algorithm Note Standard (21 Sections)
Every algorithm note must contain:
1. Mathematical Problem Formulation
2. Mathematical Derivation from First Principles
3. Optimization Objective Function
4. Algorithmic Pseudocode & Step-by-Step Walkthrough
5. Visual Convergence / Optimization Diagram (`mermaid`)
6. Vectorized NumPy Implementation from Scratch
7. Industrial Production Implementation (Scikit-Learn / PyTorch)
8. Hyperparameter Sensitivity & Search Spaces
9. Complexity Analysis ($O(\cdot)$ Training, Inference, Memory)
10. Inductive Biases & Structural Assumptions
11. Strengths, Weaknesses & Comparative Trade-Offs
12. Failure Modes, Pathological Data & Edge Cases
13. Probability Calibration & Prediction Properties
14. Scalability to Large $N$ and High Dimensions $d$
15. Interpretability & Feature Attribution Mechanisms
16. Empirical Benchmarks & Real Verification Log
17. Hardware Optimization & Latency Acceleration
18. Diagnostic Residual & Learning Curves
19. Production Deployment & Persistence Guidelines
20. Historical Context & Academic Citations
21. Exercises, Derivation Challenges & References

---

## 3. Pull Request Review Checklist

Before opening a pull request to merge changes into `main`, verify every item below:

- [ ] **Formatting**: File follows `NN-kebab-case.md` format and links use relative `file:///` markdown paths.
- [ ] **Templates**: All mandatory numbered sections from the relevant template are present and populated.
- [ ] **Verification**: All Python code blocks run without error under Python 3.11+, scikit-learn 1.9+, PyTorch 2.4+, and NumPy 2.5+.
- [ ] **Assertions**: Every code script ends with explicit test `assert` statements confirming expected numerical outputs.
- [ ] **Mermaid**: All Mermaid diagrams render cleanly without syntax errors (quote special characters).
- [ ] **Math Notation**: Mathematical equations use clean KaTeX LaTeX syntax with escaped `\$` for prose currency.
- [ ] **Zero Binaries**: Git status confirms zero `.py`, `.ipynb`, or `.pkl` files added to tracking.
- [ ] **Governance**: `ML_CHANGELOG.md` and `ML_PROGRESS.md` are updated to reflect the new additions.
