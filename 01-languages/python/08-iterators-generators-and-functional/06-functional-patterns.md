# Module 08: Iterators, Generators & Functional Programming (06-functional-patterns)

> **Topic Overview**: Python is a multi-paradigm language where functions are first-class objects. While avoiding pure academic functional constraints, Python embraces pragmatic functional programming patterns—pure functions, higher-order functions, partial application via `functools.partial`, function composition pipelines, and immutable data flows—to build decoupled, testable architectures.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Functional Programming |
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

- **Problem Statement**: Imperative code reliant on mutable global or object state creates hidden side effects, making concurrency treacherous, refactoring brittle, and unit testing difficult.
- **Mental Model**:
  - A **pure function** is a mathematical mapping: given the same inputs, it *always* returns the exact same output, producing zero observable side effects (no mutating global variables, no I/O, no changing arguments in place).
  - **Higher-order functions** treat functions as data: passing functions as arguments, returning functions from functions, and composing them into reusable pipelines.
- **When to Use**:
  - Data transformation pipelines and ETL tasks.
  - Designing stateless business logic that scales seamlessly across threads or processes.
  - Pre-binding function parameters with `functools.partial`.
- **When NOT to Use**: Do not force complex tacit / point-free functional chaining or deep recursive structures when a simple Python comprehension or loop is more readable.

---

## 2. Core Concepts

### First-Class Functions & Higher-Order Functions
In Python, functions are instances of the `function` class (`PyFunctionObject`):
1. Can be assigned to variables (`f = math.sqrt`).
2. Can be stored in collections (`handlers = {"json": parse_json}`).
3. Can be passed as arguments (`sorted(users, key=get_age)`).
4. Can be returned from other functions (closures and decorators).

### Pure Functions vs Side Effects
```text
+-----------------------+-------------------------------------------------------+
| Characteristic        | Pure Function                                         |
+-----------------------+-------------------------------------------------------+
| Determinism           | Given x, always returns f(x)                          |
| State Mutation        | Zero mutation of inputs or global state               |
| I/O Operations        | No file, network, or console operations               |
| Concurrency Safety    | 100% thread-safe without locks                        |
| Testability           | Trivial: assert f(input) == expected                  |
+-----------------------+-------------------------------------------------------+
```

### The Functional Triad: `map`, `filter`, `reduce`
- `map(func, iterable)`: Lazily applies `func` to every item in `iterable`.
- `filter(pred, iterable)`: Lazily yields items where `pred(item)` is truthy.
- `functools.reduce(func, iterable[, initial])`: Eagerly folds elements left-to-right into a single accumulated result: $f(f(f(s_0, s_1), s_2), s_3)$.

---

## 3. How It Works Internally

`functools.partial` is implemented in C (`Modules/_functoolsmodule.c`) as a native callable object rather than a Python closure:

```text
+---------------------------------------------------------------+
| partial Object Memory Layout                                  |
+---------------------------------------------------------------+
| ob_refcnt : 1                                                 |
| ob_type   : &PyPartial_Type                                   |
| fn        : &PyFunctionObject (Target function)               |
| args      : PyTupleObject (Pre-bound positional arguments)    |
| kw        : PyDictObject (Pre-bound keyword arguments)        |
+---------------------------------------------------------------+

When partial(*call_args, **call_kw) is executed:
  1. Combines pre-bound args with call_args into a single tuple.
  2. Merges pre-bound kw with call_kw into a single dictionary.
  3. Calls target function directly in C without Python closure overhead.
```

---

## 4. Syntax & API Reference

### Partial Application (`functools.partial`)
```python
import functools

def multiply(a: int, b: int) -> int:
    return a * b

# Pre-bind 'a=2' to create a dedicated 'double' function
double = functools.partial(multiply, 2)
result = double(10)  # 20
```

### Reduction (`functools.reduce`)
```python
from functools import reduce
import operator

# Product of all elements: 1 * 2 * 3 * 4 = 24
total_product = reduce(operator.mul, [1, 2, 3, 4], 1)
```

### Function Composition Utility
```python
def compose(*functions):
    """Composes functions right-to-left: compose(f, g)(x) == f(g(x))."""
    return functools.reduce(lambda f, g: lambda x: f(g(x)), functions)
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Pure Data Transformation Pipeline
```python
from typing import NamedTuple

class Product(NamedTuple):
    name: str
    price: float
    category: str

catalog = [
    Product("Laptop", 1200.0, "electronics"),
    Product("Desk Chair", 250.0, "furniture"),
    Product("Mouse", 45.0, "electronics"),
    Product("Lamp", 35.0, "furniture")
]

# Pure transformation functions
def is_electronics(p: Product) -> bool:
    return p.category == "electronics"

def apply_tax(p: Product, tax_rate: float = 0.08) -> Product:
    return Product(p.name, round(p.price * (1.0 + tax_rate), 2), p.category)

# Composed functional pipeline
taxed_electronics = [
    apply_tax(p)
    for p in catalog
    if is_electronics(p)
]

print("Taxed Electronics:")
for item in taxed_electronics:
    print(f"  {item.name}: ${item.price}")
```
**Output**:
```text
Taxed Electronics:
  Laptop: $1296.0
  Mouse: $48.6
```

### 2. Real-World Production Example: Partial Application in Task Dispatch
```python
import functools
from typing import Callable

def send_notification(service: str, priority: str, recipient: str, message: str) -> str:
    return f"[{service.upper()}][{priority}] Sent to {recipient}: {message}"

# Create specialized pre-bound dispatchers
slack_alert = functools.partial(send_notification, "Slack", "P1-CRITICAL")
email_digest = functools.partial(send_notification, "Email", "P3-LOW")

# Callers only provide recipient and message
msg1 = slack_alert("channel-devops", "Database replica connection dropped!")
msg2 = email_digest("user@example.com", "Weekly analytics report ready")

print(msg1)
print(msg2)
```
**Output**:
```text
[SLACK][P1-CRITICAL] Sent to channel-devops: Database replica connection dropped!
[EMAIL][P3-LOW] Sent to user@example.com: Weekly analytics report ready
```

### 3. Edge Case: Mutating Argument in Supposedly Pure Function
```python
def impure_sort(items: list[int]) -> list[int]:
    # BUG: items.sort() mutates the caller's list in place!
    items.sort()
    return items

def pure_sort(items: list[int]) -> list[int]:
    # PURE: sorted() creates a new list, leaving input unchanged
    return sorted(items)

original = [5, 2, 8, 1]
result_pure = pure_sort(original)
print("After pure_sort:", original)  # Unmodified: [5, 2, 8, 1]

result_impure = impure_sort(original)
print("After impure_sort:", original)  # Mutated in place!
```
**Output**:
```text
After pure_sort: [5, 2, 8, 1]
After impure_sort: [1, 2, 5, 8]
```

### 4. Failure Case & Exception Handling: `reduce()` on Empty Sequence
```python
from functools import reduce
import operator

empty_data: list[int] = []

try:
    # Calling reduce on an empty iterable without an initial value raises TypeError!
    reduce(operator.add, empty_data)
except TypeError as err:
    print(f"Captured expected error: {err}")

# Correct approach: Always provide initial default value
safe_result = reduce(operator.add, empty_data, 0)
print(f"Safe result with initial value: {safe_result}")
```
**Output**:
```text
Captured expected error: reduce() of empty iterable with no initial value
Safe result with initial value: 0
```

### 5. Idiomatic Pythonic Pattern: Pipeline Composition Helper
```python
from typing import Callable, Any

class Pipeline:
    def __init__(self, *stages: Callable[[Any], Any]) -> None:
        self.stages = stages

    def execute(self, initial_value: Any) -> Any:
        value = initial_value
        for stage in self.stages:
            value = stage(value)
        return value

# Independent pure stages
strip_whitespace = lambda s: s.strip()
to_lowercase = lambda s: s.lower()
remove_punctuation = lambda s: "".join(c for c in s if c.isalnum() or c.isspace())

text_cleaner = Pipeline(strip_whitespace, to_lowercase, remove_punctuation)
cleaned = text_cleaner.execute("   Hello, World! Welcome to Python 3.14.   ")
print("Cleaned text:", f"'{cleaned}'")
```
**Output**:
```text
Cleaned text: 'hello world welcome to python 314'
```

---

## 6. Edge Cases & Pitfalls

1. **`map` and `filter` Return Lazy Iterators**:
   - In Python 3, `map()` and `filter()` return lazy iterator objects, not lists. If printed directly, you see `<map object at 0x...>`. They can only be iterated once.
2. **List Comprehensions vs `map`/`filter`**:
   - In modern Python, `[f(x) for x in seq if p(x)]` is strongly preferred over `list(map(f, filter(p, seq)))` by Guido van Rossum and PEP 8 for readability and avoiding extra lambda overhead.
3. **`functools.partial` Attribute Preservation**:
   - Unlike standard functions, `partial` objects do not possess `__name__` or `__doc__` attributes directly (they store `func.__name__` under `partial.func`). Use `functools.update_wrapper` if decorator compatibility is required.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Calling `reduce()` without `initial` value | Crashes if sequence is unexpectedly empty | `TypeError: reduce() of empty iterable with no initial value` | Always provide `initial` parameter |
| In-place mutations in pure functions | Mutates caller state; breaks referential transparency and threading | Subtle data corruption | Return fresh copies / use immutable structures |
| Nesting multiple `map(lambda ...)` calls | Decreases readability and incurs extra bytecode frame overhead | Unreadable code / PEP 8 violation | Use list/generator comprehensions |
| Re-iterating a `map` or `filter` object | They are iterators and become exhausted after the first pass | Empty second traversal | Convert to list if reuse is needed |
| Over-using currying in Python | Python syntax is not optimized for curried calls `f(x)(y)(z)` | Clunky syntax / performance penalty | Use `functools.partial` or keyword arguments |
| Writing complex lambdas with assignments | Lambdas in Python are syntactically restricted to single expressions | `SyntaxError: invalid syntax` | Define a named `def` function |

---

## 8. Performance & Complexity

- **Time Complexity**:
  - `map()` / `filter()`: $O(1)$ startup, $O(N)$ total iteration.
  - `functools.reduce()`: $O(N)$ sequential operations.
- **`functools.partial` Dispatch**: Evaluates in compiled C, taking $\approx 20-30$ ns per call (slightly faster than pure-Python closure wrappers).
- **List Comprehension vs `map(lambda)`**: Comprehensions are typically $15-25\%$ faster than `map(lambda x: ...)` because they avoid Python function call frame overhead on every item.

---

## 9. CPython Internals & Bytecode

Let's inspect how `functools.partial` compares to a closure:

```python
import functools
import dis

def add(x, y):
    return x + y

# Partial implementation
partial_add = functools.partial(add, 10)

# Closure implementation
def closure_add(y):
    return add(10, y)

dis.dis(closure_add)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 LOAD_GLOBAL              1 (add)
             12 LOAD_CONST               1 (10)
             14 LOAD_FAST                0 (y)
             16 CALL                     2
             24 RETURN_VALUE
```
While `closure_add` runs through 6 bytecode instructions including `LOAD_GLOBAL`, `partial_add` executes directly in C using `partial_call()` without executing interpreter opcodes for parameter merging.

---

## 10. Best Practices

1. **Default to Comprehensions**: Prefer list/generator comprehensions over `map()` and `filter()` with `lambda`.
2. **Always Pass `initial` to `reduce`**: Avoid crashes on empty collections by always providing an initial accumulator value.
3. **Use `NamedTuple` or Frozen Dataclasses**: Enforce data immutability by default using `typing.NamedTuple` or `@dataclass(frozen=True)`.

---

## 11. Real-World Case Study: Functional Event Dispatcher & Reducer

```python
from typing import NamedTuple, Callable
from functools import reduce

class Action(NamedTuple):
    type: str
    payload: dict

class State(NamedTuple):
    user_count: int
    logged_in_users: tuple[str, ...]

# Pure reducer function
def auth_reducer(state: State, action: Action) -> State:
    match action.type:
        case "USER_LOGIN":
            username = action.payload["username"]
            if username not in state.logged_in_users:
                return State(
                    user_count=state.user_count + 1,
                    logged_in_users=(*state.logged_in_users, username)
                )
            return state
        case "USER_LOGOUT":
            username = action.payload["username"]
            new_users = tuple(u for u in state.logged_in_users if u != username)
            return State(
                user_count=len(new_users),
                logged_in_users=new_users
            )
        case _:
            return state

# Initial state
initial_state = State(user_count=0, logged_in_users=())

# Action stream
actions = [
    Action("USER_LOGIN", {"username": "alice"}),
    Action("USER_LOGIN", {"username": "bob"}),
    Action("USER_LOGIN", {"username": "alice"}),  # Duplicate, ignored by pure logic
    Action("USER_LOGOUT", {"username": "alice"}),
]

# Compute final state purely via reduce
final_state = reduce(auth_reducer, actions, initial_state)
print("Final State User Count:", final_state.user_count)
print("Logged in users:       ", final_state.logged_in_users)
```
**Output**:
```text
Final State User Count: 1
Logged in users:        ('bob',)
```

---

## 12. Interview Questions

### Q1: What defines a pure function and why are pure functions valuable in concurrent systems?
<details>
<summary><strong>Answer</strong></summary>
A pure function is deterministic (its output depends solely on its inputs) and side-effect free (it does not mutate external state, I/O, or input arguments). In concurrent systems, pure functions are inherently thread-safe because they operate exclusively on immutable data or thread-local frames, eliminating race conditions, deadlocks, and the need for mutex synchronization.
</details>

### Q2: What is partial application and how does `functools.partial` differ from default arguments?
<details>
<summary><strong>Answer</strong></summary>
Partial application creates a new callable by pre-binding a subset of arguments to an existing function. Unlike default arguments, which are defined statically at function definition time, <code>functools.partial</code> is created dynamically at runtime, allowing functions to be customized and passed to higher-order APIs on-the-fly.
</details>

### Q3: Why is `reduce()` no longer a built-in function in Python 3?
<details>
<summary><strong>Answer</strong></summary>
Guido van Rossum moved <code>reduce()</code> to the <code>functools</code> module in Python 3 because code relying heavily on nested <code>reduce()</code> calls is often difficult to read and debug. In most scenarios, explicit loops, generator expressions, or specialized built-ins (such as <code>sum()</code>, <code>any()</code>, <code>all()</code>, or <code>max()</code>) are more readable.
</details>

### Q4: What happens if `reduce()` is called on an iterable with only one element and no initial value?
<details>
<summary><strong>Answer</strong></summary>
<code>reduce()</code> returns that single element immediately without calling the reduction function. If the iterable is empty and no initial value is provided, it raises a <code>TypeError</code>.
</details>

### Q5: How do `map()` and `filter()` behave differently in Python 3 compared to Python 2?
<details>
<summary><strong>Answer</strong></summary>
In Python 2, <code>map()</code> and <code>filter()</code> returned materialized lists. In Python 3, they return lazy iterator objects that compute elements on-demand with $O(1)$ memory overhead and can only be consumed once.
</details>

### Q6: What is referential transparency?
<details>
<summary><strong>Answer</strong></summary>
An expression is referentially transparent if it can be replaced with its evaluated result without altering the program's behavior. Pure functions guarantee referential transparency, enabling safe compiler optimizations, memoization, and dead-code elimination.
</details>

### Q7: Can `functools.partial` objects accept new keyword arguments that override pre-bound keywords?
<details>
<summary><strong>Answer</strong></summary>
Yes. If a keyword argument is pre-bound in <code>partial</code>, passing that same keyword argument in a subsequent call will override the pre-bound value: <code>p = partial(f, mode="fast"); p(mode="safe")</code> will invoke <code>f(mode="safe")</code>.
</details>

### Q8: What is currying and how does it differ from partial application?
<details>
<summary><strong>Answer</strong></summary>
Currying transforms a function taking $N$ arguments into a chain of $N$ functions that each take exactly one argument: $f(a, b, c) \to f(a)(b)(c)$. Partial application fixes an arbitrary number of arguments to produce a function taking the remaining arguments in one call: $f(a, b, c) \to f'(b, c)$.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Composing Discounts and Tax
Given a price, create a pipeline using two pure functions: `apply_discount(price, pct=0.1)` and `add_vat(price, vat=0.2)`. Apply both to compute the final price.
<details>
<summary><strong>Solution</strong></summary>

```python
def apply_discount(price: float, pct: float = 0.1) -> float:
    return price * (1.0 - pct)

def add_vat(price: float, vat: float = 0.2) -> float:
    return price * (1.0 + vat)

# $100 -> $90 -> $108
final_price = add_vat(apply_discount(100.0, 0.1), 0.2)
assert round(final_price, 2) == 108.00
```
</details>

### Challenge 2 (Easy): Pre-bound Power Function
Using `functools.partial` and `math.pow` (or `operator.pow`), create two specialized functions: `square(x)` (computes $x^2$) and `cube(x)` (computes $x^3$).
<details>
<summary><strong>Solution</strong></summary>

```python
import functools

def power(base: float, exponent: float) -> float:
    return base ** exponent

# Pre-bind exponent as keyword argument
square = functools.partial(power, exponent=2)
cube = functools.partial(power, exponent=3)

assert square(4) == 16
assert cube(3) == 27
```
</details>

### Challenge 3 (Medium): Generic Function Composer
Write a function `compose(*funcs)` that takes an arbitrary number of single-argument functions and returns a new function executing them in right-to-left order: `compose(f, g, h)(x) == f(g(h(x)))`.
<details>
<summary><strong>Solution</strong></summary>

```python
from functools import reduce

def compose(*funcs):
    return reduce(lambda f, g: lambda x: f(g(x)), funcs)

add_one = lambda x: x + 1
double = lambda x: x * 2
square = lambda x: x ** 2

# (x^2 * 2) + 1 -> for 3: (9 * 2) + 1 = 19
pipeline = compose(add_one, double, square)
assert pipeline(3) == 19
```
</details>

### Challenge 4 (Medium): Custom Partial Implementation
Implement your own class `MyPartial` that mimics `functools.partial`, supporting pre-bound positional arguments, pre-bound keyword arguments, and overriding keyword arguments on call.
<details>
<summary><strong>Solution</strong></summary>

```python
class MyPartial:
    def __init__(self, func, *args, **kwargs) -> None:
        self.func = func
        self.args = args
        self.kwargs = kwargs

    def __call__(self, *more_args, **more_kwargs):
        merged_args = self.args + more_args
        merged_kwargs = {**self.kwargs, **more_kwargs}
        return self.func(*merged_args, **merged_kwargs)

def greet(greeting, name, punctuation="."):
    return f"{greeting}, {name}{punctuation}"

hello_john = MyPartial(greet, "Hello", "John")
assert hello_john() == "Hello, John."
assert hello_john(punctuation="!") == "Hello, John!"
```
</details>

### Challenge 5 (Hard): Deep Immutability Freeze Wrapper
Implement a function `deep_freeze(obj)` that recursively converts nested dictionaries into `types.MappingProxyType` and nested lists into `tuple`s, returning an entirely immutable, hashable, or read-only structure.
<details>
<summary><strong>Solution</strong></summary>

```python
from types import MappingProxyType

def deep_freeze(obj):
    if isinstance(obj, dict):
        return MappingProxyType({k: deep_freeze(v) for k, v in obj.items()})
    elif isinstance(obj, list):
        return tuple(deep_freeze(x) for x in obj)
    elif isinstance(obj, set):
        return frozenset(deep_freeze(x) for x in obj)
    return obj

data = {"user": "Alice", "roles": ["admin", "editor"], "meta": {"active": True}}
frozen = deep_freeze(data)

# Attempting mutation raises TypeError
try:
    frozen["user"] = "Bob"  # type: ignore[index]
    raise AssertionError("Should have raised TypeError")
except TypeError:
    pass

assert frozen["roles"] == ("admin", "editor")
assert isinstance(frozen["roles"], tuple)
```
</details>

### Challenge 6 (Hard): Multi-Argument Curry Function
Implement a decorator `curry(fn)` that transforms a multi-argument function into a curried function that can be called repeatedly with single or multiple arguments until all required positional parameters are satisfied.
<details>
<summary><strong>Solution</strong></summary>

```python
import inspect
import functools

def curry(fn):
    num_params = len(inspect.signature(fn).parameters)

    @functools.wraps(fn)
    def curried(*args):
        if len(args) >= num_params:
            return fn(*args[:num_params])
        return lambda *more_args: curried(*(args + more_args))

    return curried

@curry
def volume(length: float, width: float, height: float) -> float:
    return length * width * height

assert volume(2)(3)(4) == 24
assert volume(2, 3)(4) == 24
assert volume(2)(3, 4) == 24
assert volume(2, 3, 4) == 24
```
</details>
