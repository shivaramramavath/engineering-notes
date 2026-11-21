# Searching Algorithms in C

> Deep exploration of search algorithms in C: branch-free binary search, integer-overflow mitigation, interpolation search, exponential search, and standard library `bsearch()`.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [02-core-concepts/01-arrays.md](../../02-core-concepts/01-arrays.md), [12-sorting-algorithms.md](./12-sorting-algorithms.md) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline** | ISO C11 / C17 / C23 |

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
9. [Comparison: Searching Algorithms](#9-comparison-searching-algorithms)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
Searching is the algorithmic task of locating an element (or confirming its absence) within a collection. While linear scan ($O(N)$) works on unsorted buffers, high-throughput systems require sub-linear algorithms ($O(\log N)$ or $O(\log \log N)$).

Why searching in C requires specialized attention:
- **Integer Overflow**: The infamous `mid = (low + high) / 2` bug persisted in the JDK and standard C codebases for decades before being recognized.
- **CPU Branch Prediction**: Naive binary search suffers branch mispredictions on every iteration; branch-free binary search achieves up to 2x-3x speedup.
- **Unbounded Streams**: Searching infinite data streams where the total length $N$ is unknown (Exponential Search).

---

## 2. Core Concepts

### 2.1 The Overflow Bug
Computing `(low + high) / 2` fails when `low + high > INT_MAX`. The sum overflows into negative values, producing an invalid negative index that causes segmentation faults.
The overflow-safe formula:
$$\text{mid} = \text{low} + \frac{\text{high} - \text{low}}{2}$$

### 2.2 Variants of Binary Search
- **Exact Match**: Returns index of target, or $-1$ if absent.
- **Lower Bound (`lower_bound`)**: First index where `arr[i] >= target`.
- **Upper Bound (`upper_bound`)**: First index where `arr[i] > target`.

### 2.3 Interpolation Search
For uniformly distributed sorted data (e.g. phone books, timestamps), interpolation search estimates the probe position mathematically:
$$\text{pos} = \text{low} + \left( \frac{\text{target} - \text{arr}[\text{low}]}{\text{arr}[\text{high}] - \text{arr}[\text{low}]} \right) \times (\text{high} - \text{low})$$
- Average time complexity: $O(\log \log N)$.
- Worst-case (exponential distribution): $O(N)$.

### 2.4 Exponential Search
For unbounded or streaming arrays:
1. Find range by testing powers of two ($1, 2, 4, 8, \dots$) until `arr[i] >= target`.
2. Perform standard binary search within the bounded window $[i/2, \min(i, N-1)]$.
- Time complexity: $O(\log K)$ where $K$ is the target's position.

---

## 3. How It Works Internally

### Binary Search Range Halving

```
Search Space: [ 5, 12, 23, 38, 45, 67, 78, 89, 99 ] (N=9, Target=45)
Iteration 1: low=0, high=8, mid=4 -> arr[4] == 45 (FOUND!)

If Target=23:
Iteration 1: mid=4 (arr[4]=45 > 23) -> high = mid - 1 = 3
Iteration 2: low=0, high=3, mid=1 -> arr[1]=12 < 23 -> low = mid + 1 = 2
Iteration 3: low=2, high=3, mid=2 -> arr[2]=23 (FOUND!)
```

```mermaid
flowchart TD
    Init["Range: [0 .. N-1]"] --> Mid["mid = low + (high - low)/2"]
    Mid --> Comparr[mid] == target?
    Comp -->|Yes| Found["Return mid"]
    Comp -->|arr[mid] < target| MoveRight["low = mid + 1"]
    Comp -->|arr[mid] > target| MoveLeft["high = mid - 1"]
    MoveRight --> Nextlow <= high?
    MoveLeft --> Next
    Next -->|Yes| Mid
    Next -->|No| NotFound["Return -1"]
```

---

## 4. Syntax & Usage

### The C Standard Library `bsearch()`
Declared in `<stdlib.h>`:
```c
void *bsearch(const void *key, const void *base, size_t nel, size_t width,
              int (*compar)(const void *, const void *));
```

---

## 5. Practical Examples

### Example 1: Overflow-Safe Binary Search
Goal: Search for elements in a sorted array using overflow-safe midpoint calculation, verifying found and missing items.

```c
#include <stdio.h>
#include <stdbool.h>

static int binary_search_safe(const int *arr, int size, int target) {
    int low = 0;
    int high = size - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2; // Prevents integer overflow
        if (arr[mid] == target) return mid;
        if (arr[mid] < target) low = mid + 1;
        else high = mid - 1;
    }
    return -1;
}

int main(void) {
    printf("=== Overflow-Safe Binary Search ===\n");
    int sorted[] = { 5, 12, 23, 38, 45, 67, 78, 89, 99 };
    int n = sizeof(sorted) / sizeof(sorted[0]);

    int idx_found = binary_search_safe(sorted, n, 45);
    int idx_miss = binary_search_safe(sorted, n, 50);

    printf("Search 45: index %d\n", idx_found);
    printf("Search 50: index %d\n", idx_miss);
    return 0;
}
```

**Verified Output:**
```text
=== Overflow-Safe Binary Search ===
Search 45: index 4
Search 50: index -1
```

---

## 6. Best Practices
1. **Always Use `low + (high - low) / 2`**: Never write `(low + high) / 2` to avoid integer overflow bugs.
2. **Use Half-Open Ranges $[\text{low}, \text{high})$**: In API design, half-open ranges avoid $-1$ off-by-one errors when array is empty.
3. **Branch-Free Binary Search for Low Latency**: Replacing `if (arr[mid] < target)` branches with conditional moves (`cmov`) prevents pipeline flushes caused by CPU branch mispredictions.
4. **Use Interpolation Search Only for Uniform Data**: If data distribution is heavily skewed (e.g. logarithmic), interpolation search degrades to $O(N)$.
5. **Verify Array Is Sorted**: Binary search produces incorrect results silently if input array is not strictly sorted. In debug builds, assert sortedness.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Approach |
| :--- | :--- | :--- |
| `mid = (low + high) / 2` | **Integer Overflow**: When `low + high > INT_MAX`, wraps to negative numbers causing segmentation fault. | Write `mid = low + (high - low) / 2`. |
| Infinite loop in `while (low < high)` | Updating `low = mid` without $+1$ causes loop to hang when `high - low == 1`. | Update `low = mid + 1` or adjust rounding direction. |
| Using `bsearch` on unsorted data | Returns random false negatives or wrong indices without any warning. | Sort array with `qsort` prior to `bsearch`. |
| Off-by-one with `high = size` | Accessing `arr[high]` when `high == size` reads out-of-bounds memory. | Initialize `high = size - 1` for inclusive ranges. |
| Division by zero in interpolation search | If `arr[high] == arr[low]`, denominator becomes 0, triggering SIGFPE. | Guard `if (arr[high] == arr[low]) return arr[low] == target ? low : -1;`. |

---

## 8. Performance & Security Notes

### Complexity Analysis

| Algorithm | Average Time | Worst-Case Time | Space | Precondition |
| :--- | :--- | :--- | :--- | :--- |
| **Linear Search** | $O(N)$ | $O(N)$ | $O(1)$ | None |
| **Binary Search** | $O(\log N)$ | $O(\log N)$ | $O(1)$ | Sorted Array |
| **Interpolation Search** | $O(\log \log N)$ | $O(N)$ | $O(1)$ | Sorted & Uniform |
| **Exponential Search** | $O(\log K)$ | $O(\log K)$ | $O(1)$ | Unbounded Sorted Array |

---

## 9. Comparison: Searching Algorithms

```
Use Linear Search when:
- Array is small (N < 32).
- Array is unsorted and sorting it would be more expensive than searching.

Use Binary Search when:
- General sorted array searching.
- Strict O(log N) guarantee required.

Use Interpolation Search when:
- Elements are uniformly distributed numeric keys (e.g. timestamps, memory addresses).
- Data set is massive and memory fetch cost dominates.

Use Exponential Search when:
- Searching unbounded streams or infinite data structures.
- Target is known to be near the beginning of the collection.
```

---

## 10. Interview Questions

<details>
<summary><strong>1. Why did the binary search bug in Java's `Arrays.binarySearch` remain undiscovered for over 9 years?</strong></summary>

The bug `mid = (low + high) / 2` only manifests when `low + high` exceeds $2^{31} - 1$ ($pprox 2.14$ billion elements). In the 1990s and early 2000s, computers rarely had enough RAM to allocate 2-billion-element arrays in user applications. When 64-bit systems enabled multi-gigabyte arrays, the integer overflow was triggered and documented by Joshua Bloch in 2006.
</details>

<details>
<summary><strong>2. How does Branch-Free Binary Search improve performance on modern CPUs?</strong></summary>

In standard binary search, whether the target is in the left or right half is fundamentally a 50/50 coin flip. Modern CPUs use branch predictors; a 50% prediction rate causes a pipeline flush every other iteration (each costing 15–20 CPU cycles). A branch-free version uses conditional instructions (`cmov`) or bit manipulation to update the search window without conditional branches, executing at predictable, uniform cycle latency.
</details>

<details>
<summary><strong>3. How do you find the first and last occurrence of a duplicate element in a sorted array?</strong></summary>

Run two binary searches:
1. `lower_bound`: when `arr[mid] == target`, don't stop; set `high = mid - 1` to push left.
2. `upper_bound`: when `arr[mid] == target`, don't stop; set `low = mid + 1` to push right.
Difference gives the count and boundary indices in $O(\log N)$ time.
</details>

<details>
<summary><strong>4. How do you search in a Rotated Sorted Array in $O(\log N)$ time?</strong></summary>

At least one half of the array ($[\text{low}, \text{mid}]$ or $[\text{mid}, \text{high}]$) is always strictly sorted:
1. If `arr[low] <= arr[mid]`, left half is sorted. Check if target lies within $[\text{arr}[\text{low}], \text{arr}[\text{mid}]]$; if so, search left, else search right.
2. Otherwise, right half is sorted. Check if target lies within $[\text{arr}[\text{mid}], \text{arr}[\text{high}]]$; if so, search right, else search left.
</details>

<details>
<summary><strong>5. What is Ternary Search and how does it compare to Binary Search?</strong></summary>

Ternary search divides the range into three parts using two midpoints ($m_1, m_2$). It performs $2 \log_3 N$ comparisons. Since $2 / \log_2 3 \approx 1.26$, ternary search performs $\approx 26\%$ more comparisons than binary search for finding a value in a sorted array. However, ternary search is optimal for finding the maximum or minimum of a unimodal function.
</details>

<details>
<summary><strong>6. How does Exponential Search locate elements in an unbounded array?</strong></summary>

Start at index $1$. Repeatedly double the index ($1, 2, 4, 8, 16, \dots$) until `arr[i] >= target` or index is out of bounds. The target is guaranteed to lie in $[i/2, i]$. Binary search is then run on this bounded slice in $O(\log K)$ time.
</details>

<details>
<summary><strong>7. How can Binary Search be used to compute the square root of a number without `sqrt()`?</strong></summary>

Binary search over the range $[0, X]$: compute `mid = low + (high - low)/2`. If `mid * mid <= X`, record `mid` as candidate and move `low = mid + 1`; else move `high = mid - 1`. Terminates in $O(\log X)$ time.
</details>

<details>
<summary><strong>8. What is the difference between `bsearch()` and `qsort()` comparators in C?</strong></summary>

Both comparators take two `const void *` pointers. However, in `bsearch(key, base, ...)`, the first argument passed to the comparator is **always** the search `key` pointer, while the second argument is a pointer to the array element being examined.
</details>

---

## 11. Practice Exercises

### Easy 1: Standard Binary Search
Write a function returning the index of `target`, or -1.

<details>
<summary>Solution</summary>

```c
int binary_search(const int *arr, int n, int target) {
    int low = 0, high = n - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (arr[mid] == target) return mid;
        if (arr[mid] < target) low = mid + 1;
        else high = mid - 1;
    }
    return -1;
}
```
</details>

### Easy 2: Find Lower Bound
Find first index where `arr[i] >= target` in a sorted array.

<details>
<summary>Solution</summary>

```c
int lower_bound(const int *arr, int n, int target) {
    int low = 0, high = n;
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (arr[mid] >= target) high = mid;
        else low = mid + 1;
    }
    return low;
}
```
</details>

### Easy 3: Find Upper Bound
Find first index where `arr[i] > target` in a sorted array.

<details>
<summary>Solution</summary>

```c
int upper_bound(const int *arr, int n, int target) {
    int low = 0, high = n;
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (arr[mid] > target) high = mid;
        else low = mid + 1;
    }
    return low;
}
```
</details>

### Medium 1: Search in Rotated Sorted Array
Implement search in rotated sorted array in $O(\log N)$ time.

<details>
<summary>Solution</summary>

```c
int search_rotated(const int *arr, int n, int target) {
    int low = 0, high = n - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (arr[mid] == target) return mid;
        if (arr[low] <= arr[mid]) { // Left half sorted
            if (target >= arr[low] && target < arr[mid]) high = mid - 1;
            else low = mid + 1;
        } else { // Right half sorted
            if (target > arr[mid] && target <= arr[high]) low = mid + 1;
            else high = mid - 1;
        }
    }
    return -1;
}
```
</details>

### Medium 2: Peak Element Finder
Find a peak element (`arr[i] > arr[i-1] && arr[i] > arr[i+1]`) in $O(\log N)$ time.

<details>
<summary>Solution</summary>

```c
int find_peak_element(const int *arr, int n) {
    int low = 0, high = n - 1;
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (arr[mid] > arr[mid + 1]) high = mid;
        else low = mid + 1;
    }
    return low;
}
```
</details>

### Medium 3: Interpolation Search Implementation
Implement interpolation search handling duplicate keys safely.

<details>
<summary>Solution</summary>

```c
int interpolation_search(const int *arr, int n, int target) {
    int low = 0, high = n - 1;
    while (low <= high && target >= arr[low] && target <= arr[high]) {
        if (arr[high] == arr[low]) {
            return arr[low] == target ? low : -1;
        }
        int pos = low + (int)(((long long)(target - arr[low]) * (high - low)) / (arr[high] - arr[low]));
        if (arr[pos] == target) return pos;
        if (arr[pos] < target) low = pos + 1;
        else high = pos - 1;
    }
    return -1;
}
```
</details>

### Hard 1: Branch-Free Binary Search
Implement branch-free binary search using pointer arithmetic.

<details>
<summary>Solution</summary>

```c
int binary_search_branchless(const int *arr, int n, int target) {
    const int *base = arr;
    int len = n;
    while (len > 1) {
        int half = len / 2;
        base = (base[half] < target) ? base + half : base;
        len -= half;
    }
    return (*base == target) ? (int)(base - arr) : -1;
}
```
</details>

### Hard 2: Median of Two Sorted Arrays
Find the median of two sorted arrays in $O(\log(\min(M, N)))$ time.

<details>
<summary>Solution</summary>

```c
// Algorithm:
// Binary search on the partition of the smaller array.
// Ensure left partition has equal elements to right partition.
// Verify max_left_A <= min_right_B and max_left_B <= min_right_A.
```
</details>

### Hard 3: Exponential Search for Unbounded Stream
Implement search on a simulated unbounded sorted stream.

<details>
<summary>Solution</summary>

```c
int exponential_search(const int *arr, int n, int target) {
    if (n == 0) return -1;
    if (arr[0] == target) return 0;
    int i = 1;
    while (i < n && arr[i] <= target) {
        i *= 2;
    }
    int low = i / 2;
    int high = (i < n) ? i : n - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (arr[mid] == target) return mid;
        if (arr[mid] < target) low = mid + 1;
        else high = mid - 1;
    }
    return -1;
}
```
</details>

---

## 12. Summary & Cheat Sheet

| Algorithm | Formula / Strategy | Complexity |
| :--- | :--- | :--- |
| **Safe Midpoint** | `low + (high - low) / 2` | Prevents integer overflow |
| **Lower Bound** | `if (arr[mid] >= T) high = mid; else low = mid + 1;` | First element $\ge T$ |
| **Interpolation**| `pos = low + ((T - arr[l]) * (h - l)) / (arr[h] - arr[l])` | $O(\log \log N)$ uniform |
| **Exponential** | Double index ($1, 2, 4, 8, \dots$) then binary search | $O(\log K)$ unbounded |

---

## 13. Further Reading
- Joshua Bloch, *Extra, Extra - Read All About It: Nearly All Binary Searches and Mergesorts are Broken*, Google AI Blog (2006).
- C11 Standard ISO/IEC 9899:2011, Section 7.22.5.1: The `bsearch` function.

---

[← Previous Note: Sorting Algorithms](./12-sorting-algorithms.md) | [Data Structures Index](./README.md) | [Next Note: Algorithm Complexity](./14-algorithm-complexity.md)
