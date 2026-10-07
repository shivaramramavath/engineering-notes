# Synchronization

`synchronized` is Java's built-in locking. It gives you two guarantees at once:

1. **Mutual exclusion**: only one thread at a time can execute code guarded by the same lock.
2. **Visibility**: everything a thread wrote before releasing the lock is visible to the next thread that acquires it ([note 04](04_memory-model-and-volatile.md)).

Together they fix the lost-update bug from [Thread Safety](02_thread-safety.md) and make multi-field invariants safe.

**Prerequisites:** [Thread Safety](02_thread-safety.md), [Threads and Lifecycle](00_threads-and-lifecycle.md).

---

## 1. Syntax

```java
class Counter {
    private int count = 0;

    public synchronized void increment() { count++; }      // synchronized method: locks `this`
    public synchronized int get()        { return count; }

    private final Object lock = new Object();
    private int other;

    void update() {
        synchronized (lock) {                               // synchronized block: locks `lock`
            other++;
        }
    }

    static int total;
    static synchronized void addTotal(int n) { total += n; }   // static: locks the Class object (Counter.class)
}
```

Every Java object has an **intrinsic lock** (a *monitor*). A thread entering a `synchronized` region acquires that object's monitor, and releases it when leaving (normally, or by an exception). If another thread holds the monitor, the entering thread goes to state `BLOCKED` until it's released.

| Form | Lock acquired |
|---|---|
| `synchronized` instance method | `this` |
| `synchronized` static method | the `Class` object (`Counter.class`) |
| `synchronized (obj) { }` | `obj` |

**Locks are per object.** Two threads synchronizing on *different* objects don't exclude each other. A static `synchronized` method and an instance `synchronized` method use different locks, so they don't exclude each other either.

### Reentrancy

A thread that already holds a monitor can enter another `synchronized` region on the **same** monitor without blocking itself. That makes it safe for one synchronized method to call another:

```java
synchronized void a() { b(); }      // holds `this`
synchronized void b() { ... }       // re-enters `this`: fine
```

---

## 2. Choosing the lock

Prefer a **private final lock object** over `this`:

```java
class Account {
    private final Object lock = new Object();    // nobody outside can synchronize on it
    private long balance;

    void deposit(long amt) { synchronized (lock) { balance += amt; } }
}
```

Why not `synchronized (this)` / `synchronized` methods? Because `this` is visible to everyone: outside code can also lock on it, interfering with your locking policy (or deadlocking you). It's acceptable for small internal classes, but a private lock is more robust.

**Never synchronize on:**

| Bad lock | Problem |
|---|---|
| A `String` literal or interned string | Shared JVM-wide; unrelated code may lock the same object |
| A boxed `Integer`/`Long` | Cached and shared; `count++` also *replaces* the object, changing the lock |
| A field that gets reassigned (`synchronized (list)` then `list = new ...`) | Different threads lock different objects. Make the lock `final` |
| `Thread.class`, `Class` objects you don't own | Shared widely, causing contention |
| A `Lock` object (`ReentrantLock`) | Mixing two locking mechanisms on one object confuses everyone |

---

## 3. What to guard, and how much

- **Every access** (read *and* write) to a shared variable must use the **same lock**. Synchronizing only writers leaves readers seeing stale values.
- Guard **invariants**, not variables. If `lo <= hi` must always hold, both fields are guarded by the same lock and changed together in one critical section.
- **Keep critical sections short.** Do slow work (I/O, computation) outside the lock and lock only for the shared-state update.
- **Don't call unknown code while holding a lock**: callbacks, listeners, overridable methods. They may block, take other locks, or call back into you (deadlock risk, [note 14](14_deadlock-livelock-starvation.md)).
- **Don't block or do I/O** inside a synchronized region unless unavoidable.

```java
// Compute outside the lock, update inside
ExpensiveResult r = compute(input);
synchronized (lock) { cache.put(key, r); }
```

### Compound actions

```java
// Even with a synchronized collection, this check-then-act is racy:
List<String> list = Collections.synchronizedList(new ArrayList<>());
if (!list.contains(x)) list.add(x);

// Hold one lock around the whole sequence
synchronized (list) {
    if (!list.contains(x)) list.add(x);
}
```

Iterating a `Collections.synchronizedX` collection requires the same: `synchronized (list) { for (var s : list) ... }`.

---

## 4. `wait`, `notify`, `notifyAll`: waiting for a condition

Sometimes a thread must **wait until some state changes** (a queue is non-empty, a result is ready). Every object's monitor supports this:

| Method | Effect |
|---|---|
| `wait()` / `wait(ms)` | **Releases the monitor** and waits until notified (or the timeout) |
| `notify()` | Wakes **one** waiting thread |
| `notifyAll()` | Wakes **all** waiting threads |

Rules:

- You must **hold the monitor** of the object you call these on, or you get `IllegalMonitorStateException`.
- After waking, the thread must **re-acquire the monitor** before continuing.
- **Always call `wait()` inside a `while` loop that rechecks the condition.** Threads can wake up spuriously, and by the time you get the lock again another thread may have changed the state.

### Example: a bounded buffer

```java
class BoundedBuffer<T> {
    private final Queue<T> items = new ArrayDeque<>();
    private final int capacity;
    BoundedBuffer(int capacity) { this.capacity = capacity; }

    public synchronized void put(T item) throws InterruptedException {
        while (items.size() == capacity) {      // condition: not full
            wait();                              // releases the lock while waiting
        }
        items.add(item);
        notifyAll();                             // state changed: wake consumers (and producers)
    }

    public synchronized T take() throws InterruptedException {
        while (items.isEmpty()) {                // condition: not empty
            wait();
        }
        T item = items.remove();
        notifyAll();
        return item;
    }
}
```

`notify()` vs `notifyAll()`: `notify()` wakes an arbitrary single waiter, which might be waiting on a *different* condition than the one you just changed, so a wake-up is "wasted" and others sleep forever. When producers and consumers share one monitor (as above), use `notifyAll()`. With separate conditions, use [`Condition`](05_locks.md) objects instead.

**In new code, don't write this by hand.** `ArrayBlockingQueue` and `LinkedBlockingQueue` do exactly this correctly ([Concurrent Collections](08_concurrent-collections.md)). Wait/notify is worth understanding because it's the foundation and shows up in interviews and old code.

---

## 5. Cost, limitations, and modern notes

- **Uncontended** `synchronized` is cheap in modern JVMs. The cost shows up under **contention**: threads blocking and being rescheduled.
- No timeout, no interruptible acquisition, no "try lock": a thread blocked on `synchronized` can't be interrupted ([`Lock`](05_locks.md) offers `tryLock` and `lockInterruptibly`).
- Not fair: no guarantee about which waiting thread gets the monitor next.
- **Virtual threads:** before Java 24, a virtual thread blocking *inside* a `synchronized` block was **pinned** to its carrier thread, hurting scalability. Java 24 (JEP 491) removed that limitation for `synchronized`. See [note 13](13_virtual-threads-and-structured-concurrency.md).
- Scope is lexical (block or method): you can't acquire in one method and release in another (unlike `Lock`).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Synchronizing on different objects and expecting mutual exclusion | Everyone uses the same lock for the same data |
| Synchronizing only the writer | Reads must use the lock too |
| `wait()` in an `if` instead of a `while` | Always loop on the condition |
| `notify()` when multiple conditions share the monitor | `notifyAll()` or separate `Condition`s |
| Calling `wait`/`notify` without holding the monitor | `IllegalMonitorStateException`: put it inside `synchronized` |
| Locking on strings, boxed numbers, or a reassignable field | Private `final Object lock` |
| Long-running work or I/O inside the critical section | Narrow the lock scope |
| Calling callbacks/overridable methods while holding the lock | Copy state out, release, then call |
| Inconsistent lock ordering across methods | Define a global order ([note 14](14_deadlock-livelock-starvation.md)) |
| Double-checked locking without `volatile` | Add `volatile`, or use the holder idiom |

### Debugging

- Thread dump: threads in `BLOCKED` state show `waiting to lock <0x...>` and which thread `locked <0x...>`. `jcmd <pid> Thread.print` also reports deadlocks directly ([note 14](14_deadlock-livelock-starvation.md)).
- Lost wake-ups (a thread `WAITING` forever in `Object.wait`): the notify happened before the wait, or `notify` woke the wrong thread. Check that the condition is tested in a loop under the lock and that every state change notifies.
- Throughput collapses with more threads: contention. Shorten critical sections, shard the data, or use concurrent collections/atomics ([Concurrency Performance](../20-performance/04_concurrency-performance.md)).

---

## Quick Summary

- `synchronized` = **mutual exclusion + visibility**, tied to an object's monitor. Methods lock `this` (or the `Class` for static); blocks lock the object you name.
- Use the **same lock for all access** to a shared state, protect **invariants**, keep sections **short**, and prefer a **private final lock object**.
- Locks are reentrant. Never lock on strings, boxed values, or reassignable fields.
- `wait` / `notify` / `notifyAll` coordinate on a condition: hold the monitor, and **always `wait` in a `while` loop**.
- Prefer `BlockingQueue` and friends over hand-written wait/notify.
- `synchronized` can't time out or be interrupted. Use [`Lock`](05_locks.md) when you need that.

**Next:** [Memory Model and `volatile`](04_memory-model-and-volatile.md)