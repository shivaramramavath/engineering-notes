# Linux Performance Profiling: perf, PMU Counters & Flame Graphs

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux System Profiling: `perf`, Hardware Counters, and Brendan Gregg Flame Graphs |
| **Module** | `06-advanced` — Advanced Systems & Kernel Mechanics |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS (Kernel 6.8+), RHEL 9.4+ (Kernel 5.14+) |
| **Prerequisites** | `03-advanced-debugging.md`, `05-production/06-performance.md` |
| **Difficulty** | Advanced |
| **Last Updated** | 2026 |

---

## Table of Contents
1. [What & Why: Pinpointing Micro-Architectural Bottlenecks](#what--why-pinpointing-micro-architectural-bottlenecks)
2. [Core Concepts & Architecture](#core-concepts--architecture)
3. [Internal Mechanics: PMU Counters & Sampling Interrupts](#internal-mechanics-pmu-counters--sampling-interrupts)
4. [Command Syntax & perf Subcommand Reference](#command-syntax--perf-subcommand-reference)
5. [Progressive Practical Examples](#progressive-practical-examples)
   - [Example 1: High-Level System Telemetry with perf stat](#example-1-high-level-system-telemetry-with-perf-stat)
   - [Example 2: Interactive Real-Time Profiling with perf top](#example-2-interactive-real-time-profiling-with-perf-top)
   - [Example 3: Call-Graph Sampling with perf record and report](#example-3-call-graph-sampling-with-perf-record-and-report)
   - [Example 4: Generating SVG On-CPU Flame Graphs](#example-4-generating-svg-on-cpu-flame-graphs)
   - [Example 5: End-to-End Automated CPU Hotspot Profiling Script](#example-5-end-to-end-automated-cpu-hotspot-profiling-script)
6. [Production Best Practices & Hardening](#production-best-practices--hardening)
7. [Common Failure Modes & Debugging](#common-failure-modes--debugging)
8. [Performance, Security & Scale Considerations](#performance-security--scale-considerations)
9. [Comparison Matrix: Linux Profiling Methodologies](#comparison-matrix-linux-profiling-methodologies)
10. [Real-World Interview Scenarios](#real-world-interview-scenarios)
11. [Hands-On Practice Labs](#hands-on-practice-labs)
12. [Cheat Sheet & Quick Reference](#cheat-sheet--quick-reference)

---

## What & Why: Pinpointing Micro-Architectural Bottlenecks

Traditional monitoring tools (e.g., `top`, `htop`, Prometheus `node_exporter`) report coarse percentages: "Service X consumes 85% CPU". They cannot answer the critical engineering question: **which specific functions or source code lines are burning those CPU cycles?** Are cycles wasted executing application code, waiting on CPU cache misses, executing kernel lock contention, or servicing page faults?

Linux `perf` (also known as `perf_events`) is the official kernel subsystem and CLI suite for low-overhead performance profiling. By interfacing directly with hardware Performance Monitoring Units (PMUs) built into modern processors (Intel, AMD, ARM), `perf` samples execution stacks with minimal overhead (~1%). When paired with Brendan Gregg's **Flame Graphs**, engineers can instantly visualize millions of CPU samples and identify multi-threaded bottlenecks in complex microservices.

---

## Core Concepts & Architecture

```
+-------------------------------------------------------------------------+
|                              CPU HARDWARE                               |
|                                                                         |
|  +-------------------------------------------------------------------+  |
|  |           PMU (Performance Monitoring Unit) Hardware Counters     |  |
|  |      Cycles, Instructions, L1/L3 Cache Misses, Branch Misses      |  |
|  +-------------------------------------------------------------------+  |
|                                  | Hardware Overflow Interrupt (NMI)    |
|                                  v                                      |
+----------------------------------|--------------------------------------+
|                           LINUX KERNEL                                  |
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  |                       perf_events Subsystem                       |  |
|  |  Samples target Instruction Pointer (RIP) + Unwinds Stack         |  |
|  |  (Frame Pointer RBP / DWARF / eBPF ORC Unwinder)                  |  |
|  +-------------------------------------------------------------------+  |
|                                  | Circular Ring Buffer (mmap)          |
|                                  v                                      |
+----------------------------------|--------------------------------------+
|                           USER SPACE                                    |
|                                  v                                      |
|  +---------------------+    +-------------------+    +---------------+  |
|  |     perf record     | -> |     perf.data     | -> |  perf report  |  |
|  +---------------------+    +-------------------+    +---------------+  |
|                                       |                                 |
|                                       v                                 |
|                             stackcollapse-perf.pl                       |
|                                       v                                 |
|                               flamegraph.pl                             |
|                                       v                                 |
|                               [ flamegraph.svg ]                        |
+-------------------------------------------------------------------------+
```

1. **Hardware Events**: Measured via PMU registers directly on silicon. Counts CPU cycles, retired instructions, branch mispredictions, and cache references.
2. **Software Events**: Kernel counters for operating system events: page faults, context switches, CPU migrations.
3. **Tracepoints & Probes**: Static instrumentation points (`kmem:kmalloc`, `sched:sched_switch`) or dynamic probes (`kprobes`, `uprobes`) inserted into arbitrary code paths.
4. **Sampling vs. Counting**: Counting (`perf stat`) records aggregate event counts during an interval. Sampling (`perf record`) interrupts the CPU at a fixed frequency (e.g., 99 Hz) to capture execution backtraces.

---

## Internal Mechanics: PMU Counters & Sampling Interrupts

### 1. PMU Hardware Interruption
When sampling at 99 Hertz (`perf record -F 99`), the kernel programs the CPU's PMU counter register to overflow after a calculated number of elapsed cycles. When the counter overflows, the CPU fires a Non-Maskable Interrupt (NMI).

### 2. Stack Unwinding
Upon receiving the NMI, the kernel freezes the CPU execution context and captures the instruction pointer (`RIP`). To reconstruct the call chain leading to this instruction, the kernel unwinds the stack:
- **Frame Pointer (`fp`)**: Extremely fast, low overhead. Reads the `%rbp` register to traverse linked stack frames. Requires code compiled with `-fno-omit-frame-pointer`.
- **DWARF (`dwarf`)**: Unwinds stack using debug information tables. High overhead; copies full stack memory pages per sample.
- **LBR (Last Branch Record)**: Hardware CPU buffer recording recent branch jumps. Zero stack unwinding overhead, but limited call depth.

### 3. Flame Graph Anatomy
In an SVG Flame Graph:
- **X-axis**: Alphabetical order across the population of samples (NOT chronological time). Wider blocks mean more CPU samples were captured executing that function.
- **Y-axis**: Call stack depth. Lower blocks are parent callers; top blocks represent functions actively executing on the CPU when sampled.

---

## Command Syntax & perf Subcommand Reference

| Subcommand | Primary Flags | Functionality |
| :--- | :--- | :--- |
| `perf stat` | `-e <events>`, `-p <pid>`, `-a` (all CPUs), `-d` (detailed) | Reports aggregate hardware/software counter counts. |
| `perf top` | `-p <pid>`, `-a`, `-g` (call graph), `-F <hz>` | Interactive ncurses terminal display of active functions. |
| `perf record` | `-F <freq>`, `-g` (call-graph), `-p <pid>`, `-o <file>` | Samples backtraces into binary file (`perf.data`). |
| `perf report` | `-i <file>`, `--stdio`, `-g graph` | Analyzes and renders interactive tree view of recorded samples. |
| `perf script` | `-i <file>` | Dumps raw recorded event samples in plaintext for analysis scripts. |
| `perf list` | `hw`, `sw`, `cache`, `tracepoint` | Lists all supported hardware and software events on the host. |

---

## Progressive Practical Examples

### Example 1: High-Level System Telemetry with perf stat

Profile a database or compilation command to evaluate Instructions Per Cycle (IPC) and cache efficiency.

```bash
#!/usr/bin/env bash
# Step 1: Detailed hardware performance audit
set -euo pipefail

echo "Profiling compilation task..."
sudo perf stat -d gcc -O2 -c /tmp/workload.c -o /tmp/workload.o

# Explanation of Output Metrics:
# - instructions / cycles = IPC (Instructions Per Cycle)
#   IPC > 1.0: Efficient CPU pipeline utilization.
#   IPC < 0.5: CPU memory-bound; stalling on RAM or cache misses.
# - branch-misses: Branch predictor failures causing pipeline flushes.
# - L1-dcache-load-misses: CPU waiting for data cache refills.
```

### Example 2: Interactive Real-Time Profiling with perf top

Monitor CPU hotspots in real time across the entire host or an isolated process.

```bash
#!/usr/bin/env bash
# Step 2: Real-time ncurses profiling
set -euo pipefail

TARGET_PID=$(pgrep -f "nginx: worker" | head -n 1)

# Launch perf top focusing on worker process with call chains
sudo perf top -p "${TARGET_PID}" --call-graph fp -F 99
```

### Example 3: Call-Graph Sampling with perf record and report

Capture execution stacks of a multi-threaded service for 15 seconds and inspect using the terminal report viewer.

```bash
#!/usr/bin/env bash
# Step 3: Record and view backtrace samples
set -euo pipefail

TARGET_PID=$(pgrep -f "java" | head -n 1)
OUTPUT_DATA="/tmp/perf_java.data"

echo "Sampling PID ${TARGET_PID} at 99Hz for 15 seconds..."
# -F 99 avoids sampling in lockstep with a 100Hz periodic OS timer
sudo perf record -F 99 -p "${TARGET_PID}" -g --call-graph fp -o "${OUTPUT_DATA}" -- sleep 15

echo "Sampling complete. Analyzing report..."
sudo perf report -i "${OUTPUT_DATA}" --stdio | head -n 40
```

### Example 4: Generating SVG On-CPU Flame Graphs

Generate an interactive, zoomable SVG Flame Graph using Brendan Gregg's open-source toolchain.

```bash
#!/usr/bin/env bash
# Step 4: Generate SVG Flame Graph
set -euo pipefail

WORKDIR="/tmp/flamegraph-tools"
mkdir -p "${WORKDIR}"
cd "${WORKDIR}"

# Clone toolchain if not present
if [[ ! -d "FlameGraph" ]]; then
    git clone --depth 1 https://github.com/brendangregg/FlameGraph.git
fi

PERF_DATA="/tmp/perf_capture.data"

# 1. Record 10 seconds of system-wide CPU activity
echo "Recording system-wide CPU profile..."
sudo perf record -F 99 -a -g --call-graph fp -o "${PERF_DATA}" -- sleep 10

# 2. Extract stacks with perf script
sudo perf script -i "${PERF_DATA}" > /tmp/out.perf

# 3. Collapse stacks into single-line format
./FlameGraph/stackcollapse-perf.pl /tmp/out.perf > /tmp/out.folded

# 4. Render interactive SVG Flame Graph
./FlameGraph/flamegraph.pl /tmp/out.folded > /tmp/flamegraph.svg

echo "Flame Graph successfully generated at: /tmp/flamegraph.svg"
ls -lh /tmp/flamegraph.svg
```

### Example 5: End-to-End Automated CPU Hotspot Profiling Script

A production-grade diagnostic automation script that captures samples, validates symbol resolution, detects high-overhead kernel functions, and renders an SVG artifact.

```bash
#!/usr/bin/env bash
# Step 5: Automated production CPU profiler
set -euo pipefail

DURATION="${1:-10}"
PID_FILTER="${2:-}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUT_DIR="/var/log/profiles/${TIMESTAMP}"
sudo mkdir -p "${OUT_DIR}"

echo "=================================================="
echo "    LINUX PERF AUTOMATED DIAGNOSTIC RECORDER"
echo "    Duration: ${DURATION}s | Output: ${OUT_DIR}"
echo "=================================================="

# Check kernel permissions for profiling
PERF_PARANOID=$(cat /proc/sys/kernel/perf_event_paranoid)
if [[ "${PERF_PARANOID}" -gt 1 ]]; then
    echo "Warning: perf_event_paranoid is ${PERF_PARANOID}. Lowering temporarily to 1 for profiling."
    echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid > /dev/null
fi

PERF_CMD=(sudo perf record -F 99 -g --call-graph fp -o "${OUT_DIR}/perf.data")

if [[ -n "${PID_FILTER}" ]]; then
    echo "Profiling specific PID: ${PID_FILTER}"
    PERF_CMD+=(-p "${PID_FILTER}")
else
    echo "Profiling system-wide on all CPUs..."
    PERF_CMD+=(-a)
fi

# Execute capture
"${PERF_CMD[@]}" -- sleep "${DURATION}"

echo "Processing trace data..."
sudo perf script -i "${OUT_DIR}/perf.data" > "${OUT_DIR}/raw.script"

# Generate text summary of top 20 functions
sudo perf report -i "${OUT_DIR}/perf.data" --stdio | grep -E "^[ ]+[0-9\.]+\%" | head -n 20 > "${OUT_DIR}/top_functions.txt"

echo "Top CPU Hotspots:"
cat "${OUT_DIR}/top_functions.txt"

# Clean up raw data if required
echo "Diagnostic archive ready at: ${OUT_DIR}"
```

---

## Production Best Practices & Hardening

- **Sample at an odd frequency**: Use `-F 99` or `-F 49` instead of `-F 100`. Sampling at exact multiples of 100 Hz can accidentally synchronize with periodic kernel timer interrupts or application cron loops, producing skewed, misleading profiles.
- **Compile with `-fno-omit-frame-pointer`**: For fast, accurate stack traces without multi-gigabyte DWARF debug tables, compile production binaries (C, C++, Go, Rust) with frame pointers preserved.
- **Tune `kernel.perf_event_paranoid`**:
  - `-1`: Allow access to everything (raw tracepoints, CPU counters).
  - `0`: Allow raw tracepoints and CPU counters.
  - `1`: Allow CPU counters without raw tracepoints.
  - `2`: Allow only user-space profiling (default on Debian/Ubuntu).
- **Monitor Ring Buffer Memory**: When profiling hundreds of CPU cores under high load, increase the `perf` ring buffer size with `-m 128M` to avoid dropped sample events.
- **Avoid DWARF in Production**: Using `--call-graph dwarf` dumps entire 8KB stack segments per sample, consuming immense disk bandwidth and introducing significant CPU overhead.

---

## Common Failure Modes & Debugging

| Symptom / Error | Root Cause | Diagnosis Command | Resolution |
| :--- | :--- | :--- | :--- |
| `Permission denied: try setting /proc/sys/kernel/perf_event_paranoid to -1` | Kernel security policy blocks non-root or unprivileged profiling. | `cat /proc/sys/kernel/perf_event_paranoid` | Run with `sudo` or execute `sysctl -w kernel.perf_event_paranoid=1`. |
| Flame Graph stacks are flat or broken (`unknown [unknown]`) | Binary was stripped or compiled with `-fomit-frame-pointer`. | `file <binary>` and `objdump -d <binary>` | Recompile with `-fno-omit-frame-pointer` or install debug symbol packages. |
| Java / Node.js stacks show only memory addresses (hex numbers) | JIT compilers generate machine code dynamically in memory without ELF symbol tables. | Check for missing JIT symbol mapping files | Use `async-profiler` for Java, or Node.js `--perf-basic-prof`. |
| `perf: command not found` | The `linux-tools` package matching the exact running kernel is missing. | `uname -r` | Run `sudo apt install linux-tools-common linux-tools-$(uname -r)`. |
| High sample drop counts in `perf.data` | Disk write I/O too slow or sampling frequency too high. | `perf report` warning header | Lower sampling frequency (e.g., `-F 49`) or increase ring buffer size (`-m 64M`). |

---

## Performance, Security & Scale Considerations

```
+--------------------------------------------------------------------------+
| Profiling Mechanism Overhead Comparison                                  |
|                                                                          |
| 1. perf record (-F 99 --call-graph fp)                                    |
|    Overhead: ~0.5% - 1.5% CPU. Safe for 24/7 production sampling.        |
|                                                                          |
| 2. perf record (--call-graph dwarf)                                      |
|    Overhead: 15% - 40% CPU. High disk I/O; can cause storage saturation. |
|                                                                          |
| 3. Instrumentation Tracers (Valgrind / Callgrind)                        |
|    Overhead: 1000% - 5000% (10x-50x slowdown). Totally unusable in prod. |
+--------------------------------------------------------------------------+
```

1. **JIT Compilation Complications**: Runtimes with Just-In-Time compilation (Java HotSpot, V8 JavaScript) allocate dynamic memory pages on the fly. `perf` cannot map these addresses to method names unless a symbol map file (`/tmp/perf-<pid>.map`) is continuously updated.
2. **KASLR (Kernel Address Space Layout Randomization)**: To profile kernel functions as non-root, `/proc/sys/kernel/kptr_restrict` must be set to `0`; otherwise, kernel addresses are scrubbed as `0000000000000000` for security.

---

## Comparison Matrix: Linux Profiling Methodologies

| Dimension | `perf stat` | `perf record` (FP) | `perf record` (DWARF) | eBPF (`bpftrace`) | `strace` |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Output Type** | Counter Totals | Call Chains / Flame Graph | Deep Call Chains | Custom Aggregations| Syscall Stream |
| **CPU Overhead** | < 0.1% | ~1% | 15% - 30% | ~1% - 2% | 50% - 800% |
| **Storage Usage** | Bytes | Megabytes | Gigabytes | Kilobytes | Gigabytes |
| **Needs Symbols?**| No | Frame Pointers | DWARF `.debug_info`| Optional | No |
| **Prod Viability**| Always Safe | Safe for high load | Use with caution | Always Safe | Unsafe under load|

---

## Real-World Interview Scenarios

### Scenario 1: What is the difference between an On-CPU and an Off-CPU Flame Graph?
<details>
<summary>Detailed Answer</summary>
An **On-CPU Flame Graph** visualizes functions that are actively consuming CPU cycles while executing on cores. It is generated by sampling CPU instruction pointers at periodic intervals (e.g., 99 Hz).
An **Off-CPU Flame Graph** visualizes time that threads spend blocked waiting off-CPU (e.g., waiting for disk I/O, network socket responses, lock contention, or sleep calls). It is generated by tracing scheduler switch events (`sched:sched_switch`) and recording the time elapsed between when a thread is switched out and when it is switched back in.
</details>

### Scenario 2: Why is the X-axis in a Flame Graph sorted alphabetically instead of chronologically?
<details>
<summary>Detailed Answer</summary>
If the X-axis were chronological (like a timeline), identical call stacks occurring at different points in time would be scattered across the graph, making it impossible to evaluate aggregate time spent in any given function. Sorting alphabetically groups identical call stacks together into single merged horizontal blocks. The width of each block then directly corresponds to the total percentage of CPU samples captured in that function hierarchy.
</details>

### Scenario 3: Why should engineers avoid sampling at 100 Hz (`-F 100`)?
<details>
<summary>Detailed Answer</summary>
The Linux kernel and user-space runtimes frequently schedule periodic housekeeping, garbage collection, and timer events at round intervals (such as 100 Hz or 1000 Hz). Profiling at exactly 100 Hz causes the sampling interrupt to continuously coincide with those periodic routines (the "phase locking" or harmonic synchronization problem). Sampling at an odd or prime frequency (such as 99 Hz, 49 Hz, or 101 Hz) prevents this skew.
</details>

### Scenario 4: What is IPC (Instructions Per Cycle) and why is it important in performance tuning?
<details>
<summary>Detailed Answer</summary>
Instructions Per Cycle (IPC) is the ratio of retired instructions to CPU clock cycles. A modern multi-issue superscalar CPU can retire 2 to 4 instructions per cycle under optimal conditions. An IPC > 1.0 indicates compute-bound code operating efficiently. An IPC < 0.5 indicates memory-bound code: the CPU core is stalled waiting for memory to arrive from L3 cache or DRAM, meaning optimizing code algorithms or data structures for cache locality will yield far greater speedups than upgrading to faster CPU clock speeds.
</details>

### Scenario 5: Why do Java or Node.js Flame Graphs often display `[unknown]` hex addresses?
<details>
<summary>Detailed Answer</summary>
Standard Linux binaries (C/C++, Rust, Go) contain ELF symbol tables linking machine instruction addresses to function names. Virtual machines with JIT compilers (JVM, V8) generate machine code dynamically in anonymous executable memory pages (`mmap`). Because these memory areas are not associated with an ELF binary on disk, `perf` cannot resolve the symbol names. To resolve them, the runtime must write a `/tmp/perf-<pid>.map` symbol file, or use dedicated agents like `async-profiler` or `--perf-basic-prof`.
</details>

### Scenario 6: What are the trade-offs between Frame Pointer (`fp`) and DWARF unwinding?
<details>
<summary>Detailed Answer</summary>
Frame Pointer unwinding follows the linked list of `%rbp` base pointers on the stack. It requires minimal CPU overhead (~1%) and tiny sample storage, but requires that software is compiled with `-fno-omit-frame-pointer` (which uses up one general-purpose CPU register). DWARF unwinding does not require frame pointers, but requires reading complex debugging tables and copying large segments of the process stack (often 8KB per sample) into the ring buffer, creating high CPU overhead and massive `perf.data` files.
</details>

### Scenario 7: What does a high `branch-misses` count in `perf stat` tell you?
<details>
<summary>Detailed Answer</summary>
A high branch-miss rate indicates that the CPU's branch prediction unit is frequently guessing incorrectly on conditional branches (`if/else`, switch statements, virtual method tables). When a misprediction occurs, the CPU must flush its execution pipeline and discard speculative instructions, wasting dozens of clock cycles per miss. Resolving this involves sorting branch conditions by probability, using branchless algorithms, or leveraging compiler profile-guided optimization (PGO).
</details>

### Scenario 8: How can `perf` trace kernel function calls without crashing the operating system?
<details>
<summary>Detailed Answer</summary>
`perf` uses kernel static tracepoints and dynamic kprobes. Static tracepoints are lightweight macros pre-compiled into key kernel subsystems (`sched`, `net`, `block`). When disabled, they execute as a single `nop` instruction. When enabled, `perf` replaces the `nop` with a jump to an instrumentation handler. Dynamic kprobes patch the first byte of a target kernel instruction with a breakpoint instruction (`int3` on x86), allowing execution interception with safety checks guaranteed by the kernel probe subsystem.
</details>

---

## Hands-On Practice Labs

### Lab 1: Measure Hardware Instructions and IPC
<details>
<summary>Lab Instructions & Verification</summary>

1. Run `perf stat` on a simple CPU-intensive loop:
   ```bash
   sudo perf stat python3 -c "sum(i*i for i in range(10_000_000))"
   ```
2. Inspect the resulting report:
   - Identify `cycles`, `instructions`, and the computed `insn per cycle` (IPC).
   - Identify `page-faults` and `context-switches`.
</details>

### Lab 2: Real-Time Kernel Function Inspection with perf top
<details>
<summary>Lab Instructions & Verification</summary>

1. Start `perf top` focused only on kernel space symbols:
   ```bash
   sudo perf top -v -U
   ```
2. In a second terminal, trigger disk I/O activity:
   ```bash
   dd if=/dev/zero of=/tmp/test.img bs=1M count=1000 conv=fdatasync
   ```
3. Observe `vfs_write`, `ext4_file_write_iter`, or block layer functions rising to the top.
</details>

### Lab 3: Capture and Inspect a perf.data Recording
<details>
<summary>Lab Instructions & Verification</summary>

1. Record system-wide CPU activity for 5 seconds:
   ```bash
   sudo perf record -F 99 -a -g -- sleep 5
   ```
2. Verify the binary file size:
   ```bash
   ls -lh perf.data
   ```
3. Open the interactive text browser:
   ```bash
   sudo perf report
   ```
   (Navigate the hierarchy using Arrow Keys and Enter; press `q` to exit).
</details>

### Lab 4: Trace Scheduler Context Switches
<details>
<summary>Lab Instructions & Verification</summary>

1. Profile only context-switch software events:
   ```bash
   sudo perf stat -e context-switches,cpu-migrations sleep 2
   ```
2. Verify that context switches are recorded even when CPU utilization is near zero.
</details>

### Lab 5: Discover Available PMU Hardware Events
<details>
<summary>Lab Instructions & Verification</summary>

1. Query all supported hardware events on the current CPU:
   ```bash
   sudo perf list hw
   ```
2. Query supported cache events:
   ```bash
   sudo perf list cache
   ```
</details>

### Lab 6: Clean Up perf Profiling Artifacts
<details>
<summary>Lab Instructions & Verification</summary>

1. Remove any temporary profiling recordings:
   ```bash
   rm -f perf.data perf.data.old /tmp/out.perf /tmp/out.folded
   ```
2. Verify paranoid setting is preserved:
   ```bash
   cat /proc/sys/kernel/perf_event_paranoid
   ```
</details>

---

## Cheat Sheet & Quick Reference

```bash
# === High-Level Counter Metrics ===
sudo perf stat -d <command>                    # Detailed stats (IPC, L1/L3 misses)
sudo perf stat -p <pid> -e cycles,instructions sleep 10

# === Interactive Profiling ===
sudo perf top                                  # System-wide live top functions
sudo perf top -p <pid> -g                      # Live profiling with call graphs

# === Sampling & Stack Recording ===
sudo perf record -F 99 -p <pid> -g -- sleep 10 # Sample process at 99Hz with call graphs
sudo perf record -F 99 -a -g -- sleep 10       # System-wide profiling across all cores
sudo perf report --stdio                       # View plain text report

# === Flame Graph Generation Pipeline ===
sudo perf script > /tmp/out.perf
stackcollapse-perf.pl /tmp/out.perf > /tmp/out.folded
flamegraph.pl /tmp/out.folded > flamegraph.svg
```

---
*Next Note: [05-ebpf-and-bpftrace-intro.md](file:///04-devops/linux/06-advanced/05-ebpf-and-bpftrace-intro.md)*
