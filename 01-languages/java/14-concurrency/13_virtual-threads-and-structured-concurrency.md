# Virtual Threads and Structured Concurrency

For two decades, Java servers were limited by one fact: a **platform thread is an OS thread**, and a machine can only run a few thousand. So we built thread pools, then async callbacks and `CompletableFuture` chains, to avoid blocking threads.

**Virtual threads** (final in **Java 21**, JEP 444) remove that constraint. They're ordinary `Thread`s that are cheap enough to create **millions** of, so you can go back to simple **one thread per task, blocking-style code** and still scale.

**Structured concurrency** (a **preview API** as of Java 27) complements them: it treats a group of related subtasks as one unit with a clear lifetime, cancellation, and error handling.

**Prerequisites:** [Threads and Lifecycle](00_threads-and-lifecycle.md), [Executors and Thread Pools](10_executors-and-thread-pools.md), [CompletableFuture](12_completablefuture.md).

---

## 1. What a virtual thread is

```text
 platform thread (OS thread, ~1 MB stack, scarce)
      ▲  carrier threads: a small pool (by default as many as CPU cores) of platform threads
      │
 ┌────┴──────────────────────────────────────────────┐
 │  virtual thread 1   virtual thread 2   ...  vthread 1,000,000 │   ← JVM-managed, tiny, heap-allocated stacks
 └───────────────────────────────────────────────────┘
```

- A virtual thread is **scheduled by the JVM**, not the OS. It runs *mounted* on a carrier thread (a platform thread from an internal `ForkJoinPool`).
- When it **blocks** (on I/O, `sleep`, a lock, a `BlockingQueue`), the JVM **unmounts** it: its stack is saved on the heap, and the carrier thread is free to run another virtual thread. When the blocking operation completes, the virtual thread is mounted again.
- It's still a `java.lang.Thread`. The API, `ThreadLocal`, interruption, and stack traces all work as before.

The payoff is **throughput for blocking, I/O-bound work**. If each request spends 95% of its time waiting on a database or HTTP call, a platform thread sits idle that whole time, whereas a virtual thread costs almost nothing while waiting.

---

## 2. Creating virtual threads

```java
Thread t1 = Thread.startVirtualThread(() -> handle(request));              // create + start

Thread t2 = Thread.ofVirtual().name("worker-", 0).start(() -> handle(request));   // builder: names worker-0, worker-1...

Thread t3 = Thread.ofVirtual().unstarted(task);   t3.start();

try (ExecutorService executor = Executors.newVirtualThreadPerTaskExecutor()) {    // the usual way
    for (Request r : requests) {
        executor.submit(() -> handle(r));          // one NEW virtual thread per task
    }
}   // close() waits for all submitted tasks
```

`Executors.newVirtualThreadPerTaskExecutor()` is not a pool. It creates a new thread for every task and ends it when the task finishes.

```java
Thread.currentThread().isVirtual();       // true inside a virtual thread
```

Virtual threads are always **daemon** threads, and their priority is fixed.

---

## 3. Using them well

Virtual threads change the rules of thumb you learned for platform threads:

| Old habit (platform threads) | With virtual threads |
|---|---|
| **Pool threads** to limit and reuse them | **Don't pool them.** Create one per task. They're cheap |
| Tune pool size to control concurrency | Limit concurrency of a *resource* with a `Semaphore` ([note 07](07_synchronizers.md)) |
| Avoid blocking; use callbacks/reactive code | **Blocking is fine**: write straight-line code |
| Thread-local caches of expensive objects | Avoid heavy thread locals; prefer `ScopedValue` ([note 09](09_thread-local.md)) |

### Limit what matters: the downstream resource

A million virtual threads are fine; a million simultaneous calls to a database that allows 50 connections are not. Cap the *resource*:

```java
private final Semaphore dbPermits = new Semaphore(50);

Result query(String sql) throws InterruptedException {
    dbPermits.acquire();
    try { return db.run(sql); }
    finally { dbPermits.release(); }
}
```

Connection pools ([Connection Pooling](../16-jdbc-and-databases/05_connection-pooling.md)) and HTTP client limits provide the same protection. Don't confuse "no thread-count limit" with "no limits".

### A server in the thread-per-request style

```java
try (var server = new ServerSocket(8080);
     var executor = Executors.newVirtualThreadPerTaskExecutor()) {
    while (true) {
        Socket client = server.accept();
        executor.submit(() -> handle(client));     // simple blocking code, scales to huge connection counts
    }
}
```

Many frameworks can run request handling on virtual threads by configuration. Check your framework's documentation for the switch.

---

## 4. What virtual threads do *not* do

- **They don't speed up CPU-bound work.** CPU-bound tasks are limited by cores. A million virtual threads computing hashes just take turns on the same few carriers. Use a fixed pool or Fork/Join ([note 11](11_fork-join-and-parallelism.md)).
- **They don't remove the need for thread safety.** Shared mutable state still needs the tools from notes 02-08.
- **They don't make individual operations faster**, only let many of them wait at the same time.
- **They don't replace `CompletableFuture`** for composing independent async calls, but they remove much of the *need* for it ([note 12](12_completablefuture.md)).

---

## 5. Pinning and other caveats

A virtual thread is **pinned** when it can't be unmounted while blocked, so it holds its carrier thread hostage. Pinning reduces scalability (and in the worst case can exhaust the carriers).

| Cause | Status |
|---|---|
| Blocking inside a `synchronized` block/method | **Fixed in Java 24** (JEP 491): virtual threads can now unmount while blocked inside `synchronized`. On Java 21-23, prefer `ReentrantLock` for locks held across blocking calls ([note 05](05_locks.md)) |
| Blocking inside a **native method** or a foreign-function call | Still pins |
| Some file-system and class-initialization operations | May pin or temporarily compensate; check the release notes for your JDK |

How to find it: JFR emits a **`jdk.VirtualThreadPinned`** event when a virtual thread blocks while pinned for longer than a threshold ([JVM Diagnostics](../22-production-engineering/04_jvm-diagnostics.md)).

Other things to keep in mind:

- **Thread dumps:** a traditional `jstack`/`Thread.print` doesn't list virtual threads. Use `jcmd <pid> Thread.dump_to_file -format=json <file>` to see them ([Troubleshooting Playbook](../22-production-engineering/05_troubleshooting-playbook.md)).
- **`ThreadLocal`s are per virtual thread.** With millions of them, large thread-local values multiply.
- **Don't write your own pool of virtual threads.** Pooling defeats their purpose and keeps stale thread-local state alive.
- **Interruption and `Thread` APIs work as normal**, including `join()` and `interrupt()`.
- **GC and memory:** stacks live on the heap, so huge numbers of *deep* blocked stacks cost memory. They are cheap, not free.

---

## 6. Structured concurrency (preview)

> **Preview feature.** `StructuredTaskScope` is a *preview API* (seventh preview in Java 27, JEP 533). It needs `--enable-preview`, and its API has changed between previews: the code below matches the Java 27 shape and may not compile on older JDKs. Don't depend on it in shipped libraries. Check the current JEP before using it. Related finals: `ScopedValue` (Java 25).

### The problem with unstructured concurrency

```java
// Executor + Future: lifetimes are manual
Future<User> u = executor.submit(() -> findUser());
Future<Order> o = executor.submit(() -> fetchOrder());
User user = u.get();              // if this throws, `o` keeps running: a leaked task
Order order = o.get();
```

Nothing ties the subtasks to the method that started them. If one fails, the other keeps running, an interrupted parent leaves orphans, and thread dumps show unrelated threads with no hierarchy.

### The structured version

```java
Response handle() throws InterruptedException, ExecutionException {
    try (var scope = StructuredTaskScope.open()) {                 // opens a scope: owns its subtasks
        Subtask<User>  user  = scope.fork(() -> findUser());       // each fork runs in its own (virtual) thread
        Subtask<Order> order = scope.fork(() -> fetchOrder());

        scope.join();                                              // wait for both; throws if any failed
        return new Response(user.get(), order.get());              // safe: both succeeded
    }                                                              // leaving the block guarantees no subtask is still running
}
```

The principle: **subtasks work on behalf of the task that forked them, and cannot outlive its scope.**

- **Default policy:** `join()` waits until all subtasks succeed, or **one fails**. On failure, the remaining subtasks are **cancelled** and the failure is thrown (in the Java 27 preview, as `ExecutionException`).
- **No leaks:** the `try`-with-resources block can't exit while subtasks run. Closing the scope cancels them.
- **Cancellation propagates:** interrupting the parent cancels the scope's subtasks.
- **Observability:** thread dumps can show the parent/child relationship.

### Other policies: joiners

```java
// First successful result wins; the rest are cancelled ("race")
try (var scope = StructuredTaskScope.open(Joiner.<String>anySuccessfulOrThrow())) {
    mirrors.forEach(m -> scope.fork(() -> fetchFrom(m)));
    return scope.join();
}

// Collect all results (fails if any fails)
try (var scope = StructuredTaskScope.open(Joiner.<Price>allSuccessfulOrThrow())) {
    ids.forEach(id -> scope.fork(() -> priceFor(id)));
    List<Price> prices = scope.join();
}

// Timeout via configuration
try (var scope = StructuredTaskScope.open(cf -> cf.withTimeout(Duration.ofSeconds(2)))) { ... }
```

### When to use what

| Need | Use |
|---|---|
| Fan-out a few related calls, all-or-nothing, with clean cancellation | Structured scope (when you can use preview) |
| Same, on a stable JDK | Virtual-thread executor + `Future`s, with care for cancellation, or `CompletableFuture` |
| Composing async pipelines with transformations | `CompletableFuture` |
| Independent fire-and-forget tasks | Executor |

---

## 7. Decision guide

```text
Is the work mostly waiting on I/O (HTTP, DB, queues)?
 ├─ yes → virtual threads (Java 21+): one per task, blocking code, limit the downstream with a Semaphore
 └─ no, CPU-bound → fixed pool / Fork-Join / parallel streams, sized to the cores

Need related subtasks to succeed/fail/cancel together?
 └─ structured concurrency (preview), or carefully structured executor/CompletableFuture code
```

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Pooling virtual threads (`newFixedThreadPool` of virtual threads) | One virtual thread per task via `newVirtualThreadPerTaskExecutor` |
| Using virtual threads for CPU-bound work | Platform pool sized to cores |
| No limit on concurrent calls to a scarce downstream | `Semaphore`, connection pool limits, rate limiter |
| Holding `synchronized` across blocking calls on Java 21-23 | `ReentrantLock`, or upgrade to Java 24+ |
| Heavy per-thread state in `ThreadLocal` | Shared pool/object, or `ScopedValue` |
| Expecting `jstack` to show all virtual threads | `jcmd <pid> Thread.dump_to_file -format=json` |
| Assuming virtual threads remove the need for thread safety | Shared mutable state is still shared |
| Blocking in native code and wondering about low throughput | Pinning: find it with JFR |
| Using the preview structured-concurrency API in production libraries | Wait for it to become final |
| Treating `Executors.newVirtualThreadPerTaskExecutor()` like a bounded pool | It never queues or rejects. Add limits yourself |

### Debugging

- Throughput lower than expected → look for pinning (`jdk.VirtualThreadPinned` JFR events), CPU-bound sections, or a downstream bottleneck (database, remote service).
- A suspicious number of threads blocked → dump with `Thread.dump_to_file -format=json` and look at what they wait on.
- `--enable-preview` errors with structured concurrency → compile and run with the same JDK version and the flag; check that your code matches that version's API.
- Out-of-memory with huge numbers of tasks → each in-flight task still holds its stack and objects. Bound concurrency with a semaphore.

---

## Quick Summary

- **Virtual threads (Java 21)**: cheap, JVM-scheduled threads that **unmount while blocked**. Use them for blocking, I/O-bound work: one per task, plain blocking code.
- Create with `Executors.newVirtualThreadPerTaskExecutor()` or `Thread.ofVirtual()`. **Never pool them.**
- They help throughput of waiting tasks, not CPU-bound work, and don't remove the need for thread safety or resource limits (use a `Semaphore`/connection pool).
- **Pinning** (blocking in `synchronized` before Java 24, or in native code) hurts scalability. Detect it with JFR.
- **Structured concurrency** (preview): a `StructuredTaskScope` ties subtasks to a scope. They all finish, fail, or cancel together, with no leaks. Treat it as preview and check the current API.
- `ScopedValue` (Java 25) is the virtual-thread-friendly way to pass read-only context.

**Next:** [Deadlock, Livelock, Starvation](14_deadlock-livelock-starvation.md)