# Module 07: Exceptions, Context Managers & Debugging (02-raise-and-exception-chaining)

> **Topic Overview**: Exception chaining (PEP 3134) enables Python developers to translate low-level technical errors into rich domain-level abstractions without losing diagnostic context. Understanding the distinction between explicit causes (`__cause__`) and implicit contexts (`__context__`) is crucial for clean error logs and debugging complex distributed systems.

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

- **Problem Statement**: When an application layer catches a low-level error (e.g., `sqlite3.OperationalError` or `ConnectionRefusedError`) and raises a domain-level error (e.g., `DatabaseUnavailableError`), naive re-raising either discards the original root cause or presents confusing, interleaved tracebacks.
- **Mental Model**:
  - **Implicit Chaining (`__context__`)**: "An error occurred *while* another error was being handled." (Accidental secondary bug).
  - **Explicit Chaining (`__cause__`)**: "This high-level error was caused *directly by* that low-level error." (`raise NewError from original_error`).
  - **Context Suppression (`from None`)**: "This low-level error is an implementation detail; hide it and only show the clean domain error." (`raise NewError from None`).
- **When to Use**:
  - Always use `raise CustomError from original_error` when translating error types across architectural boundaries (e.g., Repository $\to$ Service layer).
  - Use `raise CustomError from None` when transforming internal parser exceptions into user-facing clean validation messages.
- **When NOT to Use**: Do not use `from None` when diagnosing internal development bugs, as it destroys the causality chain.

---

## 2. Core Concepts

### Implicit vs Explicit Chaining Semantics

```text
+-----------------------------------------------------------------------------------+
| Case A: Implicit Chaining (Accidental Bug)                                        |
| try: 1 / 0                                                                        |
| except ZeroDivisionError:                                                         |
|     bad_var.do_something()  --> raises AttributeError                             |
|                                                                                   |
| Result: AttributeError.__context__ = ZeroDivisionError                           |
| Traceback Header: "During handling of the above exception, another exception      |
|                    occurred:"                                                     |
+-----------------------------------------------------------------------------------+

+-----------------------------------------------------------------------------------+
| Case B: Explicit Chaining (Intentional Translation)                               |
| try: int("invalid")                                                               |
| except ValueError as exc:                                                         |
|     raise DomainError("Invalid integer input") from exc                           |
|                                                                                   |
| Result: DomainError.__cause__ = ValueError                                        |
|         DomainError.__suppress_context__ = True                                   |
| Traceback Header: "The above exception was the direct cause of the following      |
|                    exception:"                                                    |
+-----------------------------------------------------------------------------------+

+-----------------------------------------------------------------------------------+
| Case C: Explicit Suppression (Hide Implementation Details)                        |
| try: config["secret_token"]                                                       |
| except KeyError:                                                                  |
|     raise AuthenticationFailed("Missing secret token") from None                  |
|                                                                                   |
| Result: AuthenticationFailed.__cause__ = None                                     |
|         AuthenticationFailed.__suppress_context__ = True                          |
| Traceback: Only AuthenticationFailed is printed; KeyError is omitted.             |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

CPython manages exception chaining through internal object pointer attributes on `PyBaseExceptionObject`:

```text
+----------------------------------------------------+
| PyBaseExceptionObject (DomainError)               |
+----------------------------------------------------+
| ob_refcnt : 1                                      |
| ob_type   : &DomainError_Type                      |
| args      : ("Database connection failed",)        |
| traceback : <TracebackObject at 0x7f01>            |
| cause     : &PyBaseExceptionObject (OSError) ----+ |
| context   : &PyBaseExceptionObject (OSError)     | |
| suppress_context : 1 (True)                      | |
+--------------------------------------------------|-+
                                                   |
             +-------------------------------------+
             v
+----------------------------------------------------+
| PyBaseExceptionObject (OSError)                    |
+----------------------------------------------------+
| args      : (111, "Connection refused")            |
| traceback : <TracebackObject at 0x7f00>            |
| cause     : NULL                                   |
| context   : NULL                                   |
| suppress_context : 0 (False)                       |
+----------------------------------------------------+
```

When rendering the traceback, `PyTraceBack_Print` inspects `__suppress_context__`:
- If `__cause__` is set, it prints the cause first followed by `"The above exception was the direct cause..."`.
- If `__cause__` is `None` and `__suppress_context__` is `True`, it halts traversal immediately.
- If `__suppress_context__` is `False`, it recursively traverses `__context__` and prints `"During handling of the above exception..."`.

---

## 4. Syntax & API Reference

### Chaining Operators & Attributes
```python
# 1. Explicit chaining
raise NewException("message") from original_exception

# 2. Suppressing context
raise NewException("message") from None

# 3. Bare re-raise (preserves exact traceback)
try:
    risky()
except SpecificError:
    audit_log()
    raise  # Re-raises the active exception unchanged
```

### Introspecting Chained Attributes
```python
try:
    ...
except Exception as e:
    print(e.__cause__)              # Explicit cause or None
    print(e.__context__)            # Implicit context or None
    print(e.__suppress_context__)   # bool (True if 'from' syntax was used)
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Explicit Chaining
```python
class PaymentError(Exception):
    pass

def process_charge(amount: int) -> None:
    try:
        if amount <= 0:
            raise ValueError("Amount must be positive")
    except ValueError as err:
        raise PaymentError("Unable to complete payment transaction") from err

try:
    process_charge(-50)
except PaymentError as exc:
    print(f"Top-level: {exc}")
    print(f"Direct cause: {exc.__cause__}")
    print(f"Suppress context flag: {exc.__suppress_context__}")
```
**Output**:
```text
Top-level: Unable to complete payment transaction
Direct cause: Amount must be positive
Suppress context flag: True
```

### 2. Real-World Production Example: Safe Repository Abstraction
```python
import json

class DatabaseRecordNotFound(Exception):
    """Domain exception raised when record does not exist."""

class UserRepository:
    def __init__(self, raw_store: dict[str, str]) -> None:
        self._store = raw_store

    def get_user_profile(self, user_id: str) -> dict:
        try:
            raw_json = self._store[user_id]
        except KeyError as err:
            # Mask storage implementation; reveal clean domain exception
            raise DatabaseRecordNotFound(f"User {user_id} does not exist in store") from err

        try:
            return json.loads(raw_json)
        except json.JSONDecodeError as err:
            # Explicit cause reveals data corruption origin
            raise DatabaseRecordNotFound(f"User {user_id} has corrupted profile data") from err

repo = UserRepository({"u_1": '{"name": "Alice"}', "u_2": 'INVALID_JSON'})

# Test missing key
try:
    repo.get_user_profile("u_99")
except DatabaseRecordNotFound as e:
    print(f"Caught: {e} (Caused by: {type(e.__cause__).__name__})")

# Test corrupted JSON
try:
    repo.get_user_profile("u_2")
except DatabaseRecordNotFound as e:
    print(f"Caught: {e} (Caused by: {type(e.__cause__).__name__})")
```
**Output**:
```text
Caught: User u_99 does not exist in store (Caused by: KeyError)
Caught: User u_2 has corrupted profile data (Caused by: JSONDecodeError)
```

### 3. Edge Case: Context Suppression with `from None`
```python
class SensitiveKeyError(Exception):
    pass

def authenticate(credentials: dict[str, str]) -> str:
    try:
        return credentials["auth_token"]
    except KeyError:
        # Suppress KeyError to prevent revealing dictionary key names in tracebacks
        raise SensitiveKeyError("Authentication token absent from request") from None

try:
    authenticate({})
except SensitiveKeyError as err:
    print(f"Exception message: {err}")
    print(f"Cause is None: {err.__cause__ is None}")
    print(f"Context suppressed: {err.__suppress_context__}")
```
**Output**:
```text
Exception message: Authentication token absent from request
Cause is None: True
Context suppressed: True
```

### 4. Failure Case & Exception Handling: Accidental Implicit Chaining Bug
```python
def format_error(msg: str) -> str:
    # Buggy formatter referencing undefined variable
    return msg + suffix  # type: ignore[name-defined]

def risky_step() -> None:
    try:
        raise ValueError("Invalid configuration setting")
    except ValueError as exc:
        try:
            format_error(str(exc))
        except NameError as bug:
            print(f"Handled secondary bug: {bug}")
            print(f"Implicit context: {bug.__context__}")

risky_step()
```
**Output**:
```text
Handled secondary bug: name 'suffix' is not defined
Implicit context: Invalid configuration setting
```

### 5. Idiomatic Pythonic Pattern: Unwinding an Entire Exception Cause Chain
```python
def get_root_cause(exc: BaseException) -> BaseException:
    """Traverse the cause/context chain to identify the initial root trigger."""
    current = exc
    while True:
        next_cause = current.__cause__ or (current.__context__ if not current.__suppress_context__ else None)
        if next_cause is None:
            return current
        current = next_cause

def pipeline_step3():
    raise ConnectionResetError("Socket reset by peer")

def pipeline_step2():
    try:
        pipeline_step3()
    except OSError as e:
        raise RuntimeError("Service gateway failed") from e

def pipeline_step1():
    try:
        pipeline_step2()
    except RuntimeError as e:
        raise ValueError("High-level API workflow aborted") from e

try:
    pipeline_step1()
except ValueError as e:
    root = get_root_cause(e)
    print(f"Top-level error: {type(e).__name__} -> {e}")
    print(f"Root cause error: {type(root).__name__} -> {root}")
```
**Output**:
```text
Top-level error: ValueError -> High-level API workflow aborted
Root cause error: ConnectionResetError -> Socket reset by peer
```

---

## 6. Edge Cases & Pitfalls

1. **Cycle in Exception Chaining**:
   - Manually setting `e1.__cause__ = e2` and `e2.__cause__ = e1` creates an infinite loop. Python's traceback printer detects cycles in `__cause__` and `__context__` and breaks them cleanly with a `[... cycle detected ...]` note.
2. **Re-raising After Handling**:
   - Writing `raise exc` inside an `except SomeError as exc:` block causes `exc.__context__` to be bound to whatever previous exception was active. Always use a bare `raise` if re-raising without modification.
3. **`from None` Does Not Delete `__context__`**:
   - `raise X from None` sets `__cause__ = None` and sets `__suppress_context__ = True`. However, `X.__context__` still references the original exception in memory! It is simply suppressed during traceback formatting.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| `raise NewError(str(old_error))` without `from` | Destroys the original traceback, making debugging impossible | Missing root cause context | `raise NewError(...) from old_error` |
| `raise old_error` instead of bare `raise` | Resets the starting traceback frame of `old_error` to the re-raise line | Distorted traceback stack origin | Use bare `raise` |
| Using `from None` in internal libraries | Conceals internal system errors from developers trying to diagnose root causes | Debugging blind spots | Reserve `from None` for clean public APIs |
| `raise e from None` where `e` is an uninstantiated type | `from` requires an instance or `None` | `TypeError: exception causes must derive from BaseException` | `raise NewError() from None` |
| Mutating `e.__cause__` to a non-exception | Python validates that `__cause__` is an instance of `BaseException` or `None` | `TypeError: exception cause must be an exception` | Only assign `BaseException` or `None` |
| Forgetting `from None` when sanitizing errors | Leaks underlying database schemas or private file paths to client tracebacks | Information disclosure vulnerability | `raise ClientError(...) from None` |

---

## 8. Performance & Complexity

- **Time Complexity**: Setting `__cause__` or `__context__` is an $O(1)$ pointer assignment on the C-level `PyBaseExceptionObject`.
- **Traceback Traversal**: Formatting a chained exception is $O(d)$ where $d$ is the depth of chained exceptions (typically $< 5$).
- **Memory Impact**: Because `__context__` and `__cause__` retain strong references to previous exception instances—and those instances reference tracebacks and frame objects—long-lived chained exceptions can keep large local variable graphs alive in memory until the exception itself is garbage collected.

---

## 9. CPython Internals & Bytecode

Let's inspect how the `RAISE_VARARGS` instruction handles the `from` clause:

```python
import dis

def explicit_chain(e1, e2):
    raise e1 from e2

dis.dis(explicit_chain)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 LOAD_FAST                0 (e1)
              4 LOAD_FAST                1 (e2)
              6 RAISE_VARARGS            2
```

Notice `RAISE_VARARGS 2`:
- `RAISE_VARARGS 0`: Bare `raise` (re-raise current exception).
- `RAISE_VARARGS 1`: `raise e` (raise single exception without explicit cause).
- `RAISE_VARARGS 2`: `raise e1 from e2` (pop cause from stack, set `e1.__cause__ = e2`, set `e1.__suppress_context__ = 1`, and raise).

---

## 10. Best Practices

1. **Boundary Translation**: When writing an API or library, translate internal dependencies (HTTP, database, file system) into well-defined library exceptions using `raise LibraryError(...) from original_error`.
2. **Sanitize Public Errors**: When building user-facing web services or CLI tools, use `from None` to hide internal stack traces from end-user output.
3. **Check Root Causes**: In monitoring or telemetry interceptors, always traverse `__cause__` and `__context__` to extract the true underlying failure reason.

---

## 11. Real-World Case Study: Resilient Microservice HTTP Client

```python
import http.client
from typing import Any

class ServiceUnavailable(Exception):
    """Raised when external downstream service cannot be reached."""

class APIClient:
    def __init__(self, host: str) -> None:
        self.host = host

    def get_data(self, endpoint: str) -> str:
        conn = None
        try:
            conn = http.client.HTTPConnection(self.host, timeout=0.1)
            conn.request("GET", endpoint)
            response = conn.getresponse()
            if response.status >= 500:
                raise http.client.HTTPException(f"HTTP Server error: {response.status}")
            return response.read().decode()
        except OSError as network_err:
            # Low-level network socket error translated to ServiceUnavailable
            raise ServiceUnavailable(f"Network transport failed to {self.host}") from network_err
        except http.client.HTTPException as protocol_err:
            # Protocol-level error translated to ServiceUnavailable
            raise ServiceUnavailable(f"Remote service returned protocol error") from protocol_err
        finally:
            if conn:
                conn.close()

client = APIClient("127.0.0.1:65534")  # Closed port
try:
    client.get_data("/api/v1/users")
except ServiceUnavailable as err:
    print(f"Domain Error: {err}")
    print(f"Root Cause: {type(err.__cause__).__name__} -> {err.__cause__}")
```
**Output**:
```text
Domain Error: Network transport failed to 127.0.0.1:65534
Root Cause: ConnectionRefusedError -> [Errno 111] Connection refused
```

---

## 12. Interview Questions

### Q1: What is the exact difference between `__cause__` and `__context__`?
<details>
<summary><strong>Answer</strong></summary>
<code>__cause__</code> is populated explicitly using the <code>raise ... from ...</code> syntax to denote intentional translation. <code>__context__</code> is populated automatically by CPython whenever an exception is raised while another exception is actively being handled in an <code>except</code> or <code>finally</code> block.
</details>

### Q2: What does `raise SomeError from None` do under the hood?
<details>
<summary><strong>Answer</strong></summary>
It explicitly sets <code>SomeError.__cause__ = None</code> and sets the internal flag <code>SomeError.__suppress_context__ = True</code>. When Python displays the traceback, it omits the previously active exception entirely.
</details>

### Q3: Why does `raise e` differ from a bare `raise`?
<details>
<summary><strong>Answer</strong></summary>
A bare <code>raise</code> re-raises the active exception while preserving its exact traceback and origin frame. Writing <code>raise e</code> attaches the current line as a new originating frame in <code>e.__traceback__</code>, obscuring where the exception was actually thrown.
</details>

### Q4: Does `raise SomeError from None` delete the `__context__` attribute?
<details>
<summary><strong>Answer</strong></summary>
No. <code>__context__</code> remains populated with a reference to the previous exception in memory. What changes is <code>__suppress_context__</code>, which is set to <code>True</code>, instructing the traceback formatter to ignore <code>__context__</code> during output.
</details>

### Q5: Can an exception chain have multiple levels?
<details>
<summary><strong>Answer</strong></summary>
Yes. If function A raises E1, function B catches E1 and raises E2 from E1, and function C catches E2 and raises E3 from E2, <code>E3.__cause__</code> is E2, and <code>E2.__cause__</code> is E1. All three will be printed sequentially in the traceback.
</details>

### Q6: What bytecode instruction executes `raise e from cause`?
<details>
<summary><strong>Answer</strong></summary>
<code>RAISE_VARARGS 2</code>. The interpreter pops the cause and the exception from the value stack, sets <code>__cause__</code> and <code>__suppress_context__</code> on the exception object, and initiates unwinding.
</details>

### Q7: What happens if an exception cycle is inadvertently created (`e1.__cause__ = e2; e2.__cause__ = e1`)?
<details>
<summary><strong>Answer</strong></summary>
CPython's traceback rendering routine tracks visited exceptions in a set during formatting. If it encounters a previously visited exception, it outputs <code>[... cycle detected ...]</code> and safely terminates traversal rather than entering an infinite loop.
</details>

### Q8: How can memory leaks occur from chained exceptions?
<details>
<summary><strong>Answer</strong></summary>
Exceptions hold references to tracebacks, tracebacks hold references to execution frames, and frames hold references to all local variables. If a chained exception is stored long-term in an in-memory queue or global cache, all local variables from all frames in the chain remain pinned in memory.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Basic Translation with Cause
Write a function `safe_int_cast(val: str) -> int` that attempts to cast `val` to `int`. If a `ValueError` occurs, raise a custom `ParsingError(f"Cannot parse '{val}'")` explicitly chained from the `ValueError`.
<details>
<summary><strong>Solution</strong></summary>

```python
class ParsingError(Exception):
    pass

def safe_int_cast(val: str) -> int:
    try:
        return int(val)
    except ValueError as err:
        raise ParsingError(f"Cannot parse '{val}'") from err

try:
    safe_int_cast("xyz")
except ParsingError as e:
    assert isinstance(e.__cause__, ValueError)
    assert str(e) == "Cannot parse 'xyz'"
```
</details>

### Challenge 2 (Easy): Context Suppression
Write a function `mask_credentials(d: dict, key: str) -> str` that accesses `d[key]`. If `KeyError` occurs, raise `PermissionError("Access Denied")` with context suppressed via `from None`.
<details>
<summary><strong>Solution</strong></summary>

```python
def mask_credentials(d: dict, key: str) -> str:
    try:
        return d[key]
    except KeyError:
        raise PermissionError("Access Denied") from None

try:
    mask_credentials({}, "secret")
except PermissionError as e:
    assert e.__suppress_context__ is True
    assert e.__cause__ is None
```
</details>

### Challenge 3 (Medium): Full Chain Linearizer
Write a utility function `linearize_chain(exc: BaseException) -> list[BaseException]` that returns a list containing the exception and all of its causes/contexts in order from highest-level to lowest root cause, properly handling cycles.
<details>
<summary><strong>Solution</strong></summary>

```python
def linearize_chain(exc: BaseException) -> list[BaseException]:
    chain = []
    seen = set()
    current: BaseException | None = exc
    while current and id(current) not in seen:
        chain.append(current)
        seen.add(id(current))
        if current.__cause__ is not None:
            current = current.__cause__
        elif not current.__suppress_context__ and current.__context__ is not None:
            current = current.__context__
        else:
            break
    return chain

e1 = ValueError("root")
e2 = RuntimeError("middle")
e2.__cause__ = e1
e3 = Exception("top")
e3.__cause__ = e2

chain = linearize_chain(e3)
assert len(chain) == 3
assert chain[0] is e3
assert chain[1] is e2
assert chain[2] is e1
```
</details>

### Challenge 4 (Medium): Traceback Truncator Decorator
Write a decorator `@sanitize_exceptions(public_exception_cls)` that catches any internal exception raised by the decorated function and re-raises `public_exception_cls(str(err))` chained `from None`, preventing leaking internal paths.
<details>
<summary><strong>Solution</strong></summary>

```python
import functools
from typing import Callable, Type

def sanitize_exceptions(public_exception_cls: Type[Exception]):
    def decorator(fn: Callable):
        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            try:
                return fn(*args, **kwargs)
            except Exception as internal_err:
                raise public_exception_cls(f"Operation failed: {internal_err}") from None
        return wrapper
    return decorator

class PublicAPIError(Exception):
    pass

@sanitize_exceptions(PublicAPIError)
def internal_worker():
    return 1 / 0

try:
    internal_worker()
except PublicAPIError as e:
    assert e.__suppress_context__ is True
    assert "division by zero" in str(e)
```
</details>

### Challenge 5 (Hard): Recursive Chained Exception Serializer
Write a function `serialize_exception_chain(exc: BaseException) -> dict` that converts an exception and its full causal chain into a JSON-serializable dictionary with fields `type`, `message`, `notes`, and recursive `cause`.
<details>
<summary><strong>Solution</strong></summary>

```python
def serialize_exception_chain(exc: BaseException | None) -> dict | None:
    if exc is None:
        return None
    
    notes = getattr(exc, "__notes__", [])
    data = {
        "type": type(exc).__name__,
        "message": str(exc),
        "notes": list(notes),
        "cause": None
    }
    
    next_exc = exc.__cause__ if exc.__cause__ is not None else (
        exc.__context__ if not exc.__suppress_context__ else None
    )
    if next_exc is not None and next_exc is not exc:
        data["cause"] = serialize_exception_chain(next_exc)
        
    return data

try:
    try:
        raise OSError("Disk failure")
    except OSError as err:
        raise RuntimeError("Service corrupted") from err
except RuntimeError as top:
    result = serialize_exception_chain(top)
    assert result["type"] == "RuntimeError"
    assert result["cause"]["type"] == "OSError"
    assert result["cause"]["message"] == "Disk failure"
```
</details>

### Challenge 6 (Hard): Cycle-Safe Error Graph Resolver
Construct a scenario where two exception instances mutually reference each other in their `__context__` or `__cause__`. Write a resolver function `break_exception_cycles(exc: BaseException) -> int` that traverses the chain, detects cycles, severs the loop by setting `__cause__ = None` on the looping link, and returns the total number of severed cycles.
<details>
<summary><strong>Solution</strong></summary>

```python
def break_exception_cycles(exc: BaseException) -> int:
    visited = set()
    cycles_severed = 0
    current: BaseException | None = exc
    
    while current is not None:
        visited.add(id(current))
        next_cause = current.__cause__
        if next_cause is not None:
            if id(next_cause) in visited:
                current.__cause__ = None
                cycles_severed += 1
                break
            current = next_cause
        else:
            break
            
    return cycles_severed

err_a = ValueError("A")
err_b = RuntimeError("B")
err_a.__cause__ = err_b
err_b.__cause__ = err_a  # Mutual cycle created!

severed = break_exception_cycles(err_a)
assert severed == 1
assert err_b.__cause__ is None
```
</details>
