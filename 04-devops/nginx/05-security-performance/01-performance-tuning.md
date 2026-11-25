---
title: "NGINX Performance Tuning & Linux Kernel Optimization"
description: "Comprehensive guide to high-concurrency performance engineering in NGINX, covering worker affinity, kernel sysctl tuning, socket buffers, sendfile, keepalives, and event loop optimization."
module: "05-security-performance"
topic: "01-performance-tuning"
status: "completed"
tags: ["nginx", "performance", "tuning", "sysctl", "kernel", "concurrency", "sendfile", "keepalive", "epoll"]
prerequisites: ["04-internals/01-architecture-master-worker-and-event-loop.md", "03-traffic-management/09-sendfile-and-large-file-delivery.md"]
estimated_read_time: "35 min"
---

# NGINX Performance Tuning & Linux Kernel Optimization

```
+-------------------------------------------------------------------------------+
| High-Throughput Request Pipeline & Tuning Vectors                             |
|                                                                               |
|  [Network Interface Card (NIC)]                                               |
|        │  Ring Buffers (ethtool -G rx 4096)                                   |
|        ▼                                                                      |
|  [Linux Kernel Network Stack]                                                 |
|        │  net.core.somaxconn = 65535, net.ipv4.tcp_max_syn_backlog = 65535    |
|        │  net.ipv4.tcp_tw_reuse = 1, net.ipv4.ip_local_port_range = 1024 65535|
|        ▼                                                                      |
|  [NGINX Worker Processes (CPU Affinity)]                                      |
|        │  worker_processes auto; worker_cpu_affinity auto;                    |
|        │  worker_rlimit_nofile 1048576; events { worker_connections 65535; }  |
|        ▼                                                                      |
|  [Socket & Buffer Mechanics]                                                  |
|        │  sendfile on; tcp_nopush on; tcp_nodelay on;                         |
|        │  open_file_cache max=50000 inactive=60s;                             |
|        ▼                                                                      |
|  [Persistent Upstream Keepalive Pool]                                         |
|        │  upstream { keepalive 256; } proxy_http_version 1.1;                 |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts & Tuning Hierarchy](#2-core-concepts--tuning-hierarchy)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Directives Reference](#4-syntax--directives-reference)
5. [Practical Implementation Examples](#5-practical-implementation-examples)
6. [Production Best Practices & Tuning Playbook](#6-production-best-practices--tuning-playbook)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance, Resource & Capacity Calculations](#8-performance-resource--capacity-calculations)
9. [Architectural Comparison: Default vs Tuned Edge Stack](#9-architectural-comparison-default-vs-tuned-edge-stack)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

NGINX is exceptionally fast out of the box, but default configurations and standard Linux kernel parameters are engineered for general-purpose workloads, not high-throughput, sub-millisecond edge routing:
- **Default Constraints**: An unconfigured Linux box caps the socket listen backlog at 128 (`somaxconn`) and open file descriptors per process at 1024, causing TCP SYN packet drops and `EMFILE` errors under even moderate traffic spikes (5,000 req/sec).
- **Socket Churn & Port Exhaustion**: A high-traffic reverse proxy opening a new TCP connection per request can exhaust all 64,000 ephemeral ports within seconds, entering `TIME_WAIT` lockups and throwing `502 Bad Gateway` errors.
- **Context Switches & CPU Cache Misses**: Without CPU affinity, operating system schedulers bounce NGINX worker processes across physical cores, invalidating L1/L2/L3 CPU caches and inflating latency percentiles ($p99$ and $p99.9$).

Performance tuning systematically eliminates bottlenecks across five layers: the Network Interface Card (NIC), Linux kernel TCP stack, NGINX event core, file and socket buffers, and upstream connection reuse.

---

## 2. Core Concepts & Tuning Hierarchy

Performance engineering must always proceed in priority order. Tuning buffers before addressing upstream connection reuse or caching will yield negligible returns:

| Priority | Layer | Mechanism | Expected Impact |
| :--- | :--- | :--- | :--- |
| **P1** | **Caching** | `proxy_cache`, microcaching, static cache | 90–99% reduction in upstream load |
| **P2** | **Connection Pooling** | `keepalive` in `upstream {}`, HTTP/1.1 reuse | 3× throughput gain, 0 port exhaustion |
| **P3** | **OS Kernel Sockets** | `somaxconn`, `tcp_max_syn_backlog`, `tcp_tw_reuse` | Eliminates connection drop during spikes |
| **P4** | **Concurrency Ceilings** | `worker_rlimit_nofile`, `worker_connections` | Scales concurrent sockets from 1,024 to 100,000+ |
| **P5** | **Zero-Copy & File Cache**| `sendfile`, `tcp_nopush`, `open_file_cache` | 40% reduction in CPU during static delivery |
| **P6** | **CPU Affinity** | `worker_cpu_affinity auto;` | 15–25% improvement in $p99$ latency |
| **P7** | **Buffer Geometry** | `proxy_buffers`, `client_body_buffer_size` | Prevents disk spills during proxying |

---

## 3. How It Works Internally

### TCP Connection Lifecycle and Kernel Buffers

```
  Client (SYN)
      │
      ▼
 [SYN Backlog Queue]  <--- Governed by /proc/sys/net/ipv4/tcp_max_syn_backlog
      │                    (Client ACK returns)
      ▼
 [Accept Queue]       <--- Governed by /proc/sys/net/core/somaxconn & listen backlog
      │
      ▼ epoll_wait() returns readable FD
 [NGINX Worker Process]
      │
      ├─ Reads HTTP request line into client_header_buffer_size
      ├─ Reads payload into client_body_buffer_size (spills to disk if exceeded)
      ├─ Connects to upstream (reuses persistent socket from upstream keepalive cache)
      └─ Streams upstream response through proxy_buffers to client
```

1. **SYN Queue (`tcp_max_syn_backlog`)**: Holds half-open connections awaiting the final client ACK. If full during a SYN flood or traffic spike, incoming connection attempts are dropped.
2. **Accept Queue (`somaxconn` & `listen backlog`)**: Holds completed three-way handshakes awaiting NGINX's `accept4()` call. If NGINX event loops are saturated, the accept queue fills, and new connections are dropped or reset.
3. **Upstream Keepalive Cache**: Maintains persistent, pre-established TCP connections to backends. When a client request finishes, the connection to the upstream is not closed with a FIN/ACK; instead, it is placed in an LRU pool in the worker's memory.

---

## 4. Syntax & Directives Reference

### Core Concurrency Directives
- `worker_processes auto;`: Automatically detects the number of available CPU cores and spawns one worker process per core.
- `worker_cpu_affinity auto;`: Automatically binds each worker process to a dedicated CPU core, preventing thread migration across sockets/NUMA nodes.
- `worker_rlimit_nofile <number>;`: Increases the maximum open file descriptor limit (`RLIMIT_NOFILE`) for worker processes, bypassing default OS user limits.
- `events { worker_connections <number>; }`: Defines the maximum number of simultaneous open connections per worker process (both client and upstream connections count).
- `multi_accept on;`: Directs workers to accept all newly arriving connections in a single `epoll` pass rather than accepting them one at a time.

### Network & File Delivery Directives
- `sendfile on;`: Enables zero-copy data transfer directly from the kernel page cache to the network socket descriptor without passing through user space.
- `tcp_nopush on;`: Only active when `sendfile` is enabled; forces NGINX to accumulate full TCP packets (MSS) before sending, optimizing network packet density.
- `tcp_nodelay on;`: Disables Nagle's algorithm on keepalive connections, ensuring immediate transmission of small payloads without artificial 40ms delays.
- `open_file_cache max=N inactive=time;`: Caches open file descriptors, file sizes, and modification timestamps in memory to avoid redundant `stat()` and `open()` system calls.

---

## 5. Practical Implementation Examples

### Example 1: High-Performance Linux Kernel Configuration (`/etc/sysctl.d/99-nginx.conf`)

Apply these parameters to prepare the host kernel for 100,000+ concurrent connections:

```ini
# /etc/sysctl.d/99-nginx.conf

# Maximum socket listen backlog queue size
net.core.somaxconn = 65535

# Maximum number of input network packets queued per interface
net.core.netdev_max_backlog = 65535

# Maximum half-open TCP connections in SYN queue
net.ipv4.tcp_max_syn_backlog = 65535

# Ephemeral outbound port range for upstream proxy connections
net.ipv4.ip_local_port_range = 1024 65535

# Safely reuse TIME_WAIT sockets for new outgoing connections
net.ipv4.tcp_tw_reuse = 1

# Reduce FIN_WAIT timeout from 60s to 15s to free closed sockets faster
net.ipv4.tcp_fin_timeout = 15

# TCP keepalive probe settings (detect dead peers faster)
net.ipv4.tcp_keepalive_time = 300
net.ipv4.tcp_keepalive_intvl = 15
net.ipv4.tcp_keepalive_probes = 5

# Maximum number of orphan TCP sockets
net.ipv4.tcp_max_orphans = 262144

# System-wide file descriptor limit
fs.file-max = 2097152

# Network memory buffer allocations (min default max in bytes)
net.ipv4.tcp_rmem = 4096 87380 16777216
net.ipv4.tcp_wmem = 4096 65536 16777216
```

Apply immediately without rebooting:
```bash
sudo sysctl -p /etc/sysctl.d/99-nginx.conf
```

### Example 2: Systemd Unit Override for File Descriptor Limits

Ensure systemd does not impose a 1024 FD limit on the NGINX service:

```ini
# /etc/systemd/system/nginx.service.d/limits.conf
[Service]
LimitNOFILE=1048576
LimitNPROC=512000
```

Reload systemd daemon:
```bash
sudo systemctl daemon-reload
sudo systemctl restart nginx
```

### Example 3: Full Production Tuned `nginx.conf`

```nginx
# /etc/nginx/nginx.conf
user www-data;
pid /run/nginx.pid;

# 1. Concurrency and Worker Topology
worker_processes auto;
worker_cpu_affinity auto;
worker_rlimit_nofile 1048576;

# Log only critical warnings in production to save disk I/O
error_log /var/log/nginx/error.log warn;

events {
    worker_connections 65535;
    use epoll;
    multi_accept on;
}

http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    # 2. Optimized Zero-Copy File I/O
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;

    # 3. Aggressive Open File Descriptor Cache
    open_file_cache max=100000 inactive=60s;
    open_file_cache_valid 120s;
    open_file_cache_min_uses 2;
    open_file_cache_errors on;

    # 4. Client Timeouts & Connection Caps
    keepalive_timeout 65s;
    keepalive_requests 10000;
    client_body_timeout 15s;
    client_header_timeout 15s;
    send_timeout 15s;
    reset_timedout_connection on;

    # 5. Buffer Geometry
    client_body_buffer_size 128k;
    client_max_body_size 20m;
    client_header_buffer_size 4k;
    large_client_header_buffers 4 16k;

    # 6. Upstream Keepalive Connection Pool
    upstream production_api {
        server 10.0.1.10:8080 max_fails=3 fail_timeout=10s;
        server 10.0.1.11:8080 max_fails=3 fail_timeout=10s;
        server 10.0.1.12:8080 max_fails=3 fail_timeout=10s;

        # Keep open 256 idle persistent connections per worker
        keepalive 256;
        keepalive_requests 10000;
        keepalive_timeout 60s;
    }

    server {
        listen 443 ssl reuseport;
        server_name api.example.com;

        ssl_certificate /etc/letsencrypt/live/api.example.com/fullchain.pem;
        ssl_certificate_key /etc/letsencrypt/live/api.example.com/privkey.pem;
        ssl_protocols TLSv1.2 TLSv1.3;
        ssl_session_cache shared:SSL:50m;
        ssl_session_timeout 1d;
        ssl_session_tickets off;

        location / {
            proxy_pass http://production_api;

            # Mandatory for upstream keepalive connection reuse!
            proxy_http_version 1.1;
            proxy_set_header Connection "";

            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header X-Forwarded-Proto $scheme;

            # Upstream Buffering Tuning
            proxy_buffering on;
            proxy_buffer_size 8k;
            proxy_buffers 16 32k;
            proxy_busy_buffers_size 64k;
            proxy_temp_file_write_size 64k;
        }
    }
}
```

### Example 4: Disk I/O Offloading with AIO Thread Pools
When serving static files exceeding RAM page cache size, prevent disk read blocks from freezing worker event loops:

```nginx
# /etc/nginx/nginx.conf (main context)
thread_pool default_pool threads=32 max_queue=65536;

http {
    server {
        listen 80;
        server_name media.example.com;
        root /var/www/media;

        location /videos/ {
            sendfile on;
            aio threads=default_pool;
            directio 8m;
            output_buffers 2 1m;
        }
    }
}
```

### Example 5: High-Precision Buffered Logging to Eliminate Disk Stalls
Writing every access log line synchronously to disk causes severe latency degradation under 50k req/sec. Buffer log entries in memory:

```nginx
http {
    log_format perf_combined '$remote_addr - $remote_user [$time_local] '
                             '"$request" $status $body_bytes_sent '
                             '"$http_referer" "$http_user_agent" '
                             'rt=$request_time uct="$upstream_connect_time" '
                             'uht="$upstream_header_time" urt="$upstream_response_time"';

    # Write log entries only when 1MB is accumulated or 5 seconds elapse
    access_log /var/log/nginx/access.log perf_combined buffer=1m flush=5s;
}
```

---

## 6. Production Best Practices & Tuning Playbook

- **Always Verify Upstream Bottlenecks First**: Before tuning NGINX workers, inspect `$upstream_response_time` in access logs. If backends take 250ms to respond, tuning NGINX buffers will have zero perceptible impact.
- **Set `keepalive_requests` Generously**: The old NGINX default of `keepalive_requests 100;` forces TLS connections to close and renegotiate every 100 requests. In modern production environments with client reuse and microservices, set `keepalive_requests 10000;`.
- **Use `reuseport` on Multicore Systems**: The `listen ... reuseport;` directive assigns an independent kernel receive queue to each worker process, eliminating the "thundering herd" wake-up penalty across CPU cores.
- **Enable `reset_timedout_connection on;`**: When a client connection times out, sends a TCP RST packet instead of undergoing standard FIN/ACK teardown, immediately reclaiming worker memory and socket buffers.
- **Tune NUMA Node Locality**: On multi-socket Xeon/EPYC servers, bind NGINX workers to CPU cores aligned with the PCIe lanes of the physical NIC using `numactl` or `worker_cpu_affinity`.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern / Mistake | Root Cause | Observable Production Impact | Correct Solution |
| :--- | :--- | :--- | :--- |
| Omitting `Connection ""` with Upstream Keepalive | HTTP/1.1 defaults to `Connection: close` without header override | Upstream keepalive cache fails; new TCP handshake per request | Specify `proxy_http_version 1.1;` and `proxy_set_header Connection "";` |
| Setting `worker_processes` to arbitrary large numbers (e.g. 64 on 4 cores) | Excessive context switching and CPU thrashing | Severe latency spikes and elevated system load | Use `worker_processes auto;` (1:1 core matching) |
| Disabling `proxy_buffering` globally | Forces synchronous end-to-end streaming between slow client and backend | Backend threads remain blocked for the duration of client transfer | Keep `proxy_buffering on;` unless running SSE or WebSockets |
| Setting `worker_connections` without `worker_rlimit_nofile` | Worker hits OS `RLIMIT_NOFILE` before connection cap is reached | `accept4() failed (24: Too many open files)` in error.log | Set `worker_rlimit_nofile` $\ge 2 \times \text{worker\_connections}$ |
| Forgetting `tcp_nopush` with `sendfile` | Packets sent without accumulating full MSS headers | Fragmented packet delivery; elevated network packet rate | Set `tcp_nopush on;` alongside `sendfile on;` |
| Synchronous Unbuffered Access Logging | Worker writes to disk on every single HTTP transaction | Disk I/O wait reaches 100%; workers stall on `write()` syscall | Add `buffer=512k flush=5s` to `access_log` |

---

## 8. Performance, Resource & Capacity Calculations

### Worker Connection & File Descriptor Sizing Formula
Every client connection proxied to a backend consumes at least **two** file descriptors (one client socket, one upstream socket). If reading files or writing cache metadata, additional FDs are consumed:

$$\text{Max Concurrent Sockets} = \text{worker\_processes} \times \frac{\text{worker\_connections}}{2}$$

$$\text{Required } \text{worker\_rlimit\_nofile} \ge \text{worker\_connections} \times 2 + 1024$$

*Example*: On an 8-core server configured with `worker_connections 32768;`:
- Max Theoretical Reverse-Proxy Clients: $8 \times (32768 / 2) = 131,072$ concurrent connections.
- Required `worker_rlimit_nofile`: At least $65,536 + 1024 = 66,560$ (set to $131,072$ or $1,048,576$ for headroom).

### Ephemeral Port Sizing Formula
Under high request volume without upstream keepalives:
$$\text{Max Proxied Req/Sec} = \frac{\text{Available Ephemeral Ports}}{\text{tcp\_fin\_timeout (seconds)}}$$
With default 28,000 ephemeral ports and 60-second `TIME_WAIT`:
$$\text{Max Throughput} = \frac{28,000}{60} \approx 466 \text{ requests/sec}$$
Beyond 466 req/sec, ephemeral ports exhaust completely! With upstream `keepalive 256;`, the same server comfortably handles 50,000+ req/sec using under 1,000 persistent sockets.

---

## 9. Architectural Comparison: Default vs Tuned Edge Stack

| Feature / Metric | Default Ubuntu NGINX | Tuned High-Performance NGINX |
| :--- | :--- | :--- |
| **Worker Connection Cap** | 768 or 1,024 | 65,535 per worker |
| **OS Listen Backlog (`somaxconn`)**| 128 | 65,535 |
| **Max Open Files (`nofile`)** | 1,024 | 1,048,576 |
| **Upstream TCP Handling** | Connection: close (fresh handshake per req) | HTTP/1.1 persistent keepalive pool |
| **File I/O Mechanism** | Synchronous user-space read/write | Zero-copy `sendfile` + AIO Thread Pools |
| **Access Logging** | Synchronous disk write per transaction | In-memory 1MB buffer with 5s flush |
| **CPU Scheduling** | Unbound kernel thread hopping | Hard `worker_cpu_affinity auto;` |
| **Throughput Ceiling (Proxying)**| ~3,500 req/sec | ~95,000 req/sec on equivalent hardware |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. Why is setting `proxy_http_version 1.1;` and `proxy_set_header Connection "";` mandatory for upstream keepalive?</strong></summary>

By default, NGINX uses HTTP/1.0 when proxying to upstreams, which does not support persistent connections. Furthermore, NGINX forwards incoming client headers; if a client sends `Connection: close`, NGINX passes that header to the upstream, causing the upstream to close the socket immediately after responding. By forcing HTTP/1.1 and explicitly clearing the `Connection` header (`Connection ""`), NGINX instructs the upstream to maintain the TCP connection open for reuse by the `keepalive` pool.
</details>

<details>
<summary><strong>2. What causes `1024 worker_connections are not enough` in the NGINX error log, and how do you resolve it?</strong></summary>

This error occurs when the number of active file descriptors managed by an NGINX worker reaches the configured `worker_connections` limit. To resolve:
1. Increase `worker_connections` in `events {}` (e.g., to 16384 or 65535).
2. Set `worker_rlimit_nofile` in the main configuration context to at least double `worker_connections`.
3. Raise OS-level limits in `/etc/security/limits.conf` and `systemd` unit overrides (`LimitNOFILE=1048576`).
</details>

<details>
<summary><strong>3. Explain the relationship between `sendfile`, `tcp_nopush`, and `tcp_nodelay`.</strong></summary>

- `sendfile on;` allows the kernel to copy file bytes directly from page cache to the network socket descriptor without context-switching into user space.
- `tcp_nopush on;` works only in tandem with `sendfile`. It tells the kernel to wait and pack HTTP headers and the beginning of the file into full TCP packets (MSS) before sending, preventing fragmented tiny packets.
- `tcp_nodelay on;` disables Nagle's algorithm on persistent connections, ensuring that once the file transfer completes, small ACK/finishing frames or subsequent interactive requests are transmitted immediately without artificial 40ms delays.
</details>

<details>
<summary><strong>4. How does `reuseport` solve the "thundering herd" problem in NGINX?</strong></summary>

Without `reuseport`, all worker processes listen on the exact same socket file descriptor. When a new connection arrives, the kernel wakes up all workers, but only one worker wins the race to accept the connection while the others waste CPU cycles returning to sleep. With `listen ... reuseport;`, the Linux kernel creates an independent listening socket queue for each worker process and distributes incoming TCP SYN packets using a 4-tuple hash across the queues in kernel space, eliminating cross-worker contention entirely.
</details>

<details>
<summary><strong>5. What is the difference between `client_body_buffer_size` and `client_max_body_size`?</strong></summary>

- `client_max_body_size` defines the absolute maximum payload size NGINX will accept before aborting the request with HTTP `413 Request Entity Too Large`.
- `client_body_buffer_size` defines the in-memory RAM buffer size for reading the request body. If the incoming payload exceeds `client_body_buffer_size` but is within `client_max_body_size`, NGINX writes the excess body bytes to temporary disk files (`client_body_temp_path`), degrading performance.
</details>

<details>
<summary><strong>6. When should you NOT use `sendfile`?</strong></summary>

`sendfile` should be avoided when serving files mounted over network filesystems (NFS, CIFS) or virtualized shared folders (VirtualBox shared folders), as the kernel page cache often fails to detect file modification or invalidation on remote hosts, resulting in stale or corrupted bytes being served to clients.
</details>

<details>
<summary><strong>7. How do you identify whether NGINX is CPU-bound, I/O-bound, or network-bound in production?</strong></summary>

1. Run `top` or `htop`: If NGINX worker processes show 100% CPU usage in user space (`%us`), NGINX is CPU-bound (likely TLS handshakes, gzip compression, or regex evaluation).
2. Check `%wa` (I/O wait): If I/O wait is high, workers are blocked waiting for slow disk storage (unbuffered logging, missing `open_file_cache`, or small proxy buffers spilling to disk).
3. Run `sar -n DEV 1` or `ethtool -S <eth>`: If packet drops, ring buffer overruns, or bandwidth saturation occurs, the system is network-bound.
</details>

<details>
<summary><strong>8. What is the impact of `reset_timedout_connection on;`?</strong></summary>

When a client connection times out (e.g. during a slow client attack or network dropout), standard TCP initiates a FIN handshake, leaving the socket in `FIN_WAIT` or `TIME_WAIT` states and consuming memory. `reset_timedout_connection on;` tells NGINX to instruct the kernel to immediately send a TCP RST packet upon timeout, closing the connection instantly and releasing memory buffers.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Audit Live System Limits and Backlog Drops</strong></summary>

**Goal**: Inspect active system backlog and socket metrics.
1. Check current `somaxconn`:
```bash
cat /proc/sys/net/core/somaxconn
```
2. Check for socket drops due to full accept queues:
```bash
netstat -s | grep -i "listen"
# Look for "times the listen queue of a socket overflowed"
```
3. Verify NGINX worker process file limits:
```bash
cat /proc/$(pgrep -f "nginx: worker" | head -n1)/limits | grep "open files"
```
</details>

<details>
<summary><strong>Exercise 2: Benchmark Upstream Keepalive Impact</strong></summary>

**Goal**: Measure throughput difference with and without upstream persistent connection reuse.
1. Configure an upstream block without `keepalive` and run `wrk -t4 -c100 -d10s http://localhost/test`.
2. Add `keepalive 128;`, `proxy_http_version 1.1;`, and `proxy_set_header Connection "";`.
3. Re-run `wrk` and compare requests per second and CPU usage.
</details>

<details>
<summary><strong>Exercise 3: Measure Memory Spill with `proxy_buffers`</strong></summary>

**Goal**: Observe NGINX writing upstream responses to disk when buffers are too small.
1. Set `proxy_buffers 2 4k;` and `proxy_buffer_size 4k;`.
2. Proxy a backend endpoint returning a 2MB JSON payload.
3. Inspect `/var/lib/nginx/proxy/` or error logs for `a client request body is being buffered to a temporary file` or proxy temp warnings.
</details>

<details>
<summary><strong>Exercise 4: Profile Worker CPU Pinning with `htop`</strong></summary>

**Goal**: Verify that workers remain pinned to specific CPU cores.
1. In `nginx.conf`, set:
```nginx
worker_processes 4;
worker_cpu_affinity 0001 0010 0100 1000;
```
2. Reload NGINX and launch high-concurrency traffic with `ab` or `wrk`.
3. Open `htop` (press `F2` -> Display options -> Show custom thread names) and observe balanced core consumption.
</details>

<details>
<summary><strong>Exercise 5: Verify Buffered Logging Performance</strong></summary>

**Goal**: Eliminate disk I/O bottlenecks during synthetic benchmarks.
1. Test default unbuffered logging with `wrk -t8 -c200 -d15s http://localhost/`.
2. Add `buffer=1m flush=5s` to the `access_log` directive.
3. Re-test and compare disk write I/O using `iostat -xz 1`.
</details>

<details>
<summary><strong>Exercise 6: Tune Kernel TCP Buffer Geometry</strong></summary>

**Goal**: Calculate and apply optimal `tcp_wmem` and `tcp_rmem` for a 10Gbps link with 20ms RTT.
1. Calculate Bandwidth-Delay Product (BDP):
$$\text{BDP} = 10 \times 10^9 \text{ bps} \times 0.020 \text{ s} = 200,000,000 \text{ bits} = 25 \text{ MB}$$
2. Verify maximum TCP window limits in `/proc/sys/net/ipv4/tcp_rmem` and `/proc/sys/net/ipv4/tcp_wmem`.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| NGINX Performance Tuning Directive Matrix                                     |
|                                                                                |
| Directive                     | Context | Recommended High-Load Setting        |
| :---------------------------- | :------ | :----------------------------------- |
| worker_processes              | main    | auto;                                |
| worker_cpu_affinity           | main    | auto;                                |
| worker_rlimit_nofile          | main    | 1048576;                             |
| worker_connections           | events  | 65535;                               |
| multi_accept                  | events  | on;                                  |
| sendfile                      | http    | on;                                  |
| tcp_nopush                    | http    | on;                                  |
| tcp_nodelay                   | http    | on;                                  |
| open_file_cache               | http    | max=100000 inactive=60s;             |
| keepalive                     | upstream| 256;                                 |
| proxy_http_version            | location| 1.1;                                 |
| proxy_set_header Connection   | location| "";                                  |
| access_log ... buffer=flush   | http    | buffer=1m flush=5s;                  |
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [NGINX Official Tuning Guide](https://www.nginx.com/blog/tuning-nginx/)
- [Linux Kernel Networking Documentation](https://www.kernel.org/doc/html/latest/networking/)
- [Brendan Gregg: Systems Performance & Epoll Analysis](http://www.brendangregg.com/)
- [Next Concept: Security Hardening & Zero-Trust Policies](02-security-hardening.md)

---

[← Back to Module Index](README.md) | [Next Note: 02-security-hardening.md →](02-security-hardening.md)
