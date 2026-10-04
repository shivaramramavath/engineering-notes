# PriorityQueue

A `PriorityQueue<E>` is a queue where the element removed next is the one with the **highest priority** (by default the **smallest**), not the one that arrived first. It is implemented as a **binary heap** stored in an array. Use it whenever you repeatedly need "the best remaining item": task scheduling, top-K problems, shortest paths, merging sorted streams.

```java
PriorityQueue<Integer> pq = new PriorityQueue<>();
pq.offer(5); pq.offer(1); pq.offer(3);
pq.peek();           // 1: the smallest, not removed
pq.poll();           // 1
pq.poll();           // 3
pq.poll();           // 5
pq.poll();           // null: empty
```

## The contract

| Property | Detail |
|----------|--------|
| Head of the queue | The **least** element according to the ordering (natural or `Comparator`) |
| `poll()`, `remove()`, `peek()`, `element()` | Act on the head |
| Order of **iteration** | **Not sorted**: only the head is guaranteed to be the minimum |
| Duplicates | Allowed |
| `null` elements | **Not allowed** |
| Ties | Elements that compare equal come out in **unspecified** order (not FIFO, not stable) |
| Thread-safety | None: use `PriorityBlockingQueue` for concurrent use |

```java
PriorityQueue<Integer> pq = new PriorityQueue<>(List.of(5, 1, 3, 2, 4));
System.out.println(pq);              // e.g. [1, 2, 3, 5, 4]: heap layout, NOT sorted
for (int x : pq) { }                 // same arbitrary order

while (!pq.isEmpty()) System.out.print(pq.poll() + " ");     // 1 2 3 4 5  (sorted, by repeated poll)
```

## How the binary heap works

A **min-heap** is a complete binary tree in which every parent is **≤ its children**, so the smallest element is always at the root. It is stored in a flat array with no node objects:

```
 array index:   0   1   2   3   4   5
 values:      [ 1 | 2 | 3 | 5 | 4 | 8 ]

 tree view:           1            index i:  parent = (i - 1) / 2
                    /   \                    left   = 2i + 1
                   2     3                   right  = 2i + 2
                  / \   /
                 5   4 8
```

### `offer(e)`: sift up, O(log n)

Add at the end (next free slot), then swap with its parent while it is smaller than the parent.

```
add 0:        1               0 ≤ 3 (parent) → swap, 0 ≤ 1 → swap
            /   \
           2     3            result:      0
          / \   / \                       /   \
         5   4 8   0 ←new                2     1
                                        / \   / \
                                       5   4 8   3
```

### `poll()`: sift down, O(log n)

Take the root, move the **last** element to the root, then swap it with the **smaller child** until the heap property holds again.

### Building from a collection: O(n)

`new PriorityQueue<>(collection)` copies the elements and **heapifies** them in O(n), faster than n separate `offer` calls (O(n log n)).

## Cost model

| Operation | Cost |
|-----------|------|
| `offer`, `add` | **O(log n)** |
| `poll`, `remove()` | **O(log n)** |
| `peek`, `element` | **O(1)** |
| `size`, `isEmpty` | O(1) |
| `contains(o)`, `remove(o)` | **O(n)**: linear search |
| Build from n elements | **O(n)** |
| Iterate | O(n), heap order |

Default initial capacity is 11; the array grows when full (about 2× when small, then 1.5×).

## Ordering

```java
PriorityQueue<Integer> minHeap = new PriorityQueue<>();                          // natural order: smallest first
PriorityQueue<Integer> maxHeap = new PriorityQueue<>(Comparator.reverseOrder()); // largest first

record Task(String name, int priority) { }
PriorityQueue<Task> tasks = new PriorityQueue<>(Comparator.comparingInt(Task::priority));
PriorityQueue<Task> byPriorityThenName = new PriorityQueue<>(
        Comparator.comparingInt(Task::priority).thenComparing(Task::name));
```

Elements must be `Comparable` or you must supply a `Comparator`; otherwise `ClassCastException`. See [05_comparable-and-comparator.md](./05_comparable-and-comparator.md). **Never** use `a - b` in the comparator ([overflow](./05_comparable-and-comparator.md#never-subtract-to-compare)).

### FIFO among equal priorities

Because ties are unordered, add a **sequence number** if arrival order matters:

```java
record Job(int priority, long seq, String name) { }
AtomicLong counter = new AtomicLong();
PriorityQueue<Job> q = new PriorityQueue<>(Comparator.comparingInt(Job::priority).thenComparingLong(Job::seq));
q.offer(new Job(1, counter.getAndIncrement(), "first"));
```

## Changing priorities

The heap does **not** notice if an element's priority changes after insertion: the invariant silently breaks.

```java
Task t = pq.peek();
t.setPriority(99);        // WRONG: pq is no longer a valid heap
```

To update, **remove and re-insert** (`remove(Object)` is O(n)), or use **lazy deletion** (a common technique in Dijkstra): insert a new entry with the better priority and skip stale entries when polled.

```java
// Dijkstra-style: duplicates allowed, ignore outdated ones
record State(int node, int dist) { }
PriorityQueue<State> pq = new PriorityQueue<>(Comparator.comparingInt(State::dist));
pq.offer(new State(start, 0));
while (!pq.isEmpty()) {
    State s = pq.poll();
    if (s.dist() > best[s.node()]) continue;          // stale entry: a shorter path was already found
    for (Edge e : graph.get(s.node())) {
        int nd = s.dist() + e.weight();
        if (nd < best[e.to()]) { best[e.to()] = nd; pq.offer(new State(e.to(), nd)); }
    }
}
```

## Classic uses

### Top K largest elements: a min-heap of size K

```java
static List<Integer> topK(int[] nums, int k) {
    PriorityQueue<Integer> heap = new PriorityQueue<>();      // min-heap holds the K largest seen so far
    for (int n : nums) {
        heap.offer(n);
        if (heap.size() > k) heap.poll();                     // drop the smallest of the K+1
    }
    List<Integer> result = new ArrayList<>(heap);
    result.sort(Comparator.reverseOrder());
    return result;
}
```

O(n log k) time and O(k) memory, better than sorting everything (O(n log n)) when `k` is small, and it works on streams too large to hold in memory. (For the K **smallest**, use a max-heap.)

### Merge K sorted lists

```java
record Cursor(int value, Iterator<Integer> rest) { }
PriorityQueue<Cursor> pq = new PriorityQueue<>(Comparator.comparingInt(Cursor::value));
for (List<Integer> list : lists) {
    Iterator<Integer> it = list.iterator();
    if (it.hasNext()) pq.offer(new Cursor(it.next(), it));
}
List<Integer> merged = new ArrayList<>();
while (!pq.isEmpty()) {
    Cursor c = pq.poll();
    merged.add(c.value());
    if (c.rest().hasNext()) pq.offer(new Cursor(c.rest().next(), c.rest()));
}
```

### Running median: two heaps

```java
PriorityQueue<Integer> low = new PriorityQueue<>(Comparator.reverseOrder());   // max-heap: smaller half
PriorityQueue<Integer> high = new PriorityQueue<>();                           // min-heap: larger half

void add(int x) {
    low.offer(x);
    high.offer(low.poll());                       // keep every element of low <= every element of high
    if (high.size() > low.size()) low.offer(high.poll());   // balance sizes (low may have one extra)
}
double median() { return low.size() > high.size() ? low.peek() : (low.peek() + high.peek()) / 2.0; }
```

### Scheduling

Event simulation, job schedulers, rate limiters with deadlines, `ScheduledThreadPoolExecutor` (a delay-ordered heap) and A* search all use priority queues ([14-concurrency/10_executors-and-thread-pools.md](../14-concurrency/10_executors-and-thread-pools.md)).

### Heap sort

```java
PriorityQueue<Integer> pq = new PriorityQueue<>(Arrays.asList(array));
for (int i = 0; i < array.length; i++) array[i] = pq.poll();      // O(n log n)
```

More patterns: [27-dsa/08-heap-and-priority-queue](../27-dsa/08-heap-and-priority-queue/).

## `PriorityQueue` vs `TreeSet` vs sorting

| | `PriorityQueue` | `TreeSet` / `TreeMap` | Sort a list once |
|---|-----------------|-----------------------|------------------|
| Get the min/max | O(1) peek | O(log n) `first()` | O(1) after sorting |
| Insert | O(log n) (cheap in practice) | O(log n) | O(n) shift or re-sort |
| Remove min | O(log n) | O(log n) | O(n) from a list front |
| Remove arbitrary element | O(n) | **O(log n)** | O(n) |
| Duplicates | **Allowed** | Rejected as duplicates (by comparator) | Allowed |
| Iterate in sorted order | No (poll repeatedly) | **Yes** | Yes |
| Range / floor / ceiling queries | No | **Yes** | Binary search |
| Memory | Compact (array) | Node objects | Compact |

Choose `PriorityQueue` when you only ever need the **next** best element, with duplicates and frequent inserts. Choose a tree when you also need ordering, ranges or arbitrary removal.

## Concurrency

`PriorityQueue` is not thread-safe. Use `PriorityBlockingQueue` (blocking `take`, unbounded) for producer-consumer ([14-concurrency/08_concurrent-collections.md](../14-concurrency/08_concurrent-collections.md)), or `DelayQueue` for time-based release.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Printing or iterating the queue to see sorted order | Heap layout, not sorted | `poll()` in a loop, or copy and sort |
| Mutating an element's priority in place | Heap invariant broken, wrong `poll` results | Remove and re-add, or lazy deletion |
| Expecting FIFO among equal priorities | Arbitrary order | Add a sequence number tie-breaker |
| Elements not `Comparable`, no comparator | `ClassCastException` | Provide a `Comparator` |
| `null` elements | `NullPointerException` | Never add `null` |
| `Comparator` using subtraction | Overflow, wrong order | `Integer.compare` / `comparingInt` |
| Using `contains`/`remove(Object)` often | O(n) each | Auxiliary `HashSet`/`HashMap`, or lazy deletion |
| Max-heap by negating values | Fails for `Integer.MIN_VALUE` | `Comparator.reverseOrder()` |
| Sharing across threads | Corruption | `PriorityBlockingQueue` |
| Using it when you need sorted iteration or ranges | Awkward code | `TreeSet`/`TreeMap` |
| Offering n items one by one when you have them all | O(n log n) vs O(n) | Pass the collection to the constructor |

## Key takeaways

- A `PriorityQueue` is an array-based binary heap: `peek` O(1), `offer`/`poll` O(log n), `contains`/`remove(Object)` O(n)
- Default is a **min**-heap; use `Comparator.reverseOrder()` for a max-heap
- Only the head is ordered; iteration and `toString` show heap layout
- Ties are unordered; priorities must not change while queued
- Great for top-K (heap of size K), merging, scheduling, Dijkstra and running medians

**Next:** [Specialized Collections](./11_specialized-collections.md)
