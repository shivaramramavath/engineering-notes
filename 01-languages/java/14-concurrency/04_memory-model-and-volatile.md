# Java Memory Model and `volatile`

> Comprehensive architectural reference for the Java Memory Model (JSR-133): the Happens-Before relationship, `volatile` memory semantics, hardware memory barriers (LoadLoad, StoreStore, LoadStore, StoreLoad), instruction reordering, and safe publication.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [02_thread-safety.md](02_thread-safety.md), [03_synchronization.md](03_synchronization.md) |
| **Time to Read** | 28 minutes |
| **Status** | Active |
| **Java Version** | Java 5.0+ (JSR-133 JMM Revision) |

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

### The Hardware Reality vs Programmer Expectations
In single-threaded software, developers expect code to execute sequentially in the exact order written.
However, modern CPU architectures (x86, ARM, RISC-V) and optimizing compilers (like HotSpot C2) aggressively reorder instructions, buffer memory writes in store buffers, and read from local L1/L2 caches to maximize instruction pipelining and memory bus throughput.

Without a formal memory specification, concurrent threads on multi-core processors observe memory updates in unpredictable orders, leading to catastrophic race conditions, phantom values, and infinite loops.

### The Solution: JSR-133 Java Memory Model (JMM)
Finalized in Java 5, the **Java Memory Model (JMM)** defines the formal specification governing how threads interact through shared memory. It specifies the exact conditions under which a write to a variable by one thread is guaranteed to be **visible** to a read by another thread.

---

## 2. Core Concepts

### The Happens-Before Relationship
The central abstraction of the JMM is the **Happens-Before** order. If action $A$ happens-before action $B$ ($A \xrightarrow{hb} B$), then the memory modifications of $A$ are guaranteed to be completely visible to $B$, and $A$ is ordered before $B$.

Key Happens-Before Rules:
1. **Program Order Rule**: Each action in a single thread happens-before every action in that thread that comes later in program order.
2. **Monitor Lock Rule**: An unlock on a monitor lock happens-before every subsequent lock on that same monitor.
3. **Volatile Variable Rule**: A write to a `volatile` field happens-before every subsequent read of that same `volatile` field.
4. **Thread Start Rule**: A call to `Thread.start()` on a new thread happens-before any action in the started thread.
5. **Thread Termination Rule**: Any action in a thread happens-before any other thread successfully detects that thread has terminated (via `join()` or `isAlive() == false`).
6. **Interruption Rule**: A thread calling `interrupt()` on another thread happens-before the interrupted thread detects the interrupt.
7. **Transitivity**: If $A \xrightarrow{hb} B$ and $B \xrightarrow{hb} C$, then $A \xrightarrow{hb} C$.

### The `volatile` Keyword
Declaring a field `volatile` guarantees two distinct properties:
1. **Visibility**: Reads always read the most recent write directly from main memory/coherent caches; writes immediately flush through cache hierarchies.
2. **Instruction Ordering**: The compiler and CPU are forbidden from reordering memory operations across a volatile access.

---

## 3. How It Works Internally

### Hardware Memory Barriers (Fences)
To enforce the happens-before relationship, the HotSpot JIT compiler inserts **Memory Barrier** instructions into the generated machine code:
- **LoadLoad**: Guarantees earlier reads complete before subsequent reads.
- **StoreStore**: Guarantees earlier writes flush before subsequent writes.
- **LoadStore**: Guarantees earlier reads complete before subsequent writes.
- **StoreLoad**: The most heavyweight fence. Guarantees earlier writes flush to memory before subsequent reads from any core.

```
[ Normal Writes / Operations ]
          │
    StoreStore Barrier
          │
[ Volatile Write Operation ]
          │
    StoreLoad Barrier (Flushes CPU Store Buffer)
          │
[ Normal Operations / Volatile Reads ]
          │
    LoadLoad / LoadStore Barriers
```

### The Piggybacking Technique on Volatile
Because of transitivity, writing to a `volatile` variable flushes **all preceding non-volatile writes** as well!
```java
int a = 1;              // Non-volatile write
boolean b = true;       // Non-volatile write
volatile boolean ready; // Volatile flag

ready = true;           // Flushes 'a', 'b', and 'ready' together!
```
Any thread that subsequently reads `ready == true` is mathematically guaranteed to see `a == 1` and `b == true`.

---

## 4. Syntax & Usage

### The Classic Volatile Shutdown Flag
```java
public class BackgroundWorker implements Runnable {
    // volatile guarantees visibility across all cores
    private volatile boolean running = true;

    public void stop() {
        running = false;
    }

    @Override
    public void run() {
        while (running) {
            // Perform background batch processing...
        }
        System.out.println("Worker stopped cleanly.");
    }
}
```

### Thread-Safe Lazy Initialization via Double-Checked Locking (DCL)
```java
public class SafeDclSingleton {
    // CRITICAL: Must be volatile to prevent instruction reordering!
    private static volatile SafeDclSingleton instance;

    private SafeDclSingleton() {
        // Heavy resource initialization
    }

    public static SafeDclSingleton getInstance() {
        SafeDclSingleton result = instance; // Local read optimization
        if (result == null) {
            synchronized (SafeDclSingleton.class) {
                result = instance;
                if (result == null) {
                    instance = result = new SafeDclSingleton();
                }
            }
        }
        return result;
    }
}
```

---

## 5. Practical Examples

### Example 1: Demonstrating the Infinite Loop Visibility Bug
```java
// Goal: Prove that non-volatile variables cause infinite loops due to CPU caching
public class VisibilityHazardDemo {
    private static boolean flag = false; // Missing 'volatile'!

    public static void main(String[] args) throws InterruptedException {
        Thread worker = new Thread(() -> {
            System.out.println("Worker started, waiting for flag...");
            while (!flag) {
                // JIT compiler optimizes '!flag' into an infinite loop:
                // if (!flag) while(true);
            }
            System.out.println("Worker detected flag change!");
        });

        worker.start();
        Thread.sleep(100);

        flag = true; // Main thread modifies shared variable
        System.out.println("Main set flag = true. Waiting for worker to terminate...");
        worker.join(2000);

        if (worker.isAlive()) {
            System.err.println("ALERT: Worker hung! Visibility failure demonstrated.");
            System.exit(0);
        }
    }
}
```
*Explanation:* The HotSpot C2 compiler hoists the read of non-volatile `flag` outside the loop, causing the thread to loop infinitely even though main updated the variable.

### Example 2: Instruction Reordering in Double-Checked Locking
```java
// Goal: Explain why DCL without volatile leaks partially constructed objects
class UnsafeDcl {
    private static UnsafeDcl instance;
    private int data = 42;

    public static UnsafeDcl getInstance() {
        if (instance == null) {
            synchronized (UnsafeDcl.class) {
                if (instance == null) {
                    // HAZARD: Bytecode steps of 'new UnsafeDcl()':
                    // 1. Allocate memory
                    // 2. Invoke constructor (initialize data = 42)
                    // 3. Assign memory address to 'instance'
                    // WITHOUT volatile, CPU can reorder to: 1 -> 3 -> 2!
                    // Another thread sees instance != null while data is still 0!
                    instance = new UnsafeDcl();
                }
            }
        }
        return instance;
    }
}
```
*Explanation:* Adding `volatile` to `instance` introduces a `StoreStore` barrier before assignment, guaranteeing the constructor finishes completely before publishing the reference.

### Example 3: Edge Case: Final Field Freeze Action
```java
// Goal: Safe publication guarantees of final fields without volatile
public class ImmutablePublication {
    private final int x;
    private final int y;

    public ImmutablePublication(int x, int y) {
        this.x = x;
        this.y = y;
        // JSR-133 Freeze Action: StoreStore barrier emitted at constructor exit!
    }

    public int sum() { return x + y; }
}
```
*Explanation:* The JMM guarantees that any thread observing a reference to an object whose fields are `final` is guaranteed to observe the properly initialized values of those `final` fields, even without synchronization.

---

## 6. Best Practices
1. **Always Use `volatile` on Status Flags**: If a boolean flag is set by one thread and polled by another without locks, mark it `volatile`.
2. **Never Rely on `volatile` for Compound Updates**: Do not use `volatile` for counters (`count++`) or read-modify-write sequences; use `AtomicInteger` instead.
3. **Make Fields `final` for Inherent JMM Safety**: JMM guarantees safe publication for `final` fields across all threads without locks.
4. **Use Local Variable Optimization in DCL**: Read the volatile field into a local variable once (`SafeDclSingleton result = instance;`) to reduce volatile read memory barrier overhead.
5. **Prefer Enum or Holder Idiom for Singletons**: Instead of Double-Checked Locking, use an enum or the Initialization-on-demand Holder idiom.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Thinking `volatile` provides atomicity | `volatile` ensures visibility and ordering, NOT compound atomicity (`volatile int count++` loses updates). | Use `AtomicInteger` or `synchronized`. |
| Missing `volatile` in Double-Checked Locking | Reordering allows other threads to read partially constructed objects where fields are 0/null. | Declare the singleton field `volatile`. |
| Assuming `Thread.sleep()` fixes visibility | Sleep yields CPU time, which may coincidentally cause cache flushes, but provides zero JMM guarantees. | Use `volatile` or locks. |
| Relying on non-volatile variable updates across threads | JIT compiler can hoist reads outside loops, creating permanent infinite loops. | Mark shared control flags `volatile`. |
| Leaking `this` before constructor finishes | Bypasses JMM final field freeze guarantees; other threads see uninitialized memory. | Never publish `this` inside constructors. |

---

## 8. Performance & Security Notes

### Cost of Volatile Reads and Writes
- **Volatile Reads**: On x86 processors, volatile reads are virtually free because x86 already guarantees Strong Total Store Order (TSO). On ARM/PowerPC, volatile reads require lightweight barrier instructions.
- **Volatile Writes**: On x86 and ARM, volatile writes require a heavyweight `lock addl` or `mfence` instruction (StoreLoad barrier) that stalls the CPU pipeline until store buffers flush.

### Security Implications of Partial Construction
In multi-tenant or security-sensitive architectures, an un-synchronized lazy initializer can publish a security object (such as an authorization token) before its validation fields are initialized, allowing malicious concurrent threads to bypass authorization.

---

## 9. Comparison / When to Use What

| Feature | `volatile` | `synchronized` | `AtomicInteger` |
|---|---|---|---|
| **Mutual Exclusion** | No | Yes | No |
| **Visibility** | Yes | Yes | Yes |
| **Ordering** | Yes | Yes | Yes |
| **Compound Atomicity**| No | Yes | Yes (for supported ops) |
| **Performance Cost** | Low | Medium-High | Low (Hardware CAS) |
| **Best For** | Status flags, DCL | Complex invariants | Counters, state flags |

---

## 10. Interview Questions

<details>
<summary>1. What is the Java Memory Model (JSR-133)?</summary>
The JMM is the formal specification defining how threads interact through shared memory, establishing the conditions under which writes by one thread are guaranteed to be visible to reads by other threads via the Happens-Before relationship.
</details>

<details>
<summary>2. What is the Happens-Before relationship?</summary>
A partial ordering over memory actions in a Java program. If action A happens-before action B, the memory effects of A are guaranteed to be visible to B, and A cannot be reordered after B.
</details>

<details>
<summary>3. What does the `volatile` keyword guarantee in Java?</summary>
It guarantees visibility (reads always return the most recent write directly from coherent memory) and ordering (prevents the compiler and CPU from reordering memory instructions across the volatile read/write).
</details>

<details>
<summary>4. Why does Double-Checked Locking require `volatile`?</summary>
Without `volatile`, the instruction sequence of `new Singleton()` (allocate memory, invoke constructor, assign reference) can be reordered by the CPU so that the reference is assigned before the constructor completes. Another thread can observe a non-null instance with partially initialized fields.
</details>

<details>
<summary>5. What are the four types of Memory Barriers?</summary>
`LoadLoad`, `StoreStore`, `LoadStore`, and `StoreLoad`. The `StoreLoad` barrier is the most expensive because it forces store buffers to flush to main memory before subsequent reads can proceed.
</details>

<details>
<summary>6. How do `final` fields achieve safe publication under JMM?</summary>
The JMM specifies a "Freeze Action" at constructor exit: a `StoreStore` barrier is emitted, guaranteeing that all writes to `final` fields are committed to memory before the reference to the newly constructed object becomes visible to other threads.
</details>

<details>
<summary>7. What is "Piggybacking" on synchronization in the JMM?</summary>
Because happens-before is transitive, performing a volatile write or unlocking a monitor flushes all preceding non-volatile writes as well, allowing non-volatile data to be safely transmitted alongside a volatile synchronization token.
</details>

<details>
<summary>8. Can volatile variables suffer from deadlocks?</summary>
No. Volatile operations do not acquire locks or block threads; they only enforce memory visibility and ordering fences. They are completely immune to deadlocks.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Volatile Cancel Flag
- **Task**: Implement a worker thread that downloads data chunks in a loop until a `volatile boolean cancelled` flag is toggled to true by the main thread.
<details>
<summary>Solution</summary>

```java
public class VolatileWorkerDemo {
    private volatile boolean cancelled = false;

    public void cancel() {
        this.cancelled = true;
    }

    public void runWorker() throws InterruptedException {
        Thread worker = new Thread(() -> {
            int chunks = 0;
            while (!cancelled) {
                chunks++;
                try { Thread.sleep(50); } catch (InterruptedException ignored) {}
            }
            System.out.println("Worker cancelled after processing " + chunks + " chunks.");
        });

        worker.start();
        Thread.sleep(200);
        cancel();
        worker.join();
    }

    public static void main(String[] args) throws Exception {
        new VolatileWorkerDemo().runWorker();
    }
}
```
</details>

### Exercise 2 (Medium): Safe Double-Checked Locking Holder
- **Task**: Implement a thread-safe connection pool factory using Double-Checked Locking with proper `volatile` modifier and local variable optimization.
<details>
<summary>Solution</summary>

```java
public class ConnectionPoolFactory {
    private static volatile ConnectionPoolFactory instance;

    private ConnectionPoolFactory() {
        System.out.println("Connection pool initialized.");
    }

    public static ConnectionPoolFactory getInstance() {
        ConnectionPoolFactory local = instance;
        if (local == null) {
            synchronized (ConnectionPoolFactory.class) {
                local = instance;
                if (local == null) {
                    instance = local = new ConnectionPoolFactory();
                }
            }
        }
        return local;
    }
}
```
</details>

### Exercise 3 (Hard): Volatile State Machine with Guarded Publication
- **Task**: Create an immutable record `Transaction(String id, double amount)` and publish updates to a `volatile Transaction currentTransaction` reference from one thread while 3 consumer threads continuously verify that `id` and `amount` are never read in an inconsistent state.
<details>
<summary>Solution</summary>

```java
import java.util.concurrent.*;

public class VolatilePublicationDemo {
    public record Transaction(String id, double amount) {}

    private volatile Transaction currentTx = new Transaction("INIT", 0.0);

    public void update(String id, double amount) {
        // Safe publication of immutable record via volatile write
        this.currentTx = new Transaction(id, amount);
    }

    public Transaction get() {
        return this.currentTx;
    }

    public static void main(String[] args) throws Exception {
        VolatilePublicationDemo demo = new VolatilePublicationDemo();
        try (ExecutorService executor = Executors.newFixedThreadPool(4)) {
            // Producer
            executor.submit(() -> {
                for (int i = 1; i <= 1000; i++) {
                    demo.update("TX-" + i, i * 10.5);
                }
            });

            // Consumers
            for (int c = 0; c < 3; c++) {
                executor.submit(() -> {
                    for (int j = 0; j < 500; j++) {
                        Transaction tx = demo.get();
                        if (tx.id() == null) {
                            throw new AssertionError("Observed corrupted transaction!");
                        }
                    }
                });
            }
        }
        System.out.println("All volatile publication assertions passed!");
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                    JMM CHEAT SHEET                                      |
+-----------------------------------------------------------------------------------------+
| Specification            | JSR-133 (Finalized in Java 5)                                |
| Core Concept             | Happens-Before order (Guarantees visibility & ordering)      |
| Volatile Rule            | Write to volatile happens-before subsequent read of same var |
| Monitor Rule             | Unlock happens-before subsequent lock on same monitor        |
| Final Field Guarantee    | Freeze Action guarantees safe publication without volatile   |
| Memory Barriers          | LoadLoad, StoreStore, LoadStore, StoreLoad (heaviest)        |
| DCL Rule                 | Field MUST be volatile to prevent 1 -> 3 -> 2 reordering     |
| False Assumption         | Volatile DOES NOT provide compound atomicity (count++)       |
+-----------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [JSR-133: Java Memory Model and Thread Specification](https://www.cs.umd.edu/~pugh/java/memoryModel/)
- Java Concurrency in Practice by Brian Goetz — Chapter 16 (The Java Memory Model)
- "Close Encounters with the Java Memory Model" by Aleksey Shipilëv
