# Garbage Collection Tuning: Allocation Rates, Latency vs Throughput, and Modern Collectors

> Production GC engineering: G1, Shenandoah, and Generational ZGC internals, Allocation vs Promotion rates, Pause Time Guarantees, and Unified GC Logging.

| Metadata | Details |
|---|---|
| **Level** | Core / Advanced |
| **Prerequisites** | JVM Memory Areas, HotSpot GC Fundamentals |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: Generational ZGC, Colored Pointers, & Load Barriers](#3-how-it-works-internally-generational-zgc-colored-pointers--load-barriers)
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
In Java enterprise systems, the Garbage Collector (GC) is responsible for automatically reclaiming unused heap memory. However, automatic memory management is not free: historically, GC cycles required "Stop-The-World" (STW) pauses where all application threads were completely halted while the collector traversed object graphs and compacted memory.

In high-concurrency microservices and financial trading engines, a 500-millisecond pause can cause socket timeouts, trigger Kubernetes liveness probe failures, and drop client connections. Modern Java (Java 21/25 LTS) provides ultra-low latency concurrent garbage collectors—notably **Generational ZGC (JEP 439 / 474)**—capable of keeping maximum pause times strictly under **1 millisecond** across terabytes of heap. Understanding GC tuning flags, allocation vs promotion rates, and GC log forensics is essential for maintaining production SLA guarantees.

---

## 2. Core Concepts
- **Stop-The-World (STW) Pause**: Complete suspension of all mutator (application) threads at a JVM safepoint while the GC thread manipulates object pointers.
- **Weak Generational Hypothesis**: The foundational observation that most objects die very young (e.g., short-lived strings, temporary builders, stream pipeline elements).
- **Allocation Rate vs Promotion Rate**:
  - *Allocation Rate*: The volume of memory allocated by application threads per second (e.g., 2 GB/sec).
  - *Promotion Rate*: The volume of long-lived objects surviving young generation collections and moving into the Tenured (Old) generation per second.
- **Garbage-First (G1 GC)**: Default collector in modern Java. Partitions the heap into equal-sized regions, prioritizing regions with the most reclaimable garbage to satisfy a user-defined pause time target (`-XX:MaxGCPauseMillis=200`).
- **Generational ZGC (Java 21/25)**: Ultra-low latency concurrent collector using colored pointer metadata and JIT load barriers. Performs marking, relocation, and reference healing concurrently with mutator threads, guaranteeing sub-millisecond pauses.
- **Unified GC Logging (`-Xlog:gc*`)**: Standard JVM logging framework providing granular phase breakdowns and pause statistics.

---

## 3. How It Works Internally: Generational ZGC, Colored Pointers, & Load Barriers

Generational ZGC achieves sub-millisecond pauses by decoupling collection phases from STW pauses:

```
Mutator Thread executes: Object obj = account.getCustomer();
                               |
                               v
+---------------------------------------------------------------+
|               HotSpot JIT Emitted Load Barrier                |
|  - Inspects Colored Pointer bits (Bits 44-47 of reference)    |
|  - Check: Is pointer "Bad" (pointing to un-evacuated page)?   |
+---------------------------------------------------------------+
            /                                              /                                               v (Pointer is Good - 99.9% fast path)   v (Pointer is Bad - Slow path)
Direct CPU Memory Access (0 overhead)   Self-Healing: Reads forwarding table,
                                        updates reference to new evacuated page,
                                        stores Good pointer back to field!
```

### Colored Pointer Anatomy (64-Bit Architectures):
- **Bits 0–43** (44 bits): Object Virtual Address Offset (supports up to 16 Terabytes of heap).
- **Bits 44–47** (4 bits): Metadata Flags:
  - `Marked0` / `Marked1`: Track liveness across alternating GC cycles.
  - `Remapped`: Reference is updated and points to final evacuated destination.
  - `Finalizable`: Object is reachable only through clean-up finalizers.
- **Bits 48–63** (16 bits): Unused / Reserved by hardware page tables.

Because pointer metadata is encoded directly within the unused bits of 64-bit addresses, GC worker threads can relocate objects while application threads continue reading memory. If an application thread accesses an un-remapped object, the JIT load barrier automatically "self-heals" the pointer on the fly.

---

## 4. Syntax & Usage

### 4.1 Production JVM GC Configurations

#### A. Modern Default: G1 GC for General Microservices
```bash
java -XX:+UseG1GC      -Xms16g -Xmx16g      -XX:MaxGCPauseMillis=100      -XX:G1ReservePercent=15      -XX:InitiatingHeapOccupancyPercent=45      -XX:+ParallelRefProcEnabled      -Xlog:gc*,gc+phases=debug:file=/var/log/jvm/gc.log:time,uptime,pid:filecount=5,filesize=50m      -jar app.jar
```

#### B. Ultra-Low Latency: Generational ZGC (Java 21 / 25 LTS)
```bash
# In Java 21: -XX:+UseZGC -XX:+ZGenerational
# In Java 25: Generational ZGC is enabled by default with -XX:+UseZGC!
java -XX:+UseZGC      -Xms32g -Xmx32g      -XX:+ZGenerational      -XX:ConcGCThreads=4      -Xlog:gc*:file=/var/log/jvm/zgc.log:time,uptime:filecount=5,filesize=50m      -jar app.jar
```

---

## 5. Practical Examples

### Example 1: Reading and Analyzing Unified GC Logs
```text
# Sample Unified GC Log Output from G1:
[2026-11-20T14:30:10.124+0000][0.842s][info][gc,start    ] GC(12) Garbage Collection (G1 Evacuation Pause) (young)
[2026-11-20T14:30:10.138+0000][0.856s][info][gc          ] GC(12) Pause Young (Normal) (G1 Evacuation Pause) 1240M->312M(16384M) 14.120ms
[2026-11-20T14:30:10.138+0000][0.856s][info][gc,cpu      ] GC(12) User=0.08s Sys=0.01s Real=0.01s
```
**Forensic Breakdown**:
- `GC(12)`: The 12th collection event since JVM start.
- `G1 Evacuation Pause (young)`: Collected the young generation; evacuated surviving objects to Survivor/Old regions.
- `1240M->312M(16384M)`: Heap occupancy dropped from 1,240 MB to 312 MB; total heap capacity is 16,384 MB (16 GB).
- `14.120ms`: Total Stop-The-World pause duration.
- `User=0.08s Sys=0.01s Real=0.01s`: Parallel CPU core consumption vs real wall-clock pause.

### Example 2: Detecting Allocation Bursts with JFR
```bash
# Capture allocation rate spikes using JFR:
jcmd <PID> JFR.start name=alloc_trace settings=profile duration=30s filename=/tmp/alloc.jfr
# Open in JMC: Navigate to 'Memory' -> 'Allocations in new TLAB' -> Sort by Rate (MB/sec).
```

### Example 3: Edge Case - Humongous Allocations in G1
In G1 GC, any object exceeding 50% of the G1 Region Size is classified as a **Humongous Object** and allocated directly in contiguous Old generation regions:
```bash
# If GC logs show frequent `Pause Young (Concurrent Start) (G1 Humongous Allocation)`:
# Increase G1 region size to eliminate fragmented humongous pauses:
java -XX:+UseG1GC -XX:G1HeapRegionSize=16m -jar app.jar
```

---

## 6. Best Practices
1. **Set `-Xms` Equal to `-Xmx`**: Pre-allocate the entire heap at startup to prevent latency spikes caused by dynamic heap memory expansion from the OS.
2. **Always Enable Unified GC Logging in Production**: Logging overhead is $< 0.1\%$; set `-Xlog:gc*:file=/var/log/jvm/gc.log:time,uptime:filecount=5,filesize=50m`.
3. **Use Generational ZGC for Latency-Critical Services**: For microservices requiring P99.9 latency under 10 ms, migrate to Generational ZGC (`-XX:+UseZGC`).
4. **Size Heaps to Provide at least 30% Headroom**: In concurrent collectors (ZGC, Shenandoah), allocating threads run simultaneously with GC threads. Sizing heap too tightly causes allocation stalls.
5. **Tune `-XX:G1ReservePercent`**: Reserve 15–20% of heap memory to prevent G1 "to-space exhausted" evacuation failures during traffic surges.
6. **Eliminate Finalizers**: Never implement `finalize()`; finalizers stall GC cycle progression and cause catastrophic memory leaks. Use `java.lang.ref.Cleaner` instead.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Setting `-Xmx` too small for ZGC | Concurrent collection falls behind allocation rate, causing allocation stalls | Provide 30-40% heap buffer headroom |
| Disabling GC logging | Leaves operations teams blind during production latency spikes | Always enable unified `-Xlog:gc*` |
| Humongous object fragmentation in G1 | Objects $> 50\%$ region size force premature Full GC | Increase `-XX:G1HeapRegionSize` to 16m or 32m |
| Incompatible pause target (`MaxGCPauseMillis=1`) | Unrealistic goal forces G1 to run continuous CPU-thrashing collections | Keep G1 pause target between 50 and 200 ms |
| Using `System.gc()` in production | Triggers Full Stop-The-World garbage collection | Add `-XX:+DisableExplicitGC` to JVM flags |
| Ignoring Reference Processing pauses | Weak/Soft references stall GC pauses walking reference lists | Enable `-XX:+ParallelRefProcEnabled` in G1 |
| Unbalanced NUMA memory nodes | Cross-socket memory traffic degrades GC scanning performance | Run with `-XX:+UseNUMA` on multi-socket servers |
| High allocation rate mistaken for memory leak | Memory floor recovers after Full GC, but frequent young GC hurts CPU | Reduce temporary object churn in code |

---

## 8. Performance & Security Notes
- **Throughput vs Latency Trade-Off**:
  - *Parallel GC*: Maximum compute throughput ($\sim 98\%$ CPU dedicated to application work), but incurs multi-second STW pauses.
  - *G1 GC*: Balanced throughput ($\sim 95\%$) and controlled pause targets (50–200 ms).
  - *Generational ZGC*: Maximum responsiveness ($< 1	ext{ ms}$ pauses), but incurs $\sim 5-10\%$ throughput overhead due to JIT load barriers and concurrent threads.
- **Cloud Infrastructure Uncommit**: ZGC automatically uncommits unused memory pages back to the Linux kernel (`-XX:+ZUncommit`), reducing RAM usage and cloud hosting costs during low traffic.

---

## 9. Comparison / When to Use What

| Collector | Max Pause Target | Concurrency Model | CPU Overhead | Best Use Case |
|---|---|---|---|---|
| **Parallel GC** | Unconstrained (Seconds) | Stop-The-World | Lowest (< 2%) | Batch jobs, offline data processing, HPC |
| **G1 GC (Default)** | 50 – 200 ms | Incremental / STW Young | Moderate (~5%) | General enterprise web apps, microservices |
| **Shenandoah** | < 10 ms | Concurrent (Brooks Pointers) | High (~8-10%) | Low-latency microservices |
| **Generational ZGC**| **< 1 ms** | Concurrent (Colored Pointers) | High (~8-10%) | Ultra-low latency trading, real-time gaming, massive heaps |

---

## 10. Interview Questions

<details>
<summary>1. What is the Weak Generational Hypothesis and how do generational garbage collectors exploit it?</summary>
The Weak Generational Hypothesis states that the vast majority of allocated objects survive for only a very short duration before becoming unreachable (e.g., local method variables, strings, temporary DTOs).<br>
Generational collectors exploit this by partitioning the heap into Young and Old (Tenured) generations. The Young generation is collected frequently using fast copying algorithms that only traverse surviving objects (which are few), while the Old generation is collected less frequently, minimizing overall CPU scanning overhead.
</details>

<details>
<summary>2. How does Generational ZGC achieve sub-millisecond pause times across multi-terabyte heaps?</summary>
Generational ZGC executes almost all collection work—object marking, reference resolution, and object relocation—concurrently while application mutator threads are actively running. It achieves this using <b>Colored Pointers</b> (embedding metadata bits in 64-bit addresses) and <b>JIT-compiled Load Barriers</b>. If an application thread accesses an object currently being relocated, the load barrier intercepts the read, redirects the pointer using a forwarding table, and self-heals the reference in nanoseconds without an STW pause.
</details>

<details>
<summary>3. What is an Allocation Stall in concurrent collectors like ZGC?</summary>
An allocation stall occurs when mutator threads allocate memory faster than concurrent GC worker threads can reclaim and compact memory pages. If free memory is completely exhausted, the JVM is forced to stall (pause) the allocating thread until the GC cycle frees up pages, degrading application latency. Provisioning adequate heap headroom (at least 30%) prevents allocation stalls.
</details>

<details>
<summary>4. What are Humongous Objects in G1 GC and why are they problematic?</summary>
In G1, an object whose size exceeds 50% of the G1 Region Size is classified as Humongous. Humongous objects are not allocated in the young generation; they are allocated directly in contiguous regions in the Old generation. They can cause premature Old generation concurrent marking cycles, cause region fragmentation, and cannot be relocated easily during normal evacuation pauses. Increasing <code>-XX:G1HeapRegionSize</code> resolves this.
</details>

<details>
<summary>5. What is the difference between Allocation Rate and Promotion Rate?</summary>
<b>Allocation Rate</b> is the speed at which application threads allocate new memory in the Eden space (e.g., 500 MB/sec).<br>
<b>Promotion Rate</b> is the speed at which surviving objects transition from the Young generation into the Old generation (e.g., 10 MB/sec). A sudden spike in promotion rate indicates that survivor spaces are overflowing, leading to premature aging and triggering expensive Old generation collection cycles.
</details>

<details>
<summary>6. Why should -Xms be configured equal to -Xmx in production environments?</summary>
If <code>-Xms</code> is smaller than <code>-Xmx</code>, the JVM requests additional virtual memory pages from the operating system dynamically as heap occupancy increases. Requesting and zeroing memory pages from the OS introduces significant latency spikes during traffic surges. Sizing them equally pre-commits the full memory address space at startup.
</details>

<details>
<summary>7. What is the significance of -XX:+ParallelRefProcEnabled?</summary>
By default in older G1 versions, processing non-strong references (WeakReference, SoftReference, PhantomReference, FinalReference) was executed by a single thread during STW pauses. In applications using heavy caching or reflection frameworks, reference processing could take 50+ ms. Enabling <code>-XX:+ParallelRefProcEnabled</code> instructs the JVM to process reference queues concurrently using multiple GC worker threads.
</details>

<details>
<summary>8. How do you distinguish between a memory leak and high allocation churn in GC logs?</summary>
- <b>Memory Leak</b>: The minimum heap occupancy immediately following a Full GC or major collection cycle steadily climbs over hours or days, forming a "sawtooth" pattern where the trough continuously rises until an <code>OutOfMemoryError</code> occurs.<br>
- <b>Allocation Churn</b>: Collections occur very frequently (e.g., every 500 ms), but memory consistently drops back to a stable, low baseline after every collection.
</details>

---


<details>
<summary>9. What is String Deduplication in G1 GC and how does it save memory?</summary>
In enterprise applications, 20% to 30% of total heap memory is frequently occupied by duplicate String values (e.g., country codes, status strings). G1 includes an automated String Deduplication feature (<code>-XX:+UseStringDeduplication</code>). During young collection evacuation, G1 inspects candidate strings, compares their underlying byte arrays, and modifies duplicate String instances to point to the exact same shared <code>byte[]</code> array, freeing up megabytes of heap memory without requiring manual <code>String.intern()</code> calls.
</details>

<details>
<summary>10. How does Metaspace sizing differ from heap sizing in modern Java?</summary>
Metaspace resides in native operating system memory outside the Java heap. By default, its maximum size is unbounded (<code>-XX:MaxMetaspaceSize=unlimited</code>), meaning an uncontrolled classloader leak can consume all physical host RAM until the operating system crashes or invokes the OOM-killer. In production, always set an explicit upper bound (e.g., <code>-XX:MaxMetaspaceSize=512m</code>) to ensure class metadata memory leaks are caught and bounded.
</details>

## 11. Practice Exercises

<details>
<summary>Exercise 1: Write a script parsing Unified GC logs to calculate maximum and P99 pause times.</summary>

```python
import re, sys

pauses = []
with open("gc.log", "r") as f:
    for line in f:
        # Match pattern: Pause Young (...) 14.120ms
        m = re.search(r'Pause.*?(\d+\.\d+)ms', line)
        if m:
            pauses.append(float(m.group(1)))

pauses.sort()
print(f"Total collections: {len(pauses)}")
print(f"Max pause: {pauses[-1]:.2f} ms")
print(f"P99 pause: {pauses[int(len(pauses) * 0.99)]:.2f} ms")
```
</details>

<details>
<summary>Exercise 2: Configure a microservice JVM flag set for Generational ZGC on a 16 GB server.</summary>

```bash
# Recommended production flags for Java 21 / 25:
export JAVA_OPTS="-XX:+UseZGC -XX:+ZGenerational -Xms12g -Xmx12g -XX:ConcGCThreads=4 -XX:+ExitOnOutOfMemoryError -Xlog:gc*:file=/var/log/jvm/zgc.log:time,uptime,pid:filecount=5,filesize=50m"
```
</details>

<details>
<summary>Exercise 3: Write a Java test program deliberately triggering G1 Humongous allocation.</summary>

```java
public class HumongousAllocationTest {
    public static void main(String[] args) {
        // With default 1MB region size, any byte array > 512 KB is Humongous!
        int humongousSize = 600 * 1024;
        for (int i = 0; i < 500; i++) {
            byte[] humongousBuffer = new byte[humongousSize];
        }
        System.out.println("Allocated 500 humongous buffers.");
    }
}
```
</details>

<details>
<summary>Exercise 4: Verify Generational ZGC colored pointer bit alignment on x86-64.</summary>

```java
public class ZgcMemoryVerification {
    public static void main(String[] args) {
        long addressableSpace = 1L << 44; // 44 bits offset
        System.out.println("ZGC Max Addressable Space: " + (addressableSpace / (1024L * 1024L * 1024L * 1024L)) + " Terabytes");
    }
}
```
</details>

---


### 11.4 Real-World Case Study: Sizing and Tuning Generational ZGC for a 100k Req/sec Payment Engine

Consider an enterprise payment gateway processing 100,000 transactions per second with a 32 GB heap under strict SLA requirements (P99.9 < 5 ms):

1. **Initial Baseline under G1 GC**:
   - Configuration: `-Xms32g -Xmx32g -XX:+UseG1GC -XX:MaxGCPauseMillis=20`
   - Observed Metrics: Average young GC pause: 18 ms.
   - P99.9 Latency: 210 ms (caused by G1 evacuation failures during sudden traffic bursts, forcing multi-second Full GC pauses).
   - Root Cause: Allocation rate during spikes reached 4.5 GB/sec, outpacing G1 concurrent marking and exhausting free survivor regions.

2. **Migration to Generational ZGC (Java 21/25)**:
   - Migration Configuration: `-XX:+UseZGC -XX:+ZGenerational -Xms32g -Xmx32g -XX:ConcGCThreads=4`
   - Resulting Metrics:
     - Average GC pause: 0.35 ms.
     - P99 Latency: 2.1 ms.
     - P99.9 Latency: 3.8 ms.
     - Evacuation Failures / STW Full GCs: Completely eliminated.

3. **Key Architectural Trade-Off Analysis**:
   - **Throughput Impact**: Total CPU utilization increased by ~6% due to concurrent worker threads and load barriers.
   - **Headroom Necessity**: Provisioned heap was increased from 24 GB to 32 GB to provide the recommended 35% headroom buffer, ensuring mutator threads never experience allocation stalls during concurrent relocation phases.

4. **Production Diagnostic Commands**:
   ```bash
   # Trigger ad-hoc GC run to verify memory reclamation:
   jcmd <PID> GC.run

   # Inspect real-time ZGC heap status:
   jcmd <PID> GC.heap_info

   # Dump JVM virtual memory maps:
   jcmd <PID> VM.vmmaps
   ```

### 11.5 Detailed GC Sizing Matrix by Enterprise Workload

| Workload Type | Preferred Collector | Recommended Sizing Strategy | Critical JVM Flags |
|---|---|---|---|
| **Batch / Big Data (Spark, ETL)** | Parallel GC | Set `-Xms` = `-Xmx` to 90% container RAM | `-XX:+UseParallelGC` |
| **Standard REST Microservice** | G1 GC | Set pause target to 100 ms, reserve 15% space | `-XX:+UseG1GC -XX:MaxGCPauseMillis=100 -XX:G1ReservePercent=15` |
| **Ultra-Low Latency Trading** | Generational ZGC | Provide 35%+ headroom, tune concurrent threads | `-XX:+UseZGC -XX:+ZGenerational -XX:ConcGCThreads=4` |
| **Cloud Native Lambdas / CLI** | Serial GC / Epsilon | Minimal heap, zero concurrent thread footprint | `-XX:+UseSerialGC` or `-XX:+UseEpsilonGC` |



### 11.6 Production Outage Playbook: Diagnosing and Resolving OutOfMemoryErrors

When a service in production triggers `java.lang.OutOfMemoryError`, follow this systematic triage sequence:

1. **Classify the OutOfMemoryError Subtype**:
   - `java.lang.OutOfMemoryError: Java heap space`: The JVM cannot allocate an object because the heap is saturated with live references or suffering from an acute memory leak.
   - `java.lang.OutOfMemoryError: Metaspace`: Class metadata memory has exceeded `-XX:MaxMetaspaceSize`. Typically caused by uncontrolled class generation (e.g., dynamic proxies, CGLIB, reflection classloader leaks).
   - `java.lang.OutOfMemoryError: Unable to create native thread`: The operating system has exhausted thread handles or memory stacks (`-Xss`). Inspect `ulimit -u` and kernel thread limits.
   - `java.lang.OutOfMemoryError: Direct buffer memory`: Off-heap `ByteBuffer.allocateDirect()` has exceeded `-XX:MaxDirectMemorySize`.

2. **Automate Heap Dump on Crash**:
   Ensure all production JVMs run with automated heap dump flags:
   ```bash
   -XX:+HeapDumpOnOutOfMemoryError    -XX:HeapDumpPath=/var/log/jvm/dumps/heap_dump_%p.hprof    -XX:+ExitOnOutOfMemoryError
   ```

3. **Analyzing the Heap Dump with Eclipse MAT (Memory Analyzer Tool)**:
   - **Dominator Tree**: Shows the top 20 root objects holding 80%+ of total retained heap memory.
   - **Duplicate Strings**: Identifies identical string literals replicated across thousands of instances; solve via `String.intern()` or compact representations.
   - **Unbounded Collections**: Flags static maps, unbounded queues, or uncleaned ThreadLocal variables retaining dead session instances.

4. **Reference Processing Bottlenecks**:
   - Inspect GC logs for high `Reference Processing` times.
   - If applications rely heavily on `WeakReference`, `SoftReference`, or Cleaners, ensure multi-threaded reference processing is active:
     `-XX:+ParallelRefProcEnabled` (in G1 GC).



<details>
<summary>Exercise 5: Configure automated String Deduplication in G1 GC.</summary>

`ash
# Enable G1 string deduplication with 3-cycle age threshold:
java -XX:+UseG1GC      -XX:+UseStringDeduplication      -XX:StringDeduplicationAgeThreshold=3      -Xlog:gc+stringdedup=debug      -jar app.jar
# Inspect logs to observe deduplicated bytes and freed memory!
`
</details>


- **Container Cgroup Limits**: Ensure -XX:MaxRAMPercentage=75.0 is configured so that container memory quotas account for native JVM memory (Metaspace, thread stacks, JIT compiler buffers) on top of the Java heap.
- **Continuous Latency Monitoring**: Export GC pause times to Prometheus via Micrometer JVM metrics (jvm.gc.pause) to trigger proactive alerts before pauses violate SLO boundaries.

## 12. Summary / Cheat Sheet
- **Generations**: Weak Generational Hypothesis; Young (frequent, fast copy), Old (infrequent).
- **Modern Default**: G1 GC balances throughput and pause targets (`-XX:MaxGCPauseMillis=100`).
- **Sub-Millisecond Latency**: Generational ZGC (`-XX:+UseZGC`) guarantees $< 1	ext{ ms}$ pauses via colored pointers.
- **Headroom**: Concurrent collectors require 30–40% heap headroom to prevent allocation stalls.
- **Humongous Objects**: G1 objects $> 50\%$ region size allocate directly in Old gen; increase region size.
- **Logging**: Always configure `-Xlog:gc*` for forensic pause time analysis.

---

## 13. Further Reading
- OpenJDK JEP 439 & 474: Generational ZGC Architecture
- Oracle Garbage-First Garbage Collector Tuning Guide
- Stefan Karlsson and Per Liden: ZGC - Low Latency GC with Colored Pointers
- *Java Performance: In-Depth Advice for Tuning and Acceleration* by Scott Oaks
