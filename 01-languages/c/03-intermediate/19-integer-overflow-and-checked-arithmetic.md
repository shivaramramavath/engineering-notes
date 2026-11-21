# 19 Integer Overflow & Checked Arithmetic

> Master the critical distinctions between signed and unsigned overflow, write defensive arithmetic boundary guards, leverage compiler intrinsics, and use modern C23 `<stdckdint.h>`.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [01-fundamentals/03-integer-representation.md](../01-fundamentals/03-integer-representation.md), [01-fundamentals/06-type-conversions.md](../01-fundamentals/06-type-conversions.md), [03-intermediate/10-undefined-behavior-and-sanitizers.md](10-undefined-behavior-and-sanitizers.md) |
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

Integer overflow occurs when an arithmetic operation attempts to create a numeric value that falls outside the range of representable values for that integer type.

In C, integer overflow is responsible for some of the most disastrous security vulnerabilities in history:
- **Memory Allocation Wraparound**: Calculating buffer sizes with `num_items * item_size`. If multiplication wraps around to a tiny number (e.g. 16 bytes), `malloc(16)` succeeds, but the subsequent loop attempts to write 1,000,000 items, causing a catastrophic heap buffer overflow.
- **Privilege Escalation & Financial Logic**: Balance deductions or privilege checks where negative wrapping bypasses verification checks.
- **Compiler Dead Code Elimination**: Checking for overflow *after* the operation has executed (e.g. `if (a + b < a)`) triggers Undefined Behavior, allowing optimizing compilers to completely delete the security check.

Understanding ISO C rules regarding signed versus unsigned arithmetic and implementing pre-condition boundary checks or C23 hardware-checked arithmetic is essential for writing secure code.

---

## 2. Core Concepts

### 2.1 Signed vs Unsigned Arithmetic Rules
1. **Unsigned Integer Overflow**:
   - In ISO C, unsigned integer arithmetic **never overflows**.
   - It is strictly defined to wrap around modulo $2^N$ (where $N$ is the bit width of the unsigned type). For example, with 32-bit `uint32_t`, `UINT32_MAX + 1 == 0`. This is fully defined behavior.
2. **Signed Integer Overflow**:
   - In ISO C (C11, C17, C23), signed integer overflow is strictly **Undefined Behavior (UB)**.
   - The standard imposes zero constraints: the compiler can assume signed overflow never happens and optimize accordingly.

### 2.2 Defensive Pre-Condition Checks
Because signed overflow is UB, you cannot execute the operation and then check the result. You must check the inputs **before** executing the operation:
- **Addition ($a + b$)**:
  - If $b > 0$ and $a >$ `INT_MAX` $- b \implies$ Overflow.
  - If $b < 0$ and $a <$ `INT_MIN` $- b \implies$ Underflow.
- **Subtraction ($a - b$)**:
  - If $b > 0$ and $a <$ `INT_MIN` $+ b \implies$ Underflow.
  - If $b < 0$ and $a >$ `INT_MAX` $+ b \implies$ Overflow.
- **Multiplication ($a 	imes b$)**:
  - If $a > 0, b > 0$ and $a >$ `INT_MAX` $/ b \implies$ Overflow.
  - If $a > 0, b < 0$ and $b <$ `INT_MIN` $/ a \implies$ Underflow.
  - Special edge case: `INT_MIN * -1` overflows because $|$`INT_MIN`$| >$ `INT_MAX` in two's complement!

### 2.3 The Asymmetry of Two's Complement Integer Representation
Because modern CPUs employ two's complement binary representation, the negative range of any signed integer type always contains one more value than the positive range:
- For 8-bit signed `int8_t`: $-128$ to $+127$.
- For 32-bit signed `int32_t`: $-2,147,483,648$ to $+2,147,483,647$.

This structural asymmetry introduces two subtle arithmetic hazards that often escape novice code reviews:
1. **Negation Overflow**: Evaluating `-INT_MIN` is an immediate signed overflow (Undefined Behavior), because positive $2,147,483,648$ cannot fit in `int32_t`. On standard x86 processors without trapping, the CPU negates the bit pattern `0x80000000` via two's complement inversion (`~x + 1`), which evaluates right back to `0x80000000` (`INT_MIN`)!
2. **Division Trap**: Evaluating `INT_MIN / -1` triggers a hardware integer division exception (`#DE` on x86), causing the operating system kernel to terminate the program with signal `SIGFPE` (Floating Point Exception).

### 2.4 C23 `<stdckdint.h>`
ISO C23 standardized compiler intrinsics into `<stdckdint.h>`:
- `bool ckd_add(T *result, A a, B b);`
- `bool ckd_sub(T *result, A a, B b);`
- `bool ckd_mul(T *result, A a, B b);`
These macros return `true` if overflow occurred, and store the wrapped result into `*result`.

---

## 3. How It Works Internally

### CPU Hardware Flags vs Compiler Assumptions
At the CPU assembly level, x86 and ARM processors maintain status flags in the flags register:
- **Carry Flag (CF)**: Set when an *unsigned* operation overflows.
- **Overflow Flag (OF)**: Set when a *signed* two's complement operation overflows.

```
Hardware Assembly Level vs C Compiler:
x86 Instruction: add eax, edx
┌──────────────────────────────────────────────┐
│ Hardware Flags Updated:                      │
│   CF = 1 (Unsigned carry)                    │
│   OF = 1 (Signed overflow)                   │
├──────────────────────────────────────────────┤
│ Assembly Branch Instruction:                 │
│   jo .L_overflow   ; Jumps if OF == 1        │
├──────────────────────────────────────────────┤
│ Why C23 / Intrinsics are Faster:             │
│   Manual C checks require 4 branches + div!  │
│   ckd_add / __builtin_add_overflow emit a    │
│   single 'add' followed by 'jo' (0 overhead)!│
└──────────────────────────────────────────────┘
```

Manual checks written in pure C require division operations (`a > INT_MAX / b`), which cost 10–20 clock cycles. C23 `<stdckdint.h>` and compiler builtins (`__builtin_add_overflow`) inspect the CPU's Overflow Flag directly, executing in a single clock cycle!

---

## 4. Syntax & Usage

### 4.1 GCC / Clang Builtins
```c
int a = 2000000000;
int b = 1000000000;
int res;

if (__builtin_add_overflow(a, b, &res)) {
    /* Handled safely: overflow occurred */
}
```

### 4.2 C23 Standard Implementation
```c
#include <stdckdint.h>
#include <stdio.h>

void safe_math(int a, int b) {
    int result;
    if (ckd_mul(&result, a, b)) {
        printf("Multiplication overflowed!\n");
    } else {
        printf("Result: %d\n", result);
    }
}
```

---

## 5. Practical Examples

### Comprehensive Example: Pre-Condition Checked Arithmetic
The following program implements robust signed arithmetic checks for addition and multiplication, preventing undefined behavior without relying on vendor-specific compiler extensions.

```c
#include <stdio.h>
#include <limits.h>
#include <stdbool.h>

static bool checked_add_int(int a, int b, int *res) {
    if ((b > 0 && a > INT_MAX - b) ||
        (b < 0 && a < INT_MIN - b)) {
        return false;
    }
    *res = a + b;
    return true;
}

static bool checked_mul_int(int a, int b, int *res) {
    if (a > 0) {
        if (b > 0 && a > INT_MAX / b) return false;
        if (b < 0 && b < INT_MIN / a) return false;
    } else if (a < 0) {
        if (b > 0 && a < INT_MIN / b) return false;
        if (b < 0 && b < INT_MAX / a) return false;
    }
    *res = a * b;
    return true;
}

int main(void) {
    printf("=== Integer Overflow & Checked Arithmetic ===\n");

    int out = 0;

    /* Addition check */
    int x1 = INT_MAX - 5, y1 = 10;
    printf("Checking %d + %d: ", x1, y1);
    if (checked_add_int(x1, y1, &out)) {
        printf("Result = %d\n", out);
    } else {
        printf("OVERFLOW PREVENTED (exceeds INT_MAX %d)\n", INT_MAX);
    }

    /* Multiplication check */
    int x2 = 1000000, y2 = 3000;
    printf("Checking %d * %d: ", x2, y2);
    if (checked_mul_int(x2, y2, &out)) {
        printf("Result = %d\n", out);
    } else {
        printf("OVERFLOW PREVENTED (exceeds INT_MAX %d)\n", INT_MAX);
    }

    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 19-integer-overflow-and-checked-arithmetic.c -o 19-integer-overflow-and-checked-arithmetic.exe
./19-integer-overflow-and-checked-arithmetic.exe
```

```text
=== Integer Overflow & Checked Arithmetic ===
Checking 2147483642 + 10: OVERFLOW PREVENTED (exceeds INT_MAX 2147483647)
Checking 1000000 * 3000: OVERFLOW PREVENTED (exceeds INT_MAX 2147483647)
```

---

## 6. Best Practices

1. **Check Allocation Calculations for Overflow**: Always validate `nmemb * size <= SIZE_MAX` before passing size arguments to `malloc()`. Alternatively, use `calloc(nmemb, size)` which performs internal overflow checking.
2. **Never Check for Signed Overflow Post-Facto**: Avoid `if (a + b < a)`. It is undefined behavior and modern compilers eliminate it.
3. **Use Unsigned Types for Modulo Bit Arithmetic**: When modular wrapping is intentional (e.g. cryptography, hashing, RNGs), always use unsigned types (`uint32_t`, `uint64_t`).
4. **Beware of Negative Negation (`-INT_MIN`)**: In two's complement, `-INT_MIN` cannot be represented because the negative range is 1 greater than the positive range. Negating `INT_MIN` is Undefined Behavior!
5. **Enable Compiler Sanitizers**: Always compile and test with `-fsanitize=undefined` (UBSan), which catches signed overflow immediately at runtime.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| `if (x + 1 < x)` | Signed overflow is UB; the optimizer assumes $x+1 > x$ and deletes the check. | Use pre-condition check: `if (x == INT_MAX)`. |
| `malloc(n * sizeof(int))` without check | If `n * sizeof(int)` overflows, allocates smaller buffer, leading to heap buffer overflow. | Use `calloc(n, sizeof(int))` or check `n > SIZE_MAX / sizeof(int)`. |
| Negating `INT_MIN`: `-n` | In 32-bit two's complement, `INT_MIN` is `-2147483648`, but `INT_MAX` is `2147483647`. `-INT_MIN` is UB. | Handle `INT_MIN` as a special case before negating. |
| Division: `INT_MIN / -1` | Result exceeds `INT_MAX`. On x86, triggers a CPU divide error exception (SIGFPE crash!). | Verify `!(a == INT_MIN && b == -1)` before division. |
| Mixed signed/unsigned comparison | `int a = -1; unsigned int b = 1; if (a < b)` evaluates to `false` because `a` is promoted to `4294967295u`. | Avoid mixing signed and unsigned variables in comparisons. |
| Implicit integer promotion in short math | `short a = 30000, b = 30000; short c = a + b;` promotes to `int` (no UB), but assigning to `short` is implementation-defined. | Perform calculations with matching explicit types. |

---

## 8. Performance & Security Notes

- **CVE Impact**: Over 30% of critical CVEs in C/C++ libraries (OpenSSL, LibPNG, Linux Kernel) stem from integer overflow leading to undersized memory allocations and subsequent heap buffer overflows.
- **Hardware Traps (`-ftrapv`)**: GCC provides `-ftrapv` which generates hardware traps on signed overflow. However, it incurs significant performance penalties; using builtins or `<stdckdint.h>` is vastly superior.
- **Branchless Saturation Arithmetic**: In digital signal processing (DSP) and graphics, values should clamp to maximum/minimum rather than wrapping or erroring. Saturation math can be written branchlessly using bitwise masks.

---

## 9. Comparison / When to Use What

| Technique | Portability | Performance | Detects Signed Overflow? | Detects Unsigned Overflow? |
| :--- | :--- | :--- | :--- | :--- |
| **Pre-Condition Checks (`INT_MAX - b`)** | Universal ISO C | Moderate (Division costly) | Yes | Yes |
| **C23 `<stdckdint.h>`** | Standard C23 | Optimal (1 assembly flag) | Yes | Yes |
| **GCC/Clang Builtins** | GCC / Clang only | Optimal (1 assembly flag) | Yes | Yes |
| **`-fsanitize=undefined`** | Debug / Test only | Low (~5% overhead) | Yes (Aborts on error) | No (Unsigned not UB) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does unsigned integer overflow produce defined behavior, while signed overflow is undefined?</strong></summary>

Historically, C was designed to run across diverse hardware architectures, including sign-magnitude, one's complement, and two's complement systems. Unsigned types model abstract binary registers and modular arithmetic, making wraparound modulo $2^N$ consistent across all machines. For signed types, wrapping on overflow differed between architectures (or caused hardware traps). Granting compilers the assumption that signed overflow is undefined enabled aggressive loop vectorization and algebraic optimizations (such as simplifying `x * 2 / 2` to `x`).
</details>

<details>
<summary><strong>2. Why is dividing `INT_MIN` by `-1` dangerous on modern processors?</strong></summary>

In two's complement arithmetic, a 32-bit signed integer spans $-2,147,483,648$ to $+2,147,483,647$. Dividing $-2,147,483,648$ by $-1$ mathematically yields $+2,147,483,648$, which cannot fit in a 32-bit signed integer. On x86 processors, the `IDIV` instruction triggers a hardware `DIVIDE BY ZERO` exception (interrupt vector 0), causing the OS to terminate the program immediately with `SIGFPE`.
</details>

<details>
<summary><strong>3. Explain the integer promotion vulnerability in `if (len + 10 < MAX_LEN)`.</strong></summary>

If `len` is an unsigned integer close to `UINT_MAX`, evaluating `len + 10` wraps around to a small positive value (e.g. `UINT_MAX + 10 == 9`). The comparison `9 < MAX_LEN` evaluates to true, bypassing the bounds check. The application then proceeds assuming `len` is within safe limits, causing catastrophic memory corruption when accessing buffers based on `len`.
</details>

<details>
<summary><strong>4. How does `calloc()` protect against integer overflow compared to `malloc()`?</strong></summary>

`malloc(n * size)` expects the caller to pass the multiplied byte total. If the multiplication overflows, `malloc` receives the truncated wrapped value without knowing an overflow occurred. In contrast, `calloc(n, size)` takes the element count and element size as separate arguments. The standard library implementation explicitly checks for multiplication overflow internally; if `n * size > SIZE_MAX`, `calloc()` aborts the allocation and safely returns `NULL`.
</details>

<details>
<summary><strong>5. How does C23 `<stdckdint.h>` achieve zero-overhead checked arithmetic?</strong></summary>

Modern CPU architectures execute addition and multiplication while automatically setting hardware condition flags in the status register: the Carry Flag (`CF`) for unsigned overflow, and the Overflow Flag (`OF`) for signed overflow. Instead of generating expensive multi-instruction division and comparison sequences, functions like `ckd_add()` and `ckd_mul()` instruct the compiler backend to emit a simple conditional jump instruction (`jo` on x86, `b.vs` on ARM) inspecting the hardware flag directly.
</details>

<details>
<summary><strong>6. What is the subtle bug in `int abs_val(int x) { return x < 0 ? -x : x; }`?</strong></summary>

If `x` is `INT_MIN` (`-2147483648`), evaluating `-x` is Undefined Behavior because $+2,147,483,648$ exceeds `INT_MAX`. On two's complement architectures without trapping, `-INT_MIN` wraps back to `INT_MIN`! Thus, `abs_val(INT_MIN)` returns a negative number, breaking functions that expect a non-negative absolute value.
</details>

<details>
<summary><strong>7. How can you safely check if unsigned subtraction will underflow?</strong></summary>

For unsigned integers `a` and `b`, computing `a - b` wraps around if $b > a$. The check is simply:
```c
if (b > a) {
    /* Underflow would occur */
}
```
No arithmetic operation is needed before the comparison.
</details>

<details>
<summary><strong>8. Why does comparing `int a = -1; unsigned int b = 1; if (a < b)` evaluate to false?</strong></summary>

Under C's Usual Arithmetic Conversions, when an operation involves both a signed integer and an unsigned integer of the same rank (e.g. `int` and `unsigned int`), the signed operand is converted to `unsigned int`. The bit pattern of `-1` in two's complement is converted to `UINT_MAX` (`4,294,967,295`). The condition evaluates to `4294967295u < 1u`, which is false.
</details>

---

## 11. Practice Exercises

### Easy: Safe Unsigned Addition Guard
Write an inline function `bool safe_add_u32(uint32_t a, uint32_t b, uint32_t *res)` that returns `false` if unsigned addition would wrap around.

<details>
<summary>Solution</summary>

```c
#include <stdint.h>
#include <stdbool.h>

static inline bool safe_add_u32(uint32_t a, uint32_t b, uint32_t *res) {
    if (UINT32_MAX - a < b) {
        return false;
    }
    *res = a + b;
    return true;
}
```
</details>

### Medium: Safe Buffer Allocation Helper
Write a function `void *safe_malloc_array(size_t nmemb, size_t size)` that safely allocates an array or returns `NULL` if `nmemb * size` overflows `SIZE_MAX`.

<details>
<summary>Solution</summary>

```c
#include <stdlib.h>
#include <stdint.h>

void *safe_malloc_array(size_t nmemb, size_t size) {
    if (nmemb != 0 && size > SIZE_MAX / nmemb) {
        return NULL; /* Overflow prevented */
    }
    return malloc(nmemb * size);
}
```
</details>

### Hard: Saturating Signed Addition
Implement 32-bit saturating addition `int32_t sat_add_i32(int32_t a, int32_t b)` that clamps the result to `INT32_MAX` on overflow, and `INT32_MIN` on underflow.

<details>
<summary>Solution</summary>

```c
#include <stdint.h>
#include <limits.h>

int32_t sat_add_i32(int32_t a, int32_t b) {
    if (b > 0 && a > INT32_MAX - b) {
        return INT32_MAX;
    }
    if (b < 0 && a < INT32_MIN - b) {
        return INT32_MIN;
    }
    return a + b;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Operation | Unsafe Construct | Safe Pre-Condition Check |
| :--- | :--- | :--- |
| Signed Addition | `if (a + b < a)` (UB!) | `if (b > 0 && a > INT_MAX - b)` |
| Signed Subtraction | `a - b` | `if (b < 0 && a > INT_MAX + b)` |
| Signed Multiplication | `a * b` | `if (a > 0 && b > 0 && a > INT_MAX / b)` |
| Allocation Size | `malloc(n * sz)` | `if (n != 0 && sz > SIZE_MAX / n)` |
| C23 Hardware Check | N/A | `if (ckd_add(&res, a, b)) /* overflow */` |

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17) §6.2.5 (Types: signed and unsigned integer representation).
- ISO/IEC 9899:2024 (C23) §7.20 (`<stdckdint.h>` Checked integer arithmetic).
- SEI CERT C Coding Standard: Rule 04. Integers (INT30-C, INT31-C, INT32-C).
- Phrack Magazine #60: *Basic Integer Overflows* by blexim (2002).

---

[← Previous](18-strict-aliasing-and-type-punning.md) | [Section Index](README.md) | [Next →](20-floating-point-pitfalls.md)
