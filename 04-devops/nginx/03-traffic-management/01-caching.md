---
title: "HTTP Caching and Microcaching in NGINX"
description: "Mastering NGINX reverse proxy caching: proxy_cache_path architecture, microcaching, cache locking, stale-while-revalidate, slice range caching, and cache purging strategies."
module: "03-traffic-management"
track: "devops-nginx"
order: 1
difficulty: "Intermediate to Advanced"
prerequisites:
  - "01-fundamentals/03-server-and-location"
  - "02-core-features/02-reverse-proxy"
tags:
  - nginx
  - caching
  - proxy-cache
  - microcaching
  - stale-while-revalidate
  - cache-purge
  - performance
last_updated: "2026-03-30"
---

# HTTP Caching and Microcaching in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Production Multi-Tier Reverse Proxy Cache](#example-1-production-multi-tier-reverse-proxy-cache)
  - [Example 2: 1-Second Dynamic Microcaching for Extreme Concurrency](#example-2-1-second-dynamic-microcaching-for-extreme-concurrency)
  - [Example 3: Stale-While-Revalidate and Thundering Herd Defense (`proxy_cache_lock`)](#example-3-stale-while-revalidate-and-thundering-herd-defense-proxy_cache_lock)
  - [Example 4: Conditional Bypass via Cookies and Auth Tokens](#example-4-conditional-bypass-via-cookies-and-auth-tokens)
  - [Example 5: Byte-Range Media Caching with the Slice Module](#example-5-byte-range-media-caching-with-the-slice-module)
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

In modern distributed architectures, the database and backend application runtime (Node.js, Python, Java, Go) are the primary throughput bottlenecks. Under heavy traffic surges (breaking news, flash sales, viral social media posts), hitting the backend for every incoming HTTP GET request leads to thread pool exhaustion, database connection pool starvation, high latencies, and eventual cascading service failure.

NGINX provides an enterprise-grade, memory-mapped HTTP caching engine. By acting as a **caching reverse proxy**, NGINX:
1. **Absorbs 95%+ of Read Requests**: Serves responses directly from local disk and the OS page cache in microseconds without contacting backend microservices.
2. **Mitigates the "Thundering Herd" (Cache Stampede)**: Consolidates hundreds of simultaneous requests for an expired resource into a single backend fetch using `proxy_cache_lock`.
3. **Implements High-Availability Fallback**: Serves stale cached content if the backend upstream server crashes or times out (`proxy_cache_use_stale`).
4. **Enables Dynamic Microcaching**: Caches dynamically generated JSON API or HTML responses for as little as 1 to 5 seconds, boosting throughput from 200 req/sec to 15,000+ req/sec while keeping data fresh for end users.

---

## 2. Core Concepts

### Two-Tier Storage Architecture: Shared Memory vs Disk
NGINX implements an asynchronous, dual-tier caching model:
- **Shared Memory Zone (`keys_zone`)**: A dedicated slab in RAM shared across all NGINX worker processes. It stores metadata only: MD5 cache keys, file pointers, access counters, expiration timers, and CRC32 checksums. (1MB of RAM holds ~8,000 cached objects).
- **Filesystem Disk Storage (`path`)**: The actual HTTP response headers and body bytes are written to disk. The operating system's kernel page cache keeps hot response bodies memory-mapped in RAM automatically, providing memory-speed reads without consuming NGINX user-space process heap.

### Directory Hash Levels (`levels=1:2`)
Storing 100,000 files in a single flat directory causes severe filesystem directory lookup contention in Linux (ext4/xfs). The `levels` parameter instructs NGINX to organize files into nested subdirectories using the MD5 hash of the cache key:
- `levels=1:2`: Creates a 1-character parent directory and a 2-character child directory.
- Example: An MD5 hash of `b7f5f24f02c30a1a30dd1409db4f2f31` is stored at:
  `/var/cache/nginx/1/f3/b7f5f24f02c30a1a30dd1409db4f2f31`

### The `$upstream_cache_status` Variable
NGINX exposes the exact result of the cache lookup via the `$upstream_cache_status` variable. Tracking this value is essential for operational observability:
- `HIT`: The response was served directly from the NGINX cache.
- `MISS`: The requested resource was not in the cache; it was fetched from the upstream and stored in the cache.
- `BYPASS`: The cache was intentionally skipped due to a matching `proxy_cache_bypass` directive.
- `EXPIRED`: The cached object had expired; it was re-fetched from the upstream and replaced.
- `STALE`: The upstream backend was unreachable or returned an error; NGINX served an expired stale response because `proxy_cache_use_stale` was enabled.
- `UPDATING`: The cache item is expired, but another worker is currently fetching a new version; NGINX served stale data because `proxy_cache_use_stale updating` was enabled.
- `REVALIDATED`: NGINX validated the cached object using `If-Modified-Since` or `If-None-Match`, and the upstream returned `304 Not Modified`.

---

## 3. How It Works Internally

### The Cache Lookup and Loader Process State Machine

```
                        +----------------------------------------+
                        | Master Process Spawns Helper Processes |
                        +-------------------+--------------------+
                                            |
                    +-----------------------+-----------------------+
                    |                                               |
+-------------------v--------------------+      +-------------------v--------------------+
| Cache Loader Process (ngx_cache_loader)|      | Cache Manager Process (ngx_cache_mgr)  |
| - Runs once at startup/reload          |      | - Wakes up periodically on timer       |
| - Scans disk files into RAM keys_zone  |      | - Enforces max_size via LRU eviction   |
| - Terminates when disk scan is complete|      | - Prunes inactive files from disk      |
+----------------------------------------+      +----------------------------------------+
```

### Request Lifecycle through the Cache Engine

```
Client GET /products/42
           |
           v
Compute MD5 Hash of proxy_cache_key
(e.g., "GEThttp://example.com/products/42")
           |
           v
Lookup MD5 in Shared Memory Zone (RAM)
        /                             \
     FOUND                           NOT FOUND
      /                                 \
   Expired?                              v
   /      \                     Fetch from Upstream Backend
  NO       YES                           |
  |         \                            v
  |          +---> Can use stale?    Write to temp file on disk
  |                /           \         |
  |              YES            NO       v
  |               |              |   Atomic rename to cache file
  |               v              |   Insert MD5 into keys_zone
  |        Serve Stale Body      v       |
  |                              Fetch fresh from upstream
  |                                      |
  +--------------------------------------+
           |
           v
Stream response to client (Set X-Cache-Status header)
```

---

## 4. Syntax & Directives Reference

| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `proxy_cache_path path [levels=levels] keys_zone=name:size [inactive=time] [max_size=size] [use_temp_path=off]` | `http` | None | Defines disk cache location, directory hierarchy, RAM zone name and size, inactivity eviction, and disk cap. |
| `proxy_cache zone\|off` | `http`, `server`, `location` | `off` | Activates the specified shared memory cache zone. |
| `proxy_cache_key string` | `http`, `server`, `location` | `$scheme$proxy_host$request_uri` | String hashed (MD5) to identify unique cache entries. |
| `proxy_cache_valid [code ...] time` | `http`, `server`, `location` | None | Sets time-to-live (TTL) for specific HTTP response status codes. |
| `proxy_cache_bypass string ...` | `http`, `server`, `location` | None | If at least one value is non-empty and not "0", cache is skipped and fetched from upstream. |
| `proxy_no_cache string ...` | `http`, `server`, `location` | None | If at least one value is non-empty and not "0", the response received from upstream is **not saved** to cache. |
| `proxy_cache_use_stale error\|timeout\|invalid_header\|updating\|http_500\|...` | `http`, `server`, `location` | `off` | Serves stale cached content during upstream failures or updates. |
| `proxy_cache_lock on\|off` | `http`, `server`, `location` | `off` | Prevents thundering herd: only one request populates the cache for a given key. |
| `proxy_cache_lock_timeout time` | `http`, `server`, `location` | `5s` | Timeout waiting for lock before secondary requests pass directly to upstream. |
| `proxy_cache_revalidate on\|off` | `http`, `server`, `location` | `off` | Enables conditional revalidation (`If-Modified-Since` / `ETag`) of expired items. |
| `proxy_cache_min_uses number` | `http`, `server`, `location` | `1` | Number of requests before an object is written to disk (prevents caching one-hit wonders). |

---

## 5. Practical Implementation Examples

### Example 1: Production Multi-Tier Reverse Proxy Cache

An enterprise configuration defining a 50GB disk cache with a 100MB shared memory zone, storing static assets and API payloads.

```nginx
# /etc/nginx/conf.d/caching_production.conf

# Define cache zone in http context
# 100MB RAM holds ~800,000 keys; 50GB disk max; inactive after 7 days
proxy_cache_path /var/cache/nginx/prod_cache
                 levels=1:2
                 keys_zone=PROD_CACHE:100m
                 max_size=50g
                 inactive=7d
                 use_temp_path=off;

server {
    listen 443 ssl;
    http2 on;
    server_name api.example.com;

    ssl_certificate     /etc/ssl/certs/api.crt;
    ssl_certificate_key /etc/ssl/private/api.key;

    # Expose cache hit status in response headers
    add_header X-Cache-Status $upstream_cache_status always;

    location / {
        proxy_pass http://backend_upstream;
        proxy_set_header Host $host;

        # Activate cache
        proxy_cache PROD_CACHE;
        proxy_cache_key $scheme$request_method$host$request_uri;

        # TTL Rules
        proxy_cache_valid 200 302 10m;
        proxy_cache_valid 404      1m;
        proxy_cache_valid 500 502  0s; # Never cache server errors

        # Cache Lock (Thundering Herd Protection)
        proxy_cache_lock on;
        proxy_cache_lock_timeout 5s;

        # Revalidation using ETag / Last-Modified
        proxy_cache_revalidate on;

        # Keep alive to upstream
        proxy_http_version 1.1;
        proxy_set_header Connection "";
    }
}
```

### Example 2: 1-Second Dynamic Microcaching for Extreme Concurrency

Microcaching dynamic, high-traffic pages (such as news feeds, stock tickers, or sports scores) for 1 second. Under 10,000 requests per second, the backend handles only 1 request per second while 9,999 requests are served from cache.

```nginx
# /etc/nginx/conf.d/microcache.conf

proxy_cache_path /var/cache/nginx/microcache
                 levels=1:2
                 keys_zone=MICROCACHE:20m
                 max_size=2g
                 inactive=60s
                 use_temp_path=off;

server {
    listen 80;
    server_name news.example.com;

    location /feed {
        proxy_pass http://news_node_cluster;
        proxy_set_header Host $host;

        proxy_cache MICROCACHE;
        proxy_cache_key $request_method$host$request_uri;

        # Cache 200 OK responses for EXACTLY 1 second
        proxy_cache_valid 200 1s;

        # Serve stale while background update runs
        proxy_cache_use_stale updating error timeout http_500 http_502 http_503 http_504;
        proxy_cache_background_update on;
        proxy_cache_lock on;

        add_header X-Microcache $upstream_cache_status always;
    }
}
```

### Example 3: Stale-While-Revalidate and Thundering Herd Defense (`proxy_cache_lock`)

Ensures zero user-perceived latency during cache expiration: users always receive instant responses while NGINX updates the cache asynchronously in the background.

```nginx
# /etc/nginx/conf.d/stale_revalidate.conf

server {
    listen 80;
    server_name catalog.example.com;

    location /products/ {
        proxy_pass http://catalog_service;
        proxy_cache PROD_CACHE;

        # Standard TTL
        proxy_cache_valid 200 1h;

        # Serve stale content if backend is updating or failing
        proxy_cache_use_stale updating error timeout http_502 http_503 http_504;

        # Update cache in the background (asynchronous fetch)
        proxy_cache_background_update on;

        # If 500 users request an expired item at the exact same millisecond,
        # only ONE request hits the backend; the other 499 wait for the lock
        # or receive the stale cached copy immediately!
        proxy_cache_lock on;
        proxy_cache_lock_age 5s;
        proxy_cache_lock_timeout 5s;

        add_header X-Cache-Status $upstream_cache_status always;
    }
}
```

### Example 4: Conditional Bypass via Cookies and Auth Tokens

Automatically bypass the cache for logged-in users, shopping cart sessions, or explicit developer bypass headers, while caching public visitor traffic.

```nginx
# /etc/nginx/conf.d/bypass_logic.conf

# 1. Determine if cache should be bypassed
map $http_cookie $bypass_cookie {
    default                 0;
    "~*session_id|auth_token|cart" 1; # Logged-in or active shopping cart
}

map $http_authorization $bypass_auth {
    default 0;
    "~.+"   1; # Bearer token present
}

server {
    listen 80;
    server_name store.example.com;

    location / {
        # Combine bypass flags
        set $skip_cache 0;
        if ($bypass_cookie = 1) {
            set $skip_cache 1;
        }
        if ($bypass_auth = 1) {
            set $skip_cache 1;
        }
        if ($http_x_bypass_cache = "secret-debug-key") {
            set $skip_cache 1;
        }

        proxy_pass http://store_backend;
        proxy_cache PROD_CACHE;

        # proxy_cache_bypass: Don't read from cache if $skip_cache == 1
        proxy_cache_bypass $skip_cache;

        # proxy_no_cache: Don't write to cache if $skip_cache == 1
        proxy_no_cache $skip_cache;

        proxy_cache_valid 200 30m;
        add_header X-Cache-Status $upstream_cache_status always;
        add_header X-Cache-Bypassed $skip_cache always;
    }
}
```

### Example 5: Byte-Range Media Caching with the Slice Module

Standard NGINX caching caches entire files. If a client streams a 2GB video file and jumps to minute 40 via HTTP `Range: bytes=500000000-`, standard proxy caching either downloads the entire 2GB file before responding or refuses to cache. The `ngx_http_slice_module` splits large files into uniform sub-chunks (e.g., 1MB slices) and caches each slice independently.

```nginx
# /etc/nginx/conf.d/slice_caching.conf

proxy_cache_path /var/cache/nginx/media_cache
                 levels=1:2
                 keys_zone=MEDIA_CACHE:50m
                 max_size=100g
                 inactive=30d
                 use_temp_path=off;

server {
    listen 80;
    server_name video.example.com;

    location /media/ {
        proxy_pass http://media_storage_backend;

        # Slice large files into 1MB chunks
        slice 1m;

        proxy_cache MEDIA_CACHE;

        # Crucial: Include $slice_range in the cache key!
        proxy_cache_key $uri$is_args$args$slice_range;

        # Pass the calculated slice range to the upstream backend
        proxy_set_header Range $slice_range;

        # If-Range support
        proxy_cache_valid 200 206 30d;

        add_header X-Cache-Status $upstream_cache_status always;
    }
}
```

---

## 6. Production Best Practices

1. **Always Set `use_temp_path=off`**: In modern NGINX, always specify `use_temp_path=off` in `proxy_cache_path`. When enabled, NGINX writes the temporary response file directly inside the final cache directory tree and performs an instantaneous atomic `rename()` on the same filesystem. Omitting this causes NGINX to write to `/var/lib/nginx/tmp` and perform an expensive cross-filesystem copy to `/var/cache/nginx`.
2. **Always Enable `proxy_cache_lock on;`**: This is your primary defense against cache stampedes. Without it, when a popular cache item expires, hundreds of simultaneous client requests hit the upstream backend simultaneously.
3. **Include `$request_method` in `proxy_cache_key`**: If your cache key only includes `$host$request_uri`, a `HEAD` request could populate the cache with an empty body, causing subsequent `GET` requests to return an empty body.
4. **Always Expose `X-Cache-Status`**: Add `add_header X-Cache-Status $upstream_cache_status always;` across all cached virtual hosts. It is the single most valuable debugging tool for frontend and DevOps teams.
5. **Separate Static and Microcache Zones**: Use separate `proxy_cache_path` directives for long-lived static assets (large disk size, long inactivity) and fast dynamic microcaching (small disk size, aggressive eviction).
6. **Honor Upstream `Cache-Control` Headers**: By default, NGINX respects `Cache-Control: no-cache`, `no-store`, and `private` sent by the upstream backend. If you must override them, use `proxy_ignore_headers Cache-Control Expires Set-Cookie;`.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Omitting `use_temp_path=off`** | NGINX writes files to the default temp partition and performs a full disk copy to the cache partition, doubling disk I/O. | Specify `use_temp_path=off` on `proxy_cache_path`. |
| **Omitting `proxy_cache_lock` on high-traffic sites** | When a cached item expires, thousands of concurrent requests flood the backend at once (Thundering Herd), causing backend outages. | Enable `proxy_cache_lock on;` with `proxy_cache_lock_timeout 5s;`. |
| **Caching responses containing `Set-Cookie`** | NGINX caches the upstream `Set-Cookie` header and serves the first user's session cookie to all subsequent users, leaking personal accounts. | Add `proxy_ignore_headers Set-Cookie;` and `proxy_hide_header Set-Cookie;`, or prevent backend from emitting cookies on public pages. |
| **Omitting `$slice_range` from cache key when using `slice`** | Every range request overwrites the same cache key with different chunks, resulting in corrupt, scrambled video or binary downloads. | Always use `proxy_cache_key $uri$is_args$args$slice_range;`. |
| **Forgetting to set `proxy_no_cache` alongside `proxy_cache_bypass`** | Request bypasses cache, but NGINX writes the authenticated private user response back into the public cache. | Always set both `proxy_cache_bypass $skip;` and `proxy_no_cache $skip;`. |
| **Under-sizing `keys_zone`** | RAM zone fills up; NGINX stops adding new cache entries and logs: `could not allocate node in cache keys zone`. | Allocate 1MB of keys_zone RAM for every 8,000 intended cached files. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Verify Cache Status Header via curl
curl -s -I https://api.example.com/products/123 | grep -i x-cache-status
# First request should return: X-Cache-Status: MISS
# Second request should return: X-Cache-Status: HIT

# 2. Inspect Cache Disk Consumption and Directory Structure
du -sh /var/cache/nginx/prod_cache
find /var/cache/nginx/prod_cache -type f | head -n 10

# 3. Read NGINX Cache File Header Internally (Inspect Key and Upstream Headers)
# NGINX prepends binary header + HTTP response headers at the top of cache files:
head -n 25 /var/cache/nginx/prod_cache/1/f3/b7f5f24f02c30a1a30dd1409db4f2f31

# 4. Purge Cache Manually in Open Source NGINX
# To invalidate all cache entries:
rm -rf /var/cache/nginx/prod_cache/*
# Invalidate specific URL by computing MD5 of cache key:
KEY="http://example.com/products/42"
MD5=$(echo -n "$KEY" | md5sum | awk '{print $1}')
find /var/cache/nginx/prod_cache -name "$MD5" -delete
```

### Memory Footprint Calculations
- **Shared Memory Zone Sizing**:
  $$\text{RAM Needed} = \left(\frac{\text{Expected Cached Files}}{8000}\right) \times 1\text{ MB}$$
  For 1,000,000 files: $1000000 / 8000 \approx 125\text{ MB}$. Setting `keys_zone=NAME:150m` guarantees ample headroom.

---

## 9. Architectural Comparison Table

| Feature / Trait | Standard Reverse Proxy Cache | Microcaching (1–5s) | Upstream Direct Serving |
| :--- | :--- | :--- | :--- |
| **Target Content** | Static assets, immutable API responses | Dynamic news, stock feeds, search queries | Authenticated user profiles, checkout |
| **Cache TTL** | 10 minutes to 30 days | 1 to 5 seconds | 0 seconds (No cache) |
| **Cache Stampede Risk** | High without `proxy_cache_lock` | Extreme without lock | N/A |
| **Backend Load Reduction**| 98%–99% | 90%–95% under high traffic | 0% |
| **Freshness Guarantee** | Eventual consistency | Near real-time (<2s lag) | 100% Real-time consistency |
| **Storage Medium** | Disk + Kernel Page Cache | Shared RAM / Tempfs | None |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: What is the architectural purpose of use_temp_path=off in proxy_cache_path?</strong></summary>

**Answer:**
When an NGINX worker receives a response from an upstream backend that needs to be cached, it cannot write directly to the final cache file because multiple workers or clients might attempt to read the incomplete file while it is still streaming.

Historically (when `use_temp_path=on` or omitted), NGINX wrote the response to a temporary directory (e.g., `/var/lib/nginx/tmp/`). Once the entire upstream response was received, NGINX moved the file to the cache path (`/var/cache/nginx/`). If `/var/lib/nginx` and `/var/cache/nginx` resided on different mount points or disk partitions, this move required a full byte-by-byte file copy across disks, causing severe I/O bottlenecks.

Setting `use_temp_path=off` instructs NGINX to write the temporary file directly inside the target cache directory itself. Once the response completes, NGINX executes an instantaneous, atomic POSIX `rename()` system call within the same filesystem, eliminating cross-device copying and reducing disk I/O by 50%.
</details>

<details>
<summary><strong>Q2: Explain how proxy_cache_lock mitigates the "Thundering Herd" (Cache Stampede) problem.</strong></summary>

**Answer:**
When a heavily accessed cache entry expires (or before it is first created), a sudden surge of concurrent requests (e.g., 5,000 simultaneous clients) will all experience a cache miss at the exact same moment. Without protection, NGINX would forward all 5,000 requests to the upstream backend simultaneously, easily crashing the application or database.

With `proxy_cache_lock on;`:
1. When a cache miss occurs, NGINX allows **only the first request** to pass through to the upstream server to fetch and populate the cache.
2. All subsequent 4,999 requests are queued and held in memory, waiting for the lock.
3. Once the first request completes and the cache file is written, NGINX releases the lock, and the 4,999 waiting requests are served **directly from the newly populated cache**.
4. If the upstream does not respond within `proxy_cache_lock_timeout` (default 5s), the waiting requests are released to fetch directly from upstream as a fail-safe.
</details>

<details>
<summary><strong>Q3: What is the difference between proxy_cache_bypass and proxy_no_cache?</strong></summary>

**Answer:**
- `proxy_cache_bypass $condition`: Governs **reading from the cache**. If `$condition` evaluates to non-empty and not `"0"`, NGINX skips looking in the cache and immediately forwards the request to the upstream backend.
- `proxy_no_cache $condition`: Governs **writing to the cache**. If `$condition` evaluates to non-empty and not `"0"`, NGINX will **not save** the upstream response into the cache.

In production, they are almost always used together:
```nginx
proxy_cache_bypass $cookie_nocache;
proxy_no_cache     $cookie_nocache;
```
If you only configure `proxy_cache_bypass`, an authenticated user's private data would bypass the cache, but NGINX would save that private response into the public cache, leaking sensitive data to subsequent visitors.
</details>

<details>
<summary><strong>Q4: Why is caching a response that includes a Set-Cookie header a severe security vulnerability, and how do you prevent it?</strong></summary>

**Answer:**
If an upstream application emits a `Set-Cookie: session_id=XYZ123` header on a page that NGINX caches, NGINX will save that header into the cache file on disk. When subsequent anonymous visitors request that page, NGINX will serve the cached response—including the `Set-Cookie` header—assigning the first user's session ID to all other users. This leads to account hijacking and session poisoning.

Prevention:
1. Ensure the upstream backend never emits `Set-Cookie` on cacheable public pages.
2. In NGINX, configure:
   ```nginx
   proxy_ignore_headers Set-Cookie;
   proxy_hide_header Set-Cookie;
   ```
   This prevents NGINX from considering `Set-Cookie` when deciding cacheability, and strips the header so it is never served to downstream clients from the cache.
</details>

<details>
<summary><strong>Q5: How does proxy_cache_use_stale updating improve user-perceived performance?</strong></summary>

**Answer:**
Under standard caching, when an item expires, the client whose request triggers the re-fetch must wait for the backend upstream to compute and return the new response, experiencing a latency spike.

With `proxy_cache_use_stale updating;`:
- When an expired item is requested, NGINX immediately serves the **existing expired (stale) cached copy** to that client with near-zero latency.
- Simultaneously, NGINX triggers an asynchronous background subrequest to the upstream backend to fetch and update the cache file.
- End users never experience latency spikes, achieving high perceived performance and constant sub-millisecond response times.
</details>

<details>
<summary><strong>Q6: What are the Cache Loader and Cache Manager processes in NGINX, and what are their responsibilities?</strong></summary>

**Answer:**
- **Cache Loader (`ngx_cache_loader`)**: A temporary process spawned by the master process at startup or configuration reload. Its sole job is to iterate through the directory structure on disk, parse existing cache file metadata, and populate the in-memory `keys_zone` shared memory table. Once the scan is complete, the Cache Loader process cleanly terminates.
- **Cache Manager (`ngx_cache_mgr`)**: A permanent daemon process that wakes up periodically. It monitors the disk consumption of the cache directories. If disk usage exceeds `max_size`, the Cache Manager executes an LRU (Least Recently Used) eviction algorithm to delete older cache files until usage drops below the limit. It also removes files that have not been accessed within the `inactive` time window.
</details>

<details>
<summary><strong>Q7: How does the slice module enable caching of multi-gigabyte media files?</strong></summary>

**Answer:**
Standard NGINX proxy caching caches responses in their entirety. If a user requests a 4GB video with a Range header (`Range: bytes=0-1048575`), NGINX would either download the entire 4GB video into the cache before returning the first megabyte, or bypass caching entirely.

The `ngx_http_slice_module` breaks large files into fixed-size ranges (e.g., `slice 1m;`). When a client requests a byte range, NGINX translates it into subrequests for the specific 1MB slice chunks required, caches each 1MB slice independently on disk with `$slice_range` in the cache key, and stitches them together for the client. If another user requests a different part of the file, only the missing 1MB chunks are fetched from upstream.
</details>

<details>
<summary><strong>Q8: How can you purge an individual cache entry in open-source NGINX without NGINX Plus?</strong></summary>

**Answer:**
While NGINX Plus provides the native `proxy_cache_purge` directive, open-source NGINX can be purged through several techniques:
1. **Direct Filesystem Deletion**: Compute the MD5 hash of the `proxy_cache_key`. Locate and delete the file in `/var/cache/nginx/...` matching that hash:
   ```bash
   MD5=$(echo -n "$CACHE_KEY" | md5sum | awk '{print $1}')
   find /var/cache/nginx/ -name "$MD5" -delete
   ```
2. **Third-Party Modules**: Compile NGINX with `ngx_cache_purge` (FRiCKLE module).
3. **njs Unlink Handler**: Use an njs script to calculate the MD5 hash, resolve the directory path based on `levels`, and call `fs.unlinkSync()`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure a Basic 10-Minute HTTP Proxy Cache</strong></summary>

**Goal:** Configure a shared memory cache zone `TEST_CACHE` and cache successful GET responses for 10 minutes, exposing the `X-Cache-Status` header.

**Solution:**
```nginx
# Add in http {} context:
proxy_cache_path /var/cache/nginx/test_cache
                 levels=1:2
                 keys_zone=TEST_CACHE:10m
                 max_size=1g
                 inactive=1h
                 use_temp_path=off;

server {
    listen 80;
    server_name cache.local;

    location / {
        proxy_pass http://127.0.0.1:3000;
        proxy_cache TEST_CACHE;
        proxy_cache_key $scheme$request_method$host$request_uri;
        proxy_cache_valid 200 10m;
        add_header X-Cache-Status $upstream_cache_status always;
    }
}
```
Test with `curl -I http://cache.local/` twice and verify status transitions from `MISS` to `HIT`.
</details>

<details>
<summary><strong>Exercise 2: Implement 2-Second Dynamic Microcaching</strong></summary>

**Goal:** Configure microcaching on `/api/time` to cache timestamps for 2 seconds while serving stale data during backend updates.

**Solution:**
```nginx
location /api/time {
    proxy_pass http://127.0.0.1:4000;
    proxy_cache TEST_CACHE;
    proxy_cache_key $request_uri;
    proxy_cache_valid 200 2s;

    proxy_cache_use_stale updating error timeout;
    proxy_cache_background_update on;
    proxy_cache_lock on;

    add_header X-Microcache $upstream_cache_status always;
}
```
</details>

<details>
<summary><strong>Exercise 3: Configure Cache Bypass via Request Header</strong></summary>

**Goal:** Allow developers to pass `X-Refresh: true` to bypass and refresh the cached version of any page.

**Solution:**
```nginx
map $http_x_refresh $bypass_cache {
    default 0;
    "true"  1;
}

server {
    listen 80;
    server_name api.local;

    location / {
        proxy_pass http://127.0.0.1:5000;
        proxy_cache TEST_CACHE;
        proxy_cache_bypass $bypass_cache;
        proxy_cache_valid 200 5m;
        add_header X-Cache-Status $upstream_cache_status always;
    }
}
```
Test with `curl -I -H "X-Refresh: true" http://api.local/` and verify `X-Cache-Status: BYPASS`.
</details>

<details>
<summary><strong>Exercise 4: Prevent Cache Stampedes with proxy_cache_lock</strong></summary>

**Goal:** Configure cache locking with a 3-second timeout and observe that parallel requests wait for the initial cache fill.

**Solution:**
```nginx
location /heavy-computation {
    proxy_pass http://slow_backend;
    proxy_cache TEST_CACHE;
    proxy_cache_lock on;
    proxy_cache_lock_timeout 3s;
    proxy_cache_lock_age 3s;
    proxy_cache_valid 200 1h;
}
```
</details>

<details>
<summary><strong>Exercise 5: Locate and Inspect a Cache File on Disk</strong></summary>

**Goal:** Make a cached request, calculate its MD5 hash, and verify the file exists on disk with HTTP headers included.

**Solution:**
```bash
# 1. Make request
curl -s http://cache.local/item/1 > /dev/null

# 2. Compute MD5 of proxy_cache_key ("httpGETcache.local/item/1")
KEY="httpGETcache.local/item/1"
MD5=$(echo -n "$KEY" | md5sum | awk '{print $1}')
echo "Hash: $MD5"

# 3. Search cache directory
find /var/cache/nginx/test_cache -name "$MD5"
```
</details>

<details>
<summary><strong>Exercise 6: Setup Large File Caching with the Slice Module</strong></summary>

**Goal:** Configure 512KB slice caching for video files under `/videos/`.

**Solution:**
```nginx
location /videos/ {
    proxy_pass http://storage_upstream;

    slice 512k;
    proxy_cache TEST_CACHE;
    proxy_cache_key $uri$slice_range;
    proxy_set_header Range $slice_range;

    proxy_cache_valid 200 206 7d;
    add_header X-Cache-Status $upstream_cache_status always;
}
```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production Reverse Proxy Caching Cheat Sheet

# 1. Define Zone in http context
proxy_cache_path /var/cache/nginx/prod
                 levels=1:2
                 keys_zone=PROD:50m
                 max_size=20g
                 inactive=7d
                 use_temp_path=off; # Mandatory for atomic renames

# 2. Server Location Settings
server {
    location / {
        proxy_pass http://backend;

        # Cache activation & key
        proxy_cache PROD;
        proxy_cache_key $scheme$request_method$host$request_uri;

        # TTL Rules
        proxy_cache_valid 200 302 10m;
        proxy_cache_valid 404 1m;

        # Resiliency & Stampede defense
        proxy_cache_lock on;
        proxy_cache_use_stale updating error timeout http_500 http_502 http_503 http_504;
        proxy_cache_background_update on;
        proxy_cache_revalidate on;

        # Observability header
        add_header X-Cache-Status $upstream_cache_status always;
    }
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_proxy_module Caching Guide](https://nginx.org/en/docs/http/ngx_http_proxy_module.html#proxy_cache)
- [Official NGINX ngx_http_slice_module Documentation](https://nginx.org/en/docs/http/ngx_http_slice_module.html)
- [High-Performance Caching with NGINX (NGINX Blog)](https://www.nginx.com/blog/nginx-caching-guide/)
- [RFC 9111: HTTP Caching Standards](https://datatracker.ietf.org/doc/html/rfc9111)

---

[← Module Overview](README.md) | [Next: Load Balancing & Upstreams →](02-load-balancing.md)
