# Module 07: Exceptions, Context Managers & Debugging (05-context-managers-and-with)

> **Topic Overview**: The context management protocol (`with` statement, PEP 343) provides deterministic setup and teardown for runtime resources. Whether managing file descriptors, network sockets, database transactions, or temporary environment configurations, mastering `__enter__`, `__exit__`, `contextlib.contextmanager`, and `ExitStack` is foundational for writing leak-proof Python systems.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Resource Management |
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

- **Problem Statement**: Manual `try...finally` blocks for every resource acquisition lead to boilerplate code, unclosed handles during unexpected early returns, and subtle bugs when multiple resources depend on each other.
- **Mental Model**: A context manager is an encapsulation of the RAII (Resource Acquisition Is Initialization) pattern. Entering the context (`__enter__`) acquires or configures the resource; exiting the context (`__exit__`) guarantees cleanup and provides an opportunity to inspect, log, or suppress escaping exceptions.
- **When to Use**:
  - Managing external resources: files, database connections, locks, thread pools.
  - State boundaries: temporarily altering environment variables, changing decimal precision, mocking time.
  - Transactions: committing on success, rolling back on error.
- **When NOT to Use**: Do not use context managers for simple state tracking that does not require deterministic paired cleanup.

---

## 2. Core Concepts

### The Context Management Dunder Protocol
Any Python object implementing the context management protocol defines two methods:

```text
class ContextManager:
    def __enter__(self):
        # 1. Acquire resource
        # 2. Return target bound to 'as target'
        return resource

    def __exit__(self, exc_type, exc_val, exc_tb):
        # 1. Release resource
        # 2. Return True to SUPPRESS exception, or False/None to PROPAGATE
        return False
```

```text
[Enter: manager.__enter__() called]
        │
        ├── Return value bound to 'as target'
        v
[Execute with-block body]
        │
        ├── Exception raised?
        │       ├── NO  ──> manager.__exit__(None, None, None)
        │       └── YES ──> manager.__exit__(exc_type, exc_val, exc_tb)
        │                       │
        │                       ├── Returned truthy (True)? ──> Exception is SUPPRESSED
        │                       └── Returned falsy (None) ────> Exception PROPAGATES
        v
[Resume normal execution after with statement]
```

### Parenthesized Multiple Context Managers (Python 3.10+)
Since Python 3.10, the `with` statement supports multi-line parenthesized groups of context managers without trailing backslashes:

```python
with (
    open("source.txt", "r") as src,
    open("dest.txt", "w") as dst,
    acquire_lock() as lock
):
    dst.write(src.read())
```

---

## 3. How It Works Internally

CPython compiles the `with` statement into explicit setup, enter, and cleanup instructions, utilizing the `BEFORE_WITH` instruction introduced in Python 3.11:

```text
+---------------------------------------------------------------+
| Runtime Bytecode Execution for 'with C() as x:'               |
+---------------------------------------------------------------+
| 1. Evaluates C() -> pushes manager to stack                   |
| 2. BEFORE_WITH                                                |
|    - Calls __enter__()                                        |
|    - Pushes bound result (x) and __exit__ bound method        |
| 3. Enters guarded with-body                                   |
| 4. On normal exit:                                            |
|    - Pops target, loads None, None, None                      |
|    - Calls __exit__(None, None, None)                         |
| 5. On exception during body:                                  |
|    - Stack unwinder catches active exception                  |
|    - Calls __exit__(exc_type, exc_val, exc_tb)                |
|    - If result is True: pops exception and jumps forward      |
|    - If result is False: re-raises exception                  |
+---------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Protocol Signature
```python
def __enter__(self) -> Any:
    ...

def __exit__(
    self,
    exc_type: type[BaseException] | None,
    exc_val: BaseException | None,
    exc_tb: Any | None
) -> bool | None:
    ...
```

### Standard Library Helpers (`contextlib`)
- `@contextlib.contextmanager`: Decorator converting a generator function yielding once into a context manager.
- `contextlib.closing(thing)`: Wraps an object with a `.close()` method into a context manager.
- `contextlib.suppress(*exceptions)`: Context manager that silently swallows specified exception types.
- `contextlib.ExitStack`: Dynamic, programmatic context manager managing an arbitrary, runtime-determined number of resources.
- `contextlib.nullcontext(enter_result=None)`: No-op fallback context manager.

---

## 5. Code Examples

### 1. Minimal / Basic Example: Timing Context Manager
```python
import time

class Timer:
    def __enter__(self) -> "Timer":
        self.start = time.perf_counter()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.elapsed = time.perf_counter() - self.start
        print(f"Elapsed time: {self.elapsed * 1000:.3f} ms")

with Timer() as t:
    total = sum(i * i for i in range(100_000))

print("Completed block.")
```
**Output**:
```text
Elapsed time: 5.412 ms
Completed block.
```

### 2. Real-World Production Example: Transactional Database Session
```python
class MockDBSession:
    def __init__(self) -> None:
        self.in_transaction = False
        self.records: list[str] = []

    def __enter__(self) -> "MockDBSession":
        self.in_transaction = True
        self._staged = list(self.records)
        print("[DB] Transaction started")
        return self

    def add(self, record: str) -> None:
        if not self.in_transaction:
            raise RuntimeError("Cannot add record outside active transaction")
        self._staged.append(record)

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        self.in_transaction = False
        if exc_type is not None:
            # Exception occurred: rollback changes
            print(f"[DB] Rolled back transaction due to: {exc_val}")
            return False  # Propagate exception to caller
        
        # Success: commit staged records
        self.records = self._staged
        print(f"[DB] Committed {len(self.records)} records successfully")
        return True

session = MockDBSession()

# Case A: Successful commit
with session:
    session.add("user_1: Alice")
    session.add("user_2: Bob")
print(f"Session records: {session.records}")

# Case B: Rollback on error
try:
    with session:
        session.add("user_3: Charlie")
        raise ValueError("Simulated network failure during write")
except ValueError:
    pass
print(f"Session records after failed transaction: {session.records}")
```
**Output**:
```text
[DB] Transaction started
[DB] Committed 2 records successfully
Session records: ['user_1: Alice', 'user_2: Bob']
[DB] Transaction started
[DB] Rolled back transaction due to: Simulated network failure during write
Session records after failed transaction: ['user_1: Alice', 'user_2: Bob']
```

### 3. Edge Case: Suppressing Exceptions via `__exit__`
```python
class SuppressErrors:
    def __init__(self, *to_suppress: type[BaseException]) -> None:
        self.to_suppress = to_suppress

    def __enter__(self) -> None:
        pass

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        if exc_type is not None and issubclass(exc_type, self.to_suppress):
            print(f"[SUPPRESSED] Caught and silenced: {exc_type.__name__}")
            return True  # Truthy return halts exception propagation!
        return False

with SuppressErrors(ZeroDivisionError, FileNotFoundError):
    x = 10 / 0

print("Execution continued cleanly after suppressed error!")
```
**Output**:
```text
[SUPPRESSED] Caught and silenced: ZeroDivisionError
Execution continued cleanly after suppressed error!
```

### 4. Failure Case & Exception Handling: Generator Context Manager Missing Try/Finally
```python
from contextlib import contextmanager
from typing import Generator

@contextmanager
def flawed_resource() -> Generator[str, None, None]:
    print("[OPEN] Resource acquired")
    yield "ACTIVE_RESOURCE"
    # BUG: If the with-block raises, code after yield NEVER runs!
    print("[CLOSE] Resource released")

try:
    with flawed_resource() as res:
        print(f"Using {res}")
        raise RuntimeError("Something exploded inside with-block!")
except RuntimeError:
    print("[RECOVERED] Caught RuntimeError outside")
```
**Output**:
```text
[OPEN] Resource acquired
Using ACTIVE_RESOURCE
[RECOVERED] Caught RuntimeError outside
```
*Notice that `[CLOSE] Resource released` was skipped because there was no `try...finally` around the `yield`.*

### 5. Idiomatic Pythonic Pattern: Generator Context Manager with `ExitStack`
```python
from contextlib import contextmanager, ExitStack
import tempfile
import os

@contextmanager
def temp_workspace(prefix: str):
    dirpath = tempfile.mkdtemp(prefix=prefix)
    try:
        print(f"[CREATED] {dirpath}")
        yield dirpath
    finally:
        os.rmdir(dirpath)
        print(f"[REMOVED] {dirpath}")

# Programmatically manage an arbitrary number of workspaces
def setup_environments(count: int) -> None:
    with ExitStack() as stack:
        paths = [stack.enter_context(temp_workspace(f"env_{i}_")) for i in range(count)]
        print(f"All {len(paths)} environments initialized concurrently.")

setup_environments(3)
```
**Output**:
```text
[CREATED] /tmp/env_0_abc
[CREATED] /tmp/env_1_def
[CREATED] /tmp/env_2_ghi
All 3 environments initialized concurrently.
[REMOVED] /tmp/env_2_ghi
[REMOVED] /tmp/env_1_def
[REMOVED] /tmp/env_0_abc
```

---

## 6. Edge Cases & Pitfalls

1. **Returning True vs Returning Value**:
   - `__exit__` only checks truthiness. Returning any non-zero, non-None value (e.g., `1`, `"true"`) suppresses the exception. Only return `True` intentionally when suppression is specifically desired.
2. **Exceptions in `__enter__`**:
   - If `__enter__()` raises an exception, the with-block body is **not** executed, and `__exit__()` is **never called**. Any cleanup required by partial initialization in `__enter__` must be handled internally inside `__enter__`.
3. **`ExitStack` LIFO Order**:
   - `ExitStack` unwinds entered contexts in strict **Last-In, First-Out (LIFO)** reverse order, mirroring nested `with` statements.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Returning truthy value from `__exit__` by mistake | Silently swallows all exceptions from the guarded block | Masked production crashes | Return `False` or `None` by default |
| Omitting `try...finally` in `@contextmanager` | Code after `yield` is bypassed if an exception is raised in caller | Resource descriptor leak | Always wrap `yield` in `try...finally` |
| Raising exception inside `__enter__` without internal cleanup | `__exit__` is never invoked; partially initialized state leaks | Leaked connection or lock | Guard `__enter__` with internal try/finally |
| Multiple yields in `@contextmanager` | A context manager generator can only yield exactly once | `RuntimeError: generator didn't stop` | Ensure exactly one `yield` statement |
| Using `with lock:` inside async coroutines with threading Lock | Threading Lock blocks the entire event loop OS thread | Event loop stall | Use `async with asyncio.Lock():` |
| Calling `__enter__()` manually without `try...finally` | Defeats the purpose of the protocol; risks leaked state | Dangling resources | Always use the `with` statement |

---

## 8. Performance & Complexity

- **Overhead**: Entering and exiting a class-based context manager costs $\approx 30-50$ ns (two method dispatches).
- **Generator Context Manager (`@contextmanager`)**: Incurs $\approx 150-250$ ns due to generator frame allocation, `next()`, and `throw()` dispatch.
- **Python 3.11 Optimization**: The `BEFORE_WITH` instruction eliminated an unnecessary stack block, reducing bytecode dispatch overhead by ~20%.

---

## 9. CPython Internals & Bytecode

Let's inspect how Python compiles `with C() as x:`:

```python
import dis

class Resource:
    def __enter__(self): return 1
    def __exit__(self, *args): pass

def use_with():
    with Resource() as x:
        y = x + 1

dis.dis(use_with)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 LOAD_GLOBAL              1 (Resource)
             12 CALL                     0
             20 BEFORE_WITH
             22 STORE_FAST               0 (x)

  3          24 LOAD_FAST                0 (x)
             26 LOAD_CONST               1 (1)
             28 BINARY_OP                0 (+)
             32 STORE_FAST               1 (y)

  2     >>   34 LOAD_CONST               0 (None)
             36 LOAD_CONST               0 (None)
             38 LOAD_CONST               0 (None)
             40 CALL                     3
             50 POP_TOP
             52 RETURN_CONST             0 (None)

ExceptionTable:
  24 to 34 -> 54 [1]
```
Notice `BEFORE_WITH` at offset 20:
- It calls `__enter__` and stores the result in `x`.
- On normal exit (offset 34), it pushes `None, None, None` and executes `CALL 3` calling `__exit__`.
- The `ExceptionTable` routes exceptions from offset `24 to 34` to handler offset `54`.

---

## 10. Best Practices

1. **Always Use `try...finally` in Generator Context Managers**: Wrap the `yield` statement in a `try...finally` block to guarantee teardown.
2. **Keep `__exit__` Safe**: `__exit__` should never raise an exception unless it is re-raising an internal cleanup failure.
3. **Use `contextlib.ExitStack` for Dynamic Resources**: When opening a variable number of files or acquiring multiple locks, use `ExitStack` instead of nested loops.

---

## 11. Real-World Case Study: Temporary Working Directory Switcher

```python
import os
from contextlib import contextmanager
from typing import Generator

@contextmanager
def temporary_working_dir(new_path: str) -> Generator[str, None, None]:
    original_cwd = os.getcwd()
    os.chdir(new_path)
    try:
        yield os.getcwd()
    finally:
        os.chdir(original_cwd)

# Verify behavior
initial_dir = os.getcwd()
target_dir = os.path.dirname(initial_dir) or initial_dir

with temporary_working_dir(target_dir) as active_dir:
    print(f"Inside with-block: {active_dir}")
    assert os.getcwd() == target_dir

print(f"Restored directory: {os.getcwd()}")
assert os.getcwd() == initial_dir
```
**Output**:
```text
Inside with-block: /path/to/parent
Restored directory: /path/to/current
```

---

## 12. Interview Questions

### Q1: What does `__exit__` receive as arguments when an exception occurs inside the `with` block?
<details>
<summary><strong>Answer</strong></summary>
It receives three arguments: <code>exc_type</code> (the exception class, e.g., <code>ValueError</code>), <code>exc_val</code> (the exception instance), and <code>exc_tb</code> (the traceback object). If no exception occurred, all three arguments are <code>None</code>.
</details>

### Q2: How does a context manager signal that an exception should be suppressed?
<details>
<summary><strong>Answer</strong></summary>
By returning a truthy value (conventionally <code>True</code>) from its <code>__exit__</code> method. If <code>__exit__</code> returns <code>False</code> or <code>None</code>, CPython automatically re-raises the active exception.
</details>

### Q3: What happens if an exception is raised inside `__enter__`?
<details>
<summary><strong>Answer</strong></summary>
The body of the <code>with</code> block is not executed, and the context manager's <code>__exit__</code> method is <strong>never called</strong>. The exception immediately propagates to the enclosing scope.
</details>

### Q4: Why is `try...finally` mandatory when using `@contextlib.contextmanager`?
<details>
<summary><strong>Answer</strong></summary>
If an exception occurs inside the <code>with</code> block, <code>contextlib</code> re-raises that exception inside the generator at the point of the <code>yield</code> statement (using <code>generator.throw()</code>). Without a <code>try...finally</code> wrapping the <code>yield</code>, execution terminates immediately and cleanup code after the <code>yield</code> will not execute.
</details>

### Q5: What is `contextlib.ExitStack` and when should you use it?
<details>
<summary><strong>Answer</strong></summary>
<code>ExitStack</code> is a programmatic context manager designed for managing a dynamic or variable number of other context managers. It is used when the number of resources (such as an unknown number of files opened from a list) cannot be hardcoded into nested <code>with</code> statements at authoring time.
</details>

### Q6: Can a context manager be re-used multiple times across separate `with` blocks?
<details>
<summary><strong>Answer</strong></summary>
It depends on the implementation. Stateless context managers (like <code>contextlib.suppress</code> or timer utilities) are reusable. However, stateful context managers (such as file objects or single-use generators from <code>@contextmanager</code>) cannot be reused and will raise an error if entered a second time.
</details>

### Q7: What is the execution order when multiple context managers are combined in a single `with` statement?
<details>
<summary><strong>Answer</strong></summary>
They are entered from left to right (or top to bottom), and exited in reverse order (right to left / bottom to top). For <code>with A() as a, B() as b:</code>, the sequence is: <code>A.__enter__()</code>, <code>B.__enter__()</code>, body, <code>B.__exit__()</code>, <code>A.__exit__()</code>.
</details>

### Q8: What happens if `__exit__` itself raises an exception?
<details>
<summary><strong>Answer</strong></summary>
The new exception from <code>__exit__</code> propagates out and replaces any exception that was active inside the <code>with</code> block. The original exception is attached to the new exception's <code>__context__</code> attribute.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Silencer Context Manager
Implement a class `Silencer` that suppresses `KeyError` and `IndexError`, but lets all other exceptions propagate normally.
<details>
<summary><strong>Solution</strong></summary>

```python
class Silencer:
    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if exc_type is not None and issubclass(exc_type, (KeyError, IndexError)):
            return True
        return False

with Silencer():
    [][0]  # IndexError suppressed

with Silencer():
    {}[1]  # KeyError suppressed
```
</details>

### Challenge 2 (Easy): Redirect Standard Output
Use `contextlib.contextmanager` to write a context manager `captured_stdout()` that redirects `sys.stdout` to an in-memory `io.StringIO` and yields it, restoring `sys.stdout` on exit.
<details>
<summary><strong>Solution</strong></summary>

```python
import sys
import io
from contextlib import contextmanager

@contextmanager
def captured_stdout():
    old_stdout = sys.stdout
    buffer = io.StringIO()
    sys.stdout = buffer
    try:
        yield buffer
    finally:
        sys.stdout = old_stdout

with captured_stdout() as buf:
    print("Testing output interception")

assert "Testing output interception" in buf.getvalue()
```
</details>

### Challenge 3 (Medium): Mutex Lock Guard with Acquisition Timeout
Create a class `TimeoutLock` that wraps a `threading.Lock`. In `__enter__`, attempt to acquire the lock with a given timeout. If timeout expires, raise `TimeoutError`. Ensure `__exit__` only releases the lock if it was successfully acquired.
<details>
<summary><strong>Solution</strong></summary>

```python
import threading

class TimeoutLock:
    def __init__(self, lock: threading.Lock, timeout: float = 1.0) -> None:
        self.lock = lock
        self.timeout = timeout
        self.acquired = False

    def __enter__(self) -> "TimeoutLock":
        self.acquired = self.lock.acquire(timeout=self.timeout)
        if not self.acquired:
            raise TimeoutError("Failed to acquire lock within timeout")
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        if self.acquired:
            self.lock.release()
            self.acquired = False

l = threading.Lock()
with TimeoutLock(l, timeout=0.1):
    assert l.locked()
assert not l.locked()
```
</details>

### Challenge 4 (Medium): Temporary Attribute Overrider
Write a generator context manager `override_attribute(obj, name, temp_value)` that sets `getattr(obj, name)` to `temp_value`, yields, and unconditionally restores the original value (or deletes it if it didn't previously exist) in `finally`.
<details>
<summary><strong>Solution</strong></summary>

```python
from contextlib import contextmanager

@contextmanager
def override_attribute(obj, name: str, temp_value):
    had_attr = hasattr(obj, name)
    old_val = getattr(obj, name, None)
    setattr(obj, name, temp_value)
    try:
        yield
    finally:
        if had_attr:
            setattr(obj, name, old_val)
        else:
            delattr(obj, name)

class Config:
    timeout = 30

cfg = Config()
with override_attribute(cfg, "timeout", 90):
    assert cfg.timeout == 90
assert cfg.timeout == 30
```
</details>

### Challenge 5 (Hard): Re-Entrant Context Manager
Implement a class `ReentrantCounter` that can be entered multiple times (nested `with` statements). Track the current nesting depth, execute setup on first entry, and only execute final teardown when the outermost `with` block exits.
<details>
<summary><strong>Solution</strong></summary>

```python
class ReentrantCounter:
    def __init__(self) -> None:
        self.depth = 0
        self.active = False

    def __enter__(self) -> "ReentrantCounter":
        if self.depth == 0:
            self.active = True
        self.depth += 1
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.depth -= 1
        if self.depth == 0:
            self.active = False

rc = ReentrantCounter()
assert not rc.active

with rc:
    assert rc.depth == 1
    assert rc.active
    with rc:
        assert rc.depth == 2
        assert rc.active
    assert rc.depth == 1
    assert rc.active

assert rc.depth == 0
assert not rc.active
```
</details>

### Challenge 6 (Hard): Dynamic Batch File Opener with Partial Rollback
Implement a function `open_many(filepaths: list[str], mode: str = "w")` using `contextlib.ExitStack` that opens all files in the list. If any file fails to open, ensure all previously opened files are closed immediately, and all partially written files are removed from disk.
<details>
<summary><strong>Solution</strong></summary>

```python
from contextlib import ExitStack, contextmanager
import os

@contextmanager
def open_many(filepaths: list[str], mode: str = "w"):
    stack = ExitStack()
    opened_files = []
    created_paths = []
    try:
        for p in filepaths:
            f = stack.enter_context(open(p, mode))
            opened_files.append(f)
            created_paths.append(p)
        yield opened_files
    except Exception:
        stack.close()  # Close all handles
        for p in created_paths:
            if os.path.exists(p):
                os.remove(p)
        raise
    else:
        stack.close()

# Verification
paths = ["temp_test_1.txt", "temp_test_2.txt"]
try:
    with open_many(paths) as files:
        assert len(files) == 2
        files[0].write("data")
finally:
    for p in paths:
        if os.path.exists(p):
            os.remove(p)
```
</details>
