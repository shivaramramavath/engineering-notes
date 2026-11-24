# Python Curriculum Engineering Changelog (PY_CHANGELOG.md)

> Historical audit trail of all architectural implementations, corrections, migrations, and batch completions for `01-languages/python/`.

---

## [Batch 0] - 2026-11-24 - Architecture, Templates, Conventions & Scaffolding
- **Phase 0 Research**: Executed research on Python release lifecycles (3.10 to 3.14+), Free-threaded CPython (no-GIL / PEP 703), Tier-3 JIT compiler, Subinterpreters (PEP 684), PEP 695 type parameter syntax, PEP 649 deferred annotations (`annotationlib`), and PEP 750 template strings (`t-strings`). Formulated `PY_FACTS.md`.
- **Conventions & Standards**: Formulated `PY_CONVENTIONS.md` documenting the 13-section architectural model, metadata schema (`Python version`, `Tested with`, `Free-threading / platform notes`), project layout, and markdown-only repository compliance.
- **Architectural Decision Records**: Formulated `PY_DECISIONS.md` defining ADRs 001 through 005.
- **Audit & Gap Analysis**: Authored `PY_AUDIT.md` verifying the local test toolchain (CPython 3.14.0, uv 0.13.9, ruff 0.17.6, mypy 2.4.1, pytest 9.1.1, hypothesis 6.170.17).
- **Master Plan**: Formulated `PY_COMPLETION_PLAN.md` with multi-track career learning roadmaps, time estimates, cross-link integration matrix, and full 29-module directory plan.
- **Authoritative Templates**: Created `_templates/concept-template.md` (13 sections) and `_templates/project-template.md`.
- **Progress Tracking**: Created `PY_PROGRESS.md`.
