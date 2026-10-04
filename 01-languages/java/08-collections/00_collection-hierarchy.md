# Collection Hierarchy

The Collections Framework is a small number of **interfaces** plus many **implementations**. Knowing the interface tree tells you what every collection can do before you read a single implementation.

```
                         Iterable<E>
                              │
                       Collection<E>                    Map<K,V>   (separate tree: NOT a Collection)
        ┌─────────────────┼─────────────────┐              │
     List<E>           Set<E>            Queue<E>      SortedMap<K,V>
        │                 │                 │              │
        │            SortedSet<E>        Deque<E>      NavigableMap<K,V>
        │                 │
        │            NavigableSet<E>
```

Since **Java 21**, three **Sequenced** interfaces were added to give a uniform "first/last/reversed" API (see [below](#sequenced-collections-java-21)).

## The interfaces

| Interface                          | Models                                   | Duplicates   | Order                     | Index access | Key operations                                 |
| ---------------------------------- | ---------------------------------------- | ------------ | ------------------------- | ------------ | ---------------------------------------------- |
| `Iterable<E>`                      | Anything you can loop over with for-each | n/a          | n/a                       | No           | `iterator()`, `forEach`                        |
| `Collection<E>`                    | A group of elements                      | depends      | depends                   | No           | `add`, `remove`, `contains`, `size`, `stream`  |
| `List<E>`                          | An ordered sequence                      | **Yes**      | Insertion (by position)   | **Yes**      | `get(i)`, `set(i, e)`, `add(i, e)`             |
| `Set<E>`                           | A mathematical set                       | **No**       | Depends on implementation | No           | `add` (false if present), `contains`           |
| `SortedSet<E>` / `NavigableSet<E>` | A set kept sorted                        | No           | **Sorted**                | No           | `first`, `last`, `floor`, `ceiling`, `headSet` |
| `Queue<E>`                         | Elements awaiting processing             | Yes          | Typically FIFO            | No           | `offer`, `poll`, `peek`                        |
| `Deque<E>`                         | Double-ended queue (also a stack)        | Yes          | Both ends                 | No           | `addFirst`, `pollLast`, `push`, `pop`          |
| `Map<K,V>`                         | Key → value associations                 | Keys: **No** | Depends                   | By key       | `put`, `get`, `containsKey`                    |
| `SortedMap` / `NavigableMap`       | A map sorted by key                      | Keys: No     | **Sorted** by key         | By key       | `firstKey`, `floorEntry`, `subMap`             |

Details: [List](./01_list.md), [Set](./02_set.md), [Map](./03_map.md), [Queue and Deque](./04_queue-and-deque.md).

**Why isn't `Map` a `Collection`?** A `Collection` is a group of single elements; a `Map` is a group of pairs. Maps expose collection **views** (`keySet()`, `values()`, `entrySet()`).

## The implementations

| Interface | General-purpose            | Insertion-ordered | Sorted                        | Special                                                                    |
| --------- | -------------------------- | ----------------- | ----------------------------- | -------------------------------------------------------------------------- |
| `List`    | **`ArrayList`**            | (all lists are)   | n/a                           | `LinkedList`, `CopyOnWriteArrayList`, immutable `List.of`                  |
| `Set`     | **`HashSet`**              | `LinkedHashSet`   | `TreeSet`                     | `EnumSet`, `CopyOnWriteArraySet`, `ConcurrentSkipListSet`, `Set.of`        |
| `Map`     | **`HashMap`**              | `LinkedHashMap`   | `TreeMap`                     | `EnumMap`, `IdentityHashMap`, `WeakHashMap`, `ConcurrentHashMap`, `Map.of` |
| `Queue`   | `ArrayDeque`, `LinkedList` | n/a               | `PriorityQueue` (by priority) | `BlockingQueue` family, `ConcurrentLinkedQueue`                            |
| `Deque`   | **`ArrayDeque`**           | n/a               | n/a                           | `LinkedList`, `LinkedBlockingDeque`, `ConcurrentLinkedDeque`               |

Bold = the usual first choice. Implementation files: [06 ArrayList](./06_arraylist.md), [07 LinkedList](./07_linkedlist.md), [08 HashMap/HashSet](./08_hashmap-and-hashset.md), [09 TreeMap/TreeSet](./09_treemap-and-treeset.md), [10 PriorityQueue](./10_priorityqueue.md), [11 specialized](./11_specialized-collections.md).

### Naming pattern

The class name tells you the **interface + the data structure**:

| Name                              | =                               | Meaning                                         |
| --------------------------------- | ------------------------------- | ----------------------------------------------- |
| `ArrayList`                       | List + array                    | Resizable array                                 |
| `LinkedList`                      | List (and Deque) + linked nodes | Doubly linked list                              |
| `HashMap` / `HashSet`             | Map / Set + hash table          | Hash-based lookup                               |
| `TreeMap` / `TreeSet`             | Map / Set + red-black tree      | Sorted                                          |
| `LinkedHashMap` / `LinkedHashSet` | hash table + linked list        | Hash lookup **and** predictable iteration order |
| `PriorityQueue`                   | Queue + heap                    | Smallest (or highest-priority) element first    |
| `ArrayDeque`                      | Deque + array                   | Resizable circular array                        |

## Operations common to every `Collection`

```java
Collection<String> c = new ArrayList<>();

c.add("a");                  // boolean: true if the collection changed
c.addAll(List.of("b", "c"));
c.remove("a");               // removes ONE occurrence (by equals); true if present
c.removeAll(other);          // remove everything that is also in `other`
c.retainAll(other);          // keep only elements that are also in `other` (intersection)
c.removeIf(s -> s.isEmpty());// conditional removal (safe while "iterating")
c.contains("b");             // uses equals()
c.containsAll(other);
c.size();  c.isEmpty();  c.clear();
c.iterator();  c.forEach(System.out::println);
c.stream();                  // see 09-functional-java
c.toArray(new String[0]);    // or toArray(String[]::new)
```

All of them rely on `equals` for matching (and hash-based ones on `hashCode`): [04-oop/14_equals-and-hashcode.md](../04-oop/14_equals-and-hashcode.md).

## Sequenced collections (Java 21)

Before Java 21, getting "the first element" or "reverse order" had different, inconsistent APIs for each collection (`list.get(0)`, `deque.getFirst()`, `sortedSet.first()`, `linkedHashSet.iterator().next()`). Three new interfaces unify them for collections that have a **defined encounter order**:

```
SequencedCollection<E>  ◄── List, Deque, (and SequencedSet)
SequencedSet<E>         ◄── LinkedHashSet, SortedSet (TreeSet)
SequencedMap<K,V>       ◄── LinkedHashMap, SortedMap (TreeMap)
```

```java
interface SequencedCollection<E> extends Collection<E> {
    SequencedCollection<E> reversed();       // a reverse-ordered VIEW
    void addFirst(E e);   void addLast(E e);
    E getFirst();         E getLast();
    E removeFirst();      E removeLast();
}
```

```java
List<String> list = new ArrayList<>(List.of("a", "b", "c"));
list.getFirst();                  // "a"      (previously list.get(0))
list.getLast();                   // "c"      (previously list.get(list.size() - 1))
list.reversed();                  // view: [c, b, a]
list.addFirst("z");               // [z, a, b, c]

LinkedHashSet<Integer> set = new LinkedHashSet<>(List.of(1, 2, 3));
set.getLast();                    // 3
set.reversed();                   // [3, 2, 1]

SequencedMap<String, Integer> map = new LinkedHashMap<>();
map.putLast("a", 1);  map.putFirst("b", 2);
map.firstEntry();  map.lastEntry();  map.reversed();
```

`HashSet` and `HashMap` are **not** sequenced (no defined order). On older Java versions, use the older methods.

## Ordered vs sorted

| Term                                    | Meaning                                                 | Examples                                                                       |
| --------------------------------------- | ------------------------------------------------------- | ------------------------------------------------------------------------------ |
| **Unordered**                           | Iteration order is unspecified and may change           | `HashSet`, `HashMap`                                                           |
| **Ordered** (insertion/encounter order) | Iteration order is the order elements were added/placed | `ArrayList`, `LinkedHashSet`, `LinkedHashMap`                                  |
| **Sorted**                              | Iteration order follows a **comparison** rule           | `TreeSet`, `TreeMap`, `PriorityQueue` (partially: only the head is guaranteed) |

Do not rely on the order of a `HashSet`/`HashMap`: it can differ between runs, Java versions and even after resizing. `Set.of` / `Map.of` deliberately randomize their iteration order per JVM run.

## Null handling at a glance

| Collection                                            | `null` elements / keys                                           |
| ----------------------------------------------------- | ---------------------------------------------------------------- |
| `ArrayList`, `LinkedList`, `HashSet`, `LinkedHashSet` | Allowed                                                          |
| `HashMap`, `LinkedHashMap`                            | One `null` key, many `null` values                               |
| `TreeSet`, `TreeMap`                                  | `null` rejected with natural ordering (`NullPointerException`)   |
| `ArrayDeque`, `PriorityQueue`                         | **Not** allowed (`null` is the "empty" signal for `poll`/`peek`) |
| `ConcurrentHashMap`                                   | Not allowed (neither keys nor values)                            |
| `List.of`, `Set.of`, `Map.of`                         | Not allowed                                                      |

Avoid `null` in collections whenever you can.

## Thread-safety

Standard collections are **not** thread-safe. For concurrent use see [14-concurrency/08_concurrent-collections.md](../14-concurrency/08_concurrent-collections.md) (`ConcurrentHashMap`, `CopyOnWriteArrayList`, `BlockingQueue`) rather than `Collections.synchronizedXxx` wrappers. The legacy classes `Vector`, `Hashtable` and `Stack` are synchronized and should not be used in new code.

## Legacy classes to avoid

| Class                       | Problem                                                          | Use instead                                                          |
| --------------------------- | ---------------------------------------------------------------- | -------------------------------------------------------------------- |
| `Vector`                    | Every method synchronized; legacy API                            | `ArrayList` (or `CopyOnWriteArrayList` if concurrent reads dominate) |
| `Stack` (extends `Vector`)  | Synchronized, exposes `List` methods that break stack discipline | `ArrayDeque`                                                         |
| `Hashtable`                 | Synchronized, no nulls, legacy                                   | `HashMap` / `ConcurrentHashMap`                                      |
| `Dictionary`, `Enumeration` | Obsolete                                                         | `Map`, `Iterator`                                                    |
| `Properties`                | Still used for config; it is a `Hashtable<Object,Object>`        | Fine for `.properties` files; otherwise typed config                 |

## Programming to the interface

```java
List<String> names = new ArrayList<>();               // declare by interface
Map<String, Integer> counts = new HashMap<>();

void process(Collection<String> input) { ... }        // accept the most general type that works
List<Order> findOrders() { ... }                      // return an interface, not ArrayList
```

Benefits: swap implementations in one place, accept more argument types, easier testing ([04-oop/09_interfaces.md](../04-oop/09_interfaces.md)). Declare a **specific** type only when you need its extra methods (`TreeMap` for `floorKey`, `ArrayDeque`/`LinkedList` for `Deque`).

## Quick chooser

| I need...                                 | Use                                     |
| ----------------------------------------- | --------------------------------------- |
| An ordered list with fast index access    | `ArrayList`                             |
| A unique collection, fast membership test | `HashSet`                               |
| Unique **and** insertion order preserved  | `LinkedHashSet`                         |
| Unique **and** sorted                     | `TreeSet`                               |
| Key → value lookup                        | `HashMap`                               |
| Key → value with insertion order / LRU    | `LinkedHashMap`                         |
| Key → value sorted by key, range queries  | `TreeMap`                               |
| FIFO queue or LIFO stack                  | `ArrayDeque`                            |
| "Next most important" element             | `PriorityQueue`                         |
| Enum keys or sets of enum constants       | `EnumMap` / `EnumSet`                   |
| Read-only data                            | `List.of`, `Set.of`, `Map.of`, `copyOf` |

Full decision guide: [15_collection-performance-and-selection.md](./15_collection-performance-and-selection.md).

## Common mistakes

| Mistake                                                       | Symptom                                                     | Fix                                                      |
| ------------------------------------------------------------- | ----------------------------------------------------------- | -------------------------------------------------------- |
| Declaring variables by implementation (`ArrayList<String> x`) | Hard to change, over-specific parameters                    | Declare by interface                                     |
| Assuming `HashSet`/`HashMap` iterate in insertion order       | Order changes unexpectedly                                  | `LinkedHashSet`/`LinkedHashMap` or sort                  |
| Using `Vector`, `Stack` or `Hashtable`                        | Slow synchronization, awkward API                           | Modern replacements                                      |
| Treating `Map` as a `Collection`                              | `cannot find symbol`                                        | Use `keySet()`/`values()`/`entrySet()`                   |
| Using raw collection types                                    | Unchecked warnings, `ClassCastException`                    | Parameterize ([generics](../07-generics/README.md))      |
| Relying on `null` support everywhere                          | `NullPointerException` in `TreeMap`, `ArrayDeque`, `Map.of` | Check each implementation's rules                        |
| Sharing a plain collection across threads                     | Lost updates, `ConcurrentModificationException`             | Concurrent collections                                   |
| Not knowing which operations are optional                     | `UnsupportedOperationException` on `List.of(...).add`       | See [13](./13_immutable-and-unmodifiable-collections.md) |

## Key takeaways

- Two trees: `Collection` (List, Set, Queue/Deque) and `Map`; maps expose collection views
- `List` = ordered with duplicates, `Set` = unique, `Queue`/`Deque` = processing order, `Map` = key lookup
- Class names say the structure: `Array`, `Linked`, `Hash`, `Tree`
- Java 21 adds `SequencedCollection`/`SequencedSet`/`SequencedMap` (`getFirst`, `getLast`, `reversed`)
- Declare by interface; choose the implementation by access pattern; avoid `Vector`, `Stack`, `Hashtable`

**Next:** [List](./01_list.md)
