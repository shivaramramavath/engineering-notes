# Dynamic Programming & Optimization in JavaScript

> A production-grade algorithmic guide to Dynamic Programming in JavaScript, featuring Memoization, Tabulation, 1D/2D State Transitions, Space Compression, and Text Diffing Engines.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [12_recursion-and-backtracking.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/12_recursion-and-backtracking.md) |
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

In computational optimization, problems often feature extensive redundancy when solved using naive recursion. For example, evaluating the 40th Fibonacci number naively calls `fib(2)` over $63,000,000$ times, consuming exponential $O(2^N)$ time.

**Dynamic Programming (DP)** is an optimization paradigm that solves complex problems by breaking them down into simpler subproblems, solving each subproblem once, and storing their solutions to avoid redundant re-computation.
- **Complexity Collapse**: Reduces exponential $O(2^N)$ brute-force solutions to polynomial $O(N)$ or $O(N \times M)$ runtime.
- **Optimal Substructure**: The optimal solution to the overall problem incorporates optimal solutions to its constituent subproblems.
- **Ubiquitous Applications**: Used in Git diffing (LCS), text autocorrect/spellcheck (Levenshtein distance), routing networks, and financial portfolio optimization (Knapsack).

---

## 2. Core Concepts

### The Two Prerequisites for Dynamic Programming
1. **Overlapping Subproblems**: The recursive decomposition repeatedly encounters the identical subproblems with the exact same parameters.
2. **Optimal Substructure**: An optimal solution to the problem can be constructed from optimal solutions of its subproblems.

### Top-Down (Memoization) vs. Bottom-Up (Tabulation)
- **Top-Down with Memoization**: Natural recursive approach. Follows standard recursive logic, storing computed subproblem outputs in an auxiliary cache (Map or Array) before returning.
- **Bottom-Up with Tabulation**: Iterative approach. Eliminates recursion call stack overhead by starting from base cases and filling a table (`dp[]` array) iteratively forward toward the final target state.

```
Top-Down (Recursive Tree with Cache):
                 fib(5)
                /      \
           fib(4)      fib(3) ──► (Cache Hit! Returned in O(1))
          /      \
      fib(3)    fib(2)

Bottom-Up (Iterative Table):
Index:  0    1    2    3    4    5
dp[]: [ 0 ][ 1 ][ 1 ][ 2 ][ 3 ][ 5 ]  ──► Loop computes left to right
```

### Space Compression
When a recurrence relation depends only on the immediate prior states (e.g. `dp[i] = dp[i-1] + dp[i-2]`), maintaining the entire $N$-element array is unnecessary. Storing only two scalar variables (`prev1`, `prev2`) reduces auxiliary space from **$O(N)$ down to $O(1)$**.

---

## 3. How It Works Internally

In bottom-up 2D DP (e.g. Longest Common Subsequence or Knapsack), the table is populated in **Row-Major Order**:

```
Grid State Transition: dp[i][j]
                 dp[i-1][j] (from top)
                     │
dp[i][j-1] ──────► dp[i][j]
 (from left)         ▲
                     │
             dp[i-1][j-1] (diagonal match)
```

Because each cell `dp[i][j]` depends only on the current row and the immediately preceding row `i-1`, the $M \times N$ matrix can be compressed into a single 1D array of size $N$ updated in reverse, shrinking space from $O(M \times N)$ to $O(N)$.

---

## 4. Syntax & API Reference

### Dynamic Programming Blueprints
```javascript
// 1. Top-Down Memoization Blueprint
export function solveTopDown(n, memo = new Map()) {
  if (n <= 1) return n; // Base case
  if (memo.has(n)) return memo.get(n);

  const result = solveTopDown(n - 1, memo) + solveTopDown(n - 2, memo);
  memo.set(n, result);
  return result;
}

// 2. Bottom-Up Tabulation Blueprint with Space Optimization
export function solveBottomUp(n) {
  if (n <= 1) return n;

  let prev2 = 0;
  let prev1 = 1;

  for (let i = 2; i <= n; i++) {
    const current = prev1 + prev2;
    prev2 = prev1;
    prev1 = current;
  }

  return prev1;
}
```

---

## 5. Practical Examples

### Example 1: House Robber (1D Space-Optimized DP)
```javascript
/**
 * Rob maximum money without robbing two adjacent houses.
 * Recurrence: dp[i] = max(dp[i-1], dp[i-2] + nums[i])
 * Time Complexity: O(N) | Space Complexity: O(1)
 */
export function rob(nums) {
  if (nums.length === 0) return 0;
  if (nums.length === 1) return nums[0];

  let prevRobbed = 0;   // Represents dp[i-2]
  let currentRobbed = 0; // Represents dp[i-1]

  for (const num of nums) {
    const temp = Math.max(currentRobbed, prevRobbed + num);
    prevRobbed = currentRobbed;
    currentRobbed = temp;
  }

  return currentRobbed;
}

// console.log(rob([2, 7, 9, 3, 1])); // 12 (rob house 1, 3, 5: 2 + 9 + 1 = 12)
```

### Example 2: Coin Change (Unbounded Knapsack / Minimum Coins)
```javascript
/**
 * Finds minimum number of coins needed to make up amount.
 * Recurrence: dp[a] = min(dp[a], dp[a - coin] + 1)
 * Time: O(amount * coins.length) | Space: O(amount)
 */
export function coinChange(coins, amount) {
  // Initialize table with Infinity
  const dp = new Array(amount + 1).fill(Infinity);
  dp[0] = 0; // Base case: 0 coins needed for amount 0

  for (let a = 1; a <= amount; a++) {
    for (const coin of coins) {
      if (a - coin >= 0) {
        dp[a] = Math.min(dp[a], dp[a - coin] + 1);
      }
    }
  }

  return dp[amount] === Infinity ? -1 : dp[amount];
}

// console.log(coinChange([1, 2, 5], 11)); // 3 (5 + 5 + 1)
```

### Example 3: Longest Common Subsequence (2D Grid DP)
```javascript
export function longestCommonSubsequence(text1, text2) {
  const m = text1.length;
  const n = text2.length;
  // Initialize (m + 1) x (n + 1) table with 0
  const dp = Array.from({ length: m + 1 }, () => new Int32Array(n + 1));

  for (let i = 1; i <= m; i++) {
    for (let j = 1; j <= n; j++) {
      if (text1[i - 1] === text2[j - 1]) {
        dp[i][j] = dp[i - 1][j - 1] + 1; // Diagonal match
      } else {
        dp[i][j] = Math.max(dp[i - 1][j], dp[i][j - 1]); // Skip char from text1 or text2
      }
    }
  }

  return dp[m][n];
}
```

### Example 4: Edit Distance (Levenshtein Distance)
```javascript
export function minDistance(word1, word2) {
  const m = word1.length;
  const n = word2.length;
  const dp = Array.from({ length: m + 1 }, () => new Int32Array(n + 1));

  // Base cases: transforming empty string into word
  for (let i = 0; i <= m; i++) dp[i][0] = i;
  for (let j = 0; j <= n; j++) dp[0][j] = j;

  for (let i = 1; i <= m; i++) {
    for (let j = 1; j <= n; j++) {
      if (word1[i - 1] === word2[j - 1]) {
        dp[i][j] = dp[i - 1][j - 1]; // Characters match; no operation needed
      } else {
        dp[i][j] = 1 + Math.min(
          dp[i - 1][j],    // Delete
          dp[i][j - 1],    // Insert
          dp[i - 1][j - 1] // Replace
        );
      }
    }
  }

  return dp[m][n];
}

// console.log(minDistance("horse", "ros")); // 3 (replace h->r, delete r, delete e)
```

### Example 5: 0/1 Knapsack with 1D Array Space Compression
```javascript
/**
 * 0/1 Knapsack: maximize value within capacity limit.
 * Time: O(N * W) | Space: O(W) (compressed 1D array)
 */
export function knapsack01(weights, values, capacity) {
  const n = weights.length;
  const dp = new Int32Array(capacity + 1);

  for (let i = 0; i < n; i++) {
    const w = weights[i];
    const v = values[i];

    // Iterate backwards from capacity down to weight
    // Backward iteration guarantees each item is used AT MOST ONCE!
    for (let j = capacity; j >= w; j--) {
      dp[j] = Math.max(dp[j], dp[j - w] + v);
    }
  }

  return dp[capacity];
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Forward vs. Backward Iteration in 1D Knapsack**:
   - In **0/1 Knapsack** (items can only be used once), iterating capacity forward (`for j = w to capacity`) reuses the same item multiple times because `dp[j - w]` was already updated in the current step. **You must iterate backwards** (`for j = capacity down to w`).
   - In **Unbounded Knapsack** (items can be reused infinitely, like Coin Change), you deliberately iterate forward.
2. **Heap Memory Blowout on Huge 2D Matrices**:
   Allocating a 2D matrix of $50,000 \times 50,000$ JavaScript arrays attempts to allocate $2.5 \times 10^9$ array elements ($\approx 10\text{GB}$ RAM), immediately crashing Node.js with `Allocation failed - JavaScript heap out of memory`. Always compress 2D DP tables to two 1D arrays or a single rolling 1D array.
3. **Improper Table Initialization (`Infinity` vs `0`)**:
   In minimization problems (like Coin Change), initializing with `0` causes `Math.min(0, ...)` to remain 0, producing wrong outputs. Always initialize minimization tables with `Infinity`.
4. **Call Stack Limit in Deep Memoization**:
   Top-down memoized recursion on $N = 20,000$ will crash with `RangeError: Maximum call stack size exceeded` in V8. Use bottom-up iterative tabulation for large $N$.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Hazard | Production Fix |
| :--- | :--- | :--- |
| Forward loop in 0/1 Knapsack space compression | Accidental item reuse; turns 0/1 Knapsack into Unbounded Knapsack. | Iterate capacity loop backwards: `for (let j = cap; j >= w; j--)`. |
| Initializing minimization DP with 0 | `Math.min(0, candidate)` remains 0, corrupting all calculations. | Initialize minimization tables with `Infinity`. |
| Unbounded top-down recursion ($N > 10,000$) | Crashes Node.js with `RangeError` (Call Stack Overflow). | Convert to bottom-up tabulation with an iterative loop. |
| Allocating full 2D matrix when only previous row is needed | Massive memory consumption ($O(M \times N)$). | Compress space to two rolling 1D rows or a single 1D array ($O(N)$). |
| Using string key hashes in nested memoization | String serialization overhead in `memo.set('${i}:${j}')` kills performance. | Use a flat 1D `Int32Array` indexed by `i * cols + j`. |
| Missing base case initialization | `dp[0]` evaluates to `undefined`, yielding `NaN` across entire table. | Explicitly define base states before starting the loop. |

---

## 8. Performance Characteristics

- **Time Complexity**:
  - 1D DP: $O(N)$ or $O(N \times K)$ operations.
  - 2D DP: $O(M \times N)$ operations.
- **Space Complexity**:
  - Naive Tabulation: $O(M \times N)$ auxiliary memory.
  - Space Compressed: **$O(N)$** linear memory.
  - Rolling Variables (1D): **$O(1)$** constant memory.

---

## 9. Engine Internals

### V8 TypedArrays & CPU Cache Lines
- **TypedArray Performance**: Initializing tables with `new Int32Array(capacity + 1)` allocates flat, contiguous C++ memory buffers with zero pointer tagging or hidden class overhead. Index reads and writes translate directly into raw CPU memory load/store instructions.
- **Row-Major Memory Locality**: Modern CPUs prefetch adjacent memory along cache lines. Iterating row-major (`for i: for j: dp[i][j]`) accesses sequential memory addresses, maximizing L1 cache hits. Iterating column-major (`for j: for i: dp[i][j]`) strides across memory addresses, causing frequent cache misses.

---

## 10. Best Practices & Design Guidelines

1. **Follow the 4-Step DP Framework**:
   1. Define State: What does `dp[i]` or `dp[i][j]` represent?
   2. Derive Recurrence Relation: How does `dp[i]` transition from previous states?
   3. Identify Base Cases: What are the known starting values (`dp[0]`)?
   4. Optimize Space: Can we discard earlier rows and retain only the previous state?
2. **Prefer Bottom-Up Tabulation in Production**: Eliminates call stack limits and executes faster in V8.
3. **Use `Int32Array` for Numeric Tables**: Delivers lower memory footprint and zero GC overhead.

---

## 11. Real-World Case Study

### Git Text Diffing Engine (Myers Diff & LCS)
**Scenario**: Version control systems (Git) and online code editors (Monaco, VS Code) must compute line-by-line diffs between two versions of a 10,000-line source file.
**Architecture**:
- Computing which lines were inserted, deleted, or retained maps directly to the **Longest Common Subsequence (LCS)** problem.
- Lines that belong to the LCS are marked unmodified.
- Lines present in file 1 but not in the LCS are marked deleted (`-`).
- Lines present in file 2 but not in the LCS are marked added (`+`).
- Uses Eugene Myers' $O(ND)$ diff algorithm (an optimized greedy DP variant) that runs in $O(N \cdot D)$ time where $D$ is the number of differences.

**Outcome**:
- Generates clean, human-readable diffs across thousands of code lines in $<5\text{ms}$.

---

## 12. Interview Questions

<details>
<summary>1. How do you recognize that a problem can be solved with Dynamic Programming?</summary>

A problem is a candidate for Dynamic Programming if it exhibits:
1. **Overlapping Subproblems**: Recursive solutions repeatedly evaluate the same subproblems with the same parameters (e.g. `fib(n-1)` and `fib(n-2)` both evaluate `fib(n-2)`).
2. **Optimal Substructure**: The optimal solution to the overall problem can be constructed directly from the optimal solutions to its smaller subproblems (e.g. shortest path, maximum value, minimum coins).
3. The problem typically asks for an **extremum** (minimum, maximum, longest, shortest) or asks to count the **total number of ways** to achieve a target.
</details>

<details>
<summary>2. What is the difference between Top-Down (Memoization) and Bottom-Up (Tabulation)?</summary>

- **Top-Down (Memoization)**: Follows the natural recursive formulation. It starts from the top target state, explores only the subproblems required to solve that state, and caches outputs in a Map or array. Disadvantage: recursion overhead and risk of Call Stack Overflow on large inputs.
- **Bottom-Up (Tabulation)**: Starts from the smallest base cases and iteratively fills an array or table up to the target state. Advantages: zero call stack overhead, better CPU cache locality, and allows space optimization (discarding old states).
</details>

<details>
<summary>3. Why must we iterate backwards in the 1D space-compressed 0/1 Knapsack?</summary>

In 0/1 Knapsack, each item can only be selected once. The transition is `dp[j] = max(dp[j], dp[j - w] + v)`, where `dp[j - w]` represents the maximum value *without* the current item (from the previous iteration). If we iterate forward ($j = w \to \text{cap}$), `dp[j - w]` has already been updated in the *current* iteration to include the current item, causing the item to be counted multiple times. Iterating backwards ($j = \text{cap} \to w$) ensures `dp[j - w]` still holds the value from the previous item's row.
</details>

<details>
<summary>4. How does space compression reduce memory in Dynamic Programming?</summary>

Many DP recurrence relations depend only on a small sliding window of previous states. For example, in Fibonacci, `dp[i]` depends only on `dp[i-1]` and `dp[i-2]`, so we can maintain two scalar variables rather than an array of size $N$, reducing memory from $O(N)$ to $O(1)$. In 2D grid DP (e.g. LCS or Edit Distance), row $i$ depends only on row $i-1$, allowing us to retain only two rows (or one 1D array), reducing space from $O(M \times N)$ to $O(N)$.
</details>

<details>
<summary>5. What is the time and space complexity of the Edit Distance (Levenshtein) algorithm?</summary>

For strings of length $M$ and $N$:
- **Time Complexity**: **$\Theta(M \times N)$**, because every cell in the $(M + 1) \times (N + 1)$ table requires $O(1)$ constant-time transitions (checking minimum of delete, insert, replace).
- **Space Complexity**: **$O(M \times N)$** for standard 2D table, or **$O(\min(M, N))$** when compressed using two rolling rows.
</details>

<details>
<summary>6. How do you distinguish between 0/1 Knapsack and Unbounded Knapsack?</summary>

- **0/1 Knapsack**: Each item can be picked at most once. (e.g. Subset Sum, Partition Equal Subset Sum). Handled with 2D table or backward 1D iteration.
- **Unbounded Knapsack**: Each item can be picked an unlimited number of times. (e.g. Coin Change, Cutting a Rod). Handled with forward 1D iteration.
</details>

<details>
<summary>7. What causes a <code>RangeError</code> when executing Top-Down DP in V8?</summary>

In V8, the execution Call Stack is limited to approximately 10,000 frames. In top-down memoized recursion, even if subproblems are memoized, the depth of the recursive call tree to reach the base case can exceed 10,000 frames (e.g. `n = 20,000` in Climbing Stairs). When the stack exceeds memory limits, V8 throws `RangeError: Maximum call stack size exceeded`. Bottom-up tabulation avoids this by executing within a single iterative stack frame.
</details>

<details>
<summary>8. How do you reconstruct the actual elements that form the optimal solution in DP?</summary>

Maintain a "backtracking" pointer matrix or trace back from the final cell in the DP table:
At cell `dp[i][j]`, inspect which preceding cell (`dp[i-1][j]`, `dp[i][j-1]`, or `dp[i-1][j-1]`) produced the current value. By following these transition choices backwards until reaching the base case, you reconstruct the exact sequence of selected items or operations in $O(M + N)$ time.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Unique Paths in a Grid
**Specification**: There is a robot on an `m x n` grid. The robot is initially located at the top-left corner and can only move either down or right at any point. Find the number of possible unique paths to reach the bottom-right corner. Space complexity must be $O(N)$.

<details>
<summary>Solution</summary>

```javascript
export function uniquePaths(m, n) {
  // Compress 2D table to 1D array of size n
  const dp = new Int32Array(n).fill(1);

  for (let r = 1; r < m; r++) {
    for (let c = 1; c < n; c++) {
      // Current paths = paths from top (dp[c]) + paths from left (dp[c - 1])
      dp[c] = dp[c] + dp[c - 1];
    }
  }

  return dp[n - 1];
}
```
</details>

### Exercise 2: Longest Increasing Subsequence (LIS)
**Specification**: Given an integer array `nums`, return the length of the longest strictly increasing subsequence. Implement in $O(N \log N)$ time using Patience Sorting / Binary Search.

<details>
<summary>Solution</summary>

```javascript
export function lengthOfLIS(nums) {
  if (nums.length === 0) return 0;

  // tails[i] stores smallest tail of all increasing subsequences of length i + 1
  const tails = [];

  for (const x of nums) {
    let low = 0;
    let high = tails.length;

    // Binary search for first element >= x
    while (low < high) {
      const mid = low + Math.floor((high - low) / 2);
      if (tails[mid] < x) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }

    // Replace or extend
    if (low === tails.length) {
      tails.push(x);
    } else {
      tails[low] = x;
    }
  }

  return tails.length;
}
```
</details>
