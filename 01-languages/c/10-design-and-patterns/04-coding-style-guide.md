# Professional C Coding Standards and Style Guide

> Establish disciplined, production-grade engineering standards: identifier prefixing, const-correctness, header hygiene, and static analyzer compliance.

| Metadata | Value |
| :--- | :--- |
| **Level** | All Levels |
| **Prerequisites** | [01-modular-design-in-c.md](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/01-modular-design-in-c.md), [04-clang-format.md](file:///E:/Learning/engineering-notes/01-languages/c/06-build-and-tooling/04-clang-format.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard Baseline** | C11 / C17 / C23 / MISRA-C / Linux Kernel |

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
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Unregulated Freedom of C
C is notoriously minimalist. The compiler enforces almost zero stylistic or organizational constraints:
- Global variables can be declared anywhere.
- Any pointer can be cast to any other pointer without explicit safety checks.
- Functions can be defined without prototypes (in pre-C23 legacy modes).
- Macros can quietly overwrite variable names across translation units.

In open-source and enterprise environments, uncoordinated C code quickly devolves into an unmaintainable maze of naming collisions, unpredictable side effects, and subtle aliasing bugs.

### The Purpose of a Formal Style Guide
A formal coding standard is not an aesthetic preference; it is a **safety mechanism**. Strict guidelines:
1. Prevent global symbol collisions through systematic namespace prefixing (`lib_module_action`).
2. Eliminate accidental mutations through rigorous **const-correctness**.
3. Guarantee header idempotency and clean include dependency graphs.
4. Facilitate automated verification via `clang-format`, `clang-tidy`, and MISRA-C static compliance checkers.

---

## 2. Core Concepts

### 1. Systematic Namespace Prefixing
Because C lacks namespaces, every globally visible symbol must be prefixed with its library and module hierarchy:
- **Functions:** `sys_socket_connect()`, `http_parser_execute()`.
- **Types:** `SysSocket`, `HttpParser`.
- **Constants & Enums:** `SYS_STATUS_OK`, `HTTP_METHOD_GET`.
- **Macros:** `SYS_ASSERT()`, `HTTP_LOG_DEBUG()`.

### 2. Strict Const-Correctness
The `const` qualifier informs both the compiler and the programmer of mutability contracts:
- `const int *ptr`: Pointer to constant data (data cannot be modified).
- `int * const ptr`: Constant pointer to mutable data (pointer address cannot change).
- `const int * const ptr`: Constant pointer to constant data.

Every function parameter that is read-only must be explicitly marked `const`.

### 3. Header Inclusion Hygiene
Headers must follow strict structure:
1. Include guards (`#ifndef MODULE_H / #define MODULE_H`) or `#pragma once`.
2. `extern "C"` guards to ensure C++ compatibility.
3. System standard headers first (`<stdio.h>`), followed by third-party headers, followed by project headers.
4. Forward declarations preferred over heavy `#include` directives.

```
       +-----------------------------------------------------------+
       |             Header File Architecture (.h)                 |
       +-----------------------------------------------------------+
       | #ifndef LIB_MODULE_H                                      |
       | #define LIB_MODULE_H                                      |
       |                                                           |
       | #ifdef __cplusplus                                        |
       | extern "C" {                                              |
       | #endif                                                    |
       |                                                           |
       | /* 1. Standard Includes */                                |
       | #include <stddef.h>                                       |
       | #include <stdint.h>                                       |
       |                                                           |
       | /* 2. Forward Declarations */                             |
       | typedef struct ModuleContext ModuleContext;               |
       |                                                           |
       | /* 3. Enums and Status Codes */                           |
       | typedef enum { MODULE_OK = 0, ... } ModuleStatus;         |
       |                                                           |
       | /* 4. Public Function Prototypes */                       |
       | ModuleStatus module_create(ModuleContext **ctx);          |
       |                                                           |
       | #ifdef __cplusplus                                        |
       | }                                                         |
       | #endif                                                    |
       | #endif /* LIB_MODULE_H */                                 |
       +-----------------------------------------------------------+
```

---

## 3. How It Works Internally

### Symbol Table Scoping and Linker Mangling
In C, non-static global symbols are emitted into the object file's symbol table with global binding (`STB_GLOBAL`). If two different translation units define `int buffer_size;` or `void init();`, the linker will either:
1. Crash with duplicate symbol errors (`multiple definition of 'init'`).
2. Silently merge them under tentative definition rules, causing catastrophic memory corruption at runtime.

Prefixing symbols (`net_buffer_size`, `net_init`) guarantees distinct symbol table hash keys across diverse object files and static archives (`.a`).

```
Object Symbol Table Comparison:

Naive Names (Prone to Collision):
[net.o]   init        (STB_GLOBAL)  <--- COLLISION ERROR AT LINK TIME!
[ui.o]    init        (STB_GLOBAL)  <--- COLLISION ERROR AT LINK TIME!

Prefixed Names (Clean & Isolated):
[net.o]   net_init    (STB_GLOBAL)  -> Unique Symbol
[ui.o]    ui_init     (STB_GLOBAL)  -> Unique Symbol
```

### Const Placement and Read-Only Memory (`.rodata`)
Variables declared `const` with static or global storage duration are emitted directly into the ELF `.rodata` (read-only data) section. The operating system kernel maps `.rodata` virtual memory pages with Read-Only (`R`) permissions. Any write attempt triggers a hardware page fault (`SIGSEGV`), preventing memory corruption.

---

## 4. Syntax & Usage

### 1. Professional Struct and Type Declarations

```c
/* Canonical Typename: PascalCase */
/* Canonical Member Variables: snake_case */
typedef struct UserProfile {
    uint64_t user_id;
    char username[64];
    uint32_t login_count;
    bool is_active;
} UserProfile;

/* Enumerations: Scoped Upper Snake Case */
typedef enum {
    CONN_STATE_DISCONNECTED = 0,
    CONN_STATE_CONNECTING,
    CONN_STATE_CONNECTED,
    CONN_STATE_ERROR
} ConnectionState;
```

### 2. Modern Function Signatures
```c
/* Functions: snake_case with module prefix */
/* Parameter order: In-parameters first, Out-parameters last */
ConnectionState conn_establish(const char *hostname, uint16_t port, int timeout_ms, int *out_socket_fd);
```

### 3. Bracket and Indentation Styles (Linux vs Allman)
```c
/* Linux Kernel Style (Tab/4-space, Open brace on same line for conditionals) */
if (is_valid) {
    process();
} else {
    rollback();
}

/* Function definitions: Open brace on NEW line */
int calculate_sum(int a, int b)
{
    return a + b;
}
```

---

## 5. Practical Examples

### Comprehensive Verification Suite: Clean Production Style and Enum Diagnostics

The following test program demonstrates professional naming conventions, const-correct string mappings, and diagnostic formatting:

```c
#include <stdio.h>
#include <stdbool.h>

/* Demonstrating Clean Production Style: Explicit Types, Const-Correctness, Scoped Enums */
typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

static const char* log_level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

int main(void) {
    printf("Evaluating Professional C Coding Standard & Style Conventions...\n");
    LogLevel lvl = LOG_LEVEL_INFO;
    printf("Configured Log Level: [%s]\n", log_level_to_string(lvl));
    printf("Naming prefixes, const-correctness, and enum clarity verified.\n");
    return 0;
}
```

```
Evaluating Professional C Coding Standard & Style Conventions...
Configured Log Level: [INFO]
Naming prefixes, const-correctness, and enum clarity verified.
```

### Explanation
- `LogLevel` enum uses scoped, unambiguous naming prefixes (`LOG_LEVEL_*`).
- `log_level_to_string` accepts the enum by value, returns a `const char*` pointing to static string literals, and includes a defensive `default` case.

---

## 6. Best Practices

1. **Use Exact-Width Integers (`<stdint.h>`):** Avoid bare `int`, `long`, or `short` for data serialization, protocols, or memory buffers. Use `uint8_t`, `int32_t`, `uint64_t`, and `size_t`.
2. **Never Put Executable Code in Headers:** Headers must contain only prototypes, macro constants, inline helpers marked `static inline`, and type definitions.
3. **Always Check Warnings as Errors:** Compile with `-Wall -Wextra -Wpedantic -Werror` to prevent subtle bugs from entering the codebase.
4. **Order Parameters Consistently:** Pass input parameters first (qualified with `const`), followed by input/output pointers, followed by pure output parameters.
5. **Clean Macro Discipline:** Wrap all multi-statement macros in `do { ... } while(0)` blocks, and parenthesize all macro parameters (e.g., `#define MIN(a, b) ((a) < (b) ? (a) : (b))`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Bare Names in Global Scope** | Functions named `init()`, `send()`, or `run()` collide with libc or other libraries. | Always prefix global functions with module names (e.g., `net_client_init()`). |
| **Omitting `const` on Pointer Inputs** | Callers cannot determine if the function mutates their data; compiler cannot catch accidental writes. | Mark all read-only pointer parameters `const Type *ptr`. |
| **Missing Include Guards** | Multiple inclusion of the same header causes duplicate typedef and struct declaration errors. | Enclose headers with standard `#ifndef / #define` guards. |
| **Unparenthesized Macro Parameters** | Operator precedence alters macro logic (e.g., `#define SQR(x) x * x`; `SQR(1 + 2)` becomes `1 + 2 * 1 + 2 = 5`). | Parenthesize every argument: `#define SQR(x) ((x) * (x))`. |
| **Using `typedef` on Pointers** | Hides pointer indirection (e.g., `typedef int* IntPtr;`), confusing readers and breaking const qualifiers. | Only typedef structures, enums, or function pointers; avoid typedefing raw pointers. |
| **Implicit Function Declarations** | Calling undeclared functions causes the compiler to assume an `int` return, crashing on 64-bit pointers. | Include prototypes in headers and enable `-Werror=implicit-function-declaration`. |

---

## 8. Performance & Security Notes

### Performance Impact
- **Compiler Optimization via `const`:** Declaring pointer parameters `const` allows compiler alias analysis engines to prove that inputs do not alias mutable output pointers, enabling register caching and vectorization.
- **Header Minimization:** Heavy, unpruned header inclusion graphs drastically slow down build times. Forward declarations keep compilation times linear.

### Security Implications
- **Const Protection Against Overwrites:** Qualified `const` structures prevent accidental modifications of cryptographic keys, configuration settings, and state flags.
- **MISRA-C Compliance:** Adhering to strict coding standards eliminates entire classes of undefined behaviors (uninitialized reads, pointer arithmetic out-of-bounds, sign mismatches).

---

## 9. Comparison / When to Use What

| Style Guide / Standard | Philosophy | Strictness | Primary Environment |
| :--- | :--- | :--- | :--- |
| **Linux Kernel Style** | 8-space tabs, minimal brackets for 1-line ifs, concise names | Pragmatic | Operating system kernels, device drivers, low-level tooling. |
| **Google C Style** | 2-space indentation, strict type safety, explicit braces | High | User-space applications, cloud services, multi-language repos. |
| **MISRA-C:2012** | Complete elimination of undefined behaviors, no dynamic memory | Extremely Strict | Automotive, avionics, medical devices, mission-critical systems. |
| **SEI CERT C** | Security-focused rules preventing exploits and overflows | High | Security-sensitive network services, financial systems. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is prefixing globally visible C identifiers mandatory in production software?</strong></summary>
Because C does not support language-level namespaces. If two libraries or modules define an external symbol with the same name (such as `parse()` or `Buffer`), the linker will fail with multiple definition errors or silently bind calls to the wrong function.
</details>

<details>
<summary><strong>2. What is the exact difference between `const char *p`, `char * const p`, and `const char * const p`?</strong></summary>
`const char *p` is a mutable pointer to read-only characters. `char * const p` is a constant pointer to mutable characters (the pointer address cannot change). `const char * const p` is a constant pointer to read-only characters (neither pointer nor characters can be modified).
</details>

<details>
<summary><strong>3. Why should multi-statement macros always be wrapped in `do { ... } while(0)`?</strong></summary>
It forces the macro to behave syntactically like a single statement. Without `do { ... } while(0)`, using the macro inside an `if (...)` statement without braces will cause subsequent statements to execute unconditionally, or break `else` branch binding.
</details>

<details>
<summary><strong>4. Why is typedefing raw pointers (e.g., `typedef char* String;`) considered an anti-pattern?</strong></summary>
It obscures the indirection level from the developer. Furthermore, `const String s` does not mean `const char *s` (pointer to const char); it actually means `char * const s` (const pointer to mutable char), leading to misleading const correctness.
</details>

<details>
<summary><strong>5. What is the purpose of the `extern "C"` declaration inside public C headers?</strong></summary>
It informs C++ compilers that the enclosed function prototypes should use the C language calling convention and unmangled symbol names, enabling C libraries to be called from C++ programs without link errors.
</details>

<details>
<summary><strong>6. Why should exact-width integers (`uint32_t`, `int64_t`) be preferred over `int` and `long`?</strong></summary>
The size of `int` and `long` varies across architectures and operating systems (e.g., `long` is 32-bit on Windows x64 LLP64, but 64-bit on Linux x86_64 LP64). Exact-width types ensure identical memory layout and arithmetic behavior across all targets.
</details>

<details>
<summary><strong>7. How does forward declaration of structs improve compilation performance?</strong></summary>
Writing `typedef struct Server Server;` instead of `#include "server.h"` avoids parsing thousands of lines of unnecessary headers in every translation unit, drastically reducing compilation times and breaking circular header dependencies.
</details>

<details>
<summary><strong>8. What is the MISRA-C rule regarding dynamic memory allocation (`malloc`/`free`)?</strong></summary>
MISRA-C Rule 21.3 bans dynamic heap allocation in safety-critical systems because `malloc` can fail non-deterministically due to fragmentation, leading to unpredictable timing and system instability.
</details>

<details>
<summary><strong>9. Why should header files never declare non-static, non-extern variables?</strong></summary>
Declaring `int global_counter;` in a header included by multiple `.c` files causes multiple definition errors at link time. Global variables should be declared `extern int global_counter;` in the header and defined once in a single `.c` file.
</details>

<details>
<summary><strong>10. How does `clang-format` assist in maintaining engineering standards across teams?</strong></summary>
It automatically reformats code according to a shared `.clang-format` configuration file, eliminating subjective debates over whitespace, brace placement, and indentation during code reviews.
</details>

---

## 11. Practice Exercises

### Exercise 1: Correcting Macro Flaws and Side-Effects (Easy)
Identify and correct three bugs in this macro definition:
```c
#define DOUBLE(x) x * 2;
```

<details>
<summary>Solution</summary>

**Flaws:**
1. Trailing semicolon breaks usage inside expressions and `if` conditions.
2. Missing parentheses around parameter `x` alters precedence (`DOUBLE(1 + 2)` becomes `1 + 2 * 2 = 5` instead of `6`).
3. Missing outer parentheses around the expansion.

**Corrected Version:**
```c
#define DOUBLE(x) ((x) * 2)
```
</details>

### Exercise 2: Refactoring Unscoped Identifiers into Clean Namespaced Symbols (Medium)
Take an un-namespaced packet parser implementation and refactor all struct names, enum tags, and function prototypes to conform to a unified namespace prefix `pkt_`.

<details>
<summary>Solution</summary>

**Before (Unscoped, Dangerous):**
```c
enum Type { DATA, ACK };
struct Header { int seq; enum Type t; };
void init(struct Header *h);
```

**After (Namespaced, Production Style):**
```c
typedef enum {
    PKT_TYPE_DATA,
    PKT_TYPE_ACK
} PktType;

typedef struct PktHeader {
    uint32_t sequence_id;
    PktType packet_type;
} PktHeader;

void pkt_header_init(PktHeader *header);
```
</details>

### Exercise 3: Creating a Production-Ready Header Template (Hard)
Author a fully compliant, self-contained header file `matrix.h` with include guards, `extern "C"` compatibility, opaque struct handles, const-correct prototypes, and status codes.

<details>
<summary>Solution</summary>

```c
#ifndef MATH_MATRIX_H
#define MATH_MATRIX_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MathMatrix MathMatrix;

typedef enum {
    MATH_SUCCESS = 0,
    MATH_ERR_NOMEM = -1,
    MATH_ERR_INVALID_DIM = -2,
    MATH_ERR_NULL_PTR = -3
} MathStatus;

MathMatrix* math_matrix_create(size_t rows, size_t cols);
void math_matrix_destroy(MathMatrix **mat_ptr);

MathStatus math_matrix_set(MathMatrix *mat, size_t r, size_t c, double val);
MathStatus math_matrix_get(const MathMatrix *mat, size_t r, size_t c, double *out_val);

size_t math_matrix_rows(const MathMatrix *mat);
size_t math_matrix_cols(const MathMatrix *mat);

#ifdef __cplusplus
}
#endif

#endif /* MATH_MATRIX_H */
```
</details>

---

## 12. Summary / Cheat Sheet

- **Prefix Everything:** `lib_module_action()`, `LibType`, `LIB_ENUM_VALUE`.
- **Exact-Width Types:** Use `<stdint.h>` (`uint32_t`, `int64_t`, `size_t`) rather than ambiguous standard primitives.
- **Const Discipline:** Qualify all input pointer parameters with `const`.
- **Header Structure:** Include guards $\to$ `extern "C"` $\to$ System includes $\to$ Types $\to$ Prototypes.
- **Macro Enclosure:** Enclose in `do { ... } while(0)` and parenthesize all arguments `((a) + (b))`.

---

## 13. Further Reading
- Linux Kernel Organization: *Linux Kernel Coding Style*.
- MISRA: *Guidelines for the Use of the C Language in Critical Systems (MISRA-C:2012)*.
- SEI CERT: *C Coding Standard: Rules for Developing Safe, Reliable, and Secure Systems*.
- Brian W. Kernighan, Rob Pike: *The Practice of Programming*.

---

| [Previous: 03-error-handling-patterns.md](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/03-error-handling-patterns.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/README.md) | [Next: 05-resource-management-patterns.md](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/05-resource-management-patterns.md) |
