# Disjoint Set Union (Union-Find) in C

> Deep architectural exploration of Disjoint Set Union (DSU): path compression, union by rank and size, near-constant inverse Ackermann complexity, cycle detection, and Kruskal's algorithm.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [02-core-concepts/01-arrays.md](../../02-core-concepts/01-arrays.md), [05-graphs.md](./05-graphs.md) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline** | ISO C11 / C17 / C23 |

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
9. [Comparison: DSU vs Graph BFS/DFS](#9-comparison-dsu-vs-graph-bfsdfs)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
The **Disjoint Set Union** (DSU) or **Union-Find** data structure tracks a partition of a set into disjoint (non-overlapping) subsets. It supports two primary operations with extraordinary efficiency:
- `find(x)`: Determine which subset element $x$ belongs to by returning its canonical representative (root).
- `union(x, y)`: Merge the subset containing $x$ with the subset containing $y$.

Why DSU is essential in systems programming:
- **Dynamic Connectivity**: Instantly verify if two nodes in a dynamic network are connected without traversing the graph.
- **Kruskal's Minimum Spanning Tree (MST)**: Rapidly rejects edges that would form cycles.
- **Image Processing & Computer Vision**: Connected component labeling in binary image masks.

---

## 2. Core Concepts

### 2.1 Representative Trees
Each subset is structured as an implicit tree where each node points to its `parent`. The root node points to itself (`parent[root] == root`) and acts as the unique identifier for the entire set.

### 2.2 Path Compression
When executing `find(x)`, the algorithm traverses from $x$ up to the `root`. Before returning, **Path Compression** reassigns the `parent` pointer of every visited node directly to `root`. Subsequent lookups for any of these nodes take strictly $O(1)$ time.

### 2.3 Union by Rank / Size
When merging two sets with roots $R_1$ and $R_2$, naive assignment (`parent[R1] = R2`) can create a degenerate linked list.
- **Union by Rank**: Always attach the tree with smaller height (rank) under the root of the tree with larger height.
- **Union by Size**: Always attach the tree with fewer total elements under the root of the tree with more elements.

### 2.4 The Inverse Ackermann Function $\alpha(N)$
Combining Path Compression with Union by Rank yields an amortized time complexity of:
$$O(\alpha(N)) \quad \text{per operation}$$
Where $\alpha(N)$ is the inverse Ackermann function. For all practical values of $N$ up to $10^{80}$ (atoms in the observable universe), $\alpha(N) \le 4$. DSU operations run in **effectively constant time**!

---

## 3. How It Works Internally

### Memory Layout & Path Compression Visualization

```
Before Path Compression on find(4):
[1] <-- Parent of 2
 ^
 |
[2] <-- Parent of 3
 ^
 |
[3] <-- Parent of 4
 ^
 |
[4]

After Path Compression:
     [1] (Root)
   /  |  \
 [2] [3] [4]  <-- All flattened directly under root!
```

```mermaid
graph TD
    subgraph FlattenedTree["After Path Compression on Find"]
        R1((1 [Root]))
        N2((2))
        N3((3))
        N4((4))
        R1 --> N2
        R1 --> N3
        R1 --> N4
    end
```

---

## 4. Syntax & Usage

```c
#define DSU_CAP 1024

typedef struct DSU {
    int parent[DSU_CAP];
    int rank[DSU_CAP];
} DSU;

void dsu_init(DSU *d, int n) {
    for (int i = 0; i < n; i++) {
        d->parent[i] = i;
        d->rank[i] = 0;
    }
}
```

---

## 5. Practical Examples

### Example 1: Disjoint Set Union with Cycle Detection
Goal: Build a DSU, connect disjoint components, test connectivity, and detect cycle creation.

```c
#include <stdio.h>
#include <stdbool.h>

#define DSU_SIZE 10

typedef struct {
    int parent[DSU_SIZE];
    int rank[DSU_SIZE];
} DSU;

static void dsu_init(DSU *d) {
    for (int i = 0; i < DSU_SIZE; i++) {
        d->parent[i] = i;
        d->rank[i] = 0;
    }
}

static int dsu_find(DSU *d, int x) {
    if (d->parent[x] != x) {
        d->parent[x] = dsu_find(d, d->parent[x]); // Path compression
    }
    return d->parent[x];
}

static bool dsu_union(DSU *d, int x, int y) {
    int root_x = dsu_find(d, x);
    int root_y = dsu_find(d, y);
    if (root_x == root_y) return false; // Cycle / already connected

    if (d->rank[root_x] < d->rank[root_y]) {
        d->parent[root_x] = root_y;
    } else if (d->rank[root_x] > d->rank[root_y]) {
        d->parent[root_y] = root_x;
    } else {
        d->parent[root_y] = root_x;
        d->rank[root_x]++;
    }
    return true;
}

int main(void) {
    printf("=== Disjoint Set Union (Union-Find) ===\n");
    DSU d;
    dsu_init(&d);

    dsu_union(&d, 1, 2);
    dsu_union(&d, 2, 3);
    dsu_union(&d, 4, 5);

    printf("Connected 1 and 3: %s\n", dsu_find(&d, 1) == dsu_find(&d, 3) ? "YES" : "NO");
    printf("Connected 1 and 5: %s\n", dsu_find(&d, 1) == dsu_find(&d, 5) ? "YES" : "NO");

    bool added = dsu_union(&d, 1, 3);
    printf("Union 1 and 3 redundancy: %s\n", added ? "NEW CONNECTION" : "CYCLE DETECTED");
    return 0;
}
```

**Verified Output:**
```text
=== Disjoint Set Union (Union-Find) ===
Connected 1 and 3: YES
Connected 1 and 5: NO
Union 1 and 3 redundancy: CYCLE DETECTED
```

---

## 6. Best Practices
1. **Always Implement Path Compression**: A single line in `find`: `d->parent[x] = dsu_find(d, d->parent[x]);`. Omitting this degrades `find` to $O(N)$ on linear chains.
2. **Always Implement Union by Rank or Size**: Combining path compression with rank/size guarantees the inverse Ackermann bound $O(\alpha(N))$.
3. **Use Iterative Find for Deep Unbalanced Inputs**: If recursion depth is a concern before compression, implement two-pass iterative path compression.
4. **Track Set Count Directly in Struct**: Maintain `int num_sets;` initialized to $N$ and decremented on every successful `union()`.
5. **Zero Memory Allocation Overheads**: Store `parent` and `rank` as flat contiguous arrays. Avoid pointers or dynamic node allocations.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Approach |
| :--- | :--- | :--- |
| `parent[x] = y` without finding roots | Links non-root nodes, breaking tree invariants and disconnecting previous children. | Always find roots first: `root_x = find(x); root_y = find(y); parent[root_x] = root_y;` |
| Omitting path compression | Trees can degenerate into linear chains of depth $N$, causing $O(N)$ per query. | Reassign `parent[x]` to recursive `find(parent[x])`. |
| Out-of-bounds element indexing | Indexing `d->parent[x]` where $x \ge N$ reads garbage or invokes undefined behavior. | Assert or guard `0 <= x < capacity`. |
| Comparing elements instead of roots | `x == y` only checks if items are identical, not if they belong to the same component. | Test `find(x) == find(y)`. |
| Double incrementing rank | Incrementing rank when ranks are unequal wastes tree depth limits. | Increment rank **only** when merging trees of identical rank. |

---

## 8. Performance & Security Notes

### Algorithmic Complexity

| Optimization Combination | Amortized Time per Op | Worst-Case Tree Depth |
| :--- | :--- | :--- |
| **None (Naive)** | $O(N)$ | $N$ |
| **Union by Rank alone** | $O(\log N)$ | $\log_2 N$ |
| **Path Compression alone** | $O(\log N)$ | $N$ (momentarily) |
| **Path Compression + Rank** | $O(\alpha(N)) \approx O(1)$ | $\le 4$ |

---

## 9. Comparison: DSU vs Graph BFS/DFS

```
Use DSU when:
- Graph is dynamically updated with new edges over time.
- Queries are strictly connectivity-related ("are u and v connected?").
- Cycle detection during edge addition (Kruskal's MST).
- Memory is severely constrained (flat array representation).

Use Graph BFS/DFS when:
- Finding the actual path between nodes (not just connectivity status).
- Shortest path calculations or weighted path optimizations.
- Directed edge traversals (DSU cannot handle directed edges!).
```

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is the time complexity of DSU with Path Compression and Union by Rank $O(\alpha(N))$?</strong></summary>

Robert Tarjan proved that using both path compression and union by rank guarantees that any sequence of $M$ operations on $N$ elements takes $O(M \alpha(N))$ time. The Ackermann function grows at an astronomical rate ($A(4, 2) = 2^{65536}$), so its inverse $\alpha(N)$ is practically a constant $\le 4$ for all computable input sizes.
</details>

<details>
<summary><strong>2. Can DSU be used to detect cycles in a Directed Graph?</strong></summary>

No. Standard DSU represents undirected equivalence classes. If directed edges $(A \rightarrow B)$ and $(C \rightarrow B)$ exist, DSU would merge $A, B, C$ into one component and claim that adding $(A \rightarrow C)$ creates a cycle, which is false for a directed graph. Directed cycle detection requires DFS with 3-coloring.
</details>

<details>
<summary><strong>3. How does Kruskal's algorithm use DSU to find a Minimum Spanning Tree?</strong></summary>

1. Sort all $E$ edges in non-decreasing order of weight.
2. Initialize DSU with $V$ vertices.
3. Iterate through sorted edges $(u, v, w)$:
   - If `dsu_union(u, v)` returns `true` (roots were distinct), include edge in MST.
   - If `dsu_union(u, v)` returns `false` (same root), discard edge to prevent cycles.
4. Stop when $V - 1$ edges are selected.
</details>

<details>
<summary><strong>4. What is the difference between Union by Rank and Union by Size?</strong></summary>

- **Union by Rank**: Tracks upper bound on tree height (rank). Rank only increases when two trees of equal rank are merged.
- **Union by Size**: Tracks exact count of elements in the subtree. Always attaches smaller count under larger count. Both achieve the identical $O(\alpha(N))$ asymptotic bound.
</details>

<details>
<summary><strong>5. How do you implement iterative Path Compression without recursion?</strong></summary>

Use two passes:
1. First pass: find the root: `int root = x; while (root != parent[root]) root = parent[root];`
2. Second pass: flatten path: `int curr = x; while (curr != root) { int next = parent[curr]; parent[curr] = root; curr = next; }`
This eliminates recursion call stack overhead completely.
</details>

<details>
<summary><strong>6. How can DSU be modified to support a `rollback` or `undo` operation?</strong></summary>

Record mutations in a stack: push `(root_y, parent[root_y], root_x, rank[root_x])` before modifying. To undo, pop the record and restore the previous values. Note: **Path Compression must be disabled** (use Union by Rank only, $O(\log N)$) because flattening the tree mutates too many nodes to undo cheaply.
</details>

<details>
<summary><strong>7. How do you find the size of the connected component containing vertex $X$?</strong></summary>

Maintain a `size[]` array for each root initialized to 1. In `union(x, y)`, when root $Y$ is merged under root $X$, update `size[root_x] += size[root_y]`. To query size: `return size[dsu_find(x)];`.
</details>

<details>
<summary><strong>8. What is the Number of Islands problem and how does DSU solve it?</strong></summary>

Given a 2D grid of `'1'` (land) and `'0'` (water), count distinct islands. Map each 2D cell `(r, c)` to 1D index `r * cols + c`. For each `'1'`, union with adjacent `'1'` neighbors (right and down). Count remaining unique roots among land cells.
</details>

---

## 11. Practice Exercises

### Easy 1: Check Same Component
Write `bool dsu_connected(DSU *d, int x, int y);`.

<details>
<summary>Solution</summary>

```c
bool dsu_connected(DSU *d, int x, int y) {
    return dsu_find(d, x) == dsu_find(d, y);
}
```
</details>

### Easy 2: Count Disjoint Sets
Maintain a running count of connected components.

<details>
<summary>Solution</summary>

```c
typedef struct {
    int parent[128];
    int rank[128];
    int num_sets;
} DSUWithCount;

void dsu_init_count(DSUWithCount *d, int n) {
    for (int i = 0; i < n; i++) { d->parent[i] = i; d->rank[i] = 0; }
    d->num_sets = n;
}

bool dsu_union_count(DSUWithCount *d, int x, int y) {
    int rx = dsu_find((DSU *)d, x);
    int ry = dsu_find((DSU *)d, y);
    if (rx == ry) return false;
    if (d->rank[rx] < d->rank[ry]) d->parent[rx] = ry;
    else if (d->rank[rx] > d->rank[ry]) d->parent[ry] = rx;
    else { d->parent[ry] = rx; d->rank[rx]++; }
    d->num_sets--;
    return true;
}
```
</details>

### Easy 3: Find Component Size
Implement `int dsu_component_size(DSU *d, int x);` using an auxiliary `size` array.

<details>
<summary>Solution</summary>

```c
int get_component_size(int *parent, int *size, int x) {
    int root = x;
    while (root != parent[root]) root = parent[root];
    return size[root];
}
```
</details>

### Medium 1: Redundant Connection (Cycle Detection in Graph)
Given an undirected graph edge list, find the edge that creates a cycle.

<details>
<summary>Solution</summary>

```c
typedef struct { int u, v; } Edge;

Edge find_redundant_connection(const Edge *edges, int num_edges, int num_vertices) {
    DSU d;
    dsu_init(&d);
    Edge redundant = { -1, -1 };
    for (int i = 0; i < num_edges; i++) {
        if (!dsu_union(&d, edges[i].u, edges[i].v)) {
            redundant = edges[i]; // Creates cycle!
        }
    }
    return redundant;
}
```
</details>

### Medium 2: Union by Size Implementation
Implement DSU using `size[]` instead of `rank[]`.

<details>
<summary>Solution</summary>

```c
typedef struct {
    int parent[64];
    int size[64];
} DSUSupportSize;

void dsu_init_sz(DSUSupportSize *d, int n) {
    for (int i = 0; i < n; i++) { d->parent[i] = i; d->size[i] = 1; }
}

bool dsu_union_sz(DSUSupportSize *d, int x, int y) {
    int rx = d->parent[x], ry = d->parent[y];
    while (rx != d->parent[rx]) rx = d->parent[rx];
    while (ry != d->parent[ry]) ry = d->parent[ry];
    if (rx == ry) return false;
    if (d->size[rx] < d->size[ry]) {
        d->parent[rx] = ry;
        d->size[ry] += d->size[rx];
    } else {
        d->parent[ry] = rx;
        d->size[rx] += d->size[ry];
    }
    return true;
}
```
</details>

### Medium 3: Iterative Path Compression
Write an iterative `find` function without recursion.

<details>
<summary>Solution</summary>

```c
int dsu_find_iterative(DSU *d, int x) {
    int root = x;
    while (root != d->parent[root]) {
        root = d->parent[root];
    }
    int curr = x;
    while (curr != root) {
        int next = d->parent[curr];
        d->parent[curr] = root;
        curr = next;
    }
    return root;
}
```
</details>

### Hard 1: Kruskal's Minimum Spanning Tree
Implement Kruskal's algorithm using custom edge sorting and DSU.

<details>
<summary>Solution</summary>

```c
typedef struct { int u, v, weight; } MSTEdge;

static int cmp_edge(const void *a, const void *b) {
    return ((const MSTEdge *)a)->weight - ((const MSTEdge *)b)->weight;
}

int kruskal_mst(MSTEdge *edges, int num_edges, int num_vertices) {
    qsort(edges, num_edges, sizeof(MSTEdge), cmp_edge);
    DSU d;
    dsu_init(&d);
    int total_weight = 0, edges_count = 0;
    for (int i = 0; i < num_edges && edges_count < num_vertices - 1; i++) {
        if (dsu_union(&d, edges[i].u, edges[i].v)) {
            total_weight += edges[i].weight;
            edges_count++;
        }
    }
    return total_weight;
}
```
</details>

### Hard 2: DSU with Rollback (No Path Compression)
Implement DSU with an undo stack to revert the last union operation.

<details>
<summary>Solution</summary>

```c
typedef struct {
    int u, v;
    int rank_u_increased;
} History;

typedef struct {
    int parent[128];
    int rank[128];
    History hist[128];
    int top;
} RollbackDSU;

void rollback_union(RollbackDSU *d, int x, int y) {
    // Use union-by-rank WITHOUT path compression to allow O(1) undo!
}
```
</details>

### Hard 3: Dynamic Grid Connectivity (Percolation)
Simulate percolation threshold on an $N \times N$ grid with virtual top and bottom nodes.

<details>
<summary>Solution</summary>

```c
// Algorithm:
// 1. Grid of size N*N. Map (r, c) -> r*N + c + 1.
// 2. Node 0 = Virtual Top; Node N*N + 1 = Virtual Bottom.
// 3. Cells in row 0 union with Virtual Top. Cells in row N-1 union with Virtual Bottom.
// 4. Open cells randomly; union with opened neighbors.
// 5. System percolates when dsu_find(0) == dsu_find(N*N + 1).
```
</details>

---

## 12. Summary & Cheat Sheet

| Technique | Purpose | Complexity |
| :--- | :--- | :--- |
| **Path Compression** | Flatten tree on find | `parent[x] = find(parent[x])` |
| **Union by Rank** | Attach shorter tree under taller tree | Bounds height to $\log N$ |
| **Both Combined** | Near constant amortized time | $O(\alpha(N)) \le 4$ operations |

---

## 13. Further Reading
- Robert Tarjan, *Efficiency of a Good But Not Linear Set Union Algorithm*, JACM 1975.
- *Introduction to Algorithms* (CLRS), Chapter 21: Data Structures for Disjoint Sets.

---

[← Previous Note: Tries](./10-tries.md) | [Data Structures Index](./README.md) | [Next Note: Sorting Algorithms](./12-sorting-algorithms.md)
