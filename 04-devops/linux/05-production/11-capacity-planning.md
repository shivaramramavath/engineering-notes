# Linux Capacity Planning: Sizing Math, Queueing Theory, and Forecasting

| Metadata | Details |
| :--- | :--- |
| **Topic** | Infrastructure Capacity Planning, Resource Sizing Math, Queueing Theory, Historical Trending (`sar`) |
| **Module** | `05-production` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `05-production/05-troubleshooting.md`, `05-production/06-performance.md`, `05-production/07-monitoring-basics.md` |
| **Target Audience** | Systems Architects, Principal SREs, Capacity Planners, Engineering Managers |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools / Models** | `sar`, `sysstat`, Kingman's Formula, Little's Law, PromQL linear regression |

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

In production systems engineering, capacity planning is the science of answering two existential business questions:
1. **When will our infrastructure run out of resources?**
2. **What exact hardware specifications (CPU, RAM, IOPS, Network) are required to support projected business growth without latency collapse or budget waste?**

Failure to execute mathematical capacity planning leads to two dangerous extremes:
- **Severe Under-Provisioning**: Infrastructure hits saturation cliffs unexpectedly. As utilization exceeds 80%, queueing theory dictates that wait times explode non-linearly, resulting in cascading timeouts and service degradation.
- **Gross Over-Provisioning**: Engineering teams panic-provision massive 128-core VMs that sit at 4% average utilization, generating hundreds of thousands of dollars in wasted cloud spend.

By mastering resource headroom math, applying **Little's Law** and **Kingman's Queueing Formula**, extracting historical trends with `sar`, and forecasting growth using linear regression, engineers can right-size infrastructure with mathematical precision.

---

## Core Concepts

### The Knee of the Curve: Queueing Theory and Non-Linear Saturation
Many engineers assume that latency increases linearly with load: that a server at 90% utilization will be only slightly slower than a server at 50% utilization.
**This assumption is mathematically false.**

According to **Kingman's Formula** (the foundation of M/M/1 and G/G/1 queueing models), the mean waiting time in queue ($W_q$) is governed by:
$$W_q \approx \left( \frac{\rho}{1 - \rho} \right) \left( \frac{c_a^2 + c_s^2}{2} \right) \tau$$
- $\rho$: Resource utilization ($0 \le \rho < 1$).
- $\tau$: Mean service time.
- $c_a, c_s$: Coefficients of variation for arrival and service intervals.

Notice the term $\frac{\rho}{1 - \rho}$:
- At $\rho = 0.50$ (50% utilization): $\frac{0.50}{1 - 0.50} = \mathbf{1.0}$ (Queue wait time equals 1 unit).
- At $\rho = 0.80$ (80% utilization): $\frac{0.80}{1 - 0.80} = \mathbf{4.0}$ (Wait time quadruples).
- At $\rho = 0.95$ (95% utilization): $\frac{0.95}{1 - 0.95} = \mathbf{19.0}$ (Wait time is 19x higher!).
- As $\rho \to 1.0$: Wait time approaches **infinity**.

```
    Queue
    Wait
    Time
     ^                                         *
     |                                        *
     |                                       *
     |                                      *
     |                                    *
     |                                  *  <-- Saturation Cliff
     |                           *
     |            *
     | *
     +----------------------------------------->
       0%         50%         70%    85%  100%
                    Resource Utilization (rho)
```
*Takeaway*: **80% utilization is not "20% headroom"; it is the brink of exponential latency collapse.**

### Little's Law
In any stable queueing system, the average number of concurrent requests ($L$) in the system equals the arrival rate ($\lambda$) multiplied by the average response time ($W$):
$$L = \lambda \times W$$
*Practical Application*: If your API receives 2,000 requests per second ($\lambda = 2000$), and the average response time is 50ms ($W = 0.05\text{s}$):
$$L = 2000 \times 0.05 = \mathbf{100 \text{ concurrent connections}}$$
Your web server and thread pools must be sized to handle at least 100 concurrent executions simultaneously.

### The Four Pillars of Sizing Math
1. **CPU Sizing**: Calculated based on baseline thread execution time, peak traffic concurrency, and interrupt overhead ($1.3\times$ to $1.5\times$ headroom factor).
2. **Memory Sizing**: Working Set Size (RSS) + OS Base Overhead (2GB) + Storage Page Cache (typically 20%–50% of database dataset size).
3. **Storage IOPS and Throughput**: Calculated by peak transactions per second $\times$ I/O blocks per transaction. Must account for write amplification in SSDs/RAID.
4. **Network Bandwidth**: Peak egress bits per second calculated from payload size $\times$ request rate, factoring in TCP/IP encapsulation overhead.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                  Historical Telemetry Collection                        |
|                                       sysstat / sar daemons                             |
|                               /var/log/sysstat/saDD binary logs                         |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                    Trend Analysis Engine                                |
|   - Calculates p95 / p99 Peak-to-Average Ratio (PAR)                                    |
|   - Analyzes weekly seasonality (e.g., Monday 09:00 vs Sunday 03:00)                    |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                 Capacity Forecasting Model                              |
|                                                                                         |
|   1. Linear Regression Growth: y = mx + b                                               |
|   2. Queueing Model: Predicts latency cliff based on Kingman's formula                 |
|   3. Headroom Safety Buffer: Enforces rho <= 0.70 at peak                               |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                              Right-Sizing Provisioning Target                           |
|       Selects optimal Cloud Instance / Bare-Metal Spec (Cores, RAM, IOPS, Network)       |
+-----------------------------------------------------------------------------------------+
```

---

## Syntax & CLI Tools

### Historical Telemetry Extraction with `sar`
The `sysstat` suite records system activity to `/var/log/sysstat/sa<DD>` every 10 minutes.

```bash
# Query historical CPU utilization for a specific day of the month (e.g. Day 15)
sar -u -f /var/log/sysstat/sa15

# Query memory utilization history
sar -r -f /var/log/sysstat/sa15

# Query disk I/O activity history
sar -d -p -f /var/log/sysstat/sa15

# Query network bandwidth history
sar -n DEV -f /var/log/sysstat/sa15

# Generate comprehensive human-readable daily report
sar -A > /tmp/system_report.txt
```

---

## Progressive Practical Examples

### Example 1: Database Memory Sizing Formula
**Scenario**: Sizing an enterprise PostgreSQL database host holding a 500GB active transactional dataset.

```text
=============================================================================
POSTGRESQL MEMORY SIZING FORMULA
=============================================================================
1. OS Base Overhead:                     4 GB
2. Shared Buffers (25% of RAM):         Recommended starting baseline
3. Effective Page Cache (50% of RAM):   Serves hot data without disk I/O
4. Work Mem (Per-query sort/hash):      Connections x work_mem
   - Max Connections = 300
   - work_mem = 64 MB
   - Max Query Memory = 300 * 0.064 GB = 19.2 GB
5. Maintenance Work Mem (VACUUM):        2 GB
-----------------------------------------------------------------------------
Target RAM Calculation:
Let Total RAM = R
R = 4 GB + (0.25 * R) + (0.50 * R) + 19.2 GB + 2 GB
R - 0.75R = 25.2 GB
0.25R = 25.2 GB
R = 25.2 / 0.25 = 100.8 GB -> Round up to 128 GB RAM!
=============================================================================
```
*Conclusion*: Provision a **128 GB RAM** instance. Set `shared_buffers = 32GB`, `effective_cache_size = 64GB`, `work_mem = 64MB`.

### Example 2: CPU Right-Sizing Math from Little's Law
**Scenario**: An API cluster currently processes 5,000 requests per second at peak. Profiling reveals that each request requires 8ms of CPU processing time. The team projects traffic will double to 10,000 req/sec next quarter. How many CPU cores are required to ensure utilization stays below the safe 70% threshold?

```text
1. Compute Total CPU Time Required per Second at Peak:
   Arrival Rate (lambda) = 10,000 requests / second
   Service Time (tau)    = 0.008 seconds / request
   Required CPU Seconds  = 10,000 * 0.008 = 80 CPU-seconds per wall-clock second

2. Determine Saturated Core Count:
   80 CPU-seconds / second requires exactly 80 dedicated 100% saturated CPU cores.

3. Apply Safety Headroom Factor (rho <= 0.70):
   Target Cores = Saturated Cores / 0.70
   Target Cores = 80 / 0.70 = 114.28 cores

4. Factoring in Hyperthreading Efficiency (HT adds ~25% real throughput):
   Physical Cores Required = 114.28 / 1.25 = ~92 physical cores (or ~115 vCPUs).

5. Architecture Decision:
   Deploy 8 worker nodes, each provisioned with 16 vCPUs (Total: 128 vCPUs).
   Guarantees average utilization across the fleet remains at ~62%, well below the saturation cliff!
```

### Example 3: Storage IOPS Sizing and Headroom Calculation
**Scenario**: Sizing cloud block storage (e.g. AWS EBS gp3 or Azure Managed Disk) for an e-commerce order service.

```bash
#!/usr/bin/env bash
set -euo pipefail

# Peak Transaction Volume
ORDERS_PER_SEC=150
READ_OPS_PER_ORDER=12
WRITE_OPS_PER_ORDER=6
BLOCK_SIZE_KB=8

TOTAL_READ_IOPS=$(( ORDERS_PER_SEC * READ_OPS_PER_ORDER ))
TOTAL_WRITE_IOPS=$(( ORDERS_PER_SEC * WRITE_OPS_PER_ORDER ))
TOTAL_IOPS=$(( TOTAL_READ_IOPS + TOTAL_WRITE_IOPS ))

# Throughput in MB/s: (IOPS * Block Size in KB) / 1024
THROUGHPUT_MBPS=$(( (TOTAL_IOPS * BLOCK_SIZE_KB) / 1024 ))

# Headroom: Apply 1.5x buffer for traffic bursts and background VACUUM/maintenance
PROVISIONED_IOPS=$(awk "BEGIN {print int(${TOTAL_IOPS} * 1.5)}")
PROVISIONED_THROUGHPUT=$(awk "BEGIN {print int(${THROUGHPUT_MBPS} * 1.5)}")

echo "=== Storage Capacity Requirements ==="
echo "Raw Read IOPS:            ${TOTAL_READ_IOPS}"
echo "Raw Write IOPS:           ${TOTAL_WRITE_IOPS}"
echo "Total Raw IOPS:           ${TOTAL_IOPS}"
echo "Raw Throughput:           ${THROUGHPUT_MBPS} MB/s"
echo "--------------------------------------"
echo "PROVISIONED IOPS (1.5x):  ${PROVISIONED_IOPS}"
echo "PROVISIONED MB/s (1.5x):  ${PROVISIONED_THROUGHPUT} MB/s"
```

### Example 4: Linear Growth Forecasting with PromQL
Predicting exactly when a 2TB data partition will reach 100% capacity using historical 30-day write rates.

```promql
# PromQL Linear Prediction Query:
# Predict the number of days until free disk space hits 0 bytes
(
  predict_linear(node_filesystem_free_bytes{mountpoint="/data"}[30d], 365 * 86400)
)
```
```promql
# Alerting rule: Alert if free space is projected to reach 0 within 30 days:
predict_linear(node_filesystem_free_bytes{mountpoint="/data"}[14d], 30 * 86400) < 0
```

### Example 5: Historical Capacity Audit Script with `sar`
Extract average vs peak resource utilization from local sysstat archives to detect over-provisioned nodes.

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== System Historical Utilization Audit ==="

# Check if sysstat data directory exists
if [ -d /var/log/sysstat ]; then
    # Get CPU utilization stats for the current month
    echo "Historical CPU Utilization (Last 7 Days):"
    sar -u | tail -n 10
    
    echo ""
    echo "Historical Memory Utilization:"
    sar -r | tail -n 10
    
    echo ""
    echo "Peak Load Average (15-min):"
    sar -q | awk '{print $5}' | sort -nr | head -n 1
else
    echo "sysstat historical logs not available. Enable with: sudo systemctl enable --now sysstat"
fi
```

---

## Best Practices

1. **Enforce the 70% Peak Utilization Rule**: Sizing infrastructure for 70% utilization at **peak traffic** guarantees that brief 20% traffic spikes will not breach the 85% saturation cliff where queueing delays explode exponentially.
2. **Account for Peak-to-Average Ratio (PAR)**: Average utilization is misleading. A system sitting at 20% average utilization that spikes to 95% every day at 12:00 PM is severely under-provisioned. Size infrastructure to the **95th or 99th percentile peak**, not the 24-hour arithmetic average.
3. **Always Decouple Storage Capacity from Storage IOPS**: In modern cloud infrastructure (AWS EBS gp3, Azure Disk, GCP Persistent Disk), storage **size (GB)** is billed separately from provisioned **IOPS** and **throughput (MB/s)**. Sizing disks purely for gigabytes often leaves applications throttled on IOPS. Always size both axes independently.
4. **Factor in Hyperthreading Realities**: A virtual CPU (vCPU) on a hyperthreaded core (Intel Hyper-Threading / AMD SMT) provides **only 20% to 30% additional compute throughput** compared to a dedicated physical execution core. Sizing calculations must not equate 2 vCPUs to 2 physical cores.
5. **Implement Automated Downsizing for Idle Nodes**: Use historical monitoring metrics to identify "zombie instances" whose peak CPU never exceeds 10% and peak memory never exceeds 20% over 30 days. Downsize them systematically to recover cloud spend.
6. **Include Maintenance and Recovery Overhead in Headroom Math**: Database nodes require extra CPU and memory headroom to execute background maintenance (PostgreSQL autovacuum, MySQL InnoDB purge, Cassandra anti-entropy repairs). Sizing without maintenance headroom causes database queries to time out whenever maintenance runs.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Sizing for 95% average CPU utilization | Brief traffic spikes push utilization past 98%, triggering the queueing saturation cliff and cascading timeouts. | Size for maximum **70% utilization at peak**. |
| Sizing memory based purely on application heap size | Neglects operating system filesystem page cache; disk reads cannot be cached in RAM, driving up disk IOPS. | Add page cache allocation (20%–50% of active working set) to RAM sizing. |
| Equating 1 vCPU to 1 physical CPU core | vCPUs share execution units on hyperthreaded cores; delivers only ~1.25x the throughput of 1 physical core. | Factor in 0.6x to 0.7x discounting when sizing from physical benchmarks. |
| Using arithmetic mean instead of p99 latency | Averages mask severe latency tail events and microburst saturation. | Base capacity models on 95th and 99th percentile metrics. |
| Sizing storage volume size without verifying IOPS limits | Disks have sufficient gigabytes but run out of IOPS, throttling reads and writes. | Size both GB and IOPS/throughput independently. |
| Ignoring network bandwidth caps on cloud instances | Small cloud VMs (e.g. `t3.medium`) have burstable network bandwidth capped at 1 Gbps or 5 Gbps; traffic bursts trigger silent packet drops. | Size instances according to baseline network bandwidth limits. |

---

## Performance, Security & Cost Implications

### Performance
- **Queueing Cliff Avoidance**: Operating infrastructure at or below 70% utilization guarantees that packet queues, socket accept queues, and thread pools remain shallow, delivering consistent, predictable sub-millisecond p99 latencies.

### Security
- **Denial of Service (DoS) Resilience**: A system operating with 30% headroom can absorb unexpected DDoS volumetric traffic bursts long enough for automated scrubbing centers, WAFs, or auto-scaling rules to react. Systems operating at 95% utilization collapse immediately upon the slightest traffic increase.

### Cost
- **Right-Sizing ROI**: The difference between a properly sized cluster ($4,000/month) and an un-modeled, panic-provisioned cluster ($22,000/month) is hundreds of thousands of dollars annually. Capacity planning provides the mathematical justification required to defend infrastructure budgets to CFOs.

---

## Comparison Table / Alternatives

| Sizing Paradigm | Core Metric Used | Advantages | Disadvantages |
| :--- | :--- | :--- | :--- |
| **Static Peak Over-Provisioning** | Maximum historical peak + 100% | Zero latency risk, simple | High cloud cost waste |
| **Queueing-Theory Model (Recommended)**| Arrival rate ($\lambda$), Service time ($\tau$), $\rho \le 0.70$| Mathematically optimal, predicts saturation cliffs | Requires baseline benchmarking |
| **Reactive Auto-Scaling** | Dynamic CPU% threshold (e.g. 70%) | Adjusts compute automatically | 3–5 min spin-up lag; does not work for stateful databases |
| **Linear Trend Extrapolation** | Historical 30-day growth slope | Great for disk and storage planning | Fails to predict sudden viral traffic spikes |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: An API service operates normally at 70% CPU utilization, but when traffic increases by just 15%, response latency spikes by 400%. Why?
<details>
<summary>View Solution</summary>

**Explanation**: This is the real-world manifestation of **Kingman's Formula for Queueing Saturation**.
- At 70% utilization ($\rho = 0.70$), the queue wait time multiplier is:
  $$\frac{0.70}{1 - 0.70} \approx \mathbf{2.33}$$
- When traffic increases by 15%, utilization reaches 85% ($\rho = 0.85$):
  $$\frac{0.85}{1 - 0.85} \approx \mathbf{5.67}$$
- If utilization reaches 92% ($\rho = 0.92$):
  $$\frac{0.92}{1 - 0.92} = \mathbf{11.5}$$
Wait time increases exponentially, not linearly. Incoming requests spend 5x to 10x longer waiting in thread pools and socket accept queues before execution even begins.
</details>

### Q2: What is Little's Law and how do you use it to size application thread pools?
<details>
<summary>View Solution</summary>

**Little's Law**: $L = \lambda \times W$
- $L$: Average number of concurrent requests in the system.
- $\lambda$: Arrival rate (requests per second).
- $W$: Average processing duration (seconds).

**Application**:
If a payment gateway receives 500 requests/sec ($\lambda = 500$), and each payment call to upstream credit card processors takes 200ms ($W = 0.2\text{s}$):
$$L = 500 \times 0.2 = \mathbf{100 \text{ concurrent requests}}$$
The application's worker thread pool (e.g. Puma, Gunicorn, Tomcat) must be configured with at least **100 worker threads** just to service current load without queueing. To accommodate a 2x traffic surge, size the pool for 200 threads.
</details>

### Q3: How do you calculate how much RAM a Linux server needs for filesystem caching?
<details>
<summary>View Solution</summary>

While databases manage their own internal buffers (e.g., PostgreSQL `shared_buffers`), the operating system's filesystem page cache handles index reads and cold table blocks.
- **Rule of Thumb**: Aim to hold the **Active Working Set** in memory.
- If a database has 1TB of total data, but 85% of queries touch data created in the last 30 days (amounting to 80GB):
- The active working set is 80GB.
- Providing **64GB to 96GB of page cache** guarantees that ~95% of active read queries are satisfied directly from RAM, maintaining single-digit millisecond query latencies.
</details>

### Q4: Why is CPU "average load" insufficient for capacity planning?
<details>
<summary>View Solution</summary>

1. Averages smooth out **microbursts**. A service might have an average CPU of 20% over 1 hour, but experience 5-second bursts of 100% saturation every time a batch job runs, dropping user requests during those 5 seconds.
2. In multi-core systems, average CPU masks **single-threaded bottlenecks**. On a 64-core server, one core can be 100% saturated (stalling a single-threaded Python/Node worker) while total CPU usage shows only 1.5%.
3. Capacity planning must evaluate **p95/p99 peaks** and **per-core distributions**.
</details>

### Q5: How do you calculate the network egress bandwidth requirements for an image delivery service?
<details>
<summary>View Solution</summary>

$$\text{Bandwidth (bps)} = \text{Requests per Second} \times \text{Average Payload Size (Bytes)} \times 8 \times \text{Protocol Overhead Factor}$$
*Example*:
- Request rate: 1,200 images/sec
- Average image size: 150 KB ($150 \times 1024 = 153,600$ bytes)
- Protocol overhead factor (TCP/IP/TLS headers): $\approx 1.05$
$$\text{Data Rate} = 1200 \times 153,600 \times 1.05 = 193,536,000 \text{ bytes/sec}$$
$$\text{Bandwidth} = \frac{193,536,000 \times 8}{1,000,000} \approx \mathbf{1,548 \text{ Mbps (1.55 Gbps)}}$$
To support this with 1.5x peak headroom, provision a network interface supporting at least **2.5 Gbps to 5 Gbps**.
</details>

### Q6: What is the Peak-to-Average Ratio (PAR) and why does it matter?
<details>
<summary>View Solution</summary>

$$\text{PAR} = \frac{\text{Peak Utilization}}{\text{Average Utilization}}$$
- In services with steady, uniform workloads (e.g. background data ingestion), PAR is close to $1.2$–$1.5$.
- In consumer-facing applications (e.g. food delivery or ticketing platforms), traffic spikes heavily during lunch hours, producing a PAR of $4.0$ to $8.0$.
- High PAR workloads require aggressive auto-scaling or elastic serverless capacity; provisioning for average load guarantees massive outages at peak.
</details>

### Q7: How do you determine whether a system is memory-bound vs CPU-bound?
<details>
<summary>View Solution</summary>

1. **CPU-Bound**:
   - `vmstat 1`: `r` (run queue) is consistently higher than CPU core count.
   - `%usr` + `%sys` is $> 80\%$.
   - Memory `available` is stable and `si`/`so` (swapping) is zero.
2. **Memory-Bound**:
   - Memory `available` is near zero.
   - `vmstat 1`: `si` and `so` show continuous non-zero activity (active swapping).
   - CPU shows elevated `%iowait` or `%sys` (page compaction / kswapd activity).
   - `/proc/pressure/memory` shows high `some` and `full` stall percentages.
</details>

### Q8: What is write amplification in storage capacity planning?
<details>
<summary>View Solution</summary>

Write amplification occurs when the physical data written to storage media is a multiple of the logical data written by the application:
1. **Filesystem / Database Log Amplification**: Writing a 100-byte row in PostgreSQL requires writing to the Write-Ahead Log (WAL), writing the data block, and updating index blocks.
2. **SSD Flash Block Re-writing**: SSDs can only overwrite data in large blocks (e.g., 4MB). Writing a 4KB block requires reading, erasing, and rewriting the entire 4MB erase block.
Capacity planners must budget storage IOPS and flash endurance to account for write amplification factors (typically 2x–5x).
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Computing Current System Concurrency via Little's Law
**Task**: Use `ss -tan state established` to count current active connections on your machine.
<details>
<summary>View Solution</summary>

```bash
CONCURRENT_CONNS=$(ss -tan state established | grep -v "Recv-Q" | wc -l)
echo "Current Active Concurrency (L): ${CONCURRENT_CONNS}"
```
</details>

### Exercise 2: Auditing System Core Headroom
**Task**: Extract current 15-minute load average and compare it to total CPU core count to calculate active CPU headroom percentage.
<details>
<summary>View Solution</summary>

```bash
CORES=$(nproc)
LOAD15=$(uptime | awk -F'load average:' '{print $2}' | awk -F',' '{print $3}' | tr -d ' ')

HEADROOM=$(awk "BEGIN {print (1 - (${LOAD15} / ${CORES})) * 100}")
echo "Physical Cores: ${CORES}"
echo "15-Min Load:    ${LOAD15}"
echo "Current Headroom: ${HEADROOM}%"
```
</details>

### Exercise 3: Historical Network Throughput Inspection
**Task**: Query `sar -n DEV` to inspect the highest 1-minute network ingress and egress rates recorded today.
<details>
<summary>View Solution</summary>

```bash
if command -v sar &>/dev/null && [ -f /var/log/sysstat/sa$(date +%d) ]; then
    echo "Peak Ingress (rxkB/s):"
    sar -n DEV | awk 'NR>3 {print $5}' | sort -nr | head -n 1
    echo "Peak Egress (txkB/s):"
    sar -n DEV | awk 'NR>3 {print $6}' | sort -nr | head -n 1
else
    echo "sysstat historical logs not currently enabled."
fi
```
</details>

### Exercise 4: Simulating Disk Growth Linear Regression
**Task**: Given 5 daily data points of disk consumption (in GB): Day 1=100, Day 2=110, Day 3=120, Day 4=130, Day 5=140. Calculate the daily growth rate and predict when a 200GB disk will fill.
<details>
<summary>View Solution</summary>

$$\text{Slope } m = \frac{140 - 100}{5 - 1} = \frac{40}{4} = \mathbf{10 \text{ GB / day}}$$
$$\text{Remaining Space} = 200\text{ GB} - 140\text{ GB} = 60\text{ GB}$$
$$\text{Days until Full} = \frac{60\text{ GB}}{10\text{ GB/day}} = \mathbf{6 \text{ days}}$$
The disk will reach 100% capacity on Day 11.
</details>

### Exercise 5: Inspecting Host RAM Distribution
**Task**: Break down host RAM into percentage used, percentage available, and percentage allocated to disk page cache.
<details>
<summary>View Solution</summary>

```bash
free -m | awk 'NR==2 {
  total=$2; used=$3; free=$4; buff_cache=$6; avail=$7;
  printf "Total RAM:       %d MB\n", total;
  printf "Available:       %.1f%%\n", (avail/total)*100;
  printf "Used:            %.1f%%\n", (used/total)*100;
  printf "Page Cache/Buff: %.1f%%\n", (buff_cache/total)*100;
}'
```
</details>

### Exercise 6: Calculating Peak-to-Average Ratio (PAR)
**Task**: Given an hourly traffic log where average throughput is 200 req/sec and peak throughput is 850 req/sec, compute the Peak-to-Average Ratio.
<details>
<summary>View Solution</summary>

$$\text{PAR} = \frac{850}{200} = \mathbf{4.25}$$
A PAR of 4.25 indicates significant workload burstiness. The architecture must either provision capacity for 850 req/sec (plus 30% headroom = ~1,100 req/sec) or utilize auto-scaling infrastructure.
</details>

---

## Summary Cheat Sheet & Further Reading

### Sizing Formulas
| Resource Metric | Sizing Equation |
| :--- | :--- |
| **Little's Law** | $L = \lambda \times W$ (Concurrency = Arrival Rate $\times$ Latency) |
| **Kingman's Wait Time**| $W_q \approx \left(\frac{\rho}{1-\rho}\right) \cdot \left(\frac{c_a^2+c_s^2}{2}\right) \cdot \tau$ |
| **Safe CPU Sizing** | $\text{Cores} = \frac{\text{Required CPU Seconds / sec}}{0.70}$ |
| **Memory Headroom** | $\text{RAM} = \text{OS (4GB)} + \text{Working Set} + \text{Cache (25\%–50\%)} + \text{Heap}$ |
| **Network Bandwidth** | $\text{bps} = \text{Requests/sec} \times \text{Avg Payload Bytes} \times 8 \times 1.5$ |
| **Storage Throughput**| $\text{MB/s} = \frac{\text{IOPS} \times \text{Block Size (KB)}}{1024}$ |
| **Linear Forecasting**| $y = mx + b$ (`predict_linear(v[range], time)`) |

### Further Reading
- [Brendan Gregg: Systems Performance: Enterprise and the Cloud](https://www.brendangregg.com/systems-performance-2nd-edition-book.html)
- [Little's Law - Operations Research & Queueing Models](https://en.wikipedia.org/wiki/Little%27s_law)
- [Kingman's Formula for Approximating Waiting Time in Queues](https://en.wikipedia.org/wiki/Kingman%27s_formula)
- `man 1 sar` and `man 5 sysstat`
- [Google SRE Book: Addressing Cascading Failures and Capacity Planning](https://sre.google/sre-book/addressing-cascading-failures/)
