---
title: "NGINX Benchmarking Methodologies, Latency Profiling & Flame Graphs"
description: "Comprehensive guide to rigorous load testing and performance profiling of NGINX, covering wrk/wrk2, k6, Coordinated Omission, latency percentiles (p99/p99.9), Linux perf, and on-CPU flame graphs."
module: "05-security-performance"
topic: "06-benchmarking-methodologies"
status: "completed"
tags: ["nginx", "benchmarking", "load-testing", "wrk", "wrk2", "k6", "coordinated-omission", "flame-graphs", "perf", "profiling"]
prerequisites: ["05-security-performance/01-performance-tuning.md", "04-internals/01-architecture-master-worker-and-event-loop.md"]
estimated_read_time: "35 min"
---

# NGINX Benchmarking Methodologies, Latency Profiling & Flame Graphs

```
+-------------------------------------------------------------------------------+
| Scientific Load Generation & Profiling Pipeline                               |
|                                                                               |
|  [Load Generator Host (Isolated Machine)]                                     |
|        │  wrk2 / k6 (Constant-Rate Open-Loop Injection: e.g. 50,000 req/sec)  |
|        │  Avoids Coordinated Omission; records true HDR latency percentiles  |
|        ▼ (10Gbps / 40Gbps Dedicated Network Link)                             |
|  [Target NGINX Server]                                                        |
|        │                                                                      |
|        ├── System Profiling: Linux `perf record -F 99 -a -g -- sleep 30`      |
|        ├── Kernel Tracing: `bcc-tools / bpftrace` (epoll_wait, sys_enter)     |
|        ├── Connection Metrics: `stub_status` + node_exporter                  |
|        ▼                                                                      |
|  [Downstream Upstream Cluster / Fast HTTP Mock Server]                        |
|        │                                                                      |
|        ▼                                                                      |
|  [Post-Test Analysis]                                                         |
|        ├─ Brendan Gregg On-CPU Flame Graph (`flamegraph.pl`)                  |
|        ├─ Latency Distribution Analysis (p50, p90, p99, p99.9, max)           |
|        └─ Saturation Knee Point Identification                                |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Closed-Loop vs Open-Loop Testing](#2-core-concepts-closed-loop-vs-open-loop-testing)
3. [The Coordinated Omission Problem](#3-the-coordinated-omission-problem)
4. [Tooling Comparison: wrk vs wrk2 vs k6 vs Vegeta](#4-tooling-comparison-wrk-vs-wrk2-vs-k6-vs-vegeta)
5. [Practical Implementation & Benchmark Scripts](#5-practical-implementation--benchmark-scripts)
6. [Low-Level Profiling with Linux `perf` & Flame Graphs](#6-low-level-profiling-with-linux-perf--flame-graphs)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Interpreting Latency Percentiles & Saturation Knees](#8-interpreting-latency-percentiles--saturation-knees)
9. [Architectural Comparison: Synthetic vs Production Workload Profiles](#9-architectural-comparison-synthetic-vs-production-workload-profiles)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Benchmarking NGINX is frequently conducted improperly. Engineering teams often run ApacheBench (`ab -n 10000 -c 100`) from the same machine hosting NGINX, look at an arbitrary "Requests per second" number, and declare success:
- **Flawed Tooling**: Tools like `ab` or standard `wrk` suffer from **Coordinated Omission**, measuring only the time after a request is admitted and completely ignoring the time requests spent blocked in queues during latency spikes.
- **Client Contention**: Running load generators on the same host or VM forces NGINX workers and test client threads to compete for the same CPU cores, L3 cache, and NIC buffers, invalidating results.
- **Unrealistic Traffic Patterns**: Testing a single 10-byte static file with a 100% cache hit rate does not represent production workloads involving TLS handshakes, upstream proxying, request body parsing, or variable query parameters.

Scientific benchmarking measures throughput at acceptable latency percentiles ($p99 < 10\text{ms}$), determines the exact saturation knee where queuing begins, and isolates CPU bottlenecks using low-level kernel profiling.

---

## 2. Core Concepts: Closed-Loop vs Open-Loop Testing

### Closed-Loop Load Testing (Flawed for Latency)
In a closed-loop system, client threads issue a request, wait synchronously for the response, and only issue the next request once the previous response has been received:
$$\text{Arrival Rate} = f(\text{System Service Time})$$
When the server experiences a 500ms garbage collection pause or lock contention, the load generator politely pauses with it. As a result, the measured arrival rate drops, the queue length appears small, and the latency numbers look unrealistically optimistic.

### Open-Loop Constant-Rate Testing (The Gold Standard)
In an open-loop model (implemented in `wrk2` and `vegeta`), the load generator injects requests at an independent, fixed frequency (e.g. exactly 20,000 requests per second) according to a predefined schedule:
$$\text{Arrival Rate} \perp \text{System Service Time}$$
If the server freezes or slows down, requests continue to pile up on the schedule. The tool records the difference between when the request was *scheduled* to be sent and when the response was *finally received*, accurately capturing real client latency.

---

## 3. The Coordinated Omission Problem

Coordinated Omission (defined by Gil Tene) occurs when a measurement tool coordinates with the system under test to omit bad latency samples from the recorded dataset:

```
  Scenario: Generator sends 100 req/sec (1 request every 10ms).
  Server operates normally for 1 second (100 requests complete in 1ms each).
  Then, the server stalls for 1,000ms (1 second freeze).

  What Actually Happens in Production:
  - 100 clients were scheduled to send requests during that 1 second stall.
  - Client 1 waits 1,000ms.
  - Client 2 waits 990ms.
  - Client 3 waits 980ms ...
  - Real p99 latency: ~1,000ms!

  What Closed-Loop Tools (ab, standard wrk) Report:
  - The single in-flight request on that worker waits 1,000ms.
  - No other requests are sent because the client thread is blocked waiting!
  - Tool records ONE bad sample (1,000ms) and 100 good samples (1ms).
  - Reported p99 latency: ~1ms! (A 1000x error!)
```

`wrk2` solves Coordinated Omission by maintaining an internal HDR (High Dynamic Range) histogram and accounting for expected arrival times.

---

## 4. Tooling Comparison: wrk vs wrk2 vs k6 vs Vegeta

| Tool | Architecture | Open-Loop / Coordinated Omission Corrected | Scriptability | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **wrk** | C + LuaJIT (epoll event loop) | ❌ No (Closed-loop) | High (Lua) | Maximum raw throughput saturation testing |
| **wrk2** | C + LuaJIT (constant rate HDR) |  Yes (Open-loop) | High (Lua) | Latency percentiles under constant load |
| **k6** | Go runtime + JS (V8) | ⚠️ Partial (via arrival-rate executors)| Very High (ES6 JS)| Complex multi-step user workflows & CI/CD |
| **Vegeta** | Go CLI / library |  Yes (Constant rate) | Low (CLI / JSON) | Precise rate-limited stress testing |
| **ab** | C (single-threaded synchronous)| ❌ No (Severe Coordinated Omission)| None | Quick sanity checks only; avoid for benchmarks |

---

## 5. Practical Implementation & Benchmark Scripts

### Example 1: Measuring High-Rate Throughput with `wrk2`

Run a 60-second test generating exactly 40,000 requests per second across 8 threads and 500 concurrent connections:

```bash
# Install wrk2
git clone https://github.com/giltene/wrk2.git
cd wrk2 && make

# Execute constant rate benchmark (-R 40000 = 40,000 req/sec)
./wrk -t8 -c500 -d60s -R40000 --latency http://10.0.1.10/api/v1/test
```

Example Output:
```text
  Latency Distribution (HdrHistogram - Recorded Latency)
 50.000%    1.24ms
 75.000%    1.89ms
 90.000%    2.45ms
 99.000%    5.12ms
 99.900%   12.80ms
 99.990%   28.40ms
100.000%   45.20ms
```

### Example 2: Realistic Traffic Generation with Lua Scripting (`wrk2`)

Generate synthetic requests with varying URLs, randomized query parameters, and custom authentication tokens:

```lua
-- /opt/benchmarks/workload.lua

-- Initialize random seed
math.randomseed(os.time())

local user_ids = {101, 204, 305, 408, 512, 629, 781, 892}
local paths = {
    "/api/v1/catalog",
    "/api/v1/items?id=",
    "/api/v1/search?q=",
    "/static/app.css"
}

request = function()
    local path_index = math.random(1, #paths)
    local selected_path = paths[path_index]

    if path_index == 2 then
        selected_path = selected_path .. user_ids[math.random(1, #user_ids)]
    elseif path_index == 3 then
        selected_path = selected_path .. "item_" .. math.random(1, 1000)
    end

    local headers = {}
    headers["Host"] = "api.example.com"
    headers["User-Agent"] = "BenchmarkWorker/2.0"
    headers["X-Trace-ID"] = string.format("%08x", math.random(0, 0xFFFFFFFF))

    return wrk.format("GET", selected_path, headers)
end

response = function(status, headers, body)
    if status >= 500 then
        io.write(string.format("Server Error: %d\n", status))
    end
end
```

Execute the Lua script with `wrk2`:
```bash
wrk -t8 -c200 -d30s -R25000 -s /opt/benchmarks/workload.lua --latency http://10.0.1.10/
```

### Example 3: Modern K6 Open-Loop Constant Arrival Rate Script

```javascript
// benchmark.js
import http from 'k6/http';
import { check } from 'k6';

export const options = {
  scenarios: {
    constant_rate_test: {
      executor: 'constant-arrival-rate',
      rate: 15000,              // 15,000 iterations per timeUnit
      timeUnit: '1s',           // 15,000 req/sec
      duration: '2m',
      preAllocatedVUs: 200,     // Pre-allocated virtual users
      maxVUs: 1000,             // Scale up to 1000 VUs if latency rises
    },
  },
  thresholds: {
    http_req_failed: ['rate<0.001'],         // Less than 0.1% failures
    http_req_duration: ['p(99)<10'],        // 99% of requests must complete under 10ms
  },
};

export default function () {
  const res = http.get('https://api.example.com/health', {
    headers: { 'Accept-Encoding': 'gzip' },
  });
  check(res, {
    'status is 200': (r) => r.status === 200,
  });
}
```

---

## 6. Low-Level Profiling with Linux `perf` & Flame Graphs

When NGINX workers reach 100% CPU utilization, how do you determine whether cycles are spent in TLS crypto, regular expression parsing, memory allocation, or kernel socket handling? Use Brendan Gregg's Flame Graphs:

```
+-------------------------------------------------------------------------------+
| Linux Perf Profiling & Flame Graph Workflow                                   |
|                                                                               |
|  1. Ensure Debug Symbols: compile NGINX with -g or install nginx-dbg          |
|  2. Sample CPU Call Stacks at 99Hz:                                           |
|     sudo perf record -F 99 -a -g -p $(pgrep -d',' -f "nginx: worker") -- sleep 30
|  3. Process Stack Traces:                                                     |
|     sudo perf script > /tmp/out.perf-stacks                                   |
|  4. Collapse & Generate SVG Flame Graph:                                      |
|     stackcollapse-perf.pl /tmp/out.perf-stacks | flamegraph.pl > nginx_cpu.svg|
+-------------------------------------------------------------------------------+
```

### Step-by-Step Commands:
```bash
# Clone FlameGraph toolset
git clone https://github.com/brendangregg/FlameGraph.git /opt/FlameGraph

# Sample worker processes across all physical CPU cores for 30 seconds
WORKER_PIDS=$(pgrep -d',' -f "nginx: worker")
sudo perf record -F 99 -g -p "$WORKER_PIDS" -- sleep 30

# Convert binary trace to collapsed stacks and generate interactive SVG
sudo perf script | /opt/FlameGraph/stackcollapse-perf.pl | /opt/FlameGraph/flamegraph.pl > /var/www/html/nginx_flamegraph.svg
```

### Analyzing the Flame Graph:
- **Wide Towers**: Represent functions consuming the largest percentage of CPU cycles on the system.
- **Top of the Tower**: Shows the specific function currently on the CPU (e.g. `ngx_http_regex_exec`, `AES_encrypt`, `ngx_palloc`).
- **Kernel vs User Space**: `[k]` indicates Linux kernel code (e.g. `tcp_v4_rcv`, `copy_user_generic_string`), while no tag indicates NGINX user-space C code.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern / Mistake | Root Cause | Observable Production Impact | Correct Solution |
| :--- | :--- | :--- | :--- |
| Running Load Generator on Target Machine | Generator and NGINX compete for CPU, RAM, and sockets | Results show 50% lower throughput than true system capacity | Run generator on dedicated, isolated machine over LAN |
| Relying on Average Latency ($p50$) | Outliers and tail latency spikes are completely masked | 95% of users are fine, but 5% experience 5-second timeouts | Measure $p90, p99, p99.9$, and maximum latency |
| Ignoring Coordinated Omission | Using closed-loop tools (`ab`) that pause during server stalls | Server stall of 10s reported as "1.2ms average latency" | Use open-loop constant arrival tools (`wrk2`, `vegeta`) |
| Saturating Client Ephemeral Ports | Client machine runs out of outbound source ports during benchmark | Benchmark aborts with `cannot assign requested address` | Tune `net.ipv4.ip_local_port_range` and enable `tcp_tw_reuse` |
| Over-Saturating Beyond the Knee Point | Pushing 100k req/sec to a system that saturates at 30k req/sec | Latencies explode to infinity; benchmark measures queue backlog | Test in increments (5k, 10k, 15k, 20k) to locate the knee |
| Testing with Unrealistic Payloads | Testing empty `200 OK` responses without headers or cookies | Disregards real-world parsing and memory buffer spills | Mirror production access log payloads and URI distributions |

---

## 8. Interpreting Latency Percentiles & Saturation Knees

### The Latency vs Throughput Saturation Curve

```
 Latency (ms)
      ▲
      │                                                / (Exponential Queue Delay)
  1000│                                               /
      │                                              /
   100│                                             /
      │                                            /
    10│                                           /
     1│──────────────────────────────────────────/  <-- "Knee Point" (Capacity Ceiling)
      └──────────────────────────────────────────────► Throughput (Req/Sec)
        10,000      20,000      30,000      40,000
```

1. **Linear Region (0 to 30,000 req/sec)**: Latency remains flat ($1.2\text{ms}$ to $1.8\text{ms}$). The system has ample CPU and event loop capacity.
2. **Knee Point (~32,000 req/sec)**: One critical resource (CPU, NIC interrupts, or upstream sockets) reaches 100% saturation.
3. **Queue Explosion (>32,000 req/sec)**: Little's Law takes over ($L = \lambda W$). Incoming requests exceed processing capacity and sit in OS kernel backlog queues. Latency scales exponentially even though throughput plateaus.

---

## 9. Architectural Comparison: Synthetic vs Production Workload Profiles

| Dimension | Naive Synthetic Test (`ab -n 100000`) | Realistic Production Profiling (`wrk2 + perf`) |
| :--- | :--- | :--- |
| **Connection Reuse** | Single persistent connection or no keepalive | Realistic keepalive pool with varying connection lifespans |
| **TLS Negotiation** | Plaintext HTTP over port 80 | TLS 1.3 with AES-GCM and session ticket resumption |
| **URI Distribution** | Single static URL (`/index.html`) | Weighted distribution of static assets, APIs, and search |
| **Measurement Accuracy**| Subject to severe Coordinated Omission | Corrected HDR histogram ($p50$ through $p99.99$) |
| **Hardware Topology**| Generator and server share 1 VM | Dual multi-core bare-metal hosts connected via 25Gbps NIC |
| **Root-Cause Telemetry**| Requests/sec summary only | Interactive Flame Graphs identifying exact bottlenecks |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. What is Coordinated Omission and why does it invalidate latency measurements in naive benchmark tools?</strong></summary>

Coordinated Omission occurs when a load testing tool's request scheduling is coupled with the target system's response rate (a closed-loop model). If a client thread sends a request and the server freezes for 2 seconds (e.g. during a lock contention or disk stall), the client thread blocks waiting for the response and does not issue any new requests during those 2 seconds. The tool records only a single high latency sample for that interval, omitting all the other virtual requests that *should* have been sent according to the target rate. In reality, real clients would have continued arriving and waiting in line, resulting in massive queue latency. Closed-loop tools report artificially low percentiles because they coordinate with the paused server to omit the stalled requests.
</details>

<details>
<summary><strong>2. How do you identify whether NGINX is CPU-bound or network-bound during a benchmark?</strong></summary>

1. Inspect `mpstat -P ALL 1` or `top`: If NGINX worker processes show near 100% user CPU (`%usr`) and softirq (`%si`) is low, NGINX is CPU-bound (typically TLS cryptography, gzip compression, or regex evaluation).
2. Inspect `sar -n DEV 1`: Check the `rxpck/s`, `txpck/s`, and `rxkB/s`, `txkB/s`. If the throughput reaches line rate (e.g. 940MB/s on a 10Gbps link) or NIC ring buffer drops (`rx_dropped`) accumulate in `ethtool -S <eth>`, the system is network-bound.
</details>

<details>
<summary><strong>3. What does Little's Law state and how does it apply to capacity planning in NGINX?</strong></summary>

Little's Law is a fundamental queueing theorem:
$$L = \lambda \times W$$
Where $L$ is the average number of concurrent requests in the system, $\lambda$ is the request arrival rate (requests per second), and $W$ is the average response time (latency in seconds). If an NGINX proxy receives 10,000 req/sec ($\lambda = 10,000$) and the backend takes 50ms to respond ($W = 0.05$), NGINX must maintain:
$$L = 10,000 \times 0.05 = 500 \text{ concurrent open upstream connections}$$
If backend latency doubles to 200ms due to database slowness, $L$ surges to 2,000 concurrent sockets. This dictates the sizing of `worker_connections` and upstream `keepalive` pools.
</details>

<details>
<summary><strong>4. How do you interpret an On-CPU Flame Graph generated from an NGINX worker process?</strong></summary>

- The X-axis represents the population of sampled call stacks, arranged alphabetically (not chronologically).
- The Y-axis shows the call stack depth, with the root function at the bottom and child functions stacked on top.
- The width of a box indicates the proportion of total CPU time spent in that function and its children.
- A wide plateau at the top of the flame graph represents a "leaf" function consuming substantial CPU time directly (e.g. `SHA256_Update` or `ngx_http_script_run`).
</details>

<details>
<summary><strong>5. Why is it important to benchmark TLS session resumption?</strong></summary>

A full TLS 1.3 handshake requires asymmetric cryptography (Diffie-Hellman key exchange) which consumes significant CPU cycles. Resumed sessions leverage TLS session caches or session tickets, requiring only symmetric cryptographic verification. Benchmarking without simulating session resumption overestimates CPU overhead by up to 400% compared to real-world browser traffic where over 80% of connections resume existing sessions.
</details>

<details>
<summary><strong>6. What is the difference between latency and service time in an open-loop benchmark?</strong></summary>

- **Service Time**: The time elapsed from when the server begins reading the first byte of the request to when it finishes writing the last byte of the response.
- **Latency (Response Time)**: The total time elapsed from when the request was *scheduled* or sent by the client to when the response was fully received. Latency includes both the service time and any **queueing delay** spent waiting in OS socket backlogs or network buffers.
</details>

<details>
<summary><strong>7. How does HTTP/2 multiplexing change load testing methodology compared to HTTP/1.1?</strong></summary>

In HTTP/1.1, concurrency is directly proportional to open TCP connections. In HTTP/2, hundreds of concurrent request/response streams multiplex over a single long-lived TCP socket. Benchmarking tools must simulate both a realistic number of active TCP connections *and* an authentic stream-concurrency ratio per connection to test NGINX's frame parsing, flow control windows, and HPACK header decoding algorithms.
</details>

<details>
<summary><strong>8. What operating system sysctl parameter often caps client-side load generator capacity?</strong></summary>

Load generators opening thousands of rapid connections frequently exhaust the client machine's ephemeral port range (`net.ipv4.ip_local_port_range`). Furthermore, sockets left in `TIME_WAIT` cannot be immediately reused unless `net.ipv4.tcp_tw_reuse = 1` is enabled on the generator host.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Determine the Saturation Knee Point Using `wrk2`</strong></summary>

**Goal**: Identify the exact request rate where latency transitions from linear to exponential.
1. Run `wrk2` at 5,000 req/sec increments:
```bash
for rate in 5000 10000 15000 20000 25000 30000 35000; do
    echo "=== Testing Rate: $rate req/sec ==="
    ./wrk -t4 -c100 -d20s -R$rate --latency http://target-host/
done
```
2. Plot the $p99$ latency against the rate in a spreadsheet to identify the knee point.
</details>

<details>
<summary><strong>Exercise 2: Generate an On-CPU Flame Graph of NGINX Workers</strong></summary>

**Goal**: Profile NGINX CPU execution during an active benchmark.
1. Start a high-load benchmark in terminal 1.
2. In terminal 2, record stack traces for 30 seconds:
```bash
sudo perf record -F 99 -g -p $(pgrep -f "nginx: worker" | head -n1) -- sleep 30
```
3. Generate the SVG flame graph and open it in a browser. Identify whether CPU is dominated by user-space parsing or kernel networking.
</details>

<details>
<summary><strong>Exercise 3: Simulate Coordinated Omission with a Mock Stall</strong></summary>

**Goal**: Experience how closed-loop tools mask latency spikes.
1. Configure an upstream mock server that artificially delays 1 request out of 500 by 3 seconds.
2. Run standard `wrk` and note the reported $p99$.
3. Run `wrk2` at a fixed rate and compare the $p99$ and $p99.9$ figures.
</details>

<details>
<summary><strong>Exercise 4: Benchmark TLS Handshake Overhead</strong></summary>

**Goal**: Quantify the CPU cost of TLS vs plaintext HTTP.
1. Run a benchmark against plaintext HTTP port 80: record requests/sec and worker CPU.
2. Run the exact same benchmark against HTTPS port 443 with TLS 1.3.
3. Calculate the percentage throughput reduction and identify the crypto routines in `perf report`.
</details>

<details>
<summary><strong>Exercise 5: Author a K6 Constant-Arrival-Rate Scenario</strong></summary>

**Goal**: Deploy an open-loop test script with automated latency assertion thresholds.
1. Implement the `benchmark.js` script from Example 3.
2. Execute: `k6 run benchmark.js`.
3. Verify that the test fails automatically if $p99$ exceeds 10ms.
</details>

<details>
<summary><strong>Exercise 6: Profile Kernel Socket Drops During Saturation</strong></summary>

**Goal**: Detect when OS queues drop packets during load testing.
1. Monitor TCP drop metrics before and after the test:
```bash
nstat -az TcpExtListenOverflows TcpExtListenDrops
```
2. Verify if drops correlate with latency spikes reported by the load generator.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| NGINX Benchmarking & Profiling Reference                                       |
|                                                                                |
| Tool / Command                | Purpose                                        |
| :---------------------------- | :--------------------------------------------- |
| wrk2 -R <rate> --latency      | Open-loop constant-rate latency HDR profiling  |
| k6 run --vus N                | Scriptable scenario-based user workflow tests  |
| perf record -F 99 -g -p <PID> | Samples CPU call stacks across NGINX workers   |
| stackcollapse-perf.pl         | Collapses perf script output for FlameGraph    |
| flamegraph.pl                 | Generates interactive SVG visualization        |
| mpstat -P ALL 1               | Monitors individual CPU core utilization       |
| nstat -az TcpExtListenDrops   | Audits kernel socket listen queue overflow drops|
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Gil Tene: How NOT to Measure Latency (Coordinated Omission Talk)](https://www.youtube.com/watch?v=lJ8ydIuPFeU)
- [Brendan Gregg: Linux Flame Graphs & Profiling](http://www.brendangregg.com/flamegraphs.html)
- [wrk2 Source Code & Architecture](https://github.com/giltene/wrk2)
- [Next Concept: Common Vulnerabilities & Misconfigurations in NGINX](07-common-vulnerabilities-in-configs.md)

---

[← Previous Note: 05-real-ip-and-trusted-proxies.md](05-real-ip-and-trusted-proxies.md) | [Back to Module Index](README.md) | [Next Note: 07-common-vulnerabilities-in-configs.md →](07-common-vulnerabilities-in-configs.md)
