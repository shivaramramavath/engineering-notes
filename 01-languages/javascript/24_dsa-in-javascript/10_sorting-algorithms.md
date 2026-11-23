# Sorting Algorithms in JavaScript

> An authoritative engineering guide to Sorting Algorithms in JavaScript, covering Merge Sort, Quick Sort (3-way Dutch National Flag), Non-Comparison Counting Sort, and V8's native TimSort engine.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [02_arrays-and-strings.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/02_arrays-and-strings.md) |
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

Sorting—arranging elements in a specific order (ascending or descending)—is one of the most fundamental operations in computer science. Unordered data requires $O(N)$ linear scans for search and aggregation; sorted data unlocks **$O(\log N)$ binary search**, instant range queries, deduplication, and efficient database indexing.

In JavaScript, understanding sorting algorithms is crucial because:
- The default behavior of `Array.prototype.sort()` converts elements to strings before sorting, leading to catastrophic bugs (`[10, 2, 5].sort()` produces `[10, 2, 5]` because `'10' < '2'`).
- Different algorithms optimize for different constraints: stability, auxiliary space, cache locality, or specialized integer domains ($O(N)$ non-comparison sorts).

---

## 2. Core Concepts

### Sorting Classification Dimensions
1. **Stability**: A sorting algorithm is **stable** if elements with identical keys retain their original relative order after sorting. Essential when sorting by multiple columns (e.g. sort by name, then by age).
2. **In-Place**: Requires $O(1)$ auxiliary memory (e.g. QuickSort, HeapSort). Non in-place sorts require $O(N)$ extra memory (e.g. MergeSort).
3. **Comparison vs. Non-Comparison**:
   - Comparison sorts cannot beat the theoretical lower bound of **$\Omega(N \log N)$**.
   - Non-comparison sorts (Counting Sort, Radix Sort) exploit integer bit representations to sort in **$O(N + K)$ linear time**.

```
Stable Sort Preservation:
Input:   [ (Apple, 2), (Banana, 1), (Orange, 2) ]
Stable:  [ (Banana, 1), (Apple, 2), (Orange, 2) ]  ──► Apple precedes Orange
Unstable:[ (Banana, 1), (Orange, 2), (Apple, 2) ]  ──► Order altered!
```

---

## 3. How It Works Internally

### 1. Merge Sort (Divide & Conquer)
- Recursively divides array in halves until subproblems have size 1 ($O(\log N)$ division depth).
- Merges two sorted subarrays back together in $O(N)$ time.
- **Always Stable**, guaranteed **$O(N \log N)$** worst-case time, but requires $O(N)$ auxiliary space.

### 2. Quick Sort (Partitioning)
- Selects a **pivot** element.
- Re-arranges the array so elements $<$ pivot move to the left, and elements $>$ pivot move to the right.
- Recursively sorts left and right partitions.
- **In-Place ($O(1)$ space)**, average **$O(N \log N)$**, but worst-case degrades to **$O(N^2)$** if pivot selection is poor and is inherently **unstable**.

---

## 4. Syntax & API Reference

### Production-Grade Merge Sort & 3-Way Quick Sort
```javascript
/**
 * Stable Merge Sort Implementation
 * Time: O(N log N) | Space: O(N)
 */
export function mergeSort(arr, comparator = (a, b) => a - b) {
  if (arr.length <= 1) return arr;

  const mid = Math.floor(arr.length / 2);
  const left = mergeSort(arr.slice(0, mid), comparator);
  const right = mergeSort(arr.slice(mid), comparator);

  return merge(left, right, comparator);
}

function merge(left, right, comparator) {
  const result = [];
  let i = 0;
  let j = 0;

  while (i < left.length && j < right.length) {
    if (comparator(left[i], right[j]) <= 0) {
      result.push(left[i++]);
    } else {
      result.push(right[j++]);
    }
  }

  while (i < left.length) result.push(left[i++]);
  while (j < right.length) result.push(right[j++]);

  return result;
}

/**
 * In-Place QuickSort with 3-Way Partitioning (Dutch National Flag)
 * Handles duplicate keys in O(N) optimal time.
 */
export function quickSort3Way(arr, low = 0, high = arr.length - 1, comparator = (a, b) => a - b) {
  if (low >= high) return arr;

  // Randomized pivot selection to avoid O(N^2) worst case on sorted inputs
  const randomPivotIdx = low + Math.floor(Math.random() * (high - low + 1));
  [arr[low], arr[randomPivotIdx]] = [arr[randomPivotIdx], arr[low]];

  const pivot = arr[low];
  let lt = low;      // arr[low..lt-1] < pivot
  let gt = high;     // arr[gt+1..high] > pivot
  let i = low + 1;   // arr[lt..i-1] == pivot

  while (i <= gt) {
    const cmp = comparator(arr[i], pivot);
    if (cmp < 0) {
      [arr[lt], arr[i]] = [arr[i], arr[lt]];
      lt++;
      i++;
    } else if (cmp > 0) {
      [arr[i], arr[gt]] = [arr[gt], arr[i]];
      gt--;
    } else {
      i++;
    }
  }

  quickSort3Way(arr, low, lt - 1, comparator);
  quickSort3Way(arr, gt + 1, high, comparator);

  return arr;
}
```

---

## 5. Practical Examples

### Example 1: Non-Comparison Counting Sort ($O(N + K)$ Linear Time)
```javascript
/**
 * Linear-time sort for non-negative integers bounded by maxVal.
 * Time Complexity: O(N + K) | Space Complexity: O(K)
 */
export function countingSort(nums, maxVal) {
  const counts = new Int32Array(maxVal + 1);

  // 1. Frequency count
  for (let i = 0; i < nums.length; i++) {
    counts[nums[i]]++;
  }

  // 2. Reconstruct sorted array
  let writeIdx = 0;
  for (let val = 0; val <= maxVal; val++) {
    while (counts[val] > 0) {
      nums[writeIdx++] = val;
      counts[val]--;
    }
  }

  return nums;
}
```

### Example 2: QuickSelect ($O(N)$ Average Top-$K$ Selection)
```javascript
/**
 * Finds the K-th smallest element without sorting the entire array.
 * Time Complexity: O(N) average, O(N^2) worst.
 */
export function quickSelect(arr, k, low = 0, high = arr.length - 1) {
  if (low === high) return arr[low];

  const pivotIdx = partition(arr, low, high);

  if (k === pivotIdx) {
    return arr[k];
  } else if (k < pivotIdx) {
    return quickSelect(arr, k, low, pivotIdx - 1);
  } else {
    return quickSelect(arr, k, pivotIdx + 1, high);
  }
}

function partition(arr, low, high) {
  const pivot = arr[high];
  let i = low;

  for (let j = low; j < high; j++) {
    if (arr[j] <= pivot) {
      [arr[i], arr[j]] = [arr[j], arr[i]];
      i++;
    }
  }

  [arr[i], arr[high]] = [arr[high], arr[i]];
  return i;
}
```

### Example 3: Multi-Key Stable Sorting
```javascript
export function sortUsers(users) {
  // Sort primarily by department, secondarily by salary descending, tertiarily by name
  return [...users].sort((a, b) => {
    // 1. Department ascending
    const deptCompare = a.department.localeCompare(b.department);
    if (deptCompare !== 0) return deptCompare;

    // 2. Salary descending
    if (b.salary !== a.salary) {
      return b.salary - a.salary;
    }

    // 3. Name ascending
    return a.name.localeCompare(b.name);
  });
}
```

### Example 4: Sort Colors / Dutch National Flag (In-Place $O(N)$)
```javascript
export function sortColors(nums) {
  // 0: Red, 1: White, 2: Blue
  let low = 0;
  let mid = 0;
  let high = nums.length - 1;

  while (mid <= high) {
    if (nums[mid] === 0) {
      [nums[low], nums[mid]] = [nums[mid], nums[low]];
      low++;
      mid++;
    } else if (nums[mid] === 1) {
      mid++;
    } else {
      [nums[mid], nums[high]] = [nums[high], nums[mid]];
      high--;
    }
  }

  return nums;
}
```

### Example 5: Insertion Sort for Nearly-Sorted or Small Arrays
```javascript
export function insertionSort(arr) {
  for (let i = 1; i < arr.length; i++) {
    const key = arr[i];
    let j = i - 1;

    // Shift elements greater than key to the right
    while (j >= 0 && arr[j] > key) {
      arr[j + 1] = arr[j];
      j--;
    }

    arr[j + 1] = key;
  }

  return arr;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **The Native `Array.prototype.sort()` String Coercion Trap**:
   Calling `[10, 2, 5].sort()` evaluates without a comparator, converting numbers to UTF-16 strings: `'10' < '2' < '5'`. The sorted result is `[10, 2, 5]`. **Always supply a numeric comparator `(a, b) => a - b`**.
2. **QuickSort Recursion Depth and Worst-Case $O(N^2)$**:
   If an already sorted or reverse-sorted array is partitioned using the first or last element as pivot, the subproblem sizes become $N - 1$ and $0$. The tree height becomes $N$, degrading time to $O(N^2)$ and overflowing the call stack. Mitigate by using **randomized pivot selection** or median-of-three.
3. **Stability Invariant Destruction**:
   Calling an unstable sort (like standard QuickSort or HeapSort) on a multi-column table breaks prior column sort orders. Always use a stable sort (like TimSort or MergeSort) when secondary sort orders must be preserved.
4. **Non-Integer Keys in Counting Sort**:
   Counting sort requires non-negative discrete integer keys. Negative numbers or floating-point decimals corrupt index offsets unless transformed with an offset bias.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Hazard | Production Fix |
| :--- | :--- | :--- |
| `arr.sort()` without comparator | Sorts numbers lexicographically (`[1, 10, 2]`), breaking logic. | Pass explicit numeric comparator `(a, b) => a - b`. |
| QuickSort with deterministic edge pivot | Degrades to $O(N^2)$ and causes Stack Overflow on sorted data. | Use randomized pivot or median-of-three. |
| Inverting comparator to boolean (`a > b`) | Evaluates incorrectly in V8; sort becomes non-deterministic. | Return negative, zero, or positive integer numbers. |
| Mutating input array unintentionally | `arr.sort()` mutates in-place, causing unexpected side effects. | Use `arr.toSorted()` (ES2023) or `[...arr].sort()`. |
| MergeSort creating thousands of array slices | Heavy GC allocation churn on intermediate arrays. | Use an in-place merge sort or reuse pre-allocated scratch buffer. |
| Using $O(N^2)$ Bubble Sort in production | Freezes browser event loop on inputs $>5,000$ items. | Use V8 native `.sort()` or MergeSort/QuickSort. |

---

## 8. Performance Characteristics

| Algorithm | Best Time | Average Time | Worst Time | Space | Stable? |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Insertion Sort** | $O(N)$ | $O(N^2)$ | $O(N^2)$ | $O(1)$ | Yes |
| **Selection Sort** | $O(N^2)$ | $O(N^2)$ | $O(N^2)$ | $O(1)$ | No |
| **Merge Sort** | $O(N \log N)$ | $O(N \log N)$ | $O(N \log N)$ | $O(N)$ | Yes |
| **Quick Sort (Random)**| $O(N \log N)$ | $O(N \log N)$ | $O(N^2)$ | $O(\log N)$ | No |
| **Heap Sort** | $O(N \log N)$ | $O(N \log N)$ | $O(N \log N)$ | $O(1)$ | No |
| **Counting Sort** | $O(N + K)$ | $O(N + K)$ | $O(N + K)$ | $O(K)$ | Yes |
| **TimSort (Native)** | $O(N)$ | $O(N \log N)$ | $O(N \log N)$ | $O(N)$ | Yes |

---

## 9. Engine Internals

### V8's Native TimSort Implementation
In modern V8 (Node.js and Chromium), `Array.prototype.sort()` uses **TimSort** (standardized as a stable sort in ECMAScript 2019):
- **Adaptive Run Detection**: TimSort scans the array for already sorted contiguous runs (either ascending or strictly descending).
- **Insertion Sort for Small Slices**: Subarrays smaller than `minRun` (typically 32 to 64 elements) are sorted using Insertion Sort, which outperforms complex algorithms on small datasets due to minimal overhead and high cache locality.
- **Galloping Mode**: During the merge phase, if one run consistently wins over another, TimSort enters "galloping mode" using binary search to find insertion points in chunks, drastically reducing comparison counts.

---

## 10. Best Practices & Design Guidelines

1. **Use `arr.toSorted()` for Immutability**: ECMAScript 2023 introduced `toSorted()`, which returns a newly sorted array without mutating the original.
2. **Always Supply a Numeric Comparator**: Never trust default `.sort()` on numbers.
3. **Use Counting Sort for Constrained Integer Domains**: If sorting 1,000,000 ages ($0 \dots 120$), Counting Sort completes in $O(N)$ linear time ($\approx 3\text{ms}$).
4. **Use QuickSelect for Median / Top-$K$**: Never sort an entire array if you only need the $K$-th element or top $K$ items.

---

## 11. Real-World Case Study

### Multi-Column Data Grid Table Sorting
**Scenario**: An enterprise analytics dashboard displayed 50,000 employee records in an interactive data grid. Users frequently sorted by Department, then clicked to sort by Salary within that department.
**Problem**: An early custom QuickSort implementation was unstable. Sorting by Salary scrambled the previously grouped Department order, causing severe UI confusion.

**Solution**:
- Migrated to native `Array.prototype.sort()` (guaranteed stable TimSort by ECMAScript specification).
- Re-architected multi-column sorting to evaluate compound comparator logic:
  ```javascript
  records.sort((a, b) => {
    return a.department.localeCompare(b.department) || b.salary - a.salary;
  });
  ```

**Outcome**:
- Secondary sort stability 100% preserved.
- Execution completed in $<15\text{ms}$ for 50,000 records leveraging native C++ TimSort speed.

---

## 12. Interview Questions

<details>
<summary>1. Why is <code>Array.prototype.sort()</code> in modern JavaScript guaranteed to be stable?</summary>

Prior to ECMAScript 2019, the language specification did not mandate stability; V8 used QuickSort for arrays $>10$ elements, making sort results non-deterministic for equal keys. In ECMAScript 2019, the specification explicitly mandated that `Array.prototype.sort` must be **stable**. V8 re-implemented sorting using **TimSort**, guaranteeing that elements with equal comparator values permanently retain their original relative positions.
</details>

<details>
<summary>2. Why does <code>[1, 10, 2, 21].sort()</code> output <code>[1, 10, 2, 21]</code>?</summary>

When no comparator function is supplied, the specification dictates that all elements are converted to strings and compared according to their UTF-16 code unit values. Comparing strings `'10'` and `'2'`, character `'1'` has a lower ASCII code (`49`) than `'2'` (`50`), so `'10'` is placed before `'2'`. To sort numerically, pass an explicit comparator: `arr.sort((a, b) => a - b)`.
</details>

<details>
<summary>3. What is the difference between Merge Sort and Quick Sort in terms of memory and performance?</summary>

- **Merge Sort**: Guaranteed $O(N \log N)$ in all cases; **Stable**; requires **$O(N)$ auxiliary memory** because merging cannot easily be done in-place without degrading performance.
- **Quick Sort**: Average $O(N \log N)$, but worst-case degrades to $O(N^2)$ on poor pivot choices; **Unstable**; runs **in-place with $O(\log N)$ stack space**; exhibits superior cache locality and smaller constant factors than Merge Sort on real hardware.
</details>

<details>
<summary>4. How does 3-Way Partitioning (Dutch National Flag) optimize QuickSort for duplicate keys?</summary>

Standard Lomuto or Hoare partitioning divides elements into two sets ($<$ and $\ge$). If an array contains thousands of duplicate values (e.g. an array of only 0s and 1s), standard QuickSort performs redundant recursive calls on the identical values, degrading to $O(N^2)$. 3-Way partitioning divides the array into three segments: elements $<$ pivot, elements $==$ pivot, and elements $>$ pivot. Subsequent recursive calls only process the $<$ and $>$ segments, completely skipping all duplicates in $O(N)$ linear time.
</details>

<details>
<summary>5. How does QuickSelect find the $K$-th smallest element in $O(N)$ average time?</summary>

QuickSelect uses the partition step from QuickSort. It chooses a pivot and partitions the array into elements $\le$ pivot and $>$ pivot. The pivot ends up at its final sorted index $P$.
- If $P == K$, the $K$-th element is found immediately.
- If $K < P$, we recurse only into the left partition.
- If $K > P$, we recurse only into the right partition.
Because we discard half the array at each step ($N + \frac{N}{2} + \frac{N}{4} + \dots = 2N$), average time complexity is linear **$O(N)$**, compared to $O(N \log N)$ for full sorting.
</details>

<details>
<summary>6. Under what conditions can an algorithm sort faster than $O(N \log N)$?</summary>

When the algorithm is a **Non-Comparison Sort** (e.g. Counting Sort, Radix Sort, Bucket Sort). The theoretical mathematical lower bound of $\Omega(N \log N)$ applies strictly to *comparison-based* sorting (visualized as a decision tree with $N!$ leaves). When sorting data where keys are bounded integers or fixed-length strings, we can count frequencies or bucket by digit representations directly, achieving linear **$O(N + K)$** runtime.
</details>

<details>
<summary>7. What is TimSort and why is it used in production runtimes (V8, Python)?</summary>

TimSort is an adaptive hybrid sorting algorithm combining **Merge Sort** and **Insertion Sort**:
1. Real-world data often contains pre-existing sorted runs. TimSort identifies these natural runs.
2. Small runs are expanded and sorted using Insertion Sort (which has ultra-fast CPU cache locality).
3. Balanced runs are merged using a smart galloping merge algorithm.
It achieves $O(N)$ best-case time on already-sorted data while guaranteeing $O(N \log N)$ worst-case time and 100% stability.
</details>

<details>
<summary>8. What is the difference between <code>arr.sort()</code> and <code>arr.toSorted()</code>?</summary>

`arr.sort()` mutates the original array in place and returns a reference to that mutated array. `arr.toSorted()` (introduced in ECMAScript 2023) leaves the original array untouched and returns a brand-new sorted shallow copy. `toSorted()` is preferred in modern immutable programming (e.g. React state updates).
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Sort an Array of Objects by Multiple Fields
**Specification**: Write a function `sortInventory(items)` that sorts items primarily by `category` ascending, and secondarily by `price` ascending using a clean comparator.

<details>
<summary>Solution</summary>

```javascript
export function sortInventory(items) {
  return [...items].sort((a, b) => {
    const catCmp = a.category.localeCompare(b.category);
    if (catCmp !== 0) return catCmp;
    return a.price - b.price;
  });
}
```
</details>

### Exercise 2: Merge Overlapping Intervals
**Specification**: Given an array of intervals where `intervals[i] = [start, end]`, merge all overlapping intervals, and return an array of the non-overlapping intervals that cover all the intervals in the input.

<details>
<summary>Solution</summary>

```javascript
export function mergeIntervals(intervals) {
  if (intervals.length <= 1) return intervals;

  // 1. Sort intervals by start time ascending
  intervals.sort((a, b) => a[0] - b[0]);

  const merged = [intervals[0]];

  for (let i = 1; i < intervals.length; i++) {
    const current = intervals[i];
    const lastMerged = merged[merged.length - 1];

    // If current interval overlaps with last merged interval
    if (current[0] <= lastMerged[1]) {
      lastMerged[1] = Math.max(lastMerged[1], current[1]);
    } else {
      merged.push(current);
    }
  }

  return merged;
}
```
</details>
