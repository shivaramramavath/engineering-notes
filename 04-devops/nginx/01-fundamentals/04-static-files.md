<!--
================================================================================
Metadata:
  Title: Static Content Delivery: root vs. alias, MIME Types, Caching & SPA Fallbacks
  Track: NGINX & Traffic Engineering / DevOps
  Difficulty: Beginner to Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 01-fundamentals/03-server-and-location.md
  Key Terminology: root, alias, Off-by-Slash, sendfile, open_file_cache, MIME Types, autoindex, SPA Routing
================================================================================
-->

# Static Content Delivery: root vs. alias, MIME Types, Caching & SPA Fallbacks

[<- 03-server-and-location.md](./03-server-and-location.md) | [Back to Module Overview](./README.md) | [05-directives-reference-by-context.md ->](./05-directives-reference-by-context.md)

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

Serving static web content (HTML files, CSS stylesheets, client-side JavaScript bundles, images, videos, and fonts) was the original engineering design goal of NGINX. Linus Torvalds and Igor Sysoev recognized that traditional thread-per-connection servers (such as Apache MPM Worker/Prefork) spent enormous CPU and memory overhead managing process stacks simply waiting on slow disk I/O and slow network clients.

NGINX delivers static content orders of magnitude faster by combining non-blocking asynchronous event loops with operating system kernel primitives:
- **`sendfile` zero-copy transfer**: Streams bytes directly from disk page cache to network interface cards (NICs) without copying data into userspace memory.
- **`open_file_cache`**: Keeps file descriptors, stat metadata, and directory structures cached in RAM to eliminate repeated `stat()` system call overhead.
- **MIME mapping**: Maps file extensions to standard IANA media types, preventing security warnings and script execution blocks in modern browsers.

Understanding static delivery also involves mastering the fundamental difference between **`root`** and **`alias`**, including preventing the infamous **Off-by-Slash Path Traversal** vulnerability that has compromised thousands of production servers.

---

## 2. Core Concepts

### `root` vs. `alias`: The Fundamental Distinction
The single most frequent mistake in NGINX static hosting is confusing `root` and `alias`:

1. **`root` (Append URI)**:
   - NGINX constructs the physical file path by **appending the entire request URI** to the `root` path.
   - Formula: $\text{File Path} = \text{root} + \text{URI}$
   ```nginx
   location /static/ {
       root /var/www/app;
   }
   # Request: /static/css/style.css
   # Disk Path: /var/www/app/static/css/style.css
   ```

2. **`alias` (Replace Location Prefix)**:
   - NGINX constructs the physical file path by **replacing the matching location prefix** with the `alias` path.
   - Formula: $\text{File Path} = \text{alias} + (\text{URI} - \text{location prefix})$
   ```nginx
   location /static/ {
       alias /var/www/app/assets/;
   }
   # Request: /static/css/style.css
   # Disk Path: /var/www/app/assets/css/style.css
   ```

### The "Off-by-Slash" Path Traversal Vulnerability
When using `alias`, omitting the trailing slash on the location path creates a severe security vulnerability:

```nginx
# VULNERABLE CONFIGURATION:
location /files {
    alias /var/www/files/;
}
```

If an attacker requests:
`GET /files../secret.txt HTTP/1.1`
NGINX replaces `/files` with `/var/www/files/` and appends `../secret.txt`:
Resulting Disk Path: `/var/www/files/../secret.txt` $\rightarrow$ `/var/www/secret.txt`!
The attacker successfully traverses outside the intended directory to read arbitrary files!

*Mandatory Invariant*: When using `alias`, **always ensure trailing slash parity**:
```nginx
location /files/ {
    alias /var/www/files/;
}
```

---

## 3. How It Works Internally

### Kernel Zero-Copy Architecture (`sendfile`)
In traditional web servers, transmitting a file requires multiple context switches and memory copies between user space and kernel space:

```
+--------------------------------------------------------------------------+
| TRADITIONAL FILE TRANSMISSION (4 Memory Copies, 4 Context Switches)       |
|                                                                          |
| [ Disk ] ---> [ Kernel Page Cache ] ---> [ Userspace Buffer in App ]     |
|                                                  |                       |
| [ NIC Hardware ] <--- [ Socket Buffer (Kernel) ] <-------+               |
+--------------------------------------------------------------------------+
```

With NGINX **`sendfile on;`**, the kernel transmits bytes directly from the page cache to the network protocol engine via Direct Memory Access (DMA):

```
+--------------------------------------------------------------------------+
| NGINX ZERO-COPY sendfile (Zero Userspace Copies, 2 Context Switches)     |
|                                                                          |
| [ Storage Disk ]                                                         |
|       |                                                                  |
|       v (DMA Copy)                                                       |
| [ Kernel Page Cache ] ------------------------+                          |
|                                               | (Direct DMA Transfer)    |
|                                               v                          |
|                                     [ NIC Network Controller ]           |
|                                                                          |
| * Userspace memory is never touched! CPU utilization drops to near zero!  |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Core Directives for Static Hosting
```nginx
# Root directive (can be placed in http, server, or location)
root /var/www/html;

# Default index files to serve when a directory is requested
index index.html index.htm;

# Enable/Disable directory browsing
autoindex on;
autoindex_exact_size off; # Display human-readable sizes (e.g. 5 MB)
autoindex_format html;    # Options: html, xml, json, jsonp

# Zero-Copy and TCP packet optimization
sendfile on;
tcp_nopush on; # Sends HTTP response headers and beginning of file in one packet
tcp_nodelay on; # Disables Nagle's algorithm for keepalive connections

# Cache open file descriptors and stat lookups in memory
open_file_cache max=10000 inactive=20s;
open_file_cache_valid 30s;
open_file_cache_min_uses 2;
open_file_cache_errors on;
```

---

## 5. Practical Examples

### Example 1: Production Static Web Server with Optimal MIME Types
```nginx
server {
    listen 80;
    server_name static.example.com;

    root /var/www/static;
    index index.html;

    # Include official MIME type definitions
    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    # Zero-copy optimizations
    sendfile on;
    tcp_nopush on;

    # Fast-path cache for frequently accessed static descriptors
    open_file_cache max=5000 inactive=30s;
    open_file_cache_valid 60s;
    open_file_cache_min_uses 2;

    # Static Assets with 1-Year Immutable Caching
    location ^~ /assets/ {
        expires 1y;
        add_header Cache-Control "public, immutable";
        access_log off;
    }

    # HTML Pages: No-Cache to ensure users always receive latest bundle index
    location ~* \.html$ {
        expires -1;
        add_header Cache-Control "no-store, no-cache, must-revalidate";
    }
}
```

### Example 2: Exploiting & Fixing the "Off-by-Slash" Alias Traversal
```bash
# 1. Directory setup on disk
sudo mkdir -p /var/www/public_data
sudo mkdir -p /var/www/private_secrets
echo "Public Information" | sudo tee /var/www/public_data/info.txt
echo "SUPER_SECRET_KEY=12345" | sudo tee /var/www/private_secrets/key.txt

# 2. VULNERABLE CONFIGURATION:
# location /data {
#     alias /var/www/public_data/;
# }

# Attacker runs:
# curl http://localhost/data../private_secrets/key.txt
# Output: SUPER_SECRET_KEY=12345 (CRITICAL SECURITY BREACH!)

# 3. REMEDIATION (Enforce Trailing Slash Parity):
# location /data/ {
#     alias /var/www/public_data/;
# }

# Attacker runs same curl:
# Output: 404 Not Found (Attack neutralized!)
```

### Example 3: Single Page Application (React / Vue) Host with SPA Fallback
```nginx
server {
    listen 80;
    server_name dashboard.example.com;

    root /var/www/dashboard/dist;
    index index.html;

    # Hashed JS and CSS chunks
    location ^~ /static/ {
        expires 1y;
        add_header Cache-Control "public, immutable";
        try_files $uri =404;
    }

    # Favicon and Robots
    location = /favicon.ico { access_log off; log_not_found off; }
    location = /robots.txt  { access_log off; log_not_found off; }

    # Client-side router catch-all
    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

### Example 4: Automated JSON API Directory Indexer (`autoindex_format json`)
```nginx
server {
    listen 80;
    server_name files.internal;

    root /data/exports;

    location /reports/ {
        autoindex on;
        autoindex_format json;
        autoindex_exact_size on;
    }
}

# Requesting curl -H "Accept: application/json" http://files.internal/reports/
# Returns structured JSON array of files:
# [
#   { "name":"q3_metrics.csv", "type":"file", "mtime":"Wed, 25 Nov 2026 12:00:00 GMT", "size":10485760 },
#   { "name":"logs", "type":"directory", "mtime":"Tue, 24 Nov 2026 08:30:00 GMT" }
# ]
```

### Example 5: High-Performance In-Memory File Descriptor Caching (`open_file_cache`)
```nginx
http {
    # Cache up to 10,000 open file descriptors
    # Remove from cache if not accessed within 60 seconds
    open_file_cache max=10000 inactive=60s;

    # Re-validate file modification timestamps every 30 seconds
    open_file_cache_valid 30s;

    # Only cache files accessed at least 2 times within the inactive period
    open_file_cache_min_uses 2;

    # Cache negative lookups (e.g., 404 Not Found) to stop repeated disk scans
    open_file_cache_errors on;
}
```

---

## 6. Best Practices

- **Prefer `root` Over `alias` Wherever Possible**: `root` is computationally simpler and mathematically immune to off-by-slash path traversal attacks. Only use `alias` when the filesystem path does not match the URL prefix.
- **Always Pair `sendfile on;` with `tcp_nopush on;`**: `sendfile` enables kernel zero-copy; `tcp_nopush` ensures that the HTTP header packet and the file payload are coalesced into full MSS (Maximum Segment Size) network packets, reducing packet transmission overhead.
- **Differentiate Caching for Hashed vs. Unhashed Assets**:
  - Hashed bundles (e.g., `main.a8f9c2.js`): Serve with `Cache-Control: public, max-age=31536000, immutable`.
  - Root `index.html`: Serve with `Cache-Control: no-cache`. If clients cache `index.html`, they will not see new deployments because they will keep loading stale hashed script names.
- **Enable `open_file_cache` on High-Throughput Servers**: If NGINX serves thousands of static files per second, `open_file_cache` prevents the OS kernel from executing repetitive `stat()`, `open()`, and `close()` system calls.
- **Disable Access Logs on High-Traffic Static Assets**: Logging every image, CSS, and favicon request to disk burns enormous I/O bandwidth. Add `access_log off;` to static location blocks.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| Off-by-slash path traversal on `alias` | Missing trailing slash on location permits directory climbing (`/files../`). | Ensure trailing slashes on both location and alias: `location /files/ { alias /dir/; }`. |
| `alias` inside regex location without captures | Using `alias /dir/` inside `location ~ \.jpg$` loses URI path context. | Use named regex captures: `location ~ ^/img/(?<file>.*)$ { alias /data/$file; }`. |
| Missing `mime.types` causes browser to download CSS as binary | Response is sent as `application/octet-stream` because `include mime.types;` is missing. | Include MIME types in `http {}`: `include /etc/nginx/mime.types;`. |
| Stale files served after update with `open_file_cache` | NGINX trusts cached descriptor timestamps until `open_file_cache_valid` interval expires. | Lower `open_file_cache_valid` (e.g. 10s–30s) or reload NGINX upon deployment. |
| SPA fallback returns 500 Internal Server Error | `try_files $uri $uri/ index.html;` (missing leading slash on index) creates an invalid internal path. | Add leading slash: `try_files $uri $uri/ /index.html;`. |
| Broken fonts or images with `root` due to duplicate folder names | Defining `root /var/www/static` inside `location /static/` looks for `/var/www/static/static/`. | Set `root /var/www;` so `/static/` appends cleanly. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Standard userspace copying utilizes significant CPU time moving memory buffers between kernel page cache and application RAM. `sendfile` utilizes zero userspace CPU copies. A server using `sendfile` and `open_file_cache` can easily saturate a 10 Gbps network interface while maintaining < 5% CPU utilization.
- **Security**: The Off-by-slash `alias` traversal vulnerability is ranked among the top NGINX CVE findings. It allows unauthenticated remote attackers to read configuration files, source code, and database backups located adjacent to the public webroot.
- **Cost**: Serving static content directly through NGINX instead of passing requests to upstream Node.js, Python, or Ruby application workers reduces required application instances by up to 80%, substantially reducing monthly cloud compute infrastructure bills.

---

## 9. Comparison Table

| Attribute | `root` Directive | `alias` Directive |
| :--- | :--- | :--- |
| **Path Assembly Rule** | Appends full URI to path | Replaces matching location prefix |
| **Trailing Slash Sensitivity**| Not sensitive to trailing slashes | **Extremely sensitive** (Off-by-slash vulnerability) |
| **Contexts Allowed** | `http`, `server`, `location`, `if in location` | `location` strictly |
| **Regex Support** | Standard | Requires manual capture variables (`$1`, `$file`) |
| **Security Risk** | Low (path traversal protected by VFS) | High (if trailing slashes mismatch) |
| **Recommended Usage** | Default choice for all standard paths | Reserved for path rewriting where URL $\neq$ disk path |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the exact operational difference between `root` and `alias`. Provide concrete examples of path resolution.</strong></summary>

**Answer**:
- **`root`**: The request URI is appended directly to the `root` path.
  ```nginx
  location /assets/ {
      root /var/www/data;
  }
  ```
  If a client requests `/assets/css/style.css`, NGINX appends the full URI:
  Resolved Path: `/var/www/data/assets/css/style.css`.
- **`alias`**: The location prefix is stripped from the request URI, and the remaining portion is appended to the `alias` path.
  ```nginx
  location /assets/ {
      alias /var/www/data/;
  }
  ```
  If a client requests `/assets/css/style.css`, NGINX strips `/assets/` and replaces it with `/var/www/data/`:
  Resolved Path: `/var/www/data/css/style.css`.
</details>

<details>
<summary><strong>2. What is the "Off-by-Slash" vulnerability in NGINX and how is it exploited?</strong></summary>

**Answer**:
The Off-by-slash vulnerability occurs when an `alias` directive has a trailing slash, but the corresponding `location` directive does not:
```nginx
location /files {
    alias /var/www/files/;
}
```
Because `/files` does not end with `/`, a request for `/files../secret.txt` matches the location `/files`.
NGINX performs literal string replacement: it replaces the matched prefix `/files` with `/var/www/files/` and appends the remainder `../secret.txt`.
The resulting path is:
`/var/www/files/../secret.txt` $\rightarrow$ resolves to `/var/www/secret.txt`.

This allows an attacker to traverse out of `/var/www/files/` and read arbitrary files in parent or sibling directories. The remediation is to enforce trailing slash parity: `location /files/ { alias /var/www/files/; }`.
</details>

<details>
<summary><strong>3. How does `sendfile` improve static file performance at the Linux kernel level?</strong></summary>

**Answer**:
Under traditional read/write system calls:
1. The kernel reads data from the storage drive into the kernel page cache (DMA transfer).
2. The `read()` syscall copies data from kernel page cache into userspace application memory buffers.
3. The `write()` syscall copies data from userspace application memory buffers back into the kernel network socket buffer.
4. The network controller reads data from socket buffer to transmit over the network (DMA transfer).
This incurs 4 context switches and 4 memory copies (2 over the CPU memory bus).

`sendfile` invokes the `sendfile()` system call. The kernel transfers data directly from the page cache into the network interface controller buffer via DMA, bypassing user space completely. This reduces the operation to 2 context switches and 0 CPU memory copies (Zero-Copy), drastically reducing CPU load and memory bus contention.
</details>

<details>
<summary><strong>4. What is the function of `open_file_cache` and when should an SRE enable it?</strong></summary>

**Answer**:
When NGINX serves a static file, it executes several system calls: `stat()` to check file existence and permissions, `open()` to acquire a file descriptor, and `close()` upon completion. Under thousands of requests per second, repetitive filesystem metadata lookups saturate the kernel VFS layer.

`open_file_cache` caches:
1. Open file descriptors and file sizes.
2. Modification timestamps (`mtime`).
3. Directory existence metadata.
4. Error lookups (e.g., negative caching for 404s).

An SRE should enable `open_file_cache` on high-concurrency static file servers or media CDNs. It dramatically reduces disk I/O and kernel lock contention.
</details>

<details>
<summary><strong>5. Why is `try_files $uri $uri/ /index.html;` required for modern Single Page Applications (React, Vue, Angular)?</strong></summary>

**Answer**:
Modern SPAs use client-side routing (HTML5 History API): URLs like `/dashboard/settings` or `/users/42` do not exist as physical files or directories on the NGINX server disk. Only a single physical file exists: `/index.html`.

If a user navigates directly to `http://example.com/dashboard/settings`:
1. Without `try_files`, NGINX looks for `/var/www/dashboard/settings` on disk, finds nothing, and returns `404 Not Found`.
2. With `try_files $uri $uri/ /index.html;`:
   - NGINX checks if `$uri` is a physical file (e.g., a `.js` bundle or image).
   - NGINX checks if `$uri/` is a physical directory.
   - If neither exists, NGINX internally falls back to serving `/index.html`.
3. The client browser loads `index.html`, boots the React/Vue JavaScript engine, and the client-side router inspects `window.location.pathname` to render the correct view.
</details>

<details>
<summary><strong>6. What are the roles of `tcp_nopush` and `tcp_nodelay` in static asset delivery?</strong></summary>

**Answer**:
- **`tcp_nopush on;`**: Only functional when `sendfile on;` is enabled. It tells the Linux kernel (via the `TCP_CORK` socket option) to wait until an entire network packet is filled to the Maximum Segment Size (MSS) before sending it. This packages the HTTP response headers and the initial chunk of the file into a single packet, maximizing network wire efficiency.
- **`tcp_nodelay on;`**: Disables Nagle's algorithm (via `TCP_NODELAY`), which normally buffers small packets for up to 200 ms to combine them. When `tcp_nodelay` is enabled, NGINX transmits data immediately without artificial latency.

NGINX smartly combines both: it uses `tcp_nopush` to send the main file payload in full packets, and then releases the cork to let `tcp_nodelay` transmit the final trailing fragment immediately.
</details>

<details>
<summary><strong>7. How do you serve pre-compressed gzip files (`.gz`) without incurring CPU compression overhead?</strong></summary>

**Answer**:
By enabling the `gzip_static` directive (provided by `ngx_http_gzip_static_module`):
```nginx
location /static/ {
    gzip_static on;
}
```
When a client sends `Accept-Encoding: gzip`, NGINX checks if a pre-compressed `.gz` file exists on disk (e.g., if requesting `bundle.js`, it checks for `bundle.js.gz`).
If found:
1. NGINX serves the `.gz` file directly via `sendfile`.
2. Sets `Content-Encoding: gzip`.
3. Consumes **0% CPU** for compression.

This allows build pipelines (Webpack, Vite) to compress assets at build time using maximum compression levels (e.g. gzip -9 or Zopfli) while NGINX serves them with zero runtime CPU tax.
</details>

<details>
<summary><strong>8. What is the security risk of configuring `autoindex on;` in production?</strong></summary>

**Answer**:
`autoindex on;` enables directory listing: if a requested directory does not contain an `index.html` file, NGINX generates an HTML page listing all files and subdirectories.

In production, leaving `autoindex on;` exposes internal assets to automated reconnaissance:
- Attackers can discover database backups (`backup.sql`), editor swap files (`.swp`), environment secrets (`.env`), or private configuration templates.
- It exposes directory hierarchy and internal naming conventions.

`autoindex` should be strictly disabled (`autoindex off;`) in production web applications, and enabled only on dedicated internal file distribution or package repository mirrors.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Identify Resolved File Path for `root` vs `alias`</strong></summary>

**Task**: Calculate the exact disk file path for the request `GET /docs/setup/install.html` under both configurations:
1. `location /docs/ { root /opt/web; }`
2. `location /docs/ { alias /opt/web/manuals/; }`

**Solution**:
1. With `root`: `/opt/web/docs/setup/install.html` (appends full URI).
2. With `alias`: `/opt/web/manuals/setup/install.html` (replaces `/docs/` with `/opt/web/manuals/`).
</details>

<details>
<summary><strong>Exercise 2 (Easy): Configure Zero-Copy Primitives</strong></summary>

**Task**: Write the configuration snippet to enable kernel zero-copy file transmission, packet coalescing, and disable Nagle's algorithm inside the `http {}` block.

**Solution**:
```nginx
http {
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Neutralize Off-by-Slash Path Traversal</strong></summary>

**Task**: Given the vulnerable block below, correct the syntax to guarantee path traversal attacks are blocked.

```nginx
location /exports {
    alias /var/data/exports/;
}
```

**Solution**:
Enforce trailing slash parity on the location match:
```nginx
location /exports/ {
    alias /var/data/exports/;
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Build an Open File Descriptor Cache Policy</strong></summary>

**Task**: Author an `open_file_cache` block that tracks up to 2,000 active file descriptors, discards entries idle for more than 45 seconds, re-evaluates file validity every 15 seconds, and caches negative lookups (404s).

**Solution**:
```nginx
open_file_cache max=2000 inactive=45s;
open_file_cache_valid 15s;
open_file_cache_min_uses 1;
open_file_cache_errors on;
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Differentiated Caching for Single Page Applications</strong></summary>

**Task**: Author a complete NGINX virtual host for a React Single Page Application that:
1. Serves files from `/var/www/react-app/build`.
2. Serves `index.html` with headers preventing any caching.
3. Serves assets inside `/static/` with 1-year immutable caching.
4. Correctly falls back to `/index.html` for client-side routing.

**Solution**:
```nginx
server {
    listen 80;
    server_name portal.internal;

    root /var/www/react-app/build;
    index index.html;

    # Static bundled assets with hash in filename
    location ^~ /static/ {
        expires 1y;
        add_header Cache-Control "public, max-age=31536000, immutable";
        access_log off;
    }

    # SPA routing fallback
    location / {
        try_files $uri $uri/ /index.html;
    }

    # Ensure index.html is never cached by browsers
    location = /index.html {
        expires -1;
        add_header Cache-Control "no-store, no-cache, must-revalidate, proxy-revalidate, max-age=0";
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Script a Vulnerability Scanner for Alias Traversal</strong></summary>

**Task**: Write a bash script that parses an NGINX configuration directory and flags any `location` block using `alias` where the location URI lacks a trailing slash while the alias path has one.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

echo "Auditing NGINX configurations for Off-by-Slash vulnerabilities..."
grep -rnP 'location\s+/[^/\s]+\s+\{\s*alias\s+[^;]+/\s*;' /etc/nginx/ || true

# More granular awk check across multi-line files:
awk '
/location[ \t]+[^\^~][^ \t{]+/ {
    loc = $2
}
/alias[ \t]+[^;]+;/ {
    alias_path = $2
    if (loc !~ /\/$/ && alias_path ~ /\/$/) {
        printf "VULNERABILITY DETECTED in %s: location %s -> alias %s (Off-by-slash!)\n", FILENAME, loc, alias_path
    }
}
' /etc/nginx/conf.d/*.conf 2>/dev/null || echo "No vulnerable configurations found."
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Directive | Syntax Example | Meaning |
| :--- | :--- | :--- |
| **`root`** | `root /var/www;` | Appends full URI to directory path |
| **`alias`** | `alias /var/www/files/;` | Replaces location prefix with directory path |
| **`sendfile`** | `sendfile on;` | Enables kernel zero-copy DMA transfer |
| **`tcp_nopush`** | `tcp_nopush on;` | Coalesces headers and payload into full packets |
| **`open_file_cache`** | `open_file_cache max=1000 ...;` | Caches file descriptors in memory |
| **`autoindex`** | `autoindex on;` | Enables directory browsing (disable in prod) |
| **`try_files`** | `try_files $uri $uri/ /index.html;` | Sequential file check with fallback |

---

## 13. Further Reading & Official Documentation

- [Official NGINX Documentation: Serving Static Content](https://nginx.org/en/docs/http/load_balancing.html)
- [Official NGINX Documentation: ngx_http_core_module (root and alias)](https://nginx.org/en/docs/http/ngx_http_core_module.html#root)
- [Zero-Copy: Architecture and Linux Kernel System Calls](https://www.linuxjournal.com/article/6345)
- [Detecting and Preventing NGINX Alias Traversal Attacks](https://blog.detectify.com/2020/11/10/common-nginx-misconfigurations/)
- [RFC 9110: HTTP Semantics and Content Negotiation](https://www.rfc-editor.org/rfc/rfc9110.html)

---

[<- 03-server-and-location.md](./03-server-and-location.md) | [Back to Module Overview](./README.md) | [05-directives-reference-by-context.md ->](./05-directives-reference-by-context.md)
