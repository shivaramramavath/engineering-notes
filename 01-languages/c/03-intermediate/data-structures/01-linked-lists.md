# Linked Lists

A linked list stores each element in its own heap node, with a pointer to the next one. It's the simplest pointer-based structure and the foundation for stacks, queues, hash-table buckets, and graphs. It is also a good exercise in [pointers](../../02-core-concepts/03-pointers.md) and [memory management](../../02-core-concepts/08-memory-management.md).

## Structure

```text
head
 │
 ▼
┌────┬───┐   ┌────┬───┐   ┌────┬──────┐
│ 10 │ ●─┼──▶│ 20 │ ●─┼──▶│ 30 │ NULL │
└────┴───┘   └────┴───┘   └────┴──────┘
 value next
```

```c
#include <stdbool.h>
#include <stdlib.h>

typedef struct node {
    int          value;
    struct node *next;
} node_t;

typedef struct {
    node_t *head;
    size_t  len;
} list_t;
```

An empty list is `head == NULL`. Initialize with `list_t l = {0};`.

## Core operations

### Insert at the front: O(1)

```c
bool list_push_front(list_t *l, int v) {
    node_t *n = malloc(sizeof *n);
    if (n == NULL) return false;
    n->value = v;
    n->next  = l->head;
    l->head  = n;
    l->len++;
    return true;
}
```

### Search: O(n)

```c
node_t *list_find(const list_t *l, int v) {
    for (node_t *n = l->head; n != NULL; n = n->next) {
        if (n->value == v) return n;
    }
    return NULL;
}
```

### Remove with a pointer-to-pointer

The classic trick: track the *link* that points at the current node (either `head` or some `next`), so deleting the first node needs no special case.

```c
bool list_remove(list_t *l, int v) {
    for (node_t **pp = &l->head; *pp != NULL; pp = &(*pp)->next) {
        if ((*pp)->value == v) {
            node_t *dead = *pp;
            *pp = dead->next;           /* unlink */
            free(dead);
            l->len--;
            return true;
        }
    }
    return false;
}
```

### Reverse in place: O(n)

```c
void list_reverse(list_t *l) {
    node_t *prev = NULL, *cur = l->head;
    while (cur != NULL) {
        node_t *next = cur->next;       /* save before overwriting */
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    l->head = prev;
}
```

### Free the whole list

```c
void list_free(list_t *l) {
    node_t *n = l->head;
    while (n != NULL) {
        node_t *next = n->next;         /* read next BEFORE freeing n */
        free(n);
        n = next;
    }
    l->head = NULL;
    l->len = 0;
}
```

## Variations

| Variant | Adds | Use when |
|---|---|---|
| Tail pointer | O(1) append (`push_back`) | queue-like use; must update `tail` on removal of the last node |
| Doubly linked (`prev` + `next`) | O(1) removal given a node; backward traversal | LRU caches, editors; costs an extra pointer per node |
| Circular | last node points to the first | round-robin scheduling |
| Sentinel (dummy head) | removes `NULL`-head special cases | simplifies insert/remove code |
| Intrusive | the link lives inside your own struct (`struct item { ...; struct item *next; }`) | no separate allocation; used throughout the Linux kernel |

Doubly linked removal when you already hold the node:

```c
node->prev->next = node->next;
node->next->prev = node->prev;      /* with a sentinel head/tail there are no NULL checks */
```

## Complexity

| Operation | Singly linked | Array (dynamic) |
|---|---|---|
| Access by index | O(n) | O(1) |
| Insert/remove at front | O(1) | O(n) |
| Insert/remove after a known node | O(1) | O(n) |
| Search | O(n) | O(n) |
| Memory overhead | pointer + allocator overhead per element | none |

## When to use (and when not to)

Linked lists are taught early but used less than you'd think. Each node is a separate allocation scattered in memory, so traversal is cache-unfriendly, and arrays often win even for insert-heavy workloads at modest sizes. Prefer a dynamic array (see [memory management](../../02-core-concepts/08-memory-management.md)) unless you need:

- stable pointers to elements while others come and go,
- O(1) insertion/removal at known positions,
- no large reallocation spikes,
- or the structure is a building block (hash buckets, free lists, adjacency lists).

## Common mistakes

| Mistake | Effect |
|---|---|
| Freeing a node, then reading `n->next` | Use after free; save `next` first |
| Losing the head pointer (overwriting `head` before relinking) | Leaks the whole list |
| Not updating `head` after inserting/removing at the front | List appears unchanged or corrupt |
| Forgetting to update `tail` on removal | Dangling `tail`, crash on next append |
| Not checking `malloc` | `NULL` dereference |
| Cycles created by a bad `next` | Infinite traversal; detect with the tortoise/hare technique |
| Passing `node_t *head` by value and expecting the caller's head to change | Pass `list_t *` or `node_t **` |

## Debugging

- Draw the nodes and arrows on paper; check each pointer assignment in order.
- gdb: `print *head`, `print *head->next`, or `print *head->next->next` to walk manually.
- AddressSanitizer pinpoints use-after-free and leaks (`-fsanitize=address`).

## Summary

- Nodes with `next` pointers; empty list is `NULL`.
- Use pointer-to-pointer for clean removal; save `next` before freeing.
- Great as a building block; for plain sequences a dynamic array is usually faster.

**Next:** [Stacks and queues](./02-stacks-queues.md)
