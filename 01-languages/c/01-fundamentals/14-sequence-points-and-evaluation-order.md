# Sequence Points and Evaluation Order

> A formal deep dive into execution order in C: sequence points, ISO C11/C17/C23 "sequenced-before" partial-order relations, unsequenced modifications, indeterminately sequenced operations, and eliminating evaluation-order undefined behavior.

| Metadata | Details |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [05-operators.md](./05-operators.md), [10-undefined-unspecified-implementation-defined.md](./10-undefined-unspecified-implementation-defined.md) |
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

One of the most persistent misconceptions among new C developers is the belief that source code expressions evaluate strictly from left to right. In standard C, the order in which operators and sub-expressions are evaluated is largely **unspecified** to give optimizing compilers the freedom to reorder instructions for maximum CPU instruction pipeline throughput.

However, this optimization freedom introduces severe undefined behavior risks:
1. **Unsequenced Variable Modification**: Writing `a[i] = i++` or `x = x++` invokes **undefined behavior**. Compilers are not required to generate any sensible value; they can emit corrupt reads, format drives, or delete surrounding code.
2. **Side-Effect Race Conditions in Subexpressions**: In `printf("%d %d\n", get_val(), modify_val())`, the order in which functions execute is unspecified. Different compilers or optimization levels produce different outputs.
3. **Formal Evolution of the Standard**: While C89 and C99 relied on the colloquial term "sequence point," C11, C17, and C23 formalized execution order using mathematical **sequenced-before** relations aligned with C++ and modern memory model specifications.

---

## 2. Core Concepts

### Sequenced-Before Relationships in Modern C

Under ISO C11/C17/C23, execution ordering within a single thread is defined by three relationships:

1. **Sequenced-Before**: A strict, asymmetric, transitive relation between two evaluations. If evaluation $A$ is sequenced-before $B$, all value computations and side effects of $A$ are guaranteed to complete before those of $B$ begin.
2. **Indeterminately Sequenced**: $A$ is sequenced-before $B$ OR $B$ is sequenced-before $A$, but the standard does not specify which. However, they **do not overlap** (no parallel interleaving).
3. **Unsequenced**: Neither $A$ is sequenced-before $B$ nor $B$ is sequenced-before $A$. The compiler is free to interleave, reorder, or execute them concurrently on hardware registers.

```
+-------------------------------------------------------------------------+
|                  THE CORE UNDEFINED BEHAVIOR RULE                       |
+-------------------------------------------------------------------------+
| If a side effect on a scalar object is UNSEQUENCED relative to:         |
|   1. Another side effect on the SAME scalar object, OR                  |
|   2. A value computation using the value of the SAME scalar object      |
|                                                                         |
| ...the behavior is UNDEFINED.                                           |
+-------------------------------------------------------------------------+
```

```
Examples of Fatal Unsequenced Operations:
i = i++;           /* UB: Side effect of assignment is unsequenced with side effect of post-inc */
f(i++, i++);       /* UB: Two unsequenced side effects on i */
a[i] = i++;        /* UB: Read of i in a[i] is unsequenced with side effect of post-inc */
(x = 1) + (x = 2); /* UB: Two unsequenced writes to x */
```

---

## 3. How It Works Internally

### Where Sequence Points Exist in C

A **Sequence Point** marks a boundary where all previous side effects must be finalized before any subsequent evaluations begin:

1. **End of a Full Expression**: The semicolon `;` at the end of an expression statement, or controlling expressions of `if`, `switch`, `while`, `do-while`, and each clause of a `for` loop.
2. **Logical Operators**:
   - `&&` (Logical AND): Evaluation of left operand is sequenced-before right operand.
   - `||` (Logical OR): Evaluation of left operand is sequenced-before right operand.
3. **Conditional Operator (`?:`)**: Evaluation of condition is sequenced-before the selected branch.
4. **Comma Operator (`,`)**: Evaluation of left operand is sequenced-before right operand.
5. **Function Call Boundary**: Evaluation of function designator and argument expressions is sequenced-before execution of the function body.

```
       [ Expression A ]
              |
     { SEQUENCE POINT }  <-- All side effects of A finalized!
              |
       [ Expression B ]
```

---

### Comprehensive ISO Sequence Point Inventory

The ISO C specification defines sequence points at the following exact language boundaries:
1. Between the evaluation of the left and right operands of the `&&` operator.
2. Between the evaluation of the left and right operands of the `||` operator.
3. Between the evaluation of the first operand of the `?:` operator and either the second or third operand.
4. Between the evaluation of the left and right operands of the comma `,` operator.
5. Between the evaluations of the function designator and its arguments, and the actual call.
6. At the end of a full declarator containing an initializer.
7. At the end of a full expression:
   - An expression statement (ending in `;`).
   - The controlling expression of an `if` or `switch`.
   - The controlling expression of a `while` or `do-while`.
   - Each of the three expressions in a `for` loop header.
   - The expression in a `return` statement.

---

## 4. Syntax & Usage

### 1. Eliminating Unsequenced Increment Hazards
```c
/* UNCONFORMING (Undefined Behavior): */
int index = 0;
array[index] = index++; /* UB! */

/* CONFORMING & DETERMINISTIC: */
int index = 0;
array[index] = index;
index++;
```

### 2. Eliminating Argument Evaluation Ambiguities
```c
/* UNRELIABLE (Order of evaluation is unspecified): */
int total = calculate(fetch_counter(), fetch_counter());

/* CONFORMING (Order is deterministic): */
int first_val = fetch_counter();
int second_val = fetch_counter();
int total = calculate(first_val, second_val);
```

---

## 5. Practical Examples

### Sequence Points & Execution Order Demonstration Program

```c
/* File: 01-languages/c/01-fundamentals/examples/14-sequence-points-and-evaluation-order.c */
#include <stdio.h>

int main(void) {
    printf("=== Sequence Points & Sequenced Execution ===\n");
    
    /* Fully sequenced expression statements */
    int i = 0;
    i++;
    int next_val = i;
    printf("Sequenced statement evaluation: i = %d, next_val = %d\n", i, next_val);

    /* Logical short-circuit sequence point */
    int guard = 0;
    int evaluated = 0;
    int res = (guard != 0) && (++evaluated);
    printf("Short-circuit guarantee: res = %d, evaluated = %d\n", res, evaluated);

    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 14-sequence-points-and-evaluation-order.c -o run_14.exe
./run_14.exe
```
Output:
```text
=== Sequence Points & Sequenced Execution ===
Sequenced statement evaluation: i = 1, next_val = 1
Short-circuit guarantee: res = 0, evaluated = 0
```

---

## 6. Best Practices

1. **Keep Increment and Decrement Operators Isolated**: Never combine `++` or `--` with other operators in the same expression. Write `i++;` as a standalone statement.
2. **Never Pass Expressions with Side Effects as Function Arguments**: Avoid `f(i++, j++)`. Evaluate side effects before making the function call.
3. **Use the Comma Operator Exclusively in `for` Loop Headers**: Avoid using `,` in general expressions; it degrades readability and introduces subtle bugs.
4. **Compile with `-Wsequence-point`**: Both GCC and Clang provide `-Wsequence-point` (included in `-Wall`) to catch unsequenced operations at compile time.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Writing `i = i++` | Unsequenced write and post-increment on same scalar object invokes undefined behavior. | Write `i++;`. |
| Writing `a[i] = i++` | Read of `i` in `a[i]` is unsequenced relative to `i++`; undefined behavior. | Separate lines: `a[i] = i; i++;`. |
| Assuming function arguments evaluate left-to-right | Order of argument evaluation is **unspecified**. | Evaluate arguments into temporary variables on separate lines. |
| Expecting `+` or `*` to introduce a sequence point | `a() + b()` does NOT sequence `a()` before `b()`. | Call sequentially if ordering matters. |
| Macro arguments with `++` evaluated multiple times | Macro expansion copies `x++` into multiple branches, invoking unsequenced UB. | Pass pure variables to macros or use inline functions. |
| Using `,` instead of `;` to separate statements | Reduces code clarity and can alter precedence unexpectedly. | Use standard `;` statement terminators. |

---

## 8. Performance & Security Notes

- **Instruction Pipelining and Register Allocation**: Compilers leave operand evaluation unsequenced so that the scheduler can interleave independent load instructions, hiding memory latency and maximizing CPU instruction-level parallelism (ILP).
- **Subtle Security Flaws from Evaluation Reordering**: If code does `transfer(withdraw(&src, 100), deposit(&dst, 100))`, and `deposit` runs before `withdraw`, an interrupted process can grant free credit if `withdraw` fails subsequently.

---

### Compiler Optimization Barriers and Reordering Controls

Compilers are free to reorder machine instructions across sequence points if the transformation does not alter observable program behavior (the "as-if" rule). However, when interfacing with memory-mapped hardware or multi-core memory, developers must enforce strict hardware instruction ordering using:
- **Compiler Barriers**: `__asm__ volatile("" ::: "memory")` prevents the compiler from reordering reads or writes across the barrier.
- **Hardware Memory Fences**: Functions like `atomic_thread_fence(memory_order_seq_cst)` in C11 `<stdatomic.h>` emit CPU bus synchronization instructions (such as `mfence` on x86 or `dmb` on ARM) ensuring hardware memory controllers complete all prior writes before subsequent instructions execute.

---

## 9. Comparison / When to Use What

| Construct | Introduces Sequence Point? | Evaluates Sequenced Left-to-Right? | Short-Circuits? |
|---|---|---|---|
| Semicolon `;` | **Yes** | Yes | No |
| Logical AND `&&` | **Yes** | **Yes** | **Yes** |
| Logical OR `\|\|` | **Yes** | **Yes** | **Yes** |
| Comma Operator `,` | **Yes** | **Yes** | No |
| Function Call `f()` | **Yes** (at entry) | Arguments **Unsequenced** | No |
| Addition `+` / Mult `*` | **No** | Operands **Unsequenced** | No |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `x = x++` undefined behavior in C?</strong></summary>

Under ISO C, modifying a scalar object more than once, or reading it and modifying it, without an intervening sequence point invokes undefined behavior. The assignment to `x` and the post-increment of `x` are unsequenced relative to each other, so the compiler is permitted to produce any result or miscompile.
</details>

<details>
<summary><strong>2. In what order are the subexpressions evaluated in `f(a(), b()) + c()`?</strong></summary>

The evaluation order among `a()`, `b()`, and `c()` is **unspecified**. The compiler can evaluate `c()` first, then `b()`, then `a()`. The only guarantee is that `a()` and `b()` will finish completely before `f()` begins execution.
</details>

<details>
<summary><strong>3. How does the ISO C11 "sequenced-before" model improve upon the C99 "sequence point" definition?</strong></summary>

C99 defined sequence points informally as moments in time, which proved inadequate for modern multi-threaded architectures, multi-core memory models, and formal compiler verification. C11 replaced this with mathematical partial orders: "sequenced-before", "indeterminately sequenced", and "unsequenced", harmonizing C with the C++11 memory model.
</details>

<details>
<summary><strong>4. Does the comma separating function arguments act as a comma operator?</strong></summary>

No! The comma separating arguments in a function call (e.g. `f(x, y)`) is a **syntactic separator**, NOT the comma operator. It does **not** introduce a sequence point, and the arguments are unsequenced relative to each other.
</details>

<details>
<summary><strong>5. What does "indeterminately sequenced" mean? Give an example.</strong></summary>

Two evaluations are indeterminately sequenced if one is guaranteed to execute before the other without interleaving, but the standard does not specify which one goes first. For example, in nested function calls like `f(g(), h())`, `g()` and `h()` are indeterminately sequenced: one runs completely before the other, but which runs first is unspecified.
</details>

<details>
<summary><strong>6. Why does `while (*dest++ = *src++)` avoid unsequenced undefined behavior?</strong></summary>

Because the modifications apply to **different objects**: `dest` and `src` are separate pointers. Modifying two *distinct* objects without a sequence point is completely well-defined. Undefined behavior occurs only when modifying the *same* scalar object multiple times unsequenced.
</details>

<details>
<summary><strong>7. Does a cast operator introduce a sequence point?</strong></summary>

No. A cast operator `(type)expr` performs a value conversion, but does not introduce a sequence point.
</details>

<details>
<summary><strong>8. What compiler warning flag catches unsequenced evaluation errors?</strong></summary>

`-Wsequence-point`. It is enabled automatically by `-Wall` in both GCC and Clang and flags expressions like `a[i] = i++` at compile time.
</details>

---

<details>
<summary><strong>9. How does `setjmp` and `longjmp` interact with sequence points and local variables?</strong></summary>

When `longjmp` is called, all accessible objects have indeterminate values unless they are declared `volatile` or their values were not modified between the `setjmp` invocation and the `longjmp` call. This is because the compiler may store non-volatile variables purely in CPU registers, which are overwritten during execution.
</details>

<details>
<summary><strong>10. What is the sequence point guarantee when calling a function?</strong></summary>

There is a sequence point after the evaluation of the function designator and all function arguments, immediately before the first expression in the function body executes. This guarantees that all side effects in argument evaluation are fully committed before the function begins.
</details>

<details>
<summary><strong>11. Are operations inside a signal handler sequenced relative to interrupted code?</strong></summary>

No! An asynchronous signal can interrupt a thread at any arbitrary machine instruction boundary. Therefore, accessing non-atomic, non-volatile global variables from inside a signal handler is undefined behavior. Conforming handlers must only modify variables of type `volatile sig_atomic_t` or lock-free atomic types.
</details>

<details>
<summary><strong>12. Does calling `qsort()` introduce sequence points between comparator calls?</strong></summary>

Yes. Each invocation of the comparison callback function by `qsort()` is a separate function call boundary with its own sequence points. However, the order in which `qsort()` compares elements is unspecified.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Identifying Sequence Points</strong></summary>

**Goal**: Identify all sequence points in this code snippet:
```c
int a = 5;
if (a > 0 && ++a < 10) {
    printf("%d\n", a);
}
```

**Solution**:
1. At the semicolon `;` after `int a = 5;`.
2. After the left operand `a > 0` of the `&&` operator.
3. At the end of the full controlling expression of the `if` statement (before entering the `{}` block).
4. At the function call boundary before `printf` begins execution.
5. At the semicolon `;` after the `printf` call.
</details>

<details>
<summary><strong>Exercise 2 (Medium): Fixing an Unsequenced Array Assignment</strong></summary>

**Goal**: Explain why this loop contains undefined behavior, and repair it:
```c
int arr[5] = {0};
int i = 0;
while (i < 5) {
    arr[i++] = i;
}
```

**Solution**:
In `arr[i++] = i;`, the scalar object `i` is being modified by `i++` while simultaneously being read in the right-hand operand `i`, with no intervening sequence point. This is undefined behavior.
Conforming fix:
```c
int arr[5] = {0};
for (int i = 0; i < 5; ++i) {
    arr[i] = i;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Eliminating Multi-Threaded Reordering Hazards</strong></summary>

**Goal**: Why is a sequence point within a single thread insufficient to guarantee ordering across multiple CPU threads? What is required?

**Solution**:
Sequence points only establish sequenced-before relations within a **single thread of execution**. Modern multi-core processors use out-of-order execution and store buffers that can reorder memory writes across cores. To guarantee ordering across threads, one must use C11 `<stdatomic.h>` with explicit **memory order barriers** (`memory_order_release`, `memory_order_acquire`) or mutex locks that establish a cross-thread "synchronizes-with" relationship.
</details>

---

<details>
<summary><strong>Exercise 4 (Hard): Diagnosing Unsequenced Macro Expansion</strong></summary>

**Goal**: Identify why the following macro causes unsequenced undefined behavior when invoked as `CLAMP(++val, 0, 100)`:
```c
#define CLAMP(x, low, high) (((x) < (low)) ? (low) : (((x) > (high)) ? (high) : (x)))
```

**Solution**:
The argument `x` is expanded up to three times in the ternary tree. Invoking it with `++val` results in evaluating multiple sub-expressions containing `++val` without intervening sequence points, invoking unsequenced undefined behavior.
Fix: Use a `static inline` function to evaluate parameters exactly once:
```c
static inline int clamp(int val, int low, int high) {
    if (val < low) return low;
    if (val > high) return high;
    return val;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core UB Rule**: Never read and write, or write twice, to the same scalar without a sequence point.
- **`i = i++` is UB**: Keep increment operators on their own standalone lines.
- **Function argument evaluation**: Unspecified order; never rely on parameter evaluation sequence.
- **Sequence points exist at**: `;`, `&&`, `||`, `?:`, `,`, and function entry.
- Subexpressions in `+`, `-`, `*`, `/` are **unsequenced**.
- Compile with `-Wall -Wsequence-point`.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 5.1.2.3: "Program execution" & Section 6.5: "Expressions".
- SEI CERT C Coding Standard: *EXP30-C. Do not depend on the order of evaluation for side effects*.
- Derek M. Jones: *The New C Standard: An Economic and Cultural Commentary*.

---

[← Previous: 13-integer-promotions-and-usual-arithmetic-conversions.md](./13-integer-promotions-and-usual-arithmetic-conversions.md) | [Section Index](./README.md) | [Next: 15-lvalues-rvalues-and-expressions.md](./15-lvalues-rvalues-and-expressions.md) →
