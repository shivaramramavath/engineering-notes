# Variables and Constants

A variable is a named region of storage with a type. A constant is a value that must not change. C has several ways to express constants, and they behave differently in ways that surprise people coming from other languages.

**Prerequisites:** [Data types](./02-data-types.md).

## Declaring, defining, initializing

```c
int count;              /* defined, NOT initialized (for a local variable) */
int total = 0;          /* defined and initialized */
int a = 1, b = 2;       /* several in one statement */
```

### Initial values

| Where declared | Value if not initialized |
|---|---|
| Local (automatic) variable | **Indeterminate**; reading it is a bug |
| `static` local, or file-scope variable | Zero (`0`, `0.0`, `NULL`) |

```c
int main(void) {
    int x;                 /* garbage */
    printf("%d\n", x);     /* bug: uses an indeterminate value */
}
```

Initialize every local variable at its declaration, or declare it right where you first assign it. `-Wall -Wextra` warns about many uninitialized uses (`-Wuninitialized`, `-Wmaybe-uninitialized`). Valgrind and MemorySanitizer detect the rest. See [sanitizers](../06-debugging-testing/04-sanitizers-valgrind.md).

### Declaration trap

The `*` binds to the variable, not the type:

```c
int* a, b;      /* a is int*, b is plain int */
int *a, *b;     /* both pointers */
```

Declare one variable per line to avoid this.

## `const`

`const` marks an object as read-only through that name:

```c
const int max_users = 100;
max_users = 200;           /* compile error */
```

Important facts:

- `const` is a promise enforced by the compiler, not a guarantee about memory. Modifying a `const` object through a cast is undefined behavior.
- In C, a `const int` is **not** a constant expression. You cannot use it where the language needs a compile-time constant, such as a file-scope array size or a `case` label.

```c
const int n = 5;
int arr[n];         /* at file scope: error. In a function: a variable length array (VLA) */
```

Pointers and `const` have extra rules; see [type qualifiers](../02-core-concepts/05-type-qualifiers.md).

## Compile-time constants: three options

```c
#define BUFFER_SIZE 256          /* 1. macro: textual replacement */
enum { BUFFER_SIZE2 = 256 };     /* 2. enum constant: an int constant expression */
constexpr int buffer_size = 256; /* 3. C23 only */
```

| Option | Typed | Scoped | Usable as array size / `case` | Notes |
|---|---|---|---|---|
| `#define` | no | no (global from definition) | yes | No type checking; wrap expressions in parentheses |
| `enum` | `int` | yes | yes | Good for related integer constants |
| `constexpr` (C23) | yes | yes | yes | Not available before C23 |
| `const` | yes | yes | **no** | Read-only variable, not a true constant |

Macro pitfall:

```c
#define SQUARE(x) x * x
SQUARE(1 + 2)           /* expands to 1 + 2 * 1 + 2 = 5, not 9 */
#define SQUARE_OK(x) ((x) * (x))   /* still evaluates x twice: SQUARE_OK(i++) is a bug */
```

Prefer `enum` or `const`/`constexpr` over `#define` for constants, and a `static inline` function over a function-like macro. See [preprocessor](../02-core-concepts/09-preprocessor.md).

## String constants

```c
const char *p = "hello";     /* points to read-only string literal */
char s[] = "hello";          /* array: a writable copy */

s[0] = 'J';                  /* fine */
p[0] = 'J';                  /* undefined behavior: modifies a string literal */
```

Declare pointers to string literals as `const char *` so the compiler catches modification attempts. More in [strings](../02-core-concepts/02-strings.md).

## Scope and shadowing

A variable lives from its declaration to the end of its enclosing block. An inner declaration with the same name **shadows** the outer one:

```c
int x = 1;
{
    int x = 2;     /* different variable; outer x hidden here */
}
```

Shadowing hides bugs. Enable `-Wshadow`. Lifetime and linkage (`static`, `extern`) are covered in [scope and storage classes](../02-core-concepts/07-scope-storage-classes.md).

## Naming

Choose one convention and keep it. A common C style: `snake_case` for functions and variables, `UPPER_CASE` for macros and enum constants, `PascalCase` or `snake_case_t` for types. Names should say what the value means, not its type.

## Summary

- Locals are uninitialized by default; statics and globals are zeroed. Always initialize locals.
- `const` is read-only, not compile-time. Use `enum` or `#define` (or `constexpr` in C23) for true constants.
- String literals are read-only. Use `const char *` for them.
- `int* a, b;` declares one pointer and one int.

**Next:** [Operators](./05-operators.md)
