# Linux Kernel Tracing: Ftrace, Function Graphs & SystemTap

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Kernel Tracing: Ftrace (`tracefs`), Function Graph Tracers & SystemTap |
| **Module** | `06-advanced` — Advanced Systems & Kernel Mechanics |
| **Track** | `04-devops/linux` |
| **Track OS** | Ubuntu 24.04 LTS (Kernel 6.8+), RHEL 9.4+ (Kernel 5.14+) |
| **Prerequisites** | `03-advanced-debugging.md`, `05-ebpf-and-bpftrace-intro.md` |
| **Difficulty** | Advanced |
| **Last Updated** | 2026 |

---

## Table of Contents
1. [What & Why: Low-Level Kernel Execution Flow](#what--why-low-level-kernel-execution-flow)
2. [Core Concepts & Architecture](#core-concepts--architecture)
3. [Internal Mechanics: Ftrace MCOUNT & SystemTap Kernel Modules](#internal-mechanics-ftrace-mcount--systemtap-kernel-modules)
4. [Command Syntax & Tooling Reference](#command-syntax--tooling-reference)
5. [Progressive Practical Examples](#progressive-practical-examples)
   - [Example 1: Tracing Kernel Functions via tracefs Interface](#example-1-tracing-kernel-functions-via-tracefs-interface)
   - [Example 2: Visualizing Call Hierarchies with the Function Graph Tracer](#example-2-visualizing-call-hierarchies-with-the-function-graph-tracer)
   - [Example 3: Easy Multi-Core Tracing with trace-cmd](#example-3-easy-multi-core-tracing-with-trace-cmd)
   - [Example 4: Writing a Basic SystemTap Instrumentation Script](#example-4-writing-a-basic-systemtap-instrumentation-script)
   - [Example 5: Production Kernel Call Latency Profiler Script](#example-5-production-kernel-call-latency-profiler-script)
6. [Production Best Practices & Hardening](#production-best-practices--hardening)
7. [Common Failure Modes & Debugging](#common-failure-modes--debugging)
8. [Performance, Security & Scale Considerations](#performance-security--scale-considerations)
9. [Comparison Matrix: Kernel Tracing Technologies](#comparison-matrix-kernel-tracing-technologies)
10. [Real-World Interview Scenarios](#real-world-interview-scenarios)
11. [Hands-On Practice Labs](#hands-on-practice-labs)
12. [Cheat Sheet & Quick Reference](#cheat-sheet--quick-reference)

---

## What & Why: Low-Level Kernel Execution Flow

Before modern eBPF gained ubiquity, Linux system engineers and kernel developers needed dependable methods to inspect execution paths inside the kernel without recompiling it or installing bulky third-party drivers. When a database query blocks in the VFS layer or network packet processing stalls during high throughput, engineers must follow the chain of internal kernel function calls step by step.

**Ftrace (Function Tracer)** is the official in-kernel tracing framework built into every standard Linux kernel. Controlled entirely through the virtual filesystem (`/sys/kernel/tracing` or `/sys/kernel/debug/tracing`), Ftrace requires zero external dependencies, making it universally available even on minimal rescue images, embedded systems, and bare-metal environments. When paired with **`trace-cmd`** or **SystemTap**, engineers can profile function execution durations, generate ASCII function call graphs, and pinpoint lock latencies.

---

## Core Concepts & Architecture

```
+-------------------------------------------------------------------------+
|                              USER SPACE                                 |
|                                                                         |
|  +--------------------+    +------------------+    +-----------------+  |
|  | trace-cmd CLI      |    | SystemTap (stap) |    | Echo / Cat via  |  |
|  | (record / report)  |    | Translator / C   |    | /sys/.../tracing|  |
|  +--------------------+    +------------------+    +-----------------+  |
|             |                       |                       |           |
+-------------|-----------------------|-----------------------|-----------+
|             |                       v                       |           |
|             |               Generated Kernel Module         |           |
|             |               (insmod stap_xxxx.ko)           |           |
|             v                       |                       v           |
|  +-------------------------------------------------------------------+  |
|  |                         LINUX KERNEL                              |  |
|  |                                                                   |  |
|  |  +-------------------------------------------------------------+  |  |
|  |  |                     Ftrace Framework                        |  |  |
|  |  |  - Function Tracer (trace every C function entry)           |  |  |
|  |  |  - Function Graph Tracer (draw call tree + execution time)   |  |  |
|  |  |  - Tracepoint Engine (sched, irq, block events)             |  |  |
|  |  +-------------------------------------------------------------+  |  |
|  |                                |                                  |  |
|  |                                v                                  |  |
|  |  +-------------------------------------------------------------+  |  |
|  |  |                   Per-CPU Ring Buffers                      |  |  |
|  |  |        /sys/kernel/tracing/per_cpu/cpu*/trace_pipe          |  |  |
|  |  +-------------------------------------------------------------+  |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

1. **`tracefs` Virtual Filesystem**: Mounted at `/sys/kernel/tracing` (or `/sys/kernel/debug/tracing`). Configuration is performed by writing strings (`echo`) to control files, and results are read via `cat trace`.
2. **Dynamic Ftrace & `mcount`/`fentry`**: The compiler (`gcc -pg`) inserts a call to `__fentry__` at the entry point of every kernel function. At boot, the kernel replaces these calls with `nop` (no-operation) instructions. When Ftrace is enabled, targeted `nop` instructions are dynamically hot-patched with jumps to the tracer!
3. **Function Graph Tracer**: Intercepts both the entry and exit of functions, calculating the exact execution time spent inside each function and formatting the output as an indented C-style call tree.
4. **SystemTap**: A scripting language that translates high-level probe scripts into C source code, compiles them into a custom kernel module (`.ko`), and loads them into the kernel.

---

## Internal Mechanics: Ftrace MCOUNT & SystemTap Kernel Modules

### 1. Dynamic Ftrace Hot-Patching Mechanics
Unlike software breakpoints that cause processor trap exceptions (`int3` on x86), Dynamic Ftrace uses code-patching:
1. At compilation time, GCC places 5-byte `call __fentry__` stubs at the start of every kernel function.
2. During the kernel boot sequence, the kernel sweeps memory and overwrites every stub with a 5-byte `nop` instruction.
3. When you run `echo sys_openat > set_ftrace_filter`, the kernel dynamically patches only the `sys_openat` function's `nop` with a jump instruction to `ftrace_caller`.
4. As a result, when tracing is inactive, Ftrace introduces **zero CPU overhead**!

### 2. The Function Graph Return Trampoline
To measure elapsed execution time, the function graph tracer patches the function's return address on the execution stack. When the function returns, CPU control jumps to an Ftrace return trampoline handler, which records the end timestamp, computes duration, restores the original return address, and continues normal execution.

---

## Command Syntax & Tooling Reference

### Core tracefs Control Files (`/sys/kernel/tracing/`)
| Control File | Functionality |
| :--- | :--- |
| `available_tracers` | Lists all supported tracing plugins (e.g., `function`, `function_graph`, `nop`). |
| `current_tracer` | Activates a tracer plugin (e.g., `echo function_graph > current_tracer`). |
| `tracing_on` | Master switch (`1` = active, `0` = paused). |
| `set_ftrace_filter` | Restricts tracing to specific kernel function names (supports wildcards `ext4_*`). |
| `set_ftrace_pid` | Restricts tracing to a specific process ID. |
| `trace` | Displays recorded trace buffer in human-readable ASCII. |
| `trace_pipe` | Live streaming pipe; reading consumes events from the ring buffer. |

### trace-cmd CLI Subcommands
| Command | Primary Flags | Purpose |
| :--- | :--- | :--- |
| `trace-cmd record` | `-p <tracer>`, `-F` (follow process), `-P <pid>`, `-e <event>` | Records kernel trace data into `trace.dat`. |
| `trace-cmd report` | `-i <file>`, `--cpu <n>` | Decodes and prints events recorded in `trace.dat`. |
| `trace-cmd stream` | `-p function_graph -l <func>` | Streams live graph output directly to terminal. |
| `trace-cmd list` | `-t` (tracers), `-e` (events), `-f` (functions) | Enumerates available kernel hooks. |

---

## Progressive Practical Examples

### Example 1: Tracing Kernel Functions via tracefs Interface

Directly configure Ftrace via sysfs without any third-party tools installed.

```bash
#!/usr/bin/env bash
# Step 1: Manual tracefs configuration
set -euo pipefail

TRACEDIR="/sys/kernel/tracing"
if [[ ! -d "${TRACEDIR}" ]]; then
    TRACEDIR="/sys/kernel/debug/tracing"
fi

# 1. Reset and disable tracing
echo 0 | sudo tee "${TRACEDIR}/tracing_on" > /dev/null
echo nop | sudo tee "${TRACEDIR}/current_tracer" > /dev/null
echo > "${TRACEDIR}/set_ftrace_filter"

# 2. Configure function tracer for VFS reads
echo "vfs_read" | sudo tee "${TRACEDIR}/set_ftrace_filter" > /dev/null
echo "function" | sudo tee "${TRACEDIR}/current_tracer" > /dev/null

# 3. Clear existing buffer and enable
echo | sudo tee "${TRACEDIR}/trace" > /dev/null
echo 1 | sudo tee "${TRACEDIR}/tracing_on" > /dev/null

sleep 2

# 4. Disable and view results
echo 0 | sudo tee "${TRACEDIR}/tracing_on" > /dev/null
echo "Captured VFS Read Calls:"
sudo head -n 25 "${TRACEDIR}/trace"
```

### Example 2: Visualizing Call Hierarchies with the Function Graph Tracer

Render an ASCII execution graph showing internal call hierarchies and latency durations.

```bash
#!/usr/bin/env bash
# Step 2: Function Graph Call Tree
set -euo pipefail

TRACEDIR="/sys/kernel/tracing"

# Configure function_graph for do_sys_openat2
sudo bash -c "
  echo 0 > ${TRACEDIR}/tracing_on
  echo function_graph > ${TRACEDIR}/current_tracer
  echo do_sys_openat2 > ${TRACEDIR}/set_graph_function
  echo 1 > ${TRACEDIR}/tracing_on
"

# Trigger an open operation
cat /etc/issue > /dev/null

# Disable and read graph
sudo bash -c "echo 0 > ${TRACEDIR}/tracing_on"

echo "=== Kernel Function Call Graph ==="
sudo cat "${TRACEDIR}/trace" | grep -A 20 "do_sys_openat2" | head -n 30

# Expected Output Format:
#  0)               |  do_sys_openat2() {
#  0)               |    getname_flags() {
#  0)   0.210 us    |      kmem_cache_alloc();
#  0)   0.840 us    |    }
#  0)   2.140 us    |  }
```

### Example 3: Easy Multi-Core Tracing with trace-cmd

Use the user-friendly `trace-cmd` CLI to capture and analyze traces without manipulating files manually.

```bash
#!/usr/bin/env bash
# Step 3: Record and report with trace-cmd
set -euo pipefail

if ! command -v trace-cmd >/dev/null 2>&1; then
    sudo apt-get install -y trace-cmd || sudo dnf install -y trace-cmd
fi

echo "Recording socket connection graph for 3 seconds..."
sudo trace-cmd record -p function_graph -g __sys_connect -l __sys_connect sleep 3

echo "=== Analyzing Captured Events ==="
sudo trace-cmd report | head -n 35

# Cleanup
rm -f trace.dat
```

### Example 4: Writing a Basic SystemTap Instrumentation Script

Demonstrate SystemTap's syntax for intercepting kernel and user-space calls.

```bash
#!/usr/bin/env bash
# Step 4: SystemTap script example
set -euo pipefail

# SystemTap requires kernel debuginfo packages
cat << 'EOF' > /tmp/monitor_sync.stp
# SystemTap script to trace all sync() calls across the system
probe kernel.function("sys_sync") {
    printf("%s[%d] called sys_sync at %s\n", execname(), pid(), ctime(gettimeofday_s()));
}

probe timer.s(10) {
    exit();
}
EOF

echo "SystemTap script created at /tmp/monitor_sync.stp"
echo "To execute (with kernel-debuginfo installed): sudo stap /tmp/monitor_sync.stp"
```

### Example 5: Production Kernel Call Latency Profiler Script

An automated diagnostic utility that profiles a user-supplied kernel function and flags any invocation exceeding 10 microseconds.

```bash
#!/usr/bin/env bash
# Step 5: Automated Kernel Latency Outlier Detector
set -euo pipefail

TARGET_FUNC="${1:-ext4_file_write_iter}"
DURATION="${2:-5}"
THRESHOLD_US="${3:-10}"

TRACEDIR="/sys/kernel/tracing"
if [[ ! -d "${TRACEDIR}" ]]; then TRACEDIR="/sys/kernel/debug/tracing"; fi

echo "=================================================="
echo "    KERNEL FUNCTION LATENCY PROFILER"
echo "    Function: ${TARGET_FUNC} | Duration: ${DURATION}s"
echo "=================================================="

# Check if target function exists in available filter list
if ! grep -qw "${TARGET_FUNC}" "${TRACEDIR}/available_filter_functions"; then
    echo "Error: Kernel function '${TARGET_FUNC}' not found in available_filter_functions."
    exit 1
fi

# Reset and configure
sudo bash -c "
  echo 0 > ${TRACEDIR}/tracing_on
  echo nop > ${TRACEDIR}/current_tracer
  echo > ${TRACEDIR}/set_ftrace_filter
  echo > ${TRACEDIR}/set_graph_function
  echo '${TARGET_FUNC}' > ${TRACEDIR}/set_graph_function
  echo function_graph > ${TRACEDIR}/current_tracer
  echo > ${TRACEDIR}/trace
  echo 1 > ${TRACEDIR}/tracing_on
"

echo "Tracing active... Sleeping ${DURATION} seconds..."
sleep "${DURATION}"

# Stop tracing
sudo bash -c "echo 0 > ${TRACEDIR}/tracing_on"

echo "=== Invocations Exceeding ${THRESHOLD_US} us ==="
# Ftrace marks functions taking > 10us with '+' and > 100us with '!'
sudo cat "${TRACEDIR}/trace" | grep -E "(\+|!)[ ]+[0-9\.]+[ ]+us" | head -n 30 || echo "No slow calls detected."

# Reset tracer to clean state
sudo bash -c "
  echo nop > ${TRACEDIR}/current_tracer
  echo > ${TRACEDIR}/set_graph_function
"
echo "Profiler teardown complete."
```

---

## Production Best Practices & Hardening

- **Always Reset Tracers Upon Completion**: Leaving `current_tracer` set to `function` or `function_graph` system-wide introduces continuous ring-buffer writes and consumes memory. Always write `echo nop > current_tracer` and `echo 0 > tracing_on` when finished.
- **Narrow Your Filters**: Never run `current_tracer = function` without setting `set_ftrace_filter`. Tracing every single kernel function across all 64 cores simultaneously will overwhelm the CPU and fill the ring buffer in milliseconds.
- **Set Buffer Size Appropriately**: Control per-CPU ring buffer capacity via `/sys/kernel/tracing/buffer_size_kb`. Increase to `16384` (16MB) per core during high-frequency traces to avoid dropped events.
- **Filter by PID for Targeted Debugging**: When diagnosing an application daemon, always restrict Ftrace to that process using `echo $PID > set_ftrace_pid`.
- **SystemTap Security Considerations**: SystemTap compiles arbitrary C code into root-level kernel modules. Never allow untrusted users to execute `stap` without the SystemTap compile server / client architecture (`stap-client`).

---

## Common Failure Modes & Debugging

| Symptom / Error | Root Cause | Diagnosis Command | Resolution |
| :--- | :--- | :--- | :--- |
| `cat: /sys/kernel/tracing/trace: Device or resource busy` | A consumer process is actively reading `trace_pipe` simultaneously. | `lsof /sys/kernel/tracing/trace_pipe` | Terminate other tracing consumers (`trace-cmd`, custom scripts). |
| `echo: write error: Invalid argument` when setting filter | Function name does not exist, was inlined, or is marked `notrace`. | `grep <func> available_filter_functions` | Verify the function name exists in `available_filter_functions`. |
| Missing events / `CPU 0: [LOST N EVENTS]` | Ring buffer filled faster than user space could read it. | `cat /sys/kernel/tracing/trace` | Increase `buffer_size_kb` or narrow filters with `set_ftrace_filter`. |
| SystemTap fails: `missing kernel-devel` | Kernel headers or DWARF debuginfo missing for running kernel. | `uname -r` | Install matching `kernel-devel` (RHEL) or `linux-headers-$(uname -r)` (Ubuntu). |
| System freezes during full `function` trace | Tracing unconstrained without filters on high-throughput server. | Check console messages | Always specify filters in `set_ftrace_filter` prior to enabling tracing. |

---

## Performance, Security & Scale Considerations

```
+--------------------------------------------------------------------------+
| Kernel Tracing Mechanisms Comparison                                      |
|                                                                          |
| 1. Dynamic Ftrace (nop hot-patching):                                    |
|    - Disabled overhead: Exactly 0% (5-byte NOPs).                        |
|    - Active overhead: ~2% - 5% (with targeted function filters).         |
|    - Safe for bare-metal & rescue systems (zero package dependencies).   |
|                                                                          |
| 2. SystemTap (.ko compilation):                                          |
|    - Incurs overhead of compiling and loading kernel modules.            |
|    - Risk of kernel panic if C script contains invalid memory logic.     |
|                                                                          |
| 3. eBPF / bpftrace:                                                      |
|    - Verified memory safety + in-kernel aggregation via BPF maps.        |
|    - Modern standard for production continuous observability.            |
+--------------------------------------------------------------------------+
```

1. **Memory Allocation in Tracing**: Ftrace writes to pre-allocated, per-CPU lockless ring buffers. It never calls `kmalloc` in the trace path, ensuring it is safe to trace low-level memory allocation routines without deadlocks.
2. **Access Control**: Writing to `/sys/kernel/tracing` requires `CAP_SYS_ADMIN` or root privileges. Protect permissions to prevent unprivileged users from sniffing kernel data.

---

## Comparison Matrix: Kernel Tracing Technologies

| Dimension | Ftrace (`tracefs`) | `trace-cmd` | SystemTap | eBPF (`bpftrace`) |
| :--- | :--- | :--- | :--- | :--- |
| **Interface** | Filesystem (`/sys/...`) | CLI Tool | Scripting language | DSL / CLI |
| **Dependencies** | None (Built into kernel)| `trace-cmd` package | GCC, Make, Debuginfo | LLVM, Clang, Headers|
| **Safety** | High (In-kernel hotpatch)| High | Medium (Custom `.ko`)| Highest (In-kernel verifier)|
| **Call Graphs**| Yes (ASCII call tree) | Yes | Manual recursion | Manual recursion |
| **In-Kernel Aggregation**| No (Raw event dump) | No | Yes (Variables/Arrays)| Yes (BPF Maps) |

---

## Real-World Interview Scenarios

### Scenario 1: Why is Ftrace considered the ultimate fallback tool for kernel debugging?
<details>
<summary>Detailed Answer</summary>
Ftrace is compiled directly into almost every production Linux kernel (`CONFIG_FUNCTION_TRACER=y`, `CONFIG_FUNCTION_GRAPH_TRACER=y`) and is controlled purely through the standard VFS (`sysfs`/`tracefs`) using basic shell utilities like `echo` and `cat`. It requires no compilers, no Python runtimes, no kernel headers, and no external package installations. On minimal bare-metal nodes, locked-down containers, or single-user rescue environments, Ftrace is guaranteed to work out of the box.
</details>

### Scenario 2: How does Dynamic Ftrace achieve zero overhead when inactive?
<details>
<summary>Detailed Answer</summary>
When the kernel is compiled with `-pg -mfentry`, GCC inserts a 5-byte instruction stub at the start of every function. During the boot sequence, Ftrace scans the kernel text segment and overwrites each of these stubs with a 5-byte architecture-specific NOP (`nop` instruction on x86). Because executing a NOP consumes virtually zero CPU cycles and does not alter registers, Ftrace introduces negligible overhead until a user explicitly writes a function name into `set_ftrace_filter`, at which point only that specific function's NOP is hot-patched into a jump instruction.
</details>

### Scenario 3: What is the function graph tracer and how does it measure latency?
<details>
<summary>Detailed Answer</summary>
The Function Graph Tracer tracks both entry and exit of kernel functions. When a function is entered, it hijacks the return address on the execution stack and replaces it with the address of a kernel "return trampoline" function, while recording the entry timestamp in a per-thread ring buffer. When the traced function returns, the CPU executes the trampoline, which calculates the elapsed duration ($\Delta t = t_{\text{exit}} - t_{\text{entry}}$), restores the original return address, and resumes normal execution, generating an indented ASCII call hierarchy.
</details>

### Scenario 4: What is the difference between `/sys/kernel/tracing/trace` and `trace_pipe`?
<details>
<summary>Detailed Answer</summary>
`/sys/kernel/tracing/trace` provides a static, human-readable snapshot of the current contents of the ring buffer without consuming the data. Reading from it multiple times displays the same recorded events. In contrast, `trace_pipe` is an interactive streaming FIFO interface. Reading from `trace_pipe` blocks until new events arrive and consumes them permanently from the ring buffer, preventing buffer overflow during continuous live tracing.
</details>

### Scenario 5: What is the main drawback of SystemTap compared to modern eBPF?
<details>
<summary>Detailed Answer</summary>
SystemTap translates user scripts into C code and compiles them into a loadable kernel module (`.ko`) using host compilers. This approach requires installing multi-gigabyte `kernel-debuginfo` packages matching the exact running kernel version. Furthermore, because it loads an unverified kernel module, a programming error or bad memory dereference in the script can cause a fatal kernel panic, whereas eBPF programs are verified for safety before execution and require no kernel modules.
</details>

### Scenario 6: How can Ftrace be scoped to trace only a specific PID?
<details>
<summary>Detailed Answer</summary>
By writing the target process ID into `/sys/kernel/tracing/set_ftrace_pid`. Ftrace will then filter events so that only system calls, context switches, and kernel functions executed on behalf of that specific PID (or its children if `function-fork` option is enabled) are recorded into the trace buffer.
</details>

### Scenario 7: What do the `+` and `!` annotations mean in Function Graph Tracer output?
<details>
<summary>Detailed Answer</summary>
In the Function Graph Tracer output, execution duration is printed alongside each function return. Ftrace includes visual warning flags for slow executions:
- A plus sign (`+`) indicates that the function took longer than **10 microseconds**.
- An exclamation mark (`!`) indicates that the function took longer than **100 microseconds**.
These markers allow engineers to visually scan large trace outputs for latency spikes.
</details>

### Scenario 8: What is `trace-cmd` and why is it preferred over raw `tracefs` commands?
<details>
<summary>Detailed Answer</summary>
`trace-cmd` is a user-space command-line front-end for Ftrace. While configuring Ftrace through `tracefs` requires multiple manual `echo` operations, handles multi-core CPU buffers awkwardly, and can be cumbersome to automate, `trace-cmd` simplifies the workflow into clear commands (`record`, `report`, `stream`), handles per-CPU binary buffer merging automatically, and packages captures into portable `trace.dat` files for offline analysis.
</details>

---

## Hands-On Practice Labs

### Lab 1: Verify tracefs Mounting and Available Tracers
<details>
<summary>Lab Instructions & Verification</summary>

1. Confirm `tracefs` is mounted:
   ```bash
   mount | grep tracefs
   ```
2. Check available tracers:
   ```bash
   cat /sys/kernel/tracing/available_tracers
   ```
   (Verify presence of `function`, `function_graph`, and `nop`).
</details>

### Lab 2: Trace Syscall Executions with trace-cmd
<details>
<summary>Lab Instructions & Verification</summary>

1. Record system-wide process execve events:
   ```bash
   sudo trace-cmd record -e sched:sched_process_exec sleep 3
   ```
2. Report the captured executions:
   ```bash
   sudo trace-cmd report
   ```
3. Clean up:
   ```bash
   rm -f trace.dat
   ```
</details>

### Lab 3: Filter Kernel Functions with Wildcards
<details>
<summary>Lab Instructions & Verification</summary>

1. Query how many TCP functions are available for tracing:
   ```bash
   grep "^tcp_" /sys/kernel/tracing/available_filter_functions | wc -l
   ```
2. Set a wildcard filter to trace only TCP v4 functions:
   ```bash
   echo 'tcp_v4_*' | sudo tee /sys/kernel/tracing/set_ftrace_filter
   cat /sys/kernel/tracing/set_ftrace_filter | head -n 10
   ```
3. Reset filter:
   ```bash
   echo > /sys/kernel/tracing/set_ftrace_filter
   ```
</details>

### Lab 4: Stream Real-Time Kernel Events via trace_pipe
<details>
<summary>Lab Instructions & Verification</summary>

1. Configure function tracing for `do_sys_openat2`:
   ```bash
   sudo bash -c "
     echo nop > /sys/kernel/tracing/current_tracer
     echo do_sys_openat2 > /sys/kernel/tracing/set_ftrace_filter
     echo function > /sys/kernel/tracing/current_tracer
     echo 1 > /sys/kernel/tracing/tracing_on
   "
2. Stream events live for 3 seconds:
   ```bash
   timeout 3 sudo cat /sys/kernel/tracing/trace_pipe || true
   ```
3. Reset:
   ```bash
   sudo bash -c "echo 0 > /sys/kernel/tracing/tracing_on; echo nop > /sys/kernel/tracing/current_tracer"
   ```
</details>

### Lab 5: Inspect Per-CPU Trace Buffer Allocation
<details>
<summary>Lab Instructions & Verification</summary>

1. Inspect active buffer size per CPU:
   ```bash
   cat /sys/kernel/tracing/buffer_size_kb
   ```
2. Double the buffer size to 4MB:
   ```bash
   echo 4096 | sudo tee /sys/kernel/tracing/buffer_size_kb
   cat /sys/kernel/tracing/buffer_size_kb
   ```
</details>

### Lab 6: Clean Up All Active Ftrace Settings
<details>
<summary>Lab Instructions & Verification</summary>

1. Execute a comprehensive Ftrace reset:
   ```bash
   sudo bash -c "
     echo 0 > /sys/kernel/tracing/tracing_on
     echo nop > /sys/kernel/tracing/current_tracer
     echo > /sys/kernel/tracing/set_ftrace_filter
     echo > /sys/kernel/tracing/set_graph_function
     echo > /sys/kernel/tracing/trace
   "
2. Verify that `current_tracer` is `nop` and `tracing_on` is `0`.
</details>

---

## Cheat Sheet & Quick Reference

```bash
# === Quick Ftrace Workflow (/sys/kernel/tracing/) ===
echo 0 > tracing_on                            # Stop tracing
echo nop > current_tracer                      # Reset tracer
echo <function_name> > set_ftrace_filter       # Set target filter
echo function > current_tracer                 # Enable function tracer (or function_graph)
echo 1 > tracing_on                            # Start tracing
cat trace                                      # View results
echo 0 > tracing_on && echo nop > current_tracer # Always cleanup!

# === Function Graph Tracer ===
echo function_graph > current_tracer
echo do_sys_openat2 > set_graph_function
cat trace                                      # Renders ASCII call tree with latencies

# === trace-cmd High-Frequency CLI ===
trace-cmd record -p function -l vfs_read sleep 5
trace-cmd record -p function_graph -g __sys_connect sleep 5
trace-cmd report                               # Analyze trace.dat
trace-cmd list -f | grep ext4                  # Search available functions
```

---
*Module 06 Complete. Proceed to Module 07: Production Reference Projects.*
