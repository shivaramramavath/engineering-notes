# The Set Interface and Implementations

> Mathematical set abstraction in Java: uniqueness invariants, hash-based vs tree-based vs sequenced implementations, and contract rules.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_collection-hierarchy.md](00_collection-hierarchy.md), [04-oop/14_equals-and-hashcode.md](../04-oop/14_equals-and-hashcode.md) |
| **Time to Read** | 23 minutes |
| **Status** | Active |
| **Java Version** | Java 1.2+ (Set.of Java 9, SequencedSet Java 21 LTS) |

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

A `java.util.Set` models the mathematical set abstraction: a collection that contains **no duplicate elements**. More formally, a `Set` contains no pair of elements `e1` and `e2` such that `e1.equals(e2)`, and at most one `null` element (depending on implementation).

Sets are indispensable across engineering systems for:
- Deduplicating streams or databases
- Membership testing in $O(1)$ time (`contains`)
- Mathematical set operations: union, intersection, set difference
- Tracking visited nodes in graph traversals

Modern Java provides three primary general-purpose implementations (`HashSet`, `LinkedHashSet`, `TreeSet`), specialized high-performance variants (`EnumSet`), and immutable factories (`Set.of()`).

---

## 2. Core Concepts

### Core Implementations
1. **`HashSet`**: Backed by a `HashMap`. Offers $O(1)$ average time for `add`, `remove`, and `contains`. Provides **no ordering guarantee**.
2. **`LinkedHashSet`**: Extends `HashSet` with a doubly-linked list running through all entries. Maintains **insertion order** (implements `SequencedSet` in Java 21).
3. **`TreeSet`**: Backed by a `TreeMap` (Red-Black tree). Elements ordered according to natural order or a custom `Comparator`. Guarantees $O(\log N)$ time for operations. Implements `NavigableSet` and `SequencedSet`.
4. **`EnumSet`**: Specialized high-performance set for `enum` types. Internally represented as a single `long` bitmask.

### Mathematical Set Operations via JCF
The `Collection` interface methods map directly to standard set algebra:
- `setA.addAll(setB)` $\to$ **Union** ($A \cup B$)
- `setA.retainAll(setB)` $\to$ **Intersection** ($A \cap B$)
- `setA.removeAll(setB)` $\to$ **Asymmetric Difference** ($A \setminus B$)
- `setA.containsAll(setB)` $\to$ **Subset Test** ($B \subseteq A$)

---

## 3. How It Works Internally

### Backing Map Pattern
Almost all standard set implementations in Java do not implement their own hashing or tree algorithms from scratch. Instead, they **wrap their corresponding Map implementation**:

```
+-------------------------------------------------------+
|  HashSet<E>                                           |
|                                                       |
|  Backing Field: private transient HashMap<E, Object> map;
|  Dummy Value:   private static final Object PRESENT = new Object();
|                                                       |
|  add(e)      ---> map.put(e, PRESENT) == null         |
|  contains(e) ---> map.containsKey(e)                  |
|  remove(e)   ---> map.remove(e) == PRESENT            |
+-------------------------------------------------------+
```
Every `HashSet` allocates a single shared dummy `Object PRESENT` instance used as the value for all map keys.

### Mutual Mutability Hazard
If an object is stored in a `HashSet` and subsequently mutated such that its `equals` or `hashCode` changes:
- Its hash bucket position becomes incorrect.
- `set.contains(object)` returns `false` even though the object is physically inside the set!
- Memory leaks and ghost entries occur.

---

## 4. Syntax & Usage

### 1. Creation and Modern Factories
```java
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.TreeSet;

public class SetCreationDemo {
    public static void main(String[] args) {
        // 1. Unordered HashSet
        Set<String> hashSet = new HashSet<>();

        // 2. Insertion-ordered LinkedHashSet (SequencedSet in Java 21)
        Set<String> linkedSet = new LinkedHashSet<>();

        // 3. Sorted TreeSet
        Set<Integer> sortedSet = new TreeSet<>();

        // 4. Immutable Set (Java 9+)
        Set<String> immutable = Set.of("apple", "banana", "cherry");
    }
}
```

### 2. Performing Mathematical Set Operations
```java
import java.util.HashSet;
import java.util.Set;

public class SetAlgebraDemo {
    public static void main(String[] args) {
        Set<Integer> a = new HashSet<>(Set.of(1, 2, 3, 4));
        Set<Integer> b = Set.of(3, 4, 5, 6);

        // Union
        Set<Integer> union = new HashSet<>(a);
        union.addAll(b);
        System.out.println("Union: " + union); // [1, 2, 3, 4, 5, 6]

        // Intersection
        Set<Integer> intersection = new HashSet<>(a);
        intersection.retainAll(b);
        System.out.println("Intersection: " + intersection); // [3, 4]

        // Difference (A \ B)
        Set<Integer> difference = new HashSet<>(a);
        difference.removeAll(b);
        System.out.println("Difference: " + difference); // [1, 2]
    }
}
```

---

## 5. Practical Examples

### Example 1: Stream Deduplication with Encounter Order (Basic)
```java
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public class DeduplicationDemo {
    public static <T> List<T> deduplicatePreservingOrder(List<T> input) {
        // LinkedHashSet retains first encounter order
        Set<T> seen = new LinkedHashSet<>(input);
        return List.copyOf(seen);
    }

    public static void main(String[] args) {
        List<String> items = List.of("apple", "orange", "apple", "banana", "orange");
        System.out.println("Deduplicated: " + deduplicatePreservingOrder(items));
        // Output: [apple, orange, banana]
    }
}
```

### Example 2: Finding Nearest Lower/Higher Elements with `NavigableSet` (Real-World)
```java
import java.util.NavigableSet;
import java.util.TreeSet;

public class PriceThresholdMatcher {
    public static void main(String[] args) {
        NavigableSet<Integer> tiers = new TreeSet<>(Set.of(100, 250, 500, 1000));

        System.out.println("Lower than 250: " + tiers.lower(250));     // 100
        System.out.println("Floor of 250: " + tiers.floor(250));       // 250
        System.out.println("Ceiling of 300: " + tiers.ceiling(300));   // 500
        System.out.println("Higher than 500: " + tiers.higher(500));   // 1000
    }
}
```

### Example 3: Edge Case - `TreeSet` Uses `compareTo`, Not `equals`!
```java
import java.util.TreeSet;

public class TreeSetEqualityEdgeCase {
    record Person(String name, int age) implements Comparable<Person> {
        @Override
        public int compareTo(Person o) {
            // Compares ONLY age!
            return Integer.compare(this.age, o.age);
        }
    }

    public static void main(String[] args) {
        TreeSet<Person> set = new TreeSet<>();
        set.add(new Person("Alice", 30));
        boolean addedBob = set.add(new Person("Bob", 30));

        // Bob was NOT added because compareTo returns 0, even though equals() is false!
        System.out.println("Was Bob added? " + addedBob); // false
        System.out.println("Size: " + set.size());        // 1
    }
}
```

### Example 4: Failure Case - Mutating Stored Set Keys
```java
import java.util.HashSet;
import java.util.Objects;
import java.util.Set;

public class MutableSetKeyFailure {
    static class MutableKey {
        int id;
        MutableKey(int id) { this.id = id; }
        @Override
        public boolean equals(Object o) {
            return o instanceof MutableKey m && this.id == m.id;
        }
        @Override
        public int hashCode() { return Objects.hash(id); }
    }

    public static void main(String[] args) {
        Set<MutableKey> set = new HashSet<>();
        MutableKey key = new MutableKey(42);
        set.add(key);

        key.id = 99; // MUTATING HASHCODE AFTER INSERTION!

        // Key is physically inside set, but contains() returns false!
        System.out.println("Contains: " + set.contains(key)); // false! GHOST OBJECT!
    }
}
```

---

## 6. Best Practices

1. **Ensure Set Elements are Immutable**: Never mutate fields that participate in `hashCode()` or `equals()` after an element is added to a `Set`.
2. **Keep `compareTo` Consistent with `equals` in `TreeSet`**: `TreeSet` violates the general `Set` contract if `(x.compareTo(y) == 0) != x.equals(y)`.
3. **Use `EnumSet` for Enums**: `EnumSet` uses a bit-vector internally; operations compile into bitwise machine instructions that run orders of magnitude faster than `HashSet`.
4. **Pre-size `HashSet` for Bulk Insertions**: Initialize with `new HashSet<>((int) (expectedSize / 0.75f) + 1)` to prevent repeated rehashing.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Mutating element fields after adding to `HashSet` | Changes hash code; element becomes unfindable in bucket. | Make set elements immutable. |
| Inconsistent `compareTo` and `equals` in `TreeSet` | `TreeSet` drops distinct elements whose `compareTo` evaluates to 0. | Ensure `compareTo` considers all identifying fields. |
| Overriding `equals` without `hashCode` | Two equal objects land in different hash buckets, resulting in duplicate entries. | Always override both together. |
| Passing duplicate elements to `Set.of(...)` | Throws `IllegalArgumentException` at runtime immediately. | Deduplicate before calling `Set.of()`. |
| Expecting `HashSet` to preserve order | `HashSet` iteration order is non-deterministic and changes on rehashing. | Use `LinkedHashSet` if insertion order is needed. |
| Using `TreeSet` with non-`Comparable` types without a `Comparator` | Throws `ClassCastException` on `add()`. | Pass explicit `Comparator` to constructor. |

---

## 8. Performance & Security Notes

### Performance
- **Time Complexity Matrix**:
  - `HashSet`: $O(1)$ add, remove, contains.
  - `LinkedHashSet`: $O(1)$ add, remove, contains (slightly slower iteration due to linked-list pointers).
  - `TreeSet`: $O(\log N)$ add, remove, contains.
  - `EnumSet`: $O(1)$ bitwise operations.
- **Memory Footprint**: `HashSet` wraps `HashMap`, allocating an entry node for each element (~32 bytes per element overhead).

### Security
- **HashDoS Vulnerability**: If untrusted input can manipulate hash collisions, malicious payloads can force hash buckets to degenerate into linked lists or trees. Java 8+ mitigates this by converting deep buckets into red-black trees when collisions exceed threshold 8.

---

## 9. Comparison / When to Use What

| Implementation | Ordering | Performance | Nulls Allowed? | Best Use Case |
|---|---|---|---|---|
| `HashSet` | None | $O(1)$ | Yes (1 null) | High-speed deduplication, membership testing |
| `LinkedHashSet` | Insertion order | $O(1)$ | Yes (1 null) | Predictable iteration, LRU caches |
| `TreeSet` | Sorted order | $O(\log N)$ | No (throws NPE) | Range queries, ceiling/floor lookups |
| `EnumSet` | Enum ordinal | $O(1)$ bitwise | No | Enum flags, bitsets |
| `Set.of(...)` | Undefined | $O(1)$ | No | Fixed, immutable lookup sets |

---

## 10. Interview Questions

<details>
<summary>1. How does `HashSet` ensure element uniqueness internally?</summary>
`HashSet` delegates to an internal `HashMap`. When `add(e)` is called, it executes `map.put(e, PRESENT)`. If the key already exists, `HashMap` replaces the value and returns the old value (non-null), allowing `HashSet.add()` to return `false`.
</details>

<details>
<summary>2. Why does `TreeSet` not use `equals()` to check for duplicates?</summary>
`TreeSet` relies strictly on `Comparable.compareTo()` or the supplied `Comparator.compare()`. If `compare(a, b) == 0`, the two elements are deemed identical according to the tree's ordering, and the second element is rejected.
</details>

<details>
<summary>3. Why does `Set.of("a", "a")` throw an exception instead of deduplicating?</summary>
Java designers intentionally throw `IllegalArgumentException` to alert the developer to an obvious bug or logic error in literal initialization code.
</details>

<details>
<summary>4. What is the performance characteristic of `EnumSet`?</summary>
`EnumSet` is represented internally by a bit vector (a single `long` for enums with up to 64 values in `RegularEnumSet`). Operations like `contains`, `addAll`, and `removeAll` execute as single-cycle bitwise CPU instructions (`AND`, `OR`, `NOT`).
</details>

<details>
<summary>5. Can a `TreeSet` contain `null`?</summary>
No. In modern Java, calling `treeSet.add(null)` throws `NullPointerException` because `null` cannot be compared to other elements.
</details>

<details>
<summary>6. How does Java 21 Sequenced Collections affect `LinkedHashSet`?</summary>
`LinkedHashSet` implements `SequencedSet`, unlocking `addFirst()`, `addLast()`, `getFirst()`, `getLast()`, and `reversed()`.
</details>

<details>
<summary>7. What happens if you add an object to a `HashSet` and then modify its fields?</summary>
Its hash code changes, making it impossible for the `HashSet` to find the object in its original bucket. The object becomes an unsearchable "ghost" object, causing memory leaks and incorrect `contains` results.
</details>

<details>
<summary>8. How do you find the intersection of two sets in Java?</summary>
`Set<T> intersection = new HashSet<>(setA); intersection.retainAll(setB);`.
</details>

---

## 11. Practice Exercises

### Easy
1. Write a function that takes a list of strings and returns `true` if all elements are unique using a `Set`.
2. Create an `EnumSet` for standard HTTP methods (`GET`, `POST`, `PUT`, `DELETE`) and test membership.
3. Print all elements of a `TreeSet<String>` in reverse order.

### Medium
4. Implement a symmetric difference utility `public static <T> Set<T> symmetricDifference(Set<T> a, Set<T> b)` using only standard `Set` methods.
5. Create a `NavigableSet` of transaction timestamps and retrieve all transactions that occurred within a given time window using `subSet`.
6. Write a custom `CaseInsensitiveSet` that stores strings without regard to casing using `TreeSet`.

### Hard
7. Implement an in-memory inverted search index using `Map<String, Set<Long>>` where search queries perform fast multi-term intersection.
8. Design a concurrent, memory-bounded sliding window deduplicator that expires items after 10 seconds.
9. Implement a custom `BitSet`-backed `SequencedSet` for unsigned 16-bit integers with zero per-node heap allocations.

<details>
<summary>Solutions</summary>

```java
// Easy 1: Uniqueness check
public static <T> boolean areAllUnique(List<T> list) {
    return new HashSet<>(list).size() == list.size();
}

// Medium 4: Symmetric Difference ((A \ B) U (B \ A))
public static <T> Set<T> symmetricDifference(Set<T> a, Set<T> b) {
    Set<T> result = new HashSet<>(a);
    result.addAll(b); // A U B
    Set<T> intersection = new HashSet<>(a);
    intersection.retainAll(b); // A ∩ B
    result.removeAll(intersection); // (A U B) \ (A ∩ B)
    return result;
}
```
</details>

---


### Deep-Dive: Memory Layout of HashSet
Because `HashSet` wraps `HashMap`, each stored element is actually the key in a `HashMap.Node` object:
```
HashSet Memory Breakdown (per element):
- Node Object Header: 12 bytes
- Key Reference:       4 bytes
- Value Reference:     4 bytes (points to static PRESENT)
- Next Reference:      4 bytes (for hash collisions)
- Hash (int):          4 bytes
- Alignment:           4 bytes
Total per element:    32 bytes!
```
Compare this with `BitSet` (0.125 bytes per flag) or `EnumSet` (single 8-byte primitive for 64 flags). When modeling high-density sets of integers or enums, `HashSet` introduces substantial memory overhead.

### Detailed Exercise Solutions (All 9 Problems)
<details>
<summary>View Complete Solutions for All Exercises</summary>

```java
// Easy 1: Uniqueness check
public static <T> boolean areAllUnique(List<T> list) {
    return new HashSet<>(list).size() == list.size();
}

// Easy 2: EnumSet HTTP methods
enum HttpMethod { GET, POST, PUT, DELETE, PATCH }
EnumSet<HttpMethod> safeMethods = EnumSet.of(HttpMethod.GET);
boolean isSafe = safeMethods.contains(HttpMethod.POST); // false

// Easy 3: Reverse TreeSet traversal
TreeSet<String> words = new TreeSet<>(List.of("alpha", "beta", "gamma"));
for (String word : words.descendingSet()) {
    System.out.print(word + " ");
}
System.out.println();

// Medium 4: Symmetric Difference
public static <T> Set<T> symmetricDiff(Set<T> a, Set<T> b) {
    Set<T> union = new HashSet<>(a);
    union.addAll(b);
    Set<T> intersection = new HashSet<>(a);
    intersection.retainAll(b);
    union.removeAll(intersection);
    return union;
}

// Medium 5: NavigableSet Range window
public static Set<Long> getTimestampsInRange(NavigableSet<Long> set, long start, long end) {
    return set.subSet(start, true, end, true);
}

// Medium 6: CaseInsensitiveSet
public static NavigableSet<String> createCaseInsensitiveSet() {
    return new TreeSet<>(String.CASE_INSENSITIVE_ORDER);
}

// Hard 7: Inverted search index
public class InvertedIndex {
    private final Map<String, Set<Long>> index = new HashMap<>();
    public void addDocument(long docId, String text) {
        for (String word : text.toLowerCase().split("\\W+")) {
            index.computeIfAbsent(word, k -> new HashSet<>()).add(docId);
        }
    }
    public Set<Long> searchAll(List<String> terms) {
        if (terms.isEmpty()) return Set.of();
        Set<Long> result = null;
        for (String term : terms) {
            Set<Long> docs = index.getOrDefault(term.toLowerCase(), Set.of());
            if (result == null) result = new HashSet<>(docs);
            else result.retainAll(docs);
        }
        return result != null ? result : Set.of();
    }
}

// Hard 8: Sliding Window Deduplicator with Expiry
public class ExpiringDeduplicator<T> {
    private record Entry<T>(T item, long expiryMs) {}
    private final Map<T, Long> map = new java.util.concurrent.ConcurrentHashMap<>();
    private final long ttlMs;
    public ExpiringDeduplicator(long ttlMs) { this.ttlMs = ttlMs; }
    public boolean addIfAbsent(T item) {
        long now = System.currentTimeMillis();
        Long exp = map.get(item);
        if (exp != null && exp > now) return false;
        map.put(item, now + ttlMs);
        return true;
    }
}

// Hard 9: BitSet-backed SequencedSet for unsigned 16-bit ints
public class ShortSet {
    private final java.util.BitSet bitSet = new java.util.BitSet(65536);
    public void add(int val) { bitSet.set(val & 0xFFFF); }
    public boolean contains(int val) { return bitSet.get(val & 0xFFFF); }
    public int size() { return bitSet.cardinality(); }
}
```
</details>


## 12. Summary / Cheat Sheet

- **Core Contract**: No duplicate elements (`!e1.equals(e2)`).
- **HashSet**: Fast $O(1)$, unordered, backed by `HashMap`.
- **LinkedHashSet**: Fast $O(1)$, insertion-ordered, implements `SequencedSet`.
- **TreeSet**: Sorted $O(\log N)$, backed by Red-Black tree; uses `compareTo()`.
- **EnumSet**: Ultra-fast bitmask representation for enum types.
- **Set Algebra**: `addAll` (Union), `retainAll` (Intersection), `removeAll` (Difference).
- **Immutability Warning**: Never mutate fields used in `equals`/`hashCode` after insertion!

---

## 13. Further Reading
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 11 (Always override hashCode when you override equals).
- Oracle Java Documentation: *The Set Interface*.
- Goetz, Brian. *Java Theory and Practice: Hashing it out*.
