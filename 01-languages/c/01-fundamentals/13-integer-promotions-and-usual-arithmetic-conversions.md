# Integer Promotions and Usual Arithmetic Conversions

> The definitive specification of the C type conversion state machine: rank ordering, integer promotion mechanics, the Usual Arithmetic Conversions decision tree, signed vs unsigned conversion pitfalls, and bitwise arithmetic traps.

| Metadata | Details |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [02-data-types.md](./02-data-types.md), [03-integer-representation.md](./03-integer-representation.md), [06-type-conversions.md](./06-type-conversions.md) |
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

In C, the operands of binary expressions rarely execute directly in their declared types. Instead, the language applies a deterministic, mathematical set of type-balancing rules known as **Integer Promotions** and the **Usual Arithmetic Conversions**.

Understanding this state machine is mandatory for systems developers:
1. **The Narrow Type Arithmetic Trap**: Many engineers believe that adding two `uint8_t` values performs 8-bit addition with modular wrap-around at 255. In reality, both operands promote to **signed `int`**! The addition runs in 32-bit signed space, meaning overflow does not wrap at 256 until explicitly cast back.
2. **The Signed-Unsigned Comparison Disaster**: Comparing a signed `int32_t` with an unsigned `uint32_t` converts the signed integer to `uint32_t`. `-1 > 10U` evaluates to `true`, causing critical authentication and bounds-checking bypass vulnerabilities.
3. **Bitwise Shift UB on Promoted Narrow Integers**: In expressions like `(uint8_t)1 << 31`, `uint8_t` promotes to signed `int`. Shifting 1 into the sign bit of a signed 32-bit `int` is **undefined behavior**!

---

## 2. Core Concepts

### The Integer Conversion Rank

Every integer type has an integer conversion rank defined by the ISO C standard:

$$\text{\_Bool} < \text{char} == \text{signed char} == \text{unsigned char} < \text{short} == \text{unsigned short} < \text{int} == \text{unsigned int} < \text{long} == \text{unsigned long} < \text{long long} == \text{unsigned long long}$$

- Signed and unsigned versions of the same base type share the exact same conversion rank.
- Standard integer types have higher rank than extended integer types of the same width.

### Integer Promotion Mechanics

Before any operator evaluates an operand of integer type:
- If the type's conversion rank is **less than or equal to `int`**:
  - If `int` can represent all values of the source type: the value is converted to **signed `int`**.
  - Otherwise: the value is converted to **`unsigned int`**.

```
Example:
On a platform where short is 16-bit and int is 32-bit:
- signed char   (-128 to 127)        --> Promotes to: signed int
- unsigned char (0 to 255)           --> Promotes to: signed int
- signed short  (-32768 to 32767)    --> Promotes to: signed int
- unsigned short (0 to 65535)        --> Promotes to: signed int (Fits completely in 32-bit int!)
```

---

## 3. How It Works Internally

### The Usual Arithmetic Conversions State Machine

When a binary operator (such as `+`, `-`, `*`, `/`, `%`, `==`, `<`, `&`, `|`) operates on two operands:

```
[ Binary Operator: Operand A, Operand B ]
                     |
         Are either operands Float?
        /                          \
     [YES]                         [NO]
       |                             |
 Convert to highest rank:     Apply INTEGER PROMOTIONS
 long double > double > float to both operands
                                     |
                         Do types now match?
                        /                   \
                     [YES]                  [NO]
                       |                      |
                    (Done)         Are both Signed or both Unsigned?
                                  /                                 \
                               [YES]                                [NO]
                                 |                                    |
                       Convert lesser rank            Does Unsigned rank >= Signed rank?
                       to greater rank               /                                  \
                                                  [YES]                                 [NO]
                                                    |                                     |
                                          Convert Signed operand                 Can Signed type represent
                                          to Unsigned type                       all values of Unsigned type?
                                                                                /                            \
                                                                             [YES]                           [NO]
                                                                               |                               |
                                                                    Convert Unsigned to             Convert BOTH to Unsigned
                                                                    Signed type                     version of Signed type
```

---

### Step-by-Step Decision Algorithm Example

Consider evaluating: `unsigned int u = 10; int s = -20; if (u + s > 0) ...`

1. Operands are `u` (`unsigned int`) and `s` (`signed int`).
2. Neither operand is floating-point.
3. Both have rank equal to `int` (no integer promotion needed).
4. One operand is unsigned; one is signed. Both have the **same rank** (`int`).
5. Step 5 applies: The signed operand `s` is converted to `unsigned int`.
6. `-20` converted to 32-bit unsigned becomes $2^{32} - 20 = 4,294,967,276\text{U}$.
7. The addition computes: $10\text{U} + 4,294,967,276\text{U} = 4,294,967,286\text{U}$.
8. $4,294,967,286\text{U} > 0$ evaluates to **`true`**!

---

## 4. Syntax & Usage

### 1. The Signed/Unsigned Comparison Trap
```c
#include <stdio.h>
#include <stdint.h>

void evaluate_comparison(void) {
    int32_t a = -5;
    uint32_t b = 10;

    /* DANGEROUS: 'a' converts to uint32_t (4294967291U), so (a > b) is TRUE! */
    if (a > b) {
        printf("Unexpected: -5 is greater than 10 via unsigned conversion!\n");
    }

    /* SAFE & CONFORMING: */
    if (a > 0 && (uint32_t)a > b) {
        printf("Safe check evaluates correctly.\n");
    }
}
```

### 2. Preventing Bitwise Shift Promotion UB
```c
#include <stdint.h>

/* WRONG: uint8_t promotes to signed int; 1 << 31 invokes signed overflow UB! */
uint32_t bad_mask = ((uint8_t)1) << 31;

/* CORRECT: Cast to unsigned int or uint32_t before shifting */
uint32_t safe_mask = ((uint32_t)1) << 31;
```

---

## 5. Practical Examples

### Integer Promotions & Arithmetic Conversions Demonstration Program

```c
/* File: 01-languages/c/01-fundamentals/examples/13-integer-promotions-and-usual-arithmetic-conversions.c */
#include <stdio.h>
#include <stdint.h>

int main(void) {
    printf("=== Integer Promotions & Conversions Tree ===\n");
    
    uint8_t u8_a = 250;
    uint8_t u8_b = 10;
    /* uint8_t promotes to signed int; (250 - 10) produces signed int 240 */
    printf("uint8_t expression (u8_a - u8_b) = %d\n", u8_a - u8_b);

    /* Usual arithmetic conversions: unsigned vs signed of same rank */
    int32_t s32 = -5;
    uint32_t u32 = 10;
    if ((uint32_t)s32 > u32) {
        printf("Usual arithmetic conversion: -5 becomes %u (> 10)\n", (uint32_t)s32);
    }
    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 13-integer-promotions-and-usual-arithmetic-conversions.c -o run_13.exe
./run_13.exe
```
Output:
```text
=== Integer Promotions & Conversions Tree ===
uint8_t expression (u8_a - u8_b) = 240
Usual arithmetic conversion: -5 becomes 4294967291 (> 10)
```

---

## 6. Best Practices

1. **Always Enable `-Wconversion` and `-Wsign-conversion`**: The compiler will emit a diagnostic warning for every silent signed-to-unsigned conversion and value-altering narrowing truncation.
2. **Never Compare Signed and Unsigned Variables Directly**: If comparing an index (`size_t`) to a signed coordinate (`int`), ensure the signed variable is $\ge 0$ before casting.
3. **Perform Bitwise Operations Exclusively on Explicitly Unsigned Types**: Never shift signed integers. Always use `uint32_t`, `uint64_t`, or `1U`.
4. **Use Parentheses and Explicit Casts When Mixing Types**: Make type intentions explicit to avoid relying on subtle conversion rank rules.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| `-1 < sizeof(int)` evaluates to `false` | `sizeof` returns unsigned `size_t`. `-1` converts to `SIZE_MAX`, which is greater than `sizeof(int)`. | Compare signed values: `long val = -1; if (val < (long)sizeof(int))`. |
| Shifting `(uint8_t)1 << 31` | `uint8_t` promotes to signed `int`. Shifting into the sign bit invokes signed overflow UB. | Write `1U << 31` or `(uint32_t)1 << 31`. |
| Assuming `unsigned short` promotes to `unsigned int` | If `sizeof(int) > sizeof(short)`, `unsigned short` promotes to **signed `int`**. | Be aware that arithmetic on short types produces signed `int`. |
| `int32_t a = -1; uint16_t b = 1; (a < b)` result is true on 32-bit but false if ranks match | Behavior changes based on architecture data model! | Explicitly align signedness before comparison. |
| Negating unsigned integer (`-u_val`) | Unsigned values cannot be negative; `-u_val` computes $2^N - \text{u\_val}$ via modular wrap-around. | Use signed integer if negative values are required. |
| Inverting unsigned char via `~` resulting in negative int | `~((unsigned char)0)` promotes to `int` before inverting, producing `-1` in signed `int`! | Explicitly mask: `(uint8_t)(~((uint8_t)0))`. |

---

## 8. Performance & Security Notes

- **The Buffer Length Bypass Attack**: In network daemons, checking `if (user_len < MAX_BUFFER)` where `user_len` is signed and `MAX_BUFFER` is unsigned (`size_t`) allows negative lengths (e.g. `-100`) to pass the check (since `-100` converts to a huge unsigned integer). If `user_len` is then passed to an allocation function that casts back to signed, a severe buffer overflow ensues.
- **ALU Register Utilization**: Hardware execution units process 32-bit and 64-bit registers at the same speed. Integer promotion eliminates artificial truncation operations, allowing CPUs to execute chained arithmetic at peak native register performance.

---

## 9. Comparison / When to Use What

| Scenario | Rule Applied | Resulting Type | Potential Hazard |
|---|---|---|---|
| `uint8_t + uint8_t` | Integer Promotion | **`signed int`** | Arithmetic overflow on intermediate values |
| `int32_t + uint32_t` | Usual Arithmetic Conversions | **`uint32_t`** | Negative values wrap around to positive |
| `int32_t + double` | Usual Arithmetic Conversions | **`double`** | Float rounding precision loss on large integers |
| `int64_t + uint32_t` | Usual Arithmetic Conversions | **`int64_t`** (LP64/LLP64) | Safe: 64-bit signed can represent all 32-bit unsigned |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does `sizeof('a')` return 4 in standard C?</strong></summary>

In C, character literals (like `'a'`) have type `int` (4 bytes on modern platforms), NOT `char`. They are implicitly promoted to `int` by the compiler lexer. In C++, character literals have type `char` (`sizeof('a') == 1`).
</details>

<details>
<summary><strong>2. What is the value of the expression `(-1L < 1U)` on a 64-bit Linux system (LP64) versus a 32-bit system (ILP32)?</strong></summary>

- On **64-bit Linux (LP64)**: `long` is 64-bit, and `unsigned int` is 32-bit. A 64-bit signed `long` can represent all values of a 32-bit `unsigned int`. Therefore, `1U` converts to signed `long`, and `-1L < 1L` evaluates to **`true`**.
- On **32-bit system (ILP32)**: `long` and `unsigned int` are both 32-bit (same rank). The signed operand converts to unsigned, `-1L` wraps to `4294967295UL`, and the expression evaluates to **`false`**!
</details>

<details>
<summary><strong>3. What type does `unsigned char c = 0xFF; ~c` evaluate to?</strong></summary>

It evaluates to signed `int` with value `-256` (or `0xFFFFFF00`). `c` is promoted to signed `int` (`0x000000FF`) before the bitwise NOT operator `~` executes. Inverting the 32-bit value produces `0xFFFFFF00`, which is negative in two's complement.
</details>

<details>
<summary><strong>4. Under what exact conditions does `unsigned short` promote to `unsigned int` instead of signed `int`?</strong></summary>

Only on architectures where `sizeof(int) == sizeof(short)`. On systems where `int` is wider than `short` (such as all standard 32-bit and 64-bit modern platforms), `int` can represent every value of `unsigned short` ($0 \dots 65535$), so it promotes to **signed `int`**.
</details>

<details>
<summary><strong>5. Why is `(uint32_t)a + (uint32_t)b` guaranteed never to invoke undefined behavior, while `a + b` on signed ints can?</strong></summary>

The ISO C specification defines unsigned integer arithmetic to be modular arithmetic modulo $2^N$. If the mathematical result exceeds the representable range, it wraps around safely and deterministically without undefined behavior. Signed integer overflow is explicitly undefined behavior.
</details>

<details>
<summary><strong>6. In a conditional operator `cond ? (int)1 : (double)2.0`, what is the type of the result when `cond` is true?</strong></summary>

The type is **`double`**. The Usual Arithmetic Conversions are applied to the second and third operands of `?:` at compile time to establish a common type, regardless of runtime branch evaluation.
</details>

<details>
<summary><strong>7. What happens when an integer value is converted to a `_Bool` in C99/C11?</strong></summary>

Unlike normal truncation (which keeps only the lowest bits), converting any non-zero integer or pointer to `_Bool` (or `bool` in C23) produces exactly `1` (`true`). Only `0` converts to `0` (`false`).
</details>

<details>
<summary><strong>8. How does `-Wsign-conversion` protect code compared to `-Wall`?</strong></summary>

`-Wall` does not enable `-Wsign-conversion` because implicit conversions are common in legacy C code. Enabling `-Wsign-conversion` explicitly forces the compiler to flag every implicit conversion between signed and unsigned types that could change the numerical value or sign of an expression.
</details>

---

<details>
<summary><strong>9. How do C23 `_BitInt(N)` bit-precise integers interact with integer promotions?</strong></summary>

Under ISO C23, a `_BitInt(N)` type whose width is less than `int` undergoes standard integer promotion to `int` if `int` can represent all its values. However, if $N > \text{width of } \text{int}$, `_BitInt(N)` does not promote; instead, the Usual Arithmetic Conversions convert the other operand to `_BitInt(N)`.
</details>

<details>
<summary><strong>10. Why does `sizeof(char + char)` evaluate to 4 on 32-bit and 64-bit platforms?</strong></summary>

Because the addition operator `+` triggers integer promotion on both operands before evaluating the expression. Both `char` operands are promoted to signed `int`, so the resulting expression has type `int`, and `sizeof(int)` is 4 bytes.
</details>

<details>
<summary><strong>11. What is the conversion rank of an enumeration type (`enum`) in C?</strong></summary>

Under ISO C, the conversion rank of an enumeration type is identical to the rank of its compatible underlying integer type (typically `int`). In C23, enums can specify explicit underlying types (e.g. `enum Status : uint8_t`), in which case their rank matches that specified type.
</details>

<details>
<summary><strong>12. How does the compiler handle floating-point to integer conversion when the value is negative?</strong></summary>

Under ISO C, conversion of any floating-point number to an integer truncates the fractional part strictly toward zero. For example, `-3.7f` becomes `-3`, and `-0.9f` becomes `0`. If the truncated integral value cannot fit within the destination integer type, the behavior is undefined.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Predicting Promotion Types</strong></summary>

**Goal**: Determine the resulting type of each expression below on a standard 64-bit LP64 system:
1. `(char)'A' + (short)1`
2. `(unsigned int)5 + (int)-10`
3. `(float)1.5f + (long)100`

**Solution**:
1. `int` (both promote to signed `int`).
2. `unsigned int` (signed `int` converts to `unsigned int`).
3. `float` (integer converts to float).
</details>

<details>
<summary><strong>Exercise 2 (Medium): Safe Mixed Sign Comparison Function</strong></summary>

**Goal**: Implement a generic comparison function `safe_compare_signed_unsigned(int32_t s, uint32_t u)` that correctly returns `-1` if $s < u$, `0` if $s == u$, and `1` if $s > u$, without triggering sign conversion traps.

**Solution**:
```c
#include <stdint.h>

int safe_compare_signed_unsigned(int32_t s, uint32_t u) {
    if (s < 0) {
        return -1; /* Negative is always strictly less than any unsigned value */
    }
    /* s is non-negative; safe to cast to uint32_t */
    uint32_t s_u = (uint32_t)s;
    if (s_u < u) return -1;
    if (s_u > u) return 1;
    return 0;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Fixing Bitwise Mask Promotion Flaws</strong></summary>

**Goal**: Explain why the following mask check fails to detect the sign bit on an 8-bit value, and write a correct conforming replacement:
```c
uint8_t byte = 0x80;
if (~byte == 0x7F) {
    /* Expected to match, but NEVER reaches here! */
}
```

**Solution**:
`byte` promotes to signed `int` (`0x00000080`). `~byte` evaluates to `0xFFFFFF7F` (-129). Comparing `0xFFFFFF7F == 0x0000007F` evaluates to `false`!
Conforming fix:
```c
uint8_t byte = 0x80;
if ((uint8_t)(~byte) == 0x7F) {
    /* Successfully matches 0x7F! */
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Integer Promotion**: Types with rank $\le \text{int}$ promote to `signed int` (or `unsigned int`).
- `unsigned short` promotes to **signed `int`** on 32/64-bit systems.
- Comparing signed to unsigned: signed converts to unsigned (`-1 < 1U` is false!).
- Binary operators balance types via Usual Arithmetic Conversions.
- Bitwise shifts on `uint8_t` promote to signed `int`; cast to `uint32_t` before shifting 31 bits.
- Enable `-Wconversion -Wsign-conversion` to catch implicit conversion vulnerabilities.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 6.3.1: "Arithmetic operands".
- SEI CERT C Coding Standard: *INT02-C. Understand integer conversion rules*.
- Michael Barr: *Integer Promotions in C* (Embedded.com).

---

[← Previous: 12-const-correctness-basics.md](./12-const-correctness-basics.md) | [Section Index](./README.md) | [Next: 14-sequence-points-and-evaluation-order.md](./14-sequence-points-and-evaluation-order.md) →
