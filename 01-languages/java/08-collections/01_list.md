# List

A `List<E>` is an **ordered sequence** of elements that allows **duplicates** and gives **positional (index) access**. It is the most-used collection interface. This file covers the **contract**; how `ArrayList` and `LinkedList` implement it is in [06](./06_arraylist.md) and [07](./07_linkedlist.md).

```java
List<String> names = new ArrayList<>();
names.add("Ada");
names.add("Linus");
names.add("Ada");                    // duplicates are fine
names.get(1);                        // "Linus"
names.size();                        // 3
```

## What `List` promises

| Property | Meaning |
|----------|---------|
| **Ordered** | Elements keep the position you gave them; iteration follows that order |
| **Indexed** | Zero-based positions: `get(0)` … `get(size() - 1)` |
| **Duplicates allowed** | The same value (or object) can appear many times |
| Equality | Two lists are `equals` if they have the **same elements in the same order** (regardless of implementation) |

```java
new ArrayList<>(List.of(1, 2, 3)).equals(new LinkedList<>(List.of(1, 2, 3)));   // true
```

## Creating lists

```java
List<String> a = new ArrayList<>();                       // empty, mutable
List<String> b = new ArrayList<>(List.of("x", "y"));      // mutable copy
List<String> c = List.of("x", "y", "z");                  // IMMUTABLE, no nulls (Java 9+)
List<String> d = Arrays.asList("x", "y");                 // fixed-size view over an array (see below)
List<String> e = List.copyOf(someCollection);             // immutable copy
List<String> f = Stream.of("x", "y").toList();            // unmodifiable (Java 16+)
List<String> g = new ArrayList<>(1000);                   // initial capacity hint
List<String> h = Collections.nCopies(3, "x");             // immutable: [x, x, x]
```

Which are mutable and why it matters: [13_immutable-and-unmodifiable-collections.md](./13_immutable-and-unmodifiable-collections.md).

## Core operations

```java
List<String> list = new ArrayList<>(List.of("a", "b", "c"));

// Read
list.get(0);                  // "a"
list.indexOf("b");            // 1  (-1 if absent)
list.lastIndexOf("b");
list.contains("c");           // true  (uses equals)
list.isEmpty();  list.size();

// Write
list.add("d");                // append
list.add(1, "x");             // insert at index 1, shifting the rest right
list.set(0, "A");             // REPLACE the element at index 0; returns the old one
list.remove(2);               // remove by INDEX (returns the removed element)
list.remove("x");             // remove first occurrence by VALUE (returns boolean)
list.addAll(List.of("p", "q"));
list.addAll(0, List.of("m"));
list.clear();

// Bulk / functional
list.removeIf(s -> s.startsWith("a"));
list.replaceAll(String::toUpperCase);
list.sort(Comparator.naturalOrder());       // or list.sort(null) for natural order
list.forEach(System.out::println);
```

### Java 21 additions (SequencedCollection)

```java
list.getFirst();   list.getLast();
list.addFirst("z");  list.addLast("y");
list.removeFirst();  list.removeLast();
list.reversed();                           // reverse-ordered VIEW
```

([collection hierarchy](./00_collection-hierarchy.md#sequenced-collections-java-21)). On older versions use `get(0)` and `get(size() - 1)`.

## `remove(int)` vs `remove(Object)`: the classic trap

```java
List<Integer> nums = new ArrayList<>(List.of(10, 20, 30, 1));

nums.remove(1);                      // INDEX 1 → removes 20          → [10, 30, 1]
nums.remove(Integer.valueOf(1));     // VALUE 1 → removes the element 1 → [10, 30]
```

For `List<Integer>`, a literal `int` selects `remove(int index)`; box it to remove by value ([overload resolution](../02-methods/02_overloading-and-varargs.md), [wrappers](../01-fundamentals/04_wrapper-classes-and-autoboxing.md)).

## Iterating

```java
for (String s : list) { ... }                            // simplest; read-only structure
for (int i = 0; i < list.size(); i++) { list.get(i); }   // when you need the index (fine for ArrayList; O(n²) for LinkedList!)
list.forEach(s -> ...);
for (ListIterator<String> it = list.listIterator(); it.hasNext(); ) {
    int idx = it.nextIndex();
    String s = it.next();
    if (s.isEmpty()) it.remove();                        // safe removal while iterating
    else it.set(s.strip());                              // replace in place
}
```

`ListIterator` also goes **backwards** (`hasPrevious`, `previous`) and can `add`. Never modify the list during a for-each loop except through the iterator: [12_iterators-and-fail-fast-behavior.md](./12_iterators-and-fail-fast-behavior.md).

## `subList`: a **view**, not a copy

```java
List<Integer> nums = new ArrayList<>(List.of(0, 1, 2, 3, 4, 5));
List<Integer> mid = nums.subList(2, 5);       // [2, 3, 4]  (from inclusive, to EXCLUSIVE)

mid.set(0, 99);                               // writes through: nums is now [0, 1, 99, 3, 4, 5]
mid.clear();                                  // removes those elements from nums: [0, 1, 5]

nums.subList(0, 3).clear();                   // idiom: remove a range
List<Integer> copy = new ArrayList<>(nums.subList(1, 3));   // independent copy when you need one
```

**Warning:** structurally modifying the original list (add/remove) after creating a sublist makes the sublist invalid (`ConcurrentModificationException` on next use).

## Sorting and searching

```java
list.sort(Comparator.comparing(String::length).thenComparing(Comparator.naturalOrder()));
Collections.sort(list);                                  // natural order (older style)
Collections.reverse(list);  Collections.shuffle(list);   Collections.swap(list, 0, 1);

Collections.sort(list);
int idx = Collections.binarySearch(list, "b");           // list MUST be sorted; negative = not found: -(insertion point) - 1
```

Sorting is **stable** (equal elements keep their relative order). Details: [05_comparable-and-comparator.md](./05_comparable-and-comparator.md), [14_collections-and-arrays-utilities.md](./14_collections-and-arrays-utilities.md).

## Converting

```java
String[] arr = list.toArray(new String[0]);              // List → array (or list.toArray(String[]::new))
List<String> view = Arrays.asList(arr);                  // array → List (fixed-size view)
List<String> copy = new ArrayList<>(Arrays.asList(arr)); // array → modifiable list
List<Integer> boxed = IntStream.of(1, 2, 3).boxed().toList();
int[] prims = list.stream().mapToInt(Integer::intValue).toArray();    // List<Integer> → int[]
```

### `Arrays.asList`: write-through, fixed size

```java
String[] arr = {"a", "b", "c"};
List<String> view = Arrays.asList(arr);
view.set(0, "z");              // OK, and arr[0] becomes "z" too
view.add("d");                 // UnsupportedOperationException: size is fixed
view.remove(0);                // UnsupportedOperationException

int[] prims = {1, 2, 3};
Arrays.asList(prims).size();   // 1 (a List<int[]>), not 3
```

## Common implementations

| Implementation | Backing | Best for | Notes |
|----------------|---------|----------|-------|
| [`ArrayList`](./06_arraylist.md) | Resizable array | **Default**: random access, iteration, append | Insert/remove in the middle is O(n) |
| [`LinkedList`](./07_linkedlist.md) | Doubly linked nodes | Rare: queue/deque use, iterator-based middle edits | `get(i)` is O(n) |
| `List.of(...)` / `List.copyOf` | Immutable arrays | Constants, read-only data | No `null`, `add` throws |
| `Arrays.asList(...)` | The original array | Quick view or wrapper | Fixed size |
| `CopyOnWriteArrayList` | Copy on every write | Many reads, rare writes, concurrent ([14-concurrency](../14-concurrency/08_concurrent-collections.md)) | Writes are expensive |
| `Vector`, `Stack` | Synchronized array | **Legacy: avoid** | |

When in doubt: **`ArrayList`**.

## Duplicates and uniqueness

```java
Set<String> unique = new LinkedHashSet<>(list);          // remove duplicates, keep first-seen order
List<String> deduped = new ArrayList<>(unique);
List<String> distinct = list.stream().distinct().toList();
```

## `List` of what? Common element-type pitfalls

| Situation | Note |
|-----------|------|
| `List<int>` | Not allowed: use `List<Integer>` (boxing cost) or `int[]`/`IntStream` |
| `List<int[]>` | Fine, but equality and printing use identity ([arrays](../01-fundamentals/07_arrays.md)) |
| Mutable element types | Changing an element object after adding it changes the list's content; harmful if you also use it as a key or sort key |
| `List<Object>` | Loses type safety; prefer a proper type |
| `List<List<Integer>>` | Fine; for a fixed 2-D grid use arrays |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `list.remove(i)` on `List<Integer>` intending a value | Removes by index | `remove(Integer.valueOf(x))` |
| `List.of(...)` then `add`/`set`/`remove` | `UnsupportedOperationException` | `new ArrayList<>(List.of(...))` |
| `Arrays.asList(1, 2).add(3)` | `UnsupportedOperationException` | Wrap in `new ArrayList<>(...)` |
| Removing inside a for-each loop | `ConcurrentModificationException` | `removeIf` or `Iterator.remove()` |
| `get(i)` in a loop over a `LinkedList` | O(n²) | for-each or use `ArrayList` |
| Passing `null` into `List.of` | `NullPointerException` | Filter first or use `ArrayList` |
| `subList` kept after modifying the parent | `ConcurrentModificationException` | Copy the sublist, or do not modify the parent |
| `binarySearch` on an unsorted list | Garbage result | Sort first |
| `list == other` / assuming `equals` on arrays inside lists | Identity comparison | `equals` / `Arrays.equals` |
| Using a `List` for membership tests in a hot path | `contains` is O(n) | Use a `HashSet` |
| Declaring `ArrayList<String>` instead of `List<String>` | Inflexible | Declare by interface |

## Key takeaways

- `List` = ordered, indexed, duplicates allowed; two lists are equal if their elements match in order
- Default to `ArrayList`; use `List.of` / `List.copyOf` for immutable data
- `remove(int)` vs `remove(Object)` is a classic trap for `List<Integer>`
- `subList` and `Arrays.asList` are **views**; `List.of` is immutable
- Modify during iteration only through the iterator, `removeIf` or `replaceAll`

**Next:** [Set](./02_set.md)
