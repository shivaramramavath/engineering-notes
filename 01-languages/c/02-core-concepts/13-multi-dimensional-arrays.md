# Multi-Dimensional Arrays: Layout, Strides, and Pointers

> Understanding 2D and N-dimensional arrays in C: row-major memory ordering, pointer-to-array decay, flat buffer striding, and passing matrices to functions.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Arrays](./01-arrays.md), [Pointers](./03-pointers.md), [Pointer Arithmetic](./12-pointer-arithmetic.md) |
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

Physical computer memory is strictly one-dimensional: a continuous linear array of bytes addressed from 0 up to the limits of virtual memory. However, systems programming problems often require multidimensional structures: mathematical matrices, image pixel grids, game boards, terrain heightmaps, and relational tables.

In C, a multi-dimensional array is literally an **"array of arrays"**. 
- A 2D array `int grid[3][4]` is an array of 3 elements, where each element is an array of 4 integers.
- Unlike languages like Java or Python which implement 2D arrays as arrays of independent pointers (jagged arrays), C allocates multi-dimensional arrays as **a single contiguous block of memory laid out in row-major order**.

Mastering multi-dimensional arrays requires understanding the precise mechanics of pointer decay: how `int grid[3][4]` decays not into `int **`, but into a pointer to an array of 4 integers: `int (*)[4]`.

---

## 2. Core Concepts

### Row-Major Ordering
In row-major ordering, the rightmost index varies fastest in linear memory. The elements of row 0 are placed consecutively, followed immediately by row 1, then row 2:
$$\text{Element } [r][c] \implies \text{Offset} = (r \times \text{COLS}) + c$$

### The Pointer Decay Rule for 2D Arrays
When an array name appears in an expression, it decays into a pointer to its first element. 
- For a 1D array `int a[10]`, its first element is `int`, so it decays into `int *`.
- For a 2D array `int m[3][4]`, its first element is `m[0]`, which is of type `int[4]`. Therefore, `m` decays into a **pointer to an array of 4 ints**:
  ```c
  int (*p)[4] = m; /* Legal and type-safe */
  int **bad = m;   /* COMPILE ERROR / INCOMPATIBLE TYPE */
  ```

### Fixed-Dimension vs. Variable-Length Array (VLA) Function Parameters
To pass a 2D array to a function, the column dimension must be known at compile time:
```c
void process_fixed(int mat[][4], size_t rows);
void process_ptr(int (*mat)[4], size_t rows); /* Exactly identical */
```
In C99 and onwards, Variable Length Array parameter syntax allows passing dynamic matrix dimensions:
```c
void process_vla(size_t rows, size_t cols, int mat[rows][cols]);
```

---

## 3. How It Works Internally

### Memory Layout Comparison: Contiguous 2D Matrix vs. Jagged Pointer Array

```
Contiguous 2D Array: int grid[3][4] (Single 48-byte allocation)
┌────────────────────────┬────────────────────────┬────────────────────────┐
│ Row 0: [ 1,  2,  3,  4]│ Row 1: [ 5,  6,  7,  8]│ Row 2: [ 9, 10, 11, 12]│
└────────────────────────┴────────────────────────┴────────────────────────┘
Offset: 0                16                       32                      48 bytes

Jagged Pointer Array: int *rows[3] (Dispersed memory blocks, pointer overhead)
┌───────────┐
│ rows[0]   │ ───► [ 1,  2,  3,  4 ] (Heap Chunk A)
├───────────┤
│ rows[1]   │ ───► [ 5,  6,  7,  8 ] (Heap Chunk B)
├───────────┤
│ rows[2]   │ ───► [ 9, 10, 11, 12 ] (Heap Chunk C)
└───────────┘
```

In a contiguous 2D array, accessing `grid[r][c]` emits a single memory load with base-plus-scaled-index addressing:
$$\text{Address} = \text{Base} + (r \times \text{COLS} \times 4) + (c \times 4)$$
In a jagged array, accessing `rows[r][c]` requires **two serialized memory dereferences**: first loading the pointer `rows[r]`, then loading the integer from that address, destroying CPU cache spatial locality.

---

## 4. Syntax & Usage

```c
#include <stdio.h>

#define ROWS 3
#define COLS 4

/* Receiving 2D array via pointer-to-array */
void print_2d(int (*mat)[COLS], size_t rows) {
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < COLS; ++c) {
            printf("%d ", mat[r][c]);
        }
        printf("\n");
    }
}

/* 1D Flat buffer indexing (Recommended for arbitrary dimensions) */
void set_element(int *flat_grid, size_t cols, size_t r, size_t c, int val) {
    flat_grid[r * cols + c] = val;
}
```

> [!NOTE]
> ### ISO C23 Matrix Initializers and VLAs
> - In ISO C23, empty brace initialization `int grid[3][4] = {};` cleanly zero-initializes the entire multidimensional structure.
> - While Variable Length Arrays (VLAs) with automatic storage duration on the stack remain optional (`__STDC_NO_VLA__`), VLA parameter syntax in function prototypes (variably modified types) is standard and widely supported.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates row-major contiguous memory verification, `sizeof` calculations across dimensions, flat pointer traversal, and matrix function parameter passing.

```c
#include <stdio.h>
#include <stddef.h>

#define ROWS 3
#define COLS 4

/* Function taking pointer to array of COLS integers */
static void print_matrix(int (*mat)[COLS], size_t rows) {
    for (size_t r = 0; r < rows; ++r) {
        printf("Row %zu: ", r);
        for (size_t c = 0; c < COLS; ++c) {
            printf("%2d ", mat[r][c]);
        }
        printf("\n");
    }
}

int main(void) {
    printf("=== C Multi-Dimensional Arrays Demo ===\n");

    /* 2D array row-major contiguous memory */
    int grid[ROWS][COLS] = {
        { 1,  2,  3,  4},
        { 5,  6,  7,  8},
        { 9, 10, 11, 12}
    };

    printf("Total matrix sizeof: %zu bytes (%zu elements * %zu bytes)\n",
           sizeof(grid), (size_t)(ROWS * COLS), sizeof(int));
    printf("Row 0 sizeof: %zu bytes\n", sizeof(grid[0]));

    /* Verify row-major contiguous layout via flat pointer */
    int *flat_ptr = &grid[0][0];
    printf("Flat traversal: ");
    for (size_t i = 0; i < ROWS * COLS; ++i) {
        printf("%d ", *(flat_ptr + i));
    }
    printf("\n");

    /* Matrix function call */
    print_matrix(grid, ROWS);

    return 0;
}
```

```text
=== C Multi-Dimensional Arrays Demo ===
Total matrix sizeof: 48 bytes (12 elements * 4 bytes)
Row 0 sizeof: 16 bytes
Flat traversal: 1 2 3 4 5 6 7 8 9 10 11 12 
Row 0:  1  2  3  4 
Row 1:  5  6  7  8 
Row 2:  9 10 11 12
```

---

## 6. Best Practices

1. **Prefer Flat 1D Buffers for Dynamic Multi-Dimensional Data**: Instead of allocating an array of row pointers (`malloc(rows * sizeof(int*))`), allocate a single contiguous block: `int *grid = malloc(rows * cols * sizeof *grid);`. Index it with `grid[r * cols + c]`. This prevents heap fragmentation and maximizes L1 cache hits.
2. **Always Parenthesize Pointers to Arrays**: `int (*p)[4]` is a pointer to an array of 4 integers. `int *p[4]` is an array of 4 pointers to integers. Forgetting parentheses drastically alters semantics.
3. **Traverse Loops in Row-Major Order**: Always nest loops with the row index outer and column index inner: `for (r...) for (c...)`. Traversing column-first strides across memory pages, causing massive CPU cache misses.
4. **Use `sizeof(grid) / sizeof(grid[0])` for Row Count**: To dynamically obtain the row dimension of a static 2D array, use `sizeof(grid) / sizeof(grid[0])`. For column count, use `sizeof(grid[0]) / sizeof(grid[0][0])`.
5. **Never Pass Multi-Dimensional Arrays as `int **`**: A contiguous 2D array is incompatible with a double pointer `int **`. Compilers emit a constraint violation error; forcing it with a cast causes runtime crashes.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Passing 2D array to `int **` parameter** | `int grid[3][4]` decays into `int (*)[4]`, not `int **`. Dereferencing `grid[r]` treats array data as pointers, crashing. | Declare parameter as `int (*mat)[COLS]` or flatten to `int *mat`. |
| **Column-major loop traversal** | `for (c) for (r) grid[r][c]`: Steps by row strides across memory, destroying cache locality. | Always iterate rows first: `for (r) for (c) grid[r][c]`. |
| **Confusing `int (*p)[N]` with `int *p[N]`** | `int *p[N]` is an array of pointers; `int (*p)[N]` is a pointer to an array. | Enclose pointer declarators in parentheses when declaring array pointers. |
| **Omitting column bound in parameters** | `void f(int mat[][])` is illegal; compiler needs the column count to compute memory offsets. | Specify all dimensions except the first: `void f(int mat[][4])`. |
| **Allocating jagged arrays and freeing in one call** | Freeing only the master pointer `free(rows)` leaves all individual row chunks leaked in heap. | Free every row in a loop before freeing the top-level pointer array. |
| **VLA stack overflow with large dimensions** | Declaring `int grid[1000][1000]` as a local VLA allocates 4 MB on the stack, crashing with `SIGSEGV`. | Allocate large matrices dynamically on the heap via `malloc`. |

---

## 8. Performance & Security Notes

### Cache Miss Penalty: Row-Major vs. Column-Major
Consider a matrix of $2048 \times 2048$ doubles ($\approx 33.5 \text{ MB}$, larger than CPU L3 cache):
```c
/* Row-major traversal: FAST (Spatial Locality) */
for (size_t r = 0; r < 2048; r++) {
    for (size_t c = 0; c < 2048; c++) {
        sum += mat[r][c]; /* Contiguous reads, 8 doubles per cache line */
    }
}

/* Column-major traversal: 10x - 20x SLOWER (Cache Thrashing) */
for (size_t c = 0; c < 2048; c++) {
    for (size_t r = 0; r < 2048; r++) {
        sum += mat[r][c]; /* Jumps 16 KB per read, every read misses cache! */
    }
}
```

### Security: Arithmetic Overflow in Flat Buffer Calculations
When calculating dynamic matrix allocation sizes `rows * cols * sizeof(T)`, integer multiplication can wrap around 32-bit limits. An attacker specifying `rows = 65536` and `cols = 65536` on 32-bit systems causes the byte calculation to wrap to 0, resulting in a tiny allocation followed by immediate heap exploitation. Always check for overflow.

---

## 9. Comparison: When to Use What

| Layout Strategy | Memory Layout | Allocation Count | Cache Efficiency | Sizing Flexibility |
| :--- | :--- | :--- | :--- | :--- |
| **Static 2D Array (`T[R][C]`)** | 100% Contiguous | 1 (Stack or Static) | Maximum | Fixed at compile time |
| **Flat Dynamic Buffer (`T*`)** | 100% Contiguous | 1 (`malloc`) | Maximum | Fully dynamic at runtime |
| **Jagged Pointer Array (`T**`)** | Fragmented Chunks | $R + 1$ (`malloc`s) | Poor (Pointer chase) | Rows can have varying lengths |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is <code>int **p</code> incompatible with <code>int a[3][4]</code>?</strong></summary>

`int a[3][4]` is a contiguous 48-byte memory block. It contains integers, not pointers. When passed to a function, `a` decays into `int (*)[4]`, which is a pointer to a 16-byte array.
`int **p` is a pointer to a pointer. Accessing `p[0][0]` instructs the CPU to load a memory address from the first 4/8 bytes of memory, then dereference that address. Since `a[0][0]` is an integer (e.g., `1`), the CPU attempts to dereference address `0x00000001`, causing an immediate segmentation fault.
</details>

<details>
<summary><strong>2. What does <code>sizeof(grid)</code> vs. <code>sizeof(grid[0])</code> vs. <code>sizeof(grid[0][0])</code> evaluate to?</strong></summary>

For `int grid[3][4]`:
- `sizeof(grid)`: Total size of the entire 2D array: $3 \times 4 \times 4 = 48$ bytes.
- `sizeof(grid[0])`: Size of a single row (an array of 4 ints): $4 \times 4 = 16$ bytes.
- `sizeof(grid[0][0])`: Size of a single integer element: 4 bytes.
</details>

<details>
<summary><strong>3. How do you declare a pointer to a 3D array of dimensions 2x3x4?</strong></summary>

A 3D array `int arr[2][3][4]` decays into a pointer to its first element, which is a 2D array of dimensions 3x4:
`int (*p)[3][4] = arr;`
</details>

<details>
<summary><strong>4. Why must the column dimension be specified when passing a 2D array to a function?</strong></summary>

The compiler needs the column dimension to calculate memory offsets for row indexing: $\text{Offset} = (r \times \text{COLS}) + c$. Without knowing $\text{COLS}$, the compiler cannot generate the multiplication instruction needed to locate row $r$.
</details>

<details>
<summary><strong>5. What is the difference between <code>int *p[10]</code> and <code>int (*p)[10]</code>?</strong></summary>

Due to operator precedence, brackets `[]` have higher precedence than the dereference operator `*`.
- `int *p[10]`: An array of 10 pointers to integers.
- `int (*p)[10]`: A pointer to an array of 10 integers.
</details>

<details>
<summary><strong>6. How can you allocate a dynamically-sized 2D matrix in contiguous heap memory while still using <code>mat[r][c]</code> syntax?</strong></summary>

Allocate an array of row pointers `malloc(rows * sizeof(int*))` and a contiguous data buffer `malloc(rows * cols * sizeof(int))`. Then assign `row_ptrs[r] = data_buffer + (r * cols)`. This provides $O(1)$ contiguous cache locality while enabling standard `mat[r][c]` bracket notation.
</details>

<details>
<summary><strong>7. What happens if you initialize a 2D array with fewer elements than its capacity?</strong></summary>

In ISO C, if an initializer list contains fewer elements than the array dimensions, all remaining uninitialized elements are guaranteed to be initialized to zero (`0`).
</details>

<details>
<summary><strong>8. What is a "ragged" or "jagged" array and when is it preferred over a rectangular matrix?</strong></summary>

A jagged array is an array of pointers where each row can have a different length (e.g., storing lines of text or words of variable lengths). It is preferred when row lengths vary drastically, preventing wasted memory from padding all rows to the maximum length.
</details>

---

## 11. Practice Exercises

### Exercise 1: Matrix Transposition (Easy)
Write a function `void transpose_3x3(int dst[3][3], const int src[3][3])` that transposes a 3x3 matrix such that `dst[c][r] = src[r][c]`.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>

void transpose_3x3(int dst[3][3], const int src[3][3]) {
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            dst[c][r] = src[r][c];
        }
    }
}
```
</details>

### Exercise 2: Contiguous Dynamic Matrix Creator (Medium)
Implement a dynamic matrix allocator that creates an $R \times C$ matrix in a single contiguous allocation and sets up an array of row pointers, allowing `m[r][c]` syntax. Include a cleanup function.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    size_t rows;
    size_t cols;
    int **data;
} Matrix;

Matrix* matrix_create(size_t r, size_t c) {
    Matrix *m = malloc(sizeof *m);
    if (!m) return NULL;
    m->rows = r;
    m->cols = c;

    m->data = malloc(r * sizeof(int *));
    int *pool = malloc(r * c * sizeof(int));
    if (!m->data || !pool) {
        free(m->data);
        free(pool);
        free(m);
        return NULL;
    }

    for (size_t i = 0; i < r; i++) {
        m->data[i] = pool + (i * c);
    }
    return m;
}

void matrix_free(Matrix *m) {
    if (!m) return;
    if (m->data) {
        free(m->data[0]); /* Free contiguous pool */
        free(m->data);    /* Free row pointers */
    }
    free(m);
}
```
</details>

### Exercise 3: Safe Flat-Buffer Matrix Multiplication (Hard)
Implement `void matmul_flat(const double *a, const double *b, double *c, size_t M, size_t K, size_t N)` which multiplies matrix $A (M \times K)$ by matrix $B (K \times N)$ yielding $C (M \times N)$ using flat 1D buffer indexing.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>

void matmul_flat(const double *a, const double *b, double *c, size_t M, size_t K, size_t N) {
    for (size_t i = 0; i < M; i++) {
        for (size_t j = 0; j < N; j++) {
            double sum = 0.0;
            for (size_t p = 0; p < K; p++) {
                sum += a[i * K + p] * b[p * N + j];
            }
            c[i * N + j] = sum;
        }
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Declarations */
int grid[3][4];          /* 3 rows, 4 cols contiguous */
int (*p)[4] = grid;      /* Pointer to array of 4 ints */

/* Striding */
int *flat = &grid[0][0];
flat[r * COLS + c];      /* Direct offset calculation */

/* Function prototypes */
void f(int mat[][4], size_t rows);       /* Fixed cols */
void f_vla(size_t r, size_t c, int m[r][c]); /* C99 VLA */
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §6.7.6.2 "Array declarators".
- ISO/IEC 9899:2024 (C23 Standard), §6.7.6.2 "Array declarators".
- *Expert C Programming* by Peter van der Linden (Chapter 9: "More about Arrays").
- Ulrich Drepper: *What Every Programmer Should Know About Memory* (Cache-friendly data layout).

---

[← Previous: Pointer Arithmetic](./12-pointer-arithmetic.md) | [Section Index](./README.md) | [Next: Dynamic Arrays & Realloc →](./14-dynamic-arrays-and-realloc.md)
