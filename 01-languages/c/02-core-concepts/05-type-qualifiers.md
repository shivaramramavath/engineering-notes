# Type Qualifiers

Qualifiers change how the compiler may treat an object: `const` (don't modify), `volatile` (don't optimize away accesses), `restrict` (no aliasing), and `_Atomic` (indivisible access). Using them correctly documents intent, enables optimization, and catches bugs.

**Prerequisites:** [Pointers](./03-pointers.md), [declarations](./04-declarations-typedef.md).

## `const`

`const` means "this object is not modified **through this name**".

```c
const int limit = 10;
limit = 20;                 /* compile error */
```

### `const` and pointers

Read the declaration right to left (or see [declarations](./04-declarations-typedef.md)):

```c
const int *p;          /* pointer to const int: can't change *p, can change p */
int const *p;          /* same thing */
int *const p = &x;     /* const pointer to int: can change *p, can't change p */
const int *const p;    /* neither can change */
```

A rule of thumb: `const` applies to what is **immediately to its left** (or to its right if nothing is on the left).

### Const-correctness

Mark parameters that you only read:

```c
size_t count_chars(const char *s, char c);     /* promises not to modify *s */
```

Benefits: callers can pass string literals and const data; the compiler catches accidental writes; the interface documents itself. Passing a `T *` where `const T *` is expected is fine; the reverse is a warning.

```c
void f(const char *s) {
    char *w = (char *)s;     /* cast away const */
    w[0] = 'x';              /* undefined behavior if the original object was const */
}
```

Casting away `const` is only valid when the underlying object was not defined `const`. String literals should be treated as const.

> **Quirk:** `strchr`, `strstr` and similar take `const char *` but return `char *`, so they can silently strip const. C23 improves this with type-generic versions.

### `const` is not a compile-time constant

In C, `const int n = 5;` can't be used as a `case` label or file-scope array size. See [variables and constants](../01-fundamentals/04-variables-constants.md).

### Where `const` data lives

Global `const` objects are often placed in read-only memory, so writing to them crashes. Local `const` variables are still stack variables.

## `volatile`

`volatile` tells the compiler that an object's value can change in ways it can't see, so **every read and write must actually happen**, in order, and not be cached or removed.

Legitimate uses:

```c
volatile uint32_t *const UART_STATUS = (volatile uint32_t *)0x40001000;   /* memory-mapped I/O */
while ((*UART_STATUS & 0x1) == 0) { /* wait */ }                         /* re-read each time */

volatile sig_atomic_t got_signal = 0;                                     /* set in a signal handler */
void handler(int sig) { got_signal = 1; }
```

Without `volatile`, an optimizer may hoist the load out of the loop and spin forever.

**What `volatile` is not:**

- It does **not** make operations atomic.
- It does **not** provide thread synchronization or memory ordering.
- For sharing data between threads, use `_Atomic` / `<stdatomic.h>` or mutexes. See [synchronization](../04-advanced/08-concurrency/02-synchronization.md).

See also [embedded C](../04-advanced/10-embedded-c.md).

## `restrict`

`restrict` (C99) applies to pointers. It is a **promise** from you to the compiler: during the lifetime of this pointer, the object it points to is accessed only through this pointer (or pointers derived from it).

```c
void add(int n, int *restrict out, const int *restrict a, const int *restrict b) {
    for (int i = 0; i < n; i++) out[i] = a[i] + b[i];
}
```

Knowing that `out`, `a` and `b` don't overlap, the compiler can vectorize and reorder freely. If they do overlap, the behavior is **undefined**.

This is the difference between `memcpy` (parameters are `restrict`, regions must not overlap) and `memmove` (overlap allowed). Use `restrict` only for hot paths where you can guarantee no aliasing.

## `_Atomic`

`_Atomic int counter;` declares an object whose reads and writes are indivisible and ordered (C11). `<stdatomic.h>` provides `atomic_int`, `atomic_fetch_add`, and so on. Covered in [atomics and memory model](../04-advanced/08-concurrency/03-atomics-memory-model.md). Support is an optional feature (`__STDC_NO_ATOMICS__`).

## Combining qualifiers

```c
const volatile uint32_t *const STATUS_REG;
/* read-only hardware register: value may change (volatile), you must not write it (const) */
```

## Common mistakes

| Mistake | Effect |
|---|---|
| Omitting `const` on read-only pointer params | Callers can't pass const data; no protection |
| Casting away `const` and writing | Undefined behavior |
| Confusing `const char *` with `char *const` | Wrong thing is protected |
| Using `volatile` for thread synchronization | Data races remain |
| Using `restrict` on overlapping pointers | Undefined behavior, hard to debug |
| Forgetting `volatile` on a register or flag changed externally | Optimizer removes reads; loop never exits |

## Debugging

- If a variable "never changes" in an optimized build but does in `-O0`, suspect a missing `volatile` (or a data race).
- Enable `-Wcast-qual` to find casts that drop qualifiers; `-Wwrite-strings` warns on literals assigned to `char *`.

## Summary

- `const`: read-only through that name; put it on every pointer parameter you don't modify.
- `volatile`: for hardware registers and signal flags only, not for threading.
- `restrict`: a no-alias promise that enables optimization; breaking it is undefined.
- `_Atomic`: for lock-free shared variables.

**Next:** [Structs, unions, and enums](./06-structs-unions-enums.md)
