# 22 Integer Taxonomy: stdint, inttypes & limits

> Master the standard integer hierarchy, contrast platform data models (ILP32 vs LP64 vs LLP64), utilize exact-width integers, and format values portably using `<inttypes.h>`.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [01-fundamentals/02-data-types.md](../01-fundamentals/02-data-types.md), [01-fundamentals/03-integer-representation.md](../01-fundamentals/03-integer-representation.md) |
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

In early C programming, integer types (`short`, `int`, `long`) lacked fixed bit widths. The ISO C standard only specified minimal relational bounds:
$$\text{sizeof(char)} \le \text{sizeof(short)} \le \text{sizeof(int)} \le \text{sizeof(long)} \le \text{sizeof(long long)}$$
On some platforms `int` was 16 bits; on others, 32 bits. On 64-bit Linux `long` is 64 bits, while on 64-bit Windows `long` is 32 bits!

This ambiguity wreaked havoc on systems software:
- **Binary Protocols & File Formats**: An IP packet header expects exactly 32 bits for an IPv4 address. Writing `unsigned long ip;` breaks across 64-bit operating systems.
- **Buffer Allocation & Overflow**: Assuming `int` can hold 4 billion values causes security vulnerabilities on 16-bit microcontrollers.
- **Compiler Warnings**: Printing 64-bit integers with `%lu` works on Linux but triggers compiler warnings or errors on Windows.

To resolve this fragmentation, ISO C99 introduced `<stdint.h>` and `<inttypes.h>`, providing exact-width types, guaranteed-minimum types, fastest-width types, and cross-platform format string specifiers.

---

## 2. Core Concepts

### 2.1 The Platform Data Models
Operating systems adopt different integer size models:
- **ILP32 (32-bit Linux / Windows)**: `int`, `long`, and pointers are 32 bits.
- **LP64 (64-bit Linux, macOS, BSD)**: `int` is 32 bits; `long` and pointers are 64 bits.
- **LLP64 (64-bit Windows)**: `int` and `long` are 32 bits; `long long` and pointers are 64 bits.

```
Data Model Comparison (Bit Widths):
┌──────────┬───────┬───────┬───────────┬─────────┐
│ Model    │ int   │ long  │ long long │ Pointer │
├──────────┼───────┼───────┼───────────┼─────────┤
│ ILP32    │ 32    │ 32    │ 64        │ 32      │
│ LP64     │ 32    │ 64    │ 64        │ 64      │
│ LLP64    │ 32    │ 32    │ 64        │ 64      │
└──────────┴───────┴───────┴───────────┴─────────┘
```

### 2.2 The `<stdint.h>` Integer Categories
1. **Exact-Width Types**: Guaranteed to have exactly $N$ bits with two's complement representation:
   - `int8_t`, `uint8_t` (8 bits)
   - `int16_t`, `uint16_t` (16 bits)
   - `int32_t`, `uint32_t` (32 bits)
   - `int64_t`, `uint64_t` (64 bits)
2. **Minimum-Width Types**: Guaranteed to have at least $N$ bits:
   - `int_least8_t`, `int_least16_t`, `int_least32_t`, `int_least64_t`.
3. **Fastest-Width Types**: The fastest integer type available with at least $N$ bits on that CPU:
   - `int_fast8_t`, `int_fast16_t`, `int_fast32_t`, `int_fast64_t`.
4. **Pointer-Width Integers**: An integer guaranteed to be large enough to hold any object pointer address without truncation:
   - `intptr_t` (signed), `uintptr_t` (unsigned).
5. **Greatest-Width Types**: Largest integer type supported by the compiler:
   - `intmax_t`, `uintmax_t`.

### 2.3 Exact-Width Literal Macros
When writing preprocessor macros or initializing 64-bit integer variables, assigning an unadorned literal like `10000000000` can trigger compiler warnings because the literal defaults to signed `int`.
`<stdint.h>` defines literal constructor macros:
- `INT8_C(v)`, `UINT8_C(v)`: Appends type conversions for 8-bit literals.
- `INT16_C(v)`, `UINT16_C(v)`: Appends type conversions for 16-bit literals.
- `INT32_C(v)`, `UINT32_C(v)`: Appends `U` or `L` as appropriate for 32-bit types.
- `INT64_C(v)`, `UINT64_C(v)`: Appends `LL` or `ULL` for 64-bit types.
- `INTMAX_C(v)`, `UINTMAX_C(v)`: Appends suffix for the largest supported integer.

Example:
```c
uint64_t mask = UINT64_C(0xFFFFFFFFFFFFFFFF);
```

### 2.4 The `<inttypes.h>` Formatting Macros
Because the underlying primitive type for `uint64_t` varies between `unsigned long` (LP64) and `unsigned long long` (LLP64), using raw format strings like `"%lu"` is non-portable. `<inttypes.h>` provides format specifiers:
- `PRId32`, `PRIu32`, `PRIx32`
- `PRId64`, `PRIu64`, `PRIx64`
- `PRIxPTR` (pointer as hex)
- `SCNd32`, `SCNu64` (for `scanf`)

---

## 3. How It Works Internally

### Preprocessor Concatenation of String Literals
In C, adjacent string literals are automatically concatenated into a single string by the compiler:
```c
"Value: " "100" "\n"  -->  "Value: 100\n"
```
The `<inttypes.h>` macros exploit this standard feature:
```c
#if __LP64__
#define PRIu64 "lu"
#else
#define PRIu64 "llu"
#endif

printf("Val: %" PRIu64 "\n", val);
/* Expands on Linux to:   printf("Val: %" "lu" "
", val);   -> "Val: %lu
" */
/* Expands on Windows to: printf("Val: %" "llu" "
", val);  -> "Val: %llu
" */
```

---

### Compiler Architecture and Data Model Detection
Modern preprocessors define standard architecture macros that allow codebases to query data models:
- `__LP64__` or `_LP64`: Defined on 64-bit Linux, macOS, and BSD systems where `sizeof(long) == 8`.
- `_WIN32` and `_WIN64`: Defined on Windows systems where `sizeof(long) == 4` regardless of 32-bit or 64-bit target.
- `__ILP32__`: Defined on 32-bit compilation targets.
- `__SIZEOF_POINTER__`: GCC/Clang predefined macro indicating pointer size in bytes (4 or 8).

---

## 4. Syntax & Usage

### 4.1 Portable Formatting Examples
```c
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

void print_metrics(uint64_t bytes, uintptr_t address) {
    printf("Transferred: %" PRIu64 " bytes\n", bytes);
    printf("Buffer at:   0x%" PRIxPTR "\n", address);
}
```

### 4.2 Standard Integer Limits
```c
#include <limits.h>
#include <stdint.h>

/* Limits from <stdint.h> */
int64_t max_val = INT64_MAX;
uint32_t max_u   = UINT32_MAX;
size_t max_bytes = SIZE_MAX;
```

---

## 5. Practical Examples

### Comprehensive Example: Fixed-Width Printing and Pointer Manipulation
The following program demonstrates exact-width integer ranges, pointer-integer conversions, and `<inttypes.h>` formatting.

```c
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>

int main(void) {
    printf("=== stdint, inttypes, and limits Demonstration ===\n");

    /* 1. Exact-width types */
    int8_t   i8  = -128;
    uint8_t  u8  = 255;
    int16_t  i16 = -32768;
    uint16_t u16 = 65535;
    int32_t  i32 = -2147483647 - 1;
    uint32_t u32 = 4294967295u;
    int64_t  i64 = -9223372036854775807LL - 1;
    uint64_t u64 = 18446744073709551615ULL;

    printf("8-bit:   i8=%" PRId8 ", u8=%" PRIu8 "\n", i8, u8);
    printf("16-bit:  i16=%" PRId16 ", u16=%" PRIu16 "\n", i16, u16);
    printf("32-bit:  i32=%" PRId32 ", u32=%" PRIu32 "\n", i32, u32);
    printf("64-bit:  i64=%" PRId64 ", u64=%" PRIu64 "\n", i64, u64);

    /* 2. Pointer integer types */
    int dummy = 42;
    intptr_t ptr_val = (intptr_t)&dummy;
    uintptr_t uptr_val = (uintptr_t)&dummy;
    printf("Pointer as integer: 0x%" PRIxPTR ", unsigned: 0x%" PRIxPTR "\n", ptr_val, uptr_val);

    /* 3. stddef limits */
    size_t sz = sizeof(dummy);
    ptrdiff_t diff = 10 - 2;
    printf("size_t=%u, ptrdiff_t=%ld\n", (unsigned int)sz, (long)diff);

    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 22-stdint-inttypes-and-limits.c -o 22-stdint-inttypes-and-limits.exe
./22-stdint-inttypes-and-limits.exe
```

```text
=== stdint, inttypes, and limits Demonstration ===
8-bit:   i8=-128, u8=255
16-bit:  i16=-32768, u16=65535
32-bit:  i32=-2147483648, u32=4294967295
64-bit:  i64=-9223372036854775808, u64=18446744073709551615
Pointer as integer: 0x61feec, unsigned: 0x61feec
size_t=4, ptrdiff_t=8
```

---

## 6. Best Practices

1. **Use Exact-Width Types for Binary Interfaces**: Whenever serializing data, designing network packets, or mapping hardware registers, strictly use `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`.
2. **Use `size_t` for Memory Capacities and Loop Offsets**: `size_t` is guaranteed to match the addressable object size on the target platform.
3. **Use `uintptr_t` for Address Arithmetic**: If you must perform bitwise masking or alignment checks on a memory address, cast to `uintptr_t`, not `unsigned long`.
4. **Always Use `<inttypes.h>` Format Macros**: Never write `%lu` or `%lld` for fixed-width types. Always write `%" PRIu64 "`.
5. **Use Integer Constant Suffixes from `<stdint.h>`**: Use `INT64_C(100)` or `UINT32_C(0x1000)` to ensure literals have correct type width.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| `printf("%d", my_int32)` | While often working, on some ABIs `int32_t` is `long`, triggering format mismatch warnings. | Use `printf("%" PRId32, my_int32)`. |
| Casting pointer to `unsigned int` | On 64-bit platforms, `unsigned int` is 32 bits, truncating the top 32 bits of the address! | Cast to `uintptr_t`. |
| Assuming `long` is 64-bit | `long` is 32-bit on 64-bit Windows! | Use `int64_t` or `long long` for 64 bits. |
| Using `int` for array indexing | Arrays larger than 2GB on 64-bit systems will overflow signed 32-bit `int` indices. | Use `size_t`. |
| Comparing `size_t` with negative int | Negative `int` promotes to huge unsigned value, producing false comparisons. | Keep variables signed or verify `x >= 0`. |
| Assuming exact-width types always exist | On esoteric DSPs with 16-bit `char`, `int8_t` does not exist. | Use `int_least8_t` if supporting 16-bit byte DSPs. |

---

## 8. Performance & Security Notes

- **The `int_fastN_t` Tradeoff**: `int_fast16_t` selects the integer width that executes fastest on that CPU. On modern 64-bit x86, `int_fast16_t` is actually a 64-bit `long`! While faster in arithmetic loops, using it in large structs inflates memory consumption.
- **Pointer Truncation Vulnerabilities (CWE-704)**: Casting a pointer to a 32-bit `int` or `long` on 64-bit Windows truncates the high address bits. If the address resides above 4GB, dereferencing the truncated address crashes or executes unmapped memory.
- **Compiler Register Allocation**: Exact-width types smaller than the CPU register width (such as `uint8_t` on x86) may require extra zero-extension instructions (`MOVZX`) when loaded into 32-bit or 64-bit registers.

---

## 9. Comparison / When to Use What

| Type | Size Guarantee | Primary Use Case |
| :--- | :--- | :--- |
| `uint8_t` / `uint32_t` | Exactly 8 / 32 bits | Protocols, file headers, MMIO registers |
| `size_t` | Word size of address space | Buffer lengths, memory sizes, array indices |
| `ptrdiff_t` | Word size of address space (signed) | Result of subtracting two pointers |
| `uintptr_t` | Large enough to hold any pointer | Pointer bitwise math, address alignment |
| `int_fast32_t` | Fastest type $\ge$ 32 bits | High-throughput arithmetic compute loops |
| `int_least32_t` | Smallest type $\ge$ 32 bits | Portable minimal memory footprints |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `sizeof(long)` different on 64-bit Linux and 64-bit Windows?</strong></summary>

When transitioning from 32-bit to 64-bit computing, UNIX vendors adopted the **LP64** data model (`long` and pointers are 64-bit, `int` is 32-bit) to make `long` match pointer width. Microsoft chose the **LLP64** data model (`int` and `long` remain 32-bit, only `long long` and pointers are 64-bit) to preserve backward binary compatibility with legacy 32-bit Win32 APIs that heavily relied on `long` as a 32-bit integer. Consequently, code assuming `sizeof(long) == 8` fails on Windows.
</details>

<details>
<summary><strong>2. What is the difference between `size_t` and `uintptr_t`?</strong></summary>

`size_t` is an unsigned integer type returned by `sizeof` that is guaranteed to be able to represent the size in bytes of any individual object in memory. `uintptr_t` is an optional unsigned integer type that is guaranteed to be able to hold any void pointer address without data loss: `(void *)(uintptr_t)p == p`. While on modern flat-memory platforms they are typically the same size (4 bytes on 32-bit, 8 bytes on 64-bit), on segmented architectures an address pointer may be larger than any single object size.
</details>

<details>
<summary><strong>3. Are exact-width types like `int8_t` and `int32_t` guaranteed to exist on all C implementations?</strong></summary>

No. According to ISO C (§7.20.1.1), exact-width types `intN_t` and `uintN_t` are optional: they are only provided if the target architecture supports two's complement integers of exactly those bit widths with no padding bits. On certain specialized Digital Signal Processors (DSPs) where the smallest addressable memory unit (`char`) is 16 or 32 bits, `int8_t` cannot exist. In contrast, minimum-width types (`int_least8_t`) are mandatory on all conforming implementations.
</details>

<details>
<summary><strong>4. How do `<inttypes.h>` macros avoid compiler warnings across platforms?</strong></summary>

On 64-bit Linux (LP64), `uint64_t` is defined as `unsigned long`. On 64-bit Windows (LLP64), `uint64_t` is defined as `unsigned long long`. If you use `%llu` on Linux, the compiler warns of a type mismatch; if you use `%lu` on Windows, it warns of a 32-bit mismatch. The `<inttypes.h>` macro `PRIu64` expands to `"lu"` on Linux and `"llu"` on Windows. The compiler concatenates adjacent string literals at compile time, guaranteeing matching format specifiers across all platforms.
</details>

<details>
<summary><strong>5. What is `ptrdiff_t` and when should it be used?</strong></summary>

`ptrdiff_t` is a signed integer type defined in `<stddef.h>` that represents the result of subtracting two pointers pointing to elements of the same array. It is signed because the first pointer can point either after or before the second pointer. Its size matches the platform pointer width.
</details>

<details>
<summary><strong>6. Why should you avoid `int_fast8_t` for large data buffers?</strong></summary>

`int_fast8_t` chooses the integer type that the target CPU computes with most efficiently. On many 64-bit CPUs, accessing an 8-bit byte requires extra masking or byte-extraction instructions, whereas 64-bit words compute at full ALU speed. Consequently, the compiler may define `int_fast8_t` as a 64-bit `long`! If an engineer allocates an array of 1,000,000 `int_fast8_t`, it consumes 8 Megabytes of RAM instead of 1 Megabyte, thrashing CPU caches.
</details>

<details>
<summary><strong>7. What are the `INT64_C()` and `UINT32_C()` macros used for?</strong></summary>

In C, integer literals (like `1000`) default to `int` unless suffixed (`1000LL`). When writing generic macros, hardcoding suffixes (`LL` or `ULL`) can cause warnings if the target platform defines `int64_t` as `long`. The `INT64_C(x)` macro automatically appends the correct platform suffix to literal `x`, ensuring it has the exact type of `int64_t`.
</details>

<details>
<summary><strong>8. What is `intmax_t` and when should it be used?</strong></summary>

`intmax_t` is a signed integer type that can represent any value of any signed integer type supported by the implementation. It is used in generic APIs, formatters, or mathematical libraries that need to accept or return the largest possible integers without knowing their concrete types.
</details>

---

## 11. Practice Exercises

### Easy: Check Pointer Alignment
Write an inline function `bool is_aligned(const void *ptr, size_t alignment)` that returns true if `ptr` is aligned to a power-of-two boundary.

<details>
<summary>Solution</summary>

```c
#include <stdint.h>
#include <stdbool.h>

static inline bool is_aligned(const void *ptr, size_t alignment) {
    return ((uintptr_t)ptr & (alignment - 1u)) == 0;
}
```
</details>

### Medium: Portable Hex Address Dumper
Write a function `void dump_address(const void *ptr)` that prints any memory pointer as a normalized, zero-padded 64-bit hexadecimal address.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

void dump_address(const void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    printf("Memory Address: 0x%016" PRIxPTR "\n", addr);
}
```
</details>

### Hard: Safe Pointer Offset Calculation
Write a function `void *safe_pointer_advance(void *base, size_t offset, size_t limit)` that validates whether adding `offset` to `base` will wrap around the virtual address space.

<details>
<summary>Solution</summary>

```c
#include <stdint.h>
#include <stddef.h>

void *safe_pointer_advance(void *base, size_t offset, size_t limit) {
    if (!base || offset > limit) return NULL;
    uintptr_t addr = (uintptr_t)base;
    if (UINTPTR_MAX - addr < offset) {
        return NULL; /* Pointer wraparound prevented */
    }
    return (void *)(addr + offset);
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Header | Type / Macro | Purpose |
| :--- | :--- | :--- |
| `<stdint.h>` | `int8_t`, `uint32_t`, `int64_t` | Exact-width integer types |
| `<stdint.h>` | `uintptr_t`, `intptr_t` | Pointer-sized integers |
| `<stdint.h>` | `SIZE_MAX`, `INT64_MAX` | Upper limit constants |
| `<inttypes.h>` | `PRId32`, `PRIu64`, `PRIxPTR` | Portable `printf` specifiers |
| `<inttypes.h>` | `SCNd32`, `SCNu64` | Portable `scanf` specifiers |
| `<stddef.h>` | `size_t`, `ptrdiff_t` | Object sizes and pointer differences |

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17) §7.20 (Integer types `<stdint.h>`), §7.8 (Format conversion of integer types `<inttypes.h>`).
- SEI CERT C Coding Standard: INT01-C (Use char for character data and signed char / unsigned char for small integers).
- Ulrich Drepper, *64-bit Programming Models: Why LP64?* (Linux Journal).

---

[← Previous](21-signals-basics-portable.md) | [Section Index](README.md) | [Next →](23-locale-and-wide-chars-deep-dive.md)
