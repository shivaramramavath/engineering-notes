# Integer Representation

Integers in C are fixed-width bit patterns, and what happens at the edges of their range depends on whether they are signed or unsigned. Unsigned overflow wraps around; signed overflow is **undefined behavior**. This difference causes a large share of real-world C bugs and security vulnerabilities.

**Prerequisites:** [Data types](./02-data-types.md).

## Unsigned integers

An `N`-bit unsigned integer holds `0 .. 2^N - 1`. Arithmetic is performed modulo `2^N`, which is well defined and guaranteed by the standard.

```c
#include <stdint.h>

uint8_t x = 255;
x = x + 1;          /* x is 0: wraps around */
uint32_t y = 0;
y = y - 1;          /* y is 4294967295 (UINT32_MAX) */
```

Wraparound is useful for hashes and checksums but dangerous for sizes and counters.

## Signed integers: two's complement

Virtually all machines store signed integers in **two's complement**: the top bit has weight `-2^(N-1)`. C23 makes two's complement mandatory; earlier standards also allowed other representations, which no mainstream hardware uses.

```text
8-bit values:
  0111 1111  =  127   (INT8_MAX)
  0000 0001  =    1
  0000 0000  =    0
  1111 1111  =   -1
  1000 0000  = -128   (INT8_MIN)
```

To negate: invert all bits and add 1. The range is asymmetric: there is one more negative value than positive, so `-INT_MIN` cannot be represented.

## Signed overflow is undefined behavior

```c
int i = INT_MAX;
i = i + 1;            /* undefined behavior, NOT a guaranteed wrap */
```

"Undefined" means the compiler may assume it never happens. Optimizers use this, for example:

```c
if (x + 1 > x) { ... }   /* compiler may treat this as always true */
```

The same applies to `abs(INT_MIN)` and `INT_MIN / -1`. See [undefined behavior](../04-advanced/01-undefined-behavior.md). Build with `-fsanitize=undefined` to catch it while testing.

### Checking for overflow before it happens

```c
#include <limits.h>
#include <stdbool.h>

bool add_ok(int a, int b) {
    if (b > 0 && a > INT_MAX - b) return false;
    if (b < 0 && a < INT_MIN - b) return false;
    return true;
}
```

Always test **before** the operation. Checking the result afterward doesn't work for signed types because the operation has already invoked undefined behavior. C23 adds `<stdckdint.h>` (`ckd_add`, `ckd_sub`, `ckd_mul`) that report overflow directly, and GCC/Clang provide `__builtin_add_overflow` as an extension.

## Bit patterns and shifts

```c
#include <stdint.h>
#include <stdio.h>

void print_bits(uint8_t v) {
    for (int i = 7; i >= 0; i--) putchar(((v >> i) & 1) ? '1' : '0');
    putchar('\n');
}
```

Shift rules:

| Operation | Behavior |
|---|---|
| `x << n` or `x >> n` with `n < 0` or `n >=` width of the promoted type | undefined |
| Left shift of a negative signed value | undefined |
| Left shift of a signed value into or past the sign bit | undefined |
| Right shift of a negative signed value | implementation-defined (usually arithmetic, sign-preserving) |
| Right shift of an unsigned value | zero-fills |

Do bit manipulation on **unsigned** types. See [bit manipulation](../03-intermediate/05-bit-manipulation.md).

## Endianness

Endianness is the byte order of multi-byte values in memory. It matters when you read and write binary files, network data, or inspect memory.

```text
uint32_t v = 0x11223344;

little-endian (x86, most ARM):  addr+0: 44 33 22 11
big-endian (network order):     addr+0: 11 22 33 44
```

```c
#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint32_t x = 1;
    unsigned char *p = (unsigned char *)&x;   /* inspecting bytes via unsigned char is allowed */
    puts(*p == 1 ? "little-endian" : "big-endian");
    return 0;
}
```

For portable binary formats, don't copy structs to files. Serialize each field explicitly with shifts, or use `htonl`/`ntohl` (POSIX) for network byte order.

## Common mistakes

- Relying on signed overflow to wrap. Use unsigned arithmetic or check first.
- Using signed `int` for sizes and then multiplying: `int n = w * h;` can overflow before being used to allocate memory.
- Shifting by an amount equal to or larger than the type width.
- Using `abs()` on user-controlled input without handling `INT_MIN`.
- Assuming byte order when parsing binary data.

## Summary

- Unsigned arithmetic wraps modulo `2^N`; signed overflow is undefined.
- Signed values use two's complement; the range is asymmetric.
- Check for overflow before the operation, and do bit work on unsigned types.
- Endianness matters whenever bytes leave the program.

**Next:** [Variables and constants](./04-variables-constants.md)
