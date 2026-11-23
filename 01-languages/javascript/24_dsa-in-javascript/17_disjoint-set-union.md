# Disjoint Set Union (Union-Find) in JavaScript

> A production-grade algorithmic guide to the Disjoint Set Union (DSU / Union-Find) data structure in JavaScript, covering Path Compression, Union by Rank, Inverse Ackermann bounds, and Kruskal's MST.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [09_graphs-and-graph-algorithms.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/09_graphs-and-graph-algorithms.md) |
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

In graph theory, network routing, and cluster analysis, systems frequently need to track how elements are partitioned into disjoint (non-overlapping) sets:
- Dynamically tracking whether two users belong to the same social community.
- Determining if adding an edge to a network would create a cycle (Kruskal’s Minimum Spanning Tree).
- Connected component labeling in computer vision and image segmentation.

Checking connectivity between two nodes using standard BFS or DFS takes $O(V + E)$ time per query. If a system executes $100,000$ queries on a dynamic network, standard traversals become computationally prohibitive.

The **Disjoint Set Union (DSU)**, also called **Union-Find**, solves connectivity queries in **near-constant $O(\alpha(N))$ time** (where $\alpha$ is the Inverse Ackermann function, effectively $< 4$ for any real-world dataset size).

---

## 2. Core Concepts

### Core Operations
1. **`find(x)`**: Determines which set element $x$ belongs to by returning the unique **representative root** of $x$'s tree.
2. **`union(x, y)`**: Merges the set containing $x$ with the set containing $y$. Returns `true` if a new connection was made, or `false` if they were already in the same set (cycle detected!).
3. **`connected(x, y)`**: Checks if `find(x) === find(y)`.

### Two Crucial Optimizations
Without optimization, trees can degenerate into linear chains of height $N$, degrading operations to $O(N)$. Two techniques guarantee near-$O(1)$ performance:
1. **Path Compression (during `find`)**: Flattens the tree structure by making every visited node on the search path point directly to the root.
2. **Union by Rank / Size (during `union`)**: Always attaches the root of the shallower (or smaller) tree under the root of the deeper (or larger) tree, preventing tree depth from expanding.

```
Path Compression in Action:
Before find(4):           After find(4) with Path Compression:
    [ 1 ] (Root)                  [ 1 ]
      │                          /  |  \
    [ 2 ]                     [ 2 ][ 3 ][ 4 ]
      │                   (All nodes point directly to root!)
    [ 3 ]
      │
    [ 4 ]
```

---

## 3. How It Works Internally

The DSU structure is maintained using two compact 1D integer arrays:
- `parent[i]`: Points to the parent of node $i$. If `parent[i] === i`, node $i$ is a root.
- `rank[i]`: Represents the approximate tree height (or node count) rooted at $i$.

```
Index:    0   1   2   3   4
parent: [ 0 , 1 , 2 , 3 , 4 ]  (Initially every element is its own root)
rank:   [ 0 , 0 , 0 , 0 , 0 ]

After union(1, 2):
parent[2] = 1, rank[1] = 1
parent: [ 0 , 1 , 1 , 3 , 4 ]
```

---

## 4. Syntax & API Reference

### Production-Grade Disjoint Set Union Class
```javascript
/**
 * Production Disjoint Set Union (Union-Find)
 * Implements both Path Compression and Union by Rank.
 * Time: O(alpha(N)) per operation ≈ O(1)
 */
export class DisjointSetUnion {
  constructor(size) {
    this.parent = new Int32Array(size);
    this.rank = new Int32Array(size);
    this.numComponents = size;

    // Initially each node is its own parent
    for (let i = 0; i < size; i++) {
      this.parent[i] = i;
      this.rank[i] = 0;
    }
  }

  /**
   * Finds the representative root of x with Path Compression.
   */
  find(x) {
    if (this.parent[x] !== x) {
      // Path compression: flatten tree so node points directly to root
      this.parent[x] = this.find(this.parent[x]);
    }
    return this.parent[x];
  }

  /**
   * Merges sets containing x and y using Union by Rank.
   * Returns true if merged; false if they were already connected.
   */
  union(x, y) {
    const rootX = this.find(x);
    const rootY = this.find(y);

    if (rootX === rootY) {
      return false; // Already in same set; edge forms a cycle
    }

    // Attach smaller rank tree under higher rank tree
    if (this.rank[rootX] < this.rank[rootY]) {
      this.parent[rootX] = rootY;
    } else if (this.rank[rootX] > this.rank[rootY]) {
      this.parent[rootY] = rootX;
    } else {
      this.parent[rootY] = rootX;
      this.rank[rootX]++;
    }

    this.numComponents--;
    return true;
  }

  connected(x, y) {
    return this.find(x) === this.find(y);
  }
}
```

---

## 5. Practical Examples

### Example 1: Kruskal's Minimum Spanning Tree (MST)
```javascript
import { DisjointSetUnion } from './DisjointSetUnion.js';

/**
 * Kruskal's Minimum Spanning Tree Algorithm
 * Time: O(E log E) | Space: O(V)
 */
export function kruskalMST(numVertices, edges) {
  // edges: array of [u, v, weight]
  // 1. Sort edges ascending by weight
  edges.sort((a, b) => a[2] - b[2]);

  const dsu = new DisjointSetUnion(numVertices);
  const mstEdges = [];
  let totalWeight = 0;

  for (const [u, v, weight] of edges) {
    // If u and v are not in same component, union them and add to MST
    if (dsu.union(u, v)) {
      mstEdges.push([u, v, weight]);
      totalWeight += weight;

      // MST is complete when it has V - 1 edges
      if (mstEdges.length === numVertices - 1) break;
    }
  }

  return { mstEdges, totalWeight };
}
```

### Example 2: Number of Connected Components in an Undirected Graph
```javascript
import { DisjointSetUnion } from './DisjointSetUnion.js';

export function countComponents(n, edges) {
  const dsu = new DisjointSetUnion(n);

  for (const [u, v] of edges) {
    dsu.union(u, v);
  }

  return dsu.numComponents;
}

// console.log(countComponents(5, [[0,1], [1,2], [3,4]])); // 2 components ([0,1,2] and [3,4])
```

### Example 3: Detect Redundant Connection (Cycle Finder)
```javascript
import { DisjointSetUnion } from './DisjointSetUnion.js';

export function findRedundantDirectedEdge(n, edges) {
  const dsu = new DisjointSetUnion(n + 1); // 1-indexed vertices

  for (const [u, v] of edges) {
    // If union returns false, u and v are already connected -> this edge forms a cycle!
    if (!dsu.union(u, v)) {
      return [u, v];
    }
  }

  return [];
}
```

### Example 4: Accounts Merge (Email Alias Grouping)
```javascript
export function accountsMerge(accounts) {
  const emailToId = new Map();
  const emailToName = new Map();
  let id = 0;

  // 1. Assign unique integer ID to every unique email
  for (const [name, ...emails] of accounts) {
    for (const email of emails) {
      if (!emailToId.has(email)) {
        emailToId.set(email, id++);
        emailToName.set(email, name);
      }
    }
  }

  // 2. Union emails that appear in the same account
  const dsu = new DisjointSetUnion(id);
  for (const [_, ...emails] of accounts) {
    const firstId = emailToId.get(emails[0]);
    for (let i = 1; i < emails.length; i++) {
      dsu.union(firstId, emailToId.get(emails[i]));
    }
  }

  // 3. Group emails by representative DSU root ID
  const groups = new Map();
  for (const [email, emailId] of emailToId.entries()) {
    const rootId = dsu.find(emailId);
    if (!groups.has(rootId)) {
      groups.set(rootId, []);
    }
    groups.get(rootId).push(email);
  }

  // 4. Format sorted output
  const results = [];
  for (const [rootId, emails] of groups.entries()) {
    emails.sort();
    const name = emailToName.get(emails[0]);
    results.push([name, ...emails]);
  }

  return results;
}
```

### Example 5: Friend Circles / Social Clusters
```javascript
import { DisjointSetUnion } from './DisjointSetUnion.js';

export function findCircleNum(isConnected) {
  const n = isConnected.length;
  const dsu = new DisjointSetUnion(n);

  for (let i = 0; i < n; i++) {
    for (let j = i + 1; j < n; j++) {
      if (isConnected[i][j] === 1) {
        dsu.union(i, j);
      }
    }
  }

  return dsu.numComponents;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Omitting Path Compression**:
   If `find()` is written naively without path compression (`return find(parent[x])`), repeated union operations on skewed inputs create a tree of height $N$. Operations degrade from $O(\alpha(N))$ down to $O(N)$ linear time!
2. **0-Indexed vs 1-Indexed Vertices**:
   Many graph problems label nodes from $1 \dots N$ rather than $0 \dots N - 1$. Passing $1$-indexed vertices to an array allocated with size $N$ causes `IndexOutOfBounds` or writes out-of-bounds. Always allocate `size + 1` or subtract 1 from node IDs.
3. **Connecting Already Connected Nodes**:
   When implementing cycle detection, always test `find(u) === find(v)` *before* modifying edge counts. Calling `union()` returns `false` if nodes already share a root, cleanly flagging the cyclic edge.
4. **Disconnected Nodes in Component Counting**:
   Isolated nodes that participate in no edges must still be counted as individual components. The DSU constructor must initialize `this.numComponents = size`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Hazard | Production Fix |
| :--- | :--- | :--- |
| Omitting Path Compression | Trees degenerate into $O(N)$ linear chains; queries slow down 10,000x. | Flatten tree: `parent[x] = this.find(parent[x])`. |
| Omitting Union by Rank | Deep trees form on random merges. | Attach lower-rank root under higher-rank root. |
| Forgetting 1-indexed vertex offset | Out of bounds error on `parent[n]`. | Initialize DSU with `size + 1` or normalize indices to 0. |
| Setting `parent[x] = y` without finding roots | Links intermediate nodes instead of tree roots, breaking invariants. | Always union the roots: `rootX = find(x); rootY = find(y)`. |
| Allocating new objects for DSU nodes | Heavy GC overhead and memory bloat. | Use compact flat `Int32Array` buffers for `parent` and `rank`. |
| Decrementing component count on duplicate union | Under-counts component count when edge already exists. | Decrement `numComponents` only if `rootX !== rootY`. |

---

## 8. Performance Characteristics

- **Time Complexity per Operation**:
  - `find(x)`: **$O(\alpha(N))$** amortized.
  - `union(x, y)`: **$O(\alpha(N))$** amortized.
  - `connected(x, y)`: **$O(\alpha(N))$** amortized.
  - For all practical values of $N \le 10^{80}$ (atoms in the observable universe), the Inverse Ackermann function $\alpha(N) \le 4$. Thus, operations are **effectively $O(1)$ constant time**.
- **Space Complexity**:
  - **$O(N)$** linear memory, occupying two contiguous 32-bit integer arrays.

---

## 9. Engine Internals

### V8 TypedArray Flat Memory Locality
Using `new Int32Array(size)` for `this.parent` and `this.rank`:
- Allocates contiguous primitive 32-bit signed integers directly in V8 memory without object wrapper overhead.
- Because array indices are integers, accessing `parent[x]` compiles to direct pointer offset math without hash table lookups.
- A 1,000,000-element DSU consumes only **$8\text{MB}$ of RAM** ($4\text{MB}$ for `parent` + $4\text{MB}$ for `rank`).

---

## 10. Best Practices & Design Guidelines

1. **Always Implement Both Optimizations**: Always combine Path Compression with Union by Rank to achieve the theoretical $\alpha(N)$ bound.
2. **Track Set Sizes if Required**: If a problem requires knowing the size of a cluster, replace `rank` with a `size` array initialized to 1, updating `size[rootX] += size[rootY]` during union.
3. **Use DSU for Dynamic Graphs**: Reach for DSU whenever edge connections arrive dynamically over time and you must maintain connectivity status.

---

## 11. Real-World Case Study

### Computer Vision Connected Component Labeling
**Scenario**: An automated optical inspection system scanned microchips for manufacturing defects. High-resolution sensor cameras captured a $4096 \times 4096$ binary image ($16.7\text{M}$ pixels). The system needed to group adjacent defective white pixels into contiguous defect clusters (islands).
**Naive BFS**: Running BFS across $16.7\text{M}$ pixels consumed 1.8GB of RAM for queue objects and took $480\text{ms}$.

**DSU Two-Pass Scan**:
- Allocated a flat `Int32Array` DSU.
- Iterated pixels in a single raster scan pass, unioning each white pixel with its top and left neighbors ($O(\alpha(N))$).
- Counted unique roots to label all defect clusters.

**Outcome**:
- Memory consumption dropped to $67\text{MB}$.
- Processing time dropped from $480\text{ms}$ down to **$18\text{ms}$**, easily keeping pace with the 30 FPS production line conveyor belt.

---

## 12. Interview Questions

<details>
<summary>1. What is the Inverse Ackermann function $\alpha(N)$ and why is it significant in DSU?</summary>

The Ackermann function $A(m, n)$ is one of the fastest-growing mathematical functions. Its inverse, $\alpha(N)$, grows extremely slowly. While $A(4, 2)$ is an astronomical number ($2^{65536}$), $\alpha(N)$ remains strictly less than 5 for any conceivable input in our physical universe ($N < 10^{80}$). Consequently, an algorithm with time complexity $O(\alpha(N))$ performs at most 4 basic operations per lookup, which is mathematically indistinguishable from **$O(1)$ constant time** in all real-world applications.
</details>

<details>
<summary>2. How does Path Compression work during the <code>find</code> operation?</summary>

When traversing upward from node $x$ to its root, every intermediate node's parent pointer is updated to point directly to the root:
```javascript
parent[x] = find(parent[x]);
```
This flattens the tree. Subsequent lookups for any node along that path execute in a single hop ($O(1)$) rather than walking up the entire branch again.
</details>

<details>
<summary>3. Why do we need Union by Rank in addition to Path Compression?</summary>

While path compression flattens paths during `find()`, `union()` operations could still attach a deep tree to the leaf of another tree, creating a deep chain before `find()` has had a chance to compress it. Union by Rank guarantees that the root of the shallower tree is always attached under the root of the deeper tree, keeping tree height bounded by $O(\log N)$ even before path compression occurs. Combining both techniques yields the optimal $O(\alpha(N))$ runtime.
</details>

<details>
<summary>4. How does DSU detect cycles in an undirected graph?</summary>

For each edge $(u, v)$ in the graph:
1. Find the roots: `rootU = find(u)` and `rootV = find(v)`.
2. If `rootU === rootV`, vertices $u$ and $v$ are already connected in the same component. Adding an edge between them creates an alternate path, which forms a **cycle**.
3. If `rootU !== rootV`, call `union(u, v)` to connect them.
</details>

<details>
<summary>5. What is the difference between Union by Rank and Union by Size?</summary>

- **Union by Rank**: Tracks the approximate tree depth (rank). Rank increases by 1 only when two trees of identical rank are merged.
- **Union by Size**: Tracks the exact number of nodes in each subtree (`size[root]`). The tree with fewer nodes is attached under the tree with more nodes, and sizes are summed (`size[rootX] += size[rootY]`).
Both techniques achieve the identical asymptotic $O(\alpha(N))$ time complexity, but Union by Size has the practical advantage of allowing instantaneous $O(1)$ queries for component sizes.
</details>

<details>
<summary>6. How does Kruskal's Algorithm use DSU to construct a Minimum Spanning Tree?</summary>

1. Sort all $E$ edges in the graph in ascending order by weight ($O(E \log E)$).
2. Initialize a DSU with $V$ vertices.
3. Iterate through sorted edges: for each edge $(u, v)$, call `dsu.union(u, v)`.
4. If `union()` returns `true`, the edge connects two previously disjoint components without creating a cycle, so add it to the MST.
5. If `union()` returns `false`, skip the edge because it forms a cycle.
6. Terminate once $V - 1$ edges are accepted.
</details>

<details>
<summary>7. Can DSU easily support removing an edge (Disconnection)?</summary>

No. Standard DSU is an **append-only (incremental connectivity)** data structure. Because path compression flattens tree pointers, individual edge connections are dissolved, making it impossible to separate trees cleanly when an edge is deleted. Dynamic graphs requiring both edge additions and deletions require a more complex data structure like a **Link-Cut Tree** or Euler Tour Tree.
</details>

<details>
<summary>8. What is the space complexity of a DSU data structure?</summary>

**$\Theta(N)$ linear space**.
It requires storing two integer arrays of size $N$: one for `parent` references and one for `rank` (or `size`). In JavaScript, allocating these as `Int32Array` instances requires only $8N$ bytes of memory.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Number of Provinces
**Specification**: There are $n$ cities. Some are connected directly. If city $a$ is connected to $b$, and $b$ is connected to $c$, then $a$ is indirectly connected to $c$. A province is a group of directly or indirectly connected cities. Given an $n \times n$ matrix `isConnected`, return the total number of provinces.

<details>
<summary>Solution</summary>

```javascript
import { DisjointSetUnion } from './DisjointSetUnion.js';

export function findProvinces(isConnected) {
  const n = isConnected.length;
  const dsu = new DisjointSetUnion(n);

  for (let i = 0; i < n; i++) {
    for (let j = i + 1; j < n; j++) {
      if (isConnected[i][j] === 1) {
        dsu.union(i, j);
      }
    }
  }

  return dsu.numComponents;
}
```
</details>

### Exercise 2: Earliest Possible Time Everyone Becomes Friends
**Specification**: You are given a list of logs where `logs[i] = [timestamp, x, y]` indicates that $x$ and $y$ became friends at `timestamp`. Return the earliest timestamp at which every person in the network ($0 \dots n-1$) becomes friends with everyone else (all belong to one connected component), or `-1` if they never do.

<details>
<summary>Solution</summary>

```javascript
import { DisjointSetUnion } from './DisjointSetUnion.js';

export function earliestAcq(logs, n) {
  // Sort logs chronologically by timestamp
  logs.sort((a, b) => a[0] - b[0]);

  const dsu = new DisjointSetUnion(n);

  for (const [timestamp, x, y] of logs) {
    dsu.union(x, y);

    // If all people are now in a single component, return current timestamp
    if (dsu.numComponents === 1) {
      return timestamp;
    }
  }

  return -1;
}
```
</details>
