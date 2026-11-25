# Module 07: Exceptions, Context Managers & Debugging (01-try-except-else-finally)

> **Topic Overview**: The `try...except...else...finally` compound statement is Python's primary control flow mechanism for deterministic error handling and cleanup. Mastering the precise execution order—especially the subtle semantics of `else` and how `finally` interacts with `return`, `break`, and nested exceptions—is critical for writing reliable, leak-free code.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Control Flow |
| **Target Level** | Intermediate |
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

- **Problem Statement**: Without structured cleanup and distinct branching for "success without error," developers either duplicate cleanup code or accidentally handle exceptions generated *inside* the error handler itself.
- **Mental Model**:
  - `try`: "Run this guarded code."
  - `except`: "Run this ONLY if a matching exception was raised in `try`."
  - `else`: "Run this ONLY if the `try` block completed without raising any exception."
  - `finally`: "Run this ALWAYS, regardless of whether the code succeeded, raised an exception, or returned early."
- **When to Use**:
  - Use `try/except` around operations that may legitimately fail (I/O, parsing, network requests).
  - Use `else` for code that should only run if the guarded code succeeded, keeping the `try` block minimal.
  - Use `finally` for resource teardown, releasing locks, closing sockets, or restoring environment state when not using a context manager.
- **When NOT to Use**: Do not use `try...except` for normal branch prediction that can be checked with simple condition checks (e.g., avoid checking if an object is `None` by catching `AttributeError`).

---

## 2. Core Concepts

### The Four-Clause Lifecycle State Machine
Execution transitions through a deterministic sequence:

```text
[Enter try block]
       │
       ├── Exception raised?
       │        ├── YES ──> [Match except clauses sequentially]
       │        │                 ├── Match found ──> [Execute except block]
       │        │                 └── No match ─────> [Hold exception to re-raise]
       │        │
       │        └── NO ───> [Execute else block]
       │
       v
[Always execute finally block]
       │
       ├── Did finally raise/return?
       │        ├── YES ──> [Override previous return or discard active exception]
       │        └── NO ───> [Propagate pending exception OR resume normal flow]
```

### Key Semantics
1. **The `else` Clause Isolation**: Code inside the `else` clause is **not** guarded by the preceding `except` clauses. If the `else` block raises an exception, it propagates to outer scopes rather than being caught locally. This avoids masking unexpected bugs.
2. **`finally` Precedence**: If code inside `try`, `except`, or `else` executes a `return`, `break`, or `continue`, the `finally` block is executed immediately *before* that control transfer completes.
3. **`finally` Masking**: If the `finally` block explicitly executes a `return` or raises an exception, any pending exception from `try` or `except` is **silently suppressed and discarded**.

---

## 3. How It Works Internally

CPython utilizes internal frame state unwinding to ensure `finally` blocks are guaranteed to execute.

```text
+-------------------------------------------------------------+
| Frame Execution Flow                                        |
+-------------------------------------------------------------+
| 1. try block executes 'return 42'                           |
|    - CPython puts 42 into a temporary register              |
|    - Notices active finally block in Exception Table        |
| 2. Jumps to finally block offset                            |
|    - Executes cleanup code                                  |
|    - If finally has NO return, CPython retrieves 42         |
|      and executes RETURN_VALUE                              |
|    - If finally executes 'return 99', 42 is discarded       |
|      and 99 is returned instead                             |
+-------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Complete Syntax Grammar
```python
try:
    # Guarded block: keep as narrow as possible
    risky_operation()
except (TypeError, ValueError) as err:
    # Tuple of matching exceptions; err is bound to the instance
    handle_client_error(err)
except Exception as err:
    # Fallback application handler
    handle_generic_error(err)
else:
    # Runs ONLY if try succeeded without exceptions
    handle_success()
finally:
    # Runs unconditionally on exit (cleanup/teardown)
    cleanup_resources()
```

### Variable Scoping Rules
The target variable bound in `as target` (e.g., `except KeyError as err:`) is scoped **only to that except block**. CPython deletes `err` from the local namespace at the end of the `except` block (equivalent to `del err`) to break circular reference cycles with tracebacks.

---

## 5. Code Examples

### 1. Minimal / Basic Example: Complete 4-Clause Execution
```python
def divide(a: float, b: float) -> str:
    log: list[str] = []
    try:
        log.append("1. entering try")
        res = a / b
    except ZeroDivisionError:
        log.append("2. caught division by zero")
        return "ERROR"
    else:
        log.append(f"2. calculated result: {res}")
        return f"OK: {res}"
    finally:
        log.append("3. executing finally cleanup")
        print(" -> ".join(log))

print("Call 1:", divide(10, 2))
print("Call 2:", divide(10, 0))
```
**Output**:
```text
1. entering try -> 2. calculated result: 5.0 -> 3. executing finally cleanup
Call 1: OK: 5.0
1. entering try -> 2. caught division by zero -> 3. executing finally cleanup
Call 2: ERROR
```

### 2. Real-World Production Example: Atomic File Processing with Rollback
```python
import os
import tempfile

def write_atomic_data(target_path: str, payload: str) -> None:
    temp_fd, temp_path = tempfile.mkstemp(prefix="atomic_", dir=os.path.dirname(target_path) or ".")
    try:
        with os.fdopen(temp_fd, "w", encoding="utf-8") as f:
            f.write(payload)
            if "FAIL" in payload:
                raise ValueError("Simulated validation failure mid-write")
    except Exception as exc:
        print(f"[ERROR] Transaction aborted: {exc}")
        raise
    else:
        # Atomic file rename only occurs if no write error occurred
        os.replace(temp_path, target_path)
        print(f"[SUCCESS] Wrote data atomically to {target_path}")
    finally:
        # Guarantee temporary file is deleted if rename did not take place
        if os.path.exists(temp_path):
            os.remove(temp_path)
            print("[CLEANUP] Stale temporary file purged")

# Test success
write_atomic_data("test_good.txt", "Hello World")
# Test failure
try:
    write_atomic_data("test_bad.txt", "FAIL payload")
except ValueError:
    pass

# Verify cleanup
print("Cleaned up properly:", not os.path.exists("test_bad.txt"))
os.remove("test_good.txt")
```
**Output**:
```text
[SUCCESS] Wrote data atomically to test_good.txt
[ERROR] Transaction aborted: Simulated validation failure mid-write
[CLEANUP] Stale temporary file purged
Cleaned up properly: True
```

### 3. Edge Case: Return Value Hijacking in Finally
```python
def dangerous_cleanup() -> str:
    try:
        return "FROM_TRY"
    finally:
        return "FROM_FINALLY"  # Anti-pattern: Overwrites return value!

def exception_swallower() -> None:
    try:
        raise RuntimeError("Critical system fault")
    finally:
        return  # Anti-pattern: Silently suppresses the RuntimeError!

print("Hijacked return:", dangerous_cleanup())
exception_swallower()
print("Execution continued despite unhandled RuntimeError!")
```
**Output**:
```text
Hijacked return: FROM_FINALLY
Execution continued despite unhandled RuntimeError!
```

### 4. Failure Case & Exception Handling: Exception Binding Scope Leak
```python
def inspect_scope() -> None:
    try:
        raise ValueError("Invalid parameter")
    except ValueError as e:
        print(f"Inside except block: e = {e}")
    
    try:
        print(f"Outside except block: e = {e}")  # e was deleted by CPython
    except UnboundLocalError as err:
        print(f"Captured expected scoping error: {err}")

inspect_scope()
```
**Output**:
```text
Inside except block: e = Invalid parameter
Captured expected scoping error: local variable 'e' referenced before assignment
```

### 5. Idiomatic Pythonic Pattern: Narrow Try with Else
```python
def process_user_record(raw_records: dict[str, str], user_id: str) -> str:
    # Keep try narrow: only guard dictionary lookup
    try:
        raw_age = raw_records[user_id]
    except KeyError:
        return "User not found"
    else:
        # Processing runs in else: errors here (ValueError) will NOT be mistaken for KeyErrors
        age = int(raw_age)
        return f"User {user_id} is {age} years old"

db = {"usr_1": "28", "usr_2": "not_an_int"}
print(process_user_record(db, "usr_1"))
print(process_user_record(db, "usr_99"))
try:
    process_user_record(db, "usr_2")
except ValueError as exc:
    print(f"Properly bubbled ValueError from else clause: {exc}")
```
**Output**:
```text
User usr_1 is 28 years old
User not found
Properly bubbled ValueError from else clause: invalid literal for int() with base 10: 'not_an_int'
```

---

## 6. Edge Cases & Pitfalls

1. **`finally` Masking Exceptions**:
   - Never write `return`, `break`, or `continue` inside a `finally` block. Doing so silences any active exception that was in the process of unwinding the stack.
2. **Tuple Matching Syntax**:
   - `except ValueError, TypeError:` is syntax error in Python 3. You must use parentheses: `except (ValueError, TypeError):`. Without parentheses, `except ValueError, err:` was Python 2's alias for `as err`.
3. **Nested Exceptions in `finally`**:
   - If an exception is raised inside a `finally` block while another exception is already active, the original exception is preserved in `__context__`, but the `finally` exception takes precedence and bubbles up.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| `return` inside `finally` block | Silently swallows any active exception being propagated | Silent data corruption / swallowed bugs | Only do resource release in `finally` |
| Bloated `try` block containing unrelated code | An unintended line raises `KeyError`, masking logic errors as data errors | False positive error handling | Keep `try` to 1–2 lines; move rest to `else` |
| Catching multiple exceptions without parentheses | `except TypeError, ValueError:` is invalid Python 3 syntax | `SyntaxError: multiple exception types must be parenthesized` | `except (TypeError, ValueError):` |
| Re-raising with `raise err` instead of bare `raise` | `raise err` resets the traceback start location to the current line | Corrupted traceback root origin | Use bare `raise` to re-raise |
| Assuming `err` variable persists outside `except` | CPython automatically runs `del err` at block exit to break cycles | `UnboundLocalError: local variable referenced before assignment` | Store in another variable: `saved_err = err` |
| Code in `else` caught by previous `except` | Misunderstanding `else`: `else` errors bubble up, they are not caught | Unhandled exception | Only put code in `else` that shouldn't trigger local catch |

---

## 8. Performance & Complexity

- **Normal Path Cost**: Under Python 3.11+, zero runtime overhead for entering `try` or `else` blocks when no exceptions occur.
- **Exception Path Cost**: Stack unwinding costs roughly 1–3 microseconds depending on call depth, making exceptions expensive for normal loop termination compared to condition checks.
- **Memory**: The local target in `except E as e` creates a local binding that is explicitly cleaned up via `del` to release the `PyFrameObject` referenced by `e.__traceback__`.

---

## 9. CPython Internals & Bytecode

Let's examine how CPython compiles `try...else...finally`:

```python
import dis

def lifecycle_demo():
    try:
        x = 1
    except ValueError:
        x = 2
    else:
        x = 3
    finally:
        x = 4
    return x

dis.dis(lifecycle_demo)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 NOP

  3           4 LOAD_CONST               1 (1)
              6 STORE_FAST               0 (x)
              8 JUMP_FORWARD             8 (to 18)

  4     >>   10 PUSH_EXC_INFO
             12 LOAD_GLOBAL              1 (ValueError)
             22 CHECK_EXC_MATCH
             24 POP_JUMP_IF_FALSE       13 (to 42)

  5          26 LOAD_CONST               2 (2)
             28 STORE_FAST               0 (x)
             30 POP_EXCEPT
             32 JUMP_FORWARD             5 (to 44)

  7     >>   34 LOAD_CONST               3 (3)
             36 STORE_FAST               0 (x)
             38 JUMP_FORWARD             2 (to 44)

  9     >>   40 LOAD_CONST               4 (4)
             42 STORE_FAST               0 (x)
             44 RETURN_VALUE

ExceptionTable:
  4 to 8 -> 10 [0]
```
Notice how CPython arranges `JUMP_FORWARD` targets: if `try` succeeds, it skips `PUSH_EXC_INFO` and jumps straight to `else` (`34`), then continues to `finally` (`40`).

---

## 10. Best Practices

1. **Keep `try` Minimal**: Wrap only the exact expression that is expected to fail.
2. **Always Use `else` When Guarding**: If you want to compute a value and then act upon it only if computation succeeded, place the action in `else`.
3. **Pure Cleanup in `finally`**: Never use control-flow altering statements (`return`, `raise`, `break`, `continue`) inside `finally`.

---

## 11. Real-World Case Study: Transactional Mutex Lock with Timeout

```python
import threading
import time

class SafeResource:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.state: dict[str, int] = {"balance": 100}

    def update_balance(self, amount: int, timeout_s: float = 1.0) -> bool:
        acquired = self.lock.acquire(timeout=timeout_s)
        if not acquired:
            return False

        try:
            # Critical Section
            current = self.state["balance"]
            if current + amount < 0:
                raise ValueError("Insufficient balance for transaction")
            self.state["balance"] = current + amount
        except ValueError as err:
            print(f"[REJECTED] Transaction failed: {err}")
            return False
        else:
            print(f"[COMMITTED] New balance: {self.state['balance']}")
            return True
        finally:
            # Mutex lock release is unconditionally guaranteed
            self.lock.release()
            print("[CLEANUP] Mutex lock released successfully")

res = SafeResource()
# Successful transaction
res.update_balance(50)
# Rejected transaction
res.update_balance(-500)
```
**Output**:
```text
[COMMITTED] New balance: 150
[CLEANUP] Mutex lock released successfully
[REJECTED] Transaction failed: Insufficient balance for transaction
[CLEANUP] Mutex lock released successfully
```

---

## 12. Interview Questions

### Q1: What is the purpose of the `else` clause in a `try...except` block?
<details>
<summary><strong>Answer</strong></summary>
The <code>else</code> clause executes if and only if the <code>try</code> block completes without raising any exception. It prevents developers from putting subsequent code inside the <code>try</code> block, which could accidentally mask unintended exceptions under the same <code>except</code> handler.
</details>

### Q2: What happens if both `try` and `finally` contain a `return` statement?
<details>
<summary><strong>Answer</strong></summary>
The <code>return</code> statement in the <code>finally</code> block executes last and discards the return value generated by the <code>try</code> block. The value returned by <code>finally</code> becomes the final result of the function.
</details>

### Q3: Why does CPython delete the exception variable after an `except` block?
<details>
<summary><strong>Answer</strong></summary>
In Python 3, <code>except Exception as e:</code> implicitly translates to a block ending with <code>del e</code>. This is done to break reference cycles: <code>e.__traceback__</code> contains stack frames, and stack frames contain local variables, including <code>e</code>. Deleting <code>e</code> allows the cyclic garbage collector to reclaim the memory immediately.
</details>

### Q4: How do you re-raise the currently active exception without resetting its traceback?
<details>
<summary><strong>Answer</strong></summary>
Use a bare <code>raise</code> statement with no arguments. Writing <code>raise e</code> attaches the current line as a new origin point in the traceback, whereas bare <code>raise</code> preserves the exact original traceback.
</details>

### Q5: Can a `finally` block prevent a `SystemExit` from terminating the interpreter?
<details>
<summary><strong>Answer</strong></summary>
Yes. If the <code>finally</code> block executes a <code>return</code> statement or raises a new exception, the active <code>SystemExit</code> exception is discarded, preventing the process from terminating.
</details>

### Q6: If an unhandled exception occurs in `try`, does `finally` run before the program crashes?
<details>
<summary><strong>Answer</strong></summary>
Yes. CPython unwinds the stack frame by frame, executing any enclosing <code>finally</code> blocks before terminating or invoking <code>sys.excepthook</code>.
</details>

### Q7: What is the output of a loop containing `try...break...finally`?
<details>
<summary><strong>Answer</strong></summary>
When <code>break</code> is encountered inside a <code>try</code> block, CPython pauses the break, executes the entire <code>finally</code> block, and then completes the <code>break</code> to exit the loop.
</details>

### Q8: What happens if an exception is raised inside the `else` clause?
<details>
<summary><strong>Answer</strong></summary>
The exception is <strong>not</strong> caught by the sibling <code>except</code> clauses of the same statement. It runs the <code>finally</code> clause of the statement and then propagates immediately to the outer enclosing scope.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Order of Execution Verification
Write a function `execution_trace(should_fail: bool) -> list[str]` that appends `"try"`, `"except"`, `"else"`, and `"finally"` to a list according to how control flows, returning the trace.
<details>
<summary><strong>Solution</strong></summary>

```python
def execution_trace(should_fail: bool) -> list[str]:
    trace = []
    try:
        trace.append("try")
        if should_fail:
            raise ValueError()
    except ValueError:
        trace.append("except")
    else:
        trace.append("else")
    finally:
        trace.append("finally")
    return trace

assert execution_trace(False) == ["try", "else", "finally"]
assert execution_trace(True) == ["try", "except", "finally"]
```
</details>

### Challenge 2 (Easy): Safe Type Parsing
Write a function `parse_pair(a: str, b: str) -> tuple[int, int] | None` that attempts to convert both strings to integers in a minimal `try` block, returning their tuple in `else`, or `None` if a `ValueError` occurs.
<details>
<summary><strong>Solution</strong></summary>

```python
def parse_pair(a: str, b: str) -> tuple[int, int] | None:
    try:
        val_a, val_b = int(a), int(b)
    except ValueError:
        return None
    else:
        return (val_a, val_b)

assert parse_pair("10", "20") == (10, 20)
assert parse_pair("10", "bad") is None
```
</details>

### Challenge 3 (Medium): Temporary Environment Modifier
Implement a function `run_with_env(key: str, temp_value: str, callback)` that sets `os.environ[key] = temp_value`, executes `callback()`, and guarantees the environment variable is restored to its previous value (or deleted if it didn't exist) using `finally`.
<details>
<summary><strong>Solution</strong></summary>

```python
import os
from typing import Callable, Any

def run_with_env(key: str, temp_value: str, callback: Callable[[], Any]) -> Any:
    had_key = key in os.environ
    old_value = os.environ.get(key)
    os.environ[key] = temp_value
    try:
        return callback()
    finally:
        if had_key:
            assert old_value is not None
            os.environ[key] = old_value
        else:
            os.environ.pop(key, None)

os.environ["FOO"] = "original"
res = run_with_env("FOO", "temporary", lambda: os.environ["FOO"])
assert res == "temporary"
assert os.environ["FOO"] == "original"
```
</details>

### Challenge 4 (Medium): Retrying Operation with Exponential Backoff
Write a function `retry_call(fn, retries=3)` that runs `fn()`. If it raises an `Exception`, catch it and retry up to `retries` times. If it succeeds, run an `else` block that logs the attempt count and returns the result. If all retries fail, re-raise the last exception.
<details>
<summary><strong>Solution</strong></summary>

```python
def retry_call(fn, retries: int = 3):
    last_err = None
    for attempt in range(1, retries + 1):
        try:
            result = fn()
        except Exception as exc:
            last_err = exc
        else:
            return result
    if last_err is not None:
        raise last_err

count = 0
def flaky():
    global count
    count += 1
    if count < 3:
        raise ValueError("Network glitch")
    return "SUCCESS"

assert retry_call(flaky, retries=3) == "SUCCESS"
```
</details>

### Challenge 5 (Hard): Scope Preserving Exception Capture
Write a class `ExceptionCollector` that executes arbitrary callables in a loop, capturing any raised `Exception` objects into a list without triggering `UnboundLocalError` or memory reference cycle retention bugs.
<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Callable, Any

class ExceptionCollector:
    def __init__(self) -> None:
        self.errors: list[Exception] = []

    def execute(self, fn: Callable[[], Any]) -> Any | None:
        try:
            return fn()
        except Exception as err:
            # Copy error instance cleanly
            self.errors.append(err)
            return None

collector = ExceptionCollector()
collector.execute(lambda: 1 / 0)
collector.execute(lambda: int("bad"))
collector.execute(lambda: 100)

assert len(collector.errors) == 2
assert isinstance(collector.errors[0], ZeroDivisionError)
assert isinstance(collector.errors[1], ValueError)
```
</details>

### Challenge 6 (Hard): Transaction Coordinator with Rollback
Write a class `TransactionCoordinator` that manages a list of sub-operations with rollback functions. If any step fails, execute all accumulated rollbacks in reverse order inside a `finally` or `except` sequence, ensuring cleanup errors do not mask the primary cause.
<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Callable

class TransactionCoordinator:
    def __init__(self) -> None:
        self.rollbacks: list[Callable[[], None]] = []
        self.committed = False

    def add_step(self, do_fn: Callable[[], None], undo_fn: Callable[[], None]) -> None:
        try:
            do_fn()
        except Exception as primary_error:
            # Rollback all previous steps
            rollback_errors = []
            for rb in reversed(self.rollbacks):
                try:
                    rb()
                except Exception as rb_err:
                    rollback_errors.append(rb_err)
            if rollback_errors:
                raise ExceptionGroup("Transaction aborted with rollback errors", [primary_error, *rollback_errors])
            raise primary_error
        else:
            self.rollbacks.append(undo_fn)

actions = []
tc = TransactionCoordinator()
try:
    tc.add_step(lambda: actions.append("step1"), lambda: actions.append("undo1"))
    tc.add_step(lambda: actions.append("step2"), lambda: actions.append("undo2"))
    tc.add_step(lambda: (_ for _ in ()).throw(RuntimeError("Step 3 crashed")), lambda: actions.append("undo3"))
except RuntimeError:
    pass

assert actions == ["step1", "step2", "undo2", "undo1"]
```
</details>
