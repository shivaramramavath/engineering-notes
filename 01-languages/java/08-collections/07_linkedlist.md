# LinkedList

`LinkedList<E>` is a **doubly linked list**. It implements both `List` and `Deque`, so it can act as a list, a queue or a stack. Despite its textbook popularity, it is **rarely the best choice** in Java: `ArrayList` and `ArrayDeque` beat it in almost every realistic case.

```java
LinkedList<String> list = new LinkedList<>();
list.add("b");
list.addFirst("a");          // O(1)
list.addLast("c");           // O(1)
list.getFirst();             // "a"
list.removeLast();           // "c"
list.get(1);                 // O(n): walks the nodes
```

## How it works

Each element lives in its own **node** with references to its neighbours. The list keeps references to the first and last node.

```
 first                                              last
   │                                                  │
   ▼                                                  ▼
 ┌──────┐   ┌──────────────┐   ┌──────────────┐   ┌──────┐
 │ null │◄──│ prev │"a"│next│──►│ prev │"b"│next│──►│ ...  │
 └──────┘   └──────────────┘   └──────────────┘   └──────┘
```

```java
private static class Node<E> {
    E item;
    Node<E> next;
    Node<E> prev;
}
```

| Concept | Detail |
|---------|--------|
| `first`, `last` | Head and tail node references |
| `size` | Element count (`size()` is O(1)) |
| Linking | Add or remove at either end changes a few references: O(1) |
| Finding the i-th element | Start at the **nearer** end (head or tail) and walk: up to n/2 steps |
| `modCount` | Fail-fast iterators, as in `ArrayList` ([12](./12_iterators-and-fail-fast-behavior.md)) |

## Cost model

| Operation | Cost | Notes |
|-----------|------|-------|
| `addFirst`, `addLast`, `removeFirst`, `removeLast`, `getFirst`, `getLast` | **O(1)** | The strength of the structure |
| `add(e)` (append) | O(1) | |
| `get(i)`, `set(i, e)` | **O(n)** | Must walk from an end |
| `add(i, e)`, `remove(i)` | O(n) | O(n) to **find** the position, then O(1) to relink |
| `remove(Object)`, `contains`, `indexOf` | O(n) | Linear scan |
| Remove/insert **through an iterator** at the current position | **O(1)** | No search needed |
| Iteration | O(n) | But slower than `ArrayList` (pointer chasing) |

### The O(n²) trap

```java
LinkedList<Integer> list = ...;                    // 100,000 elements
for (int i = 0; i < list.size(); i++) {
    sum += list.get(i);                            // each get walks the list: total O(n²)
}

for (int x : list) sum += x;                       // O(n): the iterator holds the current node
```

Never index into a `LinkedList` in a loop. Declare variables as `List` and a developer may unknowingly pass a `LinkedList` into such code; that is one reason to avoid it.

## Why "O(1) insert in the middle" is misleading

The textbook says linked lists beat arrays at middle insertions. In practice:

1. You first have to **find** the position: O(n) pointer chasing
2. Each node is a separate heap object; nodes are scattered in memory, so traversal causes **CPU cache misses**, far slower per step than scanning an array
3. `ArrayList` shifts elements with one native `System.arraycopy`, which is extremely fast for lists up to tens or hundreds of thousands of elements
4. Every insert **allocates** a node, adding GC work

Benchmarks of mixed workloads typically show `ArrayList` ahead even for middle insertions, unless the list is huge and you hold an iterator positioned at the insertion point.

## Memory

| | `ArrayList<E>` | `LinkedList<E>` |
|---|----------------|-----------------|
| Per element | 1 reference (4-8 bytes) | 1 node object: header (12-16 bytes) + 3 references (`item`, `next`, `prev`) = roughly **24-40 bytes** |
| Locality | Contiguous | Scattered |
| GC pressure | Low | One object per element |

A `LinkedList` of 1 million elements uses several times more memory than an `ArrayList`, in addition to the elements themselves.

## Using it as a Queue, Deque or Stack

```java
Queue<String> queue = new LinkedList<>();          // works...
queue.offer("a"); queue.poll();

Deque<Integer> stack = new LinkedList<>();         // works...
stack.push(1); stack.pop();
```

`LinkedList` implements `Deque`, but **`ArrayDeque` is faster and uses less memory** for the same purpose: use it unless you need `null` elements or need the `List` interface too ([04](./04_queue-and-deque.md)).

## Iterator-based editing (its real niche)

```java
ListIterator<String> it = list.listIterator();
while (it.hasNext()) {
    String s = it.next();
    if (s.isEmpty()) it.remove();          // O(1): unlink the current node
    else if (s.equals("x")) it.add("y");   // O(1): insert after the current node
}
```

With a long list, many removals/insertions **at positions found while iterating**, no random access needed, and no better structure, `LinkedList` can be reasonable. Even then, `removeIf` on an `ArrayList` (single O(n) pass) is usually just as fast.

## Other properties

| Property | Detail |
|----------|--------|
| `null` elements | Allowed (avoid; `poll`/`peek` return `null` to signal empty) |
| Duplicates | Allowed |
| Thread-safety | None |
| `RandomAccess` | **No**: algorithms choose iterator-based strategies |
| Java 21 | Implements `SequencedCollection`: `getFirst`, `getLast`, `reversed()`, ... |
| Serializable, Cloneable | Yes |

## When to use `LinkedList`

| Situation | Better choice |
|-----------|---------------|
| General-purpose list | **`ArrayList`** |
| Queue or stack | **`ArrayDeque`** |
| Need `null` in a deque, or one object that is both `List` and `Deque` | `LinkedList` (genuinely rare) |
| Many iterator-driven insertions/removals in a huge list | `LinkedList`, after measuring |
| LRU / ordered cache | `LinkedHashMap` ([11](./11_specialized-collections.md)) |
| Concurrent queue | `ConcurrentLinkedQueue`, `BlockingQueue` |

Josh Bloch (who wrote it): *"Does anyone actually use LinkedList? I wrote it, and I never use it."* Use `ArrayList` by default and measure before choosing otherwise ([15](./15_collection-performance-and-selection.md), [20-performance/05_common-performance-pitfalls.md](../20-performance/05_common-performance-pitfalls.md)).

## Implementing a linked list yourself (interview classic)

```java
class SinglyLinkedList<T> {
    private static class Node<T> { T value; Node<T> next; Node(T v) { value = v; } }
    private Node<T> head;
    private int size;

    void addFirst(T v) { Node<T> n = new Node<>(v); n.next = head; head = n; size++; }

    T removeFirst() {
        if (head == null) throw new NoSuchElementException();
        T v = head.value;
        head = head.next;
        size--;
        return v;
    }

    void reverse() {                                   // O(n), O(1) extra space
        Node<T> prev = null, cur = head;
        while (cur != null) { Node<T> next = cur.next; cur.next = prev; prev = cur; cur = next; }
        head = prev;
    }
}
```

Linked-list algorithms (reversal, cycle detection, merging): [27-dsa/04-linked-list](../27-dsa/04-linked-list/).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `for (i...) list.get(i)` over a `LinkedList` | O(n²) slowdown | for-each or an `ArrayList` |
| Choosing it "because insertions are O(1)" | Slower and heavier than `ArrayList` | Measure; default to `ArrayList` |
| Using it as a queue/stack | Slower than `ArrayDeque`, more memory | `ArrayDeque` |
| Adding `null` and then using `poll()`/`peek()` | Cannot tell empty from `null` element | Do not store `null` |
| `list.remove(i)` expecting O(1) | O(n) to find the node | Use an iterator at the position |
| Passing a `LinkedList` into code that indexes | Silent performance cliff | Use `ArrayList`, or copy |
| Modifying during for-each | `ConcurrentModificationException` | `Iterator`/`ListIterator`, `removeIf` |
| Holding huge `LinkedList`s of boxed numbers | Memory blow-up | Arrays, primitive collections |

## Key takeaways

- `LinkedList` = doubly linked nodes; O(1) at both ends and through an iterator; **O(n) for indexed access**
- Per-element overhead and poor cache locality make it slower and heavier than `ArrayList` in practice
- For queues and stacks use `ArrayDeque`; for general lists use `ArrayList`
- Never loop with `get(i)` over a `LinkedList`
- Know the structure well (it is an interview staple), but rarely choose it

**Next:** [HashMap and HashSet](./08_hashmap-and-hashset.md)
