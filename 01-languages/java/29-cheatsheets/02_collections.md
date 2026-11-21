# Java Collections Framework Reference Cheat Sheet

> Complete architectural reference for Collections: List, Set, Queue, Deque, Map, Sequenced Collections (Java 21+), Big-O complexities, fail-fast vs fail-safe iterators, and concurrent data structures.

| Metadata | Details |
| :--- | :--- |
| **Level** | Comprehensive Reference |
| **Prerequisites** | [00_core-language.md](./00_core-language.md) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to 25 LTS |

---

## Table of Contents
1. [Collections Hierarchy & Core Interfaces](#1-collections-hierarchy--core-interfaces)
2. [Sequenced Collections (Java 21+ JEP 431)](#2-sequenced-collections-java-21-jep-431)
3. [Time Complexity Comparison Matrix](#3-time-complexity-comparison-matrix)
4. [List Implementations & Performance](#4-list-implementations--performance)
5. [Set Implementations & Hashing](#5-set-implementations--hashing)
6. [Queue & Deque Implementations](#6-queue--deque-implementations)
7. [Map Implementations & Internals](#7-map-implementations--internals)
8. [Concurrent & Thread-Safe Collections](#8-concurrent--thread-safe-collections)
9. [Unmodifiable & Immutable Collections](#9-unmodifiable--immutable-collections)
10. [Common Mistakes & Best Practices](#10-common-mistakes--best-practices)

---

## 1. Collections Hierarchy & Core Interfaces

```
                          Iterable<E>
                               |
                          Collection<E>
        +----------------------+----------------------+
        |                      |                      |
      List<E>                Set<E>                Queue<E>
        |                      |                      |
 SequencedList<E>        SortedSet<E>               Deque<E>
 (Java 21+)                    |                      |
                         SequencedSet<E>       SequencedCollection<E>
                         (Java 21+)            (Java 21+)

                       Map<K, V> (Independent Root)
                               |
                         SortedMap<K, V>
                               |
                       SequencedMap<K, V> (Java 21+)
```

---

## 2. Sequenced Collections (Java 21+ JEP 431)

Introduces uniform access to first and last elements and reverse-ordered views across lists, sets, and maps:

```java
// Uniform methods on SequencedCollection<E>
E getFirst();
E getLast();
void addFirst(E e);
void addLast(E e);
E removeFirst();
E removeLast();
SequencedCollection<E> reversed();

// SequencedMap<K, V> operations
Map.Entry<K, V> firstEntry();
Map.Entry<K, V> lastEntry();
Map.Entry<K, V> pollFirstEntry();
Map.Entry<K, V> pollLastEntry();
K firstKey();
K lastKey();
SequencedMap<K, V> reversed();
```

---

## 3. Time Complexity Comparison Matrix

| Collection | Get / Access | Search / Contains | Insert (Head) | Insert (Tail) | Insert (Middle) | Remove | Space Complexity |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`ArrayList`** | $O(1)$ | $O(N)$ | $O(N)$ | $O(1)$ amortized | $O(N)$ | $O(N)$ | $O(N)$ |
| **`LinkedList`** | $O(N)$ | $O(N)$ | $O(1)$ | $O(1)$ | $O(N)$ | $O(1)$ at node | $O(N)$ (high node overhead) |
| **`ArrayDeque`** | $O(1)$ peek | $O(N)$ | $O(1)$ amortized | $O(1)$ amortized | N/A | $O(1)$ | $O(N)$ |
| **`HashSet`** | N/A | $O(1)$ average | N/A | $O(1)$ | N/A | $O(1)$ | $O(N)$ |
| **`TreeSet`** | N/A | $O(\log N)$ | N/A | $O(\log N)$ | N/A | $O(\log N)$ | $O(N)$ |
| **`LinkedHashSet`** | N/A | $O(1)$ | N/A | $O(1)$ | N/A | $O(1)$ | $O(N)$ |
| **`HashMap`** | $O(1)$ average | $O(1)$ key lookup | N/A | $O(1)$ put | N/A | $O(1)$ | $O(N)$ |
| **`TreeMap`** | $O(\log N)$ | $O(\log N)$ | N/A | $O(\log N)$ | N/A | $O(\log N)$ | $O(N)$ |
| **`PriorityQueue`** | $O(1)$ peek | $O(N)$ contains | $O(\log N)$ | $O(\log N)$ | N/A | $O(\log N)$ poll | $O(N)$ |

---

## 4. List Implementations & Performance

### `ArrayList` Internals
- Backed by an `Object[] elementData`.
- Default capacity: 10 (when first element is added).
- Growth formula: `newCapacity = oldCapacity + (oldCapacity >> 1)` (1.5x scaling).
- Preferred for 95% of use cases due to CPU L1/L2 cache locality.

### `LinkedList` Internals
- Doubly-linked list with `Node<E>` wrappers (`prev`, `next`, `item`).
- Node overhead: 24 bytes per element on 64-bit JVM.
- Poor cache locality; almost always outperformed by `ArrayList` and `ArrayDeque`.

---

## 5. Set Implementations & Hashing

- **`HashSet`:** Backed by an internal `HashMap<E, Object>` using a static dummy object `PRESENT`. Unordered.
- **`LinkedHashSet`:** Subclass of `HashSet` maintaining a doubly-linked list across entries. Iteration order equals insertion order.
- **`TreeSet`:** Backed by a Red-Black tree (`TreeMap`). Sorted according to natural order or custom `Comparator`.

---

## 6. Queue & Deque Implementations

```java
// ArrayDeque as preferred FIFO Queue & LIFO Stack
Deque<String> deque = new ArrayDeque<>();
deque.offerLast("First"); // Queue enqueue
deque.offerLast("Second");
String item = deque.pollFirst(); // Queue dequeue -> "First"

deque.push("Top"); // Stack push
String popped = deque.pop(); // Stack pop -> "Top"
```

| Method Category | Throws Exception | Returns Special Value (null/false) |
| :--- | :--- | :--- |
| **Insert (Queue)** | `add(e)` | `offer(e)` |
| **Remove (Queue)** | `remove()` | `poll()` |
| **Examine (Queue)** | `element()` | `peek()` |

---

## 7. Map Implementations & Internals

### `HashMap` HotSpot Architecture
- Array of buckets (`Node<K, V>[] table`).
- Key index calculation: `(table.length - 1) & hash(key)`.
- Table capacity is strictly a power of 2 ($2^k$).
- Default load factor: `0.75f`. Resize threshold: $	ext{capacity} 	imes 	ext{loadFactor}$.
- **Treeification:** If a bucket exceeds 8 entries and `table.length >= 64`, bucket converts from linked list to Red-Black Tree (`TreeNode`). Converts back to list when bucket size shrinks to 6 during resize.

---

## 8. Concurrent & Thread-Safe Collections

| Classic Synchronized | Modern Concurrent Alternative | Concurrency Strategy |
| :--- | :--- | :--- |
| `Vector` | `CopyOnWriteArrayList` | Copy-on-write array (fast reads, expensive writes) |
| `Hashtable` | `ConcurrentHashMap` | Synchronized bucket-head nodes + CAS operations |
| `Collections.synchronizedList()` | `ConcurrentLinkedQueue` | Lock-free Michael & Scott queue algorithm |
| `Collections.synchronizedMap()` | `ConcurrentSkipListMap` | Lock-free SkipList (sorted concurrent map) |

---

## 9. Unmodifiable & Immutable Collections

```java
// Java 9+ Immutable Factory Methods (True immutability, null-hostile)
List<String> list = List.of("A", "B", "C");
Set<Integer> set = Set.of(1, 2, 3);
Map<String, Integer> map = Map.of("key1", 100, "key2", 200);

// Copy of an existing collection
List<String> immutableCopy = List.copyOf(existingList);

// Classic unmodifiable view (Wraps existing collection; mutates if underlying collection changes!)
List<String> view = Collections.unmodifiableList(existingList);
```

---

## 10. Common Mistakes & Best Practices

1. **Mutating Key Objects in Maps/Sets:** Modifying fields used in `hashCode()` after an object is added corrupts the bucket index, causing silent lookup failures.
2. **`ConcurrentModificationException`:** Occurs when modifying a collection directly during iteration. Use `iterator.remove()` or `collection.removeIf(predicate)`.
3. **Always Pre-Size Maps:** If expecting $N$ elements, size the map as:
   $$	ext{initialCapacity} = \left\lceil rac{N}{0.75} ightceil + 1$$


---

## 11. Exhaustive Collection Interface Methods Reference

### java.util.Collection<E> Core Methods
| Method | Return Type | Description |
| :--- | :---: | :--- |
| `add(E e)` | `boolean` | Ensures collection contains element |
| `addAll(Collection<? extends E> c)` | `boolean` | Adds all elements from given collection |
| `clear()` | `void` | Removes all elements from collection |
| `contains(Object o)` | `boolean` | True if collection contains element |
| `containsAll(Collection<?> c)` | `boolean` | True if collection contains all specified elements |
| `isEmpty()` | `boolean` | True if size is 0 |
| `iterator()` | `Iterator<E>` | Returns iterator over elements |
| `remove(Object o)` | `boolean` | Removes first matching occurrence |
| `removeAll(Collection<?> c)` | `boolean` | Removes all elements matching collection |
| `removeIf(Predicate<? super E> filter)`| `boolean` | Removes elements matching predicate |
| `retainAll(Collection<?> c)` | `boolean` | Retains only elements present in specified collection |
| `size()` | `int` | Element count (or `Integer.MAX_VALUE` if $> 2^{31}-1$) |
| `toArray()` | `Object[]` | Converts collection to fresh array |
| `toArray(T[] a)` | `T[]` | Converts collection to typed array |
| `stream()` | `Stream<E>` | Returns sequential stream over collection |
| `parallelStream()` | `Stream<E>` | Returns parallel stream over collection |

### java.util.List<E> Positional Methods
| Method | Return Type | Description |
| :--- | :---: | :--- |
| `get(int index)` | `E` | Element at specified index |
| `set(int index, E element)` | `E` | Replaces element at index; returns previous element |
| `add(int index, E element)` | `void` | Inserts element at specified index |
| `remove(int index)` | `E` | Removes element at index; shifts subsequent elements left |
| `indexOf(Object o)` | `int` | First matching index or -1 |
| `lastIndexOf(Object o)` | `int` | Last matching index or -1 |
| `subList(int fromIndex, int toIndex)` | `List<E>` | Returns structural view of sub-range $[fromIndex, toIndex)$ |
| `listIterator()` | `ListIterator<E>` | Bidirectional iterator supporting cursor navigation |
| `sort(Comparator<? super E> c)` | `void` | Sorts list in-place using TimSort |

---

## 12. Deep Dive: HashMap Hashing & Resizing Mechanics

HotSpot `HashMap` implements bucket placement using bitwise masking:
$$	ext{Bucket Index} = (n - 1) \& 	ext{hash}$$
Where $n$ is always a power of 2 ($2^k$).

### Perturbation Function (Spreading Higher Bits)
```java
static final int hash(Object key) {
    int h;
    return (key == null) ? 0 : (h = key.hashCode()) ^ (h >>> 16);
}
```
**Why XOR with `h >>> 16`?**
When the hash map table has size 16 ($n - 1 = 15 = 	ext{0b1111}$), only the lowest 4 bits of the hash code determine the bucket. If two distinct classes produce hash codes differing only in upper bits, they collide completely. XORing the upper 16 bits onto the lower 16 bits diffuses higher-bit entropy down to the lower index bits.

### Split During Resize (Power of 2 Doubling)
When doubling table size from $16$ ($2^4$) to $32$ ($2^5$), the bit mask adds one higher bit ($16 = 	ext{0b10000}$).
- If the new bit in `key.hash` is `0`, the element stays at its **original index** (`lowIndex`).
- If the new bit in `key.hash` is `1`, the element shifts to **`originalIndex + oldCapacity`** (`highIndex`).
This avoids recomputing `hashCode()` entirely during rehashing!

---

## 13. Practical Collections Recipes & Idioms

### Recipe 1: Frequency Counter with Map
```java
Map<String, Integer> wordCounts = new HashMap<>();
for (String word : words) {
    wordCounts.merge(word, 1, Integer::sum);
}
```

### Recipe 2: Grouping by Category with MultiMap Pattern
```java
Map<String, List<User>> usersByCountry = new HashMap<>();
for (User u : users) {
    usersByCountry.computeIfAbsent(u.country(), k -> new ArrayList<>()).add(u);
}
```

### Recipe 3: LRU Cache using LinkedHashMap
```java
public class LruCache<K, V> extends LinkedHashMap<K, V> {
    private final int maxEntries;

    public LruCache(int maxEntries) {
        super(maxEntries, 0.75f, true); // true sets ACCESS-ORDER mode
        this.maxEntries = maxEntries;
    }

    @Override
    protected boolean removeEldestEntry(Map.Entry<K, V> eldest) {
        return size() > maxEntries; // Auto-evicts oldest accessed entry
    }
}
```

### Recipe 4: PriorityQueue Custom Sorting with Comparator
```java
// Max-heap for integers
PriorityQueue<Integer> maxHeap = new PriorityQueue<>(Comparator.reverseOrder());

// Object comparison with composite comparator
PriorityQueue<Task> taskQueue = new PriorityQueue<>(
        Comparator.comparingInt(Task::priority).reversed()
                  .thenComparing(Task::creationTime)
);
```

---

## 14. Quick Summary / Cheat Sheet Card

- **Default Pick:** `ArrayList` for lists, `ArrayDeque` for queues/stacks, `HashMap` for key-value maps, `HashSet` for sets.
- **Java 21 Sequenced Collections:** Use `.getFirst()`, `.getLast()`, and `.reversed()` uniformly across lists, sets, and maps.
- **Thread-Safety:** Never use `Vector` or `Hashtable`. Use `ConcurrentHashMap` for concurrent maps and `CopyOnWriteArrayList` for read-heavy shared lists.
- **Unmodifiable:** Prefer `List.of()`, `Set.of()`, `Map.of()` (immutable, reject nulls) over `Collections.unmodifiableList()`.

---

## 15. Exhaustive Map Interface Methods Reference

| Method | Return Type | Description |
| :--- | :---: | :--- |
| `put(K key, V value)` | `V` | Associates value with key; returns previous value |
| `putIfAbsent(K key, V value)` | `V` | Puts value only if key not present or mapped to null |
| `get(Object key)` | `V` | Value mapped to key or null |
| `getOrDefault(Object key, V def)` | `V` | Value mapped to key or default value |
| `containsKey(Object key)` | `boolean` | True if map contains key |
| `containsValue(Object value)` | `boolean` | True if map contains value |
| `remove(Object key)` | `V` | Removes mapping; returns previous value |
| `remove(Object key, Object val)` | `boolean` | Removes mapping only if currently mapped to given value |
| `replace(K key, V value)` | `V` | Replaces entry only if key is currently mapped |
| `replace(K key, V oldVal, V newVal)`| `boolean` | Replaces entry only if currently mapped to oldVal |
| `compute(K key, BiFunction)` | `V` | Recomputes mapping using (key, oldVal) |
| `computeIfAbsent(K key, Function)` | `V` | Computes value if key not present |
| `computeIfPresent(K key, BiFunction)`| `V` | Computes new value only if key is currently present |
| `merge(K key, V value, BiFunction)` | `V` | Merges value with existing value or inserts |
| `keySet()` | `Set<K>` | Set view of keys |
| `values()` | `Collection<V>` | Collection view of values |
| `entrySet()` | `Set<Map.Entry<K,V>>`| Set view of key-value mappings |
| `forEach(BiConsumer)` | `void` | Performs action for each entry |
| `replaceAll(BiFunction)` | `void` | Replaces each entry's value with result of function |

---

## 16. NavigableSet and NavigableMap Navigation Reference

```java
NavigableMap<Integer, String> tree = new TreeMap<>();
tree.put(10, "Ten");
tree.put(20, "Twenty");
tree.put(30, "Thirty");

// Proximity Queries
tree.lowerKey(20);   // 10 (strictly < 20)
tree.floorKey(20);   // 20 (<= 20)
tree.ceilingKey(20); // 20 (>= 20)
tree.higherKey(20);  // 30 (strictly > 20)

// Sub-range Views
NavigableMap<Integer, String> sub = tree.subMap(10, true, 30, false); // [10, 30)
NavigableMap<Integer, String> rev = tree.descendingMap(); // Reversed view
```

---

## 17. ConcurrentHashMap Internal Locking Architecture

```
+-------------------------------------------------------------------------+
|                  ConcurrentHashMap Table (Node<K, V>[])                 |
|  [Bucket 0]   [Bucket 1]   [Bucket 2]   [Bucket 3]   ...   [Bucket N]   |
+-------------------------------------------------------------------------+
       |             |
       v             v
  [CAS Insert]  [synchronized(firstNode)]
  (Lock-Free)   (Bin-level lock: ONLY threads contending on Bucket 1 block!)
```
- **Empty Bucket Insertions:** Uses Lock-Free Compare-And-Swap (`Unsafe.compareAndSetReference` / `VarHandle`).
- **Collided Bucket Insertions:** Synchronizes strictly on the **first node** of the bucket chain. Threads inserting into different buckets never block each other.
- **Reads (`get()`):** Completely lock-free via volatile field semantics.

---

## 18. Specialized Collections: EnumSet, EnumMap & BitSet

### EnumSet & EnumMap
For keys or elements that are `enum` constants, `EnumSet` and `EnumMap` provide unmatched performance and compactness:
- **`EnumSet`:** Internally backed by a single `long` bitmask (`RegularEnumSet` for $\le 64$ enum elements) or a `long[]` array (`JumboEnumSet`). Operations (`contains`, `add`, `remove`) compile to single-cycle bitwise CPU instructions.
- **`EnumMap`:** Backed by a compact array indexed directly by the enum's `ordinal()`. No hash calculations or bucket collision chains.

```java
public enum Role { ADMIN, MANAGER, DEVELOPER, TESTER, GUEST }

public class EnumCollectionsExample {
    public static void main(String[] args) {
        // High-performance bitmask enum set
        EnumSet<Role> technicalTeam = EnumSet.of(Role.DEVELOPER, Role.TESTER);
        EnumSet<Role> allRoles = EnumSet.allOf(Role.class);

        // Array-indexed enum map
        EnumMap<Role, String> roleDescriptions = new EnumMap<>(Role.class);
        roleDescriptions.put(Role.ADMIN, "Full system access");
        roleDescriptions.put(Role.DEVELOPER, "Code and commit access");
    }
}
```

### BitSet for High-Density Boolean Arrays
`java.util.BitSet` stores dense vectors of boolean flags packed into 64-bit `long` words, consuming 1 bit per boolean rather than 1 byte (`boolean[]`).
```java
BitSet bitSet = new BitSet();
bitSet.set(42);
bitSet.set(100);
boolean isSet = bitSet.get(42); // true
int cardinality = bitSet.cardinality(); // 2 set bits
```
