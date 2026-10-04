# Set

A `Set<E>` is a collection that contains **no duplicate elements**. It models the mathematical set: membership matters; position does not. Use it to remove duplicates and to answer "is this in the collection?" quickly.

```java
Set<String> tags = new HashSet<>();
tags.add("java");        // true:  added
tags.add("spring");      // true
tags.add("java");        // false: already present, set unchanged
tags.size();             // 2
tags.contains("java");   // true
```

## The contract

| Property | Meaning |
|----------|---------|
| **No duplicates** | `add(e)` does nothing (and returns `false`) if an equal element is already present |
| **No index** | There is no `get(i)`; you test membership or iterate |
| "Equal" is defined by... | `equals`/`hashCode` for hash-based sets, `compareTo`/`Comparator` for sorted sets |
| Order | **Depends on the implementation** (none, insertion, sorted) |
| Set equality | Two sets are `equals` if they contain the same elements, **regardless of order or implementation** |

```java
new HashSet<>(List.of(1, 2, 3)).equals(new TreeSet<>(List.of(3, 2, 1)));    // true
```

## How "duplicate" is decided

This is the key to using sets correctly:

| Implementation | Duplicate test |
|----------------|----------------|
| `HashSet`, `LinkedHashSet` | `hashCode()` then `equals()` ([08](./08_hashmap-and-hashset.md)) |
| `TreeSet` | `compareTo()` / `Comparator.compare()` returns `0` ([09](./09_treemap-and-treeset.md)) |

So **your element class must implement the right methods**:

```java
record Point(int x, int y) { }                  // records: equals and hashCode generated
Set<Point> pts = new HashSet<>();
pts.add(new Point(1, 2));
pts.add(new Point(1, 2));                        // duplicate: ignored
pts.size();                                      // 1

class Bad { int id; }                            // no equals/hashCode
Set<Bad> bad = new HashSet<>();
bad.add(new Bad()); bad.add(new Bad());          // both kept: identity equality
```

See [04-oop/14_equals-and-hashcode.md](../04-oop/14_equals-and-hashcode.md).

## Implementations

| Implementation | Order | `add`/`contains`/`remove` | `null` | Use when |
|----------------|-------|---------------------------|--------|----------|
| **`HashSet`** | None (unspecified) | O(1) average | One `null` | **Default**: fastest membership and uniqueness |
| **`LinkedHashSet`** | **Insertion order** | O(1) average | One `null` | Uniqueness **and** predictable iteration |
| **`TreeSet`** | **Sorted** (natural or `Comparator`) | O(log n) | No `null` (natural ordering) | Sorted iteration, range queries (`floor`, `subSet`) |
| `EnumSet` | Enum declaration order | O(1) | No | Sets of enum constants ([11](./11_specialized-collections.md)) |
| `Set.of(...)`, `Set.copyOf` | **Unspecified, randomized** | O(1) | No | Immutable constants; throws on duplicates in `of` |
| `CopyOnWriteArraySet` | Insertion | O(n) | Yes | Concurrent, read-mostly, small |
| `ConcurrentSkipListSet` | Sorted | O(log n) | No | Concurrent sorted set |
| `Collections.newSetFromMap(new ConcurrentHashMap<>())` | None | O(1) | No | Concurrent hash set |

```java
Set<Integer> hash   = new HashSet<>(List.of(3, 1, 2));        // iteration order unspecified
Set<Integer> linked = new LinkedHashSet<>(List.of(3, 1, 2));  // [3, 1, 2]
Set<Integer> sorted = new TreeSet<>(List.of(3, 1, 2));        // [1, 2, 3]
Set<Integer> desc   = new TreeSet<>(Comparator.reverseOrder());
```

`HashSet` is implemented on top of a `HashMap` (elements are the map's keys), and `TreeSet` on a `TreeMap`.

## Core operations

```java
Set<String> s = new HashSet<>();

s.add("a");                 // boolean: true if it was NOT already there
s.remove("a");              // boolean: true if it was there
s.contains("a");
s.size();  s.isEmpty();  s.clear();
s.addAll(other);            // true if the set changed
s.removeIf(x -> x.isEmpty());
for (String x : s) { ... }
s.stream().sorted().toList();
```

Use the boolean from `add` instead of checking `contains` first:

```java
if (!seen.add(id)) {                 // add returns false when id was already present
    System.out.println("duplicate: " + id);
}
```

## Set algebra

`addAll`, `retainAll` and `removeAll` **mutate** the receiver, so copy first when you need the originals.

```java
Set<Integer> a = Set.of(1, 2, 3, 4);
Set<Integer> b = Set.of(3, 4, 5);

Set<Integer> union = new TreeSet<>(a);        union.addAll(b);         // [1, 2, 3, 4, 5]
Set<Integer> intersection = new TreeSet<>(a); intersection.retainAll(b);   // [3, 4]
Set<Integer> difference = new TreeSet<>(a);   difference.removeAll(b);     // [1, 2]  (a minus b)
Set<Integer> symmetric = new TreeSet<>(union); symmetric.removeAll(intersection);   // [1, 2, 5]

boolean subset = a.containsAll(Set.of(1, 2));         // is {1,2} ⊆ a?
boolean disjoint = Collections.disjoint(a, Set.of(9)); // no elements in common
```

| Operation | Method |
|-----------|--------|
| Union (A ∪ B) | `addAll` |
| Intersection (A ∩ B) | `retainAll` |
| Difference (A − B) | `removeAll` |
| Subset (B ⊆ A) | `A.containsAll(B)` |

## Common uses

```java
// 1. Remove duplicates (keeping first-seen order)
List<String> unique = new ArrayList<>(new LinkedHashSet<>(list));

// 2. Fast membership test (instead of list.contains, O(n))
Set<String> allowed = Set.of("GET", "POST");
if (allowed.contains(method)) { ... }

// 3. Detect duplicates
Set<String> seen = new HashSet<>();
for (String s : items) if (!seen.add(s)) { /* duplicate found */ }

// 4. Visited set in graph/tree traversals
Set<Node> visited = new HashSet<>();

// 5. Sorted unique values
SortedSet<Integer> sorted = new TreeSet<>(numbers);
```

## Sorted sets: `SortedSet` / `NavigableSet`

`TreeSet` implements navigation methods ([09_treemap-and-treeset.md](./09_treemap-and-treeset.md)):

```java
NavigableSet<Integer> ts = new TreeSet<>(List.of(10, 20, 30, 40));
ts.first();            // 10
ts.last();             // 40
ts.floor(25);          // 20: greatest element <= 25
ts.ceiling(25);        // 30: smallest element >= 25
ts.lower(20);  ts.higher(20);
ts.headSet(30);        // [10, 20]            (views)
ts.tailSet(30);        // [30, 40]
ts.subSet(15, 35);     // [20, 30]
ts.descendingSet();    // [40, 30, 20, 10]
ts.pollFirst();        // remove and return the smallest
```

## Mutable elements are dangerous

If an element's `hashCode` (hash sets) or sort key (tree sets) **changes after** it was added, the set can no longer find it:

```java
Set<Person> people = new HashSet<>();
Person p = new Person("Ada");
people.add(p);
p.setName("Grace");               // hashCode changed
people.contains(p);               // false: it is stored in the old bucket
people.remove(p);                 // false: now unremovable
```

Use immutable elements (records, `String`, wrappers). Same rule as hash-map keys ([08](./08_hashmap-and-hashset.md)).

## Immutable sets

```java
Set<String> s = Set.of("a", "b", "c");           // throws IllegalArgumentException on duplicate elements
Set<String> c = Set.copyOf(someCollection);       // duplicates are fine here
s.add("d");                                       // UnsupportedOperationException
```

Iteration order of `Set.of` is **intentionally unspecified and varies between JVM runs**; do not depend on it ([13](./13_immutable-and-unmodifiable-collections.md)).

## `Set` vs `List`

| Need | `Set` | `List` |
|------|-------|--------|
| Uniqueness enforced | ✅ | ❌ |
| Fast `contains` | ✅ O(1) / O(log n) | ❌ O(n) |
| Index access | ❌ | ✅ |
| Duplicates meaningful (counts, order of events) | ❌ | ✅ |
| Memory per element | Higher | Lower |

For "unique and indexed" combine: `new ArrayList<>(new LinkedHashSet<>(c))`.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Elements without `equals`/`hashCode` in a `HashSet` | "Duplicates" kept | Implement both, or use a record |
| Elements with `hashCode` but inconsistent `equals` | Missing or phantom elements | Keep the two consistent |
| `TreeSet` with a comparator inconsistent with `equals` | Elements silently dropped as "duplicates" | Compare all identifying fields ([05](./05_comparable-and-comparator.md)) |
| Relying on `HashSet` iteration order | Order changes between runs/versions | `LinkedHashSet` or `TreeSet` |
| Mutating an element after adding it | Cannot find or remove it | Immutable elements |
| `Set.of(...)` with duplicate arguments | `IllegalArgumentException` | `Set.copyOf` or a `HashSet` |
| Adding `null` to `TreeSet` / `Set.of` | `NullPointerException` | Avoid `null` |
| `retainAll`/`removeAll` on the original set when you needed it | Original destroyed | Copy first |
| Using a `List` and `contains` for large membership checks | Slow | Use a set |
| Calling `set.get(i)` or sorting a set | No such method | Convert to a list, or use `TreeSet` |

## Key takeaways

- A `Set` rejects duplicates; "duplicate" means `equals`/`hashCode` (hash sets) or comparator result `0` (tree sets)
- `HashSet` = fastest; `LinkedHashSet` = insertion order; `TreeSet` = sorted with navigation
- `add` returns `false` for a duplicate: use it to detect repeats
- Set algebra: `addAll` (union), `retainAll` (intersection), `removeAll` (difference), on a copy
- Use immutable elements; never rely on `HashSet`/`Set.of` iteration order

**Next:** [Map](./03_map.md)
