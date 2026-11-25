---
title: "Type Hints Foundations: PEP 484 and Modern Typing Syntax"
description: "Mastering Python's static type system foundations: PEP 484, primitive annotations, PEP 604 union operator |, built-in generics, type inference, and the boundary between static analysis and runtime execution."
module: "11-type-hints-and-static-typing"
order: 0
tags:
  - python
  - typing
  - type-hints
  - pep-484
  - pep-604
  - static-analysis
  - generics
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# Type Hints Foundations: PEP 484 and Modern Typing Syntax

[<- Back to Module Index](./README.md)

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts & Mental Model](#core-concepts--mental-model)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & API Reference](#syntax--api-reference)
5. [Code Examples](#code-examples)
6. [Edge Cases, Pitfalls & Anti-Patterns](#edge-cases-pitfalls--anti-patterns)
7. [Performance & Memory Characteristics](#performance--memory-characteristics)
8. [CPython Internals & Bytecode](#cpython-internals--bytecode)
9. [Best Practices & Idiomatic Patterns](#best-practices--idiomatic-patterns)
10. [Real-World Case Study: Production E-Commerce Pricing & Discount Calculation Engine](#real-world-case-study-production-e-commerce-pricing--discount-calculation-engine)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Python is historically a dynamically typed language: variables do not hold types; objects hold types. Variable names are simply pointers bound to objects in memory. Introduced in Python 3.5 via PEP 484 and modernized across Python 3.9–3.12, **Type Hints** provide a standardized, expressive notation for declaring the expected types of function parameters, return values, and class attributes.

### Motivation
In small 50-line utility scripts, dynamic typing allows rapid prototyping. In large, multi-developer codebases spanning hundreds of thousands of lines, un-annotated dynamic typing creates severe friction:
1. **Silent Runtime Crashes**: Passing `None` into a function expecting a string manifests as an `AttributeError: 'NoneType' object has no attribute 'lower'` three function calls deeper.
2. **Poor IDE Developer Experience**: Without type hints, autocomplete cannot suggest object methods, requiring engineers to continuously cross-reference documentation.
3. **Refactoring Fear**: Renaming an attribute or changing a return signature risks breaking un-tested edge cases without compile-time warnings.
4. **Self-Documenting Code**: Type annotations act as compiler-verified documentation that never gets out of date.

### Real-World Analogy
Consider shipping parcels across the world:
- **Pure dynamic typing** is packing delicate glassware into un-labeled brown cardboard boxes. Handlers have no idea what is inside until someone opens the box at the final destination or drops it on concrete.
- **Type hints** are clear, standardized shipping labels attached to every box: `"FRAGILE: GLASSWARE, MAX WEIGHT 5KG"`. Automated airport sorting scanners (static type checkers like Mypy and Pyright) inspect the labels before planes ever take off, rejecting improperly packed cargo at the loading dock.

### When to Use vs When Not to Use
- **Use Type Hints for:**
  - Production applications, microservices, libraries, public APIs, and team codebases.
  - Domain models, business logic functions, and database schemas.
  - Anywhere code will be maintained, reviewed, or refactored by other humans.
- **Do Not Expect Type Hints to:**
  - Enforce runtime validation automatically (Python does **not** raise `TypeError` at runtime if you pass an integer to a `str` annotated parameter; use **Pydantic** for runtime validation).
  - Magically accelerate CPython execution speed (CPython's virtual machine largely ignores type annotations during execution).

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| Developer Workstation / CI Pipeline                                     |
|                                                                         |
| Python Source with Type Hints:                                          |
| def calculate_tax(amount: float, rate: float = 0.05) -> float: ...      |
+-------------------------------------------------------------------------+
       |                                                 |
       | Static Analysis Phase (Compile/CI)              | Runtime Execution Phase
       v                                                 v
+-------------------------------+             +---------------------------+
| Static Type Checker           |             | CPython Virtual Machine   |
| (Mypy / Pyright / IDE)        |             | (python app.py)           |
| - Validates type safety       |             | - Ignores type hints!     |
| - Flags type mismatches       |             | - Stores in __annotations__|
| - Rejects bad builds in CI    |             | - Executes dynamic code   |
+-------------------------------+             +---------------------------+
```

### Key Milestones in Python Typing Evolution
1. **PEP 484 (Python 3.5)**: The foundational specification introducing the `typing` module (`List`, `Dict`, `Tuple`, `Optional`, `Union`).
2. **PEP 526 (Python 3.6)**: Syntax for variable annotations (`x: int = 10`).
3. **PEP 585 (Python 3.9)**: Native built-in collection generics (`list[int]`, `dict[str, Any]` replacing `typing.List`, `typing.Dict`).
4. **PEP 604 (Python 3.10)**: The union operator `|` (`int | str` replacing `Union[int, str]`, `str | None` replacing `Optional[str]`).
5. **PEP 695 (Python 3.12)**: Native generic syntax (`class Box[T]:`, `type Vector = list[float]`).

---

## How It Works Internally

### 1. Storage in `__annotations__`
When the CPython compiler encounters a typed function or class:
- It compiles the body into bytecode exactly as it would un-annotated code.
- It evaluates the annotation expressions at function definition time (unless `from __future__ import annotations` is enabled).
- It stores the resolved type objects in the function's `__annotations__` dictionary attribute:
  ```python
  def greet(name: str) -> str:
      return "Hello, " + name

  print(greet.__annotations__)
  # {'name': <class 'str'>, 'return': <class 'str'>}
  ```

### 2. The Separation of Static Checking vs Runtime Evaluation
It is vital to understand that CPython does **zero type checking at runtime**.
```python
def add_numbers(a: int, b: int) -> int:
    return a + b

# CPython executes this without any error!
result = add_numbers("hello ", "world")
print(result)  # "hello world"
```
Type hints exist strictly for static analysis tools (Mypy, Pyright, IDE language servers) and runtime reflection frameworks (Pydantic, FastAPI).

---

## Syntax & API Reference

### Primitive Types
```python
name: str = "Alice"
age: int = 30
balance: float = 1250.75
is_active: bool = True
raw_payload: bytes = b"binary_data"
```

### Modern Container Types (Python 3.9+ PEP 585)
```python
# Homogeneous collections
scores: list[int] = [95, 88, 100]
unique_ids: set[str] = {"usr_1", "usr_2"}
frozen_ids: frozenset[int] = frozenset({1, 2, 3})

# Key-Value Mappings
user_lookup: dict[str, int] = {"alice": 101, "bob": 102}

# Tuples: Fixed-length heterogeneous vs Variable-length homogeneous
coordinate: tuple[float, float] = (37.77, -122.41)       # Exactly 2 floats
var_ints: tuple[int, ...] = (1, 2, 3, 4, 5)             # Ellipsis: arbitrary count
```

### Modern Union & Nullability Syntax (Python 3.10+ PEP 604)
```python
# Replaces Union[int, float]
numeric_val: int | float = 42

# Replaces Optional[str] / Union[str, None]
user_token: str | None = None
```

### Callable Signatures
```python
from collections.abc import Callable

# Callable[[ArgTypes], ReturnType]
formatter: Callable[[str, int], str]
predicate: Callable[[int], bool]
handler: Callable[..., Any]                              # Arbitrary parameters
```

---

## Code Examples

### Example 1: Modern Function Annotations with Union Syntax
```python
from collections.abc import Sequence

def calculate_discount(
    price: float,
    discount_pct: float | None = None,
    coupon_codes: Sequence[str] | None = None
) -> float:
    """Calculates discounted price supporting either percentage or coupon codes."""
    final_price = price

    if discount_pct is not None:
        if not (0.0 <= discount_pct <= 1.0):
            raise ValueError("Discount percentage must be between 0.0 and 1.0")
        final_price -= (price * discount_pct)

    if coupon_codes:
        for code in coupon_codes:
            if code == "VIP10":
                final_price -= 10.0

    return max(0.0, final_price)

def run_discount_demo():
    p1 = calculate_discount(100.0, discount_pct=0.20)
    p2 = calculate_discount(100.0, coupon_codes=["VIP10"])
    p3 = calculate_discount(50.0)

    print(f"Discounted P1 (20% off): ${p1:.2f}")
    print(f"Discounted P2 (Coupon):  ${p2:.2f}")
    print(f"Discounted P3 (Full):    ${p3:.2f}")

run_discount_demo()
# Output:
# Discounted P1 (20% off): $80.00
# Discounted P2 (Coupon):  $90.00
# Discounted P3 (Full):    $50.00
```

### Example 2: Type Aliases with Modern `type` Statement (Python 3.12+)
```python
# Python 3.12+ PEP 695 type alias syntax
type UserID = int
type Coordinates = tuple[float, float]
type JSONPrimitive = str | int | float | bool | None
type JSONObject = dict[str, Any]

def locate_user(uid: UserID) -> Coordinates:
    # Simulated database lookup
    return (37.7749, -122.4194)

def run_type_alias_demo():
    user: UserID = 1048204
    location: Coordinates = locate_user(user)
    print(f"User {user} located at GPS: Lat={location[0]}, Lon={location[1]}")

run_type_alias_demo()
# Output:
# User 1048204 located at GPS: Lat=37.7749, Lon=-122.4194
```

### Example 3: Runtime Inspection of Annotations via `typing.get_type_hints`
```python
from typing import get_type_hints

class UserDTO:
    username: str
    age: int
    is_admin: bool = False

def run_inspection_demo():
    # Never read obj.__annotations__ directly in production!
    # Always use typing.get_type_hints() which resolves forward references and globals
    hints = get_type_hints(UserDTO)
    print("Resolved Type Hints:")
    for field, field_type in hints.items():
        print(f"  {field:10}: {field_type.__name__}")

run_inspection_demo()
# Output:
# Resolved Type Hints:
#   username  : str
#   age       : int
#   is_admin  : bool
```

### Example 4: Tuple Annotations: Fixed vs Ellipsis
```python
def process_tuples():
    # 1. Fixed-length heterogeneous tuple: exactly 3 elements with distinct types
    record: tuple[int, str, bool] = (101, "Alice", True)

    # 2. Variable-length homogeneous tuple: arbitrary number of integers
    numbers: tuple[int, ...] = (10, 20, 30, 40, 50)

    # 3. Empty tuple
    empty_tuple: tuple[()] = ()

    print("Heterogeneous record:", record)
    print("Homogeneous numbers: ", numbers)
    print("Empty tuple:         ", empty_tuple)

process_tuples()
# Output:
# Heterogeneous record: (101, 'Alice', True)
# Homogeneous numbers:  (10, 20, 30, 40, 50)
# Empty tuple:          ()
```

### Example 5: High-Order Functions with `Callable`
```python
from collections.abc import Callable, Sequence

def transform_and_filter(
    items: Sequence[int],
    transform: Callable[[int], int],
    predicate: Callable[[int], bool]
) -> list[int]:
    """Applies transformation and filters results according to type-annotated callables."""
    results: list[int] = []
    for item in items:
        transformed = transform(item)
        if predicate(transformed):
            results.append(transformed)
    return results

def run_callable_demo():
    raw_numbers = [1, 2, 3, 4, 5, 6]
    # Double each number, keep only those greater than 5
    out = transform_and_filter(
        items=raw_numbers,
        transform=lambda x: x * 2,
        predicate=lambda x: x > 5
    )
    print("Transformed & Filtered:", out)
    assert out == [6, 8, 10, 12]

run_callable_demo()
# Output:
# Transformed & Filtered: [6, 8, 10, 12]
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. The `bool` is an `int` Subtyping Trap
In Python's type hierarchy, `bool` is a subclass of `int` (`issubclass(bool, int) == True`).
A function annotated as `def process(val: int):` will accept `True` and `False` without static type errors! If you must accept strictly integers and reject booleans, static type checkers require custom `TypeGuard` or explicit checks.

### 2. Using Concrete Collections in Function Signatures
Annotating function parameters as `def process(items: list[str]):` is an anti-pattern.
- If a caller passes a `tuple` or a `set`, the type checker flags an error, even if the function only iterates over the items!
- **Rule**: Be liberal in what you accept and conservative in what you return. Accept abstract collections (`collections.abc.Sequence[str]` or `Iterable[str]`), and return concrete types (`list[str]`).

### 3. Confusing `tuple[int]` with `tuple[int, ...]`
- `tuple[int]` denotes a tuple containing **exactly one single integer**.
- `tuple[int, ...]` denotes a tuple containing **zero or more integers**.

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `from typing import List, Dict` | `list[int]`, `dict[str, int]` | Legacy PEP 484; Python 3.9+ supports built-in collections directly. | Medium |
| `from typing import Optional, Union` | `int | str`, `str | None` | Legacy syntax; Python 3.10+ PEP 604 operator `|` is cleaner and faster. | Medium |
| `def fn(data: list[int]):` (only reading) | `def fn(data: Sequence[int]):` | Over-constraining parameter prevents callers from passing tuples. | High |
| `val: tuple[int] = (1, 2, 3)` | `val: tuple[int, ...] = (1, 2, 3)` | `tuple[int]` requires exactly one element; flags static type error. | High |
| Reading `obj.__annotations__` directly | `typing.get_type_hints(obj)` | Direct attribute access fails to evaluate stringified forward references. | High |
| Expecting type hints to validate input at runtime | Use Pydantic or manual `isinstance` | Python runtime ignores annotations; invalid types execute without error. | Critical |

---

## Performance & Memory Characteristics

### Computational Overhead
- **At Runtime**: Type hints add **zero execution overhead** to function invocations. Bytecode execution speed is 100% identical between typed and untyped functions.
- **At Import Time**: Evaluating type annotations creates minor import-time overhead (~5%–10% on large modules) because Python creates type objects.
- **Mitigation (Python 3.7+)**: Adding `from __future__ import annotations` (or Python 3.14 PEP 649 deferred evaluation) delays annotation evaluation, reducing import time to near zero.

---

## CPython Internals & Bytecode

In CPython:
- Variable annotations are compiled into `SETUP_ANNOTATIONS` bytecode at module/class scope.
- Function parameter annotations are compiled and stored in `func.__annotations__` as a standard dictionary.

Bytecode disassembly of a typed function:
```python
import dis

def add_typed(x: int, y: int) -> int:
    return x + y

dis.dis(add_typed)
```
```text
  1           0 RESUME                   0
  2           2 LOAD_FAST                0 (x)
              4 LOAD_FAST                1 (y)
              6 BINARY_OP                0 (+)
             10 RETURN_VALUE
```
Notice that the bytecode contains **zero type-checking instructions**! The type hints have been completely stripped out of the execution instructions.

---

## Best Practices & Idiomatic Patterns

1. **Use Modern Union Syntax (`|`) Everywhere**:
   Replace `Union[A, B]` and `Optional[A]` with `A | B` and `A | None`.
2. **Accept Abstract Interfaces in Parameters**:
   Use `collections.abc.Iterable`, `Sequence`, or `Mapping` for function inputs.
3. **Return Concrete Types**:
   Return `list[T]` or `dict[K, V]` rather than abstract interfaces, giving callers maximum flexibility.
4. **Always Annotate Function Signatures in Public APIs**:
   Leave internal local variables un-annotated unless type inference requires disambiguation.

---

## Real-World Case Study: Production E-Commerce Pricing & Discount Calculation Engine

The following e-commerce pricing service uses strict type annotations to model pricing rules, currency boundaries, and discounts with compile-time verification.

```python
from collections.abc import Sequence, Mapping
from dataclasses import dataclass
from typing import Final, get_type_hints

# Type Aliases (PEP 695 / Modern Typing)
type Currency = str
type PriceCents = int

@dataclass(frozen=True, slots=True)
class LineItem:
    sku: str
    unit_price_cents: PriceCents
    quantity: int

@dataclass(frozen=True, slots=True)
class Cart:
    cart_id: str
    currency: Currency
    items: Sequence[LineItem]
    coupon_code: str | None = None

class PricingEngine:
    TAX_RATE: Final[float] = 0.0825  # 8.25% state tax
    VALID_COUPONS: Final[Mapping[str, float]] = {
        "SPRING10": 0.10,
        "SUMMER20": 0.20
    }

    def calculate_subtotal(self, cart: Cart) -> PriceCents:
        return sum(item.unit_price_cents * item.quantity for item in cart.items)

    def calculate_discount(self, subtotal: PriceCents, coupon_code: str | None) -> PriceCents:
        if not coupon_code or coupon_code not in self.VALID_COUPONS:
            return 0
        discount_rate = self.VALID_COUPONS[coupon_code]
        return int(subtotal * discount_rate)

    def calculate_order_total(self, cart: Cart) -> PriceCents:
        subtotal = self.calculate_subtotal(cart)
        discount = self.calculate_discount(subtotal, cart.coupon_code)
        discounted_base = max(0, subtotal - discount)
        tax = int(discounted_base * self.TAX_RATE)
        return discounted_base + tax

# Verification
if __name__ == "__main__":
    engine = PricingEngine()
    cart = Cart(
        cart_id="cart_9984",
        currency="USD",
        items=[
            LineItem(sku="KEYBOARD-01", unit_price_cents=12000, quantity=1),
            LineItem(sku="CABLE-USB-C", unit_price_cents=1500, quantity=2)
        ],
        coupon_code="SPRING10"
    )

    total_cents = engine.calculate_order_total(cart)
    print(f"Cart {cart.cart_id} Total: ${total_cents / 100:.2f} {cart.currency}")
    assert total_cents == 14613  # (15000 - 1500) = 13500 + 8.25% tax (1113)
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: Does Python raise a <code>TypeError</code> at runtime if a function receives arguments that don't match its type annotations?</strong></summary>

No. Python's runtime virtual machine completely ignores type annotations during function execution. Type hints are metadata stored in the `__annotations__` dictionary intended for static type checkers (like Mypy and Pyright) and IDEs. If you pass an invalid type at runtime, the code will execute normally until an invalid operation is attempted.
</details>

<details>
<summary><strong>2. Junior: What is the difference between <code>typing.Union[int, str]</code> and <code>int | str</code>?</strong></summary>

Functionally, they are identical. `typing.Union[int, str]` is the legacy syntax introduced in Python 3.5 (PEP 484). `int | str` is the modern union syntax introduced in Python 3.10 (PEP 604). The `|` operator syntax is vastly preferred because it requires no imports from the `typing` module, is more concise, and mirrors mathematical set union notation.
</details>

<details>
<summary><strong>3. Mid: Why should you use <code>collections.abc.Sequence[str]</code> instead of <code>list[str]</code> for function parameter annotations?</strong></summary>

`list[str]` specifies an invariant concrete type. If a caller has a `tuple[str, ...]`, a `deque`, or a custom sequence, they cannot pass it to a `list[str]` parameter without triggering a static type check error. `collections.abc.Sequence[str]` is an abstract interface that accepts any read-only ordered sequence (lists, tuples, ranges). Following the principle of "be liberal in what you accept", parameters should accept abstract collections.
</details>

<details>
<summary><strong>4. Mid: What is the difference between <code>tuple[int]</code> and <code>tuple[int, ...]</code>?</strong></summary>

`tuple[int]` specifies a fixed-length heterogeneous tuple containing **exactly one** element of type integer. `tuple[int, ...]` (using the literal ellipsis `...`) specifies a variable-length homogeneous tuple containing **zero or more** integers.
</details>

<details>
<summary><strong>5. Senior: Why is <code>bool</code> considered a subtype of <code>int</code> by static type checkers, and what problems can this cause?</strong></summary>

In Python's C-level implementation, `bool` inherits directly from `int` (`class bool(int):`), and `True` and `False` are singleton integers with values 1 and 0. Because of the Liskov Substitution Principle, any function expecting an `int` statically accepts a `bool`. This can cause subtle logic bugs (e.g. passing `True` where a count or index was expected). To reject booleans, functions must use explicit runtime `type(val) is int` checks or custom `TypeGuard` guards.
</details>

<details>
<summary><strong>6. Senior: Why should you use <code>typing.get_type_hints()</code> instead of inspecting <code>obj.__annotations__</code> directly?</strong></summary>

1. Forward References: When `from __future__ import annotations` is used, all annotations in `__annotations__` are stored as raw strings. `get_type_hints()` evaluates those strings in the correct global/local namespaces and returns actual type objects.
2. Inheritance: `get_type_hints()` traverses the class MRO and merges inherited annotations from parent classes.
3. Class Wrapper Wrapping: It correctly unpacks wrapped decorators (`__wrapped__`) and `InitVar` containers.
</details>

<details>
<summary><strong>7. Staff: How does Python 3.14's PEP 649 (Deferred Evaluation of Annotations) resolve the shortcomings of PEP 563?</strong></summary>

PEP 563 (`from __future__ import annotations`) turned all annotations into static string literals at compile time. While this eliminated import-time circular reference errors, it broke runtime introspection tools (like Pydantic and dataclasses) which had to parse strings using `eval()`.
PEP 649 (implemented via the standard library `annotationlib` in Python 3.14) replaces stringification with **lazy functions**: annotations are compiled into code objects that execute only when `get_type_hints()` or `__annotate__` is queried. This provides near-instant import times while returning genuine, un-stringified type objects at runtime.
</details>

<details>
<summary><strong>8. Staff: How would you architect a gradual typing adoption plan for a legacy 500,000-line untyped Python codebase?</strong></summary>

1. Setup CI Gate: Add Mypy or Pyright to the CI/CD pipeline with a baseline configuration that ignores existing errors (`mypy --config-file`).
2. Core Layer Typing: Mandate strict type annotations on shared core models, database schemas, and foundational utility libraries first.
3. Feature Rule: Enforce a strict "Boy Scout Rule" in code review: any modified file or new feature must have 100% typed public function signatures.
4. Enable Strictness Incrementally: Turn on strict flags progressively in configuration: `warn_unused_ignores`, `disallow_untyped_defs`, and `check_untyped_defs`.
5. Ban Untyped PRs: Use pre-commit hooks (`pre-commit`) to block new un-annotated code from entering the repository.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Typed Dictionary Filter
**Requirement:** Write a function `filter_positive_scores(scores: Mapping[str, int]) -> dict[str, int]` that accepts any mapping and returns a new dictionary containing only key-value pairs with scores $> 0$.

<details>
<summary>Show Solution</summary>

```python
from collections.abc import Mapping

def filter_positive_scores(scores: Mapping[str, int]) -> dict[str, int]:
    return {k: v for k, v in scores.items() if v > 0}

# Test
input_data: dict[str, int] = {"Alice": 50, "Bob": -10, "Charlie": 0, "David": 95}
result = filter_positive_scores(input_data)
print("Positive Scores:", result)
assert result == {"Alice": 50, "David": 95}
```
</details>

### Exercise 2: Optional Value Safe Extractor
**Requirement:** Write a function `get_first_non_empty(items: Sequence[str | None]) -> str | None` that returns the first non-null, non-empty string in a sequence.

<details>
<summary>Show Solution</summary>

```python
from collections.abc import Sequence

def get_first_non_empty(items: Sequence[str | None]) -> str | None:
    for item in items:
        if item is not None and len(item.strip()) > 0:
            return item.strip()
    return None

# Test
test_seq = [None, "", "  ", "Valid String", "Second"]
res = get_first_non_empty(test_seq)
print("First valid:", res)
assert res == "Valid String"
```
</details>

### Exercise 3: Functional Retry Pipeline with Callables
**Requirement:** Write a higher-order function `retry_operation(operation: Callable[[], T], max_retries: int) -> T` with proper generic type annotations.

<details>
<summary>Show Solution</summary>

```python
from collections.abc import Callable
from typing import TypeVar

T = TypeVar("T")

def retry_operation(operation: Callable[[], T], max_retries: int = 3) -> T:
    last_error: Exception | None = None
    for attempt in range(max_retries):
        try:
            return operation()
        except Exception as e:
            last_error = e
    raise RuntimeError(f"Operation failed after {max_retries} attempts") from last_error

# Test
counter = 0
def flaky_call() -> str:
    global counter
    counter += 1
    if counter < 2:
        raise ValueError("Network glitch")
    return "SUCCESS"

result = retry_operation(flaky_call, max_retries=3)
print("Retry result:", result)
assert result == "SUCCESS"
```
</details>

### Exercise 4: Coordinate Distance Calculator
**Requirement:** Use modern type aliases to define `Point2D = tuple[float, float]` and implement a Euclidean distance function `distance(p1: Point2D, p2: Point2D) -> float`.

<details>
<summary>Show Solution</summary>

```python
import math

type Point2D = tuple[float, float]

def distance(p1: Point2D, p2: Point2D) -> float:
    return math.hypot(p2[0] - p1[0], p2[1] - p1[1])

# Test
pt1: Point2D = (0.0, 0.0)
pt2: Point2D = (3.0, 4.0)
dist = distance(pt1, pt2)
print("Distance:", dist)
assert dist == 5.0
```
</details>

### Exercise 5: Generic Key Extractor
**Requirement:** Write a function `extract_ids[T](items: Sequence[T], id_getter: Callable[[T], str]) -> list[str]` using modern Python 3.12 generic syntax.

<details>
<summary>Show Solution</summary>

```python
from collections.abc import Sequence, Callable

# Python 3.12+ PEP 695 type parameter syntax
def extract_ids[T](items: Sequence[T], id_getter: Callable[[T], str]) -> list[str]:
    return [id_getter(item) for item in items]

# Test
users = [{"id": "usr_101", "name": "Alice"}, {"id": "usr_102", "name": "Bob"}]
ids = extract_ids(users, lambda u: u["id"])
print("Extracted IDs:", ids)
assert ids == ["usr_101", "usr_102"]
```
</details>

### Exercise 6: Runtime Type Annotation Introspector
**Requirement:** Write a function `audit_function_types(func: Callable) -> dict[str, str]` using `typing.get_type_hints()` that returns parameter and return type names as plain strings.

<details>
<summary>Show Solution</summary>

```python
from collections.abc import Callable
from typing import get_type_hints

def audit_function_types(func: Callable) -> dict[str, str]:
    hints = get_type_hints(func)
    return {k: getattr(v, "__name__", str(v)) for k, v in hints.items()}

def sample_endpoint(user_id: int, tags: list[str]) -> bool:
    return True

report = audit_function_types(sample_endpoint)
print("Audited Types:", report)
assert report["user_id"] == "int"
assert report["return"] == "bool"
```
</details>
