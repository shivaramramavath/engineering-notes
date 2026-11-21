# 2D Dynamic Programming

> Multi-dimensional dynamic programming: grid paths, sequence alignments (LCS, Edit Distance), 0/1 knapsack variants, and cache-friendly space compression.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [16_dynamic-programming-1d.md](16_dynamic-programming-1d.md) |
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

**2D Dynamic Programming** models state transitions across two interacting dimensions:
1. **Grid Paths**: Coordinates $(r, c)$ in a 2D matrix.
2. **Two Sequences / Strings**: Indices $i$ in string $A$ and $j$ in string $B$ (e.g., Longest Common Subsequence, Edit Distance).
3. **Item Subsets and Capacities**: $i$ available items and $w$ remaining capacity in Knapsack problems.
4. **Interval Ranges**: Boundaries $[i, j]$ spanning a subsegment of an array (Interval DP).

### The Problem Solved
Comparing all sub-sequences between two strings of lengths $M$ and $N$ requires exploring $2^M \times 2^N$ combinations. 2D DP decomposes this into a grid of overlapping subproblems, reducing exponential time to $O(M \times N)$ quadratic time.

---

## 2. Core Concepts

### 1. The 2D State Matrix
The entry `dp[i][j]` represents the optimal solution to the subproblem defined by prefixes $A[0 \dots i-1]$ and $B[0 \dots j-1]$. Sizing the table $(M+1) \times (N+1)$ naturally accommodates empty string base cases at index 0.

### 2. Transition Geometries
Most 2D DP problems draw their values from three immediate neighbors:
```
+---------------+---------------+
| dp[i-1][j-1]  | dp[i-1][j]    |  <-- Up / Delete in A
| (Match/Sub)   |               |
+---------------+---------------+
| dp[i][j-1]    | dp[i][j]      |  <-- Target State
| Insert in A   | (Current)     |
+---------------+---------------+
```

### 3. State Compression ($O(M \times N) \to O(N)$)
Notice that computing row $i$ references strictly row $i$ and row $i-1$. Therefore, storing the full $M \times N$ matrix in heap memory is unnecessary. We can maintain just **two rolling rows** (`prev[]` and `curr[]`), or a **single row updated in place** with a scalar variable tracking the diagonal `dp[i-1][j-1]`.

---

## 3. How It Works Internally

### JVM Memory Overhead: Jagged Arrays vs Flat Arrays
In HotSpot JVM, a 2D array `int[M][N]` is **not a contiguous matrix**. It is an array of $M$ independent object references, each pointing to a separate 1D array of length $N$:
```
int[][] dp = new int[M][N]:
dp reference (4/8 bytes)
  -> int[] array of M references (24 + 4*M bytes)
      -> row 0: int[N] (16 + 4*N bytes)
      -> row 1: int[N] (16 + 4*N bytes)
      ...
```
For $M = 1000, N = 1000$:
- 1,001 heap allocations.
- Severe CPU cache misses due to pointer chasing across heap memory.

**Optimized Alternatives**:
1. **Rolling 1D Rows**: `int[] prev = new int[N]`, `int[] curr = new int[N]`. Total allocation: 2 objects ($8N$ bytes).
2. **Flattened 1D Array**: `int[] dp = new int[M * N]`, indexed as `dp[i * N + j]`. Contiguous memory, zero pointer hopping.

---

## 4. Syntax & Usage

### 2D String Alignment Template with Rolling Space

```java
public class DP2DTemplate {
    public static int solveLCS(String text1, String text2) {
        int m = text1.length();
        int n = text2.length();

        // Space-optimized to 1D rolling array
        int[] dp = new int[n + 1];

        for (int i = 1; i <= m; i++) {
            int prevDiagonal = 0; // Represents dp[i-1][j-1]
            for (int j = 1; j <= n; j++) {
                int temp = dp[j]; // Save dp[i-1][j] before overwriting
                if (text1.charAt(i - 1) == text2.charAt(j - 1)) {
                    dp[j] = prevDiagonal + 1;
                } else {
                    dp[j] = Math.max(dp[j], dp[j - 1]);
                }
                prevDiagonal = temp;
            }
        }

        return dp[n];
    }
}
```

---

## 5. Practical Examples

### Example 1: Unique Paths I & II (LeetCode 62, 63, Grid Paths)

```java
public class UniquePaths {
    // Unique Paths I: Number of ways from top-left to bottom-right
    public int uniquePaths(int m, int n) {
        int[] dp = new int[n];
        java.util.Arrays.fill(dp, 1);

        for (int r = 1; r < m; r++) {
            for (int c = 1; c < n; c++) {
                dp[c] += dp[c - 1]; // dp[c] was previous row, dp[c-1] is current row left
            }
        }
        return dp[n - 1];
    }

    // Unique Paths II: Grid with Obstacles
    public int uniquePathsWithObstacles(int[][] obstacleGrid) {
        int n = obstacleGrid[0].length;
        int[] dp = new int[n];
        dp[0] = obstacleGrid[0][0] == 0 ? 1 : 0;

        for (int[] row : obstacleGrid) {
            for (int c = 0; c < n; c++) {
                if (row[c] == 1) {
                    dp[c] = 0; // Obstacle blocks path
                } else if (c > 0) {
                    dp[c] += dp[c - 1];
                }
            }
        }
        return dp[n - 1];
    }
}
```

### Example 2: Longest Common Subsequence (LeetCode 1143)

```java
public class LongestCommonSubsequence {
    public int longestCommonSubsequence(String text1, String text2) {
        int m = text1.length(), n = text2.length();
        int[][] dp = new int[m + 1][n + 1];

        for (int i = 1; i <= m; i++) {
            char c1 = text1.charAt(i - 1);
            for (int j = 1; j <= n; j++) {
                char c2 = text2.charAt(j - 1);
                if (c1 == c2) {
                    dp[i][j] = dp[i - 1][j - 1] + 1;
                } else {
                    dp[i][j] = Math.max(dp[i - 1][j], dp[i][j - 1]);
                }
            }
        }

        return dp[m][n];
    }
}
```

### Example 3: Edit Distance (LeetCode 72, Levenshtein Distance)

```java
public class EditDistance {
    public int minDistance(String word1, String word2) {
        int m = word1.length(), n = word2.length();
        int[] dp = new int[n + 1];

        // Base case: transforming "" to word2 prefix requires j insertions
        for (int j = 0; j <= n; j++) dp[j] = j;

        for (int i = 1; i <= m; i++) {
            int prevDiag = dp[0];
            dp[0] = i; // Deleting all i characters of word1

            for (int j = 1; j <= n; j++) {
                int temp = dp[j];
                if (word1.charAt(i - 1) == word2.charAt(j - 1)) {
                    dp[j] = prevDiag; // Free match
                } else {
                    // Min of: Replace (prevDiag), Insert (dp[j-1]), Delete (dp[j])
                    dp[j] = 1 + Math.min(prevDiag, Math.min(dp[j], dp[j - 1]));
                }
                prevDiag = temp;
            }
        }

        return dp[n];
    }
}
```

### Example 4: Maximal Square (LeetCode 221)

```java
public class MaximalSquare {
    public int maximalSquare(char[][] matrix) {
        if (matrix == null || matrix.length == 0) return 0;
        int m = matrix.length, n = matrix[0].length;
        int[] dp = new int[n + 1];
        int maxSide = 0;
        int prevDiag = 0;

        for (int r = 1; r <= m; r++) {
            for (int c = 1; c <= n; c++) {
                int temp = dp[c];
                if (matrix[r - 1][c - 1] == '1') {
                    // Smallest neighbor square dictates expansion boundary
                    dp[c] = 1 + Math.min(prevDiag, Math.min(dp[c], dp[c - 1]));
                    maxSide = Math.max(maxSide, dp[c]);
                } else {
                    dp[c] = 0;
                }
                prevDiag = temp;
            }
        }

        return maxSide * maxSide;
    }
}
```

### Example 5: Target Sum (LeetCode 494, 0/1 Knapsack)

```java
public class TargetSum {
    public int findTargetSumWays(int[] nums, int target) {
        int totalSum = 0;
        for (int n : nums) totalSum += n;

        // P - N = target, P + N = totalSum => 2P = target + totalSum
        if (Math.abs(target) > totalSum || ((target + totalSum) & 1) == 1) {
            return 0;
        }

        int subsetSum = (target + totalSum) / 2;
        int[] dp = new int[subsetSum + 1];
        dp[0] = 1;

        for (int num : nums) {
            for (int j = subsetSum; j >= num; j--) {
                dp[j] += dp[j - num];
            }
        }

        return dp[subsetSum];
    }
}
```

---

## 6. Best Practices

1. **Size Arrays by $+1$**: Size matrices `(M + 1) * (N + 1)` so that index 0 cleanly models base states (empty prefixes, zero items), avoiding clumsy negative bounds checks.
2. **Compress Space to 1D**: 95% of 2D DP problems only reference the current row and immediately prior row. Compress space to $O(N)$ with rolling arrays.
3. **Loop Outer Dimension Smaller**: If compressing space to $O(\min(M, N))$, orient the outer loop over the longer sequence and inner loop over the shorter one to minimize memory footprint.
4. **Early Exit for Feasibility Checks**: If the problem is purely boolean reachability (like Interleaving String), break out of internal transitions immediately upon true.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Index off-by-one: `charAt(i)` instead of `charAt(i - 1)` | `dp` table is 1-indexed, string is 0-indexed | Always map $dp[i]$ to $s.charAt(i - 1)$. |
| Overwriting `prevDiag` too early in 1D space compression | Destroys $dp[i-1][j-1]$ needed for diagonal lookup | Save `int temp = dp[j];` before assigning, then update `prevDiag = temp;`. |
| Forward looping in 1D 0/1 knapsack | Reuses the same item multiple times | Loop capacity backwards: `for (int j = cap; j >= weight; j--)`. |
| Out of memory on huge 2D matrices | `new int[50000][50000]` attempts 10GB allocation | Use $O(N)$ rolling array or hash map for sparse states. |

---

## 8. Performance & Security Notes

- **Time Complexity**: $O(M \times N)$ where $M$ and $N$ are the state bounds.
- **Space Complexity**: $O(M \times N)$ uncompressed; $O(\min(M, N))$ compressed.
- **Cache Optimization**: In Java, nested loop order matters significantly:
  ```java
  // Fast: iterates row-wise across contiguous primitives
  for (int r = 0; r < m; r++)
      for (int c = 0; c < n; c++)
          dp[r][c] ...

  // Slow: jumps across separate heap array pointers on every step
  for (int c = 0; c < n; c++)
      for (int r = 0; r < m; r++)
          dp[r][c] ...
  ```

---

## 9. Comparison / When to Use What

| Problem Class | Typical State | Recurrence | Complexity |
|---|---|---|---|
| **Grid Paths** | `dp[r][c]` | `dp[r-1][c] + dp[r][c-1]` | $O(M \times N)$ time, $O(N)$ space |
| **LCS Alignment** | `dp[i][j]` | Match: `diag + 1`; Mismatch: $\max(\text{up}, \text{left})$ | $O(M \times N)$ time, $O(N)$ space |
| **Edit Distance** | `dp[i][j]` | Match: `diag`; Mismatch: $1 + \min(\text{diag}, \text{up}, \text{left})$ | $O(M \times N)$ time, $O(N)$ space |
| **0/1 Knapsack** | `dp[i][w]` | $\max(dp[i-1][w], dp[i-1][w - w_i] + v_i)$ | $O(N \times W)$ time, $O(W)$ space |

---

## 10. Interview Questions

<details>
<summary>1. How do you reconstruct the actual Longest Common Subsequence, not just its length?</summary>

Maintain the full $(M+1) \times (N+1)$ table. Start at $(M, N)$ and backtrack:
- If $A[i-1] == B[j-1]$, prepend $A[i-1]$ to the result and step to $(i-1, j-1)$.
- Else if $dp[i-1][j] \ge dp[i][j-1]$, step up to $(i-1, j)$.
- Else step left to $(i, j-1)$.
Stop when $i == 0$ or $j == 0$.
</details>

<details>
<summary>2. Why does 0/1 Knapsack require looping backwards when space is compressed to 1D?</summary>

Because `dp[j]` depends on `dp[j - weight]` from the **previous row**. If we loop forwards, `dp[j - weight]` would have already been overwritten with the current row's value, allowing the current item to be selected multiple times. Looping backwards ensures `dp[j - weight]` still contains the value from the previous row.
</details>

<details>
<summary>3. What is the Hirschberg Algorithm and why is it important?</summary>

Hirschberg's algorithm combines divide-and-conquer with dynamic programming to find the exact sequence alignment in $O(M \times N)$ time and strictly **$O(\min(M, N))$ space**. It enables genomic DNA alignment on gigabyte-scale sequences where allocating a 2D matrix is impossible.
</details>

<details>
<summary>4. What is the relation between LCS and Shortest Common Supersequence (SCS)?</summary>

The length of the Shortest Common Supersequence of strings $A$ and $B$ is given by:
$\text{Len}(SCS) = |A| + |B| - \text{Len}(LCS(A, B))$.
Every character in the LCS is shared, so it appears once; characters unique to $A$ or $B$ appear once.
</details>

<details>
<summary>5. How does the space optimization of Maximal Square work?</summary>

`dp[r][c]` depends on `dp[r-1][c]`, `dp[r][c-1]`, and `dp[r-1][c-1]`. In a 1D array `dp[c]`, before updating index `c`, `dp[c]` holds the value from row $r-1$, `dp[c-1]` holds the newly updated value from row $r$, and a scalar variable `prevDiag` preserves the diagonal from $r-1, c-1$.
</details>

<details>
<summary>6. How can 2D DP be used to solve Game Theory problems (e.g., Stone Game)?</summary>

State `dp[i][j]` represents the maximum net score a player can achieve from remaining pile range $[i \dots j]$. The recurrence is:
`dp[i][j] = Math.max(piles[i] - dp[i+1][j], piles[j] - dp[i][j-1])`.
Base cases are $dp[i][i] = \text{piles}[i]$.
</details>

<details>
<summary>7. What is Pseudo-Polynomial Time in 0/1 Knapsack?</summary>

0/1 Knapsack runs in $O(N \times W)$ time where $W$ is the maximum capacity. Although it appears polynomial, $W$ is an integer value whose input size is $\log_2(W)$ bits. Thus the complexity is exponential in the input length of $W$, classifying it as pseudo-polynomial.
</details>

<details>
<summary>8. How do you optimize 2D DP when state transitions require range queries?</summary>

Use Range Maximum/Sum data structures (Fenwick Tree, Segment Tree, or Monotonic Deque) to compute the transition in $O(1)$ or $O(\log N)$ rather than scanning $O(N)$ predecessor states.
</details>

---

## 11. Practice Exercises

### Exercise 1: Interleaving String (LeetCode 97)
Determine if string `s3` is formed by an interleaving of `s1` and `s2`.

<details>
<summary>Solution</summary>

```java
public class InterleavingString {
    public boolean isInterleave(String s1, String s2, String s3) {
        if (s1.length() + s2.length() != s3.length()) return false;
        int m = s1.length(), n = s2.length();
        boolean[] dp = new boolean[n + 1];
        dp[0] = true;

        for (int j = 1; j <= n; j++) {
            dp[j] = dp[j - 1] && s2.charAt(j - 1) == s3.charAt(j - 1);
        }

        for (int i = 1; i <= m; i++) {
            dp[0] = dp[0] && s1.charAt(i - 1) == s3.charAt(i - 1);
            for (int j = 1; j <= n; j++) {
                char c3 = s3.charAt(i + j - 1);
                dp[j] = (dp[j] && s1.charAt(i - 1) == c3)
                     || (dp[j - 1] && s2.charAt(j - 1) == c3);
            }
        }

        return dp[n];
    }
}
```
</details>

### Exercise 2: Wildcard Matching (LeetCode 44, '?' and '*')

<details>
<summary>Solution</summary>

```java
public class WildcardMatching {
    public boolean isMatch(String s, String p) {
        int m = s.length(), n = p.length();
        boolean[] dp = new boolean[n + 1];
        dp[0] = true;

        // '*' can match empty prefix
        for (int j = 1; j <= n; j++) {
            if (p.charAt(j - 1) == '*') {
                dp[j] = dp[j - 1];
            }
        }

        for (int i = 1; i <= m; i++) {
            boolean prevDiag = dp[0];
            dp[0] = false;

            for (int j = 1; j <= n; j++) {
                boolean temp = dp[j];
                char pc = p.charAt(j - 1);

                if (pc == '*') {
                    // '*' matches empty (dp[j-1]) or matches 1+ characters (dp[j])
                    dp[j] = dp[j - 1] || dp[j];
                } else if (pc == '?' || pc == s.charAt(i - 1)) {
                    dp[j] = prevDiag;
                } else {
                    dp[j] = false;
                }

                prevDiag = temp;
            }
        }

        return dp[n];
    }
}
```
</details>

### Exercise 3: Regular Expression Matching (LeetCode 10, '.' and 'c*')

<details>
<summary>Solution</summary>

```java
public class RegularExpressionMatching {
    public boolean isMatch(String s, String p) {
        int m = s.length(), n = p.length();
        boolean[][] dp = new boolean[m + 1][n + 1];
        dp[0][0] = true;

        for (int j = 2; j <= n; j += 2) {
            if (p.charAt(j - 1) == '*') {
                dp[0][j] = dp[0][j - 2];
            }
        }

        for (int i = 1; i <= m; i++) {
            for (int j = 1; j <= n; j++) {
                char pc = p.charAt(j - 1);
                if (pc == '*') {
                    char prevPatternChar = p.charAt(j - 2);
                    // Match 0 times
                    dp[i][j] = dp[i][j - 2];
                    // Match 1+ times if pattern char matches current s char
                    if (prevPatternChar == '.' || prevPatternChar == s.charAt(i - 1)) {
                        dp[i][j] = dp[i][j] || dp[i - 1][j];
                    }
                } else if (pc == '.' || pc == s.charAt(i - 1)) {
                    dp[i][j] = dp[i - 1][j - 1];
                }
            }
        }

        return dp[m][n];
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Table Sizing**: Always size $(M + 1) \times (N + 1)$ to model empty prefixes cleanly.
- **Space Reduction**: Compress to 1D rolling array using `prevDiag` variable for diagonal transitions.
- **Knapsack 0/1 Iteration**: Backwards loop prevents duplicate element selection.
- **Row-Major Traversal**: Always iterate outer row, inner column for hardware cache efficiency.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 15.4: Longest Common Subsequence](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Hirschberg, D. S. (1975). A linear space algorithm for computing maximal common subsequences](https://dl.acm.org/doi/10.1145/360825.360861)
- [Baeldung: Levenshtein Distance in Java](https://www.baeldung.com/java-levenshtein-distance)
- [LeetCode 2D Dynamic Programming Card](https://leetcode.com/tag/dynamic-programming/)
