---
title: "Interview Prep: Reverse Proxying, Load Balancing & Upstreams"
description: "Master technical interview questions on NGINX reverse proxying, proxy_pass trailing slash mechanics, upstream keepalive pooling, load balancing algorithms, and buffer tuning."
module: "08-interview"
topic: "03-reverse-proxy-and-load-balancing-qa"
status: "completed"
tags: ["nginx", "interview", "reverse-proxy", "load-balancing", "upstreams", "proxy-pass", "keepalive", "qa"]
prerequisites: ["02-core-features/02-reverse-proxy.md", "03-traffic-management/02-load-balancing.md"]
estimated_read_time: "35 min"
---

# Interview Prep: Reverse Proxying, Load Balancing & Upstreams

```
+-------------------------------------------------------------------------------+
| The proxy_pass Trailing Slash Transformation Matrix                           |
|                                                                               |
|  Client Request: `GET /api/v1/users`                                          |
|                                                                               |
|  Case A: NO Trailing Slash on proxy_pass (Passes URI Untouched)               |
|  location /api/ {                                                             |
|      proxy_pass http://backend:3000;                                          |
|  }                                                                            |
|  ==> Forwarded to Upstream: `GET /api/v1/users`                              |
|                                                                               |
|  Case B: WITH Trailing Slash on proxy_pass (Strips Location Prefix)           |
|  location /api/ {                                                             |
|      proxy_pass http://backend:3000/;                                         |
|  }                                                                            |
|  ==> Forwarded to Upstream: `GET /v1/users`                                   |
|                                                                               |
|  Case C: WITH Subpath on proxy_pass (Replaces Location Prefix with Subpath)   |
|  location /api/ {                                                             |
|      proxy_pass http://backend:3000/internal/;                                |
|  }                                                                            |
|  ==> Forwarded to Upstream: `GET /internal/v1/users`                          |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [`proxy_pass` URI Manipulation & Header Forwarding](#1-proxy_pass-uri-manipulation--header-forwarding)
2. [Upstream Keepalive Pooling & Connection Longevity](#2-upstream-keepalive-pooling--connection-longevity)
3. [Load Balancing Algorithms & Traffic Distribution](#3-load-balancing-algorithms--traffic-distribution)
4. [Passive Health Checks & Circuit Breaking](#4-passive-health-checks--circuit-breaking)
5. [Upstream Buffers & Memory Mechanics](#5-upstream-buffers--memory-mechanics)
6. [Live Technical Scenarios & Troubleshooting Challenges](#6-live-technical-scenarios--troubleshooting-challenges)
7. [Summary & Quick Reference](#7-summary--quick-reference)

---

## 1. `proxy_pass` URI Manipulation & Header Forwarding

<details>
<summary><strong>Q1: What is the exact difference between `proxy_pass http://backend;` and `proxy_pass http://backend/;` (with a trailing slash)?</strong></summary>

**Model Answer:**
The presence of a URI path (even a single trailing slash) after the host in `proxy_pass` fundamentally changes how NGINX constructs the forwarded URI:
1. **Without Trailing Slash (`proxy_pass http://backend;`)**:
   - NGINX passes the incoming request URI **completely untouched** to the backend.
   - Example: With `location /api/`, a request to `/api/users` forwards as `/api/users`.
2. **With Trailing Slash or Subpath (`proxy_pass http://backend/;` or `http://backend/core;`)**:
   - NGINX performs URI transformation: it strips the matched `location` prefix from the incoming URI and appends the remainder to the URI path specified in `proxy_pass`.
   - Example: With `location /api/`, a request to `/api/users` strips `/api/` and appends `users` to `/`, forwarding as `/users`.

*Caveat*: If the `location` is defined using a regular expression (`location ~ \.php$`), adding a trailing slash or URI to `proxy_pass` is a **syntax error**; NGINX requires `rewrite ... break;` instead.
</details>

<details>
<summary><strong>Q2: Why does an upstream Node.js or Python backend report `Host: backend` or `127.0.0.1` unless headers are explicitly set?</strong></summary>

**Model Answer:**
By default, NGINX resets the `Host` request header to the name of the upstream server defined in the `proxy_pass` directive (`Host: backend` or `Host: 127.0.0.1`).
To preserve the original domain name requested by the client, you must explicitly set:
```nginx
proxy_set_header Host $host;
```
Additionally, to inform the backend of the real client IP and TLS protocol state, you must pass:
```nginx
proxy_set_header X-Real-IP $remote_addr;
proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
proxy_set_header X-Forwarded-Proto $scheme;
```
</details>

---

## 2. Upstream Keepalive Pooling & Connection Longevity

<details>
<summary><strong>Q3: Why is configuring `keepalive 32;` inside an `upstream` block insufficient on its own to enable persistent connections? What other directives are mandatory?</strong></summary>

**Model Answer:**
The `keepalive <N>;` directive inside `upstream {}` only defines the size of the in-memory pool of idle persistent connections. It does NOT automatically instruct NGINX to use persistent HTTP/1.1 connections when proxying!

To enable upstream connection reuse, you **must also configure two directives in the `location` block**:
```nginx
location / {
    proxy_pass http://backend_pool;
    proxy_http_version 1.1;
    proxy_set_header Connection "";
}
```
*Why?*
1. **`proxy_http_version 1.1;`**: By default, NGINX proxies using **HTTP/1.0**, which does not support persistent connections.
2. **`proxy_set_header Connection "";`**: NGINX forwards incoming client headers. If a client sends `Connection: close` (or default HTTP/1.0 close), NGINX forwards that to the backend, causing the backend to close the TCP connection immediately after responding. Clearing the header allows the persistent connection to remain open in the keepalive cache.
</details>

<details>
<summary><strong>Q4: What happens under high load if upstream keepalive is omitted?</strong></summary>

**Model Answer:**
Without upstream keepalives, NGINX opens a fresh TCP connection for every single incoming request and closes it immediately with a `FIN/ACK` handshake upon receiving the response.
- At 20,000 req/sec, the server rapidly exhausts the host operating system's ephemeral outbound port range (typically 28,000 to 60,000 ports).
- Closed sockets enter the kernel `TIME_WAIT` state for 60 seconds.
- The server runs out of ports, throwing `cannot assign requested address while connecting to upstream` and returning **HTTP 502 Bad Gateway** to clients.
</details>

---

## 3. Load Balancing Algorithms & Traffic Distribution

<details>
<summary><strong>Q5: Compare `round-robin`, `least_conn`, `ip_hash`, and `hash ... consistent`. When should each be used?</strong></summary>

**Model Answer:**
- **Round-Robin (Default)**: Distributes requests sequentially across upstream servers based on configured weights. Assumes all requests take roughly equal processing time.
- **`least_conn`**: Directs the request to the upstream server with the **lowest number of active connections**. Ideal for workloads where request durations vary widely (e.g. database reports vs lightweight health checks).
- **`ip_hash`**: Hashes the first 3 octets of an IPv4 address (or full IPv6) to guarantee that requests from the same client IP subnet consistently land on the same backend node. Used for stateful legacy applications requiring session stickiness.
- **`hash $key consistent` (Ketama Consistent Hashing)**: Uses consistent hashing (such as on a session cookie or URI parameter). If a backend node is added or removed from the upstream cluster, only a tiny fraction ($1/N$) of keys are remapped, preventing cache thrashing in upstream Memcached/Redis/API caches.
</details>

---

## 4. Passive Health Checks & Circuit Breaking

<details>
<summary><strong>Q6: How does passive health checking work in open-source NGINX using `max_fails` and `fail_timeout`?</strong></summary>

**Model Answer:**
Open-source NGINX uses **passive** health checking, meaning it evaluates the health of an upstream server only when a real client request is sent to it (unlike active probes in NGINX Plus):
```nginx
upstream backend_cluster {
    server 10.0.1.11:8080 max_fails=3 fail_timeout=10s;
    server 10.0.1.12:8080 max_fails=3 fail_timeout=10s;
}
```
1. If NGINX experiences 3 consecutive failed connection attempts or timeouts (`max_fails=3`) to `10.0.1.11` within a 10-second window (`fail_timeout=10s`), NGINX flags `10.0.1.11` as dead.
2. For the next 10 seconds, NGINX routes 0% of traffic to that server.
3. After 10 seconds elapses, NGINX sends a single probe request from the next incoming client. If that request succeeds, the server is marked healthy and returned to the pool.
</details>

<details>
<summary><strong>Q7: What is the role of `proxy_next_upstream` and why is it essential for zero-downtime rolling deployments?</strong></summary>

**Model Answer:**
The `proxy_next_upstream` directive defines which error conditions trigger NGINX to transparently retry a failed request on another server in the upstream pool:
```nginx
proxy_next_upstream error timeout http_502 http_503;
proxy_next_upstream_tries 3;
proxy_next_upstream_timeout 5s;
```
During a rolling deployment, when a backend instance is taken down, it may return a connection refusal or 502 error during restart. Instead of passing that 502 error to the user, NGINX catches it and immediately forwards the request to an alternate running backend instance within milliseconds, achieving 100% zero-downtime deployments.
</details>

---

## 5. Upstream Buffers & Memory Mechanics

<details>
<summary><strong>Q8: What is the difference between `proxy_buffering on;` and `proxy_buffering off;`? When must buffering be turned off?</strong></summary>

**Model Answer:**
- **`proxy_buffering on;` (Default)**:
  - NGINX reads the response from the upstream server as quickly as possible into memory buffers (`proxy_buffers`).
  - As soon as the entire response is received, NGINX closes the backend connection (freeing backend worker threads).
  - NGINX then streams the response to a slow client at the client's network speed.
- **`proxy_buffering off;`**:
  - NGINX acts as a transparent synchronous pipe, forwarding response bytes to the client the instant they arrive from the backend.

*When to Turn Buffering OFF*:
1. **Server-Sent Events (SSE) & LLM Token Streaming**: Buffering prevents tokens from reaching the browser until a 4KB/8KB buffer is full, destroying real-time interactivity.
2. **WebSockets**: Long-lived bidirectional messaging requires immediate frame transmission.
3. **Large File Uploads/Downloads**: When disk caching of multi-gigabyte files would cause excessive disk I/O thrashing.
</details>

---

## 6. Live Technical Scenarios & Troubleshooting Challenges

<details>
<summary><strong>Scenario 1: You have 3 backend nodes. Node 1 crashes. Users report occasional 502 Bad Gateway errors, even though Nodes 2 and 3 are healthy. How do you fix this in NGINX?</strong></summary>

**Diagnosis & Remediation:**
By default, when Node 1 crashes, NGINX routes a portion of requests to Node 1. Because `proxy_next_upstream` was not configured to catch `http_502` or connection errors, NGINX returns the failure directly to the client.
*Fix*:
1. In `upstream {}`, configure passive circuit breaking:
   ```nginx
   server 10.0.1.11:8080 max_fails=3 fail_timeout=10s;
   ```
2. In `location {}`, configure automatic retries and tight connect timeouts:
   ```nginx
   proxy_connect_timeout 2s;
   proxy_next_upstream error timeout http_502 http_503;
   proxy_next_upstream_tries 3;
   ```
This ensures that any request landing on Node 1 fails over to Node 2 or 3 in under 2 seconds without the client ever seeing an error.
</details>

<details>
<summary><strong>Scenario 2: An engineer added a trailing slash to `proxy_pass http://backend/;` on a location configured as `location /v1/api/`. The backend microservice immediately began returning 404 Not Found. Why?</strong></summary>

**Diagnosis:**
Before the trailing slash, a request to `/v1/api/users` forwarded to the backend as `GET /v1/api/users`.
Adding the trailing slash (`http://backend/;`) instructed NGINX to strip the matched location prefix (`/v1/api/`) and append the remainder to `/`.
As a result, the backend received `GET /users` instead of `GET /v1/api/users`. If the backend application router was configured to expect `/v1/api/users`, it threw an HTTP 404 Not Found.
*Fix*: Remove the trailing slash (`proxy_pass http://backend;`) to pass the URI unchanged.
</details>

---

## 7. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| Reverse Proxy & Upstream Rules of Thumb                                        |
|                                                                                |
| Directive                     | Mandatory Best Practice                        |
| :---------------------------- | :--------------------------------------------- |
| proxy_http_version 1.1;       | Required for upstream keepalive connection reuse|
| proxy_set_header Connection "";| Required to prevent closing persistent sockets |
| proxy_set_header Host $host;  | Preserves original client domain on backend    |
| least_conn;                   | Optimal for variable-duration API workloads    |
| proxy_next_upstream error..   | Eliminates user-visible 502s during deploys    |
| proxy_buffering off;          | Mandatory for Server-Sent Events (SSE) & LLMs  |
| Trailing slash on proxy_pass  | Strips location prefix from forwarded URI      |
+--------------------------------------------------------------------------------+
```

---

[← Previous Topic: 02-location-matching-and-rewrites-qa.md](02-location-matching-and-rewrites-qa.md) | [Back to Module Index](README.md) | [Next Topic: 04-tls-and-security-qa.md →](04-tls-and-security-qa.md)
