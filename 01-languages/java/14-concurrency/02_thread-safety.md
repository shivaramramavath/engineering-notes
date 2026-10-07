# Thread Safety

A class is **thread-safe** if it behaves correctly when accessed from multiple threads at the same time, **without the callers having to add their own synchronization**. "Correctly" means it keeps its documented invariants no matter how the threads interleave.

Thread safety is not about threads you create. It's about **whether your object could ever be reached by more than one thread**: a Spring singleton, a static field, a cache, anything captured by a lambda running in a pool.

**Prerequisites:** [Threads and Lifecycle](00_threads-and-lifecycle.md), [Immutability](../23-design-and-clean-code/04_immutability.md).

---

## 1. The problem: a race condition you can reproduce

```java
class Counter {
    private int count = 0;
    void increment() { count++; }
    int get() { return count; }
}
```

```java
Counter c = new Counter();
Runnable r = () -> { for (int i = 0; i < 100_000; i++) c.increment(); };
Thread a = new Thread(r), b = new Thread(r);
a.start(); b.start(); a.join(); b.join();
System.out.println(c.get());        // expected 200000; usually prints less
```

`count++` looks like one operation but is three: **read** the value, **add** one, **write** it back. Two threads can read the same value and both write back the same result, so one increment is **lost**.

```text
Thread A: read 5 ─────────── add → 6 ─ write 6
Thread B:          read 5 ─── add → 6 ─ write 6      ← two increments, count went 5 → 6
```

This is a **race condition**: the result depends on timing. The three fundamental hazards are listed in the [module README](README.md): atomicity, visibility ([note 04](04_memory-model-and-volatile.md)), and ordering.

### The two classic race patterns

| Pattern | Example | Why it fails |
|---|---|---|
| **Read-modify-write** | `count++`, `total += x` | Another thread changes the value between your read and your write |
| **Check-then-act** | `if (!map.containsKey(k)) map.put(k, v)`; lazy init `if (inst == null) inst = new X()` | The condition is stale by the time you act on it |

Even with a **thread-safe collection**, the *combination* of two calls is not atomic:

```java
ConcurrentHashMap<String, Integer> map = ...;
if (!map.containsKey(k)) map.put(k, 1);        // still a race
map.putIfAbsent(k, 1);                         // one atomic operation: correct
map.merge(k, 1, Integer::sum);                 // atomic increment-or-insert
```

---

## 2. Four ways to be thread-safe

Order matters: **earlier is better**, because the first two avoid the problem rather than manage it.

```text
1. Don't share      → each thread has its own data       (confinement)
2. Don't mutate     → shared but immutable               (immutability)
3. Use atomic tools → single-variable atomics, concurrent collections
4. Synchronize      → locks guard compound operations
```

### 2.1 Confinement: don't share

- **Stack confinement:** local variables and parameters live on the thread's own stack, so they're inherently safe (as long as the *objects* they refer to aren't published elsewhere).
- **Thread confinement:** an object is only ever used by one thread (for example, handed off via a queue and then never touched by the sender), or lives in a `ThreadLocal` ([note 09](09_thread-local.md)).
- **Single-writer principle:** let one thread own all mutations of a piece of state, and have others send it messages ([Concurrency Patterns](15_concurrency-patterns.md)).

### 2.2 Immutability: don't mutate

An object whose state can never change after construction is automatically thread-safe: there is nothing to race on.

```java
public final class Money {
    private final BigDecimal amount;      // final fields + no setters + final class
    private final String currency;
    public Money(BigDecimal amount, String currency) { this.amount = amount; this.currency = currency; }
    public Money plus(Money other) { return new Money(amount.add(other.amount), currency); }   // returns a new object
}
```

Use records ([Records](../12-modern-java/01_records.md)), `String`, `java.time` types ([Date and Time](../10-date-and-time/00_java-time-overview.md)), `List.of(...)`, and `Collections.unmodifiableX` over a private copy. Remember that immutability must be **deep**: a final field holding a mutable `List` is not immutable.

`final` fields also get a special guarantee: once the constructor finishes, any thread that sees the object sees the correctly initialized final fields ([note 04](04_memory-model-and-volatile.md)).

### 2.3 Atomic classes and concurrent collections

For a single counter or flag, `AtomicInteger` ([note 06](06_atomic-classes.md)). For shared maps and queues, `ConcurrentHashMap` and `BlockingQueue` ([note 08](08_concurrent-collections.md)).

### 2.4 Synchronization

When several fields must change together (an **invariant** such as `balance >= 0` or `min <= max`), guard *all* access to those fields with one lock ([note 03](03_synchronization.md), [note 05](05_locks.md)):

```java
class Counter {
    private int count = 0;
    synchronized void increment() { count++; }
    synchronized int get() { return count; }       // reads need the lock too (visibility)
}
```

---

## 3. Safe publication

Creating a thread-safe object isn't enough. You must also make it **visible to other threads safely**. Handing a reference to another thread through an ordinary field can expose it **before it is fully constructed** (see [note 04](04_memory-model-and-volatile.md)).

Safe ways to publish an object:

- initialize it in a **static initializer** (`static final Foo FOO = new Foo();`),
- store it in a **`volatile`** field or an **`AtomicReference`**,
- store it in a **`final`** field of a properly constructed object,
- guard it with a **lock** (write and read under the same lock),
- put it into a **concurrent collection** or `BlockingQueue`, or hand it over via `Executor.submit`, `Thread.start`, or `Future.get`.

### Don't let `this` escape the constructor

```java
class Listener {
    Listener(EventSource source) {
        source.register(e -> handle(e));    // `this` is visible to another thread before the constructor ends
        this.state = initialState();        // ← other thread may run handle() before this line
    }
}
```

Never start threads or register callbacks from inside a constructor. Use a static factory method that constructs first, then publishes.

### Lazy initialization

```java
// Racy: two threads can both see null and construct two instances
if (instance == null) instance = new Heavy();

// Simple and safe: initialization-on-demand holder (the class loader guarantees thread-safe init)
class HeavyHolder {
    private static class Holder { static final Heavy INSTANCE = new Heavy(); }
    static Heavy get() { return Holder.INSTANCE; }
}
```

More in [Singleton](../24-design-patterns/01-creational/00_singleton.md) and [Concurrency Patterns](15_concurrency-patterns.md).

---

## 4. Which JDK classes are thread-safe?

| Thread-safe | **Not** thread-safe |
|---|---|
| `String`, wrapper types, `java.time` types, `BigDecimal` (immutable) | `ArrayList`, `HashMap`, `HashSet`, `LinkedList`, `ArrayDeque` |
| `ConcurrentHashMap`, `CopyOnWriteArrayList`, `BlockingQueue`s | `StringBuilder` (use it per-thread) |
| `AtomicInteger`, `LongAdder`, ... | `SimpleDateFormat`, `Calendar` |
| `DateTimeFormatter`, `Pattern` | `Matcher`, `Scanner`, most `Iterator`s |
| `StringBuffer`, `Vector`, `Hashtable` (legacy: each call synchronized, compound operations still racy) | |
| `Random` (safe, but contended: prefer `ThreadLocalRandom`) | |

"Thread-safe" never means "any sequence of calls is atomic". `Collections.synchronizedList` guards each call, but iterating it or doing check-then-act still needs your own lock around the whole sequence.

Don't use a plain `HashMap` from multiple threads, even "mostly reads": concurrent modification can corrupt it (lost entries, and in older JDKs even infinite loops during resize).

---

## 5. Designing for thread safety

- **Prefer immutable objects and stateless services.** A Spring `@Service` with no mutable fields is thread-safe by construction.
- **Keep mutable state private** and expose it only through methods that enforce your locking policy.
- **Document the policy.** State in Javadoc whether a class is thread-safe, immutable, or not thread-safe, and which lock guards which field.
- **Guard all access** to a shared variable with the *same* lock, including reads.
- **Hold locks for as short a time as possible**, and don't call unknown code (callbacks, overridable methods) while holding one ([note 14](14_deadlock-livelock-starvation.md)).
- **Don't expose internal mutable state**: return copies or unmodifiable views.
- **Prefer higher-level tools** over wait/notify and hand-rolled locking.

### Testing is hard

Concurrency bugs are timing-dependent, so a passing test proves little. Helpful approaches: stress tests with many threads and iterations, assertions on invariants after the run, `jcstress` (the OpenJDK concurrency stress tester), and static analysis. Design to *make* correctness obvious rather than testing it in ([Testing Patterns](../19-testing/04_testing-patterns.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `count++` on a shared field | `AtomicInteger`/`LongAdder`, or `synchronized` |
| Check-then-act on a thread-safe collection | Use the atomic method (`putIfAbsent`, `computeIfAbsent`, `merge`) |
| Synchronizing writes but not reads | Same lock for all access |
| Using a non-thread-safe class (`SimpleDateFormat`, `HashMap`) from many threads | `DateTimeFormatter`, `ConcurrentHashMap`, or confine it |
| "It's final so it's immutable" | Final reference ≠ immutable object. Make contents immutable too |
| Publishing an object via a plain non-volatile field | `volatile`, a lock, a final field, or a concurrent structure |
| Leaking `this` from a constructor | Static factory + publish afterwards |
| Using synchronized wrapper then iterating | Lock around the iteration, or use a concurrent collection |
| Assuming a test that passes 1000 times proves correctness | Reason about the invariants and happens-before |
| Hidden shared state in singletons/statics/caches | Review every static and every singleton field |

### Debugging

- Symptoms: occasional wrong totals, `ConcurrentModificationException`, `NullPointerException` on "impossible" nulls, results that change between runs, bugs that disappear under a debugger or when logging is added (timing changes).
- Ask of every mutable field: *which threads can touch it, and what guards it?* If you can't answer in a sentence, that's the bug.
- Reproduce by increasing contention: more threads, more iterations, `Thread.yield()` or small sleeps between the racy steps (in a test only).
- Thread dumps and JFR show contention ([JVM Diagnostics](../22-production-engineering/04_jvm-diagnostics.md)).

---

## Quick Summary

- A race condition happens when threads interleave **read-modify-write** or **check-then-act** steps on shared mutable state.
- Make code thread-safe by (in order of preference): **not sharing**, **not mutating**, using **atomics / concurrent collections**, or **synchronizing** compound operations.
- Immutable objects are inherently safe, but the immutability must be deep.
- **Publish safely** (static init, `volatile`, `final`, lock, concurrent collection) and never let `this` escape a constructor.
- Thread-safe components don't make *sequences* of calls atomic.
- Document the thread-safety policy of every class that shares state.

**Next:** [Synchronization](03_synchronization.md)