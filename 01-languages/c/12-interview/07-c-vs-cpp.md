# C vs C++ Differences and Incompatibilities (`07-c-vs-cpp`)

> Understand why C is not a strict subset of C++: character literal types, implicit void pointer conversions, struct tags, const linkage, and extern "C" ABI rules.

| Metadata | Details |
| :--- | :--- |
| **Guide** | `07-c-vs-cpp` |
| **Level** | Intermediate / Advanced |
| **Standard Baseline** | ISO C11/C17 vs ISO C++14/17/20 |

---

## 1. Syntax and Type Incompatibilities

<details>
<summary><strong>Q1: What is the size of `'a'` in C versus C++?</strong></summary>

- **In C:** `sizeof('a') == sizeof(int)` (typically 4 bytes). Character constants in C are typed as `int`.
- **In C++:** `sizeof('a') == sizeof(char)` (1 byte). Character constants are typed as `char`.
</details>

<details>
<summary><strong>Q2: Why does `int *p = malloc(sizeof(int));` compile in C but fail in C++?</strong></summary>

In C, `void*` converts implicitly to any other data pointer type.
In C++, implicit conversion from `void*` to a typed pointer is strictly forbidden and triggers a compile-time error. C++ requires an explicit cast: `int *p = (int*)malloc(...);` or `static_cast<int*>(...)`.
*(Note: Casting `malloc` in C is an anti-pattern because it can mask missing `#include <stdlib.h>` prototypes in pre-C23 standard modes).*
</details>

<details>
<summary><strong>Q3: How do struct tag namespaces differ between C and C++?</strong></summary>

- **In C:** Struct tags live in a separate identifier namespace. Declaring `struct Point { int x, y; };` requires writing `struct Point p;` unless a `typedef` is provided (`typedef struct Point Point;`).
- **In C++:** Struct tags automatically enter the general type namespace. `Point p;` is valid immediately without a typedef.
</details>

<details>
<summary><strong>Q4: How does the linkage of global `const` variables differ between C and C++?</strong></summary>

- **In C:** Global `const int MAX = 100;` has **external linkage** by default (`extern`).
- **In C++:** Global `const int MAX = 100;` has **internal linkage** by default (`static`).
</details>

---

## 2. ABI and Interoperability

<details>
<summary><strong>Q5: What is the exact role of `extern "C"` when writing C libraries consumed by C++?</strong></summary>

C++ compilers mangle function names in symbol tables to support function overloading (e.g., `calculate(int)` becomes `_Z9calculatei`).
C compilers do not mangle symbol names (`calculate` remains `calculate`).
Wrapping C declarations in `extern "C" { ... }` instructs the C++ compiler to disable name mangling and adhere strictly to C ABI calling conventions, enabling the C++ linker to resolve symbols in compiled C object files.
</details>

<details>
<summary><strong>Q6: Why are Variable-Length Arrays (VLAs) invalid in standard C++?</strong></summary>

VLAs (introduced in C99, made optional in C11) allocate runtime-sized buffers on the stack (`int arr[n];`).
Standard C++ rejected VLAs because they complicate object destructor unwinding, exception handling, and stack frame analysis. C++ uses `std::vector` instead.
</details>

---

[← Systems QA](./06-systems-qa.md) | [Interview Index](./README.md) | [Next: Undefined Behavior Puzzles →](./08-undefined-behavior-puzzles.md)
