# Network Load and Bandwidth Testing: Iperf3, MTR, and Latency Diagnostics

| Metadata | Details |
| :--- | :--- |
| **Topic** | Network Benchmarking, `iperf3`, `mtr`, Bufferbloat Analysis, HTTP Load Testing (`wrk`, `hey`) |
| **Module** | `04-networking` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `04-networking/01-networking-basics.md`, `04-networking/02-ip-routing-and-interfaces.md` |
| **Target Audience** | Performance Engineers, Network Engineers, SREs, Systems Architects |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools** | `iperf3`, `mtr`, `ping`, `wrk`, `hey`, `tc` (traffic control), `ethtool` |

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

In distributed architectures, application performance is constrained by the underlying physical and virtual network fabric. When an engineer reports:
- "The database query runs in 5ms locally but takes 120ms from the web tier."
- "Our AWS VPC peering connection is supposedly 10 Gbps, but file replication caps at 150 MB/s."
- "Under heavy load, API request latency spikes from 20ms to 4,000ms."

You must determine whether the bottleneck is **raw bandwidth capacity**, **intermediate routing packet loss**, **TCP window size limits**, **bufferbloat**, or **application concurrency limits**.

Network testing requires a disciplined, layered approach:
1. **Layer 3/4 Throughput & Jitter Testing (`iperf3`)**: Measures raw link capacity, packet loss, and jitter independently of application code or disk I/O.
2. **Hop-by-Hop Continuous Route Telemetry (`mtr`)**: Combines `ping` and `traceroute` into a real-time diagnostic engine to pinpoint exactly which intermediate router or ISP hop causes packet drops.
3. **Queueing Discipline & Bufferbloat Analysis**: Detects over-buffered intermediate switches that inflate latency under load.
4. **Layer 7 HTTP Concurrency Testing (`wrk`, `hey`)**: Benchmarks web services and reverse proxies under high concurrent connection loads.

---

## Core Concepts

### Bandwidth vs Latency vs Throughput
- **Bandwidth**: The physical or provisioned maximum data carrying capacity of a link (e.g., a 10 Gbps Ethernet link).
- **Latency (Round Trip Time - RTT)**: The time it takes for a data packet to travel from sender to destination and for the acknowledgment to return (measured in milliseconds via `ping` / `mtr`).
- **Throughput**: The actual rate of successfully delivered data achieved over the link.

### The Bandwidth-Delay Product (BDP)
The fundamental physical limitation of single-stream TCP throughput is governed by the Bandwidth-Delay Product:
$$\text{BDP (bits)} = \text{Bandwidth (bits/sec)} \times \text{Round-Trip Time (sec)}$$
$$\text{BDP (bytes)} = \frac{\text{Bandwidth} \times \text{RTT}}{8}$$

*Example*: On a 10 Gbps cross-region cloud link with 40ms RTT:
$$\text{BDP} = \frac{10 \times 10^9 \text{ bps} \times 0.040 \text{ s}}{8} = 50,000,000 \text{ bytes} \approx 50 \text{ MB}$$
If the Linux kernel's TCP receive window (`tcp_rmem`) is capped at the default 4MB, **a single TCP stream will never exceed $\approx 800\text{ Mbps}$**, regardless of having a 10 Gbps wire! Achieving full line-rate requires tuning TCP buffer windows or utilizing parallel TCP streams (`iperf3 -P`).

### Jitter and UDP Testing
- **Jitter**: The statistical variance in packet arrival latency. In VoIP, real-time gaming, and video streaming, jitter causes audible distortion and packet drops.
- Unlike TCP (which hides packet loss behind retransmissions and backoffs), **UDP testing with `iperf3 -u`** tests the raw network path at an exact prescribed bitrate (e.g., 500 Mbps), revealing true underlying packet drop rates and jitter metrics.

### Bufferbloat
Bufferbloat occurs when intermediate routers or network cards configure excessively large FIFO packet queues. 
- When the link saturates, packets sit in giant queues rather than dropping.
- TCP's loss-based congestion control algorithms (Cubic) fail to detect congestion until the queue overflows, causing round-trip latency to skyrocket from 10ms to several seconds.
- Modern Linux queueing disciplines (CoDel, `fq_codel`, CAKE) dynamically drop or ECN-mark packets to prevent queue buildup.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                    Iperf3 Client-Server Architecture                    |
+-----------------------------------------------------------------------------------------+
       Sender (iperf3 Client)                                Receiver (iperf3 Server)
          [ iperf3 -c server ]                                    [ iperf3 -s ]
                   |                                                    |
                   |--- Control Channel (TCP Port 5201) --------------->|
                   |<-- Parameter Negotiation (Test Duration, Buffers) -|
                   |                                                    |
                   |=== Test Stream 1 (High-Speed Data Loop) ==========>|
                   |=== Test Stream 2 (High-Speed Data Loop) ==========>|
                   |=== Test Stream N (Parallel -P Streams) ===========>|
                   |                                                    |
                   |<-- Measurement Reports (Bytes, Retransmits, RTT) --|
                   |                                                    |
                   |--- End Test & Display Aggregated Telemetry ------->|
```

### Hop-by-Hop Packet Tracing with `mtr`
`mtr` sends successive probe packets (ICMP Echo or UDP datagrams) with incrementally increasing IP Time-To-Live (TTL) values:
```
Packet 1: TTL=1 ---> Router 1 (TTL expires in transit -> returns ICMP Type 11 Time Exceeded)
Packet 2: TTL=2 ---> Router 1 ---> Router 2 (TTL expires -> returns ICMP Time Exceeded)
Packet 3: TTL=3 ---> Router 1 ---> Router 2 ---> Destination (returns ICMP Echo Reply)
```
`mtr` runs this continuously (default 1 probe per second), computing real-time statistical distributions (Loss%, Sent, Last, Avg, Best, Worst, StDev) for **every individual hop along the route**.

---

## Syntax & CLI Tools

### Benchmarking Raw Throughput with `iperf3`
```bash
# On Server (Receiver):
iperf3 -s -p 5201

# On Client (Sender):
# Standard 10-second TCP test
iperf3 -c 10.0.1.50

# Test using 4 parallel streams (-P) to saturate multi-gigabit links
iperf3 -c 10.0.1.50 -P 4

# Reverse test (-R): Server sends, Client receives
iperf3 -c 10.0.1.50 -R

# Run for 30 seconds (-t) logging every 1 second (-i)
iperf3 -c 10.0.1.50 -t 30 -i 1

# Test raw UDP bandwidth at 500 Mbps (-u -b)
iperf3 -c 10.0.1.50 -u -b 500M

# Output results in machine-readable JSON format (-J)
iperf3 -c 10.0.1.50 -J
```

### Continuous Route and Latency Analysis with `mtr`
```bash
# Interactive real-time ncurses GUI
mtr 8.8.8.8

# Run in raw numeric mode without reverse DNS delays (-n)
mtr -n 8.8.8.8

# Non-interactive report mode (-r): send 50 packets (-c) and print summary table
mtr -n -r -c 50 1.1.1.1

# Probe using TCP SYN packets on port 443 instead of ICMP (--tcp -P)
# (Bypasses ICMP rate-limiting on enterprise firewalls)
sudo mtr --tcp -P 443 -n -r -c 30 api.github.com
```

### Layer 7 HTTP Concurrency Testing with `wrk` and `hey`
```bash
# wrk: High-performance multi-threaded HTTP benchmark
# 12 threads (-t), 400 concurrent connections (-c), 30 second duration (-d)
wrk -t12 -c400 -d30s --latency https://api.mycompany.com/health

# hey: Modern HTTP load generator with histogram output
# 200 concurrent workers (-c), 5000 total requests (-n)
hey -n 5000 -c 200 -m GET https://api.mycompany.com/health
```

---

## Progressive Practical Examples

### Example 1: Measuring True Maximum Link Bandwidth with `iperf3`
Benchmark a private network link between two AWS EC2 instances or bare-metal servers.

```bash
# 1. On Destination Node (10.0.2.100):
iperf3 -s &

# 2. On Source Node:
# Single-stream TCP benchmark
iperf3 -c 10.0.2.100 -t 10
# Sample output:
# [ ID] Interval           Transfer     Bitrate         Retr
# [  5]   0.00-10.00  sec  1.10 GBytes   944 Mbits/sec    0    sender
# [  5]   0.00-10.00  sec  1.09 GBytes   940 Mbits/sec         receiver

# 3. Parallel multi-stream benchmark (saturating multi-queue NICs)
iperf3 -c 10.0.2.100 -P 8 -t 10
# Bitrate reaches 9.42 Gbits/sec across 8 streams!
```

### Example 2: Isolating Network Jitter and Packet Loss with UDP
TCP masks packet loss behind retransmission pauses. A UDP test reveals raw link reliability.

```bash
# Transmit constant 200 Mbps UDP stream for 15 seconds
iperf3 -c 10.0.2.100 -u -b 200M -t 15
```
**Sample Output**:
```text
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Drop%
[  5]   0.00-15.00  sec   357 MBytes   200 Mbits/sec  0.042 ms  0/45780 (0%)
```
- `Jitter 0.042 ms`: Exceptionally stable latency.
- `Lost/Total 0/45780 (0%)`: Zero packet drops on the link.

### Example 3: Pinpointing Packet Drops with `mtr -r` (Report Mode)
Troubleshoot an intermittent latency problem across intermediate internet transit providers.

```bash
mtr -n -r -c 100 93.184.216.34
```
**Sample Output Analysis**:
```text
HOST: srv-mon-01                  Loss%   Snt   Last   Avg  Best  Wrst StDev
  1.|-- 10.0.0.1                   0.0%   100    0.3   0.3   0.2   0.8   0.1
  2.|-- 192.168.100.1              0.0%   100    1.1   1.2   0.9   3.2   0.3
  3.|-- 203.0.113.45              30.0%   100   14.2  15.1  13.8  45.0   4.2  <-- Router ICMP Rate-Limit?
  4.|-- 198.51.100.12              0.0%   100   14.5  14.6  14.1  18.0   0.8  <-- Loss disappeared!
  5.|-- 93.184.216.34              0.0%   100   14.4  14.5  14.0  16.2   0.4
```
**Critical Diagnostic Insight**: 
- Notice Hop 3 shows `30.0% Loss`, but Hop 4 and the final destination (Hop 5) show **`0.0% Loss`**.
- This proves Hop 3 is **NOT** dropping actual forwarded network traffic. Hop 3's control plane CPU simply rate-limits generating ICMP TTL Exceeded replies. Real customer traffic traverses Hop 3 without any packet loss.
- *True packet loss persists all the way down to the final hop*.

### Example 4: Identifying Bufferbloat Under Saturated Upload
Demonstrate how saturation of a link inflates round-trip ping latency.

```bash
#!/usr/bin/env bash
set -euo pipefail

DESTINATION="8.8.8.8"
IPERF_TARGET="speedtest.server.net"

echo "==> 1. Baseline Idle Latency:"
ping -c 5 "${DESTINATION}" | tail -n 2

echo "==> 2. Starting saturating upload stream in background..."
iperf3 -c "${IPERF_TARGET}" -t 20 -b 0 > /dev/null 2>&1 &
IPERF_PID=$!
sleep 2

echo "==> 3. Measuring Latency Under Saturated Load:"
ping -c 10 "${DESTINATION}" | grep "time="

wait "${IPERF_PID}" 2>/dev/null || true
echo "==> Test finished."
```
If latency jumps from 15ms to 400ms during step 3, **bufferbloat** is present on the path.

### Example 5: High-Concurrency HTTP Performance Benchmarking with `wrk`
Evaluate how an Nginx reverse proxy handles 500 concurrent connections.

```bash
# Run 4 threads, 500 connections for 30 seconds, printing latency percentile distribution
wrk -t4 -c500 -d30s --latency http://127.0.0.1:80/
```
**Sample Output**:
```text
Running 30s test @ http://127.0.0.1:80/
  4 threads and 500 connections
  Thread Stats   Avg      StDev     Max   +/- StDev
    Latency     8.42ms    4.12ms  85.20ms   82.14%
    Req/Sec     14.82k    1.20k   18.90k    72.50%
  Latency Distribution
     50%    7.80ms
     75%   10.20ms
     90%   13.40ms
     99%   24.80ms
  1772184 requests in 30.05s, 1.40GB read
Requests/sec:  58972.10
Transfer/sec:     47.68MB
```
- Demonstrates throughput: ~59,000 requests per second.
- 99th percentile (p99) latency is 24.8ms.

---

## Best Practices

1. **Test with Both Single-Stream and Parallel Streams in `iperf3`**: Always run both `iperf3 -c <ip>` (single stream) and `iperf3 -c <ip> -P 8` (parallel streams). If parallel streams achieve 10 Gbps but a single stream caps at 1 Gbps, the bottleneck is **TCP window size / BDP limits**, not a physical link constraint.
2. **Use TCP Mode in `mtr` When ICMP is Blocked**: Cloud firewalls and corporate enterprise routers frequently block or aggressively deprioritize ICMP ping packets. Run `mtr --tcp -P 443 -n <target>` to send realistic TCP SYN probes on HTTPS port 443.
3. **Never Evaluate Packet Loss at Intermediate Hops in Isolation**: If an intermediate router shows 40% loss but subsequent hops and the destination show 0% loss, ignore the intermediate loss. It is simply ICMP generation throttling on the router's management CPU.
4. **Deploy Fair Queueing (`fq_codel` or `cake`) to Eliminate Bufferbloat**: On Linux routers and egress interfaces, configure modern active queue management via `tc`:
   ```bash
   sudo tc qdisc replace dev eth0 root fq_codel
   ```
5. **Differentiate Client vs Server Bottlenecks in HTTP Load Tests**: When running `wrk` or `hey`, monitor CPU and network socket states on the load-generator machine itself. If the client machine maxes out its CPU, the test reports false high latencies that reflect client saturation rather than server degradation.
6. **Benchmark During Maintenance Windows**: Generating multi-gigabit saturating loads on production networks can impact co-hosted VMs and shared switch fabrics. Coordinate tests in staging or off-peak windows.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Assuming intermediate hop loss in `mtr` indicates an active network outage | Router management processors drop ICMP TTL Exceeded packets to protect their CPUs while fast-forwarding transit frames. | Only consider packet loss real if it continues through to the final destination. |
| Testing 10 Gbps links with `iperf3` without `-P` | A single TCP connection is restricted by single-core CPU interrupts and TCP window limits. | Use `iperf3 -P 4` or `-P 8` to distribute streams across multiple CPU cores and RX queues. |
| Running `iperf3 -u` without specifying bandwidth (`-b`) | By default, `iperf3` limits UDP bandwidth to an arbitrary **1 Mbit/sec**! | Always specify bandwidth: `iperf3 -u -b 1G` or `-b 0` (unlimited). |
| Forgetting to open firewall port 5201 on the `iperf3` server | Client hangs waiting for connection; reports `Connection refused` or `Connection timed out`. | Open port 5201: `sudo ufw allow 5201/tcp` or `nft add rule ...`. |
| Running load generator (`wrk`) on the same host as the web server | Client and server compete for the same CPU cores and RAM, distorting benchmark measurements. | Always run load generators on an independent external machine. |
| Relying solely on `ping` average latency | Averages conceal latency spikes (outliers). | Inspect p99 percentiles and standard deviation (`StDev`). |

---

## Performance, Security & Cost Implications

### Performance
- **TCP Congestion Control Algorithms (CUBIC vs BBR)**: Standard Linux uses CUBIC (loss-based). On links with mild packet loss (e.g. Wi-Fi or trans-oceanic transit), CUBIC cuts throughput in half. Google's **BBR (Bottleneck Bandwidth and RTT)** ignores packet loss and maximizes throughput based on physical link modeling:
  ```bash
  sudo sysctl -w net.core.default_qdisc=fq
  sudo sysctl -w net.ipv4.tcp_congestion_control=bbr
  ```
  Switching to BBR can increase cross-region replication speed by **2x–10x**.

### Security
- **Denial of Service (DoS) Risk with Public `iperf3` Servers**: Leaving an open `iperf3 -s` listener on a public IP permits arbitrary external actors to flood your interface and saturate upstream bandwidth. Protect `iperf3` listeners with firewall access lists (`nftables`) restricting access to specific testing IPs.

### Cost
- Generating continuous multi-terabyte bandwidth tests across cloud regions or out to the public internet incurs significant **cloud egress bandwidth fees** (e.g. AWS $0.09/GB). A 10 Gbps test running at full capacity for 1 hour transfers $\approx 4.5\text{ TB}$, generating hundreds of dollars in bandwidth charges.

---

## Comparison Table / Alternatives

| Tool | Testing Layer | Primary Metric | Strengths | Limitations |
| :--- | :--- | :--- | :--- | :--- |
| **`iperf3`** | Layer 4 (TCP/UDP/SCTP) | Throughput, Jitter, Loss | Standard tool, microsecond precision, parallel streams | Requires dedicated server daemon running on remote peer |
| **`mtr`** | Layer 3 (ICMP / UDP / TCP) | Latency, Route Path, Jitter | Hop-by-hop visibility, real-time stats, identifies bad routers | Does not measure throughput capacity |
| **`ping`** | Layer 3 (ICMP) | Latency, RTT, Packet Loss | Ubiquitous across all operating systems | Single-hop end-to-end only; easily blocked |
| **`wrk`** | Layer 7 (HTTP/1.1) | Requests/sec, Latency Percentiles | Ultra-high performance (C/epoll), p99 latency stats | No HTTP/2 support; basic scripting (Lua) |
| **`hey`** | Layer 7 (HTTP/1.1 & HTTP/2) | Requests/sec, Histograms | Easy CLI flags, native Go binary, clean histograms | Lower max throughput than wrk |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: A 10 Gbps cloud network link achieves 9.8 Gbps when tested with `iperf3 -P 8`, but SCP file transfer caps at 120 MB/s (~1 Gbps). Why?
<details>
<summary>View Solution</summary>

**Root Causes**:
1. **Single-Stream TCP Limit**: `scp` uses a single TCP connection. If RTT is non-trivial, single-stream throughput is capped by the Bandwidth-Delay Product (BDP) and the socket buffer size (`tcp_rmem`).
2. **Encryption Overhead**: `scp` encrypts data over SSH in a single CPU thread. The client or server CPU core becomes pinned at 100% processing AES cryptographic calculations.
3. **Disk I/O Bottleneck**: `iperf3` tests in-memory network transfer. `scp` reads and writes to physical disk; if the destination disk is slow (e.g., standard HDD or throttled cloud EBS volume), throughput caps at disk write speed.
</details>

### Q2: How do you interpret an `mtr` report where the destination shows 0% packet loss, but a hop in the middle shows 25% packet loss?
<details>
<summary>View Solution</summary>

**There is NO actual packet loss on the network.**
Intermediate routers prioritize fast-path switching of data packets in hardware (ASICs). Generating ICMP "Time Exceeded" replies requires sending packets to the router's slow-path general-purpose control CPU. Network operators configure aggressive rate-limiting on ICMP generation to protect the router CPU from DoS attacks.
Because subsequent hops and the final destination show 0% loss, the packets traversed the middle router without being dropped.
</details>

### Q3: What is Bufferbloat and how do you diagnose it using standard tools?
<details>
<summary>View Solution</summary>

Bufferbloat is the undesirable latency introduced when network equipment buffers too many packets instead of dropping them when the link is saturated.

**Diagnosis**:
1. Ping a reliable external IP (e.g., `8.8.8.8`) while the link is completely idle. Note the baseline RTT (e.g. 15ms).
2. Start a saturating upload or download stream using `iperf3` or large file transfer.
3. Observe the ping RTT during transfer. If RTT climbs to hundreds or thousands of milliseconds (e.g., 800ms) while the transfer is active, bufferbloat is present in the bottleneck buffer.
</details>

### Q4: What is the difference between TCP CUBIC and TCP BBR?
<details>
<summary>View Solution</summary>

- **CUBIC**: A **loss-based** congestion control algorithm. It increases the congestion window until packet loss occurs, then cuts the window size sharply. On networks with non-congestion loss (e.g. wireless packet drops), CUBIC drastically reduces throughput unnecessarily.
- **BBR (Bottleneck Bandwidth and RTT)**: A **model-based** congestion control algorithm created by Google. It continuously measures the actual delivery rate and minimum round-trip time to maintain maximum throughput without filling intermediate queues, drastically reducing latency and bufferbloat.
</details>

### Q5: Why is UDP testing with `iperf3 -u` essential for assessing VoIP and video quality?
<details>
<summary>View Solution</summary>

VoIP and real-time audio/video rely on RTP over UDP. They cannot tolerate the retransmission delays of TCP; dropped frames must be discarded, and arrival timing must be predictable.
`iperf3 -u` allows you to test the link at the exact audio/video codec bitrate (e.g., `-b 50M`), directly measuring:
1. **Packet Drop Rate (%)**: How many frames are lost.
2. **Jitter (ms)**: The variance in packet arrival intervals. High jitter causes audio stutter and buffering.
</details>

### Q6: How does the `-w` (TCP Window Size) flag in `iperf3` impact throughput?
<details>
<summary>View Solution</summary>

The `-w` flag sets the socket buffer size (`SO_RCVBUF` / `SO_SNDBUF`).
By default, modern Linux kernels use auto-tuning (`net.ipv4.tcp_rmem`). If you manually specify `-w 64K` on a high-latency link (e.g., 50ms RTT), the maximum data that can be in flight is 64 KB, capping throughput to:
$$\text{Max Throughput} = \frac{64 \times 1024 \times 8 \text{ bits}}{0.050 \text{ s}} \approx 10.48 \text{ Mbps}$$
Allowing Linux auto-tuning or specifying a larger buffer window removes this constraint.
</details>

### Q7: Why do HTTP load testing tools like `wrk` report p99 latency instead of just average latency?
<details>
<summary>View Solution</summary>

Average latency hides outliers and tail latency. 
In a web application where an average request takes 10ms, but 1 out of 100 requests takes 3,000ms due to database locking or GC pauses, the arithmetic mean might show an acceptable ~40ms. 
However, in modern microservice architectures where a single user page load triggers 50 downstream service calls, a high p99 latency guarantees that a large percentage of end users experience severe page lag.
</details>

### Q8: What is the purpose of `ethtool -S` when troubleshooting network interface throughput?
<details>
<summary>View Solution</summary>

`ethtool -S <interface>` queries the physical or virtual NIC driver for low-level hardware statistics.
It reveals:
- `rx_dropped`, `tx_dropped`: Driver buffer exhaustion.
- `rx_crc_errors`: Bad physical cables or transceiver optical signal degradation.
- `rx_missed_errors`: Hardware FIFO overrun due to CPU interrupt saturation.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Local Loopback Bandwidth Saturation Test with `iperf3`
**Task**: Start an `iperf3` server on localhost. Benchmark memory-to-memory throughput over the loopback interface (`lo`).
<details>
<summary>View Solution</summary>

```bash
# 1. Start iperf3 server in background
iperf3 -s -p 5555 &
SERVER_PID=$!
sleep 1

# 2. Run benchmark over loopback
iperf3 -c 127.0.0.1 -p 5555 -t 5

# Notice bitrates can exceed 30–50 Gbits/sec (pure CPU memory copy)!

# 3. Clean up
kill "${SERVER_PID}" 2>/dev/null || true
```
</details>

### Exercise 2: Tracing Internet Route Quality with `mtr`
**Task**: Run a 30-packet non-interactive `mtr` report to `1.1.1.1`. Analyze average latency and packet loss across all hops.
<details>
<summary>View Solution</summary>

```bash
mtr -n -r -c 30 1.1.1.1
```
</details>

### Exercise 3: Simulating and Measuring UDP Packet Loss
**Task**: Start an `iperf3` server. Transmit UDP traffic at a rate intentionally exceeding link bandwidth to observe packet loss statistics.
<details>
<summary>View Solution</summary>

```bash
# 1. Start server
iperf3 -s -p 5556 &
SERVER_PID=$!
sleep 1

# 2. Transmit UDP at 1 Gbps over loopback
iperf3 -c 127.0.0.1 -p 5556 -u -b 1000M -t 5

# 3. Clean up
kill "${SERVER_PID}" 2>/dev/null || true
```
</details>

### Exercise 4: Benchmarking a Web Server with `hey` or `wrk`
**Task**: Launch a Python HTTP server or local Nginx instance. Use `hey` (or `wrk`) to benchmark 1,000 requests across 50 concurrent connections.
<details>
<summary>View Solution</summary>

```bash
# 1. Start mock server
python3 -m http.server 8080 &
HTTP_PID=$!
sleep 1

# 2. Run benchmark with hey (or curl loop if hey is not installed)
if command -v hey &>/dev/null; then
    hey -n 1000 -c 50 http://127.0.0.1:8080/
elif command -v wrk &>/dev/null; then
    wrk -t2 -c50 -d5s http://127.0.0.1:8080/
else
    echo "Benchmarking with curl loop:"
    time for i in {1..50}; do curl -s http://127.0.0.1:8080/ > /dev/null; done
fi

# 3. Clean up
kill "${HTTP_PID}" 2>/dev/null || true
```
</details>

### Exercise 5: Inspecting NIC Hardware Errors with `ethtool`
**Task**: Inspect the hardware drop counters and queue statistics of your primary network interface.
<details>
<summary>View Solution</summary>

```bash
IFACE=$(ip route show default | awk '{print $5}')
echo "Inspecting interface: ${IFACE}"
sudo ethtool -S "${IFACE}" 2>/dev/null | grep -E "drop|error|miss" | head -n 20 || echo "Hardware stats not supported on virtual interface"
```
</details>

### Exercise 6: Enabling TCP BBR Congestion Control
**Task**: Check your active congestion control algorithm. If BBR is available in your kernel, enable it and verify.
<details>
<summary>View Solution</summary>

```bash
# 1. Check current algorithm
sysctl net.ipv4.tcp_congestion_control

# 2. Check available algorithms
sysctl net.ipv4.tcp_available_congestion_control

# 3. Enable BBR if supported by kernel:
if grep -q "bbr" /proc/sys/net/ipv4/tcp_available_congestion_control; then
    sudo sysctl -w net.core.default_qdisc=fq
    sudo sysctl -w net.ipv4.tcp_congestion_control=bbr
    echo "BBR enabled successfully!"
else
    echo "BBR module not loaded. Load with: sudo modprobe tcp_bbr"
fi
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **Start Iperf3 Server** | `iperf3 -s` |
| **TCP Throughput Benchmark** | `iperf3 -c <server_ip> -t 10` |
| **Parallel Stream Test** | `iperf3 -c <server_ip> -P 8` |
| **Reverse Mode (Download)** | `iperf3 -c <server_ip> -R` |
| **UDP Bandwidth Test** | `iperf3 -c <server_ip> -u -b 500M` |
| **Continuous Route Report** | `mtr -n -r -c 50 <target_ip>` |
| **TCP SYN Route Trace** | `sudo mtr --tcp -P 443 -n -r <target>` |
| **HTTP Benchmark (`wrk`)** | `wrk -t4 -c200 -d30s --latency <url>` |
| **HTTP Benchmark (`hey`)** | `hey -n 2000 -c 100 <url>` |
| **Active Queue Discipline** | `tc qdisc show dev <iface>` |

### Further Reading
- `man 1 iperf3` and [ESnet iperf3 Documentation](https://software.es.net/iperf/)
- `man 8 mtr` and [BitWizard MTR Manual](https://www.bitwizard.nl/mtr/)
- [Bufferbloat.net: Understanding and Fixing Bufferbloat](https://www.bufferbloat.net/)
- [RFC 8257: Data Center TCP (DCTCP)](https://datatracker.ietf.org/doc/html/rfc8257)
- [BBR: Congestion-Based Congestion Control - ACM Queue](https://queue.acm.org/detail.cfm?id=3022184)
