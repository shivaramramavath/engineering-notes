---
title: "Cheatsheet: NGINX Directives Alphabetical Quick Reference"
description: "Alphabetical lookup table of essential NGINX core, HTTP, proxy, TLS, caching, and rate limiting directives with valid contexts, syntax schemas, and default values."
module: "09-cheatsheets"
topic: "01-directives-quick-reference"
status: "completed"
tags: ["nginx", "cheatsheet", "directives", "reference", "syntax", "quick-reference"]
prerequisites: ["01-fundamentals/02-config-structure.md", "01-fundamentals/05-directives-reference-by-context.md"]
estimated_read_time: "25 min"
---

# Cheatsheet: NGINX Directives Alphabetical Quick Reference

```
+-------------------------------------------------------------------------------+
| NGINX Directives Context Legend                                               |
|                                                                               |
|  [M] Main / Global Context        [E] Events Context                          |
|  [H] HTTP Context                 [S] Server Context                          |
|  [L] Location Context             [U] Upstream Context                        |
|  [F] If-in-Location Context       [T] Stream (TCP/UDP) Context                |
+-------------------------------------------------------------------------------+
```

---

## 1. Core & Event Management Directives

| Directive | Contexts | Syntax & Arguments | Default | Description |
| :--- | :--- | :--- | :--- | :--- |
| `daemon` | `[M]` | `daemon on \| off;` | `on` | Determines whether NGINX forks into background (set `off` in Docker). |
| `env` | `[M]` | `env <VARIABLE>[=<value>];` | OS defaults | Binds host OS environment variables into NGINX master process. |
| `error_log` | `[M, H, S, L]` | `error_log <file> [level];` | `logs/error.log error`| Log path and severity (`debug`, `info`, `warn`, `error`, `crit`, `emerg`). |
| `include` | `ALL` | `include <file \| mask>;` | None | Reads and inlines external configuration files matching path pattern. |
| `load_module` | `[M]` | `load_module <path_to_so>;` | None | Loads dynamic shared object C modules at server startup. |
| `multi_accept` | `[E]` | `multi_accept on \| off;` | `off` | Directs worker to accept all new incoming sockets in one epoll pass. |
| `pid` | `[M]` | `pid <path>;` | `logs/nginx.pid` | Filesystem path storing the Master process ID. |
| `use` | `[E]` | `use <epoll \| kqueue \| select>;` | Auto-detected | Forces selection of specific OS event multiplexing mechanism. |
| `user` | `[M]` | `user <username> [group];` | `nobody nobody` | Unprivileged OS user and group for child worker processes. |
| `worker_connections` | `[E]` | `worker_connections <num>;` | `512` | Maximum simultaneous open connections (client + upstream) per worker. |
| `worker_cpu_affinity` | `[M]` | `worker_cpu_affinity auto \| <mask>;` | None | Binds worker processes to dedicated physical/virtual CPU cores. |
| `worker_processes` | `[M]` | `worker_processes auto \| <num>;` | `1` | Number of worker processes (set `auto` to match CPU core count). |
| `worker_rlimit_nofile` | `[M]` | `worker_rlimit_nofile <num>;` | OS default | Overrides system open file descriptor ceiling (`RLIMIT_NOFILE`). |

---

## 2. HTTP Engine & Connection Handling Directives

| Directive | Contexts | Syntax & Arguments | Default | Description |
| :--- | :--- | :--- | :--- | :--- |
| `client_body_buffer_size` | `[H, S, L]` | `client_body_buffer_size <size>;` | `8k \| 16k` | In-memory RAM buffer for reading client POST/PUT request bodies. |
| `client_body_timeout` | `[H, S, L]` | `client_body_timeout <time>;` | `60s` | Maximum interval between consecutive read operations on request body. |
| `client_header_buffer_size`| `[H, S]` | `client_header_buffer_size <size>;` | `1k` | Buffer allocated for reading client request line and headers. |
| `client_header_timeout` | `[H, S]` | `client_header_timeout <time>;` | `60s` | Maximum time to read client request headers (Slowloris defense). |
| `client_max_body_size` | `[H, S, L]` | `client_max_body_size <size>;` | `1m` | Maximum permitted request body size; rejects with HTTP 413 if exceeded. |
| `default_type` | `[H, S, L]` | `default_type <mime_type>;` | `text/plain` | Default MIME type emitted when file extension is unrecognized. |
| `http2` | `[H, S]` | `http2 on \| off;` | `off` | Enables modern HTTP/2 protocol support on listening ports (1.25.1+). |
| `http3` | `[H, S]` | `http3 on \| off;` | `off` | Enables HTTP/3 QUIC over UDP (requires 1.25.0+ and OpenSSL 3 / quictls). |
| `keepalive_requests` | `[H, S, L]` | `keepalive_requests <num>;` | `1000` | Max sequential requests served over single persistent client connection. |
| `keepalive_timeout` | `[H, S, L]` | `keepalive_timeout <time> [send];`| `75s` | Maximum idle time NGINX retains persistent client TCP socket open. |
| `listen` | `[S]` | `listen <endpoint> [options];` | `80` | Binds port, IP, and socket flags (`ssl`, `reuseport`, `default_server`). |
| `open_file_cache` | `[H, S, L]` | `open_file_cache max=N [inactive=T];`| `off` | Caches open file descriptors, sizes, and timestamps in RAM. |
| `reset_timedout_connection`| `[H, S]` | `reset_timedout_connection on \| off;`| `off` | Sends TCP RST on timed-out sockets, releasing memory instantly. |
| `sendfile` | `[H, S, L]` | `sendfile on \| off;` | `off` | Direct zero-copy file transfer from OS page cache to socket descriptor. |
| `server_name` | `[S]` | `server_name <name ...>;` | `""` | Virtual host domain names matched against client HTTP `Host` or SNI. |
| `server_tokens` | `[H, S, L]` | `server_tokens on \| off \| build;`| `on` | Emits or suppresses NGINX version string in headers and error pages. |
| `tcp_nodelay` | `[H, S, L]` | `tcp_nodelay on \| off;` | `on` | Disables Nagle's algorithm on persistent keepalive connections. |
| `tcp_nopush` | `[H, S, L]` | `tcp_nopush on \| off;` | `off` | Pairs with `sendfile` to accumulate full MSS packets before sending. |

---

## 3. Reverse Proxy & Upstream Directives

| Directive | Contexts | Syntax & Arguments | Default | Description |
| :--- | :--- | :--- | :--- | :--- |
| `ip_hash` | `[U]` | `ip_hash;` | None | Session sticky load balancing hashing client IPv4 class C subnet. |
| `keepalive` | `[U]` | `keepalive <connections>;` | None | Sets maximum idle persistent connections cached to upstream backends. |
| `least_conn` | `[U]` | `least_conn;` | None | Directs requests to backend with fewest active concurrent connections. |
| `proxy_buffer_size` | `[H, S, L]` | `proxy_buffer_size <size>;` | `4k \| 8k` | Buffer for reading the initial response header from upstream server. |
| `proxy_buffering` | `[H, S, L]` | `proxy_buffering on \| off;` | `on` | Enables memory/disk buffering of upstream responses (turn `off` for SSE). |
| `proxy_buffers` | `[H, S, L]` | `proxy_buffers <num> <size>;` | `8 4k\|8k` | Number and size of buffers allocated for upstream response body. |
| `proxy_connect_timeout` | `[H, S, L]` | `proxy_connect_timeout <time>;` | `60s` | Maximum time allowed to establish TCP connection with upstream backend. |
| `proxy_hide_header` | `[H, S, L]` | `proxy_hide_header <header>;` | None | Suppresses specified upstream response header before sending downstream. |
| `proxy_http_version` | `[H, S, L]` | `proxy_http_version 1.0 \| 1.1;` | `1.0` | HTTP protocol version used when proxying (set `1.1` for keepalives). |
| `proxy_next_upstream` | `[H, S, L]` | `proxy_next_upstream <events ...>;`| `error timeout` | Conditions under which request is retried on next upstream node. |
| `proxy_pass` | `[L, F]` | `proxy_pass <url>;` | None | Forwards request to target HTTP/HTTPS backend or upstream pool. |
| `proxy_read_timeout` | `[H, S, L]` | `proxy_read_timeout <time>;` | `60s` | Maximum time allowed between consecutive read operations from upstream. |
| `proxy_set_header` | `[H, S, L]` | `proxy_set_header <key> <val>;`| `Host, Conn` | Sets or overwrites HTTP headers forwarded to upstream backend. |
| `server` (in upstream) | `[U]` | `server <addr> [parameters];` | None | Registers backend host (`weight=N`, `max_fails=N`, `backup`, `down`). |

---

## 4. Caching & Rate Limiting Directives

| Directive | Contexts | Syntax & Arguments | Default | Description |
| :--- | :--- | :--- | :--- | :--- |
| `limit_conn` | `[H, S, L]` | `limit_conn <zone> <number>;` | None | Restricts maximum concurrent active sockets per configured key. |
| `limit_conn_zone` | `[H]` | `limit_conn_zone <key> zone=N:S;`| None | Allocates shared memory zone for connection limiting tracking. |
| `limit_req` | `[H, S, L]` | `limit_req zone=N [burst=B] [nodelay];`| None | Applies leaky bucket request rate limiting to incoming transactions. |
| `limit_req_status` | `[H, S, L]` | `limit_req_status <code>;` | `503` | Status code returned on rate limit rejection (recommend `429`). |
| `limit_req_zone` | `[H]` | `limit_req_zone <key> zone=N:S rate=R;`| None | Allocates shared memory zone and baseline rate for traffic shaping. |
| `proxy_cache` | `[H, S, L]` | `proxy_cache <zone \| off>;` | `off` | Assigns shared memory cache zone for storing reverse proxy responses. |
| `proxy_cache_bypass` | `[H, S, L]` | `proxy_cache_bypass <string ...>;`| None | Conditions under which cached response is bypassed and fetched fresh. |
| `proxy_cache_key` | `[H, S, L]` | `proxy_cache_key <string>;` | `$scheme$proxy_host$request_uri` | Unique string hashed to identify cached responses on disk. |
| `proxy_cache_lock` | `[H, S, L]` | `proxy_cache_lock on \| off;` | `off` | Prevents cache stampedes: only 1 request queries backend on miss. |
| `proxy_cache_path` | `[H]` | `proxy_cache_path <path> [options];`| None | Configures disk storage path, keys_zone, and max size for proxy cache. |
| `proxy_cache_use_stale` | `[H, S, L]` | `proxy_cache_use_stale <events ...>;`| `off` | Serves stale cached content during upstream timeouts or updating cycles. |
| `proxy_cache_valid` | `[H, S, L]` | `proxy_cache_valid [code] <time>;`| None | Defines caching lifespan for specific HTTP response status codes. |

---

## 5. TLS / SSL Security Directives

| Directive | Contexts | Syntax & Arguments | Default | Description |
| :--- | :--- | :--- | :--- | :--- |
| `ssl_certificate` | `[H, S]` | `ssl_certificate <path>;` | None | Path to public PEM-encoded SSL/TLS certificate chain. |
| `ssl_certificate_key` | `[H, S]` | `ssl_certificate_key <path>;` | None | Path to private RSA or ECDSA SSL/TLS key (`chmod 600 root:root`). |
| `ssl_ciphers` | `[H, S]` | `ssl_ciphers <ciphers_string>;` | `HIGH:!aNULL:!MD5` | Colon-delimited list of permitted cryptographic cipher suites. |
| `ssl_prefer_server_ciphers`| `[H, S]` | `ssl_prefer_server_ciphers on \| off;`| `off` | Enforces server cipher priority over client cipher priority (set `off`). |
| `ssl_protocols` | `[H, S]` | `ssl_protocols [TLSv1.2] [TLSv1.3];`| `TLSv1.2 TLSv1.3` | Permitted TLS protocol versions (never allow SSLv3/TLS1.0/TLS1.1). |
| `ssl_session_cache` | `[H, S]` | `ssl_session_cache shared:NAME:SIZE;`| `none` | Allocates in-memory shared cache for TLS session resumption. |
| `ssl_session_tickets` | `[H, S]` | `ssl_session_tickets on \| off;` | `on` | Disables stateless session tickets to preserve Perfect Forward Secrecy. |
| `ssl_session_timeout` | `[H, S]` | `ssl_session_timeout <time>;` | `5m` | Maximum validity lifespan of cached TLS sessions (recommend `1d`). |
| `ssl_stapling` | `[H, S]` | `ssl_stapling on \| off;` | `off` | Enables OCSP stapling in initial TLS handshake to accelerate setup. |
| `ssl_trusted_certificate` | `[H, S]` | `ssl_trusted_certificate <path>;` | None | Path to intermediate CA bundle for verifying stapled OCSP responses. |

---

[← Back to Module Index](README.md) | [Next Cheatsheet: 02-location-matching-table.md →](02-location-matching-table.md)
