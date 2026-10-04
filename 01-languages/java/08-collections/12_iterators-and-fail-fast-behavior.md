# Iterators and Fail-Fast Behavior

An **iterator** is an object that walks through a collection one element at a time. Every for-each loop over a collection uses one behind the scenes. Iterators are also how you **safely remove** elements during traversal, and they are the source of the infamous `ConcurrentModificationException`.

```java
List<String> names = List.of("Ada", "Linus", "Grace");

for (String n : names) { System.out.println(n); }          // for-each

Iterator<String> it = names.iterator();                     // what the compiler turns it into
while (it.hasNext()) {
    String n = it.next();
    System.out.println(n);
}
```

## `Iterable` and `Iterator`

```java
public interface Iterable<T> { Iterator<T> iterator(); /* + forEach, spliterator */ }

public interface Iterator<E> {
    boolean hasNext();                  // is there another element?
    E next();                           // return it and advance (NoSuchElementException if none)
    default void remove() { ... }       // remove the element last returned by next() (optional operation)
    default void forEachRemaining(Consumer<? super E> action) { ... }
}
```

| Interface | Role |
|-----------|------|
| `Iterable<T>` | "I can produce iterators": anything usable in a for-each loop (all `Collection`s, but not `Map`) |
| `Iterator<E>` | A **single-use cursor** over one traversal |

An iterator sits **between** elements:

```
        ┌───┬───┬───┬───┐
        │ A │ B │ C │ D │
        └───┴───┴───┴───┘
       ▲   ▲   ▲   ▲   ▲
   start   after next() calls ...
```

`next()` returns the element after the cursor and moves past it. `hasNext()` asks whether anything remains. Each call to `iterator()` produces a **fresh** cursor.

## Safe removal: `Iterator.remove()`

```java
List<Integer> nums = new ArrayList<>(List.of(1, 2, 3, 4, 5, 6));

Iterator<Integer> it = nums.iterator();
while (it.hasNext()) {
    if (it.next() % 2 == 0) it.remove();      // removes the element returned by the last next()
}
// nums: [1, 3, 5]
```

Rules for `remove()`:
- Call it **at most once per `next()`**, and only **after** a `next()` (else `IllegalStateException`)
- It is an **optional** operation: iterators over immutable collections throw `UnsupportedOperationException`

## `ConcurrentModificationException`

```java
List<Integer> nums = new ArrayList<>(List.of(1, 2, 3, 4));
for (Integer n : nums) {
    if (n == 2) nums.remove(n);                // modifies the list behind the iterator's back
}                                              // → java.util.ConcurrentModificationException
```

Despite the name, this usually happens in **single-threaded** code: you changed the collection **structurally** (add/remove/clear) while an iterator was active, **not through that iterator**.

### How fail-fast works

```
ArrayList has a counter:  modCount   (incremented by every structural modification)

Iterator creation:   expectedModCount = list.modCount
Each next():         if (list.modCount != expectedModCount) throw new ConcurrentModificationException()
Iterator.remove():   removes AND updates expectedModCount to match
```

- **Structural** modifications change the size (`add`, `remove`, `clear`, `addAll`, ...). Replacing a value (`list.set(i, x)`, `map.put(existingKey, v)`) is **not** structural
- The check is **best effort**: it is meant to catch bugs, not to guarantee behavior. Do not catch `ConcurrentModificationException` as a feature; fix the code
- A famous quirk: removing the **second-to-last** element in a for-each over an `ArrayList` does *not* throw (the loop simply ends because `hasNext()` returns false), which makes the bug intermittent

### Fail-fast vs weakly consistent iterators

| | Fail-fast | Weakly consistent / snapshot |
|---|-----------|------------------------------|
| Collections | `ArrayList`, `LinkedList`, `HashMap`, `HashSet`, `TreeMap`, `ArrayDeque`, `PriorityQueue`, `Vector` | `ConcurrentHashMap`, `ConcurrentLinkedQueue`, `ConcurrentSkipListMap`, `CopyOnWriteArrayList` |
| On concurrent change | Throws `ConcurrentModificationException` (best effort) | Never throws; may or may not reflect changes made after creation |
| Use for | Single-threaded code; bug detection | Concurrent code ([14-concurrency/08_concurrent-collections.md](../14-concurrency/08_concurrent-collections.md)) |

`CopyOnWriteArrayList` iterators work on a **snapshot** taken at creation; they never see later changes (and do not support `remove()`).

## Three correct ways to remove while looping

```java
List<Integer> nums = new ArrayList<>(List.of(1, 2, 3, 4, 5, 6));

// 1. removeIf (best: concise, single pass, O(n))
nums.removeIf(n -> n % 2 == 0);

// 2. Iterator.remove()
for (Iterator<Integer> it = nums.iterator(); it.hasNext(); ) {
    if (it.next() % 2 == 0) it.remove();
}

// 3. Collect what to keep, then replace (or filter with streams)
List<Integer> kept = new ArrayList<>();
for (int n : nums) if (n % 2 != 0) kept.add(n);
nums = kept;
// or: nums = nums.stream().filter(n -> n % 2 != 0).collect(Collectors.toCollection(ArrayList::new));
```

By index (also correct, but easy to get wrong):

```java
for (int i = nums.size() - 1; i >= 0; i--) {          // iterate BACKWARDS so removals do not shift unvisited elements
    if (nums.get(i) % 2 == 0) nums.remove(i);
}
```

Going forward with `i++` and removing skips the element that slides into position `i`.

### Maps

```java
Map<String, Integer> map = new HashMap<>(Map.of("a", 1, "b", -2, "c", 3));

map.entrySet().removeIf(e -> e.getValue() < 0);        // preferred
map.keySet().removeIf(k -> k.startsWith("tmp"));
map.values().removeIf(v -> v == null);

for (Iterator<Map.Entry<String, Integer>> it = map.entrySet().iterator(); it.hasNext(); ) {
    if (it.next().getValue() < 0) it.remove();
}

for (String k : map.keySet()) {
    map.remove(k);                                      // ConcurrentModificationException
}
for (var e : map.entrySet()) e.setValue(e.getValue() * 2);     // fine: not structural
```

### Adding while iterating

Collect additions separately and add after the loop, or use `ListIterator.add`:

```java
for (ListIterator<String> it = list.listIterator(); it.hasNext(); ) {
    String s = it.next();
    if (s.equals("x")) it.add("y");                     // inserts after the current element, safely
}
```

## `ListIterator`

A richer iterator for `List`s:

| Method | Purpose |
|--------|---------|
| `hasPrevious()`, `previous()` | Walk **backwards** |
| `nextIndex()`, `previousIndex()` | Current position |
| `set(e)` | Replace the element last returned |
| `add(e)` | Insert at the cursor |
| `remove()` | Remove the last returned element |

```java
ListIterator<String> it = list.listIterator(list.size());      // start at the end
while (it.hasPrevious()) System.out.println(it.previous());     // reverse traversal
```

(Java 21: `list.reversed()` is simpler: [hierarchy](./00_collection-hierarchy.md#sequenced-collections-java-21).)

## `forEach`, `removeIf`, `replaceAll`

Modern bulk methods handle iteration and modification **correctly and efficiently** in one call:

```java
list.forEach(System.out::println);
list.removeIf(String::isBlank);
list.replaceAll(String::trim);
map.forEach((k, v) -> ...);
map.replaceAll((k, v) -> v + 1);
map.merge(k, 1, Integer::sum);
```

`ArrayList.removeIf` runs in a single pass (O(n)), unlike repeated `remove(Object)` (O(n²)).

## Writing your own `Iterable`

Implement `Iterable<T>` and a nested iterator to make any structure work in a for-each loop:

```java
public class Range implements Iterable<Integer> {
    private final int start, end;                          // [start, end)
    public Range(int start, int end) { this.start = start; this.end = end; }

    @Override
    public Iterator<Integer> iterator() {
        return new Iterator<>() {
            private int next = start;
            @Override public boolean hasNext() { return next < end; }
            @Override public Integer next() {
                if (!hasNext()) throw new NoSuchElementException();
                return next++;
            }
        };
    }
}

for (int i : new Range(0, 3)) System.out.println(i);       // 0 1 2
new Range(0, 5).forEach(System.out::println);              // inherits forEach
```

An **inner or anonymous class** is the natural way, since it reads the outer structure ([04-oop/12_nested-and-inner-classes.md](../04-oop/12_nested-and-inner-classes.md)). For lazy or infinite sequences consider streams (`Stream.iterate`, `Stream.generate`) or generators ([09-functional-java](../09-functional-java/README.md)).

## Iterators and streams

```java
Iterator<String> it = list.iterator();
Stream<String> s = StreamSupport.stream(Spliterators.spliteratorUnknownSize(it, 0), false);
Iterable<String> iterable = stream::iterator;               // one-shot Iterable from a stream
```

A `Stream` is single-use; an `Iterable` can produce many iterators. Do not use a stream-backed `Iterable` twice.

## Performance and iteration order

| Collection | Iteration order | Notes |
|------------|-----------------|-------|
| `ArrayList`, `LinkedList` | Index order | `ArrayList` is far faster (contiguous memory) |
| `HashSet`, `HashMap` | Bucket order, unspecified | Cost O(capacity + size) |
| `LinkedHashSet`, `LinkedHashMap` | Insertion (or access) order | Cost O(size) |
| `TreeSet`, `TreeMap` | Sorted | In-order tree walk |
| `PriorityQueue` | Heap order | Not sorted |
| `ArrayDeque` | Head to tail | |
| `EnumSet`, `EnumMap` | Enum declaration order | |

for-each over an array or `ArrayList` is as fast as an indexed loop in practice; the iterator allocation is typically optimized away.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Removing/adding in a for-each loop | `ConcurrentModificationException` | `removeIf`, `Iterator.remove()`, or collect then modify |
| Calling `list.remove(x)` inside the loop even when "it works" sometimes | Intermittent failures (second-to-last quirk) | Use the iterator |
| Calling `it.remove()` twice, or before `next()` | `IllegalStateException` | One `remove()` per `next()` |
| `it.next()` without `hasNext()` | `NoSuchElementException` | Check first |
| Calling `next()` twice per loop iteration | Skipped elements, exceptions at the end | Store it: `String s = it.next();` |
| Removing by ascending index in a loop | Skipped elements | Iterate backwards, or `removeIf` |
| Catching `ConcurrentModificationException` to "handle" it | Hides the bug | Fix the modification |
| Using `Iterator.remove()` on immutable collections | `UnsupportedOperationException` | Copy to a mutable collection |
| Sharing a non-thread-safe collection and iterating concurrently | CME or wrong results | Concurrent collections or snapshot copies |
| Reusing a stream-backed `Iterable` | `IllegalStateException: stream has already been operated upon` | Recreate the stream |
| Iterating a `Map` directly | Not `Iterable` | `entrySet()`, `keySet()`, `values()` |

## Key takeaways

- `Iterable` produces `Iterator`s; for-each is sugar for `hasNext()`/`next()`
- Structural modification during iteration, other than through the iterator, triggers `ConcurrentModificationException` (fail-fast, best effort)
- Remove safely with `removeIf`, `Iterator.remove()`, or by building a new collection
- `ListIterator` adds backward traversal, `set` and `add`
- Concurrent collections use weakly consistent or snapshot iterators and never throw CME

**Next:** [Immutable and Unmodifiable Collections](./13_immutable-and-unmodifiable-collections.md)
