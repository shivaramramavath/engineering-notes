# HashMap and HashSet

`HashMap<K, V>` is the workhorse of Java: key-value lookup in **O(1) on average**. `HashSet<E>` is a thin wrapper around it. Both depend on `hashCode` and `equals` of the keys, so understanding the mechanism explains most "my map lost my entry" bugs and a classic interview topic.

```java
Map<String, Integer> ages = new HashMap<>();
ages.put("Ada", 36);
ages.get("Ada");              // 36, found without scanning
Set<String> seen = new HashSet<>();
seen.add("x");                // true; adding "x" again returns false
```

## The idea: a hash table

Instead of searching, **compute** where the entry should be.

```
key ──► hashCode() ──► spread bits ──► index = hash & (capacity - 1) ──► bucket

 table (capacity 16)
 ┌───┐
 │ 0 │ → null
 │ 1 │ → [k=Ada,v=36] → [k=Bob,v=7]     ← two keys landed in bucket 1: a COLLISION (chained)
 │ 2 │ → null
 │ 3 │ → [k=Eve,v=9]
 │...│
 │15 │ → null
 └───┘
```

| Step | What happens |
|------|--------------|
| 1. `hashCode()` | The key produces an `int` |
| 2. Spread | `h ^ (h >>> 16)` mixes high bits into low bits (so the index uses all bits) |
| 3. Index | `(capacity - 1) & hash`: capacity is always a **power of two**, so this is a fast modulo |
| 4. Bucket | Look at the entries in that bucket |
| 5. `equals` | Compare keys **in that bucket** (first by cached hash, then `==`/`equals`) |

So lookup is: `hashCode` → bucket → a few `equals` calls. A good hash function keeps buckets tiny.

## Internals

```java
transient Node<K,V>[] table;     // the buckets (allocated lazily on the first put)
int size;                        // number of entries
int threshold;                   // capacity * loadFactor: resize when size exceeds this
final float loadFactor;          // default 0.75
int modCount;                    // for fail-fast iterators

static class Node<K,V> { final int hash; final K key; V value; Node<K,V> next; }
```

### Capacity and load factor

| Setting | Default | Meaning |
|---------|---------|---------|
| Initial capacity | **16** | Number of buckets |
| **Load factor** | **0.75** | Resize when `size > capacity × 0.75` |
| Growth | **×2** | Capacity doubles (always a power of two) |

```
size > 12 with capacity 16  →  resize to 32  →  threshold 24  →  resize to 64 ...
```

**Resizing** allocates a new table and redistributes all entries (O(n)); it is amortized over many insertions. Because capacity doubles, each entry either stays at its index or moves by exactly the old capacity, which makes redistribution cheap.

Load factor trade-off: lower = fewer collisions, more memory; higher = less memory, more collisions. 0.75 is a good default; rarely change it.

### Pre-sizing

```java
Map<String, Integer> m = new HashMap<>(expected * 4 / 3 + 1);   // avoids resizes (capacity >= expected / 0.75)
Map<String, Integer> m2 = HashMap.newHashMap(expected);         // Java 19+: does the math for you
```

`new HashMap<>(100)` does **not** hold 100 entries without resizing: it would resize at 75% (the capacity is rounded up to 128, threshold 96).

## Collisions and treeification (Java 8+)

Different keys can map to the same bucket. Entries in one bucket form a linked list; `get` walks it using `equals`.

| Bucket length | Behavior |
|---------------|----------|
| Short (typical) | Linked list: O(1) expected |
| ≥ **8** nodes and table capacity ≥ **64** | Bucket converts to a **red-black tree** (worst case O(log n) instead of O(n)) |
| ≥ 8 nodes but table < 64 | The table is **resized** instead |
| Shrinks to ≤ **6** nodes | Tree converts back to a list |

Treeification protects against **hash-flooding attacks** and poor hash functions. For tree ordering inside a bucket, keys that implement `Comparable` of the same class are compared with it; otherwise, tie-breaking uses identity hash codes, so a bad `hashCode` plus non-comparable keys can still be slow.

## The `equals`/`hashCode` contract in action

```java
record Point(int x, int y) { }                   // generated equals/hashCode: works as a key
Map<Point, String> grid = new HashMap<>();
grid.put(new Point(1, 2), "A");
grid.get(new Point(1, 2));                       // "A"
```

| Rule | Why |
|------|-----|
| Equal keys **must** have equal hash codes | Otherwise they land in different buckets and are never compared |
| `hashCode` should spread values well | Clustering turns O(1) into O(n) / O(log n) |
| `hashCode` must stay the **same** while the key is in the map | Otherwise the entry is lost |
| Use **immutable** keys | Same reason |

```java
class BadKey {
    int id;
    @Override public boolean equals(Object o) { return o instanceof BadKey b && b.id == id; }
    // hashCode NOT overridden → identity hash: equal keys hash differently
}
Map<BadKey, String> m = new HashMap<>();
m.put(new BadKey(1), "x");
m.get(new BadKey(1));                            // null (almost certainly): different bucket

class MutableKey { int id; /* hashCode uses id */ }
MutableKey k = new MutableKey(); k.id = 1;
m.put(k, "x"); k.id = 2;
m.get(k);                                        // null: hash now points to another bucket; entry is stranded
```

Full details: [04-oop/14_equals-and-hashcode.md](../04-oop/14_equals-and-hashcode.md). `String`, wrappers, enums, records, `LocalDate` and `List`/`Set` (of good elements) are safe keys; **arrays** are not (identity hash).

## Operations step by step

**`put(k, v)`:** compute hash → find bucket → if empty, insert a node; else walk the bucket: if an entry has an equal key, **replace its value**; otherwise append. Increment `size`; if it exceeds the threshold, **resize**.

**`get(k)`:** compute hash → bucket → return the matching entry's value, or `null`.

**`null` key:** allowed once; its hash is `0`, so it goes to bucket 0.

## Complexity

| Operation | Average | Worst case |
|-----------|---------|------------|
| `put`, `get`, `remove`, `containsKey` | **O(1)** | O(log n) (treeified bucket; O(n) before Java 8) |
| `containsValue` | O(n) | O(n) |
| Iteration | O(capacity + size) | O(capacity + size) |
| Resize | O(n), amortized across inserts | |

Iteration visits **every bucket**, including empty ones, so a huge, sparsely filled map is slow to iterate. Do not create `new HashMap<>(1_000_000)` for a few entries.

## A simplified implementation

```java
class MiniHashMap<K, V> {
    private static class Node<K, V> { final K key; V value; Node<K, V> next;
        Node(K k, V v, Node<K, V> n) { key = k; value = v; next = n; } }

    @SuppressWarnings("unchecked")
    private Node<K, V>[] table = (Node<K, V>[]) new Node[16];
    private int size;

    private int index(Object key) {
        int h = key == null ? 0 : key.hashCode();
        h ^= (h >>> 16);                                     // spread
        return h & (table.length - 1);                       // power-of-two modulo
    }

    V get(Object key) {
        for (Node<K, V> n = table[index(key)]; n != null; n = n.next)
            if (Objects.equals(n.key, key)) return n.value;
        return null;
    }

    V put(K key, V value) {
        int i = index(key);
        for (Node<K, V> n = table[i]; n != null; n = n.next)
            if (Objects.equals(n.key, key)) { V old = n.value; n.value = value; return old; }
        table[i] = new Node<>(key, value, table[i]);         // insert at the head of the chain
        if (++size > table.length * 3 / 4) resize();
        return null;
    }

    @SuppressWarnings("unchecked")
    private void resize() {
        Node<K, V>[] old = table;
        table = (Node<K, V>[]) new Node[old.length * 2];
        for (Node<K, V> head : old)
            for (Node<K, V> n = head; n != null; n = n.next) {   // re-insert every entry
                int i = index(n.key);
                table[i] = new Node<>(n.key, n.value, table[i]);
            }
    }
}
```

This captures the core; the real `HashMap` adds treeification, order-preserving resize and many optimizations.

## `HashSet`: a `HashMap` in disguise

```java
public class HashSet<E> {
    private transient HashMap<E, Object> map;
    private static final Object PRESENT = new Object();
    public boolean add(E e) { return map.put(e, PRESENT) == null; }
    public boolean contains(Object o) { return map.containsKey(o); }
}
```

Elements are the **keys**; every value is the same dummy object. So all the rules above (hashing, resizing, `equals`/`hashCode`, order) apply to `HashSet` unchanged. `add` returns `false` when an equal element already exists: handy for duplicate detection ([02](./02_set.md)).

## Iteration order

`HashMap` and `HashSet` iterate in **bucket order**, which depends on hash codes and the current capacity. It looks random, can change when the map resizes, and can differ between Java versions.

```java
Map<String, Integer> m = new HashMap<>(Map.of("one", 1, "two", 2, "three", 3));
System.out.println(m);        // order is unspecified: never write code or tests that depend on it
```

For stable order use `LinkedHashMap`/`LinkedHashSet` or sort the keys.

## `LinkedHashMap` and `LinkedHashSet`

A hash table **plus** a doubly linked list through the entries, so iteration follows a defined order and `get` stays O(1).

| | `HashMap` | `LinkedHashMap` |
|---|-----------|-----------------|
| Lookup | O(1) | O(1) |
| Iteration order | Unspecified | **Insertion order** (default) or **access order** |
| Memory | Lower | Two extra references per entry |
| Iteration speed | O(capacity + size) | **O(size)**: walks the linked list only |
| Extras | | `removeEldestEntry` hook for caches; Java 21 `SequencedMap` methods |

```java
Map<String, Integer> ordered = new LinkedHashMap<>();
ordered.put("b", 1); ordered.put("a", 2);
ordered;                                       // {b=1, a=2}: insertion order kept
// Re-inserting an existing key does NOT change its position (in insertion-order mode)
```

**Access-order mode** (`new LinkedHashMap<>(16, 0.75f, true)`) moves an entry to the end each time it is read or written: the basis of **LRU caches** ([11_specialized-collections.md](./11_specialized-collections.md)).

`LinkedHashSet` is to `HashSet` what `LinkedHashMap` is to `HashMap`: insertion-ordered unique elements. Use it to **deduplicate while keeping order**.

## When `HashMap` is not enough

| Need | Use |
|------|-----|
| Sorted keys, range queries | `TreeMap` ([09](./09_treemap-and-treeset.md)) |
| Predictable order / LRU | `LinkedHashMap` |
| Enum keys | `EnumMap` ([11](./11_specialized-collections.md)) |
| Thread-safe | `ConcurrentHashMap` ([14-concurrency](../14-concurrency/08_concurrent-collections.md)) |
| Primitive keys/values at scale | A primitive-collection library (fastutil, Eclipse Collections), or arrays |
| Compare keys by `==` | `IdentityHashMap` |
| Weakly referenced keys | `WeakHashMap` |

## Memory

Each entry is a `Node` object (header + `hash` + 3 references ≈ 32-48 bytes), plus the key and value objects, plus table slots (about 1.3-2.7 slots per entry given the load factor). A `HashMap<Integer, Integer>` with millions of entries is heavy; boxed numbers add more ([wrappers](../01-fundamentals/04_wrapper-classes-and-autoboxing.md)).

## Thread-safety

`HashMap` is **not** thread-safe. Concurrent writes can lose updates, corrupt the structure, and (in older Java versions) cause infinite loops during resize. Use `ConcurrentHashMap`, or confine the map to one thread ([14-concurrency](../14-concurrency/README.md)).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Key class overrides `equals` but not `hashCode` (or vice versa) | Lookups fail, duplicates appear | Override both, same fields |
| Mutable keys changed after insertion | Entry unreachable | Immutable keys |
| Array as key | Never matches a different array with equal contents | `List.of(...)`, a record, or `Arrays.hashCode` wrapper |
| Relying on iteration order | Flaky tests, behavior changes | `LinkedHashMap`/`TreeMap` or sort |
| `new HashMap<>(n)` expecting room for `n` entries | Resizes anyway | `HashMap.newHashMap(n)` / `n / 0.75 + 1` |
| Huge initial capacity for a small map | Slow iteration, wasted memory | Size realistically |
| Sharing a `HashMap` between threads | Lost updates, corruption | `ConcurrentHashMap` |
| Poor `hashCode` (constant or only one field) | Degraded O(n)/O(log n) lookups | Combine all identity fields (`Objects.hash`) |
| Modifying the map while iterating | `ConcurrentModificationException` | `removeIf` on views, `Iterator.remove()` |
| `get` returning `null` read as "absent" | Wrong when `null` values exist | `containsKey`, avoid `null` values |
| `map.keySet().contains(x)` in a loop on a `List` | Fine for sets; do not do `list.contains` instead | Use the map or a set |

## Key takeaways

- A hash table: `hashCode` → spread → bucket index (`hash & (capacity - 1)`) → `equals` inside the bucket
- Default capacity 16, load factor 0.75, doubling on resize; buckets with 8+ entries become trees (Java 8+)
- Correct `equals` + `hashCode` and **immutable keys** are mandatory
- Average O(1) for `get`/`put`/`remove`; iteration cost depends on capacity; order is unspecified
- `HashSet` is a `HashMap` with dummy values; `LinkedHashMap`/`LinkedHashSet` add predictable order and LRU capability
- Not thread-safe: use `ConcurrentHashMap` for concurrency

**Next:** [TreeMap and TreeSet](./09_treemap-and-treeset.md)
