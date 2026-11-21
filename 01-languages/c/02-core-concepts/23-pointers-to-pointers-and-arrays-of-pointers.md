# Pointers to Pointers and Arrays of Pointers

> Multi-level pointer indirection in C: pointer-to-pointer out parameters, dynamic matrix indirection, ragged string tables, and pointer sorting.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Pointers](./03-pointers.md), [Arrays](./01-arrays.md), [Memory Management](./08-memory-management.md) |
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

In C, all function arguments are passed **strictly by value**. When a function receives an argument, it receives a private local copy.
- If you want a function to modify an `int` variable belonging to the caller, you pass a pointer to that int: `int *`.
- By identical logic, if you want a function to modify a **pointer** variable belonging to the caller (such as updating where it points, allocating heap memory, or updating a linked list head), you must pass a **pointer to the pointer**: `T **`.

Beyond modifying caller pointers, multi-level indirection is the architectural foundation for:
1. **Dynamic Ragged Arrays**: Representing lists of strings of varying lengths (such as command-line `char *argv[]`).
2. **High-Performance Indirect Sorting**: Sorting an array of pointers to large structures rather than copying megabytes of struct data across memory.

---

## 2. Core Concepts

### Pointer-to-Pointer Indirection Mechanics
A pointer-to-pointer `int **pp` holds the memory address of an `int *` pointer variable:
```c
int val = 42;
int *p = &val;    /* p holds address of val */
int **pp = &p;    /* pp holds address of p */

**pp = 99;        /* Dereferences twice -> updates val to 99 */
```

### Out-Parameters for Memory Allocation
When a function allocates dynamic memory on behalf of a caller, returning the pointer by value is common (`int *p = malloc(...)`). However, if the function's return value is reserved for an error status code (`int` or `bool`), the destination pointer must be passed as an out-parameter `T **`:
```c
int allocate_buffer(uint8_t **out_buf, size_t size) {
    *out_buf = malloc(size);
    return (*out_buf != NULL) ? 0 : -1;
}
```

### Arrays of Pointers (Ragged Tables)
An array of pointers `char *table[]` stores multiple addresses, each pointing to an independently allocated, variable-length string:
```c
const char *fruits[] = {
    "Apple",
    "Watermelon",
    "Fig"
};
```
Unlike a 2D rectangular matrix `char fruits[3][11]` where every row wastes memory padding up to the longest word (11 bytes), an array of pointers allocates only the exact bytes required for each string.

---

## 3. How It Works Internally

### Memory Layout Comparison: Pointer-to-Pointer vs. Contiguous 2D Array

```
1. Array of Pointers: char *cmds[3] (Dispersed memory blocks, pointer overhead)
 Stack Segment                           Virtual Memory (.rodata or Heap)
┌───────────┐
│ cmds[0]   │ ────────────────────────► "help\0" (5 bytes)
├───────────┤
│ cmds[1]   │ ────────────────────────► "connect --host localhost\0" (26 bytes)
├───────────┤
│ cmds[2]   │ ────────────────────────► "exit\0" (5 bytes)
└───────────┘
Total: 3 pointers (24 bytes) + 36 bytes string data

2. Contiguous 2D Array: char cmds[3][26] (Single rectangular block)
┌──────────────────────────┬──────────────────────────┬──────────────────────────┐
│ "help\0" + 21 PAD BYTES  │ "connect --host ...\0"   │ "exit\0" + 21 PAD BYTES  │
└──────────────────────────┴──────────────────────────┴──────────────────────────┘
Total: 78 bytes contiguous (42 bytes wasted padding!)
```

---

## 4. Syntax & Usage

```c
#include <stdio.h>
#include <stdlib.h>

/* Updating caller head pointer in linked list */
typedef struct Node {
    int val;
    struct Node *next;
} Node;

void push_front(Node **head_ref, int new_val) {
    Node *n = malloc(sizeof *n);
    if (!n) return;
    n->val = new_val;
    n->next = *head_ref;
    *head_ref = n; /* Modifies caller's head variable directly */
}
```

> [!NOTE]
> ### Multi-Level Const Qualification
> Qualifying double pointers requires careful attention:
> - `const char **p`: Pointer to a pointer to `const char`. The characters are constant; the pointers are mutable.
> - `char * const *p`: Pointer to a `const` pointer to mutable `char`.
> - `const char * const * const p`: Fully immutable double pointer.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates pointer-to-pointer out parameter memory allocation and iterating over an array of pointers (command table).

```c
#include <stdio.h>
#include <stdlib.h>

/* Modifying caller pointer via pointer-to-pointer out parameter */
static int allocate_string(char **out_str, size_t len) {
    if (!out_str) return 0;
    *out_str = malloc(len);
    return (*out_str != NULL);
}

int main(void) {
    printf("=== C Pointers to Pointers & Arrays of Pointers Demo ===\n");

    /* 1. Pointer-to-pointer mutation */
    char *msg = NULL;
    if (allocate_string(&msg, 32)) {
        sprintf(msg, "Allocated via double pointer");
        printf("msg: %s\n", msg);
        free(msg);
    }

    /* 2. Array of pointers (ragged string table) */
    const char *commands[] = {
        "help",
        "connect --host localhost",
        "status",
        "exit"
    };
    size_t cmd_count = sizeof(commands) / sizeof(commands[0]);
    printf("Command table (%zu entries):\n", cmd_count);
    for (size_t i = 0; i < cmd_count; ++i) {
        printf("  [%zu] %s\n", i, commands[i]);
    }

    return 0;
}
```

```text
=== C Pointers to Pointers & Arrays of Pointers Demo ===
msg: Allocated via double pointer
Command table (4 entries):
  [0] help
  [1] connect --host localhost
  [2] status
  [3] exit
```

---

## 6. Best Practices

1. **Always Verify `out_ptr != NULL` Before Dereferencing**: In out-parameter functions, check `if (!out_ptr) return ERROR;` to prevent crashing when callers pass a null pointer.
2. **Use Pointer Arrays to Avoid Moving Heavy Structs During Sorting**: When sorting 10,000 instances of a 512-byte struct, sort an array of `struct S *` pointers. Swapping 8-byte pointers is orders of magnitude faster than copying 512-byte records.
3. **Pass Double Pointers to Free Routines to Nullify Callers**: Implement `void safe_free(void **p)` so that freeing memory automatically sets the caller's pointer to `NULL`.
4. **Use Sentinel `NULL` Pointers for Dynamic Pointer Arrays**: Terminating dynamic pointer arrays with `NULL` (like `argv`) allows iterating without passing a separate count parameter.
5. **Never Confuse `T**` with `T[][]`**: A contiguous 2D array is fundamentally incompatible with a pointer-to-pointer.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Passing single pointer expecting mutation** | `void alloc(char *p) { p = malloc(...); }` modifies local copy; caller's pointer remains unchanged. | Pass double pointer: `void alloc(char **p) { *p = malloc(...); }`. |
| **Forgetting to dereference `*head_ref`** | Writing `head_ref = n` overwrites the local pointer to pointer instead of caller's variable. | Dereference: `*head_ref = n`. |
| **Passing 2D array to `char **`** | `char table[3][10]` is contiguous memory; passing to `char **` causes crash on dereference. | Use array pointer `char (*table)[10]` or array of pointers. |
| **Freeing top-level pointer array without freeing elements** | Freeing only `free(ptrs)` leaves all individual allocated elements leaked in heap. | Loop through and free each element before freeing the pointer array. |
| **Implicit cast from `char **` to `const char **`** | ISO C forbids this without explicit cast because it can silently bypass const correctness. | Use explicit cast or declare compatible const signatures. |
| **Dangling double pointer after free** | Freeing `*pp` leaves `*pp` dangling; subsequent dereferences trigger UAF. | Set `*pp = NULL;` immediately after `free(*pp)`. |

---

## 8. Performance & Security Notes

### Cache Miss Penalty of Pointer Tables
While ragged arrays of pointers save memory for variable-length strings, traversing `char *table[1000]` causes pointer-chasing cache misses because every string lives in an arbitrary heap or text location. In high-performance text search engines, a single contiguous buffer with an array of 32-bit offsets is significantly faster.

### Security: Double Pointer Corruption in Heap Exploits
Pointer-to-pointer structures frequently store function pointers or dynamic dispatch tables (vables). In heap buffer overflow exploits, overwriting an address stored inside a pointer-to-pointer allows attackers to hijack control flow upon the subsequent dereference.

---

## 9. Comparison: When to Use What

| Technique | Memory Structure | Indirection Levels | Memory Overhead | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **Single Pointer (`T*`)** | 1D Buffer | 1 | 0 extra | Arrays, single buffers |
| **Double Pointer (`T**`)** | Pointer to pointer variable | 2 | 1 pointer address | Out-parameters, list mutation |
| **Array of Pointers (`T*[]`)** | Dispersed chunks | 2 | 1 pointer per item | Ragged string lists, sorting indexes |
| **2D Array (`T[][]`)** | Contiguous grid | 1 | Padding bytes | Fixed mathematical matrices |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why must a function take <code>Node **head</code> instead of <code>Node *head</code> to insert at the beginning of a linked list?</strong></summary>

C passes arguments by value. If the function accepts `Node *head`, it receives a copy of the pointer address. Modifying `head = new_node` only changes the local copy inside the function; the caller's `head` variable remains unchanged. Passing `Node **head` allows the function to dereference the address (`*head = new_node`) and update the caller's actual variable.
</details>

<details>
<summary><strong>2. Why is converting <code>char **</code> to <code>const char **</code> invalid without a cast in C?</strong></summary>

In ISO C (§6.5.16.1), permitting implicit conversion from `char **` to `const char **` would allow code to modify `const` data without a cast:
```c
const char c = 'A';
char *p;
const char **pp = &p; /* If this were legal... */
*pp = &c;             /* Valid: pp points to const char* */
*p = 'B';              /* Mutates const char c! */
```
To prevent this loophole, the standard requires explicit casts.
</details>

<details>
<summary><strong>3. What is the difference in memory layout between <code>char *a[]</code> and <code>char a[][10]</code>?</strong></summary>

`char *a[]` is an array of pointers; each element is an 8-byte pointer addressing memory elsewhere in the address space.
`char a[][10]` is a flat contiguous 2D grid of characters; each row is stored consecutively with zero pointer indirection.
</details>

<details>
<summary><strong>4. How do you implement a universal safe free function using double pointers?</strong></summary>

```c
void safe_free(void **ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}
```
</details>

<details>
<summary><strong>5. What is the type of <code>argv</code> in <code>int main(int argc, char *argv[])</code>?</strong></summary>

Due to array decay in parameter lists, `char *argv[]` is strictly equivalent to `char **argv`—a pointer to a pointer to `char`.
</details>

<details>
<summary><strong>6. Can you have triple pointers (<code>T***</code>) in C?</strong></summary>

Yes. An arbitrary number of indirection levels (`***`, `****`) is supported by the standard. A triple pointer `char ***` is commonly used when dynamically allocating an array of strings inside a function.
</details>

<details>
<summary><strong>7. How does indirect sorting work using an array of pointers?</strong></summary>

Instead of moving large records in memory during sorting algorithms, an array of pointers pointing to each record is constructed. The sort algorithm compares records via pointers and swaps only the 8-byte pointers, reducing memory write bus traffic by orders of magnitude.
</details>

<details>
<summary><strong>8. What is a sentinel pointer in an array of pointers?</strong></summary>

A sentinel pointer is a `NULL` pointer placed as the final element of a pointer array (e.g., `argv[argc] == NULL`). It allows iteration functions to detect the end of the array without needing an explicit size parameter.
</details>

---

## 11. Practice Exercises

### Exercise 1: Universal Pointer Nullifying Free (Easy)
Write a macro or function `SAFE_FREE(ptr)` that frees a pointer and sets it to `NULL`.

<details>
<summary>View Solution</summary>

```c
#include <stdlib.h>

#define SAFE_FREE(p) do { free(p); (p) = NULL; } while (0)
```
</details>

### Exercise 2: Dynamic String Array Builder (Medium)
Write a function `int add_string(char ***array, size_t *count, const char *new_str)` that dynamically expands an array of strings, appending `strdup(new_str)`.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int add_string(char ***array, size_t *count, const char *new_str) {
    size_t new_count = *count + 1;
    char **tmp = realloc(*array, new_count * sizeof(char *));
    if (!tmp) return 0;

    size_t len = strlen(new_str) + 1;
    tmp[*count] = malloc(len);
    if (!tmp[*count]) return 0;
    memcpy(tmp[*count], new_str, len);

    *array = tmp;
    *count = new_count;
    return 1;
}
```
</details>

### Exercise 3: Indirect Struct Sorter (Hard)
Given an array of 5 heavy records `struct Student { char name[64]; int grade; };`, create an array of pointers to them and sort the pointers in descending order of grade using `qsort`.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char name[64];
    int grade;
} Student;

int compare_students(const void *a, const void *b) {
    const Student *s1 = *(const Student **)a;
    const Student *s2 = *(const Student **)b;
    return s2->grade - s1->grade; /* Descending */
}

int main(void) {
    Student roster[3] = {
        {"Alice", 85},
        {"Bob", 95},
        {"Charlie", 72}
    };

    Student *ptrs[3] = { &roster[0], &roster[1], &roster[2] };
    qsort(ptrs, 3, sizeof(Student *), compare_students);

    printf("Ranked Students:\n");
    for (int i = 0; i < 3; i++) {
        printf("  %s: %d\n", ptrs[i]->name, ptrs[i]->grade);
    }
    return 0;
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Double pointer declaration */
int **pp = &p;
**pp = 10;           /* Double dereference */

/* Allocation Out-Parameter */
int alloc(char **out, size_t n) {
    *out = malloc(n);
    return *out != NULL;
}

/* Array of pointers */
const char *table[] = { "str1", "str2", NULL };
for (const char **p = table; *p; p++) {
    printf("%s\n", *p);
}
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §6.7.6.1 "Pointer declarators".
- *The C Programming Language* (2nd ed.) by Kernighan & Ritchie (Chapter 5: "Pointers and Arrays", Pointers to Pointers).
- Peter van der Linden: *Expert C Programming* (Chapter 10: "More About Pointers").
- SEI CERT C Coding Standard: "MEM34-C: Only free memory allocated dynamically".

---

[← Previous: Restrict & Aliasing](./22-restrict-and-aliasing-intro.md) | [Section Index](./README.md) | [Next: Complex Declarations Reading Guide →](./24-complex-declarations-reading-guide.md)
