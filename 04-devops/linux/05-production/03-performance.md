# Performance

When a server is slow, the useful question is "which resource is the bottleneck: CPU, memory, disk, or network?" Linux exposes enough counters to answer that in a minute, if you know how to read them. This note explains the numbers people most often misread (load average, "free" memory, swap), the tools that show each resource, and the limits and kernel settings that matter for servers.

Prerequisites: [Processes and Signals](../03-system/01-processes-and-signals.md), [Storage and Filesystems](../03-system/05-storage-and-filesystems.md), and [Troubleshooting](./02-troubleshooting.md) (the quick triage commands).

## Approach: measure, then change

For each resource, ask three things (often called the USE method):

- **Utilization**: how busy is it?
- **Saturation**: is work queuing up waiting for it?
- **Errors**: is it failing (retransmits, I/O errors, drops)?

Take a baseline when the system is healthy so you know what "normal" looks like. **Tune only after you've identified the bottleneck,** change one thing at a time, and measure again. Many "tuning" gains are really fixing the application (a slow query, an unbounded cache, a log written on every request).

Install the standard tools once:

```bash
sudo apt install sysstat htop iotop    # provides iostat, mpstat, pidstat, sar; htop; iotop
```

## Load average

`uptime` (or `cat /proc/loadavg`) shows three numbers: the average load over the last 1, 5, and 15 minutes.

```text
 15:40:02 up 12 days,  load average: 3.20, 2.10, 1.05
```

**On Linux, load is the average number of tasks that are runnable (on CPU or waiting for one) *plus* tasks in uninterruptible sleep (`D` state, usually waiting on disk or NFS).** So it's not a pure CPU measure; I/O waits raise it too.

Compare it to the number of CPU cores (`nproc`):

| Load vs cores | Meaning |
|---|---|
| Below core count | spare capacity |
| About equal | fully busy, no queue |
| Well above | work is queuing (CPU saturation, or tasks blocked on I/O) |

A load of 4 is fine on 8 cores and a problem on 1. Read the trend: `3.20, 2.10, 1.05` (1 > 5 > 15) is **rising**; `1.05, 2.10, 3.20` is recovering.

**High load with low CPU use** is the classic sign of I/O (or lock) waits: look for `D` processes (`ps aux | awk '$8 ~ /D/'`) and check disk with `iostat`.

## CPU

```bash
top                  # %Cpu(s) line; press 1 for per-core
mpstat -P ALL 1      # per-core utilization every second
pidstat 1            # per-process CPU over time
top -H -p <pid>      # per-thread view of one process
```

The CPU line in `top` splits time into:

| Field | Meaning | Concern when high |
|---|---|---|
| `us` | user-space code (your app) | app is CPU-bound: profile it |
| `sy` | kernel/system calls | heavy syscalls, networking, or context switching |
| `wa` | waiting for I/O | disk or network storage bottleneck |
| `st` | **steal**: hypervisor gave CPU to another VM | noisy neighbor or oversold host; you can't fix it from inside |
| `id` | idle | |

`vmstat 1` is a compact summary of the whole system, one line per second:

```text
procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu-----
 r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st
 3  0      0 412000  80000 2100000    0    0    20    15  500 900 35 10 50  5  0
```

- `r`: runnable processes. Persistently above the core count means CPU saturation.
- `b`: processes blocked on I/O.
- `si`/`so`: memory swapped in/out per second. **Sustained non-zero values mean memory pressure.**
- `bi`/`bo`: blocks read/written to disk.
- `us sy id wa st`: as above.

## Memory

### Reading `free`

```bash
free -h
```

```text
              total   used   free   shared  buff/cache   available
Mem:           7.7G   2.1G   412M    120M        5.2G        5.3G
Swap:          2.0G     0B   2.0G
```

**Look at `available`, not `free`.** Linux deliberately uses spare RAM as **page cache** (`buff/cache`) to speed up file access, and gives it back instantly when programs need it. Low `free` with high `available` is healthy; the server isn't "out of memory". A common misconception is that a high used+cache figure means a leak.

Real memory pressure looks like: `available` close to zero, active swapping (`si`/`so` in `vmstat`), and OOM-killer messages in `dmesg`.

### Which process?

```bash
ps aux --sort=-%mem | head                 # biggest by memory
ps -o pid,rss,vsz,cmd -p <pid>             # RSS = resident (real RAM); VSZ = virtual address space (often misleadingly huge)
```

To spot a **leak**, sample the same process's RSS over time (`watch -n 60 'ps -o rss= -p <pid>'`). Steady upward growth that never levels off is the signal. Cap runtimes like Node explicitly (`--max-old-space-size`) so they fail in a controlled way instead of dragging the box down.

### Swap

Swap is disk space the kernel uses when RAM is tight. **Some swap in use isn't a problem**; it often holds idle pages that haven't been touched in a long time. The signal is **continuous swap activity** (`si`/`so` above zero for long periods), which means the system is thrashing and everything slows.

```bash
swapon --show
cat /proc/sys/vm/swappiness          # how eagerly the kernel swaps vs. dropping cache (default 60)
```

Adding a swap file on a server that has none (a small safety buffer):

```bash
sudo fallocate -l 2G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab
```

Swap buys time; it isn't a substitute for enough RAM. Disabling swap entirely isn't automatically faster; it just turns memory exhaustion into an immediate OOM kill. Databases often prefer a lower `vm.swappiness` (for example 10) so they're swapped less eagerly.

### The OOM killer

When memory and swap run out, the kernel kills a process to survive.

```bash
dmesg -T | grep -i -E "out of memory|killed process"
```

The victim is chosen by a score (`/proc/<pid>/oom_score`), usually the largest consumer, not necessarily the one that leaked. You'll see exit code **137** ([signals](../03-system/01-processes-and-signals.md)). **Containers and systemd units can have their own memory limit** (cgroups; `MemoryMax=` in a unit, `--memory` in Docker) and get OOM-killed *inside the limit* while the host has plenty free. See [namespaces and cgroups](../06-advanced/01-namespaces-and-cgroups.md). Protect a critical process by lowering its `oom_score_adj` (range -1000 to 1000), but fixing the memory use is the real answer.

## Disk I/O

```bash
iostat -xz 1             # extended per-device stats, hiding idle devices
sudo iotop -o            # which processes are doing I/O right now
pidstat -d 1             # per-process read/write rates
```

Key `iostat -x` columns:

| Column | Meaning |
|---|---|
| `r/s`, `w/s`, `rkB/s`, `wkB/s` | operations and throughput |
| `await` (or `r_await`/`w_await`) | average ms a request takes, including queue time. Rising latency is the real symptom |
| `aqu-sz` | average queue length |
| `%util` | share of time the device was busy |

`%util` near 100% means a spinning disk is saturated, but on SSD/NVMe (which handle many requests in parallel) it's less meaningful; **latency (`await`) is the better signal.**

Typical causes of heavy I/O: debug-level logging, a backup or `rsync` running at peak (use `nice`/`ionice`, [processes](../03-system/01-processes-and-signals.md)), swapping, a database without enough memory for its cache, or a full/slow disk. A virtualized or cloud disk may also hit its **provisioned IOPS/throughput cap**, which shows up as high `await` at modest throughput.

## Network

```bash
ss -s                          # socket totals by state
ip -s link show eth0           # RX/TX packets, errors, drops
sudo nstat -az | grep -i retrans   # TCP retransmissions
dmesg -T | grep -i conntrack   # "nf_conntrack: table full, dropping packet"
```

Errors/drops on the interface, many retransmits, or a full connection-tracking table explain slowness or random connection failures that look like application bugs. `sar -n DEV 1` shows per-interface throughput over time. To see where HTTP time goes, use `curl -w` timings ([networking basics](../04-networking/01-networking-basics.md)).

## Limits: `ulimit`, file descriptors, and systemd

Every process has resource limits. The one that bites servers is the **open file descriptor limit**: sockets and files both count, so a busy server with thousands of connections can exhaust the classic default of 1024 and fail with `EMFILE` / "Too many open files".

```bash
ulimit -n                       # soft limit for this shell
ulimit -Hn                      # hard limit (the ceiling)
cat /proc/<pid>/limits          # the REAL limits of a running process
ls /proc/<pid>/fd | wc -l       # how many descriptors it has open now
cat /proc/sys/fs/file-nr        # system-wide: allocated, unused, max
```

**Where to raise it depends on how the process starts**, and this is the most common mistake:

| Process started by | Set the limit in |
|---|---|
| a systemd service | `LimitNOFILE=65536` in the unit (or a drop-in via `systemctl edit`) |
| a login session (SSH, shell) | `/etc/security/limits.conf` or `/etc/security/limits.d/*.conf` (applied by PAM) |
| Docker container | `--ulimit nofile=65536:65536` |

`limits.conf` does **not** apply to systemd services, so editing it and wondering why your service still hits the limit is a classic. After changing a unit: `daemon-reload`, restart, then verify in `/proc/<pid>/limits`. If the descriptor count keeps climbing even at a high limit, you have a leak.

## sysctl: kernel parameters

`sysctl` reads and sets kernel parameters that live under `/proc/sys`.

```bash
sysctl vm.swappiness                        # read one
sysctl -a | grep -i somaxconn               # search
sudo sysctl -w vm.swappiness=10             # set at runtime (lost on reboot)
```

To persist, put it in a file and reload:

```bash
echo 'vm.swappiness = 10' | sudo tee /etc/sysctl.d/99-custom.conf
sudo sysctl --system                        # load all sysctl config files
```

Settings people legitimately change on servers:

| Parameter | Why |
|---|---|
| `vm.swappiness` | how readily to swap (lower for databases) |
| `fs.file-max` | system-wide file descriptor ceiling |
| `fs.inotify.max_user_watches` | "ENOSPC: System limit for number of file watchers reached" in dev tools |
| `net.core.somaxconn` | maximum listen backlog; its default has differed across kernel versions, so check before assuming |
| `net.ipv4.ip_local_port_range` | ephemeral ports for outgoing connections (many proxy/client connections) |
| `net.ipv4.ip_unprivileged_port_start` | lets non-root processes bind low ports (an alternative to capabilities) |

The defaults are reasonable. Don't paste long "kernel tuning" lists from the internet without understanding each line; many are outdated, obsolete on current kernels, or harmful. Change one parameter for a measured reason, record it, and re-test.

## Putting it together: a performance triage

```text
slow?  ─► uptime / vmstat 1 ─► what's the bottleneck?
          │
          ├─ high us/sy, r > cores ─► CPU: top, pidstat, profile the app
          ├─ low free + si/so > 0 ──► memory: ps --sort=-%mem, OOM logs, swap
          ├─ high wa, await ↑ ──────► disk: iostat -xz, iotop, what's writing?
          ├─ errors/retrans/drops ──► network: ip -s link, ss -s, nstat, conntrack
          └─ errors in the app ─────► limits (EMFILE), logs, connection pool, DB
```

If the numbers point at the application, move to code-level profiling. For system-level digging (what syscalls, where the CPU time goes), see [Advanced Debugging](../06-advanced/02-advanced-debugging.md) (`strace`, `perf`).

## Common mistakes

- **Reading `free` as "memory is almost gone"** instead of checking `available`.
- **Treating any swap use as a crisis**, or disabling swap as a "performance fix".
- **Judging load average without the core count**, or forgetting that `D`-state I/O waits count toward it.
- **Ignoring `st` (steal)** and tuning the app when the host is the problem.
- **Raising `limits.conf` for a systemd service** (wrong place; use `LimitNOFILE=`).
- **Trusting `ulimit -n` in your shell** instead of `/proc/<pid>/limits` for the real process.
- **Blindly applying sysctl "tuning guides"**, or editing `/proc/sys` without persisting it.
- **Using `%util` alone** to judge SSD/NVMe health; look at latency.
- **Tuning before measuring**, and changing several things at once.
- **Fixing symptoms** (bigger instance) when one log line or query is the culprit.

## Quick Summary

- Find the bottleneck first: CPU, memory, disk, or network (utilization, saturation, errors).
- Load average = runnable + uninterruptible tasks; compare to `nproc`; high load with low CPU means I/O waits.
- `free -h`: trust `available`; page cache is good. Swap *activity* (`si`/`so`) is the warning sign, not swap *use*.
- `vmstat 1`, `iostat -xz 1`, `mpstat`, `pidstat`, `ss -s` cover most diagnosis; latency matters more than `%util`.
- Descriptor limits: `LimitNOFILE=` for systemd services, `limits.conf` for login sessions; verify in `/proc/<pid>/limits`.
- `sysctl` via `/etc/sysctl.d/*.conf` + `sysctl --system`; change one thing, measure, document.

**Next:** [Namespaces and cgroups](../06-advanced/01-namespaces-and-cgroups.md)
