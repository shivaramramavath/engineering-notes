# 18 Strict Aliasing & Type Punning

> Master Type-Based Alias Analysis (TBAA), understand strict aliasing rules, avoid undefined behavior in memory reinterpretations, and safely pun types via `memcpy` and `union`.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [02-core-concepts/03-pointers.md](../02-core-concepts/03-pointers.md), [02-core-concepts/06-structs-unions-enums.md](../02-core-concepts/06-structs-unions-enums.md), [03-intermediate/16-inline-restrict-volatile.md](16-inline-restrict-volatile.md) |
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

In systems engineering, developers constantly need to view the same underlying memory bytes through different conceptual lenses:
- Inspecting the raw IEEE-754 bit fields (sign, exponent, mantissa) of a `float` or `double`.
- Serializing a multi-field C struct directly into a network transmission byte buffer.
- Fast square root or hashing tricks (such as the classic Quake III fast inverse square root algorithm).

Historically, C programmers performed this "type punning" by casting pointers:
```c
/* DANGEROUS UNDEFINED BEHAVIOR: Violates Strict Aliasing! */
float f = 5.0f;
uint32_t bits = *(uint32_t *)&f;
```
While this code appeared to work on unoptimized compilers in the 1990s, on modern optimizing compilers (GCC, Clang `-O2` / `-O3`), it triggers catastrophic **Undefined Behavior**.

The compiler operates under **Type-Based Alias Analysis (TBAA)**: it assumes that two pointers of incompatible types cannot point to the same memory address. When code violates this rule, the compiler will reorder memory reads and writes, eliminate operations as dead stores, or read stale values from registers.

Understanding strict aliasing rules and mastering the two legal, standard-compliant mechanisms for type punning (`memcpy` and `union`) is mandatory for high-performance C programming.

---

## 2. Core Concepts

### 2.1 The Strict Aliasing Rule (ISO C §6.5)
The C standard dictates that an object's stored value may only be accessed through an lvalue expression of one of the following compatible types:
1. The exact same type as the object (with or without qualifiers like `const` or `volatile`).
2. A signed or unsigned variant of the object's type (e.g. accessing `int` through `unsigned int *`).
3. An aggregate (`struct`) or `union` type that contains one of the above types among its members.
4. **The Character Pointer Exception**: A character type (`char *`, `signed char *`, `unsigned char *`, and in C23 `char8_t *`). Any object in memory can always be safely inspected as an array of raw bytes!

Any attempt to read or write an object through a pointer of any other incompatible type (e.g. reading a `float` through a `uint32_t *`, or reading a `struct Header` through a `uint64_t *`) violates strict aliasing and invokes Undefined Behavior.

### 2.2 The Two Safe Methods of Type Punning
1. **Method 1: `memcpy`**:
   ```c
   uint32_t bits;
   memcpy(&bits, &f, sizeof(bits));
   ```
   This is 100% compliant with ISO C. Modern compilers recognize `memcpy` with constant sizes and emit a single direct register `mov` instruction (zero function call overhead!).
2. **Method 2: `union` Punning**:
   ```c
   union { float f; uint32_t u; } pun;
   pun.f = 5.0f;
   uint32_t bits = pun.u;
   ```
   In C99, C11, and C23 (Annex J.1), reading from a union member other than the one last written to is explicitly defined and sanctioned by the ISO C standard (unlike in C++, where union punning remains undefined!).

---

## 3. How It Works Internally

### How TBAA Reorders Memory Operations
To understand why violating strict aliasing breaks code, examine this function:
```c
uint32_t swap_halves(uint32_t *u, uint16_t *s) {
    *u = 0x11112222;
    *s = 0x3333; /* Modifies first 2 bytes if s aliases u */
    return *u;
}
```
- Under the Strict Aliasing Rule, the compiler assumes `uint32_t *` and `uint16_t *` **cannot alias** (they are distinct types).
- Therefore, the compiler deduces that the store to `*s` cannot possibly alter the value of `*u`.
- The compiler optimizes the function into:
  ```assembly
  mov DWORD PTR [rdi], 0x11112222
  mov WORD PTR [rsi], 0x3333
  mov eax, 0x11112222              ; Constant returned directly from register!
  ret
  ```
If the caller passed `swap_halves(&val, (uint16_t *)&val)`, the return value is `0x11112222` rather than the updated in-memory value `0x11113333`. The optimization produces an erroneous result.

```
Strict Aliasing Compiler Optimization Tree:
┌──────────────────────────────────────────────┐
│ Pointer Dereference: *u = 0x11112222;        │
├──────────────────────────────────────────────┤
│ Store to Incompatible Type: *s = 0x3333;     │
├──────────────────────────────────────────────┤
│ Compiler TBAA Analysis:                      │
│ Types differ -> Pointers CANNOT point to     │
│ same memory address -> No memory dependency! │
├──────────────────────────────────────────────┤
│ Optimization Result:                         │
│ Compiler caches *u in register or reorders   │
│ stores, breaking illegal alias assumptions!  │
└──────────────────────────────────────────────┘
```

---

## 4. Syntax & Usage

### 4.1 Safe Float Bit Reinterpretation via `memcpy`
```c
#include <stdint.h>
#include <string.h>

static inline uint32_t float_as_uint32(float f) {
    uint32_t u;
    _Static_assert(sizeof(f) == sizeof(u), "Size mismatch");
    memcpy(&u, &f, sizeof(u));
    return u;
}
```
On x86_64 (`-O2`), this compiles down to a single instruction:
```assembly
movd eax, xmm0    ; Moves bits from float register directly to integer register!
ret
```

### 4.2 Safe Bit-Field Inspection via `union`
```c
typedef union {
    float f;
    uint32_t raw;
    struct {
        uint32_t mantissa : 23;
        uint32_t exponent : 8;
        uint32_t sign     : 1;
    } fields;
} FloatView;
```

---

## 5. Practical Examples

### Comprehensive Example: Dual Safe Punning Demonstration
The following program demonstrates both standard-compliant methods (`memcpy` and `union`) to safely inspect floating point IEEE-754 bit representations without strict aliasing violations.

```c
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Safe type punning using union (Standard C99/C11/C23) */
typedef union {
    float f;
    uint32_t u;
    struct {
        uint32_t mantissa : 23;
        uint32_t exponent : 8;
        uint32_t sign     : 1;
    } bits;
} FloatPunningUnion;

/* Safe type punning using memcpy (Zero overhead in modern compilers) */
static inline uint32_t float_to_bits_memcpy(float val) {
    uint32_t bits;
    memcpy(&bits, &val, sizeof(bits));
    return bits;
}

int main(void) {
    printf("=== Strict Aliasing and Safe Type Punning ===\n");

    float test_val = -12.5f;

    /* 1. Via memcpy */
    uint32_t bits_memcpy = float_to_bits_memcpy(test_val);
    printf("Float %f via memcpy bits: 0x%08X\n", test_val, (unsigned int)bits_memcpy);

    /* 2. Via Union */
    FloatPunningUnion pun;
    pun.f = test_val;
    printf("Float %f via union: Sign=%u, Exponent=0x%02X, Mantissa=0x%06X\n",
           pun.f,
           (unsigned int)pun.bits.sign,
           (unsigned int)pun.bits.exponent,
           (unsigned int)pun.bits.mantissa);

    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 18-strict-aliasing-and-type-punning.c -o 18-strict-aliasing-and-type-punning.exe
./18-strict-aliasing-and-type-punning.exe
```

```text
=== Strict Aliasing and Safe Type Punning ===
Float -12.500000 via memcpy bits: 0xC1480000
Float -12.500000 via union: Sign=1, Exponent=0x82, Mantissa=0x480000
```

---

## 6. Best Practices

1. **Prefer `memcpy` for General Type Punning**: `memcpy` is portable across both C and C++, and modern compilers optimize small fixed-size `memcpy` calls into direct register transfers.
2. **Never Cast Pointers Directly to Different Types**: Banish `*(int *)&my_float` from your codebase. It is an immediate strict aliasing violation.
3. **Use `char *` or `uint8_t *` for Byte Inspection**: When serializing structures to network sockets or disk, copy or iterate through `const unsigned char *`. The standard explicitly permits character pointers to alias any type.
4. **Enable Compiler Strict Aliasing Warnings**: Always build with `-Wall -Wstrict-aliasing=2` in GCC/Clang to detect violations at compile time.
5. **Never Use `-fno-strict-aliasing` as a Permanent Crutch**: While `-fno-strict-aliasing` tells GCC to disable TBAA optimizations, it masks bugs and prevents valuable compiler optimizations. Write standard-compliant code instead.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| `*(uint32_t *)&f` | Direct pointer cast violation of TBAA. Optimizer may reorder reads or return stale values. | Use `memcpy()` or `union`. |
| Unaligned pointer casting: `*(int *)(buffer + 1)` | In addition to strict aliasing, misaligned accesses trigger bus errors (SIGBUS) on ARM/MIPS. | Use `memcpy()` which safely handles misaligned bytes. |
| Casting struct pointer: `(Header *)packet_bytes` | Violates strict aliasing if `packet_bytes` was declared as an integer array or buffer. | Copy into local struct with `memcpy(&hdr, bytes, sizeof(hdr));`. |
| Assuming `union` punning works in C++ | C++ standard forbids reading non-active union members (it is UB in C++). | Use `std::bit_cast` in C++20 or `memcpy` in standard C/C++. |
| Modifying memory through `char *` and expecting immediate cache update | While `char *` can alias anything, excessive aliasing forces compiler to reload from memory constantly. | Use local variables and `memcpy` at boundary. |
| Forgetting `_Static_assert` on size parity | Punning types of mismatched size (e.g. `double` to `uint32_t`) reads uninitialized bytes or causes buffer overread. | Assert `sizeof(T1) == sizeof(T2)`. |

---

## 8. Performance & Security Notes

- **Zero-Cost Abstraction**: Developers often resist `memcpy` believing it causes slow function call overhead. In reality, with `-O2`, GCC and Clang completely eliminate the `memcpy` call and emit single x86 SSE/AVX instructions (`movd`, `movq`).
- **Unaligned Hardware Hazards**: On architectures like ARM Cortex-M0 or SPARC, dereferencing a casted pointer that is not aligned to a 4-byte boundary causes a hardware fault. `memcpy` handles alignment safely on all platforms.
- **Security Vulnerabilities**: When developers rely on undefined pointer casting, compiler upgrades or optimization flag changes (`-O1` to `-O2`) can suddenly reorder security checks or memory wipes, opening security exploits.

---

## 9. Comparison / When to Use What

| Technique | ISO C Status | C++ Status | Performance | Portability |
| :--- | :--- | :--- | :--- | :--- |
| **`memcpy`** | Fully Defined | Fully Defined | Optimal (Inlined by compiler) | Universal |
| **`union` Punning** | Fully Defined (Annex J) | Undefined Behavior | Optimal (Zero overhead) | C only |
| **Direct Pointer Cast `*(T2*)&v1`** | **Undefined Behavior** | **Undefined Behavior** | Dangerous (Broken by TBAA) | Non-portable |
| **`char *` Byte Iteration** | Fully Defined | Fully Defined | High | Universal |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the Strict Aliasing Rule in C?</strong></summary>

The Strict Aliasing Rule (ISO C §6.5) states that two pointers of different types cannot point to the same memory location, with specific exceptions (such as character types `char *` and signed/unsigned variants of the same type). This allows the compiler's optimizer to perform Type-Based Alias Analysis (TBAA) and assume that a write through one pointer cannot modify the value read through another pointer of a different type, enabling register caching and loop optimizations.
</details>

<details>
<summary><strong>2. Why is `memcpy` considered the most portable way to perform type punning?</strong></summary>

`memcpy()` is guaranteed by both ISO C and ISO C++ to be fully defined for all trivially copyable types. Furthermore, modern optimizing compilers have built-in recognition for `memcpy` with constant sizes: they inline the copy into a single machine register move (like `movd` or `movq`), resulting in zero runtime function call overhead while avoiding strict aliasing violations and hardware alignment faults.
</details>

<details>
<summary><strong>3. Is type punning through a `union` legal in C? How does it differ from C++?</strong></summary>

In standard C (C99, C11, C23 §6.5.2.3 and Annex J), writing to one union member and reading from another is explicitly allowed and well-defined: the bits of the active member are reinterpreted as the type of the requested member. In standard C++, however, reading from a union member other than the one most recently written to is technically Undefined Behavior.
</details>

<details>
<summary><strong>4. Why is `char *` exempt from the Strict Aliasing Rule?</strong></summary>

The C standard explicitly designates character types (`char *`, `signed char *`, `unsigned char *`) as universal byte inspection pointers. This exemption is necessary so that foundational systems operations—such as copying objects (`memcpy`), zeroing memory (`memset`), or transmitting raw bytes over network sockets—can inspect and manipulate any memory block regardless of its underlying static type.
</details>

<details>
<summary><strong>5. How does violating strict aliasing cause bugs when compiling with `-O2` but work with `-O0`?</strong></summary>

At `-O0` (no optimization), the compiler emits naive assembly that reads and writes all variables directly to and from memory on every statement. At `-O2`, the compiler activates Type-Based Alias Analysis (TBAA). Believing that two pointers of different types cannot point to the same memory address, it caches values in CPU registers across statements and reorders independent memory operations. If the pointers actually alias, the reordered code reads stale values from registers rather than the recently written memory.
</details>

<details>
<summary><strong>6. What is the difference between an alignment fault and a strict aliasing violation?</strong></summary>

Strict aliasing is a language-level semantic rule: accessing an object through an incompatible type pointer invalidates compiler optimization assumptions. An alignment fault is a hardware-level exception: many CPU architectures (e.g. ARM, SPARC) mandate that an $N$-byte integer must be stored at a memory address that is a multiple of $N$. Casting a byte buffer `(uint32_t *)(buf + 1)` both violates strict aliasing and triggers a hardware bus fault (`SIGBUS`).
</details>

<details>
<summary><strong>7. What does the GCC flag `-fno-strict-aliasing` do?</strong></summary>

The `-fno-strict-aliasing` flag tells GCC and Clang to disable Type-Based Alias Analysis optimizations. The compiler conservatively assumes that pointers of any type might alias each other. While this prevents legacy code with direct pointer-casting bugs from breaking, it reduces optimization opportunities across the entire codebase.
</details>

<details>
<summary><strong>8. How does C++20 `std::bit_cast` relate to C type punning?</strong></summary>

In C++20, `std::bit_cast` was introduced to provide an official, `constexpr`-capable mechanism for type punning, formalizing what C developers have achieved via `memcpy` and `union`. Under the hood, `std::bit_cast` performs bit reinterpretation identical to `memcpy`.
</details>

---

## 11. Practice Exercises

### Easy: Safe Fast Float Sign Flip
Write a function `float flip_sign(float f)` that inverts the sign bit of a float using bitwise XOR on its integer representation without arithmetic multiplication by `-1.0f`.

<details>
<summary>Solution</summary>

```c
#include <stdint.h>
#include <string.h>

float flip_sign(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    bits ^= (1u << 31); /* Invert sign bit */
    memcpy(&f, &bits, sizeof(f));
    return f;
}
```
</details>

### Medium: Extract IEEE-754 Exponent
Write a function `int get_float_exponent(float f)` that uses a union to extract the 8-bit biased exponent and returns the unbiased mathematical exponent (subtracting 127).

<details>
<summary>Solution</summary>

```c
#include <stdint.h>

typedef union {
    float f;
    struct {
        uint32_t mantissa : 23;
        uint32_t exponent : 8;
        uint32_t sign     : 1;
    } bits;
} FloatBits;

int get_float_exponent(float f) {
    FloatBits fb;
    fb.f = f;
    return (int)fb.bits.exponent - 127;
}
```
</details>

### Hard: Safe Big-Endian Integer Unpacking
Write a function `uint32_t unpack_be32(const uint8_t *bytes)` that reconstructs a 32-bit unsigned integer from 4 raw network bytes without violating alignment or strict aliasing rules.

<details>
<summary>Solution</summary>

```c
#include <stdint.h>

uint32_t unpack_be32(const uint8_t *bytes) {
    if (!bytes) return 0;
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8)  |
           ((uint32_t)bytes[3]);
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Approach | Safe in C? | Safe in C++? | Assembly Overhead |
| :--- | :--- | :--- | :--- |
| `memcpy(&out, &in, sz)` | **YES** (Standard) | **YES** | 0 cycles (Optimized to `movd`/`movq`) |
| `union { T1 a; T2 b; }` | **YES** (Annex J) | Undefined | 0 cycles (Direct access) |
| `*(T2 *)&t1_var` | **NO (Undefined Behavior)** | **NO** | Optimizer breaks memory order |
| `(const char *)ptr` | **YES** (Universal) | **YES** | Safe for byte copying |

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17) §6.5 (Expressions: aliasing rules).
- SEI CERT C Coding Standard: EXP39-C (Do not access a variable through a pointer of an incompatible type).
- GCC Documentation: Options That Control Optimization (`-fstrict-aliasing`, `-Wstrict-aliasing`).

---

[← Previous](17-x-macros-and-advanced-preprocessor.md) | [Section Index](README.md) | [Next →](19-integer-overflow-and-checked-arithmetic.md)
