# Self-Balancing Binary Search Trees: AVL and Red-Black Trees in Java

> Mathematical balance invariants, tree rotations (LL, RR, LR, RL), recoloring rules, and Java TreeMap / TreeSet Red-Black tree internals.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [`27-dsa/07_trees-binary-and-bst.md`](07_trees-binary-and-bst.md), [`08-collections/08_hashmap-and-hashset.md`](../08-collections/08_hashmap-and-hashset.md) |
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

In an ordinary Binary Search Tree, inserting sorted or nearly sorted data (e.g. `[1, 2, 3, 4, 5]`) degenerates the tree into a linked list with height $h = N$, degrading search, insertion, and deletion from $O(\log N)$ to $O(N)$.

**Self-Balancing Binary Search Trees** automatically restructure themselves via **Tree Rotations** during insertions and deletions, strictly guaranteeing that tree height remains $O(\log N)$ in all cases.

---

## 2. Core Concepts

### 2.1 AVL Trees (Adelson-Velsky and Landis)

An AVL tree enforces a strict height-balance invariant:
$$\text{Balance Factor } BF = \text{height}(\text{left}) - \text{height}(\text{right}) \in \{-1, 0, +1\}$$
- **Height Bound**: Maximum height is strictly bounded by $\approx 1.44 \log_2 N$.
- **Lookup Performance**: Faster lookups than Red-Black trees due to more rigid balance.
- **Rebalancing Overhead**: May require multiple rotations during deletions.

### 2.2 Red-Black Trees (Java `TreeMap` & `TreeSet`)

A Red-Black tree colors each node either **RED** or **BLACK** and enforces 5 fundamental invariants:
1. Every node is either RED or BLACK.
2. The root node is always BLACK.
3. Every leaf (`null` sentinel) is BLACK.
4. If a node is RED, both of its children must be BLACK (No two consecutive RED nodes).
5. For each node, all simple paths from the node to descendant leaves contain the identical number of BLACK nodes (**Black Height**).

- **Height Bound**: Maximum height is bounded by $2 \log_2(N + 1)$.
- **Rebalancing Overhead**: At most 2 rotations for an insert; at most 3 rotations for a delete (rest achieved via fast $O(1)$ node recoloring).

---

## 3. How It Works Internally

### 3.1 The Four Fundamental Tree Rotations

When an insertion causes an imbalance, one of four rotations restores equilibrium:

```
1. Right Rotation (LL Imbalance):
        (Y)                   (X)
       /   \                 /   \
     (X)    C    ===>       A    (Y)
    /   \                       /   \
   A     B                     B     C

2. Left Rotation (RR Imbalance):
     (X)                      (Y)
    /   \                    /   \
   A    (Y)      ===>      (X)    C
       /   \               / \
      B     C             A   B

3. Left-Right Rotation (LR Imbalance):
   Rotate left child LEFT, then rotate root RIGHT.

4. Right-Left Rotation (RL Imbalance):
   Rotate right child RIGHT, then rotate root LEFT.
```

---

## 4. Syntax & Usage

### 4.1 Java Standard Library Self-Balancing Collections

Java's standard library implements Red-Black trees directly in `java.util.TreeMap` and `java.util.TreeSet`:

```java
import java.util.TreeMap;

public class TreeMapDemo {
    public static void main(String[] args) {
        // Automatically maintains keys in a balanced Red-Black tree
        TreeMap<Integer, String> treeMap = new TreeMap<>();
        treeMap.put(50, "Fifty");
        treeMap.put(20, "Twenty");
        treeMap.put(80, "Eighty");

        // O(log N) ceiling, floor, and range queries
        System.out.println("Floor key for 25: " + treeMap.floorKey(25)); // 20
        System.out.println("Ceiling key for 25: " + treeMap.ceilingKey(25)); // 50
    }
}
```

---

## 5. Practical Examples

### 5.1 Pattern 1: Complete AVL Tree Implementation with Rotations
```java
public class AvlTree {
    public static class Node {
        int key, height;
        Node left, right;
        Node(int key) { this.key = key; this.height = 1; }
    }

    private Node root;

    public void insert(int key) {
        root = insert(root, key);
    }

    private Node insert(Node node, int key) {
        if (node == null) return new Node(key);

        if (key < node.key) node.left = insert(node.left, key);
        else if (key > node.key) node.right = insert(node.right, key);
        else return node; // Duplicate keys not allowed

        node.height = 1 + Math.max(height(node.left), height(node.right));
        int balance = getBalance(node);

        // Case 1: Left-Left (LL)
        if (balance > 1 && key < node.left.key) {
            return rotateRight(node);
        }
        // Case 2: Right-Right (RR)
        if (balance < -1 && key > node.right.key) {
            return rotateLeft(node);
        }
        // Case 3: Left-Right (LR)
        if (balance > 1 && key > node.left.key) {
            node.left = rotateLeft(node.left);
            return rotateRight(node);
        }
        // Case 4: Right-Left (RL)
        if (balance < -1 && key < node.right.key) {
            node.right = rotateRight(node.right);
            return rotateLeft(node);
        }

        return node;
    }

    private Node rotateRight(Node y) {
        Node x = y.left;
        Node t2 = x.right;

        x.right = y;
        y.left = t2;

        y.height = Math.max(height(y.left), height(y.right)) + 1;
        x.height = Math.max(height(x.left), height(x.right)) + 1;
        return x;
    }

    private Node rotateLeft(Node x) {
        Node y = x.right;
        Node t2 = y.left;

        y.left = x;
        x.right = t2;

        x.height = Math.max(height(x.left), height(x.right)) + 1;
        y.height = Math.max(height(y.left), height(y.right)) + 1;
        return y;
    }

    private int height(Node n) { return n == null ? 0 : n.height; }
    private int getBalance(Node n) { return n == null ? 0 : height(n.left) - height(n.right); }
}
```

### 5.2 Pattern 2: Floor and Ceiling Queries in Red-Black Tree
```java
import java.util.TreeSet;

public class ClosestElementsQuery {
    private final TreeSet<Integer> set = new TreeSet<>();

    public void add(int val) { set.add(val); }

    public Integer getClosest(int target) {
        Integer floor = set.floor(target);
        Integer ceiling = set.ceiling(target);

        if (floor == null) return ceiling;
        if (ceiling == null) return floor;

        return (target - floor <= ceiling - target) ? floor : ceiling;
    }
}
```

### 5.3 Pattern 3: Range Sum Query with Sorted Tree Structure
```java
import java.util.NavigableMap;
import java.util.TreeMap;

public class RangeSumBST {
    private final TreeMap<Integer, Integer> map = new TreeMap<>();

    public void add(int key, int val) {
        map.put(key, val);
    }

    public int sumRange(int low, int high) {
        NavigableMap<Integer, Integer> subMap = map.subMap(low, true, high, true);
        int sum = 0;
        for (int v : subMap.values()) sum += v;
        return sum;
    }
}
```

---

## 6. Best Practices

1. **Use `TreeMap` / `TreeSet` in Production**: Never write a custom Red-Black tree in production enterprise systems unless building low-level database engines. Java's `TreeMap` is rigorously optimized and battle-tested.
2. **Update Heights Immediately During Rotations**: When rotating nodes, always recalculate child heights before parent heights to maintain correct state.
3. **Guard Against Duplicate Keys**: Clearly define whether duplicates are rejected or stored in lists at the tree node.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Updating Heights in Wrong Order** | Parent height computed using stale unrotated child height | Recalculate lower node height first, then higher node |
| **Missing Double Rotations (LR/RL)**| Applying a single rotation to an LR imbalance leaves tree imbalanced | Check inner child key and perform double rotation |
| **Inconsistent Comparator in `TreeMap`** | Comparator returning 0 for non-equal items causes `TreeMap` to drop keys | Ensure comparator is consistent with `equals()` |
| **Modifying Key Mutability** | Mutating an object inside `TreeSet` breaks binary search ordering | Always use immutable records as tree keys |

---

## 8. Performance & Security Notes

### 8.1 AVL vs Red-Black Tree Trade-offs
- **AVL Trees**: More strictly balanced ($H \le 1.44 \log_2 N$). Ideal for read-heavy workloads where lookup performance dominates.
- **Red-Black Trees**: More relaxed balance ($H \le 2 \log_2 N$). Faster insertions and deletions (fewer rotations). Ideal for write-heavy workloads (chosen by Java's `TreeMap` and the Linux kernel CPU scheduler).

---

## 9. Comparison / When to Use What

| Feature | Standard BST | AVL Tree | Red-Black Tree (Java `TreeMap`) |
| :--- | :--- | :--- | :--- |
| **Worst-Case Search** | $O(N)$ (Degenerate) | $O(\log N)$ | $O(\log N)$ |
| **Worst-Case Insert** | $O(N)$ | $O(\log N)$ | $O(\log N)$ |
| **Worst-Case Delete** | $O(N)$ | $O(\log N)$ | $O(\log N)$ |
| **Maximum Tree Height**| $N$ | $\approx 1.44 \log_2 N$ | $\approx 2 \log_2 N$ |
| **Rotations on Insert**| None | At most 2 | At most 2 |
| **Rotations on Delete**| None | Up to $O(\log N)$ | At most 3 |
| **Primary Advantage** | Simplicity | Fastest Lookups | Faster Insert/Delete |

---

## 10. Interview Questions

<details>
<summary>1. Why did the Java team choose Red-Black Trees over AVL Trees for `TreeMap` and `HashMap`?</summary>
Red-Black trees require fewer rotations during insertions and deletions (at most 2 for insert, at most 3 for delete), relying on fast $O(1)$ bit recoloring for most balance adjustments. Since general-purpose collections experience frequent mutations, Red-Black trees offer higher overall write throughput than AVL trees.
</details>

<details>
<summary>2. What are the 5 invariants of a Red-Black Tree?</summary>
1. Every node is RED or BLACK.
2. The root is BLACK.
3. Every leaf (`null`) is BLACK.
4. If a node is RED, both its children must be BLACK (No consecutive REDs).
5. For each node, all paths to descendant leaves contain the same number of BLACK nodes.
</details>

<details>
<summary>3. What is the Balance Factor in an AVL Tree and when is a rotation triggered?</summary>
The Balance Factor is $\text{height}(\text{left}) - \text{height}(\text{right})$. An AVL tree permits only values in $\{-1, 0, +1\}$. If the balance factor becomes $+2$ or $-2$ after an insertion or deletion, a rotation is immediately triggered.
</details>

<details>
<summary>4. Explain the difference between an LR (Left-Right) rotation and a single Right rotation.</summary>
A single Right rotation resolves an LL imbalance where the newly inserted key is in the left child's left subtree. An LR imbalance occurs when the key is in the left child's right subtree (a zigzag path). A single rotation cannot fix a zigzag; it must first rotate the child LEFT to straighten the zigzag into an LL line, then rotate the root RIGHT.
</details>

<details>
<summary>5. What is the maximum possible height of a Red-Black tree with N internal nodes?</summary>
The height is at most $2 \log_2(N + 1)$. This occurs when the shortest path contains only black nodes and the longest path alternates between black and red nodes.
</details>

<details>
<summary>6. How does Java's `TreeMap` handle custom sorting?</summary>
It accepts an optional `java.util.Comparator` in its constructor. If no comparator is provided, keys must implement `Comparable` (natural ordering).
</details>

<details>
<summary>7. What happens if a `Comparable` key's `compareTo()` method is inconsistent with `equals()` in a `TreeSet`?</summary>
`TreeSet` relies strictly on `compareTo()` (not `equals()`) to determine element equality. If `compareTo()` returns 0 for two objects that are not equal according to `equals()`, the `TreeSet` will consider them duplicates and discard the second object.
</details>

<details>
<summary>8. How do Red-Black trees prevent the need for height recalculations?</summary>
Red-Black trees store a single 1-bit boolean flag (`isRed`) instead of integer heights, saving memory and eliminating the need to traverse and update height fields along parent paths during rotations.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Check if a Binary Tree is AVL Balanced (LeetCode 110)</summary>

```java
public class BalancedBinaryTree {
    public static boolean isBalanced(TreeNode root) {
        return checkHeight(root) != -1;
    }

    private static int checkHeight(TreeNode node) {
        if (node == null) return 0;

        int leftH = checkHeight(node.left);
        if (leftH == -1) return -1;

        int rightH = checkHeight(node.right);
        if (rightH == -1) return -1;

        if (Math.abs(leftH - rightH) > 1) return -1;
        return 1 + Math.max(leftH, rightH);
    }
}
```
</details>

<details>
<summary>Exercise 2: Implement Tree Rotation Helpers</summary>

```java
public class RotationHelper {
    public static TreeNode rotateRight(TreeNode y) {
        TreeNode x = y.left;
        y.left = x.right;
        x.right = y;
        return x;
    }

    public static TreeNode rotateLeft(TreeNode x) {
        TreeNode y = x.right;
        x.right = y.left;
        y.left = x;
        return y;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **AVL Trees**: Strictly balanced ($|BF| \le 1$). Faster lookups; slightly slower writes due to multiple rotations.
- **Red-Black Trees**: Relaxed balance ($H \le 2 \log N$). Faster writes; chosen by `TreeMap` and `HashMap`.
- **Rotations**: LL $\to$ Right, RR $\to$ Left, LR $\to$ Left-Right, RL $\to$ Right-Left.
- **Contract**: `TreeMap` uses `compareTo()` == 0 for equality, not `equals()`.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 13: Red-Black Trees](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [OpenJDK Source Code: `java.util.TreeMap`](https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/util/TreeMap.java)
- [Robert Sedgewick: Left-Leaning Red-Black Trees](https://www.cs.princeton.edu/~rs/talks/LLRB/LLRB.pdf)
- [Baeldung: Guide to Self-Balancing Binary Trees in Java](https://www.baeldung.com/cs/self-balancing-binary-tree)
- [Java Platform, Standard Edition API Reference: NavigableMap](https://docs.oracle.com/en/java/javase/25/docs/api/java.base/java/util/NavigableMap.html)
- [Java Platform, Standard Edition API Reference: NavigableSet](https://docs.oracle.com/en/java/javase/25/docs/api/java.base/java/util/NavigableSet.html)
- [Competitive Programmer's Handbook: Balanced Trees](https://cses.fi/book/book.pdf)
- [MIT 6.006 Lecture Notes: AVL Trees & Red-Black Trees](https://ocw.mit.edu/courses/6-006-introduction-to-algorithms-spring-2020/)

