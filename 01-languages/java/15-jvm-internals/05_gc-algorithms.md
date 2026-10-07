# GC Algorithms

HotSpot ships several garbage collectors, each making a different trade-off between **pause time**, **throughput**, and **memory footprint** ([Garbage Collection](04_garbage-collection.md#4-the-trade-off-triangle)). You pick one with a flag. For most applications the default is the right answer, but you should know what you're running and when another collector is worth testing.

**Prerequisites:** [Garbage Collection](04_garbage-collection.md), [JVM Memory Areas](02_jvm-memory-areas.md).

---

## 1. The collectors at a glance

| Collector | Flag | Pauses | Throughput | Footprint | Good for |
|---|---|---|---|---|---|
| **Serial** | `-XX:+UseSerialGC` | Stop-the-world, single-threaded | Fine for small heaps | Lowest | Tiny heaps, single-CPU, short-lived tools |
| **Parallel** | `-XX:+UseParallelGC` | Stop-the-world, multi-threaded | **Highest** | Low | Batch jobs and throughput-first workloads |
| **G1** | `-XX:+UseG1GC` | Short, **targeted** pauses, mostly concurrent marking | Good | Moderate | **General-purpose default** |
| **ZGC** | `-XX:+UseZGC` | **Sub-millisecond typical, independent of heap size** | Slightly lower | Higher | Latency-critical services, very large heaps |
| **Shenandoah** | `-XX:+UseShenandoahGC` | Very short, independent of heap size | Slightly lower | Higher | Low-latency services (in OpenJDK builds that include it) |
| **Epsilon** | `-XX:+UseEpsilonGC` (experimental: needs `-XX:+UnlockExperimentalVMOptions`) | None: **never frees memory** | Maximum | n/a | Benchmarks, tests, very short-lived jobs |

### What is the default?

| Java version | Default |
|---|---|
| 8 | Parallel |
| 9 - 26 | **G1** on a "server-class" machine (2+ CPUs and about 1.8 GB+ of memory), otherwise **Serial** |
| **27+** | **G1 in all environments** (JEP 523) |

The Java 27 change matters for small containers: a pod with 1 CPU or under ~1.8 GB used to get Serial silently. After upgrading it gets G1, with different memory and pause behavior (recent G1 improvements closed the throughput gap). Pin the collector explicitly with a flag if you want a stable, intentional choice.

```bash
java -Xlog:gc+init -version                                        # prints which collector is in use
java -XX:+PrintFlagsFinal -version | grep -E "Use(G1|Parallel|Serial|Z|Shenandoah)GC"
jcmd <pid> VM.flags                                                # flags of a running JVM
```

---

## 2. Building blocks (just enough theory)

Collectors are assembled from a few ideas:

- **Generational collection:** frequent cheap young collections, rarer old collections.
- **Copying/evacuation:** copy live objects out of a region, then free the whole region.
- **Compaction:** eliminate fragmentation by moving live data together.
- **Write barriers** (tiny code the JIT inserts on reference stores) track cross-region references (remembered sets/card tables), so a partial collection doesn't need a full-heap scan.
- **Concurrent marking:** trace the object graph while the application runs. G1 uses **snapshot-at-the-beginning (SATB)**, which treats the graph as it was when marking started and uses a write barrier to record changes.
- **Load barriers + colored pointers (ZGC), load reference barriers (Shenandoah):** let the collector **move objects while the application runs**, because every reference read is checked and fixed if the object has moved. This is what makes concurrent *compaction* (and tiny pauses) possible.

---

## 3. Serial and Parallel

**Serial** uses one thread for everything, stopping the application while it works. It has almost no overhead and no extra threads, which suits small heaps (tens to a few hundred MB), single-core environments, and short-lived command-line programs.

**Parallel** ("throughput collector") does the same stop-the-world collections but with **many GC threads**, maximizing total work done per second. Pauses grow with heap and live-set size, and full collections can be long. It's a good fit for **batch and offline processing** where only total completion time matters, not individual pauses.

```bash
java -XX:+UseParallelGC -XX:ParallelGCThreads=8 -jar batch-job.jar
```

---

## 4. G1: the default

G1 ("Garbage-First") splits the heap into **equal-sized regions** (1 MB to 32 MB, chosen so there are about 2,048 regions) rather than two big generations. Each region is assigned a role dynamically:

```text
 ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
 │ E  │ E  │ S  │ O  │ O  │ H  │ H  │ O  │ E  │ ·  │    E = Eden   S = Survivor
 └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘    O = Old     H = Humongous   · = free
```

How it works:

1. **Young collections** evacuate live objects from Eden/survivor regions (stop-the-world, parallel).
2. When heap occupancy crosses a threshold, a **concurrent marking cycle** runs alongside the application, computing how much garbage each old region contains.
3. **Mixed collections** then evacuate the young regions *plus* the old regions with the **most garbage** ("garbage first"), reclaiming the most space per unit of pause time.
4. If it can't keep up, it falls back to a **full GC** (a compacting, parallel stop-the-world collection), which is the failure case to watch for.

Key ideas and knobs:

| Idea | Detail |
|---|---|
| **Pause-time goal** | `-XX:MaxGCPauseMillis` (default **200 ms**). G1 sizes its work to *try* to meet it. It's a goal, not a guarantee |
| **Humongous objects** | Objects at least half a region in size get dedicated contiguous regions. They're costly to allocate and collect, so chunk big arrays/buffers |
| **Adaptive** | G1 adjusts young size, tenuring, and the marking trigger automatically |
| **Don't over-tune** | Setting a fixed young size (`-Xmn`) disables its pause-goal adaptation. Prefer adjusting the pause goal and heap size |

Strengths: balanced throughput and pauses, good for heaps from hundreds of MB to tens of GB, and little tuning required. Weaknesses: pauses still scale with the amount of live data evacuated per collection, so very strict latency goals (single-digit ms) or very large heaps may call for ZGC.

---

## 5. ZGC and Shenandoah: concurrent compaction

Both aim for **pause times that don't grow with heap size** by doing almost all work (marking *and* moving objects) concurrently with the application.

### ZGC

- Pauses are typically **well under a millisecond**, and independent of heap size, with heaps from small up to many terabytes supported.
- Uses **colored pointers and load barriers**: metadata bits in references let it detect and fix stale pointers when the application reads them.
- **Generational since Java 21** (opt-in), **default mode in Java 23** (JEP 474), and the non-generational mode was **removed in Java 24** (JEP 490). On current JDKs `-XX:+UseZGC` simply gives you generational ZGC.
- Costs: some extra CPU and memory compared with G1 (barriers, multi-mapping), and it needs **headroom**: if allocation outruns the concurrent collector, threads *stall*, which shows in logs as `Allocation Stall`.

```bash
java -XX:+UseZGC -Xmx16g -jar service.jar
```

### Shenandoah

- Concurrent evacuation with **load-reference barriers**, with short pauses regardless of heap size.
- Included in many OpenJDK distributions (it originated at Red Hat). Not every vendor's build ships it, so check yours.
- A **generational mode** became a product feature in Java 25 (select it with `-XX:ShenandoahGCMode=generational`; check your JDK's documentation for current options).

```bash
java -XX:+UseShenandoahGC -jar service.jar
```

Both reward you most when the live set is large and **tail latency matters** (interactive APIs, trading, real-time-ish services). For ordinary services, the difference over G1 may not justify the extra CPU.

---

## 6. Choosing a collector

```text
 Is the environment tiny (≤ a few hundred MB heap) or a short-lived CLI/serverless job?
   └─ Serial (or the default G1 on Java 27: measure startup and footprint)

 Is it a batch/offline job where total runtime matters and pauses don't?
   └─ Parallel

 Do strict tail latencies (tens of ms or less) matter, or is the heap huge (tens of GB+)?
   └─ ZGC (or Shenandoah if your build has it); test under production-like load

 Otherwise (typical web service / microservice)
   └─ G1 (the default): tune only if measurements show a problem
```

| If you care about... | Prefer |
|---|---|
| Maximum throughput | Parallel |
| Balanced behavior, little tuning | **G1** |
| Lowest pause times, large heaps | **ZGC** / Shenandoah |
| Smallest footprint and simplicity | Serial |

Always **test with realistic load** before switching, and compare *pause distribution (p99/max)*, *throughput*, *CPU use*, and *memory footprint*. Collector benchmarks on toy programs mislead.

### Practical guidance

- Start with the default, and **enable GC logging** (`-Xlog:gc*`) in production from day one.
- Set `-Xms` = `-Xmx` for predictable behavior when memory is dedicated to the JVM.
- In containers, the JVM sizes GC thread counts from the container's CPU limit. CPU **throttling** can stretch GC pauses badly ([JVM Memory Areas](02_jvm-memory-areas.md#7-sizing-for-containers)).
- Fix allocation and retention problems in code before reaching for exotic collectors ([Garbage Collection](04_garbage-collection.md#9-writing-gc-friendly-code)).
- Details and flags: [GC Tuning](../20-performance/03_gc-tuning.md).

---

## 7. Upgrading and the Java 27 default change

When moving to Java 27 from an older runtime:

- If you **don't** set a collector flag and your containers were below 2 CPUs or ~1.8 GB, the effective collector changes from Serial to **G1**: re-measure startup time, footprint, and pauses.
- If you **do** set a flag (`-XX:+UseSerialGC`, `-XX:+UseParallelGC`, ...), nothing changes.
- **Compact object headers** (also default in Java 27) shrink objects and typically reduce GC pressure ([JVM Memory Areas](02_jvm-memory-areas.md#6-what-an-object-costs)).
- GC flag names and defaults change between releases. Read the release notes for your target JDK, and watch for "ignoring option" warnings in startup logs.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Not knowing which collector is running | `-Xlog:gc+init`, `jcmd <pid> VM.flags` |
| Switching to ZGC/Shenandoah "because it's newer" | Measure first: the default often suffices |
| Setting `-Xmn` or many G1 knobs by folklore | Let G1 adapt: tune the pause goal and heap size instead |
| Treating `MaxGCPauseMillis` as a guarantee | It's a goal. Allocation rate and live set still matter |
| Too little headroom for a concurrent collector | More heap and CPU, or reduce allocation |
| Using Parallel for a latency-sensitive API | Pauses scale with the heap. Use G1/ZGC |
| Keeping a removed flag (e.g., non-generational ZGC options) after an upgrade | Remove it; read the JDK release notes |
| Comparing collectors on a microbenchmark | Test with realistic data, load, and heap |
| Forgetting CPU limits in containers affect GC threads and pauses | Check throttling and the effective CPU count |

### Debugging

- Log lines with **`Pause Full`** → the collector fell back to full GC. Look at heap size, humongous allocations, and the live set.
- **`Allocation Stall`** (ZGC) or concurrent mode failures → the application allocates faster than the collector reclaims. Give it more headroom/CPU or cut allocation.
- Long pauses even with ZGC/Shenandoah → check for non-GC causes: safepoints (`-Xlog:safepoint`), swapping, CPU throttling, or a slow disk during logging.
- Behavior changed after a JDK upgrade → compare effective flags before/after (`-XX:+PrintFlagsFinal`).

---

## Quick Summary

- **Serial** (single thread, tiny), **Parallel** (max throughput, STW), **G1** (balanced, region-based, **default**), **ZGC** and **Shenandoah** (concurrent compaction, tiny pauses regardless of heap size), **Epsilon** (no-op).
- **Java 27 makes G1 the default everywhere.** Before that, small machines got Serial.
- G1 works in regions, uses a pause-time **goal** (200 ms default), concurrent marking, and **mixed collections**. Watch for humongous objects and full-GC fallbacks.
- ZGC is generational by default (since 23, non-generational removed in 24), with typically sub-millisecond pauses, at some CPU/memory cost and a need for headroom.
- Default first, log GC always, and switch only when **measurements** show a pause, throughput, or footprint problem.

**Next:** [JIT Compiler](06_jit-compiler.md)