# Caching & Rate Limiting Snippets

> **Cheatsheet ID:** `NGX-SHT-005`  
> **Topic:** NGINX Fast Diagnostic & Production Snippets for Caching, Throttling, and Concurrency Limits  
> **Target Version:** NGINX 1.30+ (Stable) / 1.31+ (Mainline)  
> **Related Concept Notes:** [01-caching.md](../03-traffic-management/01-caching.md), [03-rate-limiting.md](../03-traffic-management/03-rate-limiting.md), [07-connection-limiting-and-concurrency.md](../03-traffic-management/07-connection-limiting-and-concurrency.md)

---

## 1. Quick Syntax Overview

### Cache Path Directive (`http` context only)
```nginx
proxy_cache_path /var/cache/nginx/api
    levels=1:2
    keys_zone=API_CACHE:50m
    inactive=12h
    max_size=20g
    use_temp_path=off;
```
* **`levels=1:2`**: 2-level directory hierarchy (`/var/cache/nginx/api/a/8b/...`) preventing ext4 inode performance degradation with millions of cached files.
* **`keys_zone=NAME:SIZE`**: Shared-memory zone for metadata keys (1MB holds ~8,000 keys; 50MB holds ~400,000 keys).
* **`inactive=12h`**: Unaccessed cache items purged after 12 hours regardless of `Expires` or `Cache-Control`.
* **`max_size=20g`**: Hard upper boundary on disk space. Cache manager process enforces this.
* **`use_temp_path=off`**: Writes temporary files directly into the cache directory before atomic rename, avoiding cross-filesystem disk I/O penalties.

---

## 2. Production Caching Recipes

### 2.1 Microcaching for Dynamic High-Traffic APIs (1-Second Cache)
Cuts backend database and application CPU load by 95%+ during traffic surges while keeping data virtually real-time.

```nginx
# In http {} context:
proxy_cache_path /var/cache/nginx/micro
    levels=1:2
    keys_zone=MICRO_CACHE:10m
    inactive=10m
    max_size=1g
    use_temp_path=off;

map $request_method $purge_method {
    PURGE   1;
    default 0;
}

# In server {} or location /api/ context:
location /api/dynamic-feed {
    proxy_pass http://backend_upstream;
    proxy_http_version 1.1;
    proxy_set_header Connection "";
    proxy_set_header Host $host;

    # Caching Directives
    proxy_cache MICRO_CACHE;
    proxy_cache_valid 200 302 1s;
    proxy_cache_valid 404 10s;
    proxy_cache_valid any 0s;

    # Key Definition
    proxy_cache_key "$scheme$request_method$host$request_uri";

    # Bypass & Purge rules
    proxy_cache_bypass $http_x_bypass_cache $arg_skip_cache;
    proxy_no_cache     $http_x_bypass_cache $arg_skip_cache;

    # Lock & Background Revalidation (Avoid Cache Stampede)
    proxy_cache_use_stale error timeout updating http_500 http_502 http_503 http_504;
    proxy_cache_lock on;
    proxy_cache_lock_timeout 5s;
    proxy_cache_background_update on;

    # Expose Cache Status Header to Clients
    add_header X-Cache-Status $upstream_cache_status always;
}
```

### 2.2 Static Asset Edge Caching (Long-Lived with Immutable Headers)
```nginx
# In http {} context:
proxy_cache_path /var/cache/nginx/static
    levels=1:2
    keys_zone=STATIC_CACHE:20m
    inactive=30d
    max_size=10g
    use_temp_path=off;

# In server {} context:
location /assets/ {
    proxy_pass http://s3_or_origin_storage;
    proxy_cache STATIC_CACHE;
    proxy_cache_valid 200 304 30d;
    proxy_cache_key "$uri";

    # Ignore Origin Cache-Control headers to force edge retention
    proxy_ignore_headers Cache-Control Expires Set-Cookie;
    proxy_hide_header Set-Cookie;

    # Add client-side immutable caching
    add_header Cache-Control "public, max-age=31536000, immutable" always;
    add_header X-Cache-Status $upstream_cache_status always;
}
```

### 2.3 Per-User & Multi-Tenant Cache Key Isolation
Prevents cache poisoning and sensitive user data leaks.
```nginx
# Include Bearer token hash or Session ID in cache key
proxy_cache_key "$scheme$host$request_uri$http_authorization$cookie_sessionid";

# Ensure Set-Cookie is never cached
proxy_hide_header Set-Cookie;
proxy_ignore_headers X-Accel-Expires;
```

---

## 3. Rate Limiting Recipes (`limit_req`)

### 3.1 Zone Definition Syntax (`http` context)
```nginx
# 10MB zone tracks ~160,000 IPv4 addresses or ~80,000 IPv6 addresses
limit_req_zone $binary_remote_addr zone=ip_limit:10m rate=10r/s;

# Per-API Key throttling zone
limit_req_zone $http_x_api_key zone=api_key_limit:20m rate=100r/m;

# Per-URI Throttling (e.g. login endpoint)
limit_req_zone $binary_remote_addr zone=login_limit:5m rate=1r/s;
```

### 3.2 Strategy Comparison Table

| Directives | Behavior Under Load | Client Latency Impact | HTTP Response |
| :--- | :--- | :--- | :--- |
| `limit_req zone=ip_limit;` | Strict leaky bucket, zero burst allowed. Extra requests dropped instantly. | Low | 503 / 429 immediately |
| `limit_req zone=ip_limit burst=20;` | Enqueues up to 20 burst requests, delays execution according to rate. | High (requests delayed) | Enqueued until queue full |
| `limit_req zone=ip_limit burst=20 nodelay;` | Processes up to 20 burst requests instantly; rejects requests beyond 20. | Low (zero added delay) | 429 if burst exhausted |
| `limit_req zone=ip_limit burst=30 delay=10;` | First 10 burst requests processed immediately; remaining 20 delayed. | Medium (smooth transition) | 429 if burst exhausted |

### 3.3 Production API Gateway Throttling Template
```nginx
# In http {} context:
limit_req_zone $binary_remote_addr zone=global_ip:10m rate=20r/s;
limit_req_zone $binary_remote_addr zone=login_strict:5m rate=2r/s;
limit_req_status 429;
limit_req_log_level warn;

# Whitelist internal monitoring and trusted IPs
geo $rate_limit_bypass {
    default        $binary_remote_addr;
    127.0.0.1      "";
    10.0.0.0/8     "";
    192.168.1.0/24 "";
}
limit_req_zone $rate_limit_bypass zone=bypassable_limit:10m rate=15r/s;

server {
    listen 443 ssl;
    server_name api.example.com;

    # Global rate limit with smooth delay
    location / {
        limit_req zone=bypassable_limit burst=30 delay=15;
        proxy_pass http://api_backend;
    }

    # Strict login endpoint protection against brute-force
    location /api/v1/auth/login {
        limit_req zone=login_strict burst=5 nodelay;
        proxy_pass http://auth_backend;
    }
}
```

---

## 4. Connection Limiting Recipes (`limit_conn`)

### 4.1 Zone Definitions & Application
```nginx
# In http {} context:
# Track concurrent connections per client IP
limit_conn_zone $binary_remote_addr zone=conn_per_ip:10m;

# Track concurrent connections across the entire virtual host
limit_conn_zone $server_name zone=conn_per_server:10m;

limit_conn_status 429;
limit_conn_log_level warn;

server {
    listen 443 ssl;
    server_name downloads.example.com;

    # Protect download bandwidth from multi-threaded download managers
    location /downloads/ {
        limit_conn conn_per_ip 2;          # Max 2 simultaneous downloads per IP
        limit_conn conn_per_server 500;    # Max 500 total downloads on this server
        limit_rate 1024k;                  # Limit download speed to 1MB/s per connection
        limit_rate_after 10m;              # Full speed for first 10MB, then throttle
        root /var/data/downloads;
    }
}
```

---

## 5. Combined Anti-DDoS Defense Blueprint

```nginx
http {
    # 1. Real IP unwinding (if behind CDN or ALB)
    set_real_ip_from 10.0.0.0/8;
    real_ip_header X-Forwarded-For;
    real_ip_recursive on;

    # 2. Zones
    limit_req_zone  $binary_remote_addr zone=ddos_req:20m rate=15r/s;
    limit_conn_zone $binary_remote_addr zone=ddos_conn:20m;

    limit_req_status  429;
    limit_conn_status 429;

    server {
        listen 443 ssl;
        http2 on;
        server_name app.example.com;

        # Mitigate Slowloris & Slow POST attacks
        client_body_timeout 10s;
        client_header_timeout 10s;
        send_timeout 10s;
        keepalive_timeout 30s 15s;

        location / {
            limit_conn ddos_conn 20;
            limit_req zone=ddos_req burst=40 nodelay;

            proxy_pass http://upstream_app;
        }

        # Custom 429 Error Handler Returning JSON
        error_page 429 = @rate_limited;
        location @rate_limited {
            default_type application/json;
            return 429 '{"error": "Too Many Requests", "retry_after": 5}';
        }
    }
}
```

---

## 6. Diagnostic CLI Commands

```bash
# 1. Test rate limiting with Apache Bench (100 requests, 10 concurrent)
ab -n 100 -c 10 https://api.example.com/api/dynamic-feed

# 2. Test burst & nodelay behavior with curl in a loop
for i in {1..25}; do curl -s -o /dev/null -w "%{http_code}\n" https://api.example.com/api/v1/auth/login; done

# 3. Inspect cache directory size and file count
du -sh /var/cache/nginx/*
find /var/cache/nginx/ -type f | wc -l

# 4. View real-time rate-limiting drops in error log
tail -f /var/log/nginx/error.log | grep -E "limiting requests|limiting connections"

# 5. Check cache HIT ratio from access logs
awk '{print $NF}' /var/log/nginx/access.log | sort | uniq -c | sort -nr
```
