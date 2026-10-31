# Graphs

A graph is a set of **vertices** (nodes) connected by **edges**. Graphs model maps, networks, dependencies, and state machines. In C the main design decision is the representation; the two core algorithms are breadth-first and depth-first search.

**Prerequisites:** [Linked lists](./01-linked-lists.md), [stacks and queues](./02-stacks-queues.md), [dynamic memory](../../02-core-concepts/08-memory-management.md).

## Vocabulary

| Term | Meaning |
|---|---|
| Directed / undirected | edges have a direction / go both ways |
| Weighted | each edge has a cost |
| Degree | number of edges at a vertex |
| Path / cycle | a walk along edges / a path that returns to its start |
| Connected | every vertex reachable from every other (undirected) |
| DAG | directed acyclic graph; supports topological ordering |
| Sparse / dense | few edges (E ≈ V) / many (E ≈ V²) |

```text
0 ──▶ 1 ──▶ 3
│     ▲
▼     │
2 ────┘
```

## Representations

| | Adjacency list | Adjacency matrix | Edge list |
|---|---|---|---|
| Memory | O(V + E) | O(V²) | O(E) |
| Is there an edge u→v? | O(deg u) | **O(1)** | O(E) |
| Iterate neighbors of u | **O(deg u)** | O(V) | O(E) |
| Best for | sparse graphs, traversals | dense graphs, quick edge tests | sorting edges (Kruskal) |

An adjacency matrix is a 2D array (`bool adj[V][V]`, or one flat allocation `adj[u * V + v]`).

### Adjacency list

```c
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct edge {
    int          to;
    struct edge *next;
} edge_t;

typedef struct {
    int      n;           /* vertices are numbered 0 .. n-1 */
    edge_t **adj;         /* adj[u] = head of u's neighbor list */
} graph_t;

graph_t *graph_new(int n) {
    graph_t *g = malloc(sizeof *g);
    if (g == NULL) return NULL;
    g->adj = calloc((size_t)n, sizeof *g->adj);
    if (g->adj == NULL) { free(g); return NULL; }
    g->n = n;
    return g;
}

/* Directed edge u → v. For an undirected graph, add both directions. */
bool graph_add_edge(graph_t *g, int u, int v) {
    edge_t *e = malloc(sizeof *e);
    if (e == NULL) return false;
    e->to = v;
    e->next = g->adj[u];
    g->adj[u] = e;
    return true;
}

void graph_free(graph_t *g) {
    if (g == NULL) return;
    for (int u = 0; u < g->n; u++) {
        edge_t *e = g->adj[u];
        while (e != NULL) { edge_t *next = e->next; free(e); e = next; }
    }
    free(g->adj);
    free(g);
}
```

For weighted graphs add a `weight` field to `edge_t`. Check that `u` and `v` are in range before indexing; out-of-range vertices are a common source of memory errors.

## Breadth-first search (BFS)

Visits vertices in order of distance (edge count) from the start. It uses a [queue](./02-stacks-queues.md) and finds **shortest paths in unweighted graphs**.

```c
void bfs(const graph_t *g, int start) {
    bool *seen  = calloc((size_t)g->n, sizeof *seen);
    int  *queue = malloc((size_t)g->n * sizeof *queue);
    if (seen == NULL || queue == NULL) { free(seen); free(queue); return; }

    size_t head = 0, tail = 0;
    seen[start] = true;
    queue[tail++] = start;                    /* each vertex is enqueued at most once → n slots suffice */

    while (head < tail) {
        int u = queue[head++];
        printf("%d ", u);
        for (const edge_t *e = g->adj[u]; e != NULL; e = e->next) {
            if (!seen[e->to]) {
                seen[e->to] = true;           /* mark when enqueuing, not when dequeuing */
                queue[tail++] = e->to;
            }
        }
    }
    putchar('\n');
    free(seen);
    free(queue);
}
```

To get distances or the actual path, also keep `dist[]` and `parent[]` arrays, set when a vertex is first discovered.

## Depth-first search (DFS)

Goes as deep as possible before backtracking. It uses a stack, either the call stack (recursion) or an explicit one.

```c
static void dfs_visit(const graph_t *g, int u, bool *seen) {
    seen[u] = true;
    printf("%d ", u);
    for (const edge_t *e = g->adj[u]; e != NULL; e = e->next) {
        if (!seen[e->to]) dfs_visit(g, e->to, seen);
    }
}

void dfs(const graph_t *g, int start) {
    bool *seen = calloc((size_t)g->n, sizeof *seen);
    if (seen == NULL) return;
    dfs_visit(g, start, seen);
    putchar('\n');
    free(seen);
}
```

Recursion depth can reach V on a long path, which risks a stack overflow on big graphs; switch to an explicit [stack](./02-stacks-queues.md) for large inputs.

## What to use each for

| Task | Tool |
|---|---|
| Shortest path (unweighted) | BFS |
| Reachability, connected components | BFS or DFS |
| Cycle detection | DFS (look for a back edge) |
| Topological sort (build order, dependencies) | DFS post-order reversed, or Kahn's algorithm (in-degree queue) |
| Shortest path (non-negative weights) | Dijkstra: needs a priority queue ([heap](./03-trees.md)) |
| Shortest path with negative weights | Bellman-Ford |
| Minimum spanning tree | Kruskal (sort edges + union-find) or Prim |

## Complexity

| Algorithm | Time | Extra space |
|---|---|---|
| BFS / DFS (adjacency list) | O(V + E) | O(V) |
| BFS / DFS (adjacency matrix) | O(V²) | O(V) |
| Dijkstra with a binary heap | O((V + E) log V) | O(V) |

## Common mistakes

| Mistake | Effect |
|---|---|
| Not tracking visited vertices | Infinite loop on cycles |
| Marking visited on dequeue in BFS | Vertices enqueued many times; the queue overflows |
| Indexing with an unchecked vertex id | Out-of-bounds access |
| Forgetting both directions in an undirected graph | Missing edges |
| Deep recursive DFS on large graphs | Stack overflow |
| Freeing the vertex array before walking each edge list | Leaks every edge |
| Using an `n × n` matrix for huge sparse graphs | Gigabytes of memory |

## Summary

- Choose adjacency lists for sparse graphs, matrices for dense ones.
- BFS (queue) gives shortest unweighted paths; DFS (stack/recursion) explores deeply and detects cycles.
- Track visited vertices, validate ids, and watch recursion depth.

**Next:** [Standard library overview](../11-standard-library.md)
