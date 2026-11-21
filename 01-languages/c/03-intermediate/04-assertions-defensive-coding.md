# Assertions and Defensive Coding: Invariants, static_assert, and Safety

> Distinguishing bugs from runtime errors in C: runtime assertions, NDEBUG disabling semantics, compile-time static assertions, precondition verification, and defensive coding techniques.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [Error Handling](./03-error-handling.md), [Preprocessor](../02-core-concepts/09-preprocessor.md), [Functions](../01-fundamentals/08-functions.md) |
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

In software engineering, failures fall into two fundamentally distinct categories:
1. **Runtime Errors (Expected Failures)**: Conditions that can happen even in a bug-free program due to external factors (file not found, out of memory, lost network connection). These must be reported via return status codes.
2. **Programmer Bugs (Invariant Violations)**: Conditions that should be mathematically impossible if the software is implemented correctly (calling a function with an impossible `NULL` pointer, index out of bounds, broken data structure invariants).

**Assertions** are diagnostic probes that detect programmer bugs early during development and testing. When an assertion fails, the program immediately aborts execution, printing the failing expression, source file, line number, and function name. 

Combined with **Compile-Time Static Assertions (`static_assert`)** and defensive coding practices, assertions allow developers to catch structural defects during the compilation pass and prevent silent memory corruption before it propagates.

---

## 2. Core Concepts

### Runtime Assertions (`<assert.h>`)
The standard `assert(condition)` macro evaluates an expression at runtime:
- If `condition` evaluates to non-zero (true), execution continues unimpeded with minimal overhead.
- If `condition` evaluates to zero (false), `assert` prints a diagnostic message to `stderr` and invokes `abort()`, terminating execution and optionally producing a core dump for GDB debugging.

### The `NDEBUG` Macro (Release Builds)
If the preprocessor macro `NDEBUG` is defined before including `<assert.h>` (standard practice in release builds via `gcc -DNDEBUG -O2`), **all runtime assertions are disabled and expand to `((void)0)`**.

> [!CAUTION]
> ### The Cardinal Rule of Assertions
> Because assertions vanish completely in release builds, **never place expressions with side-effects inside an assertion**:
> ```c
> assert(fclose(fp) == 0); /* FATAL BUG: In release builds, fclose() is never called! */
> ```
> Always execute the action first, store the result, and assert on the stored variable:
> ```c
> int rc = fclose(fp);
> assert(rc == 0);
> (void)rc; /* Avoid unused variable warning in release builds */
> ```

### Compile-Time Static Assertions (`_Static_assert` / `static_assert`)
Introduced in C11 (`_Static_assert`) and formalized in C23 as the keyword `static_assert`:
```c
static_assert(sizeof(int) >= 4, "32-bit int required");
```
Static assertions are evaluated entirely by the compiler during translation. If the constant expression evaluates to false, compilation halts with a compiler error. They have **zero runtime CPU or memory overhead**.

---

## 3. How It Works Internally

### Runtime Assertion Expansion and Abort Sequence

```
Debug Build (Without -DNDEBUG):
  assert(ptr != NULL)
        │
        ▼ Expands to:
  if (!(ptr != NULL)) {
      __assert_fail("ptr != NULL", __FILE__, __LINE__, __func__);
  }
        │
        ▼ Output to stderr:
  a.out: main.c:24: test_fn: Assertion `ptr != NULL` failed.
        │
        ▼
  SIGABRT signal raised -> Process terminates, generates coredump.

Release Build (Compiled with -DNDEBUG):
  assert(ptr != NULL)  ──► Expands to: ((void)0) (Zero assembly instructions emitted!)
```

---

## 4. Syntax & Usage

```c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Compile-time static assertions (C11/C23) */
#ifndef static_assert
#define static_assert _Static_assert
#endif

static_assert(sizeof(void *) == 4 || sizeof(void *) == 8, "Pointer size unsupported");

/* Defensive function verifying preconditions */
double calculate_ratio(double numerator, double denominator) {
    /* Bug prevention probe */
    assert(denominator != 0.0 && "Denominator cannot be zero");
    return numerator / denominator;
}
```

> [!NOTE]
> ### Informative Assertion Messages
> A classic C trick adds a string explanation to assertions using the boolean AND `&&` operator:
> `assert(count > 0 && "Buffer count must be strictly positive");`
> Because string literals evaluate to non-zero pointers in boolean expressions, the assertion condition is unchanged, but the printed diagnostic error message includes the explanatory string literal.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates verifying compile-time architecture invariants with `static_assert` and validating runtime preconditions with `assert`.

```c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>

#ifndef static_assert
#define static_assert _Static_assert
#endif

/* Compile-time static assertions */
static_assert(sizeof(int) >= 4, "System requires at least 32-bit int");
static_assert(sizeof(void *) == sizeof(char *), "All data pointers must match pointer size");

static double calculate_mean(const int *arr, size_t count) {
    /* Defensive runtime invariant assertion */
    assert(arr != NULL && "Input array must not be NULL");
    assert(count > 0 && "Count must be greater than zero");

    long sum = 0;
    for (size_t i = 0; i < count; ++i) {
        sum += arr[i];
    }
    return (double)sum / (double)count;
}

int main(void) {
    printf("=== C Assertions and Defensive Coding Demo ===\n");

    int values[4] = {10, 20, 30, 40};
    double mean = calculate_mean(values, 4);
    printf("Calculated mean: %.2f\n", mean);

    printf("Static assertions verified successfully at compile time.\n");
    return 0;
}
```

```text
=== C Assertions and Defensive Coding Demo ===
Calculated mean: 25.00
Static assertions verified successfully at compile time.
```

---

## 6. Best Practices

1. **Use Assertions Exclusively for Internal Invariants and Programmer Bugs**: Never use `assert()` to validate untrusted user input, network payloads, or file existence. Use error status codes for runtime errors.
2. **Never Place Side-Effects Inside `assert()`**: In release builds (`-DNDEBUG`), assertion bodies are stripped entirely. Any function call or assignment inside `assert` will not execute in production.
3. **Use Static Assertions for Structural and Platform Assumptions**: Verify struct packing sizes, endian assumptions, and type alignments using `static_assert` to catch portability issues at build time.
4. **Assert Defensive Postconditions**: In complex algorithms (sorting, balancing trees, encryption), assert that the output satisfies expected invariants before returning.
5. **Use Defensive `default:` in `switch` Statements**: In an exhaustive enum `switch`, place `default: assert(0 && "Unhandled enum state"); abort();` to catch future unhandled enum cases.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Side effects inside `assert`** | `assert(read_byte() != -1)` is deleted in release builds (`-DNDEBUG`); function never runs. | Execute side effect outside assert: `int b = read_byte(); assert(b != -1);`. |
| **Using `assert` for input validation** | External malicious input causes release builds to bypass checks and crash or be exploited. | Return error codes for external inputs: `if (!valid) return ERR;`. |
| **Using `assert` for `malloc` failures** | Memory exhaustion is a runtime condition, not a code bug; crashes user application abruptly. | Handle `NULL` returns gracefully and return `ERR_NOMEM`. |
| **Shipping release builds with assertions enabled** | Assertion messages leak source code paths, variable names, and internal file layouts in binaries. | Compile release builds with `-DNDEBUG`. |
| **Unused variable warnings with `-DNDEBUG`** | Variables used solely in assertions trigger `-Wunused-variable` warnings in release builds. | Cast variable to void: `(void)var;`. |
| **Asserting on floating-point exact equality** | `assert(flt == 1.0f)` fails due to floating-point rounding inaccuracies. | Check within epsilon: `assert(fabs(flt - 1.0f) < 1e-6);`. |

---

## 8. Performance & Security Notes

### Zero Overhead in Release Builds
When compiling production binaries with `-DNDEBUG`, every assertion expands to `((void)0)`. The compiler generates zero assembly instructions, zero branch checks, and zero string footprint in `.rodata`. There is **absolute zero runtime cost**.

### Security: Crash Early vs. Exploit
A key philosophy of defensive programming is **"Fail Fast, Fail Loudly"**. When an internal pointer invariant is corrupted (e.g., due to an off-by-one heap slip), allowing the program to continue running leads to arbitrary memory corruption and remote code execution vulnerabilities. Aborting immediately via `assert` terminates the exploit vector.

---

## 9. Comparison: When to Use What

| Condition Type | Detection Tool | Action on Failure | Active in Release? | Example |
| :--- | :--- | :--- | :--- | :--- |
| **Build Configuration Bug** | `static_assert` | Compiler Error | Compile-Time Only | Struct size != 16 bytes |
| **Programmer Bug (Invariant)** | `assert()` | Prints trace & `abort()` | No (`-DNDEBUG` disables) | Internal pointer is NULL |
| **External Runtime Error** | `if (error)` | Returns status code | **Always** | Network dropped, file missing |
| **Fatal System Panic** | `abort()` / `exit()` | Terminates immediately | Always | Corrupted heap free list |

---

## 10. Interview Questions

<details>
<summary><strong>1. What happens to <code>assert()</code> when the macro <code>NDEBUG</code> is defined?</strong></summary>

Under ISO C (§7.2), if `NDEBUG` is defined as a macro name at the point in the source code where `<assert.h>` is included, the `assert()` macro is defined as:
`#define assert(ignore) ((void)0)`
It produces zero machine instructions and discards its argument completely.
</details>

<details>
<summary><strong>2. Why is <code>assert(malloc(100) != NULL);</code> a catastrophic bug?</strong></summary>

In debug builds, `malloc()` runs and the assertion verifies it succeeded. However, when compiled for production with `-DNDEBUG`, the preprocessor strips `assert(...)` entirely. The call to `malloc(100)` is **never executed**, resulting in an unallocated pointer and an immediate segmentation fault on the first write.
</details>

<details>
<summary><strong>3. What is the difference between <code>assert()</code> and <code>static_assert()</code>?</strong></summary>

- `assert()` is evaluated at **runtime**. It verifies dynamic conditions during execution, can be disabled by `NDEBUG`, and terminates the program via `abort()`.
- `static_assert()` is evaluated at **compile time**. It requires an integer constant expression, cannot be disabled by `NDEBUG`, and halts compilation with a compiler error if the condition is false.
</details>

<details>
<summary><strong>4. How can you silence compiler warnings about unused variables when assertions are disabled?</strong></summary>

If a variable `rc` is only used inside an assertion (`assert(rc == 0);`), compiling with `-DNDEBUG` causes compilers with `-Wall -Wextra` to emit `-Wunused-variable` warnings. Silence the warning by adding `(void)rc;` immediately following the assertion.
</details>

<details>
<summary><strong>5. Should a public library function validate arguments using <code>assert()</code>?</strong></summary>

No. Public APIs cannot control who calls them or whether callers pass invalid arguments. If a library function aborts via `assert()`, it crashes the entire host application. Public library APIs should validate arguments at runtime and return an explicit error code (`ERR_INVALID_PARAM`). Internal private helper functions can use `assert()`.
</details>

<details>
<summary><strong>6. Can <code>static_assert</code> be used inside a function block as well as at file scope?</strong></summary>

Yes. In C11 and C23, `static_assert` may be placed at file scope (outside all functions) or inside a block statement within a function body.
</details>

<details>
<summary><strong>7. What signal does a failing runtime assertion raise?</strong></summary>

A failing runtime assertion calls `abort()`, which raises the **`SIGABRT`** (Abort) signal. Unless caught by a custom signal handler, the operating system terminates the process and generates a core dump file.
</details>

<details>
<summary><strong>8. How does ISO C23 improve <code>static_assert</code>?</strong></summary>

In ISO C23:
1. `static_assert` becomes a standard first-class keyword (matching C++11).
2. The diagnostic string message parameter is made optional: `static_assert(sizeof(int) == 4);` is valid without needing a second string argument.
</details>

---

## 11. Practice Exercises

### Exercise 1: Compile-Time Endianness Check (Easy)
Write a static assertion verifying that `sizeof(uint32_t)` is exactly 4 bytes and `sizeof(uint64_t)` is exactly 8 bytes.

<details>
<summary>View Solution</summary>

```c
#include <stdint.h>
#include <assert.h>

#ifndef static_assert
#define static_assert _Static_assert
#endif

static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");
```
</details>

### Exercise 2: Defensive Binary Search (Medium)
Write a function `int binary_search(const int *arr, size_t n, int target)` that includes defensive assertions verifying that the input array pointer is not NULL, $n > 0$, and that the array is sorted before executing the search.

<details>
<summary>View Solution</summary>

```c
#include <assert.h>
#include <stddef.h>

static int is_sorted(const int *arr, size_t n) {
    for (size_t i = 1; i < n; i++) {
        if (arr[i] < arr[i - 1]) return 0;
    }
    return 1;
}

int binary_search(const int *arr, size_t n, int target) {
    assert(arr != NULL && "Search array cannot be NULL");
    assert(is_sorted(arr, n) && "Array must be sorted prior to binary search");

    size_t low = 0, high = n;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (arr[mid] == target) return (int)mid;
        if (arr[mid] < target) low = mid + 1;
        else high = mid;
    }
    return -1;
}
```
</details>

### Exercise 3: Custom Diagnostic Assert Macro (Hard)
Implement your own custom assertion macro `MY_ASSERT(cond, msg)` that prints file, line, function name, the custom message, and aborts, but allows registering a custom logger callback prior to termination.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>

typedef void (*AssertHandler)(const char *file, int line, const char *func, const char *msg);
static AssertHandler g_handler = NULL;

void set_assert_handler(AssertHandler h) {
    g_handler = h;
}

void custom_assert_fail(const char *file, int line, const char *func, const char *expr, const char *msg) {
    fprintf(stderr, "[CRITICAL ASSERT] %s:%d in %s(): '%s' failed. Reason: %s\n",
            file, line, func, expr, msg);
    if (g_handler) {
        g_handler(file, line, func, msg);
    }
    abort();
}

#ifndef NDEBUG
#define MY_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            custom_assert_fail(__FILE__, __LINE__, __func__, #cond, msg); \
        } \
    } while (0)
#else
#define MY_ASSERT(cond, msg) ((void)0)
#endif
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Runtime assertion */
#include <assert.h>
assert(ptr != NULL && "Pointer cannot be NULL");

/* Side-effect safe idiom */
int rc = do_action();
assert(rc == 0);
(void)rc;

/* Compile-time assertion */
static_assert(sizeof(Header) == 16, "Header layout changed");

/* Release build command */
gcc -DNDEBUG -O2 main.c -o app # Completely disables all assert() checks
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §7.2 "Diagnostics `<assert.h>`".
- ISO/IEC 9899:2024 (C23 Standard), §7.2 & §6.7.11 "Static assertions".
- *Writing Solid Code* by Steve Maguire (Chapter 2: "Assert Yourself").
- SEI CERT C Coding Standard: "ERR06-C: Understand the termination behavior of assert() and NDEBUG".

---

[← Previous: Error Handling](./03-error-handling.md) | [Section Index](./README.md) | [Next: Bit Manipulation →](./05-bit-manipulation.md)
