# Port Publishing Mechanics, iptables, and Firewall Security

```
Track: DevOps & Containerization
Module: 04-volumes-and-networking
Prerequisites: 04-volumes-and-networking/04-networking.md, 04-volumes-and-networking/05-user-defined-networks-and-dns.md
Difficulty: Advanced
Estimated Duration: 45 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Flag Reference](#4-syntax--flag-reference)
5. [Practical Implementation & Examples](#5-practical-implementation--examples)
6. [The Docker vs Host Firewall (UFW/Firewalld) Dilemma](#6-the-docker-vs-host-firewall-ufwfirewalld-dilemma)
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

Publishing a port exposes an isolated container service to the outside world. While `EXPOSE` in a Dockerfile acts merely as documentation, port publishing (`-p` / `-P`) actively modifies the Linux host kernel networking tables.

Improperly configured port publishing is one of the **most common sources of enterprise security breaches**:
- **The UFW/Firewalld Bypass**: By default, Docker injects rules directly into Linux `iptables` that **completely bypass host firewalls (like UFW or Firewalld)**. Ports intended for private access (e.g., Elasticsearch, Redis, Postgres) can be accidentally exposed to the entire public internet.
- **Port Collision Storms**: Binding host ports without specifying target IP interfaces defaults to `0.0.0.0` (all IPv4 interfaces), colliding with host services or cloud VPC management daemons.
- **Resource Exhaustion**: Docker historically spawns a userland process (`docker-proxy`) for every published port, consuming host memory and CPU cycles under thousands of concurrent mapped ports.

---

## 2. Core Concepts

### Publishing (`-p`) vs Exposing (`EXPOSE`)
- **`EXPOSE 80` (Dockerfile)**: Metadata only. Signals that the container listens on port 80. It does not publish the port, does not alter iptables, and does not make the port reachable from outside the Docker virtual network.
- **`-p <host>:<container>` (CLI / Compose)**: Modifies host Linux `iptables` rules to route inbound network packets from the host's physical network adapter into the container's private virtual IP.
- **`-P` / `--publish-all`**: Scans the image's `EXPOSE` directives and automatically maps each one to a random ephemeral high-range host port (typically `32768–60999`).

### Binding Interfaces
- `0.0.0.0:8080:80` (Default if IP omitted): Binds to **all** network interfaces on the host (public internet, private VPC, local loopback).
- `127.0.0.1:8080:80`: Binds strictly to **localhost**. Only processes running on the host machine can reach it; it is completely invisible to the external network.
- `10.0.1.50:8080:80`: Binds strictly to a private corporate interface (e.g., internal VPC subnet).

---

## 3. How It Works Internally

### Netfilter, iptables, and the DOCKER Chain

When the Docker daemon starts, it creates custom chains in Linux Netfilter: `DOCKER`, `DOCKER-USER`, and `DOCKER-ISOLATION-STAGE-*`.

```
Incoming Packet (Host eth0:8080)
       |
       v
[PREROUTING Chain (nat table)]
       |
       v
Does destination match published port?
       |
       +---> YES: DNAT Rule rewrites destination IP from Host IP to Container IP (172.18.0.2:80)
       |
       v
[FORWARD Chain (filter table)]
       |
       v
+-------------------------------+
| DOCKER-USER Chain             | <--- Evaluated FIRST! Administrator custom rules go here.
+---------------+---------------+
                |
                v
+-------------------------------+
| DOCKER Chain                  | <--- Evaluated SECOND! Docker auto-generated rules accept traffic.
+---------------+---------------+
                |
                v
Packet routed across virtual bridge (br-xxx) to Container (eth0:80)
```

### The Role of `docker-proxy`
In addition to iptables NAT rules, Docker spawns a user-space proxy binary (`docker-proxy`) for each published port.
- Its primary purpose is handling **Hairpin NAT** and loopback traffic: when a container or local process on the host connects to `localhost:8080` or the host's own public IP, the Linux kernel's route table may not apply standard PREROUTING DNAT rules. `docker-proxy` acts as an in-memory socket bridge.

---

## 4. Syntax & Flag Reference

### Detailed Port Mapping Variations

```bash
# 1. Standard host-to-container mapping (Binds to 0.0.0.0 on all interfaces!)
docker run -d -p 8080:80 nginx:alpine

# 2. Bind strictly to localhost (Secure: No public access)
docker run -d -p 127.0.0.1:8080:80 nginx:alpine

# 3. Bind to specific internal private IP (e.g. VPN or VPC interface)
docker run -d -p 10.0.0.5:8080:80 nginx:alpine

# 4. Map UDP instead of TCP (default is TCP)
docker run -d -p 53:53/udp coredns:latest

# 5. Map both TCP and UDP simultaneously
docker run -d -p 53:53/tcp -p 53:53/udp dns-server:latest

# 6. Publish to a random ephemeral port (Auto-assigned)
docker run -d -p 80 nginx:alpine

# 7. Publish all EXPOSE directives to random ephemeral ports
docker run -d -P nginx:alpine

# 8. Map a range of contiguous ports
docker run -d -p 7000-7005:7000-7005 redis-cluster:latest
```

---

## 5. Practical Implementation & Examples

### Example 1: Secure Local-Only Binding for Internal Database
Preventing accidental public database exposure by binding strictly to `127.0.0.1`.

```bash
# Secure: Database only accessible from local host scripts/tunnels
docker run -d \
  --name local-dev-db \
  -p 127.0.0.1:5432:5432 \
  -e POSTGRES_PASSWORD=secret \
  postgres:17-alpine

# Verify binding using netstat / ss on host:
ss -tulpn | grep 5432
# Output displays: 127.0.0.1:5432 (NOT 0.0.0.0:5432!)
```

### Example 2: Inspecting Dynamically Assigned Ephemeral Ports (`-P`)

```bash
# Run container with -P
docker run -d --name web-random -P nginx:alpine

# Query assigned host port using docker port
docker port web-random
# Output: 80/tcp -> 0.0.0.0:32769

# Verify reachability
curl http://localhost:32769
```

### Example 3: Dual Protocol Mapping (VoIP / DNS / Gaming Servers)
Deploying services that require both TCP and UDP on the same port number.

```bash
docker run -d \
  --name bind9-dns \
  -p 127.0.0.1:5353:53/tcp \
  -p 127.0.0.1:5353:53/udp \
  internals/dns:v1
```

---

## 6. The Docker vs Host Firewall (UFW/Firewalld) Dilemma

### The Problem
When a Linux administrator installs Ubuntu and runs:
```bash
ufw default deny incoming
ufw allow 22
ufw enable
```
They expect **all** incoming ports except SSH (22) to be blocked.
However, when someone runs:
```bash
docker run -d -p 8080:80 nginx:alpine
```
**Port 8080 is immediately publicly accessible to the world!**

### Why This Happens
UFW configures rules in the `ufw-user-input` chain, which is processed inside Netfilter's `INPUT` chain.
However, Docker traffic is routed via **Network Address Translation (NAT)** into the `FORWARD` chain. Netfilter processes NAT PREROUTING and the FORWARD chain before the INPUT chain is ever reached. As a result, UFW never evaluates the packets!

### The Three Solutions

#### Solution 1: Bind Strictly to Localhost (Easiest & Most Secure)
If the container is fronted by an Nginx/Traefik reverse proxy or accessed via an SSH tunnel, never expose it on `0.0.0.0`:
```bash
docker run -p 127.0.0.1:8080:80 my-app
```

#### Solution 2: Custom Rules in the `DOCKER-USER` Chain
Docker specifically reserves the `DOCKER-USER` chain for host administrator rules. Rules added here run **before** Docker's auto-generated rules.

```bash
# 1. Block all external public access to Docker containers by default
iptables -I DOCKER-USER -i eth0 -j DROP

# 2. Allow established and related connections (so outbound responses return)
iptables -I DOCKER-USER -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

# 3. Selectively allow specific trusted subnet (e.g., Office VPN) to port 8080
iptables -I DOCKER-USER -i eth0 -s 192.168.10.0/24 -p tcp --dport 8080 -j ACCEPT
```

#### Solution 3: Deploy `ufw-docker`
The community standard tool `ufw-docker` automates modifying `DOCKER-USER` using standard UFW-like syntax:
```bash
ufw-docker allow web-container 80
```

---

## 7. Production Best Practices & Antipatterns

### Best Practices
- **Never Bind to `0.0.0.0` for Databases or Caches**: Always bind databases to `127.0.0.1` or rely strictly on internal Docker networks without publishing ports.
- **Deploy a Single Reverse Proxy on Ports 80 and 443**: Only publish ports 80 and 443 to the public internet (via Nginx, Traefik, or Envoy). Route all other microservices internally through user-defined bridge networks.
- **Use `DOCKER-USER` for Enterprise Firewall Rules**: Never modify the `DOCKER` chain directly; Docker flushes and rebuilds it on service reloads. Put custom firewall rules in `DOCKER-USER`.
- **Disable `userland-proxy` on Linux When High Concurrency Demands It**: In `/etc/docker/daemon.json`, setting `"userland-proxy": false` relies purely on kernel iptables hairpin NAT, saving RAM and eliminating thousands of idle `docker-proxy` processes.

### Anti-Patterns
- **Assuming UFW Protects Docker Containers**: Relying on standard `ufw deny` without testing external reachability leads to severe database leaks.
- **Publishing Ports for Containers on the Same Host**: If Container A needs to talk to Container B, do not use host ports (`http://host-ip:8080`). Connect them to the same user-defined network and use DNS (`http://container-b:80`).
- **Setting `"iptables": false` in `daemon.json` Without Replacement**: Disabling Docker iptables breaks automatic container outbound internet routing unless you manually write hundreds of lines of complex SNAT/DNAT rules.

---

## 8. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| `bind: address already in use` | Another process on the host is already listening on that port | `ss -tulpn \| grep <port>` or `lsof -i :<port>` | Map to a different host port (e.g., `-p 8081:80`) or stop conflicting host process. |
| Port 5432 accessible to public internet despite UFW blocking it | Docker bypasses UFW by injecting rules directly into iptables NAT table | `curl http://<public-ip>:5432` from external network | Bind explicitly to `127.0.0.1:5432:5432` or configure `DOCKER-USER` chain. |
| High memory usage with hundreds of published ports | Each published port spawns a separate `docker-proxy` process consuming ~10MB RAM | `ps aux \| grep docker-proxy \| wc -l` | Set `"userland-proxy": false` in `/etc/docker/daemon.json`. |
| Container cannot reach its own published port via host public IP | Hairpin NAT failure or disabled `userland-proxy` | Run curl from inside container to host IP | Ensure userland proxy is enabled or use container DNS names directly. |
| Port publishing fails with `iptables: No chain/target/match by that name` | Host firewall service (e.g., Firewalld) was restarted after Docker daemon started | `systemctl status docker` | Restart Docker daemon (`systemctl restart docker`) to recreate its iptables chains. |
| UDP packets dropped under high load | Linux UDP buffer size exhausted or connection tracker overflow | `netstat -su` or `dmesg \| grep nf_conntrack` | Increase `net.core.rmem_max` and `net.netfilter.nf_conntrack_max` via sysctl. |

---

## 9. Performance Tuning & Resource Optimization

### Disabling the Userland Proxy (`userland-proxy`)
On Linux systems, the kernel's `iptables` NAT handles all packet routing natively. By default, Docker also starts a userland proxy (`docker-proxy`) process written in Go for backward compatibility with kernels that lacked hairpin routing.

To eliminate `docker-proxy` memory overhead across hundreds of containers, configure `/etc/docker/daemon.json`:
```json
{
  "userland-proxy": false
}
```
Reload daemon:
```bash
sudo systemctl restart docker
```
> **Performance Gain**: Saves 5-15MB of host memory per mapped port and avoids userland context-switch bottlenecks during high-throughput packet bursts.

---

## 10. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Ransomware Encryption of Unprotected MongoDB         |
+--------------------------------------------------------------+
| Developer runs: `docker run -d -p 27017:27017 mongo`         |
| Developer assumes UFW default deny protects the port.       |
| Shodan bots scan IPv4 internet -> Port 27017 is open!        |
| Automated bot connects, dumps databases, deletes data,      |
| and leaves a Bitcoin ransom note table.                      |
|                                                              |
| PERMANENT MITIGATION: `docker run -d -p 127.0.0.1:27017:27017 mongo`|
+--------------------------------------------------------------+
```

---

## 11. Architectural Comparison Table

| Mapping Option | Interfaces Bound | Public Internet Reachable? | Security Posture | Best Used For |
|:---|:---|:---|:---|:---|
| `-p 8080:80` | `0.0.0.0` (All) | **YES (Bypasses UFW!)** | Dangerous if unprotected| Public reverse proxies only |
| `-p 127.0.0.1:8080:80`| `127.0.0.1` (Local only) | **NO** | **High** | Databases, dev servers, SSH tunnels |
| `-p 10.0.1.5:8080:80` | Private VPC IP only | **NO (VPC internal only)**| **High** | Internal corporate microservices |
| `-P` (Publish All) | `0.0.0.0` (Ephemeral) | **YES** | Low | Dynamic testing / CI test runners |
| No `-p` (Internal only)| None on host | **NO** | **Maximum** | Microservice-to-microservice calls |

---

## 12. Real-World Scenario Walkthrough

### Scenario: Securing an Enterprise Elasticsearch Cluster from Shodan Exposure

#### Incident
A healthcare startup received a vulnerability disclosure: their 5-node Elasticsearch cluster holding patient metadata was indexed on Shodan. The engineering team was shocked because their AWS EC2 security group restricted access to the office IP, and Ubuntu's UFW was active.

#### Investigation
1. The EC2 instance had a public elastic IP for maintenance.
2. The team had deployed Elasticsearch with:
   `docker run -d -p 9200:9200 elasticsearch:8.17`
3. Docker injected a `DNAT` rule into iptables `PREROUTING`. The AWS EC2 security group had accidentally left port 9200 open to `0.0.0.0/0` during a late-night debugging session.
4. UFW did nothing to block the traffic because Docker bypassed it.

#### Emergency Remediation
1. Blocked the port immediately via `DOCKER-USER`:
   ```bash
   iptables -I DOCKER-USER -p tcp --dport 9200 -j DROP
   ```
2. Re-deployed the container binding strictly to localhost:
   ```bash
   docker run -d -p 127.0.0.1:9200:9200 elasticsearch:8.17
   ```
3. Enabled TLS and mandatory password authentication inside Elasticsearch.

---

## 13. Self-Assessment & Interview Questions

<details>
<summary><strong>1. Why does Docker port publishing (`-p 8080:80`) bypass standard Ubuntu UFW firewall rules?</strong></summary>

**Answer:**
UFW inserts its filtering rules into Netfilter's `INPUT` chain. However, Docker routes container-bound packets using Destination NAT (`DNAT`), which causes the Linux kernel routing decision to send packets through the `FORWARD` chain instead of `INPUT`. Because Docker manipulates the `PREROUTING` and `FORWARD` chains directly, packets reach the container before UFW's rules in the `INPUT` chain are ever evaluated.
</details>

<details>
<summary><strong>2. How should an administrator insert custom firewall rules that Docker will respect?</strong></summary>

**Answer:**
Rules must be inserted into the custom **`DOCKER-USER`** iptables chain. Docker routes all forwarded packets through `DOCKER-USER` before processing any rules in its own auto-generated `DOCKER` chain. Rules placed in `DOCKER-USER` are preserved across container creations, destructions, and Docker daemon restarts.
</details>

<details>
<summary><strong>3. What is the difference between `-p` and `-P`?</strong></summary>

**Answer:**
`-p` allows explicit mapping of specific host ports to container ports (e.g., `-p 8080:80`). `-P` (or `--publish-all`) inspects all ports defined via `EXPOSE` in the image's Dockerfile and automatically maps each one to a randomly chosen ephemeral port from the host's high-port range (typically `32768–60999`).
</details>

<details>
<summary><strong>4. How do you bind a container port strictly to the host machine's loopback interface?</strong></summary>

**Answer:**
Prefix the host port with `127.0.0.1:`:
`docker run -p 127.0.0.1:8080:80 my-image`
This guarantees that the port cannot be accessed from outside the host machine.
</details>

<details>
<summary><strong>5. What is the `docker-proxy` process, and how can it be disabled in production Linux environments?</strong></summary>

**Answer:**
`docker-proxy` is a userland Go binary spawned for every published port to forward traffic from host ports to container ports when hairpin NAT or loopback routing is needed. On Linux kernels supporting native hairpin NAT, it can be safely disabled by adding `"userland-proxy": false` to `/etc/docker/daemon.json`, which saves memory and eliminates process overhead.
</details>

<details>
<summary><strong>6. Does publishing a port make it accessible over both TCP and UDP by default?</strong></summary>

**Answer:**
No. If no protocol is specified (e.g., `-p 8080:80`), Docker defaults exclusively to **TCP**. To publish UDP, the protocol must be explicitly suffixed: `-p 53:53/udp`. To support both, declare two separate mapping arguments.
</details>

<details>
<summary><strong>7. How can you query which host port was assigned to a container that was run with `-P`?</strong></summary>

**Answer:**
Run `docker port <container_name_or_id>`. It outputs the active mappings, e.g., `80/tcp -> 0.0.0.0:32769`.
</details>

<details>
<summary><strong>8. What is the danger of setting `"iptables": false` in `/etc/docker/daemon.json`?</strong></summary>

**Answer:**
Setting `"iptables": false` prevents Docker from modifying the host firewall entirely. While this stops Docker from bypassing UFW, it also completely disables Docker's automatic port forwarding (`-p`) and container outbound internet access (NAT masquerading). Administrators must manually write and maintain all iptables rules for every container created.
</details>

---

## 14. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Validating Localhost-Only Port Binding</strong></summary>

**Goal:** Launch Nginx bound strictly to `127.0.0.1` and verify with `ss` that it is not listening on `0.0.0.0`.

**Step-by-step Solution:**
1. Run container:
   ```bash
   docker run -d --name local-nginx -p 127.0.0.1:9090:80 nginx:alpine
   ```
2. Inspect host sockets:
   ```bash
   ss -tulpn | grep 9090
   # Shows: 127.0.0.1:9090 (Clean!)
   ```
3. Test local curl: `curl http://127.0.0.1:9090` -> HTTP 200.
</details>

<details>
<summary><strong>Exercise 2: Publishing Contiguous Port Ranges</strong></summary>

**Goal:** Map a range of 3 contiguous ports (9001 to 9003) to a container in a single command.

**Step-by-step Solution:**
1. Run container:
   ```bash
   docker run -d --name range-test -p 9001-9003:9001-9003 alpine sleep 3600
   ```
2. Verify with `docker port range-test`.
</details>

<details>
<summary><strong>Exercise 3: Observing Ephemeral Port Mapping with `-P`</strong></summary>

**Goal:** Run an image with multiple `EXPOSE` declarations and map all to random ports.

**Step-by-step Solution:**
1. Run container:
   ```bash
   docker run -d --name random-ports -P nginx:alpine
   ```
2. Query assigned port:
   ```bash
   PORT=$(docker port random-ports 80/tcp | cut -d ':' -f 2)
   echo "Assigned port: $PORT"
   curl http://localhost:$PORT
   ```
</details>

<details>
<summary><strong>Exercise 4: Inspecting Docker's Iptables PREROUTING Chain</strong></summary>

**Goal:** Locate the DNAT rule created by Docker for a published port.

**Step-by-step Solution:**
1. Launch container:
   ```bash
   docker run -d --name nat-demo -p 7777:80 nginx:alpine
   ```
2. Inspect iptables NAT table:
   ```bash
   sudo iptables -t nat -L DOCKER -n -v | grep 7777
   # Shows: DNAT tcp dpt:7777 to:172.17.0.x:80
   ```
</details>

<details>
<summary><strong>Exercise 5: Adding a Protective Rule to `DOCKER-USER`</strong></summary>

**Goal:** Insert a rule in `DOCKER-USER` to block an external IP from reaching published containers.

**Step-by-step Solution:**
1. Block IP `203.0.113.50`:
   ```bash
   sudo iptables -I DOCKER-USER -s 203.0.113.50 -j DROP
   ```
2. Verify rule position:
   ```bash
   sudo iptables -L DOCKER-USER -n -v --line-numbers
   # Rule appears at line 1!
   ```
</details>

<details>
<summary><strong>Exercise 6: Mapping Both TCP and UDP Protocols</strong></summary>

**Goal:** Map port 8888 for both TCP and UDP simultaneously.

**Step-by-step Solution:**
1. Run container:
   ```bash
   docker run -d --name dual-proto -p 8888:8888/tcp -p 8888:8888/udp alpine sleep 3600
   ```
2. Verify via `docker port dual-proto`.
</details>

---

## 15. Summary & Reference Cheat Sheet

### CLI Port Syntax
```bash
-p [ip:][host_port:]container_port[/protocol]

Examples:
-p 80:80                      # 0.0.0.0 (Public TCP)
-p 127.0.0.1:8080:80          # Localhost only (Secure!)
-p 10.0.1.10:8080:80          # Private IP only
-p 53:53/udp                  # UDP only
-P                            # All exposed ports -> Ephemeral high ports
```

### Golden Security Rules
1. **Never use `-p <port>:<port>` for internal databases**: Bind to `127.0.0.1` or rely on Docker network DNS.
2. **Never assume UFW protects Docker**: Docker bypasses UFW by default.
3. **Use the `DOCKER-USER` chain** for custom firewall rules.
4. **Disable `userland-proxy`** in production Linux daemons to save memory and CPU.

---

[← Previous: 05-user-defined-networks-and-dns.md](05-user-defined-networks-and-dns.md) | [Next: 07-container-to-host-and-container-to-container.md →](07-container-to-host-and-container-to-container.md)
