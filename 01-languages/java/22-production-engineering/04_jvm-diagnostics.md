# JVM Diagnostics and Tooling

> Authoritative CLI diagnostic toolchain for live production JVM troubleshooting: `jcmd`, `jstack`, `jmap`, `jstat`, Native Memory Tracking (NMT), crash log (`hs_err_pid`) dissection, and thread dump analysis.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Enterprise Production |
| **Prerequisites** | [15_jvm-internals](../15-jvm-internals/README.md), [14_concurrency](../14-concurrency/README.md), [20_profiling](../20-performance/00_profiling.md) |
| **Time to Read** | 35 minutes |
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

### What are JVM Diagnostics?
JVM diagnostics are the non-invasive operational commands, tools, and protocols used to inspect, monitor, diagnose, and extract runtime telemetry from live Java Virtual Machines in staging and production environments.

When an incident occurs in production—such as an unexpected CPU spike to 100%, an impending `OutOfMemoryError`, or an unresponsive service—engineers cannot attach an interactive graphical debugger or restart the instance without destroying critical volatile forensic evidence.

The JDK provides a built-in diagnostic toolchain that operates directly on live processes without requiring restarts:
- **`jcmd`**: The universal Swiss Army knife command tool for HotSpot JVM management (dumps threads, triggers GCs, inspects NMT, controls JFR).
- **`jstack`**: Extracts thread dumps and detects Java-level deadlocks.
- **`jmap`**: Inspects object histograms and generates heap dumps (`.hprof`).
- **`jstat`**: Displays real-time sampling of garbage collection capacity and generation transitions.
- **`hs_err_pid`**: JVM crash dump logs generated during fatal JVM native crashes.

---

## 2. Core Concepts

### The Universal Command: `jcmd`
Historically, Oracle JDK shipped individual tools (`jstack`, `jinfo`, `jmap`). Since Java 8+, Oracle recommends `jcmd` as the unified tool. It communicates directly with the HotSpot JVM process via Unix domain sockets (or named pipes on Windows), ensuring zero JVM code bloat.

### Safepoints
Many diagnostic commands (thread dumps, heap dumps, class redefinitions) require the JVM to bring all application threads to a **Safepoint**—a state where all thread execution is suspended at predictable bytecode boundaries.
- Inspecting a frozen JVM or a JVM stuck in uncounted loops can cause diagnostic commands to hang until threads reach safepoints.

### Native Memory Tracking (NMT)
The JVM consumes significantly more OS memory than just the Java Heap (`-Xmx`):
- Metaspace (class metadata)
- Thread Stacks (default 1 MB per platform thread)
- JIT Code Cache
- Garbage Collector internal accounting tables and card tables
- Direct ByteBuffers and off-heap memory
NMT (`-XX:NativeMemoryTracking=summary` or `detail`) monitors and categorizes every byte of native off-heap memory allocated by HotSpot.

---

## 3. How It Works Internally

### The HotSpot Attach API Mechanism
When you execute `jcmd <PID> Thread.print`:

```
+--------------------------------------------------------------------------+
|                       HOTSPOT ATTACH API ARCHITECTURE                    |
|                                                                          |
| CLI Command: jcmd 1024 Thread.print                                      |
|    |                                                                     |
|    v                                                                     |
| 1. Creates socket file: /tmp/.java_pid1024                               |
| 2. Sends OS signal: SIGQUIT (kill -3 1024)                               |
|    |                                                                     |
|    v                                                                     |
| Target JVM Process (PID 1024)                                            |
|    |                                                                     |
|    +--> Signal Dispatcher Thread receives SIGQUIT                        |
|    +--> Spawns or wakes up "Attach Listener" Thread                      |
|    +--> Reads command from /tmp/.java_pid1024                            |
|    +--> Executes internal command at Safepoint                           |
|    +--> Streams output back through socket pipe                          |
+--------------------------------------------------------------------------+
```

Because communication occurs via a local socket owned by the user running the process, `jcmd` requires identical OS user privileges as the target JVM.

---

## 4. Syntax & Usage

### 1. Essential `jcmd` Commands
```bash
# List all running Java processes
jcmd

# List all diagnostic commands supported by target JVM
jcmd <PID> help

# Capture thread dump
jcmd <PID> Thread.print > threaddump.txt

# Inspect live class histogram (count and size per class)
jcmd <PID> GC.class_histogram -all > histogram.txt

# Trigger an on-demand Java Flight Recording
jcmd <PID> JFR.start name=incident duration=60s filename=incident.jfr

# Inspect Native Memory Tracking summary
jcmd <PID> VM.native_memory summary
```

### 2. Real-Time GC Monitoring with `jstat`
```bash
# Monitor GC sampling every 1000ms
jstat -gcutil <PID> 1000
```
Columns outputted:
- `S0`, `S1`: Survivor 0 and Survivor 1 utilization (%)
- `E`: Eden generation utilization (%)
- `O`: Old generation utilization (%)
- `M`: Metaspace utilization (%)
- `YGC` / `YGCT`: Young Gen GC count and total time
- `FGC` / `FGCT`: Full GC count and total time
- `GCT`: Cumulative total GC pause time

---

## 5. Practical Examples

### Example 1: Correlating High CPU Threads to Thread Dumps
**Goal:** A Linux server reports 100% CPU on Java PID 4096. Identify the exact Java method and line number causing the CPU spike.

```bash
# Step 1: Find the specific Linux native thread ID (LWP) consuming CPU
top -H -p 4096 -b -n 1

# Output shows:
# PID   USER  PR  NI  VIRT   RES   SHR S  %CPU %MEM   TIME+  COMMAND
# 4120  app   20   0 4120m 2.1g 18m R  98.5 26.2 12:44.11 java

# Step 2: Convert the native thread ID (4120) to hexadecimal
printf "0x%x
" 4120
# Output: 0x1018

# Step 3: Capture thread dump and grep for nid=0x1018
jcmd 4096 Thread.print | grep -A 25 "nid=0x1018"

# Output reveals exact culprit:
# "Worker-Thread-3" #42 prio=5 os_prio=0 cpu=764210.12ms nid=0x1018 runnable [0x00007f3b89]
#    java.lang.Thread.State: RUNNABLE
#       at com.bank.service.ReportGenerator.calculateVariance(ReportGenerator.java:142)
#       at com.bank.service.ReportGenerator.processLoop(ReportGenerator.java:88)
```

### Example 2: Tracking Off-Heap Native Memory Leaks (NMT)
**Goal:** The container is killed by Linux cgroup OOM, but the Java heap (`-Xmx`) is only 40% full. Diagnose off-heap native memory growth.

```bash
# Step 1: Ensure JVM was launched with Native Memory Tracking enabled:
# java -XX:NativeMemoryTracking=summary -jar app.jar

# Step 2: Establish a baseline
jcmd <PID> VM.native_memory baseline

# Step 3: Wait 30 minutes, then check memory growth diff against baseline
jcmd <PID> VM.native_memory detail.diff

# Output:
# Total: reserved=4289MB (+240MB), committed=2840MB (+180MB)
# - Java Heap (reserved=2048MB, committed=2048MB)
# - Class (reserved=1080MB, committed=80MB (+5MB))
# - Thread (reserved=250MB, committed=250MB (+120MB)) <--- 120 NEW PLATFORM THREADS LEAKED!
# - Internal (reserved=120MB, committed=120MB (+55MB)) <--- DirectByteBuffers leaked!
```

### Example 3: Parsing Fatal Crash Logs (`hs_err_pid.log`)
**Goal:** The JVM terminated abruptly without an `OutOfMemoryError` or Java exception. Extract root cause from `hs_err_pid<PID>.log`.

Key sections to examine:
```
#
# A fatal error has been detected by the Java Runtime Environment:
#
#  SIGSEGV (0xb) at pc=0x00007f9c2a123456, pid=1234, tid=1250
#
# JRE version: OpenJDK Runtime Environment (21.0.1+12)
# Java VM: OpenJDK 64-Bit Server VM (21.0.1+12, mixed mode, tiered, compressed oops, g1 gc)
# Problematic frame:
# C  [libcrypto.so.3+0x123456]  AES_encrypt+0x16   <--- NATIVE C LIBRARY CRASH!
#
# Current thread (0x00007f9c10008800):  JavaThread "crypto-worker" [_thread_in_native, id=1250]
#
# Stack: [0x00007f9c30000000,0x00007f9c30100000],  sp=0x00007f9c300ff800,  free space=1022k
# Native frames: (J=compiled Java code, j=interpreted, Vv=VM code, C=native code)
# C  [libcrypto.so.3+0x123456]
# J 1205 c2 com.company.crypto.NativeAes.encrypt([B)[B (42 bytes)
```
- **SIGSEGV in Native Library**: Root cause is a segfault inside `libcrypto.so` JNI binding, not Java bytecode.
- **`_thread_in_native`**: Confirms crash occurred while executing native C code.

### Example 4: Generating Heap Dumps Safely
```bash
# Capture heap dump without triggering Full GC first
jcmd <PID> GC.heap_dump -all /tmp/heapdump.hprof

# Capture heap dump ONLY for live reachable objects (triggers Full GC before dump)
jcmd <PID> GC.heap_dump /tmp/heapdump_live.hprof
```

---

## 6. Best Practices

1. **Always Enable Native Memory Tracking in Staging**: Run `-XX:NativeMemoryTracking=summary` to identify off-heap leaks before production deployment.
2. **Collect 3 Consecutive Thread Dumps**: When troubleshooting deadlocks or high CPU, capture 3 thread dumps separated by 10 seconds to detect whether threads are moving or permanently blocked.
3. **Check Hex Thread IDs (`nid`)**: Linux `top -H` threads map 1-to-1 with thread dump `nid` attributes in hexadecimal format.
4. **Use `-XX:+HeapDumpOnOutOfMemoryError` Carefully**: If enabled, specify `-XX:HeapDumpPath=/var/log/dumps/` to write dumps to large attached volumes rather than root container filesystems.
5. **Prefer `jcmd` Over Legacy Tools**: Avoid calling legacy `jstack` or `jmap` directly; `jcmd` is maintained, safer, and supports dynamic flight recording.
6. **Beware of Safepoint Bias**: Remember that heap dumps and thread dumps force safepoints, which temporarily pauses all application traffic.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Running `jcmd` as a different OS user** | Fails with `Unable to open socket file: target process not responding`. | Run `sudo -u <jvm_user> jcmd <PID> ...`. |
| **Taking live heap dump (`GC.heap_dump`) under heavy load** | Live dump triggers an immediate Full GC pause lasting 10–60 seconds. | Use `-all` flag or take instance out of load balancer rotation first. |
| **Ignoring hex conversion for thread IDs** | Searching decimal PID in thread dump yields zero matches (`nid` is hex). | Convert Linux LWP thread ID to hex: `printf "0x%x
" <TID>`. |
| **Filling container root disk with heap dumps** | Heap dump size equals allocated RAM; fills container ephemeral disk, crashing pod. | Mount dedicated persistent volume for heap dumps. |
| **Assuming heap is the only memory** | Container killed by Linux OOM killer even though heap is small. | Use NMT to inspect Metaspace, Stacks, Direct ByteBuffers. |
| **Debugging with JDWP in production** | JDWP freezes JVM threads, exposes plaintext variables, and has no auth. | Use JFR and `jcmd` low-overhead diagnostics. |

---

## 8. Performance & Security Notes

### Overhead of `jcmd` Operations
- `jcmd <PID> Thread.print`: Very low overhead (~5–20 ms pause to reach safepoint). Safe for production.
- `jcmd <PID> GC.class_histogram`: Pauses JVM briefly to scan heap references (~200–800 ms).
- `jcmd <PID> GC.heap_dump`: High overhead. Serializes entire gigabytes of RAM to disk. Freezes JVM during write. **Only run on isolated instances removed from traffic.**

---

## 9. Comparison / When to Use What

| Diagnostic Tool | Primary Focus | Runtime Overhead | Ideal Scenario |
| :--- | :--- | :--- | :--- |
| **`jcmd Thread.print`** | Thread states, locks, call stacks | Minimal ($< 20\text{ms}$) | High CPU, thread starvation, deadlocks |
| **`jstat -gcutil`** | Generation utilization & GC pause rates | Near Zero ($< 0.1\%$) | Monitoring GC frequency and memory leaks in real time |
| **`jcmd VM.native_memory`**| Off-heap Metaspace/Stack/Direct memory | Minimal ($< 1\%$) | Diagnosing container cgroup OOM kills |
| **`jcmd GC.heap_dump`** | Full object graph analysis | High (Freezes JVM) | Post-mortem root cause of memory leaks |
| **Java Flight Recorder (JFR)**| Low-overhead event telemetry | Very Low ($< 1\%$) | Production latency profiling and allocation hotspots |

---

## 10. Interview Questions & Answers

### Q1: How do you map a high-CPU thread identified by the Linux `top` utility to a specific Java thread in a thread dump?
<details>
<summary>View Answer</summary>
Run `top -H -p <PID>` to display individual Linux native threads (LWPs) belonging to the JVM process and identify the PID of the specific thread consuming the highest CPU percentage. Convert that decimal thread ID into hexadecimal using `printf "0x%x
" <TID>`. Then generate a thread dump using `jcmd <PID> Thread.print` and search for the hexadecimal value matching the `nid` (native ID) parameter in the thread headers (e.g. `nid=0x1a2b`). The matching stack trace reveals the exact Java class, method, and source code line currently executing.
</details>

### Q2: Why does `jcmd` fail when invoked by a user other than the process owner?
<details>
<summary>View Answer</summary>
HotSpot\'s Attach API uses local OS domain sockets located in the temporary directory (e.g. `/tmp/.java_pid<PID>`). The socket file is created with file permissions restricting read/write access strictly to the operating system user that started the target JVM. If another user (even a different non-root service account) attempts to run `jcmd`, the operating system denies permission to connect to the socket, producing an access or communication failure error.
</details>

### Q3: What is the difference between `jcmd <PID> GC.heap_dump` with and without the `-all` flag?
<details>
<summary>View Answer</summary>
Without `-all` (default behavior), the JVM forces an immediate Stop-The-World Full Garbage Collection before writing the heap dump. The resulting dump contains only "live" (strongly reachable) objects, making memory leak analysis faster because garbage is eliminated. With `-all`, the JVM skips the preliminary garbage collection and dumps every allocated object on the heap, including unreachable objects pending collection.
</details>

### Q4: What is Native Memory Tracking (NMT) and why is it critical in Kubernetes environments?
<details>
<summary>View Answer</summary>
NMT is a HotSpot feature (`-XX:NativeMemoryTracking=summary`) that tracks off-heap native memory allocations performed by the JVM internals. In Kubernetes, container memory limits are enforced at the OS cgroup level. An application with `-Xmx2g` can easily consume 3.5 GB of total RSS memory due to Metaspace, thousands of platform thread stacks (1 MB each), JIT Code Cache, and Direct ByteBuffers allocated by Netty. When total memory exceeds the pod limit, Linux sends `SIGKILL` (OOMKilled). NMT allows engineers to determine exactly which non-heap subsystem caused the memory breach.
</details>

### Q5: What information does an `hs_err_pid.log` file contain?
<details>
<summary>View Answer</summary>
An `hs_err_pid.log` is a fatal crash log generated by HotSpot when an unrecoverable native error occurs (such as a SIGSEGV or internal JVM assertion failure). It contains: (1) the triggering signal and memory address, (2) the problematic native library frame, (3) the active thread state and native call stack, (4) Java registers and memory map, (5) full command-line arguments and JVM flags, and (6) host operating system and hardware resource statistics.
</details>

### Q6: What is a Safepoint and how can a long safepoint pause affect diagnostics?
<details>
<summary>View Answer</summary>
A safepoint is a state where HotSpot pauses all application threads so that the JVM can perform operations that require memory consistency (Stop-The-World GC, thread dumps, deoptimization). Threads check for safepoints at loop branches and method exits. If a thread is stuck in an uncounted loop without safepoint polls or stalled in a native JNI call, it delays the entire JVM from reaching the safepoint ("safepoint time-to-stop"). Any diagnostic command (like `jcmd Thread.print`) will hang until all threads reach the safepoint.
</details>

### Q7: How does `jstat -gcutil` help detect a memory leak before an OutOfMemoryError occurs?
<details>
<summary>View Answer</summary>
`jstat -gcutil <PID> 1000` samples generation utilization percentages every second. In a healthy application, the Old Generation percentage (`O`) rises and then drops sharply back to a baseline level following a Full GC. In an application with a memory leak, the Old Generation percentage monotonically climbs toward 100% and remains at 99%+ even after Full GC count (`FGC`) increments repeatedly. When `O` remains pegged at 99% and `FGCT` time climbs rapidly, an `OutOfMemoryError` is imminent.
</details>

### Q8: Why should you avoid attaching a remote debugger (`jdwp`) to a production JVM?
<details>
<summary>View Answer</summary>
Attaching a debugger via JDWP disables critical JIT compiler optimizations (forcing methods to run in interpreted or tier-1 mode), significantly slows execution throughput, and risks accidental thread freezing if a breakpoint is hit. Furthermore, JDWP has no native authentication or encryption; leaving debug ports exposed allows remote attackers to execute arbitrary code or inspect sensitive user credentials in memory.
</details>

---

## 11. Practice Exercises

### Exercise 1: Automate Thread Dump Capture Script
**Task:** Write a shell script that captures 3 thread dumps at 5-second intervals and automatically compresses them with the current timestamp.
<details>
<summary>View Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

PID="${1:-}"
if [ -z "${PID}" ]; then
    echo "Usage: $0 <JVM_PID>"
    exit 1
fi

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
DUMP_DIR="dumps_${TIMESTAMP}"
mkdir -p "${DUMP_DIR}"

echo "Capturing 3 thread dumps for PID ${PID}..."
for i in 1 2 3; do
    echo "Capturing dump ${i}/3..."
    jcmd "${PID}" Thread.print > "${DUMP_DIR}/threaddump_${i}.txt"
    if [ $i -lt 3 ]; then
        sleep 5
    fi
done

tar -czf "${DUMP_DIR}.tar.gz" "${DUMP_DIR}"
rm -rf "${DUMP_DIR}"
echo "Saved archive: ${DUMP_DIR}.tar.gz"
```
</details>

### Exercise 2: Script to Convert Top LWP to Thread Dump Match
**Task:** Write a one-liner that takes a decimal thread ID and prints the corresponding grep command for `jcmd`.
<details>
<summary>View Solution</summary>

```bash
# Bash function:
find_thread() {
    HEX=$(printf "0x%x" "$1")
    echo "Searching for nid=${HEX} in thread dump..."
    grep -A 20 "nid=${HEX}" threaddump.txt
}
```
</details>

### Exercise 3: Automated Heap Histogram Diff Utility
**Task:** Capture class histograms 10 seconds apart and compare instance count deltas to detect rapid object accumulation.
<details>
<summary>View Solution</summary>

```bash
#!/usr/bin/env bash
PID="$1"
jcmd "$PID" GC.class_histogram | head -n 25 > histo_1.txt
echo "Waiting 10 seconds..."
sleep 10
jcmd "$PID" GC.class_histogram | head -n 25 > histo_2.txt
diff -u histo_1.txt histo_2.txt || true
```
</details>

---

## 12. Summary / Cheat Sheet

- **Universal CLI**: Use `jcmd <PID> <command>` for thread dumps, histograms, and JFR.
- **High CPU Triage**: Use `top -H -p <PID>`, convert LWP to hex, and grep `nid=0xHEX` in thread dumps.
- **Off-Heap Tracking**: Launch with `-XX:NativeMemoryTracking=summary` and inspect with `jcmd <PID> VM.native_memory detail.diff`.
- **GC Telemetry**: Monitor generation trends with `jstat -gcutil <PID> 1000`.
- **Crash Dumps**: Inspect `hs_err_pid<PID>.log` to identify problematic native frames and signals.
- **Heap Dumps**: Never dump heap on live production pods under user traffic; isolate first.

---

## 13. Further Reading
- OpenJDK Diagnostic Commands: [jcmd Reference Manual](https://docs.oracle.com/en/java/javase/21/docs/specs/man/jcmd.html)
- Oracle JVM Troubleshooting Guide: [Troubleshooting Guide for Java SE](https://docs.oracle.com/en/java/javase/21/troubleshoot/)
- Native Memory Tracking Guide: [Native Memory Tracking](https://docs.oracle.com/en/java/javase/21/vm/native-memory-tracking.html)
- Java Flight Recorder: [JFR Command Line Reference](https://docs.oracle.com/en/java/javase/21/jfapi/flight-recorder.html)
