# nginx Interview Questions

Questions grouped by topic, with concise model answers and a link to the file that covers the detail. Use it two ways:

- **Self-test:** cover the answer, say it aloud, then check.
- **Depth practice:** for any answer, be ready for the follow-up "why?" or "what goes wrong if...?"

**Levels:** 🟢 fundamentals · 🟡 intermediate · 🔴 advanced / senior

**Contents:** [Fundamentals](#1-fundamentals) · [Core features](#2-core-features) · [Traffic management](#3-traffic-management) · [Internals](#4-internals) · [Security and performance](#5-security-and-performance) · [Production and troubleshooting](#6-production-and-troubleshooting) · [Containers](#7-containers-and-kubernetes) · [Scenarios](#8-scenario-questions) · [Hands-on tasks](#9-hands-on-tasks) · [Tips](#answering-tips)

---

## 1. Fundamentals

**🟢 Q1. What is nginx and what is it commonly used for?**
A high-performance web server that is also commonly used as a reverse proxy, load balancer, TLS terminator, HTTP cache, and API gateway. Its event-driven design lets it handle many concurrent connections with low memory.
→ [architecture.md](../04-internals/architecture.md)

**🟢 Q2. What is the difference between a web server and a reverse proxy?**
A web server serves content itself (static files, or runs handlers). A reverse proxy sits in front of backend servers, forwards client requests to them, and returns their responses. nginx does both, and often both at once: static files directly, dynamic paths proxied.
→ [reverse-proxy.md](../02-core-features/reverse-proxy.md)

**🟢 Q3. Explain the structure of an nginx config file.**
Directives (simple: `name value;`, or block: `name { ... }`) grouped into nested contexts: main, `events`, `http`, then `server` (virtual host) and `location` inside `http`. Also `upstream` in `http`, and optionally `stream` for TCP/UDP. Inner contexts inherit from outer ones.
→ [config-structure.md](../01-fundamentals/config-structure.md)

**🟡 Q4. How does inheritance work, and what is the common trap?**
Settings flow from outer to inner contexts unless overridden. The trap: list-style directives such as `add_header`, `proxy_set_header`, and `error_page` are **replaced as a group**, not merged. Defining one `add_header` in a `location` removes all `add_header` lines inherited from `server` and `http`. Fix by re-including a shared snippet.
→ [config-structure.md](../01-fundamentals/config-structure.md)

**🟢 Q5. What is the difference between `root` and `alias`?**
`root /data;` appends the full URI to the path: `/img/a.png` becomes `/data/img/a.png`. `alias /data/;` replaces the matched location prefix: with `location /img/`, `/img/a.png` becomes `/data/a.png`. Match trailing slashes between location and alias, otherwise you can get 404s or a path-traversal bug.
→ [static-files.md](../01-fundamentals/static-files.md)

**🟢 Q6. What does `try_files` do? Give an SPA example.**
It checks files or directories in order and serves the first that exists. The last argument is a fallback: a URI (internal redirect), a named location, or a status code. For an SPA: `try_files $uri $uri/ /index.html;` so unknown routes serve the app shell.
→ [static-files.md](../01-fundamentals/static-files.md)

**🟡 Q7. How does nginx choose a `location`?**
Check exact (`=`) matches first and use one if found. Otherwise find the longest matching prefix; if it carries `^~`, use it. Otherwise test regex locations (`~`, `~*`) in file order and use the first match. If no regex matches, use the remembered longest prefix. Regexes are ordered by position, prefixes by length.
→ [server-and-location.md](../01-fundamentals/server-and-location.md)

**🟡 Q8. How does nginx choose a `server` block?**
Match the connection's IP and port against `listen`, then the `Host` header (or SNI for TLS) against `server_name`: exact, then leading wildcard, trailing wildcard, first matching regex. If nothing matches, the default server for that address and port (`default_server`, otherwise the first defined) handles it.
→ [server-and-location.md](../01-fundamentals/server-and-location.md)

---

## 2. Core Features

**🟢 Q9. Why do you set `Host`, `X-Forwarded-For`, and `X-Forwarded-Proto` when proxying?**
By default the backend sees nginx as the client and a different `Host`. `Host $host` preserves the original hostname. `X-Forwarded-For` / `X-Real-IP` pass the client IP. `X-Forwarded-Proto` tells the app whether the client used HTTPS, which prevents wrong redirects and mixed content when TLS terminates at nginx. The backend should trust these only from nginx.
→ [reverse-proxy.md](../02-core-features/reverse-proxy.md)

**🟡 Q10. What is the difference between `proxy_pass http://app;` and `proxy_pass http://app/;`?**
Without a URI part, the original request URI is passed unchanged. With any URI part (even `/`), the part of the request URI matching the `location` prefix is replaced by it. For `location /api/`, `/api/users` goes to `/api/users` in the first case and `/users` in the second. With regex locations or variables the rules differ.
→ [reverse-proxy.md](../02-core-features/reverse-proxy.md)

**🟡 Q11. How do you proxy WebSockets?**
Use HTTP/1.1 and forward the upgrade headers: `proxy_http_version 1.1; proxy_set_header Upgrade $http_upgrade; proxy_set_header Connection $connection_upgrade;`, where a `map` sets `$connection_upgrade` to `upgrade` when an Upgrade header exists and `close` otherwise. Raise `proxy_read_timeout` so idle sockets are not cut after 60 seconds.
→ [reverse-proxy.md](../02-core-features/reverse-proxy.md)

**🟢 Q12. What does TLS termination mean, and how do you redirect HTTP to HTTPS?**
nginx handles the TLS handshake and encryption with clients, and talks plain HTTP (or re-encrypted TLS) to backends. Redirect with a port-80 server: `return 301 https://$host$request_uri;`.
→ [tls-https.md](../02-core-features/tls-https.md)

**🟡 Q13. What certificate file do you give nginx, and what happens if you give the wrong one?**
`ssl_certificate` should be the **full chain** (your certificate plus intermediates). With only the leaf, many browsers fetch missing intermediates and work, but curl, mobile clients, and API clients fail verification.
→ [tls-https.md](../02-core-features/tls-https.md)

**🟡 Q14. What is HSTS, and what is the risk in enabling it?**
`Strict-Transport-Security` tells browsers to use HTTPS only for a period. The risk: browsers cache it for the full `max-age`, so if HTTPS later breaks (expired certificate, a subdomain without HTTPS), users cannot fall back. Start with a small `max-age` and increase gradually.
→ [tls-https.md](../02-core-features/tls-https.md)

**🟡 Q15. Why is `if` considered problematic in nginx?**
`if` in a `location` creates an implicit nested location and only some directives behave predictably inside it. `return` and `rewrite ... last` are safe. Others (`proxy_pass`, `add_header`, `try_files`) can silently misbehave. Prefer `map`, `try_files`, separate `location` blocks, or `return`.
→ [variables-and-rewrites.md](../02-core-features/variables-and-rewrites.md)

**🟡 Q16. `return` vs `rewrite`: when to use which?**
`return` stops processing immediately and is simplest and fastest. Use it for fixed redirects and quick responses. `rewrite` uses regex and can change the URI internally (`last`, `break`) or redirect (`redirect`, `permanent`). Use it when the target depends on regex captures or you need an internal rewrite.
→ [variables-and-rewrites.md](../02-core-features/variables-and-rewrites.md)

**🟡 Q17. `$uri` vs `$request_uri`?**
`$request_uri` is the original URI with query string, exactly as sent. `$uri` is the current URI, decoded and normalized, without the query string, and it changes after rewrites or internal redirects. Use `$request_uri` in redirects to preserve the query string.
→ [variables-and-rewrites.md](../02-core-features/variables-and-rewrites.md)

**🟢 Q18. What does `map` do and where must it be defined?**
It creates a variable whose value is derived from another variable using a lookup table (exact strings, regexes, defaults). It is evaluated lazily, only when the variable is used. It must be in the `http` context. Typical uses: redirect tables, WebSocket `Connection` header, cache-bypass flags, rate-limit keys.
→ [variables-and-rewrites.md](../02-core-features/variables-and-rewrites.md)

---

## 3. Traffic Management

**🟢 Q19. What load-balancing methods does open-source nginx support?**
Round robin (default, with weights), `least_conn`, `ip_hash`, generic `hash` (optionally `consistent`), and `random` (optionally `two least_conn`). Each fits different workloads: `least_conn` for uneven request durations, `hash ... consistent` for cache locality.
→ [load-balancing.md](../03-traffic-management/load-balancing.md)

**🟡 Q20. How does nginx detect a failed backend?**
Open-source nginx uses **passive** health checks: after `max_fails` failed attempts within `fail_timeout` (defaults 1 and 10s), the server is skipped for `fail_timeout`. What counts as a failure is set by `proxy_next_upstream`. Active health checks (periodic probes) are an NGINX Plus feature; with open source you use an external checker or a load balancer in front.
→ [load-balancing.md](../03-traffic-management/load-balancing.md)

**🟡 Q21. Is it safe to retry failed POST requests on another upstream?**
By default nginx does not retry non-idempotent requests once they have been sent upstream, because a retry could duplicate side effects (double charge, duplicate order). `non_idempotent` in `proxy_next_upstream` allows it, which is only safe if the backend is idempotent (for example with idempotency keys).
→ [load-balancing.md](../03-traffic-management/load-balancing.md)

**🟡 Q22. How do you enable keepalive to upstream servers, and why?**
`keepalive N;` in the `upstream`, plus `proxy_http_version 1.1;` and `proxy_set_header Connection "";` in the location. Reusing connections avoids a TCP (and TLS) handshake per request, lowering latency and CPU, and avoiding ephemeral-port exhaustion under load.
→ [load-balancing.md](../03-traffic-management/load-balancing.md)

**🟡 Q23. How do you implement session persistence in open source nginx?**
`ip_hash` or `hash $cookie_sessionid consistent;`. `ip_hash` is unreliable behind NAT or another proxy. Cookie-based sticky sessions are an NGINX Plus feature. The robust answer is to make the app stateless with a shared session store (Redis, database).
→ [load-balancing.md](../03-traffic-management/load-balancing.md)

**🟡 Q24. How does proxy caching work, and what is cached by default?**
`proxy_cache_path` defines the disk store and a shared-memory key zone; `proxy_cache` enables it per location; `proxy_cache_valid` sets lifetimes. Only GET/HEAD are cached. Responses with `Set-Cookie`, `Cache-Control: private/no-cache/no-store`, or `Vary: *` are not cached unless you explicitly ignore those headers. The default key is `$scheme$proxy_host$request_uri`.
→ [caching.md](../03-traffic-management/caching.md)

**🔴 Q25. What is a cache stampede and how does nginx mitigate it?**
When a popular entry expires or is missing, many requests hit the backend simultaneously. `proxy_cache_lock on;` lets one request populate the cache while others wait. `proxy_cache_use_stale updating` with `proxy_cache_background_update on;` serves stale content while one request refreshes it.
→ [caching.md](../03-traffic-management/caching.md)

**🟡 Q26. `proxy_cache_bypass` vs `proxy_no_cache`?**
`proxy_cache_bypass` means "do not **serve** this request from cache". `proxy_no_cache` means "do not **store** this response". For personalized traffic you need both, otherwise the bypassed response is still saved and may be served to others.
→ [caching.md](../03-traffic-management/caching.md)

**🔴 Q27. How do you purge cache in open-source nginx?**
There is no built-in purge. Options: short TTLs with stale serving, version the cache key (for example by deploy), delete cache files manually (named by the MD5 of the key), a third-party purge module, NGINX Plus `proxy_cache_purge`, or caching at a CDN with a purge API.
→ [caching.md](../03-traffic-management/caching.md)

**🟡 Q28. Explain `limit_req` with `burst` and `nodelay`.**
It implements a leaky bucket at a fixed rate. Without `burst`, any request exceeding the rate is rejected. With `burst=N`, up to N excess requests are queued and released at the configured rate (adding delay). With `burst=N nodelay`, the burst is served immediately but the slots refill only at the configured rate; requests beyond are rejected. `delay=M` serves the first M immediately and delays the rest.
→ [rate-limiting.md](../03-traffic-management/rate-limiting.md)

**🟡 Q29. What goes wrong when rate limiting behind a proxy or CDN?**
`$remote_addr` is the proxy's IP, so all users share one bucket and get throttled together. Restore the real client IP with the `realip` module (`set_real_ip_from`, `real_ip_header`) and trust only ranges you control. Otherwise clients can spoof `X-Forwarded-For` and bypass limits.
→ [rate-limiting.md](../03-traffic-management/rate-limiting.md)

**🟡 Q30. How do you exempt trusted clients from rate limiting?**
An empty key is not limited. Use `geo` to flag trusted IPs, then `map` that flag to an empty string, otherwise `$binary_remote_addr`, and use the mapped variable as the `limit_req_zone` key.
→ [rate-limiting.md](../03-traffic-management/rate-limiting.md)

---

## 4. Internals

**🟢 Q31. Describe nginx's process architecture.**
One master process (usually root) reads config, binds ports, manages workers, and handles signals. Several worker processes (unprivileged, typically one per CPU core) run an event loop and serve connections. Cache manager and cache loader processes exist if caching is used.
→ [architecture.md](../04-internals/architecture.md)

**🟡 Q32. Why is nginx efficient with many connections?**
Each worker is single-threaded and event-driven (`epoll`/`kqueue`): it works on whichever sockets are ready and never blocks waiting on a slow client or backend. Per-connection memory is small, so thousands of idle or slow connections are cheap, unlike a thread- or process-per-connection model.
→ [architecture.md](../04-internals/architecture.md)

**🔴 Q33. What blocks a worker, and why does it matter?**
Because one worker serves thousands of connections, any blocking operation stalls all of them: slow disk reads, blocking third-party modules, heavy scripts. Mitigation: `aio threads` for file I/O, `sendfile`, keeping custom code non-blocking, and monitoring.
→ [architecture.md](../04-internals/architecture.md)

**🟡 Q34. How many clients can nginx handle?**
Roughly `worker_processes × worker_connections` for static serving, and about half that for a reverse proxy since each request uses two connections (client side and upstream side). The practical limit is also bounded by OS file descriptors (`worker_rlimit_nofile`, systemd `LimitNOFILE`).
→ [architecture.md](../04-internals/architecture.md)

**🟡 Q35. What happens during `nginx -s reload`?**
The master validates the new config. If invalid, it keeps the old one and logs the error. If valid, it starts new workers with the new config, tells old workers to stop accepting new connections, and the old workers exit after finishing in-flight requests. No connections are dropped, but long-lived connections keep old workers alive until they close (limit with `worker_shutdown_timeout`).
→ [architecture.md](../04-internals/architecture.md)

**🔴 Q36. How do you upgrade the nginx binary without downtime?**
Replace the binary, send `USR2` to the master (starts a new master and workers; the old PID file becomes `.oldbin`), send `WINCH` to the old master to drain its workers, verify, then `QUIT` the old master. To roll back, send `HUP` to the old master and `QUIT` to the new one.
→ [operations.md](../06-production/operations.md)

**🔴 Q37. Name the request processing phases and why they matter.**
Post-read, server-rewrite, find-config (location selection), rewrite, post-rewrite, preaccess (`limit_req`, `limit_conn`), access (`allow/deny`, `auth_*`), post-access, precontent (`try_files`), content (proxy, static, etc.), then output filters and log. Phase order is fixed regardless of directive order in the file, so for example `limit_req` always runs before `allow/deny`, and `return` in the rewrite phase stops everything after it.
→ [request-lifecycle.md](../04-internals/request-lifecycle.md)

**🔴 Q38. What is an internal redirect, and what is the limit?**
A redirect invisible to the client that restarts location selection. Triggered by `rewrite ... last`, the final `try_files` fallback, `error_page`, `index`, and `X-Accel-Redirect`. A different location may then handle the request. nginx stops after 10 internal redirects with a 500.
→ [request-lifecycle.md](../04-internals/request-lifecycle.md)

**🔴 Q39. `rewrite ... last` vs `break`?**
`last` stops processing rewrite directives and searches for a new location matching the changed URI. `break` stops rewrite processing but continues inside the current location with the new URI.
→ [variables-and-rewrites.md](../02-core-features/variables-and-rewrites.md), [request-lifecycle.md](../04-internals/request-lifecycle.md)

**🔴 Q40. Why do upstream blocks use `zone`?**
Without a shared memory zone, each worker process tracks upstream state (failure counts, balancing state) separately, so failover behavior is inconsistent across workers. With `zone`, state is shared.
→ [secure-gateway-stack.md](../07-projects/secure-gateway-stack.md)

---

## 5. Security and Performance

**🟢 Q41. How do you hide the nginx version?**
`server_tokens off;`. It removes the version from the `Server` header and error pages. It does not hide that you use nginx, and it is not a substitute for patching.
→ [security-hardening.md](../05-security-performance/security-hardening.md)

**🟡 Q42. Name important security headers and what they prevent.**
`X-Content-Type-Options: nosniff` (MIME sniffing), `X-Frame-Options` or CSP `frame-ancestors` (clickjacking), `Content-Security-Policy` (XSS and injection), `Referrer-Policy` (URL leakage), `Permissions-Policy` (browser features), `Strict-Transport-Security` (downgrade attacks). Use `always` so they appear on error responses too.
→ [security-hardening.md](../05-security-performance/security-hardening.md)

**🟡 Q43. What is the `alias` path traversal issue?**
`location /img { alias /var/www/images/; }` (location without trailing slash, alias with one). A request for `/img../secret.txt` maps to `/var/www/images/../secret.txt`. Fix: make slashes match (`location /img/`), or use `root`.
→ [security-hardening.md](../05-security-performance/security-hardening.md)

**🟡 Q44. How do you protect an admin area?**
Layer controls: restrict by IP (`allow`/`deny`), require authentication (`auth_basic` or `auth_request` to an identity service), serve only over HTTPS, rate-limit it, log it, and combine with `satisfy all`. Make sure the IP check sees the real client IP.
→ [security-hardening.md](../05-security-performance/security-hardening.md)

**🟡 Q45. How do you stop requests for unknown hostnames from hitting a real site?**
Define a catch-all `default_server` that returns `444`, and on HTTPS add `ssl_reject_handshake on;` (1.19.4+) so no certificate is revealed for unknown names.
→ [security-hardening.md](../05-security-performance/security-hardening.md)

**🟡 Q46. How do you defend against slow-client (Slowloris-style) attacks?**
Short `client_header_timeout`, `client_body_timeout`, and `send_timeout`, bounded header and body sizes, `limit_conn` per IP, plus upstream DDoS protection for volumetric attacks. nginx's event-driven model tolerates slow clients, but limits prevent connection and descriptor exhaustion.
→ [security-hardening.md](../05-security-performance/security-hardening.md)

**🟡 Q47. Which settings most improve performance, in order?**
Caching (browser and `proxy_cache`), keepalive (client and upstream), compression for text, workers and file-descriptor limits, `sendfile`/open file cache for static files, buffer sizing, TLS session reuse with HTTP/2, then OS tuning. Always measure first, and check whether the backend is the real bottleneck.
→ [performance-tuning.md](../05-security-performance/performance-tuning.md)

**🟡 Q48. What should and should not be gzip-compressed?**
Compress text formats (HTML, CSS, JS, JSON, SVG, XML). Do not compress already-compressed formats (JPEG, PNG, WebP, MP4, ZIP, WOFF2). Set a minimum length, and avoid double compression with a CDN or backend. `gzip_static` serves pre-compressed files at no runtime cost. Be careful with responses that mix secrets and attacker-controlled input (BREACH).
→ [performance-tuning.md](../05-security-performance/performance-tuning.md)

**🔴 Q49. What does `sendfile on` do?**
It lets the kernel copy file data directly to the socket, skipping a copy through user space, which reduces CPU and memory bandwidth for static files. `tcp_nopush` works with it to send full packets. Avoid it on filesystems where it misbehaves (certain shared or virtualized mounts).
→ [performance-tuning.md](../05-security-performance/performance-tuning.md)

---

## 6. Production and Troubleshooting

**🟢 Q50. What is the safe way to change production config?**
Keep config in version control, edit, run `nginx -t`, `reload` (not `restart`), verify with `curl` and the error log, and roll back by reverting and reloading. Test the full merged config with `nginx -T` when includes are involved.
→ [operations.md](../06-production/operations.md)

**🟡 Q51. Reload vs restart?**
Reload re-reads config gracefully with no dropped connections and keeps the old config if the new one is invalid. Restart stops and starts the process, dropping connections. Restart is needed for changes outside nginx's config such as systemd limits.
→ [operations.md](../06-production/operations.md)

**🟡 Q52. 502 vs 504?**
502 Bad Gateway: nginx reached the upstream (or tried to) and got no valid response: connection refused, reset, closed early, invalid or oversized headers, no live upstreams. 504 Gateway Timeout: the upstream did not respond within `proxy_connect_timeout` or `proxy_read_timeout`. The error log message identifies which case.
→ [troubleshooting.md](../06-production/troubleshooting.md)

**🟡 Q53. What does a 499 mean?**
An nginx-specific code: the client closed the connection before nginx finished responding. Common causes are a slow backend, a client-side timeout, or users navigating away. Investigate backend latency.
→ [troubleshooting.md](../06-production/troubleshooting.md)

**🟡 Q54. A 502 appears only on a RHEL server while `curl` to the backend works. Why?**
Possibly SELinux blocking nginx from connecting to the backend (`Permission denied` in the error log). Check `getenforce` and `ausearch -m avc`, and allow with `setsebool -P httpd_can_network_connect 1`, or fix file labels with `restorecon`.
→ [troubleshooting.md](../06-production/troubleshooting.md)

**🟡 Q55. You get 403 on a file that exists. What do you check?**
Read the error log: "directory index forbidden" (missing index file and `autoindex off`), "Permission denied" (the worker user needs read on the file and execute on every parent directory: `namei -l`), "access forbidden by rule" (`deny` or wrong client IP), and SELinux labels.
→ [troubleshooting.md](../06-production/troubleshooting.md)

**🟡 Q56. How do you find out which `server` and `location` handled a request?**
Add a temporary diagnostic header (`add_header X-Debug-Server $server_name always;`) or log `$server_name $uri $upstream_addr`, use `curl -H "Host: ..."` against the server, run `nginx -T`, or enable `debug_connection` for one client with a debug build.
→ [troubleshooting.md](../06-production/troubleshooting.md)

**🟡 Q57. Why would nginx fail to start with "host not found in upstream"?**
nginx resolves hostnames in `upstream` and `proxy_pass` at startup and reload. If DNS cannot resolve it then, it fails. In dynamic environments, start backends first, reload after IP changes, or use `resolver` with a variable in `proxy_pass` for runtime resolution (which changes some behaviors).
→ [troubleshooting.md](../06-production/troubleshooting.md), [docker-kubernetes.md](../06-production/docker-kubernetes.md)

**🟡 Q58. What do you monitor on an nginx fleet?**
5xx and 502/504 rates, request and upstream latency percentiles, active connections against capacity, `accepts` vs `handled` from `stub_status`, cache hit ratio, disk usage (logs, cache), certificate expiry, process health, and error-log patterns such as "no live upstreams" or "Too many open files".
→ [operations.md](../06-production/operations.md)

**🟢 Q59. How do logs get rotated safely?**
`logrotate` renames the files, then sends `USR1` (or runs `nginx -s reopen`) so nginx reopens them. Without that signal nginx keeps writing to the rotated file.
→ [operations.md](../06-production/operations.md)

**🟡 Q60. What is `stub_status` and what do its numbers mean?**
A module exposing basic counters: active connections, total accepted and handled connections, total requests, and Reading/Writing/Waiting counts. `handled < accepts` indicates resource limits were hit. Expose it only on loopback or an internal network.
→ [operations.md](../06-production/operations.md)

---

## 7. Containers and Kubernetes

**🟡 Q61. How do you supply configuration to the official nginx Docker image?**
Mount config (`/etc/nginx/conf.d` or `nginx.conf`) read-only, bake it into a custom image, or use `/etc/nginx/templates/*.template` files, which the image processes with environment variables at start. Logs go to stdout/stderr.
→ [docker-kubernetes.md](../06-production/docker-kubernetes.md)

**🟡 Q62. In Kubernetes, a ConfigMap changed but nginx still serves the old config. Why?**
nginx does not reload automatically when mounted files change. Trigger a rolling update (for example by changing a config checksum annotation on the pod template), or use a sidecar that runs `nginx -s reload`.
→ [docker-kubernetes.md](../06-production/docker-kubernetes.md)

**🟡 Q63. What is the status of the community ingress-nginx controller?**
Kubernetes SIG Network announced its retirement, with best-effort maintenance only until March 2026 and no releases, bug fixes, or security patches afterwards. Existing installs keep running but are unsupported. The recommended path is the Gateway API or another maintained controller. It is distinct from F5's NGINX Ingress Controller and from running your own nginx Deployment. Verify the current state before answering, since this area moves quickly.
→ [docker-kubernetes.md](../06-production/docker-kubernetes.md)

**🔴 Q64. How do rate limits and caches behave with multiple nginx replicas?**
State is per instance. Rate limits scale with the replica count (N replicas allow roughly N times the limit), and each replica has its own cache. Use a shared layer (CDN, centralized limiter) if you need global behavior.
→ [docker-kubernetes.md](../06-production/docker-kubernetes.md)

---

## 8. Scenario Questions

**🟡 S1. Users refresh `/dashboard/settings` on your SPA and get 404. Fix it.**
The server looks for a file at that path. Add the fallback `try_files $uri $uri/ /index.html;` in `location /`. Keep `/assets/` returning 404 for missing files (`try_files $uri =404;`) so missing chunks do not return HTML.
→ [spa-with-api-proxy.md](../07-projects/spa-with-api-proxy.md)

**🟡 S2. After a deploy, some users see a blank page and "Unexpected token '<'" in the console. Why?**
Their cached old `index.html` references old hashed JS files that were deleted. A fallback then returned `index.html` (HTML) with 200 for a script URL. Fixes: keep previous releases' assets for a while, return 404 for missing `/assets/`, and serve `index.html` with `no-cache` so clients pick up new filenames promptly.
→ [spa-with-api-proxy.md](../07-projects/spa-with-api-proxy.md)

**🔴 S3. Your site is behind a CDN and everyone gets 429s from your rate limit. What happened?**
nginx sees only the CDN's IPs, so all users share a few buckets. Configure `realip` with the CDN's published ranges (and only those), restore the client IP from the forwarded header, and key the limit on that.
→ [rate-limiting.md](../03-traffic-management/rate-limiting.md)

**🔴 S4. Users occasionally see another user's personalized page. What is the likely cause?**
A cache serving personalized responses to others: caching enabled for requests with cookies or `Authorization`, a `proxy_cache_key` missing a varying part, ignored `Set-Cookie` / `Cache-Control: private`, or `proxy_cache_bypass` without `proxy_no_cache`. Fix the bypass rules, the key, and never ignore private headers.
→ [caching.md](../03-traffic-management/caching.md)

**🔴 S5. A backend is slow and occasionally down. How should nginx behave?**
Timeouts that fail fast (`proxy_connect_timeout 5s`), failover to other servers with `proxy_next_upstream` and a retry cap, `max_fails`/`fail_timeout`, `proxy_cache_use_stale` for cacheable content, rate/connection limits to protect it, JSON error responses via `error_page`, and monitoring of `$upstream_response_time`. Keep retries for non-idempotent requests off.
→ [load-balancing.md](../03-traffic-management/load-balancing.md), [secure-gateway-stack.md](../07-projects/secure-gateway-stack.md)

**🔴 S6. Design a gateway for three services with different security needs.**
Describe: one default server rejecting unknown hosts; TLS at `http` level with one SAN certificate and auto-renewal reload; per-service `server` blocks; API with tiered rate limits, a cache that bypasses authenticated requests, upstream groups with keepalive and `zone`; admin with IP allow-list plus authentication; docs as cached static files; snippets for shared headers and proxy params (re-included where `add_header` overrides); JSON logs with upstream timing; `stub_status` on loopback; `nginx -t` in CI and graceful reloads.
→ [secure-gateway-stack.md](../07-projects/secure-gateway-stack.md)

---

## 9. Hands-On Tasks

Try these on a test server, then compare with the linked files.

1. **Redirect and canonical host:** redirect all HTTP to HTTPS and `www.example.com` to `example.com`, preserving the path and query.
2. **Mixed routing:** serve `/` from static files, proxy `/api/` to `127.0.0.1:3000` without the `/api` prefix reaching the backend, and return 404 JSON for everything else under `/v1/`.
3. **Failover:** configure three backends with one `backup`; stop two and observe via `$upstream_addr` logging.
4. **Rate limit:** 5 requests per second per IP on `/api/`, 10 per minute on `/api/login`, internal IPs exempt, 429 JSON body with `Retry-After`.
5. **Cache:** cache anonymous `GET /public/` for 1 minute, bypass for requests with cookies, expose `X-Cache-Status`, serve stale if the backend is down.
6. **Debug:** introduce a deliberate typo, a wrong `alias` slash, and a stopped backend, and fix each using only `nginx -t`, logs, and `curl`.
7. **Zero downtime:** run `hey` or `wrk` against the server while reloading with a changed config, and confirm no errors.

Solutions are spread across [reverse-proxy.md](../02-core-features/reverse-proxy.md), [rate-limiting.md](../03-traffic-management/rate-limiting.md), [caching.md](../03-traffic-management/caching.md), [troubleshooting.md](../06-production/troubleshooting.md), and the two [projects](../07-projects/).

---

## Answering Tips
- **Lead with the one-sentence answer**, then add the nuance. For example: "`root` appends the URI, `alias` replaces the location prefix", then the trailing-slash caveat.
- **Name the trap.** Interviewers often probe gotchas: `add_header` inheritance, trailing-slash `proxy_pass`, regex order, stale DNS, `$remote_addr` behind proxies, caching personalized responses.
- **Show how you would verify:** `nginx -t`, `curl -v`, the error log line you would expect.
- **Distinguish open source from NGINX Plus** (active health checks, sticky cookies, purge) instead of implying everything is available.
- **Admit version dependence** (HTTP/2 directive syntax, HTTP/3, `ssl_reject_handshake`) and say you would check the docs for the deployed version.
- **Tie answers to production:** limits, timeouts, monitoring, rollback.

## Related
- [cheatsheet.md](cheatsheet.md): fast lookup of commands and snippets
- [ROADMAP.md](../ROADMAP.md): study order for the whole repository
