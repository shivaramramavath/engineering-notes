# Use-After-Free and Double-Free

> **Deconstructing heap lifecycle vulnerabilities, dangling pointers, allocator metadata corruption, and defensive pointer tracking architectures in C.**

| Level | Prerequisites | Time to Read | Status | Standard |
| :--- | :--- | :--- | :--- | :--- |
| Advanced | [02-core-concepts/08-memory-management.md](../02-core-concepts/08-memory-management.md), [01-buffer-overflows.md](01-buffer-overflows.md) | 26 mins | Complete | C99 / C11 / C17 / POSIX |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-Common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In pure C, dynamic heap memory lifecycle is managed manually via `malloc()`, `calloc()`, `realloc()`, and `free()`. The C runtime does not maintain garbage collection reference counts or ownership semantics. When a block of memory is released via `free()`, the pointer variable pointing to that memory is not automatically cleared; it becomes a **dangling pointer**.

Two severe classes of memory safety vulnerabilities arise from dangling pointers:
1. **Use-After-Free (UAF):** The program dereferences a dangling pointer to read or write data after the underlying memory chunk has been returned to the heap allocator.
2. **Double-Free:** The program invokes `free()` twice on the identical pointer without an intervening allocation.

In modern software exploitation, UAF and Double-Free vulnerabilities are prized by attackers:
- When memory is freed, the allocator reassigns that physical memory chunk to subsequent allocations of a different type.
- An attacker can manipulate heap layout (heap feng-shui) so that an attacker-controlled structure (e.g., containing function pointers or network handles) overlaps the dangling pointer.
- Dereferencing the dangling pointer executes attacker-controlled function pointers, achieving arbitrary code execution without stack buffer overflows.

---

## 2. Core Concepts

### 2.1 The Dangling Pointer Lifecycle
```mermaid
flowchart TD
    Alloc[1. p = malloc(sz) -> Memory Block Allocated] --> Use[2. Normal Use: read/write *p]
    Use --> Free[3. free(p) -> Memory Chunk Returned to Allocator Free List]
    Free --> Dangling[4. p still holds original memory address: Dangling Pointer!]
    Dangling --> Realloc[5. Another subsystem allocates q = malloc(sz)]
    Realloc --> Overlap[Chunk Reused: *p and *q point to identical memory!]
    Overlap --> Exploit[6. Use-After-Free: Writing via p corrupts q's data!]
```

### 2.2 Double-Free Mechanics
When `free(ptr)` is called:
- The allocator inserts the memory chunk into an internal linked list (fastbin, tcache, or unsorted bin).
- If `free(ptr)` is invoked a second time on the same address, the chunk is inserted into the free-list bin twice, creating a **circular linked list**.
- Subsequent calls to `malloc()` return the identical memory chunk to two completely separate subsystems, leading to catastrophic type confusion and arbitrary write primitives.

---

## 3. How It Works Internally

### 3.1 Glibc Ptmalloc Free-List Internals
When a chunk is active, its body stores application payload bytes. When a chunk is freed, the allocator repurposes the payload memory to store internal free-list metadata:
```
Active Chunk:
+--------------------+--------------------+---------------------------------------+
| Prev_Size (8 bytes)| Chunk_Size (8 bytes| Application User Payload (N bytes)    |
+--------------------+--------------------+---------------------------------------+

Freed Chunk in Bin:
+--------------------+--------------------+-------------------+-------------------+
| Prev_Size (8 bytes)| Chunk_Size (8 bytes| Forward Ptr (fd)  | Backward Ptr (bk) |
+--------------------+--------------------+-------------------+-------------------+
```
1. If the application dereferences a dangling pointer and writes into it (UAF), it directly clobbers `fd` (forward pointer) and `bk` (backward pointer).
2. On the next `malloc()` call, the allocator follows the corrupted `fd` pointer and allocates memory at an arbitrary location chosen by the attacker (e.g. over the Global Offset Table).

### 3.2 Type Confusion via Heap Grooming (Feng-Shui)
Consider two structs of identical size:
```c
struct user_session {
    char username[32];
    void (*on_logout)(void); /* Function pointer at offset 32 */
};

struct attacker_message {
    char payload[40];        /* Attacker writes arbitrary bytes */
};
```
1. `session` is allocated, used, and freed, but the pointer `g_active_session` is not set to `NULL`.
2. Attacker sends a message; `msg = malloc(sizeof(struct attacker_message))` is assigned the exact chunk just vacated by `session`.
3. Attacker fills `msg->payload` with shellcode or ROP gadget addresses at offset 32.
4. Server calls `g_active_session->on_logout()`. The CPU jumps directly to the attacker's forged address!

---

## 4. Syntax & Usage

### 4.1 Vulnerable Code Pattern
```c
void vulnerable_workflow(void) {
    char *token = malloc(64);
    strcpy(token, "SECRET_AUTH_TOKEN");
    
    /* ... operations ... */
    free(token);
    
    /* FATAL: Dangling pointer read! */
    if (token[0] == 'S') { 
        printf("Token still begins with S!
");
    }
    
    /* FATAL: Double-free if executed in error cleanup path */
    free(token);
}
```

### 4.2 The Safe Free Idiom
In pure C, passing a pointer by value to `free(p)` means `free()` cannot modify the caller's pointer variable. The standard defensive idiom is the **Safe Free Macro**, which accepts the address of the pointer or evaluates it in a `do-while` block:
```c
#define SAFE_FREE(ptr) do {     if ((ptr) != NULL) {            free(ptr);                  (ptr) = NULL;           }                       } while (0)
```
Setting `ptr = NULL` ensures:
1. Any subsequent attempt to read or write `*ptr` triggers an immediate, unexploitable CPU segmentation fault (`SIGSEGV` at page 0), rather than a subtle UAF exploit.
2. The C standard explicitly guarantees that `free(NULL)` is a safe no-op, completely neutralizing double-free vulnerabilities.

---

## 5. Practical Examples

### 5.1 Verified Companion Simulation: Safe Deallocation Protection
The following self-contained program evaluates the safe pointer deallocation pattern, verifying that pointer nullification neutralizes double-free attempts:

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Safe pointer deallocation idiom with pointer nullification */
#define SAFE_FREE(ptr) do {     if ((ptr) != NULL) {         free(ptr);         (ptr) = NULL;     } } while (0)

int main(void) {
    printf("Evaluating Use-After-Free & Double-Free Mitigation...\n");
    char *session_token = malloc(32);
    if (!session_token) return 1;
    snprintf(session_token, 32, "SECURE_TOKEN_XYZ_123");

    printf("Allocated token: %s (address: %p)\n", session_token, (void *)session_token);
    SAFE_FREE(session_token);

    printf("Post-free pointer value: %p\n", (void *)session_token);
    /* Attempting secondary free is safely a no-op */
    SAFE_FREE(session_token);
    printf("Double-free successfully neutralized by pointer nullification.\n");
    return 0;
}
```

**Real Execution Output:**
```text
Evaluating Use-After-Free & Double-Free Mitigation...
Allocated token: SECURE_TOKEN_XYZ_123 (address: 009B2998)
Post-free pointer value: 00000000
Double-free successfully neutralized by pointer nullification.
```

---

## 6. Best Practices

1. **Always Nullify Pointers Immediately After `free()`:** Make `SAFE_FREE(p)` or `p = NULL` a mandatory coding reflex.
2. **Clarify Resource Ownership:** Establish strict structural ownership. A struct should only be freed by the module or function designated as its owner.
3. **Use Handle-Based Indirection:** Instead of distributing raw heap pointers throughout an application, pass lightweight integer handles (`uint32_t handle`) that index an internal handle table. When an object is freed, increment the generation counter; stale handles immediately fail lookup.
4. **Compile Test Suites with AddressSanitizer:** ASan poisons freed memory redzones and keeps freed chunks in a quarantine ring buffer. Any read or write to freed memory produces an instant crash report detailing both the allocation and deallocation call stacks.
5. **Zero Out Sensitive Buffers Before Freeing:** Always wipe passwords, keys, and tokens with `explicit_bzero()` before deallocation to prevent UAF information leakage.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Fix |
| :--- | :--- | :--- |
| **Passing pointer by value to custom free wrapper** | `my_free(p)` sets the local parameter to `NULL`, leaving the caller's pointer dangling! | Pass double pointer: `my_free(&p)`. |
| **Freeing a pointer to stack or static memory** | Passing a non-heap pointer to `free()` corrupts heap allocator structures. | Only call `free()` on pointers returned by `malloc`/`calloc`/`realloc`. |
| **Aliased pointers (multiple copies of one pointer)** | Nullifying `p1 = NULL` leaves `p2` dangling if both pointed to the same block. | Centralize ownership or use reference counting. |
| **Calling `free()` inside error cleanup loops** | If an array of pointers is partially initialized, freeing uninitialized pointers crashes. | Zero-initialize pointer arrays: `void *ptrs[10] = {0};`. |
| **Using `realloc()` incorrectly on failure** | `p = realloc(p, new_sz)` leaks original `p` if allocation fails and returns `NULL`. | Use temporary pointer: `void *tmp = realloc(p, sz); if (tmp) p = tmp;`. |
| **Assuming OS clears heap pointers** | Operating systems do not monitor or wipe C process local variables. | Explicitly manage pointer lifecycles. |

---

## 8. Performance & Security Notes

### 8.1 Performance Impact of Pointer Nullification
Assigning `ptr = NULL` compiles down to a single machine instruction (`movq $0, (%rax)`), requiring less than 1 CPU cycle. This negligible cost provides complete immunity against classical double-free attacks and turns silent memory corruptions into deterministic null-pointer faults.

### 8.2 Modern Allocator Hardening Defenses
Modern system allocators incorporate defenses against double-free and UAF attacks:
- **Tcache Double-Free Checks (glibc 2.29+):** Glibc checks if a freed chunk is already present in the target thread cache bin and aborts with `double free or corruption (fasttop)`.
- **Safe-Linking Pointer Mangling (glibc 2.32+):** Free-list forward pointers (`fd`) are obfuscated using random ASLR keys via pointer XOR masking: `mangled = (pos >> 12) ^ ptr`. Attackers cannot overwrite pointers without leaking the ASLR key.

---

## 9. Comparison / When to Use What

| Pattern | Safety Level | Overhead | Implementation Complexity |
| :--- | :--- | :--- | :--- |
| **Raw `free(p)`** | Extremely Hazardous | 0% | Trivial (Anti-pattern) |
| **`SAFE_FREE(p)` (Macro)** | High | Negligible | Low (Recommended) |
| **Double Pointer `free_obj(&p)`** | Very High | Negligible | Low (Recommended for libraries) |
| **Reference Counting** | Very High | Low (Atomic ops) | Moderate (Complex systems) |
| **Handle / Arena Allocators** | Maximum | Fast | High (Game engines, compilers) |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is a "dangling pointer" and why is it dangerous in C?</strong></summary>

A dangling pointer is a pointer that retains the memory address of an object that has already been deallocated via `free()`. It is dangerous because dereferencing it (Use-After-Free) reads or writes memory that may have been reassigned to another subsystem, leading to silent memory corruption, type confusion, or arbitrary code execution.
</details>

<details>
<summary><strong>2. Why doesn't standard C library <code>free(p)</code> automatically set <code>p = NULL</code>?</strong></summary>

In C, parameters are passed by value. When you call `free(p)`, a copy of the pointer address is passed to `free()`. The function cannot modify the caller's pointer variable because it does not receive the memory address of the pointer itself (`p` instead of `&p`).
</details>

<details>
<summary><strong>3. How does setting a freed pointer to <code>NULL</code> neutralize Double-Free vulnerabilities?</strong></summary>

The ISO C standard explicitly specifies that passing a `NULL` pointer to `free()` is completely safe and produces no side effects (it is a defined no-op). If a pointer is nullified immediately after the first `free()`, any subsequent `free()` call operates on `NULL` and returns safely without corrupting allocator metadata.
</details>

<details>
<summary><strong>4. How does AddressSanitizer (ASan) catch Use-After-Free bugs?</strong></summary>

When an application calls `free()`, ASan's allocator interceptor poisons the corresponding shadow memory bytes (marking them `0xFD` for freed heap) and places the physical chunk into a quarantine ring buffer to prevent immediate reuse. If the application attempts to read or write the dangling pointer, the compiler-injected shadow check detects the `0xFD` marker and crashes immediately with a detailed diagnostic.
</details>

<details>
<summary><strong>5. Explain the concept of "Type Confusion" resulting from a Use-After-Free bug.</strong></summary>

Type confusion occurs when freed memory previously representing Struct Type A is reallocated by the heap to represent Struct Type B. If code holding a dangling pointer to Type A accesses fields using Type A's layout, it misinterprets Type B's binary data (e.g. treating an integer in Type B as a function pointer in Type A), enabling arbitrary memory redirection.
</details>

<details>
<summary><strong>6. What is the danger of writing <code>p = realloc(p, new_size);</code>?</strong></summary>

If `realloc()` fails to find sufficient memory, it returns `NULL`. The assignment `p = NULL` overwrites the original pointer, making it impossible to free the original memory block, causing a permanent memory leak. The correct idiom uses a temporary pointer: `void *tmp = realloc(p, new_size); if (tmp) p = tmp;`.
</details>

<details>
<summary><strong>7. How does reference counting prevent Use-After-Free vulnerabilities?</strong></summary>

Reference counting tracks how many active owners hold references to an allocated object. When a subsystem acquires an object, it increments the counter; when done, it decrements the counter. The underlying memory is only deallocated when the reference count reaches exactly zero, ensuring no subsystem can ever hold a dangling reference.
</details>

<details>
<summary><strong>8. What is the "Safe-Linking" mitigation introduced in glibc 2.32?</strong></summary>

Safe-Linking protects the single-linked fastbins and tcache lists from pointer overwriting by XORing the free-list pointer with a randomized bitshift of its own memory address: `L = (P >> 12) ^ P`. An attacker who overwrites a dangling pointer cannot forge a valid destination pointer without leaking ASLR addresses.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Double Pointer Safe Free Implementation
- **Task:** Implement a safe deallocation function `void safe_free_ptr(void **pptr)` that frees the pointed-to memory and sets the caller's pointer to `NULL`.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>

void safe_free_ptr(void **pptr) {
    if (pptr != NULL && *pptr != NULL) {
        free(*pptr);
        *pptr = NULL;
    }
}

int main(void) {
    char *buf = malloc(64);
    safe_free_ptr((void **)&buf);
    if (buf == NULL) {
        printf("Pointer safely nullified!\n");
    }
    return 0;
}
```
</details>

### Exercise 2 (Medium): Reference-Counted Resource Wrapper
- **Task:** Implement a reference-counted memory wrapper `ref_counted_t` with `retain()` and `release()` methods that automatically frees payload memory when the reference count drops to 0.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
    int ref_count;
    char data[64];
} ref_object_t;

ref_object_t *ref_create(const char *msg) {
    ref_object_t *obj = malloc(sizeof(ref_object_t));
    if (!obj) return NULL;
    obj->ref_count = 1;
    snprintf(obj->data, sizeof(obj->data), "%s", msg);
    return obj;
}

void ref_retain(ref_object_t *obj) {
    if (obj) obj->ref_count++;
}

void ref_release(ref_object_t **pobj) {
    if (!pobj || !*pobj) return;
    ref_object_t *obj = *pobj;
    obj->ref_count--;
    if (obj->ref_count <= 0) {
        printf("Reference dropped to 0: Freeing object '%s'\n", obj->data);
        free(obj);
        *pobj = NULL;
    }
}

int main(void) {
    ref_object_t *item = ref_create("SharedContext");
    ref_retain(item); /* Count = 2 */
    ref_release(&item); /* Count = 1, item still active */
    ref_release(&item); /* Count = 0, freed and nullified */
    return 0;
}
```
</details>

### Exercise 3 (Hard): Generational Handle Table
- **Task:** Implement a simple handle-based allocator table that returns 32-bit handles consisting of a 16-bit slot index and a 16-bit generation counter, demonstrating that stale handles cannot access newly allocated objects.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_SLOTS 4

typedef struct {
    uint16_t generation;
    bool is_active;
    int payload;
} slot_t;

static slot_t g_slots[MAX_SLOTS];

uint32_t handle_alloc(int val) {
    for (uint16_t i = 0; i < MAX_SLOTS; ++i) {
        if (!g_slots[i].is_active) {
            g_slots[i].is_active = true;
            g_slots[i].payload = val;
            return ((uint32_t)g_slots[i].generation << 16) | i;
        }
    }
    return 0xFFFFFFFF; /* Out of slots */
}

void handle_free(uint32_t handle) {
    uint16_t idx = handle & 0xFFFF;
    uint16_t gen = (handle >> 16) & 0xFFFF;
    if (idx < MAX_SLOTS && g_slots[idx].is_active && g_slots[idx].generation == gen) {
        g_slots[idx].is_active = false;
        g_slots[idx].generation++; /* Invalidate stale handles! */
    }
}

bool handle_get(uint32_t handle, int *out_val) {
    uint16_t idx = handle & 0xFFFF;
    uint16_t gen = (handle >> 16) & 0xFFFF;
    if (idx < MAX_SLOTS && g_slots[idx].is_active && g_slots[idx].generation == gen) {
        *out_val = g_slots[idx].payload;
        return true;
    }
    return false; /* Stale handle access rejected! */
}

int main(void) {
    uint32_t h1 = handle_alloc(42);
    int val = 0;
    handle_get(h1, &val);
    printf("Access 1: %d\n", val);

    handle_free(h1);
    bool ok = handle_get(h1, &val);
    printf("Stale access outcome: %s\n", ok ? "FAILED" : "REJECTED (UAF Prevented!)");
    return 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Defect / Concept | Definition | Defense / Solution |
| :--- | :--- | :--- |
| **Dangling Pointer** | Pointer holding address of freed memory | Nullify immediately: `p = NULL` |
| **Use-After-Free (UAF)**| Dereferencing dangling pointer | `SAFE_FREE(p)`, Handle tables, ASan |
| **Double-Free** | Invoking `free()` twice on same address | Nullify pointer; `free(NULL)` is safe |
| **Heap Grooming** | Aligning chunks to achieve type confusion | Strict allocator isolation |
| **Safe Free Macro** | `#define SAFE_FREE(p) do { free(p); p = NULL; } while(0)` | Universal defensive pattern |
| **AddressSanitizer** | Detects UAF via poisoned quarantine lists | `-fsanitize=address` in testing |

---

## 13. Further Reading
- Understanding the Glibc Heap (Azeria Labs)
- CWE-416: Use After Free: `https://cwe.mitre.org/data/definitions/416.html`
- CWE-415: Double Free: `https://cwe.mitre.org/data/definitions/415.html`
- CERT C Rule MEM30-C: Do not access freed memory

---
[<- Previous: 03-integer-overflow-and-wraparound.md](03-integer-overflow-and-wraparound.md) | [Section Index](README.md) | [Next: 05-secure-coding-cert-c.md ->](05-secure-coding-cert-c.md)\n