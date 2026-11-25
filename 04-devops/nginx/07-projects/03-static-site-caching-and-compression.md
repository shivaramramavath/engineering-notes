---
title: "Project 03: Static Site Delivery with Brotli & Gzip Pre-Compression"
description: "Production reference implementation for ultra-high throughput static web servers using static Brotli and Gzip pre-compression, zero-copy kernel sendfile, and open file cache tuning."
module: "07-projects"
project_id: "03-static-site-caching-and-compression"
status: "completed"
tags: ["nginx", "project", "static-files", "brotli", "gzip", "sendfile", "performance", "pre-compression"]
prerequisites: ["01-fundamentals/04-static-files.md", "02-core-features/07-compression-gzip-and-brotli.md"]
estimated_read_time: "35 min"
---

# Project 03: Static Site Delivery with Brotli & Gzip Pre-Compression

```
+-------------------------------------------------------------------------------+
| Static Pre-Compression Zero-Copy Architecture                                 |
|                                                                               |
|  [Client Request] (Accept-Encoding: br, gzip)                                 |
|        │                                                                      |
|        ▼                                                                      |
|  [NGINX Worker Process]                                                       |
|        │                                                                      |
|        ├── 1. Check client Accept-Encoding: Supports Brotli (`br`)?           |
|        │                                                                      |
|        ├── 2. Inspect filesystem for pre-compressed `.br` sibling file:       |
|        │      Does `/var/www/site/app.js.br` exist on disk?                   |
|        │      ├── YES: Serve `app.js.br` with Content-Encoding: br!           |
|        │      └── NO:  Check for `app.js.gz` (gzip_static)                   |
|        │               ├── YES: Serve `app.js.gz` with Content-Encoding: gzip |
|        │               └── NO:  Serve uncompressed `app.js`                   |
|        ▼                                                                      |
|  [Linux Kernel Page Cache]                                                    |
|        │  Direct zero-copy data transfer to socket via sendfile(2)            |
|        ▼  ZERO on-the-fly CPU compression cycles consumed!                    |
|  [Network Socket Descriptors] ---> [Client Browser: Decompresses in 1ms]     |
+-------------------------------------------------------------------------------+
```

---

## 1. Architecture & Objective

### Objective
Build an ultra-high performance static file delivery edge server designed for Jamstack, documentation portals, and static site generators (Astro, Next.js static exports, Hugo, Docusaurus) capable of handling **50,000+ requests per second** on modest hardware with minimal CPU utilization.

### Core Architectural Requirements
1. **Zero On-the-Fly CPU Compression Overhead**: Compress assets at build time using maximum compression levels (Brotli level 11, Gzip level 9). Serve pre-compressed `.br` and `.gz` files directly from disk using `brotli_static on;` and `gzip_static on;`.
2. **Zero-Copy Kernel Delivery**: Transfer file bytes directly from kernel page cache to network socket descriptors via `sendfile on;` and `tcp_nopush on;`.
3. **Open File Descriptor In-Memory Caching**: Eliminate redundant disk `stat()` and `open()` system calls by maintaining an active in-memory cache of file metadata (`open_file_cache`).
4. **Content Negotiation**: Automatically negotiate the best supported encoding (Brotli preferred over Gzip, Gzip preferred over uncompressed) based on client `Accept-Encoding` headers.
5. **Cache-Control Automation**: Enforce 1-year immutable caching on versioned static assets while serving clean HTML files with dynamic revalidation.

---

## 2. Complete Config Code

### 1. Production Static Server Configuration (`/etc/nginx/conf.d/static.example.com.conf`)

```nginx
# Load Brotli dynamic modules (if compiled as dynamic modules)
# load_module modules/ngx_http_brotli_filter_module.so;
# load_module modules/ngx_http_brotli_static_module.so;

server {
    listen 80;
    server_name static.example.com;
    return 301 https://$host$request_uri;
}

server {
    listen 443 ssl;
    http2 on;
    server_name static.example.com;

    # TLS 1.3 / 1.2
    ssl_certificate /etc/letsencrypt/live/static.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/static.example.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_session_cache shared:SSL:20m;
    ssl_session_timeout 1d;

    root /var/www/static-site;
    index index.html;

    # 1. Zero-Copy Kernel Delivery Mechanics
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;

    # 2. Aggressive In-Memory Open File Descriptor Cache
    open_file_cache max=100000 inactive=60s;
    open_file_cache_valid 120s;
    open_file_cache_min_uses 2;
    open_file_cache_errors on;

    # 3. Static Pre-Compression Engines (Zero On-the-Fly CPU)
    # Checks for file.ext.br before serving file.ext
    brotli_static on;
    # Checks for file.ext.gz before serving file.ext
    gzip_static on;

    # 4. Fallback On-the-Fly Compression (For un-pre-compressed files)
    brotli on;
    brotli_comp_level 5;
    brotli_types text/plain text/css application/javascript application/json
                 image/svg+xml application/xml+rss application/wasm;

    gzip on;
    gzip_comp_level 5;
    gzip_vary on;
    gzip_min_length 1024;
    gzip_types text/plain text/css application/javascript application/json
               image/svg+xml application/xml+rss application/wasm;

    # Security Headers
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;
    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains" always;

    # Immutable Fingerprinted Static Assets
    location ~* \.(?:css|js|woff2|wasm|png|jpg|webp)$ {
        expires 1y;
        add_header Cache-Control "public, max-age=31536000, immutable" always;
        add_header Vary "Accept-Encoding" always;
        access_log off;
        try_files $uri =404;
    }

    # HTML Pages & Clean URLs
    location / {
        add_header Cache-Control "public, no-cache, must-revalidate" always;
        add_header Vary "Accept-Encoding" always;
        # Support clean URLs: /about maps to /about.html or /about/index.html
        try_files $uri $uri.html $uri/ /index.html =404;
    }
}
```

---

### 2. Build-Time Static Pre-Compression Script (`precompress.sh`)

Run this script in your build or CI/CD pipeline immediately following static site generation:

```bash
#!/usr/bin/env bash
set -euo pipefail

DIST_DIR="/var/www/static-site"
echo "==> Starting build-time static pre-compression in: ${DIST_DIR}"

# 1. Generate maximum compression Gzip files (.gz) using gzip -9
echo "==> Compressing with Gzip (Level 9)..."
find "${DIST_DIR}" -type f \( \
    -name "*.html" -o \
    -name "*.css" -o \
    -name "*.js" -o \
    -name "*.json" -o \
    -name "*.svg" -o \
    -name "*.wasm" \
\) -exec gzip -9 -k -f {} +

# 2. Generate maximum compression Brotli files (.br) using brotli -11 (or brotli CLI)
echo "==> Compressing with Brotli (Level 11 - Maximum Density)..."
find "${DIST_DIR}" -type f \( \
    -name "*.html" -o \
    -name "*.css" -o \
    -name "*.js" -o \
    -name "*.json" -o \
    -name "*.svg" -o \
    -name "*.wasm" \
\) -exec brotli -q 11 -f -k {} +

echo "[+] Compression Complete! File inventory:"
ls -lh "${DIST_DIR}"/*.js* || true
```

---

## 3. Step-by-Step Walkthrough

1. **Static vs Dynamic Compression Architecture**:
   - On-the-fly compression (`gzip on;`, `brotli on;`) forces NGINX worker processes to compress every response dynamically in memory using CPU cycles. At Brotli quality level 11, compressing a 1MB JavaScript bundle can take 200ms of 100% CPU time!
   - By running `precompress.sh` during the build stage, you compute the compression once.
   - When a client sends `Accept-Encoding: br`, NGINX detects `brotli_static on;`, finds `bundle.js.br` on disk, sets `Content-Encoding: br`, and sends the pre-compressed bytes directly via `sendfile(2)`. The worker uses **0% CPU** for compression.

2. **The `open_file_cache` Optimization**:
   - Serving 50,000 static requests per second normally requires 50,000 `stat()` and `open()` system calls.
   - `open_file_cache max=100000 inactive=60s;` caches open file descriptor handles, file sizes, and modification timestamps in memory.
   - NGINX reads directly from the cached file descriptor, bypassing disk directory lookups entirely.

3. **Clean URL Resolution (`try_files $uri $uri.html $uri/`)**:
   - Modern static site generators emit clean URLs without `.html` extensions (e.g., `https://static.example.com/blog/announcement`).
   - `try_files` first checks for `/blog/announcement`, then `/blog/announcement.html`, and finally `/blog/announcement/index.html`, delivering clean URLs without external redirect loops.

4. **The `Vary: Accept-Encoding` Requirement**:
   - Both location blocks emit `add_header Vary "Accept-Encoding" always;`.
   - This prevents downstream shared caches (corporate proxies, CDNs) from serving Brotli-compressed bytes to legacy clients that only support Gzip or plaintext.

---

## 4. Verification & Testing

### Test 1: Verify Brotli Pre-Compressed Delivery
```bash
curl -sI -H "Accept-Encoding: br" https://static.example.com/assets/app.js
```
*Expected Output*:
```http
HTTP/2 200
content-type: application/javascript
content-encoding: br
vary: Accept-Encoding
cache-control: public, max-age=31536000, immutable
```

### Test 2: Verify Gzip Fallback for Legacy Clients
```bash
curl -sI -H "Accept-Encoding: gzip" https://static.example.com/assets/app.js
```
*Expected Output*:
```http
HTTP/2 200
content-type: application/javascript
content-encoding: gzip
vary: Accept-Encoding
```

### Test 3: High-Concurrency Benchmark (`wrk`)
Benchmark static file throughput using zero-copy `sendfile`:
```bash
wrk -t8 -c200 -d10s -H "Accept-Encoding: br" https://static.example.com/assets/app.js
```
*Expected Output*:
```text
Running 10s test @ https://static.example.com/assets/app.js
  8 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.82ms    1.12ms  18.42ms   84.12%
    Req/Sec     6.85k   624.12     8.90k    72.50%
  548000 requests in 10.02s, 1.42GB read
Requests/sec:  54690.61
Transfer/sec:    141.71MB
```

---

## 5. Troubleshooting & Edge Cases

### Edge Case 1: `brotli_static` Unknown Directive
- **Symptom**: `nginx -t` fails with `unknown directive "brotli_static"`.
- **Root Cause**: The Google Brotli module (`ngx_brotli`) is a third-party module and is not compiled into the default standard NGINX distribution package.
- **Fix**: Install `nginx-module-brotli` (Ubuntu/Debian) or compile NGINX with `--add-dynamic-module=/path/to/ngx_brotli`. Add `load_module modules/ngx_http_brotli_static_module.so;` to the top of `nginx.conf`.

### Edge Case 2: Stale Content Served After File Replacement
- **Symptom**: You update `index.html` on disk, but NGINX continues serving the old content for up to 60 seconds.
- **Root Cause**: `open_file_cache_valid 120s;` caches the file descriptor and contents in memory until the validity period elapses.
- **Fix**: For immediate production deployment propagation, execute `sudo nginx -s reload` to cycle worker processes and invalidate file descriptor caches instantly.

---

[← Previous Project: 02-spa-with-api-proxy.md](02-spa-with-api-proxy.md) | [Back to Module Index](README.md) | [Next Project: 04-load-balanced-backend-cluster.md →](04-load-balanced-backend-cluster.md)
