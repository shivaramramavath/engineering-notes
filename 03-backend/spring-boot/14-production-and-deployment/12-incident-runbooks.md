# Production Incident Runbooks for Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | Kubernetes 1.31+, OpenTelemetry, Prometheus/Grafana, Linux cgroups v2 |

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
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
When a mission-critical Spring Boot microservice degrades in production—exhibiting sudden 504 Gateway Timeouts, JVM OutOfMemoryErrors (`java.lang.OutOfMemoryError: Java heap space`), CPU throttling at 100%, or HikariCP connection pool exhaustion—on-call engineers face extreme pressure. Ad-hoc troubleshooting, haphazard pod restarts, and guesswork prolong outages, destroy diagnostic evidence (heap dumps, thread states), and result in severe SLA violations.

### The Evolution
Modern incident management relies on codified, tested **Operational Incident Runbooks**. Instead of informal wikis, production runbooks provide deterministic step-by-step triage playbooks: triage criteria, immediate containment mitigation, diagnostic artifact extraction, root cause analysis (RCA) techniques, and safe rollback or scale procedures tailored specifically to Spring Boot and the Java runtime.

### Production Relevance
Having standardized runbooks for Spring Boot ensures rapid mean time to detect (MTTD) and mean time to resolve (MTTR). Automated alerts directly link on-call responders to the corresponding runbook section, preserving vital forensics before automated Kubernetes self-healing destroys the failing container.

---

## 2. Core Concepts

### Primary Spring Boot Production Failure Modes

```
+-----------------------------------------------------------------------------------------+
|                                SPRING BOOT FAILURE MODES                                |
+-----------------------------------------------------------------------------------------+
| Failure Mode        | Immediate Symptoms                | Primary Suspects              |
+---------------------+-----------------------------------+-------------------------------+
| OOM: Heap Space     | Pod CrashLoopBackOff (137 exit)   | Memory leak, large payload,   |
|                     | Actuator /health timeout          | unpaged DB query              |
+---------------------+-----------------------------------+-------------------------------+
| Connection Starve   | HikariCP: Connection timeout      | Long-running SQL, unclosed    |
|                     | Latency spike to 30,000ms         | connections, leak detection   |
+---------------------+-----------------------------------+-------------------------------+
| CPU Saturation      | K8s CPU throttling, high load avg | Infinite loop, RegEx catastrophic|
|                     | Latency p99 degradation           | backtracking, GC thrashing    |
+---------------------+-----------------------------------+-------------------------------+
| Thread Starvation   | Tomcat queue full, HTTP 503 drops | Blocking I/O on pinned threads|
|                     | Thread pool saturation            | Deadlock in sync blocks       |
+---------------------+-----------------------------------+-------------------------------+
| Downstream Outage   | Cascading 504 timeouts, retry     | Missing circuit breaker,      |
|                     | storms overloading gateway        | aggressive unjittered retries |
+-----------------------------------------------------------------------------------------+
```

### Invariants for Runbook Operations
- **Preserve Forensic Evidence First**: Never blindly restart or delete pods before capturing thread dumps and triggering heap dumps when memory or deadlock issues occur.
- **Fail Fast & Shed Load**: Mitigate customer impact by isolating or circuit-breaking failing dependencies before fixing the underlying root cause.
- **Automate Diagnostic Ingestion**: Configure JVM crash handlers (`-XX:+HeapDumpOnOutOfMemoryError`) to dump artifacts to persistent volumes.

---

## 3. How It Works Internally

### Diagnostic Signal Pipeline & Triage Architecture

```
[ Pod / Service ] 
       |
       +---> [ Prometheus Metrics ] ---> (AlertManager: PagerDuty / Slack)
       |         (Hikari, JVM, HTTP)                       |
       |                                                   v
       +---> [ OpenTelemetry Tracing ] ------------> [ On-Call Responder ]
       |         (Span latency > 3000ms)                   |
       |                                                   v
       +---> [ Stdout JSON Logs ] ---------------> [ Consult RUNBOOK ]
                 (Stack Traces, Errors)                    |
                                                           +---> 1. Containment (Scale / Circuit)
                                                           +---> 2. Extraction (jcmd / Actuator)
                                                           +---> 3. Resolution (Rollback / Hotfix)
```

1. **Detection**: AlertManager triggers on SLO breaches (e.g., `hikaricp_pending_threads > 10` for 2 minutes).
2. **Identification**: The alert payload carries labels linking to the exact runbook anchor (`runbook_url: "https://ops.internal/runbooks/spring#hikaricp-exhaustion"`).
3. **Forensic Capture**: The engineer accesses Actuator `/actuator/threaddump` or runs `jcmd` inside the debug ephemeral container.
4. **Mitigation**: Traffic is shifted via ingress canary or circuit breaker configuration to stabilize customer traffic.

---

## 4. Syntax & Usage

### Crucial JVM Diagnostic Flags in Production

Configure these in `JAVA_TOOL_OPTIONS` or the container command line:

```bash
JAVA_TOOL_OPTIONS="\
  -XX:+HeapDumpOnOutOfMemoryError \
  -XX:HeapDumpPath=/dumps/heap-dump-%p.hprof \
  -XX:+ExitOnOutOfMemoryError \
  -XX:NativeMemoryTracking=summary \
  -Djdk.tracePinnedThreads=full \
  -XX:+UseG1GC \
  -XX:MaxRAMPercentage=75.0"
```

### Spring Boot Actuator Endpoints for Incident Triage

Enable on management port only (`management.server.port=9090`):

```yaml
management:
  server:
    port: 9090
  endpoints:
    web:
      exposure:
        include: health,info,metrics,threaddump,heapdump,loggers,env
  endpoint:
    threaddump:
      enabled: true
    heapdump:
      enabled: true
    loggers:
      enabled: true
```

---

## 5. Practical Examples

### Example 1: Runbook 01 - HikariCP Connection Pool Exhaustion

#### Symptom & Detection
- **Alert**: `HikariCPConnectionAcquisitionTimeout`
- **Log Signature**: `java.sql.SQLTransientConnectionException: HikariPool-1 - Connection is not available, request timed out after 30000ms.`

```
+-----------------------------------------------------------------------------------------+
| STEP-BY-STEP TRIAGE: CONNECTION EXHAUSTION                                              |
+-----------------------------------------------------------------------------------------+
| 1. CHECK POOL METRICS                                                                   |
|    curl -s http://localhost:9090/actuator/metrics/hikaricp.connections.pending          |
|    curl -s http://localhost:9090/actuator/metrics/hikaricp.connections.active           |
|                                                                                         |
| 2. CAPTURE THREAD DUMP (Find who is holding connections)                                |
|    curl -s http://localhost:9090/actuator/threaddump > thread_dump.json                 |
|    grep -B 5 -A 15 "org.postgresql.jdbc" thread_dump.json                               |
|                                                                                         |
| 3. IDENTIFY SLOW QUERIES ON DATABASE                                                    |
|    SELECT pid, now() - query_start AS duration, query, state                             |
|    FROM pg_stat_activity WHERE state != 'idle' ORDER BY duration DESC LIMIT 5;          |
|                                                                                         |
| 4. IMMEDIATE CONTAINMENT                                                                |
|    - Terminate blocking backend query: SELECT pg_terminate_backend(<pid>);              |
|    - Increase max-pool-size dynamically if DB capacity allows:                           |
|      Set SPRING_DATASOURCE_HIKARI_MAXIMUM_POOL_SIZE=30 and rolling restart.             |
|                                                                                         |
| 5. PERMANENT FIX                                                                        |
|    - Add @Transactional(timeout = 5) on long running service methods.                   |
|    - Enable Hikari leak detection: `spring.datasource.hikari.leak-detection-threshold=5000` |
+-----------------------------------------------------------------------------------------+
```

### Example 2: Runbook 02 - JVM OutOfMemoryError (Heap Space / Metaspace)

#### Symptom & Detection
- **Alert**: `ContainerOOMKilled` (Exit code 137) or `JVMOOMHeapExhaustion`
- **Log Signature**: `java.lang.OutOfMemoryError: Java heap space`

```
+-----------------------------------------------------------------------------------------+
| STEP-BY-STEP TRIAGE: OOM HEAP EXHAUSTION                                                |
+-----------------------------------------------------------------------------------------+
| 1. PREVENT IMMEDIATE RESTART CASCADE                                                    |
|    Isolate one degraded pod from Service traffic by removing its label:                 |
|    kubectl label pod <pod-name> app-                                                    |
|                                                                                         |
| 2. EXTRACT HEAP DUMP BEFORE POD DIES                                                    |
|    # If actuator is alive:                                                              |
|    curl -O http://localhost:9090/actuator/heapdump                                      |
|    # If using kubectl on node:                                                          |
|    kubectl cp <pod-name>:/dumps/heap-dump.hprof ./heap-dump.hprof                        |
|                                                                                         |
| 3. ANALYZE TOP LEAK SUSPECTS (Using Eclipse MAT or jhat)                                |
|    jcmd <pid> GC.class_histogram | head -n 30                                           |
|                                                                                         |
| 4. IMMEDIATE CONTAINMENT                                                                |
|    - Scale out pod replicas to distribute incoming batch load:                          |
|      kubectl scale deployment order-service --replicas=8                                |
|    - Temporary memory limit increase in deployment manifest:                            |
|      resources.limits.memory: 4Gi, resources.requests.memory: 4Gi                       |
|                                                                                         |
| 5. ROOT CAUSE REMEDY                                                                    |
|    - Fix unpaginated `findAll()` query returning millions of rows into heap.            |
|    - Fix unbounded static caches (`Map<String, Object>`) by migrating to Caffeine with  |
|      `maximumSize` eviction.                                                            |
+-----------------------------------------------------------------------------------------+
```

### Example 3: Runbook 03 - High CPU & Thread Deadlock / Pinning

#### Symptom & Detection
- **Alert**: `HighCPUUtilization` (> 95% sustained for 3 minutes)
- **Symptom**: Pod unresponsive, HTTP 504 timeouts, Virtual thread carrier pinning

```
+-----------------------------------------------------------------------------------------+
| STEP-BY-STEP TRIAGE: CPU SATURATION & THREAD PINNING                                    |
+-----------------------------------------------------------------------------------------+
| 1. FIND HIGHEST CPU CONSUMING THREADS                                                   |
|    top -H -p <pid>                                                                      |
|    # Convert OS thread TID to hex: printf '%x\n' <tid>                                  |
|                                                                                         |
| 2. MATCH HEX TID AGAINST JVM THREAD STACK                                               |
|    jstack <pid> | grep -A 20 "nid=0x<hex_tid>"                                          |
|                                                                                         |
| 3. DETECT DEADLOCKS AUTOMATICALLY                                                       |
|    jcmd <pid> Thread.print | grep -i "Found one Java-level deadlock"                    |
|                                                                                         |
| 4. CHECK VIRTUAL THREAD PINNING                                                         |
|    # Look for logs triggered by -Djdk.tracePinnedThreads=full                           |
|    grep "pinned" /var/log/app.log                                                       |
|    # Culprit is typically `synchronized` blocks wrapping network socket I/O             |
|                                                                                         |
| 5. IMMEDIATE CONTAINMENT                                                                |
|    - Restart the stalled pod to clear the deadlocked thread pool.                       |
|    - Rate-limit or block offending upstream client IPs via Ingress WAF.                 |
|                                                                                         |
| 6. PERMANENT REMEDY                                                                     |
|    - Replace `synchronized` blocks with `ReentrantLock` for virtual thread safety.      |
|    - Optimize regex or infinite while-loop logic.                                       |
+-----------------------------------------------------------------------------------------+
```

### Example 4: Anti-Pattern (Blind Pod Restart) vs Runbook Triage

#### The Anti-Pattern: Blind Restart Without Evidence
```bash
# BAD: Operator notices slow requests and immediately runs:
kubectl delete pod --all -n production

# CONSEQUENCES:
# 1. Ephemeral pod disk is wiped; the .hprof heap dump is permanently deleted.
# 2. In-flight transactions are terminated abruptly, causing data corruption.
# 3. New pods start up cold simultaneously, hitting PostgreSQL with a massive
#    thundering herd of connection handshakes, causing total database outage!
```

#### Production Runbook Pattern: Forensic Quarantine & Controlled Drain
```bash
# GOOD:
# 1. Quarantine the degraded pod by stripping the routing label
kubectl label pod order-service-7f8d9b-x29k routing=quarantine --overwrite

# 2. Capture thread dump and active heap histogram via ephemeral container
kubectl debug pod/order-service-7f8d9b-x29k -it --image=eclipse-temurin:21-jdk --target=spring-app -- \
  jcmd 1 GC.class_histogram > /tmp/histogram.txt

# 3. Copy out evidence safely
kubectl cp order-service-7f8d9b-x29k:/tmp/histogram.txt ./histogram.txt

# 4. Gracefully terminate only after evidence is captured
kubectl delete pod order-service-7f8d9b-x29k --grace-period=30
```

### Example 5: Dynamic Log Level Adjustment During Outages

Use Actuator to increase log verbosity on a specific package without restarting the application:

```bash
# 1. Check current log level of payment gateway package
curl -X GET http://localhost:9090/actuator/loggers/com.example.payment

# Response:
# {"configuredLevel":"INFO","effectiveLevel":"INFO"}

# 2. Switch dynamically to DEBUG to capture detailed HTTP request/response payloads
curl -X POST http://localhost:9090/actuator/loggers/com.example.payment \
     -H "Content-Type: application/json" \
     -d '{"configuredLevel": "DEBUG"}'

# 3. Once incident is diagnosed, immediately revert to avoid log disk flooding!
curl -X POST http://localhost:9090/actuator/loggers/com.example.payment \
     -H "Content-Type: application/json" \
     -d '{"configuredLevel": "INFO"}'
```

---

## 6. Best Practices

1. **Pre-Configure Heap Dumps on OOM**: Always specify `-XX:+HeapDumpOnOutOfMemoryError` pointing to a mounted PersistentVolumeClaim to retain artifacts after pod exit.
2. **Implement Connection Leak Detection**: Set `spring.datasource.hikari.leak-detection-threshold=5000` (5 seconds). HikariCP will log a stack trace of any code holding a connection longer than 5s.
3. **Quarantine Before Restarting**: Remove service labels from one failing pod to prevent Kubernetes from sending traffic while engineers inspect thread states.
4. **Isolate Actuator on Separate Port**: Run Actuator on internal port `9090` (`management.server.port=9090`) to prevent exposing heap dumps and environment secrets to the public Internet.
5. **Always Set Transaction Timeouts**: Add explicit timeouts (`@Transactional(timeout = 5)`) to prevent queries from hanging forever during database deadlocks.
6. **Limit Dynamic Log Level Scope**: When changing log levels via Actuator, target the specific failing package (e.g., `com.example.order.client`) rather than `ROOT`.
7. **Document Playbooks in Git**: Store runbooks alongside service code in markdown, linked directly in alert notification templates.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Restarting pods immediately upon alert** | Wipes in-flight state and destroys heap/thread dump evidence, guaranteeing the outage will recur. | Quarantine the pod, capture `jcmd` forensics, and allow rolling replacements. |
| **Omitting HikariCP leak detection** | Connections leak silently over days until the pool is exhausted; root cause code is impossible to identify. | Set `leak-detection-threshold=5000` to log offending stack traces. |
| **Exposing `/actuator/heapdump` publicly** | Anyone can download JVM heap memory containing unencrypted database passwords, API tokens, and PII. | Bind management to private port 9090 and enforce Spring Security RBAC. |
| **Setting pool size equal to container CPU count** | Too small connection pools cause false starvation under burst traffic. | Calculate pool size using PostgreSQL/Hikari formula: `(core_count * 2) + effective_spindle_count`. |
| **Running thread dumps using `kill -3` blindly** | Output writes to standard out, mingling with application JSON logs and overwhelming log collector pipelines. | Use `jcmd <pid> Thread.print` or Actuator `/threaddump` directly to file. |
| **Ignoring Virtual Thread Pinning** | `synchronized` blocks inside third-party libraries pin carrier threads, starving the entire JVM. | Run with `-Djdk.tracePinnedThreads=full` to catch and replace pinned blocks. |

---

## 8. Performance & Security Notes

### Performance
- **Heap Dump Impact**: Triggering `/actuator/heapdump` induces a full JVM Stop-the-World pause. On a 16GB heap, this pause can last 5–15 seconds. Ensure the pod is removed from load balancer rotation before triggering!
- **Class Histogram Alternative**: Before capturing a multi-gigabyte heap dump, run `jcmd <pid> GC.class_histogram`. It produces a fast, lightweight summary of top object instances in milliseconds.

### Security
- **Data Scrubbing in Dumps**: Heap dumps contain decrypted TLS certificates, OAuth tokens, and customer passwords. Store `.hprof` files in encrypted, access-restricted S3 buckets with 7-day auto-expiry policies.
- **Actuator Security**: Restrict `/actuator/loggers`, `/actuator/env`, and `/actuator/heapdump` behind `ROLE_ACTUATOR_ADMIN` using HTTP Basic or mTLS authentication.

---

## 9. Comparison / When to Use What

| Diagnostic Tool | Overhead | Information Provided | Best Used For |
|---|---|---|---|
| **Actuator `/health`** | Negligible | UP/DOWN status of components | Kubernetes liveness/readiness probes |
| **Actuator `/threaddump`** | Very Low | Full thread stack traces and states | Detecting deadlocks, thread pool exhaustion |
| **`jcmd GC.class_histogram`** | Low (Minor STW) | Object counts and total bytes by class | Fast memory leak identification |
| **Actuator `/heapdump`** | High (5-15s STW) | Complete binary memory graph | Deep memory leak RCA in Eclipse MAT |
| **Dynamic Logger (`/loggers`)** | Negligible | Runtime package log level change | Reproducing intermittent edge-case bugs |
| **OpenTelemetry Trace** | Negligible | End-to-end distributed span latencies | Identifying slow downstream microservices |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. An alert fires: "HikariCP-1 Connection is not available, request timed out after 30000ms." What are your first three actions?</strong></summary>

1. **Check Active vs Pending Connections**: Query `/actuator/metrics/hikaricp.connections.active` and `hikaricp.connections.pending`. If active equals max pool size (e.g., 10/10) and pending is high, the pool is completely starved.
2. **Check Database Server Locks**: Query PostgreSQL `pg_stat_activity` for active queries running longer than 10 seconds or waiting on table/row locks (`wait_event_type = 'Lock'`).
3. **Capture a Thread Dump**: Fetch `/actuator/threaddump` to locate Java application threads in `WAITING` or `TIMED_WAITING` state on `com.zaxxer.hikari.pool.HikariPool.getConnection` and see what the active threads are doing (e.g., waiting on third-party HTTP call inside `@Transactional`).
</details>

<details>
<summary><strong>2. Why should third-party REST API calls never be executed inside a `@Transactional` Spring service method?</strong></summary>

When a method is annotated with `@Transactional`, Spring Boot acquires a physical database connection from the HikariCP pool *before* entering the method.
If that method executes a REST call to an external payment gateway that hangs for 30 seconds due to a network glitch:
- The database connection remains checked out and idle for 30 seconds.
- Multiplied across concurrent requests, all connection pool slots are quickly consumed.
- The entire service enters total connection starvation, even though the database itself is idle.
**Remedy**: Execute third-party I/O outside the transaction boundary and wrap only the database persistence step in `@Transactional`.
</details>

<details>
<summary><strong>3. How does `-XX:+HeapDumpOnOutOfMemoryError` work and how do you ensure the file survives a Kubernetes container crash?</strong></summary>

When the JVM detects that it cannot allocate memory in the heap and throws `OutOfMemoryError`, the hotspot runtime generates a core heap dump (`.hprof`) before terminating.
Because Kubernetes pods have ephemeral filesystems, deleting or restarting the pod permanently erases the local dump file.
**Solution**:
1. Mount a Kubernetes `PersistentVolumeClaim` (PVC) or host path to `/dumps`.
2. Configure `-XX:HeapDumpPath=/dumps/app-oom.hprof`.
3. Set pod `restartPolicy: OnFailure` so the pod remains available in `CrashLoopBackOff` status for engineers to copy the dump file via `kubectl cp`.
</details>

<details>
<summary><strong>4. What is thread pinning in Java 21 Virtual Threads and how do you triage it in production?</strong></summary>

Virtual threads run on a pool of underlying OS carrier threads. A virtual thread becomes **pinned** to its carrier thread when it executes a blocking operation inside a `synchronized` block or native JNI call.
When pinned:
- The carrier thread cannot be released to execute other virtual threads.
- If all carrier threads become pinned simultaneously, throughput drops to zero, simulating a thread starvation freeze.
**Triage**:
- Launch with `-Djdk.tracePinnedThreads=full` or `short`.
- Inspect logs for stack traces showing `java.lang.VirtualThread.parkOnCarrierThread`.
- Refactor the code by replacing `synchronized(lock)` with `ReentrantLock`.
</details>

<details>
<summary><strong>5. How do you safely diagnose a memory leak in production without taking down customer traffic?</strong></summary>

1. Identify the leaking pod using Prometheus `jvm_memory_used_bytes{area="heap"}`.
2. Edit the pod's labels (`kubectl label pod <name> app-`) so Kubernetes Service endpoints stop routing live customer traffic to it.
3. The pod is now quarantined from customer requests.
4. Run `jcmd <pid> GC.class_histogram` to inspect top classes by shallow size.
5. If necessary, execute `curl -O http://localhost:9090/actuator/heapdump` to generate a full heap dump. The Stop-the-World pause will not affect customers because traffic was already diverted.
6. Transfer the dump to an analysis workstation and terminate the quarantined pod.
</details>

<details>
<summary><strong>6. What is the difference between a Liveness Probe failure and a Readiness Probe failure during an incident?</strong></summary>

- **Liveness Probe Failure**: Kubernetes assumes the application has deadlocked or crashed irreversibly. It abruptly terminates the container and creates a new one (`SIGTERM` -> `SIGKILL`).
- **Readiness Probe Failure**: Kubernetes assumes the application is temporarily unable to handle traffic (e.g., warming up caches, or connection pool momentarily full). It **does not restart** the pod; it simply removes the pod's IP from the Service endpoints until the probe succeeds again.
**Rule**: Never configure database ping checks inside the *Liveness* probe. If the database goes down, all pods will fail liveness simultaneously and enter a catastrophic CrashLoopBackOff restart storm.
</details>

<details>
<summary><strong>7. How do you analyze a thread dump to find a Java-level deadlock?</strong></summary>

A thread dump lists all active threads, their state (`RUNNABLE`, `BLOCKED`, `WAITING`), and the lock objects they hold or await.
Hotspot JVM includes an automatic deadlock detector:
1. Search the bottom of the thread dump for the string: `Found one Java-level deadlock:`.
2. The output displays the cyclic dependency:
   - `Thread-1` holds lock `<0x00000007>` and is waiting for `<0x00000008>`.
   - `Thread-2` holds lock `<0x00000008>` and is waiting for `<0x00000007>`.
3. Locate the corresponding source code lines in the stack trace and ensure lock acquisition orders are unified.
</details>

<details>
<summary><strong>8. What is the role of Actuator `/actuator/loggers` during a high-severity incident?</strong></summary>

In production, log levels are typically set to `INFO` or `WARN` to conserve disk and network I/O.
When a rare bug or security breach occurs:
- The engineer sends an HTTP POST request to `/actuator/loggers/<package>` setting `configuredLevel: DEBUG`.
- The logger immediately outputs debug logs without restarting the JVM or re-deploying containers.
- Once the diagnostic logs are captured, the engineer sends another POST setting it back to `INFO`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Query Thread Dumps via Actuator CLI</strong></summary>

**Objective**: Use `curl` and `jq` to extract threads currently in `BLOCKED` state from a running Spring Boot Actuator endpoint.

**Task**: Write the command to filter blocked threads.

**Solution**:
```bash
curl -s http://localhost:9090/actuator/threaddump | \
  jq '.threads[] | select(.threadState == "BLOCKED") | {name: .threadName, id: .threadId, lock: .lockInfo.className}'
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Set HikariCP Connection Leak Detection Threshold</strong></summary>

**Objective**: Configure Spring Boot to log a warning whenever a database connection is held by a thread for longer than 4,000 milliseconds.

**Solution**:
In `application.yaml`:
```yaml
spring:
  datasource:
    hikari:
      leak-detection-threshold: 4000
```
When a connection is checked out for >4s, Hikari logs:
`Apparent connection leak detected on thread [http-nio-8080-exec-3], connection [HikariProxyConnection@12345] held for 4002ms` with the exact stack trace where `getConnection()` was called.
</details>

<details>
<summary><strong>Exercise 3 (Medium): Automate JVM Crash Dump Storage on Kubernetes</strong></summary>

**Objective**: Write the volume mount and environment variable snippet for a Kubernetes Deployment that ensures OOM heap dumps are saved to a persistent volume.

**Solution**:
```yaml
spec:
  containers:
  - name: order-service
    image: order-service:latest
    env:
    - name: JAVA_TOOL_OPTIONS
      value: "-XX:+HeapDumpOnOutOfMemoryError -XX:HeapDumpPath=/dumps/oom-%p.hprof -XX:MaxRAMPercentage=75.0"
    volumeMounts:
    - name: heap-dumps
      mountPath: /dumps
  volumes:
  - name: heap-dumps
    persistentVolumeClaim:
      claimName: shared-diagnostics-pvc
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Build an Ephemeral Debug Container with Diagnostic Tools</strong></summary>

**Objective**: Use `kubectl debug` to attach a container with `jcmd`, `jstack`, and `top` to a production distroless Spring Boot pod without restarting it.

**Solution**:
```bash
kubectl debug -it pod/order-service-9d7f4-kb28s \
  --image=eclipse-temurin:21-jdk-jammy \
  --target=order-service \
  -- /bin/bash

# Inside the debug shell:
# Find target Java process ID
ps aux | grep java
# Run class histogram
jcmd 1 GC.class_histogram | head -n 25
# Run thread dump
jstack 1 > /tmp/prod_thread_dump.txt
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Script Automated Quarantine and Forensics Extraction</strong></summary>

**Objective**: Write a Bash incident automation script that takes a failing pod name, removes its service routing label, extracts a thread dump and class histogram, and stores them in a timestamped tarball.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

POD_NAME="${1:?Usage: $0 <pod-name>}"
NAMESPACE="${2:-production}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT_DIR="./incident_${POD_NAME}_${TIMESTAMP}"

mkdir -p "${OUTPUT_DIR}"
echo "Step 1: Quarantining pod ${POD_NAME}..."
kubectl label pod "${POD_NAME}" -n "${NAMESPACE}" app- routing=quarantined --overwrite

echo "Step 2: Capturing thread dump..."
kubectl exec "${POD_NAME}" -n "${NAMESPACE}" -- curl -s http://localhost:9090/actuator/threaddump > "${OUTPUT_DIR}/threaddump.json"

echo "Step 3: Capturing memory histogram..."
kubectl exec "${POD_NAME}" -n "${NAMESPACE}" -- jcmd 1 GC.class_histogram > "${OUTPUT_DIR}/class_histogram.txt"

echo "Step 4: Compressing forensics..."
tar -czvf "${OUTPUT_DIR}.tar.gz" -C "${OUTPUT_DIR}" .
echo "Forensics collected successfully in ${OUTPUT_DIR}.tar.gz. Ready for safe pod restart."
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Implement Emergency Degraded-Mode Circuit Breaker via Actuator</strong></summary>

**Objective**: Create a custom Actuator endpoint that allows on-call operators to toggle an "emergency circuit breaker" that sheds heavy non-critical background jobs during database saturation.

**Solution**:
```java
package com.example.incident;

import org.springframework.boot.actuate.endpoint.annotation.Endpoint;
import org.springframework.boot.actuate.endpoint.annotation.ReadOperation;
import org.springframework.boot.actuate.endpoint.annotation.WriteOperation;
import org.springframework.stereotype.Component;

import java.util.concurrent.atomic.AtomicBoolean;

@Component
@Endpoint(id = "emergency-mode")
public class EmergencyModeEndpoint {

    private final AtomicBoolean degradedModeActive = new AtomicBoolean(false);

    @ReadOperation
    public boolean isDegradedModeActive() {
        return degradedModeActive.get();
    }

    @WriteOperation
    public String setDegradedMode(boolean enable) {
        degradedModeActive.set(enable);
        return "Degraded mode status updated: " + enable;
    }
}
```

Trigger via:
```bash
curl -X POST http://localhost:9090/actuator/emergency-mode \
     -H "Content-Type: application/json" \
     -d '{"enable": true}'
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| SPRING BOOT INCIDENT RUNBOOK QUICK REFERENCE                                                       |
+----------------------------------------------------------------------------------------------------+
| Incident Type       | Primary Command / Metric                 | Containment Action                |
+---------------------+------------------------------------------+-----------------------------------+
| Hikari Exhaustion   | actuator/metrics/hikaricp.connections.*  | Kill long-running SQL queries in  |
|                     | leak-detection-threshold=5000            | DB; check for I/O inside @Tx      |
+---------------------+------------------------------------------+-----------------------------------+
| Heap OOM            | -XX:+HeapDumpOnOutOfMemoryError          | Quarantine pod; capture jcmd      |
|                     | jcmd <pid> GC.class_histogram            | class_histogram; scale out        |
+---------------------+------------------------------------------+-----------------------------------+
| CPU 100% / Deadlock | top -H -p <pid>                          | Thread dump; check for regex      |
|                     | jcmd <pid> Thread.print                  | loops or carrier thread pinning   |
+---------------------+------------------------------------------+-----------------------------------+
| Virtual Thread Pin  | -Djdk.tracePinnedThreads=full            | Identify synchronized blocks      |
|                     | grep "pinned" /var/log/app.log           | wrapping blocking network sockets |
+---------------------+------------------------------------------+-----------------------------------+
| Secret Exfiltration | Check /actuator/env & /heapdump          | Enforce management.server.port    |
|                     | Ensure management port 9090 is private   | and Spring Security roles         |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Boot Actuator Production Operations Guide](https://docs.spring.io/spring-boot/docs/current/reference/html/actuator.html)
- [HikariCP Pool Sizing and Troubleshooting](https://github.com/brettwooldridge/HikariCP/wiki/About-Pool-Sizing)
- [OpenJDK JVM Diagnostic Command Reference (`jcmd`)](https://docs.oracle.com/en/java/javase/21/docs/specs/man/jcmd.html)
- [Site Reliability Engineering: How Google Runs Production Systems](https://sre.google/sre-book/table-of-contents/)
- [Brendan Gregg's Systems Performance: Enterprise and the Cloud](https://www.brendangregg.com/systems-performance-2nd-edition-book.html)
