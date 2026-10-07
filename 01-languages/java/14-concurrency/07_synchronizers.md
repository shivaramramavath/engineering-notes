# Synchronizers

Synchronizers are ready-made **coordination tools**: they let threads wait for each other, limit access to a resource, or move through stages together, without you hand-writing `wait`/`notify`. They live in `java.util.concurrent`, and all of them give proper happens-before guarantees (actions before the "release" are visible after the "acquire").

| Tool | Question it answers |
|---|---|
| `CountDownLatch` | "Wait until N things have happened" (one-shot) |
| `CyclicBarrier` | "Everyone wait here until all N arrive" (reusable) |
| `Semaphore` | "Allow at most N threads in at once" |
| `Phaser` | A flexible, reusable barrier with a changing number of parties |
| `Exchanger` | "Two threads swap objects at a meeting point" (rare) |

**Prerequisites:** [Thread Safety](02_thread-safety.md), [Synchronization](03_synchronization.md), [Memory Model and `volatile`](04_memory-model-and-volatile.md).

---

## 1. `CountDownLatch`: wait for a count to reach zero

Initialize it with a count. Threads call `countDown()` as events happen. Any thread calling `await()` blocks until the count hits zero. It is **one-shot**: once at zero, it stays open forever and can't be reset.

### Pattern A: wait for workers to finish

```java
int workers = 4;
CountDownLatch done = new CountDownLatch(workers);

for (int i = 0; i < workers; i++) {
    pool.execute(() -> {
        try {
            doWork();
        } finally {
            done.countDown();            // ALWAYS in finally
        }
    });
}

done.await();                            // main waits until all 4 have counted down
// or: if (!done.await(30, TimeUnit.SECONDS)) { /* timed out */ }
```

### Pattern B: starting gun (release everyone at once)

```java
CountDownLatch start = new CountDownLatch(1);

for (Runnable task : tasks) {
    new Thread(() -> {
        try { start.await(); task.run(); }               // all threads wait here
        catch (InterruptedException e) { Thread.currentThread().interrupt(); }
    }).start();
}
start.countDown();                                       // release all together (useful in tests)
```

**Pitfall:** if a worker throws before calling `countDown()`, the count never reaches zero and `await()` hangs forever. Use `finally`, and prefer the timed `await`.

---

## 2. `CyclicBarrier`: meet at a point, repeatedly

A barrier for **N parties**. Each calls `await()` and blocks until all N have arrived. Then all are released, and the barrier **resets** for the next round. An optional *barrier action* runs once per round when the last thread arrives.

```java
int parties = 3;
CyclicBarrier barrier = new CyclicBarrier(parties, () -> System.out.println("--- phase complete ---"));

Runnable worker = () -> {
    try {
        for (int phase = 0; phase < 3; phase++) {
            computePhase(phase);
            barrier.await();             // wait for the other two before starting the next phase
        }
    } catch (InterruptedException | BrokenBarrierException e) {
        Thread.currentThread().interrupt();
    }
};
```

Use it for iterative parallel algorithms where every thread must finish step *k* before anyone starts step *k+1* (simulations, matrix computations).

Behavior to know:

- If any waiting thread is **interrupted, times out, or the barrier is reset**, the barrier becomes **broken**, and every other waiting thread gets a `BrokenBarrierException`.
- The number of parties is fixed. If one worker dies, the others wait forever (or until a timeout). Use `await(timeout, unit)`.

### Latch vs barrier

| | `CountDownLatch` | `CyclicBarrier` |
|---|---|---|
| Reusable | **No** (one-shot) | **Yes** |
| Who waits | Waiters don't have to be the ones counting down | The parties themselves wait for each other |
| Count down by | Any thread, any number of times | Each party calls `await` once per round |

---

## 3. `Semaphore`: limit concurrent access

A semaphore holds a number of **permits**. `acquire()` takes one (blocking if none are left), and `release()` returns one. It limits how many threads can use a resource at the same time.

```java
Semaphore permits = new Semaphore(10);       // at most 10 concurrent database calls

String query(String sql) throws InterruptedException {
    permits.acquire();
    try {
        return db.execute(sql);
    } finally {
        permits.release();                   // ALWAYS release in finally
    }
}
```

Useful variants:

```java
permits.tryAcquire();                              // non-blocking: true/false
permits.tryAcquire(200, TimeUnit.MILLISECONDS);    // with timeout, so you can fail fast instead of piling up
permits.acquire(3);                                // multiple permits at once
new Semaphore(10, true);                           // fair (FIFO), with lower throughput
permits.availablePermits();                        // diagnostic only
```

- A semaphore with **1 permit** acts like a mutex, but with no ownership: **any** thread can release it, and it isn't reentrant.
- **A semaphore doesn't track who holds permits.** Releasing more than you acquired silently *increases* the permit count, which is a classic bug (for example releasing in a `finally` when `acquire` itself failed or timed out).

```java
// Correct handling of a timed acquire
if (permits.tryAcquire(200, TimeUnit.MILLISECONDS)) {
    try { doWork(); } finally { permits.release(); }     // release only if acquired
} else {
    rejectRequest();
}
```

### Semaphores and virtual threads

With virtual threads you don't limit concurrency by pool size (you shouldn't pool them). A `Semaphore` is the standard way to cap use of a scarce downstream resource ([note 13](13_virtual-threads-and-structured-concurrency.md)). Also see [Rate Limiting](../25-real-world-patterns/03_rate-limiting.md) for time-based limits.

---

## 4. `Phaser`: a flexible barrier

`Phaser` generalizes the barrier: it supports **phases**, parties that **register and deregister dynamically**, and one-sided notifications. Use it when parties join and leave, or when you need a barrier plus latch semantics.

```java
Phaser phaser = new Phaser(1);                  // register the main thread as a party

for (Runnable task : tasks) {
    phaser.register();                          // add a party dynamically
    new Thread(() -> {
        task.run();
        phaser.arriveAndDeregister();           // done: leave without waiting
    }).start();
}

phaser.arriveAndAwaitAdvance();                 // main waits for the current phase to complete
phaser.arriveAndDeregister();
```

It's more powerful and more complex than the others, and most code doesn't need it. Reach for it when `CyclicBarrier`'s fixed party count is too limiting.

---

## 5. `Exchanger` (rare)

Two threads meet and **swap** objects: a producer hands a full buffer and receives an empty one, and a consumer does the opposite. It's a niche tool for pipeline designs; most code uses a `BlockingQueue` ([Concurrent Collections](08_concurrent-collections.md)).

---

## 6. Choosing the right tool

| Scenario | Use |
|---|---|
| Main thread waits for N background tasks | `CountDownLatch`, or better, `ExecutorService.invokeAll`, `CompletableFuture.allOf` ([note 12](12_completablefuture.md)), or a structured scope ([note 13](13_virtual-threads-and-structured-concurrency.md)) |
| Release many threads simultaneously (tests) | `CountDownLatch(1)` |
| Iterative algorithm, step-by-step sync | `CyclicBarrier` / `Phaser` |
| Limit concurrent access to a resource | `Semaphore` |
| Hand data between threads | `BlockingQueue` |
| Dynamic participants and phases | `Phaser` |

Often a higher-level abstraction (`Future`, `CompletableFuture`, executors) is clearer than a latch. Use synchronizers when you really are coordinating *threads* rather than *results*.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `countDown()` not in `finally` → `await()` hangs | `finally { latch.countDown(); }` |
| Reusing a `CountDownLatch` | It's one-shot: create a new one, or use `CyclicBarrier`/`Phaser` |
| `await()` with no timeout in production code | Use the timed version and handle expiry |
| Wrong party count in `CyclicBarrier` | Match the number of threads exactly, and handle thread failure |
| Ignoring `BrokenBarrierException` | Treat it as "this round failed" and stop or retry |
| `release()` without a matching successful `acquire()` | Release only what you acquired (`tryAcquire` result checked) |
| Using a semaphore as a mutex but expecting ownership/reentrancy checks | Use a `Lock` |
| Holding a permit/latch wait while blocking on something that needs the same pool | Pool starvation ([note 14](14_deadlock-livelock-starvation.md)) |
| Calling `await()` from the single thread that must `countDown()` | Deadlock by design: a thread can't wait for itself |

### Debugging

- A program that hangs at a `CountDownLatch.await()`: check that `countDown()` is called the expected number of times, including on failure paths. A thread dump shows the waiter in `WAITING` inside `CountDownLatch.await`.
- `BrokenBarrierException` → look for the thread that was interrupted, timed out, or failed.
- Semaphore permit counts creeping upward → unmatched `release()` calls. Log `availablePermits()` for diagnostics.
- Tests that "need a sleep to work" often want a latch: replace `Thread.sleep` with a latch plus a timed `await`.

---

## Quick Summary

- **`CountDownLatch`**: one-shot "wait until the count reaches zero" (workers done, or a starting gun).
- **`CyclicBarrier`**: reusable meeting point for a fixed number of threads (phased algorithms).
- **`Semaphore`**: cap concurrent users of a resource with permits. No ownership, so release exactly what you acquired.
- **`Phaser`**: barrier with dynamic parties. `Exchanger`: swap between two threads.
- Always use timeouts in production, and put `countDown`/`release` in `finally`.
- Prefer higher-level tools (`Future`, `CompletableFuture`, executors, structured scopes) when you only need to wait for results.

**Next:** [Concurrent Collections](08_concurrent-collections.md)