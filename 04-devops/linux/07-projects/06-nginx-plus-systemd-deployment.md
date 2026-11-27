# Project 06: Production Nginx Reverse Proxy & Systemd Automation

| Metadata | Details |
| :--- | :--- |
| **Project** | Production Nginx Reverse Proxy & Systemd Automation |
| **Module** | `07-projects` — Production Reference Projects & Labs |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS, RHEL 9.4 / Rocky Linux 9.4 |
| **Complexity** | Intermediate |
| **Estimated Duration**| 45–60 minutes |

---

## 1. Architecture & Design

### Scenario Context
Serving web applications directly to public clients exposes backend application runtimes (Node.js, Python FastAPI, Java Spring Boot) to slow-client attacks, unoptimized TLS handshakes, large static asset loads, and lack of connection pooling.

A production-grade reverse proxy layer decouples backend microservices from edge client connections. This project delivers an automated, production-hardened Nginx reverse proxy architecture featuring:
1. **Zero-Downtime Reloads**: Validated systemd reloads (`nginx -t && systemctl reload nginx`).
2. **Modern TLS 1.3/1.2 Security**: Mozilla Intermediate SSL profile, HSTS, OCSP Stapling, and ECDHE ciphers.
3. **Upstream Keepalive Pooling**: Reduces TCP handshake latency to backend microservice clusters.
4. **Rate Limiting & DDoS Mitigation**: Dual-zone rate limiting for IP protection and brute-force prevention.

### Architectural Ingress Diagram
```
+-------------------------------------------------------------------------+
|                  Nginx Production Reverse Proxy Ingress                 |
|                                                                         |
|  [ Public Internet Clients ]                                            |
|  (HTTP/1.1, HTTP/2, TLS 1.3 over TCP Port 443 / Port 80)                |
|                                  |                                      |
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  | NGINX EDGE REVERSE PROXY                                          |  |
|  |                                                                   |  |
|  | 1. Perimeter Defense & Rate Limiting:                            |  |
|  |    - limit_req_zone $binary_remote_addr (10 req/s, burst 20)      |  |
|  |    - limit_conn_zone $binary_remote_addr (20 conns per IP)        |  |
|  |                                                                   |  |
|  | 2. TLS Termination & Security Headers:                            |  |
|  |    - Strict-Transport-Security (HSTS), X-Frame-Options            |  |
|  |    - OCSP Stapling enabled, TLS 1.3 0-RTT Anti-Replay             |  |
|  |                                                                   |  |
|  | 3. Static Asset Caching:                                         |  |
|  |    - Direct VFS sendfile bypass for static CSS/JS/Images          |  |
|  |                                                                   |  |
|  | 4. Upstream Connection Pooling:                                  |  |
|  |    - HTTP/1.1 keepalive 64 (Reuses backend TCP sockets)          |  |
|  +-------------------------------------------------------------------+  |
|                                  | Private VPC / Localhost              |
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  | BACKEND UPSTREAM CLUSTER (127.0.0.1:3000, 127.0.0.1:3001)        |  |
|  | - Node.js / NestJS / FastAPI Application Instances                 |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 2. Implementation Artifacts

### Artifact 1: Production Hardened Nginx Configuration (`/etc/nginx/nginx.conf`)

```nginx
# /etc/nginx/nginx.conf
user www-data;
worker_processes auto;
worker_rlimit_nofile 65535;
pid /run/nginx.pid;
include /etc/nginx/modules-enabled/*.conf;

events {
    worker_connections 8192;
    use epoll;
    multi_accept on;
}

http {
    # --- Basic Optimizations ---
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;
    types_hash_max_size 2048;
    server_tokens off;

    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    # --- Connection Timeouts ---
    client_body_timeout 15;
    client_header_timeout 15;
    keepalive_timeout 65;
    send_timeout 15;

    # --- Buffer Limits ---
    client_body_buffer_size 128k;
    client_max_body_size 20M;
    client_header_buffer_size 1k;
    large_client_header_buffers 4 8k;

    # --- Logging Format (Extended with Upstream & Request Latency) ---
    log_format production_json escape=json
      '{"time_local":"$time_local",'
      '"client_ip":"$remote_addr",'
      '"request_method":"$request_method",'
      '"request_uri":"$request_uri",'
      '"status":$status,'
      '"body_bytes_sent":$body_bytes_sent,'
      '"request_time":$request_time,'
      '"upstream_response_time":"$upstream_response_time",'
      '"upstream_addr":"$upstream_addr",'
      '"http_user_agent":"$http_user_agent"}';

    access_log /var/log/nginx/access.log production_json;
    error_log /var/log/nginx/error.log warn;

    # --- Gzip Compression ---
    gzip on;
    gzip_vary on;
    gzip_proxied any;
    gzip_comp_level 5;
    gzip_min_length 256;
    gzip_types
        text/plain
        text/css
        text/javascript
        application/json
        application/javascript
        application/x-javascript
        text/xml
        application/xml
        image/svg+xml;

    # --- Rate Limiting Zones ---
    # 10MB memory zone holds ~160,000 unique IP addresses
    limit_req_zone $binary_remote_addr zone=api_ip_limit:10m rate=10r/s;
    limit_conn_zone $binary_remote_addr zone=conn_ip_limit:10m;
    limit_req_status 429;
    limit_conn_status 429;

    # Include Virtual Hosts
    include /etc/nginx/conf.d/*.conf;
    include /etc/nginx/sites-enabled/*;
}
```

### Artifact 2: Production Virtual Host Reverse Proxy (`/etc/nginx/sites-available/app-gateway.conf`)

```nginx
# Upstream Application Cluster
upstream api_backend {
    server 127.0.0.1:3000 max_fails=3 fail_timeout=10s;
    server 127.0.0.1:3001 max_fails=3 fail_timeout=10s;
    
    # Maintain 64 idle keepalive connections to upstream servers
    keepalive 64;
}

# Redirect HTTP to HTTPS
server {
    listen 80;
    listen [::]:80;
    server_name api.example.com;

    location ^~ /.well-known/acme-challenge/ {
        root /var/www/certbot;
    }

    location / {
        return 301 https://$host$request_uri;
    }
}

# Primary HTTPS Reverse Proxy
server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name api.example.com;

    # --- TLS Certificates & Hardening ---
    ssl_certificate /etc/ssl/certs/api.example.com.crt;
    ssl_certificate_key /etc/ssl/private/api.example.com.key;
    ssl_session_timeout 1d;
    ssl_session_cache shared:SSL:10m;
    ssl_session_tickets off;

    # Modern TLS 1.2 / 1.3 Protocols & Ciphers (Mozilla Intermediate)
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384;
    ssl_prefer_server_ciphers off;

    # OCSP Stapling
    ssl_stapling on;
    ssl_stapling_verify on;
    resolver 1.1.1.1 8.8.8.8 valid=300s;
    resolver_timeout 5s;

    # --- Security Headers ---
    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
    add_header X-Frame-Options "DENY" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;
    add_header Content-Security-Policy "default-src 'none'; frame-ancestors 'none';" always;

    # --- Global Rate Limiting Limits ---
    limit_conn conn_ip_limit 20;

    # --- Reverse Proxy Route ---
    location / {
        limit_req zone=api_ip_limit burst=20 nodelay;

        proxy_pass http://api_backend;
        proxy_http_version 1.1;
        
        # Clear Connection header to enable keepalive pooling
        proxy_set_header Connection "";

        # Preserve Client IP & Protocol headers
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # Upstream Buffering & Timeouts
        proxy_buffering on;
        proxy_buffer_size 8k;
        proxy_buffers 16 8k;
        proxy_connect_timeout 5s;
        proxy_read_timeout 30s;
        proxy_send_timeout 10s;

        # Graceful Interception of Backend Errors
        proxy_next_upstream error timeout invalid_header http_500 http_502 http_503 http_504;
    }
}
```

### Artifact 3: Automated Zero-Downtime Reload Wrapper (`nginx-reload.sh`)

```bash
#!/usr/bin/env bash
# ==============================================================================
# Script: nginx-reload.sh
# Purpose: Validated zero-downtime Nginx reload
# ==============================================================================

set -euo pipefail

log() { echo -e "\033[1;32m[+] $*\033[0m"; }
error() { echo -e "\033[1;31m[-] $*\033[0m" >&2; exit 1; }

log "Validating Nginx syntax..."
if ! nginx -t 2>/tmp/nginx_syntax.log; then
    cat /tmp/nginx_syntax.log >&2
    error "Syntax check failed! Reload aborted. Running configuration untouched."
fi

log "Syntax check passed. Sending SIGHUP reload signal via systemd..."
systemctl reload nginx

log "Reload complete! Active master process PID:"
pgrep -f "nginx: master"
```

---

## 3. Execution & Verification

### Step-by-Step Test Procedure

1. **Deploy Configurations and Generate Self-Signed TLS Certificate**:
   ```bash
   sudo mkdir -p /etc/ssl/certs /etc/ssl/private
   sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
     -keyout /etc/ssl/private/api.example.com.key \
     -out /etc/ssl/certs/api.example.com.crt \
     -subj "/CN=api.example.com"
   ```

2. **Validate Syntax & Reload**:
   ```bash
   sudo chmod +x ./nginx-reload.sh
   sudo ./nginx-reload.sh
   ```

3. **Simulated Verification Output**:
   ```text
   [+] Validating Nginx syntax...
   nginx: the configuration file /etc/nginx/nginx.conf syntax is ok
   nginx: configuration file /etc/nginx/nginx.conf test is successful
   [+] Syntax check passed. Sending SIGHUP reload signal via systemd...
   [+] Reload complete! Active master process PID:
   12450
   ```

4. **Test Rate Limiting Enforcement**:
   Send a high-frequency burst of 30 requests:
   ```bash
   for i in {1..30}; do
       curl -k -s -o /dev/null -w "%{http_code}\n" https://localhost/
   done | sort | uniq -c
   ```
   *Expected Output:*
   ```text
      20 200
      10 429
   ```
   *(Notice requests 21 through 30 receive HTTP 429 Too Many Requests as intended).*

5. **Verify Upstream Keepalive Pooling**:
   ```bash
   sudo ss -tnp | grep 3000
   # Connections remain in ESTAB state across separate HTTP requests!
   ```

---

## 4. Failure Modes & Edge Cases

| Failure Mode / Incident | Root Cause | Remediation Runbook |
| :--- | :--- | :--- |
| **`HTTP 502 Bad Gateway`** | Upstream backend process crashed, not listening on port, or firewall blocked loopback. | Verify backend service status (`systemctl status myapp`) and port binding (`ss -tulpn \| grep 3000`). |
| **`HTTP 504 Gateway Timeout`** | Upstream backend took longer to respond than `proxy_read_timeout` (30s). | Check backend database query performance or increase `proxy_read_timeout` for slow endpoints. |
| **TLS Certificate Expired** | Certbot renewal timer failed or Let's Encrypt challenge blocked. | Run `certbot renew --dry-run` and inspect `/var/log/letsencrypt/letsencrypt.log`. |
| **Worker Process OOM Crash** | `client_max_body_size` set too large or memory buffering unbounded. | Restrict `client_body_buffer_size` to 128k; enforce `client_max_body_size 20M`. |

---

## 5. Production Considerations

- **Logrotate Configuration**: Ensure `/etc/logrotate.d/nginx` sends `USR1` signal to Nginx master after rotating logs (`kill -USR1 $(cat /run/nginx.pid)`).
- **Socket Sharding (`reuseport`)**: On high-core servers, add `listen 443 ssl http2 reuseport;` on the primary server block to enable kernel-level socket load balancing across worker threads.
- **Microcaching**: For read-heavy API endpoints, implement Nginx proxy caching (`proxy_cache_path /var/cache/nginx levels=1:2 keys_zone=api_cache:10m max_size=1g`) with `proxy_cache_valid 200 5s;` to absorb massive traffic surges.

---
*Next Project: [07-disk-full-and-oom-troubleshooting-labs.md](file:///04-devops/linux/07-projects/07-disk-full-and-oom-troubleshooting-labs.md)*
