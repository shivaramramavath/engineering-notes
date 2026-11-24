---
title: "Decorators in Python"
slug: "decorators"
category: "functions"
difficulty: "intermediate-to-advanced"
tags: ["python", "functions", "decorators", "metaprogramming", "functools-wraps", "design-patterns"]
prerequisites: ["00-defining-functions", "01-arguments-positional-keyword-star", "04-closures"]
related_topics: ["07-functools", "05-oop/03-methods-classmethod-staticmethod"]
python_version: "3.8+"
tested_with: "CPython 3.14.0"
free_threading_support: "Yes"
---

# Decorators in Python

## 1. Executive Summary & Core Value Proposition

A **decorator** in Python is a callable that accepts another function or class as an argument, extends or alters its behavior without modifying its source code, and returns the modified or wrapped callable.

Introduced via **PEP 318** (and extended to classes in PEP 3129), the `@` decorator syntax provides clean syntactic sugar for metaprogramming:

```python
@my_decorator
def target_function():
    pass

# Exactly equivalent to:
target_function = my_decorator(target_function)
```

Decorators implement the Gang of Four **Decorator Pattern** natively at the language level. They are the standard mechanism across all modern Python frameworks (FastAPI, Flask, Django, Pytest, Click) for cross-cutting concerns such as:
1. **Logging, Auditing & Profiling:** Tracking function execution times and error rates.
2. **Access Control & Authentication:** Enforcing role-based authorization (RBAC) and validating tokens.
3. **Caching & Memoization:** Intercepting repeated calls with identical parameters.
4. **Resilience & Fault Tolerance:** Automatic retries, circuit breaking, and rate limiting.

---

## 2. Syntax, Mechanics & Visual Architecture

### The Mechanics of Stacking and Execution

When stacking multiple decorators, they are applied **from the bottom up** (inside out), but execute **from the top down**:

```python
@decorator_one
@decorator_two
def original():
    pass

# Syntactic Equivalence:
original = decorator_one(decorator_two(original))
```

```
Execution Pipeline:
Caller Invocations
        |
        v
[ decorator_one Wrapper (Pre) ]
        |
        v
[ decorator_two Wrapper (Pre) ]
        |
        v
[ Original Target Function ]
        |
        v
[ decorator_two Wrapper (Post) ]
        |
        v
[ decorator_one Wrapper (Post) ]
        |
        v
Caller Receives Return Value
```

### Decorator Topologies

| Topology | Signature | Structure | Use Case |
| :--- | :--- | :--- | :--- |
| **Simple Decorator** | `@deco` | 2-level nesting: `deco(fn) -> wrapper` | Logging, timing, caching without options |
| **Parameterized Decorator** | `@deco(arg=val)` | 3-level nesting: `deco(arg) -> deco(fn) -> wrapper` | Thresholds, roles, retry counts |
| **Class-Based Decorator** | `@DecoClass` | Class with `__call__(self, *args, **kwargs)` | Stateful tracking, complex lifecycles |

---

## 3. Real-World Code Examples

### Example 1: Execution Timing with `@functools.wraps`

Without `@functools.wraps(fn)`, the wrapped function loses its original `__name__`, `__doc__`, and `__annotations__`, appearing in debuggers and logs as `"wrapper"`.

```python
import functools
import time
from typing import Callable, Any

def timer(fn: Callable) -> Callable:
    """Measures and logs function execution duration."""
    @functools.wraps(fn)  # Preserves metadata of original fn
    def wrapper(*args: Any, **kwargs: Any) -> Any:
        start_time = time.perf_counter()
        result = fn(*args, **kwargs)
        elapsed = time.perf_counter() - start_time
        print(f"[TIMER] '{fn.__name__}' completed in {elapsed*1e6:.2f} microseconds")
        return result
    return wrapper

@timer
def calculate_matrix_checksum(size: int) -> int:
    """Calculates sum of integer grid."""
    return sum(i * j for i in range(size) for j in range(size))

res = calculate_matrix_checksum(100)
print(f"Result: {res}")
print(f"Introspected function name: {calculate_matrix_checksum.__name__}")
print(f"Introspected docstring    : {calculate_matrix_checksum.__doc__.strip()}")
```

**Output:**
```
[TIMER] 'calculate_matrix_checksum' completed in 412.50 microseconds
Result: 24502500
Introspected function name: calculate_matrix_checksum
Introspected docstring    : Calculates sum of integer grid.
```

---

### Example 2: Parameterized Decorator Factory (Retry with Exponential Backoff)

To pass arguments to a decorator (`@retry(max_attempts=3, backoff=0.1)`), write a function that returns a decorator.

```python
import functools
import time
from typing import Callable, Any

def retry(max_attempts: int = 3, initial_delay: float = 0.01) -> Callable:
    """Decorator factory configuring retry limits."""
    def decorator(fn: Callable) -> Callable:
        @functools.wraps(fn)
        def wrapper(*args: Any, **kwargs: Any) -> Any:
            delay = initial_delay
            last_exception = None
            
            for attempt in range(1, max_attempts + 1):
                try:
                    return fn(*args, **kwargs)
                except Exception as err:
                    last_exception = err
                    print(f"Attempt {attempt}/{max_attempts} failed: {err}. Retrying in {delay*1000:.0f}ms...")
                    time.sleep(delay)
                    delay *= 2  # Exponential backoff
                    
            print(f"All {max_attempts} attempts failed.")
            raise last_exception
        return wrapper
    return decorator

# Simulate flaky network call
attempts_counter = 0

@retry(max_attempts=3, initial_delay=0.02)
def fetch_remote_resource(resource_id: str) -> str:
    global attempts_counter
    attempts_counter += 1
    if attempts_counter < 3:
        raise ConnectionResetError("Connection dropped by peer")
    return f"Resource '{resource_id}' payload successfully retrieved"

print("Final response:", fetch_remote_resource("payload-99"))
```

**Output:**
```
Attempt 1/3 failed: Connection dropped by peer. Retrying in 20ms...
Attempt 2/3 failed: Connection dropped by peer. Retrying in 40ms...
Final response: Resource 'payload-99' payload successfully retrieved
```

---

### Example 3: Role-Based Access Control (RBAC) Decorator

```python
import functools
from typing import Callable, Any

CURRENT_USER = {"username": "shiva_ram", "roles": {"engineer", "operator"}}

def require_roles(*required_roles: str) -> Callable:
    def decorator(fn: Callable) -> Callable:
        @functools.wraps(fn)
        def wrapper(*args: Any, **kwargs: Any) -> Any:
            user_roles = CURRENT_USER.get("roles", set())
            missing_roles = set(required_roles) - user_roles
            
            if missing_roles:
                raise PermissionError(
                    f"Access denied for user '{CURRENT_USER['username']}'. "
                    f"Missing roles: {missing_roles}"
                )
            return fn(*args, **kwargs)
        return wrapper
    return decorator

@require_roles("operator")
def restart_service(service_name: str) -> str:
    return f"Service '{service_name}' restarted successfully."

@require_roles("admin")
def purge_database() -> str:
    return "Database purged."

print(restart_service("nginx"))

try:
    purge_database()
except PermissionError as err:
    print("Caught authorization failure:", err)
```

**Output:**
```
Service 'nginx' restarted successfully.
Caught authorization failure: Access denied for user 'shiva_ram'. Missing roles: {'admin'}
```

---

### Example 4: Class-Based Decorator with State Tracking

```python
import functools
from typing import Any, Callable

class CallCounter:
    """Class-based decorator tracking invocation frequency across targets."""
    def __init__(self, fn: Callable):
        self.fn = fn
        self.count = 0
        functools.update_wrapper(self, fn)

    def __call__(self, *args: Any, **kwargs: Any) -> Any:
        self.count += 1
        print(f"[COUNTER] {self.fn.__name__} called {self.count} times")
        return self.fn(*args, **kwargs)

@CallCounter
def process_message(msg: str) -> str:
    return f"Processed: {msg}"

process_message("Alpha")
process_message("Beta")
process_message("Gamma")
print(f"Total count recorded on object: {process_message.count}")
```

**Output:**
```
[COUNTER] process_message called 1 times
[COUNTER] process_message called 2 times
[COUNTER] process_message called 3 times
Total count recorded on object: 3
```

---

### Example 5: Class Decorator (Enforcing Class Invariants)

Decorators can also decorate **classes** (`class Cls: pass`), modifying the class definition before it is instantiated.

```python
def auto_repr(cls):
    """Class decorator dynamically injecting a standardized __repr__ method."""
    def __repr__(self):
        attrs = ", ".join(f"{k}={v!r}" for k, v in self.__dict__.items())
        return f"{cls.__name__}({attrs})"
        
    cls.__repr__ = __repr__
    return cls

@auto_repr
class MicroserviceConfig:
    def __init__(self, host: str, port: int, debug: bool):
        self.host = host
        self.port = port
        self.debug = debug

cfg = MicroserviceConfig("api.internal", 443, False)
print("Auto-injected repr:", cfg)
```

**Output:**
```
Auto-injected repr: MicroserviceConfig(host='api.internal', port=443, debug=False)
```

---

## 4. Common Anti-Patterns & Mistakes

| Anti-Pattern / Mistake | Broken / Unidiomatic Snippet | Idiomatic Correction | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Omitting `@functools.wraps`** | Defining wrapper without wraps | `@functools.wraps(fn)` on wrapper | Erases function `__name__`, docstrings, and breaks Pytest / Sphinx introspection. |
| **Forgetting parens on factory** | `@retry def f(): ...` | `@retry() def f(): ...` | Decorator factory expects arguments; `fn` binds to `max_attempts` instead of function! |
| **Decorating methods with class decorator** | Using class decorator on instance methods | Bind via descriptor `__get__` | Standard class `__call__` does not pass `self` instance, raising argument errors. |
| **Changing function return value silently** | Decorator prints but forgets `return result` | Always capture and return result | Silently swallows target function return, yielding `None`. |
| **Hardcoding wrapper arguments** | `def wrapper(a, b):` | `def wrapper(*args, **kwargs):` | Prevents the decorator from wrapping functions with different signatures. |
| **Executing at import time** | Heavy I/O or network calls in decorator body | Move logic inside `wrapper` | Decorators execute at module load time; slows startup and breaks tests. |

---

## 5. Architectural & Design Decisions

### Decorators vs Middleware
- **Decorators:** Applied at the function or class level. Ideal for fine-grained, localized concerns (e.g. `@require_roles("admin")` on a specific endpoint, `@lru_cache` on a calculation).
- **Middleware:** Applied at the pipeline/layer level (e.g. Starlette/FastAPI HTTP middleware). Ideal for universal concerns that touch every single request (e.g. CORS headers, request ID tracing).

### Accessing the Original Unwrapped Function
When `@functools.wraps` is used, the wrapper gains a `__wrapped__` attribute pointing back to the original unwrapped target:

```python
original_func = decorated_func.__wrapped__
```
This is critical for unit testing, allowing developers to test business logic in isolation without triggering timing, authentication, or rate limiting wrappers.

---

## 6. Verification & Practical Testing

```python
import functools

def test_wraps_metadata_preservation():
    def simple_deco(fn):
        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            return fn(*args, **kwargs)
        return wrapper
        
    @simple_deco
    def target(x: int) -> int:
        """Target docstring."""
        return x * 10
        
    assert target(5) == 50
    assert target.__name__ == "target"
    assert target.__doc__ == "Target docstring."
    assert target.__wrapped__(5) == 50

def test_stacking_order():
    trace = []
    def deco_a(fn):
        def wrapper():
            trace.append("A_pre")
            fn()
            trace.append("A_post")
        return wrapper
        
    def deco_b(fn):
        def wrapper():
            trace.append("B_pre")
            fn()
            trace.append("B_post")
        return wrapper
        
    @deco_a
    @deco_b
    def core():
        trace.append("core")
        
    core()
    assert trace == ["A_pre", "B_pre", "core", "B_post", "A_post"]

if __name__ == "__main__":
    test_wraps_metadata_preservation()
    test_stacking_order()
    print("All decorator tests passed successfully.")
```

**Output:**
```
All decorator tests passed successfully.
```

---

## 7. Deep Dive: Supporting Both `@deco` and `@deco()`

A professional design pattern allows a decorator to be used **both with and without arguments**:

```python
import functools
from typing import Callable, Any

def smart_deco(fn: Callable | None = None, *, prefix: str = "[LOG]") -> Callable:
    def decorator(func: Callable) -> Callable:
        @functools.wraps(func)
        def wrapper(*args: Any, **kwargs: Any) -> Any:
            print(f"{prefix} Executing {func.__name__}")
            return func(*args, **kwargs)
        return wrapper
        
    if fn is None:
        # Called with arguments: @smart_deco(prefix="[CUSTOM]")
        return decorator
    else:
        # Called without arguments: @smart_deco
        return decorator(fn)

@smart_deco
def action1(): pass

@smart_deco(prefix="[AUDIT]")
def action2(): pass

action1()
action2()
```

**Output:**
```
[LOG] Executing action1
[AUDIT] Executing action2
```

---

## 8. Summary & Key Takeaways

1. **Syntactic Sugar:** `@deco` translates directly to `target = deco(target)`.
2. **Metadata Integrity:** Always use `@functools.wraps(fn)` on inner wrappers.
3. **Execution Order:** Stacked decorators evaluate bottom-up at definition time, and execute top-down at call time.
4. **Parameterized Decorators:** Require 3 levels of function nesting (factory $\rightarrow$ decorator $\rightarrow$ wrapper).
5. **Testing Access:** The original function remains accessible via `decorated_fn.__wrapped__`.

---

## 9. Frequently Asked Questions (Interview & Senior Roles)

<details>
<summary>1. When does decorator code execute: at definition time or call time?</summary>

The outer decorator function executes **at definition time** (when the module is imported and the `def` statement runs). The inner `wrapper` function executes **at call time** (every time the decorated function is invoked).
</details>

<details>
<summary>2. Why is `@functools.wraps` essential in production decorators?</summary>

Without `@functools.wraps`, the wrapped function's `__name__`, `__doc__`, `__module__`, and `__annotations__` are replaced by the wrapper function's attributes. This breaks debugging stack traces, Sphinx documentation generation, IDE autocompletion, and framework routers (like FastAPI or Flask) that rely on function names and type hints.
</details>

<details>
<summary>3. In what order do stacked decorators execute?</summary>

Decorators are applied from the bottom up (closest to the `def` first: `f = deco1(deco2(f))`). However, when `f()` is invoked, the outermost wrapper (`deco1`) executes first, delegates to `deco2`, which then calls the original function.
</details>

<details>
<summary>4. How can you write a decorator that accepts optional parameters?</summary>

Check if the first argument is callable. If it is callable, the decorator was used bare (`@deco`), so return `decorator(first_arg)`. If it is `None` or a non-callable, the decorator was called with arguments (`@deco(key=val)`), so return the inner `decorator` function.
</details>

<details>
<summary>5. How does decorating an instance method with a class-based decorator differ from decorating a normal function?</summary>

A class implementing `__call__(self, *args, **kwargs)` works as a decorator for standalone functions. But when applied to an instance method, it does not implement the descriptor protocol (`__get__`), meaning Python will not automatically bind the instance (`self`) to method calls. To support methods, the decorator class must implement `__get__(self, instance, owner)`.
</details>

<details>
<summary>6. How can you access the original undecorated function?</summary>

If the decorator used `@functools.wraps`, the original function is exposed through the `__wrapped__` attribute: `target.__wrapped__`.
</details>

<details>
<summary>7. Can you decorate a class? What does a class decorator return?</summary>

Yes (`@class_decorator`). A class decorator receives the class object itself (`cls`), can add methods, modify class attributes, or wrap `__init__`, and must return a class object (either the modified original class or a new subclass).
</details>

<details>
<summary>8. How do decorators behave in free-threaded Python (PEP 703)?</summary>

The decorator wrapping itself occurs at import time and is thread-safe. However, if the `wrapper` function reads or mutates shared mutable state across calls without locks (e.g. a shared call counter or cache), concurrent threads will trigger race conditions.
</details>

---

## 10. Practical Exercises & Self-Assessment

<details>
<summary>Exercise 1: Input Type Validation Decorator</summary>

**Task:** Write a decorator `@enforce_types` that verifies positional arguments match the function's type annotations, raising `TypeError` on mismatch.

**Solution:**
```python
import functools
import inspect

def enforce_types(fn):
    sig = inspect.signature(fn)
    @functools.wraps(fn)
    def wrapper(*args, **kwargs):
        bound = sig.bind(*args, **kwargs)
        for name, val in bound.arguments.items():
            expected = sig.parameters[name].annotation
            if expected is not inspect.Parameter.empty:
                if not isinstance(val, expected):
                    raise TypeError(f"Arg '{name}' must be {expected.__name__}, got {type(val).__name__}")
        return fn(*args, **kwargs)
    return wrapper

@enforce_types
def repeat(s: str, n: int) -> str:
    return s * n

print(repeat("Hi! ", 3))
try:
    repeat(123, 3)
except TypeError as e:
    print("Caught validation error:", e)
```
</details>

<details>
<summary>Exercise 2: Rate Limiting Decorator</summary>

**Task:** Write a decorator `@rate_limited(calls_per_sec=2)` that raises `RuntimeError` if called more frequently than allowed.

**Solution:**
```python
import functools
import time

def rate_limited(calls_per_sec: int):
    min_interval = 1.0 / calls_per_sec
    last_call = 0.0

    def decorator(fn):
        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            nonlocal last_call
            now = time.time()
            if now - last_call < min_interval:
                raise RuntimeError("Rate limit exceeded")
            last_call = now
            return fn(*args, **kwargs)
        return wrapper
    return decorator

@rate_limited(calls_per_sec=2)
def ping(): return "PONG"

print(ping())
try:
    print(ping())  # Immediate call fails
except RuntimeError as e:
    print("Caught rate limit:", e)
```
</details>

<details>
<summary>Exercise 3: Result Caching with Max-Age Expiration</summary>

**Task:** Implement `@timed_cache(seconds=60)` that caches the return value and recomputes only after expiration.

**Solution:**
```python
import functools
import time

def timed_cache(seconds: float):
    def decorator(fn):
        cache = {}
        @functools.wraps(fn)
        def wrapper(*args):
            now = time.time()
            if args in cache:
                val, timestamp = cache[args]
                if now - timestamp < seconds:
                    return val
            result = fn(*args)
            cache[args] = (result, now)
            return result
        return wrapper
    return decorator

@timed_cache(seconds=0.1)
def get_sensor(): return time.time()

v1 = get_sensor()
time.sleep(0.05)
assert get_sensor() == v1  # Cached!
time.sleep(0.06)
assert get_sensor() != v1  # Expired and recomputed!
```
</details>

<details>
<summary>Exercise 4: Class Method Compatible Descriptor Decorator</summary>

**Task:** Implement a class-based decorator `Logged` that works correctly on both standalone functions and instance methods.

**Solution:**
```python
import functools
import types

class Logged:
    def __init__(self, fn):
        self.fn = fn
        functools.update_wrapper(self, fn)

    def __call__(self, *args, **kwargs):
        print(f"Calling {self.fn.__name__}")
        return self.fn(*args, **kwargs)

    def __get__(self, instance, owner):
        # Implement descriptor protocol to bind instance methods!
        if instance is None:
            return self
        return types.MethodType(self, instance)

class Service:
    @Logged
    def action(self): return "Service OK"

s = Service()
print(s.action())
```
</details>

<details>
<summary>Exercise 5: Deprecation Warning Decorator</summary>

**Task:** Create a `@deprecated("Use bar() instead")` decorator that emits a `DeprecationWarning` when called.

**Solution:**
```python
import functools
import warnings

def deprecated(message: str):
    def decorator(fn):
        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            warnings.warn(f"'{fn.__name__}' is deprecated: {message}", category=DeprecationWarning, stacklevel=2)
            return fn(*args, **kwargs)
        return wrapper
    return decorator

@deprecated("Use calculate_v2() instead")
def legacy_calc(x): return x * 2

with warnings.catch_warnings(record=True) as w:
    warnings.simplefilter("always")
    res = legacy_calc(5)
    print("Warning caught:", w[-1].message)
```
</details>

<details>
<summary>Exercise 6: Mutate Class with Class Decorator</summary>

**Task:** Write a class decorator `@singleton` that ensures only one instance of the decorated class ever exists.

**Solution:**
```python
def singleton(cls):
    instances = {}
    @functools.wraps(cls)
    def get_instance(*args, **kwargs):
        if cls not in instances:
            instances[cls] = cls(*args, **kwargs)
        return instances[cls]
    return get_instance

@singleton
class DatabasePool:
    def __init__(self):
        self.connected = True

p1 = DatabasePool()
p2 = DatabasePool()
assert p1 is p2
print("Singleton verified: p1 is p2!")
```
</details>
