# Functions and Calling Conventions

> The mechanical anatomy of C functions: declarations vs prototypes, pass-by-value semantics, stack frames and activation records, calling conventions (System V AMD64 vs Microsoft x64), function pointers, and C23 empty parameter lists (`int f()` vs `int f(void)`).

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [01-program-structure.md](./01-program-structure.md), [00-setup/09-reading-assembly-godbolt.md](../00-setup/09-reading-assembly-godbolt.md) |
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

In C, functions are the fundamental unit of procedural abstraction. Unlike higher-level languages that pass objects by reference, support dynamic dispatch, or feature runtime reflection, C functions operate with minimal runtime ceremony:
1. **Strict Pass-by-Value Semantics**: C is **exclusively pass-by-value**. Every parameter passed to a function is copied into CPU registers or stack memory. To mutate an external caller's variable, you must explicitly pass a pointer to that variable.
2. **Calling Conventions Govern ABI Compatibility**: Compilers must adhere to rigid Application Binary Interface (ABI) agreements dictating which hardware registers hold arguments and which registers must be preserved across calls.
3. **The C23 Prototype Revolution**: For over 30 years (C89 through C17), `int func()` indicated an unprototyped function taking an unspecified number of parameters. **ISO C23 abolished this**, making `int func()` strictly identical to `int func(void)`.

---

## 2. Core Concepts

### Declarations, Prototypes, and Definitions

- **Function Declaration (Prototype)**: Specifies the function name, return type, and typed parameter list. Informs the compiler how to type-check call sites.
  ```c
  int calculate_crc(const uint8_t *data, size_t length);
  ```
- **Function Definition**: Contains the executable body and local variables.
  ```c
  int calculate_crc(const uint8_t *data, size_t length) {
      /* Executable statements */
      return 0;
  }
  ```

### The C23 Prototype Standard

| Standard Era | `int compute();` | `int compute(void);` |
|---|---|---|
| **C89 / C99 / C11 / C17** | Unspecified parameters (no type checking on arguments!) | Strictly **zero** arguments |
| **C23 (ISO/IEC 9899:2024)** | Strictly **zero** arguments (Identical to `(void)`) | Strictly **zero** arguments |

*Legacy K&R definitions* (such as `int f(a, b) int a; int b; { ... }`) were permanently **removed** from standard C in C23.

---

## 3. How It Works Internally

### Stack Frames and Activation Records

When a function call executes at the machine level, the CPU creates a new **Stack Frame** (Activation Record):

```
High Memory
+------------------------------------+
| Caller's Stack Frame               |
| Arguments 7+ (if > 6 arguments)    |
+------------------------------------+
| Return Address (pushed by 'call')  |  <-- Points to instruction after 'call'
+------------------------------------+
| Saved Frame Pointer (push %rbp)    |  <-- Current %rbp points here
+------------------------------------+
| Callee-Saved Registers Saved Here  |  <-- (%rbx, %r12 - %r15)
+------------------------------------+
| Local Variables & Spilled Values   |  <-- Allocated via sub $N, %rsp
+------------------------------------+
Low Memory (Stack grows DOWN)
```

1. **`call` instruction**: Pushes the address of the next sequential instruction (the return address) onto the stack and jumps to the function entry label.
2. **Prologue**:
   ```nasm
   push    rbp             ; Save caller's frame pointer
   mov     rbp, rsp        ; Set up new frame pointer
   sub     rsp, 32         ; Allocate stack space for local variables
   ```
3. **Epilogue**:
   ```nasm
   mov     rsp, rbp        ; Deallocate local variables
   pop     rbp             ; Restore caller's frame pointer
   ret                     ; Pop return address into %rip and jump back
   ```

### Calling Conventions: System V vs. Microsoft x64

| ABI Dimension | System V AMD64 (Linux / macOS / BSD) | Microsoft x64 (Windows) |
|---|---|---|
| **Integer/Pointer Regs** | `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9` (6 regs) | `%rcx`, `%rdx`, `%r8`, `%r9` (4 regs) |
| **Floating-Point Regs** | `%xmm0` through `%xmm7` (8 regs) | `%xmm0` through `%xmm3` (4 regs) |
| **Shadow Space** | **None** (Has 128-byte Red Zone below `%rsp`) | **Mandatory 32 bytes** allocated on stack by caller |
| **Return Register** | `%rax` (integer/pointer), `%xmm0` (float) | `%rax` (integer/pointer), `%xmm0` (float) |
| **Callee-Saved Regs** | `%rbx`, `%rsp`, `%rbp`, `%r12`, `%r13`, `%r14`, `%r15` | `%rbx`, `%rbp`, `%rdi`, `%rsi`, `%rsp`, `%r12`–`%r15` |

---

## 4. Syntax & Usage

### 1. Function Pointer Syntax and Type Aliases
Function pointers allow dynamic dispatch and callback architectures:

```c
#include <stdio.h>

/* Plain function pointer syntax */
int (*compare_func)(const void *, const void *);

/* Clean typedef idiom (Highly Recommended) */
typedef int (*comparator_t)(const void *a, const void *b);

int int_comparator(const void *a, const void *b) {
    int val_a = *(const int *)a;
    int val_b = *(const int *)b;
    return (val_a > val_b) - (val_a < val_b);
}
```

### 2. Static Inline Functions in Headers
```c
/* In header file */
static inline int max_int(int a, int b) {
    return (a > b) ? a : b;
}
```
Declaring `static inline` in a header file allows the compiler to inline the function directly at call sites while ensuring no duplicate global symbols are emitted into object files.

---

## 5. Practical Examples

### Function Semantics and Call Dispatch Demonstration Program

```c
/* File: 01-languages/c/01-fundamentals/examples/08-functions.c */
#include <stdio.h>

/* Pass-by-value demonstration */
static void swap_by_value(int a, int b) {
    int temp = a;
    a = b;
    b = temp;
    (void)a;
    (void)b;
}

/* Pass-by-reference simulation via pointers */
static void swap_by_pointer(int *a, int *b) {
    if (a && b) {
        int temp = *a;
        *a = *b;
        *b = temp;
    }
}

/* Function pointer definition */
typedef int (*binary_op_t)(int, int);

static int add(int x, int y) { return x + y; }
static int multiply(int x, int y) { return x * y; }

int main(void) {
    printf("=== Function Semantics & Call Dispatch Demo ===\n");
    int x = 10, y = 20;
    
    swap_by_value(x, y);
    printf("After swap_by_value: x = %d, y = %d (Unchanged!)\n", x, y);

    swap_by_pointer(&x, &y);
    printf("After swap_by_pointer: x = %d, y = %d (Swapped!)\n", x, y);

    /* Dynamic dispatch via function pointer */
    binary_op_t op = add;
    printf("Function pointer dispatch add(4, 5) = %d\n", op(4, 5));
    op = multiply;
    printf("Function pointer dispatch multiply(4, 5) = %d\n", op(4, 5));

    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 08-functions.c -o run_08.exe
./run_08.exe
```
Output:
```text
=== Function Semantics & Call Dispatch Demo ===
After swap_by_value: x = 10, y = 20 (Unchanged!)
After swap_by_pointer: x = 20, y = 10 (Swapped!)
Function pointer dispatch add(4, 5) = 9
Function pointer dispatch multiply(4, 5) = 20
```

---

## 6. Best Practices

1. **Always Use `(void)` for Functions Taking No Arguments in Pre-C23**: Write `int get_status(void);` rather than `int get_status();` to ensure strict parameter checking.
2. **Mark Internal Functions `static`**: Functions internal to a translation unit should always be `static` to enable inlining and avoid polluting the global linker namespace.
3. **Validate Pointer Parameters with Assertions or Null Checks**: Check pointer arguments before dereferencing (`if (!ptr) return -1;`).
4. **Use `const` for Read-Only Pointer Arguments**: Writing `int parse(const char *buffer)` documents intent and prevents accidental mutations.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Expecting caller variables to change without passing pointers | C is pass-by-value; parameters are local copies inside the callee stack frame. | Pass a pointer: `func(&my_var)`. |
| Returning a pointer to a local automatic variable | The stack frame is destroyed on return; the pointer points to dangling garbage. | Return by value, allocate on heap, or take an output pointer buffer. |
| Forgetting parentheses in function pointer declarations | `int *func(int)` declares a function returning an `int *`. `int (*func)(int)` declares a function pointer! | Use `(*func_ptr)` or declare with `typedef`. |
| Missing return statement in non-void function | Falling off the closing `}` of a non-void function invokes undefined behavior. | Ensure all control paths explicitly return a value. |
| Calling a function pointer that is `NULL` | Dereferencing a NULL function pointer triggers an immediate segmentation fault (`SIGSEGV`). | Check before invoking: `if (cb) cb(data);`. |
| Passing large structs by value | Copying a 512-byte struct across the stack degrades cache efficiency and burns CPU cycles. | Pass a pointer: `void process(const struct Large *s)`. |

---

## 8. Performance & Security Notes

- **Tail Call Optimization (TCO)**: When a function returns the direct result of another function call (`return helper(x);`), the compiler emits a `jmp` instead of a `call`, reusing the existing stack frame and eliminating stack overflow risks on recursive algorithms.
- **Buffer Overflows Overwriting the Return Address**: Stack smashing attacks write beyond the bounds of local arrays, overwriting the saved `%rip` (return address) on the stack frame to hijack execution control. Compilers protect against this with stack canaries (`-fstack-protector-strong`).

---

## 9. Comparison / When to Use What

| Technique | Dispatch Timing | Performance | Flexibility |
|---|---|---|---|
| **Direct Function Call** | Compile/Link Time | Fastest (inlinable, direct `call` or `jmp`) | Fixed at build time |
| **`static inline` Function** | Compile Time | Zero call overhead; inlined into call site | Fixed; headers only |
| **Function Pointer** | Runtime | Minor indirect branch latency | Pluggable callbacks, vtables |
| **Pre-C23 `int f()`** | Legacy | Unchecked argument types (Dangerous) | Obsolete (Removed in C23) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Does C support pass-by-reference?</strong></summary>

No. Standard C is **strictly pass-by-value**. What is colloquially called "pass-by-reference" in C is actually **passing a pointer by value**: the memory address of the object is copied into the function parameter register, allowing the callee to dereference that address and modify the caller's memory.
</details>

<details>
<summary><strong>2. How did the meaning of `void func()` change between C17 and C23?</strong></summary>

In C17 (and all prior standards back to C89), `void func();` declared an unprototyped function that accepted an unspecified number of parameters with no type checking. In C23, empty parentheses `void func();` are strictly identical to `void func(void);`, meaning the function accepts exactly zero arguments.
</details>

<details>
<summary><strong>3. What is a "stack frame" (or activation record), and what data does it store?</strong></summary>

A stack frame is a contiguous block of memory allocated on the call stack for a specific function invocation. It stores:
1. The return address of the caller.
2. The caller's saved frame pointer (`%rbp`).
3. Saved callee-preserved registers.
4. Local automatic variables.
5. Outgoing function arguments beyond the register allocation limit.
</details>

<details>
<summary><strong>4. What is the difference between caller-saved and callee-saved registers?</strong></summary>

- **Caller-saved registers** (volatile/scratch): The calling function assumes their values may be overwritten by the called function. If the caller needs their values after the call, it must save them to the stack before invoking the function.
- **Callee-saved registers** (non-volatile): The called function is required to preserve their values. If the callee modifies them, it must save them (typically in its prologue) and restore them before returning.
</details>

<details>
<summary><strong>5. What is the difference between `int *f(int)` and `int (*f)(int)`?</strong></summary>

`int *f(int)` declares a function named `f` that takes an `int` and returns a pointer to an `int` (`int *`). `int (*f)(int)` declares a function pointer variable named `f` that points to a function taking an `int` and returning a plain `int`.
</details>

<details>
<summary><strong>6. Why is returning a pointer to a local array considered undefined behavior?</strong></summary>

Local arrays have automatic storage duration, meaning their memory lifetime is tied to the stack frame of the function. When the function returns, its stack frame is marked as free. The returned pointer points to a dead stack region that will be overwritten by the next function call, leading to memory corruption.
</details>

<details>
<summary><strong>7. What is Tail Call Optimization (TCO), and what conditions allow it?</strong></summary>

TCO is an optimization where the compiler replaces a function call and subsequent return with an unconditional jump (`jmp`), reusing the current stack frame. It occurs when a function's return expression is the direct, unmanipulated return of calling another function (or itself in tail recursion).
</details>

<details>
<summary><strong>8. What is the 32-byte "Shadow Space" in the Microsoft x64 calling convention?</strong></summary>

In the Windows x64 ABI, the caller is required to reserve 32 bytes of stack space (4 quadwords) immediately above the return address before calling any function, even though the first 4 arguments are passed in registers (`%rcx`, `%rdx`, `%r8`, `%r9`). This provides the callee a designated scratch area to spill those register arguments into memory if needed.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Declaring a Clean Callback Typedef</strong></summary>

**Goal**: Declare a typedef `event_handler_t` for a function that takes an `int event_id`, a `const void *data`, and returns an `int` status code.

**Solution**:
```c
typedef int (*event_handler_t)(int event_id, const void *data);
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Implementing a Generic QuickSort Comparator</strong></summary>

**Goal**: Implement a standard conforming comparator function for standard C `qsort` that sorts an array of `double` values in descending order.

**Solution**:
```c
#include <stdlib.h>

int compare_doubles_descending(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    
    if (da < db) return 1;
    if (da > db) return -1;
    return 0;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Emulating a Dynamic Dispatch VTable</strong></summary>

**Goal**: Build a small object-oriented interface in C using a struct of function pointers (a VTable) to represent a polymorphic `Shape` with an `area` method.

**Solution**:
```c
#include <stdio.h>

typedef struct Shape Shape;

struct ShapeVTable {
    double (*area)(const Shape *self);
};

struct Shape {
    const struct ShapeVTable *vtable;
};

/* Concrete Circle */
typedef struct Circle {
    Shape base;
    double radius;
} Circle;

static double circle_area(const Shape *self) {
    const Circle *c = (const Circle *)self;
    return 3.1415926535 * c->radius * c->radius;
}

static const struct ShapeVTable circle_vtable = {
    .area = circle_area
};

Circle circle_create(double radius) {
    Circle c;
    c.base.vtable = &circle_vtable;
    c.radius = radius;
    return c;
}

int main(void) {
    Circle c = circle_create(5.0);
    Shape *s = (Shape *)&c;
    printf("Area of shape: %.2f\n", s->vtable->area(s));
    return 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Pass-by-value**: C parameters are copies; pass pointers to mutate caller state.
- **Empty Parens**: In C23, `f()` means `f(void)`. In C17, always write `f(void)`.
- **System V AMD64 ABI**: First 6 arguments in `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`.
- **Function Pointers**: Use `typedef int (*op_t)(int, int);` for readable callback types.
- **Static Functions**: Always mark file-local functions `static` to restrict linkage.
- Never return pointers to local automatic stack variables.

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17 Standard), Section 6.7.6.3: "Function declarators".
- Bryant & O'Hallaron: *Computer Systems: A Programmer's Perspective* (Chapter 3.7: Procedures).
- Michael Matz et al.: *System V Application Binary Interface AMD64 Architecture Processor Supplement*.

---

[← Previous: 07-control-flow.md](./07-control-flow.md) | [Section Index](./README.md) | [Next: 09-input-output.md](./09-input-output.md) →
