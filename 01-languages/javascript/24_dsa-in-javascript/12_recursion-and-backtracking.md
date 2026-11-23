# Recursion & Backtracking in JavaScript

> A production-grade algorithmic guide to Recursion and Backtracking in JavaScript, covering state-space trees, the Choose-Explore-Unchoose pattern, constraint pruning, and V8 call stack mechanics.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [05_stacks-and-queues.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/05_stacks-and-queues.md) |
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

Many real-world computational challenges require evaluating combinations, permutations, or configurations within large discrete state spaces:
- Solving constraint satisfaction puzzles (Sudoku, N-Queens, crossword puzzles).
- Generating combinations (e.g. generating all valid password combinations or testing UI layout permutation sets).
- Exploring game-tree paths (Minimax, chess/checkers move evaluation).

**Recursion** solves a problem by expressing its solution in terms of smaller instances of the same problem.
**Backtracking** is an algorithmic technique for finding all (or some) solutions to computational problems that incrementally builds candidates to the solutions, abandoning ("backtracking" from) candidates as soon as it determines they cannot possibly be completed into valid solutions.

---

## 2. Core Concepts

### The Anatomical Structure of Recursion
Every recursive function requires:
1. **Base Case**: The terminal condition that stops recursion and returns a concrete value without further recursive calls.
2. **Recursive Case**: The logic that decomposes the problem into smaller sub-problems and recurses toward the base case.

### The Backtracking Template: Choose ➔ Explore ➔ Unchoose
Backtracking builds a path incrementally using a shared mutable state:
1. **Choose**: Make a candidate choice (e.g. place a queen on row $r$, col $c$; append element to `path`).
2. **Explore**: Recurse down the decision tree to explore all consequences of that choice.
3. **Unchoose (Backtrack)**: Revert the candidate choice (e.g. remove queen from row $r$, col $c$; `path.pop()`), restoring the state so other branches can explore cleanly.

```
Decision Tree Progression:
                   [ Root: [] ]
                 /      |      \
           Choose 1   Choose 2   Choose 3
             /            \          \
         [ 1 ]           [ 2 ]      [ 3 ]
        /     \           ...        ...
    [ 1, 2 ]  [ 1, 3 ]
       ▲
   (Unchoose: backtrack to [1] to explore [1, 3])
```

### Pruning (Branch Cutting)
Evaluating every potential branch results in combinatorial explosion ($O(2^N)$ or $O(N!)$). **Pruning** evaluates constraints early: if a partial candidate violates a constraint, the algorithm terminates that entire subtree immediately without recursing deeper.

---

## 3. How It Works Internally

During recursion, JavaScript's V8 engine pushes a new execution stack frame onto the Call Stack for every recursive invocation.
- Variables declared within the function scope are preserved on that specific frame.
- When the function returns, its frame is popped from the call stack, restoring the parent caller's execution pointer.

```
V8 Call Stack Frames:
Step 1 (Push): [ permute([1, 2, 3]) ] ──► [ permute([2, 3]) ] ──► [ permute([3]) ]
Step 2 (Base Case Reached): Return [ [1, 2, 3] ]
Step 3 (Pop / Unwind): Frame for permute([3]) popped; execution resumes in permute([2, 3])
```

---

## 4. Syntax & API Reference

### Universal Backtracking Skeleton
```javascript
/**
 * Canonical Backtracking Algorithm Template
 */
export function solveBacktracking(candidates) {
  const results = [];
  const currentPath = [];

  function backtrack(startIndex) {
    // 1. Goal / Base Case Reached
    if (isSolution(currentPath)) {
      results.push([...currentPath]); // CRITICAL: Clone path!
      return;
    }

    // 2. Iterate through candidate choices
    for (let i = startIndex; i < candidates.length; i++) {
      const candidate = candidates[i];

      // 3. Prune invalid choices
      if (!isValidChoice(candidate, currentPath)) {
        continue;
      }

      // 4. CHOOSE
      currentPath.push(candidate);

      // 5. EXPLORE
      backtrack(i + 1);

      // 6. UNCHOOSE (Backtrack)
      currentPath.pop();
    }
  }

  backtrack(0);
  return results;
}
```

---

## 5. Practical Examples

### Example 1: Generate All Permutations of an Array
```javascript
export function permute(nums) {
  const results = [];
  const used = new Array(nums.length).fill(false);
  const current = [];

  function backtrack() {
    if (current.length === nums.length) {
      results.push([...current]); // Snapshot copy
      return;
    }

    for (let i = 0; i < nums.length; i++) {
      if (used[i]) continue; // Skip already chosen elements

      // Choose
      used[i] = true;
      current.push(nums[i]);

      // Explore
      backtrack();

      // Unchoose
      current.pop();
      used[i] = false;
    }
  }

  backtrack();
  return results;
}

// console.log(permute([1, 2, 3]));
// Outputs 6 permutations: [[1,2,3], [1,3,2], [2,1,3], [2,3,1], [3,1,2], [3,2,1]]
```

### Example 2: Subsets / Power Set Generation
```javascript
export function subsets(nums) {
  const results = [];
  const current = [];

  function backtrack(startIndex) {
    // Every state in the decision tree is a valid subset
    results.push([...current]);

    for (let i = startIndex; i < nums.length; i++) {
      current.push(nums[i]);     // Choose
      backtrack(i + 1);          // Explore (i + 1 avoids duplicates)
      current.pop();             // Unchoose
    }
  }

  backtrack(0);
  return results;
}
```

### Example 3: Combination Sum (Unbounded Candidate Reuse)
```javascript
export function combinationSum(candidates, target) {
  const results = [];
  const current = [];

  // Sort candidates to enable early break pruning
  candidates.sort((a, b) => a - b);

  function backtrack(startIndex, remaining) {
    if (remaining === 0) {
      results.push([...current]);
      return;
    }

    for (let i = startIndex; i < candidates.length; i++) {
      const val = candidates[i];
      // Prune: since array is sorted, subsequent elements will also exceed remaining
      if (val > remaining) break;

      current.push(val);
      // Pass 'i' (not i + 1) because elements can be reused unlimited times
      backtrack(i, remaining - val);
      current.pop();
    }
  }

  backtrack(0, target);
  return results;
}
```

### Example 4: N-Queens Constraint Satisfaction Problem
```javascript
export function solveNQueens(n) {
  const results = [];
  // Board represented by array where board[row] = colIndex
  const board = [];

  const cols = new Set();
  const diag1 = new Set(); // row - col
  const diag2 = new Set(); // row + col

  function backtrack(row) {
    if (row === n) {
      // Build string visual representation of board
      const solution = board.map((c) => '.'.repeat(c) + 'Q' + '.'.repeat(n - c - 1));
      results.push(solution);
      return;
    }

    for (let col = 0; col < n; col++) {
      if (cols.has(col) || diag1.has(row - col) || diag2.has(row + col)) {
        continue; // Prune invalid placement
      }

      // Choose
      cols.add(col);
      diag1.add(row - col);
      diag2.add(row + col);
      board.push(col);

      // Explore
      backtrack(row + 1);

      // Unchoose
      board.pop();
      cols.delete(col);
      diag1.delete(row - col);
      diag2.delete(row + col);
    }
  }

  backtrack(0);
  return results;
}
```

### Example 5: Word Search in 2D Grid
```javascript
export function exist(board, word) {
  const rows = board.length;
  const cols = board[0].length;

  function backtrack(r, c, charIdx) {
    if (charIdx === word.length) return true;

    // Boundary and character mismatch check
    if (r < 0 || c < 0 || r >= rows || c >= cols || board[r][c] !== word[charIdx]) {
      return false;
    }

    // Choose: Mask cell to mark as visited
    const temp = board[r][c];
    board[r][c] = '#';

    // Explore 4 directions
    const found =
      backtrack(r + 1, c, charIdx + 1) ||
      backtrack(r - 1, c, charIdx + 1) ||
      backtrack(r, c + 1, charIdx + 1) ||
      backtrack(r, c - 1, charIdx + 1);

    // Unchoose: Restore cell
    board[r][c] = temp;

    return found;
  }

  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      if (board[r][c] === word[0] && backtrack(r, c, 0)) {
        return true;
      }
    }
  }

  return false;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Pushing Uncloned Accumulators to Results**:
   Writing `results.push(current)` pushes a reference to the mutable array. As backtracking continues and eventually pops all items, `results` ends up containing an array of empty arrays: `[[], [], []]`. Always clone: `results.push([...current])` or `results.push(current.slice())`.
2. **Missing the Unchoose Step**:
   Forgetting to restore state (e.g. omitting `current.pop()` or `used[i] = false`) pollutes the accumulator, causing subsequent branches to be evaluated against dirty state.
3. **Tail Call Optimization (TCO) Illusion in Node.js/Chrome**:
   While ECMAScript 2015 specified Proper Tail Calls (PTC), **V8 (Chrome & Node.js) does NOT support TCO** due to debugging call stack preservation requirements and performance complexities. Even "tail-recursive" functions consume call stack frames in V8 and will throw `RangeError: Maximum call stack size exceeded` if recursion depth exceeds $\approx 10,000$.
4. **Duplicate Candidates Generating Duplicate Combinations**:
   When input arrays contain duplicate numbers (e.g. `[1, 2, 2]`), sorting the array upfront and skipping adjacent duplicates (`if (i > start && nums[i] === nums[i - 1]) continue;`) is mandatory to prevent duplicate output sets.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Defect | Production Remediation |
| :--- | :--- | :--- |
| `results.push(current)` without cloning | Pushes mutable reference; results array ends up full of empty arrays. | Always snapshot copy: `results.push([...current])`. |
| Omitting the unchoose step | State leaks across branches, producing corrupt output. | Pair every mutation with a symmetric reversal after `backtrack()`. |
| Relying on Tail Call Optimization in V8 | Assumes infinite recursion works; process crashes with RangeError. | Use explicit iterative loop with a stack data structure. |
| Missing duplicate pruning on sorted inputs | Generates duplicate subsets and combinatorial redundancy. | Skip duplicates: `if (i > start && nums[i] === nums[i - 1]) continue`. |
| Unsorted input in bounded combination sums | Prevents early branch pruning (`break`), wasting compute cycles. | Sort candidates ascending before starting backtracking. |
| Creating new array slices at every call | `arr.slice()` inside loops creates massive GC allocation thrashing. | Use index pointers (`startIndex`) and a single mutable accumulator. |

---

## 8. Performance Characteristics

- **Permutations Time Complexity**: $O(N! \times N)$ (there are $N!$ permutations, each requiring $O(N)$ copy operations).
- **Subsets Time Complexity**: $O(2^N \times N)$ (there are $2^N$ total subsets).
- **Auxiliary Space Complexity**: $O(N)$ call stack and accumulator space (bounded by maximum tree depth).

---

## 9. Engine Internals

### V8 Stack Limits & Trampolining
- **Max Call Stack Size**: V8 allocates a fixed stack memory buffer per isolate (typically 1MB). Depending on local variable counts per frame, recursion crashes between 9,500 and 10,500 calls.
- **Trampoline Pattern**: When deep recursion is required without stack overflow, functions can return a thunk (a closure representing the next step) executed by a driver `while` loop:
  ```javascript
  function trampoline(fn) {
    return (...args) => {
      let result = fn(...args);
      while (typeof result === 'function') result = result();
      return result;
    };
  }
  ```

---

## 10. Best Practices & Design Guidelines

1. **Sort Candidates Upfront**: Sorting enables $O(1)$ early `break` pruning when values exceed remaining targets.
2. **Use Shared Mutable State**: Maintain a single `current = []` array and push/pop elements rather than creating new array copies at every recursive step.
3. **Use Sets or Bitmasks for $O(1)$ Constraint Checks**: In N-Queens, checking column and diagonal collisions using Sets (`cols.has(col)`) takes $O(1)$ time compared to $O(N)$ board scans.
4. **Clone Only at Leaf Nodes**: Clone state (`[...path]`) only when adding a finalized solution to `results`.

---

## 11. Real-World Case Study

### Combinatorial Access Control Policy Validator
**Scenario**: A cloud identity platform allowed administrators to define complex Boolean permission rules (e.g. `(Read AND Export) OR (Admin AND NOT Revoked)`). The platform needed to verify whether any combination of user attributes could satisfy a contradictory or forbidden state.
**Implementation**:
- The validator converted the policy AST into a Boolean Satisfiability (SAT) constraint problem.
- Used a backtracking engine with constraint propagation:
  - Pick attribute state (True/False).
  - Prune branches that contradict known assignment invariants.
  - Explore subsequent attributes.
- If a valid truth assignment was found, it reported a policy security flaw.

**Outcome**:
- The backtracking engine evaluated policies across 25 variables ($2^{25} \approx 33\text{M}$ combinations) in less than $12\text{ms}$ due to aggressive constraint pruning.

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between Divide-and-Conquer and Backtracking?</summary>

- **Divide and Conquer**: Breaks a problem into non-overlapping subproblems, solves each independently, and combines their solutions (e.g. Merge Sort, Quick Sort). Each subproblem is solved once without undoing choices.
- **Backtracking**: Systematically searches a combinatorial state-space tree. It builds candidates incrementally; when a candidate violates problem constraints, it abandons the branch ("backtracks" by unchoosing the most recent action) and attempts alternative choices.
</details>

<details>
<summary>2. Why does <code>results.push(current)</code> produce bugs in backtracking algorithms?</summary>

In JavaScript, objects and arrays are reference types passed by pointer. If `current` is pushed directly, `results` stores a reference to the same underlying array instance in heap memory. As backtracking unwinds and calls `current.pop()`, that exact same array instance is mutated. By the time the function finishes, all entries in `results` point to the now-empty array. To fix this, create a snapshot copy: `results.push([...current])`.
</details>

<details>
<summary>3. Does modern JavaScript support Tail Call Optimization (TCO)?</summary>

In theory yes (standardized in ECMAScript 2015), but in practice **no**. The primary JavaScript engine—V8 (Chrome, Node.js)—does not support TCO. V8 developers declined to implement it because it makes stack traces difficult to reconstruct during error debugging and introduces architectural overhead in JIT compilation. In Node.js, recursive functions that exceed $\approx 10,000$ calls will crash with a stack overflow, regardless of whether they are written in tail-recursive form.
</details>

<details>
<summary>4. How does pruning improve the time complexity of Backtracking algorithms?</summary>

Pruning (or branch cutting) eliminates entire subtrees of the decision tree as soon as a partial solution violates a constraint (e.g., current sum exceeds target, or queen placement conflicts with an existing queen). In N-Queens, unpruned brute-force evaluation tests $N^N$ positions (e.g. $8^8 \approx 16.7\text{M}$ boards). With pruning, the search space collapses to only 2,057 explored boards—a $99.98\%$ reduction in work.
</details>

<details>
<summary>5. How do you prevent duplicate subsets when the input array contains duplicate elements?</summary>

1. Sort the input array so duplicate values are adjacent.
2. Inside the backtracking loop, skip candidates if `i > startIndex && nums[i] === nums[i - 1]`.
This ensures that a duplicate value is only used if its previous identical sibling was also selected in the current branch, eliminating duplicate combinations.
</details>

<details>
<summary>6. What is the time complexity of generating all subsets of a set of size $N$?</summary>

**$\Theta(N \cdot 2^N)$**.
Every element in the set has two choices: either it is included in a subset, or it is excluded ($2 \times 2 \times \dots \times 2 = 2^N$ total subsets). Copying each subset of average length $\frac{N}{2}$ into the results array takes $O(N)$ operations. Thus, total time is $\Theta(N \cdot 2^N)$.
</details>

<details>
<summary>7. How can you convert deep recursion into iteration to avoid Call Stack Overflow?</summary>

By maintaining an explicit array-based **Stack** on the V8 heap:
1. Initialize `stack = [initialState]`.
2. Execute a `while (stack.length > 0)` loop.
3. Pop the top state, process it, and push new candidate child states onto the stack.
Because heap memory is typically gigabytes in size (compared to the 1MB call stack limit), the explicit heap stack can handle millions of states without overflow.
</details>

<details>
<summary>8. How do you handle in-place backtracking in a 2D grid search without allocating a <code>visited</code> matrix?</summary>

By **masking the grid cell in-place**:
1. When visiting `grid[r][c]`, store its character: `const temp = grid[r][c];`.
2. Overwrite it with a sentinel character: `grid[r][c] = '#';` (so subsequent searches treat it as a wall).
3. Recurse in 4 directions.
4. After recursing, restore the original character: `grid[r][c] = temp;` (unchoose).
This operates in $O(1)$ auxiliary space without allocating a separate $M \times N$ boolean matrix.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Generate Parentheses
**Specification**: Given `n` pairs of parentheses, write a function to generate all combinations of well-formed parentheses.

<details>
<summary>Solution</summary>

```javascript
export function generateParenthesis(n) {
  const results = [];

  function backtrack(currentStr, openCount, closeCount) {
    if (currentStr.length === 2 * n) {
      results.push(currentStr);
      return;
    }

    if (openCount < n) {
      backtrack(currentStr + '(', openCount + 1, closeCount);
    }

    if (closeCount < openCount) {
      backtrack(currentStr + ')', openCount, closeCount + 1);
    }
  }

  backtrack('', 0, 0);
  return results;
}
```
</details>

### Exercise 2: Letter Combinations of a Phone Number
**Specification**: Given a string containing digits from `2-9` inclusive, return all possible letter combinations that the number could represent based on classic telephone keypads.

<details>
<summary>Solution</summary>

```javascript
export function letterCombinations(digits) {
  if (!digits || digits.length === 0) return [];

  const phoneMap = {
    '2': 'abc', '3': 'def', '4': 'ghi', '5': 'jkl',
    '6': 'mno', '7': 'pqrs', '8': 'tuv', '9': 'wxyz'
  };

  const results = [];
  const current = [];

  function backtrack(index) {
    if (index === digits.length) {
      results.push(current.join(''));
      return;
    }

    const letters = phoneMap[digits[index]];
    for (let i = 0; i < letters.length; i++) {
      current.push(letters[i]);
      backtrack(index + 1);
      current.pop();
    }
  }

  backtrack(0);
  return results;
}
```
</details>
