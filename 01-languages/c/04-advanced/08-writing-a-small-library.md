# Designing and Packaging a C Library

> Deep architectural exploration of professional C library engineering: header file hygiene, opaque handle encapsulation, `extern "C"` compatibility, prefix namespacing, semantic versioning, and distribution.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [03-intermediate/13-opaque-pointers-and-adts.md](../../03-intermediate/13-opaque-pointers-and-adts.md), [04-linking-static-vs-shared-libraries.md](./04-linking-static-vs-shared-libraries.md) |
| **Time to Read** | 40 minutes |
| **Status** | Complete |
| **Standard Baseline** | ISO C11 / C17 / C23 |

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
9. [Comparison: Header-Only vs Compiled Library](#9-comparison-header-only-vs-compiled-library)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
Writing code for internal application use is very different from authoring a reusable, public C library. A library is a contract consumed by diverse third-party applications, different compilers, and foreign programming language bindings (Python ctypes, Rust FFI, Go cgo).

A production-grade C library must guarantee:
- **ABI Stability**: Internal struct modifications do not break previously compiled client binaries.
- **Zero Global Namespace Pollution**: All symbols must be strictly namespaced with a unique prefix.
- **Flawless Multi-Language Interop**: Headers must compile seamlessly under C99, C11, C23, and C++.
- **Zero Hidden Dependencies**: Avoid dragging monolithic runtime libraries or conflicting headers into the client's build.

---

## 2. Core Concepts

### 2.1 The Opaque Handle Idiom
Never expose internal struct definitions in public headers. Expose only forward-declared typedef pointers:
```c
// Public header (tinylog.h)
typedef struct TinyLog TinyLog;

TinyLog *tinylog_create(const char *prefix, int min_level);
void     tinylog_destroy(TinyLog *l);
```
The actual struct fields reside strictly inside private `.c` implementation files. This allows library maintainers to add, remove, or reorder struct fields without breaking client binary compatibility!

### 2.2 Namespace Prefixing
C lacks namespaces. If your library declares a generic function `init()` or `log()`, it will collide with the consumer's code or libc at link time. Always choose a unique 3–5 letter prefix (e.g. `sqlite3_`, `git_`, `uv_`, `curl_`).

### 2.3 C++ Compatibility (`extern "C"`)
Always wrap public header declarations in `extern "C"` blocks:
```c
#ifdef __cplusplus
extern "C" {
#endif

// Prototypes here...

#ifdef __cplusplus
}
#endif
```

---

## 3. How It Works Internally

### Header vs Implementation Separation

```
Public Client View (tinylog.h):
+------------------------------------+
| typedef struct TinyLog TinyLog;    |  <-- Incomplete type (Pointer only)
| tinylog_log(TinyLog *l, ...);      |
+------------------------------------+

Private Library View (tinylog.c):
+------------------------------------+
| struct TinyLog {                   |  <-- Full struct layout hidden in .c
|     const char *prefix;            |
|     int min_level;                 |
|     FILE *out_stream;              |
| };                                |
+------------------------------------+
```

```mermaid
flowchart LR
    Client["Client Application (main.c)"] -->|Includes| PublicH["Public Header (tinylog.h)<br>Opaque Pointer Only"]
    PublicH -.->|Links Against| LibSO["Compiled Library (libtinylog.so)<br>Full Struct Definition Hidden"]
```

---

## 4. Syntax & Usage

### Export Macro for Cross-Platform Shared Libraries
```c
#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef TINYLOG_BUILDING_DLL
    #define TINYLOG_API __declspec(dllexport)
  #else
    #define TINYLOG_API __declspec(dllimport)
  #endif
#else
  #if __GNUC__ >= 4
    #define TINYLOG_API __attribute__((visibility("default")))
  #else
    #define TINYLOG_API
  #endif
#endif
```

---

## 5. Practical Examples

### Example 1: Full Implementation of a Minimal Production C Library
Goal: Implement an opaque-handle logging library, demonstrate creation, message logging, and destruction.

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Simulated production library header & implementation
typedef struct TinyLog TinyLog;

struct TinyLog {
    const char *prefix;
    int min_level;
};

TinyLog *tinylog_create(const char *prefix, int min_level) {
    TinyLog *l = (TinyLog *)malloc(sizeof(TinyLog));
    if (!l) return NULL;
    l->prefix = prefix;
    l->min_level = min_level;
    return l;
}

void tinylog_log(TinyLog *l, int level, const char *msg) {
    if (level >= l->min_level) {
        printf("[%s] [LEVEL %d] %s\n", l->prefix, level, msg);
    }
}

void tinylog_destroy(TinyLog *l) {
    free(l);
}

int main(void) {
    printf("=== Production C Library API Demonstration ===\n");
    TinyLog *log = tinylog_create("AUTH-SERVICE", 1);
    tinylog_log(log, 1, "User logged in successfully");
    tinylog_destroy(log);
    return 0;
}
```

**Verified Output:**
```text
=== Production C Library API Demonstration ===
[AUTH-SERVICE] [LEVEL 1] User logged in successfully
```

---

## 6. Best Practices
1. **Never Call `exit()` or `abort()` Inside a Library**: Libraries must never terminate the host process. Always return an error code or status enum to the caller.
2. **Accept Custom Allocators**: Provide an initialization struct allowing users to supply custom `malloc` and `free` function pointers (`tinylog_set_allocator(my_malloc, my_free)`).
3. **Provide Version Query APIs**: Expose compile-time version macros (`TINYLOG_VERSION_MAJOR`) and runtime version query functions (`tinylog_version_string()`).
4. **Header Self-Containment**: A public header must be completely self-contained. It should compile cleanly even if included first in an empty `.c` file without prerequisite includes.
5. **Generate `.pc` (pkg-config) Files**: Provide a `libfoo.pc` file so build systems (Meson, CMake, Autotools) can discover library include and linker flags automatically.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Approach |
| :--- | :--- | :--- |
| Exposing full struct definitions | Clients allocate structs on their stack; adding a new field breaks ABI and causes memory corruption. | Use opaque handles (`typedef struct Foo Foo;`). |
| Generic symbol names (`init()`, `open()`) | Causes fatal link-time symbol collisions with other libraries. | Always prefix all public identifiers (`foo_init()`). |
| Leaking implementation headers | Including internal headers inside public headers forces clients to depend on private internal code. | Keep public headers completely clean. |
| Missing `extern "C"` guards | Prevents C++ programs from linking with your library due to mangled symbol names. | Wrap public prototypes in `#ifdef __cplusplus`. |
| Writing to `stderr` directly | Libraries should not spam output to standard streams without explicit user consent. | Provide configurable logging callbacks. |

---

## 8. Performance & Security Notes

### Opaque Pointers and Inlining
Because opaque structs are defined only inside `.c` files, the compiler cannot inspect struct fields from client `.c` files.
- Small accessor functions (`foo_get_id(f)`) incur function call overhead and cannot be inlined by default.
- To enable inlining across library boundaries, compile with **Link-Time Optimization (LTO)** (`-flto`).

---

## 9. Comparison: Header-Only vs Compiled Library

```
Header-Only (STB-style, stb_image.h):
- Pros: Trivial integration (just #include); perfect inlining and optimization.
- Cons: Slower build times (recompiles on every file); harder to maintain ABI stability.

Compiled Shared Library (.so / .dll):
- Pros: Fast builds; strict ABI encapsulation; shared RAM pages across processes.
- Cons: Requires installation, packaging, and linker configuration.
```

---

## 10. Interview Questions

<details>
<summary><strong>1. What is Semantic Versioning (SemVer) and how does it apply to C shared library SONAMEs?</strong></summary>

Given version `MAJOR.MINOR.PATCH`:
- **MAJOR**: Incompatible API/ABI breaking changes. Requires incrementing the SONAME (`libfoo.so.1` $\to$ `libfoo.so.2`).
- **MINOR**: Backward-compatible new features. Existing binaries continue to run without re-linking.
- **PATCH**: Backward-compatible bug fixes.
</details>

<details>
<summary><strong>2. How does the "STB Single-File Header" pattern work?</strong></summary>

The header contains both the public interface and the private implementation in one file. A macro gate (`#ifdef STB_FOO_IMPLEMENTATION`) includes the implementation only in the single `.c` file that defines the macro, while acting as a standard header everywhere else.
</details>

<details>
<summary><strong>3. Why should a C library never print error messages to `stdout` or `stderr`?</strong></summary>

The calling application may be a GUI app without a terminal, a high-throughput network daemon using structured JSON logging, or an embedded device. Uncontrolled writes to standard streams pollute terminal outputs, break protocols, and prevent the application from handling errors programmatically.
</details>

<details>
<summary><strong>4. What is a `pkg-config` file and what does it contain?</strong></summary>

A `.pc` metadata text file installed in `/usr/lib/pkgconfig`. It defines variables (`prefix`, `libdir`, `includedir`) and flags:
- `Cflags: -I${includedir}/foo`
- `Libs: -L${libdir} -lfoo`
Consumers run `pkg-config --cflags --libs foo` to obtain exact compiler flags.
</details>

<details>
<summary><strong>5. How do you design thread-safe error reporting in a C library?</strong></summary>

1. Return an integer error code from the function, and return the payload via an out-parameter pointer.
2. If rich error messages are needed, store the last error string in Thread-Local Storage (`_Thread_local` in C11) accessible via `foo_get_last_error()`, avoiding global variable races.
</details>

<details>
<summary><strong>6. Why is it dangerous for a library to use `errno` directly for its own errors?</strong></summary>

`errno` is used by the C standard library and POSIX system calls. Library calls that perform internal I/O or allocation may overwrite `errno`, confusing callers. A library should define its own domain-specific error enum (`FOO_SUCCESS`, `FOO_ERR_INVALID_PARAM`).
</details>

<details>
<summary><strong>7. How do you support custom memory allocators in a C library?</strong></summary>

Define an allocator structure of function pointers:
```c
typedef struct {
    void *(*alloc)(size_t size);
    void  (*dealloc)(void *ptr);
} FooAllocator;
```
Allow the client to pass this struct during library initialization.
</details>

<details>
<summary><strong>8. What is symbol visibility and why should libraries compile with `-fvisibility=hidden`?</strong></summary>

By default on Linux, every non-static function is exported in the `.dynsym` table. Compiling with `-fvisibility=hidden` hides all symbols by default. Only functions explicitly marked with `__attribute__((visibility("default")))` are exported. This shrinks binary size, speeds up dynamic linking, and prevents clients from depending on private internal functions.
</details>

---

## 11. Practice Exercises

### Easy 1: Include Guard Boilerplate
Write standard include guard with `extern "C"` block.

<details>
<summary>Solution</summary>

```c
#ifndef MYLIB_H
#define MYLIB_H

#ifdef __cplusplus
extern "C" {
#endif

// Library APIs

#ifdef __cplusplus
}
#endif

#endif // MYLIB_H
```
</details>

### Easy 2: Version Macros and Query Function
Define compile-time version macros and runtime query prototype.

<details>
<summary>Solution</summary>

```c
#define MYLIB_VERSION_MAJOR 1
#define MYLIB_VERSION_MINOR 4
#define MYLIB_VERSION_PATCH 2

const char *mylib_version_string(void);
```
</details>

### Easy 3: Domain Error Enum
Define a clean error code enum for a networking library.

<details>
<summary>Solution</summary>

```c
typedef enum {
    NET_OK = 0,
    NET_ERR_INVALID_ARG = -1,
    NET_ERR_OUT_OF_MEMORY = -2,
    NET_ERR_CONNECTION_REFUSED = -3,
    NET_ERR_TIMEOUT = -4
} NetStatus;
```
</details>

### Medium 1: STB-Style Single Header Library Layout
Structure a header file so it acts as declaration by default, but emits implementation when a macro is defined.

<details>
<summary>Solution</summary>

```c
#ifndef MINI_MATH_H
#define MINI_MATH_H

int mini_add(int a, int b);

#endif // MINI_MATH_H

#ifdef MINI_MATH_IMPLEMENTATION
#undef MINI_MATH_IMPLEMENTATION

int mini_add(int a, int b) {
    return a + b;
}

#endif // MINI_MATH_IMPLEMENTATION
```
</details>

### Medium 2: Thread-Local Last Error Storage
Implement `const char *mylib_get_error(void);` using C11 `_Thread_local`.

<details>
<summary>Solution</summary>

```c
static _Thread_local char last_err_msg[256];

void set_error(const char *msg) {
    strncpy(last_err_msg, msg, sizeof(last_err_msg) - 1);
}

const char *mylib_get_error(void) {
    return last_err_msg;
}
```
</details>

### Medium 3: pkg-config `.pc` Template
Write a complete `mylib.pc` file for installation into `/usr/local`.

<details>
<summary>Solution</summary>

```text
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: mylib
Description: High performance systems utility library
Version: 1.0.0
Libs: -L${libdir} -lmylib
Cflags: -I${includedir}
```
</details>

### Hard 1: Pluggable Custom Memory Allocator
Design a complete library initialization API accepting custom memory allocator hooks.

<details>
<summary>Solution</summary>

```c
typedef struct {
    void *(*custom_malloc)(size_t sz);
    void  (*custom_free)(void *ptr);
} LibAllocators;

static LibAllocators g_alloc = { malloc, free };

void lib_init(const LibAllocators *allocs) {
    if (allocs && allocs->custom_malloc && allocs->custom_free) {
        g_alloc = *allocs;
    }
}
```
</details>

### Hard 2: Windows DLL Symbol Exporting Header Macro
Write the canonical multi-platform macro handling GCC visibility and Windows `__declspec(dllexport)`.

<details>
<summary>Solution</summary>

```c
#if defined(_WIN32)
  #ifdef MYLIB_EXPORTS
    #define MYLIB_API __declspec(dllexport)
  #else
    #define MYLIB_API __declspec(dllimport)
  #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
  #define MYLIB_API __attribute__((visibility("default")))
#else
  #define MYLIB_API
#endif
```
</details>

### Hard 3: C FFI Compatibility Audit
Design a C API guaranteed to bind cleanly with Python `ctypes`, Rust `bindgen`, and Go `cgo`.

<details>
<summary>Solution</summary>

```c
// Rules:
// 1. Only primitive types and opaque pointers.
// 2. No bitfields or compiler-specific extensions.
// 3. All strings must be standard null-terminated char *.
// 4. Return values must be explicit integers/pointers.
```
</details>

---

## 12. Summary & Cheat Sheet

| Design Rule | Benefit |
| :--- | :--- |
| **Opaque Handles** | Complete ABI stability across struct modifications |
| **Namespace Prefix** | Avoids link-time symbol collisions repo-wide |
| **`extern "C"`** | Native C++ compatibility |
| **No Aborts / Exits** | Graceful error propagation to caller |
| **Pluggable Allocator**| Embeddable in kernels, game engines, and memory arenas |

---

## 13. Further Reading
- Ulrich Drepper, *How To Write Shared Libraries*, Red Hat (2011).
- David R. Hanson, *C Interfaces and Implementations: Techniques for Creating Reusable Software*, Addison-Wesley.

---

[← Previous Note: Interfacing with Assembly](./07-interfacing-with-assembly.md) | [Advanced C Index](./README.md) | [Next Note: Dynamic Loading with dlopen](./09-dynamic-loading-dlopen.md)
