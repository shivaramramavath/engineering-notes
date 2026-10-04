# TreeMap and TreeSet

`TreeMap<K, V>` and `TreeSet<E>` keep their entries **sorted** at all times. They are implemented with a **red-black tree**, a self-balancing binary search tree, giving **O(log n)** operations and a rich set of **navigation methods** (nearest key, ranges, first/last). Choose them when you need order or range queries; choose hash collections when you only need fast lookup.

```java
TreeSet<Integer> set = new TreeSet<>(List.of(30, 10, 20));
System.out.println(set);                // [10, 20, 30]: always sorted
set.first();                            // 10
set.ceiling(15);                        // 20: smallest element >= 15

TreeMap<String, Integer> map = new TreeMap<>();
map.put("banana", 2); map.put("apple", 1); map.put("cherry", 3);
System.out.println(map);                // {apple=1, banana=2, cherry=3}: sorted by key
```

## How it works

A **binary search tree** keeps smaller keys on the left and larger on the right, so lookup follows one path from the root:

```
                 20
               /    \
            10        30
           /  \         \
          5    15        40

 find 15: 20 → go left → 10 → go right → 15 ✔   (log n steps when the tree is balanced)
```

Plain BSTs degrade into linked lists on sorted input. A **red-black tree** colours each node red or black and enforces rules that keep the tree approximately balanced:

| Rule | Effect |
|------|--------|
| The root is black; red nodes have only black children | No two reds in a row |
| Every path from a node to its leaves has the same number of black nodes | Longest path ≤ 2× the shortest |
| After insert/delete, **rotations** and **recolouring** restore the rules | Height stays ≤ 2·log₂(n + 1) |

You never write this code, but the consequences matter: **guaranteed O(log n)** for `put`, `get`, `remove`, `containsKey`, and in-order iteration in sorted order.

`TreeSet` is backed by a `TreeMap` (elements are keys), exactly as `HashSet` is backed by `HashMap` ([08](./08_hashmap-and-hashset.md)).

## Ordering: natural or comparator

```java
TreeSet<String> natural = new TreeSet<>();                                   // String is Comparable
TreeSet<String> byLength = new TreeSet<>(Comparator.comparing(String::length)
                                                    .thenComparing(Comparator.naturalOrder()));
TreeMap<String, Integer> caseInsensitive = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
TreeMap<Integer, String> descending = new TreeMap<>(Comparator.reverseOrder());
```

- With no comparator, keys must implement `Comparable`, else `ClassCastException` on the first `put`
- Details of writing comparators: [05_comparable-and-comparator.md](./05_comparable-and-comparator.md)

### Equality is decided by the comparator, not `equals`

```java
Set<String> ci = new TreeSet<>(String.CASE_INSENSITIVE_ORDER);
ci.add("Java");
ci.add("JAVA");                // compare == 0 → treated as a duplicate and NOT added
ci.size();                     // 1
ci.contains("java");           // true
```

**Make the comparator compare every field that identifies an element**, or distinct elements will be silently dropped. Keep it consistent with `equals`, or accept the difference knowingly.

```java
Set<Person> bad = new TreeSet<>(Comparator.comparingInt(Person::age));      // two different people with the same age → one is lost
Set<Person> ok  = new TreeSet<>(Comparator.comparingInt(Person::age).thenComparing(Person::name));
```

### `null`

With natural ordering, `null` keys throw `NullPointerException`. A custom comparator may accept them (`Comparator.nullsFirst(...)`), but avoid it.

## Navigation methods

`TreeMap` implements `NavigableMap`, `TreeSet` implements `NavigableSet`. The "nearest" family answers questions that hash collections cannot.

| Question | `TreeSet` | `TreeMap` (keys / entries) |
|----------|-----------|-----------------------------|
| Smallest / largest | `first()`, `last()` | `firstKey()`, `lastKey()`, `firstEntry()`, `lastEntry()` |
| Greatest element **≤** x | `floor(x)` | `floorKey(x)`, `floorEntry(x)` |
| Smallest element **≥** x | `ceiling(x)` | `ceilingKey(x)`, `ceilingEntry(x)` |
| Greatest element **<** x | `lower(x)` | `lowerKey(x)`, `lowerEntry(x)` |
| Smallest element **>** x | `higher(x)` | `higherKey(x)`, `higherEntry(x)` |
| Remove and return the smallest / largest | `pollFirst()`, `pollLast()` | `pollFirstEntry()`, `pollLastEntry()` |
| Reverse order | `descendingSet()`, `descendingIterator()` | `descendingMap()`, `descendingKeySet()` |

```java
TreeSet<Integer> s = new TreeSet<>(List.of(10, 20, 30, 40));
s.floor(25);        // 20
s.ceiling(25);      // 30
s.lower(20);        // 10
s.higher(40);       // null: no greater element
s.floor(5);         // null: no element <= 5
```

All return `null` when no such element exists (check before unboxing).

### Range views

```java
TreeSet<Integer> s = new TreeSet<>(List.of(10, 20, 30, 40, 50));
s.headSet(30);                        // [10, 20]         elements < 30
s.headSet(30, true);                  // [10, 20, 30]     inclusive
s.tailSet(30);                        // [30, 40, 50]     elements >= 30
s.tailSet(30, false);                 // [40, 50]
s.subSet(20, 40);                     // [20, 30]         from inclusive, to EXCLUSIVE
s.subSet(20, true, 40, true);         // [20, 30, 40]

TreeMap<Integer, String> m = ...;
m.headMap(30);  m.tailMap(30);  m.subMap(20, 40);              // same idea for maps
m.subMap(20, true, 40, true);
```

These are **live views** of the original: changes to the view write through, and vice versa. Adding a key **outside** the view's range throws `IllegalArgumentException`.

```java
s.subSet(20, 40).clear();             // removes 20 and 30 from the original set
```

**Cost caveat:** `size()` of a range view is **O(n)** (it counts), though creating the view is O(log n). Do not call `headSet(x).size()` in a loop to compute rank.

## Worked examples

### Grade lookup by thresholds (`floorEntry`)

```java
TreeMap<Integer, String> grades = new TreeMap<>(Map.of(0, "F", 60, "D", 70, "C", 80, "B", 90, "A"));
grades.floorEntry(85).getValue();      // "B": highest threshold <= 85
grades.floorEntry(59).getValue();      // "F"
```

Tax brackets, price tiers, version ranges and rate tables all fit this pattern.

### Time-based lookup

```java
TreeMap<Instant, Price> history = new TreeMap<>();
Price at(Instant t) { Map.Entry<Instant, Price> e = history.floorEntry(t); return e == null ? null : e.getValue(); }
```

### Leaderboard / top N

```java
TreeMap<Integer, String> scores = new TreeMap<>(Comparator.reverseOrder());
scores.put(90, "Ada");  scores.put(75, "Linus");
scores.firstEntry();                                           // best score
scores.headMap(80, true);                                      // everyone with >= 80 (descending order)
```

(For duplicate scores use `TreeMap<Integer, List<String>>` or a composite key.)

### Next free slot / overlap check

```java
TreeMap<Integer, Integer> bookings = new TreeMap<>();          // start → end
boolean overlaps(int s, int e) {
    Map.Entry<Integer, Integer> prev = bookings.floorEntry(s);
    if (prev != null && prev.getValue() > s) return true;      // previous booking runs into s
    Integer next = bookings.ceilingKey(s);
    return next != null && next < e;                           // next booking starts before e
}
```

## Complexity

| Operation | `TreeMap`/`TreeSet` | `HashMap`/`HashSet` |
|-----------|---------------------|---------------------|
| `put`, `get`, `remove`, `contains` | **O(log n)** guaranteed | O(1) average |
| `first`/`last`, `floor`/`ceiling`, `pollFirst` | **O(log n)** | not available |
| Range views | O(log n) to create | not available |
| Iteration (in order) | O(n), **sorted** | O(capacity + size), unordered |
| Memory per entry | ~40 bytes (node with key, value, left, right, parent, colour) | ~32-48 bytes |

For small collections the difference is negligible. `TreeMap` is slower than `HashMap` for pure lookups, so use it for **what only it can do**.

## When to choose

| Need | Collection |
|------|------------|
| Fast lookup only | `HashMap` / `HashSet` |
| Insertion order | `LinkedHashMap` / `LinkedHashSet` |
| Keys iterated in sorted order | **`TreeMap`** / **`TreeSet`** |
| "Closest key", range queries, floor/ceiling | **`TreeMap`** / **`TreeSet`** |
| Smallest/largest repeatedly, no lookups | `PriorityQueue` ([10](./10_priorityqueue.md)) |
| Sorted **concurrent** map or set | `ConcurrentSkipListMap` / `ConcurrentSkipListSet` ([14-concurrency](../14-concurrency/08_concurrent-collections.md)) |
| Keys are enums | `EnumMap` / `EnumSet` ([11](./11_specialized-collections.md)) |
| Sorted by **value**, not key | Not a `TreeMap` job: sort the entries into a list, or maintain a second structure |

## Sorted by value? Not possible directly

A `TreeMap` orders by **key**. A comparator that looks up the **value** through the map is wrong (values change, comparator calls `get` recursively, and keys with equal values collide). Instead:

```java
List<Map.Entry<String, Integer>> byValue = new ArrayList<>(map.entrySet());
byValue.sort(Map.Entry.comparingByValue());
```

## Thread-safety

Not thread-safe. Use `ConcurrentSkipListMap`/`Set`, or wrap with a lock ([14-concurrency](../14-concurrency/README.md)).

## Java 21: sequenced methods

`TreeMap`/`TreeSet` implement `SequencedMap`/`SequencedSet`: `reversed()`, `firstEntry()`, `getFirst()`, `getLast()` are available alongside the older `first()`/`last()` ([hierarchy](./00_collection-hierarchy.md#sequenced-collections-java-21)). Methods that would reorder (`addFirst`, `putFirst`) throw `UnsupportedOperationException` on sorted collections.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Key type not `Comparable` and no comparator | `ClassCastException` at the first `put` | Implement `Comparable` or pass a `Comparator` |
| Comparator ignores some identifying fields | Elements silently missing ("duplicates") | Compare all identifying fields (add tie-breakers) |
| Comparator inconsistent with `equals` and expecting `Set` semantics | `contains` surprises | Know that the comparator defines equality |
| `null` in a natural-ordering tree | `NullPointerException` | Avoid `null`, or use `nullsFirst/nullsLast` |
| Mutating a key's sort fields after insertion | Broken tree: lookups fail | Immutable keys; remove, change, re-add |
| Assuming `floor`/`ceiling` never return `null` | `NullPointerException` on unboxing | Check for `null` |
| `subSet(...).size()` in loops | O(n) each | Count by iteration once, or use a different structure |
| Adding an out-of-range key through a range view | `IllegalArgumentException` | Add to the original collection |
| Using `TreeMap` when only lookups are needed | Slower than `HashMap` | `HashMap` |
| Trying to sort a `TreeMap` by value | Wrong results | Sort entries into a list |
| Expecting `TreeMap` to be thread-safe | Corrupted tree | `ConcurrentSkipListMap` |

## Key takeaways

- `TreeMap`/`TreeSet` are red-black trees: sorted, O(log n), guaranteed
- Ordering comes from `Comparable` or a `Comparator`, and **that ordering also defines equality**
- Navigation (`floor`, `ceiling`, `lower`, `higher`, `first`, `last`) and range views (`headSet`, `tailMap`, `subMap`) are their superpower
- Use them for sorted iteration and nearest-key queries; use `HashMap` for plain lookup and `PriorityQueue` for repeated min/max
- Keys must be immutable with respect to the ordering; no `null` with natural ordering

**Next:** [PriorityQueue](./10_priorityqueue.md)
