# Type Conversions

C converts between types automatically when you mix them in an expression, assign, or pass arguments. These implicit conversions are convenient but are the source of many subtle bugs, especially around signed and unsigned integers.

**Prerequisites:** [Data types](./02-data-types.md), [integer representation](./03-integer-representation.md), [operators](./05-operators.md).

## Integer promotions

Before arithmetic, any integer type narrower than `int` (`char`, `short`, `bool`, bit-fields) is promoted to `int` (or `unsigned int` if `int` can't hold all its values).

```c
uint8_t a = 200, b = 100;
int sum = a + b;     /* 300: computed as int, no wraparound */
uint8_t c = a + b;   /* 300 converted back to uint8_t: 44 */
```

A consequence: `~a` on a `uint8_t` is computed in `int`, so `(uint8_t)~a` needs the cast back if you want 8 bits.

## Usual arithmetic conversions

When a binary operator has operands of different types, they are converted to a common type:

```text
1. If either is long double → long double
2. else if either is double → double
3. else if either is float  → float
4. else (both integers) apply promotions, then:
   - same signedness: convert to the wider type
   - unsigned has rank >= signed: convert the signed one to unsigned
   - signed type can represent all values of the unsigned type: convert to signed
   - otherwise: convert both to the unsigned version of the signed type
```

The practical rule: **when signed and unsigned of the same width meet, the signed value becomes unsigned.**

## The signed/unsigned trap

```c
int a = -1;
unsigned int b = 1;

if (a < b) puts("less");
else       puts("not less");     /* prints "not less": a becomes 4294967295 */
```

The same issue appears with `size_t`:

```c
int n = -1;
if (n < sizeof(int)) { ... }     /* sizeof is size_t (unsigned); n becomes huge */

/* Infinite loop: i is unsigned, so i >= 0 is always true */
for (size_t i = len - 1; i >= 0; i--) { ... }

/* Correct countdown */
for (size_t i = len; i-- > 0; ) { ... }
```

Enable `-Wsign-compare` (included in `-Wall` for C++, but in `-Wextra` for C) to catch comparisons. Prefer one signedness for related values, and validate before converting a signed value to `size_t`.

## Conversions on assignment and return

The right-hand side is converted to the type of the destination.

| From → To | Result |
|---|---|
| Wider integer → narrower | Unsigned target: value modulo `2^N`. Signed target: implementation-defined if it doesn't fit |
| Signed → unsigned of same width | Value modulo `2^N` (`-1` → max value) |
| Floating → integer | Fractional part discarded (truncation toward zero); **undefined if the result doesn't fit** |
| Integer → floating | Rounded if it can't be represented exactly (e.g., large `int` → `float`) |
| `double` → `float` | Rounded; undefined if out of range (on IEEE 754, may become infinity) |

```c
int i = 3.9;            /* 3 */
int j = -3.9;           /* -3 */
float f = 16777217;     /* 16777216: float cannot hold 2^24 + 1 exactly */
unsigned char c = 300;  /* 44 */
```

## Widening happens after the operation

```c
int w = 100000, h = 100000;
long long area = w * h;          /* overflow in int FIRST, then converted */
long long area_ok = (long long)w * h;   /* convert first, then multiply */
```

Likewise `double ratio = 1 / 2;` gives `0.0`, because `1 / 2` is integer division. Cast an operand *before* the operation.

## Explicit casts

```c
double avg = (double)total / count;
uint8_t low = (uint8_t)(value & 0xFF);
```

A cast silences the compiler, so use it only when you've decided the conversion is correct. Casts that hide a warning often hide the bug with it.

Pointer casts (`(char *)p`, `(void *)p`) are separate and covered in [pointers](../02-core-concepts/03-pointers.md). Casting a pointer to an unrelated type and dereferencing it violates aliasing rules and is undefined.

## `char` and the `ctype.h` functions

`isalpha`, `toupper` and friends take an `int` that must be either `EOF` or a value representable as `unsigned char`. Passing a negative `char` is undefined.

```c
#include <ctype.h>
if (isdigit((unsigned char)c)) { ... }
```

## Common mistakes

- Comparing signed with unsigned, especially `int` vs `size_t`.
- Reverse loops with an unsigned counter and `>= 0`.
- Multiplying `int`s and storing into a wider type, expecting the wider math.
- Truncating integer division when a fraction was intended.
- Storing `fgetc()`/`getchar()` results in `char` instead of `int`; EOF can't be distinguished. See [input/output](./09-input-output.md).

## Debugging

- `-Wall -Wextra -Wconversion -Wsign-conversion` highlights risky implicit conversions. It is noisy, so enable it on new code first.
- `-fsanitize=undefined` catches some of these (signed overflow, out-of-range float-to-int); `-fsanitize=integer` in Clang adds unsigned wrap and truncation checks.

## Summary

- Small integer types are promoted to `int` before arithmetic.
- Signed + unsigned of equal rank → both treated as unsigned.
- Convert operands **before** the operation if you need wider or floating math.
- Use casts deliberately; heed `-Wsign-compare` and `-Wconversion`.

**Next:** [Control flow](./07-control-flow.md)
