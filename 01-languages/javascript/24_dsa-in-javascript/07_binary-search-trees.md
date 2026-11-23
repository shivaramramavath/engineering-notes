# Binary Search Trees (BST) in JavaScript

> A production-grade algorithmic guide to Binary Search Trees in JavaScript, covering search/insert/delete operations, in-order successor replacement, boundary validation, and balanced tree invariants.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [06_trees-and-binary-trees.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/06_trees-and-binary-trees.md) |
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

While standard binary trees provide hierarchical modeling, searching for an element in an unsorted binary tree requires examining every node ($O(N)$ linear scan).

A **Binary Search Tree (BST)** organizes nodes following a strict ordering invariant:
$$\text{For every node } X: \quad \text{All keys in } \text{LeftSubtree}(X) < X.\text{key} < \text{All keys in } \text{RightSubtree}(X)$$

This ordering property enables **binary search behavior** on a linked data structure:
- **Fast Lookups**: Eliminates half of the remaining search space at every step, yielding **$O(\log N)$ average-case search, insertion, and deletion**.
- **Dynamic Sorted Data**: Maintains sorted order without requiring massive array re-allocations or shifts.
- **Efficient Range Queries**: Rapidly retrieves all elements within an interval $[A, B]$ in $O(K + \log N)$ time.

---

## 2. Core Concepts

### The BST Invariant
The invariant applies **globally across entire subtrees**, not merely between direct parents and children:
```
           [ 50 ]
          /      \
      [ 30 ]    [ 70 ]
      /    \    /    \
   [ 20 ] [40] [60]  [ 80 ]

Every node to the left of 50 is < 50.
Every node to the right of 50 is > 50.
```

### In-Order Traversal Invariant
An in-order traversal (`Left` $\to$ `Node` $\to$ `Right`) of any valid BST visits nodes in **strictly ascending sorted order**:
$$[ 20, 30, 40, 50, 60, 70, 80 ]$$

### Deletion Mechanics
Deleting a node from a BST involves three distinct cases:
1. **Leaf Node (0 children)**: Remove the node directly by setting its parent's pointer to `null`.
2. **Single Child (1 child)**: Bypass the node by linking its parent directly to its only child.
3. **Two Children (2 children)**: Replace the node's value with its **In-Order Successor** (the smallest value in its right subtree) or In-Order Predecessor, then delete that successor from the right subtree.

```
Deleting Node 50 (Two Children):
1. Find In-Order Successor in right subtree: 60 (minimum in right subtree).
2. Copy value 60 into node 50.
3. Delete the original leaf node 60 from the right subtree.
```

---

## 3. How It Works Internally

During search or insertion, the algorithm performs pointer transitions based on numeric comparisons:
```
search(target = 60, current = 50):
  60 > 50 ──► Move to current.right (70)
search(target = 60, current = 70):
  60 < 70 ──► Move to current.left (60)
search(target = 60, current = 60):
  60 === 60 ──► TARGET FOUND! Return node.
```

In a balanced BST, each step cuts the remaining search space in half ($H \approx \log_2 N$). In a degenerate BST (e.g. inserting already sorted numbers `[1, 2, 3, 4, 5]`), the tree forms a straight line ($H = N$), degrading operations to **$O(N)$ linear time**.

---

## 4. Syntax & API Reference

### Production-Grade Binary Search Tree Implementation
```javascript
export class BSTNode {
  constructor(val) {
    this.val = val;
    this.left = null;
    this.right = null;
  }
}

export class BinarySearchTree {
  constructor() {
    this.root = null;
  }

  insert(val) {
    const newNode = new BSTNode(val);
    if (!this.root) {
      this.root = newNode;
      return this;
    }

    let current = this.root;
    while (true) {
      if (val === current.val) return this; // Ignore duplicates or handle frequency

      if (val < current.val) {
        if (!current.left) {
          current.left = newNode;
          return this;
        }
        current = current.left;
      } else {
        if (!current.right) {
          current.right = newNode;
          return this;
        }
        current = current.right;
      }
    }
  }

  search(val) {
    let current = this.root;
    while (current !== null) {
      if (val === current.val) return current;
      current = val < current.val ? current.left : current.right;
    }
    return null;
  }

  delete(val) {
    this.root = this._deleteNode(this.root, val);
  }

  _deleteNode(node, val) {
    if (!node) return null;

    if (val < node.val) {
      node.left = this._deleteNode(node.left, val);
      return node;
    } else if (val > node.val) {
      node.right = this._deleteNode(node.right, val);
      return node;
    }

    // Node to delete found:
    // Case 1: Leaf node
    if (!node.left && !node.right) return null;

    // Case 2: One child
    if (!node.left) return node.right;
    if (!node.right) return node.left;

    // Case 3: Two children
    // Find in-order successor (minimum value in right subtree)
    let successor = node.right;
    while (successor.left !== null) {
      successor = successor.left;
    }

    node.val = successor.val;
    node.right = this._deleteNode(node.right, successor.val);
    return node;
  }
}
```

---

## 5. Practical Examples

### Example 1: Validate Binary Search Tree ($-\infty < x < \infty$ Bounds)
```javascript
export function isValidBST(root) {
  function validate(node, lower, upper) {
    if (!node) return true;

    // Node must strictly satisfy lower < node.val < upper
    if (node.val <= lower || node.val >= upper) {
      return false;
    }

    return (
      validate(node.left, lower, node.val) &&
      validate(node.right, node.val, upper)
    );
  }

  return validate(root, -Infinity, Infinity);
}
```

### Example 2: $K$-th Smallest Element in BST (In-Order Traversal)
```javascript
export function kthSmallest(root, k) {
  const stack = [];
  let current = root;
  let count = 0;

  while (current !== null || stack.length > 0) {
    while (current !== null) {
      stack.push(current);
      current = current.left;
    }

    current = stack.pop();
    count++;
    if (count === k) {
      return current.val;
    }

    current = current.right;
  }

  return -1;
}
```

### Example 3: Lowest Common Ancestor (LCA) in BST ($O(H)$ Time)
```javascript
export function lowestCommonAncestorBST(root, p, q) {
  let current = root;

  while (current !== null) {
    // If both nodes are smaller than root, LCA must be in left subtree
    if (p.val < current.val && q.val < current.val) {
      current = current.left;
    }
    // If both nodes are greater than root, LCA must be in right subtree
    else if (p.val > current.val && q.val > current.val) {
      current = current.right;
    }
    // Split point: one node is on left, one on right, or current matches p/q
    else {
      return current;
    }
  }

  return null;
}
```

### Example 4: Convert Sorted Array to Height-Balanced BST
```javascript
import { BSTNode } from './BinarySearchTree.js';

export function sortedArrayToBST(nums) {
  function buildBalanced(left, right) {
    if (left > right) return null;

    // Choose middle element as root to ensure equal subtree sizes
    const mid = left + Math.floor((right - left) / 2);
    const node = new BSTNode(nums[mid]);

    node.left = buildBalanced(left, mid - 1);
    node.right = buildBalanced(mid + 1, right);

    return node;
  }

  return buildBalanced(0, nums.length - 1);
}
```

### Example 5: In-Order Successor in BST
```javascript
export function inorderSuccessor(root, p) {
  let successor = null;
  let current = root;

  while (current !== null) {
    if (p.val < current.val) {
      // Current node is a candidate successor; search left for closer candidate
      successor = current;
      current = current.left;
    } else {
      current = current.right;
    }
  }

  return successor;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **The Flawed Local BST Validation Trap**:
   Checking only immediate children:
   ```javascript
   // INCORRECT: Only checks direct children, NOT whole subtree!
   if (node.left && node.left.val >= node.val) return false;
   if (node.right && node.right.val <= node.val) return false;
   ```
   A tree can satisfy local parent-child relations while violating global subtree invariants (e.g. `[10, 5, 15, null, null, 6, 20]`: node `6` is a valid left child of `15`, but resides in `10`'s right subtree where all values must be $>10$!). Always pass **upper and lower bounds** (`validate(node, min, max)`).
2. **Duplicate Value Inconsistencies**:
   Standard BSTs define strict inequalities ($<$ and $>$). If duplicates are permitted, choose a consistent convention (e.g. $\le$ goes to the left) or maintain a `count` frequency property on each node to keep the tree invariant strictly unique.
3. **Degeneration on Pre-Sorted Input**:
   Inserting an already sorted array `[1, 2, 3, 4, 5]` into a standard BST without self-balancing yields a degenerate tree of height $N$. All searches and inserts become $O(N)$. Self-balancing trees (AVL or Red-Black trees) resolve this via tree rotations.
4. **JavaScript Floating-Point Comparisons**:
   If storing floating-point numbers in a BST, be mindful of precision issues (`0.1 + 0.2 !== 0.3`). Always compare using an epsilon tolerance ($\epsilon = 10^{-9}$) if exact precision is required.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Risk | Production Fix |
| :--- | :--- | :--- |
| Validating BST only against direct child values | Allows invalid subtrees to pass validation undetected. | Pass down strict dynamic bounds (`lower, upper`) across all recursive calls. |
| Inserting sorted data without balancing | Degenerates BST into an $O(N)$ linked list. | Use Divide and Conquer (`sortedArrayToBST`) or a self-balancing tree. |
| Forgetting to re-link parent after node deletion | Orphaned subtrees; changes fail to persist in tree. | Re-assign returned node pointer (`node.left = deleteNode(...)`). |
| Inefficient LCA using full tree traversal | Fails to exploit BST ordering property, wasting CPU. | Use BST LCA algorithm checking numeric split point in $O(H)$. |
| Recursive search without tail-call optimization | Deep skewed trees trigger call stack overflow. | Use iterative `while (current !== null)` search loops. |
| Allowing duplicate insertions to duplicate nodes | Skews tree height unnecessarily. | Add a `count` property on node to handle duplicates cleanly. |

---

## 8. Performance Characteristics

| Operation | Balanced BST (AVL/Red-Black) | Average BST | Degenerate BST (Worst) |
| :--- | :--- | :--- | :--- |
| **Search** | $O(\log N)$ | $O(\log N)$ | $O(N)$ |
| **Insertion** | $O(\log N)$ | $O(\log N)$ | $O(N)$ |
| **Deletion** | $O(\log N)$ | $O(\log N)$ | $O(N)$ |
| **Space Complexity** | $O(N)$ | $O(N)$ | $O(N)$ |

---

## 9. Engine Internals

### V8 Branch Prediction & Pointer Jumps
- **CPU Branch Prediction**: When traversing a BST in a tight loop (`if (val < current.val)`), the CPU's branch prediction unit predicts whether execution branches left or right. In balanced trees with uniformly distributed queries, branch predictors achieve ~50% prediction accuracy, resulting in occasional CPU pipeline flushes.
- **Node Allocation in V8**: Constructing `BSTNode` instances initializes pointers on the V8 heap. To avoid creating polymorphic hidden classes, initialize all node properties in the exact same order in constructor (`val`, `left`, `right`).

---

## 10. Best Practices & Design Guidelines

1. **Leverage In-Order Traversal for Sorting**: Never extract BST elements and call `array.sort()`; an in-order traversal already yields sorted data in $O(N)$ time.
2. **Use Iteration for Search Operations**: Iterative BST search uses $O(1)$ auxiliary stack memory and cannot cause call stack overflow.
3. **Use BST for Range Queries**: Prune subtrees entirely if their range lies outside query bounds $[L, R]$.
4. **Balance Trees When Pre-Building**: Always build static search trees from sorted arrays by picking the middle element as root.

---

## 11. Real-World Case Study

### High-Speed Product Catalog Price Range Index
**Scenario**: An e-commerce platform hosted 200,000 products. Shoppers frequently filtered products using dynamic price range sliders (e.g. find all products between $\$25.00$ and $\$75.00$).
**Problem**: An unindexed array filter `products.filter(p => p.price >= min && p.price <= max)` took $18\text{ms}$ per query. Under 1,000 concurrent shoppers, Node.js event loops froze.

**Solution**:
- Indexed product IDs into a balanced BST ordered by price.
- Implemented a range query function `findRange(root, min, max)` that traverses only relevant branches:
  - If `node.val < min`, prune entire left subtree.
  - If `node.val > max`, prune entire right subtree.
  - Otherwise, collect `node` and explore both subtrees.

**Outcome**:
- Query execution dropped from $18\text{ms}$ down to $0.08\text{ms}$ ($80\mu\text{s}$) per filter request.
- Throughput increased by over 200x on the catalog service.

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental invariant of a Binary Search Tree?</summary>

For every node $X$ in a Binary Search Tree:
- All values in the left subtree of $X$ must be strictly less than $X.\text{val}$.
- All values in the right subtree of $X$ must be strictly greater than $X.\text{val}$.
- Both the left and right subtrees must themselves be valid binary search trees.
This invariant must hold globally across all ancestor and descendant levels, not just between direct parent and child nodes.
</details>

<details>
<summary>2. Why does checking only <code>node.left.val < node.val < node.right.val</code> fail to validate a BST?</summary>

Because the BST invariant applies to entire subtrees, not just direct children. For example, consider a tree where Root is 10, Right child is 15, and 15's Left child is 6. Locally, 6 is less than 15 (valid for 15's left child). However, node 6 resides in the right subtree of root 10, where all elements must be greater than 10. Validating only immediate children misses this global violation. A correct validator must pass down dynamic bounds (`lower, upper`).
</details>

<details>
<summary>3. What is an In-Order Successor in a BST, and how is it found?</summary>

The in-order successor of a node is the node with the smallest value that is greater than the given node's value (the node that immediately follows it in an in-order traversal).
- If the node has a **right subtree**: its successor is the leftmost node of its right subtree (`curr = node.right; while (curr.left) curr = curr.left;`).
- If the node has **no right subtree**: traverse from the root, recording the last node where you branched left to find the target.
</details>

<details>
<summary>4. What are the three cases when deleting a node from a BST?</summary>

1. **Node is a leaf (no children)**: Remove the node directly by setting its parent's pointer to `null`.
2. **Node has one child**: Bypass the node by pointing its parent directly to its single child.
3. **Node has two children**: Find its in-order successor (the smallest node in its right subtree). Copy the successor's value into the target node, then recursively delete the successor from the right subtree (which is guaranteed to have at most one child).
</details>

<details>
<summary>5. How does Lowest Common Ancestor (LCA) in a BST differ from LCA in a general binary tree?</summary>

In a general binary tree, finding LCA requires searching both subtrees of every node ($O(N)$ time). In a BST, we can leverage the ordering property to achieve $O(H)$ time:
- If both target nodes `p` and `q` are smaller than current node, LCA must be in the left subtree.
- If both are greater, LCA must be in the right subtree.
- The first node where `p` and `q` diverge (one is $\le$ and the other is $\ge$) is guaranteed to be their LCA.
</details>

<details>
<summary>6. How do you convert a sorted array into a height-balanced BST in $O(N)$ time?</summary>

Using a recursive Divide and Conquer strategy:
1. Find the middle element of the array segment: `mid = Math.floor((left + right) / 2)`.
2. Create a new `TreeNode` with the middle element as value.
3. Recursively construct the left subtree from `nums[left ... mid - 1]`.
4. Recursively construct the right subtree from `nums[mid + 1 ... right]`.
Because the middle element evenly splits the remaining elements at each step, the resulting tree is guaranteed to be height-balanced ($H = \lfloor \log_2 N \rfloor$).
</details>

<details>
<summary>7. What is the time complexity of finding the $K$-th smallest element in a BST?</summary>

Using an iterative in-order traversal with an explicit stack, the time complexity is **$O(H + k)$**, where $H$ is the tree height. We traverse down to the leftmost leaf ($H$ steps) and then pop $k$ elements. In a balanced BST, this is $O(\log N + k)$, which is significantly faster than sorting an entire array ($O(N \log N)$).
</details>

<details>
<summary>8. Why is a self-balancing tree (like AVL or Red-Black) necessary in production?</summary>

In an unbalanced BST, inserting sequential or sorted data degrades tree height from $O(\log N)$ to $O(N)$, turning searches and insertions into linear scans. Self-balancing trees execute tree rotations (left/right rotations) during insertions and deletions to guarantee that the tree height never exceeds $O(\log N)$, ensuring deterministic sub-millisecond lookups regardless of insertion order.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Search in a Binary Search Tree
**Specification**: Given the root of a binary search tree and an integer `val`, find the node in the BST that the node's value equals `val` and return the subtree rooted with that node. If such a node does not exist, return `null`. Implement iteratively in $O(1)$ auxiliary space.

<details>
<summary>Solution</summary>

```javascript
export function searchBST(root, val) {
  let current = root;

  while (current !== null) {
    if (current.val === val) {
      return current;
    }
    current = val < current.val ? current.left : current.right;
  }

  return null;
}
```
</details>

### Exercise 2: Range Sum of BST
**Specification**: Given the root node of a binary search tree and two integers `low` and `high`, return the sum of values of all nodes with a value in the inclusive range `[low, high]`. Prune subtrees that cannot contain valid values.

<details>
<summary>Solution</summary>

```javascript
export function rangeSumBST(root, low, high) {
  if (!root) return 0;

  // If current value is strictly greater than high, prune right subtree
  if (root.val > high) {
    return rangeSumBST(root.left, low, high);
  }

  // If current value is strictly less than low, prune left subtree
  if (root.val < low) {
    return rangeSumBST(root.right, low, high);
  }

  // Current value is in range: include it and explore both subtrees
  return (
    root.val +
    rangeSumBST(root.left, low, high) +
    rangeSumBST(root.right, low, high)
  );
}
```
</details>
