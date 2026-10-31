# Program Structure

A C program is a set of source files, each made of declarations and function definitions. There are no classes or modules: the structure of your code is a convention built from files, headers, and the order of declarations. Understanding the pieces now makes later topics (headers, linking, scope) much easier.

**Prerequisites:** [First program](../00-setup/04-first-program.md), [compilation process](../00-setup/03-compilation-process.md).

## Anatomy of a small program

```c
#include <stdio.h>                 /* 1. preprocessor directives */

#define MAX_ITEMS 3                /* 2. macros and constants */

static int square(int n);          /* 3. function declarations (prototypes) */

int main(void) {                   /* 4. entry point */
    for (int i = 1; i <= MAX_ITEMS; i++) {
        printf("%d squared is %d\n", i, square(i));
    }
    return 0;
}

static int square(int n) {         /* 5. function definitions */
    return n * n;
}
```

A conventional order inside a `.c` file is: includes → macros and type definitions → file-level variables → prototypes → `main` → other functions. Nothing enforces it, except that **a name must be declared before it is used**.

## Declaration vs definition

| | Declaration | Definition |
|---|---|---|
| Says | "this name exists and has this type" | "here is the actual function body or storage" |
| Function | `int square(int n);` | `int square(int n) { return n * n; }` |
| Variable | `extern int count;` | `int count = 0;` |
| Allowed | many times | exactly once in the whole program |

This split is what lets code in one file call a function defined in another: the caller needs only the declaration (usually from a header), and the linker finds the definition. See [multi-file programs](../03-intermediate/02-multi-file-programs.md).

## Translation units

The compiler works on one **translation unit** at a time: a `.c` file after the preprocessor has pasted in all its `#include`s. It knows nothing about other `.c` files. That is why each file must include the declarations it needs, and why a typo in a prototype can compile fine and fail (or misbehave) at link or run time.

## Building blocks

- **Statements** end with `;` (`x = 5;`), and **blocks** `{ ... }` group them and create a scope.
- **Expressions** produce a value. Assignment is an expression too, so `a = b = 0;` is legal and `if ((c = getchar()) != EOF)` is a common idiom.
- **Identifiers** are case-sensitive, contain letters, digits and `_`, and can't start with a digit. Avoid names beginning with `_` followed by an uppercase letter, or containing `__`; those are reserved for the implementation. Don't reuse standard library names such as `index`, `remove`, or `time` for your own globals.
- **Comments:** `/* ... */` (does not nest) and, since C99, `// ...` to end of line.

## `main`

The two portable forms on a hosted system:

```c
int main(void)
int main(int argc, char *argv[])
```

The second receives command-line arguments (see [command-line arguments](../02-core-concepts/10-command-line-arguments.md)). Prefer `(void)` over `()` when you take no parameters; before C23, empty parentheses mean "unspecified arguments".

## Common mistakes

| Mistake | Symptom |
|---|---|
| Missing `;` after a statement or `struct` definition | Confusing error reported on the *next* line |
| Using a function before declaring it | "implicit declaration" (an error on modern compilers) |
| Defining a function or variable in a header and including it twice | Linker error: `multiple definition of ...` |
| Unmatched `{` or `/*` | Errors far from the actual mistake; indent consistently to find it |
| Assuming code runs top to bottom like a script | Only `main` is the entry point; functions run when called |

## Summary

- A C program is functions plus declarations spread across translation units.
- Declare before use; define exactly once.
- `main` is the entry point; prefer `int main(void)`.

**Next:** [Data types](./02-data-types.md)
