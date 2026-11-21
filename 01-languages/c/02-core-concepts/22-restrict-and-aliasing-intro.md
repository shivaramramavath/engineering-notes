# Pointer Aliasing and the restrict Qualifier

> Demystifying pointer aliasing in C: the Strict Aliasing Rule, the char pointer exception, the restrict contract, compiler optimization boundaries, and SIMD vectorization.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Pointers](./03-pointers.md), [Type Qualifiers](./05-type-qualifiers.md), [Pointer Arithmetic](./12-pointer-arithmetic.md) |
| **Time to Read** | 25 minutes |
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

In C, two distinct pointers can point to the exact same physical memory address. When this occurs, the pointers are said to **alias** each other.

Pointer aliasing poses a major hurdle for optimizing compilers. Consider this simple function:
```c
void add_val(int *a, int *b) {
    *a += *b;
    *a += *b;
}
```
A programmer might expect the compiler to optimize this to `*a += 2 * (*b)` by reading `*b` into a CPU register once. However, what happens if `a == b`? Writing `*a` changes the value of `*b`! Because the compiler must assume `a` and `b` might alias, it is forced to emit two separate, unoptimized memory bus reads.

To solve this, ISO C establishes two critical rules:
1. **The Strict Aliasing Rule**: The compiler assumes that pointers of incompatible types never point to the same memory location.
2. **The `restrict` Qualifier (C99)**: The programmer explicitly promises to the compiler that a qualified pointer is the exclusive gateway to its referenced object, enabling loop unrolling and SIMD auto-vectorization.

---

## 2. Core Concepts

### The Strict Aliasing Rule (§6.5)
Under ISO C, an object's stored value may only be accessed through an lvalue expression of:
- A type compatible with the effective type of the object.
- A qualified version of a compatible type (e.g., `const int *` reading an `int`).
- A signed or unsigned variant of the compatible type.
- An aggregate or union type that contains one of the aforementioned types among its members.
- **A character type (`char *`, `signed char *`, `unsigned char *`, `uint8_t *`)**.

Accessing an `int` through a `float *` violates the strict aliasing rule and triggers **immediate Undefined Behavior**.

### The Character Pointer Aliasing Exception
The standard explicitly carves out an exception for character types: **any data object in C may be inspected byte-by-byte through a `char *` or `unsigned char *` pointer**. This exception allows standard library functions like `memcpy()`, `memset()`, and custom memory allocators to operate legally on arbitrary memory blocks.

### The `restrict` Promise
The `restrict` keyword applies exclusively to object pointers. It is an enforceable promise from developer to compiler: **during the lifetime of this pointer, no other pointer in the program will access the same memory object**. If the caller violates this contract by passing overlapping memory, the behavior is undefined.

---

## 3. How It Works Internally

### Assembly Transformation: With vs. Without `restrict`

```
Vector Addition Loop: void add(int *out, int *a, int *b, int n)

Without restrict:
  Compiler must assume 'out' may overlap with 'a' or 'b'.
  Must execute sequentially element-by-element (Scalar execution).
  Every write to out[i] requires reloading a[i+1] from memory.

With restrict: void add(int * restrict out, const int * restrict a, const int * restrict b, int n)
  Compiler proves 'out', 'a', and 'b' are completely disjoint.
  Enables 256-bit AVX2 SIMD Vectorization:
  ┌────────────────────────────────────────────────────────────┐
  │ vmovdqu ymm0, [rsi + rax]    ; Load 8 ints from a          │
  │ vpaddd  ymm0, ymm0, [rdx+rax]; Add 8 ints from b in 1 cycle│
  │ vmovdqu [rdi + rax], ymm0    ; Store 8 ints to out         │
  └────────────────────────────────────────────────────────────┘
  Throughput increases by 4x to 8x!
```

---

## 4. Syntax & Usage

```c
#include <stdio.h>

/* Restrict function parameters */
void vector_add(size_t n, float * restrict c, const float * restrict a, const float * restrict b);

/* Standard library prototypes demonstrating restrict */
void *memcpy(void * restrict dest, const void * restrict src, size_t n); /* Overlap FORBIDDEN */
void *memmove(void *dest, const void *src, size_t n);                    /* Overlap ALLOWED */
```

> [!NOTE]
> ### GCC Flag `-fstrict-aliasing` vs. `-fno-strict-aliasing`
> By default at `-O2` and `-O3`, GCC and Clang enable strict aliasing optimizations (`-fstrict-aliasing`). Legacy codebases that perform illegal type-punning through pointer casts (e.g., `*(float *)&my_int`) are broken by optimizing compilers. The Linux kernel explicitly compiles with `-fno-strict-aliasing` to prevent compiler optimizations from breaking hardware low-level casts.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates pointer non-aliasing accumulation and the standard character pointer aliasing exception for memory inspection.

```c
#include <stdio.h>

/* Restrict indicates disjoint memory blocks */
static void vector_accumulate(int * restrict sum, const int * restrict vals, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        *sum += vals[i];
    }
}

int main(void) {
    printf("=== C Restrict and Aliasing Demo ===\n");

    int numbers[4] = {10, 20, 30, 40};
    int total = 0;

    vector_accumulate(&total, numbers, 4);
    printf("Vector accumulate result: total=%d\n", total);

    /* Strict aliasing char* exception: inspect bytes of integer */
    unsigned int word = 0x12345678;
    unsigned char *byte_ptr = (unsigned char *)&word;
    printf("Low byte of 0x12345678 in memory: 0x%02X (Endianness check)\n", byte_ptr[0]);

    return 0;
}
```

```text
=== C Restrict and Aliasing Demo ===
Vector accumulate result: total=100
Low byte of 0x12345678 in memory: 0x78 (Endianness check)
```

### Real-World Example 2: Matrix Multiplier Inner Loop Vectorization with Restrict
In scientific matrix computations (BLAS), qualifying row and column vectors with `restrict` guarantees that output accumulation buffers never overlap with operand matrices:

```c
#include <stddef.h>

/* Restrict on all three matrices guarantees compiler can emit unrolled AVX2 FMA instructions */
void matmul_kernel_4x4(float * restrict c,
                       const float * restrict a,
                       const float * restrict b,
                       size_t lda, size_t ldb, size_t ldc) {
    for (size_t i = 0; i < 4; ++i) {
        for (size_t k = 0; k < 4; ++k) {
            float a_ik = a[i * lda + k];
            for (size_t j = 0; j < 4; ++j) {
                c[i * ldc + j] += a_ik * b[k * ldb + j];
            }
        }
    }
}
```

### Real-World Example 3: ISO-Compliant Type Punning via Union
Extracting exponent and mantissa fields from a 32-bit float without violating strict aliasing:

```c
#include <stdint.h>
#include <stdio.h>

typedef union FloatBits {
    float f;
    struct {
        uint32_t mantissa : 23;
        uint32_t exponent : 8;
        uint32_t sign     : 1;
    } parts;
} FloatBits;

void inspect_float(float val) {
    FloatBits fb = { .f = val }; /* Standard C type punning */
    printf("Float: %f -> Sign: %u, Exponent: 0x%02X, Mantissa: 0x%06X\n",
           val, fb.parts.sign, fb.parts.exponent, fb.parts.mantissa);
}
```


---

## 6. Best Practices

1. **Use `memcpy()` or Unions for Type Punning**: Never cast pointers between incompatible types (`*(float *)&my_int`). In ISO C, type-punning through `union` or `memcpy(&flt, &int_val, sizeof flt)` is fully standards-compliant and optimizes to zero instructions.
2. **Apply `restrict` to Hot Mathematical Loops**: When writing matrix, DSP, or image algorithms where input and output buffers are guaranteed distinct, mark pointers with `restrict` to unlock SIMD vectorization.
3. **Never Pass Overlapping Memory to `restrict` Parameters**: Calling `memcpy(buf + 2, buf, 10)` invokes undefined behavior because `memcpy` uses `restrict`. Use `memmove()` for overlapping regions.
4. **Compile with `-Wstrict-aliasing=2`**: Catches subtle violations of the strict aliasing rule during compilation.
5. **Remember the Character Exception**: Only `char *`, `signed char *`, `unsigned char *`, and `uint8_t *` may legally alias any type. Pointers to `int` or `void *` do not enjoy this exemption.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Type punning via pointer cast** | `*(float *)&int_var` violates strict aliasing; optimizer removes reads or reorders writes. | Use `memcpy(&flt, &int_var, sizeof flt)` or a `union`. |
| **Calling `memcpy` on overlapping buffers** | `memcpy` parameters are qualified with `restrict`; causes corrupted memory copies. | Use `memmove()` which handles overlapping memory. |
| **Passing aliased pointers to `restrict` function** | Violates developer's promise to compiler; produces corrupted vectorized results. | Guarantee buffers are distinct or remove `restrict`. |
| **Assuming `void *` can alias anything** | In ISO C, only character types can alias; `void *` must be cast to an actual compatible type. | Cast to `uint8_t *` or `unsigned char *` for raw byte access. |
| **Reordering reads/writes across aliased pointers** | Failing to realize pointers alias causes subtle race conditions and stale values. | Mark variables `volatile` or restructure data flow. |
| **Compiling legacy code without `-fno-strict-aliasing`** | Modern GCC `-O3` aggressively exploits strict aliasing, breaking historic codebases. | Add `-fno-strict-aliasing` to build flags for legacy code. |

---

## 8. Performance & Security Notes

### The 8x Vectorization Speedup of `restrict`
In signal processing and linear algebra, qualifying input arrays with `restrict` allows GCC and Clang to generate 256-bit AVX2 or 512-bit AVX-512 vector instructions. Without `restrict`, the compiler must insert expensive runtime branch checks before every loop to test if addresses overlap, or fall back to scalar instructions.

### Security: Compiler Optimization Deleting Safety Checks
When undefined behavior from strict aliasing violations occurs, modern optimizing compilers perform aggressive data-flow optimizations. If the compiler determines that a pointer dereference violates strict aliasing, it may assume that code branch is **unreachable dead code** and completely delete surrounding security bounds checks.

---

## 9. Comparison: When to Use What

| Technique | Strict Aliasing Safe? | Optimization Level | Portability | Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Pointer Cast `*(T2*)&var`** | **NO (Undefined Behavior)** | Dangerous | Non-portable | Banned in modern C |
| **Union Type Punning** | Yes (ISO C §6.5.2.3) | High (Zero cost) | ISO C99+ | Reinterpreting bit representations |
| **`memcpy()` Punning** | Yes (Character exception) | High (Optimized to register load) | 100% Portable | Converting bits between types |
| **`restrict` Qualifier** | Enforces non-aliasing | Maximum (SIMD enabled) | ISO C99+ | Matrix math, image processing kernels |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the Strict Aliasing Rule in ISO C?</strong></summary>

The Strict Aliasing Rule (§6.5) states that two pointers of different, incompatible types cannot point to the same memory location. The compiler is allowed to assume that writing through a pointer of type `T1 *` will never modify an object of type `T2 *`, allowing it to keep values cached in CPU registers across writes.
</details>

<details>
<summary><strong>2. What is the difference between <code>memcpy()</code> and <code>memmove()</code> with regard to <code>restrict</code>?</strong></summary>

`memcpy()` declares both destination and source pointers with the `restrict` qualifier (`void * restrict dest, const void * restrict src`). This promises they do not overlap, enabling the compiler to emit high-speed vector copy instructions.
`memmove()` omits `restrict`, allowing memory regions to overlap safely by detecting pointer direction and copying backwards if necessary.
</details>

<details>
<summary><strong>3. Which pointer types are exempt from the Strict Aliasing Rule?</strong></summary>

Only character pointer types (`char *`, `signed char *`, `unsigned char *`, and standard typedef `uint8_t *`) are exempt. The C standard explicitly permits character pointers to alias and inspect the object representation of any data type.
</details>

<details>
<summary><strong>4. How does the fast inverse square root algorithm from Quake III relate to strict aliasing?</strong></summary>

The famous Quake III code performed `i = * ( long * ) &y;` to interpret an IEEE-754 float as an integer. Under modern ISO C compilers with `-O2`, this pointer cast directly violates the strict aliasing rule, causing modern compilers to generate broken assembly. Modern rewrites use `memcpy()` or a union.
</details>

<details>
<summary><strong>5. Can <code>restrict</code> be applied to local variables?</strong></summary>

Yes. A developer can declare a local pointer `int * restrict p = &arr[10];`. Inside that local scope, the compiler assumes no other pointer in the function will access `arr[10]`.
</details>

<details>
<summary><strong>6. What happens if you violate the <code>restrict</code> contract?</strong></summary>

If two pointers qualified with `restrict` actually point to overlapping memory and data is modified through either pointer, the behavior is strictly undefined behavior (UB), typically resulting in corrupted data due to out-of-order execution or vector register caching.
</details>

<details>
<summary><strong>7. Why does the Linux kernel compile with <code>-fno-strict-aliasing</code>?</strong></summary>

The Linux kernel interacts directly with hardware devices, page tables, and network buffers, frequently casting raw byte buffers to struct pointers. Because rewriting millions of lines of low-level systems code to use `memcpy` or unions is impractical, Linus Torvalds mandated compiling the kernel with `-fno-strict-aliasing`.
</details>

<details>
<summary><strong>8. Does <code>restrict</code> exist in C++?</strong></summary>

`restrict` is an official keyword in ISO C (since C99), but is **not** part of the official ISO C++ standard. However, all major C++ compilers (GCC, Clang, MSVC) provide vendor extensions like `__restrict` or `__restrict__`.
</details>

<details>
<summary><strong>9. How does the compiler verify whether two pointers alias at compile time?</strong></summary>

Compilers utilize Type-Based Alias Analysis (TBAA). By inspecting the static types of expressions, the compiler generates a type-aliasing DAG (Directed Acyclic Graph). If two types belong to mutually incompatible branches of the DAG (and neither is a character type), the compiler formally proves they cannot alias without inspecting runtime values.
</details>

<details>
<summary><strong>10. Can two structs with identical members alias each other if they have different tag names?</strong></summary>

In ISO C, distinct struct tags define completely incompatible types. Even if `struct A { int x; }` and `struct B { int x; }` have identical members, pointers `struct A *` and `struct B *` cannot legally alias. Accessing the memory of `struct A` through `struct B *` violates the strict aliasing rule.
</details>


---

## 11. Practice Exercises

### Exercise 1: Fast Float Bit Inspector (Easy)
Write a function `uint32_t float_as_bits(float f)` that extracts the raw IEEE-754 bit representation of a float using `memcpy()` to conform strictly to the aliasing rule.

<details>
<summary>View Solution</summary>

```c
#include <stdint.h>
#include <string.h>

uint32_t float_as_bits(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof bits);
    return bits;
}
```
</details>

### Exercise 2: Vector Scalar Product with Restrict (Medium)
Implement `void vec_scale(float * restrict out, const float * restrict in, float factor, size_t n)` and inspect the generated assembly with `gcc -O3 -S` to observe auto-vectorization.

<details>
<summary>View Solution</summary>

```c
#include <stddef.h>

void vec_scale(float * restrict out, const float * restrict in, float factor, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        out[i] = in[i] * factor;
    }
}
```
</details>

### Exercise 3: Safe Memory Overlap Detector (Hard)
Write a function `bool buffers_overlap(const void *a, size_t len_a, const void *b, size_t len_b)` that verifies whether two memory buffers overlap in physical virtual memory.

<details>
<summary>View Solution</summary>

```c
#include <stdbool.h>
#include <stdint.h>

bool buffers_overlap(const void *a, size_t len_a, const void *b, size_t len_b) {
    uintptr_t start_a = (uintptr_t)a;
    uintptr_t end_a   = start_a + len_a;
    uintptr_t start_b = (uintptr_t)b;
    uintptr_t end_b   = start_b + len_b;

    return (start_a < end_b) && (start_b < end_a);
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Restrict qualification */
void saxpy(size_t n, float a, const float * restrict x, float * restrict y);

/* Safe type punning */
uint32_t u;
float f = 3.14f;
memcpy(&u, &f, sizeof u); /* 100% Standard compliant */

/* Rules at a Glance */
Never cast (float *)&int_var       /* Violates strict aliasing */
char * / uint8_t * can alias all   /* Legal byte inspection */
memcpy() requires disjoint memory  /* restrict parameters */
memmove() supports overlapping     /* No restrict */
union FloatBits { float f; int i; }/* Legal ISO C type punning */
gcc -fstrict-aliasing              /* Enables TBAA optimizations */

```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §6.5 "Expressions" (Strict aliasing) & §6.7.3.1 "Formal definition of restrict".
- ISO/IEC 9899:2024 (C23 Standard), §6.7.3.1 "Formal definition of restrict".
- Mike Acton: *Understanding Strict Aliasing* (Cell Performance).
- LLVM Developer Guide: *Type-Based Alias Analysis (TBAA) in LLVM*.

---

[← Previous: Flexible Array Members](./21-flexible-array-members.md) | [Section Index](./README.md) | [Next: Pointers to Pointers & Pointer Arrays →](./23-pointers-to-pointers-and-arrays-of-pointers.md)
