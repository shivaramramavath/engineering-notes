# Linked Structures with Pointers: Self-Referential Data Types

> Introduction to recursive and linked data modeling in C: self-referential structures, pointer linking, dynamic node insertion, traversal, and memory cleanup.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Pointers](./03-pointers.md), [Structs, Unions, and Enums](./06-structs-unions-enums.md), [Memory Management](./08-memory-management.md), [Pointers to Pointers](./23-pointers-to-pointers-and-arrays-of-pointers.md) |
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

Contiguous arrays require a single, continuous block of memory. While arrays offer rapid $O(1)$ random access, inserting or deleting an element in the middle requires shifting all subsequent elements ($O(N)$ time), and resizing requires reallocating the entire buffer.

**Linked Structures** solve this by distributing data across discrete, independently allocated **nodes** connected via **pointers**. Each node stores both its payload data and one or more pointers addressing related nodes:
- **Linked Lists**: Sequential nodes connected via `next` (and optionally `prev`) pointers.
- **Binary Trees**: Hierarchical nodes connected via `left` and `right` child pointers.
- **Graphs**: Nodes connected to adjacency lists.

Implementing linked structures introduces C's most powerful data modeling tool: **self-referential structures**.

---

## 2. Core Concepts

### Self-Referential Structures
A structure cannot contain an instance of itself directly by value:
```c
struct Node {
    int value;
    struct Node next; /* COMPILE ERROR: incomplete type / infinite size */
};
```
Because the compiler cannot determine the size of an infinitely recursive type, this is forbidden. However, a structure **can contain a pointer to itself**:
```c
struct Node {
    int value;
    struct Node *next; /* 100% LEGAL: all pointers have fixed size */
};
```
Because all data pointers on a given architecture have a known, fixed size (4 bytes on 32-bit, 8 bytes on 64-bit), the compiler calculates `sizeof(struct Node)` with zero ambiguity.

### Node Allocation and Pointer Linking
Every node is instantiated on the heap via `malloc()`. Connecting nodes is accomplished by assigning addresses:
```c
Node *first = malloc(sizeof *first);
Node *second = malloc(sizeof *second);

first->next = second; /* Link first to second */
second->next = NULL;  /* Sentinel terminating list */
```

### Clean Destruction
Because each node is an independent heap allocation, freeing a linked structure requires traversing the chain node-by-node. Freeing the head pointer alone does **not** free subsequent nodes, causing an unrecoverable memory leak.

---

## 3. How It Works Internally

### Node Memory Linking in Process Heap

```
Heap Memory Space (Non-Contiguous Nodes Connected by Virtual Addresses):

 Node 1 (Address: 0x1000)        Node 2 (Address: 0x4500)        Node 3 (Address: 0x2200)
┌───────────┬──────────────┐    ┌───────────┬──────────────┐    ┌───────────┬──────────────┐
│ value: 10 │ next: 0x4500 │───►│ value: 20 │ next: 0x2200 │───►│ value: 30 │ next: NULL   │
└───────────┴──────────────┘    └───────────┴──────────────┘    └───────────┴──────────────┘
```

Unlike an array where `arr[i+1]` is located at `arr + (sizeof(T))`, linked list nodes reside at arbitrary, fragmented heap addresses assigned by the allocator. The CPU must load the pointer from `curr->next` into a register before it can issue the next memory read (pointer chasing).

---

## 4. Syntax & Usage

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int value;
    struct Node *next;
} Node;

/* Insertion at head using pointer-to-pointer */
void list_prepend(Node **head_ref, int val) {
    Node *new_node = malloc(sizeof *new_node);
    if (!new_node) return;
    new_node->value = val;
    new_node->next = *head_ref;
    *head_ref = new_node;
}

/* Traversal */
void list_print(const Node *head) {
    for (const Node *p = head; p != NULL; p = p->next) {
        printf("%d -> ", p->value);
    }
    printf("NULL\n");
}
```

> [!NOTE]
> ### The Opaque Tagged Struct Idiom
> In header files, declaring `typedef struct Node Node;` allows defining APIs that manipulate linked structures without exposing internal node member definitions to consumers, achieving complete information hiding and encapsulation.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates creating self-referential nodes, prepending elements, traversing the linked chain, and freeing every node cleanly with pointer nullification.

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int value;
    struct Node *next; /* Self-referential pointer */
} Node;

static Node* node_create(int val) {
    Node *n = malloc(sizeof *n);
    if (!n) return NULL;
    n->value = val;
    n->next = NULL;
    return n;
}

static void list_push_front(Node **head_ref, int val) {
    Node *n = node_create(val);
    if (!n) return;
    n->next = *head_ref;
    *head_ref = n;
}

static void list_free(Node **head_ref) {
    if (!head_ref) return;
    Node *curr = *head_ref;
    while (curr) {
        Node *next = curr->next;
        free(curr);
        curr = next;
    }
    *head_ref = NULL;
}

int main(void) {
    printf("=== C Linked Structures with Pointers Demo ===\n");

    Node *head = NULL;
    list_push_front(&head, 30);
    list_push_front(&head, 20);
    list_push_front(&head, 10);

    printf("Traversing linked list: ");
    for (Node *curr = head; curr != NULL; curr = curr->next) {
        printf("%d -> ", curr->value);
    }
    printf("NULL\n");

    list_free(&head);
    printf("List cleanly freed, head is NULL: %s\n", head == NULL ? "YES" : "NO");

    return 0;
}
```

```text
=== C Linked Structures with Pointers Demo ===
Traversing linked list: 10 -> 20 -> 30 -> NULL
List cleanly freed, head is NULL: YES
```

---

## 6. Best Practices

1. **Always Save `curr->next` Before Calling `free(curr)`**: Dereferencing `curr->next` after calling `free(curr)` is a Use-After-Free (UAF) bug. Always cache the next pointer in a temporary variable first.
2. **Use Pointers-to-Pointers (`Node **head`) for Insertion/Deletion**: Eliminates special-case logic for inserting or deleting the head node.
3. **Always Check for `NULL` Allocation Return**: If `malloc` fails, handle memory exhaustion without corrupting the existing linked list.
4. **Initialize Terminal Pointers to `NULL`**: A newly created node's `next` pointer must be set to `NULL` explicitly. Dangling uninitialized pointers cause infinite traversal crashes.
5. **Set the Head Pointer to `NULL` Upon Deallocation**: In destruction functions, set `*head_ref = NULL;` to prevent double free and dangling pointer bugs in the caller.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **`free(curr); curr = curr->next;`** | Use-After-Free: Accesses memory after it has been returned to the heap. | Cache next pointer: `Node *n = curr->next; free(curr); curr = n;`. |
| **Self-referential struct by value** | `struct S { struct S next; };` has infinite size; compiler constraint violation. | Declare as a pointer: `struct S *next;`. |
| **Losing the head pointer** | Overwriting `head = head->next` without freeing previous nodes leaks memory. | Maintain an iteration pointer `curr = head`. |
| **Infinite loop via circular reference** | Accidental cyclic pointers cause traversal loops to hang or crash stack. | Use Floyd's Cycle-Finding Algorithm (Tortoise and Hare). |
| **Memory fragmentation with small payloads** | Allocating 4-byte integers in 24-byte nodes wastes heap overhead. | Use dynamic arrays (vectors) unless dynamic reordering is essential. |
| **Passing single pointer to insert function** | Modifying `head = new_node` only alters the local parameter copy. | Pass double pointer: `void insert(Node **head)`. |

---

## 8. Performance & Security Notes

### Cache Performance: Linked Lists vs. Arrays
In modern CPU architectures, linked lists suffer from severe **cache locality penalties**. An array of 1,000 integers resides contiguously; the CPU prefetcher loads sequential elements into L1 cache lines automatically. In contrast, linked list nodes reside in random heap chunks. Traversal forces the CPU to stall for hundreds of clock cycles on each node waiting for main memory bus reads. Unless frequent $O(1)$ mid-list splicing is required, dynamic vectors are significantly faster.

### Security: Heap Spraying and Pointer Corruption
In networked daemons, linked structures that parse untrusted input are vulnerable if an attacker can overwrite `node->next`. Directing `node->next` to an arbitrary virtual address causes subsequent write operations (`node->next->val = x`) to perform an **arbitrary memory overwrite**, enabling privilege escalation.

---

## 9. Comparison: When to Use What

| Feature | Linked List (`struct Node*`) | Dynamic Array (`int*`) | Static Array (`int[]`) |
| :--- | :--- | :--- | :--- |
| **Insertion at Head** | $O(1)$ | $O(N)$ (Shift required) | Not supported |
| **Random Indexing** | $O(N)$ (Must traverse) | $O(1)$ (Direct math) | $O(1)$ |
| **Memory Overhead** | 8–16 bytes per item (pointers) | Unused capacity | 0 bytes |
| **Cache Locality** | Poor (Pointer hopping) | Contiguous (Optimal) | Contiguous (Optimal) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why can a structure contain a pointer to itself, but not an instance of itself?</strong></summary>

A structure cannot contain an instance of itself because its size would be infinite and impossible to compute at compile time. It can contain a pointer to itself because pointers on a given platform have a fixed, known size (4 bytes on 32-bit, 8 bytes on 64-bit), allowing the compiler to determine the exact memory layout and size of the struct.
</details>

<details>
<summary><strong>2. How do you detect a cycle in a linked list?</strong></summary>

Using **Floyd's Cycle Detection Algorithm** (the Tortoise and the Hare):
Maintain two pointers (`slow` and `fast`). On each step, advance `slow` by 1 node and `fast` by 2 nodes. If the linked list contains a cycle, `fast` will eventually wrap around and equal `slow` ($O(N)$ time, $O(1)$ space). If `fast` or `fast->next` reaches `NULL`, no cycle exists.
</details>

<details>
<summary><strong>3. How do you delete a node from a singly linked list in O(1) time given only a pointer to that node?</strong></summary>

Copy the data from the next node into the current node, then delete the next node:
```c
node->value = node->next->value;
Node *temp = node->next;
node->next = node->next->next;
free(temp);
```
*(Note: This technique works for all nodes except the very last terminal node).*
</details>

<details>
<summary><strong>4. What is the advantage of using a dummy "sentinel" head node in a linked list?</strong></summary>

A dummy sentinel head node eliminates special-case logic for inserting or deleting the first element. All operations (including head mutations) treat the target node as having a valid preceding node, simplifying code and eliminating bugs.
</details>

<details>
<summary><strong>5. Why is freeing a linked list iteratively preferred over recursive freeing?</strong></summary>

Recursive freeing (`void free_list(Node *n) { if (n) { free_list(n->next); free(n); } }`) places a stack frame on the call stack for every node. For a list with 1,000,000 nodes, recursion will consume gigabytes of stack memory, triggering a fatal Stack Overflow crash. Iterative freeing uses $O(1)$ constant stack space.
</details>

<details>
<summary><strong>6. What is a doubly linked list and what trade-offs does it introduce?</strong></summary>

A doubly linked list contains both `next` and `prev` pointers.
- Advantage: Allows bidirectional traversal and $O(1)$ node removal without needing a pointer to the preceding node.
- Trade-off: Consumes an additional pointer (8 bytes per node) and increases code complexity during pointer updating.
</details>

<details>
<summary><strong>7. How does an intrusive linked list (like Linux kernel `struct list_head`) work?</strong></summary>

In an intrusive linked list, the list node pointers (`next`, `prev`) are embedded directly **inside** the data structure, rather than the list node pointing to the data. The enclosing struct address is recovered using the standard `offsetof` / `container_of()` macro. This eliminates dynamic allocation for list nodes and allows an object to exist simultaneously on multiple lists.
</details>

<details>
<summary><strong>8. What is the time complexity of reversing a singly linked list in-place?</strong></summary>

In-place reversal takes **$O(N)$ time** and **$O(1)$ auxiliary space**, requiring three pointers (`prev`, `curr`, `next`) to reverse the pointer directions in a single pass.
</details>

---

## 11. Practice Exercises

### Exercise 1: Linked List Length Counter (Easy)
Write a function `size_t list_length(const Node *head)` that counts the number of nodes in a linked list iteratively.

<details>
<summary>View Solution</summary>

```c
#include <stddef.h>

size_t list_length(const Node *head) {
    size_t count = 0;
    for (const Node *p = head; p != NULL; p = p->next) {
        count++;
    }
    return count;
}
```
</details>

### Exercise 2: In-Place Singly Linked List Reversal (Medium)
Implement `void list_reverse(Node **head_ref)` that reverses a singly linked list in-place using three pointers.

<details>
<summary>View Solution</summary>

```c
void list_reverse(Node **head_ref) {
    if (!head_ref || !*head_ref) return;
    Node *prev = NULL;
    Node *curr = *head_ref;
    Node *next = NULL;

    while (curr) {
        next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    *head_ref = prev;
}
```
</details>

### Exercise 3: Sorted List Insertion (Hard)
Write a function `void list_insert_sorted(Node **head_ref, int val)` that maintains an ascending sorted linked list, inserting the new value in its proper position using pointer-to-pointer indirection.

<details>
<summary>View Solution</summary>

```c
#include <stdlib.h>

void list_insert_sorted(Node **head_ref, int val) {
    Node *new_node = malloc(sizeof *new_node);
    if (!new_node) return;
    new_node->value = val;

    Node **curr = head_ref;
    while (*curr && (*curr)->value < val) {
        curr = &((*curr)->next);
    }

    new_node->next = *curr;
    *curr = new_node;
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Self-referential struct */
typedef struct Node {
    int val;
    struct Node *next;
} Node;

/* Iterative Traversal */
for (Node *p = head; p; p = p->next) { /* Read p->val */ }

/* Safe Iterative Free */
Node *curr = head;
while (curr) {
    Node *next = curr->next;
    free(curr);
    curr = next;
}
head = NULL;

/* Insertion via Double Pointer */
void insert(Node **head_ref, Node *new_node) {
    new_node->next = *head_ref;
    *head_ref = new_node;
}
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §6.7.2.1 "Structure and union specifiers" (Self-reference).
- *Algorithms in C* (3rd ed.) by Robert Sedgewick (Part 1: "Fundamentals", Linked Lists).
- Linux Kernel Source: `include/linux/list.h` (Intrusive circular doubly linked lists).
- *Introduction to Algorithms* (CLRS), Chapter 10: "Elementary Data Structures".

---

[← Previous: Complex Declarations](./24-complex-declarations-reading-guide.md) | [Section Index](./README.md) | [Next: Intermediate C Overview →](../03-intermediate/README.md)
