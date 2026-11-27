# Performance Bottlenecks & Troubleshooting Scenarios: Technical Interview Q&A

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Performance Analysis, Brendan Gregg's USE Method & Production Incident Triage |
| **Module** | `08-interview` — Technical Interview Preparation & Scenario Q&A |
| **Track** | `04-devops/linux` |
| **Target Level**| Senior / Staff DevOps, SRE, and Systems Performance Engineers |
| **Questions** | 20 Deep Scenario & Architectural Questions |
| **Last Updated**| 2026 |

---

## Overview

In senior technical interviews, interviewers evaluate how you think when presented with vague problem statements ("The database is slow", "The API is throwing 504 timeouts", "CPU load is 40 on a 4-core machine"). Top candidates apply structured diagnostic frameworks (like Brendan Gregg's **USE Method**), correlate metrics across kernel subsystems, and isolate root causes methodically.

---

### Q1: What is Brendan Gregg's USE Method, and how do you apply it to an unknown slow server?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The Framework:**
For every system resource (CPU, Memory, Storage Disks, Network Interfaces), evaluate:
1. **Utilization**: Percentage of time the resource was busy servicing work (e.g., CPU %usr, Disk %util, Network bandwidth).
2. **Saturation**: Degree to which the resource has extra work queued that it cannot process immediately (e.g., CPU runqueue length / load average, disk I/O request queue, network socket backlogs).
3. **Errors**: Count of error events indicating hardware failures or resource exhaustion (e.g., disk I/O read errors, NIC CRC/drop errors, memory allocation failures).

**The 60-Second Initial Triage Checklist:**
```bash
uptime            # Check 1, 5, 15 min load averages vs core count (Saturation)
dmesg -T | tail   # Check for kernel panics, OOM kills, hardware errors (Errors)
vmstat 1 5        # Check runqueue (r), blocked (b), free RAM, swap in/out (Utilization/Saturation)
mpstat -P ALL 1 2 # Check CPU utilization distribution across cores (%usr, %sys, %iowait)
iostat -xz 1 3    # Check disk throughput, latency (await), and busyness (%util)
free -h           # Check available memory vs page cache buffer
ip -s link        # Check network interface drops and errors
```
</details>

---

### Q2: What is the exact mathematical definition of Linux "Load Average", and how does it differ from CPU Utilization?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The Definition:**
In Linux, **Load Average** is the exponentially damped moving average of the number of processes in the system runqueue in two specific states:
$$\text{Load} = \text{Tasks in State } \mathbf{R} \text{ (Running / Runnable)} + \text{Tasks in State } \mathbf{D} \text{ (Uninterruptible Sleep)}$$

**Load Average vs CPU Utilization:**
- **CPU Utilization (`%usr + %sys`)**: Measures the percentage of CPU clock cycles actively executing instructions over a specific time window. Maximum is 100% per core.
- **Load Average**: A **queue length metric** measuring demand/saturation. It has no upper ceiling.

**Crucial Production Insight:**
Unlike Solaris or BSD (which only count `R` tasks), Linux uniquely includes tasks in state **`D`** (blocked on disk I/O, NFS storage locks, or page faults). Therefore:
- A server can have **100% CPU utilization with a Load Average of 4** (4 CPU cores fully saturated running compute jobs).
- A server can have **0% CPU utilization with a Load Average of 80** (all 80 threads blocked waiting for an unresponsive NFS or SAN storage volume!).
</details>

---

### Q3: What does a high `%iowait` in `top` indicate? Is the CPU actually doing work?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
`%iowait` is often misunderstood. **The CPU is NOT doing I/O work!**

`%iowait` is simply a specialized form of **CPU Idle Time**. Specifically, a CPU core reports `%iowait` when:
1. The CPU core has **zero runnable tasks** and is completely idle.
2. At least **one task initiated by this CPU is currently blocked waiting for outstanding storage block I/O** to complete.

If all pending disk I/O requests finished, that CPU time would immediately be categorized as standard `%idle`. High `%iowait` tells you that the CPU is starved and idling because applications are waiting on slow disks, failing drives, or saturated storage controllers.
</details>

---

### Q4: Explain the difference between `MemFree` and `MemAvailable` in `/proc/meminfo`.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

- **`MemFree`**: The amount of physical RAM that is completely unallocated and empty. In modern Linux, having a high `MemFree` is an indicator of under-utilized hardware, because the kernel actively uses unused RAM for the **Page Cache** and slab buffers.
- **`MemAvailable` (Kernel 3.14+)**: An estimate of **how much memory can actually be allocated by applications without causing the system to swap**. It computes:
  $$\text{MemAvailable} \approx \text{MemFree} + \text{Reclaimable Page Cache} + \text{Reclaimable Slab} - \text{Low-Watermark Safety Margin}$$

**Production Rule:**
Never alert on `MemFree`! Alerting on `MemFree` triggers false alarms because healthy servers naturally hover around 2-5% `MemFree` due to disk caching. Always monitor and alert on **`MemAvailable`**.
</details>

---

### Q5: What is `vm.swappiness` and what are the trade-offs between setting it to `0`, `1`, `10`, or `60`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The Mechanism:**
`vm.swappiness` (range 0 to 200, default 60) represents the kernel's **relative ratio of reclaiming anonymous application heap memory (by writing to swap) versus reclaiming page cache file memory**.
$$\text{Ratio} = \frac{\text{Swap Anonymous Memory}}{\text{Drop Page Cache}}$$

**Settings Evaluated:**
- **`60` (Default)**: Aggressively pages idle application heap to swap to retain file page cache. Good for general desktop/workstations; terrible for low-latency databases.
- **`10` (Cloud / Web Standard)**: Strongly biases the kernel to reclaim file page cache first, paging to swap only under significant memory pressure.
- **`1` (Database Standard - PostgreSQL / MySQL)**: Minimizes swapping to the absolute minimum without disabling it entirely.
- **`0`**: On modern kernels (3.5+), avoids swapping entirely until physical memory and page caches are completely exhausted (risk of sudden OOM kill).
</details>

---

### Q6: How do you interpret `iostat -xz 1` columns to isolate disk saturation vs disk latency?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Key Metrics in `iostat -xz 1`:**
- **`r/s` & `w/s`**: Read and write I/O requests per second (IOPS).
- **`rkB/s` & `wkB/s`**: Read and write throughput in kilobytes per second.
- **`r_await` & `w_await` (CRITICAL)**: Average time (in milliseconds) spent servicing read/write requests, including **queue wait time + physical hardware service time**.
  - NVMe SSD: $< 0.5 \text{ ms}$
  - SATA SSD: $< 2.0 \text{ ms}$
  - Spinning HDD: $10 - 25 \text{ ms}$
  - If `r_await` spikes to $100\text{ms}+$, the storage subsystem is heavily saturated or queuing!
- **`aqu-sz` (Average Queue Size)**: Number of I/O requests queued at the storage driver layer.
- **`%util`**: Percentage of time the device had requests in flight. On modern multi-queue SSDs, `%util` can hit 100% while the drive can still absorb more parallel IOPS. Always correlate `%util` with `await`.
</details>

---

### Q7: What is the difference between Voluntary and Non-Voluntary Context Switches?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

- **Voluntary Context Switches (`voluntary_ctxt_switches`)**: The thread **chose to yield the CPU voluntarily** because it blocked waiting for an external resource (e.g., disk I/O, waiting on `epoll_wait()`, `nanosleep()`, or acquiring a contended mutex). High voluntary context switches indicate an I/O-bound or lock-contended application.
- **Non-Voluntary Context Switches (`nonvoluntary_ctxt_switches`)**: The thread was **forcibly preempted by the kernel scheduler** because its allocated CPU time slice expired, or a higher-priority task became runnable. High non-voluntary context switches indicate intense **CPU starvation / CPU core oversubscription**.
</details>

---

### Q8: An application experiences sudden 2-second tail latency spikes. CPU, memory, and disk utilization are low. What kernel areas do you investigate?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Diagnostic Investigation:**
1. **Transparent Huge Pages (THP) Allocation**: Linux THP dynamically allocates 2MB memory pages. When memory is fragmented, the kernel executes **direct compaction**, freezing application threads synchronously for multi-hundred milliseconds while defragmenting memory! (Check: `grep -i compact /proc/vmstat`).
2. **Kernel Lock Contention / Futex Delays**: Thread deadlocks or heavy mutex contention on multi-threaded runtimes (Profile via `perf record -e futex:sys_enter_futex`).
3. **Cgroup CPU Throttling**: The container hit its CFS bandwidth ceiling (`cpu.max`). Check `/sys/fs/cgroup/.../cpu.stat` for `nr_throttled` and `throttled_usec`.
4. **DNS Resolution Timeouts**: An un-cached external DNS query timing out for 2 seconds.
5. **TCP Retransmissions / SYN Drop**: Network drops causing minimum RTO fallback (Check: `netstat -s | grep retransmitted`).
</details>

---

### Q9: How do you identify whether a container in Kubernetes is being CPU-throttled?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
Kubernetes CPU limits are implemented using Linux CFS (Completely Fair Scheduler) Bandwidth Control:
`cpu.max = <quota> <period>` (e.g., `50000 100000` = 0.5 CPU cores per 100ms period).
If a container exhausts its 50ms quota within the first 10ms of a 100ms period, the kernel **completely freezes every thread in that container for the remaining 90ms**!

**Inspection Command:**
```bash
# In Cgroups v2:
cat /sys/fs/cgroup/cpu.stat
# Output displays:
# usage_usec 45120512
# user_usec 30102041
# system_usec 15018471
# nr_periods 1205
# nr_throttled 342          <-- High number indicates severe throttling!
# throttled_usec 24510204    <-- Total microseconds threads were frozen!
```
</details>

---

### Q10: What does `ss -lnt` display in the `Send-Q` and `Recv-Q` columns for LISTENING sockets vs ESTABLISHED sockets?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The Meaning Changes Completely Based on State!**

1. **For LISTENING Sockets (`ss -lnt`):**
   - **`Recv-Q`**: Current number of fully established connections waiting in the **Accept Queue** to be accepted by the application via `accept()`.
   - **`Send-Q`**: The **Maximum capacity** of the listen queue (`somaxconn` or application backlog parameter).
   - *If `Recv-Q` == `Send-Q`, the accept queue is completely overflowing, and new client connections are being dropped or reset!*

2. **For ESTABLISHED Sockets (`ss -nt`):**
   - **`Recv-Q`**: Number of data bytes received from the network but not yet read by the local application via `read()`.
   - **`Send-Q`**: Number of data bytes sent by the local application that have not yet been acknowledged by the remote client.
</details>

---

### Q11: Explain NUMA (Non-Uniform Memory Access) architecture and the performance impact of remote memory access.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Architecture:**
In multi-socket servers, physical memory is partitioned across **NUMA Nodes**, with each CPU socket directly wired to its own local memory channels.
- **Local Access**: CPU Node 0 reads RAM attached to Socket 0 ($\approx 60 \text{ ns}$ latency).
- **Remote Access**: CPU Node 0 reads RAM attached to Socket 1 over the inter-socket interconnect bus (UPI / QPI) ($\approx 140 \text{ ns}$ latency).

**The "NUMA Anomaly" Issue:**
If the kernel memory allocation policy (`zone_reclaim_mode`) is misconfigured, a process running on Node 0 might invoke the page allocator, find Node 0's local memory full, and begin aggressively paging local memory to disk, **even though Node 1 has 64GB of free RAM**!
- Pin processes to local NUMA nodes: `numactl --cpunodebind=0 --membind=0 myapp`.
</details>

---

### Q12: How do you choose between `top`, `perf`, `strace`, and `bpftrace` during an active performance crisis?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

```
  High-Level Metrics (What is broken?)
  [ top / vmstat / iostat / ss ] -> Identify resource (CPU, Memory, Disk, Socket)
                 |
                 v
  Execution Function Profiling (Where are CPU cycles burned?)
  [ perf top / perf record ] -> Profile CPU instruction hotspots (Low ~1% overhead)
                 |
                 v
  Event Latency & Kernel Tracing (Why is it waiting/stalling?)
  [ bpftrace / BCC ] -> Trace lock times, disk latency histograms, TCP handshakes
                 |
                 v
  Syscall Interception (Last resort / Staging only)
  [ strace ] -> Inspect file paths, exact error codes (Heavy ~500% overhead!)
```
</details>

---

### Q13: What is the Linux VFS Slab Allocator and how do you diagnose kernel memory leaks in `dentry` or `inode_cache`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Mechanism:**
The **Slab Allocator** manages kernel-internal data structures (inodes, directory entries, socket buffers, task structs) in pre-allocated memory caches to avoid memory fragmentation.

**The Leak Scenario:**
An application traverses millions of non-existent files or opens millions of short-lived sockets. In `/proc/meminfo`, `Slab` grows to 50GB+, leaving zero RAM for application heaps!
- Inspect active slab caches:
  ```bash
  sudo slabtop -s c # Sort slab caches by cache size
  ```
- If `dentry` or `ext4_inode_cache` consumes gigabytes:
  - Force kernel to reclaim slab memory: `echo 2 > /proc/sys/vm/drop_caches`.
  - Adjust sysctl: `vm.vfs_cache_pressure = 100` (or `150`).
</details>

---

### Q14: A multi-threaded Java or Go service has 100% CPU usage on all cores, but throughput is near zero. What is happening?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Potential Root Causes:**
1. **Garbage Collection (GC) Thrashing**: The JVM heap is 98% full. Multiple parallel GC threads are running continuously across all CPU cores executing stop-the-world compaction cycles, burning 100% CPU without processing application requests.
2. **Lock Contention / Busy-Waiting Spinlocks**: Threads are executing in tight spinlock loops (`while (!ready)`) waiting for a shared mutex held by a blocked thread, burning CPU cycles without making forward progress.
3. **Corrupted Infinite Loop**: A hash map collision or concurrency race condition corrupted an internal data structure (e.g., circular linked list in non-thread-safe HashMap), trapping worker threads in endless pointer traversal loops.

**Diagnosis:**
Take thread dumps (`jstack <pid>` for Java, or `perf top`) to observe whether CPU cycles are spent in `gc` threads, `futex` spinlocks, or application methods.
</details>

---

### Q15: What causes a "TCP Retransmission Storm" and how do you diagnose it at the packet level?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Mechanism:**
When network switches drop packets due to bufferbloat, microbursts, or duplex mismatches, the sender's TCP stack receives duplicate ACKs or its Retransmission Timeout (RTO) expires. The sender retransmits the lost packets. If the bottleneck link is already congested, flooding retransmissions exacerbates the queue overflow, triggering a cascading retransmission storm.

**Diagnosis:**
```bash
# 1. System-wide retransmission rate
netstat -s | grep -i retrans
# 2. Wire-level capture of retransmissions
sudo tcpdump -i eth0 -nn "tcp[tcpflags] & tcp-syn == 0 and (tcp[13] & 0x10 != 0)" | grep "retransmission"
# 3. eBPF BCC tool
sudo tcpretrans-bpfcc
```
</details>

---

### Q16: What is a "Zombie Connection" in network load balancers and how do TCP Keepalives mitigate it?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Mechanism:**
A client laptop opens a TCP connection to an internal server through a stateful NAT firewall or cloud load balancer (AWS ALB), and then shuts its lid or loses Wi-Fi signal.
- No `FIN` or `RST` packet is ever transmitted on the wire.
- The server socket remains in the `ESTABLISHED` state indefinitely, holding memory and thread resources open for days!

**Mitigation (TCP Keepalive):**
The Linux kernel periodically transmits empty ACK probe packets on idle sockets to verify client reachability:
```bash
sysctl -w net.ipv4.tcp_keepalive_time=300   # Start probes after 5 minutes of inactivity
sysctl -w net.ipv4.tcp_keepalive_intvl=15   # Probe every 15 seconds
sysctl -w net.ipv4.tcp_keepalive_probes=4   # Drop connection after 4 unanswered probes (60s)
```
</details>

---

### Q17: What is the difference between `nice` and `ionice`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

- **`nice`**: Adjusts the **CPU scheduling priority** of a process in the Completely Fair Scheduler (CFS). Nice values range from `-20` (highest priority) to `+19` (lowest priority).
- **`ionice`**: Adjusts the **Block I/O scheduling priority** of a process (for storage controllers using the BFQ or CFQ schedulers).
  - Class 1: Real-time (first access to disk).
  - Class 2: Best-effort (default, priority 0-7).
  - Class 3: **Idle** (process gets zero disk I/O time unless the storage bus has been completely idle for a configured window). Ideal for background backup jobs: `ionice -c 3 backup.sh`.
</details>

---

### Q18: What is Kswapd, and why is high CPU usage in `kswapd0` a warning sign of memory collapse?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Mechanism:**
`kswapd0` is the kernel asynchronous memory reclamation daemon. It monitors physical memory watermarks (`pages_min`, `pages_low`, `pages_high`).
- When free memory dips below `pages_low`, `kswapd0` awakens in the background to scan memory pages, reclaim clean page cache buffers, and write dirty anonymous memory to swap until free memory reaches `pages_high`.

**The Warning Sign:**
If `top` displays `kswapd0` pegged at 100% CPU:
- Free memory has plunged below `pages_min`.
- The system has entered **Direct Memory Compaction / Direct Reclamation**: user-space application threads are being suspended synchronously to scan and reclaim memory.
- The system is on the verge of thrashing or triggering an immediate OOM Killer panic.
</details>

---

### Q19: Explain the difference between `perf stat`, `perf top`, and `perf record`.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

| Subcommand | Functionality | Primary Use Case |
| :--- | :--- | :--- |
| **`perf stat`** | Aggregates high-level hardware/software counter totals (IPC, cycles, cache misses, branch misses) across execution duration. | Comparing code algorithm efficiency or benchmarking compilation tasks. |
| **`perf top`** | Interactive ncurses real-time monitor displaying active function instruction hotspots across CPUs. | Identifying which function is burning CPU right now in live production. |
| **`perf record`** | Samples backtraces at a fixed frequency (e.g., `-F 99`) and writes sample data to a binary file (`perf.data`). | Deep offline analysis, call graph reconstruction, and generating SVG Flame Graphs. |
</details>

---

### Q20: A production web application experiences intermittent 5-second connection hangs during peak traffic. Step-by-step, how do you diagnose this?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Step-by-Step Triage Runbook:**
1. **Check Listen Backlog Drops (`Recv-Q` vs `Send-Q`)**:
   Run `ss -lnt`. Is `Recv-Q` == `Send-Q` on the web server port? Run `netstat -s | grep "listen drops"`. If listen queue drops are incrementing, the application event loop is blocking, causing incoming connections to time out.
2. **Check SYN Backlog & SYN Cookies**:
   Run `netstat -s | grep -i cookie`. If SYN cookies are being computed, the SYN backlog is overflowing.
3. **Inspect DNS Resolution Latency**:
   Verify `/etc/resolv.conf`. If an unreachable nameserver is configured with `timeout:5`, failing queries will hang for exactly 5 seconds!
4. **Check Conntrack Table Limits**:
   Check `dmesg -T | grep nf_conntrack`. If the connection tracking table is full, packets are being dropped.
5. **Inspect Cgroups CPU Throttling**:
   Check `/sys/fs/cgroup/.../cpu.stat`. Are threads being throttled by CFS quota limits?
</details>

---
*Next Note: [07-security-qa.md](file:///04-devops/linux/08-interview/07-security-qa.md)*
