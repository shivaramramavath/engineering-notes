# JVM Metrics and Diagnostics in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 12-observability-and-operations |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Configuration Reference](#4-syntax--configuration-reference)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
A Spring Boot application may pass all functional and integration tests, yet fail catastrophically in production due to underlying HotSpot JVM resource exhaustion:
- Uncontrolled object allocations trigger frequent Garbage Collection (GC) Stop-The-World (STW) pauses, causing latency spikes and client timeouts.
- Memory leaks in Tenured (Old Gen) memory slowly drive the JVM toward an unrecoverable `OutOfMemoryError: Java heap space`.
- Thread contention, locks, or carrier thread pinning can block hundreds of threads, starving incoming HTTP traffic.
- Metaspace exhaustion caused by dynamic class loading (CGLIB proxies, reflection) triggers an `OutOfMemoryError: Metaspace`.

### Historical Evolution
In early Java releases, JVM inspection required intrusive remote JMX connections over RMI (requiring custom open ports and insecure firewalls) or manual server logins with command-line tools like `jstat`, `jmap`, and `jstack`.

In modern Spring Boot 3.x on Java 21, **Micrometer JVM Binders** automatically expose high-resolution JVM telemetry over Actuator endpoints (`/actuator/metrics/jvm.*`, `/actuator/prometheus`). Furthermore, modern low-latency collectors (**G1GC**, **Generational ZGC**) and built-in **JDK Flight Recorder (JFR)** integration provide zero-overhead diagnostic telemetry directly to production monitoring dashboards.

### Production Relevance
Deep JVM observability enables operations and engineering teams to:
- Detect slow memory leaks days before an outage occurs.
- Monitor GC pause duration distributions and identify GC-induced latency violations.
- Right-size container CPU and RAM requests/limits in Kubernetes, cutting cloud infrastructure costs.
- Diagnose thread contention, thread starvation, and thread deadlocks in real-time.

---

## 2. Core Concepts

### JVM Memory Regions
1. **Young Generation**: Where newly instantiated objects are allocated.
   - **Eden Space**: Primary landing area for new objects.
   - **Survivor Spaces (S0 / S1)**: Objects that survive Minor GC cycles migrate between survivor spaces before tenure promotion.
2. **Old (Tenured) Generation**: Long-lived objects that survived multiple GC generation thresholds (e.g. caches, Spring singleton beans, thread pool workers).
3. **Metaspace (Non-Heap)**: Stores class metadata, bytecode definitions, runtime constants, and reflection artifacts. Allocated in native OS memory (not within `-Xmx`).
4. **Direct / Off-Heap Buffers**: Native memory allocated via `ByteBuffer.allocateDirect()` (used heavily by Netty, reactive WebFlux, and database drivers).
5. **CodeCache (Non-Heap)**: Memory where the JIT compiler stores compiled native machine code.

### Modern Garbage Collectors
- **G1GC (Garbage-First)**: Default collector in Java 21 LTS. Divides heap into equal-sized regions; balances throughput and latency (typical pauses 50–200ms).
- **Generational ZGC**: Ultra-low-latency collector in Java 21 LTS. Pauses are consistently **under 1 millisecond**, independent of heap size (from 16MB to 16TB).
- **Shenandoah GC**: Ultra-low-latency concurrent evacuating collector designed to minimize STW pause times.

---

## 3. How It Works Internally

### JVM Memory Architecture & Allocation Flow

```
HOTSPOT JVM RUNTIME (JAVA 21)
+------------------------------------------------------------------------------------+
|                                    HEAP MEMORY (-Xms / -Xmx)                       |
|                                                                                    |
|  +--------------------------------------------+  +-------------------------------+ |
|  |           YOUNG GENERATION                 |  |       OLD GENERATION          | |
|  |                                            |  |        (TENURED)              | |
|  |  +--------------------+  +---------------+ |  |                               | |
|  |  |     EDEN SPACE     |  | SURVIVOR (S0) | |  | Long-lived objects, singletons| |
|  |  |  (New allocations) |  +---------------+ |  | promoted from Survivor after  | |
|  |  |                    |  | SURVIVOR (S1) | |  | aging threshold               | |
|  |  +--------------------+  +---------------+ |  |                               | |
|  +--------------------------------------------+  +-------------------------------+ |
+------------------------------------------------------------------------------------+
                                          |
+------------------------------------------------------------------------------------+
|                               NON-HEAP & NATIVE MEMORY                             |
|                                                                                    |
|  +------------------------+  +-------------------------+  +---------------------+  |
|  |       METASPACE        |  |       CODE CACHE        |  |   DIRECT MEMORY     |  |
|  |  Class metadata, CGLIB |  |  JIT-compiled native    |  |  Netty / Off-heap   |  |
|  |  proxies, bytecodes    |  |  machine instructions   |  |  ByteBuffers        |  |
|  +------------------------+  +-------------------------+  +---------------------+  |
+------------------------------------------------------------------------------------+
                                          |
                                          v
+------------------------------------------------------------------------------------+
|                         MICROMETER JVM METRICS BINDERS                             |
|  JvmMemoryMetrics, JvmGcMetrics, JvmThreadMetrics, ClassLoaderMetrics              |
|  Samples HotSpot MXBeans: MemoryMXBean, GarbageCollectorMXBean, ThreadMXBean       |
|  Exposed to: /actuator/prometheus -> PromQL Grafana Dashboard                      |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Configuration Reference

### Actuator JVM Metrics Configuration (`application.yml`)
```yaml
management:
  metrics:
    enable:
      jvm: true
    distribution:
      percentiles-histogram:
        jvm.gc.pause: true # Enables histogram buckets for GC pause quantiles
      sla:
        jvm.gc.pause: 10ms, 50ms, 100ms, 250ms
  endpoints:
    web:
      exposure:
        include: [ "health", "info", "metrics", "prometheus" ]
```

### Recommended JVM Flags for Java 21 Containers (Docker / K8s)
```bash
# Production Container JVM Options
JAVA_OPTS="\
  -XX:+UseG1GC \
  -XX:MaxRAMPercentage=75.0 \
  -XX:InitialRAMPercentage=75.0 \
  -XX:+ExitOnOutOfMemoryError \
  -XX:+HeapDumpOnOutOfMemoryError \
  -XX:HeapDumpPath=/var/log/dumps/oom.hprof \
  -Xlog:gc*,gc+phases=debug:file=/var/log/gc.log:time,uptime,pid:filecount=5,filesize=50m \
  -Djava.security.egd=file:/dev/./urandom"
```

---

## 5. Practical Examples

### Example 1: Querying JVM Metrics via Spring Boot Actuator
```bash
# 1. Inspect total JVM memory used vs max
curl -X GET http://localhost:9090/actuator/metrics/jvm.memory.used

# Sample Output:
# {"name":"jvm.memory.used","measurements":[{"statistic":"VALUE","value":342891280.0}],"availableTags":[{"tag":"area","values":["heap","nonheap"]},{"tag":"id","values":["G1 Eden Space","G1 Old Gen","Metaspace",...]}]}

# 2. Inspect GC Pause durations
curl -X GET http://localhost:9090/actuator/metrics/jvm.gc.pause

# 3. Inspect Live Thread counts
curl -X GET http://localhost:9090/actuator/metrics/jvm.threads.live
```

### Example 2: Programmatically Inspecting JVM Memory State
```java
package com.example.observability.jvm;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;
import java.lang.management.MemoryUsage;

@Component
public class JvmHealthWatcher {

    private static final Logger log = LoggerFactory.getLogger(JvmHealthWatcher.class);
    private final MemoryMXBean memoryMXBean = ManagementFactory.getMemoryMXBean();

    @Scheduled(fixedRate = 30000) // Check every 30 seconds
    public void evaluateMemoryHealth() {
        MemoryUsage heapUsage = memoryMXBean.getHeapMemoryUsage();
        long usedMb = heapUsage.getUsed() / (1024 * 1024);
        long maxMb = heapUsage.getMax() / (1024 * 1024);
        double utilization = ((double) heapUsage.getUsed() / heapUsage.getMax()) * 100.0;

        if (utilization > 85.0) {
            log.warn("CRITICAL: Heap utilization is at {}% (Used: {} MB, Max: {} MB). High GC pressure anticipated!",
                String.format("%.2f", utilization), usedMb, maxMb);
        } else {
            log.debug("JVM Heap normal: {}% utilized ({} MB / {} MB)",
                String.format("%.2f", utilization), usedMb, maxMb);
        }
    }
}
```

### Example 3: Essential PromQL Queries for JVM Diagnostics

```promql
# 1. Heap Memory Saturation Percentage (Across All Pods)
(jvm_memory_used_bytes{area="heap"} / jvm_memory_max_bytes{area="heap"}) * 100

# 2. Total GC Pause Time Rate (Seconds spent pausing per second)
sum(rate(jvm_gc_pause_seconds_sum[1m])) by (application, instance)

# 3. 99th Percentile GC Pause Latency
histogram_quantile(0.99, sum(rate(jvm_gc_pause_seconds_bucket[5m])) by (le))

# 4. Old Generation Promotion Rate (MB/sec promoted from Young to Old)
rate(jvm_gc_memory_allocated_bytes_total[1m]) / (1024 * 1024)

# 5. Blocked / Waiting Thread Count
jvm_threads_states_threads{state="blocked"}
```

### Example 4: Anti-Pattern vs. Production Fix (Explicit System.gc() Calls)
```java
// ANTI-PATTERN: Manually calling System.gc() in code
// Calling System.gc() triggers a Full Stop-The-World GC cycle, halting all processing threads!
@Service
public class BadBatchJobService {
    public void processBigBatch() {
        processAllRecords();
        // CATASTROPHIC IN PRODUCTION: Halts entire JVM for 2-10 seconds!
        System.gc(); 
    }
}

// PRODUCTION FIX: Trust JVM Collector & Disable Explicit GC via JVM flag
// 1. Let the generational collector manage reclamation naturally
// 2. Add JVM flag in production: -XX:+DisableExplicitGC
@Service
public class OptimizedBatchJobService {
    public void processBigBatch() {
        processAllRecords();
        // Finished: Let G1GC or ZGC collect temporary objects concurrently without any manual trigger
    }
}
```

### Example 5: Detecting Thread Deadlocks Programmatically
```java
package com.example.observability.jvm;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;

@Component
public class DeadlockDetector {

    private static final Logger log = LoggerFactory.getLogger(DeadlockDetector.class);
    private final ThreadMXBean threadMXBean = ManagementFactory.getThreadMXBean();

    @Scheduled(fixedRate = 60000) // Check every 60 seconds
    public void checkForDeadlocks() {
        long[] deadlockedThreadIds = threadMXBean.findDeadlockedThreads();

        if (deadlockedThreadIds != null && deadlockedThreadIds.length > 0) {
            log.error("CRITICAL ALARM: {} DEADLOCKED THREADS DETECTED IN JVM!", deadlockedThreadIds.length);
            ThreadInfo[] threadInfos = threadMXBean.getThreadInfo(deadlockedThreadIds, true, true);
            for (ThreadInfo info : threadInfos) {
                log.error("Deadlocked Thread: [name: '{}', id: {}, state: {}, lockName: '{}', lockOwner: '{}']",
                    info.getThreadName(), info.getThreadId(), info.getThreadState(), 
                    info.getLockName(), info.getLockOwnerName());
            }
        }
    }
}
```

---

## 6. Best Practices & Production Guidelines

1. **Use `MaxRAMPercentage` Instead of Fixed `-Xmx` in Containers**: Set `-XX:MaxRAMPercentage=75.0`. This instructs the JVM to automatically adjust its max heap size dynamically based on the Kubernetes container memory limit, avoiding Linux OOM killer termination.
2. **Always Enable Heap Dump on OutOfMemoryError**: Include `-XX:+HeapDumpOnOutOfMemoryError -XX:HeapDumpPath=/var/log/dumps/oom.hprof`. Without the heap dump snapshot, post-mortem root cause analysis of production OOM crashes is virtually impossible.
3. **Use `-XX:+ExitOnOutOfMemoryError`**: When a JVM throws an `OutOfMemoryError`, internal state is corrupted. A lingering broken container can corrupt databases or produce corrupted data. Terminate the JVM immediately so Kubernetes can spin up a fresh pod.
4. **Enable Unified GC Logging**: GC logs have negligible overhead ($< 0.1\%$). Use `-Xlog:gc*:file=/var/log/gc.log:time,uptime,pid:filecount=5,filesize=50m` to capture all generational transitions and pause times for offline analysis with tools like GCeasy.
5. **Consider Generational ZGC for Latency-Critical Services**: If your SLA requires P99 response times under 15ms, switch to Generational ZGC via `-XX:+UseZGC -XX:+ZGenerational` (available in Java 21+).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Setting `-Xmx` equal to container memory limit | JVM native memory (Metaspace, thread stacks, direct buffers, JIT) exceeds container limit, triggering Linux cgroup OOM killer (`exit 137`). | Allocate at most 70–75% of container RAM to heap (`-XX:MaxRAMPercentage=75.0`). |
| Missing `-XX:+ExitOnOutOfMemoryError` | Pod remains running in a broken, half-dead state, failing customer requests indefinitely. | Set `-XX:+ExitOnOutOfMemoryError` so Kubernetes restarts the container immediately. |
| Calling `System.gc()` in code | Triggers an immediate Full STW GC, stopping all active request threads for seconds. | Remove manual calls and add `-XX:+DisableExplicitGC`. |
| Unbounded in-memory collections / caches | Static HashMaps or unbound caches grow indefinitely in Tenured Gen, causing eventual OOM. | Always use bounded caches (Caffeine) with eviction policies (e.g. `maximumSize`). |
| Ignoring Metaspace limits | Excessive dynamic class generation (CGLIB, ByteBuddy, reflection) leaks Metaspace until container crashes. | Monitor `jvm_memory_used_bytes{id="Metaspace"}` and cap with `-XX:MaxMetaspaceSize`. |
| Ignoring GC pause histograms | Average GC pause metrics hide 500ms tail latency spikes. | Enable `management.metrics.distribution.percentiles-histogram.jvm.gc.pause=true`. |

---

## 8. Performance & Security Notes

### Impact of GC on Latency
- During Stop-The-World (STW) pauses, all application threads (including HTTP acceptors and reactive event loops) are paused. Incoming TCP connections queue up in the OS listen backlog.
- If GC pause $> \text{client socket timeout}$, upstream API Gateways return HTTP 504 Gateway Timeout.

### JMX Security in Containers
- Avoid exposing JMX ports over public or container networks. JMX protocols lack modern authentication and can allow remote code execution if misconfigured.
- Rely on Actuator and Micrometer Prometheus scraping over HTTP instead of remote JMX.

---

## 9. Comparison / When to Use What

| Garbage Collector | Target Latency | Throughput | Best Used For |
|---|---|---|---|
| **G1GC (Default)** | 50ms – 200ms | High | Standard enterprise Spring Boot REST microservices and batch jobs |
| **Generational ZGC** | $< 1\text{ms}$ | Medium-High | Real-time APIs, financial trading, ultra-low latency SLAs ($< 15\text{ms}$) |
| **Parallel GC** | High pauses (seconds) | Maximum | Pure offline batch processing / Spring Batch with no interactive users |
| **Shenandoah GC** | 5ms – 20ms | Medium | Low-latency workloads on platforms where ZGC is unsupported |

---

## 10. Interview Questions & Answers

### Q1: What is the difference between Heap memory and Metaspace in Java 21?
<details>
<summary>Click to view answer</summary>

- **Heap Memory**: Managed by `-Xmx`. Holds all instantiated Java objects, arrays, and primitive values. Collected by the Garbage Collector.
- **Metaspace**: Allocated in native process memory (outside `-Xmx`). Holds class definitions, method metadata, bytecode representations, annotations, and constant pools. Metaspace is collected only when its corresponding `ClassLoader` is garbage collected.
</details>

### Q2: Why does setting `-Xmx` to 4GB inside a 4GB Docker container cause an OOM kill (Exit Code 137)?
<details>
<summary>Click to view answer</summary>

`-Xmx` controls **only the Java Heap**. Total JVM memory consumption includes:
$\text{Total Memory} = \text{Heap} + \text{Metaspace} + \text{Thread Stacks (} 1\text{MB per thread)} + \text{CodeCache} + \text{Direct Buffers} + \text{JVM Native Overhead}$.
If the heap is allocated 4GB, native allocations will push the total process memory beyond 4GB, causing the Linux kernel cgroup OOM killer to immediately send `SIGKILL` (exit code 137).
</details>

### Q3: What is the purpose of `-XX:+ExitOnOutOfMemoryError`?
<details>
<summary>Click to view answer</summary>

When a JVM encounters an `OutOfMemoryError`, it is in an indeterminate and potentially corrupt state. Without this flag, the JVM often continues running with broken worker threads, dropping requests or corrupting transactions. This flag forces the JVM process to exit immediately on OOM, allowing Kubernetes to detect the termination and spin up a healthy replacement pod.
</details>

### Q4: How does Generational ZGC in Java 21 achieve sub-millisecond pause times?
<details>
<summary>Click to view answer</summary>

Generational ZGC performs almost all garbage collection work **concurrently** while Java application threads continue executing. It uses colored pointers (reference metadata in pointer bits), load barriers, and separate young and old generational regions. The few remaining Stop-The-World phases (initial mark, thread stack scanning) take less than 1 millisecond regardless of whether the heap is 1GB or 1TB.
</details>

### Q5: What is a Thread Dump and what JVM thread states can it reveal?
<details>
<summary>Click to view answer</summary>

A thread dump is a snapshot of all active JVM threads and their execution call stacks. Key states include:
- `RUNNABLE`: Thread is executing in the JVM or waiting for OS CPU scheduling.
- `BLOCKED`: Thread is waiting to acquire a Java monitor lock held by another thread.
- `WAITING`: Thread is waiting indefinitely for another thread to perform an action (`Object.wait()`, `LockSupport.park()`).
- `TIMED_WAITING`: Thread is waiting for a specified timeout (`Thread.sleep()`, `poll(timeout)`).
</details>

### Q6: How do you detect a memory leak using JVM metrics?
<details>
<summary>Click to view answer</summary>

Inspect the `jvm_memory_used_bytes{area="heap", id="*Old Gen*"}` metric over time. While the young generation repeatedly allocates and empties, the Old Generation should stabilize after periodic major GC cycles. If the **minimum baseline of Old Gen usage continuously increases over time** despite major GC collections, a memory leak is retaining uncollectable objects.
</details>

### Q7: What is the difference between Direct Memory and Heap Memory?
<details>
<summary>Click to view answer</summary>

Direct Memory is allocated in native OS memory via `ByteBuffer.allocateDirect()` using JNI. It bypasses the Java garbage-collected heap, enabling native zero-copy I/O between the OS network stack and storage (used extensively by Netty, Spring WebFlux, and JDBC drivers). It is bounded by `-XX:MaxDirectMemorySize`.
</details>

### Q8: What are Carrier Threads and how do they relate to Virtual Threads in Java 21?
<details>
<summary>Click to view answer</summary>

Carrier threads are standard platform OS threads managed by a `ForkJoinPool` that execute Virtual Threads. When a Virtual Thread encounters a blocking I/O operation (socket read, file read), it unmounts from its carrier thread, allowing the carrier thread to execute other virtual threads. If synchronized blocks or native methods are used, the virtual thread becomes **pinned** to the carrier thread, preventing unmounting.
</details>

---

## 11. Practice Exercises

### Exercise 1: Configure Container Memory Bounds via application.yml / Env (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write the environment variable string to pass to a Docker container to ensure the JVM uses at most 75% of the container memory, exits on OOM, and uses G1GC.

**Solution**:
```bash
JAVA_TOOL_OPTIONS="-XX:MaxRAMPercentage=75.0 -XX:+UseG1GC -XX:+ExitOnOutOfMemoryError"
```
</details>

### Exercise 2: Programmatic Thread Count Alerter (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Implement a Spring `@Component` that monitors active platform threads and logs an ERROR if total threads exceed 500.

**Solution**:
```java
package com.example.observability.jvm;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

import java.lang.management.ManagementFactory;
import java.lang.management.ThreadMXBean;

@Component
public class ThreadCapacityMonitor {

    private static final Logger log = LoggerFactory.getLogger(ThreadCapacityMonitor.class);
    private final ThreadMXBean threadBean = ManagementFactory.getThreadMXBean();
    private static final int THREAD_THRESHOLD = 500;

    @Scheduled(fixedRate = 15000)
    public void monitorThreads() {
        int liveThreads = threadBean.getThreadCount();
        int peakThreads = threadBean.getPeakThreadCount();

        if (liveThreads > THREAD_THRESHOLD) {
            log.error("THREAD CAPACITY ALERT: Live thread count ({}) exceeded threshold of {}! Peak was {}.",
                liveThreads, THREAD_THRESHOLD, peakThreads);
        }
    }
}
```
</details>

### Exercise 3: Complete Dockerfile with Diagnostic Tooling (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a production multi-stage Dockerfile based on Eclipse Temurin 21 with JRE that includes `jcmd`, sets container memory options, non-root user, and exposes the Actuator port.

**Solution**:
```dockerfile
# Stage 1: Build & Package (Optional if jar pre-built)
FROM eclipse-temurin:21-jre-alpine AS runtime

# Install diagnostic utilities (jcmd, curl)
RUN apk add --no-cache curl bash

# Create non-root system group and user
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
USER appuser

WORKDIR /app
COPY --chown=appuser:appgroup target/service-*.jar /app/app.jar

# JVM ergonomics for container
ENV JAVA_OPTS="-XX:MaxRAMPercentage=75.0 \
               -XX:+ExitOnOutOfMemoryError \
               -XX:+HeapDumpOnOutOfMemoryError \
               -XX:HeapDumpPath=/tmp/oom.hprof \
               -XX:+UseG1GC"

EXPOSE 8080 9090

ENTRYPOINT ["sh", "-c", "exec java $JAVA_OPTS -jar /app/app.jar"]
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| JVM Flag / Concept                          | Purpose & Recommendation                                    |
+---------------------------------------------+-------------------------------------------------------------+
| -XX:MaxRAMPercentage=75.0                   | Dynamically bounds JVM heap to 75% of container memory      |
| -XX:+ExitOnOutOfMemoryError                 | Forces crash on OOM so Kubernetes can recreate healthy pod  |
| -XX:+HeapDumpOnOutOfMemoryError             | Generates hprof file on OOM for post-mortem heap analysis   |
| -XX:+UseG1GC                                | Default balanced collector for Spring Boot workloads        |
| -XX:+UseZGC -XX:+ZGenerational              | Ultra-low latency collector (sub-millisecond pauses)        |
| jvm_memory_used_bytes{area="heap"}          | PromQL metric for monitoring heap memory saturation         |
| jvm_gc_pause_seconds_sum                    | PromQL metric tracking total Stop-The-World pause duration  |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Oracle HotSpot Virtual Machine Garbage Collection Tuning Guide (Java 21)
- Generational ZGC: Ultra-Low Latency Garbage Collector Specification
- Eclipse Memory Analyzer (MAT) Documentation
- Spring Boot Production-Ready Metrics Documentation