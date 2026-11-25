# Module 00: Setup, Problem Framing & Scientific Foundations

> **Track:** Machine Learning Curriculum  
> **Module ID:** `05-ai/machine-learning/00-setup`  
> **Status:** Complete (5 of 5 notes authored and verified)

---

## 1. Overview & Purpose

Module 00 establishes the engineering, operational, and conceptual foundations of the machine learning track. It deconstructs the AI landscape, provides rigorous criteria for framing business challenges into solvable mathematical objectives, standardizes the modern Python scientific environment, establishes strict determinism and reproducibility protocols, and tours the core data science libraries (NumPy, pandas, SciPy, scikit-learn, PyTorch).

---

## 2. Note Catalog & Learning Sequence

| Note | Topic & Key Concepts | Prerequisites | Link |
| :---: | :--- | :--- | :---: |
| **01** | **What is ML & AI Landscape**<br>AI vs ML vs DL vs Data Science, 4 core paradigms (Supervised, Unsupervised, Semi-supervised, RL), inductive biases. | Basic programming, high school math | [01-what-is-ml-and-the-ai-landscape.md](./01-what-is-ml-and-the-ai-landscape.md) |
| **02** | **ML Workflow & Problem Framing**<br>CRISP-DM lifecycle, baseline hierarchy (Trivial, Rule-based, Linear), target framing, asymmetric business cost matrices. | Note 01 | [02-ml-workflow-and-problem-framing.md](./02-ml-workflow-and-problem-framing.md) |
| **03** | **Python ML Environment**<br>Modern package management (`uv`, `venv`), binary C-extension handling, notebooks vs production scripts, BLAS thread tuning. | Note 01 | [03-python-ml-environment.md](./03-python-ml-environment.md) |
| **04** | **Reproducibility & Determinism**<br>Random seeds, PRNG synchronization, non-associative floating-point summation drift, multithreaded reduction hazards. | Note 03 | [04-reproducibility-and-determinism.md](./04-reproducibility-and-determinism.md) |
| **05** | **Data Science Toolbox Tour**<br>NumPy memory layout/strides, pandas 3.x Arrow backends, SciPy sparse formats (CSR/CSC), scikit-learn API, PyTorch bridge. | Notes 01, 03 | [05-data-science-toolbox-tour.md](./05-data-science-toolbox-tour.md) |

---

## 3. Recommended Study Workflow

1. Read **Note 01** and **Note 02** to master how to frame business goals into formal machine learning tasks and avoid deploying models where simple heuristics suffice.
2. Follow **Note 03** to set up a clean, isolated virtual environment using `uv` with Python 3.11.
3. Review **Note 04** to establish strict random seeding across all pipelines.
4. Deep-dive into **Note 05** to understand how NumPy contiguous buffers, SciPy sparse representations, and scikit-learn pipelines interact with zero memory duplication.
5. Proceed to [Module 01: Math Foundations for Machine Learning](../01-math-foundations/README.md).
