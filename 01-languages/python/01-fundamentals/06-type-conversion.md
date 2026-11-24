# Module 01: Type Conversion & Casting (06-type-conversion)

> **Topic Overview**: Data type conversion in Python: implicit type coercion vs explicit type casting, built-in constructor functions (`int()`, `float()`, `str()`, `bool()`, `list()`, `dict()`), string parsing with custom radixes, conversion dunder protocols (`__int__`, `__float__`, `__index__`), and safe conversion patterns.

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
- **Problem Statement**: External data entering a system (from HTTP request bodies, database queries, environment variables, CSV files) always arrives as strings or raw byte buffers. Performing calculations directly on raw string representations causes runtime crashes or logic errors.
- **Mental Model**: Python is strongly typed, meaning it refuses to perform silent implicit type coercion between incompatible types (e.g., adding a string and an integer raises `TypeError`). Conversions must be explicit, invoking constructor functions or data model protocols.
- **When to Use**: Validating user input, parsing configuration strings, converting collections (`list` to `set`), and translating domain entities.
- **When NOT to Use**: Avoid unnecessary repeated conversions within tight computational loops (parse once at the boundary, keep typed internally).

---

## 2. Core Concepts
1. **Strong Typing vs Implicit Coercion**:
   - Python performs implicit numeric coercion *only* between compatible numeric types (e.g. `int + float -> float`).
   - Python *never* implicitly coerces numbers to strings or strings to numbers.
2. **Explicit Constructor Casting**:
   - `int(x, base=10)`: Converts integers, floats (truncating towards zero), or numeric strings to `int`. Supports custom bases (2 to 36).
   - `float(x)`: Converts strings or numbers to 64-bit IEEE 754 floats. Supports `"inf"`, `"-inf"`, and `"nan"`.
   - `str(x)`: Converts any object to its human-readable string representation via `__str__()`.
   - `bool(x)`: Evaluates truthiness via `__bool__()` or `__len__()`.
3. **The Data Model Conversion Protocols**:
   - `__int__(self)`: Called by `int(obj)`.
   - `__float__(self)`: Called by `float(obj)`.
   - `__str__(self)`: Called by `str(obj)` and `print()`.
   - `__repr__(self)`: Called by `repr(obj)` and interactive REPL.
   - `__index__(self)`: Called when an object is used as a sequence slice index or binary bit shift.
4. **Binary & Radix Conversions**:
   - `bin(x)`: Formats integer to binary string (`"0b..."`).
   - `oct(x)`: Formats integer to octal string (`"0o..."`).
   - `hex(x)`: Formats integer to hexadecimal string (`"0x..."`).

---

## 3. How It Works Internally

How `int()` parses strings in CPython (`Objects/longobject.c`):

```text
Invocation: `int("1024", base=10)`
       │
       ▼
[ PyLong_FromString ]
       │
       ├─► Validates string length <= sys.get_int_max_str_digits() (4300 digits)
       ├─► Skips leading and trailing ASCII whitespace
       ├─► Parses optional '+' or '-' sign
       │
       ▼
Digit Accumulation Loop (Base 10):
  Iterates over characters '1', '0', '2', '4':
    digit_val = char - '0'
    acc = (acc * 10) + digit_val
       │
       ▼
Allocates `PyLongObject` with appropriate 30-bit digits
Returns heap pointer to integer object
```

---

## 4. Syntax & API Reference

### Built-in Type Constructors
```python
int("42")               # 42
int("1010", base=2)     # 10
int("FF", base=16)      # 255
int(3.99)               # 3 (truncates toward zero, does NOT round!)

float("3.14159")        # 3.14159
float("1e-4")           # 0.0001
float("inf")            # Positive infinity

str(123)                # "123"
bool(0)                 # False
list((1, 2, 3))         # [1, 2, 3]
set([1, 2, 2, 3])       # {1, 2, 3}
dict([("a", 1), ("b", 2)])  # {'a': 1, 'b': 2}
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: String to Integer and Radix Conversions
```python
raw_str = "1024"
as_int = int(raw_str)
as_hex = hex(as_int)
as_bin = bin(as_int)

print(f"Parsed int: {as_int}")
print(f"Hex:        {as_hex}")
print(f"Bin:        {as_bin}")
```
**Output**:
```text
Parsed int: 1024
Hex:        0x400
Bin:        0b10000000000
```

---

### 2. Real-World Production Example: Safe Type Casting with Validation
```python
from typing import TypeVar, Callable

T = TypeVar("T")

def safe_cast(val: object, target_type: Callable[[object], T], default: T) -> T:
    """Safely converts a raw input to a target type, returning default on failure."""
    if val is None:
        return default
    try:
        return target_type(val)
    except (ValueError, TypeError):
        return default

# Testing with real-world query parameters:
print("Port valid:   ", safe_cast("8080", int, 3000))
print("Port invalid: ", safe_cast("invalid", int, 3000))
print("Float valid:  ", safe_cast("99.95", float, 0.0))
print("Float invalid:", safe_cast(None, float, 0.0))
```
**Output**:
```text
Port valid:    8080
Port invalid:  3000
Float valid:   99.95
Float invalid: 0.0
```

---

### 3. Edge Case: Implementing the `__index__` Protocol for Slicing
```python
class HexOffset:
    """A custom class that can be used directly as a slice index."""
    def __init__(self, offset_hex: str) -> None:
        self.val = int(offset_hex, 16)

    def __index__(self) -> int:
        # __index__ must return an exact integer!
        return self.val

data = ["A", "B", "C", "D", "E", "F", "G"]
offset = HexOffset("0x03")  # Index 3

print("Indexed using custom object:", data[offset])
print("Sliced using custom object: ", data[:offset])
```
**Output**:
```text
Indexed using custom object: D
Sliced using custom object:  ['A', 'B', 'C']
```

---

### 4. Failure Case: Attempting to Cast Floating-Point String to `int` Directly
```python
raw_price = "19.99"

try:
    # int() directly on a string containing a decimal point fails!
    converted = int(raw_price)
except ValueError as exc:
    print(f"Caught expected error: {type(exc).__name__}: {exc}")

# Correct two-step conversion:
int_price = int(float(raw_price))
print("Converted via float intermediate:", int_price)
```
**Output**:
```text
Caught expected error: ValueError: invalid literal for int() with base 10: '19.99'
Converted via float intermediate: 19
```

---

### 5. Idiomatic Pythonic Pattern: Converting Sequences to Sets for $O(1)$ Deduplication
```python
incoming_user_ids = ["usr_1", "usr_2", "usr_1", "usr_3", "usr_2"]

# Explicitly cast to set to deduplicate
unique_ids = sorted(set(incoming_user_ids))
print("Deduplicated IDs:", unique_ids)
```
**Output**:
```text
Deduplicated IDs: ['usr_1', 'usr_2', 'usr_3']
```

---

## 6. Edge Cases & Pitfalls
1. **`int()` Truncates, Does Not Round**: `int(5.99)` evaluates to `5`, discarding the fractional part. Use `round()` or `math.floor()` / `math.ceil()` when mathematical rounding is required.
2. **Boolean Casts on Non-Empty Falsy Strings**: `bool("False")` evaluates to `True`! Because `"False"` is a non-empty string, Python's truthiness rules consider it truthy.
3. **Empty String Conversions**: `int("")` and `float("")` raise `ValueError: could not convert string to float`. Always validate or strip strings before conversion.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Calling `bool("False")` expecting `False` | Any non-empty string is truthy in Python | Silent logic error | Check `val.lower() == "true"` |
| Calling `int("19.95")` directly | `int()` rejects fractional string literals | `ValueError: invalid literal for int() with base 10` | Use `int(float("19.95"))` or parse decimal |
| Assuming `int(float_val)` rounds | `int()` truncates toward zero | Rounds down positive numbers, up negative numbers | Use `round()` before `int()` |
| Passing invalid characters to `int(..., base=16)` | Characters outside valid radix range | `ValueError: invalid literal for int()` | Sanitize with regex before parsing |
| Converting dictionary to list | Only extracts the keys, discarding values | Lost dictionary values | Use `list(d.items())` or `list(d.values())` |
| Calling `str()` on custom class without `__str__` | Falls back to default `__repr__` | `<MyClass object at 0x...>` | Implement `__str__` explicitly |

---

## 8. Performance & Complexity
- **`int(str)` Parsing**: $O(n)$ where $n$ is string character count.
- **`list(set)` / `set(list)` Conversion**: $O(n)$ linear time.
- **`int(float)` Conversion**: $O(1)$ constant time.

---

## 9. CPython Internals & Bytecode: The `__index__` Protocol
In CPython C code, sequence indexing does not call `__int__()`; it calls `_PyLong_FromIndex()` which invokes `nb_index`:
- Why? A float implements `__int__()`, but floats cannot be used as sequence indices (`list[1.5]` is invalid).
- The `__index__` protocol guarantees that an object can losslessly represent an exact integer index without truncation or precision loss.

---

## 10. Best Practices
1. Parse and validate input strings at the system boundary (API controller, CLI parser); keep internal data strongly typed.
2. For boolean parsing from strings, use explicit string comparison (`s.lower() in ("true", "1", "yes")`).
3. Handle `ValueError` and `TypeError` when parsing user-submitted numbers.
4. Implement both `__str__` (user-facing) and `__repr__` (debugging) in custom domain classes.

---

## 11. Real-World Case Study: Environment Variable Parsing Engine
A distributed microservice loaded environment variables at boot. In production, an engineer set `ENABLE_FEATURE_FLAGS=False`.
1. The original implementation used `bool(os.environ.get("ENABLE_FEATURE_FLAGS"))`.
2. Because `"False"` is a non-empty string, `bool("False")` evaluated to `True`, inadvertently enabling experimental feature flags in production!
3. The team replaced loose casting with a strict parsing utility:
```python
def parse_bool_env(env_var: str, default: bool = False) -> bool:
    val = os.environ.get(env_var)
    if val is None:
        return default
    return val.strip().lower() in ("true", "1", "yes", "on")
```

---

## 12. Interview Questions

### Q1: Why does `bool("False")` evaluate to `True` in Python?
<details>
<summary><strong>Answer</strong></summary>
Python evaluates the truthiness of strings based on their length (`__len__()`). Any string with a length greater than zero is truthy, regardless of the characters it contains. To parse boolean strings safely, one must explicitly compare the normalized string against known true values: `val.lower() in ("true", "1", "yes")`.
</details>

### Q2: Why does `int("10.5")` fail with a `ValueError`, but `int(10.5)` succeeds?
<details>
<summary><strong>Answer</strong></summary>
When given a float literal, `int()` converts the floating point number to an integer by truncating its fractional part. When given a string, `int()` parses the string strictly as an integer literal in the specified radix (base 10 by default). Because `"10.5"` contains a decimal point, it is not a valid integer literal, so `int()` raises `ValueError`.
</details>

### Q3: What is the purpose of the `__index__` dunder method?
<details>
<summary><strong>Answer</strong></summary>
`__index__` is used to convert an arbitrary object losslessly into an exact integer whenever Python requires an integer index (such as sequence slicing `items[obj]` or range generation `range(obj)`). Python purposefully does not use `__int__` for indexing because floats implement `__int__`, and using floating-point numbers as slice indices would lead to subtle off-by-one errors.
</details>

### Q4: What is the difference between `__str__` and `__repr__`?
<details>
<summary><strong>Answer</strong></summary>
- `__repr__` is intended to provide an unambiguous, technical representation of the object, ideally valid Python code that could recreate the object (e.g. `Point(x=1, y=2)`), primarily used by developers and in debugging.
- `__str__` is intended to provide a human-readable, friendly representation of the object (e.g. `(1, 2)`), used by `print()` and `str()`.
If `__str__` is not defined on a class, Python automatically falls back to `__repr__`.
</details>

### Q5: How do you parse an integer from hexadecimal or binary strings in Python?
<details>
<summary><strong>Answer</strong></summary>
By passing the appropriate radix base to the `int()` constructor:
- Binary: `int("1010", base=2)` returns `10`.
- Hexadecimal: `int("FF", base=16)` returns `255`.
- Auto-detect: `int("0xFF", base=0)` or `int("0b1010", base=0)` inspects the prefix automatically.
</details>

### Q6: What does `dict(iterable)` require as its input?
<details>
<summary><strong>Answer</strong></summary>
The `dict()` constructor requires an iterable of key-value pairs (2-item iterables like tuples or lists), such as `dict([("a", 1), ("b", 2)])`. Alternatively, it accepts keyword arguments (`dict(a=1, b=2)`) or an existing mapping.
</details>

### Q7: Does `int()` round or truncate floating-point numbers?
<details>
<summary><strong>Answer</strong></summary>
`int()` always truncates floating-point numbers towards zero. Positive floats are rounded down (`int(3.9) == 3`), and negative floats are rounded up towards zero (`int(-3.9) == -3`).
</details>

### Q8: What exception is raised when converting a string containing non-ASCII digits with `int()`?
<details>
<summary><strong>Answer</strong></summary>
Interestingly, CPython's `int()` supports Unicode decimal digits natively! For example, `int("٤٢")` (Arabic-Indic digits for 42) or `int("１２３")` (full-width digits) successfully evaluate to their integer values (`42` and `123`). However, if a character is not recognized as a valid Unicode decimal digit, a `ValueError` is raised.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Strict Boolean Parser
Write a function `parse_bool_strict(text)` that returns `True` for `"true"` or `"1"`, `False` for `"false"` or `"0"`, and raises `ValueError` for all other inputs.
<details>
<summary><strong>Solution</strong></summary>

```python
def parse_bool_strict(text: str) -> bool:
    normalized = text.strip().lower()
    if normalized in ("true", "1"):
        return True
    elif normalized in ("false", "0"):
        return False
    raise ValueError(f"Cannot parse boolean from: '{text}'")

print("Parsed true:", parse_bool_strict("True"))
print("Parsed false:", parse_bool_strict("0"))
```
</details>

### Challenge 2 (Easy): Safe String to Float with Default
Write a function `parse_float(val, default=0.0)` that attempts to convert a string to `float`, returning `default` if the string is empty or invalid.
<details>
<summary><strong>Solution</strong></summary>

```python
def parse_float(val: str | None, default: float = 0.0) -> float:
    if not val:
        return default
    try:
        return float(val.strip())
    except ValueError:
        return default

print("Valid float:", parse_float("12.34"))
print("Invalid float:", parse_float("abc", default=-1.0))
```
</details>

### Challenge 3 (Medium): Roman Numeral to Integer Converter
Write a function `roman_to_int(s)` that converts a valid Roman numeral string (e.g. `"MCMXCIV"`) into an integer (`1994`).
<details>
<summary><strong>Solution</strong></summary>

```python
def roman_to_int(s: str) -> int:
    values = {"I": 1, "V": 5, "X": 10, "L": 50, "C": 100, "D": 500, "M": 1000}
    total = 0
    prev = 0

    for char in reversed(s.upper()):
        curr = values.get(char, 0)
        if curr < prev:
            total -= curr
        else:
            total += curr
        prev = curr

    return total

print("MCMXCIV =", roman_to_int("MCMXCIV"))  # 1994
```
</details>

### Challenge 4 (Medium): Custom Temperature Class with Protocols
Implement a `Temperature` class that wraps a Celsius float and implements `__float__`, `__int__`, and `__str__` (displaying `"XX.X °C"`).
<details>
<summary><strong>Solution</strong></summary>

```python
class Temperature:
    def __init__(self, celsius: float) -> None:
        self.celsius = celsius

    def __float__(self) -> float:
        return float(self.celsius)

    def __int__(self) -> int:
        return int(self.celsius)

    def __str__(self) -> str:
        return f"{self.celsius:.1f} °C"

t = Temperature(23.78)
print("String representation:", str(t))
print("Float conversion:     ", float(t))
print("Int truncation:       ", int(t))
```
</details>

### Challenge 5 (Hard): Custom Hex Color Parser to RGB Tuple
Write a function `hex_to_rgb(hex_code)` that parses a hex color string (e.g. `"#FF5733"` or `"FF5733"`) into an `(r, g, b)` integer tuple using base-16 conversions.
<details>
<summary><strong>Solution</strong></summary>

```python
def hex_to_rgb(hex_code: str) -> tuple[int, int, int]:
    cleaned = hex_code.lstrip("#")
    if len(cleaned) == 3:
        cleaned = "".join(c * 2 for c in cleaned)
    if len(cleaned) != 6:
        raise ValueError(f"Invalid hex color code: '{hex_code}'")

    r = int(cleaned[0:2], 16)
    g = int(cleaned[2:4], 16)
    b = int(cleaned[4:6], 16)
    return r, g, b

print("RGB for #FF5733:", hex_to_rgb("#FF5733"))
print("RGB for #FFF:   ", hex_to_rgb("#FFF"))
```
</details>

### Challenge 6 (Hard): Custom Indexable Sequence Wrapper
Implement a custom collection class `CustomSequence` that implements `__getitem__` and `__index__` so that its instances can be used as indices for other Python sequences.
<details>
<summary><strong>Solution</strong></summary>

```python
class IndexPointer:
    def __init__(self, target_idx: int) -> None:
        self.target_idx = target_idx

    def __index__(self) -> int:
        return self.target_idx

ptr = IndexPointer(2)
alphabet = ["a", "b", "c", "d", "e"]
print("Indexed via IndexPointer:", alphabet[ptr])
```
</details>
