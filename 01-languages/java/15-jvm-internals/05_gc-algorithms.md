# Modern GC Algorithms: G1, ZGC, and Shenandoah

> Comprehensive architectural comparison of modern HotSpot Garbage Collectors: G1 GC (region-based, mixed collections), Shenandoah GC, and **Generational ZGC (JEP 439 / JEP 474 in Java 21-25)**: colored pointers, load barriers, and sub-millisecond pauses.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [02_jvm-memory-areas.md](02_jvm-memory-areas.md), [04_garbage-collection.md](04_garbage-collection.md) |
| **Time to Read** | 28 minutes |
| **Status** | Active |
| **Java Version** | Java 9+ (G1 default), Java 21+ (Generational ZGC JEP 439), Java 25 (ZGC default low-latency) |

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

### The Latency vs Throughput Battle
Historically, Java applications faced a painful trade-off:
- Traditional throughput collectors (Parallel GC) achieved maximum CPU utilization, but subjected applications to **multi-second stop-the-world (STW) pauses** proportional to the size of the heap.
- As server heaps expanded from 4 GB to 100 GB, 500 GB, and multi-terabytes, traditional full GC pauses could freeze trading platforms, gaming backends, and microservices for dozens of seconds.

### The Solution: Concurrent, Low-Latency Collectors
Modern HotSpot engineering introduced collectors that perform marking, relocation, and compaction **concurrently while application threads are actively executing**:
- **G1 GC (Garbage-First)**: Production standard default since Java 9. Balances throughput with predictable, configurable pause targets.
- **Shenandoah GC**: Low-pause collector developed by Red Hat using Brooks Pointers and load-reference barriers.
- **Generational ZGC (JEP 439 / JEP 474)**: Groundbreaking low-latency collector delivering **sub-millisecond pause times** regardless of whether heap size is 16 MB or 16 TB!

---

## 2. Core Concepts

### G1 GC (Garbage-First)
- Replaces contiguous Young/Old boundaries with a grid of thousands of equal-sized **Regions** (1 MB to 32 MB).
- Regions dynamically act as Eden, Survivor, or Old generation.
- **Garbage-First Priority**: Collects regions containing the highest ratio of dead garbage first.
- **Mixed Collections**: Reclaims all Young regions plus a selected subset of the most garbage-dense Old regions in incremental pauses to satisfy `-XX:MaxGCPauseMillis`.

### Generational ZGC (Java 21 to 25)
Standardized as the flagship low-latency collector in Java 21+:
- **Colored Pointers**: Metadata flags (Marked0, Marked1, Remapped) are embedded directly inside the reference pointer bits.
- **Load Barriers**: Intercepts reference loads (`x.field`). If an object is being relocated, the load barrier transparently repairs (heals) the pointer on the fly.
- **Generational Partitioning (JEP 439)**: Separates young and old object allocations, eliminating allocation stalls on high-throughput services.
- **Pause Times**: Guaranteed $< 1$ millisecond max pause times across multi-terabyte heaps!

---

## 3. How It Works Internally

### G1 Region Grid and Remembered Sets (RSet)
In traditional collectors, scanning for cross-generational references required scanning the entire Old Generation.
G1 partitions memory into a matrix of regions and uses **Remembered Sets (RSets)**:
- Each region maintains an RSet tracking incoming references from other regions.
- During Minor GC, G1 scans only the RSets of the Young regions, isolating the collection completely without scanning the entire heap.

```
+-------+-------+-------+-------+
| Eden  | Old   | Surv  | Eden  |
+-------+-------+-------+-------+
| Old   | Free  | Humon | Old   |
+-------+-------+-------+-------+
| Surv  | Eden  | Old   | Free  |
+-------+-------+-------+-------+
```

### ZGC Colored Pointers on 64-Bit Architecture
ZGC utilizes 64-bit virtual memory addresses, dedicating high-order bits to metadata flags:
```
+--------------------+---------------------------------------+
| 16 bits | 4 bits   | 44 bits (16 Terabytes Virtual Address)|
| Unused  | Metadata | Object Memory Offset                  |
+--------------------+---------------------------------------+
          |
          +-> Finalizable | Remapped | Marked1 | Marked0
```
- Because reference pointers carry their own GC state, ZGC does not need to inspect object headers to determine if an object is live or relocated.
- When an object is moved, a **Load Barrier** checks the `Remapped` bit: if unmapped, it looks up the forwarding table, repairs the pointer, and updates the reference in place (**Self-Healing**).

---

## 4. Syntax & Usage

### Selecting and Tuning Collectors via JVM Flags
```bash
# 1. Standard G1 GC (Default in Java 9 to 25)
java -XX:+UseG1GC -XX:MaxGCPauseMillis=200 MyApp

# 2. Generational ZGC (Low-Latency Standard Java 21+)
java -XX:+UseZGC -XX:+ZGenerational MyApp

# 3. Shenandoah GC (Alternative Low-Pause)
java -XX:+UseShenandoahGC MyApp

# 4. Parallel GC (Max Throughput Batch Processing)
java -XX:+UseParallelGC MyApp
```

---

## 5. Practical Examples

### Example 1: Comparing Pause Latencies Under Load
```java
// Goal: Allocate high-frequency memory buffers and compare G1 vs ZGC pause metrics
public class GcLatencyBenchmark {
    public static void main(String[] args) {
        System.out.println("Allocating memory under: " + System.getProperty("java.vm.name"));
        long start = System.currentTimeMillis();

        for (int i = 0; i < 500_000; i++) {
            byte[] buffer = new byte[8192];
            if (buffer.length == 0) System.out.println();
        }

        long elapsed = System.currentTimeMillis() - start;
        System.out.println("Completed allocations in: " + elapsed + " ms");
    }
}
```
*Run with `-XX:+UseG1GC -Xlog:gc*` vs `-XX:+UseZGC -XX:+ZGenerational -Xlog:gc*` to compare pause times in GC logs.*

---

## 6. Best Practices
1. **Default to G1 for General Enterprise Workloads**: G1 GC is battle-tested, balanced, and the default in Java 17, 21, and 25.
2. **Switch to Generational ZGC for Latency SLAs**: If your SLA requires $99.9\text{th}$ percentile response times $< 10$ ms, enable `-XX:+UseZGC -XX:+ZGenerational`.
3. **Avoid Over-Tuning G1 Pause Targets**: Do not set `-XX:MaxGCPauseMillis=10` on G1. Setting unrealistic pause goals forces G1 into tiny Young generations, severely degrading application throughput.
4. **Use Parallel GC for Offline Batch Jobs**: For headless batch data processing, ML model training, or compile tasks where pause times do not matter, Parallel GC delivers 10%–15% higher raw throughput than concurrent collectors.
5. **Always Allocate Headroom for ZGC**: ZGC is a concurrent collector; allocate at least 25%–30% heap headroom above active data set size so the concurrent collector can outpace application allocation rates.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Setting G1 `MaxGCPauseMillis` unrealistically low | Forces Eden to shrink dramatically, causing constant GC cycles and CPU starvation. | Keep G1 target at 100ms–200ms or switch to ZGC. |
| Using ZGC without enough heap headroom | If allocation rate exceeds concurrent collection speed, ZGC encounters allocation stalls. | Provide 25%–30% memory headroom or tune thread count. |
| Forgetting `-XX:+ZGenerational` in Java 21 | In Java 21, ZGC was single-generational by default; generational mode must be explicitly enabled. | Pass `-XX:+ZGenerational`. |
| Allocating Humongous Objects in G1 | Objects larger than 50% of region size bypass Eden, fragmenting Old Gen regions. | Increase region size (`-XX:G1HeapRegionSize=16m`) or optimize buffers. |

---

## 8. Performance & Security Notes

### Load Barrier CPU Overhead in ZGC
ZGC achieves near-zero pause times by transferring relocation work to JIT-compiled **Load Barriers** executed during ordinary reference reads. This incurs a nominal 2%–3% CPU throughput penalty compared to G1, which is typically an acceptable trade-off for eliminating 500ms STW spikes.

---

## 9. Comparison / When to Use What

| Metric / Collector | Parallel GC | G1 GC | Generational ZGC |
|---|---|---|---|
| **Primary Goal** | Peak Throughput | Balanced Throughput & Pause | **Ultra-Low Latency** |
| **Max Pause Time** | Seconds | 100ms – 250ms | **< 1 Millisecond** |
| **Heap Scalability** | Up to 16 GB | 4 GB to 64 GB | **16 MB to 16 Terabytes**|
| **Default In** | Java 8 | **Java 9 to 25** | High-perf option in 21/25 |
| **Mechanism** | Stop-The-World | Region-based Mixed STW | **Concurrent Colored Pointers**|

---

## 10. Interview Questions

<details>
<summary>1. How does G1 GC differ from traditional generational collectors?</summary>
Traditional collectors divide the heap into large contiguous physical regions. G1 divides the heap into thousands of equal-sized virtual regions (1–32 MB) that act dynamically as Eden, Survivor, or Old. G1 collects regions with the most garbage first ("Garbage-First").
</details>

<details>
<summary>2. What is a Humongous Object in G1 GC?</summary>
An object whose size exceeds 50% of a G1 region. Humongous objects are allocated directly into a contiguous sequence of Old Generation regions, which can lead to memory fragmentation if allocated frequently.
</details>

<details>
<summary>3. What are Colored Pointers in ZGC?</summary>
A 64-bit pointer technique where metadata flags (Marked0, Marked1, Remapped) are stored directly inside unused high-order bits of the memory address itself, allowing ZGC to inspect reference state without dereferencing object headers.
</details>

<details>
<summary>4. What is a Load Barrier and how does Self-Healing work in ZGC?</summary>
A load barrier is an instruction emitted by the JIT compiler whenever a reference is loaded from the heap. If the object has been relocated, the load barrier intercepts the read, updates the reference to point to the new location (Self-Healing), and returns the new pointer.
</details>

<details>
<summary>5. What are the pause time guarantees of Generational ZGC in Java 21+?</summary>
ZGC guarantees pause times under 1 millisecond, independent of heap size (from megabytes to 16 terabytes).
</details>

<details>
<summary>6. What is a Mixed Collection in G1 GC?</summary>
A collection cycle where G1 reclaims all Young Generation regions plus a selected subset of the most fragmented Old Generation regions, reclaiming Old Gen memory incrementally without requiring a full stop-the-world pause.
</details>

<details>
<summary>7. When would you choose Parallel GC over G1 or ZGC?</summary>
For non-interactive batch workloads (data analytics, offline ETL jobs) where maximum CPU throughput is desired and multi-second pauses do not degrade system SLAs.
</details>

<details>
<summary>8. What is the role of Remembered Sets (RSets) in G1 GC?</summary>
RSets track which external regions contain references into the current region. This allows G1 to collect a region independently without scanning the entire heap to locate incoming pointers.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Identifying the Active GC
- **Task**: Write a program that queries `ManagementFactory.getGarbageCollectorMXBeans()` and prints the names of all active garbage collection managers.
<details>
<summary>Solution</summary>

```java
import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;

public class ActiveGcIdentifier {
    public static void main(String[] args) {
        System.out.println("Active Garbage Collectors:");
        for (GarbageCollectorMXBean gc : ManagementFactory.getGarbageCollectorMXBeans()) {
            System.out.println(" - " + gc.getName() + " (Collections: " + gc.getCollectionCount() + ", Time: " + gc.getCollectionTime() + " ms)");
        }
    }
}
```
</details>

### Exercise 2 (Medium): Simulating G1 Region Eviction
- **Task**: Run a memory allocation loop with `-XX:+UseG1GC -XX:+PrintFlagsFinal` and observe how G1 automatically calculates `G1HeapRegionSize` based on total heap size.
<details>
<summary>Solution</summary>

```bash
# Run with 4G heap and check calculated region size
java -Xms4g -Xmx4g -XX:+UseG1GC -XX:+PrintFlagsFinal -version | findstr G1HeapRegionSize
# Output will display 2MB region size (4096MB / 2048 regions = 2MB)
```
</details>

### Exercise 3 (Hard): Real-Time GC Pause Telemetry via JMX Listeners
- **Task**: Register a JMX notification listener on `GarbageCollectorMXBean` to capture and print real-time GC pause durations as they occur.
<details>
<summary>Solution</summary>

```java
import com.sun.management.GarbageCollectionNotificationInfo;
import java.lang.management.ManagementFactory;
import javax.management.*;
import javax.management.openmbean.CompositeData;

public class GcNotificationListenerDemo {
    public static void installListener() {
        for (var gcBean : ManagementFactory.getGarbageCollectorMXBeans()) {
            NotificationEmitter emitter = (NotificationEmitter) gcBean;
            emitter.addNotificationListener((notification, handback) -> {
                if (notification.getType().equals(GarbageCollectionNotificationInfo.GARBAGE_COLLECTION_NOTIFICATION)) {
                    CompositeData cd = (CompositeData) notification.getUserData();
                    GarbageCollectionNotificationInfo info = GarbageCollectionNotificationInfo.from(cd);
                    System.out.println("[GC EVENT]: " + info.getGcName() + " took " + info.getGcInfo().getDuration() + " ms");
                }
            }, null, null);
        }
    }

    public static void main(String[] args) throws Exception {
        installListener();
        System.out.println("GC listener active. Allocating memory...");
        for (int i = 0; i < 50; i++) {
            byte[] b = new byte[2 * 1024 * 1024];
            Thread.sleep(100);
        }
    }
}
```
</details>

---


### 11.4 Real-World Case Study: Migrating High-Throughput Microservice from G1 to Generational ZGC

Consider an ultra-low-latency financial ledger processing 120,000 requests per second with a 32 GB heap:

1. **Baseline Assessment under G1GC**:
   - Configuration: \-Xms32g -Xmx32g -XX:+UseG1GC -XX:MaxGCPauseMillis=20   - Observed P99 latency: 18 ms.
   - Observed P99.99 latency: 215 ms (caused by evacuation failures during sudden traffic bursts, forcing multi-second Full GC pauses).
   - Problem: When allocation rate exceeded G1 concurrent marking completion rate, G1 degraded into STW Full GC.

2. **Transition to Generational ZGC (Java 21/25)**:
   - Migration flag: \-XX:+UseZGC -XX:+ZGenerational\ (in Java 25, Generational ZGC is enabled by default with \-XX:+UseZGC\).
   - Sizing consideration: Allocate sufficient head-room so concurrent GC threads do not race against allocation threads.
   - Resulting metrics:
     - Average GC pause: 0.28 ms.
     - P99 latency: 2.1 ms.
     - P99.99 latency: 3.4 ms.
     - Evacuation failures: Completely eliminated due to continuous concurrent young generation collection.

3. **Trade-off Analysis Matrix**:
   | Attribute | Parallel GC | G1 GC | Shenandoah | Generational ZGC |
   |---|---|---|---|---|
   | Max Pause Goal | High throughput, pause unconstrained | 10 - 200 ms | < 10 ms | < 1 ms |
   | Memory Footprint Overhead | Lowest (< 3%) | Moderate (~5-10%) | Moderate (~5-10%) | High (~10-15% colored pointers/load barriers) |
   | Best Workload | Batch jobs, big data processing | General enterprise web apps | Low latency web services | Ultra-low latency microservices & trading |
   | Heap Scalability | Up to ~16 GB | 4 GB to 64 GB | 4 GB to 100+ GB | 16 GB to 16 TB |

4. **Monitoring Commands**:
   \\ash
   # Monitor real-time ZGC phases without logging to disk:
   jcmd <PID> GC.run
   jcmd <PID> VM.info
   \


### 11.5 Advanced Garbage Collection Tuning Parameters for Enterprise Scale

For complex workloads requiring deterministic latency profiles:

1. **G1 GC Fine-Tuning**:
   - `-XX:InitiatingHeapOccupancyPercent=45`: Threshold of old gen occupancy to start concurrent marking.
   - `-XX:G1ReservePercent=15`: Headroom buffer reserved to prevent evacuation failures during spikes.
   - `-XX:G1MixedGCCountTarget=8`: Distributes mixed collection work across 8 cycles to smooth pause times.
   - `-XX:G1HeapRegionSize=16m`: Explicitly size regions to avoid humongous object fragmentation (objects > 50% region size).

2. **ZGC Operational Tuning**:
   - `-XX:ConcGCThreads=4`: Adjust concurrent GC thread count if allocation rate outpaces default marking threads.
   - `-XX:ZAllocationSpikeTolerance=3`: Multiplier estimating how quickly allocations may surge, triggering earlier collection starts.
   - `-XX:+UnlockExperimentalVMOptions -XX:ZPath=/mnt/hugepages`: Place ZGC page mappings on Linux Hugetlbfs to eliminate TLB miss overhead in massive heaps.

3. **Diagnosing Uncommitted Memory with Linux Virtual Memory**:
   - ZGC aggressively uncommits unused heap pages back to the operating system by default (`-XX:+ZUncommit`), reducing cloud infrastructure billing.
   - Monitor RSS vs Virtual Size: `pmap -x <PID>` or `jcmd <PID> VM.native_memory baseline`.
   - Ensure the operating system `vm.max_map_count` is elevated (`sysctl -w vm.max_map_count=1048576`) to support multi-terabyte virtual memory address spaces required by ZGC colored pointers.
   - Monitor NUMA node allocation balance using `numastat -p <PID>` to ensure garbage collection threads run on the local socket where memory was allocated.



### 11.6 Generational ZGC Color Pointer Memory Layout Deep Dive

In 64-bit architectures, Generational ZGC uses reference pointer bits directly to track object metadata without object-header bloat:
- **Bits 0-43**: Object Offset (supports up to 16 Terabytes of physical address space).
- **Bits 44-47**: Metadata bits (Finalizable, Remapped, Marked0, Marked1).
- **Bits 48-63**: Unused / Reserved by hardware page tables.

When mutator threads load an object field, the HotSpot JIT compiler inserts a load barrier:
```assembly
# HotSpot Load Barrier Assembly pattern (x86_64)
testq %rax, %gs:ZGC_COLOR_BAD_MASK
jnz   .L_zgc_slow_path
```
If the pointer is good (colored with the current phase's expected metadata bit), execution proceeds with zero branch penalty or memory stall. If bad, it traps into a microscopic slow path where the GC thread immediately self-heals the reference by dereferencing the forwarding table, updating the pointer to point to the evacuated destination, and storing the healed good pointer back to the heap field. Subsequent thread access hits the fast path immediately.



- **Throughput vs Latency Tradeoff**: Parallel GC maximizes overall compute cycles dedicated to application work at the expense of multi-second STW pauses; Generational ZGC caps pauses strictly under 1 millisecond at the expense of ~10% sustained CPU barrier and concurrent marking overhead.
- **Allocation Stall Warning**: If mutator threads allocate memory faster than ZGC concurrent worker threads can evacuate and compact old generation pages, mutator threads will experience allocation stalls, degrading latency. Always provision at least 20-30% heap buffer headroom when deploying ZGC.
- **Shenandoah Pacer**: Shenandoah employs an internal pacing algorithm that throttles high-rate allocating threads for microseconds when a cycle falls behind, preventing full STW fallback.


- **Memory Footprint Accounting**: Ensure your Kubernetes memory requests and limits account for native memory overhead (Metaspace, thread stacks, JIT code cache, and native direct buffers), which typically adds 1.25x to 1.5x on top of -Xmx.
- **GC Logging Best Practice**: Always run with -Xlog:gc*,gc+phases=debug:file=/var/log/jvm/gc.log:time,tags:filecount=5,filesize=50m in production environments; the logging overhead is < 0.1% and provides critical forensic data during outages.

## 12. Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                MODERN GC CHEAT SHEET                                    |
+-----------------------------------------------------------------------------------------+
| G1 GC                    | -XX:+UseG1GC (Default; region grid, mixed collections)       |
| G1 Pause Goal            | -XX:MaxGCPauseMillis=200 (Default target)                    |
| Generational ZGC         | -XX:+UseZGC -XX:+ZGenerational (Sub-millisecond pauses)      |
| ZGC Architecture         | Colored Pointers (Metadata in 64-bit ptr) + Load Barriers    |
| Shenandoah GC            | -XX:+UseShenandoahGC (Concurrent compaction via Brooks ptr)  |
| Parallel GC              | -XX:+UseParallelGC (Max throughput, long pauses; batch jobs) |
| Sizing Rule for ZGC      | Provide 25% - 30% heap headroom for concurrent collection    |
+-----------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [JEP 439: Generational ZGC (Java 21)](https://openjdk.org/jeps/439)
- [JEP 474: Generational ZGC by Default (Java 25)](https://openjdk.org/jeps/474)
- Oracle G1 Garbage Collector Tuning Guide
