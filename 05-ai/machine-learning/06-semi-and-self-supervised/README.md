# Module 06: Semi-Supervised & Self-Supervised Learning

Welcome to **Module 06: Semi-Supervised & Self-Supervised Learning** of the Machine Learning Curriculum track.

Modern machine learning operates in an era where raw, unlabeled data is abundant, yet high-quality human annotation remains an expensive and scarce bottleneck. This module covers the theoretical principles, mathematical formulations, algorithmic implementations, and diagnostic frameworks for extracting maximum predictive signal from combined labeled, unlabeled, and programmatically labeled data.

---

## Module Notes Overview

| Order | Note | Format | Level | Focus & Key Algorithms |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Semi-Supervised Learning Foundations](01-semi-supervised-learning.md) | Concept | Core | Smoothness, Cluster (Low-Density Separation), and Manifold assumptions; Graph Laplacian quadratic regularizer; When SSL helps vs hurts (Negative Transfer). |
| **02** | [Self-Training, Label Propagation & Label Spreading](02-self-training-and-label-propagation.md) | Algorithm | Core | Pseudo-labeling wrapper; Zhu & Ghahramani closed-form Harmonic function; Zhou's normalized Label Spreading with soft clamping ($\alpha \in (0, 1)$); Confirmation bias defenses. |
| **03** | [Self-Supervised & Contrastive Learning](03-self-supervised-and-contrastive-learning-intro.md) | Concept | Advanced | Pretext tasks; SimCLR, MoCo, BYOL; InfoNCE contrastive loss; Mutual Information lower bound; Hyperspherical Alignment vs Uniformity; Projection head intuition. |
| **04** | [Active Learning](04-active-learning.md) | Algorithm | Advanced | Human-in-the-loop pool-based sampling; Acquisition functions: Least Confidence, Margin Sampling, Shannon Entropy, Query-by-Committee (QBC); Core-Set $k$-center greedy; Area Under Learning Curve (ALC). |
| **05** | [Weak Supervision & Label Noise](05-weak-supervision-and-label-noise.md) | Concept | Core | Programmatic Labeling Functions (LFs); Snorkel generative label model; Continuous soft cross-entropy; Confident Learning (Northcutt et al.); Generalized Cross Entropy (GCE). |
| **06** | [Transfer Learning & Domain Adaptation](06-transfer-learning-concepts.md) | Concept | Core | Inductive vs Transductive transfer; Covariate shift density ratio weighting ($P_T / P_S$); Ben-David domain adaptation bound; Catastrophic forgetting (EWC); LoRA low-rank adaptation. |

---

## Architectural Decision Flowchart

```
+---------------------------------------------------------------------------------------------------------+
|                      SEMI-SUPERVISED, SELF-SUPERVISED & TRANSFER MASTER TAXONOMY                        |
|                                                                                                         |
|                                  What is your data and annotation reality?                              |
|                 /                    |                         |                      \                 |
|                /                     |                         |                       \                |
|       [Small Labeled Set     [Massive Unlabeled Only   [Domain Experts can       [Pre-trained Model     |
|        + Large Unlabeled]     No Labels Available]      Write Heuristic Rules]    from Similar Domain]  |
|                |                     |                         |                         |              |
|        Inductive or          Visual / Representation    Programmatic Weak         Target Dataset        |
|        Transductive?         Pretext Task:              Supervision:              Size:                 |
|         /          \                 |                         |                  /          \          |
|      [Inductive]  [Transductive]     |                         |               [Small]     [Large]      |
|         /            \               ▼                         ▼                 /            \         |
|   Self-Training   Label Spreading   SimCLR / MoCo /        Snorkel Label     Feature       Layer-Wise   |
|   (with GBDT)     (Soft-Clamped)    InfoNCE Contrastive    Model + Cleanlab  Extraction    Fine-Tuning  |
|                                     Learning                                 (Freeze)      (LoRA / EWC) |
+---------------------------------------------------------------------------------------------------------+
```

---

## Module Standards & Verification

- **From-First-Principles Implementations**: Features pure NumPy implementations of harmonic label propagation, InfoNCE loss, active learning acquisition scoring, soft cross-entropy, and low-rank LoRA layers.
- **Enterprise Pipelines**: Demonstrates Scikit-Learn wrapper patterns (`SelfTrainingClassifier`, `LabelSpreading`, `cross_val_predict`) integrated with leakage-safe scaling.
- **Cryptographic Serialization**: Model persistence recipes generating SHA-256 sidecar integrity digests.
- **Interview & Practice Mastery**: 8 advanced interview questions with formal mathematical proofs and 6 hands-on coding exercises per note.
