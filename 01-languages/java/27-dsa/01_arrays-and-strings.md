# Arrays and Strings Algorithmic Patterns in Java

> High-performance array mechanics, pointer strategies (Two Pointers, Fast & Slow), Sliding Window algorithms, Prefix Sums, and Kadane's maximum subarray in Java.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Advanced |
| **Prerequisites** | [`27-dsa/00_complexity-analysis.md`](00_complexity-analysis.md), [`08-collections/00_lists.md`](../08-collections/00_lists.md) |
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

Arrays represent the fundamental contiguous memory data structure in computing. Because array elements occupy physically adjacent memory slots, CPU hardware prefetchers can load sequential array elements into Level 1 and Level 2 CPU caches in a single instruction cycle, achieving maximum memory bandwidth.

Strings in Java are immutable wrapper objects around contiguous byte arrays (`byte[]` in Java 9+ Compact Strings). Mastering algorithmic patterns over arrays and strings—such as Two Pointers, Dynamic Sliding Windows, and Prefix Sum accumulators—allows transforming brute-force $O(N^2)$ or $O(N^3)$ algorithms into optimal $O(N)$ linear-time solutions.

---

## 2. Core Concepts

### 2.1 The Core Array & String Algorithmic Patterns

1. **Two Pointers (Converging)**:
   - Left pointer starts at $0$, Right pointer starts at $N - 1$.
   - Pointers move toward each other based on comparison conditions (e.g., sorted two-sum, palindrome checking, water container trapping).
   - Eliminates nested loops from $O(N^2)$ to $O(N)$.

2. **Sliding Window (Fixed & Dynamic)**:
   - Maintains a contiguous subsegment $[L, R]$.
   - Expands right boundary $R$ to satisfy a constraint; contracts left boundary $L$ to restore invariants.
   - Ideal for substring problems, maximum subarray sum of length $K$, longest substring with $K$ distinct characters.

3. **Prefix Sum Accumulators**:
   - Precomputes cumulative sums: $P[i] = P[i-1] + \text{arr}[i]$.
   - Evaluates range sum queries $\sum_{k=i}^j \text{arr}[k] = P[j] - P[i-1]$ in constant $O(1)$ time.

4. **Kadane’s Algorithm**:
   - Solves the Maximum Subarray Sum problem in a single forward pass $O(N)$ time and $O(1)$ space.
   - Core recurrence:
     $$\text{currentMax} = \max(\text{arr}[i], \text{currentMax} + \text{arr}[i])$$

5. **Dutch National Flag (Three-Way Partitioning)**:
   - Partitions an array of 3 distinct values (e.g. 0, 1, 2) in-place in $O(N)$ time and $O(1)$ space using three pointers (`low`, `mid`, `high`).

---

## 3. How It Works Internally

### 3.1 Contiguous Memory & Cache Locality

```
CPU Core & Cache Hierarchy:
+-------------------------------------------------------+
|  CPU Register (< 1 ns)                                |
|  L1 Data Cache (32 KB, ~1 ns)                         |
|  L2 Cache (512 KB, ~3-4 ns)                           |
|  L3 Shared Cache (16-64 MB, ~10-15 ns)                |
+-------------------------------------------------------+
                           ^
                           | 64-Byte Cache Line Burst Fetch!
+--------------------------+----------------------------+
| Main RAM Memory:                                      |
| [arr[0]] [arr[1]] [arr[2]] [arr[3]] ... [arr[15]]     |
| (Contiguous block: 16 ints loaded in a single memory  |
|  transaction!)                                        |
+-------------------------------------------------------+
```

Sequential array traversal allows the CPU to predict access patterns via hardware stride prefetching, operating near theoretical memory bus speeds.

---

## 4. Syntax & Usage

### 4.1 Java Compact Strings (Java 9+)

Java 9 introduced JEP 254 (Compact Strings):
- If all characters in a `String` can be encoded using ISO-8859-1 (Latin-1), the internal storage uses a 1-byte per character `byte[]` array (`coder = 0`).
- If any character requires UTF-16, storage automatically expands to 2-bytes per character `byte[]` (`coder = 1`).
- This halves the memory footprint of Latin-1 strings across enterprise Java applications.

---

## 5. Practical Examples

### 5.1 Pattern 1: Two Pointers — Two Sum in Sorted Array (LeetCode 167)
```java
public class TwoSumSorted {
    public static int[] twoSum(int[] numbers, int target) {
        int left = 0;
        int right = numbers.length - 1;

        while (left < right) {
            int sum = numbers[left] + numbers[right];
            if (sum == target) {
                return new int[]{left + 1, right + 1}; // 1-based index
            } else if (sum < target) {
                left++;
            } else {
                right--;
            }
        }
        return new int[0];
    }
}
```

### 5.2 Pattern 2: Dynamic Sliding Window — Longest Substring Without Repeating Characters (LeetCode 3)
```java
import java.util.Arrays;

public class LongestUniqueSubstring {
    public static int lengthOfLongestSubstring(String s) {
        int[] lastIndex = new int[128];
        Arrays.fill(lastIndex, -1);

        int maxLength = 0;
        int left = 0;

        for (int right = 0; right < s.length(); right++) {
            char c = s.charAt(right);
            if (lastIndex[c] >= left) {
                left = lastIndex[c] + 1; // Slide window past duplicate
            }
            lastIndex[c] = right;
            maxLength = Math.max(maxLength, right - left + 1);
        }
        return maxLength;
    }
}
```

### 5.3 Pattern 3: Prefix Sums — Subarray Sum Equals K (LeetCode 560)
```java
import java.util.HashMap;
import java.util.Map;

public class SubarraySumK {
    public static int subarraySum(int[] nums, int k) {
        int count = 0;
        int currentSum = 0;
        Map<Integer, Integer> prefixSumOccurrences = new HashMap<>();
        prefixSumOccurrences.put(0, 1); // Base case: prefix sum of 0 appears once

        for (int num : nums) {
            currentSum += num;
            if (prefixSumOccurrences.containsKey(currentSum - k)) {
                count += prefixSumOccurrences.get(currentSum - k);
            }
            prefixSumOccurrences.merge(currentSum, 1, Integer::sum);
        }
        return count;
    }
}
```

### 5.4 Pattern 4: Kadane's Algorithm — Maximum Subarray (LeetCode 53)
```java
public class MaxSubarrayKadane {
    public static int maxSubArray(int[] nums) {
        int maxSoFar = nums[0];
        int currentMax = nums[0];

        for (int i = 1; i < nums.length; i++) {
            currentMax = Math.max(nums[i], currentMax + nums[i]);
            maxSoFar = Math.max(maxSoFar, currentMax);
        }
        return maxSoFar;
    }
}
```

### 5.5 Pattern 5: Three-Way Partitioning — Dutch National Flag (LeetCode 75)
```java
public class DutchNationalFlag {
    public static void sortColors(int[] nums) {
        int low = 0;
        int mid = 0;
        int high = nums.length - 1;

        while (mid <= high) {
            if (nums[mid] == 0) {
                swap(nums, low, mid);
                low++;
                mid++;
            } else if (nums[mid] == 1) {
                mid++;
            } else {
                swap(nums, mid, high);
                high--;
            }
        }
    }

    private static void swap(int[] arr, int i, int j) {
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}
```

---


### 5.6 Pattern 6: Two Pointers — 3Sum with Duplicate Pruning (LeetCode 15)
```java
import java.util.*;

public class ThreeSum {
    public static List<List<Integer>> threeSum(int[] nums) {
        Arrays.sort(nums);
        List<List<Integer>> result = new ArrayList<>();

        for (int i = 0; i < nums.length - 2; i++) {
            // Early termination: since array is sorted, positive head cannot sum to 0
            if (nums[i] > 0) break;
            // Skip duplicate outer elements
            if (i > 0 && nums[i] == nums[i - 1]) continue;

            int left = i + 1;
            int right = nums.length - 1;

            while (left < right) {
                int sum = nums[i] + nums[left] + nums[right];
                if (sum == 0) {
                    result.add(List.of(nums[i], nums[left], nums[right]));
                    // Skip duplicates on both inner pointers
                    while (left < right && nums[left] == nums[left + 1]) left++;
                    while (left < right && nums[right] == nums[right - 1]) right--;
                    left++;
                    right--;
                } else if (sum < 0) {
                    left++;
                } else {
                    right--;
                }
            }
        }
        return result;
    }
}
```

### 5.7 Pattern 7: Two Pointers — Trapping Rain Water (LeetCode 42)
```java
public class TrappingRainWater {
    public static int trap(int[] height) {
        if (height == null || height.length == 0) return 0;

        int left = 0, right = height.length - 1;
        int leftMax = 0, rightMax = 0;
        int totalWater = 0;

        while (left < right) {
            if (height[left] < height[right]) {
                if (height[left] >= leftMax) {
                    leftMax = height[left];
                } else {
                    totalWater += leftMax - height[left];
                }
                left++;
            } else {
                if (height[right] >= rightMax) {
                    rightMax = height[right];
                } else {
                    totalWater += rightMax - height[right];
                }
                right--;
            }
        }
        return totalWater;
    }
}
```

## 6. Best Practices

1. **Avoid Substring Allocations in Loops**: In Java 7u6+, `string.substring()` allocates a brand new character array. For parsing, compare characters by index (`charAt()`) or use indices directly.
2. **Use Lookup Tables for ASCII**: An `int[128]` array is significantly faster than `HashSet<Character>` or `HashMap<Character, Integer>` for frequency counting (no hashing, no boxing).
3. **Guard Against Empty Arrays**: Always validate `if (arr == null || arr.length == 0)` at method boundaries.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Off-by-One in Sliding Window** | Window length calculated as `right - left` instead of `right - left + 1` | Remember 0-indexed inclusive bounds formula: $\text{len} = R - L + 1$ |
| **Resetting Window Naively** | Resetting left pointer to $L+1$ on collision yields $O(N^2)$ time | Jump $L$ directly to $\text{lastIndex}[\text{char}] + 1$ |
| **Negative Prefix Sum Lookup** | Modulo arithmetic with negative numbers (`-5 % 3 = -2` in Java) breaks hash keys | Normalize negative remainder: `((rem % k) + k) % k` |
| **Kadane's Initialized to 0** | If array contains all negative numbers, max sum should be negative, but returns 0 | Initialize both `maxSoFar` and `currentMax` to `nums[0]` |

---

## 8. Performance & Security Notes

### 8.1 Buffer Overflow & Bounds Checking
Java enforces runtime array index bounds checks. When JIT compiler optimizes loops, it attempts **Bounds Check Elimination (BCE)**. Writing standard loops (`for (int i = 0; i < arr.length; i++)`) allows C2 JIT to completely eliminate bounds-checking overhead inside loop bodies.

---

## 9. Comparison / When to Use What

| Pattern | Input Precondition | Time Complexity | Space Complexity | Typical Problem Indicators |
| :--- | :--- | :--- | :--- | :--- |
| **Two Pointers** | Sorted array / Linked pairs | $O(N)$ | $O(1)$ | Pair sum, reverse array, palindrome, water container |
| **Sliding Window** | Contiguous subarrays/substrings | $O(N)$ | $O(1)$ or $O(K)$ | Longest/shortest substring with constraints, rolling average |
| **Prefix Sum** | Any array, frequent range queries | $O(N)$ precompute, $O(1)$ query | $O(N)$ | Range sum query, subarray sum equals $K$, equilibrium index |
| **Kadane's** | Any array with positive/negative | $O(N)$ | $O(1)$ | Maximum contiguous subarray sum/product |

---

## 10. Interview Questions

<details>
<summary>1. How does the Sliding Window pattern achieve O(N) time complexity?</summary>
Both the left and right pointers move strictly monotonically forward from $0$ to $N$. Since each element enters the window at most once (via right pointer) and exits at most once (via left pointer), the total operations across the entire algorithm are bounded by $2N \implies O(N)$.
</details>

<details>
<summary>2. Why does Kadane's algorithm fail if initialized to 0 on an array of negative numbers?</summary>
If all elements are negative (e.g. `[-5, -2, -8]`), the correct maximum subarray sum is `-2`. If initialized to 0, the algorithm returns `0`, which is incorrect because an empty subarray is not permitted. Initializing to `nums[0]` handles all-negative inputs correctly.
</details>

<details>
<summary>3. What is Java 9's Compact Strings feature?</summary>
Compact Strings stores characters as a `byte[]` with a 1-byte encoding flag (`coder`). If all characters fit into ISO-8859-1 (Latin-1), it uses 1 byte per char. If UTF-16 is required, it uses 2 bytes per char, saving up to 50% heap memory for ASCII text.
</details>

<details>
<summary>4. How does the Dutch National Flag algorithm work?</summary>
It maintains three pointers: `low` (boundary for 0s), `mid` (current element), and `high` (boundary for 2s). When `arr[mid] == 0`, swap with `low` and increment both. When `arr[mid] == 1`, advance `mid`. When `arr[mid] == 2`, swap with `high` and decrement `high`.
</details>

<details>
<summary>5. How do you find the equilibrium index of an array in O(N) time?</summary>
First compute the total sum of the array. Then iterate through the array maintaining a running `leftSum`. The equilibrium index is where `leftSum == totalSum - leftSum - arr[i]`.
</details>

<details>
<summary>6. Why is an `int[128]` preferred over a `HashMap<Character, Integer>` for character frequencies?</summary>
An `int[128]` array eliminates object allocation, auto-boxing overhead, hashing computation, and pointer dereferencing, providing direct CPU L1 cache lookups.
</details>

<details>
<summary>7. What is Bounds Check Elimination (BCE) in HotSpot JIT?</summary>
BCE is a compiler optimization where HotSpot proves that loop indices will never exceed array bounds (e.g. `0 <= i < arr.length`), allowing it to remove runtime boundary instructions and emit vectorized machine instructions.
</details>

<details>
<summary>8. How do you handle circular array indexing?</summary>
Use modular arithmetic: `(index + offset) % length`. To handle negative offsets safely: `((index + offset) % length + length) % length`.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Container With Most Water (LeetCode 11)</summary>

```java
public class MaxWaterContainer {
    public static int maxArea(int[] height) {
        int max = 0, l = 0, r = height.length - 1;
        while (l < r) {
            int h = Math.min(height[l], height[r]);
            max = Math.max(max, h * (r - l));
            if (height[l] < height[r]) l++;
            else r--;
        }
        return max;
    }
}
```
</details>

<details>
<summary>Exercise 2: Minimum Size Subarray Sum (LeetCode 209)</summary>

```java
public class MinSubArrayLen {
    public static int minSubArrayLen(int target, int[] nums) {
        int minLen = Integer.MAX_VALUE, sum = 0, l = 0;
        for (int r = 0; r < nums.length; r++) {
            sum += nums[r];
            while (sum >= target) {
                minLen = Math.min(minLen, r - l + 1);
                sum -= nums[l++];
            }
        }
        return minLen == Integer.MAX_VALUE ? 0 : minLen;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Two Pointers**: Opposite ends converging $\to$ Sorted pair matching in $O(N)$.
- **Sliding Window**: Moving subsegment $[L, R] \to$ Substring/subarray optimization in $O(N)$.
- **Prefix Sum**: Precalculated cumulative sum $\to$ Constant-time $O(1)$ range queries.
- **Kadane's**: Running maximum sum $\to$ Single pass $O(N)$ maximum contiguous subarray.
- **Cache Friendly**: Sequential array traversal utilizes CPU cache lines and JIT BCE optimization.

---

## 13. Further Reading

- [LeetCode Arrays & Strings Pattern Guide](https://leetcode.com/explore/interview/card/leetcodes-interview-crash-course-data-structures-and-algorithms/)
- [JEP 254: Compact Strings](https://openjdk.org/jeps/254)
- Jon Bentley, *Programming Pearls* (Addison-Wesley)
