---
title: "HTTP Compression: Gzip and Brotli in NGINX"
description: "Mastering response compression in NGINX using Gzip, dynamic Brotli, pre-compressed static assets (gzip_static, brotli_static), and mitigating the BREACH attack."
module: "02-core-features"
track: "devops-nginx"
order: 7
difficulty: "Intermediate to Advanced"
prerequisites:
  - "01-fundamentals/04-static-files"
  - "02-core-features/02-reverse-proxy"
tags:
  - nginx
  - compression
  - gzip
  - brotli
  - performance
  - breach-attack
  - static-assets
last_updated: "2026-03-30"
---

# HTTP Compression: Gzip and Brotli in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Production High-Performance Gzip Configuration](#example-1-production-high-performance-gzip-configuration)
  - [Example 2: Next-Gen Brotli Compression with Fallback to Gzip](#example-2-next-gen-brotli-compression-with-fallback-to-gzip)
  - [Example 3: Zero-CPU Pre-Compressed Assets (`gzip_static` & `brotli_static`)](#example-3-zero-cpu-pre-compressed-assets-gzip_static--brotli_static)
  - [Example 4: Compressing Proxied Upstream Responses](#example-4-compressing-proxied-upstream-responses)
  - [Example 5: BREACH Attack Mitigation for HTTPS Sensitive Endpoints](#example-5-breach-attack-mitigation-for-https-sensitive-endpoints)
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

Payload compression is one of the highest-leverage optimizations in web infrastructure. Compressing text-based web assets (HTML, CSS, JavaScript, JSON, SVG, XML) typically reduces file transfer sizes by **65% to 85%**. This directly decreases page load times, minimizes mobile data usage, and dramatically cuts cloud bandwidth egress costs.

In NGINX, compression is implemented via two primary engines:
1. **Gzip (`ngx_http_gzip_module`)**: Built directly into core NGINX. Universally supported by 100% of modern and legacy clients.
2. **Brotli (`ngx_brotli`)**: Developed by Google specifically for web payloads. Brotli provides **15% to 25% better compression density** than Gzip at comparable decompression speeds, and is natively supported across all modern browsers over HTTPS.

Furthermore, NGINX provides the static pre-compression modules (`gzip_static` and `brotli_static`), which allow files to be compressed during CI/CD build pipelines. When serving pre-compressed files, NGINX reads the `.gz` or `.br` files directly off disk and streams them with **zero runtime CPU compression overhead**.

---

## 2. Core Concepts

### Content Negotiation: `Accept-Encoding` and `Content-Encoding`
HTTP compression relies on standard client-server content negotiation:
1. The client indicates supported algorithms in the request header:
   ```http
   Accept-Encoding: gzip, deflate, br, zstd
   ```
2. NGINX inspects the header, selects the best matching algorithm (preferring Brotli if available, then Gzip), compresses the payload on the fly, and returns the response with:
   ```http
   Content-Encoding: br
   Vary: Accept-Encoding
   ```

### The `Vary: Accept-Encoding` Header
When compression is enabled, downstream intermediate caches (CDNs, forward proxies, corporate firewalls) must be informed that the response body varies depending on the client's `Accept-Encoding` header. If `Vary: Accept-Encoding` is omitted, an intermediate CDN might cache a compressed Brotli response and erroneously serve it to an older client that only understands Gzip or plain text, corrupting the page.

### Compression Levels vs CPU Tradeoffs
Both Gzip and Brotli support configurable compression levels:
- **Gzip (Levels 1–9)**: Level 5 or 6 is the universal sweet spot. Levels 7–9 consume exponentially more CPU cycles for minuscule (1–2%) reductions in payload size.
- **Brotli (Levels 0–11)**:
  - *Dynamic on-the-fly*: Level 4 to 6 is optimal for real-time proxying.
  - *Static pre-compression*: Level 11 (maximum) is used during offline build steps, where build-time CPU is irrelevant and maximum byte reduction is desired.

### The BREACH Attack (CVE-2013-3587)
BREACH (Browser Reconnaissance and Exfiltration via Adaptive Compression of Hypertext) is a side-channel cryptographic attack against HTTPS. If an application reflects user input (like a search query or CSRF token) in the same compressed HTTPS response alongside secrets, an attacker can observe encrypted packet sizes to deduce secret tokens character by character. Mitigating BREACH involves disabling compression on sensitive dynamic pages or masking secrets.

---

## 3. How It Works Internally

### On-the-Fly Dynamic Compression Pipeline

```
Upstream Backend / Static Disk
             |
             | Raw Uncompressed Stream (text/html, application/json)
             v
+------------------------------------+
| NGINX Filter Chain                 |
| (ngx_http_gzip_filter_module /     |
|  ngx_http_brotli_filter_module)    |
+------------------+-----------------+
                   |
        Does request allow 'br' or 'gzip'?
        Is Content-Type in gzip_types?
        Is body length > gzip_min_length?
        /                             \
      YES                              NO
       |                                |
+------v----------------------+         |
| Compression Buffer Allocated|         |
| Deflate / Brotli stream     |         |
| Encodes data chunks in-RAM  |         |
+--------------+--------------+         |
               |                        |
               +------------------------+
             |
             | Filtered Stream (Content-Encoding: br/gzip, Vary: Accept-Encoding)
             v
       epoll / Client Socket
```

### Static Pre-Compression (`gzip_static` / `brotli_static`)

```
Incoming Request: GET /app.js (Accept-Encoding: gzip, br)
                        |
                        v
         Check filesystem for: /app.js.br
                   /             \
             EXISTS               NOT FOUND
               |                        \
+--------------v---------------+   Check filesystem for: /app.js.gz
| Zero-Copy sendfile()         |          /             \
| Transmits /app.js.br directly|    EXISTS               NOT FOUND
| Content-Encoding: br         |       |                        \
| ZERO CPU Overhead!           | +-----v------------+    +-------v--------+
+------------------------------+ | sendfile()       |    | Dynamic Gzip   |
                                 | /app.js.gz       |    | or plain .js   |
                                 +------------------+    +----------------+
```

---

## 4. Syntax & Directives Reference

### Gzip Directives (`ngx_http_gzip_module`)
| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `gzip on\|off` | `http`, `server`, `location` | `off` | Toggles dynamic Gzip compression on or off. |
| `gzip_types mime-type ...` | `http`, `server`, `location` | `text/html` | MIME types to compress. `text/html` is always compressed. |
| `gzip_comp_level level` | `http`, `server`, `location` | `1` | Compression level (1–9). Recommended: `5` or `6`. |
| `gzip_min_length length` | `http`, `server`, `location` | `20` | Minimum payload byte size before compression is applied (e.g., `1024`). |
| `gzip_proxied [off\|expired\|no-cache\|...]` | `http`, `server`, `location` | `off` | Enables compression for responses received from proxied upstreams. Recommended: `any`. |
| `gzip_vary on\|off` | `http`, `server`, `location` | `off` | Emits `Vary: Accept-Encoding` response header when compression is active. |
| `gzip_static on\|off\|always` | `http`, `server`, `location` | `off` | Serves pre-compressed `.gz` files if present on disk. |
| `gzip_buffers number size` | `http`, `server`, `location` | `32 4k\|16 8k` | Number and size of memory buffers used to compress responses. |

### Brotli Directives (`ngx_brotli` module)
| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `brotli on\|off` | `http`, `server`, `location` | `off` | Enables dynamic on-the-fly Brotli compression. |
| `brotli_types mime-type ...` | `http`, `server`, `location` | `text/html` | MIME types to compress with Brotli. |
| `brotli_comp_level level` | `http`, `server`, `location` | `6` | Dynamic compression level (0–11). Recommended: `4` or `5`. |
| `brotli_min_length length` | `http`, `server`, `location` | `20` | Minimum response byte length for Brotli compression. |
| `brotli_static on\|off` | `http`, `server`, `location` | `off` | Serves pre-compressed `.br` files directly from disk. |

---

## 5. Practical Implementation Examples

### Example 1: Production High-Performance Gzip Configuration

Configuring dynamic Gzip in the `http {}` context to apply universally across all virtual hosts with optimal compression levels and MIME types.

```nginx
# /etc/nginx/conf.d/gzip_production.conf

http {
    # Enable Gzip
    gzip on;

    # Optimal balance between compression ratio and CPU usage (1-9)
    gzip_comp_level 5;

    # Don't compress tiny files under 1KB (headers overhead exceeds compression savings)
    gzip_min_length 1024;

    # Enable compression for all proxied upstream responses
    gzip_proxied any;

    # Always inject 'Vary: Accept-Encoding' for downstream caches/CDNs
    gzip_vary on;

    # Disable for ancient browsers (Internet Explorer 6)
    gzip_disable "msie6";

    # MIME types to compress (text/html is implicitly included)
    gzip_types
        text/plain
        text/css
        text/xml
        text/javascript
        application/javascript
        application/x-javascript
        application/json
        application/xml
        application/rss+xml
        application/atom+xml
        image/svg+xml
        font/truetype
        font/opentype
        application/vnd.ms-fontobject;
}
```

### Example 2: Next-Gen Brotli Compression with Fallback to Gzip

Leverage the Google Brotli module for modern browsers while automatically falling back to Gzip for older clients.

```nginx
# /etc/nginx/conf.d/brotli_and_gzip.conf

server {
    listen 443 ssl;
    http2 on;
    server_name fast.example.com;

    ssl_certificate     /etc/ssl/certs/fast.crt;
    ssl_certificate_key /etc/ssl/private/fast.key;

    # --- 1. Brotli Configuration ---
    brotli on;
    brotli_comp_level 5; # Level 5 is fast enough for on-the-fly proxying
    brotli_min_length 1024;
    brotli_types
        text/plain
        text/css
        text/xml
        application/javascript
        application/json
        application/xml
        image/svg+xml;

    # --- 2. Gzip Fallback Configuration ---
    gzip on;
    gzip_comp_level 5;
    gzip_min_length 1024;
    gzip_proxied any;
    gzip_vary on;
    gzip_types
        text/plain
        text/css
        application/javascript
        application/json;

    location / {
        proxy_pass http://frontend_upstream;
        proxy_set_header Host $host;
    }
}
```

### Example 3: Zero-CPU Pre-Compressed Assets (`gzip_static` & `brotli_static`)

Serving modern Webpack/Vite/Next.js production bundles pre-compressed during build time.

```nginx
# /etc/nginx/conf.d/static_precompressed.conf

server {
    listen 80;
    server_name cdn.example.com;
    root /var/www/static_assets;

    # Serve pre-compressed .br and .gz files if they exist on disk alongside .js/.css
    brotli_static on;
    gzip_static   on;

    # Fallback dynamic gzip if a file was not pre-compressed
    gzip on;
    gzip_comp_level 5;
    gzip_types text/css application/javascript image/svg+xml;

    location /assets/ {
        # High-performance zero-copy kernel streaming
        sendfile       on;
        tcp_nopush     on;
        tcp_nodelay    on;

        # Cache immutable static assets in browser for 1 year
        expires 1y;
        add_header Cache-Control "public, immutable";
        add_header Vary "Accept-Encoding";
    }
}
```

#### Asset Build-Time Pre-Compression Script:
```bash
# In CI/CD build step:
# 1. Generate .gz (level 9 maximum compression)
find /var/www/static_assets -type f \( -name "*.js" -o -name "*.css" -o -name "*.svg" \) -exec gzip -9 -k -f {} +

# 2. Generate .br (level 11 maximum compression)
find /var/www/static_assets -type f \( -name "*.js" -o -name "*.css" -o -name "*.svg" \) -exec brotli -11 -k -f {} +
```

### Example 4: Compressing Proxied Upstream Responses

When NGINX acts as a reverse proxy for Node.js, Python, or Go backends, ensure NGINX compresses upstream responses that lack compression.

```nginx
# /etc/nginx/conf.d/upstream_compression.conf

upstream api_service {
    server 127.0.0.1:3000;
}

server {
    listen 80;
    server_name api.example.com;

    location / {
        proxy_pass http://api_service;
        proxy_set_header Host $host;

        # Tell upstream NOT to compress so NGINX can handle compression uniformly
        proxy_set_header Accept-Encoding "";

        # Enable NGINX gzip for proxied responses
        gzip on;
        gzip_proxied any;
        gzip_min_length 512;
        gzip_types application/json text/plain;
    }
}
```

### Example 5: BREACH Attack Mitigation for HTTPS Sensitive Endpoints

Selectively disabling compression on endpoints containing sensitive CSRF tokens, session state, or user-reflected secrets.

```nginx
# /etc/nginx/conf.d/breach_mitigation.conf

server {
    listen 443 ssl;
    server_name bank.example.com;

    ssl_certificate     /etc/ssl/certs/bank.crt;
    ssl_certificate_key /etc/ssl/private/bank.key;

    # Enable compression globally for static assets
    gzip on;
    gzip_types text/css application/javascript;

    # 1. Public Static Assets: Fully compressed
    location /static/ {
        root /var/www/bank;
        gzip_static on;
    }

    # 2. Sensitive Authentication and Account APIs: DISABLE COMPRESSION to stop BREACH
    location ~* ^/(auth|login|account|transfer|api/v1/user) {
        gzip off;
        brotli off;

        proxy_pass http://bank_backend;
        proxy_set_header Host $host;
        add_header Cache-Control "no-store, no-cache, must-revalidate" always;
    }
}
```

---

## 6. Production Best Practices

1. **Never Compress Already-Compressed Media**: Compressing images (`.png`, `.jpg`, `.webp`), video (`.mp4`), audio (`.mp3`), or compressed archives (`.zip`, `.gz`) wastes enormous CPU cycles and actually **increases** file size due to compression header overhead. Never add image MIME types (except SVG) to `gzip_types`.
2. **Set `gzip_min_length` to at least 1024 Bytes**: Small payloads (<1KB) rarely compress well; the added HTTP header bytes and framing overhead often outweigh any reduction in body bytes.
3. **Always Include `gzip_vary on;`**: Omitting `gzip_vary` breaks caching layers, leading to compressed bytes being delivered to non-supporting clients or uncompressed bytes being cached globally.
4. **Prefer `gzip_static` in CI/CD**: Running dynamic compression on high-traffic sites consumes significant CPU. Generating `.gz` and `.br` assets during build time and using `gzip_static on;` reduces CPU usage to near zero.
5. **Set `gzip_proxied any;` for Microservices**: By default, `gzip_proxied` is `off`, meaning NGINX will **not** compress responses coming from upstream servers (`proxy_pass`). Setting `gzip_proxied any;` is required for NGINX to compress backend JSON/HTML.
6. **Limit Dynamic Brotli Level to 5**: While Brotli level 11 offers incredible compression, it is 100x slower to compress than level 5. Never use Brotli level > 6 for dynamic, on-the-fly web traffic.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Adding `image/jpeg` or `image/png` to `gzip_types`** | JPEGs and PNGs are already mathematically compressed. Re-compressing consumes 100% CPU on workers and often inflates payload size. | Only compress text and vector assets: HTML, CSS, JS, JSON, XML, SVG. |
| **Leaving `gzip_comp_level` at 9** | Level 9 increases CPU usage by 300%–500% compared to level 5, while gaining less than 1.5% in byte reduction. | Use `gzip_comp_level 5;` or `6;`. |
| **Omitting `gzip_proxied any;` behind Reverse Proxy** | NGINX refuses to compress responses coming from `proxy_pass` backends; JSON payloads are transmitted completely uncompressed. | Add `gzip_proxied any;` to `http` or `location` block. |
| **Omitting `gzip_vary on;`** | Downstream CDN / ISP caches store the compressed response under the plain URI, serving garbled Gzip bytes to clients that sent no `Accept-Encoding`. | Always enable `gzip_vary on;`. |
| **Setting `gzip_types *`** | `*` is not supported in `gzip_types`. NGINX treats `*` as a literal MIME type string, resulting in compression being disabled for everything except `text/html`. | Explicitly enumerate MIME types: `text/plain text/css application/json ...`. |
| **Forgetting `sendfile off;` when using old `gzip_static` bugs** | In legacy NGINX builds, combining `gzip_static` with certain `sendfile` versions caused truncated chunk issues. | On modern NGINX (1.20+), `gzip_static` works natively with `sendfile on;`. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test Gzip Compression via curl (Inspect Content-Encoding and Vary headers)
curl -s -I -H "Accept-Encoding: gzip" https://fast.example.com/assets/app.js | grep -E -i "content-encoding|vary|content-length"

# Expected:
# Content-Encoding: gzip
# Vary: Accept-Encoding

# 2. Test Brotli Compression via curl
curl -s -I -H "Accept-Encoding: br" https://fast.example.com/assets/app.js | grep -E -i "content-encoding|vary"

# Expected:
# Content-Encoding: br
# Vary: Accept-Encoding

# 3. Verify Static Pre-compression (Check if file is read directly without CPU spike)
ls -lh /var/www/static_assets/assets/app.js*
# Should show:
# app.js      (e.g., 500 KB)
# app.js.gz   (e.g., 120 KB)
# app.js.br   (e.g.,  95 KB)

# 4. Measure Actual Byte Savings and Compression Ratio
curl -s -H "Accept-Encoding: gzip" https://fast.example.com/api/data.json --output /tmp/compressed.gz
curl -s https://fast.example.com/api/data.json --output /tmp/raw.json
echo "Raw size: $(wc -c < /tmp/raw.json) bytes"
echo "Compressed size: $(wc -c < /tmp/compressed.gz) bytes"
```

### CPU Impact of Compression
- Dynamic Gzip level 5 typically accounts for 5% to 15% of total worker CPU utilization on a busy web proxy.
- Switching to `gzip_static on;` drops worker CPU utilization for static delivery to near zero, shifting the workload to the OS kernel's zero-copy `sendfile()` path.

---

## 9. Architectural Comparison Table

| Metric / Dimension | Gzip (Deflate) | Brotli (`br`) | Zstandard (`zstd`) |
| :--- | :--- | :--- | :--- |
| **Browser Support** | 100% (Universal) | 98%+ (All modern HTTPS browsers) | Emerging (Chrome 123+, Firefox 126+) |
| **Compression Ratio (Text)** | Baseline (~70% reduction) | **+15% to 25% better than Gzip** | Comparable to Brotli |
| **Decompression Speed** | Extremely fast | Extremely fast | Fastest |
| **Dynamic CPU Cost** | Low (Level 5) | Medium (Level 4–5) | Very Low |
| **Static Build-Time Ratio**| High (Level 9) | **Highest (Level 11)** | High (Level 22) |
| **HTTPS Requirement** | Works over HTTP and HTTPS | Enforced **HTTPS only** by browsers | Enforced **HTTPS only** by browsers |
| **NGINX Module** | Built-in (`ngx_http_gzip_module`)| Dynamic Module (`ngx_brotli`) | Third-party module |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: Why does NGINX refuse to compress responses from a reverse proxy by default, and how do you enable it?</strong></summary>

**Answer:**
By default, the directive `gzip_proxied` is set to `off`. In earlier web architectures, proxy servers did not reliably pass or understand `Accept-Encoding` and `Vary` headers, leading to corrupted cached responses in intermediate corporate or ISP proxies. 

To enable NGINX to compress responses coming from backend upstream servers (via `proxy_pass` or `fastcgi_pass`), you must configure:
```nginx
gzip_proxied any;
```
The parameter `any` tells NGINX to compress the proxied response regardless of upstream caching headers (such as `Expired`, `No-Cache`, or `Private`).
</details>

<details>
<summary><strong>Q2: What is the purpose of the gzip_vary on directive, and what critical bug does it prevent in CDNs?</strong></summary>

**Answer:**
The `gzip_vary on;` directive instructs NGINX to add the HTTP response header `Vary: Accept-Encoding` whenever compression is applied to a response.

Without this header, downstream intermediate caches (such as Cloudflare, Akamai, or corporate forward proxies) would cache the response body based solely on the URL path. If the first client to request `/script.js` supports Gzip, the CDN caches the gzipped payload. If a second client connects that does *not* support Gzip (e.g., an embedded device or older curl client), the CDN would serve the cached gzipped bytes with a plain `200 OK`, causing the client to fail to parse or execute the corrupted binary data. `Vary: Accept-Encoding` forces the CDN to maintain separate cache entries for compressed and uncompressed representations.
</details>

<details>
<summary><strong>Q3: Explain how gzip_static works and why it is superior to dynamic on-the-fly compression for static assets.</strong></summary>

**Answer:**
With dynamic compression (`gzip on;`), every time a client requests an asset, an NGINX worker must allocate memory buffers, invoke zlib/deflate compression routines, compress the bytes on the CPU, and stream them out. Under heavy load, this causes high CPU usage.

With `gzip_static on;` (provided by `ngx_http_gzip_static_module`), NGINX checks if a file with the `.gz` extension exists alongside the requested file (e.g., `/var/www/app.js.gz`). If present and the client's `Accept-Encoding` supports `gzip`, NGINX bypasses CPU compression entirely. It uses the Linux `sendfile()` kernel system call to stream `/var/www/app.js.gz` directly from the OS page cache to the network socket with **zero CPU cycles** and zero user-space memory copies.
</details>

<details>
<summary><strong>Q4: Why should Brotli compression level 11 never be used for dynamic on-the-fly compression in NGINX?</strong></summary>

**Answer:**
Brotli compression levels range from 0 to 11. Levels 0 to 4 are designed for fast real-time streaming, levels 5 to 6 provide a balanced trade-off, and levels 10 to 11 are designed for offline pre-compression.

At level 11, the Brotli algorithm performs exhaustive dictionary and context modeling. Compressing a 1MB JavaScript bundle at level 11 can take **several seconds of 100% CPU time per request**, whereas level 5 takes only a few milliseconds. Using level 11 dynamically would immediately exhaust the CPU of all NGINX worker processes, leading to extreme request queuing, 504 Gateway Timeouts, and complete server collapse. Level 11 should strictly be reserved for offline CI/CD build scripts with `brotli_static on;`.
</details>

<details>
<summary><strong>Q5: What is the BREACH attack (CVE-2013-3587), and how do you protect an application at the NGINX layer?</strong></summary>

**Answer:**
BREACH targets HTTPS when three conditions are met:
1. The connection uses stream/payload compression (Gzip/Brotli).
2. The HTTP response reflects user-controlled input (e.g., query strings or form parameters).
3. The response body contains sensitive secrets (e.g., CSRF tokens, session IDs, or credit card numbers).

Because compression algorithms replace repeated strings with smaller pointers, an attacker who can inject chosen text into the page can observe subtle variations in the encrypted TLS packet sizes. When the attacker's guess matches a character of the secret token, the compression ratio increases and the packet size decreases. By iterating through character guesses, the attacker extracts the secret.

Mitigations in NGINX:
- Disable compression (`gzip off; brotli off;`) on sensitive dynamic endpoints (login, account, payment, and CSRF token endpoints).
- Alternatively, instruct application developers to use XOR-masking on CSRF tokens (per-request randomized tokens).
</details>

<details>
<summary><strong>Q6: Why is text/html not listed in the gzip_types directive?</strong></summary>

**Answer:**
In NGINX, `text/html` is **always compressed by default** whenever `gzip on;` is enabled. Explicitly including `text/html` in `gzip_types` is redundant and unnecessary. The `gzip_types` directive exists solely to specify *additional* MIME types (such as `application/json`, `text/css`, or `application/javascript`) that should also be compressed.
</details>

<details>
<summary><strong>Q7: What happens if both gzip_static and dynamic gzip are enabled, but no .gz file exists on disk?</strong></summary>

**Answer:**
NGINX executes a graceful fallback. When a request arrives:
1. NGINX first inspects the filesystem for the `.gz` counterpart.
2. If the `.gz` file is not found, `gzip_static` steps aside without returning an error.
3. NGINX falls back to reading the uncompressed file from disk and compresses it on the fly using dynamic `gzip on;` (provided the file matches `gzip_types` and `gzip_min_length`).
</details>

<details>
<summary><strong>Q8: Why does compressing images (PNG, JPEG, WebP) in NGINX harm performance?</strong></summary>

**Answer:**
Standard image formats (JPEG, PNG, WebP, AVIF) are already heavily compressed using specialized discrete cosine transform, wavelet, or DEFLATE algorithms. Attempting to compress already compressed binary data using Gzip or Brotli yields virtually zero additional reduction (often less than 0.1%), and frequently **increases** the file size because of the added Gzip framing headers. Furthermore, it consumes significant CPU cycles on the server, degrading overall system throughput.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Universal Gzip for JSON APIs</strong></summary>

**Goal:** Configure NGINX to dynamically compress JSON API responses larger than 512 bytes with compression level 5.

**Solution:**
```nginx
server {
    listen 80;
    server_name api.local;

    gzip on;
    gzip_comp_level 5;
    gzip_min_length 512;
    gzip_proxied any;
    gzip_vary on;
    gzip_types application/json;

    location / {
        proxy_pass http://127.0.0.1:3000;
        proxy_set_header Host $host;
    }
}
```
Test with:
```bash
curl -I -H "Accept-Encoding: gzip" http://api.local/data
```
</details>

<details>
<summary><strong>Exercise 2: Setup Pre-Compressed Static File Serving</strong></summary>

**Goal:** Configure NGINX to serve pre-compressed `.gz` files for static JavaScript and CSS assets from `/var/www/dist`.

**Solution:**
```nginx
server {
    listen 80;
    server_name static.local;
    root /var/www/dist;

    gzip_static on;
    gzip on; # Fallback
    gzip_types text/css application/javascript;

    location / {
        sendfile on;
        tcp_nopush on;
        try_files $uri =404;
    }
}
```
Generate test asset:
```bash
echo "console.log('Heavy JavaScript application bundle');" > /var/www/dist/app.js
gzip -9 -k /var/www/dist/app.js
```
</details>

<details>
<summary><strong>Exercise 3: Test and Compare Gzip vs Brotli with curl</strong></summary>

**Goal:** Query an endpoint that supports both algorithms and observe the negotiated `Content-Encoding` when sending different `Accept-Encoding` values.

**Solution:**
```bash
# Test Gzip negotiation:
curl -s -I -H "Accept-Encoding: gzip" https://example.com | grep -i content-encoding

# Test Brotli negotiation:
curl -s -I -H "Accept-Encoding: br" https://example.com | grep -i content-encoding

# Test Dual negotiation (Server should prioritize Brotli):
curl -s -I -H "Accept-Encoding: gzip, deflate, br" https://example.com | grep -i content-encoding
```
</details>

<details>
<summary><strong>Exercise 4: Disable Compression on CSRF and Authentication Paths</strong></summary>

**Goal:** Secure an application against BREACH by disabling compression on `/login` and `/api/csrf` while leaving it enabled for the rest of the site.

**Solution:**
```nginx
server {
    listen 443 ssl;
    server_name secure.local;

    ssl_certificate     /etc/ssl/certs/secure.crt;
    ssl_certificate_key /etc/ssl/private/secure.key;

    gzip on;
    gzip_types text/plain text/css application/javascript application/json;

    location / {
        proxy_pass http://app_upstream;
    }

    # BREACH mitigation: Disable compression
    location ~* ^/(login|api/csrf) {
        gzip off;
        proxy_pass http://app_upstream;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5: Verify Minimum Length Compression Threshold</strong></summary>

**Goal:** Verify that files smaller than `gzip_min_length` are served uncompressed, while files above the threshold are compressed.

**Solution:**
1. Configure `gzip_min_length 1024;`.
2. Request a 200-byte file:
   ```bash
   curl -s -I -H "Accept-Encoding: gzip" http://localhost/small.txt
   # Content-Encoding should NOT be present.
   ```
3. Request a 5,000-byte file:
   ```bash
   curl -s -I -H "Accept-Encoding: gzip" http://localhost/large.txt
   # Content-Encoding: gzip MUST be present.
   ```
</details>

<details>
<summary><strong>Exercise 6: Diagnose Missing Upstream Compression</strong></summary>

**Goal:** Troubleshoot why responses from a Python FastAPI backend are not being compressed by NGINX.

**Solution:**
1. Check `error.log` and verify config:
2. Ensure `gzip_proxied any;` is present (FastAPI responses are proxied).
3. Ensure `gzip_types application/json;` is present.
4. Ensure the upstream backend is not already returning `Content-Encoding: gzip` with a corrupted double-compression.
5. In NGINX:
   ```nginx
   proxy_set_header Accept-Encoding ""; # Strip from upstream
   gzip on;
   gzip_proxied any;
   gzip_types application/json;
   ```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production Compression Cheat Sheet

# 1. Universal Gzip Settings (http context)
gzip on;
gzip_comp_level 5;
gzip_min_length 1024;
gzip_proxied any;
gzip_vary on;
gzip_types
    text/plain
    text/css
    application/javascript
    application/json
    application/xml
    image/svg+xml;

# 2. Static Pre-Compression (server / location context)
brotli_static on;
gzip_static   on;

# 3. Dynamic Brotli Settings
brotli on;
brotli_comp_level 5;
brotli_min_length 1024;
brotli_types text/css application/javascript application/json image/svg+xml;

# 4. BREACH Mitigation
location /auth {
    gzip off;
    brotli off;
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_gzip_module Documentation](https://nginx.org/en/docs/http/ngx_http_gzip_module.html)
- [Official NGINX ngx_http_gzip_static_module Documentation](https://nginx.org/en/docs/http/ngx_http_gzip_static_module.html)
- [Google Brotli Compression Specification (RFC 7932)](https://datatracker.ietf.org/doc/html/rfc7932)
- [BREACH Attack Research & Mitigation](https://breachattack.com/)
- [Google ngx_brotli Module GitHub Repository](https://github.com/google/ngx_brotli)

---

[← Previous: HTTP/2 & HTTP/3 QUIC](06-http2-and-http3-quic.md) | [Module Overview](README.md) | [Next: Headers, CORS, and Security Headers →](08-headers-cors-and-security-headers.md)
