# Disjoint Set Union (DSU / Union-Find)

> Near constant-time dynamic connectivity data structure leveraging path compression and union by rank/size.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01_arrays-and-strings.md](01_arrays-and-strings.md), [07_trees-binary-and-bst.md](07_trees-binary-and-bst.md) |
| **Time to Read** | 25 minutes |
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

The **Disjoint Set Union (DSU)** data structure—also known as **Union-Find**—maintains a collection of disjoint (non-overlapping) sets of elements partitioned into equivalence classes.

### The Problem Solved
Given an ongoing stream of connection edges between $N$ elements, answer queries such as:
1. `union(u, v)`: Merge the set containing $u$ with the set containing $v$.
2. `find(u)`: Identify the canonical representative leader of the set containing $u$.
3. `connected(u, v)`: Return true if $u$ and $v$ belong to the same component (`find(u) == find(v)`).

Using standard graph traversal (BFS or DFS), checking connectivity after each edge addition takes $O(V + E)$ time. DSU answers these dynamic connectivity queries in **amortized $O(\alpha(N))$ time**, which is effectively $O(1)$ operations per query.

---

## 2. Core Concepts

### 1. Forest of Trees Representation
Each disjoint set is represented as a rooted tree where:
- Each node stores a pointer to its parent: `parent[i]`.
- The root of the tree points to itself: `parent[root] == root`.
- The root acts as the unique **canonical representative** of the set.

### 2. Path Compression
During the `find(i)` operation, every node along the path from $i$ up to the root is flattened to point directly to the root:
```java
public int find(int i) {
    if (parent[i] != i) {
        parent[i] = find(parent[i]); // Path compression
    }
    return parent[i];
}
```

### 3. Union by Rank / Size
When merging two sets with roots $r_1$ and $r_2$:
- **Union by Rank**: Attach the tree with smaller height (rank) under the root of the tree with greater height. Rank increments only when two trees of equal rank are merged.
- **Union by Size**: Attach the tree with fewer total nodes under the tree with more nodes.

Combining **Path Compression** with **Union by Rank** bounds the amortized time per operation to $O(\alpha(N))$.

---

## 3. How It Works Internally

### The Inverse Ackermann Function $\alpha(N)$
The Ackermann function $A(m, n)$ grows at a super-exponential rate:
- $A(1, 1) = 3$
- $A(2, 2) = 7$
- $A(3, 3) = 61$
- $A(4, 4) = 2^{2^{2^{65536}}} - 3$ (a number vastly exceeding the estimated count of atoms in the known universe $\approx 10^{80}$).

The **Inverse Ackermann Function** $\alpha(N)$ is the inverse of $A(n, n)$:
$$\alpha(N) \le 4 \quad \text{for all } N \le 10^{80}$$
In practical computer science, $\alpha(N)$ is a mathematical constant $\le 4$, meaning DSU operations execute in amortized constant time.

```
Path Compression Visualization:
Before find(4):
      0 (Root)
      |
      1
      |
      2
      |
      3
      |
      4

After find(4):
        0 (Root)
      / | \ \ \
     1  2  3 4
All nodes point directly to 0!
```

---

## 4. Syntax & Usage

### Production DSU Template

```java
public class DisjointSetUnion {
    private final int[] parent;
    private final int[] rank;
    private int components;

    public DisjointSetUnion(int n) {
        this.parent = new int[n];
        this.rank = new int[n];
        this.components = n;
        for (int i = 0; i < n; i++) {
            parent[i] = i;
            rank[i] = 0;
        }
    }

    public int find(int i) {
        if (parent[i] != i) {
            parent[i] = find(parent[i]); // Path compression
        }
        return parent[i];
    }

    public boolean union(int i, int j) {
        int rootI = find(i);
        int rootJ = find(j);
        if (rootI == rootJ) return false; // Already connected

        // Union by rank
        if (rank[rootI] < rank[rootJ]) {
            parent[rootI] = rootJ;
        } else if (rank[rootI] > rank[rootJ]) {
            parent[rootJ] = rootI;
        } else {
            parent[rootJ] = rootI;
            rank[rootI]++;
        }

        components--;
        return true;
    }

    public boolean connected(int i, int j) {
        return find(i) == find(j);
    }

    public int getComponentCount() {
        return components;
    }
}
```

---

## 5. Practical Examples

### Example 1: Number of Provinces (LeetCode 547)

```java
public class NumberOfProvinces {
    public int findCircleNum(int[][] isConnected) {
        int n = isConnected.length;
        int[] parent = new int[n];
        for (int i = 0; i < n; i++) parent[i] = i;
        int provinces = n;

        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if (isConnected[i][j] == 1) {
                    int rI = find(parent, i);
                    int rJ = find(parent, j);
                    if (rI != rJ) {
                        parent[rI] = rJ;
                        provinces--;
                    }
                }
            }
        }

        return provinces;
    }

    private int find(int[] p, int i) {
        if (p[i] != i) p[i] = find(p, p[i]);
        return p[i];
    }
}
```

### Example 2: Redundant Connection (LeetCode 684)

```java
public class RedundantConnection {
    public int[] findRedundantConnection(int[][] edges) {
        int n = edges.length;
        int[] parent = new int[n + 1];
        for (int i = 1; i <= n; i++) parent[i] = i;

        for (int[] edge : edges) {
            int u = edge[0], v = edge[1];
            int rootU = find(parent, u);
            int rootV = find(parent, v);

            // An edge between already-connected vertices is redundant (forms cycle)
            if (rootU == rootV) {
                return edge;
            }
            parent[rootU] = rootV;
        }

        return new int[0];
    }

    private int find(int[] p, int i) {
        if (p[i] != i) p[i] = find(p, p[i]);
        return p[i];
    }
}
```

### Example 3: Accounts Merge (LeetCode 721, String DSU)

```java
import java.util.*;

public class AccountsMerge {
    static class UnionFind {
        int[] parent;
        public UnionFind(int n) {
            parent = new int[n];
            for (int i = 0; i < n; i++) parent[i] = i;
        }
        public int find(int i) {
            if (parent[i] != i) parent[i] = find(parent[i]);
            return parent[i];
        }
        public void union(int i, int j) {
            parent[find(i)] = find(j);
        }
    }

    public List<List<String>> accountsMerge(List<List<String>> accounts) {
        int n = accounts.size();
        UnionFind uf = new UnionFind(n);
        Map<String, Integer> emailToAcc = new HashMap<>();

        for (int i = 0; i < n; i++) {
            List<String> acc = accounts.get(i);
            for (int j = 1; j < acc.size(); j++) {
                String email = acc.get(j);
                if (emailToAcc.containsKey(email)) {
                    uf.union(i, emailToAcc.get(email));
                } else {
                    emailToAcc.put(email, i);
                }
            }
        }

        Map<Integer, Set<String>> merged = new HashMap<>();
        for (Map.Entry<String, Integer> entry : emailToAcc.entrySet()) {
            int rootAcc = uf.find(entry.getValue());
            merged.computeIfAbsent(rootAcc, k -> new TreeSet<>()).add(entry.getKey());
        }

        List<List<String>> result = new ArrayList<>();
        for (Map.Entry<Integer, Set<String>> entry : merged.entrySet()) {
            List<String> row = new ArrayList<>();
            row.add(accounts.get(entry.getKey()).get(0)); // Name
            row.addAll(entry.getValue()); // Sorted emails
            result.add(row);
        }

        return result;
    }
}
```

### Example 4: Number of Operations to Make Network Connected (LeetCode 1319)

```java
public class MakeNetworkConnected {
    public int makeConnected(int n, int[][] connections) {
        // Need at least n - 1 cables to connect n computers
        if (connections.length < n - 1) return -1;

        int[] parent = new int[n];
        for (int i = 0; i < n; i++) parent[i] = i;

        int components = n;
        for (int[] c : connections) {
            int rU = find(parent, c[0]);
            int rV = find(parent, c[1]);
            if (rU != rV) {
                parent[rU] = rV;
                components--;
            }
        }

        // To connect K components, we need K - 1 redundant cables
        return components - 1;
    }

    private int find(int[] p, int i) {
        if (p[i] != i) p[i] = find(p, p[i]);
        return p[i];
    }
}
```

### Example 5: Satisfiability of Equality Equations (LeetCode 990)

```java
public class EqualityEquations {
    public boolean equationsPossible(String[] equations) {
        int[] parent = new int[26];
        for (int i = 0; i < 26; i++) parent[i] = i;

        // Pass 1: Union all equality constraints
        for (String eq : equations) {
            if (eq.charAt(1) == '=') {
                int u = eq.charAt(0) - 'a';
                int v = eq.charAt(3) - 'a';
                parent[find(parent, u)] = find(parent, v);
            }
        }

        // Pass 2: Check inequality contradictions
        for (String eq : equations) {
            if (eq.charAt(1) == '!') {
                int u = eq.charAt(0) - 'a';
                int v = eq.charAt(3) - 'a';
                if (find(parent, u) == find(parent, v)) {
                    return false; // Contradiction!
                }
            }
        }

        return true;
    }

    private int find(int[] p, int i) {
        if (p[i] != i) p[i] = find(p, p[i]);
        return p[i];
    }
}
```

---

## 6. Best Practices

1. **Always Implement Path Compression**: A single line in `find`: `parent[i] = find(parent[i])` reduces tree height exponentially.
2. **Combine with Union by Rank or Size**: Path compression alone yields $O(N \log N)$ worst-case; combining both achieves the optimal $O(N \alpha(N))$.
3. **Use Primitive Arrays When IDs Are Contiguous**: For nodes $0 \dots N-1$, `int[] parent` is maximally cache-friendly and runs 10x faster than `Map<T, T>`.
4. **Track Component Count In Place**: Decrement a `components` counter inside `union()` when `rootI != rootJ` to answer connected component queries in $O(1)$ without rescanning.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Omitting path compression | Trees can degrade to linked lists ($O(N)$ per operation) | Use `parent[i] = find(parent[i])`. |
| Comparing nodes instead of roots | `parent[u] == parent[v]` checks immediate parent, not root | Always compare `find(u) == find(v)`. |
| Unbalanced union without rank/size | Worst-case skewed tree construction | Use union by rank or union by size. |
| Forgetting 1-indexed conversion | `ArrayIndexOutOfBoundsException` on graphs numbered $1 \dots N$ | Size arrays $N + 1$ or subtract 1. |

---

## 8. Performance & Security Notes

- **Time Complexity**:
  - `find()`: Amortized $O(\alpha(N))$
  - `union()`: Amortized $O(\alpha(N))$
  - `connected()`: Amortized $O(\alpha(N))$
- **Space Complexity**: $O(N)$ for contiguous `parent` and `rank` arrays.
- **Cache Friendliness**: Because `parent` and `rank` are flat integer arrays, they benefit from hardware prefetching and CPU L1 cache hits.

---

## 9. Comparison / When to Use What

| Feature | Disjoint Set Union (DSU) | BFS / DFS |
|---|---|---|
| **Dynamic Edge Additions** | $O(\alpha(N))$ per edge | $O(V + E)$ per query |
| **Path Reconstruction** | Not supported (only sets) | Natural (tracks exact paths) |
| **Cycle Detection** | Instantaneous ($O(\alpha(N))$) | $O(V + E)$ |
| **Memory Footprint** | Extremely minimal ($8N$ bytes) | Requires full adjacency lists |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between Path Compression and Path Halving?</summary>

In standard Path Compression, every node on the path points directly to the root (2-pass recursion). In Path Halving, every node points to its grandparent (`parent[i] = parent[parent[i]]`) in a single iterative pass. Both have the same asymptotic amortized complexity $O(\alpha(N))$.
</details>

<details>
<summary>2. Why does DSU not naturally support an efficient `disconnect()` or edge removal operation?</summary>

Because path compression flattens the tree and permanently destroys the original graph topology. Undoing a union requires either maintaining an operation history stack (Undoable DSU / Persistent DSU, which disallows path compression) or rerunning BFS/DFS.
</details>

<details>
<summary>3. Can DSU be used to detect cycles in a directed graph?</summary>

No. DSU assumes undirected connectivity (if $A$ connects to $B$ and $B$ to $C$, then $A$ connects to $C$ symmetrically). In directed graphs, $A \to B$ and $C \to B$ do not form a cycle, but DSU would report them as connected. Directed cycles require DFS 3-coloring or Kahn's algorithm.
</details>

<details>
<summary>4. What is Weighted / Potential Union-Find?</summary>

A variant where each node maintains a relation or relative weight difference with respect to its parent: `diff[i] = val[i] - val[parent[i]]`. It can evaluate relative distances, potential differences, and equations with continuous variables ($x - y = k$) in $O(\alpha(N))$.
</details>

<details>
<summary>5. What is the maximum value of the Inverse Ackermann function $\alpha(N)$ in the physical universe?</summary>

$\alpha(N) \le 4$ for all $N \le 10^{80}$. Because the observable universe has fewer than $10^{80}$ atoms, $\alpha(N) \le 4$ across all possible computing workloads.
</details>

<details>
<summary>6. How can DSU find the size of the component an element belongs to?</summary>

Maintain a `size[]` array initialized to 1. When merging root $B$ under root $A$, update `size[A] += size[B]`. Querying `size[find(i)]` gives the exact component size in $O(\alpha(N))$.
</details>

<details>
<summary>7. How do you implement DSU with rollback capability?</summary>

Do **not** use path compression. Use only union by rank or size. Record each modification `(node, oldParent, rankChanged)` on an explicit stack. Calling `rollback()` pops the stack and restores previous references, running each operation in $O(\log N)$ time.
</details>

<details>
<summary>8. How do you find the minimum number of edges to connect a disconnected graph?</summary>

Count the number of connected components $C$ using DSU. The minimum number of edges required to connect all vertices into a single component is strictly $C - 1$.
</details>

---

## 11. Practice Exercises

### Exercise 1: Smallest String With Swaps (LeetCode 1202)
Given a string and index pairs that can be swapped repeatedly, return the lexicographically smallest string possible.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class SmallestStringWithSwaps {
    public String smallestStringWithSwaps(String s, List<List<Integer>> pairs) {
        int n = s.length();
        int[] parent = new int[n];
        for (int i = 0; i < n; i++) parent[i] = i;

        for (List<Integer> pair : pairs) {
            int rU = find(parent, pair.get(0));
            int rV = find(parent, pair.get(1));
            if (rU != rV) parent[rU] = rV;
        }

        Map<Integer, PriorityQueue<Character>> groups = new HashMap<>();
        for (int i = 0; i < n; i++) {
            int root = find(parent, i);
            groups.computeIfAbsent(root, k -> new PriorityQueue<>()).offer(s.charAt(i));
        }

        StringBuilder sb = new StringBuilder(n);
        for (int i = 0; i < n; i++) {
            int root = find(parent, i);
            sb.append(groups.get(root).poll());
        }

        return sb.toString();
    }

    private int find(int[] p, int i) {
        if (p[i] != i) p[i] = find(p, p[i]);
        return p[i];
    }
}
```
</details>

### Exercise 2: Regions Cut By Slashes (LeetCode 959)
An $N \times N$ grid is filled with '/', '\', or ' '. Find the number of regions divided by the slashes.

<details>
<summary>Solution</summary>

```java
public class RegionsCutBySlashes {
    public int regionsBySlashes(String[] grid) {
        int n = grid.length;
        // Divide each 1x1 cell into 4 triangles: 0=Top, 1=Right, 2=Bottom, 3=Left
        int totalTriangles = 4 * n * n;
        int[] parent = new int[totalTriangles];
        for (int i = 0; i < totalTriangles; i++) parent[i] = i;

        int regions = totalTriangles;

        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) {
                int base = 4 * (r * n + c);
                char ch = grid[r].charAt(c);

                // Internal connections
                if (ch != '/') {
                    if (union(parent, base + 0, base + 1)) regions--;
                    if (union(parent, base + 2, base + 3)) regions--;
                }
                if (ch != '\\') {
                    if (union(parent, base + 0, base + 3)) regions--;
                    if (union(parent, base + 1, base + 2)) regions--;
                }

                // Neighbor connections
                if (r + 1 < n) {
                    int bottomBase = 4 * ((r + 1) * n + c);
                    if (union(parent, base + 2, bottomBase + 0)) regions--;
                }
                if (c + 1 < n) {
                    int rightBase = 4 * (r * n + (c + 1));
                    if (union(parent, base + 1, rightBase + 3)) regions--;
                }
            }
        }

        return regions;
    }

    private int find(int[] p, int i) {
        if (p[i] != i) p[i] = find(p, p[i]);
        return p[i];
    }

    private boolean union(int[] p, int i, int j) {
        int rI = find(p, i), rJ = find(p, j);
        if (rI == rJ) return false;
        p[rI] = rJ;
        return true;
    }
}
```
</details>

### Exercise 3: Similar String Groups (LeetCode 839)
Two strings are similar if they are identical or can be made identical by swapping two characters. Return the number of similar string groups.

<details>
<summary>Solution</summary>

```java
public class SimilarStringGroups {
    public int numSimilarGroups(String[] strs) {
        int n = strs.length;
        int[] parent = new int[n];
        for (int i = 0; i < n; i++) parent[i] = i;

        int groups = n;
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if (isSimilar(strs[i], strs[j])) {
                    int rI = find(parent, i);
                    int rJ = find(parent, j);
                    if (rI != rJ) {
                        parent[rI] = rJ;
                        groups--;
                    }
                }
            }
        }

        return groups;
    }

    private boolean isSimilar(String a, String b) {
        int diff = 0;
        for (int i = 0; i < a.length(); i++) {
            if (a.charAt(i) != b.charAt(i)) {
                if (++diff > 2) return false;
            }
        }
        return diff == 0 || diff == 2;
    }

    private int find(int[] p, int i) {
        if (p[i] != i) p[i] = find(p, p[i]);
        return p[i];
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Primitives**: `find(u)` returns representative root; `union(u, v)` merges disjoint sets.
- **Path Compression**: `parent[i] = find(parent[i])` flattens trees to depth 1.
- **Union by Rank**: Always hang the shorter tree under the taller tree.
- **Complexity**: Amortized $O(\alpha(N)) \le 4$ per operation.
- **Application Scope**: Undirected dynamic connectivity, cycle detection, Kruskal's MST, clustering.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 21: Data Structures for Disjoint Sets](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Tarjan, R. E. (1975). Efficiency of a Good But Not Linear Set Union Algorithm](https://dl.acm.org/doi/10.1145/321879.321884)
- [Robert Sedgewick: Algorithms, 4th Edition: Union-Find](https://algs4.cs.princeton.edu/15uf/)
- [Baeldung: Disjoint Set Union in Java](https://www.baeldung.com/java-union-find)
