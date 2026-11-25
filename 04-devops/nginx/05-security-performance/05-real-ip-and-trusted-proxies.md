---
title: "Real IP Resolution, Trusted Proxies & PROXY Protocol"
description: "Comprehensive guide to accurately restoring client IP addresses in NGINX through CDNs, load balancers, and Kubernetes ingresses using ngx_http_realip_module, PROXY Protocol v1/v2, and recursive chain validation."
module: "05-security-performance"
topic: "05-real-ip-and-trusted-proxies"
status: "completed"
tags: ["nginx", "real-ip", "proxy-protocol", "cloudflare", "alb", "kubernetes", "security", "spoofing", "x-forwarded-for"]
prerequisites: ["05-security-performance/04-ddos-mitigation-patterns.md", "02-core-features/02-reverse-proxy.md"]
estimated_read_time: "35 min"
---

# Real IP Resolution, Trusted Proxies & PROXY Protocol

```
+-------------------------------------------------------------------------------+
| Client IP Restoration & Trusted Proxy Architecture                           |
|                                                                               |
|  [True Client: 203.0.113.195]                                                 |
|        │                                                                      |
|        ▼                                                                      |
|  [Edge CDN / WAF (e.g. Cloudflare: 173.245.48.0/20)]                         |
|        │  Appends to X-Forwarded-For: "203.0.113.195, 173.245.48.12"          |
|        ▼                                                                      |
|  [Cloud Load Balancer (AWS ALB: 10.0.1.0/24)]                                 |
|        │  Appends ALB IP: "203.0.113.195, 173.245.48.12, 10.0.1.55"          |
|        ▼                                                                      |
|  [NGINX Ingress / Reverse Proxy (Post-Read Phase)]                            |
|        │                                                                      |
|        ├─ set_real_ip_from 10.0.1.0/24; (Trust AWS ALB)                       |
|        ├─ set_real_ip_from 173.245.48.0/20; (Trust Cloudflare)                |
|        ├─ real_ip_header X-Forwarded-For;                                     |
|        ├─ real_ip_recursive on; (Unwinds chain from right to left)           |
|        ▼                                                                      |
|  Evaluated $remote_addr restored to: 203.0.113.195 (True Client IP)           |
|  Rate limits, GeoIP, access rules, and logs now operate on true client IP!    |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts & The IP Spoofing Threat](#2-core-concepts--the-ip-spoofing-threat)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Directives Reference](#4-syntax--directives-reference)
5. [Practical Implementation Examples](#5-practical-implementation-examples)
6. [PROXY Protocol (v1 and v2) Architecture](#6-proxy-protocol-v1-and-v2-architecture)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Security & Performance Trade-Offs](#8-security--performance-trade-offs)
9. [Architectural Comparison: HTTP Headers vs PROXY Protocol](#9-architectural-comparison-http-headers-vs-proxy-protocol)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

When NGINX operates behind upstream infrastructure—such as Content Delivery Networks (Cloudflare, Fastly), cloud load balancers (AWS ALB, Google Cloud External HTTPS LB), or Kubernetes Ingress controllers—the TCP connection terminates at the intermediate proxy:
- **Loss of True Origin IP**: The socket descriptor IP (`$remote_addr`) reflects the private or public IP of the intermediate proxy, not the end user.
- **Collapsing Rate Limits & GeoIP**: Rate limiting by `$binary_remote_addr` treats millions of distinct users as a single client originating from the CDN's egress pool, causing mass outages when a single user triggers a rate limit.
- **Compromised Audit Trails**: Security compliance frameworks (PCI-DSS, SOC 2, HIPAA) mandate logging true client IPs for every transactional request.
- **IP Spoofing Vulnerabilities**: Naively reading the first element of `X-Forwarded-For` without validating trusted intermediary hops allows attackers to forge arbitrary client IPs simply by passing a custom header (`X-Forwarded-For: 1.1.1.1`), completely bypassing IP-based firewalls.

The `ngx_http_realip_module` and PROXY Protocol resolve this by establishing an authoritative, cryptographically sound chain of custody for origin IP addresses.

---

## 2. Core Concepts & The IP Spoofing Threat

### Understanding `X-Forwarded-For` Structure
The `X-Forwarded-For` (XFF) header is an append-only comma-separated list of IP addresses:
```http
X-Forwarded-For: <client>, <proxy1>, <proxy2>
```
Every intermediate proxy is supposed to append the IP address of the node that connected to it.

### The Forged Header Attack Scenario
1. An attacker sends a request with a forged header:
   ```http
   GET /admin HTTP/1.1
   Host: internal.example.com
   X-Forwarded-For: 10.0.0.1
   ```
2. The request passes through an external load balancer (IP `198.51.100.25`).
3. The load balancer appends the attacker's actual IP (`203.0.113.88`):
   ```http
   X-Forwarded-For: 10.0.0.1, 203.0.113.88
   ```
4. **Vulnerable NGINX**: If NGINX naively reads the leftmost IP (`10.0.0.1`), the attacker successfully spoofs an internal VPC administrator IP and bypasses access controls!
5. **Secure NGINX (`real_ip_recursive on`)**: NGINX begins parsing from the *rightmost* IP, validates that `198.51.100.25` is trusted, and stops at `203.0.113.88` because it is untrusted. The true client IP is correctly identified as `203.0.113.88`.

---

## 3. How It Works Internally

### Phase Execution of `ngx_http_realip_module`

```
  Incoming TCP Socket
  - c->sockaddr = 10.0.1.50 (AWS ALB Private IP)
          │
          ▼
  NGX_HTTP_POST_READ_PHASE
  - Executed immediately after reading raw HTTP request headers
          │
          ▼
  Is c->sockaddr in `set_real_ip_from` CIDR table?
          │
          ├── NO: Do nothing. $remote_addr remains 10.0.1.50.
          │
          └── YES:
              Inspect configured `real_ip_header` (e.g. X-Forwarded-For)
              If `real_ip_recursive on`:
                1. Inspect rightmost IP in header list.
                2. Is it in `set_real_ip_from`?
                   - YES: Strip it, step left, and repeat.
                   - NO:  This is the first untrusted IP!
                3. Overwrite `r->connection->sockaddr` and `$remote_addr`.
                4. Original connection IP preserved in `$realip_remote_addr`.
```

By executing in the `POST_READ` phase, the Real IP module overwrites the connection's socket address before any subsequent phase runs. As a result, the `rewrite`, `access` (`allow`/`deny`), `limit_req`, and `log` phases automatically see the true client IP transparently.

---

## 4. Syntax & Directives Reference

### Directives
- `set_real_ip_from <CIDR | IP | unix:>;`: Defines an authoritative trusted IP address or subnet that is permitted to assert client origin headers. Can be declared multiple times.
- `real_ip_header <X-Forwarded-For | X-Real-IP | proxy_protocol | <custom>>;`: Specifies which header or protocol mechanism contains the client IP.
- `real_ip_recursive on | off;`:
  - `off` (default): Replaces the client address with the last address in `real_ip_header`.
  - `on`: Enables recursive right-to-left traversal, replacing `$remote_addr` with the first *untrusted* address in the chain.
- `$realip_remote_addr`: Read-only variable preserving the raw physical TCP socket address of the immediate peer before the real IP module replaced it.

---

## 5. Practical Implementation Examples

### Example 1: Restoring Client IPs Behind AWS ALB / NLB

```nginx
# /etc/nginx/conf.d/aws-realip.conf

http {
    # 1. Trust AWS VPC Subnet CIDRs where ALBs reside
    set_real_ip_from 10.0.0.0/16;
    set_real_ip_from 172.16.0.0/12;

    # 2. Extract client IP from X-Forwarded-For
    real_ip_header X-Forwarded-For;

    # 3. Recursively unwind the chain from right to left
    real_ip_recursive on;

    # 4. Standardized Logging to Verify Client and Proxy IPs
    log_format real_combined '$remote_addr (proxy: $realip_remote_addr) - $remote_user '
                             '[$time_local] "$request" $status $body_bytes_sent '
                             '"$http_referer" "$http_user_agent"';

    access_log /var/log/nginx/access.log real_combined;
}
```

### Example 2: Complete Cloudflare Edge Integration

Cloudflare publishes an authoritative list of public IPv4 and IPv6 egress ranges. Never trust `0.0.0.0/0`!

```nginx
# /etc/nginx/snippets/cloudflare-realip.conf

# Cloudflare IPv4 CIDRs
set_real_ip_from 173.245.48.0/20;
set_real_ip_from 103.21.244.0/22;
set_real_ip_from 103.22.200.0/22;
set_real_ip_from 103.31.4.0/22;
set_real_ip_from 141.101.64.0/18;
set_real_ip_from 108.162.192.0/18;
set_real_ip_from 190.93.240.0/20;
set_real_ip_from 188.114.96.0/20;
set_real_ip_from 197.234.240.0/22;
set_real_ip_from 198.41.128.0/17;
set_real_ip_from 162.158.0.0/15;
set_real_ip_from 104.16.0.0/13;
set_real_ip_from 104.24.0.0/14;
set_real_ip_from 172.64.0.0/13;
set_real_ip_from 131.0.72.0/22;

# Cloudflare IPv6 CIDRs
set_real_ip_from 2400:cb00::/32;
set_real_ip_from 2606:4700::/32;
set_real_ip_from 2803:f800::/32;
set_real_ip_from 2405:b500::/32;
set_real_ip_from 2405:8100::/32;
set_real_ip_from 2a06:98c0::/29;
set_real_ip_from 2c0f:f248::/32;

# Prefer CF-Connecting-IP over standard X-Forwarded-For
real_ip_header CF-Connecting-IP;
real_ip_recursive on;
```

### Example 3: Multi-Tiered Proxy Chain (Cloudflare -> AWS ALB -> NGINX)

When traffic traverses both a public CDN and an internal cloud load balancer:

```nginx
# /etc/nginx/conf.d/multi-tier.conf

# 1. Trust ALB Private Subnet
set_real_ip_from 10.0.1.0/24;

# 2. Trust Cloudflare Public Edge Ranges
include /etc/nginx/snippets/cloudflare-realip.conf;

# 3. Use X-Forwarded-For with recursion
real_ip_header X-Forwarded-For;
real_ip_recursive on;

server {
    listen 80;
    server_name api.example.com;

    location / {
        # Rate limit now safely isolates real users
        limit_req zone=api_flood burst=10 nodelay;

        proxy_pass http://127.0.0.1:8000;
        proxy_set_header Host $host;
        # Forward true verified client IP to backend application
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }
}
```

---

## 6. PROXY Protocol (v1 and v2) Architecture

HTTP headers like `X-Forwarded-For` only work for HTTP/HTTPS traffic where the proxy can parse application layer payloads. For raw Layer 4 TCP/UDP proxies, TLS pass-through, or AWS Network Load Balancers (NLB), HAProxy's **PROXY Protocol** prepends a small binary or text header to the beginning of the TCP stream immediately following connection establishment:

```
+-------------------------------------------------------------------------------+
| PROXY Protocol Architecture                                                   |
|                                                                               |
|  [Client: 198.51.100.77:54321]                                               |
|        │                                                                      |
|        ▼                                                                      |
|  [AWS Network Load Balancer (NLB) / HAProxy]                                  |
|        │  Opens TCP connection to NGINX port 443                              |
|        │  Sends PROXY Header: "PROXY TCP4 198.51.100.77 10.0.1.20 54321 443\r\n"|
|        │  Followed immediately by TLS ClientHello                             |
|        ▼                                                                      |
|  [NGINX: listen 443 ssl proxy_protocol;]                                      |
|        │  Reads PROXY header from socket before starting TLS handshake        |
|        │  Sets $proxy_protocol_addr = 198.51.100.77                           |
|        │  If `real_ip_header proxy_protocol;`, restores $remote_addr!         |
+-------------------------------------------------------------------------------+
```

### PROXY Protocol Implementation in `nginx.conf`

```nginx
# /etc/nginx/conf.d/nlb-proxy-protocol.conf

server {
    # 1. Enable proxy_protocol on the listening socket
    listen 443 ssl proxy_protocol;
    server_name secure.example.com;

    ssl_certificate /etc/letsencrypt/live/secure.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/secure.example.com/privkey.pem;

    # 2. Restrict PROXY Protocol parsing to trusted NLB IP addresses ONLY
    set_real_ip_from 10.0.0.0/16;
    real_ip_header proxy_protocol;

    location / {
        proxy_pass http://127.0.0.1:5000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }
}
```

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern / Mistake | Root Cause | Observable Production Impact | Correct Solution |
| :--- | :--- | :--- | :--- |
| `set_real_ip_from 0.0.0.0/0;` | Trusting the entire public internet to supply client headers | Any client can forge `X-Forwarded-For` and spoof any IP address | Restrict `set_real_ip_from` strictly to verified CDN/LB CIDRs |
| Missing `real_ip_recursive on;` | NGINX stops at the immediate rightmost header value | Fails to resolve origin IP when requests traverse multiple proxies | Always configure `real_ip_recursive on;` in multi-proxy chains |
| Enabling `proxy_protocol` on Open Ports | `listen ... proxy_protocol;` on ports directly exposed to internet | Standard browsers and health checks crash with SSL handshake errors | Only enable `proxy_protocol` on internal ports facing the NLB |
| Trusting `X-Real-IP` Behind Multi-Hop Proxies | Intermediate proxies may overwrite or ignore `X-Real-IP` | Inconsistent client identification across microservices | Standardize on `X-Forwarded-For` or `CF-Connecting-IP` |
| Forgetting to Include Cloudflare IPv6 Ranges | Clients connecting via IPv6 bypass the IPv4 CIDR whitelist | IPv6 users are treated as untrusted; rate limits fail | Always include both IPv4 and IPv6 subnets in configuration |
| Overwriting `X-Forwarded-For` in `proxy_pass` | Using `proxy_set_header X-Forwarded-For $remote_addr;` | Drops the entire upstream chain history for backend services | Use `proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;` |

---

## 8. Security & Performance Trade-Offs

- **Security (Preventing Access Control Bypasses)**: Without recursive validation, access controls like `allow 192.168.1.0/24; deny all;` are completely ineffective if an attacker injects `X-Forwarded-For: 192.168.1.50`.
- **CPU & Memory Overhead**: The `ngx_http_realip_module` processes IP address lookups using an internal radix tree (`ngx_radix_tree_t`). Lookups for trusted CIDRs execute in $O(1)$ time and introduce less than 5 microseconds of overhead per transaction.
- **PROXY Protocol Performance**: PROXY Protocol v2 (binary format) parses faster than PROXY Protocol v1 (text format) and avoids string-splitting allocations.

---

## 9. Architectural Comparison: HTTP Headers vs PROXY Protocol

| Feature / Dimension | HTTP Header (`X-Forwarded-For`) | PROXY Protocol (v1 / v2) |
| :--- | :--- | :--- |
| **OSI Layer** | Layer 7 (Application) | Layer 4 / Layer 5 (Transport/Session) |
| **TLS Handshake Interaction**| Evaluated *after* TLS decryption | Evaluated *before* TLS decryption |
| **Protocol Support** | HTTP, HTTPS, WebSockets | Any TCP/UDP protocol (HTTP, SSL, SMTP, DB) |
| **Overhead** | String parsing, header mutation | 16–32 byte binary or ASCII prefix |
| **Spoofing Surface** | High if `set_real_ip_from` is permissive | Minimal; enforced at socket handshake |
| **Load Balancer Support** | AWS ALB, Cloudflare, Traefik | AWS NLB, HAProxy, Envoy, F5 BIG-IP |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. Explain how `real_ip_recursive on;` works and why it is critical for security in multi-proxy topologies.</strong></summary>

When a request arrives with an `X-Forwarded-For` header containing multiple IPs (e.g. `Client, ProxyA, ProxyB`), and the connection is received from `ProxyC`:
- With `real_ip_recursive off` (default), NGINX checks if `ProxyC` is in `set_real_ip_from`. If trusted, it replaces `$remote_addr` with the last address in the header list (`ProxyB`), regardless of whether `ProxyB` is trusted or untrusted.
- With `real_ip_recursive on`, NGINX traverses the IP list from right to left. It checks `ProxyB`; if `ProxyB` is also trusted in `set_real_ip_from`, it discards it and checks `ProxyA`. It continues stepping left until it encounters the **first untrusted IP address** in the chain. This untrusted address is guaranteed to be the true client because every subsequent hop to the right was verified and trusted. This prevents an attacker from prepending fake IPs to the left of the header.
</details>

<details>
<summary><strong>2. What is the difference between `$remote_addr` and `$realip_remote_addr`?</strong></summary>

- `$remote_addr`: Represents the active client IP address used by NGINX for logging, rate limiting, and access control. If `ngx_http_realip_module` is enabled and matches a trusted proxy, `$remote_addr` is rewritten to the restored origin client IP.
- `$realip_remote_addr`: Always retains the original, physical TCP socket IP address from which NGINX received the network packets (i.e., the IP of the immediate load balancer, CDN node, or peer), regardless of any real IP module rewriting.
</details>

<details>
<summary><strong>3. Why will an NGINX server fail if a direct browser connection is made to a port configured with `listen 443 ssl proxy_protocol;`?</strong></summary>

When `proxy_protocol` is enabled on a listening socket, NGINX expects the very first bytes arriving on the TCP socket to match the PROXY Protocol specification (e.g. `PROXY TCP4 ...` or the v2 binary signature `\x0d\x0a\x0d\x0a\x00\x0d\x0a\x51\x55\x49\x54\x0a`). A standard browser connecting directly sends a TLS `ClientHello` frame instead. Because the initial bytes do not match the PROXY Protocol signature, NGINX immediately terminates the TCP connection or returns a handshake error. Ports with `proxy_protocol` must only receive traffic from load balancers that inject the header.
</details>

<details>
<summary><strong>4. How do you prevent attackers from bypassing your CDN and accessing your NGINX origin directly?</strong></summary>

1. **Firewall / Security Group Whitelisting**: At the cloud firewall/security group level, allow inbound traffic on ports 80/443 only from the published IP subnets of the CDN.
2. **mTLS (Mutual TLS)**: Require client certificates from the CDN (e.g. Cloudflare Authenticated Origin Pulls) using `ssl_client_certificate` and `ssl_verify_client on;`.
3. **Shared Secret Header Verification**: Enforce a cryptographic secret header injected by the CDN at the origin using `map` or access checks.
</details>

<details>
<summary><strong>5. What is the difference between `$proxy_add_x_forwarded_for` and `$remote_addr` when forwarding traffic upstream?</strong></summary>

- `$remote_addr` contains a single IP address (the client or restored real IP).
- `$proxy_add_x_forwarded_for` is an automatic chaining variable: it takes the incoming `X-Forwarded-For` header received from the client (if any) and appends `$remote_addr` to the end, separated by a comma. If no `X-Forwarded-For` header was present, it initializes the header with `$remote_addr`.
</details>

<details>
<summary><strong>6. Why is `set_real_ip_from 0.0.0.0/0;` considered a critical security vulnerability?</strong></summary>

Trusting `0.0.0.0/0` instructs NGINX to trust *any* device connecting to it as an authoritative proxy. An attacker can connect directly to NGINX and supply an arbitrary header: `X-Forwarded-For: 127.0.0.1`. NGINX will accept this header and rewrite `$remote_addr` to `127.0.0.1`. Any endpoint protected by `allow 127.0.0.1; deny all;` will be instantly bypassed, and rate limiters can be manipulated by sending randomized IP strings.
</details>

<details>
<summary><strong>7. How does Kubernetes Ingress handle Real IP resolution when routing through NodePort vs ClusterIP?</strong></summary>

In Kubernetes, if traffic routes through an external load balancer to a `NodePort` service without `externalTrafficPolicy: Local`, kube-proxy performs SNAT (Source Network Address Translation), replacing the client IP with the internal Node IP. To preserve client IPs:
1. Configure `service.spec.externalTrafficPolicy: Local` on the Ingress Service, preserving the external client IP on the node.
2. In the NGINX Ingress Controller configuration, set `use-forwarded-headers: "true"` and `compute-full-forwarded-for: "true"`.
</details>

<details>
<summary><strong>8. What data does PROXY Protocol v2 encapsulate beyond client IP addresses?</strong></summary>

PROXY Protocol v2 supports Type-Length-Value (TLV) extension fields that can transmit:
- Client SSL/TLS connection parameters (cipher used, TLS version, client certificate verification status).
- Unique connection IDs generated by the load balancer.
- Application-specific metadata (ALPN negotiated protocol, virtual host information).
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Real IP Rewriting via `curl` Header Injection</strong></summary>

**Goal**: Observe how NGINX parses `X-Forwarded-For`.
1. Configure `set_real_ip_from 127.0.0.1; real_ip_header X-Forwarded-For;`.
2. Add a debug response header in `server {}`:
```nginx
add_header X-Echo-Client-IP "$remote_addr" always;
add_header X-Echo-Socket-IP "$realip_remote_addr" always;
```
3. Send a test curl request:
```bash
curl -I -H "X-Forwarded-For: 198.51.100.44" http://127.0.0.1/
```
4. Verify `X-Echo-Client-IP` reports `198.51.100.44` and `X-Echo-Socket-IP` reports `127.0.0.1`.
</details>

<details>
<summary><strong>Exercise 2: Demonstrate the Spoofing Vulnerability of Missing Recursion</strong></summary>

**Goal**: Understand why `real_ip_recursive on;` is mandatory.
1. Configure trusted proxy `127.0.0.1`, `real_ip_header X-Forwarded-For;`, and `real_ip_recursive off;`.
2. Send:
```bash
curl -I -H "X-Forwarded-For: 1.1.1.1, 127.0.0.1" http://127.0.0.1/
```
3. Notice `$remote_addr` evaluates to `127.0.0.1`.
4. Switch to `real_ip_recursive on;` and re-test. Observe `$remote_addr` correctly unwinds to `1.1.1.1`.
</details>

<details>
<summary><strong>Exercise 3: Automated Syncing of Cloudflare IP Ranges</strong></summary>

**Goal**: Build a bash script to update `/etc/nginx/snippets/cloudflare-realip.conf` dynamically.
1. Write a script `update_cf_ips.sh`:
```bash
#!/usr/bin/env bash
CONF="/etc/nginx/snippets/cloudflare-realip.conf"
echo "# Generated Cloudflare IPs" > $CONF
for ip in $(curl -s https://www.cloudflare.com/ips-v4); do
    echo "set_real_ip_from $ip;" >> $CONF
done
for ip in $(curl -s https://www.cloudflare.com/ips-v6); do
    echo "set_real_ip_from $ip;" >> $CONF
done
echo "real_ip_header CF-Connecting-IP;" >> $CONF
echo "real_ip_recursive on;" >> $CONF
nginx -t && systemctl reload nginx
```
2. Schedule the script in cron to execute weekly.
</details>

<details>
<summary><strong>Exercise 4: Test PROXY Protocol v1 Using Netcat</strong></summary>

**Goal**: Manually craft a PROXY Protocol string.
1. In `nginx.conf`, set:
```nginx
server {
    listen 8888 proxy_protocol;
    return 200 "Client IP: $remote_addr\n";
}
```
2. Inject the header via netcat:
```bash
echo -e "PROXY TCP4 203.0.113.99 127.0.0.1 5555 8888\r\nGET / HTTP/1.1\r\nHost: test\r\n\r\n" | nc 127.0.0.1 8888
```
3. Confirm the output displays `Client IP: 203.0.113.99`.
</details>

<details>
<summary><strong>Exercise 5: Verify Access Module Interaction with Restored IP</strong></summary>

**Goal**: Ensure `allow`/`deny` blocks evaluate the real IP, not the proxy IP.
1. Configure location:
```nginx
location /internal/ {
    allow 203.0.113.0/24;
    deny all;
}
```
2. Test requests from localhost with and without spoofed headers to verify that access decisions respect trusted CIDRs.
</details>

<details>
<summary><strong>Exercise 6: Audit Log Formatting for Compliance</strong></summary>

**Goal**: Ensure access logs capture both client IP and load balancer socket addresses.
1. Implement the `log_format real_combined` format from Example 1.
2. Tail `/var/log/nginx/access.log` while simulating requests across proxies.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| NGINX Real IP Directive Reference                                              |
|                                                                                |
| Directive                     | Context         | Purpose                      |
| :---------------------------- | :-------------- | :--------------------------- |
| set_real_ip_from <CIDR>;      | http, srv, loc  | Whitelists trusted proxy IP  |
| real_ip_header X-Forwarded-For| http, srv, loc  | Header carrying client IP    |
| real_ip_header CF-Connecting..| http, srv, loc  | Cloudflare specific header   |
| real_ip_header proxy_protocol;| http, srv, loc  | PROXY Protocol extraction    |
| real_ip_recursive on;         | http, srv, loc  | Unwinds multi-proxy chains   |
| $remote_addr                  | Variable        | Restored origin client IP     |
| $realip_remote_addr           | Variable        | Immediate TCP socket IP      |
| $proxy_protocol_addr          | Variable        | IP from PROXY Protocol header|
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [NGINX Official Documentation: Module ngx_http_realip_module](https://nginx.org/en/docs/http/ngx_http_realip_module.html)
- [HAProxy Technologies: The PROXY Protocol Specification](https://www.haproxy.org/download/1.8/doc/proxy-protocol.txt)
- [Cloudflare Official: Restoring Original Visitor IPs](https://developers.cloudflare.com/support/troubleshooting/restoring-visitor-ips/restoring-original-visitor-ips/)
- [Next Concept: Benchmarking Methodologies & Load Generation](06-benchmarking-methodologies.md)

---

[← Previous Note: 04-ddos-mitigation-patterns.md](04-ddos-mitigation-patterns.md) | [Back to Module Index](README.md) | [Next Note: 06-benchmarking-methodologies.md →](06-benchmarking-methodologies.md)
