# Python Curriculum Architecture Decisions & ADR Log (PY_DECISIONS.md)

> Record of architectural decisions, toolchain choices, design trade-offs, and scoping determinations made during the creation of the Python curriculum track.

---

## ADR 001: Modern Python Baseline — CPython 3.12 - 3.14+
- **Context**: The curriculum needs to remain fresh and forward-looking, covering both currently deployed enterprise versions (3.11/3.12) and the latest runtime shifts (3.13 free-threading/JIT and 3.14 deferred annotations/t-strings).
- **Decision**: Adopt Python 3.12+ (PEP 695 generic syntax `[T]`, `type` statement) as the modern idiom, highlighting Python 3.10/3.11 backward compatibility fallbacks where applicable, and showcasing cutting-edge features from Python 3.13/3.14 (free-threaded GIL disabling, `annotationlib`, `t-strings`).
- **Status**: Accepted.

---

## ADR 002: Toolchain Selection — uv, ruff, mypy, pytest
- **Context**: The Python tooling landscape has consolidated around ultra-fast Rust-based tools (`uv`, `ruff`) while `mypy` and `pytest` remain standard for type verification and testing.
- **Decision**:
  - Package & Project Management: `uv` (with `pip` and `pyproject.toml` compatibility).
  - Linting & Formatting: `ruff` (replaces black, flake8, isort).
  - Static Type Checking: `mypy` with `--strict` mode.
  - Testing Engine: `pytest` with `hypothesis` for property-based testing.
- **Status**: Accepted.

---

## ADR 003: Scope Boundaries with Downstream AI & Backend Tracks
- **Context**: The repository contains dedicated tracks: `03-backend/fastapi` and `05-ai/machine-learning`.
- **Decision**:
  - The Python track covers core language, standard library, concurrency, internals, packaging, and foundation data libraries (NumPy, pandas, polars basics, ASGI/WSGI fundamentals).
  - Deep framework courses (FastAPI router dependency injection, Pydantic v2 deep nested models for REST endpoints, full ML training loops in PyTorch/Scikit-learn) are cross-linked to their respective dedicated tracks.
- **Status**: Accepted.

---

## ADR 004: Markdown-Only In-Repo Storage
- **Context**: The user strictly mandates that only Markdown files (`.md`) and the root `tree.txt` exist in the repository track.
- **Decision**: Zero `.py`, `.toml`, or virtualenv files will be created in `01-languages/python/`. All runnable projects, test suites, and configurations are housed in fenced code blocks. All execution and verification runs happen in external scratch storage (`<artifacts>/scratch/`).
- **Status**: Accepted.

---

## ADR 005: Direct Note Authoring vs Scripts
- **Context**: User Speed Rule 1 strictly forbids writing meta-scripts (`gen_*.py`) to generate markdown notes.
- **Decision**: All `.md` notes and READMEs will be authored directly using single `write_file` calls per note. Batch example verification is conducted in scratch, with real output pasted directly into the notes.
- **Status**: Accepted.
