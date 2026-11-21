# Specialized Collections

> Extreme performance and niche contracts: EnumSet bitmasks, EnumMap flat arrays, IdentityHashMap reference equality, WeakHashMap memory caches, and BitSet bit vectors.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [02_set.md](02_set.md), [03_map.md](03_map.md), [04-oop/11_enums.md](../04-oop/11_enums.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 1.2+ (EnumSet/EnumMap Java 5+) |

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

The general-purpose collections (`ArrayList`, `HashMap`, `HashSet`) are designed for maximum flexibility, handling arbitrary reference types and dynamically resizing heap memory. However, specific engineering domains require radically different performance profiles or semantics:
- **Enum-based operations**: Knowing that the universe of keys is fixed and numbered enables ultra-compact bit-vectors and direct array indexing.
- **Reference equality**: Frameworks (like serialization or bytecode instrumentation) need to map object instances by **memory address identity** (`==`) rather than logical equality (`equals`).
- **Memory-sensitive caches**: Caching metadata that should automatically disappear when no longer strongly referenced elsewhere in the application.
- **Dense bit arrays**: Storing millions of binary flags with negligible memory overhead.

Java provides specialized collections tailored for these exact workloads: `EnumSet`, `EnumMap`, `IdentityHashMap`, `WeakHashMap`, and `BitSet`.

---

## 2. Core Concepts

### 1. `EnumSet`
A specialized `Set` implementation for use with enum types. All elements must come from a single enum type. Internally represented as a **bit vector** (a single `long` bitmask for $\le 64$ enum constants). Operations execute in single CPU clock cycles.

### 2. `EnumMap`
A specialized `Map` implementation for use with enum keys. Internally represented as an extremely compact, contiguous **array indexed by the enum's ordinal**. No hashing, no collisions, no linked lists, zero pointer overhead.

### 3. `IdentityHashMap`
A `Map` implementation that violates the standard `Map` general contract intentionally: it compares keys and values using **reference equality (`==`)** instead of `equals()`. Hashes are computed using `System.identityHashCode(k)`.

### 4. `WeakHashMap`
A hash table where keys are wrapped in `java.lang.ref.WeakReference`. If a key is no longer strongly referenced by any live thread, the garbage collector reclaims it, and its entry is automatically purged from the map.

### 5. `BitSet`
A growable array of bits represented by a `long[]` array. Each bit represents a boolean flag, consuming exactly **1 bit of memory** (compared to 8-32 bits for `boolean` or `Boolean` objects).

---

## 3. How It Works Internally

### 1. `EnumSet`: `RegularEnumSet` vs `JumboEnumSet`
`EnumSet` is an abstract class with two package-private subclasses:
- **`RegularEnumSet`**: Used when the enum universe has $\le 64$ constants. It stores the entire set inside a single primitive `long elements` field (64 bits). Adding an element is a single bitwise OR: `elements |= (1L << e.ordinal())`. Membership test is a single bitwise AND: `(elements & (1L << e.ordinal())) != 0`.
- **`JumboEnumSet`**: Used when the enum has $> 64$ constants. Backed by a `long[] elements` array.

```
RegularEnumSet Bit Vector (Single 64-bit primitive long):
Bit Index:  [ 63 ] ... [ 3 ][ 2 ][ 1 ][ 0 ]
Flags:      [  0 ] ... [ 1 ][ 0 ][ 1 ][ 1 ]
Enums:                 READ      WRITE EXECUTE
```

### 2. `IdentityHashMap`: Flat Linear-Probing Table
`IdentityHashMap` does not use separate chaining or bucket nodes. It stores keys and values in a single interleaved flat array: `Object[] table`.
- Index $2i$ stores Key $i$.
- Index $2i + 1$ stores Value $i$.
- Collisions are resolved using **linear probing** (stepping sequentially to the next adjacent slot).
- Identity check: `if (table[i] == key)`.

### 3. `WeakHashMap` and `ReferenceQueue`
In `WeakHashMap`, table entries extend `WeakReference<K>`:
```java
private static class Entry<K,V> extends WeakReference<Object> implements Map.Entry<K,V> {
    V value;
    final int hash;
    Entry<K,V> next;
    Entry(Object key, V value, ReferenceQueue<Object> queue, int hash, Entry<K,V> next) {
        super(key, queue); // Registers with ReferenceQueue!
        this.value = value;
        this.hash = hash;
        this.next = next;
    }
}
```
When GC determines a key is weakly reachable, it clears the reference and places the `Entry` into the associated `ReferenceQueue`. On subsequent map operations (`get`, `put`, `size`), `WeakHashMap` polls the queue and purges dead entries.

---

## 4. Syntax & Usage

### 1. Ultra-Fast Flag Checking with `EnumSet`
```java
import java.util.EnumSet;
import java.util.Set;

public class EnumSetDemo {
    public enum Permission { READ, WRITE, EXECUTE, DELETE }

    public static void main(String[] args) {
        // Create set of all permissions
        Set<Permission> all = EnumSet.allOf(Permission.class);

        // Specific flags
        Set<Permission> readOnly = EnumSet.of(Permission.READ);

        // Range of enums
        Set<Permission> standard = EnumSet.range(Permission.READ, Permission.EXECUTE);

        System.out.println("Has WRITE: " + standard.contains(Permission.WRITE)); // true
    }
}
```

### 2. Zero-Collision Mapping with `EnumMap`
```java
import java.util.EnumMap;
import java.util.Map;

public class EnumMapDemo {
    public enum Day { MON, TUE, WED, THU, FRI, SAT, SUN }

    public static void main(String[] args) {
        Map<Day, String> schedule = new EnumMap<>(Day.class);
        schedule.put(Day.MON, "Team Sync");
        schedule.put(Day.FRI, "Release Deployment");

        System.out.println("Monday task: " + schedule.get(Day.MON)); // Team Sync
    }
}
```

### 3. Bit Manipulation with `BitSet`
```java
import java.util.BitSet;

public class BitSetDemo {
    public static void main(String[] args) {
        BitSet primeFlags = new BitSet();
        primeFlags.set(2);
        primeFlags.set(3);
        primeFlags.set(5);
        primeFlags.set(7);

        System.out.println("Is 5 prime? " + primeFlags.get(5)); // true
        System.out.println("Is 6 prime? " + primeFlags.get(6)); // false
        System.out.println("Total primes set: " + primeFlags.cardinality()); // 4
    }
}
```

---

## 5. Practical Examples

### Example 1: Object Graph Cycle Detector using `IdentityHashMap` (Basic to Intermediate)
```java
import java.util.IdentityHashMap;
import java.util.Map;

public class GraphCycleDetector {
    public static class Node {
        String name;
        Node next;
        public Node(String name) { this.name = name; }
        // Even if equals() returns true for all nodes with the same name,
        // IdentityHashMap distinguishes between distinct instances!
        @Override
        public boolean equals(Object o) {
            return o instanceof Node other && this.name.equals(other.name);
        }
    }

    public static boolean hasCycle(Node head) {
        Map<Node, Boolean> visited = new IdentityHashMap<>();
        Node current = head;
        while (current != null) {
            if (visited.put(current, Boolean.TRUE) != null) {
                return true; // Visited the exact same instance in memory!
            }
            current = current.next;
        }
        return false;
    }

    public static void main(String[] args) {
        Node n1 = new Node("A");
        Node n2 = new Node("A"); // Equal by equals(), but distinct instance!
        n1.next = n2;
        n2.next = n1; // Cycle!

        System.out.println("Has cycle: " + hasCycle(n1)); // true
    }
}
```

### Example 2: Ephemeral Metadata Cache with `WeakHashMap` (Real-World)
```java
import java.util.Map;
import java.util.WeakHashMap;

public class WeakMetadataCache {
    public static void main(String[] args) throws InterruptedException {
        Map<Object, String> cache = new WeakHashMap<>();

        Object key1 = new Object();
        Object key2 = new Object();

        cache.put(key1, "Metadata for Key 1");
        cache.put(key2, "Metadata for Key 2");

        System.out.println("Cache size before GC: " + cache.size()); // 2

        key1 = null; // Sever the strong reference to key1!
        System.gc(); // Suggest garbage collection
        Thread.sleep(100);

        // Accessing the map triggers expunge of reclaimed keys
        System.out.println("Cache size after GC: " + cache.size()); // 1
        System.out.println("Key2 still present: " + cache.get(key2)); // Metadata for Key 2
    }
}
```

### Example 3: Edge Case - `WeakHashMap` Value Holding Strong Reference to Key
```java
import java.util.WeakHashMap;

public class WeakHashMapLeakHazard {
    static class BadValue {
        Object keyRef; // STRONG REFERENCE TO KEY!
        BadValue(Object key) { this.keyRef = key; }
    }

    public static void main(String[] args) {
        WeakHashMap<Object, BadValue> map = new WeakHashMap<>();
        Object key = new Object();
        // The value holds a strong reference back to the key!
        // This prevents the key from ever becoming weakly reachable!
        map.put(key, new BadValue(key));
        key = null; // KEY CANNOT BE RECLAIMED BY GC! MEMORY LEAK!
    }
}
```

### Example 4: Failure Case - Passing Non-Enum Class to `EnumSet`
```java
public class EnumSetCompilationFailure {
    public static void main(String[] args) {
        // EnumSet.allOf(String.class);
        // COMPILE ERROR: Bound mismatch: String is not an enum!
    }
}
```

---

## 6. Best Practices

1. **Always Use `EnumSet` / `EnumMap` Over `HashSet` / `HashMap` for Enums**: Vastly superior memory footprint and CPU execution speed.
2. **Never Let `WeakHashMap` Values Reference Their Keys**: A value holding a strong reference back to its key prevents garbage collection and defeats the entire purpose of `WeakHashMap`.
3. **Use `IdentityHashMap` for Graph Serialization / Topology**: Distinguishes between identical clones and handles self-referential graph loops without relying on mutable `equals()`.
4. **Use `BitSet` for Large Dense Boolean Arrays**: Replaces `boolean[]` (1 byte per item) or `Boolean[]` (16-24 bytes per item) with 1 bit per item ($8\times$ to $192\times$ memory reduction).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Value references key in `WeakHashMap` | Creates strong reference cycle; entry is never GC'd. | Ensure values do not hold key references. |
| Expecting `IdentityHashMap` to respect `equals()` | Compares strictly via `==`; two equal strings land in different slots. | Use standard `HashMap` if logical equality is needed. |
| Using `String` keys in `WeakHashMap` literals | String literals live in JVM String Pool permanently; they are never GC'd. | Use unique heap object instances as keys. |
| Using `boolean[]` for millions of flags | Consumes 1 byte per flag (8 bits), wasting megabytes of heap. | Use `java.util.BitSet`. |
| Using `HashSet` for enum flags | Allocates nodes and performs hashing when a bitwise long suffices. | Use `EnumSet.of(...)`. |
| Passing empty universe to `EnumSet` | Throws `IllegalArgumentException` on some factory methods. | Verify enum has at least one constant. |

---

## 8. Performance & Security Notes

### Performance
- **Bitwise Instruction Fusion**: In `EnumSet`, operations like `setA.retainAll(setB)` compile directly into a single assembly `AND` instruction (`and %rax, %rbx`), executing in sub-nanosecond time.
- **Cache-Line Packing in `BitSet`**: A 64-bit `long` stores 64 boolean flags in contiguous CPU register memory. Bitwise checks run with zero memory indirection.

### Security
- **Identity Spooning**: `IdentityHashMap` uses `System.identityHashCode()`, which is determined by the JVM object header. Do not rely on identity hash codes for cryptographic or security hashing.

---

## 9. Comparison / When to Use What

| Collection | Key Basis | Internal Structure | Best Use Case |
|---|---|---|---|
| `EnumSet` | Enum constant | Bit vector (`long` or `long[]`) | Enum flags, permission masks |
| `EnumMap` | Enum constant | Array indexed by ordinal | Enum-keyed properties / state machines |
| `IdentityHashMap` | Reference identity (`==`) | Interleaved flat array | Object graph serialization, cycle detection |
| `WeakHashMap` | Weak Reference (`==` / `equals`) | Hash table with `WeakReference` | Short-lived metadata caches |
| `BitSet` | Integer bit index | `long[]` array | Prime sieves, Bloom filters, dense flags |

---

## 10. Interview Questions

<details>
<summary>1. How is `EnumSet` implemented internally for an enum with fewer than 64 constants?</summary>
Via `RegularEnumSet`, which stores all set members inside a single 64-bit `long` primitive field (`elements`). Adding, removing, and testing elements execute as single-cycle bitwise operations (`|`, `&`, `~`).
</details>

<details>
<summary>2. Why does `EnumMap` outperform `HashMap`?</summary>
`EnumMap` stores values in a flat array indexed directly by the enum's ordinal (`key.ordinal()`). It requires zero hash computation, has zero collisions, needs no linked lists or trees, and does not rehash.
</details>

<details>
<summary>3. What is the contract difference between `IdentityHashMap` and `HashMap`?</summary>
`HashMap` uses `equals()` for object comparison and `hashCode()` for bucket distribution. `IdentityHashMap` intentionally violates the `Map` contract by using reference equality (`==`) and `System.identityHashCode()`.
</details>

<details>
<summary>4. When should you NOT use `WeakHashMap` for caching?</summary>
1. When values hold strong references back to their keys (causing a leak). 2. When keys are primitive wrappers or String literals (which are cached/pooled by the JVM and never collected). 3. When you need LRU eviction based on access frequency rather than GC memory pressure.
</details>

<details>
<summary>5. How much memory does a `BitSet` save compared to a `boolean[]` array?</summary>
A `boolean[]` array consumes 1 byte (8 bits) per element in HotSpot JVM. A `BitSet` consumes 1 bit per element, providing an $8\times$ reduction in memory footprint.
</details>

<details>
<summary>6. How does `WeakHashMap` know when a key has been garbage collected?</summary>
Each entry extends `WeakReference<K>` registered with an internal `ReferenceQueue`. When the GC reclaims a key, it enqueues the entry. The map polls this queue on operations and purges dead entries.
</details>

<details>
<summary>7. Can you add `null` keys or values to `EnumMap`?</summary>
`null` keys are strictly forbidden (throws `NullPointerException`). `null` values are permitted.
</details>

<details>
<summary>8. How does `JumboEnumSet` differ from `RegularEnumSet`?</summary>
`JumboEnumSet` is used when the enum has more than 64 constants. It uses an array of `long`s (`long[] elements`) to store the bit vector, where bit index is divided across array words.
</details>

---

## 11. Practice Exercises

### Easy
1. Create an `EnumSet` of weekend days (`SATURDAY`, `SUNDAY`) and test if today is a weekend.
2. Initialize an `EnumMap` mapping HTTP status code enums to description strings.
3. Use a `BitSet` to track whether numbers between 0 and 99 have been seen.

### Medium
4. Implement the Sieve of Eratosthenes using `BitSet` to find all prime numbers up to 1,000,000.
5. Build an object graph deep-cloner that uses `IdentityHashMap` to preserve cyclical object references without infinite loops.
6. Create a thread-safe weak listener registry using `WeakHashMap`.

### Hard
7. Implement a Bloom Filter using `BitSet` and multiple independent hashing functions.
8. Design a high-performance bitwise finite state machine for network protocol parsing using `EnumSet`.
9. Implement a custom `SoftHashMap` utilizing `SoftReference` keys/values with explicit memory watermark eviction.

<details>
<summary>Solutions</summary>

```java
// Medium 4: Sieve of Eratosthenes
public static BitSet sieveOfEratosthenes(int max) {
    BitSet isPrime = new BitSet(max + 1);
    isPrime.set(2, max + 1); // Set all to true initially
    for (int p = 2; p * p <= max; p++) {
        if (isPrime.get(p)) {
            for (int i = p * p; i <= max; i += p) {
                isPrime.clear(i);
            }
        }
    }
    return isPrime;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **EnumSet**: Bit-vector representation; single `long` for $\le 64$ enums.
- **EnumMap**: Flat array indexed by `ordinal()`; zero collisions.
- **IdentityHashMap**: Reference equality (`==`) using `System.identityHashCode()`.
- **WeakHashMap**: Weak-reference keys; entries auto-purged on GC. (Never let value reference key!).
- **BitSet**: 1 bit per boolean flag; dense, fast bitwise CPU operations.

---

## 13. Further Reading
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 36 (Use EnumSet instead of bit fields) and Item 37 (Use EnumMap instead of ordinal indexing).
- Oracle Java Documentation: *The EnumSet and EnumMap Classes*.
- Goetz, Brian. *Plumbing Java References: Weak, Soft, and Phantom*.
