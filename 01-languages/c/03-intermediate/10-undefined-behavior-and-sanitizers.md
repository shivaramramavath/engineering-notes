# 10 Undefined Behavior & Sanitizers

> Master the classification of C behaviors, understand how optimizing compilers exploit Undefined Behavior, and detect memory and arithmetic corruption using ASan, UBSan, and Valgrind.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [01-fundamentals/03-integer-representation.md](../01-fundamentals/03-integer-representation.md), [02-core-concepts/08-memory-management.md](../02-core-concepts/08-memory-management.md), [03-intermediate/04-assertions-defensive-coding.md](04-assertions-defensive-coding.md) |
| **Time to Read** | 30 minutes |
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

Undefined Behavior (UB) is one of the most misunderstood yet fundamental concepts in the C programming language. In C, if a program contains an operation with undefined behavior, the ISO standard imposes **no requirements whatsoever**: the program may crash with a segmentation fault, produce subtly corrupted calculations, or appear to run completely fine until recompiled with a different compiler version or optimization flag.

Historically, UB was granted to compiler writers so that C could generate blazing-fast machine code without emitting runtime safety checks for every memory access or arithmetic operation. However, modern optimizing compilers (GCC, Clang) treat UB as a mathematical contract:
- The compiler **assumes that undefined behavior never occurs**.
- If a code branch would inevitably cause undefined behavior, the optimizer can delete that entire branch as "dead code."
- Optimizers can reorder instructions, eliminate null-pointer checks, and assume signed loops never overflow.

To build secure, robust systems, developers must identify all classes of non-deterministic behavior and employ runtime sanitizers (AddressSanitizer, UBSan) and dynamic instrumentation tools (Valgrind) during development and automated testing.

---

## 2. Core Concepts

### 2.1 The Four Classes of Behavior in ISO C
The ISO C standard classifies program behavior into four distinct categories:
1. **Defined Behavior**: Explicitly specified by the standard (e.g. `1 + 1 == 2`, unsigned integer addition wraps modulo $2^N$).
2. **Implementation-Defined Behavior**: Unspecified behavior where the compiler vendor must document their choice (e.g. the size of `int`, whether signed char has sign extension, the outcome of right-shifting a negative signed integer).
3. **Unspecified Behavior**: Well-formed code where the standard allows two or more possibilities without requiring documentation (e.g. argument evaluation order in function calls: `f(g(), h())`).
4. **Undefined Behavior (UB)**: Behavior upon use of a non-portable or erroneous construct for which the standard imposes no requirements. The entire execution of the program is invalidated.

### 2.2 The Catalog of Critical Undefined Behaviors
Systems developers encounter these primary UB categories:
- **Signed Integer Overflow**: `INT_MAX + 1` (unlike unsigned overflow, which is defined).
- **Null Pointer Dereference**: `*ptr` when `ptr == NULL`.
- **Out-of-Bounds Memory Access**: Reading or writing past the allocated boundaries of an array or heap block.
- **Use-After-Free & Double Free**: Accessing or freeing memory that was already released via `free()`.
- **Reading Uninitialized Memory**: Accessing uninitialized local variables containing stack garbage.
- **Strict Aliasing Violations**: Accessing an object through a pointer of an incompatible type (e.g. casting `int *` to `float *` and dereferencing).
- **Misaligned Pointer Dereference**: Casting a byte pointer to `uint32_t *` and dereferencing at an address not divisible by 4.
- **Invalid Bit Shift**: Shifting by a negative value or by a count greater than or equal to the bit width of the operand (`x << 32` for 32-bit `x`).

### 2.3 The Modern Sanitizer Suite
LLVM and GCC provide compiler-instrumented sanitizers:
- **AddressSanitizer (ASan)** (`-fsanitize=address`): Detects out-of-bounds accesses (heap, stack, global), use-after-free, double-free, and memory leaks.
- **UndefinedBehaviorSanitizer (UBSan)** (`-fsanitize=undefined`): Detects signed integer overflow, misaligned pointer dereferences, invalid shifts, division by zero, and null pointer dereferences.
- **ThreadSanitizer (TSan)** (`-fsanitize=thread`): Detects data races between concurrent threads.
- **MemorySanitizer (MSan)** (`-fsanitize=memory`): Detects reads of uninitialized memory (Clang-specific).

---

## 3. How It Works Internally

### How Optimizers Exploit UB (The "Time Travel" Optimization)
Consider this common defensive mistake:
```c
void process_packet(Packet *pkt) {
    int len = pkt->length; /* Dereference occurs here */
    if (pkt == NULL) {      /* Defensive check */
        return;
    }
    handle_data(pkt->data, len);
}
```
To a human developer, this looks like a misplaced defensive check. To an optimizing compiler (GCC/Clang `-O2`):
1. In the first line, `pkt->length` dereferences `pkt`.
2. Dereferencing a NULL pointer is Undefined Behavior.
3. The compiler assumes the programmer did not write UB. Therefore, `pkt` **cannot be NULL**.
4. Consequently, the condition `if (pkt == NULL)` evaluates to constantly false.
5. **The compiler removes the entire `if` check from the generated assembly!**

If an attacker passes `NULL`, the program will not safely return; it crashes or executes arbitrary subsequent logic without the check.

### AddressSanitizer Shadow Memory Architecture
ASan instruments every memory load and store by mapping 1/8th of the virtual address space into a dedicated **Shadow Memory** region:

```
Virtual Memory to Shadow Memory Mapping (64-bit):
Application Virtual Address (Addr):
┌────────────────────────────────────────────────────────┐
│ 0x0000000000000000 ................ 0x7FFFFFFFFFFFFFFF │
└──────────────────────────┬─────────────────────────────┘
                           │ Shadow = (Addr >> 3) + Offset
                           ▼
Shadow Memory:
┌────────────────────────────────────────────────────────┐
│ [0] All 8 bytes valid                                  │
│ [k] First k bytes valid (1 <= k <= 7)                  │
│ [Negative] Poisoned / Redzone (Heap, Stack, Freed)     │
└────────────────────────────────────────────────────────┘
```

When memory is allocated via `malloc()`, ASan surrounds it with "redzones" (poisoned memory). On every pointer dereference, ASan executes a fast 2-instruction check against the shadow byte:
```c
byte_shadow = *(app_addr >> 3 + Offset);
if (byte_shadow != 0 && (app_addr & 7) >= byte_shadow) {
    ReportCrashAndStackTrace();
}
```
If your program touches a redzone or accesses memory after `free()` (which paints the shadow memory with poison codes), ASan halts immediately and outputs a detailed crash report with stack traces.

---

## 4. Syntax & Usage

### 4.1 Recommended Debug & Sanitizer Compilation Flags
When building for testing and continuous integration:
```bash
# GCC & Clang with ASan + UBSan
gcc -std=c11 -Wall -Wextra -Wpedantic -g -O1     -fsanitize=address,undefined     -fno-omit-frame-pointer     program.c -o program.exe
```

### 4.2 Checked Integer Arithmetic (C23 & GCC Builtins)
To perform arithmetic without triggering undefined behavior:
- **GCC / Clang Builtins**:
  ```c
  int res;
  if (__builtin_add_overflow(a, b, &res)) {
      /* Handle overflow safely */
  }
  ```
- **C23 Standard Header `<stdckdint.h>`**:
  ```c
  #include <stdckdint.h>
  int res;
  if (ckd_add(&res, a, b)) {
      /* Overflow detected */
  }
  ```

---

## 5. Practical Examples

### Comprehensive Example: Safe Arithmetic & Bounds Verification
The following program demonstrates defensive programming patterns that detect signed overflow and boundary violations before undefined behavior can occur.

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

/* Safe checked arithmetic preventing undefined behavior */
static bool safe_add_signed(int a, int b, int *result) {
    if ((b > 0 && a > INT_MAX - b) ||
        (b < 0 && a < INT_MIN - b)) {
        return false; /* Signed overflow would be Undefined Behavior */
    }
    *result = a + b;
    return true;
}

static bool safe_multiply_signed(int a, int b, int *result) {
    if (a > 0) {
        if (b > 0 && a > INT_MAX / b) return false;
        if (b < 0 && b < INT_MIN / a) return false;
    } else if (a < 0) {
        if (b > 0 && a < INT_MIN / b) return false;
        if (b < 0 && b < INT_MAX / a) return false;
    }
    *result = a * b;
    return true;
}

int main(void) {
    printf("=== Undefined Behavior Prevention and Detection ===\n");

    /* 1. Signed arithmetic overflow prevention */
    int res = 0;
    int a1 = 2000000000, b1 = 1500000000;
    printf("Attempting signed addition: %d + %d...\n", a1, b1);
    if (safe_add_signed(a1, b1, &res)) {
        printf("  Result: %d\n", res);
    } else {
        printf("  DETECTED OVERFLOW: Operation safely prevented before UB occurred.\n");
    }

    int a2 = 100000, b2 = 50000;
    printf("Attempting signed multiplication: %d * %d...\n", a2, b2);
    if (safe_multiply_signed(a2, b2, &res)) {
        printf("  Result: %d\n", res);
    } else {
        printf("  DETECTED OVERFLOW: Multiplication exceeds INT_MAX (%d).\n", INT_MAX);
    }

    /* 2. Bounds checking demonstration */
    int arr[4] = {10, 20, 30, 40};
    int requested_index = 5;
    printf("Accessing array of size 4 at index %d...\n", requested_index);
    if (requested_index >= 0 && (size_t)requested_index < (sizeof(arr)/sizeof(arr[0]))) {
        printf("  Value: %d\n", arr[requested_index]);
    } else {
        printf("  OUT-OF-BOUNDS PREVENTED: Index %d rejected (valid range 0..3).\n", requested_index);
    }

    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 10-undefined-behavior-and-sanitizers.c -o 10-undefined-behavior-and-sanitizers.exe
./10-undefined-behavior-and-sanitizers.exe
```

```text
=== Undefined Behavior Prevention and Detection ===
Attempting signed addition: 2000000000 + 1500000000...
  DETECTED OVERFLOW: Operation safely prevented before UB occurred.
Attempting signed multiplication: 100000 * 50000...
  DETECTED OVERFLOW: Multiplication exceeds INT_MAX (2147483647).
Accessing array of size 4 at index 5...
  OUT-OF-BOUNDS PREVENTED: Index 5 rejected (valid range 0..3).
```

---

## 6. Best Practices

1. **Always Check for Overflow BEFORE Executing Operations**: Checking `if (a + b < a)` is illegal because signed overflow is undefined behavior; the compiler can legally optimize away the check entirely.
2. **Never Dereference Pointers Before Validating Them**: Perform all `if (ptr == NULL)` guards before reading any members of the struct.
3. **Compile Continuous Integration Test Suites with ASan and UBSan**: Run your test suite with `-fsanitize=address,undefined` enabled on every pull request.
4. **Zero Out Pointers Immediately After `free()`**: Assign `ptr = NULL;` after `free(ptr)`. If a subsequent line attempts to read it, it produces a clear null-dereference crash rather than silent heap corruption or use-after-free exploits.
5. **Initialize All Stack Variables**: Always initialize automatic variables upon declaration (`int count = 0;`, `char buf[64] = {0};`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| `if (x + 100 < x)` to check overflow | Signed overflow is UB. Modern compilers optimize this entire `if` condition to `false` and delete it! | Use boundary check: `if (x > INT_MAX - 100)`. |
| `int *p; if (cond) p = malloc(...); *p = 5;` | If `cond` is false, `p` contains uninitialized stack garbage; dereferencing it corrupts random memory. | Initialize `int *p = NULL;` and verify `p != NULL`. |
| Freeing and accessing: `free(node); return node->val;` | Classic Use-After-Free (UAF). Memory may have been reallocated by another thread. | Save the value first: `int val = node->val; free(node); return val;`. |
| Casting `(float *)&int_var` | Strict aliasing violation. Compiler may reorder reads and writes, producing invalid results. | Use `memcpy()` or `union` for type punning. |
| Shifting 32-bit int by 32: `1u << 32` | Shifting by width $\ge$ bit count is Undefined Behavior in ISO C. | Ensure shift amount is strictly bounded $[0, 31]$. |
| `realloc(p, new_sz)` without temp pointer | If `realloc` fails, it returns `NULL`, but the original memory block at `p` remains allocated. Assigning `p = realloc(p, ...)` leaks the block. | Use temporary pointer: `void *tmp = realloc(p, ...);`. |

---

## 8. Performance & Security Notes

- **ASan Runtime Overhead**: AddressSanitizer typically introduces approximately a 2x CPU slowdown and 2x–3x virtual memory overhead. It is designed for testing, fuzzing, and staging environments, not production binary deployment.
- **UBSan Minimal Overhead**: UndefinedBehaviorSanitizer introduces minimal CPU overhead (~5%), making it feasible for staging and high-security services.
- **Exploiting UB for Code Execution**: Memory corruption bugs (buffer overflows, UAFs) allow attackers to overwrite return addresses on the stack or function pointers on the heap, hijacking the CPU's instruction pointer (`RIP`) and achieving arbitrary remote code execution (RCE).
- **Valgrind vs ASan**: ASan instruments code at compile-time and is roughly 10x faster than Valgrind. However, Valgrind operates directly on compiled x86 binaries without requiring recompilation and can detect uninitialized reads that ASan might miss.

---

## 9. Comparison / When to Use What

| Tool | Mechanism | Performance Impact | Requires Recompile? | Best For |
| :--- | :--- | :--- | :--- | :--- |
| **AddressSanitizer (ASan)** | Compiler instrumentation | ~2x slowdown | Yes (`-fsanitize=address`) | Out-of-bounds, Use-After-Free, double free |
| **UBSan** | Compiler checks | ~5% slowdown | Yes (`-fsanitize=undefined`) | Integer overflow, bad shifts, alignment |
| **Valgrind Memcheck** | Dynamic binary translation | 10x–30x slowdown | No (Works on any binary) | Production binaries without source code |
| **Static Analyzers (`clang-tidy`, `cppcheck`)** | Abstract syntax tree analysis | Zero runtime impact | No | Catching bug patterns before compilation |
| **C23 `<stdckdint.h>`** | Standard language header | Single assembly flag check | Yes | Production-grade checked math |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between Undefined Behavior and Implementation-Defined Behavior?</strong></summary>

Implementation-Defined Behavior represents well-formed code where the ISO standard allows the compiler vendor to choose among several possibilities, but **mandates** that the vendor document their choice in compiler manuals (e.g. `sizeof(int)`, sign extension on right shifts). In contrast, Undefined Behavior imposes **zero requirements** on the compiler or runtime. The compiler is free to assume the condition can never occur and may generate nonsensical assembly, delete security checks, or crash the application.
</details>

<details>
<summary><strong>2. Why does the optimizing compiler delete the check `if (x + 1 < x)`?</strong></summary>

In ISO C, signed integer arithmetic overflow is Undefined Behavior. Therefore, the compiler operates under the mathematical assumption that `x + 1` will never wrap around to a negative number; mathematically, $x + 1$ is strictly greater than $x$. Because the condition $x + 1 < x$ is mathematically impossible under the axiom that signed overflow never happens, the compiler evaluates the condition to constant false and eliminates the entire branch as dead code during optimization passes.
</details>

<details>
<summary><strong>3. How does AddressSanitizer (ASan) detect out-of-bounds accesses on heap memory?</strong></summary>

ASan intercepts `malloc()` calls and allocates extra "redzone" memory surrounding each user buffer. It maps the virtual address space to a "shadow memory" area where 1 byte of shadow tracks the accessibility of 8 bytes of application memory. Redzones and freed blocks are marked as poisoned in the shadow memory. The compiler injects inline instructions before every load and store to inspect the shadow byte. If the target address corresponds to a poisoned shadow byte, ASan immediately traps and outputs a diagnostics report.
</details>

<details>
<summary><strong>4. Why is reading an uninitialized local variable undefined behavior?</strong></summary>

Automatic (stack) variables are not initialized by the C runtime for performance reasons; they contain whatever leftover bit patterns previously occupied that stack slot. On architectures with trap representations or parity bits (and modern register architectures), reading an uninitialized variable can load invalid register states or cause the compiler to optimize assuming the variable holds an arbitrary or impossible value, leading to unstable branching.
</details>

<details>
<summary><strong>5. Explain the Strict Aliasing Rule and how violating it causes undefined behavior.</strong></summary>

The Strict Aliasing Rule states that two pointers of different types (with few exceptions, like `char *`) cannot point to the same memory location. This allows the compiler to assume that writing through an `int *` will never modify the value read through a `float *`, enabling aggressive register caching. If code violates this rule by casting `(float *)&my_int` and dereferencing, the optimizer may reorder the memory operations, causing stale or corrupted reads.
</details>

<details>
<summary><strong>6. What is a "Time-Travel Bug" caused by compiler UB assumptions?</strong></summary>

A time-travel bug occurs when a compiler optimization based on an undefined behavior occurring later in the code causes effects that appear to happen **before** the undefined behavior is executed. For example, if a function dereferences a pointer and later checks `if (ptr == NULL)`, the compiler's deduction that `ptr` cannot be null causes it to erase the null check retrospectively, leading to crashes or security check bypasses earlier in execution.
</details>

<details>
<summary><strong>7. How does C23 `<stdckdint.h>` improve integer safety?</strong></summary>

C23 introduced `<stdckdint.h>` with standardized, type-generic functions: `ckd_add(&result, a, b)`, `ckd_sub(&result, a, b)`, and `ckd_mul(&result, a, b)`. These functions perform the arithmetic and return a `bool` indicating whether overflow occurred. They compile directly to single-cycle processor condition flags (such as the x86 Overflow Flag `OF`), replacing complex manual boundary comparison formulas.
</details>

<details>
<summary><strong>8. Why is Valgrind significantly slower than AddressSanitizer?</strong></summary>

AddressSanitizer uses compile-time instrumentation: the compiler inserts 2–3 assembly instructions directly before every memory access, resulting in a fast ~2x overhead. In contrast, Valgrind is a dynamic binary translation framework: it disassembles the compiled machine code at runtime into an intermediate representation (VEX IR), instruments every memory read/write and register tracking bit, and JIT-compiles it back to machine code, incurring a 10x to 30x performance slowdown.
</details>

---

## 11. Practice Exercises

### Easy: Safe Subtraction Without Signed Underflow
Write a function `bool safe_subtract_signed(int a, int b, int *res)` that subtracts `b` from `a` and returns `false` if underflow or overflow would occur.

<details>
<summary>Solution</summary>

```c
#include <limits.h>
#include <stdbool.h>

bool safe_subtract_signed(int a, int b, int *res) {
    if ((b < 0 && a > INT_MAX + b) ||
        (b > 0 && a < INT_MIN + b)) {
        return false;
    }
    *res = a - b;
    return true;
}
```
</details>

### Medium: Defensive Pointer Dereference Guard
Write a macro or inline helper that safely retrieves an integer value from a nested struct pointer chain, returning a default fallback value if any pointer in the chain is NULL.

<details>
<summary>Solution</summary>

```c
#include <stddef.h>

typedef struct NodeC {
    int value;
} NodeC;

typedef struct NodeB {
    NodeC *c;
} NodeB;

typedef struct NodeA {
    NodeB *b;
} NodeA;

int safe_get_value(const NodeA *a, int fallback) {
    if (!a) return fallback;
    if (!a->b) return fallback;
    if (!a->b->c) return fallback;
    return a->b->c->value;
}
```
</details>

### Hard: Zero-Allocation Checked Multiplication
Write a function `bool safe_size_multiply(size_t nmemb, size_t size, size_t *out_bytes)` that validates whether an allocation size calculation `nmemb * size` will overflow `SIZE_MAX`.

<details>
<summary>Solution</summary>

```c
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

bool safe_size_multiply(size_t nmemb, size_t size, size_t *out_bytes) {
    if (nmemb != 0 && size > SIZE_MAX / nmemb) {
        return false; /* Overflow detected */
    }
    *out_bytes = nmemb * size;
    return true;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Sanitizer / Flag | Target Bug Class | Performance Overhead |
| :--- | :--- | :--- |
| `-fsanitize=address` | Buffer overflows, use-after-free, double free | ~2x slowdown |
| `-fsanitize=undefined` | Signed overflow, null deref, invalid shift | ~5% slowdown |
| `-fsanitize=thread` | Multithreaded data races | ~5x slowdown |
| `-fsanitize=leak` | Memory leaks (part of ASan on Linux) | Minimal |
| `-fno-omit-frame-pointer` | Preserves stack frames for readable traces | Negligible |
| `<stdckdint.h>` (C23) | Portable single-cycle overflow detection | 0% (Hardware flags) |

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17) §3.4.3 (Definition of undefined behavior), Annex J.2 (Comprehensive list of undefined behaviors).
- SEI CERT C Coding Standard: Rule 04. Integers (INT) & Rule 08. Memory Management (MEM).
- Chris Lattner, *What Every C Programmer Should Know About Undefined Behavior* (LLVM Blog series, 2011).
- Google AddressSanitizer Documentation (https://github.com/google/sanitizers/wiki/AddressSanitizer).

---

[← Previous](09-time-and-random.md) | [Section Index](README.md) | [Next →](11-standard-library.md)
