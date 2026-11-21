# Lvalues, Rvalues, and Value Categories

> A foundational exploration of C expression semantics: locator values (lvalues), expression values (rvalues), modifiable vs non-modifiable lvalues, array decay mechanics, compound literals as lvalues, and the mechanics of address-of (`&`) and assignment (`=`).

| Metadata | Details |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [02-data-types.md](./02-data-types.md), [05-operators.md](./05-operators.md), [12-const-correctness-basics.md](./12-const-correctness-basics.md) |
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

In C, an expression is not merely a formula that evaluates to a mathematical number; it is an abstraction classified into formal **value categories**. The most fundamental distinction in C grammar is between an **lvalue** and an **rvalue**.

Understanding value categories is essential for mastering systems C:
1. **Assignment Validity**: The left-hand operand of an assignment operator (`=`, `+=`) **must be a modifiable lvalue**. Understanding why `x = 5` compiles but `x + 1 = 5` fails prevents confusing compiler errors.
2. **Address-Of Restrictions**: The unary address-of operator `&` strictly requires an **lvalue**. You cannot take the address of an rvalue (`&5` or `&(a + b)` is a fatal syntax violation).
3. **Compound Literals and Temporary Lifetime**: C99 introduced compound literals (e.g. `(int[]){ 1, 2, 3 }`), which—unlike simple numeric literals—are **lvalues with dedicated storage** that can be mutated and whose addresses can be passed to functions.

---

## 2. Core Concepts

### Definitions: Lvalues and Rvalues

- **Lvalue (Locator Value)**: An expression that designates a specific, identifiable region of computer storage (memory). Because an lvalue refers to physical memory with an address, its address can be retrieved using `&` (unless it is declared with the `register` storage class or is a bitfield).
- **Rvalue (Value of an Expression)**: An expression that evaluates to a transient mathematical value without any persistent identifiable memory address. Examples include integer literals (`42`), arithmetic results (`a + b`), or function return values returned by value.

```
+-------------------------------------------------------------------------+
|                       C VALUE CATEGORY HIERARCHY                        |
+-------------------------------------------------------------------------+
|  EXPRESSIONS                                                            |
|       |                                                                 |
|       +---> LVALUES (Designates memory object; has address)             |
|       |        |                                                        |
|       |        +---> Modifiable Lvalues (int x; arr[0]; *ptr)           |
|       |        |     (Can be placed on left side of '=')                |
|       |        |                                                        |
|       |        +---> Non-Modifiable Lvalues (const int c; int arr[5];)  |
|       |              (Has memory address, but CANNOT be assigned to)    |
|       |                                                                 |
|       +---> RVALUES (Transient computation; no addressable location)    |
|             (Literals: 42; Temporaries: a + b; Function returns)        |
+-------------------------------------------------------------------------+
```

### Modifiable vs. Non-Modifiable Lvalues

Not all lvalues can be assigned to. To be placed on the left-hand side of an assignment operator (`=`), an lvalue must be **modifiable**:
- **Modifiable Lvalue**: An lvalue that does **not** have an array type, does **not** have an incomplete type, does **not** have a `const`-qualified type, and (if a struct/union) has no members with `const` qualification.
- **Non-Modifiable Lvalue Examples**:
  - `const int max_limit = 100;` (`max_limit` has a memory address, but cannot be assigned to).
  - Array identifiers: In `int buffer[10];`, `buffer` is an lvalue (it designates storage), but it is **non-modifiable**; you cannot write `buffer = other_buffer;`!

---

## 3. How It Works Internally

### Lvalue-to-Rvalue Conversion at the Assembly Level

When an lvalue appears in a context where its value is required (for example, on the right-hand side of an assignment `y = x;`), the compiler generates an **Lvalue-to-Rvalue Conversion**:

```
Source Code:
int x = 42;
int y = x;   <-- 'x' is an lvalue on the right; undergoes lvalue-to-rvalue conversion

Generated Assembly (x86-64):
mov     eax, DWORD PTR [rbp-4]   ; Load value from memory address of x into register %eax
mov     DWORD PTR [rbp-8], eax   ; Store rvalue from %eax into memory address of y
```

1. **Lvalue phase**: Compiler determines the virtual memory address of `x` (`[rbp-4]`).
2. **Conversion phase**: CPU reads physical memory at that address and moves the data bits into a high-speed CPU architectural register (`%eax`).
3. **Rvalue phase**: The value inside `%eax` is now a transient rvalue ready for arithmetic or storage.

### Array Decay: From Non-Modifiable Lvalue to Rvalue Pointer

In almost all C expressions, an identifier of array type `T[N]` implicitly **decays into an rvalue pointer** of type `T *` pointing to the array's initial element:

```c
int arr[5];
int *ptr = arr; /* 'arr' decays into rvalue pointer (int *)&arr[0] */
```

*The Three Strict Exceptions to Array Decay*:
1. When passed as the operand of `sizeof` (`sizeof(arr)` returns the full $5 \times 4 = 20$ bytes, not pointer size!).
2. When passed as the operand of unary address-of `&` (`&arr` produces a pointer to the entire array of type `int (*)[5]`).
3. When used as a string literal initializer for a character array (`char s[] = "hello";`).

---

## 4. Syntax & Usage

### 1. Compound Literals as Mutable Lvalues
Introduced in ISO C99, compound literals allow the creation of unnamed objects that are true **lvalues**:

```c
/* Compound literal of struct type: can pass address directly to functions! */
typedef struct { int x, y; } Point;

void draw_point(const Point *p);

void render(void) {
    /* (Point){10, 20} creates an lvalue with automatic storage duration */
    draw_point(&(Point){10, 20});
}

/* Modifiable compound literal array */
void mutate_temporary(void) {
    int *dynamic_list = (int[]){ 10, 20, 30 };
    dynamic_list[0] = 99; /* Valid! Compound literals are modifiable lvalues */
}
```

### 2. Pointer Dereference Produces an Lvalue
The unary dereference operator `*` turns a pointer rvalue into an lvalue:
```c
int val = 0;
int *p = &val;
*p = 42; /* '*p' is a modifiable lvalue referring to the storage of 'val' */
```

---

## 5. Practical Examples

### Value Categories and Expression Semantics Demonstration Program

```c
/* File: 01-languages/c/01-fundamentals/examples/15-lvalues-rvalues-and-expressions.c */
#include <stdio.h>

int main(void) {
    printf("=== Lvalues, Rvalues, and Expressions ===\n");
    
    /* Modifiable lvalue */
    int x = 42;
    x = 100; /* x on left is lvalue; 100 on right is rvalue */
    printf("Modifiable lvalue x = %d\n", x);
    
    /* Compound literal is an lvalue */
    int *arr = (int[]){ 1, 2, 3 };
    arr[0] = 10; /* Mutating compound literal */
    printf("Compound literal mutated first element: %d\n", arr[0]);

    /* Non-modifiable lvalue */
    const int ro = 50;
    printf("Non-modifiable lvalue 'ro': %d (has address: %p)\n", ro, (const void *)&ro);

    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 15-lvalues-rvalues-and-expressions.c -o run_15.exe
./run_15.exe
```
Output:
```text
=== Lvalues, Rvalues, and Expressions ===
Modifiable lvalue x = 100
Compound literal mutated first element: 10
Non-modifiable lvalue 'ro': 50 (has address: 0061ff08)
```

---

## 6. Best Practices

1. **Understand Value Categories When Reading Compiler Errors**: Errors like `lvalue required as left operand of assignment` or `lvalue required as unary '&' operand` mean you supplied an ephemeral calculation result where physical memory storage was expected.
2. **Leverage Compound Literals for Clean API Calls**: Instead of declaring dummy local structs for one-off function calls, pass compound literals `&(Config){ .timeout = 500, .retries = 3 }`.
3. **Be Aware of Compound Literal Storage Lifetimes**: A compound literal declared inside a block `{}` has **automatic storage duration**; returning a pointer to an inner-block compound literal creates a dangling pointer bug!
4. **Distinguish `arr` from `&arr`**: While both evaluate to the same numeric memory address at runtime, their types and pointer arithmetic offsets differ dramatically (`int *` vs `int (*)[N]`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Attempting to assign to an array: `arr1 = arr2;` | Arrays are **non-modifiable lvalues**. | Use `memcpy(arr1, arr2, sizeof(arr1));`. |
| Taking address of an rvalue: `&10` or `&(a + b)` | Unary `&` strictly requires an lvalue with an identifiable memory address. | Store result in a variable first: `int sum = a + b; int *p = &sum;`. |
| Returning pointer to block-scope compound literal | The compound literal is allocated on the stack; it is destroyed when the enclosing block exits. | Allocate on heap via `malloc` or declare with `static`. |
| `x++ = 5;` | Postfix `++` returns the original value as an **rvalue**; rvalues cannot be assigned to. | Write `x = 5;` or execute statements separately. |
| Taking address of a `register` variable | The `register` keyword explicitly forbids taking an object's address, even if stored in RAM. | Remove `register` specifier. |
| Taking address of a struct bitfield member | Bitfields do not occupy whole byte addresses; taking their address violates ISO C grammar. | Copy bitfield value to a standard integer variable before taking address. |

---

## 8. Performance & Security Notes

- **Register Spilling vs. Memory Addresses**: Whenever code takes the address of an automatic variable (`&var`), the compiler is **forced to allocate physical stack memory** for that variable. It can no longer optimize the variable to exist purely within high-speed CPU registers throughout the function.
- **Lifetime Exploits with Compound Literals**: If a compound literal is defined at file scope (outside all functions), it receives **static storage duration**. If declared inside a block, it receives **automatic storage duration**. Misunderstanding this difference leads to use-after-free security vulnerabilities.

---

## 9. Comparison / When to Use What

| Expression Form | Category | Has Addressable Memory? | Can Be Assigned To (`=`)? | Lifetime |
|---|---|---|---|---|
| `int x;` | Modifiable Lvalue | **Yes** (`&x`) | **Yes** (`x = 5`) | Automatic or Static |
| `const int x;` | Non-modifiable Lvalue | **Yes** (`&x`) | **No** (Compiler error) | Automatic or Static |
| `int arr[10];` | Non-modifiable Lvalue | **Yes** (`&arr`) | **No** (Array type) | Automatic or Static |
| `42` / `3.14f` | Rvalue | **No** | **No** | Transient (Literal) |
| `a + b` | Rvalue | **No** | **No** | Transient (Evaluation) |
| `(int[]){1, 2}` | Modifiable Lvalue | **Yes** | **Yes** | Block scope / Static |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is an lvalue in standard C? Does "lvalue" mean "left-hand side of assignment"?</strong></summary>

In standard C, an lvalue is an expression that designates an object in memory (a "locator value"). The historical mnemonic "left-hand side of assignment" is inaccurate in modern C because **non-modifiable lvalues** (such as `const int x` or array identifiers) are true lvalues with memory addresses, but can **never** appear on the left-hand side of an assignment.
</details>

<details>
<summary><strong>2. Why is `&(a + b)` illegal in C?</strong></summary>

The unary address-of operator `&` requires its operand to be an lvalue designating an identifiable object in memory. The binary addition operator `+` produces an **rvalue** (a transient value held in a CPU register or temporary storage), which has no persistent memory address.
</details>

<details>
<summary><strong>3. What are the three scenarios where an array identifier does NOT decay into a pointer?</strong></summary>

1. When passed as the operand of the `sizeof` operator (computes full array byte size).
2. When passed as the operand of the unary address-of operator `&` (yields a pointer to the entire array: `T (*)[N]`).
3. When used as an initializer string literal for a character array (`char s[] = "hello";`).
</details>

<details>
<summary><strong>4. What is a compound literal, and what value category does it belong to?</strong></summary>

Introduced in C99, a compound literal consists of a parenthesized type name followed by an initializer list (e.g. `(int[]){1, 2, 3}`). It evaluates to an unnamed object that is a **modifiable lvalue**. Its address can be taken, its elements can be mutated, and its storage duration matches the scope in which it is declared (automatic in blocks, static at file scope).
</details>

<details>
<summary><strong>5. Why does `(a > b ? a : b) = 10;` compile in C++, but fail to compile in C?</strong></summary>

In C++, the conditional operator `?:` can yield an lvalue if both second and third operands are lvalues of the same type. In C, the conditional operator `?:` **always yields an rvalue**, regardless of whether its operands are lvalues. Therefore, in C, the result cannot appear on the left-hand side of an assignment.
</details>

<details>
<summary><strong>6. Can you take the address of a variable declared with the `register` storage class?</strong></summary>

No. Section 6.5.3.2 of the ISO C standard explicitly states that the unary `&` operator shall not be applied to an object declared with the `register` storage class specifier, even if the compiler actually allocated the variable in RAM rather than a CPU register.
</details>

<details>
<summary><strong>7. What is the difference between `arr` and `&arr` for `int arr[5];`?</strong></summary>

Both expressions evaluate to the same numeric address in memory, but their types are fundamentally different:
- `arr` decays into type `int *` (pointer to a single integer). Adding 1 (`arr + 1`) advances by `sizeof(int)` (4 bytes).
- `&arr` has type `int (*)[5]` (pointer to an array of 5 integers). Adding 1 (`&arr + 1`) advances by the size of the entire array: $5 \times 4 = 20$ bytes!
</details>

<details>
<summary><strong>8. Why is an array identifier a non-modifiable lvalue?</strong></summary>

An array identifier designates physical memory storage (hence an lvalue), but the C language grammar defines array types as inherently non-assignable. Arrays cannot be modified as an aggregate unit via the `=` operator; elements must be modified individually or copied via `memcpy()`.
</details>

---

<details>
<summary><strong>9. Can a function call expression ever evaluate to an lvalue in standard C?</strong></summary>

No! In standard C, the result of a function call is **always an rvalue**, even if the function returns a struct or a pointer. While you can dereference a returned pointer `*get_ptr() = 10` (because the dereference operator `*` produces an lvalue), the function call itself `get_struct().x = 10` cannot be assigned to directly in C.
</details>

<details>
<summary><strong>10. How does a cast operator affect the value category of an expression?</strong></summary>

Under ISO C, applying an explicit cast `(type)expr` **always produces an rvalue**, even if `expr` was originally an lvalue! For example, if `int x = 10;`, writing `(int)x = 20;` is illegal and rejected with a compiler error because casting `x` creates a transient rvalue.
</details>

<details>
<summary><strong>11. What is the storage duration and lifetime of a compound literal declared at file scope?</strong></summary>

A compound literal declared at file scope (outside all function bodies) has **static storage duration**. Its memory is allocated in the `.data` or `.bss` segment and persists for the entire execution lifetime of the program.
</details>

<details>
<summary><strong>12. Why is `(x, y)` an lvalue or an rvalue in C?</strong></summary>

In C, the comma operator `,` evaluates the left operand, discards it, and evaluates the right operand. The result of the comma operator is **always an rvalue** in C (unlike C++, where it can be an lvalue if the right operand is an lvalue). Therefore, `(x, y) = 10;` is invalid in C.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Classifying Value Categories</strong></summary>

**Goal**: Classify each expression below as a Modifiable Lvalue, Non-Modifiable Lvalue, or Rvalue:
1. `int x = 10;` -> Expression: `x`
2. `const int y = 20;` -> Expression: `y`
3. `x + y`
4. `(int[]){ 5, 10 }[0]`

**Solution**:
1. Modifiable Lvalue.
2. Non-Modifiable Lvalue (const-qualified).
3. Rvalue (arithmetic result).
4. Modifiable Lvalue (array subscript of compound literal).
</details>

<details>
<summary><strong>Exercise 2 (Medium): Passing Temporary Structs via Compound Literals</strong></summary>

**Goal**: Given a function `void set_bounds(const struct Rect *r)`, call it without declaring a temporary named local variable, using a C99 compound literal.

**Solution**:
```c
struct Rect {
    int x, y, width, height;
};

void set_bounds(const struct Rect *r);

void apply_layout(void) {
    /* Using compound literal as an lvalue */
    set_bounds(&(struct Rect){ .x = 0, .y = 0, .width = 1920, .height = 1080 });
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Fixing Pointer Decay Arithmetic Mismatches</strong></summary>

**Goal**: Given an array `int matrix[3][4]`, write a function that receives a pointer to an entire row of 4 elements without decay, verifying pointer step offsets.

**Solution**:
```c
#include <stdio.h>

/* Parameter is a pointer to an array of 4 ints: int (*)[4] */
void process_row(int (*row)[4], size_t row_index) {
    printf("First element of row %zu: %d\n", row_index, (*row)[0]);
}

int main(void) {
    int matrix[3][4] = {
        { 1, 2, 3, 4 },
        { 5, 6, 7, 8 },
        { 9, 10, 11, 12 }
    };

    /* matrix + 1 advances by 16 bytes (entire row of 4 ints) */
    process_row(&matrix[1], 1);
    return 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Lvalue**: Designates an object in memory; has a memory address (`&lvalue`).
- **Modifiable Lvalue**: Non-array, non-const, complete type; can appear on left of `=`.
- **Rvalue**: Transient computation value; cannot take address (`&rvalue` is illegal).
- **Arrays**: Non-modifiable lvalues; decay into rvalue pointer to first element in most expressions.
- **Compound Literals**: `(T){...}` creates an unnamed modifiable lvalue.
- `sizeof` and `&` do not cause arrays to decay.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 6.3.2.1: "Lvalues, arrays, and function designators".
- Eli Bendersky: *Understanding lvalues and rvalues in C and C++*.
- Jens Gustedt: *Modern C* (Expressions and Values).

---

[← Previous: 14-sequence-points-and-evaluation-order.md](./14-sequence-points-and-evaluation-order.md) | [Section Index](./README.md) | [Next: ../02-core-concepts/01-arrays.md](../02-core-concepts/01-arrays.md) →
