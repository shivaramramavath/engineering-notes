---
title: "Project 05: Real-Time Bidirectional Streaming Gateway: WebSockets & Server-Sent Events"
description: "Production reference implementation for real-time bidirectional communication: WebSocket proxying, Server-Sent Events (SSE) streaming for LLMs, zero-buffering tuning, and extended keepalive timeouts."
module: "07-projects"
project_id: "05-websocket-and-sse-realtime-proxy"
status: "completed"
tags: ["nginx", "project", "websocket", "sse", "server-sent-events", "real-time", "streaming", "llm-tokens", "reverse-proxy"]
prerequisites: ["02-core-features/05-websockets-and-sse-proxying.md", "02-core-features/02-reverse-proxy.md"]
estimated_read_time: "35 min"
---

# Project 05: Real-Time Bidirectional Streaming Gateway: WebSockets & Server-Sent Events

```
+-------------------------------------------------------------------------------+
| Real-Time Streaming Gateway Architecture                                      |
|                                                                               |
|  [Client Browser / Mobile App]                                                |
|        │                                                                      |
|        ├── 1. WebSocket Protocol Handshake (Upgrade: websocket)               |
|        │   https://realtime.example.com/ws                                    |
|        │                                                                      |
|        └── 2. Server-Sent Events (SSE) Stream (Accept: text/event-stream)     |
|            https://realtime.example.com/api/v1/stream (LLM Token Streaming)   |
|        │                                                                      |
|        ▼                                                                      |
|  [NGINX Real-Time Gateway]                                                    |
|        │                                                                      |
|        ├── WebSocket Engine:                                                  |
|        │   - map $http_upgrade $connection_upgrade (Hop-by-hop upgrade)       |
|        │   - proxy_read_timeout 3600s; (Persistent 1-hour bidirectional pipe) |
|        │                                                                      |
|        └── SSE Engine:                                                        |
|            - proxy_buffering off; (Immediate byte-by-byte flush to client!)   |
|            - proxy_cache off; (Zero disk or RAM caching of live event stream) |
|            - chunked_transfer_encoding on;                                    |
|        │                                                                      |
|        ▼                                                                      |
|  [Backend Real-Time Service Cluster (Node.js / Python Fastify / Go)]          |
+-------------------------------------------------------------------------------+
```

---

## 1. Architecture & Objective

### Objective
Build a specialized reverse proxy gateway engineered to handle persistent, long-lived real-time connections—supporting both **bidirectional WebSockets** (for chat, multiplayer gaming, and live telemetry) and **unidirectional Server-Sent Events (SSE)** (for streaming LLM generation tokens and live notification feeds).

### Core Architectural Requirements
1. **Hop-by-Hop Protocol Switching**: Handle the standard HTTP/1.1 `101 Switching Protocols` handshake cleanly using NGINX's `map` module to pass `Upgrade: websocket` and `Connection: upgrade` headers dynamically.
2. **Zero-Buffering SSE Stream Delivery**: Disable upstream response buffering (`proxy_buffering off;`) to ensure LLM tokens and event payloads are forwarded to the client the instant they are generated, achieving sub-millisecond Time-to-First-Token (TTFT).
3. **Prevent Premature Timeout Severing**: Increase `proxy_read_timeout` and `proxy_send_timeout` from the default 60 seconds to 1 hour (3600s) to prevent idle WebSockets from disconnecting.
4. **Connection Concurrency Sizing**: Support 50,000+ simultaneous open connections per server node with tuned file descriptor caps and kernel socket memory buffers.

---

## 2. Complete Config Code

### 1. Unified Real-Time Gateway Configuration (`/etc/nginx/conf.d/realtime.example.com.conf`)

```nginx
# 1. Map HTTP Upgrade Headers Cleanly (Must be declared in http context)
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

# 2. Upstream Real-Time Backend Pool
upstream realtime_backend {
    # Hash on client IP for consistent WebSocket node stickiness
    ip_hash;
    server 10.0.2.11:8080 max_fails=2 fail_timeout=5s;
    server 10.0.2.12:8080 max_fails=2 fail_timeout=5s;

    keepalive 64;
}

# 3. HTTP to HTTPS Canonical Redirect
server {
    listen 80;
    server_name realtime.example.com;
    return 301 https://$host$request_uri;
}

# 4. Master Real-Time TLS Server
server {
    listen 443 ssl;
    http2 on;
    server_name realtime.example.com;

    # TLS 1.3 / 1.2 Configuration
    ssl_certificate /etc/letsencrypt/live/realtime.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/realtime.example.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_session_cache shared:SSL:20m;
    ssl_session_timeout 1d;

    # Security Headers
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-Frame-Options "SAMEORIGIN" always;

    # -------------------------------------------------------------
    # SECTION 1: Bidirectional WebSockets (/ws, /socket.io)
    # -------------------------------------------------------------
    location /ws/ {
        proxy_pass http://realtime_backend;
        proxy_http_version 1.1;

        # Upgrade hop-by-hop headers for WebSocket handshake
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $connection_upgrade;

        # Standard Identity Headers
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto https;

        # Disable proxy buffering for instant bidirectional messaging
        proxy_buffering off;

        # Extend timeouts to prevent NGINX from dropping idle connections
        # Relies on application ping/pong heartbeats to keep sockets alive
        proxy_connect_timeout 5s;
        proxy_read_timeout 3600s;
        proxy_send_timeout 3600s;
    }

    # -------------------------------------------------------------
    # SECTION 2: Server-Sent Events (SSE) & LLM Token Streaming
    # -------------------------------------------------------------
    location /api/v1/stream/ {
        proxy_pass http://realtime_backend;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

        # CRITICAL FOR SSE & LLMs: Turn OFF all buffering!
        proxy_buffering off;
        proxy_cache off;

        # Instruct downstream CDNs (Cloudflare) to disable buffering
        proxy_set_header X-Accel-Buffering "no";
        add_header Cache-Control "no-cache, no-transform" always;

        # Chunked Transfer Encoding
        chunked_transfer_encoding on;

        # Read timeout for slow generation models (e.g. 10 minutes)
        proxy_read_timeout 600s;
        proxy_send_timeout 600s;
    }

    # -------------------------------------------------------------
    # SECTION 3: Standard REST API Endpoints
    # -------------------------------------------------------------
    location /api/ {
        proxy_pass http://realtime_backend;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

        # Standard buffering enabled for normal API routes
        proxy_buffering on;
        proxy_buffer_size 8k;
        proxy_buffers 8 32k;
        proxy_read_timeout 30s;
    }
}
```

---

## 3. Step-by-Step Walkthrough

1. **The Dynamic `map $http_upgrade` Pattern**:
   - `Upgrade` and `Connection` are hop-by-hop HTTP headers; by default, reverse proxies strip them.
   - If a client sends an `Upgrade: websocket` header, `$http_upgrade` evaluates to `"websocket"`, and the `map` assigns `$connection_upgrade` to `"upgrade"`.
   - If a client makes a standard HTTP request without an `Upgrade` header, `$connection_upgrade` evaluates to `"close"`.
   - This ensures WebSocket handshakes succeed without corrupting standard HTTP connections that traverse the same proxy.

2. **Why `proxy_buffering off;` is Critical for SSE & LLMs**:
   - Under default NGINX configuration (`proxy_buffering on;`), NGINX waits until it accumulates a full memory buffer (e.g. 4KB or 8KB) from the upstream before sending bytes to the downstream client.
   - An LLM generating tokens produces ~10 to 30 bytes per token.
   - With buffering enabled, the end user waits 10 seconds seeing nothing, and then suddenly receives a burst of 300 tokens at once!
   - Setting `proxy_buffering off;` forces NGINX to flush each token chunk to the network socket the microsecond it arrives from the backend.

3. **Tuning `proxy_read_timeout` to 3600 Seconds**:
   - The default `proxy_read_timeout` is 60 seconds. If a chat user is idle for 60 seconds without typing, NGINX closes the TCP socket with an HTTP 504 or TCP FIN.
   - By setting `proxy_read_timeout 3600s;`, the socket remains open.
   - *Best Practice*: Application frameworks (Socket.io, WS, Gorilla) should transmit periodic ping/pong frames every 25 seconds to keep firewalls and NGINX timers refreshed.

4. **Session Stickiness via `ip_hash`**:
   - Many WebSocket frameworks (such as Socket.io) initiate a connection via HTTP Long-Polling before upgrading to WebSockets.
   - `ip_hash` ensures that all polling requests and the eventual WebSocket upgrade from the same client IP land on the exact same backend server instance.

---

## 4. Verification & Testing

### Test 1: Test WebSocket Handshake with `wscat`
Connect to the WebSocket gateway using the command-line client `wscat`:
```bash
# Install wscat if not present: npm install -g wscat
wscat -c wss://realtime.example.com/ws/
```
*Expected Output*:
```text
Connected (press CTRL+C to quit)
< {"type": "welcome", "clientId": "abc-123"}
> {"action": "ping"}
< {"action": "pong"}
```

### Test 2: Test Server-Sent Events (SSE) Unbuffered Token Streaming
Verify immediate chunk streaming using `curl`:
```bash
curl -N -v https://realtime.example.com/api/v1/stream/generate?prompt=hello
```
*Key Flag*: `-N` (`--no-buffer`) disables curl's client-side buffering.
*Expected Output*:
```http
HTTP/2 200
content-type: text/event-stream
cache-control: no-cache, no-transform
x-accel-buffering: no

data: {"token": "Hello"}

data: {"token": " world"}

data: {"token": "!"}
```
Observe each token rendering in your terminal progressively in real-time.

---

## 5. Troubleshooting & Edge Cases

### Edge Case 1: WebSocket Disconnects Every 60 Seconds
- **Symptom**: WebSocket clients disconnect precisely every 60 seconds of inactivity.
- **Root Cause**: `proxy_read_timeout` was omitted in the WebSocket `location` block, defaulting to 60 seconds.
- **Fix**: Set `proxy_read_timeout 3600s;` and implement application-level ping/pong heartbeats every 25 seconds.

### Edge Case 2: Cloudflare Buffers SSE Tokens
- **Symptom**: SSE tokens stream locally, but when routed through Cloudflare, tokens buffer into chunks.
- **Root Cause**: Cloudflare edge proxies buffer HTTP responses by default unless explicitly instructed otherwise.
- **Fix**: Emit `proxy_set_header X-Accel-Buffering "no";` and `add_header Cache-Control "no-cache, no-transform" always;`. Cloudflare recognizes these headers and disables edge buffering automatically.

---

[← Previous Project: 04-load-balanced-backend-cluster.md](04-load-balanced-backend-cluster.md) | [Back to Module Index](README.md) | [Next Project: 06-rate-limited-public-api-gateway.md →](06-rate-limited-public-api-gateway.md)
