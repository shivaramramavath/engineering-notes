# Arrays and Contiguous Memory Architecture

> A rigorous exploration of arrays in C: physical contiguous memory layouts, pointer arithmetic equivalence, array decay mechanics and exceptions, multi-dimensional row-major layouts, Variable Length Arrays (VLAs) safety implications, and bounds checker instruments.

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [01-fundamentals/02-data-types.md](../01-fundamentals/02-data-types.md), [01-fundamentals/11-sizeof-and-alignment-basics.md](../01-fundamentals/11-sizeof-and-alignment-basics.md) |
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

In C, an array is not an object, a container, or a managed class with a `.length` property. It is nothing more and nothing less than a **strictly contiguous sequence of homogeneous objects allocated in physical memory**.

Understanding how arrays behave at the machine level is critical for every systems engineer:
1. **Zero Runtime Bounds Checking**: Standard C performs **no bounds verification**. If an array has 5 elements, accessing index 10 reads whatever arbitrary data happens to reside in adjacent memory, enabling information disclosure and stack-smashing exploits.
2. **The Array Decay Illusion**: In almost all expressions, the name of an array silently converts ("decays") into an rvalue pointer to its first element. Forgetting this leads developers to write `sizeof(arr)` inside functions, mistakenly calculating pointer size (4 or 8 bytes) rather than array capacity!
3. **Cache Line Locality**: Because array elements are packed contiguously with zero overhead, traversing arrays sequentially matches hardware CPU prefetchers perfectly, yielding memory access speeds orders of magnitude faster than linked data structures.

---

## 2. Core Concepts

### Memory Layout of One-Dimensional Arrays

When an array `T arr[N];` is defined, the compiler allocates a contiguous block of memory of size $N \times \text{sizeof}(T)$ bytes:

```
Address:      [ 0x1000 ]    [ 0x1004 ]    [ 0x1008 ]    [ 0x100C ]    [ 0x1010 ]
Element:      arr[0]        arr[1]        arr[2]        arr[3]        arr[4]
Byte Offset:  +0            +4            +8            +12           +16
```

### The Fundamental Array-Pointer Equivalence

In standard C, the array indexing operator `arr[i]` is defined by the ISO standard as strictly syntactic sugar for pointer arithmetic dereference:

$$\text{arr}[i] \equiv *((arr) + (i))$$

Because addition is commutative ($a + b == b + a$), `*((arr) + (i))` is mathematically identical to `*((i) + (arr))`, which means the bizarre syntax `i[arr]` is completely valid in C and compiles to the exact same machine code!

### Array Decay and The Three Exceptions

In most expressions, an array name decays into a pointer of type `T *` pointing to `&arr[0]`. The **only three exceptions** are:
1. **`sizeof(arr)`**: Returns the total byte footprint ($N \times \text{sizeof}(T)$), not pointer size.
2. **Unary Address-Of `&arr`**: Produces a pointer to the *entire array* of type `T (*)[N]`, which advances by the whole array size when incremented.
3. **Character Array Initializer (`char s[] = "abc";`)**: The string literal initializes the array elements directly rather than decaying into a pointer.

---

## 3. How It Works Internally

### Multi-Dimensional Arrays: Row-Major Ordering

C lays out multi-dimensional arrays in **Row-Major Order** (the last index varies fastest). A 2D array `int matrix[2][3]` is stored as a flat, contiguous 1D sequence in physical RAM:

```
Source Definition:
int matrix[2][3] = {
    { 1, 2, 3 },  /* Row 0 */
    { 4, 5, 6 }   /* Row 1 */
};

Physical Memory Layout (Contiguous):
[ 1 ][ 2 ][ 3 ][ 4 ][ 5 ][ 6 ]
  ^         ^    ^
  |         |    +-- matrix[1][0] (Directly follows matrix[0][2] in memory!)
  |         +------- matrix[0][2]
  +----------------- matrix[0][0]
```

### Addressing Formula for 2D Arrays:
$$\text{Address}(\text{matrix}[r][c]) = \text{Base} + (r \times \text{Cols} + c) \times \text{sizeof}(T)$$

Notice that computing this address requires knowing the **column count (`Cols`)** at compile time! That is why when passing a 2D array to a function, the column dimension must be specified: `void process(int matrix[][3])`.

---

### 3D Array Memory Addressing Mechanics

For a 3D array defined as T tensor[D1][D2][D3], elements are laid out linearly in RAM according to the Generalized Row-Major Addressing Formula:

\\text{Offset}(i, j, k) = ((i \\times D2 + j) \\times D3 + k) \\times \\text{sizeof}(T)

Notice that the first dimension ($) does not appear in the offset formula! Only the trailing dimensions ($ and $) are required to compute the physical byte offset. This explains why C permits omitting the first dimension in function parameters (oid func(int arr[][4][8])), but requires all subsequent dimensions to be explicitly specified at compile time.

---

## 4. Syntax & Usage

### 1. Array Initialization Patterns
```c
/* Full initialization */
int scores[5] = { 90, 85, 78, 92, 88 };

/* Partial initialization: Remaining elements are ZERO-INITIALIZED */
int counters[100] = { 0 }; /* Wipes entire array to zero! */

/* Inferred dimension (size is 4) */
int primes[] = { 2, 3, 5, 7 };

/* Designated initializers (C99+) */
int sparse[10] = { [0] = 1, [5] = 42, [9] = 99 };
```

### 2. Computing Element Count Safely
```c
/* Portable macro to compute element count of real arrays */
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
```

---

## 5. Practical Examples

### Arrays and Memory Layout Demonstration Program

```c
/* File: 01-languages/c/02-core-concepts/examples/01-arrays.c */
#include <stdio.h>

int main(void) {
    printf("=== Arrays & Memory Layout Demo ===\n");
    
    int arr[5] = { 10, 20, 30, 40, 50 };
    printf("Array element count: %zu\n", sizeof(arr) / sizeof(arr[0]));
    printf("Base address &arr[0]: %p\n", (void *)&arr[0]);
    printf("Next address &arr[1]: %p (Step offset: %zu bytes)\n", 
           (void *)&arr[1], (size_t)((char *)&arr[1] - (char *)&arr[0]));

    /* 2D array row-major contiguous layout */
    int matrix[2][3] = { {1, 2, 3}, {4, 5, 6} };
    printf("Row-major element matrix[1][0] at %p\n", (void *)&matrix[1][0]);
    printf("Adjacent element matrix[0][2] at %p (Directly precedes matrix[1][0] in RAM!)\n", 
           (void *)&matrix[0][2]);

    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 01-arrays.c -o run_01.exe
./run_01.exe
```
Output:
```text
=== Arrays & Memory Layout Demo ===
Array element count: 5
Base address &arr[0]: 0061ff0c
Next address &arr[1]: 0061ff10 (Step offset: 4 bytes)
Row-major element matrix[1][0] at 0061ff00
Adjacent element matrix[0][2] at 0061fefc (Directly precedes matrix[1][0] in RAM!)
```

---

## 6. Best Practices

1. **Always Pass Array Length as an Explicit Parameter**: Because arrays decay to pointers across function boundaries, functions receiving arrays must accept `(T *arr, size_t count)`.
2. **Never Return Pointers to Local Automatic Arrays**: The stack frame is destroyed on function exit; the returned pointer is instantly dangling and corrupt.
3. **Avoid Variable Length Arrays (VLAs)**: VLAs allocate memory on the execution stack without error checking. If input size exceeds stack limits, an uncatchable stack overflow crash occurs. Prefer `malloc()` for runtime-sized arrays.
4. **Iterate 2D Arrays in Row-Major Order**: Outer loop on rows, inner loop on columns (`matrix[r][c]`). Iterating column-major destroys CPU cache prefetching performance.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Using `sizeof(arr)` inside a function | Arrays decay to pointers when passed as parameters; `sizeof` returns 4 or 8 bytes (pointer size), NOT array size! | Pass array length as a separate `size_t len` argument. |
| Off-by-one index access (`arr[N]`) | Valid indices range from $0$ to $N-1$. Accessing `arr[N]` invokes undefined behavior. | Loop while `i < N`, never `i <= N`. |
| Assigning arrays directly (`arr1 = arr2;`) | Array names are non-modifiable lvalues; they cannot be the target of an assignment. | Use `memcpy(arr1, arr2, sizeof(arr1));`. |
| Iterating 2D arrays in column-major order | Strides across memory by row jumps, causing constant CPU L1/L2 cache misses. | Loop row-by-row: `for(r) for(c)`. |
| Using unvalidated user input as VLA dimension | Large values instantly cause stack overflow crashes; zero or negative values invoke UB. | Validate size strictly or use `malloc()`. |
| Subtracting pointers from different arrays | Pointer arithmetic and subtraction is only defined for pointers within the **same** array object. | Compare and subtract pointers belonging to the same array. |

---

## 8. Performance & Security Notes

- **Spatial Locality and Cache Line Prefetching**: When the CPU loads an array element from memory into L1 cache, it fetches a full 64-byte **cache line**. If `int` is 4 bytes, loading `arr[0]` automatically pulls `arr[1]` through `arr[15]` into the fastest hardware cache simultaneously!
- **Stack Smashing via Buffer Overflows**: Writing beyond array bounds on the stack overwrites the saved return address `%rip`, allowing attackers to hijack program execution control. Always use bounds-checking instruments (`-fsanitize=address`).

---

## 9. Comparison / When to Use What

| Array Type | Allocation Location | Lifetime | Size Determinism | Safety Profile |
|---|---|---|---|---|
| **Fixed-Size Array (`int a[10]`)** | Stack or `.bss` | Scope / Program | Compile-time constant | High (Bounds checked by linters) |
| **Dynamic Array (`malloc(n * sz)`)**| Heap | Manual (`free`) | Runtime variable | High (Can check allocation failure) |
| **Variable Length Array (`int a[n]`)**| Stack | Enclosing block | Runtime variable | **Risky** (Uncatchable stack overflow) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does `sizeof(arr)` return the array size in `main()`, but pointer size inside a function taking `int arr[]`?</strong></summary>

In the scope where an array is declared, the compiler retains its complete array type including dimension, so `sizeof` computes $N \times \text{sizeof}(T)$. However, in standard C, any function parameter declared with array syntax `void f(int arr[])` is automatically rewritten by the compiler into a pointer parameter `void f(int *arr)`. Inside the function, `arr` is an actual pointer variable, so `sizeof(arr)` yields the pointer size.
</details>

<details>
<summary><strong>2. Why is `arr[i]` identical to `i[arr]` in C?</strong></summary>

Under the ISO C standard, the subscript operator `E1[E2]` is defined as `*((E1) + (E2))`. Because addition is commutative, `*((arr) + (i))` is mathematically and semantically identical to `*((i) + (arr))`, which corresponds to `i[arr]`.
</details>

<details>
<summary><strong>3. What is the difference between `int *p` and `int (*p)[5]`?</strong></summary>

- `int *p`: Pointer to a single integer. Incrementing `p++` advances the address by `sizeof(int)` (4 bytes).
- `int (*p)[5]`: Pointer to an array of 5 integers. Incrementing `p++` advances the address by the size of the entire array: $5 \times \text{sizeof}(int) = 20$ bytes.
</details>

<details>
<summary><strong>4. Why is row-major traversal faster than column-major traversal in C?</strong></summary>

In row-major order, elements within the same row are contiguous in physical RAM. Sequential access leverages spatial cache locality: loading one element brings the next 15 elements into the CPU L1 cache line. Column-major traversal jumps across entire rows, causing cache misses on every access.
</details>

<details>
<summary><strong>5. What are the three exceptions where an array does NOT decay into a pointer?</strong></summary>

1. When used as the operand of `sizeof` (`sizeof(arr)`).
2. When used as the operand of unary address-of `&` (`&arr`).
3. When used as a string literal initializer for a character array (`char s[] = "abc"`).
</details>

<details>
<summary><strong>6. Why were Variable Length Arrays (VLAs) made optional in C11?</strong></summary>

VLAs allocate memory on the thread execution stack at runtime without any failure detection mechanism. If the requested size exceeds available stack memory, a stack overflow crash occurs. Compilers for embedded systems and aerospace architectures rejected mandatory VLA support, prompting ISO C11 to make them optional (`__STDC_NO_VLA__`).
</details>

<details>
<summary><strong>7. What happens when an array initializer contains fewer elements than its declared size?</strong></summary>

The remaining elements are automatically **zero-initialized**. For example, in `int arr[100] = { 1 };`, `arr[0]` is initialized to 1, and `arr[1]` through `arr[99]` are initialized to `0`.
</details>

<details>
<summary><strong>8. Is it legal to access the element one past the end of an array?</strong></summary>

You are permitted to **compute the address** of the element one past the end of an array (`&arr[N]`), which is commonly used as an end iterator in loops. However, **dereferencing** that address (`*(&arr[N])`) invokes undefined behavior.
</details>

---

<details>
<summary><strong>9. How do arrays of pointers differ from 2D arrays in memory layout?</strong></summary>

- **2D Array (`int matrix[3][4]`)**: Allocated as a single, contiguous $3 \times 4 \times 4 = 48$-byte block in memory. All rows are adjacent.
- **Array of Pointers (`int *ptrs[3]`)**: Allocates 3 pointer variables (24 bytes on 64-bit). Each pointer can point to dynamically allocated memory located at completely disjoint, non-contiguous heap addresses, causing pointer-chasing cache misses.
</details>

<details>
<summary><strong>10. What is a Flexible Array Member in C99/C11?</strong></summary>

A flexible array member is an array declared with empty brackets as the last member of a structure: `struct Packet { int len; uint8_t data[]; };`. It allows allocating the structure and variable payload in a single contiguous `malloc(sizeof(struct Packet) + payload_len)` call.
</details>

<details>
<summary><strong>11. Can you pass a dynamic 2D array allocated via multiple `malloc` calls to a function taking `int matrix[][N]`?</strong></summary>

No! A function taking `int matrix[][N]` expects a contiguous 1D block where rows are indexed via row-stride pointer arithmetic: `*(base + r * N + c)`. An array of `malloc` pointers requires double-dereference `*(*(base + r) + c)`. Passing one where the other is expected causes memory corruption.
</details>

<details>
<summary><strong>12. What compiler flag enables runtime bounds checking for arrays in GCC and Clang?</strong></summary>

`-fsanitize=bounds` (part of UndefinedBehaviorSanitizer) and AddressSanitizer (`-fsanitize=address`). These flags insert instrumentation that detects and reports any out-of-bounds access on fixed-size and dynamic arrays at runtime.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Safe Array Summation Function</strong></summary>

**Goal**: Write a function `int sum_array(const int *arr, size_t count)` that computes the sum of elements, handling null pointers and zero counts safely.

**Solution**:
```c
#include <stddef.h>

int sum_array(const int *arr, size_t count) {
    if (!arr || count == 0) {
        return 0;
    }
    int sum = 0;
    for (size_t i = 0; i < count; ++i) {
        sum += arr[i];
    }
    return sum;
}
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Flattening a 2D Matrix to 1D</strong></summary>

**Goal**: Implement a function that converts 2D row-column coordinates `(r, c)` into a 1D flat index for a matrix with `cols` columns, validating bounds.

**Solution**:
```c
#include <stddef.h>
#include <stdbool.h>

bool get_flat_index(size_t r, size_t c, size_t rows, size_t cols, size_t *out_idx) {
    if (r >= rows || c >= cols || !out_idx) {
        return false;
    }
    *out_idx = r * cols + c; /* Row-major flat indexing */
    return true;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Cache-Friendly 2D Matrix Multiplication</strong></summary>

**Goal**: Write a matrix multiplication function for $N \times N$ matrices that transposes the second matrix before multiplication to ensure both matrix reads traverse memory in contiguous row-major order.

**Solution**:
```c
#include <stdlib.h>

void multiply_matrices_fast(const float *A, const float *B, float *C, size_t N) {
    /* Step 1: Transpose matrix B into temporary buffer */
    float *B_T = malloc(N * N * sizeof(float));
    if (!B_T) return;

    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
            B_T[j * N + i] = B[i * N + j];
        }
    }

    /* Step 2: Multiply with contiguous row access for both A and B_T */
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
            float sum = 0.0f;
            for (size_t k = 0; k < N; ++k) {
                sum += A[i * N + k] * B_T[j * N + k]; /* Contiguous streaming! */
            }
            C[i * N + j] = sum;
        }
    }

    free(B_T);
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Contiguous Memory**: `arr[i]` is identical to `*(arr + i)`.
- **Array Decay**: Arrays decay to `T *` in expressions, except in `sizeof`, `&`, and string initializers.
- **2D Arrays**: Stored in row-major order; memory is flat and contiguous.
- **`sizeof(arr) / sizeof(arr[0])`**: Only works on real arrays in declared scope, never on decayed pointers.
- Avoid VLAs (`int arr[n]`); use `malloc` for runtime bounds.
- Always iterate 2D arrays row-first (`for r ... for c`).

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 6.7.6.2: "Array declarators".
- Bryant & O'Hallaron: *Computer Systems: A Programmer's Perspective* (Chapter 3.8: Array Allocation and Access).
- SEI CERT C Coding Standard: *ARR30-C. Do not form or use out-of-bounds pointers or array subscripts*.

---

[← Previous: ../01-fundamentals/15-lvalues-rvalues-and-expressions.md](../01-fundamentals/15-lvalues-rvalues-and-expressions.md) | [Section Index](./README.md) | [Next: 02-strings.md](./02-strings.md) →
