# Scope and Storage Classes

Three separate ideas decide where a name is visible and how long its object lives: **scope** (where the name is usable), **storage duration** (how long the object exists), and **linkage** (whether other files can refer to it). The keywords `static` and `extern` adjust these.

**Prerequisites:** [Variables and constants](../01-fundamentals/04-variables-constants.md), [program structure](../01-fundamentals/01-program-structure.md).

## Scope: where a name is visible

| Scope | Where declared | Visible |
|---|---|---|
| Block | inside `{ }` (including function bodies, `for` headers) | from the declaration to the end of the block |
| File | outside any function | from the declaration to the end of the file |
| Function prototype | in a parameter list | only inside the prototype |
| Function | `goto` labels only | whole function |

An inner name **shadows** an outer one with the same name; compile with `-Wshadow`.

## Storage duration: how long the object lives

| Duration | Created / destroyed | Examples |
|---|---|---|
| Automatic | on entering / leaving the block | ordinary locals and parameters |
| Static | before `main` / program end | globals, `static` locals |
| Allocated | `malloc` / `free` | heap memory |
| Thread | per thread | `_Thread_local` objects |

```text
Process memory (simplified)

 high  ┌──────────────┐
       │ stack        │  automatic variables
       │      ↓       │
       │              │
       │      ↑       │
       │ heap         │  malloc'd memory
       ├──────────────┤
       │ .bss         │  static, zero-initialized
       │ .data        │  static, initialized
       │ .text        │  code (and read-only data)
 low   └──────────────┘
```

See [process memory layout](../05-internals/01-process-memory-layout.md).

## Automatic variables

```c
void f(void) {
    int x = 0;       /* created each call, destroyed on return */
}
```

Uninitialized automatic variables hold indeterminate values. The `auto` keyword exists but is redundant (C23 repurposes it for type inference). `register` is a long-obsolete hint; compilers ignore it, but it forbids taking the variable's address.

## `static` has two meanings

### 1. On a local variable: static storage, block scope

```c
int next_id(void) {
    static int id = 0;      /* initialized once; keeps its value between calls */
    return ++id;
}
```

The initializer must be a constant expression and runs once. Such functions are not thread-safe and not reentrant; shared state is hidden, which makes testing harder.

### 2. On a file-scope name: internal linkage

```c
static int counter;                 /* visible only in this .c file */
static void helper(void) { ... }    /* same for functions */
```

Without `static`, a file-scope name has **external linkage** and can clash with the same name in other files. Make everything `static` unless it's part of the module's public interface.

## Linkage and `extern`

| Linkage | Meaning | Typical case |
|---|---|---|
| External | same name refers to the same entity across all files | non-`static` globals and functions |
| Internal | name private to the translation unit | `static` globals and functions |
| None | name unique to its scope | locals, parameters |

`extern` **declares** a name defined elsewhere (it allocates no storage):

```c
/* config.h */
extern int verbose;            /* declaration: "it exists somewhere" */

/* config.c */
#include "config.h"
int verbose = 0;               /* the one definition */

/* main.c */
#include "config.h"
if (verbose) { ... }
```

Putting `int verbose = 0;` (a definition) in a header and including it in two files causes `multiple definition` errors at link time. Function declarations are implicitly `extern`.

## Globals: use sparingly

Global variables are convenient but hide dependencies, break reentrancy, and make threaded code racy. Prefer passing state through parameters or a struct. When a global is needed, make it `static` and expose accessor functions.

## `_Thread_local`

```c
_Thread_local int last_error;      /* one copy per thread; C23 spells it thread_local */
```

Each thread gets its own copy. `errno` works this way.

## Initialization rules

| Object | Default value |
|---|---|
| Automatic | indeterminate |
| Static / global | zero (`0`, `NULL`, `0.0`) |
| Allocated by `malloc` | indeterminate (`calloc` zeroes) |

Static initializers must be constants: `static int x = f();` is an error in C (it isn't in C++).

## Common mistakes

| Mistake | Effect |
|---|---|
| Returning the address of an automatic variable | Dangling pointer |
| Definition (`int x = 1;`) in a header | `multiple definition` linker error |
| Same-named non-static globals in two files | Link error or silent merging in old toolchains |
| Relying on a `static` local in threaded code | Data race |
| Assuming a global `int` is uninitialized | It's zero |
| Shadowing a loop or outer variable | Logic bug that compiles cleanly |

## Summary

- Scope = where a name is visible; duration = how long the object lives; linkage = cross-file visibility.
- `static` local: persists across calls. `static` file-scope: private to the file.
- `extern` declares; define each global exactly once.
- Statics start at zero; automatics don't. Prefer fewer globals.

**Next:** [Memory management](./08-memory-management.md)
