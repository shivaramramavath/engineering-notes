# Advanced Dynamic Programming: Bitmask, Tree, Interval, and Digit DP

> Advanced dynamic programming paradigms: subset state spaces via bitmasks, tree decomposition, interval subproblems, and bounded digit automata.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [16_dynamic-programming-1d.md](16_dynamic-programming-1d.md), [17_dynamic-programming-2d.md](17_dynamic-programming-2d.md), [07_trees-binary-and-bst.md](07_trees-binary-and-bst.md) |
| **Time to Read** | 35 minutes |
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

When state spaces exceed simple sequential linear indices $(i, j)$, standard 1D/2D DP is insufficient. Advanced DP paradigms model complex domain topologies:
1. **Bitmask DP**: Encodes subset memberships as integer bitfields ($2^N$ states). Solves NP-hard combinatorial optimization problems (e.g. Traveling Salesperson) for $N \le 20$.
2. **Tree DP**: Propagates state transitions from child subtrees to parent roots using post-order DFS traversals in $O(V)$ time.
3. **Interval DP**: Computes optimal solutions for subarray segments $[i, j]$ by iterating over segment lengths $L = 1 \dots N$.
4. **Digit DP**: Counts integers in range $[L, R]$ satisfying specific digit properties by building numbers digit by digit with a boolean tight bound flag.

---

## 2. Core Concepts

### 1. Bitmask DP Primitives
For a set of $N$ items, an integer bitmask represents subset inclusion:
- Check if item $i$ is in mask: `(mask & (1 << i)) != 0`
- Add item $i$ to mask: `mask | (1 << i)`
- Remove item $i$ from mask: `mask & ~(1 << i)`
- Iterate all submasks of `mask`:
  ```java
  for (int sub = mask; sub > 0; sub = (sub - 1) & mask) { ... }
  ```

### 2. Tree DP (Post-Order Subtree Synthesis)
In a rooted tree, an internal node $u$ cannot compute its state until all child subtrees $v \in \text{children}(u)$ have finalized their states. DFS post-order naturally guarantees this dependency ordering. Each node returns a tuple or array (e.g., `[includeNode, excludeNode]`).

### 3. Interval DP (Length-First Expansion)
Transitions for interval $[i, j]$ depend on sub-intervals of strictly smaller length:
$$dp[i][j] = \min_{i \le k < j} (dp[i][k] + dp[k+1][j] + \text{cost}(i, k, j))$$
Therefore, loops **must** iterate interval length $len$ from $1 \dots N$, then starting index $i$.

### 4. Digit DP (Tight Bound State Machine)
State: `dp[index][tight][sum/count]`
- `index`: Current digit position being placed (from MSD to LSD).
- `tight`: Boolean (1 or 0). If 1, the prefix placed matches the prefix of upper bound $R$, restricting the current digit choice to $[0 \dots \text{digit}(R)]$. If 0, the current digit can freely be any value $[0 \dots 9]$.

---

## 3. How It Works Internally

### Bitmask Memory Compactness & JIT Vectorization
In Java, a bitmask of up to 30 elements fits entirely inside a single 32-bit primitive integer `int`.
```
Mask: 00000000 00000000 00000010 00100101 (Set contains elements {0, 2, 5, 9})
Table: int[][] dp = new int[1 << N][N];
```
HotSpot JIT translates bitwise shifts (`1 << i`) and bitwise ANDs into single-cycle CPU instructions (`shl`, `and`, `popcnt`), executing orders of magnitude faster than `Set<Integer>`.

---

## 4. Syntax & Usage

### Canonical Interval DP Loop Template

```java
public class IntervalDPTemplate {
    public static int solveInterval(int[] nums) {
        int n = nums.length;
        int[][] dp = new int[n][n];

        // 1. Length must be outermost loop!
        for (int len = 1; len <= n; len++) {
            for (int i = 0; i <= n - len; i++) {
                int j = i + len - 1;
                if (len == 1) {
                    dp[i][j] = nums[i]; // Base case
                    continue;
                }

                dp[i][j] = Integer.MAX_VALUE;
                // 2. Iterate split point k
                for (int k = i; k < j; k++) {
                    int cost = dp[i][k] + dp[k + 1][j] + /* merge cost */ 0;
                    dp[i][j] = Math.min(dp[i][j], cost);
                }
            }
        }

        return dp[0][n - 1];
    }
}
```

---

## 5. Practical Examples

### Example 1: Traveling Salesperson Problem (Bitmask DP $O(N^2 2^N)$)

```java
import java.util.Arrays;

public class TSPBitmask {
    public static int tsp(int[][] dist) {
        int n = dist.length;
        int numStates = 1 << n;
        int[][] dp = new int[numStates][n];
        for (int[] row : dp) Arrays.fill(row, 100_000_000);

        // Base case: starting at city 0
        dp[1][0] = 0;

        for (int mask = 1; mask < numStates; mask++) {
            for (int u = 0; u < n; u++) {
                if ((mask & (1 << u)) == 0) continue; // City u not yet visited

                for (int v = 0; v < n; v++) {
                    if ((mask & (1 << v)) != 0) continue; // City v already visited

                    int nextMask = mask | (1 << v);
                    dp[nextMask][v] = Math.min(dp[nextMask][v], dp[mask][u] + dist[u][v]);
                }
            }
        }

        int minCost = Integer.MAX_VALUE;
        int allVisited = numStates - 1;
        for (int u = 1; u < n; u++) {
            minCost = Math.min(minCost, dp[allVisited][u] + dist[u][0]);
        }
        return minCost;
    }
}
```

### Example 2: Burst Balloons (LeetCode 312, Interval DP $O(N^3)$)

```java
public class BurstBalloons {
    public int maxCoins(int[] nums) {
        int n = nums.length;
        // Pad with boundaries 1 at both ends
        int[] arr = new int[n + 2];
        arr[0] = 1;
        arr[n + 1] = 1;
        System.arraycopy(nums, 0, arr, 1, n);

        int[][] dp = new int[n + 2][n + 2];

        // Length of sub-array of balloons to burst
        for (int len = 1; len <= n; len++) {
            for (int left = 1; left <= n - len + 1; left++) {
                int right = left + len - 1;

                // Let balloon 'k' be the LAST balloon burst in interval [left, right]
                for (int k = left; k <= right; k++) {
                    int coins = arr[left - 1] * arr[k] * arr[right + 1]
                              + dp[left][k - 1] + dp[k + 1][right];
                    dp[left][right] = Math.max(dp[left][right], coins);
                }
            }
        }

        return dp[1][n];
    }
}
```

### Example 3: House Robber III (LeetCode 337, Tree DP)

```java
public class HouseRobberIII {
    public static class TreeNode {
        int val;
        TreeNode left;
        TreeNode right;
        TreeNode(int val) { this.val = val; }
    }

    public int rob(TreeNode root) {
        int[] result = dfs(root);
        return Math.max(result[0], result[1]);
    }

    // Returns int[]{robThisNode, notRobThisNode}
    private int[] dfs(TreeNode node) {
        if (node == null) return new int[]{0, 0};

        int[] left = dfs(node.left);
        int[] right = dfs(node.right);

        // If we rob this node, we cannot rob left or right children
        int robThis = node.val + left[1] + right[1];

        // If we do not rob this node, children can either be robbed or not
        int notRobThis = Math.max(left[0], left[1]) + Math.max(right[0], right[1]);

        return new int[]{robThis, notRobThis};
    }
}
```

### Example 4: Number of Digit One (LeetCode 233, Digit DP)

```java
import java.util.Arrays;

public class NumberOfDigitOne {
    private int[][][] memo;
    private char[] digits;

    public int countDigitOne(int n) {
        digits = String.valueOf(n).toCharArray();
        int len = digits.length;
        // memo[index][count][tight]
        memo = new int[len][len + 1][2];
        for (int[][] a : memo) for (int[] b : a) Arrays.fill(b, -1);

        return dp(0, 0, 1);
    }

    private int dp(int index, int count, int tight) {
        if (index == digits.length) return count;
        if (memo[index][count][tight] != -1) return memo[index][count][tight];

        int limit = (tight == 1) ? (digits[index] - '0') : 9;
        int total = 0;

        for (int digit = 0; digit <= limit; digit++) {
            int nextTight = (tight == 1 && digit == limit) ? 1 : 0;
            int nextCount = count + (digit == 1 ? 1 : 0);
            total += dp(index + 1, nextCount, nextTight);
        }

        return memo[index][count][tight] = total;
    }
}
```

### Example 5: Minimum Cost to Merge Stones (LeetCode 1000, Multi-Way Interval DP)

```java
public class MergeStones {
    public int mergeStones(int[] stones, int k) {
        int n = stones.length;
        if ((n - 1) % (k - 1) != 0) return -1; // Impossible to reduce to 1 pile

        int[] prefix = new int[n + 1];
        for (int i = 0; i < n; i++) prefix[i + 1] = prefix[i] + stones[i];

        int[][] dp = new int[n][n];

        for (int len = k; len <= n; len++) {
            for (int i = 0; i <= n - len; i++) {
                int j = i + len - 1;
                dp[i][j] = Integer.MAX_VALUE;

                // Step by k - 1 to preserve pile equivalence
                for (int m = i; m < j; m += k - 1) {
                    dp[i][j] = Math.min(dp[i][j], dp[i][m] + dp[m + 1][j]);
                }

                if ((len - 1) % (k - 1) == 0) {
                    dp[i][j] += prefix[j + 1] - prefix[i];
                }
            }
        }

        return dp[0][n - 1];
    }
}
```

---

## 6. Best Practices

1. **Verify State Bound $N \le 20$ for Bitmask DP**: $2^{20} \approx 10^6$ states, which fits in memory and runtime limits ($10^6 \times 20 \approx 2 \times 10^7$ ops). For $N \ge 25$, bitmask DP is infeasible.
2. **Reverse Thinking in Burst Balloons**: Thinking forward ("burst a balloon and concatenate remaining") alters neighbor relationships unpredictably. Thinking in reverse ("pick the balloon that remains LAST in the interval") keeps boundaries $arr[left-1]$ and $arr[right+1]$ completely static and independent.
3. **Use Memoization with `tight` State in Digit DP**: Always include the `tight` flag in the memo table, or memoize strictly when `tight == 0` since the tight path is traversed only once per query.
4. **Enclose Bit Operations in Parentheses**: The bitwise AND operator `&` has lower operator precedence than comparison `==` (`mask & (1 << i) == 0` is evaluated as `mask & ((1 << i) == 0)`!). Always write: `(mask & (1 << i)) != 0`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Operator precedence bug: `mask & 1 << i != 0` | `!=` takes precedence over `&` | Always enclose: `(mask & (1 << i)) != 0`. |
| Out-of-order interval loop | Subproblems $[i, k]$ and $[k+1, j]$ are not computed yet | Loop interval length `len` outermost from $1 \dots N$. |
| Bit overflow on shifts with `long` | `1 << i` defaults to 32-bit int; overflows for $i \ge 32$ | Use `1L << i` for masks exceeding 31 bits. |
| Forgetting base cases in Tree DP | `NullPointerException` on leaf child traversals | Check `if (node == null) return new int[]{0, 0};`. |

---

## 8. Performance & Security Notes

- **Time Complexity**:
  - Bitmask DP: $O(N^2 \cdot 2^N)$ for TSP, $O(3^N)$ for submask iterations.
  - Interval DP: $O(N^3)$ (nested loops for length, start, and partition point $k$).
  - Tree DP: $O(V)$ (single post-order DFS pass).
  - Digit DP: $O(\text{digits} \times \text{states} \times 10) \approx O(1)$ for fixed integer bounds.
- **Recursion Limits**: Tree DP on degenerate trees (skewed into a linked list) can reach depth $10^5$, risking JVM `StackOverflowError`.

---

## 9. Comparison / When to Use What

| Paradigm | State Key | Input Size Limit | Classic Problem |
|---|---|---|---|
| **Bitmask DP** | `(mask, lastCity)` | $N \le 20$ | Traveling Salesperson, Assignment |
| **Interval DP** | `(left, right)` | $N \le 500$ | Burst Balloons, Matrix Chain Mult |
| **Tree DP** | `node` tuple | $N \le 10^5$ | House Robber III, Tree Diameter |
| **Digit DP** | `(idx, count, tight)` | $N \le 10^{18}$ (value) | Count integers with digit properties |

---

## 10. Interview Questions

<details>
<summary>1. How does iterating through all submasks of a mask run in $O(3^N)$ instead of $O(4^N)$?</summary>

The loop `for (int sub = mask; sub > 0; sub = (sub - 1) & mask)` visits each submask directly without scanning non-submasks. Summing over all masks with $k$ set bits:
$$\sum_{k=0}^N \binom{N}{k} 2^k = (1 + 2)^N = 3^N$$
This runs in $3^N$ operations rather than $2^N \times 2^N = 4^N$.
</details>

<details>
<summary>2. Why does Burst Balloons require defining balloon $k$ as the LAST balloon burst in the interval?</summary>

If balloon $k$ is burst first, the remaining balloons on either side now become adjacent, creating a dependency between the left and right subproblems. If balloon $k$ is the LAST to burst, the boundaries $left-1$ and $right+1$ are guaranteed to remain intact throughout all other bursts in the interval, keeping left and right subproblems completely independent.
</details>

<details>
<summary>3. What is the Knuth-Yao Optimization in Interval DP?</summary>

When the cost function satisfies the quadrangle inequality and monotonicity, the optimal split point $opt[i][j]$ satisfies:
$$opt[i][j-1] \le opt[i][j] \le opt[i+1][j]$$
Restricting the split search $k$ between these bounds reduces time complexity from $O(N^3)$ to $O(N^2)$.
</details>

<details>
<summary>4. How do you handle range queries $[L, R]$ in Digit DP?</summary>

Use prefix subtraction:
$$f([L, R]) = f([0, R]) - f([0, L - 1])$$
Evaluate the digit DP function twice: once for upper bound $R$, and once for $L - 1$.
</details>

<details>
<summary>5. What is Profile DP (Broken Profile / Plug DP)?</summary>

A specialized bitmask DP used for tiling grids (e.g., tiling an $N \times M$ grid with dominoes). Instead of entire rows, the bitmask encodes the contour boundary of the currently processed cells (the "broken profile"), progressing cell by cell in $O(N \cdot M \cdot 2^M)$ time.
</details>

<details>
<summary>6. How can Tree DP be extended to Re-Rooting (All Roots DP)?</summary>

Run two DFS passes:
1. First pass (bottom-up): Compute tree DP answers for all subtrees with root fixed at node 0.
2. Second pass (top-down): Re-root the tree at each neighbor in $O(1)$ by subtracting the neighbor's contribution and adding the parent's modified contribution. Solves all-pairs tree metrics in $O(V)$ time.
</details>

<details>
<summary>7. How do you recognize that a problem requires Bitmask DP?</summary>

1. $N$ is small ($N \le 20$).
2. The problem involves permuting items, visiting all vertices, or partitioning items into disjoint groups.
3. Decisions depend on which subset of items has already been used.
</details>

<details>
<summary>8. How do you avoid `StackOverflowError` during Tree DP on deep trees?</summary>

Either find the centroid to balance the tree, convert the post-order traversal to an iterative loop using Kahn's algorithm (peeling leaves inward via in-degrees), or launch the algorithm on a new thread with an expanded stack size (`new Thread(null, runnable, "worker", 1 << 26).start()`).
</details>

---

## 11. Practice Exercises

### Exercise 1: Binary Tree Maximum Path Sum (LeetCode 124, Tree DP)

<details>
<summary>Solution</summary>

```java
public class BinaryTreeMaxPathSum {
    public static class TreeNode {
        int val;
        TreeNode left, right;
        TreeNode(int val) { this.val = val; }
    }

    private int maxSum = Integer.MIN_VALUE;

    public int maxPathSum(TreeNode root) {
        dfs(root);
        return maxSum;
    }

    private int dfs(TreeNode node) {
        if (node == null) return 0;

        // Ignore negative paths
        int leftGain = Math.max(0, dfs(node.left));
        int rightGain = Math.max(0, dfs(node.right));

        // Path passing through this node as highest root
        maxSum = Math.max(maxSum, node.val + leftGain + rightGain);

        // Return maximum branch gain to parent
        return node.val + Math.max(leftGain, rightGain);
    }
}
```
</details>

### Exercise 2: Can I Win (LeetCode 464, Game Theory Bitmask DP)

<details>
<summary>Solution</summary>

```java
import java.util.HashMap;
import java.util.Map;

public class CanIWin {
    public boolean canIWin(int maxChoosableInteger, int desiredTotal) {
        int sum = (maxChoosableInteger * (maxChoosableInteger + 1)) / 2;
        if (sum < desiredTotal) return false;
        if (desiredTotal <= 0) return true;

        return dfs(maxChoosableInteger, desiredTotal, 0, new HashMap<>());
    }

    private boolean dfs(int maxInt, int total, int mask, Map<Integer, Boolean> memo) {
        if (memo.containsKey(mask)) return memo.get(mask);

        for (int i = 1; i <= maxInt; i++) {
            int bit = 1 << i;
            if ((mask & bit) == 0) {
                // Winning move: picking i reaches target or leaves opponent in losing state
                if (i >= total || !dfs(maxInt, total - i, mask | bit, memo)) {
                    memo.put(mask, true);
                    return true;
                }
            }
        }

        memo.put(mask, false);
        return false;
    }
}
```
</details>

### Exercise 3: Strange Printer (LeetCode 664, Interval DP)

<details>
<summary>Solution</summary>

```java
public class StrangePrinter {
    public int strangePrinter(String s) {
        int n = s.length();
        int[][] dp = new int[n][n];

        for (int len = 1; len <= n; len++) {
            for (int i = 0; i <= n - len; i++) {
                int j = i + len - 1;
                dp[i][j] = (len == 1) ? 1 : dp[i][j - 1] + 1;

                for (int k = i; k < j; k++) {
                    if (s.charAt(k) == s.charAt(j)) {
                        dp[i][j] = Math.min(dp[i][j], dp[i][k] + (k + 1 <= j - 1 ? dp[k + 1][j - 1] : 0));
                    }
                }
            }
        }

        return dp[0][n - 1];
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Bitmask DP**: Integer bitfield models subset state ($N \le 20$). Enclose operations in `(mask & (1 << i)) != 0`.
- **Interval DP**: Loop length $len$ from $1 \dots N$ before left index $i$. Split at index $k$.
- **Tree DP**: Post-order DFS; children synthesize state into parent tuple in $O(V)$ time.
- **Digit DP**: State `(idx, count, tight)` builds valid integer numbers digit by digit.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 15.2: Matrix-Chain Multiplication](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Competitive Programmer's Handbook, Chapter 10: Bit Manipulation & Bitmask DP](https://cses.fi/book/book.pdf)
- [Baeldung: Guide to Bitmasking in Java](https://www.baeldung.com/java-bitmask)
- [Codeforces Tutorial: Digit DP Step by Step](https://codeforces.com/blog/entry/53960)
