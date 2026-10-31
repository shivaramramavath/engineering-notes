# Arrays

An array is a fixed-size, contiguous block of elements of the same type. It is C's only built-in collection, and nearly everything else (strings, buffers, matrices, heap blocks) is built on it. C does not check bounds, so understanding arrays precisely is what separates working code from memory bugs.

**Prerequisites:** [Data types](../01-fundamentals/02-data-types.md), [functions](../01-fundamentals/08-functions.md).

## Declaring and initializing

```c
int a[5];                         /* 5 ints, uninitialized (if local) */
int b[5] = {1, 2, 3, 4, 5};
int c[5] = {1, 2};                /* {1, 2, 0, 0, 0}: the rest are zeroed */
int d[5] = {0};                   /* all zeros */
int e[] = {10, 20, 30};           /* size deduced: 3 */
int f[6] = { [1] = 7, [4] = 9 };  /* designated initializers (C99) */
```

Indexes run from `0` to `n - 1`. Elements are laid out back to back:

```text
int b[5]   (4-byte ints)

addr:   +0   +4   +8   +12  +16
       [ 1 ][ 2 ][ 3 ][ 4 ][ 5 ]
        b[0] b[1] b[2] b[3] b[4]
```

## No bounds checking

```c
int a[3] = {1, 2, 3};
a[3] = 99;        /* out of bounds: undefined behavior, compiles without error */
int x = a[-1];    /* also undefined */
```

The program may crash, silently corrupt a neighboring variable, or appear to work. This is the root of buffer overflow vulnerabilities. Always keep the length next to the array and check indexes that come from outside. Use `-fsanitize=address` to catch these at runtime. See [sanitizers](../06-debugging-testing/04-sanitizers-valgrind.md).

## Length with `sizeof`

```c
int a[] = {4, 8, 15, 16, 23, 42};
size_t n = sizeof a / sizeof a[0];     /* 6 */
```

This works **only where the compiler still sees the array type**, not on a pointer or a function parameter (see below). A common macro form: `#define COUNT(x) (sizeof (x) / sizeof (x)[0])`.

## Arrays decay to pointers

In most expressions, an array's name becomes a pointer to its first element:

```c
int a[4];
int *p = a;        /* same as &a[0] */
a[2] == *(a + 2);  /* indexing is pointer arithmetic */
```

The exceptions: when the array is the operand of `sizeof` or `&`, or is a string literal initializing a `char` array. Everywhere else it decays.

**An array is not a pointer.** `sizeof a` is the whole array, `sizeof p` is the pointer size, and you can't assign to an array name. See [pointers](./03-pointers.md).

### Passing arrays to functions

```c
void print(const int *a, size_t n);        /* all three declarations are the same: */
void print2(const int a[], size_t n);      /*   `a` is a pointer, not an array      */
void print3(const int a[10], size_t n);    /*   the 10 is ignored                   */
```

Inside the function `sizeof a` is the size of a pointer. **Pass the length separately.** Arrays are passed by reference in effect: the function works on the caller's elements.

## Arrays are not assignable or comparable

```c
int a[3] = {1, 2, 3}, b[3];
b = a;                          /* error */
if (a == b) { ... }             /* compares addresses, always false here */

memcpy(b, a, sizeof a);         /* copy elements */
memcmp(a, b, sizeof a) == 0;    /* compare bytes (fine for plain integers) */
```

A `struct` containing an array **is** copyable by assignment, which is a handy workaround.

## Multidimensional arrays

```c
int m[2][3] = { {1, 2, 3}, {4, 5, 6} };
m[1][2];                       /* 6 */
```

Stored in **row-major** order, as one contiguous block:

```text
m[0][0] m[0][1] m[0][2] m[1][0] m[1][1] m[1][2]
   1       2       3       4       5       6
```

Iterate with the **last** index innermost for cache-friendly access.

Passing one: every dimension except the first must be given.

```c
void print_matrix(size_t rows, int m[][3]) { ... }
```

An `int **` is **not** a 2D array; it's a pointer to pointers with different memory layout. Don't pass `int m[2][3]` to a function expecting `int **`.

## Variable length arrays (VLAs)

```c
void f(size_t n) {
    int tmp[n];                /* size decided at runtime, on the stack */
}
```

VLAs were added in C99 and became **optional** in C11. They cannot be initialized, can't have static storage, and a large `n` overflows the stack with no way to detect it. Prefer `malloc` for runtime-sized data. See [memory management](./08-memory-management.md).

## Arrays of structs and strings

```c
struct point { int x, y; };
struct point pts[] = { {0, 0}, {1, 2}, {3, 4} };

const char *names[] = { "alice", "bob", "carol" };   /* array of pointers to literals */
```

## Common mistakes

| Mistake | Effect |
|---|---|
| `i <= n` when looping | Off-by-one write or read past the end |
| `sizeof` on a parameter | Gives pointer size, not element count |
| `b = a` or `a == b` on arrays | Compile error or compares addresses |
| Returning a local array | Dangling pointer; see [functions](../01-fundamentals/08-functions.md) |
| Large local arrays | Stack overflow; use the heap for big buffers |
| Uninitialized local array | Reading garbage |
| Treating `int **` as `int [][N]` | Wrong memory layout, crash |

## Summary

- Fixed size, contiguous, zero-based, **no bounds checking**.
- Arrays decay to pointers; always pass the length alongside.
- Use `memcpy` / `memcmp` to copy and compare; arrays aren't assignable.
- Prefer heap allocation to VLAs for large or runtime-sized data.

**Next:** [Strings](./02-strings.md)
