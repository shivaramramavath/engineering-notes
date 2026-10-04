# Map

A `Map<K, V>` stores **key-value pairs**: each **key** is unique and maps to one **value**. It is the structure for lookups ("find the user with this id", "count occurrences of each word"). A `Map` is **not** a `Collection`; it has its own interface and exposes collection views.

```java
Map<String, Integer> ages = new HashMap<>();
ages.put("Ada", 36);
ages.put("Linus", 54);
ages.put("Ada", 37);              // same key: REPLACES the value, returns the old one (36)
ages.get("Ada");                  // 37
ages.get("Grace");                // null: no such key
ages.size();                      // 2
```

## The contract

| Property | Meaning |
|----------|---------|
| Keys are **unique** | A second `put` with an equal key replaces the value |
| One value per key | Values may repeat and may be `null` (implementation-dependent) |
| Key equality | `equals`/`hashCode` (hash maps) or `compareTo`/`Comparator` (tree maps) |
| Order | Depends on the implementation (none, insertion, sorted) |
| Map equality | Equal if they have the same set of key-value pairs, regardless of implementation or order |

## Implementations

| Implementation | Order | `get`/`put`/`remove` | Null keys | Use when |
|----------------|-------|----------------------|-----------|----------|
| **`HashMap`** | None | O(1) average | One | **Default** ([08](./08_hashmap-and-hashset.md)) |
| **`LinkedHashMap`** | Insertion (or access) order | O(1) | One | Predictable iteration, LRU caches ([11](./11_specialized-collections.md)) |
| **`TreeMap`** | Sorted by key | O(log n) | No | Sorted keys, range queries ([09](./09_treemap-and-treeset.md)) |
| `EnumMap` | Enum order | O(1) | No | Enum keys ([11](./11_specialized-collections.md)) |
| `IdentityHashMap` | None | O(1) | Yes | Compare keys by `==` |
| `WeakHashMap` | None | O(1) | Yes | Keys that may be garbage collected |
| `ConcurrentHashMap` | None | O(1) | **No** | Concurrent access ([14-concurrency](../14-concurrency/08_concurrent-collections.md)) |
| `Map.of`, `Map.copyOf` | Unspecified | O(1) | No | Immutable maps |
| `Hashtable` | None | O(1) | No | **Legacy: avoid** |

## Core operations

```java
Map<String, Integer> m = new HashMap<>();

m.put("a", 1);                    // returns the previous value or null
m.get("a");                       // value or null
m.getOrDefault("z", 0);           // value or the default (does not store it)
m.containsKey("a");  m.containsValue(1);   // containsValue is O(n)
m.remove("a");                    // returns the removed value or null
m.remove("a", 1);                 // removes only if currently mapped to 1
m.size();  m.isEmpty();  m.clear();
m.putAll(other);
```

### Atomic-style update methods (Java 8+)

These replace the "get, check, put" dance:

| Method | Does |
|--------|------|
| `putIfAbsent(k, v)` | Put only if the key is absent (or mapped to `null`) |
| `computeIfAbsent(k, k -> v)` | If absent, compute the value and store it; return the value |
| `computeIfPresent(k, (k, old) -> v)` | If present, recompute; returning `null` **removes** the entry |
| `compute(k, (k, old) -> v)` | Compute from the old value (may be `null`) |
| `merge(k, v, (old, v) -> r)` | If absent, store `v`; else store `fn(old, v)` |
| `replace(k, v)`, `replaceAll((k, v) -> ...)` | Replace existing values |

```java
// Count occurrences
Map<String, Integer> counts = new HashMap<>();
for (String w : words) counts.merge(w, 1, Integer::sum);

// Group into lists
Map<Integer, List<String>> byLength = new HashMap<>();
for (String w : words) byLength.computeIfAbsent(w.length(), k -> new ArrayList<>()).add(w);

// Instead of this:
Integer old = counts.get(w);
if (old == null) counts.put(w, 1); else counts.put(w, old + 1);
```

For grouping and counting with streams: [09-functional-java/06_collectors.md](../09-functional-java/06_collectors.md).

## Iterating a map

A `Map` is iterated through its **views**:

```java
Map<String, Integer> m = Map.of("a", 1, "b", 2);

for (Map.Entry<String, Integer> e : m.entrySet()) {        // best: key and value together
    System.out.println(e.getKey() + " = " + e.getValue());
}
for (String k : m.keySet()) { ... }                         // keys only
for (Integer v : m.values()) { ... }                        // values only
m.forEach((k, v) -> System.out.println(k + " = " + v));     // concise
```

| View | Type | Operations |
|------|------|------------|
| `keySet()` | `Set<K>` | Read; `remove` deletes the entry |
| `values()` | `Collection<V>` | Read; `remove` deletes an entry; **not** a `Set` (duplicates possible) |
| `entrySet()` | `Set<Map.Entry<K,V>>` | Read; `entry.setValue(v)` writes through |

```java
m.keySet().removeIf(k -> k.startsWith("tmp"));              // changes the underlying map
m.values().removeIf(v -> v == null);
m.entrySet().removeIf(e -> e.getValue() < 0);
for (var e : m.entrySet()) e.setValue(e.getValue() * 2);     // update in place
```

Do **not** loop over `keySet()` and call `get(k)` for each key when you need values too: use `entrySet()` (one lookup instead of two).

## Creating maps

```java
Map<String, Integer> a = new HashMap<>();
Map<String, Integer> b = Map.of("x", 1, "y", 2);                      // immutable; up to 10 pairs
Map<String, Integer> c = Map.ofEntries(Map.entry("x", 1), Map.entry("y", 2));   // immutable, any size
Map<String, Integer> d = Map.copyOf(other);                           // immutable copy
Map<String, Integer> e = new HashMap<>(other);                        // mutable copy
Map<String, Integer> f = new TreeMap<>(other);                        // sorted copy
```

`Map.of` throws on duplicate keys and rejects `null`; its iteration order is unspecified ([13](./13_immutable-and-unmodifiable-collections.md)).

## `null` handling: `get` is ambiguous

```java
Integer v = map.get("k");        // null means: key absent OR value is null (in HashMap)
map.containsKey("k");            // distinguishes the two
```

`HashMap` permits `null` values; `ConcurrentHashMap`, `TreeMap` (null keys) and `Map.of` do not. **Avoid `null` values**: use a sentinel, `Optional`, or leave the key out.

## Keys: rules

| Rule | Why |
|------|-----|
| Use **immutable** keys (`String`, `Integer`, enums, records) | A key whose `hashCode`/ordering changes becomes unreachable |
| Implement `equals`/`hashCode` (hash maps) or `Comparable` (tree maps) correctly | Lookups depend on them |
| Do not use arrays as keys | Identity `equals`/`hashCode` |
| Prefer small, cheap keys | Hashing and comparing cost |
| Do not use `double` keys carelessly | `-0.0`, `NaN`, rounding |

## Nested and composite maps

```java
Map<String, Map<String, Integer>> byUserThenItem = new HashMap<>();
byUserThenItem.computeIfAbsent("ada", k -> new HashMap<>()).merge("book", 1, Integer::sum);

record Key(String user, String item) { }          // composite key as a record
Map<Key, Integer> flat = new HashMap<>();
flat.merge(new Key("ada", "book"), 1, Integer::sum);
```

A record key is often clearer than nested maps. Multi-valued maps (`Map<K, List<V>>`) are common; remember to create the inner list (`computeIfAbsent`).

## Sorted maps: `SortedMap` / `NavigableMap`

```java
NavigableMap<Integer, String> grades = new TreeMap<>(Map.of(90, "A", 80, "B", 70, "C"));
grades.floorEntry(85);          // 80=B   (greatest key <= 85)
grades.ceilingKey(85);          // 90
grades.firstKey();  grades.lastEntry();
grades.headMap(80);             // keys < 80   (view)
grades.subMap(70, true, 85, true);
grades.descendingMap();
```

More in [09_treemap-and-treeset.md](./09_treemap-and-treeset.md).

## Sequenced maps (Java 21)

`LinkedHashMap` and `TreeMap` implement `SequencedMap`: `firstEntry()`, `lastEntry()`, `pollFirstEntry()`, `putFirst`, `putLast`, `reversed()`, `sequencedKeySet()`, ... ([hierarchy](./00_collection-hierarchy.md#sequenced-collections-java-21)).

## Sorting a map

Maps are not sortable in place. Produce a sorted view or list:

```java
// Entries sorted by value descending
List<Map.Entry<String, Integer>> top = new ArrayList<>(counts.entrySet());
top.sort(Map.Entry.<String, Integer>comparingByValue().reversed());

// Or with streams, into an ordered map
Map<String, Integer> sorted = counts.entrySet().stream()
    .sorted(Map.Entry.<String, Integer>comparingByValue().reversed())
    .collect(Collectors.toMap(Map.Entry::getKey, Map.Entry::getValue, (a, b) -> a, LinkedHashMap::new));

// Sorted by key: copy into a TreeMap
Map<String, Integer> byKey = new TreeMap<>(counts);
```

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `get` then `if (v == null) put(...)` | Verbose, not atomic | `computeIfAbsent`, `merge`, `putIfAbsent` |
| Looping `keySet()` then `get(k)` | Double lookup | `entrySet()` |
| Unboxing `map.get(k)` into `int` | `NullPointerException` for a missing key | `getOrDefault(k, 0)` |
| Mutable or array keys | Entries lost or never matched | Immutable keys, records |
| Modifying the map while iterating its views | `ConcurrentModificationException` | `removeIf` on the view, or `Iterator.remove()` |
| `Map.of(...)` then `put` | `UnsupportedOperationException` | `new HashMap<>(Map.of(...))` |
| Assuming `HashMap` order | Order changes | `LinkedHashMap`/`TreeMap` |
| `null` keys/values in `ConcurrentHashMap`, `TreeMap`, `Map.of` | `NullPointerException` | Avoid `null` |
| `computeIfAbsent` whose function modifies the same map | `ConcurrentModificationException` / corrupt state | Do not recurse into the same map ([recursion](../02-methods/03_recursion.md)) |
| `containsValue` in loops | O(n) each | Maintain a reverse map |
| `Map<K, List<V>>` and forgetting to create the list | `NullPointerException` | `computeIfAbsent(k, x -> new ArrayList<>())` |

## Key takeaways

- A `Map` maps unique keys to values; it is not a `Collection`, and offers `keySet`, `values` and `entrySet` views
- `HashMap` by default, `LinkedHashMap` for order, `TreeMap` for sorted keys
- Prefer `merge`, `computeIfAbsent`, `getOrDefault` over manual get/put logic
- Iterate with `entrySet()`; remove through views or `removeIf`
- Keys must be immutable with correct `equals`/`hashCode`; avoid `null` values

**Next:** [Queue and Deque](./04_queue-and-deque.md)
