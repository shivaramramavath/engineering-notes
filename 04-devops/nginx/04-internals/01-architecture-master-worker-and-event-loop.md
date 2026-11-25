---
title: "NGINX Process Architecture and the Event Loop"
description: "Deep dive into NGINX internals: Master-Worker process model, epoll non-blocking event loop, CPU affinity, SO_REUSEPORT, and inter-process communication."
module: "04-internals"
track: "devops-nginx"
order: 1
difficulty: "Advanced"
prerequisites:
  - "01-fundamentals/02-config-structure"
  - "01-fundamentals/06-command-line-and-signals"
tags:
  - nginx
  - architecture
  - master-worker
  - epoll
  - event-loop
  - cpu-affinity
  - reuseport
last_updated: "2026-03-30"
---

# NGINX Process Architecture and the Event Loop

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Production Multi-Core Worker Tuning with CPU Affinity](#example-1-production-multi-core-worker-tuning-with-cpu-affinity)
  - [Example 2: High-Concurrency Event Loop Tuning (65,536 Connections)](#example-2-high-concurrency-event-loop-tuning-65536-connections)
  - [Example 3: Kernel Socket Sharding with `SO_REUSEPORT`](#example-3-kernel-socket-sharding-with-so_reuseport)
  - [Example 4: Inspecting Worker epoll Loop and CPU Utilization via OS Tools](#example-4-inspecting-worker-epoll-loop-and-cpu-utilization-via-os-tools)
  - [Example 5: Dynamic Process Tree Tracing and Signal Handling](#example-5-dynamic-process-tree-tracing-and-signal-handling)
- [6. Production Best Practices](#6-production-best-practices)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Troubleshooting](#8-performance-security--troubleshooting)
- [9. Architectural Comparison Table](#9-architectural-comparison-table)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

In the late 1990s and early 2000s, web infrastructure hit the **C10K problem**: traditional web servers (such as Apache HTTP Server with prefork or worker MPM) could not scale past 10,000 simultaneous client connections. Traditional servers dedicated a separate operating system process or thread to every incoming connection. When concurrency surged, the OS kernel spent 90% of its CPU time performing thread context switches, thrashing CPU caches, and exhausting stack memory pools.

Igor Sysoev created NGINX in 2004 to solve the C10K problem by introducing a revolutionary architectural paradigm: **asynchronous, event-driven, non-blocking I/O multiplexing paired with a decoupled Master-Worker process model**.

Instead of dedicating a thread per connection:
- A single NGINX worker process runs an ultra-fast event loop powered by modern OS kernel event notifications (`epoll` on Linux, `kqueue` on BSD/macOS).
- A single worker process can handle **tens of thousands of concurrent connections simultaneously** with deterministic memory consumption and near-zero CPU context switching overhead.

---

## 2. Core Concepts

### The Master-Worker Process Hierarchy
NGINX executes as a hierarchical process tree composed of three distinct process types:
1. **The Master Process**:
   - Runs as `root` (or the privileged user executing NGINX).
   - Does **not** process client requests, parse HTTP packets, or execute SSL handshakes.
   - Reads, tokenizes, and validates the configuration file (`nginx.conf`).
   - Binds low-numbered privileged network ports (e.g., port 80 and 443).
   - Manages the lifecycle of worker processes (spawning, monitoring, graceful restarts, configuration reloads).
2. **Worker Processes**:
   - Run as an unprivileged system user (`nginx` or `www-data`) to enforce security isolation.
   - Inherit the open socket file descriptors from the master process.
   - Execute the non-blocking `epoll` event loop, accept incoming client connections, execute TLS handshakes, evaluate location routing rules, and stream response bodies.
3. **Helper Processes (Cache Loader & Cache Manager)**:
   - `ngx_cache_loader`: Spawns at boot/reload to read on-disk cache metadata into the shared memory `keys_zone` and then cleanly exits.
   - `ngx_cache_mgr`: Runs as a background daemon, enforcing cache `max_size` caps via LRU eviction and pruning inactive cached files.

### The Non-Blocking Event Loop (`epoll`)
On Linux, NGINX utilizes the `epoll` I/O multiplexing facility. Rather than constantly polling thousands of sockets (which wastes CPU), NGINX registers all client and upstream socket file descriptors with the kernel using `epoll_ctl()`. 
When network packets arrive at the physical NIC, the kernel fires an interrupt, marks the socket ready, and awakens the NGINX worker via `epoll_wait()`. The worker processes the ready events in user space without blocking, and immediately returns to sleep until the next event arrives.

### CPU Affinity (`worker_cpu_affinity`)
To achieve peak performance, each NGINX worker process can be pinned to a dedicated physical CPU core. Binding workers to specific cores prevents the Linux OS scheduler from migrating worker threads across different CPU cores, eliminating CPU cache misses (L1/L2 cache thrashing) and maximizing throughput.

### Kernel Socket Sharding (`SO_REUSEPORT`)
Historically, all worker processes competed for incoming connections on a single shared listen socket, causing lock contention in the kernel (the "thundering herd" on `accept()`). With `SO_REUSEPORT` (`listen 80 reuseport;`), the Linux kernel creates multiple internal socket queues—one for each worker—and distributes incoming TCP SYN packets across workers using a kernel-level 4-tuple hash.

---

## 3. How It Works Internally

### NGINX Process Tree and Inter-Process Communication (IPC)

```
                     +---------------------------------------+
                     |         Master Process (root)         |
                     |  - Reads & parses nginx.conf          |
                     |  - Binds ports 80 & 443 (bind/listen) |
                     |  - Traps OS signals (HUP, USR2, QUIT) |
                     +-------------------+-------------------+
                                         |
               +-------------------------+-------------------------+
               | fork() & setuid(nginx)                            |
               |                                                   |
+--------------v---------------+                   +--------------v---------------+
|    Worker Process 1 (nginx)  |                   |    Worker Process 2 (nginx)  |
|  - Pinned to CPU Core 0      |                   |  - Pinned to CPU Core 1      |
|  - epoll event loop          |                   |  - epoll event loop          |
|  - Handles 20k sockets       |                   |  - Handles 20k sockets       |
+--------------+---------------+                   +--------------+---------------+
               \                                                   /
                \                                                 /
                 +-----------------------+-----------------------+
                                         | IPC via Channel / Sockets
                                         v
                         +-------------------------------+
                         |   Shared Memory Zones (RAM)   |
                         | - ssl_session_cache           |
                         | - limit_req_zone / limit_conn |
                         | - proxy_cache keys_zone       |
                         +-------------------------------+
```

### The Worker epoll Event Loop Cycle

```
                      +-----------------------------+
                      | Worker starts loop cycle    |
                      +--------------+--------------+
                                     |
                                     v
                      +-----------------------------+
                      |   epoll_wait(ep, events,    |
                      |              nevents, timer)|
                      +--------------+--------------+
                                     | (Awakened by Kernel)
                                     v
                  Are there I/O events in events list?
                             /               \
                           YES                NO (Timer expired)
                           /                     \
                          v                       v
               Iterate through events array   Process timer events
                          |                   (timeouts, keepalive)
                          v
        +-----------------+-----------------+
        |                                   |
Event on Listen Socket?             Event on Client / Upstream Socket?
        |                                   |
        v                                   v
  accept4() connection               read() / write() non-blocking
  Create ngx_connection_t            Advance HTTP state machine
  Register with epoll                If blocked, return to epoll
```

---

## 4. Syntax & Directives Reference

| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `worker_processes number\|auto` | `main` | `1` | Number of worker processes. Recommended: `auto` (1 per CPU core). |
| `worker_cpu_affinity cpumask ...` | `main` | None | Binds worker processes to specific CPU cores using bitmasks. |
| `worker_rlimit_nofile number` | `main` | None | Sets maximum open file descriptors (`RLIMIT_NOFILE`) for workers. |
| `worker_priority number` | `main` | `0` | Sets scheduling priority (`nice` value: -20 to 19) of worker processes. |
| `events { ... }` | `main` | None | Context configuring network connection handling. |
| `worker_connections number` | `events` | `512` | Max simultaneous connections per worker process. |
| `use epoll\|kqueue\|eventport` | `events` | Auto-detected | Selects the underlying OS event-notification mechanism. |
| `multi_accept on\|off` | `events` | `off` | When on, worker accepts all pending connections from queue in one loop. |
| `accept_mutex on\|off` | `events` | `off` (since 1.11.3)| Legacy lock serializing `accept()` across workers (obsolete with `reuseport`). |

---

## 5. Practical Implementation Examples

### Example 1: Production Multi-Core Worker Tuning with CPU Affinity

An optimized `nginx.conf` designed for an 8-core modern Linux bare-metal server or cloud instance.

```nginx
# /etc/nginx/nginx.conf

user nginx;
pid /var/run/nginx.pid;

# 1. Automatically detect number of CPU cores and spawn 1 worker per core
worker_processes auto;

# 2. Pin workers directly to CPU cores:
# Worker 0 -> Core 0 (00000001)
# Worker 1 -> Core 1 (00000010)
# Worker 2 -> Core 2 (00000100) ...
worker_cpu_affinity auto;

# 3. Elevate worker scheduling priority above background processes
worker_priority -5;

# 4. Set OS file descriptor limit per process (Must exceed worker_connections * 2)
worker_rlimit_nofile 131072;

events {
    # Max simultaneous connections per worker
    worker_connections 32768;

    # Explicitly use Linux epoll engine
    use epoll;

    # Accept all pending connections in queue at once
    multi_accept on;
}

http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;
    
    server {
        listen 80;
        server_name _;
        return 200 "High-concurrency worker cluster ready.\n";
    }
}
```

### Example 2: High-Concurrency Event Loop Tuning (65,536 Connections)

Tuning NGINX and Linux kernel parameters to support over 100,000 simultaneous active connections.

#### Kernel Tuning (`/etc/sysctl.d/99-nginx-concurrency.conf`):
```ini
# Maximum open files system-wide
fs.file-max = 2097152

# Socket listen queue backlog cap (prevents SYN drops during traffic spikes)
net.core.somaxconn = 65535

# Increase incoming packet queue length
net.core.netdev_max_backlog = 65535

# Ephemeral port range for upstream connections
net.ipv4.ip_local_port_range = 1024 65535

# Fast recycling of closed TCP sockets
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_fin_timeout = 15
```
Apply settings: `sysctl -p /etc/sysctl.d/99-nginx-concurrency.conf`.

#### NGINX Event Configuration:
```nginx
# In /etc/nginx/nginx.conf
worker_processes auto;
worker_rlimit_nofile 262144; # Enough FDs for 65k dual sockets

events {
    worker_connections 65536;
    use epoll;
    multi_accept on;
}
```

### Example 3: Kernel Socket Sharding with `SO_REUSEPORT`

Leveraging the Linux kernel's `SO_REUSEPORT` to distribute incoming connections across workers directly in kernel space without lock contention.

```nginx
# /etc/nginx/conf.d/reuseport.conf

server {
    # 'reuseport' instructs kernel to create independent receive queues per worker
    listen 80 reuseport;
    listen 443 ssl reuseport;
    http2 on;
    server_name api.example.com;

    ssl_certificate     /etc/ssl/certs/api.crt;
    ssl_certificate_key /etc/ssl/private/api.key;

    location / {
        proxy_pass http://backend_pool;
        proxy_set_header Host $host;
    }
}
```

### Example 4: Inspecting Worker epoll Loop and CPU Utilization via OS Tools

Verifying that worker processes are evenly pinned to CPU cores and observing `epoll` system calls.

```bash
# 1. Inspect NGINX Process Tree and User Ownership
ps -ef --forest | grep nginx

# Sample output:
# root     1234      1  0 Mar25 ?  00:00:00 nginx: master process /usr/sbin/nginx
# nginx    1235   1234  2 Mar25 ?  00:15:20  \_ nginx: worker process
# nginx    1236   1234  2 Mar25 ?  00:15:18  \_ nginx: worker process

# 2. Verify Worker CPU Core Pinning (Affinity Mask)
# Taskset shows which CPU core each PID is bound to:
taskset -p $(pgrep -f "nginx: worker" | head -n 1)

# 3. Trace Non-Blocking epoll System Calls via strace
strace -f -p $(pgrep -f "nginx: worker" | head -n 1) -e trace=epoll_wait,accept4,read,write

# 4. Inspect Socket Queue Length on Port 80 (Check for backlog drops)
ss -lnt '( sport = :80 )'
# Recv-Q shows connections waiting in queue; Send-Q shows somaxconn cap
```

### Example 5: Dynamic Process Tree Tracing and Signal Handling

Demonstrates how the Master process handles binary upgrades and configuration reloads via signals without dropping connections.

```bash
# 1. Graceful Configuration Reload (SIGHUP / -1)
# Master re-parses config, spawns new generation of workers, drains old workers:
kill -HUP $(cat /run/nginx.pid)

# 2. Binary Upgrade in Place (SIGUSR2 / -12)
# Master renames PID file to .oldbin and spawns a NEW Master process:
kill -USR2 $(cat /run/nginx.pid)

# Check process tree: you will see TWO master processes running simultaneously:
ps -ef | grep nginx

# 3. Gracefully Stop Old Worker Processes (SIGWINCH / -28)
# Instructs old master to shut down its workers, shifting all new traffic to new master:
kill -WINCH $(cat /run/nginx.pid.oldbin)

# 4. Finalize Upgrade by Terminating Old Master (SIGQUIT / -3)
kill -QUIT $(cat /run/nginx.pid.oldbin)
```

---

## 6. Production Best Practices

1. **Always Set `worker_processes auto;`**: Never hardcode an integer like `worker_processes 4;`. Modern containerized and cloud environments dynamically scale CPU allocations; `auto` automatically binds to available CPU cores via `sysconf(_SC_NPROCESSORS_ONLN)`.
2. **Ensure `worker_rlimit_nofile` Exceeds `worker_connections * 2`**: Each proxied HTTP request requires **two file descriptors** (one for client socket, one for upstream socket). If `worker_connections` is 32,768, set `worker_rlimit_nofile` to at least `65,536` (plus headroom for log files, listening sockets, and disk buffers; recommended: `131,072`).
3. **Always Enable `use epoll;` on Linux**: Modern Linux kernels use `epoll`. While NGINX auto-detects this, declaring it explicitly guarantees portable build safety.
4. **Enable `multi_accept on;` in High-Throughput Environments**: When a traffic spike hits, multiple connection requests accumulate in the OS listen queue. `multi_accept on;` allows a worker to drain the entire backlog in a single `accept4()` loop rather than accepting one connection per event cycle.
5. **Use `reuseport` on High-Core Systems ($\ge 8$ Cores)**: On multi-socket NUMA systems and servers with 16+ cores, `reuseport` eliminates kernel lock contention on the accept mutex and load balances traffic evenly across cores in hardware.
6. **Never Run Workers as `root`**: The `user` directive must always specify an unprivileged system user (`nginx` or `www-data`). If a vulnerability is exploited in a worker, the attacker is trapped inside unprivileged user space.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **`worker_connections` exceeds OS `ulimit -n`** | NGINX workers fail to open sockets, logging: `accept4() failed (24: Too many open files)`. | Set `worker_rlimit_nofile` in `nginx.conf` and adjust `/etc/security/limits.conf`. |
| **`worker_processes` set higher than CPU core count** | Over-subscribing CPU cores forces the Linux kernel scheduler to context-switch between NGINX workers, degrading throughput. | Match worker process count strictly to physical core count: `worker_processes auto;`. |
| **Omitting `worker_rlimit_nofile`** | The NGINX worker inherits the default systemd `LimitNOFILE` (often only 1024), capping max connections to ~500 despite high `worker_connections`. | Explicitly configure `worker_rlimit_nofile 131072;` in the main context. |
| **Enabling `accept_mutex on;` on modern Linux** | `accept_mutex` was a workaround for the 2.6 Linux kernel "thundering herd" bug. On modern Linux, it adds unnecessary mutex lock overhead. | Keep `accept_mutex off;` (the modern default) and use `reuseport`. |
| **Hardkilling the master process (`kill -9`)** | Leaves unmanaged orphan worker processes running in the background listening on ports, preventing NGINX from restarting. | Always terminate via graceful signals: `nginx -s stop` or `kill -QUIT $(cat /run/nginx.pid)`. |
| **Modifying `reuseport` on graceful reload** | Sockets with `SO_REUSEPORT` cannot be rebound dynamically via SIGHUP reload; NGINX logs an error. | Perform a full restart (`systemctl restart nginx`) or binary upgrade (`kill -USR2`). |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Check Active Connections and Worker Health via stub_status
curl -s http://127.0.0.1/nginx_status
# Output:
# Active connections: 2450
# server accepts handled requests
#  145020 145020 489020
# Reading: 12 Writing: 48 Waiting: 2390

# 2. Inspect File Descriptor Usage of Running Worker
PID=$(pgrep -f "nginx: worker" | head -n 1)
ls -1 /proc/$PID/fd | wc -l

# 3. Check Effective Resource Limits of Worker Process
cat /proc/$PID/limits | grep "Max open files"

# 4. Measure epoll Latency Overhead via perf
perf top -p $PID
```

### Understanding `stub_status` Metrics
- `Active connections`: Total open client connections currently maintained.
- `Reading`: Connections where NGINX is actively reading request headers.
- `Writing`: Connections where NGINX is writing response data back to the client or waiting for upstream responses.
- `Waiting`: Idle keepalive connections held open waiting for new requests.

---

## 9. Architectural Comparison Table

| Dimension / Metric | NGINX (Event-Driven) | Apache HTTPD (Prefork MPM) | Apache HTTPD (Event MPM) |
| :--- | :--- | :--- | :--- |
| **Concurrency Model** | Single thread per core + `epoll` | Single process per connection | Worker threads + event listener |
| **Memory per Connection** | **~2 KB to 4 KB** | ~2 MB to 10 MB | ~100 KB to 500 KB |
| **Context Switching** | Minimal (Only when I/O ready) | Severe under high concurrency | Moderate |
| **Max Concurrent Sockets**| **500,000+** | Typically capped at ~1,000 | Typically capped at ~10,000 |
| **Non-blocking Proxying**| Native and optimized | Poor | Moderate |
| **C10K Capable** | **Yes (Solves C100K/C1000K)** | No | Yes (with tuning) |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: How does NGINX's single-threaded event loop handle thousands of concurrent requests without blocking?</strong></summary>

**Answer:**
NGINX uses asynchronous, non-blocking I/O multiplexing driven by kernel event-notification mechanisms (primarily `epoll` on Linux and `kqueue` on BSD). 

When an NGINX worker starts, it sets all socket file descriptors to non-blocking mode (`O_NONBLOCK`) and registers them with an `epoll` instance. Instead of waiting synchronously for a single client to transmit bytes, the worker calls `epoll_wait()`. The operating system kernel monitors all sockets; when network packets arrive, the kernel notifies NGINX, which reads or writes data until the socket would block (`EAGAIN` or `EWOULDBLOCK`). 

The worker then registers the socket's next expected state with `epoll` and immediately switches to the next ready socket in the queue. Because no thread ever sleeps or waits on an individual client, a single worker process can cycle through tens of thousands of active connections in milliseconds.
</details>

<details>
<summary><strong>Q2: What is the exact role of the NGINX Master Process, and why does it not process client traffic?</strong></summary>

**Answer:**
The Master Process is dedicated strictly to privileged orchestration and process lifecycle management:
1. It reads, parses, and validates the configuration file (`nginx.conf`).
2. It binds privileged network ports (e.g., ports 80 and 443 require root privileges on Linux).
3. It spawns unprivileged worker processes via `fork()` and drops permissions via `setuid(nginx)`.
4. It traps operating system signals (`SIGHUP`, `SIGQUIT`, `SIGUSR2`) to orchestrate zero-downtime configuration reloads and binary upgrades.

The Master Process intentionally does *not* handle client traffic for security and stability reasons: if a client request causes a crash, buffer overflow, or segmentation fault, only an unprivileged worker process dies. The master immediately detects the worker's death via `SIGCHLD` and spawns a fresh replacement worker instantly, ensuring 100% uptime.
</details>

<details>
<summary><strong>Q3: Explain how NGINX achieves zero-downtime binary upgrades using SIGUSR2 and SIGWINCH.</strong></summary>

**Answer:**
When upgrading the NGINX executable binary without dropping connections:
1. The operator sends `SIGUSR2` to the running master process (`kill -USR2 <master_pid>`).
2. The old master moves its PID file to `nginx.pid.oldbin` and executes the new NGINX binary (`execve`), passing the open listen socket file descriptors to the new binary.
3. The new master process boots, validates its configuration, and spawns a new generation of worker processes to begin accepting incoming traffic on the shared sockets.
4. The operator sends `SIGWINCH` to the old master process (`kill -WINCH <old_master_pid>`). The old master instructs its old worker processes to gracefully drain active requests and terminate.
5. If the new version is operating flawlessly, the operator sends `SIGQUIT` to the old master process to exit. If a bug is detected, the old master can be signaled with `SIGHUP` to re-spawn its original workers, rolling back instantly.
</details>

<details>
<summary><strong>Q4: Why does each worker process require at least twice the number of file descriptors as worker_connections?</strong></summary>

**Answer:**
In a reverse proxy configuration (`proxy_pass`):
- For every incoming client request, NGINX opens **one file descriptor** for the client-to-NGINX TCP socket.
- NGINX then opens a **second file descriptor** for the NGINX-to-upstream-backend TCP socket.
- In addition, NGINX opens file descriptors for log files (`access.log`, `error.log`), disk cache storage files, client body temporary files, and epoll control handles.

Therefore, if `worker_connections` is configured to `32,768`, the worker process can potentially manage 32,768 client sockets plus 32,768 upstream sockets simultaneously, requiring at least 65,536 file descriptors. Setting `worker_rlimit_nofile 131072;` guarantees ample headroom.
</details>

<details>
<summary><strong>Q5: What is the "Thundering Herd" problem on accept(), and how does SO_REUSEPORT solve it?</strong></summary>

**Answer:**
Historically, in multi-process web servers, multiple worker processes listened on the exact same shared socket file descriptor. When a new incoming TCP connection arrived, the operating system kernel awakened *all* waiting worker processes simultaneously. However, only *one* worker process could successfully call `accept()` and acquire the socket; the other workers failed and went back to sleep. This wasted significant CPU cycles ("thundering herd").

NGINX originally solved this in user space via `accept_mutex`, which serialized calls to `accept()` using a shared memory spinlock.

Modern Linux kernels (3.9+) solve this at the kernel level via **`SO_REUSEPORT`** (`listen 80 reuseport;`):
- The kernel allocates a dedicated listen socket queue for **each** worker process.
- When a client sends a TCP SYN packet, the Linux kernel hashes the 4-tuple and places the connection directly into one specific worker's queue.
- Only that single worker is awakened, completely eliminating lock contention and thundering herd overhead.
</details>

<details>
<summary><strong>Q6: What is the purpose of worker_cpu_affinity, and how does it prevent L1/L2 cache thrashing?</strong></summary>

**Answer:**
In multi-core operating systems, the Linux CFS (Completely Fair Scheduler) may periodically migrate a running process from one physical CPU core to another to balance thermal or workload distributions. 

When a process is migrated to a new core:
- The CPU's fast L1 and L2 caches on the new core are empty ("cold").
- The CPU must perform expensive memory bus operations to reload cache lines from main RAM, degrading throughput.

`worker_cpu_affinity` binds each NGINX worker process strictly to a dedicated physical CPU core. The worker runs permanently on that core, keeping CPU instruction and data caches hot, minimizing context switches, and maximizing instructions-per-cycle (IPC) throughput.
</details>

<details>
<summary><strong>Q7: What does multi_accept on do, and when should it be disabled?</strong></summary>

**Answer:**
By default (`multi_accept off;`), when NGINX is awakened by `epoll` indicating that new connections are waiting in the listen queue, the worker calls `accept4()` once, accepts a single connection, and returns to the event loop.

With `multi_accept on;`, the worker executes `accept4()` in a tight loop until the listen backlog is completely empty, accepting all pending connections at once before returning to the event loop.

- **When to enable**: High-throughput web environments with sudden, heavy traffic bursts.
- **When to disable**: Low-core servers or when individual workers might become overloaded with connection handshakes while starving already accepted client connections waiting for reads/writes.
</details>

<details>
<summary><strong>Q8: What are the Cache Loader and Cache Manager helper processes?</strong></summary>

**Answer:**
- **`ngx_cache_loader`**: An ephemeral helper process spawned at NGINX startup or configuration reload. It scans the cache directory tree on disk, parses the metadata headers of all cached files, and populates the in-memory shared `keys_zone` red-black tree. Once the filesystem scan is complete, it terminates cleanly.
- **`ngx_cache_mgr`**: A persistent helper process that runs in the background. It periodically checks the total disk usage of the cache directory. If usage exceeds `max_size`, it executes an LRU (Least Recently Used) eviction algorithm to delete older cache files until usage is within limits. It also purges files whose age exceeds the `inactive` parameter.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Worker Processes and Verify CPU Affinity</strong></summary>

**Goal:** Configure `worker_processes auto;` and `worker_cpu_affinity auto;`, reload NGINX, and verify worker core pinning via `taskset`.

**Solution:**
In `/etc/nginx/nginx.conf`:
```nginx
worker_processes auto;
worker_cpu_affinity auto;
```
Reload and verify:
```bash
nginx -t && systemctl reload nginx
for pid in $(pgrep -f "nginx: worker"); do
  taskset -c -p $pid
done
# Output will show each PID pinned to a distinct core:
# pid 1234's current affinity list: 0
# pid 1235's current affinity list: 1
```
</details>

<details>
<summary><strong>Exercise 2: Tune File Descriptor Limits for High Concurrency</strong></summary>

**Goal:** Configure `worker_rlimit_nofile 65536;` and verify that the running worker process has adopted the new limit via `/proc`.

**Solution:**
In `/etc/nginx/nginx.conf`:
```nginx
worker_rlimit_nofile 65536;

events {
    worker_connections 32768;
}
```
Reload and verify:
```bash
nginx -t && systemctl reload nginx
PID=$(pgrep -f "nginx: worker" | head -n 1)
grep "Max open files" /proc/$PID/limits
# Must show: Max open files 65536 65536
```
</details>

<details>
<summary><strong>Exercise 3: Test Zero-Downtime Reload with SIGHUP</strong></summary>

**Goal:** Trigger a zero-downtime configuration reload and observe the PID changes in the worker processes while the master PID remains constant.

**Solution:**
```bash
# Record master PID:
MASTER_PID=$(cat /run/nginx.pid)
echo "Master PID: $MASTER_PID"

# List old worker PIDs:
pgrep -f "nginx: worker"

# Send SIGHUP:
kill -HUP $MASTER_PID

# List new worker PIDs:
pgrep -f "nginx: worker"
# Verify master PID is unchanged, but all worker PIDs are new!
```
</details>

<details>
<summary><strong>Exercise 4: Trace epoll Event Calls via strace</strong></summary>

**Goal:** Attach `strace` to an active NGINX worker process and observe non-blocking socket handling during an HTTP request.

**Solution:**
```bash
PID=$(pgrep -f "nginx: worker" | head -n 1)
strace -f -p $PID -e epoll_wait,accept4,recvfrom,writev 2>&1 &
STRACE_PID=$!

# Make curl request:
curl -s http://localhost/ > /dev/null

# Terminate strace:
kill $STRACE_PID
```
</details>

<details>
<summary><strong>Exercise 5: Configure SO_REUSEPORT on Port 8080</strong></summary>

**Goal:** Configure an endpoint using `reuseport` and inspect kernel socket sharding via `ss`.

**Solution:**
```nginx
server {
    listen 8080 reuseport;
    server_name test.local;
    return 200 "Reuseport active\n";
}
```
Restart NGINX (`systemctl restart nginx`) and inspect:
```bash
ss -tulpn | grep 8080
# Will show multiple LISTEN lines on port 8080 (one for each worker process)
```
</details>

<details>
<summary><strong>Exercise 6: Test Graceful Worker Drain via SIGQUIT</strong></summary>

**Goal:** Send `SIGQUIT` to an individual worker process and observe that it finishes processing before exiting.

**Solution:**
```bash
WORKER_PID=$(pgrep -f "nginx: worker" | head -n 1)
kill -QUIT $WORKER_PID
# The master process immediately detects the termination and spawns a new worker to replace it.
ps -ef | grep nginx
```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Gold Standard Production Main & Events Template

# Main Context
user nginx;
worker_processes auto;
worker_cpu_affinity auto;
worker_rlimit_nofile 131072;
worker_priority -5;

events {
    worker_connections 32768;
    use epoll;
    multi_accept on;
}

http {
    # Server block with kernel socket sharding
    server {
        listen 80 reuseport;
        listen 443 ssl reuseport;
        http2 on;
        server_name example.com;

        location / {
            proxy_pass http://backend;
        }
    }
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX Architecture Guide](https://www.nginx.com/resources/library/complete-nginx-cookbook/)
- [Inside NGINX: How We Designed for Performance & Scale (NGINX Blog)](https://www.nginx.com/blog/inside-nginx-how-we-designed-for-performance-scale/)
- [Linux man pages: epoll(7)](https://man7.org/linux/man-pages/man7/epoll.7.html)
- [The C10K Problem (Dan Kegel)](http://www.kegel.com/c10k.html)

---

[← Module Overview](README.md) | [Next: Request Lifecycle & Phases →](02-request-lifecycle-and-phases.md)
