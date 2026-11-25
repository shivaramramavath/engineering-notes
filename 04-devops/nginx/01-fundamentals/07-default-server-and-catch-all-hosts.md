<!--
================================================================================
Metadata:
  Title: Default Server, Catch-All Hosts & Preventing Host-Header Attacks
  Track: NGINX & Traffic Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 01-fundamentals/03-server-and-location.md
  Key Terminology: default_server, Host Header, SNI Fallthrough, Return 444, Cache Poisoning, Port Scanning, Catch-All
================================================================================
-->

# Default Server, Catch-All Hosts & Preventing Host-Header Attacks

[<- 06-command-line-and-signals.md](./06-command-line-and-signals.md) | [Back to Module Overview](./README.md) | [Next Module: 02-core-features ->](../02-core-features/README.md)

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Troubleshooting](#7-common-mistakes--troubleshooting)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference Cheat Sheet](#12-summary--quick-reference-cheat-sheet)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

When an HTTP client connects to an NGINX server, it specifies the destination domain name in the `Host` request header (and during TLS handshakes via the Server Name Indication / SNI extension).

However, internet automated port scanners (Shodan, Censys, masscan), malicious botnets, and penetration testers do not query domains—they scan raw IP addresses (e.g. `http://203.0.113.10/`). Furthermore, attackers frequently forge arbitrary `Host` headers to execute **Host Header Injection**, **Web Cache Poisoning**, and **Password Reset Poisoning** attacks.

If NGINX is not configured with an explicit **`default_server`** catch-all block:
- NGINX falls back to routing unmatched traffic to the **very first server block loaded into memory**.
- If your first loaded virtual host is an administrative portal, a private internal API, or an SSL application, the raw IP scan will inadvertently expose that service and its TLS certificate to public reconnaissance.

Configuring an explicit, hardened catch-all server that terminates unrecognized requests using NGINX's special status code `444` (No Response) is an absolute security requirement for production infrastructure.

---

## 2. Core Concepts

### The `default_server` Directive
The `default_server` parameter is attached to the `listen` directive inside a `server {}` block:

```nginx
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name _;
    return 444;
}
```

- **`server_name _;`**: The underscore (`_`) is a conventional placeholder that never matches any real domain name.
- **`default_server`**: Tells NGINX: *"If an incoming connection on this port does not match the `server_name` of any other virtual host, route it here."*
- **The Implicit Fallback Trap**: If you omit `default_server` from all server blocks on a port, NGINX automatically designates the **first server block defined in the configuration files** as the default server!

### The Non-Standard HTTP Status Code `444`
NGINX introduces a custom internal return code: **`444` (No Response)**.
When NGINX encounters `return 444;`:
- It sends **zero bytes** of HTTP response headers.
- It sends **zero bytes** of response body.
- It closes the TCP connection immediately with a TCP RST (reset) or FIN packet.

For automated port scanners and vulnerability bots, `444` causes the scanner's connection to drop or time out without revealing whether an HTTP server is even running.

---

## 3. How It Works Internally

### Request Routing & SNI Fallthrough Flow
The following sequence details how NGINX evaluates an incoming HTTP/HTTPS request against virtual hosts:

```
+--------------------------------------------------------------------------+
| HOST HEADER & DEFAULT SERVER RESOLUTION FLOW                             |
|                                                                          |
| 1. Client opens TCP connection to port 443 (e.g., 203.0.113.10:443)      |
| 2. TLS Client Hello arrives with SNI: "internal-api.attacker.com"         |
|                                                                          |
|       +---------------------------------------------+                    |
|       | Does SNI match any configured server_name?  |                    |
|       +---------------------------------------------+                    |
|             /                                 \                          |
|         YES/                                   \ NO                      |
|           v                                     v                        |
|  [ Target Virtual Host ]               [ Check default_server ]          |
|  Terminates TLS with                   Does an explicit                  |
|  valid domain certificate.             default_server exist on port 443? |
|                                              /             \             |
|                                          YES/               \ NO         |
|                                            v                 v           |
|                                 [ Catch-All Block ]     [ FIRST BLOCK ]  |
|                                 Terminates with dummy   NGINX uses 1st   |
|                                 cert, drops conn (444)! loaded block!    |
|                                                         EXPOSES CERT &   |
|                                                         APP TO ATTACKER! |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Hardened Production Catch-All Server Blocks
Create a dedicated file `/etc/nginx/conf.d/00-default-server.conf` (naming it `00-` ensures it loads first alphabetically):

```nginx
# /etc/nginx/conf.d/00-default-server.conf

# 1. Catch-all for plain HTTP (Port 80)
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name _;

    # Drop connection immediately without sending response
    return 444;
}

# 2. Catch-all for encrypted HTTPS (Port 443)
server {
    listen 443 ssl default_server;
    listen [::]:443 ssl default_server;
    server_name _;

    # Self-signed placeholder certificates for handshake completion
    ssl_certificate /etc/nginx/ssl/catchall.crt;
    ssl_certificate_key /etc/nginx/ssl/catchall.key;

    # Drop connection immediately
    return 444;
}
```

---

## 5. Practical Examples

### Example 1: Generating Self-Signed Placeholder Certificates for HTTPS Catch-All
```bash
# HTTPS requires a valid cryptographic handshake before NGINX can parse the HTTP request
# Generate an untrusted, blank placeholder certificate for the catch-all server:
sudo mkdir -p /etc/nginx/ssl

sudo openssl req -x509 -nodes -days 3650 -newkey rsa:2048 \
  -keyout /etc/nginx/ssl/catchall.key \
  -out /etc/nginx/ssl/catchall.crt \
  -subj "/CN=untrusted-catchall/O=Quarantine"

sudo chmod 0600 /etc/nginx/ssl/catchall.key
sudo chmod 0644 /etc/nginx/ssl/catchall.crt
```

### Example 2: Simulating Host-Header Attack and Port Scan Rejection
```bash
# 1. Normal legitimate request matching approved domain
curl -I -H "Host: api.example.com" http://localhost/
# Output: HTTP/1.1 200 OK

# 2. Attack attempt: Sending forged Host header
curl -I -H "Host: evil-attacker.com" http://localhost/
# Output: curl: (52) Empty reply from server (Connection closed with 444!)

# 3. Port scanner hitting raw IP address
curl -I http://127.0.0.1/
# Output: curl: (52) Empty reply from server
```

### Example 3: Canonical Domain Redirection via Specific Virtual Host
```nginx
# Instead of dropping requests, redirect uncanonical domains (www -> non-www)
# Dedicated redirector for known alternative domains:
server {
    listen 80;
    listen 443 ssl;
    server_name www.example.com example.net www.example.net;

    ssl_certificate /etc/letsencrypt/live/example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/example.com/privkey.pem;

    # Canonical permanent redirect
    return 301 https://example.com$request_uri;
}

# Real application server
server {
    listen 443 ssl;
    server_name example.com;
    # ... application configuration ...
}

# Global catch-all drops everything else
server {
    listen 80 default_server;
    listen 443 ssl default_server;
    server_name _;
    ssl_certificate /etc/nginx/ssl/catchall.crt;
    ssl_certificate_key /etc/nginx/ssl/catchall.key;
    return 444;
}
```

### Example 4: Logging Unmatched Traffic to Security Telemetry Log
```nginx
# Log unrecognized host scans to a separate quarantine log for fail2ban / SIEM
server {
    listen 80 default_server;
    listen 443 ssl default_server;
    server_name _;

    ssl_certificate /etc/nginx/ssl/catchall.crt;
    ssl_certificate_key /etc/nginx/ssl/catchall.key;

    # Dedicated log for intrusion analysis
    access_log /var/log/nginx/unmatched_scanners.log combined;
    error_log /var/log/nginx/unmatched_errors.log warn;

    return 444;
}
```

### Example 5: Defending Against Web Cache Poisoning via Host Header
```nginx
# VULNERABLE: Application generates absolute links using NGINX $host
# proxy_set_header Host $http_host; # Vulnerable if Host is untrusted!

# HARDENED PATTERN:
# In virtual host, enforce exact trusted domain:
server {
    listen 443 ssl;
    server_name app.example.com;

    location / {
        proxy_pass http://node_upstream;
        
        # Hardcode or strictly pass validated host
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-Host $server_name;
    }
}
```

---

## 6. Best Practices

- **Always Create an Explicit `default_server` on Every Bound Port**: Never leave port 80 or 443 without an explicit default server. Leaving it implicit guarantees that your first loaded application handles raw IP scanner traffic.
- **Return `444` for Unknown Hosts**: Returning `444` wastes zero server bandwidth, sends no response headers, and denies attackers confirmation of which web server software or version is running.
- **Prefix Catch-All Configs with `00-`**: Name your catch-all file `/etc/nginx/conf.d/00-default.conf`. Even if `default_server` was accidentally omitted from the listen line, naming it `00-` guarantees it loads first alphabetically, acting as the implicit default server.
- **Never Use Production Certificates on Catch-All Blocks**: If a scanner hits your raw IP over HTTPS, the TLS handshake requires presenting a certificate. If you use `example.com`'s real certificate on the catch-all, the scanner immediately links your IP address to your company domain! Use a generic self-signed dummy certificate.
- **Validate Host Headers in Upstream Proxies**: When reverse proxying to backend web frameworks (Django, Rails, Spring Boot), pass `$host` (which NGINX validates against `server_name`), not raw `$http_host`.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| `listen ... default_server` declared twice on same port | NGINX can have only ONE default server per IP/port combination; fails with `a duplicate default server`. | Remove `default_server` from all blocks except the designated catch-all. |
| Catch-all for port 443 lacks SSL certificate directives | SSL handshake fails with `no "ssl_certificate" is defined for the "listen ... ssl"`. | Provide self-signed dummy certificate and key in the catch-all block. |
| Port scanners discover real domain on raw IP HTTPS scan | Production wildcard certificate was assigned to the default server block. | Generate an untrusted self-signed certificate for the catch-all. |
| Returning `400 Bad Request` instead of `444` | Still consumes bandwidth and leaks NGINX default error page with server signature. | Use `return 444;` to sever the TCP connection cleanly. |
| Forgetting IPv6 default server | IPv4 scanners are blocked, but IPv6 requests (`[2001:db8::1]`) hit real applications. | Include `listen [::]:80 default_server;` and `listen [::]:443 ssl default_server;`. |
| Subdomain takeover via wildcard `server_name *.example.com;` without validation | Resolves unallocated subdomains to production endpoints. | Explicitly enumerate known subdomains or quarantine unknown subdomains. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Returning `444` aborts request processing during the early HTTP header parsing phase. Zero bytes of response memory buffers are allocated, and zero backend upstream sockets are opened.
- **Security**: Neutralizes automated reconnaissance and scanning engines. Shodan and Censys index hosts by scanning public IPv4 ranges; if your server returns `444` or terminates TLS with an unmapped dummy cert, your origin IP is omitted from public internet scan maps, shielding you from direct DDoS attacks that bypass CDNs.
- **Cost**: Prevents origin bandwidth charges from malicious scrapers crawling your site via raw IP addresses or alternate host headers.

---

## 9. Comparison Table

| Catch-All Strategy | Client Experience | Security Stance | Bandwidth Consumed |
| :--- | :--- | :--- | :--- |
| **`return 444;` (Recommended)**| "Empty reply from server" / Connection reset | **Highest**: Denies information, aborts connection | **0 Bytes** (pure TCP reset) |
| **`return 400;`** | HTTP 400 Bad Request HTML page | Moderate: Returns standard error | ~300 Bytes per request |
| **Redirect to Canonical (`301`)**| Redirects to `https://primary.com` | Low: Informs attacker of your primary domain | ~450 Bytes per request |
| **Implicit First Server Block** | Serves the first application loaded | **Critical Vulnerability**: Leaks app & certs | Full page bandwidth |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What happens if an NGINX configuration contains five `server {}` blocks listening on port 80, but NONE of them specify `default_server`?</strong></summary>

**Answer**:
If no server block explicitly contains the `default_server` parameter for a given IP/port combination, NGINX automatically designates the **very first server block parsed in configuration order** as the implicit default server.

If a client sends an HTTP request with an unrecognized `Host` header (or connects via raw IP), NGINX will route the request to that first server block. In production, this can accidentally serve internal dashboards or private applications to automated internet crawlers.
</details>

<details>
<summary><strong>2. Why is HTTP status code `444` considered a powerful security tool in NGINX? How does it differ from `400` or `403`?</strong></summary>

**Answer**:
`400 Bad Request` and `403 Forbidden` are standard HTTP status codes: NGINX generates response headers, sends an HTML error body, and cleanly completes an HTTP exchange, confirming to the client that an active web server exists and potentially leaking server signature tokens.

`444 No Response` is a proprietary NGINX internal directive: it instructs NGINX to close the TCP connection immediately without sending any response headers or data packets. For an automated attacker or scanner, the connection is abruptly reset or times out. It wastes zero outbound server bandwidth and provides no technical clues regarding the underlying server software.
</details>

<details>
<summary><strong>3. Why must an HTTPS catch-all server block on port 443 configure an SSL certificate even if it immediately returns `444`?</strong></summary>

**Answer**:
In the TLS protocol sequence, the **TLS Handshake occurs before any HTTP data is transmitted**:
1. The client connects and sends the TLS `ClientHello` packet (containing SNI).
2. The server **must** respond with a TLS `ServerHello` and present a valid X.509 certificate to establish an encrypted tunnel.
3. Only *after* the TLS handshake completes does the client send the encrypted `GET / HTTP/1.1` request line and `Host` header.

If NGINX does not have an `ssl_certificate` defined in the default server block on port 443, it cannot complete the TLS handshake and fails with a fatal configuration error during startup. Therefore, an HTTPS catch-all must provide a dummy or self-signed certificate to complete the handshake before executing `return 444;`.
</details>

<details>
<summary><strong>4. How does an attacker exploit Host Header Injection to achieve Web Cache Poisoning?</strong></summary>

**Answer**:
Many web applications dynamically generate absolute URLs for password reset links or asset imports using the `Host` header provided in the request (e.g. `<script src="http://" + request.getHeader("Host") + "/app.js">`).

If NGINX forwards an arbitrary `Host` header (such as `evil-attacker.com`) to the backend, and the application generates a response containing links to `http://evil-attacker.com/app.js`:
If NGINX or an edge CDN caches this response without including the `Host` header in the cache key, subsequent legitimate users requesting the page will receive the poisoned cached response, forcing their browsers to execute malicious JavaScript hosted by the attacker.
</details>

<details>
<summary><strong>5. What is the significance of `server_name _;` in catch-all server blocks?</strong></summary>

**Answer**:
The underscore (`_`) is simply an arbitrary invalid domain name. In DNS standards (RFC 1035), domain names cannot consist solely of an underscore.

Because `_` will never match any legitimate domain name requested by a client, it guarantees that the server block will never match traffic via standard exact or regex matching rules. The block will **only** ever be reached if all other server blocks fail to match and NGINX falls back to the `default_server` listener.
</details>

<details>
<summary><strong>6. How do you prevent attackers from discovering your origin IP address when using Cloudflare or AWS CloudFront?</strong></summary>

**Answer**:
1. Configure an explicit `default_server` on port 80 and 443 that returns `444` and uses self-signed dummy certificates. Any scanner hitting the origin IP directly is immediately dropped.
2. Configure real virtual host blocks to listen only for approved domain names.
3. In NGINX, restrict access to the real application blocks so they only accept traffic originating from the CDN's published IP ranges using `allow <cdn_ip_range>; deny all;`.
4. Enforce client certificate validation (mTLS) between the CDN and NGINX (`ssl_client_certificate`), ensuring that even if an attacker discovers the origin IP, NGINX rejects connections that do not present the CDN's private client certificate.
</details>

<details>
<summary><strong>7. What is the difference between `$host`, `$http_host`, and `$server_name` in NGINX?</strong></summary>

**Answer**:
- **`$http_host`**: Contains the exact, raw, unmodified `Host` header string sent by the client, including any port numbers (e.g. `example.com:8080`) or malicious injection strings.
- **`$host`**: Evaluated in priority order:
  1. The host name from the request line.
  2. The host name from the `Host` request header (converted to lowercase, with port stripped).
  3. The `server_name` of the matching server block if `Host` was missing.
  *(This is the safest variable to use in proxy headers).*
- **`$server_name`**: The primary string declared in the `server_name` directive of the server block that accepted the request.
</details>

<details>
<summary><strong>8. Why should you configure default servers for both IPv4 (`0.0.0.0`) and IPv6 (`[::]`)?</strong></summary>

**Answer**:
Modern Linux servers operate dual-stack networking. If you declare `listen 80 default_server;` (which binds IPv4 only) and omit IPv6:
An attacker scanning IPv6 addresses will bypass your IPv4 catch-all. Because the IPv6 socket has no explicit default server, NGINX will route the IPv6 connection to the first virtual host listening on `[::]:80`, exposing internal applications to IPv6 scanners. Both address families must explicitly declare `default_server`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Identify the Catch-All Target</strong></summary>

**Task**: In the configuration below, what happens when a client sends `GET / HTTP/1.1` with `Host: random.net`?

```nginx
server {
    listen 80;
    server_name alpha.com;
    return 200 "Alpha\n";
}
server {
    listen 80 default_server;
    server_name _;
    return 444;
}
```

**Solution**:
NGINX routes the request to the second server block because it has `default_server` declared. The connection is dropped immediately with status `444`.
</details>

<details>
<summary><strong>Exercise 2 (Easy): Author Plain HTTP Catch-All Block</strong></summary>

**Task**: Write the complete NGINX server block to drop all unmatched IPv4 and IPv6 traffic on port 80.

**Solution**:
```nginx
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name _;
    return 444;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Generate Self-Signed Catch-All Certificate One-Liner</strong></summary>

**Task**: Write an OpenSSL command that generates an untrusted self-signed RSA certificate and private key in `/tmp/dummy.crt` and `/tmp/dummy.key` valid for 10 years without interactive prompts.

**Solution**:
```bash
openssl req -x509 -nodes -days 3650 -newkey rsa:2048 \
  -keyout /tmp/dummy.key \
  -out /tmp/dummy.crt \
  -subj "/CN=unmatched/O=Quarantine"
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Verify Rejection of Raw IP Requests via cURL</strong></summary>

**Task**: Write a bash script that tests both your legitimate domain and your server's raw loopback IP, verifying that the domain returns 200 and the IP returns empty reply (444).

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

echo "Testing legitimate domain..."
curl -sf -o /dev/null -H "Host: app.local" http://localhost/ && echo "Domain OK (200)"

echo "Testing raw IP scanner..."
STATUS=$(curl -s -o /dev/null -w "%{http_code}" http://127.0.0.1/ || echo "DROPPED")
if [ "$STATUS" = "000" ] || [ "$STATUS" = "DROPPED" ]; then
    echo "Raw IP dropped cleanly by default_server (444 verified)!"
else
    echo "WARNING: Raw IP returned HTTP status $STATUS!"
fi
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Fail2Ban Monitoring on Unmatched Catch-All</strong></summary>

**Task**: Author an NGINX default server configuration that logs unmatched requests to `/var/log/nginx/scanners.log` in a specialized format, paired with a sample Fail2Ban regex filter.

**Solution**:
```nginx
# 1. NGINX Configuration
log_format scanner_log '$remote_addr - [$time_local] "$request" Host:"$http_host"';

server {
    listen 80 default_server;
    server_name _;
    access_log /var/log/nginx/scanners.log scanner_log;
    return 444;
}

# 2. Fail2Ban Filter (/etc/fail2ban/filter.d/nginx-scanners.conf)
# [Definition]
# failregex = ^<HOST> - .* Host:".*"$
# ignoreregex =
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build Multi-Port Dual-Stack Default Server Harness</strong></summary>

**Task**: Author a comprehensive, production-ready `00-default-server.conf` that manages default catch-all drops on ports 80, 443, and an internal metrics port 9090 across IPv4 and IPv6.

**Solution**:
```nginx
# /etc/nginx/conf.d/00-default-server.conf

# Port 80 HTTP Catch-All
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name _;
    return 444;
}

# Port 443 HTTPS Catch-All
server {
    listen 443 ssl default_server;
    listen [::]:443 ssl default_server;
    server_name _;

    ssl_certificate /etc/nginx/ssl/catchall.crt;
    ssl_certificate_key /etc/nginx/ssl/catchall.key;

    return 444;
}

# Port 9090 Admin/Metrics Catch-All (Returns 403 Forbidden for security scanning)
server {
    listen 9090 default_server;
    listen [::]:9090 default_server;
    server_name _;
    return 403 "Forbidden: Unauthorized administrative port access\n";
}
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Directive / Technique | Syntax Example | Operational Impact |
| :--- | :--- | :--- |
| **Default Server Flag** | `listen 80 default_server;` | Captures all unmatched Host traffic on port |
| **IPv6 Default Server** | `listen [::]:80 default_server;`| Captures unmatched IPv6 traffic |
| **Placeholder Domain** | `server_name _;` | Guarantees non-matching domain |
| **Drop Connection** | `return 444;` | Closes socket without sending response |
| **Safe Host Header** | `proxy_set_header Host $host;` | Passes sanitized lowercase host |
| **Catch-All Cert** | Self-signed dummy certificate | Satisfies TLS handshake without leaking SNI |

---

## 13. Further Reading & Official Documentation

- [Official NGINX Documentation: Server Names and Default Server](https://nginx.org/en/docs/http/server_names.html)
- [Official NGINX Documentation: How NGINX Processes a Request](https://nginx.org/en/docs/http/request_processing.html)
- [PortSwigger Web Security Academy: HTTP Host Header Attacks](https://portswigger.net/web-security/host-header-attacks)
- [OWASP: Automated Threat to Web Applications (OAT-014 Reconnaissance)](https://owasp.org/www-project-automated-threats-to-web-applications/)
- [RFC 6066: Transport Layer Security (TLS) Extensions - Server Name Indication](https://www.rfc-editor.org/rfc/rfc6066.html)

---

[<- 06-command-line-and-signals.md](./06-command-line-and-signals.md) | [Back to Module Overview](./README.md) | [Next Module: 02-core-features ->](../02-core-features/README.md)
