# Machine Learning Track: Changelog

This document tracks all curriculum design milestones, note authoring, architectural decisions, and version baseline updates in `05-ai/machine-learning/`.

---

## [1.0.0] - 2026-11-25

### Curriculum Delivery & Final Production Sign-off
- Completed all 19 curriculum modules (`00-setup` through `18-cheatsheets`) with 203 markdown files.
- Completed Module 16 Reference Projects: 12 production-grade end-to-end projects with runnable code, verification logs, and troubleshooting runbooks.
- Completed Module 17 Technical Interview Preparation: 185+ technical Q&As across 8 domains, 10 algorithms implemented from scratch in pure NumPy, 5 large-scale system designs, and STAR leadership cases.
- Completed Module 18 Quick Reference Cheatsheets: 11 reference guides including algorithm chooser flowchart, scikit-learn API, preprocessing recipes, metrics formulas, hyperparameter tables, PyTorch snippets, persistence recipes, math formulas, and top 30 common errors & fixes.
- Created Track Master documents: `README.md`, `INDEX.md`, `ROADMAP.md`, `GLOSSARY.md`, `CONTRIBUTING.md`, and `ML_FINAL_REPORT.md`.
- Verified 100% template compliance (13-section concept notes, 21-section algorithm notes, 5-section project notes).
- Verified strict zero-loose-binary constraint with pure in-markdown self-contained verification harnesses.

---

## [1.0.0-draft] - 2026-11-25

### Track Initialization & Architectural Blueprint
- Initialized green-field machine learning track in `05-ai/machine-learning/`.
- Created `ML_FACTS.md` establishing verified package baselines: Python 3.11/3.12, scikit-learn 1.9.1, NumPy 2.5.5, pandas 3.1.1, SciPy 1.16, XGBoost 3.4.2, LightGBM 4.7.0, CatBoost 1.2.10, PyTorch 2.15.0 (CPU), Gymnasium 1.4.0, Stable-Baselines3 2.9.2, Optuna 5.0.0, MLflow 3.18.0, skops 0.16.0, safetensors 0.8.0, imbalanced-learn 0.14.2, umap-learn 0.6.0.
- Created `ML_AUDIT.md` analyzing scope and cross-track dependencies.
- Created `ML_COMPLETION_PLAN.md` mapping the 19-module target curriculum.
- Created `ML_DECISIONS.md` recording ADRs 001 through 007.
- Created `CONVENTIONS.md` detailing coding standards, metadata schemas, and mathematical notation.
- Created `_templates/concept-template.md` (13-section format).
- Created `_templates/algorithm-template.md` (21-section format).
- Created `_templates/project-template.md` (5-section format).
- Initialized `ML_PROGRESS.md` checklist for all 19 modules.
