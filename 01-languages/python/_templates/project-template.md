# Project [XX]: [Project Name] ([slug])

> **Project Brief**: [High-level summary of the application: architectural purpose, design patterns utilized, dependencies, and business domain modeled.]

---

## Metadata Table

| Field | Value |
|---|---|
| **Module** | 26-projects |
| **Category** | Production Reference Project |
| **Target Level** | Advanced / Enterprise Grade |
| **Python Baseline** | Python 3.12+ (CPython 3.14.0 tested) |
| **Tooling & Libraries** | uv, ruff, mypy, pytest |
| **Architecture Pattern** | Layered Architecture / SOLID / Constructor Dependency Injection |

---

## 1. System Architecture & Component Diagram

```text
+-----------------------------------------------------------------------------------+
|                            APPLICATION ARCHITECTURE                               |
+-----------------------------------------------------------------------------------+
|  CLI / Entrypoint (argparse / click / typer)                                      |
|       │                                                                           |
|       ▼                                                                           |
|  Service Layer (Business Logic & Orchestration)                                  |
|       │                                                                           |
|       ▼                                                                           |
|  Domain Models (Dataclasses / Pydantic / Enums)                                   |
|       │                                                                           |
|       ▼                                                                           |
|  Repository Layer / Storage Engine (SQLite / In-Memory / File Storage)            |
+-----------------------------------------------------------------------------------+
```

---

## 2. Complete Project Source Code

### `src/models.py`
```python
# Complete domain model with dataclasses, enums, or pydantic models
```

### `src/repository.py`
```python
# Data access protocols, in-memory or SQLite repository implementation
```

### `src/service.py`
```python
# Core business domain logic, validation, and operations
```

### `src/cli.py`
```python
# CLI interface or executable entrypoint
```

---

## 3. Project Configuration (`pyproject.toml`)

```toml
[project]
name = "project-slug"
version = "0.1.0"
description = "Production-grade reference application in Python"
readme = "README.md"
requires-python = ">=3.12"
dependencies = []

[build-system]
requires = ["hatchling"]
build-backend = "hatchling.build"

[tool.ruff]
line-length = 88
target-version = "py312"

[tool.mypy]
strict = true
python_version = "3.12"

[tool.pytest.ini_options]
testpaths = ["tests"]
```

---

## 4. Test Suite (`tests/test_app.py`)

```python
import pytest

# Complete pytest test suite covering unit operations, edge cases, and integration flows
```

---

## 5. Build, Lint & Execution Verification Log

```bash
# uv run ruff check .
All checks passed!

# uv run mypy src
Success: no issues found in 4 source files

# uv run pytest
============================== 6 passed in 0.08s ==============================
```
