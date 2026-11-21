# Java Concurrency & Multithreading Reference Cheat Sheet

> Complete quick-reference for the Java Memory Model (JMM), thread lifecycle, synchronization primitives, lock-free atomics, CompletableFuture, Virtual Threads, and Structured Concurrency.

| Metadata | Details |
| :--- | :--- |
| **Level** | Comprehensive Reference |
| **Prerequisites** | [14-concurrency](../14-concurrency/README.md) |
| **Time to Read** | 40 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to 25 LTS |

---

## Table of Contents
1. [Java Memory Model (JMM) & Happens-Before Rules](#1-java-memory-model-jmm--happens-before-rules)
2. [Thread Lifecycle & State Transitions](#2-thread-lifecycle--state-transitions)
3. [Synchronizers: Latches, Barriers, Semaphores & Phasers](#3-synchronizers-latches-barriers-semaphores--phasers)
4. [Explicit Locks: ReentrantLock, ReadWriteLock & StampedLock](#4-explicit-locks-reentrantlock-readwritelock--stampedlock)
5. [Atomic Variables, CAS & High-Throughput Adders](#5-atomic-variables-cas--high-throughput-adders)
6. [Thread Pools & ExecutorService Architecture](#6-thread-pools--executorservice-architecture)
7. [CompletableFuture Asynchronous Programming API](#7-completablefuture-asynchronous-programming-api)
8. [Virtual Threads & Carrier Scheduling (Java 21+)](#8-virtual-threads--carrier-scheduling-java-21)
9. [Structured Concurrency & Scoped Values](#9-structured-concurrency--scoped-values)
10. [Thread-Safe Concurrent Collections](#10-thread-safe-concurrent-collections)
11. [False Sharing & Cache Contention Mitigation](#11-false-sharing--cache-contention-mitigation)
12. [Common Concurrency Anti-Patterns & Pitfalls](#12-common-concurrency-anti-patterns--pitfalls)
13. [Production Code Recipes](#13-production-code-recipes)

---

## 1. Java Memory Model (JMM) & Happens-Before Rules

The Java Memory Model (JLS §17.4) defines the semantics of memory actions across threads and the rules governing when memory writes by one thread become visible to reads by another.

### Happens-Before Rules Matrix
| Rule | Description |
| :--- | :--- |
| **Program Order Rule** | Each action in a single thread happens-before every action that comes later in program order. |
| **Monitor Lock Rule** | An unlock on a monitor lock happens-before every subsequent lock on that exact same monitor. |
| **Volatile Variable Rule** | A write to a `volatile` variable happens-before every subsequent read of that same variable. |
| **Thread Start Rule** | A call to `Thread.start()` happens-before any action in the started thread. |
| **Thread Termination Rule** | Any action in a thread happens-before any other thread successfully detects that thread has terminated (via `join()` or `isAlive() == false`). |
| **Interruption Rule** | A thread calling `interrupt()` on another thread happens-before the interrupted thread detects the interrupt. |
| **Default Initialization Rule** | Default values (`0`, `null`, `false`) are written to fields before any thread reads them. |
| **Transitivity Rule** | If action $A$ happens-before $B$, and $B$ happens-before $C$, then $A$ happens-before $C$. |

---

## 2. Thread Lifecycle & State Transitions

```
               [NEW]
                 |  t.start()
                 v
            [RUNNABLE] <-----------------------+
           /     |    \                        |
          /      |     \                       | Lock acquired /
         /       |      \                      | Time elapsed /
        v        v       v                     | Condition signaled
  [BLOCKED]  [WAITING]  [TIMED_WAITING]        |
        \        |       /                     |
         +-------+-------+---------------------+
                 |
                 v
            [TERMINATED]
```

### HotSpot Thread States (`java.lang.Thread.State`)
1. **`NEW`:** Thread created but `start()` has not been invoked.
2. **`RUNNABLE`:** Executing on the CPU or waiting for OS scheduler resource allocation.
3. **`BLOCKED`:** Waiting to acquire an intrinsic monitor lock (`synchronized` block).
4. **`WAITING`:** Waiting indefinitely for another thread: `Object.wait()`, `Thread.join()`, `LockSupport.park()`.
5. **`TIMED_WAITING`:** Waiting with a specified timeout: `Thread.sleep(ms)`, `Object.wait(ms)`, `LockSupport.parkNanos()`.
6. **`TERMINATED`:** Thread completed run execution or terminated abruptly by an unhandled exception.

---

## 3. Synchronizers: Latches, Barriers, Semaphores & Phasers

| Synchronizer | Reset / Reusable? | Primary Use Case | Key Methods |
| :--- | :---: | :--- | :--- |
| **`CountDownLatch`** | **No** (One-shot) | Waiting for $N$ tasks to complete before proceeding | `await()`, `countDown()` |
| **`CyclicBarrier`** | **Yes** (Reusable) | Synchronizing $N$ threads at a common rendezvous point | `await()`, `reset()` |
| **`Semaphore`** | N/A (Permit-based) | Throttling access to a bounded resource pool | `acquire()`, `release()` |
| **`Phaser`** | **Yes** (Dynamic) | Multi-phase coordination with variable party count | `arriveAndAwaitAdvance()`, `register()` |
| **`Exchanger`** | **Yes** | Bidirectional point-to-point data exchange between 2 threads | `exchange(V x)` |

---

## 4. Explicit Locks: ReentrantLock, ReadWriteLock & StampedLock

### 1. `ReentrantLock`
Supports explicit acquisition, fairness policies, interruptible lock acquisition, and timed lock acquisition:
```java
Lock lock = new ReentrantLock(true); // true = Fair ordering policy
lock.lock();
try {
    // Critical Section
} finally {
    lock.unlock(); // Always release in finally block
}
```

### 2. `ReentrantReadWriteLock`
Allows concurrent read access while restricting write operations to mutual exclusion:
```java
ReadWriteLock rwLock = new ReentrantReadWriteLock();
Lock readLock = rwLock.readLock();
Lock writeLock = rwLock.writeLock();
```

### 3. `StampedLock` (Java 8+)
High-performance lock supporting **Optimistic Reading** without acquiring read locks:
```java
StampedLock sl = new StampedLock();

// Optimistic Read Pattern
long stamp = sl.tryOptimisticRead();
double currentX = x, currentY = y;
if (!sl.validate(stamp)) { // Check if write lock was acquired in the meantime
    stamp = sl.readLock(); // Fallback to pessimistic read lock
    try {
        currentX = x;
        currentY = y;
    } finally {
        sl.unlockRead(stamp);
    }
}
```

---

## 5. Atomic Variables, CAS & High-Throughput Adders

### Compare-And-Swap (CAS)
A hardware CPU instruction (`cmpxchg` on x86-64) that updates a memory location only if it currently holds an expected value, avoiding operating system mutex stalls.

```java
// AtomicInteger CAS loop
public final int incrementAndGet(AtomicInteger ai) {
    int current;
    do {
        current = ai.get();
    } while (!ai.compareAndSet(current, current + 1));
    return current + 1;
}
```

### `LongAdder` vs `AtomicLong` under Heavy Contention
`AtomicLong` forces all contending threads to spin on a single cache line, causing massive interconnect invalidations.
`LongAdder` dynamically stripes state across an array of `Cell` objects padded with `@Contended`:
```java
LongAdder counter = new LongAdder();
counter.increment();
long total = counter.sum(); // Aggregates cell values
```

---

## 6. Thread Pools & ExecutorService Architecture

```
                       ThreadPoolExecutor
                               |
  Tasks ---> [Core Threads Pool (corePoolSize)]
                    | (If all core threads busy)
                    v
             [Blocking Work Queue]
                    | (If work queue full)
                    v
        [Maximum Threads Pool (maximumPoolSize)]
                    | (If max threads busy & queue full)
                    v
            [RejectedExecutionHandler]
```

### Standard Rejection Policies
1. `AbortPolicy` (Default): Throws `RejectedExecutionException`.
2. `CallerRunsPolicy`: Executes the task directly on the calling producer thread (built-in backpressure!).
3. `DiscardPolicy`: Silently drops the rejected task.
4. `DiscardOldestPolicy`: Drops the oldest unprocessed task in the queue and retries submission.

---

## 7. CompletableFuture Asynchronous Programming API

| Stage Type | Same Thread / Immediate | Dedicated Async Pool (`ForkJoinPool.commonPool()`) | Custom Executor Async |
| :--- | :--- | :--- | :--- |
| **Transform** | `.thenApply(fn)` | `.thenApplyAsync(fn)` | `.thenApplyAsync(fn, executor)` |
| **Consume** | `.thenAccept(consumer)` | `.thenAcceptAsync(consumer)` | `.thenAcceptAsync(consumer, executor)` |
| **Compose** | `.thenCompose(fn)` | `.thenComposeAsync(fn)` | `.thenComposeAsync(fn, executor)` |
| **Combine 2**| `.thenCombine(other, fn)`| `.thenCombineAsync(other, fn)` | `.thenCombineAsync(other, fn, exec)` |

```java
CompletableFuture<String> pipeline = CompletableFuture.supplyAsync(() -> fetchUserId())
        .thenComposeAsync(userId -> fetchUserProfile(userId))
        .thenApply(profile -> profile.displayName())
        .exceptionally(ex -> "Fallback User");
```

---

## 8. Virtual Threads & Carrier Scheduling (Java 21+)

Virtual threads (JEP 444) are lightweight user-mode threads managed by the HotSpot runtime rather than the operating system.

```java
// 1. Create and start a single virtual thread
Thread vThread = Thread.ofVirtual().name("v-worker").start(() -> {
    System.out.println("Running in virtual thread: " + Thread.currentThread());
});

// 2. Virtual thread per task executor
try (var executor = Executors.newVirtualThreadPerTaskExecutor()) {
    IntStream.range(0, 10_000).forEach(i -> executor.submit(() -> {
        Thread.sleep(Duration.ofMillis(100)); // Unmounts from carrier thread!
        return i;
    }));
} // Automatically awaits completion of all virtual threads upon try-block exit
```

### Pinning Warning (Prior to Java 24):
In Java 21–23, executing blocking I/O inside a `synchronized` block or native method pins the virtual thread to its underlying OS carrier thread. Replace `synchronized` with `ReentrantLock` for blocking critical sections.

---

## 9. Structured Concurrency & Scoped Values

### Structured Concurrency (`StructuredTaskScope` - Preview)
Treats multiple tasks running in separate threads as a single unit of work, preventing thread leaks and simplifying error handling:

```java
// StructuredTaskScope.ShutdownOnFailure cancels remaining tasks if any task fails
try (var scope = new StructuredTaskScope.ShutdownOnFailure()) {
    Supplier<User> userSub = scope.fork(() -> fetchUser(id));
    Supplier<Order> orderSub = scope.fork(() -> fetchOrder(id));

    scope.join();           // Join both forks
    scope.throwIfFailed();  // Propagate exception if either failed

    return new Response(userSub.get(), orderSub.get());
}
```

---

## 10. Thread-Safe Concurrent Collections

| Class | Internal Locking Strategy | Typical Use Case |
| :--- | :--- | :--- |
| `ConcurrentHashMap` | Synchronized bucket bins + CAS | Concurrent key-value cache |
| `CopyOnWriteArrayList` | ReentrantLock on mutation; immutable snapshots on read | Infrequent writes, massive concurrent reads |
| `ConcurrentLinkedQueue` | Unbounded lock-free M&S queue algorithm | High-throughput async task queue |
| `ArrayBlockingQueue` | Single ReentrantLock + two Condition variables | Bounded producer-consumer pipelines |
| `LinkedBlockingQueue` | Two separate ReentrantLocks (takeLock & putLock) | Dual-lock producer-consumer queue |

---

## 11. False Sharing & Cache Contention Mitigation

When multiple CPU cores write to independent variables located in the same 64-byte L1/L2 cache line, hardware cache coherency protocols (`MESI`) continuously invalidate cache lines across cores.

```java
// Pad fields across cache line boundaries using @Contended
import jdk.internal.vm.annotation.Contended;

public class CachePaddedCounter {
    @Contended
    public volatile long countA;

    @Contended
    public volatile long countB;
}
```
*Note: Enable with `-XX:-RestrictContended` on command line.*

---

## 12. Common Concurrency Anti-Patterns & Pitfalls

| Anti-Pattern | Root Cause | Remediated Code |
| :--- | :--- | :--- |
| **Double-Checked Locking without `volatile`** | Compiler instruction reordering allows threads to see partially initialized objects. | Declare target singleton field as `private static volatile Instance INSTANCE;` |
| **Busy-Waiting Spin Loops** | `while (!flag) {}` consumes 100% CPU on mutator core without yielding. | Use `Thread.onSpinWait()`, `LockSupport.park()`, or synchronizers. |
| **Unbounded Cached Thread Pools** | `Executors.newCachedThreadPool()` spawns unlimited OS platform threads. | Use bounded `ThreadPoolExecutor` or `newVirtualThreadPerTaskExecutor()`. |
| **Lock Ordering Deadlock** | Threads acquire Lock A and Lock B in inconsistent order. | Enforce global canonical lock ordering (e.g. acquire locks sorted by ID). |

---

## 13. Production Code Recipes

### Recipe 1: Thread-Safe Lazy Initializer (Holder Pattern)
```java
public class BillPughSingleton {
    private BillPughSingleton() {}

    private static class LazyHolder {
        static final BillPughSingleton INSTANCE = new BillPughSingleton();
    }

    public static BillPughSingleton getInstance() {
        return LazyHolder.INSTANCE; // Class loaded and initialized safely by JVM <clinit>
    }
}
```

### Recipe 2: Throttling Concurrent Operations with Semaphore
```java
public class BoundedApiExecutor {
    private final Semaphore semaphore = new Semaphore(10); // Max 10 concurrent requests

    public <T> T executeWithThrottle(Callable<T> task) throws Exception {
        semaphore.acquire();
        try {
            return task.call();
        } finally {
            semaphore.release();
        }
    }
}
```

---

## 14. AbstractQueuedSynchronizer (AQS) Deep Dive

`AbstractQueuedSynchronizer` (AQS) is the foundational framework behind `ReentrantLock`, `Semaphore`, `CountDownLatch`, and `ReentrantReadWriteLock`.

```
                    +---------------------------+
                    |        head (Dummy)       |
                    +---------------------------+
                                  |
                                  v  prev / next pointers
                    +---------------------------+
                    |   Node: Thread 1 (WAIT)   |
                    +---------------------------+
                                  |
                                  v
                    +---------------------------+
                    |   Node: Thread 2 (WAIT)   |
                    +---------------------------+
                                  ^
                                  | tail
```

### AQS Core State & Node Wait Statuses
- **`state`:** A 32-bit volatile integer representing synchronization state (e.g., hold count in `ReentrantLock`, permit count in `Semaphore`).
- **Node Wait Statuses:**
  - `0`: Normal initial node state.
  - `SIGNAL (-1)`: Successor thread is parked and must be unparked upon release.
  - `CANCELLED (1)`: Thread cancelled due to timeout or interruption.
  - `CONDITION (-2)`: Node is currently waiting on a `ConditionObject` queue.
  - `PROPAGATE (-3)`: Shared release must be propagated to other nodes (in `Semaphore` / `CountDownLatch`).

---

## 15. Lock-Free Data Structure Implementations

### Lock-Free Treiber Stack
```java
package com.engineering.concurrency;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;

public class TreiberStack<E> {
    private static class Node<E> {
        final E item;
        Node<E> next;
        Node(E item) { this.item = item; }
    }

    private volatile Node<E> head;
    private static final VarHandle HEAD;

    static {
        try {
            HEAD = MethodHandles.lookup().findVarHandle(TreiberStack.class, "head", Node.class);
        } catch (ReflectiveOperationException e) {
            throw new ExceptionInInitializerError(e);
        }
    }

    public void push(E item) {
        Node<E> newHead = new Node<>(item);
        Node<E> oldHead;
        do {
            oldHead = head;
            newHead.next = oldHead;
        } while (!HEAD.compareAndSet(this, oldHead, newHead));
    }

    public E pop() {
        Node<E> oldHead;
        Node<E> newHead;
        do {
            oldHead = head;
            if (oldHead == null) return null;
            newHead = oldHead.next;
        } while (!HEAD.compareAndSet(this, oldHead, newHead));
        return oldHead.item;
    }
}
```

---

## 16. ForkJoinPool & Work-Stealing Internals

The `ForkJoinPool` operates on the **Work-Stealing Algorithm**:
- Each worker thread maintains its own double-ended queue (deque) of tasks.
- When a worker pushes subtasks via `fork()`, it pushes them onto the **head** of its own deque (LIFO order, maximizing CPU cache locality).
- When a worker finishes all its tasks, it steals tasks from the **tail** of another worker's deque (FIFO order, minimizing lock contention with the victim thread).

```java
public class FibonacciTask extends RecursiveTask<Long> {
    private final long n;
    public FibonacciTask(long n) { this.n = n; }

    @Override
    protected Long compute() {
        if (n <= 1) return n;
        FibonacciTask f1 = new FibonacciTask(n - 1);
        f1.fork(); // Asynchronously submit subtask to local deque
        FibonacciTask f2 = new FibonacciTask(n - 2);
        return f2.compute() + f1.join(); // Compute f2 locally and await f1
    }
}
```

---

## 17. Concurrency Mathematics & Sizing Laws

### Amdahl's Law (Speedup Limit)
$$S_{\text{latency}}(s) = \frac{1}{(1 - p) + \frac{p}{s}}$$
Where $p$ is the fraction of parallelizable execution, and $s$ is the number of processor cores.
*Implication:* If 10% of code is strictly serial ($1 - p = 0.10$), theoretical speedup can **never exceed 10x**, regardless of how many CPU cores are added.

### Platform Thread Pool Sizing Formula
$$N_{\text{threads}} = N_{\text{CPUs}} \times U_{\text{CPU}} \times \left(1 + \frac{W}{C}\right)$$
- $N_{\text{CPUs}}$: Available CPU cores (`Runtime.getRuntime().availableProcessors()`).
- $U_{\text{CPU}}$: Target CPU utilization ($0.0$ to $1.0$).
- $W/C$: Ratio of Wait time (network/disk latency) to Compute time.
