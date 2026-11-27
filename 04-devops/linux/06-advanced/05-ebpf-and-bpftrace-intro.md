# Linux eBPF & bpftrace: Kernel Observability & Tracing Architecture

| Metadata | Details |
| :--- | :--- |
| **Topic** | eBPF (Extended Berkeley Packet Filter) Architecture & `bpftrace` Tooling |
| **Module** | `06-advanced` — Advanced Systems & Kernel Mechanics |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS (Kernel 6.8+), RHEL 9.4+ (Kernel 5.14+) |
| **Prerequisites** | `03-advanced-debugging.md`, `04-perf-and-flamegraphs.md`, basic C syntax |
| **Difficulty** | Advanced |
| **Last Updated** | 2026 |

---

## Table of Contents
1. [What & Why: Programmable Kernel Sandboxing](#what--why-programmable-kernel-sandboxing)
2. [Core Concepts & Architecture](#core-concepts--architecture)
3. [Internal Mechanics: Verification, JIT & BPF Maps](#internal-mechanics-verification-jit--bpf-maps)
4. [Command Syntax & bpftrace Reference](#command-syntax--bpftrace-reference)
5. [Progressive Practical Examples](#progressive-practical-examples)
   - [Example 1: Tracing New Process Execution with execsnoop](#example-1-tracing-new-process-execution-with-execsnoop)
   - [Example 2: File Open Latency and Failures with bpftrace One-Liners](#example-2-file-open-latency-and-failures-with-bpftrace-one-liners)
   - [Example 3: Measuring Block I/O Latency Histograms](#example-3-measuring-block-io-latency-histograms)
   - [Example 4: TCP Connect Latency and Connection Tracking](#example-4-tcp-connect-latency-and-connection-tracking)
   - [Example 5: Production Custom bpftrace Anomaly Monitor](#example-5-production-custom-bpftrace-anomaly-monitor)
6. [Production Best Practices & Hardening](#production-best-practices--hardening)
7. [Common Failure Modes & Debugging](#common-failure-modes--debugging)
8. [Performance, Security & Scale Considerations](#performance-security--scale-considerations)
9. [Comparison Matrix: Tracing Technologies](#comparison-matrix-tracing-technologies)
10. [Real-World Interview Scenarios](#real-world-interview-scenarios)
11. [Hands-On Practice Labs](#hands-on-practice-labs)
12. [Cheat Sheet & Quick Reference](#cheat-sheet--quick-reference)

---

## What & Why: Programmable Kernel Sandboxing

Historically, extending the Linux operating system kernel required writing and loading a custom Kernel Module (`.ko`). This was notoriously hazardous: a single null-pointer dereference or infinite loop in a kernel module instantly triggered a Kernel Panic, crashing the host. Conversely, user-space debugging tools like `strace` or `ltrace` incurred severe performance penalties due to constant context switches.

**eBPF (Extended Berkeley Packet Filter)** revolutionizes Linux systems engineering by transforming the kernel into a programmable platform. It allows engineers to load custom sandboxed bytecode directly into the running kernel without modifying source code or rebooting. A rigorous in-kernel **verifier** guarantees safety before execution, and a **Just-In-Time (JIT)** compiler turns the bytecode into native machine instructions executing at bare-metal speed. In modern cloud-native architectures, eBPF powers high-performance observability (Cilium, Datadog), networking (XDP), security enforcement (Falco, Tetragon), and real-time tracing (`bpftrace`, BCC).

---

## Core Concepts & Architecture

```
+-------------------------------------------------------------------------+
|                              USER SPACE                                 |
|                                                                         |
|  +--------------------+    +------------------+    +-----------------+  |
|  | bpftrace / BCC /   |    | Prometheus / OTel|    | Cilium / Falco  |  |
|  | Custom C program   |    | Exporters        |    | Agents          |  |
|  +--------------------+    +------------------+    +-----------------+  |
|             |                       ^                       ^           |
|             | bpf() syscall         |                       |           |
|             v                       +-----------+-----------+           |
|  +--------------------+                         | Reads stats/events    |
|  | eBPF Bytecode      |                         |                       |
|  +--------------------+                         |                       |
+-------------|-----------------------------------|-----------------------+
|             v                                   |                       |
|  +----------------------------------------------|--------------------+  |
|  |                        LINUX KERNEL          |                    |  |
|  |                                              |                    |  |
|  |  +--------------------+                      |                    |  |
|  |  |   In-Kernel        | (Rejects unsafe,     |                    |  |
|  |  |   Verifier         |  unbounded loops)    |                    |  |
|  |  +--------------------+                      |                    |  |
|  |            | Passed                          |                    |  |
|  |            v                                 |                    |  |
|  |  +--------------------+                      v                    |  |
|  |  |   JIT Compiler     | ---> [ eBPF Maps: Hash / Array / Ringbuf ]|  |
|  |  +--------------------+                      ^                    |  |
|  |            | Machine Code                    | Updates data       |  |
|  |            v                                 |                    |  |
|  |  +-------------------------------------------+-----------------+  |  |
|  |  | Attachment Hook Points:                                     |  |  |
|  |  | - Tracepoints: sys_enter_execve, sched_switch               |  |  |
|  |  | - Kprobes / Kretprobes: vfs_read, tcp_v4_connect            |  |  |
|  |  | - Uprobes / Uretprobes: SSL_write (OpenSSL), Go runtime     |  |  |
|  |  | - Network Filters: XDP, tc (Traffic Control), Socket Filter|  |  |
|  |  +-------------------------------------------------------------+  |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

1. **BPF Bytecode**: An assembly-like instruction set composed of 64-bit registers, an arithmetic logic unit, and memory access instructions.
2. **BPF Verifier**: The kernel gatekeeper. It performs static analysis simulating every execution branch to ensure the program never accesses invalid memory, cannot crash the kernel, and completes within bounded execution limits.
3. **BPF Maps**: Key-value data structures residing in kernel memory that allow eBPF programs to store state, maintain histograms, and share aggregated metrics with user-space monitoring daemons.
4. **Hook Points**: Points in execution where an eBPF program is triggered. Includes kernel tracepoints, dynamic kprobes/kretprobes, user-space uprobes, and network interfaces via eXpress Data Path (XDP).

---

## Internal Mechanics: Verification, JIT & BPF Maps

### 1. The Kernel Verification Pipeline
Before an eBPF program can be attached, the `bpf()` system call submits the bytecode to the kernel verifier:
- **DAG Construction**: The verifier constructs a Directed Acyclic Graph (DAG) of all possible code paths.
- **Instruction Limits**: Verifier processes a maximum of 1 million verified instructions (in modern kernels) to prevent denial of service.
- **Loop Bounding**: Modern kernels (5.3+) allow bounded loops provided the verifier can mathematically prove loop termination.
- **Memory Safety**: Pointers are strictly typed and bounds-checked. Dereferencing an arbitrary pointer requires helper functions like `bpf_probe_read_kernel()`.

### 2. JIT (Just-In-Time) Compilation
Once verified, the kernel translates eBPF bytecode instructions directly into host CPU machine code (x86_64, ARM64). This eliminates interpreter overhead:
```bash
# Verify JIT compilation is enabled
sysctl net.core.bpf_jit_enable
# Output: net.core.bpf_jit_enable = 1
```

### 3. BPF Map Types
- `BPF_MAP_TYPE_HASH`: Key/value store for arbitrary data (e.g., tracking connection latency by IP tuple).
- `BPF_MAP_TYPE_ARRAY`: Fixed-size fast indexing arrays.
- `BPF_MAP_TYPE_RINGBUF`: High-throughput, lockless, memory-mapped circular buffer for streaming event logs to user space without dropping events.

---

## Command Syntax & bpftrace Reference

`bpftrace` uses a high-level domain-specific language inspired by awk and C.

### Probe Specification Syntax
```text
probe_type:probe_target /filter_expression/ { action_block; }
```

### Supported Probe Types
| Probe Type | Description | Example |
| :--- | :--- | :--- |
| `tracepoint` | Stable kernel static instrumentation points. | `tracepoint:syscalls:sys_enter_openat` |
| `kprobe` / `kretprobe` | Dynamic kernel function entry / return. | `kprobe:vfs_read`, `kretprobe:tcp_v4_connect` |
| `uprobe` / `uretprobe` | User-space binary dynamic function entry / return. | `uprobe:/usr/bin/bash:readline` |
| `profile` / `interval` | Periodic timer sampling across CPUs or wall clock. | `profile:hz:99`, `interval:s:5` |
| `software` / `hardware`| Kernel or PMU counter events. | `software:faults:1`, `hardware:cache-misses:1000` |

### Built-in Variables & Functions
- `$1, $2`: Script command-line arguments.
- `pid`, `tid`: Process ID and Thread ID.
- `comm`: Process name string.
- `nsecs`: Current time in nanoseconds.
- `elapsed`: Nanoseconds elapsed since script start.
- `retval`: Function return value (available only in `kretprobe` and `uretprobe`).
- `@map[key] = count()`: Frequency counting.
- `@map = hist(val)`: Logarithmic power-of-2 histogram.
- `@map = lhist(val, min, max, step)`: Linear histogram.

---

## Progressive Practical Examples

### Example 1: Tracing New Process Execution with execsnoop

Track every command, binary execution, and shell fork across the entire host in real time.

```bash
#!/usr/bin/env bash
# Step 1: Trace process creation events
set -euo pipefail

# Check if bcc-tools or bpftrace is available
if command -v execsnoop-bpfcc >/dev/null 2>&1; then
    echo "Running execsnoop via BCC tools (Ctrl+C to exit)..."
    sudo execsnoop-bpfcc -T
else
    echo "Running execsnoop using pure bpftrace one-liner..."
    sudo bpftrace -e '
      tracepoint:syscalls:sys_enter_execve {
        printf("%-8s %-6d %-16s %s\n", 
               strftime("%H:%M:%S", nsecs), 
               pid, 
               comm, 
               str(args.filename));
      }
    '
fi
```

### Example 2: File Open Latency and Failures with bpftrace One-Liners

Identify applications encountering missing files or permission errors without running intrusive `strace`.

```bash
#!/usr/bin/env bash
# Step 2: Trace failed file open calls system-wide
set -euo pipefail

echo "Monitoring failed file open attempts (Ctrl+C to stop)..."
sudo bpftrace -e '
  tracepoint:syscalls:sys_exit_openat /args.ret < 0/ {
    printf("PID %-6d (%-16s) failed openat: err=%d\n", pid, comm, args.ret);
    @failures[comm, args.ret] = count();
  }
  END {
    printf("\n=== Failure Summary ===\n");
  }
'
```

### Example 3: Measuring Block I/O Latency Histograms

Generate a real-time logarithmic latency distribution of physical disk reads and writes.

```bash
#!/usr/bin/env bash
# Step 3: Block layer latency histogram
set -euo pipefail

echo "Recording Block I/O latency distribution for 10 seconds..."
sudo bpftrace -e '
  kprobe:blk_account_io_start {
    @start[arg0] = nsecs;
  }
  kprobe:blk_account_io_done /@start[arg0]/ {
    $duration_us = (nsecs - @start[arg0]) / 1000;
    @usecs = hist($duration_us);
    delete(@start[arg0]);
  }
  interval:s:10 {
    exit();
  }
'
```

### Example 4: TCP Connect Latency and Connection Tracking

Measure outbound network connection establishment time and detect slow external dependencies.

```bash
#!/usr/bin/env bash
# Step 4: TCP outbound handshake latency
set -euo pipefail

cat << 'EOF' > /tmp/tcp_latency.bt
#include <net/sock.h>

kprobe:tcp_v4_connect {
  @start[tid] = nsecs;
}

kretprobe:tcp_v4_connect /@start[tid]/ {
  $duration_ms = (nsecs - @start[tid]) / 1000000;
  $sk = (struct sock *)retval;
  printf("%-8s %-16s PID %-6d connect time: %d ms (ret: %d)\n",
         strftime("%H:%M:%S", nsecs), comm, pid, $duration_ms, retval);
  @tcp_lat_ms = hist($duration_ms);
  delete(@start[tid]);
}

interval:s:15 {
  exit();
}
EOF

sudo bpftrace /tmp/tcp_latency.bt
rm -f /tmp/tcp_latency.bt
```

### Example 5: Production Custom bpftrace Anomaly Monitor

A self-contained production health probe that continuously tracks page faults, DNS resolution latency, and high-frequency context switches.

```bash
#!/usr/bin/env bash
# Step 5: Advanced multi-vector eBPF anomaly auditor
set -euo pipefail

REPORT_FILE="/var/log/ebpf_health_report.txt"
DURATION_SEC=15

echo "Starting eBPF health snapshot (${DURATION_SEC}s capture)..."

sudo bpftrace -o "${REPORT_FILE}" -e '
  // Count major and minor page faults per process
  software:page-faults:1 {
    @page_faults[comm] = count();
  }

  // Count context switches per process
  tracepoint:sched:sched_switch {
    @ctx_switches[args.prev_comm] = count();
  }

  // Sample CPU stack on 99Hz frequency
  profile:hz:99 {
    @cpu_stacks[kstack] = count();
  }

  interval:s:'"${DURATION_SEC}"' {
    exit();
  }
'

echo "Trace complete! Report written to ${REPORT_FILE}:"
cat "${REPORT_FILE}" | head -n 30
```

---

## Production Best Practices & Hardening

- **Prefer Static Tracepoints over Dynamic Kprobes**: Tracepoints have a fixed, stable ABI guaranteed by the Linux kernel maintainers across minor versions. Kprobes hook internal C functions whose signatures or existence can change across kernel releases.
- **Always Delete Map Keys in Return Probes**: When calculating latency using hash maps indexed by PID or `tid` (`@start[tid] = nsecs`), always execute `delete(@start[tid])` in the matching return probe to prevent kernel memory exhaustion.
- **Restrict Unprivileged BPF**: Modern kernels disable unprivileged eBPF by default to prevent speculative execution attacks (Spectre). Verify:
  ```bash
  sysctl kernel.unprivileged_bpf_disabled
  # Value must be 1 or 2
  ```
- **Ensure BPF JIT Hardening is Enabled**: Set `net.core.bpf_jit_harden=2` to blind constants in JIT-compiled programs and protect against JIT spraying exploits.
- **Monitor eBPF Overhead with `bpftool`**: Use `bpftool prog show` to inspect runtime statistics (e.g., total execution time, run count) for active eBPF programs on high-throughput servers.

---

## Common Failure Modes & Debugging

| Symptom / Error | Root Cause | Diagnosis Command | Resolution |
| :--- | :--- | :--- | :--- |
| `Permission denied: need CAP_BPF or CAP_SYS_ADMIN` | Unprivileged user executed bpftrace without root credentials. | `id -u` | Execute using `sudo` or grant `CAP_BPF` / `CAP_PERFMON`. |
| `Verifier error: program too large` or `back-edge from insn X to Y` | Bytecode contains unbounded loop or exceeds instruction limits. | Inspect bpftrace verifier log | Simplify loops, bound iteration variables, or upgrade kernel. |
| `bpftrace: /lib/modules/$(uname -r)/build: No such file or directory` | Kernel C headers matching the running kernel are missing. | `ls /lib/modules/$(uname -r)/build` | Install `linux-headers-$(uname -r)` or `kernel-devel`. |
| Dropped events / `lost N events` warning | BPF ring buffer size too small for high-frequency events. | Check bpftrace stderr | Filter events more strictly with predicate filters (`/.../`). |
| Uprobe fails: `cannot find library` | Shared library path resolution failed for target user binary. | `ldd <binary>` | Provide the absolute canonical path to the library or binary. |

---

## Performance, Security & Scale Considerations

```
+--------------------------------------------------------------------------+
| Linux Tracing Performance Cost                                           |
|                                                                          |
| 1. eBPF Static Tracepoint (bpftrace):                                    |
|    ~15-30 nanoseconds per event. In-kernel aggregation via maps.        |
|    Capable of handling > 1,000,000 events/sec safely in production.     |
|                                                                          |
| 2. Dynamic Kprobe / Uprobe:                                              |
|    ~70-150 nanoseconds per event (incurs breakpoint trap).               |
|                                                                          |
| 3. System Call Tracer (strace):                                          |
|    ~2,000-10,000 nanoseconds per event (context switches to user space). |
|    Saturates CPU above 10,000 events/sec.                               |
+--------------------------------------------------------------------------+
```

1. **JIT Spraying Defense**: Malicious programs could theoretically craft constants that, when JIT-compiled into x86 machine instructions, resemble executable shellcode. Linux kernel mitigates this with JIT blinding (`net.core.bpf_jit_harden=2`).
2. **Memory Footprint**: eBPF maps reside in kernel memory (`kmalloc` / `vmalloc`). A map tracking every individual IP address on a 10Gbps link without expiration can trigger an Out-Of-Memory (OOM) panic. Always use LRU maps (`BPF_MAP_TYPE_LRU_HASH`) for unbounded sets.

---

## Comparison Matrix: Tracing Technologies

| Dimension | `bpftrace` | BCC (Python/C) | `perf` | `strace` | Ftrace |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Language** | AWK/C DSL | Python / C | CLI flags | CLI flags | sysfs text interface |
| **Kernel Primitive** | eBPF | eBPF | perf_events | ptrace | Kernel Tracepoints |
| **Aggregation** | In-Kernel (Maps) | In-Kernel (Maps) | Post-process | Post-process | Ring buffer |
| **User Space Probes**| Yes (uprobes) | Yes (uprobes) | Yes | Syscall only | No |
| **Production Impact**| Minimal (~1%) | Minimal (~1%) | Minimal (~1%) | Destructive | Minimal |

---

## Real-World Interview Scenarios

### Scenario 1: Why is eBPF significantly more efficient than `strace` for production observability?
<details>
<summary>Detailed Answer</summary>
`strace` relies on `ptrace(2)`. For every single system call, the kernel pauses the target application, switches context to the tracer process to read registers, and then switches back. This introduces multi-microsecond delays per syscall. In contrast, an eBPF program runs entirely inside the kernel at the hook point via JIT-compiled native code. It filters, aggregates, and updates statistical BPF maps directly in kernel memory, communicating with user space only when a summary is requested or via a lockless ring buffer.
</details>

### Scenario 2: What is the primary role of the eBPF Verifier?
<details>
<summary>Detailed Answer</summary>
The in-kernel verifier is a static analysis engine that proves that submitted eBPF bytecode is safe to run inside kernel space. It validates that:
1. The program cannot dereference uninitialized or arbitrary memory pointers.
2. The program contains no unbounded loops that could hang the CPU core.
3. The program adheres to strict instruction limits (e.g., 1 million instructions).
4. The program only accesses data structures and helpers permitted for its specific program type.
</details>

### Scenario 3: Explain the difference between a `kprobe` and a `tracepoint`.
<details>
<summary>Detailed Answer</summary>
A **tracepoint** is a static instrumentation macro explicitly placed into kernel source code by kernel developers. It has a stable, documented ABI that remains consistent across kernel versions.
A **kprobe** (kernel probe) is a dynamic instrumentation mechanism. It can hook virtually any internal C function address in the kernel by dynamically overwriting the instruction with a breakpoint. However, because internal kernel functions frequently change signatures, are renamed, or get inlined across kernel releases, kprobes are unstable and fragile across kernel updates.
</details>

### Scenario 4: What are BPF Maps and why are they necessary?
<details>
<summary>Detailed Answer</summary>
eBPF programs are stateless, ephemeral handlers invoked on event triggers. Because they have a restricted stack (only 512 bytes), they cannot allocate large arrays or retain state between distinct executions. BPF Maps are kernel-allocated key-value stores (e.g., hash tables, arrays, ring buffers, histograms) that provide persistent storage across event invocations and act as the bidirectional data bridge between the kernel eBPF program and user-space management daemons.
</details>

### Scenario 5: What is XDP (eXpress Data Path) and why is it used?
<details>
<summary>Detailed Answer</summary>
XDP is an eBPF-based high-performance data path that hooks packet processing at the earliest possible point: directly inside the network interface card (NIC) driver before the kernel allocates a socket buffer (`sk_buff`) or passes the packet to the TCP/IP stack. XDP can filter, forward, or drop malicious DDoS packets (`XDP_DROP`) at wire speed (tens of millions of packets per second per core) with negligible CPU overhead.
</details>

### Scenario 6: Why are Frame Pointers or ORC unwinding required when tracing with uprobes and kprobes?
<details>
<summary>Detailed Answer</summary>
To capture call chains (backtraces) leading up to an event, eBPF must unwind the execution stack. When binaries omit frame pointers (`-fomit-frame-pointer`), the kernel cannot easily walk the linked list of stack frames without complex DWARF tables (which the kernel cannot parse safely in real time). Preserving frame pointers (`-fno-omit-frame-pointer`) or leveraging kernel ORC unwinding ensures eBPF can reconstruct call stacks in nanoseconds.
</details>

### Scenario 7: What does the `kretprobe` probe type do, and what is its main caveat?
<details>
<summary>Detailed Answer</summary>
`kretprobe` hooks the return boundary of a kernel function, providing access to the function's return value (`retval`). The primary caveat is that function arguments passed upon entry are no longer accessible at return time (since registers have been overwritten). To correlate entry parameters with return values (such as calculating latency), an engineer must use a `kprobe` to store the start timestamp in a map indexed by thread ID (`tid`), and retrieve/delete it in the corresponding `kretprobe`.
</details>

### Scenario 8: What happens if an eBPF program attempts an unbounded loop on an older kernel?
<details>
<summary>Detailed Answer</summary>
The kernel verifier detects a potential backward jump (back-edge) that could cause an infinite loop and immediately rejects the program during the `bpf()` system call with an error like `back-edge from insn X to Y: program rejected`. On Linux 5.3+, bounded loops with verifiable termination conditions are accepted, but any loop whose bounds cannot be statically determined is rejected.
</details>

---

## Hands-On Practice Labs

### Lab 1: Verify eBPF Subsystem Status and Kernel Config
<details>
<summary>Lab Instructions & Verification</summary>

1. Check kernel configuration for BPF support:
   ```bash
   grep -E "CONFIG_BPF=y|CONFIG_BPF_SYSCALL=y|CONFIG_BPF_JIT=y" /boot/config-$(uname -r)
   ```
2. Check unprivileged BPF status:
   ```bash
   sysctl kernel.unprivileged_bpf_disabled
   ```
</details>

### Lab 2: Inspect Loaded eBPF Programs with `bpftool`
<details>
<summary>Lab Instructions & Verification</summary>

1. List all active eBPF programs currently running in the kernel:
   ```bash
   sudo bpftool prog show
   ```
2. List all active BPF maps:
   ```bash
   sudo bpftool map show
   ```
</details>

### Lab 3: Track Real-Time Bash Commands via Uprobes
<details>
<summary>Lab Instructions & Verification</summary>

1. Run a bpftrace uprobe intercepting the `readline` function inside `/bin/bash`:
   ```bash
   sudo bpftrace -e '
     uprobe:/bin/bash:readline {
       printf("User PID %d invoked bash readline\n", pid);
     }
     uretprobe:/bin/bash:readline {
       printf("Command executed: %s\n", str(retval));
     }
   '
   ```
2. Open another terminal and execute a command (e.g., `uptime`). Observe the captured command string.
</details>

### Lab 4: Count System Calls by Process Name
<details>
<summary>Lab Instructions & Verification</summary>

1. Execute a 5-second trace counting all system call entries grouped by binary name:
   ```bash
   sudo bpftrace -e '
     tracepoint:raw_syscalls:sys_enter {
       @calls[comm] = count();
     }
     interval:s:5 {
       exit();
     }
   '
   ```
2. Review the resulting sorted frequency table.
</details>

### Lab 5: Profile Kernel CPU Stacks at 49 Hz
<details>
<summary>Lab Instructions & Verification</summary>

1. Sample kernel backtraces across all CPUs for 3 seconds:
   ```bash
   sudo bpftrace -e '
     profile:hz:49 {
       @[kstack] = count();
     }
     interval:s:3 {
       exit();
     }
   '
   ```
2. Identify which kernel subsystems (`arch_cpu_idle`, `poll`, etc.) appear most frequently.
</details>

### Lab 6: Measure Read System Call Data Size
<details>
<summary>Lab Instructions & Verification</summary>

1. Generate a power-of-two histogram of bytes returned by `sys_exit_read`:
   ```bash
   sudo bpftrace -e '
     tracepoint:syscalls:sys_exit_read /args.ret > 0/ {
       @read_bytes = hist(args.ret);
     }
     interval:s:5 {
       exit();
     }
   '
   ```
2. Verify the distribution of read buffer sizes.
</details>

---

## Cheat Sheet & Quick Reference

```bash
# === Essential bpftrace One-Liners ===
# Trace new processes system-wide:
sudo bpftrace -e 'tracepoint:syscalls:sys_enter_execve { printf("%s -> %s\n", comm, str(args.filename)); }'

# Syscall counts by program:
sudo bpftrace -e 'tracepoint:raw_syscalls:sys_enter { @[comm] = count(); }'

# Read byte size histogram:
sudo bpftrace -e 'tracepoint:syscalls:sys_exit_read /args.ret > 0/ { @bytes = hist(args.ret); }'

# Disk I/O size histogram:
sudo bpftrace -e 'tracepoint:block:block_rq_issue { @bytes = hist(args.bytes); }'

# Kernel function execution count:
sudo bpftrace -e 'kprobe:vfs_* { @[probe] = count(); }'

# === BCC Tools Quick Reference ===
execsnoop-bpfcc                                # Trace process executions
opensnoop-bpfcc                                # Trace openat() syscalls
biolatency-bpfcc -m                            # Disk latency histogram (ms)
tcpconnect-bpfcc                               # Trace outbound TCP handshakes
tcpretrans-bpfcc                               # Trace TCP retransmissions
```

---
*Next Note: [06-kernel-tuning-and-network-stack-tuning.md](file:///04-devops/linux/06-advanced/06-kernel-tuning-and-network-stack-tuning.md)*
