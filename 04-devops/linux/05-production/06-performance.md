# Linux Performance Tuning: Kernels, Memory, I/O Schedulers, and Tuned

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Performance Engineering, Kernel Tuning, Memory Subsystem, Disk I/O Schedulers, `tuned` Profiles |
| **Module** | `05-production` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `03-system/10-swap-and-memory-management.md`, `03-system/11-kernel-modules-and-sysctl.md`, `05-production/05-troubleshooting.md` |
| **Target Audience** | Performance Engineers, Database Administrators, SREs, Systems Architects |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools / Subsystems**| `tuned`, `tuned-adm`, `cpupower`, `sysctl`, `blockdev`, `/sys/block/`, `/proc/sys/vm/` |

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

A default Linux kernel installation is compiled with conservative "general-purpose" defaults designed to balance desktop responsiveness, battery efficiency on laptops, and generic server stability. However, production enterprise workloads (high-frequency trading platforms, massive PostgreSQL/MySQL clusters, Redis caches, high-concurrency Nginx ingress proxies) place diametrically opposed demands on the operating system:
- A database engine requires massive I/O throughput and predictable disk write-flushes, while an API gateway prioritizes minimal packet latency and rapid thread context-switching.
- Leaving CPU power governors in `powersave` mode introduces hundreds of microseconds of frequency scaling latency on incoming traffic bursts.
- Leaving `vm.dirty_ratio` at default values (20% of RAM) on a 512GB server allows up to 100GB of unwritten dirty pages to accumulate in memory, causing catastrophic multi-minute kernel I/O freezes when `sync` is forced.

**Performance Tuning** is the disciplined alignment of kernel scheduler parameters, memory reclaim policies, disk elevator algorithms, and CPU governors to the specific computational patterns of your workload. Modern enterprise Linux automates this via **`tuned`**, a dynamic tuning daemon that adapts kernel parameters on-the-fly.

---

## Core Concepts

### CPU Frequency Scaling Governors
Modern CPUs dynamically alter their clock frequency and voltage (P-states) to conserve energy:
- **`powersave`**: Clocks CPU at the lowest possible operating frequency. Unacceptable for latency-sensitive production nodes.
- **`ondemand` / `schedutil`**: Scales clock frequency dynamically based on CPU utilization reported by the kernel scheduler. Introduces small latency spikes during initial frequency ramp-up.
- **`performance`**: Forces all CPU cores to run permanently at their maximum rated non-turbo clock frequency. Eliminates frequency transition latency, maximizing throughput and minimizing p99 tail latency.

### Memory Subsystem Tuning Parameters
1. **`vm.swappiness`** (0–200, default 60):
   - Controls the relative ratio of reclaiming anonymous memory (swapping) versus reclaiming filesystem page cache.
   - For database nodes (Postgres, Redis, Cassandra), set to **`1` or `10`** to prevent active query working sets from being swapped out while keeping emergency swap headroom active.
2. **`vm.dirty_background_ratio` & `vm.dirty_ratio`**:
   - `dirty_background_ratio` (default 10%): Percentage of system memory containing dirty pages before kernel background flush threads (`pdflush`/`flush-X:Y`) begin writing data to disk asynchronously.
   - `dirty_ratio` (default 20%): Hard ceiling. If dirty memory reaches this percentage, **all writing processes are blocked** and forced to write pages to disk synchronously. On large RAM systems, set these as absolute bytes (`dirty_bytes`, `dirty_background_bytes`).
3. **Transparent HugePages (THP)**:
   - Aggregates standard 4KB virtual memory pages into 2MB chunks, reducing Translation Lookaside Buffer (TLB) misses.
   - **Beneficial** for compute-heavy batch processing.
   - **Catastrophic** for databases (Redis, MongoDB, Postgres), causing high latency spikes and memory fragmentation during page compaction. Databases require THP set to `madvise` or `never`.

### Multi-Queue Block I/O Schedulers
The Linux Multi-Queue Block I/O subsystem (`blk-mq`) determines how read/write requests are sorted and dispatched to storage devices:
- **`none` (No-op)**: Dispatches requests directly to hardware submission queues without reordering or merging. **The standard recommendation for ultra-fast NVMe SSDs**, where device controllers handle hardware parallelism.
- **`mq-deadline`**: Guarantees a hard deadline for every I/O request (default 500ms for writes, 50ms for reads), preventing read starvation. Recommended for SATA SSDs and cloud virtual disks.
- **`bfq` (Budget Fair Queueing)**: Provides proportional fair-share bandwidth allocation across desktop applications and slow mechanical HDDs; incurs CPU overhead.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                  Userspace: Tuned Framework                             |
|                                       tuned-adm profile                                 |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                    Tuned Plugins Subsystem                              |
|   +-------------------+  +-------------------+  +-------------------+  +--------------+ |
|   |    plugin_cpu     |  |    plugin_sysctl  |  |     plugin_disk   |  | plugin_net   | |
|   +---------+---------+  +---------+---------+  +---------+---------+  +-------+------+ |
+-------------|----------------------|----------------------|--------------------|--------+
              |                      |                      |                    |
              v                      v                      v                    v
+-------------+---------+  +---------+---------+  +---------+---------+  +-------+--------+
| CPU Frequency Gov     |  | Kernel Sysctl     |  | blk-mq Scheduler  |  | Network Buffers|
| /sys/devices/system/  |  | /proc/sys/vm/     |  | /sys/block/sda/   |  | net.core.      |
| cpu/cpu*/cpufreq/     |  | swappiness,       |  | queue/scheduler   |  | rmem_max,      |
| scaling_governor      |  | dirty_ratio       |  | [none], mq-deadline| | somaxconn      |
| -> "performance"      |  | -> tuned values   |  | -> "none"         |  | -> 16777216    |
+-----------------------+  +-------------------+  +-------------------+  +----------------+
```

### The Tuned Inheritance Model
Tuned profiles are modular INI configuration files located in `/usr/lib/tuned/` (system default) and `/etc/tuned/` (custom overrides). Profiles support inheritance (`include = parent_profile`), allowing custom profiles to inherit baseline server optimizations (e.g., `throughput-performance`) while adding workload-specific adjustments.

---

## Syntax & CLI Tools

### Managing Performance with `tuned-adm`
```bash
# Display active tuned profile
tuned-adm active

# List all available performance profiles
tuned-adm list

# Switch to high-throughput server profile
sudo tuned-adm profile throughput-performance

# Switch to low-latency server profile
sudo tuned-adm profile latency-performance

# Verify system parameters match the active profile specification
tuned-adm verify

# Turn off tuning completely
sudo tuned-adm off
```

### Inspecting and Setting CPU Governors with `cpupower`
```bash
# Display frequency, current governor, and driver across all cores
sudo cpupower frequency-info

# Set governor to performance across all CPU cores
sudo cpupower frequency-set -g performance

# Set governor to powersave
sudo cpupower frequency-set -g powersave
```

### Managing Disk I/O Schedulers and Readahead
```bash
# Query active scheduler for a disk (brackets denote active choice)
cat /sys/block/nvme0n1/queue/scheduler
# Output: [none] mq-deadline

# Set scheduler for a disk to 'none' at runtime
echo "none" | sudo tee /sys/block/nvme0n1/queue/scheduler

# Query read-ahead buffer (in 512-byte sectors)
sudo blockdev --getra /dev/sdb

# Set read-ahead buffer to 4096 sectors (2 MB) for sequential read workloads
sudo blockdev --setra 4096 /dev/sdb
```

---

## Progressive Practical Examples

### Example 1: Profiling and Switching to High-Throughput Tuned Profile
Transitioning a standard Rocky Linux or Ubuntu server to enterprise throughput defaults.

```bash
# 1. Inspect baseline status
tuned-adm active
# Output: Current active profile: balanced

# 2. Switch to throughput-performance profile
sudo tuned-adm profile throughput-performance

# 3. Verify changes were applied to kernel
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# Output: performance

sysctl vm.dirty_ratio
# Output: vm.dirty_ratio = 40 (or absolute bytes)
```

### Example 2: Crafting a Custom Tuned Profile for PostgreSQL / MySQL
Create an optimized, inheritance-based profile for a large-memory database server.

```bash
# Create custom profile directory
sudo mkdir -p /etc/tuned/enterprise-database
```

```ini
# /etc/tuned/enterprise-database/tuned.conf
[main]
include = throughput-performance
summary = Optimized for Large Memory PostgreSQL and MySQL Database Workloads

[cpu]
governor = performance
energy_perf_bias = performance
min_perf_pct = 100

[vm]
# Prevent active memory from swapping prematurely
transparent_hugepages = never

[sysctl]
# Memory and swap tuning
vm.swappiness = 1
vm.dirty_background_bytes = 134217728
vm.dirty_bytes = 536870912
vm.overcommit_memory = 0

# Network socket queue scaling
net.core.somaxconn = 8192
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_fin_timeout = 15

[disk]
# Optimize NVMe and SSD devices
devices = nvme*,sd*
elevator = none
readahead = 4096
```

```bash
# Activate custom profile:
sudo tuned-adm profile enterprise-database
sudo tuned-adm verify
```

### Example 3: Taming the "Flushing Freeze": Setting Absolute Dirty Bytes
On modern servers with 256GB–1TB of RAM, standard percentage-based `dirty_ratio` (20%) means the kernel allows up to **50GB–200GB** of unwritten disk pages to accumulate. When the background flusher finally kicks in, the storage controller saturates, causing minutes of application I/O freezing.

```bash
# Query existing settings
sysctl vm.dirty_ratio vm.dirty_background_ratio

# Fix: Switch to absolute byte limits (128MB background, 512MB hard limit)
sudo tee /etc/sysctl.d/99-dirty-limits.conf << 'EOF'
# Start background flusher when dirty data reaches 128 MB
vm.dirty_background_bytes = 134217728

# Block processes when dirty data reaches 512 MB
vm.dirty_bytes = 536870912
EOF

sudo sysctl -p /etc/sysctl.d/99-dirty-limits.conf
```
*(Setting `dirty_bytes` automatically zeros out `dirty_ratio`, eliminating massive I/O write freezes).*

### Example 4: Disabling Transparent HugePages (THP) for Database Latency
Redis and PostgreSQL suffer severe latency spikes when the kernel pauses application threads to defragment 2MB memory blocks.

```bash
# 1. Query current THP status
cat /sys/kernel/mm/transparent_hugepage/enabled
# Output: [always] madvise never

# 2. Disable THP at runtime
echo "never" | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
echo "never" | sudo tee /sys/kernel/mm/transparent_hugepage/defrag

# 3. Persist permanently across reboots via systemd service
sudo tee /etc/systemd/system/disable-thp.service << 'EOF'
[Unit]
Description=Disable Transparent HugePages (THP) for Databases
DefaultDependencies=no
After=sysinit.target local-fs.target
Before=mongod.service redis.service postgresql.service

[Service]
Type=oneshot
ExecStart=/bin/sh -c 'echo never > /sys/kernel/mm/transparent_hugepage/enabled && echo never > /sys/kernel/mm/transparent_hugepage/defrag'

[Install]
WantedBy=basic.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now disable-thp.service
```

### Example 5: Benchmarking Disk Scheduler Throughput Impact with `fio`
Compare read/write IOPS between `none` and `mq-deadline` on an NVMe storage device.

```bash
# Run random read/write benchmark using fio
fio --name=random-rw \
    --ioengine=libaio \
    --direct=1 \
    --rw=randrw \
    --rwmixread=75 \
    --bs=4k \
    --size=1G \
    --numjobs=4 \
    --runtime=20 \
    --group_reporting \
    --filename=/tmp/fio_test.bin

rm -f /tmp/fio_test.bin
# Compare IOPS and p99 completion latency!
```

---

## Best Practices

1. **Deploy `tuned` as the Single Source of Truth for Performance Profiles**: Avoid scattering ad-hoc tweaks across `/etc/sysctl.conf`, rc.local scripts, and cron jobs. Package all performance parameters (sysctl, CPU governors, disk schedulers, THP) into unified, version-controlled Tuned profiles.
2. **Always Use Absolute `dirty_bytes` on Large RAM Nodes**: Never use `vm.dirty_ratio` percentages on systems with $> 64\text{ GB}$ of RAM. Cap dirty buffers at **256MB–1GB** using `vm.dirty_bytes` and `vm.dirty_background_bytes` to maintain smooth, continuous I/O flushing.
3. **Disable Transparent HugePages for Database Workloads**: Always set THP to `never` or `madvise` on hosts running PostgreSQL, MySQL/MariaDB, MongoDB, Redis, or Elasticsearch.
4. **Use the `none` Scheduler for All NVMe Devices**: Modern NVMe hardware controllers feature up to 64,000 independent hardware queues. Placing a software elevator scheduler (like `bfq` or `cfq`) in front of NVMe introduces CPU locking and degrades performance. Set scheduler to `none`.
5. **Lock CPU Governors to `performance` for Latency-Critical Nodes**: In API gateways, load balancers, and real-time processing nodes, set CPU governors to `performance`. Frequency scaling transitions in `powersave` or `ondemand` add hundreds of microseconds of jitter to p99 requests.
6. **Tune Disk Readahead Based on I/O Pattern**:
   - Random read/write database workloads (OLTP): Keep readahead low (128–256 sectors / 64KB–128KB) to avoid reading unnecessary blocks into RAM.
   - Sequential read data lakes / log analytics (OLAP / ClickHouse): Increase readahead to 2048–4096 sectors (1MB–2MB).

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Leaving `vm.swappiness = 0` | Forces kernel to invoke OOM killer aggressively when anonymous memory is exhausted, rather than swapping cold pages. | Set `vm.swappiness = 1` or `10` on database servers. |
| Setting `vm.dirty_ratio = 40` on a 512GB server | Accumulates $> 100\text{ GB}$ of unwritten dirty pages, causing 3-minute synchronous write freezes. | Use `vm.dirty_bytes = 536870912` (512MB). |
| Leaving CPU governor in `powersave` on production VMs | CPU idles at lowest clock frequency, adding frequency-scaling latency to incoming requests. | Set governor to `performance` via `tuned-adm` or `cpupower`. |
| Running mechanical disk schedulers (`bfq`) on NVMe SSDs | Wastes CPU cycles reordering queues that NVMe hardware handles natively. | Set scheduler to `none`. |
| Enabling Transparent HugePages on Redis/Postgres | Compaction and defragmentation pauses freeze query execution, spiking p99 latency. | Set THP to `never`. |
| Modifying `/sys/block/.../scheduler` without persistence | Changes are lost on next system reboot. | Persist via `tuned` profile `[disk]` section or udev rules. |

---

## Performance, Security & Cost Implications

### Performance
- **Throughput vs Latency Trade-Off**: Tuning is always about trade-offs:
  - `throughput-performance` maximizes batch processing and raw network saturation at the cost of slight latency jitter.
  - `latency-performance` locks CPU frequencies, disables deep C-states, and flushes buffers aggressively to guarantee minimum p99 tail latency at the cost of higher electrical power consumption.
- **BBR Congestion Control**: As demonstrated in network benchmarking, switching TCP congestion control to BBR (`sysctl net.ipv4.tcp_congestion_control=bbr`) increases multi-region cloud replication speeds by up to **5x** over standard Cubic.

### Security
- **No Compromise**: Performance tuning modifies operational parameters (buffers, elevators, frequencies). When properly configured, it does not bypass access controls, authentication, or audit telemetry.

### Cost
- **Eliminating Over-Provisioning**: Properly tuned Linux servers achieve 3x–5x higher application density per node. Optimizing kernel I/O parameters and CPU governors enables companies to downsize cloud instance types, slashing monthly cloud computing bills by 30%–60%.

---

## Comparison Table / Alternatives

| Workload Type | Recommended Tuned Profile | CPU Governor | Disk Scheduler | THP Setting | Swappiness |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **High-Traffic Web / API Gateway** | `latency-performance` | `performance` | `none` (NVMe) | `madvise` | 10 |
| **Relational Database (PostgreSQL/MySQL)** | Custom `enterprise-database` | `performance` | `none` (NVMe) | `never` | 1 |
| **In-Memory Cache (Redis/Memcached)** | `latency-performance` | `performance` | `none` | `never` | 0 or 1 |
| **Big Data Analytics / Spark / Hadoop** | `throughput-performance`| `performance` | `mq-deadline` | `always` | 30 |
| **Generic Virtual Machine Baseline** | `throughput-performance`| `schedutil` / `perf`| `none` / `mq-deadline`| `madvise` | 60 |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: A database server periodically experiences complete I/O freezes lasting 30 to 90 seconds, during which all client queries time out. What is the root cause?
<details>
<summary>View Solution</summary>

**Root Cause**: **Dirty Page Accumulation ("Flushing Panic")**.
On large-memory systems, default `vm.dirty_ratio` (e.g. 20% of 256GB RAM = 51GB) allows up to 51GB of dirty pages to accumulate in page cache. 
When write bursts saturate the page cache and hit the hard ceiling:
- The Linux kernel triggers synchronous flushing: **all writing processes are blocked from further execution** while the kernel forces dirty pages out to storage.
- Storage controllers freeze under the sudden multi-gigabyte queue flood.

**Fix**: Enforce absolute byte limits in `/etc/sysctl.d/99-dirty.conf`:
```ini
vm.dirty_background_bytes = 134217728  # 128 MB (starts async flush early)
vm.dirty_bytes = 536870912             # 512 MB (hard ceiling)
```
</details>

### Q2: Why does Redis issue a startup warning: `WARNING: Transparent Huge Pages (THP) is enabled in your kernel. This will create latency and memory usage issues with Redis`?
<details>
<summary>View Solution</summary>

Redis allocates memory dynamically and relies heavily on `fork()` background saving (Copy-on-Write).
- With standard 4KB pages, when a background save occurs and a write arrives, the kernel clones a 4KB page.
- With **Transparent HugePages (2MB pages)**, a single 1-byte write forces the kernel to copy an entire **2MB page**.
- Furthermore, kernel page compaction algorithms (reclaiming fragmented memory to assemble contiguous 2MB blocks) freeze application threads, causing multi-second latency spikes.
**Fix**: Set THP to `never`.
</details>

### Q3: What is the difference between the `none` and `mq-deadline` disk schedulers?
<details>
<summary>View Solution</summary>

- **`none` (No-Op)**: Completely disables userspace/kernel reordering and scheduling. Requests flow directly into the hardware queues of the controller. Ideal for high-speed NVMe SSDs capable of millions of parallel IOPS.
- **`mq-deadline`**: Maintains separate sorted queues for read and write operations. It assigns an expiration deadline (default: 500ms for writes, 50ms for reads) to prevent read operations from being starved by massive sequential write streams. Ideal for SATA SSDs and cloud block storage (e.g. AWS EBS).
</details>

### Q4: How do you verify whether a CPU is running in `powersave` or `performance` mode?
<details>
<summary>View Solution</summary>

Inspect `/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor`:
```bash
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
```
Or check all cores using `cpupower`:
```bash
sudo cpupower frequency-info | grep "governor"
```
</details>

### Q5: What does `vm.overcommit_memory = 2` do and when should it be used?
<details>
<summary>View Solution</summary>

By default, Linux permits memory overcommit (`overcommit_memory = 0`), gambling that processes request more virtual memory than they actually touch.
- **`overcommit_memory = 2` (Strict No-Overcommit)**: The kernel refuses memory allocations that exceed the commit limit:
  $$\text{Commit Limit} = \text{Swap Size} + (\text{RAM Size} \times \text{overcommit_ratio} [default 50\%])$$
- Useful in mission-critical environments to guarantee that no process will ever trigger the OOM killer unexpectedly.
- *Caution*: Applications like Redis that call `fork()` during persistence will fail if strict no-overcommit is enabled without massive swap allocation.
</details>

### Q6: How does `tuned` apply settings dynamically without modifying `/etc/sysctl.conf`?
<details>
<summary>View Solution</summary>

`tuned` interacts directly with Linux kernel runtime interfaces:
- It writes parameters into `/proc/sys/` via sysctl APIs.
- It writes directly into `/sys/devices/system/cpu/` and `/sys/block/`.
- It monitors system metrics (CPU load, network throughput) and dynamically adapts parameters based on active plugins.
- It stores state in memory and drops cleanly when reverted with `tuned-adm off`.
</details>

### Q7: What is NUMA (Non-Uniform Memory Access) and how does it affect memory latency?
<details>
<summary>View Solution</summary>

In multi-socket servers, memory is partitioned into **NUMA nodes** attached directly to specific physical CPU sockets.
- Accessing local memory attached to the executing CPU socket is fast (~50ns–80ns).
- Accessing remote memory attached to another CPU socket requires crossing the interconnect bus (Intel UPI / AMD Infinity Fabric), adding significant latency.
- Tools like `numactl --interleave` or `numactl --cpunodebind` pin databases to specific NUMA nodes to eliminate cross-socket memory latency.
</details>

### Q8: What is the purpose of `vm.zone_reclaim_mode`?
<details>
<summary>View Solution</summary>

In NUMA systems, when a local memory zone runs out of pages:
- If `zone_reclaim_mode = 0` (default): The kernel allocates memory from a remote NUMA node, avoiding local page reclamation delays.
- If `zone_reclaim_mode = 1`: The kernel forces aggressive local memory reclamation (flushing dirty pages and reclaiming caches) to keep memory local, which can cause severe multi-second application stalling.
*Best Practice*: Keep `zone_reclaim_mode = 0` on enterprise servers.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Querying Active Tuned Status and Profiles
**Task**: Check if `tuned` is installed and running. Query the currently active profile and list all available profiles.
<details>
<summary>View Solution</summary>

```bash
if command -v tuned-adm &>/dev/null; then
    tuned-adm active
    tuned-adm list | head -n 20
else
    echo "tuned package not installed (sudo apt install -y tuned / dnf install -y tuned)"
fi
```
</details>

### Exercise 2: Auditing CPU Scaling Governors
**Task**: Inspect the CPU governors across all cores on your system.
<details>
<summary>View Solution</summary>

```bash
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2>/dev/null | sort | uniq -c || \
echo "Hardware CPU scaling not exposed on this virtual machine"
```
</details>

### Exercise 3: Inspecting Transparent HugePages Configuration
**Task**: Query whether Transparent HugePages (THP) is enabled or disabled in your kernel.
<details>
<summary>View Solution</summary>

```bash
cat /sys/kernel/mm/transparent_hugepage/enabled
# Look for which option is enclosed in brackets: [always], [madvise], or [never]
```
</details>

### Exercise 4: Querying Storage Device Schedulers
**Task**: Identify all storage devices on your system and inspect their active multi-queue block schedulers.
<details>
<summary>View Solution</summary>

```bash
for dev in /sys/block/*/queue/scheduler; do
    echo "Device: $(echo $dev | cut -d/ -f4)"
    cat "${dev}"
done
```
</details>

### Exercise 5: Testing Dirty Memory Parameters
**Task**: Query the current dirty memory ratios and byte limits. Calculate what percentage of total memory they represent.
<details>
<summary>View Solution</summary>

```bash
sysctl vm.dirty_ratio vm.dirty_background_ratio vm.dirty_bytes vm.dirty_background_bytes
free -m
```
</details>

### Exercise 6: Simulating a Custom Tuned Profile Creation
**Task**: Create a test Tuned profile directory `/tmp/test-profile` with a minimal `tuned.conf`. Verify its structure.
<details>
<summary>View Solution</summary>

```bash
mkdir -p /tmp/test-profile
cat << 'EOF' > /tmp/test-profile/tuned.conf
[main]
include = throughput-performance
summary = Custom Test Performance Profile

[sysctl]
vm.swappiness = 10
EOF

cat /tmp/test-profile/tuned.conf
rm -rf /tmp/test-profile
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Goal | Command |
| :--- | :--- |
| **Check Active Profile** | `tuned-adm active` |
| **Set Performance Profile** | `sudo tuned-adm profile throughput-performance` |
| **Verify Tuning Integrity** | `tuned-adm verify` |
| **Check CPU Governor** | `cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor` |
| **Force Performance Governor**| `sudo cpupower frequency-set -g performance` |
| **Query Disk Scheduler** | `cat /sys/block/<disk>/queue/scheduler` |
| **Set NVMe Scheduler None** | `echo "none" \| sudo tee /sys/block/<disk>/queue/scheduler` |
| **Check THP Status** | `cat /sys/kernel/mm/transparent_hugepage/enabled` |
| **Query Readahead (sectors)**| `sudo blockdev --getra /dev/<disk>` |

### Further Reading
- `man 8 tuned` and `man 8 tuned-adm`
- `man 1 cpupower` and `man 8 blockdev`
- [Red Hat Enterprise Linux 9: Monitoring and Managing System Status and Performance](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9/html/monitoring_and_managing_system_status_and_performance/)
- [Linux Kernel Documentation: Memory Management & Dirty Limits](https://docs.kernel.org/admin-guide/sysctl/vm.html)
- [Brendan Gregg: Systems Performance: Enterprise and the Cloud](https://www.brendangregg.com/systems-performance-2nd-edition-book.html)
