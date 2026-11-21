# Control Flow and Jump Mechanics

> A deep technical examination of C control flow: structured branching (`if`/`else`), loop mechanics (`for`, `while`, `do-while`), jump tables in `switch` statements, C23 `[[fallthrough]]`, the Linux kernel `goto cleanup` idiom, and Duff's Device.

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [05-operators.md](./05-operators.md), [06-type-conversions.md](./06-type-conversions.md) |
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

In C, control flow statements translate almost directly to conditional and unconditional hardware branch instructions (`je`, `jne`, `jmp`, `call`). Writing robust control flow requires understanding how microprocessors execute decisions:
1. **CPU Branch Prediction Penalties**: Modern superscalar processors use branch target buffers (BTB) to speculatively execute instructions. A mispredicted branch flushes the instruction pipeline, causing a 15–20 cycle latency penalty.
2. **Resource Leakage on Multi-Point Returns**: Returning early from nested `if` statements without cleaning up allocated file descriptors, mutex locks, or heap buffers causes severe memory and descriptor leaks. The `goto cleanup` idiom solves this deterministically.
3. **Accidental Switch Fallthrough**: Omitting a `break` statement in a `switch` causes execution to fall through into subsequent case blocks, a frequent source of critical business logic and security defects.

---

## 2. Core Concepts

### Branching Structures

- **`if (condition)`**: Condition evaluates to scalar true (non-zero) or false (`0`).
- **`switch (expr)`**: Multi-way branch where `expr` must evaluate to an **integer type**. Floating-point values or strings are invalid.

### Looping Constructs

1. **`while (cond)`**: Condition checked at the **top**; body may execute zero times.
2. **`do { ... } while (cond);`**: Condition checked at the **bottom**; body is guaranteed to execute at least once.
3. **`for (init; cond; update)`**: Standard idiom; since C99, `init` can declare variables scoped strictly to the loop.

### Jump Statements

- `break`: Terminates the innermost enclosing loop (`for`, `while`, `do-while`) or `switch`.
- `continue`: Skips remainder of current loop iteration, jumping directly to the loop condition (or `update` clause in a `for` loop).
- `goto label`: Unconditionally jumps to a local label within the **same function**.

---

## 3. How It Works Internally

### Switch Statement Compilation: Jump Tables vs. Binary Trees

When compiling a `switch` statement, modern compilers inspect the distribution of case values:

```
Case Distribution Type:
1. Dense Values (e.g. 1, 2, 3, 4, 5):
   Compiler emits a JUMP TABLE in .rodata:
   jmp [table + eax*8]  --> Constant O(1) time execution!

2. Sparse Values (e.g. 10, 5000, 999999):
   Compiler emits a balanced Binary Search Tree of cmp/je branches:
   O(log N) time execution.

3. Very Few Cases (e.g. 1, 2):
   Compiler emits linear cmp/je sequence.
```

```
Dense Switch Internal Structure:
+-------------------+
| Bounds Check:     |  cmp eax, 4
| (eax > MAX)       |  ja .default_case
+-------------------+
          |
          v
+-------------------+
| Indirect Branch:  |  jmp QWORD PTR [rip + .Ljump_table + rax*8]
+-------------------+
          |
   +------+------+------+------+
   |             |             |
   v             v             v
.case_1       .case_2       .case_3
```

### Duff's Device: Intertwining `switch` and `do-while`

Invented by Tom Duff in 1983 at Lucasfilm, Duff's Device performs manual loop unrolling by interleaving a `switch` statement with a `do-while` loop:

```c
void duff_copy(char *to, const char *from, size_t count) {
    size_t n = (count + 7) / 8;
    switch (count % 8) {
        case 0: do { *to++ = *from++;
        case 7:      *to++ = *from++;
        case 6:      *to++ = *from++;
        case 5:      *to++ = *from++;
        case 4:      *to++ = *from++;
        case 3:      *to++ = *from++;
        case 2:      *to++ = *from++;
        case 1:      *to++ = *from++;
                } while (--n > 0);
    }
}
```
*Why this is legal C*: In C, case labels inside a `switch` can appear on any statement inside the switch body, even if nested inside inner compound blocks like a `do-while` loop!

---

## 4. Syntax & Usage

### 1. The Canonical `goto cleanup` Pattern
The Linux kernel and production systems software use `goto cleanup` for centralized resource teardown:

```c
#include <stdlib.h>
#include <stdio.h>

int process_transaction(const char *filename) {
    FILE *fp = NULL;
    char *buffer = NULL;
    int status = -1;

    fp = fopen(filename, "rb");
    if (!fp) goto cleanup;

    buffer = malloc(4096);
    if (!buffer) goto cleanup;

    /* Perform work */
    status = 0; /* Success */

cleanup:
    /* Single exit point: resources freed in reverse order of allocation */
    if (buffer) free(buffer);
    if (fp) fclose(fp);
    return status;
}
```

### 2. Standard C23 `[[fallthrough]]` Attribute
In C23, intentional switch fallthrough is formally marked with standard attribute syntax, eliminating compiler warnings (`-Wimplicit-fallthrough`):

```c
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
switch (state) {
    case 1:
        prepare_data();
        [[fallthrough]];
    case 2:
        send_data();
        break;
}
#endif
```

---

## 5. Practical Examples

### Control Flow and Switch Demonstration Program

```c
/* File: 01-languages/c/01-fundamentals/examples/07-control-flow.c */
#include <stdio.h>

static void demonstrate_switch(int command) {
    switch (command) {
        case 1:
            printf("Action: START\n");
            break;
        case 2:
            printf("Action: PAUSE\n");
            break;
        case 3:
            printf("Action: RESUME (falling through to STOP)\n");
            /* Intentional fallthrough */
            /* In C23: [[fallthrough]]; */
        case 4:
            printf("Action: STOP\n");
            break;
        default:
            printf("Action: UNKNOWN\n");
            break;
    }
}

int main(void) {
    printf("=== Control Flow & Switch Demonstration ===\n");
    demonstrate_switch(1);
    demonstrate_switch(3);

    /* Goto cleanup idiom demonstration */
    int error_code = 0;
    int *buffer = NULL;
    
    if (error_code != 0) {
        goto cleanup;
    }
    printf("Processing completed successfully without error.\n");

cleanup:
    /* Centralized resource teardown */
    (void)buffer;
    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 07-control-flow.c -o run_07.exe
./run_07.exe
```
Output:
```text
=== Control Flow & Switch Demonstration ===
Action: START
Action: RESUME (falling through to STOP)
Action: STOP
Processing completed successfully without error.
```

---

## 6. Best Practices

1. **Always Include a `default:` Label in `switch` Statements**: Even if you believe all possible enumeration values are handled, a `default` case traps corrupt state or newly added enum values.
2. **Explicitly Annotate Intentional Fallthrough**: Use C23 `[[fallthrough]];` or comments like `/* FALLTHROUGH */` to inform static analyzers and human reviewers.
3. **Use `goto` Exclusively for Downward Error Cleanup**: Never jump backward with `goto` (create loops with `while` or `for`), and never jump over variable initializations.
4. **Prefer Scoped Loop Variables (`for (int i = 0; ...)` )**: Restricting iteration variables to loop scope prevents accidental variable reuse across successive loops.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Accidental `switch` fallthrough due to missing `break` | Execution cascades into subsequent case blocks, running unintended code. | Add `break;` or C23 `[[fallthrough]];`. |
| Declaring variables after a `case` label without a block | In C, a label cannot be followed immediately by a variable declaration without an enclosing `{}` block. | Wrap the case in braces: `case 1: { int x = 5; ... break; }`. |
| Semicolon immediately after `if` or `for` | `if (x == 5);` creates an empty statement; the subsequent block executes unconditionally! | Ensure braces `{}` follow conditionals immediately. |
| Jumping over variable initializations with `goto` | If a `goto` skips a variable initialization in the same block, reading that variable yields garbage. | Place initializations at the top of the block before any `goto`. |
| Using floating-point values in `switch (f)` | The C standard only permits integer expressions in `switch`. | Use an `if ... else if` sequence for floating-point comparisons. |
| Infinite unsigned loops `for (unsigned i = 5; i >= 0; --i)` | Unsigned integers wrap around to large positive numbers, never becoming $< 0$. | Use a signed loop counter or `while (i-- > 0)`. |

---

## 8. Performance & Security Notes

- **Branch Misprediction & Compiler Hints**: Modern compilers support built-in branch hints `__builtin_expect(!!(cond), 1)` (often wrapped as `likely(cond)` and `unlikely(cond)` in the Linux kernel). This informs the compiler driver to align the fast path linearly in instruction cache, moving the error-handling branch out of line.
- **Resource Leaks on Early Returns**: If a function allocates three buffers and returns early on the fourth failure, the first three remain leaked in memory. The single-exit `goto cleanup` idiom completely eradicates this entire vulnerability class.

---

## 9. Comparison / When to Use What

| Structure | Best Use Case | Compilation Target |
|---|---|---|
| **`if-else`** | Arbitrary boolean logic, complex ranges, floating-point checks | Conditional branches (`je`, `jne`, `cmov`) |
| **`switch`** | Multiple discrete integer constant values (e.g. enums, commands) | $O(1)$ Jump Table or Binary Search Tree |
| **`for`** | Counted iterations, traversing fixed arrays, standard updates | Loop with counter register |
| **`while`** | Indeterminate loops driven by condition (e.g. reading from socket) | Conditional jump loop |
| **`do-while`** | When loop body **must** execute at least once (e.g. retrying I/O) | Jump at bottom of block |
| **`goto cleanup`** | Centralized multi-resource error teardown in systems C | Unconditional forward jump (`jmp`) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `goto` widely used and encouraged in the Linux kernel codebase?</strong></summary>

In systems programming, functions frequently allocate multiple resources (memory, spinlocks, file descriptors, IRQ handlers). When an allocation fails halfway through, all previously acquired resources must be freed in reverse order. Scattering cleanup across dozens of nested `if` statements leads to missed cleanups and code duplication. A centralized `goto out_free_res2;` pattern guarantees clean, readable, single-exit resource teardown.
</details>

<details>
<summary><strong>2. How does a C compiler optimize a dense `switch` statement compared to a chain of `if-else` statements?</strong></summary>

A chain of $N$ `if-else` statements requires $O(N)$ comparisons in the worst case. For dense case values, a compiler generates a **jump table**—an array of code pointers stored in `.rodata`. At runtime, the switch expression is bounds-checked and used as an index to jump directly to the target case address in $O(1)$ constant time.
</details>

<details>
<summary><strong>3. What is the C23 `[[fallthrough]]` attribute, and what problem does it solve?</strong></summary>

In C, execution falls through from one `case` to the next unless interrupted by `break`. While sometimes intentional, accidental missing `break` statements cause serious bugs. Modern compilers warn on implicit fallthrough (`-Wimplicit-fallthrough`). C23 standardized `[[fallthrough]];` to explicitly inform the compiler that the fallthrough is intentional, suppressing the diagnostic warning.
</details>

<details>
<summary><strong>4. Why does `case 1: int x = 5; break;` fail to compile in standard C?</strong></summary>

In ISO C grammar, a label (such as `case 1:`) can only be attached to a **statement**, not a declaration. Furthermore, variable declarations inside a switch must not be bypassed by other case jumps. To declare variables inside a case, wrap the case body in a compound block: `case 1: { int x = 5; ... break; }`.
</details>

<details>
<summary><strong>5. What is Duff's Device, and why is it syntactically valid in C?</strong></summary>

Duff's Device is an optimization idiom for loop unrolling that merges a `switch` statement with a `do-while` loop. It is valid because the C standard permits `case` labels to appear on any sub-statement within the switch body, even if nested inside other control structures.
</details>

<details>
<summary><strong>6. What is the difference between `break` and `continue`?</strong></summary>

`break` terminates the innermost enclosing loop or `switch` entirely, resuming execution at the first statement following the loop/switch. `continue` skips the remaining statements in the current loop iteration and proceeds immediately to the next iteration (evaluating the loop condition in `while`/`do-while`, or the update expression in `for`).
</details>

<details>
<summary><strong>7. What happens if a `goto` statement jumps into a block that declares a Variable Length Array (VLA)?</strong></summary>

Under the ISO C standard, jumping into the scope of an identifier with variably modified type (such as a VLA) from outside its scope invokes **undefined behavior** because the stack allocation calculation is bypassed.
</details>

<details>
<summary><strong>8. Why should you avoid using `do { ... } while(0)` as a simple loop? What is its idiomatic purpose?</strong></summary>

`do { ... } while(0)` is not intended as an iterative loop; it is the standard C idiom for writing multi-statement preprocessor macros. It ensures that the macro expands into a single syntactic statement that behaves properly when followed by a semicolon inside `if-else` constructs without braces.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Refactoring Switch Variable Scoping</strong></summary>

**Goal**: Fix the compiler error in this switch statement:
```c
switch (type) {
    case 1:
        int length = calculate();
        process(length);
        break;
    default:
        break;
}
```

**Solution**:
Wrap the case in a compound statement block:
```c
switch (type) {
    case 1: {
        int length = calculate();
        process(length);
        break;
    }
    default:
        break;
}
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Implementing the `goto cleanup` Idiom</strong></summary>

**Goal**: Write a function `char *read_file_contents(const char *path)` that opens a file, queries its size, allocates a heap buffer, reads the file, and safely handles any error along the way using `goto cleanup` without leaking memory or open descriptors.

**Solution**:
```c
#include <stdio.h>
#include <stdlib.h>

char *read_file_contents(const char *path) {
    FILE *fp = NULL;
    char *buffer = NULL;

    fp = fopen(path, "rb");
    if (!fp) goto cleanup;

    if (fseek(fp, 0, SEEK_END) != 0) goto cleanup;
    long size = ftell(fp);
    if (size < 0) goto cleanup;
    rewind(fp);

    buffer = malloc((size_t)size + 1);
    if (!buffer) goto cleanup;

    if (fread(buffer, 1, (size_t)size, fp) != (size_t)size) {
        free(buffer);
        buffer = NULL;
        goto cleanup;
    }
    buffer[size] = '\0';

cleanup:
    if (fp) fclose(fp);
    return buffer; /* Returns buffer on success, or NULL on failure */
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Simulating a Duff's Device Unroller</strong></summary>

**Goal**: Implement a memory-clearing function `unrolled_zero(uint8_t *dest, size_t count)` using Duff's Device to unroll 8 iterations per step.

**Solution**:
```c
#include <stdint.h>
#include <stddef.h>

void unrolled_zero(uint8_t *dest, size_t count) {
    if (count == 0) return;
    size_t n = (count + 7) / 8;
    switch (count % 8) {
        case 0: do { *dest++ = 0;
        case 7:      *dest++ = 0;
        case 6:      *dest++ = 0;
        case 5:      *dest++ = 0;
        case 4:      *dest++ = 0;
        case 3:      *dest++ = 0;
        case 2:      *dest++ = 0;
        case 1:      *dest++ = 0;
                } while (--n > 0);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **`switch`**: Requires integer expressions; dense cases compile to $O(1)$ jump tables.
- **Fallthrough**: Intentional fallthrough must be marked with `[[fallthrough]];` (C23).
- **`goto cleanup`**: Standard systems C idiom for single-exit resource teardown.
- **Loop Scoping**: Use `for (int i = 0; ...)` to restrict counter scope (C99+).
- **Braces**: Always wrap case bodies declaring local variables in braces `{}`.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 6.8: "Statements and blocks".
- Tom Duff: *Duff's Device Explanation* (Historical Netnews message, 1983).
- Linux Kernel Coding Style, Chapter 7: "Centralized exiting of functions".

---

[← Previous: 06-type-conversions.md](./06-type-conversions.md) | [Section Index](./README.md) | [Next: 08-functions.md](./08-functions.md) →
