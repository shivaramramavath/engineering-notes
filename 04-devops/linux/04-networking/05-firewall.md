# Linux Firewalls: Nftables, UFW, Firewalld, and Connection Tracking

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Packet Filtering, `nftables`, `ufw`, `firewalld`, NAT, and Netfilter `conntrack` |
| **Module** | `04-networking` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `04-networking/01-networking-basics.md`, `04-networking/02-ip-routing-and-interfaces.md` |
| **Target Audience** | Security Engineers, SREs, Systems Administrators, DevOps Practitioners |
| **Target Distribution** | Ubuntu 24.04 LTS (`nftables` / `ufw`), Rocky Linux / RHEL 9.4 (`nftables` / `firewalld`) |
| **Key Tools / Files** | `nft`, `ufw`, `firewall-cmd`, `conntrack`, `/etc/nftables.conf`, `/proc/net/nf_conntrack` |

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

Every internet-facing server is continuously bombarded by port scans, brute-force bots, and malicious traffic. The Linux **Netfilter** framework provides in-kernel packet filtering, network address translation (NAT), and packet mangling.

Historically, Linux relied on the monolithic `iptables` suite (`iptables`, `ip6tables`, `arptables`, `ebtables`), which suffered from:
- Code duplication across protocols (separate IPv4 and IPv6 rules).
- Poor linear rule evaluation performance ($O(N)$ linear inspection).
- Inability to update rules atomically without flushing and reloading tables.

**Nftables (`nft`)** is the modern kernel subsystem that completely supersedes `iptables`. It features:
- A compact, bytecode-driven virtual machine in the kernel.
- Unified dual-stack filtering (`inet` family handling IPv4 and IPv6 simultaneously).
- Native dictionary sets and maps for $O(1)$ constant-time IP matching.
- Truly atomic rule updates without dropping existing connection states.

Distributions layer user-friendly management engines on top: Ubuntu provides **UFW (Uncomplicated Firewall)**, while RHEL / Rocky Linux provides **Firewalld**. Mastering both the low-level `nftables` syntax and high-level wrappers is critical for securing cloud nodes, container hosts, and edge routers.

---

## Core Concepts

### The Nftables Architecture Hierarchy
`nftables` organizes rules into a clear hierarchical tree:
1. **Tables**: Top-level containers categorized by address family (`inet`, `ip`, `ip6`, `arp`, `bridge`, `netdev`). The `inet` family inspects both IPv4 and IPv6 packets with single unified rules.
2. **Chains**: Containers for rules attached to specific Netfilter kernel hooks.
   - **Base Chains**: Attached to a Netfilter hook (`prerouting`, `input`, `forward`, `output`, `postrouting`) with an explicit priority and default policy (`accept` or `drop`).
   - **Regular Chains**: User-defined chains used as jump targets to modularize rules.
3. **Rules**: Individual matching expressions and terminal/non-terminal actions (`accept`, `drop`, `reject`, `jump`, `log`, `counter`).
4. **Sets and Maps**: In-memory data structures (hash tables, trees) that store IP addresses, CIDR blocks, or ports for high-performance lookup.

### Netfilter Hooks in the Linux Kernel
Packets traverse 5 core Netfilter kernel hooks:
- `NF_INET_PRE_ROUTING`: Invoked immediately after packet reception, before routing decisions. Ideal for DNAT and early packet drops.
- `NF_INET_LOCAL_IN`: Invoked for packets destined for local processes on this machine.
- `NF_INET_FORWARD`: Invoked for packets being routed through this host to another destination (routers, Docker bridges).
- `NF_INET_LOCAL_OUT`: Invoked for packets originated by local processes.
- `NF_INET_POST_ROUTING`: Invoked after routing decisions, right before the packet leaves the physical interface. Ideal for SNAT and Masquerading.

### Connection Tracking (`conntrack`)
Stateful packet inspection tracks bidirectional conversation flows using 4 primary states:
- `NEW`: The initial packet creating a connection (e.g., incoming TCP `SYN`).
- `ESTABLISHED`: The connection has been successfully established and acknowledged by both peers.
- `RELATED`: A secondary connection associated with an established connection (e.g., FTP data channels, ICMP error messages related to active TCP streams).
- `INVALID`: A packet that does not match any known connection state or violates protocol state rules (e.g., unexpected flags, corrupted checksums).

---

## Internal Working

```
                                  Ingress Network Frame
                                            |
                                            v
+-----------------------------------------------------------------------------------------+
|                                    PREROUTING Hook                                      |
|                             (Raw filter, DNAT, Port Redirection)                       |
+-------------------------------------------+---------------------------------------------+
                                            |
                                            v
+-----------------------------------------------------------------------------------------+
|                                  Routing Decision (FIB)                                 |
|                       Is packet destined for local host or forwarding?                  |
+-------------------------------------------+---------------------------------------------+
                                            |
                     +----------------------+----------------------+
                     | Destined for Local                           | Destined for Forward
                     v                                             v
+-----------------------------------------+     +-----------------------------------------+
|                INPUT Hook               |     |              FORWARD Hook               |
|      (Local Firewall, Drop Bad IPs)     |     |     (Routed / Container / VM Traffic)   |
+--------------------+--------------------+     +--------------------+--------------------+
                     |                                               |
                     v                                               |
+-----------------------------------------+                          |
|         Local Socket / Application      |                          |
|           (Nginx, SSHD, Postgres)       |                          |
+--------------------+--------------------+                          |
                     |                                               |
                     v                                               |
+-----------------------------------------+                          |
|               OUTPUT Hook               |                          |
|      (Locally Generated Outbound Traffic)                          |
+--------------------+--------------------+                          |
                     |                                               |
                     +----------------------+------------------------+
                                            |
                                            v
+-----------------------------------------------------------------------------------------+
|                                   POSTROUTING Hook                                      |
|                           (SNAT, Masquerade, Egress Filtering)                          |
+-------------------------------------------+---------------------------------------------+
                                            |
                                            v
                                   Egress Physical Wire
```

### The $O(1)$ Hash Table Set Lookup in Nftables
In legacy `iptables`, blocking 10,000 malicious IPs required evaluating 10,000 sequential rules for every incoming packet ($O(N)$ CPU latency). In `nftables`, IP addresses or subnets are stored in kernel **Sets** using Judy arrays or Red-Black Trees:
```text
nft add set inet filter blackhole { type ipv4_addr; flags interval; }
nft add rule inet filter input ip saddr @blackhole drop
```
The kernel executes a single $O(1)$ or $O(\log N)$ hash lookup, evaluating hundreds of thousands of IPs in nanoseconds without spiking CPU utilization.

---

## Syntax & CLI Tools

### Modern `nft` Syntax
```bash
# View all active tables, chains, and rules
sudo nft list ruleset

# Flush entire ruleset (Caution: drops all firewall rules!)
sudo nft flush ruleset

# Create table in the 'inet' family (IPv4 + IPv6)
sudo nft add table inet filter

# Create base INPUT chain with default DROP policy
sudo nft add chain inet filter input '{ type filter hook input priority 0; policy drop; }'

# Allow loopback traffic
sudo nft add rule inet filter input iif "lo" accept

# Allow established and related connections
sudo nft add rule inet filter input ct state established,related accept

# Drop invalid packets immediately
sudo nft add rule inet filter input ct state invalid drop

# Open SSH (port 22) and HTTPS (port 443)
sudo nft add rule inet filter input tcp dport { 22, 443 } accept

# Delete a specific rule by its handle number
sudo nft -a list chain inet filter input
sudo nft delete rule inet filter input handle 4
```

### Ubuntu `ufw` (Uncomplicated Firewall)
```bash
# Check status with numbered rules
sudo ufw status verbose
sudo ufw status numbered

# Set default policies (Drop incoming, Allow outgoing)
sudo ufw default deny incoming
sudo ufw default allow outgoing

# Allow specific ports and services
sudo ufw allow 22/tcp
sudo ufw allow http
sudo ufw allow https

# Allow traffic from specific subnet to specific port
sudo ufw allow from 192.168.1.0/24 to any port 5432 proto tcp

# Delete rule by number
sudo ufw delete 3

# Enable or disable UFW
sudo ufw enable
sudo ufw reload
```

### RHEL / Rocky Linux `firewalld` (`firewall-cmd`)
```bash
# View active zones and assigned interfaces
sudo firewall-cmd --get-active-zones

# View complete rules for the default zone (usually 'public')
sudo firewall-cmd --list-all

# Open ports persistently
sudo firewall-cmd --permanent --add-port=443/tcp
sudo firewall-cmd --permanent --add-service=ssh

# Reload firewall to apply permanent changes
sudo firewall-cmd --reload

# Add a rich rule (rate limiting or source IP filtering)
sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="192.168.10.0/24" service name="postgresql" accept'
```

---

## Progressive Practical Examples

### Example 1: Building a Production-Grade Minimal `nftables.conf`
A clean, hardened, dual-stack firewall configuration file located at `/etc/nftables.conf`.

```text
#!/usr/sbin/nft -f

# Flush existing ruleset
flush ruleset

# Define 'filter' table for dual-stack IPv4 & IPv6
table inet filter {
    # Ingress Filter Chain
    chain input {
        type filter hook input priority 0; policy drop;

        # 1. Accept all loopback traffic
        iif "lo" accept

        # 2. Drop invalid connection states
        ct state invalid drop

        # 3. Accept established and related traffic (essential for outbound replies)
        ct state established,related accept

        # 4. Accept ICMP (ping) and IPv6 Neighbor Discovery with rate limiting
        ip protocol icmp icmp type { echo-request, destination-unreachable, time-exceeded } limit rate 5/second accept
        ip6 nexthdr icmpv6 icmpv6 type { echo-request, destination-unreachable, time-exceeded, packet-too-big, nd-router-solicit, nd-router-advert, nd-neighbor-solicit, nd-neighbor-advert } limit rate 10/second accept

        # 5. Public Application Services
        tcp dport 22 accept comment "OpenSSH"
        tcp dport { 80, 443 } accept comment "Web Traffic (HTTP/HTTPS)"

        # 6. Log and drop remaining unsolicited packets (capped to prevent log spam)
        limit rate 3/minute burst 5 packets log prefix "[NFT-DROPPED]: " level info
        drop
    }

    # Forwarding Chain (Drop all unless router/container node)
    chain forward {
        type filter hook forward priority 0; policy drop;
    }

    # Egress Chain (Allow all outbound traffic)
    chain output {
        type filter hook output priority 0; policy accept;
    }
}
```

```bash
# Validate syntax and load atomically:
sudo nft -c -f /etc/nftables.conf
sudo nft -f /etc/nftables.conf
sudo systemctl enable --now nftables
```

### Example 2: Implementing Dynamic IP Blacklists with Sets and Timeouts
Mitigate brute-force attacks by dynamically adding offensive IPs to an auto-expiring in-kernel set.

```bash
# 1. Create table and a dynamic set that holds IPv4 addresses with automatic timeouts
sudo nft add table inet filter
sudo nft add set inet filter attackers '{ type ipv4_addr; flags timeout; }'

# 2. Add rule at the top of INPUT chain to drop blacklisted IPs
sudo nft insert rule inet filter input ip saddr @attackers drop

# 3. Add an offending IP to the blacklist with a 1-hour expiration timeout
sudo nft add element inet filter attackers '{ 203.0.113.88 timeout 1h }'

# 4. Inspect active elements in the set with remaining time
sudo nft list set inet filter attackers
```

### Example 3: Configuring Network Address Translation (NAT) and Masquerading
Turn a Linux VM with two interfaces into an internet gateway/router for a private network.

```bash
#!/usr/bin/env bash
set -euo pipefail

WAN_IFACE="enp3s0"
LAN_IFACE="enp4s0"

# 1. Enable IP forwarding in kernel
sudo sysctl -w net.ipv4.ip_forward=1

# 2. Create NAT table and chains
sudo nft add table ip nat
sudo nft add chain ip nat postrouting '{ type nat hook postrouting priority 100; policy accept; }'
sudo nft add chain ip nat prerouting  '{ type nat hook prerouting priority -100; policy accept; }'

# 3. Add Masquerading rule for outbound internet traffic
sudo nft add rule ip nat postrouting oif "${WAN_IFACE}" masquerade

# 4. Forward traffic between LAN and WAN
sudo nft add table inet filter
sudo nft add chain inet filter forward '{ type filter hook forward priority 0; policy drop; }'
sudo nft add rule inet filter forward iif "${LAN_IFACE}" oif "${WAN_IFACE}" accept
sudo nft add rule inet filter forward iif "${WAN_IFACE}" oif "${LAN_IFACE}" ct state established,related accept

# 5. Port Forwarding (DNAT): Forward external port 8080 to internal host 192.168.10.50:80
sudo nft add rule ip nat prerouting iif "${WAN_IFACE}" tcp dport 8080 dnat to 192.168.10.50:80
sudo nft add rule inet filter forward ip daddr 192.168.10.50 tcp dport 80 accept
```

### Example 4: Hardening with Rate Limiting (Mitigating SSH Brute Force)
Throttle incoming SSH connection attempts to no more than 3 per minute from any single IP.

```bash
# Using nftables meter/set:
sudo nft add rule inet filter input tcp dport 22 ct state new \
  meter ssh-meter '{ ip saddr limit rate over 3/minute burst 2 packets }' \
  drop
```

### Example 5: Auditing Connection Tracking Table and Limits
When servers face high connection counts, the `conntrack` table can exhaust, dropping all new connections.

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== Conntrack Table Health Audit ==="
CURRENT_CONN=$(cat /proc/sys/net/netfilter/nf_conntrack_count)
MAX_CONN=$(cat /proc/sys/net/netfilter/nf_conntrack_max)
USAGE_PCT=$(( CURRENT_CONN * 100 / MAX_CONN ))

echo "Active Tracked Connections: ${CURRENT_CONN}"
echo "Maximum Allowed Connections: ${MAX_CONN}"
echo "Conntrack Table Utilization: ${USAGE_PCT}%"

if [ "${USAGE_PCT}" -gt 80 ]; then
    echo "WARNING: Conntrack table over 80% capacity! Tuning max limits..."
    sudo sysctl -w net.netfilter.nf_conntrack_max=524288
fi
```

---

## Best Practices

1. **Standardize on `inet` Family in Nftables**: Avoid maintaining separate IPv4 and IPv6 rule files. Use `table inet <name>` to apply single rules across both address families simultaneously.
2. **Always Permit Established/Related Traffic First**: Place `ct state established,related accept` near the very top of your input chain. In typical workloads, 95%+ of incoming packets belong to already established streams; evaluating this rule first minimizes CPU cycle overhead per packet.
3. **Never Block Essential ICMPv6 Messages**: Blocking all ICMP in IPv6 breaks Path MTU discovery (`packet-too-big`), Neighbor Discovery (`nd-neighbor-solicit`), and Router Advertisement, completely severing IPv6 connectivity.
4. **Use Atomic File Loads via `nft -f`**: Never configure production firewalls by issuing sequential CLI commands over SSH. If you issue `policy drop` before `accept 22`, you lock yourself out instantly. Write rules to a configuration file and load atomically: `nft -f /etc/nftables.conf`.
5. **Always Set Rule Comments**: Document rule intent inline using the `comment` keyword (e.g., `tcp dport 9090 accept comment "Prometheus metrics scraper"`).
6. **Monitor `conntrack_count` on Busy Reverse Proxies & NAT Gateways**: Size `net.netfilter.nf_conntrack_max` appropriately for high-throughput environments (e.g., 262,144 or 524,288) to prevent dropped `NEW` connection packets.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Running `iptables` commands while `nftables` is active | Modifies legacy translation shim; causes rule conflicts and unexpected packet bypasses. | Standardize strictly on `nft` syntax or pure `ufw`/`firewalld`. |
| Setting INPUT policy to `drop` before allowing SSH port 22 | Immediately terminates the active SSH session and locks the administrator out. | Add `tcp dport 22 accept` before changing chain default policy. |
| Forgetting `iif "lo" accept` on loopback | Local daemons (Prometheus, Postgres, Redis, systemd-resolved) fail to communicate over `127.0.0.1`. | Always include loopback permit rule at the top of the INPUT chain. |
| Dropping all ICMP echo-requests (ping) | Breaks Path MTU Discovery (PMTUD) and network troubleshooting diagnostics. | Rate limit ICMP rather than dropping it entirely. |
| Editing UFW rules while Docker is running without `DOCKER-USER` chain awareness | Docker manipulates iptables/nftables directly, exposing container ports publicly even if UFW blocks them! | Route Docker port protections through the `DOCKER-USER` chain or disable Docker iptables integration. |
| Forgetting `systemctl enable nftables` after writing config | All firewall rules disappear upon the next host reboot. | Enable systemd service: `sudo systemctl enable --now nftables`. |

---

## Performance, Security & Cost Implications

### Performance
- **Bytecode Virtual Machine Execution**: Nftables compiles rules into compact BPF-like bytecode executed inside the kernel. Unlike `iptables`, which traversed long linked lists, `nftables` groups multi-port matches (`tcp dport { 80, 443, 8080 }`) into a single instruction, drastically reducing packet processing overhead.
- **Set Lookup Speed**: In IP blacklisting, switching from 5,000 individual `iptables -A INPUT -s <ip> -j DROP` rules to a single `nftables` set drops CPU overhead by **over 90%**.

### Security
- **Bypassing Firewalls via Docker**: A common security vulnerability in DevOps is deploying Docker with standard ports (`docker run -p 8080:8080`). Docker injects NAT `PREROUTING` rules before UFW or OS firewall filter chains execute. External attackers can reach port 8080 even if `ufw status` shows `8080` is denied. Secure container ports by binding exclusively to localhost: `127.0.0.1:8080:8080`.

### Cost
- Implementing DDoS rate limiting and GeoIP filtering directly in kernel `nftables` prevents application layer servers (Node.js, Python, Java) from saturating CPU cores under denial-of-service floods, avoiding unnecessary auto-scaling compute expenses.

---

## Comparison Table / Alternatives

| Feature / Tool | `nftables` (`nft`) | `ufw` | `firewalld` | Legacy `iptables` |
| :--- | :--- | :--- | :--- | :--- |
| **Abstraction Level** | Core Kernel Direct Engine | High-Level Simple Wrapper | Dynamic Zone-Based Manager | Monolithic Legacy Tool |
| **Dual-Stack (IPv4/v6)** | Native single rule (`inet`) | Supported (auto-duplicates) | Supported (auto-duplicates) | Separate tools (`iptables` vs `ip6tables`) |
| **Sets & Maps ($O(1)$)** | Built-in native syntax | Not natively exposed | Supported via IPsets | Requires external `ipset` utility |
| **Dynamic Updates** | Atomic transactions | Flushes / Reloads | Dynamic D-Bus API | Non-atomic table swapping |
| **Primary Distro** | Universal Modern Linux | Ubuntu / Debian default | RHEL / Rocky / Fedora default | Deprecated on modern Linux |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: Why does Docker bypass UFW rules and expose ports to the public internet?
<details>
<summary>View Solution</summary>

**Root Cause**: When Docker publishes a port (`-p 8080:80`), it inserts `PREROUTING` NAT rules and rules in the `DOCKER` chain in Netfilter. Packets destined for container ports are routed and forwarded before UFW's `INPUT` filter chain is ever evaluated. Because UFW only filters traffic in its standard `ufw-user-input` chain, it never inspects packets destined for Docker's bridge (`docker0`).

**Fix**:
1. Bind ports to loopback only: `docker run -p 127.0.0.1:8080:80 ...`
2. Or use the official `DOCKER-USER` chain: Docker routes forwarded packets through `DOCKER-USER` before container forwarding. Add custom filtering rules there.
</details>

### Q2: How do you identify which rule in `nftables` is dropping your packets?
<details>
<summary>View Solution</summary>

1. **Add logging with a unique prefix**:
   ```bash
   sudo nft add rule inet filter input log prefix "[DROP-DEBUG]: " level info
   ```
2. **Inspect kernel logs**:
   ```bash
   sudo dmesg -w | grep "[DROP-DEBUG]"
   # Or via journalctl:
   sudo journalctl -k -f | grep "[DROP-DEBUG]"
   ```
3. **Use packet trace mode**:
   Nftables has built-in tracing. Mark the packet in prerouting:
   ```bash
   sudo nft add chain inet filter trace_chain '{ type filter hook prerouting priority -300; }'
   sudo nft add rule inet filter trace_chain ip saddr <your_ip> meta nftrace set 1
   sudo nft monitor trace
   ```
</details>

### Q3: What happens when the Netfilter Connection Tracking table reaches `nf_conntrack_max`?
<details>
<summary>View Solution</summary>

When the conntrack table fills up, the Linux kernel cannot allocate state tracking entries for incoming packets. 
- The kernel logs `nf_conntrack: table full, dropping packet`.
- It **drops all new incoming and outgoing connection attempts** silently.
- Existing established connections might continue, but any packet needing state allocation fails.

**Fix**:
Increase maximum table size and hash bucket size:
```bash
sudo sysctl -w net.netfilter.nf_conntrack_max=524288
echo 65536 | sudo tee /sys/module/nf_conntrack/parameters/hashsize
```
</details>

### Q4: Explain the difference between `DROP` and `REJECT` targets.
<details>
<summary>View Solution</summary>

- **`DROP`**: Silently discards the packet without sending any notification back to the sender. The sender must wait until its retransmission timer expires (`ETIMEDOUT`). Ideal for public internet interfaces to avoid confirming host existence or giving clues to port scanners.
- **`REJECT`**: Discards the packet but actively sends an error packet back to the sender (TCP `RST` for TCP, or ICMP Port Unreachable for UDP). The sender terminates the attempt immediately (`ECONNREFUSED`). Ideal for internal corporate networks to provide fast feedback to friendly applications.
</details>

### Q5: How does `nftables` achieve atomic configuration reloads?
<details>
<summary>View Solution</summary>

`nftables` communicates with the kernel using a Netlink transaction API (`NFNL_SUBSYS_NFTABLES`). When you load a configuration file (`nft -f /etc/nftables.conf`):
1. The userspace parser sends the entire sequence of operations as a single batch transaction.
2. The kernel verifies all chains, sets, and rules in memory.
3. If valid, it commits the entire batch atomically in a single pointer swap.
4. If any single rule fails syntax or validation, the entire batch is aborted, leaving the running firewall completely untouched.
</details>

### Q6: What is the purpose of the `ct state established,related accept` rule?
<details>
<summary>View Solution</summary>

Stateful firewalls must allow return traffic for outbound requests. For example, when your server runs `apt update` or `curl https://api.github.com`, outbound packets leave on ephemeral ports. 
Without `established,related accept`, the incoming HTTP response packets would be blocked by the `INPUT` chain because incoming port rules only open server ports (like 22 or 443). This rule permits all incoming packets that belong to connections already initiated by the local host.
</details>

### Q7: What is SNAT vs Masquerade in NAT configurations?
<details>
<summary>View Solution</summary>

Both modify the source IP of egress packets.
- **SNAT (Source NAT)**: Replaces the source IP with a **static, predetermined IP address** (`snat to 203.0.113.5`). Very fast because the kernel does not query interface addresses on every packet.
- **Masquerade**: Dynamically queries the primary IP address of the egress interface for each new connection. Essential for interfaces with dynamic DHCP or PPPoE addresses where the IP can change.
</details>

### Q8: How can you list rules in `nftables` along with their internal handles for deletion?
<details>
<summary>View Solution</summary>

Use the `-a` (`--handle`) flag:
```bash
sudo nft -a list table inet filter
```
Each rule outputs `# handle <number>` at the end. You can delete that specific rule without flushing the entire chain:
```bash
sudo nft delete rule inet filter input handle 12
```
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Inspecting Active Firewalls and Rulesets
**Task**: Check which firewall framework (`nftables`, `ufw`, or `firewalld`) is active on your host. Inspect the live ruleset using `nft list ruleset`.
<details>
<summary>View Solution</summary>

```bash
# 1. Check services
systemctl is-active nftables ufw firewalld 2>/dev/null || true

# 2. View in-kernel nftables ruleset
sudo nft list ruleset
```
</details>

### Exercise 2: Creating a Dedicated Sandbox Table and Chain in Nftables
**Task**: Create a test table `inet test_lab` and a regular chain `custom_chain`. Add a rule allowing port 9999 with a counter. Verify the counter, then delete the table cleanly.
<details>
<summary>View Solution</summary>

```bash
# 1. Create table and chain
sudo nft add table inet test_lab
sudo nft add chain inet test_lab custom_chain

# 2. Add rule with packet counter
sudo nft add rule inet test_lab custom_chain tcp dport 9999 counter accept

# 3. View table
sudo nft list table inet test_lab

# 4. Clean up
sudo nft delete table inet test_lab
```
</details>

### Exercise 3: Simulating and Testing Dynamic IP Set Blocking
**Task**: Create an IP set named `blocklist`. Add `127.0.0.2` to it. Create a rule dropping all traffic from `blocklist`. Test pinging `127.0.0.2`.
<details>
<summary>View Solution</summary>

```bash
# 1. Create table, set, and rule
sudo nft add table inet test_drop
sudo nft add set inet test_drop blocklist '{ type ipv4_addr; }'
sudo nft add chain inet test_drop input '{ type filter hook input priority -10; policy accept; }'
sudo nft add rule inet test_drop input ip saddr @blocklist drop

# 2. Add 127.0.0.2 to the set
sudo nft add element inet test_drop blocklist '{ 127.0.0.2 }'

# 3. Test pinging loopback alias
ping -c 2 127.0.0.1  # Succeeds
ping -c 2 127.0.0.2  # Drops/Hangs!

# 4. Clean up
sudo nft delete table inet test_drop
```
</details>

### Exercise 4: Setting up a Basic UFW Configuration
**Task**: On an Ubuntu machine, configure UFW to allow SSH and HTTPS, set default deny on incoming, and verify the status output.
<details>
<summary>View Solution</summary>

```bash
# 1. Ensure SSH is allowed first!
sudo ufw allow 22/tcp

# 2. Allow HTTPS
sudo ufw allow 443/tcp

# 3. Set default incoming policy
sudo ufw default deny incoming
sudo ufw default allow outgoing

# 4. Enable and check status
sudo ufw --force enable
sudo ufw status verbose
```
</details>

### Exercise 5: Monitoring Conntrack Table in Real-Time
**Task**: Inspect the active connection tracking table. Use the `conntrack` CLI tool (install `conntrack` package if necessary) to observe established TCP streams.
<details>
<summary>View Solution</summary>

```bash
# Install tool if missing: sudo apt install -y conntrack
sudo conntrack -L -p tcp --status ESTABLISHED

# Observe real-time connection state transitions
sudo conntrack -E -e NEW,DESTROY
```
</details>

### Exercise 6: Verifying Atomic Configuration Syntax
**Task**: Write a test `nftables` file with an intentional syntax error. Run `nft -c -f <file>` to verify how the dry-run parser catches errors before kernel application.
<details>
<summary>View Solution</summary>

```bash
# Write invalid config
cat << 'EOF' > /tmp/bad_nft.conf
table inet broken {
    chain input {
        type filter hook input priority 0;
        this_is_an_invalid_token drop;
    }
}
EOF

# Test syntax check (-c)
sudo nft -c -f /tmp/bad_nft.conf || echo "Syntax check correctly caught the error!"
rm -f /tmp/bad_nft.conf
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Task | `nftables` Command | `ufw` Command | `firewalld` Command |
| :--- | :--- | :--- | :--- |
| **View Rules** | `sudo nft list ruleset` | `sudo ufw status numbered` | `sudo firewall-cmd --list-all` |
| **Allow Port** | `sudo nft add rule inet filter input tcp dport 80 accept` | `sudo ufw allow 80/tcp` | `sudo firewall-cmd --add-port=80/tcp --permanent` |
| **Deny Port** | `sudo nft add rule inet filter input tcp dport 23 drop` | `sudo ufw deny 23/tcp` | `sudo firewall-cmd --remove-port=23/tcp --permanent` |
| **Allow Subnet** | `sudo nft add rule inet filter input ip saddr 10.0.0.0/24 accept` | `sudo ufw allow from 10.0.0.0/24`| `sudo firewall-cmd --add-source=10.0.0.0/24 --permanent` |
| **Reload Rules**| `sudo nft -f /etc/nftables.conf`| `sudo ufw reload` | `sudo firewall-cmd --reload` |
| **Flush Rules** | `sudo nft flush ruleset` | `sudo ufw reset` | `sudo firewall-cmd --complete-reload` |

### Further Reading
- `man 8 nft` and `man 5 nftables`
- `man 8 ufw` and `man 1 firewall-cmd`
- `man 8 conntrack`
- [Nftables Official Documentation & Wiki](https://wiki.nftables.org/)
- [Netfilter Project Architecture](https://www.netfilter.org/)
- [Linux Kernel Connection Tracking Documentation](https://docs.kernel.org/networking/nf_conntrack-sysctl.html)
