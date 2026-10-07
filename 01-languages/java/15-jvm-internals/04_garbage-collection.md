# Garbage Collection

Java programs create objects with `new` and never free them. The JVM's **garbage collector (GC)** finds objects the program can no longer reach and reclaims their memory. This removes whole classes of bugs (use-after-free, double-free, most leaks of the C kind) and makes allocation cheap, at the price of **pauses**, extra memory, and a set of behaviors you need to understand when latency or memory matters.

This note covers the *concepts* shared by all collectors. The next one compares the actual collectors ([GC Algorithms](05_gc-algorithms.md)), and tuning lives in [GC Tuning](../20-performance/03_gc-tuning.md).

**Prerequisites:** [JVM Memory Areas](02_jvm-memory-areas.md).

---

## 1. What counts as garbage: reachability

An object is **live** if it's **reachable** from a **GC root** by following references. Everything else is garbage, even if objects in the garbage refer to each other (so cycles are collected, unlike with plain reference counting).

GC roots include:

- local variables and parameters on every thread's stack
- `static` fields of loaded classes
- active threads themselves
- JNI references and other JVM-internal handles

```text
 roots ──► A ──► B ──► C            live
           │
           └──► D                   live
 E ──► F ──► E  (cycle, unreachable from any root)   garbage: collected together
```

So "memory leak" in Java means **an object that is still reachable but no longer useful**. The GC can't know it's useless. Finding leaks = finding the reference path from a root ([Memory Areas](02_jvm-memory-areas.md#8-outofmemoryerror-which-one)).

---

## 2. How collectors work (the basic moves)

| Technique | Idea | Cost |
|---|---|---|
| **Mark** | Traverse from the roots, marking every reachable object | Proportional to the **live** data |
| **Sweep** | Free the unmarked objects, leaving gaps | Leaves **fragmentation** |
| **Compact** | Slide live objects together, closing gaps | Moves objects, and must fix all references |
| **Copy (evacuate)** | Copy live objects to a fresh area, then free the whole old area | Cost proportional to **live** objects. Dead ones cost nothing |

Modern collectors combine these. Copying is ideal for the young generation, where almost everything is dead.

### Stop-the-world and concurrency

- **Stop-the-world (STW):** application threads are paused at a **safepoint** while the GC works. Simple and fast per byte, but the pause is visible to users.
- **Parallel:** several GC threads work during the pause (shorter pause, same effect).
- **Concurrent:** GC threads run **alongside** the application, so pauses are short, at the cost of extra CPU, barriers on memory accesses, and complexity.

Different collectors choose different mixes ([GC Algorithms](05_gc-algorithms.md)).

---

## 3. Generations

**The weak generational hypothesis:** most objects die young, and objects that survive tend to live a long time.

```text
 Young generation                          Old generation
 ┌────────┬────────┬────────┐              ┌──────────────────────┐
 │  Eden  │  S0    │  S1    │  ──promote──►│  long-lived objects  │
 └────────┴────────┴────────┘              └──────────────────────┘
   new objects     survivors, copied          collected less often
   allocated here  each cycle (age++)         (and more expensively)
```

1. New objects are allocated in **Eden**.
2. When Eden fills, a **young (minor) GC** copies the live objects to a survivor space (and clears Eden). It's fast because the live set is small.
3. Objects that survive repeated collections (their **age** exceeds a threshold, or survivors overflow) are **promoted** to the old generation.
4. When the old generation fills up, a more expensive collection runs: a **mixed** collection (G1) or a **full GC**.

| Term | Meaning |
|---|---|
| **Minor / young GC** | Collects only the young generation |
| **Major / old GC / mixed** | Collects the old generation (and possibly part of the young) |
| **Full GC** | Collects the whole heap (and often metaspace). Usually the **slowest** pause and a sign of trouble |

Cross-generation references (an old object pointing to a young one) are tracked with **write barriers** and structures like card tables or remembered sets, so a young GC doesn't have to scan the entire old generation. Some collectors (ZGC, Shenandoah) now also have generational modes.

---

## 4. The trade-off triangle

```text
          Low pause times (latency)
                 /\
                /  \
               /    \
   High       /______\      Small memory
   throughput             footprint
```

You can't maximize all three. More concurrency lowers pauses but costs CPU (throughput) and memory. More heap headroom boosts throughput but costs footprint. Choose a collector and settings by what your application values ([GC Algorithms](05_gc-algorithms.md#6-choosing-a-collector)).

Useful terms:

| Term | Meaning |
|---|---|
| **Pause time** | How long application threads are stopped |
| **Throughput** | Share of time spent running your code vs GC |
| **Allocation rate** | MB/s of new objects. High rates trigger frequent young GCs |
| **Live set** | Data that survives a collection. This is what GC work scales with |
| **Promotion rate** | How fast objects move into the old generation |
| **Headroom** | Free heap beyond the live set. Concurrent collectors need it to keep up |

---

## 5. Reference types

Beyond ordinary **strong** references (which keep objects alive), `java.lang.ref` gives you weaker ones that let the GC reclaim objects:

| Type | Collected when... | Typical use |
|---|---|---|
| **Strong** (normal) | Never while reachable | Everything |
| `SoftReference` | The JVM is **running low on memory** (before `OutOfMemoryError`) | Memory-sensitive caches (use with care) |
| `WeakReference` | At the next GC once only weakly reachable | `WeakHashMap`, canonicalizing maps, listener registries |
| `PhantomReference` | After finalization-eligible, used to run cleanup after collection | Resource cleanup (`Cleaner`) |

```java
WeakReference<Image> ref = new WeakReference<>(loadImage());
Image img = ref.get();            // null if it was collected
if (img == null) img = reload();
```

**Finalization (`finalize()`) is deprecated for removal** (Java 18, JEP 421). It's unpredictable, slow, and can resurrect objects. Release resources with **try-with-resources** ([try-with-resources](../06-exceptions-and-debugging/03_try-with-resources.md)), and use `java.lang.ref.Cleaner` only as a safety net for native resources.

---

## 6. Allocation and special cases

- **Allocation is cheap:** each thread allocates from its own **TLAB** in Eden (a pointer bump). Short-lived objects cost almost nothing to create *and* nothing to free, which is why object pooling of small objects usually hurts.
- **Large objects:** big arrays may be allocated directly in the old generation (or, in G1, as **humongous** objects in dedicated regions), which are expensive to allocate and collect. Huge short-lived arrays are a common cause of surprising GC behavior.
- **`System.gc()`** is only a **request**. By default it can trigger a full GC (a long pause). Remove calls from application code, and consider `-XX:+DisableExplicitGC` or `-XX:+ExplicitGCInvokesConcurrent` for libraries that call it.
- **Escape analysis** can avoid allocation entirely for non-escaping objects ([JIT Compiler](06_jit-compiler.md)).
- **Off-heap memory** (direct buffers, native) isn't scanned by the GC. The owning heap objects are, and freeing the native part depends on those objects being collected.

---

## 7. Observing GC

Use **unified logging** (Java 9+):

```bash
java -Xlog:gc*:file=gc.log:time,uptime,level,tags -jar app.jar
java -Xlog:gc -jar app.jar                      # one summary line per collection
```

A typical G1 line (formats vary by collector and version):

```text
[12.345s][info][gc] GC(7) Pause Young (Normal) (G1 Evacuation Pause) 512M->96M(1024M) 8.321ms
        │               │            │                                │      │  │       └ pause duration
        │               │            │                                │      │  └ total heap size
        │               │            │                                │      └ heap after
        │               │            │                                └ heap before
        │               │            └ cause
        │               └ type: young / mixed / full
        └ GC number
```

What to read from logs:

- **Pause durations** versus your latency budget (look at the worst, not just the average).
- **Frequency** of young GCs (allocation rate).
- **Heap after each GC over time:** a steadily rising "after" curve = a leak or a growing live set. A flat sawtooth is healthy.
- **Full GCs**, which should be rare. Frequent ones are a problem.
- Cause names: `Allocation Failure`, `G1 Evacuation Pause`, `Metadata GC Threshold`, `System.gc()`, `Humongous Allocation`, `Concurrent Mode Failure`/`Allocation Stall` (concurrent collector couldn't keep up).

Tools: `jstat -gcutil <pid> 1000` for live occupancy, **JFR** for GC events and allocation profiling, `jcmd <pid> GC.heap_info`, and log analyzers (GCViewer, GCeasy). See [JVM Diagnostics](../22-production-engineering/04_jvm-diagnostics.md) and [Profiling](../20-performance/00_profiling.md).

---

## 8. Common GC problems and what they look like

| Symptom in logs/metrics | Likely cause | Direction |
|---|---|---|
| Very frequent young GCs | High allocation rate (boxing, temporary objects, big copies) | Reduce allocation in hot paths; profile allocations |
| Heap "after GC" climbs until OOM | Memory leak (reachable but useless) | Heap dump, path to GC roots |
| Frequent full GCs / `GC overhead limit exceeded` | Live set ≈ heap size, or fragmentation | More heap, or reduce live data; check leaks |
| Long young-GC pauses | Large survivor/promotion volume, too many threads, swapping | Check promotion rate, don't let the heap swap to disk |
| Long pauses at safepoints with little GC work | Slow safepoint arrival (long loops, I/O in the VM thread) | `-Xlog:safepoint`, JFR |
| Humongous allocations (G1) | Objects ≥ half a region (large arrays, big buffers) | Chunk the data; stream |
| `Allocation Stall` / concurrent collector falling behind | Heap too small for allocation rate, or too few GC threads | More headroom, more CPU, less garbage |
| Latency spikes under the container's CPU limit | GC threads starved by CPU throttling | Raise CPU limits; check throttling metrics |

---

## 9. Writing GC-friendly code

You rarely tune the GC first. You usually **create less garbage** and **keep less data alive**:

- Avoid unnecessary **allocation in hot loops**: boxing (`Long` instead of `long`), temporary strings, per-call `new` of helper objects, intermediate collections ([Bytecode](03_bytecode.md#5-what-the-compiler-does-to-your-code-desugaring)).
- Prefer **primitives and arrays** for large numeric data. A `List<Integer>` costs far more memory than an `int[]`.
- **Pre-size collections** (`new ArrayList<>(n)`, `HashMap` capacity) to avoid repeated growth and copying.
- **Stream data** instead of loading whole files/results into memory ([File Processing Patterns](../11-io-and-networking/02_file-processing-patterns.md)).
- Bound **caches and queues**, and remove listeners and thread-locals on cleanup.
- Don't pool small short-lived objects. Do pool genuinely expensive resources (connections, big buffers) when justified.
- Avoid finalizers and `System.gc()`.
- Don't "null things out" as a habit. It's rarely needed in local scope, but do **clear references in long-lived structures** (for example, a custom stack or cache that holds elements after logically removing them).

Then measure with a profiler and GC logs before tuning flags ([Memory Optimization](../20-performance/02_memory-optimization.md), [GC Tuning](../20-performance/03_gc-tuning.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Believing garbage collection means no memory leaks | Reachable-but-unused objects still leak |
| Calling `System.gc()` "to help" | Remove it |
| Relying on `finalize()` | try-with-resources, `Cleaner` as a backstop |
| Tuning GC flags before looking at allocation and the live set | Profile first |
| Judging GC health by averages | Look at max pause and the percentile your users feel |
| Looking only at heap usage "before" GC | The "after GC" trend reveals leaks |
| Oversizing the heap for a latency-sensitive service without checking the collector | A bigger heap changes pause behavior, so test it |
| Pooling tiny objects | Let young-generation allocation do its job |
| Ignoring native/direct memory when the heap looks fine | NMT, `MaxDirectMemorySize` |

---

## Quick Summary

- The GC frees objects **unreachable from GC roots**, including cycles. A "leak" is an object still reachable but no longer needed.
- Basic moves: **mark, sweep, compact, copy**. Collectors run **stop-the-world**, **parallel**, and/or **concurrent**.
- The heap is **generational** (Eden + survivors + old) because most objects die young. A **full GC** is the expensive case to avoid.
- Trade-off triangle: **latency vs throughput vs footprint**.
- Weak/soft/phantom references exist, but `finalize()` is deprecated for removal. Use try-with-resources.
- Observe with `-Xlog:gc*`, JFR, and `jstat`. Watch max pauses, frequency, and the **heap-after-GC trend**.
- The best GC tuning is usually **allocating less and retaining less**.

**Next:** [GC Algorithms](05_gc-algorithms.md)