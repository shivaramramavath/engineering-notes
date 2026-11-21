# Scope and Storage Classes

> Understanding identifiers in C: lexical scope, storage duration, linkage, and storage class specifiers.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Program Structure](../01-fundamentals/01-program-structure.md), [Variables and Constants](../01-fundamentals/04-variables-constants.md), [Functions](../01-fundamentals/08-functions.md) |
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

In C, declaring a variable or function involves three independent orthogonal dimensions:
1. **Scope**: The lexical region of source code where an identifier is visible and resolvable by the compiler.
2. **Storage Duration**: The lifetime of the underlying memory object during program execution (when memory is allocated and released).
3. **Linkage**: Whether identical identifier names in different translation units or scopes resolve to the exact same shared object during linking.

C provides five **storage class specifiers** to control these properties: `auto`, `register`, `static`, `extern`, and `_Thread_local` (C11 / `thread_local` in C23). Mastering these concepts prevents symbol collisions in large codebases, avoids memory corruption from dangling pointers to dead stack frames, and enables encapsulation across modular translation units.

---

## 2. Core Concepts

### The 4 Scopes in C
- **Block Scope**: Identifiers declared inside a compound statement `{ ... }` or within function parameter lists. Visible only from the point of declaration to the closing brace `}`.
- **File Scope**: Identifiers declared outside any function body. Visible from declaration to the end of the translation unit.
- **Function Prototype Scope**: Identifiers appearing within a function declaration prototype (e.g., `void func(int x);`). The name `x` is discarded immediately after the prototype.
- **Function Scope**: Applies exclusively to `goto` labels. A label is visible anywhere within its enclosing function regardless of block nesting.

### The 4 Storage Durations
- **Automatic**: Allocated upon entering the enclosing block and destroyed when the block exits. Automatic variables reside on the call stack and hold indeterminate garbage values until explicitly initialized.
- **Static**: Allocated prior to program startup and retained until program termination. Stored in `.data` (initialized) or `.bss` (zero-initialized).
- **Allocated (Dynamic)**: Explicitly requested at runtime via `malloc()`, `calloc()`, or `realloc()` and persists until released via `free()`.
- **Thread Duration** (C11): Dedicated per-thread storage created when a thread starts and destroyed when the thread terminates (`_Thread_local`).

### Linkage: External, Internal, and None
- **External Linkage (`extern`)**: The identifier represents an object or function shared across multiple translation units. The linker resolves all references to a single global symbol.
- **Internal Linkage (`static` at file scope)**: The identifier is strictly private to the current translation unit (`.c` file). The linker hides the symbol from other object files, preventing naming collisions.
- **No Linkage**: Local block-scope variables and type definitions. Each declaration denotes a unique entity.

---

## 3. How It Works Internally

### Memory Segments and Symbol Resolution

```
Process Address Space:
 high ┌──────────────────────────────────────────────┐
      │ Stack Segment (Automatic Storage)            │  Grows downward
      │   - Local variables, stack frames            │  SP / BP register movement
      │   - Destroyed upon function return           │
      ├──────────────────────────────────────────────┤
      │                     ↓↓                       │
      │                     ↑↑                       │
      ├──────────────────────────────────────────────┤
      │ Heap Segment (Allocated Storage)             │  Grows upward
      │   - malloc / free managed memory             │  brk / sbrk / mmap
      ├──────────────────────────────────────────────┤
      │ .bss Segment (Static Storage Duration)       │  Zero-initialized by OS loader
      │   - Uninitialized globals, static locals     │  Takes zero bytes in binary file
      ├──────────────────────────────────────────────┤
      │ .data Segment (Static Storage Duration)      │  Initialized globals & statics
      │   - Non-zero initialized data                │  Loaded directly from binary
      ├──────────────────────────────────────────────┤
  low │ .text Segment (Code & Read-Only Data)        │  Executable instructions
      └──────────────────────────────────────────────┘
```

### Static Locals vs. Globals
A local variable declared `static int seq = 0;` inside a function has **block scope** (invisible to callers) but **static storage duration** (stored in `.data` / `.bss`). The compiler generates a mangled internal symbol (e.g., `seq.1234`) ensuring it retains its state across multiple invocations without polluting the global namespace.

---

## 4. Syntax & Usage

```c
#include <stdio.h>

/* File-scope variable with external linkage (global) */
int g_active_connections = 0;

/* File-scope variable with internal linkage (private to this .c file) */
static int s_internal_token = 42;

/* External function declaration (defined in another .c file) */
extern void network_flush(void);

/* Inline function with internal linkage */
static inline int max(int a, int b) {
    return (a > b) ? a : b;
}

/* Static local retaining state */
int get_counter(void) {
    static int call_count = 0; /* Initialized once at load time */
    return ++call_count;
}
```

> [!NOTE]
> ### ISO C23 Updates: `auto` for Type Inference and `thread_local`
> - **Type Inference with `auto`**: In C23, `auto` is repurposed to infer variable types from their initializers (matching C++): `auto x = 42LL;` infers `long long`. The historic meaning of `auto` as "automatic storage duration" is deprecated.
> - **Standard `thread_local` Keyword**: C23 adds `thread_local` as a first-class keyword alongside C11's `_Thread_local` from `<threads.h>`.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates block scope shadowing, internal vs. external linkage, static local persistence, and static inline functions.

```c
#include <stdio.h>

/* File-scope external linkage variable */
int global_counter = 10;

/* File-scope internal linkage variable (private to this translation unit) */
static int file_private_counter = 100;

/* Static local variable retaining state across function invocations */
static int get_next_sequence_id(void) {
    static int sequence = 1000;
    return ++sequence;
}

/* Static inline function */
static inline int square(int x) {
    return x * x;
}

int main(void) {
    printf("=== C Scope and Storage Classes Demo ===\n");

    /* Block scope and shadowing */
    int x = 50;
    printf("Outer block x: %d\n", x);
    {
        int x = 999; /* Shadows outer x */
        printf("Inner nested block x (shadowed): %d\n", x);
    }
    printf("Outer block x restored: %d\n", x);

    /* Storage duration: static local persistence */
    printf("Sequence ID 1: %d\n", get_next_sequence_id());
    printf("Sequence ID 2: %d\n", get_next_sequence_id());
    printf("Sequence ID 3: %d\n", get_next_sequence_id());

    /* Linkage inspection */
    printf("Global counter (external linkage): %d\n", global_counter);
    printf("File private counter (internal linkage): %d\n", file_private_counter);
    printf("Inline square(9): %d\n", square(9));

    return 0;
}
```

```text
=== C Scope and Storage Classes Demo ===
Outer block x: 50
Inner nested block x (shadowed): 999
Outer block x restored: 50
Sequence ID 1: 1001
Sequence ID 2: 1002
Sequence ID 3: 1003
Global counter (external linkage): 10
File private counter (internal linkage): 100
Inline square(9): 81
```

---

## 6. Best Practices

1. **Default to `static` for File-Scope Functions and Variables**: Keep helper functions and translation-unit state internal (`static`) unless they are part of the exported public API. This avoids accidental linker collisions.
2. **Never Return Pointers to Automatic Stack Variables**: Returning `&local_var` produces a dangling pointer because stack frames are invalidated upon return. Allocate on the heap or require the caller to pass an output buffer.
3. **Compile with `-Wshadow`**: Variable shadowing inside nested blocks obscures bugs where an inner variable accidentally masks an outer variable or parameter.
4. **Header Files Should Only Contain `extern` Declarations**: Never define non-static global variables in header files (`int count;` in `foo.h` causes multiple-definition linker errors when included by multiple `.c` files). Use `extern int count;` in the header and `int count = 0;` in one `.c` file.
5. **Use `static inline` for Header Functions**: Utility functions defined in headers should be marked `static inline` so each translation unit receives an internal copy without linker conflicts.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Returning address of local variable** | Stack memory is reclaimed upon function return; dereferencing it causes undefined behavior or crashes. | Return by value, allocate dynamically with `malloc()`, or pass a buffer pointer. |
| **Defining global variables in headers** | Including the header in multiple `.c` files causes linker collision: `multiple definition of 'symbol'`. | Declare `extern int var;` in header, define `int var = 0;` in one `.c` file. |
| **Assuming static local resets on call** | A static local initializer `static int i = 0;` executes only once at program startup, not on each call. | Explicitly reset the static variable when reinitialization is required. |
| **Using `auto` expecting C++ behavior in C99/C11** | In C prior to C23, `auto` only declares automatic storage duration and requires an explicit type specifier. | Write the full type in C11/C17 (`int x = 42;`) or use C23 mode (`-std=c2x`). |
| **Relying on the `register` keyword for speed** | Modern optimizing compilers ignore `register` hints and assign registers via graph coloring. It also forbids `&reg`. | Trust compiler optimization flags (`-O2`, `-O3`) and remove `register`. |
| **Non-reentrant functions using static locals** | Functions utilizing static local variables share state across calls, making them thread-unsafe and non-reentrant. | Pass state structs via pointers or use `_Thread_local`. |

---

## 8. Performance & Security Notes

### Reentrancy, Thread Safety, and `_Thread_local`
Functions relying on static local variables (like standard `strtok()` or `asctime()`) are fundamentally **non-thread-safe**. When multiple threads invoke `strtok()` concurrently, they corrupt each other's parsing state. C11 introduced `_Thread_local` to grant each thread an isolated instance of the static variable:
```c
_Thread_local int t_thread_id; /* Thread-specific static storage */
```

### Security: Information Leaks via Uninitialized Automatic Variables
Automatic variables allocated on the stack are not zeroed by the runtime. If an uninitialized stack buffer is partially populated and sent over a network socket, the uninitialized portions leak previous function call frames, potentially exposing cryptographic keys, passwords, or return addresses (defeating ASLR).

---

## 9. Comparison: When to Use What

| Storage Specifier | Scope | Lifetime | Linkage | Typical Use Case |
| :--- | :--- | :--- | :--- | :--- |
| `auto` (C11) | Block | Automatic (Stack) | None | Standard local variables (redundant) |
| `static` (Local) | Block | Static (Process) | None | Persistent state, generators, singletons |
| `static` (Global) | File | Static (Process) | Internal | Private module state, internal helpers |
| `extern` | File | Static (Process) | External | Shared public APIs and global variables |
| `_Thread_local` | Block / File | Thread lifetime | Internal / External | Per-thread caches, error codes (`errno`) |

---

## 10. Interview Questions

<details>
<summary><strong>1. What does the <code>static</code> keyword mean when applied to a local variable vs. a global variable?</strong></summary>

When applied to a **local variable**, `static` alters its storage duration from automatic to static: the variable is allocated in the data segment, initialized once at program startup, and retains its value between function calls, while remaining scoped locally.
When applied to a **global variable** or function at file scope, `static` alters its linkage from external to internal: the symbol is private to that translation unit and cannot be accessed or linked by other `.c` files.
</details>

<details>
<summary><strong>2. What is a "tentative definition" in C?</strong></summary>

In C, a file-scope declaration without an initializer and without the `extern` specifier is a tentative definition: `int x;`. If no actual definition with an initializer appears later in the translation unit, the compiler treats it as if it were initialized to zero: `int x = 0;`. This permits multiple identical tentative definitions in the same file.
</details>

<details>
<summary><strong>3. What is the difference between declaring a variable and defining a variable?</strong></summary>

A **declaration** introduces the identifier and its type to the compiler without allocating storage: `extern int count;`.
A **definition** instantiates the object, causing the compiler to allocate physical memory: `int count = 10;`. All definitions are declarations, but not all declarations are definitions.
</details>

<details>
<summary><strong>4. Why is returning a pointer to a local variable undefined behavior, and how does the compiler detect it?</strong></summary>

Local variables have automatic storage duration on the call stack frame. When the function returns, the stack pointer adjusts, marking that frame memory as reusable. The address returned points to dead memory. Compilers use static data-flow analysis to detect addresses of local variables escaping the function scope and emit `-Wreturn-local-addr` warnings.
</details>

<details>
<summary><strong>5. What is the difference between <code>static inline</code> and <code>extern inline</code>?</strong></summary>

A `static inline` function provides a private copy to every translation unit where it is included; if the compiler chooses not to inline it, it emits an internal static function.
An `extern inline` function informs the compiler that an external out-of-line definition exists elsewhere; if inlining fails, the compiler emits a call to that external symbol.
</details>

<details>
<summary><strong>6. What happens if two different <code>.c</code> files define a global variable with the same name without <code>static</code>?</strong></summary>

Under modern compilers with `-fno-common` (default in GCC 10+), the linker rejects the program with a `multiple definition of 'symbol'` error. In legacy systems with common symbol support, the linker merged them into a single uninitialized symbol, often causing silent memory corruption.
</details>

<details>
<summary><strong>7. How does the C23 <code>auto</code> keyword differ from previous standards?</strong></summary>

Prior to C23, `auto` was a storage class specifier explicitly denoting automatic storage duration (which was already the default for locals). In ISO C23, `auto` denotes type inference: the variable's type is deduced from its initialization expression, e.g., `auto x = 1.5;` deduces `double`.
</details>

<details>
<summary><strong>8. What is the scope of a variable declared inside the header of a <code>for</code> loop?</strong></summary>

Since C99, a variable declared in a `for` loop header (`for (int i = 0; i < 10; i++)`) has block scope confined entirely to the loop body and header. It cannot be accessed outside or after the loop.
</details>

---

## 11. Practice Exercises

### Exercise 1: State Machine ID Generator (Easy)
Write a function `uint32_t generate_uuid(void)` that uses a static local variable to generate monotonically increasing IDs starting at 100000.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdint.h>

uint32_t generate_uuid(void) {
    static uint32_t current_id = 100000;
    return current_id++;
}
```
</details>

### Exercise 2: Translation Unit Encapsulation (Medium)
Create a counter module with two files: `counter.h` and `counter.c`. The internal count must be strictly private (internal linkage), manipulated only through public functions `counter_inc()`, `counter_get()`, and `counter_reset()`.

<details>
<summary>View Solution</summary>

`counter.h`:
```c
#ifndef COUNTER_H
#define COUNTER_H

void counter_inc(void);
int  counter_get(void);
void counter_reset(void);

#endif
```

`counter.c`:
```c
#include "counter.h"

static int s_count = 0; /* Internal linkage: private to counter.c */

void counter_inc(void) { s_count++; }
int  counter_get(void) { return s_count; }
void counter_reset(void) { s_count = 0; }
```
</details>

### Exercise 3: Thread-Safe Per-Thread Error Logging (Hard)
Implement a per-thread error state recorder using `_Thread_local` or a simulated thread-local wrapper that stores the last error message string independently for each thread.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <string.h>

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#define THREAD_LOCAL _Thread_local
#else
#define THREAD_LOCAL /* Fallback for single-threaded environment */
#endif

static THREAD_LOCAL char t_last_error[256] = "OK";

void set_thread_error(const char *msg) {
    if (msg) {
        strncpy(t_last_error, msg, sizeof(t_last_error) - 1);
        t_last_error[sizeof(t_last_error) - 1] = '\0';
    }
}

const char* get_thread_error(void) {
    return t_last_error;
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Scope rules */
{ int x; }             /* Block scope: x destroyed at closing brace */
int g_x;               /* File scope: accessible throughout file */

/* Storage duration */
int a;                 /* Automatic: lives during block execution */
static int s;          /* Static: lives during entire program run */
void *p = malloc(n);   /* Allocated: lives until free(p) */

/* Linkage */
static int private_v;  /* Internal linkage: this .c file only */
extern int public_v;   /* External linkage: visible to linker across all .c files */
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §6.2.1 "Scopes of identifiers" & §6.2.2 "Linkages of identifiers".
- ISO/IEC 9899:2024 (C23 Standard), §6.7.1 "Storage-class specifiers" (C23 `auto` & `thread_local`).
- *Computer Systems: A Programmer's Perspective* (3rd ed.) by Bryant & O'Hallaron (Chapter 7: "Linking").
- *Deep C Secrets* by Peter van der Linden (Storage and Scope mechanics).

---

[← Previous: Structs, Unions & Enums](./06-structs-unions-enums.md) | [Section Index](./README.md) | [Next: Memory Management →](./08-memory-management.md)
