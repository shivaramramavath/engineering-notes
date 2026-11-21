# Sizeof Operator and Memory Alignment Basics

> A comprehensive deep dive into memory alignment architecture: the `sizeof` operator, CPU hardware alignment constraints, struct member padding, member reordering optimization, C11 `<stdalign.h>` (`alignof`, `alignas`), and `max_align_t`.

| Metadata | Details |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [02-data-types.md](./02-data-types.md), [03-integer-representation.md](./03-integer-representation.md) |
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

In C, the size of a structure is rarely equal to the mathematical sum of the sizes of its individual members. Memory is not a continuous, unconstrained byte array; it is accessed across hardware memory buses in 32-bit, 64-bit, or 128-bit chunks.

Failing to understand memory alignment causes critical software problems:
1. **Silent Memory Bloat**: Poorly ordered struct members introduce invisible compiler padding bytes, inflating data structure sizes by 30% to 50% across large arrays and wasting valuable CPU L1/L2 cache space.
2. **Hardware Faults (`SIGBUS`)**: On many CPU architectures (SPARC, MIPS, older ARM), attempting to read a 4-byte integer from an odd memory address triggers an immediate unrecoverable hardware exception (`SIGBUS`).
3. **Binary Serialization Desynchronization**: Sending raw C structures over a network socket or writing them to disk leads to data corruption if the sending and receiving systems have differing padding alignments.

---

## 2. Core Concepts

### The `sizeof` Operator

The `sizeof` operator yields the size (in bytes) of its operand:
- **Compile-Time Constant**: For all standard types and fixed-size arrays, `sizeof` is evaluated completely at compile time; the expression inside `sizeof(expr)` is **never evaluated at runtime**!
  ```c
  int i = 0;
  sizeof(i++); /* i++ is NEVER executed; i remains 0! */
  ```
- **The VLA Exception**: If the operand is a Variable Length Array (VLA, C99+), `sizeof` must evaluate the array dimension at runtime.
- **Return Type**: The result of `sizeof` is an unsigned integer type of implementation-defined width named `size_t` (defined in `<stddef.h>`), formatted with `%zu`.

### Memory Alignment Principles

An object of a given type cannot be stored at an arbitrary memory address:
- An $N$-byte data type typically requires an address divisible by $N$:
  - `char` (1 byte): Can reside at any memory address ($\text{alignment} = 1$).
  - `int16_t` (2 bytes): Address must be a multiple of 2 ($\text{addr} \pmod 2 == 0$).
  - `int32_t` (4 bytes): Address must be a multiple of 4 ($\text{addr} \pmod 4 == 0$).
  - `double` / `int64_t` (8 bytes): Address must be a multiple of 8 ($\text{addr} \pmod 8 == 0$).

### Modern Standard Alignment (`<stdalign.h>`)

Standardized in C11 (and native keywords in C23):
- `alignof(type)`: Queries the alignment requirement of a type.
- `alignas(N)`: Requests specific custom alignment on an object or struct declaration.
- `max_align_t`: A standard type whose alignment requirement is at least as strict as that of any scalar type supported by the platform (typically 8 or 16 bytes).

---

## 3. How It Works Internally

### Struct Padding and Alignment Calculation

The compiler enforces two strict layout rules when constructing structures:
1. **Member Alignment**: Every member must be placed at an offset from the struct start that is a multiple of that member's individual alignment requirement.
2. **Total Size Alignment**: The total size of the structure must be a multiple of the **largest alignment requirement** among all its members (to ensure that in an array of structures, every subsequent element begins at a properly aligned address).

```
Unoptimized Struct Layout:
struct Unoptimized {
    char a;      /* Offset 0: 1 byte */
                 /* Offset 1-3: 3 PADDING BYTES */
    int b;       /* Offset 4: 4 bytes (must align to 4) */
    char c;      /* Offset 8: 1 byte */
                 /* Offset 9-11: 3 TRAILING PADDING BYTES (struct must align to 4) */
};
Total Size: 12 bytes! (Only 6 bytes of actual data, 6 bytes of wasted padding!)

Optimized Struct Layout (Ordered by Decreasing Alignment):
struct Optimized {
    int b;       /* Offset 0: 4 bytes */
    char a;      /* Offset 4: 1 byte */
    char c;      /* Offset 5: 1 byte */
                 /* Offset 6-7: 2 TRAILING PADDING BYTES */
};
Total Size: 8 bytes! (Saved 33% memory footprint with zero code changes!)
```

```
Memory Visualizer:
Unoptimized [ a ][ P ][ P ][ P ][   b   ][   b   ][   b   ][   b   ][ c ][ P ][ P ][ P ] = 12 B
Optimized   [   b   ][   b   ][   b   ][   b   ][ a ][ c ][ P ][ P ]                         =  8 B
```

---

## 4. Syntax & Usage

### 1. Querying Alignments with `<stdalign.h>`
```c
#include <stdio.h>
#include <stdalign.h>
#include <stddef.h>

void print_alignments(void) {
    printf("char alignment:    %zu\n", alignof(char));
    printf("int alignment:     %zu\n", alignof(int));
    printf("double alignment:  %zu\n", alignof(double));
    printf("max_align_t align: %zu\n", alignof(max_align_t));
}
```

### 2. Custom Alignment with `alignas` (e.g. Cache-Line or SIMD Alignment)
```c
#include <stdalign.h>

/* Align audio buffer to 64-byte boundary to match CPU cache lines */
alignas(64) float audio_cache_buffer[1024];

/* Align custom vector to 32-byte boundary for AVX instructions */
struct alignas(32) Vector256 {
    float values[8];
};
```

---

## 5. Practical Examples

### Sizeof and Memory Alignment Verification Program

```c
/* File: 01-languages/c/01-fundamentals/examples/11-sizeof-and-alignment-basics.c */
#include <stdio.h>
#include <stddef.h>
#include <stdalign.h>

struct Unoptimized {
    char a;      /* 1 byte */
                 /* 3 padding bytes */
    int b;       /* 4 bytes */
    char c;      /* 1 byte */
                 /* 3 padding bytes */
};

struct Optimized {
    int b;       /* 4 bytes */
    char a;      /* 1 byte */
    char c;      /* 1 byte */
                 /* 2 padding bytes */
};

int main(void) {
    printf("=== Sizeof & Memory Alignment Verification ===\n");
    printf("sizeof(struct Unoptimized): %zu bytes (Alignment: %zu)\n", 
           sizeof(struct Unoptimized), alignof(struct Unoptimized));
    printf("sizeof(struct Optimized):   %zu bytes (Alignment: %zu)\n", 
           sizeof(struct Optimized), alignof(struct Optimized));
    printf("alignof(max_align_t):       %zu bytes\n", alignof(max_align_t));
    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 11-sizeof-and-alignment-basics.c -o run_11.exe
./run_11.exe
```
Output:
```text
=== Sizeof & Memory Alignment Verification ===
sizeof(struct Unoptimized): 12 bytes (Alignment: 4)
sizeof(struct Optimized):   8 bytes (Alignment: 4)
alignof(max_align_t):       8 bytes
```

---

## 6. Best Practices

1. **Order Struct Members by Decreasing Alignment Requirement**: Place 8-byte members first (`double`, pointers, `uint64_t`), followed by 4-byte members (`int`), 2-byte members (`short`), and 1-byte members (`char`). This automatically minimizes padding without compiler-specific hacks.
2. **Never Write Expressions with Side Effects Inside `sizeof`**: Remember that `sizeof(i++)` does not increment `i`. Keep `sizeof` operands clean of side effects.
3. **Do Not Blindly Use `#pragma pack(1)`**: Packing structs removes padding but forces the CPU to generate slow, multi-instruction unaligned read sequences, and triggers fatal `SIGBUS` faults on non-x86 processors.
4. **Use `offsetof` from `<stddef.h>` to Inspect Offsets**: Query member byte positions reliably rather than computing manual pointer differences.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Computing struct size by adding member sizes | Ignores compiler alignment padding bytes. | Always use `sizeof(struct MyStruct)`. |
| Writing side effects inside `sizeof(a++)` | `sizeof` operand is not evaluated at runtime; `a` remains unmodified. | Execute increment on a separate statement. |
| Using `sizeof` on pointer expecting array size | `sizeof(ptr)` returns pointer size (4 or 8 bytes), NOT array element count! | Pass array length as a dedicated parameter. |
| Transmitting packed structs across networks directly | Binary representation differs across architectures due to endianness and word sizes. | Serialize fields individually with explicit byte encoding. |
| Dereferencing misaligned pointers | `int *p = (int *)(char_buf + 1); *p = 5;` invokes undefined behavior and triggers `SIGBUS` on ARM. | Use `memcpy(&val, char_buf + 1, sizeof(int))`. |
| Overlooking trailing struct padding | In `struct { int x; char c; }`, trailing padding is added so arrays of the struct align properly. | Account for trailing padding when calculating memory consumption. |

---

## 8. Performance & Security Notes

- **Cache Line Splitting Penalty**: When an unaligned 8-byte integer straddles the boundary between two 64-byte L1 CPU cache lines, the memory controller must perform **two separate cache line fetches** and merge the bytes, degrading memory throughput by up to 300%.
- **Padding Information Leaks**: When structures are written to disk or sent over network sockets, the uninitialized padding bytes between members leak previous stack/register values. Always zero the entire struct using `memset(&s, 0, sizeof(s))` before transmission.

---

### Vectorization and SIMD Alignment Requirements

Modern vector instructions (such as AVX-512 and ARM Neon) operate at peak performance when memory operands are aligned to 32-byte or 64-byte boundaries. Loading unaligned memory into vector registers requires slower instructions (e.g. movdqu rather than movdqa). Ensuring aligned struct arrays allows compilers to auto-vectorize loops without generating fallback scalar peeling loops.

---

## 9. Comparison / When to Use What

| Technique | Portability | Memory Footprint | Access Speed |
|---|---|---|---|
| **Member Ordering (Natural Alignment)** | 100% (ISO Conforming) | Minimal (Optimized) | Maximum (Single-cycle load) |
| **Compiler Packing (`#pragma pack(1)`)**| Non-standard / Compiler extension | Absolute minimum (Zero padding) | Slow (Unaligned loads, potential `SIGBUS`) |
| **Explicit Alignment (`alignas(64)`)** | Standard (C11 `<stdalign.h>`, C23) | Larger (Explicit padding) | Maximum (SIMD / Cache-line aligned) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `sizeof(struct { char a; int b; char c; })` typically 12 bytes instead of 6 bytes?</strong></summary>

Because of hardware alignment requirements. The `int` member `b` requires 4-byte alignment, forcing 3 padding bytes after `char a` (offsets 1-3). After `char c` (offset 8), the compiler must add 3 trailing padding bytes (offsets 9-11) so that the total structure size (12 bytes) is a multiple of the structure's largest alignment requirement (4 bytes).
</details>

<details>
<summary><strong>2. Is the operand of the `sizeof` operator ever evaluated at runtime?</strong></summary>

Yes, exactly in one scenario: when the operand is a **Variable Length Array (VLA)** introduced in C99. For all standard types, pointers, expressions, and fixed-size arrays, `sizeof` is evaluated strictly at compile time, and its operand is not evaluated.
</details>

<details>
<summary><strong>3. What is `max_align_t`, and where is it defined?</strong></summary>

`max_align_t` is defined in `<stddef.h>`. It is a scalar type whose alignment requirement is at least as great as that of every scalar type supported by the implementation. The memory returned by `malloc()` is guaranteed to be aligned suitably for `max_align_t` (typically 8 bytes on 32-bit systems and 16 bytes on 64-bit systems).
</details>

<details>
<summary><strong>4. What does the macro `offsetof(type, member)` do, and how does it work?</strong></summary>

`offsetof` (defined in `<stddef.h>`) returns the byte offset of a given member from the beginning of its structure. Conceptually, it evaluates the address of the member within a hypothetical struct located at address zero: `(size_t)&(((type *)0)->member)`.
</details>

<details>
<summary><strong>5. What is the difference between `alignof` and `sizeof`?</strong></summary>

`sizeof` returns the total number of bytes occupied by an object or type in memory. `alignof` returns the address boundary constraint (a positive power of two) to which memory addresses of that type must be aligned.
</details>

<details>
<summary><strong>6. Why should you avoid `#pragma pack(1)` in production systems code?</strong></summary>

Packing structs removes alignment padding, causing members to reside at arbitrary, unaligned memory addresses. On x86, unaligned memory accesses incur performance penalties and break atomic operations. On strict alignment architectures (SPARC, older ARM, MIPS), unaligned accesses trigger immediate CPU hardware exceptions (`SIGBUS`) that crash the process.
</details>

<details>
<summary><strong>7. How does member reordering optimize structure size?</strong></summary>

By arranging structure members in order of decreasing alignment requirements (e.g. 8-byte members, then 4-byte, 2-byte, 1-byte), each member naturally falls at an offset that satisfies its alignment constraint without the compiler needing to inject padding bytes between members.
</details>

<details>
<summary><strong>8. What is the value of `sizeof(char)` in standard C?</strong></summary>

`sizeof(char)` is **1 by definition** in all ISO C standards. It is the fundamental unit of measurement for `sizeof`.
</details>

---

<details>
<summary><strong>9. What is "false sharing" in multi-threaded systems, and how does alignment solve it?</strong></summary>

False sharing occurs when two independent threads running on separate CPU cores read and write unrelated variables that happen to reside on the same 64-byte L1 CPU cache line. Even though the variables are distinct, the hardware cache coherency protocol invalidates the entire cache line across cores on every write, causing massive CPU bus stalls. Aligning thread-specific variables to separate 64-byte boundaries using `alignas(64)` eliminates false sharing entirely.
</details>

<details>
<summary><strong>10. How do bitfields interact with struct alignment?</strong></summary>

Bitfields do not have individual byte addresses; they are packed into an underlying storage unit (typically `unsigned int`). The alignment of the enclosing structure is determined by the underlying type of the bitfield. The compiler cannot take the address of a bitfield member (`&field` is a compile error) because addresses must resolve to whole byte boundaries.
</details>

<details>
<summary><strong>11. Why does `sizeof` on a function type fail in standard C?</strong></summary>

Under ISO C, `sizeof` cannot be applied to function types or incomplete types (such as `void` or forward-declared structs). While GCC provides an extension treating `sizeof(void)` and function `sizeof` as 1, conforming standard code must never apply `sizeof` to functions.
</details>

<details>
<summary><strong>12. What is the difference between `aligned_alloc` and `posix_memalign`?</strong></summary>

`aligned_alloc(alignment, size)` is standardized in ISO C11 `<stdlib.h>` and requires `size` to be a multiple of `alignment`. `posix_memalign(&memptr, alignment, size)` is a POSIX-specific API that allocates aligned memory without requiring `size` to be an exact multiple of `alignment`, but is not available on non-POSIX targets like native Windows MSVC.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Computing Array Element Count</strong></summary>

**Goal**: Write a macro `ARRAY_COUNT(arr)` that safely computes the number of elements in a fixed-size array at compile time.

**Solution**:
```c
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
```
*(Note: Only works on real arrays, not on array arguments that have decayed into pointers!)*
</details>

<details>
<summary><strong>Exercise 2 (Medium): Optimal Struct Reordering</strong></summary>

**Goal**: Reorder the members of this struct to minimize its memory footprint:
```c
struct SensorData {
    char active;       /* 1 byte */
    double timestamp;  /* 8 bytes */
    short id;          /* 2 bytes */
    float reading;     /* 4 bytes */
    char error_code;   /* 1 byte */
};
```

**Solution**:
Order by descending alignment requirements (8, 4, 2, 1, 1):
```c
struct SensorData {
    double timestamp;  /* 8 bytes (Offset 0) */
    float reading;     /* 4 bytes (Offset 8) */
    short id;          /* 2 bytes (Offset 12) */
    char active;       /* 1 byte  (Offset 14) */
    char error_code;   /* 1 byte  (Offset 15) */
};                     /* Total size: 16 bytes! (Originally 24 bytes) */
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Cache-Line-Aligned Buffer Allocator</strong></summary>

**Goal**: Using C11 `aligned_alloc()`, allocate a 4096-byte memory buffer aligned to a 64-byte CPU cache-line boundary, validating preconditions according to the ISO standard.

**Solution**:
```c
#include <stdio.h>
#include <stdlib.h>

void *allocate_cache_aligned_buffer(size_t size) {
    size_t alignment = 64;
    
    /* ISO C11 requires size to be an integral multiple of alignment */
    if (size % alignment != 0) {
        size = ((size + alignment - 1) / alignment) * alignment;
    }
    
    void *ptr = aligned_alloc(alignment, size);
    return ptr;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- `sizeof(char) == 1` always; result type is `size_t` (`%zu`).
- `sizeof` expressions are compile-time constants (except for VLAs).
- Order struct members largest to smallest to eliminate padding.
- `alignof(T)` queries alignment constraint (C11 `<stdalign.h>`, C23 keyword).
- Memory from `malloc()` is aligned to at least `alignof(max_align_t)`.
- Never pack structs (`#pragma pack`) unless strictly interfacing with hardware protocols.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 6.5.3.4: "The sizeof and alignof operators".
- Eric S. Raymond: *The Lost Art of Structure Packing*.
- Ulrich Drepper: *What Every Programmer Should Know About Memory*.

---

[← Previous: 10-undefined-unspecified-implementation-defined.md](./10-undefined-unspecified-implementation-defined.md) | [Section Index](./README.md) | [Next: 12-const-correctness-basics.md](./12-const-correctness-basics.md) →
