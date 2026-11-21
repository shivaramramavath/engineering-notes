# Shortest Path Algorithms

> Comprehensive shortest path algorithms: Dijkstra's priority-queue traversal, Bellman-Ford negative edge relaxation, and Floyd-Warshall all-pairs dynamic programming.

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

Shortest path algorithms determine the path between two vertices in a weighted graph such that the sum of the weights of its constituent edges is minimized.
- **Single-Source Shortest Path (SSSP)**: Computes shortest paths from a single source vertex to all other vertices.
  - **Dijkstra's Algorithm**: Optimal for non-negative edge weights ($O((V + E) \log V)$).
  - **Bellman-Ford Algorithm**: Handles negative edge weights and detects negative weight cycles ($O(V \times E)$).
- **All-Pairs Shortest Path (APSP)**: Computes shortest paths between every pair of vertices.
  - **Floyd-Warshall Algorithm**: Dynamic programming approach ($O(V^3)$).

---

## 2. Core Concepts

### 1. Edge Relaxation
The fundamental operation common to all shortest path algorithms is **relaxation**. Given an edge from $u$ to $v$ with weight $w$:
```
if (dist[v] > dist[u] + w) {
    dist[v] = dist[u] + w;
}
```

### 2. Dijkstra's Greedy Choice Property
Dijkstra maintains a priority queue of unfinalized vertices ordered by tentative distance. Because all edge weights are strictly $\ge 0$, extracting the minimum distance vertex guarantees that no future relaxation can discover a shorter path to it.

### 3. Bellman-Ford Invariant
In a graph with $V$ vertices without negative cycles, any shortest path contains at most $V - 1$ edges. Relaxing every edge in the graph $V - 1$ times guarantees convergence. An additional $V$-th pass that succeeds in further relaxation proves the existence of a **negative weight cycle**.

### 4. Floyd-Warshall Invariant
Let $dp[k][i][j]$ be the shortest path from $i$ to $j$ using only intermediate vertices from $\{0, \dots, k\}$.
$$dp[k][i][j] = \min(dp[k-1][i][j], dp[k-1][i][k] + dp[k-1][k][j])$$
This can be computed in-place using a 2D matrix $dp[i][j]$.

---

## 3. How It Works Internally

### Stale Node Handling in Java Dijkstra
Standard Java `PriorityQueue` lacks an $O(\log N)$ `decreaseKey` method. Re-inserting updated pairs `(v, newDist)` creates duplicate entries for $v$.

```
PriorityQueue Heap Array:
[(node=2, dist=4), (node=2, dist=10), (node=3, dist=7)]
          ^                  ^
       Latest             Stale Entry!
```

To eliminate performance degradation, check if the popped distance exceeds the recorded best:
```java
if (currentDist > dist[u]) {
    continue; // Stale entry; drop immediately!
}
```
This guarantees each vertex expands its neighbors at most once, bounding total heap pushes to $O(E)$ and operations to $O(E \log V)$.

---

## 4. Syntax & Usage

### Production Dijkstra's Algorithm Implementation

```java
import java.util.*;

public class Dijkstra {
    public record Edge(int to, int weight) {}
    public record NodeDist(int node, long dist) implements Comparable<NodeDist> {
        @Override
        public int compareTo(NodeDist other) {
            return Long.compare(this.dist, other.dist);
        }
    }

    public static long[] findShortestPaths(int n, List<List<Edge>> adj, int start) {
        long[] dist = new long[n];
        Arrays.fill(dist, Long.MAX_VALUE);
        dist[start] = 0;

        PriorityQueue<NodeDist> pq = new PriorityQueue<>();
        pq.offer(new NodeDist(start, 0));

        while (!pq.isEmpty()) {
            NodeDist current = pq.poll();
            int u = current.node();
            long d = current.dist();

            if (d > dist[u]) continue; // Skip stale heap entry

            for (Edge edge : adj.get(u)) {
                int v = edge.to();
                long newDist = d + edge.weight();

                if (newDist < dist[v]) {
                    dist[v] = newDist;
                    pq.offer(new NodeDist(v, newDist));
                }
            }
        }

        return dist;
    }
}
```

---

## 5. Practical Examples

### Example 1: Network Delay Time (LeetCode 743, Dijkstra SSSP)

```java
import java.util.*;

public class NetworkDelayTime {
    public record Edge(int to, int time) {}

    public int networkDelayTime(int[][] times, int n, int k) {
        List<List<Edge>> adj = new ArrayList<>(n + 1);
        for (int i = 0; i <= n; i++) adj.add(new ArrayList<>());
        for (int[] t : times) {
            adj.get(t[0]).add(new Edge(t[1], t[2]));
        }

        int[] dist = new int[n + 1];
        Arrays.fill(dist, Integer.MAX_VALUE);
        dist[k] = 0;

        PriorityQueue<int[]> pq = new PriorityQueue<>(Comparator.comparingInt(a -> a[1]));
        pq.offer(new int[]{k, 0});

        while (!pq.isEmpty()) {
            int[] curr = pq.poll();
            int u = curr[0], d = curr[1];
            if (d > dist[u]) continue;

            for (Edge e : adj.get(u)) {
                if (d + e.time() < dist[e.to()]) {
                    dist[e.to()] = d + e.time();
                    pq.offer(new int[]{e.to(), dist[e.to()]});
                }
            }
        }

        int maxTime = 0;
        for (int i = 1; i <= n; i++) {
            if (dist[i] == Integer.MAX_VALUE) return -1;
            maxTime = Math.max(maxTime, dist[i]);
        }
        return maxTime;
    }
}
```

### Example 2: Cheapest Flights Within K Stops (LeetCode 787, Bellman-Ford)

```java
import java.util.Arrays;

public class CheapestFlights {
    public int findCheapestPrice(int n, int[][] flights, int src, int dst, int k) {
        int[] dist = new int[n];
        Arrays.fill(dist, Integer.MAX_VALUE);
        dist[src] = 0;

        // Relax at most k + 1 edges
        for (int i = 0; i <= k; i++) {
            int[] temp = Arrays.copyOf(dist, n);
            for (int[] flight : flights) {
                int u = flight[0], v = flight[1], w = flight[2];
                if (dist[u] != Integer.MAX_VALUE && dist[u] + w < temp[v]) {
                    temp[v] = dist[u] + w;
                }
            }
            dist = temp;
        }

        return dist[dst] == Integer.MAX_VALUE ? -1 : dist[dst];
    }
}
```

### Example 3: Path With Minimum Effort (LeetCode 1631, Matrix Dijkstra)

```java
import java.util.*;

public class PathWithMinimumEffort {
    private static final int[][] DIRS = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    public int minimumEffortPath(int[][] heights) {
        int m = heights.length, n = heights[0].length;
        int[][] effort = new int[m][n];
        for (int[] row : effort) Arrays.fill(row, Integer.MAX_VALUE);
        effort[0][0] = 0;

        PriorityQueue<int[]> pq = new PriorityQueue<>(Comparator.comparingInt(a -> a[2]));
        pq.offer(new int[]{0, 0, 0}); // [r, c, effort]

        while (!pq.isEmpty()) {
            int[] curr = pq.poll();
            int r = curr[0], c = curr[1], diff = curr[2];
            if (r == m - 1 && c == n - 1) return diff;
            if (diff > effort[r][c]) continue;

            for (int[] d : DIRS) {
                int nr = r + d[0], nc = c + d[1];
                if (nr >= 0 && nr < m && nc >= 0 && nc < n) {
                    int nextEffort = Math.max(diff, Math.abs(heights[nr][nc] - heights[r][c]));
                    if (nextEffort < effort[nr][nc]) {
                        effort[nr][nc] = nextEffort;
                        pq.offer(new int[]{nr, nc, nextEffort});
                    }
                }
            }
        }

        return 0;
    }
}
```

### Example 4: City With Smallest Neighbors at Threshold (LeetCode 1334, Floyd-Warshall)

```java
import java.util.Arrays;

public class FindTheCity {
    public int findTheCity(int n, int[][] edges, int distanceThreshold) {
        int[][] dist = new int[n][n];
        for (int[] row : dist) Arrays.fill(row, 100_000_000);
        for (int i = 0; i < n; i++) dist[i][i] = 0;

        for (int[] e : edges) {
            dist[e[0]][e[1]] = e[2];
            dist[e[1]][e[0]] = e[2];
        }

        // Floyd-Warshall 3 nested loops
        for (int k = 0; k < n; k++) {
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) {
                    if (dist[i][k] + dist[k][j] < dist[i][j]) {
                        dist[i][j] = dist[i][k] + dist[k][j];
                    }
                }
            }
        }

        int minNeighbors = Integer.MAX_VALUE;
        int bestCity = -1;

        for (int i = 0; i < n; i++) {
            int reachable = 0;
            for (int j = 0; j < n; j++) {
                if (i != j && dist[i][j] <= distanceThreshold) {
                    reachable++;
                }
            }

            if (reachable <= minNeighbors) {
                minNeighbors = reachable;
                bestCity = i; // Prefer greater city ID on ties
            }
        }

        return bestCity;
    }
}
```

### Example 5: Swim in Rising Water (LeetCode 778, Min-Max Dijkstra)

```java
import java.util.*;

public class SwimInWater {
    private static final int[][] DIRS = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    public int swimInWater(int[][] grid) {
        int n = grid.length;
        boolean[][] visited = new boolean[n][n];
        PriorityQueue<int[]> pq = new PriorityQueue<>(Comparator.comparingInt(a -> a[2]));

        pq.offer(new int[]{0, 0, grid[0][0]});
        visited[0][0] = true;

        while (!pq.isEmpty()) {
            int[] cell = pq.poll();
            int r = cell[0], c = cell[1], time = cell[2];
            if (r == n - 1 && c == n - 1) return time;

            for (int[] d : DIRS) {
                int nr = r + d[0], nc = c + d[1];
                if (nr >= 0 && nr < n && nc >= 0 && nc < n && !visited[nr][nc]) {
                    visited[nr][nc] = true;
                    pq.offer(new int[]{nr, nc, Math.max(time, grid[nr][nc])});
                }
            }
        }

        return 0;
    }
}
```

---

## 6. Best Practices

1. **Always Drop Stale Entries**: In Java Dijkstra, always include `if (d > dist[u]) continue;`. Omitting this degrades performance from $O(E \log V)$ to exponential on adversarial graphs.
2. **Prevent Integer Overflow During Relaxation**: If initializing distances to `Integer.MAX_VALUE`, evaluating `dist[u] + weight` causes integer wrap-around to negative values! Use `long` arrays or initialize with `Integer.MAX_VALUE / 2` or `1e9`.
3. **Use Bellman-Ford Copy Array for Bounded Hops**: When solving $K$-stop problems, relax using a temporary snapshot of the previous iteration's distance array to prevent cascaded multi-hop relaxation in a single step.
4. **Order of Loops in Floyd-Warshall**: The intermediate node `k` **must** be the outermost loop: `for k ... for i ... for j ...`. Swapping loop orders produces completely invalid results.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Dijkstra on negative edge weights | Greedy choice invariant fails; nodes finalized prematurely | Use Bellman-Ford or SPFA. |
| Integer overflow on `dist[u] + w` | Overflows to negative numbers; corrupts min distance | Use `long[]` or sentinel value `1_000_000_000`. |
| Outermost loop variable in Floyd-Warshall | Putting `i` or `j` outermost breaks DP transition | Ensure intermediate vertex `k` is strictly the outermost loop. |
| Forgetting to clone array in $K$-stop Bellman-Ford | Chains updates within the same iteration, violating hop bounds | Clone `dist` array at the start of each step. |

---

## 8. Performance & Security Notes

- **Time Complexity**:
  - Dijkstra: $O((V + E) \log V)$ with Binary Heap; $O(V \log V + E)$ with Fibonacci Heap.
  - Bellman-Ford: $O(V \times E)$.
  - Floyd-Warshall: $O(V^3)$.
- **Space Complexity**:
  - Dijkstra: $O(V + E)$
  - Bellman-Ford: $O(V)$
  - Floyd-Warshall: $O(V^2)$
- **DoS Vectors**: Submitting negative cycles into Dijkstra causes infinite loops if cycle checks are absent. Enforce edge validation or cycle timeouts.

---

## 9. Comparison / When to Use What

| Algorithm | Type | Graph Weights | Complexity | Best For |
|---|---|---|---|---|
| **BFS** | SSSP | Unweighted ($w = 1$) | $O(V + E)$ | Grid shortest paths |
| **Dijkstra** | SSSP | Non-negative ($w \ge 0$) | $O((V + E)\log V)$ | Road networks, routing |
| **Bellman-Ford** | SSSP | Negative allowed | $O(V \times E)$ | Forex arbitrage, bounded hops |
| **Floyd-Warshall** | APSP | Negative allowed | $O(V^3)$ | Dense graphs with $V \le 400$ |

---

## 10. Interview Questions

<details>
<summary>1. Why does Dijkstra's algorithm fail with negative edge weights?</summary>

Dijkstra relies on a greedy invariant: once a vertex is popped from the priority queue, its shortest distance is finalized and will never decrease. A negative edge encountered later could lower the distance to an already-finalized vertex, invalidating the greedy assumption.
</details>

<details>
<summary>2. How do you detect a negative weight cycle using Bellman-Ford?</summary>

Run the standard relaxation loop $V - 1$ times. Then execute a $V$-th pass. If any edge $u \to v$ can still be relaxed (`dist[v] > dist[u] + w`), that edge belongs to or is reachable from a negative weight cycle.
</details>

<details>
<summary>3. Why must the $k$ loop be outermost in Floyd-Warshall?</summary>

Floyd-Warshall is a dynamic programming algorithm where state $k$ builds on solutions using intermediate nodes from $\{0, \dots, k-1\}$. If $k$ is inner, paths using intermediate nodes larger than $k$ would be referenced before they are computed.
</details>

<details>
<summary>4. What is the SPFA (Shortest Path Faster Algorithm)?</summary>

SPFA is a queue-optimized variant of Bellman-Ford. Instead of blindly relaxing all edges in every pass, it only relaxes outgoing edges from vertices whose distances were updated in the previous step. Average time is $O(E)$, but worst case remains $O(V \times E)$.
</details>

<details>
<summary>5. How can you find the actual shortest path (sequence of vertices), not just the distance?</summary>

Maintain a `parent[]` array. Whenever edge $u \to v$ successfully relaxes `dist[v]`, set `parent[v] = u`. At the end, backtrack from the destination to source and reverse the list.
</details>

<details>
<summary>6. How does A* Search relate to Dijkstra's algorithm?</summary>

A* is Dijkstra informed by a heuristic function $h(u)$ estimating the remaining distance to target. Its priority queue sorts by $f(u) = g(u) + h(u)$ where $g(u)$ is known distance from start. When $h(u) = 0$, A* is identical to Dijkstra.
</details>

<details>
<summary>7. What is Johnson's Algorithm for All-Pairs Shortest Paths?</summary>

Johnson's algorithm uses Bellman-Ford to re-weight all edges to non-negative values using vertex potentials ($w'(u,v) = w(u,v) + h(u) - h(v)$), then runs Dijkstra from every vertex. Time complexity is $O(V^2 \log V + VE)$, far superior to Floyd-Warshall on sparse graphs.
</details>

<details>
<summary>8. How do you find currency arbitrage opportunities using graph algorithms?</summary>

Model currencies as vertices and exchange rates $R_{ij}$ as directed edges. An arbitrage exists if $R_{12} \times R_{23} \times \dots \times R_{k1} > 1$. Taking $-\ln(R_{ij})$ transforms the product into a sum: $\sum -\ln(R) < 0$. Running Bellman-Ford detects this negative cycle in $O(V \times E)$ time.
</details>

---

## 11. Practice Exercises

### Exercise 1: Number of Ways to Arrive at Destination (LeetCode 1976)
Find the number of ways to reach node $n - 1$ from node $0$ in the minimum time modulo $10^9 + 7$.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class NumberOfWaysToArrive {
    public int countPaths(int n, int[][] roads) {
        final int MOD = 1_000_000_007;
        List<List<int[]>> adj = new ArrayList<>(n);
        for (int i = 0; i < n; i++) adj.add(new ArrayList<>());
        for (int[] r : roads) {
            adj.get(r[0]).add(new int[]{r[1], r[2]});
            adj.get(r[1]).add(new int[]{r[0], r[2]});
        }

        long[] dist = new long[n];
        Arrays.fill(dist, Long.MAX_VALUE);
        dist[0] = 0;

        long[] ways = new long[n];
        ways[0] = 1;

        PriorityQueue<long[]> pq = new PriorityQueue<>(Comparator.comparingLong(a -> a[1]));
        pq.offer(new long[]{0, 0});

        while (!pq.isEmpty()) {
            long[] curr = pq.poll();
            int u = (int) curr[0];
            long d = curr[1];

            if (d > dist[u]) continue;

            for (int[] edge : adj.get(u)) {
                int v = edge[0];
                long w = edge[1];

                if (d + w < dist[v]) {
                    dist[v] = d + w;
                    ways[v] = ways[u];
                    pq.offer(new long[]{v, dist[v]});
                } else if (d + w == dist[v]) {
                    ways[v] = (ways[v] + ways[u]) % MOD;
                }
            }
        }

        return (int) ways[n - 1];
    }
}
```
</details>

### Exercise 2: Second Minimum Time to Reach Destination (LeetCode 2045)
Find the second strictly minimum time to reach vertex $n$ in an unweighted graph with traffic signal wait delays.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class SecondMinimumTime {
    public int secondMinimum(int n, int[][] edges, int time, int change) {
        List<List<Integer>> adj = new ArrayList<>(n + 1);
        for (int i = 0; i <= n; i++) adj.add(new ArrayList<>());
        for (int[] e : edges) {
            adj.get(e[0]).add(e[1]);
            adj.get(e[1]).add(e[0]);
        }

        int[] dist1 = new int[n + 1];
        int[] dist2 = new int[n + 1];
        Arrays.fill(dist1, -1);
        Arrays.fill(dist2, -1);

        Queue<int[]> queue = new ArrayDeque<>();
        queue.offer(new int[]{1, 0});
        dist1[1] = 0;

        while (!queue.isEmpty()) {
            int[] curr = queue.poll();
            int u = curr[0], t = curr[1];

            // Calculate signal wait time
            int round = t / change;
            int actualTime = (round % 2 == 1) ? (round + 1) * change + time : t + time;

            for (int v : adj.get(u)) {
                if (dist1[v] == -1) {
                    dist1[v] = actualTime;
                    queue.offer(new int[]{v, actualTime});
                } else if (dist2[v] == -1 && actualTime > dist1[v]) {
                    dist2[v] = actualTime;
                    if (v == n) return actualTime;
                    queue.offer(new int[]{v, actualTime});
                }
            }
        }

        return 0;
    }
}
```
</details>

### Exercise 3: Shortest Path to Get All Keys (LeetCode 864, State Bitmask BFS)

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class ShortestPathAllKeys {
    private static final int[][] DIRS = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    public int shortestPathAllKeys(String[] grid) {
        int m = grid.length, n = grid[0].length();
        int allKeys = 0;
        int startR = -1, startC = -1;

        for (int r = 0; r < m; r++) {
            for (int c = 0; c < n; c++) {
                char ch = grid[r].charAt(c);
                if (ch == '@') {
                    startR = r; startC = c;
                } else if (ch >= 'a' && ch <= 'f') {
                    allKeys |= (1 << (ch - 'a'));
                }
            }
        }

        boolean[][][] visited = new boolean[m][n][64];
        Queue<int[]> queue = new ArrayDeque<>();
        queue.offer(new int[]{startR, startC, 0, 0}); // [r, c, keysMask, steps]
        visited[startR][startC][0] = true;

        while (!queue.isEmpty()) {
            int[] curr = queue.poll();
            int r = curr[0], c = curr[1], keys = curr[2], steps = curr[3];
            if (keys == allKeys) return steps;

            for (int[] d : DIRS) {
                int nr = r + d[0], nc = c + d[1];
                if (nr < 0 || nr >= m || nc < 0 || nc >= n) continue;
                char ch = grid[nr].charAt(nc);
                if (ch == '#') continue;

                int nextKeys = keys;
                if (ch >= 'a' && ch <= 'f') {
                    nextKeys |= (1 << (ch - 'a'));
                } else if (ch >= 'A' && ch <= 'F') {
                    if ((keys & (1 << (ch - 'A'))) == 0) continue; // Missing lock key
                }

                if (!visited[nr][nc][nextKeys]) {
                    visited[nr][nc][nextKeys] = true;
                    queue.offer(new int[]{nr, nc, nextKeys, steps + 1});
                }
            }
        }

        return -1;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Dijkstra**: $O((V+E)\log V)$ greedy SSSP for non-negative graphs. Discard stale queue items.
- **Bellman-Ford**: $O(VE)$ relaxation for graphs with negative weights; cycle check on pass $V$.
- **Floyd-Warshall**: $O(V^3)$ APSP dynamic programming. Loop $k$ must be outermost!
- **Overflow Hazard**: Always initialize distances to `Long.MAX_VALUE` or `1e9`, never `Integer.MAX_VALUE` with primitive addition.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapters 24 & 25: Single-Source & All-Pairs Shortest Paths](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Dijkstra, E. W. (1959). A note on two problems in connexion with graphs](https://link.springer.com/article/10.1007/BF01386390)
- [Baeldung: Guide to Dijkstra's Algorithm in Java](https://www.baeldung.com/java-dijkstra)
- [Robert Sedgewick: Algorithms, 4th Edition: Shortest Paths](https://algs4.cs.princeton.edu/44sp/)
