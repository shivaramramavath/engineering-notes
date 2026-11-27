# Socket Investigation and Port Troubleshooting with `ss` and `lsof`

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Socket Statistics (`ss`), Socket States, Buffer Tuning, Port Conflicts, and `lsof` |
| **Module** | `04-networking` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `04-networking/01-networking-basics.md`, `03-system/01-processes-and-signals.md` |
| **Target Audience** | SREs, Systems Engineers, DevOps Practitioners, Network Administrators |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools / Files** | `ss`, `lsof`, `fuser`, `/proc/net/sockstat`, `/proc/sys/net/ipv4/` |

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

In production environments running thousands of concurrent microservices, proxying millions of requests, socket exhaustion and port conflicts are frequent causes of severe outages. Common scenarios include:
- A new Nginx deployment crashing on startup with `bind() to 0.0.0.0:80 failed (98: Address already in use)`.
- An outbound API client failing with `Cannot assign requested address` (`EADDRNOTAVAIL`) due to ephemeral port starvation.
- Sockets accumulating in `CLOSE_WAIT` until file descriptor limits (`EMFILE`) paralyze the operating system.
- Sockets stuck in `TIME_WAIT` consuming kernel memory.

For decades, sysadmins relied on `netstat`. However, on modern high-load systems with 50,000+ active connections, `netstat` is dangerously slow and locks kernel data structures. **`ss` (Socket Statistics)**, part of the `iproute2` package, provides orders of magnitude faster execution, deep TCP state filtering, and rich socket memory telemetry via Linux Netlink sockets. Paired with **`lsof`**, it enables instant correlation between network sockets, file descriptors, and running processes.

---

## Core Concepts

### Why `netstat` is Deprecated
`netstat` reads `/proc/net/tcp` and `/proc/net/tcp6` line-by-line as raw ASCII text.
- Under heavy connection counts (e.g. 100,000 active sockets), parsing `/proc/net/tcp` requires the kernel to hold internal socket hash table locks while formatting text strings, causing CPU spikes and network packet latency.
- In contrast, `ss` uses the **Netlink protocol (`NETLINK_INET_DIAG`)**, querying the kernel directly for binary socket diagnostic structures (`inet_diag_msg`). Filtering occurs inside the kernel, returning only matching records instantly.

### The Lifecycle of a TCP Port
A TCP port is not a physical device; it is a 16-bit integer identifier ($0$ to $65535$) used by the transport layer to demultiplex packet streams to specific sockets.
- **Well-Known Ports** ($0$–$1023$): Reserved for system and privileged services (HTTP 80, HTTPS 443, SSH 22). Binding requires `CAP_NET_BIND_SERVICE` or root privileges.
- **Registered Ports** ($1024$–$49151$): Assigned to specific user services (Postgres 5432, Redis 6379, MySQL 3306).
- **Dynamic / Ephemeral Ports** ($49152$–$65535$, or $32768$–$60999$ by default on Linux): Automatically assigned by the kernel for outbound client connections.

### Socket Queue Semantics: `Recv-Q` and `Send-Q`
Understanding the dual meaning of `Recv-Q` and `Send-Q` in `ss` output is critical for troubleshooting:

| Socket State | `Recv-Q` Meaning | `Send-Q` Meaning | Diagnostic Warning Signs |
| :--- | :--- | :--- | :--- |
| **LISTEN** | Current number of connections waiting in the **Accept Queue** | Maximum capacity of the Accept Queue (backlog) | If `Recv-Q > Send-Q`, the application cannot keep up with incoming connections; new clients get dropped or timed out! |
| **ESTABLISHED** | Number of bytes received from the network but **not yet read** by the application via `read()` / `recv()` | Number of bytes sent by the application but **not yet acknowledged** (`ACK`) by the remote peer | High `Recv-Q`: App thread blocked or slow. High `Send-Q`: Remote receiver slow, network packet loss, or MTU blackhole. |

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                  Userspace Utility: ss / lsof                           |
+----------------------------------------------------+------------------------------------+
                                                     |
                         Netlink Socket Request:     | AF_NETLINK, NETLINK_INET_DIAG
                         INET_DIAG_REQ_V2            | (Filtered: state, port, family)
                                                     v
+-----------------------------------------------------------------------------------------+
|                                    Linux Kernel Subsystem                               |
|                                       inet_diag.ko                                      |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                               TCP Established & Listen Hash Tables                      |
|                                     tcp_hashinfo.ehash                                  |
|                                     tcp_hashinfo.listening_hash                         |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                           Extracts struct sock / inet_sock metrics                      |
|           State, RTT, CWND, Congestion Algorithm, Socket Buffer Allocations             |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
                               Binary Netlink Responses Streamed Back
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                               Formatted Display in Userspace                            |
|        State    Recv-Q Send-Q   Local Address:Port      Peer Address:Port               |
|        LISTEN   0      511            0.0.0.0:80             0.0.0.0:*                  |
+-----------------------------------------------------------------------------------------+
```

### Socket Memory Breakdown (`skmem`)
When debugging memory consumption, `ss -m` breaks down socket buffer allocations:
- `rmem_alloc` (`r`): Total memory allocated for receiving data packets.
- `rcv_buf` (`rb`): Maximum receive buffer limit for this socket (governed by `tcp_rmem`).
- `wmem_alloc` (`tb`): Total memory allocated for sending/transmitting data packets.
- `snd_buf` (`wb`): Maximum send buffer limit for this socket (governed by `tcp_wmem`).
- `fwd_alloc` (`f`): Pre-allocated memory page pool allocated by the kernel for this socket.

---

## Syntax & CLI Tools

### Standard `ss` Command Flags
```bash
# Core filter options
-t, --tcp         # Display TCP sockets
-u, --udp         # Display UDP sockets
-x, --unix        # Display Unix Domain Sockets
-l, --listening   # Display listening sockets only (omitted by default: shows established)
-a, --all         # Display both listening and non-listening (established, time-wait)
-n, --numeric     # Do not resolve service names (shows port 22 instead of ssh)
-p, --processes   # Show process name and PID owning the socket (requires sudo/root)
-e, --extended    # Show detailed socket attributes (UID, inode number, cookie)
-m, --memory      # Show socket buffer memory allocation (skmem)
-i, --info        # Show internal TCP diagnostics (RTT, CWND, MSS, retransmissions)
-s, --summary     # Display high-level summary counters of total sockets by type
```

### The Standard Production Flag Combination
```bash
# List all listening TCP and UDP sockets with numeric ports and owning processes
sudo ss -tulpn
```

### Powerful `ss` State and Address Filter Syntax
```bash
# Filter by TCP state
ss -tan state established
ss -tan state time-wait
ss -tan state close-wait
ss -tan state syn-recv

# Filter by port number
ss -tan '( sport = :443 or dport = :443 )'

# Filter by destination CIDR
ss -tan dst 10.0.0.0/16

# Filter by process name
ss -tanp '( sport = :80 )'
```

### Correlating with `lsof` and `fuser`
```bash
# List process holding port 80 open
sudo lsof -i :80

# List all TCP connections opened by a specific PID
sudo lsof -iTCP -a -p 1234 -P -n

# Identify and kill whatever process is holding port 5432
sudo fuser -v 5432/tcp
sudo fuser -k 5432/tcp   # SENDS SIGKILL! Use with caution
```

---

## Progressive Practical Examples

### Example 1: Resolving `Address already in use` Port Conflicts
**Scenario**: You attempt to start a new Nginx container or daemon, but it fails with port 80 already bound.

```bash
# 1. Identify which process is currently listening on port 80
sudo ss -tulpn | grep ":80"
# Output:
# tcp   LISTEN 0      128          0.0.0.0:80        0.0.0.0:*    users:(("apache2",pid=3842,fd=4))

# 2. Correlate with lsof to verify process hierarchy
sudo lsof -i :80
# COMMAND   PID     USER   FD   TYPE DEVICE SIZE/OFF NODE NAME
# apache2  3842     root    4u  IPv4  32819      0t0  TCP *:http (LISTEN)
# apache2  3843 www-data    4u  IPv4  32819      0t0  TCP *:http (LISTEN)

# 3. Stop or disable the conflicting service
sudo systemctl stop apache2
sudo systemctl disable apache2

# 4. Confirm port 80 is now completely free
sudo ss -tulpn | grep ":80"
# (Zero output confirms port is available)
```

### Example 2: Diagnosing Application Latency with `ss -ti` (TCP Internals)
Identify whether latency is caused by a slow client or packet loss using in-kernel metrics.

```bash
# Inspect internal TCP state of an active connection on port 443
ss -tin '( dport = :443 )'
```
**Sample Output**:
```text
ESTAB 0 0 192.168.1.50:41234 140.82.114.6:443
   cubic wscale:7,7 rto:204 rtt:14.2/0.8 ato:40 mss:1460 rcvspace:64240 ssthresh:10 cwnd:10
   retrans:0/1 lost:0 unack:0
```
- `rtt:14.2/0.8`: Round Trip Time is 14.2ms with 0.8ms jitter.
- `cwnd:10`: Congestion window size is 10 segments (~14.6 KB).
- `retrans:0/1`: 0 active retransmissions, 1 cumulative retransmission.

### Example 3: Auditing Ephemeral Port Exhaustion
When a client machine issues hundreds of HTTP requests per second to a backend API without HTTP Keep-Alive, sockets accumulate in `TIME_WAIT` until outbound ports run out.

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== Ephemeral Port Consumption Audit ==="

# 1. Get ephemeral port boundaries
RANGE=$(sysctl -n net.ipv4.ip_local_port_range)
PORT_MIN=$(echo "${RANGE}" | awk '{print $1}')
PORT_MAX=$(echo "${RANGE}" | awk '{print $2}')
TOTAL_EPHEMERAL=$(( PORT_MAX - PORT_MIN + 1 ))

echo "Ephemeral Port Range: ${PORT_MIN} to ${PORT_MAX} (Total: ${TOTAL_EPHEMERAL})"

# 2. Count active outbound sockets originating in ephemeral range
OUTBOUND_COUNT=$(ss -tan "( sport >= :${PORT_MIN} and sport <= :${PORT_MAX} )" | grep -v "State" | wc -l)
echo "Active Ephemeral Sockets: ${OUTBOUND_COUNT}"

# 3. Count sockets stuck in TIME_WAIT
TW_COUNT=$(ss -tan state time-wait | grep -v "State" | wc -l)
echo "Sockets in TIME_WAIT: ${TW_COUNT}"

# 4. Calculate saturation percentage
SATURATION=$(( OUTBOUND_COUNT * 100 / TOTAL_EPHEMERAL ))
echo "Port Pool Saturation: ${SATURATION}%"

if [ "${SATURATION}" -gt 80 ]; then
    echo "CRITICAL: Ephemeral port pool near exhaustion! Enable tcp_tw_reuse."
fi
```

### Example 4: Diagnosing Accept Queue Overflow on Loaded Web Servers
When a web server (Nginx, Gunicorn) is overwhelmed, incoming connections stall in the kernel accept queue.

```bash
# Check listening sockets where Recv-Q is filling up
ss -lnt
# Output:
# State   Recv-Q  Send-Q   Local Address:Port   Peer Address:Port
# LISTEN  129     128            0.0.0.0:80          0.0.0.0:*
```
Notice `Recv-Q (129)` exceeds `Send-Q (128)`. The application cannot call `accept()` fast enough. 
- Kernel drops newly arriving connection requests or replies with `SYN-ACK` retries.
- Check drops via: `netstat -s | grep -i "listen"` or `nstat -az TcpExtListenOverflows`.

### Example 5: Detecting Socket Leaks (`CLOSE_WAIT` Accumulation)
Automated watchdog script identifying processes leaking unclosed socket file descriptors.

```bash
#!/usr/bin/env bash
set -euo pipefail

THRESHOLD=50

echo "==> Auditing sockets stuck in CLOSE_WAIT..."
LEAKS=$(ss -tanp state close-wait | grep -v "State")

if [ -n "${LEAKS}" ]; then
    COUNT=$(echo "${LEAKS}" | wc -l)
    echo "Detected ${COUNT} sockets in CLOSE_WAIT."

    if [ "${COUNT}" -ge "${THRESHOLD}" ]; then
        echo "ALERT: High count of CLOSE_WAIT sockets detected!"
        echo "Top offending processes:"
        echo "${LEAKS}" | awk '{print $NF}' | sort | uniq -c | sort -rn | head -n 5
    fi
else
    echo "No CLOSE_WAIT leaks detected. System healthy."
fi
```

---

## Best Practices

1. **Standardize on `ss -tulpn` for Port Inspections**: Replace `netstat -tulpn` everywhere in runbooks, monitoring scripts, and documentation. `ss` is faster, non-blocking, and native to modern Linux.
2. **Always Pass `-n` to Prevent DNS Hanging**: Never omit `-n`. When inspecting thousands of sockets, resolving every IP to a hostname triggers thousands of reverse DNS lookups that will freeze your shell session.
3. **Tune `net.ipv4.tcp_tw_reuse = 1` for High-Throughput HTTP Clients**: In microservices that make frequent short-lived outbound HTTP connections, enable `tcp_tw_reuse`. This permits the kernel to safely reuse outgoing `TIME_WAIT` sockets for new connections to the same destination when timestamps match.
4. **Scale Both `somaxconn` and Application Backlogs Simultaneously**: If you increase `sysctl net.core.somaxconn = 4096`, you **must** also update your application configuration (e.g., `listen 80 backlog=4096;` in Nginx). The effective accept queue size is $\min(\text{somaxconn}, \text{backlog})$.
5. **Monitor Sockets via `/proc/net/sockstat` for Fast Polling**: Rather than running `ss -s` every second in monitoring agents (Datadog, Prometheus node_exporter), read `/proc/net/sockstat` directly. It exposes total allocated, in-use, and orphan sockets in a single lightweight text read.
6. **Beware of Ephemeral Port Collision on Multiple Local IPs**: Remember that the 5-tuple makes a connection unique: `{src_ip, src_port, dst_ip, dst_port, protocol}`. You can establish 50,000 connections from `10.0.0.1` to `1.1.1.1:443`, and **another** 50,000 connections from `10.0.0.1` to `8.8.8.8:443` simultaneously without running out of ephemeral ports!

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Running `netstat` on a node with 100,000 connections | Spikes CPU to 100% and holds kernel locks while formatting `/proc/net/tcp`. | Use `ss -tan` which queries the kernel via binary Netlink protocols. |
| Forgetting `sudo` when running `ss -p` or `lsof -i` | Non-root users cannot inspect socket file descriptors owned by other users (`pid=` field is empty). | Always run with `sudo` or as root to view process ownership. |
| Assuming `TIME_WAIT` sockets mean the server is broken | `TIME_WAIT` is normal TCP protocol design; it ensures delayed packets don't corrupt subsequent connections. | Do not panic; verify whether ports are running out before tuning. |
| Setting `tcp_tw_recycle = 1` on modern kernels | Removed in Linux 4.12+ because it broke connections for all users behind NAT routers. | Use `tcp_tw_reuse = 1`, never `tcp_tw_recycle`. |
| Increasing `somaxconn` without increasing app backlog | Application's `listen(fd, backlog)` parameter overrides kernel maximum if smaller. | Update both `net.core.somaxconn` and application config (e.g. Nginx `backlog=4096`). |
| Misinterpreting `Send-Q` on listening sockets | On listening sockets, `Send-Q` is the **backlog limit**, not queued outbound bytes! | Consult the state column: LISTEN vs ESTABLISHED queues differ completely. |

---

## Performance, Security & Cost Implications

### Performance
- **Socket Buffer Memory Footprint**: Every active TCP socket allocates memory for read and write buffers (`net.ipv4.tcp_rmem`, `tcp_wmem`). While minimum sizes are small (4 KB), under high bandwidth-delay products (BDP), each socket can expand to 4MB–6MB. 100,000 concurrent sockets buffering 2MB each consumes **200 GB of RAM**. Monitor `cat /proc/net/sockstat` under `TCP: mem` to prevent OOM panics.
- **Netlink Socket Query Speed**: In benchmarks on a system with 50,000 active TCP connections:
  - `netstat -tan`: ~1.8 seconds execution time, 85% CPU.
  - `ss -tan`: ~0.04 seconds execution time, 3% CPU.

### Security
- **Auditing Exposed Listeners**: Running `sudo ss -tulpn` reveals unintended management services binding to `0.0.0.0` (all interfaces) rather than `127.0.0.1` (localhost only). Common culprits: Redis, MongoDB, Docker API, Prometheus metrics endpoints (`:9090`).
- **Orphan Sockets in Denial of Service (DoS)**: Under slowloris or SYN flood attacks, sockets can get orphaned (not attached to any user file descriptor). Limit max orphan sockets using `net.ipv4.tcp_max_orphans`.

### Cost
- Prematurely scaling cloud instances due to "connection timeouts" when the root cause was simply an accept queue overflow (`somaxconn = 128`) wastes significant cloud compute spend. Tuning kernel socket limits allows single instances to handle 10x traffic volume.

---

## Comparison Table / Alternatives

| Diagnostic Task | Modern Standard Tool | Legacy / Deprecated Tool | Raw Kernel Interface |
| :--- | :--- | :--- | :--- |
| **List Listening Ports** | `ss -tulpn` | `netstat -tulpn` | Netlink (`NETLINK_INET_DIAG`) |
| **Inspect Active Sockets** | `ss -tan` | `netstat -tan` | `/proc/net/tcp` |
| **Identify Port Owner PID**| `lsof -i :port` or `fuser port/tcp` | `netstat -p` | `/proc/<pid>/fd/` |
| **Socket Memory Allocation**| `ss -tm` | Not easily available | `/proc/net/sockstat` |
| **TCP RTT & CWND Metrics** | `ss -ti` | Not available | Kernel `tcp_info` struct |
| **Kill Process on Port** | `fuser -k port/tcp` | `kill $(lsof -t -i :port)` | Process signal delivery |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: An application fails on startup with `bind(): Address already in use`. How do you find and eliminate the culprit?
<details>
<summary>View Solution</summary>

1. **Find the offending process**:
   ```bash
   sudo ss -tulpn | grep ":<port>"
   # Or using lsof:
   sudo lsof -i :<port>
   ```
2. **Analyze process status**:
   Inspect the PID. Is it a lingering zombie worker, another service (e.g. Apache conflicting with Nginx), or a crashed container?
3. **Terminate gracefully**:
   ```bash
   sudo systemctl stop <service_name>
   # Or if orphaned:
   sudo kill -15 <PID>
   ```
4. **Force kill if unresponsive**:
   ```bash
   sudo fuser -k -9 <port>/tcp
   ```
</details>

### Q2: What is the root cause of `Cannot assign requested address` (`EADDRNOTAVAIL`) on outbound API calls?
<details>
<summary>View Solution</summary>

**Cause**: **Ephemeral Port Exhaustion**.
When a client application opens thousands of outbound connections to the same target IP and port without reusing connections (HTTP connection pooling / keep-alive), the client exhausts all available ports in `/proc/sys/net/ipv4/ip_local_port_range`.
The kernel cannot allocate a unique 5-tuple for the new socket and rejects `connect()` with `EADDRNOTAVAIL`.

**Fixes**:
1. Implement HTTP Keep-Alive / connection pooling in application code.
2. Enable socket reuse: `sudo sysctl -w net.ipv4.tcp_tw_reuse=1`.
3. Expand local port range: `sudo sysctl -w net.ipv4.ip_local_port_range="10240 65535"`.
</details>

### Q3: Why is `Recv-Q` non-zero for an established TCP connection?
<details>
<summary>View Solution</summary>

For an `ESTABLISHED` connection, `Recv-Q` represents the number of bytes that the Linux kernel has received from the network, verified, and placed into the socket's receive buffer, but **the application has not yet read them** via `read()` or `recv()`.

If `Recv-Q` remains persistently high or grows:
- The application thread is CPU-bound, blocked on disk I/O, or stuck in a database query.
- The application is experiencing a long Garbage Collection (GC) pause.
- The application cannot keep up with the incoming data ingestion rate.
</details>

### Q4: What does `ss -s` tell you about overall system health?
<details>
<summary>View Solution</summary>

`ss -s` prints a summary of all socket allocations directly from kernel counters:
```text
Total: 412
TCP:   85 (estab 42, closed 12, orphaned 0, timewait 10)
Transport Total     IP        IPv6
RAW	  1         0         1        
UDP	  8         5         3        
TCP	  73        45        28       
INET	  82        50        32       
FRAG	  0         0         0        
```
It immediately highlights:
- High number of `orphaned` sockets (clients dropped mid-transfer; kernel waiting to flush).
- Excessive `timewait` sockets.
- Overall socket memory footprint.
</details>

### Q5: How do you identify which specific network namespace a socket belongs to?
<details>
<summary>View Solution</summary>

By default, `ss` only displays sockets in the **current network namespace**.
To inspect sockets in a Docker container or Kubernetes pod namespace:
1. Locate the container's PID: `docker inspect <container_id> --format '{{.State.Pid}}'`
2. Execute `ss` inside that namespace using `nsenter`:
   ```bash
   sudo nsenter -t <container_pid> -n ss -tulpn
   ```
</details>

### Q6: What is the significance of the `TIME-WAIT` state on a server vs on a client?
<details>
<summary>View Solution</summary>

The side of the connection that initiates the **active close** (sends the first `FIN`) is the side that enters the `TIME_WAIT` state.
- **On a Server** (e.g. HTTP/1.0 without keep-alive where server closes connection): Sockets in `TIME_WAIT` consume a small amount of kernel memory (`struct tcp_timewait_sock`), but do **not** cause port exhaustion because the server's listening port (e.g. 80) can accept new connections from any new client 5-tuple.
- **On a Client** (outbound API requests): Sockets in `TIME_WAIT` hold ephemeral ports open for 60 seconds, which **can cause complete client port exhaustion**.
</details>

### Q7: How do you check if TCP SYN packets are being dropped due to a full accept queue?
<details>
<summary>View Solution</summary>

Inspect TCP extension counters using `nstat` or `netstat -s`:
```bash
nstat -az TcpExtListenOverflows TcpExtListenDrops
```
- `TcpExtListenOverflows`: The number of times a connection could not be accepted because the accept queue was completely full.
- `TcpExtListenDrops`: The number of incoming SYNs or connection attempts dropped due to queue saturation or system memory limits.
If either counter is incrementing, increase `net.core.somaxconn` and application backlog.
</details>

### Q8: What is the difference between `lsof -i :80` and `fuser 80/tcp`?
<details>
<summary>View Solution</summary>

- `lsof -i :80`: Lists detailed information about every process, user, file descriptor number, socket type, and connection state using port 80.
- `fuser 80/tcp`: A lightweight tool that prints only the PIDs accessing port 80. Crucially, `fuser` includes the `-k` flag to send signals directly (e.g., `fuser -k -9 80/tcp` kills all processes holding port 80 instantly).
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Querying Listening Sockets and Process Ownership
**Task**: Use `ss` to list all listening TCP and UDP sockets with numeric ports and process names. Filter out loopback listeners.
<details>
<summary>View Solution</summary>

```bash
sudo ss -tulpn | grep -v "127.0.0.1" | grep -v "::1"
```
</details>

### Exercise 2: Simulating a Port Conflict and Resolving It
**Task**: Start a Python HTTP server on port `8888`. Attempt to start a second listener on the same port. Use `lsof` and `fuser` to locate and terminate the first server.
<details>
<summary>View Solution</summary>

```bash
# 1. Start listener
python3 -m http.server 8888 &
PID1=$!

# 2. Attempt to start second listener (observe failure)
python3 -m http.server 8888 2>&1 | grep "Address already in use" || true

# 3. Locate process using port
sudo lsof -i :8888

# 4. Kill using fuser
sudo fuser -k 8888/tcp

# 5. Verify port is released
ss -tan | grep ":8888"
```
</details>

### Exercise 3: Filtering Sockets by Specific States
**Task**: Query `ss` for all sockets currently in `TIME-WAIT` or `CLOSE-WAIT` states. Count their occurrences.
<details>
<summary>View Solution</summary>

```bash
# Count TIME-WAIT
ss -tan state time-wait | grep -v "State" | wc -l

# Count CLOSE-WAIT
ss -tan state close-wait | grep -v "State" | wc -l
```
</details>

### Exercise 4: Inspecting Internal TCP Congestion and RTT Metrics
**Task**: Make an active HTTPS connection to a remote site (e.g., download a small file) and query its TCP internal metrics (`ss -ti`) during transfer.
<details>
<summary>View Solution</summary>

```bash
# In background, stream download
curl -s https://speed.cloudflare.com/__down?bytes=50000000 > /dev/null &
CURL_PID=$!
sleep 0.5

# Inspect TCP metrics of the active transfer
ss -tin '( dport = :443 )'

wait "${CURL_PID}" 2>/dev/null || true
```
</details>

### Exercise 5: Auditing Socket Memory Utilization (`skmem`)
**Task**: Use `ss -tm` to inspect socket buffer allocations for established SSH or web connections.
<details>
<summary>View Solution</summary>

```bash
ss -tme state established | head -n 10
# Look for: skmem:(r<allocated>,rb<limit>,t<tx>,tb<limit>...)
```
</details>

### Exercise 6: Correlating Socket Inodes with `/proc`
**Task**: Pick a listening socket from `ss -e`. Extract its inode number and locate the corresponding file descriptor inside `/proc/<pid>/fd/`.
<details>
<summary>View Solution</summary>

```bash
# 1. Pick a listening socket and get inode
LINE=$(sudo ss -tulpne | grep -v "Netid" | head -n 1)
echo "${LINE}"

PID=$(echo "${LINE}" | awk -F'pid=' '{print $2}' | awk -F',' '{print $1}')
INODE=$(echo "${LINE}" | awk -F'ino:' '{print $2}' | awk '{print $1}')

echo "Process PID: ${PID}, Socket Inode: ${INODE}"

# 2. Verify inode in /proc/<pid>/fd/
sudo ls -l "/proc/${PID}/fd" | grep "${INODE}"
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **All Listening Ports + PIDs**| `sudo ss -tulpn` |
| **All Established TCP** | `ss -tan state established` |
| **All Sockets in TIME_WAIT** | `ss -tan state time-wait` |
| **Sockets on Port 443** | `ss -tan '( sport = :443 or dport = :443 )'` |
| **TCP Internal Diagnostics** | `ss -tin` |
| **Socket Memory Limits** | `ss -tm` |
| **Socket Summary Counts** | `ss -s` |
| **Identify Port Owner** | `sudo lsof -i :<port>` |
| **Kill Process on Port** | `sudo fuser -k <port>/tcp` |
| **Global Socket Counters** | `cat /proc/net/sockstat` |

### Further Reading
- `man 8 ss`
- `man 8 lsof` and `man 1 fuser`
- `man 7 tcp` and `man 7 ip`
- [iproute2 GitHub Repository](https://github.com/shemminger/iproute2)
- [Linux Kernel Socket Diagnostics Documentation](https://docs.kernel.org/networking/netlink_mmap.html)
