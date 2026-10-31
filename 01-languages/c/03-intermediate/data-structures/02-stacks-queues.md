# Stacks and Queues

A **stack** is last-in, first-out (LIFO); a **queue** is first-in, first-out (FIFO). They are interfaces more than structures: you can build either on an array or a linked list. Both appear constantly in parsing, graph traversal, scheduling, and buffering.

**Prerequisites:** [Linked lists](./01-linked-lists.md), [memory management](../../02-core-concepts/08-memory-management.md).

## Stack

```text
push(3) push(7) push(9)         pop() → 9

 ┌───┐                           ┌───┐
 │ 9 │ ← top                     │ 7 │ ← top
 │ 7 │                           │ 3 │
 │ 3 │                           └───┘
 └───┘
```

### Array-based stack with growth

```c
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
    int    *data;
    size_t  len;
    size_t  cap;
} stack_t_;                         /* avoid clashing with POSIX stack_t */

bool stack_push(stack_t_ *s, int v) {
    if (s->len == s->cap) {
        size_t ncap = s->cap ? s->cap * 2 : 16;
        int *tmp = realloc(s->data, ncap * sizeof *tmp);
        if (tmp == NULL) return false;
        s->data = tmp;
        s->cap  = ncap;
    }
    s->data[s->len++] = v;
    return true;
}

bool stack_pop(stack_t_ *s, int *out) {
    if (s->len == 0) return false;          /* underflow: report, don't crash */
    *out = s->data[--s->len];
    return true;
}

bool stack_peek(const stack_t_ *s, int *out) {
    if (s->len == 0) return false;
    *out = s->data[s->len - 1];
    return true;
}

void stack_free(stack_t_ *s) { free(s->data); *s = (stack_t_){0}; }
```

Push and pop are O(1) (amortized for push). A linked-list stack is just `push_front` / `pop_front` on a [linked list](./01-linked-lists.md).

### Example: balanced brackets

```c
bool balanced(const char *s) {
    stack_t_ st = {0};
    bool ok = true;
    for (; *s && ok; s++) {
        switch (*s) {
        case '(': case '[': case '{':
            ok = stack_push(&st, *s);
            break;
        case ')': case ']': case '}': {
            int open;
            ok = stack_pop(&st, &open) &&
                 ((open == '(' && *s == ')') ||
                  (open == '[' && *s == ']') ||
                  (open == '{' && *s == '}'));
            break;
        }
        }
    }
    ok = ok && st.len == 0;
    stack_free(&st);
    return ok;
}
```

### Where stacks appear

- Function calls: the **call stack** itself ([stack frames](../../05-internals/02-stack-frames.md)).
- Expression evaluation and parsing (shunting-yard, bracket matching).
- Depth-first search and backtracking; "undo" histories.
- Explicit stacks replace deep recursion, which avoids stack overflow on large inputs.

## Queue

```text
enqueue(3) enqueue(7) enqueue(9)       dequeue() → 3

 front → [3][7][9] ← back              front → [7][9] ← back
```

A queue needs both ends. A plain array with `memmove` on every dequeue is O(n), and a linked list with a tail pointer allocates per element. The standard answer is a **ring buffer** (circular array).

### Ring buffer queue

```c
typedef struct {
    int    *data;
    size_t  cap;        /* fixed capacity */
    size_t  head;       /* index of the next element to dequeue */
    size_t  len;        /* number of elements stored */
} queue_t_;

bool queue_init(queue_t_ *q, size_t cap) {
    q->data = malloc(cap * sizeof *q->data);
    q->cap = cap; q->head = 0; q->len = 0;
    return q->data != NULL;
}

bool queue_enqueue(queue_t_ *q, int v) {
    if (q->len == q->cap) return false;                  /* full */
    size_t tail = (q->head + q->len) % q->cap;
    q->data[tail] = v;
    q->len++;
    return true;
}

bool queue_dequeue(queue_t_ *q, int *out) {
    if (q->len == 0) return false;                       /* empty */
    *out = q->data[q->head];
    q->head = (q->head + 1) % q->cap;
    q->len--;
    return true;
}
```

```text
cap = 5, after several operations:

index:  0   1   2   3   4
       [ ] [ ] [x] [x] [x]       head = 2, len = 3, next write at (2+3)%5 = 0
```

Tracking `len` separately removes the classic ambiguity between "full" and "empty" when only `head` and `tail` indices are kept. If `cap` is a power of two, `& (cap - 1)` replaces `%`. For an unbounded queue, grow by allocating a larger array and copying the elements in logical order.

Ring buffers are the standard structure for I/O buffers, producer/consumer pipelines, logging, and audio.

### Linked queue

Use a [linked list](./01-linked-lists.md) with `head` and `tail`: enqueue at the tail, dequeue at the head, both O(1). It grows without copying, at the cost of per-node allocation.

### Where queues appear

- Breadth-first search ([graphs](./05-graphs.md)).
- Task schedulers, message passing, buffering between a fast producer and slower consumer.

## Related structures

| Structure | Description |
|---|---|
| Deque | insert/remove at both ends; ring buffer or doubly linked list |
| Priority queue | dequeue the smallest/largest; implemented as a binary **heap** in an array (O(log n) insert and remove) |

Concurrent queues need synchronization; see [synchronization](../../04-advanced/08-concurrency/02-synchronization.md).

## Complexity

| Operation | Array stack | Ring-buffer queue | Linked (with tail) |
|---|---|---|---|
| push / enqueue | O(1) amortized | O(1) | O(1) |
| pop / dequeue | O(1) | O(1) | O(1) |
| peek | O(1) | O(1) | O(1) |

## Common mistakes

| Mistake | Effect |
|---|---|
| Popping an empty stack/queue without checking | Underflow, out-of-bounds read |
| Ignoring a full ring buffer | Overwrites unread data |
| Using `%` with a negative index | Wrong slot; keep indexes unsigned |
| `realloc` result assigned directly to the pointer | Leak on failure |
| Dequeuing by shifting the whole array | O(n²) behavior |
| Naming a type `stack_t` | Collides with POSIX `<signal.h>` |

## Summary

- Stack = LIFO (array with a length); queue = FIFO (ring buffer or linked list with tail).
- Always report underflow/overflow instead of assuming valid state.
- Stacks power DFS/parsing; queues power BFS/buffering.

**Next:** [Trees](./03-trees.md)
