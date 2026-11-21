# 13 Opaque Pointers & Abstract Data Types (ADTs)

> Master information hiding, encapsulation, incomplete struct types, and ABI stability in C using opaque pointers and Abstract Data Types.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [02-core-concepts/03-pointers.md](../02-core-concepts/03-pointers.md), [02-core-concepts/06-structs-unions-enums.md](../02-core-concepts/06-structs-unions-enums.md), [03-intermediate/02-multi-file-programs.md](02-multi-file-programs.md) |
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

In object-oriented languages, encapsulation is enforced by language keywords like `private` and `protected`. In C, structs declared in public header files expose all their member variables to client code. If a client accesses `struct_ptr->internal_buffer` directly:
- **Tight Coupling**: Any future change to the struct's internal layout (such as replacing an array with a linked list or changing a field type) breaks client code.
- **ABI Fragility**: Changing the size or alignment of a struct changes its byte layout. Any dynamically linked application compiled against the old header will crash due to mismatched memory offsets when linked against the new library.
- **Invariant Corruption**: Callers can bypass validation logic and mutate state directly, leading to race conditions and memory corruption.

C achieves true encapsulation and information hiding through **Opaque Pointers** (also known as the "Cheshire Cat" or "PIMPL - Pointer to Implementation" idiom). By declaring an **incomplete struct type** in the public header and defining its full contents strictly inside a private `.c` translation unit, client code can only manipulate the object through authorized API functions.

---

## 2. Core Concepts

### 2.1 Complete vs Incomplete Types
- **Complete Type**: A type whose size and memory layout are fully known to the compiler (e.g. `int`, `double`, or a fully defined `struct Point { int x, y; };`). The compiler can allocate space on the stack or compute `sizeof()`.
- **Incomplete Type**: A type whose name is known, but whose size and internal members have not yet been defined (e.g. `struct DatabaseConnection;`).
  - You **can** declare pointers to incomplete types (`struct DatabaseConnection *db;`) because all data pointers have the identical machine width on a given architecture (4 bytes on 32-bit, 8 bytes on 64-bit).
  - You **cannot** allocate an incomplete type on the stack, dereference it (`db->socket`), or compute its size (`sizeof(*db)`).

### 2.2 The Abstract Data Type (ADT) Pattern
An ADT is a mathematical model for data types where the type is defined solely by the operations that can be performed on it, rather than its concrete memory layout. In C, an ADT consists of:
1. An opaque typedef in the public header: `typedef struct IntStack IntStack;`.
2. A constructor creating the object on the heap: `IntStack *intstack_create(size_t cap);`.
3. A destructor releasing resources: `void intstack_destroy(IntStack *s);`.
4. Operations acting upon the opaque handle: `bool intstack_push(...)`, `bool intstack_pop(...)`.

### 2.3 Real-World Opaque Handles
The standard C library and operating system APIs rely heavily on opaque pointers:
- Standard I/O: `FILE *` is an opaque pointer in most standard implementations; callers never access the underlying file descriptor or stream buffer directly.
- POSIX: `pthread_t`, `regex_t`, `DIR *` (from `opendir()`).
- Windows API: `HANDLE`, `HWND`, `HDC` are opaque integer handles wrapping internal kernel objects.

---

## 3. How It Works Internally

### Compilation Boundaries & Memory Layout
When client code includes the public header, the compiler only knows that `IntStack` is a struct tag. The compiler cannot inspect or emit instructions accessing internal fields.

```
Compilation Separation:
Public Header (stack.h):
┌───────────────────────────────────────┐
│ typedef struct IntStack IntStack;     │
│ IntStack *intstack_create(size_t cap);│
│ bool intstack_push(IntStack *s, int); │
└───────────────────────────────────────┘
                   ▲
                   │ #include
┌──────────────────┴────────────────────┐   ┌───────────────────────────────────┐
│ Client Code (main.c):                 │   │ Implementation (stack.c):         │
│ IntStack *s = intstack_create(10);    │   │ struct IntStack {                 │
│ intstack_push(s, 42);                 │   │     int *data;                    │
│ // s->size = 5;  <-- COMPILE ERROR!   │   │     size_t size, capacity;        │
│ // sizeof(*s);   <-- COMPILE ERROR!   │   │ };                               │
└───────────────────────────────────────┘   └───────────────────────────────────┘
```

Because client code only holds an 8-byte pointer (`IntStack *s`), the library maintainer can add fields, change buffer types, or add synchronization mutexes inside `stack.c` without altering the size or calling convention of `IntStack *`. Client binaries do not need to be recompiled!

---

## 4. Syntax & Usage

### 4.1 The Canonical Header / Implementation Layout
**Public Header (`stack.h`):**
```c
#ifndef STACK_H
#define STACK_H

#include <stddef.h>
#include <stdbool.h>

/* Opaque struct declaration */
typedef struct IntStack IntStack;

/* Lifecycle APIs */
IntStack *intstack_create(size_t initial_cap);
void intstack_destroy(IntStack *stack);

/* Accessor and mutator operations */
bool intstack_push(IntStack *stack, int val);
bool intstack_pop(IntStack *stack, int *out_val);
size_t intstack_size(const IntStack *stack);

#endif
```

**Private Implementation (`stack.c`):**
```c
#include "stack.h"
#include <stdlib.h>

struct IntStack {
    int *data;
    size_t size;
    size_t capacity;
};

IntStack *intstack_create(size_t initial_cap) {
    IntStack *s = malloc(sizeof(IntStack));
    if (!s) return NULL;
    s->data = malloc(initial_cap * sizeof(int));
    if (!s->data) {
        free(s);
        return NULL;
    }
    s->size = 0;
    s->capacity = initial_cap;
    return s;
}

void intstack_destroy(IntStack *stack) {
    if (!stack) return;
    free(stack->data);
    free(stack);
}
```

---

## 5. Practical Examples

### Comprehensive Example: Encapsulated Dynamic Stack ADT
The following program demonstrates full encapsulation of a dynamic stack data structure using opaque pointers and defensive lifecycle management.

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* --- Public Header Interface Simulation --- */
typedef struct IntStack IntStack; /* Opaque handle */

IntStack *intstack_create(size_t initial_capacity);
void intstack_destroy(IntStack *stack);
bool intstack_push(IntStack *stack, int value);
bool intstack_pop(IntStack *stack, int *out_value);
size_t intstack_size(const IntStack *stack);

/* --- Private Implementation Simulation --- */
struct IntStack {
    int *data;
    size_t size;
    size_t capacity;
};

IntStack *intstack_create(size_t initial_capacity) {
    if (initial_capacity == 0) initial_capacity = 4;
    IntStack *s = (IntStack *)malloc(sizeof(IntStack));
    if (!s) return NULL;

    s->data = (int *)malloc(initial_capacity * sizeof(int));
    if (!s->data) {
        free(s);
        return NULL;
    }
    s->size = 0;
    s->capacity = initial_capacity;
    return s;
}

void intstack_destroy(IntStack *stack) {
    if (!stack) return;
    free(stack->data);
    free(stack);
}

bool intstack_push(IntStack *stack, int value) {
    if (!stack) return false;
    if (stack->size == stack->capacity) {
        size_t new_cap = stack->capacity * 2;
        int *new_data = (int *)realloc(stack->data, new_cap * sizeof(int));
        if (!new_data) return false;
        stack->data = new_data;
        stack->capacity = new_cap;
    }
    stack->data[stack->size++] = value;
    return true;
}

bool intstack_pop(IntStack *stack, int *out_value) {
    if (!stack || stack->size == 0) return false;
    *out_value = stack->data[--stack->size];
    return true;
}

size_t intstack_size(const IntStack *stack) {
    return stack ? stack->size : 0;
}

int main(void) {
    printf("=== Opaque Pointers and ADT Encapsulation ===\n");

    IntStack *stack = intstack_create(2);
    if (!stack) {
        fprintf(stderr, "Failed to create stack\n");
        return 1;
    }

    printf("Pushing elements: 10, 20, 30...\n");
    intstack_push(stack, 10);
    intstack_push(stack, 20);
    intstack_push(stack, 30); /* Causes dynamic reallocation */

    printf("Stack size: %u\n", (unsigned int)intstack_size(stack));

    int popped = 0;
    while (intstack_pop(stack, &popped)) {
        printf("Popped: %d\n", popped);
    }

    intstack_destroy(stack);
    printf("Stack successfully destroyed without exposing internal fields.\n");
    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 13-opaque-pointers-and-adts.c -o 13-opaque-pointers-and-adts.exe
./13-opaque-pointers-and-adts.exe
```

```text
=== Opaque Pointers and ADT Encapsulation ===
Pushing elements: 10, 20, 30...
Stack size: 3
Popped: 30
Popped: 20
Popped: 10
Stack successfully destroyed without exposing internal fields.
```

---

## 6. Best Practices

1. **Always Provide a Safe Destroyer Function**: Every opaque type allocated by a constructor (`create`) must have a matching destructor (`destroy`) that accepts `NULL` gracefully (similar to standard `free`).
2. **Const-Qualify Read-Only Handles**: Functions that only inspect the ADT without mutating it should accept `const OpaqueType *handle`.
3. **Never Expose Internal Buffer Pointers**: If the ADT needs to return string data, return a copied buffer or an immutable view. Returning a pointer directly to an internal mutable array breaks encapsulation.
4. **Enforce Dynamic Allocation in the Library**: Do not allow callers to allocate memory for opaque types. The library's constructor must allocate the memory to ensure memory layout compatibility across compilation boundaries.
5. **Use Defensive Null Checks**: At the beginning of every public API function, assert or verify that the passed handle is non-null.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| `sizeof(IntStack)` in client code | Incomplete types have no known size. Triggers compiler error: `invalid application of 'sizeof' to incomplete type`. | Only use `sizeof` on pointers (`sizeof(IntStack *)`) or inside the implementation file. |
| Stack allocation: `IntStack s;` | The compiler cannot allocate stack frames for structs whose byte size is unknown. | Opaque ADTs must be allocated via heap constructor functions. |
| Forgetting `destroy()` | Because clients cannot inspect struct internals, they cannot free inner allocations; leads to severe memory leaks. | Always invoke the corresponding `destroy()` function. |
| Exposing struct definition in header | Defeats information hiding; clients become coupled to concrete fields, destroying ABI stability. | Keep struct definition strictly in private `.c` file. |
| Passing uninitialized handle to destructor | Passing garbage stack pointers to `destroy()` causes invalid pointer free crashes. | Initialize handles to `NULL`: `IntStack *s = NULL;`. |
| Modifying handle after destruction | Classic Use-After-Free (UAF). Caller can attempt to push to a freed stack. | Set `s = NULL;` immediately after `destroy(s)`. |

---

## 8. Performance & Security Notes

- **Heap Indirection Overhead**: Because opaque structs cannot be allocated on the stack, every instance requires heap allocation via `malloc()`. This adds memory allocator overhead and causes pointer chasing (cache misses) during dereferences.
- **Inability to Inline**: Compilers cannot inline function calls that manipulate opaque structs across different translation units unless Link-Time Optimization (LTO / `-flto`) is enabled.
- **Security Isolation**: Opaque pointers protect internal state from tampering. Attackers cannot directly overwrite function pointers, buffer limits, or state flags residing inside the opaque struct without triggering memory segmentation faults.

---

## 9. Comparison / When to Use What

| Architecture Pattern | Encapsulation | Stack Allocatable? | ABI Stability | Performance |
| :--- | :--- | :--- | :--- | :--- |
| **Opaque Pointers (ADTs)** | Complete | No (Heap only) | Maximum (Fields can change freely) | Moderate (Pointer chasing) |
| **Public Structs** | Zero | Yes (Fast stack memory) | Poor (Any field change breaks ABI) | Maximum (Direct register access) |
| **`void *` Untyped Handles** | Complete | No | Maximum | Low (Lacks compile-time type safety) |
| **C++ Classes (`private:`)** | Language enforced | Yes | Poor (Header changes trigger recompile) | High (Inlined methods) |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is an incomplete type in C, and what operations are permitted on it?</strong></summary>

An incomplete type is a type that describes an identifier but lacks the information needed to determine its size or memory layout (e.g. `struct Node;`). Permitted operations include: declaring pointers to the type (`struct Node *p;`), declaring functions that take or return pointers to the type, and using the pointer in comparisons (`p == NULL`). Forbidden operations include: declaring an actual instance on the stack (`struct Node n;`), computing its size (`sizeof(struct Node)`), performing pointer arithmetic (`p++`), and accessing members (`p->val`).
</details>

<details>
<summary><strong>2. How do opaque pointers provide Application Binary Interface (ABI) stability?</strong></summary>

When a library exposes public structs in headers, the client application compiles machine instructions hardcoding the byte offsets of struct fields and the total struct size. If a new version of the library adds a field, the offsets change, and old client binaries crash. With opaque pointers, client code only interacts with fixed-width pointer addresses (`void *` or `struct Opaque *`). All field offsets and allocations occur inside the library's compiled `.so` or `.dll`, allowing internal layouts to change without breaking existing client binaries.
</details>

<details>
<summary><strong>3. Why can't an opaque struct be allocated as a local variable on the stack?</strong></summary>

When a compiler generates code for a function, it calculates the total stack frame size needed for all local variables. Because the declaration of an opaque struct in a header does not include its member definitions, the compiler cannot determine how many bytes to allocate on the stack. Attempting to allocate an incomplete struct directly triggers a compilation error.
</details>

<details>
<summary><strong>4. How does the standard library's `FILE` type demonstrate the opaque pointer pattern?</strong></summary>

In `<stdio.h>`, `FILE` is an opaque struct tag (`typedef struct _IO_FILE FILE;` in glibc). Developers open files with `fopen()` which returns a `FILE *`, and perform operations via `fread()`, `fwrite()`, and `fclose()`. The client never inspects the file descriptor integer, stream position, or write buffer directly, ensuring that changes to the C runtime's I/O subsystems never break application source code.
</details>

<details>
<summary><strong>5. What is the performance cost of using opaque pointers compared to exposed structs?</strong></summary>

Opaque pointers incur two performance costs:
1. Every instance must be dynamically allocated on the heap, increasing allocation and deallocation latency compared to instantaneous stack frame allocation.
2. Every field access requires an extra level of pointer indirection (pointer chasing), which can degrade CPU data cache locality. Additionally, cross-file function calls cannot be inlined without Link-Time Optimization (LTO).
</details>

<details>
<summary><strong>6. Can an opaque pointer be cast to `void *`? Is it recommended?</strong></summary>

Yes, any data pointer in C can be implicitly converted to and from `void *`. However, using `typedef struct Opaque Opaque;` is strongly preferred over raw `void *` because it preserves compile-time type safety. If an API accepts `void *`, passing a `DatabaseConnection *` to a function expecting a `SocketHandle *` compiles without warnings, leading to runtime crashes. An incomplete struct pointer prevents accidental type mismatches at compile time.
</details>

<details>
<summary><strong>7. How can Link-Time Optimization (LTO) mitigate the performance overhead of opaque pointers?</strong></summary>

Normally, compilers compile each `.c` file independently, preventing the compiler from inlining accessor functions defined in another translation unit. With LTO enabled (`gcc -flto`), the compiler preserves intermediate representations (GIMPLE/LLVM IR) across translation units until link time. The linker can then see the complete struct definition and inline accessor functions across file boundaries, eliminating call overhead while retaining source-level encapsulation.
</details>

<details>
<summary><strong>8. How should error handling be designed in an opaque pointer constructor?</strong></summary>

Constructors (such as `intstack_create()`) must return `NULL` on memory exhaustion or invalid initialization arguments. For richer error reporting, the constructor can accept an out-parameter for error codes: `IntStack *intstack_create(size_t cap, StatusCode *out_status)`. The destructor should accept `NULL` pointers without error, matching the defensive behavior of standard `free(NULL)`.
</details>

---

## 11. Practice Exercises

### Easy: Incomplete Struct Declaration
Write the minimal header and implementation demonstrating an opaque `Counter` ADT supporting `counter_create()`, `counter_increment()`, `counter_get()`, and `counter_destroy()`.

<details>
<summary>Solution</summary>

**Header (`counter.h`):**
```c
#ifndef COUNTER_H
#define COUNTER_H

typedef struct Counter Counter;

Counter *counter_create(void);
void counter_destroy(Counter *c);
void counter_increment(Counter *c);
int counter_get(const Counter *c);

#endif
```

**Implementation (`counter.c`):**
```c
#include "counter.h"
#include <stdlib.h>

struct Counter {
    int count;
};

Counter *counter_create(void) {
    Counter *c = malloc(sizeof(Counter));
    if (c) c->count = 0;
    return c;
}

void counter_destroy(Counter *c) {
    free(c);
}

void counter_increment(Counter *c) {
    if (c) c->count++;
}

int counter_get(const Counter *c) {
    return c ? c->count : 0;
}
```
</details>

### Medium: Thread-Safe Opaque String Builder
Design an opaque `StringBuilder` ADT that dynamically appends characters and strings without exposing its internal reallocation buffer.

<details>
<summary>Solution</summary>

```c
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct StringBuilder StringBuilder;

struct StringBuilder {
    char *buffer;
    size_t length;
    size_t capacity;
};

StringBuilder *sb_create(size_t initial_capacity) {
    if (initial_capacity == 0) initial_capacity = 16;
    StringBuilder *sb = malloc(sizeof(StringBuilder));
    if (!sb) return NULL;
    sb->buffer = malloc(initial_capacity);
    if (!sb->buffer) {
        free(sb);
        return NULL;
    }
    sb->buffer[0] = '\0';
    sb->length = 0;
    sb->capacity = initial_capacity;
    return sb;
}

void sb_destroy(StringBuilder *sb) {
    if (!sb) return;
    free(sb->buffer);
    free(sb);
}

bool sb_append(StringBuilder *sb, const char *str) {
    if (!sb || !str) return false;
    size_t add_len = strlen(str);
    if (sb->length + add_len + 1 > sb->capacity) {
        size_t new_cap = (sb->capacity + add_len) * 2;
        char *new_buf = realloc(sb->buffer, new_cap);
        if (!new_buf) return false;
        sb->buffer = new_buf;
        sb->capacity = new_cap;
    }
    memcpy(sb->buffer + sb->length, str, add_len);
    sb->length += add_len;
    sb->buffer[sb->length] = '\0';
    return true;
}

const char *sb_get_string(const StringBuilder *sb) {
    return sb ? sb->buffer : "";
}
```
</details>

### Hard: Polymorphic Handle Wrapper with Refcounting
Implement an opaque handle that includes an internal reference count, ensuring memory is freed only when the last reference is released.

<details>
<summary>Solution</summary>

```c
#include <stdlib.h>
#include <stdbool.h>

typedef struct SharedResource SharedResource;

struct SharedResource {
    int data;
    int ref_count;
};

SharedResource *resource_create(int data) {
    SharedResource *res = malloc(sizeof(SharedResource));
    if (!res) return NULL;
    res->data = data;
    res->ref_count = 1;
    return res;
}

SharedResource *resource_retain(SharedResource *res) {
    if (res) {
        res->ref_count++;
    }
    return res;
}

void resource_release(SharedResource *res) {
    if (!res) return;
    res->ref_count--;
    if (res->ref_count <= 0) {
        free(res);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Step | Location | Code Pattern |
| :--- | :--- | :--- |
| **Declaration** | Header (`.h`) | `typedef struct MyType MyType;` |
| **API Prototypes** | Header (`.h`) | `MyType *my_create(void); void my_destroy(MyType *p);` |
| **Definition** | Implementation (`.c`) | `struct MyType { int internal_data; };` |
| **Allocation** | Implementation (`.c`) | `MyType *p = malloc(sizeof(MyType));` |
| **Cleanup** | Implementation (`.c`) | `free(p->internal); free(p);` |

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17) §6.2.5 (Types: incomplete types).
- David R. Hanson, *C Interfaces and Implementations: Techniques for Creating Reusable Software* (Addison-Wesley) — The definitive architectural handbook for C ADTs.
- SEI CERT C Coding Standard: DCL12-C (Implement abstract data types using opaque types).

---

[← Previous](12-generic-programming.md) | [Section Index](README.md) | [Next →](14-callbacks-and-function-pointer-tables.md)
