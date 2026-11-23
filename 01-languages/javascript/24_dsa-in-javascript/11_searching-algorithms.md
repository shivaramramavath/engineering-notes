# Searching Algorithms & Binary Search Patterns in JavaScript

> A production-grade algorithmic guide to Searching in JavaScript, featuring Linear Search, Classic Binary Search, Lower/Upper Bounds, Rotated Array searches, and Binary Search on Answer spaces.

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

Locating a target value within a collection of data is an omnipresent operation across software systems. While unsorted collections require **$O(N)$ linear scans** (e.g. `array.find()`, `array.indexOf()`), ordered collections unlock **$O(\log N)$ logarithmic search**.

Binary Search is not limited to searching arrays; it is a general algorithmic paradigm for **optimizing monotonic decision spaces**:
- In an array of $1,000,000,000$ elements, a linear search executes up to $1,000,000,000$ comparisons, freezing an event loop for seconds.
- Binary Search locates the target in at most **30 comparisons** ($\log_2 10^9 \approx 29.89$), completing in sub-microsecond time.
- Applying binary search to monotonic functions ("Binary Search on Answer") solves complex minimax optimization problems in logistics, rate limiting, and capacity scheduling.

---

## 2. Core Concepts

### Search Paradigms
1. **Linear Search ($O(N)$)**: Evaluates each element sequentially from index $0$ to $N - 1$. Optimal only for unsorted data or small arrays ($N < 20$).
2. **Exact-Match Binary Search ($O(\log N)$)**: Finds an exact target in a sorted collection.
3. **Lower Bound (First Occurrence / Bisect Left)**: Finds the first index where element $\ge \text{target}$.
4. **Upper Bound (Last Occurrence / Bisect Right)**: Finds the first index where element $> \text{target}$.
5. **Binary Search on Answer Space**: Applied when a problem asks for the "minimum possible maximum" or "maximum possible minimum", and a feasibility function `canAchieve(mid)` is monotonic ($FFFFTTTT$).

```
Exact Match:
[ 1 , 3 , 5 , 7 , 9 , 11 , 13 ] ──► Target: 7 ──► Index: 3

Lower Bound / Bisect Left (Target = 4):
[ 1 , 2 , 4 , 4 , 4 , 6 , 8 ]
          ▲ (Index 2: First element >= 4)

Upper Bound / Bisect Right (Target = 4):
[ 1 , 2 , 4 , 4 , 4 , 6 , 8 ]
                      ▲ (Index 5: First element > 4)
```

---

## 3. How It Works Internally

Binary search maintains two boundary pointers (`low` and `high`). At each iteration, it inspects the midpoint:
$$\text{mid} = \text{low} + \lfloor (\text{high} - \text{low}) / 2 \rfloor$$
- If `arr[mid] === target`, target is located.
- If `arr[mid] < target`, the target must reside in the right half: $\text{low} = \text{mid} + 1$.
- If `arr[mid] > target`, the target must reside in the left half: $\text{high} = \text{mid} - 1$.

```
Search Space:
low                               mid                               high
[ 10 | 20 | 30 | 40 | 50 | 60 | 70 | 80 | 90 | 100 | 110 | 120 | 130 ]
                                  │
                   Target (90) > arr[mid] (70)
                                  ▼
                     Shrink search space to right half:
                                 low                               high
                               [ 80 | 90 | 100 | 110 | 120 | 130 ]
```

---

## 4. Syntax & API Reference

### Universal Binary Search & Bounds Templates
```javascript
/**
 * Exact Match Binary Search
 * Time: O(log N) | Space: O(1)
 */
export function binarySearch(arr, target) {
  let low = 0;
  let high = arr.length - 1;

  while (low <= high) {
    const mid = low + Math.floor((high - low) / 2);

    if (arr[mid] === target) return mid;
    if (arr[mid] < target) {
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }

  return -1;
}

/**
 * Lower Bound (Bisect Left): First index where arr[i] >= target.
 */
export function lowerBound(arr, target) {
  let low = 0;
  let high = arr.length; // Range [0, length]

  while (low < high) {
    const mid = low + Math.floor((high - low) / 2);
    if (arr[mid] >= target) {
      high = mid; // Candidate found; search left for earlier occurrence
    } else {
      low = mid + 1;
    }
  }

  return low;
}

/**
 * Upper Bound (Bisect Right): First index where arr[i] > target.
 */
export function upperBound(arr, target) {
  let low = 0;
  let high = arr.length;

  while (low < high) {
    const mid = low + Math.floor((high - low) / 2);
    if (arr[mid] > target) {
      high = mid;
    } else {
      low = mid + 1;
    }
  }

  return low;
}
```

---

## 5. Practical Examples

### Example 1: Find First and Last Position of Element in Sorted Array
```javascript
import { lowerBound, upperBound } from './binarySearch.js';

export function searchRange(nums, target) {
  const start = lowerBound(nums, target);

  // If start is out of bounds or not equal to target
  if (start === nums.length || nums[start] !== target) {
    return [-1, -1];
  }

  const end = upperBound(nums, target) - 1;
  return [start, end];
}

// console.log(searchRange([5, 7, 7, 8, 8, 10], 8)); // [3, 4]
```

### Example 2: Search in Rotated Sorted Array ($O(\log N)$)
```javascript
export function searchRotated(nums, target) {
  let low = 0;
  let high = nums.length - 1;

  while (low <= high) {
    const mid = low + Math.floor((high - low) / 2);

    if (nums[mid] === target) return mid;

    // Check if left half is normally sorted
    if (nums[low] <= nums[mid]) {
      // Is target within the sorted left half?
      if (nums[low] <= target && target < nums[mid]) {
        high = mid - 1;
      } else {
        low = mid + 1;
      }
    }
    // Otherwise, right half must be sorted
    else {
      // Is target within the sorted right half?
      if (nums[mid] < target && target <= nums[high]) {
        low = mid + 1;
      } else {
        high = mid - 1;
      }
    }
  }

  return -1;
}

// console.log(searchRotated([4, 5, 6, 7, 0, 1, 2], 0)); // 4
```

### Example 3: Search a 2D Matrix ($O(\log(M \times N))$)
```javascript
/**
 * Searches an M x N matrix where each row is sorted and first integer
 * of each row is greater than last of previous row.
 */
export function searchMatrix(matrix, target) {
  if (!matrix || matrix.length === 0 || matrix[0].length === 0) return false;

  const m = matrix.length;
  const n = matrix[0].length;
  let low = 0;
  let high = m * n - 1;

  while (low <= high) {
    const mid = low + Math.floor((high - low) / 2);
    // Convert 1D virtual index to 2D row/col coordinates
    const row = Math.floor(mid / n);
    const col = mid % n;
    const midVal = matrix[row][col];

    if (midVal === target) return true;
    if (midVal < target) {
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }

  return false;
}
```

### Example 4: Koko Eating Bananas (Binary Search on Answer Space)
```javascript
export function minEatingSpeed(piles, h) {
  let low = 1;
  let high = Math.max(...piles);
  let result = high;

  function canFinish(speed) {
    let totalHours = 0;
    for (const pile of piles) {
      totalHours += Math.ceil(pile / speed);
    }
    return totalHours <= h;
  }

  while (low <= high) {
    const midSpeed = low + Math.floor((high - low) / 2);

    if (canFinish(midSpeed)) {
      result = midSpeed;   // Feasible; try searching for a slower speed
      high = midSpeed - 1;
    } else {
      low = midSpeed + 1;  // Too slow; increase speed
    }
  }

  return result;
}
```

### Example 5: Find Peak Element ($O(\log N)$)
```javascript
export function findPeakElement(nums) {
  let low = 0;
  let high = nums.length - 1;

  while (low < high) {
    const mid = low + Math.floor((high - low) / 2);

    // If mid element is smaller than its right neighbor, a peak exists on the right
    if (nums[mid] < nums[mid + 1]) {
      low = mid + 1;
    } else {
      high = mid; // Peak is at mid or to the left
    }
  }

  return low;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Integer Overflow in Midpoint Calculation**:
   In languages with 32-bit signed integers (C/C++, Java), calculating `(low + high) / 2` overflows when `low + high > 2,147,483,647`. While JavaScript numbers are 64-bit IEEE-754 floats capable of exact integer representation up to $2^{53} - 1$ (`Number.MAX_SAFE_INTEGER`), using `low + Math.floor((high - low) / 2)` or `(low + high) >>> 1` is an industry-standard best practice that avoids unexpected floating-point precision issues.
2. **Infinite Loops in Boundary Updates**:
   When writing `while (low < high)`, if the midpoint calculation floors downward and the update rule is `low = mid`, when `high = low + 1`, `mid` evaluates to `low`. The boundary never changes, producing an **infinite loop**. Rule:
   - If `low = mid + 1`, use `mid = Math.floor((low + high) / 2)`.
   - If `high = mid - 1`, use `mid = Math.floor((low + high) / 2)`.
   - If `low = mid`, compute ceiling: `mid = Math.ceil((low + high) / 2)`.
3. **Array Must Be Sorted**:
   Binary search relies strictly on the sorted invariant. Running binary search on unsorted data produces arbitrary, incorrect results.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Defect | Production Remediation |
| :--- | :--- | :--- |
| `mid = (low + high) / 2` without rounding | Produces floating-point decimal indices (`arr[2.5] === undefined`). | Use `Math.floor()` or unsigned bitwise shift `>>> 1`. |
| Infinite loop via `low = mid` with floor | Loop halts progress when search space reaches 2 elements. | Use `low = mid + 1` or calculate ceiling midpoint. |
| Running binary search on unsorted array | Returns false negatives; target elements missed. | Sort collection first or use linear search if only searching once. |
| Linear search on million-element datasets | Freezes event loop thread for seconds on every search. | Sort data and use binary search ($O(\log N)$). |
| Off-by-one in rotated array binary search | Fails when `nums[low] === nums[mid]` on duplicates. | Check strict inequality or advance `low++` on duplicates. |
| Forgetting to convert 1D index to 2D in matrix | Throws out-of-bounds error. | Row: `Math.floor(mid / cols)`, Col: `mid % cols`. |

---

## 8. Performance Characteristics

- **Time Complexity**:
  - Best Case: $O(1)$ (target located at initial midpoint).
  - Average Case: $O(\log N)$.
  - Worst Case: $O(\log N)$.
- **Space Complexity**:
  - Iterative Implementation: **$O(1)$ constant auxiliary space**.
  - Recursive Implementation: $O(\log N)$ call stack space. Always favor iterative binary search in JavaScript.

---

## 9. Engine Internals

### V8 Smi Optimization & Bitwise Shift Midpoint
- **Unsigned Bitwise Shift (`(low + high) >>> 1`)**:
  In V8, `(low + high) >>> 1` performs a 32-bit unsigned right bitwise shift. This simultaneously computes integer division by 2, discards fractional remainders, and yields a small integer (`Smi`) in a single hardware CPU instruction, executing faster than `Math.floor((low + high) / 2)`.

---

## 10. Best Practices & Design Guidelines

1. **Always Use Iterative Binary Search**: Saves call stack allocation and guarantees zero stack overflow risk.
2. **Standardize on `low <= high` for Exact Match**: Maintain clear boundary contraction (`low = mid + 1`, `high = mid - 1`).
3. **Use Lower/Upper Bounds for Range Frequency**: The count of elements matching `target` in a sorted array is simply:
   $$\text{Count} = \text{upperBound}(\text{target}) - \text{lowerBound}(\text{target})$$
4. **Identify Monotonic Feasibility in Word Problems**: If increasing a parameter monotonically transitions an outcome from impossible to possible ($FFFFTTTT$), apply Binary Search on the answer space.

---

## 11. Real-World Case Study

### Distributed Telemetry Log Timestamp Bisection
**Scenario**: An observability platform indexed 50,000,000 log entries sorted strictly by Unix epoch millisecond timestamps. Engineers debugging outages queried specific time ranges (e.g. all logs between 14:00:00 and 14:05:00).
**Naive Approach**:
Linear scan `logs.filter(log => log.ts >= start && log.ts <= end)` took $620\text{ms}$ on 50M records, timing out HTTP API queries.

**Binary Search Bounds Approach**:
```javascript
// O(log N) bounds lookup
const startIdx = lowerBound(logs, startTime);
const endIdx = upperBound(logs, endTime);
const results = logs.slice(startIdx, endIdx);
```
**Outcome**:
- Finding the start and end boundary indices took **$0.00008\text{ms}$ (80 nanoseconds)** across 50,000,000 items ($26$ comparisons).
- Query latency dropped from $620\text{ms}$ to $<1\text{ms}$.

---

## 12. Interview Questions

<details>
<summary>1. Why does Binary Search have an $O(\log N)$ time complexity?</summary>

At each iteration, the algorithm compares the target with the middle element. By determining whether the target lies in the left or right half, it discards exactly half of the remaining search space. The number of times a collection of size $N$ can be divided by 2 before reaching 1 is $\log_2 N$. Thus, the maximum number of iterations is $\lceil \log_2 N \rceil$, resulting in strict logarithmic time complexity.
</details>

<details>
<summary>2. What is the difference between <code>lowerBound</code> and <code>upperBound</code>?</summary>

- `lowerBound(target)` finds the first index where element $\ge \text{target}$. If the target exists, it returns the index of its **first occurrence**.
- `upperBound(target)` finds the first index where element $> \text{target}$. If the target exists, it returns the index immediately following its **last occurrence**.
The number of occurrences of `target` in a sorted array is `upperBound(target) - lowerBound(target)`.
</details>

<details>
<summary>3. Why is <code>(low + high) >>> 1</code> commonly used instead of <code>Math.floor((low + high) / 2)</code>?</summary>

The unsigned right shift operator `>>> 1` coerces the sum to a 32-bit unsigned integer and shifts bits to the right by 1, which performs integer division by 2 and floor truncation in a single native CPU instruction. It is concise, fast, and guarantees an integer output in JavaScript.
</details>

<details>
<summary>4. How does Binary Search work in a Rotated Sorted Array?</summary>

Even though the entire array is not sorted, at any midpoint, **at least one half (left or right) is guaranteed to be normally sorted**:
1. Check if left half is sorted (`nums[low] <= nums[mid]`). If so, check if `target` falls within `[nums[low], nums[mid])`. If yes, search left; else search right.
2. Otherwise, the right half must be sorted. Check if `target` falls within `(nums[mid], nums[high]]`. If yes, search right; else search left.
This preserves the $O(\log N)$ runtime.
</details>

<details>
<summary>5. What is "Binary Search on Answer", and when should you use it?</summary>

Binary search on answer is used when the problem asks for the minimum or maximum value of a variable $X$ that satisfies a condition, and the feasibility of $X$ is monotonic:
- If a value $K$ works, all values $> K$ also work (or vice versa: $FFFFTTTT$).
Instead of searching through an array of data, we binary search over the **range of possible answers** $[X_{\text{min}}, X_{\text{max}}]$, checking validity at each midpoint with a greedy helper function `isValid(mid)`. Time complexity is $O(\text{Cost}(\text{check}) \times \log(\text{Range}))$.
</details>

<details>
<summary>6. What causes an infinite loop in Binary Search, and how do you prevent it?</summary>

An infinite loop occurs when the search range fails to shrink (e.g. `low === mid` when `high - low === 1`). If `low = mid` is used, the midpoint calculation must round up (`Math.ceil` or `low + Math.floor((high - low + 1) / 2)`). Standardizing on the symmetric template `while (low <= high)` with `low = mid + 1` and `high = mid - 1` prevents infinite loops because the search range shrinks by at least 1 on every iteration.
</details>

<details>
<summary>7. How do you map a 1D virtual index to 2D matrix coordinates?</summary>

For an $M \times N$ matrix (where $M$ is row count and $N$ is column count), a 1D index $I$ maps to:
- **Row**: $\lfloor I / N \rfloor$
- **Column**: $I \pmod N$
This allows binary searching an entire 2D matrix in $O(\log(M \times N))$ without flattening the matrix in memory.
</details>

<details>
<summary>8. Can Binary Search be used on a Linked List?</summary>

Technically yes, but practically **no**. While binary search logic applies, finding the midpoint of a linked list requires traversing pointers from the head, which takes $O(N)$ linear time. Because finding the midpoint takes linear time at each step ($N + \frac{N}{2} + \frac{N}{4} \dots = 2N$), total time complexity is $O(N)$, completely negating the benefit of binary search. Fast $O(\log N)$ search on linked structures requires a **Skip List**.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Search Insert Position
**Specification**: Given a sorted array of distinct integers and a target value, return the index if the target is found. If not, return the index where it would be if it were inserted in order. Must run in $O(\log N)$ time.

<details>
<summary>Solution</summary>

```javascript
export function searchInsert(nums, target) {
  let low = 0;
  let high = nums.length - 1;

  while (low <= high) {
    const mid = low + Math.floor((high - low) / 2);

    if (nums[mid] === target) {
      return mid;
    } else if (nums[mid] < target) {
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }

  return low; // low is the insertion point
}
```
</details>

### Exercise 2: Find Minimum in Rotated Sorted Array
**Specification**: Suppose an array of length $n$ sorted in ascending order is rotated between 1 and $n$ times. Given the sorted rotated array `nums` of unique elements, return the minimum element of this array. Must run in $O(\log N)$ time.

<details>
<summary>Solution</summary>

```javascript
export function findMin(nums) {
  let low = 0;
  let high = nums.length - 1;

  while (low < high) {
    const mid = low + Math.floor((high - low) / 2);

    // If mid element is greater than high element, minimum must be in right half
    if (nums[mid] > nums[high]) {
      low = mid + 1;
    } else {
      // Minimum is at mid or in left half
      high = mid;
    }
  }

  return nums[low];
}
```
</details>
