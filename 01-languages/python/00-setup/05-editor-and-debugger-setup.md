# Module 00: Editor & Debugger Setup (05-editor-and-debugger-setup)

> **Topic Overview**: Configuring professional developer environments for Python: VS Code, PyCharm, Neovim, Language Server Protocols (Pyright / Ty), the Astral Ruff extension, debugpy architecture, breakpoint debugging, and launch configurations.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Setup, Toolchains & Python Runtime |
| **Target Level** | Beginner to Senior Software Engineer |
| **Python Version** | Python 3.10+ (tested on CPython 3.14.0) |
| **Tested With** | VS Code / debugpy, Ruff 0.17.6, Mypy 2.4.1 |
| **Free-Threading / Platform** | Platform independent |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Code Examples](#5-code-examples)
6. [Edge Cases & Pitfalls](#6-edge-cases--pitfalls)
7. [Common Mistakes](#7-common-mistakes)
8. [Performance & Complexity](#8-performance--complexity)
9. [CPython Internals & Bytecode](#9-cpython-internals--bytecode)
10. [Best Practices](#10-best-practices)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
- **Problem Statement**: Writing Python code without automated editor tooling leads to syntax errors discovered only at runtime, inconsistent formatting across team members, unnoticed type bugs, and painful debugging using scattered `print()` statements.
- **Mental Model**: Modern editors decouple the user interface from language intelligence using standardized protocols: the **Language Server Protocol (LSP)** for autocomplete, hover documentation, and diagnostics; and the **Debug Adapter Protocol (DAP)** for breakpoint debugging and variable inspection.
- **When to Use**: Every professional Python developer workflow.
- **When NOT to Use**: Running automated headless production scripts in CI/CD containers (use CLI tools).

---

## 2. Core Concepts
1. **Language Server Protocol (LSP)**: Microsoft's open protocol allowing editors (VS Code, Neovim, Emacs) to communicate via JSON-RPC with language servers like **Pyright** (or Astral's Rust-based **Ty**) to provide real-time type checking, semantic tokens, and refactoring.
2. **Debug Adapter Protocol (DAP)**: Standardized protocol used by **`debugpy`** to connect GUI debuggers to the CPython runtime, setting hardware/software breakpoints, evaluating watch expressions, and inspecting call stacks.
3. **Astral Ruff VS Code Extension**: Replaces legacy Flake8, Black, and isort extensions with a single ultra-fast Rust server that formats on save in under 10 milliseconds.
4. **`launch.json`**: VS Code configuration file specifying how debuggers launch or attach to running Python processes, passing CLI arguments and environment variables.

---

## 3. How It Works Internally

How the Debug Adapter Protocol (`debugpy`) controls CPython execution:

```text
VS Code Editor UI
       │
       ▼  [ JSON-RPC over TCP / Named Pipe ]
debugpy Daemon Process
       │
       ▼  [ sys.settrace / PyEval_SetTrace ]
CPython Virtual Machine
       │
       ├─► Attaches trace function to every newly spawned frame
       ├─► Line Event: Evaluates breakpoint conditions
       │      └─► MATCH: Pauses execution thread
       │
       ▼  Sends StoppedEvent to VS Code
Editor highlights current source line, populates Scope/Variables tree
```

---

## 4. Syntax & API Reference

### Recommended VS Code `settings.json`
```json
{
  "[python]": {
    "editor.defaultFormatter": "charliermarsh.ruff",
    "editor.formatOnSave": true,
    "editor.codeActionsOnSave": {
      "source.fixAll": "explicit",
      "source.organizeImports": "explicit"
    }
  },
  "python.analysis.typeCheckingMode": "strict",
  "python.defaultInterpreterPath": "${workspaceFolder}/.venv/Scripts/python.exe"
}
```

### Production VS Code `launch.json`
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Python: Current File",
      "type": "debugpy",
      "request": "launch",
      "program": "${file}",
      "console": "integratedTerminal",
      "justMyCode": true
    },
    {
      "name": "Python: Fastify / FastAPI Service",
      "type": "debugpy",
      "request": "launch",
      "module": "uvicorn",
      "args": ["main:app", "--reload", "--port", "8000"],
      "jinja": false
    }
  ]
}
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Breakpoint Debugging with Built-in `breakpoint()`
```python
def calculate_factorial(n: int) -> int:
    """Calculates factorial with an intentional debug tracepoint."""
    if n < 0:
        raise ValueError("Factorial not defined for negative numbers")
    res = 1
    for i in range(1, n + 1):
        res *= i
        # In an interactive debug session, calling breakpoint() pauses execution
        # breakpoint()
    return res

if __name__ == "__main__":
    result = calculate_factorial(5)
    print(f"Factorial of 5 = {result}")
```
**Output**:
```text
Factorial of 5 = 120
```

---

### 2. Real-World Production Example: Programmatic Remote Debugging with `debugpy`
```python
import sys

def attach_remote_debugger(port: int = 5678) -> None:
    """Enables remote debugging for containerized or background services."""
    try:
        import debugpy  # type: ignore[import-untyped]
        
        debugpy.listen(("0.0.0.0", port))
        print(f"Debugpy listening on port {port}. Awaiting debugger attach...")
        # debugpy.wait_for_client()  # Blocks until IDE attaches
    except ImportError:
        print("debugpy not installed; continuing in production mode.")

if __name__ == "__main__":
    attach_remote_debugger()
    print("Application proceeding with normal execution.")
```
**Output**:
```text
debugpy not installed; continuing in production mode.
Application proceeding with normal execution.
```

---

### 3. Edge Case: Controlling `sys.breakpointhook`
```python
import sys

def disabled_breakpoint(*args: object, **kwargs: object) -> None:
    """Disables breakpoint() calls completely in production environments."""
    print("WARNING: breakpoint() was called but has been silenced by production hook.")

# Overriding default breakpointhook
sys.breakpointhook = disabled_breakpoint

breakpoint()
print("Execution continued uninterrupted.")
```
**Output**:
```text
WARNING: breakpoint() was called but has been silenced by production hook.
Execution continued uninterrupted.
```

---

### 4. Failure Case: Unhandled Exception in Fast local Scope
```python
def compute_ratio(numerator: int, denominator: int) -> float:
    # A common bug that an IDE debugger catches instantly on the frame stack
    return numerator / denominator

try:
    compute_ratio(100, 0)
except ZeroDivisionError as exc:
    print(f"Caught expected error: {type(exc).__name__}: {exc}")
```
**Output**:
```text
Caught expected error: ZeroDivisionError: division by zero
```

---

### 5. Idiomatic Pythonic Pattern: Environment-Driven Breakpoint Hook via `PYTHONBREAKPOINT`
```bash
# In your terminal or launch configuration:
# Disable all breakpoints without modifying code:
$ export PYTHONBREAKPOINT=0
$ python app.py

# Route breakpoints to custom debugger (e.g. pudb):
$ export PYTHONBREAKPOINT=pudb.set_trace
$ python app.py
```

---

## 6. Edge Cases & Pitfalls
1. **`justMyCode: true` vs Library Debugging**: By default, VS Code sets `justMyCode: true`, which prevents the debugger from stepping into standard library or third-party package source code. If you need to trace into requests or pydantic internals, set `"justMyCode": false` in `launch.json`.
2. **Subprocess Debugging**: If your application spawns worker processes via `multiprocessing` or `subprocess`, standard debuggers miss child processes unless `"subProcess": true` is explicitly enabled in `launch.json`.
3. **`PYTHONBREAKPOINT=0` in Production**: Ensure production container environments set `PYTHONBREAKPOINT=0` to prevent accidental developer `breakpoint()` statements from hanging production services.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Hardcoding absolute interpreter paths in `settings.json` | Breaks when other team members clone repo | `Invalid Python interpreter selected` | Use `${workspaceFolder}/.venv/...` |
| Leaving `print()` statements for debugging | Clutters logs, pollutes stdout, and exposes secrets | Production log pollution | Use `breakpoint()` or structured logging |
| Debugging multi-threaded code with `justMyCode: true` | Stepping skips external library worker threads | Breakpoints appear not to hit | Set `"justMyCode": false` and `"subProcess": true` |
| Forgetting to install the Python extension in VS Code | Editor treats `.py` files as plain text | No autocomplete or syntax diagnostics | Install official Microsoft Python & Ruff extensions |
| Running multiple formatters simultaneously (Black + Ruff) | Conflicting rules cause formatting race conditions | Editor cursor jumps and endless file saves | Select a single formatter (`charliermarsh.ruff`) |
| Committing `.vscode/` with developer-specific credentials | Leaks personal paths or tokens | Security and git hygiene issue | Add `.vscode/*.local.json` to `.gitignore` |

---

## 8. Performance & Complexity
- **Tracing Overhead in CPython**: When a debugger attaches, CPython activates `sys.settrace()`, which evaluates a Python callback on every single line and call event. This can slow down pure Python loops by $10\times - 30\times$.
- **LSP Memory Overhead**: Language servers (Pyright, PyCharm backend) index all dependencies in the virtual environment, typically consuming 100–300 MB of RAM for large codebases.

---

## 9. CPython Internals & Bytecode: The `sys.settrace` API
Under the hood, `debugpy` and the `pdb` module rely on `sys.settrace(trace_function)`:
```python
import sys

def trace_calls(frame, event, arg):
    if event == "call":
        print(f"Calling: {frame.f_code.co_name}")
    return trace_calls

# The CPython VM checks PyThreadState.c_tracefunc on every bytecode dispatch!
```

---

## 10. Best Practices
1. Commit `.vscode/extensions.json` recommending the official Python, Ruff, and Mypy extensions to your team.
2. Commit a sanitized `.vscode/launch.json` providing standard launch targets for local testing and test runs.
3. Configure format-on-save with `ruff` to enforce zero-friction formatting consistency.
4. Set `PYTHONBREAKPOINT=0` in production environments.

---

## 11. Real-World Case Study: Diagnosing Deadlocks in Async Microservices
A backend team observed intermittent worker timeouts in an async FastAPI service when handling WebSockets:
1. They configured VS Code with `"subProcess": true` and attached `debugpy` to the local worker process.
2. Rather than guessing with print logs, they triggered a deadlock scenario and clicked "Pause" in the IDE.
3. The Call Stack panel immediately revealed the issue: a synchronous blocking database call was invoked inside an async event loop thread, completely starving the asyncio event loop.
4. The issue was resolved in one commit by offloading the blocking call to `asyncio.to_thread()`.

---

## 12. Interview Questions

### Q1: What is the Language Server Protocol (LSP) and why is it beneficial?
<details>
<summary><strong>Answer</strong></summary>
The LSP is an open standard that decouples language analysis tools (like Pyright or Ty) from client editors (like VS Code, Neovim, or Sublime Text). Instead of every editor re-implementing parsers and type checkers for every language ($M \times N$ complexity), a single language server communicates with any supporting editor via standard JSON-RPC ($M + N$ complexity).
</details>

### Q2: What happens when the built-in `breakpoint()` function is executed?
<details>
<summary><strong>Answer</strong></summary>
Introduced in Python 3.7 (PEP 553), `breakpoint()` calls `sys.breakpointhook(*args, **kwargs)`. By default, `sys.breakpointhook` inspects the `PYTHONBREAKPOINT` environment variable. If unset or empty, it imports `pdb` and calls `pdb.set_trace()`. If set to `0`, it returns immediately. If set to an importable path (e.g. `ipdb.set_trace`), it imports and executes that function.
</details>

### Q3: How does the Debug Adapter Protocol (DAP) differ from the Language Server Protocol (LSP)?
<details>
<summary><strong>Answer</strong></summary>
LSP handles static code analysis, semantic tokens, hover documentation, diagnostics, and code completions prior to or during writing. DAP handles dynamic runtime execution: pausing threads, setting breakpoints, inspecting variables on the call stack, and evaluating expressions in live frames.
</details>

### Q4: Why does running under an active debugger significantly slow down Python code execution?
<details>
<summary><strong>Answer</strong></summary>
Debuggers install a trace function via `sys.settrace()` (or the C-level `PyEval_SetTrace`). In this mode, CPython's bytecode evaluation loop cannot optimize bytecode dispatch and must invoke the trace callback on every function call, line execution, exception, and return event.
</details>

### Q5: What does `"justMyCode": true` do in a VS Code debug configuration?
<details>
<summary><strong>Answer</strong></summary>
`justMyCode: true` restricts debugging solely to the source files within the workspace. The debugger automatically steps over and ignores breakpoints inside standard library modules or third-party packages installed in `site-packages`.
</details>

### Q6: How can you disable all `breakpoint()` calls in a container without changing source files?
<details>
<summary><strong>Answer</strong></summary>
By setting the environment variable `PYTHONBREAKPOINT=0`. When set to string `"0"`, CPython's default `sys.breakpointhook` immediately returns without doing any work.
</details>

### Q7: What is the purpose of `debugpy` in Docker containers?
<details>
<summary><strong>Answer</strong></summary>
`debugpy` acts as a server running inside the Docker container, listening on a network port (e.g., 5678). Developers on the host machine can attach their local IDE debugger to the containerized process across the network port, enabling interactive breakpoint debugging on code executing inside the container.
</details>

### Q8: What is the difference between Pyright and Mypy?
<details>
<summary><strong>Answer</strong></summary>
Mypy is the authoritative reference static type checker written in Python, commonly used for strict CI/CD gatekeeping. Pyright is a high-speed type checker written in TypeScript by Microsoft, designed for near-instant interactive feedback in editors (VS Code's Pylance), offering faster project re-indexing and comprehensive LSP features.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Detecting Breakpoint Status Programmatically
Write a function `is_breakpoint_enabled()` that inspects `os.environ` and `sys.breakpointhook` to return whether calling `breakpoint()` will actively trigger a debugger.
<details>
<summary><strong>Solution</strong></summary>

```python
import os
import sys

def is_breakpoint_enabled() -> bool:
    env_val = os.environ.get("PYTHONBREAKPOINT", "").strip()
    if env_val == "0":
        return False
    return sys.breakpointhook is not sys.__breakpointhook__ or env_val != "0"

print(f"Breakpoints active: {is_breakpoint_enabled()}")
```
</details>

### Challenge 2 (Easy): Validating Interpreter Path in VS Code Settings
Write a script that parses a `.vscode/settings.json` file and verifies that the configured interpreter path exists on disk.
<details>
<summary><strong>Solution</strong></summary>

```python
import json
from pathlib import Path

def validate_vscode_settings(settings_file: Path) -> bool:
    if not settings_file.exists():
        return True
    data = json.loads(settings_file.read_text(encoding="utf-8"))
    interpreter = data.get("python.defaultInterpreterPath")
    if interpreter and not Path(interpreter).exists():
        print(f"Warning: Configured interpreter not found: {interpreter}")
        return False
    return True
```
</details>

### Challenge 3 (Medium): Custom Line-by-Line Execution Tracer
Write a context manager `trace_execution()` that uses `sys.settrace` to print each line number executed within its block.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections.abc import Generator
import contextlib
import sys
from typing import Any

@contextlib.contextmanager
def trace_execution() -> Generator[None, None, None]:
    def tracer(frame: Any, event: str, arg: Any) -> Any:
        if event == "line":
            print(f"Executing line {frame.f_lineno} in {frame.f_code.co_name}")
        return tracer

    old_trace = sys.gettrace()
    sys.settrace(tracer)
    try:
        yield
    finally:
        sys.settrace(old_trace)

with trace_execution():
    a = 10
    b = 20
    c = a + b
```
</details>

### Challenge 4 (Medium): Conditional Debugger Breakpoint Helper
Write a helper function `break_if(condition: bool, msg: str)` that invokes `breakpoint()` only when a specific boolean condition holds, logging a message first.
<details>
<summary><strong>Solution</strong></summary>

```python
def break_if(condition: bool, msg: str = "") -> None:
    if condition:
        if msg:
            print(f"Conditional breakpoint triggered: {msg}")
        # In interactive use: breakpoint()

break_if(10 > 5, "Number exceeds threshold")
```
</details>

### Challenge 5 (Hard): Remote Debugpy Initialization Decorator
Write a decorator `@debug_on_failure` that, upon catching an unexpected exception, starts a `debugpy` listener on port 5678 and awaits an IDE attachment before re-raising.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections.abc import Callable
import functools
import sys
from typing import TypeVar

F = TypeVar("F", bound=Callable[..., object])

def debug_on_failure(func: F) -> F:
    @functools.wraps(func)
    def wrapper(*args: object, **kwargs: object) -> object:
        try:
            return func(*args, **kwargs)
        except Exception as exc:
            print(f"Exception in {func.__name__}: {exc}")
            try:
                import debugpy  # type: ignore[import-untyped]
                if not debugpy.is_client_connected():
                    debugpy.listen(("0.0.0.0", 5678))
                    print("Debugpy listening on 5678. Attach debugger now...")
                    # debugpy.wait_for_client()
            except ImportError:
                pass
            raise
    return wrapper  # type: ignore[return-value]
```
</details>

### Challenge 6 (Hard): Inspecting Live Call Stack Frames
Write a function `dump_call_stack()` that introspects the active thread's frame stack using `inspect.currentframe()` and returns a list of dictionaries with function names, filenames, line numbers, and local variables.
<details>
<summary><strong>Solution</strong></summary>

```python
import inspect

def dump_call_stack() -> list[dict[str, object]]:
    stack = []
    frame = inspect.currentframe()
    if frame is not None:
        frame = frame.f_back  # Step out of dump_call_stack
    while frame is not None:
        stack.append({
            "function": frame.f_code.co_name,
            "filename": frame.f_code.co_filename,
            "lineno": frame.f_lineno,
            "locals_keys": list(frame.f_locals.keys()),
        })
        frame = frame.f_back
    return stack

def sample_caller() -> list[dict[str, object]]:
    local_val = 42
    return dump_call_stack()

print(sample_caller()[0]["function"])
```
</details>
