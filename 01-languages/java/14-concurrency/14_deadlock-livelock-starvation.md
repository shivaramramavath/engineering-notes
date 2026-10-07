# Deadlock, Livelock, Starvation

These are **liveness failures**: the program is not producing wrong values (a *safety* problem, as in races), it is **failing to make progress**. They tend to appear under load, after a rare interleaving, or only in production, and they freeze systems, so knowing how to recognize and prevent them is essential.

| Problem | Threads are... | CPU usage | Progress |
|---|---|---|---|
| **Deadlock** | Blocked forever, each waiting for another | Idle | None for the involved threads |
| **Livelock** | Running, but keep reacting to each other | **High** | None |
| **Starvation** | Waiting because others always go first | Normal | Some threads progress, one or more never do |

**Prerequisites:** [Synchronization](03_synchronization.md), [Locks](05_locks.md), [Executors and Thread Pools](10_executors-and-thread-pools.md).

---

## 1. Deadlock

A deadlock is a cycle of threads each holding something another needs.

```java
Object lockA = new Object(), lockB = new Object();

// Thread 1                                    // Thread 2
synchronized (lockA) {                          synchronized (lockB) {
    sleep(100);                                     sleep(100);
    synchronized (lockB) { ... }                    synchronized (lockA) { ... }
}                                               }
```

```text
Thread 1 holds A ──wants──► B (held by Thread 2)
Thread 2 holds B ──wants──► A (held by Thread 1)        → neither can ever proceed
```

### The four conditions (all must hold)

| Condition | Meaning | Break it by... |
|---|---|---|
| **Mutual exclusion** | A resource can be held by one thread at a time | Using lock-free/immutable designs where possible |
| **Hold and wait** | A thread holds one resource while waiting for another | Acquiring all locks at once, or never calling out while holding a lock |
| **No preemption** | Locks can't be forcibly taken away | `tryLock` with timeout, then back off and release |
| **Circular wait** | A cycle of waiting threads exists | **A global lock ordering** (the most practical fix) |

### Realistic example: transfers between accounts

```java
// Deadlock-prone: transfer(a, b) and transfer(b, a) run concurrently
void transfer(Account from, Account to, long amount) {
    synchronized (from) {
        synchronized (to) {
            from.debit(amount);
            to.credit(amount);
        }
    }
}
```

The fix is to **always lock in the same global order**, regardless of the parameter order:

```java
void transfer(Account from, Account to, long amount) {
    Account first  = from.id() < to.id() ? from : to;       // a stable, unique ordering key
    Account second = (first == from) ? to : from;

    synchronized (first) {
        synchronized (second) {
            from.debit(amount);
            to.credit(amount);
        }
    }
}
```

If there is no natural unique key, order by `System.identityHashCode`, and add a single tie-breaking lock for the rare equal-hash case.

### Fix with timeouts (break "no preemption")

```java
if (a.lock.tryLock(100, TimeUnit.MILLISECONDS)) {
    try {
        if (b.lock.tryLock(100, TimeUnit.MILLISECONDS)) {
            try { /* work */ return true; }
            finally { b.lock.unlock(); }
        }
    } finally { a.lock.unlock(); }
}
return false;       // couldn't get both: release everything, back off, retry (see livelock below)
```

### Other forms of deadlock

- **Thread-pool starvation deadlock:** tasks that **wait for other tasks in the same pool**.

  ```java
  ExecutorService pool = Executors.newFixedThreadPool(1);
  Future<Integer> outer = pool.submit(() -> {
      Future<Integer> inner = pool.submit(() -> 42);     // queued behind us...
      return inner.get();                                // ...but we hold the only thread: waits forever
  });
  ```

  The same happens with a larger pool when *all* threads wait on subtasks that can't start. Don't block pool tasks on other tasks of the same pool: use separate pools, `CompletableFuture` composition, or virtual threads.
- **Lost signals / missed notifications:** a thread waits for a `notify` that already happened. Always check the condition in a loop under the lock ([note 03](03_synchronization.md)).
- **Latches and barriers:** a thread calls `await()` for something only it can do (or a `countDown()` that was skipped by an exception).
- **Class-initialization deadlocks:** two classes whose static initializers depend on each other, initialized by different threads.
- **Calling "alien" code while holding a lock:** a callback or overridable method takes another lock in the opposite order. *Open calls* (call out only when you hold no locks) avoid this.
- **Database deadlocks:** two transactions locking rows in opposite order. The database detects it and aborts one with an error. Retry the transaction ([Transactions](../16-jdbc-and-databases/04_transactions.md)).

---

## 2. Detecting deadlocks

**Thread dump** (works on a live process):

```bash
jcmd <pid> Thread.print          # or: jstack <pid>
```

The JVM reports Java-level deadlocks it finds, along these lines:

```text
Found one Java-level deadlock:
=============================
"worker-2":
  waiting to lock monitor 0x... (object 0x..., a java.lang.Object),
  which is held by "worker-1"
"worker-1":
  waiting to lock monitor 0x... (object 0x..., a java.lang.Object),
  which is held by "worker-2"
```

Below it you get each thread's stack, showing the exact `synchronized` lines. For `ReentrantLock`-based deadlocks, use `jcmd <pid> Thread.print -l` to include ownable synchronizers.

**Programmatically** (health checks, monitoring):

```java
ThreadMXBean mx = ManagementFactory.getThreadMXBean();
long[] ids = mx.findDeadlockedThreads();         // null if none
if (ids != null) {
    for (ThreadInfo info : mx.getThreadInfo(ids, true, true)) {
        log.error("Deadlocked: {}", info);
    }
}
```

Take **two or three dumps a few seconds apart**: threads stuck in the same place across dumps are stuck, not just busy. Tools such as VisualVM and JFR show the same data graphically ([JVM Diagnostics](../22-production-engineering/04_jvm-diagnostics.md), [Troubleshooting Playbook](../22-production-engineering/05_troubleshooting-playbook.md)).

Pool-starvation deadlocks don't produce a "Found one Java-level deadlock" message. Look instead for all pool workers `WAITING` in `Future.get`/`join` with a non-empty queue.

---

## 3. Livelock

In a livelock, threads aren't blocked: they keep **responding to each other** and never get anywhere, like two people in a corridor repeatedly stepping aside in the same direction.

```java
// Both threads: try to get both locks, and if the second is busy, release the first and retry immediately
while (true) {
    if (a.tryLock()) {
        try {
            if (b.tryLock()) { try { work(); return; } finally { b.unlock(); } }
        } finally { a.unlock(); }
    }
    // retry immediately: the other thread does the same, in lockstep
}
```

Other examples: endless retry collisions on the same schedule, two message handlers repeatedly bouncing a failing message back and forth, optimistic updates (CAS loops) that keep invalidating each other under heavy contention.

**Fixes:**

- **Randomized backoff:** sleep a small random time before retrying, which desynchronizes the threads ([Retry and Backoff](../25-real-world-patterns/02_retry-and-backoff.md)).
- **Arbitration:** give one party priority (lock ordering again).
- **Bounded retries** with a failure path, instead of retrying forever.

Livelock shows up as **high CPU with no useful output**. Thread dumps show threads `RUNNABLE` in the same retry loop.

---

## 4. Starvation

A thread is **starved** when it can't get the resources it needs because other threads keep winning. The system as a whole progresses, but some work never does.

| Cause | Example | Mitigation |
|---|---|---|
| **Unfair locks** | A `synchronized` block or non-fair lock lets other threads "barge in" repeatedly | Fair `ReentrantLock(true)` (costs throughput) when starvation is proven |
| **Greedy threads / long critical sections** | One task holds a lock for seconds | Shorten critical sections; do slow work outside locks |
| **Thread-pool saturation** | A flood of long tasks fills the pool so short, urgent ones wait | Separate pools per workload (bulkhead), bounded queues, priorities in the queue |
| **Reader/writer imbalance** | Constant readers starve a writer (or the reverse) | Fairness mode, or a different structure ([Locks](05_locks.md)) |
| **Priorities** | Low-priority threads rarely scheduled | Don't depend on priorities |
| **Common-pool abuse** | Blocking tasks occupy the shared `ForkJoinPool`, starving everything using it | Dedicated executors ([note 11](11_fork-join-and-parallelism.md)) |

Starvation is hard to spot because there's no error: a request just never completes. Look for **queue wait times growing**, tasks with unbounded latency, and threads that never get scheduled. Metrics on queue length and task wait time reveal it.

---

## 5. Prevention checklist

1. **Avoid holding more than one lock at a time.** Redesign so each operation needs one lock, or use higher-level structures (concurrent collections, atomics, immutable snapshots).
2. If you must nest locks, define and **document a global lock order**, and follow it everywhere.
3. **Never call unknown code** (callbacks, listeners, overridable methods, I/O) while holding a lock. Copy what you need, release, then call out.
4. Use **timeouts** (`tryLock`, `await(timeout)`, `Future.get(timeout)`, `invokeAll` with a timeout) so a stuck wait becomes an error you can see instead of a hang.
5. **Keep critical sections short** and free of blocking operations.
6. **Don't wait on tasks in your own pool.** Use separate pools or non-blocking composition.
7. Always release resources in `finally`: latch `countDown`, semaphore `release`, lock `unlock`.
8. Prefer **message passing / immutable data** and single-writer designs ([Concurrency Patterns](15_concurrency_patterns_placeholder)).
9. **Name threads** and keep thread-dump tooling ready, since you'll need it at 3 a.m.
10. Add a **deadlock check** to health monitoring (`findDeadlockedThreads`) and alert on it.
11. Test under load with many threads, and use tools such as `jcstress` or lock-order analyzers where available.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Acquiring locks in different orders in different methods | One global ordering |
| Calling listeners/callbacks while holding a lock | Open calls: notify after releasing |
| Retrying lock acquisition immediately in a tight loop | Randomized backoff, bounded retries |
| A pool task blocking on `Future.get()` of a sibling task from the same pool | Different pool, or `CompletableFuture`/virtual threads |
| `await()`/`get()`/`take()` with no timeout in production paths | Always use a timeout and handle expiry |
| Skipping `finally` for `countDown`/`release`/`unlock` | Put it in `finally` |
| Assuming priority or fairness will fix starvation | Fix the contention: shorter holds, separate pools |
| Holding a lock across a remote/database call | Don't. Narrow the scope |
| Ignoring a "Found one Java-level deadlock" message in a dump | Fix the lock order that it shows |
| Only testing with one or two threads | Stress with many threads and iterations |

### Diagnosis flow

```text
Application hangs or some requests never finish
   │
   ├─ Take 2-3 thread dumps, a few seconds apart  (jcmd <pid> Thread.print -l)
   │
   ├─ "Found one Java-level deadlock"?                         → lock cycle: fix ordering (section 1)
   ├─ All pool workers WAITING in Future.get / join, queue > 0? → pool starvation deadlock
   ├─ Threads RUNNABLE in the same loop, CPU high?              → livelock / spinning retry
   ├─ Many threads BLOCKED on one monitor?                      → heavy contention / long critical section
   └─ Some tasks waiting a very long time, others fine?         → starvation: look at queues, fairness, pool isolation
```

---

## Quick Summary

- **Deadlock:** a cycle of threads each holding what another needs (mutual exclusion + hold-and-wait + no preemption + circular wait). Fix mainly with **global lock ordering**, fewer nested locks, `tryLock` timeouts, and open calls.
- **Pool-starvation deadlock:** tasks waiting on tasks in the same pool. Isolate pools or avoid blocking waits.
- **Livelock:** busy but not progressing (lockstep retries). Fix with **randomized backoff** and bounded retries.
- **Starvation:** some threads never get resources (unfair locks, long critical sections, saturated or shared pools). Fix with shorter holds, fairness where needed, and **bulkheads**.
- Diagnose with **repeated thread dumps** (`jcmd <pid> Thread.print -l`) and `ThreadMXBean.findDeadlockedThreads()`.
- Use timeouts and `finally` everywhere you wait or acquire.

**Next:** [Concurrency Patterns](15_concurrency-patterns.md)