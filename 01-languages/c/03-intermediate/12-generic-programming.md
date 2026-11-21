# 12 Generic Programming

> Master type-generic programming in C using the C11 `_Generic` selection keyword, macro metaprogramming, `void *` generic containers, and type-safe wrappers.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [01-fundamentals/02-data-types.md](../01-fundamentals/02-data-types.md), [02-core-concepts/09-preprocessor.md](../02-core-concepts/09-preprocessor.md) |
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

C is a statically typed language that historically lacked native support for generic programming (such as C++ templates, Java generics, or Rust traits). As a consequence, C developers historically resorted to two extreme compromises:
1. **Writing Duplicate Code**: Authoring distinct functions for every data type (`vector_int`, `vector_float`, `vector_double`), cluttering codebases and complicating maintenance.
2. **`void *` Type Erasure**: Casting pointers to untyped `void *`, passing element size at runtime, and sacrificing all compile-time type safety.

The ISO C11 standard introduced the `_Generic` keyword, enabling compile-time type-based dispatch directly within the C language. By combining `_Generic` with preprocessor macros and inline wrapper functions, C developers can build expressive, high-performance, and type-safe APIs without runtime overhead.

---

## 2. Core Concepts

### 2.1 The `_Generic` Selection Keyword
The `_Generic` expression evaluates the static type of a controlling expression at compile-time and selects an associated expression:
```c
_Generic(controlling_expression,
    type_1: expression_1,
    type_2: expression_2,
    default: default_expression
)
```
- The controlling expression is **not evaluated at runtime**. It is analyzed purely by the compiler for its static semantic type. Side-effects (such as `_Generic(x++, ...)`) do not execute.
- Type matching is exact: `char *` does not match `const char *`, and `int` does not match `long` even if they have the same bit width on the target platform.
- The `default` branch is optional; if omitted and no matching type is found, compilation fails with an error.

### 2.2 Qualified Types and Conversions
In `_Generic`:
- Lvalue conversions are applied to the controlling expression: array types decay to pointer types, and function types decay to function pointers.
- Type qualifiers on the controlling expression are stripped: `const int` matches `int`.
- Pointer targets retain their qualifiers: `const int *` matches `const int *`, **not** `int *`.

### 2.3 The Three Patterns of C Generic Programming
1. **Compile-Time Dispatch (`_Generic`)**: Selects different function names or constants based on operand type. Emits direct assembly calls without indirect jump penalties.
2. **Untyped Type Erasure (`void *` + `size_t element_size`)**: Used by standard functions like `qsort` and generic collection libraries. Enables runtime polymorphism at the expense of casting discipline.
3. **Macro Template Generation**: Generating type-specific structs and functions using token concatenation (`##`) and multi-line macros (e.g. `DEFINE_VECTOR(int)`). Provides optimal performance and strict type checking, but increases executable binary footprint.

### 2.4 Metaprogramming with Macro Expansion and Token Pasting
Before C11 `_Generic`, the canonical idiom for generic data structures in C was macro-instantiated container templates:
```c
#define DECLARE_STACK(type, name) \
    typedef struct { \
        type *data; \
        size_t size; \
        size_t capacity; \
    } name; \
    name *name##_create(size_t cap); \
    void name##_push(name *s, type val); \
    type name##_pop(name *s);
```
While effective, this technique produced substantial code bloat, difficult debugging sessions with macro expansions, and confusing compiler diagnostics. `_Generic` allows developers to unify these interfaces under a single, readable function call interface.

---

## 3. How It Works Internally

### Compile-Time AST Resolution
Unlike C++ templates which instantiate new compiled functions on demand (mangling symbol names), `_Generic` is purely an AST-level compile-time selector:

```
Compilation Pipeline of _Generic:
Macro Call: print_val(x)
      │
      ▼ (Preprocessor expansion)
_Generic((x), int: print_int, double: print_double, default: print_str)(x)
      │
      ▼ (Compiler Semantic Analysis: static type of 'x' is 'double')
print_double(x)
      │
      ▼ (Code Generation)
Single direct assembly call: call print_double
```

All non-selected branches are discarded by the compiler. However, **all expressions in all branches must be syntactically and semantically valid**, even for unselected branches!

---

## 4. Syntax & Usage

### 4.1 Type-Generic Mathematical Macro
Standard `<tgmath.h>` uses `_Generic` internally. You can construct custom generic math operations:
```c
#include <math.h>

#define fast_abs(x) _Generic((x), \
    int: abs, \
    long: labs, \
    long long: llabs, \
    float: fabsf, \
    double: fabs, \
    long double: fabsl \
)(x)
```

### 4.2 Type-Safe Vector Access Wrapper
```c
#define vector_push(vec, val) _Generic((val), \
    int: vector_push_int, \
    double: vector_push_double, \
    char *: vector_push_str \
)((vec), (val))
```

---

## 5. Practical Examples

### Comprehensive Example: C11 `_Generic` Type Inspector and Dispatcher
The following program demonstrates static type identification and dynamic dispatching to specialized printers using C11 `_Generic`.

```c
#include <stdio.h>
#include <stdbool.h>

/* Type-generic print macro using C11 _Generic */
#define type_name(x) _Generic((x), \
    _Bool: "bool", \
    char: "char", \
    signed char: "signed char", \
    unsigned char: "unsigned char", \
    short: "short", \
    unsigned short: "unsigned short", \
    int: "int", \
    unsigned int: "unsigned int", \
    long: "long", \
    unsigned long: "unsigned long", \
    long long: "long long", \
    unsigned long long: "unsigned long long", \
    float: "float", \
    double: "double", \
    char *: "char pointer", \
    const char *: "const char pointer", \
    default: "other type")

static void print_formatted_int(int x) { printf("INTEGER: %d\n", x); }
static void print_formatted_double(double x) { printf("DOUBLE: %.4f\n", x); }
static void print_formatted_str(const char *x) { printf("STRING: \"%s\"\n", x); }

#define print_val(x) _Generic((x), \
    int: print_formatted_int, \
    double: print_formatted_double, \
    float: print_formatted_double, \
    const char *: print_formatted_str, \
    char *: print_formatted_str \
)(x)

int main(void) {
    printf("=== C11 _Generic Type-Generic Programming ===\n");

    int i = 42;
    double d = 3.14159;
    const char *s = "Generic Macro";
    bool b = true;

    printf("Inspecting types:\n");
    printf("  i has type: %s\n", type_name(i));
    printf("  d has type: %s\n", type_name(d));
    printf("  s has type: %s\n", type_name(s));
    printf("  b has type: %s (value: %d)\n", type_name(b), (int)b);

    printf("\nDispatching print_val:\n  ");
    print_val(i);
    printf("  ");
    print_val(d);
    printf("  ");
    print_val(s);

    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 12-generic-programming.c -o 12-generic-programming.exe
./12-generic-programming.exe
```

```text
=== C11 _Generic Type-Generic Programming ===
Inspecting types:
  i has type: int
  d has type: double
  s has type: const char pointer
  b has type: bool (value: 1)

Dispatching print_val:
  INTEGER: 42
  DOUBLE: 3.1416
  STRING: "Generic Macro"
```

---

## 6. Best Practices

1. **Always Provide a Clear `default` Case**: If an unexpected type is passed, having `default: unhandled_type_error()` produces readable error messages rather than cryptic compiler syntax rejections.
2. **Match Function Pointers, Not Function Calls, Inside `_Generic`**: Write `_Generic(x, int: print_int, double: print_double)(x)` rather than `_Generic(x, int: print_int(x), double: print_double(x))`. Passing `x` after the selection ensures `x` is evaluated only once.
3. **Include Both `char *` and `const char *`**: Because literal strings like `"hello"` have type `char[]` which decays to `char *` in C (unlike C++ where it is `const char *`), always handle both mutable and const pointer types.
4. **Use Parentheses Around Controlling Expressions in Macros**: Wrap macro parameters: `#define foo(x) _Generic((x), ...)`.
5. **Combine `_Generic` with Static Assertions**: Validate sizes or constraints: `_Static_assert(sizeof(x) <= 8, "Unsupported payload size");`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| `_Generic(x, int: f(x), float: g(x))` | Both branches are validated by compiler! If `g(x)` cannot accept type of `x`, compilation fails even on unselected branch! | Dispatch to function pointer: `_Generic(x, int: f, float: g)(x)`. |
| Expecting runtime side-effects in `_Generic(x++, ...)` | The controlling expression is unevaluated at runtime; `x` is never incremented. | Perform increments outside the generic selection. |
| Forgetting `const` on pointer targets | `const int *` does not match `int *`. Compilation fails with no matching type error. | Provide explicit entries for `const` pointer variants. |
| Assuming `char` is signed or unsigned | In ISO C, `char`, `signed char`, and `unsigned char` are three distinct types! | Include entries for all three when handling byte streams. |
| Missing parentheses in macro arguments | Complex expressions like `a + b` can break precedence inside `_Generic`. | Wrap controlling expression in parentheses: `((x))`. |
| Type ambiguity between `int` and `long` | On 32-bit platforms where `sizeof(int) == sizeof(long)`, they are still distinct types in `_Generic`. | Explicitly provide branches for both `int` and `long`. |

---

## 8. Performance & Security Notes

- **Zero Runtime Overhead**: `_Generic` is evaluated strictly during compilation. The emitted assembly contains direct, non-polymorphic calls to the resolved functions, with zero indirect jump penalties or vtable overhead.
- **Type Safety Enforcement**: Unlike `void *` APIs where an incorrect pointer type is silently cast, `_Generic` enforces strict type checking at compile-time, eliminating type-confusion vulnerabilities.
- **Compiler Inlining**: Because the target function is statically known, the compiler can inline the dispatched function, enabling dead-code elimination and register optimization.

---

## 9. Comparison / When to Use What

| Technique | Type Safety | Runtime Overhead | Code Bloat | Ease of Debugging |
| :--- | :--- | :--- | :--- | :--- |
| **C11 `_Generic`** | High | Zero (Direct calls) | Low | High (Standard C symbol names) |
| **`void *` Type Erasure** | Low (Cast errors silent) | Low (Indirect function pointers) | Minimal | Moderate (Pointer debugging) |
| **Token-Pasting Macro Templates** | High | Zero | High (New code per type) | Low (Obscure macro stack traces) |
| **C++ Templates** | Very High | Zero | Moderate to High | High (Compiler type error pages) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Does the controlling expression in a C11 `_Generic` statement get executed at runtime?</strong></summary>

No. The controlling expression in `_Generic` is an unevaluated operand, similar to the operand of `sizeof`. The compiler analyzes its static semantic type during semantic analysis to determine which branch to select. Any runtime side-effects (such as `_Generic(n++, ...)` or `_Generic(printf("hi"), ...)`) will never be executed.
</details>

<details>
<summary><strong>2. Why does `_Generic(x, int: f(x), double: g(x))` often fail to compile when `x` is an integer?</strong></summary>

Even though `_Generic` only evaluates one branch at runtime, ISO C mandates that **all expressions in all branches must be semantically valid**. If `g` is defined as `void g(struct Complex c)`, and `x` is an integer, passing `int` to `g(x)` is a type violation. The compiler checks `g(x)` during AST semantic validation and emits a compile error, even though the branch is unselected. The correct idiom is `_Generic(x, int: f, double: g)(x)`, which selects the function pointer before invoking it.
</details>

<details>
<summary><strong>3. Are `char`, `signed char`, and `unsigned char` considered different types by `_Generic`?</strong></summary>

Yes. In the C type system, `char`, `signed char`, and `unsigned char` are three distinct types, even though `char` has the same representation and range as either `signed char` or `unsigned char` depending on the platform. A `_Generic` selector matching `signed char` will not match a plain `char`.
</details>

<details>
<summary><strong>4. How does `_Generic` handle array types?</strong></summary>

Before type matching occurs, lvalue conversion is performed on the controlling expression. Consequently, an expression of array type (such as `int arr[10]`) decays into a pointer of type `int *`. Therefore, `_Generic(arr, int*: 1, int[10]: 2)` will match `int *` (branch 1).
</details>

<details>
<summary><strong>5. Can `_Generic` distinguish between `const int *` and `int *`?</strong></summary>

Yes. While type qualifiers at the top level of the controlling expression are dropped (so `const int` matches `int`), type qualifiers on pointed-to types are preserved. Therefore, `const int *` and `int *` are distinct types, and `_Generic` can route them to different handlers.
</details>

<details>
<summary><strong>6. What happens if no type in a `_Generic` selection matches and there is no `default` branch?</strong></summary>

If the static type of the controlling expression does not match any type association in the generic association list and no `default` association is present, the program is ill-formed and the compiler will issue a compilation error.
</details>

<details>
<summary><strong>7. How is `<tgmath.h>` implemented in C11?</strong></summary>

In C99, type-generic math was implemented using proprietary compiler builtins. In C11, `<tgmath.h>` is implemented entirely using `_Generic` macros. For example, `sin(x)` is defined as a macro that selects `sinf(x)` if `x` is `float`, `sin(x)` if `x` is `double`, `sinl(x)` if `x` is `long double`, or complex equivalents `csin(x)` if `x` is complex.
</details>

<details>
<summary><strong>8. What are the limitations of `_Generic` compared to C++ function overloading?</strong></summary>

`_Generic` only allows dispatching based on a single expression's type. It cannot dispatch based on the types of multiple arguments simultaneously without nesting `_Generic` selections. Furthermore, it cannot infer return types or synthesize new types on demand like C++ templates; all dispatched functions must be explicitly declared and implemented beforehand.
</details>

---

## 11. Practice Exercises

### Easy: Type Identifier Macro
Write a macro `is_floating_point(x)` using `_Generic` that evaluates to `true` if `x` is `float`, `double`, or `long double`, and `false` otherwise.

<details>
<summary>Solution</summary>

```c
#include <stdbool.h>

#define is_floating_point(x) _Generic((x), \
    float: true, \
    double: true, \
    long double: true, \
    default: false \
)
```
</details>

### Medium: Generic Clamp Macro
Implement a type-generic clamp macro `clamp(x, min, max)` that safely clamps values of type `int`, `long`, or `double` without double evaluation.

<details>
<summary>Solution</summary>

```c
static inline int clamp_i(int x, int min, int max) {
    return x < min ? min : (x > max ? max : x);
}

static inline long clamp_l(long x, long min, long max) {
    return x < min ? min : (x > max ? max : x);
}

static inline double clamp_d(double x, double min, double max) {
    return x < min ? min : (x > max ? max : x);
}

#define clamp(x, min, max) _Generic((x), \
    int: clamp_i, \
    long: clamp_l, \
    double: clamp_d \
)((x), (min), (max))
```
</details>

### Hard: Multi-Argument Type-Generic Dispatcher
Write a macro `add_generic(a, b)` that supports adding two arguments of types `(int, int)` or `(double, double)` using nested `_Generic` selectors.

<details>
<summary>Solution</summary>

```c
static inline int add_ii(int a, int b) { return a + b; }
static inline double add_dd(double a, double b) { return a + b; }

#define add_generic(a, b) _Generic((a), \
    int: _Generic((b), int: add_ii), \
    double: _Generic((b), double: add_dd) \
)((a), (b))
```
</details>

---

## 12. Summary / Cheat Sheet

| Feature | Syntax | Behavior |
| :--- | :--- | :--- |
| Basic Selection | `_Generic((expr), T1: e1, T2: e2)` | Compile-time static type match |
| Default Case | `default: fallback_expr` | Selected when no type matches |
| Array Handling | `int a[5]` decays to `int *` | Lvalue conversion applies |
| Function Pointer Dispatch | `_Generic((x), int: f, double: g)(x)` | Evaluates `x` exactly once |
| Type Strictness | `char`, `signed char`, `unsigned char` | Evaluated as 3 distinct types |

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17) §6.5.1.1 (Generic selection).
- Jens Gustedt, *Modern C* (Manning) — Chapter 16: Type-Generic Programming.
- SEI CERT C Coding Standard: PRE31-C (Avoid side effects in arguments to unsafe macros).

---

[← Previous](11-standard-library.md) | [Section Index](README.md) | [Next →](13-opaque-pointers-and-adts.md)
