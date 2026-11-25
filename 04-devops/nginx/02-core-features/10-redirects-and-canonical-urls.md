---
title: "Redirects and Canonical URLs in NGINX"
description: "Mastering HTTP redirects, canonical domain consolidation, trailing slash governance, and query preservation using 301, 302, 307, and 308 in NGINX."
module: "02-core-features"
track: "devops-nginx"
order: 10
difficulty: "Intermediate"
prerequisites:
  - "01-fundamentals/03-server-and-location"
  - "02-core-features/04-variables-and-rewrites"
tags:
  - nginx
  - redirects
  - canonical-urls
  - 301-vs-308
  - trailing-slash
  - seo
  - routing
last_updated: "2026-03-30"
---

# Redirects and Canonical URLs in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Strict HTTP to HTTPS Apex Redirection](#example-1-strict-http-to-https-apex-redirection)
  - [Example 2: Canonical www vs Non-www Domain Consolidation](#example-2-canonical-www-vs-non-www-domain-consolidation)
  - [Example 3: Trailing Slash Governance (Enforce vs Strip)](#example-3-trailing-slash-governance-enforce-vs-strip)
  - [Example 4: REST API Redirections Preserving HTTP POST (308 Permanent)](#example-4-rest-api-redirections-preserving-http-post-308-permanent)
  - [Example 5: High-Performance Legacy URL Migration via `map`](#example-5-high-performance-legacy-url-migration-via-map)
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

In web infrastructure and search engine optimization (SEO), **canonicalization** is the process of ensuring that every resource is accessible via a single, authoritative, normalized URL. Allowing the same web page or API resource to be accessed via multiple distinct URLs (e.g., `http://` vs `https://`, `www.example.com` vs `example.com`, `/products` vs `/products/`, or uppercase `/About` vs `/about`):
1. **Splits SEO Link Equity**: Search engines treat duplicate URLs as separate pages, dividing page authority and causing ranking penalties.
2. **Breaks Caching Layers**: CDNs, browser caches, and reverse proxies cache URLs based on exact string keys, resulting in duplicate cached entries and wasted memory.
3. **Breaks CORS and Cookies**: Cookies set on `example.com` may not be accessible to `www.example.com`, and cross-origin security rules reject mismatched origin strings.
4. **Mutates HTTP Methods**: Historically, returning a standard `301 Moved Permanently` on a `POST` or `PUT` request caused browsers to mutate the request method to `GET`, dropping the payload body and corrupting transactional flows.

Executing canonical redirects directly at the NGINX edge guarantees ultra-fast redirection ($<1\text{ms}$ response time) before application workers are ever invoked.

---

## 2. Core Concepts

### The HTTP Redirection Status Codes
Understanding the exact semantics of redirection status codes defined in RFC 9110 / RFC 7538 is essential:

- **`301 Moved Permanently`**: The resource has permanently moved. **Historical caveat**: Browsers are permitted to rewrite the HTTP method from `POST` to `GET` and drop the request body. Browsers cache 301 redirects aggressively in local disk caches.
- **`302 Found`**: Temporary redirect. Browsers historically rewrite `POST` to `GET`. Not cached by default.
- **`307 Temporary Redirect`**: Modern temporary redirect. **Strictly mandates that the HTTP method and request body MUST NOT be altered**. A `POST` remains a `POST`.
- **`308 Permanent Redirect`**: Modern permanent redirect. **Strictly mandates that the HTTP method and request body MUST NOT be altered**. A `POST` remains a `POST`. This is the modern successor to 301 for APIs.

### The Immutable `$request_uri`
When redirecting incoming requests, always use `$request_uri` rather than `$uri`:
- `$request_uri`: Contains the complete, original, raw request URI sent by the client, including the path and unparsed query arguments (e.g., `/search?category=shoes&page=2`).
- `$uri`: The normalized path *without* query arguments. Redirecting to `$uri` strips query strings unless `$is_args$args` is explicitly appended.

### Trailing Slash Nuances
Web servers historically treated `/folder/` (with a trailing slash) as a directory index request, and `/folder` (without a trailing slash) as a file request. If an endpoint expects a trailing slash, NGINX's static file module (`ngx_http_static_module`) will automatically issue a 301 redirect to `/folder/` if a directory matches. However, behind a reverse proxy or in single-page applications, trailing slash mismatches cause 404 errors or duplicate API endpoints unless explicitly governed.

### Port and Host Normalization Directives
- `port_in_redirect off;`: Prevents NGINX from appending the internal listening port (e.g., `8080` or `8443`) to the `Location` header when generating redirects behind upstream load balancers (AWS ALB, Cloudflare).
- `server_name_in_redirect off;`: Ensures NGINX uses the incoming client `Host` header in generated redirects rather than the internal `server_name` string.

---

## 3. How It Works Internally

### Canonical Redirection Pipeline in the Event Loop

```
Client Request: HTTP GET http://www.example.com/item?id=42
                        |
                        v
+--------------------------------------------------------+
| Server Selection Phase (server_name matching)          |
| Matches: server { listen 80; server_name www.example...|
+-----------------------+--------------------------------+
                        |
                        v
+--------------------------------------------------------+
| Rewrite Phase Execution                                |
| Directive: return 301 https://example.com$request_uri; |
+-----------------------+--------------------------------+
                        |
+-----------------------v--------------------------------+
| Immediate Response Generation                          |
| Status: HTTP/1.1 301 Moved Permanently                 |
| Header: Location: https://example.com/item?id=42       |
| Connection: keep-alive (zero backend I/O)              |
+--------------------------------------------------------+
```

### The Method Mutation Trap: 301 vs 308

```
Client POST /api/v1/orders ----------------------> NGINX Edge
Payload: {"item": "laptop", "qty": 1}
                                                         |
                                                [Rule: Redirect to /api/v2/orders]
                                                         |
If 301 Moved Permanently:                                | If 308 Permanent Redirect:
<--- HTTP 301 Location: /api/v2/orders ------------------| <--- HTTP 308 Location: /api/v2/orders
                                                         |
Client Browser Behavior:                                 | Client Browser Behavior:
Client converts POST to GET!                             | Client preserves POST method and body!
Sends: GET /api/v2/orders (Payload DROPPED!)             | Sends: POST /api/v2/orders {"item": "laptop"...}
Backend returns: 405 Method Not Allowed / Bug!           | Backend processes order successfully!
```

---

## 4. Syntax & Directives Reference

| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `return code URL` | `server`, `location`, `if` | None | Halts processing and emits immediate HTTP redirect (301, 302, 307, 308). |
| `port_in_redirect on\|off` | `http`, `server`, `location` | `on` | Controls whether NGINX appends the listening port to redirected URLs. |
| `server_name_in_redirect on\|off` | `http`, `server`, `location` | `off` | Uses first name from `server_name` in redirects instead of client `Host`. |
| `absolute_redirect on\|off` | `http`, `server`, `location` | `on` | If off, redirects issued by NGINX are relative (e.g., `Location: /new/`). |
| `rewrite regex replacement [flag]` | `server`, `location`, `if` | None | Rewrites URI; `redirect` emits 302; `permanent` emits 301. |

---

## 5. Practical Implementation Examples

### Example 1: Strict HTTP to HTTPS Apex Redirection

The gold-standard enterprise redirection architecture: separate, dedicated server blocks for port 80 and port 443 with query preservation.

```nginx
# /etc/nginx/conf.d/canonical_https.conf

# 1. Port 80 Catch-All Redirection
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name _;

    # Safe against port pollution behind load balancers
    port_in_redirect off;

    # 301 Permanent Redirect to HTTPS apex domain
    return 301 https://$host$request_uri;
}

# 2. Port 443 Production Virtual Host
server {
    listen 443 ssl default_server;
    listen [::]:443 ssl default_server;
    http2 on;
    server_name example.com;

    ssl_certificate     /etc/ssl/certs/example.com.crt;
    ssl_certificate_key /etc/ssl/private/example.com.key;

    location / {
        proxy_pass http://backend_cluster;
        proxy_set_header Host $host;
    }
}
```

### Example 2: Canonical www vs Non-www Domain Consolidation

Consolidates all secondary domains (`www.example.com`, `example.org`, `www.example.org`) into the single canonical apex domain `https://example.com`.

```nginx
# /etc/nginx/conf.d/domain_consolidation.conf

# Redirect secondary domains on HTTPS to canonical apex domain
server {
    listen 443 ssl;
    http2 on;
    server_name www.example.com example.org www.example.org;

    ssl_certificate     /etc/ssl/certs/multi_domain.crt;
    ssl_certificate_key /etc/ssl/private/multi_domain.key;

    # 301 Permanent redirect to canonical apex HTTPS
    return 301 https://example.com$request_uri;
}

# The One True Canonical Server Block
server {
    listen 443 ssl;
    http2 on;
    server_name example.com;

    ssl_certificate     /etc/ssl/certs/example.com.crt;
    ssl_certificate_key /etc/ssl/private/example.com.key;

    location / {
        root /var/www/canonical;
        index index.html;
    }
}
```

### Example 3: Trailing Slash Governance (Enforce vs Strip)

Demonstrates how to strictly enforce trailing slashes on directory-like paths, or conversely, how to strip unwanted trailing slashes from API paths.

```nginx
# /etc/nginx/conf.d/slash_governance.conf

server {
    listen 80;
    server_name blog.example.com;

    # POLICY A: Enforce Trailing Slash on Blog Categories
    # Redirects /categories/devops -> /categories/devops/
    rewrite ^/categories/([^/.]+)$ /categories/$1/ permanent;

    # POLICY B: Strip Trailing Slash on REST API Endpoints
    # Redirects /api/v1/users/ -> /api/v1/users (Preserving query arguments)
    rewrite ^/api/(.+)/$ /api/$1 permanent;

    location /categories/ {
        root /var/www/blog;
    }

    location /api/ {
        proxy_pass http://api_backend;
    }
}
```

### Example 4: REST API Redirections Preserving HTTP POST (308 Permanent)

Migrates a legacy API route to a new version while guaranteeing that clients sending `POST`, `PUT`, or `DELETE` requests do not have their methods converted to `GET` or payloads dropped.

```nginx
# /etc/nginx/conf.d/api_308_redirect.conf

server {
    listen 443 ssl;
    http2 on;
    server_name api.example.com;

    ssl_certificate     /etc/ssl/certs/api.crt;
    ssl_certificate_key /etc/ssl/private/api.key;

    # Legacy v1 route: Migrate to v2 using HTTP 308 Permanent Redirect!
    # 308 GUARANTEES the client sends the exact same POST/PUT body to /v2/
    location /v1/checkout {
        return 308 https://api.example.com/v2/checkout$is_args$args;
    }

    location /v2/checkout {
        proxy_pass http://checkout_backend;
        proxy_set_header Host $host;
    }
}
```

### Example 5: High-Performance Legacy URL Migration via `map`

Handling hundreds of arbitrary SEO URL redirections (e.g., following a company rebrand or CMS migration) without cluttering location blocks or sacrificing memory.

```nginx
# /etc/nginx/conf.d/url_migration_map.conf

# Map exact legacy paths to new canonical target URLs
map $request_uri $redirect_target {
    default                 "";
    "/about-us.php"         "/about";
    "/company/history.html" "/about";
    "/contact-support"      "/support";
    "/pricing-2024"         "/pricing";
    "/products/widget-pro"  "/store/widgets/pro";
}

server {
    listen 80;
    server_name site.example.com;

    # Check if requested URI matches any legacy redirection entry
    if ($redirect_target != "") {
        return 301 https://site.example.com$redirect_target;
    }

    location / {
        root /var/www/site;
        try_files $uri $uri/ /index.html;
    }
}
```

---

## 6. Production Best Practices

1. **Always Use Dedicated Server Blocks for Redirection**: Do not use `if ($scheme != "https")` or `if ($host = "www...")` inside a single HTTPS server block. Defining separate `server {}` blocks for port 80 and secondary domains is faster, cleaner, and immune to configuration errors.
2. **Use `$request_uri` Exclusively**: In redirection statements, always write `return 301 https://$host$request_uri;`. Never use `$uri` alone, which drops query parameters.
3. **Use 308 for APIs and Form Endpoints**: When redirecting API endpoints or payment submission routes, use `return 308` rather than `301`. Older 301 redirects cause browsers to convert `POST` requests to `GET`, breaking transactions.
4. **Always Set `port_in_redirect off;` Behind Reverse Proxies**: If NGINX sits behind an AWS Application Load Balancer or Cloudflare listening on internal port 8080, NGINX may generate `Location: https://example.com:8080/path/` unless `port_in_redirect off;` is configured.
5. **Beware of Aggressive Browser 301 Caching**: Web browsers cache HTTP 301 redirects indefinitely in their persistent disk caches. If you make a mistake in a 301 redirection rule, affected users' browsers will remain broken even after you fix the server configuration until they manually clear their browser caches. Use `302` or `307` during testing!
6. **Use `map` for Bulk URL Mappings**: For migrations with 50+ URLs, never create 50 separate `location` or `rewrite` statements. Use a `map` in `http {}`, which compiles into an $O(1)$ hash table.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Using `return 301 https://$host$uri;`** | Drops all query strings! A request to `/search?q=nginx` is redirected to `/search`, losing user query arguments. | Use `$request_uri`: `return 301 https://$host$request_uri;`. |
| **Using `if ($scheme = http)` inside HTTPS block** | "If is Evil" inside server blocks creates subtle request handling bugs and adds CPU branching overhead. | Declare a separate `server { listen 80; ... }` block that performs the redirect unconditionally. |
| **Using 301 for API `POST` endpoints** | Client browsers and HTTP libraries mutate `POST` to `GET`, dropping JSON bodies and failing with HTTP 405 Method Not Allowed. | Use modern `return 308` to enforce method and body preservation. |
| **Port leakage (`Location: https://example.com:8080/`)** | NGINX running on non-standard internal port behind load balancer appends its internal port to redirects. | Add `port_in_redirect off;` to the `http` or `server` context. |
| **Infinite Trailing Slash Loops** | A rewrite rule forces `/path` to `/path/`, but another rule forces `/path/` to `/path`, creating an infinite redirection loop (HTTP 310 / ERR_TOO_MANY_REDIRECTS). | Choose ONE trailing slash convention site-wide and ensure rules do not overlap. |
| **Testing with 301 in development** | Developers hard-cache broken redirects in their local browser caches that persist across reboots. | Test redirection logic using `return 302` or `return 307` first; promote to 301/308 only after validation. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test HTTP to HTTPS Redirection and Inspect Location Header
curl -s -I http://example.com/test?param=value | grep -E -i "http/|location"

# Expected:
# HTTP/1.1 301 Moved Permanently
# Location: https://example.com/test?param=value

# 2. Test Modern 308 Redirection with POST Payload Preservation
curl -s -i -X POST https://api.example.com/v1/orders \
  -H "Content-Type: application/json" \
  -d '{"product_id": 99}'

# Expected:
# HTTP/1.1 308 Permanent Redirect
# Location: https://api.example.com/v2/orders

# 3. Follow Redirects Automatically and Print Intermediate Status Codes
curl -s -L -v -o /dev/null http://www.example.com/item

# 4. Verify That Internal Ports are NOT Leaked in Location Header
curl -s -I -H "Host: mydomain.com" http://127.0.0.1:8080/folder | grep -i location
# Must show: Location: http://mydomain.com/folder/ (NO :8080 present)
```

---

## 9. Architectural Comparison Table

| Status Code | Standard Name | Method Preservation | Typical Use Case | Browser Cache Behavior |
| :--- | :--- | :--- | :--- | :--- |
| **`301`** | Moved Permanently | **NO** (Often mutates to GET) | Domain migrations, HTTP to HTTPS | Cached indefinitely |
| **`302`** | Found (Temporary) | **NO** (Often mutates to GET) | Temporary landing pages, login gates | Not cached |
| **`307`** | Temporary Redirect | **YES** (Strictly preserves method & body) | Temporary API maintenance, payment gateways | Not cached |
| **`308`** | Permanent Redirect | **YES** (Strictly preserves method & body) | Permanent REST/GraphQL API migrations | Cached indefinitely |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: What is the critical technical difference between HTTP 301 Moved Permanently and HTTP 308 Permanent Redirect?</strong></summary>

**Answer:**
Under the original HTTP/1.0 and HTTP/1.1 specifications (RFC 1945 and RFC 2616), when a client received a `301 Moved Permanently` response to a `POST` request, web browsers historically rewrote the request method to `GET` for the redirected request, discarding the original request body. Although this violated the original specification, it became universal de facto browser behavior.

RFC 7538 introduced **`308 Permanent Redirect`** to resolve this ambiguity. A 308 redirect explicitly forbids the client from changing the HTTP method: a `POST` request must remain a `POST`, and a `PUT` must remain a `PUT`, with the exact original request payload body retransmitted to the new target URL. For modern REST APIs, 308 is the correct permanent redirection status code.
</details>

<details>
<summary><strong>Q2: Why should you always use $request_uri instead of $uri when redirecting in NGINX?</strong></summary>

**Answer:**
- `$request_uri`: Represents the original, raw, unparsed request URI sent directly by the client, including the path and any query string arguments (e.g., `/catalog/shoes?brand=nike&size=10`). It is immutable.
- `$uri`: Represents the normalized path only (e.g., `/catalog/shoes`), with percent-decoding applied and query strings completely removed.

If you write `return 301 https://example.com$uri;`, any request containing query parameters will have those parameters permanently stripped upon redirection, breaking user searches, tracking tags, and API queries. Using `return 301 https://example.com$request_uri;` guarantees all query parameters are preserved.
</details>

<details>
<summary><strong>Q3: Why is it considered an anti-pattern to handle HTTP to HTTPS redirection using if ($scheme != "https")?</strong></summary>

**Answer:**
Using `if ($scheme != "https")` inside an HTTPS `server {}` block is an anti-pattern for three reasons:
1. **Performance**: Evaluating `if` on every request invokes the rewrite phase condition parser needlessly.
2. **"If is Evil" Risks**: Using `if` inside server blocks can create unexpected variable inheritance bugs and configuration drift.
3. **Architectural Separation**: The clean, standard NGINX pattern is to define a dedicated `server { listen 80; ... }` block that immediately executes `return 301 https://$host$request_uri;`. This guarantees port 80 traffic is redirected immediately without loading SSL contexts or parsing TLS parameters.
</details>

<details>
<summary><strong>Q4: What causes NGINX to leak internal ports like :8080 in the Location header, and how is it prevented?</strong></summary>

**Answer:**
When NGINX is deployed behind a reverse proxy, SSL terminator, or cloud load balancer (such as an AWS ALB or Cloudflare), the load balancer terminates public client connections on port 80 or 443 and forwards traffic to NGINX on an internal private port (e.g., `8080` or `8443`).

When NGINX generates an automatic redirect (for example, adding a trailing slash to a directory), its default directive `port_in_redirect on;` appends the internal socket port to the generated `Location` header, returning `Location: http://example.com:8080/dir/`. Because port 8080 is internal and blocked by firewalls, the client's browser hangs. 

Setting `port_in_redirect off;` forces NGINX to emit the redirect without appending the internal port number.
</details>

<details>
<summary><strong>Q5: What danger is associated with testing HTTP 301 redirects in a development environment?</strong></summary>

**Answer:**
Modern web browsers treat `301 Moved Permanently` as an immutable, permanent instruction. Browsers cache 301 redirects in a dedicated local persistent cache that is decoupled from standard HTTP cache-control headers. 

If a developer accidentally misconfigures a 301 redirect during development or testing, their local browser will continue redirecting to the erroneous target indefinitely—even after the NGINX configuration is corrected or NGINX is completely shut down. The developer must manually clear browser history/cache or test exclusively in Incognito/Private windows. For this reason, always test redirection rules using `302 Found` or `307 Temporary Redirect` before switching to 301 or 308.
</details>

<details>
<summary><strong>Q6: How do you handle bulk migrations of hundreds of legacy URLs without degrading NGINX performance?</strong></summary>

**Answer:**
Do not write hundreds of sequential `location` blocks or `rewrite` directives, as NGINX would have to evaluate them sequentially or build a massive regex tree, consuming CPU.

Instead, define a `map` block in the `http {}` context:
```nginx
map $request_uri $target_url {
    default "";
    include /etc/nginx/legacy_redirects.map;
}
```
NGINX compiles the entries in the map into an optimized static hash table at configuration reload. At runtime, looking up a redirect target requires an $O(1)$ constant-time hash lookup with zero measurable latency.
</details>

<details>
<summary><strong>Q7: What is absolute_redirect, and when would you set it to off?</strong></summary>

**Answer:**
By default, `absolute_redirect on;` instructs NGINX to generate fully qualified absolute URLs in the `Location` header (e.g., `Location: http://example.com/new-path/`).

Setting `absolute_redirect off;` instructs NGINX to emit relative redirects (e.g., `Location: /new-path/`). This is particularly useful in complex multi-tier reverse proxy architectures or when NGINX is running in microservice containers behind diverse API gateways, as relative redirects prevent NGINX from having to know the public scheme or domain name of the edge gateway.
</details>

<details>
<summary><strong>Q8: How does trailing slash normalization affect SEO?</strong></summary>

**Answer:**
Search engines view `https://example.com/page` and `https://example.com/page/` as two distinct, unique URLs. If both URLs return the exact same content with an HTTP 200 OK status code, search engines view this as **duplicate content**, splitting inbound backlink authority and potentially reducing rankings. Standardizing on either trailing slash or non-trailing slash and issuing 301 permanent redirects for the alternative ensures all search equity consolidates onto a single canonical URL.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Universal Port 80 to HTTPS Redirector</strong></summary>

**Goal:** Configure a dedicated default server block on port 80 that redirects all incoming traffic to HTTPS while preserving hostnames and query arguments.

**Solution:**
```nginx
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name _;

    port_in_redirect off;
    return 301 https://$host$request_uri;
}
```
Test with:
```bash
curl -I "http://localhost/test?category=tech&page=1"
# Verify Location: https://localhost/test?category=tech&page=1
```
</details>

<details>
<summary><strong>Exercise 2: Consolidate www to Apex Domain</strong></summary>

**Goal:** Permanently redirect all traffic arriving at `www.company.local` to `company.local`.

**Solution:**
```nginx
server {
    listen 80;
    server_name www.company.local;

    return 301 http://company.local$request_uri;
}

server {
    listen 80;
    server_name company.local;

    location / {
        return 200 "Welcome to canonical company.local!\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Configure an API Migration with 308 Permanent Redirect</strong></summary>

**Goal:** Redirect all incoming requests from `/v1/users` to `/v2/users`, ensuring that `POST` requests maintain their method and JSON payloads.

**Solution:**
```nginx
server {
    listen 80;
    server_name api.local;

    location /v1/users {
        return 308 /v2/users$is_args$args;
    }

    location /v2/users {
        proxy_pass http://127.0.0.1:4000;
    }
}
```
Test with:
```bash
curl -i -X POST http://api.local/v1/users -d '{"name": "Alice"}'
# Verify HTTP/1.1 308 Permanent Redirect
```
</details>

<details>
<summary><strong>Exercise 4: Enforce Trailing Slash on Documentation Paths</strong></summary>

**Goal:** Redirect requests like `/docs/getting-started` to `/docs/getting-started/`.

**Solution:**
```nginx
server {
    listen 80;
    server_name docs.local;

    # Match paths under /docs/ that don't end in a slash or file extension
    rewrite ^/docs/([^/.]+)$ /docs/$1/ permanent;

    location /docs/ {
        return 200 "Docs directory index served.\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 5: Prevent Port Leaks Behind Reverse Proxy</strong></summary>

**Goal:** Configure NGINX listening on port 8080 behind an upstream gateway so it never exposes port 8080 in redirects.

**Solution:**
```nginx
server {
    listen 8080;
    server_name portal.local;

    port_in_redirect off;
    server_name_in_redirect off;

    location /admin {
        # Without port_in_redirect off, NGINX redirects to portal.local:8080/admin/
        return 301 /admin/;
    }

    location /admin/ {
        return 200 "Admin portal\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 6: Build a Hash-Mapped Redirection Table</strong></summary>

**Goal:** Use the `map` directive to redirect 3 legacy URLs to new canonical paths with an $O(1)$ lookup table.

**Solution:**
```nginx
map $request_uri $target_path {
    default           "";
    "/old-home"       "/";
    "/products.php"   "/store";
    "/help-center"    "/support";
}

server {
    listen 80;
    server_name store.local;

    if ($target_path != "") {
        return 301 $target_path;
    }

    location / {
        return 200 "Store home\n";
    }
}
```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production Canonical Redirection Cheat Sheet

# 1. HTTP to HTTPS Apex Redirection
server {
    listen 80 default_server;
    server_name _;
    port_in_redirect off;
    return 301 https://$host$request_uri;
}

# 2. Apex Domain Consolidation (www -> non-www)
server {
    listen 443 ssl;
    server_name www.example.com;
    return 301 https://example.com$request_uri;
}

# 3. Modern REST API Redirection (Method & Body Preserved)
location /api/v1/ {
    return 308 https://api.example.com/api/v2/$is_args$args;
}

# 4. Trailing Slash Enforcement
rewrite ^/section/([^/.]+)$ /section/$1/ permanent;

# 5. Reverse Proxy Port Protection
port_in_redirect off;
server_name_in_redirect off;
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_rewrite_module Documentation](https://nginx.org/en/docs/http/ngx_http_rewrite_module.html)
- [Official NGINX ngx_http_core_module (port_in_redirect)](https://nginx.org/en/docs/http/ngx_http_core_module.html#port_in_redirect)
- [RFC 9110: HTTP Semantics - Redirection 3xx](https://datatracker.ietf.org/doc/html/rfc9110#section-15.4)
- [RFC 7538: The Hypertext Transfer Protocol Status Code 308 (Permanent Redirect)](https://datatracker.ietf.org/doc/html/rfc7538)
- [Google Search Central: Canonicalization Best Practices](https://developers.google.com/search/docs/crawling-indexing/canonicalization)

---

[← Previous: Error Handling & Custom Pages](09-error-handling-and-custom-pages.md) | [Module Overview](README.md) | [Next: ACME and Certbot Automation →](11-acme-and-certbot-automation.md)
