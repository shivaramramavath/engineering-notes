# nginx Cheat Sheet

Fast lookup for commands, defaults, and copy-ready snippets. Each section links to the full explanation.

**Contents:** [CLI](#cli) · [Signals](#signals) · [Paths](#default-paths) · [Skeleton](#config-skeleton) · [Matching](#location-matching) · [Variables](#common-variables) · [Defaults](#defaults-worth-knowing) · [Snippets](#snippets) · [Errors](#errors-quick-lookup) · [Debugging](#debugging-commands) · [Gotchas](#top-gotchas)

---

## CLI

| Command | Purpose |
|---|---|
| `nginx -t` | Test config syntax and referenced files |
| `nginx -T` | Test, then dump the full merged config (all includes expanded) |
| `nginx -s reload` | Graceful reload (validates first; keeps old config on error) |
| `nginx -s reopen` | Reopen log files (after rotation) |
| `nginx -s quit` | Graceful shutdown (drain connections) |
| `nginx -s stop` | Fast shutdown |
| `nginx -V` | Version, build flags, modules, default paths |
| `nginx -v` | Version only |
| `nginx -c /path/nginx.conf` | Use a different main config file |
| `nginx -g "daemon off;"` | Pass a global directive on the command line (used in containers) |
| `nginx -p /prefix/` | Set the prefix path |

```bash
sudo systemctl start|stop|restart|reload|status nginx
sudo systemctl enable --now nginx
sudo journalctl -u nginx --since "10 min ago"
```
Always: `sudo nginx -t && sudo systemctl reload nginx`. See [operations.md](../06-production/operations.md).

## Signals

| Signal | Equivalent | Effect |
|---|---|---|
| `HUP` | `-s reload` | Re-read config, start new workers, drain old ones |
| `QUIT` | `-s quit` | Graceful shutdown |
| `TERM` / `INT` | `-s stop` | Fast shutdown |
| `USR1` | `-s reopen` | Reopen log files |
| `USR2` | n/a | Start a new master (binary upgrade) |
| `WINCH` | n/a | Gracefully stop the old master's workers |

```bash
sudo kill -HUP $(cat /run/nginx.pid)
```
Details: [architecture.md](../04-internals/architecture.md).

## Default Paths
(Package installs. Confirm with `nginx -V`.)

| Path | What |
|---|---|
| `/etc/nginx/nginx.conf` | Main config |
| `/etc/nginx/conf.d/*.conf` | Drop-in configs |
| `/etc/nginx/sites-available/`, `sites-enabled/` | Debian/Ubuntu site convention (symlink to enable) |
| `/var/log/nginx/access.log`, `error.log` | Logs |
| `/usr/share/nginx/html` or `/var/www/html` | Default web root |
| `/run/nginx.pid` | Master PID |
| `/var/cache/nginx/` | Typical cache directory |

## Config Skeleton

```nginx
user www-data;                      # main
worker_processes auto;
events { worker_connections 4096; } # events
http {                              # http
    include mime.types;
    upstream app { server 127.0.0.1:3000; keepalive 16; }
    server {                        # server (virtual host)
        listen 80;
        server_name example.com;
        location / { ... }          # location
    }
}
```
Inheritance: inner contexts inherit outer values. **List-style directives (`add_header`, `proxy_set_header`, `error_page`) are replaced, not merged**, once a lower level defines any of them. See [config-structure.md](../01-fundamentals/config-structure.md).

## Location Matching

| Syntax | Meaning | Priority |
|---|---|---|
| `location = /x` | Exact | 1 (stops immediately) |
| `location ^~ /x` | Prefix, skips regex if it is the longest prefix | 2 |
| `location ~ re` | Regex, case-sensitive | 3 (first match in file order) |
| `location ~* re` | Regex, case-insensitive | 3 |
| `location /x` | Plain prefix | 4 (longest wins, used if no regex matched) |

Rule: exact, then longest prefix (stop if `^~`), then regexes **in file order**, then the remembered longest prefix.

`server_name` priority: exact, then leading wildcard (`*.x.com`), then trailing wildcard (`x.*`), then first matching regex, then default server. See [server-and-location.md](../01-fundamentals/server-and-location.md).

## Common Variables

| Variable | Meaning |
|---|---|
| `$host` | Host name (header, lowercased, no port) |
| `$request_uri` | Original URI with query string, unmodified |
| `$uri` | Current, normalized URI without query string (changes after rewrites) |
| `$args`, `$arg_x` | Query string, one parameter |
| `$scheme` | `http` / `https` |
| `$remote_addr` | Client IP (after `realip`) |
| `$binary_remote_addr` | Compact IP for rate-limit keys |
| `$http_<header>` | Request header, e.g. `$http_user_agent` |
| `$request_time` | Total request time |
| `$upstream_addr` | Backend that served the request |
| `$upstream_response_time` | Backend time |
| `$upstream_cache_status` | HIT, MISS, BYPASS, EXPIRED, STALE, UPDATING, REVALIDATED |
| `$request_id` | Unique request ID |
| `$status` | Response status (log time) |

## Defaults Worth Knowing

| Directive | Default |
|---|---|
| `worker_connections` | 512 |
| `keepalive_timeout` | 75s |
| `client_max_body_size` | 1m |
| `proxy_connect_timeout` / `proxy_read_timeout` / `proxy_send_timeout` | 60s |
| `proxy_buffering` | on |
| `proxy_http_version` | 1.0 in most versions in use (newer releases may differ, so set `1.1` explicitly for keepalive and WebSockets) |
| `proxy_next_upstream` | `error timeout` |
| `gzip` | off (and `gzip_types` defaults to `text/html` only) |
| `limit_req_status` / `limit_conn_status` | 503 |
| `server_tokens` | on |
| `ssl_prefer_server_ciphers` | off |
| `upstream server` `max_fails` / `fail_timeout` | 1 / 10s |
| `accept_mutex` | off (since 1.11.3) |
| Internal redirect limit | 10 |

---

## Snippets

### Static site
```nginx
server {
    listen 80;
    server_name example.com;
    root /var/www/example;
    index index.html;
    location / { try_files $uri $uri/ =404; }
}
```

### SPA fallback
```nginx
location / { try_files $uri $uri/ /index.html; }
```

### `root` vs `alias`
```nginx
location /img/ { root  /data; }       # /img/a.png -> /data/img/a.png
location /img/ { alias /data/; }      # /img/a.png -> /data/a.png   (match trailing slashes!)
```

### Redirects
```nginx
return 301 https://example.com$request_uri;                        # HTTP -> HTTPS
server { server_name www.example.com; return 301 https://example.com$request_uri; }   # canonical host
rewrite ^/old/(.*)$ /new/$1 permanent;                              # regex redirect
```

### HTTPS server
```nginx
server {
    listen 443 ssl;
    http2 on;                                  # 1.25.1+; older: listen 443 ssl http2;
    server_name example.com;
    ssl_certificate     /etc/letsencrypt/live/example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/example.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_session_cache shared:SSL:10m;
    add_header Strict-Transport-Security "max-age=31536000" always;
}
```
```bash
sudo certbot certonly --webroot -w /var/www/certbot -d example.com
sudo certbot renew --dry-run
```

### Reverse proxy
```nginx
location / {
    proxy_pass http://127.0.0.1:3000;          # no URI part: path unchanged
    proxy_http_version 1.1;
    proxy_set_header Host              $host;
    proxy_set_header X-Real-IP         $remote_addr;
    proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
}
# proxy_pass http://127.0.0.1:3000/;  <- trailing "/" replaces the matched location prefix
```

### WebSocket
```nginx
map $http_upgrade $connection_upgrade { default upgrade; '' close; }   # http context

location /ws/ {
    proxy_pass http://app;
    proxy_http_version 1.1;
    proxy_set_header Upgrade    $http_upgrade;
    proxy_set_header Connection $connection_upgrade;
    proxy_read_timeout 1h;
}
```

### Upstream, load balancing, keepalive
```nginx
upstream app {
    zone app 64k;                              # share failure state across workers
    least_conn;                                # or ip_hash; or hash $request_uri consistent;
    server 10.0.0.11:3000 weight=3 max_fails=3 fail_timeout=15s;
    server 10.0.0.12:3000;
    server 10.0.0.99:3000 backup;
    keepalive 32;
}
location / {
    proxy_pass http://app;
    proxy_http_version 1.1;
    proxy_set_header Connection "";            # required for upstream keepalive
    proxy_next_upstream error timeout http_502 http_503 http_504;
}
```

### Proxy cache
```nginx
proxy_cache_path /var/cache/nginx/app levels=1:2 keys_zone=app:10m max_size=1g inactive=60m use_temp_path=off;

location / {
    proxy_cache app;
    proxy_cache_valid 200 301 10m;
    proxy_cache_lock on;
    proxy_cache_use_stale error timeout updating http_500 http_502 http_503 http_504;
    proxy_cache_bypass $skip_cache;            # use together with:
    proxy_no_cache     $skip_cache;
    add_header X-Cache-Status $upstream_cache_status always;
    proxy_pass http://app;
}
```

### Rate limiting
```nginx
limit_req_zone  $binary_remote_addr zone=perip:10m rate=10r/s;     # http context
limit_conn_zone $binary_remote_addr zone=conn:10m;
limit_req_status 429;

location /api/ {
    limit_req  zone=perip burst=20 nodelay;     # burst: queue; nodelay: serve burst immediately
    limit_conn conn 10;
}
```

### Real client IP behind a proxy or CDN
```nginx
set_real_ip_from 10.0.0.0/8;       # trusted proxies ONLY
real_ip_header   X-Forwarded-For;
real_ip_recursive on;
```

### Compression and browser caching
```nginx
gzip on;
gzip_comp_level 5;
gzip_min_length 256;
gzip_vary on;
gzip_proxied any;
gzip_types text/css text/javascript application/javascript application/json image/svg+xml;

location ~* \.(css|js|png|jpg|svg|woff2)$ { expires 30d; }
```

### Static file performance
```nginx
sendfile on;
tcp_nopush on;
open_file_cache max=10000 inactive=30s;
open_file_cache_valid 60s;
```

### Security basics
```nginx
server_tokens off;
add_header X-Content-Type-Options "nosniff" always;
add_header X-Frame-Options "SAMEORIGIN" always;
add_header Referrer-Policy "strict-origin-when-cross-origin" always;

location ~ /\.(?!well-known) { deny all; }       # hide dotfiles

location /admin/ {
    allow 10.0.0.0/8;
    deny  all;
    auth_basic "Restricted";
    auth_basic_user_file /etc/nginx/.htpasswd;    # sudo htpasswd -c file user
}
```

### Reject unknown hosts
```nginx
server { listen 80  default_server; server_name _; return 444; }
server { listen 443 ssl default_server; server_name _; ssl_reject_handshake on; }   # 1.19.4+
```

### Upload size and timeouts
```nginx
client_max_body_size 20m;
proxy_read_timeout 120s;
```

### Maintenance mode
```nginx
if (-f /etc/nginx/maintenance.on) { return 503; }
error_page 503 /maintenance.html;
location = /maintenance.html { root /var/www/errors; internal; }
```

### JSON access log
```nginx
log_format json escape=json '{"time":"$time_iso8601","ip":"$remote_addr","uri":"$request_uri",'
                            '"status":$status,"rt":"$request_time","urt":"$upstream_response_time"}';
access_log /var/log/nginx/access.json json buffer=32k flush=5s;
```

### Health check and status
```nginx
location = /healthz { access_log off; default_type text/plain; return 200 "ok\n"; }

server {                                       # stub_status on loopback
    listen 127.0.0.1:8080;
    location = /nginx_status { stub_status; allow 127.0.0.1; deny all; }
}
```

### CORS (allow-list)
```nginx
map $http_origin $cors_origin { default ""; "https://app.example.com" $http_origin; }
add_header Access-Control-Allow-Origin $cors_origin always;
add_header Vary Origin always;
```

---

## Errors: Quick Lookup

| Status / message | Usual cause |
|---|---|
| **400** | Oversized cookie/header, HTTP sent to an HTTPS port |
| **403** | File permissions, no `index`, `deny` rule, SELinux |
| **404** | Wrong `root`/`alias`, wrong location matched, `try_files` |
| **413** | `client_max_body_size` too small |
| **429** | `limit_req`/`limit_conn` triggered |
| **444** | nginx intentionally closed the connection |
| **499** | Client closed the connection first (often a slow backend) |
| **502** | Backend down, refused, reset, invalid response, or too-big headers |
| **503** | Limits, maintenance flag, no live upstreams |
| **504** | Backend slower than `proxy_read_timeout` / `proxy_connect_timeout` |
| `connect() failed (111: Connection refused)` | Backend not listening |
| `upstream timed out (110)` | Backend too slow |
| `upstream sent too big header` | Raise `proxy_buffer_size` |
| `open() ... (13: Permission denied)` | File or socket permissions, SELinux |
| `open() ... (2: No such file or directory)` | Wrong path, check `root` vs `alias` |
| `host not found in upstream` | DNS failure at startup/reload |
| `bind() ... (98: Address already in use)` | Another process on the port |
| `worker_connections are not enough` | Raise `worker_connections` |
| `(24: Too many open files)` | Raise `worker_rlimit_nofile` and systemd `LimitNOFILE` |
| `key values mismatch` | Certificate and private key do not match |

Full diagnosis: [troubleshooting.md](../06-production/troubleshooting.md).

## Debugging Commands

```bash
sudo nginx -t                                              # config OK?
sudo nginx -T | less                                       # effective config
sudo ss -ltnp | grep nginx                                 # listening sockets
sudo tail -f /var/log/nginx/error.log                      # live errors
curl -vI http://example.com/                               # status, headers, redirects
curl -IL http://example.com/                               # follow redirects
curl -H "Host: example.com" http://127.0.0.1/              # test virtual host locally
curl --resolve example.com:443:203.0.113.10 https://example.com/    # test a specific server
curl -o /dev/null -s -w "connect:%{time_connect} tls:%{time_appconnect} ttfb:%{time_starttransfer} total:%{time_total}\n" https://example.com/
echo | openssl s_client -connect example.com:443 -servername example.com 2>/dev/null | openssl x509 -noout -dates -subject
namei -l /var/www/example/index.html                       # permissions along the path
getenforce; sudo ausearch -m avc -ts recent                # SELinux denials
for i in $(seq 1 30); do curl -s -o /dev/null -w "%{http_code}\n" URL; done | sort | uniq -c   # rate-limit test
```

Temporary diagnostics:
```nginx
add_header X-Debug-Upstream $upstream_addr always;        # which backend answered (remove afterwards)
rewrite_log on;                                           # needs error_log ... notice;
events { debug_connection 203.0.113.10; }                 # needs error_log ... debug; and a --with-debug build
```

## Top Gotchas

1. `add_header` / `proxy_set_header` in a child block **drop all inherited ones**. Re-include a snippet.
2. Add `always` to `add_header`, or error responses miss the header.
3. `proxy_pass http://x;` vs `proxy_pass http://x/;`: a URI part (even `/`) replaces the matched prefix.
4. `root` appends the URI, `alias` replaces the location prefix.
5. Regex locations are matched **in file order**, prefixes by **length**. `^~` skips regex.
6. Avoid `if` inside `location` except for `return`/`rewrite ... last`. Use `map`, `try_files`, or separate blocks.
7. nginx resolves upstream hostnames **once** at start/reload.
8. Upstream keepalive needs `proxy_http_version 1.1;` **and** `proxy_set_header Connection "";`.
9. `proxy_cache_bypass` without `proxy_no_cache` can still store personalized responses.
10. Behind a proxy, `$remote_addr` is the proxy unless you configure `realip`, and then trust only your own ranges.
11. Reverse-proxy capacity is about half of `worker_processes × worker_connections`, since each request uses two connections.
12. `reload` is graceful but long-lived connections keep old workers alive. Use `worker_shutdown_timeout`.
13. Use `$request_uri` (not `$uri`) in redirects to preserve the query string.
14. Don't cache or compress what you shouldn't: authenticated responses, already-compressed formats.
15. Always `nginx -t` before reload. Rotated logs need `reopen` (`USR1`).

## Full Guides
[Installation](../01-fundamentals/installation.md) · [Config structure](../01-fundamentals/config-structure.md) · [Server & location](../01-fundamentals/server-and-location.md) · [Static files](../01-fundamentals/static-files.md) · [Reverse proxy](../02-core-features/reverse-proxy.md) · [TLS](../02-core-features/tls-https.md) · [Variables & rewrites](../02-core-features/variables-and-rewrites.md) · [Logging](../02-core-features/logging.md) · [Load balancing](../03-traffic-management/load-balancing.md) · [Caching](../03-traffic-management/caching.md) · [Rate limiting](../03-traffic-management/rate-limiting.md) · [Architecture](../04-internals/architecture.md) · [Request lifecycle](../04-internals/request-lifecycle.md) · [Security](../05-security-performance/security-hardening.md) · [Performance](../05-security-performance/performance-tuning.md) · [Operations](../06-production/operations.md) · [Troubleshooting](../06-production/troubleshooting.md) · [Docker & Kubernetes](../06-production/docker-kubernetes.md)
