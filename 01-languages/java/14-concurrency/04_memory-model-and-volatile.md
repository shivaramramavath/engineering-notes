# Memory Model and `volatile`

Even if no two threads ever run the same line at the same instant, a thread may **not see what another thread wrote**, or may see writes **in a different order than the code was written**. Modern CPUs keep values in registers and per-core caches, and both the compiler (JIT) and hardware reorder instructions for speed. In single-threaded code you can't tell. Across threads you can.

The **Java Memory Model (JMM)** is the contract that says exactly when one thread is *guaranteed* to see another thread's writes. If you follow its rules, your code is correct on every JVM and CPU. If you don't, it may "work" on your laptop and fail in production.

**Prerequisites:** [Thread Safety](02_thread-safety.md), [Synchronization](03_synchronization.md).

---

## 1. The visibility problem

```java
class Worker implements Runnable {
    private boolean running = true;               // NOT volatile

    public void run() {
        while (running) { /* do work */ }         // may never see running == false
        System.out.println("stopped");
    }
    void stop() { running = false; }              // called from another thread
}
```

This loop can run **forever**. Nothing tells the JIT that another thread might change `running`, so it is allowed to read the field once and reuse the value (effectively `while (true)`). It may work in a debugger or with `-Xint`, and spin forever in production.

### Reordering can expose half-built objects

```java
class Holder { int x; Holder() { x = 42; } }

static Holder holder;                              // plain field

// Thread A                          // Thread B
holder = new Holder();               if (holder != null) System.out.println(holder.x);   // may print 0!
```

Creating the object involves allocating it, running the constructor (`x = 42`), and storing the reference. Without synchronization, the reference store may become visible to Thread B **before** the field write. B sees a non-null reference to an incompletely initialized object.

Neither bug is a "timing" problem you can fix with `sleep`. They are *missing happens-before relationships*.

---

## 2. Happens-before

The JMM's core idea: if action A **happens-before** action B, then everything A did (including all writes before it) is **visible** to B, and A is ordered before B. If there's no happens-before edge between two actions on different threads, the JMM guarantees *nothing* about what one sees of the other.

The edges you can rely on:

| Rule | Happens-before edge |
|---|---|
| **Program order** | Each action in a thread happens-before later actions in that same thread |
| **Monitor lock** | `unlock` of a monitor → every later `lock` of the **same** monitor (`synchronized`, `Lock.unlock()` → `lock()`) |
| **`volatile`** | A write to a volatile field → every later read of that **same** field |
| **Thread start** | `thread.start()` → everything in the started thread |
| **Thread termination** | Everything in a thread → another thread's successful `join()` (or `isAlive()` returning `false`) |
| **Interruption** | `thread.interrupt()` → the interrupted thread detecting it |
| **Transitivity** | If A → B and B → C then A → C |

The `java.util.concurrent` classes provide edges too, which is a large part of why they are safe to use:

- submitting a task to an `Executor` → the task's execution; the task's actions → `Future.get()` returning
- `BlockingQueue.put` → the matching `take`
- `CountDownLatch.countDown()` → the corresponding `await()` return
- writes to a `ConcurrentHashMap` → later reads of that key
- `Lock.unlock()` → later `lock()` of the same lock

### Seeing it in practice

```java
int data;                  // plain
volatile boolean ready;    // volatile

// Thread A                         // Thread B
data = 42;                          while (!ready) { }        // spin until true
ready = true;   // volatile write    System.out.println(data); // guaranteed 42
```

`data = 42` happens-before `ready = true` (program order); the volatile write happens-before the volatile read that sees `true`; and the read happens-before `println` (program order). By transitivity, the write of `data` is visible, even though `data` itself isn't volatile.

---

## 3. `volatile`

Declaring a field `volatile` gives you two things:

1. **Visibility:** a read always sees the most recent write to that variable by any thread.
2. **Ordering:** reads and writes of other variables can't be reordered across the volatile access in ways that break the happens-before rule above.

Fixing the earlier stop flag:

```java
private volatile boolean running = true;
```

### What `volatile` does **not** do: atomicity

```java
private volatile int count;
void increment() { count++; }       // STILL BROKEN: read, add, write is three steps
```

`volatile` makes each individual read or write visible but does not make *sequences* atomic. For `count++` use `AtomicInteger` ([note 06](06_atomic-classes.md)) or `synchronized`. A rule of thumb:

> `volatile` is enough when **one thread writes** (or writes are independent of the current value) and **others only read**, and when the new value doesn't depend on the old one or on other variables.

Other facts:

- Reads and writes of a `volatile long`/`double` are atomic. For ordinary (non-volatile) `long`/`double` fields the JLS allows a 64-bit write to be performed as two 32-bit writes, so a reader could see a torn value. (64-bit JVMs in practice don't tear, but the guarantee requires `volatile`.)
- A `volatile` reference to an array or object makes the **reference** volatile, not the elements or fields it points to.
- Volatile accesses are cheap compared with locks, but they limit optimizations, so don't sprinkle them everywhere.

### Good uses

| Use | Example |
|---|---|
| Status/stop flags | `volatile boolean shutdown` |
| Publishing an **immutable** object (safe publication) | `volatile Config config;` replaced by one writer, read by many |
| "Latest value" holders | A single writer, many readers |
| Double-checked locking | See below |
| Cheap read-mostly state with rare updates | Snapshot swapped atomically |

```java
// One writer swaps an immutable snapshot; readers always see a complete, consistent Config
private volatile Config config = Config.load();

void reload() { config = Config.load(); }          // new immutable object, one reference write
String host() { return config.host(); }            // read the reference once, use it consistently
```

---

## 4. Safe publication and `final`

Safe ways to hand an object to another thread: `volatile`/`AtomicReference`, a lock, a static initializer, a concurrent collection, or a **`final` field**.

`final` fields get a special guarantee: once the constructor completes, any thread that obtains a reference to the object sees the **final fields correctly initialized** (and objects reachable through them), even without other synchronization. This is why immutable objects are so safe to share.

```java
class Holder { final int x; Holder() { x = 42; } }   // B can never see x == 0 (provided `this` didn't escape)
```

It only holds if `this` doesn't escape the constructor ([Thread Safety](02_thread-safety.md#3-safe-publication)).

---

## 5. Double-checked locking

Lazy initialization with cheap reads after the first call. It's correct **only with `volatile`**:

```java
class Lazy {
    private static volatile Heavy instance;           // volatile is essential

    static Heavy get() {
        Heavy local = instance;                       // one volatile read in the fast path
        if (local == null) {
            synchronized (Lazy.class) {
                local = instance;
                if (local == null) {
                    instance = local = new Heavy();
                }
            }
        }
        return local;
    }
}
```

Without `volatile`, another thread may see a non-null `instance` whose constructor effects aren't yet visible (the half-built object problem). In most cases the simpler **initialization-on-demand holder** idiom ([Thread Safety](02_thread-safety.md#lazy-initialization)) or an `enum` singleton is better ([Singleton](../24-design-patterns/01-creational/00_singleton.md)).

---

## 6. Beyond `volatile`: finer-grained access (advanced)

`VarHandle` (and the atomic classes built on it) expose weaker, cheaper access modes: *plain*, *opaque*, *acquire/release*, and *volatile*, plus compare-and-set operations. They matter for building lock-free data structures and are rarely needed in application code. Use `volatile`, atomics, and locks first.

---

## 7. Misconceptions

| Belief | Reality |
|---|---|
| "`volatile` makes my class thread-safe" | Only visibility/ordering of that variable. Compound actions still race |
| "`synchronized` is only for mutual exclusion" | It also provides visibility. A lock-free read of data written under a lock is **not** safe |
| "It works on my machine, so it's fine" | x86 CPUs hide many reordering bugs that ARM and others expose. The JIT's optimizations also change with load and warm-up |
| "The write happens instantly everywhere" | Without a happens-before edge there's no guarantee it ever becomes visible |
| "Adding `sleep` or `System.out.println` fixed it" | It changed timing (and println synchronizes). It did not fix the bug |
| "Only multi-core machines have these issues" | Compiler reordering and register caching can occur on a single core |
| "Memory model = CPU caches" | Caches are one cause. The JMM is an abstract contract that covers JIT and hardware together |

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Stop flag that isn't `volatile` | `volatile boolean` (or use `interrupt()`) |
| `volatile int count; count++` | `AtomicInteger` or a lock |
| Reading shared data without the lock that writers use | Use the same lock, or `volatile`/atomics |
| Double-checked locking without `volatile` | Add `volatile`, or use the holder idiom |
| Publishing a mutable object via a volatile field and then mutating it | Publish immutable objects, or guard mutation |
| Assuming `volatile` on a collection makes its contents safe | It only affects the reference |
| Making everything `volatile` "just in case" | Understand what needs to be shared and why |
| Leaking `this` from constructors | Static factory + safe publication |

### Debugging

- Symptoms of a visibility bug: a loop that never ends, stale values that eventually (or never) update, `null`/zero fields on "constructed" objects. They often appear only after JIT warm-up or under production load.
- Questions to ask: *What happens-before edge links the write to this read?* If you can't name one (lock, volatile, `join`, executor submit, concurrent collection), there isn't one.
- Reproduction aids: run with more threads and iterations, test on different CPU architectures, and use `jcstress` for concurrency stress testing.

---

## Quick Summary

- Without synchronization, threads may see **stale values** and **reordered writes**. The JMM defines when visibility is guaranteed via **happens-before**.
- Edges come from: **program order, monitor lock/unlock, volatile write/read, `Thread.start`/`join`**, and `java.util.concurrent` operations (executor submit, `Future.get`, queues, latches, concurrent maps).
- `volatile` = visibility + ordering for one variable. It is **not atomic** for `x++`.
- `final` fields are safe to read after construction (if `this` doesn't escape): the basis of immutable-object safety.
- Double-checked locking works only with `volatile`. Simpler alternatives exist.
- "It works on my machine" proves nothing about concurrency.

**Next:** [Locks](05_locks.md)