# Undefined, Unspecified, and Implementation-Defined Behavior

> The foundational triad of non-deterministic behavior in C: ISO definitions, how modern optimizers exploit Undefined Behavior (UB), time-travel optimizations, implementation-defined limits (`<limits.h>`), unspecified evaluation orders, and detection tooling.

| Metadata | Details |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01-program-structure.md](./01-program-structure.md), [05-operators.md](./05-operators.md) |
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

In most modern programming languages (Java, Rust, Python, Go), the language runtime guarantees memory safety, handles exceptions, or traps illegal operations deterministically. C is fundamentally different: the ISO standard deliberately leaves certain operations **non-deterministic** to allow compilers to generate the most efficient possible machine code for any target CPU architecture.

Failing to understand these boundaries is the primary reason C programs "break" when upgrading compiler versions or moving from `-O0` to `-O2`:
1. **The Optimizer Assumption**: Compilers optimize code under the mathematical assumption that **Undefined Behavior never occurs**. If a code path contains UB, the compiler is legally permitted to delete surrounding security checks, reorder writes, or eliminate entire functions!
2. **Portability Blindspots**: Code relying on Implementation-Defined behavior (such as `sizeof(long) == 8` or signed `char`) silently corrupts data when compiled on Windows or ARM targets.
3. **Ghost Bugs (Heisenbugs)**: A program invoking UB may appear to function perfectly on one compiler, only to crash or leak data when compiled with `-O3` or executed in production.

---

## 2. Core Concepts

### The ISO C Non-Deterministic Taxonomy

The ISO C specification defines four distinct categories of non-standard behavior:

| Behavior Category | Standard Definition | Compiler Obligation | Classic Examples |
|---|---|---|---|
| **Undefined Behavior (UB)** | Behavior upon use of a non-portable or erroneous construct for which the Standard imposes **no requirements** | None (Can crash, execute formatting code, or optimize away) | Null pointer dereference, signed integer overflow, out-of-bounds array access, use-after-free |
| **Unspecified Behavior** | Use of an unspecified value, or two or more possibilities where the Standard imposes no requirement on which is chosen | None (Does not need to document choice) | Order of evaluation of function call arguments, order of sub-expression evaluation |
| **Implementation-Defined**| Unspecified behavior where each implementation **must document** how the choice is made | **Mandatory Documentation** in compiler manual | `sizeof(int)`, signedness of plain `char`, behavior of bitwise right-shift on signed negative numbers |
| **Locale-Specific** | Behavior depending on local conventions of nationality, culture, and language | Documented in runtime locale | Formatting of currency, decimal point character (comma vs period), collation order |

```
+-------------------------------------------------------------------------+
|                  NON-DETERMINISTIC BEHAVIOR TAXONOMY                    |
+-------------------------------------------------------------------------+
|  [ Undefined Behavior (UB) ]  --> ILLEGAL. Standard imposes NO RULES.   |
|                                   Optimizers assume it NEVER happens.   |
|                                                                         |
|  [ Unspecified Behavior ]     --> LEGAL. Compiler chooses an option;   |
|                                   NOT documented. (e.g. arg order)      |
|                                                                         |
|  [ Implementation-Defined ]   --> LEGAL. Compiler chooses an option;   |
|                                   MUST DOCUMENT in manual. (e.g. types) |
+-------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### How Modern Optimizers Exploit Undefined Behavior

Optimizing compilers (GCC, Clang) perform **Dead-Code Elimination (DCE)** using data-flow value ranges. Consider this security check:

```c
void authenticate_and_dispatch(int *user_ptr) {
    int id = *user_ptr; /* Line 1: Dereference user_ptr */
    
    if (user_ptr == NULL) { /* Line 3: Null check */
        audit_log_security_breach();
        return;
    }
    
    process_user(id);
}
```

#### What the Optimizer Does Under `-O2`:
1. On Line 1, the compiler observes `*user_ptr`. If `user_ptr == NULL`, dereferencing it is **Undefined Behavior**.
2. The optimizer operates on the axiom: *"A conforming C program never executes Undefined Behavior."*
3. Therefore, for execution to successfully reach Line 3, `user_ptr` **cannot be NULL**!
4. The condition `user_ptr == NULL` is evaluated as **mathematically impossible (false)** at compile time.
5. The optimizer **completely deletes Line 3, Line 4, and Line 5**!
6. The security breach audit log is erased from the generated binary!

```
Generated Assembly (-O2):
authenticate_and_dispatch:
    mov     edi, DWORD PTR [rdi]    ; Dereference user_ptr
    jmp     process_user            ; Null check was completely deleted!
```

---

## 4. Syntax & Usage

### 1. Documenting Implementation-Defined Limits in Code
```c
#include <stdio.h>
#include <limits.h>

void print_system_parameters(void) {
    /* Implementation-defined char signedness */
#if CHAR_MIN < 0
    printf("Host char signedness: Signed (-fsigned-char)\n");
#else
    printf("Host char signedness: Unsigned (-funsigned-char)\n");
#endif

    /* Implementation-defined integer bit widths */
    printf("Bits per byte (CHAR_BIT): %d\n", CHAR_BIT);
    printf("INT_MAX:  %d\n", INT_MAX);
    printf("LONG_MAX: %ld\n", LONG_MAX);
}
```

### 2. Eliminating Unspecified Argument Evaluation Order
```c
/* UNRELIABLE (Unspecified evaluation order): */
int total = calculate(get_next_id(), get_next_id());

/* CONFORMING & DETERMINISTIC: */
int id_first = get_next_id();
int id_second = get_next_id();
int total = calculate(id_first, id_second);
```

---

## 5. Practical Examples

### Non-Deterministic Behaviors Demonstration Program

```c
/* File: 01-languages/c/01-fundamentals/examples/10-undefined-unspecified-implementation-defined.c */
#include <stdio.h>
#include <limits.h>

int main(void) {
    printf("=== The Triad of Non-Deterministic Behaviors ===\n");
    
    /* 1. Implementation-Defined Behavior: Character signedness & integer width */
#if CHAR_MIN < 0
    printf("[Implementation-Defined] 'char' is SIGNED on this target\n");
#else
    printf("[Implementation-Defined] 'char' is UNSIGNED on this target\n");
#endif
    printf("[Implementation-Defined] sizeof(int) = %zu bytes, sizeof(long) = %zu bytes\n",
           sizeof(int), sizeof(long));

    /* 2. Unspecified Behavior: Evaluation order of function arguments */
    printf("[Unspecified] Evaluation order of operands in expressions is compiler-dependent\n");

    /* 3. Undefined Behavior: Guarding against signed overflow */
    int a = INT_MAX;
    printf("[Defined Guard] INT_MAX is %d; adding 1 without check would be Undefined Behavior!\n", a);
    
    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 10-undefined-unspecified-implementation-defined.c -o run_10.exe
./run_10.exe
```
Output:
```text
=== The Triad of Non-Deterministic Behaviors ===
[Implementation-Defined] 'char' is SIGNED on this target
[Implementation-Defined] sizeof(int) = 4 bytes, sizeof(long) = 4 bytes
[Unspecified] Evaluation order of operands in expressions is compiler-dependent
[Defined Guard] INT_MAX is 2147483647; adding 1 without check would be Undefined Behavior!
```

---

## 6. Best Practices

1. **Never Assume UB Will Cause an Immediate Crash**: Undefined behavior does not guarantee a segmentation fault. It frequently runs silently while producing corrupt data or deleting defensive security checks.
2. **Build Continuous Integration with Sanitizers**: Always run automated test suites with `-fsanitize=undefined,address`. UndefinedBehaviorSanitizer (UBSan) inserts runtime checks that trap integer overflow, misaligned pointers, and null dereferences.
3. **Check Preconditions Before Performing Dangerous Operations**:
   - Check `ptr != NULL` **before** dereferencing.
   - Check `a > INT_MAX - b` **before** adding signed integers.
   - Check `index < length` **before** indexing an array.
4. **Never Write Code That Depends on Argument Evaluation Order**: Always evaluate modifying sub-expressions on separate lines.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Null check after dereference (`int x = *p; if (!p) ...`) | Dereference establishes assumption that `p != NULL`; optimizer deletes the null check! | Perform null check **before** dereferencing. |
| Checking signed overflow after adding (`if (a + b < a)`) | Signed overflow is UB; compiler deletes the check under `-O2`. | Check preconditions: `if (a > INT_MAX - b)`. |
| Assuming `char` is signed on all platforms | ARM and PowerPC default to `unsigned char`, breaking `< 0` EOF checks! | Use `signed char` or store return from `getchar()` in `int`. |
| Relying on struct padding bit values | The contents of padding bytes between struct members are **unspecified**. | Zero structs with `memset(&s, 0, sizeof(s))` before comparison or transmission. |
| Reading uninitialized local variables | Reads indeterminate stack memory; compiler can assume uninitialized variable holds any arbitrary value. | Initialize on declaration: `int count = 0;`. |
| Shifting a 32-bit int by 32 bits (`val << 32`) | Bit shift amounts $\ge$ bit width invoke undefined behavior. | Validate `shift < 32`. |

---

## 8. Performance & Security Notes

- **Time-Travel Optimization**: Compilers reorder operations aggressively. If a bug invoking UB is reached in a function, the compiler's optimizer can emit code that assumes the condition causing the UB was true *even before the line of code is reached in program order*.
- **Compiler Flags for Defensive Control**:
  - `-fno-delete-null-pointer-checks`: Prevents the compiler from deleting null checks following a dereference.
  - `-fwrapv`: Instructs GCC/Clang to treat signed integer overflow as defined two's-complement wrap-around rather than undefined behavior (at a slight optimization penalty).

---

## 9. Comparison / When to Use What

| Category | Standard Requirement | Documented by Vendor? | Sanitizer Detection Tool |
|---|---|---|---|
| **Undefined Behavior** | None (Total freedom) | No | `-fsanitize=undefined,address` |
| **Implementation-Defined** | Implementation must choose | **Yes** (In compiler manual) | N/A (Conforming) |
| **Unspecified Behavior** | Multiple legal possibilities | No | ThreadSanitizer (if data race) |
| **Portability Hazard** | Non-standard dialect extension | Compiler manual | `-Wpedantic -pedantic-errors` |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between Undefined Behavior and Implementation-Defined Behavior?</strong></summary>

Undefined Behavior imposes **zero requirements** on the compiler; the program is non-conforming, and the compiler may generate any code (including deleting checks or crashing). Implementation-Defined Behavior represents a well-defined construct where the compiler is given a choice between options, but is **mandated by the ISO standard to document its choice** in the compiler documentation (e.g. the bit-width of integers or signedness of `char`).
</details>

<details>
<summary><strong>2. Why is signed integer overflow undefined behavior, while unsigned integer overflow is not?</strong></summary>

Signed overflow was designated as UB so that compilers could optimize loops aggressively under the assumption that `i + 1 > i` is always true, and to accommodate historical hardware architectures that fired hardware traps on signed overflow. Unsigned integers were explicitly standardized to model modular abstract algebra ($\pmod{2^N}$).
</details>

<details>
<summary><strong>3. What does "Time-Travel Optimization" mean in the context of C compilers?</strong></summary>

Because the compiler assumes Undefined Behavior is impossible, if a code path inevitably hits UB under certain conditions, the compiler can reason backward in time and deduce that those conditions could never have occurred in earlier code, deleting defensive checks that preceded the buggy line.
</details>

<details>
<summary><strong>4. Give three examples of Implementation-Defined behavior in C.</strong></summary>

1. The exact number of bits in a `char` (`CHAR_BIT >= 8`).
2. Whether plain `char` is signed or unsigned.
3. The result of a bitwise right-shift on a negative signed integer (arithmetic vs logical shift).
</details>

<details>
<summary><strong>5. Give three examples of Unspecified behavior in C.</strong></summary>

1. The order of evaluation of function call arguments (`f(g(), h())`).
2. The order of evaluation of sub-expressions in binary operations (`a() + b()`).
3. The layout and value of padding bytes between structure members.
</details>

<details>
<summary><strong>6. Why does the compiler delete `if (p == NULL)` in `int x = *p; if (p == NULL) return;` under `-O2`?</strong></summary>

Dereferencing `*p` is undefined behavior if `p` is `NULL`. The optimizer assumes the program is valid and therefore concludes `p` cannot be `NULL`. Consequently, the subsequent `p == NULL` condition is evaluated as compile-time false and eliminated as dead code.
</details>

<details>
<summary><strong>7. How does UndefinedBehaviorSanitizer (UBSan) detect UB at runtime?</strong></summary>

UBSan inserts instrumentation instructions before arithmetic operations, memory dereferences, and shifts. For example, before a signed addition `a + b`, UBSan inserts a check querying CPU overflow flags; if an overflow is detected, UBSan prints a diagnostic stack trace and aborts execution.
</details>

<details>
<summary><strong>8. What compiler flag forces signed integer overflow to behave deterministically as two's complement wrap-around?</strong></summary>

`-fwrapv`. It instructs GCC and Clang to treat signed integer overflow as defined two's-complement modular arithmetic, disabling optimizations that assume signed overflow never occurs.
</details>

---

<details>
<summary><strong>9. What is `unreachable()` in C23?</strong></summary>

ISO C23 introduced the `unreachable()` macro in `<stddef.h>`. It informs the compiler that a program execution point can never be reached under valid execution. The compiler uses this information to optimize away branch checks and dead code. Calling `unreachable()` when that execution path is actually reached invokes undefined behavior.
</details>

<details>
<summary><strong>10. Can undefined behavior occur at compile time?</strong></summary>

No. Undefined behavior is strictly a runtime concept regarding the execution of the abstract C machine. However, compilers frequently emit warnings (or errors under `-Werror`) when static data-flow analysis proves that an operation will inevitably invoke undefined behavior at runtime (e.g. constant division by zero or constant out-of-bounds indexing).
</details>

<details>
<summary><strong>11. What is the difference between a hardware exception (trap) and compiler undefined behavior?</strong></summary>

A hardware exception (like an MMU page fault on dereferencing address zero, or an x86 integer division exception) is a physical CPU event that stops execution and invokes an OS interrupt handler. Compiler Undefined Behavior is a language specification rule: the compiler is allowed to assume the event never occurs and can emit code that omits instructions entirely.
</details>

<details>
<summary><strong>12. Why does shifting a signed negative integer right invoke implementation-defined behavior?</strong></summary>

Because computer architectures historically differed in how signed shifts were implemented. Arithmetic right shift (`sar` on x86) sign-extends the MSB, preserving negative values. Logical right shift (`shr`) inserts zero bits, turning negative numbers into large positive numbers. The C standard accommodates both by designating it implementation-defined.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Identifying the Non-Deterministic Category</strong></summary>

**Goal**: Classify each of the following scenarios as Undefined Behavior, Implementation-Defined Behavior, or Unspecified Behavior:
1. `int a = 10 / 0;`
2. Checking `sizeof(long)`
3. Calling `printf("%d %d\n", get_a(), get_b());`
4. Accessing `arr[10]` on an array of 5 elements

**Solution**:
1. Undefined Behavior (Division by zero).
2. Implementation-Defined Behavior (Width of primitive types).
3. Unspecified Behavior (Order of argument evaluation).
4. Undefined Behavior (Out-of-bounds array access).
</details>

<details>
<summary><strong>Exercise 2 (Medium): Fixing a Defective Precondition Check</strong></summary>

**Goal**: Explain why this function's safety check is broken under `-O2`, and fix it:
```c
int get_length(const char *str) {
    int len = 0;
    while (*str++) len++;
    if (len + 1 < len) return -1; /* Check for overflow */
    return len;
}
```

**Solution**:
Signed integer overflow is undefined behavior. The optimizer assumes `len + 1 > len` is always true, deleting the `if (len + 1 < len)` check entirely!
Fix by validating bounds before incrementing:
```c
#include <limits.h>

int get_length(const char *str) {
    int len = 0;
    while (*str++) {
        if (len == INT_MAX) return -1; /* Safe precondition check */
        len++;
    }
    return len;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Eliminating Unspecified Subexpression Ordering</strong></summary>

**Goal**: Refactor this code so that its behavior is 100% deterministic and portable across all conforming ISO C compilers:

```c
int counter = 0;
int next(void) { return ++counter; }

int main(void) {
    int result = next() * 10 + next();
}
```

**Solution**:
The order in which the two `next()` function calls are executed is unspecified. Depending on compiler choice, `result` could evaluate to $(1 \times 10) + 2 = 12$ or $(2 \times 10) + 1 = 21$.
Conforming deterministic refactoring:
```c
int first = next();
int second = next();
int result = first * 10 + second; /* Guaranteed: 1 * 10 + 2 = 12 */
```
</details>

---

## 12. Summary / Cheat Sheet

- **Undefined Behavior**: No rules; compiler assumes it never happens; can erase security checks!
- **Implementation-Defined**: Compiler chooses and **must document** choice (e.g. `sizeof(long)`).
- **Unspecified**: Compiler chooses between options; **no documentation** required (e.g. arg order).
- **Always build with**: `-fsanitize=undefined,address` during testing.
- Check pointer for `NULL` **before** dereferencing.
- Check signed integers for overflow **before** adding.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 3.4: "Behavior definitions".
- Chris Lattner: *What Every C Programmer Should Know About Undefined Behavior* (LLVM Blog Series).
- SEI CERT C Coding Standard: Complete Guide to Undefined Behaviors.

---

[← Previous: 09-input-output.md](./09-input-output.md) | [Section Index](./README.md) | [Next: 11-sizeof-and-alignment-basics.md](./11-sizeof-and-alignment-basics.md) →
