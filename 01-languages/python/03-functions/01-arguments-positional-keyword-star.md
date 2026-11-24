---
title: "Function Arguments in Python (Positional, Keyword, *args & **kwargs)"
slug: "arguments-positional-keyword-star"
category: "functions"
difficulty: "beginner-to-intermediate"
tags: ["python", "functions", "arguments", "parameters", "args", "kwargs", "call-by-sharing"]
prerequisites: ["00-defining-functions", "02-data-structures/05-unpacking"]
related_topics: ["02-default-argument-pitfalls", "10-positional-only-and-keyword-only", "06-decorators"]
python_version: "3.8+"
tested_with: "CPython 3.14.0"
free_threading_support: "Yes"
---

# Function Arguments in Python (Positional, Keyword, *args & **kwargs)

## 1. Executive Summary & Core Value Proposition

Python's parameter passing evaluation strategy is formally known as **call-by-sharing** (or *call-by-object-reference*). Function parameters receive copies of the caller's object pointers:
- Rebinding a parameter name inside the function (`x = new_val`) alters only the local reference frame, leaving the caller's variable unchanged.
- Modifying a mutable object in-place through a parameter (`x.append(item)`) mutates the shared underlying heap object, directly affecting the caller.

To support expressive, flexible APIs, Python provides a rich argument syntax combining:
1. **Positional Arguments:** Matched by offset order.
2. **Keyword Arguments:** Matched by parameter name, independent of order.
3. **Arbitrary Positional Arguments (`*args`):** Captures excess positional values into an immutable `tuple`.
4. **Arbitrary Keyword Arguments (`**kwargs`):** Captures excess named arguments into a mutable `dict`.

---

## 2. Syntax, Mechanics & Visual Architecture

### Canonical Parameter Declaration Order

Python enforces a strict syntactic order for declaring parameters in a function signature:

```
Formal Signature Order:
def func(
    pos_only, /,          # 1. Positional-only parameters (optional)
    standard_args,        # 2. Standard positional-or-keyword parameters
    *args,                # 3. Var-positional arguments (packed into tuple)
    kw_only,              # 4. Keyword-only parameters (optional)
    **kwargs              # 5. Var-keyword arguments (packed into dict)
):
```

Violating this order (such as placing positional parameters after `*args` without keyword-only rules, or placing arguments after `**kwargs`) raises a compile-time `SyntaxError`.

### Call-by-Sharing Visual Architecture

```
Caller Scope:                     Function Local Scope:
+---------------+                 +---------------+
| my_list       | -----\   /----> | param         |
+---------------+       \ /       +---------------+
                         v
             Heap Object: [ 10, 20 ]
             
Scenario A: In-Place Mutation
  param.append(30)  --> Mutates HEAP object directly. Both scopes see [10, 20, 30]!

Scenario B: Rebinding
  param = [99]      --> Rebinds local pointer. Caller's 'my_list' remains [10, 20]!
```

---

## 3. Real-World Code Examples

### Example 1: Call-by-Sharing In-Place Mutation vs Rebinding

```python
def mutate_and_rebind(shared_list: list[int], rebind_candidate: list[int]) -> None:
    # 1. In-place mutation modifies heap object directly
    shared_list.append(999)
    
    # 2. Rebinding modifies only local pointer
    rebind_candidate = [100, 200, 300]
    print("Inside function rebind_candidate:", rebind_candidate)

caller_a = [1, 2, 3]
caller_b = ["alpha", "beta"]

mutate_and_rebind(caller_a, caller_b)

print("Caller A after call (mutated) :", caller_a)
print("Caller B after call (unaltered):", caller_b)
```

**Output:**
```
Inside function rebind_candidate: [100, 200, 300]
Caller A after call (mutated) : [1, 2, 3, 999]
Caller B after call (unaltered): ['alpha', 'beta']
```

---

### Example 2: Flexible Variadic Calculations with `*args`

```python
def compute_weighted_average(base_weight: float, *scores: float) -> float:
    """Computes average where each score in *scores is weighted by base_weight."""
    print(f"Captured *scores tuple: {scores} (Type: {type(scores).__name__})")
    if not scores:
        return 0.0
    weighted_sum = sum(s * base_weight for s in scores)
    return round(weighted_sum / len(scores), 2)

res1 = compute_weighted_average(1.5, 80.0, 90.0, 85.0)
print("Weighted average 1:", res1)

# Unpacking a caller sequence into *scores
grade_list = [70.0, 75.0, 80.0]
res2 = compute_weighted_average(1.0, *grade_list)
print("Weighted average 2:", res2)
```

**Output:**
```
Captured *scores tuple: (80.0, 90.0, 85.0) (Type: tuple)
Weighted average 1: 127.5
Captured *scores tuple: (70.0, 75.0, 80.0) (Type: tuple)
Weighted average 2: 75.0
```

---

### Example 3: Dynamic Extensible Options with `**kwargs`

```python
def build_database_uri(engine: str, host: str, port: int, **options: str | int | bool) -> str:
    """Constructs database connection string with arbitrary query options."""
    base_uri = f"{engine}://{host}:{port}"
    if options:
        # Build query string
        query_parts = [f"{k}={v}" for k, v in options.items()]
        query_string = "&".join(query_parts)
        return f"{base_uri}?{query_string}"
    return base_uri

# Standard call with explicit kwargs
uri_simple = build_database_uri("postgresql", "localhost", 5432)
print("Simple URI  :", uri_simple)

# Call with dynamic options
uri_complex = build_database_uri(
    "postgresql", "db.internal.net", 5432,
    sslmode="require",
    pool_size=20,
    connect_timeout=10
)
print("Complex URI :", uri_complex)
```

**Output:**
```
Simple URI  : postgresql://localhost:5432
Complex URI : postgresql://db.internal.net:5432?sslmode=require&pool_size=20&connect_timeout=10
```

---

### Example 4: Full Spectrum Signature Order

```python
def signature_showcase(required_pos: str, default_arg: int = 10, *args: str, tag: str = "DEFAULT", **kwargs: Any) -> None:
    print(f"required_pos: {required_pos}")
    print(f"default_arg : {default_arg}")
    print(f"args        : {args}")
    print(f"tag         : {tag}")
    print(f"kwargs      : {kwargs}")

signature_showcase(
    "Alpha", 42,
    "extra1", "extra2",
    tag="CUSTOM_TAG",
    timeout=5000,
    retries=3
)
```

**Output:**
```
required_pos: Alpha
default_arg : 42
args        : ('extra1', 'extra2')
tag         : CUSTOM_TAG
kwargs      : {'timeout': 5000, 'retries': 3}
```

---

### Example 5: Universal Argument Forwarding / Transparent Proxies

```python
import time
from typing import Callable, Any

def timing_proxy(target_fn: Callable, *args: Any, **kwargs: Any) -> Any:
    """Executes target_fn with arbitrary arguments, reporting execution latency."""
    t0 = time.perf_counter()
    result = target_fn(*args, **kwargs)  # Perfect forwarding
    elapsed = time.perf_counter() - t0
    print(f"[PROXY] '{target_fn.__name__}' completed in {elapsed*1e6:.1f} microseconds")
    return result

def query_service(endpoint: str, timeout: int = 30, retry: bool = True) -> str:
    return f"Response from {endpoint} (timeout={timeout}, retry={retry})"

output = timing_proxy(query_service, "https://api.internal/v1", timeout=15, retry=False)
print("Output:", output)
```

**Output:**
```
[PROXY] 'query_service' completed in 1.2 microseconds
Output: Response from https://api.internal/v1 (timeout=15, retry=False)
```

---

## 4. Common Anti-Patterns & Mistakes

| Anti-Pattern / Mistake | Broken / Unidiomatic Snippet | Idiomatic Correction | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Positional after keyword** | `f(a=1, 2)` | `f(2, a=1)` or `f(1, 2)` | Positional arguments cannot follow keyword arguments in a call (`SyntaxError`). |
| **Multiple values for argument** | `def f(a): ...; f(1, a=2)` | Pass either positionally or by name | Passing both offset and keyword name raises `TypeError: got multiple values for argument`. |
| **Mutating arguments unexpectedly** | `def sort_users(u): u.sort()` | `return sorted(u)` | In-place mutation unexpectedly corrupts caller's data collection. |
| **Declaring `**kwargs` before `*args`** | `def f(**kw, *a):` | `def f(*a, **kw):` | `**kwargs` must strictly appear as the final parameter. |
| **Modifying `*args`** | `args[0] = 99` | Convert to list: `lst = list(args)` | `*args` is packed as an immutable `tuple`. |
| **Passing non-mapping to `**kwargs`** | `f(**[1, 2])` | `f(*[1, 2])` | `**` requires key-value mapping; sequence raises `TypeError`. |

---

## 5. Architectural & Design Decisions

### Call-by-Sharing vs Pass-by-Value vs Pass-by-Reference
- **Pass-by-Value (C / Go):** Memory bytes are duplicated. Modifying data in the function never touches caller state.
- **Pass-by-Reference (C++ `&`):** The parameter is an alias for the caller's variable. Rebinding the parameter rebinds the caller's variable in place.
- **Call-by-Sharing (Python / Java / JS):** Pointers to heap objects are passed by value. Rebinding the parameter only changes local pointer destination, but mutating the referenced object modifies the shared heap state.

### Why `*args` is a Tuple and `**kwargs` is a Dict
- `*args` is an immutable `tuple` because positional arguments are inherently sequential, ordered, fixed in size for the call, and benefit from tuple free-list allocation speed.
- `**kwargs` is a mutable `dict` because named parameters are associative mappings where subsequent functions or decorators may need to inspect, modify, or inject additional configuration flags before forwarding.

---

## 6. Verification & Practical Testing

```python
def test_args_kwargs_packing():
    def introspect(*args, **kwargs):
        return type(args), type(kwargs), len(args), len(kwargs)
        
    t_args, t_kw, n_args, n_kw = introspect(1, 2, 3, a="x", b="y")
    assert t_args is tuple
    assert t_kw is dict
    assert n_args == 3
    assert n_kw == 2

def test_multiple_values_error():
    def sample(x): return x
    try:
        sample(10, x=20)
        assert False, "Should have raised TypeError"
    except TypeError:
        pass

def test_forwarding():
    def add(a, b): return a + b
    def forwarder(fn, *a, **kw): return fn(*a, **kw)
    assert forwarder(add, 10, b=20) == 30

if __name__ == "__main__":
    test_args_kwargs_packing()
    test_multiple_values_error()
    test_forwarding()
    print("All argument mechanics tests passed successfully.")
```

**Output:**
```
All argument mechanics tests passed successfully.
```

---

## 7. Deep Dive: Bytecode Argument Passing (`CALL` vs `KW_NAMES`)

In Python 3.11+, keyword argument passing was dramatically overhauled via the `KW_NAMES` opcode:

```python
import dis

def caller():
    f(1, timeout=5, retries=3)

dis.dis(caller)
```

Bytecode disassembly:
1. `LOAD_GLOBAL (f)`
2. `LOAD_CONST (1)`
3. `LOAD_CONST (5)`
4. `LOAD_CONST (3)`
5. `KW_NAMES (('timeout', 'retries'))`: Pushes an immutable tuple of keyword names onto the stack.
6. `CALL 1`: Evaluates the call.

Instead of building a temporary dictionary on every function call with keyword arguments, CPython passes keyword values directly on the stack accompanied by a static tuple of keys, avoiding dictionary allocation entirely for standard calls!

---

## 8. Summary & Key Takeaways

1. **Evaluation Model:** Call-by-sharing; mutating mutables affects caller, rebinding does not.
2. **Packing:** `*args` packs excess positionals into a `tuple`; `**kwargs` packs excess keywords into a `dict`.
3. **Ordering:** Order must strictly follow `(pos, *args, kw_only, **kwargs)`.
4. **Forwarding:** Combine `*args, **kwargs` with unpacking `func(*args, **kwargs)` for universal proxy wrappers.
5. **Bytecode:** Python 3.11+ uses `KW_NAMES` to pass keyword arguments on the stack without dictionary allocation.

---

## 9. Frequently Asked Questions (Interview & Senior Roles)

<details>
<summary>1. Is Python pass-by-value or pass-by-reference?</summary>

Neither in the traditional sense; Python is **call-by-sharing** (or pass-by-object-reference). When calling a function, object pointers are passed by value. This means rebinding the argument name inside the function does not change the caller's variable, but mutating a mutable object in-place through that pointer alters the shared heap object visible to the caller.
</details>

<details>
<summary>2. Why does <code>def func(a, b=2, c):</code> raise a SyntaxError?</summary>

Parameters without defaults cannot follow parameters with defaults. If this were permitted, calling `func(1, 3)` would be ambiguous: is `3` intended for `b` or `c`? Non-default arguments must always precede default arguments.
</details>

<details>
<summary>3. Can a parameter appear in both `*args` and `**kwargs`?</summary>

No. `*args` captures all excess positional arguments, and `**kwargs` captures all excess keyword arguments. If a caller supplies an argument positionally and by keyword name simultaneously (e.g. `f(1, a=1)`), Python raises `TypeError: got multiple values for argument`.
</details>

<details>
<summary>4. What is the performance overhead of using `*args` and `**kwargs` in performance-critical code?</summary>

In Python 3.11+, calls without `*args`/`**kwargs` execute via specialized fast-paths directly on the evaluation stack. Using `*args` requires packing elements into a tuple, and using `**kwargs` requires allocating a dictionary, adding slight allocation and unpack overhead.
</details>

<details>
<summary>5. How can you define a parameter that can ONLY be passed as a keyword?</summary>

Place it after `*args` or after a bare asterisk `*` in the parameter list: `def func(a, *, timeout=30):`. `timeout` can now only be passed by name (`func(1, timeout=60)`), not positionally.
</details>

<details>
<summary>6. What happens if a function definition contains two `*args` parameters?</summary>

It raises a compile-time `SyntaxError: * argument may appear only once`.
</details>

<details>
<summary>7. How does Python unpack dictionaries when using `func(**data)`?</summary>

Python iterates through the dictionary's keys and values, verifies that all keys are strings (raising `TypeError: keywords must be strings` if not), and matches them against the function's parameter names.
</details>

<details>
<summary>8. How does free-threaded Python (PEP 703) handle mutable argument modifications?</summary>

Because arguments are shared heap references, modifying a mutable argument (such as a shared `list` or `dict`) across multiple threads without synchronization creates data races. Applications must use explicit locks (e.g. `threading.Lock`) when passing mutable state to concurrent functions.
</details>

---

## 10. Practical Exercises & Self-Assessment

<details>
<summary>Exercise 1: Dynamic Mathematical Summation Wrapper</summary>

**Task:** Write a function `dynamic_sum(*numbers, multiplier: float = 1.0) -> float` that accepts any number of positional arguments and multiplies the total sum by `multiplier`.

**Solution:**
```python
def dynamic_sum(*numbers: float, multiplier: float = 1.0) -> float:
    return sum(numbers) * multiplier

print(dynamic_sum(1, 2, 3, 4, multiplier=2.5))  # 25.0
print(dynamic_sum(10, 20))                      # 30.0
```
</details>

<details>
<summary>Exercise 2: Strict Dictionary Config Validator</summary>

**Task:** Write a function `configure_server(**kwargs)` that accepts only `port` (int) and `host` (str), raising `ValueError` if any unrecognized keyword arguments are provided.

**Solution:**
```python
def configure_server(**kwargs):
    allowed = {"port", "host"}
    unknown = kwargs.keys() - allowed
    if unknown:
        raise ValueError(f"Unknown configuration keys: {unknown}")
    print(f"Configuring server: {kwargs.get('host', 'localhost')}:{kwargs.get('port', 8080)}")

configure_server(host="0.0.0.0", port=9000)
try:
    configure_server(host="localhost", bad_option=True)
except ValueError as e:
    print("Caught validation error:", e)
```
</details>

<details>
<summary>Exercise 3: Transparent Retry Decorator with Full Forwarding</summary>

**Task:** Write a decorator `retry_thrice(fn)` that uses `*args, **kwargs` to retry a failing function up to 3 times before raising the exception.

**Solution:**
```python
def retry_thrice(fn):
    def wrapper(*args, **kwargs):
        last_err = None
        for attempt in range(1, 4):
            try:
                return fn(*args, **kwargs)
            except Exception as err:
                last_err = err
                print(f"Attempt {attempt} failed: {err}")
        raise last_err
    return wrapper

@retry_thrice
def flaky_calc(x):
    if x < 0: raise ValueError("Negative!")
    return x * 10

print("Result:", flaky_calc(5))
```
</details>

<details>
<summary>Exercise 4: Merge Variadic Keyword Dictionaries</summary>

**Task:** Write a function that accepts multiple dictionaries via `*dicts` and merges them into a single dictionary using unpacking.

**Solution:**
```python
def merge_all_dicts(*dicts: dict) -> dict:
    merged = {}
    for d in dicts:
        merged.update(d)
    return merged

d1 = {"a": 1}
d2 = {"b": 2}
d3 = {"a": 99, "c": 3}
print(merge_all_dicts(d1, d2, d3))  # {'a': 99, 'b': 2, 'c': 3}
```
</details>

<details>
<summary>Exercise 5: Call Signature Inspector</summary>

**Task:** Use `inspect.signature` to print all parameter names, kinds, and defaults for a given function.

**Solution:**
```python
import inspect

def sample(a, b=10, *args, c=20, **kwargs): pass

sig = inspect.signature(sample)
for name, param in sig.parameters.items():
    print(f"Param: {name:<8} | Kind: {param.kind.name:<20} | Default: {param.default}")
```
</details>

<details>
<summary>Exercise 6: Diagnose In-Place Mutation Trap</summary>

**Task:** Refactor the following function to prevent accidental mutation of the caller's input list:
```python
def clean_tags(tags: list[str]) -> list[str]:
    for i in range(len(tags)):
        tags[i] = tags[i].strip().lower()
    return tags
```

**Solution:**
```python
def clean_tags(tags: list[str]) -> list[str]:
    # Return new list to preserve caller's collection
    return [t.strip().lower() for t in tags]

raw = ["  PYTHON ", "Docker  "]
cleaned = clean_tags(raw)
assert raw == ["  PYTHON ", "Docker  "]  # Caller unaffected!
```
</details>
