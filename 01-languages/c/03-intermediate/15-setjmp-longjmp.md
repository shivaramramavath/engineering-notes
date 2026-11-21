# 15 Non-Local Jumps: setjmp & longjmp

> Master non-local control flow across stack frames using `<setjmp.h>`, simulate structured exception handling, and navigate register preservation and volatile semantics.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [01-fundamentals/07-control-flow.md](../01-fundamentals/07-control-flow.md), [01-fundamentals/08-functions.md](../01-fundamentals/08-functions.md), [02-core-concepts/20-memory-layout-stack-heap-data-bss.md](../02-core-concepts/20-memory-layout-stack-heap-data-bss.md) |
| **Time to Read** | 25 minutes |
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

In C, the standard `goto` statement is strictly local: it can only transfer execution to another label located within the **same function**. It cannot jump across function boundaries.

In complex software systems (such as compilers, interpreters, recursive-descent parsers, and deeply nested algorithmic searches), an unrecoverable error may occur deep down a call stack of 20 or 30 nested function invocations. Propagating error return codes through every intermediate function requires writing repetitive checks at every call site:
```c
if (parse_expr() != SUCCESS) return ERROR;
```
If an unrecoverable syntax error or out-of-memory condition occurs, unwinding manually is tedious and error-prone.

Standard C provides `<setjmp.h>` with `setjmp()` and `longjmp()` to perform **non-local jumps**. A non-local jump bypasses all intermediate stack frames in a single operation, transferring control directly back to a previously saved execution context.

---

## 2. Core Concepts

### 2.1 The `<setjmp.h>` Primitives
- `jmp_buf`: An implementation-defined array type that stores the CPU register state, stack pointer, and instruction counter.
- `int setjmp(jmp_buf env)`: Macro/function that saves the current execution context into `env`.
  - **Initial Call**: Returns `0`.
  - **Return via `longjmp`**: Returns the non-zero integer value passed to `longjmp()`.
- `void longjmp(jmp_buf env, int val)`: Restores the execution context saved in `env`. Execution resumes at the point of the `setjmp()` call as if `setjmp` had just returned `val`. If `val` is passed as `0`, the C standard mandates that `setjmp` receives `1`.

### 2.2 The `volatile` Variable Mandate
A critical rule in ISO C (§7.13.2.1):
> All local automatic variables that are modified between the invocation of `setjmp()` and the execution of `longjmp()` acquire **indeterminate values** upon return from `longjmp` unless they are qualified with `volatile`.

When the compiler optimizes code (`-O2`), local variables are cached in CPU registers. When `longjmp` restores the register context from `jmp_buf`, any modifications made to those registers after the `setjmp` call are wiped out, reverting to their old values or becoming corrupted stack garbage. Marking the variable `volatile` forces the compiler to store and reload it from physical RAM, preserving its updated state across the jump.

### 2.3 Danger of Dangling Stack Frames
You can **only jump backward up the call stack** to an ancestor function that is still currently active:
```
Call Stack:
main()         [Contains setjmp(env)]  <─── VALID TARGET
  └─ parse()   [Calls longjmp(env, 1)]
```
If a function containing `setjmp(env)` returns, its stack frame is popped. Invoking `longjmp(env, 1)` afterward jumps into an expired, invalidated stack frame, causing catastrophic memory corruption and segmentation faults.

---

## 3. How It Works Internally

### CPU Register Snapshot and Restoration
When `setjmp(env)` executes, the CPU saves its core architectural registers into the `jmp_buf` memory structure:
- **x86_64 Register Context**: Instruction Pointer (`RIP`), Stack Pointer (`RSP`), Base Pointer (`RBP`), and callee-saved registers (`RBX`, `R12`, `R13`, `R14`, `R15`).

```
Non-Local Jump Stack Unwinding Mechanics:
┌──────────────────────────────────────────────┐
│ main(): setjmp(env) saves RSP_main, RIP_main │ [Active Frame]
├──────────────────────────────────────────────┤
│ func_A(): calls func_B()                     │ [Active Frame]
├──────────────────────────────────────────────┤
│ func_B(): calls func_C()                     │ [Active Frame]
├──────────────────────────────────────────────┤
│ func_C(): executes longjmp(env, 2)           │ [Deepest Frame]
└──────────────────────┬───────────────────────┘
                       │ longjmp restores RSP_main and RIP_main
                       ▼
┌──────────────────────────────────────────────┐
│ main(): Resumes as if setjmp returned 2      │ [Intermediate frames discarded!]
└──────────────────────────────────────────────┘
```

Notice that intermediate stack frames (`func_A`, `func_B`, `func_C`) are **not orderly unwound**: no destructors run, no local variables are cleaned up, and no heap memory allocated in those frames is freed!

---

## 4. Syntax & Usage

### 4.1 Simulating `try` / `catch` Blocks
```c
#include <setjmp.h>
#include <stdio.h>

static jmp_buf s_jump_buffer;

#define TRY if (setjmp(s_jump_buffer) == 0)
#define CATCH(code) else if (code)
#define THROW(val) longjmp(s_jump_buffer, (val))

void risky_operation(int x) {
    if (x == 0) THROW(400); /* Throw error code 400 */
}
```

### 4.2 Portable Signal Jumps (`sigsetjmp` / `siglongjmp`)
In POSIX systems, `setjmp` does not portably save or restore the signal mask. If code jumps out of a signal handler using standard `longjmp`, the signal may remain permanently blocked. Always use POSIX `sigsetjmp(env, savesigs)` and `siglongjmp(env, val)`.

---

## 5. Practical Examples

### Comprehensive Example: Structured Exception Emulation with Volatile Verification
The following program demonstrates simulating structured exception handling with `setjmp` and `longjmp`, highlighting volatile counter preservation.

```c
#include <stdio.h>
#include <setjmp.h>
#include <stdbool.h>

static jmp_buf s_exception_env;

typedef enum {
    ERR_NONE = 0,
    ERR_INVALID_INPUT = 1,
    ERR_DIVIDE_BY_ZERO = 2
} ErrorCode;

static void compute_ratio(int numerator, int denominator) {
    printf("  [compute_ratio] Computing %d / %d...\n", numerator, denominator);
    if (denominator == 0) {
        printf("  [compute_ratio] Denominator is zero! Throwing ERR_DIVIDE_BY_ZERO via longjmp.\n");
        longjmp(s_exception_env, ERR_DIVIDE_BY_ZERO);
    }
    if (numerator < 0) {
        printf("  [compute_ratio] Negative numerator! Throwing ERR_INVALID_INPUT via longjmp.\n");
        longjmp(s_exception_env, ERR_INVALID_INPUT);
    }
    printf("  [compute_ratio] Ratio = %d\n", numerator / denominator);
}

int main(void) {
    printf("=== Non-Local Jumps with setjmp and longjmp ===\n");

    /* volatile qualifier is MANDATORY for local variables modified between setjmp and longjmp */
    volatile int attempts = 0;

    int test_cases[][2] = {
        {10, 2},
        {8, 0},
        {-5, 5}
    };

    for (int i = 0; i < 3; i++) {
        attempts++;
        printf("\nIteration %d:\n", attempts);

        int code = setjmp(s_exception_env);
        if (code == ERR_NONE) {
            /* Normal execution block (TRY) */
            compute_ratio(test_cases[i][0], test_cases[i][1]);
            printf("  Computation succeeded.\n");
        } else {
            /* Exception catch block (CATCH) */
            printf("  CAUGHT EXCEPTION: Error code %d\n", code);
            if (code == ERR_DIVIDE_BY_ZERO) {
                printf("  Recovery action: Division by zero avoided.\n");
            } else if (code == ERR_INVALID_INPUT) {
                printf("  Recovery action: Negative argument rejected.\n");
            }
        }
    }

    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 15-setjmp-longjmp.c -o 15-setjmp-longjmp.exe
./15-setjmp-longjmp.exe
```

```text
=== Non-Local Jumps with setjmp and longjmp ===

Iteration 1:
  [compute_ratio] Computing 10 / 2...
  [compute_ratio] Ratio = 5
  Computation succeeded.

Iteration 2:
  [compute_ratio] Computing 8 / 0...
  [compute_ratio] Denominator is zero! Throwing ERR_DIVIDE_BY_ZERO via longjmp.
  CAUGHT EXCEPTION: Error code 2
  Recovery action: Division by zero avoided.

Iteration 3:
  [compute_ratio] Computing -5 / 5...
  [compute_ratio] Negative numerator! Throwing ERR_INVALID_INPUT via longjmp.
  CAUGHT EXCEPTION: Error code 1
  Recovery action: Negative argument rejected.
```

---

## 6. Best Practices

1. **Mark Modified Automatic Variables `volatile`**: Any local variable in the function calling `setjmp` that changes value after `setjmp` must be qualified as `volatile`.
2. **Never Return from the Function Containing `setjmp` Before Calling `longjmp`**: Ensure the lifetime of the stack frame where `setjmp` was called exceeds all possible invocations of `longjmp`.
3. **Guard Against Resource Leaks**: Intermediate heap buffers, open file handles, and mutex locks held in skipped stack frames will never be freed. Track allocated resources in a dedicated global/context cleanup list before jumping.
4. **Pass Meaningful Non-Zero Error Codes**: Use discrete enum constants for the second argument of `longjmp()`. Never rely on passing 0 (since C converts 0 to 1).
5. **Restrict Use to Catastrophic Recovery**: Do not use `setjmp`/`longjmp` for routine control flow. Restrict its usage to top-level command loops, parser fatal error recovery, or thread coroutines.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| Jumping into returned function | Frame no longer exists on stack; restores garbage stack pointer, causing segmentation fault. | Ensure `setjmp` resides in top-level persistent caller. |
| Forgetting `volatile` on locals | Compiler registers cached before `longjmp` overwrite variable changes made between `setjmp` and `longjmp`. | Qualify variable with `volatile`. |
| Leaking memory during `longjmp` | Skipped intermediate stack frames bypass all `free()` and `fclose()` calls. | Maintain resource cleanup stacks or rollback logs. |
| Calling `longjmp(env, 0)` expecting `0` | ISO C mandates that if `0` is passed, `setjmp` receives `1`. | Pass explicit non-zero error constants (`ERR_FATAL`). |
| Signal handler jumping with `longjmp` | Fails to unmask the active signal on POSIX systems, permanently blocking future signals. | Use POSIX `sigsetjmp` and `siglongjmp`. |
| Using `setjmp` in complex expressions | ISO C strictly restricts where `setjmp` can appear (only in `if`, `switch`, or loop conditions). | Keep `setjmp` as a standalone condition: `int rc = setjmp(env);`. |

---

## 8. Performance & Security Notes

- **Execution Speed**: `setjmp()` and `longjmp()` are extraordinarily fast—typically taking only 5 to 10 CPU cycles because they execute simple register block copy instructions (`mov` / `movaps`). They are significantly faster than C++ zero-cost exception handling unwinding tables.
- **Security Vulnerabilities (Buffer Overflows)**: If an attacker can overwrite the contents of a `jmp_buf` via a buffer overflow, they control the restored Instruction Pointer (`RIP`) and Stack Pointer (`RSP`), enabling complete process takeover.
- **Pointer Mangling / Pointer Guard**: Modern C runtimes (glibc, Windows CRT) implement pointer encoding (`PTR_MANGLE`) to XOR encrypt stack pointers stored inside `jmp_buf` with a random per-process secret key.

---

## 9. Comparison / When to Use What

| Mechanism | Scope | Resource Safety | Portability | Overhead |
| :--- | :--- | :--- | :--- | :--- |
| **Status Codes (`goto cleanup`)** | Local to function | High (Manual orderly cleanup) | Universal ISO C | Zero runtime cost |
| **`setjmp` / `longjmp`** | Cross-function (Non-local) | Low (Bypasses intermediate frees) | Universal ISO C | Extremely low (~10 cycles) |
| **POSIX `siglongjmp`** | Cross-signal / Non-local | Low | POSIX systems only | Low |
| **C++ Exceptions (`throw`)** | Cross-function | High (RAII destructors run) | C++ only (Not C) | High table unwinding cost |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does ISO C mandate that local variables modified between `setjmp` and `longjmp` must be volatile?</strong></summary>

Under compiler optimization passes, local variables are assigned to physical CPU registers rather than stack memory slots. When `setjmp()` is called, it snapshots the current register states. If the code continues and modifies one of those variables, the register value updates. When `longjmp()` subsequently restores the saved register context from `jmp_buf`, it clobbers the current registers with the old values from the moment of `setjmp`. Marking the variable `volatile` forces the compiler to store the variable in physical RAM, ensuring the latest value is preserved across the jump.
</details>

<details>
<summary><strong>2. What happens if `longjmp(env, 0)` is called with zero as the second argument?</strong></summary>

The ISO C standard explicitly states that if the second argument to `longjmp` is passed as `0`, the function will cause `setjmp` to return `1` instead. This prevents an infinite loop: `setjmp` returns `0` upon initial setup; if `longjmp` could return `0`, the caller would be unable to distinguish an initial setup call from a non-local jump return.
</details>

<details>
<summary><strong>3. What are the severe risks of using `longjmp()` in code that dynamically allocates memory?</strong></summary>

`longjmp()` performs an abrupt stack unwind by resetting the CPU's stack pointer (`RSP`) directly to the ancestor frame. Unlike languages with RAII or garbage collection, C intermediate stack frames cannot execute cleanup logic. Any dynamically allocated heap blocks (`malloc`), acquired mutex locks, or open file streams (`fopen`) created inside the skipped intermediate functions will never be released, causing memory leaks, resource starvation, and deadlocks.
</details>

<details>
<summary><strong>4. Why is `setjmp` subject to strict syntactic placement rules in ISO C?</strong></summary>

ISO C (§7.13.1.1) mandates that `setjmp` may only appear as:
1. The entire controlling expression of a selection or iteration statement (`if (setjmp(env))` or `switch (setjmp(env))`).
2. One operand of a relational or equality operator with an integer constant (`if (setjmp(env) == 0)`).
3. The operand of a unary `!` operator (`if (!setjmp(env))`).
4. A standalone expression statement (`(void)setjmp(env);`).
This restriction is enforced because restoring register contexts mid-expression in complex statements (like `x = a + setjmp(env) * b`) would require the compiler to preserve complex partial expression evaluation states, which is unfeasible across heterogeneous CPU architectures.
</details>

<details>
<summary><strong>5. What is the difference between `setjmp` and POSIX `sigsetjmp`?</strong></summary>

When an operating system delivers a signal to a process, it masks that signal so that another instance of the same signal is blocked during the handler's execution. Standard ISO C `setjmp` and `longjmp` do not specify whether the signal mask is preserved. If `longjmp` is used to exit a signal handler, the signal may remain permanently blocked on POSIX systems. `sigsetjmp(env, savesigs)` explicitly takes a flag indicating whether to save and restore the process signal mask via `siglongjmp`.
</details>

<details>
<summary><strong>6. What happens if you call `longjmp()` on a `jmp_buf` whose enclosing function has already returned?</strong></summary>

This triggers severe Undefined Behavior. Once the function that called `setjmp()` returns, its stack frame is marked as invalid and available for reuse by future function calls. Calling `longjmp()` restores the Stack Pointer (`RSP`) to an already overwritten or corrupted stack area, causing stack smashing, corrupted return addresses, and immediate segmentation faults.
</details>

<details>
<summary><strong>7. How is `jmp_buf` protected against security exploitation in modern operating systems?</strong></summary>

Because `jmp_buf` holds the Instruction Pointer (`RIP`) and Stack Pointer (`RSP`), attackers targeting buffer overflows frequently overwrite `jmp_buf` to hijack execution. Modern C libraries (such as glibc) use "pointer mangling": when `setjmp` writes pointers to `jmp_buf`, it encrypts them by XORing them with a random 64-bit secret key stored in Thread-Local Storage (TLS) and bit-rotating them. `longjmp` decrypts the pointers before loading them into CPU registers, preventing exploitation.
</details>

<details>
<summary><strong>8. Can `setjmp` and `longjmp` be used to implement cooperative multitasking (coroutines)?</strong></summary>

Yes. By declaring separate `jmp_buf` structures for different tasks and manipulating the Stack Pointer (`RSP`) to point to separate dynamically allocated stack memory blocks (historically using `sigaltstack` or inline assembly), an application can switch between cooperative green threads or coroutines using `setjmp` and `longjmp` without kernel context switches.
</details>

---

## 11. Practice Exercises

### Easy: Safe Division with Error Jump
Write a function `int divide(int a, int b, jmp_buf env)` that performs integer division, but calls `longjmp(env, 1)` if `b == 0`.

<details>
<summary>Solution</summary>

```c
#include <setjmp.h>
#include <stdio.h>

int divide(int a, int b, jmp_buf env) {
    if (b == 0) {
        longjmp(env, 1);
    }
    return a / b;
}
```
</details>

### Medium: Volatile Loop Verification
Demonstrate how a non-volatile counter fails or becomes indeterminate compared to a `volatile` counter when incremented across a `longjmp` jump loop.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <setjmp.h>

static jmp_buf env;

void test_volatile(void) {
    volatile int v_count = 0;
    int normal_count = 0;

    if (setjmp(env) < 3) {
        v_count++;
        normal_count++;
        printf("v_count=%d, normal_count=%d\n", v_count, normal_count);
        longjmp(env, v_count);
    }
    /* v_count is guaranteed to reach 3; normal_count is indeterminate under -O2 */
}
```
</details>

### Hard: Nested Exception Handling System
Implement a stack of `jmp_buf` structures supporting nested `TRY` / `CATCH` blocks where an inner unhandled exception can be re-thrown to the outer handler.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <setjmp.h>
#include <stdbool.h>

#define MAX_EXCEPTION_DEPTH 8

static jmp_buf s_exception_stack[MAX_EXCEPTION_DEPTH];
static int s_stack_top = -1;

bool push_exception_frame(void) {
    if (s_stack_top + 1 >= MAX_EXCEPTION_DEPTH) return false;
    s_stack_top++;
    return true;
}

void pop_exception_frame(void) {
    if (s_stack_top >= 0) s_stack_top--;
}

void throw_exception(int error_code) {
    if (s_stack_top < 0) {
        fprintf(stderr, "Unhandled exception: %d\n", error_code);
        return;
    }
    int target = s_stack_top;
    pop_exception_frame();
    longjmp(s_exception_stack[target], error_code);
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Function / Type | Header | Purpose |
| :--- | :--- | :--- |
| `jmp_buf` | `<setjmp.h>` | Holds CPU registers and stack context |
| `setjmp(env)` | `<setjmp.h>` | Returns `0` on setup, non-zero on longjmp return |
| `longjmp(env, val)` | `<setjmp.h>` | Jumps directly to `setjmp`, making it return `val` |
| `volatile` | Keyword | Mandatory for modified locals in `setjmp` frame |
| `sigsetjmp` / `siglongjmp` | `<setjmp.h>` | POSIX variants preserving signal masks |

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17) §7.13 (Nonlocal jumps `<setjmp.h>`).
- SEI CERT C Coding Standard: ERR52-C (Do not use `setjmp()` and `longjmp()`).
- SEI CERT C Coding Standard: MSC22-C (Use the `sig_atomic_t`, `volatile`, and `atomic` types appropriately).

---

[← Previous](14-callbacks-and-function-pointer-tables.md) | [Section Index](README.md) | [Next →](16-inline-restrict-volatile.md)
