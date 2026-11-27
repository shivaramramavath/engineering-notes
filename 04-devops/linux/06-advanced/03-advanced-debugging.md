# Advanced Linux Debugging: System Tracing, Core Dumps & Process Inspection

| Metadata | Details |
| :--- | :--- |
| **Topic** | Advanced Linux Debugging: Tracing, Memory Dumps, and Kernel Inspection |
| **Module** | `06-advanced` — Advanced Systems & Kernel Mechanics |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS (Kernel 6.8+), RHEL 9.4+ (Kernel 5.14+) |
| **Prerequisites** | `03-system/01-processes-and-signals.md`, `05-production/05-troubleshooting.md` |
| **Difficulty** | Advanced |
| **Last Updated** | 2026 |

---

## Table of Contents
1. [What & Why: Observability Beyond Log Files](#what--why-observability-beyond-log-files)
2. [Core Concepts & Architecture](#core-concepts--architecture)
3. [Internal Mechanics: ptrace and /proc Virtual Filesystem](#internal-mechanics-ptrace-and-proc-virtual-filesystem)
4. [Command Syntax & Diagnostic Options Reference](#command-syntax--diagnostic-options-reference)
5. [Progressive Practical Examples](#progressive-practical-examples)
   - [Example 1: Tracing File & Network Syscalls with strace](#example-1-tracing-file--network-syscalls-with-strace)
   - [Example 2: Dynamic Library Interception with ltrace](#example-2-dynamic-library-interception-with-ltrace)
   - [Example 3: Inspecting Frozen Processes via /proc/#/wchan and stack](#example-3-inspecting-frozen-processes-via-procwchan-and-stack)
   - [Example 4: Generating and Analyzing Crash Core Dumps with GDB](#example-4-generating-and-analyzing-crash-core-dumps-with-gdb)
   - [Example 5: Non-Invasive Production Process Diagnostic Script](#example-5-non-invasive-production-process-diagnostic-script)
6. [Production Best Practices & Hardening](#production-best-practices--hardening)
7. [Common Failure Modes & Debugging](#common-failure-modes--debugging)
8. [Performance, Security & Scale Considerations](#performance-security--scale-considerations)
9. [Comparison Matrix: Debugging & Tracing Utilities](#comparison-matrix-debugging--tracing-utilities)
10. [Real-World Interview Scenarios](#real-world-interview-scenarios)
11. [Hands-On Practice Labs](#hands-on-practice-labs)
12. [Cheat Sheet & Quick Reference](#cheat-sheet--quick-reference)

---

## What & Why: Observability Beyond Log Files

Application logs and metrics dashboards report that an error or latency spike occurred; they rarely explain the underlying kernel or operating system mechanism responsible for the blockage. When a microservice hangs indefinitely in production without throwing an exception, or throws a cryptic `EACCES` / `ECONNREFUSED` error despite seemingly correct configurations, engineers must inspect process execution at the boundary between user space and the Linux kernel.

Advanced Linux debugging employs system call tracing (`strace`), user-space shared library call tracing (`ltrace`), memory and stack analysis (`gdb`, core dumps), and kernel wait-channel inspection (`/proc/<pid>/wchan`, `/proc/<pid>/stack`). These tools provide definitive, incontrovertible visibility into thread execution, locking deadlocks, leaked file descriptors, and CPU stalls.

---

## Core Concepts & Architecture

```
+-------------------------------------------------------------------------+
|                              USER SPACE                                 |
|                                                                         |
|  +--------------------+                     +------------------------+  |
|  |  Target Process    |                     | Debugger / Tracer      |  |
|  |  (e.g., Nginx, Go) |                     | (strace, ltrace, GDB)  |  |
|  +--------------------+                     +------------------------+  |
|             |                                            ^              |
|   glibc / musl call                                      |              |
|             v                                            | ptrace(2)    |
|  +--------------------+                                  | control      |
|  |  Library Functions | <--- ltrace hooks (.plt entries) |              |
|  +--------------------+                                  |              |
|             |                                            |              |
|      syscall boundary (SYSENTER / SYSCALL)               |              |
+-------------|--------------------------------------------|---------------+
|             v                                            |              |
|  +-------------------------------------------------------+-----------+  |
|  |                         LINUX KERNEL                              |  |
|  |                                                                   |  |
|  |   Syscall Entry Trap -> ptrace_stop() notifies tracer             |  |
|  |   Syscall Dispatch: sys_openat(), sys_futex(), sys_connect()      |  |
|  |   Syscall Return Trap -> ptrace_stop() notifies tracer            |  |
|  |                                                                   |  |
|  |   Virtual Filesystems:                                            |  |
|  |   /proc/<pid>/wchan  (kernel sleeping symbol)                     |  |
|  |   /proc/<pid>/stack  (kernel thread backtrace)                    |  |
|  |   /proc/<pid>/maps   (virtual memory mapping ranges)              |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

1. **System Call Boundary**: Applications request OS resources (disk, network, memory mapping, thread synchronization) through system calls. `strace` traps entry and exit for these calls.
2. **Dynamic Linker & PLT**: Applications compiled dynamically resolve library symbols (e.g., `malloc()`, `SSL_read()`, `pthread_mutex_lock()`) through the Procedure Linkage Table (PLT). `ltrace` rewrites PLT breakpoint opcodes to intercept user-space function invocations.
3. **Wait Channels (`wchan`)**: When a thread sleeps in the `D` (Uninterruptible Sleep) or `S` (Interruptible Sleep) state, the kernel stores the name of the function where the task went to sleep in `/proc/<pid>/wchan`.
4. **Kernel Stack (`/proc/<pid>/stack`)**: Displays the in-kernel execution call stack of a sleeping thread, pinpointing blocked mutexes, socket waits, or NFS IO deadlocks.

---

## Internal Mechanics: ptrace and /proc Virtual Filesystem

The primary engine behind `strace` and `gdb` is the `ptrace(2)` system call. When a tracer attaches to a target:
1. **Attachment**: The tracer invokes `ptrace(PTRACE_ATTACH, target_pid)`. The kernel sends `SIGSTOP` to the target.
2. **Syscall Interception**: Tracer calls `ptrace(PTRACE_SYSCALL, ...)`. The kernel resumes the target process until either:
   - It is about to execute a system call (`PTRACE_SYSCALL_ENTER`).
   - The system call has executed and is about to return to user space (`PTRACE_SYSCALL_EXIT`).
3. **Context Switch Overhead**: At each boundary, the target is halted, triggering context switches to the tracer process to inspect registers (`orig_rax`, `rdi`, `rsi` on x86_64). This introduces significant CPU and latency overhead (often 2x to 10x slowdown).
4. **YAMA Security Scope**: Modern Linux kernels protect processes against arbitrary ptrace eavesdropping via `/proc/sys/kernel/yama/ptrace_scope`:
   - `0` (Classic): Any process can trace any process owned by the same UID.
   - `1` (Restricted default): A process can only ptrace its direct child processes. Root (`sudo`) is required to trace unrelated daemons.
   - `2` (Admin only): Only processes with `CAP_SYS_PTRACE` can attach.
   - `3` (Locked): No process can use `ptrace`. Cannot be unset without reboot.

---

## Command Syntax & Diagnostic Options Reference

| Command / Tool | Primary Options | Functionality & Target Use |
| :--- | :--- | :--- |
| `strace` | `-p <pid>` | Attach to running process PID. |
| | `-f` | Follow all child processes and spawned threads (`clone`/`fork`). |
| | `-e trace=<set>` | Filter syscalls (`-e trace=network`, `-e trace=openat,read,write`). |
| | `-T` | Show elapsed time spent inside each system call. |
| | `-tt` / `-ttt` | Prefix each line with microsecond wall-clock time or UNIX epoch. |
| | `-c` | Summarize time, calls, and errors in a tabular report on exit. |
| | `-s <size>` | Maximum string size printed (default is 32 bytes; use `-s 256` or `-s 1024`). |
| `ltrace` | `-p <pid>` | Attach and intercept user-space shared library calls (`.so`). |
| | `-e <sym>` | Filter specific library symbols (e.g., `-e malloc+free`). |
| `gdb` | `-p <pid>` | Attach GNU Debugger to live process. |
| | `<binary> <core>` | Open an offline core dump with the corresponding unstripped binary. |
| `cat /proc/<pid>/...` | `wchan` | Returns kernel symbol where process is currently blocked. |
| | `stack` | Returns raw kernel backtrace of the thread. |
| | `status` | Reports process state, VmRSS, VmSize, Voluntary/Nonvoluntary ctxsw. |

---

## Progressive Practical Examples

### Example 1: Tracing File & Network Syscalls with strace

Diagnose why a daemon fails to initialize its database configuration without altering source code.

```bash
#!/usr/bin/env bash
# Step 1: Trace file opens and network socket connections
set -euo pipefail

TARGET_PID=$(pgrep -f "api-service" | head -n 1)

if [[ -z "${TARGET_PID}" ]]; then
    echo "Process not running, executing standalone trace..."
    sudo strace -f -tt -T -s 512 \
      -e trace=openat,connect,read,write \
      -o /tmp/api_startup.strace \
      /usr/local/bin/api-service --config /etc/api/config.json
else
    echo "Attaching to running PID ${TARGET_PID}..."
    # Trace for 10 seconds or until Ctrl+C
    sudo strace -p "${TARGET_PID}" -f -tt -T -s 512 \
      -e trace=openat,connect,sendto,recvfrom \
      -o /tmp/api_live.strace
fi

# Analyze captured errors (EACCES, ENOENT, ECONNREFUSED)
grep -E "ENOENT|EACCES|ECONNREFUSED" /tmp/api_*.strace | head -n 20
```

### Example 2: Dynamic Library Interception with ltrace

Pinpoint cryptographic handshakes or memory allocation bottlenecks in dynamic libraries.

```bash
#!/usr/bin/env bash
# Step 2: Trace dynamic library calls using ltrace
set -euo pipefail

# Trace SSL handshake calls in curl
ltrace -e "SSL_*" -o /tmp/curl_ssl.ltrace curl -s https://example.com > /dev/null

echo "Captured SSL Library calls:"
cat /tmp/curl_ssl.ltrace | head -n 15
```

### Example 3: Inspecting Frozen Processes via /proc/#/wchan and stack

Inspect an unresponsive process stuck in uninterruptible sleep (`D` state) without incurring ptrace overhead.

```bash
#!/usr/bin/env bash
# Step 3: Zero-overhead kernel sleep inspection
set -euo pipefail

PID="${1:-1}"

echo "=== Inspecting Process PID: ${PID} ==="

# 1. State check
STATE=$(awk '/State:/ {print $2, $3}' "/proc/${PID}/status")
echo "State: ${STATE}"

# 2. Kernel wait channel
echo -n "Kernel Wait Channel: "
cat "/proc/${PID}/wchan"
echo ""

# 3. Kernel Call Stack (Requires root)
echo "=== Kernel Call Stack ==="
sudo cat "/proc/${PID}/stack"

# 4. Open File Descriptors and Sockets
echo "=== Top Open File Descriptors ==="
sudo ls -l "/proc/${PID}/fd" | head -n 15
```

### Example 4: Generating and Analyzing Crash Core Dumps with GDB

Configure Linux systemd and the kernel to capture full process memory dumps when an application crashes with `SIGSEGV` or `SIGABRT`.

```bash
#!/usr/bin/env bash
# Step 4: Core dump configuration and analysis
set -euo pipefail

# 1. Enable unlimited core file size for current shell
ulimit -c unlimited

# 2. Configure kernel core dump pattern to /var/crash
echo "/var/crash/core.%e.%p.%t" | sudo tee /proc/sys/kernel/core_pattern > /dev/null
sudo mkdir -p /var/crash
sudo chmod 1777 /var/crash

# 3. Simulate a crash with a minimal C program or abort signal
cat << 'EOF' > /tmp/crash_demo.c
#include <stdio.h>
#include <stdlib.h>

void broken_function() {
    int *ptr = NULL;
    *ptr = 42; // Triggers SIGSEGV
}

int main() {
    broken_function();
    return 0;
}
EOF

gcc -g /tmp/crash_demo.c -o /tmp/crash_demo
/tmp/crash_demo || true

# Find generated core dump
CORE_FILE=$(ls -t /var/crash/core.crash_demo.* 2>/dev/null | head -n 1)

if [[ -f "${CORE_FILE}" ]]; then
    echo "Found core file: ${CORE_FILE}"
    # 4. Load core dump into GDB to print stack trace
    gdb -batch -ex "bt" -ex "quit" /tmp/crash_demo "${CORE_FILE}"
fi
```

### Example 5: Non-Invasive Production Process Diagnostic Script

A robust, multi-threaded diagnostic script designed for production environments to snapshot thread states, memory limits, and file handles without halting critical workloads.

```bash
#!/usr/bin/env bash
# Step 5: Non-invasive production process health audit
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <target_pid>"
    exit 1
fi

TARGET_PID="$1"

if [[ ! -d "/proc/${TARGET_PID}" ]]; then
    echo "Error: Process PID ${TARGET_PID} does not exist."
    exit 1
fi

REPORT_FILE="/tmp/proc_diag_${TARGET_PID}_$(date +%s).txt"
exec > >(tee -a "${REPORT_FILE}") 2>&1

echo "=================================================="
echo "    PROCESS DIAGNOSTIC SNAPSHOT: PID ${TARGET_PID}"
echo "    Generated: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
echo "=================================================="

echo "--- Basic Info ---"
cat "/proc/${TARGET_PID}/comm"
cat "/proc/${TARGET_PID}/cmdline" | tr '\0' ' ' ; echo ""

echo "--- Memory Status ---"
grep -E "VmSize|VmRSS|VmSwap|Threads" "/proc/${TARGET_PID}/status"

echo "--- Thread States & Kernel Wait Channels ---"
printf "%-10s %-15s %-25s\n" "TID" "STATE" "WCHAN"
for task in /proc/${TARGET_PID}/task/*; do
    TID=$(basename "${task}")
    T_STATE=$(awk '/State:/ {print $2}' "${task}/status")
    T_WCHAN=$(cat "${task}/wchan" 2>/dev/null || echo "N/A")
    printf "%-10s %-15s %-25s\n" "${TID}" "${T_STATE}" "${T_WCHAN}"
done

echo "--- Socket & Network Connections ---"
sudo ss -tupna | grep "pid=${TARGET_PID}," || echo "No active network sockets found."

echo "--- File Descriptors Utilization ---"
FD_COUNT=$(ls -1 "/proc/${TARGET_PID}/fd" | wc -l)
MAX_FD=$(awk '/Max open files/ {print $4}' "/proc/${TARGET_PID}/limits")
echo "Open FDs: ${FD_COUNT} / Limit: ${MAX_FD}"

echo "=================================================="
echo "Diagnostic complete. Report saved to: ${REPORT_FILE}"
```

---

## Production Best Practices & Hardening

- **Do NOT run unconstrained `strace` on production workloads**: `strace` halts threads twice per system call. For heavy I/O services (PostgreSQL, Redis, Envoy), this can degrade performance by 80%+ and trigger cluster health check timeouts. Use `perf` or `bpftrace` instead.
- **Always filter syscalls**: When using `strace` in live staging or troubleshooting pods, always specify filters like `-e trace=openat,connect,nanosleep` to avoid capturing millions of repetitive `read`/`write` buffer loops.
- **Enforce restricted `yama.ptrace_scope`**: Ensure `/proc/sys/kernel/yama/ptrace_scope` is set to `1` or `2` on production hosts to prevent compromised non-root accounts from using `ptrace` to scrape encryption keys or tokens from other processes.
- **Manage Core Dump Storage via systemd-coredump**: Do not leave unlimited core dumps written to root filesystems. Use `/etc/systemd/coredump.conf` with `Storage=external`, `MaxUse=5G`, and `ProcessSizeMax=2G` to avoid disk exhaustion.
- **Sanitize Core Dumps and Trace Files**: Traces and memory dumps contain plaintext database passwords, customer tokens, and memory buffers. Never export unredacted core dumps to external bug trackers.

---

## Common Failure Modes & Debugging

| Symptom / Error | Root Cause | Diagnosis Command | Resolution |
| :--- | :--- | :--- | :--- |
| `strace: attach: ptrace(PTRACE_SEIZE, ...): Operation not permitted` | YAMA ptrace scope prevents attaching to process not spawned by tracer. | `cat /proc/sys/kernel/yama/ptrace_scope` | Execute with `sudo` (`CAP_SYS_PTRACE`) or lower ptrace scope for debugging session. |
| Process hangs in `D` state; `kill -9` fails | Process is in Uninterruptible Sleep waiting for hardware/NFS I/O. | `cat /proc/<pid>/wchan` | Resolve underlying storage/NFS mount or network blockage; reboot host if NFS lock deadlocked. |
| Core dump file is 0 bytes (`empty`) | Shell or systemd process resource limit set to 0. | `ulimit -c` | Set `ulimit -c unlimited` in user profile or set `LimitCORE=infinity` in systemd unit file. |
| Application crashes during `strace` | Tracer interrupted via `SIGKILL`, or timing-sensitive lock timed out under latency. | Check `dmesg -T` | Use non-invasive eBPF tracing (`bpftrace`) or sample using `perf`. |
| Missing debugging symbols in GDB (`?? ()`) | Application binary was stripped (`strip` command) of DWARF debug info. | `file /usr/bin/target` | Install debuginfo/dbgsym packages (e.g., `apt install <pkg>-dbgsym`). |

---

## Performance, Security & Scale Considerations

```
+--------------------------------------------------------------------------+
| Observability Overhead Comparison                                        |
|                                                                          |
| 1. /proc inspection (wchan, stack, status)                               |
|    Overhead: < 0.1% | Safe for high-throughput production                 |
|                                                                          |
| 2. Linux perf & eBPF (bpftrace)                                          |
|    Overhead: ~1% - 3% | Ring-buffer kernel tracing, safe for production  |
|                                                                          |
| 3. strace / ltrace / GDB (ptrace)                                        |
|    Overhead: 50% - 900% | CPU context switches per syscall/instruction    |
+--------------------------------------------------------------------------+
```

1. **Kernel Lock Contention**: Attaching GDB or strace stops all threads in a multi-threaded JVM or Go binary. In clustered distributed systems (Raft/Paxos, etcd, Consul), stopping a node for > 1000ms causes cluster quorum heartbeats to fail, triggering node evictions.
2. **Core Dump Disk I/O Throttling**: A 64 GB heap Java application crashing on a shared production SSD can saturate host block I/O write queues for tens of seconds while generating the core file. Use `systemd-coredump` compression (`lz4`/`zstd`).

---

## Comparison Matrix: Debugging & Tracing Utilities

| Feature / Metric | `strace` | `ltrace` | `/proc` Filesystem | `gdb` | `perf` / `bpftrace` |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Inspection Level** | System Calls | Shared Libs (.so) | Kernel internals | Machine instructions | Kernel/User Events |
| **Kernel Primitive** | `ptrace` | Breakpoints / PLT | Virtual VFS | `ptrace` / DWARF | eBPF / PMU Counters |
| **Performance Impact**| Extremely High | High | Near Zero | Extremely High | Extremely Low |
| **Requires Symbols?**| No | No | No | Yes (for symbols) | Optional |
| **Safe for Prod Load?**| No (use sparingly)| No | Yes | No | Yes |

---

## Real-World Interview Scenarios

### Scenario 1: Why does `kill -9` fail to terminate a process in state `D`?
<details>
<summary>Detailed Answer</summary>
The state `D` stands for Uninterruptible Sleep (`TASK_UNINTERRUPTIBLE`). In this state, a thread is waiting directly on a hardware device or synchronous kernel driver subsystem (most commonly blocked disk I/O, synchronous network filesystems like NFS/Ceph, or kernel mutex locks). The kernel intentionally ignores all signals—including `SIGKILL` (`9`)—while a task is in this state to preserve internal filesystem and driver data consistency. Once the hardware device responds or times out, the task transitions to runnable and processes the pending termination signal.
</details>

### Scenario 2: How does `ptrace` cause substantial performance degradation in `strace`?
<details>
<summary>Detailed Answer</summary>
When a process runs under `ptrace(PTRACE_SYSCALL)`, the CPU triggers a trap to kernel mode upon every system call entry and exit. The kernel halts the target process, saves its hardware registers, issues a `SIGTRAP`, and schedules a context switch to the tracer process (`strace`). The tracer reads the registers via `ptrace(PTRACE_GETREGSET)` and writes to stdout or disk. Another context switch is then required to resume the target. This turns a fast 50-nanosecond system call into a multi-microsecond ordeal across 4 context switches.
</details>

### Scenario 3: Explain the role of `/proc/sys/kernel/yama/ptrace_scope`.
<details>
<summary>Detailed Answer</summary>
YAMA is a Linux Security Module (LSM) that restricts `ptrace` capabilities. By default on many distributions (setting `1`), a process can only trace its own descendants (children created via `fork`/`exec`). This prevents malware executing under a compromised user account from attaching to other running processes (like SSH agents, Web browsers, or password managers) owned by the same user to extract session keys or inject malicious shellcode.
</details>

### Scenario 4: What is the significance of the `futex` system call appearing in `strace`?
<details>
<summary>Detailed Answer</summary>
`futex` (Fast Userspace Mutex) is the Linux synchronization primitive used by threading runtimes (such as `libpthread`, Go runtime, and Java JVM). When a lock is uncontended, synchronization occurs purely in user space via atomic CPU instructions without entering the kernel. If `futex()` shows up heavily in `strace` (often with `FUTEX_WAIT`), it indicates lock contention: a thread failed to acquire the lock in user space and was forced to invoke the kernel to put itself to sleep until woken by `FUTEX_WAKE`.
</details>

### Scenario 5: How can you discover why a binary throws "Permission Denied" when file permissions are `777`?
<details>
<summary>Detailed Answer</summary>
Execute `strace -e trace=openat,access <command>`. Observe the exact system call returning `-1 EACCES` or `-1 EPERM`. Potential causes include:
1. Missing execute (`+x`) permissions on one of the parent directories along the path.
2. Security modules (SELinux denying access due to incorrect security contexts, or AppArmor profiles blocking access).
3. Filesystem mounted with `noexec` or `ro` flags.
4. Linux immutable file attributes set via `chattr +i`.
</details>

### Scenario 6: What is the difference between an application core dump and a kernel crash dump (kdump)?
<details>
<summary>Detailed Answer</summary>
An application core dump is a memory image of a single user-space process saved to disk by the kernel when the process terminates abnormally (e.g., `SIGSEGV`, `SIGBUS`, `SIGABRT`). A kernel crash dump (kdump) captures the entire physical memory of the operating system kernel when the kernel panics. Kdump boots a secondary "capture kernel" via `kexec` to write the crash dump memory image without relying on the crashed kernel's compromised filesystem drivers.
</details>

### Scenario 7: What does `/proc/<pid>/wchan` displaying `do_sys_poll` or `epoll_wait` indicate?
<details>
<summary>Detailed Answer</summary>
It indicates that the process is an event-driven server (such as Nginx, Node.js, Redis, or Netty) idling normally while waiting for network socket activity. The thread has blocked inside the kernel's `epoll` subsystem awaiting incoming TCP packets or client connections, consuming zero CPU cycles while idle.
</details>

### Scenario 8: How do you extract environment variables from a running process without using tools like `env`?
<details>
<summary>Detailed Answer</summary>
By reading `/proc/<pid>/environ`. In Linux, the kernel exposes the exact environment string array passed to the process at invocation time via this virtual file. Because entries are separated by null bytes (`\0`), inspect them using:
`cat /proc/<pid>/environ | tr '\0' '\n'`
</details>

---

## Hands-On Practice Labs

### Lab 1: Pinpoint Hidden Config File Reads with strace
<details>
<summary>Lab Instructions & Verification</summary>

1. Run standard `man` or `date` under strace and filter for file opens:
   ```bash
   strace -e trace=openat date
   ```
2. Identify which timezone configuration file is opened:
   Notice calls opening `/etc/localtime` or `/usr/share/zoneinfo/`.
</details>

### Lab 2: Analyze Thread Sleep Channels in /proc
<details>
<summary>Lab Instructions & Verification</summary>

1. Start a sleep command in the background:
   ```bash
   sleep 300 &
   BG_PID=$!
   ```
2. Read its wait channel:
   ```bash
   cat "/proc/${BG_PID}/wchan"
   ```
   (Output displays `hr_timer_nanosleep` or `do_nanosleep`).
3. Terminate the process:
   ```bash
   kill "${BG_PID}"
   ```
</details>

### Lab 3: Summarize Syscall Statistics with `strace -c`
<details>
<summary>Lab Instructions & Verification</summary>

1. Execute a command that performs heavy filesystem reads under statistical profiling:
   ```bash
   strace -c find /usr -name "*.so" > /dev/null 2>&1
   ```
2. Review the resulting summary table showing total calls, errors, time percentages, and system call distributions (`newfstatat`, `getdents64`, `close`).
</details>

### Lab 4: Debug Dynamic Library Linking with `LD_DEBUG`
<details>
<summary>Lab Instructions & Verification</summary>

1. Run a binary while requesting the dynamic linker (`ld.so`) to trace symbol resolution:
   ```bash
   LD_DEBUG=libs ls -l /tmp
   ```
2. Observe how the kernel loader locates and binds `libc.so.6`, `libselinux.so.1`, etc.
</details>

### Lab 5: Intercept Process Memory Maps with `/proc/<pid>/maps`
<details>
<summary>Lab Instructions & Verification</summary>

1. Inspect memory mappings of your active shell:
   ```bash
   cat /proc/$$/maps | head -n 25
   ```
2. Identify virtual memory addresses assigned to the heap (`[heap]`), execution stack (`[stack]`), and mapped shared libraries.
</details>

### Lab 6: Trigger and Catch a Segment Violation with Core Dumps
<details>
<summary>Lab Instructions & Verification</summary>

1. Configure temporary core dump dumping:
   ```bash
   ulimit -c unlimited
   ```
2. Send an explicit abort signal to a disposable background process:
   ```bash
   sleep 100 &
   TEST_PID=$!
   kill -SIGABRT "${TEST_PID}"
   ```
3. Inspect `dmesg -T` or `coredumpctl list` (on systemd systems) to confirm core generation.
</details>

---

## Cheat Sheet & Quick Reference

```bash
# === strace One-Liners ===
strace -p <pid> -f -tt -T -o /tmp/trace.log    # Full trace of process & threads
strace -p <pid> -e trace=network -s 512        # Trace network calls with larger string dump
strace -c ls -la /var                          # Performance & call count summary
strace -e trace=openat,access <command>        # Discover file access attempts & missing configs

# === /proc Virtual Filesystem ===
cat /proc/<pid>/wchan                          # Name of kernel function where thread sleeps
cat /proc/<pid>/stack                          # Kernel call stack (requires root)
cat /proc/<pid>/cmdline | tr '\0' ' '          # Full command line arguments
cat /proc/<pid>/environ | tr '\0' '\n'         # Process environment variables
ls -l /proc/<pid>/fd                           # Inspect open file descriptors & sockets

# === GDB Core Dump Commands ===
gdb /path/to/binary /path/to/core              # Open core dump
(gdb) bt                                       # Print backtrace of active thread
(gdb) thread apply all bt                      # Print backtrace of all threads
(gdb) info registers                           # Inspect CPU hardware registers
```

---
*Next Note: [04-perf-and-flamegraphs.md](file:///04-devops/linux/06-advanced/04-perf-and-flamegraphs.md)*
