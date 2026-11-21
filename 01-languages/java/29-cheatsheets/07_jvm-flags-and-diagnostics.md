# JVM Flags, Tuning & Diagnostics Reference Cheat Sheet

> Complete operational quick-reference for HotSpot JVM tuning flags, Garbage Collection options (G1, ZGC, Shenandoah), Unified JVM Logging (-Xlog), JFR continuous profiling, and diagnostic CLI tooling (jcmd, jstat, jstack).

| Metadata | Details |
| :--- | :--- |
| **Level** | Comprehensive Reference |
| **Prerequisites** | [15-jvm-internals](../15-jvm-internals/README.md), [20-performance](../20-performance/README.md) |
| **Time to Read** | 40 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to 25 LTS |

---

## Table of Contents
1. [Memory Sizing Flags](#1-memory-sizing-flags)
2. [Container Awareness & Resource Budgeting](#2-container-awareness--resource-budgeting)
3. [Garbage Collection Selection & Tuning Matrix](#3-garbage-collection-selection--tuning-matrix)
4. [Unified JVM Logging Framework (-Xlog)](#4-unified-jvm-logging-framework--xlog)
5. [JIT Compilation & Inlining Optimization Flags](#5-jit-compilation--inlining-optimization-flags)
6. [Out-of-Memory & Emergency Diagnostic Flags](#6-out-of-memory--emergency-diagnostic-flags)
7. [JDK Diagnostic CLI Tooling Reference](#7-jdk-diagnostic-cli-tooling-reference)
8. [JDK Flight Recorder (JFR) & JMC Commands](#8-jdk-flight-recorder-jfr--jmc-commands)
9. [Native Memory Tracking (NMT) Reference](#9-native-memory-tracking-nmt-reference)
10. [Async-Profiler & FlameGraph Generation](#10-async-profiler--flamegraph-generation)
11. [Production Flags Reference Templates](#11-production-flags-reference-templates)
12. [Common Mistakes & Dangerous Flags](#12-common-mistakes--dangerous-flags)

---

## 1. Memory Sizing Flags

| Flag | Purpose | Default Behavior | Example |
| :--- | :--- | :--- | :--- |
| `-Xms<size>` | Initial Java Heap size | $1/64$ physical RAM | `-Xms4g` |
| `-Xmx<size>` | Maximum Java Heap size | $1/4$ physical RAM | `-Xmx8g` |
| `-Xmn<size>` | Young generation heap size | Dynamic in G1/ZGC | `-Xmn2g` (Parallel GC) |
| `-Xss<size>` | Thread stack size | 1MB (64-bit Linux/x86) | `-Xss512k` |
| `-XX:MetaspaceSize=<size>` | Initial threshold to trigger Metaspace GC | $\approx 21.8\text{MB}$ | `-XX:MetaspaceSize=256m` |
| `-XX:MaxMetaspaceSize=<size>`| Maximum native Metaspace size | Unlimited | `-XX:MaxMetaspaceSize=512m` |
| `-XX:MaxDirectMemorySize=<size>`| Maximum off-heap Direct ByteBuffers | Equal to `-Xmx` | `-XX:MaxDirectMemorySize=2g` |
| `-XX:SurvivorRatio=<ratio>`| Ratio of Eden space to Survivor space | 8 (Eden = 8x Survivor) | `-XX:SurvivorRatio=6` |
| `-XX:NewRatio=<ratio>` | Ratio of Old generation to Young generation | 2 (Old = 2x Young) | `-XX:NewRatio=2` |

```bash
# Recommended production heap sizing idiom: set -Xms equal to -Xmx to prevent heap resizing pauses
java -Xms8g -Xmx8g -XX:MaxMetaspaceSize=512m -jar service.jar
```

---

## 2. Container Awareness & Resource Budgeting

Modern JVMs (Java 11+ and backported to Java 8u191+) are container-aware and read cgroup limits for CPU and memory quota rather than host hardware:

| Flag | Purpose | Recommended Production Value |
| :--- | :--- | :---: |
| `-XX:+UseContainerSupport` | Enables reading Linux cgroups CPU/memory quota | Enabled by default |
| `-XX:MaxRAMPercentage=<pct>` | Sets `-Xmx` as a percentage of total container limit | `75.0` to `80.0` |
| `-XX:InitialRAMPercentage=<pct>`| Sets `-Xms` as a percentage of total container limit | Equal to MaxRAMPercentage |
| `-XX:MinRAMPercentage=<pct>` | Sizing percentage for small memory containers (<250MB) | `50.0` |
| `-XX:ActiveProcessorCount=<N>` | Explicitly overrides CPU core count detected from cgroup | Core count allocated |

```bash
# Optimal Docker / Kubernetes JVM Options
java -XX:+UseContainerSupport \
     -XX:MaxRAMPercentage=75.0 \
     -XX:InitialRAMPercentage=75.0 \
     -jar backend-service.jar
```

---

## 3. Garbage Collection Selection & Tuning Matrix

### Collector Selection Flags
- `-XX:+UseG1GC`: Enables Garbage-First GC (Default for multi-core server JVMs since Java 9).
- `-XX:+UseZGC`: Enables Z Garbage Collector (Sub-millisecond pause times, terabyte heaps).
- `-XX:+ZGenerational`: Enables Generational ZGC in Java 21+ (default in Java 25).
- `-XX:+UseShenandoahGC`: Low-pause concurrent GC developed by Red Hat.
- `-XX:+UseParallelGC`: High-throughput generational collector (batch workloads).
- `-XX:+UseSerialGC`: Single-threaded collector (microservices with <512MB RAM).

### Collector Tuning Matrix
| Collector | Tuning Flag | Default | Description |
| :--- | :--- | :---: | :--- |
| **G1 GC** | `-XX:MaxGCPauseMillis=<N>` | `200` | Target max GC pause duration in ms |
| | `-XX:InitiatingHeapOccupancyPercent=<N>` | `45` | Old gen threshold % to trigger concurrent mark |
| | `-XX:G1ReservePercent=<N>` | `10` | Free heap % reserved as buffer against promotion failure |
| | `-XX:G1HeapRegionSize=<N>` | Auto ($1\text{M}-32\text{M}$) | Region partition size (must be power of 2) |
| | `-XX:ParallelGCThreads=<N>` | $\approx 5/8 \times \text{CPUs}$ | STW worker threads |
| | `-XX:ConcGCThreads=<N>` | $\approx 1/4 \times \text{ParallelGCThreads}$ | Background concurrent marking threads |
| **ZGC** | `-XX:ZAllocationSpikeTolerance=<N>` | `2` | Multiplier for allocation rate bursts |
| | `-XX:+ZGenerational` | Preview in 21, Default in 25 | Enables separate Young/Old concurrent collection |

---

## 4. Unified JVM Logging Framework (`-Xlog`)

Unified JVM Logging (Java 9+ JEP 158 / JEP 271) standardizes all HotSpot diagnostic logging under the single `-Xlog` switch:

```bash
# General Syntax:
-Xlog:<tagset>:<output>:<decorators>:<output-options>
```

### Essential Logging Configurations
```bash
# 1. Production GC Logging with file rotation
-Xlog:gc*,gc+phases=debug:file=/var/log/jvm/gc.log:time,uptime,pid:filecount=10,filesize=50M

# 2. Safepoint Latency & TTSP Logging (Diagnose latency spikes)
-Xlog:safepoint=info,safepoint+stats=debug:file=/var/log/jvm/safepoints.log:time,uptime:filecount=5,filesize=20M

# 3. Class Loading and Unloading
-Xlog:class+load=info,class+unload=info:file=/var/log/jvm/classload.log:time

# 4. JIT Compilation Logging
-Xlog:jit+compilation=info:file=/var/log/jvm/jit.log:time
```

---

## 5. JIT Compilation & Inlining Optimization Flags

| Flag | Purpose | Default |
| :--- | :--- | :---: |
| `-XX:+TieredCompilation` | Enables C1 (client) and C2 (server) tiered compilation | Enabled |
| `-XX:CompileThreshold=<N>` | Invocations before method is JIT-compiled (Tier 4) | 10,000 |
| `-XX:MaxInlineLevel=<N>` | Maximum call-tree depth for inlining | 9 |
| `-XX:MaxInlineSize=<N>` | Maximum bytecode size of a method to be inlined | 35 bytes |
| `-XX:FreqInlineSize=<N>` | Maximum bytecode size for frequently invoked methods | 325 bytes |
| `-XX:+DoEscapeAnalysis` | Enables escape analysis for scalar replacement | Enabled |
| `-XX:+PrintCompilation` | Prints JIT-compiled methods to standard out | Disabled |

---

## 6. Out-of-Memory & Emergency Diagnostic Flags

```bash
# Production Crash & Failure Guard Flags:
-XX:+HeapDumpOnOutOfMemoryError
-XX:HeapDumpPath=/var/log/jvm/dumps/
-XX:+ExitOnOutOfMemoryError
-XX:ErrorFile=/var/log/jvm/hs_err_pid%p.log
-XX:+PrintCommandLineFlags
```
- `-XX:+HeapDumpOnOutOfMemoryError`: Automatically dumps the full Java heap to an `.hprof` file upon first OOM occurrence.
- `-XX:+ExitOnOutOfMemoryError`: Terminates the JVM process immediately when OOM occurs, allowing Kubernetes to restart the unhealthy container rather than lingering in a zombie thrashing state.
- `-XX:ErrorFile`: Directs HotSpot crash dump reports (`hs_err_pid.log`) to a designated log directory.

---

## 7. JDK Diagnostic CLI Tooling Reference

| Tool | Primary Purpose | Command Example |
| :--- | :--- | :--- |
| **`jcmd`** | Universal command interface for running JVMs | `jcmd <pid> GC.heap_info` |
| | Capture Thread Dump | `jcmd <pid> Thread.print -l > dump.txt` |
| | Capture Heap Dump | `jcmd <pid> GC.heap_dump /tmp/heap.hprof` |
| | View Active JVM Flags | `jcmd <pid> VM.flags -all` |
| | View System Properties | `jcmd <pid> VM.system_properties` |
| **`jstat`** | Real-time Garbage Collection monitoring | `jstat -gcutil <pid> 1000 10` |
| **`jstack`** | Traditional thread dump utility | `jstack -l <pid> > thread_dump.txt` |
| **`jmap`** | Memory map and class histogram | `jmap -histo:live <pid>` |
| **`jinfo`** | View and dynamically modify manageable flags | `jinfo -flag +HeapDumpOnOutOfMemoryError <pid>` |
| **`jps`** | Lists active JVM processes on the host | `jps -lmv` |

---

## 8. JDK Flight Recorder (JFR) & JMC Commands

JDK Flight Recorder captures low-overhead (<1%) production telemetry directly within the HotSpot kernel:

```bash
# 1. Start continuous recording at JVM startup
java -XX:StartFlightRecording=disk=true,dumponexit=true,filename=/var/log/jvm/app.jfr,settings=profile.jfc,maxage=24h,maxsize=5g -jar app.jar

# 2. Start a 60-second diagnostic recording dynamically via jcmd
jcmd <pid> JFR.start name=diag_record duration=60s filename=/tmp/diagnostic.jfr settings=profile

# 3. Check status of running recordings
jcmd <pid> JFR.check

# 4. Manually dump and stop recording
jcmd <pid> JFR.dump name=diag_record filename=/tmp/manual_dump.jfr
jcmd <pid> JFR.stop name=diag_record
```

---

## 9. Native Memory Tracking (NMT) Reference

Tracks all off-heap native memory allocations (Metaspace, thread stacks, CodeCache, GC structures, Direct ByteBuffers):

```bash
# 1. Enable NMT at JVM startup (summary or detail mode)
java -XX:NativeMemoryTracking=summary -jar app.jar

# 2. Establish a baseline during normal operation
jcmd <pid> VM.native_memory baseline

# 3. Compare current memory allocation against baseline to detect native memory leaks
jcmd <pid> VM.native_memory detail.diff
```

---

## 10. Async-Profiler & FlameGraph Generation

Async-profiler avoids JVM safepoint bias by sampling directly via Linux kernel `perf_events` and HotSpot `AsyncGetCallTrace`:

```bash
# 1. Profile CPU execution for 30 seconds and generate an interactive HTML FlameGraph
./asprof -d 30 -f /tmp/cpu_flamegraph.html <pid>

# 2. Profile native memory allocations
./asprof -d 30 -e alloc -f /tmp/alloc_flamegraph.html <pid>

# 3. Profile lock contention (threads waiting on monitor locks or ReentrantLocks)
./asprof -d 30 -e lock -f /tmp/lock_flamegraph.html <pid>
```

---

## 11. Production Flags Reference Templates

### Standard Microservice Template (Containerized - G1 GC)
```bash
java -server \
     -XX:+UseContainerSupport \
     -XX:MaxRAMPercentage=75.0 \
     -XX:InitialRAMPercentage=75.0 \
     -XX:+UseG1GC \
     -XX:MaxGCPauseMillis=150 \
     -XX:+ParallelRefProcEnabled \
     -XX:+HeapDumpOnOutOfMemoryError \
     -XX:HeapDumpPath=/var/log/jvm/dumps/ \
     -XX:+ExitOnOutOfMemoryError \
     -Xlog:gc*,gc+phases=debug:file=/var/log/jvm/gc.log:time,uptime,pid:filecount=5,filesize=50M \
     -jar microservice.jar
```

### Ultra-Low-Latency Template (Java 21+ Generational ZGC)
```bash
java -server \
     -Xms16g -Xmx16g \
     -XX:+UseZGC \
     -XX:+ZGenerational \
     -XX:+AlwaysPreTouch \
     -XX:+HeapDumpOnOutOfMemoryError \
     -XX:HeapDumpPath=/var/log/jvm/dumps/ \
     -Xlog:gc*:file=/var/log/jvm/zgc.log:time,uptime:filecount=5,filesize=50M \
     -jar low-latency-service.jar
```

---

## 12. Common Mistakes & Dangerous Flags

1. **Avoid `-XX:+DisableExplicitGC`:** Disabling `System.gc()` breaks Direct ByteBuffer reclamation in frameworks like Netty that invoke `System.gc()` to clean up off-heap memory. Instead, use `-XX:+ExplicitGCInvokesConcurrent`.
2. **Setting `-Xmx` too close to Container Limits:** If a Kubernetes pod has a 4GB RAM limit and `-Xmx3.8g` is configured, native memory overhead (Metaspace, thread stacks, CodeCache) will breach 4GB, causing the OS kernel OOM Killer to abruptly terminate the pod with exit code 137.
3. **Omitting `-XX:+AlwaysPreTouch` for Low Latency:** Without page pre-touching, physical memory pages are allocated lazily upon first write, causing latency spikes during startup and early transactions.

---

## 13. Exhaustive HotSpot `-XX` Diagnostic Flags Catalog

### Memory & Sizing Flags
| Flag | Syntax / Type | Default | Description |
| :--- | :--- | :---: | :--- |
| `UseCompressedOops` | Boolean | True ($\le 32	ext{GB}$) | Enables 32-bit pointers for 64-bit JVM objects |
| `UseCompressedClassPointers` | Boolean | True | Uses 32-bit compressed class pointers in Klass Word |
| `CompressedClassSpaceSize` | Size | 1GB | Native memory reserved for compressed class metadata |
| `ReservedCodeCacheSize` | Size | 240MB | Maximum memory reserved for JIT-compiled native code |
| `InitialCodeCacheSize` | Size | 2.5MB | Initial code cache memory allocation |
| `StringTableSize` | Integer | 65536 | Number of buckets in the native String Constant Pool hash table |
| `MaxRAM` | Size | Host RAM | Explicitly overrides total host physical memory calculation |

### Garbage Collector Tuning Flags (G1 & Parallel)
| Flag | Syntax / Type | Default | Description |
| :--- | :--- | :---: | :--- |
| `G1PeriodicGCInterval` | Milliseconds | 0 (disabled) | Triggers periodic concurrent GC to return memory to OS (Java 12+) |
| `G1PeriodicGCSysLoadThreshold` | Float | 0.0 | Maximum OS system load average allowed for periodic GC |
| `G1NewSizePercent` | Percentage | 5 | Minimum percentage of heap allocated to Young Generation |
| `G1MaxNewSizePercent` | Percentage | 60 | Maximum percentage of heap allocated to Young Generation |
| `G1MixedGCCountTarget` | Integer | 8 | Target number of mixed GC phases to complete marking cycle |
| `G1MixedGCLiveThresholdPercent`| Percentage | 85 | Old regions with live data above this % are skipped during mixed GC |
| `G1OldCSetRegionThresholdPercent`| Percentage | 10 | Max percentage of old regions collected in a single mixed GC pause |

---

## 14. GC Log Parsing Guide & Entry Anatomy

### Unified GC Log Entry Anatomy
```
[2026-11-21T14:32:01.124+0000][0.523s][info][gc,start    ] GC(12) Pause Young (Normal) (G1 Evacuation Pause)
[2026-11-21T14:32:01.125+0000][0.524s][info][gc,phases   ] GC(12) Pre Evacuate Collection Set: 0.1ms
[2026-11-21T14:32:01.140+0000][0.539s][info][gc,phases   ] GC(12) Evacuate Collection Set: 14.8ms
[2026-11-21T14:32:01.141+0000][0.540s][info][gc,phases   ] GC(12) Post Evacuate Collection Set: 1.2ms
[2026-11-21T14:32:01.142+0000][0.541s][info][gc          ] GC(12) Pause Young (Normal) (G1 Evacuation Pause) 2048M->512M(8192M) 17.214ms
[2026-11-21T14:32:01.142+0000][0.541s][info][gc,cpu      ] GC(12) User=0.08s Sys=0.01s Real=0.02s
```

### Decoded Metrics:
1. `Pause Young (Normal)`: Young generation collection phase.
2. `G1 Evacuation Pause`: Copying surviving live objects from Eden and Survivor regions into new Survivor or Old regions.
3. `2048M->512M(8192M)`:
   - **`2048M`**: Heap occupancy before GC pause.
   - **`512M`**: Heap occupancy immediately after GC pause (1.5GB reclaimed!).
   - **`(8192M)`**: Total current committed heap capacity.
4. `17.214ms`: Wall-clock pause duration (STW time experienced by application mutator threads).
5. `User=0.08s Sys=0.01s Real=0.02s`:
   - `User`: Cumulative CPU time across all GC worker threads.
   - `Real`: Wall-clock elapsed time ($0.02	ext{s} pprox 20	ext{ms}$). Notice $	ext{User} / 	ext{Real} pprox 4$, indicating 4 parallel GC worker threads ran concurrently.

---

## 15. Total JVM Native Memory Footprint Formula

$$	ext{Total Resident Set Size (RSS)} = 	ext{Heap} + 	ext{Metaspace} + 	ext{Thread Stacks} + 	ext{CodeCache} + 	ext{Direct Memory} + 	ext{GC Overhead} + 	ext{Symbols} + 	ext{C-Heap}$$

### Component Breakdown:
1. **Java Heap:** Size configured via `-Xmx` (committed or reserved).
2. **Metaspace:** Native class metadata, bounded via `-XX:MaxMetaspaceSize`.
3. **Thread Stacks:** $N_{	ext{threads}} 	imes 	ext{-Xss}$. For 2,000 platform threads at `-Xss1m`, this consumes 2GB native memory.
4. **CodeCache:** JIT-compiled assembly machine code, bounded by `-XX:ReservedCodeCacheSize` (default 240MB).
5. **Direct Memory:** Allocated via `ByteBuffer.allocateDirect()`, bounded by `-XX:MaxDirectMemorySize`.
6. **GC Data Structures:** G1 Remembered Sets (R-Sets) and Card Tables typically require 5–10% of total heap size.
7. **Symbol Tables:** Interned string hashes and constant pool symbolic references.
8. **C-Heap Malloc Fragmentation:** glibc dynamic memory allocator per-thread memory arenas ($8 	imes 	ext{CPU cores}$ arenas).

---

## 16. Rapid Production Diagnostic Runbook

### Incident: Process Latency Spike Triage
```bash
# Step 1: Confirm CPU consumption of JVM process
top -b -n 1 | grep java

# Step 2: Check GC pressure and pause counts in real-time
jstat -gcutil <pid> 1000 5

# Step 3: Capture full thread dump
jcmd <pid> Thread.print -l > /tmp/thread_dump_$(date +%s).txt

# Step 4: Check top memory consumer classes
jcmd <pid> GC.class_histogram | head -n 30

# Step 5: Trigger immediate JFR recording for 60 seconds
jcmd <pid> JFR.start name=triage duration=60s filename=/tmp/triage.jfr settings=profile
```

---

## 17. Linux OS Core Dump Configuration for JVM Crashes

When the JVM experiences a native crash (segmentation fault `SIGSEGV` or `SIGBUS` in JNI/C code), capturing a core dump is critical:

```bash
# 1. Enable unlimited core dump file size in Linux shell
ulimit -c unlimited

# 2. Configure Linux kernel core dump pattern
echo "/var/log/jvm/core-%e-%p-%t" > /proc/sys/kernel/core_pattern

# 3. Enable JVM flag to dump core on crash
java -XX:+CreateCoredumpOnCrash -jar service.jar

# 4. Inspect core dump with GDB
gdb $JAVA_HOME/bin/java /var/log/jvm/core-java-28412-1718291021
(gdb) bt
```

---

## 18. Common JVM Exit Codes Reference

| Exit Code | Meaning | Typical Root Cause |
| :---: | :--- | :--- |
| `0` | Success | Normal shutdown via `System.exit(0)` or main thread completion |
| `1` | Generic Error | Unhandled exception or custom error exit code |
| `130` | Terminated by `SIGINT` | Process killed via `Ctrl+C` in terminal |
| `137` | Terminated by `SIGKILL` (`128 + 9`) | **Linux OS OOM Killer** killed process for exceeding cgroup memory quota |
| `143` | Terminated by `SIGTERM` (`128 + 15`) | Graceful container shutdown request from Kubernetes pod eviction |

---

## 19. Quick Summary / Diagnostic Cheat Sheet Card

- **Heap Sizing:** Set `-Xms` equal to `-Xmx` in non-container VMs. In Docker/K8s, use `-XX:+UseContainerSupport -XX:MaxRAMPercentage=75.0`.
- **Garbage Collection:** Default is G1 (`-XX:+UseG1GC`). For low-pause terabyte heaps, use ZGC (`-XX:+UseZGC -XX:+ZGenerational`).
- **Emergency Dumps:** Always enable `-XX:+HeapDumpOnOutOfMemoryError -XX:HeapDumpPath=/var/log/jvm/dumps/ -XX:+ExitOnOutOfMemoryError`.
- **CLI Commands:** `jcmd <pid> Thread.print` for thread dumps, `jcmd <pid> GC.heap_info` for memory summary, `jstat -gcutil <pid> 1000` for real-time GC rates.

---

## 20. JMX Remote Monitoring Flags

```bash
# Secure remote JMX access configuration
java -Dcom.sun.management.jmxremote \
     -Dcom.sun.management.jmxremote.port=9010 \
     -Dcom.sun.management.jmxremote.rmi.port=9010 \
     -Dcom.sun.management.jmxremote.local.only=false \
     -Dcom.sun.management.jmxremote.authenticate=true \
     -Dcom.sun.management.jmxremote.password.file=/etc/jmx.password \
     -Dcom.sun.management.jmxremote.access.file=/etc/jmx.access \
     -Dcom.sun.management.jmxremote.ssl=true \
     -jar backend-service.jar
```
