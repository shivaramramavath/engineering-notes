# Immutable and Unmodifiable Collections

Shared mutable collections are a leading cause of bugs: a caller changes a list you thought was yours, a "constant" list is modified, one thread changes a map while another reads it. Java gives several ways to prevent modification, and they are **not** equivalent. This file separates them.

```java
List<String> a = List.of("x", "y");                       // immutable
List<String> b = Collections.unmodifiableList(source);    // read-only VIEW of a list that may still change
List<String> c = List.copyOf(source);                     // immutable SNAPSHOT
List<String> d = Arrays.asList("x", "y");                 // fixed-size, but elements can be replaced
```

## Vocabulary

| Term | Meaning |
|------|---------|
| **Mutable** | You can add, remove and replace elements (`ArrayList`) |
| **Unmodifiable (view)** | *You* cannot modify it through this reference, but the underlying collection may change, and you will see the changes |
| **Immutable** | No one can modify it, ever (`List.of`, `List.copyOf`), as long as the **elements** are immutable too |
| **Shallow** immutability | The collection structure is frozen, but element objects may still be mutable |
| **Defensive copy** | A private copy made so later changes elsewhere cannot affect you |

## The factory methods (Java 9+): `List.of`, `Set.of`, `Map.of`

```java
List<String> list = List.of("a", "b", "c");
Set<Integer> set = Set.of(1, 2, 3);
Map<String, Integer> map = Map.of("a", 1, "b", 2);                       // up to 10 pairs
Map<String, Integer> big = Map.ofEntries(Map.entry("a", 1), Map.entry("b", 2));   // any number
```

```java
list.add("d");         // UnsupportedOperationException
list.set(0, "z");      // UnsupportedOperationException
list.remove("a");      // UnsupportedOperationException
list.sort(null);       // UnsupportedOperationException
list.iterator().remove();   // UnsupportedOperationException
```

### Properties

| Property | Detail |
|----------|--------|
| **Truly immutable** (structure) | All mutators throw `UnsupportedOperationException` |
| **No `null`** | `List.of("a", null)` → `NullPointerException`; also `contains(null)` / `indexOf(null)` on `List.of` throw `NullPointerException` |
| `Set.of` / `Map.of`: **no duplicates** | Duplicate element or key → `IllegalArgumentException` |
| `Set.of` / `Map.of`: **iteration order unspecified and randomized per JVM run** | Never depend on it ([08](./08_hashmap-and-hashset.md)) |
| Compact and fast | Special small implementations for 0-2 elements; array-backed otherwise |
| **Value-based** classes | Do not synchronize on them or depend on identity (`==`) |
| Serializable | Yes |
| Null-hostile `toArray`, `stream` etc. | Work normally |

Use them for **constants, test data, and return values you do not want modified**.

```java
private static final Set<String> ALLOWED = Set.of("GET", "POST", "PUT");      // a constant that is really constant
```

### `copyOf`: immutable snapshots

```java
List<String> snapshot = List.copyOf(mutableList);          // copies; later changes to mutableList are not seen
Set<String> s = Set.copyOf(collection);                    // duplicates are fine here (unlike Set.of)
Map<String, Integer> m = Map.copyOf(map);
```

`copyOf` returns the **same instance** if its argument is already an immutable collection of the right kind (no needless copy), and throws `NullPointerException` on `null` elements.

## Unmodifiable **views**: `Collections.unmodifiableXxx`

```java
List<String> source = new ArrayList<>(List.of("a", "b"));
List<String> view = Collections.unmodifiableList(source);

view.add("c");          // UnsupportedOperationException: cannot modify through the view
source.add("c");        // allowed: the owner can still change the data
view;                   // [a, b, c]: the view SEES the change
```

A view is a thin **wrapper** around the original. It is not a copy.

| Factory | Wraps |
|---------|-------|
| `unmodifiableCollection`, `unmodifiableList`, `unmodifiableSet`, `unmodifiableMap` | The base interfaces |
| `unmodifiableSortedSet`, `unmodifiableNavigableSet`, `unmodifiableSortedMap`, `unmodifiableNavigableMap` | Sorted/navigable variants |
| `unmodifiableSequencedCollection`, `unmodifiableSequencedSet`, `unmodifiableSequencedMap` | Java 21 sequenced types |

Characteristics:
- Allows `null` elements (if the source does)
- O(1) to create; no copying
- Does **not** make elements immutable; mutable elements can still change
- Keeps the whole source alive

### View vs copy: choose deliberately

```java
class Team {
    private final List<String> members = new ArrayList<>();

    // View: cheap, always current, but exposes changes to callers who keep it
    public List<String> membersView() { return Collections.unmodifiableList(members); }

    // Snapshot: a stable picture at one moment; O(n) to create
    public List<String> membersSnapshot() { return List.copyOf(members); }
}
```

| Want | Use |
|------|-----|
| Callers see **live** state but cannot change it | `Collections.unmodifiableList(...)` |
| Callers get a **stable** value that will not change under them | `List.copyOf(...)` |
| A constant | `List.of(...)` |
| Hide an internal collection cheaply in a getter | View (document that it is live) |
| Return value stored by the caller for a long time | Snapshot |

## Other collections that cannot (fully) change

| Creation | Behavior |
|----------|----------|
| `Arrays.asList(array)` | **Fixed-size**: `set` works and **writes through to the array**, `add`/`remove` throw |
| `stream.toList()` (Java 16+) | **Unmodifiable**, allows `null`; returns a list that cannot be changed |
| `Collectors.toList()` | A **mutable** list (implementation unspecified: do not rely on it) |
| `Collectors.toUnmodifiableList()/Set()/Map()` (Java 10+) | Immutable; no `null` |
| `Collections.emptyList()/emptySet()/emptyMap()` | Immutable singletons |
| `Collections.singletonList(x)`, `singleton(x)`, `singletonMap(k, v)` | Immutable, allow `null` |
| `Collections.nCopies(n, x)` | Immutable list of `n` references to one object |
| `Map.entry(k, v)` | Immutable entry (no `null`); `AbstractMap.SimpleEntry` is mutable |
| `List.subList(...)` | A view of the parent: mutable if the parent is |

```java
List<Integer> a = Stream.of(1, 2, 3).toList();               // unmodifiable
a.add(4);                                                    // UnsupportedOperationException
List<Integer> b = Stream.of(1, 2, 3).collect(Collectors.toList());   // mutable ArrayList in practice
```

## Immutable collection ≠ immutable contents

```java
List<StringBuilder> list = List.of(new StringBuilder("a"));
list.get(0).append("!");                 // the list is immutable; the element is not
list;                                    // [a!]

record Person(String name, List<String> tags) { }          // looks immutable, but `tags` can change if the caller keeps it
```

For **deep** immutability use immutable element types (records of immutable fields, `String`, `LocalDate`, wrappers, enums) and copy mutable inputs:

```java
public record Person(String name, List<String> tags) {
    public Person {                                         // compact constructor: normalize
        tags = List.copyOf(tags);                           // defensive copy + immutable; rejects nulls
    }
}
```

See [12-modern-java/01_records.md](../12-modern-java/01_records.md) and [23-design-and-clean-code/04_immutability.md](../23-design-and-clean-code/04_immutability.md).

## Defensive copying at API boundaries

```java
public class Order {
    private final List<Item> items;

    public Order(List<Item> items) {
        this.items = List.copyOf(items);              // copy IN: the caller's list can change freely afterwards
    }

    public List<Item> items() {
        return items;                                // already immutable: safe to return directly (no copy needed)
    }
}
```

With `List.copyOf` in the constructor, the getter can return the field directly. With a mutable internal list, return `List.copyOf(...)` or an unmodifiable view ([04-oop/04_encapsulation-and-access-modifiers.md](../04-oop/04_encapsulation-and-access-modifiers.md), [02-methods/01_pass-by-value.md](../02-methods/01_pass-by-value.md)).

## Why prefer immutable collections

| Benefit | Explanation |
|---------|-------------|
| **Safe sharing** | No defensive copies needed on every read |
| **Thread-safe** | Immutable data needs no synchronization (safe publication: `final` fields: [14-concurrency/04_memory-model-and-volatile.md](../14-concurrency/04_memory-model-and-volatile.md)) |
| **Predictable** | No action at a distance |
| **Efficient** | Compact implementations, no wasted capacity |
| **Valid map keys / set elements** | They cannot change after insertion |
| **Easy reasoning and caching** | Value semantics |

Cost: "changing" means creating a new collection (`new ArrayList<>(old)` then modify, then `copyOf`). For large collections that change often, keep a **mutable** internal structure and expose immutable **snapshots** or views.

## Third-party immutable collections

| Library | Offers |
|---------|--------|
| **Guava** | `ImmutableList`, `ImmutableMap`, `ImmutableSet`, builders, `ImmutableMultimap` |
| **Vavr** / **Eclipse Collections** | Persistent (structure-sharing) immutable collections with cheap "modified copies" |

The JDK's `List.of` family is enough for most code; add a library only for persistent collections or richer builders.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Modifying a `List.of` / `Set.of` / `Map.of` result | `UnsupportedOperationException` | `new ArrayList<>(List.of(...))` if you need a mutable one |
| Passing `null` to `List.of`, `Set.of`, `Map.of`, `copyOf` | `NullPointerException` | Remove `null`s, or use `ArrayList` |
| `List.of(...).contains(null)` | `NullPointerException` | Guard before calling |
| `Set.of` with duplicates | `IllegalArgumentException` | `Set.copyOf` or a `HashSet` |
| Depending on `Set.of`/`Map.of` iteration order | Differs per run | Sort, or use `LinkedHashSet`/`TreeSet` |
| Thinking `Collections.unmodifiableList(x)` is immutable | Changes to `x` appear in the "unmodifiable" list | `List.copyOf(x)` |
| Returning an internal mutable list from a getter | Callers corrupt your state | Return an unmodifiable view or a copy |
| Assuming `Collectors.toList()` is immutable (or mutable) | Surprises either way | `toList()` (stream) or `toUnmodifiableList()` for immutable; `toCollection(ArrayList::new)` for mutable |
| `Arrays.asList(...)` treated as a general list | `UnsupportedOperationException` on `add`; array write-through | Wrap in `new ArrayList<>(...)` |
| Immutable list of mutable elements | Contents change anyway | Immutable element types, deep copies |
| Using an unmodifiable view and expecting a snapshot | Surprise updates | Use `copyOf` |
| Synchronizing on `List.of(...)` instances | Lint warnings, shared singletons | Use a dedicated lock object |

## Key takeaways

- **Immutable**: `List.of`, `Set.of`, `Map.of`, `copyOf`, `Stream.toList()`: no changes by anyone, no `null` (except `Stream.toList`)
- **Unmodifiable view**: `Collections.unmodifiableXxx`: read-only for you, but live; the owner can still change it
- **Snapshot vs view**: `copyOf` copies; `unmodifiableXxx` wraps
- Collection immutability is **shallow**: use immutable element types for real safety
- Copy mutable inputs in constructors; return immutable or unmodifiable collections from getters
- Never rely on `Set.of`/`Map.of` ordering

**Next:** [Collections and Arrays Utilities](./14_collections-and-arrays-utilities.md)