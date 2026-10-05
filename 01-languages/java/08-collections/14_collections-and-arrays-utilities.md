# Collections and Arrays Utilities

Three utility classes hold static helper methods you will use constantly. Learn what they offer so you do not write your own (usually slower and buggier) versions.

| Class | Package | Operates on |
|-------|---------|-------------|
| **`Collections`** (with an **s**) | `java.util` | `Collection`s, `List`s, `Map`s |
| **`Arrays`** | `java.util` | Arrays |
| **`Objects`** | `java.util` | Any objects (null-safe helpers) |

Do not confuse `Collections` (the utility class) with `Collection` (the interface), nor `Objects` with `Object`.

## `Collections`

### Sorting, searching, ordering

```java
List<Integer> list = new ArrayList<>(List.of(5, 2, 9, 1));

Collections.sort(list);                           // natural order (list.sort(null) is equivalent)
Collections.sort(list, Comparator.reverseOrder());
Collections.reverse(list);                        // reverse in place
Collections.shuffle(list);                        // random order
Collections.shuffle(list, new Random(42));        // reproducible shuffle
Collections.swap(list, 0, 1);
Collections.rotate(list, 2);                      // rotate right by 2 (negative = left)

Collections.sort(list);
int pos = Collections.binarySearch(list, 5);      // list MUST be sorted with the same ordering
                                                  // found → index; not found → -(insertion point) - 1
```

### Extremes and counting

```java
Collections.max(list);                            // largest (NoSuchElementException on an empty collection)
Collections.min(list, comparator);
Collections.frequency(list, 5);                   // number of elements equal to 5
Collections.disjoint(a, b);                       // true if they share no elements
```

### Filling and copying

```java
Collections.fill(list, 0);                        // set every element to 0
Collections.addAll(list, 7, 8, 9);                // varargs add (faster than addAll(Arrays.asList(...)))
Collections.replaceAll(list, 1, 100);             // replace all equal elements

List<Integer> dest = new ArrayList<>(Collections.nCopies(list.size(), 0));    // dest needs the SAME size first
Collections.copy(dest, list);                     // IndexOutOfBoundsException if dest is smaller than src
List<Integer> better = new ArrayList<>(list);     // usually what you actually want
```

`Collections.copy` is a classic trap: the destination must already be at least as long as the source, because `copy` overwrites by index. For a copy, use `new ArrayList<>(list)`.

### Factories for special collections

```java
Collections.emptyList();   Collections.emptySet();   Collections.emptyMap();     // immutable empties
Collections.singletonList("x");   Collections.singleton("x");                      // one element
Collections.singletonMap("k", "v");
Collections.nCopies(3, "ab");                                                      // [ab, ab, ab], immutable
Collections.reverseOrder();                       // Comparator for the reverse of natural order
Collections.reverseOrder(cmp);                    // reverse of a given comparator
Collections.newSetFromMap(new ConcurrentHashMap<>());   // a Set backed by any Map
```

Since Java 9, `List.of()`, `Set.of()`, `Map.of()` cover most of these.

### Wrappers

| Wrapper | Result | Notes |
|---------|--------|-------|
| `unmodifiableList/Set/Map/Collection(...)` | Read-only **view** | [13](./13_immutable-and-unmodifiable-collections.md) |
| `synchronizedList/Set/Map/Collection(...)` | Each method synchronized | Compound actions and **iteration** still need external locking |
| `checkedList/Set/Map(..., type)` | Runtime type checks on insertion | Catches heap pollution at the source ([07-generics/03_type-erasure.md](../07-generics/03_type-erasure.md)) |

```java
List<String> sync = Collections.synchronizedList(new ArrayList<>());
synchronized (sync) {                             // REQUIRED while iterating
    for (String s : sync) { ... }
}
```

Prefer `java.util.concurrent` collections to synchronized wrappers ([14-concurrency/08_concurrent-collections.md](../14-concurrency/08_concurrent-collections.md)).

## `Arrays`

### Printing and comparing

```java
int[] a = {3, 1, 2};
System.out.println(Arrays.toString(a));                    // [3, 1, 2]
int[][] grid = {{1, 2}, {3, 4}};
System.out.println(Arrays.deepToString(grid));             // [[1, 2], [3, 4]]

Arrays.equals(new int[]{1, 2}, new int[]{1, 2});           // true (contents)
Arrays.deepEquals(grid1, grid2);                           // nested arrays
Arrays.hashCode(a);   Arrays.deepHashCode(grid);

Arrays.compare(a, b);                                      // lexicographic comparison (Java 9+)
Arrays.mismatch(a, b);                                     // index of the first difference, -1 if equal (Java 9+)
```

`a.equals(b)` and `a.hashCode()` on arrays use **identity**: always use the `Arrays` versions ([arrays](../01-fundamentals/07_arrays.md)).

### Sorting and searching

```java
Arrays.sort(a);                                   // primitives: dual-pivot quicksort
Arrays.sort(a, 1, 4);                             // sort a range [1, 4)
Arrays.sort(names, String.CASE_INSENSITIVE_ORDER);// objects: stable merge sort with a comparator
Arrays.sort(people, Comparator.comparingInt(Person::age));
Arrays.parallelSort(bigArray);                    // multi-core sort for large arrays (>~8K elements)

Arrays.binarySearch(a, 2);                        // array MUST be sorted; same return convention as Collections
```

Comparator-based sorting needs **object** arrays (`Integer[]`, not `int[]`).

### Filling and copying

```java
Arrays.fill(a, -1);                               // all elements
Arrays.fill(a, 1, 3, 0);                          // a range
Arrays.setAll(squares, i -> i * i);               // from an index function
int[] copy1 = Arrays.copyOf(a, a.length);         // copy (pad with zeros / truncate to the new length)
int[] copy2 = Arrays.copyOfRange(a, 1, 3);        // [from, to)
// 2-D arrays: Arrays.fill(grid, -1) fails; fill each row
for (int[] row : grid) Arrays.fill(row, -1);
```

### Conversion and streams

```java
List<String> view = Arrays.asList("a", "b");      // fixed-size view backed by the array
IntStream s = Arrays.stream(a);                   // array → stream
int sum = Arrays.stream(a).sum();
double avg = Arrays.stream(a).average().orElse(0);
int[] fromList = list.stream().mapToInt(Integer::intValue).toArray();
```

## `Objects`

Null-safe helpers, indispensable in `equals`, `hashCode`, `toString` and argument checks.

```java
Objects.equals(a, b);                             // true if both null or a.equals(b); never throws
Objects.hash(name, age, email);                   // combined hash code (varargs; allocates an array)
Objects.hashCode(o);                              // 0 for null
Objects.toString(o);                              // "null" for null
Objects.toString(o, "n/a");                       // custom default
Objects.requireNonNull(x);                        // throws NullPointerException if x is null; returns x
Objects.requireNonNull(x, "x must not be null");
Objects.requireNonNull(x, () -> "expensive message " + id);   // lazy message (Supplier)
Objects.requireNonNullElse(x, defaultValue);
Objects.requireNonNullElseGet(x, () -> compute());
Objects.isNull(x);   Objects.nonNull(x);          // handy as method references: filter(Objects::nonNull)
Objects.compare(a, b, comparator);                // null-tolerant if the comparator is
Objects.checkIndex(i, length);                    // IndexOutOfBoundsException with a good message
```

```java
public Person(String name, int age) {
    this.name = Objects.requireNonNull(name, "name");
    this.age = age;
}
@Override public boolean equals(Object o) {
    return o instanceof Person p && age == p.age && Objects.equals(name, p.name);
}
@Override public int hashCode() { return Objects.hash(name, age); }
```

See [04-oop/13_object-class.md](../04-oop/13_object-class.md) and [04-oop/14_equals-and-hashcode.md](../04-oop/14_equals-and-hashcode.md).

## `Map.Entry` helpers (for sorting entries)

```java
Map.Entry.comparingByKey();
Map.Entry.comparingByValue();
Map.Entry.<String, Integer>comparingByValue(Comparator.reverseOrder());

map.entrySet().stream()
   .sorted(Map.Entry.<String, Integer>comparingByValue().reversed())
   .limit(3)
   .forEach(e -> System.out.println(e.getKey() + "=" + e.getValue()));
```

## Which helper for which job

| Task | Use |
|------|-----|
| Sort a list | `list.sort(cmp)` (or `Collections.sort`) |
| Sort an array | `Arrays.sort` |
| Reverse a list | `Collections.reverse` (or `list.reversed()` in Java 21 for a view) |
| Shuffle | `Collections.shuffle(list, random)` |
| Largest/smallest | `Collections.max/min`, or `stream().max(...)` |
| Count occurrences | `Collections.frequency` (or `groupingBy(counting())` for all) |
| Copy a list | `new ArrayList<>(list)` / `List.copyOf(list)` |
| Copy an array | `Arrays.copyOf`, `clone()` |
| Print an array | `Arrays.toString` / `deepToString` |
| Compare arrays | `Arrays.equals` / `deepEquals` |
| Fill with a value | `Collections.fill`, `Arrays.fill` |
| Immutable constants | `List.of`, `Set.of`, `Map.of` |
| Null-safe equals/hash/toString | `Objects.*` |
| Fail fast on `null` | `Objects.requireNonNull` |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `Collections.copy(dest, src)` with an empty `dest` | `IndexOutOfBoundsException` | `new ArrayList<>(src)` |
| `Collections.max` on an empty collection | `NoSuchElementException` | Check emptiness, or use `stream().max(...)` (an `Optional`) |
| `Collections.sort` on `List.of(...)` or `Arrays.asList` of an immutable source | `UnsupportedOperationException` | Sort a mutable copy |
| `binarySearch` on unsorted data or with a different comparator | Wrong index | Sort first with the same ordering |
| `System.out.println(array)` | `[I@6d06d69c` | `Arrays.toString` |
| `array.equals(other)` / `hashCode` | Identity semantics | `Arrays.equals` / `Arrays.hashCode` |
| `Arrays.fill(grid, 0)` for a 2-D array | `ArrayStoreException` | Fill each row |
| `Arrays.sort(int[], comparator)` | Does not compile | Use `Integer[]` or sort the stream |
| `Arrays.asList(int[])` | A list with one element | Use `Integer[]` or `IntStream.boxed()` |
| `Collections.synchronizedList` iterated without locking | `ConcurrentModificationException`, races | Hold the wrapper's lock, or use concurrent collections |
| `Objects.hash` in a hot path | Varargs array + boxing allocations | Manual hash for performance-critical classes |
| Confusing `Collection` and `Collections`, `Object` and `Objects` | `cannot find symbol` | Plural = utility class |

## Key takeaways

- `Collections`: sort, reverse, shuffle, min/max, frequency, binarySearch, factories (`emptyList`, `nCopies`), wrappers (`unmodifiable`, `synchronized`)
- `Arrays`: `toString`, `equals`, `sort`, `fill`, `copyOf`, `asList`, `stream`; use `deep*` variants for nested arrays
- `Objects`: null-safe `equals`, `hash`, `toString`, and `requireNonNull` for argument checks
- Know the traps: `Collections.copy` size requirement, `Arrays.asList` fixed size, `binarySearch` needing a sorted input

**Next:** [Collection Performance and Selection](./15_collection-performance-and-selection.md)