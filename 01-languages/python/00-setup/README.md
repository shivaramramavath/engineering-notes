# Module 00: Setup, Toolchains & Python Runtime (`00-setup`)

> **Scope**: Installing Python, runtime implementations (CPython, PyPy, GraalPy, Free-Threaded build), the Astral uv toolchain, virtual environment internals, package management (pip, uv, wheels), the enhanced Python 3.13+ REPL, editor & debugger configuration (VS Code, debugpy), CPython execution architecture (bytecode, `__pycache__`, evaluation loop), and PEP 8 / Ruff code hygiene.

---

## 📋 Module Overview

| Metric | Details |
|---|---|
| **Directory** | `01-languages/python/00-setup` |
| **Status** | 100% COMPLETE & FULLY VERIFIED |
| **Notes Count** | 8 Concept Notes |
| **Baseline Toolchain** | CPython 3.14.0, uv 0.13.9, ruff 0.17.6, mypy 2.4.1 |
| **Next Module** | [01-fundamentals](../01-fundamentals/README.md) |

---

## 🎯 Learning Outcomes
By completing this module, you will:
1. Understand the architecture of CPython and how it compares to alternative runtimes (PyPy, GraalPy, Free-threaded no-GIL builds).
2. Master multi-version Python management with Astral `uv` without corrupting system packages.
3. Comprehend virtual environment internals (`pyvenv.cfg`, `site-packages` isolation, executable redirection).
4. Understand wheel vs sdist distribution formats, PubGrub resolution, and deterministic lockfile generation.
5. Leverage the Python 3.13+ enhanced interactive REPL and configure IDE debuggers with `debugpy`.
6. Trace how CPython transforms `.py` source text into bytecode, caches `.pyc` files, and evaluates opcodes in `ceval.c`.
7. Enforce clean, idiomatic Python code conforming to PEP 8 using Astral Ruff.

---

## 📑 Syllabus & Notes Directory

| Note File | Title | Focus & Core Concept |
|---|---|---|
| [00-what-is-python-and-implementations.md](./00-what-is-python-and-implementations.md) | What Is Python & Implementations | CPython, PyPy, GraalPy, Free-Threading (PEP 703), Copy-and-Patch JIT |
| [01-installing-and-managing-versions.md](./01-installing-and-managing-versions.md) | Installing & Managing Versions | Astral `uv python`, `pyenv`, Windows launcher `py.exe`, PEP 668 |
| [02-virtual-environments.md](./02-virtual-environments.md) | Virtual Environments Internals | `pyvenv.cfg`, `sys.prefix`, activation mechanics, `uv venv` |
| [03-packages-with-pip-and-uv.md](./03-packages-with-pip-and-uv.md) | Package Management with pip & uv | Wheels (`.whl`), ABI tags, PubGrub resolver, `uv lock`, cryptographic hashes |
| [04-repl-and-first-program.md](./04-repl-and-first-program.md) | REPL & First Program | Python 3.13+ REPL, `sys.ps1`/`sys.ps2`, `_` variable, `__name__ == '__main__'` |
| [05-editor-and-debugger-setup.md](./05-editor-and-debugger-setup.md) | Editor & Debugger Setup | VS Code, Pyright LSP, Ruff extension, `debugpy` DAP, `breakpoint()` |
| [06-how-python-runs-code.md](./06-how-python-runs-code.md) | How Python Runs Code | Tokenizer, PEG Parser, `PyCodeObject`, `__pycache__`, evaluation loop |
| [07-zen-of-python-and-pep8.md](./07-zen-of-python-and-pep8.md) | Design Philosophy & PEP 8 | PEP 20 heuristics, PEP 8 standards, naming, guard clauses, Ruff formatting |
