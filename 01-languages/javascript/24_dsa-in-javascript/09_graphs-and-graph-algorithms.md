# Graphs & Graph Algorithms in JavaScript

> A production-grade algorithmic guide to Graphs in JavaScript, featuring Adjacency Lists, BFS/DFS traversals, Kahn's Topological Sorting, Dijkstra's Shortest Path, and cycle detection in dependency graphs.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [05_stacks-and-queues.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/05_stacks-and-queues.md), [08_heaps-and-priority-queues.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/08_heaps-and-priority-queues.md) |
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

In software engineering, relationships between entities rarely conform to strict linear or hierarchical models. Real-world systems are networks:
- **Build Systems & Module Bundlers**: Webpack, Vite, and Turbopack model ES modules and dependencies as a Directed Acyclic Graph (DAG) to determine compilation order.
- **Social Networks**: Users and friendships modeled as bidirectional graph connections.
- **Microservice Architectures**: Service-to-service RPC dependencies, routing networks, and circuit-breaker telemetry.

A **Graph** $G = (V, E)$ consists of a set of **Vertices (Nodes)** $V$ connected by **Edges (Links)** $E$. Graph algorithms provide deterministic mathematical solutions for finding shortest paths, resolving dependency orders, detecting deadlocks/cycles, and partitioning connected networks.

---

## 2. Core Concepts

### Graph Classifications
- **Undirected Graph**: Edges are bidirectional ($A \leftrightarrow B$).
- **Directed Graph (Digraph)**: Edges have direction ($A \to B$).
- **Weighted Graph**: Edges carry numerical values (distance, latency, bandwidth cost).
- **Directed Acyclic Graph (DAG)**: A directed graph with **no cycles**, essential for task scheduling and package managers.

```
Undirected Graph:            Directed Acyclic Graph (DAG):
   [ A ] ────── [ B ]               [ Task A ] ──► [ Task B ]
     │            │                    │              │
     │            │                    ▼              ▼
   [ C ] ────── [ D ]               [ Task C ] ──► [ Task D ]
```

### Representation: Adjacency List vs. Adjacency Matrix
1. **Adjacency List (`Map<Vertex, Array<Edge>>`)**:
   - Stores only existing edges.
   - **Space Complexity**: $O(V + E)$ (optimal for sparse graphs).
   - Finding neighbors of vertex $U$: $O(\text{degree}(U))$.
2. **Adjacency Matrix (`V x V` 2D array)**:
   - Stores all potential connections.
   - **Space Complexity**: $O(V^2)$ (wastes memory on sparse graphs; efficient for dense graphs).
   - Edge lookup between $U$ and $W$: $O(1)$.

---

## 3. How It Works Internally

### Topological Sorting (Kahn's In-Degree Algorithm)
1. Calculate the **in-degree** (number of incoming directed edges) for every vertex.
2. Enqueue all vertices with `in-degree === 0` (nodes with zero pending dependencies).
3. Dequeue a vertex, append it to the topological order, and decrement the in-degree of all its neighbors.
4. If a neighbor's in-degree drops to 0, enqueue it.
5. If the resulting order length equals $V$, a valid build order exists; otherwise, a **cyclic dependency (deadlock)** exists.

```
Kahn's Algorithm Progression:
[ A (in: 0) ] ──► [ B (in: 1) ] ──► [ C (in: 2) ]
                      ▲                   │
                      └───────────────────┘ (Cycle: B <-> C)
Initial Queue: [ A ]
Process A: Decrement B (in becomes 0). Enqueue B.
Process B: Decrement C (in becomes 1). Queue becomes empty!
Order length (2) !== Total vertices (3) ──► Cycle Detected!
```

---

## 4. Syntax & API Reference

### Production-Grade Graph Class with BFS, DFS, and Dijkstra
```javascript
import { PriorityQueue } from './08_heaps-and-priority-queues.js';

export class Graph {
  constructor(isDirected = false) {
    this.isDirected = isDirected;
    this.adjList = new Map(); // Vertex -> Array of { node, weight }
  }

  addVertex(v) {
    if (!this.adjList.has(v)) {
      this.adjList.set(v, []);
    }
  }

  addEdge(u, v, weight = 1) {
    this.addVertex(u);
    this.addVertex(v);
    this.adjList.get(u).push({ node: v, weight });
    if (!this.isDirected) {
      this.adjList.get(v).push({ node: u, weight });
    }
  }

  getNeighbors(v) {
    return this.adjList.get(v) || [];
  }

  /**
   * Dijkstra's Single-Source Shortest Path Algorithm
   * Time: O((V + E) log V) | Space: O(V)
   */
  dijkstra(startNode) {
    const distances = new Map();
    const previous = new Map();
    // Min-heap ordered by accumulated distance: [node, distance]
    const pq = new PriorityQueue((a, b) => a[1] - b[1]);

    for (const v of this.adjList.keys()) {
      distances.set(v, Infinity);
    }
    distances.set(startNode, 0);
    pq.push([startNode, 0]);

    while (!pq.isEmpty()) {
      const [current, currentDist] = pq.pop();

      // If popped distance is greater than known shortest, skip stale entry
      if (currentDist > distances.get(current)) continue;

      for (const { node: neighbor, weight } of this.getNeighbors(current)) {
        const newDist = currentDist + weight;

        if (newDist < distances.get(neighbor)) {
          distances.set(neighbor, newDist);
          previous.set(neighbor, current);
          pq.push([neighbor, newDist]);
        }
      }
    }

    return { distances, previous };
  }
}
```

---

## 5. Practical Examples

### Example 1: Number of Islands (2D Grid Graph Traversal)
```javascript
export function numIslands(grid) {
  if (!grid || grid.length === 0) return 0;

  const rows = grid.length;
  const cols = grid[0].length;
  let islandCount = 0;

  function dfs(r, c) {
    // Boundary check & water check
    if (r < 0 || c < 0 || r >= rows || c >= cols || grid[r][c] === '0') {
      return;
    }

    // Mark visited by sinking the island cell to '0'
    grid[r][c] = '0';

    // Explore 4-directional adjacent neighbors
    dfs(r + 1, c);
    dfs(r - 1, c);
    dfs(r, c + 1);
    dfs(r, c - 1);
  }

  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      if (grid[r][c] === '1') {
        islandCount++;
        dfs(r, c);
      }
    }
  }

  return islandCount;
}
```

### Example 2: Course Schedule / Dependency Ordering (Kahn's Algorithm)
```javascript
export function canFinishCourses(numCourses, prerequisites) {
  const inDegree = new Array(numCourses).fill(0);
  const adjList = new Array(numCourses).fill(null).map(() => []);

  // Build directed graph: [dest, src]
  for (const [course, prereq] of prerequisites) {
    adjList[prereq].push(course);
    inDegree[course]++;
  }

  const queue = [];
  for (let i = 0; i < numCourses; i++) {
    if (inDegree[i] === 0) {
      queue.push(i);
    }
  }

  let resolvedCourses = 0;

  while (queue.length > 0) {
    const current = queue.shift();
    resolvedCourses++;

    for (const neighbor of adjList[current]) {
      inDegree[neighbor]--;
      if (inDegree[neighbor] === 0) {
        queue.push(neighbor);
      }
    }
  }

  return resolvedCourses === numCourses;
}
```

### Example 3: Word Ladder Shortest Path (BFS on State Graph)
```javascript
export function ladderLength(beginWord, endWord, wordList) {
  const wordSet = new Set(wordList);
  if (!wordSet.has(endWord)) return 0;

  const queue = [[beginWord, 1]]; // [word, stepCount]
  const visited = new Set([beginWord]);

  while (queue.length > 0) {
    const [currentWord, steps] = queue.shift();

    if (currentWord === endWord) {
      return steps;
    }

    // Generate all valid 1-letter transformations
    for (let i = 0; i < currentWord.length; i++) {
      for (let code = 97; code <= 122; code++) { // 'a' through 'z'
        const nextChar = String.fromCharCode(code);
        if (nextChar === currentWord[i]) continue;

        const candidate = currentWord.slice(0, i) + nextChar + currentWord.slice(i + 1);

        if (wordSet.has(candidate) && !visited.has(candidate)) {
          visited.add(candidate);
          queue.push([candidate, steps + 1]);
        }
      }
    }
  }

  return 0;
}
```

### Example 4: Network Delay Time (Dijkstra's Shortest Path)
```javascript
import { PriorityQueue } from './08_heaps-and-priority-queues.js';

export function networkDelayTime(times, n, k) {
  const adj = new Map();
  for (let i = 1; i <= n; i++) adj.set(i, []);
  for (const [u, v, w] of times) {
    adj.get(u).push({ node: v, weight: w });
  }

  const distances = new Map();
  for (let i = 1; i <= n; i++) distances.set(i, Infinity);
  distances.set(k, 0);

  const pq = new PriorityQueue((a, b) => a[1] - b[1]);
  pq.push([k, 0]);

  while (!pq.isEmpty()) {
    const [u, dist] = pq.pop();

    if (dist > distances.get(u)) continue;

    for (const { node: v, weight: w } of adj.get(u)) {
      if (dist + w < distances.get(v)) {
        distances.set(v, dist + w);
        pq.push([v, dist + w]);
      }
    }
  }

  let maxTime = 0;
  for (let i = 1; i <= n; i++) {
    const d = distances.get(i);
    if (d === Infinity) return -1; // Unreachable node
    if (d > maxTime) maxTime = d;
  }

  return maxTime;
}
```

### Example 5: Clone Graph (Deep Copy with Cycle Handling)
```javascript
class GraphNode {
  constructor(val = 0, neighbors = []) {
    this.val = val;
    this.neighbors = neighbors;
  }
}

export function cloneGraph(node) {
  if (!node) return null;

  const clones = new Map(); // Original -> Cloned
  const queue = [node];
  clones.set(node, new GraphNode(node.val));

  while (queue.length > 0) {
    const current = queue.shift();

    for (const neighbor of current.neighbors) {
      if (!clones.has(neighbor)) {
        clones.set(neighbor, new GraphNode(neighbor.val));
        queue.push(neighbor);
      }
      clones.get(current).neighbors.push(clones.get(neighbor));
    }
  }

  return clones.get(node);
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Infinite Loops from Missing `visited` Sets**:
   Unlike trees, graphs can contain cycles ($A \to B \to A$). Traversal without an explicit `visited = new Set()` causes infinite recursive loops or unbounded queue growth until memory exhausts.
2. **Disconnected Components**:
   Calling BFS or DFS starting from a single node visits only nodes in that connected component. If a graph contains disconnected islands, you must loop over all vertices: `for (const v of vertices) if (!visited.has(v)) traverse(v)`.
3. **Negative Edge Weights in Dijkstra**:
   Dijkstra's algorithm assumes edge weights are non-negative ($w \ge 0$). With negative edge weights, once a node is marked settled, Dijkstra never revisits it, producing incorrect shortest paths. Graphs with negative weights require the **Bellman-Ford algorithm** ($O(V \times E)$).
4. **Adjacency Matrix Memory Waste on Sparse Graphs**:
   Allocating an adjacency matrix for $100,000$ users with an average of 5 friends each allocates a $100,000 \times 100,000$ matrix ($10^{10}$ integers $\approx 40\text{GB}$ RAM). Always use an **Adjacency List** for sparse real-world graphs.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Hazard | Production Fix |
| :--- | :--- | :--- |
| Missing `visited` set in cyclic graphs | Infinite recursion crashing with Stack Overflow or OOM. | Track visited nodes via `new Set()` or state flags. |
| Using `array.shift()` in high-volume BFS | Drops BFS performance to $O(V^2)$ due to array shifting. | Use a fast linked-list Queue. |
| Running Dijkstra on negative edge weights | Produces incorrect distances and infinite loops on negative cycles. | Use Bellman-Ford algorithm if weights can be negative. |
| Assuming a single BFS visits entire graph | Skips disconnected components and isolated vertices. | Iterate across all vertices in an outer loop. |
| Dense matrix allocation for sparse graphs | Triggers massive heap allocation and V8 out-of-memory crash. | Use `Map<Node, Node[]>` Adjacency List. |
| Mutating 2D grid cells without bounds check | Throws `TypeError: Cannot read properties of undefined`. | Check `r >= 0 && r < rows && c >= 0 && c < cols` first. |

---

## 8. Performance Characteristics

| Algorithm | Time Complexity | Space Complexity | Best For |
| :--- | :--- | :--- | :--- |
| **BFS (Adjacency List)** | $O(V + E)$ | $O(V)$ | Shortest path in unweighted graphs |
| **DFS (Adjacency List)** | $O(V + E)$ | $O(V)$ | Cycle detection, topological sort, maze solving |
| **Kahn's Topological Sort** | $O(V + E)$ | $O(V)$ | Compilation order, circular dependency detection |
| **Dijkstra's Algorithm** | $O((V + E) \log V)$ | $O(V)$ | Shortest path in non-negative weighted graphs |
| **Bellman-Ford** | $O(V \times E)$ | $O(V)$ | Shortest path with negative edge weights |

---

## 9. Engine Internals

### V8 Map Key Efficiency in Graphs
- **Primitive vs Object Keys**: In JavaScript, using integer indices (`0, 1, 2...`) as map keys allows V8 to utilize fast array indexing or small-integer hash maps. Using strings or objects incurs hashing overhead.
- **TypedArray Flat Adjacency Lists (CSR)**: In high-performance graphics or gaming engines, graphs are stored using **Compressed Sparse Row (CSR)** format via two contiguous `Int32Array` buffers (one for edge destination offsets, one for targets). This eliminates object pointer dereferencing completely.

---

## 10. Best Practices & Design Guidelines

1. **Choose BFS for Unweighted Shortest Path**: BFS guarantees that the first time a destination node is dequeued, it was reached via the minimal number of edges.
2. **Choose DFS for Exhaustive Path Finding / Backtracking**: Exploring all possible paths or detecting cycles is cleanest using DFS.
3. **Use Kahn's Algorithm for Build DAGs**: Easily detects circular import errors when `result.length !== numVertices`.
4. **Prune Stale Entries in Dijkstra**: In a Min-Heap Dijkstra, multiple distance updates for the same node can sit in the priority queue. Always skip if `currentDist > distances.get(current)`.

---

## 11. Real-World Case Study

### Webpack / Vite Circular Dependency Detector
**Problem**: In large enterprise frontend applications containing 5,000+ JavaScript files, circular imports (`moduleA.js` imports `moduleB.js` which imports `moduleA.js`) frequently cause `undefined` runtime execution errors due to temporal dead zones.
**Solution**:
- The bundler builds a directed dependency graph $G = (V, E)$ where vertices are file paths and edges represent `import` statements.
- Executes Kahn's algorithm or Tarjan's strongly connected components algorithm during the dependency resolution phase.
- If a cycle is detected, the bundler traces the exact cyclic path ($A \to B \to C \to A$) and aborts compilation with an actionable developer warning before shipping broken code to production.

---

## 12. Interview Questions

<details>
<summary>1. When should you represent a graph as an Adjacency List versus an Adjacency Matrix?</summary>

- **Adjacency List**: Best for **sparse graphs** (where $E \ll V^2$, typical in real-world systems like road networks, web links, social networks). Space is $O(V + E)$, and iterating over neighbors takes $O(\text{degree})$.
- **Adjacency Matrix**: Best for **dense graphs** (where $E \approx V^2$) or when you need instant $O(1)$ verification of edge existence between two arbitrary vertices (`matrix[u][v] === 1`). Space is $O(V^2)$.
</details>

<details>
<summary>2. Why does Breadth-First Search (BFS) guarantee the shortest path in unweighted graphs?</summary>

BFS explores nodes in order of their distance from the source: distance 1, then distance 2, then distance 3, and so on. Because edges have uniform weight (1), the first time BFS encounters a node, it is mathematically impossible for any unexplored path to reach that node in fewer steps. Thus, the first discovery of any node is guaranteed to be its shortest path.
</details>

<details>
<summary>3. How does Kahn's algorithm detect cycles in a directed graph?</summary>

Kahn's algorithm maintains an in-degree count for each vertex and processes only vertices with an in-degree of 0. When a node is processed, it is removed and the in-degrees of its outgoing neighbors are decremented. If a cycle exists, the nodes participating in the cycle can never have their in-degrees reduced to 0 because each node in the cycle is waiting on another node in the cycle. Consequently, they are never enqueued, and the total count of processed nodes will be strictly less than the total vertex count $V$.
</details>

<details>
<summary>4. Why does Dijkstra's algorithm fail on graphs with negative edge weights?</summary>

Dijkstra's algorithm is greedy: once a vertex is extracted from the Priority Queue, its shortest distance is finalized and assumed optimal; it is never relaxed again. If negative edge weights exist, a longer path with positive edges could later take a negative-weight edge and produce a lower total path distance after the destination node was already marked visited. Graphs with negative edge weights require the **Bellman-Ford algorithm**.
</details>

<details>
<summary>5. What is the difference between a Tree and a Graph?</summary>

A **Tree** is a special restricted form of a Graph:
1. It is **connected** and **acyclic** (contains zero cycles).
2. It has exactly $V - 1$ edges for $V$ vertices.
3. Exactly one unique path exists between any two vertices.
4. It has a designated root node where every child has exactly one parent.
A **Graph** can be cyclic, disconnected, directed or undirected, and nodes can have multiple incoming/outgoing edges without a root.
</details>

<details>
<summary>6. What is the time complexity of Dijkstra's algorithm implemented with a Min-Heap?</summary>

**$O((V + E) \log V)$**.
- Every vertex is inserted into and popped from the priority queue at most once ($V \log V$).
- For every edge, we potentially relax a neighbor and push a new distance entry into the priority queue ($E \log V$).
Summing these gives $O((V + E) \log V)$, which is vastly superior to the naive $O(V^2)$ array-based implementation on sparse graphs.
</details>

<details>
<summary>7. How can you detect cycles in an undirected graph using DFS?</summary>

Run a recursive DFS tracking `visited` nodes and passing the `parent` node that led to the current node. For each neighbor:
- If the neighbor is unvisited, recurse with `dfs(neighbor, current)`.
- If the neighbor is already visited AND is **not equal to the parent**, a cycle exists (we reached an already visited node through an alternate path).
</details>

<details>
<summary>8. What is a Bipartite Graph and how do you test for it?</summary>

A graph is **Bipartite** if its vertices can be partitioned into two independent sets $U$ and $W$ such that every edge connects a vertex in $U$ to a vertex in $W$ (no edge connects vertices within the same set). We test for it using **2-Coloring (BFS or DFS)**: color the start node Red (0), and color all its neighbors Blue (1). If we ever encounter an adjacent neighbor that already possesses the same color as the current node, the graph is not bipartite. A graph is bipartite if and only if it contains **no odd-length cycles**.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Find if Path Exists in Graph
**Specification**: Given an undirected graph represented by $n$ vertices and a 2D array `edges`, write a function that returns `true` if there is a valid path from `source` to `destination`, or `false` otherwise.

<details>
<summary>Solution</summary>

```javascript
export function validPath(n, edges, source, destination) {
  if (source === destination) return true;

  const adj = new Map();
  for (let i = 0; i < n; i++) adj.set(i, []);
  for (const [u, v] of edges) {
    adj.get(u).push(v);
    adj.get(v).push(u);
  }

  const visited = new Set([source]);
  const queue = [source];

  while (queue.length > 0) {
    const current = queue.shift();
    if (current === destination) return true;

    for (const neighbor of adj.get(current)) {
      if (!visited.has(neighbor)) {
        visited.add(neighbor);
        queue.push(neighbor);
      }
    }
  }

  return false;
}
```
</details>

### Exercise 2: Redundant Connection (Cycle in Undirected Graph)
**Specification**: In an undirected graph that started as a tree with $n$ nodes labeled 1 to $n$, one additional edge was added. Find and return an edge that can be removed so that the resulting graph is a tree.

<details>
<summary>Solution</summary>

```javascript
export function findRedundantConnection(edges) {
  const parent = new Array(edges.length + 1).fill(0).map((_, i) => i);

  function find(i) {
    if (parent[i] === i) return i;
    return (parent[i] = find(parent[i])); // Path compression
  }

  function union(i, j) {
    const rootI = find(i);
    const rootJ = find(j);
    if (rootI === rootJ) return false; // Already connected: redundant edge found!
    parent[rootI] = rootJ;
    return true;
  }

  for (const [u, v] of edges) {
    if (!union(u, v)) {
      return [u, v];
    }
  }

  return [];
}
```
</details>
