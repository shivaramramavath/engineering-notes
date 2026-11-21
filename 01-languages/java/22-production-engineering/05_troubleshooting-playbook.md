# Production Troubleshooting Playbook for Java

> Step-by-step incident response runbooks for enterprise Java outages: High CPU Saturation, Memory Leaks & OOM, Thread Pool Starvation & Deadlocks, and Connection Pool Exhaustion.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Production Incident Management |
| **Prerequisites** | [04_jvm-diagnostics](04_jvm-diagnostics.md), [20_gc-tuning](../20-performance/03_gc-tuning.md), [16_connection-pooling](../16-jdbc-and-databases/05_connection-pooling.md) |
| **Time to Read** | 40 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

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
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is a Troubleshooting Playbook?
A troubleshooting playbook is an algorithmic, step-by-step operational runbook designed to guide on-call engineers through rapid incident containment, root-cause diagnosis, and system recovery during high-severity production outages.

During production outages:
- Ad-hoc debugging and random server restarts often destroy critical diagnostic evidence (heap dumps, thread dumps, GC logs) without fixing underlying bugs.
- Having predetermined triage playbooks cuts Mean Time to Resolution (MTTR) by 70%, stabilizes business operations, and ensures accurate post-mortem analysis.

### The Four Major Enterprise Outage Modes
Over 90% of production Java incidents fall into four distinct categories:
1. **High CPU Saturation (100% CPU)**: Infinite loops, lock-free spinloops, catastrophic regular expression backtracking (ReDoS), or continuous GC thrashing.
2. **Memory Leak and OutOfMemoryError**: JVM heap exhaustion, unclosed caches, Metaspace growth, or off-heap Direct ByteBuffer leaks.
3. **Thread Starvation & Deadlocks**: Worker thread pools depleted waiting on slow downstream APIs, or classic multi-lock inversions.
4. **Database Connection Pool Exhaustion**: Leaked connections, unclosed ResultSets, long-running unindexed transactions, or insufficient pool sizing.

---

## 2. Core Concepts

### Golden Incident Triage Workflow
```
[ Step 1: Mitigate & Isolate ]
  Take pod out of load balancer / Route traffic to healthy cluster / Auto-scale
         |
         v
[ Step 2: Capture Forensic Artifacts ]
  Collect: Thread Dumps (x3), GC Metrics, Heap Dump (if OOM), NMT summary
         |
         v
[ Step 3: Recover Service ]
  Restart failed pod / Failover to secondary database / Rollback bad release
         |
         v
[ Step 4: Root Cause Analysis (Offline) ]
  Analyze artifacts in Eclipse MAT, JMC, or Flame Graphs without production pressure
```

---

## 3. How It Works Internally

### Outage Mode 1: High CPU Triage Decision Tree
```
                    [ 100% CPU Alert Triggered ]
                                 |
                 Run: jstat -gcutil <PID> 1000
                                 |
             +-------------------+-------------------+
             |                                       |
    [ Old Gen O >= 98% ]                    [ Old Gen O < 80% ]
             |                                       |
  Cause: GC Thrashing (Full GC)            Cause: Application CPU Spike
  Action: Capture Heap Dump                Action: top -H -p <PID> -> Hex nid
  Root: Heap Sizing / Memory Leak          Root: Busy Loop / ReDoS / JIT Loop
```

---

## 4. Syntax & Usage

### 1. The 60-Second Forensic Capture Script
Execute immediately upon logging into an impaired node before restarting:
```bash
#!/usr/bin/env bash
PID=$(pgrep -f "java.*app.jar")
OUT="/tmp/triage_$(date +%s)"
mkdir -p "$OUT"

# 1. Thread Dumps (3 samples, 5s apart)
for i in 1 2 3; do
    jcmd "$PID" Thread.print > "$OUT/threads_$i.txt"
    sleep 5
done

# 2. GC Stats
jstat -gcutil "$PID" 1000 5 > "$OUT/jstat.txt"

# 3. Top Threads by CPU
top -H -b -n 1 -p "$PID" > "$OUT/top_threads.txt"

# 4. Memory Histogram (Top 50 Classes)
jcmd "$PID" GC.class_histogram | head -n 50 > "$OUT/histogram.txt"

echo "Forensic artifacts captured in $OUT"
```

---

## 5. Practical Examples

### Playbook 1: High CPU Saturation Runbook
**Symptoms:** Alerts fire for CPU $> 90\%$; response latencies spike; health checks time out.

**Step-by-Step Procedure:**
1. **Determine whether CPU is Application Code or Garbage Collection:**
   ```bash
   jstat -gcutil <PID> 1000 3
   ```
   - If `O` is $> 95\%$ and `FGCT` is rapidly climbing, the CPU is consumed entirely by GC threads attempting to reclaim memory. Jump to **Playbook 2 (Memory Leak)**.
   - If `O` is normal ($< 80\%$) and `FGC` is not incrementing, application threads are burning CPU in application logic.
2. **Pinpoint Culprit Native Thread ID:**
   ```bash
   top -H -p <PID>
   ```
   Note the `PID` column for the thread using the highest CPU (e.g. `5542`).
3. **Convert Thread ID to Hex:**
   ```bash
   printf "0x%x
" 5542  # Output: 0x15a6
   ```
4. **Locate Code in Thread Dump:**
   ```bash
   jcmd <PID> Thread.print | grep -A 30 "nid=0x15a6"
   ```
5. **Mitigation:**
   - If thread is stuck in regex backtracking: Block the client IP or payload via WAF.
   - If stuck in a busy loop: Restart the pod and deploy an emergency fix patch.

---

### Playbook 2: Memory Leak & OutOfMemoryError Runbook
**Symptoms:** `java.lang.OutOfMemoryError: Java heap space` or container killed with exit code `137` (OOMKilled).

**Step-by-Step Procedure:**
1. **Verify Heap vs. Off-Heap:**
   - If JVM threw `OutOfMemoryError: Java heap space`: The Java heap is exhausted.
   - If JVM threw `OutOfMemoryError: Metaspace`: Dynamic class loading leak (e.g. CGLIB/proxy churn).
   - If JVM threw `OutOfMemoryError: Direct buffer memory`: Netty or NIO direct byte buffer leak.
   - If container terminated without Java OOM log (Exit Code 137): Linux cgroup memory limit exceeded by native memory.
2. **Extract Dominator Tree from Heap Dump:**
   - Open `.hprof` file in **Eclipse Memory Analyzer Tool (MAT)**.
   - Run the **Leak Suspects Report**.
   - Inspect the **Dominator Tree**: Identify the single object holding the largest retained heap percentage.
3. **Common Root Causes:**
   - Static unbounded collections: `static Map<Key, Value> cache = new HashMap<>()` without eviction.
   - Unbounded ThreadLocal storage: ThreadLocal objects not removed after worker requests complete.
   - Unclosed `ResultSet` or `Stream`: Loading 5,000,000 rows into memory instead of streaming with cursor batching.
4. **Mitigation:**
   - Temporarily increase container memory limit (`resources.limits.memory`) or scale out replicas.
   - Roll back to the previous stable release.

---

### Playbook 3: Thread Deadlock Runbook
**Symptoms:** Service throughput drops to zero; CPU drops to near 0%; incoming HTTP requests timeout.

**Step-by-Step Procedure:**
1. **Trigger Automated Deadlock Detection:**
   ```bash
   jcmd <PID> Thread.print
   ```
2. **Scroll to the bottom of the dump output:**
   HotSpot automatically runs deadlock detection algorithms during `Thread.print`. If a deadlock exists, HotSpot prints an explicit block:
   ```
   Found one Java-level deadlock:
   =============================
   "Thread-1":
     waiting to lock monitor 0x00007f (object 0x1234, a com.bank.Account),
     which is held by "Thread-2"
   "Thread-2":
     waiting to lock monitor 0x00007e (object 0x5678, a com.bank.Account),
     which is held by "Thread-1"
   ```
3. **Mitigation:**
   - Deadlocks cannot self-heal. The blocked threads will never make progress.
   - **Immediately restart the container** to restore service availability.
   - **Fix in Code**: Enforce a strict global lock ordering (e.g. always lock accounts in order of ascending account ID).

---

### Playbook 4: Database Connection Pool Exhaustion Runbook
**Symptoms:** Application logs report `HikariPool-1 - Connection is not available, request timed out after 30000ms`.

**Step-by-Step Procedure:**
1. **Inspect Active Connection Pool Metrics:**
   - Check HikariCP JMX/Micrometer metrics: `hikaricp.connections.active`, `hikaricp.connections.idle`, `hikaricp.connections.pending`.
   - If `active == maxPoolSize` and `pending > 50`, all connections are saturated.
2. **Inspect Database Engine Active Queries:**
   ```sql
   -- PostgreSQL: Find queries running longer than 10 seconds
   SELECT pid, now() - query_start AS duration, query, state 
   FROM pg_stat_activity 
   WHERE state != 'idle' AND (now() - query_start) > interval '10 seconds';
   ```
3. **Identify Leak vs. Long Transactions:**
   - If queries in database are `idle in transaction`: The application opened a transaction, acquired a connection, and called an external slow HTTP API before committing.
   - If queries are executing unindexed full-table scans: Add the missing database index immediately.
   - If database shows 0 active queries but pool is full: The application is leaking connections (failing to call `connection.close()` in a `finally` block).
4. **Mitigation:**
   - Kill slow queries on the database: `SELECT pg_terminate_backend(pid);`.
   - Enable HikariCP leak detection in configuration: `leak-detection-threshold=15000` (prints stack trace of any connection held $> 15\text{s}$).

---

## 6. Best Practices

1. **Capture Before Restarting**: Always execute the 60-second forensic capture script before recycling a malfunctioning container.
2. **Automate Heap Dumps on OOM**: Ensure `-XX:+HeapDumpOnOutOfMemoryError` is enabled with a safe destination volume.
3. **Configure Connection Leak Detection**: Set `leak-detection-threshold` in HikariCP to detect unclosed connections in staging and production.
4. **Enforce Global Lock Hierarchy**: Prevent deadlocks by always acquiring multiple resources/locks in a deterministic order.
5. **Never Execute Remote Calls Inside DB Transactions**: Calling third-party REST APIs while holding a database connection holds connection slots hostage for seconds.
6. **Set Aggressive Connection Timeouts**: Configure `connect-timeout` and `read-timeout` on every external HTTP and database client.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Restarting pods immediately on alert** | Erases volatile thread stacks and heap dumps; bug recurs indefinitely. | Capture thread dumps and GC stats first. |
| **Increasing connection pool size blindly** | Satures database CPU and memory, causing complete database collapse. | Fix slow queries or connection leaks; keep pool moderate ($\le 30$). |
| **Ignoring hex thread ID conversion** | Searching decimal PID in thread dump yields zero matches. | Convert Linux LWP thread ID to hex: `printf "0x%x
" <TID>`. |
| **Calling HTTP services inside `@Transactional`** | Holds DB connection open while waiting on slow network latency. | Move external REST calls outside transaction boundary. |
| **Unbounded in-memory collections** | Collections grow indefinitely until OutOfMemoryError crashes JVM. | Use bounded caches with LRU eviction (Caffeine). |
| **Missing read timeouts on HTTP clients** | Default infinite timeout locks worker threads permanently if server hangs. | Set explicit `SocketTimeout` (e.g. 3–5 seconds). |

---

## 8. Performance & Security Notes

### The Cost of Premature Restarts
While restarting a stalled pod restores immediate uptime, recurring bugs (such as a memory leak that triggers every 6 hours) will continuously destabilize users. Taking one pod out of load balancer rotation, capturing complete forensics, and letting it fail cleanly provides the diagnostic artifacts necessary to write a permanent code fix.

---

## 9. Comparison / When to Use What

| Incident Symptom | Primary Diagnostic Tool | Key Metric / Output | Immediate Action |
| :--- | :--- | :--- | :--- |
| **CPU 100% (GC Thrashing)** | `jstat -gcutil` | `O >= 98%`, `FGCT` climbing | Capture heap dump, scale pod memory |
| **CPU 100% (Busy Loop)** | `top -H` + `jcmd Thread.print` | Single thread in `RUNNABLE` | Identify culprit code line, patch loop |
| **Throughput 0% (Deadlock)**| `jcmd Thread.print` | `Found one Java-level deadlock` | Restart pod, fix lock acquisition order |
| **DB Pool Timeout** | `pg_stat_activity` / Hikari metrics | `hikaricp.pending > 0` | Kill long queries, add DB index |

---

## 10. Interview Questions & Answers

### Q1: What is your systematic approach when an enterprise Java application suddenly spikes to 100% CPU in production?
<details>
<summary>View Answer</summary>
1. Check whether CPU is consumed by Garbage Collection: Run `jstat -gcutil <PID> 1000`. If Old Generation is $> 98\%$ and Full GC time is climbing continuously, the CPU spike is caused by GC thrashing due to heap exhaustion. The remedy is to capture a heap dump and inspect memory leaks.
2. If GC is normal, run `top -H -p <PID>` to identify the specific native Linux thread ID consuming CPU.
3. Convert the decimal thread ID to hexadecimal (`printf "0x%x
" <TID>`).
4. Generate a thread dump via `jcmd <PID> Thread.print` and grep for the hexadecimal native ID (`nid=0xHEX`).
5. Inspect the stack trace of the thread to identify the infinite loop, busy spin, or ReDoS regex causing the spike.
</details>

### Q2: What causes `java.lang.OutOfMemoryError: Metaspace` and how do you diagnose it?
<details>
<summary>View Answer</summary>
Metaspace stores class metadata, method bytecode, annotations, and constant pools. Metaspace exhaustion is caused by continuous class loading without unloading, typically triggered by frameworks that dynamically generate bytecode and proxies (CGLIB, ByteBuddy, Spring, Javassist) when classloaders are leaked. Diagnosis: Capture a class histogram (`jcmd <PID> GC.class_histogram`) to inspect loaded class counts, analyze heap dumps in Eclipse MAT to trace classloader reference chains preventing garbage collection, and set `-XX:MaxMetaspaceSize=512m` to bound growth.
</details>

### Q3: Why does calling a remote HTTP API inside a `@Transactional` database method frequently cause connection pool exhaustion?
<details>
<summary>View Answer</summary>
When a method is marked `@Transactional`, the application acquires a database connection from the pool immediately upon entering the method and holds it open until the method exits and commits. If the method makes a network call to a remote HTTP service that stalls for 5 seconds, that database connection remains idle and completely blocked for the full 5 seconds. Under high traffic, all available database connections are quickly tied up waiting on external network I/O, starving other requests and causing pool exhaustion.
</details>

### Q4: How does HotSpot detect thread deadlocks automatically?
<details>
<summary>View Answer</summary>
HotSpot maintains an internal object monitor and lock ownership graph. When `jcmd <PID> Thread.print` is executed (or during diagnostic checks), HotSpot runs a cycle detection algorithm (Tarjan\'s or DFS) on the directed graph of thread lock acquisitions. If Thread A holds Lock 1 and waits for Lock 2, while Thread B holds Lock 2 and waits for Lock 1, HotSpot detects the cycle and prints the exact threads, lock addresses, and monitor classes involved at the bottom of the dump.
</details>

### Q5: What is the difference between shallow heap and retained heap in memory leak analysis?
<details>
<summary>View Answer</summary>
- **Shallow Heap**: The memory consumed directly by the object itself (object header, primitive fields, and reference pointers). It is typically tiny (e.g. 24–64 bytes).
- **Retained Heap**: The total memory that would be freed by the garbage collector if that specific object were removed. It includes the object\'s shallow heap plus the shallow heap of all objects transitively reachable *solely* through this object. In memory leak investigations, engineers search for objects with massive retained heaps.
</details>

### Q6: How does HikariCP\'s `leak-detection-threshold` work?
<details>
<summary>View Answer</summary>
When `leak-detection-threshold` is configured (e.g. `15000` ms), HikariCP schedules a background timer task whenever a connection is checked out of the pool. If the application does not return the connection to the pool within 15 seconds, HikariCP logs a warning stack trace showing where the connection was originally checked out in code. If the connection is subsequently closed, it logs that it was returned, distinguishing between long-held operations and permanent connection leaks.
</details>

### Q7: What causes container Exit Code 137 in Kubernetes Java pods?
<details>
<summary>View Answer</summary>
Exit Code 137 indicates that the Linux kernel terminated the container with `SIGKILL` ($128 + 9 = 137$) because the container exceeded its cgroup memory limit (`resources.limits.memory`). This occurs when the total Resident Set Size (RSS) memory of the container—combining Java heap, Metaspace, thread stacks, JIT compiler memory, off-heap Direct ByteBuffers, and OS overhead—exceeds the allocated memory ceiling.
</details>

### Q8: What is a Thread Starvation incident and how does it manifest?
<details>
<summary>View Answer</summary>
Thread starvation occurs when all worker threads in a thread pool (e.g. Tomcat HTTP executor pool) are in a `WAITING` or `TIMED_WAITING` state waiting on resources (slow database queries, exhausted connection pools, or unresponsive downstream microservices). New incoming requests cannot be accepted and queue up or get rejected with HTTP 503 errors. In thread dumps, dozens or hundreds of worker threads are visible parked at the exact same socket read or lock acquisition line.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Deadlock Simulator
**Task:** Write a simple Java program that intentionally triggers a circular lock deadlock between two threads for diagnostic testing with `jcmd`.
<details>
<summary>View Solution</summary>

```java
public class DeadlockSimulator {
    private static final Object LOCK_A = new Object();
    private static final Object LOCK_B = new Object();

    public static void main(String[] args) {
        Thread t1 = new Thread(() -> {
            synchronized (LOCK_A) {
                System.out.println("Thread 1 acquired LOCK_A");
                try { Thread.sleep(100); } catch (InterruptedException ignored) {}
                synchronized (LOCK_B) {
                    System.out.println("Thread 1 acquired LOCK_B");
                }
            }
        }, "Deadlock-Thread-1");

        Thread t2 = new Thread(() -> {
            synchronized (LOCK_B) {
                System.out.println("Thread 2 acquired LOCK_B");
                try { Thread.sleep(100); } catch (InterruptedException ignored) {}
                synchronized (LOCK_A) {
                    System.out.println("Thread 2 acquired LOCK_A");
                }
            }
        }, "Deadlock-Thread-2");

        t1.start();
        t2.start();
    }
}
```
</details>

### Exercise 2: Automated CGroup Memory Diagnostic Script
**Task:** Write a shell script to inspect Kubernetes cgroup memory limits vs. current JVM RSS usage on Linux.
<details>
<summary>View Solution</summary>

```bash
#!/usr/bin/env bash
echo "=== CONTAINER CGROUP MEMORY AUDIT ==="
if [ -f /sys/fs/cgroup/memory.max ]; then
    LIMIT=$(cat /sys/fs/cgroup/memory.max)
    CURRENT=$(cat /sys/fs/cgroup/memory.current)
    echo "CGroup v2 Memory Limit: $((LIMIT / 1024 / 1024)) MB"
    echo "CGroup v2 Current RSS:  $((CURRENT / 1024 / 1024)) MB"
elif [ -f /sys/fs/cgroup/memory/memory.limit_in_bytes ]; then
    LIMIT=$(cat /sys/fs/cgroup/memory/memory.limit_in_bytes)
    CURRENT=$(cat /sys/fs/cgroup/memory/memory.usage_in_bytes)
    echo "CGroup v1 Memory Limit: $((LIMIT / 1024 / 1024)) MB"
    echo "CGroup v1 Current RSS:  $((CURRENT / 1024 / 1024)) MB"
fi
```
</details>

### Exercise 3: Quick Post-Mortem Template Generator
**Task:** Create a markdown template for recording root-cause incident reviews.
<details>
<summary>View Solution</summary>

```markdown
# Incident Post-Mortem: [INCIDENT_NAME]

## Summary
- **Date & Time:** YYYY-MM-DD HH:MM UTC
- **Duration:** XX minutes
- **Impact:** % of requests affected
- **Root Cause Category:** [High CPU / Memory Leak / Deadlock / DB Pool]

## Timeline
- **12:00 UTC:** Alert triggered
- **12:05 UTC:** On-call engineer engaged; captured thread dumps
- **12:10 UTC:** Pod restarted; traffic recovered

## Root Cause
Detailed description of the underlying defect.

## Corrective Actions
1. Code fix: PR #XXX
2. Alert tuning: Add P99 latency alert
```
</details>

---

## 12. Summary / Cheat Sheet

- **Triage Order**: Mitigate $\to$ Capture Forensics $\to$ Recover $\to$ Root Cause Analysis.
- **CPU 100%**: Check `jstat -gcutil` first. If GC normal, check `top -H` and convert TID to hex `nid`.
- **Memory Leaks**: Inspect Retained Heap in Eclipse MAT; look for unbounded collections/ThreadLocals.
- **Deadlocks**: Capture thread dump; HotSpot prints deadlock analysis automatically at bottom.
- **DB Connection Pools**: Never make HTTP calls inside `@Transactional`; enable leak detection.
- **Forensic Capture**: Always capture 3 thread dumps and GC stats before killing an impaired JVM.

---

## 13. Further Reading
- Eclipse Memory Analyzer (MAT): [User Guide & Concepts](https://eclipse.dev/mat/)
- Netflix TechBlog: [Java in Two Seconds (Startup & Diagnosis)](https://netflixtechblog.com/)
- HikariCP Wiki: [Bad Pool Sizing & Connection Leaks](https://github.com/brettwooldridge/HikariCP/wiki/About-Pool-Sizing)
- Site Reliability Engineering (SRE) Book: [Incident Management](https://sre.google/sre-book/incident-management/)
