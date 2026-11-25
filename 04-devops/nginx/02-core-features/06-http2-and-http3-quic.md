---
title: "HTTP/2 and HTTP/3 (QUIC) in NGINX"
description: "Comprehensive guide to configuring HTTP/2 and modern HTTP/3 over QUIC/UDP in NGINX, covering stream multiplexing, binary framing, Alt-Svc negotiation, and Rapid Reset mitigations."
module: "02-core-features"
track: "devops-nginx"
order: 6
difficulty: "Advanced"
prerequisites:
  - "02-core-features/03-tls-https"
tags:
  - nginx
  - http2
  - http3
  - quic
  - udp
  - alt-svc
  - rapid-reset
  - hpack
last_updated: "2026-03-30"
---

# HTTP/2 and HTTP/3 (QUIC) in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Modern Standalone HTTP/2 Configuration](#example-1-modern-standalone-http2-configuration)
  - [Example 2: Production Dual-Stack HTTP/2 and HTTP/3 (QUIC) over UDP](#example-2-production-dual-stack-http2-and-http3-quic-over-udp)
  - [Example 3: HTTP/2 DoS and Rapid Reset Attack Mitigation](#example-3-http2-dos-and-rapid-reset-attack-mitigation)
  - [Example 4: Tuning HTTP/2 & HTTP/3 Streams, Buffers, and HPACK](#example-4-tuning-http2--http3-streams-buffers-and-hpack)
  - [Example 5: GSS / Alt-Svc Header Strategy and Zero-Downtime QUIC Restart](#example-5-gss--alt-svc-header-strategy-and-zero-downtime-quic-restart)
- [6. Production Best Practices](#6-production-best-practices)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Troubleshooting](#8-performance-security--troubleshooting)
- [9. Architectural Comparison Table](#9-architectural-comparison-table)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

Web performance has undergone two generational paradigm shifts over the past decade:
1. **HTTP/2 (RFC 7540 / RFC 9113)**: Replaced plain-text HTTP/1.1 with binary framing, stream multiplexing over a single TCP connection, header compression via HPACK, and stream prioritization. It eliminated application-layer head-of-line (HOL) blocking.
2. **HTTP/3 (RFC 9114)**: Replaced TCP with **QUIC (RFC 9000)**, a modern transport protocol built on top of **UDP**. Because TCP guarantees in-order byte delivery, a single dropped TCP packet stalls all multiplexed HTTP/2 streams on that connection (transport-layer HOL blocking). HTTP/3 solves this by isolating packet loss strictly to the affected stream. Furthermore, QUIC integrates TLS 1.3 directly into its transport handshake, enabling 1-RTT (or 0-RTT) connection setup and seamless connection migration across IP changes (e.g., switching from Wi-Fi to cellular).

In NGINX:
- **Version 1.25.1+** deprecated the legacy `listen ... http2;` syntax in favor of the standalone `http2 on;` directive.
- **Mainline 1.25.0+ and Stable 1.26.0+** introduced official production support for the `ngx_http_v3_module`, allowing NGINX to serve HTTP/1.1, HTTP/2, and HTTP/3 simultaneously from the same virtual host.

---

## 2. Core Concepts

### Multiplexing vs Head-of-Line (HOL) Blocking
- **HTTP/1.1**: Requests on a single TCP connection must be processed sequentially. To load multiple assets in parallel, browsers open 6 simultaneous TCP connections per domain ("domain sharding").
- **HTTP/2 Application-Layer Multiplexing**: Interleaves independent binary frames (DATA, HEADERS) across a single TCP socket. However, if an underlying TCP packet is dropped, the OS TCP stack pauses delivery of all frames until the missing packet is retransmitted.
- **HTTP/3 QUIC Independent Streams**: Each HTTP/3 stream is an independent entity at the transport layer. A dropped UDP packet only pauses the single stream it belongs to; all other concurrent streams continue streaming uninterrupted.

### The Modern Syntax Shift: `http2 on;`
Prior to NGINX 1.25.1, HTTP/2 was enabled per listen socket:
```nginx
# DEPRECATED (Triggers warning in 1.25.1+):
listen 443 ssl http2;
```
Starting with NGINX 1.25.1, HTTP/2 can be toggled per virtual server using a dedicated directive:
```nginx
# MODERN STANDARD:
listen 443 ssl;
http2 on;
```

### HTTP/3 and the Alt-Svc (Alternative Services) Header
Web browsers always begin by connecting to web servers via standard TCP (HTTP/1.1 or HTTP/2). A browser cannot guess whether UDP port 443 is open or blocked by enterprise firewalls.
To discover HTTP/3, the server sends an `Alt-Svc` response header over the established HTTPS/TCP connection:
```http
Alt-Svc: h3=":443"; ma=86400
```
This instructs the browser: *"This origin is also available via HTTP/3 (`h3`) on UDP port 443; remember this for 86,400 seconds (24 hours)."* On subsequent visits, the browser connects directly via UDP.

### The `reuseport` Socket Flag Requirement
In NGINX, HTTP/3 requires the `reuseport` parameter on the QUIC listen directive:
```nginx
listen 443 quic reuseport;
```
`reuseport` instructs the Linux kernel to create multiple receive queues and distribute incoming UDP datagrams across all NGINX worker processes directly using kernel-level hashing. **Important:** Adding or changing `reuseport` requires a full process restart (`systemctl restart nginx`), as socket options cannot be bound dynamically via `nginx -s reload`.

---

## 3. How It Works Internally

### Protocol Stack Comparison: HTTP/1.1 vs HTTP/2 vs HTTP/3

```
+------------------+    +------------------+    +------------------+
|     HTTP/1.1     |    |      HTTP/2      |    |      HTTP/3      |
|  Plaintext ASCII |    |  Binary Framing  |    |  QPACK Framing   |
+------------------+    +------------------+    +------------------+
|     TLS 1.2/1.3  |    |     TLS 1.2/1.3  |    |      QUIC        |
+------------------+    +------------------+    | (TLS 1.3 built-in|
|       TCP        |    |       TCP        |    |  + flow control) |
+------------------+    +------------------+    +------------------+
|        IP        |    |        IP        |    |    UDP / IP      |
+------------------+    +------------------+    +------------------+
```

### The Dual-Stack TCP + UDP Negotiation Flow

```
Browser Client                                       NGINX Edge
      |                                                  |
      |=== 1. Initial Connection: TCP Handshake ========>|
      |=== 2. TLS 1.3 Handshake (ALPN: h2, http/1.1) ===>|
      |<== 3. HTTP/2 Response + Alt-Svc: h3=":443" ======|
      |                                                  |
      |   [Browser caches Alt-Svc: h3 for 24 hours]      |
      |                                                  |
      |=== 4. Subsequent Visit: QUIC Handshake (UDP 443)>|
      |       (Includes TLS 1.3 in initial QUIC packet)  |
      |<== 5. HTTP/3 0-RTT / 1-RTT Response =============|
```

1. **ALPN (Application-Layer Protocol Negotiation)**: During the initial TLS handshake on TCP, the client and server negotiate `h2` or `http/1.1` via TLS extension.
2. **Alt-Svc Advertisement**: NGINX injects the `Alt-Svc` header into the HTTP response.
3. **UDP Parallel Probe**: Modern browsers trigger a background UDP probe to port 443. If UDP packets traverse the network successfully, the browser switches the session to QUIC.

---

## 4. Syntax & Directives Reference

### HTTP/2 Directives (`ngx_http_v2_module`)
| Directive | Context | Default | Purpose |
| :--- | :--- | :--- | :--- |
| `http2 on\|off` | `http`, `server` | `off` | Enables HTTP/2 protocol support (NGINX 1.25.1+). |
| `http2_max_concurrent_streams number` | `http`, `server` | `128` | Maximum number of concurrent bidirectional streams per HTTP/2 connection. |
| `http2_recv_buffer_size size` | `http` | `256k` | Size of the per-worker input buffer for HTTP/2 processing. |
| `http2_chunk_size size` | `http`, `server`, `location` | `8k` | Maximum size of chunks into which the HTTP/2 response body is sliced. |
| `http2_max_field_size size` | `http`, `server` | `4k` | Maximum size of an individual compressed header field. |
| `http2_max_header_size size` | `http`, `server` | `16k` | Maximum total size of an entire compressed HTTP/2 header list. |

### HTTP/3 Directives (`ngx_http_v3_module`)
| Directive | Context | Default | Purpose |
| :--- | :--- | :--- | :--- |
| `listen port quic [reuseport]` | `server` | None | Binds a UDP socket for incoming QUIC datagrams. |
| `http3 on\|off` | `http`, `server` | `on` | Enables or disables HTTP/3 protocol negotiation. |
| `http3_hq on\|off` | `http`, `server` | `off` | Enables draft HTTP/0.9 "HTTP-over-QUIC" (for development only). |
| `http3_max_concurrent_streams number` | `http`, `server` | `128` | Max concurrent streams per QUIC connection. |
| `http3_stream_buffer_size size` | `http`, `server` | `64k` | Size of the stream read and write buffer. |
| `quic_retry on\|off` | `http`, `server` | `off` | Enforces QUIC Address Validation retry packet to prevent UDP spoofing attacks. |
| `quic_gso on\|off` | `http`, `server` | `on` | Uses Linux UDP Generic Segmentation Offload (GSO) for high throughput. |

---

## 5. Practical Implementation Examples

### Example 1: Modern Standalone HTTP/2 Configuration

Configures clean, compliant HTTP/2 using modern NGINX 1.25.1+ syntax with safe TLS 1.3/1.2 parameters.

```nginx
# /etc/nginx/conf.d/http2_modern.conf

server {
    listen 80;
    server_name web.example.com;
    return 301 https://$host$request_uri;
}

server {
    listen 443 ssl;
    listen [::]:443 ssl;

    # Modern HTTP/2 directive (replaces 'listen 443 ssl http2;')
    http2 on;

    server_name web.example.com;

    # SSL Certificates and Modern TLS
    ssl_certificate     /etc/ssl/certs/web.example.com.crt;
    ssl_certificate_key /etc/ssl/private/web.example.com.key;
    ssl_protocols       TLSv1.2 TLSv1.3;
    ssl_ciphers         ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256;

    # HTTP/2 Stream and Buffer Tuning
    http2_max_concurrent_streams 256;
    http2_recv_buffer_size 512k;

    location / {
        proxy_pass http://backend_pool;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-Proto https;
    }
}
```

### Example 2: Production Dual-Stack HTTP/2 and HTTP/3 (QUIC) over UDP

Simultaneously terminates HTTP/2 over TCP and HTTP/3 over UDP with `reuseport`, QUIC address validation, and `Alt-Svc` advertisement.

```nginx
# /etc/nginx/conf.d/dual_h2_h3.conf

server {
    # 1. Standard TCP Listeners for HTTP/1.1 and HTTP/2
    listen 443 ssl;
    listen [::]:443 ssl;
    http2 on;

    # 2. UDP Listeners for HTTP/3 QUIC
    # Note: 'reuseport' must be declared on at least one quic listen directive
    listen 443 quic reuseport;
    listen [::]:443 quic reuseport;
    http3 on;

    server_name fast.example.com;

    # TLS 1.3 is MANDATORY for HTTP/3 (QUIC will not work with TLS 1.2)
    ssl_certificate     /etc/ssl/certs/fast.example.com.crt;
    ssl_certificate_key /etc/ssl/private/fast.example.com.key;
    ssl_protocols       TLSv1.2 TLSv1.3;

    # Advertise HTTP/3 to HTTP/2 and HTTP/1.1 clients
    add_header Alt-Svc 'h3=":443"; ma=86400' always;

    # Enable QUIC address validation to mitigate UDP amplification attacks
    quic_retry on;
    quic_gso on;

    location / {
        proxy_pass http://backend_pool;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-Proto $scheme;

        # Header indicating to client which protocol version was negotiated
        add_header X-Served-By-Protocol $server_protocol always;
    }
}
```

### Example 3: HTTP/2 DoS and Rapid Reset Attack Mitigation

Mitigating the HTTP/2 "Rapid Reset" vulnerability (CVE-2023-44487), where malicious clients flood streams and immediately cancel them with `RST_STREAM` frames.

```nginx
# /etc/nginx/conf.d/h2_security.conf

server {
    listen 443 ssl;
    http2 on;
    server_name secure-api.example.com;

    ssl_certificate     /etc/ssl/certs/api.crt;
    ssl_certificate_key /etc/ssl/private/api.key;

    # Limit concurrent streams per connection to stop frame exhaustion
    http2_max_concurrent_streams 64;

    # Enforce maximum number of requests over a single persistent connection
    keepalive_requests 1000;

    # Keepalive timeout to reclaim abandoned TCP connections
    keepalive_timeout 60s;

    # Limit header field sizes to prevent HPACK bomb memory exhaustion
    http2_max_field_size 4k;
    http2_max_header_size 16k;

    location / {
        proxy_pass http://internal_api;
    }
}
```

### Example 4: Tuning HTTP/2 & HTTP/3 Streams, Buffers, and HPACK

Fine-tuning chunk delivery sizes to optimize Time to First Byte (TTFB) and rendering latency for Single Page Applications.

```nginx
# /etc/nginx/conf.d/stream_tuning.conf

http {
    # Response chunk slicing:
    # 8k is optimal for balancing TCP MSS packet packing and latency
    http2_chunk_size 8k;

    # QUIC stream memory tuning
    http3_stream_buffer_size 128k;
    http3_max_concurrent_streams 256;

    server {
        listen 443 ssl;
        http2 on;
        listen 443 quic;
        http3 on;
        server_name assets.example.com;

        ssl_certificate     /etc/ssl/certs/assets.crt;
        ssl_certificate_key /etc/ssl/private/assets.key;

        root /var/www/assets;

        location ~* \.(js|css|png|jpg|woff2)$ {
            expires 1y;
            add_header Cache-Control "public, immutable";
            add_header Alt-Svc 'h3=":443"; ma=86400' always;
            sendfile on;
            tcp_nopush on;
        }
    }
}
```

### Example 5: Alt-Svc Header Strategy and Zero-Downtime QUIC Restart

Handling edge cases where UDP traffic may be dropped, and demonstrating safe maintenance procedures when changing `reuseport`.

```nginx
# /etc/nginx/conf.d/alt_svc_canary.conf

# Canary Alt-Svc: Only advertise HTTP/3 to 20% of traffic
map $remote_addr $h3_advertising {
    default "";
    "~^[0-4]" 'h3=":443"; ma=3600'; # Only IPs starting with 0-4 get Alt-Svc
}

server {
    listen 443 ssl;
    http2 on;
    listen 443 quic reuseport;
    http3 on;
    server_name canary.example.com;

    ssl_certificate     /etc/ssl/certs/canary.crt;
    ssl_certificate_key /etc/ssl/private/canary.key;

    location / {
        if ($h3_advertising != "") {
            add_header Alt-Svc $h3_advertising always;
        }
        proxy_pass http://backend_pool;
    }
}
```

#### Proper Zero-Downtime Restart with `reuseport`:
```bash
# IMPORTANT: 'systemctl reload nginx' DOES NOT rebind reuseport sockets!
# To reload when changing reuseport without dropping traffic:
# 1. Send USR2 signal to master to spawn new binary and new listen sockets:
kill -USR2 $(cat /var/run/nginx.pid)

# 2. Old master moves its PID to .oldbin and spawns new master.
# 3. Gracefully stop old workers once new workers are verified:
kill -WINCH $(cat /var/run/nginx.pid.oldbin)
kill -QUIT  $(cat /var/run/nginx.pid.oldbin)
```

---

## 6. Production Best Practices

1. **Always Use the Standalone `http2 on;` Directive**: Avoid deprecated `listen 443 ssl http2;` in modern NGINX (1.25.1+).
2. **Mandate TLS 1.3 for HTTP/3**: The QUIC standard strictly requires TLS 1.3. While NGINX can support TLS 1.2 for fallback HTTP/2 connections, `ssl_protocols` must include `TLSv1.3`.
3. **Always Advertise Alt-Svc with `always`**: Web browsers cannot initiate QUIC without discovering it first. Always set `add_header Alt-Svc 'h3=":443"; ma=86400' always;` on the HTTPS server block.
4. **Open UDP Port 443 in Firewalls**: HTTP/3 operates entirely over UDP. Security groups (AWS Security Groups, Azure NSG, `ufw`, `iptables`) must explicitly allow inbound **UDP port 443**.
5. **Enable `quic_retry on;` on Public Networks**: UDP is connectionless and prone to spoofed IP reflection attacks. Enabling `quic_retry` forces the client to validate its source IP via a token handshake before NGINX allocates connection memory.
6. **Limit `http2_max_concurrent_streams`**: Set this to `128` or `256` (never leave it excessively high) to prevent memory exhaustion from client-side multiplexing floods.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Using `listen 443 ssl http2;` on NGINX 1.25.1+** | Generates `[warn] the "http2" parameter of the "listen" directive is deprecated` in error logs on reload. | Use `listen 443 ssl;` and add `http2 on;` inside the `server {}` block. |
| **Forgetting UDP firewall rules for HTTP/3** | NGINX listens on UDP 443, but cloud security groups drop UDP packets; clients hang or fail back to TCP after timeout. | Open UDP port 443 in `ufw`, `iptables`, and cloud security groups alongside TCP 443. |
| **Omitting `reuseport` on QUIC listen** | Linux kernel assigns all incoming UDP datagrams to a single worker process, creating massive CPU imbalance and dropped UDP packets. | Always specify `reuseport` on the primary QUIC listen directive: `listen 443 quic reuseport;`. |
| **`nginx -s reload` after modifying `reuseport`** | Sockets with `SO_REUSEPORT` cannot be rebound dynamically on graceful reload; NGINX logs an error or ignores the flag. | Perform a full restart (`systemctl restart nginx`) or binary upgrade (`kill -USR2`). |
| **`ssl_protocols TLSv1.2;` with HTTP/3** | QUIC specification explicitly forbids TLS 1.2 and earlier. QUIC handshakes fail immediately. | Set `ssl_protocols TLSv1.2 TLSv1.3;`. |
| **Omitting `always` on the `Alt-Svc` header** | Without `always`, NGINX only includes the header on HTTP 2xx/3xx responses, omitting it on 4xx/5xx responses. | Always declare: `add_header Alt-Svc 'h3=":443"; ma=86400' always;`. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test HTTP/2 Negotiation via curl
curl -I --http2 -s https://web.example.com | grep HTTP
# Expected: HTTP/2 200

# 2. Test HTTP/3 (QUIC) Negotiation via curl (Requires curl built with ngtcp2 or quiche)
curl -I --http3 https://fast.example.com
# Expected: HTTP/3 200

# 3. Verify Alt-Svc Header Advertisement over standard HTTP/2
curl -I https://fast.example.com | grep -i alt-svc
# Expected: alt-svc: h3=":443"; ma=86400

# 4. Verify Listening Sockets for TCP and UDP Port 443
ss -tulpn | grep 443
# Must show both:
# tcp  LISTEN  0  511  *:443  *:*  users:(("nginx",...))
# udp  UNCONN  0  0    *:443  *:*  users:(("nginx",...))

# 5. Check for QUIC and HTTP/2 Disconnects in Error Logs
tail -f /var/log/nginx/error.log | grep -E "quic|http2"
```

### Kernel Sysctl Tuning for High-Throughput UDP QUIC
Linux defaults for UDP socket buffers are modest and can drop bursts of QUIC packets under load. Apply these sysctl settings:
```ini
# /etc/sysctl.d/99-quic-tuning.conf
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216
net.core.rmem_default = 1048576
net.core.wmem_default = 1048576
```

---

## 9. Architectural Comparison Table

| Metric / Dimension | HTTP/1.1 | HTTP/2 | HTTP/3 (QUIC) |
| :--- | :--- | :--- | :--- |
| **Transport Layer** | TCP | TCP | UDP |
| **Security Layer** | Optional TLS 1.2/1.3 | Mandates TLS (in practice) | Built-in TLS 1.3 (Mandatory) |
| **Multiplexing** | No (Sequential per socket) | Yes (Binary frames) | Yes (Independent streams) |
| **Transport Head-of-Line Blocking** | N/A | **Yes** (TCP packet drop stalls all streams) | **No** (Packet drop isolated to single stream) |
| **Connection Handshake** | 1-RTT TCP + 2-RTT TLS | 1-RTT TCP + 1-2 RTT TLS | 1-RTT Combined (0-RTT resumption) |
| **Connection Migration** | No (Tied to IP:Port 4-tuple) | No (Tied to IP:Port 4-tuple) | **Yes** (64-bit Connection ID survives IP changes) |
| **Header Compression** | None (Plain ASCII) | HPACK (Static + Dynamic table) | QPACK (Out-of-order stream compression) |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: Why did NGINX deprecate the listen ... http2 syntax in version 1.25.1, and what replaced it?</strong></summary>

**Answer:**
Historically, HTTP/2 was configured as a parameter of the `listen` directive (e.g., `listen 443 ssl http2;`). Because `listen` parameters bind directly to the underlying OS socket (IP and Port), enabling `http2` on one virtual host automatically enabled it for *all* virtual hosts sharing that same IP and port combination, creating configuration ambiguities and unexpected inheritance behaviors.

In NGINX 1.25.1, `listen ... http2` was deprecated in favor of the standalone `http2 on;` directive. This allows HTTP/2 to be cleanly toggled at the `http {}` or `server {}` level, matching the architectural design of `http3 on;` and standardizing virtual host protocol controls.
</details>

<details>
<summary><strong>Q2: Explain how HTTP/3 eliminates the transport-layer Head-of-Line (HOL) blocking problem present in HTTP/2.</strong></summary>

**Answer:**
In HTTP/2, all multiplexed application requests and responses are interleaved across a single underlying TCP connection. However, the operating system's TCP stack operates on an ordered byte-stream model: TCP knows nothing about HTTP/2 frames and guarantees that application data must be delivered strictly in sequential byte order. If a single TCP packet is dropped or corrupted in transit, the OS TCP stack pauses delivery of all subsequent packets to the application until the missing segment is retransmitted and acknowledged. Consequently, every concurrent HTTP/2 stream is blocked.

HTTP/3 replaces TCP with QUIC over UDP. In QUIC, the transport layer itself understands the concept of distinct streams. Each stream maintains its own independent sequence offsets. If a UDP packet belonging to Stream 3 is lost, only Stream 3 is paused pending retransmission; Streams 1, 2, 4, and 5 continue streaming uninterrupted to the client application.
</details>

<details>
<summary><strong>Q3: How do web browsers discover and negotiate HTTP/3 if they initially connect via TCP?</strong></summary>

**Answer:**
Because UDP traffic is frequently blocked or rate-limited by middleboxes, enterprise firewalls, and public Wi-Fi networks, browsers cannot assume HTTP/3 is accessible.

Discovery follows the **Alternative Services (Alt-Svc)** mechanism (RFC 7838):
1. The browser connects via standard TCP (HTTP/1.1 or HTTP/2 over TLS).
2. NGINX returns the HTTP response with the `Alt-Svc` header:
   `Alt-Svc: h3=":443"; ma=86400`
3. The browser caches this association (`h3` available on port 443 for 86,400 seconds) and initiates a non-blocking background UDP probe.
4. On subsequent HTTP requests to that origin, the browser immediately attempts a QUIC connection over UDP port 443 while retaining a fallback to TCP if UDP is blocked.
</details>

<details>
<summary><strong>Q4: Why is the reuseport parameter required on the listen directive for HTTP/3 in NGINX?</strong></summary>

**Answer:**
Unlike TCP, where the OS kernel handles the three-way handshake and assigns accepted connections to worker processes via `accept4()`, UDP is connectionless. Without `reuseport`, all incoming UDP packets on port 443 would be queued to the socket buffer of a single NGINX worker process, causing extreme CPU core contention and high packet drop rates under load.

With `listen 443 quic reuseport;`, the Linux kernel creates multiple internal socket receive queues (one per worker process). The kernel computes a hash of the incoming packet's source IP and port (or QUIC Connection ID) and load-balances incoming UDP datagrams evenly across all worker processes directly in kernel space.
</details>

<details>
<summary><strong>Q5: What was the HTTP/2 "Rapid Reset" vulnerability (CVE-2023-44487), and how do you mitigate it in NGINX?</strong></summary>

**Answer:**
The Rapid Reset attack exploits the HTTP/2 stream multiplexing mechanism. An attacker opens hundreds of concurrent HTTP/2 streams by sending `HEADERS` frames and immediately issues a `RST_STREAM` cancellation frame for each stream. Because resetting a stream is a normal HTTP/2 control frame, the server immediately cancels the request, freeing the stream slot without generating significant network traffic for the attacker. 

However, parsing the request, initiating upstream lookups, and tearing down the stream state consumes significant CPU memory and processing time on the server. By looping this at high frequency, attackers generated massive Denial of Service conditions.

Mitigations in NGINX:
1. Update to patched NGINX releases (which enforce rate-limiting on resets).
2. Set `http2_max_concurrent_streams 64;` or `128;` to limit concurrent in-flight streams.
3. Configure `keepalive_requests 1000;` to force connection termination after a reasonable request volume.
4. Apply rate limiting via `limit_req_zone`.
</details>

<details>
<summary><strong>Q6: Why is TLS 1.3 mandatory for HTTP/3, while HTTP/2 can run over TLS 1.2?</strong></summary>

**Answer:**
In HTTP/2, TLS is a distinct cryptographic layer that sits directly on top of the TCP byte stream. As long as the negotiated TLS version supports ALPN (`h2`), HTTP/2 can function over both TLS 1.2 and TLS 1.3.

In HTTP/3, the transport protocol is **QUIC**, which does not run on top of TLS. Instead, QUIC **integrates TLS 1.3 directly into its own packet headers and framing**. QUIC uses TLS 1.3 handshake messages (like `ClientHello` and `ServerHello`) to establish cryptographic keys, but replaces TLS record layer framing with QUIC packet protection. Because the QUIC specification (RFC 9001) was architected exclusively around the modern key schedule, 1-RTT handshake, and message formats of TLS 1.3, TLS 1.2 cannot be used with QUIC.
</details>

<details>
<summary><strong>Q7: What is QUIC Connection Migration, and how does it benefit mobile clients?</strong></summary>

**Answer:**
In standard TCP (HTTP/1.1 and HTTP/2), a connection is uniquely identified by the 4-tuple: `(Source IP, Source Port, Destination IP, Destination Port)`. If a mobile user walks out of their house and transitions from Wi-Fi to a 5G cellular network, their client IP address changes. This breaks the TCP 4-tuple, causing the connection to terminate and forcing the client to perform a full TCP + TLS handshake from scratch.

QUIC solves this by introducing a **64-bit Connection ID (CID)** that is independent of network socket addresses. When the mobile client switches from Wi-Fi to cellular, it continues sending UDP packets with the same Connection ID from its new cellular IP. NGINX inspects the CID, matches it to the existing session state, and resumes data transfer seamlessly with zero reconnection downtime.
</details>

<details>
<summary><strong>Q8: What is the difference between HPACK in HTTP/2 and QPACK in HTTP/3?</strong></summary>

**Answer:**
- **HPACK (RFC 7541)**: Compresses HTTP headers in HTTP/2 using a static table and a dynamically updated shared state table. Because TCP guarantees strictly ordered delivery, HPACK assumes header blocks arrive in exact sequence; updates to the dynamic table can be referenced immediately by subsequent streams.
- **QPACK (RFC 9204)**: In HTTP/3, streams arrive out-of-order over UDP. If Stream 4 references a dynamic header table entry emitted by Stream 2, and Stream 2 is delayed due to packet loss, Stream 4 would be blocked waiting for Stream 2 (reintroducing HOL blocking). QPACK redesigns header compression by creating dedicated unidirectional encoder and decoder control streams, allowing out-of-order header decompression and avoiding cross-stream dependencies.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Convert Legacy listen ... http2 to Modern Standalone Syntax</strong></summary>

**Goal:** Identify and refactor a deprecated HTTP/2 configuration to the modern NGINX 1.25.1+ standard.

**Solution:**
```nginx
# BEFORE (Deprecated):
server {
    listen 443 ssl http2;
    server_name example.com;
    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
}

# AFTER (Modern):
server {
    listen 443 ssl;
    http2 on;
    server_name example.com;
    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
}
```
Validate syntax with `nginx -t`.
</details>

<details>
<summary><strong>Exercise 2: Configure and Verify the Alt-Svc Header</strong></summary>

**Goal:** Configure an HTTPS server to broadcast HTTP/3 availability to connecting clients for a period of 48 hours.

**Solution:**
```nginx
server {
    listen 443 ssl;
    http2 on;
    server_name portal.local;

    ssl_certificate     /etc/ssl/certs/portal.crt;
    ssl_certificate_key /etc/ssl/private/portal.key;

    # 48 hours = 172800 seconds
    add_header Alt-Svc 'h3=":443"; ma=172800' always;

    location / {
        return 200 "Alt-Svc broadcast verified\n";
    }
}
```
Test with `curl -I https://portal.local | grep -i alt-svc`.
</details>

<details>
<summary><strong>Exercise 3: Configure Dual TCP and UDP Listeners for QUIC</strong></summary>

**Goal:** Write a complete server configuration binding both TCP 443 (HTTP/2) and UDP 443 (HTTP/3) with `quic_retry` enabled.

**Solution:**
```nginx
server {
    listen 443 ssl;
    http2 on;

    listen 443 quic reuseport;
    http3 on;

    server_name quic.local;

    ssl_certificate     /etc/ssl/certs/quic.crt;
    ssl_certificate_key /etc/ssl/private/quic.key;
    ssl_protocols       TLSv1.3; # Enforce TLS 1.3

    quic_retry on;
    add_header Alt-Svc 'h3=":443"; ma=86400' always;

    location / {
        return 200 "Protocol: $server_protocol\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 4: Verify UDP Listening Sockets with ss and netstat</strong></summary>

**Goal:** Verify from the Linux command line that NGINX has successfully opened UDP sockets on port 443 with SO_REUSEPORT.

**Solution:**
```bash
# Check UDP listeners
ss -u -l -n -p | grep 443
# Expect output showing 'nginx' listening on 0.0.0.0:443
```
</details>

<details>
<summary><strong>Exercise 5: Restrict HTTP/2 Concurrent Streams to Mitigate Flooding</strong></summary>

**Goal:** Harden an NGINX API endpoint against rapid multiplexing attacks by capping concurrent streams and restricting field sizes.

**Solution:**
```nginx
server {
    listen 443 ssl;
    http2 on;
    server_name hardened-api.local;

    ssl_certificate     /etc/ssl/certs/api.crt;
    ssl_certificate_key /etc/ssl/private/api.key;

    # Security caps
    http2_max_concurrent_streams 32;
    http2_max_field_size 4k;
    http2_max_header_size 16k;
    keepalive_requests 500;

    location / {
        proxy_pass http://127.0.0.1:4000;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6: Perform a Zero-Downtime Binary Upgrade on a QUIC Socket</strong></summary>

**Goal:** Safely update an NGINX configuration that modified `reuseport` without dropping client connections using Linux signals.

**Solution:**
```bash
# 1. Verify syntax
nginx -t

# 2. Instruct running master process to execute new binary / reload listening sockets
kill -USR2 $(cat /run/nginx.pid)

# 3. Verify that the new master and new worker processes are active
ps aux | grep nginx

# 4. Gracefully terminate old worker processes
kill -WINCH $(cat /run/nginx.pid.oldbin)
kill -QUIT  $(cat /run/nginx.pid.oldbin)
```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Gold Standard Dual-Stack HTTP/2 & HTTP/3 Server Template
server {
    # TCP Listener (HTTP/1.1 and HTTP/2)
    listen 443 ssl;
    listen [::]:443 ssl;
    http2 on;

    # UDP Listener (HTTP/3 QUIC)
    listen 443 quic reuseport;
    listen [::]:443 quic reuseport;
    http3 on;

    server_name example.com;

    # Certificates & TLS 1.3 Enforcement
    ssl_certificate         /etc/ssl/certs/example.com.crt;
    ssl_certificate_key     /etc/ssl/private/example.com.key;
    ssl_protocols           TLSv1.2 TLSv1.3; # TLS 1.3 required for QUIC

    # HTTP/3 Advertisement
    add_header Alt-Svc 'h3=":443"; ma=86400' always;

    # QUIC Security & Performance Offload
    quic_retry on;
    quic_gso on;

    # Stream Limits
    http2_max_concurrent_streams 128;
    http3_max_concurrent_streams 128;

    location / {
        proxy_pass http://upstream_cluster;
        proxy_set_header Host $host;
    }
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_v2_module Documentation](https://nginx.org/en/docs/http/ngx_http_v2_module.html)
- [Official NGINX ngx_http_v3_module (QUIC/HTTP/3) Documentation](https://nginx.org/en/docs/http/ngx_http_v3_module.html)
- [RFC 9113: HTTP/2 Specification](https://datatracker.ietf.org/doc/html/rfc9113)
- [RFC 9000: QUIC: A UDP-Based Multiplexed and Secure Transport](https://datatracker.ietf.org/doc/html/rfc9000)
- [RFC 9114: HTTP/3 Specification](https://datatracker.ietf.org/doc/html/rfc9114)

---

[← Previous: WebSockets and SSE Proxying](05-websockets-and-sse-proxying.md) | [Module Overview](README.md) | [Next: Compression (Gzip and Brotli) →](07-compression-gzip-and-brotli.md)
