# Binary Heaps & Priority Queues in JavaScript

> A production-grade algorithmic guide to Min-Heaps, Max-Heaps, and Priority Queues in JavaScript, covering array-based binary trees, sift-up/down invariants, linear-time heap construction, and top-K streaming algorithms.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [06_trees-and-binary-trees.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/06_trees-and-binary-trees.md) |
| **Standard Baseline** | ECMAScript 2024 / Node.js 20+ LTS / V8 Engine |
| **Status** | Production Ready |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In operating system schedulers, network packet prioritizers, and graph search algorithms (e.g. Dijkstra's shortest path, Prim's minimum spanning tree), applications need to retrieve the **highest or lowest priority element dynamically**.

Using a standard array or linked list creates a severe performance bottleneck:
- **Unsorted Array**: $O(1)$ insertion, but finding/extracting the minimum element requires an $O(N)$ scan.
- **Sorted Array**: $O(1)$ minimum extraction, but inserting a new element requires $O(N)$ memory shifting.

A **Binary Heap** is a specialized complete binary tree that balances these trade-offs:
- **Instant Minimum/Maximum Access**: $O(1)$ constant-time `peek()`.
- **Logarithmic Mutation**: $O(\log N)$ insertion and deletion (`push` and `pop`).
- **Cache-Optimized Compact Memory**: Stored directly in a contiguous flat array with **zero pointer overhead**.

---

## 2. Core Concepts

### Min-Heap vs. Max-Heap
- **Min-Heap Property**: For every node $i$, the value of $i$ is less than or equal to the values of its children:
  $$\text{Heap}[i] \le \text{Heap}[2i + 1] \quad \text{and} \quad \text{Heap}[i] \le \text{Heap}[2i + 2]$$
  The global minimum element is always at the root (`index 0`).
- **Max-Heap Property**: For every node $i$, the value of $i$ is greater than or equal to its children. The global maximum is at the root.

```
Min-Heap Tree:                         Equivalent Flat Array:
        [ 10 ]                         Index: 0   1   2   3   4   5
        /    \                         Value: [10, 15, 20, 17, 25, 30]
     [ 15 ]  [ 20 ]
     /    \    /
  [ 17 ] [25] [30]
```

### Complete Binary Tree Array Mapping (0-Indexed)
For any element at index $i$:
- **Parent Index**: $\lfloor (i - 1) / 2 \rfloor$
- **Left Child Index**: $2i + 1$
- **Right Child Index**: $2i + 2$

---

## 3. How It Works Internally

### 1. `push(val)` — Sift-Up (Bubble-Up)
1. Append the new element to the end of the array ($O(1)$ amortized).
2. Compare the element with its parent. If the heap invariant is violated (e.g. child $<$ parent in min-heap), swap them.
3. Repeat up the tree until the invariant is restored or the root is reached ($O(\log N)$).

### 2. `pop()` — Sift-Down (Bubble-Down)
1. Cache root element (`index 0`).
2. Move the last element of the array to the root position (`index 0`), popping the last slot.
3. Compare the new root with its smallest child. If invariant is violated, swap with the smaller child.
4. Repeat down the tree until the invariant is restored or a leaf is reached ($O(\log N)$).

```
Pop Root (10) Sift-Down:
Step 1: Replace Root with Last Element (30): [ 30, 15, 20, 17, 25 ]
Step 2: 30 > Children (15, 20). Swap with smaller (15): [ 15, 30, 20, 17, 25 ]
Step 3: 30 > Children (17, 25). Swap with smaller (17): [ 15, 17, 20, 30, 25 ]
Finished!
```

---

## 4. Syntax & API Reference

### Full Generic PriorityQueue / Binary Heap Implementation
```javascript
/**
 * Production-ready Priority Queue implemented using a Binary Heap.
 * Supports custom comparator for Min-Heap, Max-Heap, or complex object prioritization.
 */
export class PriorityQueue {
  constructor(comparator = (a, b) => a - b) {
    this.heap = [];
    this.comparator = comparator; // Default: Min-Heap
  }

  size() {
    return this.heap.length;
  }

  isEmpty() {
    return this.heap.length === 0;
  }

  peek() {
    return this.heap.length > 0 ? this.heap[0] : undefined;
  }

  push(val) {
    this.heap.push(val);
    this._siftUp(this.heap.length - 1);
  }

  pop() {
    if (this.isEmpty()) return undefined;

    const root = this.heap[0];
    const last = this.heap.pop();

    if (this.heap.length > 0) {
      this.heap[0] = last;
      this._siftDown(0);
    }

    return root;
  }

  _siftUp(idx) {
    while (idx > 0) {
      const parentIdx = Math.floor((idx - 1) / 2);
      // If child has higher priority than parent (comparator returns negative)
      if (this.comparator(this.heap[idx], this.heap[parentIdx]) < 0) {
        this._swap(idx, parentIdx);
        idx = parentIdx;
      } else {
        break;
      }
    }
  }

  _siftDown(idx) {
    const length = this.heap.length;

    while (true) {
      const leftChild = 2 * idx + 1;
      const rightChild = 2 * idx + 2;
      let highestPriority = idx;

      if (leftChild < length && this.comparator(this.heap[leftChild], this.heap[highestPriority]) < 0) {
        highestPriority = leftChild;
      }

      if (rightChild < length && this.comparator(this.heap[rightChild], this.heap[highestPriority]) < 0) {
        highestPriority = rightChild;
      }

      if (highestPriority !== idx) {
        this._swap(idx, highestPriority);
        idx = highestPriority;
      } else {
        break;
      }
    }
  }

  _swap(i, j) {
    const temp = this.heap[i];
    this.heap[i] = this.heap[j];
    this.heap[j] = temp;
  }
}
```

---

## 5. Practical Examples

### Example 1: $K$-th Largest Element in an Array (Min-Heap of Size $K$)
```javascript
import { PriorityQueue } from './PriorityQueue.js';

/**
 * Finds the K-th largest element using a Min-Heap of bounded size K.
 * Time Complexity: O(N log K) | Space Complexity: O(K)
 */
export function findKthLargest(nums, k) {
  const minHeap = new PriorityQueue((a, b) => a - b);

  for (const num of nums) {
    minHeap.push(num);
    // Evict smallest element when size exceeds K
    if (minHeap.size() > k) {
      minHeap.pop();
    }
  }

  return minHeap.peek(); // Root of size-K min-heap is K-th largest element
}

// console.log(findKthLargest([3, 2, 1, 5, 6, 4], 2)); // 5
```

### Example 2: Top $K$ Frequent Elements
```javascript
import { PriorityQueue } from './PriorityQueue.js';

export function topKFrequent(nums, k) {
  const frequencies = new Map();
  for (const num of nums) {
    frequencies.set(num, (frequencies.get(num) || 0) + 1);
  }

  // Min-Heap ordered by frequency count
  const minHeap = new PriorityQueue((a, b) => a[1] - b[1]);

  for (const entry of frequencies.entries()) {
    minHeap.push(entry);
    if (minHeap.size() > k) {
      minHeap.pop();
    }
  }

  const result = [];
  while (!minHeap.isEmpty()) {
    result.push(minHeap.pop()[0]);
  }

  return result.reverse();
}
```

### Example 3: Continuous Median Stream Tracker (Dual Heaps)
```javascript
import { PriorityQueue } from './PriorityQueue.js';

/**
 * Tracks the running median of a data stream in O(log N) insert and O(1) read.
 */
export class MedianFinder {
  constructor() {
    this.maxHeap = new PriorityQueue((a, b) => b - a); // Lower half
    this.minHeap = new PriorityQueue((a, b) => a - b); // Upper half
  }

  addNum(num) {
    if (this.maxHeap.isEmpty() || num <= this.maxHeap.peek()) {
      this.maxHeap.push(num);
    } else {
      this.minHeap.push(num);
    }

    // Rebalance heaps so size difference is at most 1
    if (this.maxHeap.size() > this.minHeap.size() + 1) {
      this.minHeap.push(this.maxHeap.pop());
    } else if (this.minHeap.size() > this.maxHeap.size()) {
      this.maxHeap.push(this.minHeap.pop());
    }
  }

  findMedian() {
    if (this.maxHeap.size() > this.minHeap.size()) {
      return this.maxHeap.peek();
    }
    return (this.maxHeap.peek() + this.minHeap.peek()) / 2;
  }
}
```

### Example 4: Merge $K$ Sorted Linked Lists
```javascript
import { PriorityQueue } from './PriorityQueue.js';
import { ListNode } from './04_linked-lists.js';

export function mergeKLists(lists) {
  const minHeap = new PriorityQueue((a, b) => a.val - b.val);

  for (const head of lists) {
    if (head) minHeap.push(head);
  }

  const dummy = new ListNode(0);
  let tail = dummy;

  while (!minHeap.isEmpty()) {
    const smallestNode = minHeap.pop();
    tail.next = smallestNode;
    tail = tail.next;

    if (smallestNode.next) {
      minHeap.push(smallestNode.next);
    }
  }

  return dummy.next;
}
```

### Example 5: Task Scheduler with Cooldown & Frequency Max-Heap
```javascript
import { PriorityQueue } from './PriorityQueue.js';

export function leastInterval(tasks, n) {
  const counts = new Map();
  for (const t of tasks) counts.set(t, (counts.get(t) || 0) + 1);

  // Max-Heap ordered by remaining frequency
  const maxHeap = new PriorityQueue((a, b) => b - a);
  for (const count of counts.values()) maxHeap.push(count);

  const cooldownQueue = []; // Array of [remainingCount, readyTime]
  let time = 0;

  while (!maxHeap.isEmpty() || cooldownQueue.length > 0) {
    time++;

    if (!maxHeap.isEmpty()) {
      const remaining = maxHeap.pop() - 1;
      if (remaining > 0) {
        cooldownQueue.push([remaining, time + n]);
      }
    }

    if (cooldownQueue.length > 0 && cooldownQueue[0][1] === time) {
      maxHeap.push(cooldownQueue.shift()[0]);
    }
  }

  return time;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Mutating Stored Object Properties Outside the Heap**:
   If an object stored inside a Priority Queue has its priority property mutated directly in memory (`task.priority = 10`), the internal array invariant is broken without the heap knowing. The object will not reposition itself. Always pop the object, mutate its property, and re-push it to trigger `_siftUp` / `_siftDown`.
2. **0-Indexed vs 1-Indexed Arithmetic**:
   In a 0-indexed array, child calculations are $2i + 1$ and $2i + 2$, with parent at $\lfloor (i - 1) / 2 \rfloor$. Using 1-indexed formulas ($2i$, $2i + 1$) on a 0-indexed array will skip index 0 and corrupt tree links.
3. **Array Reallocation Spikes on Mass Insertion**:
   Calling `push()` $N$ times individually takes $O(N \log N)$ total time. When constructing a heap from an existing array, use the **$O(N)$ linear-time Heapify algorithm** (sifting down starting from the last non-leaf parent at index $\lfloor N/2 \rfloor - 1$ down to 0).
4. **Comparator Return Values**:
   Custom comparators must return negative if the first element has higher priority, positive if lower, and zero if equal. Returning booleans (`true`/`false`) breaks numerical priority logic in JavaScript.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Defect | Production Remediation |
| :--- | :--- | :--- |
| `array.sort()` on every insertion | Degrades insertion into an **$O(N \log N)$** bottleneck. | Use a Binary Heap to insert in $O(\log N)$ and peek in $O(1)$. |
| Mutating object priority while inside heap | Breaks heap invariant; subsequent pops return wrong elements. | Pop object, mutate priority, and re-insert into heap. |
| Inverting comparator signs | Turns Min-Heap into Max-Heap unintentionally. | Min-Heap: `(a, b) => a - b`; Max-Heap: `(a, b) => b - a`. |
| Sifting down without boundary checks | Accesses array elements out of bounds, producing `undefined` comparisons. | Check `leftChild < length` and `rightChild < length`. |
| Building heap via $N$ pushes | Takes $O(N \log N)$ time instead of optimal $O(N)$. | Use bottom-up `heapify` starting at parent indices down to 0. |
| Calling `pop()` on empty heap without checking | Causes index errors or corrupts heap array length. | Guard with `if (this.isEmpty()) return undefined`. |

---

## 8. Performance Characteristics

| Operation | Binary Heap | Unsorted Array | Sorted Array |
| :--- | :--- | :--- | :--- |
| **`peek()` (Min/Max)** | $O(1)$ | $O(N)$ | $O(1)$ |
| **`push()` (Insert)** | $O(\log N)$ amortized | $O(1)$ | $O(N)$ |
| **`pop()` (Extract)** | $O(\log N)$ | $O(N)$ | $O(1)$ (if at end) |
| **Build Heap (`heapify`)** | $O(N)$ | $O(1)$ | $O(N \log N)$ |
| **Auxiliary Memory** | $O(N)$ (Contiguous Array) | $O(N)$ | $O(N)$ |

---

## 9. Engine Internals

### V8 Packed Arrays & Cache Pre-fetching
- **Dense Array Representation**: Because a complete binary tree has no missing indices between $0$ and $N-1$, V8 stores the heap as a `PACKED_SMI_ELEMENTS` or `PACKED_ELEMENTS` contiguous C++ vector.
- **Hardware L1 Cache Hits**: Jumping between index $i$, $2i + 1$, and $2i + 2$ keeps memory accesses within nearby hardware cache lines, significantly outperforming pointer-based BST trees in real-world benchmark execution times.

---

## 10. Best Practices & Design Guidelines

1. **Use Bounded Heaps for Top-$K$ Problems**: Maintain a heap of maximum size $K$. This bounds auxiliary memory to $O(K)$ and reduces runtime from $O(N \log N)$ to $O(N \log K)$.
2. **Use Dual Heaps for Median Tracking**: Max-Heap for lower 50% and Min-Heap for upper 50% delivers instant $O(1)$ median calculations.
3. **Prefer In-Place Array Swaps**: Use `[arr[i], arr[j]] = [arr[j], arr[i]]` or simple temporary variables for fast swaps.
4. **Use Bottom-Up Heapify for Bulk Data**: When converting a collection of $N$ items into a heap, heapify from index $\lfloor N/2 \rfloor - 1$ down to 0 in linear $O(N)$ time.

---

## 11. Real-World Case Study

### High-Throughput Job Queue Priority Scheduler
**Scenario**: A video transcoding service handled thousands of concurrent encoding requests with distinct priority tiers (Enterprise VIP: 1, Paid: 2, Free: 3) and timestamp deadlines.
**Problem**: An earlier implementation stored jobs in a Redis array and ran `jobs.sort()` whenever worker threads requested the next job, burning 85% of Node.js CPU on sorting.

**Solution**:
- Replaced the array sort with an in-memory `PriorityQueue` with a multi-factor comparator:
  ```javascript
  const jobComparator = (a, b) => {
    if (a.tier !== b.tier) return a.tier - b.tier; // Lower tier number = higher priority
    return a.submittedAt - b.submittedAt;          // Earlier timestamp = higher priority
  };
  ```
- Workers extracted the next job in $O(\log N)$ time ($<0.01\text{ms}$).

**Outcome**:
- Job dispatch latency dropped from $35\text{ms}$ down to $0.005\text{ms}$.
- CPU utilization across scheduler nodes dropped from 85% to 4%.

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between a Binary Heap and a Binary Search Tree (BST)?</summary>

- **Ordering Invariant**: In a BST, the left child is smaller and the right child is greater than the parent. In a Min-Heap, both children are greater than or equal to the parent, with no defined ordering between left and right siblings.
- **Underlying Representation**: A Binary Heap is a complete binary tree stored compactly in a contiguous flat array with zero pointer overhead. A BST is typically a pointer-linked node structure.
- **Use Case**: A BST supports $O(\log N)$ search for arbitrary elements; a Heap supports instant $O(1)$ access to the global minimum or maximum element.
</details>

<details>
<summary>2. Why does building a heap from an array take $O(N)$ time instead of $O(N \log N)$?</summary>

Inserting $N$ items one by one takes $O(N \log N)$. However, the bottom-up `heapify` algorithm starts at the bottom non-leaf nodes and sifts down.
- Half of the nodes ($\frac{N}{2}$) are leaf nodes and sift down 0 levels ($0$ operations).
- A quarter of the nodes ($\frac{N}{4}$) sift down at most 1 level.
- An eighth of the nodes ($\frac{N}{8}$) sift down at most 2 levels.
Summing the geometric series $\sum_{h=1}^{\log N} \frac{N}{2^{h+1}} \times h$ converges mathematically to $O(N)$ linear time.
</details>

<details>
<summary>3. Why is an array an ideal data structure for representing a Complete Binary Tree?</summary>

Because a Complete Binary Tree fills every level from left to right with no missing nodes, its elements map directly to consecutive array indices with zero gaps. Relationships are computed instantaneously using simple arithmetic: parent is at $\lfloor (i - 1) / 2 \rfloor$, left child is at $2i + 1$, and right child is at $2i + 2$. This eliminates memory overhead for left/right object pointers and provides superior CPU cache locality.
</details>

<details>
<summary>4. How does a Min-Heap of size $K$ find the $K$-th largest element in an array?</summary>

Iterate through the array of $N$ elements, inserting each element into a Min-Heap. Whenever the heap size exceeds $K$, pop the root (which is the smallest element currently in the heap). After processing all $N$ elements, the heap contains the $K$ largest elements of the entire array, and its root is the smallest of those $K$ elements—which is precisely the $K$-th largest element. Time complexity is $O(N \log K)$ and space is $O(K)$.
</details>

<details>
<summary>5. How does the Dual-Heap algorithm find the median of a continuous data stream in $O(1)$ time?</summary>

Maintain two heaps:
1. `maxHeap`: stores the smaller half of numbers.
2. `minHeap`: stores the larger half of numbers.
Ensure the invariant that `maxHeap.size() == minHeap.size()` (if even total count) or `maxHeap.size() == minHeap.size() + 1` (if odd count).
- When a new number arrives, insert and rebalance in $O(\log N)$ time.
- To find median: if sizes are equal, average the two roots (`(maxHeap.peek() + minHeap.peek()) / 2`); if odd, median is `maxHeap.peek()`, taking $O(1)$ time.
</details>

<details>
<summary>6. What happens if an object stored in a priority queue has its priority mutated directly in memory?</summary>

The priority queue does not detect in-memory property mutations on stored objects. The object remains at its current array index, violating the heap invariant. Subsequent `pop()` and `peek()` operations will return incorrect elements. To update an object's priority, either implement a specialized indexed heap (e.g. tracking index positions in a `Map`) or remove the object, mutate its property, and re-push it to trigger sifting.
</details>

<details>
<summary>7. What is Heap Sort, and what are its time and space complexities?</summary>

Heap Sort is a comparison-based sorting algorithm:
1. Build a Max-Heap from the input array in $O(N)$ time.
2. Repeatedly swap the root element (maximum) with the last element of the heap, reduce heap size by 1, and sift down the new root ($N$ times, each taking $O(\log N)$).
- **Time Complexity**: $O(N \log N)$ across all cases (best, average, worst).
- **Space Complexity**: $O(1)$ auxiliary memory (sorts in-place within the array).
</details>

<details>
<summary>8. Why is Binary Heap extraction $O(\log N)$ when finding the minimum is $O(1)$?</summary>

Finding the minimum is $O(1)$ because it resides at the root index 0. However, when extracting (popping) the minimum, the root slot becomes empty. To maintain the complete binary tree structure, the last element in the array is moved to the root. This new root almost certainly violates the min-heap invariant and must be sifted down by comparing and swapping with its smaller child at each level. In a tree with $N$ nodes, height is $\log_2 N$, requiring up to $\log_2 N$ swaps ($O(\log N)$).
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement an In-Place Array `heapify` (Min-Heap)
**Specification**: Implement an in-place `heapify(arr)` function that converts an arbitrary array into a valid Min-Heap in $O(N)$ time without allocating a secondary array.

<details>
<summary>Solution</summary>

```javascript
export function heapify(arr) {
  const n = arr.length;

  function siftDown(idx) {
    while (true) {
      const left = 2 * idx + 1;
      const right = 2 * idx + 2;
      let smallest = idx;

      if (left < n && arr[left] < arr[smallest]) {
        smallest = left;
      }
      if (right < n && arr[right] < arr[smallest]) {
        smallest = right;
      }

      if (smallest !== idx) {
        const temp = arr[idx];
        arr[idx] = arr[smallest];
        arr[smallest] = temp;
        idx = smallest;
      } else {
        break;
      }
    }
  }

  // Sift down starting from the last non-leaf node down to the root
  for (let i = Math.floor(n / 2) - 1; i >= 0; i--) {
    siftDown(i);
  }

  return arr;
}
```
</details>

### Exercise 2: Last Stone Weight
**Specification**: You are given an array of integers `stones` where `stones[i]` is the weight of the $i$-th stone. We play a game: on each turn, choose the heaviest two stones with weights $x$ and $y$ ($x \le y$). Smash them together: if $x == y$, both are destroyed; if $x \ne y$, the stone of weight $x$ is destroyed and the other has weight $y - x$. Return the weight of the last remaining stone, or 0 if none remain.

<details>
<summary>Solution</summary>

```javascript
import { PriorityQueue } from './PriorityQueue.js';

export function lastStoneWeight(stones) {
  // Max-Heap: comparator b - a
  const maxHeap = new PriorityQueue((a, b) => b - a);

  for (const s of stones) {
    maxHeap.push(s);
  }

  while (maxHeap.size() > 1) {
    const y = maxHeap.pop(); // Heaviest
    const x = maxHeap.pop(); // Second heaviest

    if (x !== y) {
      maxHeap.push(y - x);
    }
  }

  return maxHeap.isEmpty() ? 0 : maxHeap.peek();
}
```
</details>
