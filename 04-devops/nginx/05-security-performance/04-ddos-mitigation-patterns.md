---
title: "DDoS Mitigation Patterns & Edge Concurrency Defense"
description: "Comprehensive engineering guide to defending against Layer 7 Distributed Denial of Service (DDoS) attacks in NGINX, covering HTTP floods, Slowloris, Slow POST, connection shedding, iptables/ipset integration, and rate limiting architectures."
module: "05-security-performance"
topic: "04-ddos-mitigation-patterns"
status: "completed"
tags: ["nginx", "ddos", "security", "rate-limiting", "slowloris", "http-flood", "fail2ban", "ipset", "concurrency"]
prerequisites: ["03-traffic-management/03-rate-limiting.md", "05-security-performance/01-performance-tuning.md"]
estimated_read_time: "35 min"
---

# DDoS Mitigation Patterns & Edge Concurrency Defense

```
+-------------------------------------------------------------------------------+
| Multi-Tiered Layer 7 DDoS Mitigation Pipeline                                 |
|                                                                               |
|  [Volumetric DDoS & Distributed Botnets]                                      |
|        │                                                                      |
|        ▼                                                                      |
|  [Kernel Layer: SYN Cookies & Conntrack Tuning]                               |
|        │  net.ipv4.tcp_syncookies = 1; net.netfilter.nf_conntrack_max = 2M    |
|        ▼                                                                      |
|  [Firewall Filtering: ipset + iptables Hash Drops]                            |
|        │  Immediate kernel drop for known abusive CIDRs / fail2ban jails       |
|        ▼                                                                      |
|  [NGINX Connection Shield: Active Sockets Cap]                                |
|        │  limit_conn_zone $binary_remote_addr zone=addr_cap:20m;               |
|        │  limit_conn addr_cap 25; (Prevents socket exhaustion)               |
|        ▼                                                                      |
|  [Anti-Slowloris Timeouts & RST Reclamation]                                  |
|        │  client_header_timeout 5s; client_body_timeout 5s;                   |
|        │  reset_timedout_connection on;                                       |
|        ▼                                                                      |
|  [Request Rate Shaping: Leaky Bucket Throttle]                                |
|        │  limit_req_zone $binary_remote_addr zone=api_flood:20m rate=15r/s;   |
|        │  limit_req zone=api_flood burst=30 nodelay; (Return 429)             |
|        ▼                                                                      |
|  [Legitimate Traffic Passes to Protected Application Upstream]                |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Attack Vectors at Layer 7](#2-core-attack-vectors-at-layer-7)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Directives Reference](#4-syntax--directives-reference)
5. [Practical Implementation Examples](#5-practical-implementation-examples)
6. [Automated Offense Defense: fail2ban & IPset Integration](#6-automated-offense-defense-fail2ban--ipset-integration)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance, Kernel & Memory Calculations](#8-performance-kernel--memory-calculations)
9. [Architectural Comparison: Layer 4 vs Layer 7 DDoS Defense](#9-architectural-comparison-layer-4-vs-layer-7-ddos-defense)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Distributed Denial of Service (DDoS) attacks are designed to exhaust system resources—bandwidth, CPU cycles, RAM, operating system socket descriptors, or backend database connection pools:
- **Layer 7 Asymmetry**: An attacker generating a 50KB HTTP request containing complex recursive JSON or SQL query parameters requires negligible attacker bandwidth, but consumes 100% of an application server's CPU and database thread pool.
- **Socket Starvation (Slowloris)**: Attackers open thousands of TCP connections and transmit HTTP header bytes at excruciatingly slow speeds (e.g. 1 byte every 10 seconds). Unmitigated servers exhaust their worker connection pools, locking out legitimate users while consuming virtually zero network bandwidth.
- **HTTP Floods**: Massive botnet swarms send millions of valid GET or POST requests per second directly to computationally expensive endpoints (`/search`, `/login`, `/checkout`).

NGINX serves as the shock absorber, shedding abusive traffic at wire speed before transactions can saturate application runtimes.

---

## 2. Core Attack Vectors at Layer 7

| Attack Vector | Mechanics | Targeted Resource | Mitigation Mechanism in NGINX |
| :--- | :--- | :--- | :--- |
| **HTTP Request Flood** | High-volume valid HTTP requests targeting CPU-heavy routes | Application CPU, DB connection pools | `limit_req` with burst/nodelay, return 429 |
| **Slowloris** | Holding TCP sockets open by sending partial HTTP headers slowly | NGINX worker connections, OS file descriptors | `client_header_timeout 5s`, `reset_timedout_connection on` |
| **Slow POST (RUDY)**| Transmitting request body bytes at 1 byte/interval | Worker memory buffers, application threads | `client_body_timeout 5s`, `client_body_buffer_size` |
| **Connection Flood** | Rapidly opening thousands of TCP connections from single IPs | Kernel conntrack tables, NGINX memory | `limit_conn` per `$binary_remote_addr` |
| **HashDoS** | Sending POST payloads with engineered hash collision keys | Backend hash table parsing (100% CPU lock) | Strict `client_max_body_size`, WAF regex filters |

---

## 3. How It Works Internally

### Connection Lifecycle Under Slowloris Attack

```
  Attacker opens TCP Socket
          │
          ▼
  Sends: "GET /index.html HTTP/1.1\r\n"
  Sends: "X-Header-1: a" (pauses 10s)
  Sends: "X-Header-2: b" (pauses 10s)
          │
          ▼
  NGINX Worker Process Timer Mechanics:
  - Event loop associates `client_header_timeout` with socket read event.
  - Timer stored in Red-Black Tree (`ngx_rbtree_t`).
  - Timer tick evaluates: Has a complete header line arrived within 5s?
          │
          ├── NO (Timeout Expires):
          │   1. NGINX closes socket immediately.
          │   2. If `reset_timedout_connection on;`, sends TCP RST.
          │   3. Socket enters CLOSED state immediately (bypasses TIME_WAIT).
          │   4. Worker connection FD returned to free pool in O(1) time.
          │
          └── YES:
              Reset timer and continue reading.
```

By enforcing strict timeouts and utilizing `reset_timedout_connection on;`, NGINX avoids the standard 4-way TCP teardown and 60-second `TIME_WAIT` state, releasing memory and connection slots immediately.

---

## 4. Syntax & Directives Reference

### Connection & Request Limiting
- `limit_conn_zone <key> zone=<name>:<size>;`: Allocates a shared memory zone for tracking concurrent open connections indexed by key (typically `$binary_remote_addr`).
- `limit_conn <zone_name> <number>;`: Enforces the maximum concurrent active connections allowed for a given key.
- `limit_conn_status <code|default 503>;`: Sets the HTTP status code returned to rejected clients (recommend changing to `429 Too Many Requests`).
- `limit_req_zone <key> zone=<name>:<size> rate=<rate>;`: Defines a shared memory zone for tracking request arrival rates using the leaky bucket algorithm.
- `limit_req zone=<name> [burst=N] [nodelay | delay=N];`: Applies request rate limiting. `nodelay` processes burst requests immediately up to limit $N$ while rejecting excess requests with 429.

### Timeouts & Connection Reclamation
- `client_header_timeout <time>;`: Maximum duration between consecutive read operations while reading the client request header.
- `client_body_timeout <time>;`: Maximum duration between consecutive read operations while reading the request body.
- `reset_timedout_connection on | off;`: Instructs the operating system to send a TCP RST packet to clear timed-out sockets, releasing kernel memory immediately.

---

## 5. Practical Implementation Examples

### Example 1: Multi-Layered DDoS Defense Configuration (`nginx.conf`)

A complete production-grade DDoS defense configuration featuring connection caps, tiered rate limits, and aggressive timeout reclamation:

```nginx
# /etc/nginx/nginx.conf
user www-data;
worker_processes auto;
worker_rlimit_nofile 1048576;

events {
    worker_connections 65535;
    use epoll;
    multi_accept on;
}

http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    # 1. Shared Memory Zones for DDoS Tracking (10MB holds ~160,000 IPv4 addresses)
    limit_conn_zone $binary_remote_addr zone=conn_per_ip:20m;
    limit_conn_zone $server_name zone=conn_per_server:20m;

    # General traffic rate limit: 20 requests/second per IP
    limit_req_zone $binary_remote_addr zone=req_general:20m rate=20r/s;

    # Sensitive endpoints (login, search): 3 requests/second per IP
    limit_req_zone $binary_remote_addr zone=req_sensitive:20m rate=3r/s;

    # Customize rejection status to RFC 6585 standard (429 Too Many Requests)
    limit_req_status 429;
    limit_conn_status 429;

    # 2. Anti-Slowloris and Aggressive Resource Reclamation
    client_header_timeout 5s;
    client_body_timeout 5s;
    send_timeout 5s;
    keepalive_timeout 30s 15s; # Handshake timeout 15s
    keepalive_requests 1000;

    reset_timedout_connection on;

    # 3. Buffer Constraints (Prevent memory exhaustion)
    client_max_body_size 10m;
    client_body_buffer_size 128k;
    client_header_buffer_size 1k;
    large_client_header_buffers 4 8k;

    server {
        listen 443 ssl default_server;
        server_name app.example.com;

        ssl_certificate /etc/letsencrypt/live/app.example.com/fullchain.pem;
        ssl_certificate_key /etc/letsencrypt/live/app.example.com/privkey.pem;

        # Enforce global connection limits per IP (max 20 concurrent sockets)
        limit_conn conn_per_ip 20;

        # Enforce baseline rate limit: allow burst up to 40, reject excess immediately
        limit_req zone=req_general burst=40 nodelay;

        location / {
            proxy_pass http://127.0.0.1:8080;
            proxy_http_version 1.1;
            proxy_set_header Connection "";
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
        }

        # Stricter protection on authentication and search endpoints
        location ~* /(login|auth|search|checkout) {
            limit_conn conn_per_ip 5;
            limit_req zone=req_sensitive burst=5 nodelay;

            proxy_pass http://127.0.0.1:8080;
            proxy_http_version 1.1;
            proxy_set_header Connection "";
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
        }
    }
}
```

### Example 2: Whitelisting Trusted IPs and Internal CIDRs from Rate Limiting

Avoid throttling corporate VPNs, internal microservices, or partner webhooks by using the `geo` and `map` modules:

```nginx
http {
    # 1. Map client IP to determine if rate limiting applies
    geo $rate_limited_client {
        default         1;
        127.0.0.1/32    0; # Localhost
        10.0.0.0/8      0; # Internal VPC
        192.168.1.0/24  0; # Corporate VPN
        203.0.113.50/32 0; # Partner Static Webhook IP
    }

    # 2. If client is whitelisted (0), set tracking key to empty string ""
    # NGINX skips rate limiting when the key evaluates to an empty string!
    map $rate_limited_client $limit_key {
        0 "";
        1 $binary_remote_addr;
    }

    # 3. Zone tracks $limit_key instead of raw $binary_remote_addr
    limit_req_zone $limit_key zone=smart_limit:20m rate=10r/s;

    server {
        listen 80;
        server_name example.com;

        location /api/ {
            limit_req zone=smart_limit burst=20 nodelay;
            proxy_pass http://127.0.0.1:3000;
        }
    }
}
```

---

## 6. Automated Offense Defense: fail2ban & IPset Integration

When an IP continuously hammers rate-limited endpoints, continuing to process its TLS handshakes and return HTTP 429 still consumes CPU. Offload repeated offenders to the Linux kernel firewall:

### Step 1: Configure NGINX Error Log Parsing in fail2ban

```ini
# /etc/fail2ban/filter.d/nginx-ratelimit.conf
[Definition]
failregex = limiting requests, excess: .* by zone ".*", client: <HOST>
ignoreregex =
```

### Step 2: Define the fail2ban Jail with IPset Action

```ini
# /etc/fail2ban/jail.d/nginx-ddos.local
[nginx-ratelimit]
enabled  = true
port     = http,https
filter   = nginx-ratelimit
logpath  = /var/log/nginx/error.log
maxretry = 10
findtime = 60
bantime  = 3600
banaction = iptables-ipset-proto4
```

### Step 3: High-Performance IPset Kernel Dropping
`ipset` uses a constant-time $O(1)$ hash table inside Netfilter, enabling the Linux kernel to drop millions of malicious packets per second without scanning linear iptables rule chains:

```bash
# Verify active IPset blacklist managed by fail2ban
sudo ipset list f2b-nginx-ratelimit
```

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern / Mistake | Root Cause | Observable Production Impact | Correct Solution |
| :--- | :--- | :--- | :--- |
| Using `$remote_addr` instead of `$binary_remote_addr` | Storing IPv4 addresses as text (7–15 bytes) instead of binary (4 bytes) | Shared memory zone runs out of space 4× faster under attack | Always use `$binary_remote_addr` for zone definitions |
| Using `limit_req` without `nodelay` or `delay` | Requests exceeding the base rate are queued synchronously | Artificial latency spikes ($p99$ > 10s); clients experience severe hangs | Use `nodelay` for immediate 429 shedding or `delay=N` for two-stage shaping |
| Rate limiting behind CDN/ALB without `real_ip` | All incoming internet traffic appears to originate from CloudFront/ALB IPs | Entire website throttles globally when a single attacker attacks | Configure `set_real_ip_from` and `real_ip_header` |
| Default `503 Service Unavailable` for Rate Limits | 503 triggers retry storms in mobile apps and degrades search engine ranking | Upstream thrashed by aggressive automatic client retries | Configure `limit_req_status 429;` |
| Omitting `reset_timedout_connection on;` | Slowloris sockets undergo standard FIN/ACK handshake | Sockets remain locked in kernel `TIME_WAIT` for 60 seconds | Set `reset_timedout_connection on;` |
| Excessively tight `burst` on dynamic web apps | Modern web apps load 50–100 assets simultaneously upon page load | Legitimate users experience broken CSS, images, and fonts (429 errors) | Set `burst=40` or higher on general web endpoints |

---

## 8. Performance, Kernel & Memory Calculations

### Shared Memory Zone Sizing Formula
- IPv4 stored via `$binary_remote_addr` consumes **64 bytes** per state entry in the shared slab allocator (accounting for rbtree overhead and expiration timestamps).
- IPv6 addresses consume **80 bytes**.
- A 10MB memory zone holds:
$$\text{Capacity} = \frac{10 \times 1024 \times 1024 \text{ bytes}}{64 \text{ bytes/client}} \approx 163,840 \text{ concurrent unique IPv4 addresses}$$
If a botnet exceeds 163,840 unique IPs within the expiration window, NGINX evicts LRU entries, potentially allowing some requests through. For large botnets, size zones to **20MB or 50MB**.

### Kernel SYN Backlog and Conntrack Sizing
Under volumetric SYN floods:
```bash
# Increase maximum tracked connections in Netfilter
sudo sysctl -w net.netfilter.nf_conntrack_max=2097152

# Enable kernel SYN cookies (zero RAM allocated for half-open sockets)
sudo sysctl -w net.ipv4.tcp_syncookies=1
```

---

## 9. Architectural Comparison: Layer 4 vs Layer 7 DDoS Defense

| Attack Type | Layer 4 (SYN Flood, UDP Amp) | Layer 7 (HTTP Flood, Slowloris) |
| :--- | :--- | :--- |
| **Inspection Depth** | TCP/UDP headers, IP 5-tuple | Full HTTP URI, headers, request body |
| **Mitigation Point** | BGP Anycast, Cloudflare Magic Transit, AWS Shield | NGINX, WAF, application rate limiters |
| **Resource Consumed** | Network bandwidth, NIC interrupt queues | CPU cycles, worker threads, RAM buffers |
| **Identification Signal** | High PPS (packets/sec), malformed flags | High RPS (requests/sec), slow transmission |
| **Mitigation Action** | BGP blackholing, SYN Cookies, rate dropping | Leaky-bucket 429, TCP RST, CAPTCHA challenges |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. Explain how the Leaky Bucket algorithm is implemented in NGINX's `limit_req` module.</strong></summary>

NGINX implements the Leaky Bucket algorithm using shared memory and high-precision timers. The bucket has a defined leak rate (e.g. `rate=10r/s`, meaning one token leaves the bucket every 100ms). When a request arrives:
1. If the bucket is empty, the request passes immediately, and the bucket fills with one token.
2. If subsequent requests arrive faster than 100ms apart, they fill the bucket up to the configured `burst` limit.
3. If `nodelay` is specified, burst requests are admitted immediately without waiting, but the bucket remains full. If further requests arrive before the bucket leaks, they are rejected with HTTP 429.
4. If `nodelay` is omitted, excess burst requests are delayed in a queue and processed at the steady leak rate.
</details>

<details>
<summary><strong>2. How does Slowloris bypass traditional network firewalls, and why is NGINX naturally resilient to it?</strong></summary>

Traditional network firewalls and thread-per-connection web servers (like classic Apache HTTPD) allocate an entire operating system thread and stack space to every open TCP connection. Slowloris sends periodic header fragments (e.g. `X-Header: value\r\n`), maintaining valid TCP traffic that stateful firewalls permit. Because Apache blocks a dedicated thread waiting for the headers, 1,000 slow connections consume all available worker threads. NGINX is inherently resilient because of its non-blocking `epoll` architecture: thousands of idle or slow connections sit on OS file descriptors consuming only a few kilobytes of RAM each. With `client_header_timeout 5s;`, NGINX drops connections that fail to advance within 5 seconds, neutralizing the attack.
</details>

<details>
<summary><strong>3. Why is `$binary_remote_addr` used instead of `$remote_addr` in rate limiting zones?</strong></summary>

The string variable `$remote_addr` stores IP addresses as human-readable ASCII text (e.g. `"192.168.100.254"` is 15 bytes plus string headers). In contrast, `$binary_remote_addr` stores IPv4 addresses in binary form as exactly 4 bytes (or 16 bytes for IPv6). This reduces the memory footprint of each entry in the shared slab memory zone, allowing the same 10MB zone to store four times as many unique tracking records under a distributed attack.
</details>

<details>
<summary><strong>4. What happens when an NGINX `limit_req_zone` runs out of shared memory during a massive attack?</strong></summary>

When a shared memory zone fills up, NGINX attempts to reclaim memory by evicting older, least-recently-used (LRU) state entries whose timers have expired. If memory cannot be freed (all entries are actively receiving requests within the current second), NGINX returns `503 Service Unavailable` (or the configured `limit_req_status`) for newly arriving client IP addresses that are not already present in the zone.
</details>

<details>
<summary><strong>5. Explain the two-stage rate limiting mechanism enabled by the `delay=N` parameter in NGINX.</strong></summary>

Introduced in NGINX 1.15.7, `limit_req zone=name burst=20 delay=10;` enables two-stage rate shaping:
1. The first 10 burst requests (`delay=10`) are processed immediately without any artificial delay.
2. The subsequent 10 burst requests (from 11 up to 20) are admitted but artificially delayed to smooth out traffic spikes and protect the backend.
3. Any request exceeding the total burst of 20 is rejected immediately with HTTP 429.
This provides the ideal balance between accommodating rapid web page asset loads and throttling abusive scripts.
</details>

<details>
<summary><strong>6. How do you distinguish between a DDoS attack and a legitimate viral traffic surge in NGINX logs?</strong></summary>

1. **User-Agent & Request Entropy**: Attacks often feature uniform User-Agents, missing `Accept-Language` headers, or randomized query strings designed to bypass caching (`/?cb=12345`).
2. **Referrer Analysis**: Viral traffic typically originates from major aggregators or social media referrers (`news.ycombinator.com`, `reddit.com`, `t.co`).
3. **Session Behavior**: Legitimate users fetch HTML, followed immediately by CSS, JS, and image assets. Botnet attacks often hit pure compute endpoints (`POST /api/search`) repeatedly without requesting static assets.
4. **Geographic Distribution**: Sudden bursts of traffic from countries outside your target market indicate botnet activity.
</details>

<details>
<summary><strong>7. Why must you configure `set_real_ip_from` when deploying NGINX behind Cloudflare or an AWS ALB?</strong></summary>

Without `set_real_ip_from`, NGINX sees the IP address of the reverse proxy / CDN edge node as `$remote_addr`. If you apply `limit_req_zone $binary_remote_addr`, NGINX applies rate limits to the Cloudflare or ALB IP addresses! As soon as one IP generates 20 req/sec, NGINX blocks all other legitimate users whose traffic happens to route through that same edge proxy. `set_real_ip_from` instructs NGINX to trust the `CF-Connecting-IP` or `X-Forwarded-For` header from specific CIDRs, restoring the true client IP into `$remote_addr`.
</details>

<details>
<summary><strong>8. What is the role of `reset_timedout_connection on;` in DDoS defense?</strong></summary>

Normally, closing a TCP connection requires a 4-way FIN/ACK handshake. If an attacker opens thousands of connections and abandons them, standard closure leaves sockets in `FIN_WAIT` or `TIME_WAIT` for up to 60 seconds. `reset_timedout_connection on;` instructs the operating system to send a TCP RST (reset) packet when a connection times out, bypassing the graceful closure state machine and instantly freeing worker connection memory and kernel file descriptors.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Simulate an HTTP Flood with `wrk` and Observe 429 Responses</strong></summary>

**Goal**: Validate that `limit_req` enforces traffic shedding.
1. Configure a test location with `limit_req zone=req_general burst=10 nodelay;`.
2. Launch a synthetic flood:
```bash
wrk -t4 -c50 -d5s http://localhost/
```
3. Check the HTTP status code distribution. Confirm that excess requests receive `429 Too Many Requests`.
4. Inspect `/var/log/nginx/error.log` for `limiting requests, excess: ...` entries.
</details>

<details>
<summary><strong>Exercise 2: Test Anti-Slowloris Timeouts with Netcat</strong></summary>

**Goal**: Verify that NGINX terminates slow clients after `client_header_timeout`.
1. Configure `client_header_timeout 5s; reset_timedout_connection on;`.
2. Open a connection manually:
```bash
nc localhost 80
```
3. Send: `GET / HTTP/1.1\r\n` and wait.
4. Verify that exactly 5 seconds later, the connection is forcefully closed by NGINX.
</details>

<details>
<summary><strong>Exercise 3: Configure fail2ban to Jail Abusive Rate-Limit Offenders</strong></summary>

**Goal**: Automate firewall drops for persistent attackers.
1. Install fail2ban: `sudo apt-get install fail2ban ipset`.
2. Create `/etc/fail2ban/filter.d/nginx-limit.conf` matching NGINX limit errors.
3. Configure jail with `bantime = 600` and trigger an attack using `ab -n 1000 -c 10 http://localhost/`.
4. Run `sudo fail2ban-client status nginx-limit` and verify the IP is jailed in `ipset`.
</details>

<details>
<summary><strong>Exercise 4: Verify IP Whitelisting with the `geo` and `map` Modules</strong></summary>

**Goal**: Ensure localhost and admin IPs bypass rate limiting rules.
1. Implement the `geo` and `map` whitelist configuration from Example 2.
2. Run `wrk -t2 -c20 -d5s http://127.0.0.1/api/`.
3. Verify 0 requests receive HTTP 429 because `127.0.0.1` is whitelisted.
</details>

<details>
<summary><strong>Exercise 5: Inspect Shared Memory Slab Allocation</strong></summary>

**Goal**: Audit memory usage inside `limit_req_zone`.
1. Configure NGINX with `stub_status` module enabled.
2. Query metrics endpoint under rate-limited load.
3. Calculate active memory slots based on number of unique simulated client IPs.
</details>

<details>
<summary><strong>Exercise 6: Tune Linux Kernel SYN Flood Defenses</strong></summary>

**Goal**: Harden host kernel against Layer 4 SYN floods.
1. Verify SYN cookie activation: `sysctl net.ipv4.tcp_syncookies`.
2. Test raising the backlog queue:
```bash
sudo sysctl -w net.ipv4.tcp_max_syn_backlog=65535
sudo sysctl -w net.core.somaxconn=65535
```
3. Verify persistent entries in `/etc/sysctl.d/99-ddos.conf`.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| NGINX DDoS Mitigation Directive Cheat Sheet                                    |
|                                                                                |
| Directive                     | Context         | Purpose                      |
| :---------------------------- | :-------------- | :--------------------------- |
| limit_conn_zone $binary_...   | http            | Allocates connection zone    |
| limit_conn <zone> <limit>;    | http, srv, loc  | Caps active open sockets     |
| limit_req_zone $binary_...    | http            | Allocates rate limit zone    |
| limit_req ... burst=N nodelay;| http, srv, loc  | Leaky bucket rate shaping    |
| limit_req_status 429;         | http, srv, loc  | Returns RFC 429 Too Many Req |
| client_header_timeout 5s;     | http, server    | Drops Slowloris headers      |
| client_body_timeout 5s;       | http, server    | Drops Slow POST payloads     |
| reset_timedout_connection on; | http, server    | Immediate TCP RST on timeout |
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [NGINX Official Mitigation Guide: Mitigating DDoS Attacks with NGINX](https://www.nginx.com/blog/mitigating-ddos-attacks-with-nginx-and-nginx-plus/)
- [RFC 6585: Additional HTTP Status Codes (HTTP 429)](https://datatracker.ietf.org/doc/html/rfc6585)
- [Netfilter IPset Documentation](https://ipset.netfilter.org/)
- [Next Concept: Real IP, Trusted Proxies & Ingress Topologies](05-real-ip-and-trusted-proxies.md)

---

[← Previous Note: 03-waf-and-modsecurity-overview.md](03-waf-and-modsecurity-overview.md) | [Back to Module Index](README.md) | [Next Note: 05-real-ip-and-trusted-proxies.md →](05-real-ip-and-trusted-proxies.md)
