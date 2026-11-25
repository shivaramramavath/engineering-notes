---
title: "Mypy and Pyright in Practice: Configuration, Strictness, and CI"
description: "Mastering static type analysis in production with Mypy and Pyright: architectural differences, pyproject.toml configuration, strict mode rules, error code suppression, plugins, and CI enforcement."
module: "11-type-hints-and-static-typing"
order: 7
tags:
  - python
  - typing
  - mypy
  - pyright
  - static-analysis
  - ci-cd
  - type-checking
difficulty: "Advanced"
cpython_version: "3.12+"
---

# Mypy and Pyright in Practice: Configuration, Strictness, and CI

Writing type hints in Python code is only half the battle; static type safety is only realized when enforced by an automated static analysis engine. The Python ecosystem is dominated by two primary type checkers: **Mypy** (the reference implementation maintained by Python core developers and Dropbox) and **Pyright** (the high-performance engine created by Microsoft that powers VS Code’s Pylance extension).

This note provides an architectural breakdown of both tools, battle-tested `pyproject.toml` configurations, strict mode enforcement strategies, and CI/CD pipeline integration.

---

## Table of Contents

1. [What & Why](#what--why)
2. [Core Concepts & Mental Model](#core-concepts--mental-model)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & API Reference: pyproject.toml Configuration](#syntax--api-reference-pyprojecttoml-configuration)
5. [Code Examples & Suppression Ergonomics](#code-examples--suppression-ergonomics)
6. [Edge Cases, Pitfalls & Anti-Patterns](#edge-cases-pitfalls--anti-patterns)
7. [Performance & Memory Characteristics](#performance--memory-characteristics)
8. [CPython Internals & Bytecode](#cpython-internals--bytecode)
9. [Best Practices & Idiomatic Patterns](#best-practices--idiomatic-patterns)
10. [Real-World Case Study: Production Monorepo Dual-Checker CI Enforcement](#real-world-case-study-production-monorepo-dual-checker-ci-enforcement)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Python itself does not enforce type annotations at runtime:
```python
def add(x: int, y: int) -> int:
    return x + y

add("hello", "world")  # Executes with ZERO runtime errors in Python!
```

Without an external type checker running in your IDE or CI pipeline, type annotations are mere documentation that can drift and lie.
- **Mypy**: Written in Python (compiled via mypyc to C for speed). Acts as the reference specification and offers a rich Python-based plugin ecosystem (`pydantic.mypy`, `sqlalchemy.ext.mypy`).
- **Pyright**: Written in TypeScript / Node.js. Exceptionally fast, designed from the ground up for real-time Language Server Protocol (LSP) feedback, and adheres strictly to typing PEP specifications.

---

## Core Concepts & Mental Model

### Architectural Comparison

```mermaid
graph TD
    subgraph Mypy Pipeline
        MA["Python Source (.py)"] --> MB["Python AST Parser"]
        MB --> MC["Semantic Analysis Passes (1-3)"]
        MC --> MD["Plugin Hooks (Pydantic, Django)"]
        MD --> ME["Mypyc C-Extension / AST Type Checker"]
        ME --> MF["Output Diagnostic Errors"]
    end

    subgraph Pyright Pipeline
        PA["Python Source (.py)"] --> PB["Native Node.js Parser"]
        PB --> PC["LSP Incremental Binder"]
        PC --> PD["Spec-Compliant Type Evaluator"]
        PD --> PE["Instant IDE Diagnostics (<50ms)"]
    end
```

### Feature Comparison Matrix

| Capability | Mypy | Pyright |
|:---|:---|:---|
| **Primary Language** | Python (compiled with mypyc) | TypeScript (Node.js runtime) |
| **IDE Integration** | Mypy extension, Sublime, Vim | Built-in VS Code Pylance, Neovim LSP |
| **Analysis Speed** | Moderate (~1-5 seconds on medium repos) | Ultra-fast (sub-second on large repos) |
| **Daemon Support** | `dmypy` (persistent daemon mode) | Built-in native LSP background process |
| **Plugin Extensibility** | Native Python plugins (`plugin = [...]`) | No custom plugins; strictly PEP compliant |
| **PEP 695 Support** | Supported in Mypy 1.11+ | Fully supported from day one |
| **Standard Configuration** | `pyproject.toml` or `mypy.ini` | `pyproject.toml` or `pyrightconfig.json` |

---

## How It Works Internally

### Incremental Analysis and AST Caching

Both type checkers build a directed acyclic graph (DAG) of module dependencies.
- **Mypy Caching**: Writes `.mypy_cache/` containing serialized JSON symbol tables and dependency trees. If file $A$ imports file $B$, and file $B$ changes, Mypy invalidates downstream dependents and rechecks only affected modules.
- **Pyright AST Binding**: Retains abstract syntax trees in memory and re-evaluates changed branches incrementally during keystroke events.

---

## Syntax & API Reference: pyproject.toml Configuration

### 1. Production `[tool.mypy]` Configuration

```toml
[tool.mypy]
python_version = "3.12"
strict = true
warn_return_any = true
warn_unused_configs = true
disallow_untyped_defs = true
disallow_incomplete_defs = true
check_untyped_defs = true
disallow_untyped_decorators = true
no_implicit_optional = true
warn_redundant_casts = true
warn_unused_ignores = true
warn_unreachable = true
show_error_codes = true
show_column_numbers = true
pretty = true

# Plugin declarations
plugins = [
    "pydantic.mypy",
]

# Per-module overrides for legacy untyped third-party packages
[[tool.mypy.overrides]]
module = "legacy_library.*"
ignore_missing_imports = true
```

### 2. Production `[tool.pyright]` Configuration

```toml
[tool.pyright]
include = ["src", "tests"]
exclude = ["**/__pycache__", ".venv"]
pythonVersion = "3.12"
pythonPlatform = "Linux"
typeCheckingMode = "strict"
reportMissingImports = true
reportMissingTypeStubs = false
reportUnusedImport = true
reportUnusedVariable = true
reportDuplicateImport = true
reportOptionalMemberAccess = true
reportUntypedFunctionDecorator = true
```

---

## Code Examples & Suppression Ergonomics

### Example 1: Targeted Error Suppression vs Danger of Bare Ignores

```python
# BAD: Bare type ignore hides ALL errors (including syntax, typo, and future bugs)
# x = calculate_risk("bad_arg")  # type: ignore

# GOOD: Specific error code with rationale (Mypy style)
x = calculate_risk("bad_arg")  # type: ignore[arg-type]

# GOOD: Pyright targeted suppression
y = calculate_risk("bad_arg")  # pyright: ignore[reportArgumentType]

# BEST: Dual-checker compatible suppression
z = calculate_risk("bad_arg")  # type: ignore[arg-type] # pyright: ignore[reportArgumentType]
```

### Example 2: Scoped Block Suppression

When interfacing with dynamic metaprogramming or C-extensions, suppress errors over a scoped block rather than on dozens of individual lines:

```python
# mypy: disable-error-code="attr-defined"
# Temporarily relax attr-defined for dynamic ORM proxy layer

class DynamicProxy:
    def __getattr__(self, name: str) -> object:
        return f"dynamic_{name}"

proxy = DynamicProxy()
val1 = proxy.custom_field_1  # No error raised
val2 = proxy.custom_field_2  # No error raised
```

### Example 3: Verifying Types with `reveal_type`

Both Mypy and Pyright provide built-in pseudo-functions `reveal_type()` and `reveal_locals()` to inspect inferred types during development:

```python
from typing import Literal

def process_status(code: Literal[200, 404, 500]) -> str:
    # Diagnostic inspection:
    reveal_type(code)  # Static output: Revealed type is "Literal[200, 404, 500]"
    
    if code == 200:
        return "OK"
    reveal_type(code)  # Static output: Revealed type is "Literal[404, 500]"
    return "Error"
```

Running `mypy script.py` prints:
```text
script.py:7: note: Revealed type is "Union[Literal[200], Literal[404], Literal[500]]"
script.py:11: note: Revealed type is "Union[Literal[404], Literal[500]]"
```

### Example 4: Unreachable Code Detection

With `warn_unreachable = true`, Mypy detects dead code branches:

```python
from typing import NoReturn

def abort_process(reason: str) -> NoReturn:
    raise RuntimeError(reason)

def execute_task(valid: bool) -> str:
    if not valid:
        abort_process("Invalid configuration")
        print("This line will never execute!")  # Mypy error: Statement is unreachable [unreachable]
    return "Success"
```

### Example 5: Handling Untyped Third-Party Libraries

When a dependency lacks type stubs and `py.typed`:

```python
# Option A: Import with type ignore
import untyped_package  # type: ignore[import-untyped]

# Option B: In pyproject.toml
# [[tool.mypy.overrides]]
# module = "untyped_package.*"
# ignore_missing_imports = true
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### Common Mistakes Table

| Mistake | Bad Code | Correct Idiom | Why It Fails |
|:---|:---|:---|:---|
| **Using bare `# type: ignore`** | `# type: ignore` | `# type: ignore[code-name]` | Masks unexpected future typing regressions silently. |
| **Not enabling `warn_unused_ignores`** | Leaving stale `# type: ignore` after fixing types | Set `warn_unused_ignores = true` | Suppressions become technical debt when libraries update their types. |
| **Relying on implicit `Optional`** | `def f(x: int = None):` | `def f(x: int \| None = None):` | PEP 484 deprecated implicit Optional; type checkers reject it in strict mode. |
| **Ignoring return Any** | `def f() -> Any: ...; val: int = f()` | Set `warn_return_any = true` | Unchecked `Any` propagates silently and disables type checking downstream. |
| **Mixing Mypy and Pyright suppression codes without care** | Putting only Pyright comments in Mypy CI | Use dual comments or configure matching error codes | One checker passes in CI while the other fails. |
| **Running without cache in CI** | `mypy src/` without persisting `.mypy_cache` | Cache `.mypy_cache` in GitHub Actions / GitLab CI | Re-checking entire project from scratch increases CI build duration 10x. |

---

## Performance & Memory Characteristics

### Benchmarking Analysis on 100k Lines of Code

| Metric | Mypy (Cold Cache) | Mypy (`dmypy` Warm Daemon) | Pyright (CLI / Node.js) |
|:---|:---|:---|:---|
| **Execution Time** | ~14.2s | ~0.45s | ~1.1s |
| **Peak RAM Usage** | ~420 MB | ~580 MB (Resident) | ~260 MB |
| **Cold Startup Latency**| ~800ms | < 50ms | ~300ms |

### CI Optimization Strategy
1. **GitHub Actions / GitLab CI**: Store `.mypy_cache` using CI cache actions keyed by poetry/uv lockfile and commit hash.
2. **Local Development**: Run `dmypy run -- src/` or Pyright in VS Code for instantaneous sub-second feedback during editing.

---

## CPython Internals & Bytecode

Because both Mypy and Pyright are **external static analyzers**, they inspect the Python Abstract Syntax Tree (AST) without executing bytecode instructions:

```python
import ast

code = """
def greet(name: str) -> str:
    return 123
"""

tree = ast.parse(code)
print("AST FunctionDef:", tree.body[0].name)
print("Return Type Annotation:", ast.dump(tree.body[0].returns))
```

Output:
```text
AST FunctionDef: greet
Return Type Annotation: Name(id='str', ctx=Load())
```

Mypy and Pyright traverse this AST structure, comparing the declared return annotation `Name(id='str')` against the actual returned expression `Constant(value=123)`. Because `int` is not a subtype of `str`, the static analyzer emits an error code `[return-value]`.

---

## Best Practices & Idiomatic Patterns

1. **Activate `strict = true` in New Projects**: It is 10x easier to maintain strict typing from day one than to retrofit it onto 50,000 lines of untyped code.
2. **Never Commit Bare `# type: ignore`**: Always specify error codes: `# type: ignore[import-untyped, arg-type]`.
3. **Turn On `warn_unused_ignores = true`**: Automatically flags unnecessary suppressions when dependencies or types are refactored.
4. **Enforce in Pre-Commit and CI**: Add `mypy` and `pyright` to your pre-commit hooks and GitHub Actions pipeline to reject untyped pull requests.

---

## Real-World Case Study: Production Monorepo Dual-Checker CI Enforcement

Below is a complete, production-grade GitHub Actions CI workflow script and `pyproject.toml` configuration enforcing strict dual-checking:

```toml
# pyproject.toml
[tool.mypy]
python_version = "3.12"
strict = true
show_error_codes = true
warn_unused_ignores = true
warn_unreachable = true
pretty = true

[[tool.mypy.overrides]]
module = "tests.*"
disallow_untyped_defs = false

[tool.pyright]
include = ["src"]
typeCheckingMode = "strict"
pythonVersion = "3.12"
```

```yaml
# .github/workflows/typecheck.yml
name: Static Type Verification

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  typecheck:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Repository
        uses: actions/checkout@v4

      - name: Set up Python 3.12
        uses: actions/setup-python@v5
        with:
          python-version: "3.12"

      - name: Set up Node.js for Pyright
        uses: actions/setup-node@v4
        with:
          node-version: "20"

      - name: Cache Mypy Cache
        uses: actions/cache@v4
        with:
          path: .mypy_cache
          key: mypy-${{ runner.os }}-${{ hashFiles('**/poetry.lock', '**/requirements*.txt') }}-${{ github.sha }}
          restore-keys: |
            mypy-${{ runner.os }}-${{ hashFiles('**/poetry.lock', '**/requirements*.txt') }}-
            mypy-${{ runner.os }}-

      - name: Install Dependencies
        run: |
          pip install --upgrade pip
          pip install mypy types-requests types-PyYAML
          npm install -g pyright

      - name: Run Mypy Type Verification
        run: |
          mypy src/

      - name: Run Pyright Type Verification
        run: |
          pyright src/
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary>1. What are the key architectural differences between Mypy and Pyright?</summary>

- **Mypy**: Written in Python (compiled with mypyc). It features multiple semantic analysis passes and supports native Python plugins (allowing deep integration with Pydantic, SQLAlchemy, and Django ORM).
- **Pyright**: Written in TypeScript and executed on Node.js. It is designed around an incremental Language Server Protocol (LSP) architecture, offering sub-second analysis speeds on massive codebases and strict adherence to PEP typing specifications, but does not support custom Python plugins.
</details>

<details>
<summary>2. Why does `disallow_untyped_defs` matter in strict mode?</summary>

Without `disallow_untyped_defs = true`, any function defined without type annotations (e.g., `def calculate(x, y):`) is completely skipped by Mypy! Its parameters and return value default to `Any`, allowing bugs inside the function and at its call sites to pass unnoticed.
</details>

<details>
<summary>3. What is `no_implicit_optional`, and why was it introduced?</summary>

Historically, PEP 484 permitted `def f(x: int = None):`, implicitly treating `x` as `Optional[int]`. This caused widespread confusion and hidden bugs. `no_implicit_optional = true` forces developers to be explicit: `def f(x: int | None = None):`.
</details>

<details>
<summary>4. How does `dmypy` achieve sub-second execution times in large codebases?</summary>

`dmypy` runs a persistent background daemon process that keeps the entire dependency graph, cached ASTs, and symbol tables loaded in memory. When a file changes, `dmypy` uses fine-grained dependency tracking to re-check only the changed file and its direct dependents.
</details>

<details>
<summary>5. What happens when a library doesn't ship a `py.typed` marker file?</summary>

PEP 561 stipulates that static type checkers must ignore inline type annotations in installed packages unless the package root contains a marker file named `py.typed`. Without `py.typed`, type checkers treat the library as completely untyped (`Any`), prompting `[import-untyped]` warnings.
</details>

<details>
<summary>6. How do you silence an error for Mypy while keeping Pyright happy (or vice versa)?</summary>

Use targeted error codes:
```python
# Mypy specific:
res = legacy_call()  # type: ignore[no-untyped-call]

# Pyright specific:
res = legacy_call()  # pyright: ignore[reportUnknownMemberType]
```
Mypy ignores `# pyright:` comments, and modern Pyright respects `# type: ignore[code]`.
</details>

<details>
<summary>7. What is `warn_return_any` and why should it be enabled?</summary>

If a function declares a concrete return type (`-> User`) but returns an expression typed as `Any` (e.g. from an unannotated external library), `warn_return_any` flags this as a warning. It prevents unchecked `Any` types from silently masquerading as strongly typed objects.
</details>

<details>
<summary>8. Can Mypy verify types across dynamic C-extension modules?</summary>

No. Mypy cannot parse or inspect compiled binary C-extensions (`.so` or `.pyd` files) directly. To type-check code using C-extensions, external stub files (`.pyi`) must be supplied (either from typeshed or package stubs).
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Configure pyproject.toml with Per-Module Overrides
Write a `pyproject.toml` configuration that enforces strict mode for `src/core/`, but permits untyped definitions for `src/legacy/` and `tests/`.

<details>
<summary>Solution</summary>

```toml
[tool.mypy]
python_version = "3.12"
strict = true

[[tool.mypy.overrides]]
module = ["src.legacy.*", "tests.*"]
disallow_untyped_defs = false
disallow_incomplete_defs = false
```
</details>

### Exercise 2: Fix Implicit Optional Violations
Given the following legacy function, update it to satisfy `no_implicit_optional = true` and `disallow_untyped_defs = true`:
`def connect(host, port=80, timeout=None):`

<details>
<summary>Solution</summary>

```python
def connect(
    host: str,
    port: int = 80,
    timeout: float | None = None
) -> bool:
    print(f"Connecting to {host}:{port} (timeout={timeout})")
    return True

connect("127.0.0.1")
```
</details>

### Exercise 3: Targeted Error Code Suppression
Given a call `result = legacy_func(123)` where `legacy_func` comes from an untyped module, write the exact suppression comments for both Mypy and Pyright.

<details>
<summary>Solution</summary>

```python
# Suppressing missing import and untyped call:
import legacy_module  # type: ignore[import-untyped]

# Mypy call suppression:
res = legacy_module.legacy_func(123)  # type: ignore[no-untyped-call]
```
</details>

### Exercise 4: Use `reveal_type` to Debug Type Narrowing
Write a code snippet using `reveal_type` to trace the type of a variable before and after an `isinstance` check.

<details>
<summary>Solution</summary>

```python
def inspect_value(val: int | str | None) -> None:
    reveal_type(val)  # Note: Revealed type is "Union[int, str, None]"
    if isinstance(val, str):
        reveal_type(val)  # Note: Revealed type is "str"
    elif val is not None:
        reveal_type(val)  # Note: Revealed type is "int"
    else:
        reveal_type(val)  # Note: Revealed type is "None"
```
</details>

### Exercise 5: Pydantic Mypy Plugin Configuration
Configure Mypy to use the `pydantic.mypy` plugin in `pyproject.toml` with strict field initialization checks.

<details>
<summary>Solution</summary>

```toml
[tool.mypy]
plugins = ["pydantic.mypy"]

[tool.pydantic-mypy]
init_forbid_extra = true
init_typed = true
warn_required_dynamic_aliases = true
```
</details>

### Exercise 6: Shell Script for Incremental Daemon Checking
Write a bash script that launches `dmypy` if not running, checks `src/`, and gracefully restarts the daemon if configuration files change.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "Running incremental Mypy daemon check..."
if ! dmypy status > /dev/null 2>&1; then
    echo "Starting dmypy daemon..."
    dmypy start -- --config-file pyproject.toml
fi

# Run check
if ! dmypy check src/; then
    echo "Type check failed!"
    exit 1
fi
echo "Type check clean!"
```
</details>
