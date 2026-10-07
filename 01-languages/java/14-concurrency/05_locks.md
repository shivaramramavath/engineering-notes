# Locks

`java.util.concurrent.locks` provides explicit lock objects. They do the same job as `synchronized` (mutual exclusion plus visibility), but as ordinary objects you lock and unlock yourself, which unlocks capabilities `synchronized` lacks: **timeouts, try-lock, interruptible waiting, fairness, multiple wait conditions, and shared (read) vs exclusive (write) locking**.

Rule of thumb: **use `synchronized` by default; reach for a `Lock` when you need one of those capabilities.**

**Prerequisites:** [Synchronization](03_synchronization.md), [Memory Model and `volatile`](04_memory-model-and-volatile.md).

---

## 1. `ReentrantLock`

```java
class Inventory {
    private final ReentrantLock lock = new ReentrantLock();
    private int stock;

    void add(int n) {
        lock.lock();                 // acquire BEFORE the try
        try {
            stock += n;
        } finally {
            lock.unlock();           // ALWAYS release in finally
        }
    }
}
```

The `lock()` / `try { ... } finally { unlock(); }` shape is mandatory. Unlike `synchronized`, an exception won't release the lock for you. Put `lock()` **outside** the `try`, so a failed acquisition doesn't trigger an `unlock()` of a lock you never held.

It's **reentrant** (the same thread can lock again, with a hold count), has the same memory-visibility guarantees as `synchronized` (unlock happens-before the next lock), and is **not** fair by default.

### What you gain over `synchronized`

```java
// 1. Try without blocking
if (lock.tryLock()) {
    try { ... } finally { lock.unlock(); }
} else {
    // do something else, or report "busy"
}

// 2. Try with a timeout
if (lock.tryLock(500, TimeUnit.MILLISECONDS)) {       // throws InterruptedException
    try { ... } finally { lock.unlock(); }
} else {
    throw new TimeoutException("could not acquire lock");
}

// 3. Interruptible acquisition: can be cancelled while waiting
lock.lockInterruptibly();                              // throws InterruptedException

// 4. Fairness: longest-waiting thread gets the lock next (lower throughput)
new ReentrantLock(true);
```

Useful inspection methods: `isLocked()`, `isHeldByCurrentThread()`, `getHoldCount()`, `getQueueLength()`. They are meant for diagnostics and assertions, not control flow.

### Timeouts to avoid deadlock

```java
boolean transfer(Account from, Account to, long amount) throws InterruptedException {
    if (from.lock.tryLock(100, TimeUnit.MILLISECONDS)) {
        try {
            if (to.lock.tryLock(100, TimeUnit.MILLISECONDS)) {
                try {
                    from.debit(amount);
                    to.credit(amount);
                    return true;
                } finally { to.lock.unlock(); }
            }
        } finally { from.lock.unlock(); }
    }
    return false;               // caller can retry with backoff
}
```

If two threads transfer in opposite directions, neither waits forever. Prefer a consistent lock *ordering* when possible ([note 14](14_deadlock-livelock-starvation.md)), and use timeouts as a safety net.

---

## 2. `Condition`: multiple wait sets per lock

`Object.wait/notify` gives each monitor **one** wait set. A `Condition` is created from a `Lock`, and you can have several, each for a different "what I'm waiting for". The bounded buffer from [note 03](03_synchronization.md) improves because producers wait on `notFull` and consumers wait on `notEmpty`, with no wasted wake-ups:

```java
class BoundedBuffer<T> {
    private final Lock lock = new ReentrantLock();
    private final Condition notFull  = lock.newCondition();
    private final Condition notEmpty = lock.newCondition();
    private final Queue<T> items = new ArrayDeque<>();
    private final int capacity;
    BoundedBuffer(int capacity) { this.capacity = capacity; }

    void put(T item) throws InterruptedException {
        lock.lock();
        try {
            while (items.size() == capacity) notFull.await();     // releases the lock while waiting
            items.add(item);
            notEmpty.signal();                                    // wake ONE consumer
        } finally { lock.unlock(); }
    }

    T take() throws InterruptedException {
        lock.lock();
        try {
            while (items.isEmpty()) notEmpty.await();
            T item = items.remove();
            notFull.signal();
            return item;
        } finally { lock.unlock(); }
    }
}
```

The same rules apply as for `wait`: hold the lock when calling `await`/`signal` (else `IllegalMonitorStateException`), and **always `await` in a `while` loop**. `await(timeout, unit)` and `awaitNanos` support timed waits. In real code, a `BlockingQueue` already does all this ([Concurrent Collections](08_concurrent-collections.md)).

---

## 3. `ReadWriteLock`

Many data structures are **read often, written rarely**. A `ReadWriteLock` lets any number of readers hold the lock together, but a writer needs exclusive access.

```java
class Cache<K, V> {
    private final ReentrantReadWriteLock rw = new ReentrantReadWriteLock();
    private final Map<K, V> map = new HashMap<>();

    V get(K key) {
        rw.readLock().lock();
        try { return map.get(key); }
        finally { rw.readLock().unlock(); }
    }

    void put(K key, V value) {
        rw.writeLock().lock();
        try { map.put(key, value); }
        finally { rw.writeLock().unlock(); }
    }
}
```

| Situation | Allowed? |
|---|---|
| Many threads hold the read lock | Yes |
| One thread holds the write lock | Yes, and nobody else (readers included) |
| Holding the write lock and acquiring the read lock (**downgrade**) | Yes: acquire read, then release write |
| Holding the read lock and acquiring the write lock (**upgrade**) | **No: deadlocks.** Release the read lock first, then take the write lock and re-check |

Caveats:

- It pays off only when reads **dominate** and read sections are long enough to benefit. For short critical sections, plain `synchronized` or `ReentrantLock` is often as fast.
- Writers can be **starved** by a continuous stream of readers (or the reverse) depending on fairness settings.
- For a keyed cache, `ConcurrentHashMap` is usually simpler and faster than a `HashMap` under a read-write lock.

---

## 4. `StampedLock`: optimistic reads

`StampedLock` (Java 8) adds an **optimistic read**: read without locking, then check afterwards whether a writer interfered. If not, you paid nothing.

```java
class Point {
    private final StampedLock sl = new StampedLock();
    private double x, y;

    void move(double dx, double dy) {
        long stamp = sl.writeLock();
        try { x += dx; y += dy; }
        finally { sl.unlockWrite(stamp); }
    }

    double distanceFromOrigin() {
        long stamp = sl.tryOptimisticRead();          // no lock taken
        double cx = x, cy = y;                        // read fields into locals
        if (!sl.validate(stamp)) {                    // a write happened meanwhile?
            stamp = sl.readLock();                    // fall back to a real read lock
            try { cx = x; cy = y; }
            finally { sl.unlockRead(stamp); }
        }
        return Math.hypot(cx, cy);
    }
}
```

Strict rules:

- **Not reentrant**: re-acquiring it on the same thread **deadlocks**.
- No `Condition` support, and the stamp (a `long`) must be passed to the matching unlock.
- Copy values into **locals** during an optimistic read and use only those after `validate`; never act on fields read optimistically before validating.
- It's an expert tool. Use it only when profiling shows read-lock overhead matters.

---

## 5. `synchronized` vs `Lock`

| | `synchronized` | `ReentrantLock` |
|---|---|---|
| Release | Automatic at block exit (even on exception) | Manual `unlock()` in `finally` |
| Try / timeout | No | `tryLock()`, `tryLock(time, unit)` |
| Interruptible wait | No | `lockInterruptibly()` |
| Fairness option | No | Yes (costs throughput) |
| Multiple conditions | No (one wait set) | Yes (`newCondition()`) |
| Scope | Lexical (block/method) | Any: acquire in one method, release in another (with care) |
| Risk | Little | **Forgetting `unlock`** |
| Virtual threads | Pinned before Java 24, fine from 24 on | Does not pin |
| Diagnostics | Monitor owner shown in thread dumps | Ownership appears in thread dumps with `jcmd <pid> Thread.print -l` |

When they're equivalent, prefer `synchronized`: shorter and impossible to leave locked. Choose a `Lock` for timeouts, `tryLock`, interruptibility, multiple conditions, or read/write separation.

### Virtual threads

A virtual thread that blocks on a `ReentrantLock` is *unmounted* from its carrier thread, so other virtual threads keep running ([note 13](13_virtual-threads-and-structured-concurrency.md)). `synchronized` had pinning problems until Java 24 (JEP 491), after which the difference is much smaller.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `lock()` without `unlock()` on every path | `lock(); try { ... } finally { unlock(); }` |
| `lock()` inside the `try` | Acquire before `try`, so `finally` only unlocks what you hold |
| Unlocking a lock you don't hold → `IllegalMonitorStateException` | Match lock/unlock pairs on the same thread |
| Calling `await`/`signal` without holding the lock | Hold the associated lock |
| `await()` in an `if` | Loop on the condition |
| Mixing `synchronized` and `Lock` on the same data | One mechanism per piece of state |
| Upgrading read → write lock | Release read, acquire write, re-validate |
| Re-entering a `StampedLock` | It's not reentrant: restructure |
| Using fairness "to be safe" | It reduces throughput. Use only if starvation is a proven problem |
| Doing slow I/O while holding a lock | Narrow the critical section |
| Not using a timeout where a deadlock would be costly | `tryLock` with timeout and a fallback |

### Debugging

- Hung threads: `jcmd <pid> Thread.print` (add `-l` for lock/ownable synchronizer info) shows which thread owns which `ReentrantLock` and who is parked waiting for it. Locks that were never unlocked appear as an owner that is idle or gone.
- `IllegalMonitorStateException` from `unlock`/`await` → you don't hold the lock on this thread, or an earlier path already released it.
- Intermittent stalls under load: log `tryLock` failures and `getQueueLength()` to measure contention.
- A held lock leak is usually a missing `finally`. Search for `lock()` calls without a paired `finally` unlock.

---

## Quick Summary

- `ReentrantLock` = `synchronized` + **`tryLock`, timeouts, interruptible acquire, fairness, multiple `Condition`s**. Always `lock(); try { ... } finally { unlock(); }`.
- `Condition.await/signal` replace `wait/notify` with separate wait sets. Always loop on the predicate.
- `ReentrantReadWriteLock`: many readers or one writer. No read→write upgrade (deadlock). Worth it only for read-heavy workloads.
- `StampedLock`: optimistic reads for performance, but non-reentrant and tricky.
- Default to `synchronized`, or better, higher-level tools (atomics, concurrent collections, queues).

**Next:** [Atomic Classes](06_atomic-classes.md)