# ArrayList

`ArrayList<E>` is a **resizable array** implementing `List`. It is the default list in Java: fast random access, compact memory, cache-friendly iteration. Understanding how it works explains its cost model and its few weaknesses.

```java
List<String> list = new ArrayList<>();
list.add("a");
list.add("b");
list.get(1);              // "b": direct array index, O(1)
```

## How it works

```
ArrayList
 ├── Object[] elementData   ──►  [ "a" | "b" | "c" | null | null | null | null | null | null | null ]
 │                                  0     1     2    └──────────── spare capacity ──────────────┘
 ├── int size = 3           (number of real elements)
 └── int modCount           (structural modification counter: for fail-fast iterators)
```

| Concept | Meaning |
|---------|---------|
| **Size** | Number of elements stored (`size()`) |
| **Capacity** | Length of the backing array (not exposed directly) |
| `elementData[size..capacity)` | Unused slots, kept `null` |
| **Random access** | `get(i)` is `elementData[i]` after a bounds check: O(1) |

### Growth

When the array is full, `add` allocates a **bigger** array (about **1.5×**: `newCapacity = oldCapacity + (oldCapacity >> 1)`) and copies the old elements over.

```
capacity 10 ──full──► 15 ──full──► 22 ──full──► 33 ...
```

- `new ArrayList<>()` starts with an **empty shared array** and allocates capacity **10** on the first `add`
- `new ArrayList<>(n)` allocates `n` slots immediately
- `new ArrayList<>(collection)` copies the collection exactly (capacity = its size)
- The array **never shrinks automatically**; `trimToSize()` does it on demand

A simplified model of the idea:

```java
class MyList<E> {
    private Object[] data = new Object[10];
    private int size;

    void add(E e) {
        if (size == data.length) data = Arrays.copyOf(data, size + (size >> 1) + 1);   // grow ~1.5x
        data[size++] = e;
    }
    @SuppressWarnings("unchecked") E get(int i) {
        Objects.checkIndex(i, size);
        return (E) data[i];
    }
    E remove(int i) {
        Objects.checkIndex(i, size);
        @SuppressWarnings("unchecked") E old = (E) data[i];
        System.arraycopy(data, i + 1, data, i, size - i - 1);    // shift left
        data[--size] = null;                                     // let the GC reclaim the object
        return old;
    }
}
```

## Cost model

| Operation | Cost | Why |
|-----------|------|-----|
| `get(i)`, `set(i, e)` | **O(1)** | Array index |
| `add(e)` (append) | **O(1) amortized** | Usually a write; occasionally an O(n) copy that is spread over many appends |
| `add(i, e)` (insert) | O(n) | Shift everything after `i` right (`System.arraycopy`) |
| `remove(i)` | O(n) | Shift everything after `i` left |
| `remove(Object)`, `indexOf`, `contains` | O(n) | Linear scan with `equals` |
| `removeFirst()`/`add(0, e)` | O(n) | Shifts all elements |
| `size()`, `isEmpty()` | O(1) | Field |
| `clear()` | O(n) | Nulls out every slot |
| Iteration | O(n), very fast | Contiguous memory, cache-friendly |
| `removeIf`, `replaceAll`, `sort` | O(n) / O(n log n) | Single pass or merge sort |

The amortized argument: with 1.5× growth, each element is copied only a small constant number of times on average, so a sequence of n appends costs O(n) total.

### `System.arraycopy` is fast

Insertion and removal in the middle are O(n) but use a native memory move, so for lists up to tens of thousands of elements they are usually quicker than the pointer chasing of a `LinkedList` ([07](./07_linkedlist.md)).

## Capacity tuning

```java
List<String> list = new ArrayList<>(10_000);     // you know roughly how many: avoids repeated regrowth
list.ensureCapacity(50_000);                      // ensure before a big bulk add
((ArrayList<String>) list).trimToSize();          // release unused slots (rarely needed)
```

Pre-sizing matters only for large lists; correctness never depends on it. `new ArrayList<>(0)` or `List.of()` for small, rarely used fields avoids allocating the default array.

## Removing elements

```java
List<Integer> nums = new ArrayList<>(List.of(1, 2, 3, 4, 5, 6));

nums.removeIf(n -> n % 2 == 0);                  // single O(n) pass: BEST for conditional removal
```

Worse alternatives:

```java
for (int i = 0; i < nums.size(); i++) {
    if (nums.get(i) % 2 == 0) nums.remove(i);    // BUG: skips elements after a removal; and each remove is O(n) → O(n²)
}
for (Integer n : nums) {
    if (n % 2 == 0) nums.remove(n);              // ConcurrentModificationException
}
```

Correct manual approaches: iterate **backwards** by index, use `Iterator.remove()`, or build a new list ([12](./12_iterators-and-fail-fast-behavior.md)).

`ArrayList` nulls out removed slots, so it does not keep removed objects alive (no memory leak from the list itself).

## Fail-fast iteration

Every **structural** modification (`add`, `remove`, `clear`, ...) increments `modCount`. Iterators remember the value at creation; if it changes underneath them, they throw `ConcurrentModificationException`. `set(i, e)` is **not** structural. See [12_iterators-and-fail-fast-behavior.md](./12_iterators-and-fail-fast-behavior.md).

## Memory

| Aspect | Detail |
|--------|--------|
| Per-element overhead | One reference (4 or 8 bytes) in the array: very compact |
| Spare capacity | Up to ~33% wasted after growth |
| Boxed numbers | `ArrayList<Integer>` stores references to `Integer` objects (16+ bytes each beyond the reference); for millions of numbers use `int[]` or primitive streams |
| Large lists | One big contiguous array: a very large list needs a large contiguous heap block |

## Other properties

| Property | Detail |
|----------|--------|
| `RandomAccess` marker | Algorithms (`Collections.binarySearch`, `shuffle`) pick the index-based strategy |
| Thread-safety | **None**: use `CopyOnWriteArrayList`, a lock, or `Collections.synchronizedList` ([14-concurrency](../14-concurrency/08_concurrent-collections.md)) |
| `null` elements | Allowed (avoid) |
| Duplicates | Allowed |
| Serializable, Cloneable | Yes (`clone()` is shallow: [04-oop/15](../04-oop/15_clone-and-copying.md)) |
| `equals`/`hashCode` | Defined by elements, in order |
| `subList` | A view that depends on the parent's `modCount` |
| `toArray` | A copy |

## ArrayList vs array

| `ArrayList` | Array |
|-------------|-------|
| Grows and shrinks | Fixed size |
| Objects only (boxing for numbers) | Can hold primitives |
| Rich API (`add`, `remove`, `contains`, `sort`) | Minimal (`Arrays` helpers) |
| Slightly more overhead | Fastest, smallest |

Use arrays for fixed-size, primitive-heavy, performance-critical data ([01-fundamentals/07_arrays.md](../01-fundamentals/07_arrays.md)); `ArrayList` for the rest.

## ArrayList vs LinkedList

| | `ArrayList` | `LinkedList` |
|---|-------------|--------------|
| `get(i)` | O(1) | O(n) |
| Append | O(1) amortized | O(1) |
| Insert/remove at the front | O(n) | O(1) |
| Insert/remove in the middle | O(n) shift (fast native copy) | O(n) to find + O(1) to link |
| Memory | Compact | ~3-5× more per element |
| Iteration speed | Fast | Slower (cache misses) |

`ArrayList` wins in nearly every real benchmark; see [07_linkedlist.md](./07_linkedlist.md). For a **queue or stack**, use `ArrayDeque` ([04](./04_queue-and-deque.md)).

## Typical usage patterns

```java
// Build and return
List<Order> result = new ArrayList<>();
for (Order o : orders) if (o.isOpen()) result.add(o);
return result;

// Copy
List<String> copy = new ArrayList<>(original);

// Preallocate when the size is known
List<Result> results = new ArrayList<>(inputs.size());

// Defensive copy at an API boundary
return List.copyOf(internal);       // or Collections.unmodifiableList(internal)
```

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Removing in a for-each loop | `ConcurrentModificationException` | `removeIf`, `Iterator.remove()` |
| Removing by index inside an ascending index loop | Skipped elements | Iterate backwards or use `removeIf` |
| `list.remove(0)` repeatedly to use it as a queue | O(n²) | `ArrayDeque` |
| Frequent insertion at the front | O(n) each | `ArrayDeque` or another structure |
| `contains` in a loop over a large list | O(n²) | Put elements in a `HashSet` |
| `ArrayList<Integer>` for millions of numbers | High memory use, boxing churn | `int[]`, `IntStream`, or a primitive collection library |
| Forgetting that `new ArrayList<>(n)` creates an **empty** list of capacity `n` | `IndexOutOfBoundsException` on `set(0, x)` | Capacity is not size; use `Collections.nCopies` to fill |
| Sharing one `ArrayList` between threads | Lost or corrupt updates | Thread-safe alternatives |
| Holding a huge, mostly-empty list | Wasted memory | `trimToSize()` or a new list |
| Returning the internal list from a getter | Callers mutate your state | Copy or unmodifiable view |
| Using `Vector` "for thread safety" | Slow, still not safe for compound actions | Concurrent collections |

## Key takeaways

- `ArrayList` is a growable `Object[]`: O(1) indexed access and amortized O(1) append
- Insert/remove away from the end is O(n) (a fast native shift); `contains`/`indexOf` are O(n)
- Growth is ~1.5× when full; pre-size when you know the count
- Use `removeIf` for conditional removal; never remove inside a for-each
- It is the right default list; reach for `ArrayDeque`, `HashSet` or arrays when the access pattern calls for them

**Next:** [LinkedList](./07_linkedlist.md)
