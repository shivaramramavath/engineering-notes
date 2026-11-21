# PriorityQueue Deep Dive

> Binary min-heap array representation, logarithmic sift-up/sift-down operations, linear-time heapify, and task scheduling.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [04_queue-and-deque.md](04_queue-and-deque.md), [05_comparable-and-comparator.md](05_comparable-and-comparator.md) |
| **Time to Read** | 24 minutes |
| **Status** | Active |
| **Java Version** | Java 1.5+ |

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

A standard `Queue` processes elements in FIFO (First-In, First-Out) order. However, many real-world engineering problems require processing elements based on **priority** rather than arrival order:
- Dijkstra's shortest path and A* pathfinding algorithms
- Huffman coding and data compression
- Task scheduling and rate limiting
- Merging $K$ sorted streams or database files
- Finding top-$K$ frequent elements in streaming data

`java.util.PriorityQueue` is an unbounded priority queue based on a **binary min-heap**. Elements are ordered according to their natural ordering (`Comparable`) or by a custom `Comparator` provided at construction time.

The head of the queue is the **least element** with respect to the specified ordering.

---

## 2. Core Concepts

### Min-Heap Property
A binary heap is a complete binary tree where each node satisfies the heap property:
- For any given node $C$, the value of its parent $P$ is less than or equal to the value of $C$ ($\text{val}(P) \le \text{val}(C)$).
- Consequently, the minimum element of the entire tree is always at the **root** ($O(1)$ access).

### Unordered Iteration Hazard
**Crucial Concept**: The iterator returned by `priorityQueue.iterator()` is **NOT guaranteed** to traverse elements in priority order! It simply scans the internal flat array in sequential index order. To extract elements in priority order, you must repeatedly call `poll()`.

---

## 3. How It Works Internally

### Array Representation of a Binary Heap
`PriorityQueue` does not allocate tree node objects. It stores the complete binary tree inside a flat array: `Object[] queue`.

For an element at index $i$:
- **Parent index**: `(i - 1) >>> 1`
- **Left child index**: `(i << 1) + 1`  ($2i + 1$)
- **Right child index**: `(i << 1) + 2`  ($2i + 2$)

```
Binary Tree View:
                  [ 10 ] (index 0)
                /        \
         [ 20 ]            [ 15 ] (indices 1, 2)
        /      \          /      \
    [ 30 ]    [ 40 ]  [ 50 ]    [ 25 ] (indices 3, 4, 5, 6)

Flat Array Representation:
Index:   [  0 ][  1 ][  2 ][  3 ][  4 ][  5 ][  6 ]
Items:   [ 10 ][ 20 ][ 15 ][ 30 ][ 40 ][ 50 ][ 25 ]
```

### Sift-Up Mechanics (`offer` / `add`)
When a new element is added:
1. It is placed at the end of the array (`queue[size++]`).
2. It is compared with its parent. If smaller than parent, it swaps places and moves up the tree ("sifts up").
3. Process repeats until heap invariant is restored.
4. Time Complexity: $O(\log N)$.

```
Sift-Up on inserting 5:
1. Append 5 at index 7 (child of 15 at index 2).
2. Compare 5 with parent 15 -> 5 is smaller! Swap!
3. Compare 5 with parent 10 (root) -> 5 is smaller! Swap!
4. 5 becomes the new root!
```

### Sift-Down Mechanics (`poll`)
When the minimum element is removed:
1. The root element (`queue[0]`) is saved for return.
2. The last element in the array is moved to root (`queue[0] = queue[--size]`).
3. It is compared with its smallest child. If larger than smallest child, it swaps and moves down ("sifts down").
4. Process repeats until heap invariant is restored.
5. Time Complexity: $O(\log N)$.

### Linear-Time Heapify: $O(N)$ Construction
Constructing a `PriorityQueue` from an existing collection (`new PriorityQueue<>(coll)`) does **not** call `offer()` $N$ times ($O(N \log N)$). It copies the array and runs the Floyd **heapify** algorithm from the lowest non-leaf node up to the root, constructing the heap in **$O(N)$ linear time**.

---

## 4. Syntax & Usage

### 1. Natural Order Min-Heap
```java
import java.util.PriorityQueue;
import java.util.Queue;

public class MinHeapDemo {
    public static void main(String[] args) {
        Queue<Integer> minHeap = new PriorityQueue<>();
        minHeap.offer(40);
        minHeap.offer(10);
        minHeap.offer(30);
        minHeap.offer(20);

        while (!minHeap.isEmpty()) {
            System.out.print(minHeap.poll() + " "); // 10 20 30 40
        }
        System.out.println();
    }
}
```

### 2. Max-Heap via Reverse Comparator
```java
import java.util.Collections;
import java.util.PriorityQueue;

public class MaxHeapDemo {
    public static void main(String[] args) {
        // Max-heap: largest element at head
        PriorityQueue<Integer> maxHeap = new PriorityQueue<>(Collections.reverseOrder());
        maxHeap.offer(15);
        maxHeap.offer(80);
        maxHeap.offer(45);

        System.out.println("Max element: " + maxHeap.poll()); // 80
    }
}
```

---

## 5. Practical Examples

### Example 1: Top-K Frequent Elements (Basic to Intermediate)
```java
import java.util.HashMap;
import java.util.Map;
import java.util.PriorityQueue;

public class TopKFrequent {
    public static void printTopK(int[] nums, int k) {
        Map<Integer, Integer> counts = new HashMap<>();
        for (int n : nums) counts.merge(n, 1, Integer::sum);

        // Min-heap of size K ordered by frequency
        PriorityQueue<Map.Entry<Integer, Integer>> minHeap = new PriorityQueue<>(
            (e1, e2) -> Integer.compare(e1.getValue(), e2.getValue())
        );

        for (Map.Entry<Integer, Integer> entry : counts.entrySet()) {
            minHeap.offer(entry);
            if (minHeap.size() > k) {
                minHeap.poll(); // Evict smallest frequency element!
            }
        }

        System.out.println("Top " + k + " frequent elements:");
        while (!minHeap.isEmpty()) {
            var entry = minHeap.poll();
            System.out.println("Number: " + entry.getKey() + " (Count: " + entry.getValue() + ")");
        }
    }

    public static void main(String[] args) {
        int[] nums = {1, 1, 1, 2, 2, 3, 4, 4, 4, 4, 5};
        printTopK(nums, 2); // 4 (count 4), 1 (count 3)
    }
}
```

### Example 2: Merging $K$ Sorted Streams (Real-World)
```java
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.PriorityQueue;

public class KWayMerge {
    record StreamCursor(int value, int listIndex, int elementIndex) {}

    public static List<Integer> mergeSortedLists(List<List<Integer>> lists) {
        PriorityQueue<StreamCursor> minHeap = new PriorityQueue<>(Comparator.comparingInt(StreamCursor::value));

        for (int i = 0; i < lists.size(); i++) {
            if (!lists.get(i).isEmpty()) {
                minHeap.offer(new StreamCursor(lists.get(i).get(0), i, 0));
            }
        }

        List<Integer> result = new ArrayList<>();
        while (!minHeap.isEmpty()) {
            StreamCursor current = minHeap.poll();
            result.add(current.value());

            int nextIndex = current.elementIndex() + 1;
            List<Integer> sourceList = lists.get(current.listIndex());
            if (nextIndex < sourceList.size()) {
                minHeap.offer(new StreamCursor(sourceList.get(nextIndex), current.listIndex(), nextIndex));
            }
        }
        return result;
    }

    public static void main(String[] args) {
        List<List<Integer>> lists = List.of(
            List.of(1, 4, 7),
            List.of(2, 5, 8),
            List.of(3, 6, 9)
        );
        System.out.println("Merged: " + mergeSortedLists(lists)); // [1, 2, 3, 4, 5, 6, 7, 8, 9]
    }
}
```

### Example 3: Edge Case - Iteration Does Not Return Sorted Elements
```java
import java.util.PriorityQueue;

public class UnsortedIterationTrap {
    public static void main(String[] args) {
        PriorityQueue<Integer> pq = new PriorityQueue<>();
        pq.offer(50); pq.offer(10); pq.offer(30); pq.offer(20);

        System.out.print("Iterator order: ");
        for (int n : pq) {
            System.out.print(n + " "); // Prints internal array layout, NOT sorted!
        }
        System.out.println();
    }
}
```

### Example 4: Failure Case - Modifying Elements While Inside Queue
```java
import java.util.PriorityQueue;

public class MutableElementHeapFailure {
    static class Task implements Comparable<Task> {
        int priority;
        Task(int priority) { this.priority = priority; }
        @Override
        public int compareTo(Task o) { return Integer.compare(this.priority, o.priority); }
    }

    public static void main(String[] args) {
        PriorityQueue<Task> pq = new PriorityQueue<>();
        Task t1 = new Task(10);
        Task t2 = new Task(20);
        pq.offer(t1);
        pq.offer(t2);

        t1.priority = 100; // MUTATED PRIORITY WHILE IN QUEUE! Heap invariants broken!

        // Head is still t1 even though t2 now has lower priority!
        System.out.println("Polled: " + pq.poll().priority); // 100 instead of 20!
    }
}
```

---

## 6. Best Practices

1. **Keep Heap Elements Strictly Immutable**: Mutating an object while inside a `PriorityQueue` corrupts the heap order without triggering a re-heapify.
2. **Never Iterate to Extract Sorted Elements**: Always use `while (!pq.isEmpty()) pq.poll()` to consume items in sorted priority order.
3. **Pre-size Capacity When Known**: Initialize with `new PriorityQueue<>(capacity)` to prevent internal array doubling and copying.
4. **Use Fixed-Size Heap for Top-K Problems**: A min-heap of size $K$ processes $N$ elements in $O(N \log K)$ time and $O(K)$ space.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Assuming `for (E e : pq)` traverses in priority order | Iterator returns elements in arbitrary flat-array order. | Use `while (!pq.isEmpty()) pq.poll()`. |
| Mutating priority fields while element is in queue | Heap invariant is violated; subsequent `poll()` returns incorrect elements. | Remove, mutate, and re-insert (or use immutable objects). |
| Passing `null` element | `PriorityQueue` throws `NullPointerException` because elements must be compared. | Never insert `null`. |
| Using `remove(Object o)` for priority removal | Arbitrary removal searches array in $O(N)$ and sifts down in $O(\log N)$. | Prefer `poll()` for root removal ($O(\log N)$). |
| Expecting thread safety | `PriorityQueue` is not thread-safe. | Use `PriorityBlockingQueue` for concurrent access. |
| Forgetting comparator when elements don't implement `Comparable` | Throws `ClassCastException` on insertion. | Pass custom `Comparator` to constructor. |

---

## 8. Performance & Security Notes

### Performance
- **Time Complexity Matrix**:
  - `peek()`: $O(1)$
  - `offer()` / `add()`: $O(\log N)$
  - `poll()`: $O(\log N)$
  - `remove(Object)`: $O(N)$ search + $O(\log N)$ sift
  - `new PriorityQueue<>(Collection)`: $O(N)$ linear-time heapify
- **Memory Efficiency**: Backed by a contiguous `Object[]` array; zero per-node object pointer overhead.

### Security
- **Denial of Service via Unbounded Growth**: `PriorityQueue` is unbounded. Malicious input streams can trigger continuous array doubling, exhausting JVM heap memory.

---

## 9. Comparison / When to Use What

| Feature | `PriorityQueue` | `TreeSet` | `PriorityBlockingQueue` |
|---|---|---|---|
| **Underlying Structure** | Array Binary Min-Heap | Red-Black Tree | Heap + ReentrantLock |
| **Duplicates Allowed?** | Yes | No (Strict uniqueness) | Yes |
| **Search (`contains`)** | $O(N)$ linear scan | $O(\log N)$ | $O(N)$ |
| **Peek / Poll Min** | $O(1)$ / $O(\log N)$ | $O(\log N)$ / $O(\log N)$ | $O(1)$ / $O(\log N)$ |
| **Thread-Safe?** | No | No | Yes |

---

## 10. Interview Questions

<details>
<summary>1. What data structure underlies Java's `PriorityQueue`?</summary>
A binary min-heap stored in a dynamically resizing contiguous array (`Object[] queue`).
</details>

<details>
<summary>2. How are child and parent indices calculated in `PriorityQueue`?</summary>
For index $i$: Parent is `(i - 1) >>> 1`. Left child is `2 * i + 1`. Right child is `2 * i + 2`.
</details>

<details>
<summary>3. Why does `pq.iterator()` not return elements in priority order?</summary>
The iterator traverses the internal flat backing array sequentially from index 0 to `size - 1`. The heap invariant only guarantees that parents are smaller than their children, not that array indices are strictly sorted.
</details>

<details>
<summary>4. What is the time complexity of `new PriorityQueue<>(existingCollection)`?</summary>
$O(N)$ linear time, utilizing Floyd's bottom-up heap construction algorithm (`heapify()`), rather than $O(N \log N)$ repeated insertions.
</details>

<details>
<summary>5. Can a `PriorityQueue` contain `null`?</summary>
No. Attempting to add `null` throws `NullPointerException` because elements must be compared with other elements.
</details>

<details>
<summary>6. How do you create a Max-Heap using `PriorityQueue`?</summary>
`new PriorityQueue<>(Collections.reverseOrder())` or by providing a custom comparator `(a, b) -> Integer.compare(b, a)`.
</details>

<details>
<summary>7. What is the time complexity of `remove(Object o)` in `PriorityQueue`?</summary>
$O(N)$. It must perform a linear scan to find the object index in the array, followed by an $O(\log N)$ sift-up or sift-down operation.
</details>

<details>
<summary>8. How do you solve the Top-K largest elements problem in $O(N \log K)$ time?</summary>
Maintain a **min-heap** of capacity $K$. For each incoming element, add it to the heap; if heap size exceeds $K$, call `poll()` to evict the smallest element. The remaining $K$ elements are the top-$K$ largest.
</details>

---

## 11. Practice Exercises

### Easy
1. Create a `PriorityQueue<Integer>`, insert 10 random integers, and print them in ascending order using `poll()`.
2. Construct a Max-Heap for strings ordered by string length.
3. Check the top element of a priority queue without removing it.

### Medium
4. Implement a running median tracker that returns the median of a streaming dataset in $O(1)$ time using two priority queues (min-heap and max-heap).
5. Merge $K$ sorted linked lists using a `PriorityQueue`.
6. Implement a CPU job scheduler that orders tasks by priority, and breaks ties by earliest submission time.

### Hard
7. Implement an indexable priority queue (Indexed Priority Queue) supporting $O(\log N)$ updates to arbitrary elements by key.
8. Design a cache with time-based TTL expiration using a `PriorityQueue` and compare against `DelayQueue`.
9. Implement a D-ary heap (where each node has $D$ children) and compare cache performance against standard binary `PriorityQueue`.

<details>
<summary>Solutions</summary>

```java
// Easy 1: Ascending poll
PriorityQueue<Integer> pq = new PriorityQueue<>(List.of(5, 1, 9, 3, 7));
while (!pq.isEmpty()) System.out.print(pq.poll() + " "); // 1 3 5 7 9

// Medium 4: Running Median
public class MedianFinder {
    private final PriorityQueue<Integer> maxHeap = new PriorityQueue<>(Collections.reverseOrder());
    private final PriorityQueue<Integer> minHeap = new PriorityQueue<>();

    public void addNum(int num) {
        maxHeap.offer(num);
        minHeap.offer(maxHeap.poll());
        if (minHeap.size() > maxHeap.size()) {
            maxHeap.offer(minHeap.poll());
        }
    }

    public double findMedian() {
        if (maxHeap.size() > minHeap.size()) return maxHeap.peek();
        return (maxHeap.peek() + minHeap.peek()) / 2.0;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Data Structure**: Binary min-heap stored in an `Object[]` array.
- **Root Element**: Smallest element at `queue[0]` ($O(1)$ peek).
- **Insert / Remove**: $O(\log N)$ via sift-up (`offer`) and sift-down (`poll`).
- **Heapify**: $O(N)$ linear construction from existing collection.
- **Iteration Trap**: `pq.iterator()` is NOT sorted; use `poll()` in a loop.
- **Null Forbidden**: Throws `NullPointerException` on `null`.

---

## 13. Further Reading
- Cormen, Leiserson, Rivest, Stein. *Introduction to Algorithms (CLRS)*: Chapter 6 (Heapsort and Priority Queues).
- OpenJDK: `java.util.PriorityQueue` implementation source.
- Sedgewick, Robert and Wayne, Kevin. *Algorithms (4th Edition)*: Section 2.4 (Priority Queues).
