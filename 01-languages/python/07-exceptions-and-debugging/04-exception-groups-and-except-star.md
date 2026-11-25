# Module 07: Exceptions, Context Managers & Debugging (04-exception-groups-and-except-star)

> **Topic Overview**: Introduced in Python 3.11 via PEP 654, `ExceptionGroup` and the `except*` compound statement revolutionize error handling in concurrent and multi-failure environments. Instead of discarding secondary errors or picking an arbitrary "first" exception, Python can now propagate, filter, and handle trees of unrelated concurrent exceptions simultaneously.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Concurrency & Error Handling |
| **Target Level** | Advanced |
| **Python Version** | Python 3.11+ (tested on Python 3.12 - 3.14+) |
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

- **Problem Statement**: Prior to Python 3.11, concurrent programming models (such as `asyncio.gather` or multi-threaded worker pools) faced a fundamental dilemma when multiple tasks failed simultaneously. The runtime had to either pick one arbitrary exception to raise (swallowing all others), or return a list of raw exceptions, forcing callers to inspect and manually route errors.
- **Mental Model**: An `ExceptionGroup` is a tree of exceptions. Traditional `try...except` handles *one* exception type at a time. The modern `try...except*` ("except star") construct acts like a concurrent filter, splitting the group into matching subgroups, executing corresponding handlers, and re-bundling any unhandled exceptions to propagate up the stack.
- **When to Use**:
  - Always when working with `asyncio.TaskGroup` in Python 3.11+.
  - When batch-processing tasks where multiple failures can occur independently (e.g., validation rules, multi-server health checks).
- **When NOT to Use**: Do not use `except*` for standard sequential code where only one exception can possibly occur at a time. Do not mix `except` and `except*` on the same `try` block.

---

## 2. Core Concepts

### ExceptionGroup Hierarchy
PEP 654 introduced two primary types:

```text
BaseException
 ├── BaseExceptionGroup[BaseException]  (Can contain SystemExit, KeyboardInterrupt)
 └── Exception
      └── ExceptionGroup[Exception]     (Can ONLY contain Exception subclasses)
```

1. **`ExceptionGroup(message, exceptions)`**: Holds one or more `Exception` instances. It is a subclass of `Exception`.
2. **`BaseExceptionGroup(message, exceptions)`**: Holds one or more `BaseException` instances (or a mix of `Exception` and `BaseException` like `KeyboardInterrupt`). It is a subclass of `BaseException`, not `Exception`.

### The `except*` Matching and Splitting Algorithm
Unlike classic `except` which executes at most **one** clause, `try...except*` can execute **multiple** `except*` clauses for a single group!

```text
[Incoming ExceptionGroup containing: ValueError, TypeError, KeyError]
                          │
                          v
         try: ... except* ValueError:  <--- Matches ValueError
                          │                 Executes handler for ValueError
                          │                 Remaining in group: [TypeError, KeyError]
                          v
                  except* LookupError: <--- Matches KeyError (subclass)
                          │                 Executes handler for KeyError
                          │                 Remaining in group: [TypeError]
                          v
[Unhandled: TypeError is re-packaged into a new ExceptionGroup and propagated up]
```

---

## 3. How It Works Internally

CPython implements exception group matching via recursive splitting (`subgroup` and `split`):

```text
+--------------------------------------------------------------------------+
| ExceptionGroup Decomposition Flow                                        |
+--------------------------------------------------------------------------+
| Input Group: ExceptionGroup("batch errors", [E1(ValueError), E2(OSError)])|
|                                                                          |
| 1. Evaluates 'except* ValueError as eg':                                 |
|    - Calls internal C routine: PyException_Split(group, match_type)      |
|    - Matching subgroup: ExceptionGroup("batch errors", [E1])             |
|    - Non-matching rest: ExceptionGroup("batch errors", [E2])             |
| 2. 'eg' is bound to Matching Subgroup; handler executes                  |
| 3. Unmatched rest continues down to next except* clause                  |
| 4. If any errors remain unhandled at the end, CPython synthesizes        |
|    a new ExceptionGroup preserving the original tree shape and raises it |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Syntax of `try...except*`
```python
try:
    risky_concurrent_work()
except* ValueError as eg:
    # eg is an ExceptionGroup containing ONLY the matching ValueErrors
    for err in eg.exceptions:
        handle_value_error(err)
except* (KeyError, IndexError) as eg:
    # Multiple matching types in a tuple
    handle_lookup_errors(eg)
except* Exception as eg:
    # Catches all remaining application exceptions in the group
    handle_fallback(eg)
```

### Methods on `ExceptionGroup`
- `eg.exceptions`: Tuple of immediate children exceptions.
- `eg.subgroup(condition)`: Returns a new `ExceptionGroup` containing only exceptions satisfying `condition` (either a type, tuple of types, or predicate function `Callable[[BaseException], bool]`), or `None` if no match.
- `eg.split(condition)`: Returns a tuple `(match_subgroup, rest_subgroup)`. If no match, `(None, self)`. If full match, `(self, None)`.

---

## 5. Code Examples

### 1. Minimal / Basic Example: Raising and Catching with `except*`
```python
def trigger_errors() -> None:
    raise ExceptionGroup(
        "Multiple validation failures",
        [
            ValueError("Username is too short"),
            TypeError("Age must be an integer"),
            ValueError("Password requires digits")
        ]
    )

try:
    trigger_errors()
except* ValueError as eg:
    print(f"Caught {len(eg.exceptions)} ValueErrors:")
    for err in eg.exceptions:
        print(f"  - {err}")
except* TypeError as eg:
    print(f"Caught {len(eg.exceptions)} TypeErrors:")
    for err in eg.exceptions:
        print(f"  - {err}")
```
**Output**:
```text
Caught 2 ValueErrors:
  - Username is too short
  - Password requires digits
Caught 1 TypeErrors:
  - Age must be an integer
```

### 2. Real-World Production Example: Async TaskGroup Failure
```python
import asyncio

async def fetch_user_data(user_id: int) -> dict:
    if user_id == 1:
        raise ConnectionResetError("Database connection lost")
    if user_id == 2:
        raise ValueError("Invalid user permissions")
    await asyncio.sleep(0.01)
    return {"id": user_id, "status": "active"}

async def run_batch():
    try:
        async with asyncio.TaskGroup() as tg:
            tg.create_task(fetch_user_data(1))
            tg.create_task(fetch_user_data(2))
    except* ConnectionResetError as eg:
        print(f"[RETRYABLE] Network issues detected: {eg.exceptions}")
    except* ValueError as eg:
        print(f"[NON-RETRYABLE] Client error detected: {eg.exceptions}")

asyncio.run(run_batch())
```
**Output**:
```text
[RETRYABLE] Network issues detected: (ConnectionResetError('Database connection lost'),)
[NON-RETRYABLE] Client error detected: (ValueError('Invalid user permissions'),)
```

### 3. Edge Case: Programmatic Splitting with `eg.split()`
```python
eg = ExceptionGroup(
    "Service failures",
    [
        FileNotFoundError("/etc/app.conf"),
        ConnectionRefusedError("127.0.0.1:5432"),
        KeyError("Missing secret"),
        PermissionError("/var/log/app.log")
    ]
)

# Split out all filesystem OSErrors
filesystem_errors, remaining = eg.split(lambda e: isinstance(e, (FileNotFoundError, PermissionError)))

print("Filesystem Subgroup:", filesystem_errors.exceptions if filesystem_errors else None)
print("Remaining Subgroup:", remaining.exceptions if remaining else None)
```
**Output**:
```text
Filesystem Subgroup: (FileNotFoundError('/etc/app.conf'), PermissionError('/var/log/app.log'))
Remaining Subgroup: (ConnectionRefusedError('127.0.0.1:5432'), KeyError('Missing secret'))
```

### 4. Failure Case & Exception Handling: Unhandled Subgroups Re-Raising
```python
def partial_handler():
    try:
        raise ExceptionGroup(
            "Batch run",
            [
                ValueError("Bad value"),
                RuntimeError("Worker segfault simulation")
            ]
        )
    except* ValueError as eg:
        print("Handled ValueError cleanly.")
        # Notice: RuntimeError is NOT handled here!

try:
    partial_handler()
except ExceptionGroup as unhandled_group:
    print(f"Top-level intercepted unhandled subgroup: {unhandled_group.message}")
    print(f"Contained unhandled types: {[type(e).__name__ for e in unhandled_group.exceptions]}")
```
**Output**:
```text
Handled ValueError cleanly.
Top-level intercepted unhandled subgroup: Batch run
Contained unhandled types: ['RuntimeError']
```

### 5. Idiomatic Pythonic Pattern: Nested Exception Group Inspection
```python
def inspect_group_tree(exc: BaseException, indent: int = 0) -> None:
    prefix = "  " * indent
    if isinstance(exc, (ExceptionGroup, BaseExceptionGroup)):
        print(f"{prefix}+-- [Group] {exc.message} ({len(exc.exceptions)} items)")
        for child in exc.exceptions:
            inspect_group_tree(child, indent + 1)
    else:
        print(f"{prefix}+-- [Leaf] {type(exc).__name__}: {exc}")

nested_tree = ExceptionGroup(
    "Root Coordinator",
    [
        ExceptionGroup("Worker Pool A", [ValueError("A1 failed"), TimeoutError("A2 timed out")]),
        ExceptionGroup("Worker Pool B", [KeyError("B1 missing key")])
    ]
)

inspect_group_tree(nested_tree)
```
**Output**:
```text
+-- [Group] Root Coordinator (2 items)
  +-- [Group] Worker Pool A (2 items)
    +-- [Leaf] ValueError: A1 failed
    +-- [Leaf] TimeoutError: A2 timed out
  +-- [Group] Worker Pool B (1 items)
    +-- [Leaf] KeyError: 'B1 missing key'
```

---

## 6. Edge Cases & Pitfalls

1. **Cannot Mix `except` and `except*`**:
   - In a single `try` statement, you must choose either all traditional `except` clauses or all `except*` clauses. Mixing them generates a `SyntaxError: cannot have both 'except' and 'except*' on the same 'try'`.
2. **`BaseExceptionGroup` with `SystemExit`**:
   - If an `ExceptionGroup` contains even a single `BaseException` (like `KeyboardInterrupt` or `SystemExit`), Python automatically constructs a `BaseExceptionGroup`. Standard `except* Exception` clauses will **not** match that signal! You must use `except* BaseException` or `except* KeyboardInterrupt`.
3. **Empty Exception Groups Are Prohibited**:
   - Calling `ExceptionGroup("msg", [])` raises `ValueError: ExceptionGroup exceptions must be a non-empty sequence`.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Mixing `except` and `except*` on same try | Invalid language grammar in Python 3.11+ | `SyntaxError: cannot have both 'except' and 'except*' on the same 'try'` | Use all `except*` or all `except` |
| Instantiating `ExceptionGroup` with empty list | PEP 654 requires at least one nested exception | `ValueError: ExceptionGroup exceptions must be a non-empty sequence` | Only raise if `len(errors) > 0` |
| Catching `ExceptionGroup` with classic `except ExceptionGroup:` | Prevents subgroup filtering; forces manual unwinding | High maintenance / fragile code | Use `except* SpecificError:` |
| Raising non-exception in group list | Every element in the sequence must derive from `BaseException` | `TypeError: Item 0 of exceptions is not an exception` | Ensure all items are exception instances |
| Assuming `except*` halts at first match | Unlike `except`, `except*` tests and executes ALL clauses | Multiple clauses running unexpectedly | Design clauses knowing multiple can run |
| Raising new exception inside `except*` | Replaces or wraps unhandled exceptions into another higher-level group | Surprising grouped exceptions | Handle carefully or use chaining |

---

## 8. Performance & Complexity

- **Splitting Complexity**: `eg.split(cls)` performs a preorder traversal of the group tree in $O(N)$ where $N$ is the total count of nested exception leaves.
- **Handler Execution**: Each matching `except*` clause is executed sequentially in the order written.
- **Memory**: An `ExceptionGroup` node adds 64 bytes plus a tuple referencing child exceptions and individual traceback structures.

---

## 9. CPython Internals & Bytecode

Let's inspect the bytecode instructions compiled for an `except*` statement:

```python
import dis

def except_star_demo():
    try:
        pass
    except* ValueError:
        pass

dis.dis(except_star_demo)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 NOP

  3           4 JUMP_FORWARD             2 (to 8)

  4     >>    6 PUSH_EXC_INFO
              8 LOAD_GLOBAL              1 (ValueError)
             18 CHECK_EG_MATCH
             20 POP_JUMP_IF_NONE        12 (to 46)

  5          22 POP_EXCEPT
             24 JUMP_BACKWARD            4 (to 18)
```
Notice the specialized CPython instructions:
- `CHECK_EG_MATCH`: Takes the target exception type and splits the active `ExceptionGroup`. If a match is found, it extracts the matching subgroup and pushes it to the stack. If no match is found, it pushes `None`.
- `POP_JUMP_IF_NONE`: If `CHECK_EG_MATCH` returned `None`, it jumps immediately to the next `except*` clause without running the block.

---

## 10. Best Practices

1. **Adopt with TaskGroups**: Pair `except*` directly with `asyncio.TaskGroup()` for clean async structured concurrency.
2. **Handle by Domain Category**: Write distinct `except*` clauses for retryable transient errors (network timeouts) versus non-retryable fatal errors (schema validation).
3. **Preserve Subgroups When Re-raising**: When transforming errors, derive custom exception groups by subclassing `ExceptionGroup`.

---

## 11. Real-World Case Study: Parallel Health Check Orchestrator

```python
import time
from typing import Callable

def check_redis() -> str:
    raise ConnectionRefusedError("Redis server port 6379 closed")

def check_postgres() -> str:
    raise TimeoutError("PostgreSQL pool acquire timeout after 5.0s")

def check_disk_space() -> str:
    return "DISK_OK: 45GB free"

def run_health_checks(checks: dict[str, Callable[[], str]]) -> dict[str, str]:
    results = {}
    failures = []

    for name, check_fn in checks.items():
        try:
            results[name] = check_fn()
        except Exception as exc:
            failures.append(exc)

    if failures:
        raise ExceptionGroup("Multiple system health checks failed", failures)
    return results

# Orchestrator handling health check results
try:
    run_health_checks({
        "redis": check_redis,
        "postgres": check_postgres,
        "disk": check_disk_space
    })
except* ConnectionRefusedError as eg:
    print(f"[ALARM - CRITICAL] Database/Cache offline: {eg.exceptions}")
except* TimeoutError as eg:
    print(f"[ALARM - WARNING] Service latency bottleneck: {eg.exceptions}")
```
**Output**:
```text
[ALARM - CRITICAL] Database/Cache offline: (ConnectionRefusedError('Redis server port 6379 closed'),)
[ALARM - WARNING] Service latency bottleneck: (TimeoutError('PostgreSQL pool acquire timeout after 5.0s'),)
```

---

## 12. Interview Questions

### Q1: What motivated the introduction of `ExceptionGroup` and `except*` in PEP 654?
<details>
<summary><strong>Answer</strong></summary>
Prior to Python 3.11, structured concurrency libraries (like <code>asyncio.TaskGroup</code>) had no clean way to report multiple simultaneous failures. If three concurrent tasks failed, the runtime had to discard two or return raw lists. <code>ExceptionGroup</code> allows multiple exceptions to be bundled into a tree, while <code>except*</code> allows selective handling of individual exception types from that tree.
</details>

### Q2: How does `except*` differ fundamentally from classic `except`?
<details>
<summary><strong>Answer</strong></summary>
Classic <code>except</code> matches at most one clause per <code>try</code> block (the first match executes, and the rest are ignored). In contrast, <code>except*</code> can execute <strong>multiple</strong> clauses for a single <code>ExceptionGroup</code>. Each clause extracts and handles its matching subgroup, and any unhandled exceptions are recombined and propagated.
</details>

### Q3: What happens if an `except*` clause raises a new exception?
<details>
<summary><strong>Answer</strong></summary>
If an <code>except*</code> clause raises a new exception, that new exception is combined with any other exceptions raised in other <code>except*</code> clauses and any unhandled leaves of the original group, forming a new <code>ExceptionGroup</code> that propagates up the call stack.
</details>

### Q4: Can you catch a normal, single `ValueError` using `except* ValueError:`?
<details>
<summary><strong>Answer</strong></summary>
Yes! If a non-grouped exception (e.g. <code>raise ValueError("simple")</code>) is thrown inside a <code>try...except*</code> block, CPython automatically wraps it into an ephemeral <code>ExceptionGroup</code> with one element. The matching <code>except* ValueError as eg:</code> clause executes, and <code>eg.exceptions</code> contains that single exception.
</details>

### Q5: What is the difference between `ExceptionGroup` and `BaseExceptionGroup`?
<details>
<summary><strong>Answer</strong></summary>
<code>ExceptionGroup</code> inherits from <code>Exception</code> and can only contain children that inherit from <code>Exception</code>. If any child is a direct subclass of <code>BaseException</code> (such as <code>KeyboardInterrupt</code> or <code>SystemExit</code>), CPython automatically requires or creates a <code>BaseExceptionGroup</code>, which inherits directly from <code>BaseException</code>.
</details>

### Q6: Can you catch an `ExceptionGroup` using a standard `except Exception:` clause?
<details>
<summary><strong>Answer</strong></summary>
Yes. Because <code>ExceptionGroup</code> inherits from <code>Exception</code>, a classic <code>except Exception as e:</code> block will catch the entire group as a monolithic object. However, you lose the automatic subgroup filtering and parallel handling capabilities of <code>except*</code>.
</details>

### Q7: What does `eg.subgroup(predicate)` return if no exceptions match?
<details>
<summary><strong>Answer</strong></summary>
It returns <code>None</code>. It never returns an empty <code>ExceptionGroup</code> because empty groups are prohibited in Python.
</details>

### Q8: What bytecode instruction was added to CPython for `except*`?
<details>
<summary><strong>Answer</strong></summary>
Python 3.11 added the <code>CHECK_EG_MATCH</code> instruction. It takes an exception type from the stack, splits the active exception group, and either pushes the matching subgroup or pushes <code>None</code> if no match was found.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Basic Group Construction
Create an `ExceptionGroup` named `"Math Errors"` containing a `ZeroDivisionError` and an `OverflowError`. Catch both in separate `except*` blocks and print their messages.
<details>
<summary><strong>Solution</strong></summary>

```python
messages = []
try:
    raise ExceptionGroup("Math Errors", [ZeroDivisionError("Div by 0"), OverflowError("Int too large")])
except* ZeroDivisionError as eg:
    messages.append(str(eg.exceptions[0]))
except* OverflowError as eg:
    messages.append(str(eg.exceptions[0]))

assert messages == ["Div by 0", "Int too large"]
```
</details>

### Challenge 2 (Easy): Programmatic Subgroup Extraction
Given an `ExceptionGroup` with mixed errors (`KeyError`, `ValueError`, `TypeError`), use `.subgroup()` to extract all `LookupError` instances into a new group and verify its leaf count.
<details>
<summary><strong>Solution</strong></summary>

```python
eg = ExceptionGroup("mixed", [KeyError("k1"), ValueError("v1"), KeyError("k2"), TypeError("t1")])
lookups = eg.subgroup(LookupError)

assert lookups is not None
assert len(lookups.exceptions) == 2
assert all(isinstance(e, KeyError) for e in lookups.exceptions)
```
</details>

### Challenge 3 (Medium): Leaf Count Flattener
Write a function `count_leaf_exceptions(eg: BaseException) -> int` that recursively counts the total number of non-group leaf exceptions inside an arbitrary nested `ExceptionGroup` tree.
<details>
<summary><strong>Solution</strong></summary>

```python
def count_leaf_exceptions(exc: BaseException) -> int:
    if isinstance(exc, (ExceptionGroup, BaseExceptionGroup)):
        return sum(count_leaf_exceptions(child) for child in exc.exceptions)
    return 1

tree = ExceptionGroup("root", [
    ValueError("1"),
    ExceptionGroup("sub", [TypeError("2"), KeyError("3")]),
    RuntimeError("4")
])

assert count_leaf_exceptions(tree) == 4
```
</details>

### Challenge 4 (Medium): Filter and Re-Raise Group
Write a function `filter_transient_errors(eg: ExceptionGroup) -> ExceptionGroup | None` that uses `eg.split()` to remove all `ConnectionError` and `TimeoutError` exceptions (assuming they are handled). If any non-transient exceptions remain, return the remaining subgroup; otherwise, return `None`.
<details>
<summary><strong>Solution</strong></summary>

```python
def filter_transient_errors(eg: ExceptionGroup) -> ExceptionGroup | None:
    _, remaining = eg.split((ConnectionError, TimeoutError))
    return remaining

transient_only = ExceptionGroup("net", [ConnectionRefusedError("fail"), TimeoutError("timed out")])
assert filter_transient_errors(transient_only) is None

mixed = ExceptionGroup("net", [ConnectionRefusedError("fail"), ValueError("Bad format")])
rem = filter_transient_errors(mixed)
assert rem is not None
assert len(rem.exceptions) == 1
assert isinstance(rem.exceptions[0], ValueError)
```
</details>

### Challenge 5 (Hard): Custom Recursive Exception Group Transformer
Write a function `sanitize_group_messages(eg: ExceptionGroup, secret_mask: str = "[REDACTED]") -> ExceptionGroup` that recursively traverses an exception group tree and replaces any occurrence of sensitive substrings in exception messages with `secret_mask`, returning a new reconstructed `ExceptionGroup` with the exact same tree structure and exception types.
<details>
<summary><strong>Solution</strong></summary>

```python
def sanitize_group_messages(exc: BaseException, secret: str = "password123") -> BaseException:
    if isinstance(exc, (ExceptionGroup, BaseExceptionGroup)):
        new_children = [sanitize_group_messages(child, secret) for child in exc.exceptions]
        new_msg = exc.message.replace(secret, "[REDACTED]")
        return type(exc)(new_msg, new_children)
    
    # Leaf exception
    old_msg = str(exc)
    new_msg = old_msg.replace(secret, "[REDACTED]")
    return type(exc)(new_msg)

tree = ExceptionGroup("root password123", [
    ValueError("user sent password123 as name"),
    ExceptionGroup("nested", [RuntimeError("leak password123 in log")])
])

sanitized = sanitize_group_messages(tree)
assert "[REDACTED]" in sanitized.message
assert "password123" not in sanitized.message
assert "[REDACTED]" in str(sanitized.exceptions[0])
```
</details>

### Challenge 6 (Hard): Concurrent Batch Executor with Dual Logging
Write a class `BatchJobRunner` that accepts a list of callables, executes all of them sequentially, collects all raised exceptions, and if any fail, raises an `ExceptionGroup`. Write test code using `try...except*` that routes `IOError` leaves to an `io_logger` and all other errors to an `app_logger`.
<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Callable, Any

class BatchJobRunner:
    def __init__(self, tasks: list[Callable[[], Any]]) -> None:
        self.tasks = tasks

    def run(self) -> None:
        errors = []
        for t in self.tasks:
            try:
                t()
            except Exception as e:
                errors.append(e)
        if errors:
            raise ExceptionGroup("BatchJobRunner execution failures", errors)

io_logs = []
app_logs = []

runner = BatchJobRunner([
    lambda: (_ for _ in ()).throw(FileNotFoundError("Missing config")),
    lambda: (_ for _ in ()).throw(ValueError("Bad number")),
    lambda: (_ for _ in ()).throw(PermissionError("Cannot write to /var")),
])

try:
    runner.run()
except* OSError as eg:
    for e in eg.exceptions:
        io_logs.append(f"IO: {e}")
except* Exception as eg:
    for e in eg.exceptions:
        app_logs.append(f"APP: {e}")

assert len(io_logs) == 2
assert len(app_logs) == 1
assert "Missing config" in io_logs[0]
assert "Bad number" in app_logs[0]
```
</details>
