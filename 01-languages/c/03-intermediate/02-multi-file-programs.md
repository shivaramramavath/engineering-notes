# Multi-File Modular C Programs: Headers, Translation Units, and Linkage

> Modular architecture in C: header interfaces, implementation files, include guards, circular dependency breaking, external linkage, and multi-file compilation pipelines.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [Compilation Process](../00-setup/03-compilation-process.md), [Scope and Storage Classes](../02-core-concepts/07-scope-storage-classes.md), [Preprocessor](../02-core-concepts/09-preprocessor.md) |
| **Time to Read** | 30 minutes |
| **Status** | Complete |
| **Standard** | ISO C11 / C17 / C23 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison: When to Use What](#9-comparison-when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Trivial programs fit into a single `.c` source file. Real-world software—operating systems, game engines, database servers, and network stacks—comprises thousands of source files organized into modular subsystems.

C does not have built-in `package` or `import` keywords like Java or Python. Instead, C uses a battle-tested physical convention based on two distinct file types:
1. **Header Files (`.h`)**: The public interface contract declaring function prototypes, type definitions (`typedef`), and external global declarations (`extern`).
2. **Implementation Files (`.c`)**: The private source implementation defining function bodies and internal translation-unit state (`static`).

Each `.c` file is compiled in complete isolation into an independent **Translation Unit** generating an object file (`.o` or `.obj`). The **Linker (`ld`)** then resolves symbolic cross-references across all object files to assemble the final binary executable. Mastering multi-file architectures prevents "undefined reference" and "multiple definition" errors and accelerates incremental build speeds.

---

## 2. Core Concepts

### Separation of Declaration and Definition
- **Declaration**: Informs the compiler that an identifier and its type exist somewhere in the program. Does **not** allocate physical memory storage:
  ```c
  /* In header.h */
  int calculate_hash(const char *data); /* Function declaration */
  extern int g_total_requests;          /* Variable declaration */
  ```
- **Definition**: Instantiates the actual object, causing the compiler to allocate physical machine code in `.text` or memory storage in `.data`/`.bss`:
  ```c
  /* In module.c */
  int g_total_requests = 0;             /* Variable definition */
  int calculate_hash(const char *data) { return 42; } /* Function definition */
  ```

### Header Include Guards
When a header is included multiple times within the same translation unit (e.g., transitively via other headers), type definitions would be duplicated, causing compile-time errors. Standard include guards guarantee idempotency:
```c
#ifndef MYMODULE_H
#define MYMODULE_H

/* Declarations */

#endif /* MYMODULE_H */
```

### Internal Linkage (`static`) for Encapsulation
Functions and global variables declared `static` at file scope possess **internal linkage**. The linker hides these symbols from other translation units, preventing naming collisions and enforcing information hiding.

---

## 3. How It Works Internally

### The Multi-File Compilation & Linking Pipeline

```
 mathutil.h (Public Header)
   ├── Included by mathutil.c
   └── Included by main.c

 mathutil.c ──► [ Preprocessor ] ──► [ Compiler cc1 ] ──► mathutil.o (Defines math_gcd)
                                                               │
 main.c     ──► [ Preprocessor ] ──► [ Compiler cc1 ] ──► main.o     (Refers to math_gcd)
                                                               │
                                                               ▼
                                                     [ Linker ld / gcc ]
                                                               │
                                                               ▼
                                                       app.exe (Executable)
```

During step 1, `main.c` compiles with zero knowledge of `mathutil.c`'s source code; it only verifies the prototype in `mathutil.h` and emits an unresolved symbolic reference (`R_X86_64_PLT32 math_gcd`) into `main.o`. The linker inspects the symbol tables (`.symtab`) of both object files, locates `math_gcd` inside `mathutil.o`, patches the call address inside `main.o`, and produces the final binary.

---

## 4. Syntax & Usage

```c
/* Project Structure */
/* include/module.h */
#ifndef MODULE_H
#define MODULE_H

typedef struct ModuleContext ModuleContext; /* Forward declaration */

ModuleContext* module_init(void);
void           module_shutdown(ModuleContext *ctx);

#endif

/* src/module.c */
#include "module.h"
#include <stdlib.h>

struct ModuleContext { int state; }; /* Full definition hidden */

ModuleContext* module_init(void) { return malloc(sizeof(ModuleContext)); }
void module_shutdown(ModuleContext *ctx) { free(ctx); }
```

> [!NOTE]
> ### Always Include Own Header First
> In `module.c`, the very first non-system include should always be its own header `#include "module.h"`. This practice proves that `module.h` is **self-contained**: if `module.h` forgets to include `<stddef.h>` for `size_t`, `module.c` fails to compile immediately.

---

## 5. Practical Examples

### Complete Demonstration Program
The following multi-file project implements a modular mathematical utility library (`mathutil.h`, `mathutil.c`) integrated with an application entry point (`main.c`).

#### `mathutil.h` (Interface)
```c
#ifndef MATHUTIL_H
#define MATHUTIL_H

/* Public interface */
int math_gcd(int a, int b);
int math_lcm(int a, int b);
extern int g_mathutil_ops_count;

#endif /* MATHUTIL_H */
```

#### `mathutil.c` (Implementation)
```c
#include "mathutil.h"

int g_mathutil_ops_count = 0;

static int internal_abs(int x) {
    return x < 0 ? -x : x;
}

int math_gcd(int a, int b) {
    g_mathutil_ops_count++;
    a = internal_abs(a);
    b = internal_abs(b);
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int math_lcm(int a, int b) {
    if (a == 0 || b == 0) return 0;
    return internal_abs(a * b) / math_gcd(a, b);
}
```

#### `main.c` (Consumer)
```c
#include <stdio.h>
#include "mathutil.h"

int main(void) {
    printf("=== C Multi-File Modular Compilation Demo ===\n");
    int a = 24, b = 36;
    printf("GCD(%d, %d) = %d\n", a, b, math_gcd(a, b));
    printf("LCM(%d, %d) = %d\n", a, b, math_lcm(a, b));
    printf("Total math operations tracked: %d\n", g_mathutil_ops_count);
    return 0;
}
```

```text
=== C Multi-File Modular Compilation Demo ===
GCD(24, 36) = 12
LCM(24, 36) = 72
Total math operations tracked: 2
```

---

## 6. Best Practices

1. **Headers Must Be Self-Contained and Idempotent**: Any header should compile cleanly when included by an empty `.c` file without requiring preceding includes. Always wrap headers in include guards.
2. **Never Define Variables in Headers**: Writing `int count = 0;` in a header causes fatal `multiple definition of 'count'` linker errors when included in multiple `.c` files. Use `extern int count;` in the header and define it once in a `.c` file.
3. **Use Forward Declarations to Break Circular Dependencies**: Instead of `#include "b.h"` in `a.h`, write `typedef struct B B;` if `a.h` only uses pointers (`B *`).
4. **Mark Internal Helpers `static`**: Any function not intended for public export must be marked `static` in its `.c` file to keep the global symbol table clean and enable compiler inlining.
5. **Use `static inline` for Header-Only Utility Functions**: Small performance-critical utility functions placed in headers must be declared `static inline` so each translation unit receives an internal copy without linker symbol collisions.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Defining non-static variables in headers** | Causes linker collision: `multiple definition of 'symbol'`. | Declare `extern int var;` in header; define in one `.c` file. |
| **Including `.c` files directly** | Violates modular translation unit compilation; creates massive compilation bloat. | Include only `.h` interface headers; link `.o` files. |
| **Circular header inclusion without guards** | Preprocessor recurses infinitely until hitting maximum include depth. | Add `#ifndef HEADER_H / #define HEADER_H` guards. |
| **Non-self-contained headers** | Header fails to compile unless specific other headers are included first. | Include required type headers (`<stdint.h>`, `<stddef.h>`) inside the header. |
| **Missing `extern` on global declaration** | Creates a tentative definition in every translation unit, causing linker errors under `-fno-common`. | Always prefix header declarations with `extern`. |
| **Omitting include guards** | Including header transitively multiple times redefines structs and typedefs. | Always enclose headers in standard include guards. |

---

## 8. Performance & Security Notes

### Incremental Compilation Speedups
In large codebases (e.g., the Linux kernel with 30,000 files), modifying a single `.c` file requires recompiling only that one file into a `.o` object file (taking 0.1 seconds) followed by linking. If all code were in a single file or headers included unnecessary dependencies, every small edit would force a full 30-minute recompile.

### Security: Symbol Exposure and Attack Surface
Global symbols exported without `static` are visible in the binary's global symbol table (`.dynsym`). In shared libraries (`.so` / `.dll`), attackers can intercept or override these symbols via `LD_PRELOAD`. Marking internal functions `static` strips their symbols from dynamic linking tables, hardening the binary against symbol interposition attacks.

---

## 9. Comparison: When to Use What

| Mechanism | Scope / Visibility | Allocation | Best Used For |
| :--- | :--- | :--- | :--- |
| **`extern` in Header** | Global across all files | In one `.c` file | Public configuration state, shared APIs |
| **`static` in `.c` File** | Private to Translation Unit | In `.data`/`.bss` of this module | Private helper functions, internal state |
| **`static inline` in Header** | Private copy per includer | Inlined or emitted locally | Fast math accessors, byte-swap utilities |
| **Opaque Typedef Struct** | Abstract handle | Pointer only | Complete encapsulation (ADTs) |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is a Translation Unit in C?</strong></summary>

A Translation Unit (TU) is the complete C source stream produced by the preprocessor for a single `.c` file after recursively expanding all `#include` directives, evaluating conditional `#if` blocks, and expanding all `#define` macros. The compiler processes each translation unit independently to produce an object file.
</details>

<details>
<summary><strong>2. What is the difference between an "undefined reference" and a "multiple definition" error?</strong></summary>

- **"Undefined Reference"**: A linker error occurring when an identifier was declared (e.g., via a prototype in a header) and called in code, but the linker cannot find a definition matching that symbol in any linked object file or library.
- **"Multiple Definition"**: A linker error occurring when two or more object files define an external symbol with the exact same name, creating ambiguity over which definition to link.
</details>

<details>
<summary><strong>3. Why should you never write <code>#include "foo.c"</code>?</strong></summary>

Including a `.c` file inlines the entire implementation directly into another translation unit. This violates the principle of independent translation units, causes duplicate symbol definitions if `foo.c` is also compiled separately, and destroys incremental build efficiency.
</details>

<details>
<summary><strong>4. How do you resolve a circular dependency between two headers (A includes B, and B includes A)?</strong></summary>

By breaking the cycle using **forward declarations**. If `header_a.h` only references pointers to types defined in `header_b.h`, replace `#include "header_b.h"` with a forward declaration `typedef struct B B;`. Move the actual `#include "header_b.h"` into the `.c` implementation file.
</details>

<details>
<summary><strong>5. What is the purpose of the <code>extern "C"</code> guard in headers?</strong></summary>

When C header files are included by C++ translation units, the C++ compiler applies name mangling to function symbols for function overloading. The `extern "C"` linkage specification instructs the C++ compiler to maintain standard C linkage and symbol naming, enabling seamless interoperation between C and C++.
</details>

<details>
<summary><strong>6. What is the GCC flag <code>-fno-common</code> and why is it now the default?</strong></summary>

Historically, C allowed "common symbols" where multiple tentative definitions of uninitialized globals in different object files were merged by the linker into a single symbol. GCC 10 changed the default to `-fno-common`, causing duplicate global definitions in headers to fail loudly at link time, enforcing strict ISO C compliance.
</details>

<details>
<summary><strong>7. How does a forward declaration differ from an incomplete type?</strong></summary>

A forward declaration `struct Point;` introduces the tag name to the compiler as an **incomplete type**. You can declare pointers to an incomplete type (`struct Point *p;`), but you cannot allocate it by value or access its members (`p->x`) until the complete type definition is provided.
</details>

<details>
<summary><strong>8. What is the role of the <code>-I</code> compiler flag?</strong></summary>

The `-I<dir>` flag adds `<dir>` to the list of directories searched by the preprocessor when resolving `#include <...>` and `#include "..."` directives, enabling clean separation between source code and public header directories (e.g., `-Iinclude`).
</details>

---

## 11. Practice Exercises

### Exercise 1: Modular String Counter (Easy)
Create a two-file module (`strutil.h` and `strutil.c`). Implement `size_t count_words(const char *str)`. Verify that `strutil.h` compiles cleanly on its own.

<details>
<summary>View Solution</summary>

`strutil.h`:
```c
#ifndef STRUTIL_H
#define STRUTIL_H

#include <stddef.h>

size_t count_words(const char *str);

#endif
```

`strutil.c`:
```c
#include "strutil.h"
#include <ctype.h>

size_t count_words(const char *str) {
    if (!str) return 0;
    size_t count = 0;
    int in_word = 0;
    while (*str) {
        if (isspace((unsigned char)*str)) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            count++;
        }
        str++;
    }
    return count;
}
```
</details>

### Exercise 2: Circular Dependency Refactoring (Medium)
Given two structs `User` and `Account` where `User` has an `Account *` and `Account` has a `User *`, write two headers (`user.h` and `account.h`) that resolve the circular dependency using forward declarations.

<details>
<summary>View Solution</summary>

`user.h`:
```c
#ifndef USER_H
#define USER_H

typedef struct Account Account; /* Forward declaration */

typedef struct User {
    int user_id;
    Account *account;
} User;

#endif
```

`account.h`:
```c
#ifndef ACCOUNT_H
#define ACCOUNT_H

typedef struct User User; /* Forward declaration */

typedef struct Account {
    int account_id;
    User *owner;
} Account;

#endif
```
</details>

### Exercise 3: Opaque Handle Stack Module (Hard)
Implement an abstract data type Stack module (`stack.h` and `stack.c`). The consumer must only receive an opaque pointer `typedef struct Stack Stack;`. All internal node and array fields must be completely invisible outside `stack.c`.

<details>
<summary>View Solution</summary>

`stack.h`:
```c
#ifndef STACK_H
#define STACK_H

#include <stdbool.h>

typedef struct Stack Stack;

Stack* stack_create(void);
bool   stack_push(Stack *s, int val);
bool   stack_pop(Stack *s, int *out_val);
void   stack_destroy(Stack *s);

#endif
```

`stack.c`:
```c
#include "stack.h"
#include <stdlib.h>

struct Stack {
    int items[128];
    int top;
};

Stack* stack_create(void) {
    Stack *s = malloc(sizeof *s);
    if (s) s->top = 0;
    return s;
}

bool stack_push(Stack *s, int val) {
    if (!s || s->top >= 128) return false;
    s->items[s->top++] = val;
    return true;
}

bool stack_pop(Stack *s, int *out_val) {
    if (!s || s->top <= 0) return false;
    if (out_val) *out_val = s->items[--s->top];
    return true;
}

void stack_destroy(Stack *s) {
    free(s);
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Header Template (mymodule.h) */
#ifndef MYMODULE_H
#define MYMODULE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Public prototypes and types */
int mymodule_init(void);

#ifdef __cplusplus
}
#endif

#endif /* MYMODULE_H */

/* Compilation Commands */
gcc -Wall -Wextra -c module.c -o module.o  # Compile unit 1
gcc -Wall -Wextra -c main.c   -o main.o    # Compile unit 2
gcc main.o module.o -o app                 # Link executable
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §6.2.2 "Linkages of identifiers".
- *Computer Systems: A Programmer's Perspective* (3rd ed.) by Bryant & O'Hallaron (Chapter 7: "Linking").
- *The Practice of Programming* by Brian Kernighan & Rob Pike (Modular interface design).
- GCC Online Docs: *Options Controlling the Preprocessor (-I, -D, -fno-common)*.

---

[← Previous: Function Pointers](./01-function-pointers.md) | [Section Index](./README.md) | [Next: Error Handling →](./03-error-handling.md)
