# Bit Manipulation

Bit operations let you pack flags, parse binary formats, talk to hardware, and write fast low-level code. The operators are simple; the bugs come from signedness, shift widths, and precedence.

**Prerequisites:** [Integer representation](../01-fundamentals/03-integer-representation.md), [operators](../01-fundamentals/05-operators.md).

## Ground rules

- Do bit work on **unsigned** types. Shifting negative or overflowing signed values is undefined or implementation-defined.
- Use the right literal width: `1` is a 32-bit `int`; `1u << 31` is fine but `1 << 31` is undefined; for 64-bit use `1ULL << n`.
- Shift counts must be `>= 0` and **less than the bit width** of the promoted left operand.
- Parenthesize: `==` binds tighter than `&`, `^`, `|`.
- Use fixed-width types (`uint32_t`) from `<stdint.h>` when the width matters.

## The core operations

```c
#include <stdint.h>

x |=  (1u << n);          /* set bit n */
x &= ~(1u << n);          /* clear bit n */
x ^=  (1u << n);          /* toggle bit n */
int on = (x >> n) & 1u;   /* test bit n (0 or 1) */
```

Bit positions count from 0 at the least significant bit.

```text
x = 0b1011 0100
bit:   7654 3210
set bit 0 →  1011 0101
clear bit 2 → 1011 0000
```

### Masks and fields

```c
#define MASK(width)        ((1u << (width)) - 1u)     /* width < 32 */

uint32_t get_field(uint32_t v, unsigned shift, unsigned width) {
    return (v >> shift) & MASK(width);
}

uint32_t set_field(uint32_t v, unsigned shift, unsigned width, uint32_t val) {
    uint32_t m = MASK(width) << shift;
    return (v & ~m) | ((val << shift) & m);
}
```

Example: packing a date into 16 bits: `year(7) | month(4) | day(5)`. Use these helpers instead of bit-fields, whose layout is implementation-defined. See [structs](../02-core-concepts/06-structs-unions-enums.md).

## Useful idioms (unsigned `x`)

| Goal | Expression | Notes |
|---|---|---|
| Is a power of two? | `x != 0 && (x & (x - 1)) == 0` | `x & (x-1)` clears the lowest set bit |
| Isolate lowest set bit | `x & (0u - x)` | two's complement trick |
| Clear lowest set bit | `x & (x - 1)` | |
| Is odd? | `x & 1u` | |
| Round up to a multiple of `a` (power of two) | `(x + a - 1) & ~(a - 1)` | beware overflow of `x + a - 1` |
| Divide / multiply by `2^n` | `x >> n`, `x << n` | compilers already do this for you |
| Swap without temp | `a ^= b; b ^= a; a ^= b;` | a curiosity: breaks if `a` and `b` alias; not faster |

Don't hand-optimize `x * 8` or `x / 2`; write what you mean and let the optimizer work.

## Counting and searching bits

```c
/* Portable: Kernighan's loop, one iteration per set bit */
int popcount(uint32_t x) {
    int n = 0;
    while (x) { x &= x - 1; n++; }
    return n;
}
```

GCC and Clang provide builtins that compile to single instructions where available:

```c
__builtin_popcount(x)     /* number of set bits (unsigned int) */
__builtin_clz(x)          /* leading zeros; undefined for x == 0 */
__builtin_ctz(x)          /* trailing zeros; undefined for x == 0 */
```

C23 standardizes these in `<stdbit.h>` (`stdc_count_ones`, `stdc_leading_zeros`, `stdc_trailing_zeros`, ...), where supported by your compiler and library.

## Rotation and byte swapping

```c
static inline uint32_t rotl32(uint32_t x, unsigned n) {
    n &= 31u;
    return n ? (x << n) | (x >> (32u - n)) : x;     /* avoid shift by 32 */
}

static inline uint32_t bswap32(uint32_t x) {
    return (x >> 24) | ((x >> 8) & 0x0000FF00u) |
           ((x << 8) & 0x00FF0000u) | (x << 24);
}
```

Compilers recognize these patterns and emit native rotate and byte-swap instructions. For network order, prefer `htonl`/`ntohl` (POSIX).

## Reading binary data portably

Parse fields from bytes explicitly, independent of host endianness:

```c
uint32_t read_be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}
```

The cast to `uint32_t` *before* shifting matters: `p[0]` promotes to `int`, and `p[0] << 24` can overflow into the sign bit.

## Bit sets

An array of words can store thousands of flags compactly:

```c
#define BITSET_WORDS(n)  (((n) + 63) / 64)

static inline void bs_set(uint64_t *bs, size_t i)   { bs[i / 64] |=  (uint64_t)1 << (i % 64); }
static inline void bs_clear(uint64_t *bs, size_t i) { bs[i / 64] &= ~((uint64_t)1 << (i % 64)); }
static inline int  bs_test(const uint64_t *bs, size_t i) { return (bs[i / 64] >> (i % 64)) & 1u; }
```

Used for sieves, visited sets, permission masks, and allocator bitmaps.

## Flags

```c
enum { F_READ = 1u << 0, F_WRITE = 1u << 1, F_EXEC = 1u << 2 };
unsigned perms = F_READ | F_WRITE;
if (perms & F_WRITE) { /* ... */ }
```

## Common mistakes

| Mistake | Fix |
|---|---|
| `1 << 31` or `1 << n` with `n >= 32` | `1u << n` / `1ULL << n`, keep `n < width` |
| `if (x & MASK == 0)` | `if ((x & MASK) == 0)` |
| Shifting signed negatives | Convert to unsigned first |
| `p[0] << 24` on a `uint8_t` array | Cast to `uint32_t` first |
| Relying on bit-field layout | Use masks and shifts |
| `x & (1 << n)` returns `1<<n`, not 1 | Normalize with `!!` or `>> n & 1` |
| Using `~` on small types and expecting 8 bits | Result promotes to `int`; cast or mask back |

## Debugging

- Print values in binary and hex (`%08x`) while debugging; gdb: `p/t x` (binary), `p/x x` (hex).
- `-fsanitize=undefined` flags invalid shifts at runtime.

## Summary

- Use unsigned types, correct literal widths, and parenthesized expressions.
- `x & (x - 1)` clears the lowest set bit; it powers popcount and power-of-two tests.
- Parse binary data byte by byte; avoid bit-fields for external formats.
- Use compiler builtins or C23 `<stdbit.h>` for popcount, leading and trailing zeros.

**Next:** [Variadic functions](./06-variadic-functions.md)
