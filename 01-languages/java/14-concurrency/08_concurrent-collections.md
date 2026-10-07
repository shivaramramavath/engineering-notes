# Concurrent Collections

The ordinary collections (`ArrayList`, `HashMap`, `ArrayDeque`) are **not thread-safe**. The `java.util.concurrent` package provides collections designed for concurrent use that are safer *and* far more scalable than wrapping a regular collection in one big lock.

They give you three things: **thread safety**, **atomic compound operations** (like `putIfAbsent`), and **iterators that don't throw `ConcurrentModificationException`**.

**Prerequisites:** [Thread Safety](02_thread-safety.md), [Map](../08-collections/03_map.md), [Queue and Deque](../08-collections/04_queue-and-deque.md), [Iterators and Fail-Fast Behavior](../08-collections/12_iterators-and-fail-fast-behavior.md).

---

## 1. Why not just `Collections.synchronizedX`?

```java
Map<String, Integer> m = Collections.synchronizedMap(new HashMap<>());
```

This wraps **every method in one lock**, so:

- Only one thread at a time can touch the map (poor scalability).
- **Compound actions are still racy**: `if (!m.containsKey(k)) m.put(k, v)` is two calls.
- **Iterating requires manual locking** (`synchronized (m) { for (...) }`), or you risk `ConcurrentModificationException`.

`Vector` and `Hashtable` are the legacy equivalents. Prefer the `java.util.concurrent` classes below.

---

## 2. `ConcurrentHashMap`

A hash map for many concurrent readers and writers. Reads are essentially lock-free, and writes lock only a small part of the table (per bin), so threads updating different keys rarely block each other.

```java
ConcurrentHashMap<String, Integer> counts = new ConcurrentHashMap<>();

counts.merge("apple", 1, Integer::sum);                  // atomic "increment or insert"
counts.putIfAbsent("pear", 0);                           // atomic check-then-put
counts.computeIfAbsent("kiwi", k -> loadFromDb(k));      // compute once per key, atomically
counts.compute("apple", (k, v) -> v == null ? 1 : v + 1);// atomic read-modify-write for one key
counts.replace("apple", 3, 4);                           // CAS-style: only if current value is 3
counts.remove("apple", 4);                               // remove only if the value matches
```

These methods are the point of the class: **use them instead of `get` followed by `put`**. Each is atomic per key.

### Rules and gotchas

- **No `null` keys or values.** `put(k, null)` throws `NullPointerException` (so `get(k) == null` unambiguously means "absent"). Use `Optional` or a sentinel value if you need "present but empty".
- **Keep `compute*`/`merge` functions short, quick, and side-effect free.** They run while the key's bin is locked. They must **not** modify the map (including recursively calling `computeIfAbsent` on the same map). Doing so can throw `IllegalStateException` ("Recursive update") or hang. Never do blocking I/O inside them.
- **`size()` and iteration are approximate under concurrent modification.** Use `mappingCount()` for a `long` count. Iterators are **weakly consistent**: they never throw `ConcurrentModificationException` and may or may not reflect updates made after the iterator was created.
- **Compound operations across keys aren't atomic.** Moving a value from key A to key B is two separate operations, and another thread can see the in-between state.
- A concurrent set: `Set<String> set = ConcurrentHashMap.newKeySet();`

### Bulk operations

```java
long parallelismThreshold = 1_000;   // use parallelism only when the map has more than this many elements
counts.forEach(parallelismThreshold, (k, v) -> process(k, v));
long total = counts.reduceValues(parallelismThreshold, Integer::sum);
String found = counts.search(parallelismThreshold, (k, v) -> v > 100 ? k : null);
```

Example: concurrent word count

```java
Map<String, Long> freq = new ConcurrentHashMap<>();
lines.parallelStream()
     .flatMap(l -> Arrays.stream(l.split("\\W+")))
     .forEach(w -> freq.merge(w, 1L, Long::sum));
```

(For streams, `Collectors.groupingBy` with a downstream `counting()` is usually cleaner: [Collectors](../09-functional-java/06_collectors.md).)

---

## 3. Copy-on-write collections

`CopyOnWriteArrayList` and `CopyOnWriteArraySet` make a **fresh copy of the underlying array on every modification**. Readers iterate over an immutable **snapshot**, so they never block and never see a `ConcurrentModificationException`.

```java
private final List<Listener> listeners = new CopyOnWriteArrayList<>();

void addListener(Listener l) { listeners.add(l); }                  // copies the array: O(n)
void fire(Event e) { for (Listener l : listeners) l.on(e); }        // safe even if a listener (un)registers itself
```

| Good for | Bad for |
|---|---|
| Read-mostly lists: **listeners**, observers, configuration lists | Frequent writes (each is O(n) with allocation) |
| Iteration that must tolerate concurrent modification | Large lists with frequent changes |

Iterators reflect the state **at creation time** and can't remove (`remove()` throws `UnsupportedOperationException`).

---

## 4. Queues

### Non-blocking: `ConcurrentLinkedQueue` / `ConcurrentLinkedDeque`

Lock-free, unbounded, thread-safe FIFO (and double-ended). `poll()` returns `null` when empty. `size()` is **O(n)** and approximate, so use `isEmpty()`. Good when consumers can poll without waiting.

### Blocking: `BlockingQueue`

A queue where producers can **wait for space** and consumers can **wait for items**. This is the standard way to hand work between threads.

| | Throws exception | Special value | Blocks | Times out |
|---|---|---|---|---|
| Insert | `add(e)` | `offer(e)` | `put(e)` | `offer(e, t, unit)` |
| Remove | `remove()` | `poll()` | `take()` | `poll(t, unit)` |
| Examine | `element()` | `peek()` | | |

| Implementation | Notes |
|---|---|
| `ArrayBlockingQueue` | **Bounded**, array-backed, optional fairness |
| `LinkedBlockingQueue` | Linked nodes, **optionally bounded**. Default capacity is `Integer.MAX_VALUE`, i.e. effectively unbounded |
| `PriorityBlockingQueue` | Unbounded, ordered by priority (iteration order is *not* sorted) |
| `DelayQueue` | Elements become available only after their delay expires (scheduling) |
| `SynchronousQueue` | **Zero capacity**: each `put` waits for a matching `take` (direct hand-off; used by cached thread pools) |
| `LinkedBlockingDeque`, `LinkedTransferQueue` | Double-ended / transfer semantics |

### Producer-consumer

```java
record Job(String payload) {}
private static final Job POISON = new Job(null);              // sentinel meaning "no more work"

BlockingQueue<Job> queue = new ArrayBlockingQueue<>(100);     // bounded → backpressure

Runnable producer = () -> {
    try {
        for (String s : inputs) queue.put(new Job(s));        // blocks if the queue is full
        queue.put(POISON);                                    // tell the consumer to stop
    } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
};

Runnable consumer = () -> {
    try {
        for (Job job = queue.take(); job != POISON; job = queue.take()) {   // blocks if empty
            process(job);
        }
    } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
};
```

**Bounded queues matter.** If producers are faster than consumers, an unbounded queue just grows until `OutOfMemoryError`. A bounded queue makes producers *wait*, which is **backpressure**. Thread pools have the same trap ([Executors](10_executors-and-thread-pools.md)).

With several consumers, send one poison pill per consumer, or have each consumer re-insert the pill before exiting.

---

## 5. Sorted concurrent collections

`ConcurrentSkipListMap` and `ConcurrentSkipListSet` are **sorted** (natural order or a `Comparator`), thread-safe, navigable (`floorKey`, `ceilingEntry`, `headMap`, ...), with O(log n) operations and no global lock. Use them instead of `TreeMap`/`TreeSet` when multiple threads need ordered access. `ConcurrentHashMap` is faster when you don't need ordering.

---

## 6. Choosing

| You need... | Use |
|---|---|
| A shared map/cache with atomic per-key updates | `ConcurrentHashMap` |
| A concurrent set | `ConcurrentHashMap.newKeySet()` |
| A sorted concurrent map/set | `ConcurrentSkipListMap/Set` |
| A list that's read constantly and rarely changed (listeners) | `CopyOnWriteArrayList` |
| Hand work between threads, with backpressure | `ArrayBlockingQueue` / bounded `LinkedBlockingQueue` |
| Direct hand-off with no buffering | `SynchronousQueue` |
| Priority-ordered work | `PriorityBlockingQueue` |
| Delayed/scheduled items | `DelayQueue` (or a `ScheduledExecutorService`) |
| Lock-free queue that never blocks | `ConcurrentLinkedQueue` |
| Constant data | Immutable collections (`List.of`, `Map.of`), which are safe to share ([Immutable and Unmodifiable Collections](../08-collections/13_immutable-and-unmodifiable-collections.md)) |
| Just one thread uses it | Plain `ArrayList` / `HashMap` |

For caches with eviction, expiry, and statistics, use a cache library rather than a hand-rolled `ConcurrentHashMap` ([Caching](../25-real-world-patterns/00_caching.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `if (!map.containsKey(k)) map.put(k, v)` on a `ConcurrentHashMap` | `putIfAbsent` / `computeIfAbsent` / `merge` |
| Storing `null` keys/values in `ConcurrentHashMap` | Use `Optional` or a sentinel |
| Slow or blocking code inside `computeIfAbsent`/`merge` | Compute first, then insert; keep mapping functions tiny |
| Recursive `computeIfAbsent` on the same map (for example memoized Fibonacci) | Use an explicit get/put, or restructure |
| Assuming `size()` is exact under concurrent writes | It's an estimate; use it for monitoring |
| Assuming ordering of `PriorityBlockingQueue` iteration | Only `poll`/`take` honor priority |
| Unbounded `LinkedBlockingQueue` between fast producers and slow consumers | Give it a capacity |
| Using `CopyOnWriteArrayList` for write-heavy lists | Use another structure |
| Using `HashMap`/`ArrayList` from several threads "because it's mostly reads" | Switch to a concurrent or immutable collection |
| Iterating a `synchronizedList` without locking | `synchronized (list) { ... }` or use a concurrent list |
| Assuming two operations on a concurrent collection form one atomic action | Use its atomic methods, or add a lock around the whole sequence |

### Debugging

- `ConcurrentModificationException` on a regular collection → it was modified during iteration (by another thread or by the loop body). The fix is a concurrent collection, a copy, or proper locking ([Iterators and Fail-Fast Behavior](../08-collections/12_iterators-and-fail-fast-behavior.md)).
- Lost entries or corrupted state in a `HashMap` → it was shared across threads unsafely.
- `OutOfMemoryError` with a queue growing → unbounded queue plus slow consumers. Check `queue.size()` over time.
- A consumer stuck in `take()` forever → the producer died without sending the stop signal. Use poison pills in `finally` or timed `poll`.
- `IllegalStateException: Recursive update` → a `computeIfAbsent` function touched the same map.

---

## Quick Summary

- Don't share plain `ArrayList`/`HashMap` across threads. Don't rely on `synchronizedX` wrappers for compound actions.
- **`ConcurrentHashMap`**: scalable, atomic `putIfAbsent`/`computeIfAbsent`/`merge`; no nulls; keep compute functions tiny; `size()` approximate; iterators weakly consistent.
- **`CopyOnWriteArrayList`**: snapshot iteration for read-mostly lists (listeners).
- **`BlockingQueue`** is the producer-consumer workhorse. **Bound it** for backpressure.
- `ConcurrentSkipListMap/Set` for sorted concurrent access.
- Use immutable collections for constant data, and plain collections for single-thread use.

**Next:** [ThreadLocal](09_thread-local.md)