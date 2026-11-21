# Function Pointers: Dynamic Dispatch, Callbacks, and Polymorphism

> Dynamic execution dispatch in C: function pointer syntax, callback architecture with context pointers, standard comparators, and object-oriented polymorphism emulation.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [Pointers](../02-core-concepts/03-pointers.md), [Declarations and Typedef](../02-core-concepts/04-declarations-typedef.md), [Complex Declarations](../02-core-concepts/24-complex-declarations-reading-guide.md) |
| **Time to Read** | 25 minutes |
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

In C, functions are not objects that can be copied or passed by value. However, compiled functions reside at concrete memory addresses in the `.text` executable code segment. A **Function Pointer** is a variable that stores the entry point address of a function, allowing programs to choose *which* code to execute dynamically at runtime.

Function pointers enable essential systems programming patterns:
1. **Callbacks and Inversion of Control**: A generic sorting algorithm (`qsort`) or event loop can execute caller-supplied behavior without knowing its implementation.
2. **Reentrancy and State Abstraction**: By pairing function pointers with a `void *user_data` context pointer, C programs emulate closures and stateful objects without global variables.
3. **Table-Driven Dispatch and State Machines**: Eliminating enormous, slow `switch` statements by indexing directly into an array of function pointers.
4. **Object-Oriented Polymorphism (vtables)**: Implementing interfaces, drivers, and virtual method tables directly in pure C.

---

## 2. Core Concepts

### Function Pointer Syntax and Type Signature
A function pointer's type consists of its **return type**, the parenthesized pointer name `(*name)`, and its **parameter types**:
```c
int add(int a, int b) { return a + b; }

/* Pointer declaration */
int (*op)(int, int) = add; /* Function name automatically decays to its address */

/* Invocation */
int res1 = op(10, 20);     /* Implicit dereference (Standard C) */
int res2 = (*op)(10, 20);   /* Explicit dereference (Identical) */
```

### The Universal Callback Pattern (`void *context`)
Because C lacks nested closures, callback functions cannot implicitly access variables from their calling environment. Production C libraries solve this by always passing a **user context pointer** (`void *user_data`):
```c
typedef void (*EventCallback)(int event_id, void *user_data);

void register_event(int event_id, EventCallback cb, void *user_data);
```
When the event fires, the library passes `user_data` back to the callback, providing complete access to caller state without thread-unsafe global variables.

### Standard Library Comparators (`qsort` & `bsearch`)
The standard library functions `qsort()` and `bsearch()` rely on generic comparison function pointers:
```c
int (*compar)(const void *a, const void *b);
```
- Returns `< 0` if `a < b`.
- Returns `0` if `a == b`.
- Returns `> 0` if `a > b`.
Crucially, comparators must **never** return `a - b` because integer subtraction can overflow, causing inverted sorting or undefined behavior. Use the three-way comparison idiom: `(a > b) - (a < b)`.

---

## 3. How It Works Internally

### Indirect CPU Jump vs. Direct Call

```
Direct Call: add(10, 20)
  Assembly: call 0x401120 <add>
  (Target address is hardcoded directly into the opcode)

Indirect Call: op(10, 20)
  op holds memory address: 0x401120
  Assembly:
    mov rax, QWORD PTR [rip+op] ; Load function address into register
    call rax                    ; Indirect call through register
```

### Vtable Emulation in C

```
struct AnimalVTable {
    void (*speak)(void *instance);
    void (*move)(void *instance);
};

struct Animal {
    const struct AnimalVTable *vptr; /* Pointer to static vtable */
    char name[32];
};
```
Each animal instance contains a pointer to a shared, static vtable in `.rodata`. Calling `dog->vptr->speak(dog)` executes dynamic dispatch identical to C++ virtual methods.

---

## 4. Syntax & Usage

```c
#include <stdio.h>

/* Clean typedef for binary operation */
typedef int (*BinOp)(int, int);

/* Returning a function pointer */
BinOp select_operation(char op) {
    switch (op) {
        case '+': return add;
        case '-': return sub;
        default:  return NULL;
    }
}
```

> [!NOTE]
> ### Function Name Decay
> In C, a function identifier in an expression automatically decays into a pointer to that function: `op = add;` is strictly identical to `op = &add;`. Similarly, `op(1, 2)` is identical to `(*op)(1, 2)`. Modern C style strongly prefers the cleaner syntax without `&` and `*`.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates the context-pointer callback pattern, safe `qsort` integer comparator implementation, and binary searching with `bsearch`.

```c
#include <stdio.h>
#include <stdlib.h>

/* Callback typedef with user context pointer */
typedef void (*ItemCallback)(int value, void *user_data);

static void array_for_each(const int *arr, size_t n, ItemCallback cb, void *ctx) {
    for (size_t i = 0; i < n; ++i) {
        cb(arr[i], ctx);
    }
}

static void sum_accumulator(int value, void *ctx) {
    long *total = (long *)ctx;
    *total += value;
}

static int compare_ints(const void *a, const void *b) {
    int arg1 = *(const int *)a;
    int arg2 = *(const int *)b;
    return (arg1 > arg2) - (arg1 < arg2); /* Safe comparator avoiding overflow */
}

int main(void) {
    printf("=== C Function Pointers & Callbacks Demo ===\n");

    int nums[5] = {40, 10, 50, 20, 30};
    
    /* 1. Callback with user context */
    long total = 0;
    array_for_each(nums, 5, sum_accumulator, &total);
    printf("Accumulated sum via callback: %ld\n", total);

    /* 2. Standard qsort */
    qsort(nums, 5, sizeof(nums[0]), compare_ints);
    printf("Sorted array: ");
    for (size_t i = 0; i < 5; ++i) {
        printf("%d ", nums[i]);
    }
    printf("\n");

    /* 3. Standard bsearch */
    int key = 30;
    int *found = bsearch(&key, nums, 5, sizeof(nums[0]), compare_ints);
    printf("bsearch for %d: %s\n", key, found ? "FOUND" : "NOT FOUND");

    return 0;
}
```

```text
=== C Function Pointers & Callbacks Demo ===
Accumulated sum via callback: 150
Sorted array: 10 20 30 40 50 
bsearch for 30: FOUND
```

---

## 6. Best Practices

1. **Always Use `typedef` for Function Pointers**: Raw function pointer syntax is unreadable. Defining `typedef void (*ActionFn)(int);` makes APIs self-documenting.
2. **Always Provide a `void *user_data` Parameter in Callback APIs**: Never design a callback interface that takes only data without a user context pointer. Omitting context forces users to resort to non-reentrant, thread-unsafe global variables.
3. **Never Subtract Integers in `qsort` Comparators**: Writing `return a - b;` overflows when `a = INT_MAX` and `b = INT_MIN`, reversing sorting logic or invoking undefined behavior. Always use `(a > b) - (a < b)`.
4. **Always Check for `NULL` Before Invoking**: If a function pointer parameter is optional, verify `if (cb) cb(data);` before calling to avoid fatal null pointer dereference crashes.
5. **Mark Dispatch Tables `const`**: Static arrays of function pointers should be declared `static const ActionFn dispatch_table[] = { ... };` so they reside in read-only `.rodata`, preventing memory corruption attacks.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Omitting parentheses in declarator** | `int *f(int)` declares a function returning `int *` instead of a function pointer. | Enclose pointer in parentheses: `int (*f)(int)`. |
| **Overflow in `qsort` comparator** | `return a - b` overflows on large signed differences, corrupting sorting. | Use safe comparison: `(a > b) - (a < b)`. |
| **Calling uninitialized function pointer** | Causes immediate CPU instruction fault or wild code execution jump (`SIGSEGV`). | Initialize to `NULL` and verify `if (fn)` before calling. |
| **Omitting `void *context` in callback API** | Forces callers to use global state, making the library thread-unsafe and non-reentrant. | Always add `void *user_data` to callback signatures. |
| **Casting incompatible function pointer types** | Calling a function through a pointer with mismatched parameters invokes undefined behavior. | Ensure parameter types match the signature exactly. |
| **Mutable function pointer arrays in `.data`** | Writable function tables are prime targets for buffer overflow arbitrary execution attacks. | Qualify dispatch tables with `const`. |

---

## 8. Performance & Security Notes

### Branch Prediction and Compiler Inlining
Direct function calls (`add()`) can be inlined by optimizing compilers (`-O2`), completely eliminating call overhead. Calls through function pointers (`op()`) require an indirect branch instruction (`call rax`). On modern CPUs, indirect branches rely on the Branch Target Buffer (BTB); unpredictable indirect targets cause pipeline flushes.

### Security: Control Flow Hijacking
Function pointers are the primary target of memory exploitation. If an attacker can overwrite a function pointer (e.g., via a heap buffer overflow), the subsequent indirect call redirects execution to injected shellcode or ROP gadgets. Modern operating systems use Control Flow Integrity (CFI) and write-protected vtables to mitigate this threat.

---

## 9. Comparison: When to Use What

| Technique | Inlining Possible? | Runtime Overhead | Flexibility | Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Direct Call** | Yes (Full inlining) | 0 ns | Fixed at compile time | Core math, fixed internal helpers |
| **Function Pointer** | Rarely | ~2–5 ns (Indirect call) | Fully dynamic | Callbacks, plugins, `qsort` |
| **Macro Expansion** | Yes | 0 ns | Code duplication | X-Macros, generic wrappers |
| **Switch Dispatch** | Partial | Jump table overhead | Fixed enum cases | Packet opcode handlers, state machines |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is <code>return a - b;</code> dangerous in a <code>qsort</code> comparison function?</strong></summary>

If `a` is a large positive integer (e.g., `2,000,000,000`) and `b` is a large negative integer (e.g., `-2,000,000,000`), the mathematical difference `4,000,000,000` exceeds `INT_MAX` (2,147,483,647 on 32-bit `int`). In C, signed integer overflow is Undefined Behavior and wraps to a negative value, causing the comparator to incorrectly report that `a < b`.
</details>

<details>
<summary><strong>2. What is the purpose of the <code>void *user_data</code> parameter in callback functions?</strong></summary>

C does not support closures or lexical scoping for functions. If a callback needs access to caller-specific state (such as an accumulator, a GUI widget, or an open connection), without a user context parameter it would be forced to use global variables. A `void *user_data` pointer allows passing arbitrary instance state through the API safely and reentrantly.
</details>

<details>
<summary><strong>3. How does the C compiler know how to invoke a function pointer?</strong></summary>

The compiler loads the memory address stored inside the function pointer variable into a general-purpose CPU register (e.g., `rax` on x86-64), then emits an indirect call instruction: `call rax`.
</details>

<details>
<summary><strong>4. Can you convert a function pointer to a <code>void *</code> object pointer in standard ISO C?</strong></summary>

In strict ISO C (§6.3.2.3), converting between a function pointer and an object pointer (`void *`) is undefined behavior because on certain Harvard architectures, code memory and data memory have completely different address buses and pointer sizes. However, POSIX standard mandates that `sizeof(void *) == sizeof(void (*)())` to support dynamic loading via `dlsym()`.
</details>

<details>
<summary><strong>5. What is a virtual method table (vtable) and how is it implemented in C?</strong></summary>

A vtable is a structure containing function pointers representing the interface methods of an object. Each object instance stores a pointer to a static vtable (`const struct VTable *vptr;`). Method calls are dispatched dynamically: `obj->vptr->method(obj);`.
</details>

<details>
<summary><strong>6. How do you declare a function that returns a pointer to a function?</strong></summary>

Using raw declarator syntax:
`int (*select_fn(char op))(int, int);`
Using `typedef` (recommended):
`typedef int (*MathFn)(int, int); MathFn select_fn(char op);`
</details>

<details>
<summary><strong>7. What happens if a function pointer is invoked with mismatched arguments?</strong></summary>

Invoking a function through a pointer whose type is not compatible with the actual function definition invokes immediate Undefined Behavior. It typically causes stack corruption, register mismatches, or program termination.
</details>

<details>
<summary><strong>8. Is the address-of operator <code>&amp;</code> required when assigning a function to a function pointer?</strong></summary>

No. In C, an uncalled function identifier automatically decays into a pointer to that function. Therefore, `fp = my_func;` is identical to `fp = &my_func;`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Generic Array Filter (Easy)
Write a function `size_t array_filter(const int *src, size_t n, int *dst, bool (*predicate)(int))` that copies only elements satisfying the predicate into `dst`.

<details>
<summary>View Solution</summary>

```c
#include <stdbool.h>
#include <stddef.h>

size_t array_filter(const int *src, size_t n, int *dst, bool (*predicate)(int)) {
    size_t count = 0;
    for (size_t i = 0; i < n; ++i) {
        if (predicate(src[i])) {
            dst[count++] = src[i];
        }
    }
    return count;
}
```
</details>

### Exercise 2: String Sorter with Case-Insensitive Comparator (Medium)
Sort an array of string pointers alphabetically using standard `qsort` with a custom comparator that uses POSIX `strcasecmp` or a custom lowercase loop.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int compare_strings_case_insensitive(const void *a, const void *b) {
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    while (*s1 && *s2) {
        int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (diff != 0) return diff;
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int main(void) {
    const char *words[] = {"banana", "Apple", "cherry"};
    qsort(words, 3, sizeof(char *), compare_strings_case_insensitive);
    for (int i = 0; i < 3; i++) printf("%s ", words[i]);
    printf("\n");
    return 0;
}
```
</details>

### Exercise 3: Simulated Object-Oriented Shape Hierarchy (Hard)
Implement an object-oriented Shape interface in C using a struct of function pointers (`area`, `draw`). Implement a `Circle` and `Rectangle` struct, and write a polymorphic function `void render_shape(const void *shape)`.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>

typedef struct ShapeVTable {
    double (*area)(const void *self);
    void (*draw)(const void *self);
} ShapeVTable;

typedef struct Shape {
    const ShapeVTable *vptr;
} Shape;

/* Circle implementation */
typedef struct Circle {
    Shape base;
    double radius;
} Circle;

static double circle_area(const void *self) {
    const Circle *c = (const Circle *)self;
    return 3.14159265 * c->radius * c->radius;
}
static void circle_draw(const void *self) {
    const Circle *c = (const Circle *)self;
    printf("Drawing Circle(r=%.1f)\n", c->radius);
}
static const ShapeVTable circle_vtable = { circle_area, circle_draw };

Circle circle_create(double r) {
    return (Circle){ .base = { .vptr = &circle_vtable }, .radius = r };
}

void render_shape(const void *shape) {
    const Shape *s = (const Shape *)shape;
    s->vptr->draw(shape);
    printf("  Area = %.2f\n", s->vptr->area(shape));
}

int main(void) {
    Circle c = circle_create(5.0);
    render_shape(&c);
    return 0;
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Typedef declaration */
typedef int (*BinaryOp)(int a, int b);

/* Assignment and call */
BinaryOp op = add;
int res = op(10, 20);

/* Safe qsort comparator */
int cmp(const void *a, const void *b) {
    int v1 = *(const int *)a;
    int v2 = *(const int *)b;
    return (v1 > v2) - (v1 < v2);
}

/* Callback signature */
typedef void (*Callback)(int event, void *user_data);
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §6.7.6.3 "Function declarators".
- *The C Programming Language* (2nd ed.) by Kernighan & Ritchie (Chapter 5: "Pointers and Arrays", Function Pointers).
- Axel-Tobias Schreiner: *Object-Oriented Programming with ANSI-C*.
- POSIX.1-2017: `qsort`, `bsearch`, and `dlsym` specifications.

---

[← Previous Module: 02-core-concepts](../02-core-concepts/README.md) | [Section Index](./README.md) | [Next: Multi-File Programs →](./02-multi-file-programs.md)
