# Interview Mastery: Collections Framework & Internals

> High-yield interview guide for Java Collections: `HashMap` treeification mechanics, `ConcurrentHashMap` striped synchronization, `ArrayDeque` circular buffers, Sequenced Collections, and Fail-Fast iterators.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_core-java.md](00_core-java.md), [08-collections](../08-collections/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to Java 25 LTS |

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

The **Java Collections Framework (JCF)** is the central data architecture of enterprise Java applications. In senior technical interviews, questions move far beyond basic API usage (`list.add()`) to scrutinize **hot-path memory layouts, resize mathematics, concurrency primitives, and algorithmic degradation under load**.

Interviewers specifically probe:
- Why `HashMap` table sizes are strictly powers of two (`(n - 1) & hash`).
- The transition boundary between linked lists and **Red-Black trees (Treeification, JEP 180)**.
- Why `ArrayDeque` is substantially faster than `LinkedList` and `Stack`.
- The synchronization difference between `Hashtable`, `Collections.synchronizedMap()`, and `ConcurrentHashMap`.
- Modern Java 21 **Sequenced Collections (JEP 431)**.

---

## 2. Core Concepts

### 1. The Core Hierarchy & Sequenced Collections (Java 21+)
Java 21 unified historical collection interfaces under **Sequenced Collections**:
- `SequencedCollection<E>`: Defines uniform first/last access: `addFirst()`, `addLast()`, `getFirst()`, `getLast()`, `removeFirst()`, `removeLast()`, and `reversed()`.
- Implemented by `List`, `Deque`, `LinkedHashSet`, `SortedSet`.
- `SequencedMap<K, V>`: Implemented by `LinkedHashMap`, `SortedMap`.

### 2. `HashMap` Resizing and Bucket Treeification
- **Default Initial Capacity**: 16; **Default Load Factor**: 0.75.
- **Index Calculation**: Bitwise AND instead of expensive modulus: `index = (capacity - 1) & hash`.
- **Treeification Threshold**: When a bucket reaches **8 elements** and the total table capacity is **$\ge 64$**, HotSpot converts that bucket from a singly linked list (`Node<K,V>`) into a Red-Black tree (`TreeNode<K,V>`), bounding worst-case lookup from $O(N)$ to $O(\log N)$.
- **Untreeify Threshold**: Shrinks back to a linked list when bucket count drops to **6** during resizing.

### 3. Fail-Fast vs Fail-Safe / Weakly Consistent Iterators
- **Fail-Fast (`ArrayList`, `HashMap`)**: Tracks an internal counter `modCount`. If `modCount` changes during iteration (concurrent modification without using `Iterator.remove()`), the iterator throws `ConcurrentModificationException` immediately.
- **Weakly Consistent (`ConcurrentHashMap`)**: Iterators operate on a snapshot or tolerate concurrent modifications without throwing exceptions.
- **Fail-Safe / Snapshot (`CopyOnWriteArrayList`)**: Iterates over an immutable snapshot of the backing array created at iterator instantiation time.

---

## 3. How It Works Internally

### `HashMap` Memory Architecture
```
HashMap Table (Node<K, V>[] table)
+-----------------------------------------------------------+
| [0] null                                                  |
| [1] -> Node(K, V) -> Node(K, V) -> null (Linked List)     |
| [2] null                                                  |
| [3] -> TreeNode(K, V) [Red-Black Tree Root]               |
|          /         \                                      |
|      TreeNode    TreeNode                                 |
+-----------------------------------------------------------+

TreeNode Memory Cost:
- Extends LinkedHashMap.Entry which extends HashMap.Node:
  12 bytes Header + 4 refs (parent, left, right, prev) + boolean red
  Each TreeNode costs ~56 bytes (more than double standard 32-byte Node).
  Treeification is delayed until capacity >= 64 to save memory!
```

### `ConcurrentHashMap` Fine-Grained Locking
Java 8 completely eliminated the old 16-segment lock table:
1. **Empty Bucket Insertion**: Uses lock-free hardware **Compare-And-Swap (CAS)** (`Unsafe.compareAndSetObject`).
2. **Occupied Bucket Collisions**: Locks **strictly the head node** of the target bucket using `synchronized(headNode)`. All other buckets continue concurrent read and write operations without blocking!

---

## 4. Syntax & Usage

### Modern Collections Idioms (Java 17–25)

```java
import java.util.*;

public class ModernCollectionsSyntax {
    public static void main(String[] args) {
        // 1. Unmodifiable Factories (Java 9+)
        List<String> list = List.of("A", "B", "C"); // Immutable, rejects nulls
        Map<String, Integer> map = Map.of("X", 10, "Y", 20);

        // 2. Defensive Unmodifiable Snapshot (Java 10+)
        List<String> copy = List.copyOf(list);

        // 3. Sequenced Collections (Java 21+)
        List<String> seq = new ArrayList<>(List.of("first", "middle", "last"));
        assert seq.getFirst().equals("first");
        assert seq.getLast().equals("last");

        List<String> reversedView = seq.reversed(); // O(1) reversed live view
        assert reversedView.getFirst().equals("last");

        // 4. Map Compute Idioms
        Map<String, Integer> wordCounts = new HashMap<>();
        wordCounts.merge("apple", 1, Integer::sum);
        wordCounts.computeIfAbsent("banana", k -> 0);
    }
}
```

---

## 5. Practical Examples

### Example 1: `HashMap` Treeification & Hash Collision Simulation

```java
import java.util.HashMap;
import java.util.Map;

public class TreeificationProof {
    // BadKey forces all instances into bucket index (capacity - 1) & 42
    record BadKey(int id) implements Comparable<BadKey> {
        @Override
        public int hashCode() {
            return 42; // Constant hash forces collision!
        }

        @Override
        public int compareTo(BadKey other) {
            return Integer.compare(this.id, other.id); // Allows Red-Black Tree ordering
        }
    }

    public static void main(String[] args) {
        Map<BadKey, String> map = new HashMap<>();

        // Insert 10 elements with identical hashcode
        for (int i = 0; i < 10; i++) {
            map.put(new BadKey(i), "Val" + i);
        }

        // When size reaches 8 and table capacity >= 64,
        // HotSpot treeifies the collided bucket from LinkedList to Red-Black Tree!
        System.out.println("Retrieved key 5: " + map.get(new BadKey(5)));
    }
}
```

### Example 2: Building an LRU Cache with `LinkedHashMap`

```java
import java.util.LinkedHashMap;
import java.util.Map;

public class SimpleLRUCache<K, V> extends LinkedHashMap<K, V> {
    private final int maxEntries;

    public SimpleLRUCache(int maxEntries) {
        // initialCapacity, loadFactor=0.75, accessOrder=true (least-recently-accessed evicted first)
        super(maxEntries, 0.75f, true);
        this.maxEntries = maxEntries;
    }

    @Override
    protected boolean removeEldestEntry(Map.Entry<K, V> eldest) {
        // Automatically evicts the eldest element when size exceeds capacity
        return size() > maxEntries;
    }

    public static void main(String[] args) {
        SimpleLRUCache<Integer, String> cache = new SimpleLRUCache<>(3);
        cache.put(1, "A");
        cache.put(2, "B");
        cache.put(3, "C");

        cache.get(1); // Access key 1; moves 1 to most recently used
        cache.put(4, "D"); // Exceeds capacity 3 -> Evicts key 2!

        System.out.println(cache.keySet()); // Prints [3, 1, 4]
    }
}
```

### Example 3: Fail-Fast vs Fail-Safe Demonstration

```java
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

public class FailFastVsFailSafe {
    public static void main(String[] args) {
        // 1. Fail-Fast in HashMap
        Map<Integer, String> standardMap = new HashMap<>();
        standardMap.put(1, "A");
        standardMap.put(2, "B");

        try {
            for (Integer key : standardMap.keySet()) {
                if (key == 1) {
                    standardMap.put(3, "C"); // Mutates map directly -> modifies modCount!
                }
            }
        } catch (ConcurrentModificationException e) {
            System.out.println("Caught expected ConcurrentModificationException on HashMap!");
        }

        // 2. Weakly-Consistent in ConcurrentHashMap
        Map<Integer, String> concurrentMap = new ConcurrentHashMap<>();
        concurrentMap.put(1, "A");
        concurrentMap.put(2, "B");

        for (Integer key : concurrentMap.keySet()) {
            if (key == 1) {
                concurrentMap.put(3, "C"); // Completely safe; iterator reflects concurrent state
            }
        }
        System.out.println("ConcurrentHashMap iteration finished safely. Size: " + concurrentMap.size());
    }
}
```

### Example 4: `ArrayList` Dynamic Resizing Mathematics

```java
public class ArrayListGrowthAnalysis {
    public static void main(String[] args) {
        // In HotSpot OpenJDK:
        // int newCapacity = oldCapacity + (oldCapacity >> 1); // 1.5x growth
        int oldCap = 10;
        int newCap = oldCap + (oldCap >> 1); // 10 + 5 = 15
        System.out.println("Initial capacity 10 expands to: " + newCap); // 15

        oldCap = 15;
        newCap = oldCap + (oldCap >> 1); // 15 + 7 = 22
        System.out.println("Initial capacity 15 expands to: " + newCap); // 22
    }
}
```

### Example 5: `IdentityHashMap` vs Standard `HashMap`

```java
import java.util.HashMap;
import java.util.IdentityHashMap;

public class IdentityHashMapInterview {
    public static void main(String[] args) {
        String key1 = new String("key");
        String key2 = new String("key");

        // Standard HashMap uses .equals() and .hashCode()
        HashMap<String, String> standard = new HashMap<>();
        standard.put(key1, "Val1");
        standard.put(key2, "Val2");
        System.out.println("Standard map size: " + standard.size()); // 1 (key1.equals(key2) == true)

        // IdentityHashMap uses System.identityHashCode() and reference equality (==)
        IdentityHashMap<String, String> identity = new IdentityHashMap<>();
        identity.put(key1, "Val1");
        identity.put(key2, "Val2");
        System.out.println("Identity map size: " + identity.size()); // 2 (key1 != key2 on heap)
    }
}
```

---

## 6. Best Practices

1. **Pre-Size `HashMap` to Prevent Rehashing**:
   $$\text{initialCapacity} = \left\lceil \frac{\text{expectedElements}}{0.75} \right\rceil + 1$$
   Or in Java 19+: `HashMap.newHashMap(expectedElements)`.
2. **Prefer `ArrayDeque` Over `LinkedList` and `Stack`**: `ArrayDeque` has zero object allocation overhead per element, fits neatly into CPU cache lines, and avoids legacy synchronization locks.
3. **Use Immutable Collections for DTOs**: Use `List.of()`, `Set.of()`, and `Map.of()`. They reject nulls, consume minimal memory, and guarantee immutability.
4. **Keys in `HashMap` Must Be Immutable**: If an object key's fields mutate while stored in a `HashMap`, its `hashCode` shifts, rendering the entry unretrievable (`get()` returns `null`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Modifying collection during foreach loop | `modCount != expectedModCount` triggers `ConcurrentModificationException` | Use `iterator.remove()` or `collection.removeIf()`. |
| Using `Arrays.asList()` expecting a resizable list | Returns a fixed-size adapter backed by the array; `add()` throws `UnsupportedOperationException` | Wrap in `new ArrayList<>(Arrays.asList(...))`. |
| Mutating key object stored in `HashMap` | Hash code changes; element is trapped in the wrong bucket forever | Make all Map key objects strictly immutable (`final` or `record`). |
| Using `TreeMap` without `Comparable` or `Comparator` | Throws `ClassCastException` upon first element insertion | Implement `Comparable<T>` or pass explicit `Comparator`. |
| Expecting `Set.of()` to accept duplicates | Throws `IllegalArgumentException` at construction time | Verify uniqueness or populate a standard `HashSet`. |

---

## 8. Performance & Security Notes

- **Initial Capacity Performance Impact**: An under-sized `HashMap` that grows to $1,000,000$ elements triggers 16 consecutive table reallocations and millions of element re-hash operations.
- **HashDoS Resilience**: Prior to Java 8, an attacker could send thousands of strings sharing the same `hashCode` to crash servers via $O(N^2)$ CPU spikes. Treeification (JEP 180) bounds worst-case bucket processing strictly to $O(\log N)$ when keys implement `Comparable`.

---

## 9. Comparison / When to Use What

| Collection | Underlying Data Structure | Access Time | Insert / Remove | Thread Safe |
|---|---|---|---|---|
| **`ArrayList`** | Resizable `Object[]` | $O(1)$ random | $O(N)$ amortized | No |
| **`LinkedList`** | Doubly Linked List | $O(N)$ sequential | $O(1)$ at head/tail | No |
| **`ArrayDeque`** | Circular `Object[]` | $O(1)$ at ends | $O(1)$ amortized | No |
| **`HashMap`** | Array + List / Red-Black Tree | $O(1)$ avg, $O(\log N)$ worst | $O(1)$ avg | No |
| **`TreeMap`** | Red-Black Tree | $O(\log N)$ | $O(\log N)$ | No |
| **`ConcurrentHashMap`** | Array + List / Tree + CAS / Locks | $O(1)$ avg | $O(1)$ avg | Yes (Concurrent) |

---

## 10. Interview Questions

<details>
<summary>1. Why must `HashMap` bucket capacities always be a power of two?</summary>

To calculate bucket index $i$, standard arithmetic uses $i = \text{hash} \pmod{\text{capacity}}$. The modulo operator `%` is computationally expensive on CPUs. When capacity is a power of 2 ($2^k$), mathematically:
$$\text{hash} \pmod{2^k} \equiv \text{hash} \ \& \ (2^k - 1)$$
Bitwise AND `&` executes in a single CPU clock cycle, delivering maximum lookup performance.
</details>

<details>
<summary>2. Explain the criteria and process of `HashMap` Treeification in Java 8+.</summary>

When a hash collision appends an element to a bucket:
1. If bucket length reaches **8 elements (TREEIFY_THRESHOLD)**:
2. The JVM checks if total table capacity is $\ge 64$ (**MIN_TREEIFY_CAPACITY**).
3. If capacity $< 64$, the table is resized instead of treeifying.
4. If capacity $\ge 64$, the bucket is converted into a Red-Black Tree (`TreeNode`).
5. If elements in the bucket are later deleted down to **6 (UNTREEIFY_THRESHOLD)** during resize, it converts back to a linked list.
</details>

<details>
<summary>3. How does `ConcurrentHashMap` achieve thread safety in Java 8+ without segment locks?</summary>

Java 8 discarded `Segment` locks in favor of:
1. **CAS (Compare-And-Swap)** for inserting the very first node into an empty bucket (`tabAt` and `casTabAt`).
2. **Synchronized Blocks on Bucket Head Node**: When a collision occurs, it locks strictly the head node (`synchronized (f)`). Different threads accessing different buckets run completely concurrently.
3. **Concurrent Resizing**: Multiple threads cooperate to migrate buckets during table expansion using transfer markers.
</details>

<details>
<summary>4. Why is `ArrayDeque` preferred over `LinkedList` as a Queue or Stack?</summary>

1. `ArrayDeque` uses a circular contiguous `Object[]` array with zero object allocation overhead per element. `LinkedList` allocates an independent `Node` object for every insertion (24 bytes overhead per node).
2. `ArrayDeque` elements are stored contiguously in memory, maximizing CPU cache line spatial locality.
3. `ArrayDeque` outperforms `LinkedList` by 2x to 5x in almost every benchmark.
</details>

<details>
<summary>5. What are Sequenced Collections introduced in Java 21 (JEP 431)?</summary>

Prior to Java 21, collections lacked a uniform API for ordered elements: `List.get(0)` vs `Deque.getFirst()` vs `LinkedHashSet.iterator().next()`. Java 21 introduced `SequencedCollection<E>`, `SequencedSet<E>`, and `SequencedMap<K, V>` providing standard first/last accessors (`getFirst()`, `getLast()`, `addFirst()`, `addLast()`) and an $O(1)$ live reversed view (`reversed()`).
</details>

<details>
<summary>6. What is the difference between `Comparable` and `Comparator`?</summary>

- **`Comparable<T>`**: Implemented by the class itself via `compareTo(T other)` to define its **natural ordering** (e.g., `String`, `Integer`).
- **`Comparator<T>`**: An external strategy interface implemented via `compare(T o1, T o2)` to define **custom or secondary orderings** (e.g. sorting employees by salary descending, then by name ascending).
</details>

<details>
<summary>7. What happens if a `HashMap` key object does not implement `Comparable` when treeifying?</summary>

If keys in a collided bucket implement `Comparable`, the Red-Black tree sorts nodes by `compareTo()`. If keys do NOT implement `Comparable`, HotSpot falls back to `System.identityHashCode()` as a secondary tie-breaker to ensure a deterministic binary search tree structure.
</details>

<details>
<summary>8. Why does `List.of()` throw an exception when passing `null`, while `Arrays.asList()` permits `null`?</summary>

Modern Java collections (`List.of`, `Set.of`, `Map.of`) follow a strict modern design philosophy: **null-hostile collections**. Null values are the leading cause of `NullPointerExceptions` and semantic ambiguity (does `map.get(k) == null` mean key is absent or value is null?). Modern factories proactively reject nulls via `Objects.requireNonNull()`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Implement a Frequency Counter Using `Map.merge()`
Count the frequencies of words in a sentence using modern Java `Map` idioms.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class WordFrequencyCounter {
    public static Map<String, Integer> countFrequencies(String text) {
        Map<String, Integer> counts = new HashMap<>();
        if (text == null || text.isBlank()) return counts;

        for (String word : text.toLowerCase().split("\\W+")) {
            if (!word.isBlank()) {
                // If key absent, puts 1; if present, applies Integer::sum
                counts.merge(word, 1, Integer::sum);
            }
        }
        return counts;
    }
}
```
</details>

### Exercise 2: Diagnose the ConcurrentModificationException
Identify why this code crashes and provide the idiomatic Java 8+ fix:
```java
List<String> list = new ArrayList<>(List.of("alpha", "beta", "gamma"));
for (String s : list) {
    if (s.startsWith("b")) {
        list.remove(s);
    }
}
```

<details>
<summary>Solution</summary>

**Cause**: Calling `list.remove(s)` mutates the underlying `ArrayList`, incrementing `modCount`. The enhanced for-loop's iterator detects `modCount != expectedModCount` on the next iteration and throws `ConcurrentModificationException`.

**Idiomatic Fix (Java 8+)**:
```java
list.removeIf(s -> s.startsWith("b"));
```
</details>

### Exercise 3: Build a Composite Multi-Field Comparator
Sort a list of `Employee` records by Department ascending, then Salary descending, then Name ascending.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class EmployeeComparatorDemo {
    public record Employee(String name, String department, double salary) {}

    public static void sortEmployees(List<Employee> list) {
        Comparator<Employee> comparator = Comparator
            .comparing(Employee::department)
            .thenComparing(Comparator.comparingDouble(Employee::salary).reversed())
            .thenComparing(Employee::name);

        list.sort(comparator);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **HashMap Power of 2**: Bucket index calculated as `(capacity - 1) & hash`.
- **Treeification Rules**: Bucket length $\ge 8$ and table capacity $\ge 64 \implies$ Red-Black tree.
- **ConcurrentHashMap**: CAS for empty buckets; synchronized locking on the head node for collisions.
- **Sequenced Collections (Java 21)**: Uniform `getFirst()`, `getLast()`, and `reversed()` across lists, deques, and linked sets.
- **Pre-Sizing**: Always allocate `expectedSize / 0.75 + 1` to prevent expensive re-hashing.

---

## 13. Further Reading

- [JEP 431: Sequenced Collections (Java 21 LTS)](https://openjdk.org/jeps/431)
- [JEP 180: Handle Frequent HashMap Collisions with Balanced Trees](https://openjdk.org/jeps/180)
- [OpenJDK Source Code: `java.util.HashMap`](https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/util/HashMap.java)
- [Baeldung: Guide to ConcurrentHashMap Internals](https://www.baeldung.com/java-concurrent-map)
