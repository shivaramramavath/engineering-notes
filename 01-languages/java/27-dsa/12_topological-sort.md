# Topological Sort

> Linear ordering of vertices in Directed Acyclic Graphs (DAGs) where every directed edge $u \to v$ guarantees $u$ comes before $v$.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [11_graphs-bfs-dfs.md](11_graphs-bfs-dfs.md), [03_stacks-and-queues.md](03_stacks-and-queues.md) |
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

A **Topological Sort** of a directed graph is a linear ordering of its vertices such that for every directed edge from vertex $u$ to vertex $v$, vertex $u$ appears before vertex $v$ in the ordering.

### Key Invariant
A topological ordering is possible **if and only if the graph is a Directed Acyclic Graph (DAG)**. If the graph contains even one cycle ($A \to B \to C \to A$), no valid linear sequence can exist because each node must precede itself.

### Common Real-World Use Cases
- **Build Systems (Maven / Gradle / Make)**: Compiling dependencies before dependent libraries.
- **Task & Workflow Schedulers (Airflow / Temporal)**: Running upstream data pipeline jobs before downstream aggregations.
- **Course Prerequisites**: Determining graduation course plans.
- **Spreadsheet Formula Evaluation**: Updating cells in dependency order.

---

## 2. Core Concepts

There are two canonical algorithms to compute topological order:

1. **Kahn's Algorithm (BFS In-Degree Reduction)**:
   - Calculate the in-degree (number of incoming edges) for every vertex.
   - Enqueue all vertices with in-degree 0 into a FIFO queue.
   - While the queue is not empty:
     - Pop vertex $u$ and append it to the topological order list.
     - For each outgoing neighbor $v$, decrement in-degree: `inDegree[v]--`.
     - If `inDegree[v] == 0`, enqueue $v$.
   - If the output list size is less than $V$, the graph contains a **cycle**.

2. **DFS Post-Order Reversal (Tarjan's Approach)**:
   - For every unvisited vertex, launch DFS.
   - Maintain 3 states (0=Unvisited, 1=Visiting/On Stack, 2=Visited).
   - If a neighbor is currently 1 (Visiting), a cycle exists.
   - When DFS for vertex $u$ finishes (all descendants visited), push $u$ onto a stack.
   - Reversing the post-order sequence yields a valid topological sort.

---

## 3. How It Works Internally

### Kahn's BFS In-Degree Trace

Consider dependencies: $0 \to 1, 0 \to 2, 1 \to 3, 2 \to 3$

```
Initial In-degrees:
[0]: 0  [1]: 1  [2]: 1  [3]: 2

Step 1: In-degree 0 is {0}. Queue = [0]. Order = []
Step 2: Pop 0. Decrement in-degree of 1 and 2.
        In-degrees: [1]: 0, [2]: 0. Queue = [1, 2]. Order = [0]
Step 3: Pop 1. Decrement in-degree of 3 (now 1). Queue = [2]. Order = [0, 1]
Step 4: Pop 2. Decrement in-degree of 3 (now 0, enqueue 3). Queue = [3]. Order = [0, 1, 2]
Step 5: Pop 3. Queue = []. Order = [0, 1, 2, 3]
Result: [0, 1, 2, 3] (or [0, 2, 1, 3])
```

```
HotSpot Memory Layout:
int[] inDegree: 4 bytes per node
List<List<Integer>> adj: contiguous reference arrays
Queue<Integer> queue: ArrayDeque primitive circular buffer
```

---

## 4. Syntax & Usage

### Production Kahn's Algorithm Implementation

```java
import java.util.*;

public class KahnTopologicalSort {
    public static List<Integer> sort(int numNodes, List<List<Integer>> adj) {
        int[] inDegree = new int[numNodes];
        for (int u = 0; u < numNodes; u++) {
            for (int v : adj.get(u)) {
                inDegree[v]++;
            }
        }

        Queue<Integer> queue = new ArrayDeque<>();
        for (int i = 0; i < numNodes; i++) {
            if (inDegree[i] == 0) {
                queue.offer(i);
            }
        }

        List<Integer> order = new ArrayList<>(numNodes);
        while (!queue.isEmpty()) {
            int u = queue.poll();
            order.add(u);

            for (int v : adj.get(u)) {
                if (--inDegree[v] == 0) {
                    queue.offer(v);
                }
            }
        }

        if (order.size() != numNodes) {
            throw new IllegalStateException("Graph contains at least one cycle; topological sort impossible.");
        }

        return order;
    }
}
```

---

## 5. Practical Examples

### Example 1: Course Schedule I (Cycle Detection) — LeetCode 207

```java
import java.util.*;

public class CourseSchedule {
    public boolean canFinish(int numCourses, int[][] prerequisites) {
        List<List<Integer>> adj = new ArrayList<>(numCourses);
        for (int i = 0; i < numCourses; i++) adj.add(new ArrayList<>());
        int[] inDegree = new int[numCourses];

        for (int[] p : prerequisites) {
            int course = p[0];
            int pre = p[1];
            adj.get(pre).add(course);
            inDegree[course]++;
        }

        Queue<Integer> queue = new ArrayDeque<>();
        for (int i = 0; i < numCourses; i++) {
            if (inDegree[i] == 0) queue.offer(i);
        }

        int processed = 0;
        while (!queue.isEmpty()) {
            int curr = queue.poll();
            processed++;

            for (int next : adj.get(curr)) {
                if (--inDegree[next] == 0) {
                    queue.offer(next);
                }
            }
        }

        return processed == numCourses;
    }
}
```

### Example 2: Course Schedule II (Ordering Output) — LeetCode 210

```java
import java.util.*;

public class CourseScheduleII {
    public int[] findOrder(int numCourses, int[][] prerequisites) {
        List<List<Integer>> adj = new ArrayList<>(numCourses);
        for (int i = 0; i < numCourses; i++) adj.add(new ArrayList<>());
        int[] inDegree = new int[numCourses];

        for (int[] p : prerequisites) {
            adj.get(p[1]).add(p[0]);
            inDegree[p[0]]++;
        }

        Queue<Integer> queue = new ArrayDeque<>();
        for (int i = 0; i < numCourses; i++) {
            if (inDegree[i] == 0) queue.offer(i);
        }

        int[] order = new int[numCourses];
        int idx = 0;

        while (!queue.isEmpty()) {
            int curr = queue.poll();
            order[idx++] = curr;

            for (int neighbor : adj.get(curr)) {
                if (--inDegree[neighbor] == 0) {
                    queue.offer(neighbor);
                }
            }
        }

        return (idx == numCourses) ? order : new int[0];
    }
}
```

### Example 3: Alien Dictionary (Lexicographical Character Ordering) — LeetCode 269

```java
import java.util.*;

public class AlienDictionary {
    public String alienOrder(String[] words) {
        Map<Character, Set<Character>> adj = new HashMap<>();
        Map<Character, Integer> inDegree = new HashMap<>();

        for (String w : words) {
            for (char c : w.toCharArray()) {
                inDegree.putIfAbsent(c, 0);
                adj.putIfAbsent(c, new HashSet<>());
            }
        }

        for (int i = 0; i < words.length - 1; i++) {
            String w1 = words[i], w2 = words[i + 1];
            // Invalid prefix rule: "abc" before "ab" is invalid
            if (w1.length() > w2.length() && w1.startsWith(w2)) {
                return "";
            }

            for (int j = 0; j < Math.min(w1.length(), w2.length()); j++) {
                char c1 = w1.charAt(j), c2 = w2.charAt(j);
                if (c1 != c2) {
                    if (adj.get(c1).add(c2)) {
                        inDegree.put(c2, inDegree.get(c2) + 1);
                    }
                    break;
                }
            }
        }

        Queue<Character> queue = new ArrayDeque<>();
        for (char c : inDegree.keySet()) {
            if (inDegree.get(c) == 0) queue.offer(c);
        }

        StringBuilder sb = new StringBuilder();
        while (!queue.isEmpty()) {
            char curr = queue.poll();
            sb.append(curr);

            for (char next : adj.get(curr)) {
                inDegree.put(next, inDegree.get(next) - 1);
                if (inDegree.get(next) == 0) {
                    queue.offer(next);
                }
            }
        }

        return sb.length() == inDegree.size() ? sb.toString() : "";
    }
}
```

### Example 4: Longest Path in a DAG (Dynamic Programming over Topo Order)

```java
import java.util.*;

public class LongestPathInDAG {
    public record Edge(int to, int weight) {}

    public int[] findLongestPaths(int n, List<List<Edge>> adj, int start) {
        int[] inDegree = new int[n];
        for (int u = 0; u < n; u++) {
            for (Edge e : adj.get(u)) inDegree[e.to()]++;
        }

        Queue<Integer> queue = new ArrayDeque<>();
        for (int i = 0; i < n; i++) {
            if (inDegree[i] == 0) queue.offer(i);
        }

        int[] dist = new int[n];
        Arrays.fill(dist, Integer.MIN_VALUE);
        dist[start] = 0;

        while (!queue.isEmpty()) {
            int u = queue.poll();
            for (Edge e : adj.get(u)) {
                if (dist[u] != Integer.MIN_VALUE) {
                    dist[e.to()] = Math.max(dist[e.to()], dist[u] + e.weight());
                }
                if (--inDegree[e.to()] == 0) {
                    queue.offer(e.to());
                }
            }
        }

        return dist;
    }
}
```

### Example 5: Minimum Height Trees (LeetCode 310, Inward Topological Peeling)

```java
import java.util.*;

public class MinimumHeightTrees {
    public List<Integer> findMinHeightTrees(int n, int[][] edges) {
        if (n == 1) return List.of(0);

        List<Set<Integer>> adj = new ArrayList<>(n);
        for (int i = 0; i < n; i++) adj.add(new HashSet<>());
        for (int[] e : edges) {
            adj.get(e[0]).add(e[1]);
            adj.get(e[1]).add(e[0]);
        }

        List<Integer> leaves = new ArrayList<>();
        for (int i = 0; i < n; i++) {
            if (adj.get(i).size() == 1) leaves.add(i);
        }

        int remainingNodes = n;
        while (remainingNodes > 2) {
            remainingNodes -= leaves.size();
            List<Integer> newLeaves = new ArrayList<>();

            for (int leaf : leaves) {
                int neighbor = adj.get(leaf).iterator().next();
                adj.get(neighbor).remove(leaf);
                if (adj.get(neighbor).size() == 1) {
                    newLeaves.add(neighbor);
                }
            }
            leaves = newLeaves;
        }

        return leaves;
    }
}
```

---

## 6. Best Practices

1. **Prefer Kahn's Algorithm in Production**: Kahn's algorithm is iterative, avoids recursion stack limits, naturally detects cycles by comparing `order.size() == V`, and allows finding lexicographically smallest topological orders using a `PriorityQueue`.
2. **Handle Multiple Valid Orderings**: A DAG can have multiple valid topological orderings. If a deterministic sequence is required, replace the `ArrayDeque` with a `PriorityQueue`.
3. **Verify Acyclicity**: Always check that all vertices have been processed. Never assume inputs are cycle-free unless proven.
4. **Use Flat Arrays for Primitive IDs**: Use `int[] inDegree` rather than `Map<Integer, Integer>` whenever vertex IDs are dense integers $0 \le i < V$.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Inverting edge direction | Yields reverse topological order | Edge must go from dependency (prerequisite) to dependent. |
| Forgetting cycle check | Silent omission of nodes in cyclical graphs | Verify `processedCount == V` at completion. |
| Recursive DFS without 3 states | Cannot differentiate between cross-edges and true cycles | Use 0=Unvisited, 1=Visiting, 2=Visited. |
| Prefix edge cases in Alien Dictionary | `"apple"` before `"app"` violates dictionary rules | Check for illegal prefix truncation before building graph. |

---

## 8. Performance & Security Notes

- **Time Complexity**: $O(V + E)$ where $V$ is vertices and $E$ is edges.
- **Space Complexity**: $O(V + E)$ for adjacency list, $O(V)$ for queue and in-degree array.
- **Deterministic Complexity**: When using a `PriorityQueue` for lexicographical ordering, time increases to $O(E + V \log V)$.

---

## 9. Comparison / When to Use What

| Feature | Kahn's Algorithm (BFS) | DFS Post-Order |
|---|---|---|
| **Mechanism** | In-degree queue | Recursion / Stack |
| **Cycle Detection** | Count of ordered nodes $< V$ | Gray node encounter |
| **Lexicographical Ordering** | Trivial (use `PriorityQueue`) | Complex |
| **Call Stack Risk** | None ($O(1)$ stack) | High if recursion depth $> 10^4$ |

---

## 10. Interview Questions

<details>
<summary>1. Why can a graph with cycles never have a topological sort?</summary>

In a topological sort, if $u \to v$, $u$ must appear before $v$. In a cycle $u \to v \to w \to u$, $u$ must appear before $v$, $v$ before $w$, and $w$ before $u$. This forms a logical contradiction: $u$ must appear strictly before itself, which is impossible.
</details>

<details>
<summary>2. How can you find the lexicographically smallest topological sort?</summary>

Use Kahn's algorithm, but replace the standard FIFO queue (`ArrayDeque`) with a Min-Heap (`PriorityQueue<Integer>`). Whenever multiple vertices have in-degree 0, the priority queue greedily chooses the smallest node ID first.
</details>

<details>
<summary>3. Can a DAG have more than one valid topological sort?</summary>

Yes. Any time the queue in Kahn's algorithm contains two or more vertices simultaneously, those vertices have no mutual dependencies and can appear in any relative order, yielding multiple valid topological sorts.
</details>

<details>
<summary>4. How does topological sort enable finding shortest/longest paths in DAGs in $O(V+E)$ time?</summary>

Because vertices are visited in topological order, by the time we process vertex $u$, all possible paths leading into $u$ have already been fully evaluated. We can relax its outgoing edges in a single linear pass without revisiting nodes, bypassing the need for Dijkstra's heap ($O((V+E)\log V)$).
</details>

<details>
<summary>5. What is the relation between Topological Sort and Strongly Connected Components (Kosaraju / Tarjan)?</summary>

The condensation graph of a directed graph (where each strongly connected component is contracted into a single super-node) is always a DAG. Running topological sort on the condensation graph defines the component dependency ordering.
</details>

<details>
<summary>6. How do build tools (e.g. Maven, Gradle) execute parallel task compilation using topological sort?</summary>

In Kahn's algorithm, all nodes in the queue with in-degree 0 at any given stage have all dependencies satisfied and can be executed concurrently on separate worker threads. When tasks complete, their dependent edges are decremented; newly unblocked nodes are pushed into the worker pool.
</details>

<details>
<summary>7. What happens if a graph is disconnected?</summary>

Kahn's algorithm handles disconnected components seamlessly: all nodes with in-degree 0 across all components are enqueued at step 1. DFS handles it by looping across all unvisited vertices $0 \dots V-1$.
</details>

<details>
<summary>8. How do you find if a unique topological sort exists?</summary>

A topological sort is unique if and only if at every step of Kahn's algorithm, the queue contains **exactly one element**. If the queue ever contains more than one element, multiple valid orderings exist.
</details>

---

## 11. Practice Exercises

### Exercise 1: Sequence Reconstruction (LeetCode 444)
Check whether the original sequence `nums` is the only sequence that can be reconstructed from given subsequence patterns.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class SequenceReconstruction {
    public boolean sequenceReconstruction(int[] nums, List<List<Integer>> sequences) {
        int n = nums.length;
        List<Set<Integer>> adj = new ArrayList<>(n + 1);
        for (int i = 0; i <= n; i++) adj.add(new HashSet<>());
        int[] inDegree = new int[n + 1];

        for (List<Integer> seq : sequences) {
            for (int i = 0; i < seq.size() - 1; i++) {
                int u = seq.get(i), v = seq.get(i + 1);
                if (adj.get(u).add(v)) {
                    inDegree[v]++;
                }
            }
        }

        Queue<Integer> queue = new ArrayDeque<>();
        for (int i = 1; i <= n; i++) {
            if (inDegree[i] == 0) queue.offer(i);
        }

        int idx = 0;
        while (!queue.isEmpty()) {
            // Must have strictly 1 candidate at each step for unique sequence!
            if (queue.size() > 1) return false;
            int curr = queue.poll();
            if (nums[idx++] != curr) return false;

            for (int next : adj.get(curr)) {
                if (--inDegree[next] == 0) {
                    queue.offer(next);
                }
            }
        }

        return idx == n;
    }
}
```
</details>

### Exercise 2: Sort Items by Groups Respecting Dependencies (LeetCode 1203)
Solve dual-level topological sort where items belong to groups and dependencies exist between items and between groups.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class SortItemsByGroups {
    public int[] sortItems(int n, int m, int[] group, List<List<Integer>> beforeItems) {
        for (int i = 0; i < n; i++) {
            if (group[i] == -1) group[i] = m++;
        }

        List<List<Integer>> itemAdj = new ArrayList<>(n);
        int[] itemInDegree = new int[n];
        for (int i = 0; i < n; i++) itemAdj.add(new ArrayList<>());

        List<List<Integer>> groupAdj = new ArrayList<>(m);
        int[] groupInDegree = new int[m];
        for (int i = 0; i < m; i++) groupAdj.add(new ArrayList<>());

        for (int curr = 0; curr < n; curr++) {
            for (int prev : beforeItems.get(curr)) {
                itemAdj.get(prev).add(curr);
                itemInDegree[curr]++;

                if (group[prev] != group[curr]) {
                    groupAdj.get(group[prev]).add(group[curr]);
                    groupInDegree[group[curr]]++;
                }
            }
        }

        List<Integer> itemOrder = topoSort(itemAdj, itemInDegree, n);
        List<Integer> groupOrder = topoSort(groupAdj, groupInDegree, m);

        if (itemOrder.isEmpty() || groupOrder.isEmpty()) return new int[0];

        Map<Integer, List<Integer>> groupToItems = new HashMap<>();
        for (int item : itemOrder) {
            groupToItems.computeIfAbsent(group[item], k -> new ArrayList<>()).add(item);
        }

        int[] result = new int[n];
        int idx = 0;
        for (int grp : groupOrder) {
            List<Integer> items = groupToItems.get(grp);
            if (items != null) {
                for (int item : items) result[idx++] = item;
            }
        }

        return result;
    }

    private List<Integer> topoSort(List<List<Integer>> adj, int[] inDegree, int count) {
        Queue<Integer> queue = new ArrayDeque<>();
        for (int i = 0; i < count; i++) {
            if (inDegree[i] == 0) queue.offer(i);
        }
        List<Integer> order = new ArrayList<>(count);
        while (!queue.isEmpty()) {
            int u = queue.poll();
            order.add(u);
            for (int v : adj.get(u)) {
                if (--inDegree[v] == 0) queue.offer(v);
            }
        }
        return order.size() == count ? order : Collections.emptyList();
    }
}
```
</details>

### Exercise 3: Parallel Courses (LeetCode 1136)
Find the minimum number of semesters needed to take all courses, taking as many courses in parallel as possible.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class ParallelCourses {
    public int minimumSemesters(int n, int[][] relations) {
        List<List<Integer>> adj = new ArrayList<>(n + 1);
        for (int i = 0; i <= n; i++) adj.add(new ArrayList<>());
        int[] inDegree = new int[n + 1];

        for (int[] r : relations) {
            adj.get(r[0]).add(r[1]);
            inDegree[r[1]]++;
        }

        Queue<Integer> queue = new ArrayDeque<>();
        for (int i = 1; i <= n; i++) {
            if (inDegree[i] == 0) queue.offer(i);
        }

        int semesters = 0;
        int completed = 0;

        while (!queue.isEmpty()) {
            int size = queue.size();
            semesters++;

            for (int i = 0; i < size; i++) {
                int u = queue.poll();
                completed++;
                for (int v : adj.get(u)) {
                    if (--inDegree[v] == 0) {
                        queue.offer(v);
                    }
                }
            }
        }

        return completed == n ? semesters : -1;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Condition**: Topological sort requires a Directed Acyclic Graph (DAG).
- **Kahn's BFS**: Track in-degrees, enqueue zeros, decrement on edge removal. $O(V + E)$.
- **Cycle Detection**: If Kahn's sorted count $< V$, a cycle exists.
- **Level Processing**: Processing Kahn's queue level-by-level gives the minimum parallel execution steps.
- **DAG DP**: Topological ordering enables $O(V+E)$ single-source shortest/longest path calculations.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 22.4: Topological Sort](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Kahn, A. B. (1962). Topological sorting of large networks](https://dl.acm.org/doi/10.1145/368996.369025)
- [Baeldung: Guide to Topological Sorting in Java](https://www.baeldung.com/cs/dag-topological-sort)
- [LeetCode Topological Sort Study List](https://leetcode.com/tag/topological-sort/)
