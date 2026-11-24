---
title: "Default Argument Pitfalls in Python (The Mutable Default Trap)"
slug: "default-argument-pitfalls"
category: "functions"
difficulty: "intermediate"
tags: ["python", "functions", "default-arguments", "mutable-defaults", "gotchas", "antipatterns"]
prerequisites: ["00-defining-functions", "01-arguments-positional-keyword-star"]
related_topics: ["03-scope-legb-global-nonlocal", "04-closures"]
python_version: "3.8+"
tested_with: "CPython 3.14.0"
free_threading_support: "Yes"
---

# Default Argument Pitfalls in Python (The Mutable Default Trap)

## 1. Executive Summary & Core Value Proposition

The **mutable default argument trap** is one of the most infamous and prolific sources of bugs in Python programming. It occurs when a mutable container (such as a `list`, `dict`, or `set`) is used as a default parameter value in a function definition:

```python
def append_item(element, target=[]):  # DANGEROUS!
    target.append(element)
    return target
```

In Python, default parameter expressions are evaluated **once at definition time** (when the `def` statement executes and the module loads), **not at invocation time** (when the function is called). The evaluated object is stored in the function object's `__defaults__` tuple on the heap.

Every subsequent call that omits the parameter reuses the **exact same mutable heap instance**, resulting in persistent state leakage across invocations.

Understanding this mechanism—and applying the canonical `None` sentinel idiom—is essential for writing safe, robust Python code.

---

## 2. Syntax, Mechanics & Visual Architecture

### Why Definition-Time Evaluation Occurs

When CPython executes a `def` statement:
1. It evaluates all default argument expressions from left to right.
2. It packs the resulting objects into an immutable tuple stored in `func.__defaults__` (or `func.__kwdefaults__`).
3. When the function is invoked, if a caller omits an argument, Python binds the local variable directly to the pointer in `func.__defaults__`.

```
Definition Time (Module Load):
+-------------------------------+
| def append_item(val, target=[]) |
+-------------------------------+
                |
                v
  Evaluates '[]' ONCE on heap:  Address 0x7FFF0010
  Stores pointer in:            append_item.__defaults__ = ([...],)

Invocation Time:
Call 1: append_item("A")  --> Modifies object at 0x7FFF0010: ["A"]
Call 2: append_item("B")  --> Modifies object at 0x7FFF0010: ["A", "B"]!
Call 3: append_item("C")  --> Modifies object at 0x7FFF0010: ["A", "B", "C"]!
```

### The Canonical Idiom: Sentinel `None`

```python
def append_item(element, target=None):
    if target is None:
        target = []  # Allocated freshly on EVERY invocation!
    target.append(element)
    return target
```

---

## 3. Real-World Code Examples

### Example 1: Demonstrating the Mutable Default Trap & `__defaults__`

```python
def broken_accumulator(val: int, history: list[int] = []) -> list[int]:
    history.append(val)
    return history

print("Call 1:", broken_accumulator(10))
print("Call 2:", broken_accumulator(20))
print("Call 3:", broken_accumulator(30))

# Inspect function's internal __defaults__ attribute
print("\nInternal __defaults__ state:", broken_accumulator.__defaults__)
print("Memory address of default:", id(broken_accumulator.__defaults__[0]))
```

**Output:**
```
Call 1: [10]
Call 2: [10, 20]
Call 3: [10, 20, 30]

Internal __defaults__ state: ([10, 20, 30],)
Memory address of default: 2184594248464
```

---

### Example 2: The Canonical `None` Sentinel Pattern

```python
from typing import Sequence

def safe_accumulator(val: int, history: list[int] | None = None) -> list[int]:
    """Canonical production pattern: freshly allocates list if None."""
    if history is None:
        history = []
    history.append(val)
    return history

print("Call 1:", safe_accumulator(10))
print("Call 2:", safe_accumulator(20))
print("Call 3:", safe_accumulator(30))

# Custom user list passed explicitly
custom_list = [100, 200]
safe_accumulator(300, custom_list)
print("Explicit custom list:", custom_list)
```

**Output:**
```
Call 1: [10]
Call 2: [20]
Call 3: [30]
Explicit custom list: [100, 200, 300]
```

---

### Example 3: The Frozen Dynamic Default Trap (`datetime.now()`)

A second major manifestation of definition-time evaluation occurs with dynamic computations (e.g. timestamps or random numbers).

```python
import datetime
import time

# BROKEN: datetime.now() evaluated ONCE when module loads!
def log_event_broken(message: str, timestamp: datetime.datetime = datetime.datetime.now()) -> str:
    return f"[{timestamp.strftime('%H:%M:%S.%f')}] {message}"

print(log_event_broken("Initial startup"))
time.sleep(0.05)
print(log_event_broken("Database connected"))
time.sleep(0.05)
print(log_event_broken("Server listening"))
```

**Output:**
```
[10:14:02.124500] Initial startup
[10:14:02.124500] Database connected
[10:14:02.124500] Server listening
```
*(Notice all timestamps are strictly identical!)*

```python
# CORRECT: Compute timestamp dynamically inside function
def log_event_safe(message: str, timestamp: datetime.datetime | None = None) -> str:
    if timestamp is None:
        timestamp = datetime.datetime.now()
    return f"[{timestamp.strftime('%H:%M:%S.%f')}] {message}"

print(log_event_safe("Initial startup"))
time.sleep(0.05)
print(log_event_safe("Database connected"))
```

**Output:**
```
[10:14:02.124500] Initial startup
[10:14:02.174820] Database connected
```

---

### Example 4: Distinguishing `None` from Omitted Arguments (Custom Sentinels)

When `None` is itself a valid, intentional argument value that a caller might pass, a custom sentinel object is required.

```python
# Create unique object sentinel
_UNSET = object()

def update_user_profile(user_id: int, email: str | None = _UNSET) -> dict:
    profile = {"user_id": user_id, "email": "original@example.com"}
    
    if email is _UNSET:
        print("Email parameter omitted; keeping existing value.")
    elif email is None:
        print("Email explicitly set to None; clearing email record.")
        profile["email"] = None
    else:
        print(f"Updating email to: {email}")
        profile["email"] = email
        
    return profile

# Case 1: Omitted
update_user_profile(101)

# Case 2: Explicitly cleared
update_user_profile(101, email=None)

# Case 3: Updated
update_user_profile(101, email="shiva@example.com")
```

**Output:**
```
Email parameter omitted; keeping existing value.
Email explicitly set to None; clearing email record.
Updating email to: shiva@example.com
```

---

### Example 5: Legitimate Use Cases for Mutable Defaults (Caching / Static State)

In rare advanced patterns, mutable default parameters are intentionally exploited to store function-local static state or lightweight memoization caches without global variables.

```python
from typing import Callable

# Intentionally exploit mutable default dictionary as a fast memoization cache
def fibonacci_memoized(n: int, _cache: dict[int, int] = {0: 0, 1: 1}) -> int:
    """Computes Fibonacci with self-contained, persistent cache."""
    if n not in _cache:
        _cache[n] = fibonacci_memoized(n - 1) + fibonacci_memoized(n - 2)
    return _cache[n]

print("Fib(10):", fibonacci_memoized(10))
print("Fib(30):", fibonacci_memoized(30))
print("Cache entries stored:", len(fibonacci_memoized.__defaults__[0]))
```

**Output:**
```
Fib(10): 55
Fib(30): 832040
Cache entries stored: 31
```

---

## 4. Common Anti-Patterns & Mistakes

| Anti-Pattern / Mistake | Broken / Unidiomatic Snippet | Idiomatic Correction | Why It Fails |
| :--- | :--- | :--- | :--- |
| **List as default** | `def f(items=[])` | `def f(items=None): if items is None: items = []` | Reuses the same list across all invocations that omit `items`. |
| **Dict as default** | `def f(config={})` | `def f(config=None): if config is None: config = {}` | Mutating `config` leaks state to subsequent calls. |
| **Dynamic time as default** | `def log(t=time.time())` | `def log(t=None): if t is None: t = time.time()` | Freezes timestamp to module import time. |
| **Checking truthiness instead of identity** | `if not items: items = []` | `if items is None: items = []` | Falsely replaces explicitly passed empty containers (`[]`, `{}`) or `0` with new objects. |
| **Random generator as default** | `def f(seed=random.randint(1, 100))` | Generate inside function body | Every call without `seed` receives the exact same random integer. |
| **Mutable set as default** | `def collect(seen=set())` | `seen=None` | Retains previously seen elements across independent calls. |

---

## 5. Architectural & Design Decisions

### Why Did Python Design Defaults This Way?
1. **Performance:** Default expressions are evaluated only once when loading the function, avoiding repeated object allocation and evaluation overhead on every single function call.
2. **Determinism:** If defaults were evaluated at call time, functions that reference variables in their default expressions would be vulnerable to dynamic scope pollution and difficult-to-trace bugs.

### Static Analysis Detection (Ruff & Flake8)
Modern linters flag this pattern immediately:
- **Ruff rule `B006`:** *Do not use mutable data structures for argument defaults.*
- **Ruff rule `B008`:** *Do not perform function calls in argument defaults.*

---

## 6. Verification & Practical Testing

```python
def test_mutable_leak():
    def leak(x, container=[]):
        container.append(x)
        return container
        
    assert leak(1) == [1]
    assert leak(2) == [1, 2]  # State leaked!

def test_safe_none_pattern():
    def safe(x, container=None):
        if container is None:
            container = []
        container.append(x)
        return container
        
    assert safe(1) == [1]
    assert safe(2) == [2]     # Fresh container!

def test_custom_sentinel():
    MISSING = object()
    def check(val=MISSING):
        return val is MISSING
    assert check() is True
    assert check(None) is False

if __name__ == "__main__":
    test_mutable_leak()
    test_safe_none_pattern()
    test_custom_sentinel()
    print("All default argument pitfall tests passed successfully.")
```

**Output:**
```
All default argument pitfall tests passed successfully.
```

---

## 7. Deep Dive: Modifying `func.__defaults__` Directly at Runtime

Because Python is dynamic, `__defaults__` is a mutable attribute of the function object. You can inspect or even hot-patch default values at runtime:

```python
def greet(name, greeting="Hello"):
    return f"{greeting}, {name}!"

print("Original default:", greet("Alice"))

# Hot-patching function default
greet.__defaults__ = ("Greetings and salutations",)
print("Patched default :", greet("Alice"))
```

**Output:**
```
Original default: Hello, Alice!
Patched default : Greetings and salutations, Alice!
```

---

## 8. Summary & Key Takeaways

1. **Definition-Time Rule:** Default argument expressions execute once when the function is defined, not when called.
2. **The Leak:** Using mutable objects (`[]`, `{}`) as defaults persists modifications across calls.
3. **The Fix:** Default to `None`, then assign a fresh instance inside the function body (`if target is None: target = []`).
4. **Sentinels:** Use `_UNSET = object()` when `None` is a meaningful value that callers might explicitly pass.
5. **Tooling:** Enable Ruff / Flake8 rules `B006` and `B008` in CI/CD to prevent mutable defaults from entering codebases.

---

## 9. Frequently Asked Questions (Interview & Senior Roles)

<details>
<summary>1. Why does Python evaluate default arguments at definition time instead of call time?</summary>

For performance and simplicity. Python avoids re-evaluating expressions and re-resolving variable names on every function call. Evaluating once at definition time compiles the defaults into a static tuple (`__defaults__`), making call-time argument binding fast and predictable.
</details>

<details>
<summary>2. Why is <code>if not arg: arg = []</code> considered an anti-pattern when fixing mutable defaults?</summary>

Because empty containers (`[]`, `()`, `{}`), empty strings (`""`), `0`, and `False` are all falsy in Python. If a caller explicitly passes an empty list intending to populate it, `if not arg:` will discard the caller's list and instantiate a new one, breaking the caller's reference. Always check explicitly: `if arg is None:`.
</details>

<details>
<summary>3. Where are default argument objects stored in memory?</summary>

Positional defaults are stored in the function object's `__defaults__` tuple. Keyword-only defaults are stored in the function object's `__kwdefaults__` dictionary.
</details>

<details>
<summary>4. What happens when you use an immutable default like an integer or string?</summary>

Because integers, strings, and tuples of immutables cannot be modified in-place, reusing the same instance across calls is completely safe. Any operation on them creates a new object rather than mutating the default stored in `__defaults__`.
</details>

<details>
<summary>5. How can you define a parameter where `None` is a valid input distinct from "omitted"?</summary>

Create a unique sentinel using `_SENTINEL = object()`. Set the parameter default to `_SENTINEL`. Inside the function, check `if param is _SENTINEL:` to detect whether the argument was omitted, allowing `param is None` to be handled as an explicit input.
</details>

<details>
<summary>6. How does FastAPI use default values with `Depends()` and `Query()`?</summary>

FastAPI leverages definition-time evaluation deliberately: `param: int = Query(...)` or `db: Session = Depends(get_db)`. FastAPI introspects the function's signature at startup and uses these default object instances as dependency injection descriptors, replacing them at request time with resolved values.
</details>

<details>
<summary>7. Can you inspect default arguments using the standard library `inspect` module?</summary>

Yes. Using `sig = inspect.signature(func)`, you can iterate over `sig.parameters.items()` and check `param.default`. If a parameter has no default, `param.default` is equal to `inspect.Parameter.empty`.
</details>

<details>
<summary>8. How does free-threaded Python (PEP 703) impact mutable defaults?</summary>

In free-threaded Python (no-GIL), multiple threads calling a function with a mutable default will read and mutate the shared `__defaults__` object concurrently. This creates severe race conditions and data corruption, making the mutable default antipattern even more dangerous in multithreaded environments.
</details>

---

## 10. Practical Exercises & Self-Assessment

<details>
<summary>Exercise 1: Refactor Flawed Class Method</summary>

**Task:** Identify and fix the bug in this class definition:
```python
class TeamRegistry:
    def __init__(self, members: list[str] = []):
        self.members = members
```

**Solution:**
```python
class TeamRegistry:
    def __init__(self, members: list[str] | None = None):
        self.members = [] if members is None else members

t1 = TeamRegistry()
t1.members.append("Alice")
t2 = TeamRegistry()
print(t2.members)  # [] (Fixed! t1 and t2 do not share members)
```
</details>

<details>
<summary>Exercise 2: Dynamic Expiration Calculator</summary>

**Task:** Write a function `create_session(user_id: str, expires_at: datetime = ...)` that defaults to 1 hour from current execution time without freezing the timestamp at import.

**Solution:**
```python
import datetime

def create_session(user_id: str, expires_at: datetime.datetime | None = None) -> dict:
    if expires_at is None:
        expires_at = datetime.datetime.now() + datetime.timedelta(hours=1)
    return {"user_id": user_id, "expires_at": expires_at}
```
</details>

<details>
<summary>Exercise 3: Custom Sentinel Nullable Cache</summary>

**Task:** Implement a function `cache_set(key: str, value: Any = _UNSET)` where passing `value=None` stores `None` as a valid cached value, but omitting `value` raises `ValueError`.

**Solution:**
```python
_UNSET = object()
cache = {}

def cache_set(key: str, value: object = _UNSET):
    if value is _UNSET:
        raise ValueError("Must provide value argument")
    cache[key] = value

cache_set("key1", None)
print(cache)  # {'key1': None} (Stored correctly!)
```
</details>

<details>
<summary>Exercise 4: Reset Corrupted Defaults</summary>

**Task:** Given a corrupted function whose `__defaults__` contains an accumulated list `[1, 2, 3]`, write a utility function to reset its default argument back to an empty list `[]`.

**Solution:**
```python
def corrupted_fn(x, store=[]):
    store.append(x)
    return store

corrupted_fn(1); corrupted_fn(2)
print("Before reset:", corrupted_fn.__defaults__)

# Reset the internal default list in-place
corrupted_fn.__defaults__[0].clear()
print("After reset :", corrupted_fn.__defaults__)
```
</details>

<details>
<summary>Exercise 5: Static Call Counter via Mutable Default</summary>

**Task:** Create a function `call_tracker(_state=[0])` that returns how many times it has been called without using any global variables or classes.

**Solution:**
```python
def call_tracker(_state=[0]) -> int:
    _state[0] += 1
    return _state[0]

print(call_tracker())  # 1
print(call_tracker())  # 2
print(call_tracker())  # 3
```
</details>

<details>
<summary>Exercise 6: Ruff Linter Simulation</summary>

**Task:** Write an introspection function `audit_function_defaults(fn)` that inspects a function's `__defaults__` and prints a warning if any default is a mutable container (`list`, `dict`, `set`).

**Solution:**
```python
def audit_function_defaults(fn):
    if not fn.__defaults__:
        return
    for idx, default_val in enumerate(fn.__defaults__):
        if isinstance(default_val, (list, dict, set)):
            print(f"SECURITY WARNING: Function '{fn.__name__}' parameter default #{idx} is mutable ({type(default_val).__name__})!")

def bad_example(a, b=[]): pass
audit_function_defaults(bad_example)
```
</details>
