# Sub-Module: Data Structures in C

> Production-grade, cache-conscious, memory-safe data structures implemented from first principles in standard C: linear containers, trees, hash maps, heaps, and graphs.

| Metadata | Details |
| :--- | :--- |
| **Module Code** | `03-intermediate/data-structures` |
| **Prerequisites** | [02-core-concepts/03-pointers.md](../../02-core-concepts/03-pointers.md), [02-core-concepts/08-memory-management.md](../../02-core-concepts/08-memory-management.md), [03-intermediate/13-opaque-pointers-and-adts.md](../13-opaque-pointers-and-adts.md) |
| **Duration** | ~20 hours |
| **Status** | Complete (Batches 14–16) |
| **Standard Baseline**| ISO C11 / C17 / C23 |

---

## Overview & Learning Outcomes
Data structures in C are fundamentally different from collections in managed languages (like Java or Python). Without a garbage collector, built-in dynamic arrays, or automatic hash tables, systems engineers must construct every container from raw memory buffers and pointers.

In this module, you will master:
1. **Dynamic Memory Layouts**: Contiguous array buffers vs node-based pointer graphs, evaluating CPU cache locality and pointer chasing overhead.
2. **Abstract Data Type (ADT) Encapsulation**: Opaque struct handles (`typedef struct DynamicArray DynamicArray;`) that hide implementation details.
3. **Algorithmic Mechanics**: Singly and doubly linked lists, circular ring buffers, min/max heaps, binary search trees (AVL/Red-Black), and hash tables with open addressing and separate chaining.
4. **Memory Safety & Defensive Allocation**: Handling `realloc()` failure without memory leaks, sentinel nodes, and cycle detection.

---

## Module Table of Contents

| Note | Title | Level | Status | Focus Areas |
| :--- | :--- | :--- | :--- | :--- |
| **01** | [Linked Lists](./01-linked-lists.md) | Intermediate | Complete | Singly, doubly, circular, sentinel nodes, reverse, cycle detection |
| **02** | [Stacks & Queues](./02-stacks-queues.md) | Intermediate | Complete | Array-based and node-based LIFO/FIFO implementations |
| **03** | [Trees](./03-trees.md) | Intermediate | Complete | Binary trees, traversals (pre/in/post/level), recursive vs iterative |
| **04** | [Hash Tables](./04-hash-tables.md) | Intermediate | Complete | MurmurHash3/djb2, separate chaining, open addressing (linear probing) |
| **05** | [Graphs](./05-graphs.md) | Intermediate | Complete | Adjacency list vs matrix, BFS, DFS, memory representation |
| **06** | [Dynamic Array](./06-dynamic-array.md) | Intermediate | Complete | Growth factor, amortized $O(1)$ push, safe `realloc`, generic vector |
| **07** | [Queues & Circular Buffers](./07-queues-circular-buffer.md) | Intermediate | Complete | Fixed-size ring buffer for embedded systems & lock-free queues |
| **08** | [Heaps & Priority Queues](./08-heaps-and-priority-queues.md) | Intermediate | Complete | Binary min/max heap, sift-up, sift-down, `heapify`, priority queue |
| **09** | [Binary Search Trees](./09-binary-search-trees.md) | Intermediate | Complete | BST insertion, deletion, search, AVL rotation, Red-Black theory |
| **10** | [Tries](./10-tries.md) | Intermediate | Complete | Prefix tree for fast dictionary lookups and IP routing |
| **11** | [Union-Find](./11-union-find.md) | Intermediate | Complete | Disjoint-set data structure, path compression, union by rank |
| **12** | [Sorting Algorithms](./12-sorting-algorithms.md) | Intermediate | Complete | Quicksort, mergesort, heapsort, insertion sort, hybrid sort |
| **13** | [Searching Algorithms](./13-searching-algorithms.md) | Intermediate | Complete | Binary search, interpolation search, exponential search |
| **14** | [Algorithm Complexity](./14-algorithm-complexity.md) | Intermediate | Complete | Big-O, Big-Omega, Big-Theta, amortized analysis, cache complexity |
| **15** | [Skip Lists](./15-skip-lists.md) | Intermediate | Complete | Probabilistic balance, multi-level pointer skips, concurrent lookups |
| **16** | [Bloom Filters](./16-bloom-filters.md) | Intermediate | Complete | Space-efficient probabilistic set membership, false positive math |
| **17** | [LRU Cache Structure](./17-lru-cache-structure.md) | Intermediate | Complete | Hash table + doubly linked list $O(1)$ eviction policy |
| **18** | [Graph Algorithms](./18-graph-algorithms.md) | Intermediate | Complete | Dijkstra's shortest path, Bellman-Ford, Kruskal/Prim MST, topological sort |
| **19** | [String Algorithms](./19-string-algorithms.md) | Intermediate | Complete | KMP (Knuth-Morris-Pratt), Rabin-Karp rolling hash, Boyer-Moore |

---

## Suggested Study Order
1. **Linear Foundations**: Start with [Dynamic Array](./06-dynamic-array.md) and [Linked Lists](./01-linked-lists.md) to understand cache locality vs pointer indirection.
2. **Access-Restricted Containers**: Progress to [Stacks & Queues](./02-stacks-queues.md) and [Circular Buffers](./07-queues-circular-buffer.md).
3. **Hierarchical & Associative Structures**: Study [Trees](./03-trees.md), [Binary Search Trees](./09-binary-search-trees.md), and [Hash Tables](./04-hash-tables.md).
4. **Specialized & Advanced Structures**: Conclude with [Heaps](./08-heaps-and-priority-queues.md), [Tries](./10-tries.md), [Skip Lists](./15-skip-lists.md), [Bloom Filters](./16-bloom-filters.md), and [LRU Cache](./17-lru-cache-structure.md).

---

[← Previous Module: Intermediate C](../README.md) | [Next Module: Advanced C](../../04-advanced/README.md)
