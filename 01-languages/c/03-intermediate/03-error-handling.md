# Robust Error Handling: Status Codes, errno, and Cascading Cleanup

> Systematic error recovery in C: sentinel values, error codes, thread-local errno, strerror_r, and the canonical goto cleanup resource management idiom.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [Functions](../01-fundamentals/08-functions.md), [Control Flow](../01-fundamentals/07-control-flow.md), [Memory Management](../02-core-concepts/08-memory-management.md) |
| **Time to Read** | 30 minutes |
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

Unlike modern high-level languages (C++, Java, Python, Rust), C provides **no exception handling primitives** (`try`, `catch`, `throw`). In C, errors are reported exclusively through in-band sentinel return values, dedicated error enum codes, out-parameters, and the global integer `errno`.

Because C requires manual memory and resource management, error handling is fundamentally intertwined with **cleanup**. When an error occurs halfway through a complex operation that has already allocated memory buffers, opened file descriptors, and acquired mutex locks, the program must gracefully release all acquired resources in reverse order before returning an error code. Failing to handle errors properly causes memory leaks, file descriptor exhaustion, deadlocks, and silent data corruption.

---

## 2. Core Concepts

### The 4 Standard Error Reporting Strategies
1. **Sentinel Return Values**:
   - Returning an impossible data value to indicate failure (`NULL` from `malloc()`, `EOF` from `fgetc()`, `-1` from POSIX `open()`).
   - *Limitation*: Only works when a valid return range leaves an unused sentinel value.
2. **Status Code via Return + Data via Out-Parameter (Recommended)**:
   - Reserving the function return value strictly for an error code (`int` or `enum`), while returning payload data through caller-supplied pointer arguments:
     ```c
     StatusCode parse_record(const char *raw, Record *out_rec);
     ```
3. **Thread-Local `errno` (`<errno.h>`)**:
   - Standard C library functions set the integer `errno` on failure.
   - *Rule*: `errno` is meaningful **only immediately after a function reports failure**. It is never reset to zero on success.
4. **Error Enums**:
   - Strongly-typed enumeration error codes (`ERR_NOMEM`, `ERR_TIMEOUT`, `ERR_NOT_FOUND`) allowing callers to differentiate causes and implement targeted recovery.

### The Canonical `goto cleanup;` Idiom
In single-exit function design, acquiring multiple resources sequentially requires handling failures at each step. Rather than duplicating nested deallocations in every `if (error)` branch, systems engineers use `goto cleanup;`:
- Variables are initialized to `NULL` / invalid state.
- On failure, code jumps to a centralized `cleanup:` label at the end of the function.
- Resources are released safely in reverse order of acquisition.

---

## 3. How It Works Internally

### Resource Acquisition and Cascading Rollback Architecture

```
Normal Flow:                                Error Rollback (e.g. Allocation 3 Fails):
 Allocate Resource 1 ──► OK                  Allocate Resource 1 ──► OK
          │                                           │
 Allocate Resource 2 ──► OK                  Allocate Resource 2 ──► OK
          │                                           │
 Allocate Resource 3 ──► OK                  Allocate Resource 3 ──► FAILED!
          │                                           │
    Execute Logic                             goto cleanup; ───┐
          │                                                    ▼
    goto cleanup; ───┐                                 cleanup:
                     ▼                                   free(Resource 2)
                 cleanup:                                free(Resource 1)
                   free(Resource 3)                      return STATUS_ERR_NOMEM;
                   free(Resource 2)
                   free(Resource 1)
                   return STATUS_OK;
```

In compiled assembly, `goto cleanup` translates to an immediate unconditional jump instruction (`jmp .Lcleanup`). Centralizing cleanup minimizes binary code size, eliminates duplicate free instructions, and guarantees consistent cleanup across all exit paths.

---

## 4. Syntax & Usage

```c
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* Checking errno defensively */
void read_file_example(const char *path) {
    errno = 0; /* Reset before operations that don't return sentinel */
    FILE *f = fopen(path, "r");
    if (!f) {
        /* Capture errno immediately */
        int err = errno;
        fprintf(stderr, "Error opening '%s': %s (code %d)\n",
                path, strerror(err), err);
        return;
    }
    fclose(f);
}
```

> [!NOTE]
> ### Thread-Safety of `errno` and `strerror_r`
> In modern multi-threaded systems (POSIX and C11), `errno` is defined as a macro expanding to a thread-local function call (e.g., `(*__errno_location())`), guaranteeing that threads do not overwrite each other's error codes. However, standard `strerror(errno)` returns a pointer to an internal static buffer that is **not thread-safe**. Multithreaded systems should use POSIX `strerror_r()` instead.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates the canonical `goto cleanup` architecture, status code propagation, and safe ownership transfer of dynamically allocated resources.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef enum StatusCode {
    STATUS_OK = 0,
    STATUS_ERR_NOMEM = -1,
    STATUS_ERR_INVALID = -2,
    STATUS_ERR_IO = -3
} StatusCode;

/* Clean resource management using goto cleanup */
static StatusCode process_records(const char *header, size_t count, int **out_buffer) {
    if (!header || count == 0 || !out_buffer) {
        return STATUS_ERR_INVALID;
    }

    StatusCode status = STATUS_OK;
    int *buf1 = NULL;
    int *buf2 = NULL;

    buf1 = malloc(count * sizeof *buf1);
    if (!buf1) {
        status = STATUS_ERR_NOMEM;
        goto cleanup;
    }

    buf2 = malloc(count * sizeof *buf2);
    if (!buf2) {
        status = STATUS_ERR_NOMEM;
        goto cleanup;
    }

    /* Simulate processing */
    for (size_t i = 0; i < count; ++i) {
        buf1[i] = (int)(i * 10);
        buf2[i] = buf1[i] + 5;
    }

    *out_buffer = buf1;
    buf1 = NULL; /* Transfer ownership to caller */

cleanup:
    free(buf1);
    free(buf2);
    return status;
}

int main(void) {
    printf("=== C Error Handling & Goto Cleanup Demo ===\n");

    int *result = NULL;
    StatusCode rc = process_records("TestHeader", 4, &result);
    printf("Operation status code: %d (%s)\n",
           rc, rc == STATUS_OK ? "SUCCESS" : "FAILED");

    if (rc == STATUS_OK && result) {
        printf("Result data: [%d, %d, %d, %d]\n",
               result[0], result[1], result[2], result[3]);
        free(result);
    }

    return 0;
}
```

```text
=== C Error Handling & Goto Cleanup Demo ===
Operation status code: 0 (SUCCESS)
Result data: [0, 10, 20, 30]
```

---

## 6. Best Practices

1. **Use `goto cleanup;` for Multi-Resource Functions**: Do not fear `goto` for error handling. It is the gold standard in the Linux kernel, Git, and SQLite for structured, leak-free cascading deallocation.
2. **Initialize Pointers to `NULL` at Function Entry**: The `free(NULL)` call is a guaranteed safe no-op. Initializing all resource pointers to `NULL` allows `cleanup:` to safely call `free()` on every pointer regardless of which allocation failed.
3. **Nullify Pointers When Transferring Ownership**: When returning an allocated resource via an out-parameter `*out_ptr = buf;`, immediately set `buf = NULL;` so that `cleanup:` does not accidentally free the returned data.
4. **Never Check `errno` Without Verifying Return Value**: `errno` is only meaningful when a function explicitly reports failure. Successful library calls may leave `errno` set to a non-zero value.
5. **Clear `errno = 0` Before Indeterminate Functions**: For functions like `strtol()` that can return valid integers matching sentinel values, clear `errno = 0` immediately before the call.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Testing `errno` after successful call** | Successful library calls do not clear `errno`; checks produce false error alarms. | Only check `errno` when the function return value indicates failure. |
| **Calling other library functions before reading `errno`** | Calling `printf()` or `free()` inside an error branch overwrites `errno`. | Save `errno` to a local variable immediately: `int err = errno;`. |
| **Using `strerror()` in multithreaded code** | `strerror()` returns a pointer to a shared static buffer, causing race conditions. | Use POSIX `strerror_r()` or C11 `strerror_s()`. |
| **Duplicate cleanup blocks in nested ifs** | Deeply nested `if-else` cascades duplicate `free()` logic, leading to maintenance leaks. | Refactor to single-exit `goto cleanup;`. |
| **Freeing uninitialized pointers in `cleanup`** | Uninitialized pointers hold garbage addresses; calling `free()` causes immediate crash. | Always initialize all pointers to `NULL` at declaration. |
| **Ignoring `close()` / `fclose()` return values** | Flush errors or full disk failures occurring during buffer flushing are missed. | Check `if (fclose(f) != 0) { /* handle write error */ }`. |

---

## 8. Performance & Security Notes

### Zero Overhead on Success Path
Unlike C++ exceptions, which incur runtime unwinding tables, stack frame metadata, and RTTI overhead, C status codes and `goto cleanup` execute entirely as simple register comparisons and conditional branches (`test eax, eax; jz .Lcleanup`). On modern CPUs, the success path branch is predicted with $> 99\%$ accuracy, incurring zero runtime penalty.

### Security: Resource Leaks Causing Denial of Service
Failing to release file descriptors or memory buffers in error handling branches allows attackers to trigger deliberate error states (e.g., sending malformed packets), rapidly exhausting system file descriptors (`EMFILE`) or triggering the Out-Of-Memory (OOM) killer to crash daemon services.

---

## 9. Comparison: When to Use What

| Error Strategy | Expressiveness | Thread Safety | Performance Overhead | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **Sentinel Value (`NULL`, `-1`)** | Low | Yes | Zero | Low-level memory/file allocation |
| **Status Code + Out-Param** | High | Yes | Zero | General systems API design |
| **Thread-Local `errno`** | Standard | Yes (Per thread) | Minor TLS access | POSIX / C runtime system calls |
| **`setjmp` / `longjmp`** | Exception-like | Tricky (Stack bugs) | High | Parser aborts, non-local escapes |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is the <code>goto cleanup;</code> pattern considered an industry best practice in C despite general warnings against <code>goto</code>?</strong></summary>

Dijkstra's famous warning against `goto` targeted arbitrary, unstructured jumps (spaghetti code). In C systems programming, `goto cleanup` is strictly unidirectional and jumps downward to a single exit point at the end of the function. It guarantees that all resources are cleaned up in reverse order without duplicate code, eliminating memory leaks and Use-After-Free bugs.
</details>

<details>
<summary><strong>2. Why is checking <code>if (errno != 0)</code> directly after a library call dangerous?</strong></summary>

The ISO C standard does not require successful library calls to reset `errno` to zero. A previous failed call may have set `errno = ENOENT`, and subsequent successful calls may leave `errno` untouched. You must only check `errno` when the library call explicitly returns an error indicator (such as `-1` or `NULL`).
</details>

<details>
<summary><strong>3. How does <code>strerror_r()</code> differ between POSIX and GNU implementations?</strong></summary>

POSIX defines `int strerror_r(int errnum, char *buf, size_t buflen)` which returns 0 on success.
GNU provides an alternate variant: `char *strerror_r(int errnum, char *buf, size_t buflen)` which may return a pointer to an immutable static string or to the buffer. Compilers select the variant based on `_GNU_SOURCE` and `_POSIX_C_SOURCE` macros.
</details>

<details>
<summary><strong>4. How should you design a function that needs to return both a data payload and an error code?</strong></summary>

Return the error code as the function's return value (`StatusCode`), and accept a pointer to a caller-allocated variable (or a pointer-to-pointer for dynamic allocations) as an out-parameter:
`int fetch_user_data(int user_id, UserData *out_data);`
</details>

<details>
<summary><strong>5. What is the contract of <code>free(NULL)</code>?</strong></summary>

The ISO C standard explicitly guarantees that passing a null pointer to `free()` performs no action and returns safely. This guarantee makes the `goto cleanup` idiom clean, because unallocated pointers initialized to `NULL` can be passed to `free()` without needing guards.
</details>

<details>
<summary><strong>6. Why must you clear <code>errno = 0</code> before invoking <code>strtol()</code>?</strong></summary>

`strtol()` returns `LONG_MAX` or `LONG_MIN` on overflow and sets `errno = ERANGE`. However, `LONG_MAX` can also be a legitimate valid integer parsed from the input string. Clearing `errno = 0` beforehand allows code to distinguish between an actual overflow and a valid maximum integer.
</details>

<details>
<summary><strong>7. How does error propagation work across architectural layers in C?</strong></summary>

Lower-level functions return granular error codes (e.g., `STATUS_ERR_DISK_FULL`). Higher-level layers inspect the code, clean up intermediate state, and either translate it into a domain-specific error code or bubble it up to the application controller to display a message to the user.
</details>

<details>
<summary><strong>8. What is the difference between an error and a bug in software engineering?</strong></summary>

An **error** is an expected runtime failure condition that a correct program must anticipate and recover from (network timeout, file not found, out of memory).
A **bug** is a programmer defect violating program invariants (null pointer dereference, buffer overflow, impossible switch case). Bugs should be caught with assertions; errors should be handled with status codes.
</details>

---

## 11. Practice Exercises

### Exercise 1: Robust File Reader with Goto Cleanup (Easy)
Write a function `char* read_file_contents(const char *path, size_t *out_size)` that opens a file, queries its size with `fseek`/`ftell`, allocates memory, and reads the data, using `goto cleanup` to ensure the file is always closed on any failure.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>

char* read_file_contents(const char *path, size_t *out_size) {
    FILE *f = NULL;
    char *buf = NULL;

    if (!path || !out_size) goto cleanup;

    f = fopen(path, "rb");
    if (!f) goto cleanup;

    if (fseek(f, 0, SEEK_END) != 0) goto cleanup;
    long sz = ftell(f);
    if (sz < 0) goto cleanup;
    rewind(f);

    buf = malloc((size_t)sz + 1);
    if (!buf) goto cleanup;

    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        buf = NULL;
        goto cleanup;
    }

    buf[sz] = '\0';
    *out_size = (size_t)sz;

cleanup:
    if (f) fclose(f);
    return buf;
}
```
</details>

### Exercise 2: Cascading Multi-Buffer Allocator (Medium)
Implement `int allocate_pipeline(size_t n, int **b1, int **b2, int **b3)` that allocates three separate integer buffers of size $n$. If any allocation fails, all previously allocated buffers must be released cleanly.

<details>
<summary>View Solution</summary>

```c
#include <stdlib.h>

int allocate_pipeline(size_t n, int **b1, int **b2, int **b3) {
    if (!b1 || !b2 || !b3 || n == 0) return -1;

    int *p1 = NULL, *p2 = NULL, *p3 = NULL;
    int rc = 0;

    p1 = malloc(n * sizeof *p1);
    if (!p1) { rc = -1; goto cleanup; }

    p2 = malloc(n * sizeof *p2);
    if (!p2) { rc = -1; goto cleanup; }

    p3 = malloc(n * sizeof *p3);
    if (!p3) { rc = -1; goto cleanup; }

    *b1 = p1; p1 = NULL;
    *b2 = p2; p2 = NULL;
    *b3 = p3; p3 = NULL;

cleanup:
    free(p1);
    free(p2);
    free(p3);
    return rc;
}
```
</details>

### Exercise 3: Custom Error Registry (Hard)
Create a thread-safe custom error recording module with functions `set_custom_error(int code, const char *fmt, ...)` and `const char* get_custom_error(void)` utilizing `_Thread_local` storage.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdarg.h>

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#define THREAD_LOCAL _Thread_local
#else
#define THREAD_LOCAL
#endif

static THREAD_LOCAL int t_err_code = 0;
static THREAD_LOCAL char t_err_msg[256] = "Success";

void set_custom_error(int code, const char *fmt, ...) {
    t_err_code = code;
    va_list args;
    va_start(args, fmt);
    vsnprintf(t_err_msg, sizeof t_err_msg, fmt, args);
    va_end(args);
}

const char* get_custom_error(void) {
    return t_err_msg;
}

int get_custom_error_code(void) {
    return t_err_code;
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* The Canonical Goto Cleanup Pattern */
int do_work(void) {
    int rc = -1;
    void *res1 = NULL;
    void *res2 = NULL;

    res1 = malloc(100);
    if (!res1) goto cleanup;

    res2 = malloc(200);
    if (!res2) goto cleanup;

    rc = 0; /* Success */

cleanup:
    free(res1);
    free(res2);
    return rc;
}

/* Safe errno idiom */
errno = 0;
long val = strtol(str, &end, 10);
if (errno != 0 || end == str || *end != '\0') { /* Error */ }
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §7.5 "Errors `<errno.h>`".
- Linux Kernel Coding Style: Chapter 7 "Centralized exiting of functions".
- SEI CERT C Coding Standard: "ERR30-C: Take care when reading errno".
- *The Practice of Programming* by Kernighan & Pike (Error Handling Design).

---

[← Previous: Multi-File Programs](./02-multi-file-programs.md) | [Section Index](./README.md) | [Next: Assertions & Defensive Coding →](./04-assertions-defensive-coding.md)
