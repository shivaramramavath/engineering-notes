---
title: "WebSocket and Server-Sent Events (SSE) Proxying in NGINX"
description: "Configuring NGINX for long-lived real-time connections: HTTP/1.1 protocol upgrade handshakes, SSE unbuffered streaming, timeout tuning, and connection scaling."
module: "02-core-features"
track: "devops-nginx"
order: 5
difficulty: "Intermediate to Advanced"
prerequisites:
  - "02-core-features/02-reverse-proxy"
  - "02-core-features/04-variables-and-rewrites"
tags:
  - nginx
  - websocket
  - sse
  - real-time
  - proxy-buffering
  - event-source
  - streaming
last_updated: "2026-03-30"
---

# WebSocket and Server-Sent Events (SSE) Proxying in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: The Canonical WebSocket Proxy with Map](#example-1-the-canonical-websocket-proxy-with-map)
  - [Example 2: Server-Sent Events (SSE) with Unbuffered Streaming](#example-2-server-sent-events-sse-with-unbuffered-streaming)
  - [Example 3: Combined REST API and WebSocket Multiplexing](#example-3-combined-rest-api-and-websocket-multiplexing)
  - [Example 4: Upstream Load Balancing for Stateful WebSocket Clusters](#example-4-upstream-load-balancing-for-stateful-websocket-clusters)
  - [Example 5: High-Concurrency WebSocket Gateway Tuning (100k+ Connections)](#example-5-high-concurrency-websocket-gateway-tuning-100k-connections)
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

Modern interactive web applications (chat platforms, live financial tickers, gaming backends, collaborative documents, and AI streaming chat interfaces) rely on two fundamental real-time paradigms:
1. **WebSockets (RFC 6455)**: Full-duplex, bidirectional communication over a single persistent TCP connection.
2. **Server-Sent Events (SSE / W3C EventSource)**: Unidirectional, server-to-client streaming of text events over standard HTTP.

By default, NGINX is optimized for standard request-response HTTP cycles: it uses HTTP/1.0 when speaking to upstreams (which closes connections after every request), buffers upstream responses in memory chunks before forwarding them to clients, and aggressively closes idle connections after 60 seconds.

Without explicit configuration, **WebSocket upgrade handshakes will fail** (NGINX will strip the `Upgrade` header or close the socket), and **SSE streams will hang** because NGINX will buffer event chunks until internal 4KB/8KB buffers fill up rather than streaming them immediately to the browser. Configuring NGINX for real-time traffic transforms it into an ultra-low-latency bidirectional streaming gateway.

---

## 2. Core Concepts

### The WebSocket Upgrade Handshake
The WebSocket protocol starts life as a standard HTTP/1.1 request. The client sends specific hop-by-hop headers:
```http
GET /chat HTTP/1.1
Host: server.example.com
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Sec-WebSocket-Version: 13
```
If the backend accepts the connection, it returns:
```http
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
```
Once the `101 Switching Protocols` status code is exchanged, the TCP socket transitions into a bidirectional binary/text framing protocol, and HTTP semantics cease.

### Hop-by-Hop Headers and the NGINX Proxy Trap
Under RFC 2616 / RFC 7230, `Upgrade` and `Connection` are **hop-by-hop headers**. They are intended only for a single transport link and must not be blindly retransmitted by proxies. By default, NGINX's reverse proxy module explicitly strips both headers when forwarding requests to backends. To proxy WebSockets, you must explicitly instruct NGINX to pass them through.

### The Canonical `map` Directive
If a client sends an `Upgrade: websocket` header, NGINX must set `Connection "upgrade"`. However, if a regular HTTP client connects to the same location, setting `Connection "upgrade"` would break normal HTTP requests. The industry-standard solution is an on-demand `map`:
```nginx
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}
```

### Server-Sent Events (SSE) and Response Buffering
Server-Sent Events use `Content-Type: text/event-stream`. Unlike WebSockets, SSE remains standard HTTP. However, NGINX's default `proxy_buffering on;` collects response bytes until a 4KB/8KB buffer is full before transmitting them to the client. This breaks real-time streaming (such as LLM token generation or live metrics). SSE requires **disabling proxy buffering** or passing the `X-Accel-Buffering: no` response header from the backend.

---

## 3. How It Works Internally

### WebSocket Tunneling State Machine in NGINX

```
Client                             NGINX Event Loop                    Upstream Backend
  |                                        |                                  |
  |--- HTTP GET (Upgrade: websocket) ----->|                                  |
  |                                        |--- HTTP/1.1 (Upgrade/Connection)->|
  |                                        |<-- HTTP 101 Switching Protocols -|
  |<-- HTTP 101 Switching Protocols -------|                                  |
  |                                        |                                  |
  +========================================+==================================+
  |              TCP STREAM TUNNEL ESTABLISHED (Full Duplex)                 |
  +========================================+==================================+
  |--- Binary/Text WebSocket Frame ------->| [epoll read -> write direct] --->|
  |<-- Binary/Text WebSocket Frame --------|<-- [epoll read -> write direct] -|
  |                                        |                                  |
  |        [Idle connection monitored by proxy_read_timeout timer]           |
  |                                        |                                  |
```

1. **Handshake Phase**: NGINX parses the incoming client headers. If configured with `proxy_set_header Upgrade $http_upgrade;` and `proxy_http_version 1.1;`, it forwards the headers to the upstream.
2. **Tunnel Mode Activation**: When NGINX receives the `101 Switching Protocols` response from upstream, it detaches the HTTP request parser. The connection transitions into an internal transparent proxy tunnel (`ngx_http_upstream_process_non_buffered_downstream`).
3. **Event Loop Epoll Piping**: Data frames arriving from either the client socket or upstream socket are piped directly across without HTTP header re-parsing.
4. **Timeout Enforcement**: The `proxy_read_timeout` timer resets whenever a frame (including ping/pong control frames) is read. If neither client nor backend sends any data within `proxy_read_timeout`, NGINX terminates the TCP socket with a TCP RST or FIN.

---

## 4. Syntax & Directives Reference

| Directive | Context | Default | Purpose / WebSocket & SSE Role |
| :--- | :--- | :--- | :--- |
| `proxy_http_version 1.1` | `http`, `server`, `location` | `1.0` | **Mandatory** for WebSockets and HTTP keep-alive to upstreams. |
| `proxy_set_header Upgrade $http_upgrade` | `http`, `server`, `location` | None | Passes the hop-by-hop `Upgrade` header through to upstream. |
| `proxy_set_header Connection $connection_upgrade` | `http`, `server`, `location` | `close` | Sets `Connection: upgrade` if `$http_upgrade` is present; else `close`. |
| `proxy_read_timeout time` | `http`, `server`, `location` | `60s` | Maximum time NGINX waits for upstream to transmit data before closing. |
| `proxy_send_timeout time` | `http`, `server`, `location` | `60s` | Maximum time NGINX waits for client to acknowledge transmitted data. |
| `proxy_buffering on\|off` | `http`, `server`, `location` | `on` | **Must be `off` for SSE** to ensure real-time chunk flushing. |
| `proxy_cache_bypass $variable` | `http`, `server`, `location` | None | Ensures real-time streaming routes are never cached in proxy cache. |
| `chunked_transfer_encoding on\|off` | `http`, `server`, `location` | `on` | Enables HTTP/1.1 chunked encoding for variable-length SSE streams. |

---

## 5. Practical Implementation Examples

### Example 1: The Canonical WebSocket Proxy with Map

The industry-standard production setup in `nginx.conf`.

```nginx
# /etc/nginx/conf.d/websocket.conf

# Map Upgrade header to Connection header value
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

upstream ws_backend {
    server 127.0.0.1:8080 max_fails=3 fail_timeout=10s;
    keepalive 32; # Keep pooled connections open to backend
}

server {
    listen 443 ssl;
    http2 on;
    server_name realtime.example.com;

    ssl_certificate     /etc/ssl/certs/realtime.crt;
    ssl_certificate_key /etc/ssl/private/realtime.key;

    location /socket.io/ {
        proxy_pass http://ws_backend;
        
        # Mandatory WebSocket Protocol Upgrades
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $connection_upgrade;

        # Standard Proxy Headers
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # Keep alive idle WebSockets for up to 24 hours (fallback if pings fail)
        proxy_read_timeout 86400s;
        proxy_send_timeout 86400s;
    }
}
```

### Example 2: Server-Sent Events (SSE) with Unbuffered Streaming

Ensures AI completions (e.g., OpenAI / Ollama token streaming) and live dashboard notifications flush immediately without buffering.

```nginx
# /etc/nginx/conf.d/sse_streaming.conf

upstream ai_stream_backend {
    server 127.0.0.1:11434;
}

server {
    listen 443 ssl;
    server_name stream.example.com;

    ssl_certificate     /etc/ssl/certs/stream.crt;
    ssl_certificate_key /etc/ssl/private/stream.key;

    location /v1/events/ {
        proxy_pass http://ai_stream_backend;
        proxy_http_version 1.1;

        # SSE Buffering Directives
        proxy_buffering off;         # Disable disk/memory chunk buffering
        proxy_cache off;             # Disable proxy response cache
        chunked_transfer_encoding on; # Ensure chunked delivery

        # Set headers
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header Connection ''; # Clear Connection header to use HTTP/1.1 keepalive

        # Prevent browser and intermediate proxies from caching event stream
        proxy_hide_header Cache-Control;
        add_header Cache-Control "no-cache, no-transform" always;

        # Extend read timeout for slow-generating LLMs or long event gaps
        proxy_read_timeout 3600s;
    }
}
```

### Example 3: Combined REST API and WebSocket Multiplexing

Route REST API calls (`/api/`) and real-time WebSockets (`/ws/`) through a single unified edge domain.

```nginx
# /etc/nginx/conf.d/api_ws_gateway.conf

map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

server {
    listen 443 ssl;
    http2 on;
    server_name app.example.com;

    ssl_certificate     /etc/ssl/certs/app.crt;
    ssl_certificate_key /etc/ssl/private/app.key;

    # 1. Standard Buffered REST API
    location /api/ {
        proxy_pass http://api_backend;
        proxy_buffering on;
        proxy_buffers 8 16k;
        proxy_buffer_size 16k;
        proxy_read_timeout 30s;
        proxy_set_header Host $host;
    }

    # 2. Real-Time Bidirectional WebSockets
    location /ws/ {
        proxy_pass http://ws_backend;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $connection_upgrade;
        proxy_set_header Host $host;
        proxy_read_timeout 3600s;
    }

    # 3. Static Web Application Assets
    location / {
        root /var/www/frontend/dist;
        try_files $uri $uri/ /index.html;
    }
}
```

### Example 4: Upstream Load Balancing for Stateful WebSocket Clusters

When WebSockets maintain in-memory state (like room participants), load balancing requires sticky sessions via `ip_hash` or `hash $cookie`.

```nginx
# /etc/nginx/conf.d/stateful_ws_cluster.conf

upstream chat_cluster {
    # Route clients to the same backend based on IP address
    ip_hash;

    server 10.0.1.10:8000 max_fails=2 fail_timeout=5s;
    server 10.0.1.11:8000 max_fails=2 fail_timeout=5s;
    server 10.0.1.12:8000 max_fails=2 fail_timeout=5s;

    # Upstream persistent connection pool
    keepalive 64;
}

server {
    listen 80;
    server_name chat.example.com;

    location / {
        proxy_pass http://chat_cluster;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $connection_upgrade;
        proxy_set_header Host $host;
        proxy_read_timeout 7200s;
    }
}
```

### Example 5: High-Concurrency WebSocket Gateway Tuning (100k+ Connections)

Tuning worker processes, OS file descriptors, and socket buffers for massive persistent connection loads.

```nginx
# /etc/nginx/nginx.conf

user nginx;
worker_processes auto;
worker_rlimit_nofile 1048576; # Max open file descriptors for master + workers

events {
    worker_connections 65536; # Max simultaneous sockets per worker process
    use epoll;
    multi_accept on;
}

http {
    # Low socket buffer allocation per connection to conserve memory for 100k+ sockets
    proxy_buffer_size 4k;
    proxy_buffers 4 4k;

    # Map for WebSocket header handling
    map $http_upgrade $connection_upgrade {
        default upgrade;
        ''      close;
    }

    server {
        listen 443 ssl default_server;
        server_name _;

        ssl_certificate     /etc/ssl/certs/gateway.crt;
        ssl_certificate_key /etc/ssl/private/gateway.key;

        location / {
            proxy_pass http://ws_node_cluster;
            proxy_http_version 1.1;
            proxy_set_header Upgrade $http_upgrade;
            proxy_set_header Connection $connection_upgrade;
            proxy_set_header Host $host;
            proxy_read_timeout 86400s;
        }
    }
}
```

---

## 6. Production Best Practices

1. **Always Use the Map Directive**: Never hardcode `proxy_set_header Connection "upgrade";`. If a regular non-WebSocket HTTP request hits that location, the backend will attempt a protocol upgrade or drop the connection.
2. **Implement Application-Level Heartbeats**: Do not rely solely on `proxy_read_timeout 86400s`. Cloud firewalls, NAT gateways (e.g., AWS NAT Gateway), and corporate proxies will silently kill idle TCP connections after 300 to 350 seconds. Ensure your WebSocket application sends ping/pong frames every 30 to 45 seconds.
3. **Disable Buffering for SSE Explicitly**: Ensure `proxy_buffering off;` is configured on SSE endpoints, or ensure your backend emits the header `X-Accel-Buffering: no`.
4. **Tune `worker_rlimit_nofile`**: Every persistent WebSocket connection consumes two file descriptors on NGINX (one for client-to-NGINX, one for NGINX-to-upstream). If you expect 20,000 concurrent WebSockets, you need at least 40,000 available file descriptors.
5. **Set HTTP/1.1 for Upstream Keepalive**: Always configure `proxy_http_version 1.1;` when proxying real-time traffic to prevent NGINX from closing upstream sockets prematurely.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Omitting `proxy_http_version 1.1;`** | NGINX defaults to HTTP/1.0 for upstreams. HTTP/1.0 does not support `Upgrade` or persistent connections, causing handshake failure. | Always declare `proxy_http_version 1.1;` inside WebSocket locations. |
| **Hardcoding `proxy_set_header Connection "upgrade"`** | When standard HTTP/1.1 requests enter the location, they receive a malformed header, breaking regular REST operations. | Use `map $http_upgrade $connection_upgrade` to set dynamic value. |
| **Leaving `proxy_read_timeout` at default 60s** | Connections with quiet periods exceeding 60 seconds are dropped abruptly by NGINX with HTTP 504 / TCP FIN. | Increase `proxy_read_timeout` (e.g. `3600s`) AND implement periodic application ping/pong heartbeats. |
| **Leaving `proxy_buffering on;` for SSE** | Events are trapped in NGINX's internal 4KB buffer until full; client receives zero live events, destroying real-time UX. | Add `proxy_buffering off;` and `proxy_cache off;` to SSE location. |
| **Insufficient OS File Descriptors** | `worker_connections` exceeds OS `ulimit -n`, causing NGINX error log: `accept4() failed (24: Too many open files)`. | Set `worker_rlimit_nofile 1048576;` in `nginx.conf` and adjust `/etc/security/limits.conf`. |
| **Socket.io transport fallback failure** | Socket.io starts with HTTP long-polling before upgrading. If load balancing without sticky sessions, poll requests hit wrong nodes. | Use `ip_hash;` or cookie affinity on upstream clusters. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test WebSocket Upgrade Handshake via curl
curl -i -N \
  -H "Connection: Upgrade" \
  -H "Upgrade: websocket" \
  -H "Sec-WebSocket-Key: SGVsbG8sIHdvcmxkIQ==" \
  -H "Sec-WebSocket-Version: 13" \
  https://realtime.example.com/socket.io/

# Expected response:
# HTTP/1.1 101 Switching Protocols
# Upgrade: websocket
# Connection: upgrade

# 2. Test Real-time SSE Stream (Unbuffered) via curl
curl -N -H "Accept: text/event-stream" https://stream.example.com/v1/events/

# 3. Monitor Active Concurrent Sockets on NGINX host
ss -s
# Or check active TCP states:
netstat -an | awk '/^tcp/ {++y[$NF]} END {for(w in y) print w, y[w]}'

# 4. Inspect Open File Descriptors for NGINX worker
ls -l /proc/$(pgrep -f "nginx: worker" | head -n 1)/fd | wc -l
```

### Memory Footprint of Persistent Sockets
- Each active TCP connection consumes kernel socket memory (buffer allocated by `rmem` and `wmem`).
- For 50,000 concurrent WebSockets, tuning Linux sysctl:
  ```ini
  net.ipv4.tcp_rmem = 4096 87380 4194304
  net.ipv4.tcp_wmem = 4096 65536 4194304
  ```
  allows NGINX to hold tens of thousands of idle connections with less than 2GB total system RAM.

---

## 9. Architectural Comparison Table

| Feature / Trait | WebSockets (RFC 6455) | Server-Sent Events (SSE) | HTTP Long-Polling |
| :--- | :--- | :--- | :--- |
| **Communication Direction** | Full-duplex (Bidirectional) | Simplex (Server to Client only) | Half-duplex (Simulated push) |
| **Protocol** | Custom TCP framing (post HTTP 101) | Standard HTTP/1.1 or HTTP/2 | Standard HTTP/1.0 or 1.1 |
| **NGINX Buffering** | Completely bypassed (TCP tunnel) | Must disable `proxy_buffering` | Standard buffering applied |
| **Reconnection Support** | Manual handling in client JS | Built-in native browser retry | Manual loop in client JS |
| **Binary Data Support** | Native binary and UTF-8 frames | Text/UTF-8 only (Base64 required) | Base64 or multipart |
| **Header Overhead** | 2 to 14 bytes per frame | Standard HTTP headers on connect | Full HTTP headers on every request |
| **Best Used For** | Multiplayer games, collaborative editing, chat | AI token streaming, live stock feeds | Legacy fallback when WS blocked |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: Why does NGINX strip the Upgrade and Connection headers by default?</strong></summary>

**Answer:**
Under the HTTP specification (RFC 2616 and RFC 7230), `Upgrade` and `Connection` are classified as **hop-by-hop headers**. Hop-by-hop headers are intended strictly for a single point-to-point transport link between an immediate client and an immediate proxy, not for end-to-end transmission across an entire proxy chain. 

As a compliant HTTP proxy, NGINX strips all hop-by-hop headers by default to prevent downstream connection parameters from being misinterpreted by upstream backends. To forward a WebSocket handshake, NGINX must be explicitly configured to reconstruct and pass these headers.
</details>

<details>
<summary><strong>Q2: Explain why map $http_upgrade $connection_upgrade is preferred over a hardcoded proxy_set_header Connection "upgrade".</strong></summary>

**Answer:**
If you hardcode `proxy_set_header Connection "upgrade";`, every request passing through that `location` block—including standard REST GET/POST requests that do not contain an `Upgrade` header—will be forwarded to the backend with `Connection: upgrade`. Many backend application servers (Node.js, Go, Python Gunicorn) will either reject the request with `400 Bad Request` or attempt to initiate a WebSocket upgrade protocol exchange on a non-WebSocket request.

Using `map $http_upgrade $connection_upgrade`:
- When `$http_upgrade` is present (WebSocket), `$connection_upgrade` evaluates to `"upgrade"`.
- When `$http_upgrade` is empty (standard HTTP), `$connection_upgrade` evaluates to `"close"`.
This ensures standard HTTP requests and WebSocket upgrade requests can safely share the same proxy location.
</details>

<details>
<summary><strong>Q3: What causes Server-Sent Events (SSE) to stop streaming in real time behind NGINX, and how do you fix it?</strong></summary>

**Answer:**
The root cause is NGINX's default response buffering (`proxy_buffering on;`). When buffering is enabled, NGINX reads response bytes from the upstream and holds them in internal memory buffers (typically 4KB or 8KB) until the buffer is filled before flushing data downstream to the client. Because SSE events are typically small text lines (50–200 bytes), they sit trapped in NGINX's memory buffer until enough events arrive or the connection terminates.

To fix it:
1. In NGINX configuration, add `proxy_buffering off;` and `proxy_cache off;` inside the SSE `location` block.
2. Alternatively, the backend application can send the response header `X-Accel-Buffering: no`, which instructs NGINX to disable response buffering dynamically for that specific request.
</details>

<details>
<summary><strong>Q4: Why does an idle WebSocket disconnect after exactly 60 seconds behind NGINX, and how do you resolve it properly?</strong></summary>

**Answer:**
The `proxy_read_timeout` directive defaults to `60s`. When a WebSocket tunnel is established, NGINX monitors traffic activity. If neither the client nor the upstream backend transmits any data across the socket for 60 consecutive seconds, NGINX's timer expires and it closes the TCP connection.

The proper resolution is twofold:
1. Increase `proxy_read_timeout` in NGINX to a reasonable operational threshold (e.g., `proxy_read_timeout 3600s;`).
2. Implement **application-level heartbeat pings** (WebSocket Ping/Pong frames) every 30 to 45 seconds. Heartbeat frames reset NGINX's `proxy_read_timeout` timer and prevent intermediate cloud firewalls, NAT gateways, and stateful load balancers from silently dropping the connection.
</details>

<details>
<summary><strong>Q5: How many file descriptors does NGINX consume per active WebSocket connection, and what system limits must be tuned?</strong></summary>

**Answer:**
Each proxied WebSocket connection consumes **two file descriptors** within NGINX:
1. One file descriptor for the inbound TCP socket from the client.
2. One file descriptor for the outbound TCP socket to the upstream backend server.

To support high concurrency (e.g., 50,000 active WebSockets):
- NGINX requires at least 100,000 available file descriptors.
- In `nginx.conf`: `worker_rlimit_nofile 131072;` and `events { worker_connections 65536; }`.
- In Linux OS: `/etc/security/limits.conf` (`nofile 131072`), and `/proc/sys/fs/file-max`.
</details>

<details>
<summary><strong>Q6: Can NGINX terminate HTTP/2 or HTTP/3 on the client side while proxying WebSockets to an HTTP/1.1 backend?</strong></summary>

**Answer:**
Yes. Modern NGINX supports terminating HTTP/2 (and HTTP/3 QUIC) on the client side. When an HTTP/2 client initiates a WebSocket connection (using RFC 8441 Extended CONNECT method in supported builds, or standard HTTP/1.1 upgrade negotiation), NGINX terminates the client TLS/HTTP session and establishes a clean HTTP/1.1 upgrade connection to the backend upstream.
</details>

<details>
<summary><strong>Q7: Why is ip_hash or sticky session routing necessary when load balancing Socket.io clusters?</strong></summary>

**Answer:**
Socket.io begins its connection lifecycle by establishing standard HTTP long-polling requests before attempting an upgrade to WebSockets. Furthermore, Socket.io stores session handshake state in the memory of the specific backend process that handled the initial polling request. 

If multiple backend nodes sit behind NGINX without sticky sessions, round-robin load balancing will send subsequent polling requests or the upgrade handshake to a different server instance, which will return `400 Bad Request: Session ID unknown`. Using `ip_hash;` or cookie-based session persistence ensures all requests from a client hit the exact same backend process.
</details>

<details>
<summary><strong>Q8: What is the purpose of proxy_set_header Connection '' (empty string) in keep-alive upstream configurations?</strong></summary>

**Answer:**
In standard reverse proxying with upstream persistent connection pools (`upstream { keepalive 32; }`), NGINX must forward requests to upstreams using HTTP/1.1 without closing the connection. By default, NGINX sets the `Connection: close` header when proxying. 

By declaring `proxy_set_header Connection "";`, NGINX removes the `Connection` header completely from the upstream request, allowing the upstream server to keep the underlying TCP socket open and reuse it across multiple client requests.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Working WebSocket Reverse Proxy Configuration</strong></summary>

**Goal:** Configure NGINX to proxy requests matching `/ws/` to a local backend on port `8080`, supporting full WebSocket upgrade negotiation.

**Solution:**
```nginx
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

server {
    listen 80;
    server_name ws.local;

    location /ws/ {
        proxy_pass http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $connection_upgrade;
        proxy_set_header Host $host;
        proxy_read_timeout 300s;
    }
}
```
</details>

<details>
<summary><strong>Exercise 2: Configure Unbuffered Server-Sent Events (SSE) for AI Token Streaming</strong></summary>

**Goal:** Configure a dedicated location `/stream` that terminates TLS and proxies an SSE stream with all buffering and proxy caches disabled.

**Solution:**
```nginx
server {
    listen 443 ssl;
    server_name ai.local;

    ssl_certificate     /etc/ssl/certs/ai.crt;
    ssl_certificate_key /etc/ssl/private/ai.key;

    location /stream {
        proxy_pass http://127.0.0.1:11434;
        proxy_http_version 1.1;
        proxy_buffering off;
        proxy_cache off;
        chunked_transfer_encoding on;
        proxy_set_header Connection '';
        proxy_read_timeout 600s;
        add_header Cache-Control "no-cache, no-transform" always;
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Simulate WebSocket Handshake with curl</strong></summary>

**Goal:** Use command-line tools to verify whether an NGINX endpoint is correctly returning HTTP 101 Switching Protocols.

**Solution:**
```bash
curl -i -N \
  -H "Connection: Upgrade" \
  -H "Upgrade: websocket" \
  -H "Sec-WebSocket-Key: AQIDBAUGBwgJCgsMDQ4PEA==" \
  -H "Sec-WebSocket-Version: 13" \
  http://localhost/ws/
```
Verify that the output starts with:
```http
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: upgrade
```
</details>

<details>
<summary><strong>Exercise 4: Load Balance Stateful WebSocket Nodes with IP Hash</strong></summary>

**Goal:** Create an upstream cluster of 3 backend WebSocket servers with IP hashing and connection health checks.

**Solution:**
```nginx
upstream chat_nodes {
    ip_hash;
    server 127.0.0.1:9001 max_fails=3 fail_timeout=10s;
    server 127.0.0.1:9002 max_fails=3 fail_timeout=10s;
    server 127.0.0.1:9003 max_fails=3 fail_timeout=10s;
    keepalive 32;
}

server {
    listen 80;
    server_name chat.local;

    location / {
        proxy_pass http://chat_nodes;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $connection_upgrade;
        proxy_read_timeout 1800s;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5: Tune Worker Limits for 50,000 Concurrent WebSockets</strong></summary>

**Goal:** Configure `nginx.conf` main and events contexts to safely handle up to 50,000 concurrent long-lived WebSockets without running out of worker connections or file descriptors.

**Solution:**
```nginx
# Top-level main context
user nginx;
worker_processes auto;
worker_rlimit_nofile 131072; # Enough file descriptors for 65k dual sockets

events {
    worker_connections 65536; # Max sockets per worker
    use epoll;
    multi_accept on;
}

http {
    # Keep buffer sizes modest to prevent RAM exhaustion
    proxy_buffer_size 4k;
    proxy_buffers 4 4k;
}
```
</details>

<details>
<summary><strong>Exercise 6: Troubleshoot SSE Buffering via X-Accel-Buffering</strong></summary>

**Goal:** Verify the behavior of backend-controlled buffering by setting up an endpoint and overriding buffering dynamically from the upstream response header.

**Solution:**
1. Configure NGINX with `proxy_buffering on;` (default):
   ```nginx
   location /events {
       proxy_pass http://127.0.0.1:5000;
       proxy_http_version 1.1;
       proxy_read_timeout 300s;
   }
   ```
2. In the upstream backend (Node.js/Express/Python), emit:
   ```http
   Content-Type: text/event-stream
   X-Accel-Buffering: no
   Cache-Control: no-cache
   ```
3. Verify with `curl -N http://localhost/events` that events stream line by line immediately without waiting for buffer flushes.
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production WebSocket and SSE Cheat Sheet

# 1. Map Upgrade in http context
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

# 2. WebSocket Location
location /ws/ {
    proxy_pass http://backend_pool;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection $connection_upgrade;
    proxy_set_header Host $host;
    proxy_read_timeout 3600s;
}

# 3. Server-Sent Events (SSE) Location
location /events/ {
    proxy_pass http://backend_pool;
    proxy_http_version 1.1;
    proxy_buffering off;
    proxy_cache off;
    proxy_set_header Connection '';
    proxy_read_timeout 3600s;
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX WebSocket Proxying Guide](https://nginx.org/en/docs/http/websocket.html)
- [RFC 6455: The WebSocket Protocol](https://datatracker.ietf.org/doc/html/rfc6455)
- [W3C Server-Sent Events Specification](https://html.spec.whatwg.org/multipage/server-sent-events.html)
- [RFC 8441: Bootstrapping WebSockets with HTTP/2](https://datatracker.ietf.org/doc/html/rfc8441)

---

[← Previous: Variables & Rewrites](04-variables-and-rewrites.md) | [Module Overview](README.md) | [Next: HTTP/2 and HTTP/3 QUIC →](06-http2-and-http3-quic.md)
