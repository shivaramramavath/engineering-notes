# Dynamic Arrays: Capacity Growth, Realloc, and Amortized O(1)

> Building resizeable dynamic arrays (vectors) in C: geometric growth strategies, temporary pointer realloc idioms, memory layouts, and amortized complexity.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Pointers](./03-pointers.md), [Memory Management](./08-memory-management.md), [Structs, Unions, and Enums](./06-structs-unions-enums.md) |
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

Fixed-size arrays allocated on the stack or static data segments suffer from a major limitation: their size must be predetermined. If the input data exceeds the array capacity, the program either overflows its buffer or crashes. Conversely, over-allocating enormous arrays wastes memory.

A **Dynamic Array** (frequently called a `Vector` or `ArrayList`) overcomes this limitation by managing a heap-allocated buffer that grows automatically as elements are inserted. Unlike linked lists, dynamic arrays preserve $O(1)$ random access, contiguous memory cache locality, and minimal memory overhead.

Implementing dynamic arrays in C requires understanding:
1. The difference between **size** (elements currently stored) and **capacity** (elements allocated).
2. The mathematical requirement for **geometric capacity scaling** (e.g., $1.5\times$ or $2\times$) to maintain $O(1)$ amortized insertion time.
3. The defensive use of temporary pointers with `realloc()` to prevent catastrophic memory leaks on allocation failure.

---

## 2. Core Concepts

### Size vs. Capacity
A dynamic array maintains two fundamental size metrics:
- **`size`**: The number of valid active elements currently populated by the user.
- **`capacity`**: The total number of elements the current heap block can accommodate before requiring reallocation.
When `size == capacity`, the array is completely full. Pushing an additional element requires triggering a reallocation.

### Geometric Growth vs. Linear Growth
- **Linear Growth ($+K$)**: If capacity expands by a fixed constant (e.g., adding 10 slots each time), appending $N$ elements requires $O(N)$ reallocation passes, copying an aggregate of $O(N^2)$ items. The average insertion cost degrades to $O(N)$.
- **Geometric Growth ($\times 1.5$ or $\times 2.0$)**: By doubling capacity when full, each reallocation copies double the elements of the previous pass, but occurs half as frequently. The total work to insert $N$ items is bounded by $\sum 2^i \approx 2N$, resulting in an **amortized time complexity of $O(1)$ per push operation**.

### The Defensive `realloc` Idiom
```c
/* CRITICAL: Never assign realloc directly to the buffer pointer! */
int *tmp = realloc(v->items, new_capacity * sizeof *tmp);
if (!tmp) {
    /* Handle out-of-memory error; v->items is still valid! */
    return 0;
}
v->items = tmp;
v->capacity = new_capacity;
```

---

## 3. How It Works Internally

### Memory Layout and Reallocation Lifecycle

```
Dynamic Vector Structure:
┌───────────────────────────┬───────────────────────────┬───────────────────────────┐
│ size = 3                  │ capacity = 4              │ items (pointer)           │
└───────────────────────────┴───────────────────────────┴─────────────┬─────────────┘
                                                                      │
                                   ┌──────────────────────────────────┘
                                   ▼
Heap Buffer (Capacity = 4):
┌───────────────┬───────────────┬───────────────┬───────────────┐
│ items[0] = 10 │ items[1] = 20 │ items[2] = 30 │ [ Unused ]    │
└───────────────┴───────────────┴───────────────┴───────────────┘

Inserting 4th item -> size becomes 4 (Full).
Inserting 5th item -> Triggers realloc(items, 8 * sizeof(int)):
  1. Allocator looks for adjacent free heap space.
  2. If blocked, allocates new 8-element chunk elsewhere.
  3. Copies elements 0-3 to new block.
  4. Releases old 4-element chunk back to heap.
  5. Returns pointer to new block; capacity becomes 8.
```

---

## 4. Syntax & Usage

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Vector {
    size_t size;
    size_t capacity;
    int *data;
} Vector;

Vector* vector_init(size_t initial_capacity);
int     vector_push_back(Vector *v, int value);
int     vector_pop_back(Vector *v, int *out_value);
void    vector_free(Vector *v);
```

> [!NOTE]
> ### 1.5x vs. 2.0x Growth Factors
> While doubling ($2\times$) is common and simple to implement with bit shifts, many production allocators (like MSVC `std::vector` and Facebook's folly vector) use a growth factor of **1.5x**. With $2\times$ growth, the size of each new allocation is strictly greater than the sum of all previously freed chunks, preventing the memory allocator from ever reusing previously vacated memory blocks. A factor of $1.5\times$ allows recycled memory reuse after several reallocations.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates initializing a dynamic vector, appending elements with automatic geometric doubling, safely handling memory via temporary pointers, and freeing resources cleanly.

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct IntVector {
    size_t size;
    size_t capacity;
    int *items;
} IntVector;

static IntVector* vector_create(size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 4;
    IntVector *v = malloc(sizeof *v);
    if (!v) return NULL;
    v->items = malloc(initial_cap * sizeof *(v->items));
    if (!v->items) {
        free(v);
        return NULL;
    }
    v->capacity = initial_cap;
    v->size = 0;
    return v;
}

static int vector_push(IntVector *v, int val) {
    if (v->size >= v->capacity) {
        size_t new_cap = v->capacity * 2; /* 2.0x geometric growth */
        int *new_items = realloc(v->items, new_cap * sizeof *new_items);
        if (!new_items) return 0;
        v->items = new_items;
        v->capacity = new_cap;
    }
    v->items[v->size++] = val;
    return 1;
}

static void vector_free(IntVector **v_ptr) {
    if (!v_ptr || !*v_ptr) return;
    IntVector *v = *v_ptr;
    free(v->items);
    free(v);
    *v_ptr = NULL;
}

int main(void) {
    printf("=== C Dynamic Arrays and Realloc Demo ===\n");

    IntVector *v = vector_create(2);
    if (!v) return 1;

    printf("Initial vector capacity: %zu\n", v->capacity);
    for (int i = 1; i <= 7; ++i) {
        vector_push(v, i * 11);
    }
    printf("Vector populated: size=%zu, capacity=%zu\n", v->size, v->capacity);
    printf("Elements: ");
    for (size_t i = 0; i < v->size; ++i) {
        printf("%d ", v->items[i]);
    }
    printf("\n");

    vector_free(&v);
    printf("Vector cleanly freed: %s\n", v == NULL ? "YES" : "NO");

    return 0;
}
```

```text
=== C Dynamic Arrays and Realloc Demo ===
Initial vector capacity: 2
Vector populated: size=7, capacity=8
Elements: 11 22 33 44 55 66 77 
Vector cleanly freed: YES
```

---

## 6. Best Practices

1. **Always Use a Temporary Pointer for `realloc`**: Direct assignment `v->items = realloc(v->items, ...)` overwrites the original pointer on failure, leaking previously allocated memory.
2. **Never Retain Pointers to Vector Elements Across Pushes**: When a dynamic array expands, `realloc` may relocate the entire buffer to a new address. Any external pointers pointing to elements in the old buffer become immediate dangling pointers.
3. **Guard Against Multiplication Wrap-Around in Reallocation**: When computing `new_cap * sizeof(T)`, verify that `new_cap <= SIZE_MAX / sizeof(T)` to prevent integer overflow.
4. **Implement Shrink-To-Fit Judiciously**: After deleting large numbers of elements, shrinking the buffer via `realloc(v->items, v->size * sizeof *v->items)` reclaims memory. Avoid shrinking eagerly on every `pop`, as alternating between push and pop can cause pathological reallocation thrashing.
5. **Pass Double Pointers to Free**: Design cleanup functions as `void vector_free(Vector **v)` to automatically set the caller's pointer to `NULL`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **`v->data = realloc(v->data, ...)`** | If `realloc` returns `NULL`, the pointer is overwritten, leaking the existing memory block. | Always store result in a temporary pointer first (`tmp = realloc(...)`). |
| **Dangling pointers to internal elements** | Pointers to vector elements (`int *p = &v->data[0]`) are invalidated whenever reallocation moves the buffer. | Store indices (`size_t index`) instead of raw pointers. |
| **Linear capacity growth (`cap += 1`)** | Calling `realloc` on every insertion produces $O(N^2)$ quadratic copying overhead. | Use geometric scaling (`cap *= 2` or `cap = cap * 3 / 2`). |
| **Forgetting to check `realloc` return** | If the host is out of memory and `realloc` fails, proceeding to write causes `SIGSEGV`. | Verify `if (!tmp) { return ERROR; }`. |
| **Double free in nested struct vectors** | If a vector holds structs with their own dynamically allocated strings, calling `free(v->data)` leaks the strings. | Iterate through all elements and free their internal buffers before freeing the vector. |
| **Unchecked index access** | Accessing `v->data[i]` without validating `i < v->size` causes out-of-bounds reads/writes. | Provide a bounds-checked accessor function (`vector_get(v, i)`). |

---

## 8. Performance & Security Notes

### Amortized O(1) Proof
Let the cost of inserting an element without reallocation be 1 credit. When capacity doubles from $N$ to $2N$, copying the existing $N$ elements costs $N$ credits. By charging 3 credits per insertion:
- 1 credit pays for the immediate write.
- 2 credits are stored as savings to pay for copying this element and an older element during the next doubling.
Because every operation is bounded by a constant number of amortized credits, the amortized cost per insertion is strictly **$O(1)$**.

### Security: Integer Overflow during Capacity Doubling
On 32-bit systems, if capacity reaches $2^{30}$ elements of 4-byte integers, doubling capacity (`cap * 2`) computes $2^{31} \times 4 = 2^{33}$, which overflows `size_t` to 0. The program allocates 0 bytes, followed by writes that corrupt the heap. Defensive code must check:
```c
if (v->capacity > SIZE_MAX / (2 * sizeof(int))) {
    return 0; /* Refuse allocation before overflow */
}
```

---

## 9. Comparison: When to Use What

| Data Structure | Random Access | Push Back Time | Memory Overhead | Cache Locality |
| :--- | :--- | :--- | :--- | :--- |
| **Static Array** | $O(1)$ | $O(1)$ (Bounded) | 0 bytes | Perfect |
| **Dynamic Array (Vector)** | $O(1)$ | Amortized $O(1)$ | Unused capacity | Excellent (Contiguous) |
| **Singly Linked List** | $O(N)$ | $O(1)$ (with tail) | 1 pointer per item | Poor (Node hopping) |
| **Doubly Linked List** | $O(N)$ | $O(1)$ | 2 pointers per item | Poor (Fragmented) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does geometric growth guarantee amortized O(1) insertion time while arithmetic growth does not?</strong></summary>

With geometric growth (e.g., doubling), the cost of copying memory during expansion is distributed over an exponentially increasing number of cheap insertions. Inserting $N$ items copies at most $N/2 + N/4 + \dots \approx N$ total elements across all reallocations combined, yielding an average of $\approx 2$ copies per insertion ($O(1)$).
With arithmetic growth (e.g., $+10$), $N/10$ reallocations occur, copying $10 + 20 + 30 + \dots \approx N^2 / 20$ elements, degrading average insertion to $O(N)$.
</details>

<details>
<summary><strong>2. What is pointer invalidation in dynamic arrays?</strong></summary>

Pointer invalidation occurs when `realloc()` cannot expand the existing memory block in place and is forced to allocate a new block elsewhere in the heap, copying the data and freeing the old chunk. Any pointers or references previously pointing into the old block now point to deallocated memory (dangling pointers). Dereferencing them causes undefined behavior.
</details>

<details>
<summary><strong>3. What happens if <code>realloc()</code> fails to allocate the requested size?</strong></summary>

If `realloc()` fails due to memory exhaustion, it returns `NULL`. Crucially, **the original memory block passed as the first argument remains completely intact and allocated in the heap**. This is why assigning the result directly to the original pointer variable is a fatal bug.
</details>

<details>
<summary><strong>4. Why do some standard libraries use a growth factor of 1.5 instead of 2.0?</strong></summary>

When a buffer grows by $2.0\times$, the sum of all previously allocated and freed blocks ($\sum_{k=0}^{n-1} 2^k = 2^n - 1$) is always strictly less than the newly requested size ($2^n$). Therefore, the allocator can never reuse the memory footprint of previous allocations. With a growth factor $\le \frac{1 + \sqrt{5}}{2} \approx 1.618$ (the Golden Ratio), the allocator can eventually recycle contiguous previously freed chunks.
</details>

<details>
<summary><strong>5. How does a generic dynamic array work in C without C++ templates?</strong></summary>

In C, generic dynamic arrays are implemented either via:
1. `void *` element buffers: The vector stores element size `elem_size` and performs raw byte copying via `memcpy()` and `char *` arithmetic.
2. Preprocessor macro templates: Macro definitions that expand into strongly-typed vector implementations for a given type `T`.
</details>

<details>
<summary><strong>6. What is the "shrink-to-fit" operation and what are its risks?</strong></summary>

Shrink-to-fit calls `realloc(v->data, v->size * sizeof(T))` to release unused excess capacity back to the heap manager. The risk is that if new elements are subsequently added, the vector must immediately reallocate again, introducing performance jitter.
</details>

<details>
<summary><strong>7. How can you implement an O(1) remove operation if element order does not matter?</strong></summary>

Swap the element to be removed with the very last element in the array, then decrement the `size` counter:
`v->data[index] = v->data[--v->size];`. This avoids the expensive $O(N)$ shift operation.
</details>

<details>
<summary><strong>8. What is the memory footprint overhead of a typical dynamic array struct in C?</strong></summary>

A typical dynamic array header struct contains two `size_t` fields (`size` and `capacity`) and one pointer (`items`). On 64-bit systems, each field is 8 bytes, resulting in a 24-byte header overhead regardless of element count.
</details>

---

## 11. Practice Exercises

### Exercise 1: Bounded Vector Pop (Easy)
Add a function `bool vector_pop(IntVector *v, int *out_val)` that retrieves the last element, decrements size, and returns true, or returns false if the vector is empty.

<details>
<summary>View Solution</summary>

```c
#include <stdbool.h>

bool vector_pop(IntVector *v, int *out_val) {
    if (!v || v->size == 0) return false;
    v->size--;
    if (out_val) {
        *out_val = v->items[v->size];
    }
    return true;
}
```
</details>

### Exercise 2: Fast Unordered Remove (Medium)
Implement `bool vector_remove_unordered(IntVector *v, size_t index)` that removes an item in $O(1)$ time by overwriting it with the last element.

<details>
<summary>View Solution</summary>

```c
#include <stdbool.h>

bool vector_remove_unordered(IntVector *v, size_t index) {
    if (!v || index >= v->size) return false;
    v->items[index] = v->items[v->size - 1];
    v->size--;
    return true;
}
```
</details>

### Exercise 3: Generic Byte-Level Vector (Hard)
Implement a generic vector that can hold any data type `T` by storing `size_t elem_size`, using `memcpy` to push items, and returning `void *` pointers to elements.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t size;
    size_t capacity;
    size_t elem_size;
    char *data;
} GenericVector;

GenericVector* gvec_create(size_t elem_size, size_t cap) {
    if (cap == 0) cap = 4;
    GenericVector *v = malloc(sizeof *v);
    if (!v) return NULL;
    v->data = malloc(cap * elem_size);
    if (!v->data) { free(v); return NULL; }
    v->size = 0;
    v->capacity = cap;
    v->elem_size = elem_size;
    return v;
}

int gvec_push(GenericVector *v, const void *elem) {
    if (v->size >= v->capacity) {
        size_t new_cap = v->capacity * 2;
        char *tmp = realloc(v->data, new_cap * v->elem_size);
        if (!tmp) return 0;
        v->data = tmp;
        v->capacity = new_cap;
    }
    memcpy(v->data + (v->size * v->elem_size), elem, v->elem_size);
    v->size++;
    return 1;
}

void* gvec_get(const GenericVector *v, size_t index) {
    if (index >= v->size) return NULL;
    return v->data + (index * v->elem_size);
}

void gvec_free(GenericVector *v) {
    if (!v) return;
    free(v->data);
    free(v);
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Standard Vector Structure */
typedef struct {
    size_t size;
    size_t capacity;
    int *data;
} Vector;

/* Safe Reallocation */
size_t new_cap = v->capacity * 2;
int *tmp = realloc(v->data, new_cap * sizeof *tmp);
if (tmp) {
    v->data = tmp;
    v->capacity = new_cap;
}

/* Time Complexity */
Push Back:    Amortized O(1)
Pop Back:     O(1)
Random Get:   O(1)
Ordered Del:  O(N) (Requires memory shift)
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §7.22.3.5 "The realloc function".
- Introduction to Algorithms (CLRS), Chapter 17: "Amortized Analysis (Dynamic Tables)".
- Facebook Folly: *FBVector - The 1.5x Growth Factor Rationale*.
- Sean Barrett: *stb.h stretchy_buffer pattern*.

---

[← Previous: Multi-Dimensional Arrays](./13-multi-dimensional-arrays.md) | [Section Index](./README.md) | [Next: Safe String Handling & string.h →](./15-string-h-functions-and-safe-alternatives.md)
