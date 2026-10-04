# 08 - Collections

The **Java Collections Framework** is a set of interfaces and implementations for storing and manipulating groups of objects: lists, sets, maps, queues. After strings and arrays, it is the most-used part of the standard library, and the source of many interview questions and performance bugs.

```
 interfaces (WHAT it promises)             implementations (HOW it does it)
 ─────────────────────────────             ────────────────────────────────
 List   ordered, indexed, duplicates   ──► ArrayList, LinkedList
 Set    no duplicates                  ──► HashSet, LinkedHashSet, TreeSet
 Map    key → value                    ──► HashMap, LinkedHashMap, TreeMap
 Queue / Deque  processing order       ──► ArrayDeque, PriorityQueue, LinkedList
```

## How this folder is organized

Two layers, deliberately kept in separate files because you look them up for different reasons:

| Layer | Files | Question it answers |
|-------|-------|---------------------|
| **Contracts** (interfaces) | [00](./00_collection-hierarchy.md) to [04](./04_queue-and-deque.md) | "What operations exist, and what do they promise?" |
| **Ordering** | [05](./05_comparable-and-comparator.md) | "How do I define sort order?" |
| **Implementations** (internals) | [06](./06_arraylist.md) to [10](./10_priorityqueue.md) | "How does it work, what does it cost, when do I pick it?" |
| **Supporting APIs** | [11](./11_specialized-collections.md) to [14](./14_collections-and-arrays-utilities.md) | "Everything else you will meet: enum collections, iterators, immutability, utilities" |
| **Decision guide** | [15](./15_collection-performance-and-selection.md) | "Which collection should I use, and what will it cost?" |

## Prerequisites

[07-generics](../07-generics/README.md) (every collection is generic), [04-oop](../04-oop/README.md) (especially [interfaces](../04-oop/09_interfaces.md) and [equals and hashCode](../04-oop/14_equals-and-hashcode.md)), and [01-fundamentals/07_arrays.md](../01-fundamentals/07_arrays.md).

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 0 | [00_collection-hierarchy.md](./00_collection-hierarchy.md) | The interface tree, Sequenced collections (Java 21), common operations |
| 1 | [01_list.md](./01_list.md) | The `List` contract, pitfalls (`remove(int)` vs `remove(Object)`), `subList` |
| 2 | [02_set.md](./02_set.md) | The `Set` contract, set algebra, how equality decides duplicates |
| 3 | [03_map.md](./03_map.md) | The `Map` contract, `merge`/`computeIfAbsent`, views, iteration |
| 4 | [04_queue-and-deque.md](./04_queue-and-deque.md) | Queue, stack and deque operations; why not `Stack` |
| 5 | [05_comparable-and-comparator.md](./05_comparable-and-comparator.md) | Natural vs custom ordering, comparator chains, sort contracts |
| 6 | [06_arraylist.md](./06_arraylist.md) | Backing array, growth, costs |
| 7 | [07_linkedlist.md](./07_linkedlist.md) | Doubly linked nodes, costs, why it is rarely the right choice |
| 8 | [08_hashmap-and-hashset.md](./08_hashmap-and-hashset.md) | Hashing, buckets, resizing, treeification, `LinkedHashMap` |
| 9 | [09_treemap-and-treeset.md](./09_treemap-and-treeset.md) | Red-black trees, navigation methods |
| 10 | [10_priorityqueue.md](./10_priorityqueue.md) | Binary heap, top-K problems |
| 11 | [11_specialized-collections.md](./11_specialized-collections.md) | `EnumMap`, `EnumSet`, `LinkedHashMap` as LRU, `IdentityHashMap`, `WeakHashMap` |
| 12 | [12_iterators-and-fail-fast-behavior.md](./12_iterators-and-fail-fast-behavior.md) | `Iterator`, `ConcurrentModificationException`, safe removal |
| 13 | [13_immutable-and-unmodifiable-collections.md](./13_immutable-and-unmodifiable-collections.md) | `List.of`, `copyOf`, unmodifiable views, defensive copies |
| 14 | [14_collections-and-arrays-utilities.md](./14_collections-and-arrays-utilities.md) | `Collections`, `Arrays`, `Objects` helpers |
| 15 | [15_collection-performance-and-selection.md](./15_collection-performance-and-selection.md) | Big-O table, decision guide |

Streams and collectors build on this folder: [09-functional-java](../09-functional-java/README.md). Thread-safe collections are in [14-concurrency/08_concurrent-collections.md](../14-concurrency/08_concurrent-collections.md).

## Practice

| After file | Try |
|------------|-----|
| 01-03 | Remove duplicates from a list; count word frequencies with `merge`; group names by first letter with `computeIfAbsent` |
| 04 | Check balanced brackets with a stack; run a BFS with a queue |
| 05 | Sort people by age descending, then name; explain why `a.age - b.age` is a bug |
| 06-07 | Time `get(i)` loops on `ArrayList` vs `LinkedList` with 100,000 elements |
| 08 | Write a class used as a `HashMap` key; break it by changing `hashCode`; fix it |
| 09 | Find the floor/ceiling of a value in a `TreeSet`; build a range query on a `TreeMap` |
| 10 | Find the K largest numbers with a size-K min-heap |
| 11 | Implement an LRU cache with `LinkedHashMap` |
| 12 | Remove elements while iterating in three correct ways and one failing way |
| 13 | Make a class return a safe, unmodifiable view of its internal list |

**Project:** [26-projects/03-library-management](../26-projects/03-library-management/) uses `List`, `Map`, `Set` and sorting together.

## You are done when you can

- [ ] Choose a collection from requirements (order, duplicates, lookup, sorting) and justify it
- [ ] Explain why `HashSet` needs `equals` **and** `hashCode`, and `TreeSet` needs `compareTo`
- [ ] State the Big-O of `get`, `add`, `remove` and `contains` for `ArrayList`, `LinkedList`, `HashMap`, `TreeMap`
- [ ] Explain what `ConcurrentModificationException` means and fix it three ways
- [ ] Explain the difference between `List.of`, `Collections.unmodifiableList` and `new ArrayList<>(list)`
- [ ] Write a multi-field `Comparator` without overflow bugs
- [ ] Declare variables by interface (`List<String>`, `Map<K, V>`) rather than by class

## Key takeaways

- Program to interfaces (`List`, `Map`); pick the implementation by access pattern
- Defaults: `ArrayList`, `HashMap`, `HashSet`, `ArrayDeque`; use `Tree*` for sorted needs, `Linked*` for insertion order
- Hash-based collections depend on `equals` + `hashCode`; sorted ones on `compareTo`/`Comparator`
- Never modify a collection while iterating it (except through the iterator)
- Prefer immutable collections for data you share

**Next:** [09-functional-java](../09-functional-java/README.md)