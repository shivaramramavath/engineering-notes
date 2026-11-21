# Concurrency Performance: Lock Contention, Contended Padding, and Lock-Free Scaling

> Scaling concurrent Java applications: Lock Contention Diagnostics, Cache Line Bouncing, LongAdder Striping, VarHandle vs Atomic, and Virtual Thread Scheduling.

| Metadata | Details |
|---|---|
| **Level** | Core / Advanced |
| **Prerequisites** | Java Concurrency, Locks, CPU Cache Lines |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: Cache Coherence Protocols & Striped Counters](#3-how-it-works-internally-cache-coherence-protocols--striped-counters)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why
Writing multi-threaded Java code is straightforward; writing multi-threaded Java code that scales linearly across 64 CPU cores is notoriously difficult. Under high concurrency, naive synchronization mechanisms—such as coarse `synchronized` blocks or heavily contended `AtomicInteger` variables—introduce massive CPU cache invalidation traffic and context-switching bottlenecks.

A single atomic counter updated by 32 concurrent threads can degrade throughput from millions of operations per second to a crawl due to **Cache Line Bouncing** (the MESI cache coherence storm). Scaling concurrent Java systems requires understanding hardware cache line behavior, false sharing, striped cell accumulators (`LongAdder`), `VarHandle` memory fences, and Virtual Thread pinning avoidance.

---

## 2. Core Concepts
- **Amdahl's Law**: The theoretical speedup of a program using multiple processors is limited by the sequential fraction of the program ($Speedup = rac{1}{S + rac{1 - S}{N}}$). Reducing synchronized lock duration directly raises scalability ceilings.
- **Cache Coherence Protocols (MESI/MOESI)**: Hardware protocols maintaining consistency across CPU L1/L2 caches (Modified, Exclusive, Shared, Invalid).
- **False Sharing**: Performance degradation occurring when two threads update distinct variables that reside within the same 64-byte hardware cache line.
- **Lock Striping**: Partitioning a collection or counter across multiple independent locks or accumulator cells (e.g., `ConcurrentHashMap` bins, `LongAdder` cells) to eliminate contention on a single shared memory address.
- **Atomic vs LongAdder**: `AtomicLong` uses a single shared CAS memory cell, bottlenecking under high contention. `LongAdder` maintains a dynamic array of striped `Cell` objects, achieving near-linear scaling across cores.
- **VarHandle**: Introduced in Java 9 to provide type-safe, low-overhead memory ordering modes (Plain, Opaque, Acquire/Release, Volatile).
- **Virtual Thread Pinning**: The anti-pattern where a virtual thread executes blocking operations inside a native `synchronized` block, pinning its underlying carrier OS thread and starving the ForkJoinPool.

---

## 3. How It Works Internally: Cache Coherence Protocols & Striped Counters

Understanding why AtomicLong bottlenecks under contention and how LongAdder scales:

```
AtomicLong Contention Bottleneck:
Thread 1 (Core 0) ----CAS----\ 
Thread 2 (Core 1) ----CAS-----+---> [ Single Memory Address: value ]
Thread 3 (Core 2) ----CAS----/       (Repeated CAS failures loop continuously;
                                      MESI protocol invalidates L1 cache line on every core!)

LongAdder Striped Scaling:
Thread 1 (Core 0) ----CAS---------> [ Cell 0: @Contended Padded Cell ]
Thread 2 (Core 1) ----CAS---------> [ Cell 1: @Contended Padded Cell ]
Thread 3 (Core 2) ----CAS---------> [ Cell 2: @Contended Padded Cell ]
                                             |
                                 Sum = Cell 0 + Cell 1 + Cell 2
                                 (Zero cache line bouncing across cores!)
```

### The LongAdder Striping Algorithm:
1. Under zero or low contention, threads update a single `base` volatile field using standard CAS.
2. When contention is detected (a CAS on `base` fails), `LongAdder` lazily initializes an internal array of `Cell` objects.
3. Each thread uses its thread-local probe hash (`ThreadLocalRandom.getProbe()`) to index into the cell array.
4. Each `Cell` is annotated with `@jdk.internal.vm.annotation.Contended`, which pads the cell to 128 bytes, guaranteeing that no two cells ever share the same CPU cache line.

---

## 4. Syntax & Usage

### 4.1 High-Throughput Metrics Collection with LongAdder
```java
package com.example.performance;

import java.util.concurrent.atomic.LongAdder;

public final class ThroughputCounter {
    // Shared across hundreds of worker threads with zero cache bouncing
    private final LongAdder requestCounter = new LongAdder();

    public void recordRequest() {
        requestCounter.increment(); // Nanosecond-fast lock-free cell increment
    }

    public long getTotalRequests() {
        return requestCounter.sum(); // Aggregates cells across all threads
    }

    public long getAndReset() {
        return requestCounter.sumThenReset();
    }
}
```

### 4.2 Low-Overhead Memory Ordering via VarHandle
```java
package com.example.performance;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;

public class SequenceGenerator {
    private volatile long sequence = 0L;

    private static final VarHandle SEQUENCE_HANDLE;

    static {
        try {
            SEQUENCE_HANDLE = MethodHandles.lookup()
                    .findVarHandle(SequenceGenerator.class, "sequence", long.class);
        } catch (ReflectiveOperationException e) {
            throw new ExceptionInInitializerError(e);
        }
    }

    // Release semantics: Guarantees prior writes are visible to reader without full volatile fence
    public void publishSequence(long nextSeq) {
        SEQUENCE_HANDLE.setRelease(this, nextSeq);
    }

    // Acquire semantics: Lightest barrier ensuring reader sees published data
    public long readSequence() {
        return (long) SEQUENCE_HANDLE.getAcquire(this);
    }
}
```

---

## 5. Practical Examples

### Example 1: Eliminating Virtual Thread Carrier Pinning
```java
package com.example.performance;

import java.util.concurrent.locks.ReentrantLock;

public final class VirtualThreadSafeResource {
    // ANTI-PATTERN: Synchronized block pins the underlying carrier OS thread during blocking I/O!
    /*
    public synchronized void brokenMethod() {
        socketRead(); // Pins carrier thread!
    }
    */

    // OPTIMIZED: ReentrantLock allows Virtual Thread to unmount cleanly from carrier thread!
    private final ReentrantLock lock = new ReentrantLock();

    public void safeMethod() {
        lock.lock();
        try {
            socketRead(); // Virtual thread unmounts cleanly; carrier thread runs other virtual threads!
        } finally {
            lock.unlock();
        }
    }

    private void socketRead() {
        try { Thread.sleep(50); } catch (InterruptedException ignored) {}
    }
}
```

### Example 2: High-Performance Lock Striping (Fine-Grained Locking)
```java
package com.example.performance;

import java.util.concurrent.locks.ReentrantLock;

public final class StripedAccountManager {
    private static final int STRIPE_COUNT = 16;
    private final ReentrantLock[] locks = new ReentrantLock[STRIPE_COUNT];

    public StripedAccountManager() {
        for (int i = 0; i < STRIPE_COUNT; i++) {
            locks[i] = new ReentrantLock();
        }
    }

    private ReentrantLock getLock(long accountId) {
        // Map account ID to specific lock stripe
        int index = (int) Math.abs(accountId % STRIPE_COUNT);
        return locks[index];
    }

    public void processAccount(long accountId, Runnable operation) {
        ReentrantLock lock = getLock(accountId);
        lock.lock();
        try {
            operation.run();
        } finally {
            lock.unlock();
        }
    }
}
```

### Example 3: Edge Case - Diagnosing Carrier Thread Pinning in Java 21+
```bash
# Run JVM with virtual thread pinning diagnostics flag:
java -Djdk.tracePinnedThreads=full -jar target/app.jar
# Prints full stack traces whenever a virtual thread blocks inside synchronized!
```

---

## 6. Best Practices
1. **Prefer `LongAdder` over `AtomicLong` for Metrics & Counters**: When write throughput is high and reads are occasional, `LongAdder` scales linearly across cores.
2. **Replace `synchronized` with `ReentrantLock` when using Virtual Threads**: Virtual threads cannot unmount while holding object monitors, pinning carrier threads and exhausting executor capacity.
3. **Minimize Critical Section Scope**: Keep locked code blocks as small as possible; perform input validation, hashing, and DTO construction *before* acquiring locks.
4. **Use Striped Locking for High-Volume Keyed Entities**: Partition locks by key hash (like `ConcurrentHashMap`) to avoid a single global bottleneck.
5. **Never Sleep or Perform Network Calls Inside Locks**: Holding locks during network I/O starves all competing threads.
6. **Pad Contended Variables with `@Contended`**: When building high-performance ring buffers or concurrency primitives, isolate cache lines using padding.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Using `AtomicLong` for high-concurrency metric counters | Threads thrash in tight CAS retry loops, degrading CPU caches | Use `LongAdder` or `LongAccumulator` |
| Blocking I/O inside `synchronized` with Virtual Threads | Pins carrier thread, stalling the entire Virtual Thread pool | Replace `synchronized` with `ReentrantLock` |
| Coarse-grained global locks | All threads serialize behind a single bottleneck | Adopt fine-grained lock striping |
| False sharing in adjacent volatile variables | Memory bus saturated by MESI cache line invalidation cycles | Separate variables or use `@Contended` padding |
| Unbounded thread pools (`Executors.newCachedThreadPool`) | Spawns thousands of OS threads, causing OOM or thrashing | Use bounded thread pools or Virtual Threads |
| Inconsistent lock ordering across threads | Thread 1 locks A then B; Thread 2 locks B then A; causes deadlock | Always acquire locks in a deterministic order |
| Calling expensive methods inside lock critical section | Extends lock hold time, severely lowering throughput | Move non-shared computation outside lock |
| Busy-waiting loops without backoff (`while (flag) {}`) | Spins CPU cores at 100%, starving competing worker threads | Use `Thread.onSpinWait()` or explicit synchronizers |

---

## 8. Performance & Security Notes
- **MESI Protocol Invalidation Storms**: When multiple CPU cores execute CAS operations on the same cache line, the hardware memory bus is flooded with cache invalidation broadcasts, causing CPU core execution pipelines to stall waiting for memory coherency.
- **Hardware Pause Hint (`Thread.onSpinWait()`)**: In spin-loops, invoking `Thread.onSpinWait()` emits the x86 `PAUSE` instruction, which signals to the CPU that a spin-lock is executing, reducing core power consumption and avoiding memory order violation penalties.
- **Deadlock Vulnerability**: Deadlocks freeze application processing indefinitely. Enable automated JFR lock contention monitoring to detect long-lived lock hold times before they escalate into complete system deadlocks.

---

## 9. Comparison / When to Use What

| Concurrency Primitive | Contention Scalability | Memory Overhead | Best Use Case |
|---|---|---|---|
| **`AtomicLong`** | Poor under high write contention | Minimal (8 bytes) | Sequences, CAS state machines, low-concurrency counters |
| **`LongAdder`** | Linear across CPU cores | Moderate (Array of padded cells) | High-volume telemetry, request counters, metrics |
| **`synchronized`** | High (Monitor lock, Carrier pinning)| Zero object overhead | Short, non-I/O mutual exclusion in platform thread apps |
| **`ReentrantLock`** | High (Virtual thread unmount safe) | Object header + node allocation | Virtual Threads, timeouts (`tryLock`), fairness |
| **`StampedLock`** | Highest (Optimistic read capability) | Moderate | Read-heavy concurrent caches |

---

## 10. Interview Questions

<details>
<summary>1. Why does LongAdder scale significantly better than AtomicLong under high write contention?</summary>
<code>AtomicLong</code> stores its value in a single <code>volatile long</code> memory location. When dozens of concurrent threads call <code>incrementAndGet()</code>, they all compete to execute a Compare-And-Swap (CAS) on that exact address. Only one thread succeeds; the remaining threads fail and loop repeatedly, triggering an avalanche of hardware cache invalidations across CPU cores.<br>
<code>LongAdder</code> maintains a dynamically sized array of <code>Cell</code> objects, each padded to 128 bytes to prevent false sharing. Each thread hashes to a separate cell based on its thread probe, updating its own independent cell with zero contention. When the total is needed, <code>sum()</code> aggregates all cells together.
</details>

<details>
<summary>2. What is Virtual Thread Carrier Pinning, and what causes it in Java 21?</summary>
Virtual threads are lightweight user-mode threads managed by the JVM and scheduled onto a small pool of underlying operating system threads (Carrier Threads). Under normal conditions, when a virtual thread blocks on I/O or a <code>ReentrantLock</code>, the JVM unmounts the virtual thread, freeing the carrier thread to execute other virtual threads.<br>
However, if a virtual thread blocks on I/O or sleep <i>inside a native synchronized block or method</i>, or calls native code via JNI, the virtual thread is <b>pinned</b> to its carrier thread. The carrier thread cannot be unmounted, starving the ForkJoinPool and causing severe application freeze.
</details>

<details>
<summary>3. What is False Sharing and how does @Contended resolve it?</summary>
CPUs transfer memory between RAM and L1/L2 caches in fixed 64-byte blocks called cache lines. If Thread A modifies variable X and Thread B modifies variable Y, and both variables reside on the same 64-byte cache line, modifying X forces the hardware cache coherence protocol (MESI) to invalidate the entire cache line on Thread B's core. Both cores constantly invalidate each other's cache lines (Cache Line Bouncing).<br>
The <code>@jdk.internal.vm.annotation.Contended</code> annotation instructs HotSpot to inject 128 bytes of padding around the annotated field, ensuring it resides alone on its own isolated cache line.
</details>

<details>
<summary>4. What is Amdahl's Law and how does it apply to Java lock contention?</summary>
Amdahl's Law states that the theoretical maximum speedup of a program running on $N$ processor cores is strictly limited by the sequential (synchronized) portion of the code:<br>
$Speedup = rac{1}{S + rac{1 - S}{N}}$, where $S$ is the serial fraction.<br>
If 5% of a program executes inside a synchronized block ($S = 0.05$), the program can never achieve more than a 20x speedup, regardless of whether you provision 64, 128, or 1000 CPU cores. Minimizing lock scope is essential to unlocking multi-core scalability.
</details>

<details>
<summary>5. What are the memory ordering modes provided by VarHandle in Java 9+?</summary>
<code>VarHandle</code> replaces `sun.misc.Unsafe` with explicit memory fences:
1. <b>Plain</b>: Standard read/write with zero memory barrier (ordinary non-volatile access).
2. <b>Opaque</b>: Guarantees bitwise atomicity and program order, but no cross-thread memory visibility ordering.
3. <b>Acquire / Release</b>: Guarantees that preceding writes are visible to subsequent reads without the full cost of a volatile store.
4. <b>Volatile</b>: Full sequentially consistent memory ordering with hardware memory barriers.
</details>

<details>
<summary>6. How does StampedLock provide optimistic read capabilities?</summary>
<code>StampedLock</code> provides an optimistic read mode via <code>tryOptimisticRead()</code> which returns a validation stamp without acquiring a mutual exclusion lock. The thread reads the required fields and then verifies whether the stamp is still valid via <code>validate(stamp)</code>. If no write lock was acquired in the interim, the read succeeds without memory barriers or lock overhead. If invalid, it falls back to a standard pessimistic read lock.
</details>

<details>
<summary>7. What is Lock Striping and where is it used in the JDK?</summary>
Lock Striping is the technique of replacing a single global lock with an array of independent locks, where operations acquire locks based on a hash of the target entity. This is the core architectural principle behind <code>ConcurrentHashMap</code> and <code>LongAdder</code>, enabling multiple threads to execute reads and writes concurrently as long as they target different hash stripes.
</details>

<details>
<summary>8. What is the purpose of Thread.onSpinWait()?</summary>
<code>Thread.onSpinWait()</code> is an intrinsic hint to the CPU that the current thread is executing a busy-wait spin-loop. On x86 processors, HotSpot compiles it to the <code>PAUSE</code> instruction. This optimizes CPU pipeline execution, prevents memory order violation penalties upon exiting the spin loop, and saves physical core power.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Write a high-performance thread-safe rate limiter using LongAdder.</summary>

```java
public final class StripedRateLimiter {
    private final LongAdder counter = new LongAdder();
    private final long maxLimitPerSecond;
    private volatile long currentSecondEpoch;

    public StripedRateLimiter(long maxLimitPerSecond) {
        this.maxLimitPerSecond = maxLimitPerSecond;
        this.currentSecondEpoch = System.currentTimeMillis() / 1000;
    }

    public boolean tryAcquire() {
        long currentSec = System.currentTimeMillis() / 1000;
        if (currentSec != currentSecondEpoch) {
            synchronized (this) {
                if (currentSec != currentSecondEpoch) {
                    currentSecondEpoch = currentSec;
                    counter.reset();
                }
            }
        }
        if (counter.sum() < maxLimitPerSecond) {
            counter.increment();
            return true;
        }
        return false;
    }
}
```
</details>

<details>
<summary>Exercise 2: Implement an optimistic read pattern using StampedLock.</summary>

```java
import java.util.concurrent.locks.StampedLock;

public class OptimisticPoint {
    private double x, y;
    private final StampedLock lock = new StampedLock();

    public double calculateDistanceFromOrigin() {
        long stamp = lock.tryOptimisticRead(); // Non-blocking optimistic check
        double curX = x, curY = y;
        if (!lock.validate(stamp)) { // If write occurred, fall back to read lock
            stamp = lock.readLock();
            try {
                curX = x;
                curY = y;
            } finally {
                lock.unlockRead(stamp);
            }
        }
        return Math.sqrt(curX * curX + curY * curY);
    }
}
```
</details>

<details>
<summary>Exercise 3: Write a JMH benchmark comparing AtomicLong vs LongAdder under 16 concurrent threads.</summary>

```java
@BenchmarkMode(Mode.Throughput)
@OutputTimeUnit(TimeUnit.MICROSECONDS)
@Threads(16)
@State(Scope.Benchmark)
public class AtomicVsLongAdderBenchmark {
    private AtomicLong atomicCounter = new AtomicLong();
    private LongAdder adderCounter = new LongAdder();

    @Benchmark
    public void incrementAtomic() {
        atomicCounter.incrementAndGet();
    }

    @Benchmark
    public void incrementAdder() {
        adderCounter.increment();
    }
}
```
</details>

<details>
<summary>Exercise 4: Test carrier thread unmounting with ReentrantLock vs Synchronized.</summary>

```java
public class CarrierPinningVerification {
    public static void main(String[] args) throws Exception {
        var lock = new java.util.concurrent.locks.ReentrantLock();
        Thread.ofVirtual().start(() -> {
            lock.lock();
            try {
                Thread.sleep(100); // Unmounts cleanly!
            } catch (InterruptedException ignored) {
            } finally {
                lock.unlock();
            }
        }).join();
        System.out.println("Carrier unmounting test complete.");
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Amdahl's Law**: Sequential fractions throttle multi-core scaling; minimize critical sections.
- **AtomicLong vs LongAdder**: Use `LongAdder` for write-heavy counters to eliminate MESI cache bouncing.
- **False Sharing**: Separate concurrently modified fields across 64-byte cache lines with `@Contended`.
- **Virtual Thread Safety**: Replace `synchronized` with `ReentrantLock` around blocking calls to prevent pinning.
- **Spin Loops**: Always invoke `Thread.onSpinWait()` in busy-wait loops to emit `PAUSE` instructions.
- **Lock Striping**: Partition locks by key hash to prevent global serialization bottlenecks.

---

## 13. Further Reading
- Martin Thompson: Mechanical Sympathy and False Sharing
- OpenJDK JEP 444: Virtual Threads (Carrier Pinning and Monitors)
- Doug Lea: Scalable Concurrent Counters and LongAdder Architecture
- *Java Concurrency in Practice* by Brian Goetz
