# Integer Representation and Bit-Level Mechanics

> The low-level architecture of integers in C: two's complement mechanics, signed overflow undefined behavior vs unsigned modulo arithmetic, endianness and byte-swapping, sign extension, C23 bit-precise integers (`_BitInt(N)`), and `<stdckdint.h>` checked arithmetic.

| Metadata | Details |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [02-data-types.md](./02-data-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard Baseline** | ISO C11 / C17 / C23 (Two's Complement Standardized) |

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

In C, integers are not boundless abstract numbers; they are fixed-width hardware register encodings. A failure to understand bit-level integer representation is the single most common cause of critical security vulnerabilities:
1. **Signed Overflow Undefined Behavior**: The C standard dictates that signed integer overflow is **undefined behavior**. Compilers routinely eliminate security checks like `if (a + 100 < a)` under `-O2` because the compiler assumes signed overflow is mathematically impossible!
2. **Unsigned Wrap-Around**: Unsigned integers never overflow; they obey modular arithmetic ($\pmod{2^N}$). While well-defined, unintended wrap-around causes negative index crashes when subtracted.
3. **Endianness Mismatches**: Sending raw multi-byte integers across a network or writing them to disk without byte-order conversion corrupts data when transferred between x86 (little-endian) and network protocols (big-endian).

---

## 2. Core Concepts

### Two's Complement Representation

While historical standards (C89, C99, C11) permitted sign-and-magnitude or ones' complement, **ISO C23 officially mandated two's complement as the sole legal signed integer representation**.

In an $N$-bit signed two's complement integer:
- The Most Significant Bit (MSB) carries negative weight: $-2^{N-1}$.
- All other bits carry positive weights: $\sum_{i=0}^{N-2} b_i 2^i$.

$$\text{Value} = -b_{N-1} 2^{N-1} + \sum_{i=0}^{N-2} b_i 2^i$$

```
32-bit Integer Two's Complement Examples:
Value  1:  0x00000001 -> [00000000 00000000 00000000 00000001]
Value -1:  0xFFFFFFFF -> [11111111 11111111 11111111 11111111]
INT32_MIN: 0x80000000 -> [10000000 00000000 00000000 00000000] (-2,147,483,648)
INT32_MAX: 0x7FFFFFFF -> [01111111 11111111 11111111 11111111] (+2,147,483,647)
```
*Key Asymmetry*: In two's complement, there is one more negative number than positive numbers. Consequently, evaluating `-INT_MIN` cannot fit within the signed type, triggering immediate signed overflow undefined behavior!

### Signed Overflow vs. Unsigned Wrap-Around

| Operation | Signed Integer (`int32_t`) | Unsigned Integer (`uint32_t`) |
|---|---|---|
| Exceeding Maximum Limit | **Undefined Behavior (UB)**: Compiler may delete code, crash, or format drive | **Well-Defined Modular Arithmetic**: Wraps around modulo $2^{32}$ (`0`) |
| Below Minimum Limit | **Undefined Behavior (UB)** | **Well-Defined Modular Arithmetic**: Wraps around modulo $2^{32}$ (`UINT32_MAX`) |
| Compiler Optimization | Compiler assumes it **never** happens | Compiler generates explicit truncation/masking instructions |

---

## 3. How It Works Internally

### Endianness: Little-Endian vs. Big-Endian

Endianness defines the sequential ordering of bytes composing a multi-byte integer in physical memory:

```
Consider 32-bit Integer: 0xA1B2C3D4 (Stored at address 0x1000)

Big-Endian (Network Byte Order, Motorola, SPARC):
Most Significant Byte (0xA1) stored at lowest memory address.
Address: [0x1000]  [0x1001]  [0x1002]  [0x1003]
Bytes:      A1        B2        C3        D4

Little-Endian (x86, x86-64, modern ARM default):
Least Significant Byte (0xD4) stored at lowest memory address.
Address: [0x1000]  [0x1001]  [0x1002]  [0x1003]
Bytes:      D4        C3        B2        A1
```

### Sign Extension vs. Zero Extension

When promoting a smaller integer to a larger integer (e.g. `int8_t` to `int32_t`):
- **Sign Extension (Signed Types)**: The CPU copies the sign bit (MSB) into all newly added upper bits.
  - `-1` (`0xFF`) as 8-bit sign-extends to `0xFFFFFFFF` as 32-bit. Instruction: `movsx` (Move with Sign Extension).
- **Zero Extension (Unsigned Types)**: The CPU fills all newly added upper bits with zeros.
  - `255` (`0xFF`) as 8-bit zero-extends to `0x000000FF` as 32-bit. Instruction: `movzx` (Move with Zero Extension).

---

## 4. Syntax & Usage

### 1. Byte Swapping for Network Portability
```c
#include <stdint.h>

/* Convert 32-bit integer between host and network (Big-Endian) byte order */
static inline uint32_t swap_uint32(uint32_t val) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(val); /* Single CPU instruction: bswap */
#else
    return ((val >> 24) & 0xFF) |
           ((val >> 8)  & 0xFF00) |
           ((val << 8)  & 0xFF0000) |
           ((val << 24) & 0xFF000000);
#endif
}
```

### 2. C23 Checked Arithmetic (`<stdckdint.h>`)
In ISO C23, checked arithmetic eliminates fragile manual overflow detection logic:

```c
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#include <stdckdint.h>
#include <stdbool.h>

bool safe_sum(int a, int b, int *result) {
    /* Returns true if overflow occurred; stores wrapped result in *result */
    return ckd_add(result, a, b);
}
#endif
```

---

## 5. Practical Examples

### Two's Complement & Endianness Inspection Program

```c
/* File: 01-languages/c/01-fundamentals/examples/03-integer-representation.c */
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

static void inspect_bytes(const void *ptr, size_t n) {
    const uint8_t *b = (const uint8_t *)ptr;
    for (size_t i = 0; i < n; ++i) {
        printf("%02X ", b[i]);
    }
    printf("\n");
}

int main(void) {
    printf("=== Integer Representation & Two's Complement ===\n");
    int32_t pos = 1;
    int32_t neg = -1;
    int32_t min_int = INT32_MIN;

    printf("Value  1 in memory (hex bytes): ");
    inspect_bytes(&pos, sizeof(pos));

    printf("Value -1 in memory (hex bytes): ");
    inspect_bytes(&neg, sizeof(neg));

    printf("INT32_MIN in memory (hex bytes): ");
    inspect_bytes(&min_int, sizeof(min_int));

    /* Unsigned modular arithmetic wrap-around */
    uint32_t u_max = UINT32_MAX;
    uint32_t u_wrapped = u_max + 1U;
    printf("UINT32_MAX (%u) + 1 = %u (Defined modular wrap)\n", u_max, u_wrapped);

    /* Endianness check */
    uint16_t endian_test = 0x0102;
    uint8_t *first_byte = (uint8_t *)&endian_test;
    if (*first_byte == 0x02) {
        printf("Host Byte Ordering: Little-Endian\n");
    } else {
        printf("Host Byte Ordering: Big-Endian\n");
    }

    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 03-integer-representation.c -o run_03.exe
./run_03.exe
```
Output:
```text
=== Integer Representation & Two's Complement ===
Value  1 in memory (hex bytes): 01 00 00 00 
Value -1 in memory (hex bytes): FF FF FF FF 
INT32_MIN in memory (hex bytes): 00 00 00 80 
UINT32_MAX (4294967295) + 1 = 0 (Defined modular wrap)
Host Byte Ordering: Little-Endian
```

---

## 6. Best Practices

1. **Never Check for Signed Overflow After the Addition Occurs**: Writing `if (a + b < a)` is undefined behavior. Compilers optimize this check away entirely under `-O2`! Always validate preconditions before addition: `if (a > INT_MAX - b)`.
2. **Use Unsigned Integers for Bitwise Manipulations**: Shifting a signed negative integer (`x >> 2`) is implementation-defined arithmetic right-shift, and shifting into the sign bit is undefined behavior. Always cast bitmasks to `uint32_t` or `uint64_t`.
3. **Always Serialize Multi-Byte Integers in Big-Endian (Network Byte Order)**: Use `htons()`, `htonl()`, `ntohs()`, `ntohl()` (or compiler builtins `__builtin_bswap32`) when sending integers across sockets or saving binary files.
4. **Use C23 `<stdckdint.h>` When Available**: Standardizes fast, branchless, hardware-flag-assisted overflow checks (`ckd_add`, `ckd_mul`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Writing `if (a + b < a)` to detect overflow | Signed overflow is UB. Under `-O2`, compiler deletes the `if` check completely, assuming overflow never happens! | Check bounds beforehand: `if (b > 0 && a > INT_MAX - b)`. |
| Negating `INT_MIN` (`-INT_MIN`) | $| \text{INT\_MIN} |$ is $2^{31}$, but `INT_MAX` is only $2^{31}-1$. The negation overflows signed `int`, invoking UB. | Store in wider type (`int64_t`) or unsigned representation. |
| Right-shifting signed negative integers (`-4 >> 1`) | Implementation-defined behavior (arithmetic vs logical shift). | Cast to unsigned type before right shifting. |
| Bit-shifting by an amount $\ge$ bit-width | `1U << 32` on a 32-bit integer invokes undefined behavior on x86 processors. | Ensure shift amount is strictly $< \text{width}$. |
| Casting `char` to `int` expecting positive byte value | If `char` is signed, a byte with MSB set (`0x80`) sign-extends to `0xFFFFFF80` (-128) rather than +128. | Cast through `(unsigned char)`: `(int)(unsigned char)c`. |
| Storing binary protocol structs directly to disk | Fails when read on a machine with opposite endianness or different struct packing alignment. | Serialize fields individually with explicit byte-order conversions. |

---

## 8. Performance & Security Notes

- **The Arithmetic Overflow Vulnerability**: Attackers pass massive values to size calculations (e.g. `num_elements * sizeof(struct item)`). If multiplication overflows a 32-bit integer, it wraps around to a tiny number (e.g. 16 bytes). `malloc(16)` succeeds, but the subsequent loop writes hundreds of items into that 16-byte buffer, granting attackers remote code execution!
- **Hardware Flag Utilization**: Processors have hardware flags (Overflow Flag `OF`, Carry Flag `CF`). Compilers with `<stdckdint.h>` inspect these flags directly without branching, achieving zero-cost overflow detection.

---

## 9. Comparison / When to Use What

| Category | Signed (`int32_t`) | Unsigned (`uint32_t`) |
|---|---|---|
| **Semantic Meaning** | Quantities, balances, relative coordinates, return error codes | Bitmasks, memory sizes (`size_t`), cryptographic hashes, modulo counters |
| **Overflow Behavior** | **Undefined Behavior** (UB) | Defined modulo $2^N$ wrap-around |
| **Bitwise Operations** | Dangerous / Implementation-defined | Safe, fully portable |
| **Loop Terminators** | Safe for `i >= 0` termination | `for (size_t i = 10; i >= 0; --i)` causes infinite loop! |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is signed integer overflow undefined behavior in C, while unsigned integer overflow is defined?</strong></summary>

Historically, C was designed to run across diverse hardware architectures, including ones' complement, two's complement, and sign-magnitude processors, some of which triggered hardware trap exceptions on signed overflow. Making signed overflow UB gave compiler optimizers the freedom to assume variables never wrap, unlocking aggressive loop transformations. Unsigned integers were explicitly specified to emulate modular abstract algebra ($\pmod{2^N}$).
</details>

<details>
<summary><strong>2. What is the value of `-INT_MIN` in standard C on a two's complement system?</strong></summary>

It invokes undefined behavior. In a 32-bit two's complement system, `INT_MIN` is $-2,147,483,648$, while `INT_MAX` is $+2,147,483,647$. Because $+2,147,483,648$ cannot be represented in a 32-bit signed `int`, negating it causes signed integer overflow.
</details>

<details>
<summary><strong>3. Why does the compiler optimize away the check `if (x + 1 > x)` under `-O2`?</strong></summary>

Under the ISO C standard, signed integer overflow is undefined behavior. The compiler's optimizer assumes that the program never executes undefined behavior. Under that premise, adding $1$ to any integer mathematically produces a greater number; therefore, the expression is evaluated as compile-time `true`, and the branch is removed.
</details>

<details>
<summary><strong>4. How do you safely detect if multiplying two positive signed integers `a` and `b` will overflow?</strong></summary>

Check preconditions before the multiplication occurs:
```c
if (a > 0 && b > 0 && a > INT_MAX / b) {
    /* Overflow will occur */
}
```
In C23, use `ckd_mul(&result, a, b)`.
</details>

<details>
<summary><strong>5. What is the difference between Big-Endian and Little-Endian byte ordering?</strong></summary>

Big-Endian stores the most significant byte of a multi-byte word at the lowest memory address (natural reading order from left to right). Little-Endian stores the least significant byte at the lowest memory address.
</details>

<details>
<summary><strong>6. Why does `for (unsigned int i = 5; i >= 0; --i)` create an infinite loop?</strong></summary>

Because an `unsigned int` can never be negative. When `i` reaches `0` and is decremented by `--i`, it wraps around via modular arithmetic to `UINT_MAX` (e.g. `4294967295`), which is still $\ge 0$, resulting in an infinite loop.
</details>

<details>
<summary><strong>7. What are `_BitInt(N)` types introduced in C23?</strong></summary>

C23 standardized bit-precise integers `_BitInt(N)` (and `unsigned _BitInt(N)`), allowing variables of exact arbitrary bit width (e.g. `_BitInt(24) val;` or `_BitInt(128)`). This eliminates padding bytes and matches hardware registers in FPGA and embedded architectures.
</details>

<details>
<summary><strong>8. What happens when a signed 8-bit `-1` (`0xFF`) is cast to an `unsigned int` on a 32-bit system?</strong></summary>

First, integer promotion promotes the signed `int8_t` to signed `int32_t` via sign extension, resulting in `0xFFFFFFFF` (-1). Next, converting signed `int32_t` (-1) to `unsigned int` wraps it modulo $2^{32}$, producing `4,294,967,295U` (`0xFFFFFFFF`).
</details>

---

<details>
<summary><strong>9. Why did C23 eliminate support for ones' complement and sign-and-magnitude representations?</strong></summary>

All modern computing hardware manufactured in the last four decades uses two's complement arithmetic. The historical flexibility in C89/C99/C11 forced the standard to accommodate multiple representations of negative zero (+0 and -0) and padding bits, which added specification complexity and prevented compiler optimizations. Standardizing two's complement simplifies the language and aligns C with hardware reality.
</details>

<details>
<summary><strong>10. How does sign extension work at the assembly level in x86-64?</strong></summary>

x86-64 provides dedicated instructions for sign extension:
- movsx (Move with Sign-Extension): Moves an 8-bit or 16-bit source into a 32-bit register, copying the sign bit.
- movsxd: Moves a 32-bit source into a 64-bit destination register with sign-extension.
- cdqe (Convert Doubleword to Quadword Extended): Extends %eax to %rax.
- cqo: Extends %rax into %rdx:%rax before signed division (idiv).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Runtime Endianness Detection</strong></summary>

**Goal**: Write a C function `is_little_endian(void)` that returns `1` if running on a little-endian machine, and `0` on a big-endian machine, without using macros.

**Solution**:
```c
int is_little_endian(void) {
    unsigned int test = 1U;
    char *byte_ptr = (char *)&test;
    return (int)(*byte_ptr);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Safe Integer Addition Before C23</strong></summary>

**Goal**: Implement a safe addition function `safe_add(int a, int b, int *res)` that returns `0` on success and `-1` if signed overflow would occur, without triggering undefined behavior.

**Solution**:
```c
#include <limits.h>

int safe_add(int a, int b, int *res) {
    if ((b > 0 && a > INT_MAX - b) || 
        (b < 0 && a < INT_MIN - b)) {
        return -1; /* Overflow */
    }
    *res = a + b;
    return 0;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Portable 64-bit Byte Swap</strong></summary>

**Goal**: Implement a branchless 64-bit endian swap function `swap64(uint64_t val)` using only bitwise shifts, masks, and OR operations.

**Solution**:
```c
#include <stdint.h>

uint64_t swap64(uint64_t val) {
    return ((val & 0x00000000000000FFULL) << 56) |
           ((val & 0x000000000000FF00ULL) << 40) |
           ((val & 0x0000000000FF0000ULL) << 24) |
           ((val & 0x00000000FF000000ULL) << 8)  |
           ((val & 0x000000FF00000000ULL) >> 8)  |
           ((val & 0x0000FF0000000000ULL) >> 24) |
           ((val & 0x00FF000000000000ULL) >> 40) |
           ((val & 0xFF00000000000000ULL) >> 56);
}
```
</details>

---

## 12. Summary / Cheat Sheet

- Two's complement is standardized as the sole representation in C23.
- Signed overflow is **UB**; unsigned overflow is **defined modular wrap** ($\pmod{2^N}$).
- Endianness: Little-endian stores LSB first (x86); Big-endian stores MSB first (Network).
- Never negate `INT_MIN` directly (`-INT_MIN` is UB).
- Use `ckd_add`, `ckd_mul` in C23 `<stdckdint.h>`.
- Use `_BitInt(N)` for exact bit-width registers in C23.

---

## 13. Further Reading

- ISO/IEC 9899:2024 (C23 Standard), Section 6.2.6.2: "Integer types".
- Henry S. Warren Jr.: *Hacker's Delight* (Second Edition).
- SEI CERT C Coding Standard: *INT32-C. Ensure that operations on signed integers do not result in overflow*.

---

[← Previous: 02-data-types.md](./02-data-types.md) | [Section Index](./README.md) | [Next: 04-variables-constants.md](./04-variables-constants.md) →
