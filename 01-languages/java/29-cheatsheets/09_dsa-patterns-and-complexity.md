# DSA Patterns, Big-O Complexity & Algorithms Reference Cheat Sheet

> Master Big-O complexity reference tables for all data structures and sorting algorithms, 16 fundamental algorithmic patterns, Java idiom templates, and memory complexity analysis.

| Metadata | Details |
| :--- | :--- |
| **Level** | Comprehensive Reference |
| **Prerequisites** | [27-dsa](../27-dsa/README.md) |
| **Time to Read** | 45 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to 25 LTS |

---

## Table of Contents
1. [Master Big-O Complexity Hierarchy](#1-master-big-o-complexity-hierarchy)
2. [Data Structure Operations Complexity Matrix](#2-data-structure-operations-complexity-matrix)
3. [Sorting Algorithms Complexity & Stability Matrix](#3-sorting-algorithms-complexity--stability-matrix)
4. [Searching & Graph Algorithms Complexity](#4-searching--graph-algorithms-complexity)
5. [16 Core Algorithmic Patterns Reference](#5-16-core-algorithmic-patterns-reference)
6. [Pattern 1: Two Pointers (Opposite & Same Direction)](#pattern-1-two-pointers)
7. [Pattern 2: Sliding Window (Fixed & Dynamic)](#pattern-2-sliding-window)
8. [Pattern 3: Fast & Slow Pointers (Cycle Detection)](#pattern-3-fast--slow-pointers)
9. [Pattern 4: Monotonic Stack & Queue](#pattern-4-monotonic-stack--queue)
10. [Pattern 5: Top-K Elements & Heap Priority](#pattern-5-top-k-elements--heap-priority)
11. [Pattern 6: Interval Merge & Overlap](#pattern-6-interval-merge--overlap)
12. [Pattern 7: Breadth-First Search (BFS)](#pattern-7-breadth-first-search-bfs)
13. [Pattern 8: Depth-First Search & Backtracking](#pattern-8-depth-first-search--backtracking)
14. [Pattern 9: Binary Search on Answer Space](#pattern-9-binary-search-on-answer-space)
15. [Pattern 10: Dynamic Programming (Memoization vs Tabulation)](#pattern-10-dynamic-programming)
16. [Pattern 11: Disjoint Set Union (DSU / Union-Find)](#pattern-11-disjoint-set-union-dsu)
17. [Pattern 12: Trie (Prefix Tree) Implementation](#pattern-12-trie-prefix-tree)
18. [Pattern 13: Topological Sort (Kahn's Algorithm)](#pattern-13-topological-sort)
19. [Pattern 14: Shortest Path (Dijkstra's Algorithm)](#pattern-14-shortest-path)
20. [Pattern 15: Minimum Spanning Tree (Kruskal & Prim)](#pattern-15-minimum-spanning-tree)
21. [Pattern 16: Bit Manipulation & Bitmasks](#pattern-16-bit-manipulation--bitmasks)
22. [Space Complexity & Stack Frame Allocation](#22-space-complexity--stack-frame-allocation)

---

## 1. Master Big-O Complexity Hierarchy

$$O(1) < O(\log N) < O(\sqrt{N}) < O(N) < O(N \log N) < O(N^2) < O(N^3) < O(2^N) < O(N!)$$

```
  Execution Time / Operations
   ^
   |                                     / O(N!)
   |                                    /
   |                                   |  / O(2^N)
   |                                   | |
   |                                   | |  / O(N^2)
   |                                   | | /
   |                                   | |/  / O(N log N)
   |                                   | /  /
   |                                   |/  /  / O(N)
   |                                  //  /  /
   |                                 //  /  /  / O(log N)
   |                                /// /  /  /
   |----------------------------------------------------> O(1)
   +----------------------------------------------------> Input Size (N)
```

### Growth Rules of Thumb:
- $N \le 12$: $O(N!)$ or $O(2^N \times N^2)$ algorithms will pass ($<10^7$ ops).
- $N \le 25$: $O(2^N)$ backtracking will pass.
- $N \le 1,000$: $O(N^2)$ nested loops will pass ($\le 10^6$ ops).
- $N \le 100,000$: $O(N \log N)$ sorting and heap operations will pass.
- $N \le 10,000,000$: $O(N)$ linear scans or $O(\log N)$ binary search required.

---

## 2. Data Structure Operations Complexity Matrix

| Data Structure | Access (Average) | Search (Average) | Insertion (Average) | Deletion (Average) | Worst Case Search | Worst Case Insertion | Worst Case Deletion | Space Overhead |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Array** | $O(1)$ | $O(N)$ | N/A | N/A | $O(N)$ | N/A | N/A | $O(N)$ |
| **Dynamic Array (`ArrayList`)** | $O(1)$ | $O(N)$ | $O(1)$ amortized | $O(N)$ | $O(N)$ | $O(N)$ | $O(N)$ | $O(N)$ |
| **Singly Linked List** | $O(N)$ | $O(N)$ | $O(1)$ at head | $O(1)$ at head | $O(N)$ | $O(1)$ | $O(1)$ | $O(N)$ |
| **Doubly Linked List** | $O(N)$ | $O(N)$ | $O(1)$ head/tail | $O(1)$ at node | $O(N)$ | $O(1)$ | $O(1)$ | $O(N)$ |
| **Stack (`ArrayDeque`)** | $O(1)$ peek | $O(N)$ | $O(1)$ push | $O(1)$ pop | $O(N)$ | $O(1)$ amortized | $O(1)$ | $O(N)$ |
| **Queue (`ArrayDeque`)** | $O(1)$ peek | $O(N)$ | $O(1)$ offer | $O(1)$ poll | $O(N)$ | $O(1)$ amortized | $O(1)$ | $O(N)$ |
| **Hash Table (`HashMap`)** | N/A | $O(1)$ | $O(1)$ | $O(1)$ | $O(\log N)$ (treeified) | $O(\log N)$ | $O(\log N)$ | $O(N)$ |
| **Binary Search Tree (Unbalanced)**| $O(\log N)$ | $O(\log N)$ | $O(\log N)$ | $O(\log N)$ | $O(N)$ (skewed) | $O(N)$ | $O(N)$ | $O(N)$ |
| **Red-Black Tree (`TreeMap`)** | $O(\log N)$ | $O(\log N)$ | $O(\log N)$ | $O(\log N)$ | $O(\log N)$ | $O(\log N)$ | $O(\log N)$ | $O(N)$ |
| **Binary Heap (`PriorityQueue`)** | $O(1)$ peek | $O(N)$ | $O(\log N)$ offer | $O(\log N)$ poll | $O(N)$ | $O(\log N)$ | $O(\log N)$ | $O(N)$ |
| **Trie (Prefix Tree)** | $O(L)$ | $O(L)$ | $O(L)$ | $O(L)$ | $O(L)$ | $O(L)$ | $O(L)$ | $O(\Sigma \times L)$ |
| **Disjoint Set Union (DSU)** | N/A | $O(\alpha(N))$ find | $O(\alpha(N))$ union | N/A | $O(\alpha(N))$ | $O(\alpha(N))$ | N/A | $O(N)$ |
| **Segment Tree** | N/A | $O(\log N)$ range | $O(\log N)$ update | N/A | $O(\log N)$ | $O(\log N)$ | N/A | $O(4N)$ |
| **Fenwick Tree (Binary Indexed)** | N/A | $O(\log N)$ prefix| $O(\log N)$ update | N/A | $O(\log N)$ | $O(\log N)$ | N/A | $O(N)$ |

*Note: $L$ represents string length; $\Sigma$ represents alphabet size; $\alpha(N)$ is the Inverse Ackermann Function ($\le 4$ for all physical universes).*

---

## 3. Sorting Algorithms Complexity & Stability Matrix

| Algorithm | Best Time | Average Time | Worst Time | Space Complexity | Stable? | In-Place? | Recommended Use Case |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **TimSort (`Arrays.sort` Objects)** | $O(N)$ | $O(N \log N)$ | $O(N \log N)$ | $O(N)$ | **Yes** | No | General-purpose object sorting |
| **Dual-Pivot QuickSort (Primitives)**| $O(N \log N)$ | $O(N \log N)$ | $O(N^2)$ | $O(\log N)$ stack | **No** | **Yes** | Primitive array sorting |
| **Merge Sort** | $O(N \log N)$ | $O(N \log N)$ | $O(N \log N)$ | $O(N)$ | **Yes** | No | Linked lists, external disk sorting |
| **Heap Sort** | $O(N \log N)$ | $O(N \log N)$ | $O(N \log N)$ | $O(1)$ | **No** | **Yes** | Guaranteed $O(N \log N)$ zero extra space |
| **Insertion Sort** | $O(N)$ | $O(N^2)$ | $O(N^2)$ | $O(1)$ | **Yes** | **Yes** | Nearly sorted or small arrays ($N < 47$) |
| **Counting Sort** | $O(N + K)$ | $O(N + K)$ | $O(N + K)$ | $O(K)$ | **Yes** | No | Small integer keys ($K \ll N$) |
| **Radix Sort** | $O(D \times (N + K))$| $O(D \times (N + K))$| $O(D \times (N + K))$| $O(N + K)$ | **Yes** | No | Fixed-width integers, strings |
| **Bucket Sort** | $O(N + K)$ | $O(N + K)$ | $O(N^2)$ | $O(N)$ | **Yes** | No | Uniformly distributed floating point numbers |

---

## 4. Searching & Graph Algorithms Complexity

| Algorithm | Graph Representation | Time Complexity | Space Complexity | Key Characteristics |
| :--- | :--- | :---: | :---: | :--- |
| **Breadth-First Search (BFS)** | Adjacency List | $O(V + E)$ | $O(V)$ | Shortest path in unweighted graphs |
| **Depth-First Search (DFS)** | Adjacency List | $O(V + E)$ | $O(V)$ | Cycle detection, topological ordering |
| **Dijkstra's Algorithm** | Min-Heap + Adj List | $O((V + E) \log V)$ | $O(V)$ | Single-source shortest path (Non-negative weights) |
| **Bellman-Ford Algorithm** | Edge List | $O(V \times E)$ | $O(V)$ | Shortest path with negative edge weights |
| **Floyd-Warshall Algorithm** | Adjacency Matrix | $O(V^3)$ | $O(V^2)$ | All-pairs shortest path |
| **Kruskal's Algorithm** | Edge List + DSU | $O(E \log E)$ | $O(V)$ | Minimum Spanning Tree (MST) |
| **Prim's Algorithm** | Min-Heap + Adj List | $O((V + E) \log V)$ | $O(V)$ | Dense Minimum Spanning Tree |
| **Kahn's Topological Sort** | In-Degree Array + Queue | $O(V + E)$ | $O(V)$ | Directed Acyclic Graph (DAG) scheduling |

---

## 5. 16 Core Algorithmic Patterns Reference

```
                             [Algorithmic Mastery]
                                       |
    +-------------------+--------------+--------------+-------------------+
    |                   |                             |                   |
[Pointers & Windows]  [Stacks & Heaps]            [Graphs & Trees]    [Optimization]
 - Two Pointers        - Monotonic Stack           - BFS (Level-order) - Binary Search Answer
 - Sliding Window      - Top-K Elements            - DFS Backtracking  - Dynamic Programming
 - Fast & Slow         - Merge Intervals           - TopoSort (Kahn's) - Disjoint Set Union
                       - Trie Prefix               - Dijkstra / MST    - Bitmasking
```

---

## Pattern 1: Two Pointers

```java
// Two-Sum on Sorted Array (Opposite Direction)
public int[] twoSumSorted(int[] nums, int target) {
    int left = 0, right = nums.length - 1;
    while (left < right) {
        int sum = nums[left] + nums[right];
        if (sum == target) return new int[]{left, right};
        else if (sum < target) left++;
        else right--;
    }
    return new int[]{-1, -1};
}
```

---

## Pattern 2: Sliding Window

```java
// Maximum Sum Subarray of Fixed Size K
public int maxSumSubarray(int[] nums, int k) {
    int windowSum = 0, maxSum = 0;
    for (int i = 0; i < k; i++) windowSum += nums[i];
    maxSum = windowSum;

    for (int i = k; i < nums.length; i++) {
        windowSum += nums[i] - nums[i - k]; // Slide window right
        maxSum = Math.max(maxSum, windowSum);
    }
    return maxSum;
}
```

---

## Pattern 3: Fast & Slow Pointers

```java
// Floyd's Cycle Detection on Linked List
public boolean hasCycle(ListNode head) {
    ListNode slow = head, fast = head;
    while (fast != null && fast.next != null) {
        slow = slow.next;
        fast = fast.next.next;
        if (slow == fast) return true; // Cycle detected
    }
    return false;
}
```

---

## Pattern 4: Monotonic Stack & Queue

```java
// Next Greater Element
public int[] nextGreaterElements(int[] nums) {
    int n = nums.length;
    int[] result = new int[n];
    Arrays.fill(result, -1);
    Deque<Integer> stack = new ArrayDeque<>(); // Monotonically decreasing stack (stores indices)

    for (int i = 0; i < n; i++) {
        while (!stack.isEmpty() && nums[i] > nums[stack.peek()]) {
            result[stack.pop()] = nums[i];
        }
        stack.push(i);
    }
    return result;
}
```

---

## Pattern 5: Top-K Elements & Heap Priority

```java
// Kth Largest Element in Unsorted Array
public int findKthLargest(int[] nums, int k) {
    PriorityQueue<Integer> minHeap = new PriorityQueue<>(k);
    for (int num : nums) {
        minHeap.offer(num);
        if (minHeap.size() > k) minHeap.poll(); // Keep heap size at exactly k
    }
    return minHeap.peek();
}
```

---

## Pattern 6: Interval Merge & Overlap

```java
public int[][] mergeIntervals(int[][] intervals) {
    if (intervals.length <= 1) return intervals;
    Arrays.sort(intervals, Comparator.comparingInt(a -> a[0]));

    List<int[]> merged = new ArrayList<>();
    int[] current = intervals[0];
    merged.add(current);

    for (int[] next : intervals) {
        if (next[0] <= current[1]) {
            current[1] = Math.max(current[1], next[1]); // Overlap: extend end
        } else {
            current = next;
            merged.add(current);
        }
    }
    return merged.toArray(new int[merged.size()][]);
}
```

---

## Pattern 7: Breadth-First Search (BFS)

```java
// Level-order Binary Tree Traversal
public List<List<Integer>> levelOrder(TreeNode root) {
    List<List<Integer>> result = new ArrayList<>();
    if (root == null) return result;
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
        result.add(currentLevel);
    }
    return result;
}
```

---

## Pattern 8: Depth-First Search & Backtracking

```java
// Permutations Generator
public List<List<Integer>> permute(int[] nums) {
    List<List<Integer>> result = new ArrayList<>();
    backtrack(nums, new ArrayList<>(), new boolean[nums.length], result);
    return result;
}

private void backtrack(int[] nums, List<Integer> current, boolean[] used, List<List<Integer>> result) {
    if (current.size() == nums.length) {
        result.add(new ArrayList<>(current)); // Found complete permutation
        return;
    }
    for (int i = 0; i < nums.length; i++) {
        if (used[i]) continue;
        used[i] = true;
        current.add(nums[i]);
        backtrack(nums, current, used, result);
        current.remove(current.size() - 1); // Backtrack
        used[i] = false;
    }
}
```

---

## Pattern 9: Binary Search on Answer Space

```java
// Minimum capacity to ship packages within D days
public int shipWithinDays(int[] weights, int days) {
    int low = Arrays.stream(weights).max().orElse(0);
    int high = Arrays.stream(weights).sum();

    while (low < high) {
        int mid = low + (high - low) / 2;
        if (canShip(weights, days, mid)) {
            high = mid; // Feasible: search lower half
        } else {
            low = mid + 1; // Infeasible: need higher capacity
        }
    }
    return low;
}
```

---

## Pattern 10: Dynamic Programming (1D & 2D)

```java
// 0/1 Knapsack Problem (Space-Optimized 1D Tabulation)
public int knapsack(int[] values, int[] weights, int capacity) {
    int[] dp = new int[capacity + 1];
    for (int i = 0; i < values.length; i++) {
        for (int w = capacity; w >= weights[i]; w--) { // Traverse backwards to avoid reuse
            dp[w] = Math.max(dp[w], dp[w - weights[i]] + values[i]);
        }
    }
    return dp[capacity];
}
```

---

## Pattern 11: Disjoint Set Union (DSU / Union-Find)

```java
public class DSU {
    private final int[] parent;
    private final int[] rank;

    public DSU(int n) {
        parent = new int[n];
        rank = new int[n];
        for (int i = 0; i < n; i++) parent[i] = i;
    }

    public int find(int i) {
        if (parent[i] == i) return i;
        return parent[i] = find(parent[i]); // Path compression
    }

    public boolean union(int i, int j) {
        int rootI = find(i), rootJ = find(j);
        if (rootI == rootJ) return false; // Already connected
        if (rank[rootI] < rank[rootJ]) parent[rootI] = rootJ;
        else if (rank[rootI] > rank[rootJ]) parent[rootJ] = rootI;
        else {
            parent[rootJ] = rootI;
            rank[rootI]++;
        }
        return true;
    }
}
```

---

## Pattern 12: Trie (Prefix Tree) Implementation

```java
public class Trie {
    private static class Node {
        final Node[] children = new Node[26];
        boolean isEndOfWord;
    }
    private final Node root = new Node();

    public void insert(String word) {
        Node curr = root;
        for (char c : word.toCharArray()) {
            int idx = c - 'a';
            if (curr.children[idx] == null) curr.children[idx] = new Node();
            curr = curr.children[idx];
        }
        curr.isEndOfWord = true;
    }

    public boolean startsWith(String prefix) {
        Node curr = root;
        for (char c : prefix.toCharArray()) {
            int idx = c - 'a';
            if (curr.children[idx] == null) return false;
            curr = curr.children[idx];
        }
        return true;
    }
}
```

---

## Pattern 13: Topological Sort (Kahn's Algorithm)

```java
public int[] findOrder(int numCourses, int[][] prerequisites) {
    int[] inDegree = new int[numCourses];
    List<List<Integer>> adj = new ArrayList<>();
    for (int i = 0; i < numCourses; i++) adj.add(new ArrayList<>());

    for (int[] edge : prerequisites) {
        adj.get(edge[1]).add(edge[0]);
        inDegree[edge[0]]++;
    }

    Queue<Integer> queue = new ArrayDeque<>();
    for (int i = 0; i < numCourses; i++) {
        if (inDegree[i] == 0) queue.offer(i);
    }

    int[] order = new int[numCourses];
    int idx = 0;
    while (!queue.isEmpty()) {
        int course = queue.poll();
        order[idx++] = course;
        for (int next : adj.get(course)) {
            if (--inDegree[next] == 0) queue.offer(next);
        }
    }
    return idx == numCourses ? order : new int[0]; // If idx != numCourses, cycle exists!
}
```

---

## Pattern 14: Shortest Path (Dijkstra's Algorithm)

```java
public int[] dijkstra(int n, List<List<int[]>> adj, int start) {
    int[] dist = new int[n];
    Arrays.fill(dist, Integer.MAX_VALUE);
    dist[start] = 0;

    PriorityQueue<int[]> pq = new PriorityQueue<>(Comparator.comparingInt(a -> a[1]));
    pq.offer(new int[]{start, 0});

    while (!pq.isEmpty()) {
        int[] top = pq.poll();
        int u = top[0], d = top[1];
        if (d > dist[u]) continue;

        for (int[] edge : adj.get(u)) {
            int v = edge[0], weight = edge[1];
            if (dist[u] + weight < dist[v]) {
                dist[v] = dist[u] + weight;
                pq.offer(new int[]{v, dist[v]});
            }
        }
    }
    return dist;
}
```

---

## Pattern 15: Minimum Spanning Tree (Kruskal's Algorithm)

```java
public int kruskalMST(int n, int[][] edges) {
    // edges: [u, v, weight]
    Arrays.sort(edges, Comparator.comparingInt(a -> a[2]));
    DSU dsu = new DSU(n);
    int totalWeight = 0, edgesUsed = 0;

    for (int[] edge : edges) {
        if (dsu.union(edge[0], edge[1])) {
            totalWeight += edge[2];
            if (++edgesUsed == n - 1) break;
        }
    }
    return edgesUsed == n - 1 ? totalWeight : -1;
}
```

---

## Pattern 16: Bit Manipulation & Bitmasks

```java
// Subset generation via bitmasks
public List<List<Integer>> subsets(int[] nums) {
    int n = nums.length;
    int totalSubsets = 1 << n; // 2^n subsets
    List<List<Integer>> result = new ArrayList<>(totalSubsets);

    for (int mask = 0; mask < totalSubsets; mask++) {
        List<Integer> current = new ArrayList<>();
        for (int i = 0; i < n; i++) {
            if ((mask & (1 << i)) != 0) current.add(nums[i]);
        }
        result.add(current);
    }
    return result;
}
```

---

## 22. Space Complexity & Stack Frame Allocation

- **Call Stack Memory Overhead:** Each recursive function invocation creates an operating system stack frame consuming memory ($O(D)$, where $D$ is max recursion depth).
- **Default Stack Limit:** `-Xss1m` supports approximately 5,000–10,000 recursive frames before throwing `StackOverflowError`.
- **Heap vs Stack:** Primitive local variables reside on stack; object instances and array allocations always reside on the Java Heap.
