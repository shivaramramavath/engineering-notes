# Binary Trees and Binary Search Trees (BST) in Java

> Hierarchical data structures, DFS traversals (Inorder, Preorder, Postorder), BFS Level-Order, BST invariants, LCA (Lowest Common Ancestor), and serialization algorithms in Java.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Advanced |
| **Prerequisites** | [`27-dsa/05_recursion-and-backtracking.md`](05_recursion-and-backtracking.md), [`27-dsa/03_stacks-and-queues.md`](03_stacks-and-queues.md) |
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

Trees are hierarchical, non-linear data structures consisting of nodes connected by directed edges. A **Binary Tree** restricts every node to at most two children (left and right).

A **Binary Search Tree (BST)** imposes a strict ordering invariant:
$$\forall \text{ node } X, \quad \text{key}(\text{leftSubtree}) < \text{key}(X) < \text{key}(\text{rightSubtree})$$
This invariant guarantees that an **In-Order Traversal** visits keys in strictly ascending sorted order, enabling $O(h)$ average lookup, insertion, and deletion where $h$ is tree height.

---

## 2. Core Concepts

### 2.1 Traversal Taxonomies

1. **Depth-First Search (DFS)**:
   - **In-Order** (Left, Root, Right): Returns sorted sequence for BST.
   - **Pre-Order** (Root, Left, Right): Ideal for serialization and cloning tree hierarchies.
   - **Post-Order** (Left, Right, Root): Ideal for bottom-up aggregation (e.g., calculating tree height, deleting tree nodes).

2. **Breadth-First Search (BFS / Level-Order)**:
   - Uses an auxiliary FIFO queue to traverse nodes level by level horizontally.

```
       [ 4 ]
      /     \
   [ 2 ]   [ 6 ]
   /   \   /   \
 [ 1 ] [ 3 ] [ 5 ] [ 7 ]

In-Order:   1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 (Sorted!)
Pre-Order:  4 -> 2 -> 1 -> 3 -> 6 -> 5 -> 7
Post-Order: 1 -> 3 -> 2 -> 5 -> 7 -> 6 -> 4
BFS Level:  Level 0: [4], Level 1: [2, 6], Level 2: [1, 3, 5, 7]
```

---

## 3. How It Works Internally

### 3.1 Tree Degeneration & Height Complexity

```
BALANCED TREE (Height = O(log N)):
       (4)
      /   \
    (2)   (6)
    / \   / \
   (1)(3)(5)(7)
Search: O(log N) steps.

DEGENERATE TREE / SKEWED (Height = O(N)):
   (1)
     \
     (2)
       \
       (3)
         \
         (4)
Search: Degrades to O(N) linked list!
```

---

## 4. Syntax & Usage

### 4.1 Canonical Java TreeNode Definition

```java
public class TreeNode {
    public int val;
    public TreeNode left;
    public TreeNode right;

    public TreeNode() {}
    public TreeNode(int val) { this.val = val; }
    public TreeNode(int val, TreeNode left, TreeNode right) {
        this.val = val;
        this.left = left;
        this.right = right;
    }
}
```

---

## 5. Practical Examples

### 5.1 Pattern 1: Validate Binary Search Tree (LeetCode 98)
```java
public class ValidateBst {
    public static boolean isValidBST(TreeNode root) {
        return validate(root, null, null);
    }

    private static boolean validate(TreeNode node, Integer min, Integer max) {
        if (node == null) return true;

        if ((min != null && node.val <= min) || (max != null && node.val >= max)) {
            return false;
        }

        return validate(node.left, min, node.val) && validate(node.right, node.val, max);
    }
}
```

### 5.2 Pattern 2: Level-Order Traversal BFS (LeetCode 102)
```java
import java.util.*;

public class LevelOrderTraversal {
    public static List<List<Integer>> levelOrder(TreeNode root) {
        List<List<Integer>> results = new ArrayList<>();
        if (root == null) return results;

        Queue<TreeNode> queue = new ArrayDeque<>();
        queue.offer(root);

        while (!queue.isEmpty()) {
            int levelSize = queue.size();
            List<Integer> currentLevel = new ArrayList<>(levelSize);

            for (int i = 0; i < levelSize; i++) {
                TreeNode node = queue.poll();
                currentLevel.add(node.val);

                if (node.left != null) queue.offer(node.left);
                if (node.right != null) queue.offer(node.right);
            }
            results.add(currentLevel);
        }
        return results;
    }
}
```

### 5.3 Pattern 3: Lowest Common Ancestor in Binary Tree (LeetCode 236)
```java
public class LowestCommonAncestor {
    public static TreeNode lowestCommonAncestor(TreeNode root, TreeNode p, TreeNode q) {
        if (root == null || root == p || root == q) return root;

        TreeNode left = lowestCommonAncestor(root.left, p, q);
        TreeNode right = lowestCommonAncestor(root.right, p, q);

        if (left != null && right != null) return root; // p and q are in separate branches
        return (left != null) ? left : right;
    }
}
```

### 5.4 Pattern 4: Diameter of Binary Tree (LeetCode 543)
```java
public class DiameterOfBinaryTree {
    private static int maxDiameter = 0;

    public static int diameterOfBinaryTree(TreeNode root) {
        maxDiameter = 0;
        calculateHeight(root);
        return maxDiameter;
    }

    private static int calculateHeight(TreeNode node) {
        if (node == null) return 0;

        int leftH = calculateHeight(node.left);
        int rightH = calculateHeight(node.right);

        maxDiameter = Math.max(maxDiameter, leftH + rightH);
        return 1 + Math.max(leftH, rightH);
    }
}
```

### 5.5 Pattern 5: Serialize and Deserialize Binary Tree (LeetCode 297)
```java
import java.util.*;

public class Codec {
    // Encodes a tree to a single string (Preorder with '#' for nulls)
    public String serialize(TreeNode root) {
        StringBuilder sb = new StringBuilder();
        buildString(root, sb);
        return sb.toString();
    }

    private void buildString(TreeNode node, StringBuilder sb) {
        if (node == null) {
            sb.append("#,");
            return;
        }
        sb.append(node.val).append(",");
        buildString(node.left, sb);
        buildString(node.right, sb);
    }

    // Decodes encoded string back to tree
    public TreeNode deserialize(String data) {
        Queue<String> nodes = new ArrayDeque<>(Arrays.asList(data.split(",")));
        return buildTree(nodes);
    }

    private TreeNode buildTree(Queue<String> nodes) {
        String val = nodes.poll();
        if (val == null || val.equals("#")) return null;

        TreeNode node = new TreeNode(Integer.parseInt(val));
        node.left = buildTree(nodes);
        node.right = buildTree(nodes);
        return node;
    }
}
```

### 5.6 Pattern 6: Maximum Path Sum in Binary Tree (LeetCode 124)
```java
public class MaxPathSumTree {
    private static int maxSum = Integer.MIN_VALUE;

    public static int maxPathSum(TreeNode root) {
        maxSum = Integer.MIN_VALUE;
        maxGain(root);
        return maxSum;
    }

    private static int maxGain(TreeNode node) {
        if (node == null) return 0;

        // Max sum on subtrees (ignore negative sums)
        int leftGain = Math.max(0, maxGain(node.left));
        int rightGain = Math.max(0, maxGain(node.right));

        int priceNewPath = node.val + leftGain + rightGain;
        maxSum = Math.max(maxSum, priceNewPath);

        return node.val + Math.max(leftGain, rightGain);
    }
}
```

---

## 6. Best Practices

1. **Pass Valid Range Boundaries, Not Just Parent**: In BST validation, checking `node.val > node.left.val` is insufficient; the entire left subtree must be less than the root ancestor! Pass `(min, max)` bounds.
2. **Handle Null Pointers Early**: Always check `if (node == null)` as the first base case line in recursive tree methods.
3. **Use Integer Min/Max Sentinels with Null**: When validating BSTs with `Integer.MIN_VALUE` or `Integer.MAX_VALUE`, use `Integer` objects initialized to `null` to prevent false negatives.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Local BST Check** | Only checks `node.left.val < node.val`, missing ancestor violations | Pass min/max bounds: `validate(node, min, max)` |
| **Level-Order Size Drift** | Using `queue.size()` dynamically inside `for (int i=0; i<queue.size(); i++)` changes size as children are added | Save size before loop: `int levelSize = queue.size();` |
| **NPE on Null Leaves** | Dereferencing `node.left.val` without checking `node.left != null` | Check `node.left != null` before dereferencing |
| **Diameter Counts Nodes Not Edges**| Forgetting that diameter is defined as number of *edges* | Diameter = `leftHeight + rightHeight` (edges between nodes) |

---

## 8. Performance & Security Notes

### 8.1 Stack Overflow from Skewed Trees
A degenerate tree with $100,000$ nodes causes $100,000$ nested stack frames, immediately crashing with `StackOverflowError`. For guaranteed $O(\log N)$ depth, always use self-balancing trees (AVL or Red-Black).

---

## 9. Comparison / When to Use What

| Traversal | Order | Common Use Case |
| :--- | :--- | :--- |
| **Pre-Order** | Root $\to$ Left $\to$ Right | Serialization, copying tree, prefix expressions |
| **In-Order** | Left $\to$ Root $\to$ Right | BST sorted element retrieval |
| **Post-Order** | Left $\to$ Right $\to$ Root | Bottom-up calculations (height, deletion, directory size) |
| **Level-Order**| Top $\to$ Bottom, Left $\to$ Right | Shortest path, zigzag traversal, printing levels |

---

## 10. Interview Questions

<details>
<summary>1. Why is checking `node.val > node.left.val` insufficient to validate a BST?</summary>
In a valid BST, all nodes in the left subtree must be strictly less than all ancestor nodes above them. A tree where root is 10, right child is 15, and right child's left child is 6 satisfies local parent-child checks, but 6 is less than root 10 in the right subtree, violating the global BST property.
</details>

<details>
<summary>2. How do you find the Lowest Common Ancestor (LCA) in a BST vs a general Binary Tree?</summary>
In a BST, we can use the ordering invariant: if both $p$ and $q$ are smaller than root, LCA is in the left subtree; if both are larger, LCA is in the right subtree; if root splits $p$ and $q$, root IS the LCA ($O(h)$ time). In a general binary tree, we must search both subtrees recursively ($O(N)$ time).
</details>

<details>
<summary>3. What is the space complexity of BFS Level-Order Traversal?</summary>
$O(W)$ where $W$ is the maximum width of the binary tree. For a full binary tree with $N$ nodes, the leaf level contains $\lceil N/2 \rceil$ nodes, requiring $O(N)$ queue memory.
</details>

<details>
<summary>4. How does Morris In-Order Traversal achieve O(1) auxiliary space?</summary>
Morris traversal creates temporary threaded links from the rightmost node of the left subtree (in-order predecessor) back to the current node. It traverses without a stack or recursion, removing the threads on the second visit to restore original tree structure.
</details>

<details>
<summary>5. What is the relationship between tree height and diameter?</summary>
The diameter of a tree at any node is `height(leftSubtree) + height(rightSubtree)`. The tree's overall diameter is the maximum diameter found across all nodes.
</details>

<details>
<summary>6. How can you uniquely reconstruct a binary tree from traversal arrays?</summary>
A binary tree can be uniquely reconstructed if given:
1. In-order + Pre-order traversals, OR
2. In-order + Post-order traversals.
Pre-order + Post-order is NOT sufficient unless the tree is strictly full (every node has 0 or 2 children).
</details>

<details>
<summary>7. What is an in-order successor in a BST?</summary>
The node with the smallest key strictly greater than the given node. If the node has a right child, its in-order successor is the leftmost node in its right subtree.
</details>

<details>
<summary>8. How do you invert/flip a binary tree?</summary>
Recursively swap `node.left` and `node.right` for every node in the tree using pre-order or post-order traversal.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Invert Binary Tree (LeetCode 226)</summary>

```java
public class InvertTree {
    public static TreeNode invertTree(TreeNode root) {
        if (root == null) return null;
        TreeNode temp = root.left;
        root.left = invertTree(root.right);
        root.right = invertTree(temp);
        return root;
    }
}
```
</details>

<details>
<summary>Exercise 2: Maximum Depth of Binary Tree (LeetCode 104)</summary>

```java
public class MaxDepthTree {
    public static int maxDepth(TreeNode root) {
        if (root == null) return 0;
        return 1 + Math.max(maxDepth(root.left), maxDepth(root.right));
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **BST Property**: Left subtree $<$ Root $<$ Right subtree.
- **In-Order**: Visits BST nodes in ascending sorted sequence.
- **BFS Level-Order**: `int levelSize = queue.size()` freezes level boundaries.
- **LCA**: Node where $p$ and $q$ split into different branches.
- **Validation**: Pass `(min, max)` boundaries downwards through recursive calls.

---

## 13. Further Reading

- [Algorithms 4th Edition: Binary Search Trees](https://algs4.cs.princeton.edu/32bst/)
- [LeetCode Binary Tree Card](https://leetcode.com/explore/learn/card/data-structure-tree/)
"""

write_dsa_note("07_trees-binary-and-bst.md", c07)
