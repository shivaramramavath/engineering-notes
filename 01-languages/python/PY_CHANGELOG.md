# Python Curriculum Engineering Changelog (PY_CHANGELOG.md)

> Historical audit trail of all architectural implementations, corrections, migrations, and batch completions for `01-languages/python/`.

---

## [Module 00] - 2026-11-24 - Setup, Toolchains & Python Runtime
- **00-what-is-python-and-implementations.md**: Completed exhaustive 13-section guide covering CPython, PyPy, GraalPy, Free-Threaded build (PEP 703), and Tier-3 Copy-and-Patch JIT compiler.
- **01-installing-and-managing-versions.md**: Completed 13-section guide detailing `uv python`, `pyenv`, Windows launcher `py.exe`, and PEP 668 `EXTERNALLY-MANAGED` environments.
- **02-virtual-environments.md**: Completed 13-section guide on `pyvenv.cfg` mechanics, `sys.prefix` vs `sys.base_prefix`, activation scripts, and `uv venv`.
- **03-packages-with-pip-and-uv.md**: Completed 13-section guide on wheel `.whl` anatomy, ABI tags, PubGrub resolution, hash validation, and `uv lock`.
- **04-repl-and-first-program.md**: Completed 13-section guide on the Python 3.13+ enhanced REPL, `sys.ps1`/`sys.ps2`, `_` variable, and canonical `__main__` entrypoint.
- **05-editor-and-debugger-setup.md**: Completed 13-section guide on VS Code settings, Pyright LSP, Astral Ruff extension, and `debugpy` DAP architecture.
- **06-how-python-runs-code.md**: Completed 13-section guide on the compilation pipeline, `__pycache__` and `.pyc` 16-byte headers, `PyCodeObject`, and `PyEval_EvalFrameDefault`.
- **07-zen-of-python-and-pep8.md**: Completed 13-section guide on PEP 20 heuristics, PEP 8 styling, guard clauses, and Ruff formatting.
- **00-setup/README.md**: Updated syllabus table and module outcomes.

---

## [Batch 0] - 2026-11-24 - Architecture, Templates, Conventions & Scaffolding
- **Phase 0 Research**: Formulated `PY_FACTS.md`.
- **Conventions & Standards**: Formulated `PY_CONVENTIONS.md`.
- **Architectural Decision Records**: Formulated `PY_DECISIONS.md`.
- **Audit & Gap Analysis**: Formulated `PY_AUDIT.md`.
- **Master Plan**: Formulated `PY_COMPLETION_PLAN.md`.
- **Authoritative Templates**: Created `_templates/concept-template.md` and `_templates/project-template.md`.
- **Progress Tracking**: Created `PY_PROGRESS.md`.
- **Directory Scaffolding**: Scaffolding initialized for all 29 module directories and READMEs.
