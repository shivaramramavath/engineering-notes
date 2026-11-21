# Garbage Collection Fundamentals

> Comprehensive architectural reference for Garbage Collection in the HotSpot JVM: the Weak Generational Hypothesis, object allocation lifecycles, GC Roots reachability analysis, Stop-The-World (STW) pauses, safepoints, and core reclamation algorithms (Mark-Sweep, Mark-Compact, Copying).

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_jvm-architecture.md](00_jvm-architecture.md), [02_jvm-memory-areas.md](02_jvm-memory-areas.md) |
| **Time to Read** | 28 minutes |
| **Status** | Active |
| **Java Version** | Java 1.0+ (Modern Generational Mechanics Java 21-25) |

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

### The Burden of Manual Memory Management
In languages like C and C++, memory must be explicitly allocated (`malloc`) and freed (`free`).
Manual memory management is the leading cause of critical software bugs:
- **Dangling Pointers / Use-After-Free**: Reading memory that has already been deallocated, causing data corruption or Remote Code Execution (RCE).
- **Double Free**: Freeing the same memory region twice, corrupting allocator metadata.
- **Memory Leaks**: Forgetting to free memory, causing gradual exhaustion of RAM until process crash.

### The Solution: Automatic Garbage Collection
The Java Virtual Machine manages heap memory automatically through **Garbage Collection (GC)**:
- Programmers allocate objects with `new`; the runtime tracks object reachability.
- When objects become unreachable by executing threads, the GC reclaims their memory transparently in background cycles.
- Eliminates use-after-free and double-free vulnerabilities entirely.

---

## 2. Core Concepts

### The Weak Generational Hypothesis
Decades of empirical observation in computer science revealed two universal truths about object lifecycles:
1. **Most objects die young**: Over 95% of objects are ephemeral (local variables inside methods, temporary builders, I/O buffers) and become unreachable shortly after allocation.
2. **Objects that survive become long-lived**: Objects that survive multiple garbage collection cycles tend to remain reachable for a very long time (caches, singletons, session contexts).

### Generational Heap Layout
Because of this hypothesis, the HotSpot heap is partitioned into two primary generations:
1. **Young Generation**:
   - **Eden Space**: Where new objects are initially allocated via TLABs.
   - **Survivor Spaces (`S0` and `S1`)**: Two alternating semi-spaces (FromSpace and ToSpace). Live objects surviving an Eden collection are copied here.
2. **Old (Tenured) Generation**: Holds long-lived objects that survived multiple collections (tenuring threshold, default 15).

### GC Roots and Reachability Analysis
How does the JVM know which objects are alive?
HotSpot uses **Tracing Reachability Analysis** starting from a designated set of **GC Roots**:
- **Local Variables on Thread Stacks**: Parameters and local variables in currently executing stack frames.
- **Static Class Fields**: References held by loaded classes in Metaspace.
- **JNI / Native Global and Local References**: References passed to native C code.
- **Thread Objects**: Running, active thread instances themselves.
- **Synchronization Monitors**: Objects currently used as locks in `synchronized` blocks.

Any object that cannot be reached by traversing reference pointers from at least one GC Root is considered **unreachable garbage** and is eligible for collection.

---

## 3. How It Works Internally

### The Three Foundational GC Algorithms
1. **Mark-Copy (Used in Young Generation)**:
   - Divides memory into active and inactive regions.
   - Traces live objects from Eden and copies them into the empty Survivor space.
   - Clears the entire Eden space in a single instant! Extremely fast and leaves zero memory fragmentation.
2. **Mark-Sweep (Used in older Old Generation collectors)**:
   - Phase 1 (Mark): Traces and tags all live objects.
   - Phase 2 (Sweep): Reclaims memory of untagged objects.
   - *Downside*: Leaves free memory fragmented into discontinuous gaps.
3. **Mark-Compact (Used in modern Old Generation collectors)**:
   - Phase 1 (Mark): Traces all live objects.
   - Phase 2 (Compact): Slides all surviving objects toward the beginning of the memory space, creating a contiguous block of free memory.

```
+-----------------------------------------------------------------------------------+
|                            YOUNG GENERATION LIFECYCLE                             |
|  +-----------------------------+       +---------------+       +---------------+  |
|  |         Eden Space          | ----> |  Survivor S0  | <---> |  Survivor S1  |  |
|  | (New allocations via TLABs) | Minor |   (FromSpace) | Minor |   (ToSpace)   |  |
|  +-----------------------------+   GC  +---------------+   GC  +---------------+  |
+-----------------------------------------------------------------------------------+
                                          │
                                          │ Survives Tenuring Threshold (e.g. 15)
                                          ▼
+-----------------------------------------------------------------------------------+
|                              OLD (TENURED) GENERATION                             |
|                  Long-lived objects, caches, singletons, big buffers               |
+-----------------------------------------------------------------------------------+
```

### Stop-The-World (STW) Pauses and Safepoints
To safely trace references or relocate objects without threads mutating pointers simultaneously, the JVM brings all application threads to a coordinated halt at a **Safepoint**.
During an STW pause, application throughput drops to zero while the GC safely traces roots or relocates memory blocks. Modern GC engineering focuses on minimizing or eliminating STW duration.

---

## 4. Syntax & Usage

### Inspecting GC Activity via Command-Line
```bash
# Print GC logging to console in modern Java
java -Xlog:gc* -version

# Launch application with unified GC logging to file
java -Xms4g -Xmx4g -Xlog:gc*,gc+phases=debug:file=gc.log:time,uptime,pid:filecount=5,filesize=50m MyApp
```

### Invoking Explicit GC (Anti-Pattern)
```java
// Anti-Pattern: Never call in production!
System.gc(); // Suggests the JVM perform a full GC (frequently ignored or triggers STW)
```
Disable explicit `System.gc()` calls using `-XX:+DisableExplicitGC`.

---

## 5. Practical Examples

### Example 1: Demonstrating Object Promotion to Old Generation
```java
// Goal: Observe object promotion from Eden through Survivor to Old Gen
import java.util.ArrayList;
import java.util.List;

public class ObjectPromotionDemo {
    public static void main(String[] args) throws InterruptedException {
        List<byte[]> longLivedCache = new ArrayList<>();

        System.out.println("Allocating short-lived vs long-lived objects...");
        for (int i = 0; i < 50; i++) {
            // Ephemeral object: allocated in Eden and dies immediately
            byte[] ephemeral = new byte[512 * 1024];

            if (i % 5 == 0) {
                // Long-lived object: retained in list, survives Young GCs, promoted to Old Gen!
                longLivedCache.add(new byte[1024 * 1024]);
            }
            Thread.sleep(50);
        }

        System.out.println("Retained " + longLivedCache.size() + " long-lived buffers in memory.");
    }
}
```

### Example 2: Observing Finalization Elimination (Cleaner API)
Historically, `finalize()` was used for resource cleanup before collection. Deprecated in Java 9 and permanently disabled in Java 18+ (JEP 421), modern Java uses `java.lang.ref.Cleaner`:
```java
import java.lang.ref.Cleaner;

public class ModernCleanerDemo implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();

    private static class CleanAction implements Runnable {
        private final String resourceId;
        CleanAction(String id) { this.resourceId = id; }
        @Override
        public void run() {
            System.out.println("CLEANER: Native resource released for " + resourceId);
        }
    }

    private final Cleaner.Cleanable cleanable;

    public ModernCleanerDemo(String resourceId) {
        // Register cleanup action that runs when object becomes phantom reachable
        this.cleanable = CLEANER.register(this, new CleanAction(resourceId));
    }

    @Override
    public void close() {
        cleanable.clean();
    }
}
```

---

## 6. Best Practices
1. **Never Call `System.gc()` in Production**: Explicit GC requests can trigger expensive full stop-the-world collections that derail SLA targets. Use `-XX:+DisableExplicitGC`.
2. **Tune for Young Generation Throughput**: Ensure Eden is large enough that short-lived objects die before Minor GC, avoiding premature promotion into Old Gen.
3. **Use `-XX:+AlwaysPreTouch`**: Pre-touches all heap pages at JVM startup, mapping physical memory immediately and eliminating page fault latency during peak load.
4. **Avoid Giant Object Allocations in Loops**: Allocations larger than half a G1 region are classified as **Humongous Objects** and allocated directly into Old Generation, triggering premature concurrent marking.
5. **Analyze GC Logs**: Always configure `-Xlog:gc*` in production to capture GC duration, pause times, and allocation rates for diagnostic analysis.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Relying on `finalize()` | Severely deprecated/removed (JEP 421); causes object resurrection and memory leaks. | Use `AutoCloseable` or `java.lang.ref.Cleaner`. |
| Unbounded static collections | Static fields are GC Roots; collections attached to static fields never get collected. | Use bounded caches with LRU eviction (`Caffeine`). |
| Premature promotion of short-lived objects | Eden is too small; short-lived objects overflow into Old Gen, causing frequent Major GCs. | Increase Young Generation size (`-XX:NewRatio` or G1 max young). |
| Assuming GC reclaims native off-heap memory | GC only tracks heap objects; off-heap memory (`ByteBuffer.allocateDirect`) can leak. | Ensure explicit release or rely on cleaner wrappers. |

---

## 8. Performance & Security Notes

### Premature Promotion and Promotion Failure
When Survivor spaces are full during a Minor GC, surviving objects are prematurely promoted into the Old Generation. If the Old Generation has insufficient contiguous free space to accommodate the promoted objects, a **Promotion Failure** occurs, triggering an emergency full stop-the-world compaction pause.

---

## 9. Comparison / When to Use What

| GC Phase | Young Gen (Minor GC) | Old Gen (Major / Full GC) |
|---|---|---|
| **Primary Algorithm** | Mark-Copy (Eden $\to$ Survivor) | Mark-Sweep-Compact |
| **Frequency** | High (several times per minute) | Rare (hours or days) |
| **Pause Duration** | Short (typically 1–10ms) | Longer (10ms–seconds) |
| **Space Targeted** | Eden, S0, S1 | Tenured Space |

---

## 10. Interview Questions

<details>
<summary>1. What is the Weak Generational Hypothesis?</summary>
The empirical principle that most objects die very shortly after allocation (over 95%), while objects that survive early collections tend to remain alive for a long time. It justifies partitioning heap memory into Young and Old generations.
</details>

<details>
<summary>2. What are GC Roots in Java?</summary>
The initial set of references from which reachability analysis begins. They include local variables on thread stacks, static class fields, JNI native handles, and active Thread objects.
</details>

<details>
<summary>3. Why is Mark-Copying preferred for the Young Generation?</summary>
Because most Young Gen objects are dead, copying only the tiny percentage of live objects to Survivor space is fast and completely eliminates memory fragmentation without compaction passes.
</details>

<details>
<summary>4. What is a Safepoint?</summary>
A point in program execution (such as loop branch back-edges or method entries) where all application threads can be safely paused to permit global VM operations like GC roots scanning.
</details>

<details>
<summary>5. Why was `finalize()` deprecated and removed (JEP 421)?</summary>
Because it suffered from arbitrary thread execution, delayed reclamation, object resurrection bugs, and severe performance penalties. The recommended replacements are `AutoCloseable` and `java.lang.ref.Cleaner`.
</details>

<details>
<summary>6. What is the difference between Minor GC, Major GC, and Full GC?</summary>
Minor GC collects only the Young Generation. Major GC collects the Old Generation. Full GC collects the entire heap (Young, Old, and Metaspace) and typically involves a full stop-the-world compaction.
</details>

<details>
<summary>7. What is Premature Promotion?</summary>
When short-lived objects are promoted into the Old Generation before their time due to insufficient Survivor space, leading to rapid Old Gen fragmentation and frequent Major GCs.
</details>

<details>
<summary>8. What is the effect of `-XX:+DisableExplicitGC`?</summary>
It instructs the JVM to ignore programmatic calls to `System.gc()`, preventing third-party libraries from triggering unexpected full GC pauses.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Simulating Ephemeral Allocation Churn
- **Task**: Write a program that allocates 1,000,000 small `String` objects in a loop without storing them in a collection, observing zero heap accumulation due to Minor GCs.
<details>
<summary>Solution</summary>

```java
public class AllocationChurnDemo {
    public static void main(String[] args) {
        long start = System.currentTimeMillis();
        for (int i = 0; i < 1_000_000; i++) {
            String s = "Ephemeral-" + i;
            if (s.isEmpty()) System.out.println(); // prevent dead-code elimination
        }
        System.out.println("Finished 1M allocations in " + (System.currentTimeMillis() - start) + " ms");
    }
}
```
</details>

### Exercise 2 (Medium): SoftReference In-Memory Cache
- **Task**: Implement a memory-sensitive cache using `java.lang.ref.SoftReference` that allows cached entries to be cleared by the GC only when heap memory is constrained.
<details>
<summary>Solution</summary>

```java
import java.lang.ref.SoftReference;
import java.util.*;

public class SoftMemoryCache<K, V> {
    private final Map<K, SoftReference<V>> cache = new HashMap<>();

    public void put(K key, V value) {
        cache.put(key, new SoftReference<>(value));
    }

    public V get(K key) {
        SoftReference<V> ref = cache.get(key);
        if (ref == null) return null;
        V value = ref.get();
        if (value == null) {
            cache.remove(key); // Reclaimed by GC under memory pressure!
        }
        return value;
    }
}
```
</details>

### Exercise 3 (Hard): GC Roots Leak Diagnosis Simulator
- **Task**: Write a class that inadvertently leaks memory by holding object instances in a static list, and show how memory accumulates until `OutOfMemoryError`.
<details>
<summary>Solution</summary>

```java
import java.util.ArrayList;
import java.util.List;

public class StaticRootLeakSimulator {
    // Static fields are GC Roots: items added here are NEVER collected!
    private static final List<byte[]> ROOT_LEAK = new ArrayList<>();

    public static void main(String[] args) {
        try {
            while (true) {
                ROOT_LEAK.add(new byte[1024 * 1024]); // Leak 1 MB per iteration
            }
        } catch (OutOfMemoryError e) {
            System.err.println("Demonstrated OOM via static GC root leak: " + ROOT_LEAK.size() + " MB retained.");
        }
    }
}
```
</details>

---


### 11.4 In-Depth Troubleshooting Lab: Diagnosing GC Thrashing

When diagnosing GC performance degradation under extreme production load, follow this systematic methodology:

1. **Verify Heap Sizing and Footprint**:
   Inspect baseline memory consumption vs peak consumption:
   \\ash
   jcmd <PID> GC.heap_info
   \   Check whether current survivor space allocation is continuously overflowing into tenured generation due to prematurely exhausted survivor bounds.

2. **Diagnose Allocation Bursts**:
   Use JDK Flight Recorder without imposing perceptible overhead:
   \\ash
   jcmd <PID> JFR.start name=alloc_profile settings=profile duration=60s filename=alloc.jfr
   \   Analyze allocations by thread and type in JDK Mission Control. Look for temporary byte arrays, collections resized repeatedly without initial capacity hints, or intermediate streaming pipeline objects.

3. **Memory Leak vs Sizing Mismatch**:
   - If memory recovers fully to baseline after each Full GC or major cycle, the application suffers from allocation burstiness, not a permanent memory leak. Increase nursery/young generation size or reduce allocation rates.
   - If minimum post-GC memory floor steadily climbs over hours or days until OutOfMemoryError occurs, permanent references are retained (e.g., static maps, uncleaned thread locals, event listener registrations).

4. **Reference Processing Bottlenecks**:
   Examine GC logs for high Reference Processing times. Applications relying heavily on Finalizers (deprecated) or uncoordinated Cleaners / WeakReferences can stall GC pauses while walking reference lists.


## 12. Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                               GARBAGE COLLECTION CHEAT SHEET                            |
+-----------------------------------------------------------------------------------------+
| Weak Gen Hypothesis      | Most objects die young; survivors become long-lived          |
| Heap Partitioning        | Young Gen (Eden, S0, S1) / Old Gen (Tenured)                 |
| Tracing Analysis         | Starts from GC Roots (Stacks, Statics, JNI, Active Threads)  |
| 3 Basic Algorithms       | Mark-Copy (Young), Mark-Sweep, Mark-Compact (Old)            |
| Safepoints               | Coordinated halt where all threads pause for STW operations  |
| Pre-touch Flag           | -XX:+AlwaysPreTouch (Maps physical RAM at JVM startup)       |
| Disable Explicit GC      | -XX:+DisableExplicitGC (Ignores System.gc())                 |
| Modern Cleanup           | java.lang.ref.Cleaner (Replaces deprecated finalize())       |
+-----------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [The Garbage Collection Handbook](https://gchandbook.org/) by Richard Jones, Antony Hosking, Eliot Moss
- Oracle Dev.java: Understanding Java Garbage Collection
- JEP 421: Deprecate Finalization for Removal
