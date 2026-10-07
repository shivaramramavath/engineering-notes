# Fork/Join and Parallelism

The **Fork/Join framework** is designed for one shape of problem: **divide-and-conquer on CPU-bound work**. You split a big task into smaller subtasks (*fork*), solve them in parallel, and combine the results (*join*). A `ForkJoinPool` runs these tasks with **work stealing**, so idle workers take work from busy ones.

It's the engine under parallel streams ([Parallel Streams](../09-functional-java/07_parallel-streams.md)), `Arrays.parallelSort`, and the default asynchronous methods of `CompletableFuture`. Knowing it explains why those behave the way they do.

**Prerequisites:** [Executors and Thread Pools](10_executors-and-thread-pools.md), [Runnable, Callable and Future](01_runnable-callable-and-future.md).

---

## 1. Parallelism in one paragraph

**Parallelism** means using several CPU cores at the same instant to finish one job faster. It only helps when the work is **CPU-bound**, **large enough** to outweigh the cost of splitting and merging, and **divisible into independent pieces**. **Amdahl's Law** limits the gain: if a fraction *s* of the work is inherently sequential, the maximum speedup is `1 / s` no matter how many cores you add. If 10% is sequential, 100 cores can never give more than 10×.

---

## 2. The model

```text
                  sum(0..1000)
                 /            \
        sum(0..500)          sum(500..1000)        ← fork: split until small enough
        /        \             /          \
   sum(0..250) sum(250..500) ...          ...
      ↓ (below threshold: compute directly)
   partial sums  ────────── join: combine results back up the tree
```

Two task types:

| Class | `compute()` returns | Use for |
|---|---|---|
| `RecursiveTask<V>` | a result `V` | Sums, searches, merges: anything that yields a value |
| `RecursiveAction` | nothing | In-place work (sorting an array section, filling) |

### Example: parallel sum

```java
class SumTask extends RecursiveTask<Long> {
    private static final int THRESHOLD = 10_000;       // below this, just loop
    private final long[] data;
    private final int from, to;

    SumTask(long[] data, int from, int to) { this.data = data; this.from = from; this.to = to; }

    @Override
    protected Long compute() {
        if (to - from <= THRESHOLD) {
            long sum = 0;
            for (int i = from; i < to; i++) sum += data[i];
            return sum;
        }
        int mid = (from + to) >>> 1;
        SumTask left  = new SumTask(data, from, mid);
        SumTask right = new SumTask(data, mid, to);

        left.fork();                       // run `left` asynchronously (another worker may steal it)
        long rightResult = right.compute();// compute `right` in THIS thread: no point forking both
        long leftResult  = left.join();    // wait for `left`
        return leftResult + rightResult;
    }
}

long total = ForkJoinPool.commonPool().invoke(new SumTask(data, 0, data.length));
```

Idioms to copy:

- **Fork one half, compute the other directly**: it avoids a pointless extra task and keeps the current thread busy.
- Or use `invokeAll(left, right)` to fork and join both.
- **Choose a sensible threshold.** Too small → the overhead of creating tasks dominates. Too large → not enough parallelism. A common rule is to aim for a few times more tasks than cores, and then measure.
- Subtasks must be **independent** and shouldn't mutate shared state. `RecursiveAction` implementations should write to disjoint regions.

---

## 3. Work stealing

Each worker thread has its **own double-ended queue (deque)** of tasks:

- A worker pushes and pops from **its own end** (LIFO), which is cache-friendly and cheap.
- A worker with nothing to do **steals** from the **opposite end** of another worker's deque (FIFO), taking the *oldest*, usually largest, task.

This balances load automatically without a central queue everyone fights over, which is why fork/join scales better than a plain thread pool for recursive, uneven workloads.

---

## 4. `ForkJoinPool` and the common pool

```java
ForkJoinPool pool = new ForkJoinPool(4);               // your own pool with 4 workers
pool.invoke(task);                                     // run and wait for the result
pool.submit(task);                                     // returns a ForkJoinTask (a Future)

ForkJoinPool common = ForkJoinPool.commonPool();       // JVM-wide shared pool
common.getParallelism();                               // by default (available processors - 1)
```

The **common pool** is shared by the whole JVM: parallel streams, `Arrays.parallelSort`, and the default executor of `CompletableFuture.supplyAsync`/`runAsync` all use it. Resize it with `-Djava.util.concurrent.ForkJoinPool.common.parallelism=N`.

Because it's shared, **one misbehaving task can starve everything else**:

- **Don't block** (I/O, `sleep`, `Future.get()` on unrelated work, lock waits) inside fork/join or parallel-stream tasks. Blocked workers can't steal or run work. If you must block, use `ForkJoinPool.managedBlock` with a `ManagedBlocker`, which lets the pool compensate with an extra thread.
- Use your own `ForkJoinPool` (or a regular executor) for long or blocking jobs so the common pool stays responsive ([Parallel Streams](../09-functional-java/07_parallel-streams.md#6-the-common-pool-problem)).

Virtual threads run on a separate, internal `ForkJoinPool` as their scheduler ([note 13](13_virtual-threads-and-structured-concurrency.md)). You normally don't interact with it.

---

## 5. Higher-level parallelism in the JDK

You rarely write `RecursiveTask` yourself. Use the built-ins when they fit:

```java
Arrays.parallelSort(bigArray);                        // parallel merge sort for large arrays
Arrays.parallelPrefix(arr, Integer::sum);             // running totals
long n = list.parallelStream().filter(this::test).count();
```

Parallel streams are the most convenient entry point, but come with the correctness and performance caveats described in [Parallel Streams](../09-functional-java/07_parallel-streams.md): shared state, associativity, small inputs, blocking in the common pool.

`CompletableFuture` async methods run on the common pool by default too. Pass an explicit executor when the work blocks ([note 12](12_completablefuture.md)).

---

## 6. When to use Fork/Join

| Good fit | Poor fit |
|---|---|
| Recursive divide-and-conquer on in-memory data: merge sort, tree/graph processing, big array reductions | I/O-bound work: use a normal executor or virtual threads |
| CPU-bound loops that split cleanly | Tiny inputs, where the overhead exceeds the gain |
| Uneven subproblems (work stealing balances them) | Tasks that block on locks, network, or each other |
| Independent subtasks with a cheap merge step | Heavily shared mutable state |

Before parallelizing, check that the **sequential** version is actually too slow, then benchmark both with realistic data and core counts ([Benchmarking with JMH](../20-performance/01_benchmarking-with-jmh.md)). Many "parallel" versions are slower.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Threshold too small (a task per element) | Pick a threshold so each leaf does meaningful work, and measure |
| `left.fork(); right.fork(); left.join(); right.join();` | Fork one, compute the other in the current thread (or `invokeAll`) |
| Calling `join()` before forking the other half, serializing the work | Fork first, then compute, then join |
| Blocking I/O inside tasks | Use a different pool, or `ManagedBlocker` |
| Mutating shared state from subtasks | Compute independent partial results and combine them |
| Expecting linear speedup | Amdahl's Law, memory bandwidth, and overhead limit the gain |
| Running long jobs on the common pool | Dedicated `ForkJoinPool` |
| Using fork/join for tiny datasets | A plain loop is faster |
| Forgetting that exceptions propagate through `join()` | Handle exceptions at the top-level call |
| Not shutting down a custom `ForkJoinPool` | Call `shutdown()` when you're done (the common pool needs no shutdown) |

### Debugging

- Thread dumps show workers named `ForkJoinPool-1-worker-N` (custom pool) or `ForkJoinPool.commonPool-worker-N` ([Troubleshooting Playbook](../22-production-engineering/05_troubleshooting-playbook.md)). If common-pool workers are all blocked in I/O, something is blocking the shared pool.
- Parallel version slower than sequential → threshold too small, work too cheap, memory-bound, or the machine is busy with other work. Profile and benchmark.
- Results differ between runs → shared mutable state or non-associative combining.
- Slow unrelated features while a parallel job runs → it's saturating the common pool.

---

## Quick Summary

- Fork/Join = **divide-and-conquer for CPU-bound work**: `fork()` subtasks, `join()` results, run on a `ForkJoinPool` with **work stealing**.
- `RecursiveTask<V>` returns a value, `RecursiveAction` doesn't. Fork one half, compute the other directly, and use a sensible threshold.
- The **common pool** is shared by parallel streams, `parallelSort`, and default `CompletableFuture` async calls. Never block in it.
- Speedup is limited by Amdahl's Law and overhead. Benchmark before parallelizing.
- For I/O-bound concurrency use executors or virtual threads, not Fork/Join.

**Next:** [CompletableFuture](12_completablefuture.md)