# Atomic Classes and Non-Blocking Synchronization

> Architectural guide to lock-free programming in Java: hardware Compare-And-Swap (CAS), `AtomicInteger`, `AtomicLong`, `AtomicReference`, the ABA problem and `AtomicStampedReference`, `AtomicFieldUpdater`, and high-contention striping with `LongAdder`.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [02_thread-safety.md](02_thread-safety.md), [04_memory-model-and-volatile.md](04_memory-model-and-volatile.md) |
| **Time to Read** | 26 minutes |
| **Status** | Active |
| **Java Version** | Java 5.0+ (LongAdder in Java 8+) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Cost of Blocking Locks
Traditional synchronization uses pessimistic blocking locks. When a thread encounters lock contention:
- The OS suspends (parks) the thread.
- A kernel context switch occurs.
- When the lock is freed, the OS schedules and unparks the thread.
For simple operations (like incrementing a counter or updating a state pointer), context switching takes thousands of CPU clock cycles, destroying throughput.

### The Solution: Hardware Compare-And-Swap (CAS)
Modern CPUs provide hardware-level atomic instructions (like `CMPXCHG` on x86).
Instead of blocking other threads, a thread speculatively performs an update:
1. It reads the current value $V$.
2. It computes the new value $N$.
3. It executes a single hardware CAS instruction: *"If current value is still $V$, set it to $N$. If not, return false."*
4. If CAS fails (another thread intervened), the thread simply retries in a tight spin loop without going to sleep.

Java encapsulates this in the `java.util.concurrent.atomic` package.

---

## 2. Core Concepts

### Core Atomic Types
- **Scalar Primitives**: `AtomicInteger`, `AtomicLong`, `AtomicBoolean`.
- **References**: `AtomicReference<V>` (atomic updates to object pointers).
- **Arrays**: `AtomicIntegerArray`, `AtomicReferenceArray`.
- **Field Updaters**: `AtomicIntegerFieldUpdater`, `AtomicReferenceFieldUpdater` (reflection-based CAS directly on volatile fields to eliminate wrapper object allocation).

### The ABA Problem and `AtomicStampedReference`
Suppose a thread reads reference $A$, prepares to CAS it to $C$.
Meanwhile, another thread changes $A \to B \to A$.
When the first thread performs CAS, the value is still $A$, so CAS succeeds—even though the state modified between the read and write!
In linked data structures (like lock-free stacks), this causes node recycling memory corruption.
**Solution**: `AtomicStampedReference<V>` pairs the reference with an integer version stamp, ensuring CAS fails if the stamp changed.

### High-Contention Striping: `LongAdder` and `DoubleAdder` (Java 8)
When hundreds of threads concurrently update an `AtomicLong`, they all compete to CAS the **exact same memory address**. CAS retries spin wastefully, degrading performance.
`LongAdder` solves this by maintaining an array of cells striped across CPU cores (**Dynamic Striping**). Each thread modifies its own dedicated cell; `sum()` aggregates all cells together.

---

## 3. How It Works Internally

### Hardware CAS Instruction Pipelining
```
+-----------------------------------------------------------+
| Thread A reads V = 10                                     |
| Thread B reads V = 10                                     |
+-----------------------------------------------------------+
                             │
                             ▼
+-----------------------------------------------------------+
| Thread A: CMPXCHG(expected=10, new=11) -> SUCCEEDS (V=11) |
| Thread B: CMPXCHG(expected=10, new=12) -> FAILS    (V=11) |
+-----------------------------------------------------------+
                             │
                             ▼
+-----------------------------------------------------------+
| Thread B re-reads V = 11, computes 11 + 1 = 12            |
| Thread B: CMPXCHG(expected=11, new=12) -> SUCCEEDS        |
+-----------------------------------------------------------+
```

### `LongAdder` Internal `Striped64` Architecture
`LongAdder` extends `Striped64`:
- When uncontended, it updates a single `base` variable via CAS.
- When contention is detected, it lazily allocates an array of `Cell` objects padded with `@jdk.internal.vm.annotation.Contended` to prevent false sharing.
- Threads hash their thread ID to update a specific cell independently without contention!

---

## 4. Syntax & Usage

### Basic `AtomicInteger` Operations
```java
import java.util.concurrent.atomic.AtomicInteger;

public class AtomicDemo {
    private final AtomicInteger counter = new AtomicInteger(0);

    public void increment() {
        counter.incrementAndGet(); // Atomic ++count
    }

    public boolean updateIfMatches(int expected, int update) {
        return counter.compareAndSet(expected, update); // CAS
    }

    public int accumulate(int value) {
        // Atomic functional update loop
        return counter.updateAndGet(current -> Math.max(current, value));
    }
}
```

### High-Throughput Metrics with `LongAdder`
```java
import java.util.concurrent.atomic.LongAdder;

public class RequestMetricsTracker {
    private final LongAdder requestCount = new LongAdder();

    public void recordRequest() {
        requestCount.increment(); // Extreme throughput across 100+ cores!
    }

    public long getTotalRequests() {
        return requestCount.sum(); // Sums across striped cells
    }
}
```

---

## 5. Practical Examples

### Example 1: Lock-Free Stack (Treiber Stack)
```java
// Goal: Implement a 100% lock-free concurrent LIFO stack using CAS
import java.util.concurrent.atomic.AtomicReference;

public class LockFreeStack<T> {
    private static class Node<T> {
        final T value;
        Node<T> next;
        Node(T val) { this.value = val; }
    }

    private final AtomicReference<Node<T>> head = new AtomicReference<>(null);

    public void push(T item) {
        Node<T> newHead = new Node<>(item);
        while (true) {
            Node<T> currentHead = head.get();
            newHead.next = currentHead;
            // Atomic CAS update: if head still equals currentHead, swap with newHead
            if (head.compareAndSet(currentHead, newHead)) {
                return; // Successfully pushed
            }
            // Otherwise, another thread pushed; spin and retry!
        }
    }

    public T pop() {
        while (true) {
            Node<T> currentHead = head.get();
            if (currentHead == null) {
                return null; // Empty stack
            }
            Node<T> nextHead = currentHead.next;
            if (head.compareAndSet(currentHead, nextHead)) {
                return currentHead.value; // Successfully popped
            }
        }
    }
}
```
*Explanation:* The Treiber stack guarantees forward progress without locks. If one thread stalls, others continue pushing and popping unimpeded.

### Example 2: Solving the ABA Problem with `AtomicStampedReference`
```java
// Goal: Demonstrate versioned reference updates to prevent ABA corruption
import java.util.concurrent.atomic.AtomicStampedReference;

public class AbaMitigationDemo {
    public static void main(String[] args) {
        String initialRef = "A";
        int initialStamp = 1;

        AtomicStampedReference<String> stampedRef = new AtomicStampedReference<>(initialRef, initialStamp);

        // Thread 1 inspects state
        int stampHolder = stampedRef.getStamp();
        String refHolder = stampedRef.getReference();

        // Thread 2 performs A -> B -> A update
        stampedRef.compareAndSet("A", "B", stampedRef.getStamp(), stampedRef.getStamp() + 1);
        stampedRef.compareAndSet("B", "A", stampedRef.getStamp(), stampedRef.getStamp() + 1);

        // Thread 1 attempts CAS using original stamp (1)
        boolean success = stampedRef.compareAndSet(refHolder, "C", stampHolder, stampHolder + 1);

        // CAS fails because stamp changed from 1 to 3!
        System.out.println("Did Thread 1 CAS succeed: " + success); // false
        System.out.println("Current reference: " + stampedRef.getReference() + ", stamp: " + stampedRef.getStamp());
    }
}
```
*Output:*
```
Did Thread 1 CAS succeed: false
Current reference: A, stamp: 3
```

---

## 6. Best Practices
1. **Prefer `LongAdder` for High-Contention Accumulators**: When multiple threads update a shared counter frequently and read rarely (e.g. metrics, stats), use `LongAdder` over `AtomicLong`.
2. **Use `updateAndGet()` for Complex Transformations**: Instead of manual CAS loops, use `atomic.updateAndGet(x -> ...)` to ensure mathematical updates execute safely.
3. **Beware of Unbounded Spin Loops**: If CAS contention is extreme, endless retries consume 100% CPU. Add a backoff policy (e.g. `Thread.onSpinWait()` in Java 9+).
4. **Use `AtomicReference` for Immutable State Replacement**: Package multi-variable state into an immutable Record and update the entire state atomically with a single `AtomicReference`.
5. **Use `AtomicFieldUpdater` for Massive Scale**: If instantiating millions of node objects, use `AtomicReferenceFieldUpdater` to avoid wrapper object memory overhead.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Multiple atomic calls without coordinated locking | Two independent atomic operations are NOT atomic together (check-then-act race). | Use `compareAndSet`, `updateAndGet`, or locks. |
| Using `AtomicLong` for global telemetry | All cores contend for the same cache line, collapsing performance. | Replace with `LongAdder`. |
| Unbounded CAS spin loops causing CPU starvation | Threads spinning at 100% CPU under severe contention degrade system responsiveness. | Introduce backoff or use `Thread.onSpinWait()`. |
| Ignoring the ABA problem in memory pooling | Reusing memory addresses can trick CAS into corrupting pointer graphs. | Use `AtomicStampedReference`. |
| Modifying non-volatile fields via `AtomicFieldUpdater` | Target fields must be declared `volatile` or runtime `IllegalArgumentException` is thrown. | Mark field `volatile`. |

---

## 8. Performance & Security Notes

### CAS Scalability Limits vs `LongAdder`
As the number of CPU cores increases (e.g. 64 or 128 cores), `AtomicLong` performance drops dramatically due to cache line bouncing (MESI protocol invalidation cycles across cores). `LongAdder` scales linearly with core counts by spreading writes across independent cache-line padded cells.

### Spin-Wait Loop Hints (`Thread.onSpinWait()`)
Java 9 introduced `Thread.onSpinWait()`. It emits an x86 `PAUSE` instruction inside busy-wait CAS loops, signaling the CPU pipeline to reduce power consumption and optimize speculative execution.

---

## 9. Comparison / When to Use What

| Metric / Feature | `AtomicLong` | `LongAdder` | `synchronized` |
|---|---|---|---|
| **Contention Performance**| Low under high threads | **Extremely High** | Low |
| **Exact Real-Time Read** | Yes (`get()`) | Eventual (`sum()`) | Yes |
| **Memory Footprint** | Tiny (8 bytes) | Larger (Striped cells)| Small |
| **Primary Use Case** | Unique IDs, sequence nums | Telemetry, counters | Multi-variable state |

---

## 10. Interview Questions

<details>
<summary>1. How does Compare-And-Swap (CAS) work at the hardware level?</summary>
CAS is an atomic CPU instruction (e.g. `CMPXCHG`) that compares the contents of a memory location with a given expected value. If they are equal, it modifies the memory to a new value; otherwise, it leaves memory unchanged and reports failure.
</details>

<details>
<summary>2. Why does `LongAdder` outperform `AtomicLong` under heavy contention?</summary>
`AtomicLong` forces all threads to CAS a single memory address, causing severe cache line invalidations. `LongAdder` stripes updates across an internal array of cells, allowing threads to update independent cells concurrently without collision.
</details>

<details>
<summary>3. What is the ABA problem and how is it resolved in Java?</summary>
The ABA problem occurs when a memory location changes from A to B and back to A. A thread reading A believes nothing changed, even though invariants may be invalidated. Java resolves it using `AtomicStampedReference`, which tracks a version number alongside the reference.
</details>

<details>
<summary>4. What does `Thread.onSpinWait()` do?</summary>
Introduced in Java 9, it is an intrinsic hint to the CPU (emitting a `PAUSE` instruction on x86) that the thread is in a busy-wait spin loop, reducing CPU core power draw and avoiding pipeline thrashing.
</details>

<details>
<summary>5. Can two separate calls on `AtomicInteger` be considered thread-safe together?</summary>
No. While each call is individually atomic, the compound sequence (e.g. `if (ai.get() == 0) ai.set(1);`) is not atomic and contains a check-then-act race condition. You must use `ai.compareAndSet(0, 1)`.
</details>

<details>
<summary>6. What is an `AtomicReferenceFieldUpdater`?</summary>
A reflection-based utility that enables atomic CAS operations directly on a normal `volatile` field of an existing class, avoiding the memory allocation overhead of dedicated atomic wrapper objects.
</details>

<details>
<summary>7. What are the performance trade-offs of `LongAdder.sum()`?</summary>
`sum()` must iterate over all cells and add their values. It provides an eventual, approximate snapshot during concurrent updates, but does not acquire locks.
</details>

<details>
<summary>8. Is lock-free programming always faster than lock-based programming?</summary>
No. Under low to moderate contention, locks and CAS perform similarly. Under extreme contention on a single variable, CAS spin loops consume 100% CPU without making progress, whereas locks park threads to prevent CPU waste.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Unique Sequence Generator
- **Task**: Implement a thread-safe sequence generator `SequenceGenerator` that issues unique incremental `long` identifiers starting from 1 using `AtomicLong`.
<details>
<summary>Solution</summary>

```java
import java.util.concurrent.atomic.AtomicLong;

public class SequenceGenerator {
    private final AtomicLong current = new AtomicLong(0);

    public long nextId() {
        return current.incrementAndGet();
    }
}
```
</details>

### Exercise 2 (Medium): Lock-Free Max-Value Tracker
- **Task**: Implement a `MaxValueTracker` with method `void update(int value)` that updates the maximum observed value using a CAS loop on `AtomicInteger`.
<details>
<summary>Solution</summary>

```java
import java.util.concurrent.atomic.AtomicInteger;

public class MaxValueTracker {
    private final AtomicInteger max = new AtomicInteger(Integer.MIN_VALUE);

    public void update(int value) {
        // updateAndGet handles the CAS retry loop internally
        max.updateAndGet(current -> Math.max(current, value));
    }

    public int getMax() {
        return max.get();
    }
}
```
</details>

### Exercise 3 (Hard): Lock-Free Atomic State Transition
- **Task**: Model a workflow state machine with states `CREATED -> VALIDATED -> PROCESSED -> ARCHIVED`. Implement `Workflow(String id)` using `AtomicReference` that only permits legal forward transitions.
<details>
<summary>Solution</summary>

```java
import java.util.concurrent.atomic.AtomicReference;

public class Workflow {
    public enum State { CREATED, VALIDATED, PROCESSED, ARCHIVED }

    private final String id;
    private final AtomicReference<State> state = new AtomicReference<>(State.CREATED);

    public Workflow(String id) { this.id = id; }

    public boolean transition(State expected, State next) {
        // Enforce valid forward transitions
        boolean valid = switch (expected) {
            case CREATED -> next == State.VALIDATED;
            case VALIDATED -> next == State.PROCESSED;
            case PROCESSED -> next == State.ARCHIVED;
            case ARCHIVED -> false;
        };
        if (!valid) {
            throw new IllegalArgumentException("Illegal transition: " + expected + " -> " + next);
        }
        return state.compareAndSet(expected, next);
    }

    public State getState() { return state.get(); }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                 ATOMIC CLASSES CHEAT SHEET                              |
+-----------------------------------------------------------------------------------------+
| Core Primitive Classes   | AtomicInteger, AtomicLong, AtomicBoolean                     |
| Object Reference Class   | AtomicReference<T> (CAS on reference pointers)               |
| Atomic Operations        | getAndIncrement(), addAndGet(), compareAndSet(exp, update)   |
| Functional Update        | atomic.updateAndGet(v -> Math.max(v, newValue))              |
| ABA Solution             | AtomicStampedReference<T> (Pairs reference + integer stamp)  |
| Contention Scaling       | LongAdder (Striped64 cells, sum() aggregates)                |
| CPU Spin Optimization    | Thread.onSpinWait() (Emits x86 PAUSE instruction)            |
+-----------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Javadoc: java.util.concurrent.atomic](https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/util/concurrent/atomic/package-summary.html)
- Java Concurrency in Practice by Brian Goetz — Chapter 15 (Non-blocking Synchronization)
- "A Pragmatic Approach to Lock-Free Synchronization" by Maurice Herlihy
