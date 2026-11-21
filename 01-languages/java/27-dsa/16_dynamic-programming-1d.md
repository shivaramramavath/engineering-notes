# 1D Dynamic Programming

> Foundational dynamic programming: optimal substructure, overlapping subproblems, memoization vs tabulation, state compression, and patience sorting.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01_arrays-and-strings.md](01_arrays-and-strings.md), [05_recursion-and-backtracking.md](05_recursion-and-backtracking.md) |
| **Time to Read** | 30 minutes |
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

**Dynamic Programming (DP)** is an algorithmic paradigm that solves complex problems by breaking them down into simpler subproblems, solving each subproblem exactly once, and storing their solutions.

A problem can be solved with Dynamic Programming if and only if it exhibits two core mathematical properties:
1. **Overlapping Subproblems**: The same subproblems are solved repeatedly in a naive recursive solution (e.g., computing Fibonacci $F(n-1)$ and $F(n-2)$ both recursively require $F(n-2)$).
2. **Optimal Substructure**: The optimal solution to the problem can be constructed directly from optimal solutions of its subproblems.

### The Problem Solved
Naive recursion often suffers from **exponential time complexity** $O(2^N)$ because the execution tree branches redundantly. Storing subproblem solutions (via memoization or tabulation) drops the time complexity from exponential $O(2^N)$ to **linear or polynomial $O(N)$**.

---

## 2. Core Concepts

### 1. Top-Down (Memoization) vs Bottom-Up (Tabulation)
- **Top-Down with Memoization**: Write the natural recursive relationship, and look up answers in an array or map before computing. If absent, compute and cache.
- **Bottom-Up with Tabulation**: Identify the base cases, and build up solutions iteratively using loops from smallest inputs to largest.

### 2. The 4-Step DP Formulation Framework
1. **Define State**: Clearly define what `dp[i]` represents in plain English (e.g., "The maximum money robbable from houses $0$ through $i$").
2. **Determine Recurrence Relation (Transition)**: Express `dp[i]` as a mathematical formula of preceding states (e.g., `dp[i] = Math.max(dp[i-1], dp[i-2] + nums[i])`).
3. **Identify Base Cases**: Determine initial values where no transitions are needed (e.g., `dp[0] = nums[0]`, `dp[1] = Math.max(nums[0], nums[1])`).
4. **Identify Order of Computation & Optimize Space**: Ensure dependencies are computed before dependents. If `dp[i]` only depends on `dp[i-1]` and `dp[i-2]`, reduce space from $O(N)$ array to two $O(1)$ scalar variables.

---

## 3. How It Works Internally

### Stack Frame Overhead vs Contiguous Loop Processing

Top-Down Memoization:
```
Call Stack:
fib(5) -> fib(4) -> fib(3) -> fib(2) -> fib(1)
Each invocation pushes ~48 bytes HotSpot stack frame.
Deep recursion risks StackOverflowError!
```

Bottom-Up Tabulation:
```
Single Stack Frame (Iterative loop):
int[] dp = new int[N + 1];
Contiguous primitive memory, cache line friendly (64-byte L1 cache prefetching).
Zero GC pressure, strictly linear memory access.
```

Space-Optimized Rolling Variables:
```
[prev2]  [prev1]  ->  [curr = prev1 + prev2]
  |         |
  +-- Shifts forward each iteration
Heap allocation: ZERO. Operates entirely inside CPU registers!
```

---

## 4. Syntax & Usage

### 1D DP State Machine & Rolling Space Pattern

```java
public class DP1DTemplate {
    // Space-optimized O(1) state transitions
    public static int solveSpaceOptimized(int[] nums) {
        if (nums == null || nums.length == 0) return 0;
        if (nums.length == 1) return nums[0];

        int prev2 = nums[0];
        int prev1 = Math.max(nums[0], nums[1]);

        for (int i = 2; i < nums.length; i++) {
            int current = Math.max(prev1, prev2 + nums[i]);
            prev2 = prev1;
            prev1 = current;
        }

        return prev1;
    }
}
```

---

## 5. Practical Examples

### Example 1: Climbing Stairs (LeetCode 70)

```java
public class ClimbingStairs {
    public int climbStairs(int n) {
        if (n <= 2) return n;
        int prev2 = 1;
        int prev1 = 2;

        for (int i = 3; i <= n; i++) {
            int curr = prev1 + prev2;
            prev2 = prev1;
            prev1 = curr;
        }

        return prev1;
    }
}
```

### Example 2: House Robber I & II (LeetCode 198, 213, Circular DP)

```java
public class HouseRobber {
    // House Robber I (Linear Street)
    public int rob(int[] nums) {
        if (nums.length == 1) return nums[0];
        int prev2 = 0;
        int prev1 = 0;

        for (int num : nums) {
            int curr = Math.max(prev1, prev2 + num);
            prev2 = prev1;
            prev1 = curr;
        }
        return prev1;
    }

    // House Robber II (Circular Street: First and Last adjacent)
    public int robCircular(int[] nums) {
        if (nums.length == 1) return nums[0];
        return Math.max(robRange(nums, 0, nums.length - 2),
                        robRange(nums, 1, nums.length - 1));
    }

    private int robRange(int[] nums, int start, int end) {
        int prev2 = 0, prev1 = 0;
        for (int i = start; i <= end; i++) {
            int curr = Math.max(prev1, prev2 + nums[i]);
            prev2 = prev1;
            prev1 = curr;
        }
        return prev1;
    }
}
```

### Example 3: Coin Change (LeetCode 322, Unbounded Min DP)

```java
import java.util.Arrays;

public class CoinChange {
    public int coinChange(int[] coins, int amount) {
        int max = amount + 1;
        int[] dp = new int[amount + 1];
        Arrays.fill(dp, max);
        dp[0] = 0;

        for (int i = 1; i <= amount; i++) {
            for (int coin : coins) {
                if (coin <= i) {
                    dp[i] = Math.min(dp[i], dp[i - coin] + 1);
                }
            }
        }

        return dp[amount] > amount ? -1 : dp[amount];
    }
}
```

### Example 4: Longest Increasing Subsequence (LeetCode 300, $O(N \log N)$ Patience Sort)

```java
import java.util.Arrays;

public class LongestIncreasingSubsequence {
    // Standard O(N^2) Tabulation
    public int lengthOfLIS_DP(int[] nums) {
        int n = nums.length;
        int[] dp = new int[n];
        Arrays.fill(dp, 1);
        int maxLen = 1;

        for (int i = 1; i < n; i++) {
            for (int j = 0; j < i; j++) {
                if (nums[i] > nums[j]) {
                    dp[i] = Math.max(dp[i], dp[j] + 1);
                }
            }
            maxLen = Math.max(maxLen, dp[i]);
        }
        return maxLen;
    }

    // Optimal O(N log N) Patience Sorting with Binary Search
    public int lengthOfLIS_BinarySearch(int[] nums) {
        int[] tails = new int[nums.length];
        int size = 0;

        for (int x : nums) {
            int i = 0, j = size;
            while (i != j) {
                int mid = (i + j) >>> 1;
                if (tails[mid] < x) {
                    i = mid + 1;
                } else {
                    j = mid;
                }
            }
            tails[i] = x;
            if (i == size) size++;
        }

        return size;
    }
}
```

### Example 5: Decode Ways (LeetCode 91)

```java
public class DecodeWays {
    public int numDecodings(String s) {
        if (s == null || s.isEmpty() || s.charAt(0) == '0') return 0;
        int n = s.length();

        int prev2 = 1; // dp[i-2]
        int prev1 = 1; // dp[i-1]

        for (int i = 1; i < n; i++) {
            int curr = 0;
            int singleDigit = s.charAt(i) - '0';
            int twoDigits = Integer.parseInt(s.substring(i - 1, i + 1));

            if (singleDigit != 0) {
                curr += prev1;
            }
            if (twoDigits >= 10 && twoDigits <= 26) {
                curr += prev2;
            }

            prev2 = prev1;
            prev1 = curr;
        }

        return prev1;
    }
}
```

---

## 6. Best Practices

1. **Always Check for $O(1)$ Space Reductions**: If the transition for `dp[i]` references only a fixed window of preceding states (e.g., `dp[i-1]` and `dp[i-2]`), discard the $O(N)$ array and maintain scalar rolling variables.
2. **Sentinel Initialization for Min/Max**: When computing minimum costs, initialize with `amount + 1` or `1e9` rather than `Integer.MAX_VALUE` to avoid arithmetic wrap-around.
3. **Patience Sorting for LIS**: When finding Longest Increasing Subsequences, always choose the $O(N \log N)$ patience sorting technique over $O(N^2)$ DP for arrays with $N > 1,000$.
4. **Circular Constraint Elimination**: For circular DP (e.g., House Robber II), break the circle into two overlapping linear subproblems: $[0, N-2]$ and $[1, N-1]$.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Initializing memo with 0 when 0 is a valid result | Triggers infinite redundant re-computation | Fill memo array with `-1` using `Arrays.fill(memo, -1)`. |
| `Integer.MAX_VALUE + 1` overflow | Wraps around to `Integer.MIN_VALUE` | Use sentinel `amount + 1` or `1_000_000_000`. |
| Out of bounds on base cases | Indexing `dp[1]` when input array has length 1 | Add explicit guard `if (nums.length == 1) return nums[0];`. |
| Confusing Subsequence vs Subarray | Subarrays must be contiguous; subsequences can skip elements | Subarray problems often use Kadane's algorithm ($O(N)$). |

---

## 8. Performance & Security Notes

- **Time Complexity**: Typically $O(N)$ for linear transitions; $O(N^2)$ for nested comparisons (like naive LIS); $O(N \log N)$ with binary search optimization.
- **Space Complexity**: Reduced from $O(N)$ to $O(1)$ using rolling state variables.
- **Recursion Depth Limits**: Recursive memoization on inputs $N > 10,000$ will throw `StackOverflowError` on the standard HotSpot JVM default stack size (1MB). Use bottom-up tabulation for large $N$.

---

## 9. Comparison / When to Use What

| Technique | Time Complexity | Space Complexity | Stack Overhead | Ease of Formulation |
|---|---|---|---|---|
| **Memoization (Top-Down)** | $O(N)$ | $O(N)$ | Yes ($O(N)$ frames) | Natural (matches recursion) |
| **Tabulation (Bottom-Up)** | $O(N)$ | $O(N)$ | None ($O(1)$ frames) | Requires loop ordering |
| **Space-Optimized DP** | $O(N)$ | $O(1)$ | None | Best production standard |

---

## 10. Interview Questions

<details>
<summary>1. How do you recognize if a problem can be solved with Dynamic Programming?</summary>

Look for two hallmarks:
1. The problem asks for an optimum (minimum cost, maximum profit, longest length, shortest path) or counts total ways.
2. The decision at the current step depends on optimal decisions of smaller subproblems, and the same subproblem states recur repeatedly across decision paths.
</details>

<details>
<summary>2. Why does Patience Sorting find the LIS length in $O(N \log N)$ time?</summary>

Patience Sorting maintains an array `tails` where `tails[i]` is the smallest tail element of all increasing subsequences of length `i + 1`. Because `tails` is strictly sorted ascending, we can use binary search (`Arrays.binarySearch` or custom) to locate the insertion slot in $O(\log N)$ for each element, achieving $O(N \log N)$ total time.
</details>

<details>
<summary>3. Can dynamic programming always be space-optimized to $O(1)$?</summary>

No. Space optimization to $O(1)$ is only possible when state $i$ depends on a constant number $K$ of immediately preceding states (e.g., $i-1, i-2$). If state $i$ depends on all previous states $0 \dots i-1$ (as in LIS or Coin Change), an array of size $O(N)$ is strictly required.
</details>

<details>
<summary>4. What is Kadane's Algorithm and how does it relate to 1D DP?</summary>

Kadane's algorithm solves Maximum Subarray Sum in $O(N)$ time and $O(1)$ space. The state definition is:
`dp[i] = Math.max(nums[i], dp[i-1] + nums[i])`.
Because it only references `dp[i-1]`, it is maintained in a single scalar variable `currentMax`.
</details>

<details>
<summary>5. What is the difference between Unbounded Knapsack and 0/1 Knapsack in 1D DP?</summary>

In Unbounded Knapsack (like Coin Change), an item can be reused infinitely; loop the capacity array forwards from $w \to W$. In 0/1 Knapsack, an item can be used at most once; loop the capacity array backwards from $W \to w$ to prevent using the same item twice in the same round.
</details>

<details>
<summary>6. How do you recover the actual path or elements chosen, not just the optimal value?</summary>

Either backtrack through the tabulated DP array comparing values (`if (dp[i] == dp[i - w] + val)`), or store a parallel `parent[]` array recording the transition decision made at each state.
</details>

<details>
<summary>7. What happens when transitions contain cycles (e.g. $A \to B \to A$)?</summary>

Dynamic Programming requires a DAG of dependencies. If transitions contain circular dependencies, standard DP breaks. In such cases, use shortest path algorithms (Dijkstra or Bellman-Ford) or linear equation solvers (Gaussian Elimination).
</details>

<details>
<summary>8. How do you solve the Paint House problem with $K$ colors in $O(N \cdot K)$ time?</summary>

Track only the minimum and second-minimum costs from the previous house, along with the color index of the minimum. For house $i$ and color $c$, if $c$ matches the previous minimum color, add the second-minimum; otherwise add the minimum. This avoids testing all $K$ colors, reducing transition time from $O(K^2)$ to $O(K)$.
</details>

---

## 11. Practice Exercises

### Exercise 1: Word Break (LeetCode 139)
Determine if a string can be segmented into space-separated dictionary words.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class WordBreak {
    public boolean wordBreak(String s, List<String> wordDict) {
        Set<String> dict = new HashSet<>(wordDict);
        int n = s.length();
        boolean[] dp = new boolean[n + 1];
        dp[0] = true;

        for (int i = 1; i <= n; i++) {
            for (int j = 0; j < i; j++) {
                if (dp[j] && dict.contains(s.substring(j, i))) {
                    dp[i] = true;
                    break;
                }
            }
        }

        return dp[n];
    }
}
```
</details>

### Exercise 2: Partition Equal Subset Sum (LeetCode 416, 0/1 Knapsack in 1D)

<details>
<summary>Solution</summary>

```java
public class PartitionEqualSubsetSum {
    public boolean canPartition(int[] nums) {
        int sum = 0;
        for (int n : nums) sum += n;
        if ((sum & 1) == 1) return false; // Odd sum cannot be partitioned

        int target = sum / 2;
        boolean[] dp = new boolean[target + 1];
        dp[0] = true;

        for (int num : nums) {
            // Loop backwards to ensure each number is used at most once
            for (int j = target; j >= num; j--) {
                dp[j] = dp[j] || dp[j - num];
            }
        }

        return dp[target];
    }
}
```
</details>

### Exercise 3: Maximum Subarray Sum (LeetCode 53, Kadane's Algorithm)

<details>
<summary>Solution</summary>

```java
public class MaximumSubarray {
    public int maxSubArray(int[] nums) {
        int currentMax = nums[0];
        int globalMax = nums[0];

        for (int i = 1; i < nums.length; i++) {
            currentMax = Math.max(nums[i], currentMax + nums[i]);
            globalMax = Math.max(globalMax, currentMax);
        }

        return globalMax;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Requisites**: Optimal substructure and overlapping subproblems.
- **Top-Down vs Bottom-Up**: Memoization recurses; tabulation loops. Prefer tabulation in Java for zero stack overhead.
- **Space Compression**: If state $i$ depends only on $i-1$ and $i-2$, reduce space to $O(1)$ variables.
- **0/1 Knapsack 1D**: Loop backwards `for (int j = target; j >= weight; j--)` to prevent duplicate item usage.
- **Patience Sorting**: Solves LIS in $O(N \log N)$ using binary search.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 15: Dynamic Programming](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Richard Bellman: Dynamic Programming (1957)](https://press.princeton.edu/books/paperback/9780691146683/dynamic-programming)
- [Baeldung: Introduction to Dynamic Programming in Java](https://www.baeldung.com/cs/dynamic-programming-intro)
- [LeetCode Dynamic Programming Study Plan](https://leetcode.com/studyplan/dynamic-programming/)
