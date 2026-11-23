# Algorithmic Problem-Solving Patterns in JavaScript

> A production-grade masterclass on the 14 foundational algorithmic patterns in JavaScript, featuring decision matrices, time-space bounds, pattern classification, and interview problem mapping.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced / Masterclass |
| **Prerequisites** | All prior DSA notes ([01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md) through [17_disjoint-set-union.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/17_disjoint-set-union.md)) |
| **Standard Baseline** | ECMAScript 2024 / Node.js 20+ LTS / V8 Engine |
| **Status** | Production Ready |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: The 14 Patterns](#2-core-concepts-the-14-patterns)
3. [How It Works Internally: The Pattern Decision Flowchart](#3-how-it-works-internally-the-pattern-decision-flowchart)
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

Software engineering problems—whether encountered in algorithmic coding interviews, systems architecture, or high-throughput data processing—often appear unique on the surface. However, over 90% of algorithmic challenges reduce directly to one of **14 fundamental patterns**.

Memorizing specific code solutions to individual problems is brittle. By internalizing the **underlying invariant and trigger conditions** of each pattern, an engineer can recognize problem types instantly, construct optimal data structures, and execute correct solutions in minimal time with zero trial-and-error guessing.

---

## 2. Core Concepts: The 14 Patterns

| # | Pattern Name | Key Trigger Indicators in Problem Statement | Primary Complexity |
| :- | :--- | :--- | :--- |
| **1** | **Two Pointers** | Sorted array/string; find pairs, triplets, or reverse elements in-place. | $O(N)$ Time, $O(1)$ Space |
| **2** | **Sliding Window** | Contiguous subarray/substring; find longest, shortest, or target sum $K$. | $O(N)$ Time, $O(1)$ Space |
| **3** | **Fast & Slow Pointers** | Linked list or cyclic array; detect cycles or find middle/entry point. | $O(N)$ Time, $O(1)$ Space |
| **4** | **Merge Intervals** | Overlapping time windows, schedules, calendar conflicts. | $O(N \log N)$ Time |
| **5** | **Cyclic Sort** | Numbers bounded in range $[1 \dots N]$ or $[0 \dots N]$; find missing/duplicate. | $O(N)$ Time, $O(1)$ Space |
| **6** | **In-Place Linked List Reversal** | Reverse sub-segments of a linked list without auxiliary memory. | $O(N)$ Time, $O(1)$ Space |
| **7** | **Tree BFS (Level-Order)** | Level-by-level traversal, minimum depth, zigzag, connect level pointers. | $O(N)$ Time, $O(W)$ Space |
| **8** | **Tree DFS** | Root-to-leaf paths, maximum depth, LCA, path sum equals target. | $O(N)$ Time, $O(H)$ Space |
| **9** | **Two Heaps (Dual Heap)** | Continuous median tracking, stream partition into lower/upper halves. | $O(\log N)$ Insert, $O(1)$ Read |
| **10**| **Subsets / Backtracking** | Combinations, permutations, power set, constraint satisfaction. | $O(2^N)$ or $O(N!)$ Time |
| **11**| **Modified Binary Search** | Sorted or rotated array; find first/last bounds, search in unknown size. | $O(\log N)$ Time, $O(1)$ Space |
| **12**| **Top 'K' Elements** | Find $K$ largest, smallest, or most frequent items in an array. | $O(N \log K)$ Time, $O(K)$ Space |
| **13**| **K-Way Merge** | Merge $K$ sorted arrays, lists, or streams into a single sorted stream. | $O(N \log K)$ Time |
| **14**| **Dynamic Programming** | Overlapping subproblems, optimal substructure, minimax, counting paths. | $O(N)$ or $O(N \times W)$ Time |

---

## 3. How It Works Internally: The Pattern Decision Flowchart

```
                            [ Problem Statement ]
                                     │
                     Is input a Graph or Tree?
                     ├── YES ──► Level-by-level? ──► Tree BFS
                     │           All paths / LCA? ──► Tree DFS
                     │           Topological/Cycle? ──► Kahn's / Dijkstra
                     └── NO
                                     │
                     Is input an Array or String?
                     ├── Are elements sorted?
                     │   ├── YES ──► Looking for pair/triplet? ──► Two Pointers
                     │   │           Search target in O(log N)? ──► Binary Search
                     │   └── NO  ──► Numbers bounded [1..N]? ──► Cyclic Sort
                     │               Contiguous subsegment? ──► Sliding Window
                     │               Top / Extreme K items? ──► Top-K Heap
                     │               Find all combinations? ──► Backtracking
                     └── Overlapping subproblems / Min / Max? ──► Dynamic Programming
```

---

## 4. Syntax & API Reference

### Signature Pattern Templates
```javascript
// Pattern 4: Merge Intervals Template
export function mergeIntervals(intervals) {
  if (intervals.length <= 1) return intervals;
  intervals.sort((a, b) => a[0] - b[0]);

  const merged = [intervals[0]];
  for (let i = 1; i < intervals.length; i++) {
    const curr = intervals[i];
    const prev = merged[merged.length - 1];

    if (curr[0] <= prev[1]) {
      prev[1] = Math.max(prev[1], curr[1]);
    } else {
      merged.push(curr);
    }
  }
  return merged;
}

// Pattern 5: Cyclic Sort Template (O(N) Time, O(1) Space)
export function cyclicSort(nums) {
  let i = 0;
  while (i < nums.length) {
    const correctIdx = nums[i] - 1; // Assuming range [1..N]
    if (nums[i] !== nums[correctIdx]) {
      // Swap into its correct position
      const temp = nums[i];
      nums[i] = nums[correctIdx];
      nums[correctIdx] = temp;
    } else {
      i++;
    }
  }
  return nums;
}
```

---

## 5. Practical Examples

### Example 1: Cyclic Sort (Find Missing Number in Array $[0 \dots N]$)
```javascript
/**
 * Places each number at its corresponding index in O(N) time and O(1) space.
 */
export function findMissingNumberCyclic(nums) {
  let i = 0;
  const n = nums.length;

  while (i < n) {
    const correctIdx = nums[i];
    if (nums[i] < n && nums[i] !== nums[correctIdx]) {
      // Swap to correct index
      const temp = nums[i];
      nums[i] = nums[correctIdx];
      nums[correctIdx] = temp;
    } else {
      i++;
    }
  }

  // Find index that does not match its value
  for (let idx = 0; idx < n; idx++) {
    if (nums[idx] !== idx) {
      return idx;
    }
  }

  return n;
}

// console.log(findMissingNumberCyclic([3, 0, 1])); // 2
```

### Example 2: Insert Interval (Merge Intervals Pattern)
```javascript
export function insertInterval(intervals, newInterval) {
  const result = [];
  let i = 0;
  const n = intervals.length;

  // 1. Add all intervals ending before newInterval starts
  while (i < n && intervals[i][1] < newInterval[0]) {
    result.push(intervals[i]);
    i++;
  }

  // 2. Merge overlapping intervals with newInterval
  while (i < n && intervals[i][0] <= newInterval[1]) {
    newInterval[0] = Math.min(newInterval[0], intervals[i][0]);
    newInterval[1] = Math.max(newInterval[1], intervals[i][1]);
    i++;
  }
  result.push(newInterval);

  // 3. Add all remaining intervals starting after newInterval ends
  while (i < n) {
    result.push(intervals[i]);
    i++;
  }

  return result;
}
```

### Example 3: $K$ Closest Points to Origin (Top-$K$ Pattern)
```javascript
import { PriorityQueue } from './08_heaps-and-priority-queues.js';

export function kClosest(points, k) {
  // Max-Heap: store points ordered by distance descending
  const distanceSq = ([x, y]) => x * x + y * y;
  const maxHeap = new PriorityQueue((a, b) => distanceSq(b) - distanceSq(a));

  for (const point of points) {
    maxHeap.push(point);
    if (maxHeap.size() > k) {
      maxHeap.pop(); // Evict farthest point
    }
  }

  const result = [];
  while (!maxHeap.isEmpty()) {
    result.push(maxHeap.pop());
  }

  return result;
}
```

### Example 4: Merge $K$ Sorted Arrays ($K$-Way Merge Pattern)
```javascript
import { PriorityQueue } from './08_heaps-and-priority-queues.js';

export function mergeKSortedArrays(arrays) {
  // Min-Heap stores [val, arrayIndex, elementIndex]
  const minHeap = new PriorityQueue((a, b) => a[0] - b[0]);

  // Insert first element from each non-empty array
  for (let i = 0; i < arrays.length; i++) {
    if (arrays[i].length > 0) {
      minHeap.push([arrays[i][0], i, 0]);
    }
  }

  const result = [];

  while (!minHeap.isEmpty()) {
    const [val, arrIdx, elemIdx] = minHeap.pop();
    result.push(val);

    // If more elements exist in the same array, push the next element
    if (elemIdx + 1 < arrays[arrIdx].length) {
      minHeap.push([arrays[arrIdx][elemIdx + 1], arrIdx, elemIdx + 1]);
    }
  }

  return result;
}
```

### Example 5: In-Place Reverse Linked List Subsegment (Pattern 6)
```javascript
import { ListNode } from './04_linked-lists.js';

export function reverseBetween(head, left, right) {
  if (!head || left === right) return head;

  const dummy = new ListNode(0, head);
  let prev = dummy;

  // Advance prev to node immediately preceding 'left'
  for (let i = 0; i < left - 1; i++) {
    prev = prev.next;
  }

  const curr = prev.next;

  // Re-link nodes in-place in a single pass
  for (let i = 0; i < right - left; i++) {
    const nextTemp = curr.next;
    curr.next = nextTemp.next;
    nextTemp.next = prev.next;
    prev.next = nextTemp;
  }

  return dummy.next;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Sliding Window on Arrays with Negative Numbers**:
   Standard sliding window assumes that expanding the right boundary increases the sum and shrinking the left boundary decreases the sum (monotonic property). If an array contains **negative numbers**, this invariant breaks completely. Use **Prefix Sum + Hash Map** (Pattern 14) instead of Sliding Window for arrays with negative numbers.
2. **Infinite Loops in Cyclic Sort**:
   If an array contains duplicate elements (e.g. `[2, 2]`), a naive swap condition `nums[i] !== i + 1` causes an infinite loop swapping `2` with `2`. Always check `nums[i] !== nums[correctIdx]`.
3. **Overriding Bounds in Binary Search Answer Space**:
   Setting an improper lower or upper bound in binary search on answer truncates potential solutions. Ensure $X_{\text{low}}$ and $X_{\text{high}}$ cover all edge cases (e.g. `low = 1`, `high = Math.max(...piles)`).

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Hazard | Production Fix |
| :--- | :--- | :--- |
| Applying Sliding Window to arrays with negative numbers | Window contraction logic breaks; fails silently. | Use Prefix Sum + Hash Map for non-monotonic sum arrays. |
| Sorting whole array for Top-$K$ items | Wastes CPU sorting $N$ items when only $K$ are needed. | Use Min-Heap of size $K$ ($O(N \log K)$) or QuickSelect ($O(N)$). |
| Swapping duplicates indefinitely in Cyclic Sort | Causes infinite loop freezing the runtime thread. | Check `nums[i] !== nums[correctIdx]` before swapping. |
| Using recursion where stack depth $>10,000$ | Crashes with Call Stack Overflow in V8. | Convert to iterative implementation with explicit heap stack. |
| Sorting intervals by start time for activity selection | Picks overly long intervals that block multiple short ones. | Sort by **end time** for interval selection; sort by **start time** for interval merging. |
| Mutating shared arrays in Backtracking without copying | Results array ends up containing empty sub-arrays. | Always snapshot copy: `results.push([...current])`. |

---

## 8. Performance Characteristics

- **Comparative Time-Space Matrix**:
  - Two Pointers: $O(N)$ time, $O(1)$ space.
  - Sliding Window: $O(N)$ time, $O(1)$ or $O(K)$ space.
  - Top-$K$ Heap: $O(N \log K)$ time, $O(K)$ space.
  - Dynamic Programming: $O(N)$ to $O(N \times W)$ time, $O(1)$ to $O(W)$ compressed space.
  - Backtracking: $O(2^N)$ or $O(N!)$ time, $O(N)$ auxiliary stack space.

---

## 9. Engine Internals

### V8 Hidden Class & Pointer Optimization across Patterns
- **Memory Reuse in Sliding Windows**: Sliding window algorithms maintain scalar pointers (`left`, `right`) and numeric counters without allocating new objects, avoiding V8 garbage collector invocation entirely.
- **Cache Locality in In-Place Swapping**: Cyclic sort and two-pointer in-place mutations operate entirely within contiguous V8 `PACKED_ELEMENTS` buffers, maximizing CPU L1 cache line hits.

---

## 10. Best Practices & Design Guidelines

1. **Classify Before Coding**: Spend the first 2 minutes identifying the core pattern rather than writing ad-hoc loops.
2. **Analyze Constraints to Validate Complexity**:
   - $N \le 20 \implies O(2^N)$ Backtracking is acceptable.
   - $N \le 100,000 \implies$ Must be $O(N)$ or $O(N \log N)$.
   - $N \le 1,000,000,000 \implies$ Must be $O(\log N)$ Binary Search or $O(1)$ Math.
3. **Prefer In-Place Mutating Patterns when Space is Constrained**: Use Cyclic Sort, Two Pointers, and In-Place Linked List Reversal.

---

## 11. Real-World Case Study

### High-Throughput Search Query Planner
**Scenario**: An enterprise search engine processed millions of complex analytical filter queries combining ranges, text keywords, and categorical tags.
**Architecture**:
- The query optimizer analyzes user filter constraints and applies algorithmic patterns:
  - Timestamp range overlaps $\implies$ **Merge Intervals** ($O(N \log N)$).
  - Top 10 trending queries in live stream $\implies$ **Top-$K$ Min-Heap** ($O(N \log 10) \approx O(N)$).
  - Autocomplete suggestion matching $\implies$ **Trie Prefix Matching** ($O(L)$).
  - Duplicate record deduplication $\implies$ **Hash Set** ($O(N)$).

**Outcome**:
- Standardizing query execution on verified patterns reduced P99 latency across all query categories to sub-millisecond execution.

---

## 12. Interview Questions

<details>
<summary>1. How do you distinguish between when to use Sliding Window vs Two Pointers?</summary>

- **Two Pointers**: Used when the array is sorted, and you are comparing two distinct elements at independent positions (e.g. opposite ends moving inward for Two-Sum, or fast/slow pointers for in-place compaction).
- **Sliding Window**: Used when you are tracking a **contiguous subsegment (subarray or substring)**, where elements between the two pointers collectively satisfy a condition (e.g. sum $\le K$, or at most $K$ distinct characters).
</details>

<details>
<summary>2. Why does Cyclic Sort execute in $O(N)$ time despite containing a while loop with swaps?</summary>

In Cyclic Sort, on every swap, at least one element is placed into its permanent, final sorted index. Because an array of size $N$ has only $N$ elements, an element can be placed into its correct position at most once. Therefore, the swap operation executes at most $N - 1$ times across the entire algorithm execution. Since the loop counter increments at most $N$ times, total operations are bounded by $2N$, guaranteeing strict linear **$O(N)$** time complexity.
</details>

<details>
<summary>3. When does the Sliding Window pattern fail for finding subarrays with sum equal to $K$?</summary>

Sliding Window fails when the array contains **negative numbers**. The sliding window contraction rule relies on the assumption that adding elements increases the window sum and removing elements decreases the window sum (monotonic behavior). If negative numbers exist, shrinking the window can actually increase the sum, breaking the invariant. For arrays with negative numbers, use the **Prefix Sum + Hash Map** pattern instead ($O(N)$ time and space).
</details>

<details>
<summary>4. What pattern should you reach for when asked to merge $K$ sorted streams?</summary>

The **$K$-Way Merge pattern** using a **Min-Heap (Priority Queue)**.
Insert the first element of each of the $K$ sorted streams into a Min-Heap ($O(K)$). Extract the minimum root element, append it to the result, and insert the next element from that specific stream into the heap ($O(\log K)$). Repeat until all streams are exhausted. Total time complexity is **$O(N \log K)$**, where $N$ is the total count of elements across all streams.
</details>

<details>
<summary>5. How can you find a cycle in an array where values are pointers to indices?</summary>

Using the **Fast & Slow Pointers (Tortoise and Hare)** pattern.
Treat each index as a node and `nums[i]` as the `next` pointer. Initialize `slow = nums[0]` and `fast = nums[nums[0]]`. Advance `slow = nums[slow]` (one step) and `fast = nums[nums[fast]]` (two steps). If a cycle exists, `fast` and `slow` are guaranteed to meet inside the cycle in $O(N)$ time and $O(1)$ space (e.g. LeetCode 287 - Find the Duplicate Number).
</details>

<details>
<summary>6. How do input constraints determine which pattern to select?</summary>

- If $N \le 20 \implies$ Exponential $O(2^N)$ or $O(N!)$ (Backtracking, Subsets).
- If $N \le 1,000 \implies$ Quadratic $O(N^2)$ (2D Dynamic Programming, Matrix Traversal).
- If $N \le 100,000 \implies$ Linearithmic $O(N \log N)$ (Sorting, Heaps, Divide & Conquer) or Linear $O(N)$ (Two Pointers, Sliding Window, Monotonic Stack, DSU).
- If $N \ge 1,000,000 \implies$ Logarithmic $O(\log N)$ (Binary Search) or $O(1)$ (Math, Bitwise).
</details>

<details>
<summary>7. What is the difference between sorting intervals by Start Time vs End Time?</summary>

- **Sort by Start Time**: Used for **Interval Merging** (combining overlapping intervals) or inserting a new interval, because you need to evaluate intervals in chronological arrival order.
- **Sort by End Time**: Used for **Interval Scheduling / Selection** (finding the maximum number of non-overlapping intervals), because picking the interval that finishes earliest maximizes remaining time for future intervals.
</details>

<details>
<summary>8. How do you implement In-Place Linked List Reversal in a single pass without allocating extra nodes?</summary>

Maintain three pointers: `prev`, `curr`, and `next`.
Iterate through the desired subsegment:
1. `next = curr.next;`
2. `curr.next = next.next;`
3. `next.next = prev.next;`
4. `prev.next = next;`
By repeatedly moving the next node to the front of the reversed sub-segment, the reversal completes in $O(N)$ time and $O(1)$ memory without allocating secondary nodes.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Find All Numbers Disappeared in an Array
**Specification**: Given an array `nums` of $n$ integers where `nums[i]` is in the range `[1, n]`, return an array of all the integers in the range `[1, n]` that do not appear in `nums`. Must run in $O(N)$ time and $O(1)$ auxiliary space using Cyclic Sort.

<details>
<summary>Solution</summary>

```javascript
export function findDisappearedNumbers(nums) {
  let i = 0;
  const n = nums.length;

  while (i < n) {
    const correctIdx = nums[i] - 1;
    if (nums[i] !== nums[correctIdx]) {
      const temp = nums[i];
      nums[i] = nums[correctIdx];
      nums[correctIdx] = temp;
    } else {
      i++;
    }
  }

  const missing = [];
  for (let idx = 0; idx < n; idx++) {
    if (nums[idx] !== idx + 1) {
      missing.push(idx + 1);
    }
  }

  return missing;
}
```
</details>

### Exercise 2: Meeting Rooms II (Minimum Meeting Rooms Required)
**Specification**: Given an array of meeting time intervals consisting of start and end times `[[s1, e1], [s2, e2], ...]`, find the minimum number of conference rooms required.

<details>
<summary>Solution</summary>

```javascript
export function minMeetingRooms(intervals) {
  if (intervals.length === 0) return 0;

  const starts = intervals.map((i) => i[0]).sort((a, b) => a - b);
  const ends = intervals.map((i) => i[1]).sort((a, b) => a - b);

  let rooms = 0;
  let endPtr = 0;

  for (let startPtr = 0; startPtr < starts.length; startPtr++) {
    // If meeting starts before earliest meeting ends, need a new room
    if (starts[startPtr] < ends[endPtr]) {
      rooms++;
    } else {
      // Room freed up
      endPtr++;
    }
  }

  return rooms;
}
```
</details>
