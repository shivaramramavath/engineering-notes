# Type Conversions and Promotions

> The definitive mechanics of C type conversions: integer promotions, the Usual Arithmetic Conversions, sign conversion vulnerabilities, float-to-integer truncation, explicit casting rules, pointer conversion semantics, and strict aliasing.

| Metadata | Details |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [02-data-types.md](./02-data-types.md), [05-operators.md](./05-operators.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard Baseline** | ISO C11 / C17 / C23 |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

In C, type conversions occur constantly—often silently and implicitly without explicit casts. Whenever you perform arithmetic, pass arguments, or compare variables of different types, the compiler applies strict algebraic conversion rules:
1. **The Integer Promotion Surprise**: In C, arithmetic is never performed directly on types narrower than `int` (such as `char` or `short`). They are automatically promoted to `int` first! Consequently, adding two `unsigned short` variables produces a **signed `int`**.
2. **The Sign Conversion Vulnerability**: When comparing a signed `int` to an `unsigned int`, the signed value is implicitly converted to `unsigned int`. A negative number like `-1` becomes `4,294,967,295U`, causing security checks like `if (user_input < max_len)` to fail silently!
3. **Pointer Aliasing Traps**: Casting arbitrary pointers (e.g. `(float *)&my_int`) violates the C strict aliasing rule, causing optimizers under `-O2` to generate broken machine code.

---

## 2. Core Concepts

### Integer Promotion Rules

Any integer type with a conversion rank **less than or equal to the rank of `int` and `unsigned int`** undergoes **Integer Promotion**:
- If `int` can represent all values of the original type, the value is converted to **signed `int`**.
- Otherwise, the value is converted to **`unsigned int`**.

```
Rank Hierarchy:
_Bool < char == signed char == unsigned char < short == unsigned short < int == unsigned int
```

*Critical Subtlety*: On modern 32-bit and 64-bit platforms where `sizeof(short) == 2` and `sizeof(int) == 4`, an `unsigned short` has values $0 \dots 65,535$. Since signed `int` ranges up to $+2,147,483,647$, an `unsigned short` **promotes to signed `int`**, NOT `unsigned int`!

### The Usual Arithmetic Conversions

When two operands of differing types are passed to a binary operator (e.g. `+`, `*`, `==`, `<`):
1. If either operand is `long double`, `double`, or `float`, the other operand is converted to match that floating-point type.
2. Otherwise, both operands undergo **Integer Promotions**.
3. If both operands now have the same type, no further conversion is needed.
4. If both operands are signed or both are unsigned, the operand with lesser conversion rank is converted to the type of the operand with greater rank.
5. If the operand with **unsigned type** has rank $\ge$ the rank of the signed operand, the signed operand is converted to the **unsigned operand's type**.
6. If the signed operand's type can represent all values of the unsigned type, the unsigned operand is converted to the **signed type**.
7. Otherwise, both operands are converted to the **unsigned type corresponding to the signed operand's type**.

---

## 3. How It Works Internally

### Hardware Register Promotion and Zero/Sign Extension

Modern 32-bit and 64-bit microprocessors do not possess native 8-bit or 16-bit ALU computation pipelines; their arithmetic units operate on 32-bit or 64-bit register words. 

When you write:
```c
uint8_t a = 200;
uint8_t b = 100;
uint8_t c = a + b;
```

The compiler translates this to:
```nasm
movzx   eax, BYTE PTR [rbp-1]   ; Zero-extend 'a' to 32-bit register %eax
movzx   edx, BYTE PTR [rbp-2]   ; Zero-extend 'b' to 32-bit register %edx
add     eax, edx                ; 32-bit addition: 200 + 100 = 300 (fits in int!)
mov     BYTE PTR [rbp-3], al    ; Truncate lower 8 bits back into 'c' (300 & 0xFF = 44)
```

The intermediate addition produces `300` in a 32-bit register without overflowing, and the subsequent assignment truncates `300` modulo 256 to `44`.

---

## 4. Syntax & Usage

### 1. Explicit Casting Syntax
```c
/* Standard explicit cast */
double raw_voltage = 3.85;
int quantized = (int)raw_voltage; /* Truncates fractional part toward zero */

/* Void pointer conversions: Implicit in C, explicit required in C++ */
void *generic_memory = malloc(1024);
int *int_array = generic_memory; /* Valid ISO C; no cast required! */
```

### 2. Pointer to Integer Conversion (`uintptr_t`)
```c
#include <stdint.h>

void log_pointer_address(const void *ptr) {
    /* uintptr_t is guaranteed to hold a pointer address without truncation */
    uintptr_t addr = (uintptr_t)ptr;
    printf("Pointer virtual address: 0x%" PRIxPTR "\n", addr);
}
```

---

## 5. Practical Examples

### Type Conversions and Promotions Demonstration Program

```c
/* File: 01-languages/c/01-fundamentals/examples/06-type-conversions.c */
#include <stdio.h>
#include <stdint.h>

int main(void) {
    printf("=== Type Conversions & Integer Promotion Demo ===\n");
    
    /* Integer promotion of unsigned char to signed int */
    unsigned char a = 100;
    unsigned char b = 200;
    /* In expression (a + b), both promote to int! */
    int sum = a + b;
    printf("Unsigned char sum promoted to int: %d\n", sum);

    /* Sign conversion trap demonstration */
    int32_t negative = -1;
    uint32_t positive = 1;
    if ((uint32_t)negative > positive) {
        printf("Caution: (uint32_t)-1 (%u) > 1 is TRUE via unsigned conversion\n", (uint32_t)negative);
    }

    /* Floating point truncation */
    double pi = 3.1415926535;
    int truncated_pi = (int)pi;
    printf("Double %.4f explicitly cast to int: %d\n", pi, truncated_pi);

    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 06-type-conversions.c -o run_06.exe
./run_06.exe
```
Output:
```text
=== Type Conversions & Integer Promotion Demo ===
Unsigned char sum promoted to int: 300
Caution: (uint32_t)-1 (4294967295) > 1 is TRUE via unsigned conversion
Double 3.1416 explicitly cast to int: 3
```

---

## 6. Best Practices

1. **Always Enable `-Wconversion` and `-Wsign-conversion`**: These compiler diagnostic flags expose every implicit narrowing conversion and signed/unsigned comparison before compilation finishes.
2. **Never Compare Signed to Unsigned Variables**: Explicitly cast or validate bounds before comparing `int` to `size_t` or `uint32_t`.
3. **Do Not Cast the Result of `malloc()`**: In C, `void *` implicitly converts to any object pointer type. Writing `int *p = (int *)malloc(n);` can mask a failure to `#include <stdlib.h>` on legacy compilers.
4. **Use `uintptr_t` for Pointer Arithmetic as Integers**: Never cast pointers to `int` or `long`. Only `uintptr_t` or `intptr_t` guarantee no truncation on 64-bit systems.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Comparing `int` to `unsigned int` (`-1 < 1U`) | Signed `-1` is converted to `unsigned int`, wrapping around to `4294967295U`, making the comparison evaluate to `false`! | Ensure both operands are signed or check `if (val >= 0 && (unsigned)val < limit)`. |
| Float to integer truncation overflow | Converting a float exceeding `INT_MAX` (e.g. `(int)1e15f`) invokes undefined behavior. | Validate range before casting: `if (f >= INT_MIN && f <= INT_MAX)`. |
| Casting `int *` to `float *` to reinterpret bits | Violates strict aliasing; optimizer reorders memory reads and writes unpredictably. | Use `memcpy(&flt, &i, sizeof(flt))` or a union. |
| Expecting `(unsigned short)~0` to be unsigned | Due to integer promotion, `~` operates on signed `int`, producing a signed negative integer `-1`! | Explicitly mask: `((unsigned short)~0U)`. |
| Casting `char *` to `int *` without alignment check | Dereferencing an unaligned `int *` on ARM/SPARC causes a hardware `SIGBUS` fault. | Ensure memory is aligned to `_Alignof(int)`. |
| Bitwise operations on promoted signed ints | Shifting negative ints or shifting into the sign bit invokes undefined behavior after promotion. | Cast operands to uint32_t before shifting. |
| Integer division before float assignment (`float f = 5 / 2;`) | Integer division `5 / 2` evaluates to `2` before converting to float `2.0f`. | Make an operand float: `float f = 5.0f / 2;`. |

---

## 8. Performance & Security Notes

- **Remote Integer Truncation Exploits**: In packet processing, reading a 16-bit length into an `int`, adding a header length, and assigning to a 16-bit variable truncates on overflow. If this smaller length allocates a buffer but the original payload length is copied, an exploitable heap overflow occurs.
- **Strict Aliasing Rule**: The compiler assumes that pointers of different types never alias the same memory (except `char *`). Using `memcpy()` to convert bit representations is compiled away into a single register move with zero runtime overhead on modern optimizing compilers!

---

### C23 Checked Conversion Extensions

In ISO C23, checked integer operations (`<stdckdint.h>`) provide explicit macros that guarantee safe conversion without manual range verification:
- `ckd_add(&result, a, b)`
- `ckd_sub(&result, a, b)`
- `ckd_mul(&result, a, b)`

These macros return a boolean flag indicating whether the arithmetic operation exceeded the representable range of the destination type, allowing developers to handle truncation or overflow safely and deterministically without relying on undefined compiler behavior.

---

## 9. Comparison / When to Use What

| Conversion Type | Syntax | Safety | Compiler Inspection |
|---|---|---|---|
| **Integer Promotion** | Implicit (e.g. `char + char`) | High (Promotes to `int`) | Automatic |
| **Usual Arithmetic** | Implicit (e.g. `int + double`) | Medium (Risk of signed wrap) | Flagged by `-Wsign-conversion` |
| **Explicit Numerical Cast** | `(int)my_double` | Intentional | Suppresses truncation warnings |
| **Pointer Bit Reinterpretation**| `memcpy(&dst, &src, sz)` | 100% Safe (Strict aliasing compliant) | Optimized to zero instructions |
| **Raw Pointer Cast** | `*(float *)&my_int` | **Dangerous (Undefined Behavior)** | Breaks optimizer under `-O2` |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is Integer Promotion in C? Give an example where it affects the result.</strong></summary>

Integer promotion is the automatic conversion of integer types with a rank smaller than `int` (such as `char` and `short`) to `int` (or `unsigned int` if `int` cannot represent the range) before evaluating an expression. Example:
```c
unsigned char a = 0xFF;
char b = 0xFF;
/* a == b compares (int)255 == (int)(-1) on systems where char is signed, evaluating to FALSE! */
```
</details>

<details>
<summary><strong>2. What is the result of the expression `(-1 < 1U)` in C, and why?</strong></summary>

It evaluates to `false` (or `0`). Under the Usual Arithmetic Conversions, when a signed `int` is compared to an `unsigned int`, the signed `int` is converted to `unsigned int`. `-1` converted to an unsigned 32-bit integer wraps around to `4,294,967,295U`, which is greater than `1U`.
</details>

<details>
<summary><strong>3. What type does `(unsigned short)a + (unsigned short)b` produce on a 32-bit system?</strong></summary>

It produces a **signed `int`**. Because `int` (32 bits, up to $+2,147,483,647$) can represent all possible values of an `unsigned short` ($0$ to $65,535$), integer promotion converts both operands to signed `int`.
</details>

<details>
<summary><strong>4. Why is casting the result of `malloc()` considered bad practice in standard C?</strong></summary>

In C, `void *` automatically and implicitly converts to and from any object pointer type. Explicitly casting `(int *)malloc(...)` is unnecessary and, in legacy pre-C99 code, could mask the omission of `#include <stdlib.h>`, leading the compiler to assume `malloc` returned a 32-bit `int` and truncating 64-bit pointers.
</details>

<details>
<summary><strong>5. What is the strict aliasing rule, and how do you safely inspect the raw bytes of an object?</strong></summary>

The strict aliasing rule asserts that two pointers of different types cannot point to the same memory location, allowing the compiler to optimize register caching. Accessing an object through a pointer of a different type invokes undefined behavior. The sole standard exception is `char *` (or `unsigned char *`), which is explicitly permitted to alias any object to inspect its raw bytes.
</details>

<details>
<summary><strong>6. What happens when a floating-point value is converted to an integer?</strong></summary>

The fractional part is truncated toward zero (e.g. `3.9` becomes `3`, `-3.9` becomes `-3`). If the truncated value cannot fit within the destination integer type (e.g. `(int)1e12`), the behavior is undefined.
</details>

<details>
<summary><strong>7. How does `memcpy` avoid violating the strict aliasing rule?</strong></summary>

`memcpy` operates on `void *` parameters that internally manipulate memory as sequences of `unsigned char`. Because the C standard explicitly exempts character types from the strict aliasing rule, copying bytes via `memcpy` is well-defined and recognized as an intrinsic idiom by compilers.
</details>

<details>
<summary><strong>8. What is the difference between `(uint32_t)(int8_t)-1` and `(uint32_t)(uint8_t)-1`?</strong></summary>

- `(uint32_t)(int8_t)-1`: First, `int8_t` (-1) undergoes sign extension to 32 bits (`0xFFFFFFFF`), which evaluates to `4,294,967,295U`.
- `(uint32_t)(uint8_t)-1`: `-1` converted to `uint8_t` is `255` (`0xFF`). Zero-extending `255` to 32 bits evaluates to `255U` (`0x000000FF`).
</details>

---

<details>
<summary><strong>9. What is a "trap representation" in standard C?</strong></summary>

A trap representation is a specific bit pattern in memory that does not represent a valid value of a type. Reading or loading a trap representation into a CPU register invokes undefined behavior and can trigger a hardware exception (e.g. signaling NaNs in floating-point, or unnormalized values in historical architectures). ISO C guarantees that `unsigned char` and `char` have no trap representations; every bit pattern represents a valid value.
</details>

<details>
<summary><strong>10. Why is `(unsigned char)c + (unsigned char)d` not evaluated as 8-bit arithmetic?</strong></summary>

Because of the C Integer Promotion rule. All operands with conversion rank below `int` are promoted to `int` before any arithmetic operator (+, -, *, /) executes. Therefore, the addition is performed using 32-bit CPU arithmetic, preventing 8-bit overflow until the final result is explicitly stored back into an 8-bit variable.
</details>

<details>
<summary><strong>11. What is the difference between explicit conversion via pointer cast vs `memcpy`?</strong></summary>

A pointer cast `*(float *)&my_int` violates the strict aliasing rule, allowing the compiler optimizer under `-O2` to assume that memory is never accessed as both types, resulting in reordered or discarded instructions. `memcpy(&my_float, &my_int, sizeof(my_float))` uses `void *` (character-type equivalent), which conforms to strict aliasing and is optimized by modern compilers into a single register transfer without any function call overhead.
</details>

<details>
<summary><strong>12. What happens during conversion from `double` to `float` if the value exceeds `FLT_MAX`?</strong></summary>

Under ISO C, converting a `double` to a `float` whose magnitude exceeds the representable range of `float` results in either positive or negative infinity (if supported by IEEE 754), or invokes undefined behavior on non-IEEE architectures.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Fixing Signed/Unsigned Comparisons</strong></summary>

**Goal**: Fix the sign comparison warning emitted by `-Wsign-compare` in this function:
```c
int find_index(const int *arr, int len, int target) {
    for (size_t i = 0; i < len; ++i) {
        if (arr[i] == target) return (int)i;
    }
    return -1;
}
```

**Solution**:
Validate that `len >= 0` before comparing against `size_t`:
```c
int find_index(const int *arr, int len, int target) {
    if (len < 0) return -1;
    for (size_t i = 0; i < (size_t)len; ++i) {
        if (arr[i] == target) return (int)i;
    }
    return -1;
}
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Safe Float-to-Int Conversion</strong></summary>

**Goal**: Write a function `safe_float_to_int(float val, int *out)` that checks for overflow, NaN, and infinity, returning `true` on success and `false` if the conversion would invoke undefined behavior.

**Solution**:
```c
#include <stdbool.h>
#include <limits.h>
#include <math.h>

bool safe_float_to_int(float val, int *out) {
    if (isnan(val) || isinf(val)) {
        return false;
    }
    /* In C, float to int truncates toward zero */
    if (val < (float)INT_MIN || val > (float)INT_MAX) {
        return false;
    }
    *out = (int)val;
    return true;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Strict-Aliasing-Compliant Float Bit Inspector</strong></summary>

**Goal**: Write a function `float_to_bits(float f)` that extracts the IEEE 754 raw 32-bit integer representation of a float without violating strict aliasing rules or invoking undefined behavior.

**Solution**:
```c
#include <stdint.h>
#include <string.h>

uint32_t float_to_bits(float f) {
    uint32_t bits = 0;
    /* memcpy is strictly aliasing-compliant and optimized away by compilers */
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Integer Promotion**: Types narrower than `int` promote to `int` in expressions.
- **Signed/Unsigned Comparison**: Signed converts to unsigned (`-1 < 1U` is FALSE).
- **Float-to-Int**: Truncates toward zero; values outside `[INT_MIN, INT_MAX]` invoke UB.
- **Strict Aliasing**: Never use `*(float *)&my_int`; use `memcpy()` for bit reinterpretation.
- Always use `uintptr_t` to cast pointer addresses to integers.
- Compile with `-Wconversion -Wsign-conversion` to catch all implicit conversion flaws.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 6.3: "Conversions".
- SEI CERT C Coding Standard: *INT02-C. Understand integer conversion rules*.
- Michael Barr: *Integer Promotions in C* (Embedded Systems Programming).

---

[← Previous: 05-operators.md](./05-operators.md) | [Section Index](./README.md) | [Next: 07-control-flow.md](./07-control-flow.md) →
