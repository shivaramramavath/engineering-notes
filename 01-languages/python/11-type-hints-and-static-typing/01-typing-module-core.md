---
title: "The typing Module Core: Any, Never, Literal, Final, and Type Casting"
description: "Mastering core typing abstractions in Python: Any vs object, Never and NoReturn bottom types, Literal value restriction, Final immutability, ClassVar scope, type casting with cast(), and reveal_type debugging."
module: "11-type-hints-and-static-typing"
order: 1
tags:
  - python
  - typing
  - any
  - never
  - literal
  - final
  - cast
  - type-system
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# The typing Module Core: Any, Never, Literal, Final, and Type Casting

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
10. [Real-World Case Study: State Machine & Exhaustive Pattern Matching Engine](#real-world-case-study-state-machine--exhaustive-pattern-matching-engine)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

While primitive types (`int`, `str`, `list`) define the data shapes of basic programming, modern type systems require special boundary types to represent extreme points in the type lattice:
- **`Any`**: The dynamic escape hatch. Disables all static type checking on the variable.
- **`object`**: The universal top type. Accepts any value, but allows only universal operations (e.g. `str()`, `id()`).
- **`Never` / `NoReturn`**: The bottom type. Represents impossible values, unreachable code paths, and functions that never return (e.g. infinite loops or functions that always raise exceptions).
- **`Literal`**: Restricts a variable strictly to specific fixed values (e.g. `Literal["GET", "POST"]`).
- **`Final`**: Declares that a variable or attribute must not be re-assigned, or a method must not be overridden in subclasses.
- **`cast()`**: Overrides static type checker inference when the developer knows more about an object's runtime type than the compiler can prove.
- **`reveal_type()`**: Standardized debugging helper that instructs static type checkers to emit an informational message showing the inferred type of an expression.

### Motivation
Real-world systems regularly encounter scenarios that standard classes cannot express:
1. **Exhaustive Control Flow**: When checking states in an enum or union, how do you mathematically prove that every possible case has been handled and no branches were forgotten? (`assert_never()`).
2. **Strict API Contracts**: How do you prevent callers from passing arbitrary strings to a database mode parameter when only `"readonly"` or `"readwrite"` are supported? (`Literal`).
3. **Escaping Rigid Type Checkers**: When interacting with dynamic legacy libraries or deserialization layers where static inference fails, how do you safely cast types without runtime overhead? (`typing.cast()`).

### Real-World Analogy
Consider airport security gates:
- **`object`** is an international passenger: security knows they are a human, but they cannot enter restricted boarding zones without showing a boarding pass (type narrowing).
- **`Any`** is a diplomat with universal diplomatic immunity: they bypass all metal detectors and passport checkpoints without inspection. Using `Any` everywhere destroys security.
- **`Never`** is an emergency fire exit door with an armed alarm: if a passenger ever walks through it during normal flight operations, the system immediately sounds a critical alarm (`assert_never`).
- **`Literal`** is a boarding pass stamped `"FIRST_CLASS_ONLY"`.

### When to Use vs When Not to Use
- **Use `Literal` for:** Configuration options, HTTP methods, finite state flags, and tagged union discriminators.
- **Use `Final` for:** Constants, immutable configuration values, and un-overridable methods.
- **Use `Never` / `assert_never` for:** Exhaustive pattern matching checks in `if/elif` or `match/case` blocks.
- **Use `cast()` sparingly:** Only when interacting with dynamic third-party libraries where type narrowing cannot prove the type.
- **Do Not Use:**
  - `Any` as a lazy shortcut to silence type checker warnings (use `object` with type narrowing instead).
  - `NoReturn` for functions that return `None` (a function returning `None` returns a value; use `None` as the return annotation).

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| The Python Type Lattice                                                 |
+-------------------------------------------------------------------------+
                    TOP TYPE: object
                    (Supertype of all types. Accepts everything,
                     permits only base object operations)
                            |
           +----------------+----------------+
           |                                 |
           v                                 v
     int, str, float                  User Classes, DTOs
           |                                 |
           +----------------+----------------+
                            |
                   BOTTOM TYPE: Never
                   (Subtype of all types. Empty set.
                    Represents unreachable states)

SPECIAL GRADUAL TYPING ESCAPE HATCH: Any
(Both a supertype and subtype of everything. Disables static checking.)
```

### `Any` vs `object`
- **`Any`**: Opt-out of typing. You can call arbitrary methods on an `Any` object (`x.foo()`, `x[0]`, `x + 5`) and the type checker allows it without verification.
- **`object`**: Safe top type. You can assign anything to an `object`, but attempting `x.foo()` triggers a static type check error until you narrow the type via `isinstance()`.

---

## How It Works Internally

### 1. `typing.cast()` Zero Runtime Cost
In CPython (`Lib/typing.py`), `cast` is defined as:
```python
def cast(typ, val):
    return val
```
At runtime, `cast()` immediately returns the input value untouched! It does **no validation**, no conversion, and no type coercion. Its sole purpose is to instruct static type checkers (Mypy/Pyright) to treat the returned expression as having type `typ`.

### 2. `assert_never()` and Exhaustive Type Narrowing (PEP 673)
`assert_never(arg)` takes an argument of type `Never`.
During static analysis:
- If a developer handles every case in a `match/case` or `if/elif` chain, the remaining type of the variable after all branches fall through is mathematically the empty set: **`Never`**.
- If a developer adds a new state to a Union (e.g. adding `"CANCELLED"` to an order status) but forgets to handle it in the `if` statement, the type checker evaluates the unhandled branch, sees that the variable has type `Literal["CANCELLED"]` (not `Never`), and raises a compile-time error: `Argument 1 to assert_never has incompatible type "Literal['CANCELLED']"; expected "Never"`.

---

## Syntax & API Reference

### Boundary Types
```python
from typing import Any, Never, NoReturn, Literal, Final, ClassVar, cast, reveal_type, assert_never

# Any vs Object
untyped_data: Any = parse_external_input()
safe_unknown: object = fetch_opaque_blob()

# Bottom Types
def fatal_abort(message: str) -> NoReturn:  # Or -> Never
    raise SystemExit(message)

# Literals (Exact value constraints)
Mode = Literal["read", "write", "append"]
HttpStatus = Literal[200, 400, 404, 500]

# Immutability & Scope
API_KEY: Final[str] = "secret_token_123"

class Model:
    TABLE_NAME: ClassVar[str] = "users"     # Class attribute; not instance field
    name: str                               # Instance attribute
```

---

## Code Examples

### Example 1: `Any` vs `object` Safety Demonstration
```python
from typing import Any

def process_with_any(data: Any):
    # DANGEROUS: Type checker permits this, but it crashes at runtime!
    # Mypy: Success (no errors reported)
    data.non_existent_method()

def process_with_object(data: object):
    # SAFE: Type checker blocks unverified access
    # data.non_existent_method() -> Mypy ERROR: "object" has no attribute "non_existent_method"

    # Must narrow type explicitly:
    if isinstance(data, str):
        print("Data is string with length:", len(data))
    elif isinstance(data, (int, float)):
        print("Data is numeric:", data * 2)
    else:
        print("Data is unhandled object type:", type(data))

def run_any_vs_object():
    process_with_object("Hello Antigravity")
    process_with_object(42)

run_any_vs_object()
# Output:
# Data is string with length: 17
# Data is numeric: 84
```

### Example 2: Strict State Constraints with `Literal`
```python
from typing import Literal

type ConnectionState = Literal["DISCONNECTED", "CONNECTING", "CONNECTED", "RECONNECTING"]

def transition_state(current: ConnectionState, action: Literal["start", "succeed", "fail", "drop"]) -> ConnectionState:
    match (current, action):
        case ("DISCONNECTED", "start"):
            return "CONNECTING"
        case ("CONNECTING", "succeed"):
            return "CONNECTED"
        case ("CONNECTING", "fail"):
            return "DISCONNECTED"
        case ("CONNECTED", "drop"):
            return "RECONNECTING"
        case ("RECONNECTING", "succeed"):
            return "CONNECTED"
        case ("RECONNECTING", "fail"):
            return "DISCONNECTED"
        case _:
            raise ValueError(f"Illegal state transition from {current} via {action}")

def run_literal_demo():
    s1: ConnectionState = "DISCONNECTED"
    s2 = transition_state(s1, "start")
    s3 = transition_state(s2, "succeed")
    print(f"State transition flow: {s1} -> {s2} -> {s3}")
    assert s3 == "CONNECTED"

run_literal_demo()
# Output:
# State transition flow: DISCONNECTED -> CONNECTING -> CONNECTED
```

### Example 3: Exhaustive Matching with `assert_never()`
```python
from typing import Literal, assert_never

type Shape = Literal["circle", "square", "triangle"]

def calculate_perimeter(shape: Shape, size: float) -> float:
    match shape:
        case "circle":
            return 2 * 3.14159 * size
        case "square":
            return 4 * size
        case "triangle":
            return 3 * size
        case _ as unreachable:
            # If a developer adds 'hexagon' to Shape above, Mypy flags this line!
            assert_never(unreachable)

def run_exhaustive_demo():
    c_perim = calculate_perimeter("circle", 10.0)
    s_perim = calculate_perimeter("square", 5.0)
    print(f"Circle Perimeter: {c_perim:.2f}")
    print(f"Square Perimeter: {s_perim:.2f}")

run_exhaustive_demo()
# Output:
# Circle Perimeter: 62.83
# Square Perimeter: 20.00
```

### Example 4: Override Static Inference with `typing.cast()`
```python
from typing import cast
import json

def fetch_raw_user() -> object:
    # Simulates an external API returning untyped data
    return json.loads('{"id": 104, "username": "alice", "active": true}')

def run_cast_demo():
    raw_data: object = fetch_raw_user()

    # The type checker knows only that raw_data is 'object'
    # raw_data["username"] -> Mypy Error: Value of type "object" is not indexable

    # Developer cast: We tell the static type checker what type it actually is
    user_dict = cast(dict[str, Any], raw_data)

    print("Extracted username via cast:", user_dict["username"])
    print("User ID:", user_dict["id"])

run_cast_demo()
# Output:
# Extracted username via cast: alice
# User ID: 104
```

### Example 5: Class Constants and Immutability with `Final` and `ClassVar`
```python
from typing import Final, ClassVar

DATABASE_PORT: Final[int] = 5432
# DATABASE_PORT = 9999 -> Mypy ERROR: Cannot assign to final name "DATABASE_PORT"

class BaseWorker:
    # ClassVar belongs strictly to the class, not instance namespaces
    THREAD_POOL_NAME: ClassVar[str] = "DEFAULT_WORKERS"

    def __init__(self, worker_id: int):
        self.worker_id: Final[int] = worker_id  # Cannot be mutated after init

    def process(self):
        print(f"Worker {self.worker_id} running in pool {self.THREAD_POOL_NAME}")

def run_final_demo():
    worker = BaseWorker(404)
    worker.process()
    print("Pool name from class:", BaseWorker.THREAD_POOL_NAME)

run_final_demo()
# Output:
# Worker 404 running in pool DEFAULT_WORKERS
# Pool name from class: DEFAULT_WORKERS
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. The `cast()` Illusion
`typing.cast(TargetType, val)` does **NOT** perform any runtime conversion!
```python
val = "123"
# cast does NOT turn "123" into integer 123!
bad_int = cast(int, val)
print(type(bad_int))  # Still <class 'str'>!
```
If you pass the wrong type to `cast()`, you fool the type checker while introducing runtime bugs. Use genuine type converters (`int(val)`) when runtime coercion is needed.

### 2. Using `NoReturn` for Functions that Return `None`
In Python, a function that finishes without a `return` statement implicitly executes `return None`.
- **Correct**: `def log_event(msg: str) -> None:`
- **Incorrect**: `def log_event(msg: str) -> NoReturn:`
`NoReturn` (or `Never`) must be reserved strictly for functions that **never finish normally** (e.g. `sys.exit()`, `os.abort()`, or an infinite server loop `while True:`).

### 3. Mutating `Final` Objects at Runtime
`Final` is a static type checking enforcement, not an operating system memory write-lock.
At runtime, Python permits re-assignment:
```python
X: Final[int] = 10
X = 20  # Mypy flags error, but Python executes without error!
```
To achieve true runtime immutability, use frozen dataclasses or read-only properties.

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `def f() -> NoReturn: pass` | `def f() -> None: pass` | `pass` returns `None`; `NoReturn` is for functions that never return. | High |
| `val = cast(int, "42")` expecting int | `val = int("42")` | `cast()` is a static type hint; does zero runtime type conversion. | Critical |
| `def process(x: Any):` everywhere | `def process(x: object):` with narrowing | `Any` completely disables type safety, defeating the purpose of typing. | High |
| Incomplete union matching without default | Exhaustive check with `assert_never` | Adding new union variants creates silent unhandled branch bugs. | High |
| Class attribute annotated as normal type | Annotate with `ClassVar[T]` | Prevents dataclasses and ORMs from mistaking it for an instance field. | Medium |
| Using `reveal_type()` in production code | Remove before committing to main | `reveal_type` is a debugging construct and raises NameError in older Python. | Low |

---

## Performance & Memory Characteristics

### Runtime Overhead
- `Any`, `Never`, `Literal`, `Final`, `ClassVar`: Zero runtime memory overhead.
- `cast(typ, val)`: Costs exactly one function call returning `val` ($\approx 25 \text{ ns}$ overhead).
- `assert_never(arg)`: Instantiates and raises `AssertionError` only when an illegal state is hit.

---

## CPython Internals & Bytecode

In CPython:
- `typing` special forms (`Any`, `Never`, `Literal`, `Final`) are instances of `typing._SpecialForm`.
- `_SpecialForm` objects override `__getitem__` to validate arguments without creating classes.
- When `from __future__ import annotations` is used, the special forms are not even evaluated at import time.

Bytecode for `cast`:
```python
import dis
from typing import cast

def convert(x):
    return cast(int, x)

dis.dis(convert)
```
```text
  1           0 RESUME                   0
  2           2 LOAD_GLOBAL              1 (cast + NULL)
             12 LOAD_GLOBAL              3 (int + NULL)
             22 LOAD_FAST                0 (x)
             24 CALL                     2
             32 RETURN_VALUE
```
Notice that at runtime, `cast` is simply a standard function call returning its argument.

---

## Best Practices & Idiomatic Patterns

1. **Use `object` Instead of `Any` as a Safe Default**:
   Force yourself to prove the object's type via `isinstance` checks before accessing attributes.
2. **Pair `Literal` with Pattern Matching**:
   ```python
   match status:
       case "PENDING": ...
       case "CONFIRMED": ...
       case _ as unhandled:
           assert_never(unhandled)
   ```
3. **Use `Final` on Module Constants**:
   Communicates intent to other developers and prevents accidental rebinding.
4. **Use `reveal_type()` During Development**:
   Place `reveal_type(expr)` in your code and run `mypy file.py` to inspect complex inferred types.

---

## Real-World Case Study: State Machine & Exhaustive Pattern Matching Engine

The following transactional order state machine uses `Literal`, `Final`, `assert_never`, and type narrowing to guarantee compile-time verification of all legal lifecycle transitions.

```python
from typing import Literal, Final, assert_never, NamedTuple
import time

# Finite state domain model
type OrderState = Literal["DRAFT", "PENDING_PAYMENT", "PAID", "SHIPPED", "CANCELLED"]

class OrderTransition(NamedTuple):
    order_id: str
    from_state: OrderState
    to_state: OrderState
    timestamp: float

class OrderStateMachine:
    TERMINAL_STATES: Final[frozenset[OrderState]] = frozenset({"SHIPPED", "CANCELLED"})

    @staticmethod
    def transition(order_id: str, current: OrderState, event: Literal["submit", "pay", "ship", "cancel"]) -> OrderTransition:
        now = time.time()

        match current:
            case "DRAFT":
                if event == "submit":
                    return OrderTransition(order_id, current, "PENDING_PAYMENT", now)
                elif event == "cancel":
                    return OrderTransition(order_id, current, "CANCELLED", now)

            case "PENDING_PAYMENT":
                if event == "pay":
                    return OrderTransition(order_id, current, "PAID", now)
                elif event == "cancel":
                    return OrderTransition(order_id, current, "CANCELLED", now)

            case "PAID":
                if event == "ship":
                    return OrderTransition(order_id, current, "SHIPPED", now)
                elif event == "cancel":
                    return OrderTransition(order_id, current, "CANCELLED", now)

            case "SHIPPED" | "CANCELLED":
                raise ValueError(f"Cannot transition from terminal state: {current}")

            case _ as unhandled_state:
                # Compile-time exhaustiveness check!
                # If a developer adds 'REFUNDED' to OrderState, Mypy flags this line immediately.
                assert_never(unhandled_state)

        raise ValueError(f"Illegal event '{event}' for state '{current}'")

# Verification
if __name__ == "__main__":
    sm = OrderStateMachine()
    
    t1 = sm.transition("ORD-1", "DRAFT", "submit")
    t2 = sm.transition("ORD-1", t1.to_state, "pay")
    t3 = sm.transition("ORD-1", t2.to_state, "ship")

    print(f"Order Transition Sequence: {t1.to_state} -> {t2.to_state} -> {t3.to_state}")
    assert t3.to_state == "SHIPPED"
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: What is the difference between <code>Any</code> and <code>object</code> in Python's type system?</strong></summary>

- `object` is the top type of Python's class hierarchy. Any value can be assigned to `object`, but static type checkers permit only operations common to all objects (e.g. `str()`, `id()`). You cannot call arbitrary methods on an `object` without first narrowing its type via `isinstance()`.
- `Any` is an escape hatch from the static type system. When a variable is typed as `Any`, the type checker disables all checks: you can call arbitrary methods, access arbitrary attributes, and pass it anywhere without errors.
</details>

<details>
<summary><strong>2. Junior: What is the purpose of <code>typing.Literal</code>?</strong></summary>

`typing.Literal` restricts a variable to one or more specific primitive values (strings, integers, booleans, or enums). For example, `mode: Literal["r", "w", "a"]` forces callers to provide exactly one of those three literal string values, preventing invalid arguments at compile time.
</details>

<details>
<summary><strong>3. Mid: What is the difference between <code>NoReturn</code> and <code>None</code> as return type annotations?</strong></summary>

- `-> None`: The function completes its execution normally and returns the singleton value `None` (either implicitly or explicitly via `return None`).
- `-> NoReturn` (or `Never`): The function **never returns**. Execution either terminates the process (e.g. `sys.exit()`), enters an infinite loop, or always raises an exception.
</details>

<details>
<summary><strong>4. Mid: What does <code>typing.cast()</code> do at runtime?</strong></summary>

At runtime, `typing.cast(target_type, value)` does absolutely nothing except return `value` unchanged. It does **not** validate, coerce, or convert data. It is purely a static directive instructing type checkers (Mypy/Pyright) to treat the expression as `target_type`.
</details>

<details>
<summary><strong>5. Senior: How does <code>typing.assert_never()</code> enforce compile-time exhaustiveness in pattern matching?</strong></summary>

`assert_never(arg)` expects an argument of type `Never` (the bottom type representing an empty set of values). In an exhaustive `match/case` statement covering all members of a Union, the type checker narrows the type after every handled branch. If all cases are handled, the variable in the default `case _` has type `Never`, satisfying `assert_never`. If a case was forgotten, the variable retains the unhandled type, causing the static type checker to fail compilation.
</details>

<details>
<summary><strong>6. Senior: What is the difference between <code>typing.Final</code> and <code>typing.ClassVar</code>?</strong></summary>

- `Final`: Declares that a variable, attribute, or method cannot be re-assigned or overridden. Can be applied to module constants, class attributes, or instance attributes.
- `ClassVar`: Declares that an attribute belongs strictly to the **class namespace**, not instance namespaces. Prevents dataclasses, Pydantic, and IDEs from generating instance constructor arguments for that attribute.
</details>

<details>
<summary><strong>7. Staff: How does Python 3.12's PEP 695 <code>type</code> statement improve upon <code>typing.TypeAlias</code>?</strong></summary>

Prior to Python 3.12, type aliases were declared using assignments (`Vector: TypeAlias = list[float]`). This evaluated the alias immediately at module import time, causing circular import errors and namespace pollution.
PEP 695 introduces the dedicated `type` statement:
`type Vector[T] = list[T]`
This is lazily evaluated (deferred), supports native generic parameters without `TypeVar` boilerplate, and creates a clean `TypeAliasType` object with proper scoping.
</details>

<details>
<summary><strong>8. Staff: How does <code>reveal_type()</code> work during development, and what is its CPython runtime status?</strong></summary>

`reveal_type(expression)` is a pseudo-function recognized by static type checkers. When Mypy or Pyright runs, it prints an informational diagnostic showing the exact inferred type of `expression`.
In Python 3.11+, `reveal_type` was added to the built-in namespace, so it can be executed at runtime (printing the runtime type and returning the value). In older Python versions, leaving `reveal_type()` in code raised a `NameError` at runtime unless imported or mocked.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Literal HTTP Method Dispatcher
**Requirement:** Define a type alias `HttpMethod = Literal["GET", "POST", "PUT", "DELETE"]` and implement a function `dispatch_request(method: HttpMethod, url: str) -> str`.

<details>
<summary>Show Solution</summary>

```python
from typing import Literal

type HttpMethod = Literal["GET", "POST", "PUT", "DELETE"]

def dispatch_request(method: HttpMethod, url: str) -> str:
    return f"Dispatching {method} to {url}"

# Test
print(dispatch_request("GET", "https://api.com/users"))
print(dispatch_request("POST", "https://api.com/users"))
assert dispatch_request("DELETE", "https://api.com/1") == "Dispatching DELETE to https://api.com/1"
```
</details>

### Exercise 2: Safe Type Narrower from `object`
**Requirement:** Write a function `stringify_safe(val: object) -> str` that inspects `val` and returns formatted strings for `int`, `list`, or `dict`, raising `ValueError` for unsupported types without using `Any`.

<details>
<summary>Show Solution</summary>

```python
def stringify_safe(val: object) -> str:
    if isinstance(val, int):
        return f"INTEGER: {val}"
    elif isinstance(val, list):
        return f"LIST(len={len(val)}): {val}"
    elif isinstance(val, dict):
        return f"DICT(keys={list(val.keys())})"
    raise ValueError(f"Unsupported type: {type(val).__name__}")

# Test
print(stringify_safe(42))
print(stringify_safe([1, 2, 3]))
print(stringify_safe({"a": 1}))
assert stringify_safe(42) == "INTEGER: 42"
```
</details>

### Exercise 3: Exhaustive Enum Validator with `assert_never`
**Requirement:** Implement a function that calculates discount rates for an explicit user tier union, using `assert_never` to enforce compile-time exhaustiveness.

<details>
<summary>Show Solution</summary>

```python
from typing import Literal, assert_never

type UserTier = Literal["BRONZE", "SILVER", "GOLD"]

def get_tier_discount(tier: UserTier) -> float:
    match tier:
        case "BRONZE":
            return 0.05
        case "SILVER":
            return 0.15
        case "GOLD":
            return 0.25
        case _ as unhandled:
            assert_never(unhandled)

# Test
assert get_tier_discount("BRONZE") == 0.05
assert get_tier_discount("GOLD") == 0.25
print("Exhaustive tier discounts verified.")
```
</details>

### Exercise 4: Final Immutable Registry
**Requirement:** Create a class `SystemSettings` with `Final` attributes and a `ClassVar` version identifier.

<details>
<summary>Show Solution</summary>

```python
from typing import Final, ClassVar

class SystemSettings:
    VERSION: ClassVar[str] = "v3.0.0"

    def __init__(self, cluster_id: str, max_nodes: int):
        self.cluster_id: Final[str] = cluster_id
        self.max_nodes: Final[int] = max_nodes

cfg = SystemSettings("cluster-us-west", 100)
print(f"Settings: {cfg.cluster_id}, Max Nodes: {cfg.max_nodes}, Ver: {SystemSettings.VERSION}")
assert cfg.cluster_id == "cluster-us-west"
assert SystemSettings.VERSION == "v3.0.0"
```
</details>

### Exercise 5: Cast Override for JSON Deserialization
**Requirement:** Use `typing.cast` to safely inform static type checkers of an untyped dictionary's internal structure after validating its keys with `isinstance`.

<details>
<summary>Show Solution</summary>

```python
from typing import cast, Any

def parse_user_payload(raw_json: object) -> dict[str, str]:
    if not isinstance(raw_json, dict):
        raise TypeError("Payload must be a dictionary")
    
    # Cast after validation
    typed_dict = cast(dict[str, str], raw_json)
    return typed_dict

# Test
raw = {"name": "Alice", "role": "admin"}
user = parse_user_payload(raw)
print("Parsed user name:", user["name"])
assert user["name"] == "Alice"
```
</details>

### Exercise 6: Unreachable Error Guard with `Never`
**Requirement:** Write a function `fail_with_code(code: int, msg: str) -> Never` that logs the error and raises `SystemExit`.

<details>
<summary>Show Solution</summary>

```python
from typing import Never
import sys

def fail_with_code(code: int, msg: str) -> Never:
    print(f"FATAL [{code}]: {msg}", file=sys.stderr)
    raise SystemExit(code)

# Test via catching SystemExit
try:
    fail_with_code(1, "Database connection unreachable")
except SystemExit as err:
    print("Caught expected exit code:", err.code)
    assert err.code == 1
```
</details>
