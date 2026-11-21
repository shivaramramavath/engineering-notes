# Java Collections Framework (`08-collections`)

> Definitive architectural reference for the Java Collections Framework: lists, sets, maps, queues, deques, sequenced collections (Java 21 JEP 431), iterators, utilities, and performance mechanics.

---

## Module Overview

The Java Collections Framework (JCF) provides a unified, highly optimized architecture for storing and manipulating groups of objects. This module provides a comprehensive, deep-dive reference covering theoretical algorithmic contracts, memory layouts on 64-bit JVMs, internal data structure mechanics, and selection guidelines from Java 1.2 through Java 25 LTS.

---

## Notes Directory

| # | Note | Description | Level | Estimated Reading Time |
|---|---|---|---|---|
| 00 | [Collection Hierarchy & Sequenced Collections](00_collection-hierarchy.md) | JCF architectural blueprint, core interfaces, and modern Java 21 Sequenced Collections (JEP 431). | Intermediate | 25 min |
| 01 | [The List Interface & Implementations](01_list.md) | Ordered sequences, positional indexing, subList view mechanics, and list iterators. | Intermediate | 24 min |
| 02 | [The Set Interface & Implementations](02_set.md) | Mathematical set abstraction, uniqueness invariants, and backing map delegation. | Intermediate | 23 min |
| 03 | [The Map Interface & Implementations](03_map.md) | Associative key-value mappings, collection views, and Java 8+ functional methods. | Intermediate | 25 min |
| 04 | [Queue and Deque Interfaces](04_queue-and-deque.md) | FIFO buffers, LIFO stacks, double-ended queues, and circular ArrayDeque mechanics. | Intermediate | 22 min |
| 05 | [Comparable and Comparator](05_comparable-and-comparator.md) | Natural vs explicit ordering, fluent combinator composition, and TimSort invariants. | Intermediate | 24 min |
| 06 | [ArrayList Deep Dive](06_arraylist.md) | Dynamic 1.5x array growth, contiguous memory cache locality, and System.arraycopy. | Intermediate | 25 min |
| 07 | [LinkedList Deep Dive](07_linkedlist.md) | Doubly-linked node architecture, pointer chasing penalties, and memory footprint. | Intermediate | 22 min |
| 08 | [HashMap & HashSet Internals](08_hashmap-and-hashset.md) | Power-of-two indexing, hash spreading, treeification threshold 8, and zero-modulo rehashing. | Advanced | 26 min |
| 09 | [TreeMap & TreeSet Deep Dive](09_treemap-and-treeset.md) | Red-Black tree balancing, NavigableMap range queries, and floor/ceiling lookups. | Advanced | 25 min |
| 10 | [PriorityQueue Deep Dive](10_priorityqueue.md) | Binary min-heap array representation, sift-up/sift-down, and linear-time heapify. | Intermediate | 24 min |
| 11 | [Specialized Collections](11_specialized-collections.md) | EnumSet bitmasks, EnumMap flat arrays, IdentityHashMap, WeakHashMap, and BitSet. | Advanced | 25 min |
| 12 | [Iterators & Fail-Fast Mechanics](12_iterators-and-fail-fast-behavior.md) | Iterator, ListIterator, Spliterator, and modCount fail-fast invariants. | Intermediate | 24 min |
| 13 | [Immutable & Unmodifiable Collections](13_immutable-and-unmodifiable-collections.md) | Unmodifiable views vs true immutable collections (`List.of`, `copyOf`) and randomized order. | Intermediate | 23 min |
| 14 | [Collections & Arrays Utilities](14_collections-and-arrays-utilities.md) | TimSort vs Dual-Pivot Quicksort, binary search mechanics, and vectorized mismatch. | Intermediate | 24 min |
| 15 | [Collection Performance & Selection](15_collection-performance-and-selection.md) | Big-O time/space matrices, memory overhead per element, and architectural decision trees. | Advanced | 25 min |

---

## Core Principles Quick Reference

### The Golden Rules of JCF Selection
1. **Default to `ArrayList`** for lists. It has superior CPU cache line locality and zero per-node pointer overhead.
2. **Default to `ArrayDeque`** for stacks and queues. It completely replaces `java.util.Stack` and outperforms `LinkedList`.
3. **Default to `HashMap`** for key-value dictionaries. Fast $O(1)$ amortized lookups.
4. **Default to `HashSet`** for uniqueness and membership testing.
5. **Use `EnumSet` and `EnumMap`** whenever keys are enums. They execute with single-cycle bitwise operations.
6. **Use `TreeSet` and `TreeMap`** only when elements must be sorted or range queries are required.
7. **Use `ConcurrentHashMap`** for thread-safe concurrent reads and writes.

---

## Prerequisites
- [04-oop](../04-oop/README.md) - Object-Oriented Programming (Classes, Interfaces, Polymorphism, equals/hashCode)
- [07-generics](../07-generics/README.md) - Generics and PECS Wildcard Variance
