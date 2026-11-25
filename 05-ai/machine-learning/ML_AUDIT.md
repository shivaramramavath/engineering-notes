# Machine Learning Track: Initial Curriculum & Structural Audit

> **Area:** `05-ai`  
> **Track:** `machine-learning`  
> **Date:** 2026-11-25  
> **Audit Type:** Green-Field Curriculum Design & Structural Conformance Audit

---

## 1. Initial State Assessment

* **Pre-existing Notes:** Zero files existed in `05-ai/machine-learning/`. The parent directory `05-ai/` contains LangChain (`01-langchain-js`), LangGraph (`02-langgraph-js`), LlamaIndex (`03-llamaindex`), and RAG notes, which focus on LLM application engineering rather than classical machine learning foundations.
* **Curriculum Need:** A complete, rigorous, classical machine learning curriculum covering:
  - Supervised learning (regression, classification, ensemble models)
  - Unsupervised learning (clustering, dimensionality reduction, density estimation, anomaly detection)
  - Reinforcement learning (tabular MDPs, value-based DQN, policy gradients PPO/SAC)
  - Mathematics foundations, data preprocessing, model evaluation, postprocessing, safe persistence, and MLOps.
* **Cross-Track Integration Requirements:**
  - Python Language & Type Hints: Link to [`01-languages/python/`](file:///01-languages/python/) (especially `11-type-hints/` and `21-data-and-scientific-basics/`).
  - Model Serving: Link to [`03-backend/fastapi/`](file:///03-backend/fastapi/) (`16-ai-and-ml-serving/`).
  - Containerization: Link to [`04-devops/docker/`](file:///04-devops/docker/).
  - Feature Stores & SQL Data Extraction: Link to [`09-sql/`](file:///09-sql/).

---

## 2. Structural Requirements & Conformance Specifications

Every file in the Machine Learning curriculum must adhere strictly to one of three authoritative templates:

### 2.1 The 21-Section Algorithm Template (`algorithm-template.md`)
Required for every algorithm-specific note (e.g., Linear Regression, Random Forest, K-Means, Q-Learning, PPO):
1. Title & One-line Summary
2. Metadata Table (Level, Prerequisites, Time, Status, Tested with: library versions + seed)
3. Table of Contents
4. Section 01: Intuition & Problem It Solves (Plain language, real-world analogy)
5. Section 02: Algorithm Card (Learning type, task, parametric?, assumptions, scaling?, missing values?, categorical?, interpretability, complexity, hyperparameters)
6. Section 03: Diagrams (At least 2: Mermaid flowchart of pipeline AND geometric ASCII/Mermaid conceptual picture; for RL: Agent-Environment loop + update rule)
7. Section 04: The Math (Loss/objective, update rules, derivation outline, complete symbol table)
8. Section 05: Step-by-Step Pseudocode & From-Scratch NumPy Implementation (checked against library output)
9. Section 06: Using the Library (scikit-learn / PyTorch / Gymnasium) with the complete 9-stage verified workflow
10. Section 07: Preprocessing Required (Scaling, encoding, missing values, outliers)
11. Section 08: Training (Fit, hyperparameters, tuning strategy, convergence diagnostics)
12. Section 09: Prediction & Inference (Latency, batch vs single, predict vs predict_proba)
13. Section 10: Postprocessing (Thresholding, calibration, inverse transforms, business rules, uncertainty)
14. Section 11: Saving & Loading (Safe persistence, skops/safetensors/ONNX vs pickle security warnings, pipeline verification)
15. Section 12: Evaluation (Metrics, validation scheme, baselines, error analysis)
16. Section 13: Advantages
17. Section 14: Limitations & Failure Modes (Assumption violations, data hunger, drift, when NOT to use)
18. Section 15: Comparison & Trade-off Matrix (Table vs neighbouring algorithms)
19. Section 16: Common Mistakes & Pitfalls Table ($\ge 6$ rows: mistake / why wrong / fix)
20. Section 17: Performance, Security & Ethics Notes
21. Section 18: Interview Questions (8 technical questions with `<details>` collapsible answers)
22. Section 19: Practice Exercises (6 exercises: 2 easy, 2 medium, 2 hard with `<details>` solutions)
23. Section 20: Summary & Cheat Sheet
24. Section 21: Further Reading (verified URLs only) + Navigation Footer

### 2.2 The 13-Section Concept Template (`concept-template.md`)
Required for foundational, conceptual, preprocessing, evaluation, and operational notes:
1. Title & Metadata Header
2. Architectural ASCII / Mermaid Diagram
3. Section 01: Concept Overview & Core Motivation
4. Section 02: Theoretical Foundations & Mathematics
5. Section 03: Implementation & Practical Code
6. Section 04: Edge Cases & Advanced Scenarios
7. Section 05: Anti-Patterns & Common Pitfalls Table ($\ge 6$ rows)
8. Section 06: Best Practices & Production Guidelines
9. Section 07: Real-World Case Study
10. Section 08: Performance & Resource Optimization
11. Section 09: Security, Privacy & Ethics Considerations
12. Section 10: Diagnostic & Verification Playbook
13. Section 11: Technical Interview Q&As (8 questions with `<details>`)
14. Section 12: Hands-On Exercises (6 problems with `<details>` solutions)
15. Section 13: Summary, Verified References & Navigation Footer

### 2.3 The 5-Section Production Project Template (`project-template.md`)
Required for all reference projects in `16-projects/`:
1. Project Overview & Architecture
2. Problem Definition & Data Schema
3. Complete Implementation Code (Modular, fully typed, leakage-safe pipeline)
4. Verification, Training Log & Test Results (Real verified metrics and outputs)
5. Troubleshooting, Failure Modes & Production Next Steps
