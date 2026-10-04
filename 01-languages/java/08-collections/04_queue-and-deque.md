# Queue and Deque

A **queue** holds elements for processing, normally **first-in, first-out (FIFO)**. A **deque** ("double-ended queue", pronounced "deck") lets you add and remove at **both** ends, so it also works as a **stack** (last-in, first-out, LIFO).

```
Queue (FIFO)                          Stack (LIFO)                 Deque
add at tail ──► [ a b c d ] ──► remove at head      push/pop at the same end     both ends
                 tail     head
```

## `Queue<E>`

Every operation comes in **two forms**: one throws an exception on failure, the other returns a special value.

| Operation | Throws exception | Returns special value |
|-----------|------------------|-----------------------|
| **Insert** at the tail | `add(e)` | `offer(e)` (false if it cannot be added) |
| **Remove** the head | `remove()` | `poll()` (`null` if empty) |
| **Examine** the head | `element()` | `peek()` (`null` if empty) |

```java
Queue<String> q = new ArrayDeque<>();
q.offer("a");
q.offer("b");
q.offer("c");
q.peek();            // "a": look at the head, do not remove
q.poll();            // "a": remove the head
q.poll();            // "b"
q.size();            // 1
new ArrayDeque<String>().poll();     // null (empty)
new ArrayDeque<String>().remove();   // NoSuchElementException
```

Prefer `offer`/`poll`/`peek` for ordinary code: an empty queue is a normal situation. Use `add`/`remove`/`element` when emptiness would be a bug.

Because `null` means "empty" for `poll`/`peek`, queues generally **do not allow `null` elements** (`ArrayDeque` and `PriorityQueue` throw `NullPointerException`; `LinkedList` allows it but you should not).

## `Deque<E>`: both ends

| Operation | First (head) end | Last (tail) end |
|-----------|------------------|-----------------|
| Insert | `addFirst(e)` / `offerFirst(e)` | `addLast(e)` / `offerLast(e)` |
| Remove | `removeFirst()` / `pollFirst()` | `removeLast()` / `pollLast()` |
| Examine | `getFirst()` / `peekFirst()` | `getLast()` / `peekLast()` |

The first name of each pair throws on failure, the second returns a special value, just like `Queue`.

```java
Deque<Integer> d = new ArrayDeque<>();
d.addFirst(1);  d.addLast(2);  d.addFirst(0);      // [0, 1, 2]
d.pollFirst();                                      // 0
d.pollLast();                                       // 2
d.removeFirstOccurrence(1);
d.descendingIterator();                             // iterate tail to head
```

### Queue methods map to Deque methods

| `Queue` method | Equivalent `Deque` method |
|----------------|---------------------------|
| `add(e)` / `offer(e)` | `addLast(e)` / `offerLast(e)` |
| `remove()` / `poll()` | `removeFirst()` / `pollFirst()` |
| `element()` / `peek()` | `getFirst()` / `peekFirst()` |

## As a stack: `push`, `pop`, `peek`

```java
Deque<String> stack = new ArrayDeque<>();
stack.push("a");        // = addFirst
stack.push("b");
stack.push("c");        // stack (top first): [c, b, a]
stack.peek();           // "c": top, not removed (= peekFirst)
stack.pop();            // "c": removes the top (= removeFirst; throws if empty)
stack.pop();            // "b"
stack.isEmpty();
```

`push` and `pop` work at the **head**, so iteration over a `Deque` used as a stack shows the **top first**.

## Why not `java.util.Stack`?

```java
Stack<Integer> s = new Stack<>();      // legacy: avoid
```

| Problem | Detail |
|---------|--------|
| It **extends `Vector`**, so it exposes `get(i)`, `add(i, e)`, `remove(i)` | Breaks stack discipline (a stack should not allow random access) |
| Every method is **synchronized** | Needless locking overhead |
| Wrong direction in `List` view | Its iteration order is bottom → top, the opposite of `ArrayDeque` |
| Javadoc itself recommends `Deque` | "A more complete and consistent set of LIFO stack operations is provided by the Deque interface" |

Use `ArrayDeque`.

## Implementations

| Implementation | Structure | Notes |
|----------------|-----------|-------|
| **`ArrayDeque`** | Resizable circular array | **Default** for both queue and stack; fastest; no `null` |
| `LinkedList` | Doubly linked list | Implements `List` **and** `Deque`; allows `null`; slower and more memory ([07](./07_linkedlist.md)) |
| `PriorityQueue` | Binary heap | Not FIFO: removes the **smallest** (or highest-priority) element ([10](./10_priorityqueue.md)) |
| `ConcurrentLinkedQueue` / `ConcurrentLinkedDeque` | Lock-free linked nodes | Concurrent, non-blocking |
| `LinkedBlockingQueue`, `ArrayBlockingQueue`, `PriorityBlockingQueue`, `SynchronousQueue`, `DelayQueue` | Blocking queues | `put`/`take` wait; for producer-consumer ([14-concurrency/15_concurrency-patterns.md](../14-concurrency/15_concurrency-patterns.md)) |

```java
Queue<Task> fifo = new ArrayDeque<>();
Deque<Integer> stack = new ArrayDeque<>();
Queue<Job> byPriority = new PriorityQueue<>(Comparator.comparingInt(Job::priority));
```

### Cost summary

| Operation | `ArrayDeque` | `LinkedList` | `PriorityQueue` |
|-----------|--------------|--------------|------------------|
| Add / remove at either end | O(1) amortized | O(1) | O(log n) |
| Peek the head | O(1) | O(1) | O(1) |
| `contains` / remove arbitrary | O(n) | O(n) | O(n) |
| Iteration | Fast (array) | Slower (nodes) | Heap order, **not** sorted |

## Typical uses

### Queue: breadth-first search, task scheduling, buffering

```java
// BFS on a graph
Queue<Node> queue = new ArrayDeque<>();
Set<Node> visited = new HashSet<>();
queue.offer(start);
visited.add(start);
while (!queue.isEmpty()) {
    Node n = queue.poll();
    for (Node next : n.neighbors()) {
        if (visited.add(next)) queue.offer(next);
    }
}
```

### Stack: balanced brackets, undo, depth-first search, expression evaluation

```java
static boolean balanced(String s) {
    Deque<Character> stack = new ArrayDeque<>();
    for (char c : s.toCharArray()) {
        switch (c) {
            case '(', '[', '{' -> stack.push(c);
            case ')' -> { if (stack.isEmpty() || stack.pop() != '(') return false; }
            case ']' -> { if (stack.isEmpty() || stack.pop() != '[') return false; }
            case '}' -> { if (stack.isEmpty() || stack.pop() != '{') return false; }
            default -> { }
        }
    }
    return stack.isEmpty();
}
```

Explicit stacks also replace deep recursion ([02-methods/03_recursion.md](../02-methods/03_recursion.md)).

### Deque: sliding window maximum, palindromes, work stealing

```java
// Sliding window maximum: keep a deque of indices with decreasing values
Deque<Integer> window = new ArrayDeque<>();
for (int i = 0; i < nums.length; i++) {
    while (!window.isEmpty() && window.peekFirst() <= i - k) window.pollFirst();     // drop out-of-window
    while (!window.isEmpty() && nums[window.peekLast()] < nums[i]) window.pollLast(); // drop smaller
    window.offerLast(i);
    if (i >= k - 1) result[i - k + 1] = nums[window.peekFirst()];
}
```

More algorithm patterns: [27-dsa/03-stack-and-queue](../27-dsa/03-stack-and-queue/).

## Blocking queues (preview)

For passing work **between threads**, use `BlockingQueue`: `put` blocks when full, `take` blocks when empty.

```java
BlockingQueue<Task> queue = new LinkedBlockingQueue<>(100);     // bounded
producer: queue.put(task);          // waits if the queue is full
consumer: Task t = queue.take();    // waits if the queue is empty
```

See [14-concurrency/08_concurrent-collections.md](../14-concurrency/08_concurrent-collections.md). Plain `ArrayDeque` is **not** thread-safe.

## Iteration order and printing

```java
Queue<Integer> q = new ArrayDeque<>(List.of(1, 2, 3));
System.out.println(q);                    // [1, 2, 3]: head first

Deque<Integer> stack = new ArrayDeque<>();
stack.push(1); stack.push(2); stack.push(3);
System.out.println(stack);                // [3, 2, 1]: top first

Queue<Integer> pq = new PriorityQueue<>(List.of(5, 1, 3));
System.out.println(pq);                   // heap order, e.g. [1, 5, 3]: NOT sorted
```

Iterating a `PriorityQueue` does **not** give priority order; repeatedly `poll` instead.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Using `Stack` or `Vector` | Synchronization overhead, awkward API | `ArrayDeque` |
| `LinkedList` where `ArrayDeque` would do | Slower, more memory | `ArrayDeque` |
| Mixing `push/pop` with `offer/poll` and expecting the same end | Elements come out in a surprising order | `push`/`pop` use the **head**; `offer`/`poll` add at the tail and remove at the head |
| `remove()`/`element()` on an empty queue | `NoSuchElementException` | Use `poll()`/`peek()` and check for `null` |
| Adding `null` to `ArrayDeque`/`PriorityQueue` | `NullPointerException` | Do not store `null` |
| Iterating a `PriorityQueue` to get sorted output | Heap order only | `poll` in a loop |
| Sharing an `ArrayDeque` across threads | Corruption, lost elements | `BlockingQueue` / concurrent queues |
| Using `queue.contains` / `remove(Object)` in hot loops | O(n) | Auxiliary `HashSet` |
| Using `ArrayList.remove(0)` as a queue | O(n) per removal | `ArrayDeque` |
| Unbounded queues under heavy producers | `OutOfMemoryError` | Bounded blocking queues, backpressure |

## Key takeaways

- `Queue` = FIFO with two method families (throwing vs special value); `Deque` = both ends, so it is also the stack
- Use `ArrayDeque` for queues **and** stacks; avoid `Stack`; `LinkedList` rarely
- `push/pop/peek` work at the head; `offer/poll/peek` add at the tail and remove at the head
- `PriorityQueue` orders by priority, not arrival; iteration is not sorted
- Cross-thread queues belong to `java.util.concurrent` (`BlockingQueue`)

**Next:** [Comparable and Comparator](./05_comparable-and-comparator.md)
