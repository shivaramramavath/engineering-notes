# The Map Interface and Implementations

> Key-value associative mapping architecture: core contracts, hash tables, tree maps, collision handling, and modern concurrent mapping.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_collection-hierarchy.md](00_collection-hierarchy.md), [04-oop/14_equals-and-hashcode.md](../04-oop/14_equals-and-hashcode.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 1.2+ (Map.of Java 9, SequencedMap Java 21 LTS) |

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

The `java.util.Map<K, V>` interface models an associative array or dictionary: an object that maps keys to values. A map cannot contain duplicate keys; each key can map to at most one value.

Maps are the computational backbone of modern applications:
- In-memory caching and session tracking
- Relational joins, aggregations, and frequency indexing
- Configuration bags and dependency registries
- JSON and serialization data mapping

While `Map` is an integral part of the Java Collections Framework, it does **not** implement `Collection` or `Iterable`. Instead, it provides three collection views:
1. `Set<K> keySet()`: Set of keys.
2. `Collection<V> values()`: Collection of values (may contain duplicates).
3. `Set<Map.Entry<K, V>> entrySet()`: Set of key-value pairs.

---

## 2. Core Concepts

### Core Implementations
1. **`HashMap`**: Hash-table based; $O(1)$ average time for `get` and `put`. Unordered.
2. **`LinkedHashMap`**: Hash table with a doubly-linked list. Maintains **insertion order** or **access order** (for LRU caches). Implements `SequencedMap` in Java 21.
3. **`TreeMap`**: Red-black tree based; $O(\log N)$ operations. Keys sorted by natural order or `Comparator`. Implements `NavigableMap` and `SequencedMap`.
4. **`ConcurrentHashMap`**: High-performance thread-safe map utilizing lock striping, compare-and-swap (CAS) primitives, and lock-free reads.
5. **`EnumMap`**: Specialized map where keys are enum constants; stored as a flat array.

### Java 8+ Functional Methods on Map
Java 8 enriched `Map` with powerful lambda-friendly default methods:
- `getOrDefault(Object key, V defaultValue)`
- `putIfAbsent(K key, V value)`
- `computeIfAbsent(K key, Function<? super K, ? extends V> mappingFunction)`
- `computeIfPresent(K key, BiFunction<? super K, ? super V, ? extends V> remappingFunction)`
- `merge(K key, V value, BiFunction<? super V, ? super V, ? extends V> remappingFunction)`

---

## 3. How It Works Internally

### The Collection Views and Backing Reflection
The `keySet()`, `values()`, and `entrySet()` methods return **live views** backed by the map.
- If you call `map.keySet().remove("key1")`, the entry `"key1"` is deleted from the underlying `Map`.
- Calling `map.values().remove("val1")` removes the first entry with that value.
- Adding to `keySet()` or `values()` (`keySet().add("newKey")`) throws `UnsupportedOperationException`.

```
Map Storage:  [ ("id", 101), ("name", "Alice"), ("status", "ACTIVE") ]
                    ^                    ^                     ^
                    |                    |                     |
keySet() View:   [ "id",               "name",               "status" ]
values() View:   [ 101,                "Alice",              "ACTIVE" ]
entrySet() View: [ ("id", 101),        ("name", "Alice"),    ("status", "ACTIVE") ]
```

### SequencedMap in Java 21
`SequencedMap` retrofits map ordering:
- `putFirst(K, V)` / `putLast(K, V)`
- `firstEntry()` / `lastEntry()`
- `pollFirstEntry()` / `pollLastEntry()`
- `sequencedKeySet()`, `sequencedValues()`, `sequencedEntrySet()`
- `reversed()`: returns a live reverse-ordered map view!

---

## 4. Syntax & Usage

### 1. Functional Map Manipulation (Java 8+)
```java
import java.util.HashMap;
import java.util.Map;

public class FunctionalMapDemo {
    public static void main(String[] args) {
        Map<String, Integer> wordCounts = new HashMap<>();

        // Frequency counter using merge
        String[] words = {"apple", "banana", "apple", "cherry", "apple"};
        for (String word : words) {
            wordCounts.merge(word, 1, Integer::sum);
        }
        System.out.println("Word counts: " + wordCounts); // {banana=1, apple=3, cherry=1}

        // Cache computation using computeIfAbsent
        Map<Integer, String> cache = new HashMap<>();
        String val = cache.computeIfAbsent(42, k -> "ComputedValue-" + k);
        System.out.println("Cached: " + val);
    }
}
```

### 2. Creation via Modern Factories (Java 9+)
```java
import java.util.Map;

public class MapFactoryDemo {
    public static void main(String[] args) {
        // Up to 10 entries via overloaded of(...)
        Map<String, Integer> config = Map.of(
            "timeout", 5000,
            "retries", 3,
            "port", 8080
        );

        // Arbitrary entries via ofEntries and entry(...)
        Map<String, String> routes = Map.ofEntries(
            Map.entry("/api/users", "UserController"),
            Map.entry("/api/orders", "OrderController")
        );
    }
}
```

---

## 5. Practical Examples

### Example 1: Grouping Elements without Streams (Basic)
```java
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class GroupingDemo {
    public static void main(String[] args) {
        List<String> names = List.of("Alice", "Bob", "Charlie", "David", "Amy", "Brian");
        Map<Character, List<String>> grouped = new HashMap<>();

        for (String name : names) {
            grouped.computeIfAbsent(name.charAt(0), k -> new ArrayList<>()).add(name);
        }
        System.out.println("Grouped by first letter: " + grouped);
        // {A=[Alice, Amy], B=[Bob, Brian], C=[Charlie], D=[David]}
    }
}
```

### Example 2: Access-Order LRU Cache with `LinkedHashMap` (Real-World)
```java
import java.util.LinkedHashMap;
import java.util.Map;

public class LruCache<K, V> extends LinkedHashMap<K, V> {
    private final int maxEntries;

    // accessOrder = true enables LRU access-order tracking
    public LruCache(int maxEntries) {
        super(16, 0.75f, true);
        this.maxEntries = maxEntries;
    }

    @Override
    protected boolean removeEldestEntry(Map.Entry<K, V> eldest) {
        return size() > maxEntries; // Automatically evicts eldest entry!
    }

    public static void main(String[] args) {
        LruCache<String, String> cache = new LruCache<>(2);
        cache.put("k1", "v1");
        cache.put("k2", "v2");
        cache.get("k1"); // Access k1, making k2 the eldest
        cache.put("k3", "v3"); // Evicts k2!

        System.out.println("Remaining entries: " + cache.keySet()); // [k1, k3]
    }
}
```

### Example 3: Edge Case - Iterating and Removing via `entrySet()`
```java
import java.util.HashMap;
import java.util.Map;

public class MapEntryRemovalEdgeCase {
    public static void main(String[] args) {
        Map<String, Integer> map = new HashMap<>();
        map.put("A", 10);
        map.put("B", 20);
        map.put("C", 30);

        // Safe removal using entrySet iterator or removeIf
        map.entrySet().removeIf(entry -> entry.getValue() > 15);
        System.out.println("Remaining: " + map); // {A=10}
    }
}
```

### Example 4: Failure Case - Modifying Map during `computeIfAbsent`
```java
import java.util.HashMap;
import java.util.Map;

public class ComputeIfAbsentRecursionFailure {
    public static void main(String[] args) {
        Map<String, String> map = new HashMap<>();
        try {
            map.computeIfAbsent("key", k -> {
                map.put("otherKey", "val"); // STRUCTURAL MUTATION INSIDE COMPUTE!
                return "result";
            });
        } catch (Exception e) {
            // Throws ConcurrentModificationException in HashMap!
            System.out.println("Caught expected exception: " + e.getClass().getSimpleName());
        }
    }
}
```

---

## 6. Best Practices

1. **Use `computeIfAbsent` and `merge`**: Eliminate boilerplate null-checks and multi-step lookups with functional methods.
2. **Iterate over `entrySet()` Instead of `keySet()` + `get()`**: Iterating keys and then calling `get(k)` causes repeated hash lookups ($2 \times$ overhead). `entrySet()` traverses keys and values simultaneously in a single pass.
3. **Use Immutable Keys**: Always ensure map keys are strictly immutable (`String`, `Integer`, `record`).
4. **Choose `ConcurrentHashMap` for Threaded Code**: Never wrap a `HashMap` in `Collections.synchronizedMap` if scalable concurrent reads and writes are needed.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Calling `map.keySet().add(k)` | KeySet is an unmodifiable view for insertions; throws `UnsupportedOperationException`. | Add via `map.put(k, v)`. |
| Modifying key fields after insertion | Breaks hash bucket indexing; entry becomes unretrievable. | Use immutable objects as keys. |
| Looping `keySet()` and calling `map.get(k)` | Wastes CPU doing double hash calculations. | Loop over `map.entrySet()`. |
| Passing duplicate keys to `Map.of("a", 1, "a", 2)` | Throws `IllegalArgumentException` on initialization. | Deduplicate initial keys. |
| Calling `computeIfAbsent` recursively on the same map | Causes `ConcurrentModificationException` or deadlock in `ConcurrentHashMap`. | Keep mapping functions side-effect free. |
| Using `Hashtable` for thread safety | Global synchronization bottleneck from Java 1.0. | Use `ConcurrentHashMap`. |

---

## 8. Performance & Security Notes

### Performance
- **Traversing `entrySet()` vs `keySet()`**: Traversing `entrySet()` avoids $N$ secondary hash lookups, resulting in up to a 40% performance gain on large maps.
- **Pre-sizing Capacity**: If storing $N$ entries, initialize capacity with $\lceil N / 0.75 \rceil + 1$ to avoid expensive table resizing and rehashing.

### Security
- **Hash Collision Resistance**: Java 8+ converts hash bins into balanced Red-Black trees when bin entries exceed 8, guarding against Denial of Service attacks that exploit engineered hash collisions.

---

## 9. Comparison / When to Use What

| Implementation | Ordering | Concurrency | Null Keys / Values | Best Use Case |
|---|---|---|---|---|
| `HashMap` | None | Non-thread-safe | 1 null key, null values | General-purpose dictionary |
| `LinkedHashMap` | Insertion / Access | Non-thread-safe | 1 null key, null values | Predictable iteration, LRU caches |
| `TreeMap` | Sorted key order | Non-thread-safe | No null keys | Range queries, subMaps |
| `ConcurrentHashMap` | None | Thread-safe (CAS/locks) | Strictly NO nulls | Multi-threaded caching & indexing |
| `EnumMap` | Enum ordinal | Non-thread-safe | No null keys | Enum-keyed mapping |

---

## 10. Interview Questions

<details>
<summary>1. Why does `ConcurrentHashMap` forbid `null` keys and `null` values?</summary>
To eliminate ambiguity in concurrent environments. In a non-concurrent map, `map.get(key) == null` could mean the key is not present, or the key is mapped to `null` (resolved via `containsKey`). In a concurrent map, between calling `get()` and `containsKey()`, another thread could modify the map, creating race conditions.
</details>

<details>
<summary>2. What is the difference between `putIfAbsent` and `computeIfAbsent`?</summary>
`putIfAbsent(k, v)` always evaluates the value expression even if the key is already present. `computeIfAbsent(k, mappingFunction)` evaluates the function lazily only if the key is absent or mapped to null, preventing expensive wasted computations.
</details>

<details>
<summary>3. How does `LinkedHashMap` support access-order for LRU caching?</summary>
When constructed with `new LinkedHashMap<>(capacity, loadFactor, true)`, every `get()` or `put()` moves the accessed entry to the tail of the internal doubly-linked list. Overriding `removeEldestEntry` allows evicting the head of the list when capacity is exceeded.
</details>

<details>
<summary>4. What does the `merge` method do?</summary>
`map.merge(key, value, remappingFunction)`: If the key is not present or null, it associates it with the given value. If present, it computes a new value using the function `(oldVal, newVal) -> computedVal`. If the function returns `null`, the key is removed.
</details>

<details>
<summary>5. What are collection views in `Map`?</summary>
`keySet()`, `values()`, and `entrySet()`. They provide views into the map's elements without copying. Deleting an item from a view deletes it from the map.
</details>

<details>
<summary>6. How does Java 21 Sequenced Collections enhance `Map`?</summary>
Via `SequencedMap`, providing uniform `firstEntry()`, `lastEntry()`, `pollFirstEntry()`, `pollLastEntry()`, `putFirst()`, `putLast()`, and `reversed()`.
</details>

<details>
<summary>7. What happens if two distinct keys produce the exact same `hashCode()`?</summary>
A hash collision occurs. Both entries are stored in the same bucket (bin). In Java 8+, if the bin contains fewer than 8 entries, it is stored as a linked list. If it exceeds 8 and table capacity is at least 64, the bin is converted into a balanced Red-Black tree.
</details>

<details>
<summary>8. Why should you avoid `Collections.synchronizedMap` in modern applications?</summary>
`Collections.synchronizedMap` synchronizes every operation on a single mutual exclusion lock, creating severe thread contention. `ConcurrentHashMap` provides fine-grained locking and lock-free reads.
</details>

---

## 11. Practice Exercises

### Easy
1. Count the frequency of characters in a string using `Map.merge`.
2. Convert a `List<String>` of words into a map of `(word -> length)`.
3. Retrieve all keys mapped to a value greater than 50 using `entrySet()`.

### Medium
4. Invert a `Map<K, V>` into a `Map<V, List<K>>` handling duplicate values cleanly.
5. Implement a simple two-level nested cache `Map<String, Map<String, Object>>` with safe `computeIfAbsent`.
6. Write a function that finds the first non-repeating character in a stream using `LinkedHashMap`.

### Hard
7. Build a full-featured thread-safe LRU cache with TTL expiration using `ConcurrentHashMap` and a doubly-linked list.
8. Implement a generic multimap `MultiMap<K, V>` that enforces set uniqueness on values.
9. Construct a memory-efficient sparse matrix using a `Map<Pair<Integer, Integer>, Double>`.

<details>
<summary>Solutions</summary>

```java
// Easy 1: Character frequency
public static Map<Character, Integer> charFrequencies(String s) {
    Map<Character, Integer> freq = new HashMap<>();
    for (char c : s.toCharArray()) {
        freq.merge(c, 1, Integer::sum);
    }
    return freq;
}

// Medium 4: Invert map
public static <K, V> Map<V, List<K>> invertMap(Map<K, V> map) {
    Map<V, List<K>> inverted = new HashMap<>();
    for (Map.Entry<K, V> entry : map.entrySet()) {
        inverted.computeIfAbsent(entry.getValue(), k -> new ArrayList<>()).add(entry.getKey());
    }
    return inverted;
}
```
</details>

---


### Deep-Dive: Bytecode Execution of `map.merge()`
The `Map.merge(key, value, remappingFunction)` default method compiles into a clean, atomic lookup-and-replace loop:
```java
default V merge(K key, V value, BiFunction<? super V, ? super V, ? extends V> remappingFunction) {
    Objects.requireNonNull(remappingFunction);
    Objects.requireNonNull(value);
    V oldValue = get(key);
    V newValue = (oldValue == null) ? value : remappingFunction.apply(oldValue, value);
    if (newValue == null) {
        remove(key);
    } else {
        put(key, newValue);
    }
    return newValue;
}
```
In `ConcurrentHashMap`, `merge()` is overridden to execute atomically under bucket node synchronization without locking the whole table, providing maximum concurrency.

### Detailed Exercise Solutions (All 9 Problems)
<details>
<summary>View Complete Solutions for All Exercises</summary>

```java
// Easy 1: Character frequency
public static Map<Character, Integer> charFreq(String s) {
    Map<Character, Integer> map = new HashMap<>();
    for (char c : s.toCharArray()) map.merge(c, 1, Integer::sum);
    return map;
}

// Easy 2: Word length map
public static Map<String, Integer> wordLengths(List<String> words) {
    Map<String, Integer> map = new HashMap<>();
    for (String w : words) map.put(w, w.length());
    return map;
}

// Easy 3: Filter entries > 50
public static Set<String> keysGreaterThan50(Map<String, Integer> map) {
    Set<String> res = new HashSet<>();
    for (Map.Entry<String, Integer> e : map.entrySet()) {
        if (e.getValue() > 50) res.add(e.getKey());
    }
    return res;
}

// Medium 4: Invert Map
public static <K, V> Map<V, List<K>> invert(Map<K, V> map) {
    Map<V, List<K>> inverted = new HashMap<>();
    for (Map.Entry<K, V> e : map.entrySet()) {
        inverted.computeIfAbsent(e.getValue(), k -> new ArrayList<>()).add(e.getKey());
    }
    return inverted;
}

// Medium 5: Nested Cache
public class NestedCache {
    private final Map<String, Map<String, Object>> store = new HashMap<>();
    public void put(String namespace, String key, Object val) {
        store.computeIfAbsent(namespace, k -> new HashMap<>()).put(key, val);
    }
    public Object get(String namespace, String key) {
        Map<String, Object> ns = store.get(namespace);
        return ns != null ? ns.get(key) : null;
    }
}

// Medium 6: First non-repeating character
public static Character firstNonRepeating(String s) {
    Map<Character, Integer> counts = new LinkedHashMap<>();
    for (char c : s.toCharArray()) counts.merge(c, 1, Integer::sum);
    for (Map.Entry<Character, Integer> e : counts.entrySet()) {
        if (e.getValue() == 1) return e.getKey();
    }
    return null;
}

// Hard 7: Concurrent LRU Cache with TTL
public class ConcurrentLruTtlCache<K, V> {
    private record CachedItem<V>(V val, long expiry) {}
    private final int capacity;
    private final long ttlMs;
    private final java.util.concurrent.ConcurrentHashMap<K, CachedItem<V>> map =
        new java.util.concurrent.ConcurrentHashMap<>();
    public ConcurrentLruTtlCache(int capacity, long ttlMs) { this.capacity = capacity; this.ttlMs = ttlMs; }
    public void put(K key, V val) {
        if (map.size() >= capacity) {
            K eldest = map.keySet().iterator().next();
            map.remove(eldest);
        }
        map.put(key, new CachedItem<>(val, System.currentTimeMillis() + ttlMs));
    }
    public V get(K key) {
        CachedItem<V> item = map.get(key);
        if (item == null) return null;
        if (System.currentTimeMillis() > item.expiry()) {
            map.remove(key);
            return null;
        }
        return item.val();
    }
}

// Hard 8: SetMultiMap
public class SetMultiMap<K, V> {
    private final Map<K, Set<V>> map = new HashMap<>();
    public void put(K key, V val) {
        map.computeIfAbsent(key, k -> new HashSet<>()).add(val);
    }
    public Set<V> get(K key) {
        return map.getOrDefault(key, Set.of());
    }
}

// Hard 9: Sparse Matrix
public class SparseMatrix {
    private record Point(int row, int col) {}
    private final Map<Point, Double> matrix = new HashMap<>();
    public void set(int r, int c, double val) {
        if (val == 0.0) matrix.remove(new Point(r, c));
        else matrix.put(new Point(r, c), val);
    }
    public double get(int r, int c) {
        return matrix.getOrDefault(new Point(r, c), 0.0);
    }
}
```
</details>


## 12. Summary / Cheat Sheet

- **Core Concept**: Unique keys mapped to values; not a `Collection`, but provides views.
- **Collection Views**: `keySet()`, `values()`, `entrySet()` (live backing views).
- **Modern Defaults**: `getOrDefault`, `putIfAbsent`, `computeIfAbsent`, `merge`.
- **Implementations**:
  - `HashMap`: Fast $O(1)$, general-purpose.
  - `LinkedHashMap`: Insertion or access order (LRU).
  - `TreeMap`: Sorted by keys $O(\log N)$.
  - `ConcurrentHashMap`: Thread-safe, non-blocking reads.
- **Iteration**: Always iterate `entrySet()` rather than `keySet()` + `get()`.

---

## 13. Further Reading
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 81 (Prefer concurrency utilities to wait and notify).
- Oracle Java Documentation: *The Map Interface*.
- Goetz, Brian. *Java Concurrency in Practice*: Chapter 5 (Building Blocks - Concurrent Collections).
