# Trees & Binary Trees in JavaScript

> A production-grade algorithmic guide to Hierarchical Trees and Binary Trees in JavaScript, featuring recursive/iterative DFS, BFS level-order traversals, LCA algorithms, and AST/DOM engines.

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

Linear data structures (arrays, linked lists) organize data sequentially. However, real-world data is inherently hierarchical:
- **The Browser DOM**: An HTML document is a nested tree of element nodes (`<html>` $\to$ `<body>` $\to$ `<div>`).
- **Compiler Abstract Syntax Trees (AST)**: JavaScript parsers (Babel, Acorn, SWC) convert source code into AST trees to evaluate syntax, optimize bytecodes, and execute transformations.
- **Hierarchical Storage**: JSON schemas, filesystem directory structures, and UI component render trees.

A **Tree** is a non-linear, acyclic connected graph where each child node has exactly one parent node, originating from a single **Root** node. A **Binary Tree** restricts each node to at most two children (`left` and `right`).

---

## 2. Core Concepts

### Binary Tree Classifications
- **Full Binary Tree**: Every node has either $0$ or $2$ children.
- **Complete Binary Tree**: Every level is completely filled, except possibly the last, which is filled from left to right (crucial for Binary Heaps).
- **Perfect Binary Tree**: All interior nodes have $2$ children and all leaf nodes reside at the identical depth. Total nodes $N = 2^{H+1} - 1$.
- **Balanced Binary Tree**: The height difference between the left and right subtrees of any node is at most $1$. Guarantees $O(\log N)$ search and insertion.

```
       [ Root: 1 ]
        /        \
   [ Node: 2 ]  [ Node: 3 ]
     /     \         \
 [Leaf:4] [Leaf:5]  [Leaf:6]
```

### Traversal Strategies
1. **Depth-First Search (DFS)**: Explores branches as deep as possible before backtracking:
   - **Pre-order**: Node $\to$ Left $\to$ Right (Used for tree cloning and serialization).
   - **In-order**: Left $\to$ Node $\to$ Right (Yields sorted order in a Binary Search Tree).
   - **Post-order**: Left $\to$ Right $\to$ Node (Used for tree deletion and bottom-up metric calculation).
2. **Breadth-First Search (BFS)**: Explores nodes level-by-level using a FIFO queue.

---

## 3. How It Works Internally

```
DFS Call Stack Progression:
1. visit(Root)
2.   visit(Root.left)
3.     visit(Root.left.left) ──► Reaches null (backtrack)
4.   visit(Root.left.right)
5. visit(Root.right)

BFS Level Queue Progression:
Queue: [ Root ]
Pop Root, Enqueue children: [ LeftChild, RightChild ]
Pop LeftChild, Enqueue children: [ RightChild, LeftGrandChild1, LeftGrandChild2 ]
```

In a balanced tree with $N$ nodes, DFS consumes **$O(H) = O(\log N)$** call stack memory. In a degenerate skewed tree (like a linked list), height $H = N$, consuming **$O(N)$** stack memory.

---

## 4. Syntax & API Reference

### Core Tree Node Class & Traversal Engine
```javascript
export class TreeNode {
  constructor(val = 0, left = null, right = null) {
    this.val = val;
    this.left = left;
    this.right = right;
  }
}

/**
 * Iterative In-Order Traversal using an explicit Stack.
 * Eliminates V8 call stack overflow risks.
 * Time: O(N) | Space: O(H)
 */
export function inOrderIterative(root) {
  const result = [];
  const stack = [];
  let current = root;

  while (current !== null || stack.length > 0) {
    // Traverse as far left as possible
    while (current !== null) {
      stack.push(current);
      current = current.left;
    }

    current = stack.pop();
    result.push(current.val);
    current = current.right;
  }

  return result;
}
```

---

## 5. Practical Examples

### Example 1: Level-Order Traversal (BFS Returning 2D Level Arrays)
```javascript
import { FastQueue } from './05_stacks-and-queues.js';

export function levelOrder(root) {
  if (!root) return [];

  const result = [];
  const queue = new FastQueue();
  queue.enqueue(root);

  while (!queue.isEmpty()) {
    const levelSize = queue.size;
    const currentLevel = [];

    for (let i = 0; i < levelSize; i++) {
      const node = queue.dequeue();
      currentLevel.push(node.val);

      if (node.left) queue.enqueue(node.left);
      if (node.right) queue.enqueue(node.right);
    }

    result.push(currentLevel);
  }

  return result;
}
```

### Example 2: Maximum Depth / Height of Binary Tree (Post-Order DFS)
```javascript
export function maxDepth(root) {
  if (!root) return 0;

  const leftDepth = maxDepth(root.left);
  const rightDepth = maxDepth(root.right);

  return Math.max(leftDepth, rightDepth) + 1;
}
```

### Example 3: Invert / Mirror a Binary Tree
```javascript
export function invertTree(root) {
  if (!root) return null;

  // Swap left and right children
  const temp = root.left;
  root.left = root.right;
  root.right = temp;

  invertTree(root.left);
  invertTree(root.right);

  return root;
}
```

### Example 4: Lowest Common Ancestor (LCA) in a Binary Tree
```javascript
export function lowestCommonAncestor(root, p, q) {
  // Base case: if root is null or matches either target node
  if (!root || root === p || root === q) {
    return root;
  }

  const left = lowestCommonAncestor(root.left, p, q);
  const right = lowestCommonAncestor(root.right, p, q);

  // If both left and right return non-null, root is the split point (LCA)
  if (left && right) {
    return root;
  }

  return left !== null ? left : right;
}
```

### Example 5: Validate Symmetric (Mirror) Binary Tree
```javascript
export function isSymmetric(root) {
  if (!root) return true;

  function isMirror(t1, t2) {
    if (!t1 && !t2) return true;
    if (!t1 || !t2) return false;

    return (
      t1.val === t2.val &&
      isMirror(t1.left, t2.right) &&
      isMirror(t1.right, t2.left)
    );
  }

  return isMirror(root.left, root.right);
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Stack Overflow on Degenerate (Skewed) Trees**:
   If a binary tree is skewed (every node has only a right child, mimicking a linked list), a recursive traversal of $15,000$ nodes will exceed V8's call stack limit ($\approx 10,000$ frames) and crash with `RangeError: Maximum call stack size exceeded`. Always use iterative traversals with an explicit stack for deep or user-generated trees.
2. **BFS Queue Memory Blowup on Wide Trees**:
   In a perfect binary tree of $N$ nodes, the last level contains $\frac{N}{2}$ leaf nodes. In a tree with $1,000,000$ nodes, a BFS queue must hold $500,000$ active node references simultaneously at the final level ($O(W) = O(N)$ memory).
3. **Null Root Guards**:
   Always check `if (!root) return ...` at the very top of recursive and iterative routines. Accessing `root.left` on `null` throws an uncaught TypeError.
4. **Distinguishing Tree Identity vs Value Equality**:
   In algorithms like LCA (`lowestCommonAncestor`), comparisons should evaluate node identity references (`root === p`), not numerical equality (`root.val === p.val`), since distinct tree nodes can share identical values.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Hazard | Production Fix |
| :--- | :--- | :--- |
| Unbounded recursive DFS on deep trees | Crashes runtime with `RangeError` (Call Stack Overflow). | Use iterative DFS with an explicit array stack. |
| `array.shift()` inside BFS level loop | Degrades BFS from $O(N)$ linear to $O(N^2)$ quadratic. | Use a fast linked-list Queue with $O(1)$ dequeue. |
| Forgetting to save queue level size in BFS | Inner loop evaluates dynamic `queue.length`, breaking level segmentation. | Snapshot `const levelSize = queue.size` before iterating. |
| Value comparison (`node.val === target.val`) for LCA | Fails when multiple distinct nodes hold identical values. | Compare reference identity (`node === target`). |
| Inverting subtree before storing pointer | Overwrites one branch with the other, corrupting tree. | Swap pointers using destructuring or temporary variables. |
| Re-calculating subtree heights redundantly | Turns $O(N)$ balance check into an $O(N^2)$ traversal. | Compute balance bottom-up in a single post-order pass. |

---

## 8. Performance Characteristics

- **Time Complexity**:
  - All traversals (Pre-order, In-order, Post-order, Level-order) visit each of the $N$ nodes exactly once: **$O(N)$**.
- **Space Complexity**:
  - **DFS (Call Stack)**: $O(H)$, where $H$ is tree height.
    - Balanced Tree: $H = \log_2 N \implies O(\log N)$ memory.
    - Degenerate Tree: $H = N \implies O(N)$ memory.
  - **BFS (Queue)**: $O(W)$, where $W$ is the maximum width (number of leaves at deepest level). In a balanced tree, $W \approx \frac{N}{2} \implies O(N)$ memory.

---

## 9. Engine Internals

### V8 Hidden Classes & DOM Tree Host Objects
- **Shape Invariance**: If tree nodes are constructed with identical properties (`this.val`, `this.left`, `this.right`), V8 assigns them a single monomorphic Hidden Class. Adding dynamic ad-hoc properties (`node.isVisited = true`) forces V8 to allocate transition shapes, deoptimizing traversal loops. Use a `Set` or `WeakSet` to track visited states rather than mutating node shapes.
- **Blink DOM vs V8 Objects**: In the browser, DOM tree nodes are C++ Blink objects. When JavaScript accesses `node.childNodes` or `node.parentElement`, V8 constructs a JavaScript wrapper object bridging the C++ DOM heap.

---

## 10. Best Practices & Design Guidelines

1. **Use In-Order Traversal for Sorting**: In a BST, in-order traversal yields nodes in strictly non-decreasing order.
2. **Use Post-Order for Bottom-Up Aggregations**: Computing subtree sizes, heights, or memory footprints requires evaluating children before the parent node.
3. **Use Level-Order for Shortest Paths**: Finding the minimum depth to a leaf node is fastest using BFS because it terminates immediately upon reaching the first leaf without exploring deep subtrees.
4. **Use Iterative Traversals for Production Safety**: Guard against stack overflow on arbitrary tree depths.

---

## 11. Real-World Case Study

### Virtual DOM Tree Reconciliation (React Fiber Engine)
**Scenario**: In modern frontend frameworks (React), the UI is modeled as a tree of Virtual DOM nodes. When state changes, the framework builds a new VDOM tree and must determine the minimal set of mutations to update the real DOM.
**Algorithm**:
- Naive tree edit distance algorithms (such as the Zhang-Shasha algorithm) operate in **$O(N^3)$** time complexity. For a UI with 1,000 nodes, $1,000^3 = 1,000,000,000$ operations would freeze the browser for seconds.
- React uses an **$O(N)$ Heuristic Tree Diffing Algorithm**:
  1. Two elements of different component types produce different trees (subtrees are replaced without traversal).
  2. Lists of child nodes are reconciled using stable `key` properties via hash map lookups ($O(1)$).
  3. The diffing engine traverses the tree in a single linear pass ($O(N)$).

**Outcome**:
- Enables 60 FPS re-renders even in complex web applications with tens of thousands of component nodes.

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between Pre-Order, In-Order, and Post-Order traversals?</summary>

- **Pre-order (Root, Left, Right)**: Processes the current node before visiting its subtrees. Ideal for copying, cloning, or serializing a tree structure.
- **In-order (Left, Root, Right)**: Visits the left subtree, processes current node, then visits the right subtree. In a Binary Search Tree (BST), this visits elements in strictly sorted ascending order.
- **Post-order (Left, Right, Root)**: Explores both child subtrees before processing the current node. Ideal for bottom-up calculations (e.g. deleting a tree, computing subtree heights, evaluating directory disk usage).
</details>

<details>
<summary>2. Why does Breadth-First Search (BFS) use a Queue while Depth-First Search (DFS) uses a Stack?</summary>

- **BFS** explores nodes layer by layer horizontally. To visit all nodes at distance $d$ before any node at distance $d+1$, nodes must be processed in the exact order they were discovered, which requires a **FIFO (First-In, First-Out) Queue**.
- **DFS** explores deep vertical paths down to the leaves before backtracking. Backtracking requires returning to the most recently visited branching node, which requires a **LIFO (Last-In, First-Out) Stack** (either the language Call Stack or an explicit array stack).
</details>

<details>
<summary>3. What is the maximum number of nodes in a Binary Tree of height $H$?</summary>

For a binary tree of height $H$ (where the root is at height 0), the maximum number of nodes occurs in a **Perfect Binary Tree**, which is:
$$N_{\text{max}} = 2^0 + 2^1 + 2^2 + \dots + 2^H = 2^{H+1} - 1$$
Conversely, the minimum height of a tree with $N$ nodes is $\lfloor \log_2 N \rfloor$.
</details>

<details>
<summary>4. What is the difference between a Complete Binary Tree and a Full Binary Tree?</summary>

- **Full Binary Tree**: Every node has either 0 or 2 children (no node has exactly 1 child).
- **Complete Binary Tree**: Every level is completely filled, except possibly the last level, and all nodes on the last level are packed as far left as possible. Complete binary trees can be compactly represented in contiguous arrays without pointer overhead (as in Binary Heaps).
</details>

<details>
<summary>5. How can you determine if a Binary Tree is height-balanced in $O(N)$ time?</summary>

Use a bottom-up post-order DFS. Have the helper function return the height of the subtree if balanced, or `-1` if unbalanced. At each node:
1. Check left subtree: if left returns `-1`, return `-1`.
2. Check right subtree: if right returns `-1`, return `-1`.
3. If `Math.abs(left - right) > 1`, return `-1`.
4. Otherwise, return `Math.max(left, right) + 1`.
Because each node is visited only once, this runs in strict $O(N)$ time, avoiding the $O(N^2)$ trap of re-computing heights from the top down.
</details>

<details>
<summary>6. How does the iterative In-Order traversal work using an explicit Stack?</summary>

Initialize an empty `stack = []` and pointer `curr = root`.
Loop `while (curr !== null || stack.length > 0)`:
1. Inside, push `curr` onto stack and advance `curr = curr.left` until `curr` is null (reaching leftmost node).
2. Pop node from stack: `curr = stack.pop()`.
3. Record `curr.val`.
4. Advance to right child: `curr = curr.right`.
This mirrors the recursive call stack iteratively without risking call stack overflow.
</details>

<details>
<summary>7. What causes a Call Stack Overflow during recursive tree operations in V8?</summary>

In V8, each function call adds a frame to the call stack. The engine caps call stack size at approximately 10,000 frames to prevent host machine crashes. If a tree is unbalanced (e.g. 15,000 nodes linked linearly like a linked list), a recursive DFS recurses 15,000 frames deep, exceeding the memory limit and throwing `RangeError: Maximum call stack size exceeded`.
</details>

<details>
<summary>8. How do you find the Lowest Common Ancestor (LCA) in a general binary tree?</summary>

Recursively traverse the tree:
- If current node is `null`, `p`, or `q`, return current node.
- Recurse on left and right subtrees: `left = lca(root.left, p, q)` and `right = lca(root.right, p, q)`.
- If both `left` and `right` return non-null, `root` is the node where `p` and `q` diverge, meaning `root` is the LCA.
- If only one is non-null, return the non-null result.
Time complexity is $O(N)$ and space complexity is $O(H)$.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Same Tree
**Specification**: Given the roots of two binary trees `p` and `q`, write a function to check if they are structurally identical and have the same node values.

<details>
<summary>Solution</summary>

```javascript
export function isSameTree(p, q) {
  if (!p && !q) return true;
  if (!p || !q) return false;
  if (p.val !== q.val) return false;

  return isSameTree(p.left, q.left) && isSameTree(p.right, q.right);
}
```
</details>

### Exercise 2: Binary Tree Zigzag Level Order Traversal
**Specification**: Given the root of a binary tree, return the zigzag level order traversal of its nodes' values (i.e., from left to right, then right to left for the next level and alternate between).

<details>
<summary>Solution</summary>

```javascript
import { FastQueue } from './05_stacks-and-queues.js';

export function zigzagLevelOrder(root) {
  if (!root) return [];

  const result = [];
  const queue = new FastQueue();
  queue.enqueue(root);
  let leftToRight = true;

  while (!queue.isEmpty()) {
    const levelSize = queue.size;
    const currentLevel = new Array(levelSize);

    for (let i = 0; i < levelSize; i++) {
      const node = queue.dequeue();
      // Place element at left or right boundary based on zigzag flag
      const index = leftToRight ? i : levelSize - 1 - i;
      currentLevel[index] = node.val;

      if (node.left) queue.enqueue(node.left);
      if (node.right) queue.enqueue(node.right);
    }

    result.push(currentLevel);
    leftToRight = !leftToRight;
  }

  return result;
}
```
</details>
