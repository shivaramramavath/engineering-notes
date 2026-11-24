---
title: "Positional-Only (/) and Keyword-Only (*) Parameters in Python"
slug: "positional-only-and-keyword-only"
category: "functions"
difficulty: "intermediate"
tags: ["python", "functions", "positional-only", "keyword-only", "pep-570", "pep-3102", "api-design"]
prerequisites: ["00-defining-functions", "01-arguments-positional-keyword-star"]
related_topics: ["02-default-argument-pitfalls", "09-docstrings-and-annotations"]
python_version: "3.8+"
tested_with: "CPython 3.14.0"
free_threading_support: "Yes"
---

# Positional-Only (`/`) and Keyword-Only (`*`) Parameters in Python

## 1. Executive Summary & Core Value Proposition

In Python, function parameters historically accepted arguments either positionally or by keyword name indiscriminately. While this flexibility is convenient, it creates critical API design hazards:
1. **The Keyword Argument Coupling Hazard:** If a caller uses a parameter's name as a keyword argument (`func(x=10)`), the library author can **never rename** that parameter without breaking existing client code.
2. **The Boolean Flag Trap:** Functions accepting multiple boolean or numeric flags positionally (`process(True, False, 10, True)`) produce unreadable, error-prone call sites.

To solve these architectural challenges, modern Python introduces two boundary markers:
- **Positional-Only Parameters (`/` - PEP 570 in Python 3.8+):** Parameters preceding the forward slash `/` can **only** be supplied by position, never by keyword name.
- **Keyword-Only Parameters (`*` - PEP 3102):** Parameters following the asterisk `*` can **only** be supplied by keyword name, never positionally.

---

## 2. Syntax, Mechanics & Visual Architecture

### The Unified Signature Grammar

```python
def function(
    pos_only_1, pos_only_2, /,    # Must be passed positionally
    pos_or_kw_1, pos_or_kw_2,     # Can be passed positionally OR by keyword
    *,                            # Boundary marker
    kw_only_1, kw_only_2          # Must be passed by keyword name
):
    pass
```

```
Signature Boundary Segmentation:

  def create_endpoint( path, /, method="GET", *, timeout=30, auth=True ):
  |--------------------------|  |-----------|   |-----------------------|
        Positional-Only         Standard (Either)       Keyword-Only
        Must call:              Can call:               Must call:
        ("api/v1")              ("POST")                (timeout=10, auth=False)
        CANNOT call:            OR                      CANNOT call:
        path="api/v1"           method="POST"           10, False
```

### Key Behavioral Invariants

| Marker | Position | Constraint on Preceding / Following Parameters | Caller Error on Violation |
| :--- | :--- | :--- | :--- |
| **`/`** | After positional-only parameters | Everything **before** `/` cannot be called with `name=val` | `TypeError: got some positional-only arguments passed as keyword arguments` |
| **`*`** | Before keyword-only parameters | Everything **after** `*` cannot be called positionally | `TypeError: takes X positional arguments but Y were given` |

---

## 3. Real-World Code Examples

### Example 1: Eliminating the Boolean Flag Anti-Pattern via Keyword-Only Arguments

```python
# UNIDIOMATIC: Call site is completely unreadable
def configure_cache_bad(size: int, evict: bool, compress: bool, log: bool):
    pass

# Idiomatic API design: Enforce keyword-only parameters with '*'
def configure_cache(
    size_mb: int,
    *,
    auto_evict: bool = True,
    compression: bool = False,
    enable_logging: bool = True
) -> dict:
    return {
        "size_mb": size_mb,
        "auto_evict": auto_evict,
        "compression": compression,
        "enable_logging": enable_logging
    }

# Call site is self-documenting and crystal clear:
cache_settings = configure_cache(512, auto_evict=True, compression=True)
print("Configured Cache:", cache_settings)

try:
    # Attempting to pass flags positionally raises TypeError
    configure_cache(512, True, True)  # type: ignore
except TypeError as err:
    print("Caught expected positional violation:", err)
```

**Output:**
```
Configured Cache: {'size_mb': 512, 'auto_evict': True, 'compression': True, 'enable_logging': True}
Caught expected positional violation: configure_cache() takes 1 positional argument but 3 were given
```

---

### Example 2: API Decoupling via Positional-Only Parameters (`/`)

Using `/` guarantees that you can refactor internal parameter names in future releases without breaking third-party callers.

```python
# Version 1.0: parameter named 'raw_text'
def parse_markdown(raw_text: str, /, dialect: str = "commonmark") -> str:
    return f"Parsed {len(raw_text)} chars with dialect '{dialect}'"

# Caller can ONLY pass raw_text positionally:
print(parse_markdown("# Title", dialect="gfm"))

try:
    parse_markdown(raw_text="# Title")  # type: ignore
except TypeError as err:
    print("Caught expected keyword violation:", err)
```

**Output:**
```
Parsed 7 chars with dialect 'gfm'
Caught expected keyword violation: parse_markdown() got some positional-only arguments passed as keyword arguments: 'raw_text'
```

---

### Example 3: Resolving Parameter Name Collisions with `**kwargs`

Without `/`, if a function defines a parameter named `name` and accepts `**kwargs`, passing `name` inside `**kwargs` causes a collision:

```python
# WITHOUT '/': Collides if user data contains key 'name'!
def insert_record_broken(name: str, **kwargs):
    pass

# WITH '/': 'name' is positional-only; kwargs can freely contain 'name'!
def insert_record_safe(table_name: str, /, **fields):
    """table_name is positional-only; 'fields' can contain any key, even 'table_name'."""
    return f"INSERT INTO {table_name} VALUES ({fields})"

payload = {"id": 101, "table_name": "nested_metadata", "status": "active"}
result = insert_record_safe("users", **payload)
print(result)
```

**Output:**
```
INSERT INTO users VALUES ({'id': 101, 'table_name': 'nested_metadata', 'status': 'active'})
```

---

### Example 4: Mirroring C-Level Built-in Signatures

Many built-in CPython functions (like `len()`, `min()`, `abs()`, `pow()`) are positional-only by nature. PEP 570 allows pure-Python functions to emulate this exact C-native signature.

```python
# Mimicking standard library pow(base, exp, mod=None)
def fast_power(base: int, exp: int, /, mod: int | None = None) -> int:
    result = base ** exp
    return result if mod is None else result % mod

print("2^10       :", fast_power(2, 10))
print("2^10 mod 7 :", fast_power(2, 10, mod=7))
print("2^10 mod 7 (pos):", fast_power(2, 10, 7))
```

**Output:**
```
2^10       : 1024
2^10 mod 7 : 2
2^10 mod 7 (pos): 2
```

---

### Example 5: Comprehensive Full-Spectrum Signature

```python
def full_spectrum_api(
    source_ip: str, port: int, /,          # Positional-Only
    protocol: str = "tcp",                 # Standard (Either)
    *,
    timeout_sec: float = 5.0,              # Keyword-Only
    verify_cert: bool = True               # Keyword-Only
) -> str:
    return (
        f"Connecting to {source_ip}:{port} over {protocol.upper()} "
        f"[timeout={timeout_sec}s, verify={verify_cert}]"
    )

# Valid call
conn1 = full_spectrum_api("10.0.0.1", 443, "https", timeout_sec=2.5)
print(conn1)

conn2 = full_spectrum_api("10.0.0.1", 443, protocol="tcp", verify_cert=False)
print(conn2)
```

**Output:**
```
Connecting to 10.0.0.1:443 over HTTPS [timeout=2.5s, verify=True]
Connecting to 10.0.0.1:443 over TCP [timeout=5.0s, verify=False]
```

---

## 4. Common Anti-Patterns & Mistakes

| Anti-Pattern / Mistake | Broken / Unidiomatic Snippet | Idiomatic Correction | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Multiple `/` markers** | `def f(a, /, b, /):` | Exactly one `/` allowed | SyntaxError: only one `/` is permitted in a signature. |
| **Placing `/` after `*`** | `def f(*args, /):` | `/` must precede `*` | SyntaxError: positional-only parameters cannot follow var-positional. |
| **Passing kw-only positionally** | `def f(*, log=True); f(True)` | `f(log=True)` | Raises `TypeError: f() takes 0 positional arguments but 1 was given`. |
| **Passing pos-only as keyword** | `def f(x, /); f(x=10)` | `f(10)` | Raises `TypeError: got some positional-only arguments passed as keyword arguments`. |
| **Positional-only without defaults after default** | `def f(a=1, b, /):` | `def f(a, b=1, /):` | Standard default ordering rules still apply before `/`. |
| **Overusing `/` on simple scripts** | Adding `/` to internal private helper | Reserve `/` for public libraries | Unnecessary rigidity in small scripts where keyword clarity aids readability. |

---

## 5. Architectural & Design Decisions

### When to Use Positional-Only (`/`)
1. **The parameter has no natural semantic name:** For example, `abs(x)` or `len(s)`. Calling `abs(x=-5)` adds zero clarity over `abs(-5)`.
2. **Library API Stability:** When authoring public libraries where you want the freedom to rename parameters in minor releases without breaking client code.
3. **Preventing `**kwargs` Collisions:** When accepting arbitrary user dictionaries via `**kwargs`.

### When to Use Keyword-Only (`*`)
1. **Boolean flags and configuration options:** Calling `render(True, False)` is obscure; calling `render(cache=True, debug=False)` is immediately understandable.
2. **Parameters with defaults that are rarely altered:** Prevents callers from having to supply all intermediate arguments just to change one trailing option.

---

## 6. Verification & Practical Testing

```python
def test_positional_only_enforcement():
    def add(a, b, /): return a + b
    assert add(2, 3) == 5
    try:
        add(a=2, b=3)  # type: ignore
        assert False, "Should have raised TypeError"
    except TypeError:
        pass

def test_keyword_only_enforcement():
    def log(msg, *, level="INFO"): return f"[{level}] {msg}"
    assert log("Hello") == "[INFO] Hello"
    assert log("Error", level="ERROR") == "[ERROR] Error"
    try:
        log("Test", "DEBUG")  # type: ignore
        assert False, "Should have raised TypeError"
    except TypeError:
        pass

def test_signature_kinds():
    import inspect
    def sample(p_only, /, standard, *, kw_only): pass
    sig = inspect.signature(sample)
    kinds = [p.kind for p in sig.parameters.values()]
    assert kinds[0] == inspect.Parameter.POSITIONAL_ONLY
    assert kinds[1] == inspect.Parameter.POSITIONAL_OR_KEYWORD
    assert kinds[2] == inspect.Parameter.KEYWORD_ONLY

if __name__ == "__main__":
    test_positional_only_enforcement()
    test_keyword_only_enforcement()
    test_signature_kinds()
    print("All positional-only and keyword-only tests passed successfully.")
```

**Output:**
```
All positional-only and keyword-only tests passed successfully.
```

---

## 7. Deep Dive: Bytecode Efficiency of Positional-Only Arguments

In CPython, positional arguments are passed via stack offsets without dictionary lookups or keyword mapping checks. When a function specifies positional-only parameters (`/`), the Python VM executes a streamlined parameter validation check in C:
- It verifies `PyTuple_GET_SIZE(args) >= min_args`.
- It verifies `kwargs == NULL || PyDict_GET_SIZE(kwargs) == 0`.

This eliminates parameter name scanning and keyword dictionary hashing, making positional-only calls slightly faster than standard positional-or-keyword calls.

---

## 8. Summary & Key Takeaways

1. **`/` Marker:** All parameters preceding `/` are positional-only. Callers cannot pass them by keyword name.
2. **`*` Marker:** All parameters following `*` are keyword-only. Callers cannot pass them positionally.
3. **API Evolution:** Use `/` in public libraries to safely rename internal parameter names in future versions.
4. **Code Readability:** Use `*` for boolean flags and optional configurations to force self-documenting call sites.
5. **Collision Defense:** Use `/` on fixed parameters when taking `**kwargs` to prevent naming collisions.

---

## 9. Frequently Asked Questions (Interview & Senior Roles)

<details>
<summary>1. Why was the forward slash `/` chosen as the positional-only syntax marker in PEP 570?</summary>

The `/` syntax was chosen to complement the existing `*` marker. The asterisk `*` resembles a wildcard that expands or divides positional arguments from keyword arguments. The forward slash `/` was already the established convention used in Python's internal documentation and C docstrings (e.g. `help(len)` showed `len(obj, /)`) since Python 1.5.
</details>

<details>
<summary>2. Can a function have both `/` and `*` in the same signature?</summary>

Yes. For example: `def f(a, /, b, *, c):`. In this signature, `a` is positional-only, `b` is positional-or-keyword, and `c` is keyword-only.
</details>

<details>
<summary>3. What is the difference between `def f(*args, kw):` and `def f(*, kw):`?</summary>

- `def f(*args, kw):` accepts arbitrary excess positional arguments, packing them into tuple `args`, and requires `kw` as keyword-only.
- `def f(*, kw):` accepts **no** excess positional arguments (a bare `*` is purely a syntactic boundary marker and does not instantiate a tuple).
</details>

<details>
<summary>4. How does `inspect.Parameter.kind` represent these boundaries?</summary>

The `inspect` module provides five distinct enumeration members:
1. `POSITIONAL_ONLY`
2. `POSITIONAL_OR_KEYWORD`
3. `VAR_POSITIONAL` (`*args`)
4. `KEYWORD_ONLY`
5. `VAR_KEYWORD` (`**kwargs`)
</details>

<details>
<summary>5. How can you combine positional-only parameters with default values?</summary>

Place defaults on positional-only parameters before the `/`:
```python
def sample(a, b=10, /): pass
```
`a` is required positional-only; `b` is optional positional-only with default 10.
</details>

<details>
<summary>6. Why is keyword-only syntax useful in decorators?</summary>

Decorators often accept configuration parameters that must never be confused with the target wrapped function. Using keyword-only parameters (`def retry(*, max_attempts=3):`) prevents callers from accidentally passing the function as a positional argument.
</details>

<details>
<summary>7. What happens if a caller passes a positional argument after a keyword argument?</summary>

Python raises a compile-time `SyntaxError: positional argument follows keyword argument`. Positional arguments must always precede keyword arguments at the call site.
</details>

<details>
<summary>8. How do positional-only and keyword-only parameters interact with Free-threaded Python (PEP 703)?</summary>

Parameter resolution happens during frame initialization on the local thread stack. Positional-only and keyword-only boundary validations are purely thread-local validation logic and carry zero threading overhead.
</details>

---

## 10. Practical Exercises & Self-Assessment

<details>
<summary>Exercise 1: Refactor Obscure Boolean Signatures</summary>

**Task:** Refactor `send_email(to, subject, body, True, False)` to enforce keyword-only arguments for `html` and `encrypt` flags.

**Solution:**
```python
def send_email(to: str, subject: str, body: str, *, html: bool = False, encrypt: bool = False):
    return f"To: {to} | Encrypted: {encrypt} | HTML: {html}"

# Must call with keywords:
print(send_email("alice@work.com", "Report", "Attached", html=True, encrypt=False))
```
</details>

<details>
<summary>Exercise 2: Public Library API Hardening with `/`</summary>

**Task:** Design a matrix determinant function `det(matrix, /)` that prevents users from coupling to the internal name `matrix`.

**Solution:**
```python
def det(matrix: list[list[float]], /) -> float:
    # Safe to rename parameter to 'm' or 'grid' internally in v2.0!
    return matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0]

assert det([[1, 2], [3, 4]]) == -2
```
</details>

<details>
<summary>Exercise 3: Safe Dispatcher with Keyword Collision Avoidance</summary>

**Task:** Write an event dispatcher `emit(event_name, /, **payload)` that can safely accept an event where the payload contains `event_name="login"`.

**Solution:**
```python
def emit(event_name: str, /, **payload):
    print(f"Dispatched event: '{event_name}' with data: {payload}")

emit("audit_log", event_name="sub_event", user="shiva")
# Works without name collision!
```
</details>

<details>
<summary>Exercise 4: Signature Kind Audit Utility</summary>

**Task:** Write a function `audit_signature(fn)` that prints the name and kind of every parameter in `fn`.

**Solution:**
```python
import inspect

def audit_signature(fn):
    sig = inspect.signature(fn)
    for name, param in sig.parameters.items():
        print(f"Parameter: {name:<12} | Kind: {param.kind.name}")

def demo(a, b, /, c, *, d=10): pass
audit_signature(demo)
```
</details>

<details>
<summary>Exercise 5: Math Function Emulating C-Native Signatures</summary>

**Task:** Implement `hypot(x, y, /)` matching `math.hypot` positional-only semantics.

**Solution:**
```python
def hypot(x: float, y: float, /) -> float:
    return (x**2 + y**2) ** 0.5

print(hypot(3.0, 4.0))  # 5.0
try:
    hypot(x=3.0, y=4.0) # type: ignore
except TypeError as err:
    print("Caught expected positional-only violation:", err)
```
</details>

<details>
<summary>Exercise 6: Decorator Factory with Mandatory Keyword Options</summary>

**Task:** Create a decorator factory `@timeout(*, seconds=30)` that strictly requires keyword-only arguments.

**Solution:**
```python
import functools

def timeout(*, seconds: float = 30.0):
    def decorator(fn):
        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            print(f"Enforcing {seconds}s timeout on {fn.__name__}")
            return fn(*args, **kwargs)
        return wrapper
    return decorator

@timeout(seconds=5.0)
def fetch_data(): return "OK"

fetch_data()
```
</details>
