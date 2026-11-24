# Contributing to the FastAPI Curriculum (CONTRIBUTING.md)

> Standards, testing requirements, and authoring guidelines for contributors and reviewers.

---

## 1. Core Authoring Rules

1. **Strict Markdown-Only Output**:
   - Only `.md` files (and `tree.txt`) may be committed.
   - Never commit `.py`, `.json`, `.toml`, virtual environments (`.venv`), or test databases.
   - Code must be placed in fenced blocks with accurate language tags (`python`, `bash`, `dockerfile`, `sql`).
2. **Adherence to Templates**:
   - Concept notes must follow `_templates/concept-template.md` (all 13 sections).
   - Projects must follow `_templates/project-template.md` (all 5 parts).
3. **No Untested Examples**:
   - All code snippets must be tested against real Python 3.12+ interpreters in external scratch environments.
   - Code must pass `ruff check` and `mypy --strict`.
4. **Pydantic v2 & Lifespan Only**:
   - Never use Pydantic v1 syntax (`class Config`, `.dict()`).
   - Never use deprecated `@app.on_event("startup")`.
