# Minimum Spanning Tree (MST)

> Graph algorithms for finding minimum weight cycle-free spanning subgraphs: Kruskal's algorithm with Disjoint Set Union and Prim's algorithm with priority queues.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [10_heaps-and-priority-queues.md](10_heaps-and-priority-queues.md), [11_graphs-bfs-dfs.md](11_graphs-bfs-dfs.md) |
| **Time to Read** | 30 minutes |
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

A **Spanning Tree** of a connected, undirected graph $G = (V, E)$ is an acyclic subgraph that connects all $V$ vertices using exactly $V - 1$ edges.

A **Minimum Spanning Tree (MST)** is a spanning tree whose sum of edge weights is strictly minimal among all possible spanning trees:
$$\min \sum_{e \in T} w(e)$$

### Real-World Applications
- **Telecommunications & Power Grids**: Laying fiber-optic cable or electrical wires connecting cities with minimum total cable length.
- **Circuit Design (VLSI)**: Connecting pins on a microchip with minimum total wire resistance.
- **Cluster Analysis**: Single-linkage hierarchical clustering constructs an MST and cuts the highest-weight edges.
- **Approximation Algorithms**: 2-approximation for the Metric Traveling Salesperson Problem (TSP).

---

## 2. Core Concepts

### 1. The Cut Property
Let a **cut** $(S, V \setminus S)$ be any partition of the graph's vertices into two disjoint non-empty sets. An edge **crosses the cut** if one endpoint lies in $S$ and the other in $V \setminus S$.
> **The Cut Property Theorem**: If the edge with the strictly minimum weight crossing a cut is unique, that edge **must belong to every Minimum Spanning Tree** of the graph.

### 2. Kruskal's Algorithm (Edge-Centric Greedy)
1. Sort all $E$ edges in ascending order of weight.
2. Initialize an empty spanning tree $T$ and a Disjoint Set Union (DSU) structure where each vertex is its own component.
3. Iterate through sorted edges:
   - If endpoints $u$ and $v$ belong to different sets, add edge $(u, v)$ to $T$ and union their sets.
   - If endpoints are already in the same set, discard the edge to prevent creating a cycle.
4. Stop when $T$ contains $V - 1$ edges.

### 3. Prim's Algorithm (Vertex-Centric Greedy)
1. Start with an arbitrary root vertex in visited set $S$.
2. Maintain a Min-Priority Queue of all edges crossing the cut between visited set $S$ and unvisited set $V \setminus S$.
3. Extract the minimum edge $(u, v)$ where $v \notin S$.
4. Add $v$ to $S$ and insert all edges from $v$ to unvisited neighbors into the priority queue.
5. Repeat until all $V$ vertices are added to $S$.

---

## 3. How It Works Internally

### Execution Walkthrough: Kruskal vs Prim

```
Graph:
    (1) --- 1 --- (2)
     | \           |
     4   2         3
     |     \       |
    (3) --- 5 --- (4)

Kruskal's steps (sorted edges):
- Pick (1, 2) weight 1 -> Accepted
- Pick (1, 4) weight 2 -> Accepted
- Pick (2, 4) weight 3 -> Rejected (Cycle: 1-2-4)
- Pick (1, 3) weight 4 -> Accepted
Total edges = 3 = V - 1. Finished! MST weight = 1 + 2 + 4 = 7.
```

```
HotSpot JVM Data Structures:
- Kruskal: int[][] edges sorted using Dual-Pivot Quicksort (Arrays.sort)
  DSU: int[] parent, int[] rank arrays (cache-friendly contiguous memory)
- Prim: PriorityQueue<Edge> backed by Object[] min-heap
  boolean[] visited for O(1) membership checks
```

---

## 4. Syntax & Usage

### Clean Kruskal's Algorithm Implementation

```java
import java.util.*;

public class KruskalMST {
    public record Edge(int u, int v, int weight) implements Comparable<Edge> {
        @Override
        public int compareTo(Edge other) {
            return Integer.compare(this.weight, other.weight);
        }
    }

    static class DSU {
        private final int[] parent;
        private final int[] rank;

        public DSU(int n) {
            parent = new int[n];
            rank = new int[n];
            for (int i = 0; i < n; i++) parent[i] = i;
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
            if (rootI == rootJ) return false;

            if (rank[rootI] < rank[rootJ]) {
                parent[rootI] = rootJ;
            } else if (rank[rootI] > rank[rootJ]) {
                parent[rootJ] = rootI;
            } else {
                parent[rootJ] = rootI;
                rank[rootI]++;
            }
            return true;
        }
    }

    public static int computeMSTWeight(int n, List<Edge> edges) {
        Collections.sort(edges);
        DSU dsu = new DSU(n);

        int totalWeight = 0;
        int edgesAdded = 0;

        for (Edge e : edges) {
            if (dsu.union(e.u(), e.v())) {
                totalWeight += e.weight();
                if (++edgesAdded == n - 1) break;
            }
        }

        if (edgesAdded != n - 1) {
            throw new IllegalArgumentException("Graph is not connected; MST does not exist.");
        }

        return totalWeight;
    }
}
```

---

## 5. Practical Examples

### Example 1: Min Cost to Connect All Points (LeetCode 1584, Prim's Algorithm)

```java
import java.util.Arrays;

public class MinCostConnectPoints {
    public int minCostConnectPoints(int[][] points) {
        int n = points.length;
        int[] minDist = new int[n];
        Arrays.fill(minDist, Integer.MAX_VALUE);
        minDist[0] = 0;

        boolean[] inMST = new boolean[n];
        int totalCost = 0;

        // O(V^2) Prim's without priority queue (optimal for dense complete graph)
        for (int step = 0; step < n; step++) {
            int curr = -1;
            for (int i = 0; i < n; i++) {
                if (!inMST[i] && (curr == -1 || minDist[i] < minDist[curr])) {
                    curr = i;
                }
            }

            inMST[curr] = true;
            totalCost += minDist[curr];

            for (int next = 0; next < n; next++) {
                if (!inMST[next]) {
                    int dist = Math.abs(points[curr][0] - points[next][0])
                             + Math.abs(points[curr][1] - points[next][1]);
                    if (dist < minDist[next]) {
                        minDist[next] = dist;
                    }
                }
            }
        }

        return totalCost;
    }
}
```

### Example 2: Connecting Cities With Minimum Cost (LeetCode 1135, Kruskal)

```java
import java.util.*;

public class ConnectingCities {
    public int minimumCost(int n, int[][] connections) {
        // Sort connections by cost ascending
        Arrays.sort(connections, Comparator.comparingInt(a -> a[2]));

        int[] parent = new int[n + 1];
        for (int i = 1; i <= n; i++) parent[i] = i;

        int totalCost = 0;
        int edgesAdded = 0;

        for (int[] edge : connections) {
            int rootU = find(parent, edge[0]);
            int rootV = find(parent, edge[1]);

            if (rootU != rootV) {
                parent[rootU] = rootV;
                totalCost += edge[2];
                if (++edgesAdded == n - 1) return totalCost;
            }
        }

        return edgesAdded == n - 1 ? totalCost : -1;
    }

    private int find(int[] parent, int i) {
        if (parent[i] != i) parent[i] = find(parent, parent[i]);
        return parent[i];
    }
}
```

### Example 3: Optimize Water Distribution in a Village (LeetCode 1168, Virtual Node)

```java
import java.util.*;

public class OptimizeWaterDistribution {
    public int minCostToSupplyWater(int n, int[] wells, int[][] pipes) {
        List<int[]> edges = new ArrayList<>(pipes.length + n);

        // Virtual node 0 represents the water reservoir / aquifer
        for (int i = 0; i < n; i++) {
            edges.add(new int[]{0, i + 1, wells[i]});
        }
        for (int[] p : pipes) {
            edges.add(new int[]{p[0], p[1], p[2]});
        }

        edges.sort(Comparator.comparingInt(a -> a[2]));

        int[] parent = new int[n + 1];
        for (int i = 0; i <= n; i++) parent[i] = i;

        int totalCost = 0;
        int components = n; // Need to connect n houses to virtual node 0

        for (int[] e : edges) {
            int rootU = find(parent, e[0]);
            int rootV = find(parent, e[1]);
            if (rootU != rootV) {
                parent[rootU] = rootV;
                totalCost += e[2];
                if (--components == 0) break;
            }
        }

        return totalCost;
    }

    private int find(int[] parent, int i) {
        if (parent[i] != i) parent[i] = find(parent, parent[i]);
        return parent[i];
    }
}
```

### Example 4: Critical and Pseudo-Critical Edges in MST (LeetCode 1489)

```java
import java.util.*;

public class CriticalEdgesMST {
    static class UnionFind {
        int[] parent;
        int count;
        public UnionFind(int n) {
            parent = new int[n];
            for (int i = 0; i < n; i++) parent[i] = i;
            count = n;
        }
        public int find(int i) {
            if (parent[i] != i) parent[i] = find(parent[i]);
            return parent[i];
        }
        public boolean union(int i, int j) {
            int rI = find(i), rJ = find(j);
            if (rI == rJ) return false;
            parent[rI] = rJ;
            count--;
            return true;
        }
    }

    public List<List<Integer>> findCriticalAndPseudoCriticalEdges(int n, int[][] edges) {
        int m = edges.length;
        int[][] indexedEdges = new int[m][4];
        for (int i = 0; i < m; i++) {
            indexedEdges[i][0] = edges[i][0];
            indexedEdges[i][1] = edges[i][1];
            indexedEdges[i][2] = edges[i][2];
            indexedEdges[i][3] = i; // Original index
        }

        Arrays.sort(indexedEdges, Comparator.comparingInt(a -> a[2]));
        int baseWeight = buildMST(n, indexedEdges, -1, -1);

        List<Integer> critical = new ArrayList<>();
        List<Integer> pseudoCritical = new ArrayList<>();

        for (int i = 0; i < m; i++) {
            // Check if critical by excluding edge i
            if (buildMST(n, indexedEdges, i, -1) > baseWeight) {
                critical.add(indexedEdges[i][3]);
            } else if (buildMST(n, indexedEdges, -1, i) == baseWeight) {
                // Check if pseudo-critical by force-including edge i
                pseudoCritical.add(indexedEdges[i][3]);
            }
        }

        return List.of(critical, pseudoCritical);
    }

    private int buildMST(int n, int[][] edges, int exclude, int forceInclude) {
        UnionFind uf = new UnionFind(n);
        int weight = 0;

        if (forceInclude != -1) {
            uf.union(edges[forceInclude][0], edges[forceInclude][1]);
            weight += edges[forceInclude][2];
        }

        for (int i = 0; i < edges.length; i++) {
            if (i == exclude) continue;
            if (uf.union(edges[i][0], edges[i][1])) {
                weight += edges[i][2];
            }
        }

        return uf.count == 1 ? weight : Integer.MAX_VALUE;
    }
}
```

### Example 5: Spanning Forest for Disconnected Graphs

```java
import java.util.*;

public class MinimumSpanningForest {
    public static List<int[]> getForest(int n, int[][] edges) {
        Arrays.sort(edges, Comparator.comparingInt(a -> a[2]));
        int[] parent = new int[n];
        for (int i = 0; i < n; i++) parent[i] = i;

        List<int[]> forestEdges = new ArrayList<>();
        for (int[] e : edges) {
            int rootU = find(parent, e[0]);
            int rootV = find(parent, e[1]);
            if (rootU != rootV) {
                parent[rootU] = rootV;
                forestEdges.add(e);
            }
        }
        return forestEdges;
    }

    private static int find(int[] p, int i) {
        if (p[i] != i) p[i] = find(p, p[i]);
        return p[i];
    }
}
```

---

## 6. Best Practices

1. **Choose Between Kruskal and Prim Based on Graph Density**:
   - For **Sparse Graphs** ($E \ll V^2$): Kruskal's algorithm ($O(E \log E)$) or Prim's with Binary Heap ($O(E \log V)$).
   - For **Dense Complete Graphs** ($E \approx V^2$): Prim's algorithm using a flat array distance table runs in strictly $O(V^2)$, beating heap-based algorithms by eliminating $O(\log V)$ overhead.
2. **Virtual Node Idiom**: When vertices can independently choose internal resources (e.g., digging a well vs laying pipe), connect all nodes to a virtual super-node 0 with edge weights equal to internal cost. A standard MST then solves the joint optimization.
3. **Verify Connectedness**: Always verify that exactly $V - 1$ edges were selected. If fewer, the graph was disconnected.
4. **Use Path Compression & Union by Rank**: Guarantees near constant-time $O(\alpha(V))$ per edge check in Kruskal's.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Attempting MST on directed graphs | Cut property does not apply to directed graphs | Use Chu-Liu/Edmonds algorithm for optimum branching. |
| Forgetting to check connectedness | Returns partial weight when graph has isolated nodes | Check if `edgesAdded == n - 1` or `dsu.count == 1`. |
| Using heap-based Prim on complete graphs | Overhead of $O(V^2 \log V)$ priority queue operations | Use adjacency matrix + $O(V^2)$ scan. |
| Inverting sorting order | Finds Maximum Spanning Tree instead of Minimum | Sort edge weights in ascending order. |

---

## 8. Performance & Security Notes

- **Time Complexity**:
  - Kruskal's: $O(E \log E) = O(E \log V)$ for edge sorting, plus $O(E \alpha(V))$ DSU operations.
  - Prim's with Binary Heap: $O((V + E) \log V)$.
  - Prim's with Flat Array: $O(V^2)$.
- **Space Complexity**:
  - Kruskal's: $O(E + V)$ for edge list and DSU arrays.
  - Prim's: $O(V + E)$ for adjacency list and priority queue.
- **Cache Locality**: Kruskal's edge array sorting and flat DSU arrays exhibit exceptional spatial locality.

---

## 9. Comparison / When to Use What

| Criteria | Kruskal's Algorithm | Prim's (Binary Heap) | Prim's (Flat Array) |
|---|---|---|---|
| **Graph Density** | Sparse ($E \ll V^2$) | Sparse / Moderate | Dense ($E \approx V^2$) |
| **Complexity** | $O(E \log E)$ | $O(E \log V)$ | $O(V^2)$ |
| **Implementation** | Trivial with DSU | Medium (heap + graph) | Very simple ($O(V^2)$ loops) |
| **Edge Representation** | Edge List | Adjacency List | Adjacency Matrix |

---

## 10. Interview Questions

<details>
<summary>1. Is the Minimum Spanning Tree always unique?</summary>

The MST is guaranteed to be unique if **all edge weights in the graph are distinct**. If two or more edges have identical weights, multiple distinct spanning trees may share the same minimum total weight.
</details>

<details>
<summary>2. What is the difference between a Shortest Path Tree and a Minimum Spanning Tree?</summary>

An MST minimizes the **sum of all edge weights** across the entire tree connecting all vertices. A Shortest Path Tree (SPT, from Dijkstra) minimizes the **distance from a single designated source root** to every other vertex. An SPT edge weight sum can be significantly larger than an MST.
</details>

<details>
<summary>3. Can edge weights in an MST be negative?</summary>

Yes! Both Kruskal's and Prim's algorithms work correctly with negative edge weights without any modifications, because the Cut Property holds regardless of edge signs (unlike Dijkstra's shortest path).
</details>

<details>
<summary>4. What happens if you add an edge to an existing MST?</summary>

Adding any edge to an MST creates exactly one cycle. To restore the MST, find the cycle and delete the edge with the maximum weight on that cycle.
</details>

<details>
<summary>5. How do you find the Maximum Spanning Tree?</summary>

Either sort the edges in descending order (for Kruskal's) or multiply all edge weights by $-1$ and run standard MST algorithms.
</details>

<details>
<summary>6. What is a "Critical Edge" in an MST?</summary>

An edge $e$ is critical if deleting $e$ from the graph strictly increases the weight of the Minimum Spanning Tree (or makes the graph disconnected).
</details>

<details>
<summary>7. What is a "Pseudo-Critical Edge" in an MST?</summary>

An edge $e$ is pseudo-critical if it can appear in at least one MST, but not in all MSTs. That is, forcing $e$ into the tree yields the same minimum total weight as the optimal MST.
</details>

<details>
<summary>8. How does Borůvka's algorithm work and why is it useful?</summary>

Borůvka's algorithm repeatedly connects each component to its nearest neighbor in parallel phases. Each phase halves the number of components, terminating in $O(\log V)$ phases and $O(E \log V)$ total time. It is the basis for distributed and GPU-parallel MST algorithms.
</details>

---

## 11. Practice Exercises

### Exercise 1: Min Cost to Reach Destination in Time (LeetCode 1928)
Find minimum fee to travel within max time, balancing edge travel times and city passing fees.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class MinCostInTime {
    public int minCost(int maxTime, int[][] edges, int[] passingFees) {
        int n = passingFees.length;
        List<List<int[]>> adj = new ArrayList<>(n);
        for (int i = 0; i < n; i++) adj.add(new ArrayList<>());
        for (int[] e : edges) {
            adj.get(e[0]).add(new int[]{e[1], e[2]});
            adj.get(e[1]).add(new int[]{e[0], e[2]});
        }

        // minTime[node] stores min time encountered to reach this node
        int[] minTime = new int[n];
        Arrays.fill(minTime, Integer.MAX_VALUE);

        // [node, cost, time]
        PriorityQueue<int[]> pq = new PriorityQueue<>(Comparator.comparingInt(a -> a[1]));
        pq.offer(new int[]{0, passingFees[0], 0});
        minTime[0] = 0;

        while (!pq.isEmpty()) {
            int[] curr = pq.poll();
            int u = curr[0], cost = curr[1], time = curr[2];

            if (u == n - 1) return cost;

            for (int[] next : adj.get(u)) {
                int v = next[0], travelTime = next[1];
                int nextTime = time + travelTime;
                int nextCost = cost + passingFees[v];

                if (nextTime <= maxTime && nextTime < minTime[v]) {
                    minTime[v] = nextTime;
                    pq.offer(new int[]{v, nextCost, nextTime});
                }
            }
        }

        return -1;
    }
}
```
</details>

### Exercise 2: Checking Existence of Edge Length Limited Paths (LeetCode 1697, Offline Queries + Kruskal)

<details>
<summary>Solution</summary>

```java
import java.util.Arrays;
import java.util.Comparator;

public class DistanceLimitedPathsExist {
    public boolean[] distanceLimitedPathsExist(int n, int[][] edgeList, int[][] queries) {
        int m = edgeList.length, k = queries.length;
        int[][] sortedQueries = new int[k][4];
        for (int i = 0; i < k; i++) {
            sortedQueries[i] = new int[]{queries[i][0], queries[i][1], queries[i][2], i};
        }

        Arrays.sort(edgeList, Comparator.comparingInt(a -> a[2]));
        Arrays.sort(sortedQueries, Comparator.comparingInt(a -> a[2]));

        int[] parent = new int[n];
        for (int i = 0; i < n; i++) parent[i] = i;

        boolean[] result = new boolean[k];
        int edgeIdx = 0;

        for (int[] q : sortedQueries) {
            int u = q[0], v = q[1], limit = q[2], origIdx = q[3];

            while (edgeIdx < m && edgeList[edgeIdx][2] < limit) {
                union(parent, edgeList[edgeIdx][0], edgeList[edgeIdx][1]);
                edgeIdx++;
            }

            result[origIdx] = find(parent, u) == find(parent, v);
        }

        return result;
    }

    private int find(int[] p, int i) {
        if (p[i] != i) p[i] = find(p, p[i]);
        return p[i];
    }

    private void union(int[] p, int i, int j) {
        int rI = find(p, i), rJ = find(p, j);
        if (rI != rJ) p[rI] = rJ;
    }
}
```
</details>

### Exercise 3: Remove Max Number of Edges to Keep Graph Fully Traversable (LeetCode 1579)

<details>
<summary>Solution</summary>

```java
public class MaxNumEdgesToRemove {
    static class UF {
        int[] p;
        int count;
        public UF(int n) {
            p = new int[n + 1];
            for (int i = 0; i <= n; i++) p[i] = i;
            count = n;
        }
        public int find(int i) {
            if (p[i] != i) p[i] = find(p, p[i]);
            return p[i];
        }
        public boolean union(int i, int j) {
            int rI = find(i), rJ = find(j);
            if (rI == rJ) return false;
            p[rI] = rJ;
            count--;
            return true;
        }
    }

    public int maxNumEdgesToRemove(int n, int[][] edges) {
        UF alice = new UF(n);
        UF bob = new UF(n);
        int usedEdges = 0;

        // Process Type 3 edges first (shared by both)
        for (int[] e : edges) {
            if (e[0] == 3) {
                boolean uA = alice.union(e[1], e[2]);
                boolean uB = bob.union(e[1], e[2]);
                if (uA || uB) usedEdges++;
            }
        }

        // Process Type 1 (Alice only)
        for (int[] e : edges) {
            if (e[0] == 1 && alice.union(e[1], e[2])) {
                usedEdges++;
            }
        }

        // Process Type 2 (Bob only)
        for (int[] e : edges) {
            if (e[0] == 2 && bob.union(e[1], e[2])) {
                usedEdges++;
            }
        }

        if (alice.count == 1 && bob.count == 1) {
            return edges.length - usedEdges;
        }
        return -1;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Spanning Tree Definition**: $V$ vertices connected by exactly $V - 1$ edges with no cycles.
- **Kruskal's**: Greedy edge sorting + DSU. $O(E \log E)$. Optimal for sparse graphs.
- **Prim's**: Greedy frontier expansion with Min-Heap ($O(E \log V)$) or array ($O(V^2)$ for dense).
- **Cut Property**: Minimum weight edge crossing any cut belongs to the MST.
- **Virtual Node Pattern**: Solve multi-source resource construction by connecting nodes to a virtual vertex 0.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 23: Minimum Spanning Trees](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Kruskal, J. B. (1956). On the shortest spanning subtree of a graph](https://www.ams.org/journals/proc/1956-007-01/S0002-9939-1956-0078686-7/)
- [Prim, R. C. (1957). Shortest connection networks and some generalizations](https://ieeexplore.ieee.org/document/6770284)
- [Baeldung: Prim's Algorithm vs Kruskal's Algorithm](https://www.baeldung.com/cs/prim-vs-kruskal)
