# Parallel Streams

A parallel stream splits its source into chunks, processes the chunks on multiple threads, and merges the partial results. Switching is one call (`.parallel()` or `parallelStream()`), which makes it very easy to use and very easy to use wrongly.

The honest summary: **parallel streams are a CPU-bound data-parallelism tool with a narrow sweet spot.** Many pipelines get slower or incorrect when you add `.parallel()`.

**Prerequisites:** [Streams Fundamentals](04_streams-fundamentals.md), [Stream Operations](05_stream-operations.md), [Collectors](06_collectors.md), and basic thread-safety ideas from [Thread Safety](../14-concurrency/02_thread-safety.md).

---

## 1. Creating a parallel stream

```java
list.parallelStream().map(this::compute).toList();

list.stream().parallel().map(this::compute).toList();   // equivalent

IntStream.range(0, 1_000_000).parallel().sum();

stream.isParallel();       // check
```

`parallel()` and `sequential()` only set a flag on the **whole pipeline**. The **last call wins**, regardless of where in the chain you put it:

```java
list.stream().parallel().map(...).sequential().forEach(...);   // runs sequentially
```

---

## 2. How it works

```text
            source (Spliterator)
                   │ trySplit()
         ┌─────────┴─────────┐
       chunk A             chunk B
       ┌──┴──┐             ┌──┴──┐
      A1    A2            B1    B2        ← split until chunks are small enough
       │     │             │     │
   [pipeline][pipeline] [pipeline][pipeline]   ← each runs on a Fork/Join worker
       └──┬──┘             └──┬──┘
       combine              combine         ← collector combiner / reduce combiner
              └──────┬──────┘
                   result
```

- The source's **`Spliterator`** decides how well it splits.
- Work runs on the **common `ForkJoinPool`** (`ForkJoinPool.commonPool()`), shared by the whole JVM. See [Fork/Join](../14-concurrency/11_fork-join-and-parallelism.md).
- The calling thread also participates in the work.
- Partial results are merged with the **combiner** of your `Collector`, or the combiner/associative function of your `reduce`.

Default common-pool parallelism is based on available processors (roughly cores minus one). You can override it with a system property, set before the pool is first used:

```bash
java -Djava.util.concurrent.ForkJoinPool.common.parallelism=4 -jar app.jar
```

---

## 3. When parallel helps

Parallelism pays off only when **all** of these hold:

1. **Enough work.** The overhead of splitting, scheduling, and merging must be dwarfed by the actual work. As a rough heuristic, elements × cost per element should be large (tens of thousands of simple operations at minimum). Tiny collections almost never benefit.
2. **CPU-bound work.** The tasks compute rather than wait.
3. **A source that splits cheaply.**
4. **Independent, stateless operations** that can run in any order.
5. **Cores available.** On a busy server that is already saturated by request threads, there are no spare cores to gain.

### Source matters

| Splits well | Splits poorly |
|---|---|
| `ArrayList`, arrays, `IntStream.range` | `LinkedList` |
| `HashSet` / `HashMap` (reasonable) | `Stream.iterate` (inherently sequential) |
| `TreeSet` / `TreeMap` (decent) | Sources reading from I/O, `BufferedReader.lines()` |

### Operation matters

| Parallel-friendly | Costly in parallel |
|---|---|
| `map`, `filter`, `mapToInt`, `sum`, `reduce` | `sorted`, `distinct` (need buffering/coordination) |
| `collect` with a good combiner | `limit`, `skip`, `findFirst` on ordered streams (must respect order) |
| Primitive streams (no boxing) | `forEachOrdered` (forces ordering) |

---

## 4. Correctness rules

Most parallel-stream bugs are not performance bugs; they are wrong answers.

### 4.1 No shared mutable state

```java
// BROKEN: ArrayList is not thread-safe → lost elements, exceptions, nulls
List<Integer> out = new ArrayList<>();
IntStream.range(0, 10_000).parallel().forEach(out::add);

// CORRECT: let the stream own the accumulation
List<Integer> out = IntStream.range(0, 10_000).parallel().boxed().toList();
```

Wrapping the target in `synchronized` or a concurrent collection "fixes" the crash but adds contention and usually kills the speedup. Use `collect`/`reduce`.

### 4.2 `reduce` needs an identity and an associative function

For parallel `reduce(identity, accumulator, combiner)` to be correct:

- `identity` must be a true identity: `accumulator(identity, x) == x`
- the function must be **associative**
- it must be **stateless and non-interfering**

```java
// WRONG identity: each chunk starts from 100, so 100 gets added once per chunk
int bad = Stream.of(1, 2, 3, 4).parallel().reduce(100, Integer::sum);   // not 110

// WRONG: subtraction is not associative
int bad2 = Stream.of(10, 3, 2).parallel().reduce(0, (a, b) -> a - b);   // unreliable

// CORRECT
int ok = Stream.of(1, 2, 3, 4).parallel().reduce(0, Integer::sum);       // 10
```

### 4.3 Non-interference

Don't modify the stream's source while the pipeline runs. For most collections this results in `ConcurrentModificationException` or undefined behavior.

### 4.4 `forEach` vs `forEachOrdered`

```java
List.of(1, 2, 3, 4, 5).parallelStream().forEach(System.out::print);         // order not guaranteed
List.of(1, 2, 3, 4, 5).parallelStream().forEachOrdered(System.out::print);  // 12345, but loses most of the benefit
```

### 4.5 Encounter order is preserved by default

A parallel stream from an ordered source (a `List`) still gives you **ordered results** from `collect(toList())`, `toList()`, `findFirst`, and so on. This is *not* free. If you don't care about order, say so:

```java
list.parallelStream().unordered().distinct().limit(100)...   // can be much faster
```

`findAny()` is cheaper than `findFirst()` in parallel for the same reason.

---

## 5. Collectors in parallel

A parallel `collect` creates multiple containers (via the supplier), fills them in different threads, and then **merges** them with the combiner. So:

- Built-in collectors (`toList`, `groupingBy`, `joining`, ...) are already correct.
- `groupingBy` merges maps, which has a cost. `groupingByConcurrent` fills one shared `ConcurrentMap` instead, with a trade-off: it does not preserve element order within groups. Use it only after measuring.
- Custom collectors must follow the contract in [Collectors](06_collectors.md#7-how-a-collector-works).
- Calling `Collector.of(...)` with a supplier that returns a shared container is a classic bug.

---

## 6. The common-pool problem

All parallel streams in the JVM share **one** pool. Consequences:

```java
// BAD: blocking calls inside a parallel stream
ids.parallelStream().map(id -> httpClient.fetch(id)).toList();
```

- The worker threads block on I/O, so you get no CPU parallelism.
- Other parallel streams (and anything else using the common pool, such as default `CompletableFuture` async methods) are **starved** while those workers wait.
- One slow pipeline can degrade unrelated parts of the application.

Alternatives for I/O-bound work:

- an explicit `ExecutorService` ([Executors](../14-concurrency/10_executors-and-thread-pools.md))
- `CompletableFuture` with your own executor ([CompletableFuture](../14-concurrency/12_completablefuture.md))
- virtual threads ([Virtual Threads](../14-concurrency/13_virtual-threads-and-structured-concurrency.md))

### Running in a custom pool

A widely used trick is to submit the stream task to your own `ForkJoinPool`:

```java
ForkJoinPool pool = new ForkJoinPool(4);
try {
    List<Integer> result = pool.submit(() ->
        data.parallelStream().map(this::heavy).toList()
    ).get();
} finally {
    pool.shutdown();
}
```

This works in practice because the stream's tasks fork into the pool of the thread that starts them, but it is an **implementation detail, not a documented guarantee**. If you need guaranteed control of threads, use an `ExecutorService` explicitly.

---

## 7. Measure, don't guess

```java
long t0 = System.nanoTime();
// run stream
long ms = (System.nanoTime() - t0) / 1_000_000;
```

This kind of timing is misleading because of JIT warm-up, GC, and dead-code elimination. For real decisions use JMH ([Benchmarking with JMH](../20-performance/01_benchmarking-with-jmh.md)), and benchmark in conditions resembling production (same core count, same concurrent load).

A reasonable decision procedure:

```text
Is the sequential version actually too slow?            no  → stop, keep sequential
Is the work CPU-bound and per-element expensive enough? no  → stop
Is the source cheap to split (array/ArrayList/range)?   no  → probably stop
Are operations stateless, associative, non-blocking?    no  → fix first, or don't
Benchmarked under realistic load and it's faster?       yes → use parallel
```

---

## Common mistakes

| Mistake | Why it hurts |
|---|---|
| Adding `.parallel()` "for speed" on small lists | Overhead exceeds the work |
| Writing to a shared non-thread-safe collection in `forEach` | Race conditions, lost updates |
| Blocking I/O or `Thread.sleep` in a parallel stream | Starves the common pool |
| Non-associative `reduce` or a wrong identity | Wrong results that vary run to run |
| `sorted()`, `distinct()`, `limit()` on large ordered streams, assuming linear speedup | Needs coordination and buffering |
| Boxed streams (`Stream<Integer>`) for numeric work | Allocation and GC dominate; use `IntStream` and friends |
| Using `ThreadLocal` assumptions inside lambdas | Work may run on different threads, including pool threads |
| Assuming parallel output order equals sequential order for `forEach` | Order is not guaranteed there |
| Nesting parallel streams | Competes for the same pool; rarely beneficial |

### Debugging

- Wrong or fluctuating results → suspect shared state, non-associative reduce, or a bad identity. First check by removing `.parallel()`: if the bug vanishes, it was a concurrency bug.
- Occasional `ArrayIndexOutOfBoundsException` or missing elements in an output list → an unsafe collection was written from `forEach`.
- Slow service while one batch job runs → check whether that job uses `parallelStream()` on the shared common pool (thread dumps show `ForkJoinPool.commonPool-worker-N`).
- Print `Thread.currentThread().getName()` inside a lambda *temporarily* to see which threads actually run the work.

---

## Quick Summary

- `parallel()` splits the source, runs on the shared common `ForkJoinPool`, and merges results.
- It helps only for **large, CPU-bound, stateless** workloads on well-splitting sources, and only when measured.
- The last `parallel()`/`sequential()` call applies to the whole pipeline.
- `reduce` needs a true identity and an associative function. Never mutate shared state: use `collect`.
- Don't block inside parallel streams; use executors, `CompletableFuture`, or virtual threads for I/O.
- Use `unordered()` / `findAny()` when order doesn't matter, and primitive streams to avoid boxing.
- Default stance: **sequential first, parallel only with evidence.**

**Next:** [Functional Patterns](08_functional-patterns.md)
