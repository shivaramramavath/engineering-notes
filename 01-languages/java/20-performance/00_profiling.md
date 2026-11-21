# JVM Profiling: CPU, Memory, and Contention Diagnostics

> Low-overhead production diagnostics: async-profiler, JDK Flight Recorder (JFR), JDK Mission Control (JMC), and Flame Graph Analysis.

| Metadata | Details |
|---|---|
| **Level** | Core / Advanced |
| **Prerequisites** | JVM Architecture, Bytecode, Linux Perf Basics |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: Safepoint Bias vs Perf Events](#3-how-it-works-internally-safepoint-bias-vs-perf-events)
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
In distributed enterprise architectures, performance bottlenecks rarely manifest as simple algorithmic bugs. Instead, they appear as subtle latency spikes: thread pool saturation, unexpected garbage collection pauses, lock contention, or CPU cache misses.

Attempting to diagnose performance issues by guessing or instrumenting code with log statements alters execution timings (the Observer Effect) and often degrades production throughput further. **Profiling** is the systematic measurement of a live JVM's execution characteristics: CPU utilization, object allocation rates, lock wait durations, and I/O latency. Modern tools like **async-profiler** and **JDK Flight Recorder (JFR)** operate with less than 1% overhead, making continuous production profiling safe and actionable.

---

## 2. Core Concepts
- **Sampling vs Instrumentation**:
  - *Instrumentation*: Injects bytecode probes into every method entry and exit. Incurs massive overhead (up to 1000%), altering execution order and distorting JIT inlining.
  - *Sampling*: Periodically captures the stack traces of running threads at fixed intervals (e.g., every 10 ms). Low overhead (< 1-2%), suitable for production.
- **Safepoint Bias Problem**: Traditional Java profilers only sample threads when they reach a JVM Safepoint (such as loop ends, allocations, or method exits). Tight loops without safepoint polls are completely invisible, producing wildly inaccurate CPU profiles.
- **async-profiler**: An open-source low-overhead sampling profiler for Linux and macOS that bypasses the Safepoint Bias problem by combining HotSpot's `AsyncGetCallTrace` API with Linux hardware performance counters (`perf_events`).
- **JDK Flight Recorder (JFR)**: A high-performance diagnostic event recorder built directly into the HotSpot JVM kernel. Captures execution samples, GC pauses, lock contention, and socket I/O with near-zero overhead.
- **Flame Graph**: A hierarchical visualization invented by Brendan Gregg showing stack trace profiles, where the x-axis represents the population percentage of samples and the y-axis shows stack depth.
- **Wall-Clock Profiling**: Samples threads regardless of whether they are executing on CPU or blocked in I/O/lock waiting, crucial for identifying latency bottlenecks in microservices.

---

## 3. How It Works Internally: Safepoint Bias vs Perf Events

Understanding why traditional profilers lie and how modern async-profiler works:

```
Traditional JVM Profiler (Safepoint Biased):
Thread Running Tight Loop --------> [No Safepoint] -------> [No Sample Captured!]
Thread Allocates Memory ----------> [Safepoint Poll] ------> [Sample Captured!]
(Result: Garbage Collection & Allocation methods falsely appear as 90% of CPU!)

Modern async-profiler (Hardware Signal Driven):
OS Timer / Hardware Counter Interrupt (perf_event)
            |
            v  Sends POSIX signal (SIGPROF) directly to CPU core
Target Thread Interrupted Instantly (Any instruction: tight loops, JIT code)
            |
            v  Calls AsyncGetCallTrace to walk JVM frames without a safepoint
Accurate Stack Trace Recorded into Lock-Free Ring Buffer
```

### The AsyncGetCallTrace Mechanism:
When `perf_events` fires a timer interrupt on Linux, it sends a POSIX signal (`SIGPROF`) to the target hardware thread. The signal handler invokes `AsyncGetCallTrace`, an internal HotSpot JVM function. It inspects the Program Counter (`PC`), Frame Pointer (`FP`), and Stack Pointer (`SP`) directly to walk the mixed Java and native C++ call stack safely without waiting for the JVM thread to reach a designated safepoint.

---

## 4. Syntax & Usage

### 4.1 Capturing CPU Flame Graphs with async-profiler
```bash
# 1. Download and extract async-profiler
tar -xzf async-profiler-3.0-linux-x64.tar.gz

# 2. Capture a 60-second CPU profile and output an interactive SVG Flame Graph
./asprof -d 60 -f /tmp/flamegraph_cpu.html <JVM_PID>

# 3. Capture Wall-Clock profile (diagnoses threads waiting on I/O, DB, or Locks)
./asprof -e wall -d 30 -f /tmp/flamegraph_wall.html <JVM_PID>

# 4. Capture Memory Allocation profile (identifies exact methods allocating heap memory)
./asprof -e alloc -d 45 -f /tmp/flamegraph_alloc.html <JVM_PID>
```

### 4.2 Controlling JDK Flight Recorder via jcmd
```bash
# Check if JFR is available and list active recordings
jcmd <JVM_PID> JFR.check

# Start a 60-second continuous recording with the default profile settings
jcmd <JVM_PID> JFR.start name=prod_profile settings=profile duration=60s filename=/var/log/jvm/flight_record.jfr

# Dump active in-flight recording immediately during an incident
jcmd <JVM_PID> JFR.dump name=prod_profile filename=/var/log/jvm/incident_dump.jfr

# Stop and discard recording
jcmd <JVM_PID> JFR.stop name=prod_profile
```

---

## 5. Practical Examples

### Example 1: Starting Continuous JFR Recording at JVM Bootstrap
```bash
# Add to JVM startup arguments in production container or systemd service:
java -XX:+FlightRecorder      -XX:StartFlightRecording=disk=true,dumponexit=true,filename=/var/log/jvm/crash_recording.jfr,settings=profile      -XX:FlightRecorderOptions=stackdepth=128      -jar target/order-service.jar
```
**Explanation**: This automatically initiates flight recording at JVM launch. If the JVM terminates, encounters an OOM, or crashes, it automatically dumps the flight recording to disk, preserving the last few minutes of CPU samples and thread activity for forensic inspection.

### Example 2: Analyzing Lock Contention Events in Java 21+ JFR
```java
package com.example.performance;

import jdk.jfr.Configuration;
import jdk.jfr.Recording;
import java.nio.file.Path;
import java.time.Duration;

public final class ProgrammaticJfrCapture {
    public static void recordDuringHeavyLoad(Runnable workload) throws Exception {
        Configuration config = Configuration.getConfiguration("profile");
        try (Recording recording = new Recording(config)) {
            // Enable lock contention tracking with 10ms threshold
            recording.enable("jdk.JavaMonitorEnter").withThreshold(Duration.ofMillis(10));
            recording.enable("jdk.ThreadSleep").withThreshold(Duration.ofMillis(20));

            recording.start();
            System.out.println("JFR recording started...");

            workload.run();

            recording.stop();
            recording.dump(Path.of("/tmp/workload_profile.jfr"));
            System.out.println("JFR recording dumped successfully.");
        }
    }
}
```

### Example 3: Edge Case - Diagnosing Native Memory Leaks via async-profiler
```bash
# Profile native C/C++ memory allocations (malloc) in third-party JNI or FFM libraries
./asprof -e malloc -d 30 -f /tmp/native_alloc_flame.html <JVM_PID>
```

---

## 6. Best Practices
1. **Always Use Sampling Profilers in Production**: Never deploy instrumentation-based profilers (like older versions of JProfiler or VisualVM CPU sampler without async agent) to production.
2. **Profile Wall-Clock Time When Debugging Microservice Latency**: CPU profiling only shows active on-CPU compute; wall-clock profiling identifies threads blocked waiting for HTTP calls, database pools, or locks.
3. **Inspect the Width of Flame Graph Plateaus**: In a Flame Graph, the widest horizontal boxes represent the methods consuming the largest percentage of CPU cycles; focus refactoring efforts strictly on these wide plateaus.
4. **Keep Stack Depth Configured to at least 128**: Deep enterprise frameworks (Spring, Hibernate, Jackson) easily exceed default stack depths; set `-XX:FlightRecorderOptions=stackdepth=128`.
5. **Continuous JFR Ring Buffers in Production**: Run JFR continuously with a rolling 2-hour circular buffer; when an outage occurs, dump the recording to capture pre-incident metrics.
6. **Correlate Allocation Profiles with GC Frequency**: If young generation GC runs every 2 seconds, capture an allocation profile (`-e alloc`) to pinpoint the exact classes generating short-lived heap bloat.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Trusting standard JVM safepoint sampling | Distorts profiles due to Safepoint Bias; misses tight loops | Use `async-profiler` or native JFR |
| Profiling CPU when latency is caused by I/O | CPU profile shows 95% idle, missing blocked threads | Capture Wall-clock (`-e wall`) profile |
| Running heavy instrumentation profilers in production | Incurs 500%+ CPU overhead, triggering severe production crashes | Use low-overhead sampling profilers (< 1% overhead) |
| Reading Flame Graph x-axis as chronological time | X-axis is sorted alphabetically to aggregate identical stacks | Understand that width represents sample count, not timeline |
| Discarding JFR recordings after process exit | Misses root-cause events preceding an OOM or fatal crash | Enable `dumponexit=true` on JFR startup |
| Forgetting to configure OS permissions for perf | Linux perf_events blocks non-root profiling | Set `sysctl -w kernel.perf_event_paranoid=1` |
| Profiling un-warmed JVM applications | Profiles JIT compilation and class loading rather than steady state | Warm up application with load before capturing profile |
| Optimizing thin spikes in Flame Graphs | Narrow spikes represent tiny fractions of execution time | Focus optimization strictly on the widest flat plateaus |

---

## 8. Performance & Security Notes
- **Linux perf_events Security Restrictions**: On modern Linux kernels, non-root users cannot access hardware performance counters unless `kernel.perf_event_paranoid` is configured to `1` or `0`, or `CAP_SYS_ADMIN` / `CAP_PERFMON` is granted to the container.
- **Flight Recorder Buffer Overhead**: JFR writes events to small in-memory thread-local buffers (typically 32 KB per thread) before asynchronously flushing them to disk. Total memory consumption rarely exceeds 15 MB.
- **Flame Graph Interpretation**: The total width of a Flame Graph represents 100% of collected samples. Top-level horizontal plateaus that have no bars on top of them are "leaf methods" consuming raw CPU time directly inside their own instructions.

---

## 9. Comparison / When to Use What

| Profiling Tool | Mechanism | Overhead | Best Use Case |
|---|---|---|---|
| **async-profiler** | `perf_events` + AsyncGetCallTrace | < 1% | CPU hotspots, allocation profiling, wall-clock, native memory |
| **JDK Flight Recorder (JFR)** | In-kernel JVM events | < 1% | Continuous production monitoring, lock contention, GC pauses |
| **VisualVM / JConsole** | JMX / JVMTI | Moderate | Local development inspection, heap dump triggering |
| **Linux `perf`** | OS Kernel hardware counters | Minimal | Kernel time, cache misses, branch mispredictions, assembly |

---

## 10. Interview Questions

<details>
<summary>1. What is the Safepoint Bias problem in Java profiling and how does async-profiler overcome it?</summary>
Traditional Java profilers request thread stack traces through the JVM Tool Interface (JVMTI) <code>GetAllStackTraces()</code>. The JVM can only stop and inspect a thread when it reaches a pre-compiled Safepoint (such as loop ends, method invocations, or allocation checks). Consequently, tight computational loops that contain no safepoints cannot be sampled, while methods containing allocation checks are sampled disproportionately, leading to highly biased, inaccurate CPU profiles.<br>
<b>async-profiler</b> overcomes this by using Linux OS timers or hardware performance counters (<code>perf_events</code>) to send a POSIX <code>SIGPROF</code> interrupt signal directly to the CPU core. The signal handler catches the thread at the exact machine instruction currently executing and walks the stack using HotSpot's internal <code>AsyncGetCallTrace</code> API without waiting for a safepoint.
</details>

<details>
<summary>2. How do you interpret a Flame Graph generated by async-profiler?</summary>
In a Flame Graph:
- <b>Y-Axis</b> represents call stack depth: the bottom represents the entry point (e.g., <code>Thread.run()</code>), and each level up represents a method call, with the top-most bar representing the executing leaf method.
- <b>X-Axis</b> represents the entire population of samples sorted alphabetically to group identical call stacks together. It does <b>NOT</b> represent chronological time.
- <b>Width</b> of a bar is proportional to the percentage of time that method was present on the stack. The wider the box, the more CPU or wall-clock time was consumed. Flat horizontal plateaus at the top indicate methods consuming raw CPU in their own code.
</details>

<details>
<summary>3. What is the difference between CPU profiling and Wall-Clock profiling?</summary>
<b>CPU Profiling</b> samples threads only when they are actively running on a physical CPU core. It is ideal for locating compute-heavy algorithmic bottlenecks.<br>
<b>Wall-Clock Profiling</b> samples all threads at regular time intervals regardless of thread state: running, waiting for locks, sleeping, or blocked on network/socket I/O. It is indispensable for diagnosing high latency and low throughput in microservices where threads spend 90% of their time waiting for database queries or downstream HTTP endpoints.
</details>

<details>
<summary>4. What is JDK Flight Recorder (JFR) and why is its overhead so low?</summary>
JFR is an event-recording engine integrated directly into the HotSpot JVM kernel. It records JVM-internal events (GC pauses, compiler optimizations, thread synchronization, I/O) directly into tiny thread-local ring buffers using highly optimized binary encoding without entering Java code. Because events are written to memory buffers and flushed asynchronously, its runtime overhead is less than 1%, making it safe to leave enabled permanently in production.
</details>

<details>
<summary>5. How does Allocation Profiling work in async-profiler?</summary>
Instead of tracking every single object allocation (which would destroy performance), async-profiler intercepts the JVM Thread-Local Allocation Buffer (TLAB) allocation path. Whenever a thread exhausts its current TLAB (typically every 512 KB to 2 MB) and requests a new one, or when an object is too large and allocates directly in the tenured generation, async-profiler records a sample. This accurately reflects memory allocation distribution across methods with minimal overhead.
</details>

<details>
<summary>6. How do you diagnose Lock Contention using JFR?</summary>
JFR records the <code>jdk.JavaMonitorEnter</code> event whenever a thread is blocked waiting to acquire a synchronized monitor lock longer than a configured threshold (e.g., 10 ms). The event records the duration of the wait, the exact monitor class, the contending thread, and the thread currently holding the lock, allowing engineers to pinpoint lock contention bottlenecks in JDK Mission Control.
</details>

<details>
<summary>7. What command line flags are required to run async-profiler inside a Docker/Kubernetes container?</summary>
Because async-profiler requires access to Linux kernel tracing subsystems, the container must have appropriate capabilities. In Docker, run with <code>--cap-add SYS_ADMIN</code> or <code>--cap-add PERFMON</code>, or set the host kernel sysctl <code>kernel.perf_event_paranoid=1</code>.
</details>

<details>
<summary>8. How does JDK Mission Control (JMC) complement JFR?</summary>
JFR is the in-JVM recorder that captures event data into <code>.jfr</code> files. JDK Mission Control (JMC) is the desktop analysis GUI application used to inspect and visualize JFR files. It provides automated rule-based analysis, memory leak detection, GC pause breakdowns, thread visualization, and method execution statistics.
</details>

---


<details>
<summary>9. What is the difference between In-Process Profiling and Out-of-Process Profiling?</summary>
In-process profiling loads an agent (like async-profiler via JVMTI) directly into the target JVM process memory space, providing direct access to internal JVM data structures, JIT compiler tables, and call stacks. Out-of-process profiling uses external OS tools (like Linux perf) from outside the JVM, inspecting kernel-level thread execution without any internal JVM symbols unless a symbol map (perf-PID.map) is explicitly exported by HotSpot.
</details>

<details>
<summary>10. What is Safepoint latency and how do you diagnose it with JFR?</summary>
Safepoint latency is the time it takes from when the JVM requests a safepoint (e.g., to perform a GC pause or revoke a biased lock) until all running application threads actually halt. If a thread is stuck in an uncounted loop without safepoint checks, all other threads stall waiting for it. JFR records <code>jdk.SafepointBegin</code> and <code>jdk.SafepointWaitBlocked</code> events to pinpoint which specific thread delayed the safepoint.
</details>

## 11. Practice Exercises

<details>
<summary>Exercise 1: Write a shell script that captures a CPU profile and an allocation profile sequentially during a load test.</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

PID=$(pgrep -f "order-service.jar")
echo "Profiling Java PID: $PID"

# Capture 30s CPU profile
./asprof -d 30 -f /tmp/cpu_profile.html "$PID"
echo "CPU profile saved to /tmp/cpu_profile.html"

# Capture 30s Memory Allocation profile
./asprof -e alloc -d 30 -f /tmp/alloc_profile.html "$PID"
echo "Allocation profile saved to /tmp/alloc_profile.html"
```
</details>

<details>
<summary>Exercise 2: Write a method triggering a JFR dump via Java management APIs.</summary>

```java
import java.lang.management.ManagementFactory;
import javax.management.MBeanServer;
import javax.management.ObjectName;

public final class DiagnosticHelper {
    public static void dumpJfrRecording(String outputPath) throws Exception {
        MBeanServer mbs = ManagementFactory.getPlatformMBeanServer();
        ObjectName diagnosticBean = new ObjectName("com.sun.management:type=DiagnosticCommand");
        
        mbs.invoke(
            diagnosticBean,
            "jfrDump",
            new Object[]{new String[]{"filename=" + outputPath}},
            new String[]{"[Ljava.lang.String;"}
        );
        System.out.println("Dumped JFR to: " + outputPath);
    }
}
```
</details>

<details>
<summary>Exercise 3: Configure an automated JFR event streaming consumer to log long lock contentions.</summary>

```java
import jdk.jfr.consumer.RecordingStream;
import java.time.Duration;

public final class ContentionWatchdog {
    public static void start() {
        Thread.ofVirtual().start(() -> {
            try (var rs = new RecordingStream()) {
                rs.enable("jdk.JavaMonitorEnter").withThreshold(Duration.ofMillis(20));
                rs.onEvent("jdk.JavaMonitorEnter", event -> {
                    System.err.printf("[ALERT] Lock Contention: %s blocked for %d ms waiting for monitor %s held by %s%n",
                            event.getThread().getJavaName(),
                            event.getDuration().toMillis(),
                            event.getClass("monitorClass").getName(),
                            event.getThread("previousOwner") != null ? event.getThread("previousOwner").getJavaName() : "unknown");
                });
                rs.start();
            }
        });
    }
}
```
</details>

---


### 11.4 In-Depth Production Triage Playbook: Profiling Live Incidents

When a high-severity alert fires in production for high latency or CPU saturation, follow this disciplined forensic workflow:

1. **Step 1: Check System Metrics First**:
   Verify host-level resource constraints before touching the JVM:
   ```bash
   uptime
   top -H -p <PID>
   vmstat 1 5
   ```
   Determine whether the saturation is caused by user-space compute (`%usr`), kernel system calls (`%sys`), disk I/O wait (`%iowait`), or hypervisor CPU throttling (`%steal`).

2. **Step 2: Collect Concurrent Thread Dumps**:
   Capture three consecutive thread dumps spaced 5 seconds apart:
   ```bash
   for i in 1 2 3; do
       jcmd <PID> Thread.print > /tmp/threaddump_$i.txt
       sleep 5
   done
   ```
   Identify threads in `RUNNABLE` state vs those stuck in `BLOCKED` or `WAITING` on database connections or locks.

3. **Step 3: Trigger Non-Invasive async-profiler Capture**:
   Execute a 30-second mixed CPU and Wall-clock profile:
   ```bash
   ./asprof -e cpu,wall -d 30 -f /tmp/incident_profile.html <PID>
   ```
   Inspect the Flame Graph immediately:
   - If wide plateaus show GC worker threads (`G1 ParNew`, `ZWorker`), the issue is heap exhaustion and memory churn.
   - If wide plateaus show JSON parsing (`com.fasterxml.jackson`), optimize payload serialization or streaming.
   - If wide plateaus show lock synchronizers (`java.util.concurrent.locks`), investigate lock contention and thread pool sizing.

4. **Step 4: Continuous Telemetry via JFR Event Streaming**:
   Ensure application health monitoring frameworks subscribe directly to JVM Flight Recorder events in-process without manual SSH access.



- **Continuous Production Profiling**: Modern platforms deploy continuous profilers (such as Pyroscope, Datadog Continuous Profiler, or Google Cloud Profiler) that record 1-minute sampling profiles every 10 minutes across entire container fleets, enabling forensic comparison between software releases.
- **Flame Scope Analysis**: For investigating periodic latency spikes (e.g., a 2-second freeze occurring every 5 minutes), use Netflix FlameScope. It visualizes sub-second CPU distribution as a heatmap, allowing engineers to generate flame graphs strictly for the duration of the latency spike.
- **Off-CPU Analysis**: Remember that threads waiting for network, disk, or synchronization do not show up on CPU flame graphs. Always combine CPU profiling with Wall-clock or JFR thread wait profiling.


<details>
<summary>Exercise 4: Generate a mixed Flame Graph showing both Java and native C/C++ frames.</summary>

`ash
# Capture mixed profile including C standard library and Linux kernel frames:
./asprof -d 30 --lines -f /tmp/mixed_flame.html <JVM_PID>
# Allows inspecting HotSpot runtime internals alongside application Java methods!
`
</details>

## 12. Summary / Cheat Sheet
- **Methodology**: Always use low-overhead sampling profilers; avoid instrumentation in production.
- **Safepoint Bias**: Avoid traditional profilers; use `async-profiler` with `perf_events` and `AsyncGetCallTrace`.
- **Flame Graphs**: X-axis = sample population percentage; Y-axis = stack depth; target the widest plateaus.
- **Profiling Modes**: CPU (`-e cpu`) for compute; Wall-Clock (`-e wall`) for latency and I/O; Alloc (`-e alloc`) for memory churn.
- **JFR Production Rule**: Enable continuous circular recording (`-XX:StartFlightRecording=dumponexit=true`).
- **Contention**: Trace lock wait durations via `jdk.JavaMonitorEnter` events.

---

## 13. Further Reading
- async-profiler GitHub Repository and Architecture Guide
- Brendan Gregg: Flame Graphs and Performance Visualization
- Oracle JDK Flight Recorder and JDK Mission Control Documentation
- *Java Performance: In-Depth Advice for Tuning and Acceleration* by Scott Oaks
