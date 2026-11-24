# Python Platform Facts & Ecosystem Baseline (PY_FACTS.md)

> Verified technical facts, release lifecycles, compiler features, and ecosystem tool versions for the Python curriculum track. Verified on 2026-11-24.

---

## 1. CPython Release Lifecycles & Support Status

| Python Version | Release Date | Current Phase | EOL Date | Key Language & Runtime Milestones |
|---|---|---|---|---|
| **Python 3.10** | Oct 4, 2021 | End-of-Life | Oct 1, 2026 | Structural pattern matching (`match/case`), parenthesized context managers |
| **Python 3.11** | Oct 24, 2022 | Security Fixes Only | Oct 2027 | Faster CPython (adaptive specializing interpreter), Exception Groups (`except*`), `Self` type, `tomllib` |
| **Python 3.12** | Oct 2, 2023 | Security Fixes Only | Oct 2028 | PEP 695 type parameter syntax (`[T]`), `type` statement, per-interpreter GIL C-API, f-string syntax lifting |
| **Python 3.13** | Oct 7, 2024 | Security Fixes Only | Oct 2029 | Experimental Free-threaded build (no-GIL, `python3.13t`), experimental JIT compiler (Tier-3 copy-and-patch), enhanced interactive REPL (PyPy-derived) |
| **Python 3.14** | Oct 7, 2025 | Active Bugfix | Oct 2030 | PEP 649/749 deferred evaluation of annotations (`annotationlib`), PEP 750 Template Strings (`t"..."`), enhanced error reporting |

*Source*: Official Python Developer Guide / CPython Release Calendars (`https://peps.python.org/pep-0602/`).

---

## 2. Concurrency & Internal Runtimes: Free-Threading, JIT & Subinterpreters

### A. Free-Threaded Build (No-GIL / PEP 703)
- **Status in 3.13 / 3.14**: Experimental build tier.
- **Binary Naming**: Executables distributed as `python3.13t` / `python.exe` with free-threading flag.
- **Runtime Toggle**: Can force GIL re-enablement via environment variable `PYTHON_GIL=1` or command flag `-X gil=1`.
- **Performance Tradeoff**: Single-threaded throughput incurs 10%–20% overhead due to atomic reference counting and mimalloc biased reference counting; multi-threaded CPU-bound throughput scales linearly across available cores.
- **C-Extension Ecosystem**: NumPy 2.x, PyTorch 2.x, and Cython have upstream wheels supporting free-threading; legacy C-extensions crash if relying on implicit GIL serialization.

### B. Experimental Copy-and-Patch JIT
- **Status**: Experimental (Tier 3).
- **Architecture**: Builds on PEP 659 adaptive specializing interpreter. Translates traces of micro-ops into machine code using LLVM copy-and-patch stencils.
- **Compilation Flag**: Requires source compilation with `--enable-experimental-jit`.

### C. Subinterpreters (PEP 684 / PEP 554)
- **Status**: Production C-API since Python 3.12 with independent per-interpreter GILs.
- **Mechanism**: Multiple isolated interpreter states in a single OS process sharing memory without global lock contention, communicating via cross-interpreter queues or channels.

---

## 3. Type System Evolution (PEP 695 & PEP 649)

### A. PEP 695 Modern Type Syntax (Python 3.12+)
- **Generic Functions**: `def first[T](items: list[T]) -> T:`
- **Generic Classes**: `class Box[T]: ...` (no longer requires `from typing import Generic`)
- **Type Aliases**: `type Vector[T] = list[T]` (replaces `Vector = list[T]`)
- **ParamSpec & Variadics**: `def logged[**P, R](fn: Callable[P, R]) -> Callable[P, R]:`
- **Scoped Type Parameters**: Type parameters do not pollute module global scope.

### B. PEP 649 / PEP 749 Deferred Annotation Evaluation (Python 3.14+)
- **Mechanism**: Annotations are not evaluated at class/function definition time. The compiler saves annotation code objects in `.annotations` descriptors and evaluates them lazily on access.
- **Module**: `annotationlib` provides tools to introspect deferred annotations in various modes (`VALUE`, `FORWARDREF`, `STRING`).
- **Impact**: Resolves circular import issues and eliminated the need to quote forward references without `from __future__ import annotations`.

---

## 4. Modern Tooling Baseline (Local System Verified)

| Tool | Verified Version | Purpose & Capabilities |
|---|---|---|
| **CPython** | `3.14.0` | Active modern CPython interpreter |
| **uv** | `0.13.9` | High-performance Rust project & package manager, drop-in replacement for pip/pipx/poetry/pyenv |
| **ruff** | `0.17.6` | Sub-millisecond Rust linter and code formatter conforming to flake8/black/isort rules |
| **mypy** | `2.4.1` (compiled) | Authoritative static type checker for Python |
| **pytest** | `9.1.1` | Standard test execution engine with async/parametrize plugins |
| **hypothesis** | `6.170.17` | Advanced property-based testing framework |
| **pydantic** | `2.14.1` | High-speed data validation and settings management (pydantic-core in Rust) |
| **httpx** | `0.28.1` | Next-generation synchronous and asynchronous HTTP client for Python |
| **numpy** | `2.4.6` | Fundamental package for scientific computing with vectorized C/Fortran arrays |
