---
title: "Connection Limiting and Concurrency Control in NGINX"
description: "Mastering concurrent connection limiting with limit_conn, bandwidth throttling with limit_rate, and mitigating Slowloris and slow HTTP attacks in NGINX."
module: "03-traffic-management"
track: "devops-nginx"
order: 7
difficulty: "Intermediate to Advanced"
prerequisites:
  - "01-fundamentals/03-server-and-location"
  - "03-traffic-management/03-rate-limiting"
tags:
  - nginx
  - connection-limiting
  - limit-conn
  - bandwidth-throttling
  - slowloris
  - ddos-mitigation
  - concurrency
last_updated: "2026-03-30"
---

# Connection Limiting and Concurrency Control in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Per-IP and Per-Server Concurrent Connection Limiting](#example-1-per-ip-and-per-server-concurrent-connection-limiting)
  - [Example 2: Bandwidth Throttling with Initial Burst Allowance](#example-2-bandwidth-throttling-with-initial-burst-allowance)
  - [Example 3: Comprehensive Slowloris & Slow HTTP DoS Mitigation](#example-3-comprehensive-slowloris--slow-http-dos-mitigation)
  - [Example 4: Tiered Download Speeds Based on User Role / Token](#example-4-tiered-download-speeds-based-on-user-role--token)
  - [Example 5: Combining `limit_conn`, `limit_req`, and `limit_rate`](#example-5-combining-limit_conn-limit_req-and-limit_rate)
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

While request rate limiting (`limit_req`) governs the **frequency** of incoming HTTP requests over time (e.g., 10 requests per second), it does not protect against attacks that consume system resources by holding connections open indefinitely. 

A single attacker can open 10,000 simultaneous TCP sockets, send one HTTP byte every 30 seconds, and completely exhaust the server's file descriptors, worker connections, and upstream sockets without ever violating a 10 req/sec rate limit. This class of attack is known as **Slowloris** or **Slow POST DoS**.

Furthermore, users downloading massive multi-gigabyte video or binary files can saturate edge network uplinks, starving other users of bandwidth.

NGINX provides two dedicated concurrency and bandwidth control modules:
1. **`ngx_http_limit_conn_module`**: Tracks and limits the number of **active concurrent connections** per client IP, per session, or per virtual host.
2. **`ngx_http_core_module` Bandwidth Controls**: Caps downstream data transmission rates (`limit_rate`) and provides initial unthrottled bursts (`limit_rate_after`).

---

## 2. Core Concepts

### Connection Limiting vs Request Rate Limiting
- **`limit_req` (Rate Limiting)**: Measures *velocity* (requests per second). A request that arrives, executes in 5ms, and finishes frees its quota immediately.
- **`limit_conn` (Connection Limiting)**: Measures *state* (simultaneous open connections). A connection only counts against the limit while it is actively being processed by NGINX (reading headers, waiting for upstream backend, or streaming response bytes to the client). Once the socket is closed or enters idle keepalive, it no longer counts against `limit_conn`.

### What Counts as an Active Connection?
In NGINX, a connection is counted by `limit_conn` **only if the request has already been assigned to a server and its headers have been read**. Idle keepalive connections do not count against `limit_conn`.

### Bandwidth Throttling Mechanics (`limit_rate` and `limit_rate_after`)
- `limit_rate speed`: Limits response transfer speed to a client socket in bytes per second (e.g., `500k` = 500 KB/s). The limit is enforced per individual connection, not globally.
- `limit_rate_after size`: Disables throttling for the first `size` bytes of the response (e.g., `10m` = 10 MB). This allows web browsers or media players to buffer the initial seconds of a video instantly, while throttling sustained playback to conserve bandwidth.

### The Slowloris Attack Surface
Slowloris exploits the fact that web servers must allocate memory and an active socket while waiting for a client to finish sending HTTP headers or body bytes. An attacker sends partial headers (e.g., `X-Header: a\r\n`) at 15-second intervals. Mitigating this requires tuning:
- `client_header_timeout`: Time allowed between consecutive header read operations.
- `client_body_timeout`: Time allowed between consecutive body read operations.
- `send_timeout`: Time allowed between consecutive client write acknowledgments.

---

## 3. How It Works Internally

### Connection Tracking via Shared Memory Red-Black Tree

```
Incoming Client Request: TCP Connect from IP 198.51.100.25
                         |
                         v
                Parse Request Headers
                         |
                         v
   Lookup $binary_remote_addr in limit_conn_zone
          /                             \
      FOUND                          NOT FOUND
        /                                 \
  Read active connections counter      Create new Node in rbtree
        |                              Set connections counter = 1
        v                                 |
  Is (counter + 1) > limit?               |
      /                   \               |
    YES                    NO             |
     |                      |             |
OVERFLOW DETECTED       Increment counter v
     |                  Proceed with Request Processing
Return HTTP 429 / 503       |
(Close socket)              +------> Stream response / wait for upstream
                                     |
                                Response Complete / Socket Closed
                                     |
                                Decrement counter in rbtree!
```

---

## 4. Syntax & Directives Reference

### Connection Limiting (`ngx_http_limit_conn_module`)
| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `limit_conn_zone key zone=name:size` | `http` | None | Declares shared memory zone and key for tracking concurrent connections. |
| `limit_conn zone number` | `http`, `server`, `location` | None | Caps simultaneous active connections allowed for a given key. |
| `limit_conn_status code` | `http`, `server`, `location` | `503` | Status code returned on connection limit breach. Recommended: `429`. |
| `limit_conn_log_level info\|notice\|warn\|error` | `http`, `server`, `location` | `error` | Severity level for logging connection limit violations. |
| `limit_conn_dry_run on\|off` | `http`, `server`, `location` | `off` | Tests limits and logs violations without rejecting connections. |

### Bandwidth & Concurrency Timeouts (`ngx_http_core_module`)
| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `limit_rate rate` | `http`, `server`, `location`, `if in location` | `0` (Uncapped)| Caps transmission speed per connection (e.g., `500k`, `1m`). |
| `limit_rate_after size` | `http`, `server`, `location`, `if in location` | `0` | Byte threshold before `limit_rate` throttling is applied. |
| `client_header_timeout time` | `http`, `server` | `60s` | Max time allowed between client header read operations. |
| `client_body_timeout time` | `http`, `server`, `location` | `60s` | Max time allowed between client body read operations. |
| `send_timeout time` | `http`, `server`, `location` | `60s` | Max time allowed between consecutive write operations to client. |

---

## 5. Practical Implementation Examples

### Example 1: Per-IP and Per-Server Concurrent Connection Limiting

Caps each individual client IP to 10 concurrent active connections, while capping the entire virtual host to 1,000 simultaneous connections.

```nginx
# /etc/nginx/conf.d/conn_limits.conf

# 1. Zone tracking connections per individual client IP
limit_conn_zone $binary_remote_addr zone=per_ip_conn:10m;

# 2. Zone tracking connections across the entire virtual server
limit_conn_zone $server_name zone=per_server_conn:10m;

server {
    listen 80;
    server_name api.example.com;

    # Return standard HTTP 429 instead of default 503
    limit_conn_status 429;
    limit_conn_log_level warn;

    location / {
        # Enforce maximum 10 active connections per client IP
        limit_conn per_ip_conn 10;

        # Enforce maximum 1000 total active connections across this virtual host
        limit_conn per_server_conn 1000;

        proxy_pass http://backend_cluster;
        proxy_set_header Host $host;
    }
}
```

### Example 2: Bandwidth Throttling with Initial Burst Allowance

Optimizing video and large file distribution: clients can download the first 10MB at full network speed to buffer video, after which speed is throttled to 500 KB/s.

```nginx
# /etc/nginx/conf.d/video_streaming.conf

server {
    listen 80;
    server_name media.example.com;
    root /var/www/media;

    # Limit each individual download connection
    location /downloads/ {
        # Allow first 10MB unthrottled
        limit_rate_after 10m;

        # Throttle subsequent transfer to 500 KB/s
        limit_rate 500k;

        # Prevent users from opening 20 download threads to bypass limit
        limit_conn per_ip_conn 2;

        sendfile on;
        tcp_nopush on;
    }
}
```

### Example 3: Comprehensive Slowloris & Slow HTTP DoS Mitigation

Hardening NGINX against slow header and body starvation attacks.

```nginx
# /etc/nginx/conf.d/slowloris_hardening.conf

http {
    # 1. Restrict maximum allowable time between read operations
    # Default is 60s; reduce to 10s-15s to quickly drop stalled sockets
    client_header_timeout 10s;
    client_body_timeout   10s;
    send_timeout          10s;

    # 2. Enforce minimum request header size and count
    client_header_buffer_size 1k;
    large_client_header_buffers 4 8k;

    # 3. Limit maximum allowable request body size (blocks oversized POST floods)
    client_max_body_size 10m;

    # 4. Limit concurrent connections per IP
    limit_conn_zone $binary_remote_addr zone=anti_slowloris:10m;

    server {
        listen 80 default_server;
        server_name _;

        # Drop connections exceeding 20 concurrent sockets from a single IP
        limit_conn anti_slowloris 20;
        limit_conn_status 429;

        # Reset timed out connections instantly via TCP RST (frees socket RAM immediately)
        reset_timedout_connection on;

        location / {
            proxy_pass http://backend_pool;
            proxy_set_header Host $host;
        }
    }
}
```

### Example 4: Tiered Download Speeds Based on User Role / Token

Dynamically throttle anonymous users to 200 KB/s, while granting registered premium users 5 MB/s speeds via `map`.

```nginx
# /etc/nginx/conf.d/tiered_bandwidth.conf

map $http_x_user_tier $user_bandwidth {
    default     200k;   # Anonymous: 200 KB/s
    "standard"  1m;     # Free Registered: 1 MB/s
    "premium"   5m;     # Paid Subscriber: 5 MB/s
    "unlimited" 0;      # Enterprise: Uncapped (0 disables throttling)
}

server {
    listen 80;
    server_name files.example.com;
    root /var/www/files;

    location / {
        # Dynamically assign bandwidth limit from variable
        limit_rate $user_bandwidth;

        sendfile on;
        tcp_nopush on;
        add_header X-Applied-Speed-Limit $user_bandwidth always;
    }
}
```

### Example 5: Combining `limit_conn`, `limit_req`, and `limit_rate`

The ultimate three-layered traffic governance architecture for production API gateways.

```nginx
# /etc/nginx/conf.d/three_layered_defense.conf

# Layer 1: Request Rate Limiter (Velocity: 10 req/sec)
limit_req_zone $binary_remote_addr zone=req_layer:10m rate=10r/s;

# Layer 2: Concurrency Limiter (Concurrency: 5 active sockets)
limit_conn_zone $binary_remote_addr zone=conn_layer:10m;

server {
    listen 443 ssl;
    server_name secure-api.example.com;

    ssl_certificate     /etc/ssl/certs/api.crt;
    ssl_certificate_key /etc/ssl/private/api.key;

    limit_req_status 429;
    limit_conn_status 429;

    location /api/reports/ {
        # 1. Enforce Velocity: 10 r/s with a burst of 5
        limit_req zone=req_layer burst=5 nodelay;

        # 2. Enforce Concurrency: Max 5 concurrent sockets per IP
        limit_conn conn_layer 5;

        # 3. Enforce Bandwidth: Max 1 MB/s download speed per socket
        limit_rate 1m;

        proxy_pass http://reporting_backend;
        proxy_set_header Host $host;
    }
}
```

---

## 6. Production Best Practices

1. **Always Use `$binary_remote_addr` for Connection Zones**: Never use `$remote_addr`. The 4-byte binary IP saves memory and prevents cache zone exhaustion.
2. **Combine `limit_conn` with `limit_rate` on File Downloads**: Limiting download speeds without limiting connections is useless: a user can simply open 20 download connections in parallel using a download accelerator (e.g., IDM, aria2) to bypass the limit. Always cap concurrent connections (`limit_conn per_ip 2;`).
3. **Always Set `reset_timedout_connection on;`**: When a client connection times out (e.g., during a Slowloris attack or when a mobile client drops off the network), standard TCP close procedures leave the socket in `FIN_WAIT` or `TIME_WAIT` states in kernel memory. Enabling `reset_timedout_connection on;` instructs NGINX to send a TCP RST packet and terminate the socket instantly, reclaiming socket memory immediately.
4. **Tune Header and Body Timeouts Down from Default 60s**: The default `client_header_timeout 60s;` and `client_body_timeout 60s;` are excessively generous. In modern networks, genuine clients send HTTP headers in less than 50ms. Reducing timeouts to `10s` or `15s` mitigates Slowloris attacks without impacting real users.
5. **Always Set `limit_conn_status 429;`**: Prevent false infrastructure crash alerts by returning standard `429 Too Many Requests` rather than default `503`.
6. **Exempt Health Check Probes**: Never rate-limit or connection-limit cloud load balancer health checker IP addresses (`/healthz`).

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Limiting `limit_rate` without limiting `limit_conn`** | Users open multi-threaded download managers (aria2), opening 30 simultaneous sockets and bypassing the speed cap. | Always pair `limit_rate` with `limit_conn per_ip 2;`. |
| **Leaving timeouts at default 60s** | Slowloris tools (e.g., slowhttptest) open 2,000 sockets and hold them open for 60 seconds each, easily exhausting worker connection limits. | Reduce `client_header_timeout` and `client_body_timeout` to `10s`. |
| **Using `limit_conn` behind Cloudflare / CDN without Real-IP** | All client connections share the CDN's edge IP; `limit_conn per_ip 10;` blocks the entire global user base once 10 connections exist. | Restore real client IPs with `ngx_http_realip_module` or key off headers. |
| **Confusing connection limits with request limits** | Expecting `limit_conn 10` to stop a client from making 100 requests per second. (If requests take 2ms, a single connection can make 500 req/sec!). | Use `limit_req` for request frequency and `limit_conn` for socket concurrency. |
| **Setting `client_max_body_size` too high** | Attackers upload 2GB payloads, filling NGINX client body disk buffers and exhausting disk space. | Set `client_max_body_size` to a strict reasonable limit (e.g., `10m` or `50m`). |
| **Setting `limit_conn` too low on WebSockets** | WebSockets are long-lived persistent connections. A limit of `limit_conn 2` prevents a user from opening multiple browser tabs. | Tune WebSocket concurrency limits higher (e.g., `10` to `20` per IP). |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test Concurrent Connection Limit via curl & background jobs
# Attempt to open 15 simultaneous connections when limit is 10:
for i in {1..15}; do
  curl -s -o /dev/null -w "%{http_code}\n" http://api.example.com/slow-endpoint &
done; wait

# Expected output: Ten 200s, and Five 429s

# 2. Test Bandwidth Throttling with curl (Measure transfer rate)
curl -s -o /dev/null --limit-rate 0 http://media.example.com/downloads/bigfile.zip \
  -w 'Average Speed: %{speed_download} bytes/sec\n'
# Observe average speed caps near configured limit_rate (e.g., 512,000 B/s)

# 3. Simulate Slowloris Attack using slowhttptest
# (Install: apt-get install slowhttptest)
slowhttptest -c 500 -H -g -o /tmp/slowloris_report -i 10 -r 200 -t GET -u http://example.com/
# Verify NGINX safely drops slow connections and remains responsive

# 4. Inspect Connection Rejection Logs
tail -f /var/log/nginx/error.log | grep "limiting connections"
# Sample: 2026/03/30 [warn] 1234#1234: *567 limiting connections by zone "per_ip_conn", client: 192.168.1.50
```

---

## 9. Architectural Comparison Table

| Feature / Metric | `limit_conn` Module | `limit_req` Module | `limit_rate` Directive |
| :--- | :--- | :--- | :--- |
| **Metric Tracked** | Active open TCP sockets | Request arrival frequency | Data transfer speed |
| **Units** | Integer (e.g., `10`) | Rate (e.g., `5r/s`) | Bytes per sec (e.g., `500k`) |
| **Memory Zone Required**| **Yes** (`limit_conn_zone`)| **Yes** (`limit_req_zone`)| No (Per-socket control) |
| **Primary Threat Mitigated**| Slowloris, socket exhaustion | Brute force, API scraping | Bandwidth saturation |
| **Queuing / Delay Support** | None (Immediate rejection) | Yes (`burst`, `delay`) | N/A |
| **Action on Breach** | Returns HTTP 429 / 503 | Returns HTTP 429 / 503 | Delays packet transmission |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: What is the fundamental difference between limit_req and limit_conn?</strong></summary>

**Answer:**
- **`limit_req` (Rate Limiting)**: Restricts request **frequency** over a time window (velocity). It monitors how many requests arrive per second or minute using the Leaky Bucket algorithm. Once a request completes its cycle (even if it takes only 5ms), it no longer consumes active server resources.
- **`limit_conn` (Connection Limiting)**: Restricts the **number of simultaneous active in-flight connections** (concurrency). It tracks how many client sockets are currently open and actively processing data (reading headers, waiting for upstream responses, or transferring bodies). A client making only 1 request per minute can still violate `limit_conn` if that single request holds the connection open for 10 minutes.
</details>

<details>
<summary><strong>Q2: How does a Slowloris attack work, and how does NGINX defend against it?</strong></summary>

**Answer:**
A Slowloris attack is a Layer 7 denial-of-service technique where an attacker opens hundreds or thousands of TCP connections to a web server and transmits incomplete HTTP headers at agonizingly slow intervals (e.g., sending `X-Header: val\r\n` every 15 seconds). Because the headers are never completed, standard thread-per-connection servers (like Apache MPM prefork) hold worker threads open waiting for the request to complete, quickly exhausting thread pools and denying service to legitimate clients.

**NGINX Defense**:
1. **Event-Driven Architecture**: NGINX does not dedicate a thread per connection; a single worker process handles tens of thousands of connections asynchronously via `epoll`.
2. **Aggressive Timeouts**: Lowering `client_header_timeout` and `client_body_timeout` from `60s` to `10s` forces NGINX to close connections if the client does not send consecutive bytes within 10 seconds.
3. **`limit_conn`**: Caps the maximum concurrent connections per IP address (e.g., `limit_conn per_ip 10;`).
4. **`reset_timedout_connection on;`**: Terminates stalled sockets instantly with TCP RST packets.
</details>

<details>
<summary><strong>Q3: Why doesn't an idle HTTP keepalive connection count against limit_conn?</strong></summary>

**Answer:**
In NGINX, a connection is added to the `limit_conn` red-black tree counter **only after NGINX has read the request headers and is actively processing the request**. 

Once NGINX finishes transmitting the HTTP response to the client:
- If keepalive is enabled, the request processing phase ends, and NGINX decrements the `limit_conn` counter in shared memory.
- The underlying TCP connection remains open in an idle keepalive state, waiting for the client to send a new request.
Because idle keepalive sockets consume virtually zero CPU and minimal memory, NGINX does not penalize clients for holding persistent connections open.
</details>

<details>
<summary><strong>Q4: How does limit_rate_after interact with limit_rate?</strong></summary>

**Answer:**
`limit_rate_after` specifies an initial payload byte threshold that is transmitted at the maximum available network line speed without any bandwidth throttling. 

Once the total bytes sent across that connection exceed `limit_rate_after`, NGINX's output filter kicks in and enforces the rate cap specified in `limit_rate`. 

This is the standard pattern for streaming media: players can pre-buffer the first 5MB or 10MB of video instantly to start playback without stuttering, after which NGINX throttles delivery to match the video's actual bitrate, saving server bandwidth.
</details>

<details>
<summary><strong>Q5: What happens when limit_conn per_server and limit_conn per_ip are both configured in the same block?</strong></summary>

**Answer:**
NGINX evaluates all configured `limit_conn` directives sequentially. 
If an incoming request violates **either** the per-IP limit or the global per-server limit, NGINX rejects the request immediately with the configured `limit_conn_status` (e.g., HTTP 429). The request is only permitted to proceed if it satisfies the limits of **both** zones simultaneously.
</details>

<details>
<summary><strong>Q6: Why is reset_timedout_connection on recommended in high-traffic environments?</strong></summary>

**Answer:**
When a client connection times out (e.g., due to client disconnection, network failure, or timeout enforcement), standard TCP teardown requires sending a `FIN` packet and transitioning through the `FIN_WAIT_1`, `FIN_WAIT_2`, and `TIME_WAIT` states. During this time, the OS kernel keeps socket buffers allocated in memory.

Setting `reset_timedout_connection on;` instructs NGINX to send a `TCP RST` (reset) packet instead of a graceful `FIN`. The RST packet immediately tears down the socket on both ends, freeing the kernel socket buffers and NGINX worker connection slots instantaneously.
</details>

<details>
<summary><strong>Q7: Can limit_rate be controlled dynamically using variables?</strong></summary>

**Answer:**
Yes. The `limit_rate` directive accepts variable arguments (e.g., `limit_rate $download_speed;`). 

By pairing `limit_rate` with the `map` directive, you can dynamically adjust bandwidth limits based on:
- User authentication tier (e.g., free vs premium subscribers).
- Client IP address or geolocation.
- Time of day.
Setting the variable to `0` completely disables rate limiting for that request.
</details>

<details>
<summary><strong>Q8: What is the purpose of limit_conn_dry_run?</strong></summary>

**Answer:**
`limit_conn_dry_run on;` allows operators to test and calibrate connection limits in production without impacting live users. 

When dry-run mode is active:
- NGINX tracks connections in the shared memory zone and evaluates limits.
- If a client exceeds the connection limit, NGINX logs an alert to `error.log`.
- However, NGINX **does not reject the connection**. The connection proceeds normally.
This enables engineers to audit real-world concurrency peaks and ensure limits are safe before enforcing hard rejections.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Per-IP Connection Limiting with HTTP 429</strong></summary>

**Goal:** Configure NGINX to cap connections from a single IP address to a maximum of 3 concurrent sockets, returning HTTP 429 on breach.

**Solution:**
```nginx
limit_conn_zone $binary_remote_addr zone=ip_cap:10m;

server {
    listen 80;
    server_name conn.local;

    limit_conn_status 429;

    location / {
        limit_conn ip_cap 3;
        proxy_pass http://127.0.0.1:4000;
    }
}
```
</details>

<details>
<summary><strong>Exercise 2: Implement 250 KB/s Bandwidth Throttling after 1MB</strong></summary>

**Goal:** Throttle downloads under `/files/` so that the first 1MB downloads instantly, followed by a 250 KB/s speed cap.

**Solution:**
```nginx
server {
    listen 80;
    server_name files.local;
    root /var/www/files;

    location /files/ {
        limit_rate_after 1m;
        limit_rate 250k;
        sendfile on;
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Harden NGINX against Slowloris Attacks</strong></summary>

**Goal:** Reduce header and body timeouts to 10 seconds and enable immediate TCP reset on timeouts.

**Solution:**
```nginx
server {
    listen 80;
    server_name hardened.local;

    client_header_timeout 10s;
    client_body_timeout   10s;
    send_timeout          10s;
    reset_timedout_connection on;

    location / {
        return 200 "Hardened against slowloris\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 4: Test Connection Limits with Parallel curl Calls</strong></summary>

**Goal:** Verify connection rejection by triggering 5 background curl downloads on an endpoint with `limit_conn 2`.

**Solution:**
```bash
# In terminal:
for i in {1..5}; do
  curl -s -o /dev/null -w "%{http_code}\n" http://conn.local/slow &
done; wait
# Output should show two 200s and three 429s.
```
</details>

<details>
<summary><strong>Exercise 5: Dynamic Bandwidth Throttling Based on Query Argument</strong></summary>

**Goal:** If a user passes `?fast=true`, grant uncapped speed; otherwise throttle to 100 KB/s.

**Solution:**
```nginx
map $arg_fast $speed_cap {
    "true"  0;    # Uncapped
    default 100k; # 100 KB/s
}

server {
    listen 80;
    location /data {
        limit_rate $speed_cap;
        return 200 "Streaming data...\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 6: Test Connection Limiting in Dry-Run Mode</strong></summary>

**Goal:** Enable `limit_conn_dry_run on;`, breach the limit, and verify that HTTP 200 is returned while violations appear in `error.log`.

**Solution:**
```nginx
location /dry-run {
    limit_conn ip_cap 1;
    limit_conn_dry_run on;
    return 200 "Dry run success\n";
}
```
Run parallel requests and verify error log:
```bash
tail -n 10 /var/log/nginx/error.log | grep "dry run"
```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production Concurrency & Bandwidth Cheat Sheet

# 1. Define Zones in http context
# 10MB zone tracks ~160,000 unique client IPs
limit_conn_zone $binary_remote_addr zone=PER_IP:10m;
limit_conn_zone $server_name        zone=PER_SERVER:10m;

server {
    # Always use HTTP 429 for rate/conn limit violations
    limit_conn_status 429;
    limit_conn_log_level warn;

    # 2. Slowloris Defense Timeouts
    client_header_timeout 10s;
    client_body_timeout   10s;
    send_timeout          10s;
    reset_timedout_connection on;

    # 3. Connection & Bandwidth Enforcement
    location /downloads/ {
        # Cap simultaneous sockets
        limit_conn PER_IP 2;
        limit_conn PER_SERVER 500;

        # Bandwidth Shaping: First 10MB free, then 500 KB/s
        limit_rate_after 10m;
        limit_rate 500k;

        sendfile on;
        tcp_nopush on;
    }
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_limit_conn_module Documentation](https://nginx.org/en/docs/http/ngx_http_limit_conn_module.html)
- [Official NGINX ngx_http_core_module limit_rate Documentation](https://nginx.org/en/docs/http/ngx_http_core_module.html#limit_rate)
- [Mitigating Slowloris and Slow HTTP Attacks with NGINX](https://www.nginx.com/blog/mitigating-ddos-attacks-with-nginx-and-nginx-plus/)
- [RFC 6585: Additional HTTP Status Codes (429)](https://datatracker.ietf.org/doc/html/rfc6585#section-4)

---

[← Previous: Canary & Blue-Green Deployments](06-canary-and-blue-green-deployments.md) | [Module Overview](README.md) | [Next: Circuit Breaking & Upstream Health Checks →](08-circuit-breaking-and-upstream-health-checks.md)
