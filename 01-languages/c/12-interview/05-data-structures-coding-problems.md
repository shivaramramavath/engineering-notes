# Data Structures Coding Interview Problems in C (`05-data-structures-coding-problems`)

> Production-grade implementations of foundational data structures under strict interview constraints: intrusive linked lists, binary heaps, circular ring buffers, and balanced trees.

| Metadata | Details |
| :--- | :--- |
| **Guide** | `05-data-structures-coding-problems` |
| **Level** | Advanced |
| **Standard Baseline** | C11 / Standard Architecture |

---

## 1. Problem: Reverse a Singly-Linked List In-Place

<details>
<summary><strong>Solution & Complete Code</strong></summary>

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int val;
    struct Node *next;
} Node;

Node* reverse_list(Node *head) {
    Node *prev = NULL;
    Node *cur = head;
    while (cur) {
        Node *next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    return prev;
}
```
- **Time Complexity:** $O(N)$
- **Space Complexity:** $O(1)$
</details>

---

## 2. Problem: Detect and Find Start of Cycle in Linked List (Floyd's Tortoise and Hare)

<details>
<summary><strong>Solution & Complete Code</strong></summary>

```c
Node* detect_cycle_start(Node *head) {
    if (!head || !head->next) return NULL;
    Node *slow = head;
    Node *fast = head;

    /* Phase 1: Determine if cycle exists */
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) break;
    }

    if (slow != fast) return NULL; /* No cycle */

    /* Phase 2: Find cycle entry */
    slow = head;
    while (slow != fast) {
        slow = slow->next;
        fast = fast->next;
    }
    return slow;
}
```
- **Time Complexity:** $O(N)$
- **Space Complexity:** $O(1)$
</details>

---

## 3. Problem: Implement a Circular FIFO Ring Buffer

<details>
<summary><strong>Solution & Complete Code</strong></summary>

```c
#include <stdbool.h>
#include <stddef.h>

#define RING_CAP 16

typedef struct {
    int data[RING_CAP];
    size_t head;
    size_t tail;
    size_t count;
} RingBuffer;

bool ring_push(RingBuffer *rb, int val) {
    if (rb->count == RING_CAP) return false; /* Full */
    rb->data[rb->tail] = val;
    rb->tail = (rb->tail + 1) % RING_CAP;
    rb->count++;
    return true;
}

bool ring_pop(RingBuffer *rb, int *out_val) {
    if (rb->count == 0) return false; /* Empty */
    *out_val = rb->data[rb->head];
    rb->head = (rb->head + 1) % RING_CAP;
    rb->count--;
    return true;
}
```
</details>

---

## 4. Problem: Linux Kernel Intrusive Linked List Pattern

<details>
<summary><strong>Solution & Complete Code</strong></summary>

```c
#include <stddef.h>

struct list_head {
    struct list_head *next, *prev;
};

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

static inline void list_add(struct list_head *new_node, struct list_head *head) {
    head->next->prev = new_node;
    new_node->next = head->next;
    new_node->prev = head;
    head->next = new_node;
}
```
**Why:** The intrusive pattern eliminates secondary dynamic memory allocations for list nodes; the list pointers are embedded directly into user data structs.
</details>

---

[← Bit Manipulation QA](./04-bit-manipulation-qa.md) | [Interview Index](./README.md) | [Next: Systems & POSIX QA →](./06-systems-qa.md)
