# Declarations, Typedefs, and Declarator Grammar

> A masterclass in C declaration grammar: declaration specifiers vs declarators, parsing complex types using the Clockwise / Spiral Rule, `typedef` abstractions, function pointer syntax, and C23 `typeof` and `typeof_unqual` type deduction.

| Metadata | Details |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [03-pointers.md](./03-pointers.md), [01-fundamentals/08-functions.md](../01-fundamentals/08-functions.md) |
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

In C, the syntax for declaring variables and types is widely recognized as one of the most confusing grammars in computer science. Declarations in C are designed around the principle: *"Declaration reflects use"*. The declaration mirrors the syntax of expressions used to retrieve the underlying base type.

Failing to master C declaration syntax causes immediate developer paralysis:
1. **Unintelligible Complex Declarators**: Declarations like `void (*signal(int sig, void (*func)(int)))(int);` appear completely impenetrable without knowing the formal parsing algorithm.
2. **Subtle Pointer Declaration Bugs**: Writing `int* a, b;` does **not** declare two integer pointers; it declares `a` as an `int *` and `b` as a plain `int`! The asterisk binds to the declarator identifier, not the type specifier.
3. **Loss of Abstraction Without `typedef`**: Function pointers and hardware register tables written out raw clutter code, making refactoring impossible.

---

## 2. Core Concepts

### Anatomy of a C Declaration

Every declaration consists of two distinct components:

$$\text{Declaration} \equiv \text{Declaration Specifiers} + \text{Declarator}$$

1. **Declaration Specifiers**: Define storage class (`static`, `extern`), type qualifiers (`const`, `volatile`), and type specifiers (`int`, `double`, `struct Point`).
2. **Declarator**: Introduces the identifier and modifies the type using pointer symbols (`*`), array brackets (`[]`), or function parentheses (`()`).

```
Declaration:  static const int *ptr_array[10];
Specifiers:   [static const int]  --> Base type is static const integer
Declarator:   [*ptr_array[10]]     --> Array of 10 pointers to that base type
```

### The Clockwise / Spiral Rule for Parsing Declarations

To decipher any C declaration, start at the identifier and spiral outward clockwise, following operator precedence:
1. Start at the identifier.
2. Look to the **right**: If there are `[]` (array) or `()` (function), parse them first.
3. Look to the **left**: If there is a `*` (pointer), parse it.
4. If inside parentheses `(...)`, resolve the entire grouping before moving outside.
5. Finally, read the remaining base type specifiers on the far left.

```
Example: int (*fp)(char, float);
1. Start at identifier:           'fp'
2. Look right: hit ')' -> look left:  'is a pointer...'
3. Exit inner parens, look right: '...to a function taking (char, float)...'
4. Look left:                     '...and returning int.'
Result: fp is a POINTER to a FUNCTION taking (char, float) and returning INT.
```

---

## 3. How It Works Internally

### Decoding Complex Systems Declarations: The Classic `signal` Example

Consider the standard UNIX `signal()` declaration from `<signal.h>`:

```c
void (*signal(int sig, void (*func)(int)))(int);
```

Let us trace it using the Spiral Rule:
1. Start at identifier: `signal`
2. Look right: `(int sig, void (*func)(int))` $\rightarrow$ *"is a function taking an int and a function pointer `func`..."*
3. Look left: `*` $\rightarrow$ *"...returning a pointer..."*
4. Exit outer parentheses, look right: `(int)` $\rightarrow$ *"...to a function taking an int..."*
5. Look left: `void` $\rightarrow$ *"...and returning void."*

```
Dissected Structure:
   void (*signal(int sig, void (*func)(int)))(int);
   ^    ^ ^      ^                         ^ ^
   |    | |      |                         | +-- Taking an int
   |    | |      +-- Argument list         +---- (Outer parens)
   |    | +--------- Function identifier
   |    +----------- Returning pointer to function
   +---------------- Returning void
```

With `typedef`, this terrifying declaration becomes clean, human-readable prose:
```c
typedef void (*sighandler_t)(int);
sighandler_t signal(int sig, sighandler_t func);
```

---

## 4. Syntax & Usage

### 1. The Power of `typedef` for Encapsulation
```c
/* 1. Renaming structured data */
typedef struct UserAccount {
    int id;
    char username[32];
} UserAccount;

/* 2. Opaque Handle Idiom */
typedef struct DatabaseConnection *db_handle_t;

/* 3. Function pointer callback alias */
typedef int (*event_callback_t)(int event_id, void *context);
```

### 2. C23 `typeof` and `typeof_unqual`
Standardized in ISO C23 (replacing GCC `__typeof__`):
```c
int counter = 0;
typeof(counter) duplicate = 42; /* 'duplicate' is type int */

/* Generic Type-Safe Min Macro */
#define SAFE_MIN(a, b) ({ \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _a < _b ? _a : _b; \
})
```

---

## 5. Practical Examples

### Declarations and Complex Typedefs Demonstration Program

```c
/* File: 01-languages/c/02-core-concepts/examples/04-declarations-typedef.c */
#include <stdio.h>

/* Complex typedef: Function pointer taking int and returning int */
typedef int (*math_func_t)(int);

static int square(int x) { return x * x; }
static int cube(int x) { return x * x * x; }

int main(void) {
    printf("=== Declarations & Complex Typedefs Demo ===\n");
    
    /* Array of function pointers */
    math_func_t pipeline[2] = { square, cube };
    int input = 3;
    
    int step1 = pipeline[0](input);
    int step2 = pipeline[1](input);
    printf("square(%d) = %d\n", input, step1);
    printf("cube(%d) = %d\n", input, step2);

    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 04-declarations-typedef.c -o run_04.exe
./run_04.exe
```
Output:
```text
=== Declarations & Complex Typedefs Demo ===
square(3) = 9
cube(3) = 27
```

---

## 6. Best Practices

1. **Always Bind the Asterisk to the Identifier**: Write `int *p;` instead of `int* p;`. This visually reinforces that `*` belongs to the declarator, preventing the classic bug `int* a, b;` where `b` is mistakenly assumed to be a pointer.
2. **Use `typedef` for All Function Pointers**: Raw function pointer syntax is unreadable. Always wrap them in a clear `typedef`: `typedef void (*callback_t)(void);`.
3. **Avoid Overusing `typedef` for Primitive Structs**: Hiding pointers behind typedefs (e.g. `typedef struct Node *NodePtr;`) obscures memory ownership and indirection levels. Only hide pointers when designing completely opaque abstract handles (`db_handle_t`).
4. **Use C23 `typeof` for Generic Macro Design**: Eliminates double evaluation of macro arguments while maintaining complete type safety.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Declaring `int* a, b;` | The asterisk `*` modifies only `a`. `b` is compiled as a plain `int`! | Write `int *a, *b;` or put each on a separate line. |
| Forgetting parentheses in function pointers (`int *f(void)`) | Declares a function returning `int *`, NOT a function pointer! | Wrap pointer in parens: `int (*f)(void);`. |
| Confusing array of pointers with pointer to array | `int *arr[5]` is an array of 5 pointers. `int (*arr)[5]` is a pointer to an array of 5 ints. | Use the Clockwise Rule: `[]` binds tighter than `*`. |
| Hiding pointer indirection in typedefs (`typedef int* IntPtr;`) | When declared `const IntPtr p;`, `p` becomes a **const pointer** (`int * const`), NOT a pointer to const! | Keep pointer asterisks visible: `typedef int Value; const Value *p;`. |
| Attempting to define recursive structs via pure `typedef` | `typedef struct { Node *next; } Node;` fails because `Node` is not yet defined when `Node *next` is parsed. | Name the struct tag: `typedef struct Node { struct Node *next; } Node;`. |

---

## 8. Performance & Security Notes

- **Zero Runtime Overhead for `typedef`**: A `typedef` is strictly a compile-time alias evaluated during parsing. It does not introduce any type wrapping, memory allocation, or runtime overhead whatsoever.
- **Dispatch Table Security**: Arrays of function pointers (`pipeline[2]`) compile to indirect jumps (`jmp [table + rax*8]`). If an attacker overwrites an entry in a function pointer table, they hijack program execution. Always store function pointer tables in `.rodata` by qualifying them with `const`: `const math_func_t pipeline[] = { ... };`.

---

## 9. Comparison / When to Use What

| Pattern | Readability | Type Safety | Flexibility | Recommended Use |
|---|---|---|---|---|
| **Raw Function Pointer** | Lowest (Cryptic) | Strict | High | Never (Refactor with typedef) |
| **`typedef` Function Pointer** | Highest (Clean) | Strict | High | Callbacks, state machines |
| **`#define` Type Alias** | Poor | **Unsafe** (Textual) | Dangerous | Never (Always use `typedef`) |
| **C23 `typeof(expr)`** | High | Strict | Automated | Generic macros, template-like code |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does `int* a, b;` declare `b` as an `int` rather than an `int *`?</strong></summary>

In C declaration grammar, `int` is the declaration specifier, while `* a` and `b` are separate declarators. The pointer operator `*` is syntactically part of the individual declarator modifying `a`, not part of the base type specifier. Therefore, `a` receives pointer type, while `b` receives only the base type `int`.
</details>

<details>
<summary><strong>2. What does `int (*(*foo)(void))[3];` declare? Use the Clockwise Rule.</strong></summary>

1. Start at `foo`.
2. Look right: hit `)` -> look left: `*` $\rightarrow$ *"foo is a pointer..."*
3. Look right: `(void)` $\rightarrow$ *"...to a function taking void..."*
4. Look left: `*` $\rightarrow$ *"...returning a pointer..."*
5. Look right: `[3]` $\rightarrow$ *"...to an array of 3..."*
6. Look left: `int` $\rightarrow$ *"...integers."*
Result: `foo` is a pointer to a function taking void and returning a pointer to an array of 3 ints.
</details>

<details>
<summary><strong>3. What is the difference between `typedef` and `#define` for type aliasing?</strong></summary>

`#define` is a dumb preprocessor text replacement performed before compilation. It does not respect C grammar or scope, and fails on pointer declarations (e.g. `#define PTR int* \n PTR a, b;` declares `b` as int!). `typedef` is a formal compiler language feature that respects block scope, enforces type rules, and correctly qualifies pointers.
</details>

<details>
<summary><strong>4. If `typedef int *IntPtr;`, what type does `const IntPtr p;` produce?</strong></summary>

It produces a **const pointer to a mutable integer** (`int * const p`), NOT a pointer to const! `const` modifies the underlying `IntPtr` type (which is a pointer), making the pointer itself immutable. This is why hiding pointers inside typedefs is considered dangerous.
</details>

<details>
<summary><strong>5. How do you declare a pointer to an array of 10 integers?</strong></summary>

```c
int (*ptr)[10];
```
Parentheses around `*ptr` are mandatory because array brackets `[]` have higher precedence than the pointer dereference operator `*`. Omitting parentheses (`int *ptr[10]`) declares an array of 10 integer pointers!
</details>

<details>
<summary><strong>6. What is the difference between C23 `typeof` and `typeof_unqual`?</strong></summary>

`typeof(expr)` retains all type qualifiers (`const`, `volatile`, `atomic`). If `x` is `const int`, `typeof(x)` is `const int`. `typeof_unqual(expr)` strips all type qualifiers, producing plain `int`. This is critical when creating mutable temporary variables inside generic macros.
</details>

<details>
<summary><strong>7. How do you define a self-referential structure with `typedef`?</strong></summary>

```c
typedef struct Node {
    int data;
    struct Node *next; /* Must use 'struct Node', not 'Node' */
} Node;
```
Inside the struct definition, the typedef name `Node` is not yet completed; therefore, the self-referential pointer must refer to the struct tag `struct Node *next`.
</details>

<details>
<summary><strong>8. What does `typedef int Matrix[3][3];` do? How do you use it?</strong></summary>

It defines `Matrix` as an alias for a 2D array of $3 \times 3$ integers. You can instantiate it as `Matrix m;`, and access it as `m[0][0] = 1;`. Passing `m` to a function still decays into a pointer of type `int (*)[3]`.
</details>

---

<details>
<summary><strong>9. Can a `typedef` name be shadowed by an inner block variable with the same name?</strong></summary>

Yes! A `typedef` name resides in the ordinary identifier namespace. If an inner block declares a local variable with the exact same name as an outer `typedef`, the inner variable shadows the typedef name within that block scope, preventing the type from being used until the block exits.
</details>

<details>
<summary><strong>10. Why is `typedef void (*sighandler_t)(int);` clearer than raw function pointer syntax?</strong></summary>

Raw function pointer syntax requires intertwining the return type, pointer asterisk, and parameter types directly into the variable or parameter declaration. A `typedef` abstracts this into a clean, standalone type identifier (`sighandler_t`), allowing developers to declare variables, arrays, and return types as easily as primitive `int` variables.
</details>

<details>
<summary><strong>11. What is the difference between `typedef struct S S;` and `struct S { int x; };`?</strong></summary>

`struct S { ... }` introduces a **tag name** in the struct tag namespace, requiring the keyword `struct` whenever declaring variables (`struct S my_var;`). Adding `typedef struct S S;` aliases the tag into the ordinary identifier namespace, allowing concise variable declarations: `S my_var;`.
</details>

<details>
<summary><strong>12. How do you declare a function that returns an array of 5 integers in C?</strong></summary>

In standard C, **functions cannot return arrays directly**! C functions can return a struct containing an array, or return a pointer to an array (`int (*func(void))[5]`), but cannot return bare array types.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Parsing Declarations</strong></summary>

**Goal**: Decode the following declarations into plain English:
1. `double *(*arr[5])(void);`
2. `int (*calc)(int, int);`

**Solution**:
1. `arr` is an array of 5 pointers to functions taking void and returning a pointer to double (`double *`).
2. `calc` is a pointer to a function taking two ints and returning an int.
</details>

<details>
<summary><strong>Exercise 2 (Medium): Authoring a Clean Calculator Dispatch Table</strong></summary>

**Goal**: Author a typedef for a binary arithmetic function, and create a lookup table mapping character opcodes (`+`, `-`, `*`) to their respective functions.

**Solution**:
```c
#include <stdio.h>

typedef int (*op_func_t)(int, int);

static int add(int a, int b) { return a + b; }
static int sub(int a, int b) { return a - b; }
static int mul(int a, int b) { return a * b; }

typedef struct {
    char op;
    op_func_t func;
} OpEntry;

static const OpEntry dispatch_table[] = {
    { '+', add },
    { '-', sub },
    { '*', mul }
};

int execute_op(char op, int a, int b) {
    for (size_t i = 0; i < sizeof(dispatch_table)/sizeof(dispatch_table[0]); ++i) {
        if (dispatch_table[i].op == op) {
            return dispatch_table[i].func(a, b);
        }
    }
    return 0;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Emulating C23 `typeof` Generic Max Macro</strong></summary>

**Goal**: Implement a safe `MAX(a, b)` macro in C23 using `typeof_unqual` that evaluates arguments exactly once and handles mixed signs safely.

**Solution**:
```c
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define MAX(a, b) ({ \
    typeof_unqual(a) _a = (a); \
    typeof_unqual(b) _b = (b); \
    _a > _b ? _a : _b; \
})
#endif
```
</details>

---

## 12. Summary / Cheat Sheet

- **Declaration = Specifiers + Declarators**.
- The `*` binds to the identifier, not the type (`int *a, b;` means `b` is `int`).
- **Clockwise Rule**: Start at identifier; look right (arrays `[]`, functions `()`), then left (`*`).
- `int (*p)[10]`: Pointer to array of 10 ints.
- `int *p[10]`: Array of 10 pointers to int.
- Use `typedef` to clean up function pointers and opaque handles.
- C23: Use `typeof()` and `typeof_unqual()` for compile-time type deduction.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 6.7: "Declarations".
- David Anderson: *The "Clockwise/Spiral Rule"* (1994).
- Peter van der Linden: *Expert C Programming: Deep C Secrets* (Chapter 3: Unscrambling C Declarations).

---

[← Previous: 03-pointers.md](./03-pointers.md) | [Section Index](./README.md) | [Next: 05-type-qualifiers.md](./05-type-qualifiers.md) →
