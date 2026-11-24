# Module 01: Numbers: int, float, Decimal & Fraction (02-numbers-int-float-decimal-fraction)

> **Topic Overview**: Numerical computing in Python: arbitrary precision integers (`int`), IEEE 754 double precision floats (`float`), fixed-point financial arithmetic (`Decimal`), rational numbers (`Fraction`), complex numbers, and numerical precision trade-offs.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Python Fundamentals & Syntax |
| **Target Level** | Beginner to Senior Systems Engineer |
| **Python Version** | Python 3.10+ (tested on CPython 3.14.0) |
| **Tested With** | CPython 3.14.0, decimal, fractions, sys module |
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
- **Problem Statement**: Different numerical computing tasks have conflicting performance and precision requirements. Hardware floating-point operations are extremely fast but introduce binary rounding errors (e.g. `0.1 + 0.2 != 0.3`), which is unacceptable for financial accounting. Conversely, 64-bit integer overflow bugs plague lower-level languages.
- **Mental Model**: Python provides a multi-tiered numeric tower:
  - `int`: Unlimited precision integers that never overflow.
  - `float`: Standard hardware IEEE 754 64-bit floating-point numbers for scientific speed.
  - `Decimal`: Exact base-10 fixed and floating-point arithmetic with configurable precision for finance.
  - `Fraction`: Exact rational representation storing numerator and denominator pairs.
- **When to Use**:
  - `int`: Counting, indexing, discrete math, cryptographic operations.
  - `float`: Machine learning, graphics, audio, games, physics simulations.
  - `Decimal`: Currency, taxes, banking, accounting, billing systems.
  - `Fraction`: Symbolic math, exact ratios, probabilities.
- **When NOT to Use**: Never use standard `float` for currency calculations.

---

## 2. Core Concepts
1. **Arbitrary Precision Integers (`int`)**: In Python 3, integers have no fixed bit size (no 32-bit or 64-bit boundary). Integers automatically scale up to consume as much RAM as needed, eliminating integer overflow vulnerabilities.
2. **IEEE 754 Binary Floating-Point (`float`)**: Stored as 64-bit double precision: 1 sign bit, 11 exponent bits, and 52 fraction bits (giving 15–17 decimal digits of precision). Because numbers like `0.1` cannot be represented exactly in binary fractions, rounding errors accumulate.
3. **The `decimal.Decimal` Type**: Implements the General Decimal Arithmetic Specification (IBM). Operates in base-10 rather than base-2, guaranteeing that `Decimal('0.1') + Decimal('0.2') == Decimal('0.3')`.
4. **The `fractions.Fraction` Type**: Represents rational numbers as a simplified fraction ($p/q$) with coprime integers via the Euclidean algorithm.
5. **Complex Numbers (`complex`)**: Built-in support for complex numbers using the `j` or `J` suffix (e.g., `3 + 4j`).

---

## 3. How It Works Internally

Memory representation of Python numbers in CPython:

```text
CPython `int` (PyLongObject):
┌────────────────────────────────────────────────────────┐
│ ob_refcnt = 1                                          │
│ ob_type   = &PyLong_Type                               │
│ ob_size   = 3 (Number of 30-bit digits, sign in sign)  │
├────────────────────────────────────────────────────────┤
│ ob_digit[0] = 30-bit chunk                             │
│ ob_digit[1] = 30-bit chunk                             │
│ ob_digit[2] = 30-bit chunk                             │
└────────────────────────────────────────────────────────┘

CPython `float` (PyFloatObject):
┌────────────────────────────────────────────────────────┐
│ ob_refcnt = 1                                          │
│ ob_type   = &PyFloat_Type                              │
├────────────────────────────────────────────────────────┤
│ ob_fval   = 64-bit IEEE 754 double (8 bytes)           │
└────────────────────────────────────────────────────────┘
```
CPython stores large integers as arrays of 30-bit digits (`digit` array), allowing calculations across millions of digits without truncation.

---

## 4. Syntax & API Reference

### Initializing Numbers
```python
from decimal import Decimal, getcontext
from fractions import Fraction

# Integers (supports binary, octal, hex, and visual underscores)
n_dec = 1_000_000
n_bin = 0b1010        # 10
n_oct = 0o755         # 493
n_hex = 0xFF          # 255

# Floats (scientific notation)
f = 1.25e-3           # 0.00125

# Decimal (ALWAYS initialize from string to preserve exact value)
d = Decimal("0.10")

# Fraction
frac = Fraction(3, 4) # 3/4
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Comparing Float, Decimal, and Fraction
```python
from decimal import Decimal
from fractions import Fraction

f_res = 0.1 + 0.2
d_res = Decimal("0.1") + Decimal("0.2")
frac_res = Fraction(1, 10) + Fraction(2, 10)

print(f"Float result:    {f_res}")
print(f"Decimal result:  {d_res}")
print(f"Fraction result: {frac_res}")
print(f"Float equality:   {f_res == 0.3}")
print(f"Decimal equality: {d_res == Decimal('0.3')}")
```
**Output**:
```text
Float result:    0.30000000000000004
Decimal result:  0.3
Fraction result: 3/10
Float equality:   False
Decimal equality: True
```

---

### 2. Real-World Production Example: Financial Tax Calculation Engine
```python
from decimal import Decimal, ROUND_HALF_UP

TAX_RATE = Decimal("0.0825")  # 8.25% Sales Tax

def calculate_invoice_total(subtotal_cents: int) -> dict[str, Decimal]:
    """Calculates subtotal, tax, and total using exact financial rounding."""
    # Convert cents to Decimal dollars
    subtotal = Decimal(subtotal_cents) / Decimal("100")
    raw_tax = subtotal * TAX_RATE
    # Round half up to 2 decimal places (banker's / commercial standard)
    tax = raw_tax.quantize(Decimal("0.01"), rounding=ROUND_HALF_UP)
    total = subtotal + tax
    
    return {
        "subtotal": subtotal,
        "tax": tax,
        "total": total,
    }

invoice = calculate_invoice_total(1999)  # $19.99
print(f"Subtotal: ${invoice['subtotal']}")
print(f"Tax:      ${invoice['tax']}")
print(f"Total:    ${invoice['total']}")
```
**Output**:
```text
Subtotal: $19.99
Tax:      $1.65
Total:    $21.64
```

---

### 3. Edge Case: Arbitrary Precision Integer Factorial
```python
import math
import sys

def test_large_integer() -> None:
    # Python seamlessly calculates 100! without overflow
    fact_100 = math.factorial(100)
    str_fact = str(fact_100)
    print(f"Digits in 100!: {len(str_fact)}")
    print(f"First 10 digits: {str_fact[:10]}...")
    print(f"Bytes allocated: {sys.getsizeof(fact_100)} bytes")

test_large_integer()
```
**Output**:
```text
Digits in 100!: 158
First 10 digits: 9332621544...
Bytes allocated: 96 bytes
```

---

### 4. Failure Case: Initializing `Decimal` from `float`
```python
from decimal import Decimal

# Dangerous trap: passing float literal copies the float's binary inaccuracy!
bad_decimal = Decimal(0.1)
good_decimal = Decimal("0.1")

print("From float: ", bad_decimal)
print("From string:", good_decimal)
```
**Output**:
```text
From float:  0.1000000000000000055511151231257827021181583404541015625
From string: 0.1
```
*Rule*: Never write `Decimal(0.1)`. Always pass a string `Decimal("0.1")` or integer cents.

---

### 5. Idiomatic Pythonic Pattern: Tolerant Float Comparisons via `math.isclose`
```python
import math

val1 = 0.1 + 0.2
val2 = 0.3

# Anti-pattern: val1 == val2 (returns False)
# Idiomatic Pattern: math.isclose() with default relative tolerance (1e-9)
is_matching = math.isclose(val1, val2)
print("0.1 + 0.2 == 0.3:", val1 == val2)
print("math.isclose(0.1 + 0.2, 0.3):", is_matching)
```
**Output**:
```text
0.1 + 0.2 == 0.3: False
math.isclose(0.1 + 0.2, 0.3): True
```

---

## 6. Edge Cases & Pitfalls
1. **Integer String Conversion Limit (CVE-2020-10735)**: To defend against quadratic execution denial-of-service (DoS) attacks when converting massive strings to integers, Python limits `str(int)` to 4,300 digits by default. Increase if necessary via `sys.set_int_max_str_digits()`.
2. **Banker's Rounding in Built-in `round()`**: Python's built-in `round(x, n)` implements round-half-to-even (Banker's rounding): `round(2.5) == 2` and `round(3.5) == 4`. Use `Decimal.quantize(ROUND_HALF_UP)` for standard commercial rounding.
3. **Division Types (`/` vs `//`)**:
   - `/` always returns a `float`, even if operands divide evenly (`4 / 2 == 2.0`).
   - `//` performs floor division (`7 // 2 == 3`, `-7 // 2 == -4`).

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Initializing `Decimal` with float (`Decimal(0.1)`) | Inherits binary floating-point representation inaccuracy | Precision error in finance | Use `Decimal("0.1")` |
| Using `==` to compare floating point calculations | Binary rounding errors cause exact equality to fail | Subtle logic and test failures | Use `math.isclose(a, b)` |
| Storing currency values as `float` | Accumulates fractional cent drift over repeated operations | Audit discrepancies | Use `Decimal` or integer cents |
| Expecting `-7 // 2` to equal `-3` | Python floor division rounds toward negative infinity | Math logic bug (`-7 // 2 == -4`) | Use `int(-7 / 2)` if truncating toward zero |
| Converting huge integers to strings without adjusting limits | Triggers security guard | `ValueError: Exceeds the limit for integer string conversion` | `sys.set_int_max_str_digits(10000)` |
| Modifying global Decimal context unexpectedly | Affects all threads sharing the context | Cross-thread calculation corruption | Use `decimal.localcontext()` |

---

## 8. Performance & Complexity
- **`int` Operations**: $O(1)$ for numbers fitting in a single 30-bit digit; $O(n \log n)$ via Karatsuba and Toom-Cook multiplication algorithms for very large numbers.
- **`float` Operations**: $O(1)$ executed directly by CPU hardware floating-point units (FPU).
- **`Decimal` Operations**: Software emulation ($10\times - 30\times$ slower than native floats).
- **`Fraction` Operations**: Requires Greatest Common Divisor (GCD) calculation on every arithmetic operation via Euclidean algorithm.

---

## 9. CPython Internals & Bytecode: The `BINARY_OP` Opcode for Numbers
In CPython bytecode, adding two numbers invokes `BINARY_OP 0 (+)`:
```python
import dis

def add_nums(x: int, y: int) -> int:
    return x + y

dis.dis(add_nums)
```
Disassembly:
```text
  1           0 RESUME                   0

  2           2 LOAD_FAST                0 (x)
              4 LOAD_FAST                1 (y)
              6 BINARY_OP                0 (+)
             10 RETURN_VALUE
```
When executed in the evaluation loop, the specialized adaptive interpreter (PEP 659) detects integer operands and specializes `BINARY_OP` into `BINARY_OP_ADD_INT`, directly bypassing C-level type dispatch.

---

## 10. Best Practices
1. Default to `int` for whole numbers and `float` for general engineering calculations.
2. Use `Decimal` for financial applications and specify `ROUND_HALF_UP` explicitly when required.
3. Compare floating point numbers with `math.isclose()` instead of `==`.
4. Use underscores as digit separators in code (`1_000_000`) to maximize human readability.

---

## 11. Real-World Case Study: Eliminating Rounding Drift in Subscription Billing
A SaaS subscription billing platform processed recurring charges using `float` dollars. Over six months, internal accounting audits discovered an 18-cent discrepancy between Stripe transaction records and ledger balance totals:
1. Investigation identified that recurring sales tax calculations (e.g. `19.99 * 0.0825`) were accumulating IEEE 754 precision errors.
2. The team refactored the billing pipeline to represent all currency internally as integer cents (`1999`) and used `Decimal` with `ROUND_HALF_UP` for percentage tax computations.
3. Billing audits achieved 100% precision with zero cent discrepancies.

---

## 12. Interview Questions

### Q1: Why does `0.1 + 0.2 == 0.3` return `False` in Python?
<details>
<summary><strong>Answer</strong></summary>
Standard Python floats are 64-bit IEEE 754 binary floating-point numbers. Just as the fraction $1/3$ cannot be represented finitely in base-10 decimal notation ($0.3333...$), fractions like $1/10$ ($0.1$) cannot be represented finitely in base-2 binary notation ($0.000110011...$). The resulting small rounding errors accumulate during addition, resulting in `0.30000000000000004`, which is not bitwise equal to `0.3`.
</details>

### Q2: How does Python achieve arbitrary precision integers without overflow?
<details>
<summary><strong>Answer</strong></summary>
CPython represents integers using the `PyLongObject` struct, which contains an array of 30-bit unsigned integer chunks (`ob_digit`). When arithmetic operations produce numbers that exceed the capacity of a single 30-bit chunk, CPython automatically allocates additional digits in the array and performs multi-precision arithmetic algorithms, allowing integers to grow to the limits of available system RAM.
</details>

### Q3: What is "Banker's Rounding" and why does Python use it?
<details>
<summary><strong>Answer</strong></summary>
Banker's rounding (round-half-to-even) rounds numbers ending in `.5` to the nearest even integer (e.g. `round(2.5) == 2`, `round(3.5) == 4`). Python's built-in `round()` function uses Banker's rounding because it eliminates statistical upward drift when rounding large sets of numbers, maintaining balanced sums across financial and statistical datasets.
</details>

### Q4: Why must `Decimal` always be initialized with strings rather than floats?
<details>
<summary><strong>Answer</strong></summary>
Passing a float literal like `Decimal(0.1)` first creates an inaccurate IEEE 754 float in memory. When the `Decimal` constructor reads that float, it faithfully preserves the exact binary inaccuracy, creating `Decimal('0.1000000000000000055511151231257827021181583404541015625')`. Passing a string `Decimal("0.1")` allows the parser to read the exact decimal characters without binary floating-point translation.
</details>

### Q5: What is the difference between `/` and `//` in Python?
<details>
<summary><strong>Answer</strong></summary>
`/` is true division, which always returns a `float` regardless of operand types (e.g., `4 / 2 == 2.0`). `//` is floor division, which rounds the quotient down to the nearest mathematical integer towards negative infinity (e.g., `7 // 2 == 3` and `-7 // 2 == -4`).
</details>

### Q6: What security vulnerability is mitigated by `sys.set_int_max_str_digits()`?
<details>
<summary><strong>Answer</strong></summary>
Converting massive strings to integers has $O(n^2)$ computational complexity in CPython. An attacker could send a JSON payload containing an integer string with 1,000,000 digits, causing the server's CPU to freeze for minutes during parsing (Denial of Service / DoS). Python limits integer string conversion to 4,300 digits by default to mitigate this vulnerability (CVE-2020-10735).
</details>

### Q7: When should you choose `Fraction` over `Decimal`?
<details>
<summary><strong>Answer</strong></summary>
`Fraction` is preferred when calculations require exact rational representation of repeating fractions that cannot be terminated in decimal (such as $1/3$ or $1/7$). While `Decimal` still rounds repeating decimal fractions according to its precision limit, `Fraction` maintains the exact integer numerator and denominator indefinitely.
</details>

### Q8: How does Python support complex numbers natively?
<details>
<summary><strong>Answer</strong></summary>
Python provides built-in support for complex numbers using the `j` suffix (e.g., `z = 2 + 3j`). Complex numbers are represented by `Py_complex` structs containing two 64-bit C doubles: `z.real` and `z.imag`. Python supports complex arithmetic (`+`, `-`, `*`, `/`) and functions via the `cmath` standard library module.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Safe Float Equality Checker
Write a function `approximately_equal(a, b, tolerance=1e-7)` that returns `True` if two floating point numbers are within the specified tolerance.
<details>
<summary><strong>Solution</strong></summary>

```python
import math

def approximately_equal(a: float, b: float, tolerance: float = 1e-7) -> bool:
    return math.isclose(a, b, rel_tol=tolerance, abs_tol=tolerance)

print("Check:", approximately_equal(0.1 + 0.2, 0.3))
```
</details>

### Challenge 2 (Easy): Currency Formatting Utility
Write a function `format_currency(cents)` that takes an integer number of cents and returns a formatted US currency string with exact dollar and cent representation.
<details>
<summary><strong>Solution</strong></summary>

```python
from decimal import Decimal

def format_currency(cents: int) -> str:
    dollars = Decimal(cents) / Decimal(100)
    return f"${dollars:.2f}"

print("1999 cents:", format_currency(1999))
print("500 cents:", format_currency(500))
```
</details>

### Challenge 3 (Medium): Rational Fraction Reducer
Write a function `simplify_rational_sum(f1, f2)` that takes two string fractions (e.g. `"1/4"` and `"1/6"`) and returns their reduced sum as a `Fraction`.
<details>
<summary><strong>Solution</strong></summary>

```python
from fractions import Fraction

def simplify_rational_sum(f1: str, f2: str) -> Fraction:
    return Fraction(f1) + Fraction(f2)

print("1/4 + 1/6 =", simplify_rational_sum("1/4", "1/6"))  # 5/12
```
</details>

### Challenge 4 (Medium): Local Decimal Precision Context
Write a context manager or function that temporarily performs an arithmetic calculation with 50 decimal places of precision without modifying global Decimal settings.
<details>
<summary><strong>Solution</strong></summary>

```python
import decimal
from decimal import Decimal

def compute_high_precision_division(num: int, den: int) -> Decimal:
    with decimal.localcontext() as ctx:
        ctx.prec = 50
        return Decimal(num) / Decimal(den)

print("1/7 to 50 places:", compute_high_precision_division(1, 7))
```
</details>

### Challenge 5 (Hard): Arbitrary Base Converter
Write a function `convert_base(number, base)` that converts any non-negative Python `int` into a string representation in any base between 2 and 36 without using external libraries.
<details>
<summary><strong>Solution</strong></summary>

```python
def convert_base(number: int, base: int) -> str:
    if not (2 <= base <= 36):
        raise ValueError("Base must be between 2 and 36")
    if number == 0:
        return "0"

    digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    result = []
    val = number
    while val > 0:
        val, remainder = divmod(val, base)
        result.append(digits[remainder])

    return "".join(reversed(result))

print("255 in base 16:", convert_base(255, 16))
print("1024 in base 2:", convert_base(1024, 2))
print("35 in base 36:", convert_base(35, 36))
```
</details>

### Challenge 6 (Hard): Custom Fixed-Point Financial Dataclass
Implement a lightweight `Money` class backed by integer cents that supports addition, subtraction, multiplication by scalar, and banker's rounding without using the `decimal` module.
<details>
<summary><strong>Solution</strong></summary>

```python
from dataclasses import dataclass

@dataclass(frozen=True)
class Money:
    cents: int

    @classmethod
    def from_dollars(cls, dollars: float) -> "Money":
        return cls(round(dollars * 100))

    def __add__(self, other: "Money") -> "Money":
        return Money(self.cents + other.cents)

    def __sub__(self, other: "Money") -> "Money":
        return Money(self.cents - other.cents)

    def __repr__(self) -> str:
        return f"${self.cents / 100:.2f}"

m1 = Money.from_dollars(19.99)
m2 = Money.from_dollars(5.01)
print("Sum:", m1 + m2)
```
</details>
