# Atomic Classes

The classes in `java.util.concurrent.atomic` give you **thread-safe operations on a single variable without locks**. They fix the `count++` lost-update bug ([Thread Safety](02_thread-safety.md)) and also provide `volatile` visibility ([note 04](04_memory-model-and-volatile.md)).

```java
AtomicInteger count = new AtomicInteger();
count.incrementAndGet();          // atomic read-modify-write: no lock, no lost updates
```

They're built on a CPU instruction called **compare-and-swap (CAS)**, which is why they are fast under low-to-moderate contention and can never deadlock.

**Prerequisites:** [Thread Safety](02_thread-safety.md), [Memory Model and `volatile`](04_memory-model-and-volatile.md).

---

## 1. The core classes

| Class | Holds |
|---|---|
| `AtomicInteger`, `AtomicLong` | An `int` / `long` |
| `AtomicBoolean` | A `boolean` |
| `AtomicReference<V>` | An object reference |
| `AtomicIntegerArray`, `AtomicLongArray`, `AtomicReferenceArray` | Arrays whose **elements** are atomic |
| `AtomicStampedReference`, `AtomicMarkableReference` | A reference plus a version stamp / mark bit |
| `LongAdder`, `DoubleAdder`, `LongAccumulator`, `DoubleAccumulator` | High-contention counters and aggregates |

### `AtomicInteger` / `AtomicLong` operations

```java
AtomicInteger n = new AtomicInteger(10);

n.get();                         // 10
n.set(20);
n.incrementAndGet();             // 21, returns the NEW value
n.getAndIncrement();             // returns 21, new value is 22  (the OLD value)
n.addAndGet(5);                  // 27
n.compareAndSet(27, 100);        // true: was 27, set to 100. false (and no change) if it wasn't 27
n.updateAndGet(x -> x * 2);      // 200  (function applied atomically)
n.getAndUpdate(x -> x + 1);      // returns 200, now 201
n.accumulateAndGet(5, Math::max);// 201
```

The `xxxAndGet` / `getAndXxx` naming: *which value do I get back, the new one or the previous one?*

---

## 2. How CAS works

`compareAndSet(expected, newValue)` is **one atomic hardware step**: "if the variable still equals `expected`, set it to `newValue` and report success; otherwise change nothing and report failure."

Methods like `incrementAndGet` are loops built on it:

```java
// Conceptually what updateAndGet does
int prev, next;
do {
    prev = value;                 // read the current value
    next = f(prev);               // compute the new value
} while (!compareAndSet(prev, next));   // retry if another thread changed it in between
```

Consequences:

- **No locks, no blocking.** A thread that loses a race just retries, so a slow thread can't block others. Under extreme contention retries burn CPU (see `LongAdder`).
- The function you pass to `updateAndGet`/`accumulateAndGet` **may run more than once**, so it must be **side-effect free** (no logging counters, no I/O, no mutation).
- Atomicity covers **one variable**. It can't keep two variables consistent with each other.

---

## 3. Using them

### Counters and IDs

```java
private static final AtomicLong NEXT_ID = new AtomicLong();
long newId() { return NEXT_ID.incrementAndGet(); }
```

### Flags and one-shot actions

```java
private final AtomicBoolean started = new AtomicBoolean(false);

void start() {
    if (started.compareAndSet(false, true)) {     // exactly one caller wins
        doStart();
    }
}
```

This replaces the racy `if (!started) { started = true; ... }`.

### Lock-free updates of immutable state with `AtomicReference`

To change several fields together without a lock, put them in an **immutable object** and swap the whole thing:

```java
record Stats(long count, long total) {
    Stats add(long value) { return new Stats(count + 1, total + value); }
}

private final AtomicReference<Stats> stats = new AtomicReference<>(new Stats(0, 0));

void record(long value) {
    stats.updateAndGet(s -> s.add(value));        // atomically replaces one consistent snapshot with another
}
```

Readers calling `stats.get()` always see a consistent `Stats`. This is a powerful, simple pattern: **immutable value + `AtomicReference`**.

### Atomic arrays

`AtomicIntegerArray` makes element updates atomic (`incrementAndGet(i)`). An `AtomicInteger[]` or a `volatile int[]` does not (volatile applies to the reference, not the elements).

---

## 4. High contention: `LongAdder`

If many threads hammer one `AtomicLong`, CAS retries pile up. `LongAdder` spreads updates across internal **cells** and adds them up on demand:

```java
LongAdder requests = new LongAdder();

requests.increment();             // very low contention cost
requests.add(5);
long total = requests.sum();      // adds the cells: NOT an atomic snapshot while updates continue
```

| | `AtomicLong` | `LongAdder` |
|---|---|---|
| Update cost under heavy contention | Higher (CAS retries) | Much lower |
| Read | `get()`: always exact | `sum()`: may miss concurrent updates |
| Memory | One variable | Several cells |
| Good for | IDs, sequence numbers, anything needing exact current value, CAS | **Statistics and metrics**: counts you read occasionally |

Use `LongAdder` for hit counters, metrics, and totals. **Don't** use it to generate unique IDs or for any logic that depends on an exact instantaneous value. `LongAccumulator` generalizes this to any associative function (such as `max`).

---

## 5. Pitfalls

### Compare-and-set uses `==`, not `equals`

`AtomicReference.compareAndSet(expected, new)` compares **references**. With boxed values this can surprise you:

```java
AtomicReference<Integer> ref = new AtomicReference<>(1000);
ref.compareAndSet(1000, 2000);    // likely FALSE: autoboxing creates a different Integer object than the stored one
```

Prefer `AtomicInteger`/`AtomicLong` for numbers. For objects, keep the exact reference you read (`prev = ref.get(); ... ref.compareAndSet(prev, next)`) or use `updateAndGet`, which does this for you.

### The ABA problem

CAS only checks "is the value still `A`?". It can't tell if it changed `A → B → A` in between. For simple counters that's harmless, but for linked structures it can corrupt logic. `AtomicStampedReference` pairs the reference with a version number, so `A(v1)` and `A(v3)` differ.

### Compound actions across multiple atomics are not atomic

```java
if (balance.get() >= amount) {          // check
    balance.addAndGet(-amount);         // act: another thread may have withdrawn in between
}
```

Make the check part of one atomic operation (a CAS loop, or `updateAndGet` with a condition), or guard the whole thing with a lock, or keep the related state in one immutable object behind an `AtomicReference`.

```java
// Correct CAS loop for a withdrawal that must not go negative
long prev, next;
do {
    prev = balance.get();
    if (prev < amount) return false;
    next = prev - amount;
} while (!balance.compareAndSet(prev, next));
return true;
```

### Two atomics ≠ one invariant

`AtomicInteger min` and `AtomicInteger max` can each be updated atomically, but you can't atomically keep `min <= max` across both. Use a lock or an immutable pair in an `AtomicReference`.

---

## 6. When to use what

| Need | Use |
|---|---|
| One counter / flag / reference, simple update | **Atomic** class |
| Heavily contended statistics | `LongAdder` |
| Several variables that must change together | Lock, or immutable object in `AtomicReference` |
| Long computations inside the update | Lock (a CAS retry would redo the work) |
| Blocking/waiting behavior | Locks, queues, synchronizers |

Atomics are also what many concurrent collections use internally. They're one of the building blocks behind `ConcurrentHashMap` ([note 08](08_concurrent-collections.md)). For field-level atomics without wrapper objects there are `AtomicXxxFieldUpdater`s and `VarHandle`s, which are advanced tools for library authors.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `if (a.get() == x) a.set(y)` (check-then-act) | `a.compareAndSet(x, y)` |
| `a.set(a.get() + 1)` | `a.incrementAndGet()` |
| Side effects inside `updateAndGet`'s lambda | Keep it pure: it may run several times |
| `AtomicReference<Integer>` with `compareAndSet(1000, ...)` | Use `AtomicInteger`, or reuse the exact object read |
| `LongAdder` for sequence/ID generation or exact reads | `AtomicLong` |
| Expecting two atomics to stay consistent with each other | Lock, or an immutable holder |
| Using atomics where a simple lock would be clearer | Choose the simplest correct tool |
| Assuming `volatile int[]` has atomic elements | `AtomicIntegerArray` |
| Mutating the object inside an `AtomicReference` | Replace it with a new immutable instance |

### Debugging

- Wrong totals despite using atomics → look for a check-then-act split across `get()` and `set()`, or two related atomics.
- High CPU with many threads on one counter → CAS contention. Try `LongAdder`.
- `compareAndSet` "never succeeds" on boxed values → reference comparison. Use primitive atomics or the exact reference.
- Results differ between `sum()` calls during updates → expected for `LongAdder`.

---

## Quick Summary

- Atomic classes give **lock-free, thread-safe, single-variable** operations with `volatile` semantics, built on **CAS** (compare-and-swap).
- Prefer `incrementAndGet`, `updateAndGet`, `compareAndSet` over `get()` then `set()`. The update function must be side-effect free.
- **Immutable value + `AtomicReference`** lets you swap multi-field state atomically.
- `LongAdder` for hot statistics counters; `AtomicLong` when you need exact values or IDs.
- Watch out for `==` comparison in `AtomicReference`, the ABA problem, and invariants spanning several atomics. Those need a lock.

**Next:** [Synchronizers](07_synchronizers.md)