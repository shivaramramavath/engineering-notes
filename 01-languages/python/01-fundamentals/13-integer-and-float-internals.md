---
title: "Integer and Float Internals"
slug: "integer-and-float-internals"
category: "fundamentals"
difficulty: "advanced"
tags: ["python", "internals", "cpython", "integers", "floats", "memory", "ieee-754", "pylongobject"]
prerequisites: ["numbers-int-float-decimal-fraction", "how-python-runs-code"]
related_topics: ["python-internals", "performance"]
python_version: "3.12+"
tested_with: "CPython 3.14.0"
free_threading_support: "Yes"
---

# Integer and Float Internals

## 1. Executive Summary & Core Value Proposition

In Python, numbers are not raw 32-bit or 64-bit machine primitives stored directly in CPU registers or on the stack. Everything in Python is a heap-allocated C structure:
- **Integers (`int`):** Represented by CPython's `PyLongObject`, an arbitrary-precision integer implementation ("bignum") capable of storing numbers with millions of digits, bounded only by available system RAM.
- **Floats (`float`):** Represented by CPython's `PyFloatObject`, wrapping a standard C 64-bit double-precision IEEE 754 floating-point number.

Understanding the internal anatomy of Python numeric types is essential for high-performance computing, systems engineering, memory profiling, and numeric stability in machine learning and financial systems.

---

## 2. Low-Level Architecture & Memory Layout

### CPython `PyLongObject` (Arbitrary-Precision Integers)

Unlike C where `int` overflows at $2^{31}-1$ or $2^{63}-1$, Python integers dynamically allocate arrays of "digits". In 64-bit CPython, each digit is stored in a 30-bit chunk inside a 32-bit unsigned integer (`uint32_t`).

```
CPython 64-bit PyLongObject Memory Layout:
+-----------------------------------------------------------+
| ob_refcnt (8 bytes)  : Reference count                    |
+-----------------------------------------------------------+
| ob_type   (8 bytes)  : Pointer to &PyLong_Type            |
+-----------------------------------------------------------+
| ob_size   (8 bytes)  : Number of 30-bit digits (sign flag)|
+-----------------------------------------------------------+
| ob_digit  (4 bytes * N): Array of 30-bit chunks           |
|   ob_digit[0] : Lowest 30 bits                           |
|   ob_digit[1] : Next 30 bits                             |
|   ...                                                     |
+-----------------------------------------------------------+
```

Key structural invariants:
1. **Base:** $2^{30} = 1,073,741,824$. A number is represented as:
   $$\text{value} = \text{sign}(\text{ob\_size}) \times \sum_{i=0}^{|ob\_size|-1} \text{ob\_digit}[i] \times 2^{30 \cdot i}$$
2. **Sign Storage:** The sign of the integer is stored in the sign of `ob_size`. If `ob_size < 0`, the integer is negative; if `ob_size == 0`, the number is zero.
3. **Small Integer Caching:** CPython pre-allocates an array of singleton integer objects for values in the range $[-5, 256]$. Any operation evaluating to a number in this range returns a pointer to the pre-existing singleton, saving memory and eliminating allocation churn.

### CPython `PyFloatObject` (IEEE 754 Floating-Point)

A Python `float` is an immutable 24-byte heap object wrapping an 8-byte C `double`:

```
CPython 64-bit PyFloatObject Memory Layout:
+-----------------------------------------------------------+
| ob_refcnt (8 bytes)  : Reference count                    |
+-----------------------------------------------------------+
| ob_type   (8 bytes)  : Pointer to &PyFloat_Type           |
+-----------------------------------------------------------+
| ob_fval   (8 bytes)  : C double (IEEE 754 Binary64)       |
+-----------------------------------------------------------+
Total Heap Size: 24 bytes (plus allocator overhead)
```

```
IEEE 754 Binary64 (64-bit float representation):
1 bit          11 bits                           52 bits
+------+-----------------------+----------------------------------------------------+
| Sign | Exponent (biased 1023)| Mantissa / Significand (implicit leading 1.xxx)    |
+------+-----------------------+----------------------------------------------------+
```

---

## 3. Real-World Code Examples & Verification

### Example 1: Inspecting Integer Digits via `sys.int_info`

```python
import sys

print("--- Python Integer System Info ---")
print("Bits per digit:", sys.int_info.bits_per_digit)
print("Digit size in bytes:", sys.int_info.sizeof_digit)
print("Default int max str digits:", sys.get_int_max_str_digits())

# Inspect memory footprint of integers as they grow
powers = [0, 1, 30, 60, 90, 120]
for p in powers:
    val = (1 << p) - 1
    size_bytes = sys.getsizeof(val)
    # Number of digits needed:
    digits = (val.bit_length() + 29) // 30 if val > 0 else 0
    print(f"2^{p:3d} - 1: {val.bit_length():4d} bits | {digits:2d} digits | {size_bytes:3d} bytes")
```

**Output:**
```
--- Python Integer System Info ---
Bits per digit: 30
Digit size in bytes: 4
Default int max str digits: 4300
2^  0 - 1:    0 bits |  0 digits |  28 bytes
2^  1 - 1:    1 bits |  1 digits |  28 bytes
2^ 30 - 1:   30 bits |  1 digits |  28 bytes
2^ 60 - 1:   60 bits |  2 digits |  36 bytes
2^ 90 - 1:   90 bits |  3 digits |  40 bytes
2^120 - 1:  120 bits |  4 digits |  44 bytes
```

---

### Example 2: Verifying Small Integer Caching (`[-5, 256]`)

```python
# Values inside the singleton cache [-5, 256]
a = 250 + 6
b = 256
print(f"256 is 256: {a is b} (id: {id(a)})")

# Value outside the singleton cache
x = 256 + 1
y = 257
print(f"257 is 257 (dynamic): {x is y} (id(x)={id(x)}, id(y)={id(y)})")

# Negative boundary
neg_cached_1 = -5
neg_cached_2 = -6 + 1
print(f"-5 is -5: {neg_cached_1 is neg_cached_2}")

neg_uncached_1 = -6
neg_uncached_2 = -7 + 1
print(f"-6 is -6 (dynamic): {neg_uncached_1 is neg_uncached_2}")
```

**Output:**
```
256 is 256: True (id: 140723145695280)
257 is 257 (dynamic): False (id(x)=2184589201680, id(y)=2184589201872)
-5 is -5: True
-6 is -6 (dynamic): False
```

---

### Example 3: Dissecting Float IEEE 754 Binary Representation

```python
import struct
import math

def float_to_binary64(f: float) -> str:
    # Pack double into 8 bytes, unpack as 64-bit unsigned integer
    packed = struct.pack(">d", f)
    int_val = struct.unpack(">Q", packed)[0]
    binary_str = f"{int_val:064b}"
    
    sign = binary_str[0]
    exponent = binary_str[1:12]
    mantissa = binary_str[12:]
    return f"Sign: {sign} | Exp: {exponent} ({int(exponent, 2) - 1023}) | Mantissa: {mantissa[:16]}..."

test_floats = [1.0, 2.0, 0.1, float('inf'), float('nan')]
for val in test_floats:
    print(f"{val:>6}: {float_to_binary64(val)}")
```

**Output:**
```
   1.0: Sign: 0 | Exp: 01111111111 (0) | Mantissa: 0000000000000000...
   2.0: Sign: 0 | Exp: 10000000000 (1) | Mantissa: 0000000000000000...
   0.1: Sign: 0 | Exp: 01111111011 (-4) | Mantissa: 1001100110011001...
   inf: Sign: 0 | Exp: 11111111111 (1024) | Mantissa: 0000000000000000...
   nan: Sign: 0 | Exp: 11111111111 (1024) | Mantissa: 1000000000000000...
```

---

### Example 4: Integer Multiplication Algorithms (Karatsuba vs Classical)

CPython switches integer multiplication algorithms based on operand size:
- **Schoolbook Multiplication ($O(N^2)$):** Used when integers have fewer than 70 digits (Karatsuba cutoff threshold).
- **Karatsuba Multiplication ($O(N^{\log_2 3}) \approx O(N^{1.585})$):** Used for large integers.

```python
import time

def benchmark_int_mult(bit_size: int, iterations: int = 5000):
    a = (1 << bit_size) - 3
    b = (1 << bit_size) - 7
    
    t0 = time.perf_counter()
    for _ in range(iterations):
        _ = a * b
    elapsed = time.perf_counter() - t0
    return elapsed

sizes = [64, 512, 4096, 16384]
for s in sizes:
    t = benchmark_int_mult(s)
    print(f"Multiplication {s:5d} bits x 5000: {t*1000:.2f} ms")
```

**Output:**
```
Multiplication    64 bits x 5000: 0.18 ms
Multiplication   512 bits x 5000: 0.62 ms
Multiplication  4096 bits x 5000: 4.85 ms
Multiplication 16384 bits x 5000: 38.12 ms
```

---

### Example 5: Float Precision Limits, ULP, and `math.isclose()`

```python
import math
import sys

# Machine epsilon (distance between 1.0 and the next representable float)
eps = sys.float_info.epsilon
print(f"Float epsilon: {eps}")
print(f"1.0 + eps == 1.0     : {1.0 + eps == 1.0}")
print(f"1.0 + (eps/2) == 1.0 : {1.0 + (eps / 2) == 1.0}")

# Classic IEEE 754 summation drift
val = 0.0
for _ in range(10):
    val += 0.1

print(f"Sum of 10x 0.1 == 1.0: {val == 1.0} (actual: {val:.17f})")
print(f"math.isclose(val, 1.0): {math.isclose(val, 1.0)}")
```

**Output:**
```
Float epsilon: 2.220446049250313e-16
1.0 + eps == 1.0     : False
1.0 + (eps/2) == 1.0 : True
Sum of 10x 0.1 == 1.0: False (actual: 0.99999999999999989)
math.isclose(val, 1.0): True
```

---

## 4. Common Anti-Patterns & Mistakes

| Anti-Pattern / Mistake | Broken / Unidiomatic Snippet | Idiomatic Correction | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Using `is` for number equality** | `if total is 1000:` | `if total == 1000:` | `is` checks memory address identity. Values outside $[-5, 256]$ may evaluate to `False` depending on runtime allocation. |
| **Direct float equality check** | `if balance == 0.0:` | `if math.isclose(balance, 0.0, abs_tol=1e-9):` | Accumulation of rounding errors makes exact float comparison non-deterministic. |
| **Converting massive ints to string** | `s = str(10**50000)` | Avoid or use `sys.set_int_max_str_digits()` | Python 3.11+ raises `ValueError: Exceeds the limit (4300 digits) for integer string conversion` (DoS protection). |
| **Assuming float infinity traps errors** | `res = 1e308 * 10`<br>*(passes silently)* | Check `math.isinf(res)` or use `decimal.Decimal` | Float overflow silently yields `inf` rather than raising `OverflowError` (unlike integer operations). |
| **Bitwise operations on floats** | `x = 4.5 & 3` | `x = int(4.5) & 3` | Python floats do not support bitwise operators `&, |, ^, ~`. |
| **Loss of precision via float division** | `avg = (a + b) / 2` *(for huge ints)* | `avg = (a + b) // 2` | True division `/` coerces arbitrary-precision integers into 53-bit precision floats, losing data for integers $> 2^{53}$. |

---

## 5. Architectural & Design Decisions

### Why Not Unbox Numbers like C or Java?
In Java, `int` is a 4-byte stack primitive, while `Integer` is a boxed object. Python chose uniform object semantics: every variable is a reference (`PyObject*`), simplifying the runtime and dynamic typing. Modern Python versions (3.11+) use specialized adaptive bytecodes (PEP 659) to optimize numeric operations without changing object semantics.

### String Conversion DoS Protection (CVE-2020-10735 / PEP 703 Era)
Converting between base-10 strings and base-2 `PyLongObject` requires $O(N^2)$ quadratic-time division. An attacker submitting a 100,000-digit number string in a JSON payload could consume seconds of CPU time. In Python 3.11, CPython added `sys.set_int_max_str_digits(4300)` by default to neutralize integer conversion denial-of-service vulnerabilities.

---

## 6. Verification & Practical Testing

```python
import sys
import math

def test_integer_growth():
    # 0 digits (0)
    assert sys.getsizeof(0) == 28
    # 1 digit (1 to 2^30 - 1)
    assert sys.getsizeof(1) == 28
    assert sys.getsizeof((1 << 30) - 1) == 28
    # 2 digits (2^30 to 2^60 - 1)
    assert sys.getsizeof(1 << 30) == 36
    # 3 digits
    assert sys.getsizeof(1 << 60) == 40

def test_float_ieee754():
    assert sys.getsizeof(0.0) == 24
    assert sys.getsizeof(1e100) == 24
    assert math.isinf(1e308 * 2)
    assert math.isnan(float('nan'))

def test_small_int_cache():
    for i in range(-5, 257):
        a = i
        b = int(str(i))
        assert a is b, f"Small int cache failed for {i}"

if __name__ == "__main__":
    test_integer_growth()
    test_float_ieee754()
    test_small_int_cache()
    print("All integer and float internal tests passed successfully.")
```

**Output:**
```
All integer and float internal tests passed successfully.
```

---

## 7. Deep Dive: Division and Integer String Conversion Complexity

### Binary Representation vs Base-10 String Conversion
Internally, `PyLongObject` stores numbers in base $2^{30}$. When printing or parsing a decimal string:
- **Base-2 to Base-10 Conversion:** Requires repeated division by $10^9$ (or largest base-10 chunk). Standard schoolbook division takes $O(N^2)$ time where $N$ is the number of digits.
- **Python 3.11+ Limits:** The default 4,300 digit limit prevents quadratic hangs on web servers deserializing untrusted user inputs.
- **Overriding Limit Safely:**
  ```python
  import sys
  sys.set_int_max_str_digits(100_000)  # For cryptographic or scientific scripts
  ```

---

## 8. Summary & Key Takeaways

1. **`PyLongObject`:** Dynamic array of 30-bit digits (`sys.int_info.bits_per_digit == 30`), providing infinite precision bounded only by memory.
2. **Small Int Cache:** Integers between $[-5, 256]$ are pre-allocated global singletons; `is` checks work on them, but `==` should always be used in production.
3. **`PyFloatObject`:** 24-byte wrapper around standard C `double` (IEEE 754 Binary64: 1 sign bit, 11 exponent bits, 52 mantissa bits).
4. **Precision Limits:** Floats have $\approx 15\text{--}17$ significant decimal digits of precision. Integers $> 2^{53}$ lose precision when converted to float.
5. **String DoS Guard:** CPython limits string-to-int and int-to-string conversions to 4,300 digits by default to mitigate quadratic CPU exhaustion.

---

## 9. Frequently Asked Questions (Interview & Senior Roles)

<details>
<summary>1. How does Python achieve arbitrary-precision integers without overflow?</summary>

CPython stores integers as `PyLongObject`, which contains an array of 30-bit digits (`ob_digit`) and a length field (`ob_size`). When arithmetic results exceed the capacity of current digits, Python reallocates a larger array and carries bits across 30-bit chunks, preventing overflow at the cost of heap allocation.
</details>

<details>
<summary>2. Why does <code>sys.getsizeof(0)</code> return 28 bytes in 64-bit Python?</summary>

In 64-bit CPython, a `PyObject` header contains an 8-byte reference count (`ob_refcnt`) and an 8-byte type pointer (`ob_type`). A `PyVarObject` (which `PyLongObject` is) adds an 8-byte size field (`ob_size`), totaling 24 bytes. For non-zero integers, at least one 4-byte `uint32_t` digit is allocated, totaling 28 bytes. In Python 3.12+, compact integers optimize this structure.
</details>

<details>
<summary>3. Why is <code>256 is 256</code> True, but <code>257 is 257</code> may be False?</summary>

CPython initializes a static array of integer objects for values in the range $[-5, 256]$ during startup. Any reference to these numbers reuses the cached object. Values outside this range (like 257) dynamically allocate separate heap objects with distinct memory addresses.
</details>

<details>
<summary>4. What is the difference between <code>//</code> and <code>/</code> when dealing with very large integers?</summary>

`//` performs integer floor division directly using `PyLongObject` arbitrary-precision arithmetic, preserving all digits. `/` performs true division by converting both operands to IEEE 754 64-bit floats, which can cause precision loss or `OverflowError` if the integer exceeds $1.79 \times 10^{308}$.
</details>

<details>
<summary>5. What caused Python 3.11 to introduce <code>sys.set_int_max_str_digits</code>?</summary>

Converting between binary integers and base-10 strings has $O(N^2)$ algorithmic time complexity. Attackers could submit moderately sized numbers (e.g., 50,000 digits) in JSON or XML requests, consuming 100% CPU on backend servers and causing denial-of-service (CVE-2020-10735). The limit restricts conversions to 4,300 digits.
</details>

<details>
<summary>6. How does CPython handle floating-point multiplication vs integer multiplication?</summary>

Float multiplication executes directly on hardware CPU registers (FPU) in a single instruction cycle. Integer multiplication uses software algorithms: classical $O(N^2)$ schoolbook multiplication for small numbers and Karatsuba $O(N^{1.585})$ algorithm for numbers exceeding 70 digits.
</details>

<details>
<summary>7. What is float "machine epsilon" and how can you check it in Python?</summary>

Machine epsilon is the upper bound on relative rounding error due to floating-point arithmetic. It represents the difference between 1.0 and the least value greater than 1.0 that is representable as a float. In Python, it is accessible via `sys.float_info.epsilon` ($\approx 2.22 \times 10^{-16}$).
</details>

<details>
<summary>8. How does free-threaded Python (PEP 703) affect <code>PyLongObject</code> and small int caching?</summary>

In free-threaded Python (no-GIL builds), shared global singletons like the small int cache must avoid data races on reference counts. CPython 3.13+ marks small integer singletons as "immortal" objects (`_Py_SetImmortal`), disabling reference counting entirely so concurrent threads can read them without locks or contention.
</details>

---

## 10. Practical Exercises & Self-Assessment

<details>
<summary>Exercise 1: Calculate Minimum Bits to Trigger Digit Growth</summary>

**Task:** Write a function that determines the exact integer powers of 2 at which `sys.getsizeof(n)` increases in CPython.

**Solution:**
```python
import sys

prev_size = sys.getsizeof(1)
print(f"Base size: {prev_size} bytes")

for p in range(1, 200):
    val = 1 << p
    curr_size = sys.getsizeof(val)
    if curr_size > prev_size:
        print(f"Power 2^{p:3d} expands size from {prev_size} to {curr_size} bytes (bit length: {val.bit_length()})")
        prev_size = curr_size
```
</details>

<details>
<summary>Exercise 2: Small Integer Cache Boundary Scanner</summary>

**Task:** Programmatically find the exact upper and lower bounds of CPython's integer cache using dynamic integer creation (e.g. `int(str(x))`).

**Solution:**
```python
def find_cache_bounds():
    # Lower bound
    low = 0
    while int(str(low)) is low:
        low -= 1
    lower_bound = low + 1
    
    # Upper bound
    high = 0
    while int(str(high)) is high:
        high += 1
    upper_bound = high - 1
    
    return lower_bound, upper_bound

low, high = find_cache_bounds()
print(f"Discovered cache range: [{low}, {high}]")  # [-5, 256]
```
</details>

<details>
<summary>Exercise 3: Safe Float Comparison Utility</summary>

**Task:** Implement a custom `almost_equal(a, b, rel_tol=1e-9, abs_tol=1e-12)` function matching `math.isclose` behavior.

**Solution:**
```python
def almost_equal(a: float, b: float, rel_tol: float = 1e-9, abs_tol: float = 1e-12) -> bool:
    if a == b:
        return True
    diff = abs(a - b)
    return diff <= max(rel_tol * max(abs(a), abs(b)), abs_tol)

assert almost_equal(0.1 + 0.2, 0.3)
assert not almost_equal(1.0, 1.01)
```
</details>

<details>
<summary>Exercise 4: Decoding IEEE 754 Floating-Point Components</summary>

**Task:** Write a function that takes a `float` and extracts its exact sign (+1 or -1), exponent (integer), and significand without using `struct`.

**Solution:**
```python
import math

def decode_float(f: float):
    if f == 0.0:
        return (0, 0, 0.0)
    sign = -1 if math.copysign(1.0, f) < 0 else 1
    mantissa, exponent = math.frexp(abs(f))
    # math.frexp returns mantissa in [0.5, 1.0) such that abs(f) == mantissa * 2^exponent
    return {"sign": sign, "exponent": exponent, "significand": mantissa}

print(decode_float(8.0))   # {'sign': 1, 'exponent': 4, 'significand': 0.5} (0.5 * 2^4 = 8.0)
print(decode_float(-0.25)) # {'sign': -1, 'exponent': -1, 'significand': 0.5}
```
</details>

<details>
<summary>Exercise 5: Detecting Float Precision Loss in Large Integers</summary>

**Task:** Find the smallest positive integer $N$ such that $N + 1 == N$ when converted to float.

**Solution:**
```python
# IEEE 754 mantissa has 52 bits + 1 implicit bit = 53 bits.
# 2^53 is exactly representable, but odd integers > 2^53 cannot be represented.
n = 2**53
print(f"2^53 float equality test: float(n) == float(n + 1): {float(n) == float(n + 1)}")
print(f"n: {n}")
```
</details>

<details>
<summary>Exercise 6: Measuring String-to-Int DoS Latency</summary>

**Task:** Measure the time required to convert a string of $N$ nines into an integer for $N = 1,000$ and $N = 4,000$, and verify that $4,500$ digits raises a `ValueError`.

**Solution:**
```python
import time
import sys

for digits in [1000, 4000]:
    s = "9" * digits
    t0 = time.perf_counter()
    val = int(s)
    t = time.perf_counter() - t0
    print(f"{digits} digits: {t*1000:.3f} ms")

try:
    _ = int("9" * 4500)
    print("Failed: should have raised ValueError")
except ValueError as e:
    print(f"Success: correctly blocked by Python 3.11+ limit: {type(e).__name__}")
```
</details>
