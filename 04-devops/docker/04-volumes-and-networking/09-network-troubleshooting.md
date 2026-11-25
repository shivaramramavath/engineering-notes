# Container Network Troubleshooting and Packet Diagnostics

```
Track: DevOps & Containerization
Module: 04-volumes-and-networking
Prerequisites: 04-volumes-and-networking/04-networking.md, 04-volumes-and-networking/06-ports-publishing-and-firewalls.md
Difficulty: Advanced
Estimated Duration: 50 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [The Network Diagnostic Decision Tree](#3-the-network-diagnostic-decision-tree)
4. [Essential Diagnostic Tooling (Netshoot)](#4-essential-diagnostic-tooling-netshoot)
5. [Syntax & Command Reference](#5-syntax--command-reference)
6. [Practical Implementation & Examples](#6-practical-implementation--examples)
7. [Production Best Practices & Antipatterns](#7-production-best-practices--antipatterns)
8. [Common Pitfalls & Troubleshooting](#8-common-pitfalls--troubleshooting)
9. [Performance Tuning & Resource Optimization](#9-performance-tuning--resource-optimization)
10. [Security Hardening & Threat Model](#10-security-hardening--threat-model)
11. [Architectural Comparison Table](#11-architectural-comparison-table)
12. [Real-World Scenario Walkthrough](#12-real-world-scenario-walkthrough)
13. [Self-Assessment & Interview Questions](#13-self-assessment--interview-questions)
14. [Hands-on Practice Exercises](#14-hands-on-practice-exercises)
15. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

Container networking operates across multiple virtualized layers: Linux network namespaces, virtual ethernet (`veth`) pairs, software bridge switches, iptables NAT tables, and embedded DNS daemons.

When microservices fail to connect, naive engineers resort to random container restarts or disabling firewalls. A production engineer uses a systematic, Layer-by-Layer troubleshooting methodology:
- **Layer 1/2 (Physical/Data Link)**: Is the virtual ethernet interface (`eth0`) up? Is the `veth` peer connected to the bridge?
- **Layer 3 (Network)**: Does the container have a valid IP address? Is the default gateway reachable? Are iptables rules dropping packets?
- **Layer 4 (Transport)**: Is the target listening on the expected port? Is TCP SYN receiving SYN-ACK, or is it silently dropped?
- **Layer 7 (Application/DNS)**: Does `127.0.0.11` resolve the target hostname? Is HTTP proxying intercepting the request?

---

## 2. Core Concepts

### Network Namespace Inspection via `nsenter`
Containers isolate networking inside a dedicated Linux network namespace (`/proc/<PID>/ns/net`).
Using the host `nsenter` utility, an administrator can execute diagnostic tools directly inside a target container's network namespace **without installing any packages inside the container itself**:
```bash
nsenter -t <CONTAINER_PID> -n netstat -tulpn
```

### The Netshoot Diagnostic Swiss Army Knife
In production, hardened containers (Alpine, Distroless, Scratch) omit diagnostic tools (`curl`, `dig`, `tcpdump`, `traceroute`, `nc`).
The industry standard solution is attaching **Netshoot** (`nicolaka/netshoot`)—a specialized container pre-loaded with every networking diagnostic utility—directly into the target container's network namespace using:
```bash
docker run --rm -it --net=container:<target_container> nicolaka/netshoot
```

---

## 3. The Network Diagnostic Decision Tree

```
                     +---------------------------------------+
                     | Symptom: Connection Failed / Timeout  |
                     +-------------------+-------------------+
                                         |
                                         v
                     +---------------------------------------+
                     | Step 1: Is DNS Resolving Name?        |
                     | Command: nslookup <target_service>    |
                     +-------------------+-------------------+
                                         |
                       +-----------------+-----------------+
                       |                                   |
                     Failed                              Passed
                       |                                   |
                       v                                   v
+-------------------------------+   +---------------------------------------+
| Fix DNS:                      |   | Step 2: Can Gateway Ping?             |
| - Check user-defined network  |   | Command: ping <gateway_ip>            |
| - Verify container is running |   +-------------------+-------------------+
| - Check /etc/resolv.conf      |                       |
+-------------------------------+     +-----------------+-----------------+
                                      |                                   |
                                    Failed                              Passed
                                      |                                   |
                                      v                                   v
       +------------------------------------+   +---------------------------------------+
       | Fix L2/L3 Bridge:                  |   | Step 3: Can Target TCP Port Connect?  |
       | - Verify veth pair attached        |   | Command: nc -zv <target_ip> <port>    |
       | - Check sysctl net.ipv4.ip_forward |   +-------------------+-------------------+
       +------------------------------------+                       |
                                              +---------------------+---------------------+
                                              |                                           |
                                            Failed                                      Passed
                                              |                                           |
                                              v                                           v
               +--------------------------------------+   +---------------------------------------+
               | Fix Port/Firewall:                   |   | L4 OK: Application Issue!             |
               | - Is service listening on 0.0.0.0?   |   | - Check HTTP 500 error logs           |
               | - Check iptables DOCKER-USER chain   |   | - Check TLS certificate expiry        |
               +--------------------------------------+   +---------------------------------------+
```

---

## 4. Essential Diagnostic Tooling (Netshoot)

Netshoot bundles dozens of diagnostic tools:
- **DNS**: `dig`, `drill`, `nslookup`, `bind-tools`
- **Socket / Port Discovery**: `nc` (netcat), `socat`, `nmap`, `ss`, `netstat`
- **Packet Capture & Sniffing**: `tcpdump`, `tshark`, `ngrep`
- **Routing & Path Tracing**: `traceroute`, `mtr`, `tracepath`, `ip route`
- **HTTP / Layer 7**: `curl`, `httpie`, `wget`, `openssl`
- **Bandwidth Benchmarking**: `iperf3`

---

## 5. Syntax & Command Reference

### Primary Diagnostic Commands

```bash
# 1. Attach Netshoot to any running container instantly
docker run --rm -it --net=container:<target_container> nicolaka/netshoot

# 2. Trace DNS resolution through Docker embedded DNS
dig @127.0.0.11 <service_name> +trace

# 3. Test TCP socket reachability with timeout
nc -zv -w 3 <target_host> <port>

# 4. Capture raw HTTP traffic inside container namespace
tcpdump -i eth0 -nn -s0 -A 'tcp port 80'

# 5. Trace Layer 3 packet hops with MTR
mtr -n --report <destination_ip>

# 6. Inspect host bridge interface status
ip link show type bridge
bridge link show
```

---

## 6. Practical Implementation & Examples

### Example 1: Debugging a Shell-Less Distroless Container with Netshoot
Inspecting active TCP connections and testing outbound database connectivity inside a container that has no shell.

```bash
# Target container: Running Distroless Node.js with no shell
docker run -d --name payment-api gcr.io/distroless/nodejs22-debian12

# Attach Netshoot into payment-api's network namespace
docker run --rm -it --net=container:payment-api nicolaka/netshoot

# (Inside Netshoot shell):
# 1. View all listening sockets of payment-api
ss -tulpn

# 2. Test connectivity to internal database
nc -zv postgres-db 5432

# 3. Capture live traffic
tcpdump -i eth0 -n
```

### Example 2: Live Packet Sniffing with `tcpdump` and Wireshark over SSH
Capturing real-time container network traffic on a remote production server and piping it directly into local Wireshark on a developer workstation.

```bash
# Stream live pcap from remote container directly into local Wireshark
ssh user@production-host.internal \
  "docker run --rm --net=container:web-prod nicolaka/netshoot tcpdump -i eth0 -U -w - not port 22" \
| wireshark -k -i -
```

### Example 3: Diagnosing DNS Latency and Resolution Failures
Checking query response times from Docker's embedded DNS server (`127.0.0.11`).

```bash
# Query with timing metrics
dig @127.0.0.11 redis-master.internal-net

# Output reveals:
# ;; Query time: 1 msec
# ;; SERVER: 127.0.0.11#53(127.0.0.11)
# ;; WHEN: Tue Feb 25 10:14:00 UTC 2025
# ;; MSG SIZE  rcvd: 78
```

### Example 4: Diagnosing Docker iptables Packet Drops
Identifying whether host firewall chains are dropping container forwarding traffic.

```bash
# Inspect iptables filter FORWARD and DOCKER-USER chains with packet and byte counters
sudo iptables -L DOCKER-USER -n -v --line-numbers
sudo iptables -L FORWARD -n -v --line-numbers

# Look for rules where the "pkts" counter is actively incrementing on DROP targets!
```

### Example 5: Entering Container Network Namespace via Host `nsenter`
Inspecting network state directly from the Linux host terminal without launching a container.

```bash
# 1. Extract container process ID (PID)
PID=$(docker inspect --format '{{.State.Pid}}' web-container)

# 2. Execute 'ip a' inside that container's network namespace
sudo nsenter -t $PID -n ip a

# 3. View socket connections inside the container namespace
sudo nsenter -t $PID -n ss -tulpn
```

---

## 7. Production Best Practices & Antipatterns

### Best Practices
- **Use Ephemeral Sidecars (Netshoot) for Debugging**: Never install diagnostic packages (`curl`, `net-tools`, `gdb`) permanently into production Dockerfiles; use `docker run --net=container:<target> nicolaka/netshoot`.
- **Verify Host IP Forwarding First**: If containers cannot reach the internet, the first check on Linux must always be `sysctl net.ipv4.ip_forward`.
- **Use Packet Captures (`tcpdump`) for Flaky Timeouts**: Intermittent connection resets are almost always revealed by TCP RST packets in a 30-second `tcpdump`.
- **Check DNS Resolver Latency**: Verify whether queries to `127.0.0.11` are taking > 5ms, indicating socket backlog or thread starvation.

### Anti-Patterns
- **Disabling Iptables or UFW blindly**: Flushing iptables (`iptables -F`) flushes Docker's custom routing chains, completely breaking all container port publishing across the host.
- **Assuming ICMP Ping Equals Service Health**: Many cloud routers and firewalls drop ICMP echo requests (ping) while permitting TCP traffic. Always test with `nc -zv` or `curl`, not `ping`.
- **Leaving Debug Containers Running**: Always pass `--rm` when running troubleshooting containers like Netshoot to avoid cluttering disk and memory.

---

## 8. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| `nc: connect to db port 5432: Connection refused` | Service inside target container is listening on `127.0.0.1` instead of `0.0.0.0` | In target: `ss -tulpn` shows `127.0.0.1:5432` | Configure service to bind to `0.0.0.0`. |
| `ping: bad address 'api'` | Containers reside on different Docker bridge networks | `docker inspect <c1> <c2>` network names | Connect both containers to a common user-defined network. |
| Container can ping public IPs (`8.8.8.8`) but cannot resolve domain names | Host upstream DNS resolver blocked or misconfigured in `/etc/resolv.conf` | `docker run --rm alpine nslookup google.com 8.8.8.8` | Configure valid upstream DNS in `/etc/docker/daemon.json` (`"dns": ["1.1.1.1"]`). |
| Inbound connections hang indefinitely (SYN sent, no ACK) | Host firewall (UFW / iptables) dropping packets in `DOCKER-USER` or `FORWARD` | `sudo iptables -L FORWARD -v -n` | Add explicit `ACCEPT` rule in `DOCKER-USER` chain. |
| `tcpdump: eth0: That device is not up` | Container has stopped or interface has crashed | `docker ps -a` | Restart container and inspect logs for process exit. |
| Port mapping works from external machines but fails from localhost on host | Hairpin NAT broken or `userland-proxy` disabled without hairpin sysctl | `curl http://localhost:<port>` on host | Enable `userland-proxy: true` or set `sysctl net.ipv4.conf.all.route_localnet=1`. |

---

## 9. Performance Tuning & Resource Optimization

1. **TCP Window Sizing & Buffer Tuning**:
   For multi-gigabit container-to-container transfers across Docker bridges, tune Linux host TCP buffers:
   ```bash
   sysctl -w net.core.rmem_max=16777216
   sysctl -w net.core.wmem_max=16777216
   sysctl -w net.ipv4.tcp_rmem="4096 87380 16777216"
   sysctl -w net.ipv4.tcp_wmem="4096 65536 16777216"
   ```
2. **Conntrack Table Sizing**:
   High-concurrency reverse proxies running inside Docker can exhaust the Linux connection tracking table (`nf_conntrack`), leading to dropped packets. Check conntrack usage:
   `sysctl net.netfilter.nf_conntrack_count`
   Increase table limit:
   `sysctl -w net.netfilter.nf_conntrack_max=1048576`

---

## 10. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Unauthorized Network Sniffing via Privileged Sidecar|
+--------------------------------------------------------------+
| Attacker gains access to Docker CLI or Kubernetes API.       |
| Attacker launches Netshoot attached to victim container:     |
| `docker run --net=container:payment-gateway netshoot tcpdump`|
| Attacker captures unencrypted internal credit card traffic!  |
|                                                              |
| MITIGATIONS:                                                 |
| 1. Enforce mTLS (Mutual TLS) between all microservices.      |
| 2. Restrict Docker socket access strictly to root.          |
+--------------------------------------------------------------+
```

---

## 11. Architectural Comparison Table

| Tool | Target Layer | Output Type | Best Used For |
|:---|:---|:---|:---|
| **`ping`** | Layer 3 (ICMP) | Round-trip latency | Basic host reachability (if ICMP allowed) |
| **`nc` (netcat)** | Layer 4 (TCP/UDP) | Exit code / Banner | Port availability & firewall drop detection |
| **`dig` / `nslookup`**| Layer 7 (DNS) | DNS Record sets | Embedded DNS (`127.0.0.11`) resolution audit |
| **`tcpdump`** | Layer 2-7 (Packets)| Raw pcap frames | Deep protocol analysis, TCP handshakes |
| **`mtr` / `traceroute`**| Layer 3 (Routing) | Hop latency breakdown | Locating routing black holes / drops |
| **`ss` / `netstat`** | Layer 4 (Sockets)| Socket binding table | Verifying `0.0.0.0` vs `127.0.0.1` binding |

---

## 12. Real-World Scenario Walkthrough

### Scenario: Diagnosing Random 10-Second API Delays in Microservice Mesh

#### Incident
A microservice cluster exhibited random 10-second latency spikes on 1% of transactions. Application logs simply reported `Connection Timeout to Auth Service`.

#### Diagnosis Steps
1. Attached Netshoot to the calling service:
   ```bash
   docker run --rm -it --net=container:web-service nicolaka/netshoot
   ```
2. Ran continuous TCP socket checks:
   ```bash
   while true; do nc -zv -w 2 auth-service 8080; sleep 0.5; done
   ```
   Connection succeeded 99% of the time in 1ms, but occasionally hung for exactly 2 seconds.
3. Captured packet trace during latency event:
   ```bash
   tcpdump -i eth0 -nn -tt 'host auth-service'
   ```
4. **Packet Trace Analysis**:
   - The trace showed TCP SYN packets sent by `web-service`.
   - No TCP SYN-ACK was returned for 2 seconds.
   - The calling service retransmitted TCP SYN after 1 second (Linux initial RTO timeout).
5. **Root Cause**:
   The Linux host's connection tracking table (`nf_conntrack`) was hitting its maximum capacity (`nf_conntrack_max=65536`), causing the host kernel to silently drop new TCP SYN packets.
6. **Remediation**:
   Increased `nf_conntrack_max` to `524288`. Timeout dropped to 0%.

---

## 13. Self-Assessment & Interview Questions

<details>
<summary><strong>1. How can you troubleshoot network connectivity of a running container that lacks `curl`, `ping`, and `netstat`?</strong></summary>

**Answer:**
Launch an ephemeral diagnostic container (such as `nicolaka/netshoot` or `alpine`) sharing the target container's network namespace using:
`docker run --rm -it --net=container:<target_container> nicolaka/netshoot`
This provides a full suite of networking tools (`curl`, `nc`, `tcpdump`, `dig`) operating directly inside the target's network stack without modifying the original container. Alternatively, on a Linux host, run tools via `nsenter -t <PID> -n <command>`.
</details>

<details>
<summary><strong>2. Why is testing port reachability with `ping` unreliable in container environments?</strong></summary>

**Answer:**
`ping` uses the ICMP protocol (Layer 3), while applications listen on TCP or UDP (Layer 4). A host or container can respond to ICMP ping even if the application service has crashed or is not listening on the target port. Conversely, firewalls or cloud security groups frequently block ICMP ping while allowing TCP traffic through. To test whether a service is reachable, always use TCP probing tools like `nc -zv <host> <port>` or `curl`.
</details>

<details>
<summary><strong>3. What does `Connection refused` indicate compared to a connection timeout?</strong></summary>

**Answer:**
- **`Connection refused`**: A TCP RST (Reset) packet was returned. Network routing is functional, the host was reached, but no application process is listening on the target port (or the service is bound strictly to `127.0.0.1`).
- **Connection Timeout**: No packet was returned at all. Packets were silently dropped by a firewall, routing failed, or the destination IP address does not exist.
</details>

<details>
<summary><strong>4. How can you stream a packet capture from a remote production container directly into Wireshark on your local workstation?</strong></summary>

**Answer:**
Pipe standard output from an ephemeral `tcpdump` sidecar over an SSH session into local Wireshark:
`ssh user@remote "docker run --rm --net=container:<id> nicolaka/netshoot tcpdump -i eth0 -U -w - not port 22" | wireshark -k -i -`
</details>

<details>
<summary><strong>5. What is the significance of the `DOCKER-USER` chain when debugging firewall packet drops?</strong></summary>

**Answer:**
All container-bound forwarding traffic passes through the `DOCKER-USER` iptables chain before Docker's auto-generated `DOCKER` rules are evaluated. If a network packet is being dropped between the host and container, checking the packet counter in `iptables -L DOCKER-USER -n -v` immediately reveals if an administrator rule or firewall software (like UFW) is dropping the packets.
</details>

<details>
<summary><strong>6. How does `nsenter` allow host administrators to inspect container network namespaces?</strong></summary>

**Answer:**
Linux organizes namespaces under `/proc/<PID>/ns/`. The `nsenter` command allows a privileged process to enter one or more namespaces of a target process ID. By running `nsenter -t <PID> -n <cmd>`, the command executes within the target process's network namespace (`-n`), providing direct access to its interfaces, routing tables, and socket bindings from the host.
</details>

<details>
<summary><strong>7. How do you verify whether a container service is listening on all interfaces (`0.0.0.0`) versus loopback (`127.0.0.1`)?</strong></summary>

**Answer:**
Execute `ss -tulpn` or `netstat -tulpn` inside the container namespace. If the `Local Address:Port` column shows `127.0.0.1:<port>`, other containers and host port forwards cannot reach it. It must display `0.0.0.0:<port>` (or `*:<port>`) to accept external network traffic.
</details>

<details>
<summary><strong>8. What is the purpose of `options single-request` in container `/etc/resolv.conf`?</strong></summary>

**Answer:**
It forces the C library resolver to send IPv4 `A` queries and IPv6 `AAAA` queries sequentially on different sockets rather than in parallel over the same socket. This prevents 5-second DNS timeouts caused by intermediate NAT gateways or firewall state tables dropping concurrent UDP DNS replies.
</details>

---

## 14. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Debugging with Netshoot Attached to a Target</strong></summary>

**Goal:** Launch an Nginx container and attach Netshoot to verify listening sockets.

**Step-by-step Solution:**
1. Start Nginx:
   ```bash
   docker run -d --name test-web nginx:alpine
   ```
2. Attach Netshoot:
   ```bash
   docker run --rm -it --net=container:test-web nicolaka/netshoot ss -tulpn
   ```
3. Observe output: port 80 is listening on `0.0.0.0`.
</details>

<details>
<summary><strong>Exercise 2: Capturing Live HTTP Traffic with `tcpdump`</strong></summary>

**Goal:** Sniff HTTP requests entering a container in real time.

**Step-by-step Solution:**
1. In Terminal 1, attach Netshoot with tcpdump:
   ```bash
   docker run --rm -it --net=container:test-web nicolaka/netshoot tcpdump -i eth0 -nn -A 'tcp port 80'
   ```
2. In Terminal 2, send HTTP request:
   ```bash
   docker run --rm --network bridge alpine wget -qO- http://$(docker inspect test-web --format '{{.NetworkSettings.IPAddress}}')
   ```
3. Terminal 1 displays the raw HTTP GET request and response headers!
</details>

<details>
<summary><strong>Exercise 3: Diagnosing a Closed Port with `nc`</strong></summary>

**Goal:** Use netcat to distinguish between a listening port and a closed port.

**Step-by-step Solution:**
1. Test open port 80:
   ```bash
   docker run --rm --net=container:test-web nicolaka/netshoot nc -zv 127.0.0.1 80
   # 127.0.0.1 (127.0.0.1:80) open
   ```
2. Test non-listening port 81:
   ```bash
   docker run --rm --net=container:test-web nicolaka/netshoot nc -zv 127.0.0.1 81
   # (Exits with connection refused)
   ```
</details>

<details>
<summary><strong>Exercise 4: Inspecting Embedded DNS Query Metrics with Dig</strong></summary>

**Goal:** Benchmark DNS query latency against `127.0.0.11`.

**Step-by-step Solution:**
1. Create user-defined network:
   ```bash
   docker network create diag-net
   ```
2. Run query:
   ```bash
   docker run --rm --network diag-net nicolaka/netshoot dig @127.0.0.11 google.com
   ```
3. Note Query Time in msec.
</details>

<details>
<summary><strong>Exercise 5: Inspecting Network Namespace via Host `nsenter`</strong></summary>

**Goal:** Retrieve interface details of a container using host `nsenter`.

**Step-by-step Solution:**
1. Extract PID:
   ```bash
   PID=$(docker inspect test-web --format '{{.State.Pid}}')
   ```
2. (Linux Host with root):
   ```bash
   sudo nsenter -t $PID -n ip addr show
   ```
</details>

<details>
<summary><strong>Exercise 6: Simulating Packet Drops with Custom Iptables Rule</strong></summary>

**Goal:** Drop packets in `DOCKER-USER` and observe connection timeout with `nc`.

**Step-by-step Solution:**
1. Drop port 9999 in `DOCKER-USER`:
   ```bash
   sudo iptables -I DOCKER-USER -p tcp --dport 9999 -j DROP
   ```
2. Test connection with `nc -zv -w 2 <ip> 9999`.
3. Observe immediate timeout. Clean up: `sudo iptables -D DOCKER-USER -p tcp --dport 9999 -j DROP`.
</details>

---

## 15. Summary & Reference Cheat Sheet

### Troubleshooting Swiss Army Knife
```bash
# Attach Diagnostic Container:
docker run --rm -it --net=container:<target_id> nicolaka/netshoot

# Top Diagnostic One-Liners:
ss -tulpn                           # Check listening ports
nc -zv -w 2 <host> <port>           # Check TCP socket
dig @127.0.0.11 <name>              # Check Docker DNS
tcpdump -i eth0 -nn -s0 -A          # Sniff raw traffic
iptables -L DOCKER-USER -n -v       # Check firewall drops
```

### 4 Golden Diagnostic Rules
1. **Never guess**: Use `tcpdump` to observe reality at the packet level.
2. **Never install debug tools in production images**: Use Netshoot as a sidecar.
3. **Always verify `0.0.0.0` binding**: If a service listens on `127.0.0.1`, it cannot accept external container traffic.
4. **Never run `iptables -F`**: Flushing iptables destroys Docker's routing topology.

---

[← Previous: 08-ipv6-and-proxies.md](08-ipv6-and-proxies.md) | [Next: Module 05 Docker Compose →](../05-compose/README.md)
