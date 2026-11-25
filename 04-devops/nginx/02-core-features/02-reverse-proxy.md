<!--
================================================================================
Metadata:
  Title: Reverse Proxy Architecture: proxy_pass, Headers, Buffering & Timeouts
  Track: NGINX & Traffic Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 55 minutes
  Prerequisites: 01-fundamentals/03-server-and-location.md
  Key Terminology: Reverse Proxy, proxy_pass, Trailing Slash, X-Forwarded-For, Upstream Keepalive, proxy_buffering, proxy_read_timeout
================================================================================
-->

# Reverse Proxy Architecture: proxy_pass, Headers, Buffering & Timeouts

[<- 01-logging.md](./01-logging.md) | [Back to Module Overview](./README.md) | [03-tls-https.md ->](./03-tls-https.md)

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

In modern software engineering, web application frameworks (Node.js/Express, NestJS, Python FastAPI, Java Spring Boot, Go, Next.js) are designed to execute business logic, query databases, and render dynamic templates. They are **not** optimized to manage raw internet traffic: they lack high-concurrency connection pooling, robust TLS session management, edge rate limiting, and protection against slow-client attacks (like Slowloris).

Deploying NGINX as an edge **Reverse Proxy** resolves these fundamental challenges:
- **Connection Absorption & Thread Offloading**: NGINX buffers incoming requests and absorbs slow mobile clients over high-latency networks. A backend Node or Spring thread is only engaged for the few milliseconds required to process the request, rather than staying occupied for seconds waiting on slow client downloads.
- **Protocol & Security Decoupling**: NGINX terminates TLS encryption, validates HTTP/2 and HTTP/3 transport streams, and forwards clean, standardized HTTP/1.1 requests to internal microservices over private VPC networks.
- **Topological Abstraction**: Multiple disparate internal services (e.g., `/api/auth` in Go, `/api/billing` in Java, `/` in Next.js) are unified under a single public-facing domain without exposing internal IP topologies or ports.

---

## 2. Core Concepts

### The `proxy_pass` Trailing Slash Invariant
The single most critical rule when configuring NGINX reverse proxying is understanding how a **trailing slash** on the `proxy_pass` target URL alters URI rewriting:

1. **NO Trailing Slash (`proxy_pass http://backend;`)**:
   - NGINX passes the **exact, unmodified incoming client URI** directly to the backend.
   ```nginx
   location /api/ {
       proxy_pass http://10.0.0.1:8080; # Notice: NO trailing slash!
   }
   # Client Request: GET /api/v1/users
   # Backend Receives: GET /api/v1/users
   ```

2. **WITH a Trailing Slash (`proxy_pass http://backend/;` or `http://backend/path/`)**:
   - NGINX **strips the matching location prefix** from the incoming URI and appends the remainder to the `proxy_pass` path.
   ```nginx
   location /api/ {
       proxy_pass http://10.0.0.1:8080/; # Notice: Trailing slash present!
   }
   # Client Request: GET /api/v1/users
   # Backend Receives: GET /v1/users  (The prefix '/api/' was stripped!)
   ```

```
+--------------------------------------------------------------------------+
| PROXY_PASS URI REWRITING RULES                                           |
|                                                                          |
| Config 1 (Pass-Through):                                                 |
| location /service/ { proxy_pass http://backend; }                        |
| -> /service/data  ====>  http://backend/service/data                     |
|                                                                          |
| Config 2 (Prefix Replacement):                                           |
| location /service/ { proxy_pass http://backend/; }                       |
| -> /service/data  ====>  http://backend/data                             |
|                                                                          |
| Config 3 (Path Substitution):                                            |
| location /service/ { proxy_pass http://backend/internal/; }              |
| -> /service/data  ====>  http://backend/internal/data                    |
+--------------------------------------------------------------------------+
```

### Essential Forwarding Headers
Because a reverse proxy initiates a brand new TCP connection to the backend, the backend application sees the proxy's IP address (`127.0.0.1` or internal VPC IP) as the client. To preserve client telemetry, NGINX must explicitly forward client identification headers:

- **`Host $host;`**: Passes the original domain name requested by the client. (Without this, NGINX defaults to passing the upstream name declared in `proxy_pass`, breaking virtual hosts on the backend).
- **`X-Real-IP $remote_addr;`**: Passes the immediate client's IP address.
- **`X-Forwarded-For $proxy_add_x_forwarded_for;`**: Appends the client's IP to the existing chain of proxy hops.
- **`X-Forwarded-Proto $scheme;`**: Informs the backend whether the original client request was `http` or `https`, preventing infinite redirect loops in frameworks enforcing SSL.
- **`Connection "";`**: Clears the `Connection: close` header to allow persistent HTTP/1.1 keepalive connections to upstream backends.

---

## 3. How It Works Internally

### Proxy Buffering Mechanics
When `proxy_buffering on;` is enabled (the default), NGINX acts as an asynchronous buffer between the fast backend and the client:

```
+--------------------------------------------------------------------------+
| PROXY BUFFERING ARCHITECTURE                                             |
|                                                                          |
| [ Fast Upstream Backend ]                                                |
|       |                                                                  |
|       v (Transfers at 10 Gbps / LAN Speed in 5 milliseconds)             |
| [ NGINX Worker Process ]                                                 |
|   ├── Step 1: Reads response header into 'proxy_buffer_size' (e.g. 8k)   |
|   ├── Step 2: Reads response body into 'proxy_buffers' in RAM (e.g. 64k) |
|   └── Step 3: If payload > RAM buffers, writes overflow to disk:         |
|               /var/cache/nginx/proxy_temp                                |
|                                                                          |
| * Upstream backend finishes immediately and releases database connection! |
|                                                                          |
| [ NGINX Worker Process ]                                                 |
|       |                                                                  |
|       v (Trickles data to mobile client over 3G/LTE in 4.5 seconds)      |
| [ Slow Mobile Client ]                                                   |
+--------------------------------------------------------------------------+
```

If buffering is disabled (`proxy_buffering off;`), NGINX passes data synchronously from upstream to client as it arrives. This is **mandatory for Server-Sent Events (SSE)**, WebSocket connections, and real-time streaming APIs.

---

## 4. Syntax & Usage

### Core Proxy Directives
```nginx
location / {
    # Target backend address (IP, domain, or upstream block name)
    proxy_pass http://backend_pool;

    # Force HTTP/1.1 for upstream connections (defaults to HTTP/1.0!)
    proxy_http_version 1.1;

    # Clear Connection header to enable upstream keepalive
    proxy_set_header Connection "";

    # Forward client identification headers
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
    proxy_set_header X-Forwarded-Host $host;
    proxy_set_header X-Forwarded-Port $server_port;

    # Timeout Configuration
    proxy_connect_timeout 5s;   # Max time to establish connection to upstream
    proxy_send_timeout 60s;     # Max time between successive writes to upstream
    proxy_read_timeout 60s;     # Max time between successive reads from upstream

    # Buffering Configuration
    proxy_buffering on;
    proxy_buffer_size 8k;            # Buffer for upstream response headers
    proxy_buffers 8 64k;             # 8 buffers of 64 KB each for response body
    proxy_busy_buffers_size 128k;    # Max memory allowed in transit to client
    proxy_max_temp_file_size 1024m;  # Max disk buffer file size before throttling

    # Error handling
    proxy_intercept_errors on; # Allows NGINX error_page to intercept upstream 5xx
}
```

---

## 5. Practical Examples

### Example 1: Production Reverse Proxy for Node.js / NestJS Backend
```nginx
# Define upstream with persistent keepalive connections
upstream node_cluster {
    server 127.0.0.1:3000 max_fails=3 fail_timeout=10s;
    server 127.0.0.1:3001 max_fails=3 fail_timeout=10s;
    
    # Maintain up to 32 idle keepalive connections to upstream backends
    keepalive 32;
}

server {
    listen 80;
    server_name api.example.com;

    location / {
        proxy_pass http://node_cluster;
        proxy_http_version 1.1;
        
        # Enable keepalive by wiping the Hop-by-Hop Connection header
        proxy_set_header Connection "";
        
        # Standard headers
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # Timeouts tuned for fast API responses
        proxy_connect_timeout 3s;
        proxy_read_timeout 15s;
    }
}
```

### Example 2: Prefix Replacement (`/service/` -> `/`)
```nginx
server {
    listen 80;
    server_name gateway.internal;

    # Scenario: The client accesses http://gateway.internal/auth/login
    # But the internal auth service listens at http://10.0.1.20:5000/login (no /auth prefix!)
    location /auth/ {
        # Trailing slash tells NGINX to strip '/auth/' and pass '/login'
        proxy_pass http://10.0.1.20:5000/;
        
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }
}
```

### Example 3: Tuning Proxy Buffering for Massive JSON Payloads
```nginx
server {
    listen 80;
    server_name reports.internal;

    location /export/ {
        proxy_pass http://reporting_service;
        
        # Large JSON dumps (e.g. 5 MB) cause disk write warnings if buffers are too small:
        # "an upstream response is buffered to a temporary file"
        proxy_buffering on;
        proxy_buffer_size 16k;
        proxy_buffers 16 128k;       # 2 MB in-memory buffer pool
        proxy_busy_buffers_size 256k;
        
        # If response exceeds 2 MB, allow up to 500 MB on disk
        proxy_max_temp_file_size 500m;
        proxy_temp_file_write_size 64k;
    }
}
```

### Example 4: Unbuffered Streaming for Server-Sent Events (SSE)
```nginx
server {
    listen 80;
    server_name telemetry.internal;

    location /stream/events {
        proxy_pass http://event_publisher;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        # CRITICAL: Disable buffering so events stream immediately to client!
        proxy_buffering off;
        proxy_cache off;
        
        # Disable gzip chunking interference
        gzip off;

        # Keep connection open for long-lived real-time streams
        proxy_read_timeout 24h;
        proxy_send_timeout 24h;
    }
}
```

### Example 5: Intercepting Upstream Errors with Custom Branded 502/504 Pages
```nginx
server {
    listen 80;
    server_name portal.example.com;

    location / {
        proxy_pass http://unstable_backend;
        
        # Intercept upstream 5xx errors so client does not see raw framework stacktraces
        proxy_intercept_errors on;
        error_page 502 503 504 /custom_gateway_error.html;
    }

    location = /custom_gateway_error.html {
        internal;
        root /var/www/static_errors;
    }
}
```

---

## 6. Best Practices

- **Always Explicitly Set `proxy_http_version 1.1;`**: NGINX defaults to HTTP/1.0 for upstream connections for historical reasons. HTTP/1.0 forces upstream backends to close the TCP socket after every request, destroying connection reuse and increasing latency.
- **Pair `proxy_http_version 1.1;` with `proxy_set_header Connection "";`**: To leverage `keepalive` connection pooling inside an `upstream` block, you must wipe the client's `Connection` header so NGINX can maintain persistent sockets.
- **Never Use Untrusted Variables in `proxy_pass` Without Resolvers**: If you configure `proxy_pass http://$target_host;`, NGINX cannot resolve the domain name at startup and requires an explicit `resolver` directive (e.g., `resolver 1.1.1.1;`). Furthermore, passing untrusted variables introduces severe Server-Side Request Forgery (SSRF) risks.
- **Tune `proxy_read_timeout` to Match SLA**: The default `60s` read timeout is too generous for standard APIs. If an upstream deadlocks, waiting 60 seconds ties up NGINX worker buffers and frustrates users. Set API timeouts to 5s–10s, reserving long timeouts strictly for background exports or WebSockets.
- **Use Standardized Snippets for Proxy Headers**: Include `/etc/nginx/snippets/proxy-params.conf` across all locations to prevent forgetting critical headers like `X-Forwarded-Proto`.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| Backend receives 404 due to unexpected trailing slash | `proxy_pass http://backend/;` stripped the path prefix, sending `/` instead of `/api/users`. | Remove the trailing slash: `proxy_pass http://backend;`. |
| Upstream connections constantly created and destroyed | Missing `proxy_http_version 1.1` and `proxy_set_header Connection ""`. | Add HTTP/1.1 and wipe `Connection` header to enable keepalive reuse. |
| Warning: `an upstream response is buffered to a temporary file` | Upstream response body exceeded in-memory `proxy_buffers`, forcing disk writes. | Increase `proxy_buffers` memory size (e.g. `16 64k`) or verify why responses are so large. |
| SSE / EventSource events delayed or received in batches | `proxy_buffering on;` holds chunked events in memory until the buffer fills. | Disable buffering on streaming routes: `proxy_buffering off;`. |
| Infinite redirect loop on HTTPS (`http` $\rightarrow$ `https` $\rightarrow$ `http`) | Backend framework does not know request was HTTPS because `X-Forwarded-Proto $scheme` was missing. | Forward proto header: `proxy_set_header X-Forwarded-Proto $scheme;`. |
| `502 Bad Gateway` error returned immediately | Backend application is not running, listening on wrong port, or firewall blocks connection. | Verify backend service state and test directly via `curl -I http://127.0.0.1:<port>`. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Enabling `keepalive` inside `upstream` blocks reduces latency by 30–50% by eliminating repetitive TCP 3-way handshakes and TLS negotiations between NGINX and backend pods.
- **Security**: The `proxy_pass` directive is the boundary between the public internet and private infrastructure. Never allow client-supplied headers or parameters to construct the `proxy_pass` hostname without validation to prevent **Server-Side Request Forgery (SSRF)**.
- **Cost**: Proxy buffering shields backend application processes (e.g. Node, Python, Ruby, PHP-FPM) from slow clients. By offloading I/O waits to NGINX, backend application servers can handle 3x to 5x more throughput with the exact same CPU/RAM footprint.

---

## 9. Comparison Table

| Attribute | Proxy Buffering ENABLED (`on`) | Proxy Buffering DISABLED (`off`) |
| :--- | :--- | :--- |
| **Backend Offloading** | **Maximum**: Backend finishes in milliseconds | **None**: Backend held open until client finishes |
| **Client Latency (TTFB)** | Slightly higher (waits for initial buffer fill) | **Instantaneous**: Bytes stream as generated |
| **Memory Footprint** | Consumes `proxy_buffers` in RAM per request | Minimal (only socket buffer) |
| **Disk Spillover** | Spills to `/var/cache/nginx/proxy_temp` if huge | Never touches disk |
| **Best Used For** | Standard REST APIs, HTML rendering, file downloads| WebSockets, Server-Sent Events (SSE), gRPC |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain in detail how a trailing slash on `proxy_pass` affects the request URI received by the upstream backend.</strong></summary>

**Answer**:
The presence or absence of a URI component (including a trailing slash) on the `proxy_pass` target URL changes NGINX's path handling behavior completely:
- **Case 1 (NO URI Component on proxy_pass)**:
  `location /api/ { proxy_pass http://backend; }`
  When `proxy_pass` has no URI component (no path after hostname/port), NGINX passes the incoming request URI to the backend completely unmodified. If the client requests `/api/v1/users`, the backend receives `/api/v1/users`.
- **Case 2 (URI Component Present on proxy_pass)**:
  `location /api/ { proxy_pass http://backend/; }` (or `http://backend/v2/`)
  When a URI component is present (even a single trailing slash `/`), NGINX **substitutes the portion of the URI that matched the location** with the URI specified in the `proxy_pass` directive.
  If the client requests `/api/v1/users`, NGINX strips the matched `/api/` prefix and appends `/v1/users` to `/`. The backend receives `/v1/users`.
</details>

<details>
<summary><strong>2. What is the difference between passing `$host`, `$http_host`, and `$proxy_host` in the `Host` proxy header?</strong></summary>

**Answer**:
- **`$proxy_host`**: Default behavior if no `proxy_set_header Host` is declared. Sets the `Host` header to the name and port of the upstream specified in `proxy_pass` (e.g., `backend_pool` or `127.0.0.1:8080`). This breaks virtual hosting on backend applications that expect the real domain name.
- **`$http_host`**: Passes the exact, unvalidated string sent by the client in the `Host` request header, including ports (e.g., `api.example.com:8080`).
- **`$host`**: Evaluated in order: host from request line, or host from client `Host` header (lowercased, port stripped), or the `server_name` of the matching server block if `Host` was missing.

*Best Practice*: Always use `proxy_set_header Host $host;` to guarantee a valid, sanitized hostname is forwarded.
</details>

<details>
<summary><strong>3. Why is `keepalive` inside an `upstream` block useless unless specific proxy headers are set in the `location` block?</strong></summary>

**Answer**:
By default, NGINX uses HTTP/1.0 for upstream requests and sends `Connection: close`. HTTP/1.0 does not support persistent connections without special headers, forcing the backend server to close the TCP socket immediately upon sending the response.

To make the `keepalive` directive inside an `upstream {}` block actually work, you must configure two directives in the `location` context:
1. `proxy_http_version 1.1;`: Switches upstream protocol to HTTP/1.1 (which supports persistent connections by default).
2. `proxy_set_header Connection "";`: Wipes the `Connection: close` header (or client's `Connection` header), allowing the socket to remain open in the NGINX upstream keepalive pool.
</details>

<details>
<summary><strong>4. How does `proxy_buffering` protect backend applications from Slowloris attacks?</strong></summary>

**Answer**:
A Slowloris or slow-client attack works by opening connections to an application and trickling request data or reading response data one byte every few seconds, tying up backend worker threads until the application thread pool is completely exhausted.

When `proxy_buffering on;` is enabled:
1. NGINX reads the entire client request body before contacting the upstream backend.
2. The upstream processes the request and responds at maximum LAN/memory speed.
3. NGINX reads the entire response into its memory/disk buffers and closes the backend upstream transaction immediately (freeing the application thread in milliseconds).
4. NGINX handles the slow client over its asynchronous `epoll` event loop, which can manage tens of thousands of slow connections without tying up backend application threads.
</details>

<details>
<summary><strong>5. What causes a `502 Bad Gateway` vs a `504 Gateway Timeout` in an NGINX reverse proxy?</strong></summary>

**Answer**:
- **502 Bad Gateway**: NGINX attempted to connect to the upstream server, but the connection was actively refused (`ECONNREFUSED`), the upstream crashed and closed the socket prematurely (`ECONNRESET`), or the upstream returned an invalid/corrupt HTTP response header. (Indicates backend is down, dead, or misconfigured).
- **504 Gateway Timeout**: NGINX successfully established a TCP connection to the upstream, but the upstream failed to return response bytes within the configured `proxy_read_timeout` window (defaults to 60 seconds). (Indicates backend is overloaded, deadlocked on a database lock, or running an excessively slow query).
</details>

<details>
<summary><strong>6. Explain the purpose and operation of `proxy_next_upstream`.</strong></summary>

**Answer**:
The `proxy_next_upstream` directive defines under what conditions NGINX should automatically fail over and retry a client request against the next available server in the `upstream` pool.

Valid conditions include: `error` (connection failure), `timeout`, `invalid_header`, `http_500`, `http_502`, `http_503`, and `http_504`.

*Operational Caution*: By default, NGINX only retries **idempotent** requests (`GET`, `HEAD`). It will **not** retry non-idempotent requests (`POST`, `LOCK`) if data was already transmitted to an upstream server to prevent duplicate charges or double database insertions, unless `non_idempotent` is explicitly specified.
</details>

<details>
<summary><strong>7. What happens if a response body exceeds `proxy_buffers` and `proxy_max_temp_file_size` is set to `0`?</strong></summary>

**Answer**:
If `proxy_max_temp_file_size 0;` is configured, NGINX completely prohibits writing upstream response data to temporary disk files.

If an upstream sends a response body larger than the allocated in-memory `proxy_buffers`:
NGINX fills the available RAM buffers and is forced to stop reading from the upstream socket. The upstream backend thread stalls and is blocked until the client consumes bytes over the network, effectively degenerating into unbuffered synchronous proxying for the remainder of that request.
</details>

<details>
<summary><strong>8. What is the security risk of using an unvalidated variable inside `proxy_pass`?</strong></summary>

**Answer**:
If you configure:
```nginx
location /proxy/ {
    proxy_pass http://$arg_target;
}
```
An attacker can pass `?target=169.254.169.254` (the AWS EC2 instance metadata service) or `?target=10.0.0.5:6379` (internal Redis). NGINX will execute an internal network request to the specified target and return the response to the attacker. This is a critical **Server-Side Request Forgery (SSRF)** vulnerability that allows remote attackers to steal cloud IAM credentials or compromise internal unauthenticated microservices.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Predict Backend URI Path</strong></summary>

**Task**: A client requests `GET /services/auth/token`. What exact URI does the backend receive under each configuration?
1. `location /services/ { proxy_pass http://127.0.0.1:8080; }`
2. `location /services/ { proxy_pass http://127.0.0.1:8080/; }`
3. `location /services/ { proxy_pass http://127.0.0.1:8080/v1/; }`

**Solution**:
1. `/services/auth/token` (No trailing slash: full URI passed).
2. `/auth/token` (Trailing slash: strips `/services/` prefix).
3. `/v1/auth/token` (Substituted: replaces `/services/` with `/v1/`).
</details>

<details>
<summary><strong>Exercise 2 (Easy): Configure Upstream Keepalive Headers</strong></summary>

**Task**: Write the two required directives in a location block to enable HTTP/1.1 and keepalive connection pooling to an upstream cluster.

**Solution**:
```nginx
location / {
    proxy_pass http://upstream_pool;
    proxy_http_version 1.1;
    proxy_set_header Connection "";
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Disable Buffering Strictly for SSE Streams</strong></summary>

**Task**: Author an NGINX configuration that proxies all requests to `http://backend`, but specifically disables buffering and extends read timeouts to 24 hours for `/api/stream`.

**Solution**:
```nginx
server {
    listen 80;
    server_name app.local;

    location / {
        proxy_pass http://backend;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
    }

    location ^~ /api/stream {
        proxy_pass http://backend;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        
        # Streaming optimizations
        proxy_buffering off;
        proxy_cache off;
        proxy_read_timeout 24h;
        proxy_send_timeout 24h;
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Handle Upstream Failover on 502/503</strong></summary>

**Task**: Configure an upstream pool of two servers with active retry logic so that if the primary returns an HTTP 502 or 503, NGINX automatically retries the secondary within a 2-second timeout.

**Solution**:
```nginx
upstream resilient_pool {
    server 10.0.1.10:8080 max_fails=2 fail_timeout=10s;
    server 10.0.1.11:8080 max_fails=2 fail_timeout=10s;
}

server {
    listen 80;
    server_name api.local;

    location / {
        proxy_pass http://resilient_pool;
        proxy_connect_timeout 2s;
        proxy_next_upstream error timeout http_502 http_503;
        proxy_next_upstream_tries 2;
        proxy_next_upstream_timeout 5s;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Standardize Microservice Ingress Header Gateway</strong></summary>

**Task**: Author a reusable snippet file `/etc/nginx/snippets/proxy_headers.conf` that sets `Host`, `X-Real-IP`, `X-Forwarded-For`, `X-Forwarded-Proto`, and a custom request tracing ID header `$request_id`.

**Solution**:
```nginx
# /etc/nginx/snippets/proxy_headers.conf
proxy_set_header Host $host;
proxy_set_header X-Real-IP $remote_addr;
proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
proxy_set_header X-Forwarded-Proto $scheme;
proxy_set_header X-Forwarded-Host $host;
proxy_set_header X-Forwarded-Port $server_port;

# Distributed Tracing Correlation ID
proxy_set_header X-Request-ID $request_id;
add_header X-Request-ID $request_id always;
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Construct Dual-Protocol Web & WebSocket Gateway</strong></summary>

**Task**: Configure a reverse proxy block that handles standard REST traffic to Node.js, while properly passing WebSocket upgrade requests for `/socket.io/` without dropping connections.

**Solution**:
```nginx
# Map connection upgrade
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

upstream node_app {
    server 127.0.0.1:3000;
    keepalive 32;
}

server {
    listen 80;
    server_name chat.internal;

    # Standard API
    location / {
        proxy_pass http://node_app;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host $host;
    }

    # WebSocket Realtime Gateway
    location ^~ /socket.io/ {
        proxy_pass http://node_app;
        proxy_http_version 1.1;
        
        # Upgrade negotiation
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $connection_upgrade;
        
        proxy_set_header Host $host;
        proxy_read_timeout 3600s;
        proxy_send_timeout 3600s;
    }
}
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Directive / Header | Syntax Example | Key Effect |
| :--- | :--- | :--- |
| **`proxy_pass`** | `proxy_pass http://backend;` | Forwards request to upstream |
| **Trailing Slash Rule** | `proxy_pass http://backend/;` | Strips matching location prefix |
| **HTTP Version** | `proxy_http_version 1.1;` | Enables HTTP/1.1 for upstreams |
| **Wipe Connection** | `proxy_set_header Connection "";` | Enables upstream keepalive reuse |
| **Forward Client IP** | `proxy_set_header X-Real-IP $remote_addr;`| Passes true client IP |
| **Forward Proto** | `proxy_set_header X-Forwarded-Proto $scheme;`| Informs backend of HTTPS state |
| **Proxy Buffering** | `proxy_buffering on;` | Absorbs slow clients, offloads app |
| **Disable Buffering** | `proxy_buffering off;` | Required for SSE & WebSockets |
| **Read Timeout** | `proxy_read_timeout 15s;` | Fails with 504 if upstream hangs |

---

## 13. Further Reading & Official Documentation

- [Official NGINX Documentation: Module ngx_http_proxy_module](https://nginx.org/en/docs/http/ngx_http_proxy_module.html)
- [Official NGINX Documentation: HTTP Load Balancing and Keepalive](https://nginx.org/en/docs/http/load_balancing.html)
- [RFC 9110: HTTP Semantics - Forwarded and Proxied Headers](https://www.rfc-editor.org/rfc/rfc9110.html)
- [PortSwigger Web Security Academy: Server-Side Request Forgery (SSRF)](https://portswigger.net/web-security/ssrf)
- [Mozilla Developer Network: Server-Sent Events Architecture](https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events)

---

[<- 01-logging.md](./01-logging.md) | [Back to Module Overview](./README.md) | [03-tls-https.md ->](./03-tls-https.md)
