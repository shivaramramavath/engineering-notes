# Function Pointers

A function pointer holds the address of a function, so code can choose *which* function to call at runtime. They are how C does callbacks, plugin tables, state machines, and generic algorithms like `qsort`.

**Prerequisites:** [Pointers](../02-core-concepts/03-pointers.md), [declarations and typedef](../02-core-concepts/04-declarations-typedef.md), [functions](../01-fundamentals/08-functions.md).

## Syntax

```c
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

int (*op)(int, int);      /* op: pointer to function (int, int) -> int */
op = add;                 /* function name decays to its address; &add is equivalent */
int r = op(3, 4);         /* 7; (*op)(3, 4) is equivalent */
op = sub;
r = op(3, 4);             /* -1 */
```

The parentheses in `int (*op)(int, int)` matter: `int *op(int, int)` declares a function returning `int *`.

Use a `typedef` to keep declarations readable:

```c
typedef int (*binop_fn)(int, int);

binop_fn pick(char c) { return c == '+' ? add : sub; }
```

## Callbacks

The caller supplies behavior; the library supplies the algorithm.

```c
void for_each(const int *a, size_t n, void (*fn)(int, void *), void *ctx) {
    for (size_t i = 0; i < n; i++) fn(a[i], ctx);
}

static void accumulate(int v, void *ctx) { *(long *)ctx += v; }

long total = 0;
for_each(data, len, accumulate, &total);
```

C has no closures, so callbacks conventionally take a **`void *ctx`** (user data) that is passed back unchanged. Always include one in your own callback APIs; it saves you from global variables.

## The standard library: `qsort` and `bsearch`

```c
#include <stdlib.h>

static int cmp_int(const void *pa, const void *pb) {
    int a = *(const int *)pa, b = *(const int *)pb;
    return (a > b) - (a < b);          /* -1, 0, or 1; never a - b (can overflow) */
}

qsort(arr, n, sizeof arr[0], cmp_int);
int key = 42;
int *hit = bsearch(&key, arr, n, sizeof arr[0], cmp_int);   /* array must be sorted */
```

Sorting an array of strings: elements are `char *`, so the comparator receives `char **`:

```c
static int cmp_str(const void *pa, const void *pb) {
    const char *a = *(const char *const *)pa;
    const char *b = *(const char *const *)pb;
    return strcmp(a, b);
}
```

The comparator must define a consistent total order; an inconsistent one is undefined behavior. `qsort` is not guaranteed to be stable.

## Dispatch tables

An array of function pointers replaces long `switch` chains:

```c
typedef struct {
    const char *name;
    int (*run)(int argc, char **argv);
} command_t;

static int cmd_add(int argc, char **argv);
static int cmd_list(int argc, char **argv);

static const command_t commands[] = {
    { "add",  cmd_add  },
    { "list", cmd_list },
};

for (size_t i = 0; i < sizeof commands / sizeof commands[0]; i++) {
    if (strcmp(argv[1], commands[i].name) == 0) return commands[i].run(argc - 1, argv + 1);
}
```

Common in interpreters, CLIs, and state machines (`state = states[state].next(input)`).

## Structs of function pointers

Grouping function pointers in a struct gives you interfaces, the C version of virtual methods:

```c
typedef struct {
    int  (*open)(void *self, const char *path);
    long (*read)(void *self, void *buf, size_t n);
    void (*close)(void *self);
} reader_ops;
```

See [opaque types and OOP in C](../04-advanced/07-opaque-types-oop-in-c.md).

## Rules and pitfalls

- **Signatures must match.** Calling through a pointer of the wrong function type is undefined. This includes casting `int (*)(const void *, const void *)` to a differently typed comparator.
- **Check for `NULL`** before calling optional callbacks; calling a null function pointer crashes.
- **No arithmetic** on function pointers; comparing them with `==` is fine.
- **Don't convert** between function pointers and `void *` in portable code. POSIX requires it to work (`dlsym`), but ISO C doesn't.
- **Lifetime:** a function pointer to a normal function never dangles; pointers into unloaded shared libraries do.
- A callback that calls back into the library can cause reentrancy bugs; document what is allowed.

## Debugging

- In gdb, `print op` shows the target function name; `info symbol 0x...` maps an address to a function.
- A crash at an address with no symbol usually means a corrupted or uninitialized function pointer.

## Summary

- `ret (*name)(params)` declares a function pointer; use a `typedef` for clarity.
- Pass `void *ctx` along with callbacks.
- `qsort` comparators use `(a > b) - (a < b)` and receive pointers *to elements*.
- Tables and structs of function pointers replace `switch` chains and give polymorphism.

**Next:** [Multi-file programs](./02-multi-file-programs.md)
