---
title: "High Availability NGINX Clusters with Keepalived & VRRP"
description: "Master engineering guide to building active-passive and active-active high availability NGINX topologies using Keepalived, VRRP virtual IP failover, non-local socket binding, and split-brain defenses."
module: "06-production"
topic: "06-high-availability-and-vrrp"
status: "completed"
tags: ["nginx", "high-availability", "keepalived", "vrrp", "failover", "vip", "clustering", "active-passive", "split-brain"]
prerequisites: ["06-production/03-operations-zero-downtime-reloads-and-ci.md", "05-security-performance/01-performance-tuning.md"]
estimated_read_time: "35 min"
---

# High Availability NGINX Clusters with Keepalived & VRRP

```
+-------------------------------------------------------------------------------+
| Active-Passive VRRP High Availability Topology                                |
|                                                                               |
|  [Clients / Internet DNS]                                                     |
|        │  Resolves to Virtual IP (VIP): 192.168.1.50                         |
|        ▼                                                                      |
|  ==================== Shared Ethernet / Layer 2 Broadcast Domain =============|
|        │                                                      │               |
|        ▼ (Active Owner of VIP)                                ▼ (Standby)     |
|  [Node 1: MASTER (Priority 101)]             [Node 2: BACKUP (Priority 100)]  |
|  - Real IP: 192.168.1.11                     - Real IP: 192.168.1.12          |
|  - Keepalived daemon (Heartbeat: 1s) <-----> - Keepalived daemon (Listening)  |
|  - Binds VIP: 192.168.1.50                   - VIP inactive (Standby)         |
|  - NGINX Proxy Active                        - NGINX Proxy Warm & Ready       |
|                                                                               |
|  Failover Trigger:                                                            |
|  If Node 1 crashes or NGINX process dies:                                     |
|  1. Node 1 stops sending VRRP advertisements.                                 |
|  2. Node 2 detects missed heartbeats (3 seconds).                             |
|  3. Node 2 transitions to MASTER state.                                       |
|  4. Node 2 sends Gratuitous ARP (GARP) claiming VIP 192.168.1.50!             |
|  5. Traffic instantly switches to Node 2 with zero DNS propagation delay!     |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: VRRP & Virtual IPs (VIP)](#2-core-concepts-vrrp--virtual-ips-vip)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Keepalived Configuration Reference](#4-syntax--keepalived-configuration-reference)
5. [Practical Implementation Examples](#5-practical-implementation-examples)
6. [Active-Active Dual-VIP Topology](#6-active-active-dual-vip-topology)
7. [Split-Brain Scenarios & Defenses](#7-split-brain-scenarios--defenses)
8. [Common Pitfalls & Anti-Patterns](#8-common-pitfalls--anti-patterns)
9. [Architectural Comparison: Keepalived VRRP vs DNS Round-Robin vs BGP Anycast](#9-architectural-comparison-keepalived-vrrp-vs-dns-round-robin-vs-bgp-anycast)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Even the most optimized, hardened NGINX server represents a single point of failure (SPOF) if it runs on a single physical host or virtual machine:
- **Hardware Failures & Maintenance**: Power supply blowouts, kernel panics, hypervisor crashes, and OS security patching require taking individual machines offline.
- **The Inadequacy of DNS Failover**: Relying on DNS record updates to divert traffic away from a dead server is too slow. Browsers, client operating systems, and recursive resolvers cache DNS records far beyond their configured TTL, resulting in 5 to 30 minutes of customer-facing downtime.
- **Layer 2 Failover Speed**: Keepalived leveraging the Virtual Router Redundancy Protocol (VRRP) operates at Layer 2/3. If an active NGINX node dies, failover completes within **1 to 3 seconds** without requiring any DNS modifications.

A Keepalived VRRP cluster guarantees that client traffic continues routing uninterrupted to surviving edge nodes.

---

## 2. Core Concepts: VRRP & Virtual IPs (VIP)

### The Virtual Router Redundancy Protocol (VRRP - RFC 5798)
VRRP is an election protocol that dynamically assigns responsibility for a virtual router IP address to one of the VRRP routers on a local area network (LAN):
- **Master Node**: Periodically broadcasts VRRP advertisement packets (IP protocol 112, multicast address `224.0.0.18`) at a fixed interval (typically 1 second).
- **Backup Node**: Listens passively for advertisements. If it fails to receive advertisements for three consecutive intervals ($3 \times \text{advert\_int}$ plus skew time), it assumes the Master is dead and transitions to the Master state.
- **Gratuitous ARP (GARP)**: When a node becomes Master, it broadcasts an unsolicited Gratuitous ARP packet across the local network switch fabric. This immediately updates the ARP cache of the upstream network router and switches, directing all traffic for the Virtual IP to the new node's MAC address instantly.

---

## 3. How It Works Internally

### The Kernel Non-Local Binding Prerequisite (`ip_nonlocal_bind`)
When NGINX starts on the Standby node, it attempts to bind to port 80/443 on the Virtual IP (`listen 192.168.1.50:443 ssl;`). However, the Standby node does NOT yet own the VIP on its network interface.
By default, the Linux kernel refuses to bind sockets to IP addresses that do not exist locally on the machine, causing NGINX to fail to start:
```text
nginx: [emerg] bind() to 192.168.1.50:443 failed (99: Cannot assign requested address)
```
To solve this, you must enable non-local socket binding in the Linux kernel:
```bash
sudo sysctl -w net.ipv4.ip_nonlocal_bind=1
```
This instructs the kernel to allow NGINX to bind to IP addresses that are not currently assigned to local interfaces, keeping NGINX running in warm standby.

---

## 4. Syntax & Keepalived Configuration Reference

Keepalived configurations reside in `/etc/keepalived/keepalived.conf` and consist of three main blocks:

### 1. `vrrp_script` (Health Check Monitoring)
Defines an automated script executed by Keepalived to verify that the local NGINX process is alive:
```nginx
vrrp_script check_nginx {
    script "/usr/bin/killall -0 nginx" # Returns 0 if process exists
    interval 2                         # Run every 2 seconds
    weight -20                         # Subtract 20 from priority if script fails
    fall 2                             # Require 2 failures before penalizing
    rise 2                             # Require 2 successes before restoring
}
```

### 2. `vrrp_instance` (VRRP State Machine)
- `state MASTER | BACKUP`: Initial state upon startup.
- `interface <name>`: Network interface where VRRP advertisements are broadcast.
- `virtual_router_id <1-255>`: Unique ID identifying this VRRP cluster on the LAN. Must match on all cluster nodes!
- `priority <number>`: Highest priority node becomes Master (e.g. 101 for Node 1, 100 for Node 2).
- `advert_int <seconds>`: Frequency of heartbeat advertisements.
- `virtual_ipaddress`: The shared IP address(es) managed by the cluster.

---

## 5. Practical Implementation Examples

### Step 1: Enable Non-Local Binding on Both Nodes

```ini
# /etc/sysctl.d/99-keepalived.conf (On Node 1 and Node 2)
net.ipv4.ip_nonlocal_bind = 1
```
Apply:
```bash
sudo sysctl -p /etc/sysctl.d/99-keepalived.conf
```

---

### Step 2: Configure Node 1 (`MASTER` - 192.168.1.11)

```ini
# /etc/keepalived/keepalived.conf on Node 1
global_defs {
    router_id nginx_node_1
    enable_script_security
    script_user root
}

# 1. Health check script
vrrp_script check_nginx_alive {
    script "/usr/bin/killall -0 nginx"
    interval 1
    weight -20
    fall 2
    rise 2
}

# 2. VRRP Instance
vrrp_instance VI_EDGE {
    state MASTER
    interface eth0
    virtual_router_id 51
    priority 101 # Higher priority than Node 2
    advert_int 1

    authentication {
        auth_type PASS
        auth_pass Secr3tVRRPK3y
    }

    # Shared Virtual IP
    virtual_ipaddress {
        192.168.1.50/24 dev eth0 label eth0:vip
    }

    track_script {
        check_nginx_alive
    }

    notify_master "/opt/scripts/notify.sh MASTER"
    notify_backup "/opt/scripts/notify.sh BACKUP"
    notify_fault  "/opt/scripts/notify.sh FAULT"
}
```

---

### Step 3: Configure Node 2 (`BACKUP` - 192.168.1.12)

```ini
# /etc/keepalived/keepalived.conf on Node 2
global_defs {
    router_id nginx_node_2
    enable_script_security
    script_user root
}

vrrp_script check_nginx_alive {
    script "/usr/bin/killall -0 nginx"
    interval 1
    weight -20
    fall 2
    rise 2
}

vrrp_instance VI_EDGE {
    state BACKUP
    interface eth0
    virtual_router_id 51
    priority 100 # Lower priority than Node 1
    advert_int 1

    authentication {
        auth_type PASS
        auth_pass Secr3tVRRPK3y
    }

    virtual_ipaddress {
        192.168.1.50/24 dev eth0 label eth0:vip
    }

    track_script {
        check_nginx_alive
    }

    notify_master "/opt/scripts/notify.sh MASTER"
    notify_backup "/opt/scripts/notify.sh BACKUP"
    notify_fault  "/opt/scripts/notify.sh FAULT"
}
```

---

### Step 4: Configure NGINX on Both Nodes

Ensure NGINX binds to the Virtual IP cleanly on both machines:

```nginx
# /etc/nginx/conf.d/app.conf (Identical on both nodes)
server {
    listen 192.168.1.50:80;
    listen 192.168.1.50:443 ssl;
    server_name app.example.com;

    ssl_certificate /etc/nginx/ssl/fullchain.pem;
    ssl_certificate_key /etc/nginx/ssl/privkey.pem;

    location / {
        proxy_pass http://backend_cluster;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

---

## 6. Active-Active Dual-VIP Topology

In standard active-passive setups, Node 2 sits completely idle, wasting 50% of available server hardware. To achieve **Active-Active** operation, configure **Dual VIPs** where each node acts as Master for one VIP and Backup for the other:

```
+-------------------------------------------------------------------------------+
| Active-Active Dual-VIP Topology                                               |
|                                                                               |
|  DNS Round-Robin / GeoDNS:                                                    |
|  app.example.com --> Resolves to VIP 1 (192.168.1.50) & VIP 2 (192.168.1.51)  |
|                                                                               |
|  [Node 1]                                     [Node 2]                        |
|  - VIP 1: MASTER (Priority 101)               - VIP 1: BACKUP (Priority 100)  |
|  - VIP 2: BACKUP (Priority 100)               - VIP 2: MASTER (Priority 101)  |
|                                                                               |
|  Normal Operation: Both nodes actively process 50% of production traffic!     |
|                                                                               |
|  Failover State (If Node 1 dies):                                             |
|  - Node 2 assumes ownership of BOTH VIP 1 and VIP 2.                          |
|  - Handles 100% of traffic until Node 1 recovers.                             |
+-------------------------------------------------------------------------------+
```

---

## 7. Split-Brain Scenarios & Defenses

A **Split-Brain** condition occurs when the network connection between Node 1 and Node 2 is severed, but both machines and NGINX processes remain healthy:
- Node 2 stops receiving VRRP advertisements from Node 1.
- Node 2 assumes Node 1 has died and promotes itself to Master.
- **The Disaster**: Both Node 1 and Node 2 assign the same IP address (`192.168.1.50`) to their interfaces and send Gratuitous ARPs simultaneously. Switches thrash ARP tables, TCP connections drop intermittently, and half the traffic routes to Node 1 while the other half routes to Node 2.

### Defenses Against Split-Brain:
1. **Unicast VRRP Instead of Multicast**: In cloud environments or virtualized networks (AWS, GCP, VMware) where multicast traffic is dropped or unreliable, configure explicit unicast peers:
   ```ini
   vrrp_instance VI_EDGE {
       unicast_src_ip 192.168.1.11
       unicast_peer {
           192.168.1.12
       }
   }
   ```
2. **Dual Network Heartbeats**: Run VRRP heartbeats over an isolated, dedicated cross-connect crossover cable or secondary NIC, independent of public data traffic.
3. **Third-Party Quorum / Fencing**: In advanced clusters, integrate Corosync/Pacemaker with external STONITH (Shoot The Other Node In The Head) fencing agents to physically power down unresponsive nodes.

---

## 8. Common Pitfalls & Anti-Patterns

| Anti-Pattern / Mistake | Root Cause | Observable Production Impact | Correct Solution |
| :--- | :--- | :--- | :--- |
| Forgetting `net.ipv4.ip_nonlocal_bind = 1` | Kernel forbids binding to unassigned VIP on Backup node | NGINX crashes on Backup startup; failover fails | Enable `ip_nonlocal_bind=1` via `/etc/sysctl.d/` |
| Duplicate `virtual_router_id` on LAN | Another team configured VRRP with the same router ID (e.g. 51) | Priority conflicts; both nodes oscillate between Master and Backup | Assign unique `virtual_router_id` across the entire subnet |
| Using Unauthenticated Multicast | Default VRRP multicast on shared network | Rogue devices can inject bogus VRRP packets and hijack VIP | Use `auth_type PASS` and configure unicast peer IPs |
| Overly Aggressive Check Scripts | Health check script checking external websites (e.g. `ping google.com`)| External network flap triggers constant internal cluster failovers | Check strictly local NGINX process state (`killall -0 nginx`)|
| Asymmetric NGINX Configurations | Configuration changes deployed to Node 1 but forgotten on Node 2 | When failover occurs, features, certs, or routes are missing | Synchronize configurations automatically via GitOps / Ansible |
| Using Multicast in Cloud Environments | AWS VPC and GCP VPC drop Layer 2 multicast traffic | Backup node never sees heartbeats; instant permanent split-brain | Use unicast VRRP or cloud native load balancers (NLB/ALB) |

---

## 9. Architectural Comparison: Keepalived VRRP vs DNS Round-Robin vs BGP Anycast

| Dimension | Keepalived + VRRP | DNS Round-Robin | BGP Anycast |
| :--- | :--- | :--- | :--- |
| **Failover Latency** | **1 to 3 seconds** | 5 to 30 minutes (DNS TTL caching) | Sub-second BGP convergence |
| **Network Layer** | Layer 2 / Layer 3 (LAN) | Application Layer (DNS) | Layer 3 / Layer 4 (Internet WAN) |
| **Geographic Scope** | Single data center / subnet | Global internet | Global internet |
| **Infrastructure Need**| Standard Linux VMs/bare metal| Authoritative DNS server | Autonomous System (ASN), BGP routers |
| **Cost & Complexity** | Very Low (Open source) | Low | High (Enterprise ISP contracts) |
| **Load Distribution** | Active-Passive (or Dual VIP)| Active-Active (Random) | Active-Active (Nearest network path) |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. How does a Gratuitous ARP (GARP) packet allow Keepalived to switch traffic instantly during a failover?</strong></summary>

Under standard network operations, upstream switches and client devices store a mapping between an IP address and a physical MAC address in their ARP cache tables. When Node 2 assumes the Master role for the Virtual IP, the VIP moves from Node 1's MAC address to Node 2's MAC address. Node 2 immediately broadcasts a Gratuitous ARP packet. This packet informs the upstream network switch and gateway: *"The MAC address for IP 192.168.1.50 is now Node 2's MAC address."* The switch updates its internal MAC forwarding table in milliseconds, routing subsequent network frames directly to Node 2 without waiting for old ARP cache timers to expire.
</details>

<details>
<summary><strong>2. Why is `net.ipv4.ip_nonlocal_bind = 1` mandatory for NGINX HA clusters?</strong></summary>

In an active-passive cluster, NGINX is configured to listen on the shared Virtual IP address (`listen 192.168.1.50:443`). However, only the Master node physically possesses this IP on its network interface. On the Backup node, the IP does not exist until a failover occurs. Standard Linux kernel security prevents any application from binding a socket to an IP address that is not assigned to a local interface (`EADDRNOTAVAIL`). Without `ip_nonlocal_bind = 1`, NGINX on the Backup node crashes on startup. Enabling it allows NGINX to bind and run in standby, ready to accept connections instantly the moment Keepalived assigns the VIP.
</details>

<details>
<summary><strong>3. What is a "Split-Brain" condition in a VRRP cluster and how do you resolve it?</strong></summary>

A split-brain condition occurs when communication between the Master and Backup nodes is severed (e.g. cross-connect switch failure), while both nodes remain fully functional. Because the Backup node stops receiving heartbeats, it promotes itself to Master. Both nodes now claim ownership of the same VIP and broadcast conflicting Gratuitous ARPs. This causes erratic routing and dropped sessions. Mitigations include:
1. Using explicit **unicast VRRP** rather than multicast.
2. Running heartbeat signals over redundant physical network paths.
3. Using an odd-numbered quorum manager (e.g. Corosync) with automated node fencing (STONITH).
</details>

<details>
<summary><strong>4. How does `vrrp_script` prioritize node election in Keepalived?</strong></summary>

Keepalived assigns a base numerical priority to each node (e.g. Node 1 has 101, Node 2 has 100). The `vrrp_script` periodically executes a health check (e.g., verifying NGINX is running). If the check fails on the Master node, Keepalived adjusts the node's priority by the configured weight (e.g. `weight -20`, reducing Node 1's priority from 101 to 81). Because Node 1's priority (81) is now lower than Node 2's priority (100), Node 1 voluntarily relinquishes the Master state and triggers an orderly transition of the VIP to Node 2.
</details>

<details>
<summary><strong>5. Can Keepalived be used inside public cloud environments like AWS EC2 or Google Cloud?</strong></summary>

Standard VRRP cannot function in native AWS VPC or GCP environments because cloud software-defined networks (SDN) completely drop Layer 2 multicast and broadcast traffic, preventing VRRP heartbeats and Gratuitous ARPs from reaching virtual switches. In public clouds:
1. VRRP can be configured using explicit **unicast** mode for heartbeats.
2. However, assigning the VIP requires a `notify_master` script that calls the cloud API (e.g. `aws ec2 assign-private-ip-addresses`) to reassign the secondary private IP across network interfaces.
3. In modern cloud architectures, teams prefer native cloud load balancers (AWS ALB/NLB, GCP Cloud Load Balancing) over Keepalived.
</details>

<details>
<summary><strong>6. What is the difference between Preemption (`preempt`) and Non-Preemption (`nopreempt`) in Keepalived?</strong></summary>

- **Preempt (default)**: If Node 1 (priority 101) crashes and Node 2 takes over, when Node 1 recovers and boots back up, its higher priority causes it to immediately reclaim the Master role, forcing the VIP to switch back.
- **Nopreempt**: When Node 1 recovers, it remains in the Backup state as long as Node 2 is healthy, avoiding an unnecessary secondary failover switch and minimizing session disruptions. Production clusters frequently use `nopreempt`.
</details>

<details>
<summary><strong>7. How do you monitor that VRRP failover is functioning correctly without taking down production NGINX?</strong></summary>

Execute a controlled manual failover during a maintenance window:
1. Stop Keepalived on Node 1: `sudo systemctl stop keepalived`.
2. Observe Node 2 logs: `journalctl -u keepalived -f`. Verify transition to `MASTER` state within 1 second.
3. Continuously send ping and curl requests to the VIP from a separate machine to verify zero dropped packets.
4. Restart Keepalived on Node 1.
</details>

<details>
<summary><strong>8. What is the purpose of `enable_script_security` in Keepalived?</strong></summary>

`enable_script_security` prevents Keepalived from executing `vrrp_script` tracking scripts as the `root` user unless explicitly specified. This defends against privilege escalation attacks where an attacker with write access to the script file could achieve arbitrary root execution via Keepalived.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Verify and Enable Kernel Non-Local Binding</strong></summary>

**Goal**: Configure the Linux kernel to allow NGINX to bind to inactive VIPs.
1. Inspect current setting:
```bash
cat /proc/sys/net/ipv4/ip_nonlocal_bind
```
2. Enable dynamically:
```bash
sudo sysctl -w net.ipv4.ip_nonlocal_bind=1
```
3. Test binding NGINX to an unassigned IP address (e.g. `listen 192.168.1.99:8080;`) and verify that `nginx -t` succeeds.
</details>

<details>
<summary><strong>Exercise 2: Setup a Two-Node Keepalived Cluster with Vagrant or VMs</strong></summary>

**Goal**: Deploy and observe active-passive VIP failover.
1. Spin up two Linux VMs on a shared private network (`192.168.56.11` and `192.168.56.12`).
2. Install Keepalived on both: `sudo apt-get install keepalived`.
3. Apply the Master and Backup configurations from Section 5.
4. Verify VIP assignment on Node 1: `ip addr show eth0`. Confirm `192.168.1.50` is active.
5. Verify Node 2 does NOT have the VIP assigned.
</details>

<details>
<summary><strong>Exercise 3: Simulate an NGINX Crash and Measure Failover Time</strong></summary>

**Goal**: Observe Keepalived tracking script behavior.
1. From a client machine, run continuous pings: `ping 192.168.1.50`.
2. On Node 1, terminate NGINX: `sudo systemctl stop nginx`.
3. Observe Node 1 logs: `vrrp_script check_nginx_alive` fails, reducing priority.
4. Observe Node 2 logs: transitions to `MASTER` and binds the VIP.
5. Check client ping: confirm only 1 or 2 packet drops occurred during failover.
</details>

<details>
<summary><strong>Exercise 4: Test Unicast VRRP Configuration</strong></summary>

**Goal**: Convert multicast VRRP to unicast for restricted networks.
1. Modify `keepalived.conf` on both nodes: add `unicast_src_ip` and `unicast_peer` blocks.
2. Reload Keepalived on both nodes.
3. Capture VRRP packets with tcpdump:
```bash
sudo tcpdump -i eth0 -nn 'ip proto 112'
```
4. Confirm packets are point-to-point unicast between node IPs rather than multicast `224.0.0.18`.
</details>

<details>
<summary><strong>Exercise 5: Author a Failover Notification Script</strong></summary>

**Goal**: Send alerts when state transitions occur.
1. Create `/opt/scripts/notify.sh`:
```bash
#!/bin/bash
STATE=$1
echo "$(date): Keepalived transitioned to $STATE" >> /var/log/keepalived-state.log
```
2. Make executable: `chmod +x /opt/scripts/notify.sh`.
3. Link into `notify_master`, `notify_backup`, and `notify_fault`.
4. Trigger failover and inspect `/var/log/keepalived-state.log`.
</details>

<details>
<summary><strong>Exercise 6: Test Non-Preemptive Failover Behavior</strong></summary>

**Goal**: Prevent unnecessary VIP bounce-back.
1. Add `nopreempt` to `vrrp_instance` on both nodes.
2. Kill NGINX on Node 1 (failover to Node 2).
3. Restart NGINX on Node 1.
4. Observe that the VIP remains on Node 2, preventing a secondary traffic disruption.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| Keepalived & VRRP High Availability Reference                                  |
|                                                                                |
| Directive / Parameter         | Context         | Purpose                      |
| :---------------------------- | :-------------- | :--------------------------- |
| net.ipv4.ip_nonlocal_bind = 1 | sysctl          | Allows binding inactive VIP  |
| vrrp_script <name>            | keepalived.conf | Defines process health check |
| virtual_router_id <ID>        | vrrp_instance   | Shared cluster identifier    |
| priority <num>                | vrrp_instance   | Election weight (101 vs 100) |
| advert_int 1                  | vrrp_instance   | Heartbeat frequency (sec)    |
| virtual_ipaddress { <VIP> }   | vrrp_instance   | Managed floating IP address  |
| unicast_peer { <IP> }         | vrrp_instance   | Point-to-point VRRP mode     |
| nopreempt                     | vrrp_instance   | Prevents bounce-back failover|
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Keepalived Official Documentation](https://www.keepalived.org/documentation.html)
- [RFC 5798: Virtual Router Redundancy Protocol (VRRP) Version 3](https://datatracker.ietf.org/doc/html/rfc5798)
- [Red Hat Enterprise Linux: Configuring High Availability with Keepalived](https://access.redhat.com/)
- [Next Concept: CDN & Edge Integration Architecture](07-cdn-and-edge-integration.md)

---

[← Previous Note: 05-monitoring-metrics-and-observability.md](05-monitoring-metrics-and-observability.md) | [Back to Module Index](README.md) | [Next Note: 07-cdn-and-edge-integration.md →](07-cdn-and-edge-integration.md)
