# Linux Production Troubleshooting: Methodologies, Triage, and Diagnostics

| Metadata | Details |
| :--- | :--- |
| **Topic** | Systematic Linux Troubleshooting, USE/RED Methodologies, Brendan Gregg 60-Second Triage, Subsystem Diagnosis |
| **Module** | `05-production` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `03-system/01-processes-and-signals.md`, `03-system/07-storage-and-filesystems.md`, `04-networking/01-networking-basics.md` |
| **Target Audience** | Site Reliability Engineers (SREs), Production Engineers, Systems Architects |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools** | `uptime`, `dmesg`, `vmstat`, `mpstat`, `pidstat`, `iostat`, `free`, `sar`, `ss`, `top` / `htop` |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [Internal Working](#internal-working)
4. [Syntax & CLI Tools](#syntax--cli-tools)
5. [Progressive Practical Examples](#progressive-practical-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance, Security & Cost Implications](#performance-security--cost-implications)
9. [Comparison Table / Alternatives](#comparison-table--alternatives)
10. [Real-World Troubleshooting & Interview Q&A](#real-world-troubleshooting--interview-qa)
11. [Hands-On Practice Exercises](#hands-on-practice-exercises)
12. [Summary Cheat Sheet & Further Reading](#summary-cheat-sheet--further-reading)

---

## What & Why

When a mission-critical production service experiences degraded performance, elevated error rates, or complete unresponsiveness, inexperienced engineers often succumb to the "anti-methodology" of random guessing: rebooting nodes blindly, restarting random daemons, or tweaking arbitrary sysctl parameters. This frequently destroys critical in-memory forensic evidence, causes cascading failovers, and prolongs outages.

Professional Site Reliability Engineering requires a **disciplined, repeatable triage methodology**. By systematically evaluating:
- **The USE Method (Utilization, Saturation, Errors)** across every hardware and virtual resource (CPUs, Memory, Storage, Network).
- **The RED Method (Rate, Errors, Duration)** across application request flows.
- **Brendan Gregg's 60-Second Performance Triage Checklist** using standard core OS utilities.

An engineer can isolate whether an incident is driven by user-space CPU starvation, kernel lock contention (`%sys`), disk I/O wait (`%iowait`), memory pressure and OOM eviction, hypervisor CPU steal (`%steal`), or network drop exhaustion in under two minutes.

---

## Core Concepts

### The USE Method (Resource-Oriented)
Pioneered by Brendan Gregg for all physical and virtual system resources:
- **Utilization**: The percentage of time that the resource was busy servicing work (e.g., 90% disk busy time, 95% CPU active time).
- **Saturation**: The degree to which the resource has extra work queued that cannot be serviced (e.g., CPU run queue length $> \text{cores}$, disk queue depth, network backlog drops).
- **Errors**: Count of error events recorded by the subsystem (e.g., disk I/O read errors, NIC dropped packets, ECC memory errors).

### The RED Method (Request-Oriented)
Pioneered by Tom Wilkie for microservices and API architectures:
- **Rate**: The number of requests served per second (throughput).
- **Errors**: The number of failed requests per second (HTTP 5xx, timeouts).
- **Duration**: The latency distribution of requests (specifically p90, p99, and p99.9 tail latencies).

### CPU Breakdown Categories in Linux
CPU time is accounted into specific buckets:
- **`%usr` (User Space)**: Time spent executing unprivileged application code (Java, Python, Node.js, C++).
- **`%sys` (Kernel / System)**: Time spent executing kernel code on behalf of applications (system calls, context switching, memory page allocation, software interrupts). High `%sys` indicates excessive syscalls or lock contention.
- **`%iowait`**: CPU idle time spent waiting for an outstanding disk or NFS I/O request to complete. Indicates disk subsystem bottlenecks.
- **`%steal`**: Time stolen from a virtual machine by the underlying hypervisor (KVM, AWS Xen/Nitro, VMware) to service other VMs. High `%steal` (> 5%–10%) indicates an **over-subscribed, noisy neighbor host**.
- **`%si` (Software Interrupt)**: Time spent processing kernel softirqs (predominantly network packet ingestion).

### The Linux Load Average Demystified
The three numbers reported by `uptime` (`load average: 2.15, 1.80, 1.20`) represent exponentially damped moving averages over 1, 5, and 15 minutes.
- In Linux, Load Average counts processes in two distinct states:
  1. **`TASK_RUNNING` (`R`)**: Actively executing on CPU or waiting in the runnable CPU queue.
  2. **`TASK_UNINTERRUPTIBLE` (`D`)**: Blocked waiting for an uninterruptible event (usually disk I/O, NFS locks, or hardware operations).
- *Rule of Thumb*: If Load Average exceeds the number of physical CPU cores, the system is saturated. If load is high but `%usr` and `%sys` are low, **disk I/O (`D` state processes) is the primary bottleneck**.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                               Brendan Gregg 60-Second Triage                            |
+-----------------------------------------------------------------------------------------+
| Step 1:  uptime                 -> Is load average climbing? Is it > CPU core count?   |
| Step 2:  dmesg -T | tail        -> OOM killer invoked? Hardware/disk errors?            |
| Step 3:  vmstat 1 5             -> Are processes blocked (b > 0)? Is memory swapping?   |
| Step 4:  mpstat -P ALL 1 1      -> Are individual CPU cores saturated? (Hot cores)     |
| Step 5:  pidstat 1 3            -> Which specific PID/thread consumes CPU or memory?    |
| Step 6:  iostat -xz 1 3         -> Is disk saturated (%util > 80%)? High await latency? |
| Step 7:  free -h                -> Total available memory? Buffers vs Cache vs Swap?    |
| Step 8:  sar -n DEV 1 3         -> Network interface saturation (rxkB/s, txkB/s)?       |
| Step 9:  sar -n TCP,ETCP 1 3    -> TCP retransmissions, passive/active opens, resets?   |
| Step 10: top                    -> Correlate top consumers, verify states (R vs D)      |
+-----------------------------------------------------------------------------------------+
```

### The System Metrics Extraction Flow
All performance diagnostic tools read directly from the Linux `/proc` and `/sys` virtual filesystems:
- `vmstat`, `uptime` -> `/proc/loadavg`, `/proc/stat`, `/proc/vmstat`
- `iostat` -> `/proc/diskstats`
- `free` -> `/proc/meminfo`
- `pidstat`, `top` -> `/proc/<pid>/stat`, `/proc/<pid>/status`, `/proc/<pid>/wchan`
- `sar -n DEV` -> `/proc/net/dev`

Because these virtual files are generated in-memory by kernel structures, querying them is non-intrusive and safe on heavily loaded production systems.

---

## Syntax & CLI Tools

### The 10 Essential Triage Commands
```bash
# 1. Check system uptime and 1/5/15 minute load averages
uptime

# 2. Check kernel ring buffer for hardware failures, segfaults, and OOM kills
sudo dmesg -T --level=emerg,alert,crit,err | tail -n 30

# 3. Process, memory, paging, and CPU high-level overview
vmstat 1 5

# 4. Per-core CPU breakdown (identifies single-threaded bottlenecks)
mpstat -P ALL 1 3

# 5. Per-process CPU, memory, and thread metrics
pidstat 1 3
pidstat -d 1 3   # Per-process disk I/O metrics

# 6. Detailed storage device metrics, await latency, and queue saturation
iostat -xz 1 3

# 7. Memory breakdown (Used, Free, Available, Buffers/Cache, Swap)
free -h

# 8. Network throughput and packet counters per interface
sar -n DEV 1 3

# 9. TCP connection rates and retransmissions
sar -n TCP,ETCP 1 3

# 10. Process overview sorted by CPU or Memory
top -b -n 1 | head -n 30
```

---

## Progressive Practical Examples

### Example 1: Diagnosing High Load Driven by Disk I/O Wait (`%iowait`)
**Symptom**: Server load average is `18.5` on an 8-core server. Application latency is spiking.

```bash
# 1. Run vmstat to inspect run queues and blocked processes
vmstat 1 3
# Output:
# procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu-----
#  r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st
#  1  9      0 120450 142010 412090    0    0 85200 42100 1240 3120  5  4  2 89  0
```
**Analysis**:
- `b = 9`: 9 processes are stuck in **uninterruptible sleep** waiting for I/O!
- `wa = 89%`: CPU is spending 89% of its time completely idle, blocked on waiting for disk I/O.
- `bi = 85200`: Extremely high blocks read per second.

```bash
# 2. Identify the saturated storage device via iostat
iostat -xz 1 2
# Output:
# Device    r/s    w/s   rkB/s   wkB/s  await r_await w_await  aqu-sz  %util
# sdb     1200.0  450.0 85200.0 42100.0  42.50   50.10   22.10   12.40  99.80
```
- `sdb` is at **`99.80% %util`** (completely saturated).
- `await` latency is **42.50 ms** (abnormally slow for NVMe/SSD, indicating heavy queuing).

```bash
# 3. Locate the specific process generating the I/O using pidstat
sudo pidstat -d 1 2
# Output reveals: PID 4512 (postgres) is writing 42000 kB/s due to an unindexed query.
```

### Example 2: Diagnosing Single-Core CPU Saturation (The Node.js / Python Problem)
**Symptom**: An 8-core server shows low overall CPU usage (`top` reports only 12.5% CPU used), but API endpoints are failing with 504 Gateway Timeouts.

```bash
# 1. Inspect per-core CPU breakdown
mpstat -P ALL 1 1
# Output:
# CPU    %usr   %nice    %sys %iowait    %irq   %soft  %steal  %guest   %idle
# all   12.50    0.00    0.20    0.00    0.00    0.00    0.00    0.00   87.30
#   0    0.10    0.00    0.00    0.00    0.00    0.00    0.00    0.00   99.90
#   1  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00  <-- HOT CORE!
#   2    0.00    0.00    0.10    0.00    0.00    0.00    0.00    0.00   99.90
# ...
```
**Analysis**:
- CPU core 1 is pinned at **100.00% `%usr`**, while all other cores are 100% idle!
- Single-threaded application runtimes (Node.js, Python, Ruby) cannot scale across multiple cores without process clustering (e.g. Node.js `cluster` mode or Gunicorn workers).
- Run `pidstat -p $(pgrep node) 1 1` to confirm the specific single-threaded worker saturating core 1.

### Example 3: Investigating an Out-Of-Memory (OOM) Killer Event
**Symptom**: A critical application daemon (e.g. Elasticsearch or Redis) vanished suddenly without any entry in its application logs.

```bash
# 1. Search kernel ring buffer for OOM killer invocations
sudo dmesg -T | grep -i -E "killed process|out of memory|oom-killer"
```
**Sample Output**:
```text
[Tue Nov 26 10:20:15 2026] Out of memory: Killed process 3842 (java) total-vm:8452140kB, anon-rss:6210450kB, file-rss:0kB, shmem-rss:0kB, UID:1002 pgtables:16800kB oom_score_adj:0
[Tue Nov 26 10:20:15 2026] oom_reaper: reaped process 3842 (java), now anon-rss:0kB, file-rss:0kB, shmem-rss:0kB
```
**Analysis**:
- The Linux kernel OOM killer was triggered.
- Process `3842` (`java`) was consuming ~6.2 GB of anonymous RSS memory when the system ran out of physical memory and swap.
- The kernel terminated the process with `SIGKILL` (`kill -9`), preventing the daemon from writing an orderly shutdown entry to its own log.

### Example 4: Diagnosing Kernel Lock Contention and High `%sys`
**Symptom**: Web server CPU shows 70% `%sys` and low `%usr`. High context switching.

```bash
# 1. Inspect context switches with vmstat
vmstat 1 3
# Notice: cs (context switches) > 150,000/sec, in (interrupts) > 80,000/sec

# 2. Inspect threads and context switching with pidstat
sudo pidstat -w 1 2
# Look for cswch/s (voluntary) and nvcswch/s (involuntary context switches)
```
- High involuntary context switches (`nvcswch/s`): More threads are competing for CPU than physical cores exist; threads are forcibly preempted by the scheduler.
- Profile kernel functions using `perf top` to isolate which kernel locks (e.g. `futex`, `spin_lock`) are spinning.

### Example 5: Production 60-Second Automated Triage Script
An automated triage collector that snapshots complete system telemetry during high-priority incidents.

```bash
#!/usr/bin/env bash
set -euo pipefail

OUTDIR="/var/log/incident_triage_$(date +%Y%m%d_%H%M%S)"
mkdir -p "${OUTDIR}"

echo "==> Collecting 60-Second Linux Performance Triage..."

uptime > "${OUTDIR}/01_uptime.txt"
dmesg -T | tail -n 50 > "${OUTDIR}/02_dmesg.txt"
vmstat 1 5 > "${OUTDIR}/03_vmstat.txt" &
mpstat -P ALL 1 5 > "${OUTDIR}/04_mpstat.txt" &
iostat -xz 1 5 > "${OUTDIR}/05_iostat.txt" &
free -h > "${OUTDIR}/06_free.txt"
sar -n DEV 1 5 > "${OUTDIR}/07_sar_dev.txt" &
sar -n TCP,ETCP 1 5 > "${OUTDIR}/08_sar_tcp.txt" &
top -b -n 1 | head -n 40 > "${OUTDIR}/09_top.txt"

wait
echo "==> Triage collection completed. Files saved to: ${OUTDIR}"
tar -czf "${OUTDIR}.tar.gz" -C "${OUTDIR}" .
rm -rf "${OUTDIR}"
ls -lh "${OUTDIR}.tar.gz"
```

---

## Best Practices

1. **Follow the 60-Second Checklist Before Executing Any Destructive Commands**: Never restart services or reboot hosts until you run the basic checklist commands (`uptime`, `dmesg`, `vmstat`, `free`, `iostat`). Reboots clear kernel ring buffers, destroy volatile socket state, and eliminate the forensic data required for root cause analysis.
2. **Focus on `free -h` "available", NOT "free"**: In Linux, "free" memory is memory that is completely unutilized. A healthy Linux system uses idle RAM for filesystem page caches (`buff/cache`). The true metric of memory headroom is **`available`** memory, which calculates free memory plus reclaimable page cache.
3. **Differentiate User Space (`%usr`) vs Kernel Space (`%sys`) Bottlenecks**:
   - High `%usr`: Inefficient application code, regex catastrophic backtracking, un-indexed loops. (Fix in application code).
   - High `%sys`: Excessive syscall overhead, frequent file opening/closing, lock contention, memory fragmentation. (Fix via connection pooling, epoll, batching).
4. **Inspect `dmesg` with Human Timestamps (`-T`)**: Raw `dmesg` prints seconds since kernel boot (`[12842.129]`). Always run `dmesg -T` to correlate kernel events with real-world application log timestamps (`[Tue Nov 26 10:15:30 2026]`).
5. **Monitor Hypervisor Steal Time (`%steal`) in Cloud Environments**: On AWS EC2, GCP, or Azure, if `%steal` exceeds 5%–10%, the underlying physical hypervisor host is over-committed and starved by other tenants. The solution is migrating the instance or moving to dedicated instance types.
6. **Use `pidstat` to Drill Down to Thread-Level Granularity**: When troubleshooting multi-threaded applications (Java, Go, Rust), standard tools show only the parent process. Pass `-t` to `pidstat` (`pidstat -t -p <PID> 1`) to identify the exact thread ID (TID) causing CPU saturation.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Panicking because `free -m` shows only 150MB "free" | Linux allocates unused RAM to disk buffers and page cache; this memory is instantly reclaimable. | Look at the **`available`** column, not `free`. |
| Rebooting a server immediately upon alert | Wipes out in-memory diagnostic state, core dumps, and ephemeral socket tables; root cause remains unknown. | Collect 60-second triage snapshot before any service restarts. |
| Assuming Load Average 8 on an 8-core machine means 100% CPU usage | Processes blocked on slow disks or NFS (`D` state) increment load average even when CPU is 100% idle. | Check `vmstat` (`r` vs `b`) and `iostat` (`%iowait`) before concluding CPU saturation. |
| Misinterpreting `iostat %util` on modern SSD / NVMe RAIDs | Modern SSDs can process parallel I/O queues; a device can show 100% `%util` while still handling more throughput. | Evaluate `await` (latency) and `aqu-sz` (queue depth) alongside `%util`. |
| Running `strace -p <PID>` on high-throughput production services | `strace` pauses the process on every single syscall, degrading performance by **10x–50x** and causing timeouts. | Use `perf`, `bpftrace`, or sample briefly with `strace -c`. |
| Ignoring `%steal` on cloud instances | High latency caused by hypervisor throttling is falsely diagnosed as application software bugs. | Check `mpstat` or `top` for non-zero `%steal`. |

---

## Performance, Security & Cost Implications

### Performance
- **Triage Command Overhead**: Core diagnostic tools (`uptime`, `vmstat`, `mpstat`, `iostat`, `sar`, `free`) consume virtually zero CPU (< 0.1%) because they parse lightweight `/proc` and `/sys` counters. They are 100% safe to run continuously during high-severity production incidents.
- **Tracing Tools Caution**: Dynamic tracing tools (`strace`, `ltrace`) incur heavy process context switching. Avoid running `strace` on production databases or core web reverse proxies without strict duration limits.

### Security
- **Forensic Preservation**: In incident response (e.g. potential malware or unauthorized shell access), taking a memory snapshot and preserving `/proc/<pid>/` file descriptors before killing suspicious processes preserves the cryptographic and network evidence required for security compliance reporting.

### Cost
- Prematurely resizing cloud VM instance types (e.g., doubling instance size from `m5.large` to `m5.2xlarge`) when the true issue was an unindexed SQL query causing disk I/O wait wastes tens of thousands of dollars in cloud infrastructure spend annually.

---

## Comparison Table / Alternatives

| Diagnostic Domain | Primary Diagnostic Tool | Secondary / Detailed Tool | Kernel Virtual Source |
| :--- | :--- | :--- | :--- |
| **System Load & Run Queues** | `uptime` | `vmstat 1` | `/proc/loadavg`, `/proc/stat` |
| **CPU Utilization (Cores)** | `mpstat -P ALL 1` | `top` / `htop` | `/proc/stat` |
| **Per-Process Metrics** | `pidstat 1` | `htop` / `ps aux` | `/proc/<pid>/stat` |
| **Storage & Disk I/O** | `iostat -xz 1` | `pidstat -d 1` | `/proc/diskstats` |
| **Memory & Cache** | `free -h` | `vmstat -s` / `/proc/meminfo` | `/proc/meminfo` |
| **Network Throughput** | `sar -n DEV 1` | `ip -s link` | `/proc/net/dev` |
| **TCP Retransmits & State** | `sar -n TCP,ETCP 1`| `ss -s` / `ss -tin` | `/proc/net/snmp`, Netlink |
| **Kernel Hardware & OOM** | `dmesg -T` | `journalctl -k` | Kernel ring buffer (`/dev/kmsg`) |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: Walk through your first 60 seconds of troubleshooting a server reported as "running extremely slow".
<details>
<summary>View Solution</summary>

1. **`uptime`**: Check load average vs number of cores (`nproc`). Has the load spiked in the last minute (1-min > 15-min) or has it been sustained?
2. **`dmesg -T | tail`**: Check for OOM killer invocations, disk read/write errors, hardware warnings, or network drops.
3. **`vmstat 1 5`**: 
   - Check `r` (runnable CPU processes) vs `b` (blocked on I/O).
   - Check `si` and `so` (swap-in/swap-out); non-zero values indicate memory starvation.
   - Check `us`, `sy`, `id`, `wa`, `st` CPU breakdown.
4. **`free -h`**: Check `available` memory. Is the system out of memory?
5. **`iostat -xz 1 3`**: Is any disk at 100% `%util` or showing high `await` latency?
6. **`top` / `pidstat 1`**: Identify the specific processes consuming the constrained resource.
</details>

### Q2: What is the difference between a process in state `R` vs state `D` in `top`?
<details>
<summary>View Solution</summary>

- **State `R` (`TASK_RUNNING`)**: The process is actively executing instructions on a CPU core or sitting in the kernel run queue waiting for its next CPU timeslice.
- **State `D` (`TASK_UNINTERRUPTIBLE`)**: The process is sleeping, waiting for a hardware condition or kernel lock (most commonly waiting for a disk block read/write, NFS response, or device driver lock).
  *Key Difference*: Processes in state `D` **cannot be killed** with `kill -9` (`SIGKILL`) because the kernel will not deliver signals until the uninterruptible wait finishes. If the hardware or NFS server is hung, the process remains in state `D` until the hardware responds or the system is rebooted.
</details>

### Q3: An 8-core server shows a Load Average of 25, but CPU utilization is only 10%. How is this possible?
<details>
<summary>View Solution</summary>

In Linux, the Load Average formula includes both processes in state `R` (CPU bound) and processes in state **`D` (Uninterruptible Sleep, blocked on I/O)**.
If a disk, SAN, or mounted NFS share hangs or becomes saturated:
- Processes issuing file read/write syscalls block in state `D`.
- Because they are in state `D`, each blocked process increments the load average counter by 1.
- However, because they are sleeping waiting for disk I/O, they consume **0% CPU**!
- Diagnosis: Run `vmstat 1` (check `b` column) and `iostat -xz 1`.
</details>

### Q4: What does non-zero `%steal` in `top` or `mpstat` signify?
<details>
<summary>View Solution</summary>

`%steal` occurs exclusively in virtualized environments (cloud VMs, KVM, VMware). It represents the percentage of time that the virtual CPU was ready to execute instructions, but the **underlying physical hypervisor CPU was servicing another virtual machine**.
- High `%steal` (> 5%–10%) indicates that the physical host is over-committed or co-hosted with a "noisy neighbor" VM consuming excessive compute.
- The application experiences severe latency jitter through no fault of its own software.
</details>

### Q5: How do you identify whether memory pressure is causing active swapping?
<details>
<summary>View Solution</summary>

Inspect the `si` (swap-in) and `so` (swap-out) columns in `vmstat 1`:
- Merely having swap space allocated (`swpd > 0`) is **not** an emergency; Linux moves dormant, unused memory pages to swap by design.
- However, if **`si` and `so` are continuously non-zero** (e.g. hundreds of kilobytes per second), the system is actively thrashing: reading and writing pages between RAM and disk continuously.
- Application latency collapses by orders of magnitude because disk I/O is 1,000x slower than RAM.
</details>

### Q6: What is Pressure Stall Information (PSI) and why is it superior to load averages?
<details>
<summary>View Solution</summary>

Load average is an ambiguous number that conflates CPU demand with disk wait times without indicating how much capacity was actually lost.
**Pressure Stall Information (PSI)** (accessible via `/proc/pressure/cpu`, `/proc/pressure/memory`, `/proc/pressure/io`) measures the exact percentage of time that tasks were delayed due to resource starvation:
- `some`: Percentage of time in which at least one task was stalled waiting for the resource.
- `full`: Percentage of time in which **all non-idle tasks** were stalled simultaneously (complete system lockup).
PSI provides unambiguous, quantitative metrics for automated auto-scaling and OOM mitigation.
</details>

### Q7: Why should you avoid using `kill -9` as your first troubleshooting step?
<details>
<summary>View Solution</summary>

`kill -9` (`SIGKILL`) cannot be caught, blocked, or handled by the target application:
1. The application cannot flush dirty data buffers to disk, potentially corrupting database files or state.
2. The application cannot release file locks, socket listeners, or clean up temporary files.
3. The application cannot log an orderly shutdown trace or capture an in-memory stack dump.
*Best Practice*: Always send `SIGTERM` (`kill -15`) first, allowing the process to gracefully terminate. Only escalate to `SIGKILL` if the process fails to exit after a reasonable grace period (e.g. 15–30 seconds).
</details>

### Q8: How can you identify which specific process is writing the most data to disk in real-time?
<details>
<summary>View Solution</summary>

Use `pidstat -d 1`:
```bash
sudo pidstat -d 1 5
```
It displays per-process disk metrics:
- `kB_rd/s`: Kilobytes read per second.
- `kB_wr/s`: Kilobytes written per second.
- `kB_ccwr/s`: Kilobytes cancelled writes per second (dirty page overwrites).
Alternatively, use `sudo iotop -o` to view only active I/O-generating processes.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Executing the Brendan Gregg 60-Second Triage
**Task**: Run the first 5 triage commands sequentially on your machine. Identify your CPU core count, load average, and available memory.
<details>
<summary>View Solution</summary>

```bash
# 1. Core count and uptime
echo "Physical Cores: $(nproc)"
uptime

# 2. Kernel messages
dmesg -T 2>/dev/null | tail -n 10 || echo "dmesg restricted (run with sudo)"

# 3. High level system metrics
vmstat 1 3

# 4. Memory availability
free -h

# 5. Disk utilization
iostat -xz 1 2 2>/dev/null || echo "sysstat package needed for iostat"
```
</details>

### Exercise 2: Simulating and Detecting CPU Core Saturation
**Task**: Use a single-threaded CPU loop to saturate one CPU core. Run `mpstat -P ALL 1` to locate the hot core, then kill the loop.
<details>
<summary>View Solution</summary>

```bash
# 1. Start single-threaded busy loop in background
python3 -c "while True: pass" &
LOOP_PID=$!

# 2. Inspect per-core usage with mpstat
mpstat -P ALL 1 3

# 3. Locate the offending PID
pidstat -p "${LOOP_PID}" 1 2

# 4. Terminate loop cleanly
kill "${LOOP_PID}"
```
</details>

### Exercise 3: Inspecting Pressure Stall Information (PSI)
**Task**: Query the Linux kernel Pressure Stall Information for CPU, Memory, and I/O.
<details>
<summary>View Solution</summary>

```bash
echo "=== CPU Pressure ==="
cat /proc/pressure/cpu 2>/dev/null || echo "PSI not enabled in kernel"

echo "=== Memory Pressure ==="
cat /proc/pressure/memory 2>/dev/null || echo "PSI not enabled in kernel"

echo "=== I/O Pressure ==="
cat /proc/pressure/io 2>/dev/null || echo "PSI not enabled in kernel"
```
</details>

### Exercise 4: Identifying Blocked (`D` State) Processes
**Task**: Search the process table for any processes currently in uninterruptible sleep (`D`).
<details>
<summary>View Solution</summary>

```bash
ps -eo pid,user,stat,wchan:20,comm | awk '($3 ~ /^D/) {print $0}'
# If zero output, no processes are currently blocked on I/O!
```
</details>

### Exercise 5: Correlating Top Memory Consumers by RSS
**Task**: List the top 5 memory-consuming processes formatted by Resident Set Size (RSS) in megabytes.
<details>
<summary>View Solution</summary>

```bash
ps -eo pid,user,rss,comm --sort=-rss | head -n 6 | awk 'NR==1 {print $0} NR>1 {$3=$3/1024 " MB"; print $0}'
```
</details>

### Exercise 6: Testing Automatic Snapshot Triage Script
**Task**: Execute the automated triage script from Example 5. Inspect the generated archive contents.
<details>
<summary>View Solution</summary>

```bash
# Run quick snapshot test
mkdir -p /tmp/triage_test && cd /tmp/triage_test
uptime > 01_uptime.txt
vmstat 1 2 > 02_vmstat.txt
free -h > 03_free.txt

tar -czf /tmp/triage_snapshot.tar.gz *.txt
tar -tzvf /tmp/triage_snapshot.tar.gz
rm -rf /tmp/triage_test /tmp/triage_snapshot.tar.gz
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Diagnostic Task | Command |
| :--- | :--- |
| **System Load & Uptime** | `uptime` |
| **Kernel Errors & OOM** | `sudo dmesg -T --level=err,crit,alert,emerg` |
| **System Overview (1s interval)**| `vmstat 1` |
| **Per-Core CPU Saturation**| `mpstat -P ALL 1` |
| **Per-Process CPU Tracking**| `pidstat 1` |
| **Per-Process Disk I/O** | `pidstat -d 1` |
| **Storage Device Latency** | `iostat -xz 1` |
| **Memory Available Headroom**| `free -h` |
| **Network Throughput / Drops**| `sar -n DEV 1` |
| **TCP Retransmissions** | `sar -n TCP,ETCP 1` |

### Further Reading
- [Brendan Gregg: Linux Performance Analysis in 60 Seconds](https://www.brendangregg.com/blog/2015-12-09/linux-performance-analysis-60-seconds.html)
- [The USE Method: Utilization, Saturation, and Errors](https://www.brendangregg.com/usemethod.html)
- [Pressure Stall Information (PSI) - Linux Kernel Documentation](https://docs.kernel.org/accounting/psi.html)
- `man 1 vmstat`, `man 1 mpstat`, and `man 1 iostat`
