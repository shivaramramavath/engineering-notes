# Thread Safety and Shared Mutable State

> The foundational principles of Java thread safety: shared mutable state, race conditions, data races vs race conditions, atomicity, visibility, ordering, immutability, and thread confinement.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_threads-and-lifecycle.md](00_threads-and-lifecycle.md), [04-oop/03_final.md](../04-oop/03_final.md) |
| **Time to Read** | 26 minutes |
| **Status** | Active |
| **Java Version** | Java 1.0+ |

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

### The Root of All Concurrency Bugs
A class is **thread-safe** if it behaves correctly when accessed from multiple concurrent threads, regardless of the scheduling or interleaving of the execution of those threads, and with no additional synchronization or coordination on the part of the calling code.

All concurrency bugs stem from a single lethal combination:
$$\text{Concurrency Hazard} = \text{Shared} + \text{Mutable} + \text{State}$$
If state is **not shared** (thread-confined), it is safe.
If state is **not mutable** (immutable), it is safe.
Only when multiple threads concurrently read and write the exact same memory locations without synchronization do race conditions, data corruption, and deadlocks occur.

---

## 2. Core Concepts

### The Three Pillars of Thread Safety
1. **Atomicity**: An operation is atomic if it executes as a single indivisible unit. It either happens completely or not at all. No thread can observe the operation in a partially completed state (e.g. `count++` is **not** atomic; it is 3 separate instructions: read, increment, write).
2. **Visibility**: When one thread modifies shared state, visibility guarantees that another thread immediately sees the updated value, rather than reading a stale value cached in CPU L1/L2 hardware registers.
3. **Ordering**: In the absence of synchronization, the compiler, JIT, and CPU hardware aggressively reorder instructions for performance. Ordering guarantees that operations happen in a predictable sequence.

### Data Race vs Race Condition
- **Data Race**: A low-level memory flaw where two threads access the same memory location concurrently, at least one access is a write, and there is no synchronization (happens-before relationship) between them.
- **Race Condition**: A high-level semantic flaw where the correctness of a program depends on the timing or interleaving of threads (e.g., check-then-act, read-modify-write). You can have a race condition even without a data race!

### Strategies to Achieve Thread Safety
1. **Thread Confinement**: Do not share the data across threads (e.g., local variables, `ThreadLocal`).
2. **Immutability**: Make state completely immutable (e.g., `final` fields, Java Records).
3. **Synchronization / Locks**: Use intrinsic monitors (`synchronized`) or explicit `ReentrantLock` to serialize access.
4. **Non-Blocking Atomics**: Use hardware Compare-And-Swap (CAS) via `AtomicInteger`, `AtomicReference`, etc.

---

## 3. How It Works Internally

### CPU Hardware Cache Architecture and Visibility
Modern multi-core processors have dedicated L1 and L2 caches per core, and a shared L3 cache:
```
+-------------------+       +-------------------+
|     Core 1        |       |     Core 2        |
|  +-------------+  |       |  +-------------+  |
|  |  L1 Cache   |  |       |  |  L1 Cache   |  |
|  +-------------+  |       |  +-------------+  |
|  |  L2 Cache   |  |       |  |  L2 Cache   |  |
+--------+----------+       +--------+----------+
         |                           |
         +-------------+-------------+
                       |
               +-------+-------+
               |   L3 Cache    |
               +-------+-------+
                       |
               +-------+-------+
               |  Main Memory  |
               +---------------+
```
When Thread A on Core 1 writes to a variable:
- The write sits in Core 1's store buffer / L1 cache.
- Unless a **Memory Barrier (Fence)** instruction is emitted, Core 2 continues reading stale data from its own local L1 cache indefinitely.

### Read-Modify-Write Bytecode Disassembly
The seemingly atomic statement `count++` compiles to three distinct bytecode instructions:
```bytecode
aload_0         // Push 'this' onto stack
getfield #2     // READ current value of field count
iconst_1        // Push constant 1
iadd            // MODIFY: Add 1
putfield #2     // WRITE updated value back to field count
```
If two threads interleave execution of these instructions, one increment is lost!

---

## 4. Syntax & Usage

### Thread-Safe Counter: Three Approaches
```java
import java.util.concurrent.atomic.AtomicInteger;

// Approach 1: Synchronized (Locking)
class SynchronizedCounter {
    private int count = 0;
    public synchronized void increment() { count++; }
    public synchronized int get() { return count; }
}

// Approach 2: Atomic (Lock-Free Hardware CAS)
class AtomicCounter {
    private final AtomicInteger count = new AtomicInteger(0);
    public void increment() { count.incrementAndGet(); }
    public int get() { return count.get(); }
}

// Approach 3: Thread Confinement (ThreadLocal)
class ThreadLocalCounter {
    private static final ThreadLocal<Integer> counter = ThreadLocal.withInitial(() -> 0);
    public void increment() { counter.set(counter.get() + 1); }
    public int get() { return counter.get(); }
}
```

---

## 5. Practical Examples

### Example 1: Demonstrating Lost Updates via Race Condition
```java
// Goal: Prove that 'count++' is not thread-safe under concurrent load
import java.util.concurrent.*;

public class RaceConditionDemo {
    private static int counter = 0;

    public static void main(String[] args) throws InterruptedException {
        int threads = 10;
        int incrementsPerThread = 10_000;

        try (ExecutorService executor = Executors.newFixedThreadPool(threads)) {
            for (int i = 0; i < threads; i++) {
                executor.submit(() -> {
                    for (int j = 0; j < incrementsPerThread; j++) {
                        counter++; // NOT THREAD-SAFE!
                    }
                });
            }
        } // Executor close() waits for all tasks to complete

        // Expected: 100,000. Actual: Significantly less (e.g. 43,219)!
        System.out.println("Expected: " + (threads * incrementsPerThread));
        System.out.println("Actual counter value: " + counter);
    }
}
```
*Output:*
```
Expected: 100000
Actual counter value: 43219
```
*Explanation:* Lost updates occur because threads overwrite each other's stale reads.

### Example 2: Check-Then-Act Race Condition (Lazy Initialization)
```java
// Goal: Demonstrate flaw in unsynchronized lazy initialization
public class UnsafeLazyInit {
    private static ExpensiveObject instance;

    // RACE CONDITION: Two threads can see instance == null simultaneously!
    public static ExpensiveObject getInstance() {
        if (instance == null) { // Check
            instance = new ExpensiveObject(); // Act (Duplicate creation!)
        }
        return instance;
    }

    static class ExpensiveObject {
        ExpensiveObject() {
            System.out.println("Expensive resource allocated!");
        }
    }
}
```
*Explanation:* Multiple threads evaluate `instance == null` as true simultaneously, constructing duplicate instances and leaking system resources.

### Example 3: Immutability as the Ultimate Thread-Safety Guarantee
```java
// Goal: Deeply immutable value object safe across unlimited concurrent threads
import java.util.*;

public final class UserSecurityContext {
    private final String username;
    private final List<String> permissions;

    public UserSecurityContext(String username, List<String> permissions) {
        this.username = Objects.requireNonNull(username);
        // Defensive copy guarantees outside caller cannot mutate list!
        this.permissions = List.copyOf(permissions);
    }

    public String username() { return username; }
    public List<String> permissions() { return permissions; }
}
```
*Explanation:* Immutable objects require zero locks, zero memory barriers, and are inherently thread-safe across all CPU cores.

---

## 6. Best Practices
1. **Default to Immutability**: Make all fields `private final`. Use Java Records for data models.
2. **Minimize Shared State**: Design architectures where threads operate on private, thread-confined local data and communicate via concurrent message queues.
3. **Use Atomics for Single Variables**: Prefer `AtomicInteger`, `AtomicLong`, and `AtomicReference` over explicit locks for simple shared variables.
4. **Never Publish Unfinished Objects**: Never allow the `this` reference to escape inside a constructor before initialization is complete.
5. **Protect Both Reads and Writes**: Synchronizing writes while leaving reads unsynchronized causes stale read visibility bugs.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Assuming `volatile` provides atomicity | `volatile` guarantees visibility and ordering, but NOT atomicity (`volatile count++` is broken). | Use `AtomicInteger` or `synchronized`. |
| Synchronizing only the write method | Reads without synchronization can see stale cached memory indefinitely. | Synchronize both getter and setter on the same monitor lock. |
| Leaking `this` in constructor | Other threads can observe partially constructed, corrupt object state. | Never pass `this` to listeners or start threads inside constructors. |
| Returning mutable internal state | Callers can mutate internal collections, bypassing class invariant checks. | Return unmodifiable views (`Collections.unmodifiableList`) or defensive copies. |
| Check-then-act without locking | State can change between the check and the act, invalidating the condition. | Guard compound checks atomically (`computeIfAbsent`, `putIfAbsent`). |

---

## 8. Performance & Security Notes

### False Sharing and Cache Line Invalidation
CPUs transfer memory between caches in 64-byte chunks called **Cache Lines**. If two independent variables accessed concurrently by different threads reside on the same 64-byte cache line, modifying one variable invalidates the other core's cache line, causing severe memory bus contention (**False Sharing**). Modern high-performance Java libraries use `@jdk.internal.vm.annotation.Contended` to pad memory offsets.

### Escaped References Security Vulnerability
If an object's internal mutable state (such as a private `byte[]` encryption key) is returned directly via a getter without defensive copying, external malicious code can modify the key in-place, subverting security checks.

---

## 9. Comparison / When to Use What

| Technique | Complexity | Contention Scalability | Best Used For |
|---|---|---|---|
| **Immutability** | Very Low | Infinite | Value objects, DTOs, configurations |
| **Thread Confinement** | Low | Infinite | Request-scoped contexts, buffers |
| **Atomic Classes (CAS)**| Medium | High | Counters, flags, single references |
| **Explicit Locks** | High | Medium | Multi-variable compound invariants |

---

## 10. Interview Questions

<details>
<summary>1. What constitutes thread safety in Java?</summary>
A class is thread-safe if it continues to function correctly when accessed by multiple concurrent threads without requiring the calling code to provide external synchronization, regardless of thread interleaving.
</details>

<details>
<summary>2. Why is `count++` not thread-safe even if `count` is declared `volatile`?</summary>
Because `count++` is a compound operation consisting of three separate bytecode instructions: read, increment, and write. `volatile` guarantees visibility of reads and writes, but does not make the composite sequence atomic.
</details>

<details>
<summary>3. What is the difference between a Data Race and a Race Condition?</summary>
A data race occurs when two threads concurrently access the same memory location without synchronization, with at least one access being a write. A race condition is a logical flaw where correctness depends on relative execution timing (e.g. check-then-act).
</details>

<details>
<summary>4. What are the rules for creating an Immutable class in Java?</summary>
1. Declare class `final` (or use Records).
2. Make all fields `private` and `final`.
3. Do not provide setters.
4. Ensure exclusive access to any mutable components via defensive copying in constructors and getters.
5. Do not let `this` escape during construction.
</details>

<details>
<summary>5. What is Thread Confinement?</summary>
An architecture where mutable data is accessed exclusively by a single thread, eliminating the need for synchronization. Examples include method-local variables and `ThreadLocal` containers.
</details>

<details>
<summary>6. What is a Memory Barrier (Fence)?</summary>
A low-level CPU instruction that enforces an ordering constraint on memory operations, forcing CPU store buffers to flush to cache and invalidating stale read caches across cores.
</details>

<details>
<summary>7. What is False Sharing?</summary>
When two threads on separate cores modify independent variables that happen to share the same 64-byte CPU cache line. Modifying one variable forces the other core to reload its entire cache line, drastically degrading performance.
</details>

<details>
<summary>8. How does the Initialization-on-demand Holder idiom provide safe lazy loading?</summary>
It relies on the JVM's class initialization guarantees (`JLS 12.4.2`). The static nested class is not loaded until referenced by `getInstance()`, and the JVM guarantees thread-safe class loading without explicit locks.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Thread-Safe Accumulator
- **Task**: Implement a thread-safe `LongAccumulator` with methods `add(long val)` and `get()` using `AtomicLong`.
<details>
<summary>Solution</summary>

```java
import java.util.concurrent.atomic.AtomicLong;

public class ThreadSafeAccumulator {
    private final AtomicLong total = new AtomicLong(0);

    public void add(long val) {
        total.addAndGet(val);
    }

    public long get() {
        return total.get();
    }
}
```
</details>

### Exercise 2 (Medium): Thread-Safe Lazy Initialization Holder
- **Task**: Implement the thread-safe Initialization-on-demand Holder idiom to lazily instantiate a heavy `DatabaseConfig` object.
<details>
<summary>Solution</summary>

```java
public class DatabaseConfigManager {
    public static class DatabaseConfig {
        private DatabaseConfig() {
            System.out.println("Loaded heavy database configuration.");
        }
    }

    private static class Holder {
        private static final DatabaseConfig INSTANCE = new DatabaseConfig();
    }

    public static DatabaseConfig getInstance() {
        // Guaranteed thread-safe by JVM class initialization phase!
        return Holder.INSTANCE;
    }
}
```
</details>

### Exercise 3 (Hard): Thread-Safe Bounded Stack with Invariant Protection
- **Task**: Implement a bounded stack `SafeBoundedStack<T>` with methods `push(T item)` and `pop()` that prevents race conditions, enforces capacity limits, and returns `Optional<T>`.
<details>
<summary>Solution</summary>

```java
import java.util.*;

public class SafeBoundedStack<T> {
    private final Object[] elements;
    private final int capacity;
    private int size = 0;

    public SafeBoundedStack(int capacity) {
        if (capacity <= 0) throw new IllegalArgumentException();
        this.capacity = capacity;
        this.elements = new Object[capacity];
    }

    public synchronized boolean push(T item) {
        Objects.requireNonNull(item);
        if (size >= capacity) {
            return false; // Full
        }
        elements[size++] = item;
        return true;
    }

    @SuppressWarnings("unchecked")
    public synchronized Optional<T> pop() {
        if (size == 0) {
            return Optional.empty(); // Empty
        }
        size--;
        T val = (T) elements[size];
        elements[size] = null; // Prevent memory leak!
        return Optional.of(val);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                               THREAD SAFETY CHEAT SHEET                                 |
+-----------------------------------------------------------------------------------------+
| Golden Formula           | Concurrency Bug = Shared + Mutable + State                   |
| 3 Invariants             | Atomicity, Visibility, Ordering                              |
| Data Race                | Concurrent unsynchronized access with at least 1 write       |
| Immutability Rule        | All fields private final; defensive copies in and out        |
| Volatile Limitation      | Volatile gives visibility & ordering, NOT compound atomicity |
| Atomic Classes           | Use AtomicInteger/Long for lock-free atomic updates          |
| Confinement              | Keep state thread-local; zero synchronization required       |
+-----------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- Java Concurrency in Practice by Brian Goetz — Chapter 2 (Thread Safety)
- JSR-133: Java Memory Model and Thread Specification
- "The Art of Multiprocessor Programming" by Maurice Herlihy & Nir Shavit
