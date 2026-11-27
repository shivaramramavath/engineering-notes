# DNS Resolution and Diagnostic Tools in Linux

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux DNS Resolution, `systemd-resolved`, `resolvectl`, `dig`, `/etc/resolv.conf`, `nsswitch` |
| **Module** | `04-networking` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `04-networking/01-networking-basics.md`, `04-networking/02-ip-routing-and-interfaces.md` |
| **Target Audience** | SREs, Cloud Engineers, DevOps Practitioners, Network Administrators |
| **Target Distribution** | Ubuntu 24.04 LTS (`systemd-resolved`), Rocky Linux / RHEL 9.4 |
| **Key Tools / Files** | `dig`, `resolvectl`, `nslookup`, `host`, `/etc/resolv.conf`, `/etc/nsswitch.conf`, `/etc/hosts` |

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

"It's not DNS. There's no way it's DNS. It was DNS." This ubiquitous sysadmin adage captures the reality that DNS misconfigurations account for a massive percentage of production incidents in cloud environments.

When an application invokes an HTTP endpoint or connects to a database (`postgres.prod.internal:5432`), the Linux operating system must translate that domain name into an IP address. On modern Linux:
- **Glibc Name Service Switch (`/etc/nsswitch.conf`)** determines the resolution sequence (files, DNS, systemd-resolved, mDNS).
- **`systemd-resolved`** acts as a local caching resolver, maintaining per-link DNS configurations, split-horizon search domains, and DNS-over-TLS (DoT).
- **`/etc/resolv.conf`** serves as the universal API stub file consumed by standard C libraries and container engines.

Mastering DNS mechanics, debugging query timeouts with `dig` trace outputs, and configuring split-horizon internal routing prevents severe latency penalties and connection outages across Kubernetes clusters and multi-VPC cloud networks.

---

## Core Concepts

### The Name Service Switch (`/etc/nsswitch.conf`)
When an application calls `getaddrinfo()` or `gethostbyname()`, the GNU C Library (glibc) consults the `hosts:` directive inside `/etc/nsswitch.conf`:
```text
hosts: files mdns4_minimal [NOTFOUND=return] resolve [!UNAVAIL=return] dns
```
- `files`: Inspects local static entries in `/etc/hosts`.
- `resolve`: Routes queries through the local `systemd-resolved` D-Bus interface.
- `dns`: Traditional fallback querying nameservers defined in `/etc/resolv.conf`.

### `systemd-resolved` and `/etc/resolv.conf` Symlinks
In modern Ubuntu and Debian distributions, `systemd-resolved` runs a local caching stub listener at `127.0.0.53:53`.
The file `/etc/resolv.conf` is typically a symbolic link pointing to one of two modes:
1. **Stub Resolver Mode (Recommended)**:  
   `/etc/resolv.conf -> ../run/systemd/resolve/stub-resolv.conf`  
   Points upstream applications to `nameserver 127.0.0.53`, allowing `systemd-resolved` to handle caching, per-interface split-DNS, and DNSSEC validation.
2. **Direct Upstream Mode**:  
   `/etc/resolv.conf -> ../run/systemd/resolve/resolv.conf`  
   Bypasses the local stub listener and writes actual external nameservers (e.g., cloud DHCP servers, `1.1.1.1`) directly into `/etc/resolv.conf`.

### Standard DNS Record Types
- **A**: Maps a hostname to an IPv4 address (`api.example.com -> 93.184.216.34`).
- **AAAA**: Maps a hostname to an IPv6 address (`2606:2800:220:1:248:1893:25c8:1946`).
- **CNAME**: Canonical Name; aliases one hostname to another (`www.example.com -> example.com`).
- **MX**: Mail Exchange; specifies mail servers and priority weights.
- **TXT**: Arbitrary text; used for SPF (Sender Policy Framework), DKIM, and domain ownership verification.
- **SRV**: Service locator; specifies hostname and port for specific protocols (extensively used by Consul, Kubernetes, Active Directory).
- **PTR**: Pointer record; used for Reverse DNS lookups (mapping IP back to FQDN).
- **SOA / NS**: Start of Authority and Authoritative Nameserver delegation records.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                    Application Process                                  |
|                             curl https://api.prod.company.internal                      |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                    Glibc getaddrinfo()                                  |
|                            Consults /etc/nsswitch.conf (hosts)                          |
+----------------------------------------------------+------------------------------------+
                                                     |
                      +------------------------------+------------------------------+
                      | Step 1: files                                               | Step 2: resolve / dns
                      v                                                             v
          +-----------------------+                                     +-----------------------+
          |      /etc/hosts       |                                     |    /etc/resolv.conf   |
          | (Local Static Records)|                                     |  nameserver 127.0.0.53|
          +-----------------------+                                     +-----------+-----------+
                      | Match?                                                      |
           +----------+----------+                                                  v
           | Yes                 | No                                   +-----------------------+
           v                     v                                      |   systemd-resolved    |
    Return IP to App      Check next provider                           |  Local DNS Cache (53) |
                                                                        +-----------+-----------+
                                                                                    |
                                                  +---------------------------------+---------------------------------+
                                                  | Cache Hit?                                                        | Cache Miss
                                                  v                                                                   v
                                       Return Cached Record                                              +----------------------------+
                                                                                                         | Check Interface Routing    |
                                                                                                         | Per-link DNS & Search Path |
                                                                                                         +--------------+-------------+
                                                                                                                        |
                                                                                                +-----------------------+-----------------------+
                                                                                                |                                               |
                                                                                                v                                               v
                                                                                    +-----------------------+                       +-----------------------+
                                                                                    | Corporate VPN Link    |                       | Public Uplink (eth0)  |
                                                                                    | DNS: 10.0.0.2         |                       | DNS: 1.1.1.1          |
                                                                                    | Domain: ~.company.int |                       | Domain: *             |
                                                                                    +-----------------------+                       +-----------------------+
```

### The Recursive Query Sequence
When a cache miss occurs and queries leave the local host:
1. **Stub Resolver**: Client queries the Recursive Resolver (ISP, Cloud VPC resolver `169.254.169.253`, or `8.8.8.8`).
2. **Recursive Resolver**: Checks its cache. If absent, queries the **Root Nameservers** (`.` [a.root-servers.net to m.root-servers.net]).
3. **Root Server**: Returns referral to Top-Level Domain (TLD) nameservers (`.internal` or `.com`).
4. **TLD Server**: Returns referral to the Authoritative Nameservers for the target domain (`ns1.company.internal`).
5. **Authoritative Server**: Returns the final answer (`A` or `AAAA` record) with a Time-To-Live (TTL).
6. **Recursive Resolver**: Caches the record and returns the response to the client.

---

## Syntax & CLI Tools

### Modern Querying with `dig` (Domain Information Groper)
`dig` is the industry standard for DNS diagnosis (provided by `bind9-dnsutils`).
```bash
# Basic query for A record
dig example.com

# Short output (prints only resolved IP)
dig +short example.com

# Query specific record type (MX, TXT, AAAA, SRV)
dig example.com MX +short
dig example.com TXT +short

# Query a specific DNS server directly, bypassing local resolver
dig @1.1.1.1 example.com

# Perform a full recursive trace from root servers down to authoritative
dig example.com +trace

# Reverse DNS lookup (IP to hostname)
dig -x 8.8.8.8 +short
```

### Modern Resolver Control with `resolvectl` (Systemd)
```bash
# View global and per-interface DNS servers, search domains, and DNSSEC status
resolvectl status

# Query hostname through systemd-resolved
resolvectl query api.prod.company.internal

# View DNS cache statistics (hits, misses)
resolvectl statistics

# Flush the local systemd-resolved DNS cache completely
sudo resolvectl flush-caches

# Configure per-interface DNS server dynamically
sudo resolvectl dns wg0 10.100.0.1
sudo resolvectl domain wg0 "~corp.internal"
```

### Legacy and Quick Diagnostic Tools (`host`, `nslookup`)
```bash
# Quick host lookup
host example.com

# Query specific type
host -t CNAME www.example.com

# Interactive query using nslookup
nslookup example.com 8.8.8.8
```

---

## Progressive Practical Examples

### Example 1: Deconstructing a Detailed `dig` Response
```bash
$ dig api.github.com
```
**Output Breakdown**:
```text
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 48215
;; flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 1
```
- `status: NOERROR`: Query succeeded (other statuses: `NXDOMAIN` = domain does not exist, `SERVFAIL` = nameserver failed, `REFUSED` = nameserver rejected query).
- `flags: qr rd ra`:
  - `qr`: Query Response (this is a reply).
  - `rd`: Recursion Desired (client requested recursive lookup).
  - `ra`: Recursion Available (server supports recursive queries).
- **ANSWER SECTION**:
  ```text
  api.github.com.    60    IN    A    140.82.114.6
  ```
  Name, remaining TTL (60 seconds), Class (`IN` = Internet), Type (`A`), Target IP.
- **Query time: 14 msec**: Round-trip latency to the resolver.
- **SERVER: 127.0.0.53#53**: Local `systemd-resolved` stub resolver processed the query.

### Example 2: Configuring Split-Horizon DNS with `systemd-resolved`
**Scenario**: When connected to an office VPN (`wg0`), requests for `*.corp.internal` must route to the private internal nameserver (`10.0.0.2`), while all public internet traffic (`google.com`, `github.com`) continues resolving via standard public DNS (`1.1.1.1`).

```bash
#!/usr/bin/env bash
set -euo pipefail

VPN_IFACE="wg0"
CORP_DNS="10.0.0.2"
CORP_DOMAIN="corp.internal"

echo "==> Configuring split-horizon DNS for ${VPN_IFACE}..."
# 1. Assign nameserver to VPN interface
sudo resolvectl dns "${VPN_IFACE}" "${CORP_DNS}"

# 2. Assign routing domain with leading '~' (tilde designates routing domain only)
# '~' instructs systemd-resolved to ONLY route queries ending in this suffix to this interface
sudo resolvectl domain "${VPN_IFACE}" "~${CORP_DOMAIN}"

# 3. Verify routing policy
resolvectl domain "${VPN_IFACE}"
echo "==> Testing split routing..."
resolvectl query "db.${CORP_DOMAIN}"
```

### Example 3: Auditing and Repairing a Broken `/etc/resolv.conf`
When moving VMs or containers across networks, `/etc/resolv.conf` often becomes an orphaned static file pointing to dead IPs.

```bash
#!/usr/bin/env bash
set -euo pipefail

RESOLV_CONF="/etc/resolv.conf"
STUB_TARGET="/run/systemd/resolve/stub-resolv.conf"

echo "=== Auditing /etc/resolv.conf ==="
if [ -L "${RESOLV_CONF}" ]; then
    echo "Status: Symlink pointing to: $(readlink -f "${RESOLV_CONF}")"
else
    echo "WARNING: /etc/resolv.conf is a static regular file, not managed by systemd-resolved!"
fi

# Repair: Restore canonical systemd-resolved stub symlink
if [ -f "${STUB_TARGET}" ]; then
    echo "==> Re-linking /etc/resolv.conf to systemd-resolved stub..."
    sudo rm -f "${RESOLV_CONF}"
    sudo ln -s "${STUB_TARGET}" "${RESOLV_CONF}"
    sudo systemctl restart systemd-resolved
    echo "==> Repaired successfully."
else
    echo "ERROR: systemd-resolved stub file not found."
    exit 1
fi

resolvectl status
```

### Example 4: Tracing Full DNS Delegation with `dig +trace`
Observe how root servers delegate to TLDs and authoritative servers down the tree.

```bash
dig kubernetes.io +trace

# Output traces:
# 1. Root hints (.) queried -> returns .io TLD nameservers (a0.nic.io)
# 2. .io TLD queried -> returns Cloudflare authoritative servers (ns3.cloudflare.com)
# 3. Cloudflare queried -> returns final A record with authorative AA flag
```

### Example 5: Benchmarking Local Cache vs Upstream Latency
Demonstrating the latency impact of local caching.

```bash
#!/usr/bin/env bash
set -euo pipefail

TARGET_DOMAIN="aws.amazon.com"

# Flush cache first
sudo resolvectl flush-caches

echo "==> Cold Query (Cache Miss):"
dig "${TARGET_DOMAIN}" | grep "Query time"

echo "==> Warm Query (Cache Hit):"
dig "${TARGET_DOMAIN}" | grep "Query time"
# Query time drops from ~20-50ms to 0ms!
```

---

## Best Practices

1. **Leverage the Leading Tilde (`~`) for Split-Horizon Routing Domains**: In `systemd-resolved`, configuring `Domains=~corp.internal` marks the domain as a **routing domain** rather than a search domain. This ensures general unqualified hostnames do not append `.corp.internal` unnecessarily, preventing latency leaks.
2. **Never Edit `/etc/resolv.conf` Manually When Managed by Systemd**: Any manual edits to `/etc/resolv.conf` will be overwritten by `systemd-resolved`, `netplan`, or NetworkManager on reboot or network renegotiation. Modify the source configuration in `/etc/netplan/*.yaml` or `/etc/systemd/resolved.conf`.
3. **Use `+short` and `+trace` in Shell Scripting and Auditing**: When parsing outputs programmatically, use `dig +short <domain>`. When investigating intermittent resolution failures or DNS cache poisoning, always run `dig +trace` to isolate which delegation tier is failing.
4. **Implement Container DNS Caching (NodeLocal DNSCache)**: In Kubernetes, thousands of pods hitting CoreDNS simultaneously can saturate conntrack tables and trigger 5-second UDP drop timeouts. Deploying `NodeLocal DNSCache` caches queries on the local Linux host via loopback.
5. **Always Verify `/etc/nsswitch.conf` Ordering**: Ensure `files` precedes `resolve` or `dns` in `/etc/nsswitch.conf` so emergency overrides in `/etc/hosts` take immediate precedence without network roundtrips.
6. **Tune Negative Caching (TTL) for Failed Lookups**: In `/etc/systemd/resolved.conf`, verify that `Cache=yes` is enabled and review `Cache-Negative-Min-TTLSec` to avoid caching temporary NXDOMAIN failures during continuous deployment rollouts.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Overwriting `/etc/resolv.conf` with a static file | Breaks `systemd-resolved` caching and per-interface split-DNS logic; overwritten on reboot. | Configure nameservers in Netplan or `/etc/systemd/resolved.conf.d/`. |
| Adding multiple `nameserver` lines in `/etc/resolv.conf` expecting round-robin load balancing | Glibc resolver queries the **first** nameserver; it only tries the second if the first times out (after 5 seconds). | Do not expect load balancing; use local caching resolver or DNS load balancers. |
| Forgetting trailing dot in BIND/Zone files (`api.corp.com` vs `api.corp.com.`) | Missing terminal dot appends the parent zone domain name (`api.corp.com.corp.com`). | Always include terminal dot for fully qualified authoritative domain names. |
| Using `nslookup` for script parsing | `nslookup` output varies widely between operating systems and does not use the glibc resolution path. | Use `dig +short` or `getent hosts <domain>` in scripts. |
| Search path amplification delay | A long `search domain1 domain2 domain3` list in `/etc/resolv.conf` forces `getaddrinfo()` to execute 3 failed queries before trying the root domain for unqualified names. | Keep search domain lists minimal (1–2 domains maximum). |
| Docker container unable to resolve host private DNS | Docker by default uses public DNS (`8.8.8.8`) if the host's `/etc/resolv.conf` points to `127.0.0.53`. | Configure `/etc/docker/daemon.json` with explicit internal DNS servers. |

---

## Performance, Security & Cost Implications

### Performance
- **Local Cache Hit vs Remote Query**: A remote DNS query across the internet incurs **15ms–80ms** of network latency per lookup. In high-frequency microservice environments making thousands of outbound HTTP calls without connection pooling, local caching (`systemd-resolved` or `dnsmasq`) reduces lookup latency to **< 1ms**.
- **TCP vs UDP 53**: Traditional DNS queries use UDP port 53. If a response exceeds 512 bytes (or EDNS0 buffer size), the server truncates the packet (`TC` flag set), forcing the client to re-establish a 3-way handshake over TCP port 53.

### Security
- **DNS Spoofing & Cache Poisoning**: Traditional unencrypted UDP DNS can be intercepted or spoofed by on-path attackers. Enabling **DNSSEC** (`DNSSEC=allow-downgrade` or `yes` in `/etc/systemd/resolved.conf`) verifies cryptographic signatures (`RRSIG`, `DNSKEY`) from root down.
- **DNS-over-TLS (DoT)**: Encrypts DNS queries between the host and upstream resolvers via TLS port 853 (`DNSOverTLS=yes` in `resolved.conf`), preventing eavesdropping on corporate network lookups.

### Cost
- In AWS, querying the Amazon Route 53 Resolver VPC endpoint (`169.254.169.253`) has a hard quota of **1024 packets per second per network interface**. Exceeding this quota results in throttling and packet drops. Caching queries locally avoids hitting VPC resolver limits.

---

## Comparison Table / Alternatives

| Feature / Utility | `dig` | `resolvectl` | `nslookup` | `getent hosts` |
| :--- | :--- | :--- | :--- | :--- |
| **Origin / Suite** | BIND Utilities | systemd | BIND / POSIX | Glibc Core Utilities |
| **Resolution Engine** | Direct DNS Protocol (queries port 53) | `systemd-resolved` D-Bus | Direct DNS Protocol | Full `nsswitch.conf` stack (hosts + DNS) |
| **Output Detail** | Comprehensive raw protocol headers | Resolved metadata + link | Basic human readable | Single-line IP + Hostname |
| **Supports `+trace`** | Yes (full delegation path) | No | No | No |
| **Best Used For** | Low-level DNS debugging & record checks | Managing Linux system resolver state | Quick interactive test | Testing what an application actually sees |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: Why does `curl api.example.com` fail while `dig api.example.com` succeeds?
<details>
<summary>View Solution</summary>

**Root Cause**: `dig` and `curl` use two completely different resolution paths:
1. `dig` bypasses system resolution configuration: it constructs raw UDP packets directly to the nameserver specified in `/etc/resolv.conf` (or via `@server`).
2. `curl` uses the C library `getaddrinfo()`, which evaluates `/etc/nsswitch.conf`.

**Troubleshooting Steps**:
- Check `/etc/nsswitch.conf`: Ensure `hosts: files dns` or `hosts: files resolve` is present and not broken.
- Check `/etc/hosts`: Verify there isn't a stale, invalid IP mapped to `api.example.com`.
- Test using `getent hosts api.example.com` to observe what glibc actually returns to applications.
</details>

### Q2: What is the purpose of `127.0.0.53` in `/etc/resolv.conf` on Ubuntu?
<details>
<summary>View Solution</summary>

`127.0.0.53` is the local IPv4 loopback address where `systemd-resolved` runs its internal stub listener daemon. 

It provides:
- Centralized in-memory DNS caching for all local processes.
- Dynamic per-link DNS selection (routing internal queries over VPN interfaces and public queries over physical NICs).
- DNSSEC validation and DNS-over-TLS encryption.
Applications send standard UDP queries to `127.0.0.53:53`, and `systemd-resolved` handles upstream lookups.
</details>

### Q3: How do you flush the DNS cache on a modern systemd-based Linux distribution?
<details>
<summary>View Solution</summary>

Use `resolvectl`:
```bash
sudo resolvectl flush-caches
```
Verify the cache was cleared:
```bash
resolvectl statistics
```
The "Current Cache Size" will reset to 0.
</details>

### Q4: What does `status: NXDOMAIN` mean in a `dig` response?
<details>
<summary>View Solution</summary>

`NXDOMAIN` stands for **Non-Existent Domain**. It is an authoritative response from the DNS server indicating that the queried domain name does not exist in the DNS hierarchy. 

*Contrast with*:
- `SERVFAIL`: The nameserver could not obtain an answer (e.g., upstream timeout, DNSSEC validation failure).
- `REFUSED`: The nameserver refused to answer (e.g., access control list prevents recursive queries from this client IP).
</details>

### Q5: Why do Docker containers sometimes experience 5-second DNS delays when making external API calls?
<details>
<summary>View Solution</summary>

**Root Cause**: Linux glibc sends both `A` (IPv4) and `AAAA` (IPv6) queries concurrently over UDP using the same socket source port. In the Linux kernel, Netfilter Connection Tracking (`conntrack`) handles NAT for container traffic. A known kernel race condition in `conntrack` drops one of the two concurrent UDP packets when handling identical socket tuples. Glibc then waits for its built-in **5-second timeout** before retransmitting.

**Mitigation**:
- Enable `single-request-reopen` in `/etc/resolv.conf`:
  ```text
  options single-request-reopen
  ```
  This forces glibc to close the socket and open a new one between `A` and `AAAA` lookups.
- Alternatively, disable IPv6 lookups if not needed.
</details>

### Q6: How does split-horizon (Split-Brain) DNS work?
<details>
<summary>View Solution</summary>

Split-horizon DNS provides different answers to the same DNS query depending on the source IP of the requester:
- An internal client inside the corporate network querying `portal.company.com` receives the private IP `10.0.1.50`.
- An external internet client querying `portal.company.com` receives the public IP `203.0.113.10` (pointing to a WAF or DMZ reverse proxy).
On the Linux client, `systemd-resolved` manages this using routing domains (`~domain`) tied to specific VPN or LAN interfaces.
</details>

### Q7: What is the function of the `search` directive in `/etc/resolv.conf`?
<details>
<summary>View Solution</summary>

The `search` directive specifies a list of domain suffixes to append to unqualified (single-word) hostnames.
Example:
```text
search corp.internal staging.internal
```
When an application queries `ping db`:
1. Glibc first queries `db.corp.internal`.
2. If that returns NXDOMAIN, it queries `db.staging.internal`.
3. If that fails, it queries `db` as a root domain.
</details>

### Q8: What is DNSSEC and how does it prevent attacks?
<details>
<summary>View Solution</summary>

DNSSEC (Domain Name System Security Extensions) adds cryptographic authentication to DNS. It uses public-key cryptography to sign DNS records (`RRSIG`) with a private key. Resolvers validate the chain of trust from the ICANN root zone (`.`) down through the TLD to the authoritative domain. 
This protects against **DNS spoofing** and **cache poisoning** (where an attacker injects forged IP records into a recursive resolver's cache).
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Querying Record Types with `dig`
**Task**: Query Google's public DNS servers for the `MX` records and `TXT` records of `wikipedia.org`. Extract only the values using `+short`.
<details>
<summary>View Solution</summary>

```bash
# Query MX records
dig @8.8.8.8 wikipedia.org MX +short

# Query TXT records
dig @8.8.8.8 wikipedia.org TXT +short
```
</details>

### Exercise 2: Measuring Local DNS Cache Performance
**Task**: Check your current DNS cache statistics via `resolvectl statistics`. Issue 5 queries for a popular domain, and observe the hit counter increase.
<details>
<summary>View Solution</summary>

```bash
# 1. Inspect baseline stats
resolvectl statistics

# 2. Issue repeated queries
for i in {1..5}; do
    resolvectl query cloudflare.com > /dev/null
done

# 3. Verify Cache Hits increased
resolvectl statistics
```
</details>

### Exercise 3: Simulating and Testing `/etc/hosts` Override
**Task**: Add a mock entry in `/etc/hosts` mapping `fake.api.internal` to `127.0.0.1`. Verify that `getent hosts` and `curl` resolve it, but direct `dig` fails.
<details>
<summary>View Solution</summary>

```bash
# 1. Add entry
echo "127.0.0.1 fake.api.internal" | sudo tee -a /etc/hosts

# 2. Query via getent (uses nsswitch -> files)
getent hosts fake.api.internal
# Returns: 127.0.0.1 fake.api.internal

# 3. Query via dig (bypasses /etc/hosts -> queries DNS port 53 directly)
dig fake.api.internal +short
# Returns nothing (NXDOMAIN)

# 4. Clean up
sudo sed -i '/fake.api.internal/d' /etc/hosts
```
</details>

### Exercise 4: Tracing Authoritative Nameservers
**Task**: Find the authoritative nameservers for `kernel.org` using `dig NS`, then query one of those authoritative nameservers directly.
<details>
<summary>View Solution</summary>

```bash
# 1. Find NS records
NS_SERVER=$(dig kernel.org NS +short | head -n 1)
echo "Authoritative server: ${NS_SERVER}"

# 2. Query that authoritative server directly with +norecurse
dig @"${NS_SERVER}" kernel.org A +norecurse
# Look for the 'aa' (Authoritative Answer) flag in the header!
```
</details>

### Exercise 5: Inspecting Systemd-Resolved Link Configuration
**Task**: Run `resolvectl status` and identify which interface is designated as the default DNS route and which upstream DNS servers are configured.
<details>
<summary>View Solution</summary>

```bash
resolvectl status

# Look for:
# Link 2 (enp3s0 or eth0):
#   Current DNS Server: 192.168.1.1
#   DNS Servers: 192.168.1.1
#   DNS Domain: corp.local
```
</details>

### Exercise 6: Testing Reverse DNS Resolution (PTR Records)
**Task**: Perform a reverse DNS lookup on Google's public DNS IP `8.8.8.8` using both `dig -x` and `host`.
<details>
<summary>View Solution</summary>

```bash
# Using dig
dig -x 8.8.8.8 +short
# Output: dns.google.

# Using host
host 8.8.8.8
# Output: 8.8.8.8.in-addr.arpa domain name pointer dns.google.
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command |
| :--- | :--- |
| **Simple A Record Query** | `dig example.com +short` |
| **Query Specific Record** | `dig example.com [MX\|TXT\|AAAA\|SRV] +short` |
| **Query Specific DNS Server** | `dig @1.1.1.1 example.com` |
| **Full Trace Delegation** | `dig example.com +trace` |
| **Reverse DNS Lookup** | `dig -x <IP_ADDRESS> +short` |
| **Systemd Resolver Status** | `resolvectl status` |
| **Query via Systemd Resolver**| `resolvectl query example.com` |
| **Flush DNS Cache** | `sudo resolvectl flush-caches` |
| **Application-Level Resolution**| `getent hosts example.com` |

### Further Reading
- `man 1 dig` and `man 1 resolvectl`
- `man 5 resolv.conf` and `man 5 nsswitch.conf`
- [systemd-resolved.service Documentation](https://www.freedesktop.org/software/systemd/man/latest/systemd-resolved.service.html)
- [RFC 1034: Domain Names - Concepts and Facilities](https://datatracker.ietf.org/doc/html/rfc1034)
- [RFC 1035: Domain Names - Implementation and Specification](https://datatracker.ietf.org/doc/html/rfc1035)
