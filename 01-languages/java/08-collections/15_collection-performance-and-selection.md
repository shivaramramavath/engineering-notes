# Collection Performance and Selection

This is the **decision file** for the folder: one Big-O table, a flowchart for choosing a collection, and the common performance traps. Use it as a reference once you have read the individual files.

## How to read Big-O here

| Notation | Meaning | Growth for n = 1,000,000 |
|----------|---------|---------------------------|
| O(1) | Constant: independent of size | ~1 step |
| O(log n) | Halving the problem each step | ~20 steps |
| O(n) | Linear: touches every element | 1,000,000 steps |
| O(n log n) | Sorting | ~20,000,000 steps |
| O(n²) | Nested loops | 10¹² steps: unusable |

*Amortized* O(1) means "O(1) on average over many operations; an occasional operation costs more" (for example, `ArrayList` resizing). More: [27-dsa/00_complexity-analysis.md](../27-dsa/00_complexity-analysis.md).

## The master table

| Operation | `ArrayList` | `LinkedList` | `ArrayDeque` | `HashSet` / `HashMap` | `LinkedHashSet` / `LinkedHashMap` | `TreeSet` / `TreeMap` | `PriorityQueue` |
|-----------|:-----------:|:------------:|:------------:|:---------------------:|:---------------------------------:|:---------------------:|:---------------:|
| **Get by index** `get(i)` | **O(1)** | O(n) | n/a | n/a | n/a | n/a | n/a |
| **Add at end** | O(1)\* | O(1) | O(1)\* | n/a | n/a | n/a | n/a |
| **Add at front** | O(n) | O(1) | O(1)\* | n/a | n/a | n/a | n/a |
| **Add in the middle** | O(n) | O(n)† | n/a | n/a | n/a | n/a | n/a |
| **Remove by index / from front** | O(n) | O(n)‡ | O(1) | n/a | n/a | n/a | n/a |
| **Insert / `put` / `add(e)`** (set/map) | n/a | n/a | n/a | **O(1)**\* | O(1)\* | **O(log n)** | O(log n) |
| **`contains` / `get(key)`** | O(n) | O(n) | O(n) | **O(1)** | O(1) | **O(log n)** | O(n) |
| **Remove by value / key** | O(n) | O(n) | O(n) | **O(1)** | O(1) | **O(log n)** | O(n) |
| **Min / max (first / last)** | O(n) | O(1) at ends | O(1) at ends | O(n) | O(1) at ends (21+) | **O(log n)** | **O(1)** peek, O(log n) poll |
| **Floor / ceiling / range** | n/a | n/a | n/a | n/a | n/a | **O(log n)** | n/a |
| **Iterate all** | O(n) fast | O(n) slower | O(n) fast | O(capacity + n) | O(n) | O(n) sorted | O(n) heap order |
| **Order** | Insertion | Insertion | Insertion | **None** | Insertion / access | **Sorted** | Heap (head = min) |
| **Memory / element** | Lowest | Highest | Low | Medium | Medium + | Medium + | Low |

\* amortized (occasional O(n) resize)  †  O(n) to find the position, O(1) to link  ‡ O(1) at the ends, O(n) to find an index

Implementation notes: [06 ArrayList](./06_arraylist.md), [07 LinkedList](./07_linkedlist.md), [08 hash](./08_hashmap-and-hashset.md), [09 tree](./09_treemap-and-treeset.md), [10 heap](./10_priorityqueue.md), [04 queues](./04_queue-and-deque.md).

### Special-purpose rows

| Collection | `get`/`contains` | `add` | Notes |
|------------|:----------------:|:-----:|-------|
| `EnumMap` / `EnumSet` | O(1) | O(1) | Array/bit-vector backed; enum keys only ([11](./11_specialized-collections.md)) |
| `ConcurrentHashMap` | O(1) | O(1) | Thread-safe; no nulls |
| `CopyOnWriteArrayList` | O(1) | **O(n)** (copies) | Many reads, rare writes |
| `ConcurrentSkipListMap` | O(log n) | O(log n) | Sorted, concurrent |
| `BitSet` | O(1) | O(1) | One bit per int |
| `List.of` / `Set.of` / `Map.of` | O(1)–O(n)* | n/a | Immutable; *`List.contains` is O(n) |
| `Collections.unmodifiableX` | Same as wrapped | n/a | View |

## Decision flowchart

```
What do you need to store?
│
├─ Key → value associations ─► MAP
│     ├─ keys are enums ───────────────────► EnumMap
│     ├─ sorted keys / range / floor-ceiling ► TreeMap
│     ├─ predictable order or LRU ───────────► LinkedHashMap
│     ├─ used from several threads ──────────► ConcurrentHashMap
│     └─ otherwise ──────────────────────────► HashMap
│
├─ Unique elements ─► SET
│     ├─ enum constants ─────────────────────► EnumSet
│     ├─ sorted / navigable ─────────────────► TreeSet
│     ├─ keep insertion order ───────────────► LinkedHashSet
│     ├─ small dense non-negative ints ──────► BitSet
│     └─ otherwise ──────────────────────────► HashSet
│
├─ Process in some order ─► QUEUE / DEQUE
│     ├─ FIFO or LIFO (stack) ───────────────► ArrayDeque
│     ├─ best/smallest element first ────────► PriorityQueue
│     └─ hand work to other threads ─────────► BlockingQueue (LinkedBlockingQueue ...)
│
└─ A sequence with duplicates ─► LIST
      ├─ read-only constant ─────────────────► List.of(...)
      ├─ mostly reads, rare writes, threads ─► CopyOnWriteArrayList
      └─ otherwise ──────────────────────────► ArrayList
```

## Selection by requirement

| Requirement | Choice | Why |
|-------------|--------|-----|
| Fast random access by index | `ArrayList` | O(1) |
| Fast "is x in here?" | `HashSet` / `HashMap` | O(1) |
| Remove duplicates, preserve order | `LinkedHashSet` | Unique + insertion order |
| Always iterate sorted | `TreeSet` / `TreeMap` | Maintains order |
| "Nearest value", ranges, thresholds | `TreeMap.floorEntry` etc. | Navigation methods |
| Repeatedly take the smallest/largest | `PriorityQueue` | Heap: O(log n) per `poll` |
| Queue / stack | `ArrayDeque` | Fastest, compact |
| Count occurrences | `HashMap<K, Integer>` + `merge`, or `groupingBy(counting())` | O(1) update |
| Group items by key | `HashMap<K, List<V>>` + `computeIfAbsent` | Standard idiom |
| Cache with eviction | `LinkedHashMap` (small) / Caffeine | LRU |
| Thread-safe map | `ConcurrentHashMap` | Atomic operations |
| Memory-sensitive numeric data | Arrays / primitive collections | Avoid boxing |
| Data that must never change | `List.of`, `Set.of`, `Map.of`, `copyOf` | Immutable |

**Rule of thumb:** start with `ArrayList`, `HashMap`, `HashSet`, `ArrayDeque`; change only for a **reason** (order, sorting, ranges, concurrency, memory) or a **measurement**.

## Constants hide behind Big-O

Big-O describes growth, not speed at a given size.

| Reality | Effect |
|---------|--------|
| **CPU caches**: arrays are contiguous; linked structures chase pointers | `ArrayList` beats `LinkedList` even where theory says otherwise |
| **Small n**: for under ~50 elements, a linear scan of an `ArrayList` can beat a `HashSet` lookup | Do not over-engineer small collections |
| **Hashing cost**: `HashMap` pays for `hashCode` + `equals`; long strings or complex keys cost more | A bad `hashCode` degrades everything |
| **Boxing**: `ArrayList<Integer>`/`HashMap<Integer, Integer>` allocate objects and chase references | Arrays or primitive collections are many times faster for numeric workloads |
| **Resizing**: growth copies everything | Pre-size when counts are known |
| **GC pressure**: per-entry nodes (`HashMap`, `LinkedList`, `TreeMap`) | More allocation, more GC work |

**Measure before optimizing.** Use a real profiler or JMH microbenchmarks, not intuition ([20-performance/00_profiling.md](../20-performance/00_profiling.md), [01_benchmarking-with-jmh.md](../20-performance/01_benchmarking-with-jmh.md)). Naive `System.nanoTime()` loops are misleading because of JIT warm-up.

## Memory footprint (rough, 64-bit JVM, compressed references)

| Structure | Approximate overhead per element (excluding the element object itself) |
|-----------|-----------------------------------------------------------------------|
| `int[]` | 4 bytes |
| `Integer[]` / `ArrayList<Integer>` | 4 bytes (reference) + ~16 bytes (`Integer` object) |
| `ArrayList<E>` | ~4 bytes reference (+ up to ~33% spare capacity) |
| `ArrayDeque<E>` | ~4-8 bytes |
| `HashSet<E>` | ~40-60 bytes (`Node` + table slots) |
| `HashMap<K, V>` | ~40-60 bytes per entry (+ key and value objects) |
| `LinkedHashMap` | ~50-70 bytes (extra before/after links) |
| `TreeMap<K, V>` | ~40-50 bytes (`Entry` with 5 fields) |
| `LinkedList<E>` | ~24-32 bytes (node: item, next, prev) |

Estimates only; use a heap analyzer (JOL, VisualVM, JFR) for real numbers ([20-performance/02_memory-optimization.md](../20-performance/02_memory-optimization.md)).

## Common performance traps

| Trap | Why it hurts | Better |
|------|--------------|--------|
| `list.contains(x)` inside a loop | O(n²) overall | Put the data in a `HashSet` first |
| `for (i) linkedList.get(i)` | O(n²) | for-each, or `ArrayList` |
| `ArrayList.remove(0)` / `add(0, x)` as a queue | O(n) each | `ArrayDeque` |
| `String +=` or `list.toString()` in loops | O(n²) copies | `StringBuilder` ([strings](../03-strings-and-text/03_stringbuilder-and-stringbuffer.md)) |
| Removing elements one by one with `remove(Object)` | O(n²) | `removeIf` (single pass) or build a new list |
| `new ArrayList<>()` growing to millions | Repeated resizes | Pre-size: `new ArrayList<>(n)` |
| `new HashMap<>(n)` expecting room for `n` | Resizes at 0.75 × capacity | `HashMap.newHashMap(n)` (Java 19+) or `n / 0.75 + 1` |
| Large boxed collections (`List<Integer>` with millions) | Memory and GC | `int[]`, `IntStream`, primitive collections |
| Bad or missing `hashCode` | Collisions → O(n) / O(log n) | Hash all identity fields |
| Mutable keys in hash collections | Lost entries | Immutable keys |
| `TreeMap` when only lookups are needed | Slower than `HashMap` | `HashMap` |
| `LinkedList` "for fast insertion" | Slower in practice | `ArrayList`/`ArrayDeque` |
| `Collections.synchronizedMap` for high contention | Single lock bottleneck | `ConcurrentHashMap` |
| `CopyOnWriteArrayList` with frequent writes | Each write copies everything | `ConcurrentLinkedQueue`, locks, or another design |
| Sorting repeatedly | O(n log n) each time | Keep a `TreeSet`/`PriorityQueue`, or sort once |
| `stream().sorted()` just to find min/max | O(n log n) | `Collections.max`, `stream().max()` (O(n)) |
| Computing `size()` of a `TreeMap` sub-view in loops | O(n) per call | Count once |
| Iterating a huge, sparse `HashMap` | Visits all buckets | Right-size it, or `LinkedHashMap` |
| Creating many tiny collections per object | Overhead adds up | Lazy creation, `List.of()`/`emptyList()` |

## Concurrency selection

| Need | Collection |
|------|------------|
| Shared map | `ConcurrentHashMap` (use `merge`/`compute` for atomic updates) |
| Shared sorted map/set | `ConcurrentSkipListMap` / `Set` |
| Shared queue, no blocking | `ConcurrentLinkedQueue` / `Deque` |
| Producer-consumer with back-pressure | `ArrayBlockingQueue`, `LinkedBlockingQueue` |
| Read-mostly list (listeners) | `CopyOnWriteArrayList` |
| Single thread only | The ordinary collections: no synchronization cost |

Details and pitfalls: [14-concurrency/08_concurrent-collections.md](../14-concurrency/08_concurrent-collections.md).

## Quick interview answers

| Question | Short answer |
|----------|--------------|
| `ArrayList` vs `LinkedList`? | Array: O(1) index, compact, fast iteration; linked: O(1) ends, O(n) index, heavy. Use `ArrayList` (or `ArrayDeque` for queues) |
| `HashMap` vs `TreeMap`? | Hash: O(1), unordered; tree: O(log n), sorted with navigation |
| `HashSet` vs `TreeSet` vs `LinkedHashSet`? | Unordered O(1); sorted O(log n); insertion order O(1) |
| `HashMap` vs `Hashtable`? | `HashMap`: unsynchronized, allows one `null` key; `Hashtable`: legacy, synchronized, no nulls |
| `HashMap` vs `ConcurrentHashMap`? | The latter is thread-safe with fine-grained locking/CAS, no nulls |
| How does `HashMap` work? | `hashCode` → spread → bucket (`hash & (n-1)`) → `equals`; resize at 0.75; trees at 8+ collisions |
| What if `equals` is overridden without `hashCode`? | Equal objects land in different buckets: lookups fail |
| Fail-fast vs fail-safe iterators? | Fail-fast throws `ConcurrentModificationException` on structural change; concurrent collections have weakly consistent iterators |
| `Comparable` vs `Comparator`? | Natural order inside the class vs external, multiple orders |
| `List.of` vs `Collections.unmodifiableList`? | Immutable snapshot vs read-only live view |
| `Collection` vs `Collections`? | Interface vs utility class |
| Why `ArrayDeque` over `Stack`? | `Stack` is a synchronized `Vector` with a leaky API |
| Complexity of `PriorityQueue` operations? | `peek` O(1), `offer`/`poll` O(log n), `remove(Object)`/`contains` O(n) |
| `ArrayList` growth? | About 1.5× when full; amortized O(1) append |

More: [28-interview/03_collections.md](../28-interview/03_collections.md), cheat sheet [29-cheatsheets/02_collections.md](../29-cheatsheets/02_collections.md).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Choosing by habit instead of access pattern | Slow code at scale | Use the table and flowchart |
| Optimizing collection choice without measuring | Wasted effort, sometimes slower code | Profile and benchmark first |
| Using `List` for lookup-heavy code | O(n) searches | `Set`/`Map` |
| Using hash collections without proper `equals`/`hashCode` | Wrong results | Implement correctly ([04-oop/14](../04-oop/14_equals-and-hashcode.md)) |
| Ignoring memory for large collections | `OutOfMemoryError`, GC thrash | Primitive structures, compaction |
| Assuming O(1) means "fast" regardless of constants | Surprises at small n | Measure |
| Using concurrent collections where one thread suffices | Needless overhead | Plain collections |
| Forgetting that iteration order of `HashMap`/`Set.of` is unspecified | Flaky tests | Sorted or linked variants |

## Key takeaways

- Defaults: `ArrayList`, `HashMap`, `HashSet`, `ArrayDeque`; switch only for order, sorting/ranges, priority, concurrency or memory
- Index access: array-backed; membership/lookup: hash-based; sorted/range: tree-based; "next best": heap
- Big-O guides growth, but cache locality, boxing, resizing and GC often decide real speed: **measure**
- The classic traps: `contains` on lists in loops, `LinkedList.get(i)`, `ArrayList` as a queue, boxed numbers, mutable keys
- Declare by interface so the choice stays easy to change

**Next:** [09-functional-java](../09-functional-java/README.md)