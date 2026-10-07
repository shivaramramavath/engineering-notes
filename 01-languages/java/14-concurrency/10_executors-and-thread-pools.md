# Executors and Thread Pools

Creating a thread per task is expensive and unbounded. A **thread pool** keeps a small set of worker threads alive and feeds them tasks from a queue. The **Executor framework** (`java.util.concurrent`) separates *what* to run (a `Runnable`/`Callable`) from *how and where* it runs (the pool's policy), and it's the standard way to run concurrent work in Java.

```java
ExecutorService pool = Executors.newFixedThreadPool(4);
Future<Integer> f = pool.submit(() -> compute());      // task goes to the queue; a worker runs it
```

**Prerequisites:** [Threads and Lifecycle](00_threads-and-lifecycle.md), [Runnable, Callable and Future](01_runnable-callable-and-future.md), [Concurrent Collections](08_concurrent-collections.md) (queues).

---

## 1. The interfaces

```text
Executor                    execute(Runnable)
  └─ ExecutorService        submit, invokeAll, invokeAny, shutdown, awaitTermination, close
       └─ ScheduledExecutorService   schedule, scheduleAtFixedRate, scheduleWithFixedDelay
```

| Method | Behavior |
|---|---|
| `execute(Runnable)` | Fire-and-forget. An exception goes to the thread's uncaught-exception handler |
| `submit(Runnable/Callable)` | Returns a `Future`. An exception is **captured in the future** |
| `invokeAll(tasks)` / `invokeAny(tasks)` | Run a batch; wait for all / the first success ([note 01](01_runnable-callable-and-future.md)) |

---

## 2. Factory methods in `Executors`

| Factory | Threads | Queue | Notes |
|---|---|---|---|
| `newFixedThreadPool(n)` | exactly `n` | **unbounded** `LinkedBlockingQueue` | Predictable; the unbounded queue can grow without limit |
| `newCachedThreadPool()` | 0 → **unbounded**, idle ones die after 60 s | `SynchronousQueue` (direct hand-off) | Good for many short tasks; can create thousands of threads under load |
| `newSingleThreadExecutor()` | 1 | unbounded | Tasks run sequentially, in order |
| `newScheduledThreadPool(n)` | `n` | delay queue | Delayed and periodic tasks |
| `newWorkStealingPool()` | by CPU count | per-thread deques | A `ForkJoinPool` ([note 11](11_fork-join-and-parallelism.md)) |
| `newVirtualThreadPerTaskExecutor()` (21+) | a **new virtual thread per task** | none | For blocking I/O workloads ([note 13](13_virtual-threads-and-structured-concurrency.md)) |

The convenience factories hide two production hazards: an **unbounded queue** (`fixed`, `single`) and **unbounded thread count** (`cached`). For anything that faces real load, configure a `ThreadPoolExecutor` explicitly.

---

## 3. `ThreadPoolExecutor`: the real thing

```java
ThreadPoolExecutor pool = new ThreadPoolExecutor(
        4,                                   // corePoolSize: threads kept even when idle
        8,                                   // maximumPoolSize: upper bound on threads
        60, TimeUnit.SECONDS,                // keepAliveTime: how long extra (non-core) threads idle before dying
        new ArrayBlockingQueue<>(100),       // workQueue: BOUNDED, holds waiting tasks
        Thread.ofPlatform().name("orders-", 1).factory(),     // threadFactory: names threads "orders-1", "orders-2"...
        new ThreadPoolExecutor.CallerRunsPolicy());          // rejection policy
```

### How a task flows through the pool

```text
submit(task)
   │
   ├─ fewer than corePoolSize threads?        → start a NEW thread to run it (even if others are idle)
   │
   ├─ else: does the queue accept it?         → queue it; a worker will pick it up
   │
   ├─ else: fewer than maximumPoolSize?       → start an extra thread to run it
   │
   └─ else                                    → REJECT (RejectedExecutionHandler)
```

Key consequence: **threads beyond the core count are only created once the queue is full.** With an *unbounded* queue the queue never fills, so `maximumPoolSize` is never used. That's why `newFixedThreadPool` is "fixed", and why a huge `maximumPoolSize` with a big queue does nothing.

### Rejection policies

| Policy | When the pool is saturated... |
|---|---|
| `AbortPolicy` (default) | Throws `RejectedExecutionException` |
| `CallerRunsPolicy` | The **submitting thread runs the task itself**, which naturally slows the producer (backpressure) |
| `DiscardPolicy` | Silently drops the task |
| `DiscardOldestPolicy` | Drops the oldest queued task and retries |

Pick deliberately. Silent discarding hides data loss, and `CallerRunsPolicy` on a request thread makes that request slow rather than failing.

### Other knobs

```java
pool.allowCoreThreadTimeOut(true);     // let even core threads time out when idle
pool.prestartAllCoreThreads();         // start core threads up front (avoid first-request latency)
```

Subclass hooks `beforeExecute`, `afterExecute`, and `terminated` are useful for metrics and logging task exceptions.

---

## 4. Sizing a pool

There is no universal number. Reason from the kind of work:

| Workload | Guideline |
|---|---|
| **CPU-bound** (computation) | About the number of cores (maybe +1). More threads only add context switching |
| **I/O-bound** (blocking on network/DB) | More than cores. A common estimate: `cores × (1 + waitTime / computeTime)` |
| Mixed | Separate pools for separate kinds of work ([bulkhead](15_concurrency-patterns.md)) |

```java
int cores = Runtime.getRuntime().availableProcessors();    // respects container CPU limits in modern JVMs
```

Then **measure** under realistic load. Another useful relation is Little's Law: concurrency needed ≈ throughput × latency. If each request takes 200 ms and you want 500 requests/s, you need ~100 requests in flight.

Don't forget downstream limits: 200 threads calling a database pool of 20 connections mostly wait for connections ([Connection Pooling](../16-jdbc-and-databases/05_connection-pooling.md)). For heavy blocking I/O with Java 21+, virtual threads usually beat tuning huge pools ([note 13](13_virtual-threads-and-structured-concurrency.md)).

Use a **bounded queue** unless you've decided that unbounded growth is acceptable: a bounded queue gives you backpressure, while an unbounded one hides overload until memory runs out.

---

## 5. Shutting down properly

A pool's worker threads are non-daemon by default, so a pool that is never shut down **keeps the JVM alive**.

| Method | Effect |
|---|---|
| `shutdown()` | Stop accepting tasks; let queued and running tasks finish |
| `shutdownNow()` | Stop accepting, **interrupt** running tasks, and return the list of never-started tasks |
| `awaitTermination(t, unit)` | Block until the pool has terminated or the time elapses |
| `close()` (Java 19+) | `shutdown()` then waits for termination (interrupting if the waiting thread is interrupted). Enables try-with-resources |

```java
try (ExecutorService pool = Executors.newFixedThreadPool(4)) {     // Java 19+
    pool.submit(task1);
    pool.submit(task2);
}   // close(): waits for submitted tasks to finish
```

The classic idiom for older code or for a timeout:

```java
pool.shutdown();                                         // graceful: let queued tasks complete
try {
    if (!pool.awaitTermination(30, TimeUnit.SECONDS)) {
        pool.shutdownNow();                              // timed out: interrupt the stragglers
        if (!pool.awaitTermination(10, TimeUnit.SECONDS)) {
            log.warn("pool did not terminate");
        }
    }
} catch (InterruptedException e) {
    pool.shutdownNow();
    Thread.currentThread().interrupt();
}
```

Tasks must respond to interruption for `shutdownNow()` to be effective ([note 00](00_threads-and-lifecycle.md#4-interruption-the-cooperative-way-to-stop)). Register shutdown in your application's lifecycle (framework hook or shutdown hook) so in-flight work isn't lost.

---

## 6. Exceptions in tasks

```java
pool.execute(() -> { throw new RuntimeException("x"); });   // thread's uncaught handler prints it; the worker thread dies and is replaced
pool.submit(() -> { throw new RuntimeException("x"); });    // SWALLOWED into the Future: nobody sees it unless you call get()
```

If you use `submit` and ignore the `Future`, failures vanish. Either call `get()`, catch and log inside the task, or use `execute` plus an uncaught-exception handler (via the `ThreadFactory`) for fire-and-forget work.

---

## 7. `ScheduledExecutorService`

```java
ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(2);

scheduler.schedule(() -> sendReminder(), 10, TimeUnit.MINUTES);                  // once, after a delay

scheduler.scheduleAtFixedRate(() -> poll(), 0, 5, TimeUnit.SECONDS);             // every 5 s measured from start times
scheduler.scheduleWithFixedDelay(() -> poll(), 0, 5, TimeUnit.SECONDS);          // 5 s AFTER each run finishes
```

| | `scheduleAtFixedRate` | `scheduleWithFixedDelay` |
|---|---|---|
| Spacing | Start-to-start | End-to-start |
| If a run takes longer than the period | Next run starts as soon as the previous ends (runs don't overlap, but may "catch up") | Always waits the full delay |

**Important:** if a periodic task **throws**, all future executions are **silently suppressed**. Wrap the body in `try/catch` and log:

```java
scheduler.scheduleAtFixedRate(() -> {
    try { poll(); }
    catch (Exception e) { log.error("poll failed", e); }       // keep the schedule alive
}, 0, 5, TimeUnit.SECONDS);
```

Prefer this over the legacy `java.util.Timer` (single thread, one failed task kills the timer). For cron-like scheduling, persistence, and clustering, see [Background Jobs](../25-real-world-patterns/05_background-jobs.md).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Never shutting the pool down, so the JVM won't exit | `shutdown()` / try-with-resources / lifecycle hook |
| `newFixedThreadPool` under heavy load (unbounded queue → memory) | Bounded `ThreadPoolExecutor` with a rejection policy |
| `newCachedThreadPool` for slow tasks (thread explosion) | Bounded pool |
| `submit` and ignoring the `Future` → hidden exceptions | `get()`, or log inside, or use `execute` |
| A task that blocks waiting for another task *in the same pool* | Pool starvation deadlock: separate pools, or avoid nested waits ([note 14](14_deadlock-livelock-starvation.md)) |
| Single-thread executor with tasks that depend on each other | They run strictly in sequence: a task waiting on a later one hangs |
| Large `maximumPoolSize` with an unbounded queue | Extra threads never start: set the queue bound |
| Unnamed pool threads | Provide a `ThreadFactory` with names |
| Periodic task that can throw | Catch inside the task |
| One shared pool for everything (a slow dependency exhausts it) | Separate pools per dependency (bulkhead) |
| Sizing the pool by guess | Measure and tune; consider virtual threads for blocking I/O |
| Using `shutdownNow()` and expecting tasks to stop | They must respond to interrupts |

### Debugging

- Take a thread dump ([Troubleshooting Playbook](../22-production-engineering/05_troubleshooting-playbook.md)): all workers `WAITING` on a queue → idle; all `RUNNABLE`/`BLOCKED` with a long queue → saturated or blocked on a downstream.
- Inspect at runtime: `getPoolSize()`, `getActiveCount()`, `getQueue().size()`, `getCompletedTaskCount()`, `getLargestPoolSize()`. Export them as metrics.
- `RejectedExecutionException` → pool is saturated, or it was shut down. Check which.
- Growing memory with a steady pool → an unbounded queue is filling.
- JVM won't exit → a non-daemon pool thread is still alive.
- Requests slow while CPU is idle → threads are blocked on I/O or a lock, or the pool is too small for the blocking time.

---

## Quick Summary

- An `ExecutorService` runs tasks on pooled threads, decoupling *submission* from *execution*.
- Task flow: core threads → **queue** → extra threads up to max → **reject**. With an unbounded queue, max is never used.
- `Executors.newFixedThreadPool`/`newCachedThreadPool` are convenient but hide unbounded queue/thread growth. Use a configured `ThreadPoolExecutor` with a **bounded queue**, named threads, and a deliberate rejection policy.
- Size by workload: ~cores for CPU-bound; more for I/O-bound; measure. Virtual threads are the simpler answer for blocking I/O.
- **Always shut down** (`close()`/try-with-resources, or `shutdown` + `awaitTermination` + `shutdownNow`).
- `submit` captures exceptions in the `Future`. Scheduled tasks that throw stop being rescheduled, so catch inside.

**Next:** [Fork/Join and Parallelism](11_fork-join-and-parallelism.md)