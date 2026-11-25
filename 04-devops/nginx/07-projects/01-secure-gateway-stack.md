---
title: "Project 01: Secure Edge Gateway Stack"
description: "Production reference implementation of a hardened multi-domain reverse proxy gateway featuring TLS 1.3 termination, tiered rate limiting, microcaching, least_conn load balancing, and automated zero-trust catch-alls."
module: "07-projects"
project_id: "01-secure-gateway-stack"
status: "completed"
tags: ["nginx", "project", "gateway", "reverse-proxy", "tls13", "rate-limiting", "microcaching", "production"]
prerequisites: ["05-security-performance/02-security-hardening.md", "03-traffic-management/02-load-balancing.md"]
estimated_read_time: "35 min"
---

# Project 01: Secure Edge Gateway Stack

```
+-------------------------------------------------------------------------------+
| Secure Edge Gateway Architecture                                              |
|                                                                               |
|  [Internet Clients / Public Traffic]                                          |
|        │                                                                      |
|        ▼                                                                      |
|  [TCP Port 80 & 443 Ingress]                                                  |
|        │                                                                      |
|        ├── Unknown Hosts / Direct IP Scans --> [Catch-All Default Server]     |
|        │                                       return 444; (Instant Drop)     |
|        │                                                                      |
|        ├── http://*                        --> [Global Port 80 Redirect]      |
|        │                                       return 301 https://$host...    |
|        │                                                                      |
|        └── https://api.example.com         --> [Public API Gateway]           |
|            │                                   TLS 1.3, Rate limits,          |
|            │                                   least_conn load balancing      |
|            ├── /v1/public/*                --> [Microcache: 1s buffer]        |
|            └── /v1/checkout/*              --> [Strict Rate Limit: 5r/s]      |
|                                                │                              |
|                                                ▼                              |
|                                       [3x Upstream Nodes]                     |
|                                       10.0.1.11, 10.0.1.12, 10.0.1.13:3000   |
|                                       keepalive 64; proxy_http_version 1.1;   |
+-------------------------------------------------------------------------------+
```

---

## 1. Architecture & Objective

### Objective
Design and implement an enterprise-grade, single-instance NGINX edge gateway that acts as the hardened front door for public API traffic, administration consoles, and static documentation services.

### Core Architectural Requirements
1. **Multi-Domain Ingress**: Route `api.example.com`, `admin.example.com`, and `docs.example.com` on ports 80/443 using SNI.
2. **Defensive Catch-All**: Drop all unauthorized Host headers and raw IP address scans instantly with HTTP 444.
3. **Transport Security**: Enforce TLS 1.3 and TLS 1.2 with strict cipher suites, HSTS preloading, and complete HTTP-to-HTTPS redirection.
4. **Resilient Load Balancing**: Distribute API traffic across three upstream nodes using the `least_conn` algorithm with active keepalive connection pools and passive circuit breaking.
5. **Tiered Rate Limiting**: Apply baseline rate limiting (30 req/sec) globally, while enforcing tight leaky-bucket rate limiting (3 req/sec) on sensitive checkout and authentication endpoints.
6. **Microcaching Layer**: Accelerate high-throughput public catalog routes with a 1-second dynamic microcache to eliminate backend cache stampedes.

---

## 2. Complete Config Code

### 1. Main Global Configuration (`/etc/nginx/nginx.conf`)

```nginx
user www-data;
pid /run/nginx.pid;
worker_processes auto;
worker_cpu_affinity auto;
worker_rlimit_nofile 1048576;

error_log /var/log/nginx/error.log warn;

events {
    worker_connections 65535;
    use epoll;
    multi_accept on;
}

http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    # Performance & File I/O
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;
    open_file_cache max=50000 inactive=60s;
    open_file_cache_valid 120s;

    # Timeouts
    keepalive_timeout 65s;
    keepalive_requests 10000;
    client_header_timeout 10s;
    client_body_timeout 10s;
    send_timeout 10s;
    reset_timedout_connection on;

    # Buffer Limits
    client_max_body_size 10m;
    client_body_buffer_size 128k;
    client_header_buffer_size 1k;
    large_client_header_buffers 4 8k;

    # Structured JSON Logging
    log_format json_main escape=json '{'
        '"timestamp":"$time_iso8601",'
        '"client_ip":"$remote_addr",'
        '"host":"$host",'
        '"request":"$request",'
        '"status":$status,'
        '"bytes_sent":$body_bytes_sent,'
        '"request_time":$request_time,'
        '"upstream_addr":"$upstream_addr",'
        '"upstream_response_time":"$upstream_response_time",'
        '"user_agent":"$http_user_agent"'
    '}';

    access_log /var/log/nginx/access.log json_main buffer=512k flush=5s;

    # Shared Memory Zones for Rate Limiting
    limit_conn_zone $binary_remote_addr zone=addr_cap:20m;
    limit_req_zone $binary_remote_addr zone=api_general:20m rate=30r/s;
    limit_req_zone $binary_remote_addr zone=api_sensitive:20m rate=3r/s;
    limit_req_status 429;
    limit_conn_status 429;

    # Shared Memory Zone for Microcaching
    proxy_cache_path /var/cache/nginx/microcache
                     levels=1:2
                     keys_zone=micro_zone:20m
                     max_size=1g
                     inactive=10m
                     use_temp_path=off;

    # Security Tokens
    server_tokens off;

    # Include modular configurations
    include /etc/nginx/conf.d/*.conf;
    include /etc/nginx/sites-enabled/*.conf;
}
```

---

### 2. Common Security Headers (`/etc/nginx/snippets/security-headers.conf`)

```nginx
add_header X-Content-Type-Options "nosniff" always;
add_header X-Frame-Options "DENY" always;
add_header Referrer-Policy "strict-origin-when-cross-origin" always;
add_header Permissions-Policy "camera=(), microphone=(), geolocation=()" always;
add_header Content-Security-Policy "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none';" always;
add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
```

---

### 3. Catch-All Default Server (`/etc/nginx/conf.d/00-default.conf`)

```nginx
# Drop unknown HTTP hosts
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name _;
    return 444;
}

# Drop unknown HTTPS hosts
server {
    listen 443 ssl default_server;
    listen [::]:443 ssl default_server;
    server_name _;

    ssl_certificate /etc/nginx/ssl/dummy-fallback.crt;
    ssl_certificate_key /etc/nginx/ssl/dummy-fallback.key;
    ssl_protocols TLSv1.2 TLSv1.3;

    return 444;
}
```

---

### 4. Public API Virtual Host (`/etc/nginx/sites-enabled/api.example.com.conf`)

```nginx
upstream api_cluster {
    least_conn;
    server 10.0.1.11:3000 max_fails=3 fail_timeout=10s;
    server 10.0.1.12:3000 max_fails=3 fail_timeout=10s;
    server 10.0.1.13:3000 max_fails=3 fail_timeout=10s;

    keepalive 64;
    keepalive_requests 1000;
    keepalive_timeout 60s;
}

# Canonical HTTP to HTTPS Redirect
server {
    listen 80;
    server_name api.example.com;
    return 301 https://$host$request_uri;
}

server {
    listen 443 ssl;
    http2 on;
    server_name api.example.com;

    # TLS 1.3 / 1.2 Configuration
    ssl_certificate /etc/letsencrypt/live/api.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/api.example.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384;
    ssl_prefer_server_ciphers off;
    ssl_session_cache shared:SSL:20m;
    ssl_session_timeout 1d;
    ssl_session_tickets off;

    # Global Connection and Rate Caps for this host
    limit_conn addr_cap 30;
    limit_req zone=api_general burst=50 nodelay;

    # Include Base Security Headers
    include /etc/nginx/snippets/security-headers.conf;

    # General API Proxying
    location / {
        proxy_pass http://api_cluster;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto https;

        proxy_hide_header X-Powered-By;
        proxy_next_upstream error timeout http_502 http_503;

        include /etc/nginx/snippets/security-headers.conf;
    }

    # Microcached Catalog Endpoints
    location /v1/catalog {
        proxy_pass http://api_cluster;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        proxy_cache micro_zone;
        proxy_cache_valid 200 302 1s;
        proxy_cache_valid 404 10s;
        proxy_cache_use_stale error timeout updating http_502 http_503 http_504;
        proxy_cache_lock on;

        add_header X-Cache-Status $upstream_cache_status always;
        include /etc/nginx/snippets/security-headers.conf;
    }

    # Stricter Rate-Limited Sensitive Routes
    location ~* /v1/(checkout|auth|login) {
        limit_req zone=api_sensitive burst=5 nodelay;

        proxy_pass http://api_cluster;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

        include /etc/nginx/snippets/security-headers.conf;
    }
}
```

---

## 3. Step-by-Step Walkthrough

1. **Defensive Catch-All (`00-default.conf`)**:
   - The `default_server` listens on both port 80 and 443 with `server_name _`.
   - Any client reaching the server via its raw IP address (e.g. `https://198.51.100.20/`) or with a bogus `Host: malicious.com` header immediately executes `return 444;`.
   - NGINX sends a TCP RST packet and drops the socket without returning an HTTP payload, preserving server anonymity.

2. **Upstream Keepalive Optimization (`api_cluster`)**:
   - `least_conn` directs requests to the node with the fewest active connections.
   - `keepalive 64;` retains a warm pool of 64 persistent idle TCP connections per worker process.
   - `proxy_http_version 1.1;` and `proxy_set_header Connection "";` strip the default `Connection: close` header, maintaining persistent upstream sockets.

3. **Microcaching Mechanism (`/v1/catalog`)**:
   - `proxy_cache_valid 200 302 1s;` caches dynamic responses for exactly 1 second.
   - `proxy_cache_lock on;` prevents cache stampedes: if 10,000 requests arrive simultaneously during an expiration second, only 1 request queries the backend while 9,999 wait for the cache to fill.
   - `proxy_cache_use_stale updating;` serves the stale 1-second-old payload while the background fetch completes, ensuring zero latency spikes.

4. **Security Header Re-inclusion**:
   - Notice that `include /etc/nginx/snippets/security-headers.conf;` is called inside both the `server` block and the individual `location` blocks. This ensures that custom location-level headers (`X-Cache-Status`) do not trigger NGINX's array inheritance trap and wipe out HSTS or CSP headers!

---

## 4. Verification & Testing

### Test 1: Verify Catch-All Drops Malicious Host Headers
```bash
curl -v -H "Host: attacker.com" https://127.0.0.1/ -k
```
*Expected Output*:
```text
* Empty reply from server
* Connection reset by peer
```

### Test 2: Verify TLS 1.3 & Security Headers
```bash
curl -sI https://api.example.com --resolve api.example.com:443:127.0.0.1
```
*Expected Output*:
```http
HTTP/2 200
server: nginx
strict-transport-security: max-age=63072000; includeSubDomains; preload
x-content-type-options: nosniff
x-frame-options: DENY
content-security-policy: default-src 'self'; ...
```

### Test 3: Verify Microcache Cache Hits
```bash
# Request 1: Triggers backend fetch (MISS)
curl -sI https://api.example.com/v1/catalog | grep X-Cache-Status
# Output: X-Cache-Status: MISS

# Request 2 (Within 1 second): Served from memory cache (HIT)
curl -sI https://api.example.com/v1/catalog | grep X-Cache-Status
# Output: X-Cache-Status: HIT
```

### Test 4: Verify Sensitive Endpoint Rate Limiting
```bash
for i in {1..10}; do
    curl -s -o /dev/null -w "%{http_code}\n" https://api.example.com/v1/login
done
```
*Expected Output*:
```text
200
200
200
200
200
200
429
429
429
429
```

---

## 5. Troubleshooting & Edge Cases

### Edge Case 1: Backend Fails with `502 Bad Gateway`
- **Cause**: The application on `10.0.1.11` is offline or firewall rules block port 3000.
- **Triage**: Check `/var/log/nginx/error.log` for `(111: Connection refused)`. Verify `proxy_next_upstream` automatically re-routes traffic to `10.0.1.12` without user impact.

### Edge Case 2: Memory Zone Depletion During Distributed Attacks
- **Cause**: Botnet sends requests from 500,000 unique IP addresses, exceeding the 20MB `api_general` zone.
- **Triage**: Check error log for `could not allocate new entry in limit_req_zone`.
- **Fix**: Increase zone size to `50m` in `nginx.conf`, or offload persistent attackers to kernel firewalls via `fail2ban` and `ipset`.

---

[← Previous Note: Module 06 Overview](../06-production/README.md) | [Back to Module Index](README.md) | [Next Project: 02-spa-with-api-proxy.md →](02-spa-with-api-proxy.md)
