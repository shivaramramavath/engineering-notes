# C Standards and History

> A comprehensive evolution guide from K&R C, ANSI C (C89/C90), C99, C11, C17, to ISO/IEC 9899:2024 (C23): macros, syntax features, breaking changes, and compiler flag baselines.

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [01-toolchain.md](./01-toolchain.md), [04-first-program.md](./04-first-program.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard Baseline** | C11 / C17 (with exhaustive C23 comparative matrix) |

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

The C language is not a single immutable technology; it is governed by the ISO/IEC JTC1/SC22/WG14 international standards committee. Understanding the chronological standards roadmap is critical for systems developers:
1. **Avoiding Antiquated Paradigms**: Legacy tutorials frequently showcase pre-C99 patterns (such as declaring all variables at the beginning of a block, implicit `int` return types, or using dangerous functions like `gets()`). Modern standards explicitly invalidate these bad practices.
2. **Deterministic Cross-Platform Compilation**: Code written relying on compiler-specific dialect extensions (`gnu11`, `gnu17`) breaks unexpectedly when moved to strictly conforming ISO environments (such as embedded compilers or Clang with `-pedantic`).
3. **Navigating C23 Breaking Changes**: ISO/IEC 9899:2024 (C23, published October 30, 2024) introduced key syntax changes—such as `int f()` now strictly meaning "takes no parameters" (`(void)`), deprecating unprototyped functions, and elevating `bool`, `true`, `false`, and `nullptr` to native keywords.

```
1972          1978          1989/1990        1999            2011          2018         2024
Dennis        K&R 1st       ANSI C89 /       ISO C99         ISO C11       ISO C17      ISO C23
Ritchie       Edition       ISO C90          (Major syntax)  (Generics,    (Defect      (nullptr, bool,
(Bell Labs)   (De facto)    (Formal spec)                    Atomics)      fixes only)  attributes)
```

---

## 2. Core Concepts

### The Standard Version Macro: `__STDC_VERSION__`

Standard C compilers declare predefined preprocessor macros to identify the active standard conformance level:

| Standard Edition | Informal Name | Publication Year | Value of `__STDC_VERSION__` | Key Compiler Baseline Flags |
|---|---|---|---|---|
| ISO/IEC 9899:1990 | C89 / C90 | 1989 (ANSI) / 1990 (ISO) | *Undefined* (or `199409L` with AMD1) | `-std=c89`, `-std=c90`, `-ansi` |
| ISO/IEC 9899:1999 | C99 | 1999 | `199901L` | `-std=c99` |
| ISO/IEC 9899:2011 | C11 | 2011 | `201112L` | `-std=c11` |
| ISO/IEC 9899:2018 | C17 / C18 | 2018 | `201710L` | `-std=c17`, `-std=c18` |
| ISO/IEC 9899:2024 | C23 | 2024 | `202311L` | `-std=c23` (or `-std=c2x` in GCC < 14) |

```c
#if !defined(__STDC_VERSION__)
    /* Pre-C99 / C89 compiler */
    #define STANDARD_ERA "C89/C90"
#elif __STDC_VERSION__ == 199901L
    #define STANDARD_ERA "C99"
#elif __STDC_VERSION__ == 201112L
    #define STANDARD_ERA "C11"
#elif __STDC_VERSION__ == 201710L
    #define STANDARD_ERA "C17"
#elif __STDC_VERSION__ >= 202311L
    #define STANDARD_ERA "C23"
#endif
```

### Major Feature Transitions Across Eras

1. **C89 / C90**: Standardized function prototypes, `void *`, `const`, `volatile`, and standard library headers (`<stdio.h>`, `<stdlib.h>`, `<string.h>`).
2. **C99**: 
   - `//` single-line comments.
   - Declarations anywhere within a block, and `for (int i = 0; ...)` loop-scoped iteration variables.
   - Fixed-width integer types via `<stdint.h>` (`int32_t`, `uint64_t`).
   - Boolean type via `<stdbool.h>` (`_Bool`, `bool`, `true`, `false`).
   - Designated initializers (`struct S s = { .x = 1, .y = 2 };`).
   - Flexible array members, compound literals (`(int[]){1, 2, 3}`), and `inline` keyword.
   - Variable Length Arrays (VLAs) (mandatory in C99, made optional in C11).
3. **C11**:
   - `_Generic` compile-time type-based dispatch.
   - Static assertions via `_Static_assert`.
   - Alignment support via `_Alignas` and `_Alignof` (`<stdalign.h>`).
   - Optional multithreading (`<threads.h>`) and lock-free atomic operations (`<stdatomic.h>`).
   - Deprecated and removed `gets()` completely from the ISO standard.
   - Made VLAs optional (`__STDC_NO_VLA__` defined if omitted).
4. **C17**: Bug-fix and technical corrigenda revision; addressed defect reports (DRs) without introducing any new syntax or keywords.
5. **C23 (ISO/IEC 9899:2024)**:
   - `bool`, `true`, `false`, `nullptr`, `constexpr`, `typeof`, `typeof_unqual`, `thread_local`, `alignas`, `alignof` are native keywords.
   - Standard attribute syntax `[[nodiscard]]`, `[[maybe_unused]]`, `[[deprecated]]`, `[[fallthrough]]`, `[[unsequenced]]`, `[[reproducible]]`.
   - Binary integer literals (`0b10100110`) and digit separators (`1_000_000` or `1'000'000`).
   - `#embed` preprocessor directive for binary resource embedding.
   - Standard checked integer arithmetic via `<stdckdint.h>` (`ckd_add`, `ckd_sub`, `ckd_mul`).
   - Empty parameter lists `int func()` now mean `int func(void)` (taking zero arguments).

---

## 3. How It Works Internally

### Standard Compliance vs. GNU Dialects

When GCC or Clang runs, the flag `-std=c11` configures the compiler's lexer, parser, and preprocessor tables differently than `-std=gnu11`:

```
               +-------------------------------------------------------+
               |                  Compiler Front-End                   |
               +-------------------------------------------------------+
                                          |
                +-------------------------+-------------------------+
                |                                                   |
       [-std=c11 -Wpedantic]                                  [-std=gnu11]
                |                                                   |
     Strict ISO Validation                                GNU Extensions Enabled
     ---------------------                                ----------------------
     - Disallows typeof() as keyword                      - Enables typeof()
     - Rejects nested function definitions                - Permits nested functions
     - Rejects non-standard asm block syntax              - Allows inline asm statements
     - Rejects statement expressions ({ int a = 5; a; })  - Allows statement expressions
     - Emits warnings on non-standard extensions          - Suppresses extension warnings
```

### The C23 Prototype Revolution

In all standards prior to C23 (C89 through C17):
```c
int compute(); /* In C89-C17: Unspecified number of parameters! */
```
The declaration above indicated that `compute()` accepted an **unspecified number of arguments** (a legacy behavior from K&R C), whereas:
```c
int compute(void); /* Takes strictly zero arguments */
```
Under **C23**, this ambiguity was resolved: `int compute()` is syntactically equivalent to `int compute(void)`. Supplying arguments to `compute(42)` under C23 is a compile-time diagnostic error.

---

## 4. Syntax & Usage

### 1. Preprocessor Standard Detection Guard
```c
#include <stdio.h>

void report_standard(void) {
#if defined(__STDC_VERSION__)
    printf("Standard Version Macro: %ldL\n", (long)__STDC_VERSION__);
    #if __STDC_VERSION__ >= 202311L
        printf("Mode: ISO C23 active\n");
    #elif __STDC_VERSION__ >= 201710L
        printf("Mode: ISO C17 active\n");
    #elif __STDC_VERSION__ >= 201112L
        printf("Mode: ISO C11 active\n");
    #elif __STDC_VERSION__ >= 199901L
        printf("Mode: ISO C99 active\n");
    #endif
#else
    printf("Mode: ANSI C89 / ISO C90 active\n");
#endif
}
```

### 2. C11 Generic Selection Syntax
```c
#define PRINT_TYPE(x) printf("%s\n", _Generic((x), \
    int: "int", \
    long: "long", \
    float: "float", \
    double: "double", \
    char *: "char pointer", \
    default: "unrecognized type"))
```

### 3. C23 Native Attributes Syntax
```c
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
[[nodiscard]] int calculate_crc(const char *data, size_t len);
[[maybe_unused]] static void internal_helper(void);
#else
/* C11 / C17 fallback using compiler attributes */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((warn_unused_result)) int calculate_crc(const char *data, size_t len);
__attribute__((unused)) static void internal_helper(void);
#else
int calculate_crc(const char *data, size_t len);
static void internal_helper(void);
#endif
#endif
```

---

## 5. Practical Examples

### Conformance Verification Program

The following program tests compliance with C99, C11, and C23 features in a single translation unit:

```c
/* File: 01-languages/c/00-setup/examples/05-c-standards-history.c */
#include <stdio.h>
#include <limits.h>

int main(void) {
    printf("=== C Standards Compliance Matrix ===\n");
#if defined(__STDC_VERSION__)
    printf("Active Standard Macro (__STDC_VERSION__): %ldL\n", (long)__STDC_VERSION__);
#else
    printf("Active Standard: Pre-C89 (K and R C)\n");
#endif

    /* C99 feature: Variable declaration in for loop */
    int sum = 0;
    for (int i = 1; i <= 5; ++i) {
        sum += i;
    }
    printf("[C99] For-loop scoped iterator sum: %d\n", sum);

    /* C11 feature: _Generic selection */
    #define TYPE_NAME(x) _Generic((x), \
        int: "signed int", \
        double: "double precision float", \
        char *: "pointer to char", \
        default: "other type")

    int a = 10;
    double b = 3.14;
    printf("[C11] Generic selection for 'a' (%d): %s\n", a, TYPE_NAME(a));
    printf("[C11] Generic selection for 'b' (%.2f): %s\n", b, TYPE_NAME(b));

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    printf("[C23] Native nullptr and [[attributes]] active\n");
#else
    printf("[C11/C17 Baseline] Stable LTS configuration verified\n");
#endif

    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 05-c-standards-history.c -o run_05.exe
./run_05.exe
```
Output:
```text
=== C Standards Compliance Matrix ===
Active Standard Macro (__STDC_VERSION__): 201112L
[C99] For-loop scoped iterator sum: 15
[C11] Generic selection for 'a' (10): signed int
[C11] Generic selection for 'b' (3.14): double precision float
[C11/C17 Baseline] Stable LTS configuration verified
```

---

## 6. Best Practices

1. **Pin Standard Explicitly in Every Makefile / CMakeLists**: Never rely on compiler defaults. GCC 13 defaults to `gnu17`, GCC 15 defaults to `gnu23`, and MSVC historically defaulted to C89/C90. Always declare `-std=c11` or `-std=c17`.
2. **Combine `-std=` with `-pedantic-errors`**: Passing `-std=c17` alone allows compiler vendor extensions unless strict conformance checking is enforced.
3. **Avoid Variable Length Arrays (VLAs)**: VLAs allocate runtime-sized memory directly on the execution stack without any allocation failure recovery mechanism. If the requested size exceeds remaining stack space, immediate stack overflow and SIGSEGV occur.
4. **Prefer Fixed-Width Types (`<stdint.h>`) Over Raw Primitive Types**: Always use `int32_t`, `uint32_t`, `int64_t`, and `size_t` for portable data definitions rather than assuming `long` is 64-bit (which is false on Windows x86-64).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Assuming `int f()` takes zero arguments in C11/C17 | In C11/C17, empty parens mean "any number of unspecified parameters", disabling compiler type checking. | Explicitly specify `int f(void)` for zero parameters. |
| Assuming `sizeof(int) == 4` and `sizeof(long) == 8` | The C standard only mandates minimum widths (`int >= 16 bits`, `long >= 32 bits`). On 64-bit Windows (LLP64), `sizeof(long) == 4`. | Use `<stdint.h>` types (`int32_t`, `int64_t`, `intptr_t`). |
| Using `gets()` | `gets()` provides no buffer length boundary checking and was permanently removed in C11. | Use `fgets(buffer, sizeof(buffer), stdin)`. |
| Writing variable declarations only at the top of a function | While mandatory in C89, doing this in modern C increases variable scope unnecessarily and leads to uninitialized variable bugs. | Declare variables at point of first use with initialization (C99+). |
| Mixing `//` comments into strict C89 targets | C89 compilers will fail with syntax errors because only `/* ... */` block comments were defined. | Stick to `/* ... */` if targeting legacy C89. |
| Treating C as a pure subset of C++ | C++ rejects valid C code like `int *p = malloc(sizeof(int));` and does not support designated initializers identically. | Compile C with a dedicated C compiler driver (`gcc`, `clang`), never `g++`. |

---

## 8. Performance & Security Notes

- **Buffer Overflows via Removed APIs**: Standard C11 abolished `gets()` because safe usage is mathematically impossible. Ensure legacy libraries are audited for obsolete C89 APIs.
- **Checked Arithmetic (`<stdckdint.h>`) in C23**: In pre-C23 standards, overflow detection on signed integers was prone to undefined behavior (compilers optimize away `if (a + b < a)`). C23 introduces hardware-accelerated, branchless checked arithmetic macros `ckd_add(&result, a, b)` that guarantee safety.
- **Stack Smashing via VLAs**: Avoid `int arr[n];` in network-facing server code. Malicious client packets specifying large dimensions will immediately crash the process or execute stack-pivot exploits.

---

## 9. Comparison / When to Use What

| Standard | When to Target | Recommended Flag Set |
|---|---|---|
| **C89 / C90** | Ultra-legacy embedded architectures, aerospace certifications requiring legacy toolchains. | `-std=c89 -pedantic-errors` |
| **C99** | Legacy microcontrollers, older Linux distributions, strict POSIX.1-2001 compatibility. | `-std=c99 -Wall -Wextra` |
| **C11** | General systems programming requiring `_Generic`, static asserts, and multi-platform atomics. | `-std=c11 -Wall -Wextra -Wpedantic` |
| **C17 (Default Recommendation)** | Modern production enterprise systems, Linux kernel modules, robust cross-platform libraries. | `-std=c17 -Wall -Wextra -Wpedantic` |
| **C23** | Modern bleeding-edge C development using GCC 14+ / Clang 18+, embedded binary assets (`#embed`), and native keywords. | `-std=c23 -Wall -Wextra -Wpedantic` |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between C11 and C17?</strong></summary>

C17 introduced **zero new language or library features**. It was purely a maintenance bug-fix release dedicated to addressing Defect Reports (DRs) submitted against the C11 specification. Practically, C17 represents the stable, corrected state of C11.
</details>

<details>
<summary><strong>2. How did the meaning of `void func()` change between C17 and C23?</strong></summary>

In C17 (and all prior standards back to C89), a function declared with empty parentheses like `int calculate();` indicated that the function accepts an unspecified number and type of arguments (disabling prototype argument checking). In C23, empty parentheses mean the function takes strictly **zero** arguments, identical to `int calculate(void);`.
</details>

<details>
<summary><strong>3. What macro indicates the version of the C standard in a conforming compiler?</strong></summary>

The preprocessor macro `__STDC_VERSION__`. In C99 it expands to `199901L`, in C11 to `201112L`, in C17 to `201710L`, and in C23 to `202311L`. In ANSI C89/C90, the macro is undefined.
</details>

<details>
<summary><strong>4. Why were Variable Length Arrays (VLAs) demoted from mandatory in C99 to optional in C11?</strong></summary>

VLAs introduce severe safety risks: allocating arrays of runtime-variable size on the thread stack can silently exceed stack limits, causing uncatchable stack overflow crashes. Additionally, compiler implementation complexity on architectures without dedicated stack frames led the ISO committee to make them optional (`__STDC_NO_VLA__`).
</details>

<details>
<summary><strong>5. What did C11 introduce to replace compiler-specific macros for compile-time type matching?</strong></summary>

`_Generic` selection. It provides compile-time type-based dispatch without runtime performance overhead, functioning like a type-level switch statement evaluated during preprocessing/semantic analysis.
</details>

<details>
<summary><strong>6. Is C a strict subset of C++? Give two examples that compile in C but not C++.</strong></summary>

No. 
1. Converting `void *` to any pointer type is implicit in C (`int *p = malloc(10);`), but requires an explicit cast in C++ (`int *p = (int *)malloc(10);`).
2. Identifiers that are valid in C (e.g. `int new = 1;` or `int class = 2;`) are reserved keywords in C++.
</details>

<details>
<summary><strong>7. What native keywords replaced `<stdbool.h>` macros in C23?</strong></summary>

In C23, `bool`, `true`, and `false` are first-class keywords recognized directly by the language grammar without needing `#include <stdbool.h>`.
</details>

<details>
<summary><strong>8. What is the `#embed` preprocessor directive in C23?</strong></summary>

`#embed` allows binary data files (such as images, certificates, or firmware blobs) to be directly incorporated into source code as an array initializer at compile time, eliminating the need for external tools like `xxd -i` or `objcopy`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Standard Feature Detector</strong></summary>

**Goal**: Write a header file `std_compat.h` that detects if the active compiler standard is C11 or newer. If not, generate a `#error` aborting compilation.

**Solution**:
```c
#ifndef STD_COMPAT_H
#define STD_COMPAT_H

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 201112L)
    #error "This library requires ISO C11 or newer. Pass -std=c11 or -std=c17 to your compiler."
#endif

#endif /* STD_COMPAT_H */
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Portable Boolean Definition</strong></summary>

**Goal**: Write a portable macro layer that provides `bool`, `true`, and `false` across C89, C99, C11, and C23 environments.

**Solution**:
```c
#ifndef PORTABLE_BOOL_H
#define PORTABLE_BOOL_H

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    /* In C23, bool, true, and false are native keywords */
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    /* In C99 and C11, use stdbool.h */
    #include <stdbool.h>
#else
    /* In C89, define manual enum */
    typedef enum { false = 0, true = 1 } bool;
#endif

#endif /* PORTABLE_BOOL_H */
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Generic Multi-Type Absolute Value Macro</strong></summary>

**Goal**: Using C11 `_Generic`, implement a type-safe `ABS(x)` macro that dispatches to `abs()`, `labs()`, `llabs()`, `fabsf()`, or `fabs()` depending on the type of argument, without evaluating the argument twice.

**Solution**:
```c
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define ABS(x) _Generic((x), \
    int: abs, \
    long: labs, \
    long long: llabs, \
    float: fabsf, \
    double: fabs \
)(x)

int main(void) {
    int i = -42;
    double d = -99.5;
    printf("Abs int: %d\n", ABS(i));
    printf("Abs double: %.2f\n", ABS(d));
    return 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Default Recommendation**: Pin `-std=c17 -Wall -Wextra -Wpedantic` for all new projects.
- **C89**: No single-line comments, declarations must precede block statements, no `<stdint.h>`.
- **C99**: Scoped `for` loops, designated initializers, `int32_t`, `bool`.
- **C11**: `_Generic`, `_Static_assert`, atomics, optional VLAs, `gets()` removed.
- **C17**: Definitive standard release; bug-fixed edition of C11.
- **C23**: `nullptr`, native `bool`, `[[nodiscard]]`, binary literals `0b...`, `#embed`, `<stdckdint.h>`.

---

## 13. Further Reading

- ISO/IEC 9899:2024 Standard specification (ISO C23 Working Group drafts: WG14 N3220).
- Jens Gustedt's *Modern C* (covering C11/C17 architecture in depth).
- GCC Online Documentation: Language Standards Supported by GCC.

---

[← Previous: 04-first-program.md](./04-first-program.md) | [Section Index](./README.md) | [Next: 06-debugging-with-gdb.md](./06-debugging-with-gdb.md) →
