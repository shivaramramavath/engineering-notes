# Module 00: The Python REPL & First Program (04-repl-and-first-program)

> **Topic Overview**: The Python interactive shell (REPL): Python 3.13+ enhanced REPL with colorized tracebacks and multiline editing, standard streams, primary/secondary prompts (`sys.ps1`/`sys.ps2`), the special underscore (`_`) variable, interactive debugging, and authoring your first Python application.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Setup, Toolchains & Python Runtime |
| **Target Level** | Beginner to Intermediate |
| **Python Version** | Python 3.10+ (incorporating Python 3.13+ enhanced REPL) |
| **Tested With** | CPython 3.14.0, Windows Terminal / PowerShell |
| **Free-Threading / Platform** | Supported across all platforms |

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
- **Problem Statement**: Developers need an immediate feedback loop to test small code expressions, introspect object attributes, inspect runtime behavior, and prototype algorithms without creating and saving temporary files to disk.
- **Mental Model**: A **REPL** (**R**ead-**E**val-**P**rint-**L**oop) continuously reads user input from standard input, compiles and evaluates the expression in the global scope, formats and prints the result to standard output, and loops indefinitely until exited.
- **When to Use**: Quick expression testing, inspecting third-party library signatures (`help()`, `dir()`), exploratory data analysis, and step-by-step debugging.
- **When NOT to Use**: Building production applications or authoring complex multi-module logic (use structured files and version-controlled scripts).

---

## 2. Core Concepts
1. **The Python 3.13+ Enhanced REPL**: Based on code from PyPy, Python 3.13 introduced a completely revamped interactive shell featuring multiline editing with arrow keys, syntax highlighting, colorized tracebacks, and interactive `help()` pagination without needing external packages like `IPython` or `bpython`.
2. **Standard Prompts (`sys.ps1` and `sys.ps2`)**:
   - `sys.ps1`: Primary prompt, defaults to `>>> `.
   - `sys.ps2`: Secondary prompt shown when entering multiline blocks (indented blocks, loops, function definitions), defaults to `... `.
3. **The Special Underscore (`_`) Variable**: In the interactive REPL, the special variable `_` automatically stores the result of the previously evaluated expression.
4. **Interactive Exit**: In Python 3.13+, typing `exit` or `quit` exits immediately without requiring parentheses `exit()` or EOF signals (`Ctrl+D` / `Ctrl+Z + Enter`).

---

## 3. How It Works Internally

The execution lifecycle of a single REPL interaction:

```text
User Input Terminal (e.g. `2 + 3` or `def add(a, b): ...`)
            │
            ▼
    [ READ ] -> Captures line from sys.stdin via PyOS_Readline
            │
            ▼
    [ COMPILE ] -> Compiles string with mode='single' via PyRun_InteractiveOneObject
            │
            ▼
    [ EVAL ] -> Evaluates bytecode frame in module '__main__' globals
            │
            ▼
    [ PRINT ] -> If value is not None:
            │      Calls sys.displayhook(value)
            │      Assigns builtins._ = value
            │      Writes repr(value) to sys.stdout
            │
            ▼
    [ LOOP ] -> Displays sys.ps1 and awaits next user line
```

---

## 4. Syntax & API Reference

### Launching the REPL
```bash
# Launch standard interactive shell
python

# Launch REPL with inspect flag after running a script
python -i script.py

# Launch isolated REPL without site-packages or user environment variables
python -s -E

# Launch REPL using uv
uv run python
```

### REPL Built-in Introspection Commands
```python
help(object)      # Displays built-in documentation and method signatures
dir(object)       # Lists all attributes and methods of an object
type(object)      # Returns the concrete class/type of an object
id(object)        # Returns the unique memory address integer of an object
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Authoring a Clean First Program
```python
import sys

def greet(name: str) -> str:
    """Returns a greeting string annotated with the active Python version."""
    major = sys.version_info.major
    minor = sys.version_info.minor
    return f"Hello, {name}! Welcome to Python {major}.{minor}."

if __name__ == "__main__":
    message = greet("Engineer")
    print(message)
```
**Output**:
```text
Hello, Engineer! Welcome to Python 3.14.
```

---

### 2. Real-World Production Example: Customizing REPL Prompts via `PYTHONSTARTUP`
```python
import sys

def configure_interactive_session() -> None:
    """Configures primary and secondary REPL prompts with environment context."""
    # Customizing sys.ps1 and sys.ps2
    sys.ps1 = "py[dev]>>> "
    sys.ps2 = "py[dev]... "
    print("Interactive session configured with custom prompt.")

if __name__ == "__main__":
    configure_interactive_session()
    print(f"Primary Prompt: '{sys.ps1}'")
    print(f"Secondary Prompt: '{sys.ps2}'")
```
**Output**:
```text
Interactive session configured with custom prompt.
Primary Prompt: 'py[dev]>>> '
Secondary Prompt: 'py[dev]... '
```

---

### 3. Edge Case: Customizing `sys.displayhook`
```python
import sys

def custom_displayhook(value: object) -> None:
    """Custom REPL display hook that formats numbers with hex representation."""
    if value is not None:
        # Update builtins._
        __builtins__._ = value  # type: ignore[attr-defined]
        if isinstance(value, int):
            sys.stdout.write(f"{value} (hex: {hex(value)})\n")
        else:
            sys.stdout.write(f"{repr(value)}\n")

# Test the display hook logic directly
custom_displayhook(255)
custom_displayhook("Hello World")
```
**Output**:
```text
255 (hex: 0xff)
'Hello World'
```

---

### 4. Failure Case: Typing `exit` vs `exit()` on Older Python Releases
```python
# In Python 3.12 and earlier, typing 'exit' printed a helper string instead of exiting:
class SimulatedExit:
    def __repr__(self) -> str:
        return "Use exit() or Ctrl-Z plus Return to exit"

simulated_exit = SimulatedExit()
print(repr(simulated_exit))
```
**Output**:
```text
Use exit() or Ctrl-Z plus Return to exit
```
*(In Python 3.13+, the new interactive REPL catches bare `exit` and `quit` and cleanly terminates the process).*

---

### 5. Idiomatic Pythonic Pattern: The Canonical `__name__ == "__main__"` Boilerplate
```python
import sys

def calculate_service_metrics(throughput: float, latency_ms: float) -> dict[str, float]:
    """Calculates efficiency metrics for a backend service."""
    return {
        "throughput_rps": throughput,
        "latency_p99_ms": latency_ms,
        "efficiency_score": round(throughput / max(latency_ms, 0.001), 2),
    }

def main() -> None:
    metrics = calculate_service_metrics(12500.0, 14.2)
    print("Production Service Metrics:")
    for key, val in metrics.items():
        print(f"  {key}: {val}")

if __name__ == "__main__":
    main()
```
**Output**:
```text
Production Service Metrics:
  throughput_rps: 12500.0
  latency_p99_ms: 14.2
  efficiency_score: 880.28
```

---

## 6. Edge Cases & Pitfalls
1. **Interactive Scope Leaks**: Variables defined during an interactive session live in `__main__` globals until the shell terminates. Accidental name shadowing can lead to unexpected behavior during prolonged sessions.
2. **Difference Between `single` and `exec` Compilation**: The REPL compiles each line using `mode="single"`. Unlike `mode="exec"`, `mode="single"` prints the evaluated expression if it produces a non-`None` result.
3. **`_` Variable Overwrite**: If you assign a variable to `_` in code (`_ = 10` or in a loop `for _ in range(5):`), it overrides Python's automatic REPL result caching variable until deleted with `del _`.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Omitting `if __name__ == "__main__":` | Code executes immediately when imported by other modules or multiprocessing | Unintended execution and process fork bombs | Wrap top-level scripts in `if __name__ == "__main__":` |
| Pasting tabs and spaces into the REPL | Python enforces strict indentation consistency | `IndentationError: unexpected indent` | Configure REPL bracketed paste or use modern REPL |
| Typing `exit()` in non-standard embedded Python | `exit` and `quit` come from `site.py`; running `python -S` omits them | `NameError: name 'exit' is not defined` | Use `import sys; sys.exit(0)` |
| Expecting `print()` to return a value to `_` | `print()` outputs to stdout but returns `None` | `_` retains previous non-None value | Evaluate expressions directly without `print()` in REPL |
| Modifying `PYTHONSTARTUP` with broken syntax | Crashes every interactive Python session upon launch | Silent failure or interactive shell crash | Test startup scripts standalone before setting env var |
| Running long CPU loops directly in REPL without interrupts | Blocks terminal input until computation completes | High CPU usage | Use `Ctrl+C` (`KeyboardInterrupt`) to halt execution |

---

## 8. Performance & Complexity
- **Compilation Latency**: The REPL compiles individual statements into transient code objects in sub-millisecond times ($<0.1$ ms).
- **Interactive Memory Footprint**: Minimal; standard interactive CPython sessions consume ~10–15 MB of RAM.

---

## 9. CPython Internals & Bytecode: The `compile(..., mode="single")` Mode
Python has three distinct compilation modes:
- `"exec"`: Compiles arbitrary module-level statements; expressions are evaluated and discarded without printing.
- `"eval"`: Compiles a single expression and returns its evaluated value.
- `"single"`: Used exclusively by the REPL. If the compiled code is an expression statement, it appends a `PRINT_EXPR` bytecode instruction, routing the result to `sys.displayhook`.

```python
import dis

# Inspecting REPL single compilation:
code_obj = compile("x + 5", "<stdin>", "single")
dis.dis(code_obj)
```
Disassembly:
```text
  1           0 RESUME                   0
              2 LOAD_NAME                0 (x)
              4 LOAD_CONST               0 (5)
              6 BINARY_OP                0 (+)
             10 PRINT_EXPR              # Automatically emits result to sys.displayhook!
             12 RETURN_CONST             0 (None)
```

---

## 10. Best Practices
1. Guard all executable scripts with `if __name__ == "__main__": main()`.
2. Use the `-i` flag (`python -i script.py`) to drop into an interactive session after executing a script for instant debugging.
3. Configure `PYTHONSTARTUP` to automatically import frequently used utilities (e.g., `import json, sys, os`).
4. Rely on `sys.exit(code)` for programmatic script termination with exit codes.

---

## 11. Real-World Case Study: Post-Mortem Interactive Inspection with `python -i`
A data engineering team experienced intermittent schema parse crashes in a 20-minute ETL batch job. Instead of adding temporary `print()` statements and re-running the slow batch:
1. They invoked the job with `python -i etl_job.py`.
2. When the unhandled exception crashed execution, Python halted at the exact point of failure, dropping into the REPL with all in-memory variables and state intact.
3. The engineers inspected the corrupted payload variable directly in the shell, diagnosed the malformed date string, and developed the fix in minutes.

---

## 12. Interview Questions

### Q1: What does `if __name__ == "__main__":` actually do?
<details>
<summary><strong>Answer</strong></summary>
Every Python module has a built-in string variable named `__name__`. When a Python file is executed directly as the top-level script (e.g., `python app.py`), CPython sets `__name__ = "__main__"`. If the file is imported into another script (e.g., `import app`), CPython sets `__name__ = "app"`. The guard ensures that entrypoint code only runs upon direct invocation, preventing unwanted side effects during imports.
</details>

### Q2: What is the purpose of the special `_` variable in the interactive shell?
<details>
<summary><strong>Answer</strong></summary>
In the interactive REPL, the special variable `_` is maintained by `sys.displayhook` and holds the result of the most recently evaluated expression that returned a non-`None` value.
</details>

### Q3: How did Python 3.13 improve the interactive REPL?
<details>
<summary><strong>Answer</strong></summary>
Python 3.13 replaced the basic readline-based REPL with an enhanced interactive shell adapted from PyPy:
1. Multi-line editing with arrow navigation and history.
2. Native syntax highlighting and colorized tracebacks.
3. Interactive `help()` with built-in paging.
4. Support for exiting by typing `exit` or `quit` without parentheses.
</details>

### Q4: What is the difference between `sys.ps1` and `sys.ps2`?
<details>
<summary><strong>Answer</strong></summary>
`sys.ps1` defines the primary interactive prompt string (default `>>> `) shown when waiting for the start of a new statement. `sys.ps2` defines the secondary prompt string (default `... `) shown when continuing a multiline compound statement (such as a function definition or loop).
</details>

### Q5: What is `PYTHONSTARTUP`?
<details>
<summary><strong>Answer</strong></summary>
`PYTHONSTARTUP` is an operating system environment variable pointing to a Python file. If set, CPython executes the specified file before displaying the primary prompt whenever an interactive shell session is initiated.
</details>

### Q6: How does the `PRINT_EXPR` opcode work?
<details>
<summary><strong>Answer</strong></summary>
In bytecode compiled with mode `"single"` (used by the REPL), an expression statement is followed by the `PRINT_EXPR` instruction. When executed by the virtual machine loop, it pops the top of the stack, checks if it is `None`, and if not, passes it to `sys.displayhook`, which prints its representation and sets `builtins._`.
</details>

### Q7: Why do `exit()` and `quit()` not work when running with `python -S`?
<details>
<summary><strong>Answer</strong></summary>
`exit` and `quit` are helper objects added to the built-in namespace by the standard library `site.py` module during startup. Passing the `-S` flag disables `site.py` importing, leaving the built-in namespace without those helpers. In such environments, one must use `import sys; sys.exit()`.
</details>

### Q8: What is the difference between `python -c "..."` and `python -m <module>`?
<details>
<summary><strong>Answer</strong></summary>
`python -c "..."` compiles and executes an inline string of Python code passed as a command-line argument. `python -m <module>` searches `sys.path` for the specified module or package, loads its `__main__.py` if it is a package, and executes it as the top-level script (`__name__ == "__main__"`).
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Basic Greeting Function
Write a standalone Python script containing a function `greet_user(name)` that prints a customized welcome message and is cleanly guarded by `if __name__ == "__main__":`.
<details>
<summary><strong>Solution</strong></summary>

```python
def greet_user(name: str) -> None:
    print(f"Welcome to Python Engineering, {name}!")

if __name__ == "__main__":
    greet_user("Alex")
```
</details>

### Challenge 2 (Easy): Inspecting Python Keywords Interactively
Write a script that uses the standard library `keyword` module to list all reserved Python keywords.
<details>
<summary><strong>Solution</strong></summary>

```python
import keyword

def list_keywords() -> list[str]:
    return keyword.kwlist

print(f"Total Python keywords: {len(list_keywords())}")
print(list_keywords()[:10])
```
</details>

### Challenge 3 (Medium): Custom Dynamic Primary Prompt
Write a custom prompt class for `sys.ps1` that dynamically increments and displays the current line number for each command.
<details>
<summary><strong>Solution</strong></summary>

```python
import sys

class NumberedPrompt:
    def __init__(self) -> None:
        self.count = 0

    def __str__(self) -> str:
        self.count += 1
        return f"[{self.count}]>>> "

# In the REPL: sys.ps1 = NumberedPrompt()
p = NumberedPrompt()
print(str(p))
print(str(p))
```
</details>

### Challenge 4 (Medium): Custom Displayhook with Timing Information
Write a custom `sys.displayhook` implementation that prints the execution timestamp along with the evaluated representation.
<details>
<summary><strong>Solution</strong></summary>

```python
from datetime import datetime, timezone
import sys

def timed_displayhook(value: object) -> None:
    if value is not None:
        __builtins__._ = value  # type: ignore[attr-defined]
        now = datetime.now(timezone.utc).strftime("%H:%M:%S")
        sys.stdout.write(f"[{now} UTC] {repr(value)}\n")

timed_displayhook("Test expression")
```
</details>

### Challenge 5 (Hard): Replicating a Minimal Python REPL
Implement a lightweight pure-Python REPL loop using `input()`, `compile(..., mode="single")`, and `exec()` that supports multiline expression compilation and handles `KeyboardInterrupt` without terminating.
<details>
<summary><strong>Solution</strong></summary>

```python
import sys
import traceback

def mini_repl() -> None:
    scope: dict[str, object] = {"__name__": "__main__"}
    print("Mini Python REPL (type 'exit' to quit)")
    
    while True:
        try:
            line = input("mini>>> ")
            if line.strip() in ("exit", "quit"):
                break
            if not line.strip():
                continue
                
            code = compile(line, "<mini-repl>", "single")
            exec(code, scope)
        except KeyboardInterrupt:
            print("\nKeyboardInterrupt")
        except Exception:
            traceback.print_exc()

# Call mini_repl() in interactive test
```
</details>

### Challenge 6 (Hard): AST Inspection of Evaluated Code
Write a function `inspect_repl_ast(statement_str)` that parses a single-line REPL statement using the `ast` module and returns the name of the root AST node type.
<details>
<summary><strong>Solution</strong></summary>

```python
import ast

def inspect_repl_ast(statement_str: str) -> str:
    tree = ast.parse(statement_str, mode="single")
    body = tree.body
    if not body:
        return "Empty"
    return type(body[0]).__name__

print("AST for 'x = 1':", inspect_repl_ast("x = 1"))
print("AST for '2 + 3':", inspect_repl_ast("2 + 3"))
```
</details>
