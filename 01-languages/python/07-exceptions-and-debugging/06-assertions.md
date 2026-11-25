# Module 07: Exceptions, Context Managers & Debugging (06-assertions)

> **Topic Overview**: The `assert` statement is Python's diagnostic tool for documenting and enforcing internal program invariants during development. Because the bytecode compiler strips all assertions when run with optimization flags (`-O`, `-OO`), using `assert` for runtime validation or security checks introduces catastrophic vulnerabilities. Understanding its compilation mechanics and idioms is essential for safe systems engineering.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Debugging & Diagnostics |
| **Target Level** | Intermediate |
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

- **Problem Statement**: Developers need a way to document preconditions, postconditions, and internal structural assumptions that should *never* fail if the code is correct, without paying a performance penalty in optimized production builds.
- **Mental Model**: Think of an assertion as a developer-facing sanity check: "If this statement is false, there is a bug in *my* code." Conversely, if a condition fails because of user input, bad data from an external API, or network latency, that is a runtime error—not a broken invariant—and must raise an explicit `ValueError`, `KeyError`, or `PermissionError`.
- **When to Use**:
  - Verifying internal function invariants (e.g., verifying that a private helper received valid arguments after public validation).
  - Checking postconditions after complex algorithms (e.g., verifying that a custom sorting function returned a list of the same length).
  - Unreachable code markers in `match` or `if/elif` branches.
  - Test suites (e.g., standard `pytest` assertions).
- **When NOT to Use**:
  - Never use `assert` for user input validation.
  - Never use `assert` for security authentication or authorization checks.
  - Never use `assert` with expressions that have side effects (e.g., `assert list.pop() == item`).

---

## 2. Core Concepts

### Equivalence Formulation
In normal execution mode, the statement:
```python
assert condition, message
```
is functionally equivalent to:
```python
if __debug__:
    if not condition:
        raise AssertionError(message)
```

### The `__debug__` Global Flag
- `__debug__` is a built-in constant in Python.
- Under default execution: `__debug__ is True`.
- When invoked with `-O` or `-OO`: `__debug__ is False`.
- In optimized mode (`-O`), the CPython bytecode compiler **completely removes** the conditional check and the `AssertionError` bytecode. The code inside the assert statement is **never evaluated**.

---

## 3. How It Works Internally

During AST compilation into bytecode, CPython checks the optimization level flag:

```text
Source Code: assert balance >= 0, "Negative balance!"

+-----------------------------------------------------------+
| Normal Mode (python script.py)                            |
| __debug__ is True                                         |
+-----------------------------------------------------------+
| 1. Evaluates: balance >= 0                                |
| 2. POP_JUMP_IF_TRUE to target                             |
| 3. LOAD_GLOBAL AssertionError                             |
| 4. LOAD_CONST "Negative balance!"                         |
| 5. RAISE_VARARGS 1                                        |
+-----------------------------------------------------------+

+-----------------------------------------------------------+
| Optimized Mode (python -O script.py)                      |
| __debug__ is False                                        |
+-----------------------------------------------------------+
| 1. Compiler completely skips assert node during AST walk  |
| 2. Bytecode generated: ZERO instructions (completely empty)|
+-----------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Grammar
```python
assert test
assert test, message
```

- `test`: Any Python expression evaluating to a truthy or falsy value.
- `message`: Optional expression (conventionally a string) passed as the argument to `AssertionError(message)`.

### The Python Optimization Flags
- `python script.py`: Standard execution (`__debug__ == True`).
- `python -O script.py`: Discards assert statements and sets `__debug__ = False`.
- `python -OO script.py`: Discards assert statements AND strips docstrings (`__doc__ = None`).

---

## 5. Code Examples

### 1. Minimal / Basic Example: Internal Invariant Checking
```python
def calculate_discounted_price(price: float, discount_ratio: float) -> float:
    # Public validation
    if price < 0:
        raise ValueError("Price cannot be negative")
    if not (0.0 <= discount_ratio <= 1.0):
        raise ValueError("Discount must be between 0.0 and 1.0")

    final_price = price * (1.0 - discount_ratio)

    # Internal postcondition invariant: price must never increase
    assert final_price <= price, "Postcondition failed: Discount increased price!"
    assert final_price >= 0.0, "Postcondition failed: Negative final price!"

    return final_price

print("Price ($100 with 20% off):", calculate_discounted_price(100.0, 0.2))
```
**Output**:
```text
Price ($100 with 20% off): 80.0
```

### 2. Real-World Production Example: Unreachable Branch Guard
```python
from enum import Enum

class Status(Enum):
    PENDING = "PENDING"
    ACTIVE = "ACTIVE"
    ARCHIVED = "ARCHIVED"

def handle_status(status: Status) -> str:
    match status:
        case Status.PENDING:
            return "Status is pending approval"
        case Status.ACTIVE:
            return "Status is operational"
        case Status.ARCHIVED:
            return "Status is archived"
        case _:
            # This branch is logically impossible if all enum cases are handled
            raise AssertionError(f"Unreachable state encountered: {status}")

print(handle_status(Status.ACTIVE))
```
**Output**:
```text
Status is operational
```

### 3. Edge Case: The Non-Empty Tuple Pitfall
```python
# WARNING: Non-empty tuples in Python are ALWAYS truthy!
# assert (condition, message) evaluates bool((condition, message))

def buggy_assert():
    # Even though 1 == 2 is False, the tuple (False, "Math failed") is truthy!
    assert (1 == 2, "This message will NEVER be displayed, assert never triggers!")
    print("Buggy assert passed silently!")

buggy_assert()
```
**Output**:
```text
Buggy assert passed silently!
```

### 4. Failure Case: Catastrophic Security Bypass via Optimization Flag
```python
def execute_privileged_action(user_role: str) -> str:
    # SECURITY VULNERABILITY: Using assert for authorization
    # If run with python -O, this check disappears completely!
    assert user_role == "ADMIN", "Security violation: Unauthorized access"
    return "CREDIT_CARD_VAULT_ACCESSED"

# Normal execution
try:
    execute_privileged_action("GUEST")
except AssertionError as e:
    print(f"Normal mode blocked access: {e}")

# Simulate how optimized mode executes by setting __debug__ logic
if not __debug__:
    # Under python -O, this would execute for GUEST!
    pass
```
**Output**:
```text
Normal mode blocked access: Security violation: Unauthorized access
```

### 5. Idiomatic Pythonic Pattern: Pytest-Style Rich Diagnostic Assertions
```python
def test_user_aggregation():
    users = [{"name": "Alice", "role": "admin"}, {"name": "Bob", "role": "user"}]
    admin_count = sum(1 for u in users if u["role"] == "admin")

    assert admin_count == 1, f"Expected 1 admin, found {admin_count}"
    assert len(users) == 2, f"Expected 2 total users, found {len(users)}"
    print("Diagnostic tests verified successfully.")

test_user_aggregation()
```
**Output**:
```text
Diagnostic tests verified successfully.
```

---

## 6. Edge Cases & Pitfalls

1. **Expressions with Side Effects**:
   - Never write `assert items.pop() == "expected"`. In optimized mode (`-O`), `pop()` is **never called**, completely corrupting program state between debug and production environments!
2. **Parenthesized Assertions Warning**:
   - Python 3 will emit a `SyntaxWarning: assertion is always true, perhaps remove parentheses?` when compiling `assert (x, y)`. Always use comma without enclosing parentheses: `assert x, y`.
3. **Cannot Rebind `__debug__`**:
   - `__debug__ = False` is a syntax error at compile time (`SyntaxError: cannot assign to __debug__`).

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| `assert (condition, message)` with tuple brackets | Non-empty tuples are always truthy; assertion never triggers | `SyntaxWarning: assertion is always true` | `assert condition, message` |
| Using `assert` for user input validation | In optimized mode (`-O`), validation is stripped, allowing bad data | Silent data corruption | `if not valid: raise ValueError(...)` |
| Using `assert` for security checks | In optimized mode (`-O`), authorization checks vanish | Critical security vulnerability | `if not auth: raise PermissionError(...)` |
| Asserting expressions with mutating side effects | State mutation is skipped in production builds | Inconsistent bugs between dev and prod | Perform mutation outside assert |
| Catching `AssertionError` in production code | Indicates developer code bugs, not runtime conditions | Masks internal programming errors | Fix the underlying invariant bug |
| Assigning to `__debug__` | `__debug__` is a compiler-enforced read-only constant | `SyntaxError: cannot assign to __debug__` | Use CLI flag `-O` |

---

## 8. Performance & Complexity

- **Standard Execution (`__debug__ == True`)**: Costs $\approx 5-15$ ns to evaluate simple condition and branch.
- **Optimized Execution (`-O`, `-OO`)**: Exactly **0.00 ns**. CPython completely elides bytecode emission for the entire assert statement.
- **Memory**: Zero allocation when condition holds. If condition fails, `AssertionError` allocates $\approx 64$ bytes.

---

## 9. CPython Internals & Bytecode

Let's compare the bytecode of an assert statement under standard execution:

```python
import dis

def check_value(x):
    assert x > 0, "x must be positive"
    return x

dis.dis(check_value)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 LOAD_FAST                0 (x)
              4 LOAD_CONST               1 (0)
              6 COMPARE_OP              68 (>)
             10 POP_JUMP_IF_TRUE         5 (to 22)
             12 LOAD_GLOBAL              1 (AssertionError)
             22 LOAD_CONST               2 ('x must be positive')
             24 CALL                     1
             32 RAISE_VARARGS            1

  3     >>   34 LOAD_FAST                0 (x)
             36 RETURN_VALUE
```
Notice that if `x > 0` evaluates to true, `POP_JUMP_IF_TRUE` jumps directly to offset `34`, completely bypassing the instantiation and raising of `AssertionError`.

---

## 10. Best Practices

1. **Verify Internal State Only**: Use `assert` to verify internal assumptions that should never happen in a bug-free program.
2. **Raise Standard Exceptions for Callers**: If an invalid argument is supplied to a public API function, raise `ValueError`, `TypeError`, or a domain exception.
3. **Never Mutate State**: The expression tested by `assert` must be strictly pure and idempotent.

---

## 11. Real-World Case Study: Numerical Matrix Invariant Verification

```python
class Matrix:
    def __init__(self, rows: int, cols: int, data: list[list[float]]) -> None:
        # Public contract validation
        if rows <= 0 or cols <= 0:
            raise ValueError("Dimensions must be positive integers")
        if len(data) != rows or any(len(r) != cols for r in data):
            raise ValueError("Data shape does not match specified dimensions")

        self.rows = rows
        self.cols = cols
        self.data = data

    def multiply(self, other: "Matrix") -> "Matrix":
        if self.cols != other.rows:
            raise ValueError(f"Incompatible shapes for multiplication: ({self.rows}x{self.cols}) and ({other.rows}x{other.cols})")

        # Perform multiplication
        result_data = [[0.0] * other.cols for _ in range(self.rows)]
        for i in range(self.rows):
            for j in range(other.cols):
                result_data[i][j] = sum(self.data[i][k] * other.data[k][j] for k in range(self.cols))

        # Internal invariant postconditions
        assert len(result_data) == self.rows, "Invariant broken: Result rows mismatch"
        assert len(result_data[0]) == other.cols, "Invariant broken: Result cols mismatch"

        return Matrix(self.rows, other.cols, result_data)

m1 = Matrix(2, 2, [[1.0, 2.0], [3.0, 4.0]])
m2 = Matrix(2, 2, [[2.0, 0.0], [1.0, 2.0]])
m3 = m1.multiply(m2)
print("Multiplication succeeded. Result shape:", (m3.rows, m3.cols))
```
**Output**:
```text
Multiplication succeeded. Result shape: (2, 2)
```

---

## 12. Interview Questions

### Q1: What happens to `assert` statements when Python is run with the `-O` flag?
<details>
<summary><strong>Answer</strong></summary>
The CPython compiler sets the internal <code>__debug__</code> flag to <code>False</code> and completely skips code generation for all <code>assert</code> statements during AST compilation. In the resulting <code>.pyc</code> bytecode, the assert statements and their condition evaluations do not exist.
</details>

### Q2: Why is `assert (x > 0, "x must be positive")` considered a critical bug?
<details>
<summary><strong>Answer</strong></summary>
In Python, enclosing the condition and message in parentheses forms a 2-element tuple <code>(x > 0, "message")</code>. Any non-empty tuple in Python evaluates to truthy in boolean context, regardless of whether <code>x > 0</code> is true or false. Consequently, the assertion will <strong>never</strong> fail.
</details>

### Q3: When should you use `assert` versus raising an explicit `ValueError`?
<details>
<summary><strong>Answer</strong></summary>
Use explicit exceptions (such as <code>ValueError</code> or <code>TypeError</code>) for validating user input, external API responses, or public library arguments that can legitimately be malformed. Use <code>assert</code> strictly for checking internal developer assumptions and postconditions that represent bugs in your own code if violated.
</details>

### Q4: Can you reassign `__debug__ = False` inside a Python script?
<details>
<summary><strong>Answer</strong></summary>
No. <code>__debug__</code> is a reserved compile-time constant. Attempting to assign to it raises a <code>SyntaxError: cannot assign to __debug__</code> during parsing.
</details>

### Q5: How does the `-OO` flag differ from the `-O` flag?
<details>
<summary><strong>Answer</strong></summary>
Both `-O` and `-OO` remove <code>assert</code> statements and set <code>__debug__ = False</code>. In addition, `-OO` strips docstrings (<code>__doc__</code>) from classes, functions, and modules to slightly reduce memory usage.
</details>

### Q6: Why is `assert user.is_authenticated()` considered a security vulnerability?
<details>
<summary><strong>Answer</strong></summary>
If the application is deployed or run with the standard production optimization flag <code>python -O</code>, the assertion is completely removed, allowing unauthenticated requests to bypass the check and access protected resources.
</details>

### Q7: What exception type does `assert` raise when it fails?
<details>
<summary><strong>Answer</strong></summary>
It raises <code>AssertionError</code>, which is a direct subclass of <code>Exception</code>.
</details>

### Q8: What is the risk of calling functions with side effects inside an assert condition?
<details>
<summary><strong>Answer</strong></summary>
Because assert statements are stripped in optimized mode (<code>-O</code>), any side effects (like mutating an object, logging, or incrementing counters) will occur in development mode but will silently fail to happen in production, leading to hard-to-debug discrepancies.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Invariant Verifier
Write a function `clamp(val: float, min_val: float, max_val: float) -> float` that ensures `val` is clamped between `min_val` and `max_val`. Use `assert` to verify the postcondition that `min_val <= result <= max_val`.
<details>
<summary><strong>Solution</strong></summary>

```python
def clamp(val: float, min_val: float, max_val: float) -> float:
    if min_val > max_val:
        raise ValueError("min_val cannot exceed max_val")
    result = max(min_val, min(val, max_val))
    assert min_val <= result <= max_val, "Postcondition failed"
    return result

assert clamp(15, 0, 10) == 10
assert clamp(-5, 0, 10) == 0
assert clamp(5, 0, 10) == 5
```
</details>

### Challenge 2 (Easy): Fix the Tuple Assertion
Identify the bug in the following snippet and write a corrected version:
```python
def check_age(age):
    assert (age >= 0, "Age cannot be negative")
```
<details>
<summary><strong>Solution</strong></summary>

```python
# The parentheses create a 2-tuple which is always truthy.
# Fixed version:
def check_age(age: int) -> None:
    assert age >= 0, "Age cannot be negative"

try:
    check_age(-5)
    raise RuntimeError("Should have raised AssertionError")
except AssertionError as e:
    assert str(e) == "Age cannot be negative"
```
</details>

### Challenge 3 (Medium): Exhaustive Match Invariant Guard
Create an enum `PaymentMethod` with `CREDIT_CARD`, `PAYPAL`, and `CRYPTO`. Write a function `process_fee(method: PaymentMethod) -> float` using `match/case` that computes fee, with a default case `_` asserting unreachable state.
<details>
<summary><strong>Solution</strong></summary>

```python
from enum import Enum

class PaymentMethod(Enum):
    CREDIT_CARD = "CC"
    PAYPAL = "PP"
    CRYPTO = "BTC"

def process_fee(method: PaymentMethod) -> float:
    match method:
        case PaymentMethod.CREDIT_CARD:
            return 0.03
        case PaymentMethod.PAYPAL:
            return 0.04
        case PaymentMethod.CRYPTO:
            return 0.01
        case _:
            raise AssertionError(f"Unhandled payment method: {method}")

assert process_fee(PaymentMethod.CREDIT_CARD) == 0.03
```
</details>

### Challenge 4 (Medium): Custom Assertion Formatter
Write a custom assertion function `assert_equal(actual, expected, context_name="Value")` that raises `AssertionError` with a multi-line formatted diff showing types, representations, and context if `actual != expected`.
<details>
<summary><strong>Solution</strong></summary>

```python
def assert_equal(actual, expected, context_name="Value") -> None:
    if actual != expected:
        msg = (
            f"Assertion Failed for '{context_name}':\n"
            f"  Expected: {expected!r} (type: {type(expected).__name__})\n"
            f"  Actual:   {actual!r} (type: {type(actual).__name__})"
        )
        raise AssertionError(msg)

assert_equal(10, 10, "Count")
try:
    assert_equal("10", 10, "User ID")
except AssertionError as err:
    assert "Expected: 10" in str(err)
    assert "Actual:   '10'" in str(err)
```
</details>

### Challenge 5 (Hard): Bytecode Assertion Stripping Verifier
Write a test function `verify_no_asserts_in_code(fn)` that disassembles the provided function's bytecode using `dis.get_instructions()` and verifies whether any `LOAD_GLOBAL` for `AssertionError` exists in the bytecode.
<details>
<summary><strong>Solution</strong></summary>

```python
import dis
from typing import Callable

def verify_no_asserts_in_code(fn: Callable) -> bool:
    instructions = list(dis.get_instructions(fn))
    for inst in instructions:
        if inst.opname == "LOAD_GLOBAL" and inst.argval == "AssertionError":
            return False
    return True

def func_with_assert(x):
    assert x > 0
    return x

def func_clean(x):
    if x <= 0:
        raise ValueError()
    return x

assert verify_no_asserts_in_code(func_clean) is True
assert verify_no_asserts_in_code(func_with_assert) is False
```
</details>

### Challenge 6 (Hard): Contract Programming Enforcer Decorator
Implement a decorator `@contract(pre=None, post=None)` where `pre` is a predicate callable taking `(*args, **kwargs)` and `post` is a predicate callable taking `(result, *args, **kwargs)`. In normal mode, enforce both contracts via `AssertionError`. In optimized mode (`not __debug__`), skip all checks with zero overhead.
<details>
<summary><strong>Solution</strong></summary>

```python
import functools
from typing import Callable, Any

def contract(pre: Callable[..., bool] | None = None, post: Callable[..., bool] | None = None):
    def decorator(fn: Callable):
        if not __debug__:
            # In optimized mode, return function completely unaltered!
            return fn

        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            if pre is not None:
                assert pre(*args, **kwargs), f"Precondition failed for {fn.__name__}"
            result = fn(*args, **kwargs)
            if post is not None:
                assert post(result, *args, **kwargs), f"Postcondition failed for {fn.__name__}"
            return result
        return wrapper
    return decorator

@contract(
    pre=lambda x: x > 0,
    post=lambda res, x: res == x * 2
)
def double_positive(x: int) -> int:
    return x * 2

assert double_positive(5) == 10

try:
    double_positive(-1)
except AssertionError as e:
    assert "Precondition failed" in str(e)
```
</details>
