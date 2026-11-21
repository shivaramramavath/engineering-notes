# Ownership and Lifetime Conventions in C

> Prevent memory leaks and use-after-free bugs by establishing explicit ownership transfer contracts, borrowing idioms, and lifetime discipline in C systems.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [02-core-concepts/03-pointers.md](file:///E:/Learning/engineering-notes/01-languages/c/02-core-concepts/03-pointers.md), [01-modular-design-in-c.md](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/01-modular-design-in-c.md), [05-resource-management-patterns.md](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/05-resource-management-patterns.md) |
| **Time to Read** | 25 minutes |
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

### The Root Cause of C Memory Vulnerabilities
In languages like Rust or modern C++, memory ownership is tracked and enforced by the compiler (via borrow checkers or smart pointers). In C, **every pointer is syntactically identical**:
```c
void process_packet(char *data);
```
Looking solely at this function signature, a developer cannot answer any of the following critical questions:
- Does `process_packet` take ownership of `data` (is it responsible for calling `free(data)`)?
- Does `process_packet` borrow `data` temporarily (must `data` outlive the function call)?
- Is `data` pointing to heap memory, stack memory, or static string literal memory?
- Will `process_packet` retain an internal pointer alias after the call returns?

When ownership contracts are ambiguous, software inevitably suffers from **Use-After-Free (UAF)**, **Double Free**, or **Memory Leaks**.

### The Solution: Explicit Ownership Discipline
In professional C codebases (such as Linux kernel, Git, and systemd), developers adhere to strict, documented architectural conventions governing resource ownership:
- **Caller-Allocates vs Callee-Allocates:** Standardizing who provides memory buffers.
- **Transfer of Ownership:** Clear handoffs where responsibility to deallocate shifts permanently to the callee.
- **Borrowing Semantics:** Passing pointers for temporary inspection with a guarantee that no aliases persist beyond the function call.

---

## 2. Core Concepts

### 1. Caller-Allocates (Borrowing Model)
The caller is responsible for allocating memory (on the stack or heap) and passing a pointer and capacity to the callee:
```c
int string_to_upper(char *dest, size_t dest_cap, const char *src);
```
- **Advantages:** Zero heap allocation overhead; caller controls memory lifetime (stack allocation friendly); callee cannot leak memory.

### 2. Callee-Allocates (Transfer Model)
The function dynamically allocates memory on the heap and transfers ownership to the caller:
```c
char* string_duplicate(const char *src);
```
- **Contract:** The return value is an owned pointer. The caller **must** take responsibility for calling `free()` or passing it to another owning function.

### 3. Ownership Annotations (Documentation Discipline)
While the C compiler does not enforce ownership syntax, enterprise codebases annotate signatures using macros or conventions:
```c
/* Ownership Annotations */
#define OWNED       /* Callee or Caller assumes full deallocation responsibility */
#define BORROWED    /* Pointer is loaned; must not be stored or freed */

OWNED char* path_join(BORROWED const char *base, BORROWED const char *sub);
void queue_push(Queue *q, OWNED void *item);
```

```
       +-------------------------------------------------------------+
       |               C Ownership Contract Taxonomy                 |
       +-------------------------------------------------------------+
                                     |
         +---------------------------+---------------------------+
         |                                                       |
         v                                                       v
[ Owned (Transfer) ]                                    [ Borrowed (Loan) ]
- Allocated by creator                                  - Pointer loaned temporarily
- Ownership transferred across API                      - Must NOT call free()
- Exactly ONE entity responsible for free()             - Must NOT retain alias beyond call
- Examples: malloc(), path_join()                       - Examples: strlen(), memcmp()
```

---

## 3. How It Works Internally

### Stack vs Heap Lifetime Boundaries
Understanding ownership requires understanding memory storage durations:
- **Automatic Storage (Stack):** Lifetime bound strictly to the enclosing `{ ... }` scope. Returning a pointer to a stack-allocated variable triggers undefined behavior (dangling stack pointer).
- **Dynamic Storage (Heap):** Lifetime extends indefinitely until an explicit call to `free()`.
- **Static Storage (`.data` / `.rodata`):** Lifetime spans the entire execution of the program. Attempting to call `free()` on static memory corrupts the heap manager.

```
Memory Segment Lifetimes:

[Stack Frame]      ----(Scope Exits)----> DESTROYED! Returning &local_var = BUG!
[Heap (malloc)]    ---------------------> PERSISTS until free(ptr). Who owns it?
[Static (.rodata)] ---------------------> PERMANENT. Calling free() = CRASH!
```

### The Alias Problem (Shared Pointers)
A memory leak or use-after-free occurs when an owning pointer is cloned (aliased) without clear lifetime hierarchy:
```
Pointer A (Owner) -----> [ Heap Object (0x2000) ] <----- Pointer B (Alias)
                                 |
                     Pointer A calls free(0x2000)
                                 |
                                 v
               Pointer B is now a DANGLING POINTER!
            Subsequent access through B = USE-AFTER-FREE!
```
Enforcing a single owner or using formal reference counting prevents multiple pointers from fighting over destruction responsibility.

---

## 4. Syntax & Usage

### 1. The Caller-Allocates Pattern (High Performance)

```c
#include <stdio.h>
#include <string.h>

/* Callee borrows buffer; caller retains ownership and controls lifetime */
int format_user_badge(char *out_buf, size_t out_cap, const char *username, int rank) {
    if (!out_buf || out_cap == 0 || !username) {
        return -1;
    }

    int written = snprintf(out_buf, out_cap, "[Rank #%d] %s", rank, username);
    if (written < 0 || (size_t)written >= out_cap) {
        return -2; /* Truncation or error */
    }

    return 0; /* Success */
}

void render_dashboard(void) {
    /* Stack allocation: Zero malloc overhead, automatic lifetime cleanup */
    char badge[64];
    if (format_user_badge(badge, sizeof(badge), "Alice", 1) == 0) {
        printf("User Badge: %s\n", badge);
    }
}
```

### 2. The Transfer-of-Ownership Pattern (Dynamic Container)
```c
typedef struct Node {
    void *owned_data; /* Node assumes ownership of this pointer */
    struct Node *next;
} Node;

void list_push_transfer(Node **head, void *owned_item) {
    Node *n = (Node*)malloc(sizeof(Node));
    n->owned_data = owned_item; /* Ownership transferred to List */
    n->next = *head;
    *head = n;
}

void list_destroy_all(Node **head, void (*item_free)(void*)) {
    Node *cur = *head;
    while (cur) {
        Node *next = cur->next;
        if (item_free && cur->owned_data) {
            item_free(cur->owned_data); /* Release owned item */
        }
        free(cur);
        cur = next;
    }
    *head = NULL;
}
```

---

## 5. Practical Examples

### Comprehensive Verification Suite: Ownership Transfer vs Borrowing Verification

The following test program demonstrates both the Transfer Ownership model (callee allocates) and the Borrowing model (caller allocates):

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Demonstrating Caller-Allocates vs Callee-Allocates Conventions */
/* Callee-allocates (Transfers ownership to caller) */
char* string_duplicate_transfer(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *copy = (char*)malloc(len + 1);
    if (copy) {
        memcpy(copy, src, len + 1);
    }
    return copy;
}

/* Caller-allocates (Borrowing model, zero heap allocation) */
int string_copy_borrow(char *dest, size_t dest_cap, const char *src) {
    if (!dest || !src || dest_cap == 0) return -1;
    size_t len = strlen(src);
    if (len >= dest_cap) return -1;
    memcpy(dest, src, len + 1);
    return 0;
}

int main(void) {
    printf("Evaluating Ownership & Lifetime Conventions in C...\n");

    /* Test Transfer Ownership */
    char *transferred = string_duplicate_transfer("Transferred Ownership");
    printf("Transferred String: [%s]\n", transferred);
    free(transferred);

    /* Test Borrow Model */
    char stack_buf[32];
    string_copy_borrow(stack_buf, sizeof(stack_buf), "Borrowed Buffer");
    printf("Borrowed String: [%s]\n", stack_buf);

    printf("Explicit ownership transfer and borrowing contracts verified.\n");
    return 0;
}
```

```
Evaluating Ownership & Lifetime Conventions in C...
Transferred String: [Transferred Ownership]
Borrowed String: [Borrowed Buffer]
Explicit ownership transfer and borrowing contracts verified.
```

### Explanation
- `string_duplicate_transfer` allocates memory on the heap and transfers ownership directly to the caller. The caller acknowledges ownership by calling `free(transferred)`.
- `string_copy_borrow` accepts a caller-owned stack buffer (`stack_buf`), performing the copy with zero heap overhead and zero lifetime ambiguity.

---

## 6. Best Practices

1. **Default to the Caller-Allocates Pattern:** Whenever possible, require callers to supply output buffers and capacities (`dest, dest_cap`). This allows callers to use stack allocation or custom arenas.
2. **Document Ownership on Every Pointer Parameter:** Explicitly state in header documentation whether a pointer is borrowed or ownership is transferred.
3. **Never Return Pointers to Stack Memory:** Never return the address of a local automatic variable. Compile with `-Wreturn-local-addr` to catch this at build time.
4. **Invalidate Local Pointers Immediately Upon Transfer:** If you transfer ownership of `ptr` to another structure, set `ptr = NULL;` locally to prevent accidental usage.
5. **Use Explicit Verb Naming:** Functions that allocate and transfer ownership should use verbs like `create`, `clone`, `duplicate`, or `alloc` (e.g., `user_clone()`). Borrowing functions should use `get`, `peek`, or `format`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Returning Address of Local Variable** | Stack frame is popped on return; memory is reused immediately, corrupting data. | Return dynamically allocated memory or accept a caller-supplied buffer. |
| **Freeing Borrowed Pointers** | Callee calls `free()` on a borrowed pointer, leaving caller with a dangling pointer. | Only the designated owner entity is permitted to deallocate memory. |
| **Storing Borrowed Pointers in Global State** | The borrowed object goes out of scope and is deallocated, leaving the global pointer dangling. | If an entity needs long-term retention, it must clone or incref the object. |
| **Silent Memory Leaks in Fallible Callee-Allocates** | Callee allocates multiple sub-objects; if the 3rd fails, it returns NULL without freeing the first two. | Use `goto cleanup` to unwind all acquired allocations before returning NULL. |
| **Unbounded Caller Buffers** | Passing `char *dest` without a `size_t dest_cap` leads to buffer overflows. | Always pass destination buffer size alongside the pointer. |
| **Double Ownership in Data Structures** | Inserting the same owned pointer into two separate lists leads to double free upon list destruction. | Maintain a single owning list and use borrowed pointers for secondary indices. |

---

## 8. Performance & Security Notes

### Performance Impact
- **Zero-Allocation Calling:** The Caller-Allocates pattern allows performance-critical inner loops to reuse a single stack or pre-allocated scratchpad buffer, completely eliminating heap contention.
- **Cache Locality:** Stack-allocated buffers live in hot L1 CPU cache lines, whereas heap allocations incur allocator traversal and potential cache misses.

### Security Implications
- **Elimination of Heap Exploits:** By minimizing heap allocations through caller-allocated stack buffers, code becomes inherently immune to heap metadata corruption, double-free, and use-after-free attack vectors.
- **Buffer Overflow Defense:** Pairing every destination pointer with an explicit capacity (`dest_cap`) prevents buffer overflow vulnerabilities.

---

## 9. Comparison / When to Use What

| Pattern | Allocation Site | Deallocation Responsibility | Stack Usable? | Best Suited For |
| :--- | :--- | :--- | :--- | :--- |
| **Caller-Allocates** | Caller (Stack or Heap) | Caller | Yes | Formatting, string copies, hash calculations, cryptographic digests. |
| **Callee-Allocates (Transfer)** | Callee (Heap) | Caller | No | Dynamic objects whose size is unknown in advance (JSON parsing, regex trees). |
| **Shared (Ref-Counted)** | Callee (Heap) | Last remaining owner | No | Multi-threaded asynchronous tasks, shared immutable assets. |
| **Static Borrow** | Compile-Time (`.rodata`) | Operating System | N/A | Static error messages, lookup tables, constant configurations. |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between borrowing a pointer and transferring ownership?</strong></summary>
Borrowing means a function receives a temporary pointer to access or modify data for the duration of the call; the callee must neither free the pointer nor retain persistent aliases after returning. Transferring ownership means the caller shifts full responsibility for the resource's lifetime (including deallocation) to the recipient.
</details>

<details>
<summary><strong>2. Why is the "Caller-Allocates" pattern preferred in high-performance C libraries?</strong></summary>
Because it allows the caller to decide where memory lives (on the stack, in a thread-local arena, or on the heap). It completely decouples the library from the heap manager, eliminates dynamic memory allocation overhead, and avoids memory fragmentation.
</details>

<details>
<summary><strong>3. What happens if a function returns a pointer to a local array declared inside its body?</strong></summary>
Local variables have automatic storage duration, meaning their stack memory is released when the function returns. Returning a pointer to a local variable creates a dangling pointer; accessing it invokes Undefined Behavior as that memory region is overwritten by subsequent stack frames.
</details>

<details>
<summary><strong>4. How should an API indicate that a function transfers ownership of its return value?</strong></summary>
By using naming conventions with explicit allocation verbs (such as `create`, `duplicate`, `alloc`, `clone`), returning explicit error codes with out-parameters, and documenting the ownership transfer in the public header.
</details>

<details>
<summary><strong>5. What is a "Use-After-Free" (UAF) bug, and how does ownership discipline prevent it?</strong></summary>
A UAF occurs when a pointer is accessed after the memory it points to has been deallocated. Ownership discipline prevents UAF by ensuring that exactly one designated owner manages the resource, setting pointers to `NULL` upon destruction, and ensuring borrowed references do not outlive the owner.
</details>

<details>
<summary><strong>6. How does Git maintain ownership across its object database and revision traversal?</strong></summary>
Git differentiates between transient borrowed objects (parsed into memory and discarded after diffing) and permanent repository structures. For large commit graphs, Git uses arena allocators to batch-release entire revision walks at once.
</details>

<details>
<summary><strong>7. Why is it dangerous to pass a string literal (`"hello"`) to a function expecting an owned pointer?</strong></summary>
String literals reside in the read-only data segment (`.rodata`). If the function assumes ownership and eventually calls `free()` on the pointer, the heap allocator aborts or crashes because the address does not belong to the heap heap arena.
</details>

<details>
<summary><strong>8. How can you safely implement a "Peek" accessor without transferring ownership?</strong></summary>
Return a `const` qualified pointer (e.g., `const char* queue_peek(const Queue *q)`). Document that the returned pointer is borrowed and valid only as long as the parent data structure is not modified or destroyed.
</details>

<details>
<summary><strong>9. What is the "Tombstone" pattern in memory management?</strong></summary>
Instead of pointers pointing directly to heap objects, they point to a fixed tombstone handle containing the real object address. When the object is freed, the tombstone address is set to `NULL`. All aliases reading through the tombstone immediately see the null pointer, preventing UAF.
</details>

<details>
<summary><strong>10. How does the POSIX `getline()` function handle ownership and dynamic memory allocation?</strong></summary>
`getline(char **lineptr, size_t *n, FILE *stream)` employs a hybrid ownership model: if `*lineptr` is NULL, it allocates memory dynamically; if `*lineptr` already points to an allocated buffer, it reallocates as needed. The caller retains ultimate ownership and is responsible for calling `free(*lineptr)`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactoring Callee-Allocates to Caller-Allocates (Easy)
Refactor a function that allocates a new formatted string on the heap into a caller-allocated function that writes into a provided buffer.

<details>
<summary>Solution</summary>

**Before (Callee-Allocates, Heap Dependent):**
```c
char* make_timestamp(int h, int m, int s) {
    char *buf = malloc(32);
    if (buf) sprintf(buf, "%02d:%02d:%02d", h, m, s);
    return buf; /* Caller must remember to free! */
}
```

**After (Caller-Allocates, Safe & Stack Friendly):**
```c
int make_timestamp_safe(char *dest, size_t dest_cap, int h, int m, int s) {
    if (!dest || dest_cap < 9) return -1;
    snprintf(dest, dest_cap, "%02d:%02d:%02d", h, m, s);
    return 0;
}
```
</details>

### Exercise 2: Implementing a Safe Ring Buffer with Borrowed Peek (Medium)
Construct a circular ring buffer where items pushed transfer ownership to the buffer, items popped transfer ownership to the caller, and peek operations loan a borrowed pointer.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define RING_CAP 4

typedef struct {
    void *items[RING_CAP];
    int head;
    int tail;
    int count;
} RingQueue;

void ring_init(RingQueue *rq) {
    rq->head = 0;
    rq->tail = 0;
    rq->count = 0;
}

/* Transfers ownership of 'item' into queue */
bool ring_push(RingQueue *rq, void *owned_item) {
    if (rq->count == RING_CAP) return false;
    rq->items[rq->tail] = owned_item;
    rq->tail = (rq->tail + 1) % RING_CAP;
    rq->count++;
    return true;
}

/* Transfers ownership of popped item to caller */
void* ring_pop(RingQueue *rq) {
    if (rq->count == 0) return NULL;
    void *item = rq->items[rq->head];
    rq->items[rq->head] = NULL;
    rq->head = (rq->head + 1) % RING_CAP;
    rq->count--;
    return item;
}

/* Loans a borrowed pointer to caller; ownership remains in queue */
const void* ring_peek(const RingQueue *rq) {
    if (rq->count == 0) return NULL;
    return rq->items[rq->head];
}

int main(void) {
    RingQueue q;
    ring_init(&q);

    int *val = malloc(sizeof(int));
    *val = 42;
    ring_push(&q, val); /* Ownership transferred */

    const int *peeked = ring_peek(&q); /* Borrowed */
    printf("Peeked value: %d\n", *peeked);

    int *popped = ring_pop(&q); /* Ownership reclaimed */
    printf("Popped value: %d\n", *popped);
    free(popped); /* Final deallocation */
    return 0;
}
```
</details>

### Exercise 3: Tracking Object Lifetimes with an Ownership Debug Registry (Hard)
Implement a debug wrapper around `malloc` and `free` that records the file and line number of allocation and flags double-free or leaked resources at program termination.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AllocRecord {
    void *address;
    size_t size;
    const char *file;
    int line;
    struct AllocRecord *next;
} AllocRecord;

static AllocRecord *g_alloc_head = NULL;

void* dbg_malloc(size_t size, const char *file, int line) {
    void *ptr = malloc(size);
    if (!ptr) return NULL;

    AllocRecord *rec = malloc(sizeof(AllocRecord));
    rec->address = ptr;
    rec->size = size;
    rec->file = file;
    rec->line = line;
    rec->next = g_alloc_head;
    g_alloc_head = rec;

    return ptr;
}

void dbg_free(void *ptr) {
    if (!ptr) return;

    AllocRecord **curr = &g_alloc_head;
    while (*curr) {
        if ((*curr)->address == ptr) {
            AllocRecord *to_free = *curr;
            *curr = (*curr)->next;
            free(to_free);
            free(ptr);
            return;
        }
        curr = &(*curr)->next;
    }

    fprintf(stderr, "ERROR: Attempted double-free or untracked pointer: %p\n", ptr);
}

void dbg_report_leaks(void) {
    AllocRecord *curr = g_alloc_head;
    if (!curr) {
        printf("Memory clean! Zero leaks detected.\n");
        return;
    }
    printf("LEAK REPORT:\n");
    while (curr) {
        printf("Leaked %zu bytes at %p (Allocated at %s:%d)\n",
               curr->size, curr->address, curr->file, curr->line);
        curr = curr->next;
    }
}

#define MALLOC(sz) dbg_malloc(sz, __FILE__, __LINE__)
#define FREE(p)    dbg_free(p)

int main(void) {
    char *p1 = MALLOC(64);
    char *p2 = MALLOC(128);
    FREE(p1);
    /* p2 intentionally leaked for reporting */
    dbg_report_leaks();
    FREE(p2);
    return 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Caller-Allocates:** Caller provides buffer and capacity. Zero heap allocations; fast and stack-friendly.
- **Transfer Model:** Callee allocates heap memory and transfers ownership to caller; caller must call `free()`.
- **Borrow Model:** Pointers are loaned temporarily. Callee must never call `free()` or persist aliases.
- **Never Return Stack Pointers:** Local automatic variables are destroyed on function return.
- **Zero Local Pointers on Transfer:** Set transferred pointers to `NULL` to avoid accidental alias operations.

---

## 13. Further Reading
- Robert C. Seacord: *Effective C: An Introduction to Professional C Programming* (Dynamic Memory Allocation).
- Linux Kernel Source: *Memory Management and Pointer Passing Conventions*.
- Rust Documentation: *The Rust Book - Chapter 4: Understanding Ownership (Comparative Study)*.
- ISO C Standard (ISO/IEC 9899:2011), Section 6.2.4: *Storage durations of objects*.

---

| [Previous: 06-api-design-for-c-libraries.md](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/06-api-design-for-c-libraries.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/README.md) | [Next: Module 11 - Projects](../11-projects/README.md) |
