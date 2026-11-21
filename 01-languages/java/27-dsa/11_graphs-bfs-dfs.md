# Graph Traversal: BFS and DFS

> Core graph representations and systematic search algorithms for shortest paths, connectivity, bipartiteness, and cycle detection.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [02_linked-lists.md](02_linked-lists.md), [03_stacks-and-queues.md](03_stacks-and-queues.md), [05_recursion-and-backtracking.md](05_recursion-and-backtracking.md) |
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

A **Graph** $G = (V, E)$ consists of a set of vertices (nodes) $V$ connected by a set of edges $E$. Graphs model network routing, dependency graphs, social networks, state machines, and spatial maps.

Graph traversal refers to visiting every vertex reachable from a starting vertex without getting stuck in infinite loops.
- **Breadth-First Search (BFS)**: Explores radially outward, level-by-level. It guarantees finding the **shortest path in unweighted graphs**.
- **Depth-First Search (DFS)**: Plunges deep down a branch until hitting a dead end or visited node, then backtracks. It is fundamental for topological sorting, cycle detection, connected components, and pathfinding.

---

## 2. Core Concepts

### Graph Representations
1. **Adjacency List**:
   - `List<List<Integer>>` or `Map<Integer, List<Integer>>`.
   - Space: $O(V + E)$.
   - Best for sparse graphs ($E \ll V^2$).
2. **Adjacency Matrix**:
   - `boolean[][]` or `int[][]`.
   - Space: $O(V^2)$.
   - Edge lookup $u \to v$ in $O(1)$. Best for dense graphs ($E \approx V^2$).
3. **Edge List**:
   - Array or list of edge records `record Edge(int u, int v, int weight)`.
   - Space: $O(E)$. Standard for Kruskal's MST and Bellman-Ford.

### BFS vs DFS Traversal Invariants
- **BFS**: Utilizes a FIFO Queue (`ArrayDeque`). Nodes at distance $d$ are strictly processed before any nodes at distance $d+1$.
- **DFS**: Utilizes a LIFO Stack (either the call stack via recursion or an explicit `ArrayDeque`).

---

## 3. How It Works Internally

### Node States During DFS (3-Coloring Cycle Detection)
In directed graphs, a simple `boolean[] visited` is insufficient to differentiate between cross-edges and true back-edges (cycles). HotSpot implementations use three states:
- **WHITE (0 / Unvisited)**: Node has not been touched.
- **GRAY (1 / Visiting / On Current Path)**: Node is currently on the recursion call stack. Encountering a GRAY node confirms a **cycle**.
- **BLACK (2 / Visited / Completed)**: Node and all its descendants have been fully processed.

```
DFS Recursion Call Stack:
+-------------------+
| dfs(v=3) [GRAY]   | --> Sees neighbor 1 [GRAY] => CYCLE FOUND!
+-------------------+
| dfs(v=2) [GRAY]   |
+-------------------+
| dfs(v=1) [GRAY]   |
+-------------------+
| main()            |
+-------------------+
```

---

## 4. Syntax & Usage

### Standard Graph Representation & Traversal

```java
import java.util.*;

public class GraphTraversals {
    private final int v;
    private final List<List<Integer>> adj;

    public GraphTraversals(int v) {
        this.v = v;
        this.adj = new ArrayList<>(v);
        for (int i = 0; i < v; i++) {
            adj.add(new ArrayList<>());
        }
    }

    public void addEdge(int u, int v, boolean directed) {
        adj.get(u).add(v);
        if (!directed) adj.get(v).add(u);
    }

    // BFS Traversal
    public void bfs(int start) {
        boolean[] visited = new boolean[v];
        Queue<Integer> queue = new ArrayDeque<>();

        visited[start] = true; // Crucial: mark visited on enqueue!
        queue.offer(start);

        while (!queue.isEmpty()) {
            int node = queue.poll();
            System.out.print(node + " ");

            for (int neighbor : adj.get(node)) {
                if (!visited[neighbor]) {
                    visited[neighbor] = true;
                    queue.offer(neighbor);
                }
            }
        }
        System.out.println();
    }

    // DFS Traversal (Recursive)
    public void dfs(int start) {
        boolean[] visited = new boolean[v];
        dfsHelper(start, visited);
        System.out.println();
    }

    private void dfsHelper(int node, boolean[] visited) {
        visited[node] = true;
        System.out.print(node + " ");

        for (int neighbor : adj.get(node)) {
            if (!visited[neighbor]) {
                dfsHelper(neighbor, visited);
            }
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Clone Graph (LeetCode 133)
Deep copy an undirected graph where each node contains an integer value and a list of neighbors.

```java
import java.util.*;

public class CloneGraphSolution {
    static class Node {
        public int val;
        public List<Node> neighbors;
        public Node(int val) {
            this.val = val;
            this.neighbors = new ArrayList<>();
        }
    }

    public Node cloneGraph(Node node) {
        if (node == null) return null;

        Map<Node, Node> visited = new HashMap<>();
        Queue<Node> queue = new ArrayDeque<>();

        Node cloneRoot = new Node(node.val);
        visited.put(node, cloneRoot);
        queue.offer(node);

        while (!queue.isEmpty()) {
            Node curr = queue.poll();
            for (Node neighbor : curr.neighbors) {
                if (!visited.containsKey(neighbor)) {
                    visited.put(neighbor, new Node(neighbor.val));
                    queue.offer(neighbor);
                }
                visited.get(curr).neighbors.add(visited.get(neighbor));
            }
        }

        return cloneRoot;
    }
}
```

### Example 2: Number of Islands (LeetCode 200, Grid BFS)

```java
import java.util.*;

public class NumberOfIslands {
    private static final int[][] DIRS = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    public int numIslands(char[][] grid) {
        if (grid == null || grid.length == 0) return 0;
        int m = grid.length, n = grid[0].length;
        int islands = 0;

        for (int r = 0; r < m; r++) {
            for (int c = 0; c < n; c++) {
                if (grid[r][c] == '1') {
                    islands++;
                    bfs(grid, r, c, m, n);
                }
            }
        }
        return islands;
    }

    private void bfs(char[][] grid, int r, int c, int m, int n) {
        grid[r][c] = '0'; // Mutate directly to mark visited
        Queue<int[]> queue = new ArrayDeque<>();
        queue.offer(new int[]{r, c});

        while (!queue.isEmpty()) {
            int[] cell = queue.poll();
            int currR = cell[0], currC = cell[1];

            for (int[] d : DIRS) {
                int nr = currR + d[0];
                int nc = currC + d[1];

                if (nr >= 0 && nr < m && nc >= 0 && nc < n && grid[nr][nc] == '1') {
                    grid[nr][nc] = '0'; // Immediately mark to avoid duplicate queuing!
                    queue.offer(new int[]{nr, nc});
                }
            }
        }
    }
}
```

### Example 3: Word Ladder (LeetCode 127, Bidirectional BFS)

```java
import java.util.*;

public class WordLadder {
    public int ladderLength(String beginWord, String endWord, List<String> wordList) {
        Set<String> dict = new HashSet<>(wordList);
        if (!dict.contains(endWord)) return 0;

        Set<String> startSet = new HashSet<>();
        Set<String> endSet = new HashSet<>();
        Set<String> visited = new HashSet<>();

        startSet.add(beginWord);
        endSet.add(endWord);
        visited.add(beginWord);
        visited.add(endWord);

        int steps = 1;

        while (!startSet.isEmpty() && !endSet.isEmpty()) {
            // Always expand the smaller frontier
            if (startSet.size() > endSet.size()) {
                Set<String> tmp = startSet;
                startSet = endSet;
                endSet = tmp;
            }

            Set<String> nextLevel = new HashSet<>();
            for (String word : startSet) {
                char[] chars = word.toCharArray();
                for (int i = 0; i < chars.length; i++) {
                    char orig = chars[i];
                    for (char c = 'a'; c <= 'z'; c++) {
                        if (c == orig) continue;
                        chars[i] = c;
                        String candidate = new String(chars);

                        if (endSet.contains(candidate)) {
                            return steps + 1; // Frontiers met!
                        }

                        if (dict.contains(candidate) && visited.add(candidate)) {
                            nextLevel.add(candidate);
                        }
                    }
                    chars[i] = orig;
                }
            }

            startSet = nextLevel;
            steps++;
        }

        return 0;
    }
}
```

### Example 4: Is Graph Bipartite? (LeetCode 785, 2-Coloring)

```java
import java.util.*;

public class IsBipartite {
    public boolean isBipartite(int[][] graph) {
        int n = graph.length;
        // 0 = uncolored, 1 = red, -1 = blue
        int[] colors = new int[n];

        for (int i = 0; i < n; i++) {
            if (colors[i] != 0) continue;

            Queue<Integer> queue = new ArrayDeque<>();
            queue.offer(i);
            colors[i] = 1;

            while (!queue.isEmpty()) {
                int curr = queue.poll();
                for (int neighbor : graph[curr]) {
                    if (colors[neighbor] == 0) {
                        colors[neighbor] = -colors[curr];
                        queue.offer(neighbor);
                    } else if (colors[neighbor] == colors[curr]) {
                        return false; // Adjacent nodes have identical color!
                    }
                }
            }
        }

        return true;
    }
}
```

### Example 5: Graph Valid Tree (LeetCode 261, Cycle & Connectivity Check)

```java
import java.util.*;

public class GraphValidTree {
    public boolean validTree(int n, int[][] edges) {
        // A tree with n vertices MUST have exactly n - 1 edges
        if (edges.length != n - 1) return false;

        List<List<Integer>> adj = new ArrayList<>(n);
        for (int i = 0; i < n; i++) adj.add(new ArrayList<>());
        for (int[] e : edges) {
            adj.get(e[0]).add(e[1]);
            adj.get(e[1]).add(e[0]);
        }

        boolean[] visited = new boolean[n];
        Queue<Integer> queue = new ArrayDeque<>();
        queue.offer(0);
        visited[0] = true;
        int visitedCount = 0;

        while (!queue.isEmpty()) {
            int node = queue.poll();
            visitedCount++;
            for (int neighbor : adj.get(node)) {
                if (!visited[neighbor]) {
                    visited[neighbor] = true;
                    queue.offer(neighbor);
                }
            }
        }

        // Must visit all vertices to be connected
        return visitedCount == n;
    }
}
```

---

## 6. Best Practices

1. **Mark Visited Upon Offer, Not Poll**: In BFS, always set `visited[neighbor] = true` *immediately* when offering to the queue. If you wait until `poll()`, duplicate elements are enqueued exponentially on dense graphs.
2. **Use Flat Arrays for Visited Sets**: Prefer `boolean[] visited = new boolean[N]` or `int[] visited` over `Set<Integer>`. Flat arrays use 1 byte per vertex and run an order of magnitude faster.
3. **Prefer Bidirectional BFS for Pathfinding**: If both start and destination are known, search from both ends simultaneously. This shrinks search space from $O(b^d)$ to $O(b^{d/2})$.
4. **Guard Against Deep Stack In DFS**: Deep graphs ($V > 10,000$) will trigger `StackOverflowError` in recursive DFS. Use an explicit `ArrayDeque<Integer>` as a stack or increase `-Xss`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Marking visited upon `poll()` instead of `offer()` | Enqueues identical nodes multiple times, causing OOM | Mark `visited[neighbor] = true` right before `queue.offer()`. |
| Undirected graph cycle check without tracking parent | Reports an immediate false cycle back to caller's parent | Pass `parent` parameter in DFS / BFS and ignore edge if `neighbor == parent`. |
| Assuming disconnected graphs have single root | Misses isolated subgraphs or components | Loop over all vertices $0 \dots V-1$ and trigger search if `!visited[i]`. |
| Modifying graph during iterator traversal | Triggers `ConcurrentModificationException` | Collect removals or clone adjacency list before mutating. |
| Forgetting to check edge count before tree checks | Missing early exit on cycles ($E \ge V$) | Enforce $E == V - 1$ before running BFS/DFS. |

---

## 8. Performance & Security Notes

- **Time Complexity**:
  - Adjacency List: $O(V + E)$
  - Adjacency Matrix: $O(V^2)$
- **Space Complexity**:
  - Adjacency List: $O(V + E)$
  - BFS Queue: $O(W)$ where $W$ is the maximum width of the graph (up to $O(V)$)
  - DFS Call Stack: $O(H)$ where $H$ is the maximum depth of the graph (up to $O(V)$)
- **Denial of Service**: Unbounded recursive DFS on user-supplied graphs can crash JVMs via `StackOverflowError`. Always enforce depth limits or use iterative implementations.

---

## 9. Comparison / When to Use What

| Criteria | BFS | DFS | Bidirectional BFS |
|---|---|---|---|
| **Shortest Path (Unweighted)** | Optimal ($O(V+E)$) | Not guaranteed | Optimal & much faster |
| **Memory Consumption** | $O(\text{Width})$ — high on dense trees | $O(\text{Depth})$ — low on bushy trees | $O(b^{d/2})$ |
| **Topological Sort** | Yes (Kahn's algorithm) | Yes (post-order reversal) | No |
| **Cycle Detection** | Yes | Yes (3-coloring) | No |
| **Maze / Backtracking** | Cumbersome | Natural & idiomatic | Inapplicable |

---

## 10. Interview Questions

<details>
<summary>1. When does BFS fail to find the shortest path in a graph?</summary>

BFS fails to find the shortest path when edges have non-uniform weights. In such cases, a path with more hops could have a lower total weight than a path with fewer hops. Use Dijkstra's algorithm for non-negative weights or Bellman-Ford for graphs with negative weights.
</details>

<details>
<summary>2. Why does Bidirectional BFS have a lower time complexity than standard BFS?</summary>

For branching factor $b$ and distance $d$, standard BFS explores $O(b^d)$ nodes. Bidirectional BFS stops when two searches meet at depth $d/2$, exploring $2 \times O(b^{d/2}) = O(b^{d/2})$ nodes. When $b=10$ and $d=6$, standard BFS explores $1,000,000$ nodes while bidirectional BFS explores only $2,000$ nodes.
</details>

<details>
<summary>3. How do you detect a cycle in an undirected graph using BFS?</summary>

During BFS, maintain a `parent` mapping for each node. When expanding neighbors of node $u$, if neighbor $v$ is already visited and $v \neq \text{parent}[u]$, a cycle is detected.
</details>

<details>
<summary>4. What is the 3-coloring algorithm for directed cycle detection?</summary>

Vertices are colored:
- White (0): Unvisited
- Gray (1): Currently on the recursion stack
- Black (2): Completely explored

If DFS visits an edge leading to a Gray vertex, that edge is a back-edge and the graph contains a cycle.
</details>

<details>
<summary>5. How do you find all connected components in an undirected graph?</summary>

Iterate from vertex $0$ to $V-1$. Whenever an unvisited vertex $i$ is encountered, increment the component counter and launch a full BFS or DFS starting from $i$, marking all reachable nodes as visited.
</details>

<details>
<summary>6. Why is `ArrayDeque` preferred over `LinkedList` for the BFS queue in Java?</summary>

`ArrayDeque` is backed by a circular resizable array. It has zero per-element node allocation overhead, fits neatly into CPU cache lines, and never garbage-collects node objects. It is consistently 2x to 5x faster than `LinkedList`.
</details>

<details>
<summary>7. Can an undirected graph be bipartite if it has an odd-length cycle?</summary>

No. A graph is bipartite if and only if it contains no odd-length cycles. If an odd cycle exists, it is impossible to 2-color the vertices without adjacent nodes sharing a color.
</details>

<details>
<summary>8. How do you avoid `StackOverflowError` in deep DFS traversals?</summary>

Transform the recursive method into an iterative one using an explicit `ArrayDeque<Integer>` stack, or increase the thread stack size using the `-Xss` JVM flag (e.g., `-Xss4m`).
</details>

---

## 11. Practice Exercises

### Exercise 1: Pacific Atlantic Water Flow (LeetCode 417)
Find all coordinates from which rainwater can flow to both Pacific (top/left) and Atlantic (bottom/right) oceans.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class PacificAtlantic {
    private static final int[][] DIRS = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    public List<List<Integer>> pacificAtlantic(int[][] heights) {
        int m = heights.length, n = heights[0].length;
        boolean[][] pac = new boolean[m][n];
        boolean[][] atl = new boolean[m][n];

        Queue<int[]> qPac = new ArrayDeque<>();
        Queue<int[]> qAtl = new ArrayDeque<>();

        for (int r = 0; r < m; r++) {
            qPac.offer(new int[]{r, 0}); pac[r][0] = true;
            qAtl.offer(new int[]{r, n - 1}); atl[r][n - 1] = true;
        }
        for (int c = 0; c < n; c++) {
            qPac.offer(new int[]{0, c}); pac[0][c] = true;
            qAtl.offer(new int[]{m - 1, c}); atl[m - 1][c] = true;
        }

        bfs(heights, qPac, pac, m, n);
        bfs(heights, qAtl, atl, m, n);

        List<List<Integer>> result = new ArrayList<>();
        for (int r = 0; r < m; r++) {
            for (int c = 0; c < n; c++) {
                if (pac[r][c] && atl[r][c]) {
                    result.add(List.of(r, c));
                }
            }
        }
        return result;
    }

    private void bfs(int[][] h, Queue<int[]> q, boolean[][] visited, int m, int n) {
        while (!q.isEmpty()) {
            int[] cell = q.poll();
            int r = cell[0], c = cell[1];

            for (int[] d : DIRS) {
                int nr = r + d[0], nc = c + d[1];
                if (nr >= 0 && nr < m && nc >= 0 && nc < n && !visited[nr][nc] && h[nr][nc] >= h[r][c]) {
                    visited[nr][nc] = true;
                    q.offer(new int[]{nr, nc});
                }
            }
        }
    }
}
```
</details>

### Exercise 2: All Paths From Source to Target (LeetCode 797)
Find all paths from node $0$ to node $n - 1$ in a DAG.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class AllPathsSourceTarget {
    public List<List<Integer>> allPathsSourceTarget(int[][] graph) {
        List<List<Integer>> result = new ArrayList<>();
        List<Integer> path = new ArrayList<>();
        path.add(0);
        dfs(graph, 0, path, result);
        return result;
    }

    private void dfs(int[][] graph, int node, List<Integer> path, List<List<Integer>> res) {
        if (node == graph.length - 1) {
            res.add(new ArrayList<>(path));
            return;
        }

        for (int neighbor : graph[node]) {
            path.add(neighbor);
            dfs(graph, neighbor, path, res);
            path.remove(path.size() - 1); // Backtrack
        }
    }
}
```
</details>

### Exercise 3: Reconstruct Itinerary (LeetCode 332, Hierholzer's Eulerian Path)

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class ReconstructItinerary {
    public List<String> findItinerary(List<List<String>> tickets) {
        Map<String, PriorityQueue<String>> targets = new HashMap<>();
        for (List<String> ticket : tickets) {
            targets.computeIfAbsent(ticket.get(0), k -> new PriorityQueue<>()).add(ticket.get(1));
        }

        LinkedList<String> route = new LinkedList<>();
        dfs("JFK", targets, route);
        return route;
    }

    private void dfs(String airport, Map<String, PriorityQueue<String>> targets, LinkedList<String> route) {
        PriorityQueue<String> dests = targets.get(airport);
        while (dests != null && !dests.isEmpty()) {
            dfs(dests.poll(), targets, route);
        }
        route.addFirst(airport); // Post-order collection
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Adjacency List**: Standard choice for graph problems ($O(V + E)$).
- **BFS Shortest Path**: Guarantees shortest path in unweighted graphs. Mark visited when adding to queue.
- **Queue Implementation**: Always use `ArrayDeque` over `LinkedList`.
- **DFS Cycle Detection**: Use 3 states (White, Gray, Black) for directed graphs; use `parent` tracking for undirected graphs.
- **Bidirectional BFS**: Dramatically reduces search radius for bidirectional shortest path problems.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 22: Elementary Graph Algorithms](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Robert Sedgewick: Algorithms, 4th Edition: Undirected & Directed Graphs](https://algs4.cs.princeton.edu/41graph/)
- [Baeldung: Guide to Graph Traversals in Java](https://www.baeldung.com/java-graph-has-cycle)
- [LeetCode Graph Exploration Card](https://leetcode.com/explore/featured/card/graph/)
