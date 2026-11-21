# Const-Correctness Basics and Pointer Semantics

> The definitive engineering guide to `const` in C: read-only contracts, pointer-to-const vs const-pointer, reading declarations using the right-to-left clockwise rule, casting away constness pitfalls, function parameter safety, and compiler `.rodata` placement.

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [04-variables-constants.md](./04-variables-constants.md), [02-data-types.md](./02-data-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard Baseline** | ISO C11 / C17 / C23 |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

In C, the `const` type qualifier does not mean "constant" in the mathematical sense; it means **read-only**. It establishes an explicit compiler-enforced contract that an identifier cannot be used as the target of an assignment.

Adopting rigorous const-correctness provides foundational software engineering advantages:
1. **Self-Documenting API Interfaces**: A function taking `const char *src` explicitly communicates to callers that their data will not be mutated, distinguishing pure input buffers from output buffers (`char *dest`).
2. **Compile-Time Defect Interception**: If an internal helper inadvertently modifies a read-only configuration struct, the compiler immediately rejects compilation with `assignment of read-only location`.
3. **Hardware Memory Protection**: Declaring static lookup tables `const` instructs the linker to place the data in the `.rodata` segment, mapped with read-only page permissions that trigger a hardware fault if corrupted by a runaway pointer.

---

## 2. Core Concepts

### Pointer Constness Permutations

Pointers have two distinct entities: the **pointer itself** (the variable holding an address) and the **data being pointed to**. The position of `const` relative to the asterisk `*` governs which entity is read-only:

```
+--------------------------------------------------------------------------+
|                       POINTER CONSTNESS PERMUTATIONS                     |
+--------------------------------------------------------------------------+
|  Declaration                | Can Modify Target (*p)? | Can Modify Ptr (p)?|
|  -------------------------- | ----------------------- | -------------------|
|  int *ptr                   | YES (*ptr = 5)          | YES (ptr++)        |
|  const int *ptr             | **NO** (Read-only data) | YES (ptr++)        |
|  int const *ptr             | **NO** (Identical)      | YES (ptr++)        |
|  int * const ptr            | YES (*ptr = 5)          | **NO** (Fixed addr)|
|  const int * const ptr      | **NO** (Read-only data) | **NO** (Fixed addr)|
+--------------------------------------------------------------------------+
```

### The Clockwise / Right-to-Left Reading Rule

To parse complex `const` declarations unambiguously, read from right to left starting at the identifier:

```
const int * const ptr;
 ^     ^  ^   ^     ^
 |     |  |   |     +-- "ptr is a..."
 |     |  |   +-------- "...const..."
 |     |  +------------ "...pointer to..."
 |     +--------------- "...int..."
 +--------------------- "...which is const."
Result: ptr is a CONST POINTER to a CONST INT.
```

---

## 3. How It Works Internally

### The `.rodata` Segment and Memory Protection

When an object with static storage duration is qualified with `const`, the compiler and linker place it in the `.rodata` (Read-Only Data) section of the ELF executable:

```
Virtual Memory Pages:
+-----------------------------------+
| .text Segment (Code)              | --> Permissions: PROT_READ | PROT_EXEC
+-----------------------------------+
| .rodata Segment (const data)      | --> Permissions: PROT_READ (Write-Protected!)
+-----------------------------------+
| .data / .bss Segment (globals)    | --> Permissions: PROT_READ | PROT_WRITE
+-----------------------------------+
| Heap / Stack (Variables)          | --> Permissions: PROT_READ | PROT_WRITE
+-----------------------------------+
```

If code attempts to bypass `const` via an explicit cast:
```c
const int global_val = 100; /* Stored in .rodata */
int *p = (int *)&global_val; /* Cast away constness */
*p = 200; /* HARDWARE EXCEPTION: Operating system catches write to read-only page -> SIGSEGV! */
```
The CPU Memory Management Unit (MMU) catches the invalid write violation and kills the process immediately with a segmentation fault.

---

## 4. Syntax & Usage

### 1. Function Parameter Const-Correctness
```c
#include <stddef.h>

/* 'src' is an input buffer (read-only); 'dest' is an output buffer (mutable) */
void safe_copy(char *dest, const char *src, size_t capacity) {
    if (!dest || !src || capacity == 0) return;
    
    size_t i = 0;
    while (i + 1 < capacity && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}
```

### 2. Multi-Level Pointer Const Qualification
Passing a `char **` to a function expecting `const char **` is **NOT allowed implicitly in C**!
```c
void process(const char **list);

void caller(void) {
    char *my_list[5];
    /* process(my_list); <-- COMPILER WARNING / ERROR IN C! */
    process((const char **)my_list); /* Explicit cast required */
}
```
*Why C forbids this implicitly*: A classic subtle security flaw in C allows a caller's `char *` pointer to be assigned the address of a `const char` string literal, subverting `const` safety without an explicit cast.

---

## 5. Practical Examples

### Const-Correctness & Pointer Semantics Demonstration Program

```c
/* File: 01-languages/c/01-fundamentals/examples/12-const-correctness-basics.c */
#include <stdio.h>

/* Parameter points to read-only memory; cannot be modified by callee */
static void inspect_data(const int *data, size_t count) {
    printf("Inspecting data: [");
    for (size_t i = 0; i < count; ++i) {
        printf("%d%s", data[i], (i + 1 < count) ? ", " : "");
    }
    printf("]\n");
}

int main(void) {
    printf("=== Const-Correctness & Pointer Semantics ===\n");
    int values[] = { 10, 20, 30, 40 };
    inspect_data(values, 4);

    /* Pointer to const vs Const pointer */
    const int target = 99;
    const int *ptr_to_const = &target; /* Value cannot change via ptr */
    printf("Value via ptr_to_const: %d\n", *ptr_to_const);

    int mutable_val = 100;
    int *const const_ptr = &mutable_val; /* Pointer address cannot change */
    *const_ptr = 200;                    /* Value CAN change! */
    printf("Mutated value via const_ptr: %d\n", *const_ptr);

    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 12-const-correctness-basics.c -o run_12.exe
./run_12.exe
```
Output:
```text
=== Const-Correctness & Pointer Semantics ===
Inspecting data: [10, 20, 30, 40]
Value via ptr_to_const: 99
Mutated value via const_ptr: 200
```

---

## 6. Best Practices

1. **Tag All Non-Mutated Pointer Parameters as `const`**: If a function does not modify the data referenced by a pointer parameter, declare it `const Type *param`.
2. **Never Cast Away `const` to Mutate Data**: Casting away `const` and modifying an object that was originally declared `const` is undefined behavior and causes crashes on read-only memory pages.
3. **Use `const` for Global Tables and Strings**: Ensure strings and lookup tables are declared `const` so they reside in `.rodata`.
4. **Prefer `const Type *` Over `Type const *`**: While syntactically identical, `const Type *` is the universal convention in the C community.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Casting away `const` to modify an object | If the underlying object was declared `const`, mutating it invokes undefined behavior (`SIGSEGV`). | Do not mutate `const` data. |
| Forgetting `const` on string literal pointers (`char *s = "hello";`) | String literals reside in `.rodata`. Writing `s[0] = 'H'` triggers an immediate crash. | Always declare string literals as `const char *s = "hello";`. |
| Confusing `const int *p` with `int * const p` | `const int *p` protects the data; `int * const p` protects the pointer address. | Use the right-to-left reading rule. |
| Expecting `const` to make a variable a compile-time constant in C11 | In C11/C17, `const` creates a read-only variable in memory, NOT a constant expression (creates a VLA in array sizes). | Use an `enum` or C23 `constexpr`. |
| Implicitly passing `char **` to `const char **` | Compiler emits a warning/error because implicit conversion could allow violating const safety. | Use explicit cast or refactor signatures. |
| Modifying struct through a `const struct *` pointer | Compiler rejects compilation; all fields within a struct become transitively `const`. | Pass non-const pointer if mutation is required. |

---

## 8. Performance & Security Notes

- **Read-Only Memory Page Sharing**: Read-only segments (`.rodata`, `.text`) can be shared across multiple instances of a running process by the operating system virtual memory subsystem, saving physical RAM.
- **Compiler Invariant Optimization**: When a function takes a pointer to `const`, the compiler's optimizer knows that internal calls that do not take the pointer will not modify the data, enabling better register caching.

---

### Type-Based Alias Analysis (TBAA) with Const Qualifiers

In optimizing compilers, const pointers provide strong hints for alias analysis. If a function receives a const int *data alongside a mutable int *counter, and the compiler can determine they do not alias, it can hoist memory reads of *data outside of loops, keeping values pinned in CPU registers across calls that update *counter.

---

## 9. Comparison / When to Use What

| Qualifier Form | Can Mutate Address? | Can Mutate Data? | Typical Use Case |
|---|---|---|---|
| `const char *buf` | **Yes** (`buf++`) | **No** (`*buf = 'x'` fails) | Reading strings, input buffers |
| `char * const buf` | **No** (`buf++` fails) | **Yes** (`*buf = 'x'`) | Memory-mapped I/O registers |
| `const char * const buf` | **No** | **No** | Immutable fixed lookup pointers |
| `char *buf` | **Yes** | **Yes** | Output buffers, modifiable arrays |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the difference between `const int *p` and `int * const p`?</strong></summary>

- `const int *p`: Pointer to a constant integer. The integer data pointed to cannot be modified through `p`, but the pointer itself can be redirected to point to another memory address (`p++`).
- `int * const p`: Constant pointer to an integer. The pointer address is immutable and cannot point elsewhere, but the integer data it points to can be modified (`*p = 50`).
</details>

<details>
<summary><strong>2. What happens if you cast away `const` from a pointer to modify a string literal?</strong></summary>

String literals (like `"hello"`) are placed in the read-only `.rodata` segment by the compiler. Casting away `const` (e.g. `char *s = (char *)"hello"; s[0] = 'H';`) compiles without error, but triggers an immediate hardware segmentation fault (`SIGSEGV`) at runtime when the CPU attempts to write to a read-only page.
</details>

<details>
<summary><strong>3. Why does standard C forbid implicitly converting `char **` to `const char **`?</strong></summary>

To prevent a subtle violation of `const` safety without an explicit cast. If `char **` could implicitly convert to `const char **`, a program could assign the address of a `const char` object to an inner pointer without casting, and then modify that `const` object through the original `char *` pointer!
</details>

<details>
<summary><strong>4. Does `const` provide compile-time type safety, runtime memory protection, or both?</strong></summary>

Both. At compile time, the compiler's semantic analyzer rejects any assignment to a `const`-qualified lvalue. At runtime, objects with static storage duration qualified with `const` are mapped into physical `.rodata` memory pages with write-protection flags enforced by the CPU MMU.
</details>

<details>
<summary><strong>5. In a struct qualified with `const`, are nested members also `const`?</strong></summary>

Yes. Constness in C is **transitive through structures**. If a struct variable is declared `const struct Point pt;`, every member within `pt` (such as `pt.x` and `pt.y`) becomes read-only and cannot be modified. However, if the struct contains a pointer, only the pointer itself is const, not the memory it points to!
</details>

<details>
<summary><strong>6. What is the difference between `const` in C and `const` in C++?</strong></summary>

In C++, a `const` variable with an initializer is a true compile-time constant expression by default, and has internal linkage unless explicitly marked `extern`. In C (prior to C23), a `const` variable is merely a read-only variable stored in memory, has external linkage by default at file scope, and cannot be used in case labels or non-VLA array sizes.
</details>

<details>
<summary><strong>7. How does the "clockwise/spiral rule" help parse declarations like `int * const *p`?</strong></summary>

Reading from the identifier outwards: `p` is a pointer to a `const pointer` to an `int`.
</details>

<details>
<summary><strong>8. Why should you always declare string literals as `const char *` rather than `char *`?</strong></summary>

While historical C permitted `char *s = "text";` for backward compatibility with 1970s code, string literals are immutable. Declaring them `const char *` ensures that any attempt to modify the string is caught at compile time by the compiler rather than crashing at runtime with a segmentation fault.
</details>

---

<details>
<summary><strong>9. Can an object be both `const` and `volatile` simultaneously?</strong></summary>

Yes! A common real-world example is a hardware status register in embedded systems:
```c
extern const volatile uint32_t * const UART_STATUS_REG;
```
- `const`: The user software program is forbidden from writing to the register.
- `volatile`: The hardware peripheral can change the register value at any moment, so the compiler must re-read physical memory on every access and not cache the value in a CPU register.
</details>

<details>
<summary><strong>10. Does returning a `const` value (e.g. `const int get_val(void)`) have any effect in C?</strong></summary>

No. In C, values are returned by value (as rvalues). Applying `const` to a scalar return type like `const int` has no practical effect because the caller receives a temporary copy that is already an rvalue. It merely clutters the function signature and is considered an anti-pattern.
</details>

<details>
<summary><strong>11. How does `const` qualification interact with compound literals?</strong></summary>

A compound literal can be qualified with `const` to create a read-only unnamed object:
```c
const int *p = (const int[]){ 10, 20, 30 };
```
Attempting to modify elements through `p[0] = 5;` is rejected at compile time.
</details>

<details>
<summary><strong>12. What does `const` mean when applied to an array parameter in a function declaration?</strong></summary>

In C, an array parameter decays into a pointer. Therefore, `void print(const int arr[])` is syntactically equivalent to `void print(const int *arr)`. It means the elements of the array cannot be modified through `arr`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Declaring Constant Pointers</strong></summary>

**Goal**: Write declarations for:
1. A pointer to a read-only double.
2. A constant pointer to a mutable double.
3. A constant pointer to a read-only double.

**Solution**:
```c
const double *ptr1;         /* 1. Pointer to const double */
double * const ptr2 = &val; /* 2. Const pointer to double */
const double * const ptr3 = &val; /* 3. Const pointer to const double */
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Const-Correct String Length Function</strong></summary>

**Goal**: Implement a custom `my_strlen()` function that adheres strictly to const-correctness, ensuring it can accept both mutable and immutable strings without warnings.

**Solution**:
```c
#include <stddef.h>

size_t my_strlen(const char *s) {
    if (!s) return 0;
    const char *p = s;
    while (*p != '\0') {
        p++;
    }
    return (size_t)(p - s);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Fixing Multi-Level Pointer Const Violations</strong></summary>

**Goal**: You have an array of mutable strings `char *argv[]`. Write a function that accepts this array and prints all arguments without modifying them, fixing any pointer qualifier warnings properly.

**Solution**:
In standard C, passing `char *argv[]` (which decays to `char **`) to a function expecting `const char * const *` requires an explicit cast, or a const-pointer signature:
```c
#include <stdio.h>

void print_args(size_t argc, char * const argv[]) {
    for (size_t i = 0; i < argc; ++i) {
        printf("Arg %zu: %s\n", i, argv[i]);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- `const int *p`: Pointer to read-only data (data immutable; pointer mutable).
- `int * const p`: Const pointer (data mutable; pointer address immutable).
- Use `const` on all input pointer parameters to document APIs and prevent bugs.
- Always declare string literals as `const char *str = "literal";`.
- Never cast away `const` to modify an object that was originally defined `const`.
- `const` static objects live in read-only `.rodata` memory pages.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 6.7.3: "Type qualifiers".
- Dan Saks: *Const T vs T const* (Embedded Systems Programming).
- SEI CERT C Coding Standard: *EXP05-C. Do not cast away a const or volatile qualification for a pointer*.

---

[← Previous: 11-sizeof-and-alignment-basics.md](./11-sizeof-and-alignment-basics.md) | [Section Index](./README.md) | [Next: 13-integer-promotions-and-usual-arithmetic-conversions.md](./13-integer-promotions-and-usual-arithmetic-conversions.md) →
