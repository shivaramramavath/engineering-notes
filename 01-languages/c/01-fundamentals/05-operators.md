# Operators and Precedence

> The complete architectural catalog of C operators: arithmetic, bitwise masks, logical short-circuiting, the comma operator, operator precedence and associativity hierarchies, sequence points vs "sequenced-before" relationships, and undefined evaluation orders.

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [02-data-types.md](./02-data-types.md), [03-integer-representation.md](./03-integer-representation.md) |
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

In C, operators are not high-level polymorphic functions; they map almost directly to underlying microprocessor instruction set primitives (arithmetic logic units, bit-shifters, condition flags). 

Misunderstanding operator semantics causes severe defects:
1. **Unspecified Evaluation Order and UB**: In expressions like `f(i++, i++)` or `a[i] = i++`, developers assume left-to-right evaluation. In standard C, the order in which function arguments or operand sub-expressions are evaluated is **unspecified**, and modifying an object twice without an intervening sequence point invokes **undefined behavior**.
2. **Precedence Misconceptions**: Bitwise operators (`&`, `|`, `^`) have **lower precedence** than equality operators (`==`, `!=`). Writing `if (flags & MASK == 0)` evaluates as `flags & (MASK == 0)`, causing catastrophic security bugs!
3. **Short-Circuit Side Effect Failures**: Relying on an increment or function call inside the right-hand operand of `&&` or `||` leads to silent logic bugs when the left operand short-circuits.

---

## 2. Core Concepts

### Complete Operator Precedence and Associativity Table

The table below summarizes all C operators, ordered from highest precedence (Level 1) to lowest (Level 15):

| Precedence | Operator Category | Operators | Associativity |
|---|---|---|---|
| **1 (Highest)**| Postfix / Member Access | `()` `[]` `->` `.` `++` `--` (postfix) `(compound literal)` | Left-to-Right |
| **2** | Unary Prefix | `++` `--` (prefix) `+` `-` `!` `~` `(type)` `*` `&` `sizeof` `_Alignof` | **Right-to-Left** |
| **3** | Multiplicative | `*` `/` `%` | Left-to-Right |
| **4** | Additive | `+` `-` | Left-to-Right |
| **5** | Bitwise Shift | `<<` `>>` | Left-to-Right |
| **6** | Relational | `<` `<=` `>` `>=` | Left-to-Right |
| **7** | Equality | `==` `!=` | Left-to-Right |
| **8** | Bitwise AND | `&` | Left-to-Right |
| **9** | Bitwise XOR | `^` | Left-to-Right |
| **10** | Bitwise OR | `\|` | Left-to-Right |
| **11** | Logical AND | `&&` (Short-circuiting) | Left-to-Right |
| **12** | Logical OR | `\|\|` (Short-circuiting) | Left-to-Right |
| **13** | Conditional (Ternary)| `?:` | **Right-to-Left** |
| **14** | Assignment | `=` `+=` `-=` `*=` `/=` `%=` `<<=` `>>=` `&=` `^=` `\|=` | **Right-to-Left** |
| **15 (Lowest)** | Comma | `,` (Sequenced) | Left-to-Right |

---

## 3. How It Works Internally

### Sequence Points vs. Modern "Sequenced-Before" Relation

In C89 and C99, order of operations was defined using the concept of **sequence points** (points in execution where all side effects of previous evaluations are guaranteed to be complete). In ISO C11, C17, and C23, this was formalized with strict mathematical partial-order relations:
- **Sequenced-Before**: A strict, asymmetric, transitive relation between two evaluations within the same thread.
- **Indeterminately Sequenced**: Evaluation $A$ is sequenced before $B$, or $B$ is sequenced before $A$, but which one is unspecified.
- **Unsequenced**: Neither is sequenced before the other.

```
Rule of Undefined Behavior:
If a side effect on a scalar object is UNSEQUENCED relative to another side effect 
on the same scalar object, or relative to a value computation using the value of 
the same object, the behavior is UNDEFINED.
```

```
Examples of Undefined Behavior:
i = i++;            /* UB: Store to i is unsequenced with respect to post-increment */
a[i] = i++;         /* UB: Sub-expression read of i is unsequenced with write to i */
f(v++, v++);        /* UB: Two unsequenced modifications of v */
```

### Short-Circuit Evaluation Mechanics

Unlike bitwise operators (`&`, `|`) which always evaluate both operands, logical operators (`&&`, `||`) introduce an explicit sequence point:
- For `A && B`: $A$ is fully evaluated and all side effects completed. If $A == 0$, $B$ is **never evaluated**.
- For `A || B`: $A$ is fully evaluated and all side effects completed. If $A \ne 0$, $B$ is **never evaluated**.

```
       [ Evaluate A ]
             |
       +-----+-----+
       |           |
    (A == 0)    (A != 0)
       |           |
       v           v
  Return 0     [ Evaluate B ]  <-- B is ONLY evaluated if A is non-zero!
 (B skipped)       |
                   v
               Return Result
```

---

## 4. Syntax & Usage

### 1. Safe Bit Manipulation Idioms
```c
#include <stdint.h>

/* Set bit N */
#define BIT_SET(val, n)   ((val) |= (1U << (n)))

/* Clear bit N */
#define BIT_CLEAR(val, n) ((val) &= ~(1U << (n)))

/* Toggle bit N */
#define BIT_TOGGLE(val, n)((val) ^= (1U << (n)))

/* Check bit N */
#define BIT_CHECK(val, n) (((val) >> (n)) & 1U)
```

### 2. Comma Operator Usage
The comma operator `,` evaluates its left operand, discards the result, introduces a sequence point, and evaluates and returns the right operand:
```c
/* Canonical conforming use in for loop updates */
for (int i = 0, j = 10; i < j; ++i, --j) {
    /* Both i and j updated within loop iteration */
}
```

---

## 5. Practical Examples

### Operators and Precedence Demonstration Program

```c
/* File: 01-languages/c/01-fundamentals/examples/05-operators.c */
#include <stdio.h>
#include <stdint.h>

int main(void) {
    printf("=== Operators & Bitwise Precedence Demo ===\n");
    
    /* Bitwise manipulation */
    uint8_t flags = 0x05; /* 0000 0101 */
    uint8_t mask = 0x02;  /* 0000 0010 */
    
    /* Set bit */
    flags |= mask;
    printf("Flags after setting bit 1: 0x%02X\n", flags);
    
    /* Short-circuit evaluation */
    int side_effect = 0;
    int a = 0;
    if (a != 0 && ++side_effect) {
        /* Not reached */
    }
    printf("Short-circuit: side_effect = %d (should be 0)\n", side_effect);

    /* Ternary operator */
    int x = 15;
    int max_val = (x > 10) ? x : 10;
    printf("Ternary result: %d\n", max_val);

    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 05-operators.c -o run_05.exe
./run_05.exe
```
Output:
```text
=== Operators & Bitwise Precedence Demo ===
Flags after setting bit 1: 0x07
Short-circuit: side_effect = 0 (should be 0)
Ternary result: 15
```

---

## 6. Best Practices

1. **Always Parenthesize Bitwise Operations with Relational/Equality Operators**: Bitwise `&`, `|`, and `^` have lower precedence than `==`, `<`, and `>`. Always write `if ((val & MASK) == 0)`, never `if (val & MASK == 0)`.
2. **Never Put Modifying Expressions in Macro Arguments**: Macros evaluate their parameters multiple times. If `SQUARE(x)` is `#define SQUARE(x) ((x) * (x))`, writing `SQUARE(i++)` invokes undefined behavior!
3. **Never Rely on Subexpression Evaluation Order**: In `f(get_a(), get_b())`, the compiler is free to call `get_b()` before `get_a()`. If they share state, evaluate them sequentially on separate lines.
4. **Avoid Writing Increment/Decrement in Complex Expressions**: Write `i++` on its own dedicated statement line to eliminate sequence point ambiguities.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| `if (flags & MASK == 0)` | `==` binds tighter than `&`. Evaluates as `flags & (MASK == 0)`. | Parenthesize: `if ((flags & MASK) == 0)`. |
| `i = i++;` | Unsequenced write and post-increment on the same scalar object invokes undefined behavior. | Simply write `i++;`. |
| `a[i] = i++;` | Unsequenced access and modification of `i` invokes undefined behavior. | Separate lines: `a[i] = i; i++;`. |
| Calling `f(i++, i++)` | Function argument evaluations are unsequenced relative to each other; UB. | Compute values into temporary variables first. |
| Using `&` instead of `&&` for logic | Bitwise `&` does not short-circuit; both sides are always evaluated, causing null pointer dereferences if guarding pointers! | Use `&&` for logical boolean expressions. |
| Shifting by a negative value or $\ge$ bit width | `val << 32` on a 32-bit integer is undefined behavior on x86 processors. | Validate shift bounds: `assert(shift >= 0 && shift < 32);`. |

---

## 8. Performance & Security Notes

- **Branchless Logic via Conditional Moves**: Modern optimizing compilers translate the ternary operator `(x > 0) ? a : b` into `cmovg` (Conditional Move) assembly instructions, eliminating branch mispredictions and protecting cryptographic implementations against timing side-channel attacks.
- **Short-Circuit Null Checks**: The idiom `if (ptr != NULL && ptr->is_valid)` is guaranteed safe by ISO standards because the right-hand operand is never evaluated if `ptr == NULL`.

---

## 9. Comparison / When to Use What

| Operator Type | Operators | Evaluates Both Operands? | Evaluates in Parallel / Undefined? |
|---|---|---|---|
| **Bitwise** | `&` `\|` `^` | **Always** evaluates both operands | Subexpressions unsequenced |
| **Logical** | `&&` `\|\|` | **Short-circuits** (skips right if determined) | Strictly sequenced left-to-right |
| **Ternary** | `?:` | Evaluates condition, then **only one** branch | Condition sequenced before branch |
| **Comma** | `,` | Evaluates left, then evaluates right | Strictly sequenced left-to-right |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `i = i++` undefined behavior in C?</strong></summary>

Under the ISO C standard, if a side effect on a scalar object is unsequenced relative to another side effect on the same scalar object, the behavior is undefined. The assignment to `i` (a side effect) and the post-increment of `i` (another side effect) occur without an intervening sequence point, making the operation undefined.
</details>

<details>
<summary><strong>2. What is the precedence of the bitwise AND operator `&` relative to the equality operator `==`?</strong></summary>

The equality operator `==` has **higher precedence** (Level 7) than the bitwise AND operator `&` (Level 8). Therefore, `x & 1 == 0` is parsed as `x & (1 == 0)`.
</details>

<details>
<summary><strong>3. How does short-circuit evaluation guarantee safety in pointer guards?</strong></summary>

The `&&` operator introduces a strict sequence point: the left-hand operand is completely evaluated before the right-hand operand is considered. If the left operand evaluates to false (e.g. `ptr == NULL`), evaluation terminates immediately, guaranteeing that `ptr->field` in the right operand is never executed.
</details>

<details>
<summary><strong>4. What is the value and side effect of the expression `(x = 1, x + 5)`?</strong></summary>

The comma operator evaluates the left operand `x = 1`, discards its result, completes all side effects (storing `1` into `x`), and then evaluates and returns the right operand `x + 5`. The value of the overall expression is `6`.
</details>

<details>
<summary><strong>5. In what order are function call arguments evaluated in standard C?</strong></summary>

The order of evaluation of function call arguments (e.g. `f(g(), h())`) is **unspecified**. The compiler may evaluate `g()` first or `h()` first, and the order may vary between optimization levels or compilers.
</details>

<details>
<summary><strong>6. What does the prefix operator `++i` return versus the postfix operator `i++`?</strong></summary>

Prefix `++i` increments `i` and evaluates to the **new** value of `i`. Postfix `i++` increments `i` as a side effect, but evaluates to the **original, pre-increment** value of `i`.
</details>

<details>
<summary><strong>7. Why is `1U << 32` undefined behavior on a system where `unsigned int` is 32 bits?</strong></summary>

Section 6.5.7 of the ISO C standard states that if the value of the right operand is negative or is greater than or equal to the width of the promoted left operand, the behavior is undefined. On x86 processors, the shift count is masked by the hardware (`shift & 31`), meaning `1U << 32` would unexpectedly produce `1U << 0 == 1`.
</details>

<details>
<summary><strong>8. What is the associativity of the assignment operator `=` and ternary operator `?:`?</strong></summary>

Both have **right-to-left associativity**. Therefore, `a = b = c = 0` is evaluated as `a = (b = (c = 0))`, and `a ? b : c ? d : e` is evaluated as `a ? b : (c ? d : e)`.
</details>

---

<details>
<summary><strong>9. How is the resulting type of a conditional (ternary) expression `condition ? expr1 : expr2` determined?</strong></summary>

Under ISO C, the type of the ternary expression is determined at **compile time** using the Usual Arithmetic Conversions applied to both branches, regardless of which branch is taken at runtime. For example, in `int x = 1; float y = 2.0f; auto res = cond ? x : y;`, the expression type is `float` in both cases, and `x` is converted to `float` if `cond` is true.
</details>

<details>
<summary><strong>10. What is the C23 `typeof` and `typeof_unqual` operator?</strong></summary>

C23 standardized the long-standing GCC `__typeof__` extension as native keywords: `typeof(expr)` yields the exact type of an expression including qualifiers (`const`, `volatile`), while `typeof_unqual(expr)` strips all type qualifiers. This allows safe, generic macro definitions:
```c
#define SWAP(a, b) do { \
    typeof_unqual(a) _tmp = (a); \
    (a) = (b); \
    (b) = _tmp; \
} while (0)
```
</details>

<details>
<summary><strong>11. Why does `sizeof('a')` equal 4 in C but 1 in C++?</strong></summary>

In C, character literals like `'a'` have type `int` (which is typically 4 bytes). In C++, character literals have type `char` (which is guaranteed to be 1 byte). Therefore, `sizeof('a') == sizeof(int)` in C, but `sizeof('a') == 1` in C++.
</details>

<details>
<summary><strong>12. What is the difference between bitwise shift and arithmetic division by power of two for negative signed integers?</strong></summary>

In C, integer division `/` truncates toward zero: `-5 / 2 == -2`. However, an arithmetic right shift `>>` (on systems that implement it) floors toward negative infinity: `-5 >> 1 == -3`. Therefore, right-shifting negative numbers is not equivalent to division by powers of two!
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Parenthesizing Precedence Traps</strong></summary>

**Goal**: Insert explicit parentheses into this expression to reflect standard C compiler parsing:
```c
int result = a + b << c & d == e;
```

**Solution**:
Following standard precedence rules:
1. `+` (Additive, Level 4)
2. `<<` (Shift, Level 5)
3. `==` (Equality, Level 7)
4. `&` (Bitwise AND, Level 8)
```c
int result = ((a + b) << c) & (d == e);
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Bitwise Rounding to Next Power of Two</strong></summary>

**Goal**: Implement a function using bitwise shifts and OR operations that rounds a 32-bit unsigned integer up to the nearest power of two.

**Solution**:
```c
#include <stdint.h>

uint32_t round_up_pow2(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Fixing Unsequenced Sub-Expressions</strong></summary>

**Goal**: The following logging macro contains an unsequenced modification bug. Identify it and rewrite it into a safe static inline function.

```c
#define LOG_STEP(idx) printf("Step %d: next is %d\n", idx, ++idx)
```

**Solution**:
In `printf("...", idx, ++idx)`, the arguments are unsequenced; reading `idx` while modifying `++idx` is undefined behavior!
Conforming static inline fix:
```c
#include <stdio.h>

static inline void log_step(int *idx) {
    int current = *idx;
    (*idx)++;
    printf("Step %d: next is %d\n", current, *idx);
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Bitwise vs Equality Precedence Trap**: Always write `((val & MASK) == 0)`.
- **Short-circuiting**: `&&` and `||` guarantee left-to-right sequencing and skip right operand if outcome is determined.
- **Unsequenced UB**: Never read and write, or write twice, to the same scalar object without an intervening sequence point (`i = i++` is UB).
- **Argument evaluation order**: Unspecified in standard C; evaluate dependencies before calling functions.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 6.5: "Expressions".
- SEI CERT C Coding Standard: *EXP30-C. Do not depend on the order of evaluation for side effects*.
- Henry S. Warren Jr.: *Hacker's Delight* (Bitwise Arithmetic Logic).

---

[← Previous: 04-variables-constants.md](./04-variables-constants.md) | [Section Index](./README.md) | [Next: 06-type-conversions.md](./06-type-conversions.md) →
