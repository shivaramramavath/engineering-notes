# Thread and Heap Dump Analysis for Spring Boot

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
- [4. Syntax & Command Reference](#4-syntax--command-reference)
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
When a production Spring Boot service experiences severe degradation, metrics and logs alone are often insufficient:
- The CPU spikes to 100%, but no error logs appear. Which exact line of code is spinning in an infinite loop?
- HTTP request threads are exhausted (504 Gateway Timeouts), but the database is healthy. Are threads deadlocked or blocked waiting on an unreleased lock?
- The JVM terminates with `java.lang.OutOfMemoryError: Java heap space`. Which classes or collections are consuming gigabytes of heap memory?

### Historical Evolution
Diagnostic analysis originally required intrusive command-line utilities (`jstack`, `jmap`) executed directly on host servers. In modern containerized Kubernetes environments where developers lack direct SSH access to production containers, Spring Boot Actuator exposes secure diagnostic endpoints (`/actuator/threaddump`, `/actuator/heapdump`). Simultaneously, modern diagnostic CLI commands like `jcmd` provide safe, low-overhead introspection inside running containers.

### Production Relevance
Thread and Heap dump analysis is the definitive forensic technique for solving:
- **Deadlocks**: Mutually blocking threads waiting on synchronized monitors or `ReentrantLock`s.
- **Thread Starvation & Pinning**: Carrier thread exhaustion caused by pinned Virtual Threads in Java 21.
- **Memory Leaks**: Identifying the "Dominator Tree" and "Leak Suspects" in Eclipse Memory Analyzer (MAT).

---

## 2. Core Concepts

### Thread Dump Essentials
1. **Thread Dump**: A complete snapshot of all active JVM threads at a single instant in time, including thread names, IDs, states (`RUNNABLE`, `BLOCKED`, `WAITING`, `TIMED_WAITING`), and method call stack frames.
2. **Thread Contention / Lock Ownership**:
   - `waiting to lock <0x00000007...>`: Thread is blocked waiting for a monitor held by another thread.
   - `locked <0x00000007...>`: Thread currently owns the monitor lock.
3. **Deadlock**: Two or more threads each hold a lock that the other needs, creating an unresolvable cyclic dependency.

### Heap Dump Essentials
1. **Heap Dump (`.hprof`)**: A binary snapshot of all objects residing in the JVM heap at a specific instant, including object references, field values, array sizes, and memory retention trees.
2. **Shallow Size**: The memory consumed by the object itself (its header and primitive fields), excluding any referenced objects.
3. **Retained Size**: The total memory that would be freed if this object were garbage collected (the object itself plus the transitive closure of objects reachable *only* through it).
4. **Dominator Tree**: An object graph hierarchy in Eclipse MAT where object $A$ dominates object $B$ if every path from the GC root to $B$ must pass through $A$. Dominator trees immediately pinpoint leak culprits.

---

## 3. How It Works Internally

### Dump Generation and Memory Mechanics

```
DIAGNOSTIC TRIGGER
(jcmd <pid> GC.heap_dump / HotSpotDiagnosticMXBean / Actuator)
  |
  v
+------------------------------------------------------------------------------------+
| 1. HOTSPOT JVM SAFELINK & STW PAUSE                                               |
|    JVM brings all application threads to a Safepoint (Stop-The-World pause)       |
|    No objects can be allocated or modified during the snapshot capture             |
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 2. OBJECT GRAPH SERIALIZATION                                                      |
|    Traverses GC Roots (Thread Stacks, JNI Globals, Static Classes)                 |
|    Writes binary HPROF records directly to filesystem:                             |
|    - Header: "JAVA PROFILE 1.0.2"                                                  |
|    - Class Dumps, Instance Dumps, Object Arrays, Primitive Arrays                  |
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 3. SAFELINK RELEASE & POST-ANALYSIS                                                |
|    Application threads resume execution                                           |
|    HPROF file opened in Eclipse MAT / VisualVM:                                    |
|    - Parses Dominator Tree                                                         |
|    - Identifies Leak Suspects (Objects holding >20% of heap)                       |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Command Reference

### Generating Thread Dumps
```bash
# Method 1: Using Actuator HTTP (Download JSON thread dump)
curl -X GET http://localhost:9090/actuator/threaddump > threaddump.json

# Method 2: Inside a container using jcmd (Preferred HotSpot utility)
jcmd <PID> Thread.print > /tmp/threaddump.txt

# Method 3: Using jstack
jstack -l <PID> > /tmp/threaddump.txt

# Method 4: Virtual Thread dump in Java 21
jcmd <PID> Thread.dump_to_file -format=json /tmp/virtual_threads.json
```

### Generating Heap Dumps
```bash
# Method 1: Using Actuator HTTP (Downloads binary gzip hprof directly)
curl -X GET http://localhost:9090/actuator/heapdump --output heapdump.hprof

# Method 2: Inside container using jcmd
jcmd <PID> GC.heap_dump /tmp/heapdump.hprof

# Method 3: Automated dump on OutOfMemoryError (Recommended JVM Flag)
-XX:+HeapDumpOnOutOfMemoryError -XX:HeapDumpPath=/var/log/dumps/oom.hprof
```

---

## 5. Practical Examples

### Example 1: Diagnosing a Deadlock from a Thread Dump
Below is an excerpt of a real Java thread dump exhibiting a classic deadlock between two threads:

```text
Found one Java-level deadlock:
=============================
"OrderProcessor-1":
  waiting to lock monitor 0x00007f9c (object 0x0000000712ab88, a java.lang.Object),
  which is held by "InventorySync-2"

"InventorySync-2":
  waiting to lock monitor 0x00007fa0 (object 0x0000000712ab90, a java.lang.Object),
  which is held by "OrderProcessor-1"

Java stack information for the threads listed above:
===================================================
"OrderProcessor-1":
  at com.example.service.OrderService.transferItems(OrderService.java:45)
  - waiting to lock <0x0000000712ab88> (a java.lang.Object)
  - locked <0x0000000712ab90> (a java.lang.Object)
  at java.util.concurrent.ThreadPoolExecutor.runWorker(ThreadPoolExecutor.java:1144)

"InventorySync-2":
  at com.example.service.InventoryService.syncWithOrder(InventoryService.java:82)
  - waiting to lock <0x0000000712ab90> (a java.lang.Object)
  - locked <0x0000000712ab88> (a java.lang.Object)
  at java.util.concurrent.ThreadPoolExecutor.runWorker(ThreadPoolExecutor.java:1144)
```

**Root Cause**: Inconsistent lock acquisition order between `OrderService` (Lock A then Lock B) and `InventoryService` (Lock B then Lock A).  
**Remedy**: Enforce global lock ordering or replace nested `synchronized` blocks with `tryLock(timeout)` using `java.util.concurrent.locks.ReentrantLock`.

### Example 2: Analyzing CPU Thrashing (Matching Thread Dump to `top -H -p`)
When an application instance experiences 100% CPU utilization:

```bash
# Step 1: Find the OS thread ID consuming CPU inside the container
top -H -p <JVM_PID>
# Output: PID 14205 is consuming 98% CPU!

# Step 2: Convert OS PID (14205) to hexadecimal
printf "%x\n" 14205
# Output: 377d

# Step 3: Search thread dump for nid=0x377d
grep -A 15 "nid=0x377d" threaddump.txt
```

```text
"WorkerThread-4" #32 prio=5 os_prio=0 cpu=142981.12ms nid=0x377d runnable [0x00007f8]
   java.lang.Thread.State: RUNNABLE
        at com.example.service.ReportingService.calculateMetrics(ReportingService.java:112)
        at com.example.service.ReportingService.generateReport(ReportingService.java:45)
```
**Conclusion**: Line 112 in `ReportingService.java` contains an infinite `while` loop or unconstrained regex evaluation consuming 100% of the CPU core.

### Example 3: Simulating a Memory Leak and Analyzing with Eclipse MAT
```java
package com.example.observability.diagnostics;

import org.springframework.stereotype.Service;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

@Service
public class LeakyCacheService {

    // ANTI-PATTERN: Unbounded static cache without eviction policy!
    private static final Map<String, List<byte[]>> UNBOUNDED_LEAK = new ConcurrentHashMap<>();

    public void processPayload(String transactionId, byte[] payload) {
        // Appends 100KB chunks continuously without ever clearing!
        UNBOUNDED_LEAK.computeIfAbsent(transactionId, k -> new ArrayList<>())
            .add(payload);
    }
}
```

**Eclipse Memory Analyzer (MAT) Analysis Workflow**:
1. Open `heapdump.hprof` in Eclipse MAT.
2. Run the **Leak Suspects Report**.
3. MAT Report Output:
   ```text
   Problem Suspect 1
   The class "com.example.observability.diagnostics.LeakyCacheService", loaded by 
   "jdk.internal.loader.ClassLoaders$AppClassLoader", occupies 1,428,912,800 (88.4%) bytes.
   
   The memory is accumulated in one instance of "java.util.concurrent.ConcurrentHashMap" 
   referenced by "UNBOUNDED_LEAK".
   ```
4. Right-click the suspect $\rightarrow$ **Immediate Dominators** $\rightarrow$ identify the exact field `UNBOUNDED_LEAK`.

### Example 4: Pinning Virtual Threads in Java 21
```java
// ANTI-PATTERN: Synchronized block inside a Virtual Thread executing blocking I/O
// Causes: Virtual thread cannot unmount from its carrier thread (PINNING)!
// If 16 carrier threads are pinned, all other 50,000 virtual threads FREEZE!
@Service
public class BadVirtualThreadWorker {

    public synchronized String callDownstreamService() {
        // SYNCHRONIZED BLOCK + BLOCKING I/O = CARRIER PINNING
        return restClient.get().uri("/data").retrieve().body(String.class);
    }
}

// PRODUCTION FIX: Replace synchronized with ReentrantLock
@Service
public class ResilientVirtualThreadWorker {

    private final ReentrantLock lock = new ReentrantLock();

    public String callDownstreamService() {
        lock.lock(); // ReentrantLock allows Virtual Thread unmounting cleanly!
        try {
            return restClient.get().uri("/data").retrieve().body(String.class);
        } finally {
            lock.unlock();
        }
    }
}
```

### Example 5: Capturing Consecutive Thread Dumps for Trend Analysis
```bash
#!/bin/bash
# Captures 3 thread dumps 5 seconds apart to detect stuck vs. progressing threads
PID=$(pgrep -f "spring-boot")
echo "Capturing 3 thread dumps for PID: $PID"

for i in 1 2 3; do
    echo "Capturing dump $i..."
    jcmd $PID Thread.print > "/tmp/threaddump_${i}_$(date +%s).txt"
    sleep 5
done

echo "Dumps captured. Comparing thread states..."
```

---

## 6. Best Practices & Production Guidelines

1. **Always Capture Multiple Thread Dumps**: A single thread dump is a freeze-frame. A thread in `RUNNABLE` might have been active for 1 millisecond or stuck for 2 hours. Capture at least 3 thread dumps 5–10 seconds apart to observe whether thread state and stack frames are advancing or permanently stuck.
2. **Never Run `jmap -dump` on High-Traffic Nodes**: `jmap` is legacy and can freeze the JVM for tens of seconds. Always use `jcmd <PID> GC.heap_dump` or trigger dumps via Actuator.
3. **Store Heap Dumps on Persistent Volumes**: Heap dumps equal the size of your allocated heap (e.g. 8GB). Ensure `HeapDumpPath` points to a persistent volume with sufficient free disk space to avoid filling root filesystems.
4. **Use ReentrantLock Instead of Synchronized in Java 21**: Virtual Threads in Java 21 become pinned to carrier threads when executing inside `synchronized` blocks. Standardize on `java.util.concurrent.locks.ReentrantLock`.
5. **Secure the `/actuator/heapdump` Endpoint**: A heap dump contains every byte in application memory: plaintext passwords, API keys, decrypted TLS tokens, customer data. Strictly guard access with admin roles and network security policies.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Triggering heap dump on root disk | Fills container disk to 100%, causing the Linux kernel to crash the node. | Mount a dedicated persistent scratch volume for dumps (`/var/dumps`). |
| Analyzing only Shallow Size in MAT | Shallow size only shows the memory of the container map itself (a few KB), hiding the gigabytes of data held by it. | Always inspect **Retained Size** and the **Dominator Tree**. |
| Assuming `RUNNABLE` means healthy | A thread stuck in an infinite `while(true)` loop or computing a catastrophic regex is in `RUNNABLE` state while thrashing CPU. | Correlate thread IDs with `top -H` CPU usage. |
| Forgetting `-XX:+ExitOnOutOfMemoryError` with HeapDump | JVM captures the heap dump but remains running in a corrupted state, continuing to accept and fail requests. | Pair `-XX:+HeapDumpOnOutOfMemoryError` with `-XX:+ExitOnOutOfMemoryError`. |
| Single thread dump for diagnosing hangs | Cannot tell if a thread is legitimately working or permanently stuck on an un-timeouted network socket. | Take 3 consecutive thread dumps spaced 5 seconds apart. |
| Missing source line numbers in stack traces | Code compiled without debug symbols (`-g:none`) hides line numbers, making stack traces difficult to trace. | Ensure Maven/Gradle builds compile with default debug info (`-g`). |

---

## 8. Performance & Security Notes

### Overhead of Heap Dumps
- Generating a heap dump triggers a Stop-The-World (STW) pause that lasts from 1 to 15 seconds depending on heap size and disk write throughput.
- In production, take the target pod out of Kubernetes service routing (set `ReadinessState.REFUSING_TRAFFIC`) before capturing a manual heap dump.

### Data Privacy & Compliance (GDPR / PCI)
- Heap dumps contain plaintext customer PII, session tokens, and cryptographic keys.
- Store `.hprof` files in encrypted buckets with automated lifecycle deletion policies (e.g., delete after 7 days).

---

## 9. Comparison / When to Use What

| Diagnostic Artifact | Primary Use Case | Collection Overhead | Primary Analysis Tool |
|---|---|---|---|
| **Thread Dump** | High CPU, Deadlocks, Thread pool starvation, Latency spikes | Very Low ($< 10\text{ms}$ STW) | FastThread, VisualVM, `grep` |
| **Heap Dump** | `OutOfMemoryError`, Slow memory leaks, High Old Gen utilization | High (1-10s STW, disk I/O) | Eclipse MAT, VisualVM, YourKit |
| **JDK Flight Recorder (JFR)**| Continuous low-overhead profiling (allocations, CPU, locks) | Ultra Low ($< 1\%$ continuous) | JDK Mission Control (JMC) |

---

## 10. Interview Questions & Answers

### Q1: What is the difference between Shallow Size and Retained Size in Eclipse MAT?
<details>
<summary>Click to view answer</summary>

- **Shallow Size**: The amount of memory allocated to store the object itself, including its object header (12-16 bytes) and primitive fields, but **excluding** any objects it references.
- **Retained Size**: The total memory that would be reclaimed by the garbage collector if this object were destroyed. It equals the object's shallow size plus the shallow sizes of all objects that are reachable *exclusively* from this object.
</details>

### Q2: How do you identify the exact Java thread causing 100% CPU on a Linux server?
<details>
<summary>Click to view answer</summary>

1. Run `top -H -p <JVM_PID>` to list all threads in the JVM process sorted by CPU usage. Note the OS thread ID (e.g. `24510`).
2. Convert the thread ID to hexadecimal: `printf "%x\n" 24510` $\rightarrow$ `5fc6`.
3. Take a JVM thread dump: `jcmd <JVM_PID> Thread.print > dump.txt`.
4. Search the dump for the native thread ID: `grep "nid=0x5fc6" dump.txt`. The matching stack trace reveals the exact method and line of code executing on that core.
</details>

### Q3: What is a Dominator Tree in Memory Analyzer (MAT)?
<details>
<summary>Click to view answer</summary>

In graph theory, node $A$ dominates node $B$ if every path from the root node to $B$ must pass through $A$. In a heap dump dominator tree, if an object $A$ dominates object $B$, garbage collecting $A$ will automatically collect $B$. The dominator tree provides a top-down hierarchy of the largest memory accumulators, instantly revealing which singleton, cache, or collection dominates the heap.
</details>

### Q4: What is Virtual Thread pinning in Java 21, and how does it show up in diagnostics?
<details>
<summary>Click to view answer</summary>

Virtual thread pinning occurs when a Virtual Thread executes inside a `synchronized` method/block or native method call and then attempts a blocking operation. Instead of unmounting from its underlying carrier thread, the virtual thread remains pinned to the OS carrier thread. If many virtual threads pin carrier threads, the carrier pool becomes exhausted, stalling all other virtual threads. It can be detected using the JVM flag `-Djdk.tracePinnedThreads=full` or JFR events.
</details>

### Q5: Why is it recommended to capture at least three thread dumps instead of one?
<details>
<summary>Click to view answer</summary>

A single thread dump shows an instantaneous snapshot. A thread in `RUNNABLE` or `WAITING` could be operating normally or permanently hung. By comparing three consecutive dumps taken 5 seconds apart:
- If a thread's stack frame has changed, the thread is actively progressing.
- If a thread remains stuck at the exact same line of code across all 3 dumps, it is blocked or hung.
</details>

### Q6: What does `Thread.State: BLOCKED` mean versus `Thread.State: WAITING`?
<details>
<summary>Click to view answer</summary>

- `BLOCKED`: The thread is trying to enter a `synchronized` block or method, but another thread currently holds the monitor lock.
- `WAITING`: The thread voluntarily yielded execution and is waiting indefinitely for another thread to notify it (e.g., `Object.wait()`, `LockSupport.park()`, or `CompletableFuture.get()`).
</details>

### Q7: Can a thread dump be taken without access to the host command line?
<details>
<summary>Click to view answer</summary>

Yes. Spring Boot Actuator exposes `/actuator/threaddump` over HTTP. When invoked, it queries `ThreadMXBean.dumpAllThreads()` and returns a structured JSON payload containing all active threads, lock owners, and stack traces.
</details>

### Q8: What is a "GC Root" in heap dump analysis?
<details>
<summary>Click to view answer</summary>

A Garbage Collection Root is an object that is fundamentally accessible from outside the heap and acts as the starting anchor for garbage collection reachability analysis. Examples include:
- Local variables and parameters on active thread call stacks.
- Static fields of loaded classes.
- JNI (Java Native Interface) global references.
Objects reachable from a GC root cannot be garbage collected.
</details>

---

## 11. Practice Exercises

### Exercise 1: Generate Thread Dump via Actuator and Filter Blocked Threads (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a curl and jq command to retrieve a thread dump from `/actuator/threaddump` and list all threads in `BLOCKED` state.

**Solution**:
```bash
curl -s http://localhost:9090/actuator/threaddump | \
  jq '.threads[] | select(.threadState=="BLOCKED") | {threadName, lockInfo, lockedMonitors}'
```
</details>

### Exercise 2: Identify Deadlock from Stack Trace Excerpt (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Given the following two threads, identify the deadlock condition and write the corrected lock acquisition order.

```text
Thread-1: locked <0xAA>, waiting for <0xBB>
Thread-2: locked <0xBB>, waiting for <0xAA>
```

**Solution**:
The deadlock occurs due to **lock ordering inconsistency** (Thread 1 acquires AA then BB, while Thread 2 acquires BB then AA).  
**Fix**: Enforce strict canonical ordering. Both threads must acquire `<0xAA>` first, and then `<0xBB>`.
```java
public void transfer(Account from, Account to) {
    Account firstLock = from.getId() < to.getId() ? from : to;
    Account secondLock = from.getId() < to.getId() ? to : from;

    synchronized(firstLock) {
        synchronized(secondLock) {
            // Safe: Global lock hierarchy prevents cyclic wait conditions!
        }
    }
}
```
</details>

### Exercise 3: Automated Heap Dump Ingestion Script for Kubernetes (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Author a Kubernetes pod lifecycle `preStop` hook or bash script that detects an OOM exit, compresses the `.hprof` file, and uploads it to an S3 bucket before the container is wiped.

**Solution**:
```bash
#!/bin/bash
# Upload OOM Heap Dump to S3
DUMP_FILE="/var/log/dumps/oom.hprof"

if [ -f "$DUMP_FILE" ]; then
    echo "Found OOM heap dump. Compressing..."
    gzip "$DUMP_FILE"
    GZ_FILE="${DUMP_FILE}.gz"
    TIMESTAMP=$(date +%s)
    S3_TARGET="s3://prod-heap-dumps/${HOSTNAME}_${TIMESTAMP}.hprof.gz"

    echo "Uploading to $S3_TARGET..."
    aws s3 cp "$GZ_FILE" "$S3_TARGET"
    echo "Upload complete."
fi
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Command / Diagnostic Tool                   | Description & Usage                                         |
+---------------------------------------------+-------------------------------------------------------------+
| jcmd <PID> Thread.print                     | Generates full JVM thread dump                              |
| jcmd <PID> GC.heap_dump <file.hprof>        | Captures binary heap dump with minimal STW impact           |
| top -H -p <PID>                             | Identifies high-CPU OS threads; convert TID to hex for nid  |
| -XX:+HeapDumpOnOutOfMemoryError             | Automatically generates hprof snapshot on OOM               |
| Retained Size (MAT)                         | Total memory freed if object is garbage collected           |
| Dominator Tree (MAT)                        | Identifies root accumulators dominating heap usage          |
| /actuator/threaddump                        | HTTP endpoint returning structured JSON thread dump         |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Eclipse Memory Analyzer (MAT) Official Documentation & Tutorials
- Oracle HotSpot Diagnostic Commands Reference (jcmd)
- FastThread.io: JVM Thread Dump Analysis Patterns
- Java 21 Project Loom: Troubleshooting Virtual Thread Pinning