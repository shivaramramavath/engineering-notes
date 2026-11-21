# Structure Padding, Alignment, and offsetof

> Deep architectural exploration of hardware memory alignment: bus cycles, member padding, trailing padding, offset calculations, member reordering, and packed attributes.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Structs, Unions, and Enums](./06-structs-unions-enums.md), [Pointers](./03-pointers.md), [Data Types](../01-fundamentals/02-data-types.md) |
| **Time to Read** | 30 minutes |
| **Status** | Complete |
| **Standard** | ISO C11 / C17 / C23 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison: When to Use What](#9-comparison-when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Novice programmers often assume that the size of a structure is simply the sum of the sizes of its individual members:
$$\text{sizeof}(\text{struct}) \stackrel{?}{=} \sum \text{sizeof}(m_i)$$
In modern computer architectures, this is almost never true. A structure containing a 1-byte `char`, a 4-byte `int`, and an 8-byte `double` might consume 16 or 24 bytes in memory.

This occurs because modern CPUs do not read memory byte-by-byte; they fetch memory across 32-bit, 64-bit, or 128-bit hardware data buses. Accessing a 4-byte integer from an address not divisible by 4 requires multiple memory bus transactions, cache line splits, or triggers a fatal hardware **Bus Error (`SIGBUS`)**. To ensure optimal execution speed and prevent hardware faults, compilers insert invisible **padding bytes** between members and at the end of structures.

Understanding structure alignment and padding enables systems engineers to:
- Slash the memory footprint of large datasets by up to 50% via optimal member ordering.
- Safely serialize binary data across networks and disk files.
- Prevent subtle security information leaks caused by uninitialized padding bytes.

---

## 2. Core Concepts

### Natural Alignment
Every basic data type in C has an architectural **alignment requirement**: an integer $K$ such that objects of that type must be stored at a memory address where $\text{Address} \pmod K == 0$.
- `char`, `uint8_t`: Alignment 1 (any byte address).
- `short`, `int16_t`: Alignment 2 (addresses divisible by 2).
- `int`, `uint32_t`, `float`: Alignment 4 (addresses divisible by 4).
- `double`, `uint64_t`, pointers (`void *`): Alignment 8 on 64-bit architectures (addresses divisible by 8).

### The Two Rules of Struct Layout
1. **Internal Member Padding**: Each member is placed at an offset that is an integer multiple of that member's individual alignment requirement.
2. **Trailing Padding and Total Size**: The total size of the entire structure is rounded up to an integer multiple of the **largest alignment requirement of any of its members**. This guarantees that in an array of structures, every subsequent element begins on a properly aligned boundary.

### The `offsetof` Macro (`<stddef.h>`)
Standard C provides `offsetof(type, member)` to query the exact byte offset of a member from the beginning of a struct at compile time.

---

## 3. How It Works Internally

### Memory Layout Comparison: Suboptimal vs. Optimized

```
Suboptimal Layout: Total Size = 24 Bytes (8 bytes wasted padding!)
struct Suboptimal {
    char a;       /* Offset 0, 1 byte */
                  /* Offset 1-3: 3 PADDING BYTES */
    int b;        /* Offset 4, 4 bytes */
    char c;       /* Offset 8, 1 byte */
                  /* Offset 9-15: 7 PADDING BYTES */
    double d;     /* Offset 16, 8 bytes */
};

 0        1        2        3        4        5        6        7
┌────────┬────────┬────────┬────────┬────────────────────────────────┐
│   a    │  PAD   │  PAD   │  PAD   │            b (int)             │
├────────┴────────┴────────┴────────┼────────────────────────────────┤
│   c    │  PAD   │  PAD   │  PAD   │  PAD   │  PAD   │  PAD   │  PAD  │
├───────────────────────────────────┴────────────────────────────────┤
│                           d (double)                               │
└────────────────────────────────────────────────────────────────────┘
 8        9        10       11       12       13       14       15       16 ... 23

Optimized Layout: Reordered in Descending Alignment (Total Size = 16 Bytes, 33% reduction!)
struct Optimized {
    double d;     /* Offset 0, 8 bytes */
    int b;        /* Offset 8, 4 bytes */
    char a;       /* Offset 12, 1 byte */
    char c;       /* Offset 13, 1 byte */
                  /* Offset 14-15: 2 TRAILING PADDING BYTES (to reach multiple of 8) */
};

 0                                                                   7
┌────────────────────────────────────────────────────────────────────┐
│                           d (double)                               │
├───────────────────────────────────┬────────┬────────┬────────┬─────┤
│              b (int)              │   a    │   c    │  PAD   │ PAD │
└───────────────────────────────────┴────────┴────────┴────────┴─────┘
 8        9        10       11       12       13       14       15
```

---

## 4. Syntax & Usage

```c
#include <stdio.h>
#include <stddef.h>
#include <stdalign.h> /* C11 alignas, alignof */

/* Querying alignment in C11/C23 */
size_t align_double = alignof(double);

/* Custom aligned structure (e.g., 64-byte CPU cache line alignment) */
struct alignas(64) CacheLineBlock {
    int counter;
    char buffer[60];
};

/* GCC / Clang packed attribute: strips all padding */
struct __attribute__((packed)) WirePacket {
    uint8_t  type;
    uint32_t sequence;
};
```

> [!NOTE]
> ### ISO C11 `<stdalign.h>` vs. ISO C23 Keywords
> - In C11, alignment primitives were implemented as macros defined in `<stdalign.h>` (`alignas`, `alignof`).
> - In ISO C23, `alignas` and `alignof` are first-class native keywords (matching C++11), and `<stdalign.h>` is deprecated.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates calculating member offsets with `offsetof` and comparing suboptimal vs. optimized structure layouts.

```c
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

struct Suboptimal {
    char a;       /* 1 byte + 3 pad */
    int b;        /* 4 bytes */
    char c;       /* 1 byte + 7 pad */
    double d;     /* 8 bytes */
};

struct Optimized {
    double d;     /* 8 bytes */
    int b;        /* 4 bytes */
    char a;       /* 1 byte */
    char c;       /* 1 byte + 2 pad */
};

int main(void) {
    printf("=== C Struct Padding and Alignment Demo ===\n");
    printf("Suboptimal size: %zu bytes\n", sizeof(struct Suboptimal));
    printf("  offset a: %zu\n", offsetof(struct Suboptimal, a));
    printf("  offset b: %zu\n", offsetof(struct Suboptimal, b));
    printf("  offset c: %zu\n", offsetof(struct Suboptimal, c));
    printf("  offset d: %zu\n", offsetof(struct Suboptimal, d));

    printf("Optimized size:   %zu bytes\n", sizeof(struct Optimized));
    printf("  offset d: %zu\n", offsetof(struct Optimized, d));
    printf("  offset b: %zu\n", offsetof(struct Optimized, b));
    printf("  offset a: %zu\n", offsetof(struct Optimized, a));
    printf("  offset c: %zu\n", offsetof(struct Optimized, c));

    return 0;
}
```

```text
=== C Struct Padding and Alignment Demo ===
Suboptimal size: 24 bytes
  offset a: 0
  offset b: 4
  offset c: 8
  offset d: 16
Optimized size:   16 bytes
  offset d: 0
  offset b: 8
  offset a: 12
  offset c: 13
```

---

## 6. Best Practices

1. **Order Struct Members in Descending Order of Alignment**: Place 8-byte members first (pointers, `double`, `uint64_t`), then 4-byte integers, 2-byte shorts, and 1-byte chars at the end. This automatically eliminates internal padding.
2. **Never Compare Structs with `memcmp()`**: Padding bytes contain uninitialized garbage values. Comparing two structs containing identical fields using `memcmp` will frequently return non-zero (false negative).
3. **Clear Structs with `memset(&s, 0, sizeof s)` Before Transmitting**: Zeroing the entire struct prevents kernel/stack memory leaks via uninitialized padding bytes when serializing data.
4. **Use `#pragma pack(1)` or `__attribute__((packed))` with Extreme Caution**: Packing eliminates padding but forces unaligned memory accesses, causing severe performance penalties or fatal `SIGBUS` faults on non-x86 architectures (ARM, MIPS, SPARC).
5. **Always Use `sizeof(struct S)` Instead of Sum of Members**: Never manually compute buffer sizes for `malloc` or file writes by adding member sizes.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Comparing structs with `memcmp()`** | Garbage values in padding bytes cause false mismatch errors. | Write dedicated field-by-field comparison functions. |
| **Assuming `sizeof(struct)` is sum of members** | Ignores compiler padding bytes, allocating insufficient memory. | Always use `sizeof(struct Name)`. |
| **Dereferencing unaligned packed pointers** | Accessing `uint32_t *p = &packed.unaligned_int` causes CPU `SIGBUS` on ARM/RISC. | Copy data via `memcpy()` into an aligned local variable first. |
| **Transmitting raw structs across network sockets** | Different compilers/OS/architectures use different padding and endianness rules. | Serialize fields individually into a packed byte stream. |
| **Ignoring trailing padding in arrays** | Struct size is padded to a multiple of largest member alignment. | Account for trailing padding in memory calculations. |
| **Padding byte information leakage** | Uninitialized padding bytes sent over network leak sensitive memory. | Zero struct with `memset(&s, 0, sizeof s)` at initialization. |

---

## 8. Performance & Security Notes

### Cache Line Utilization
In high-throughput databases and game physics engines, reordering struct fields from 24 bytes to 16 bytes allows an array of elements to pack 4 instances per 64-byte L1 cache line instead of 2.6. This yields an immediate $30\text{--}40\%$ speedup in iteration algorithms due to reduced memory bus bandwidth saturation.

### Security: Heartbleed-Style Information Leakage via Padding
When operating systems copy kernel structures to user-space via system calls (or when network daemons serialize structs directly to sockets), uninitialized padding bytes between members contain residual data from previous stack frames or kernel memory. An attacker inspecting the raw bytes can recover encryption keys, passwords, or memory pointers that defeat Address Space Layout Randomization (ASLR).

---

## 9. Comparison: When to Use What

| Technique | Memory Footprint | Alignment Overhead | Safety | Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Natural Struct** | Padded | Zero penalty | Maximum | General application data, fast in-memory computing |
| **Packed Struct (`__attribute__((packed))`)** | Minimal | Hardware penalty on unaligned read | Dangerous (Unsafe pointers) | Hardware EEPROM parsing, fixed legacy binary formats |
| **Explicit Bitwise Serialization** | Minimal | Zero hardware penalty | Maximum (Portable) | Production network protocols, file formats |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does the compiler add padding bytes at the very end of a structure?</strong></summary>

Trailing padding ensures that when structures are allocated in a contiguous array (`struct S arr[10]`), every subsequent element `arr[1]`, `arr[2]`, etc., starts on a memory boundary that satisfies the structure's overall alignment requirement.
</details>

<details>
<summary><strong>2. What is <code>max_align_t</code> in `<stddef.h>`?</strong></summary>

`max_align_t` is a standard type whose alignment is at least as great as that of any scalar type supported by the implementation. Dynamic memory allocators (`malloc()`) are guaranteed to return pointers aligned to at least `alignof(max_align_t)` (typically 16 bytes on modern 64-bit systems).
</details>

<details>
<summary><strong>3. What happens if you take the address of an unaligned member in a packed struct?</strong></summary>

Taking the address yields an unaligned pointer (`int *p = &packed_s.unaligned_member;`). On architectures that forbid unaligned memory access (such as certain ARM, SPARC, or MIPS processors), dereferencing `*p` triggers an unaligned access hardware fault (`SIGBUS`), instantly terminating the program. GCC emits `-Waddress-of-packed-member` warnings for this pattern.
</details>

<details>
<summary><strong>4. Why is comparing structs with <code>memcmp(&amp;a, &amp;b, sizeof a)</code> dangerous?</strong></summary>

C makes no guarantees about the contents of padding bytes inserted between struct members. Even if all named fields of `a` and `b` are identical, their padding bytes may hold differing residual garbage data, causing `memcmp` to return non-zero.
</details>

<details>
<summary><strong>5. How does the <code>offsetof</code> macro work under the hood?</strong></summary>

Standard library implementations typically define `offsetof` by simulating a structure located at address 0 and casting the address of the member to `size_t`:
`#define offsetof(type, member) ((size_t)&(((type *)0)->member))`
Because this operation occurs entirely during compile-time constant evaluation, address 0 is never actually dereferenced at runtime.
</details>

<details>
<summary><strong>6. Can a struct have an alignment greater than the alignment of its largest member?</strong></summary>

Yes. Using C11 `alignas(N)` (or C23 `alignas`), a developer can explicitly request an alignment greater than natural alignment (e.g., aligning a buffer to 64 bytes for cache line optimization or AVX-512 SIMD vectorization).
</details>

<details>
<summary><strong>7. How can you find the amount of wasted padding in a structure?</strong></summary>

You can calculate wasted padding by subtracting the sum of `sizeof(member)` for all individual members from the total `sizeof(struct)`. Developer tools like `pahole` (Poke-a-Hole) automate this analysis on compiled ELF binaries.
</details>

<details>
<summary><strong>8. Does the order of members in a struct affect its sizeof?</strong></summary>

Yes, dramatically. Grouping small members together allows them to share alignment slots, whereas alternating small and large members introduces padding between every field, potentially doubling or tripling the struct's size.
</details>

---

## 11. Practice Exercises

### Exercise 1: Struct Packing Optimizer (Easy)
Given the following struct, calculate its size on a 64-bit architecture, then reorder its members to achieve the smallest possible size:
```c
struct Unoptimized {
    char a;
    double b;
    short c;
    int d;
    char e;
};
```

<details>
<summary>View Solution</summary>

Original layout:
- `a`: 1 byte + 7 pad = 8
- `b`: 8 bytes
- `c`: 2 bytes + 2 pad = 4
- `d`: 4 bytes
- `e`: 1 byte + 7 pad = 8
Total: 32 bytes.

Optimized layout (descending size order):
```c
struct Reordered {
    double b; /* 8 bytes */
    int d;    /* 4 bytes */
    short c;  /* 2 bytes */
    char a;   /* 1 byte */
    char e;   /* 1 byte */
};            /* Total: 16 bytes (50% reduction!) */
```
</details>

### Exercise 2: Padding Inspector Utility (Medium)
Write a macro or helper function that prints the offset and size of every member in a struct along with the detected padding bytes between adjacent members.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stddef.h>

struct Test {
    char c;
    int i;
    double d;
};

int main(void) {
    size_t off_c = offsetof(struct Test, c);
    size_t off_i = offsetof(struct Test, i);
    size_t off_d = offsetof(struct Test, d);

    printf("Member c: offset %zu, size %zu\n", off_c, sizeof(char));
    printf("Padding after c: %zu bytes\n", off_i - (off_c + sizeof(char)));
    printf("Member i: offset %zu, size %zu\n", off_i, sizeof(int));
    printf("Padding after i: %zu bytes\n", off_d - (off_i + sizeof(int)));
    printf("Member d: offset %zu, size %zu\n", off_d, sizeof(double));
    printf("Trailing padding: %zu bytes\n", sizeof(struct Test) - (off_d + sizeof(double)));
    return 0;
}
```
</details>

### Exercise 3: Safe Unaligned Little-Endian Buffer Reader (Hard)
Write robust functions `uint16_t read_u16_le(const uint8_t *buf)` and `uint32_t read_u32_le(const uint8_t *buf)` that safely extract multi-byte integers from arbitrary unaligned byte pointers without triggering `SIGBUS` faults on any CPU.

<details>
<summary>View Solution</summary>

```c
#include <stdint.h>
#include <string.h>

/* Using bitwise shifts: 100% portable, works regardless of host alignment or endianness */
uint16_t read_u16_le(const uint8_t *buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

uint32_t read_u32_le(const uint8_t *buf) {
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Standard offset inspection */
#include <stddef.h>
size_t offset = offsetof(struct MyStruct, member_name);

/* C11 / C23 Alignment */
size_t align = alignof(type);
struct alignas(32) AlignedBlock { ... };

/* Reordering rule */
Order fields: 8-byte -> 4-byte -> 2-byte -> 1-byte
Eliminates internal padding automatically.

/* Safety rules */
Never use memcmp() on structs (garbage in padding bytes).
Zero memory with memset() before serialization to prevent leaks.
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §6.2.8 "Alignment of objects" & §6.7.2.1 "Structure and union specifiers".
- ISO/IEC 9899:2024 (C23 Standard), §6.2.8 "Alignment of objects".
- *The Lost Art of Structure Packing* by Eric S. Raymond.
- Ulrich Drepper: *What Every Programmer Should Know About Memory* (Cache-conscious data layout).

---

[← Previous: Designated Initializers & Compound Literals](./18-designated-initializers-and-compound-literals.md) | [Section Index](./README.md) | [Next: Process Memory Layout →](./20-memory-layout-stack-heap-data-bss.md)
