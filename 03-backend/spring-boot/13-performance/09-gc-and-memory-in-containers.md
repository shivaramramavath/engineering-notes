# JVM Garbage Collection and Memory Management in Containers

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 13-performance |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & JVM Flag Reference](#4-syntax--jvm-flag-reference)
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
Deploying Spring Boot applications inside containerized environments (Docker, Kubernetes) introduces memory challenges that do not exist on bare-metal servers:
- The Linux kernel enforces hard memory limits via **cgroups**. If a container's total memory consumption exceeds its configured `resources.limits.memory`, the Linux kernel instantly terminates the process with **Exit Code 137 (OOMKilled)** via `SIGKILL`.
- Setting a fixed `-Xmx` (e.g., `-Xmx2g` in a 2GB container) guarantees an OOMKilled crash because the JVM requires significant memory outside the heap (Metaspace, thread stacks, CodeCache, direct byte buffers).
- Improperly tuned garbage collectors introduce multi-second Stop-The-World (STW) pauses, tripping Kubernetes liveness probes and triggering rolling container restarts.

### Historical Evolution
Prior to Java 8u191 and Java 10, the HotSpot JVM was "container-blind": it read CPU core counts and total RAM directly from `/proc/meminfo` (the host hardware), completely ignoring Docker/cgroup limits. A 2GB container running on a 64GB host would attempt to allocate a 16GB default heap, resulting in immediate termination by the Linux kernel.

Modern **Java 21 LTS** provides full native **cgroups v1 and cgroups v2 container ergonomics**, supporting dynamic percentage-based heap sizing (`-XX:MaxRAMPercentage`), automatic core detection, and ultra-low-latency collectors like **Generational ZGC**.

### Production Relevance
Mastering container memory management ensures that:
- Containers operate safely within Kubernetes resource limits without sudden OOMKilled terminations.
- JVM heap dynamically scales when Kubernetes pod vertical autoscaling (VPA) adjusts container limits.
- Tail latencies (P99) stay sub-10ms using G1GC or sub-1ms using Generational ZGC.

---

## 2. Core Concepts

### Total JVM Memory Equation
The total physical Resident Set Size (RSS) memory consumed by a containerized JVM process is calculated as:

$$\text{Total Memory (RSS)} = \text{Heap} + \text{Metaspace} + \text{Thread Stacks} + \text{CodeCache} + \text{Direct Buffers} + \text{JVM Native Overhead}$$

1. **Heap Memory (`-XX:MaxRAMPercentage`)**: Object instances and arrays. Typically sized to **70%–75%** of container memory.
2. **Metaspace (`-XX:MaxMetaspaceSize`)**: Class metadata, bytecode definitions, reflection structures. Allocated in native memory.
3. **Thread Stacks (`-Xss`)**: Memory allocated for thread execution frames (default 1MB per platform thread).
4. **CodeCache (`-XX:ReservedCodeCacheSize`)**: JIT-compiled native machine code (typically 128MB–256MB).
5. **Direct Memory (`-XX:MaxDirectMemorySize`)**: Off-heap buffers allocated via `ByteBuffer.allocateDirect()` (used by Netty, NIO, JDBC drivers).
6. **Linux OOM Killer (Exit Code 137)**: The kernel mechanism that terminates a container process when its total cgroup memory exceeds the container limit.

---

## 3. How It Works Internally

### Container Memory Allocation and cgroups v2 Architecture

```
KUBERNETES POD CONTAINER (Memory Limit: 4096 MB)
+------------------------------------------------------------------------------------+
| LINUX KERNEL CGROUPS V2 ENVELOPE (memory.max = 4GB)                                |
|                                                                                    |
|  +-------------------------------------------------------------------------------+ |
|  | JVM HEAP MEMORY (-XX:MaxRAMPercentage=75.0 -> 3072 MB)                        | |
|  |                                                                               | |
|  |  [ Eden Space ]   [ Survivor S0/S1 ]   [ Old Generation / Tenured ]           | |
|  +-------------------------------------------------------------------------------+ |
|                                                                                    |
|  +-------------------------------------------------------------------------------+ |
|  | NON-HEAP & NATIVE MEMORY (Reserved Headroom: 1024 MB = 25%)                   | |
|  |                                                                               | |
|  |  - Metaspace (-XX:MaxMetaspaceSize=256m)                                      | |
|  |  - CodeCache (-XX:ReservedCodeCacheSize=128m)                                 | |
|  |  - Thread Stacks (200 threads * 1MB = 200MB)                                  | |
|  |  - Direct Byte Buffers (Netty / NIO = 256MB)                                  | |
|  |  - C/C++ Native Overhead & GC metadata (~184MB)                               | |
|  +-------------------------------------------------------------------------------+ |
+------------------------------------------------------------------------------------+
  |
  +--> Total Memory Used = 3072MB (Heap) + 1024MB (Non-Heap) = 4096MB
       (Perfect container fit! Zero risk of Linux kernel OOMKilled 137!)
```

---

## 4. Syntax & JVM Flag Reference

### Production Container JVM Flags (Java 21 LTS)

```bash
# Sizing Heap via Percentages
-XX:MaxRAMPercentage=75.0
-XX:InitialRAMPercentage=75.0
-XX:MinRAMPercentage=75.0

# Non-Heap Memory Caps
-XX:MaxMetaspaceSize=256m
-XX:ReservedCodeCacheSize=128m
-XX:MaxDirectMemorySize=256m
-Xss1m

# Crash Diagnostics & Self-Healing
-XX:+ExitOnOutOfMemoryError
-XX:+HeapDumpOnOutOfMemoryError
-XX:HeapDumpPath=/var/log/dumps/oom.hprof

# Garbage Collector Selection (Choose ONE):
# 1. G1GC (Default - High throughput, balanced 50-200ms pauses)
-XX:+UseG1GC
-XX:MaxGCPauseMillis=100
-XX:InitiatingHeapOccupancyPercent=45

# 2. Generational ZGC (Java 21+ - Ultra-low latency, sub-1ms pauses)
-XX:+UseZGC
-XX:+ZGenerational

# GC Logging (Zero overhead in Java 21)
-Xlog:gc*,gc+phases=debug:file=/var/log/gc.log:time,uptime,pid:filecount=5,filesize=50m
```

---

## 5. Practical Examples

### Example 1: Verifying JVM Container Ergonomics via `jcmd`
To inspect how the JVM calculates its container memory limits inside a Kubernetes pod:

```bash
# Inside the running container:
jcmd 1 VM.info | grep -E "container|Memory"
```

```text
OS container metrics:
    provider: cgroupv2
    memory_limit: 2097152 kB (2048 MB)
    memory_soft_limit: unlimited
    memory_usage: 524288 kB (512 MB)
    cpu_quota: 200000 us
    cpu_period: 100000 us (2.0 CPUs)
Heap Configuration:
    MinHeapFreeRatio         = 40
    MaxHeapFreeRatio         = 70
    MaxHeapSize              = 1572864 kB (1536 MB = 75.0% of 2048MB)
    InitialHeapSize          = 1572864 kB (1536 MB)
```
*Result*: HotSpot accurately detected cgroupv2 and assigned exactly 75% of the 2GB limit (1536MB) to the heap.

### Example 2: Complete Multi-Stage Production Dockerfile
```dockerfile
# Stage 1: Runtime Base Image with Eclipse Temurin 21 JRE
FROM eclipse-temurin:21-jre-jammy AS runtime

# Install debugging utilities (curl, bash)
RUN apt-get update && apt-get install -y --no-install-recommends curl bash && rm -rf /var/lib/apt/lists/*

# Create secure non-root system group and user
RUN groupadd -r appgroup && useradd -r -g appgroup -u 1001 appuser

# Set working directory and create dump storage
WORKDIR /app
RUN mkdir -p /var/log/dumps && chown -R appuser:appgroup /var/log/dumps /app

# Copy executable jar
COPY --chown=appuser:appgroup target/billing-service-*.jar /app/app.jar

# JVM Ergonomics for Kubernetes Containers
ENV JAVA_TOOL_OPTIONS="\
  -XX:MaxRAMPercentage=75.0 \
  -XX:InitialRAMPercentage=75.0 \
  -XX:MaxMetaspaceSize=256m \
  -XX:+ExitOnOutOfMemoryError \
  -XX:+HeapDumpOnOutOfMemoryError \
  -XX:HeapDumpPath=/var/log/dumps/oom.hprof \
  -XX:+UseG1GC \
  -XX:MaxGCPauseMillis=100 \
  -Djava.security.egd=file:/dev/./urandom"

USER 1001:1001
EXPOSE 8080 9090

ENTRYPOINT ["java", "-jar", "/app/app.jar"]
```

### Example 3: Kubernetes Pod Specification with Accurate Limits (`deployment.yaml`)
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: billing-service
spec:
  replicas: 3
  template:
    metadata:
      labels:
        app: billing-service
    spec:
      containers:
        - name: billing-app
          image: billing-service:1.0.0
          resources:
            requests:
              cpu: "1000m"
              memory: "2048Mi"
            limits:
              cpu: "2000m"
              # When memory is set to 2048Mi, -XX:MaxRAMPercentage=75.0 allocates ~1536MB heap,
              # leaving 512MB for Metaspace, threads, and off-heap memory!
              memory: "2048Mi"
```

### Example 4: Anti-Pattern vs. Production Fix (Hardcoded Fixed `-Xmx`)
```dockerfile
# ANTI-PATTERN: Hardcoding -Xmx in Dockerfile
# If Kubernetes deployment changes memory limit from 4GB to 1GB,
# the JVM still tries to allocate 3GB heap -> INSTANT OOMKilled Crash!
ENV JAVA_OPTS="-Xmx3072m -Xms3072m" # BRITTLE AND DANGEROUS!

# PRODUCTION FIX: Percentage-based sizing
# Dynamically adapts to ANY container memory limit set in Kubernetes manifests!
ENV JAVA_TOOL_OPTIONS="-XX:MaxRAMPercentage=75.0 -XX:InitialRAMPercentage=75.0"
```

### Example 5: Diagnosing Exit Code 137 (OOMKilled) in Kubernetes
When a container dies with exit code 137:

```bash
# 1. Inspect pod termination reason
kubectl describe pod billing-service-78f99-x21z
```

```text
Last State:     Terminated
  Reason:       OOMKilled
  Exit Code:    137
  Started:      Mon, 24 Mar 2026 10:00:00 +0000
  Finished:     Mon, 24 Mar 2026 10:14:22 +0000
```
*Diagnosis*: Exit code 137 ($128 + 9 = \text{SIGKILL}$). The total container memory exceeded the cgroup limit.  
*Action*:
1. If no `oom.hprof` was generated: The memory was consumed **outside the heap** (Metaspace leak, direct memory leak in Netty, or high thread count).
2. If `oom.hprof` exists: The heap was exhausted (`java.lang.OutOfMemoryError: Java heap space`).

---

## 6. Best Practices & Production Guidelines

1. **Always Set `MaxRAMPercentage=75.0`**: Allocating 70% to 75% to heap guarantees that the remaining 25% to 30% is preserved for Metaspace, thread stacks, CodeCache, and native OS libraries, eliminating Linux kernel OOM kills.
2. **Set `InitialRAMPercentage` Equal to `MaxRAMPercentage`**: Pre-allocates the heap on startup. This prevents heap expansion pauses during live traffic surges and catches memory allocation issues immediately upon pod boot.
3. **Always Include `-XX:+ExitOnOutOfMemoryError`**: A JVM running with corrupted state after an OOM will fail transactions silently. This flag ensures the process terminates immediately so Kubernetes can spin up a healthy replacement pod.
4. **Cap Metaspace (`-XX:MaxMetaspaceSize=256m`)**: Without a limit, Metaspace will grow into native memory until the entire container limit is exceeded. Capping Metaspace triggers a clean Java `OutOfMemoryError: Metaspace` rather than an opaque kernel SIGKILL.
5. **Switch to Generational ZGC for Low-Latency SLAs**: In Java 21+, use `-XX:+UseZGC -XX:+ZGenerational` if your application requires P99 response times under 15ms. Generational ZGC reduces Stop-The-World pauses to less than 1 millisecond.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Setting `-Xmx` equal to container limit | Native memory pushes total process size over cgroup limit; Linux kernel kills container (`OOMKilled 137`). | Use `-XX:MaxRAMPercentage=75.0`. |
| Uncapped Metaspace | Dynamic proxy generation (CGLIB, reflection) leaks Metaspace until container crashes. | Set `-XX:MaxMetaspaceSize=256m`. |
| Omitting `-XX:+ExitOnOutOfMemoryError` | Pod remains running with dead worker threads, serving HTTP 500 errors indefinitely. | Add `-XX:+ExitOnOutOfMemoryError` to `JAVA_TOOL_OPTIONS`. |
| Disabling GC logs in containers | Zero forensic visibility into pause times, allocation rates, and memory pool transitions. | Enable unified logging: `-Xlog:gc*:file=/var/log/gc.log...`. |
| Setting `memory.requests` vastly lower than `limits` | Kubernetes node overcommit causes node kernel to evict pods when memory pressure occurs. | Set memory requests equal to memory limits for critical production services. |
| Using outdated base images | Older OpenJDK images (Java 8/11) lack full cgroupv2 support, reading incorrect host core and RAM counts. | Standardize on Eclipse Temurin 21 or Chainguard Java 21 images. |

---

## 8. Performance & Security Notes

### CPU Throttling and CFS Bandwidth
- Kubernetes CPU limits enforce Linux Completely Fair Scheduler (CFS) quota throttling.
- If a container exhausts its CPU quota (e.g. 50ms quota within a 100ms period), the Linux kernel **pauses the entire container** for the remaining 50ms.
- This creates massive artificial latency spikes. SRE best practice: avoid setting strict CPU limits or set limits sufficiently high (e.g. 4000m) while monitoring CPU throttling metrics (`container_cpu_cfs_throttled_periods_total`).

### Running as Non-Root User
- Containers must never execute as `root` (`UID 0`).
- Always declare `USER 1001:1001` or use distroless non-root images to prevent container escape vulnerabilities.

---

## 9. Comparison / When to Use What

| GC Collector | Target Pause Time | Throughput | Best Used For |
|---|---|---|---|
| **G1GC (Default)** | 50ms – 200ms | **Maximum** | General enterprise Spring Boot REST microservices |
| **Generational ZGC (Java 21)** | **$< 1\text{ ms}$** | Very High (95-98% of G1) | Real-time payment systems, low-latency microservices |
| **Serial GC** | High (Seconds) | Low | Tiny microservices with $< 512\text{MB}$ total container RAM |
| **Shenandoah GC** | 5ms – 20ms | High | Low-latency workloads on non-Oracle OpenJDK distributions |

---

## 10. Interview Questions & Answers

### Q1: Why does a Spring Boot container crash with Exit Code 137 (OOMKilled) even when `-Xmx` is set below the container memory limit?
<details>
<summary>Click to view answer</summary>

`-Xmx` bounds **only the Java Heap**. Total process memory includes:
$$\text{Total Memory} = \text{Heap} + \text{Metaspace} + \text{Thread Stacks (}1\text{MB per thread)} + \text{CodeCache} + \text{Direct Buffers (Netty)} + \text{JVM Native Overhead}$$
If an application has a 2GB container limit and sets `-Xmx1800m`, non-heap allocations (e.g. 250MB Metaspace + 200MB thread stacks) push total memory beyond 2048MB. The Linux kernel cgroup OOM killer immediately terminates the process with `SIGKILL` (exit code 137).
</details>

### Q2: How does `-XX:MaxRAMPercentage` solve the container memory problem?
<details>
<summary>Click to view answer</summary>

Instead of hardcoding a fixed megabyte value, `-XX:MaxRAMPercentage=75.0` instructs the HotSpot JVM to query the container's cgroup memory limit dynamically and allocate at most 75% of that limit to the Java heap. If Kubernetes adjusts the container limit from 2GB to 4GB, the JVM automatically scales its max heap from 1.5GB to 3GB without requiring changes to configuration or Dockerfiles.
</details>

### Q3: What is the difference between cgroups v1 and cgroups v2 regarding JVM container awareness?
<details>
<summary>Click to view answer</summary>

- **cgroups v1**: Legacy hierarchy where controllers (memory, cpu, blkio) operated in separate directory trees (`/sys/fs/cgroup/memory/memory.limit_in_bytes`).
- **cgroups v2**: Modern unified hierarchy (`/sys/fs/cgroup/memory.max`) with improved pressure stall information (PSI) and unified memory/swap management.
Java 21 natively detects both cgroups v1 and v2 seamlessly.
</details>

### Q4: How does Generational ZGC in Java 21 achieve sub-millisecond Stop-The-World pauses?
<details>
<summary>Click to view answer</summary>

Generational ZGC (JEP 439) performs almost all collection work concurrently with application threads:
1. It separates the heap into young and old generational regions.
2. It uses colored pointers (metadata bits in reference addresses) and load barriers to track object references during concurrent marking and relocation.
3. The only Stop-The-World phases are brief root-scanning phases that consistently complete in less than 1 millisecond regardless of whether the heap is 2GB or 2TB.
</details>

### Q5: Why is setting `InitialRAMPercentage` equal to `MaxRAMPercentage` recommended in production?
<details>
<summary>Click to view answer</summary>

When `InitialRAMPercentage` is lower than `MaxRAMPercentage`, the JVM starts with a small heap and dynamically expands it as traffic grows. Dynamic heap expansion causes allocation latency spikes and triggers frequent minor GC cycles. Pre-allocating the full heap on startup ensures uniform latency and fails fast if the host operating system cannot satisfy the memory allocation.
</details>

### Q6: What does `-XX:+ExitOnOutOfMemoryError` do and why is it preferred over staying alive?
<details>
<summary>Click to view answer</summary>

When an `OutOfMemoryError` occurs, Java threads are terminated mid-execution, leaving static variables, caches, and database connection pools in an inconsistent, half-dead state. The application will continue running but will fail subsequent user requests. `-XX:+ExitOnOutOfMemoryError` forces the JVM to terminate immediately on OOM, allowing Kubernetes to recreate a clean pod.
</details>

### Q7: What causes a containerized Spring Boot app to suffer from CPU throttling in Kubernetes?
<details>
<summary>Click to view answer</summary>

When `resources.limits.cpu` is configured in Kubernetes, the Linux kernel Completely Fair Scheduler (CFS) enforces a quota (e.g. 200ms of CPU time per 100ms period for `cpu: 2`). If multiple threads (or parallel GC workers) consume that quota in the first 30ms, the kernel pauses all container threads for the remaining 70ms, resulting in severe latency spikes.
</details>

### Q8: How can you limit Netty off-heap direct memory in a container?
<details>
<summary>Click to view answer</summary>

Use the JVM flag `-XX:MaxDirectMemorySize=256m`. This caps the total memory that can be allocated via native NIO/Netty buffers (`ByteBuffer.allocateDirect()`), preventing off-heap memory leaks from exceeding the container's cgroup memory limit.
</details>

---

## 11. Practice Exercises

### Exercise 1: Calculate Safe JVM Memory Budget for 2GB Container (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: A Kubernetes pod has a memory limit of `2048Mi`. Calculate the exact megabyte allocations for Heap (75%), Metaspace cap, and reserved headroom.

**Solution**:
1. $\text{Total Container Memory} = 2048 \text{ MB}$.
2. $\text{Heap Memory (75\%)} = 2048 \times 0.75 = 1536 \text{ MB}$.
3. $\text{Reserved Headroom (25\%)} = 2048 - 1536 = 512 \text{ MB}$.
4. Recommended breakdown of 512MB headroom:
   - `-XX:MaxMetaspaceSize=256m`
   - `-XX:ReservedCodeCacheSize=128m`
   - Thread stacks (100 threads $\times$ 1MB) $= 100 \text{ MB}$
   - Native JVM overhead $= 28 \text{ MB}$.
</details>

### Exercise 2: Configure Generational ZGC in Container Flags (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write the `JAVA_TOOL_OPTIONS` string for a Java 21 container to enable Generational ZGC with 75% heap, exit on OOM, and enable GC logging to `/tmp/gc.log`.

**Solution**:
```bash
JAVA_TOOL_OPTIONS="-XX:MaxRAMPercentage=75.0 -XX:InitialRAMPercentage=75.0 -XX:+UseZGC -XX:+ZGenerational -XX:+ExitOnOutOfMemoryError -Xlog:gc*:file=/tmp/gc.log:time,uptime,pid:filecount=3,filesize=20m"
```
</details>

### Exercise 3: Post-Mortem OOM Bash Script for Kubernetes Init/PreStop (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a shell script that checks if an `oom.hprof` file was generated after a crash, extracts the largest memory accumulator class name using `jhat` or `jcmd`, and writes an alert log before pod restart.

**Solution**:
```bash
#!/bin/bash
DUMP_DIR="/var/log/dumps"
LATEST_DUMP=$(ls -t $DUMP_DIR/*.hprof 2>/dev/null | head -n1)

if [ -n "$LATEST_DUMP" ]; then
    echo "CRITICAL: Detected OutOfMemoryError heap dump at $LATEST_DUMP"
    DUMP_SIZE=$(du -h "$LATEST_DUMP" | cut -f1)
    echo "Dump size: $DUMP_SIZE. Archiving to cloud storage..."

    # Compress and ship to centralized diagnostic bucket
    gzip -c "$LATEST_DUMP" > "${LATEST_DUMP}.gz"
    echo "Archive ready: ${LATEST_DUMP}.gz"
else
    echo "No heap dump found. Crash may have been caused by Linux cgroup OOM (Native/Metaspace)."
fi
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| JVM Flag / Concept                          | Purpose & Production Rule                                   |
+---------------------------------------------+-------------------------------------------------------------+
| -XX:MaxRAMPercentage=75.0                   | Allocates 75% of container cgroup memory to heap            |
| -XX:InitialRAMPercentage=75.0               | Pre-allocates full heap on startup to prevent resize pauses |
| -XX:MaxMetaspaceSize=256m                   | Caps native class metadata memory to prevent cgroup breach  |
| -XX:+ExitOnOutOfMemoryError                 | Forces crash on OOM so Kubernetes creates a fresh pod       |
| -XX:+HeapDumpOnOutOfMemoryError             | Generates hprof file on OOM for root-cause diagnosis        |
| -XX:+UseZGC -XX:+ZGenerational              | Java 21 sub-millisecond ultra-low-latency collector         |
| -XX:MaxDirectMemorySize=256m                | Limits Netty/NIO off-heap buffer allocations                |
| Exit Code 137                               | OOMKilled by Linux kernel (Process exceeded cgroup limit)   |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- OpenJDK JEP 439: Generational ZGC
- Kubernetes Documentation: Managing Resources for Containers
- Oracle HotSpot JVM Container Ergonomics Guide
- Cloud Native Java: Memory Sizing in Docker & Kubernetes