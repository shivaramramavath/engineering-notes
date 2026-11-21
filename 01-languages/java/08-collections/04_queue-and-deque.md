# The Queue and Deque Interfaces

> FIFO buffers, LIFO stacks, double-ended queues: contracts, exception vs special-value idioms, and ArrayDeque mechanics.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_collection-hierarchy.md](00_collection-hierarchy.md), [01_list.md](01_list.md) |
| **Time to Read** | 22 minutes |
| **Status** | Active |
| **Java Version** | Java 1.5+ (SequencedCollection retrofitting Java 21 LTS) |

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

A `java.util.Queue` represents a collection designed for holding elements prior to processing. Beyond basic `Collection` operations, queues provide additional insertion, extraction, and inspection operations. Most queues order elements in a FIFO (First-In-First-Out) manner, though exceptions include priority queues (which order elements according to natural order or comparators) and LIFO stacks.

A `java.util.Deque` (Double-Ended Queue, pronounced "deck") extends `Queue` to support element insertion and removal at both endpoints. A `Deque` can function simultaneously as a FIFO queue and as a LIFO stack.

In modern Java, `java.util.ArrayDeque` completely replaces the legacy `java.util.Stack` class and outclasses `LinkedList` for almost all queue and stack use cases.

---

## 2. Core Concepts

### Two Forms of Queue Methods
The `Queue` interface defines two distinct sets of methods for each operation: one that throws an exception if the operation fails, and one that returns a special value (`false` or `null`):

| Operation | Throws Exception | Returns Special Value |
|---|---|---|
| **Insert** | `add(e)` | `offer(e)` (returns `false` if bounded and full) |
| **Remove** | `remove()` | `poll()` (returns `null` if empty) |
| **Examine** | `element()` | `peek()` (returns `null` if empty) |

### Double-Ended Queue (`Deque`)
`Deque` mirrors these operations for both ends (head and tail):
- **Insert**: `addFirst(e)`, `offerFirst(e)` / `addLast(e)`, `offerLast(e)`
- **Remove**: `removeFirst()`, `pollFirst()` / `removeLast()`, `pollLast()`
- **Examine**: `getFirst()`, `peekFirst()` / `getLast()`, `peekLast()`

### LIFO Stack Operations
`Deque` replaces the legacy `Stack` class with modern, unsynchronized equivalents:
- `push(e)` $\equiv$ `addFirst(e)`
- `pop()` $\equiv$ `removeFirst()`
- `peek()` $\equiv$ `peekFirst()`

---

## 3. How It Works Internally

### `ArrayDeque` Circular Array Mechanics
`ArrayDeque` is implemented as a **resizing circular array**. It maintains two integer pointers: `head` and `tail`.
- When an element is added to the head (`addFirst`), `head` is decremented modulo array length: `head = (head - 1) & (elements.length - 1)`.
- When an element is added to the tail (`addLast`), `tail` is incremented modulo array length: `tail = (tail + 1) & (elements.length - 1)`.
- Array length is always a **power of two**, allowing the modulo operation to be computed with an ultra-fast bitwise AND (`& (length - 1)`).

```
ArrayDeque Circular Buffer (Capacity 8, Power of Two):
Indices:   [ 0 ][ 1 ][ 2 ][ 3 ][ 4 ][ 5 ][ 6 ][ 7 ]
Items:     [ E ][ F ][   ][   ][   ][ B ][ C ][ D ]
                      ^              ^
                      | tail = 2     | head = 5

Head moves left on addFirst(); Tail moves right on addLast().
If head == tail, buffer is full -> array doubles in size!
```

---

## 4. Syntax & Usage

### 1. Using `ArrayDeque` as a FIFO Queue
```java
import java.util.ArrayDeque;
import java.util.Queue;

public class FifoQueueDemo {
    public static void main(String[] args) {
        Queue<String> taskQueue = new ArrayDeque<>();

        // Producer: offer
        taskQueue.offer("Job-1");
        taskQueue.offer("Job-2");

        // Consumer: poll
        while (!taskQueue.isEmpty()) {
            String job = taskQueue.poll();
            System.out.println("Processing: " + job);
        }
    }
}
```

### 2. Using `ArrayDeque` as a LIFO Stack (Replacing `Stack`)
```java
import java.util.ArrayDeque;
import java.util.Deque;

public class StackDemo {
    public static void main(String[] args) {
        Deque<Integer> stack = new ArrayDeque<>();

        stack.push(10);
        stack.push(20);
        stack.push(30);

        while (!stack.isEmpty()) {
            System.out.println("Popped: " + stack.pop()); // 30, 20, 10
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Validating Balanced Parentheses with Deque Stack (Basic)
```java
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.Map;

public class ParenthesesValidator {
    private static final Map<Character, Character> PAIRS = Map.of(')', '(', '}', '{', ']', '[');

    public static boolean isValid(String expression) {
        Deque<Character> stack = new ArrayDeque<>();
        for (char c : expression.toCharArray()) {
            if (c == '(' || c == '{' || c == '[') {
                stack.push(c);
            } else if (PAIRS.containsKey(c)) {
                if (stack.isEmpty() || stack.pop() != PAIRS.get(c)) {
                    return false;
                }
            }
        }
        return stack.isEmpty();
    }

    public static void main(String[] args) {
        System.out.println("Valid: " + isValid("{[()]}")); // true
        System.out.println("Valid: " + isValid("{[(])}")); // false
    }
}
```

### Example 2: Breadth-First Search (BFS) Traversal (Real-World)
```java
import java.util.ArrayDeque;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Queue;
import java.util.Set;

public class GraphBfs {
    public static void bfs(String startNode, Map<String, List<String>> graph) {
        Queue<String> queue = new ArrayDeque<>();
        Set<String> visited = new HashSet<>();

        queue.offer(startNode);
        visited.add(startNode);

        while (!queue.isEmpty()) {
            String current = queue.poll();
            System.out.print(current + " -> ");

            for (String neighbor : graph.getOrDefault(current, List.of())) {
                if (visited.add(neighbor)) {
                    queue.offer(neighbor);
                }
            }
        }
        System.out.println("END");
    }

    public static void main(String[] args) {
        Map<String, List<String>> graph = Map.of(
            "A", List.of("B", "C"),
            "B", List.of("D"),
            "C", List.of("E"),
            "D", List.of(),
            "E", List.of()
        );
        bfs("A", graph); // A -> B -> C -> D -> E -> END
    }
}
```

### Example 3: Edge Case - `null` Elements are Forbidden in `ArrayDeque`
```java
import java.util.ArrayDeque;

public class NullInDequeEdgeCase {
    public static void main(String[] args) {
        ArrayDeque<String> deque = new ArrayDeque<>();
        try {
            deque.add(null);
        } catch (NullPointerException e) {
            System.out.println("Caught expected NPE: ArrayDeque strictly forbids null elements!");
        }
    }
}
```

### Example 4: Failure Case - Memory Leaks with Legacy `Stack` Class
```java
import java.util.Stack;

public class LegacyStackIssue {
    public static void main(String[] args) {
        // java.util.Stack extends Vector!
        // It synchronizes every operation, has flawed inheritance (allows stack.insertElementAt(0, item)),
        // and is obsolete. Always use ArrayDeque instead.
        Stack<String> legacy = new Stack<>();
        legacy.push("bad");
    }
}
```

---

## 6. Best Practices

1. **Always Favor `ArrayDeque` Over `Stack`**: `java.util.Stack` is legacy, thread-synchronized, and extends `Vector`. `ArrayDeque` is unsynchronized and vastly faster.
2. **Favor `ArrayDeque` Over `LinkedList` for Queues**: `ArrayDeque` avoids node object allocations and preserves cache locality.
3. **Use Special-Value Methods (`offer`, `poll`, `peek`) for Queues**: Avoid throwing `NoSuchElementException` when polling empty queues.
4. **Pre-size `ArrayDeque` for Known Depths**: Initialize capacity with `new ArrayDeque<>(capacity)` to prevent internal circular buffer resizing.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Using `java.util.Stack` in modern code | Extends `Vector`; synchronizes every call and allows arbitrary index insertions. | Use `Deque<T> stack = new ArrayDeque<>()`. |
| Using `LinkedList` for simple queues | Allocates a 24-byte node object for every single queued item. | Use `ArrayDeque`. |
| Inserting `null` into `ArrayDeque` | `ArrayDeque` throws `NullPointerException` because `null` is used as special return by `poll()`. | Use non-null objects or `Optional`. |
| Confusing `poll()` and `remove()` | `remove()` throws `NoSuchElementException` when empty; `poll()` returns `null`. | Choose the method that matches your error handling model. |
| Using `Queue` when bidirectional access is required | Standard `Queue` only exposes head extraction and tail insertion. | Declare reference as `Deque`. |
| Forgetting that `push()` adds to head | In `Deque`, `push()` is `addFirst()` while `offer()` is `addLast()`. | Use matching stack (`push`/`pop`) or queue (`offer`/`poll`) methods. |

---

## 8. Performance & Security Notes

### Performance
- **Bitwise Circular Modulo**: `ArrayDeque` keeps its capacity as powers of two so pointer wraps execute as `(tail + 1) & (length - 1)`, compiling to single-cycle machine instructions.
- **Amortized $O(1)$ Operations**: All operations (`addFirst`, `addLast`, `pollFirst`, `pollLast`, `peek`) run in amortized $O(1)$ time.

### Security
- **Unbounded Growth**: `ArrayDeque` has no capacity limit by default and doubles when full. If fed untrusted streaming inputs without bounds checking, it can cause `OutOfMemoryError`. Use `LinkedBlockingQueue` or `ArrayBlockingQueue` with fixed capacity for untrusted backpressure buffers.

---

## 9. Comparison / When to Use What

| Feature | `ArrayDeque` | `LinkedList` | `PriorityQueue` | `ArrayBlockingQueue` |
|---|---|---|---|---|
| **Backing Structure** | Circular Array | Doubly-Linked Nodes | Binary Min-Heap Array | Bounded Circular Array |
| **Queue Order** | FIFO | FIFO | Priority (Sorted) | FIFO (Concurrent) |
| **Stack (LIFO)** | Yes ($O(1)$) | Yes ($O(1)$) | No | No |
| **Thread-Safe** | No | No | No | Yes (ReentrantLock) |
| **Null Elements** | Prohibited | Allowed | Prohibited | Prohibited |

---

## 10. Interview Questions

<details>
<summary>1. Why is `ArrayDeque` better than `Stack` for LIFO stack operations?</summary>
`Stack` extends `Vector`, which synchronizes every method call on the instance lock, adding overhead. Furthermore, `Vector` allows violating stack semantics by exposing indexed methods (`insertElementAt`, `remove(index)`). `ArrayDeque` is unsynchronized, faster, and encapsulates stack operations cleanly.
</details>

<details>
<summary>2. How does `ArrayDeque` avoid array shifts when adding/removing from both ends?</summary>
It uses a circular buffer with `head` and `tail` pointers. Adding to the front decrements `head`; adding to the back increments `tail`. No elements are shifted until the capacity is exhausted and resizing occurs.
</details>

<details>
<summary>3. Why does `ArrayDeque` forbid `null` elements?</summary>
Because methods like `poll()`, `peek()`, `pollFirst()`, and `peekLast()` use `null` as a sentinel return value to indicate that the deque is empty. Allowing `null` elements would make it impossible to distinguish between an empty deque and an element that is `null`.
</details>

<details>
<summary>4. What is the difference between `add()` and `offer()` on a Queue?</summary>
On bounded queues, `add()` throws an `IllegalStateException` if the queue is full, whereas `offer()` returns `false`. On empty queues, `remove()` throws `NoSuchElementException`, while `poll()` returns `null`.
</details>

<details>
<summary>5. What is the time complexity of `ArrayDeque` operations?</summary>
Amortized $O(1)$ for insertion and removal at both ends (`addFirst`, `addLast`, `pollFirst`, `pollLast`).
</details>

<details>
<summary>6. How does `ArrayDeque` ensure power-of-two capacity?</summary>
The constructor calculates the next highest power of two (e.g., requesting capacity 10 allocates 16). This allows using bitwise AND `& (length - 1)` instead of the integer division `% length`.
</details>

<details>
<summary>7. When would you prefer `LinkedList` over `ArrayDeque`?</summary>
Almost never. The only scenario where `LinkedList` might be preferred is if you need an iterator that supports removing elements in the middle in $O(1)$ time while holding an `Iterator` reference, or when strict worst-case memory reallocation pauses cannot be tolerated.
</details>

<details>
<summary>8. How do Sequenced Collections (Java 21) affect `Deque`?</summary>
`Deque` directly extends `SequencedCollection`. Its existing methods `addFirst`, `addLast`, `getFirst`, `getLast`, `removeFirst`, `removeLast`, and `reversed()` seamlessly implement the sequenced contract.
</details>

---

## 11. Practice Exercises

### Easy
1. Implement a FIFO queue of customer names using `ArrayDeque` and process all entries.
2. Reverse a string using an `ArrayDeque` stack.
3. Write a helper that peeks at the top of a stack without removing it, returning `Optional<T>`.

### Medium
4. Implement a MinStack that supports `push`, `pop`, `top`, and retrieving the minimum element in $O(1)$ time.
5. Solve the classic "Sliding Window Maximum" problem using a monotonic `Deque`.
6. Implement an undo/redo manager using two `ArrayDeque` stacks.

### Hard
7. Implement a lock-free bounded MPMC (Multi-Producer Multi-Consumer) circular queue using atomic references.
8. Design a rate limiter that tracks request timestamps in a sliding window using a `Deque`.
9. Construct a memory-efficient zero-allocation deque for primitive integers.

<details>
<summary>Solutions</summary>

```java
// Easy 2: Reverse string with ArrayDeque
public static String reverseString(String input) {
    Deque<Character> stack = new ArrayDeque<>();
    for (char c : input.toCharArray()) stack.push(c);
    StringBuilder sb = new StringBuilder();
    while (!stack.isEmpty()) sb.append(stack.pop());
    return sb.toString();
}

// Medium 4: MinStack
public class MinStack {
    private final Deque<Integer> stack = new ArrayDeque<>();
    private final Deque<Integer> minStack = new ArrayDeque<>();

    public void push(int val) {
        stack.push(val);
        if (minStack.isEmpty() || val <= minStack.peek()) {
            minStack.push(val);
        }
    }

    public void pop() {
        if (!stack.isEmpty()) {
            int val = stack.pop();
            if (val == minStack.peek()) {
                minStack.pop();
            }
        }
    }

    public int top() { return stack.peek(); }
    public int getMin() { return minStack.peek(); }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Queue**: FIFO structure; `offer` (safe insert), `poll` (safe remove), `peek` (safe examine).
- **Deque**: Double-ended queue; supports FIFO and LIFO.
- **Stack Replacement**: Use `Deque<T> stack = new ArrayDeque<>()` with `push()`, `pop()`, `peek()`.
- **Circular Buffer**: `ArrayDeque` uses bitwise circular indexing; no array shifts.
- **Null Safety**: `ArrayDeque` strictly forbids `null`.

---

## 13. Further Reading
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 64 (Refer to objects by their interfaces).
- Oracle Java Documentation: *The Queue Interface and ArrayDeque*.
- Lea, Doug. *Concurrent Programming in Java* (Queue architectures).
