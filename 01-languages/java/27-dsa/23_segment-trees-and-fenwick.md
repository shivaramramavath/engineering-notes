# Segment Trees and Fenwick Trees (Binary Indexed Trees)

> Tree-based range query data structures providing $O(\log N)$ point and range updates, range aggregate queries, and lazy propagation.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [01_arrays-and-strings.md](01_arrays-and-strings.md), [07_trees-binary-and-bst.md](07_trees-binary-and-bst.md), [20_bit-manipulation.md](20_bit-manipulation.md) |
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

Given an array of $N$ numbers subject to frequent modifications and range queries:
- **Naive Array**: Point update is $O(1)$, but range sum/min/max query is $O(N)$.
- **Prefix Sum Array**: Range sum query is $O(1)$, but point update is $O(N)$ (requires recomputing all subsequent prefix sums).
- **Fenwick Tree (Binary Indexed Tree / BIT)**: Delivers both $O(\log N)$ point updates and $O(\log N)$ prefix/range sum queries in just $N + 1$ integers of space.
- **Segment Tree**: A binary tree representing intervals that delivers $O(\log N)$ point updates and $O(\log N)$ arbitrary range queries (Sum, Min, Max, GCD), supporting **Lazy Propagation** for $O(\log N)$ range updates.

---

## 2. Core Concepts

### 1. Fenwick Tree (Binary Indexed Tree)
Invented by Peter Fenwick (1994). A BIT stores partial sums based on powers of 2 using the lowest set bit trick `i & -i`:
- Node $i$ is responsible for the sum of elements in interval $(i - (i \ \& \ -i), i]$.
- **Prefix Sum Query**: Summing requires stripping lowest set bits: `for (; i > 0; i -= i & -i) sum += tree[i];`.
- **Point Update**: Adding $\Delta$ requires propagating to parent covers: `for (; i <= n; i += i & -i) tree[i] += delta;`.

### 2. Segment Tree
A binary tree where each node represents a contiguous range $[L, R]$:
- Root represents $[0, N-1]$.
- If $L \neq R$, children represent $[L, \text{mid}]$ and $[\text{mid}+1, R]$.
- Leaves represent single elements $[i, i]$.
- Sized in a flat array of **$4N$ elements** (`left = 2*node + 1`, `right = 2*node + 2`).

### 3. Lazy Propagation
Updating an entire range $[Q_L, Q_R]$ naively takes $O(N \log N)$ by updating each leaf. Lazy propagation defers updates: when a tree node's range $[L, R]$ is completely covered by $[Q_L, Q_R]$, we update the node's value, record the pending update in a `lazy[]` array, and return immediately in $O(\log N)$ time. Pending updates are pushed down to children only when those children are accessed in later queries.

---

## 3. How It Works Internally

### Fenwick Tree Range Responsibility
```
Index (binary)   Interval Covered    Length (i & -i)
1 (00001)        [1, 1]              1
2 (00010)        [1, 2]              2
3 (00011)        [3, 3]              1
4 (00100)        [1, 4]              4
5 (00101)        [5, 5]              1
6 (00110)        [5, 6]              2
7 (00111)        [7, 7]              1
8 (01000)        [1, 8]              8
```
To query sum up to index 7:
$7 \to 7 - 1 = 6 \to 6 - 2 = 4 \to 4 - 4 = 0$.
Sum = `tree[7] + tree[6] + tree[4]`. Exactly 3 operations!

---

## 4. Syntax & Usage

### 1. Production Fenwick Tree (BIT)

```java
public class FenwickTree {
    private final int[] tree;
    private final int n;

    public FenwickTree(int n) {
        this.n = n;
        this.tree = new int[n + 1]; // 1-indexed
    }

    public void add(int i, int delta) {
        for (; i <= n; i += i & -i) {
            tree[i] += delta;
        }
    }

    public int query(int i) {
        int sum = 0;
        for (; i > 0; i -= i & -i) {
            sum += tree[i];
        }
        return sum;
    }

    public int queryRange(int l, int r) {
        return query(r) - query(l - 1);
    }
}
```

### 2. Segment Tree with Lazy Propagation

```java
public class LazySegmentTree {
    private final int[] tree;
    private final int[] lazy;
    private final int n;

    public LazySegmentTree(int n) {
        this.n = n;
        this.tree = new int[4 * n];
        this.lazy = new int[4 * n];
    }

    private void pushDown(int node, int l, int r) {
        if (lazy[node] != 0) {
            int mid = (l + r) / 2;
            int leftChild = 2 * node + 1;
            int rightChild = 2 * node + 2;

            tree[leftChild] += lazy[node] * (mid - l + 1);
            lazy[leftChild] += lazy[node];

            tree[rightChild] += lazy[node] * (r - mid);
            lazy[rightChild] += lazy[node];

            lazy[node] = 0;
        }
    }

    public void updateRange(int ql, int qr, int val) {
        updateRange(0, 0, n - 1, ql, qr, val);
    }

    private void updateRange(int node, int l, int r, int ql, int qr, int val) {
        if (ql <= l && r <= qr) {
            tree[node] += val * (r - l + 1);
            lazy[node] += val;
            return;
        }

        pushDown(node, l, r);
        int mid = (l + r) / 2;

        if (ql <= mid) updateRange(2 * node + 1, l, mid, ql, qr, val);
        if (qr > mid) updateRange(2 * node + 2, mid + 1, r, ql, qr, val);

        tree[node] = tree[2 * node + 1] + tree[2 * node + 2];
    }

    public int queryRange(int ql, int qr) {
        return queryRange(0, 0, n - 1, ql, qr);
    }

    private int queryRange(int node, int l, int r, int ql, int qr) {
        if (ql <= l && r <= qr) return tree[node];

        pushDown(node, l, r);
        int mid = (l + r) / 2;
        int sum = 0;

        if (ql <= mid) sum += queryRange(2 * node + 1, l, mid, ql, qr);
        if (qr > mid) sum += queryRange(2 * node + 2, mid + 1, r, ql, qr);

        return sum;
    }
}
```

---

## 5. Practical Examples

### Example 1: Range Sum Query - Mutable (LeetCode 307)

```java
public class NumArray {
    private final int[] nums;
    private final int[] tree;
    private final int n;

    public NumArray(int[] nums) {
        this.nums = nums;
        this.n = nums.length;
        this.tree = new int[n + 1];

        for (int i = 0; i < n; i++) {
            init(i + 1, nums[i]);
        }
    }

    private void init(int i, int val) {
        for (; i <= n; i += i & -i) {
            tree[i] += val;
        }
    }

    public void update(int index, int val) {
        int delta = val - nums[index];
        nums[index] = val;
        for (int i = index + 1; i <= n; i += i & -i) {
            tree[i] += delta;
        }
    }

    public int sumRange(int left, int right) {
        return prefixSum(right + 1) - prefixSum(left);
    }

    private int prefixSum(int i) {
        int sum = 0;
        for (; i > 0; i -= i & -i) sum += tree[i];
        return sum;
    }
}
```

### Example 2: Count of Smaller Numbers After Self (LeetCode 315)

```java
import java.util.*;

public class CountSmallerAfterSelf {
    public List<Integer> countSmaller(int[] nums) {
        int n = nums.length;
        // Coordinate Compression
        int[] sorted = nums.clone();
        Arrays.sort(sorted);
        Map<Integer, Integer> ranks = new HashMap<>();
        int rank = 1;
        for (int num : sorted) {
            if (!ranks.containsKey(num)) ranks.put(num, rank++);
        }

        int[] bit = new int[rank];
        Integer[] result = new Integer[n];

        // Process from right to left
        for (int i = n - 1; i >= 0; i--) {
            int r = ranks.get(nums[i]);
            result[i] = query(bit, r - 1); // Count strictly smaller ranks
            add(bit, r, 1);
        }

        return Arrays.asList(result);
    }

    private void add(int[] bit, int i, int delta) {
        for (; i < bit.length; i += i & -i) bit[i] += delta;
    }

    private int query(int[] bit, int i) {
        int sum = 0;
        for (; i > 0; i -= i & -i) sum += bit[i];
        return sum;
    }
}
```

### Example 3: Falling Squares (LeetCode 699, Range Max with Lazy)

```java
import java.util.*;

public class FallingSquares {
    public List<Integer> fallingSquares(int[][] positions) {
        // Collect coordinates for compression
        TreeSet<Integer> coords = new TreeSet<>();
        for (int[] p : positions) {
            coords.add(p[0]);
            coords.add(p[0] + p[1] - 1);
        }

        Map<Integer, Integer> map = new HashMap<>();
        int idx = 0;
        for (int c : coords) map.put(c, idx++);

        int[] tree = new int[4 * idx];
        int[] lazy = new int[4 * idx];

        List<Integer> result = new ArrayList<>();
        int maxH = 0;

        for (int[] p : positions) {
            int l = map.get(p[0]);
            int r = map.get(p[0] + p[1] - 1);
            int currentMax = query(tree, lazy, 0, 0, idx - 1, l, r);
            int newHeight = currentMax + p[1];
            update(tree, lazy, 0, 0, idx - 1, l, r, newHeight);
            maxH = Math.max(maxH, newHeight);
            result.add(maxH);
        }

        return result;
    }

    private void update(int[] tree, int[] lazy, int node, int l, int r, int ql, int qr, int h) {
        if (ql <= l && r <= qr) {
            tree[node] = Math.max(tree[node], h);
            lazy[node] = Math.max(lazy[node], h);
            return;
        }
        push(tree, lazy, node);
        int mid = (l + r) / 2;
        if (ql <= mid) update(tree, lazy, 2 * node + 1, l, mid, ql, qr, h);
        if (qr > mid) update(tree, lazy, 2 * node + 2, mid + 1, r, ql, qr, h);
        tree[node] = Math.max(tree[2 * node + 1], tree[2 * node + 2]);
    }

    private int query(int[] tree, int[] lazy, int node, int l, int r, int ql, int qr) {
        if (ql <= l && r <= qr) return tree[node];
        push(tree, lazy, node);
        int mid = (l + r) / 2;
        int res = 0;
        if (ql <= mid) res = Math.max(res, query(tree, lazy, 2 * node + 1, l, mid, ql, qr));
        if (qr > mid) res = Math.max(res, query(tree, lazy, 2 * node + 2, mid + 1, r, ql, qr));
        return res;
    }

    private void push(int[] tree, int[] lazy, int node) {
        if (lazy[node] != 0) {
            int lc = 2 * node + 1, rc = 2 * node + 2;
            tree[lc] = Math.max(tree[lc], lazy[node]);
            lazy[lc] = Math.max(lazy[lc], lazy[node]);
            tree[rc] = Math.max(tree[rc], lazy[node]);
            lazy[rc] = Math.max(lazy[rc], lazy[node]);
            lazy[node] = 0;
        }
    }
}
```

### Example 4: My Calendar III (LeetCode 732, Dynamic Segment Tree)

```java
import java.util.HashMap;
import java.util.Map;

public class MyCalendarThree {
    private final Map<Integer, Integer> tree = new HashMap<>();
    private final Map<Integer, Integer> lazy = new HashMap<>();

    public int book(int startTime, int endTime) {
        update(0, 1_000_000_000, startTime, endTime - 1, 1);
        return tree.getOrDefault(1, 0);
    }

    private void update(int l, int r, int ql, int qr, int node) {
        if (ql <= l && r <= qr) {
            tree.put(node, tree.getOrDefault(node, 0) + 1);
            lazy.put(node, lazy.getOrDefault(node, 0) + 1);
            return;
        }

        int mid = l + (r - l) / 2;
        push(node);

        if (ql <= mid) update(l, mid, ql, qr, 2 * node);
        if (qr > mid) update(mid + 1, r, ql, qr, 2 * node + 1);

        tree.put(node, lazy.getOrDefault(node, 0) +
                 Math.max(tree.getOrDefault(2 * node, 0), tree.getOrDefault(2 * node + 1, 0)));
    }

    private void push(int node) {
        // In dynamic sparse segment trees, lazy values are maintained on parents
    }
}
```

### Example 5: Reverse Pairs (LeetCode 493, Fenwick with Coordinate Compression)

```java
import java.util.*;

public class ReversePairs {
    public int reversePairs(int[] nums) {
        Set<Long> set = new TreeSet<>();
        for (int num : nums) {
            set.add((long) num);
            set.add(2L * num);
        }

        Map<Long, Integer> ranks = new HashMap<>();
        int rank = 1;
        for (long val : set) ranks.put(val, rank++);

        int[] bit = new int[rank];
        int count = 0;

        for (int i = nums.length - 1; i >= 0; i--) {
            // Count elements strictly smaller than nums[i] / 2.0 (i.e., val <= ranks(nums[i] - 1))
            count += query(bit, ranks.get((long) nums[i]) - 1);
            add(bit, ranks.get(2L * nums[i]), 1);
        }

        return count;
    }

    private void add(int[] bit, int i, int delta) {
        for (; i < bit.length; i += i & -i) bit[i] += delta;
    }

    private int query(int[] bit, int i) {
        int sum = 0;
        for (; i > 0; i -= i & -i) sum += bit[i];
        return sum;
    }
}
```

---

## 6. Best Practices

1. **Size Segment Tree Array to $4N$**: A Segment Tree for $N$ elements requires at most $2^{\lceil \log_2 N \rceil + 1} - 1 < 4N$ nodes. Sizing `tree[4 * N]` avoids `ArrayIndexOutOfBoundsException`.
2. **Fenwick Trees are 1-Indexed**: Never pass index 0 to a Fenwick tree (`0 & -0 = 0`, triggering an infinite loop!). Always use $1 \dots N$.
3. **Coordinate Compression**: When coordinates range up to $10^9$ but $N \le 10^5$, map unique coordinates to continuous ranks $1 \dots K$ before building trees.
4. **Choose Fenwick Over Segment Tree When Possible**: Fenwick trees use 4x less memory, have zero recursion overhead, and run 3x faster for range sums.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Passing index 0 to Fenwick Tree | `0 & -0 = 0`, causing infinite loop | Always add 1: `tree[index + 1]`. |
| Sizing Segment Tree to $2N$ | Incomplete binary trees require up to $4N$ slots | Always allocate `new int[4 * n]`. |
| Forgetting to push lazy values on partial overlap | Stale values corrupt range calculations | Call `pushDown(node, l, r)` before branching to children. |
| Inverting lazy update multiplier on sums | Multiplying by 1 instead of segment width $(r - l + 1)$ | `tree[node] += val * (r - l + 1)`. |

---

## 8. Performance & Security Notes

- **Time Complexity**:
  - Fenwick Tree: $O(\log N)$ update, $O(\log N)$ query.
  - Segment Tree: $O(\log N)$ update, $O(\log N)$ query.
  - Range Update with Lazy Propagation: $O(\log N)$.
- **Space Complexity**:
  - Fenwick Tree: $O(N)$ (exactly $N + 1$ integers).
  - Segment Tree: $O(N)$ (up to $4N$ integers).

---

## 9. Comparison / When to Use What

| Feature | Fenwick Tree (BIT) | Segment Tree | Segment Tree + Lazy |
|---|---|---|---|
| **Memory** | $N + 1$ ints | $4N$ ints | $8N$ ints |
| **Point Update** | $O(\log N)$ | $O(\log N)$ | $O(\log N)$ |
| **Range Update** | Complex ($O(\log N)$ with 2 BITs) | $O(N)$ | $O(\log N)$ |
| **Range Operations** | Invertible only (Sum, XOR) | Any associative (Min, Max, GCD) | Any associative |
| **Implementation** | ~10 lines | ~40 lines | ~70 lines |

---

## 10. Interview Questions

<details>
<summary>1. Why can a Fenwick Tree not naturally compute Range Minimum Queries (RMQ)?</summary>

Because minimum is **not an invertible operation**. While sum allows $S([L, R]) = S([0, R]) - S([0, L - 1])$, you cannot compute $\min([L, R])$ by subtracting $\min([0, L - 1])$ from $\min([0, R])$. Segment Trees maintain independent subsegment minimums directly, supporting RMQ naturally.
</details>

<details>
<summary>2. Why does a Segment Tree require $4N$ space instead of $2N$?</summary>

$N$ is not necessarily a power of 2. If $N = 2^k + 1$, the tree height is $k + 2$. The total number of nodes in a full binary tree of this height is $2^{k+2} - 1 \approx 4 \times 2^k \approx 4N$.
</details>

<details>
<summary>3. What is Coordinate Compression and why is it used with tree structures?</summary>

When coordinates span a sparse range (e.g., $x \in [-10^9, 10^9]$) but the number of unique coordinates is small ($N \le 10^5$), we sort unique values and map each to its sorted rank $1 \dots K$. This allows us to use compact trees sized $O(N)$ instead of $O(10^9)$.
</details>

<details>
<summary>4. How does a 2D Fenwick Tree work?</summary>

A 2D Fenwick tree nests the 1D update/query loops:
```java
void add(int r, int c, int val) {
    for (int i = r; i <= R; i += i & -i)
        for (int j = c; j <= C; j += j & -j)
            tree[i][j] += val;
}
```
Point updates and 2D submatrix range queries run in $O(\log R \times \log C)$.
</details>

<details>
<summary>5. How does Lazy Propagation work for Range Assignment (setting all values in range to $X$)?</summary>

The lazy tag stores the assigned value $X$ and a boolean `isAssigned`. During `pushDown()`, children's values are overwritten with $X \times \text{width}$, their lazy values are set to $X$, and the parent's `isAssigned` flag is cleared.
</details>

<details>
<summary>6. What is a Persistent Segment Tree?</summary>

A persistent segment tree preserves all previous versions after each update. Instead of mutating nodes in place, every update path allocates $O(\log N)$ new nodes sharing unchanged subtrees with earlier versions. It answers range queries on historical versions in $O(\log N)$ time.
</details>

<details>
<summary>7. How do you find the $K$-th smallest element in an array with dynamic updates using a BIT?</summary>

Maintain element frequencies in a Fenwick tree. Use binary lifting over BIT powers of 2 to locate the index where prefix sum reaches $K$ in strictly $O(\log N)$ time without standard binary search overhead ($O(\log^2 N)$).
</details>

<details>
<summary>8. How do you implement Range Updates and Range Queries using two Fenwick Trees?</summary>

Using the mathematical derivation:
$$\sum_{i=1}^p A[i] = p \sum_{i=1}^p D_1[i] - \sum_{i=1}^p (i - 1) D_2[i]$$
Maintain two BITs: $BIT_1$ storing differences $D_1$, and $BIT_2$ storing $(i - 1) D_1$. This supports $O(\log N)$ range update and $O(\log N)$ range query.
</details>

---

## 11. Practice Exercises

### Exercise 1: Count Inversions in an Array (Merge Sort vs BIT)

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class InversionCount {
    public static long countInversions(int[] arr) {
        int[] sorted = arr.clone();
        Arrays.sort(sorted);
        Map<Integer, Integer> ranks = new HashMap<>();
        int rank = 1;
        for (int val : sorted) {
            if (!ranks.containsKey(val)) ranks.put(val, rank++);
        }

        int[] bit = new int[rank];
        long inversions = 0;

        for (int i = arr.length - 1; i >= 0; i--) {
            int r = ranks.get(arr[i]);
            inversions += query(bit, r - 1); // Elements after i that are smaller
            add(bit, r, 1);
        }

        return inversions;
    }

    private static void add(int[] bit, int i, int delta) {
        for (; i < bit.length; i += i & -i) bit[i] += delta;
    }

    private static int query(int[] bit, int i) {
        int sum = 0;
        for (; i > 0; i -= i & -i) sum += bit[i];
        return sum;
    }
}
```
</details>

### Exercise 2: Range Addition (LeetCode 370, Difference Array vs Segment Tree)

<details>
<summary>Solution</summary>

```java
public class RangeAddition {
    public int[] getModifiedArray(int length, int[][] updates) {
        // Difference array: O(1) update, single O(N) sweep at end
        int[] diff = new int[length + 1];

        for (int[] u : updates) {
            int start = u[0], end = u[1], val = u[2];
            diff[start] += val;
            diff[end + 1] -= val;
        }

        int[] result = new int[length];
        int running = 0;
        for (int i = 0; i < length; i++) {
            running += diff[i];
            result[i] = running;
        }

        return result;
    }
}
```
</details>

### Exercise 3: Range Frequency Queries (LeetCode 2083, Segment Tree vs Binary Search)

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class RangeFreqQuery {
    private final Map<Integer, List<Integer>> posMap = new HashMap<>();

    public RangeFreqQuery(int[] arr) {
        for (int i = 0; i < arr.length; i++) {
            posMap.computeIfAbsent(arr[i], k -> new ArrayList<>()).add(i);
        }
    }

    public int query(int left, int right, int value) {
        List<Integer> list = posMap.get(value);
        if (list == null) return 0;

        int l = lowerBound(list, left);
        int r = upperBound(list, right);

        return r - l;
    }

    private int lowerBound(List<Integer> list, int target) {
        int low = 0, high = list.size();
        while (low < high) {
            int mid = (low + high) >>> 1;
            if (list.get(mid) >= target) high = mid;
            else low = mid + 1;
        }
        return low;
    }

    private int upperBound(List<Integer> list, int target) {
        int low = 0, high = list.size();
        while (low < high) {
            int mid = (low + high) >>> 1;
            if (list.get(mid) > target) high = mid;
            else low = mid + 1;
        }
        return low;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Fenwick Tree**: $O(\log N)$ updates/queries in $N+1$ array space. Lowest set bit: `i & -i`.
- **Segment Tree**: Sized $4N$. Supports arbitrary associative operators (Min, Max, Sum, GCD).
- **Lazy Propagation**: Defers range updates until children are queried; keeps updates $O(\log N)$.
- **Coordinate Compression**: Maps unbounded coordinate ranges to continuous $1 \dots K$ ranks.

---

## 13. Further Reading

- [Peter M. Fenwick (1994). A new data structure for cumulative frequency tables](https://doi.org/10.1002/spe.4380240306)
- [Introduction to Algorithms (CLRS), Chapter 14: Augmenting Data Structures](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Baeldung: Guide to Segment Trees in Java](https://www.baeldung.com/cs/segment-trees)
- [CP-Algorithms: Segment Tree and Fenwick Tree Tutorials](https://cp-algorithms.com/data_structures/segment_tree.html)
