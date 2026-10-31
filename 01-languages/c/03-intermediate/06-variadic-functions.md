# Variadic Functions

A variadic function takes a variable number of arguments, like `printf`. `<stdarg.h>` lets you write your own, but the language provides **no type or count information**: your function must know both by convention, and mistakes are undefined behavior.

**Prerequisites:** [Functions](../01-fundamentals/08-functions.md), [type conversions](../01-fundamentals/06-type-conversions.md).

## Basic usage

```c
#include <stdarg.h>

int sum(int count, ...) {
    va_list ap;
    va_start(ap, count);              /* count is the last named parameter */

    int total = 0;
    for (int i = 0; i < count; i++) {
        total += va_arg(ap, int);     /* you must name the correct type */
    }

    va_end(ap);                       /* required cleanup */
    return total;
}

sum(3, 10, 20, 30);                   /* 60 */
```

| Macro | Purpose |
|---|---|
| `va_list ap` | state for walking the arguments |
| `va_start(ap, last)` | initialize; `last` is the final named parameter |
| `va_arg(ap, T)` | fetch the next argument as type `T` |
| `va_end(ap)` | clean up (always call it before returning) |
| `va_copy(dst, src)` | duplicate a `va_list` (C99) so you can traverse twice |

A variadic function needs at least one named parameter before `...` (C23 relaxes this, and `va_start` then needs no second argument).

## How the function knows what to read

The caller must communicate **how many** arguments there are and **what types** they have:

1. **Explicit count**, as above.
2. **Sentinel value** that ends the list: `join(sep, "a", "b", "c", (char *)NULL)`.
3. **Format string** that describes the arguments: how `printf` works.

None of these is checked by the compiler. Reading more arguments than were passed, or reading the wrong type, is undefined behavior.

## Default argument promotions

Arguments matching `...` undergo promotions, so you can't `va_arg` the narrow types:

| Passed as | Read with |
|---|---|
| `char`, `short`, `_Bool` | `int` |
| `float` | `double` |
| `unsigned char`, `unsigned short` | `int` (or `unsigned` depending on range; read as `int`) |

```c
va_arg(ap, char);      /* wrong: undefined */
va_arg(ap, int);       /* right */
va_arg(ap, float);     /* wrong */
va_arg(ap, double);    /* right */
```

Pointer arguments must be passed with the exact type you read. A bare `NULL` may be an `int 0`, so cast it: `(char *)NULL`.

## Wrapping `printf`: the `v` functions

To forward your arguments to `printf`-style functions, use their `v` versions, which take a `va_list`:

```c
#include <stdarg.h>
#include <stdio.h>

void log_msg(const char *level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s] ", level);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

log_msg("INFO", "loaded %d records from %s", n, path);
```

You can't pass `...` straight through to another variadic function; that's why `vprintf`, `vfprintf`, `vsnprintf` and `vsscanf` exist.

### Allocating the formatted result

`vsnprintf` returns the length the output needs, so format twice, using `va_copy` for the second pass:

```c
char *vformat(const char *fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);          /* measure */
    if (n < 0) { va_end(ap2); return NULL; }

    char *buf = malloc((size_t)n + 1);
    if (buf != NULL) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;                                    /* caller frees */
}
```

A `va_list` can be traversed only once; reusing it after `vsnprintf` without `va_copy` is undefined.

## Letting the compiler check format strings

GCC and Clang can validate calls to your printf-like functions:

```c
#if defined(__GNUC__)
#  define PRINTF_LIKE(fmt_idx, arg_idx) __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#  define PRINTF_LIKE(fmt_idx, arg_idx)
#endif

void log_msg(const char *level, const char *fmt, ...) PRINTF_LIKE(2, 3);
```

With `-Wformat` (part of `-Wall`), mismatched specifiers in `log_msg` calls become warnings. Always add this to logging wrappers.

## Alternatives

When the argument types are fixed, prefer a normal function taking an **array and length**, or a struct. Variadic macros (see [preprocessor](../02-core-concepts/09-preprocessor.md)) are handy for logging but have the same lack of type safety.

## Common mistakes

| Mistake | Effect |
|---|---|
| Reading more arguments than were passed | Undefined behavior; garbage or crash |
| `va_arg(ap, float)` / `char` / `short` | Undefined; use `double` / `int` |
| Passing an `int` where a `long` or pointer is read | Misaligned reads on some platforms |
| Missing `va_end` | Undefined on some ABIs |
| Reusing a `va_list` without `va_copy` | Undefined |
| Passing user-controlled text as the format string | Format-string vulnerability |
| Passing `NULL` sentinel without a cast | Reads wrong width |

## Summary

- `va_start` → `va_arg` (right type, right count) → `va_end`.
- Arguments are promoted: read `int` and `double`, not `char` or `float`.
- Forward with `vprintf` family; use `va_copy` to traverse twice.
- Add a `format` attribute to printf-like wrappers; prefer arrays or structs when you can.

**Next:** [Text processing and parsing](./07-text-processing-parsing.md)
