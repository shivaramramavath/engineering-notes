# Recursion and Backtracking Algorithmic Patterns in Java

> Call stack frames, base cases, state space tree exploration, pruning optimizations, N-Queens, Subsets, and Sudoku Solver in Java.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Advanced |
| **Prerequisites** | [`27-dsa/00_complexity-analysis.md`](00_complexity-analysis.md), [`01-fundamentals/07_functions.md`](../01-fundamentals/07_functions.md) |
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

Recursion is a computational paradigm where a function solves a problem by calling itself with smaller sub-instances until a terminating base case is reached.

**Backtracking** is a systematic algorithmic technique for exploring all (or some) solutions to combinatorial problems. It models the problem space as an implicit Decision Tree (State Space Tree), traversing downward along a promising path (DFS), and immediately backtracking (undoing choices) when a path violates problem constraints.

---

## 2. Core Concepts

### 2.1 Anatomy of a Backtracking Algorithm

Every backtracking template follows a strict three-phase pattern:
1. **Choose**: Make a candidate choice (e.g., place a queen, include a number).
2. **Explore**: Recurse down to the next decision level.
3. **Un-choose (Backtrack)**: Revert the choice to restore state for parallel decision branches.

```
+-------------------------------------------------------+
|                BACKTRACKING DECISION TREE             |
|                                                       |
|                       [Root: ()]                      |
|                      /          \                     |
|                 Choose 1      Choose 2                |
|                  /                \                   |
|             [ (1) ]              [ (2) ]              |
|             /     \              /     \             |
|        Choose 2  Prune(x)    Choose 1  Prune(x)       |
|          /                      /                     |
|      [ (1,2) ]              [ (2,1) ]                 |
+-------------------------------------------------------+
```

### 2.2 Pruning & Branch-and-Bound
Pruning abandons dead-end branches early without traversing deeper subtrees, transforming intractable $O(N!)$ or $O(2^N)$ brute-force searches into practical algorithms.

---

## 3. How It Works Internally

### 3.1 JVM Call Stack Frame Mechanics

Each recursive invocation pushes a new stack frame onto the current thread's execution stack:
```
Thread Stack Memory (Default 1MB):
+-------------------------------------------------------+
| Frame 3: backtrack(depth=3, state={...})              |
| - Local Variables, Return PC to Frame 2               |
+-------------------------------------------------------+
| Frame 2: backtrack(depth=2, state={...})              |
+-------------------------------------------------------+
| Frame 1: backtrack(depth=1, state={...})              |
+-------------------------------------------------------+
| Frame 0: main()                                       |
+-------------------------------------------------------+
```
If recursion depth exceeds available stack space without hitting a base case, the JVM throws `java.lang.StackOverflowError`.

---

## 4. Syntax & Usage

### 4.1 Canonical Java Backtracking Template

```java
public void backtrack(State state, List<Result> results) {
    if (isGoal(state)) {
        results.add(new Result(state)); // Must take deep copy!
        return;
    }

    for (Choice choice : getChoices(state)) {
        if (isValid(choice, state)) {
            state.apply(choice);          // 1. Choose
            backtrack(state, results);    // 2. Explore
            state.revert(choice);         // 3. Un-choose
        }
    }
}
```

---

## 5. Practical Examples

### 5.1 Pattern 1: Generating Subsets / Power Set (LeetCode 78)
```java
import java.util.*;

public class SubsetsGenerator {
    public static List<List<Integer>> subsets(int[] nums) {
        List<List<Integer>> results = new ArrayList<>();
        generate(0, nums, new ArrayList<>(), results);
        return results;
    }

    private static void generate(int index, int[] nums, List<Integer> current, List<List<Integer>> results) {
        results.add(new ArrayList<>(current)); // Deep copy!

        for (int i = index; i < nums.length; i++) {
            current.add(nums[i]);                     // 1. Choose
            generate(i + 1, nums, current, results);  // 2. Explore
            current.remove(current.size() - 1);       // 3. Un-choose
        }
    }
}
```

### 5.2 Pattern 2: Permutations (LeetCode 46)
```java
import java.util.*;

public class PermutationsGenerator {
    public static List<List<Integer>> permute(int[] nums) {
        List<List<Integer>> results = new ArrayList<>();
        boolean[] used = new boolean[nums.length];
        backtrack(nums, used, new ArrayList<>(), results);
        return results;
    }

    private static void backtrack(int[] nums, boolean[] used, List<Integer> current, List<List<Integer>> results) {
        if (current.size() == nums.length) {
            results.add(new ArrayList<>(current));
            return;
        }

        for (int i = 0; i < nums.length; i++) {
            if (used[i]) continue;

            used[i] = true;
            current.add(nums[i]);
            backtrack(nums, used, current, results);
            current.remove(current.size() - 1);
            used[i] = false;
        }
    }
}
```

### 5.3 Pattern 3: Combination Sum with Duplicates (LeetCode 39)
```java
import java.util.*;

public class CombinationSum {
    public static List<List<Integer>> combinationSum(int[] candidates, int target) {
        List<List<Integer>> results = new ArrayList<>();
        Arrays.sort(candidates);
        findCombinations(0, target, candidates, new ArrayList<>(), results);
        return results;
    }

    private static void findCombinations(int start, int remaining, int[] candidates, List<Integer> current, List<List<Integer>> results) {
        if (remaining == 0) {
            results.add(new ArrayList<>(current));
            return;
        }

        for (int i = start; i < candidates.length; i++) {
            if (candidates[i] > remaining) break; // Prune branch!

            current.add(candidates[i]);
            findCombinations(i, remaining - candidates[i], candidates, current, results); // i allowed again
            current.remove(current.size() - 1);
        }
    }
}
```

### 5.4 Pattern 4: N-Queens Problem (LeetCode 51)
```java
import java.util.*;

public class NQueensSolver {
    public static List<List<String>> solveNQueens(int n) {
        List<List<String>> results = new ArrayList<>();
        boolean[] cols = new boolean[n];
        boolean[] diag1 = new boolean[2 * n]; // row - col + n
        boolean[] diag2 = new boolean[2 * n]; // row + col

        char[][] board = new char[n][n];
        for (char[] row : board) Arrays.fill(row, '.');

        placeQueen(0, n, board, cols, diag1, diag2, results);
        return results;
    }

    private static void placeQueen(int row, int n, char[][] board, boolean[] cols, boolean[] d1, boolean[] d2, List<List<String>> results) {
        if (row == n) {
            List<String> solution = new ArrayList<>();
            for (char[] r : board) solution.add(new String(r));
            results.add(solution);
            return;
        }

        for (int col = 0; col < n; col++) {
            int id1 = row - col + n;
            int id2 = row + col;

            if (cols[col] || d1[id1] || d2[id2]) continue; // Pruned

            board[row][col] = 'Q';
            cols[col] = d1[id1] = d2[id2] = true;

            placeQueen(row + 1, n, board, cols, d1, d2, results);

            board[row][col] = '.';
            cols[col] = d1[id1] = d2[id2] = false;
        }
    }
}
```

### 5.5 Pattern 5: Sudoku Solver (LeetCode 37)
```java
public class SudokuSolver {
    public static boolean solveSudoku(char[][] board) {
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                if (board[r][c] == '.') {
                    for (char ch = '1'; ch <= '9'; ch++) {
                        if (isValid(board, r, c, ch)) {
                            board[r][c] = ch;
                            if (solveSudoku(board)) return true;
                            board[r][c] = '.'; // Backtrack
                        }
                    }
                    return false; // No digit valid -> trigger backtrack
                }
            }
        }
        return true; // Solved
    }

    private static boolean isValid(char[][] board, int row, int col, char ch) {
        for (int i = 0; i < 9; i++) {
            if (board[row][i] == ch) return false;
            if (board[i][col] == ch) return false;
            if (board[3 * (row / 3) + i / 3][3 * (col / 3) + i % 3] == ch) return false;
        }
        return true;
    }
}
```

---

## 6. Best Practices

1. **Always Clone Before Adding to Results**: Writing `results.add(current)` stores a reference to a mutable list that ends up empty when backtracking finishes. Always do `results.add(new ArrayList<>(current))`.
2. **Sort Inputs to Enable Pruning**: Sorting arrays allows early termination (`if (candidates[i] > remaining) break;`), pruning massive subtrees.
3. **Use Primitive Bitmasks for State**: Replace `boolean[]` with an integer bitmask (`int mask |= (1 << val)`) for 3x speedup.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Storing Mutable Reference** | Final result contains empty lists | Store copy: `new ArrayList<>(current)` |
| **Missing Base Case** | Recursion never halts, crashing with `StackOverflowError` | Ensure all branches reach a terminating base condition |
| **Incomplete Backtrack Cleanup** | State mutation leaks into sibling branches, giving corrupt results | Revert all mutated state in reverse order |
| **Duplicate Combinations** | For combinations with repeated elements, identical branches are traversed | Skip duplicates: `if (i > start && nums[i] == nums[i-1]) continue;` |

---

## 8. Performance & Security Notes

### 8.1 Tail-Call Optimization (TCO) in Java
The standard HotSpot JVM does NOT support automatic Tail-Call Optimization due to the requirement to preserve accurate stack traces for security checks (`AccessController`, `SecurityManager`). Deep recursion must be manually converted into iterative loops with explicit `ArrayDeque` stacks.

---

## 9. Comparison / When to Use What

| Algorithm Style | Decision Model | When to Use |
| :--- | :--- | :--- |
| **Backtracking** | Explores ALL valid solutions / paths | Permutations, N-Queens, Sudoku, Combinations |
| **Dynamic Programming**| Overlapping subproblems, optimal substructure | Counting paths, Max/Min optimization |
| **Greedy** | Makes irreversible best local choice | Interval scheduling, Dijkstra, Kruskal |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between Subsets, Combinations, and Permutations?</summary>
- **Subsets**: All possible subsets of any length (order does not matter): $2^N$ total.
- **Combinations**: Subsets of specific length $K$ (order does not matter): $inom{N}{K}$ total.
- **Permutations**: Reorderings of all $N$ elements (order matters): $N!$ total.
</details>

<details>
<summary>2. Why does Java not support Tail-Call Optimization (TCO)?</summary>
The JVM stack model mandates that every method call maintains its own stack frame to facilitate accurate stack trace generation and security inspection. Collapsing stack frames would obscure caller identities.
</details>

<details>
<summary>3. How do you prevent duplicate combinations when input contains duplicates?</summary>
Sort the array first. When looping at the current decision level, skip identical elements: `if (i > start && nums[i] == nums[i - 1]) continue;`.
</details>

<details>
<summary>4. What is the time complexity of N-Queens?</summary>
Upper bounded by $O(N!)$, since the first queen has $N$ choices, the second has at most $N-1$, etc. With pruning, the actual search tree is significantly smaller.
</details>

<details>
<summary>5. How can you optimize diagonal collision checking in N-Queens to O(1)?</summary>
Use two boolean arrays (or bitmasks) indexed by `row - col + N` for major diagonals and `row + col` for minor diagonals.
</details>

<details>
<summary>6. What causes `StackOverflowError` in recursive algorithms?</summary>
Exceeding the JVM thread stack memory limit (typically 1MB), usually caused by missing base cases or recursion depth exceeding ~10,000 frames.
</details>

<details>
<summary>7. How do you convert a recursive algorithm to an iterative one?</summary>
Use an explicit `ArrayDeque` stack to simulate the call stack, pushing parameter state frames manually.
</details>

<details>
<summary>8. What is the time complexity of generating all subsets of a set of size N?</summary>
$O(N \cdot 2^N)$, because there are $2^N$ total subsets, and copying each subset takes on average $O(N)$ time.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Word Search on Grid (LeetCode 79)</summary>

```java
public class WordSearch {
    public static boolean exist(char[][] board, String word) {
        int m = board.length, n = board[0].length;
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                if (dfs(board, word, 0, i, j)) return true;
            }
        }
        return false;
    }

    private static boolean dfs(char[][] board, String word, int idx, int r, int c) {
        if (idx == word.length()) return true;
        if (r < 0 || c < 0 || r >= board.length || c >= board[0].length || board[r][c] != word.charAt(idx)) return false;

        char temp = board[r][c];
        board[r][c] = '#'; // Mark visited
        boolean found = dfs(board, word, idx + 1, r + 1, c)
                     || dfs(board, word, idx + 1, r - 1, c)
                     || dfs(board, word, idx + 1, r, c + 1)
                     || dfs(board, word, idx + 1, r, c - 1);
        board[r][c] = temp; // Backtrack
        return found;
    }
}
```
</details>

<details>
<summary>Exercise 2: Letter Combinations of a Phone Number (LeetCode 17)</summary>

```java
import java.util.*;

public class PhoneCombinations {
    private static final String[] MAPPINGS = {"", "", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz"};

    public static List<String> letterCombinations(String digits) {
        List<String> res = new ArrayList<>();
        if (digits == null || digits.isEmpty()) return res;
        dfs(0, digits, new StringBuilder(), res);
        return res;
    }

    private static void dfs(int idx, String digits, StringBuilder current, List<String> res) {
        if (idx == digits.length()) {
            res.add(current.toString());
            return;
        }
        String letters = MAPPINGS[digits.charAt(idx) - '0'];
        for (char ch : letters.toCharArray()) {
            current.append(ch);
            dfs(idx + 1, digits, current, res);
            current.deleteCharAt(current.length() - 1);
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Pattern**: Choose $	o$ Explore $	o$ Un-choose.
- **Copy**: Always instantiate a new copy `new ArrayList<>(current)` when adding to results.
- **Pruning**: Sort candidates and break early to trim dead-end branches.
- **Stack**: Java HotSpot lacks TCO; convert deep recursion ($N > 10^4$) to iterative stacks.

---

## 13. Further Reading

- [Algorithms by Sedgewick: Backtracking & Search](https://algs4.cs.princeton.edu/)
- [LeetCode Backtracking Study Guide](https://leetcode.com/explore/learn/card/recursion-ii/)
