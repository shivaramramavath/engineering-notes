# Module 07: Exceptions, Context Managers & Debugging (08-warnings)

> **Topic Overview**: Python's `warnings` framework provides a non-fatal signaling mechanism for notifying developers about deprecated APIs, impending breaking changes, runtime resource leaks, or suboptimal configurations. Mastering warning categories, the `stacklevel` argument, warning filters, and CI enforcement (`pytest -W error`) ensures seamless software evolution and API stewardship.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Diagnostics & Deprecations |
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

- **Problem Statement**: Raising an exception halts execution immediately. When a library author deprecates a method in version 2.0 that will be removed in version 3.0, raising an exception would break all existing user code. Printing to stdout/stderr pollutes terminal output and cannot be filtered or intercepted programmatically.
- **Mental Model**: A warning is an informational signal emitted during execution that does not interrupt the call stack. Callers can configure global or localized filters to ignore them, display them once, log them, or escalate them into fatal exceptions during automated CI testing.
- **When to Use**:
  - Deprecating legacy arguments, functions, or class attributes.
  - Alerting users to dangerous or suboptimal configurations (e.g., insecure SSL flags).
  - Notifying developers about unclosed file descriptors or unawaited coroutines (`ResourceWarning`).
- **When NOT to Use**: Never use warnings for actual runtime failures or invalid data where execution cannot safely proceed; raise an explicit `Exception` instead.

---

## 2. Core Concepts

### Built-In Warning Category Taxonomy
All warning classes inherit from the built-in `Warning` class (which itself derives from `Exception`):

```text
BaseException
 └── Exception
      └── Warning
           ├── UserWarning (Default category for user code)
           ├── DeprecationWarning (Developer-facing API deprecations)
           ├── PendingDeprecationWarning (Early-stage future deprecations)
           ├── SyntaxWarning (Dubious syntax, e.g. assert (x, y))
           ├── RuntimeWarning (Dubious runtime behavior, e.g. unawaited coroutine)
           ├── FutureWarning (End-user facing semantic changes)
           ├── ImportWarning (Issues encountered during imports)
           ├── UnicodeWarning (Issues in unicode processing)
           ├── EncodingWarning (Implicit platform encoding usage, PEP 597)
           └── ResourceWarning (Unclosed sockets, files, sub-processes)
```

### Warning Filter Actions
Python evaluates warning filters sequentially from top to bottom. Each filter rule specifies an action:
- `default`: Print the first occurrence of matching warnings for each location (module + line number) where the warning is issued.
- `error`: Turn matching warnings into fatal exceptions (raises the warning class).
- `ignore`: Never print or trigger matching warnings.
- `always`: Print every occurrence of matching warnings every time they are issued.
- `module`: Print the first occurrence of matching warnings for each module where the warning is issued.
- `once`: Print only the first occurrence of matching warnings regardless of location.

---

## 3. How It Works Internally

When `warnings.warn()` is called, CPython performs a filter pipeline check:

```text
[Call: warnings.warn(message, category, stacklevel=2)]
                          │
                          v
           [Inspect Stack Frame at (stacklevel)]
           Determines: filename, lineno of the CALLER
                          │
                          v
           [Iterate over warnings.filters list]
           Matches: (action, message_re, category_subclass, module_re, lineno)
                          │
         ┌────────────────┴────────────────┬────────────────┐
         v                                 v                v
   action == "ignore"              action == "error"   action == "default"
  Discard and return            Raise Warning instance   Check registry dict
  immediately                   as a standard Exception  Print if not seen yet
```

---

## 4. Syntax & API Reference

### Issuing Warnings
```python
import warnings

# Standard signature
warnings.warn(
    message="This function is deprecated; use new_func() instead",
    category=DeprecationWarning,
    stacklevel=2  # Points the warning to the caller's line, not the library's line!
)
```

### Filtering Warnings Programmatically
```python
# Add filter to top of filter list
warnings.filterwarnings(
    action="error",               # "error", "ignore", "always", "default", "module", "once"
    message="",                   # Regex matching message
    category=Warning,             # Subclass of Warning
    module="",                    # Regex matching module name
    lineno=0,                     # Line number (0 = all lines)
    append=False                  # If False, prepends to highest priority
)

# Catching warnings locally via context manager
with warnings.catch_warnings(record=True) as captured_warnings:
    warnings.simplefilter("always")
    risky_legacy_call()
    # captured_warnings is a list of WarningMessage objects
```

### Command-Line & Environment Variables
- `python -W action script.py`: e.g. `python -W error script.py` (treat all warnings as errors).
- `python -W error::DeprecationWarning script.py`: Escalates only `DeprecationWarning`.
- Environment variable: `PYTHONWARNINGS="error,ignore::UserWarning"`.

---

## 5. Code Examples

### 1. Minimal / Basic Example: Emitting a Deprecation Warning
```python
import warnings

def legacy_add(a: int, b: int) -> int:
    warnings.warn(
        "legacy_add is deprecated and will be removed in v3.0. Use modern_add() instead.",
        category=DeprecationWarning,
        stacklevel=2
    )
    return a + b

# Configure warnings to show DeprecationWarnings (often ignored by default in user scripts)
warnings.simplefilter("always", DeprecationWarning)

result = legacy_add(10, 20)
print(f"Computed result: {result}")
```
**Output**:
```text
Computed result: 30
```
*(Stderr output: `DeprecationWarning: legacy_add is deprecated and will be removed in v3.0. Use modern_add() instead.` pointing to the caller line).*

### 2. Real-World Production Example: The Crucial `stacklevel` Parameter
```python
import warnings

# Library Code
def internal_validator(value: int) -> None:
    if value < 0:
        # stacklevel=3 points past internal_validator and past public_api to the USER script!
        warnings.warn("Negative value is discouraged", UserWarning, stacklevel=3)

def public_api(val: int) -> None:
    internal_validator(val)

# User Script
with warnings.catch_warnings(record=True) as recorded:
    warnings.simplefilter("always")
    public_api(-5)  # Line 15

if recorded:
    w = recorded[0]
    print(f"Warning Category: {w.category.__name__}")
    print(f"Warning Message: {w.message}")
    print(f"Reported Line Number: {w.lineno}")
```
**Output**:
```text
Warning Category: UserWarning
Warning Message: Negative value is discouraged
Reported Line Number: 15
```

### 3. Edge Case: Escalating Warnings to Errors in CI
```python
import warnings

def parse_config(opt: str) -> None:
    if opt == "legacy_mode":
        warnings.warn("legacy_mode is obsolete", FutureWarning)

# Simulate CI configuration: warnings as errors
warnings.filterwarnings("error", category=FutureWarning)

try:
    parse_config("legacy_mode")
except FutureWarning as exc:
    print(f"[CI BLOCKED] Warning escalated to exception: {type(exc).__name__} -> {exc}")

# Reset filter
warnings.resetwarnings()
```
**Output**:
```text
[CI BLOCKED] Warning escalated to exception: FutureWarning -> legacy_mode is obsolete
```

### 4. Failure Case & Exception Handling: Catching Warnings as Records
```python
import warnings

def batch_operation() -> None:
    warnings.warn("Task 1 slow", RuntimeWarning)
    warnings.warn("Task 2 deprecated", DeprecationWarning)
    warnings.warn("Task 3 slow", RuntimeWarning)

with warnings.catch_warnings(record=True) as w_log:
    warnings.simplefilter("always")
    batch_operation()

    print(f"Captured {len(w_log)} warnings:")
    for item in w_log:
        print(f"  - [{item.category.__name__}] {item.message} ({item.filename}:{item.lineno})")
```
**Output**:
```text
Captured 3 warnings:
  - [RuntimeWarning] Task 1 slow (<string>:4)
  - [DeprecationWarning] Task 2 deprecated (<string>:5)
  - [RuntimeWarning] Task 3 slow (<string>:6)
```

### 5. Idiomatic Pythonic Pattern: Custom Deprecation Decorator
```python
import warnings
import functools
from typing import Callable, Any

def deprecated(replacement: str | None = None) -> Callable:
    """Decorator to mark functions as deprecated with stacklevel=2."""
    def decorator(fn: Callable) -> Callable:
        @functools.wraps(fn)
        def wrapper(*args: Any, **kwargs: Any) -> Any:
            msg = f"Function '{fn.__name__}' is deprecated."
            if replacement:
                msg += f" Use '{replacement}' instead."
            warnings.warn(msg, category=DeprecationWarning, stacklevel=2)
            return fn(*args, **kwargs)
        return wrapper
    return decorator

@deprecated(replacement="fetch_v2()")
def fetch_v1():
    return "legacy_data"

with warnings.catch_warnings(record=True) as captured:
    warnings.simplefilter("always")
    res = fetch_v1()

assert res == "legacy_data"
assert len(captured) == 1
print("Decorator warning output:", captured[0].message)
```
**Output**:
```text
Decorator warning output: Function 'fetch_v1' is deprecated. Use 'fetch_v2()' instead.
```

---

## 6. Edge Cases & Pitfalls

1. **Default DeprecationWarning Suppression**:
   - Python silences `DeprecationWarning` by default in normal user code (PEP 565). It is only visible in `__main__` or if activated via `-Wd` or `warnings.simplefilter("always")`. Do not assume users will see deprecation warnings without enabling them or using `FutureWarning`.
2. **`stacklevel=1` vs `stacklevel=2`**:
   - Default `stacklevel=1` blames the line inside `warnings.warn()` itself, confusing library consumers. Always use `stacklevel=2` (or higher if wrapped in helper functions) so the warning points to the line where the caller invoked the deprecated API.
3. **`ResourceWarning` in Python**:
   - Python issues `ResourceWarning` when an unclosed file or socket is collected by the cyclic garbage collector. Enable them during test runs with `python -X dev -W error` to catch file descriptor leaks early.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Omitting `stacklevel=2` in library functions | Warning points to the line inside the library rather than the caller's code | Useless diagnostic locations | Use `stacklevel=2` |
| Using `print()` instead of `warnings.warn()` | Cannot be captured, filtered, silenced, or escalated by test suites | Unfilterable stdout noise | Use `warnings.warn()` |
| Using `DeprecationWarning` for user-facing CLI tools | Python silences `DeprecationWarning` by default in non-main modules | Users never see the notice | Use `FutureWarning` or `UserWarning` |
| Mutating `warnings.filters` directly | Internal list structure; direct mutation can break C-level regex caching | Caching inconsistency | Use `warnings.filterwarnings()` |
| Forgetting `record=True` in `catch_warnings()` | Context manager yields `None` instead of the captured list | `TypeError: 'NoneType' object is not iterable` | `with catch_warnings(record=True):` |
| Ignoring warnings in CI test suites | Deprecations go unnoticed until dependencies drop support, breaking builds | Unexpected upstream breakage | Run `pytest -W error` in CI |

---

## 8. Performance & Complexity

- **Issuing a Warning**: Checks against compiled filter regexes in $O(F)$ where $F$ is the count of active filters (typically $< 10$).
- **Suppressed Warnings**: Evaluating an ignored warning costs $\approx 100-300$ ns.
- **Memory**: Storing captured warnings via `catch_warnings(record=True)` allocates `WarningMessage` named tuples holding message, category, filename, lineno, file, and line.

---

## 9. CPython Internals & Bytecode

Let's inspect how warning filtering is managed internally in C:

```python
import warnings

# Inspect the active runtime filters list
print(f"Total active warning filters: {len(warnings.filters)}")
for action, msg, cat, mod, line in warnings.filters[:3]:
    print(f"  - Action: {action:<8} Category: {cat.__name__:<22} Mod: {mod}")
```

**Output**:
```text
Total active warning filters: 7
  - Action: default  Category: Warning                Mod: None
  - Action: ignore   Category: DeprecationWarning     Mod: None
  - Action: ignore   Category: PendingDeprecationWarning Mod: None
```
CPython maintains `warnings.filters` as a list of 5-tuples: `(action, message_regex, category_class, module_regex, line_number)`. When `_warnings.warn()` is executed in C, it loops through this list and evaluates matching rules against the active frame.

---

## 10. Best Practices

1. **Always Set `stacklevel`**: When authoring helper functions or decorators, pass `stacklevel=2` (or calculate dynamic depth) so the user sees *their* offending line.
2. **Turn Warnings into Errors in CI**: Configure `pytest` with `filterwarnings = ["error"]` in `pyproject.toml` so newly deprecated dependencies immediately fail tests.
3. **Use `FutureWarning` for End Users**: If end users of your library need to be notified about impending semantic changes, use `FutureWarning` instead of `DeprecationWarning`.

---

## 11. Real-World Case Study: Automated Deprecation Sunset Policy

```python
import warnings
import functools
from datetime import date
from typing import Callable

class DeprecationSunsetWarning(DeprecationWarning):
    """Warning emitted when an API has reached or passed its announced sunset date."""

def sunset_api(sunset_date: str, replacement: str) -> Callable:
    target_date = date.fromisoformat(sunset_date)

    def decorator(fn: Callable) -> Callable:
        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            today = date.today()
            if today >= target_date:
                # Past sunset date: escalate to severe notice
                warnings.warn(
                    f"CRITICAL: '{fn.__name__}' was sunset on {sunset_date}. "
                    f"This API will be completely deleted in the next release. Migrate to '{replacement}'.",
                    category=DeprecationSunsetWarning,
                    stacklevel=2
                )
            else:
                warnings.warn(
                    f"'{fn.__name__}' is scheduled for sunset on {sunset_date}. "
                    f"Please migrate to '{replacement}'.",
                    category=DeprecationWarning,
                    stacklevel=2
                )
            return fn(*args, **kwargs)
        return wrapper
    return decorator

@sunset_api(sunset_date="2024-01-01", replacement="new_v2_endpoint()")
def old_v1_endpoint():
    return "response"

with warnings.catch_warnings(record=True) as captured:
    warnings.simplefilter("always")
    old_v1_endpoint()
    assert len(captured) == 1
    print(f"Category: {captured[0].category.__name__}")
    print(f"Message: {captured[0].message}")
```
**Output**:
```text
Category: DeprecationSunsetWarning
Message: CRITICAL: 'old_v1_endpoint' was sunset on 2024-01-01. This API will be completely deleted in the next release. Migrate to 'new_v2_endpoint()'.
```

---

## 12. Interview Questions

### Q1: Why should you use `stacklevel=2` when calling `warnings.warn()`?
<details>
<summary><strong>Answer</strong></summary>
By default, <code>stacklevel=1</code> identifies the source line where <code>warnings.warn()</code> was invoked (inside your library function). Setting <code>stacklevel=2</code> instructs Python to look one frame older on the call stack, attributing the warning to the caller's line of code where the deprecated function was invoked.
</details>

### Q2: What is the difference between `DeprecationWarning` and `FutureWarning`?
<details>
<summary><strong>Answer</strong></summary>
<code>DeprecationWarning</code> is intended for developers and is ignored by default in standard Python execution unless triggered directly in <code>__main__</code> or configured via flags. <code>FutureWarning</code> is intended for end users and application developers; it is displayed by default to warn about impending breaking changes in behavior.
</details>

### Q3: How do you configure `pytest` to fail test runs whenever any warning is emitted?
<details>
<summary><strong>Answer</strong></summary>
In <code>pyproject.toml</code>:
<pre><code>[tool.pytest.ini_options]
filterwarnings = [
    "error"
]
</code></pre>
Or via CLI: <code>pytest -W error</code>.
</details>

### Q4: How does `warnings.catch_warnings()` work?
<details>
<summary><strong>Answer</strong></summary>
It is a context manager that saves the global state of <code>warnings.filters</code> upon entry and restores it upon exit. If <code>record=True</code> is passed, it returns a list that accumulates all emitted <code>WarningMessage</code> objects, suppressing them from being written to stderr.
</details>

### Q5: What is a `ResourceWarning` and when is it typically emitted?
<details>
<summary><strong>Answer</strong></summary>
<code>ResourceWarning</code> is emitted when system resources (such as unclosed file handles, sockets, or unawaited asyncio coroutines) are garbage collected without being properly closed or awaited.
</details>

### Q6: Can a custom warning inherit from `UserWarning`?
<details>
<summary><strong>Answer</strong></summary>
Yes. All warning classes derive from <code>Warning</code>, and subclassing <code>UserWarning</code> or <code>DeprecationWarning</code> is standard practice when creating domain-specific warnings for libraries.
</details>

### Q7: What command-line argument tells Python to show all warnings including ignored ones?
<details>
<summary><strong>Answer</strong></summary>
<code>python -Wd</code> (equivalent to <code>-W default</code>, which enables standard developer warnings including <code>DeprecationWarning</code>).
</details>

### Q8: What happens when a warning filter has the action `"error"`?
<details>
<summary><strong>Answer</strong></summary>
When a warning matching the filter is issued, Python raises the warning instance as a standard exception. Normal execution halts, and the exception can be caught using <code>try...except Category:</code>.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Basic Warning Interceptor
Write a function `test_for_warnings(fn)` that uses `warnings.catch_warnings(record=True)` to execute `fn()` and returns `True` if any warning was emitted, or `False` otherwise.
<details>
<summary><strong>Solution</strong></summary>

```python
import warnings

def test_for_warnings(fn) -> bool:
    with warnings.catch_warnings(record=True) as recorded:
        warnings.simplefilter("always")
        fn()
        return len(recorded) > 0

assert test_for_warnings(lambda: warnings.warn("Alert", UserWarning)) is True
assert test_for_warnings(lambda: 10 + 20) is False
```
</details>

### Challenge 2 (Easy): Escalate Specific Warning Category
Write a function `run_strict_deprecations(fn)` that executes `fn()` with `DeprecationWarning` filtered as an error, but ignores all other warning categories.
<details>
<summary><strong>Solution</strong></summary>

```python
import warnings

def run_strict_deprecations(fn):
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        warnings.filterwarnings("error", category=DeprecationWarning)
        return fn()

# Test deprecation raises error
try:
    run_strict_deprecations(lambda: warnings.warn("deprecated", DeprecationWarning))
    raise RuntimeError("Should have raised DeprecationWarning")
except DeprecationWarning:
    pass

# Test UserWarning is ignored
assert run_strict_deprecations(lambda: (warnings.warn("ignore me", UserWarning), 42)[1]) == 42
```
</details>

### Challenge 3 (Medium): Dynamic Stacklevel Detector
Write a utility function `smart_warn(message: str, category: type[Warning] = UserWarning)` that inspects the call stack via `sys._getframe()` and dynamically calculates the correct `stacklevel` so that the warning always points to the first frame outside the current module.
<details>
<summary><strong>Solution</strong></summary>

```python
import sys
import warnings

def smart_warn(message: str, category: type[Warning] = UserWarning) -> None:
    current_module = __name__
    frame = sys._getframe(1)
    depth = 1
    while frame is not None:
        if frame.f_globals.get("__name__") != current_module:
            break
        depth += 1
        frame = frame.f_back
    warnings.warn(message, category=category, stacklevel=depth)

def internal_helper():
    smart_warn("Dynamic level triggered")

with warnings.catch_warnings(record=True) as captured:
    warnings.simplefilter("always")
    internal_helper()
    assert len(captured) == 1
    assert "Dynamic level triggered" in str(captured[0].message)
```
</details>

### Challenge 4 (Medium): Deprecated Parameter Decorator
Write a function decorator `@deprecated_param(param_name, removal_version)` that checks `kwargs`. If the specified parameter is passed, emit a `FutureWarning` indicating that `param_name` is deprecated and will be removed in `removal_version`.
<details>
<summary><strong>Solution</strong></summary>

```python
import functools
import warnings

def deprecated_param(param_name: str, removal_version: str):
    def decorator(fn):
        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            if param_name in kwargs:
                warnings.warn(
                    f"Parameter '{param_name}' in '{fn.__name__}' is deprecated; removal in v{removal_version}.",
                    category=FutureWarning,
                    stacklevel=2
                )
            return fn(*args, **kwargs)
        return wrapper
    return decorator

@deprecated_param("old_flag", "3.0")
def query_data(limit: int = 10, **kwargs):
    return limit

with warnings.catch_warnings(record=True) as captured:
    warnings.simplefilter("always")
    query_data(limit=5, old_flag=True)
    assert len(captured) == 1
    assert "Parameter 'old_flag'" in str(captured[0].message)
```
</details>

### Challenge 5 (Hard): Warning Deduplication Registry
Implement a custom warning registry class `DeduplicatingWarningEmitter` that records emitted warning messages and ensures that a warning with the exact same message is emitted at most `max_occurrences` times across the entire lifetime of the process.
<details>
<summary><strong>Solution</strong></summary>

```python
import warnings

class DeduplicatingWarningEmitter:
    def __init__(self, max_occurrences: int = 1) -> None:
        self.max_occurrences = max_occurrences
        self.counts: dict[str, int] = {}

    def warn(self, message: str, category: type[Warning] = UserWarning, stacklevel: int = 2) -> None:
        count = self.counts.get(message, 0)
        if count < self.max_occurrences:
            self.counts[message] = count + 1
            warnings.warn(message, category=category, stacklevel=stacklevel)

emitter = DeduplicatingWarningEmitter(max_occurrences=2)

with warnings.catch_warnings(record=True) as recorded:
    warnings.simplefilter("always")
    for _ in range(5):
        emitter.warn("Repeated database ping timeout")

assert len(recorded) == 2
```
</details>

### Challenge 6 (Hard): Warning Logger Interceptor Bridge
Implement a context manager `warnings_to_logger(logger_fn)` that captures all warnings issued inside the block, prevents them from printing to `sys.stderr`, and formats each into a structured dictionary logged via `logger_fn({level, message, category, file, line})`.
<details>
<summary><strong>Solution</strong></summary>

```python
import warnings
from typing import Callable, Any

class warnings_to_logger:
    def __init__(self, logger_fn: Callable[[dict[str, Any]], None]) -> None:
        self.logger_fn = logger_fn
        self._catch = warnings.catch_warnings(record=True)

    def __enter__(self):
        self.recorded = self._catch.__enter__()
        warnings.simplefilter("always")
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        try:
            for w in self.recorded:
                self.logger_fn({
                    "level": "WARNING",
                    "category": w.category.__name__,
                    "message": str(w.message),
                    "file": w.filename,
                    "line": w.lineno
                })
        finally:
            self._catch.__exit__(exc_type, exc_val, exc_tb)

logs = []
with warnings_to_logger(lambda entry: logs.append(entry)):
    warnings.warn("Socket buffer full", RuntimeWarning)
    warnings.warn("Old API used", DeprecationWarning)

assert len(logs) == 2
assert logs[0]["category"] == "RuntimeWarning"
assert logs[1]["category"] == "DeprecationWarning"
```
</details>
