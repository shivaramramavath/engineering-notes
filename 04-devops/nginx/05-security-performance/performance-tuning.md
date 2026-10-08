# Performance Tuning

## Concept
nginx is fast by default. Tuning is about removing specific bottlenecks: wasted CPU, extra round trips, small buffers causing disk spills, and OS limits. **Measure first, change one thing at a time, and measure again.**

**Prerequisites:** [architecture.md](../04-internals/architecture.md), [reverse-proxy.md](../02-core-features/reverse-proxy.md), [logging.md](../02-core-features/logging.md)

## Where to Look First (Priority Order)

| Priority | Area | Typical gain |
|---|---|---|
| 1 | Cache what you can (browser + `proxy_cache`) | Largest: avoids work entirely |
| 2 | Keepalive (clients and upstreams) | Fewer handshakes and connections |
| 3 | Compression for text content | 60-80% smaller text payloads |
| 4 | Workers, connections, file limits | Removes hard ceilings |
| 5 | Static file delivery (`sendfile`, open file cache) | Lower CPU and disk overhead |
| 6 | Buffers and timeouts | Avoids disk spills and stuck connections |
| 7 | TLS and HTTP/2 / HTTP/3 | Lower latency |
| 8 | OS tuning | Needed at high connection rates |

Often the real bottleneck is the **backend**, not nginx. Check `$upstream_response_time` before tuning nginx.

## Workers and Connections
```nginx
worker_processes auto;
worker_rlimit_nofile 65535;

events {
    worker_connections 4096;
}
```
See [architecture.md](../04-internals/architecture.md) for the capacity formula (halve it for reverse proxy). Raise OS limits too (systemd: `LimitNOFILE=65535` in a unit override).

## Efficient Static File Delivery
```nginx
http {
    sendfile    on;        # kernel copies file to socket directly
    tcp_nopush  on;        # with sendfile: send headers and file start in full packets
    tcp_nodelay on;        # disable Nagle for keepalive connections (default on)

    open_file_cache          max=10000 inactive=30s;
    open_file_cache_valid    60s;
    open_file_cache_min_uses 2;
    open_file_cache_errors   on;
}
```
- `open_file_cache` caches file descriptors and metadata, which helps with many small files. Updated files may not appear until `open_file_cache_valid` passes.
- Large files on slow storage: add `aio threads;` to avoid blocking workers.
- Do not enable `sendfile` on some network or virtual filesystems (e.g. certain VirtualBox shared folders) where it serves stale or corrupt content.

## Keepalive

### Client side
```nginx
keepalive_timeout  30s;      # default 75s; shorter frees connections faster
keepalive_requests 1000;     # requests per connection (default 1000 in modern nginx)
```
### Upstream side
```nginx
upstream app {
    server 10.0.0.11:3000;
    keepalive 32;
}
location / {
    proxy_pass http://app;
    proxy_http_version 1.1;
    proxy_set_header Connection "";
}
```
Without upstream keepalive, every proxied request opens a new TCP connection to the backend, which wastes time and can exhaust ephemeral ports under load. See [load-balancing.md](../03-traffic-management/load-balancing.md).

## Compression
```nginx
http {
    gzip on;
    gzip_comp_level 5;            # 1-9; 4-6 is the sweet spot
    gzip_min_length 256;          # tiny responses aren't worth it
    gzip_vary on;                 # adds Vary: Accept-Encoding for caches
    gzip_proxied any;             # also compress responses from proxied backends
    gzip_types
        text/css text/plain text/xml application/json application/javascript
        application/xml image/svg+xml font/ttf font/otf;
}
```
- `text/html` is always compressed when `gzip on`. Other types must be listed.
- **Do not compress** already-compressed formats (JPEG, PNG, WebP, MP4, ZIP, WOFF2). It wastes CPU.
- `gzip_static on;` serves pre-compressed `.gz` files created at build time, which is free at runtime and allows maximum compression (needs the `ngx_http_gzip_static_module`, included in most packages).
- **Brotli** compresses better than gzip for text. It is not in the official nginx build. It requires the third-party `ngx_brotli` module or a distribution package that includes it. Verify it exists in your build before adding config.
- If a CDN or the backend already compresses, avoid double compression.
- Security: compressing responses that mix secrets with attacker-controlled input can enable BREACH-style attacks. Don't compress those responses, or use per-request CSRF token masking in the application.

## Browser Caching
```nginx
location ~* \.(css|js|png|jpg|svg|woff2)$ {
    expires 30d;
    add_header Cache-Control "public, immutable";
}
```
The fastest request is one the browser never sends. Use fingerprinted filenames for long expiries. See [static-files.md](../01-fundamentals/static-files.md).

## Proxy Buffers
```nginx
proxy_buffering on;
proxy_buffer_size        8k;       # for response headers
proxy_buffers            16 8k;    # per connection, for the body
proxy_busy_buffers_size  16k;
client_body_buffer_size  128k;     # bodies larger than this go to a temp file
```
- If responses exceed buffers, nginx writes temp files to disk, shown in the error log as "an upstream response is buffered to a temporary file". Raise buffers moderately, or accept it for rare large responses.
- Buffers are **per connection**, so large values multiplied by thousands of connections consume real memory.
- Turn off `proxy_buffering` only for streaming (SSE, long polling).

## TLS Performance
```nginx
ssl_session_cache   shared:SSL:10m;     # ~40,000 sessions per 10 MB
ssl_session_timeout 1d;
ssl_protocols       TLSv1.2 TLSv1.3;
http2 on;                                # nginx 1.25.1+, or `listen 443 ssl http2;` on older
```
- Session resumption avoids repeated full handshakes.
- TLS 1.3 reduces handshake round trips.
- ECDSA certificates are cheaper to handshake than RSA, with RSA still valid for compatibility.
- OCSP stapling avoids client lookups (check your CA's current support, see [tls-https.md](../02-core-features/tls-https.md)).
- Keep `keepalive` enabled so TLS connections are reused.

## HTTP/2 and HTTP/3
- **HTTP/2** multiplexes many requests over one connection, which usually helps, with `http2 on`.
- **HTTP/3 (QUIC)** is supported in newer nginx builds that include the QUIC module. It needs UDP port 443 open, and typically:
  ```nginx
  listen 443 quic reuseport;
  listen 443 ssl;
  add_header Alt-Svc 'h3=":443"; ma=86400';
  ```
  Check `nginx -V` for `--with-http_v3_module` before using it, and review the current nginx docs, since this area has been changing quickly.

## Logging Overhead
```nginx
access_log /var/log/nginx/access.log main buffer=32k flush=5s;
location /static/ { access_log off; }
```
Log writes can be significant at high request rates. Buffer them or skip noisy paths. See [logging.md](../02-core-features/logging.md).

## Regex and Configuration Cost
- Prefer prefix or exact `location` matches over regex when possible.
- Use `map` and `return` instead of repeated `if` and `rewrite`.
- Avoid expensive regex patterns on hot paths.

## Socket and OS Tuning
Only needed at high connection rates. Apply and test with `sysctl`, and persist in `/etc/sysctl.d/`.
```bash
# Larger accept queue (also set `listen 80 backlog=4096;` in nginx)
sysctl -w net.core.somaxconn=4096

# More ephemeral ports for nginx to backend connections
sysctl -w net.ipv4.ip_local_port_range="10240 65535"

# Raise file descriptor limit for the service (systemd)
# [Service]
# LimitNOFILE=65535
```
- Prefer upstream keepalive over tweaking `tcp_tw_reuse`, which is a workaround.
- Use `listen ... reuseport;` on busy servers for better connection spread across workers.
- Don't copy large `sysctl` blocks from the internet. Change settings you can justify with measurements.

## Measure Before and After

### Load testing
```bash
wrk -t4 -c200 -d30s https://example.com/            # throughput and latency
hey -z 30s -c 100 https://example.com/api/items
ab -n 10000 -c 100 http://localhost/                # simple, but HTTP/1.0-style
```
Test from a different machine, and test realistic URLs and TLS settings. Benchmarking `localhost` mostly measures the load generator.

### What to watch
```nginx
log_format perf '$remote_addr "$request" $status rt=$request_time '
                'uct=$upstream_connect_time urt=$upstream_response_time cache=$upstream_cache_status';
```
| Metric | Meaning |
|---|---|
| `$request_time` | Total time spent serving the request |
| `$upstream_connect_time` | High values suggest connection setup cost, so add keepalive |
| `$upstream_response_time` | Backend latency. If this dominates, tune the backend |
| `$upstream_cache_status` | Cache hit ratio |

Also monitor CPU per worker, memory, open connections (`stub_status`, see [operations](../06-production/operations.md)), and error log warnings.

## Common Mistakes
- Tuning nginx when the backend is the bottleneck.
- Setting `worker_processes` far above CPU count.
- Compressing images and video, or compressing at maximum level (`gzip_comp_level 9`) for tiny gains.
- Huge per-connection buffers that cause memory spikes.
- Enabling `open_file_cache` with a long validity on frequently changing files.
- Missing upstream keepalive (`proxy_http_version 1.1` and `Connection ""`).
- Copying "ultimate nginx.conf" templates without understanding each value.
- Benchmarking only a static page and expecting the same results for proxied dynamic traffic.

## Debugging Slowness
```bash
curl -o /dev/null -s -w "dns:%{time_namelookup} connect:%{time_connect} tls:%{time_appconnect} ttfb:%{time_starttransfer} total:%{time_total}\n" https://example.com/
sudo grep "buffered to a temporary file" /var/log/nginx/error.log | tail
sudo ss -tan state established '( sport = :443 )' | wc -l
```
Compare `ttfb` and `$upstream_response_time` to separate nginx from backend latency.

## Related / Next
- [caching.md](../03-traffic-management/caching.md): the biggest single performance lever
- [security-hardening.md](security-hardening.md): keeping limits and timeouts sane
- [operations](../06-production/operations.md): monitoring and capacity in production
