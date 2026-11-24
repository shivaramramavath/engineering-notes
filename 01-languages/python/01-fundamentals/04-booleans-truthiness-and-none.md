# Module 01: Booleans, Truthiness & None (04-booleans-truthiness-and-none)

> **Topic Overview**: Truth values and logical foundations in Python: the `bool` type as a subclass of `int`, truth value testing (`bool()`), truthy vs falsy objects, the `__bool__()` and `__len__()` data model protocols, short-circuit evaluation (`and`/`or`), and the `NoneType` singleton.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Python Fundamentals & Syntax |
| **Target Level** | Beginner to Intermediate |
| **Python Version** | Python 3.10+ (tested on CPython 3.14.0) |
| **Tested With** | CPython 3.14.0, Ruff 0.17.6, Mypy 2.4.1 |
| **Free-Threading / Platform** | Platform independent |

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
- **Problem Statement**: In many languages, boolean checks require rigid boolean types, forcing verbose checks like `if len(items) > 0:` or `if ptr != null:`. In other languages (like JavaScript), confusing implicit coercion rules (e.g. `[] == false` is true, but `if ([])` is truthy) introduce subtle logic errors.
- **Mental Model**: Python employs an elegant, mathematically consistent protocol for truth evaluation. **Every object in Python possesses an inherent truth value (truthiness)**. An empty collection or zero is falsy; a populated collection or non-zero value is truthy. `None` is the unique singleton representing the intentional absence of a value.
- **When to Use**: Conditional branching (`if`, `while`), default value fallback expressions (`a = x or y`), and sentinel initialization.
- **When NOT to Use**: Do not rely on loose truthiness when `0` or `False` are valid, meaningful business domain inputs (e.g. `if not count:` treats `0` as absent!).

---

## 2. Core Concepts
1. **The `bool` Type Subclass**: `bool` is an explicit subclass of `int` (`issubclass(bool, int) is True`). `True` evaluates to `1` and `False` evaluates to `0` in arithmetic contexts (e.g., `True + True == 2`).
2. **Standard Falsy Values**:
   - Constants: `None`, `False`.
   - Zero of any numeric type: `0`, `0.0`, `0j`, `Decimal(0)`, `Fraction(0, 1)`.
   - Empty sequences and collections: `""`, `()`, `[]`, `{}`, `set()`, `range(0)`.
   - Custom objects returning `False` from `__bool__()` or `0` from `__len__()`.
3. **The Truthiness Protocol**:
   - When evaluating `bool(x)`, CPython first checks if `x.__bool__()` is defined.
   - If not defined, CPython checks if `x.__len__()` is defined (returns `True` if `len > 0`, `False` if `len == 0`).
   - If neither is defined, the object is considered **truthy by default**.
4. **Short-Circuit Evaluation**:
   - `x and y`: Evaluates `x`. If `x` is falsy, returns `x` immediately without evaluating `y`. Otherwise, evaluates and returns `y`.
   - `x or y`: Evaluates `x`. If `x` is truthy, returns `x` immediately without evaluating `y`. Otherwise, evaluates and returns `y`.
5. **The `None` Singleton**: `None` is the sole instance of `NoneType`. It represents the absence of a value and must always be checked with identity (`is None`), never equality.

---

## 3. How It Works Internally

How CPython tests truthiness in the virtual machine:

```text
Evaluating: `if obj:`
            │
            ▼
    [ PyObject_IsTrue(PyObject *v) ]
            │
            ├─► Is v == Py_True?  ──► Return 1 (True)
            ├─► Is v == Py_False? ──► Return 0 (False)
            ├─► Is v == Py_None?  ──► Return 0 (False)
            │
            ▼
    Does v->ob_type define `tp_as_number->nb_bool`?
            ├─► YES: Call __bool__() -> Return 1 or 0
            │
            ▼
    Does v->ob_type define `tp_as_mapping->mp_length` or `tp_as_sequence->sq_length`?
            ├─► YES: Call __len__() -> Return (len != 0)
            │
            ▼
    Default: Return 1 (Object is Truthy!)
```

---

## 4. Syntax & API Reference

### Testing Truthiness
```python
bool(x)             # Explicitly converts any object to its boolean value
x is None           # Identity check for None singleton
x is not None       # Identity check for non-None
```

### Short-Circuit Return Values
```python
# 'or' returns the first truthy value, or the last value
name = user_input or "Default User"

# 'and' returns the first falsy value, or the last value
result = is_valid and compute_expensive_value()
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Inspecting Built-in Truthiness
```python
values = [0, 1, "", "text", [], [1], None, object()]
truth_map = {repr(v): bool(v) for v in values}
for k, v in list(truth_map.items())[:6]:
    print(f"{k} -> {v}")
```
**Output**:
```text
0 -> False
1 -> True
'' -> False
'text' -> True
[] -> False
[1] -> True
```

---

### 2. Real-World Production Example: Custom Truthiness in Domain Models
```python
class QueryResultSet:
    """A database query result set that evaluates truthy only if it contains rows."""
    def __init__(self, rows: list[dict[str, object]]) -> None:
        self._rows = rows

    def __len__(self) -> int:
        return len(self._rows)

    def __bool__(self) -> bool:
        # Explicit truthiness definition
        return len(self._rows) > 0

empty_results = QueryResultSet([])
populated_results = QueryResultSet([{"id": 1, "status": "active"}])

print(f"Empty results truthiness:     {bool(empty_results)}")
print(f"Populated results truthiness: {bool(populated_results)}")

if populated_results:
    print("Found active rows in query result set.")
```
**Output**:
```text
Empty results truthiness:     False
Populated results truthiness: True
Found active rows in query result set.
```

---

### 3. Edge Case: Truthiness vs Numeric Zero in Optional Arguments
```python
def configure_cache_ttl(ttl_seconds: int | None = None) -> int:
    """Configures cache expiration. A value of 0 means 'no expiration'."""
    # ANTI-PATTERN: ttl = ttl_seconds or 3600 (converts 0 to 3600!)
    # CORRECT: Explicit check for None
    if ttl_seconds is None:
        return 3600  # Default 1 hour
    return ttl_seconds

print("Default (None):", configure_cache_ttl(None))
print("Zero TTL (0):   ", configure_cache_ttl(0))
```
**Output**:
```text
Default (None): 3600
Zero TTL (0):    0
```

---

### 4. Failure Case: Defining `__bool__` That Returns Non-Boolean
```python
class BrokenTruth:
    def __bool__(self) -> int:  # type: ignore[override]
        return 42  # __bool__ MUST return True or False

try:
    bool(BrokenTruth())
except TypeError as exc:
    print(f"Caught expected error: {type(exc).__name__}: {exc}")
```
**Output**:
```text
Caught expected error: TypeError: __bool__ should return bool, returned int
```

---

### 5. Idiomatic Pythonic Pattern: Guarding Optional Chains with Short-Circuit
```python
def get_user_display_name(user_record: dict[str, str] | None) -> str:
    """Safely extracts full name, username, or anonymous fallback."""
    # Leverages dict.get and short-circuit evaluation
    return (user_record and user_record.get("full_name")) or "Anonymous Guest"

print("With record:   ", get_user_display_name({"full_name": "Ada Lovelace"}))
print("With empty:    ", get_user_display_name({}))
print("With None:     ", get_user_display_name(None))
```
**Output**:
```text
With record:    Ada Lovelace
With empty:     Anonymous Guest
With None:      Anonymous Guest
```

---

## 6. Edge Cases & Pitfalls
1. **`bool` Is an `int`**: Because `bool` subclasses `int`, `True == 1` and `False == 0`. This means `isinstance(True, int)` is `True`! In dictionary keys, `{1: "one", True: "true"}` collapses into a single entry `{1: "true"}` because `hash(1) == hash(True)` and `1 == True`.
2. **Short-Circuit Returns the Operand, Not a Boolean**: `a or b` does not return `True` or `False`; it returns the actual object `a` or `b`.
3. **Empty Custom Classes**: By default, user-defined class instances without `__bool__` or `__len__` evaluate to `True`, even if all their internal fields are empty.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Using `if not value:` when `0` is valid | Falsely treats `0` as absent | Logic error where valid zero is overwritten | Use `if value is None:` |
| Using `x == None` instead of `x is None` | Calls `__eq__` and violates PEP 8 | Lint error `E711` | Use `x is None` |
| Returning integers from `__bool__()` | Python requires `__bool__` to return strict `bool` | `TypeError: __bool__ should return bool` | Return `True` or `False` |
| Using `bool` as dictionary keys alongside `0` and `1` | `True` collides with `1` and `False` collides with `0` | Overwrites dictionary keys silently | Use distinct string keys |
| Expecting `a and b` to always return boolean | Returns operand values, not `True`/`False` | Type mismatches downstream | Use `bool(a and b)` if bool needed |
| Writing `if is_valid == True:` | Redundant and non-idiomatic | Style warning `E712` | Use `if is_valid:` |

---

## 8. Performance & Complexity
- **`PyObject_IsTrue()` Check**: $O(1)$ constant time.
- **Identity Check (`is None`)**: Single pointer comparison in CPU register ($O(1)$, $<1$ nanosecond).

---

## 9. CPython Internals & Bytecode: The `TO_BOOL` and `POP_JUMP_IF_FALSE` Opcodes
In CPython 3.12+, conditional statements are optimized with specialized branch opcodes:

```python
import dis

def branch(val: object) -> str:
    if val:
        return "yes"
    return "no"

dis.dis(branch)
```
Disassembly:
```text
  1           0 RESUME                   0

  2           2 LOAD_FAST                0 (val)
              4 TO_BOOL                  # Evaluates PyObject_IsTrue
              8 POP_JUMP_IF_FALSE        3 (to 16)

  3          10 LOAD_CONST               1 ('yes')
             12 RETURN_VALUE

  4     >>   16 LOAD_CONST               2 ('no')
             18 RETURN_VALUE
```

---

## 10. Best Practices
1. Check for `None` using identity (`if x is None:` and `if x is not None:`).
2. Leverage Pythonic truthiness for empty containers (`if not items:` instead of `if len(items) == 0:`).
3. Be cautious with `value or default` if `0`, `0.0`, or `False` are valid configurations.
4. Never write `if x == True:` or `if x == False:`.

---

## 11. Real-World Case Study: The Zero Threshold Bug in Alerting Systems
A site reliability engineering team implemented an alert escalation service:
```python
# Buggy code in production alert threshold:
max_allowed_errors = config.get("error_threshold") or 10
```
When an engineer configured a mission-critical billing pipeline with `error_threshold: 0` (zero tolerance for errors), the expression evaluated `0 or 10`, setting the threshold to `10`!
1. The bug allowed 9 billing errors to pass silently without triggering alerts.
2. The team refactored the code to use explicit `None` checks:
```python
threshold = config["error_threshold"] if config.get("error_threshold") is not None else 10
```

---

## 12. Interview Questions

### Q1: Why is `issubclass(bool, int)` True in Python?
<details>
<summary><strong>Answer</strong></summary>
Historically, Python did not have a dedicated boolean type in its earliest versions; integers `1` and `0` were used for truth values. When PEP 285 introduced `bool` in Python 2.3, it was implemented as a subclass of `int` to preserve backward compatibility with existing code that expected numerical operations on boolean results.
</details>

### Q2: How does Python determine whether an object is truthy or falsy?
<details>
<summary><strong>Answer</strong></summary>
CPython evaluates truthiness via `PyObject_IsTrue()`:
1. Direct pointers `True`, `False`, and `None` are resolved immediately.
2. If the object defines `__bool__()`, it is invoked (must return `bool`).
3. If `__bool__()` is not defined, CPython checks for `__len__()`; if it returns `0`, the object is falsy; if greater than `0`, it is truthy.
4. If neither protocol is implemented, the object is considered truthy by default.
</details>

### Q3: What is the exact return value of `x or y` and `x and y`?
<details>
<summary><strong>Answer</strong></summary>
Python logical operators do not coerce their results to boolean.
- `x or y`: Evaluates `x`. If `x` is truthy, it returns `x` immediately without evaluating `y`. Otherwise, it evaluates and returns `y`.
- `x and y`: Evaluates `x`. If `x` is falsy, it returns `x` immediately without evaluating `y`. Otherwise, it evaluates and returns `y`.
</details>

### Q4: Why does `{True: "a", 1: "b"}` contain only one key?
<details>
<summary><strong>Answer</strong></summary>
Dictionaries determine key uniqueness using both hash equality (`hash(k1) == hash(k2)`) and value equality (`k1 == k2`). Because `bool` subclasses `int`, `hash(True) == 1` and `True == 1`. When `1: "b"` is inserted, it recognizes the existing key `True` and overwrites its value, resulting in `{True: 'b'}`.
</details>

### Q5: Why is `None` a singleton and how should it be checked?
<details>
<summary><strong>Answer</strong></summary>
`None` is the sole instance of `NoneType` in the entire CPython runtime process. Because only one instance exists in memory, testing for `None` should always use pointer identity (`if x is None:` or `if x is not None:`). Identity checks avoid method calls to `__eq__()` and run in a single CPU instruction.
</details>

### Q6: Can a class define both `__bool__` and `__len__`? Which takes precedence?
<details>
<summary><strong>Answer</strong></summary>
Yes. If a class defines both `__bool__()` and `__len__()`, Python calls `__bool__()` first. `__len__()` is only consulted as a fallback if `__bool__()` is not defined on the class.
</details>

### Q7: What does the `TO_BOOL` opcode do in Python 3.12+?
<details>
<summary><strong>Answer</strong></summary>
`TO_BOOL` evaluates the truth value of the top of the stack using `PyObject_IsTrue()` and replaces the object on the stack with a boolean value or uses specialized inline caching for rapid subsequent branch evaluation.
</details>

### Q8: What is a sentinel value and why is `None` commonly used as one?
<details>
<summary><strong>Answer</strong></summary>
A sentinel is a unique object used to signal a special condition, such as the absence of an argument or the end of a sequence. `None` is the standard default sentinel for optional arguments. If `None` is a valid argument value, a custom sentinel object (e.g. `_MISSING = object()`) can be created to distinguish between a user passing `None` and omitting the argument entirely.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Identify All Falsy Elements in a List
Write a function `filter_falsy(items)` that returns only the elements of a list that evaluate to falsy.
<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Any

def filter_falsy(items: list[Any]) -> list[Any]:
    return [x for x in items if not x]

print("Falsy items:", filter_falsy([0, 1, "", "hello", None, [], [10]]))
```
</details>

### Challenge 2 (Easy): Safe First Non-None Value Selector
Write a function `coalesce(*args)` that returns the first argument that is not `None`, or `None` if all arguments are `None`.
<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Any

def coalesce(*args: Any) -> Any:
    for arg in args:
        if arg is not None:
            return arg
    return None

print("Result:", coalesce(None, None, 0, 100))  # Correctly returns 0, not 100!
```
</details>

### Challenge 3 (Medium): Custom Truthiness Container
Implement a `FiniteQueue` class with an internal list that evaluates to `False` whenever it is empty OR whenever it is completely full.
<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Any

class FiniteQueue:
    def __init__(self, capacity: int) -> None:
        self.capacity = capacity
        self.items: list[Any] = []

    def push(self, item: Any) -> None:
        if len(self.items) < self.capacity:
            self.items.append(item)

    def __bool__(self) -> bool:
        # Falsy if empty or full
        return 0 < len(self.items) < self.capacity

q = FiniteQueue(capacity=2)
print("Empty queue truthiness:", bool(q))
q.push("item1")
print("Half-full queue truthiness:", bool(q))
q.push("item2")
print("Full queue truthiness:", bool(q))
```
</details>

### Challenge 4 (Medium): Sentinel Object Pattern
Implement a function `fetch_configuration(key, default=_SENTINEL)` using a unique sentinel object so that passing `default=None` is distinguished from omitting the argument.
<details>
<summary><strong>Solution</strong></summary>

```python
_MISSING = object()

CONFIG_STORE = {"db_host": "localhost", "db_port": None}

def fetch_configuration(key: str, default: object = _MISSING) -> object:
    if key in CONFIG_STORE:
        return CONFIG_STORE[key]
    if default is not _MISSING:
        return default
    raise KeyError(f"Configuration key '{key}' not found.")

print("Found key with None value:", fetch_configuration("db_port"))
print("Missing key with default:", fetch_configuration("db_user", default="root"))
```
</details>

### Challenge 5 (Hard): AST Truthiness Optimization Inspector
Write a function that uses the `ast` module to scan Python source code for redundant boolean comparisons like `x == True` or `x == False` and reports their line numbers.
<details>
<summary><strong>Solution</strong></summary>

```python
import ast

def detect_redundant_boolean_comparisons(source: str) -> list[int]:
    tree = ast.parse(source)
    violations = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Compare):
            for comparator in node.comparators:
                if isinstance(comparator, ast.Constant) and isinstance(comparator.value, bool):
                    violations.append(node.lineno)
    return violations

sample = """
if is_ready == True:
    pass
if not active == False:
    pass
"""
print("Violations detected on lines:", detect_redundant_boolean_comparisons(sample))
```
</details>

### Challenge 6 (Hard): Lazy Boolean Expression Evaluator
Implement a custom `LazyOr` class that chains deferred computation functions and executes them only until the first truthy result is encountered.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections.abc import Callable
from typing import Any

class LazyOr:
    def __init__(self, *suppliers: Callable[[], Any]) -> None:
        self.suppliers = suppliers

    def evaluate(self) -> Any:
        for fn in self.suppliers:
            val = fn()
            if val:
                return val
        return None

def expensive_fallback() -> str:
    print("Executing expensive fallback...")
    return "Fallback Value"

lazy_chain = LazyOr(lambda: None, lambda: "", expensive_fallback)
print("Resolved:", lazy_chain.evaluate())
```
</details>
