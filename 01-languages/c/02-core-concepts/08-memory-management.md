# Memory Management: malloc, calloc, realloc, and free

> Dynamic memory allocation in C: heap architecture, allocator mechanics, memory leak prevention, and defensive allocation patterns.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Pointers](./03-pointers.md), [Structs, Unions, and Enums](./06-structs-unions-enums.md), [Scope and Storage Classes](./07-scope-storage-classes.md) |
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

In C, memory is not managed by a garbage collector or automatic runtime engine. While stack allocation is fast and automatically scoped, it has severe limitations:
- Stack frames are strictly bounded (typically 1–8 MB total per thread). Large allocations cause fatal stack overflow crashes.
- Stack variable lifetimes are tied to function return; objects cannot survive past their enclosing scope.
- Allocation sizes must generally be known or bounded at compile time.

**Dynamic Memory Management** solves this by granting direct control over the system **heap**. Using `<stdlib.h>` functions (`malloc`, `calloc`, `realloc`, and `free`), programs can request memory of arbitrary size at runtime, expand or shrink buffers dynamically, and pass object ownership across architectural layers. Mastering dynamic memory is the cornerstone of robust systems programming in C.

---

## 2. Core Concepts

### The Standard Allocation API (`<stdlib.h>`)
- `void* malloc(size_t size)`: Allocates an uninitialized contiguous block of `size` bytes. The initial memory contents are indeterminate garbage.
- `void* calloc(size_t num, size_t size)`: Allocates memory for an array of `num` elements of `size` bytes each, and initializes every bit to zero. Additionally, `calloc` safely checks for arithmetic overflow during `num * size`.
- `void* realloc(void *ptr, size_t new_size)`: Resizes an existing allocation. If the existing memory block cannot be expanded in place, `realloc` allocates a new block elsewhere, copies the old data, frees the old block, and returns the new pointer.
- `void free(void *ptr)`: Returns the allocated block back to the heap manager. Passing `NULL` to `free` is explicitly safe and performs no operation.

### Fundamental Rules of Dynamic Memory
1. **Never Cast `malloc` in C**: In C, `void *` implicitly converts to any data pointer type. Writing `int *p = (int *)malloc(...)` is unnecessary and can dangerously suppress compiler diagnostics if `<stdlib.h>` was accidentally omitted.
2. **Use the `sizeof *ptr` Idiom**: Write `int *p = malloc(n * sizeof *p);`. Referencing `*p` rather than `sizeof(int)` guarantees type consistency if the variable's declaration is refactored later.
3. **Always Check for `NULL`**: In low-memory conditions, virtual memory exhaustion, or security sandbox limits, allocation functions return `NULL`. Dereferencing an unchecked return value causes an immediate crash (`SIGSEGV`).
4. **Free Exactly Once**: Freeing the same memory block more than once causes heap metadata corruption (Double Free), a critical security vulnerability.

---

## 3. How It Works Internally

### Heap Architecture and Allocator Metadata

```
Heap Memory Layout (Chunk Structure in glibc ptmalloc):
 ┌───────────────────────┬───────────────────────────────────┐
 │ Chunk Size / Flags    │ User Data Payload                 │
 │ (4 or 8 bytes header) │ (Returned pointer points HERE)    │
 └───────────────────────┴───────────────────────────────────┘
 ↑                       ↑
 Real Chunk Start        Pointer returned by malloc()
```

When you call `malloc(32)`, the memory allocator (such as glibc's `ptmalloc`, jemalloc, or Windows Heap):
1. Adds internal management metadata (chunk size, previous chunk size, in-use flags).
2. Rounds the requested size up to meet hardware alignment requirements (typically 8 bytes on 32-bit, 16 bytes on 64-bit systems).
3. Searches internal segregated free lists (tcache, fastbins, unsorted bins).
4. If no free chunk is available, requests more virtual memory from the operating system kernel via `brk()` / `sbrk()` (for small blocks) or `mmap()` (for large blocks, typically > 128 KB).
5. Returns a pointer to the start of the payload region, immediately after the chunk header.

### The `realloc` Reallocation State Machine

```
                        realloc(ptr, new_size)
                                   │
              ┌────────────────────┴────────────────────┐
              ▼                                         ▼
   Can expand in place?                      Must relocate?
              │                                         │
     Yes ─────┴───── No                        Allocate new block
      │              │                                  │
  Update header   Fall back to relocate             Copy old data
  Return ptr                                            │
                                                    Free old block
                                                        │
                                                    Return new ptr
```

---

## 4. Syntax & Usage

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Safe dynamic array allocation */
int* allocate_integers(size_t count) {
    if (count == 0) return NULL;
    
    int *arr = malloc(count * sizeof *arr);
    if (!arr) {
        perror("malloc failed");
        return NULL;
    }
    return arr;
}

/* Safe realloc pattern */
int grow_buffer(int **buf_ptr, size_t *capacity) {
    size_t new_cap = (*capacity == 0) ? 4 : (*capacity * 2);
    
    /* CRITICAL: Use temporary pointer! */
    int *temp = realloc(*buf_ptr, new_cap * sizeof *temp);
    if (!temp) {
        /* Allocation failed; *buf_ptr is still valid and not leaked! */
        return 0;
    }
    
    *buf_ptr = temp;
    *capacity = new_cap;
    return 1;
}
```

> [!NOTE]
> ### ISO C11/C23 Updates: `aligned_alloc` and `<stdckdint.h>`
> - **`aligned_alloc(alignment, size)`**: Standardized in C11, allows requesting memory with custom hardware alignment (e.g., 64-byte alignment for AVX-512 SIMD operations or GPU cache lines).
> - **Checked Integer Arithmetic (`<stdckdint.h>`)**: Standardized in C23, functions like `ckd_mul(&total, count, sizeof *p)` prevent integer overflow vulnerabilities when calculating allocation byte counts.
> - **`free_sized` / `free_aligned_sized`**: C23 introduces sized deallocation primitives allowing allocators to reclaim memory faster without querying internal heap headers.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates dynamic buffer creation, dynamic array growth using `realloc` with temporary pointers, zero-initialization verification with `calloc`, and safe pointer nullification upon deallocation.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Demonstrate safe dynamic allocation, growth with realloc, and cleanup */

typedef struct Buffer {
    size_t capacity;
    size_t size;
    int *data;
} Buffer;

static Buffer* buffer_create(size_t initial_cap) {
    /* Guard against zero or extreme overflow */
    if (initial_cap == 0) initial_cap = 4;
    
    Buffer *buf = malloc(sizeof *buf);
    if (!buf) return NULL;

    buf->data = malloc(initial_cap * sizeof *(buf->data));
    if (!buf->data) {
        free(buf);
        return NULL;
    }
    buf->capacity = initial_cap;
    buf->size = 0;
    return buf;
}

static int buffer_push(Buffer *buf, int val) {
    if (buf->size >= buf->capacity) {
        size_t new_cap = buf->capacity * 2;
        /* Safe realloc idiom using temporary pointer */
        int *new_data = realloc(buf->data, new_cap * sizeof *new_data);
        if (!new_data) {
            return 0; /* Failed to grow, original buf->data is still valid */
        }
        buf->data = new_data;
        buf->capacity = new_cap;
    }
    buf->data[buf->size++] = val;
    return 1;
}

static void buffer_free(Buffer **buf_ptr) {
    if (!buf_ptr || !*buf_ptr) return;
    Buffer *b = *buf_ptr;
    free(b->data);
    b->data = NULL;
    free(b);
    *buf_ptr = NULL; /* Prevent dangling pointer at caller site */
}

int main(void) {
    printf("=== C Memory Management Demo ===\n");

    /* 1. Dynamic buffer lifecycle */
    Buffer *buf = buffer_create(2);
    if (!buf) {
        fprintf(stderr, "Failed to create buffer\n");
        return 1;
    }
    printf("Buffer allocated with capacity: %zu\n", buf->capacity);

    for (int i = 1; i <= 6; ++i) {
        buffer_push(buf, i * 10);
    }
    printf("Buffer populated: size=%zu, capacity=%zu\n", buf->size, buf->capacity);
    printf("Contents: ");
    for (size_t i = 0; i < buf->size; ++i) {
        printf("%d ", buf->data[i]);
    }
    printf("\n");

    /* 2. calloc zero-initialization check */
    size_t count = 5;
    int *zeroed = calloc(count, sizeof *zeroed);
    if (!zeroed) {
        buffer_free(&buf);
        return 1;
    }
    int all_zeros = 1;
    for (size_t i = 0; i < count; ++i) {
        if (zeroed[i] != 0) all_zeros = 0;
    }
    printf("calloc zero-check: %s\n", all_zeros ? "VERIFIED (all 0)" : "FAILED");
    free(zeroed);
    zeroed = NULL;

    /* 3. Safe cleanup */
    buffer_free(&buf);
    printf("Buffer safely freed, pointer is NULL: %s\n", buf == NULL ? "TRUE" : "FALSE");

    return 0;
}
```

```text
=== C Memory Management Demo ===
Buffer allocated with capacity: 2
Buffer populated: size=6, capacity=8
Contents: 10 20 30 40 50 60 
calloc zero-check: VERIFIED (all 0)
Buffer safely freed, pointer is NULL: TRUE
```

---

## 6. Best Practices

1. **Always Use a Temporary Pointer with `realloc`**: Never write `ptr = realloc(ptr, new_size);`. If `realloc` fails, it returns `NULL`, immediately overwriting `ptr`. The original allocation remains active in memory but is now unreferenced, causing an unrecoverable memory leak.
2. **Nullify Pointers Immediately After `free()`**: Writing `free(p); p = NULL;` ensures that any subsequent accidental call to `free(p)` is a harmless no-op, and any accidental dereference triggers an immediate, debuggable crash rather than silent memory corruption.
3. **Explicitly Guard Against Multiplication Wrap-Around**: Before allocating `n * sizeof(T)`, verify that `n <= SIZE_MAX / sizeof(T)`. Unchecked multiplication can overflow 32-bit or 64-bit bounds, allocating a tiny buffer that is immediately smashed by data writes.
4. **Clear Ownership Contracts**: Every dynamically allocated resource must have a clearly designated owner module responsible for releasing it. Avoid ambiguous APIs where ownership is unclear.
5. **Use AddressSanitizer (ASan) and Valgrind in CI**: Run all test suites with `gcc -fsanitize=address,undefined` or `valgrind --leak-check=full` to catch leaks, out-of-bounds writes, and use-after-free bugs during development.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **`p = realloc(p, size)` without temp** | If `realloc` fails, `p` becomes `NULL`, leaking the original block permanently. | Assign to temporary pointer `tmp = realloc(...)`; check `tmp != NULL` before updating `p`. |
| **Use-After-Free (UAF)** | Accessing `*p` after calling `free(p)` reads corrupted heap data or causes security exploits. | Set `p = NULL` immediately after `free(p)`. |
| **Double Free** | Calling `free(p)` twice on the same address corrupts the allocator's free list metadata. | Set `p = NULL` after freeing; `free(NULL)` is a safe no-op. |
| **Integer overflow in `malloc(n * size)`** | Huge `n` causes `n * size` to wrap to a tiny value, leading to severe heap buffer overflows. | Validate `n <= SIZE_MAX / size` or use `calloc(n, size)`. |
| **Memory Leak on early function exit** | Returning early from error handling branches without calling `free()` leaks allocated buffers. | Use the `goto cleanup;` pattern to centralize deallocation. |
| **Freeing a pointer not returned by `malloc`** | Passing stack addresses, interior pointers, or string literals to `free()` corrupts the heap. | Ensure only base pointers returned by `malloc`/`calloc`/`realloc` are passed to `free()`. |

---

## 8. Performance & Security Notes

### The Dangers of Integer Overflow in Allocation
A classic remote code execution vulnerability occurs when allocation calculations wrap:
```c
/* Vulnerable code */
void handle_packet(uint32_t count) {
    /* If count is 0x40000001 on 32-bit: count * 4 wraps around to 4 bytes! */
    uint32_t bytes = count * sizeof(int); 
    int *arr = malloc(bytes); /* Allocates only 4 bytes */
    for (uint32_t i = 0; i < count; i++) {
        arr[i] = read_int(); /* Overflows heap after first write! */
    }
}
```
Using `calloc(count, sizeof(int))` prevents this vulnerability because `calloc` internally checks for integer multiplication overflow and returns `NULL` if wrapping occurs.

### Arena and Memory Pool Allocators
For high-performance applications (such as game engines, network switches, or parsers), calling `malloc` millions of times causes heap fragmentation and thread contention. Allocating a large contiguous memory block upfront (an **Arena**) and bumping an offset pointer yields $O(1)$ allocation speeds and allows freeing an entire batch of objects in a single operation.

---

## 9. Comparison: When to Use What

| Function | Initialization | Overflow Checked? | Preserves Existing Data? | Typical Use Case |
| :--- | :--- | :--- | :--- | :--- |
| `malloc` | Uninitialized (Garbage) | No | N/A | General purpose dynamic buffers |
| `calloc` | Zeroed (`0x00`) | Yes (`num * size`) | N/A | Zero-initialized arrays, matrix buffers |
| `realloc` | Uninitialized for new bytes | No | Yes (up to old size) | Dynamic array / string buffer resizing |
| `aligned_alloc` | Uninitialized | No | N/A | SIMD AVX/NEON vectors, DMA buffers |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why should you never cast the result of <code>malloc()</code> in C?</strong></summary>

In C, `void *` automatically and implicitly converts to any other object pointer type, making casts redundant. Furthermore, if `<stdlib.h>` is inadvertently omitted, in pre-C99 standards the compiler assumed `malloc` returned `int`. Casting the result hid the compiler warning, causing truncation on 64-bit architectures where pointers are 64 bits but `int` is 32 bits.
</details>

<details>
<summary><strong>2. What is the exact difference between <code>malloc()</code> and <code>calloc()</code>?</strong></summary>

`malloc(size)` allocates `size` bytes of uninitialized memory. 
`calloc(num, size)` allocates `num * size` bytes and initializes every byte to zero. Additionally, `calloc` safely checks for integer multiplication overflow during `num * size` and returns `NULL` if arithmetic overflow occurs.
</details>

<details>
<summary><strong>3. What does <code>realloc(ptr, 0)</code> do?</strong></summary>

Historically in C89/C99/C11, `realloc(ptr, 0)` was implementation-defined: it could either free the memory and return `NULL`, or return an implementation-defined non-null pointer. Because this caused portability bugs and vulnerabilities, **ISO C23 explicitly makes calling `realloc` with a size of zero undefined behavior**. To free memory, always use `free(ptr)`.
</details>

<details>
<summary><strong>4. What is a "Double Free" vulnerability and how does it compromise security?</strong></summary>

A double free occurs when `free()` is called twice on the same memory pointer without an intervening allocation. In older heap allocators, this corrupted the doubly-linked list free chunks, allowing attackers to overwrite arbitrary function pointers or memory addresses when subsequent allocations occurred. Modern allocators detect double frees and abort immediately with `SIGABRT`.
</details>

<details>
<summary><strong>5. What is memory fragmentation, and what is the difference between internal and external fragmentation?</strong></summary>

**Internal Fragmentation**: Occurs when the allocator allocates more memory than requested due to alignment or chunk size constraints (e.g., requesting 5 bytes but receiving a 16-byte chunk).
**External Fragmentation**: Occurs when total free heap memory is sufficient, but it is broken into small, non-contiguous blocks, preventing a large contiguous allocation from succeeding.
</details>

<details>
<summary><strong>6. What is a memory leak, and what are its real-world consequences in long-running daemon processes?</strong></summary>

A memory leak occurs when dynamically allocated memory loses all references without being released via `free()`. In short-lived CLI utilities, the OS reclaims all memory upon process termination. In long-running servers or daemons, leaks continuously consume virtual memory, eventually causing performance degradation due to swapping and eventual termination by the OS Out-Of-Memory (OOM) killer.
</details>

<details>
<summary><strong>7. How does the <code>goto cleanup;</code> idiom solve memory management complexity?</strong></summary>

When a function acquires multiple resources (files, sockets, memory buffers), error handling branches frequently result in duplicated cleanup code or leaks. The `goto cleanup;` idiom centralizes all resource release logic at the end of the function, ensuring consistent deallocation before exit.
</details>

<details>
<summary><strong>8. What is the difference between <code>brk()</code>/<code>sbrk()</code> and <code>mmap()</code> in OS heap management?</strong></summary>

`brk()` and `sbrk()` adjust the program break (the boundary of the process data segment), expanding the heap contiguously upward.
`mmap()` creates an independent anonymous memory mapping in the process virtual address space. Allocators typically use `brk` for small allocations and `mmap` for large allocations (e.g., > 128 KB) so they can be unmapped and returned directly to the kernel upon deallocation.
</details>

---

## 11. Practice Exercises

### Exercise 1: Safe String Duplication (Easy)
Implement your own version of `char* my_strdup(const char *s)` using `malloc`, `strlen`, and `memcpy`. Handle `NULL` inputs safely.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* my_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (!copy) return NULL;
    memcpy(copy, s, len);
    return copy;
}
```
</details>

### Exercise 2: Dynamic String Concatenation Buffer (Medium)
Create a `StringBuilder` structure that dynamically appends strings, doubling its internal capacity whenever the buffer becomes full.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *str;
    size_t len;
    size_t cap;
} StringBuilder;

StringBuilder* sb_create(void) {
    StringBuilder *sb = malloc(sizeof *sb);
    if (!sb) return NULL;
    sb->cap = 16;
    sb->len = 0;
    sb->str = malloc(sb->cap);
    if (!sb->str) { free(sb); return NULL; }
    sb->str[0] = '\0';
    return sb;
}

int sb_append(StringBuilder *sb, const char *text) {
    if (!sb || !text) return 0;
    size_t add_len = strlen(text);
    while (sb->len + add_len + 1 > sb->cap) {
        size_t new_cap = sb->cap * 2;
        char *new_str = realloc(sb->str, new_cap);
        if (!new_str) return 0;
        sb->str = new_str;
        sb->cap = new_cap;
    }
    memcpy(sb->str + sb->len, text, add_len + 1);
    sb->len += add_len;
    return 1;
}

void sb_free(StringBuilder *sb) {
    if (!sb) return;
    free(sb->str);
    free(sb);
}
```
</details>

### Exercise 3: Dynamic 2D Matrix Allocator (Hard)
Write two functions:
1. `double** matrix_create(size_t rows, size_t cols)`: Allocates a contiguous 2D matrix in a single heap allocation to prevent memory fragmentation, returning an array of row pointers.
2. `void matrix_free(double **mat)`: Deallocates the matrix cleanly.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>

double** matrix_create(size_t rows, size_t cols) {
    if (rows == 0 || cols == 0) return NULL;
    
    /* Allocate pointer array and contiguous data buffer in one shot */
    double **row_ptrs = malloc(rows * sizeof(double *));
    if (!row_ptrs) return NULL;
    
    double *data = malloc(rows * cols * sizeof(double));
    if (!data) {
        free(row_ptrs);
        return NULL;
    }
    
    for (size_t r = 0; r < rows; r++) {
        row_ptrs[r] = data + (r * cols);
    }
    return row_ptrs;
}

void matrix_free(double **mat) {
    if (!mat) return;
    free(mat[0]); /* Free contiguous data buffer */
    free(mat);    /* Free row pointer array */
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Basic allocation */
int *p = malloc(count * sizeof *p);
if (!p) { /* handle error */ }

/* Zero-initialized allocation with overflow guard */
int *arr = calloc(count, sizeof *arr);

/* Safe realloc idiom */
void *tmp = realloc(p, new_count * sizeof *p);
if (!tmp) { /* handle error; p is still valid */ }
p = tmp;

/* Defensive free */
free(p);
p = NULL;
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §7.22.3 "Memory management functions".
- ISO/IEC 9899:2024 (C23 Standard), §7.22.3 "Memory management functions" (realloc size zero UB & sized free).
- *The Linux Programming Interface* by Michael Kerrisk (Chapter 7: "Memory Allocation").
- Doug Lea's Malloc Design: *A Memory Allocator* (dlmalloc architecture).

---

[← Previous: Scope & Storage Classes](./07-scope-storage-classes.md) | [Section Index](./README.md) | [Next: Preprocessor →](./09-preprocessor.md)
