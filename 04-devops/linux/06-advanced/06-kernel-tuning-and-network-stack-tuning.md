# Linux Kernel & High-Performance Network Stack Tuning

| Metadata | Details |
| :--- | :--- |
| **Topic** | Production Kernel Optimization, 100GbE Network Stack & Virtual Memory Tuning |
| **Module** | `06-advanced` — Advanced Systems & Kernel Mechanics |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS (Kernel 6.8+), RHEL 9.4+ (Kernel 5.14+) |
| **Prerequisites** | `04-networking/01-networking-basics.md`, `05-production/06-performance.md` |
| **Difficulty** | Advanced |
| **Last Updated** | 2026 |

---

## Table of Contents
1. [What & Why: Scaling Beyond Out-of-the-Box Defaults](#what--why-scaling-beyond-out-of-the-box-defaults)
2. [Core Concepts & Architecture](#core-concepts--architecture)
3. [Internal Mechanics: Packet Flow & TCP Queueing Stages](#internal-mechanics-packet-flow--tcp-queueing-stages)
4. [Sysctl & Subsystem Parameter Reference](#sysctl--subsystem-parameter-reference)
5. [Progressive Practical Examples](#progressive-practical-examples)
   - [Example 1: Expanding Socket Listen and Backlog Queues](#example-1-expanding-socket-listen-and-backlog-queues)
   - [Example 2: Tuning TCP Memory & Enabling BBR Congestion Control](#example-2-tuning-tcp-memory--enabling-bbr-congestion-control)
   - [Example 3: NIC Hardware Tuning with ethtool (Ring Buffers & Offloads)](#example-3-nic-hardware-tuning-with-ethtool-ring-buffers--offloads)
   - [Example 4: Virtual Memory & Page Cache Tuning (Swappiness & Dirty Pages)](#example-4-virtual-memory--page-cache-tuning-swappiness--dirty-pages)
   - [Example 5: Production Comprehensive sysctl.d Drop-In Profile](#example-5-production-comprehensive-sysctld-drop-in-profile)
6. [Production Best Practices & Hardening](#production-best-practices--hardening)
7. [Common Failure Modes & Debugging](#common-failure-modes--debugging)
8. [Performance, Security & Scale Considerations](#performance-security--scale-considerations)
9. [Comparison Matrix: TCP Congestion Control Algorithms](#comparison-matrix-tcp-congestion-control-algorithms)
10. [Real-World Interview Scenarios](#real-world-interview-scenarios)
11. [Hands-On Practice Labs](#hands-on-practice-labs)
12. [Cheat Sheet & Quick Reference](#cheat-sheet--quick-reference)

---

## What & Why: Scaling Beyond Out-of-the-Box Defaults

Default Linux kernel settings are intentionally conservative. Distributions such as Ubuntu, Debian, and RHEL choose parameters optimized to boot reliably on modest hardware (1-2 CPU cores, 2 GB RAM) and handle general desktop or low-traffic server workloads without exhausting memory.

In modern enterprise production—supporting 10GbE to 100GbE network fabrics, high-concurrency API gateways, Redis clusters, or distributed databases handling 100,000+ connections per second—these defaults become catastrophic bottlenecks. Un-tuned systems prematurely drop SYN packets, experience severe TCP buffer stalling, trigger unexpected latency jitter during dirty page flushes, or incur NUMA thrashing across multi-socket CPUs. Systematic kernel tuning configures the operating system to exploit high-speed hardware at maximum efficiency.

---

## Core Concepts & Architecture

```
+-------------------------------------------------------------------------+
|                    Linux Network Packet Ingress Path                    |
|                                                                         |
|  +--------------------+                                                 |
|  | Network Cable / SFP| (100GbE Physical Wire)                          |
|  +--------------------+                                                 |
|             |                                                           |
|             v                                                           |
|  +--------------------+                                                 |
|  |  NIC Ring Buffer   | (ethtool -G rx 4096: hardware DMA ring)         |
|  +--------------------+                                                 |
|             | Hardware Interrupt (IRQ) -> MSI-X Vector -> irqbalance   |
|             v                                                           |
|  +--------------------+                                                 |
|  |  NAPI Poll Routine | (net.core.netdev_max_backlog)                   |
|  +--------------------+                                                 |
|             | sk_buff allocation                                        |
|             v                                                           |
|  +--------------------+                                                 |
|  |  TCP Ingress Stack | (net.ipv4.tcp_rmem: min / default / max)        |
|  +--------------------+                                                 |
|             |                                                           |
|             +---> [ SYN Backlog Queue ] (net.ipv4.tcp_max_syn_backlog)  |
|             |                                                           |
|             +---> [ Accept Listen Queue ] (net.core.somaxconn)          |
|                                                                         |
|             v accept()                                                  |
|  +--------------------+                                                 |
|  | Application Socket | (epoll worker thread pinned to NUMA node)       |
|  +--------------------+                                                 |
+-------------------------------------------------------------------------+
```

1. **NIC Ring Buffers**: Circular hardware DMA memory structures on the network card. Packets are dropped at silicon level (`rx_dropped`) if the CPU cannot drain this ring before it fills.
2. **NAPI (New API) & Backlog**: Linux kernel polling mechanism that transitions from interrupt-driven to polling-driven packet consumption under heavy load to prevent "interrupt storms."
3. **TCP Memory Buffers (`tcp_rmem`, `tcp_wmem`)**: Auto-tuning socket buffer windows (receive and transmit) that scale dynamically based on Bandwidth-Delay Product (BDP).
4. **Connection Queues**: Dual queue architecture: the **SYN queue** holds half-open connections during the TCP 3-way handshake; the **Accept queue** holds fully established connections waiting for user-space `accept()`.

---

## Internal Mechanics: Packet Flow & TCP Queueing Stages

### 1. The TCP Connection Queues
```
Client                      Server Kernel                       Application
  |                               |                                  |
  |--- SYN ---------------------->|                                  |
  |                               | [ Enters SYN Queue:              |
  |                               |   tcp_max_syn_backlog ]          |
  |<-- SYN-ACK -------------------|                                  |
  |                               |                                  |
  |--- ACK ---------------------->|                                  |
  |                               | [ Transitions to Accept Queue:   |
  |                               |   net.core.somaxconn ]           |
  |                               |                                  |
  |                               |<=================== accept() ====|
```
- If the **SYN queue** overflows, incoming SYN packets are dropped, or the kernel falls back to **SYN Cookies** (`net.ipv4.tcp_syncookies = 1`).
- If the **Accept queue** overflows (application slow or event loop blocked), new connections are silently dropped, or the kernel sends RST if `net.ipv4.tcp_abort_on_overflow = 1`.

### 2. Bandwidth-Delay Product (BDP) & Window Scaling
To achieve maximum line-rate throughput on high-latency links (e.g., cross-region AWS links or transatlantic transfers), the TCP receive window must equal or exceed the BDP:
$$\text{BDP (bytes)} = \text{Bandwidth (bytes/sec)} \times \text{Round-Trip Time (sec)}$$

For a 10 Gbps link with 50 ms RTT:
$$\text{BDP} = \left(\frac{10 \times 10^9}{8}\right) \times 0.050 = 62.5 \text{ Megabytes}$$

If `net.ipv4.tcp_rmem` max buffer is configured to standard defaults (typically 4 MB to 6 MB), single-stream TCP transfer speed maxes out at less than 10% of available wire speed!

---

## Sysctl & Subsystem Parameter Reference

| Parameter | Recommended High-Scale Value | Description & Impact |
| :--- | :--- | :--- |
| `net.core.somaxconn` | `65535` | Maximum depth of the listen queue for `accept()`. Prevents connection drops under burst. |
| `net.ipv4.tcp_max_syn_backlog`| `65535` | Maximum number of half-open connections in SYN queue before drops begin. |
| `net.core.netdev_max_backlog` | `100000` | Number of packets queued on input interface before processing by kernel IP stack. |
| `net.ipv4.tcp_rmem` | `4096 87380 67108864` | `min default max` TCP read buffer in bytes (64MB max for 10GbE/100GbE). |
| `net.ipv4.tcp_wmem` | `4096 65536 67108864` | `min default max` TCP write buffer in bytes. |
| `net.ipv4.tcp_congestion_control`| `bbr` | Selects Google BBR (model-based) over loss-based Cubic. |
| `net.ipv4.tcp_slow_start_after_idle`| `0` | Disables TCP window shrinking after connection goes idle (critical for persistent HTTP/2 & gRPC). |
| `vm.swappiness` | `10` (or `1` for DBs) | Bias toward reclaiming page cache before paging anonymous memory to swap. |
| `vm.dirty_ratio` | `10` | Percentage of total memory where process is forced to block and synchronously flush dirty pages. |
| `vm.dirty_background_ratio` | `5` | Percentage of memory where `kswapd` begins flushing dirty pages in background. |

---

## Progressive Practical Examples

### Example 1: Expanding Socket Listen and Backlog Queues

Remediate connection timeout errors on high-throughput Nginx or Envoy proxy clusters.

```bash
#!/usr/bin/env bash
# Step 1: Enlarge listen and connection backlogs
set -euo pipefail

# Apply live via sysctl
sudo sysctl -w net.core.somaxconn=65535
sudo sysctl -w net.ipv4.tcp_max_syn_backlog=65535
sudo sysctl -w net.core.netdev_max_backlog=100000

# Verify listen backlog drops
echo "Current SYN queue drop counter:"
netstat -s | grep -i "listen drops" || echo "0 drops"
```

### Example 2: Tuning TCP Memory & Enabling BBR Congestion Control

Enable Google BBR to maintain high throughput and low latency across lossy or long-haul network links.

```bash
#!/usr/bin/env bash
# Step 2: Enable TCP BBR and high-bandwidth buffers
set -euo pipefail

# Ensure kernel module is loaded
sudo modprobe tcp_bbr

# Configure queuing discipline to Fair Queueing (required for BBR pacing)
sudo sysctl -w net.core.default_qdisc=fq

# Set congestion control algorithm to BBR
sudo sysctl -w net.ipv4.tcp_congestion_control=bbr

# Enlarge TCP auto-tuning buffer windows (64MB max)
sudo sysctl -w net.ipv4.tcp_rmem="4096 87380 67108864"
sudo sysctl -w net.ipv4.tcp_wmem="4096 65536 67108864"
sudo sysctl -w net.core.rmem_max=67108864
sudo sysctl -w net.core.wmem_max=67108864

# Verify BBR is active
sysctl net.ipv4.tcp_congestion_control
```

### Example 3: NIC Hardware Tuning with ethtool (Ring Buffers & Offloads)

Maximize hardware NIC DMA queues and verify packet offload engines on physical network interfaces.

```bash
#!/usr/bin/env bash
# Step 3: Hardware NIC optimization
set -euo pipefail

INTERFACE="eth0"

echo "=== Inspecting NIC Ring Buffer Capacity ==="
sudo ethtool -g "${INTERFACE}"

echo "Maximizing RX and TX ring buffers..."
# Query maximum supported ring buffer capacity
MAX_RX=$(sudo ethtool -g "${INTERFACE}" | awk '/Pre-set maximums:/,/RX:/' | awk '/RX:/ {print $2}' | head -n 1)
MAX_TX=$(sudo ethtool -g "${INTERFACE}" | awk '/Pre-set maximums:/,/TX:/' | awk '/TX:/ {print $2}' | head -n 1)

if [[ -n "${MAX_RX}" && "${MAX_RX}" -gt 0 ]]; then
    sudo ethtool -G "${INTERFACE}" rx "${MAX_RX}" tx "${MAX_TX}"
    echo "Ring buffers set to RX: ${MAX_RX}, TX: ${MAX_TX}"
fi

echo "=== Inspecting Hardware Offloads (TSO, GSO, GRO) ==="
sudo ethtool -k "${INTERFACE}" | grep -E "tcp-segmentation-offload|generic-segmentation-offload|generic-receive-offload"

# Enable hardware checksumming and offload if supported
sudo ethtool -K "${INTERFACE}" tso on gso on gro on rxhash on || true
```

### Example 4: Virtual Memory & Page Cache Tuning (Swappiness & Dirty Pages)

Eliminate database freeze-ups caused by massive unconstrained background disk flushes.

```bash
#!/usr/bin/env bash
# Step 4: Virtual memory and page cache latency mitigation
set -euo pipefail

# Lower swappiness to prevent premature paging of active process heaps
sudo sysctl -w vm.swappiness=10

# Configure background flusher to start early (5% of RAM)
sudo sysctl -w vm.dirty_background_ratio=5

# Lower synchronous blocking flush threshold to 10%
sudo sysctl -w vm.dirty_ratio=10

# Increase vfs_cache_pressure to reclaim dentry and inode caches aggressively
sudo sysctl -w vm.vfs_cache_pressure=50

echo "Virtual memory parameters active:"
sysctl vm.swappiness vm.dirty_background_ratio vm.dirty_ratio vm.vfs_cache_pressure
```

### Example 5: Production Comprehensive sysctl.d Drop-In Profile

A complete, production-hardened sysctl configuration file designed for high-concurrency Linux application servers and gateways.

```bash
#!/usr/bin/env bash
# Step 5: Install persistent enterprise production sysctl configuration
set -euo pipefail

CONF_FILE="/etc/sysctl.d/99-production-high-performance.conf"

sudo tee "${CONF_FILE}" > /dev/null << 'EOF'
# ====================================================================
# Enterprise Linux Production Optimization Profile (2026 Baseline)
# ====================================================================

# 1. Network Queue Limits & Concurrency
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 65535
net.core.netdev_max_backlog = 100000
net.ipv4.ip_local_port_range = 1024 65535

# 2. TCP Buffer Windows & Memory (64MB Max for 10GbE / 100GbE)
net.core.rmem_max = 67108864
net.core.wmem_max = 67108864
net.core.rmem_default = 1048576
net.core.wmem_default = 1048576
net.ipv4.tcp_rmem = 4096 87380 67108864
net.ipv4.tcp_wmem = 4096 65536 67108864

# 3. Congestion Control & Modern Pacing
net.core.default_qdisc = fq
net.ipv4.tcp_congestion_control = bbr
net.ipv4.tcp_slow_start_after_idle = 0
net.ipv4.tcp_window_scaling = 1
net.ipv4.tcp_timestamps = 1
net.ipv4.tcp_sack = 1

# 4. Connection Lifecycle & TIME_WAIT Management
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_fin_timeout = 15
net.ipv4.tcp_max_tw_buckets = 2000000

# 5. SYN Flood Defense & Path MTU
net.ipv4.tcp_syncookies = 1
net.ipv4.tcp_synack_retries = 2
net.ipv4.tcp_mtu_probing = 1

# 6. Virtual Memory & Storage Flushes
vm.swappiness = 10
vm.dirty_background_ratio = 5
vm.dirty_ratio = 10
vm.vfs_cache_pressure = 50
vm.max_map_count = 262144

# 7. File Handles & System Limits
fs.file-max = 2097152
fs.inotify.max_user_watches = 524288
EOF

# Apply configuration immediately without reboot
sudo sysctl --system

echo "High-performance profile deployed successfully to ${CONF_FILE}."
```

---

## Production Best Practices & Hardening

- **Never enable deprecated `tcp_tw_recycle`**: In older kernels (pre-4.12), `tcp_tw_recycle` was often erroneously recommended. It breaks connections originating from behind NAT gateways because it rejects packets with older TCP timestamps. The parameter was completely removed in Linux 4.12+. Use `net.ipv4.tcp_tw_reuse = 1` instead.
- **Match application listen backlog to OS backlog**: Setting `net.core.somaxconn = 65535` is insufficient if the application passes a small backlog to `listen(fd, backlog)`. For example, Nginx defaults to `listen 80 default_server backlog=511;`. Update application configs to match.
- **Pin IRQs on Multi-Queue NICs**: Ensure `irqbalance` is running or manually pin NIC RX/TX MSI-X interrupt vectors to specific CPU cores aligned with the PCIe NUMA node using `/proc/irq/<num>/smp_affinity`.
- **Set `vm.max_map_count` for Memory-Mapped Engines**: Databases and search engines (Elasticsearch, Kafka, Lucene) require at least `262144` memory map areas to avoid out-of-memory errors during memory-mapped file operations.
- **Disable Transparent Huge Pages (THP) for Latency-Sensitive Databases**: Redis, MongoDB, and PostgreSQL suffer severe tail-latency spikes when THP allocates or splits 2MB pages synchronously. Disable THP via `echo never > /sys/kernel/mm/transparent_hugepage/enabled`.

---

## Common Failure Modes & Debugging

| Symptom / Error | Root Cause | Diagnosis Command | Resolution |
| :--- | :--- | :--- | :--- |
| `connection reset by peer` under traffic burst | Accept queue overflowed; `tcp_abort_on_overflow` triggered. | `netstat -s \| grep "listen drops"` | Increase `net.core.somaxconn` and application `listen(..., backlog)`. |
| Packet drops visible on NIC (`rx_dropped` / `rx_missed_errors`) | Hardware NIC ring buffer too small to absorb burst between IRQ polls. | `ethtool -S eth0 \| grep drop` | Enlarge ring buffer using `ethtool -G eth0 rx 4096`. |
| Cross-datacenter throughput caps at low speed | TCP receive buffer window too small for Bandwidth-Delay Product. | `sysctl net.ipv4.tcp_rmem` | Increase `net.ipv4.tcp_rmem` max window to 32MB or 64MB. |
| Database stalls for several seconds during disk writes | High `dirty_ratio` allowed tens of gigabytes of unwritten pages to accumulate. | `grep Dirty /proc/meminfo` | Lower `vm.dirty_ratio` to 10 and `vm.dirty_background_ratio` to 5. |
| High CPU usage in `ksoftirqd` | CPU saturated processing network packet software interrupts. | `top` (check `%si` column) | Enable RPS/RFS or distribute NIC interrupts across cores with `irqbalance`. |

---

## Performance, Security & Scale Considerations

```
+--------------------------------------------------------------------------+
| NUMA & Cache Locality in High-Throughput Packet Processing               |
|                                                                          |
| [ NUMA Node 0: CPUs 0-15 ]             [ NUMA Node 1: CPUs 16-31 ]       |
|            |                                        |                    |
|   PCIe Gen 5 x16 Bus                                |                    |
|            |                                        |                    |
|   [ 100GbE NIC ]                                    |                    |
|   DMA writes direct to Node 0 RAM                   |                    |
|            |                                        |                    |
|  * Fast Local Cache Access *             * Slow UPI/QPI Bus Cross-Traffic*
|  (Process pinned to Node 0)              (Process running on Node 1)     |
|  Latency: ~60ns                          Latency: ~140ns (Tail Spikes)   |
+--------------------------------------------------------------------------+
```

1. **NUMA Alignment**: In multi-socket servers, network cards are physically wired to a specific PCIe root complex on a specific NUMA node. Always execute `cat /sys/class/net/<interface>/device/numa_node` and pin worker processes to that exact node using `numactl --cpunodebind=<node>`.
2. **TCP SYN Cookie Overhead**: While `net.ipv4.tcp_syncookies = 1` prevents Denial-of-Service exhaustion of the SYN backlog, computing cryptographic SHA-1 hashes consumes CPU. Prevent SYN floods at wire speed using XDP eBPF filters whenever possible.

---

## Comparison Matrix: TCP Congestion Control Algorithms

| Algorithm | Model / Control Metric | Throughput on Lossy Links | Bufferbloat / Latency | Kernel Support |
| :--- | :--- | :--- | :--- | :--- |
| **Cubic** | Loss-based (Cubic polynomial) | Poor (halves window on packet drop) | High (fills router buffers) | Default in most distros |
| **BBR (v1/v2/v3)** | Model-based (Max BW & Min RTT) | Excellent (ignores random loss) | Extremely Low (drains buffers)| Kernel 4.9+ (Built-in) |
| **Reno** | Loss-based (AIMD) | Extremely Poor | High | Legacy Default |
| **DCTCP** | ECN (Explicit Congestion Notif) | Ultra-high in Datacenters | Zero queuing delay | Specialized Datacenters |

---

## Real-World Interview Scenarios

### Scenario 1: Why does a 10Gbps cross-region connection achieve only 150 Mbps with default settings?
<details>
<summary>Detailed Answer</summary>
This is caused by the Bandwidth-Delay Product (BDP) constraint and default TCP buffer limits. With a 10Gbps link and a 60ms Round-Trip Time (RTT), the BDP is:
$$\text{BDP} = 1.25 \times 10^9 \text{ bytes/s} \times 0.060 \text{ s} = 75 \text{ Megabytes}$$
Default Linux `net.ipv4.tcp_rmem` typically caps the maximum receive window at 4MB or 6MB. Because TCP cannot transmit more unacknowledged data than the receiver's window, the maximum throughput is mathematically capped at:
$$\text{Throughput} = \frac{\text{Window Size}}{\text{RTT}} = \frac{4 \times 10^6 \times 8}{0.060} \approx 533 \text{ Mbps}$$
Raising `net.ipv4.tcp_rmem` and `wmem` max fields to 67MB+ allows the TCP window to scale up to the true BDP, unlocking full line rate.
</details>

### Scenario 2: What is the difference between `net.core.somaxconn` and `net.ipv4.tcp_max_syn_backlog`?
<details>
<summary>Detailed Answer</summary>
The Linux TCP implementation uses two distinct connection queues:
1. `net.ipv4.tcp_max_syn_backlog`: Governs the **SYN queue** (half-open connections). This queue holds connections that have sent a SYN packet and received a SYN-ACK, but have not yet returned the final ACK to complete the 3-way handshake.
2. `net.core.somaxconn`: Governs the **Accept queue** (fully established connections). Once the 3-way handshake completes, the connection transitions from the SYN queue to the Accept queue, where it sits waiting for the application to invoke `accept()`.
</details>

### Scenario 3: Why should `net.ipv4.tcp_slow_start_after_idle` be disabled on web and API servers?
<details>
<summary>Detailed Answer</summary>
By default (`1`), Linux resets the TCP congestion window (`cwnd`) back to the Initial Congestion Window (IW10) whenever a connection has been idle for more than one retransmission timeout (RTO). For persistent HTTP/1.1 keep-alive or HTTP/2 and gRPC streams, this forces subsequent API responses to undergo slow-start ramp-up, incurring unnecessary round-trip latency. Setting it to `0` preserves the negotiated congestion window across idle periods.
</details>

### Scenario 4: What is the role of Receive Side Scaling (RSS) and Receive Packet Steering (RPS)?
<details>
<summary>Detailed Answer</summary>
**RSS (Receive Side Scaling)** is a hardware NIC feature that hashes incoming packet 4-tuples (src IP, src port, dst IP, dst port) to distribute packets across multiple hardware receive queues, firing distinct hardware MSI-X interrupts handled by separate CPU cores.
**RPS (Receive Packet Steering)** is the software emulation of RSS within the Linux kernel. When a NIC has only a single hardware queue, RPS calculates the packet hash in software and forwards packet processing to a configured bitmask of CPU cores via inter-processor interrupts (IPIs).
</details>

### Scenario 5: Why does a Linux host with 64GB RAM freeze when writing to a slow USB or SAN disk?
<details>
<summary>Detailed Answer</summary>
This is the classic "dirty page cache flush freeze." By default, `vm.dirty_ratio` might be set to 20% or 30%. On a 64GB machine, this allows 12GB to 18GB of unwritten dirty pages to accumulate in RAM before the kernel enforces synchronous flushing. Once the threshold is breached, user-space processes performing I/O are blocked synchronously while the kernel flushes the dirty cache to the slow storage medium, stalling the system. Setting `vm.dirty_ratio=10` and `vm.dirty_background_ratio=5` forces smaller, continuous background flushes.
</details>

### Scenario 6: Explain why BBR handles packet loss better than Cubic.
<details>
<summary>Detailed Answer</summary>
Cubic is a **loss-based** congestion control algorithm. It assumes that any packet loss is evidence of network congestion and immediately cuts its congestion window in half. On modern wireless or long-distance internet paths, transient packet loss often occurs due to noise or transient drops rather than saturated buffers. In contrast, **BBR (Bottleneck Bandwidth and RTT)** is a model-based algorithm that measures maximum delivery rate and minimum round-trip time. It paces packet transmission to match bottleneck bandwidth regardless of packet loss, maintaining high throughput even on lossy links.
</details>

### Scenario 7: What does the `%si` metric in `top` indicate, and how do you resolve it?
<details>
<summary>Detailed Answer</summary>
`%si` measures CPU percentage spent servicing **Software Interrupts** (bottom-half processing, primarily network packet handling in `ksoftirqd`). If `%si` is pegged at 100% on a single CPU core while other cores are idle, it indicates that all network packets are hitting a single NIC queue tied to that core. Resolve this by enabling multi-queue RSS on the NIC, spreading hardware IRQs across cores via `irqbalance`, or configuring RPS bitmasks in `/sys/class/net/<dev>/queues/rx-0/rps_cpus`.
</details>

### Scenario 8: What is the purpose of `vm.vfs_cache_pressure`?
<details>
<summary>Detailed Answer</summary>
`vm.vfs_cache_pressure` controls the kernel's tendency to reclaim Directory Entry (`dentry`) and Inode objects from the VFS slab cache relative to page cache memory. The default value is `100` (fair balance). Setting it to `50` instructs the kernel to prefer retaining inode and directory metadata in RAM, which dramatically accelerates directory traversal and file lookup performance on systems with millions of small files.
</details>

---

## Hands-On Practice Labs

### Lab 1: Query and Inspect Active Sysctl Settings
<details>
<summary>Lab Instructions & Verification</summary>

1. Check active TCP congestion control algorithm:
   ```bash
   sysctl net.ipv4.tcp_congestion_control
   ```
2. Query available congestion control algorithms supported by your kernel:
   ```bash
   sysctl net.ipv4.tcp_available_congestion_control
   ```
3. Inspect listen backlog queue maximum:
   ```bash
   sysctl net.core.somaxconn
   ```
</details>

### Lab 2: Analyze Socket Buffer Window Autotuning
<details>
<summary>Lab Instructions & Verification</summary>

1. Inspect active TCP read and write memory windows:
   ```bash
   sysctl net.ipv4.tcp_rmem net.ipv4.tcp_wmem
   ```
2. Interpret the three integer values printed:
   - Value 1: Minimum buffer size (allocated per socket).
   - Value 2: Default buffer size (initial window).
   - Value 3: Maximum buffer size (ceiling for window scaling).
</details>

### Lab 3: Inspect Network Interface Ring Buffers
<details>
<summary>Lab Instructions & Verification</summary>

1. Identify your primary network interface:
   ```bash
   ip route show default | awk '{print $5}'
   ```
2. Query the current and maximum hardware ring buffer settings:
   ```bash
   sudo ethtool -g $(ip route show default | awk '{print $5}')
   ```
</details>

### Lab 4: Measure Memory Dirty Page Accumulation
<details>
<summary>Lab Instructions & Verification</summary>

1. Inspect active dirty pages awaiting writeback:
   ```bash
   grep -E "Dirty|Writeback" /proc/meminfo
   ```
2. In a second terminal, write a 200MB file without syncing:
   ```bash
   dd if=/dev/zero of=/tmp/dirty_test bs=1M count=200
   ```
3. Immediately re-run `grep Dirty /proc/meminfo` to observe the temporary spike before writeback clears it.
4. Clean up:
   ```bash
   rm -f /tmp/dirty_test
   ```
</details>

### Lab 5: Discover Network Drop Counters via Netstat
<details>
<summary>Lab Instructions & Verification</summary>

1. Query detailed TCP protocol error statistics:
   ```bash
   netstat -s | grep -E "listen|overflow|dropped|pruned"
   ```
2. Verify if any listen queue overflows or drops have occurred since boot.
</details>

### Lab 6: Verify TCP BBR Activation
<details>
<summary>Lab Instructions & Verification</summary>

1. Check if the BBR module is compiled or loaded:
   ```bash
   lsmod | grep bbr || grep -i bbr /boot/config-$(uname -r)
   ```
2. Verify the active queuing discipline:
   ```bash
   sysctl net.core.default_qdisc
   ```
</details>

---

## Cheat Sheet & Quick Reference

```bash
# === Real-Time Sysctl Inspection & Modification ===
sysctl -a | grep <param>                       # Query kernel parameter
sudo sysctl -w <param>=<value>                 # Temporarily apply parameter
sudo sysctl --system                           # Reload all /etc/sysctl.d/ files

# === High-Performance Network One-Liners ===
sudo sysctl -w net.core.somaxconn=65535        # Maximize listen queue
sudo sysctl -w net.ipv4.tcp_max_syn_backlog=65535 # Maximize SYN backlog
sudo sysctl -w net.ipv4.tcp_congestion_control=bbr # Enable BBR
sudo sysctl -w net.core.default_qdisc=fq       # Required for BBR

# === NIC & Hardware Inspection ===
ethtool -g eth0                                # Check ring buffer size
sudo ethtool -G eth0 rx 4096 tx 4096           # Maximize ring buffer
ethtool -k eth0                                # Inspect TSO, GRO, GSO offloads
ethtool -S eth0 | grep -i drop                 # Check for hardware packet drops

# === Memory & Dirty Pages ===
sysctl vm.swappiness=10                        # Minimize swap usage
sysctl vm.dirty_ratio=10                       # Synchronous flush threshold
sysctl vm.dirty_background_ratio=5             # Background flush threshold
```

---
*Next Note: [07-linux-security-modules-and-capabilities.md](file:///04-devops/linux/06-advanced/07-linux-security-modules-and-capabilities.md)*
