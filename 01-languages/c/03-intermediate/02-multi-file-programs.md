# Multi-File Programs

Real programs are split across files. C's module system is a convention: a **header** (`.h`) declares an interface, a **source file** (`.c`) implements it, and the linker joins the pieces. Getting this convention right avoids most "undefined reference" and "multiple definition" errors.

**Prerequisites:** [Compilation process](../00-setup/03-compilation-process.md), [scope and storage classes](../02-core-concepts/07-scope-storage-classes.md), [preprocessor](../02-core-concepts/09-preprocessor.md).

## A minimal module

```text
project/
├── main.c
├── mathutil.h
└── mathutil.c
```

```c
/* mathutil.h: the public interface */
#ifndef MATHUTIL_H
#define MATHUTIL_H

int gcd(int a, int b);
extern int mathutil_calls;          /* declaration of a global (use rarely) */

#endif
```

```c
/* mathutil.c: the implementation */
#include "mathutil.h"               /* include own header first: checks consistency */

int mathutil_calls = 0;             /* the one definition */

static int abs_int(int x) { return x < 0 ? -x : x; }   /* private helper */

int gcd(int a, int b) {
    mathutil_calls++;
    a = abs_int(a); b = abs_int(b);
    while (b != 0) { int t = a % b; a = b; b = t; }
    return a;
}
```

```c
/* main.c */
#include <stdio.h>
#include "mathutil.h"

int main(void) {
    printf("%d\n", gcd(12, 18));    /* 6 */
    return 0;
}
```

```bash
gcc -std=c17 -Wall -Wextra -c main.c      -o main.o
gcc -std=c17 -Wall -Wextra -c mathutil.c  -o mathutil.o
gcc main.o mathutil.o -o app
```

Each `.c` file is its own translation unit and is compiled independently; `main.c` sees only the *declaration* of `gcd`, and the linker connects the call to the definition.

## What belongs in a header

| Put in the `.h` | Keep in the `.c` |
|---|---|
| Function prototypes for the public API | Function bodies |
| `typedef`s, `struct`/`enum` definitions callers need | Private helpers (mark `static`) |
| `extern` declarations of shared globals | Variable definitions |
| Macros and constants | Internal macros |
| `static inline` functions | Everything not part of the interface |

**Never define a non-`static` variable or ordinary function in a header.** Each file that includes it gets its own copy, and the linker reports `multiple definition`.

## Rules for good headers

1. **Include guards** on every header (see [preprocessor](../02-core-concepts/09-preprocessor.md)).
2. **Self-contained:** a header should compile when included first and alone. Test with `echo '#include "x.h"' | gcc -x c -fsyntax-only -`. A header that uses `size_t` must include `<stddef.h>`.
3. **Include what you use** in the `.c`; don't rely on a header including another transitively.
4. **Minimal includes in headers:** prefer a forward declaration when only a pointer is needed:

```c
/* parser.h */
struct lexer;                               /* forward declaration: incomplete type */
int parse(struct lexer *lx);                /* fine: only a pointer is used */
```

   Only include the other header when you need the full struct layout.
5. **No `using`-style pollution:** don't `#define` short common names in headers.
6. **Prefix public names** (`mathutil_gcd`) in libraries to avoid collisions; C has no namespaces.

### Circular dependencies

If `a.h` and `b.h` each need the other, break the cycle with forward declarations, or move the shared type into a third header. Include guards prevent infinite recursion but won't fix a missing type.

## Visibility

- **Public:** declared in the header, non-`static`.
- **Private:** `static` in the `.c` file. This gives internal linkage; the symbol isn't exported and can't clash.
- **Opaque types:** declare `typedef struct foo foo_t;` in the header and define the struct only in the `.c`; callers can't touch fields. See [declarations and typedef](../02-core-concepts/04-declarations-typedef.md).

## Mixing with C++

A header meant to be called from C++ should wrap its declarations:

```c
#ifdef __cplusplus
extern "C" {
#endif

int gcd(int a, int b);

#ifdef __cplusplus
}
#endif
```

## Building with `make`

Rebuild only what changed:

```make
CC      = gcc
CFLAGS  = -std=c17 -Wall -Wextra -g -MMD -MP
OBJS    = main.o mathutil.o

app: $(OBJS)
	$(CC) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

clean:
	rm -f app $(OBJS) $(OBJS:.o=.d)
```

`-MMD -MP` makes the compiler write `.d` files listing each object's header dependencies, so editing a header triggers the right rebuilds. Recipe lines must start with a **tab**. See [build systems](../08-production/01-build-systems.md).

## Common errors

| Message | Cause |
|---|---|
| `undefined reference to 'f'` | Declared but never defined, or its `.o`/library isn't on the link line |
| `multiple definition of 'x'` | Definition placed in a header, or two `.c` files define the same global |
| `unknown type name 'size_t'` (in a header) | Header isn't self-contained |
| `implicit declaration of function` | Missing `#include` of the header declaring it |
| `conflicting types for 'f'` | Prototype in the header disagrees with the definition (include the header in the `.c` to catch this) |
| Stale behavior after editing a header | Build doesn't track header dependencies |

## Summary

- Header = interface, source = implementation; share declarations, not definitions.
- Guard headers, keep them self-contained, forward-declare when possible.
- Use `static` for anything private; prefix public names.
- Let your build tool track header dependencies.

**Next:** [Error handling](./03-error-handling.md)
