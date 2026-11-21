# Pointer Arithmetic: Offsets, Indexing, and Object Bounds

> Mastering pointer calculations in C: scaling by element size, pointer differences, array traversal, negative indexing, and ISO boundary constraints.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Arrays](./01-arrays.md), [Pointers](./03-pointers.md), [Declarations and Typedef](./04-declarations-typedef.md) |
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

In C, a pointer does not merely hold a raw memory address; it holds an address **bound to a specific type**. Because types have explicit byte sizes, C equips pointers with specialized arithmetic:
- Adding `1` to an `int *` advances the address not by 1 byte, but by `sizeof(int)` bytes (typically 4 bytes).
- Adding `1` to a `double *` advances the address by `sizeof(double)` bytes (8 bytes).

**Pointer Arithmetic** allows direct, efficient traversal of contiguous memory buffers, arrays, and memory pools without the overhead of index multiplication. It establishes the foundational duality in C: array subscript notation `a[i]` is identical to `*(a + i)`. Mastering pointer arithmetic is essential for writing high-performance algorithms, ring buffers, string tokenizers, and custom memory allocators.

---

## 2. Core Concepts

### Permitted Pointer Operations
ISO C strictly defines the set of legal arithmetic operations on pointers:
1. **Adding an integer to a pointer**: `p + n` or `n + p`. Yields a pointer of the same type pointing $n$ elements forward.
2. **Subtracting an integer from a pointer**: `p - n`. Yields a pointer of the same type pointing $n$ elements backward.
3. **Subtracting two pointers**: `p2 - p1`. Both pointers must reference elements within the **same array object** (or one past the end). The result has signed type **`ptrdiff_t`** defined in `<stddef.h>`, representing the number of intervening elements.
4. **Relational comparisons**: `p1 < p2`, `p1 >= p2`. Valid only if both pointers point into the same array object.
5. **Equality comparisons**: `p1 == p2`, `p1 != p2`. Valid between any compatible pointers or `NULL`.

### Forbidden Pointer Operations
- **Adding two pointers**: `p1 + p2` is strictly illegal and causes a compile-time error. (Adding two addresses has no physical meaning).
- **Multiplying or dividing pointers**: Strictly illegal.
- **Arithmetic on `void *` or function pointers**: In ISO C, `void` and functions are incomplete types with no `sizeof`, making arithmetic invalid. (GCC supports `void *` arithmetic as an extension treating `sizeof(void) == 1`).

### The "One-Past-the-End" Rule
ISO C permits computing a pointer to the element immediately following the last element of an array (e.g., `&arr[N]` for an array of size $N$). This pointer is valid for comparisons (acting as an end-sentinel for loops), but **must never be dereferenced**. Computing a pointer two elements past the end, or before the start, invokes immediate undefined behavior.

---

## 3. How It Works Internally

### Hardware Address Calculation and Scaling

```
Pointer Scaling Mechanics:
Consider: int arr[5] (where sizeof(int) == 4 bytes)
Base Address = 0x1000

 Index:        arr[0]      arr[1]      arr[2]      arr[3]      arr[4]      arr[5] (End)
 Address:      0x1000      0x1004      0x1008      0x100C      0x1010      0x1014
 Pointer:       ptr        ptr + 1     ptr + 2     ptr + 3     ptr + 4     ptr + 5
 Offset:       +0 bytes    +4 bytes    +8 bytes    +12 bytes   +16 bytes   +20 bytes
```

When you write `ptr + n`, the compiler emits an assembly instruction utilizing scaled-index addressing modes (such as `[rbx + rdi * 4]` on x86-64). The compiler automatically scales `n` by `sizeof(*ptr)`:
$$\text{Physical Address} = \text{ptr} + (n \times \text{sizeof}(*ptr))$$

### Pointer Subtraction Formula
When subtracting two pointers `p2 - p1`:
$$\text{Difference (ptrdiff\_t)} = \frac{\text{Address}(p2) - \text{Address}(p1)}{\text{sizeof}(*p1)}$$

---

## 4. Syntax & Usage

```c
#include <stdio.h>
#include <stddef.h>

void traverse_elements(const int *start, size_t count) {
    const int *end = start + count; /* One-past-the-end sentinel */
    
    for (const int *p = start; p < end; ++p) {
        printf("%d ", *p);
    }
    printf("\n");
}

/* Negative Indexing on Interior Pointers */
void inspect_neighbors(const int *arr) {
    const int *mid = &arr[2];
    
    int prev = mid[-1]; /* Same as *(mid - 1) -> arr[1] */
    int curr = mid[0];  /* Same as *mid -> arr[2] */
    int next = mid[1];  /* Same as *(mid + 1) -> arr[3] */
    
    printf("prev=%d, curr=%d, next=%d\n", prev, curr, next);
}
```

> [!NOTE]
> ### ISO C Pointer Bounds and Undefined Behavior
> Under ISO C (§6.5.6), pointer arithmetic is only valid when the pointer and the result point within, or one past the end of, the **same array object**. Even calculating an out-of-bounds pointer (e.g., `p - 1` before the start of an array) invokes Undefined Behavior immediately, even if the pointer is never dereferenced, because optimizing compilers assume pointers never wrap around or cross object boundaries.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates pointer increment traversal, pointer subtraction yielding `ptrdiff_t`, scaling by `sizeof(T)`, and negative indexing on interior pointers.

```c
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

int main(void) {
    printf("=== C Pointer Arithmetic Demo ===\n");

    int arr[5] = {10, 20, 30, 40, 50};
    int *start = arr;
    int *end = arr + 5; /* One-past-the-end pointer: valid to compute, not to dereference */

    /* 1. Traversal using pointer increment */
    printf("Forward traversal: ");
    for (int *p = start; p < end; ++p) {
        printf("%d ", *p);
    }
    printf("\n");

    /* 2. Pointer difference (ptrdiff_t) */
    int *p2 = &arr[4];
    int *p1 = &arr[1];
    ptrdiff_t diff = p2 - p1;
    printf("p2 - p1 difference: %td elements\n", diff);

    /* 3. Pointer scale factor by sizeof(T) */
    uintptr_t byte_diff = (uintptr_t)p2 - (uintptr_t)p1;
    printf("Byte distance: %zu bytes (diff * sizeof(int) = %td * %zu = %zu)\n",
           (size_t)byte_diff, diff, sizeof(int), (size_t)(diff * sizeof(int)));

    /* 4. Negative indexing and symmetry */
    int *mid = &arr[2]; /* Points to 30 */
    printf("mid[0]=%d, mid[-1]=%d, mid[1]=%d\n", mid[0], mid[-1], mid[1]);

    return 0;
}
```

```text
=== C Pointer Arithmetic Demo ===
Forward traversal: 10 20 30 40 50 
p2 - p1 difference: 3 elements
Byte distance: 12 bytes (diff * sizeof(int) = 3 * 4 = 12)
mid[0]=30, mid[-1]=20, mid[1]=40
```

### Real-World Example 2: 2D Matrix Row Stride Arithmetic
In numerical computing, graphics engines, and game physics, flat contiguous 1D buffers are often indexed as 2D grids using explicit pointer arithmetic strides rather than jagged pointer-to-pointer allocations:

```c
#include <stdio.h>

/* Traverse row by advancing pointer by row_stride */
void print_row(const float *grid, size_t row_index, size_t cols) {
    const float *row_start = grid + (row_index * cols);
    const float *row_end = row_start + cols;
    
    printf("Row %zu: ", row_index);
    for (const float *p = row_start; p < row_end; ++p) {
        printf("%.1f ", *p);
    }
    printf("\n");
}
```

### Real-World Example 3: Generic Memory Pattern Search with Byte Pointers
Standard generic functions like `memchr()` or binary search engines use byte pointers (`const uint8_t *` or `const char *`) to traverse arbitrary structures safely:

```c
#include <stddef.h>
#include <stdint.h>

const void* find_byte_pattern(const void *buffer, size_t buf_len, uint8_t target) {
    const uint8_t *start = (const uint8_t *)buffer;
    const uint8_t *end = start + buf_len;
    
    for (const uint8_t *p = start; p < end; ++p) {
        if (*p == target) {
            return (const void *)p; /* Match located */
        }
    }
    return NULL;
}
```


---

## 6. Best Practices

1. **Always Use `ptrdiff_t` for Pointer Differences**: Never cast pointer subtraction to `int` or `long`. Pointer differences have type `ptrdiff_t` (printed using the standard format specifier `%td`).
2. **Never Dereference the One-Past-The-End Pointer**: The pointer `arr + N` is strictly for boundary comparison. Attempting `*(arr + N)` reads out-of-bounds memory.
3. **Avoid Relational Comparisons Across Unrelated Objects**: Comparing `p1 < p2` when `p1` and `p2` belong to different variables or heap allocations invokes undefined behavior.
4. **Cast to `char *` or `uint8_t *` for Raw Byte Arithmetic**: Because `sizeof(char) == 1`, casting any pointer to `char *` allows stepping through memory byte-by-byte in custom allocators or serializers.
5. **Use Interior Pointers for Windowed Sub-Buffers**: Passing an interior pointer `&arr[offset]` along with a remaining size count allows algorithms to operate on sub-slices without copying memory.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Arithmetic on `void *`** | `void` has no size in standard ISO C; illegal syntax. | Cast to `uint8_t *` or `char *` before performing arithmetic. |
| **Dereferencing `arr + N`** | One-past-the-end pointer is valid to calculate, but holds no valid data; causes read overflow. | Stop loop condition before reaching end: `p < end`. |
| **Subtracting pointers to different objects** | Undefined Behavior. Pointers must belong to the same array or struct object. | Compare only pointers derived from the same base allocation. |
| **Assuming `p + 1` adds 1 byte** | Pointers scale automatically by `sizeof(*p)`. If `p` is `int *`, it advances 4 bytes. | Remember scaling is automatic; do not multiply by `sizeof(T)`. |
| **Printing `ptrdiff_t` with `%d`** | `ptrdiff_t` is 64 bits on 64-bit systems; `%d` truncates and causes format mismatches. | Use `%td` specifier from `<inttypes.h>` / `<stddef.h>`. |
| **Calculating pointers before array start** | `p - 1` before the base element `arr[0]` invokes undefined behavior even if not dereferenced. | Ensure pointer calculations stay within `[0, N]`. |

---

## 8. Performance & Security Notes

### Compiler Strength Reduction and Auto-Vectorization
Iterating arrays using pointer increments (`p++`) vs. array indices (`arr[i]`) historically produced faster code in pre-1990 compilers. Modern optimizing compilers recognize both patterns and apply **induction variable elimination** to emit identical, optimal assembly. Write whichever is clearer.

### Security: Pointer Wrapping and Buffer Overflows
If an attacker controls an integer `offset` added to a pointer `ptr + offset`, an extremely large integer can wrap 32-bit or 64-bit addresses, creating a pointer that references memory before the intended buffer, bypassing bounds-checking assertions. Always validate offsets against maximum element counts before arithmetic.

---

## 9. Comparison: When to Use What

| Technique | Readability | Pointer Bounds Safe? | Underlying Mechanics | Typical Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Array Indexing `a[i]`** | High | Index easily checked | Computes `*(a + i)` | General application logic, math vectors |
| **Pointer Traversal `*p++`** | Moderate | Requires sentinel check | Direct pointer increment | Stream parsing, string manipulation |
| **Raw Byte Pointers `char*`** | Low | Manual sizing required | 1-byte increments | Custom allocators, packet serializers |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is <code>i[arr]</code> valid C syntax and identical to <code>arr[i]</code>?</strong></summary>

In ISO C, the subscript operator is defined purely in terms of pointer arithmetic: `E1[E2]` is strictly identical to `*((E1) + (E2))`. Because addition is commutative, `arr[i]` expands to `*(arr + i)`, and `i[arr]` expands to `*(i + arr)`. Both expressions evaluate to the exact same memory location.
</details>

<details>
<summary><strong>2. What is <code>ptrdiff_t</code>, what header defines it, and what is its printf format specifier?</strong></summary>

`ptrdiff_t` is a standard signed integer type representing the result of subtracting two pointers pointing into the same array. It is defined in `<stddef.h>`, and its standard `printf` conversion specifier is `%td`.
</details>

<details>
<summary><strong>3. Can pointer arithmetic be performed on a <code>void *</code> pointer?</strong></summary>

In standard ISO C, no. `void` is an incomplete type that cannot be completed; therefore, `sizeof(void)` is undefined and pointer arithmetic is a constraint violation. However, the GNU C compiler (GCC) and Clang implement a compiler extension treating `sizeof(void)` as 1, allowing arithmetic unless compiling with `-pedantic-errors`.
</details>

<details>
<summary><strong>4. What does the expression <code>*(p++)</code> vs. <code>(*p)++</code> mean?</strong></summary>

- `*(p++)`: The postfix `++` increments the pointer `p` to point to the next element, but due to postfix evaluation, dereferences the original address of `p` before the increment.
- `(*p)++`: The parentheses force dereferencing `p` first; the postfix `++` increments the integer value stored at `*p` in place, leaving the pointer `p` unchanged.
</details>

<details>
<summary><strong>5. What is the difference between subtracting two pointers vs. subtracting a pointer and an integer?</strong></summary>

Subtracting two pointers (`p2 - p1`) computes the distance between them, yielding an integer count of elements (`ptrdiff_t`).
Subtracting an integer from a pointer (`p - n`) computes a new memory address, yielding a pointer of the same type shifted backwards by $n$ elements.
</details>

<details>
<summary><strong>6. Why is comparing two pointers with <code>&lt;</code> or <code>&gt;</code> undefined if they point to different variables?</strong></summary>

In modern virtual memory architectures, different variables or heap blocks may be allocated in arbitrary, non-contiguous virtual address pages. The ISO C standard does not guarantee any relative ordering between independent objects; hence, relational comparisons between unrelated objects are undefined.
</details>

<details>
<summary><strong>7. How does pointer arithmetic operate on multi-dimensional arrays?</strong></summary>

For a 2D array `int mat[3][4]`, `mat` decays into a pointer to an array of 4 integers: `int (*)[4]`. Adding 1 to this pointer (`mat + 1`) advances the address by `sizeof(int[4])` (16 bytes), skipping an entire row of 4 integers.
</details>

<details>
<summary><strong>8. What is an interior pointer and where is it used?</strong></summary>

An interior pointer is a pointer that points to a sub-element or field inside a larger composite object (such as `&struct_inst.field` or `&arr[5]`). It is widely used in parsers and string algorithms to process sub-slices without copying memory.
</details>

---

## 11. Practice Exercises

### Exercise 1: In-Place Array Reversal (Easy)
Write a function `void reverse_array(int *start, size_t n)` that reverses an array of integers in-place using two pointer pointers (`left` and `right`) incrementing and decrementing towards the center.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>

void reverse_array(int *start, size_t n) {
    if (!start || n <= 1) return;
    int *left = start;
    int *right = start + (n - 1);
    while (left < right) {
        int tmp = *left;
        *left = *right;
        *right = tmp;
        left++;
        right--;
    }
}
```
</details>

### Exercise 2: Custom Fast String Length (Medium)
Implement `size_t my_strlen(const char *s)` using pointer arithmetic: advance a pointer until reaching `'\0'`, and return the pointer difference `p - s`.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>

size_t my_strlen(const char *s) {
    if (!s) return 0;
    const char *p = s;
    while (*p) {
        p++;
    }
    return (size_t)(p - s);
}
```
</details>

### Exercise 3: Zero-Copy String Tokenizer (Hard)
Implement a zero-copy tokenizer `char* next_token(char **cursor, char delimiter)` that steps through a string, replaces the delimiter with `'\0'`, updates `*cursor` to point to the start of the next token, and returns the current token.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>

char* next_token(char **cursor, char delimiter) {
    if (!cursor || !*cursor || **cursor == '\0') return NULL;
    char *token_start = *cursor;
    char *p = token_start;
    while (*p != '\0' && *p != delimiter) {
        p++;
    }
    if (*p == delimiter) {
        *p = '\0';
        *cursor = p + 1;
    } else {
        *cursor = p; /* At end of string */
    }
    return token_start;
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Basic arithmetic */
T *p = arr;
p + n       /* Advances by n * sizeof(T) bytes */
p - n       /* Retreats by n * sizeof(T) bytes */
p2 - p1     /* Yields ptrdiff_t (element count difference) */

/* Duality */
arr[i] == *(arr + i) == *(i + arr) == i[arr]

/* Boundary rule */
arr + N     /* One-past-the-end: valid to compare, ILLEGAL to dereference */

/* Formatter */
printf("diff = %td\n", p2 - p1);
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §6.5.6 "Additive operators".
- ISO/IEC 9899:2024 (C23 Standard), §6.5.6 "Additive operators".
- *The C Programming Language* (2nd ed.) by Kernighan & Ritchie (Chapter 5: "Pointers and Arrays").
- SEI CERT C Coding Standard: "ARR30-C: Do not form or use out-of-bounds pointers or array subscripts".

---

[← Previous: File I/O](./11-file-io.md) | [Section Index](./README.md) | [Next: Dynamic Arrays & Realloc →](./13-dynamic-arrays-and-realloc-growth.md)
