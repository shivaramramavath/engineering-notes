# Data Types

Every value in C has a type that fixes how many bytes it occupies and how those bytes are interpreted. Unlike many languages, C deliberately leaves the exact sizes of its basic types to the implementation. Writing portable, correct code means knowing what is guaranteed and what is not.

## The basic types

| Type | Typical use | Guaranteed minimum |
|---|---|---|
| `char` | a byte / character | 8 bits (`CHAR_BIT`), `sizeof(char)` is always 1 |
| `short` | small integers | 16 bits |
| `int` | default integer | 16 bits (32 on all mainstream platforms) |
| `long` | larger integers | 32 bits |
| `long long` | 64-bit integers (C99) | 64 bits |
| `float` | single-precision real | not specified by the language |
| `double` | double-precision real (default for real literals) | at least as precise as `float` |
| `long double` | extended precision | at least as precise as `double` |
| `bool` | true/false | C23 keyword; earlier via `<stdbool.h>` (`_Bool`) |

Integer types come in `signed` and `unsigned` forms. `int` means `signed int`. `unsigned` alone means `unsigned int`.

> **`char` signedness is implementation-defined.** Plain `char` is signed on x86 Linux and unsigned on many ARM targets. Use `signed char` / `unsigned char` (or `uint8_t`) when you mean a number, and plain `char` only for text.

## Sizes are not fixed

The standard guarantees only minimum ranges and the ordering `char ≤ short ≤ int ≤ long ≤ long long`. Real platforms use a few common layouts:

| Data model | `int` | `long` | `long long` | pointer | Used by |
|---|---|---|---|---|---|
| ILP32 | 32 | 32 | 64 | 32 | 32-bit Linux, Windows, ARM |
| LP64 | 32 | 64 | 64 | 64 | 64-bit Linux, macOS |
| LLP64 | 32 | 32 | 64 | 64 | 64-bit Windows |

So `long` is 64 bits on Linux but 32 bits on Windows. If you need an exact width, don't guess.

## Fixed-width and special types

```c
#include <stdint.h>
#include <stddef.h>

int32_t  a = -5;        /* exactly 32 bits, signed   */
uint8_t  b = 255;       /* exactly 8 bits, unsigned  */
uint64_t c = 1ULL << 40;
size_t   n = sizeof a;  /* unsigned type for sizes and indexes */
ptrdiff_t d = &c - &c;  /* signed difference of pointers */
```

- Use `<stdint.h>` types (`int32_t`, `uint16_t`, ...) for binary formats, protocols, and bit manipulation.
- Use `size_t` for object sizes, array lengths and indexes. It is what `sizeof`, `strlen` and `malloc` use.
- Use `int` for ordinary counters and small values; it is the "natural" fast type.

## Discovering limits

```c
#include <limits.h>
#include <float.h>
#include <stdio.h>

int main(void) {
    printf("int:   %d .. %d\n", INT_MIN, INT_MAX);
    printf("uint:  0 .. %u\n", UINT_MAX);
    printf("char bits: %d\n", CHAR_BIT);
    printf("double epsilon: %g\n", DBL_EPSILON);
    printf("sizeof(long) = %zu\n", sizeof(long));   /* %zu is for size_t */
    return 0;
}
```

`sizeof` is an operator evaluated at compile time (except for variable length arrays). It yields a `size_t`, so print it with `%zu`.

## Literals

| Literal | Type |
|---|---|
| `42` | `int` (or a larger type if it doesn't fit) |
| `42u`, `42UL`, `42LL`, `42ULL` | `unsigned`, `unsigned long`, `long long`, `unsigned long long` |
| `0x2A`, `052`, `0b101010` (C23, common extension earlier) | hex, octal, binary |
| `3.14` | `double` |
| `3.14f`, `3.14L` | `float`, `long double` |
| `'A'` | `int` in C (a character constant), not `char` |
| `"hi"` | array of `char` including the terminating `'\0'` |

A leading `0` means octal: `010` is 8, not 10.

## Floating point

Almost all platforms use IEEE 754 (`float` = 32 bits, `double` = 64 bits), though the core language does not require it. Floating-point numbers are binary approximations:

```c
double x = 0.1 + 0.2;
printf("%d\n", x == 0.3);            /* 0 (false) */
printf("%.17g\n", x);                /* 0.30000000000000004 */
```

Never compare floats with `==` unless you know the values are exact. Compare against a tolerance suited to the magnitude of your data, for example `fabs(a - b) <= 1e-9 * fmax(fabs(a), fabs(b))`. Division by zero for floats yields `inf` or `nan` on IEEE 754 systems rather than crashing.

## `void`

`void` means "no value": a function that returns nothing (`void f(void)`) or a generic pointer (`void *`, covered in [pointers](../02-core-concepts/03-pointers.md)). You cannot declare a variable of type `void`.

## Common mistakes

- Assuming `int` is 32 bits or `long` is 64 bits everywhere. Use fixed-width types when it matters.
- Using plain `char` for byte values and getting negative numbers. Use `unsigned char` or `uint8_t`.
- Printing with the wrong specifier (`%d` for a `long`, `%d` for `size_t`). This is undefined behavior; see [input/output](./09-input-output.md).
- Treating `'A'` as a `char` constant. It is an `int`, though that rarely matters.
- Comparing `double` values with `==`.

## Summary

- Sizes are implementation-defined; only minimum ranges are guaranteed.
- Use `<stdint.h>` for exact widths and `size_t` for sizes and indexes.
- Plain `char` signedness varies. Floating point is approximate.

**Next:** [Integer representation](./03-integer-representation.md)
