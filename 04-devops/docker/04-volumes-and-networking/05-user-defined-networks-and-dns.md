# User-Defined Networks and Embedded DNS Resolution

```
Track: DevOps & Containerization
Module: 04-volumes-and-networking
Prerequisites: 04-volumes-and-networking/04-networking.md
Difficulty: Intermediate
Estimated Duration: 45 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & IPAM Configuration Reference](#4-syntax--ipam-configuration-reference)
5. [Practical Implementation & Examples](#5-practical-implementation--examples)
6. [Production Best Practices & Antipatterns](#6-production-best-practices--antipatterns)
7. [Common Pitfalls & Troubleshooting](#7-common-pitfalls--troubleshooting)
8. [Performance Tuning & Resource Optimization](#8-performance-tuning--resource-optimization)
9. [Security Hardening & Threat Model](#9-security-hardening--threat-model)
10. [Architectural Comparison Table](#10-architectural-comparison-table)
11. [Real-World Scenario Walkthrough](#11-real-world-scenario-walkthrough)
12. [Self-Assessment & Interview Questions](#12-self-assessment--interview-questions)
13. [Hands-on Practice Exercises](#13-hands-on-practice-exercises)
14. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

In distributed container systems, static IP addressing is an anti-pattern. As containers scale up, fail over, restart, or update across continuous deployment cycles, their internal IP addresses fluctuate unpredictably.

Docker's **User-Defined Networks** solve this fundamental coordination problem by introducing:
- **Embedded DNS Discovery**: A built-in DNS server (`127.0.0.11`) running inside each container's network namespace that maps container names and service aliases directly to their real-time dynamic IP addresses.
- **Internal DNS Round-Robin Load Balancing**: Registering multiple containers under the same network alias automatically distributes requests across instances without deploying an external load balancer.
- **Custom IP Address Management (IPAM)**: Full deterministic control over subnets, CIDR blocks, gateways, and reserved IP ranges to avoid enterprise corporate VPN/VPC conflicts.
- **Segmented Security Domains**: Strict Layer 3/4 network isolation between microservice tiers (e.g., public frontend vs confidential payment databases).

---

## 2. Core Concepts

### The Embedded DNS Resolver (`127.0.0.11`)
When a container connects to a user-defined network, Docker configures `/etc/resolv.conf` to point exclusively to `nameserver 127.0.0.11`.
- Any query for a container name or network alias is intercepted and resolved locally by Docker's internal DNS daemon.
- Any external query (e.g., `api.stripe.com`, `github.com`) is seamlessly forwarded upstream to the host's configured DNS resolvers (`8.8.8.8`, `1.1.1.1`, or corporate DNS).

### Network Aliases (`--network-alias`)
A container can possess multiple DNS names within a network:
1. Its container name (e.g., `web-service-1`).
2. Its container hostname (e.g., `api.corp.local`).
3. One or more **network aliases** (e.g., `web-service`). If three containers join the network with `--network-alias web-service`, Docker's DNS returns all three IPs in round-robin order, providing native client-side load balancing!

---

## 3. How It Works Internally

### DNS Resolution Flow Architecture

```
+-------------------------------------------------------------------------+
|                          CONTAINER NAMESPACE                            |
|                                                                         |
|  Application HTTP Request -> http://payment-service:8080                |
|  Resolver queries nameserver: 127.0.0.11:53                             |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  DOCKER EMBEDDED DNS DAEMON (libnetwork)                |
|                                                                         |
|  1. Is "payment-service" a container or alias on this Docker network?   |
|     +---> YES: Return Container IP (e.g. 172.24.0.5)                   |
|     |                                                                   |
|     +---> NO: Forward query to Host DNS (/etc/resolv.conf)              |
|               e.g., 8.8.8.8 / Corporate CoreDNS                         |
+-------------------------------------------------------------------------+
```

### DNS Round-Robin Mechanics
When three backend replicas share the network alias `api`:
- Query 1 returns: `[172.20.0.2, 172.20.0.3, 172.20.0.4]`
- Query 2 returns: `[172.20.0.3, 172.20.0.4, 172.20.0.2]`
- Query 3 returns: `[172.20.0.4, 172.20.0.2, 172.20.0.3]`

Clients connecting to the first IP in the list distribute incoming traffic evenly across the fleet.

---

## 4. Syntax & IPAM Configuration Reference

### Advanced Network Creation with Custom IPAM

```bash
docker network create \
  --driver bridge \
  --subnet 10.50.0.0/16 \
  --ip-range 10.50.10.0/24 \
  --gateway 10.50.0.1 \
  --aux-address "reserved1=10.50.10.2" \
  --opt com.docker.network.bridge.name=br-production \
  --opt com.docker.network.bridge.enable_icc=true \
  production-vpc
```

### Options Breakdown:
- `--subnet`: CIDR block representing the total addressable network space.
- `--ip-range`: Smaller slice of the subnet allocated dynamically to new containers.
- `--gateway`: The IPv4 address of the bridge interface on the host.
- `--aux-address`: Reserves specific IPs so Docker never dynamically assigns them to containers.
- `com.docker.network.bridge.name`: Sets the exact name of the Linux network interface on the host (e.g., `br-production` instead of random `br-7a3b4...`).
- `enable_icc=false`: Disables inter-container communication on the same bridge (containers cannot talk to each other without explicit port publishing).

---

## 5. Practical Implementation & Examples

### Example 1: Multi-Tier Microservice Architecture with Network Isolation
Segregating frontend, application, and database tiers so the public frontend has zero direct network connectivity to the database.

```bash
# 1. Create two isolated user-defined networks
docker network create public-tier
docker network create private-tier

# 2. Database attached ONLY to private-tier
docker run -d \
  --name order-database \
  --network private-tier \
  -e POSTGRES_PASSWORD=secret \
  postgres:17-alpine

# 3. Backend API attached to BOTH networks (Dual-Homed)
docker run -d \
  --name order-api \
  --network private-tier \
  alpine:3.21 sleep 3600

docker network connect public-tier order-api

# 4. Frontend Reverse Proxy attached ONLY to public-tier
docker run -d \
  --name public-proxy \
  --network public-tier \
  -p 80:80 \
  nginx:alpine

# 5. Verification:
# public-proxy can reach order-api by name:
docker exec public-proxy ping -c 1 order-api # SUCCESS

# public-proxy CANNOT reach order-database by name or IP:
docker exec public-proxy ping -c 1 order-database # FAILS (Isolated!)
```

### Example 2: Client-Side DNS Round-Robin Load Balancing
Deploying multiple worker containers sharing an identical network alias.

```bash
# 1. Create network
docker network create cluster-net

# 2. Launch 3 workers with identical alias: search-engine
docker run -d --name worker-1 --network cluster-net --network-alias search-engine alpine sleep 3600
docker run -d --name worker-2 --network cluster-net --network-alias search-engine alpine sleep 3600
docker run -d --name worker-3 --network cluster-net --network-alias search-engine alpine sleep 3600

# 3. Query DNS multiple times from a client container
docker run --rm --network cluster-net alpine \
  sh -c "apk add --no-cache bind-tools && nslookup search-engine"

# Output shows all 3 IPs returned simultaneously:
# Address: 172.21.0.2
# Address: 172.21.0.3
# Address: 172.21.0.4
```

### Example 3: Static IP Assignment on a User-Defined Network
Assigning deterministic static IPs when integrating with legacy software requiring IP-whitelisting.

```bash
# 1. Create custom subnet
docker network create --subnet 172.30.0.0/16 static-net

# 2. Assign static IP
docker run -d \
  --name fixed-node \
  --network static-net \
  --ip 172.30.5.100 \
  alpine sleep 3600

# 3. Verify IP
docker inspect fixed-node --format '{{.NetworkSettings.Networks.static_net.IPAddress}}'
# Displays: 172.30.5.100
```

### Example 4: Custom Upstream DNS Configuration per Container
Overriding host DNS resolvers for a specific container while retaining internal container discovery.

```bash
docker run -d \
  --name custom-dns-worker \
  --network cluster-net \
  --dns 1.1.1.1 \
  --dns 8.8.8.8 \
  --dns-search corp.internal \
  alpine sleep 3600
```

### Example 5: Hot Disconnecting and Reconnecting Networks
Dynamically migrating a container from a development network to a staging network without restarting the process.

```bash
docker network create dev-net
docker network create staging-net

docker run -d --name migrator --network dev-net alpine sleep 3600

# Attach to staging
docker network connect staging-net migrator

# Detach from dev
docker network disconnect dev-net migrator
```

---

## 6. Production Best Practices & Antipatterns

### Best Practices
- **Define Explicit Subnets in Corporate Environments**: Never let Docker randomly choose subnets in enterprise settings; Docker might pick `172.17.0.0/16` or `192.168.0.0/16`, colliding with corporate office VPNs or cloud VPC subnets.
- **Use Network Aliases for High Availability**: Decouple client configurations from container names. Use `--network-alias redis-master` or `--network-alias auth-service`.
- **Enforce Tiered Isolation**: Databases and message brokers should never be reachable from public-facing reverse proxy networks.
- **Name Bridge Interfaces Explicitly**: Use `com.docker.network.bridge.name` to make host-level network monitoring (`iftop`, `tcpdump`, `prom-node-exporter`) readable.

### Anti-Patterns
- **Using Hardcoded IP Addresses**: Never configure microservice clients with raw IP addresses; always utilize DNS names.
- **Relying on Single Giant Flat Networks**: Placing 50 microservices on a single flat `/16` network eliminates Layer 3 security controls and risks lateral movement if one container is compromised.
- **Modifying `/etc/hosts` Directly**: Manual edits to `/etc/hosts` inside containers do not persist across container restarts.

---

## 7. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| `nslookup: can't resolve '<name>'` | Containers are attached to different networks | `docker network inspect <net1> <net2>` | Connect containers to a shared network via `docker network connect`. |
| Docker daemon crashes with `address already in use` | Chosen `--subnet` collides with existing physical LAN or another Docker network | `ip route` or `route -n` on host | Choose a non-conflicting CIDR block (e.g. `10.250.0.0/16`). |
| Host cannot ping container IP | Expected behavior: container is inside private Linux bridge namespace | `ip route` on host | Containers on bridge networks must be accessed via published ports (`-p`) or routed via gateway. |
| Container can resolve internal names but cannot reach external internet | Host iptables blocking outbound forward chain or DNS forwarding failed | `docker run --rm alpine ping -c 1 1.1.1.1` | Enable `sysctl net.ipv4.ip_forward=1` and check upstream DNS. |
| DNS resolution hangs for 5 seconds | Alpine `musl libc` issuing parallel A and AAAA (IPv6) queries where IPv6 times out | `tcpdump -i any port 53` | Add `options single-request-reopen` in `/etc/resolv.conf` or disable IPv6. |
| `docker network connect` fails with `endpoint with name already exists` | Orphaned endpoint state after ungraceful daemon crash | `docker network inspect <net>` | Run `docker network disconnect -f <net> <container>`. |

---

## 8. Performance Tuning & Resource Optimization

1. **DNS Caching in Application Runtimes**:
   Languages like Node.js do not natively cache DNS lookups by default, sending a UDP packet to `127.0.0.11` for every single outgoing HTTP request. Under 10,000 req/sec, this floods the kernel UDP buffer. Always configure application-level DNS caching:
   - Java: `-Dnetworkaddress.cache.ttl=60`
   - Node.js: `dnscache({ enable: true, ttl: 300 })`
2. **Alpine Musl DNS Caveat**:
   Alpine Linux uses `musl libc`, which performs concurrent `A` and `AAAA` lookups over the same UDP socket. If the upstream network drops AAAA queries, DNS resolution hangs for 5 seconds. Resolve by adding `options single-request` in `/etc/resolv.conf`.

---

## 9. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK SCENARIO: Compromise of Frontend Web Service          |
+--------------------------------------------------------------+
| Web App running Nginx/Node has remote code execution (RCE).  |
| Attacker spawns shell inside web container.                  |
| Attacker scans internal network for SQL databases.           |
+--------------------------------------------------------------+
| IN FLAT NETWORK ARCHITECTURE:                                |
|  - Database is on same subnet (172.18.0.5).                  |
|  - Attacker dumps entire SQL database immediately.           |
+--------------------------------------------------------------+
| IN TIERED ARCHITECTURE (dmz-net vs backend-net):             |
|  - Web container is physically absent from backend-net.     |
|  - Linux kernel drops all packets to backend-net.            |
|  - DATABASE REMAINS COMPLETELY UNTOUCHED AND UNREACHABLE.    |
+--------------------------------------------------------------+
```

---

## 10. Architectural Comparison Table

| Feature / Trait | Default Bridge (`docker0`) | User-Defined Bridge (`br-custom`) |
|:---|:---|:---|
| **Creation** | Automatic on daemon start | Manual via `docker network create` |
| **DNS Service Discovery** | **Disabled** (Static IPs only) | **Enabled** (Automatic via `127.0.0.11`) |
| **Network Aliases** | Not supported | **Supported** (`--network-alias`) |
| **Subnet Customization**| Global daemon flag only | **Per-network** (`--subnet`, `--gateway`) |
| **Hot Plugging** | No | **Yes** (`docker network connect/disconnect`) |
| **Isolation Scope** | Shared across all unassigned containers | **Strictly isolated** to member containers |

---

## 11. Real-World Scenario Walkthrough

### Scenario: Resolving Docker Subnet Collision with Corporate Cloud VPN

#### The Issue
An engineering team connecting to AWS via WireGuard corporate VPN (`172.17.0.0/16`) found that whenever Docker Desktop started, their corporate VPN connections dropped instantly, preventing access to JIRA, GitHub Enterprise, and internal staging environments.

#### Root Cause
Docker's default bridge network automatically allocated `172.17.0.0/16` for `docker0`. The host routing table routed all corporate VPN traffic into the local Docker software bridge instead of the WireGuard tunnel interface.

#### The Permanent Fix
1. Configured `/etc/docker/daemon.json` to move the default bridge and allocate dynamic subnets strictly within the `10.200.0.0/16` address space:
   ```json
   {
     "bip": "10.200.0.1/24",
     "default-address-pools": [
       {
         "base": "10.200.0.0/16",
         "size": 24
       }
     ]
   }
   ```
2. Restarted Docker daemon.
3. All subsequently created user-defined networks received subnets `10.200.1.0/24`, `10.200.2.0/24`, etc., completely resolving the VPN routing collision.

---

## 12. Self-Assessment & Interview Questions

<details>
<summary><strong>1. How does Docker's embedded DNS server (`127.0.0.11`) handle queries for external internet domain names?</strong></summary>

**Answer:**
When a container queries `127.0.0.11`, Docker checks its internal registry of container names and network aliases on that specific network. If no local match exists, the embedded DNS server automatically forwards the query upstream to the DNS nameservers configured in the host's `/etc/resolv.conf` (or custom `--dns` servers defined on the container or daemon).
</details>

<details>
<summary><strong>2. How can you implement DNS-based round-robin load balancing using standard Docker CLI commands?</strong></summary>

**Answer:**
Launch multiple containers attached to the same user-defined network and pass the exact same `--network-alias <alias>` flag to each container:
```bash
docker run -d --network my-net --network-alias api-service service:v1
docker run -d --network my-net --network-alias api-service service:v1
```
Any client container querying `api-service` receives all member IP addresses in rotating round-robin order.
</details>

<details>
<summary><strong>3. What is the purpose of the `default-address-pools` setting in `daemon.json`?</strong></summary>

**Answer:**
`default-address-pools` specifies the global CIDR blocks that the Docker daemon slices from when creating user-defined bridge networks. By defining custom base pools (e.g., `10.100.0.0/16` with size 24), administrators prevent Docker from automatically selecting `172.17.0.0/16` or `192.168.0.0/16`, eliminating conflicts with corporate networks and VPNs.
</details>

<details>
<summary><strong>4. Can two containers with the same container name exist on different user-defined networks on the same host?</strong></summary>

**Answer:**
No. Container names must be globally unique across a single Docker daemon host. However, two containers can share the same **network alias** on the same network (for load balancing) or on different networks.
</details>

<details>
<summary><strong>5. What does the `enable_icc=false` bridge option do?</strong></summary>

**Answer:**
`enable_icc` stands for Inter-Container Communication. When set to `false` during network creation (`--opt com.docker.network.bridge.enable_icc=false`), Docker adds iptables `DROP` rules preventing containers on the same bridge network from communicating directly with each other, requiring all traffic to route through published ports on the host.
</details>

<details>
<summary><strong>6. How can an administrator assign a static IP address to a Docker container?</strong></summary>

**Answer:**
The container must be attached to a **user-defined network** that has an explicitly defined `--subnet`. The static IP is specified using the `--ip` flag during `docker run`:
`docker run --network custom-net --ip 10.10.1.50 my-image`
(Static IPs cannot be assigned on the default `docker0` bridge).
</details>

<details>
<summary><strong>7. What happens when a container is disconnected from a network using `docker network disconnect` while running?</strong></summary>

**Answer:**
Docker destroys the container's virtual ethernet interface (`ethX`) for that network inside its network namespace and removes the associated `veth` peer from the host bridge. The container immediately loses connectivity to that network without its main process being stopped or restarted.
</details>

<details>
<summary><strong>8. Why does Alpine Linux sometimes experience 5-second DNS resolution delays in Docker containers?</strong></summary>

**Answer:**
Alpine's `musl libc` DNS resolver issues `A` (IPv4) and `AAAA` (IPv6) queries concurrently over the same socket. Many intermediate firewalls, routers, or Docker DNS implementations handle concurrent lookups poorly or drop IPv6 responses, causing `musl` to wait for a 5-second timeout before falling back.
</details>

---

## 13. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Creating a Custom CIDR Bridge Network</strong></summary>

**Goal:** Create a custom bridge with subnet `10.88.0.0/16` and inspect the resulting gateway.

**Step-by-step Solution:**
1. Create network:
   ```bash
   docker network create --subnet 10.88.0.0/16 --gateway 10.88.0.1 custom-ipam-net
   ```
2. Inspect gateway:
   ```bash
   docker network inspect custom-ipam-net --format '{{(index .IPAM.Config 0).Gateway}}'
   # Outputs: 10.88.0.1
   ```
</details>

<details>
<summary><strong>Exercise 2: DNS Round-Robin Verification via Dig</strong></summary>

**Goal:** Launch two containers under alias `search-pool` and observe DNS rotations using `dig`.

**Step-by-step Solution:**
1. Setup:
   ```bash
   docker network create rr-net
   docker run -d --name s1 --network rr-net --network-alias search-pool alpine sleep 3600
   docker run -d --name s2 --network rr-net --network-alias search-pool alpine sleep 3600
   ```
2. Run dig:
   ```bash
   docker run --rm --network rr-net alpine sh -c "apk add --no-cache bind-tools && dig +short search-pool"
   ```
3. Observe two IP addresses returned.
</details>

<details>
<summary><strong>Exercise 3: Static IP Assignment and Ping Test</strong></summary>

**Goal:** Assign static IP `10.88.0.99` and verify via ping.

**Step-by-step Solution:**
1. Run container:
   ```bash
   docker run -d --name static-tester --network custom-ipam-net --ip 10.88.0.99 alpine sleep 3600
   ```
2. Ping from another container:
   ```bash
   docker run --rm --network custom-ipam-net alpine ping -c 2 10.88.0.99
   ```
</details>

<details>
<summary><strong>Exercise 4: Verifying Complete Network Isolation</strong></summary>

**Goal:** Launch containers on `net-a` and `net-b` and verify packets cannot cross networks.

**Step-by-step Solution:**
1. Create:
   ```bash
   docker network create net-a
   docker network create net-b
   docker run -d --name box-a --network net-a alpine sleep 3600
   docker run -d --name box-b --network net-b alpine sleep 3600
   ```
2. Attempt cross-ping:
   ```bash
   docker exec box-a ping -c 1 box-b
   # ping: bad address 'box-b'
   ```
</details>

<details>
<summary><strong>Exercise 5: Hot-Connecting a Third Network</strong></summary>

**Goal:** Connect running `box-a` to `net-b` and verify instant connectivity.

**Step-by-step Solution:**
1. Connect:
   ```bash
   docker network connect net-b box-a
   ```
2. Ping again:
   ```bash
   docker exec box-a ping -c 1 box-b
   # Ping succeeds!
   ```
</details>

<details>
<summary><strong>Exercise 6: Inspecting Linux Bridge Name on Host</strong></summary>

**Goal:** Set a custom Linux interface name and verify it in host `ip link`.

**Step-by-step Solution:**
1. Create:
   ```bash
   docker network create -o com.docker.network.bridge.name=br-mycorp corp-net
   ```
2. On host:
   ```bash
   ip link show br-mycorp
   # Displays the virtual switch interface!
   ```
</details>

---

## 14. Summary & Reference Cheat Sheet

### CLI Quick Reference
```bash
# Advanced IPAM Creation:
docker network create --subnet 10.10.0.0/16 --gateway 10.10.0.1 <net_name>

# Static IP & Network Alias:
docker run --network <net> --ip 10.10.0.50 --network-alias <alias> <image>

# Dynamic Attachment:
docker network connect <net> <container>
docker network disconnect <net> <container>
```

### Golden Architectural Rules
1. **Always use user-defined networks** for inter-container communication.
2. **Never hardcode IP addresses** in application code; use DNS service discovery.
3. **Segregate microservice tiers** (frontend, backend, database) across distinct networks.
4. **Use `--network-alias`** for client-side round-robin load distribution.

---

[← Previous: 04-networking.md](04-networking.md) | [Next: 06-ports-publishing-and-firewalls.md →](06-ports-publishing-and-firewalls.md)
