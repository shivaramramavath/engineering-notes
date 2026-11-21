# Design Patterns in C: Object-Oriented and Architectural Idioms

> Implement classic Gang of Four (GoF) design patterns—Strategy, Factory, Observer, Iterator, State Machine, and Polymorphic Vtables—in pure procedural C.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [02-core-concepts/03-pointers.md](file:///E:/Learning/engineering-notes/01-languages/c/02-core-concepts/03-pointers.md), [03-intermediate/14-callbacks-and-function-pointer-tables.md](file:///E:/Learning/engineering-notes/01-languages/c/03-intermediate/14-callbacks-and-function-pointer-tables.md), [01-modular-design-in-c.md](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/01-modular-design-in-c.md) |
| **Time to Read** | 30 minutes |
| **Status** | Complete |
| **Standard Baseline** | C11 / C17 / C23 |

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
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Object-Oriented Principles Without C++
Design patterns capture proven solutions to recurring software engineering problems. While popularized in object-oriented languages like Smalltalk and C++, design patterns are fundamentally architectural paradigms, not language syntax.

In procedural C:
- **Encapsulation:** Implemented via opaque pointer handles and translation unit separation.
- **Inheritance:** Implemented via struct embedding (composition) and pointer casting.
- **Polymorphism:** Implemented via function pointer tables (virtual method tables or *vtables*).

Large production systems written in C—including the Linux kernel Virtual Filesystem Switch (VFS), GTK/GObject, COM/Win32 APIs, and network protocol stacks—rely entirely on procedural design patterns to achieve pluggable, extensible architectures.

---

## 2. Core Concepts

### 1. The Strategy Pattern
Decouples an algorithm from the host object that uses it, allowing the algorithm to vary independently at runtime. In C, a Strategy is simply a function pointer or a struct of function pointers passed to a processing routine.

### 2. The Virtual Method Table (Vtable) Pattern
To achieve runtime polymorphism (dynamic dispatch), objects embed a pointer to a read-only table of function pointers. All instances of a class share one static vtable in `.rodata`, minimizing memory footprint.

```
       +------------------------------------+
       |   Instance: Shape (Circle)         |
       +------------------------------------+
       | const ShapeVTable *vptr ----------+-----> +--------------------------+
       | double radius = 5.0                |      | struct ShapeVTable       |
       +------------------------------------+      +--------------------------+
                                                   | double (*area)(Shape*)   |
       +------------------------------------+      | void (*draw)(Shape*)     |
       |   Instance: Shape (Rectangle)      |      +--------------------------+
       +------------------------------------+                   ^
       | const ShapeVTable *vptr -----------+-------------------+
       | double width = 4.0                 |
       | double height = 6.0                |
       +------------------------------------+
```

### 3. Struct Embedding (Single Inheritance)
The C standard guarantees that a pointer to a struct can be safely cast to a pointer to its first member without padding offset. By embedding the "base" struct as the very first member of a "derived" struct, C achieves single inheritance:
```c
struct Animal {
    int age;
    void (*speak)(struct Animal *self);
};

struct Dog {
    struct Animal base; /* MUST be the first member */
    const char *breed;
};

/* Safe conversion */
struct Dog my_dog;
struct Animal *a = (struct Animal*)&my_dog;
```

---

## 3. How It Works Internally

### Memory Layout and Upcasting Mechanics
The ISO C standard (Section 6.7.2.1) explicitly states: *A pointer to a structure object, suitably converted, points to its initial member. There may be unnamed padding within a structure object, but not at its beginning.*

```
Physical Memory Representation:

&my_dog (Base Address: 0x1000)
+-------------------------------------------------------+
| struct Animal base (Offset 0)                         |
| +---------------------------------------------------+ |
| | const AnimalVTable *vptr (8 bytes)   [Offset 0]   | |
| | int age                  (4 bytes)   [Offset 8]   | |
| | char padding             (4 bytes)   [Offset 12]  | |
| +---------------------------------------------------+ |
| const char *breed          (8 bytes)   [Offset 16]    |
+-------------------------------------------------------+

Because &my_dog == &(my_dog.base) == 0x1000:
(struct Animal*)&my_dog points directly to the valid Animal layout!
```

### Vtable Dispatch Assembly
When invoking a polymorphic method:
```c
shape->vptr->draw(shape);
```
The compiler emits an indirect call through two pointer dereferences:
```assembly
mov rax, [rdi]        ; Load vptr from Shape struct (offset 0)
mov rax, [rax + 8]    ; Load draw function pointer from Vtable (offset 8)
call rax              ; Indirect call
```

---

## 4. Syntax & Usage

### 1. Polymorphic Interface with Vtable

```c
#include <stdio.h>

/* Forward declaration */
typedef struct Shape Shape;

/* Virtual Table Definition */
typedef struct {
    double (*area)(const Shape *self);
    void (*draw)(const Shape *self);
} ShapeVTable;

/* Base Class Definition */
struct Shape {
    const ShapeVTable *vptr;
};

/* Derived Class: Circle */
typedef struct {
    Shape base; /* Inheritance via first member */
    double radius;
} Circle;

static double circle_area(const Shape *self) {
    const Circle *c = (const Circle*)self;
    return 3.1415926535 * c->radius * c->radius;
}

static void circle_draw(const Shape *self) {
    const Circle *c = (const Circle*)self;
    printf("Drawing Circle with radius: %.2f\n", c->radius);
}

/* Static Vtable in .rodata */
static const ShapeVTable CIRCLE_VTABLE = {
    .area = circle_area,
    .draw = circle_draw
};

void circle_init(Circle *c, double r) {
    c->base.vptr = &CIRCLE_VTABLE;
    c->radius = r;
}
```

### 2. The Observer Pattern
```c
#define MAX_OBSERVERS 8

typedef void (*EventCallback)(int event_id, void *data, void *context);

typedef struct {
    EventCallback callback;
    void *context;
} Observer;

typedef struct {
    Observer observers[MAX_OBSERVERS];
    size_t count;
} EventEmitter;

void event_emitter_subscribe(EventEmitter *ee, EventCallback cb, void *ctx) {
    if (ee->count < MAX_OBSERVERS) {
        ee->observers[ee->count++] = (Observer){ .callback = cb, .context = ctx };
    }
}

void event_emitter_publish(const EventEmitter *ee, int event_id, void *data) {
    for (size_t i = 0; i < ee->count; ++i) {
        ee->observers[i].callback(event_id, data, ee->observers[i].context);
    }
}
```

---

## 5. Practical Examples

### Comprehensive Verification Suite: Strategy Pattern and Dispatch in C

The following test program demonstrates dynamic strategy switching and functional dispatching:

```c
#include <stdio.h>

/* Strategy Pattern in C */
typedef int (*FilterStrategy)(int);

static int filter_even(int x) { return (x % 2 == 0); }
static int filter_positive(int x) { return (x > 0); }

void process_array(const int *arr, size_t len, FilterStrategy strategy, const char *name) {
    printf("Applying Strategy [%s]: ", name);
    for (size_t i = 0; i < len; ++i) {
        if (strategy(arr[i])) {
            printf("%d ", arr[i]);
        }
    }
    printf("\n");
}

int main(void) {
    printf("Evaluating Classic GoF Design Patterns in Procedural C...\n");
    int data[] = {-3, -2, -1, 0, 1, 2, 3, 4, 5, 6};
    size_t n = sizeof(data) / sizeof(data[0]);

    process_array(data, n, filter_even, "Even Numbers");
    process_array(data, n, filter_positive, "Positive Numbers");
    return 0;
}
```

```
Evaluating Classic GoF Design Patterns in Procedural C...
Applying Strategy [Even Numbers]: -2 0 2 4 6 
Applying Strategy [Positive Numbers]: 1 2 3 4 5 6 
```

### Explanation
- The strategy function pointer `FilterStrategy` defines a dynamic algorithm contract.
- `process_array` accepts different filtering behaviors at runtime without modifying its internal processing loop.

---

## 6. Best Practices

1. **Place Vtables in Static Read-Only Memory:** Always declare vtables `static const`. This places the table in the `.rodata` section, protecting function pointers from accidental corruption or malicious overwrite.
2. **Embed Base Struct as the Absolute First Member:** Never place private members or padding before the base struct member; doing so breaks pointer upcasting.
3. **Always Pass Context Pointers in Callbacks:** A function pointer callback in C is useless without state. Always provide a `void *user_data` or `void *context` pointer parameter.
4. **Use `container_of` Macro for Non-Initial Struct Embedding:** If an interface cannot be the first member, use the Linux kernel `container_of` offset macro to calculate the derived pointer safely.
5. **Implement Factory Functions for Polymorphic Instantiation:** Never let clients initialize complex polymorphic structs manually; provide factory constructors (e.g., `shape_create_circle(r)`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Placing Base Struct Not at Offset 0** | Casting `(Base*)derived` yields an invalid pointer that points to whatever field was placed first. | Ensure `struct Base base;` is strictly the very first declared member. |
| **Storing Function Pointers Per Instance** | Storing raw function pointers in every object instance bloats memory (8 bytes per method per object). | Group methods into a shared `const VTable *vptr` stored once per object. |
| **Missing `user_data` in Callbacks** | Clients cannot pass stateful data into the callback, forcing dangerous global variables. | Always declare callback signatures as `void (*cb)(Event e, void *user_data)`. |
| **Modifying Vtables at Runtime** | Dynamic mutation of vtables causes race conditions in multi-threaded code and invites exploits. | Mark all vtable definitions `static const`. |
| **Manual Downcasting Without Type Tags** | Casting `(Circle*)shape` when the shape is actually a `Rectangle` leads to memory corruption. | Include an enum type tag (`ShapeType type;`) in the base struct to validate downcasts. |
| **Neglecting Virtual Destructors** | Freeing a derived object through a base pointer (`free(base_ptr)`) fails to release derived resources. | Include a virtual destructor `void (*destroy)(Shape *self)` in the vtable. |

---

## 8. Performance & Security Notes

### Performance Overhead
- **Indirect Branch Misprediction:** Vtable calls execute indirect jumps (`call rax`). If the branch target changes frequently, the CPU Branch Target Buffer (BTB) mispredicts, incurring a 15–20 cycle pipeline flush penalty.
- **Inlining Suppression:** Compilers cannot inline indirect function calls through vtables unless Link-Time Optimization (LTO) performs speculative devirtualization.

### Security Implications
- **Vtable Hijacking:** If an attacker overwrites an object's `vptr` via a heap buffer overflow, subsequent method dispatches jump to attacker-controlled shellcode. Marking vtables `const` in `.rodata` mitigates table modification, but object pointer validation is vital.

---

## 9. Comparison / When to Use What

| Pattern | Complexity | Dispatch Mechanism | Memory Overhead | Best Suited For |
| :--- | :--- | :--- | :--- | :--- |
| **Strategy (Function Pointer)** | Low | Single indirect call | 8 bytes per strategy | Pluggable sorting, filtering, or hashing algorithms. |
| **Polymorphic Vtable** | Medium | Table indirect call | 8 bytes (`vptr`) per object | Heterogeneous object hierarchies (GUI widgets, VFS drivers). |
| **Tag + Switch (Discriminated Union)**| Very Low | Direct jumps (Jump Table) | Enum tag (4 bytes) | Fixed, closed sets of variants where new types are rarely added. |
| **State Machine Table** | Medium | Array lookup | Static state table | Network protocol parsers, game state logic, regex engines. |

---

## 10. Interview Questions

<details>
<summary><strong>1. How does procedural C achieve single inheritance with zero runtime overhead?</strong></summary>
By embedding the base struct as the first member of the derived struct (`struct Derived { struct Base base; ... };`). The C standard guarantees that the address of a struct equals the address of its first member, permitting zero-cost pointer casting between derived and base types.
</details>

<details>
<summary><strong>2. What is a virtual table (vtable) and why is it preferred over storing function pointers inside each struct?</strong></summary>
A vtable is a single table of function pointers shared across all instances of a class. Storing function pointers directly inside every struct instance wastes memory (e.g., 10 methods $\times$ 8 bytes = 80 bytes per object). A vtable pointer consumes only 8 bytes per object regardless of method count.
</details>

<details>
<summary><strong>3. How does the Linux kernel's `container_of` macro enable polymorphism?</strong></summary>
`container_of(ptr, type, member)` uses `offsetof` to compute the base address of an enclosing structure from a pointer to an internal embedded member. This allows objects to embed interface structures anywhere in their layout, not just at offset 0.
</details>

<details>
<summary><strong>4. What is the role of a virtual destructor in a C polymorphic hierarchy?</strong></summary>
If derived objects allocate dynamic memory (e.g., internal arrays), deleting an object through a base pointer (`free(base)`) leaks the derived memory. A virtual destructor (`self->vptr->destroy(self)`) ensures derived cleanup routines execute before memory deallocation.
</details>

<details>
<summary><strong>5. Why is the Strategy pattern frequently implemented with a `void *user_data` pointer?</strong></summary>
Because C functions cannot capture lexical scope (closures). The `void *user_data` parameter allows callers to pass state or context into the strategy callback, making it reusable across diverse callers.
</details>

<details>
<summary><strong>6. How does the State Machine pattern reduce cyclomatic complexity in network parsers?</strong></summary>
Instead of deeply nested `if`/`switch` statements, the State Machine pattern uses a 2D matrix of function pointers `action[current_state][event]`. State transitions and actions are executed via a single table lookup, making transitions $O(1)$ and easy to extend.
</details>

<details>
<summary><strong>7. How does the Factory pattern assist in managing opaque types?</strong></summary>
Since clients cannot stack-allocate opaque types whose sizes are unknown, factory functions (e.g., `widget_create_button()`) encapsulate heap allocation, vtable binding, and default parameter initialization behind a clean public API.
</details>

<details>
<summary><strong>8. Can compilers inline virtual method calls in C?</strong></summary>
Generally no, because the call target is resolved at runtime via a pointer dereference. However, with Link-Time Optimization (LTO) and Profile-Guided Optimization (PGO), compilers can perform *indirect call devirtualization*, replacing the call with a direct jump if one target dominates.
</details>

<details>
<summary><strong>9. What is the Iterator pattern in C and how is it structured?</strong></summary>
An Iterator provides sequential access to elements of an aggregate object without exposing its underlying representation. It typically defines an interface with `has_next()`, `next()`, and `reset()` function pointers.
</details>

<details>
<summary><strong>10. How does the Linux VFS (Virtual Filesystem) implement object-oriented file operations?</strong></summary>
The Linux VFS defines `struct file_operations`, which contains function pointers (`read`, `write`, `open`, `mmap`, etc.). Every filesystem driver (ext4, btrfs, NFS) provides its own static instance of `file_operations`, dynamically binding driver behavior to generic file descriptors.
</details>

---

## 11. Practice Exercises

### Exercise 1: Building a Pluggable Comparator Strategy for Generic Quicksort (Easy)
Write a generic sorting function that accepts an array of strings and a comparison strategy function pointer (`str_compare_asc` vs `str_compare_desc`).

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <string.h>

typedef int (*StrCompare)(const char *a, const char *b);

static int cmp_asc(const char *a, const char *b) { return strcmp(a, b); }
static int cmp_desc(const char *a, const char *b) { return strcmp(b, a); }

void bubble_sort_strings(const char **arr, size_t n, StrCompare cmp) {
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n - i - 1; ++j) {
            if (cmp(arr[j], arr[j + 1]) > 0) {
                const char *tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }
}

int main(void) {
    const char *names[] = {"banana", "apple", "cherry", "date"};
    bubble_sort_strings(names, 4, cmp_asc);
    printf("Sorted Asc: %s, %s, %s, %s\n", names[0], names[1], names[2], names[3]);
    bubble_sort_strings(names, 4, cmp_desc);
    printf("Sorted Desc: %s, %s, %s, %s\n", names[0], names[1], names[2], names[3]);
    return 0;
}
```
</details>

### Exercise 2: Implementing a Generic Iterator Pattern over a Linked List (Medium)
Construct an Iterator interface (`has_next`, `next`) that walks an internal singly linked list without exposing `struct Node` to client code.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct Node Node;
struct Node { int val; Node *next; };

typedef struct {
    Node *current;
} ListIterator;

bool iter_has_next(const ListIterator *it) {
    return it->current != NULL;
}

int iter_next(ListIterator *it) {
    if (!it->current) return -1;
    int val = it->current->val;
    it->current = it->current->next;
    return val;
}

int main(void) {
    Node n3 = {30, NULL};
    Node n2 = {20, &n3};
    Node n1 = {10, &n2};

    ListIterator it = { &n1 };
    printf("Iterating: ");
    while (iter_has_next(&it)) {
        printf("%d ", iter_next(&it));
    }
    printf("\n");
    return 0;
}
```
</details>

### Exercise 3: Designing a Table-Driven State Machine for Protocol Framing (Hard)
Implement a finite state machine using a 2D function pointer table to parse a simple packet format: `[HEADER (0xAA)] [LENGTH] [PAYLOAD...] [CHECKSUM]`.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum { STATE_WAIT_HEADER, STATE_READ_LEN, STATE_READ_DATA, STATE_VERIFY } State;

typedef struct {
    State state;
    uint8_t length;
    uint8_t bytes_read;
    uint8_t buffer[64];
} ParserFSM;

void fsm_init(ParserFSM *fsm) {
    fsm->state = STATE_WAIT_HEADER;
    fsm->length = 0;
    fsm->bytes_read = 0;
}

void fsm_feed_byte(ParserFSM *fsm, uint8_t byte) {
    switch (fsm->state) {
        case STATE_WAIT_HEADER:
            if (byte == 0xAA) {
                fsm->state = STATE_READ_LEN;
            }
            break;
        case STATE_READ_LEN:
            fsm->length = byte;
            fsm->bytes_read = 0;
            fsm->state = (byte > 0) ? STATE_READ_DATA : STATE_VERIFY;
            break;
        case STATE_READ_DATA:
            fsm->buffer[fsm->bytes_read++] = byte;
            if (fsm->bytes_read >= fsm->length) {
                fsm->state = STATE_VERIFY;
            }
            break;
        case STATE_VERIFY:
            printf("Packet fully received! Length: %d, Checksum: 0x%02X\n", fsm->length, byte);
            fsm->state = STATE_WAIT_HEADER;
            break;
    }
}

int main(void) {
    ParserFSM fsm;
    fsm_init(&fsm);
    uint8_t packet[] = {0xAA, 0x03, 'A', 'B', 'C', 0x55};
    for (size_t i = 0; i < sizeof(packet); ++i) {
        fsm_feed_byte(&fsm, packet[i]);
    }
    return 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Polymorphism in C:** Place `const VTable *vptr` in the base struct. Store all vtables `static const` in `.rodata`.
- **Inheritance in C:** Embed `struct Base base;` as the **first** member of the derived struct to ensure zero-cost upcasting.
- **Strategy Pattern:** Pass function pointers with `void *context` for customizable algorithms.
- **Virtual Destructors:** Always declare `void (*destroy)(Base *self)` in vtables to properly deallocate derived memory.
- **State Machine:** Use table-driven dispatch or structured enum switches for deterministic state transitions.

---

## 13. Further Reading
- Erich Gamma, Richard Helm, Ralph Johnson, John Vlissides: *Design Patterns: Elements of Reusable Object-Oriented Software*.
- Linux Kernel Source: *Virtual Filesystem Switch (`include/linux/fs.h` `struct file_operations`)*.
- Axel-Tobias Schreiner: *Object-Oriented Programming with ANSI-C*.
- GObject Reference Manual: *The GLib Object System Architecture*.

---

| [Previous: 01-modular-design-in-c.md](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/01-modular-design-in-c.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/README.md) | [Next: 03-error-handling-patterns.md](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/03-error-handling-patterns.md) |
