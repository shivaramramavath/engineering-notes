# C Data Types and Widths

> A rigorous reference on C type architecture: primitive types, standard minimum widths, the integer conversion rank, `<stdint.h>` exact-width integers, `size_t` vs `ptrdiff_t`, floating-point IEEE 754 representations, and C23 native keywords (`nullptr_t`, `bool`).

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [01-program-structure.md](./01-program-structure.md) |
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

In C, types are not abstract objects managed by an engine; they are explicit contracts dictating:
1. **Physical Bit Allocation**: The exact quantity of contiguous memory bytes assigned to store an entity.
2. **Binary Encoding Interpretation**: Whether a 4-byte sequence in RAM represents a two's-complement signed integer, an IEEE-754 floating-point number, an unsigned bitmask, or a virtual memory pointer.
3. **Machine Instruction Selection**: Whether an addition translates to an integer instruction (`add`), a floating-point instruction (`addss`), or a vector instruction (`vaddps`).

Relying on primitive types (`int`, `long`) without understanding architecture-specific data models (such as LP64 on Linux vs LLP64 on Windows) creates critical software vulnerabilities, truncation bugs, and portability failures.

---

## 2. Core Concepts

### Standard Primitive Types and ISO Minimum Widths

The ISO C specification **never defines absolute byte sizes** for primitive types. Instead, it defines **minimum bit widths** and ordering relationships:

$$\text{sizeof}(\text{char}) \le \text{sizeof}(\text{short}) \le \text{sizeof}(\text{int}) \le \text{sizeof}(\text{long}) \le \text{sizeof}(\text{long long})$$

| Type | Standard Minimum Width | Typical 32-bit (ILP32) | Typical 64-bit Linux (LP64) | Typical 64-bit Windows (LLP64) |
|---|---|---|---|---|
| `char` | $\ge 8$ bits (exactly 1 byte) | 1 byte | 1 byte | 1 byte |
| `short` | $\ge 16$ bits | 2 bytes | 2 bytes | 2 bytes |
| `int` | $\ge 16$ bits | 4 bytes | 4 bytes | 4 bytes |
| `long` | $\ge 32$ bits | 4 bytes | **8 bytes** | **4 bytes** |
| `long long` | $\ge 64$ bits | 8 bytes | 8 bytes | 8 bytes |
| `float` | 32 bits (IEEE 754) | 4 bytes | 4 bytes | 4 bytes |
| `double` | 64 bits (IEEE 754) | 8 bytes | 8 bytes | 8 bytes |
| `long double` | $\ge \text{sizeof}(\text{double})$ | 8 or 12 bytes | 16 bytes (x87 80-bit padded) | 8 bytes |

```
Standard Guarantee: sizeof(char) == 1 by definition in all ISO C standards.
CHAR_BIT in <limits.h> defines how many bits compose a byte (typically 8).
```

### Modern Fixed-Width Types (`<stdint.h>`)

Because primitive widths vary across architectures, C99 introduced `<stdint.h>`:

- **Exact-width integers**: `int8_t`, `int16_t`, `int32_t`, `int64_t` (and unsigned `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`).
- **Pointer-sized integers**: `intptr_t` and `uintptr_t` (integers guaranteed to hold a void pointer without truncation).
- **Fastest and least-width types**: `int_fast32_t`, `int_least32_t`.
- **Maximum-width integers**: `intmax_t`, `uintmax_t`.

### Standard Memory Types (`<stddef.h>`)

- `size_t`: Unsigned integer type of the result of `sizeof`. Represents the size of any object in memory.
- `ptrdiff_t`: Signed integer type of the result of subtracting two pointers.
- `nullptr_t`: (Standardized in C23) The distinct type of the `nullptr` literal.

---

## 3. How It Works Internally

### Data Models: LP64 vs. LLP64

When software transitioned to 64-bit architectures, operating systems chose differing data models:

```
+-------------------------------------------------------------------------+
|                  64-BIT DATA MODEL ARCHITECTURE MATRIX                  |
+-------------------------------------------------------------------------+
|  Model   | OS Target          | short | int | long | long long | Pointer|
|  ------- | ------------------ | ----- | --- | ---- | --------- | ------ |
|  ILP32   | 32-bit Linux / Win | 16    | 32  | 32   | 64        | 32     |
|  LP64    | 64-bit Linux / Mac | 16    | 32  | 64   | 64        | 64     |
|  LLP64   | 64-bit Windows     | 16    | 32  | 32   | 64        | 64     |
+-------------------------------------------------------------------------+
```

*CRITICAL WINDOWS TRAP*: On 64-bit Windows (LLP64), `sizeof(long) == 4`! Casting a 64-bit pointer to `long` silently discards the upper 32 bits, crashing the process. Always use `uintptr_t` or `intptr_t` when converting pointers to integers.

### Floating-Point IEEE 754 Internal Encoding

A standard 32-bit `float` in memory is structured into three bitfields:

```
 31    30        23 22                                  0
+----+-------------+------------------------------------+
| S  |   Exponent  |              Fraction              |
| (1)|   (8 bits)  |             (23 bits)              |
+----+-------------+------------------------------------+
```
- **Sign (S)**: 1 bit ($0 = \text{positive}, 1 = \text{negative}$).
- **Biased Exponent**: 8 bits (bias of 127).
- **Mantissa / Fraction**: 23 bits (normalized with an implicit leading 1).

Because floating-point numbers represent values as binary fractions, base-10 decimals like `0.1` cannot be represented with finite binary digits, leading to precision drift: `0.1f + 0.2f != 0.3f`.

---

## 4. Syntax & Usage

### 1. Conforming Format Specifiers for Fixed-Width Types (`<inttypes.h>`)
When printing `<stdint.h>` types with `printf()`, never use `%d` or `%ld` directly; use the portable macros defined in `<inttypes.h>`:

```c
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

void print_values(void) {
    uint32_t u32 = 4294967295U;
    int64_t i64 = -9223372036854775807LL;
    size_t sz = 1024;
    ptrdiff_t diff = -50;

    printf("uint32_t: %" PRIu32 "\n", u32);
    printf("int64_t:  %" PRId64 "\n", i64);
    printf("size_t:   %zu\n", sz);
    printf("ptrdiff:  %td\n", diff);
}
```

### 2. C23 Native Literals and Keywords
In ISO C23:
```c
/* Binary literals (0b...) */
int flags = 0b10100110;

/* Digit separators */
long debt = 1_000_000_000L;

/* Native boolean */
bool is_active = true;

/* Native nullptr */
int *ptr = nullptr;
```

---

## 5. Practical Examples

### Verification of Types and Bit Widths

```c
/* File: 01-languages/c/01-fundamentals/examples/02-data-types.c */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>

int main(void) {
    printf("=== C Data Types & Widths Verification ===\n");
    printf("sizeof(char):        %zu byte  (Bits: %d)\n", sizeof(char), CHAR_BIT);
    printf("sizeof(short):       %zu bytes\n", sizeof(short));
    printf("sizeof(int):         %zu bytes\n", sizeof(int));
    printf("sizeof(long):        %zu bytes\n", sizeof(long));
    printf("sizeof(long long):   %zu bytes\n", sizeof(long long));
    printf("sizeof(size_t):      %zu bytes\n", sizeof(size_t));
    printf("sizeof(ptrdiff_t):   %zu bytes\n", sizeof(ptrdiff_t));
    printf("sizeof(intptr_t):    %zu bytes\n", sizeof(intptr_t));
    printf("sizeof(double):      %zu bytes\n", sizeof(double));

    /* Fixed-width integers */
    uint32_t val32 = 0xDEADBEEFU;
    int64_t val64 = 9223372036854775807LL;
    printf("uint32_t value:      0x%X\n", val32);
    printf("int64_t max value:   %lld\n", (long long)val64);
    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 02-data-types.c -o run_02.exe
./run_02.exe
```
Output:
```text
=== C Data Types & Widths Verification ===
sizeof(char):        1 byte  (Bits: 8)
sizeof(short):       2 bytes
sizeof(int):         4 bytes
sizeof(long):        4 bytes
sizeof(long long):   8 bytes
sizeof(size_t):      4 bytes
sizeof(ptrdiff_t):   4 bytes
sizeof(intptr_t):    4 bytes
sizeof(double):      8 bytes
uint32_t value:      0xDEADBEEF
int64_t max value:   9223372036854775807
```
*(Notice on this 32-bit execution target: `sizeof(long)` and `sizeof(size_t)` are 4 bytes, proving that assuming 64-bit widths without `<stdint.h>` is a fatal flaw!)*

---

## 6. Best Practices

1. **Use `<stdint.h>` Types Everywhere in Structs and Networking**: Never use `int` or `long` in file formats, serialized packets, or memory-mapped hardware. Always specify `uint32_t`, `uint16_t`, or `uint8_t`.
2. **Use `size_t` for All Array Indexing and Loop Counts**: `size_t` matches the machine's address space. Using `int` for array indices invokes undefined behavior if an array exceeds $2^{31}-1$ elements.
3. **Never Compare Floats with `==`**: Due to IEEE 754 precision limitations, compare floats using an epsilon: `fabs(a - b) < 1e-6f`.
4. **Use `<inttypes.h>` Format Macros**: Guarantees format string safety when printing fixed-width types across 32-bit and 64-bit platforms.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Casting pointer to `long` | Fails on 64-bit Windows where `sizeof(long) == 4` and pointers are 8 bytes, truncating address. | Cast to `uintptr_t` or `intptr_t`. |
| Assuming `char` is always signed | The C standard leaves signedness of `char` implementation-defined (ARM/PowerPC default to unsigned; x86 defaults to signed). | Explicitly write `signed char` or `unsigned char` when doing arithmetic. |
| Using `%d` to print `size_t` | `size_t` is unsigned and often 64-bit, causing stack corruption or garbage prints. | Use `%zu`. |
| Comparing floating point with `if (val == 0.0)` | Minor rounding errors cause comparisons to fail unexpectedly. | Check absolute difference against an epsilon: `fabs(val) < 1e-7`. |
| Assuming `sizeof(int) == 4` | On 16-bit microcontrollers (AVR, MSP430), `sizeof(int) == 2`. | Use `int32_t` if you require 32 bits. |
| Negating `INT_MIN` | In two's complement, $| \text{INT\_MIN} | = \text{INT\_MAX} + 1$, which overflows signed `int`, invoking undefined behavior! | Validate or compute in wider or unsigned type. |

---

## 8. Performance & Security Notes

- **Integer Truncation Exploits**: Assigning a 64-bit `uint64_t` length to a 32-bit `uint32_t` variable silently discards upper bits. If this truncated length is used in an allocation while the original length is used in `memcpy`, an immediate remote heap overflow occurs.
- **Natural Alignment Speedups**: Modern CPUs read memory fastest when data is aligned to multiples of its size (4-byte ints at 4-byte boundaries, 8-byte doubles at 8-byte boundaries). Misaligned accesses incur CPU hardware penalties or `SIGBUS` exceptions on architectures like SPARC or ARM.

---

## 9. Comparison / When to Use What

| Type | When to Use | Print Specifier |
|---|---|---|
| `size_t` | Object sizes, array indexing, loop limits | `%zu` |
| `ptrdiff_t` | Pointer subtraction, relative offsets | `%td` |
| `uint8_t` | Raw byte buffers, cryptographic keys, binary protocols | `PRIu8` / `02X` |
| `uint32_t` / `uint64_t` | Well-defined integer ranges, hash calculations, bitmasks | `PRIu32` / `PRIu64` |
| `uintptr_t` | Converting pointers to integers for hashing or bitwise tagging | `PRIxPTR` |
| `double` | General mathematical and scientific calculation | `%f` / `%lf` |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `sizeof(char)` guaranteed to be 1 in C, but `CHAR_BIT` is not guaranteed to be 8?</strong></summary>

The ISO C specification defines `sizeof` as returning units of `sizeof(char)`, meaning `sizeof(char)` is 1 by mathematical definition. However, a byte is defined as the addressable unit of data capable of holding a character, which on specialized DSPs or historical mainframes can be 16, 24, or 32 bits (`CHAR_BIT >= 8`).
</details>

<details>
<summary><strong>2. What is the difference between LP64 and LLP64 data models?</strong></summary>

In LP64 (used by 64-bit Linux, macOS, BSD), `long` and pointers are 64-bit, while `int` is 32-bit. In LLP64 (used by 64-bit Windows), only `long long` and pointers are 64-bit; `long` remains 32-bit.
</details>

<details>
<summary><strong>3. Why does casting `void *` to `long` fail on 64-bit Windows?</strong></summary>

On 64-bit Windows (LLP64), pointers are 64 bits (8 bytes), but `long` is only 32 bits (4 bytes). Casting a pointer to `long` truncates the upper 32 bits of the memory address, causing catastrophic memory corruption if cast back.
</details>

<details>
<summary><strong>4. What is the signedness of plain `char` in standard C?</strong></summary>

It is implementation-defined. Compilers for x86 architectures typically configure `char` as signed by default (`-fsigned-char`), while ARM and PowerPC compilers configure `char` as unsigned by default (`-funsigned-char`). Portable code must explicitly use `signed char` or `unsigned char` if performing arithmetic or bitwise shifts.
</details>

<details>
<summary><strong>5. What is `nullptr_t` in C23, and why was it introduced?</strong></summary>

`nullptr_t` is the distinct type of the `nullptr` literal introduced in C23. In earlier standards, `NULL` was typically defined as `((void*)0)` or plain `0`. The lack of a distinct pointer type caused ambiguities in `_Generic` expressions and function overloads; `nullptr` provides an unambiguous, type-safe null pointer constant.
</details>

<details>
<summary><strong>6. What is the difference between `size_t` and `uintptr_t`?</strong></summary>

`size_t` represents the size of any object in bytes (the result of `sizeof`). `uintptr_t` is an unsigned integer type capable of storing a pointer address without truncation. While they often have the same bit width on linear memory architectures, their semantic purpose differs, and on segmented memory architectures their sizes can diverge.
</details>

<details>
<summary><strong>7. Why can't `0.1` be represented exactly in standard IEEE 754 floating-point?</strong></summary>

Because numbers are encoded in binary (base-2) powers ($1/2, 1/4, 1/8, 1/16, \dots$). The fraction $1/10$ in binary is an infinitely repeating fraction: $0.0001100110011\dots_2$. Stored in a 23-bit or 52-bit mantissa, it must be rounded, resulting in a representation error.
</details>

<details>
<summary><strong>8. What is the macro for printing a `uint64_t` value portably?</strong></summary>

The `PRIu64` macro from `<inttypes.h>`. It expands to `"lu"` on LP64 systems and `"llu"` on LLP64 systems, allowing portable syntax: `printf("%" PRIu64 "\n", val);`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Identifying Proper Format Specifiers</strong></summary>

**Goal**: Fix the format specifier errors in this code snippet:
```c
size_t len = 100;
uint32_t id = 500;
int64_t balance = -12000;
printf("Length: %d, ID: %d, Balance: %ld\n", len, id, balance);
```

**Solution**:
```c
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

size_t len = 100;
uint32_t id = 500;
int64_t balance = -12000;
printf("Length: %zu, ID: %" PRIu32 ", Balance: %" PRId64 "\n", len, id, balance);
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Detecting System Pointer Width</strong></summary>

**Goal**: Write a standard C preprocessor check that detects whether the compilation target is 32-bit or 64-bit using `<stdint.h>` limit macros.

**Solution**:
```c
#include <stdio.h>
#include <stdint.h>

int main(void) {
#if UINTPTR_MAX == 0xFFFFFFFFU
    printf("Target Architecture: 32-bit Address Space\n");
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFULL
    printf("Target Architecture: 64-bit Address Space\n");
#else
    printf("Target Architecture: Non-standard address width\n");
#endif
    return 0;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Safe Floating-Point Equality Function</strong></summary>

**Goal**: Implement a robust floating-point comparison function `almost_equal(float a, float b, float epsilon)` that handles positive and negative zero, near-zero comparisons, and subnormal floats properly.

**Solution**:
```c
#include <stdbool.h>
#include <math.h>

bool almost_equal(float a, float b, float epsilon) {
    /* If identical (e.g. both are infinity or exact zero) */
    if (a == b) {
        return true;
    }
    
    float diff = fabsf(a - b);
    
    /* If numbers are close to zero, absolute difference suffices */
    if (a == 0.0f || b == 0.0f || diff < 1e-7f) {
        return diff < (epsilon * 1e-7f);
    }
    
    /* Relative difference comparison */
    return diff / fminf(fabsf(a) + fabsf(b), 3.402823466e+38F) < epsilon;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- `sizeof(char) == 1` always; `CHAR_BIT` is in `<limits.h>`.
- **LP64** (Linux/macOS): `long` is 64-bit.
- **LLP64** (Windows): `long` is 32-bit!
- Always use `<stdint.h>` (`uint32_t`, `int64_t`) for binary protocols and structs.
- Use `size_t` (`%zu`) for memory sizes and array indexing.
- Use `uintptr_t` (`PRIxPTR`) to store pointer addresses in integers.
- C23: `bool`, `true`, `false`, `nullptr`, `0b...` literals, `1'000'000` digit separators.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 6.2.5: "Types".
- David Goldberg: *What Every Computer Scientist Should Know About Floating-Point Arithmetic*.
- SEI CERT C Coding Standard: *INT31-C. Ensure that integer conversions do not result in lost or misinterpreted data*.

---

[← Previous: 01-program-structure.md](./01-program-structure.md) | [Section Index](./README.md) | [Next: 03-integer-representation.md](./03-integer-representation.md) →
