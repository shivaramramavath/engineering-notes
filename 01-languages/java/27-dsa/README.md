# Module 27: Data Structures & Algorithms (Java)

> Comprehensive, production-grade Data Structures and Algorithms curriculum written in modern Java (Java 21–25 LTS). Every note features theoretical foundations, JVM memory layouts, hot-path benchmarks, 5+ LeetCode implementations, pitfall tables, interview Q&A with hidden solutions, and further reading.

---

## Track Overview

This module provides exhaustive coverage of fundamental and advanced data structures, algorithmic design patterns, and interview mastery. Every implementation is verified against Oracle JDK 25 LTS, utilizing modern features such as Records, Pattern Matching, and optimized collection primitives.

---

## Curriculum Table of Contents

| # | Note | Level | Topics Covered | Lines |
|---|---|---|---|---|
| **00** | [00_complexity-analysis.md](00_complexity-analysis.md) | Intermediate | Big-O, Big-$\Omega$, Big-$\Theta$, Master Theorem, Amortized Analysis | 400+ |
| **01** | [01_arrays-and-strings.md](01_arrays-and-strings.md) | Intermediate | Contiguous Memory, Compact Strings, Sliding Window, Two Pointers | 400+ |
| **02** | [02_linked-lists.md](02_linked-lists.md) | Intermediate | Singly/Doubly Linked Lists, Fast & Slow Pointers, In-Place Reversals | 500+ |
| **03** | [03_stacks-and-queues.md](03_stacks-and-queues.md) | Intermediate | `ArrayDeque` Internals, Monotonic Stacks, Sliding Window Maximum | 400+ |
| **04** | [04_hash-tables-and-hashing.md](04_hash-tables-and-hashing.md) | Advanced | HashMap Internals, Treeification, Open Addressing, Collision DoS | 470+ |
| **05** | [05_recursion-and-backtracking.md](05_recursion-and-backtracking.md) | Intermediate | Call Stack Frames, State Pruning, N-Queens, Sudoku Solver | 440+ |
| **06** | [06_binary-search.md](06_binary-search.md) | Intermediate | Left/Right Bounds, Answer-Space Search, Rotated Arrays | 420+ |
| **07** | [07_trees-binary-and-bst.md](07_trees-binary-and-bst.md) | Intermediate | Traversals, Tree Reconstruction, LCA, BST Invariants | 400+ |
| **08** | [08_self-balancing-trees.md](08_self-balancing-trees.md) | Advanced | AVL Rotations, Red-Black Trees, `TreeMap` Internals | 400+ |
| **09** | [09_trie.md](09_trie.md) | Intermediate | Prefix Trees, Wildcard Search, Bitwise Trie, Autocomplete | 660+ |
| **10** | [10_heaps-and-priority-queues.md](10_heaps-and-priority-queues.md) | Intermediate | Min/Max Binary Heap, $O(N)$ BuildHeap, MedianFinder, Top-K | 540+ |
| **11** | [11_graphs-bfs-dfs.md](11_graphs-bfs-dfs.md) | Intermediate | Adjacency Lists, Shortest Path BFS, 3-Coloring Cycle DFS, Bipartite | 630+ |
| **12** | [12_topological-sort.md](12_topological-sort.md) | Intermediate | Kahn's In-Degree BFS, Post-Order DFS, DAG Cycle Check, Alien Dict | 650+ |
| **13** | [13_shortest-path-algorithms.md](13_shortest-path-algorithms.md) | Advanced | Dijkstra with PriorityQueue, Bellman-Ford Cycles, Floyd-Warshall | 620+ |
| **14** | [14_minimum-spanning-tree.md](14_minimum-spanning-tree.md) | Advanced | Kruskal's with DSU, Prim's Algorithm, Cut Property, Virtual Nodes | 690+ |
| **15** | [15_disjoint-set-union.md](15_disjoint-set-union.md) | Intermediate | Path Compression, Union by Rank, Inverse Ackermann $\alpha(N)$ | 620+ |
| **16** | [16_dynamic-programming-1d.md](16_dynamic-programming-1d.md) | Intermediate | Memoization vs Tabulation, Rolling Space $O(1)$, Patience Sort LIS | 470+ |
| **17** | [17_dynamic-programming-2d.md](17_dynamic-programming-2d.md) | Advanced | Grid Paths, LCS, Edit Distance, Maximal Square, 1D Compression | 520+ |
| **18** | [18_dynamic-programming-advanced.md](18_dynamic-programming-advanced.md) | Advanced | Bitmask DP, Tree DP, Interval DP, Digit DP, Submask Loops | 520+ |
| **19** | [19_greedy-algorithms.md](19_greedy-algorithms.md) | Intermediate | Exchange Arguments, Interval Scheduling, Two-Pass Greedy, Gas Station | 490+ |
| **20** | [20_bit-manipulation.md](20_bit-manipulation.md) | Intermediate | Two's Complement, Brian Kernighan, Single Number I/II/III, Intrinsics | 420+ |
| **21** | [21_math-and-number-theory.md](21_math-and-number-theory.md) | Intermediate | Euclidean GCD, Sieve of Eratosthenes, Fast Modular Pow, Fermat Inv | 510+ |
| **22** | [22_string-algorithms.md](22_string-algorithms.md) | Advanced | KMP Algorithm, Rabin-Karp Rolling Hash, Manacher's Palindromes | 580+ |
| **23** | [23_segment-trees-and-fenwick.md](23_segment-trees-and-fenwick.md) | Advanced | Binary Indexed Tree (BIT), Range Queries, Lazy Propagation $O(\log N)$ | 670+ |
| **24** | [24_intervals.md](24_intervals.md) | Intermediate | Overlapping Intervals, Meeting Rooms II, Chronological Sweep-Line | 500+ |
| **25** | [25_matrix-and-grid-algorithms.md](25_matrix-and-grid-algorithms.md) | Intermediate | In-Place Rotations, Spiral Order, Staircase Search, Row-Major Locality | 530+ |
| **26** | [26_dsa-patterns-index.md](26_dsa-patterns-index.md) | Advanced | Master Taxonomy of 16 Core Patterns, Complexity Matrix, Decision Flow | 430+ |
| **27** | [27_leetcode-patterns-mastery.md](27_leetcode-patterns-mastery.md) | Advanced | Blind 75 & NeetCode 150 Taxonomies, 45-Min Interview Budget, LRU Cache | 540+ |

---

## Pedagogical Progression

```
[Phase 1: Fundamentals & Primitives]
  00 Complexity -> 01 Arrays & Strings -> 02 Linked Lists -> 03 Stacks & Queues -> 04 Hash Tables

[Phase 2: Recursion & Trees]
  05 Recursion & Backtracking -> 06 Binary Search -> 07 Trees & BST -> 08 Balanced Trees -> 09 Trie -> 10 Heaps

[Phase 3: Graphs & Connectivity]
  11 BFS & DFS -> 12 Topological Sort -> 13 Shortest Paths -> 14 MST -> 15 Disjoint Set Union (DSU)

[Phase 4: Dynamic Programming & Optimization]
  16 1D DP -> 17 2D DP -> 18 Advanced DP (Bitmask/Tree/Interval/Digit) -> 19 Greedy

[Phase 5: Advanced Computation & Geometry]
  20 Bit Manipulation -> 21 Math & Number Theory -> 22 String Algorithms -> 23 Segment & Fenwick Trees -> 24 Intervals -> 25 Matrix & Grid

[Phase 6: Interview Mastery & System Synthesis]
  26 DSA Patterns Index (16 Master Archetypes) -> 27 LeetCode Patterns Mastery (Blind 75 & NeetCode 150)
```

---

## Verification & Modern Standards

All code examples in this module adhere strictly to modern Java best practices:
- **Zero Raw Types**: Full generic safety (`PriorityQueue<T>`, `List<T>`).
- **Data Modeling**: Immutable domain carriers utilize Java Records (`record Edge(...)`).
- **Memory Efficiency**: Contiguous primitive arrays (`int[]`) and hardware-accelerated intrinsics (`Integer.bitCount()`) are prioritized over boxed objects.
- **Verification Toolchain**: Tested and compiled against Oracle JDK 25 LTS with `-Xlint:all`.
