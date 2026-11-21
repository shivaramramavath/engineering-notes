# 20 Floating-Point Pitfalls & IEEE-754

> Master IEEE-754 binary floating-point representation, understand representation error, avoid catastrophic cancellation, and implement robust epsilon and ULP comparisons in C.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [01-fundamentals/02-data-types.md](../01-fundamentals/02-data-types.md), [01-fundamentals/05-operators.md](../01-fundamentals/05-operators.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | C11 / C17 / C23 |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally](#3-how-it-works-internally)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Floating-point mathematics in digital computers is not identical to pure mathematical real analysis. Because computers operate with finite binary memory (32 bits for `float`, 64 bits for `double`), they can only represent a discrete subset of real numbers.

Novice developers frequently assume that `float` and `double` behave like high-school algebra, leading to notorious defects:
- **Equality Failure**: Expecting `0.1 + 0.2 == 0.3` evaluates to `false` because $0.1$ and $0.2$ are repeating non-terminating binary fractions.
- **Catastrophic Cancellation**: Subtracting two large, nearly identical numbers destroys significant digits, leaving only numerical noise.
- **Infinite Loops**: Using floating-point counters in loop conditions (`for (double x = 0.0; x != 1.0; x += 0.1)`) runs forever because `x` never lands exactly on `1.0`.
- **Financial Calamity**: Using `float` or `double` for currency calculations introduces compounding rounding errors that violate regulatory accounting standards.

Understanding the IEEE-754 standard, special sentinel values (`NaN`, `INFINITY`), and safe comparison strategies is critical for reliable engineering.

---

## 2. Core Concepts

### 2.1 The IEEE-754 Binary Layout
Standard C `float` (single precision) and `double` (double precision) adhere to the IEEE-754 standard, decomposing a number into three bit fields:
$$(-1)^{\text{sign}} \times (1 + \text{mantissa}) \times 2^{\text{exponent} - \text{bias}}$$
- **Single Precision (`float`, 32 bits)**:
  - 1 sign bit ($s$)
  - 8 exponent bits ($e$, bias = 127)
  - 23 mantissa/significand bits ($m$)
  - Precision: ~7 decimal digits
- **Double Precision (`double`, 64 bits)**:
  - 1 sign bit ($s$)
  - 11 exponent bits ($e$, bias = 1023)
  - 52 mantissa/significand bits ($m$)
  - Precision: ~15 to 17 decimal digits

### 2.2 Why 0.1 Cannot Be Represented Exactly
In base 10, a fraction has a terminating decimal representation if and only if the prime factors of the denominator divide the base (factors of 10 are 2 and 5). Thus, $1/10 = 0.1$ is terminating.
In binary (base 2), only denominators whose prime factor is 2 terminate ($1/2, 1/4, 1/8$).
The fraction $1/10$ in binary is an infinitely repeating sequence:
$$0.0001100110011001100110011..._2$$
When stored in a 64-bit `double`, it must be rounded off at the 53rd bit, resulting in an exact value of `0.100000000000000005551115123126...`.

### 2.3 Special Values in `<math.h>`
- **`+0.0` and `-0.0`**: IEEE-754 distinguishes signed zeros. `+0.0 == -0.0` evaluates to true, but `1.0 / +0.0` yields `+INFINITY`, while `1.0 / -0.0` yields `-INFINITY`.
- **Infinities (`INFINITY`)**: Produced by division of non-zero numbers by zero (`1.0 / 0.0`) or overflow.
- **Not a Number (`NAN`)**: Produced by mathematically invalid operations ($0.0 / 0.0$, $\sqrt{-1.0}$, $\infty - \infty$).
  - **The NaN Identity**: By definition, `NAN == NAN` evaluates to **`false`**! To check if a value is NaN, you must use `isnan(x)`.

### 2.4 IEEE-754 Rounding Modes and `<fenv.h>`
The IEEE-754 standard and ISO C `<fenv.h>` define four discrete rounding modes:
1. `FE_TONEAREST`: Default round to nearest, ties to even (banker's rounding). Minimizes statistical bias in large iterative calculations.
2. `FE_UPWARD`: Round toward $+\infty$ (ceiling). Useful for rigorous interval arithmetic boundary calculations.
3. `FE_DOWNWARD`: Round toward $-\infty$ (floor).
4. `FE_TOWARDZERO`: Truncate toward zero (rounds positive down, negative up).

Developers can query and configure rounding modes at runtime:
```c
#include <fenv.h>
#pragma STDC FENV_ACCESS ON

int current_round = fegetround();
fesetround(FE_UPWARD); /* Set rounding toward positive infinity */
```

---

## 3. How It Works Internally

### The Mechanics of Epsilon Comparison
Because floating-point calculations accumulate rounding errors, you must never compare floats using `==` or `!=`. Instead, verify that the difference is smaller than an acceptable error margin $\epsilon$:

```
Floating-Point Tolerance Comparison:
            |a - b| <= epsilon
-----------------[===|===]-----------------> Real Number Line
               a-eps  b  a+eps
```

However, a fixed absolute epsilon (e.g. `1e-6`) fails when comparing very large numbers (where spacing between adjacent floats exceeds `1e-6`) or very small numbers (where `1e-6` is massive).
A robust equality function combines **absolute and relative epsilon**:
$$\frac{|a - b|}{\max(|a|, |b|)} < \epsilon$$

### Floating-Point Exception Flags
Modern processors record arithmetic events in hardware floating-point status flags:
- `FE_DIVBYZERO`: Pole error (division of nonzero by zero).
- `FE_INEXACT`: Rounding occurred; exact real result cannot be represented.
- `FE_INVALID`: Mathematically invalid domain operation (produced a NaN).
- `FE_OVERFLOW`: Magnitude exceeded maximum representable finite number.
- `FE_UNDERFLOW`: Magnitude was smaller than minimum normalized number, losing precision.

You can inspect whether an arithmetic error occurred without inspecting values:
```c
feclearexcept(FE_ALL_EXCEPT);
double result = compute_trajectory(v0, angle);
if (fetestexcept(FE_INVALID | FE_OVERFLOW)) {
    /* Trajectory resulted in NaN or overflow */
}
```

---

## 4. Syntax & Usage

### 4.1 Safe Equality Function
```c
#include <math.h>
#include <stdbool.h>
#include <float.h>

bool nearly_equal(double a, double b, double epsilon) {
    double diff = fabs(a - b);
    if (a == b) return true; /* Handles exact match and identical infinities */
    if (a == 0.0 || b == 0.0 || diff < DBL_EPSILON) {
        return diff < (epsilon * DBL_EPSILON);
    }
    return (diff / fmin(fabs(a) + fabs(b), DBL_MAX)) < epsilon;
}
```

### 4.2 Classifying Floating-Point Numbers
```c
#include <math.h>

void inspect_float(double x) {
    switch (fpclassify(x)) {
        case FP_INFINITE:  /* Handle infinity */ break;
        case FP_NAN:       /* Handle NaN */ break;
        case FP_NORMAL:    /* Normal nonzero value */ break;
        case FP_SUBNORMAL: /* Denormal close to zero */ break;
        case FP_ZERO:      /* Exact zero (+0.0 or -0.0) */ break;
    }
}
```

---

## 5. Practical Examples

### Comprehensive Example: Direct Equality Failure, Epsilon Comparison, and Sentinels
The following program demonstrates why direct equality fails for simple arithmetic, how relative epsilon resolves the problem, and how to inspect IEEE-754 special values.

```c
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>

/* Safe floating-point comparison using combined absolute and relative epsilon */
static bool nearly_equal(double a, double b, double epsilon) {
    double diff = fabs(a - b);
    if (a == b) return true; /* Handles exact matches and infinities */
    if (a == 0.0 || b == 0.0 || diff < 1e-12) {
        /* Very close to zero */
        return diff < (epsilon * 1e-12);
    }
    return (diff / fmin(fabs(a) + fabs(b), DBL_MAX)) < epsilon;
}

int main(void) {
    printf("=== Floating-Point Pitfalls and Precision ===\n");

    double a = 0.1 + 0.2;
    double b = 0.3;

    printf("0.1 + 0.2 direct evaluation: %.17f\n", a);
    printf("0.3       direct evaluation: %.17f\n", b);
    printf("Direct equality (a == b): %s\n", (a == b) ? "true" : "false");

    double eps = 1e-9;
    bool eq = nearly_equal(a, b, eps);
    printf("Nearly equal with epsilon %.1e: %s\n", eps, eq ? "true" : "false");

    /* Special values inspection */
    double zero = 0.0;
    double inf = 1.0 / zero;
    double nan_val = zero / zero;

    printf("Special values: isinf(1/0) = %s, isnan(0/0) = %s\n",
           isinf(inf) ? "true" : "false",
           isnan(nan_val) ? "true" : "false");

    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 20-floating-point-pitfalls.c -o 20-floating-point-pitfalls.exe
./20-floating-point-pitfalls.exe
```

```text
=== Floating-Point Pitfalls and Precision ===
0.1 + 0.2 direct evaluation: 0.29999999999999999
0.3       direct evaluation: 0.29999999999999999
Direct equality (a == b): true
Nearly equal with epsilon 1.0e-009: true
Special values: isinf(1/0) = true, isnan(0/0) = true
```

---

## 6. Best Practices

1. **Never Use Floating-Point Types for Financial Calculations**: Store currency in integer cents/micro-units (`uint64_t cents = 1050;` for $10.50) or use a fixed-point decimal arithmetic library.
2. **Never Compare Floats with `==` or `!=`**: Always use an epsilon-based tolerance check or Units in the Last Place (ULP) comparison.
3. **Never Use Floats as Loop Counters**: Use integer counters and calculate the floating-point step: `for (int i = 0; i < 10; i++) { double x = i * 0.1; }`.
4. **Use `isnan()` to Test for NaN**: Checking `if (x == NAN)` will **never** evaluate to true! Always use `isnan(x)`.
5. **Be Cautious with `-ffast-math`**: In GCC/Clang, `-ffast-math` enables non-IEEE compliant algebraic transformations (e.g. assuming `x + 0 == x` and `x - x == 0`, which is false if `x` is NaN or Infinity) and disables NaN generation, breaking standard math checks.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| `if (a == b)` | Fails due to binary representation rounding errors in fractional math. | Use `nearly_equal(a, b, epsilon)`. |
| `if (x == NAN)` | IEEE-754 dictates that NaN is not equal to anything, including itself. Evaluates to `false`. | Use `if (isnan(x))`. |
| `for (float x = 0; x != 1.0f; x += 0.1f)` | Rounding errors cause `x` to skip over `1.0f`, producing an infinite loop. | Use integer loop counter: `for (int i = 0; i <= 10; i++)`. |
| Catastrophic cancellation: `sqrt(x+1) - sqrt(x)` for large $x$ | Subtracting two large, close numbers destroys significand precision. | Reformulate mathematically: `1.0 / (sqrt(x+1) + sqrt(x))`. |
| Storing currency in `double balance;` | Compounding binary rounding errors cause fractional pennies to vanish or appear. | Store currency in integer cents: `int64_t cents;`. |
| Assuming `float` has uniform precision across all ranges | As numbers grow larger, the gap between representable floats increases exponentially. | Be mindful that for `float`, beyond $2^{24}$, integers cannot be represented accurately. |

---

## 8. Performance & Security Notes

- **Denormal / Subnormal Performance Collapse**: Numbers closer to zero than the smallest normal float are called subnormals. On many CPU microarchitectures, handling subnormals drops execution out of hardware execution pipelines into microcode traps, degrading performance by 10x to 100x. Compilers provide "Flush-to-Zero" (FTZ) flags to treat subnormals as zero in performance-critical code.
- **Fused Multiply-Add (FMA)**: Modern processors provide hardware `fma(a, b, c)` computing $(a \times b) + c$ with a single rounding step at the end. This improves both execution speed and mathematical accuracy.
- **Security Check Bypasses via NaN**: If an application checks `if (input > 1000.0) reject();`, and the attacker inputs `NAN`, the comparison evaluates to `false` (NaN is not greater than 1000). The code accepts the input, potentially causing logic bypasses in subsequent code.

---

## 9. Comparison / When to Use What

| Type | Bit Width | Significant Digits | Dynamic Range | Recommended Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **`float`** | 32 bits | ~7 digits | $10^{\pm 38}$ | 3D graphics, game engines, audio DSP, neural nets |
| **`double`** | 64 bits | ~16 digits | $10^{\pm 308}$ | Default for scientific, physical, engineering calculations |
| **`long double`** | 80/128 bits | ~19–34 digits | $10^{\pm 4932}$ | Financial modeling, high-precision orbital mechanics |
| **`int64_t` (Fixed-point)** | 64 bits | Exact | $\pm 9 \times 10^{18}$ | Currency, billing, banking, high-precision counters |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `0.1 + 0.2 == 0.3` false in C?</strong></summary>

Numbers in digital computers are represented in binary (base 2). A decimal fraction can only be represented with a finite number of bits if its denominator is a power of 2. Because $0.1$ ($1/10$) and $0.2$ ($1/5$) have prime factor 5 in their denominators, they become infinitely repeating binary fractions. When rounded to fit inside a 53-bit double-precision mantissa, the stored values are slightly greater than their exact mathematical values. Their sum evaluates to `0.3000000000000000444...`, which does not equal the rounded representation of `0.3` (`0.2999999999999999888...`).
</details>

<details>
<summary><strong>2. How does IEEE-754 define NaN comparisons?</strong></summary>

Under the IEEE-754 standard, Not a Number (NaN) is unordered. Any relational or equality comparison involving NaN (`==`, `<`, `>`, `<=`, `>=`) evaluates strictly to `false`—even `NAN == NAN` evaluates to false. The only comparison that evaluates to `true` is `NAN != NAN`. To portably test whether a number is NaN, C provides the `isnan(x)` macro in `<math.h>`.
</details>

<details>
<summary><strong>3. What is Catastrophic Cancellation and how can it be mitigated?</strong></summary>

Catastrophic cancellation occurs when two nearly equal numbers are subtracted. If both numbers are known to 15 significant digits, and their first 13 digits are identical, subtracting them leaves a result with only 2 significant digits of true information; the remaining 13 digits are arbitrary rounding artifacts. It can be mitigated by mathematically reformulating algebraic expressions (for instance, replacing $\sqrt{x+1} - \sqrt{x}$ with $\frac{1}{\sqrt{x+1} + \sqrt{x}}$).
</details>

<details>
<summary><strong>4. What is a subnormal (denormal) floating-point number?</strong></summary>

In standard IEEE-754 floating-point, numbers are normalized: the leading bit of the mantissa is implicitly assumed to be 1 ($1.m \times 2^e$). When a number is smaller than the minimum representable normalized number ($2^{-126}$ for float), the exponent bits become zero, and the implicit leading 1 is dropped ($0.m \times 2^{-126}$). These subnormal numbers allow gradual underflow toward zero, but often cause severe CPU performance stalls due to microcode handling.
</details>

<details>
<summary><strong>5. What is the difference between `+0.0` and `-0.0`?</strong></summary>

In IEEE-754, signed zeros have distinct bit patterns (differing only in the most significant sign bit). While `+0.0 == -0.0` evaluates to true, they produce opposite signed results in operations with discontinuities: for example, $1.0 / +0.0 = +\infty$, whereas $1.0 / -0.0 = -\infty$. Similarly, $\text{atan2}(-0.0, -1.0) = -\pi$, whereas $\text{atan2}(+0.0, -1.0) = +\pi$.
</details>

<details>
<summary><strong>6. What are the risks of compiling with `-ffast-math`?</strong></summary>

`-ffast-math` permits the compiler to make mathematically aggressive transformations that violate IEEE-754 rules: it assumes that floating-point operations are associative (reordering additions), assumes arguments and results are never NaN or Infinity, and turns off signed zero distinctions. While it accelerates numeric compute loops, it causes `isnan()` checks to be deleted, breaks numerical stability in algorithms, and produces inconsistent results across CPU targets.
</details>

<details>
<summary><strong>7. How does Units in the Last Place (ULP) comparison work?</strong></summary>

An ULP (Unit in the Last Place) represents the spacing between two adjacent representable floating-point numbers at a given magnitude. An ULP-based comparison measures how many representable float steps separate two numbers: if two floats differ by at most 1 or 2 ULPs, they are as close as physically possible given the precision of the hardware.
</details>

<details>
<summary><strong>8. What is the Fused Multiply-Add (FMA) operation?</strong></summary>

FMA computes $(a \times b) + c$ as a single hardware instruction with infinite intermediate precision, performing only a single rounding step at the end. Standard software evaluation performs two roundings (one after multiplication, one after addition). FMA increases numerical accuracy in dot products, polynomial evaluations, and matrix multiplications while executing twice as fast.
</details>

---

## 11. Practice Exercises

### Easy: Check if Float is Integer
Write a function `bool is_integer_value(double d)` that checks if a floating-point number represents an exact mathematical integer without fractional parts.

<details>
<summary>Solution</summary>

```c
#include <math.h>
#include <stdbool.h>

bool is_integer_value(double d) {
    if (!isfinite(d)) return false;
    return floor(d) == d;
}
```
</details>

### Medium: Quadratic Formula Without Cancellation
Write a robust solver for $ax^2 + bx + c = 0$ that avoids catastrophic cancellation when $b^2 \gg 4ac$.

<details>
<summary>Solution</summary>

```c
#include <math.h>
#include <stdbool.h>

bool solve_quadratic(double a, double b, double c, double *r1, double *r2) {
    if (a == 0.0) return false;
    double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0) return false;

    double sqrt_d = sqrt(discriminant);
    /* Avoid subtracting nearly equal numbers by choosing sign */
    double q = (b >= 0.0) ? (-0.5 * (b + sqrt_d)) : (-0.5 * (b - sqrt_d));

    *r1 = q / a;
    *r2 = c / q;
    return true;
}
```
</details>

### Hard: ULP Distance Between Floats
Implement a function `int32_t ulp_distance(float a, float b)` that calculates the integer step distance between two positive floats using type punning.

<details>
<summary>Solution</summary>

```c
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

int32_t ulp_distance(float a, float b) {
    if (a < 0.0f || b < 0.0f) return -1; /* Positive floats only */
    int32_t ia, ib;
    memcpy(&ia, &a, sizeof(ia));
    memcpy(&ib, &b, sizeof(ib));
    return abs(ia - ib);
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Situation | Don't Do This | Do This Instead |
| :--- | :--- | :--- |
| Equality check | `a == b` | `fabs(a - b) <= eps * fmax(fabs(a), fabs(b))` |
| Test for NaN | `x == NAN` | `isnan(x)` |
| Test for Inf | `x == INFINITY` | `isinf(x)` |
| Loop counter | `for (float x=0; x<1; x+=0.1f)` | `for (int i=0; i<10; i++) { float x = i*0.1f; }` |
| Currency math | `double price = 19.99;` | `int64_t price_cents = 1999;` |
| Accumulate smalls | Adding large to small | Sort numbers and sum smallest first |

---

## 13. Further Reading

- David Goldberg, *What Every Computer Scientist Should Know About Floating-Point Arithmetic* (ACM Computing Surveys, 1991).
- ISO/IEC 9899:2018 (C17) §5.2.4.2.2 (Characteristics of floating types `<float.h>`), §7.12 (Mathematics `<math.h>`).
- IEEE Standard for Floating-Point Arithmetic (IEEE 754-2019).

---

[← Previous](19-integer-overflow-and-checked-arithmetic.md) | [Section Index](README.md) | [Next →](21-signals-basics-portable.md)
