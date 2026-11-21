# Interview Mastery: Concurrency, Multithreading & Virtual Threads

> Comprehensive technical interview guide for Java Concurrency: Java Memory Model (JMM) happens-before invariants, lock inflation, Virtual Threads (Project Loom), CAS atomics, and Structured Concurrency.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_core-java.md](00_core-java.md), [14-concurrency](../14-concurrency/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to Java 25 LTS |

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

Concurrency is the most rigorous and differentiating domain in Java technical interviews. Senior engineers must demonstrate mastery over **hardware memory caches, CPU instruction reordering, lock contention, and asynchronous coordination**.

Key interview focuses include:
- The **Java Memory Model (JMM, JSR-133)**: What *happens-before* guarantees actually mean.
- The precise mechanics of **`volatile`**: Memory barriers vs CPU cache coherence.
- How HotSpot inflates locks from **Biased** $\to$ **Lightweight (CAS)** $\to$ **Heavyweight (OS Mutex)**.
- The revolutionary architecture of **Virtual Threads (JEP 444, Java 21 LTS)** and how they eliminate reactive spaghetti code.
- Preventing and debugging **Deadlocks, Livelocks, and Thread Starvation**.

---

## 2. Core Concepts

### 1. The Java Memory Model (JMM) & Happens-Before Order
CPUs execute instructions out of order and maintain independent L1/L2/L3 cache hierarchies. The JMM defines formal rules ensuring when a write by one thread is guaranteed to be visible to a read by another thread:
- **Program Order Rule**: Each action in a single thread happens-before every action that comes later in program order.
- **Monitor Lock Rule**: An unlock on a monitor happens-before every subsequent lock on that same monitor.
- **Volatile Variable Rule**: A write to a `volatile` field happens-before every subsequent read of that same field.
- **Thread Start / Join Rule**: A call to `thread.start()` happens-before any action in the started thread; any action in a thread happens-before `thread.join()` returns.
- **Transitivity**: If $A \to B$ and $B \to C$, then $A \to C$.

### 2. Platform Threads vs Virtual Threads (Project Loom, Java 21+)
- **Platform Thread (`java.lang.Thread`)**: A 1:1 wrapper around an OS kernel thread. Costs ~1MB of reserved memory (stack space). Max limit: ~5,000 to 10,000 threads per JVM.
- **Virtual Thread (JEP 444)**: A lightweight user-mode thread managed directly by the JVM. Costs **under 1KB of heap memory**. Max limit: **millions of concurrent threads**.
- When a virtual thread performs blocking I/O (e.g. `Socket.read()`, `Thread.sleep()`), the JVM unmounts it from the OS carrier thread and parks its continuation state in heap memory, allowing the carrier thread to execute other virtual threads!

---

## 3. How It Works Internally

### The Mechanics of `volatile`
A `volatile` variable provides **Visibility** and **Ordering**, but **NOT Atomicity**:
```
Thread 1 (Core 1)                          Thread 2 (Core 2)
Write: volatileVar = 42;                    Read: int x = volatileVar;
        |                                           ^
        v                                           |
[StoreStore Barrier]                       [LoadLoad Barrier]
[StoreLoad Barrier]                        [LoadStore Barrier]
Forces CPU to flush store buffer           Invalidates Core 2 L1 Cache;
directly to main memory / L3 cache.        Forces reload from cache coherence line.
```
- Prevents the compiler and CPU from reordering instructions across the barrier.
- **No Atomicity**: Compound operations like `count++` (`read -> modify -> write`) are NOT atomic on volatile fields!

### HotSpot Object Monitor & Lock Inflation
Every Java object header contains a **Mark Word** tracking synchronization state:
1. **Unlocked**: Object mark word holds raw identity hash code.
2. **Biased Locking (Deprecated in Java 15+)**: Biased to the first thread that acquires it with zero CAS overhead.
3. **Lightweight Locking**: Thread allocates a BasicObjectLock on its thread stack and attempts a hardware CAS to swap the mark word. If uncontended, takes ~5ns.
4. **Heavyweight Locking**: If threads contend (CAS fails), HotSpot inflates the lock into an OS mutex (`ObjectMonitor`) with `_WaitSet` and `_EntryList`, parking contending threads via OS kernel `futex` calls.

---

## 4. Syntax & Usage

### Modern Virtual Thread Executor & CompletableFuture

```java
import java.util.concurrent.*;

public class ModernConcurrencySyntax {
    public static void main(String[] args) throws Exception {
        // 1. Virtual Thread-per-Task Executor (Java 21+)
        try (var executor = Executors.newVirtualThreadPerTaskExecutor()) {
            Future<String> f1 = executor.submit(() -> {
                Thread.sleep(100); // Parks virtual thread; frees carrier thread!
                return "Task 1 Result";
            });

            Future<String> f2 = executor.submit(() -> {
                Thread.sleep(100);
                return "Task 2 Result";
            });

            System.out.println(f1.get() + " & " + f2.get());
        } // AutoCloseable executor awaits all tasks before exiting!

        // 2. Asynchronous Pipeline with CompletableFuture
        CompletableFuture.supplyAsync(() -> "User_123")
            .thenApplyAsync(userId -> "Profile_for_" + userId)
            .thenAccept(System.out::println)
            .join();
    }
}
```

---

## 5. Practical Examples

### Example 1: Thread-Safe Double-Checked Locking (DCL) Singleton

```java
public class DCLSingleton {
    // CRITICAL: Must be volatile to prevent instruction reordering!
    private static volatile DCLSingleton instance;

    private DCLSingleton() {
        // Prevent reflection instantiation
        if (instance != null) {
            throw new IllegalStateException("Already instantiated");
        }
    }

    public static DCLSingleton getInstance() {
        DCLSingleton localRef = instance; // Local read optimization
        if (localRef == null) {
            synchronized (DCLSingleton.class) {
                localRef = instance;
                if (localRef == null) {
                    // Without volatile, the JVM could reorder:
                    // 1. Allocate memory -> 2. Point reference -> 3. Run constructor
                    // Another thread could read a partially initialized object!
                    instance = localRef = new DCLSingleton();
                }
            }
        }
        return localRef;
    }
}
```

### Example 2: Programmatic Deadlock Detection using `ThreadMXBean`

```java
import java.lang.management.ManagementFactory;
import java.lang.management.ThreadMXBean;

public class DeadlockDetector {
    private static final Object LOCK_A = new Object();
    private static final Object LOCK_B = new Object();

    public static void main(String[] args) {
        // Thread 1: Locks A then B
        Thread t1 = new Thread(() -> {
            synchronized (LOCK_A) {
                try { Thread.sleep(50); } catch (InterruptedException e) {}
                synchronized (LOCK_B) {}
            }
        }, "Worker-1");

        // Thread 2: Locks B then A (Circular wait!)
        Thread t2 = new Thread(() -> {
            synchronized (LOCK_B) {
                try { Thread.sleep(50); } catch (InterruptedException e) {}
                synchronized (LOCK_A) {}
            }
        }, "Worker-2");

        t1.start();
        t2.start();

        // Diagnostic detection thread
        Thread detector = new Thread(() -> {
            try { Thread.sleep(200); } catch (InterruptedException e) {}
            ThreadMXBean bean = ManagementFactory.getThreadMXBean();
            long[] deadlockedThreads = bean.findDeadlockedThreads();

            if (deadlockedThreads != null && deadlockedThreads.length > 0) {
                System.err.println("DEADLOCK DETECTED! Thread IDs involved: " + deadlockedThreads.length);
                for (long id : deadlockedThreads) {
                    System.err.println("  Deadlocked Thread: " + bean.getThreadInfo(id).getThreadName());
                }
            }
        });
        detector.start();
    }
}
```

### Example 3: Producer-Consumer with `ReentrantLock` and Dual `Condition`

```java
import java.util.ArrayDeque;
import java.util.Queue;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

public class BoundedBuffer<T> {
    private final Queue<T> queue = new ArrayDeque<>();
    private final int capacity;
    private final ReentrantLock lock = new ReentrantLock();
    private final Condition notFull = lock.newCondition();
    private final Condition notEmpty = lock.newCondition();

    public BoundedBuffer(int capacity) {
        this.capacity = capacity;
    }

    public void put(T item) throws InterruptedException {
        lock.lock();
        try {
            while (queue.size() == capacity) {
                notFull.await(); // Releases lock and waits for consumer
            }
            queue.offer(item);
            notEmpty.signal(); // Wakes up waiting consumer
        } finally {
            lock.unlock();
        }
    }

    public T take() throws InterruptedException {
        lock.lock();
        try {
            while (queue.isEmpty()) {
                notEmpty.await(); // Releases lock and waits for producer
            }
            T item = queue.poll();
            notFull.signal(); // Wakes up waiting producer
            return item;
        } finally {
            lock.unlock();
        }
    }
}
```

### Example 4: Asynchronous Fan-Out & Aggregation with `CompletableFuture`

```java
import java.util.List;
import java.util.concurrent.CompletableFuture;

public class AsyncFanOut {
    public record ServiceResponse(String service, long latencyMs) {}

    public static CompletableFuture<ServiceResponse> callService(String name, long delay) {
        return CompletableFuture.supplyAsync(() -> {
            try { Thread.sleep(delay); } catch (InterruptedException e) {}
            return new ServiceResponse(name, delay);
        });
    }

    public static void main(String[] args) {
        var future1 = callService("AuthService", 120);
        var future2 = callService("CatalogService", 80);
        var future3 = callService("PricingService", 150);

        // Aggregate all futures concurrently
        CompletableFuture.allOf(future1, future2, future3)
            .thenApply(v -> List.of(future1.join(), future2.join(), future3.join()))
            .thenAccept(responses -> {
                System.out.println("All services responded:");
                responses.forEach(r -> System.out.println("  " + r.service() + ": " + r.latencyMs() + "ms"));
            })
            .join();
    }
}
```

### Example 5: Virtual Threads Carrier Pinning Trap

```java
public class PinningTrapDemo {
    private static final Object MONITOR = new Object();

    public static void main(String[] args) throws Exception {
        // PINNING HAZARD: In Java 21-23, executing blocking I/O inside synchronized(MONITOR)
        // 'pins' the virtual thread to its underlying OS carrier thread!
        // The carrier thread cannot be reused while waiting!
        // Java 24 (JEP 491) eliminated pinning for synchronized blocks!

        Thread vThread = Thread.ofVirtual().start(() -> {
            synchronized (MONITOR) {
                try {
                    Thread.sleep(100); // Fixed in Java 24+; previously pinned carrier thread!
                } catch (InterruptedException e) {}
            }
        });

        vThread.join();
        System.out.println("Virtual thread completed successfully.");
    }
}
```

---

## 6. Best Practices

1. **Always Use `volatile` with Double-Checked Locking**: Without `volatile`, instruction reordering allows threads to see non-null references to partially constructed objects.
2. **Never Synchronize on Reusable Literals**: Never write `synchronized("myLock")` or `synchronized(Integer.valueOf(1))`. Strings and boxed integers are cached and shared globally across the entire JVM.
3. **Always Acquire Locks in a Uniform Global Order**: If two threads need locks $A$ and $B$, both threads must acquire $A$ before $B$. Inverting the order guarantees deadlocks under load.
4. **Prefer `ReentrantLock` for Advanced Features**: Use `ReentrantLock` when you need timed lock acquisition (`tryLock(timeout)`), multiple conditions (`Condition`), or interruptible lock acquisition.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Assuming `volatile` makes `count++` thread-safe | `count++` is 3 operations (read, modify, write); race condition occurs | Use `AtomicInteger` or `LongAdder`. |
| Calling `.get()` on `CompletableFuture` synchronously in web threads | Blocks thread; converts asynchronous pipeline back into blocking synchronous | Chain callbacks using `.thenApply()` and `.thenAccept()`. |
| Thread pool starvation with nested tasks | Parent tasks in pool wait for sub-tasks submitted to the same pool; all threads blocked | Use Virtual Threads or separate dedicated pools for child tasks. |
| Forgetting `lock.unlock()` in a `finally` block | If an exception occurs, the lock is never released, causing permanent system freeze | Always wrap in `try { lock.lock(); ... } finally { lock.unlock(); }`. |
| Unhandled `InterruptedException` (`catch (InterruptedException e) {}`) | Swallows thread cancellation signal | Restore interrupt status: `Thread.currentThread().interrupt();`. |

---

## 8. Performance & Security Notes

- **High-Contention Counters**: For high-throughput metrics and counters updated by dozens of threads, prefer `LongAdder` over `AtomicLong`. `LongAdder` stripes cell counters across multiple cache lines, eliminating CPU cache bouncing and CAS contention.
- **False Sharing**: When two independent variables accessed by different threads reside on the same 64-byte CPU cache line, updating one invalidates the other core's cache line. Use `@jdk.internal.vm.annotation.Contended` to pad fields with empty bytes.
- **Carrier Thread Exhaustion**: Running compute-heavy (CPU-bound) algorithms on millions of Virtual Threads provides zero speedup; Virtual Threads shine exclusively for I/O-bound concurrency.

---

## 9. Comparison / When to Use What

| Mechanism | Visibility | Atomicity | Lock Overhead | Best For |
|---|---|---|---|---|
| **`volatile`** | Yes (happens-before) | No (compound ops fail)| Zero (hardware barrier) | State flags, DCL singleton |
| **`AtomicInteger`** | Yes | Yes (hardware CAS) | Extremely low (lock-free) | Unique IDs, sequence numbers |
| **`synchronized`** | Yes | Yes | Low to High (inflates to OS mutex) | Simple critical sections |
| **`ReentrantLock`** | Yes | Yes | Low to High | `tryLock()`, timeouts, conditions |
| **`Virtual Threads`**| Yes | Managed via standard locks| Negligible heap footprint | High-concurrency I/O services |

---

## 10. Interview Questions

<details>
<summary>1. Why is the `volatile` keyword required in the Double-Checked Locking singleton pattern?</summary>

The statement `instance = new Singleton()` is not an atomic operation. The CPU executes it in three steps:
1. Allocate memory on the heap.
2. Initialize the object by executing the constructor.
3. Assign the memory address to `instance`.
Without `volatile`, the compiler and CPU can reorder steps 2 and 3 (allocate $\to$ assign pointer $\to$ construct). If Thread A executes step 1 and 3, and context switches before step 2, Thread B can call `getInstance()`, observe `instance != null`, and access a partially initialized, corrupt object. `volatile` establishes a memory barrier that prohibits this reordering.
</details>

<details>
<summary>2. How do Virtual Threads differ from Reactive Programming (e.g. Project Reactor, RxJava)?</summary>

Both aim to handle high-concurrency I/O without consuming an OS thread per connection.
- **Reactive Programming** achieves this via non-blocking callbacks, but destroys standard control flow: stack traces become unreadable, step-by-step debugging fails, and code is forced into complex functional pipelines (`mono.flatMap()`).
- **Virtual Threads** allow developers to write standard synchronous, blocking code (`try-catch`, standard loops, clean stack traces) while the JVM handles thread suspension under the hood. It makes reactive frameworks obsolete for standard I/O services.
</details>

<details>
<summary>3. What is the difference between `synchronized` and `ReentrantLock`?</summary>

1. **Syntax**: `synchronized` is a language-level construct that automatically unlocks on exit or exception. `ReentrantLock` is an API class that requires explicit `lock()` and `finally { unlock(); }`.
2. **Fairness**: `ReentrantLock` supports an optional fairness parameter (`new ReentrantLock(true)`) to grant access in FIFO order. `synchronized` is strictly non-fair.
3. **Advanced Features**: `ReentrantLock` supports non-blocking polling (`tryLock()`), interruptible acquisition (`lockInterruptibly()`), and multiple wait conditions via `Condition`.
</details>

<details>
<summary>4. What is Lock Contention and how does HotSpot handle lock inflation?</summary>

Lock contention occurs when a thread attempts to acquire a monitor currently held by another thread.
- If uncontended, HotSpot uses **Lightweight Locking**, using a single CAS operation on the object's Mark Word.
- If contention occurs, HotSpot spins briefly via adaptive spinning. If the lock is not freed, it **inflates** the lock into a **Heavyweight Lock** (`ObjectMonitor`), allocating an OS synchronization primitive (mutex/futex) and suspending the contending thread into kernel sleep.
</details>

<details>
<summary>5. What is the difference between `CountDownLatch` and `CyclicBarrier`?</summary>

- **`CountDownLatch`**: A one-time synchronization barrier initialized with count $N$. Threads call `countDown()` to decrement; other threads await until count reaches 0. **It cannot be reset or reused**.
- **`CyclicBarrier`**: Synchronizes a fixed number of threads ($N$) at a common barrier point. All $N$ threads call `barrier.await()`, and none proceed until all $N$ arrive. **It can be reset and reused cyclically**.
</details>

<details>
<summary>6. Why is `LongAdder` preferred over `AtomicLong` under high thread contention?</summary>

`AtomicLong` relies on a single shared memory location updated via a CAS loop. Under high contention across 64 CPU cores, 63 threads fail their CAS on every cycle and spin redundantly, causing severe CPU cache coherence traffic. `LongAdder` stripes the count across a dynamic array of internal `Cell` objects; threads hash to different cells and update without contention. The final sum aggregates the cells.
</details>

<details>
<summary>7. What is Virtual Thread Pinning and how was it addressed in Java 24 (JEP 491)?</summary>

In Java 21 through 23, if a Virtual Thread executed blocking I/O inside a `synchronized` block or method, or inside a native JNI call, it was **pinned** to its carrier OS thread, preventing the carrier thread from being unmounted. In Java 24, **JEP 491 (Synchronize Virtual Threads without Pinning)** completely redesigned object monitors to allow virtual threads to unmount freely during blocking operations inside `synchronized` blocks.
</details>

<details>
<summary>8. How do you gracefully shut down an `ExecutorService`?</summary>

Follow the standard two-phase shutdown protocol:
```java
executor.shutdown(); // Disable new tasks
try {
    if (!executor.awaitTermination(60, TimeUnit.SECONDS)) {
        executor.shutdownNow(); // Cancel executing tasks via interrupt
        if (!executor.awaitTermination(60, TimeUnit.SECONDS)) {
            System.err.println("Pool did not terminate");
        }
    }
} catch (InterruptedException ie) {
    executor.shutdownNow();
    Thread.currentThread().interrupt();
}
```
</details>

---

## 11. Practice Exercises

### Exercise 1: Implement a Thread-Safe Bounded Blocking Queue
Implement a blocking queue from scratch using only `wait()` and `notifyAll()`.

<details>
<summary>Solution</summary>

```java
import java.util.LinkedList;
import java.util.Queue;

public class ClassicBlockingQueue<T> {
    private final Queue<T> queue = new LinkedList<>();
    private final int capacity;

    public ClassicBlockingQueue(int capacity) {
        this.capacity = capacity;
    }

    public synchronized void put(T item) throws InterruptedException {
        while (queue.size() == capacity) {
            wait(); // Must be in a while loop to guard against spurious wakeups!
        }
        queue.add(item);
        notifyAll(); // Wake up potential waiting consumers
    }

    public synchronized T take() throws InterruptedException {
        while (queue.isEmpty()) {
            wait();
        }
        T item = queue.remove();
        notifyAll(); // Wake up potential waiting producers
        return item;
    }
}
```
</details>

### Exercise 2: Diagnose the Missing Volatile Bug
Why can this program loop infinitely on multi-core hardware, even after `stop()` is called?
```java
public class Worker implements Runnable {
    private boolean running = true;
    public void run() {
        while (running) {
            // Hot loop
        }
        System.out.println("Worker stopped.");
    }
    public void stop() { running = false; }
}
```

<details>
<summary>Solution</summary>

**Cause**: The field `running` is not `volatile`. The JIT compiler optimizes the loop by caching `running` inside a CPU register (hoisting: `if (running) while(true);`). When another thread calls `stop()`, Core 2's store buffer is not flushed to main memory, and Core 1 continues reading its local cached register forever.

**Fix**: Declare the flag as `private volatile boolean running = true;`.
</details>

### Exercise 3: Parallel Web Crawler with Virtual Threads
Implement a concurrent crawler task using Java 21 Virtual Threads that limits concurrency to 50 simultaneous requests using a `Semaphore`.

<details>
<summary>Solution</summary>

```java
import java.util.List;
import java.util.concurrent.*;

public class RateLimitedCrawler {
    private final Semaphore throttle = new Semaphore(50);

    public void crawl(List<String> urls) {
        try (var executor = Executors.newVirtualThreadPerTaskExecutor()) {
            for (String url : urls) {
                executor.submit(() -> {
                    throttle.acquire();
                    try {
                        fetchUrl(url);
                    } finally {
                        throttle.release();
                    }
                    return null;
                });
            }
        } // Awaits termination of all virtual threads
    }

    private void fetchUrl(String url) {
        // Simulated I/O call
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **JMM Happens-Before**: Ensures visibility and instruction ordering across memory barriers.
- **Volatile**: Guarantees visibility and ordering; zero atomicity for compound mutations.
- **Virtual Threads (Java 21+)**: Millions of user-mode threads; blocking I/O unmounts carrier threads.
- **Atomic Operations**: Hardware CAS loops; `LongAdder` eliminates contention across multi-core CPUs.
- **Lock Invariant**: Always lock in identical sequence; always unlock in a `finally` block.

---

## 13. Further Reading

- [Brian Goetz et al.: Java Concurrency in Practice](https://jcip.net/)
- [JEP 444: Virtual Threads (Java 21 LTS)](https://openjdk.org/jeps/444)
- [JEP 491: Synchronize Virtual Threads without Pinning (Java 24)](https://openjdk.org/jeps/491)
- [Aleksey Shipilëv: Java Memory Model Pragmatics](https://shipilev.net/blog/2014/jmm-pragmatics/)
