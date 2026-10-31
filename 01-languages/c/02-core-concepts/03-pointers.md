# Pointers

A pointer is a variable that holds the address of another object (or function). Pointers are how C passes data by reference, builds dynamic structures, and talks to hardware. They are also the main source of crashes and security bugs, so it pays to understand exactly what they are.

**Prerequisites:** [Arrays](./01-arrays.md), [functions](../01-fundamentals/08-functions.md).

## The basics

```c
int x = 42;
int *p = &x;        /* p holds the address of x */

printf("%d\n", *p); /* 42: dereference, read what p points to */
*p = 7;             /* x is now 7 */
```

- `&x`: address-of
- `*p`: dereference (follow the pointer)
- `int *p`: "p is a pointer to int"

```text
        address     value
x   :   0x1000  →   42
p   :   0x1008  →   0x1000   ──┐ points to
                                └──▶ x
```

Print an address with `%p` and a cast to `void *`: `printf("%p\n", (void *)p);`.

## NULL

`NULL` is a pointer value meaning "points to nothing" (C23 adds the keyword `nullptr`). Initialize pointers either to a valid address or to `NULL`, and check before dereferencing:

```c
int *p = NULL;
if (p != NULL) { *p = 1; }
```

Dereferencing `NULL` is undefined behavior; on most systems it crashes with a segmentation fault, which is the *good* outcome.

## Pointer arithmetic

Adding `n` to a pointer moves it by `n` **elements**, not bytes:

```c
int a[4] = {10, 20, 30, 40};
int *p = a;

*(p + 2);      /* 30, same as a[2] */
p++;           /* p now points to a[1]; address increased by sizeof(int) */
```

`a[i]` is defined as `*(a + i)`. Subtracting two pointers into the same array gives the element count as a `ptrdiff_t`.

Valid pointer arithmetic stays within an array, or one element past its end (which may be compared but not dereferenced). Going further is undefined, even without dereferencing.

`void *` has no element size, so arithmetic on it is not standard C (GCC allows it as an extension). Cast to `char *` to step by bytes.

## Pointers and functions

Pass a pointer so a function can modify the caller's variable:

```c
void swap(int *a, int *b) {
    int t = *a;
    *a = *b;
    *b = t;
}

swap(&x, &y);
```

To let a function change **a pointer itself**, pass a pointer to the pointer:

```c
void alloc_buffer(char **out, size_t n) {
    *out = malloc(n);          /* modifies the caller's pointer */
}

char *buf;
alloc_buffer(&buf, 64);
```

For functions that only read through a pointer, use `const`: `void print(const char *s)`. See [type qualifiers](./05-type-qualifiers.md).

## Pointers to structs

```c
struct point { int x, y; };
struct point pt = {1, 2};
struct point *pp = &pt;

pp->x = 10;          /* same as (*pp).x = 10; */
```

## `void *`: the generic pointer

`void *` can hold the address of any object and converts to and from other object pointer types without a cast in C:

```c
void *raw = malloc(100);
int *ints = raw;           /* no cast needed in C (but required in C++) */
```

You cannot dereference a `void *`. It is how `malloc`, `memcpy`, and `qsort` stay generic. See [generic programming](../04-advanced/04-generic-programming.md).

## Arrays and pointers: the actual relationship

- An array name usually **decays** to a pointer to its first element.
- Indexing and pointer arithmetic are equivalent.
- But an array is not a pointer: `sizeof` and `&` behave differently, and an array name can't be reassigned.

```c
int a[5];
int *p = a;
sizeof a;    /* 20 (5 ints) */
sizeof p;    /* 8 (a pointer, on a 64-bit system) */
```

## Pointer to pointer, and arrays of pointers

```c
char *names[] = { "ann", "bo" };     /* array of pointers */
char **pp = names;                   /* pointer to the first pointer */
printf("%s\n", pp[1]);               /* bo */
```

`argv` is exactly this type. See [command-line arguments](./10-command-line-arguments.md).

## Dangling, wild, and null pointers

| Kind | Cause | Result |
|---|---|---|
| Null | Not checked | Crash (usually) |
| Wild | Never initialized | Writes to a random address |
| Dangling | Object freed or out of scope | Reads garbage or corrupts data |

```c
int *bad(void) { int x = 1; return &x; }        /* dangling: x is gone */

int *p = malloc(sizeof *p);
free(p);
*p = 5;                                         /* use after free */
```

Habit: initialize pointers; set a pointer to `NULL` after `free`; never return addresses of locals.

## Aliasing and type punning

Reading an object through a pointer of an incompatible type (for example accessing an `int` through a `float *`) violates the **strict aliasing rule** and is undefined; optimizers rely on it. To inspect an object's bytes use `unsigned char *`, or copy with `memcpy`:

```c
float f = 1.5f;
uint32_t bits;
memcpy(&bits, &f, sizeof bits);    /* well-defined way to reinterpret */
```

## Comparing and misconceptions

- Only compare pointers with `==` / `!=`, or with `<` / `>` when they point into the same array.
- **"A pointer is just an integer."** Not guaranteed: pointer representation is implementation-defined, and converting between them is not portable (use `uintptr_t` if you must).
- **"Arrays and pointers are the same."** They are related by decay, not identical.
- **"Segfault means the bug is on that line."** The corruption may have happened much earlier.

## Common mistakes

| Mistake | Fix |
|---|---|
| Dereferencing without a `NULL` check | Check, or guarantee non-null by contract |
| `int *p; *p = 5;` | Point it at valid storage first |
| Returning `&local` | Return a value or heap memory |
| `sizeof(p)` to get array size | Track length separately |
| Pointer arithmetic past the array | Keep within bounds |
| `int* a, b;` | `b` is not a pointer |
| Mixing up `*p++` and `(*p)++` | `*p++` reads then moves the pointer; `(*p)++` increments the value |

## Debugging

- `gdb`: `print *p`, `print p`, `x/8xw p`, `info frame`; use `bt` after a crash.
- AddressSanitizer reports use-after-free, out-of-bounds, and use-after-return with a stack trace for the allocation and free.
- Compile with `-Wall -Wextra`; many uninitialized and dangling-pointer cases are flagged.

## Summary

- A pointer stores an address; `&` takes it, `*` follows it.
- Arithmetic is scaled by the element size; stay within the array.
- Pass pointers to modify caller data; pass `T **` to modify a pointer.
- Never dereference `NULL`, uninitialized, freed, or out-of-scope pointers.
- Arrays decay to pointers but are not pointers.

**Next:** [Declarations and typedef](./04-declarations-typedef.md)
