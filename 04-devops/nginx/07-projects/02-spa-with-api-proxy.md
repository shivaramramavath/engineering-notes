---
title: "Project 02: Single-Page Application (SPA) with API Reverse Proxy"
description: "Production reference implementation for hosting modern Single-Page Applications (React, Vue, Vite, Next.js static) alongside a backend API on a single unified domain without CORS."
module: "07-projects"
project_id: "02-spa-with-api-proxy"
status: "completed"
tags: ["nginx", "project", "spa", "react", "vite", "reverse-proxy", "caching", "try-files", "zero-cors"]
prerequisites: ["01-fundamentals/04-static-files.md", "02-core-features/02-reverse-proxy.md"]
estimated_read_time: "35 min"
---

# Project 02: Single-Page Application (SPA) with API Reverse Proxy

```
+-------------------------------------------------------------------------------+
| Same-Domain Unified Architecture (Zero CORS Overhead)                         |
|                                                                               |
|  Client Browser (https://app.example.com)                                     |
|        │                                                                      |
|        ▼                                                                      |
|  [NGINX Reverse Proxy & Static Host (Port 443)]                               |
|        │                                                                      |
|        ├── /assets/* (JS/CSS/Fonts)     --> [1-Year Immutable Disk Cache]     |
|        │                                    Cache-Control: public, max-age=1y |
|        │                                                                      |
|        ├── / (index.html / Deep Routes) --> [Zero-Cache HTML Entrypoint]      |
|        │   /dashboard, /settings            try_files $uri $uri/ /index.html; |
|        │                                    Cache-Control: no-cache, no-store |
|        │                                                                      |
|        └── /api/*                       --> [Upstream Backend Node.js / Go]   |
|            /api/v1/auth/*                   proxy_pass http://127.0.0.1:3000; |
|                                             Same-origin: ZERO CORS HEADERS!   |
+-------------------------------------------------------------------------------+
```

---

## 1. Architecture & Objective

### Objective
Host a modern Single-Page Application (React, Vue, Vite, Angular, or Svelte) and its corresponding backend API service on a **single unified origin** (`app.example.com`).

### Core Architectural Requirements
1. **Zero CORS Overhead**: Serving both the frontend assets and `/api/` endpoints from the same origin eliminates Cross-Origin Resource Sharing (CORS) preflight `OPTIONS` requests, reducing API latency by 50%.
2. **Client-Side Deep Link Routing**: Support HTML5 pushState routing (e.g. direct visits to `/dashboard/analytics`) using `try_files $uri $uri/ /index.html;` so hard refreshes never return HTTP 404.
3. **Strict Cache Invalidation Hierarchy**:
   - `index.html` must **never be cached** by browsers (`no-cache, no-store`), ensuring users instantly download new versions upon deployment.
   - Fingerprinted static assets (`/assets/*.a8f9c.js`) must be cached for **1 year** with `immutable`.
4. **Atomic Zero-Downtime Deployment Pipeline**: Deploy frontend builds using symlinked directory flipping (`/var/www/app/current -> releases/YYYYMMDDHHMMSS`).

---

## 2. Complete Config Code

### 1. Unified Virtual Host Configuration (`/etc/nginx/conf.d/app.example.com.conf`)

```nginx
# Upstream Application Backend
upstream node_backend {
    server 127.0.0.1:3000;
    keepalive 32;
}

# HTTP to HTTPS Canonical Redirect
server {
    listen 80;
    server_name app.example.com;
    return 301 https://$host$request_uri;
}

# Master SPA & API Virtual Host
server {
    listen 443 ssl;
    http2 on;
    server_name app.example.com;

    # TLS Certificates
    ssl_certificate /etc/letsencrypt/live/app.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/app.example.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5;
    ssl_session_cache shared:SSL:10m;
    ssl_session_timeout 1d;

    # Root Directory Points to Current Deployment Symlink
    root /var/www/app/current;
    index index.html;

    # Performance
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;

    # Enable Gzip Compression for Frontend Assets
    gzip on;
    gzip_vary on;
    gzip_proxied any;
    gzip_comp_level 6;
    gzip_min_length 1024;
    gzip_types text/plain text/css application/json application/javascript
               text/xml application/xml application/xml+rss text/javascript
               image/svg+xml;

    # Global Rate Limiting for Public App
    limit_req zone=api_general burst=40 nodelay;

    # 1. API Reverse Proxy Layer
    location /api/ {
        proxy_pass http://node_backend;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto https;

        # Disable Buffering for Real-time Streaming
        proxy_buffering on;
        proxy_buffer_size 8k;
        proxy_buffers 8 32k;

        # Security Headers
        add_header X-Content-Type-Options "nosniff" always;
        add_header X-Frame-Options "DENY" always;
    }

    # 2. Strict Rate Limiting on Authentication API
    location /api/v1/auth/ {
        limit_req zone=api_sensitive burst=5 nodelay;

        proxy_pass http://node_backend;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto https;
    }

    # 3. Fingerprinted Static Assets (Vite / Webpack / CRA output)
    location /assets/ {
        expires 1y;
        add_header Cache-Control "public, max-age=31536000, immutable" always;
        add_header X-Content-Type-Options "nosniff" always;
        access_log off;
        try_files $uri =404;
    }

    # 4. Standard Static Root Files (Favicons, Robots.txt)
    location ~* \.(?:ico|png|svg|txt|webmanifest)$ {
        expires 7d;
        add_header Cache-Control "public, max-age=604800" always;
        access_log off;
        try_files $uri =404;
    }

    # 5. HTML Entrypoint & Client-Side HTML5 PushState Fallback
    location / {
        # Never cache index.html!
        add_header Cache-Control "no-store, no-cache, must-revalidate, proxy-revalidate, max-age=0" always;
        add_header Pragma "no-cache" always;
        add_header X-Content-Type-Options "nosniff" always;
        add_header X-Frame-Options "SAMEORIGIN" always;

        # Route matching: 1. Exact file, 2. Directory, 3. Fallback to index.html
        try_files $uri $uri/ /index.html;
    }
}
```

---

### 2. Atomic Zero-Downtime Deployment Script (`deploy.sh`)

```bash
#!/usr/bin/env bash
set -euo pipefail

APP_DIR="/var/www/app"
TIMESTAMP=$(date +%Y%m%d%H%M%S)
NEW_RELEASE="${APP_DIR}/releases/${TIMESTAMP}"

echo "==> Creating release directory: ${NEW_RELEASE}"
mkdir -p "${NEW_RELEASE}"

echo "==> Unpacking frontend build artifacts..."
tar -xzf dist.tar.gz -C "${NEW_RELEASE}"

echo "==> Setting permissions..."
chown -R www-data:www-data "${NEW_RELEASE}"
find "${NEW_RELEASE}" -type d -exec chmod 755 {} +
find "${NEW_RELEASE}" -type f -exec chmod 644 {} +

echo "==> Atomically switching symlink..."
# Using ln -sfn ensures atomic swap
ln -sfn "${NEW_RELEASE}" "${APP_DIR}/current"

echo "==> Pruning older releases (keep last 5)..."
cd "${APP_DIR}/releases" && ls -1t | tail -n +6 | xargs -r rm -rf

echo "[+] Deployment completed successfully. Active release: ${TIMESTAMP}"
```

---

## 3. Step-by-Step Walkthrough

1. **The `try_files` Routing Invariant**:
   - When a user visits `https://app.example.com/analytics`, the browser requests the literal URI path `/analytics`.
   - NGINX checks if `/var/www/app/current/analytics` exists as a file ($uri). It does not.
   - NGINX checks if `/var/www/app/current/analytics/` exists as a directory ($uri/). It does not.
   - NGINX falls back to `/index.html`.
   - NGINX serves `/index.html` with an **HTTP 200** status code.
   - The browser receives `index.html`, executes the JavaScript bundle, and React Router / Vue Router parses the URL path `/analytics` to render the correct view client-side.

2. **The "Never Cache HTML" Rule**:
   - Notice lines 85–88 inside `location /`:
     ```nginx
     add_header Cache-Control "no-store, no-cache, must-revalidate, max-age=0" always;
     ```
   - When a new deployment occurs, the new `index.html` references newly hashed JavaScript files (`bundle.e4a2.js`).
   - If `index.html` were cached by the browser, the user would continue loading the old HTML file, which in turn requests old asset hashes that no longer exist on the server, resulting in white screens and 404 errors!

3. **Immutable Fingerprinted Asset Caching**:
   - In `location /assets/`, Vite or Webpack outputs files with content hashes in the filename.
   - Setting `Cache-Control: public, max-age=31536000, immutable` tells browsers that this specific file will never change. Even when the user presses refresh, the browser loads the asset from local disk in 0ms without sending conditional HTTP requests to NGINX.

4. **Zero-CORS Same-Origin Routing**:
   - Because API requests target `/api/`, the request domain matches the page origin (`app.example.com`).
   - Browsers omit the preflight `OPTIONS` handshake, eliminating one entire network round-trip for every API call and freeing developers from configuring complex `Access-Control-Allow-Origin` headers.

---

## 4. Verification & Testing

### Test 1: Verify Client-Side Deep Link Fallback (HTTP 200)
```bash
curl -sI https://app.example.com/deep/nested/route/profile
```
*Expected Output*:
```http
HTTP/2 200
content-type: text/html; charset=utf-8
cache-control: no-store, no-cache, must-revalidate, max-age=0
```

### Test 2: Verify Immutable Cache on JavaScript Assets
```bash
curl -sI https://app.example.com/assets/index-D7b3Fa1.js
```
*Expected Output*:
```http
HTTP/2 200
content-type: application/javascript
cache-control: public, max-age=31536000, immutable
expires: Wed, 24 Nov 2027 12:00:00 GMT
```

### Test 3: Verify API Proxying Without CORS
```bash
curl -sI https://app.example.com/api/v1/health
```
*Expected Output*:
```http
HTTP/2 200
content-type: application/json; charset=utf-8
x-content-type-options: nosniff
```

---

## 5. Troubleshooting & Edge Cases

### Edge Case 1: Hard Refresh Returns 404 on Assets
- **Symptom**: User leaves tab open for 3 days; new release deploys; user clicks a subpage and receives 404 on `assets/vendor-old.js`.
- **Root Cause**: The deployment script deleted the previous release folder immediately, while users still had the old `index.html` loaded in memory.
- **Fix**: The deployment script must retain the last **3 to 5 releases** in `/var/www/app/releases/` so that active user tabs can continue loading old chunks during rolling deploys.

### Edge Case 2: Broken Inode Desync on Linux Symlink Swap
- **Symptom**: NGINX continues serving files from the old release after symlink update.
- **Root Cause**: `open_file_cache` is configured with an excessively long `inactive` or `valid` timer, causing NGINX worker processes to hold open file descriptors to the old release directory.
- **Fix**: Set `open_file_cache_valid 60s;` or execute `sudo nginx -s reload` in the deployment script to cycle worker caches immediately.

---

[← Previous Project: 01-secure-gateway-stack.md](01-secure-gateway-stack.md) | [Back to Module Index](README.md) | [Next Project: 03-static-site-caching-and-compression.md →](03-static-site-caching-and-compression.md)
