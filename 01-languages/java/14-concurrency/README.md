# 14 · Concurrency

Concurrency is where Java programs go from "obviously correct" to "correct on my machine, mostly". Bugs depend on timing, so they pass tests, fail in production, and vanish when you add a log line. This module builds the mental model first, then the tools, then the modern alternatives that make a lot of the old complexity unnecessary.

## The one idea behind everything

When several threads touch the same **mutable** data, three things can go wrong:

```text
Atomicity    two steps that must happen together get interleaved   (count++ loses updates)
Visibility   one thread's write isn't seen by another               (a stop flag never "turns on")
Ordering     the compiler/CPU reorders operations                   (a half-built object becomes visible)
```

Every tool in this module exists to fix one or more of these, or to avoid shared mutable state altogether.

## Contents

| # | Note | Focus |
|---|------|-------|
| 00 | [Threads and Lifecycle](00_threads-and-lifecycle.md) | Creating threads, states, `join`, interruption, daemon threads |
| 01 | [Runnable, Callable and Future](01_runnable-callable-and-future.md) | Tasks that return results or throw; `Future` and its limits |
| 02 | [Thread Safety](02_thread-safety.md) | Race conditions, safe publication, confinement, immutability |
| 03 | [Synchronization](03_synchronization.md) | `synchronized`, monitors, `wait`/`notify` |
| 04 | [Memory Model and `volatile`](04_memory-model-and-volatile.md) | Happens-before, visibility, reordering |
| 05 | [Locks](05_locks.md) | `ReentrantLock`, `Condition`, read-write locks, `StampedLock` |
| 06 | [Atomic Classes](06_atomic-classes.md) | CAS, `AtomicInteger`, `LongAdder`, `AtomicReference` |
| 07 | [Synchronizers](07_synchronizers.md) | `CountDownLatch`, `CyclicBarrier`, `Semaphore`, `Phaser` |
| 08 | [Concurrent Collections](08_concurrent-collections.md) | `ConcurrentHashMap`, `BlockingQueue`, copy-on-write |
| 09 | [ThreadLocal](09_thread-local.md) | Per-thread state, leaks, `ScopedValue` |
| 10 | [Executors and Thread Pools](10_executors-and-thread-pools.md) | `ThreadPoolExecutor`, sizing, shutdown, scheduling |
| 11 | [Fork/Join and Parallelism](11_fork-join-and-parallelism.md) | Divide-and-conquer, work stealing, the common pool |
| 12 | [CompletableFuture](12_completablefuture.md) | Composing async pipelines, error handling, timeouts |
| 13 | [Virtual Threads and Structured Concurrency](13_virtual-threads-and-structured-concurrency.md) | Cheap blocking threads, scoped lifetimes |
| 14 | [Deadlock, Livelock, Starvation](14_deadlock-livelock-starvation.md) | Causes, diagnosis, prevention |
| 15 | [Concurrency Patterns](15_concurrency-patterns.md) | Producer-consumer, pipelines, bulkheads, graceful shutdown |

## Suggested path

- **First pass (foundations):** 00 → 01 → 02 → 03 → 04. Don't skip 02 and 04. They explain *why* the rest exist.
- **Toolbox:** 05 → 06 → 07 → 08 → 09.
- **Execution models:** 10 → 11 → 12 → 13.
- **Failure and design:** 14 → 15.

## Which tool for which problem?

| You need to... | Reach for |
|---|---|
| Run many independent tasks | An executor ([10](10_executors-and-thread-pools.md)), or virtual threads for blocking I/O ([13](13_virtual-threads-and-structured-concurrency.md)) |
| Share a counter | `AtomicLong` / `LongAdder` ([06](06_atomic-classes.md)) |
| Protect a multi-step invariant | `synchronized` or a `Lock` ([03](03_synchronization.md), [05](05_locks.md)) |
| Signal "stop" or publish a flag | `volatile` ([04](04_memory-model-and-volatile.md)) |
| Hand work between threads | `BlockingQueue` ([08](08_concurrent-collections.md)) |
| Share a map | `ConcurrentHashMap` ([08](08_concurrent-collections.md)) |
| Wait for N things to finish | `CountDownLatch`, `CompletableFuture.allOf`, or a structured scope ([07](07_synchronizers.md), [12](12_completablefuture.md)) |
| Compose async calls | `CompletableFuture` ([12](12_completablefuture.md)) |
| Split a CPU-bound computation | Fork/Join or parallel streams ([11](11_fork-join-and-parallelism.md)) |
| Avoid sharing at all | Immutability, confinement, message passing ([02](02_thread-safety.md), [15](15_concurrency-patterns.md)) |

**The best concurrency bug is the one that can't exist.** Prefer immutable data, local variables, and higher-level tools (executors, concurrent collections, `CompletableFuture`, virtual threads) before hand-written locking.

## Prerequisites

[Exceptions](../06-exceptions-and-debugging/README.md), [Lambda Expressions](../09-functional-java/00_lambda-expressions.md), [Immutability](../23-design-and-clean-code/04_immutability.md), [Collections](../08-collections/README.md), and [Duration](../10-date-and-time/03_duration-and-period.md) for timeouts.

## Related references

[Concurrency interview questions](../28-interview/07_concurrency.md) · [Concurrency cheatsheet](../29-cheatsheets/04_concurrency.md) · [Concurrency performance](../20-performance/04_concurrency-performance.md)

**Next module:** [JVM Internals](../15-jvm-internals/README.md)