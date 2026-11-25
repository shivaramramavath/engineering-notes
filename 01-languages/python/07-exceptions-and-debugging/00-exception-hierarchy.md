# Module 07: Exceptions, Context Managers & Debugging (00-exception-hierarchy)

> **Topic Overview**: Python's exception model is founded upon a strict, single-inheritance class hierarchy rooted at `BaseException`. Understanding this tree—specifically why system-exiting signals diverge from standard application exceptions—is vital for building robust error handling, preventing hung processes, and structuring resilient architectures.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Error Handling |
| **Target Level** | Intermediate to Advanced |
| **Python Version** | Python 3.10+ (tested on Python 3.12 - 3.14+) |
| **Tested With** | CPython 3.14.0, uv 0.13.9, ruff 0.17.6, mypy 2.4.1, pytest 9.1.1 |
| **Free-Threading / Platform** | Supported / Platform Independent |

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

- **Problem Statement**: Without a structured exception hierarchy, distinguishing between programmatic bugs (e.g., `IndexError`), external I/O failures (e.g., `FileNotFoundError`), and operational lifecycle signals (e.g., `KeyboardInterrupt`, `SystemExit`) is impossible. Catching an unpartitioned root error risks intercepting Ctrl+C or preventing process termination.
- **Mental Model**: Consider the exception tree as an organizational taxonomy. `BaseException` represents any event that interrupts normal control flow. `Exception` is the subset intended for application and library recovery. Siblings of `Exception` (such as `SystemExit`, `KeyboardInterrupt`, and `GeneratorExit`) bypass general exception handlers to ensure runtime control integrity.
- **When to Use**: Catch specific descendants of `Exception` (or `Exception` itself at top-level service boundaries). Inherit from `Exception` when creating domain-specific application errors.
- **When NOT to Use**: Never catch `BaseException` in business logic or worker loops unless re-raising immediately after cleanup. Never inherit custom application errors directly from `BaseException`.

---

## 2. Core Concepts

### The CPython Exception Tree Topology
All built-in exceptions form an acyclic single-inheritance tree. The primary branches are:

```text
BaseException
 ├── BaseExceptionGroup (Python 3.11+)
 ├── GeneratorExit
 ├── KeyboardInterrupt
 ├── SystemExit
 └── Exception
      ├── ArithmeticError
      │    ├── FloatingPointError
      │    ├── OverflowError
      │    └── ZeroDivisionError
      ├── AssertionError
      ├── AttributeError
      ├── BufferError
      ├── EOFError
      ├── ExceptionGroup (Python 3.11+)
      ├── ImportError
      │    └── ModuleNotFoundError
      ├── LookupError
      │    ├── IndexError
      │    └── KeyError
      ├── MemoryError
      ├── NameError
      │    └── UnboundLocalError
      ├── OSError
      │    ├── BlockingIOError
      │    ├── ChildProcessError
      │    ├── ConnectionError
      │    │    ├── BrokenPipeError
      │    │    ├── ConnectionAbortedError
      │    │    ├── ConnectionRefusedError
      │    │    └── ConnectionResetError
      │    ├── FileExistsError
      │    ├── FileNotFoundError
      │    ├── InterruptedError
      │    ├── IsADirectoryError
      │    ├── NotADirectoryError
      │    ├── PermissionError
      │    ├── ProcessLookupError
      │    └── TimeoutError
      ├── ReferenceError
      ├── RuntimeError
      │    ├── NotImplementedError
      │    └── RecursionError
      ├── StopAsyncIteration
      ├── StopIteration
      ├── SyntaxError
      │    └── IndentationError
      │         └── TabError
      ├── SystemError
      ├── TypeError
      ├── ValueError
      │    └── UnicodeError
      │         ├── UnicodeDecodeError
      │         ├── UnicodeEncodeError
      │         └── UnicodeTranslateError
      └── Warning
```

### Type Invariants & Groupings
1. **LookupError Grouping**: Both `KeyError` and `IndexError` inherit from `LookupError`. Catching `LookupError` provides clean polymorphic handling over mappings and sequences.
2. **OSError Consolidation**: Since Python 3.3 (PEP 3151), OS-level `errno` values map to distinct `OSError` subclasses (like `FileNotFoundError`, `PermissionError`, and `ConnectionResetError`), while preserving compatibility under the unified `OSError` umbrella.

---

## 3. How It Works Internally

When an exception is raised, CPython unwinds the call stack searching for a matching exception handler. In Python 3.11+, stack unwinding uses zero-cost exception handling tables rather than runtime block stacks.

```text
+--------------------------------------------------------------------------+
|                        CPython Stack Unwinding                           |
+--------------------------------------------------------------------------+
| Frame 2 (Worker):   raise KeyError("item missing")                       |
|                     -> Looks up instruction offset in CodeObject Table  |
|                     -> No local handler found                            |
|                     -> Unwinds Frame 2                                   |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
| Frame 1 (Caller):   try: worker() except LookupError: ...                |
|                     -> Checks: issubclass(KeyError, LookupError)? -> TRUE|
|                     -> Jumps directly to Handler Target Offset in Table  |
|                     -> Instantiates traceback frame & binds exception    |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Introspecting the Hierarchy
```python
import builtins

# Test subtyping relationships
issubclass(KeyError, LookupError)       # True
issubclass(LookupError, Exception)       # True
issubclass(Exception, BaseException)    # True
issubclass(KeyboardInterrupt, Exception) # False!
```

### Standard BaseException Attributes
- `args`: Tuple of arguments passed to the constructor (conventionally `(message,)`).
- `__traceback__`: Traceback object associated with the point of failure.
- `__context__`: Implicitly chained exception during handling.
- `__cause__`: Explicitly chained exception (`raise ... from cause`).
- `__notes__`: List of strings added via `add_note(message)` (Python 3.11+).

---

## 5. Code Examples

### 1. Minimal / Basic Example: Catching Polymorphic Errors
```python
def fetch_item(collection: list[int] | dict[str, int], key: int | str) -> int | None:
    try:
        return collection[key]  # type: ignore[index]
    except LookupError as err:
        print(f"Caught polymorphic lookup failure: {type(err).__name__} -> {err}")
        return None

items = [10, 20, 30]
mapping = {"alpha": 1, "beta": 2}

r1 = fetch_item(items, 99)
r2 = fetch_item(mapping, "gamma")
print(f"Results: r1={r1}, r2={r2}")
```
**Output**:
```text
Caught polymorphic lookup failure: IndexError -> list index out of range
Caught polymorphic lookup failure: KeyError -> 'gamma'
Results: r1=None, r2=None
```

### 2. Real-World Production Example: Resilient Worker Loop
```python
import time
import sys

def process_job(job_id: int) -> None:
    if job_id == 3:
        raise ValueError("Corrupted payload")
    if job_id == 5:
        # Simulate SIGINT (Ctrl+C)
        raise KeyboardInterrupt()
    print(f"Job {job_id} processed successfully.")

def run_worker() -> None:
    job_queue = [1, 2, 3, 4, 5, 6]
    for jid in job_queue:
        try:
            process_job(jid)
        except Exception as exc:
            # Handles application failures without aborting the queue
            print(f"[RECOVERED] Job {jid} failed: {exc}", file=sys.stderr)
        except BaseException as fatal:
            # Intercepts lifecycle interrupts: must log and re-raise
            print(f"[SHUTDOWN] Interrupted by {type(fatal).__name__}. Graceful exit...", file=sys.stderr)
            raise

try:
    run_worker()
except KeyboardInterrupt:
    print("Worker stopped cleanly by operator.")
```
**Output**:
```text
Job 1 processed successfully.
Job 2 processed successfully.
[RECOVERED] Job 3 failed: Corrupted payload
Job 4 processed successfully.
[SHUTDOWN] Interrupted by KeyboardInterrupt. Graceful exit...
Worker stopped cleanly by operator.
```

### 3. Edge Case: Custom Base Exception vs Standard Exception
```python
class SystemServiceError(BaseException):
    """Dangerous: Bypasses standard application handlers!"""

class ServiceError(Exception):
    """Correct: Inherits from Exception."""

def faulty_call() -> None:
    raise ServiceError("Service timeout")

try:
    faulty_call()
except Exception as e:
    print(f"Caught expected application error: {e}")
```
**Output**:
```text
Caught expected application error: Service timeout
```

### 4. Failure Case & Exception Handling: Catching BaseException Swallowing Exit
```python
def unkillable_task() -> None:
    try:
        # User pressed Ctrl+C or sys.exit() invoked
        raise SystemExit(1)
    except BaseException:
        # Anti-pattern: Swallowing BaseException suppresses program termination!
        print("Swallowed SystemExit! The program erroneously continues...")

unkillable_task()
print("Execution resumed improperly!")
```
**Output**:
```text
Swallowed SystemExit! The program erroneously continues...
Execution resumed improperly!
```

### 5. Idiomatic Pythonic Pattern: Enriching Exceptions with PEP 678 Notes
```python
def parse_record(data: dict[str, str]) -> int:
    try:
        return int(data["user_id"])
    except KeyError as err:
        err.add_note("Context: Missing mandatory primary key in authorization payload")
        raise
    except ValueError as err:
        err.add_note(f"Context: Raw value '{data.get('user_id')}' cannot be converted to integer")
        raise

try:
    parse_record({"user_id": "ABC_INVALID"})
except ValueError as exc:
    print(f"Exception: {exc}")
    print(f"Notes attached: {exc.__notes__}")
```
**Output**:
```text
Exception: invalid literal for int() with base 10: 'ABC_INVALID'
Notes attached: ["Context: Raw value 'ABC_INVALID' cannot be converted to integer"]
```

---

## 6. Edge Cases & Pitfalls

1. **Bare `except:` vs `except Exception:`**:
   - `except:` without an argument catches `BaseException`, masking `SystemExit` and `KeyboardInterrupt`. Always write `except Exception:`.
2. **`GeneratorExit` Handling**:
   - `GeneratorExit` is raised inside a generator when `close()` is called. If the generator catches it and yields another value, CPython immediately raises a `RuntimeError: generator ignored GeneratorExit`.
3. **`ArithmeticError` Precision**:
   - Integers in Python 3 have arbitrary precision and do not raise `OverflowError`. However, floating-point operations exceeding IEEE 754 limits or math functions (e.g., `math.exp(1000)`) raise `OverflowError`.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| `except BaseException:` without re-raise | Prevents user from stopping script with Ctrl+C; blocks orchestrator SIGTERM | Silent hung processes | `except Exception:` for general errors |
| Subclassing `BaseException` for custom app error | Standard error handlers will not catch it; bubbles up to crash root | Unhandled crash | Subclass `Exception` or domain base |
| Out-of-order `except` blocks | Python checks top-to-bottom. Catching base first renders derived handler unreachable | Dead code (Ruff B018 / mypy warn) | Place specific exception before broad exception |
| Raising string as exception | Python 3 requires exceptions to be instances or subclasses of `BaseException` | `TypeError: exceptions must derive from BaseException` | `raise ValueError("message")` |
| Catching `TypeError` when expecting `LookupError` | Accessing `None[0]` raises `TypeError`, not `IndexError` | Uncaught `TypeError` | Check for `None` or catch `(LookupError, TypeError)` |
| Overwriting `__traceback__` incorrectly | Direct assignment of non-traceback objects corrupts frame stack | `TypeError: expected traceback object, got ...` | Use `e.with_traceback(tb)` |

---

## 8. Performance & Complexity

- **Time Complexity of Lookup**: Finding matching `except` clauses is $O(k)$ where $k$ is the number of except clauses, executing `PyType_IsSubtype` in $O(1)$ amortized tree depth.
- **Zero-Cost Exceptions (Python 3.11+)**: Entering a `try` block has zero runtime CPU cost when no exception occurs. The interpreter consults a static jump table stored in the `PyCodeObject` only when an exception is raised.
- **Memory Overhead**: `PyBaseExceptionObject` allocates 64–96 bytes plus `args` tuple and traceback dictionary.

---

## 9. CPython Internals & Bytecode

Disassembling a standard `try...except` block in Python 3.12+ reveals compiler exception tables:

```python
import dis

def example_handler():
    try:
        x = 1 / 0
    except ZeroDivisionError:
        x = 0
    return x

dis.dis(example_handler)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 NOP

  3           4 LOAD_CONST               1 (1)
              6 LOAD_CONST               2 (0)
              8 BINARY_OP               11 (/)
             12 STORE_FAST               0 (x)
             14 JUMP_FORWARD             8 (to 24)

  4     >>   16 PUSH_EXC_INFO
             18 LOAD_GLOBAL              1 (ZeroDivisionError)
             28 CHECK_EXC_MATCH
             30 POP_JUMP_IF_FALSE       14 (to 46)

  5          32 LOAD_CONST               2 (0)
             34 STORE_FAST               0 (x)
             36 POP_EXCEPT
             38 JUMP_FORWARD             1 (to 42)
        >>   40 POP_EXCEPT
             42 JUMP_FORWARD             2 (to 48)

  6     >>   44 POP_EXCEPT
             46 RERAISE                  0

  7     >>   48 LOAD_FAST                0 (x)
             50 RETURN_VALUE

ExceptionTable:
  4 to 12 -> 16 [0]
```
The `ExceptionTable` maps byte ranges `4 to 12` to target handler offset `16`. `CHECK_EXC_MATCH` verifies that the active exception matches the specified type.

---

## 10. Best Practices

1. **Be Specific**: Always catch the narrowest exception possible (e.g., `FileNotFoundError` over `OSError`, `KeyError` over `Exception`).
2. **Protect Main Loops**: Top-level event loops (e.g., Celery workers, HTTP request routers) should catch `Exception`, never `BaseException`.
3. **Use Notes for Metadata**: Attach telemetry and troubleshooting context using `exc.add_note()` instead of mangling the original error message string.

---

## 11. Real-World Case Study: Resilient Database Connection Pool

```python
import time
from typing import Any

class DatabasePool:
    def __init__(self) -> None:
        self.connected = False

    def acquire(self) -> str:
        if not self.connected:
            raise ConnectionRefusedError("Pool exhausted or DB offline")
        return "Connection_1"

def execute_query_with_retry(pool: DatabasePool, max_retries: int = 3) -> str:
    delay = 0.05
    for attempt in range(1, max_retries + 1):
        try:
            conn = pool.acquire()
            return f"Queried using {conn}"
        except OSError as err:
            # Polymorphic handling covers ConnectionRefusedError, BrokenPipeError, TimeoutError
            print(f"[Attempt {attempt}/{max_retries}] OS Network failure: {err}")
            if attempt == max_retries:
                raise
            time.sleep(delay)
            delay *= 2
            # Simulate recovery on attempt 3
            if attempt == 2:
                pool.connected = True
    return "Failed"

db = DatabasePool()
result = execute_query_with_retry(db, max_retries=3)
print(f"Final Query Result: {result}")
```
**Output**:
```text
[Attempt 1/3] OS Network failure: Pool exhausted or DB offline
[Attempt 2/3] OS Network failure: Pool exhausted or DB offline
Final Query Result: Queried using Connection_1
```

---

## 12. Interview Questions

### Q1: Why does `BaseException` exist separately from `Exception`?
<details>
<summary><strong>Answer</strong></summary>
Python 2.5 introduced <code>BaseException</code> to separate exceptions that signal the system or interpreter to terminate (such as <code>SystemExit</code>, <code>KeyboardInterrupt</code>, and <code>GeneratorExit</code>) from application-level exceptions. Prior to this, developers writing <code>except Exception:</code> would inadvertently swallow Ctrl+C or process termination requests.
</details>

### Q2: What happens if an `except` block catches `LookupError` before `KeyError`?
<details>
<summary><strong>Answer</strong></summary>
Because <code>KeyError</code> inherits from <code>LookupError</code>, the <code>LookupError</code> block will match first, and the subsequent <code>except KeyError:</code> block will never be executed. Python evaluates except clauses sequentially from top to bottom.
</details>

### Q3: When should a custom exception subclass `BaseException`?
<details>
<summary><strong>Answer</strong></summary>
Almost never. Subclassing <code>BaseException</code> should only be done for control flow signals that must bypass all normal application logic (such as specialized coroutine cancellation tokens or custom process abort signals). All standard library and third-party domain exceptions must derive from <code>Exception</code>.
</details>

### Q4: How does Python 3.11's zero-cost exception handling improve performance?
<details>
<summary><strong>Answer</strong></summary>
Prior to 3.11, entering a <code>try</code> block executed <code>SETUP_FINALLY</code> bytecode, pushing an exception frame onto the call stack. Python 3.11 replaced this with static tables stored in code objects. When no exception is raised, execution flow has zero additional CPU overhead. Overhead only occurs during stack unwinding when an exception is actually thrown.
</details>

### Q5: What is the difference between `IndexError` and `KeyError` in terms of base classes?
<details>
<summary><strong>Answer</strong></summary>
Both derive directly from <code>LookupError</code>, which itself derives from <code>Exception</code>. This enables client code to catch <code>LookupError</code> when operating on arbitrary mapping or sequence containers.
</details>

### Q6: Can a function catch a `KeyboardInterrupt` inside a multi-threaded Python program?
<details>
<summary><strong>Answer</strong></summary>
In CPython, <code>KeyboardInterrupt</code> (generated by SIGINT) is only delivered to the <strong>main thread</strong>. Worker threads do not receive <code>KeyboardInterrupt</code> directly; they must be monitored or stopped via threading events or shared flags initiated by the main thread handler.
</details>

### Q7: What is the output of `issubclass(MemoryError, Exception)` vs `issubclass(SystemExit, Exception)`?
<details>
<summary><strong>Answer</strong></summary>
<code>issubclass(MemoryError, Exception)</code> returns <code>True</code> (it is an operational memory failure). <code>issubclass(SystemExit, Exception)</code> returns <code>False</code> (it inherits directly from <code>BaseException</code>).
</details>

### Q8: How does `Exception.add_note()` work and how does it affect exception serialization?
<details>
<summary><strong>Answer</strong></summary>
Added in PEP 678 (Python 3.11), <code>add_note(str)</code> appends a string to the <code>__notes__</code> attribute of the exception instance. When CPython prints the standard traceback, all attached notes are displayed sequentially under the error message. It does not alter the <code>args</code> tuple.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Catching Lookup Errors
Write a function `safe_access(data, index_or_key)` that takes any sequence or mapping and returns the value, or `"MISSING"` if either `IndexError` or `KeyError` occurs, using a single `except` statement.
<details>
<summary><strong>Solution</strong></summary>

```python
def safe_access(data, index_or_key):
    try:
        return data[index_or_key]
    except LookupError:
        return "MISSING"

assert safe_access([1, 2, 3], 10) == "MISSING"
assert safe_access({"a": 1}, "b") == "MISSING"
assert safe_access({"a": 1}, "a") == 1
```
</details>

### Challenge 2 (Easy): Audit Exception Lineage
Write a function `get_lineage(cls)` that takes an exception class and returns a list of its ancestor class names up to `BaseException`.
<details>
<summary><strong>Solution</strong></summary>

```python
def get_lineage(cls: type[BaseException]) -> list[str]:
    return [c.__name__ for c in cls.__mro__ if issubclass(c, BaseException)]

assert get_lineage(KeyError) == ["KeyError", "LookupError", "Exception", "BaseException"]
```
</details>

### Challenge 3 (Medium): Safe Threaded Loop Guard
Write a decorator `@suppress_app_exceptions` that catches all application-level errors, prints a warning, and returns a default value, but immediately lets system signals (`SystemExit`, `KeyboardInterrupt`) bubble up uninhibited.
<details>
<summary><strong>Solution</strong></summary>

```python
import functools
from typing import Callable, Any

def suppress_app_exceptions(default_value: Any = None):
    def decorator(fn: Callable):
        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            try:
                return fn(*args, **kwargs)
            except Exception as err:
                print(f"Warning: {fn.__name__} raised application error: {err}")
                return default_value
        return wrapper
    return decorator

@suppress_app_exceptions("default")
def buggy():
    return 1 / 0

assert buggy() == "default"
```
</details>

### Challenge 4 (Medium): Hierarchical Error Reporter
Write a function `categorize_exception(exc: BaseException) -> str` that inspects an exception instance and classifies it into: `"SYSTEM_SIGNAL"`, `"IO_ERROR"`, `"DATA_ERROR"` (for `ValueError`, `TypeError`, `LookupError`), or `"UNKNOWN"`.
<details>
<summary><strong>Solution</strong></summary>

```python
def categorize_exception(exc: BaseException) -> str:
    if not isinstance(exc, Exception):
        return "SYSTEM_SIGNAL"
    if isinstance(exc, OSError):
        return "IO_ERROR"
    if isinstance(exc, (ValueError, TypeError, LookupError)):
        return "DATA_ERROR"
    return "UNKNOWN"

assert categorize_exception(KeyboardInterrupt()) == "SYSTEM_SIGNAL"
assert categorize_exception(FileNotFoundError()) == "IO_ERROR"
assert categorize_exception(KeyError("k")) == "DATA_ERROR"
assert categorize_exception(RuntimeError()) == "UNKNOWN"
```
</details>

### Challenge 5 (Hard): Dynamic Tree Visualizer
Implement a function `render_exception_tree(root_cls: type[BaseException] = BaseException, depth: int = 0) -> list[str]` that dynamically inspects `__subclasses__()` and returns an indented list representation of the active runtime exception tree.
<details>
<summary><strong>Solution</strong></summary>

```python
def render_exception_tree(root_cls: type[BaseException] = BaseException, depth: int = 0) -> list[str]:
    lines = ["  " * depth + root_cls.__name__]
    for sub in sorted(root_cls.__subclasses__(), key=lambda c: c.__name__):
        lines.extend(render_exception_tree(sub, depth + 1))
    return lines

tree = render_exception_tree(LookupError)
assert "LookupError" in tree[0]
assert any("IndexError" in line for line in tree)
assert any("KeyError" in line for line in tree)
```
</details>

### Challenge 6 (Hard): Context Enriched Exception Wrapper
Implement a context manager `enrich_errors(**metadata)` that intercepts any `Exception` escaping the managed block and attaches all metadata key-value pairs as formatted notes via `.add_note()`, while preserving the exact exception type and traceback.
<details>
<summary><strong>Solution</strong></summary>

```python
from contextlib import contextmanager

@contextmanager
def enrich_errors(**metadata):
    try:
        yield
    except Exception as exc:
        for k, v in metadata.items():
            exc.add_note(f"{k}: {v}")
        raise

try:
    with enrich_errors(service="auth", request_id="req-9912", user_id=402):
        raise ValueError("Invalid credentials token")
except ValueError as e:
    assert len(e.__notes__) == 3
    assert "service: auth" in e.__notes__[0]
```
</details>
