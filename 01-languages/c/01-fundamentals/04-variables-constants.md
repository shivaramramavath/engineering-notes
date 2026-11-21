# Variables, Constants, and Storage Durations

> The complete lifecycle of C objects: declaration vs definition, the 4 storage durations (automatic, static, allocated, thread), scope and visibility, `const` semantics vs `#define` vs `enum` constants, and C23 `constexpr` objects.

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [02-data-types.md](./02-data-types.md), [03-integer-representation.md](./03-integer-representation.md) |
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

In C, variables are not flexible symbol tables; they are typed memory locations governed by strict lifetime and visibility contracts. Understanding how variables exist in memory prevents several categories of fatal bugs:
1. **Uninitialized Memory Garbage**: Automatic (stack) variables in C are **not zero-initialized** by default. Reading an uninitialized local variable reads whatever residual stack bytes existed from prior function calls, invoking undefined behavior.
2. **The "Const is Not Constant" Trap**: In C11 and C17, a `const int N = 10;` is **not a true constant expression**—it is merely a "read-only variable." It cannot be used as a case label in a switch statement or as a non-VLA array dimension!
3. **Dangling Stack Pointers**: Returning a pointer to an automatic local variable destroys memory integrity: once the function returns, its stack frame is marked for reuse, leaving the pointer pointing to invalidated memory.

---

## 2. Core Concepts

### Declarations vs. Definitions

- **Declaration**: Introduces an identifier and specifies its type to the compiler, without allocating storage. Can appear multiple times.
  ```c
  extern int global_counter; /* Declaration only */
  ```
- **Definition**: Instantiates the object, reserving physical memory. Can only appear once per program (One Definition Rule).
  ```c
  int global_counter = 0;   /* Definition: allocates 4 bytes */
  ```

### The 4 Storage Durations

| Storage Duration | Memory Region | Lifetime | Initialization |
|---|---|---|---|
| **Automatic** | Stack | Enters enclosing `{}` block until block exits | **Uninitialized** (indeterminate garbage bits) |
| **Static** | `.data` / `.bss` | Program startup until program termination | Zero-initialized by default (`0` or `NULL`) |
| **Allocated** | Heap | Created by `malloc()`, lives until `free()` | Uninitialized (`malloc`) or zeroed (`calloc`) |
| **Thread** | Thread Stack / TLS | Thread start until thread exit (`thread_local`) | Zero-initialized by default |

### The 4 Scopes

1. **Block Scope**: Identifier declared inside `{}` is visible only until the closing `}`.
2. **File Scope**: Identifier declared outside any function is visible from declaration to end of translation unit.
3. **Function Scope**: Applies exclusively to `goto` labels (visible throughout their entire enclosing function).
4. **Function-Prototype Scope**: Parameter names in forward declarations (e.g. `int add(int a, int b);`).

---

## 3. How It Works Internally

### Memory Segment Allocation

The compiler and linker place variables into distinct ELF segments based on their storage duration and initialization state:

```
High Memory
+-------------------------------------------------------------+
| Stack (Automatic Variables: int x = 5;)                     |  <-- Grows DOWN
|   - Reclaimed instantaneously on function return            |
+-------------------------------------------------------------+
|                         v                                   |
|                         ^                                   |
+-------------------------------------------------------------+
| Heap (Allocated Storage: malloc / free)                     |  <-- Grows UP
+-------------------------------------------------------------+
| .bss Segment (Uninitialized Static Variables: static int y;)|  <-- Zeroed by OS
+-------------------------------------------------------------+
| .data Segment (Initialized Static Variables: static int z=1;)| <-- From binary
+-------------------------------------------------------------+
| .rodata Segment (Read-only constants, string literals)      |  <-- Write-protected
+-------------------------------------------------------------+
| .text Segment (Machine code instructions)                   |  <-- Executable
+-------------------------------------------------------------+
Low Memory
```

### Why `const` is Not a Compile-Time Constant in C11/C17

In C++ and C23, `constexpr` creates true immutable compile-time constants. But in **C11/C17**:
```c
const int ARRAY_SIZE = 16;
int buffer[ARRAY_SIZE]; /* In C99/C11/C17, buffer is a Variable Length Array (VLA)! */
```
Under C11, the compiler treats `ARRAY_SIZE` as a read-only object allocated in memory (often `.rodata`), not an integer constant expression. Therefore, `int buffer[ARRAY_SIZE]` creates a Variable Length Array (VLA) allocated dynamically on the stack!

To define true compile-time integer constants in C11/C17 without preprocessor macros, use an `enum`:
```c
enum { ARRAY_SIZE = 16 };
int buffer[ARRAY_SIZE]; /* Truly fixed-size array in .bss or stack! */
```

In **C23**, native `constexpr` solves this cleanly:
```c
constexpr int ARRAY_SIZE = 16; /* True constant object in C23 */
```

---

## 4. Syntax & Usage

### 1. Storage Class Specifiers
```c
/* Automatic storage duration, block scope (auto is optional/implicit) */
void worker(void) {
    auto int temp = 10;
    
    /* Static storage duration, block scope: retains value across calls */
    static int call_count = 0;
    call_count++;
    
    /* Register hint (historical: suggests register allocation; address & cannot be taken) */
    register int fast_counter = 0;
    (void)fast_counter;
}

/* Thread-local storage duration (C11) */
#include <threads.h>
thread_local int thread_specific_id = 0;
```

### 2. The Four Constants Paradigms Compared
```c
/* 1. Preprocessor Macro (Text substitution, untyped) */
#define MAX_RETRIES 5

/* 2. Enum Constant (True compile-time integer constant, typed as int) */
enum Config { TIMEOUT_MS = 1000 };

/* 3. Const-Qualified Object (Read-only variable in memory) */
const int read_only_limit = 50;

/* 4. C23 Constexpr Object (True typed compile-time constant) */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
constexpr int BUFFER_SIZE = 4096;
#endif
```

---

## 5. Practical Examples

### Storage Duration & Constant Demonstration Program

```c
/* File: 01-languages/c/01-fundamentals/examples/04-variables-constants.c */
#include <stdio.h>

/* File-scope static variable with static storage duration */
static int g_counter = 0;

void increment_static_counter(void) {
    /* Block-scope static variable retains value across calls */
    static int call_count = 0;
    call_count++;
    g_counter += 10;
    printf("Call #%d: call_count = %d, g_counter = %d\n", call_count, call_count, g_counter);
}

int main(void) {
    printf("=== Storage Duration & Constant Demonstration ===\n");
    
    /* Automatic storage duration */
    int auto_val = 5;
    printf("Automatic variable 'auto_val' = %d\n", auto_val);

    increment_static_counter();
    increment_static_counter();
    increment_static_counter();

    /* Compile-time constant via enum */
    enum Config { MAX_ITEMS = 128 };
    printf("Compile-time enum constant MAX_ITEMS = %d\n", MAX_ITEMS);

    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 04-variables-constants.c -o run_04.exe
./run_04.exe
```
Output:
```text
=== Storage Duration & Constant Demonstration ===
Automatic variable 'auto_val' = 5
Call #1: call_count = 1, g_counter = 10
Call #2: call_count = 2, g_counter = 20
Call #3: call_count = 3, g_counter = 30
Compile-time enum constant MAX_ITEMS = 128
```

---

## 6. Best Practices

1. **Always Initialize Local Variables at Declaration**: Never write `int x;`. Always write `int x = 0;` or initialize immediately from a function call to eliminate indeterminate stack garbage bugs.
2. **Use `enum` for Integer Constants in C11/C17**: Rather than `#define BUFFER_SIZE 256`, use `enum { BUFFER_SIZE = 256 };`. This preserves debugger symbol tables and enforces type checking while avoiding VLA creation.
3. **Mark Constant Buffers `const`**: Using `const uint8_t table[] = { ... }` allows the compiler to place the table in the read-only `.rodata` segment, protecting it from runtime memory corruption and sharing it across processes.
4. **Minimize Variable Scope**: Declare variables in the innermost enclosing block where they are needed rather than at the top of the function.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Reading uninitialized local variable | Local variables contain stack garbage; reading them invokes undefined behavior. | Initialize on declaration: `int count = 0;`. |
| Returning a pointer to an automatic variable | The stack frame is destroyed on function exit; the returned pointer is immediately dangling. | Allocate on heap via `malloc()`, or require caller to pass buffer. |
| Using `const int N = 10;` in `switch` cases | In C11/C17, `const` variables are not integer constant expressions, causing `case label does not reduce to an integer constant`. | Use an `enum` or `#define` or C23 `constexpr`. |
| Modifying a `const` variable via a pointer cast | Violates type qualifier semantics; if the variable was placed in `.rodata`, hardware throws an immediate `SIGSEGV` fault! | Respect `const` contracts; never cast away `const` to mutate data. |
| Multiple definitions of global variable in headers | Violates the One Definition Rule; causes duplicate symbol linker errors. | Use `extern int g_val;` in header and `int g_val = 0;` in single `.c`. |
| Shadowing variables in inner blocks | Inner variable masks outer variable, creating obscure logic errors that are hard to spot. | Enable `-Wshadow` to catch shadowed names. |

---

## 8. Performance & Security Notes

- **Memory Disclosure via Uninitialized Structs**: Structs allocated on the stack contain alignment padding bytes. If the struct is copied across a socket or IPC channel without first being zeroed via `memset(&s, 0, sizeof(s))`, the uninitialized padding bytes leak residual stack memory (such as passwords, private keys, or pointers) to external clients!
- **`.rodata` Hardware Memory Protection**: Const-qualified static data is mapped into physical memory pages marked with read-only permissions (`PROT_READ`). Any buffer overflow or stray pointer attempting to overwrite this memory triggers a CPU page-fault exception immediately.

---

## 9. Comparison / When to Use What

| Constant Style | Scope Control | Type Safety | Usable in Case Labels / Fixed Arrays? | Available In |
|---|---|---|---|---|
| **`#define NAME 10`** | None (Leads to macro pollution) | None (Untyped text replacement) | Yes | All C standards |
| **`enum { NAME = 10 }`** | Scoped to block or file | Typed as `int` | **Yes** | All C standards |
| **`const int NAME = 10`** | Scoped to block or file | Strictly typed | **No** (Creates VLA in C11) | All C standards |
| **`constexpr int NAME = 10`** | Scoped to block or file | Strictly typed | **Yes** | **C23** |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is an uninitialized global variable zero, but an uninitialized local variable contains garbage?</strong></summary>

Global and static variables are placed in the `.bss` segment by the linker. When the operating system loads the program into memory, it maps the `.bss` segment using anonymous zero-filled pages. Local variables reside on the execution stack; when a function stack frame is allocated via `sub %rsp`, the CPU simply moves the stack pointer without wiping preexisting stack memory bytes.
</details>

<details>
<summary><strong>2. Can a `const int` variable be used as the size of a standard fixed-size array in C11?</strong></summary>

No. In C11 and C17, a `const` variable is a read-only object, not an "integer constant expression". Writing `int arr[N]` where `N` is `const int` defines a Variable Length Array (VLA) allocated dynamically on the stack at runtime. In C23, `constexpr` provides true constant expressions that create fixed-size arrays.
</details>

<details>
<summary><strong>3. What happens if you cast away the `const` qualifier from a pointer and mutate the value?</strong></summary>

If the original object was defined as `const` (e.g. `const int x = 42;`), modifying it through any pointer cast invokes undefined behavior. In practice, modern operating systems place `const` data in write-protected `.rodata` memory pages, causing an immediate segmentation fault (`SIGSEGV`).
</details>

<details>
<summary><strong>4. What is the difference between `static` inside a function and `static` outside a function?</strong></summary>

Inside a function (block scope), `static` changes the variable's **storage duration** from automatic (stack) to static (lifetime of the entire program), preserving its value between function calls. Outside a function (file scope), `static` changes the identifier's **linkage** from external to internal, hiding it from other translation units.
</details>

<details>
<summary><strong>5. Why is the `register` keyword rarely used in modern C code?</strong></summary>

Modern optimizing compilers perform advanced graph-coloring register allocation that easily outperforms human register hints. Furthermore, declaring a variable `register` prevents taking its address (`&var`), which can impede compiler optimization passes.
</details>

<details>
<summary><strong>6. What is a dangling pointer, and how does variable storage duration relate to it?</strong></summary>

A dangling pointer is a pointer that points to a memory address that has been deallocated or reclaimed. It commonly occurs when a function returns the address of a local variable with automatic storage duration; once the function returns, that stack frame is reused, and dereferencing the pointer corrupts stack memory.
</details>

<details>
<summary><strong>7. How does C23 `constexpr` differ from C11 `const`?</strong></summary>

In C23, `constexpr` objects are guaranteed to be evaluated at compile time and represent true constant expressions. They can be used as case labels in switch statements, fixed-size non-VLA array bounds, and in static assertions (`static_assert`), whereas `const` objects are merely read-only variables stored in memory.
</details>

<details>
<summary><strong>8. What is the scope and lifetime of a variable declared in the initialization clause of a `for` loop?</strong></summary>

Since C99, a variable declared in `for (int i = 0; ...)` has block scope restricted exclusively to the loop body and header, and automatic storage duration that terminates when the loop finishes execution.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Eliminating Indeterminate Local Variable Reads</strong></summary>

**Goal**: Identify the undefined behavior in this function and repair it:
```c
int calculate_total(int count) {
    int sum;
    for (int i = 0; i < count; ++i) {
        sum += i;
    }
    return sum;
}
```

**Solution**:
`sum` is an automatic variable that is read before being initialized, which invokes undefined behavior. Fix by initializing `sum = 0;`:
```c
int calculate_total(int count) {
    int sum = 0;
    for (int i = 0; i < count; ++i) {
        sum += i;
    }
    return sum;
}
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Safe Array Bound Constants Without Macros</strong></summary>

**Goal**: Define a table size constant in C11 without using `#define` that guarantees that `int lookup_table[SIZE];` is compiled as a fixed-size array in the `.bss` or data segment, rather than a VLA.

**Solution**:
Use an `enum` constant:
```c
enum { TABLE_SIZE = 1024 };

/* Guaranteed true fixed-size array, NOT a VLA! */
static int lookup_table[TABLE_SIZE];
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Fixing Uninitialized Struct Padding Leak</strong></summary>

**Goal**: Given a struct with alignment padding sent over a network socket, fix the memory disclosure vulnerability where uninitialized stack padding bytes leak to the network client.

```c
struct Message {
    char type;     /* 1 byte */
                   /* 3 padding bytes */
    int payload;   /* 4 bytes */
};

void send_message(int socket_fd) {
    struct Message msg;
    msg.type = 1;
    msg.payload = 100;
    write(socket_fd, &msg, sizeof(msg)); /* LEAKS 3 BYTES OF STACK MEMORY! */
}
```

**Solution**:
Zero the entire memory block using `memset` before assigning individual fields:
```c
#include <string.h>

void send_message(int socket_fd) {
    struct Message msg;
    memset(&msg, 0, sizeof(msg)); /* Wipes all padding bytes to zero */
    msg.type = 1;
    msg.payload = 100;
    write(socket_fd, &msg, sizeof(msg)); /* Safe: zeroed padding bytes */
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Automatic**: Stack allocation; lifetime is the `{}` block; uninitialized garbage by default.
- **Static**: `.data` or `.bss`; lifetime is entire program; zeroed by default.
- **`const`**: Read-only variable in memory; **not** a compile-time constant in C11/C17.
- **`enum`**: Best way to define true typed compile-time integer constants in C11/C17.
- **`constexpr`**: C23 feature for true compile-time constant objects.
- Always initialize locals (`int x = 0;`) and zero structs (`memset(&s, 0, sizeof(s))`).

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 6.2.4: "Storage durations of objects" & Section 6.7: "Declarations".
- SEI CERT C Coding Standard: *EXP33-C. Do not read uninitialized memory*.
- Jens Gustedt: *Modern C* (Storage and Lifetime Architecture).

---

[← Previous: 03-integer-representation.md](./03-integer-representation.md) | [Section Index](./README.md) | [Next: 05-operators.md](./05-operators.md) →
