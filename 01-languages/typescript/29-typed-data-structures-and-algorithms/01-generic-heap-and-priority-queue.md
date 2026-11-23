# Generic Heap and Priority Queue in TypeScript

> Implementation of generic MinHeap, MaxHeap, and PriorityQueue in TypeScript: array-backed complete binary trees, sift-up/sift-down operations, custom comparators, and streaming top-K algorithms.

| Metadata | Details |
| :--- | :--- |
| **Category** | Data Structures & Algorithms |
| **Difficulty** | Advanced |
| **Prerequisites** | [29-typed-data-structures-and-algorithms/00-typed-linked-lists-and-trees.md](./00-typed-linked-lists-and-trees.md), [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In task scheduling, graph routing (Dijkstra, A*), and real-time streaming, applications constantly need to extract the element with the highest or lowest priority.
- Sorting a standard array on every insert takes $O(N \log N)$ or $O(N)$ with binary insertion.
- Linearly scanning an unsorted array takes $O(N)$ for every extraction.

A **Binary Heap** is a complete binary tree satisfying the heap property:
- **MinHeap**: Parent key $\le$ children keys. Root is always the minimum.
- **MaxHeap**: Parent key $\ge$ children keys. Root is always the maximum.

A Heap guarantees:
- $O(1)$ peek at the top priority item.
- $O(\log N)$ insertion (`push`).
- $O(\log N)$ extraction (`pop`).

TypeScript allows us to implement a single, generic `Heap<T>` parameterized by a type-safe `Comparator<T>`, enabling reuse across task queues, event loops, and numeric aggregators.

---

## 2. Core Concepts

### Array-Backed Complete Binary Tree
Heaps are implemented without node pointer objects. Because the tree is always structurally complete, nodes are packed directly into a contiguous flat array. For an element at index $i$:
- **Parent**: $\lfloor (i - 1) / 2 \rfloor$ (Bitwise: `(i - 1) >> 1`)
- **Left Child**: $2i + 1$ (Bitwise: `(i << 1) + 1`)
- **Right Child**: $2i + 2$ (Bitwise: `(i << 1) + 2`)

```
+-------------------------------------------------------------------------+
|                  Array-Backed Binary Heap Layout                        |
+-------------------------------------------------------------------------+
|                                                                         |
|  Logical Tree:                     Array Representation:                |
|           [ 10 (i=0) ]             Index: [  0 |  1 |  2 |  3 |  4 ]    |
|          /            \            Value: [ 10 | 20 | 15 | 30 | 25 ]    |
|    [ 20 (i=1) ]   [ 15 (i=2) ]                                          |
|    /          \                                                         |
| [ 30 (i=3) ]  [ 25 (i=4) ]                                              |
|                                                                         |
|  Left child of index 1 = 2(1) + 1 = 3 (Value: 30)                       |
|  Parent of index 4     = (4 - 1) / 2 = 1 (Value: 20)                    |
|                                                                         |
+-------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **Sift-Up (Bubble-Up)**: When a new element is inserted at the end of the array, it is compared with its parent. If it violates the heap property, it swaps with the parent, repeating up the tree until balanced ($O(\log N)$).
2. **Sift-Down (Sink-Down)**: When the root element is extracted, the last element in the array moves to index 0. It is compared with its smallest (or largest) child and swapped downwards until the heap property is restored ($O(\log N)$).

---

## 4. Syntax & API Reference

### Generic Heap Interface
```typescript
export type Comparator<T> = (a: T, b: T) => number;

export interface IHeap<T> {
  readonly size: number;
  isEmpty(): boolean;
  peek(): T | undefined;
  push(item: T): void;
  pop(): T | undefined;
}
```

---

## 5. Practical Examples

### Example 1: Generic Heap Implementation
```typescript
import { Comparator, IHeap } from "./heap-contract.js";

export class Heap<T> implements IHeap<T> {
  private data: T[] = [];

  constructor(private compare: Comparator<T>) {}

  public get size(): number {
    return this.data.length;
  }

  public isEmpty(): boolean {
    return this.data.length === 0;
  }

  public peek(): T | undefined {
    return this.data[0];
  }

  public push(item: T): void {
    this.data.push(item);
    this.siftUp(this.data.length - 1);
  }

  public pop(): T | undefined {
    if (this.isEmpty()) return undefined;
    const top = this.data[0];
    const bottom = this.data.pop()!;
    if (this.data.length > 0) {
      this.data[0] = bottom;
      this.siftDown(0);
    }
    return top;
  }

  private siftUp(index: number): void {
    let curr = index;
    while (curr > 0) {
      const parent = (curr - 1) >> 1;
      if (this.compare(this.data[curr]!, this.data[parent]!) < 0) {
        this.swap(curr, parent);
        curr = parent;
      } else {
        break;
      }
    }
  }

  private siftDown(index: number): void {
    let curr = index;
    const len = this.data.length;

    while ((curr << 1) + 1 < len) {
      let candidate = (curr << 1) + 1;
      const right = candidate + 1;

      if (right < len && this.compare(this.data[right]!, this.data[candidate]!) < 0) {
        candidate = right;
      }

      if (this.compare(this.data[candidate]!, this.data[curr]!) < 0) {
        this.swap(curr, candidate);
        curr = candidate;
      } else {
        break;
      }
    }
  }

  private swap(i: number, j: number): void {
    const temp = this.data[i]!;
    this.data[i] = this.data[j]!;
    this.data[j] = temp;
  }
}
```

### Example 2: Priority Queue for Background Jobs
```typescript
import { Heap } from "./heap.js";

export interface PriorityTask<TData> {
  id: string;
  priority: number; // Lower number = higher priority
  payload: TData;
}

export class PriorityQueue<T> {
  private heap = new Heap<PriorityTask<T>>((a, b) => a.priority - b.priority);

  public enqueue(id: string, priority: number, payload: T): void {
    this.heap.push({ id, priority, payload });
  }

  public dequeue(): PriorityTask<T> | undefined {
    return this.heap.pop();
  }

  public get size(): number {
    return this.heap.size;
  }
}
```

### Example 3: Edge Case - Streaming Top-K Elements
```typescript
// Finding the K largest elements in a stream of N numbers in O(N log K) time
export function findTopK(stream: number[], k: number): number[] {
  // Use MinHeap of size K: the root will hold the Kth largest element
  const minHeap = new Heap<number>((a, b) => a - b);

  for (const num of stream) {
    minHeap.push(num);
    if (minHeap.size > k) {
      minHeap.pop(); // Evicts smaller items
    }
  }

  const result: number[] = [];
  while (!minHeap.isEmpty()) {
    result.push(minHeap.pop()!);
  }
  return result.reverse();
}
```

### Example 4: Compiler Error & Type Safety with PriorityQueue
```typescript
const queue = new PriorityQueue<{ email: string }>();

// @ts-expect-error - TS2345: Argument of type 'string' is not assignable to parameter of type '{ email: string; }'.
queue.enqueue("task_1", 10, "invalid_payload");
```

### Example 5: High-Performance In-Place Heapify
```typescript
// Transforms an arbitrary array into a heap in O(N) time
export function heapify<T>(array: T[], compare: Comparator<T>): void {
  const len = array.length;
  const start = (len >> 1) - 1;

  for (let i = start; i >= 0; i--) {
    let curr = i;
    while ((curr << 1) + 1 < len) {
      let candidate = (curr << 1) + 1;
      const right = candidate + 1;

      if (right < len && compare(array[right]!, array[candidate]!) < 0) {
        candidate = right;
      }

      if (compare(array[candidate]!, array[curr]!) < 0) {
        const temp = array[curr]!;
        array[curr] = array[candidate]!;
        array[candidate] = temp;
        curr = candidate;
      } else {
        break;
      }
    }
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Pop on Empty Heap**:
   Always handle `undefined` when popping or peeking from an empty heap. Under `"noUncheckedIndexedAccess": true`, `data[0]` is typed as `T | undefined`.
2. **Non-Integer Index Divisions**:
   In JavaScript, `(i - 1) / 2` produces floating-point fractions (`1.5`). Always use Math.floor or bitwise shift `(i - 1) >> 1`.
3. **Comparator Stability**:
   Standard binary heaps are not stable: elements with identical priority are not guaranteed to emerge in FIFO arrival order. To achieve stability, attach a secondary sequence counter to items.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Floating Point Index Math** | `(i - 1) / 2` | `(i - 1) >> 1` | Fractional indices cause lookup failures |
| **Building Heap by Repeated Push** | Inserting $N$ items via `push()` ($O(N \log N)$) | Use bottom-up `heapify()` ($O(N)$) | Sub-optimal initialization speed |
| **Mutable Priority Keys** | Modifying priority while in heap | Remove and re-insert item | Breaks heap invariants |
| **Assuming FIFO Stability** | Expecting equal priority items in FIFO | Track sequence counter | Unpredictable dequeue order |
| **Loose Unchecked Indexing** | Forcing `!` on `data[0]!` | Safely handle `undefined` on empty | Runtime `TypeError` on empty peek |
| **Sorting on Every Insert** | `arr.push(x); arr.sort()` | Use binary heap `push()` | $O(N \log N)$ vs $O(\log N)$ |

---

## 8. Performance & Type-Checker Complexity

| Operation | Array (Sorted) | Array (Unsorted) | Binary Heap |
| :--- | :--- | :--- | :--- |
| **Peek** | $O(1)$ | $O(N)$ | $O(1)$ |
| **Push** | $O(N)$ | $O(1)$ | $O(\log N)$ |
| **Pop** | $O(1)$ | $O(N)$ | $O(\log N)$ |
| **Heapify (Build)** | $O(N \log N)$ | $O(1)$ | $O(N)$ |

---

## 9. Compiler Internals

When using `Heap<T>`:
1. `ts.TypeChecker` verifies that `compare: (a: T, b: T) => number` matches the array elements' type `T`.
2. The bitwise operators `>> 1` and `<< 1` operate on numeric types, which the compiler verifies as valid JavaScript bitwise expressions.

---

## 10. Best Practices & Design Guidelines

1. **Use Bitwise Shifts for Index Math**: `(i - 1) >> 1` and `(i << 1) + 1` are faster and guarantee integer truncation.
2. **Build Bulk Heaps via `heapify()`**: Runs in $O(N)$ time instead of $O(N \log N)$.
3. **Store Invariant Sequence IDs for Stable Priority Queues**.
4. **Constrain Comparator Functions**: Ensure total ordering (antisymmetric, transitive).

---

## 11. Real-World Case Study

An edge proxy needed to handle rate-limiting and job dispatching across 100,000 active concurrent WebSocket connections. A timer tick executed every 10ms to check expired timers.

**Original Architecture**:
Timers were stored in an unindexed array. Every 10ms, the worker filtered the array (`timers.filter(t => t.expires <= now)`), taking 35ms per tick and blocking the event loop completely.

**Heap Migration**:
Migrated to a `MinHeap<Timer>` keyed on `expires`:
- Only the root is inspected ($O(1)$). If `root.expires > now`, the tick terminates immediately.
- Expired timers are popped in $O(\log N)$ time.
- Tick execution dropped from **35ms** to **0.08ms**, allowing the proxy to handle 500,000 connections effortlessly.

---

## 12. Interview Questions

<details>
<summary>1. Why is building a heap via bottom-up `heapify` $O(N)$ while repeated `push` is $O(N \log N)$?</summary>

In bottom-up heapify, the majority of nodes reside near the bottom of the tree where sift-down height is small: half the nodes have height 0, a quarter have height 1, and only one node has height $\log N$. The summation $\sum_{h=0}^{\log N} \frac{N}{2^h} h$ converges mathematically to $O(N)$. Repeated push adds nodes at the bottom and bubbles up to the top, doing $O(\log N)$ work for every element.
</details>

<details>
<summary>2. How are binary heaps represented in contiguous arrays?</summary>

For any element at zero-based index $i$, its left child is at index $2i + 1$, its right child is at index $2i + 2$, and its parent is at index $\lfloor (i - 1) / 2 \rfloor$.
</details>

<details>
<summary>3. What is the difference between a MinHeap and a MaxHeap?</summary>

In a MinHeap, each parent node is less than or equal to its children, making the root the minimum element. In a MaxHeap, each parent node is greater than or equal to its children, making the root the maximum element.
</details>

<details>
<summary>4. How can you make a priority queue stable (FIFO for equal priorities)?</summary>

By attaching an auto-incrementing integer sequence counter to each inserted item. In the comparator, if priorities are equal, compare sequence counters (`a.seq - b.seq`).
</details>

<details>
<summary>5. How does a MinHeap solve the "Top-K Frequent Elements" problem efficiently?</summary>

Maintain a MinHeap of size $K$. For each element in the stream, push it into the heap; if heap size exceeds $K$, pop the minimum. At the end of the stream, the heap retains the $K$ largest elements in $O(N \log K)$ time.
</details>

<details>
<summary>6. What happens if a heap element's priority changes after insertion?</summary>

The heap property is violated. The element must either be removed and re-inserted, or a specialized `updatePriority(index)` method must call `siftUp` or `siftDown` on that specific index.
</details>

<details>
<summary>7. Why is bitwise `(i - 1) >> 1` preferred over `Math.floor((i - 1) / 2)` in JavaScript?</summary>

Bitwise shifts operate directly on 32-bit signed integers in V8 CPU registers, avoiding floating-point division and function call overhead while guaranteeing integer truncation.
</details>

<details>
<summary>8. What is the space complexity of an array-backed binary heap?</summary>

$O(N)$ with zero overhead for node pointer wrappers, because elements are stored contiguously in an array.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): MaxHeap Comparator</summary>

**Task**: Instantiate the generic `Heap` class as a MaxHeap of numbers.

**Solution**:
```typescript
import { Heap } from "./heap.js";

export const maxHeap = new Heap<number>((a, b) => b - a);
maxHeap.push(10);
maxHeap.push(50);
maxHeap.push(20);
console.log(maxHeap.pop()); // 50
```
</details>

<details>
<summary>Challenge 2 (Easy): Kth Smallest Element in an Array</summary>

**Task**: Write a function that finds the Kth smallest element in an array using a MaxHeap.

**Solution**:
```typescript
import { Heap } from "./heap.js";

export function findKthSmallest(nums: number[], k: number): number {
  const maxHeap = new Heap<number>((a, b) => b - a);
  for (const num of nums) {
    maxHeap.push(num);
    if (maxHeap.size > k) {
      maxHeap.pop();
    }
  }
  return maxHeap.peek()!;
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Merge K Sorted Lists</summary>

**Task**: Merge $K$ sorted arrays into a single sorted array using a MinHeap in $O(N \log K)$ time.

**Solution**:
```typescript
import { Heap } from "./heap.js";

interface QueueEntry {
  val: number;
  arrayIndex: number;
  elementIndex: number;
}

export function mergeKSorted(arrays: number[][]): number[] {
  const minHeap = new Heap<QueueEntry>((a, b) => a.val - b.val);
  const result: number[] = [];

  for (let i = 0; i < arrays.length; i++) {
    if (arrays[i] && arrays[i]!.length > 0) {
      minHeap.push({ val: arrays[i]![0]!, arrayIndex: i, elementIndex: 0 });
    }
  }

  while (!minHeap.isEmpty()) {
    const top = minHeap.pop()!;
    result.push(top.val);

    const nextIndex = top.elementIndex + 1;
    const sourceArr = arrays[top.arrayIndex]!;
    if (nextIndex < sourceArr.length) {
      minHeap.push({ val: sourceArr[nextIndex]!, arrayIndex: top.arrayIndex, elementIndex: nextIndex });
    }
  }

  return result;
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Stable Priority Queue with Secondary Sequence</summary>

**Task**: Implement a `StablePriorityQueue` that guarantees FIFO extraction order for identical priorities.

**Solution**:
```typescript
import { Heap } from "./heap.js";

interface StableEntry<T> {
  item: T;
  priority: number;
  sequence: number;
}

export class StablePriorityQueue<T> {
  private seq = 0;
  private heap = new Heap<StableEntry<T>>((a, b) => {
    if (a.priority !== b.priority) return a.priority - b.priority;
    return a.sequence - b.sequence; // Secondary FIFO tie-breaker
  });

  public enqueue(item: T, priority: number): void {
    this.heap.push({ item, priority, sequence: this.seq++ });
  }

  public dequeue(): T | undefined {
    return this.heap.pop()?.item;
  }
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Median of Data Stream (Two Heaps)</summary>

**Task**: Design a class `MedianFinder` that supports `addNum(num)` in $O(\log N)$ and `findMedian()` in $O(1)$ using two heaps.

**Solution**:
```typescript
import { Heap } from "./heap.js";

export class MedianFinder {
  private maxHeap = new Heap<number>((a, b) => b - a); // Lower half
  private minHeap = new Heap<number>((a, b) => a - b); // Upper half

  public addNum(num: number): void {
    if (this.maxHeap.isEmpty() || num <= this.maxHeap.peek()!) {
      this.maxHeap.push(num);
    } else {
      this.minHeap.push(num);
    }

    // Rebalance heaps so size difference <= 1
    if (this.maxHeap.size > this.minHeap.size + 1) {
      this.minHeap.push(this.maxHeap.pop()!);
    } else if (this.minHeap.size > this.maxHeap.size) {
      this.maxHeap.push(this.minHeap.pop()!);
    }
  }

  public findMedian(): number {
    if (this.maxHeap.size > this.minHeap.size) {
      return this.maxHeap.peek()!;
    }
    return (this.maxHeap.peek()! + this.minHeap.peek()!) / 2;
  }
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Generic Indexed Priority Queue with In-Place Key Updates</summary>

**Task**: Implement an `IndexedPriorityQueue` supporting `updateKey(id, newPriority)` in $O(\log N)$ by tracking element indices with a `Map`.

**Solution**:
```typescript
export class IndexedPriorityQueue<TKey, TValue> {
  private keys: TKey[] = [];
  private values: TValue[] = [];
  private indexMap = new Map<TKey, number>();

  constructor(private compare: (a: TValue, b: TValue) => number) {}

  public insert(key: TKey, value: TValue): void {
    if (this.indexMap.has(key)) throw new Error("Key already exists");
    const idx = this.keys.length;
    this.keys.push(key);
    this.values.push(value);
    this.indexMap.set(key, idx);
    this.bubbleUp(idx);
  }

  public update(key: TKey, newValue: TValue): void {
    const idx = this.indexMap.get(key);
    if (idx === undefined) throw new Error("Key not found");
    this.values[idx] = newValue;
    this.bubbleUp(idx);
    this.sinkDown(idx);
  }

  private bubbleUp(idx: number): void {
    let curr = idx;
    while (curr > 0) {
      const parent = (curr - 1) >> 1;
      if (this.compare(this.values[curr]!, this.values[parent]!) < 0) {
        this.swap(curr, parent);
        curr = parent;
      } else break;
    }
  }

  private sinkDown(idx: number): void {
    let curr = idx;
    const len = this.values.length;
    while ((curr << 1) + 1 < len) {
      let cand = (curr << 1) + 1;
      const r = cand + 1;
      if (r < len && this.compare(this.values[r]!, this.values[cand]!) < 0) cand = r;
      if (this.compare(this.values[cand]!, this.values[curr]!) < 0) {
        this.swap(curr, cand);
        curr = cand;
      } else break;
    }
  }

  private swap(i: number, j: number): void {
    const k1 = this.keys[i]!;
    const k2 = this.keys[j]!;
    this.indexMap.set(k1, j);
    this.indexMap.set(k2, i);

    const tempK = this.keys[i]!;
    this.keys[i] = this.keys[j]!;
    this.keys[j] = tempK;

    const tempV = this.values[i]!;
    this.values[i] = this.values[j]!;
    this.values[j] = tempV;
  }
}
```
</details>
