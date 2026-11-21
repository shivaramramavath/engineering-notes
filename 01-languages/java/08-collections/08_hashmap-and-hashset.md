# HashMap and HashSet Internals

> Deep architecture of Java's hash table: bitwise index masking, hash spreading function, treeification thresholds, and rehashing mechanics.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [02_set.md](02_set.md), [03_map.md](03_map.md), [04-oop/14_equals-and-hashcode.md](../04-oop/14_equals-and-hashcode.md) |
| **Time to Read** | 26 minutes |
| **Status** | Active |
| **Java Version** | Java 1.2+ (Treeification Java 8+) |

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

`java.util.HashMap` is the primary general-purpose hash table implementation in Java, providing $O(1)$ amortized average time for lookups, insertions, and deletions. `HashSet` is implemented directly on top of `HashMap`, storing elements as keys and mapping them to a dummy object constant.

Prior to Java 8, hash table collisions were resolved strictly via singly-linked bucket chains. Under heavy hash collisions—whether accidental or caused by intentional HashDoS attacks—hash bucket lookups degraded to $O(N)$ linear scans.

In Java 8, `HashMap` underwent a major architectural redesign: when collisions in any bucket exceed a threshold (**TREEIFY_THRESHOLD = 8**) and total table capacity is at least 64, the linked bucket is transformed into a balanced **Red-Black Tree** (`TreeNode`), guaranteeing $O(\log N)$ worst-case time complexity.

---

## 2. Core Concepts

### Core Constants and Parameters
1. **`DEFAULT_INITIAL_CAPACITY = 16`**: Must be a power of two.
2. **`MAXIMUM_CAPACITY = 1 << 30`**: Largest allowed power-of-two table size ($2^{30} = 1,073,741,824$).
3. **`DEFAULT_LOAD_FACTOR = 0.75f`**: Balances time and space costs.
4. **`TREEIFY_THRESHOLD = 8`**: Bin count at which a linked list converts to a tree.
5. **`UNTREEIFY_THRESHOLD = 6`**: Bin count at which a tree converts back to a linked list during resize.
6. **`MIN_TREEIFY_CAPACITY = 64`**: Minimum table capacity before bins are treeified. If capacity is below 64, table is resized instead of treeifying.

### State Fields
- `transient Node<K,V>[] table`: Array of bucket references.
- `transient int size`: Number of key-value pairs stored.
- `int threshold`: Next size value at which to resize (`capacity * loadFactor`).
- `final float loadFactor`: The load factor for the hash table.

---

## 3. How It Works Internally

### 1. Hash Spreading Function
A raw 32-bit `hashCode()` can have any value. Simply using `hash % table.length` would only look at the lower bits, making it vulnerable to poor hash functions that only vary in higher bits.

`HashMap` applies a secondary **perturbation function** (hash spreader):
```java
static final int hash(Object key) {
    int h;
    return (key == null) ? 0 : (h = key.hashCode()) ^ (h >>> 16);
}
```
This XORs the high 16 bits with the low 16 bits of the hash code (`h ^ (h >>> 16)`). This ensures that bits from both upper and lower halves influence bucket calculation.

### 2. Power-of-Two Index Masking
Because `table.length` is strictly guaranteed to be a power of two ($2^k$), computing the bucket index does not use expensive division (`hash % length`). It uses bitwise AND:
```java
index = (table.length - 1) & hash;
```
For example, if `length = 16`, `length - 1 = 15` (`0b00001111`). Bitwise AND masks off the lower 4 bits in a single CPU cycle.

### 3. Treeification Mechanics
```
Normal Bucket (List):
table[i] ---> Node1 ---> Node2 ---> Node3 ... (Up to 7 nodes: O(N) scan)

Treeified Bucket (Red-Black Tree, size >= 8 & table.length >= 64):
table[i] ---> TreeNode (Black Root)
               /        \
         TreeNode (Red)  TreeNode (Black)  (Guaranteed O(log N) lookup)
```

```
+-----------------------------------------------------------------+
| Put Operation Flowchart                                         |
|                                                                 |
| 1. Compute hash = (h = key.hashCode()) ^ (h >>> 16)            |
| 2. Calculate index = (n - 1) & hash                             |
| 3. If table[index] == null:                                     |
|      Create new Node and store.                                 |
| 4. Else (Collision):                                            |
|      If first node key matches -> replace value.                |
|      Else if node is TreeNode -> putTreeVal() [O(log N)].       |
|      Else (LinkedList chain):                                   |
|         Traverse list; if key found -> replace value.           |
|         If end reached -> append new Node.                      |
|         If chain length reaches 8:                              |
|            If table.length < 64 -> resize table!                |
|            Else -> treeifyBin() into Red-Black Tree!            |
| 5. If ++size > threshold -> resize()!                           |
+-----------------------------------------------------------------+
```

### 4. Zero-Modulo Rehashing During Table Doubling
When the table capacity doubles ($N \to 2N$), elements in bucket `i` either:
- Stay at index `i` (if new bit in hash is 0), OR
- Move to index `i + oldCap` (if new bit in hash is 1).

`HashMap` tests `(e.hash & oldCap) == 0`. It splits the bucket into two separate sublists (`loHead`/`loTail` and `hiHead`/`hiTail`) without recalculating hashes or performing modulo division!

---

## 4. Syntax & Usage

### 1. Pre-sizing Capacity to Prevent Rehashing
```java
import java.util.HashMap;
import java.util.Map;

public class PresizedMapDemo {
    public static void main(String[] args) {
        int expectedEntries = 1000;
        // Formula: capacity = ceil(expectedEntries / loadFactor) + 1
        int capacity = (int) Math.ceil(expectedEntries / 0.75f) + 1;

        Map<String, Integer> map = new HashMap<>(capacity);
        for (int i = 0; i < expectedEntries; i++) {
            map.put("key-" + i, i); // Zero resize operations!
        }
    }
}
```

### 2. Modern Compute and Merge Idioms
```java
import java.util.HashMap;
import java.util.Map;

public class ModernMapMethods {
    public static void main(String[] args) {
        Map<String, Integer> scores = new HashMap<>();

        // Add 10 or increment existing score by 10
        scores.merge("Alice", 10, Integer::sum);
        scores.merge("Alice", 15, Integer::sum);
        System.out.println("Alice score: " + scores.get("Alice")); // 25
    }
}
```

---

## 5. Practical Examples

### Example 1: Custom Object Key Contract Implementation (Basic to Intermediate)
```java
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

public final class AccountKey {
    private final String tenantId;
    private final long accountId;

    public AccountKey(String tenantId, long accountId) {
        this.tenantId = Objects.requireNonNull(tenantId);
        this.accountId = accountId;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof AccountKey other)) return false;
        return this.accountId == other.accountId && this.tenantId.equals(other.tenantId);
    }

    @Override
    public int hashCode() {
        int result = tenantId.hashCode();
        result = 31 * result + Long.hashCode(accountId);
        return result;
    }

    public static void main(String[] args) {
        Map<AccountKey, String> map = new HashMap<>();
        AccountKey k1 = new AccountKey("US_WEST", 4001L);
        AccountKey k2 = new AccountKey("US_WEST", 4001L);

        map.put(k1, "Active");
        System.out.println("Lookup by equivalent key: " + map.get(k2)); // "Active"
    }
}
```

### Example 2: Inspecting Collision Behavior (Real-World)
```java
import java.util.HashMap;
import java.util.Map;

public class CollisionAnalysis {
    // Deliberately forced hash collision
    record BadKey(int id) {
        @Override
        public int hashCode() {
            return 42; // All instances hash to same bucket!
        }
    }

    public static void main(String[] args) {
        Map<BadKey, String> map = new HashMap<>();
        for (int i = 0; i < 20; i++) {
            map.put(new BadKey(i), "Val-" + i);
        }
        // In Java 8+, this bucket treeifies instead of remaining an O(N) linked list!
        System.out.println("Retrieved: " + map.get(new BadKey(15))); // Val-15
    }
}
```

### Example 3: Edge Case - Null Key Mapping to Bucket 0
```java
import java.util.HashMap;

public class NullKeyEdgeCase {
    public static void main(String[] args) {
        HashMap<String, Integer> map = new HashMap<>();
        // HashMap explicitly assigns hash = 0 to null keys:
        // (key == null) ? 0 : ...
        map.put(null, 100);

        System.out.println("Null key maps to index 0: " + map.get(null)); // 100
    }
}
```

### Example 4: Failure Case - Missing `hashCode` Overriding
```java
import java.util.HashMap;
import java.util.Map;

public class MissingHashCodeTrap {
    static class BrokenPerson {
        final String name;
        BrokenPerson(String name) { this.name = name; }
        @Override
        public boolean equals(Object o) {
            return o instanceof BrokenPerson bp && this.name.equals(bp.name);
        }
        // FORGOT TO OVERRIDE hashCode()! Uses System.identityHashCode()!
    }

    public static void main(String[] args) {
        Map<BrokenPerson, String> map = new HashMap<>();
        map.put(new BrokenPerson("Bob"), "Engineer");

        // get() produces a different identity hash code, landing in a different bucket!
        System.out.println(map.get(new BrokenPerson("Bob"))); // NULL!
    }
}
```

---

## 6. Best Practices

1. **Always Override `equals` and `hashCode` Together**: Obey the rule: if `a.equals(b)`, then `a.hashCode() == b.hashCode()`.
2. **Make Keys Strictly Immutable**: Mutating a key's fields changes its hash code, corrupting bucket indexing and creating ghost objects.
3. **Pre-calculate Capacity When Known**: Use `(int) (expectedSize / 0.75f) + 1` to prevent dynamic resizing pauses.
4. **Prefer Records or Enums as Keys**: Modern Java `record` automatically generates canonical, correct, and immutable `equals` and `hashCode` implementations.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Overriding `equals` without `hashCode` | Equal objects produce different hashes and land in different buckets; lookups fail. | Override both together. |
| Mutating key fields after insertion | Changes `hashCode`; `get()` looks in wrong bucket, returning `null`. | Keep keys immutable. |
| Using `HashMap` in multi-threaded code | Causes race conditions, lost updates, and corrupted bucket chains. | Use `ConcurrentHashMap`. |
| Choosing load factor too high ($> 0.9$) | Increases bucket collision chains, degrading lookup speed. | Keep default `0.75`. |
| Using arrays as keys (`int[]`, `byte[]`) | Arrays inherit identity `equals` and `hashCode` from `Object`. | Wrap array in a class or use `Arrays.hashCode()` / `Arrays.equals()`. |
| Creating millions of single-entry maps with default size | Allocates unused bucket arrays, exhausting heap. | Use `Map.of()` for small fixed maps. |

---

## 8. Performance & Security Notes

### Performance
- **Bitwise Indexing**: `(n - 1) & hash` takes less than 1 CPU cycle, compared to 15-40 cycles for integer division (`hash % n`).
- **Treeification Cost**: Converting a linked list of 8 nodes to a Red-Black tree allocates `TreeNode` wrappers. However, lookups drop from $O(N)$ to $O(\log N)$, preventing worst-case performance collapse.

### Security
- **HashDoS Attack Mitigation**: Attackers sending thousands of colliding HTTP query parameters once froze web servers running older Java runtimes. Java 8's treeification ensures $O(\log N)$ worst-case lookup, neutralising HashDoS exploits.

---

## 9. Comparison / When to Use What

| Feature | `HashMap` | `LinkedHashMap` | `TreeMap` | `ConcurrentHashMap` |
|---|---|---|---|---|
| **Underlying Structure** | Array of Nodes / Trees | Hash table + Doubly-Linked List | Red-Black Tree | Striped Hash Table (CAS) |
| **Lookup Time** | $O(1)$ avg / $O(\log N)$ worst | $O(1)$ avg | $O(\log N)$ | $O(1)$ avg |
| **Key Ordering** | Unordered | Insertion / Access order | Sorted | Unordered |
| **Null Key** | Permitted (index 0) | Permitted (index 0) | Prohibited | Strictly Prohibited |
| **Thread Safety** | No | No | No | Yes (Thread-safe) |

---

## 10. Interview Questions

<details>
<summary>1. How does `HashMap` compute the bucket index for a key?</summary>
First, it applies the perturbation function `hash = (h = key.hashCode()) ^ (h >>> 16)`. Then, it computes the index using bitwise AND: `index = (table.length - 1) & hash`.
</details>

<details>
<summary>2. Why must the table capacity of `HashMap` always be a power of two?</summary>
Because when capacity $N$ is a power of two, $N - 1$ consists entirely of 1s in binary (e.g., $16 - 1 = 15 = 0b1111$). This enables fast bitwise AND masking (`hash & (N - 1)`), which distributes hash bits evenly across all bucket indices.
</details>

<details>
<summary>3. What is treeification in `HashMap` and when does it occur?</summary>
In Java 8+, when a single bucket accumulates 8 or more elements (`TREEIFY_THRESHOLD = 8`), and total table capacity is at least 64 (`MIN_TREEIFY_CAPACITY`), the bucket's linked list is converted into a balanced Red-Black Tree (`TreeNode`). This limits worst-case search complexity to $O(\log N)$.
</details>

<details>
<summary>4. What happens when a `HashMap` reaches its threshold?</summary>
It invokes `resize()`, which doubles the table capacity ($N \to 2N$) and redistributes all entries. Elements either remain at their original index or move to `index + oldCapacity`.
</details>

<details>
<summary>5. Why is the default load factor 0.75?</summary>
Based on Poisson distribution probability modeling, a load factor of 0.75 strikes an optimal empirical balance between minimizing memory waste (empty buckets) and avoiding excessive collision chains.
</details>

<details>
<summary>6. How does `HashSet` work internally?</summary>
`HashSet` is a wrapper around an internal `HashMap<E, Object>`. Elements are stored as keys, and a shared dummy `Object PRESENT` constant is stored as the value.
</details>

<details>
<summary>7. What happens if you use a mutable key in a `HashMap` and mutate it?</summary>
The key's hash code changes. Subsequent `get()` calls calculate the new bucket index, which does not match the bucket where the entry was originally stored. The entry becomes unretrievable, creating a memory leak.
</details>

<details>
<summary>8. How does `HashMap` handle null keys?</summary>
If `key == null`, `HashMap` assigns `hash = 0` and stores the entry in `table[0]`.
</details>

---

## 11. Practice Exercises

### Easy
1. Create a `HashMap<String, Integer>`, insert 5 pairs, and look up keys using `getOrDefault`.
2. Write a function that counts word frequencies in a text using `HashMap.merge`.
3. Demonstrate that equal keys overwrite previous values in a `HashMap`.

### Medium
4. Implement a custom key record `Coordinate(int x, int y)` with proper `equals` and `hashCode`, testing it in a `HashMap`.
5. Write a small simulator that deliberately forces 10 hash collisions into a `HashMap` and verify via reflection that the bucket converted to a `TreeNode`.
6. Implement a two-sum algorithm in $O(N)$ time using a `HashMap`.

### Hard
7. Implement a simplified `MyHashMap<K, V>` from scratch supporting power-of-two table sizing, bitwise index masking, separate chaining, and automatic doubling resizing.
8. Design a memory-efficient Robin Hood Hash Map in Java that minimizes lookup variance by shifting entries based on probe sequence length.
9. Implement a lock-free concurrent hash map segment using `AtomicReferenceArray`.

<details>
<summary>Solutions</summary>

```java
// Easy 2: Word frequency with merge
public static Map<String, Integer> wordFrequency(String text) {
    Map<String, Integer> map = new HashMap<>();
    for (String word : text.split("\\s+")) {
        map.merge(word.toLowerCase(), 1, Integer::sum);
    }
    return map;
}

// Medium 6: Two Sum
public static int[] twoSum(int[] nums, int target) {
    Map<Integer, Integer> map = new HashMap<>();
    for (int i = 0; i < nums.length; i++) {
        int complement = target - nums[i];
        if (map.containsKey(complement)) {
            return new int[]{map.get(complement), i};
        }
        map.put(nums[i], i);
    }
    return new int[0];
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Average Complexity**: $O(1)$ `put`, `get`, `remove`.
- **Worst-Case Complexity**: $O(\log N)$ due to Java 8+ treeification.
- **Index Formula**: `(table.length - 1) & ((h = key.hashCode()) ^ (h >>> 16))`.
- **Treeification Rules**: Bin length $\ge 8$ and table capacity $\ge 64$.
- **Growth**: Capacity doubles ($2\times$) when `size > capacity * loadFactor`.
- **Golden Rule**: Always keep map keys immutable and override `equals` + `hashCode` together!

---

## 13. Further Reading
- OpenJDK: `java.util.HashMap` source code comments (Cliff Click, Doug Lea, Josh Bloch).
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 11 (Always override hashCode when you override equals).
- Goetz, Brian. *HashDoS Vulnerabilities in Web Platforms*.
