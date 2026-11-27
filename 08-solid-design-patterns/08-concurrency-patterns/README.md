# Module 08: Concurrency Design Patterns

Welcome to **Module 08: Concurrency Design Patterns**. Concurrent execution introduces non-deterministic race conditions, deadlocks, memory visibility bugs, and unbounded queue OOMs. These 13 patterns—spanning classic POSA concurrency models and modern Java 25 Virtual Threads / Structured Concurrency, TypeScript `async/await`, and Python `asyncio`—give you battle-tested primitives for safe, scalable parallelism.

---

## Module Roadmap & Contents

| # | Note | Core Focus & Purpose | Status | Est. Time |
|---|---|---|:---:|:---:|
| 00 | [Concurrency Patterns Overview](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/00-overview.md) | Shared-state locking vs message passing vs structured concurrency | Complete | 15 min |
| 01 | [Thread Pool & Executor](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/01-thread-pool-and-executor.md) | Bounding OS threads, work queues, Virtual Threads (`newVirtualThreadPerTaskExecutor`) | Complete | 30 min |
| 02 | [Producer-Consumer](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/02-producer-consumer.md) | Bounded `BlockingQueue` / `asyncio.Queue` decoupling producers from consumers | Complete | 30 min |
| 03 | [Monitor Object & Lock Patterns](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/03-monitor-object-and-lock-patterns.md) | Synchronized invariants, condition variables, deadlock prevention (lock ordering) | Complete | 30 min |
| 04 | [Read-Write Lock](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/04-read-write-lock.md) | Concurrent readers + exclusive writers (`ReentrantReadWriteLock`, `StampedLock`) | Complete | 25 min |
| 05 | [Active Object](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/05-active-object.md) | Decoupling method invocation from execution via a dedicated mailbox & thread | Complete | 25 min |
| 06 | [Future & Promise](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/06-future-and-promise.md) | Composable asynchronous results (`CompletableFuture`, `Promise.all`, `asyncio.gather`) | Complete | 30 min |
| 07 | [Reactor & Proactor](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/07-reactor-and-proactor.md) | Single-threaded non-blocking I/O event loops (Netty, Node libuv, `asyncio`) | Complete | 25 min |
| 08 | [Immutability & Thread Confinement](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/08-immutability-and-thread-confinement.md) | Zero-lock concurrency by eliminating shared mutable state (`ThreadLocal`, records) | Complete | 25 min |
| 09 | [Actor Model](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/09-actor-model.md) | Isolated state + asynchronous mailboxes + "Let It Crash" supervision (Akka/Erlang) | Complete | 30 min |
| 10 | [Double-Checked Locking & Why It Fails](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/10-double-checked-locking-and-why-it-fails.md) | CPU/JIT reordering hazards, `volatile` happens-before, and safer alternatives | Complete | 25 min |
| 11 | [Async/Await Patterns](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/11-async-await-patterns.md) | Avoiding sequential `await` waterfalls, event-loop starvation, and unhandled rejections | Complete | 30 min |
| 12 | [Structured Concurrency Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/12-structured-concurrency-pattern.md) | Eliminating orphan thread/task leaks (`StructuredTaskScope`, `asyncio.TaskGroup`) | Complete | 30 min |
| 13 | [Backpressure & Flow Control](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/13-backpressure-and-flow-control.md) | Preventing OOM crashes when fast producers overwhelm slow consumers | Complete | 30 min |

---

[← Back to Module 07: Behavioral Patterns](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/README.md) | [Next: 00 - Concurrency Patterns Overview →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/00-overview.md)
