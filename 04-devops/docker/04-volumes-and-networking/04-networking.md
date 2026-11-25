# Docker Networking Architecture: Drivers, Namespaces, and Packet Routing

```
Track: DevOps & Containerization
Module: 04-volumes-and-networking
Prerequisites: 01-fundamentals/03-docker-architecture.md, 01-fundamentals/05-how-isolation-works.md
Difficulty: Intermediate to Advanced
Estimated Duration: 50 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Network Drivers Deep Dive](#4-network-drivers-deep-dive)
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

By default, every Docker container executes within an isolated network sandbox. It has its own private loopback interface (`lo`), virtual Ethernet adapter (`eth0`), private routing table, and ARP cache.

Understanding Docker networking is essential because microservices must communicate across multiple topologies:
- Container-to-container communication within a single host.
- Host-to-container port mapping for public internet traffic.
- Container-to-external world outbound requests (APIs, package downloads).
- Multi-host cross-cluster communication across distinct physical servers.

Docker provides the **Container Network Model (CNM)** via `libnetwork`, giving developers fine-grained control over driver implementations ranging from lightweight virtual software bridges to direct hardware L2 MAC address assignment.

---

## 2. Core Concepts

### The 5 Native Network Drivers
1. **`bridge`**: The default driver. Creates a virtual software switch (`docker0` or `br-<network-id>`) on the host. Containers receive private IPs (e.g., `172.17.0.0/16`) and communicate using NAT (Network Address Translation).
2. **`host`**: Completely disables network isolation. The container shares the host's network namespace directly (`CLONE_NEWNET` is skipped). Yields maximum raw network throughput, but port collisions can occur with host services. (Linux only).
3. **`none`**: Disables all networking. The container gets only an isolated loopback interface (`127.0.0.1`). Used for batch cryptographic processing, batch analytics, or air-gapped jobs.
4. **`overlay`**: Enables multi-host networking across multiple Docker daemons (Docker Swarm or Consul-backed clusters) using VXLAN tunnels.
5. **`macvlan` / `ipvlan`**: Assigns a real physical MAC or IP address directly to the container, making it appear as a physical hardware device on the enterprise LAN.

### The Default Bridge vs User-Defined Bridge
| Feature | Default `bridge` (`docker0`) | User-Defined Bridge (`my-net`) |
|:---|:---|:---|
| **Automatic DNS Resolution** | **NO** (Must use legacy `--link` or IP) | **YES** (Resolves container name via `127.0.0.11`) |
| **Port Exposure Between Containers**| All ports exposed to all default containers | Isolated: Only containers on the same network can talk |
| **Dynamic Attach/Detach** | Cannot disconnect while running | Can attach/detach on the fly (`docker network connect`) |
| **Environment Variables Sharing** | Shared only via `--link` | No unintended leak; strict DNS only |

---

## 3. How It Works Internally

### Virtual Ethernet (`veth`) Pairs and Bridge Architecture

When Docker creates a container on a bridge network, the Linux kernel creates a **Virtual Ethernet pair (`veth`)**:
- One end of the pair stays in the host network namespace attached to the software bridge switch (named `vethxxxx`).
- The other end is moved into the container's private network namespace and renamed `eth0`.

```
+-------------------------------------------------------------------------+
|                               HOST MACHINE                              |
|                                                                         |
|  Physical Interface: eth0 (IP: 192.168.1.50)                            |
|                                                                         |
|  +-------------------------------------------------------------------+  |
|  | Linux Bridge (Software Switch): docker0 (IP: 172.17.0.1)          |  |
|  +-------------------+---------------------------+-------------------+  |
|                      |                           |                      |
|                 veth1a2b3                   veth4c5d6                   |
|                      |                           |                      |
+----------------------|---------------------------|----------------------+
                       | (Peer)                    | (Peer)
                       v                           v
+-----------------------------+             +-----------------------------+
|    CONTAINER A NAMESPACE    |             |    CONTAINER B NAMESPACE    |
|                             |             |                             |
| Interface: eth0             |             | Interface: eth0             |
| IP: 172.17.0.2              |             | IP: 172.17.0.3              |
| Gateway: 172.17.0.1         |             | Gateway: 172.17.0.1         |
+-----------------------------+             +-----------------------------+
```

### Packet Routing & Netfilter (iptables)
1. **Outbound Traffic (Container -> Internet)**:
   A container sends packets to its gateway (`172.17.0.1`). The host kernel matches an iptables `MASQUERADE` rule:
   `-A POSTROUTING -s 172.17.0.0/16 ! -o docker0 -j MASQUERADE`
   The packet's source IP is dynamically rewritten to the host's public IP (`SNAT`).
2. **Inbound Traffic (Host Port Mapping `-p 8080:80`)**:
   Incoming traffic hitting host port 8080 is intercepted by iptables in the `PREROUTING` chain:
   `-A DOCKER -p tcp -m tcp --dport 8080 -j DNAT --to-destination 172.17.0.2:80`
   The destination IP is translated to the container's private internal IP (`DNAT`).

---

## 4. Network Drivers Deep Dive

### Host Network Driver (`--network host`)
```
[Host eth0: 192.168.1.100] <==== DIRECT BINDING ====> [Container Application]
```
- No virtual ethernet interfaces, no bridge, no iptables NAT.
- Binds directly to the host's network interfaces.
- Running `nginx` on port 80 inside container binds port 80 directly on the host machine.
- 0% CPU latency overhead for high-frequency trading or multi-gigabit streaming.

### Macvlan Driver (`--network macvlan`)
- Grants the container its own distinct MAC address on the physical router.
- Appears to the network switch as a separate physical computer requesting its own DHCP lease.
- Bypasses host NAT completely.
- Caveat: Host cannot communicate directly with container over the macvlan interface without creating a dedicated macvlan sub-interface on the host.

---

## 5. Syntax & Command Reference

### Network Lifecycle Commands

```bash
# List all networks
docker network ls

# Create a user-defined bridge network
docker network create --driver bridge my-app-net

# Create a network with custom subnet and gateway
docker network create \
  --driver bridge \
  --subnet 10.20.0.0/16 \
  --gateway 10.20.0.1 \
  custom-subnet-net

# Inspect network topology, connected containers, and assigned IPs
docker network inspect my-app-net

# Connect a running container to an existing network
docker network connect my-app-net my-running-container

# Disconnect a container from a network
docker network disconnect my-app-net my-running-container

# Remove an unused network
docker network rm my-app-net

# Prune all unreferenced networks
docker network prune -f
```

---

## 6. Practical Implementation & Examples

### Example 1: Multi-Container Communication via Embedded DNS
Connecting a web API to a PostgreSQL database using automatic DNS service discovery on a user-defined bridge network.

```bash
# 1. Create user-defined network
docker network create internal-tier

# 2. Launch database container attached to network
docker run -d \
  --name database \
  --network internal-tier \
  -e POSTGRES_PASSWORD=secret \
  postgres:17-alpine

# 3. Launch application container on the same network
# Notice: No -p port mapping needed! Internal traffic flows directly on port 5432!
docker run -it --rm \
  --network internal-tier \
  alpine:3.21 \
  sh -c "nc -zv database 5432"

# Output: database (172.18.0.2:5432) open!
```

### Example 2: Host Network Driver for Zero-Overhead Throughput
Benchmarking maximum bandwidth using host network mode.

```bash
docker run -d \
  --name high-throughput-nginx \
  --network host \
  nginx:alpine

# Nginx is immediately reachable directly on http://<host-ip>:80 without -p flag!
curl http://localhost:80
```

### Example 3: Dual-Homed Container (Network DMZ Architecture)
A reverse proxy container connected to both an external public network and an internal private database network, ensuring the database is physically isolated from the internet.

```bash
# 1. Create two isolated networks
docker network create dmz-net
docker network create backend-net

# 2. Database attached ONLY to backend-net (Completely unreachable from DMZ or host)
docker run -d --name secure-db --network backend-net postgres:17-alpine

# 3. API gateway attached to dmz-net
docker run -d --name api-gateway --network dmz-net -p 80:80 nginx:alpine

# 4. Attach API gateway to backend-net (Dual-homed)
docker network connect backend-net api-gateway

# Result: api-gateway can reach both dmz and secure-db; secure-db cannot reach internet!
```

### Example 4: Complete Air-Gapped Container with `none` Driver
Running a sensitive key generation container with zero network capability.

```bash
docker run --rm -it \
  --network none \
  alpine:3.21 \
  sh -c "ip addr && ping -c 1 8.8.8.8 || echo 'Air-gap confirmed: No outbound network!'"
```

### Example 5: Macvlan Deployment for Legacy LAN Integration
Assigning a static IP on the physical local router network (`192.168.1.0/24`).

```bash
docker network create -d macvlan \
  --subnet=192.168.1.0/24 \
  --gateway=192.168.1.1 \
  -o parent=eth0 \
  lan-direct-net

docker run -d \
  --name static-lan-node \
  --net=lan-direct-net \
  --ip=192.168.1.220 \
  nginx:alpine
```

---

## 7. Production Best Practices & Antipatterns

### Best Practices
- **Always Create User-Defined Networks**: Never deploy multi-container systems to the default `bridge` network. User-defined networks provide automatic DNS, scoped security isolation, and hot-plugging.
- **Isolate Tiers with Network Segregation**: Frontends should never share a network with databases. Place reverse proxies and APIs on a `frontend` network; place APIs and databases on a `backend` network.
- **Do Not Publish Internal Ports (`-p`) Unnecessarily**: If a database only speaks to a backend API on the same Docker network, do **not** publish its port (`-p 5432:5432`). Publishing exposes the port to the public host IP.
- **Use `--network host` Only When Necessary**: Host networking circumvents container port isolation and limits portability to Linux hosts.

### Anti-Patterns
- **Using Legacy `--link`**: The `--link` flag was deprecated in Docker 1.10. It hardcodes IP addresses into `/etc/hosts` and does not handle container restarts dynamically.
- **Hardcoding Container IP Addresses**: Never configure application connection strings with static IPs (e.g., `DATABASE_URL=172.17.0.2:5432`). Container IPs change on restart; use DNS names (`DATABASE_URL=database:5432`).
- **Relying on Default Bridge for Inter-Container Communication**: Containers on `docker0` cannot resolve each other by container name.

---

## 8. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| `getaddrinfo ENOTFOUND db` inside container | Containers are on the default `bridge` network instead of a user-defined network | `docker inspect <container> --format '{{json .NetworkSettings.Networks}}'` | Create user-defined network (`docker network create`) and attach both containers. |
| Container cannot access the internet | Host IP forwarding is disabled in Linux kernel (`net.ipv4.ip_forward=0`) | `sysctl net.ipv4.ip_forward` | Run `sysctl -w net.ipv4.ip_forward=1` and set in `/etc/sysctl.conf`. |
| Port published with `-p 8080:80` is accessible from the internet despite UFW blocking it | Docker bypasses UFW by manipulating iptables directly before UFW rules execute | `iptables -L DOCKER -n` | Bind explicitly to localhost `-p 127.0.0.1:8080:80` or use `ufw-docker` package. |
| `docker run --network host` still cannot bind ports on macOS/Windows | Docker Desktop on macOS/Windows runs inside a lightweight Linux VM; `--network host` binds to VM, not Mac/Windows | Verify OS environment | Port publishing (`-p`) is required on macOS and Windows Docker Desktop. |
| Two containers cannot communicate on same user-defined network | Subnet overlapping with host physical LAN or conflicting firewall rule | `docker network inspect <net>` | Create network with distinct non-overlapping `--subnet` (e.g., `10.200.0.0/16`). |
| Macvlan container cannot ping the Docker host | Linux kernel security feature prevents host-to-macvlan communication via parent interface | `ping <host-ip>` from macvlan container | Create a macvlan bridge sub-interface on the host itself. |

---

## 9. Performance Tuning & Resource Optimization

1. **Host Network Driver for High Packet Per Second (PPS) Workloads**:
   NAT translation via iptables adds ~5-15 microseconds of packet processing latency and consumes CPU cycles for connection tracking (`nf_conntrack`). For DNS servers, video streaming (WebRTC), or trading applications, `--network host` achieves 100% bare-metal performance.
2. **MTU (Maximum Transmission Unit) Tuning**:
   If containers communicate over VPN tunnels, overlay networks, or cloud VPCs with custom MTU (e.g., AWS Jumbo Frames MTU 9001), ensure Docker networks match the host MTU to prevent packet fragmentation:
   ```bash
   docker network create --opt com.docker.network.driver.mtu=9001 custom-mtu-net
   ```

---

## 10. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Cross-Container Lateral Movement on Default Bridge   |
+--------------------------------------------------------------+
| Attacker compromises Vulnerable Web Container A.             |
| Container A is on default `bridge` (docker0).                |
| Container B (Internal Payroll DB) is also on `docker0`.      |
| Attacker runs port scan on 172.17.0.0/16 subnet.             |
| Attacker finds Container B, exploits unauthenticated port!   |
|                                                              |
| MITIGATION: USER-DEFINED NETWORK ISOLATION (ZERO LATERAL ACCESS)|
+--------------------------------------------------------------+
```

### The Docker vs UFW Firewall Trap
On Linux hosts with UFW (Uncomplicated Firewall) enabled:
- UFW rules typically block incoming public traffic on ports like `8080`.
- When Docker publishes a port via `-p 8080:80`, Docker injects `PREROUTING` rules directly into the Linux `iptables` table before UFW's filter chain evaluates.
- **Result: The port is exposed to the entire public internet, bypassing UFW!**
- **Fix**: Always bind explicitly to localhost for private services:
  `docker run -p 127.0.0.1:8080:80 ...`

---

## 11. Architectural Comparison Table

| Driver | Isolation Level | Latency Overhead | Multi-Host Capable? | DNS Auto-Discovery? |
|:---|:---|:---|:---|:---|
| **Default Bridge** | Low (Shared subnet) | Low (iptables NAT) | No | No |
| **User-Defined Bridge**| **High (Tiered subnets)**| Low (iptables NAT) | No | **Yes (127.0.0.11)** |
| **Host** | **Zero Isolation** | **Zero Overhead (Native)**| No | No (Uses Host DNS) |
| **None** | **Maximum (Air-gapped)**| N/A | N/A | N/A |
| **Overlay** | High (Virtual VXLAN) | Moderate (Encapsulation)| **Yes** | **Yes** |
| **Macvlan** | High (Hardware L2) | **Zero (Native L2)** | Yes (Same LAN) | No (External DNS) |

---

## 12. Real-World Scenario Walkthrough

### Scenario: Resolving Intermittent DNS Timeouts in a High-Scale API

#### Symptoms
An e-commerce API processing 15,000 requests/sec started throwing intermittent `504 Gateway Timeout` errors. Application logs reported:
`Error: queryA ECONNREFUSED redis-master:6379`.

#### Investigation
The application and Redis were deployed on a user-defined Docker bridge network. Under high concurrency, Docker's embedded DNS server (`127.0.0.11:53`) was bombarded with 30,000 UDP DNS queries per second because the Node.js HTTP client was not caching DNS responses. The Linux kernel dropped UDP socket packets due to socket buffer exhaustion.

#### Solution
1. Configured connection pooling and in-process DNS caching in the Node.js application (`lookup: dnscache({ enable: true })`).
2. Configured custom upstream DNS servers in Docker daemon `/etc/docker/daemon.json`:
   ```json
   {
     "dns": ["10.0.0.2", "1.1.1.1"]
   }
   ```
3. Docker DNS lookup drop rate decreased to 0%.

---

## 13. Self-Assessment & Interview Questions

<details>
<summary><strong>1. Why can containers resolve each other by name on a user-defined bridge network, but not on the default `bridge` network?</strong></summary>

**Answer:**
Docker runs an embedded DNS server at `127.0.0.11` inside container network namespaces attached to user-defined networks. When an application queries a container name, Docker's internal DNS resolver intercepts the query and returns the target container's current IP address. On the default `bridge` network (`docker0`), this embedded DNS resolver is disabled for backwards compatibility; it relies on static `/etc/hosts` entries which do not update automatically.
</details>

<details>
<summary><strong>2. Explain how Docker's port publishing (`-p 8080:80`) works at the Linux kernel level.</strong></summary>

**Answer:**
Docker uses the Linux `netfilter` subsystem via `iptables`. When `-p 8080:80` is used, Docker adds a Destination Network Address Translation (`DNAT`) rule in the `PREROUTING` chain of the `nat` table. Incoming packets targeting host port 8080 have their destination IP rewritten to the container's private virtual IP (e.g., `172.18.0.2`) and destination port to `80`. A user-space proxy (`docker-proxy`) is also spawned as a fallback for loopback routing.
</details>

<details>
<summary><strong>3. What is a `veth` pair, and what role does it play in Docker container networking?</strong></summary>

**Answer:**
A `veth` (Virtual Ethernet) pair is a Linux kernel networking construct acting like a bi-directional virtual patch cable. Packets sent into one peer interface emerge immediately from the other. Docker places one end (`vethxxxx`) in the host's root network namespace attached to a software bridge (`docker0` or `br-<id>`), and places the other end inside the container's private network namespace, renamed as `eth0`.
</details>

<details>
<summary><strong>4. Why should you avoid publishing ports using `-p 5432:5432` for internal backend databases?</strong></summary>

**Answer:**
Publishing a port exposes that port on all host network interfaces (`0.0.0.0`), making the database directly reachable from external networks and the public internet. If other backend services run in containers on the same Docker host, they can reach the database directly across a user-defined Docker bridge network using its container name (`db:5432`) without publishing any host ports.
</details>

<details>
<summary><strong>5. What is the fundamental difference between `--network host` and a bridge network?</strong></summary>

**Answer:**
Under a bridge network, the container runs in an isolated network namespace (`CLONE_NEWNET`) with its own IP address, virtual network device (`eth0`), routing table, and iptables NAT. Under `--network host`, network isolation is completely disabled: the container shares the host's network namespace, binds directly to host IP addresses and ports, avoids all NAT overhead, but shares the host's port namespace (causing conflicts if two containers use the same port).
</details>

<details>
<summary><strong>6. How can an organization prevent Docker from bypassing host UFW firewall rules?</strong></summary>

**Answer:**
Either:
1. Always bind published ports explicitly to localhost: `-p 127.0.0.1:8080:80`.
2. Use tools like `ufw-docker` which insert custom iptables rules into the `DOCKER-USER` chain (which Docker evaluates before its own `DOCKER` chain rules).
3. Set `"iptables": false` in `/etc/docker/daemon.json` (though this disables automatic container port routing and requires manual rule management).
</details>

<details>
<summary><strong>7. What happens when a container is connected to two user-defined networks simultaneously?</strong></summary>

**Answer:**
The container receives two virtual network interfaces (`eth0` and `eth1`), each with an IP address from the respective network's subnet. It can resolve and communicate with containers on both networks via Docker's embedded DNS, effectively functioning as a network router or DMZ gateway between the two isolated networks.
</details>

<details>
<summary><strong>8. What is the `DOCKER-USER` iptables chain used for?</strong></summary>

**Answer:**
Docker routes all container forwarding traffic through the `DOCKER-USER` chain before processing its own auto-generated rules in the `DOCKER` chain. System administrators can insert custom security and firewall rules into `DOCKER-USER` to filter or block container traffic without their rules being overwritten whenever Docker restarts or reloads.
</details>

---

## 14. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Validating Embedded DNS Resolution on User-Defined Network</strong></summary>

**Goal:** Create a user-defined network, launch two containers, and test DNS ping by name.

**Step-by-step Solution:**
1. Create network:
   ```bash
   docker network create lab-dns-net
   ```
2. Run target container:
   ```bash
   docker run -d --name target-node --network lab-dns-net alpine:3.21 sleep 3600
   ```
3. Run query container:
   ```bash
   docker run --rm --network lab-dns-net alpine:3.21 ping -c 2 target-node
   # Ping succeeds: 2 packets transmitted, 2 packets received!
   ```
</details>

<details>
<summary><strong>Exercise 2: Demonstrating DNS Failure on Default Bridge</strong></summary>

**Goal:** Launch two containers on the default bridge and verify name resolution fails.

**Step-by-step Solution:**
1. Launch target on default bridge:
   ```bash
   docker run -d --name default-target alpine:3.21 sleep 3600
   ```
2. Attempt ping by name:
   ```bash
   docker run --rm alpine:3.21 ping -c 2 default-target
   # Output: ping: bad address 'default-target'
   ```
</details>

<details>
<summary><strong>Exercise 3: Inspecting Docker's Embedded DNS Server (`127.0.0.11`)</strong></summary>

**Goal:** Inspect `/etc/resolv.conf` inside a container on a user-defined network.

**Step-by-step Solution:**
1. Run inspection:
   ```bash
   docker run --rm --network lab-dns-net alpine:3.21 cat /etc/resolv.conf
   ```
2. Verify nameserver points to:
   ```
   nameserver 127.0.0.11
   options ndots:0
   ```
</details>

<details>
<summary><strong>Exercise 4: Creating a DMZ Multi-Homed Routing Topology</strong></summary>

**Goal:** Build two isolated networks and a bridge container that connects both.

**Step-by-step Solution:**
1. Create networks:
   ```bash
   docker network create net-frontend
   docker network create net-backend
   ```
2. Launch isolated backend:
   ```bash
   docker run -d --name backend-service --network net-backend alpine sleep 3600
   ```
3. Launch frontend:
   ```bash
   docker run -d --name frontend-service --network net-frontend alpine sleep 3600
   ```
4. Verify frontend CANNOT ping backend:
   ```bash
   docker exec frontend-service ping -c 1 backend-service || echo "Isolated!"
   ```
5. Connect frontend to backend network:
   ```bash
   docker network connect net-backend frontend-service
   ```
6. Verify frontend can now ping backend:
   ```bash
   docker exec frontend-service ping -c 1 backend-service
   ```
</details>

<details>
<summary><strong>Exercise 5: Inspecting Host Linux Veth Interfaces via `ip link`</strong></summary>

**Goal:** Trace a container's `eth0` interface index to its corresponding `veth` peer on the Linux host.

**Step-by-step Solution:**
1. Run container:
   ```bash
   CID=$(docker run -d alpine sleep 3600)
   ```
2. Retrieve interface peer index from container:
   ```bash
   docker exec $CID cat /sys/class/net/eth0/iflink
   # Outputs index number, e.g., 42
   ```
3. On host:
   ```bash
   ip link | grep "^42:"
   # Shows matching host interface: 42: vethxxxx@if41
   ```
</details>

<details>
<summary><strong>Exercise 6: Inspecting Iptables NAT Rules Injected by Docker</strong></summary>

**Goal:** Launch a container with `-p 8888:80` and find the exact DNAT rule in iptables.

**Step-by-step Solution:**
1. Run Nginx:
   ```bash
   docker run -d --name test-nat -p 8888:80 nginx:alpine
   ```
2. Inspect iptables NAT table:
   ```bash
   sudo iptables -t nat -L DOCKER -n -v
   ```
3. Observe rule: `tcp dpt:8888 to:172.17.0.x:80`.
</details>

---

## 15. Summary & Reference Cheat Sheet

### Essential Network Commands
```bash
docker network ls
docker network create --driver bridge <name>
docker network inspect <name>
docker network connect <network> <container>
docker network disconnect <network> <container>
docker network prune -f
```

### Golden Rules
1. **Never use the default bridge (`docker0`)** for production multi-container apps.
2. **Never publish database ports (`-p`)** if only backend containers need access.
3. **Always bind to `127.0.0.1`** (`-p 127.0.0.1:8080:80`) if ports should not be publicly accessible.
4. **Use `--network host`** only when native packet performance is required on Linux.

---

[← Previous: 03-backup-and-restore-of-volumes.md](03-backup-and-restore-of-volumes.md) | [Next: 05-user-defined-networks-and-dns.md →](05-user-defined-networks-and-dns.md)
