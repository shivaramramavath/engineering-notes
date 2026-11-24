---
title: "Nginx and Reverse Proxy Settings for FastAPI"
description: "Production Nginx reverse proxy configuration, TLS/SSL termination, HTTP/2, WebSocket proxying, buffer tuning, rate limiting, and SSE streaming."
fastapi_version: "0.115+"
nginx_version: "1.26+"
python_version: "3.12+"
free_threading_compatible: true
tested_with: "Nginx 1.26.2, Uvicorn 0.32.0, FastAPI 0.115.6"
---

# Nginx and Reverse Proxy Settings for FastAPI

## 1. Conceptual Foundation & Mental Model

While Uvicorn is an efficient ASGI server, exposing Python web applications directly to public internet traffic is a dangerous anti-pattern. Public-facing web servers must handle malicious slowloris attacks, TLS handshake overhead, static file delivery, HTTP protocol normalization, and request buffering across unreliable mobile networks.

In production architecture, **Nginx functions as the perimeter shield and reverse proxy**:
1. **Perimeter Security & TLS Offloading**: Nginx terminates incoming TLS/SSL encryption, verifies client certificates, manages cipher suites (TLS 1.2/1.3), and protects against slow-read and slow-write DoS attacks.
2. **Buffering & Concurrency Shielding**: Nginx quickly reads incoming request payloads from slow clients into memory buffers before forwarding the completed payload to Uvicorn in a burst, shielding Python event loops from prolonged connection stalls.
3. **Protocol Multiplexing**: Nginx negotiates HTTP/2 or HTTP/3 with internet clients while proxying upstream traffic to Uvicorn over clean HTTP/1.1 keep-alive connections or high-speed UNIX domain sockets.

```
[ Internet Clients (Mobile / Web) ]
               │
        HTTPS (TLS 1.3 / HTTP/2)
               │
               ▼
[ Nginx Reverse Proxy (Perimeter Shield) ]
       ├── Terminates TLS / Enforces HSTS
       ├── Buffers slow client uploads
       ├── Applies rate limits (limit_req)
       ├── Serves static assets directly from disk (/static/)
       └── Proxies to FastAPI Cluster
               │
      HTTP/1.1 Keep-Alive OR UNIX Domain Socket
               │
               ▼
   [ FastAPI / Uvicorn ASGI Cluster ]
   (127.0.0.1:8000 OR /run/uvicorn.sock)
```

---

## 2. Architecture & Internal Mechanics

```
+---------------------------------------------------------------+
|                      Public Internet                          |
|   Client Request (Slow 3G Connection, TLS Handshake)          |
+-------------------------------+-------------------------------+
                                │
                                ▼
+---------------------------------------------------------------+
|                     Nginx Master & Workers                    |
|  - event { use epoll; worker_connections 4096; }              |
|  - ssl_session_cache shared:SSL:10m;                          |
|  - limit_req zone=api_limit burst=20 nodelay;                 |
|  - proxy_pass http://fastapi_upstream;                        |
|  - proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for|
|  - proxy_set_header X-Forwarded-Proto $scheme;                |
+-------------------------------+-------------------------------+
                                │
                                ▼ (Fast local loopback / UNIX Socket)
+---------------------------------------------------------------+
|                  Uvicorn / FastAPI Backend                    |
|  --proxy-headers --forwarded-allow-ips "127.0.0.1"            |
+---------------------------------------------------------------+
```

### Critical Header Translations
For FastAPI to generate correct absolute URLs (e.g. for OAuth redirects, Swagger UI asset links, and pagination URLs), Nginx must pass standard forwarding headers:
- `X-Forwarded-For`: The original public IP address of the client.
- `X-Forwarded-Proto`: The original protocol scheme (`https` or `http`).
- `X-Forwarded-Host`: The original `Host` header sent by the client.
- `Host`: Passes the requested domain name to the application.

---

## 3. Comprehensive Syntax & Core Mechanics

### Production `nginx.conf` Optimized for FastAPI
```nginx
user nginx;
worker_processes auto;
worker_rlimit_nofile 65535;
pid /var/run/nginx.pid;

events {
    worker_connections 4096;
    use epoll;
    multi_accept on;
}

http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    # Performance Tuning
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;
    keepalive_timeout 65;
    types_hash_max_size 2048;
    server_tokens off;

    # Rate Limiting Zones
    limit_req_zone $binary_remote_addr zone=api_rate_limit:10m rate=50r/s;
    limit_conn_zone $binary_remote_addr zone=addr_conn_limit:10m;

    # Upstream Backend Pool
    upstream fastapi_backend {
        server 127.0.0.1:8000 max_fails=3 fail_timeout=10s;
        keepalive 64; # Maintain persistent keep-alive pool to Uvicorn
    }

    server {
        listen 80;
        server_name api.example.com;
        return 301 https://$host$request_uri; # Force HTTPS
    }

    server {
        listen 443 ssl http2;
        server_name api.example.com;

        # TLS Configuration
        ssl_certificate /etc/letsencrypt/live/api.example.com/fullchain.pem;
        ssl_certificate_key /etc/letsencrypt/live/api.example.com/privkey.pem;
        ssl_protocols TLSv1.2 TLSv1.3;
        ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384;
        ssl_prefer_server_ciphers off;
        ssl_session_cache shared:SSL:10m;
        ssl_session_timeout 1d;
        ssl_session_tickets off;

        # Security Headers
        add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
        add_header X-Frame-Options "DENY" always;
        add_header X-Content-Type-Options "nosniff" always;
        add_header Referrer-Policy "strict-origin-when-cross-origin" always;

        client_max_body_size 20M;

        # Standard API Reverse Proxy
        location / {
            limit_req zone=api_rate_limit burst=20 nodelay;
            limit_conn addr_conn_limit 20;

            proxy_pass http://fastapi_backend;
            proxy_http_version 1.1;

            # HTTP Keep-Alive Header Clearing
            proxy_set_header Connection "";

            # Proxy Forwarding Headers
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header X-Forwarded-Proto $scheme;
            proxy_set_header X-Forwarded-Port $server_port;

            # Buffer Tuning
            proxy_buffering on;
            proxy_buffer_size 8k;
            proxy_buffers 8 16k;
            proxy_busy_buffers_size 32k;
            
            # Timeouts
            proxy_connect_timeout 5s;
            proxy_read_timeout 30s;
            proxy_send_timeout 30s;
        }

        # WebSocket Endpoint Proxying
        location /ws/ {
            proxy_pass http://fastapi_backend;
            proxy_http_version 1.1;
            proxy_set_header Upgrade $http_upgrade;
            proxy_set_header Connection "upgrade";
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

            # WebSocket idle timeout (extend to prevent premature drops)
            proxy_read_timeout 86400s;
            proxy_send_timeout 86400s;
        }

        # Server-Sent Events (SSE) Streaming
        location /sse/ {
            proxy_pass http://fastapi_backend;
            proxy_http_version 1.1;
            proxy_set_header Connection "";
            proxy_buffering off; # CRITICAL: Disable buffering for real-time events
            proxy_cache off;
            chunked_transfer_encoding on;
            proxy_read_timeout 3600s;
        }
    }
}
```

---

## 4. Practical Implementation: 5 Production Scenarios

### Scenario 1: UNIX Domain Socket Upstream for 15% Lower Latency
```nginx
# When Nginx and Uvicorn run on the same virtual machine:
upstream fastapi_unix_pool {
    server unix:/run/fastapi/uvicorn.sock fail_timeout=0;
}

server {
    listen 80;
    location / {
        proxy_pass http://fastapi_unix_pool;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

### Scenario 2: Caching Static Documentation & OpenAPI Schemas at Nginx Layer
```nginx
# Cache OpenAPI JSON schema and static docs assets at Nginx layer
proxy_cache_path /var/cache/nginx/api levels=1:2 keys_zone=API_CACHE:10m max_size=500m inactive=60m;

server {
    # Cache /openapi.json for 10 minutes
    location = /openapi.json {
        proxy_pass http://fastapi_backend;
        proxy_cache API_CACHE;
        proxy_cache_valid 200 10m;
        proxy_cache_use_stale error timeout updating;
        add_header X-Cache-Status $upstream_cache_status;
    }
}
```

### Scenario 3: Granular Rate Limiting by API Key Header
```nginx
# Map API Key header to rate limiting key
map $http_x_api_key $limit_key {
    default $http_x_api_key;
    ""      $binary_remote_addr; # Fall back to IP if no API key present
}

limit_req_zone $limit_key zone=tenant_rate_limit:10m rate=100r/s;

server {
    location /api/v1/ {
        limit_req zone=tenant_rate_limit burst=50 nodelay;
        proxy_pass http://fastapi_backend;
    }
}
```

### Scenario 4: Nginx Mutual TLS (mTLS) Authentication Proxy
```nginx
server {
    listen 443 ssl http2;
    server_name m2m.example.com;

    ssl_certificate /etc/nginx/certs/server.crt;
    ssl_certificate_key /etc/nginx/certs/server.key;

    # Require client certificate verification
    ssl_client_certificate /etc/nginx/certs/ca.crt;
    ssl_verify_client on;

    location / {
        # Forward verified client certificate subject to FastAPI
        proxy_pass http://fastapi_backend;
        proxy_set_header X-Client-DN $ssl_client_s_dn;
        proxy_set_header X-Client-Verified $ssl_client_verify;
    }
}
```

### Scenario 5: Blue-Green Traffic Split via Nginx Split Clients
```nginx
# Dynamically route 10% of traffic to Green (v2), 90% to Blue (v1)
split_clients "${remote_addr}${http_user_agent}" $backend_variant {
    10%     http://fastapi_green;
    *       http://fastapi_blue;
}

upstream fastapi_blue {
    server 127.0.0.1:8001;
}

upstream fastapi_green {
    server 127.0.0.1:8002;
}

server {
    location / {
        proxy_pass $backend_variant;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }
}
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Proxy Buffering Breaking Server-Sent Events (SSE)**:
   By default, `proxy_buffering on;` holds streamed data chunks until Nginx's internal buffer (e.g. 8KB) fills. In SSE streams, messages are typically 100-500 bytes, causing the client to receive no events until the stream closes. Always set `proxy_buffering off;` or pass `X-Accel-Buffering: no` on SSE routes.

2. **`Connection: close` Stalling Uvicorn Performance**:
   By default, Nginx sends `Connection: close` to upstream servers. This forces Uvicorn to open and close a new TCP connection on every incoming HTTP request. Adding `proxy_http_version 1.1;` and `proxy_set_header Connection "";` enables HTTP keep-alive socket reuse.

3. **Client IP Spoofing via Misconfigured Trust**:
   If Uvicorn's `--forwarded-allow-ips` is set to `*` and an attacker reaches Uvicorn directly or sends a forged `X-Forwarded-For` header through an insecure upstream proxy, the attacker can spoof their identity. Uvicorn must only trust Nginx's specific internal IP.

4. **HTTP 413 Payload Too Large**:
   Nginx defaults `client_max_body_size` to 1MB. If a client uploads a 5MB image to FastAPI, Nginx rejects the request with HTTP 413 *before* the request ever reaches FastAPI. Configure `client_max_body_size 20M;` in the `server` block.

---

## 6. Anti-Patterns & Common Mistakes

| Anti-Pattern | Operational Impact | Correct Pattern |
| :--- | :--- | :--- |
| Exposing Uvicorn directly to the public internet | Vulnerable to slowloris attacks and TLS handshake overhead | Always place Nginx or an enterprise reverse proxy in front |
| Leaving `proxy_buffering on` for SSE endpoints | Clients receive no events until buffer fills or socket closes | Disable buffering: `proxy_buffering off;` |
| Omitting `proxy_http_version 1.1` and `Connection ""` | Disables keep-alive; TCP connection opened per request | Configure HTTP/1.1 persistent keep-alive pool |
| Setting `client_max_body_size 1m` for upload services | Rejects valid file uploads with HTTP 413 before reaching app | Explicitly size `client_max_body_size` to match business needs |
| Omitting `proxy_set_header X-Forwarded-Proto $scheme` | FastAPI generates HTTP URLs for redirects, breaking HTTPS | Pass `X-Forwarded-Proto $scheme` to Uvicorn |
| Exposing Nginx server version in headers | Discloses software version to automated exploit scanners | Add `server_tokens off;` to `http` configuration |

---

## 7. Performance & Scalability Considerations

- **Epoll and Worker Connections**: Ensure `events { use epoll; worker_connections 4096; }` is configured on Linux to maximize concurrent socket capacity.
- **TLS Session Caching**: Configuring `ssl_session_cache shared:SSL:10m;` and `ssl_session_timeout 1d;` eliminates full cryptographic handshakes for returning clients, reducing connection establishment latency by 85%.

---

## 8. Security & OWASP Guardrails

- **HSTS Enforcement**: Enforce HTTP Strict Transport Security:
  `add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;`
- **Clickjacking & MIME Sniffing Mitigation**: Enforce standard defense headers:
  `add_header X-Frame-Options "DENY" always;`
  `add_header X-Content-Type-Options "nosniff" always;`

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
import httpx

def test_nginx_security_headers_and_proxy_forwarding():
    # Verify Nginx reverse proxy enforces security headers
    url = "https://api.example.internal"
    
    with httpx.Client(verify=False) as client:
        resp = client.get(f"{url}/health")
        assert resp.status_code == 200
        
        # Verify Security Headers
        assert resp.headers.get("x-frame-options") == "DENY"
        assert resp.headers.get("x-content-type-options") == "nosniff"
        assert "strict-transport-security" in resp.headers
        
        # Verify Server Version Token Masking
        assert resp.headers.get("server") == "nginx"  # Version number stripped
```

---

## 10. Real-World Production Incident

**Incident**: A healthcare telemedicine service launched real-time doctor-patient status updates using Server-Sent Events (SSE). Users complained that doctor status badges were delayed by up to 5 minutes, making doctors appear offline when they were active.

**Root Cause**: The infrastructure team placed Nginx in front of FastAPI but used the default reverse proxy settings. Because `proxy_buffering on;` was enabled, Nginx accumulated the small SSE update frames (80 bytes each) until its 4KB buffer filled. Updates were only flushed to browsers every 5 minutes when enough data accumulated.

**Resolution**: Added a dedicated `location /sse/` block in `nginx.conf`:
```nginx
location /sse/ {
    proxy_pass http://fastapi_backend;
    proxy_buffering off;
    proxy_cache off;
    proxy_set_header Connection "";
    chunked_transfer_encoding on;
}
```
Event latency dropped from 5 minutes to under 10 milliseconds instantly.

---

## 11. Technical Interview Questions

<details>
<summary>1. Why should you place Nginx in front of Uvicorn rather than exposing Uvicorn directly to the internet?</summary>

Exposing Uvicorn directly to the public internet presents significant operational and security risks:
1. **Slowloris & Buffer Attacks**: Uvicorn is designed for fast, local asynchronous request processing. Slow internet clients transmitting bytes incrementally tie up worker coroutines. Nginx buffers slow uploads in memory before sending complete payloads to Uvicorn in a fast burst.
2. **TLS Offloading & HTTP/2**: Nginx handles computationally heavy TLS handshakes, session caching, and HTTP/2/HTTP/3 framing at compiled C speed, allowing Uvicorn to focus purely on application logic.
3. **Static Asset Offloading**: Nginx serves static assets (images, documentation files) directly from kernel cache via `sendfile`, bypassing Python entirely.
</details>

<details>
<summary>2. What is the purpose of passing <code>proxy_set_header X-Forwarded-Proto $scheme;</code>?</summary>

When Nginx terminates TLS encryption, the connection between Nginx and Uvicorn is typically unencrypted HTTP. Without the `X-Forwarded-Proto` header, FastAPI assumes the original client request was plain HTTP. This causes FastAPI to generate insecure HTTP redirect URLs (e.g., during OAuth authentication flows or trailing slash redirects) and breaks Swagger UI asset loading. Passing `$scheme` informs FastAPI that the original connection was HTTPS.
</details>

<details>
<summary>3. How does Nginx proxy WebSockets differently from standard HTTP?</summary>

Standard HTTP/1.1 requests are stateless request-response exchanges. WebSockets begin with an HTTP Upgrade handshake and establish a persistent, bidirectional TCP connection. Nginx requires explicit configuration to support this transition:
```nginx
proxy_http_version 1.1;
proxy_set_header Upgrade $http_upgrade;
proxy_set_header Connection "upgrade";
proxy_read_timeout 86400s;
```
Without `Upgrade` and `Connection` headers, Nginx treats the WebSocket handshake as a standard HTTP call and closes the connection.
</details>

<details>
<summary>4. Why does Nginx default configuration cause issues with Server-Sent Events (SSE)?</summary>

Nginx enables proxy buffering by default (`proxy_buffering on;`). It buffers chunks received from upstream until its buffer (typically 4KB-8KB) fills. In SSE, small messages (100-300 bytes) are emitted intermittently. Nginx holds these chunks in memory instead of forwarding them immediately, breaking real-time behavior. Setting `proxy_buffering off;` ensures immediate delivery of each event chunk.
</details>

<details>
<summary>5. How do you configure persistent HTTP keep-alive connections between Nginx and Uvicorn?</summary>

In the `upstream` block, specify `keepalive <number>;`. In the `location` block, configure:
```nginx
proxy_http_version 1.1;
proxy_set_header Connection "";
```
This overrides Nginx's default behavior of sending `Connection: close`, allowing Nginx to reuse established TCP connections to Uvicorn across multiple client requests.
</details>

<details>
<summary>6. How does Nginx handle client IP preservation across multiple proxies?</summary>

The `X-Forwarded-For` header contains a comma-separated list of IP addresses representing the chain of proxies a request traversed. Using `$proxy_add_x_forwarded_for` appends the immediate client's IP (`$remote_addr`) to the existing header. Uvicorn, configured with `--proxy-headers` and trusted proxy IPs, reads the leftmost untrusted IP as the true client IP.
</details>

<details>
<summary>7. What does Nginx's <code>limit_req</code> directive do, and what does <code>nodelay</code> mean?</summary>

`limit_req` implements the Leaky Bucket rate-limiting algorithm. The `rate=` parameter sets the average allowed request frequency (e.g., `10r/s`). The `burst=` parameter allows transient spikes above this rate (e.g., `burst=20`). Without `nodelay`, excess requests are delayed and throttled to match the rate. With `nodelay`, requests within the burst allowance are processed immediately without artificial delays, rejecting only requests that exceed the total burst capacity with HTTP 429.
</details>

<details>
<summary>8. How do UNIX domain sockets improve performance when Nginx and Uvicorn share the same host?</summary>

UNIX domain sockets bypass the entire TCP/IP networking stack (packet encapsulation, checksums, loopback routing, TCP window scaling, and port allocation). Data transfer occurs via direct memory copying within the Linux kernel, reducing CPU overhead and latency by 10-15% compared to TCP loopback (`127.0.0.1`).
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic Proxy Pass Location Block
**Task**: Write an Nginx `location /api/` block that proxies requests to an upstream named `fastapi_app` on port 8000.
<details>
<summary>Solution</summary>

```nginx
location /api/ {
    proxy_pass http://fastapi_app;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
}
```
</details>

### Exercise 2 (Easy): Increase Upload File Size Limit
**Task**: Write the Nginx configuration directive to allow file uploads up to 50 Megabytes for an endpoint `/upload`.
<details>
<summary>Solution</summary>

```nginx
location /upload {
    client_max_body_size 50M;
    proxy_pass http://fastapi_app;
}
```
</details>

### Exercise 3 (Medium): Rate-Limit Login Endpoint
**Task**: Configure an Nginx rate limit allowing 5 requests per minute with a burst of 2 for `/auth/login`, returning HTTP 429 when exceeded.
<details>
<summary>Solution</summary>

```nginx
# In http block:
limit_req_zone $binary_remote_addr zone=login_limit:10m rate=5r/m;
limit_req_status 429;

# In server block:
location /auth/login {
    limit_req zone=login_limit burst=2 nodelay;
    proxy_pass http://fastapi_app;
    proxy_set_header Host $host;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
}
```
</details>

### Exercise 4 (Medium): WebSocket Configuration Block
**Task**: Write a complete Nginx `location /ws` block that properly upgrades HTTP connections to WebSockets and sets a 12-hour timeout.
<details>
<summary>Solution</summary>

```nginx
location /ws {
    proxy_pass http://fastapi_app;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_read_timeout 43200s;
    proxy_send_timeout 43200s;
}
```
</details>

### Exercise 5 (Hard): Zero-Downtime Blue/Green Switch via Nginx Reload
**Task**: Write a bash script that validates an updated Nginx configuration pointing to a new upstream port and performs a zero-downtime reload without dropping active client connections.
<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "Testing Nginx configuration syntax..."
nginx -t

echo "Configuration valid. Initiating graceful zero-downtime reload..."
nginx -s reload

echo "Nginx successfully reloaded without dropping active sockets."
```
</details>

### Exercise 6 (Hard): Complete Hardened Production Virtual Host
**Task**: Write a complete production Nginx virtual host with TLS 1.3, HSTS, security headers, Gzip compression, keep-alive upstream, rate limiting, and SSE unbuffered streaming.
<details>
<summary>Solution</summary>

```nginx
upstream fastapi_prod_pool {
    server 127.0.0.1:8000;
    keepalive 32;
}

limit_req_zone $binary_remote_addr zone=prod_api:10m rate=30r/s;

server {
    listen 443 ssl http2;
    server_name api.enterprise.internal;

    ssl_certificate /etc/ssl/certs/api.crt;
    ssl_certificate_key /etc/ssl/private/api.key;
    ssl_protocols TLSv1.3;
    ssl_session_cache shared:SSL:10m;
    ssl_session_timeout 1d;

    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-Frame-Options "DENY" always;

    gzip on;
    gzip_types application/json text/plain text/css application/javascript;
    gzip_min_length 1000;

    location / {
        limit_req zone=prod_api burst=15 nodelay;
        proxy_pass http://fastapi_prod_pool;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto https;
        proxy_buffering on;
    }

    location /stream/ {
        proxy_pass http://fastapi_prod_pool;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_buffering off;
        proxy_cache off;
        proxy_read_timeout 3600s;
    }
}
```
</details>
