# Preprocessor

The preprocessor runs before the compiler and does **text substitution** on your source: it includes files, expands macros, and conditionally removes code. It knows nothing about C types or syntax, which makes it powerful and easy to misuse.

**Prerequisites:** [Compilation process](../00-setup/03-compilation-process.md).

Inspect its output at any time with `gcc -E file.c`.

## `#include`

```c
#include <stdio.h>        /* search system include directories */
#include "mylib.h"        /* search the current file's directory first, then the system paths */
```

Use `<>` for system and third-party headers, `""` for your project's own. Add search directories with `-Iinclude`.

### Include guards

A header included twice would redefine its types. Guard against it:

```c
/* vec.h */
#ifndef VEC_H
#define VEC_H

/* declarations */

#endif /* VEC_H */
```

`#pragma once` does the same thing and is supported by all major compilers, but it is not part of the standard. Make guard names unique (include the project name for libraries).

## Object-like macros

```c
#define BUFFER_SIZE 256
#define PI 3.14159265358979
```

Prefer `enum` or `const` (or `constexpr` in C23) for typed constants; see [variables and constants](../01-fundamentals/04-variables-constants.md). Macros have no scope and no type.

## Function-like macros

```c
#define MAX(a, b) ((a) > (b) ? (a) : (b))
```

Two rules: **parenthesize every parameter** and the whole body, and remember that arguments are **evaluated each time they appear**:

```c
#define SQUARE(x) x * x
SQUARE(1 + 2)          /* 1 + 2 * 1 + 2 = 5 */

MAX(i++, j)            /* i++ may be evaluated twice */
```

Prefer a `static inline` function: it is type-checked, evaluates its arguments once, and is debuggable.

### Multi-statement macros: `do { } while (0)`

```c
#define LOG_ERROR(msg) do {                    \
        fprintf(stderr, "error: %s\n", msg);   \
        errors++;                              \
    } while (0)

if (bad) LOG_ERROR("boom"); else ok();         /* works as one statement */
```

Without the `do/while(0)` wrapper, the second statement would escape an unbraced `if`.

## Stringification and token pasting

```c
#define STR(x)   #x                 /* turns the argument into a string literal */
#define CAT(a,b) a##b               /* joins two tokens */

STR(hello)                          /* "hello" */
CAT(var_, 1)                        /* var_1 */

#define CHECK(cond) \
    do { if (!(cond)) fprintf(stderr, "failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); } while (0)
```

To expand a macro argument first, use a two-level macro: `#define XSTR(x) STR(x)`.

## Variadic macros

```c
#define LOG(fmt, ...) fprintf(stderr, "[%s:%d] " fmt "\n", __FILE__, __LINE__, __VA_ARGS__)
```

This form fails when no extra arguments are given. GCC/Clang accept `##__VA_ARGS__` as an extension, and C23 adds `__VA_OPT__(,)` for the portable case.

## Conditional compilation

```c
#ifdef DEBUG
    fprintf(stderr, "x = %d\n", x);
#endif

#if defined(__linux__)
    /* Linux-specific */
#elif defined(_WIN32)
    /* Windows-specific */
#else
#   error "Unsupported platform"
#endif

#if __STDC_VERSION__ >= 201112L
    /* C11 or later */
#endif
```

Set macros from the command line: `gcc -DDEBUG -DVERSION=\"1.2\" ...`. Use conditional compilation for portability and feature switches, but sparingly: each `#if` multiplies the code paths you must test.

## Predefined macros

| Macro | Value |
|---|---|
| `__FILE__`, `__LINE__` | current file name, line number |
| `__func__` | current function name (C99; technically a predefined identifier, not a macro) |
| `__DATE__`, `__TIME__` | build date/time (hurts reproducible builds) |
| `__STDC_VERSION__` | language standard (see [standards](../00-setup/05-c-standards-history.md)) |
| `__GNUC__`, `__clang__`, `_MSC_VER`, `__linux__`, `_WIN32` | compiler / platform detection (non-standard but universal) |

## Other directives

- `#undef NAME`: remove a macro definition.
- `#error "msg"`: stop compilation with a message; useful for unsupported configurations.
- `#line`, `#pragma`: advanced and compiler-specific. See [advanced macros](../04-advanced/05-advanced-macros.md).

## Common mistakes

| Mistake | Effect |
|---|---|
| Missing parentheses in macro body or arguments | Operator-precedence bugs |
| Arguments with side effects (`MAX(i++, j)`) | Evaluated more than once |
| Trailing `;` in `#define X 5;` | Syntax errors where `X` is used |
| Missing include guard | Redefinition errors |
| Macro name that collides with a variable or function | Confusing errors; use `UPPER_CASE` |
| A space after the macro name in a function-like macro (`#define F (x)`) | Defines an object-like macro |
| Heavy macro use in place of functions or enums | Unreadable errors, no debugging |

## Debugging

- `gcc -E file.c | less` shows exactly what the compiler sees.
- `gcc -E -dM - < /dev/null` lists all predefined macros.
- Errors that point to a macro invocation usually come from its expansion; read the expanded form.

## Summary

- The preprocessor performs text substitution before compilation.
- Use include guards; parenthesize macro parameters; avoid side effects in arguments.
- Prefer `enum`, `const`, and `static inline` over macros where possible.
- Use `#if` for platform and feature selection; inspect results with `-E`.

**Next:** [Command-line arguments](./10-command-line-arguments.md)
