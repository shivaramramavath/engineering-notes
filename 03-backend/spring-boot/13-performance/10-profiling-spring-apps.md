# Production Profiling for Spring Boot: JFR and async-profiler

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
- [4. Syntax & Profiling Command Reference](#4-syntax--profiling-command-reference)
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
When a Spring Boot application experiences latency degradation, CPU saturation, or memory churn in production, external metrics (Prometheus, APM agents) only reveal *what* is failing (e.g. "Endpoint `/api/checkout` has 800ms P99 latency"), but cannot reveal *why* or *where* in the code the time is being spent:
- Is CPU time burned in Jackson JSON serialization, regular expression evaluation, or cryptographic hashing?
- Are threads blocked on database lock acquisition, cache synchronization, or logging I/O?
- Which specific method is allocating millions of short-lived objects that trigger frequent GC thrashing?

Legacy profilers (YourKit, JProfiler, VisualVM) rely on **bytecode instrumentation** or uncoordinated thread sampling, which imposes severe performance overhead (10% to 50% CPU penalty) and introduces the **Safepoint Bias problem**, making them completely unsafe for production use.

### Historical Evolution
Modern production profiling is defined by two low-overhead, zero-safepoint-bias technologies:
1. **JDK Flight Recorder (JFR)**: Built directly into the HotSpot JVM kernel. It continuously collects CPU execution samples, memory allocations, thread locks, and I/O events with **less than 1% runtime overhead**, making it safe to leave enabled 24/7 in production.
2. **async-profiler**: An open-source, highly accurate sampling profiler for Linux and macOS that leverages the HotSpot `AsyncGetCallTrace` API and Linux `perf_events` to capture both Java code and native kernel/C++ frames without safepoint bias, rendering actionable **Interactive Flamegraphs**.

### Production Relevance
Continuous profiling enables engineering teams to:
- Instantly diagnose production CPU spikes down to the exact line of Java code.
- Eliminate object allocation hotspots before they trigger GC pressure.
- Profile live container pods in Kubernetes without restarting the application or attaching intrusive remote debuggers.

---

## 2. Core Concepts

### Profiling Fundamentals
1. **Safepoint Bias**: Traditional profilers can only capture thread stack traces when threads reach a JVM "safepoint" (e.g. loop boundaries, allocations). Code executing tight loops or JNI calls without safepoints appears invisible, skewing profile results.
2. **`AsyncGetCallTrace`**: A non-safepoint HotSpot internal API used by async-profiler to capture stack traces at arbitrary times (e.g. on OS timer interrupts), eliminating safepoint bias.
3. **CPU Profiling**: Identifies methods consuming the most CPU cycles (on-CPU execution time).
4. **Allocation Profiling**: Identifies code paths responsible for allocating memory on the Java heap, pinpointing GC pressure sources.
5. **Lock / Wall-Clock Profiling**: Identifies where threads spend time waiting (off-CPU latency: I/O waits, lock contention, thread park).
6. **Flamegraph**: A hierarchical visualization where the $X$-axis represents the population percentage of samples (width = time or memory consumed) and the $Y$-axis represents call stack depth.

---

## 3. How It Works Internally

### async-profiler vs. Legacy Profiler Mechanics

```
A. LEGACY SAMPLING PROFILER (Safepoint Biased - High Overhead)
   OS Timer Fires -> JVM requests Global Safepoint -> ALL Threads Pause ->
   Samples Stack Traces -> Threads Resume (Causes latency skew & 15% CPU hit!)

B. ASYNC-PROFILER (Lock-Free AsyncGetCallTrace - < 1% Overhead)
   Linux perf_events Timer Fires -> Sends SIGPROF signal directly to thread ->
   Signal Handler invokes AsyncGetCallTrace() -> Records Stack Trace to RingBuffer ->
   Thread continues IMMEDIATELY without safepoint pause! (Zero latency impact!)
```

### Interpreting a Flamegraph

```
+------------------------------------------------------------------------------------+
|                                    FLAMEGRAPH                                      |
|                                                                                    |
|       +---------------------------------------------+                              |
|       |   com.fasterxml.jackson.databind.ObjectMapper   | (Wide box = 45% of CPU!) |
|       +---------------------------------------------+                              |
|       |        com.example.service.OrderService      |                             |
|       +----------------------------------------------+                             |
|       |     org.springframework.web.servlet...       |                             |
| +--------------------------------------------------------------------------------+ |
| |                                 main / Tomcat                                  | |
| +--------------------------------------------------------------------------------+ |
+------------------------------------------------------------------------------------+
  Reading Rule:
  1. Width of a box is proportional to the fraction of CPU time spent in that method.
  2. The top-most "plateaus" (wide boxes with no children on top) are the true bottlenecks!
```

---

## 4. Syntax & Profiling Command Reference

### 1. Profiling with JDK Flight Recorder (JFR)
```bash
# Method 1: Start 60-second diagnostic recording via jcmd
jcmd <PID> JFR.start name=prod-profile duration=60s filename=/tmp/profile.jfr settings=profile

# Method 2: Check recording status
jcmd <PID> JFR.check

# Method 3: Continuous 24/7 Rolling JFR in Container (Keeps last 1 hour)
JAVA_TOOL_OPTIONS="-XX:StartFlightRecording=disk=true,dumponexit=true,filename=/var/log/dumps/rolling.jfr,maxage=1h,settings=profile"
```

### 2. Profiling with async-profiler (CLI)
```bash
# 1. Profile CPU for 30 seconds and generate interactive HTML Flamegraph
./asprof -d 30 -f /tmp/cpu-flamegraph.html <PID>

# 2. Profile Heap Allocations (Identifies GC churn hotspots)
./asprof -d 30 -e alloc -f /tmp/alloc-flamegraph.html <PID>

# 3. Profile Lock Contention (Finds blocked threads)
./asprof -d 30 -e lock -f /tmp/lock-flamegraph.html <PID>

# 4. Profile Wall-Clock Time (Identifies I/O & sleeping bottlenecks)
./asprof -d 30 -e wall -t -f /tmp/wall-flamegraph.html <PID>
```

---

## 5. Practical Examples

### Example 1: Integrating async-profiler into Spring Boot Actuator
We can expose an administrative endpoint to capture on-demand flamegraphs directly over HTTP without SSH access:

```java
package com.example.performance.profiling;

import org.springframework.boot.actuate.endpoint.annotation.Endpoint;
import org.springframework.boot.actuate.endpoint.annotation.ReadOperation;
import org.springframework.boot.actuate.endpoint.annotation.Selector;
import org.springframework.stereotype.Component;

import java.io.File;
import java.io.InputStream;
import java.nio.file.Files;
import java.util.concurrent.TimeUnit;

@Component
@Endpoint(id = "profiler")
public class AsyncProfilerEndpoint {

    private static final String ASPROF_PATH = "/opt/async-profiler/bin/asprof";

    @ReadOperation(produces = "text/html")
    public byte[] captureProfile(@Selector String event, Integer durationSeconds) {
        int duration = (durationSeconds != null && durationSeconds > 0) ? durationSeconds : 15;
        String eventType = (event != null && !event.isBlank()) ? event : "cpu";

        File outputFile = new File("/tmp/profile-" + System.currentTimeMillis() + ".html");
        try {
            long pid = ProcessHandle.current().pid();
            Process process = new ProcessBuilder(
                ASPROF_PATH,
                "-d", String.valueOf(duration),
                "-e", eventType,
                "-f", outputFile.getAbsolutePath(),
                String.valueOf(pid)
            ).start();

            boolean finished = process.waitFor(duration + 10, TimeUnit.SECONDS);
            if (!finished || process.exitValue() != 0) {
                throw new IllegalStateException("Profiling failed or timed out");
            }

            return Files.readAllBytes(outputFile.toPath());
        } catch (Exception ex) {
            return ("<h3>Profiling Error: " + ex.getMessage() + "</h3>").getBytes();
        } finally {
            outputFile.delete();
        }
    }
}
```

**Triggering Profile via Browser or Curl**:
```bash
# Capture 30-second CPU flamegraph directly from production container
curl -X GET "http://localhost:9090/actuator/profiler/cpu?durationSeconds=30" > cpu.html
open cpu.html # Opens interactive SVG/HTML flamegraph in browser!
```

### Example 2: Diagnosing an Allocation Churn Hotspot with JFR
Consider a service experiencing frequent Minor GC pauses due to rapid object allocations:

```java
package com.example.performance.profiling;

import org.springframework.stereotype.Service;

import java.util.UUID;
import java.util.regex.Pattern;

@Service
public class InefficientDataService {

    // ANTI-PATTERN: Re-compiling regex pattern inside a loop!
    // Allocates millions of Pattern and Node objects, causing high allocation rate!
    public boolean validateUserData(String email) {
        // CATASTROPHIC: Recompiled on every single incoming HTTP call!
        Pattern emailPattern = Pattern.compile("^[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,6}$", Pattern.CASE_INSENSITIVE);
        return emailPattern.matcher(email).matches();
    }
}
```

**Analyzing in JDK Mission Control (JMC)**:
1. Open `profile.jfr` in JMC.
2. Navigate to **Memory $\rightarrow$ Allocations by Method**.
3. *Top Allocator*: `java.util.regex.Pattern.compile()` accounts for **65% of all heap allocations** ($400\text{ MB/sec}$).
4. *Remedy*: Pre-compile pattern as a `private static final Pattern`.

### Example 3: Wall-Clock Profiling to Detect Hidden Network Latency
When CPU utilization is low ($< 10\%$) but request latency is high ($> 2\text{s}$), CPU profiling shows almost no samples. **Wall-clock profiling** captures what threads are waiting on:

```bash
./asprof -d 15 -e wall -f /tmp/wall-clock.html <PID>
```
*Flamegraph Result*:
- Wide plateau in `java.net.SocketInputStream.read()` inside `com.example.client.InventoryRestClient`.
- *Finding*: Threads are blocked for 1.8 seconds waiting for a slow third-party inventory API without an asynchronous cache.

### Example 4: Anti-Pattern vs. Production Fix (Logging Overhead Revealed by Profiler)
```java
// ANTI-PATTERN: Calling toString() on massive entity graphs inside debug logging
// Profiler reveals that 30% of total CPU time is spent inside Jackson / Reflection toString()!
@Service
public class BadAuditLoggingService {
    public void recordAction(ComplexOrderGraph order) {
        // Heavy serialization executed even if DEBUG is disabled if written with concatenation!
        log.debug("Processed order graph: " + order.toString()); 
    }
}

// PRODUCTION FIX: Guard with isDebugEnabled() and use lightweight identifiers
@Service
public class OptimizedAuditLoggingService {
    public void recordAction(ComplexOrderGraph order) {
        if (log.isDebugEnabled()) {
            log.debug("Processed order id: {}", order.getId());
        }
    }
}
```

### Example 5: Continuous Production JFR Archiving Script
```bash
#!/bin/bash
# Automatically dumps JFR profile upon receiving SIGUSR2 or incident trigger
PID=$(pgrep -f "spring-boot")
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTFILE="/var/log/dumps/incident_${TIMESTAMP}.jfr"

echo "Dumping last 10 minutes of JFR data for PID $PID..."
jcmd $PID JFR.dump name=prod-profile filename=$OUTFILE

echo "Compressing JFR profile..."
gzip $OUTFILE
echo "JFR Profile saved to ${OUTFILE}.gz"
```

---

## 6. Best Practices & Production Guidelines

1. **Keep Continuous 24/7 JFR Recording Active**: JFR has $< 1\%$ overhead. Configure a circular buffer (`-XX:StartFlightRecording=disk=true,maxage=30m`) so that when a production incident occurs, you already have the last 30 minutes of high-resolution profile data available for post-mortem analysis.
2. **Use async-profiler for CPU and Allocation Flamegraphs**: When performing targeted live triage, async-profiler's interactive HTML flamegraphs allow engineers to zoom into specific call stacks and search method names interactively.
3. **Always Check Allocation Profiling During GC Pauses**: If GC pause metrics spike, run an allocation profile (`-e alloc`). Optimizing the top 2 allocating methods frequently reduces GC pause frequency by 80%.
4. **Use Wall-Clock Profiling for Latency / Virtual Threads**: For investigating latency or Virtual Thread unmounting/blocking bottlenecks, CPU profiling is insufficient. Use `-e wall` to capture off-CPU blocking states.
5. **Install Debug Symbols in Containers**: Ensure the base container image includes `gdb` or standard debug symbols so async-profiler can resolve native Linux kernel and C++ JVM frames.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Using bytecode-instrumenting profilers in production | Bytecode injection (e.g. YourKit default) slows down execution by 20–50%, causing cascade failures. | Standardize strictly on JFR and async-profiler. |
| Relying only on CPU profiling for latency issues | When threads are waiting on I/O or database locks, they consume 0% CPU, appearing invisible in CPU profiles. | Use Wall-Clock profiling (`-e wall`). |
| Overlooking allocation churn | Code allocates millions of short-lived objects; CPU looks normal, but application pauses repeatedly. | Profile allocations with `-e alloc`. |
| Missing Linux `perf_events` permissions in Docker | async-profiler fails with `perf_event_open() failed: Permission denied`. | Run container with `CAP_PERFMON` or `CAP_SYS_ADMIN`, or set `sysctl kernel.perf_event_paranoid=1`. |
| Ignoring wide plateaus in Flamegraphs | Spending engineering effort optimizing narrow boxes while ignoring the wide, flat plateaus at the top. | Always target the widest boxes at the top of the flamegraph first. |
| Keeping profiling sessions too long | Running a 1-hour async-profiler session generates a 500MB flamegraph that crashes web browsers. | Keep profile durations between 15 and 60 seconds. |

---

## 8. Performance & Security Notes

### Overhead of JFR vs. async-profiler
- **JFR**: Continuous overhead is $< 1\%$, fully managed by HotSpot thread local event buffers.
- **async-profiler**: Active sampling overhead is $\approx 1\text{--}2\%$. It is intended for on-demand 15–60 second targeted diagnostic bursts.

### Security of Profiling Data
- Profiler call stacks reveal internal class names, method parameters, and architecture layout.
- Protect profiling endpoints (like `/actuator/profiler`) behind strict administrative roles (`ROLE_OPS_ADMIN`) and mutual TLS.

---

## 9. Comparison / When to Use What

| Diagnostic Tool | Primary Focus | Runtime Overhead | Safepoint Biased? | Safe for 24/7 Production? |
|---|---|---|---|---|
| **JDK Flight Recorder (JFR)** | Holistic JVM events (CPU, memory, locks, GC, I/O) | **$< 1\%$** | **No** | **Yes (Standard Practice)** |
| **async-profiler** | High-precision CPU, Allocation, and Lock Flamegraphs | **$\approx 1\text{--}2\%$** | **No** | **Yes (On-demand bursts)** |
| **VisualVM / JConsole** | Real-time graphs, MBeans | Medium | Yes | Only in Staging / Dev |
| **Commercial APM (Datadog/Dynatrace)**| End-to-end tracing, service topology | $2\text{--}5\%$ | Varies | Yes |

---

## 10. Interview Questions & Answers

### Q1: What is the "Safepoint Bias" problem in Java profiling?
<details>
<summary>Click to view answer</summary>

Traditional profilers rely on JVM thread dumps or sampling mechanisms that require threads to stop at a "safepoint" to capture a stack trace. Safepoints are typically placed at method returns, loop iterations, or object allocations. Code paths that do not contain safepoints (e.g. uncounted loops, array copies, or native JNI calls) cannot be interrupted, making them invisible to the profiler. This distorts the profile, falsely blaming methods that contain safepoints rather than the code that actually burned the CPU.
</details>

### Q2: How does async-profiler avoid Safepoint Bias?
<details>
<summary>Click to view answer</summary>

async-profiler uses the Linux kernel's `perf_events` subsystem to deliver asynchronous timer interrupts (`SIGPROF`) directly to the target thread. When the signal handler executes, it calls HotSpot's internal `AsyncGetCallTrace` function. This function reads the thread's program counter and walks the call stack directly from the signal handler context without waiting for the thread to reach a JVM safepoint, capturing true unbiased CPU usage.
</details>

### Q3: What is the difference between CPU profiling and Wall-Clock profiling?
<details>
<summary>Click to view answer</summary>

- **CPU Profiling**: Samples threads that are actively executing instructions on a CPU core (`RUNNABLE` and consuming clock cycles). Ideal for identifying computationally expensive algorithms.
- **Wall-Clock Profiling**: Samples all threads regardless of state (`RUNNABLE`, `BLOCKED`, `WAITING`, `TIMED_WAITING`). It measures total elapsed wall-clock time, revealing where threads are parked waiting for database responses, external REST calls, or synchronized locks.
</details>

### Q4: How do you interpret the width and height of boxes in a Flamegraph?
<details>
<summary>Click to view answer</summary>

- **Width ($X$-axis)**: Represents the percentage of total samples spent in that method (or its children). A wider box means more CPU time (or memory allocations) were consumed by that call tree.
- **Height ($Y$-axis)**: Represents the call stack depth. Root methods (e.g. `main`, Tomcat acceptor) are at the bottom; child methods are stacked above their callers.
- **Optimization Target**: Look for wide boxes at the top of the flamegraph that have no children ("plateaus"). These represent methods where the CPU is actively burning cycles inside the method body itself.
</details>

### Q5: What is JDK Flight Recorder (JFR) and what data does it collect?
<details>
<summary>Click to view answer</summary>

JFR is an event-recording engine built directly into the HotSpot JVM kernel. It records low-overhead hardware and software events:
- CPU execution samples and thread execution states.
- Memory allocations in TLAB (Thread-Local Allocation Buffers) and outside TLAB.
- Garbage collection phases and Stop-The-World pause durations.
- File and socket I/O read/write durations.
- Virtual thread pinning events (`jdk.VirtualThreadPinned`).
</details>

### Q6: What is an Allocation Flamegraph and how does it help optimize garbage collection?
<details>
<summary>Click to view answer</summary>

An allocation flamegraph measures the **bytes allocated** rather than CPU time spent. The width of each box corresponds to the volume of heap memory instantiated by that method. By identifying the widest plateaus, developers can pinpoint the exact constructors, collections, or serializers that generate excessive short-lived objects, allowing them to optimize allocation churn and eliminate GC pressure.
</details>

### Q7: Can JFR be triggered dynamically in Kubernetes without restarting the pod?
<details>
<summary>Click to view answer</summary>

Yes. Using `jcmd <PID> JFR.start name=incident duration=60s filename=/tmp/dump.jfr settings=profile`, a 60-second high-resolution recording can be triggered on a live container without any configuration change or process restart.
</details>

### Q8: What Linux capability is required to run async-profiler inside a Docker container?
<details>
<summary>Click to view answer</summary>

async-profiler requires access to Linux `perf_events`. In Docker or Kubernetes, the container must be granted `CAP_PERFMON` (Linux 5.8+) or `CAP_SYS_ADMIN`, or the host must set `sysctl -w kernel.perf_event_paranoid=1`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Trigger JFR Profile via Command Line (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write the `jcmd` command to start a 30-second JFR recording named `health-check` saving to `/tmp/health.jfr`.

**Solution**:
```bash
jcmd $(pgrep -f "spring-boot") JFR.start name=health-check duration=30s filename=/tmp/health.jfr settings=profile
```
</details>

### Exercise 2: Identify Allocation Hotspot from Code Snippet (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: The following method is identified as an allocation hotspot in a flamegraph. Refactor it to eliminate heap allocations.

```java
// Original Hotspot Code
public String formatOrder(Long id, BigDecimal amount) {
    // Allocates StringBuilders, String objects, and Object arrays on every call!
    return String.format("ORDER-%08d: $%.2f", id, amount);
}
```

**Solution**:
```java
// Optimized Zero-Allocation / Low-Allocation Code
public String formatOrder(Long id, BigDecimal amount) {
    // StringBuilder with pre-allocated capacity avoids intermediate formatting objects
    StringBuilder sb = new StringBuilder(32);
    sb.append("ORDER-");
    String idStr = id.toString();
    for (int i = 0; i < 8 - idStr.length(); i++) {
        sb.append('0');
    }
    sb.append(idStr).append(": $").append(amount.toPlainString());
    return sb.toString();
}
```
</details>

### Exercise 3: Automated Profile Capture Script on High CPU Alert (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Author a bash monitoring daemon that watches process CPU usage every 5 seconds. If CPU exceeds 85%, it captures a 20-second async-profiler CPU flamegraph and exits.

**Solution**:
```bash
#!/bin/bash
PID=$(pgrep -f "spring-boot")
THRESHOLD=85

echo "Monitoring PID $PID for CPU spikes > $THRESHOLD%..."

while true; do
    CPU=$(top -b -n 1 -p $PID | tail -1 | awk '{print int($9)}')
    if [ "$CPU" -gt "$THRESHOLD" ]; then
        echo "ALERT: CPU at $CPU%. Capturing 20s async-profiler flamegraph..."
        TIMESTAMP=$(date +%s)
        /opt/async-profiler/bin/asprof -d 20 -f "/var/log/dumps/cpu_spike_${TIMESTAMP}.html" $PID
        echo "Profile captured to /var/log/dumps/cpu_spike_${TIMESTAMP}.html"
        break
    fi
    sleep 5
done
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Profiling Tool / Command                    | Purpose & Production Rule                                   |
+---------------------------------------------+-------------------------------------------------------------+
| jcmd <PID> JFR.start duration=60s           | Captures holistic JVM events with <1% overhead              |
| asprof -d 30 -f cpu.html <PID>              | Generates unbiased interactive CPU Flamegraph               |
| asprof -d 30 -e alloc -f alloc.html <PID>   | Pinpoints memory allocation hotspots causing GC churn       |
| asprof -d 30 -e wall -f wall.html <PID>     | Identifies off-CPU blocking states (network/DB/lock waits)  |
| Safepoint Bias                              | Skew in legacy profilers; eliminated by AsyncGetCallTrace   |
| Flamegraph Interpretation                   | Target wide flat plateaus at the top of the graph first     |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- async-profiler Official GitHub Repository & Documentation
- Brendan Gregg: Flame Graphs Visualization Specification
- Oracle JDK Mission Control (JMC) User Guide
- SRE Performance Diagnostics: Continuous Profiling in Production