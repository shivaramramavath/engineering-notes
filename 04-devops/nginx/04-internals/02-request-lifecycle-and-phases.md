---
title: "The 11 Request Processing Phases in NGINX"
description: "Deep dive into the NGINX HTTP request processing pipeline: the 11 sequential phases, phase return codes, filter chain execution, and why configuration order does not dictate execution order."
module: "04-internals"
track: "devops-nginx"
order: 2
difficulty: "Advanced"
prerequisites:
  - "01-fundamentals/03-server-and-location"
  - "04-internals/01-architecture-master-worker-and-event-loop"
tags:
  - nginx
  - request-lifecycle
  - http-phases
  - filter-chain
  - internals
  - c-architecture
last_updated: "2026-03-30"
---

# The 11 Request Processing Phases in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. The 11 Processing Phases Dissected](#4-the-11-processing-phases-dissected)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Demonstrating Directive Execution Order vs Config Order](#example-1-demonstrating-directive-execution-order-vs-config-order)
  - [Example 2: The `satisfy any` Multi-Module Access Gate](#example-2-the-satisfy-any-multi-module-access-gate)
  - [Example 3: Post-Read Phase IP Transformation (`real_ip`)](#example-3-post-read-phase-ip-transformation-real_ip)
  - [Example 4: The Filter Chain Pipeline (Headers & Body Transformations)](#example-4-the-filter-chain-pipeline-headers--body-transformations)
  - [Example 5: Internal Phase Tracing with NGINX Debug Log](#example-5-internal-phase-tracing-with-nginx-debug-log)
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

One of the most common points of confusion for engineers learning NGINX is why directives written in an NGINX configuration block **do not execute sequentially from top to bottom**. 

In an imperative programming language (or an Apache `.htaccess` script), statements execute line by line in the exact order they are written. In contrast, NGINX is a **declarative, phase-based state machine**.

When NGINX parses `nginx.conf`, it does not compile an imperative script. Instead, each module registers handler functions into one of **11 strictly ordered processing phases** (`ngx_http_phases`). When an HTTP request enters NGINX, it traverses these 11 phases in a fixed, immutable order. Understanding this phase pipeline is essential for debugging authorization bypasses, rate-limiting precedence, rewrite behavior, and module interactions.

---

## 2. Core Concepts

### The 11 HTTP Phases at a Glance
When an HTTP request is received, NGINX processes it through 11 sequential phases:
1. `NGX_HTTP_POST_READ_PHASE`: First phase after reading request headers. Modules inspect or transform the raw request (e.g., `ngx_http_realip_module`).
2. `NGX_HTTP_SERVER_REWRITE_PHASE`: Executes URI rewrite directives declared at the `server {}` level before location selection.
3. `NGX_HTTP_FIND_CONFIG_PHASE`: Location matching. Searches the radix tree of prefix locations and evaluates regular expressions to choose the best `location {}` block. (Hardcoded in core; no modules can hook here).
4. `NGX_HTTP_REWRITE_PHASE`: Executes rewrite directives declared inside the chosen `location {}` block.
5. `NGX_HTTP_POST_REWRITE_PHASE`: Evaluates the result of the rewrite phase. If the URI was altered by `rewrite ... last;`, NGINX re-routes the request back to `FIND_CONFIG` (up to 10 iterations to prevent infinite loops).
6. `NGX_HTTP_PREACCESS_PHASE`: Pre-access rate and connection limiting. Handlers enforce `limit_req` (request frequency) and `limit_conn` (concurrent sockets).
7. `NGX_HTTP_ACCESS_PHASE`: Security and authorization verification. Handlers enforce `allow`/`deny` (IP access lists), `auth_basic` (HTTP password auth), and `auth_request` (subrequest SSO).
8. `NGX_HTTP_POST_ACCESS_PHASE`: Resolves the `satisfy any` or `satisfy all` policy between multiple access modules.
9. `NGX_HTTP_PRECONTENT_PHASE`: Handlers execute before content generation (e.g., `try_files`, `mirror`).
10. `NGX_HTTP_CONTENT_PHASE`: Generates the response. Either an explicit content handler takes over (`proxy_pass`, `fastcgi_pass`, `return`), or NGINX falls back to static file delivery or autoindex.
11. `NGX_HTTP_LOG_PHASE`: Executes after the entire response has been transmitted to the client. Writes access logs (`access_log`) and records latency metrics.

### Phase Return Codes
Every module handler registered in a phase returns a status integer that instructs the NGINX core engine how to proceed:
- `NGX_OK`: Handler completed successfully; proceed to the next phase.
- `NGX_DECLINED`: Handler is not interested in this request; pass request to the next handler within the same phase.
- `NGX_AGAIN`: Asynchronous operation pending (waiting for network I/O or timer); yield back to `epoll`.
- `NGX_DONE`: Handler has assumed full ownership of the request (e.g., subrequest initiated); suspend current phase.
- `NGX_ERROR` / HTTP Status Code ($\ge 300$): Abort current processing and execute error handling (`error_page`).

### The Outgoing Filter Chain
Once the `CONTENT_PHASE` produces response headers and body buffers, data passes through a linked list of **Output Filters**:
- **Header Filters** (`ngx_http_header_filter`): Modify or inject HTTP response headers (e.g., `add_header`, `server_tokens`).
- **Body Filters** (`ngx_http_body_filter`): Transform streaming response body chunks (e.g., `gzip`, `brotli`, `sub_filter`, `image_filter`).

---

## 3. How It Works Internally

### The 11 Processing Phases Architecture

```
                               Incoming HTTP Request
                                         │
                                         ▼
                     ┌───────────────────────────────────────┐
                     │ 1. NGX_HTTP_POST_READ_PHASE           │
                     │    - ngx_http_realip_module           │
                     └───────────────────┬───────────────────┘
                                         ▼
                     ┌───────────────────────────────────────┐
                     │ 2. NGX_HTTP_SERVER_REWRITE_PHASE      │
                     │    - set, rewrite, return in server {}│
                     └───────────────────┬───────────────────┘
                                         ▼
                     ┌───────────────────────────────────────┐
                     │ 3. NGX_HTTP_FIND_CONFIG_PHASE         │
                     │    - Exact, Prefix, and Regex Match   │
                     └───────────────────┬───────────────────┘
                                         ▼
                     ┌───────────────────────────────────────┐
                     │ 4. NGX_HTTP_REWRITE_PHASE             │
                     │    - set, rewrite, return in location │
                     └───────────────────┬───────────────────┘
                                         ▼
                     ┌───────────────────────────────────────┐
                     │ 5. NGX_HTTP_POST_REWRITE_PHASE        │
                     │    - Detect 'last' rewrite loops (max 10)
                     └───────────────────┬───────────────────┘
                                         ▼
                     ┌───────────────────────────────────────┐
                     │ 6. NGX_HTTP_PREACCESS_PHASE           │
                     │    - limit_req (Rate Limiting)        │
                     │    - limit_conn (Connection Limiting) │
                     └───────────────────┬───────────────────┘
                                         ▼
                     ┌───────────────────────────────────────┐
                     │ 7. NGX_HTTP_ACCESS_PHASE              │
                     │    - allow / deny (IP filtering)      │
                     │    - auth_basic, auth_request         │
                     └───────────────────┬───────────────────┘
                                         ▼
                     ┌───────────────────────────────────────┐
                     │ 8. NGX_HTTP_POST_ACCESS_PHASE         │
                     │    - satisfy any | all evaluation     │
                     └───────────────────┬───────────────────┘
                                         ▼
                     ┌───────────────────────────────────────┐
                     │ 9. NGX_HTTP_PRECONTENT_PHASE          │
                     │    - try_files, mirror module         │
                     └───────────────────┬───────────────────┘
                                         ▼
                     ┌───────────────────────────────────────┐
                     │ 10. NGX_HTTP_CONTENT_PHASE            │
                     │     - proxy_pass, fastcgi_pass, return│
                     │     - static file sendfile()          │
                     └───────────────────┬───────────────────┘
                                         │ Response stream
                                         ▼
                     ┌───────────────────────────────────────┐
                     │          Output Filter Chain          │
                     │ - Header Filters (add_header, CSP)    │
                     │ - Body Filters (gzip, brotli, sub)    │
                     └───────────────────┬───────────────────┘
                                         │ Transmitted to client
                                         ▼
                     ┌───────────────────────────────────────┐
                     │ 11. NGX_HTTP_LOG_PHASE                │
                     │     - access_log, latency metrics     │
                     └───────────────────────────────────────┘
```

---

## 4. The 11 Processing Phases Dissected

### Phase 1: POST_READ
Executes immediately after reading and parsing the HTTP request headers from the client socket. Handlers here can modify the request structure before any routing occurs.
- *Primary Module*: `ngx_http_realip_module` inspects `X-Forwarded-For` or `CF-Connecting-IP` and rewrites `$remote_addr` so all subsequent phases see the real client IP.

### Phase 2: SERVER_REWRITE
Executes `set`, `rewrite`, and `return` directives declared at the `server {}` level.
- Used for canonical domain redirects (e.g., `return 301 https://$host$request_uri;`) before location matching.

### Phase 3: FIND_CONFIG
The core location matching phase. NGINX compares the normalized request URI against all configured `location` blocks using its internal prefix radix tree and PCRE regular expressions. Modules cannot register handlers in this phase.

### Phase 4: REWRITE
Executes `set`, `rewrite`, and `return` directives declared inside the chosen `location {}` block. Handlers evaluate variables and can alter the URI.

### Phase 5: POST_REWRITE
Evaluates the result of the `REWRITE` phase. If a directive altered the URI and specified the `last` flag (`rewrite ... last;`), NGINX increments an internal jump counter and returns the request to Phase 3 (`FIND_CONFIG`). If this cycle repeats more than 10 times, NGINX aborts with an HTTP `500 Internal Server Error` (Rewrite or internal redirection cycle).

### Phase 6: PREACCESS
Executes resource-limiting controls before expensive authentication or database checks.
- *Primary Modules*: `ngx_http_limit_req_module` and `ngx_http_limit_conn_module`. If a client violates limits, NGINX rejects the request immediately, protecting downstream authentication backends.

### Phase 7: ACCESS
Enforces security, authorization, and identity verification.
- Handlers: `ngx_http_access_module` (`allow`/`deny`), `ngx_http_auth_basic_module`, `ngx_http_auth_request_module`.

### Phase 8: POST_ACCESS
Resolves the `satisfy` directive:
- `satisfy all;` (default): Both IP access lists (`allow`/`deny`) AND authentication (`auth_basic`/`auth_request`) must pass.
- `satisfy any;`: If IP access passes, authentication is skipped, and vice-versa.

### Phase 9: PRECONTENT
Executes specialized handlers that determine content availability before main content generation begins.
- *Primary Modules*: `ngx_http_try_files_module` checks filesystem paths sequentially; `ngx_http_mirror_module` dispatches background shadow requests.

### Phase 10: CONTENT
The heart of NGINX: produces the HTTP response body and status code.
- If an explicit content handler is declared (`proxy_pass`, `fastcgi_pass`, `return`, `js_content`), it takes complete ownership of response generation.
- If no explicit handler is defined, NGINX executes default handlers: `ngx_http_static_module` (reads file from disk), `ngx_http_autoindex_module`, or `ngx_http_index_module`.

### Phase 11: LOG
Executes after the final byte of the response has been written and acknowledged by the client socket. Handlers record telemetry, execution timings, and upstream response metrics into `access_log`.

---

## 5. Practical Implementation Examples

### Example 1: Demonstrating Directive Execution Order vs Config Order

A classic puzzle: in what order do these directives execute?

```nginx
# /etc/nginx/conf.d/order_puzzle.conf

server {
    listen 80;
    server_name puzzle.local;

    location /test {
        # Config line 1: echo/return (CONTENT phase)
        return 200 "Content generated successfully!\n";

        # Config line 2: Rate limit (PREACCESS phase)
        limit_req zone=one;

        # Config line 3: IP denial (ACCESS phase)
        deny 192.168.1.50;

        # Config line 4: Variable set (REWRITE phase)
        set $my_var "initialized";
    }
}
```

#### Actual Execution Order in the NGINX Pipeline:
1. **Phase 4 (REWRITE)**: `set $my_var "initialized";` runs first.
2. **Phase 6 (PREACCESS)**: `limit_req zone=one;` evaluates next. If rate limit is breached, returns 429 immediately!
3. **Phase 7 (ACCESS)**: `deny 192.168.1.50;` evaluates next. If client matches IP, returns 403 immediately!
4. **Phase 10 (CONTENT)**: `return 200 "Content...";` executes last.
*Notice that line 4 executed first, and line 1 executed last!*

### Example 2: The `satisfy any` Multi-Module Access Gate

Permit internal corporate IP addresses to access an administrative portal without a password, while requiring external public users to authenticate via HTTP Basic Auth.

```nginx
# /etc/nginx/conf.d/satisfy_any.conf

server {
    listen 80;
    server_name admin.example.com;

    location / {
        # Policy: Either IP whitelist OR Basic Auth must succeed!
        satisfy any;

        # Check 1: IP Access Rules (Phase 7 - ngx_http_access_module)
        allow 10.0.0.0/8;
        allow 192.168.1.0/24;
        deny  all;

        # Check 2: HTTP Basic Auth (Phase 7 - ngx_http_auth_basic_module)
        auth_basic           "Administrator Access Required";
        auth_basic_user_file /etc/nginx/.htpasswd;

        # Evaluated in Phase 8 (POST_ACCESS) -> Passes if EITHER condition is true!

        proxy_pass http://internal_admin_service;
        proxy_set_header Host $host;
    }
}
```

### Example 3: Post-Read Phase IP Transformation (`real_ip`)

Demonstrates how `real_ip` executes in the `POST_READ` phase so that subsequent phases (Rewrite, Preaccess, Access) see the restored client IP rather than the load balancer's IP.

```nginx
# /etc/nginx/conf.d/realip_pipeline.conf

# Executes in Phase 1 (POST_READ)
set_real_ip_from 10.0.0.0/8;       # Trust internal AWS ALB
real_ip_header   X-Forwarded-For;
real_ip_recursive on;

# Rate limiting in Phase 6 (PREACCESS)
# Because real_ip ran in Phase 1, $binary_remote_addr contains the REAL client IP!
limit_req_zone $binary_remote_addr zone=real_client_limit:10m rate=10r/s;

server {
    listen 80;
    server_name api.example.com;

    location / {
        # Enforces rate limit against real user IP, not ALB IP!
        limit_req zone=real_client_limit burst=5 nodelay;

        # IP Filtering in Phase 7 (ACCESS) operates on real user IP!
        deny 203.0.113.100;

        proxy_pass http://backend_pool;
    }
}
```

### Example 4: The Filter Chain Pipeline (Headers & Body Transformations)

Demonstrates how the content phase hands data over to the output filter chain for header injection and dynamic text substitution.

```nginx
# /etc/nginx/conf.d/filter_chain.conf

server {
    listen 80;
    server_name news.example.com;

    location /articles/ {
        proxy_pass http://news_backend;

        # --- Filter 1: Header Filter ---
        # ngx_http_headers_filter_module injects headers
        add_header X-Content-Filtered "true" always;
        add_header X-Frame-Options "DENY" always;

        # --- Filter 2: Sub Filter (Body Transformation) ---
        # ngx_http_sub_filter_module dynamically rewrites response body strings
        sub_filter_once off;
        sub_filter_types text/html;
        sub_filter 'http://cdn.old.net' 'https://cdn.new.net';

        # --- Filter 3: Gzip Filter ---
        # ngx_http_gzip_filter_module compresses the transformed body
        gzip on;
        gzip_types text/html text/plain;
    }
}
```

### Example 5: Internal Phase Tracing with NGINX Debug Log

Enabling the internal NGINX debug log to trace every phase transition step-by-step.

```nginx
# /etc/nginx/nginx.conf
error_log /var/log/nginx/debug.log debug;

# Trace sample in debug.log when a request is processed:
# 2026/03/30 [debug] 1234#1234: *1 HTTP post read phase: 0
# 2026/03/30 [debug] 1234#1234: *1 HTTP server rewrite phase: 1
# 2026/03/30 [debug] 1234#1234: *1 find configuration for "/api/users"
# 2026/03/30 [debug] 1234#1234: *1 HTTP rewrite phase: 3
# 2026/03/30 [debug] 1234#1234: *1 HTTP preaccess phase: 5
# 2026/03/30 [debug] 1234#1234: *1 HTTP access phase: 6
# 2026/03/30 [debug] 1234#1234: *1 HTTP content phase: 9
# 2026/03/30 [debug] 1234#1234: *1 HTTP header filter
# 2026/03/30 [debug] 1234#1234: *1 HTTP body filter
# 2026/03/30 [debug] 1234#1234: *1 HTTP log phase: 10
```

---

## 6. Production Best Practices

1. **Never Assume Sequential Execution**: Never assume that placing directive A above directive B in a location block guarantees A runs before B. Always consult the 11-phase architecture.
2. **Be Mindful of Content Phase Exclusivity**: In the `CONTENT` phase, only **one** content generator can execute. If you declare both `proxy_pass http://backend;` and `try_files $uri /index.html;`, `try_files` (precontent) intercepts the request, and `proxy_pass` will be ignored or behave unpredictably.
3. **Use `satisfy any` for Secure Maintenance Backdoors**: Use `satisfy any;` to provide friction-free access to trusted office CIDRs while enforcing strict multi-factor or basic authentication on public networks.
4. **Leverage the Post-Rewrite Limit**: Be aware that `rewrite ... last;` causes an internal jump back to `FIND_CONFIG`. If you write circular rewrites, NGINX enforces a hard cap of **10 internal redirects** before failing with HTTP 500 to protect the worker event loop.
5. **Always Place Security Filters Early**: Ensure rate limits (`limit_req` in Phase 6) and IP bans (`deny` in Phase 7) protect expensive authentication subrequests (`auth_request` in Phase 7) from Denial of Service floods.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Assuming `echo` / `return` executes before `deny`** | Directives execute by phase; `deny` (Access Phase) executes *before* `return` (Content Phase), so unauthorized users are blocked regardless of config order. | Understand that Access Phase always precedes Content Phase. |
| **Multiple Content Handlers in one location** | Declaring `root /var/www;` and `proxy_pass http://backend;` in the same block causes conflicts; only one handler generates content. | Use separate `location` blocks for static files vs reverse proxying. |
| **Infinite Rewrite Loops (`rewrite ^ /test last;`)** | Enters an infinite cycle between Phase 4 and Phase 3; NGINX terminates after 10 loops, returning `500 Internal Server Error`. | Ensure regexes have termination conditions or use `break` instead of `last`. |
| **Thinking `add_header` runs before body filters** | In the filter chain, the Header Filter runs first to emit response headers; subsequent Body Filters cannot alter headers (e.g., adding `Content-Length`). | Let NGINX calculate chunked encoding for transformed body filters. |
| **Confusing `try_files` with `proxy_pass`** | `try_files` executes in the `PRECONTENT` phase. If the file is found, it terminates the phase, completely skipping `proxy_pass`! | Use `@named` fallback locations with `try_files $uri $uri/ @backend;`. |
| **Using `rewrite ... break;` when new location needed** | `break` stops rewrite processing and remains in the current location; if the rewritten URI requires a different location handler, requests 404. | Use `last` if you want NGINX to re-enter Phase 3 (`FIND_CONFIG`). |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Verify Configuration Syntax and Compile Phase Trees
nginx -t

# 2. Trace 11-Phase Execution using the NGINX Debug Binary
# (Install debug package: apt-get install nginx-core-dbg)
/usr/sbin/nginx-debug -c /etc/nginx/nginx.conf

# 3. Filter Debug Log for Specific Phase Transitions
grep -E "phase:|find configuration" /var/log/nginx/debug.log

# 4. Detect Rewrite Loops in Error Logs
tail -f /var/log/nginx/error.log | grep "rewrite or internal redirection cycle"
# Sample error:
# 2026/03/30 [error] 1234#1234: *1 rewrite or internal redirection cycle while internally redirecting to "/index.html"
```

### Phase Execution Benchmark
Because the 11 phases are executed via direct C function pointer arrays inside the `ngx_http_request_t` struct, advancing from Phase 1 through Phase 10 consumes less than **200 CPU clock cycles** ($<0.0001\text{ms}$) per request.

---

## 9. Architectural Comparison Table

| Phase | Phase Name | Primary Modules / Directives | Phase Purpose |
| :--- | :--- | :--- | :--- |
| **1** | `POST_READ` | `real_ip` | Early request transformation (client IP rewrite) |
| **2** | `SERVER_REWRITE` | `rewrite`, `set`, `return` in `server {}` | Global domain canonical redirects |
| **3** | `FIND_CONFIG` | Core NGINX Radix / Regex Engine | Location block selection |
| **4** | `REWRITE` | `rewrite`, `set`, `return` in `location {}` | URI transformation and internal variable sets |
| **5** | `POST_REWRITE` | Core Rewrite Loop Guard | Enforces 10-cycle cap on `rewrite ... last` |
| **6** | `PREACCESS` | `limit_req`, `limit_conn` | Rate and concurrency policing |
| **7** | `ACCESS` | `allow`, `deny`, `auth_basic`, `auth_request` | Authorization and identity checks |
| **8** | `POST_ACCESS` | `satisfy any\|all` | Resolves multi-module access criteria |
| **9** | `PRECONTENT` | `try_files`, `mirror` | Pre-generation checks and traffic shadowing |
| **10**| `CONTENT` | `proxy_pass`, `fastcgi_pass`, `static` file | Response payload generation |
| **11**| `LOG` | `access_log` | Request metrics recording after client write |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: Why does a deny all directive execute before a return 200 directive placed above it in the same location block?</strong></summary>

**Answer:**
Directives in NGINX do not execute in top-to-bottom textual order. Instead, they execute in the order of the **11 internal HTTP processing phases** defined in the NGINX C core:
- The `deny all;` directive belongs to `ngx_http_access_module`, which registers its handler in **Phase 7 (`NGX_HTTP_ACCESS_PHASE`)**.
- The `return 200;` directive belongs to `ngx_http_rewrite_module` when acting as a content generator in **Phase 10 (`NGX_HTTP_CONTENT_PHASE`)**.

Because Phase 7 strictly precedes Phase 10 in the NGINX state machine, NGINX evaluates the access check first. When it encounters `deny all`, it immediately halts request processing and returns `403 Forbidden`, never reaching the content generation phase.
</details>

<details>
<summary><strong>Q2: What is the purpose of the POST_REWRITE phase, and what error occurs if it fails?</strong></summary>

**Answer:**
The `POST_REWRITE` phase (Phase 5) is a safety guard that executes immediately after the `REWRITE` phase. 

If a directive in Phase 4 altered the request URI using the `last` flag (`rewrite ^/a /b last;`), NGINX must search for a new matching `location` block for the modified URI `/b`. To do this, NGINX increments an internal redirection counter on the request struct and jumps back to Phase 3 (`FIND_CONFIG`).

If a developer accidentally creates a circular rewrite rule (e.g., `/b` rewrites back to `/a`), NGINX would enter an infinite loop, freezing the worker thread. The `POST_REWRITE` phase enforces a hard limit of **10 internal cycles**. If the counter exceeds 10, NGINX aborts the request, logs an error, and returns:
`HTTP 500 Internal Server Error (rewrite or internal redirection cycle)`
</details>

<details>
<summary><strong>Q3: How does satisfy any differ from satisfy all in Phase 8 (POST_ACCESS)?</strong></summary>

**Answer:**
In Phase 7 (`ACCESS`), multiple independent authorization modules can register handlers (e.g., `ngx_http_access_module` for IP whitelists and `ngx_http_auth_basic_module` for password auth).

In Phase 8 (`POST_ACCESS`), NGINX evaluates the outcome:
- **`satisfy all;` (Default)**: The request must successfully pass **every** access module. If the client IP is allowed, but they fail password authentication, access is denied.
- **`satisfy any;`**: The request is approved if **at least one** access module succeeds. If an incoming client connects from an allowed IP subnet (e.g., corporate VPN), NGINX grants access immediately without prompting for basic auth passwords.
</details>

<details>
<summary><strong>Q4: Why does real_ip have to execute in the POST_READ phase?</strong></summary>

**Answer:**
The `POST_READ` phase (Phase 1) is the very first phase that executes after NGINX finishes reading and parsing the HTTP request headers from the network socket.

The `ngx_http_realip_module` must execute in Phase 1 so that it can inspect headers like `X-Forwarded-For` or `CF-Connecting-IP` and rewrite the internal `$remote_addr` variable *before* any subsequent phases execute. If `real_ip` ran later (e.g., in Phase 7), earlier phases (such as `SERVER_REWRITE`, `FIND_CONFIG`, and `PREACCESS` rate limiting) would evaluate against the proxy's IP address rather than the real client's IP.
</details>

<details>
<summary><strong>Q5: What happens during the FIND_CONFIG phase, and why can no third-party module hook into it?</strong></summary>

**Answer:**
Phase 3 (`FIND_CONFIG`) is the internal location selection engine. It takes the normalized URI and matches it against all defined `location` blocks using NGINX's compiled prefix radix tree and PCRE regular expression lists.

No module can hook into `FIND_CONFIG` because it is hardcoded into NGINX's core routing logic (`ngx_http_core_find_config_phase`). The integrity of the location matching algorithm is critical to the predictable operation of the entire server, so NGINX forbids arbitrary module code from intercepting or mutating location matching.
</details>

<details>
<summary><strong>Q6: Explain the difference between Header Filters and Body Filters in the output filter chain.</strong></summary>

**Answer:**
Once the `CONTENT` phase produces a response, it is passed through two sequential filter chains:
1. **Header Filters (`ngx_http_top_header_filter`)**: Handlers that inspect and mutate HTTP response headers (e.g., adding headers via `add_header`, setting cookies, modifying status codes, or configuring Content-Type). Header filters execute first and serialize the HTTP response header buffer onto the wire.
2. **Body Filters (`ngx_http_top_body_filter`)**: Handlers that inspect and transform the response body streaming data chunks (buffers) before they are sent to the client (e.g., compressing via `gzip`/`brotli`, replacing strings via `sub_filter`, or image resizing via `image_filter`).
</details>

<details>
<summary><strong>Q7: In which phase does try_files execute, and what happens if a file is matched?</strong></summary>

**Answer:**
`try_files` executes in **Phase 9 (`NGX_HTTP_PRECONTENT_PHASE`)**. 

When `try_files $uri $uri/ /index.html;` executes:
1. It checks the filesystem for the first file `$uri`. If found, it halts phase processing, sets the content handler to `ngx_http_static_module`, and delivers the file.
2. If not found, it checks the directory `$uri/`.
3. If neither exists, it executes an internal redirect to the final parameter `/index.html`, restarting the pipeline from Phase 3 (`FIND_CONFIG`).
Because `try_files` executes in Phase 9, any directive declared in Phase 10 (such as `proxy_pass`) in that same location will be completely bypassed if `try_files` finds a matching file on disk!
</details>

<details>
<summary><strong>Q8: What is the purpose of Phase 11 (LOG_PHASE), and can it modify the client response?</strong></summary>

**Answer:**
Phase 11 (`NGX_HTTP_LOG_PHASE`) executes after the final byte of the HTTP response has been completely written to the client network socket and closed or placed into keepalive.

Handlers registered in this phase (primarily `ngx_http_log_module`) format and write entries to `access_log` and calculate total request latency (`$request_time`). 

Because the response headers and body have already been transmitted over the wire to the client, **Phase 11 cannot modify the response, alter status codes, or inject headers**. It is strictly a post-processing telemetry phase.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Verify That Access Phase Precedes Content Phase</strong></summary>

**Goal:** Create a location block with `return 200 "Success\n";` and `deny all;` and observe that NGINX returns HTTP 403 Forbidden.

**Solution:**
```nginx
server {
    listen 80;
    server_name phase.local;

    location /test {
        return 200 "Success\n";
        deny all;
    }
}
```
Test with `curl -i http://phase.local/test`. Output confirms `HTTP/1.1 403 Forbidden` because `deny` (Phase 7) runs before `return` (Phase 10).
</details>

<details>
<summary><strong>Exercise 2: Implement satisfy any with IP Whitelist and Basic Auth</strong></summary>

**Goal:** Allow access to `127.0.0.1` without credentials, but require Basic Auth for all other clients.

**Solution:**
```nginx
server {
    listen 80;
    server_name secure.local;

    location / {
        satisfy any;

        allow 127.0.0.1;
        deny all;

        auth_basic "Restricted";
        auth_basic_user_file /etc/nginx/.htpasswd;

        return 200 "Access granted!\n";
    }
}
```
Test from `localhost` (returns 200 without password).
</details>

<details>
<summary><strong>Exercise 3: Trigger and Observe a 10-Cycle Rewrite Loop</strong></summary>

**Goal:** Intentionally create a circular rewrite loop between two URIs and inspect the resulting HTTP 500 in `error.log`.

**Solution:**
```nginx
server {
    listen 80;
    server_name loop.local;

    location /loop-a {
        rewrite ^/loop-a /loop-b last;
    }

    location /loop-b {
        rewrite ^/loop-b /loop-a last;
    }
}
```
Query `curl http://loop.local/loop-a` -> Returns `500 Internal Server Error`.
Inspect log: `grep "rewrite or internal redirection cycle" /var/log/nginx/error.log`.
</details>

<details>
<summary><strong>Exercise 4: Verify Post-Read Phase IP Replacement with real_ip</strong></summary>

**Goal:** Configure `set_real_ip_from` and verify that `$remote_addr` reflects the simulated `X-Forwarded-For` header.

**Solution:**
```nginx
set_real_ip_from 127.0.0.1;
real_ip_header   X-Forwarded-For;

server {
    listen 80;
    server_name realip.local;

    location / {
        return 200 "Client IP: $remote_addr\n";
    }
}
```
Test with:
```bash
curl -H "X-Forwarded-For: 203.0.113.199" http://realip.local/
# Output: Client IP: 203.0.113.199
```
</details>

<details>
<summary><strong>Exercise 5: Trace Request Phases in Debug Log</strong></summary>

**Goal:** Enable `error_log ... debug;` and trace the 11 phase names in `debug.log` for a single request.

**Solution:**
```nginx
error_log /var/log/nginx/debug.log debug;
```
Run `curl http://localhost/` and inspect:
```bash
grep -E "phase: [0-9]+" /var/log/nginx/debug.log
```
</details>

<details>
<summary><strong>Exercise 6: Observe try_files Bypassing Content Handlers</strong></summary>

**Goal:** Create a file `/var/www/test.txt` and demonstrate that `try_files` serves the file in Phase 9, completely bypassing a `proxy_pass` in Phase 10.

**Solution:**
```nginx
server {
    listen 80;
    root /var/www;

    location / {
        try_files $uri =404;
        proxy_pass http://127.0.0.1:9999; # Dead port
    }
}
```
Create `/var/www/hello.txt` and request `http://localhost/hello.txt`. NGINX returns the file with 200 OK without ever touching the dead port 9999!
</details>

---

## 12. Summary & Quick Reference

```
# The 11 Processing Phases Quick Reference

Phase 1:  POST_READ       (real_ip)
Phase 2:  SERVER_REWRITE  (rewrite/set in server {})
Phase 3:  FIND_CONFIG     (location matching via radix tree/regex)
Phase 4:  REWRITE         (rewrite/set in location {})
Phase 5:  POST_REWRITE    (guard against infinite rewrite loops)
Phase 6:  PREACCESS       (limit_req, limit_conn)
Phase 7:  ACCESS          (allow/deny, auth_basic, auth_request)
Phase 8:  POST_ACCESS     (satisfy any | all evaluation)
Phase 9:  PRECONTENT      (try_files, mirror)
Phase 10: CONTENT         (proxy_pass, fastcgi, static sendfile)
Filter:   OUTPUT FILTERS  (Header & Body transformations, gzip, sub)
Phase 11: LOG             (access_log telemetry recording)
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX Development Guide: HTTP Request Phases](https://nginx.org/en/docs/dev/development_guide.html#http_phases)
- [Emiller's Advanced Guide to NGINX Module Development](http://www.evanmiller.org/nginx-modules-guide.html)
- [Official NGINX ngx_http_core_module satisfy Documentation](https://nginx.org/en/docs/http/ngx_http_core_module.html#satisfy)
- [NGINX Internals: The Request State Machine](https://www.nginx.com/resources/library/complete-nginx-cookbook/)

---

[← Previous: Process Architecture & Event Loop](01-architecture-master-worker-and-event-loop.md) | [Module Overview](README.md) | [Next: Config Parsing and Reload Mechanics →](03-config-parsing-and-reload-mechanics.md)
