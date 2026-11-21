# Heaps and Priority Queues

> Complete binary tree-based priority queues providing $O(1)$ extremum access, $O(\log N)$ insertion and extraction, and $O(N)$ linear-time heap construction.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01_arrays-and-strings.md](01_arrays-and-strings.md), [07_trees-binary-and-bst.md](07_trees-binary-and-bst.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java Version** | Java 8+ (Verified on Java 25 LTS) |

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

A **Binary Heap** is a complete binary tree that satisfies the **Heap Property**:
- **Min-Heap Property**: The key at every node is less than or equal to the keys of its children. The minimum element always resides at the root.
- **Max-Heap Property**: The key at every node is greater than or equal to the keys of its children. The maximum element always resides at the root.

### The Problem Solved
When processing high-velocity streaming data, order processing systems, and graph traversal algorithms (such as Dijkstra's shortest path or Prim's MST), we constantly need:
1. Instant access to the highest-priority item ($O(1)$).
2. Efficient insertion of newly arriving tasks ($O(\log N)$).
3. Immediate removal of the top-priority task ($O(\log N)$).

Using an unsorted list makes extraction $O(N)$. Using a fully sorted list makes insertion $O(N)$. A Binary Heap strikes the optimal algorithmic balance: $O(1)$ peek and $O(\log N)$ updates, implemented within a compact, contiguous array with zero pointer overhead.

---

## 2. Core Concepts

1. **Complete Binary Tree**: Every tree level, except possibly the last, is completely filled, and all leaf nodes in the last level are packed as far left as possible.
2. **Array Index Mapping**:
   For an element at 0-based array index $i$:
   - **Parent**: `parent(i) = (i - 1) / 2`
   - **Left Child**: `left(i) = 2 * i + 1`
   - **Right Child**: `right(i) = 2 * i + 2`
3. **Sift-Up (Bubble-Up)**: Used during `insert()`. Place the new element at the end of the array, then bubble it up by swapping with its parent until the heap property is restored. Worst-case: $O(\log N)$.
4. **Sift-Down (Bubble-Down / Heapify)**: Used during `extractMin()` / `poll()`. Replace root with the last element of the array, shrink size by 1, and sift this element down by swapping with its smaller child. Worst-case: $O(\log N)$.
5. **Linear-Time Construction ($O(N)$ Heapify)**: Calling sift-down from the last internal node `(N/2 - 1)` down to index 0 transforms an arbitrary array into a valid heap in strictly $O(N)$ operations.

---

## 3. How It Works Internally

### Mathematical Proof of $O(N)$ `buildHeap`
Why does bottom-up `buildHeap` run in $O(N)$ rather than $O(N \log N)$?
In a complete tree with $N$ nodes:
- Half the nodes are leaves (height $h = 0$) and require 0 swaps.
- A quarter of the nodes are at height $h = 1$ and require at most 1 swap.
- In general, at height $h$, there are at most $\lceil N / 2^{h+1} \rceil$ nodes, each taking at most $h$ swaps.

Total swaps:
$$S = \sum_{h=0}^{\lfloor \log N \rfloor} \frac{N}{2^{h+1}} h = \frac{N}{2} \sum_{h=0}^{\infty} \frac{h}{2^h}$$
Since the geometric-arithmetic series $\sum_{h=0}^{\infty} \frac{h}{2^h} = 2$, we have:
$$S = \frac{N}{2} \times 2 = N = O(N)$$

### HotSpot JVM `java.util.PriorityQueue` Internals
```
+-----------------------------------------------------------+
| PriorityQueue Object Header: 12 bytes                     |
| Object[] queue: reference (4 bytes)                       |
| int size: 4 bytes                                         |
| Comparator comparator: reference (4 bytes)                |
| int modCount: 4 bytes                                     |
+-----------------------------------------------------------+
        |
        v
+-----------------------------------------------------------+
| Object[] queue (contiguous heap memory)                   |
| [0] Min element                                           |
| [1] Left child of [0]                                     |
| [2] Right child of [0]                                    |
| [3] Left child of [1] ...                                 |
+-----------------------------------------------------------+
```

When capacity is exhausted, HotSpot resizes:
```java
int newCapacity = oldCapacity + ((oldCapacity < 64) ?
                                 (oldCapacity + 2) :
                                 (oldCapacity >> 1)); // 50% growth
```

---

## 4. Syntax & Usage

### Basic PriorityQueue Operations in Java

```java
import java.util.*;

public class PriorityQueueBasics {
    public static void main(String[] args) {
        // Natural ordering (Min-Heap by default)
        PriorityQueue<Integer> minHeap = new PriorityQueue<>();
        minHeap.offer(40);
        minHeap.offer(10);
        minHeap.offer(30);

        System.out.println(minHeap.peek()); // 10
        System.out.println(minHeap.poll()); // 10 (removes)
        System.out.println(minHeap.poll()); // 30

        // Custom Comparator: Max-Heap
        PriorityQueue<Integer> maxHeap = new PriorityQueue<>(Collections.reverseOrder());
        maxHeap.addAll(List.of(15, 80, 42));
        System.out.println(maxHeap.peek()); // 80

        // Object Comparator with lambda
        record Job(String id, int priority) {}
        PriorityQueue<Job> jobQueue = new PriorityQueue<>(
            Comparator.comparingInt(Job::priority).reversed()
        );
        jobQueue.offer(new Job("email", 1));
        jobQueue.offer(new Job("auth", 10));
        System.out.println(jobQueue.poll().id()); // "auth"
    }
}
```

---

## 5. Practical Examples

### Example 1: Kth Largest Element in an Array (LeetCode 215)
Using a Min-Heap of bounded size $K$:

```java
import java.util.PriorityQueue;

public class KthLargestElement {
    public int findKthLargest(int[] nums, int k) {
        // Min-heap maintains the top k largest elements seen so far
        PriorityQueue<Integer> minHeap = new PriorityQueue<>(k);
        for (int num : nums) {
            minHeap.offer(num);
            if (minHeap.size() > k) {
                minHeap.poll(); // Evict smallest among the k+1 elements
            }
        }
        return minHeap.peek(); // Top of min-heap is the kth largest
    }
}
```

### Example 2: Top K Frequent Elements (LeetCode 347)

```java
import java.util.*;

public class TopKFrequentElements {
    public int[] topKFrequent(int[] nums, int k) {
        Map<Integer, Integer> freqMap = new HashMap<>();
        for (int n : nums) {
            freqMap.put(n, freqMap.getOrDefault(n, 0) + 1);
        }

        // Min-heap ordering by frequency ascending
        PriorityQueue<Map.Entry<Integer, Integer>> pq = new PriorityQueue<>(
            Comparator.comparingInt(Map.Entry::getValue)
        );

        for (Map.Entry<Integer, Integer> entry : freqMap.entrySet()) {
            pq.offer(entry);
            if (pq.size() > k) {
                pq.poll();
            }
        }

        int[] result = new int[k];
        for (int i = 0; i < k; i++) {
            result[i] = pq.poll().getKey();
        }
        return result;
    }
}
```

### Example 3: Merge K Sorted Lists (LeetCode 23)

```java
import java.util.PriorityQueue;
import java.util.Comparator;

public class MergeKSortedLists {
    public static class ListNode {
        public int val;
        public ListNode next;
        public ListNode(int val) { this.val = val; }
    }

    public ListNode mergeKLists(ListNode[] lists) {
        if (lists == null || lists.length == 0) return null;

        PriorityQueue<ListNode> pq = new PriorityQueue<>(
            lists.length, Comparator.comparingInt(node -> node.val)
        );

        for (ListNode head : lists) {
            if (head != null) pq.offer(head);
        }

        ListNode dummy = new ListNode(0);
        ListNode tail = dummy;

        while (!pq.isEmpty()) {
            ListNode smallest = pq.poll();
            tail.next = smallest;
            tail = tail.next;

            if (smallest.next != null) {
                pq.offer(smallest.next);
            }
        }

        return dummy.next;
    }
}
```

### Example 4: Find Median from Data Stream (LeetCode 295, Dual Heaps)

```java
import java.util.PriorityQueue;
import java.util.Collections;

public class MedianFinder {
    // maxHeap stores the lower half of numbers
    private final PriorityQueue<Integer> maxHeap = new PriorityQueue<>(Collections.reverseOrder());
    // minHeap stores the upper half of numbers
    private final PriorityQueue<Integer> minHeap = new PriorityQueue<>();

    public void addNum(int num) {
        maxHeap.offer(num);
        minHeap.offer(maxHeap.poll()); // Balance value ordering

        // Balance heap sizes (maxHeap holds equal or 1 more than minHeap)
        if (minHeap.size() > maxHeap.size()) {
            maxHeap.offer(minHeap.poll());
        }
    }

    public double findMedian() {
        if (maxHeap.size() > minHeap.size()) {
            return maxHeap.peek();
        }
        return (maxHeap.peek() + minHeap.peek()) / 2.0;
    }
}
```

### Example 5: Task Scheduler (LeetCode 621)

```java
import java.util.*;

public class TaskScheduler {
    public int leastInterval(char[] tasks, int n) {
        int[] counts = new int[26];
        for (char c : tasks) counts[c - 'A']++;

        PriorityQueue<Integer> maxHeap = new PriorityQueue<>(Collections.reverseOrder());
        for (int c : counts) {
            if (c > 0) maxHeap.offer(c);
        }

        int time = 0;
        // Queue pairs: [frequency, readyTimestamp]
        Queue<int[]> cooldownQueue = new ArrayDeque<>();

        while (!maxHeap.isEmpty() || !cooldownQueue.isEmpty()) {
            time++;
            if (!maxHeap.isEmpty()) {
                int rem = maxHeap.poll() - 1;
                if (rem > 0) {
                    cooldownQueue.offer(new int[]{rem, time + n});
                }
            }

            if (!cooldownQueue.isEmpty() && cooldownQueue.peek()[1] == time) {
                maxHeap.offer(cooldownQueue.poll()[0]);
            }
        }

        return time;
    }
}
```

---

## 6. Best Practices

1. **Initial Capacity**: If you know the bounded window size $K$, always specify `new PriorityQueue<>(K)` to avoid unnecessary array reallocations.
2. **Never Mutate Elements in a Heap**: If an object's fields change such that its comparison priority shifts, `PriorityQueue` will NOT re-heapify automatically, corrupting internal invariant state. If mutation is required, `remove()` the element first or use an indexed priority queue.
3. **Avoid Iterating Over `PriorityQueue` Expecting Sorted Order**: The iterator yields elements in array storage order, NOT sorted sequence! To process in sorted order, drain using `while (!pq.isEmpty()) pq.poll()`.
4. **Prefer `offer()` and `poll()` over `add()` and `remove()`**: `offer()` and `poll()` return `false` / `null` gracefully instead of throwing exceptions.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Comparator subtraction: `(a, b) -> a.val - b.val` | Integer overflow when `a.val` is very negative and `b.val` is positive | Use `Integer.compare(a.val, b.val)` or `Comparator.comparingInt()`. |
| Expecting `pq.iterator()` to be sorted | The backing array is a partially ordered binary heap | Use `pq.poll()` repeatedly or copy to an array and call `Arrays.sort()`. |
| Calling `pq.remove(object)` in tight loops | Linear search $O(N)$ scanning the backing array | Maintain an index lookup table (Indexed Priority Queue). |
| Putting non-Comparable objects without a Comparator | Throws `ClassCastException` at runtime | Provide a explicit `Comparator` or implement `Comparable<T>`. |
| Null elements in `PriorityQueue` | HotSpot explicitly forbids `null` elements (throws NPE) | Filter nulls or wrap inside an `Optional` or custom sentinel. |

---

## 8. Performance & Security Notes

- **Time Complexities**:
  - `peek()`: $O(1)$
  - `offer()` / `poll()`: $O(\log N)$
  - `remove(Object)` / `contains(Object)`: $O(N)$
  - `buildHeap` (bulk array heapify): $O(N)$
- **CPU Cache Performance**: Binary heaps in flat arrays have excellent cache locality for root and shallow levels, but deeper jumps (`2*i + 1`) may trigger cache misses. For massive datasets exceeding cache lines, a $d$-ary heap ($d=4$ or $d=8$) often outperforms a binary heap by reducing tree height.

---

## 9. Comparison / When to Use What

| Feature | `PriorityQueue` | `TreeSet` | `ArrayList` + `Collections.sort()` |
|---|---|---|---|
| **Top Element Access** | $O(1)$ | $O(\log N)$ | $O(1)$ (if kept sorted) |
| **Insert** | $O(\log N)$ | $O(\log N)$ | $O(N)$ |
| **Delete Arbitrary** | $O(N)$ | $O(\log N)$ | $O(N)$ |
| **Duplicate Keys** | Supported | Disallowed (Set) | Supported |
| **Memory Overhead** | Minimal (flat array) | High (32 bytes/node) | Minimal |

---

## 10. Interview Questions

<details>
<summary>1. Why does building a heap from an array take $O(N)$ time instead of $O(N \log N)$?</summary>

Because the majority of nodes in a complete binary tree reside at the bottom. Half the nodes are leaves requiring 0 operations; a quarter require at most 1 swap. The sum $\sum rac{h}{2^h}$ converges to 2, bounding the total operations strictly to $O(N)$.
</details>

<details>
<summary>2. How would you design an Indexed Priority Queue?</summary>

Maintain three arrays: `keys[]` (stores payload), `pq[]` (stores index of keys representing the heap layout), and `qp[]` (stores the inverse mapping: `qp[i]` is the position of key `i` in `pq[]`). This allows $O(1)$ lookup and $O(\log N)$ updates or removals for arbitrary elements by ID.
</details>

<details>
<summary>3. Can `java.util.PriorityQueue` accept duplicate elements?</summary>

Yes. Binary heaps do not enforce uniqueness. Multiple elements with identical priority keys are stored alongside each other without conflict.
</details>

<details>
<summary>4. What happens if an element inside a `PriorityQueue` is mutated?</summary>

The heap property is violated. The modified element will not move to its correct position. Subsequent `poll()` operations may return elements out of priority order, silently corrupting results.
</details>

<details>
<summary>5. How do you find the K largest elements in a stream of $N$ items where $N \gg K$?</summary>

Use a **Min-Heap** of fixed size $K$. For each incoming element, if heap size $< K$, insert it. If the incoming element is greater than `minHeap.peek()`, pop the root and insert the new element. At the end, the heap contains the $K$ largest elements in $O(N \log K)$ time and $O(K)$ space.
</details>

<details>
<summary>6. Why does `PriorityQueue` throw `NullPointerException` when offering `null`?</summary>

Because `poll()` returns `null` to signal an empty queue. If `null` were allowed as a valid element, callers could not distinguish between a drained queue and an offered `null`.
</details>

<details>
<summary>7. What is a Fibonacci Heap and why is it not used in standard Java collections?</summary>

A Fibonacci Heap offers $O(1)$ amortized insert and `decreaseKey()` operations. However, its constant factors are extremely large, node structure is complex with numerous pointers, and it has poor cache locality. For practical applications, simple binary heaps consistently outperform Fibonacci heaps.
</details>

<details>
<summary>8. How do you implement a Max-Heap using Java's default `PriorityQueue`?</summary>

Pass `Collections.reverseOrder()` or `(a, b) -> Integer.compare(b, a)` into the `PriorityQueue` constructor:
`PriorityQueue<Integer> maxHeap = new PriorityQueue<>(Collections.reverseOrder());`
</details>

---

## 11. Practice Exercises

### Exercise 1: Reorganize String (LeetCode 767)
Given a string `s`, rearrange the characters such that no two adjacent characters are identical. Return `""` if impossible.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class ReorganizeString {
    public String reorganizeString(String s) {
        int[] counts = new int[26];
        for (char c : s.toCharArray()) counts[c - 'a']++;

        PriorityQueue<int[]> maxHeap = new PriorityQueue<>((a, b) -> b[1] - a[1]);
        for (int i = 0; i < 26; i++) {
            if (counts[i] > 0) {
                if (counts[i] > (s.length() + 1) / 2) return "";
                maxHeap.offer(new int[]{i, counts[i]});
            }
        }

        StringBuilder sb = new StringBuilder();
        while (maxHeap.size() >= 2) {
            int[] first = maxHeap.poll();
            int[] second = maxHeap.poll();

            sb.append((char) ('a' + first[0]));
            sb.append((char) ('a' + second[0]));

            if (--first[1] > 0) maxHeap.offer(first);
            if (--second[1] > 0) maxHeap.offer(second);
        }

        if (!maxHeap.isEmpty()) {
            sb.append((char) ('a' + maxHeap.poll()[0]));
        }

        return sb.toString();
    }
}
```
</details>

### Exercise 2: Minimum Cost to Connect Sticks (LeetCode 1167)
You have sticks with positive lengths. You can connect any two sticks of lengths $x$ and $y$ at cost $x + y$. Find minimum cost to connect all sticks into one.

<details>
<summary>Solution</summary>

```java
import java.util.PriorityQueue;

public class ConnectSticks {
    public int connectSticks(int[] sticks) {
        PriorityQueue<Integer> minHeap = new PriorityQueue<>();
        for (int stick : sticks) {
            minHeap.offer(stick);
        }

        int totalCost = 0;
        while (minHeap.size() > 1) {
            int first = minHeap.poll();
            int second = minHeap.poll();
            int cost = first + second;
            totalCost += cost;
            minHeap.offer(cost);
        }

        return totalCost;
    }
}
```
</details>

### Exercise 3: IPO — Maximize Capital (LeetCode 502)
Given capital constraints and profits for projects, find maximum maximized capital after finishing at most $k$ distinct projects.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class IPO {
    public int findMaximizedCapital(int k, int w, int[] profits, int[] capital) {
        int n = profits.length;
        int[][] projects = new int[n][2];
        for (int i = 0; i < n; i++) {
            projects[i][0] = capital[i];
            projects[i][1] = profits[i];
        }

        // Sort projects by required capital ascending
        Arrays.sort(projects, Comparator.comparingInt(a -> a[0]));

        // Max-heap stores profits of all currently affordable projects
        PriorityQueue<Integer> availableProfits = new PriorityQueue<>(Collections.reverseOrder());
        int idx = 0;

        for (int i = 0; i < k; i++) {
            while (idx < n && projects[idx][0] <= w) {
                availableProfits.offer(projects[idx][1]);
                idx++;
            }

            if (availableProfits.isEmpty()) break;
            w += availableProfits.poll();
        }

        return w;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Heap Structure**: Complete binary tree encoded in a 1D array (`parent=(i-1)/2`, `left=2i+1`, `right=2i+2`).
- **Core Costs**: $O(1)$ peek, $O(\log N)$ offer/poll, $O(N)$ build heap.
- **Top-K Pattern**: Use a **Min-Heap** of size $K$ to track the $K$ largest elements.
- **Dual Heaps Pattern**: Balance Max-Heap (lower half) and Min-Heap (upper half) for continuous median calculations.
- **Do Not Mutate**: Never change object state while it is stored in a `PriorityQueue`.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 6: Heapsort](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [OpenJDK Source Code: `java.util.PriorityQueue`](https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/util/PriorityQueue.java)
- [Baeldung: Guide to PriorityQueue in Java](https://www.baeldung.com/java-priority-queue)
- [Robert Sedgewick: Algorithms, 4th Edition: Priority Queues](https://algs4.cs.princeton.edu/24pq/)
- [LeetCode Heap Tagged Problems](https://leetcode.com/tag/heap-priority-queue/)
