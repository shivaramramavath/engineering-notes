# Program Structure and Translation Units

> A deep technical dissection of C program structure: translation units, the phase separation of preprocessor directives, external vs internal linkage (`static` vs `extern`), C runtime startup sequences, and hosted vs freestanding execution environments.

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [00-setup/03-compilation-process.md](../00-setup/03-compilation-process.md), [00-setup/11-headers-and-include-guards-intro.md](../00-setup/11-headers-and-include-guards-intro.md) |
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

In C, a "program" is not written as a single monolithic block evaluated top-to-bottom. It is partitioned into independent compilation entities known as **Translation Units**. 

Understanding program structure prevents widespread architectural errors:
1. **Linker Collisions and Namespace Pollution**: Without understanding linkage, developers declare global variables in header files, causing `multiple definition` link errors, or inadvertently expose internal helper functions globally.
2. **Freestanding vs Hosted Confusion**: Embedded microcontrollers and operating system kernels execute in "freestanding" environments where standard headers like `<stdio.h>` or `<stdlib.h>` do not exist, and execution begins at an architectural reset vector rather than `main()`.
3. **Implicit Conversion Failures**: C translation units are compiled strictly in isolation. The compiler has zero awareness of functions defined in another `.c` file unless a forward declaration or header prototype is visible.

---

## 2. Core Concepts

### Translation Units (TU)

A **Translation Unit** is the fundamental input to the C compiler. It consists of:
- A single `.c` source file.
- All headers (`.h` files) included via `#include`, recursively expanded.
- Minus any code excluded by conditional compilation directives (`#ifdef`, `#if`).

```
Source File (engine.c) + Headers (config.h, stdio.h)
                     |
            [ C Preprocessor ]
                     |
                     v
             Translation Unit (.i)
                     |
            [ Compiler Backend (cc1) ]
                     |
                     v
             Object File (.o)
```

### Scope, Linkage, and Storage Duration

These three orthogonal properties define every identifier in C:

| Property | Definition | Options |
|---|---|---|
| **Scope** | Region of source code text where an identifier can be referred to by name | Block, Function, Function-Prototype, File |
| **Linkage** | How references across different translation units or scopes resolve to the same entity | External (`extern`), Internal (`static`), None (local variables) |
| **Storage Duration** | The lifetime of the memory allocated for the object | Static (entire program run), Automatic (stack block), Allocated (heap `malloc`), Thread (`thread_local`) |

### Hosted vs. Freestanding Environments

- **Hosted Environment**: Standard desktop and server OS (Linux, Windows, macOS). Full standard library is present. `main()` returns an integer to the operating system.
- **Freestanding Environment**: Embedded microcontrollers (ARM Cortex-M, RISC-V), OS kernels, bootloaders. Standard library is minimal (`<float.h>`, `<limits.h>`, `<stdalign.h>`, `<stdarg.h>`, `<stdbool.h>`, `<stddef.h>`, `<stdint.h>`, `<stdnoreturn.h>`). Execution startup is implementation-defined (e.g. `void reset_handler(void)`).

---

## 3. How It Works Internally

### The Anatomy of Linkage in the Symbol Table

When the compiler translates a translation unit into an ELF or COFF object file (`.o`), it creates a **Symbol Table** (`.symtab`):

```
+--------------------------------------------------------------------------+
|                       OBJECT FILE SYMBOL TABLE                           |
+--------------------------------------------------------------------------+
|  Symbol Name        | Bind Type  | Section  | Meaning                    |
|  -----------------  | ---------  | -------- | -------------------------  |
|  greet_internal     | LOCAL      | .text    | Internal Linkage (static)  |
|  greet_external     | GLOBAL     | .text    | External Linkage (extern)  |
|  main               | GLOBAL     | .text    | Program Entry Point        |
|  puts               | UNDEFINED  | *UND*    | External reference to libc |
+--------------------------------------------------------------------------+
```

1. **Internal Linkage (`static` at file scope)**: The compiler emits the symbol with `STB_LOCAL` binding. The linker strictly forbids any other object file from referencing this symbol.
2. **External Linkage (`extern` or default function)**: The compiler emits the symbol with `STB_GLOBAL` binding. The linker allows other object files with `UNDEFINED` references to resolve against it.
3. **No Linkage**: Local variables declared inside functions exist purely as stack offsets or register allocations and do not appear in the linker symbol table at all.

---

## 4. Syntax & Usage

### 1. Canonical Program Layout in Hosted C
```c
/* 1. Preprocessor Header Inclusions */
#include <stdio.h>
#include <stdlib.h>

/* 2. Macro Definitions & Compile-Time Constants */
#define BUFFER_CAPACITY 1024

/* 3. Type Definitions & Struct Declarations */
typedef struct app_config {
    int log_level;
    const char *app_name;
} app_config_t;

/* 4. Global Variables & Static Prototypes */
static app_config_t g_config = { .log_level = 1, .app_name = "CoreDaemon" };
static void initialize_system(void);

/* 5. Main Function */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    initialize_system();
    printf("Starting %s at log level %d\n", g_config.app_name, g_config.log_level);
    return EXIT_SUCCESS;
}

/* 6. Static Helper Implementations */
static void initialize_system(void) {
    /* Hardware or subsystem initialization */
}
```

### 2. Standard `main` Signatures
In hosted C, ISO standards recognize two conforming signatures:
```c
int main(void);
int main(int argc, char *argv[]);
/* Or equivalent: int main(int argc, char **argv) */
```
*Note on `void main()`*: Returning `void` from `main()` is **non-standard** in hosted C. It causes undefined behavior on architectures where the caller expects the return value in a specific register (`%eax` on x86). Always return `int`.

---

## 5. Practical Examples

### Translation Unit & Linkage Structure Program

```c
/* File: 01-languages/c/01-fundamentals/examples/01-program-structure.c */
#include <stdio.h>

/* Internal linkage (file-scoped static function) */
static void greet_internal(void) {
    printf("[Internal Linkage] Hello from translation unit private function!\n");
}

/* External linkage (can be called across translation units) */
void greet_external(void) {
    printf("[External Linkage] Public API function invoked.\n");
}

int main(void) {
    printf("=== Translation Unit & Linkage Structure Demo ===\n");
    greet_internal();
    greet_external();
    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 01-program-structure.c -o run_01.exe
./run_01.exe
```
Output:
```text
=== Translation Unit & Linkage Structure Demo ===
[Internal Linkage] Hello from translation unit private function!
[External Linkage] Public API function invoked.
```

#### Symbol Inspection with `nm`:
```bash
nm run_01.exe | grep greet
# Output demonstrates linkage bindings:
# 00401530 t greet_internal    <-- 't' lowercase denotes LOCAL (internal)
# 00401548 T greet_external    <-- 'T' uppercase denotes GLOBAL (external)
```

---

## 6. Best Practices

1. **Default to `static` for All File-Scope Functions and Variables**: Restrict symbol visibility to the current translation unit unless explicitly designing a public library API. This reduces link-time collisions and aids compiler inlining.
2. **Never Put Non-Inline Function Definitions in Headers**: Including a header containing function bodies in multiple `.c` files causes duplicate symbol errors at link time.
3. **Always Declare Prototypes Before Use**: Place function prototypes at the top of the file or in headers. Never call unprototyped functions.
4. **Always Return `EXIT_SUCCESS` or `EXIT_FAILURE`**: Use standard constants from `<stdlib.h>` for portable exit codes across Windows, POSIX, and VMS.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Writing `void main()` | Violates ISO standard in hosted environments; exit status returned to OS is undefined garbage. | Use `int main(void)` or `int main(int argc, char *argv[])`. |
| Defining global variables in header files without `extern` | Multiple translation units including that header emit duplicate global symbols, breaking the linker. | Put `extern int val;` in `.h` and `int val = 0;` in one `.c` file. |
| Forgetting `static` on helper functions | Emits global symbols that pollute the global namespace and can conflict with other libraries. | Tag all internal helper functions with `static`. |
| Leaving `main` unclosed or relying on implicit `int` | Pre-C99 implicit `int` was removed in C99; missing braces break compilation. | Write explicit types, function bodies, and return codes. |
| Assuming order of evaluation between translation units | Global variables with static initialization in one translation unit cannot reliably depend on globals in another. | Use an explicit `init_subsystem()` function call order. |
| Calling functions without prior declarations | In C89, unprototyped functions defaulted to returning `int`, silently corrupting pointer returns on 64-bit platforms. (Removed in C99/C23). | Always `#include` the relevant header or declare a prototype before calling. |

---

## 8. Performance & Security Notes

- **Link-Time Optimization (LTO)**: Passing `-flto` during compilation and linking instructs the compiler to emit GIMPLE intermediate representation into object files. The linker can then perform cross-translation-unit inlining and dead-code elimination across the entire program.
- **Symbol Visibility Hardening**: Using `-fvisibility=hidden` prevents internal symbols from leaking into shared library dynamic symbol tables, preventing reverse engineering and malicious hook attacks.

---

## 9. Comparison / When to Use What

| Linkage Type | Keyword / Scope | Lifetime | Visibility | Symbol Binding |
|---|---|---|---|---|
| **External Linkage** | `extern` (or file-scope function/var without static) | Program duration | Entire program across all translation units | `GLOBAL` (`STB_GLOBAL`) |
| **Internal Linkage** | `static` at file scope | Program duration | Restricted strictly to the current translation unit | `LOCAL` (`STB_LOCAL`) |
| **No Linkage** | Local variables inside functions | Block scope (stack) | Restricted strictly to current block | Not in symbol table |

---

## 10. Interview Questions

<details>
<summary><strong>1. What constitutes a "Translation Unit" in standard C?</strong></summary>

A translation unit is a single source file (`.c`) after it has been fully processed by the C preprocessor—meaning all `#include` headers have been recursively substituted, conditional directives (`#if`, `#ifdef`) evaluated, and macro names expanded.
</details>

<details>
<summary><strong>2. What is the difference between internal and external linkage?</strong></summary>

An identifier with external linkage denotes an entity that can be referenced by other translation units during the linking phase. An identifier with internal linkage (marked with `static` at file scope) is strictly private to its own translation unit and invisible to the linker when resolving external symbols.
</details>

<details>
<summary><strong>3. What are the two strictly conforming signatures of `main()` in hosted C environments?</strong></summary>

1. `int main(void)`
2. `int main(int argc, char *argv[])` (or equivalent `char **argv`).
Any other signature (like `void main()`) is implementation-defined or non-conforming.
</details>

<details>
<summary><strong>4. What does the C standard mandate happens if execution reaches the closing brace `}` of `main()` without an explicit `return`?</strong></summary>

Since C99, if `main()` reaches the closing brace without encountering a `return` statement, the runtime behaves as if `return 0;` was executed. Note that this implicit return behavior applies **only** to `main()`; in all other non-void functions, falling off the end without returning a value invokes undefined behavior.
</details>

<details>
<summary><strong>5. What is the difference between a hosted and a freestanding C environment?</strong></summary>

A hosted environment provides an underlying operating system, the complete standard library, and a standard `main()` entry point. A freestanding environment (e.g. microcontrollers, OS kernels) runs on bare metal with minimal headers (`<stdint.h>`, `<stddef.h>`, etc.) and has implementation-defined startup mechanisms.
</details>

<details>
<summary><strong>6. Why is `static` used at file scope versus block scope?</strong></summary>

At file scope, `static` alters **linkage** from external to internal, hiding the function or variable from other translation units. At block scope (inside a function), `static` alters **storage duration** from automatic (stack) to static (lifetime of the entire program), preserving its value across function calls.
</details>

<details>
<summary><strong>7. What is a "tentative definition" in C?</strong></summary>

A file-scope variable declaration without an initializer and without `extern` (e.g. `int x;`) is a tentative definition. If no actual definition (`int x = 5;`) appears in the same translation unit before file end, the compiler treats it as a definition with initializer `0`.
</details>

<details>
<summary><strong>8. How does Link-Time Optimization (LTO) break translation unit isolation?</strong></summary>

Standard compilation optimizes each translation unit independently and dumps machine code into `.o` files. LTO preserves intermediate compiler representation in object files, allowing the linker's optimizer to analyze all translation units simultaneously, inlining functions and propagating constants across `.c` boundaries.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Identifying Linkage</strong></summary>

**Goal**: Determine the linkage (external, internal, or none) and storage duration of each identifier below:
```c
int a = 1;
static int b = 2;

void func(int c) {
    static int d = 4;
    int e = 5;
}
```

**Solution**:
- `a`: External linkage, static storage duration.
- `b`: Internal linkage, static storage duration.
- `func`: External linkage, static storage duration (code).
- `c`: No linkage, automatic storage duration (parameter).
- `d`: No linkage, static storage duration.
- `e`: No linkage, automatic storage duration.
</details>

<details>
<summary><strong>Exercise 2 (Medium): Multi-File Linkage Repair</strong></summary>

**Goal**: You have two files. `main.c` attempts to access variable `max_connections` defined in `server.c`. Currently, `server.c` has `static int max_connections = 100;`. Explain why linking fails and fix it.

**Solution**:
Because `server.c` declared `max_connections` as `static`, it was given internal linkage and marked `LOCAL` in the symbol table. The linker in `main.c` cannot resolve the symbol, failing with `undefined reference to 'max_connections'`.
Fix:
In `server.h`:
```c
#ifndef SERVER_H
#define SERVER_H
extern int max_connections;
#endif
```
In `server.c`:
```c
#include "server.h"
int max_connections = 100; /* External linkage */
```
In `main.c`:
```c
#include "server.h"
/* Can now safely access max_connections */
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Simulating a Freestanding Entry Point</strong></summary>

**Goal**: Write a minimal C program for Linux x86-64 that compiles with `-nostdlib` (freestanding mode), bypasses `main()`, and exits cleanly with status code `42` using a direct Linux `sys_exit` system call.

**Solution**:
```c
/* freestanding_exit.c */
void _start(void) {
    /* Direct x86-64 syscall for sys_exit (syscall #60 on Linux) */
    long syscall_num = 60;
    long exit_code = 42;

    __asm__ volatile (
        "syscall"
        :
        : "a"(syscall_num), "D"(exit_code)
        : "memory"
    );

    /* Unreachable, but prevents CPU runaway */
    while (1) {}
}
```
Compile and verify exit status:
```bash
gcc -nostdlib freestanding_exit.c -o freestanding
./freestanding
echo $?
# 42
```
</details>

---

## 12. Summary / Cheat Sheet

- **Translation Unit**: Preprocessed `.c` file + all expanded headers.
- **`static` at file scope**: Internal linkage (`STB_LOCAL`), hidden from other object files.
- **`extern`**: External linkage (`STB_GLOBAL`), shared across translation units.
- **`main()` signatures**: `int main(void)` or `int main(int argc, char *argv[])`.
- Hosted: Full standard library; Freestanding: Bare metal, implementation-defined startup.
- Always use `EXIT_SUCCESS` / `EXIT_FAILURE` from `<stdlib.h>`.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 5.1.1: "Translation environment" & Section 6.2.2: "Linkages of identifiers".
- Computer Systems: A Programmer's Perspective (Chapter 7: Linking).
- Michael Kerrisk: *The Linux Programming Interface* (Processes and Environment).

---

[← Previous: ../00-setup/11-headers-and-include-guards-intro.md](../00-setup/11-headers-and-include-guards-intro.md) | [Section Index](./README.md) | [Next: 02-data-types.md](./02-data-types.md) →
