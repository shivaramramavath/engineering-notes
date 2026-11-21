# Binary Search and Predicate Space Algorithmic Patterns in Java

> Classical binary search, lower/upper bound bisecting, search in rotated sorted arrays, and Binary Search on the Answer space in Java.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Advanced |
| **Prerequisites** | [`27-dsa/00_complexity-analysis.md`](00_complexity-analysis.md), [`27-dsa/01_arrays-and-strings.md`](01_arrays-and-strings.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified (Java 8 to 25 LTS) |
| **Standard** | Java SE 21 / 25 LTS |

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

Binary Search is a divide-and-conquer search algorithm that finds the position of a target value within a sorted array or monotonic search space in $O(\log N)$ logarithmic time.

Beyond searching simple arrays, its most powerful algorithmic application is **Binary Search on the Answer (Predicate Space)**: finding the optimal value $X$ where a feasibility predicate function $P(X)$ transitions monotonically from `false` to `true` (e.g. $[F, F, F, T, T, T]$).

---

## 2. Core Concepts

### 2.1 The Monotonicity Condition

Binary search does NOT strictly require a sorted array; it requires a **monotonic predicate condition**:
$$P(x) \le P(y) \quad \forall x \le y$$
If the domain can be partitioned into two distinct contiguous halves:
$$\text{Search Space: } [F, F, F, \dots, F, \mathbf{T}, T, T, \dots, T]$$
Binary search finds the first $\mathbf{T}$ in $O(\log(\text{Range}))$ evaluations of $P(x)$.

### 2.2 Boundary Templates: Closed vs Open Intervals

1. **Closed Interval $[L, R]$**:
   - `while (low <= high)`
   - Midpoint: `mid = low + (high - low) / 2`
   - Adjustments: `low = mid + 1` or `high = mid - 1`

2. **Half-Open Interval $[L, R)$ (Lower Bound / Bisect Left)**:
   - Finds first index where `arr[i] >= target`.
   - `while (low < high)`
   - `if (arr[mid] >= target) high = mid; else low = mid + 1;`
   - Terminates when `low == high`.

---

## 3. How It Works Internally

### 3.1 Eliminating 32-Bit Signed Integer Overflow

In Java, array lengths can reach $2 \times 10^9$. Naive midpoint calculation:
```java
int mid = (low + high) / 2; // BUG: Overflows if low + high > 2,147,483,647!
```
When `low + high` overflows, the high bit becomes 1, producing a negative number. When divided by 2, it produces a negative index, triggering `ArrayIndexOutOfBoundsException`.
- **The Idiomatic Fix**:
  $$mid = low + \frac{high - low}{2}$$
- **Unsigned Shift Alternative**:
  $$mid = (low + high) >>> 1$$

---

## 4. Syntax & Usage

### 4.1 Java Standard Library `Arrays.binarySearch()`

```java
import java.util.Arrays;

public class StdBinarySearch {
    public static void demonstrate() {
        int[] sorted = {10, 20, 30, 40, 50};
        int idx = Arrays.binarySearch(sorted, 30); // Returns 2

        // If not found, returns (-(insertion point) - 1)
        int missing = Arrays.binarySearch(sorted, 25); // Returns -3 (insertion point is 2)
    }
}
```

---

## 5. Practical Examples

### 5.1 Pattern 1: Lower Bound & Upper Bound (Bisect Left & Right)
```java
public class BisectBounds {
    // First element >= target
    public static int lowerBound(int[] arr, int target) {
        int low = 0, high = arr.length;
        while (low < high) {
            int mid = low + (high - low) / 2;
            if (arr[mid] >= target) high = mid;
            else low = mid + 1;
        }
        return low;
    }

    // First element > target
    public static int upperBound(int[] arr, int target) {
        int low = 0, high = arr.length;
        while (low < high) {
            int mid = low + (high - low) / 2;
            if (arr[mid] > target) high = mid;
            else low = mid + 1;
        }
        return low;
    }
}
```

### 5.2 Pattern 2: Search in Rotated Sorted Array (LeetCode 33)
```java
public class RotatedSortedSearch {
    public static int search(int[] nums, int target) {
        int low = 0, high = nums.length - 1;

        while (low <= high) {
            int mid = low + (high - low) / 2;
            if (nums[mid] == target) return mid;

            // Check if left half is normally sorted
            if (nums[low] <= nums[mid]) {
                if (target >= nums[low] && target < nums[mid]) {
                    high = mid - 1;
                } else {
                    low = mid + 1;
                }
            } else { // Right half is normally sorted
                if (target > nums[mid] && target <= nums[high]) {
                    low = mid + 1;
                } else {
                    high = mid - 1;
                }
            }
        }
        return -1;
    }
}
```

### 5.3 Pattern 3: Find Minimum in Rotated Sorted Array (LeetCode 153)
```java
public class RotatedSortedMin {
    public static int findMin(int[] nums) {
        int low = 0, high = nums.length - 1;

        while (low < high) {
            int mid = low + (high - low) / 2;
            if (nums[mid] > nums[high]) {
                low = mid + 1; // Inflection point lies to the right
            } else {
                high = mid;    // Inflection point is at mid or to the left
            }
        }
        return nums[low];
    }
}
```

### 5.4 Pattern 4: Binary Search on Answer — Koko Eating Bananas (LeetCode 875)
```java
public class KokoEatingBananas {
    public static int minEatingSpeed(int[] piles, int h) {
        int low = 1;
        int high = 0;
        for (int p : piles) high = Math.max(high, p);

        int result = high;
        while (low <= high) {
            int speed = low + (high - low) / 2;
            if (canFinish(piles, h, speed)) {
                result = speed;
                high = speed - 1; // Try slower speed
            } else {
                low = speed + 1;  // Must eat faster
            }
        }
        return result;
    }

    private static boolean canFinish(int[] piles, int h, int speed) {
        long hours = 0;
        for (int pile : piles) {
            hours += (pile + speed - 1) / speed; // Ceiling division
        }
        return hours <= h;
    }
}
```

### 5.5 Pattern 5: Capacity to Ship Packages Within D Days (LeetCode 1011)
```java
public class ShipWithinDays {
    public static int shipWithinDays(int[] weights, int days) {
        int maxWeight = 0;
        int totalWeight = 0;
        for (int w : weights) {
            maxWeight = Math.max(maxWeight, w);
            totalWeight += w;
        }

        int low = maxWeight, high = totalWeight;
        int bestCapacity = high;

        while (low <= high) {
            int capacity = low + (high - low) / 2;
            if (canShip(weights, days, capacity)) {
                bestCapacity = capacity;
                high = capacity - 1;
            } else {
                low = capacity + 1;
            }
        }
        return bestCapacity;
    }

    private static boolean canShip(int[] weights, int days, int capacity) {
        int requiredDays = 1;
        int currentLoad = 0;
        for (int w : weights) {
            if (currentLoad + w > capacity) {
                requiredDays++;
                currentLoad = 0;
            }
            currentLoad += w;
        }
        return requiredDays <= days;
    }
}
```

---


### 5.6 Pattern 6: Median of Two Sorted Arrays in O(log(min(M, N))) (LeetCode 4)
`java
public class MedianTwoSortedArrays {
    public static double findMedianSortedArrays(int[] nums1, int[] nums2) {
        if (nums1.length > nums2.length) return findMedianSortedArrays(nums2, nums1);
        int m = nums1.length, n = nums2.length;
        int low = 0, high = m;
        while (low <= high) {
            int pX = low + (high - low) / 2;
            int pY = (m + n + 1) / 2 - pX;
            int maxLeftX = (pX == 0) ? Integer.MIN_VALUE : nums1[pX - 1];
            int minRightX = (pX == m) ? Integer.MAX_VALUE : nums1[pX];
            int maxLeftY = (pY == 0) ? Integer.MIN_VALUE : nums2[pY - 1];
            int minRightY = (pY == n) ? Integer.MAX_VALUE : nums2[pY];
            if (maxLeftX <= minRightY && maxLeftY <= minRightX) {
                if ((m + n) % 2 == 0) return (Math.max(maxLeftX, maxLeftY) + Math.min(minRightX, minRightY)) / 2.0;
                else return Math.max(maxLeftX, maxLeftY);
            } else if (maxLeftX > minRightY) high = pX - 1;
            else low = pX + 1;
        }
        return 0.0;
    }
}
`

## 6. Best Practices

1. **Calculate Midpoint Safely**: Always write `low + (high - low) / 2` to prevent 32-bit signed integer overflow.
2. **Identify Monotonic Feasibility**: In optimization problems asking for "minimum speed to..." or "maximum distance such that...", formulate a helper `boolean isValid(mid)` function.
3. **Use Ceiling Division Without Floating Point**: Compute $\lceil A / B \rceil$ using pure integer arithmetic: `(A + B - 1) / B`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Integer Overflow on Sum** | `(low + high) / 2` produces negative index when sum $> 2^{31}-1$ | Use `low + (high - low) / 2` |
| **Infinite Loop on 2 Elements** | `low = mid` without rounding up leaves `mid == low`, looping forever | Use `low = mid + 1` or `mid = low + (high - low + 1) / 2` |
| **Floating Point in Predicate** | Floating point rounding errors (`(double) pile / speed`) fail ceiling check | Use integer arithmetic: `(pile + speed - 1) / speed` |
| **Wrong Interval Invariant** | Mixing `while (low < high)` with `high = mid - 1` misses target element | Match invariants consistently |

---

## 8. Performance & Security Notes

### 8.1 Branch Prediction & Binary Search
Because binary search jumps randomly across memory blocks ($N/2, N/4, N/8$), CPU hardware prefetchers cannot predict cache lines, and branch predictors often experience $\approx 50\%$ misprediction rates. For small arrays ($N \le 64$), linear scan using SIMD vector instructions often outperforms binary search!

---

## 9. Comparison / When to Use What

| Algorithm | Search Space | Complexity | Best For |
| :--- | :--- | :--- | :--- |
| **Linear Search** | Unsorted or small arrays ($N \le 64$) | $O(N)$ | Unsorted data, cache locality |
| **Classical Binary Search** | Sorted 1D/2D array | $O(\log N)$ | Point lookup, bound finding |
| **Rotated Binary Search** | Shifted sorted array | $O(\log N)$ | Pivot discovery, rotated arrays |
| **Binary Search on Answer** | Monotonic predicate function $P(x)$ | $O(N \log(\text{Range}))$ | Resource allocation, scheduling, min-max |

---

## 10. Interview Questions

<details>
<summary>1. How does Binary Search on the Answer space work?</summary>
Instead of searching indices in an array, it searches the range of possible answers $[L, R]$. A monotonic verification function `boolean isValid(mid)` checks if `mid` satisfies the constraints. If `isValid(mid)` is true, we narrow the search to find an even smaller/larger optimal answer.
</details>

<details>
<summary>2. Why does `(low + high) / 2` overflow in Java?</summary>
Java integers are 32-bit signed two's complement. If `low + high > 2,147,483,647`, the addition overflows to a negative integer, causing `(low + high) / 2` to yield a negative index and throw `ArrayIndexOutOfBoundsException`.
</details>

<details>
<summary>3. What is the difference between lower bound and upper bound?</summary>
Lower bound finds the first index where `arr[i] >= target`. Upper bound finds the first index where `arr[i] > target`.
</details>

<details>
<summary>4. How does binary search work on a rotated sorted array?</summary>
At least one half (left or right) of the array is always strictly sorted. We identify which half is sorted by comparing `nums[low]` and `nums[mid]`, then check if the target falls within that sorted half's bounds.
</details>

<details>
<summary>5. How do you implement ceiling division in integer arithmetic?</summary>
To compute $\lceil A / B \rceil$ using positive integers, write `(A + B - 1) / B`.
</details>

<details>
<summary>6. What is the return value of Java's `Arrays.binarySearch()` when an element is missing?</summary>
It returns `-(insertion_point) - 1`, where `insertion_point` is the index where the key would be inserted. This guarantees a negative return value while still communicating the exact insertion point via `(-return_val - 1)`.
</details>

<details>
<summary>7. Why does linear search sometimes beat binary search on small arrays?</summary>
Linear search features perfect sequential memory access (utilizing CPU cache lines) and predictable branch execution. Binary search causes cache misses and CPU pipeline stalls from unpredictable branch jumps.
</details>

<details>
<summary>8. How do you perform binary search on a 2D sorted matrix of size M x N?</summary>
Treat the matrix as a virtual 1D array of length $M \times N$. For index $K$, the matrix row is $K / N$ and column is $K \% N$. Lookup takes $O(\log(M \times N))$.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Search a 2D Matrix (LeetCode 74)</summary>

```java
public class Search2DMatrix {
    public static boolean searchMatrix(int[][] matrix, int target) {
        int m = matrix.length, n = matrix[0].length;
        int low = 0, high = m * n - 1;

        while (low <= high) {
            int mid = low + (high - low) / 2;
            int val = matrix[mid / n][mid % n];
            if (val == target) return true;
            if (val < target) low = mid + 1;
            else high = mid - 1;
        }
        return false;
    }
}
```
</details>

<details>
<summary>Exercise 2: Peak Index in a Mountain Array (LeetCode 852)</summary>

```java
public class PeakIndexMountain {
    public static int peakIndexInMountainArray(int[] arr) {
        int low = 0, high = arr.length - 1;
        while (low < high) {
            int mid = low + (high - low) / 2;
            if (arr[mid] < arr[mid + 1]) low = mid + 1;
            else high = mid;
        }
        return low;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Safe Midpoint**: `low + (high - low) / 2`.
- **Lower Bound**: First index where `arr[i] >= target`.
- **Upper Bound**: First index where `arr[i] > target`.
- **Answer Space**: Binary search across value range $[min, max]$ using monotonic `isValid(mid)` tester.
- **Ceiling Division**: `(A + B - 1) / B`.

---

## 13. Further Reading

- [TopCoder: Binary Search Tutorial](https://www.topcoder.com/thrive/articles/Binary%20Search)
- [Jon Bentley, *Programming Pearls*: Writing Correct Binary Search](https://en.wikipedia.org/wiki/Binary_search_algorithm#Implementation_issues)
"""

write_dsa_note("06_binary-search.md", c06)
