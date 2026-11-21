# TreeMap and TreeSet Deep Dive

> Red-Black tree invariants, NavigableMap/NavigableSet range queries, predecessor/successor lookups, and logarithmic search mechanics.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [02_set.md](02_set.md), [03_map.md](03_map.md), [05_comparable-and-comparator.md](05_comparable-and-comparator.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 1.2+ (NavigableMap Java 6+, SequencedMap Java 21 LTS) |

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

While `HashMap` and `HashSet` offer ultra-fast $O(1)$ lookups, they are inherently unordered. In many systems, operations must deal with ordered relationships:
- Finding the nearest lower or higher price tier
- Executing range scans between two timestamps ($T_1 \le t \le T_2$)
- Traversing records in strictly sorted order
- Maintaining real-time leaderboards and priority queues

`java.util.TreeMap` is a balanced binary search tree implementation of `SortedMap`, `NavigableMap`, and `SequencedMap` based on a **Red-Black Tree**. `TreeSet` is a direct set wrapper around `TreeMap`.

`TreeMap` guarantees $O(\log N)$ time complexity for `containsKey`, `get`, `put`, and `remove`, while providing rich range navigation APIs.

---

## 2. Core Concepts

### Red-Black Tree Invariants
A Red-Black Tree is a self-balancing binary search tree where each node has a color attribute (`RED` or `BLACK`). It strictly maintains 5 fundamental invariants:
1. Every node is either **RED** or **BLACK**.
2. The root node is always **BLACK**.
3. Every leaf (`null` sentinel) is **BLACK**.
4. If a node is **RED**, both of its children must be **BLACK** (no two consecutive red nodes on any path).
5. For each node, all simple paths from the node to descendant leaves contain the **exact same number of black nodes** (the Black-Height invariant).

These rules guarantee that the longest possible path from root to leaf is at most twice as long as the shortest path, bounding tree height to $2 \log_2 (N + 1)$.

```
Sample Red-Black Tree:
            [20: Black]  (Root)
           /           \
     [10: Red]       [30: Black]
     /       \             \
[5: Black] [15: Black]   [40: Red]
```

### The `NavigableMap` Interface
`TreeMap` implements `NavigableMap`, which provides directional query methods:
- `lowerKey(K key)`: Strictly greatest key $< \text{key}$
- `floorKey(K key)`: Greatest key $\le \text{key}$
- `ceilingKey(K key)`: Smallest key $\ge \text{key}$
- `higherKey(K key)`: Strictly smallest key $> \text{key}$
- `firstEntry()`, `lastEntry()`, `pollFirstEntry()`, `pollLastEntry()`
- `subMap(from, fromInclusive, to, toInclusive)`: Range view

---

## 3. How It Works Internally

### Node Structure in `TreeMap`
```java
static final class Entry<K,V> implements Map.Entry<K,V> {
    K key;
    V value;
    Entry<K,V> left;
    Entry<K,V> right;
    Entry<K,V> parent;
    boolean color = BLACK;
}
```
Every node maintains pointers to its `left` child, `right` child, and `parent`, allowing bidirectional in-order tree traversal.

### Tree Balancing: Rotations and Recolorings
When an element is inserted, it is initially placed as a **RED** leaf. If its parent is also RED, invariant 4 is violated ("Double Red").
The algorithm balances the tree via two mechanisms:
1. **Recoloring**: Flipping parent and uncle colors when both are red.
2. **Rotations**: Left and Right tree rotations to balance path depths.

```
Left Rotation on node X:
      X                         Y
     / \                       / \
    A   Y       --->          X   C
       / \                   / \
      B   C                 A   B
```
Balancing during insertion takes at most **2 rotations** and $O(\log N)$ recolorings.

---

## 4. Syntax & Usage

### 1. Navigable Range Lookups
```java
import java.util.NavigableMap;
import java.util.TreeMap;

public class NavigableRangeDemo {
    public static void main(String[] args) {
        NavigableMap<Integer, String> statusCodes = new TreeMap<>();
        statusCodes.put(200, "OK");
        statusCodes.put(301, "Moved Permanently");
        statusCodes.put(400, "Bad Request");
        statusCodes.put(404, "Not Found");
        statusCodes.put(500, "Internal Server Error");

        System.out.println("Floor of 403: " + statusCodes.floorEntry(403)); // 400=Bad Request
        System.out.println("Ceiling of 403: " + statusCodes.ceilingEntry(403)); // 404=Not Found

        // Range slice from 300 to 499 (Client/Redirect errors)
        NavigableMap<Integer, String> slice = statusCodes.subMap(300, true, 500, false);
        System.out.println("Client/Redirect errors: " + slice);
    }
}
```

### 2. Custom Comparator Construction
```java
import java.util.Comparator;
import java.util.TreeMap;

public class CustomComparatorMap {
    public static void main(String[] args) {
        // Sort strings by length descending, then lexicographically
        Comparator<String> comp = Comparator
            .comparingInt(String::length).reversed()
            .thenComparing(Comparator.naturalOrder());

        TreeMap<String, Integer> map = new TreeMap<>(comp);
        map.put("banana", 1);
        map.put("apple", 2);
        map.put("fig", 3);
        map.put("watermelon", 4);

        System.out.println("Ordered keys: " + map.keySet());
        // [watermelon, banana, apple, fig]
    }
}
```

---

## 5. Practical Examples

### Example 1: Server Load Balancer Consistent Hashing Ring (Basic to Intermediate)
```java
import java.util.NavigableMap;
import java.util.TreeMap;

public class SimpleHashRing {
    private final NavigableMap<Integer, String> ring = new TreeMap<>();

    public void addServer(String server, int token) {
        ring.put(token, server);
    }

    public String routeRequest(int requestHash) {
        if (ring.isEmpty()) return null;
        // Find first server with token >= requestHash
        var entry = ring.ceilingEntry(requestHash);
        // If past the end of the ring, wrap around to first server
        if (entry == null) {
            entry = ring.firstEntry();
        }
        return entry.getValue();
    }

    public static void main(String[] args) {
        SimpleHashRing hashRing = new SimpleHashRing();
        hashRing.addServer("Server-A", 100);
        hashRing.addServer("Server-B", 300);
        hashRing.addServer("Server-C", 600);

        System.out.println("Route hash 250 -> " + hashRing.routeRequest(250)); // Server-B
        System.out.println("Route hash 700 -> " + hashRing.routeRequest(700)); // Server-A (wrapped)
    }
}
```

### Example 2: Financial Order Book Depth Matching (Real-World)
```java
import java.util.Collections;
import java.util.NavigableMap;
import java.util.TreeMap;

public class OrderBook {
    // Bids sorted descending (highest buyer first)
    private final NavigableMap<Double, Integer> bids = new TreeMap<>(Collections.reverseOrder());
    // Asks sorted ascending (lowest seller first)
    private final NavigableMap<Double, Integer> asks = new TreeMap<>();

    public void addBid(double price, int quantity) {
        bids.merge(price, quantity, Integer::sum);
    }

    public void addAsk(double price, int quantity) {
        asks.merge(price, quantity, Integer::sum);
    }

    public double getSpread() {
        if (bids.isEmpty() || asks.isEmpty()) return Double.NaN;
        return asks.firstKey() - bids.firstKey();
    }

    public static void main(String[] args) {
        OrderBook book = new OrderBook();
        book.addBid(100.50, 10);
        book.addBid(100.75, 5);
        book.addAsk(101.00, 20);

        System.out.println("Best Bid: " + book.bids.firstKey()); // 100.75
        System.out.println("Best Ask: " + book.asks.firstKey()); // 101.00
        System.out.println("Spread: $" + book.getSpread());      // $0.25
    }
}
```

### Example 3: Edge Case - `null` Keys Cause Immediate NPE
```java
import java.util.TreeMap;

public class TreeMapNullKeyEdgeCase {
    public static void main(String[] args) {
        TreeMap<String, String> map = new TreeMap<>();
        try {
            map.put(null, "value");
        } catch (NullPointerException e) {
            System.out.println("TreeMap forbids null keys because compareTo() cannot evaluate null!");
        }
    }
}
```

### Example 4: Failure Case - Inconsistent Comparator Drops Valid Keys
```java
import java.util.TreeMap;

public class DroppedKeyFailure {
    record Student(String name, int score) {}

    public static void main(String[] args) {
        // Flawed comparator comparing ONLY score!
        TreeMap<Student, String> map = new TreeMap<>((s1, s2) -> Integer.compare(s1.score(), s2.score()));
        map.put(new Student("Alice", 90), "Grade A");
        map.put(new Student("Bob", 90), "Grade A"); // DROPPED! compare() returns 0!

        System.out.println("Size: " + map.size()); // 1! Bob overwritten or dropped!
    }
}
```

---

## 6. Best Practices

1. **Ensure Comparison Considers Full Identity**: If comparing records in a `TreeMap`, always add a secondary tie-breaker (such as an ID or name) so distinct items with the same primary value are not discarded.
2. **Use `NavigableMap` Interface as the Variable Type**: Declare variables as `NavigableMap<K, V>` rather than `Map<K, V>` to expose `floor`, `ceiling`, and `subMap` APIs.
3. **Prefer `subMap` Views Over Manual Iteration**: Range views execute in $O(\log N)$ to find endpoints and scan only the relevant range.
4. **Never Pass `null` Keys**: `TreeMap` requires key comparison; `null` keys throw `NullPointerException`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Comparator returning 0 for distinct objects | `TreeMap` treats them as the same key, dropping or overwriting data. | Include unique identity fields in comparison. |
| Passing `null` key to `TreeMap` | Throws `NullPointerException` on `compare()`. | Use `HashMap` if null keys are needed. |
| Using non-`Comparable` key without `Comparator` | Throws `ClassCastException` on first insertion. | Implement `Comparable` or supply a `Comparator`. |
| Mutating key fields after insertion | Corrupts binary search tree invariants; searches land in wrong subtrees. | Use strictly immutable keys. |
| Choosing `TreeMap` when sorting is not required | Incurs $O(\log N)$ lookup overhead instead of $O(1)$ from `HashMap`. | Default to `HashMap`. |
| SubMap index out-of-bounds mutation | Inserting a key outside the `subMap` bounds throws `IllegalArgumentException`. | Insert within declared range boundaries. |

---

## 8. Performance & Security Notes

### Performance
- **Guaranteed Logarithmic Bounds**: Unlike `HashMap` which relies on uniform hashing, `TreeMap` guarantees $O(\log N)$ operations regardless of key distribution.
- **Node Allocation Overhead**: Each entry allocates an internal `Entry<K, V>` object with 5 object references (left, right, parent, key, value) + 1 boolean, consuming 32 bytes of heap per entry.

### Security
- **Predictable Performance Invariants**: `TreeMap` is immune to HashDoS attacks because its balancing depends entirely on binary comparisons rather than hash algorithms.

---

## 9. Comparison / When to Use What

| Feature | `TreeMap` | `HashMap` | `LinkedHashMap` |
|---|---|---|---|
| **Lookup Time** | $O(\log N)$ | $O(1)$ amortized | $O(1)$ amortized |
| **Ordering** | Sorted (Red-Black Tree) | Unordered | Insertion / Access order |
| **Range Queries** | Supported (`subMap`) | Unsupported | Unsupported |
| **Null Keys** | Prohibited (NPE) | Allowed (1 null key) | Allowed (1 null key) |
| **Contract Basis** | `Comparable` / `Comparator` | `equals()` + `hashCode()` | `equals()` + `hashCode()` |

---

## 10. Interview Questions

<details>
<summary>1. What are the 5 invariants of a Red-Black Tree?</summary>
1. Every node is red or black. 2. The root is black. 3. Every leaf (NIL) is black. 4. If a node is red, both children are black (no consecutive red nodes). 5. Every simple path from a node to descendant leaves contains the same number of black nodes.
</details>

<details>
<summary>2. Why does `TreeMap` not require `equals()` or `hashCode()` for key lookups?</summary>
`TreeMap` locates entries entirely by traversing its binary search tree using `Comparable.compareTo()` or `Comparator.compare()`. If `compare(a, b) == 0`, the keys are deemed identical.
</details>

<details>
<summary>3. What is the difference between `floorKey()` and `lowerKey()`?</summary>
`floorKey(k)` returns the greatest key $\le k$. `lowerKey(k)` returns the strictly greatest key $< k$.
</details>

<details>
<summary>4. What is the time complexity of building a `TreeMap` from an unsorted list of $N$ elements?</summary>
$O(N \log N)$ because each of the $N$ insertions requires $O(\log N)$ tree traversal and rotation steps.
</details>

<details>
<summary>5. How does `TreeMap.subMap()` work internally?</summary>
It returns an internal `AscendingSubMap` view bounding the search range. It does not copy entries; operations on the submap interact directly with the backing tree within the bounded interval.
</details>

<details>
<summary>6. Can you insert a `null` key into a `TreeMap`?</summary>
No. In modern Java, attempting to insert a `null` key throws `NullPointerException`.
</details>

<details>
<summary>7. How does Java 21 Sequenced Collections affect `TreeMap`?</summary>
`TreeMap` implements `SequencedMap`. It provides `firstEntry()`, `lastEntry()`, `pollFirstEntry()`, `pollLastEntry()`, `putFirst()`, `putLast()`, and `reversed()`.
</details>

<details>
<summary>8. How many rotations are required in the worst-case during a `TreeMap` insertion?</summary>
At most 2 rotations are required to restore Red-Black invariants after an insertion.
</details>

---

## 11. Practice Exercises

### Easy
1. Create a `TreeMap<Integer, String>`, insert 5 pairs, and print them in reverse order using `descendingMap()`.
2. Find the closest lower and higher temperature thresholds in a `TreeSet<Double>`.
3. Demonstrate that inserting a `null` key into a `TreeMap` throws `NullPointerException`.

### Medium
4. Implement a time-series cache using `TreeMap<Instant, MetricData>` that retrieves all metrics recorded between two timestamps.
5. Build a leaderboard system that maintains the top 10 players by score using `TreeMap`.
6. Write a function that finds all words with a specific prefix in a `TreeSet<String>` using `subSet`.

### Hard
7. Implement an in-memory B-Tree from scratch and compare search performance against `TreeMap`.
8. Implement an interval tree using `TreeMap` that finds all overlapping time intervals in $O(\log N + K)$ time.
9. Construct a concurrent lock-free skip list and verify logarithmic bounds against `TreeMap`.

<details>
<summary>Solutions</summary>

```java
// Easy 2: Threshold lookup
TreeSet<Double> temps = new TreeSet<>(List.of(18.5, 20.0, 22.5, 25.0));
System.out.println("Lower: " + temps.lower(21.0));   // 20.0
System.out.println("Higher: " + temps.higher(21.0)); // 22.5

// Medium 6: Prefix matching with subSet
public static Set<String> wordsWithPrefix(TreeSet<String> dict, String prefix) {
    return dict.subSet(prefix, prefix + Character.MAX_VALUE);
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Data Structure**: Balanced Red-Black Tree.
- **Complexity**: $O(\log N)$ for `get`, `put`, `remove`, `containsKey`.
- **Navigation**: `floorKey`, `ceilingKey`, `lowerKey`, `higherKey`.
- **Range Views**: `subMap(from, fromIncl, to, toIncl)`.
- **Key Equality**: Relies exclusively on `compareTo()` / `compare()`, ignores `equals()`.
- **Null Keys**: Strictly forbidden (`NullPointerException`).

---

## 13. Further Reading
- Cormen, Leiserson, Rivest, Stein. *Introduction to Algorithms (CLRS)*: Chapter 13 (Red-Black Trees).
- OpenJDK: `java.util.TreeMap` implementation comments.
- Oracle Java Documentation: *The NavigableMap Interface*.
