# Module 14: Concurrency and Multithreading

> The definitive architectural guide to multi-threading, the Java Memory Model, lock-free atomics, high-throughput concurrent collections, executors, Project Loom Virtual Threads, and Structured Concurrency.

---

## Module Overview

Concurrency is at the core of high-performance Java enterprise applications. Over decades of innovation culminating in Java 21–25 LTS, Java has evolved from low-level thread and monitor mechanics into a premier ecosystem featuring non-blocking atomics, the modern Java Memory Model (JSR-133), fork-join work-stealing, and Project Loom's revolutionary Virtual Threads and Structured Concurrency.

---

## Notes Index

| # | Note | Description | Status |
|---|---|---|---|
| 00 | [00_threads-and-lifecycle.md](00_threads-and-lifecycle.md) | Platform vs Virtual threads, 6 JVM thread states, cooperative interruption | Complete (470+ lines) |
| 01 | [01_runnable-callable-and-future.md](01_runnable-callable-and-future.md) | Asynchronous task modeling, Callable vs Runnable, Future state machine | Complete (480+ lines) |
| 02 | [02_thread-safety.md](02_thread-safety.md) | Shared mutable state, atomicity, visibility, ordering, immutability | Complete (420+ lines) |
| 03 | [03_synchronization.md](03_synchronization.md) | Intrinsic locks, monitors, Mark Word inflation, wait/notify, reentrancy | Complete (470+ lines) |
| 04 | [04_memory-model-and-volatile.md](04_memory-model-and-volatile.md) | JSR-133 JMM, Happens-Before relationship, volatile fences, safe publication | Complete (460+ lines) |
| 05 | [05_locks.md](05_locks.md) | ReentrantLock, fair vs non-fair, Condition variables, StampedLock optimistic | Complete (460+ lines) |
| 06 | [06_atomic-classes.md](06_atomic-classes.md) | Hardware CAS, AtomicInteger/Reference, ABA problem, LongAdder striping | Complete (415+ lines) |
| 07 | [07_synchronizers.md](07_synchronizers.md) | CountDownLatch, CyclicBarrier, Semaphore, Phaser, Exchanger | Complete (450+ lines) |
| 08 | [08_concurrent-collections.md](08_concurrent-collections.md) | ConcurrentHashMap internals, CopyOnWriteArrayList, BlockingQueue variants | Complete (435+ lines) |
| 09 | [09_thread-local.md](09_thread-local.md) | ThreadLocalMap internals, weak key memory leaks in pools, removal protocols | Complete (405+ lines) |
| 10 | [10_executors-and-thread-pools.md](10_executors-and-thread-pools.md) | ThreadPoolExecutor sizing math, bounded queues, saturation policies, shutdown | Complete (420+ lines) |
| 11 | [11_fork-join-and-parallelism.md](11_fork-join-and-parallelism.md) | ForkJoinPool, RecursiveTask, work-stealing algorithm, commonPool hazards | Complete (455+ lines) |
| 12 | [12_completablefuture.md](12_completablefuture.md) | Non-blocking reactive pipelines, thenApply/Compose/Combine, allOf/anyOf | Complete (450+ lines) |
| 13 | [13_virtual-threads-and-structured-concurrency.md](13_virtual-threads-and-structured-concurrency.md) | Project Loom, Virtual Threads (JEP 444), continuations, carrier pinning, StructuredTaskScope | Complete (460+ lines) |
| 14 | [14_deadlock-livelock-starvation.md](14_deadlock-livelock-starvation.md) | Coffman conditions, deadlock detection via ThreadMXBean, lock ordering | Complete (450+ lines) |
| 15 | [15_concurrency-patterns.md](15_concurrency-patterns.md) | Producer-Consumer, Active Object, Guarded Suspension, Double-Checked Locking | Complete (440+ lines) |
| 16 | [16_scoped-values.md](16_scoped-values.md) | Scoped Values (JEP 506 / JEP 446), immutable context, Virtual Thread scaling | Complete (430+ lines) |

---

## Recommended Learning Path
1. Master core lifecycle and hazards in `00_threads-and-lifecycle.md` through `04_memory-model-and-volatile.md`.
2. Adopt explicit locks and non-blocking synchronization in `05_locks.md` through `07_synchronizers.md`.
3. Build robust pipelines with `08_concurrent-collections.md`, `10_executors-and-thread-pools.md`, and `12_completablefuture.md`.
4. Embrace the modern paradigm with `13_virtual-threads-and-structured-concurrency.md` and `16_scoped-values.md`.
