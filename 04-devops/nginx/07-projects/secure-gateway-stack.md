# Project: Secure Gateway Stack

## Goal
Build one nginx instance that acts as the **single, hardened front door** for several services:

```
                                    ┌──────────────────────────── nginx gateway ────────────────────────────┐
                                    │  default server:  unknown hosts are rejected                          │
                                    │                                                                      │
  Internet ──► (CDN / LB) ──► :443 ─┤  api.example.com    TLS · rate limits · cache · least_conn ─► 3 × API │
                                    │  admin.example.com  TLS · IP allow-list + basic auth ──────► admin app│
                                    │  docs.example.com   TLS · static files · long-lived caching           │
                                    │                                                                      │
                                    │  127.0.0.1:8080/nginx_status   (monitoring, loopback only)           │
                                    └──────────────────────────────────────────────────────────────────────┘
```

This project combines most of the earlier material into one layout you can reuse: shared snippets, one TLS certificate, tiered rate limits, a cacheable public API path, JSON logs, and defaults that fail closed.

## What You Will Practice

| Skill | Topic file |
|---|---|
| Config layout, contexts, inheritance | [config-structure.md](../01-fundamentals/config-structure.md) |
| Multiple virtual hosts, default server | [server-and-location.md](../01-fundamentals/server-and-location.md) |
| Reverse proxy, WebSockets | [reverse-proxy.md](../02-core-features/reverse-proxy.md) |
| TLS, redirect, one SAN certificate | [tls-https.md](../02-core-features/tls-https.md) |
| `map` / `geo`, JSON logs | [variables-and-rewrites.md](../02-core-features/variables-and-rewrites.md), [logging.md](../02-core-features/logging.md) |
| Upstream groups, failover | [load-balancing.md](../03-traffic-management/load-balancing.md) |
| Cache with safe bypass rules | [caching.md](../03-traffic-management/caching.md) |
| Tiered rate limits, real client IP | [rate-limiting.md](../03-traffic-management/rate-limiting.md) |
| Access control, headers, reject unknown hosts | [security-hardening.md](../05-security-performance/security-hardening.md) |
| Monitoring and zero-downtime change | [operations.md](../06-production/operations.md) |

## Assumptions
- Three DNS names point at the gateway: `api.example.com`, `admin.example.com`, `docs.example.com`.
- API instances at `10.0.1.11:3000`, `10.0.1.12:3000`, `10.0.1.13:3000`; admin app at `10.0.2.10:8000`; docs built into `/var/www/docs`.
- `10.0.0.0/8` is your private network (also used as the trusted-proxy range and for exemptions). Adjust every IP range to your environment.
- nginx is reached **directly** or behind a load balancer inside that range. If it is directly on the internet, remove the `set_real_ip_from` / `real_ip_*` lines from `00-global.conf` (see Step 3).

## Layout

```
/etc/nginx/
├── nginx.conf
├── conf.d/                     # http-level definitions, loaded first (alphabetical)
│   ├── 00-global.conf          # logs, TLS defaults, real IP, maps, zones, cache path
│   ├── 10-upstreams.conf
│   ├── 20-status.conf          # stub_status on loopback
│   └── 99-default.conf         # catch-all: reject unknown hosts
├── sites/                      # one file per virtual host, loaded after conf.d
│   ├── 00-http.conf            # port 80: ACME challenge + redirect for all names
│   ├── 10-api.example.com.conf
│   ├── 20-admin.example.com.conf
│   └── 30-docs.example.com.conf
├── snippets/
│   ├── security-headers.conf
│   ├── proxy-params.conf
│   └── tls-cert.conf
└── .htpasswd-admin
```
Load order matters: variables created by `map`/`geo` and log formats must exist before something uses them, which is why shared definitions live in `conf.d/` with numeric prefixes.

## Step 1: Main File

`/etc/nginx/nginx.conf`:
```nginx
user www-data;
worker_processes auto;
worker_rlimit_nofile 65535;
pid /run/nginx.pid;
error_log /var/log/nginx/error.log warn;

events {
    worker_connections 4096;
}

http {
    include      /etc/nginx/mime.types;
    default_type application/octet-stream;

    server_tokens off;
    sendfile      on;
    tcp_nopush    on;

    keepalive_timeout 30s;
    client_header_timeout 10s;
    client_body_timeout   10s;
    send_timeout          30s;
    client_max_body_size  2m;        # raise per location where needed

    gzip on;
    gzip_comp_level 5;
    gzip_min_length 256;
    gzip_vary on;
    gzip_proxied any;
    gzip_types text/css text/javascript application/javascript application/json
               image/svg+xml text/plain;

    include /etc/nginx/conf.d/*.conf;
    include /etc/nginx/sites/*.conf;
}
```

## Step 2: Shared Snippets

`/etc/nginx/snippets/security-headers.conf`:
```nginx
add_header X-Content-Type-Options    "nosniff" always;
add_header X-Frame-Options           "SAMEORIGIN" always;
add_header Referrer-Policy           "strict-origin-when-cross-origin" always;
add_header Permissions-Policy        "geolocation=(), camera=(), microphone=()" always;
add_header Strict-Transport-Security "max-age=300" always;     # raise after verifying HTTPS everywhere
```

`/etc/nginx/snippets/proxy-params.conf`:
```nginx
proxy_http_version 1.1;
proxy_set_header Host              $host;
proxy_set_header X-Real-IP         $remote_addr;
proxy_set_header X-Forwarded-For   $remote_addr;      # $remote_addr is already the real client (see realip)
proxy_set_header X-Forwarded-Proto $scheme;
proxy_set_header X-Request-ID      $request_id;
proxy_next_upstream         error timeout http_502 http_503 http_504;
proxy_next_upstream_tries   2;
proxy_next_upstream_timeout 10s;
```
Timeouts (`proxy_*_timeout`) are deliberately **not** in this snippet. Setting the same single-value directive twice in one block is a config error, and some locations (WebSockets) need different values. Set them at server level and override per location.

`/etc/nginx/snippets/tls-cert.conf`:
```nginx
ssl_certificate     /etc/letsencrypt/live/gateway/fullchain.pem;
ssl_certificate_key /etc/letsencrypt/live/gateway/privkey.pem;
```

## Step 3: Global Definitions

`/etc/nginx/conf.d/00-global.conf`:
```nginx
# ---------- TLS defaults (http level, so every virtual host agrees) ----------
ssl_protocols       TLSv1.2 TLSv1.3;
ssl_session_cache   shared:SSL:20m;
ssl_session_timeout 1d;
ssl_session_tickets off;

# ---------- Real client IP (REMOVE if nginx faces the internet directly) ----------
set_real_ip_from  10.0.0.0/8;           # your load balancer / CDN ranges only
real_ip_header    X-Forwarded-For;
real_ip_recursive on;

# ---------- Logging ----------
log_format json escape=json
  '{"time":"$time_iso8601","host":"$host","ip":"$remote_addr","method":"$request_method",'
  '"uri":"$request_uri","status":$status,"bytes":$body_bytes_sent,"rt":"$request_time",'
  '"upstream":"$upstream_addr","urt":"$upstream_response_time","cache":"$upstream_cache_status",'
  '"req_id":"$request_id"}';

# ---------- Variables ----------
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

# Internal clients are not rate limited. An empty key means "no limit".
geo $rate_limited {
    default    1;
    10.0.0.0/8 0;
}
map $rate_limited $api_limit_key {
    0 "";
    1 $binary_remote_addr;
}

# Cache only anonymous requests: any Authorization header or cookie => skip the cache
map "$http_authorization$http_cookie" $skip_cache {
    ""      0;
    default 1;
}

# ---------- Rate limits and connection limits ----------
limit_req_zone  $api_limit_key        zone=api_ip:20m   rate=20r/s;
limit_req_zone  $binary_remote_addr   zone=login_ip:10m rate=10r/m;
limit_req_zone  $binary_remote_addr   zone=admin_ip:10m rate=5r/s;
limit_conn_zone $binary_remote_addr   zone=conn_ip:10m;
limit_req_status  429;
limit_conn_status 429;

# ---------- Cache storage ----------
proxy_cache_path /var/cache/nginx/api levels=1:2 keys_zone=api_cache:20m
                 max_size=500m inactive=30m use_temp_path=off;
```
```bash
sudo mkdir -p /var/cache/nginx/api /var/www/certbot /var/www/docs
sudo chown -R www-data /var/cache/nginx        # use the `nginx` user on RHEL-family systems
```

Notes:
- TLS settings are at `http` level because for virtual hosts sharing an IP and port, settings like `ssl_protocols` are effectively taken from the default server.
- The `real_ip` settings must list **only** addresses you control. Otherwise anyone can fake `X-Forwarded-For` and bypass the rate limits and the admin IP allow-list.

## Step 4: Upstreams

`/etc/nginx/conf.d/10-upstreams.conf`:
```nginx
upstream api_stable {
    zone api_stable 64k;                  # shared memory: failure counts apply across all workers
    least_conn;
    server 10.0.1.11:3000 max_fails=3 fail_timeout=15s;
    server 10.0.1.12:3000 max_fails=3 fail_timeout=15s;
    server 10.0.1.13:3000 max_fails=3 fail_timeout=15s;
    keepalive 32;
}

upstream admin_backend {
    zone admin_backend 64k;
    server 10.0.2.10:8000;
    keepalive 8;
}
```
Without `zone`, each worker process tracks backend failures separately.

## Step 5: Monitoring Endpoint and Catch-All

`/etc/nginx/conf.d/20-status.conf`:
```nginx
server {
    listen 127.0.0.1:8080;
    server_name localhost;
    access_log off;

    location = /nginx_status {
        stub_status;
        allow 127.0.0.1;
        deny  all;
    }
}
```

`/etc/nginx/conf.d/99-default.conf`:
```nginx
# Any request whose Host header matches none of our sites ends here.
server {
    listen 80 default_server;
    server_name _;
    return 444;                        # close the connection with no response
}

server {
    listen 443 ssl default_server;
    server_name _;
    ssl_reject_handshake on;           # nginx 1.19.4+: no certificate is revealed for unknown names
}
```
Add matching `listen [::]:...` lines if you serve IPv6.

## Step 6: Port 80 and the Certificate

`/etc/nginx/sites/00-http.conf`:
```nginx
server {
    listen 80;
    server_name api.example.com admin.example.com docs.example.com;

    location /.well-known/acme-challenge/ {
        root /var/www/certbot;
    }
    location / {
        return 301 https://$host$request_uri;
    }
}
```
Bring nginx up with only the files created so far (**no HTTPS site files yet**, since they reference a certificate that does not exist), then issue **one certificate covering all three names**:
```bash
sudo nginx -t && sudo systemctl reload nginx
sudo certbot certonly --webroot -w /var/www/certbot --cert-name gateway \
     -d api.example.com -d admin.example.com -d docs.example.com

sudo tee /etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh >/dev/null <<'EOF'
#!/bin/sh
systemctl reload nginx
EOF
sudo chmod +x /etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh
sudo certbot renew --dry-run
```

## Step 7: The API Site

`/etc/nginx/sites/10-api.example.com.conf`:
```nginx
server {
    listen 443 ssl;
    http2 on;                          # nginx 1.25.1+; older: listen 443 ssl http2;
    server_name api.example.com;
    include snippets/tls-cert.conf;

    access_log /var/log/nginx/api.access.json json buffer=32k flush=5s;
    error_log  /var/log/nginx/api.error.log warn;

    include snippets/security-headers.conf;

    client_max_body_size  5m;
    proxy_connect_timeout 5s;
    proxy_send_timeout    30s;
    proxy_read_timeout    30s;

    error_page 429         = @rate_limited;
    error_page 502 503 504 = @api_down;

    # Liveness of nginx itself (no limits, no logging)
    location = /healthz {
        access_log off;
        default_type text/plain;
        return 200 "ok\n";
    }

    # Credential endpoints: strict, never cached
    location /v1/auth/ {
        limit_req zone=login_ip burst=5 nodelay;
        include snippets/proxy-params.conf;
        proxy_set_header Connection "";
        proxy_pass http://api_stable;
    }

    # Public, anonymous, cacheable GETs
    location /v1/public/ {
        limit_req  zone=api_ip burst=40 nodelay;
        limit_conn conn_ip 20;

        proxy_cache           api_cache;
        proxy_cache_key       "$scheme$request_method$host$request_uri";
        proxy_cache_valid     200 301 1m;
        proxy_cache_valid     404 10s;
        proxy_cache_bypass    $skip_cache;
        proxy_no_cache        $skip_cache;
        proxy_cache_lock      on;
        proxy_cache_use_stale error timeout updating http_500 http_502 http_503 http_504;
        proxy_cache_background_update on;

        include snippets/security-headers.conf;     # this block has its own add_header
        add_header X-Cache-Status $upstream_cache_status always;

        include snippets/proxy-params.conf;
        proxy_set_header Connection "";
        proxy_pass http://api_stable;
    }

    # Everything else under /v1/
    location /v1/ {
        limit_req  zone=api_ip burst=40 nodelay;
        limit_conn conn_ip 20;
        include snippets/proxy-params.conf;
        proxy_set_header Connection "";
        proxy_pass http://api_stable;
    }

    # WebSockets
    location /ws/ {
        limit_conn conn_ip 20;
        include snippets/proxy-params.conf;
        proxy_set_header Upgrade    $http_upgrade;
        proxy_set_header Connection $connection_upgrade;
        proxy_read_timeout 1h;
        proxy_pass http://api_stable;
    }

    # Deny by default: only the paths above exist
    location / {
        return 404;
    }

    location @rate_limited {
        default_type application/json;
        add_header Retry-After 1 always;
        include snippets/security-headers.conf;
        return 429 '{"error":"rate_limited"}';
    }

    location @api_down {
        default_type application/json;
        add_header Retry-After 5 always;
        include snippets/security-headers.conf;
        return 503 '{"error":"service_unavailable"}';
    }
}
```

## Step 8: The Admin Site

`/etc/nginx/sites/20-admin.example.com.conf`:
```nginx
server {
    listen 443 ssl;
    http2 on;
    server_name admin.example.com;
    include snippets/tls-cert.conf;

    access_log /var/log/nginx/admin.access.json json;
    include snippets/security-headers.conf;

    # Two independent gates, BOTH must pass (satisfy all is the default)
    allow 10.0.0.0/8;                   # VPN / office
    allow 203.0.113.0/24;
    deny  all;
    auth_basic           "Administration";
    auth_basic_user_file /etc/nginx/.htpasswd-admin;

    location / {
        limit_req zone=admin_ip burst=20 nodelay;
        include snippets/proxy-params.conf;
        proxy_set_header Connection "";
        proxy_connect_timeout 5s;
        proxy_read_timeout    60s;
        proxy_pass http://admin_backend;
    }
}
```
```bash
sudo htpasswd -c /etc/nginx/.htpasswd-admin alice        # package: apache2-utils (Debian) / httpd-tools (RHEL)
sudo chmod 640 /etc/nginx/.htpasswd-admin
sudo chgrp www-data /etc/nginx/.htpasswd-admin
```
The IP check only works if `$remote_addr` is the true client address, which is why the `real_ip` range must be correct.

## Step 9: The Docs Site

`/etc/nginx/sites/30-docs.example.com.conf`:
```nginx
server {
    listen 443 ssl;
    http2 on;
    server_name docs.example.com;
    include snippets/tls-cert.conf;

    root  /var/www/docs;
    index index.html;
    include snippets/security-headers.conf;

    location / {
        try_files $uri $uri/ =404;
        expires 1h;                      # `expires` does not count as add_header, so inherited headers survive
    }

    location /assets/ {
        expires 1y;
        access_log off;
    }

    location ~ /\.(?!well-known) { deny all; }
}
```

## Step 10: Apply and Verify

```bash
sudo nginx -t && sudo systemctl reload nginx
```

| Check | Command | Expected |
|---|---|---|
| Redirect | `curl -sI http://api.example.com/v1/x` | `301` to HTTPS |
| Health | `curl -s https://api.example.com/healthz` | `ok` |
| Unknown host over HTTP | `curl -s -H "Host: evil.test" http://<server-ip>/` | Empty reply (connection closed) |
| Unknown host over TLS | `curl -sk --resolve evil.test:443:<server-ip> https://evil.test/` | TLS handshake failure |
| Deny by default | `curl -sI https://api.example.com/secret` | `404` |
| Cache | `curl -sI https://api.example.com/v1/public/items \| grep -i x-cache` (run twice) | `MISS`, then `HIT` |
| Cache skipped when authenticated | Same request with `-H "Authorization: Bearer x"` | `BYPASS` |
| Rate limit | `for i in $(seq 1 100); do curl -s -o /dev/null -w "%{http_code}\n" https://api.example.com/v1/items; done \| sort \| uniq -c` (from a non-exempt IP) | Mix of 200 and 429 |
| Login limit | Repeat against `/v1/auth/login` | 429 after a handful |
| Admin blocked | Request from outside the allow-list | `403` |
| Admin needs credentials | Request from an allowed IP without credentials | `401` |
| Security headers on errors | `curl -sI https://api.example.com/secret \| grep -i x-content-type` | Header present |
| Monitoring | `curl -s http://127.0.0.1:8080/nginx_status` | Connection counters |
| Cert covers all names | `echo \| openssl s_client -connect docs.example.com:443 -servername docs.example.com 2>/dev/null \| openssl x509 -noout -ext subjectAltName` | All three DNS names |

### Failure drills
Do these in staging to see the design working:
1. **Stop one API instance.** Requests keep succeeding. Check `upstream` in the JSON log: a comma-separated value means a retry happened.
2. **Stop all API instances.** Clients get the JSON `503`, not an nginx page. Cached public paths still return `STALE` content.
3. **Reload under load** (`hey -z 30s -c 50 https://api.example.com/v1/items` while running `sudo nginx -s reload`): no failed requests.
4. **Break the config on purpose** and run `nginx -t`: it refuses, and the live config stays untouched.

## Decisions at a Glance

| Decision | Reason |
|---|---|
| Catch-all default servers (`444`, `ssl_reject_handshake`) | Scanners and wrong-Host requests never reach real sites |
| Deny-by-default `location / { return 404; }` on the API | Only intentionally exposed paths exist |
| Snippets for headers and proxy params | One place to change policy, but they must be re-included where a block defines its own `add_header` |
| Timeouts outside the proxy snippet | Avoids duplicate-directive errors and allows per-location overrides |
| `zone` in `upstream` | Consistent failure tracking across workers |
| `map $http_authorization$http_cookie` for cache bypass | Personalized responses can never be stored or served to others |
| `$api_limit_key` empty for internal IPs | Internal services are not throttled |
| TLS settings at `http` level | Same protocol policy for all virtual hosts |
| One SAN certificate + deploy hook | One renewal job, and nginx reloads after renewal |
| JSON logs with upstream timing and request ID | Searchable, and correlate with backend logs |
| `stub_status` on loopback only | Metrics without public exposure |

## Extensions to Try
1. **Canary release:** send 10% of traffic to a new version.
   ```nginx
   # http level
   split_clients "${remote_addr}${http_user_agent}" $api_pool {
       10%  api_canary;
       *    api_stable;
   }
   # in a proxy location: proxy_pass http://$api_pool;   (define upstream api_canary first)
   ```
   When `proxy_pass` uses a variable whose value matches an `upstream` name, nginx uses that group.
2. **Maintenance mode:** the flag-file `if (-f ...) { return 503; }` pattern from [operations.md](../06-production/operations.md).
3. **mTLS or TLS to backends:** `proxy_ssl_verify on;` with your internal CA ([tls-https.md](../02-core-features/tls-https.md)).
4. **Per-API-key limits:** use `$http_x_api_key` as the `limit_req_zone` key.
5. **Fail2ban:** ban IPs producing many 401/403/429 responses from the JSON logs.
6. **Containerize:** run the same layout in Docker Compose or Kubernetes ([docker-kubernetes.md](../06-production/docker-kubernetes.md)).
7. **Prometheus metrics:** run the nginx Prometheus exporter against `stub_status` and alert on 5xx and `accepts != handled` ([operations.md](../06-production/operations.md)).

## Limitations
- Rate limits and caches are **per nginx instance**. With several gateway nodes, the effective limit scales with the node count.
- Open-source nginx performs **passive** health checks only. Failed requests mark a backend down, so a dead backend still costs a retry on some requests. See [load-balancing.md](../03-traffic-management/load-balancing.md).
- A single nginx node is a single point of failure. Run two behind a floating IP or cloud load balancer for production.
- nginx cannot absorb volumetric DDoS. Use upstream protection ([security-hardening.md](../05-security-performance/security-hardening.md)).

## Related / Next
- [spa-with-api-proxy.md](spa-with-api-proxy.md): the simpler single-app version of this setup
- [operations.md](../06-production/operations.md): running and changing this safely
- [troubleshooting.md](../06-production/troubleshooting.md): diagnosing problems in this stack
- [interview-questions.md](../08-reference/interview-questions.md): practice explaining these decisions
