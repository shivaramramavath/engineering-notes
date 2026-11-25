---
title: "Error Handling and Custom Error Pages in NGINX"
description: "Designing resilient error handling in NGINX: custom error pages, JSON API error formatting, proxy_intercept_errors, internal error locations, and graceful degradation."
module: "02-core-features"
track: "devops-nginx"
order: 9
difficulty: "Intermediate"
prerequisites:
  - "01-fundamentals/03-server-and-location"
  - "02-core-features/02-reverse-proxy"
tags:
  - nginx
  - error-page
  - proxy-intercept-errors
  - json-errors
  - internal-directive
  - maintenance-page
  - reliability
last_updated: "2026-03-30"
---

# Error Handling and Custom Error Pages in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Branded Static HTML Error Pages with Internal Protection](#example-1-branded-static-html-error-pages-with-internal-protection)
  - [Example 2: Upstream Error Interception (`proxy_intercept_errors`)](#example-2-upstream-error-interception-proxy_intercept_errors)
  - [Example 3: Dynamic Content Negotiation (JSON for APIs, HTML for Browsers)](#example-3-dynamic-content-negotiation-json-for-apis-html-for-browsers)
  - [Example 4: Automated Maintenance Mode with 503 Service Unavailable](#example-4-automated-maintenance-mode-with-503-service-unavailable)
  - [Example 5: Upstream Fallback with Named Locations (`@fallback`)](#example-5-upstream-fallback-with-named-locations-fallback)
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

In production web operations, failures are inevitable: backend microservices crash, databases exhaust connection pools, networks experience transient timeouts, and users request non-existent paths. Default NGINX error responses (the plain white "502 Bad Gateway" or "404 Not Found" banner displaying NGINX version details) present multiple problems:
1. **User Experience Degradation**: Raw server errors break user trust and offer no navigation back to working application flows.
2. **Information Disclosure**: Default error banners expose server signatures, software versions, and internal infrastructure topology to malicious scanners.
3. **API Contract Inconsistency**: When an upstream REST or GraphQL microservice fails (502/504), returning a raw HTML error page crashes client-side mobile apps and single-page applications expecting strict JSON payloads.

Configuring comprehensive error handling in NGINX enables centralized error interception, branded fallback pages, structured JSON error payloads, and seamless automated failover.

---

## 2. Core Concepts

### The `error_page` Directive
The `error_page` directive associates one or more HTTP error status codes (e.g., 404, 500, 502, 503, 504) with an internal URI or named location:
```nginx
error_page 500 502 503 504 /50x.html;
```
When an error occurs, NGINX halts standard response processing and initiates an internal redirect to the designated URI.

### Intercepting Upstream Errors (`proxy_intercept_errors`)
By default, if an upstream server behind `proxy_pass` returns an HTTP error code (such as a 500 from a Python crash or a 404 from Node.js), NGINX **passes that error response directly to the client**. NGINX does *not* invoke its own `error_page` handlers.
To force NGINX to intercept error status codes ($\ge 300$) returned by upstream backends and substitute NGINX's own custom error handlers, you must configure:
```nginx
proxy_intercept_errors on;
```

### The `internal;` Directive
When creating dedicated error page locations (such as `/custom_404.html` or `/errors/json_500`), you must ensure external clients cannot access these pages directly via standard GET requests. The `internal;` directive restricts a location block so that it can **only be entered via internal NGINX transitions**:
- Triggered by `error_page`
- Triggered by `rewrite ... last;`
- Triggered by `try_files`
- Triggered by upstream `X-Accel-Redirect` headers

Direct client requests to an `internal;` location receive an immediate `404 Not Found`.

### Status Code Rewriting
NGINX allows altering the response status code during an error redirect using the syntax `=response`:
```nginx
# Map 404 to custom URI but return HTTP 200 OK
error_page 404 =200 /index.html;

# Preserve original error status code while serving custom body
error_page 502 503 504 = /maintenance.html;
```

---

## 3. How It Works Internally

### Upstream Error Interception State Machine

```
Client Request (GET /api/orders/99)
       |
       v
[NGINX Reverse Proxy]
       |
       |--- Forward HTTP Request ------------------> [Upstream Backend Node]
       |                                                    |
       |                                             (Crashes / OOM)
       |<-- Returns HTTP 500 Internal Server Error ---------|
       |
+------v---------------------------------------------+
| Is proxy_intercept_errors ON?                      |
+----------------------+-----------------------------+
       |                                     |
      YES                                    NO
       |                                     |
+------v-----------------------+     +-------v-----------------------+
| Check error_page 500 config  |     | Stream upstream 500 response  |
| Match found: /error_500.json |     | directly back to client       |
| Perform INTERNAL REDIRECT    |     +-------------------------------+
+--------------+---------------+
               |
               v
+-------------------------------+
| Location /error_500.json      |
| Serve structured JSON payload |
| Status: 500                   |
+-------------------------------+
```

---

## 4. Syntax & Directives Reference

| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `error_page code ... [=[response]] uri` | `http`, `server`, `location`, `if in location` | None | Defines URI or named location to serve for specified HTTP status codes. |
| `proxy_intercept_errors on\|off` | `http`, `server`, `location` | `off` | When on, upstream responses with codes $\ge 300$ trigger NGINX `error_page`. |
| `fastcgi_intercept_errors on\|off` | `http`, `server`, `location` | `off` | Intercepts error codes $\ge 300$ from FastCGI (PHP-FPM) backends. |
| `recursive_error_pages on\|off` | `http`, `server`, `location` | `off` | Enables multiple redirects through `error_page` if an error occurs *inside* an error handler. |
| `internal` | `location` | None | Specifies that the given location can only be used for internal requests. |

---

## 5. Practical Implementation Examples

### Example 1: Branded Static HTML Error Pages with Internal Protection

Production configuration serving customized static HTML pages for 404 and 50x errors while preventing direct client access to error files.

```nginx
# /etc/nginx/conf.d/custom_pages.conf

server {
    listen 80;
    server_name www.example.com;
    root /var/www/html;

    # Map error status codes to internal error URIs
    error_page 404             /errors/404.html;
    error_page 500 502 503 504 /errors/50x.html;

    location / {
        try_files $uri $uri/ =404;
    }

    # Protected internal error directory
    location ^~ /errors/ {
        root /var/www/static;
        internal; # Blocks direct external requests like 'GET /errors/404.html'
        
        # Ensure security headers are maintained on error pages
        add_header X-Frame-Options "DENY" always;
        add_header X-Content-Type-Options "nosniff" always;
    }
}
```

### Example 2: Upstream Error Interception (`proxy_intercept_errors`)

Intercepts backend microservice crashes and converts them into branded gateway error representations.

```nginx
# /etc/nginx/conf.d/intercept_proxy.conf

upstream payment_service {
    server 127.0.0.1:8080 max_fails=2 fail_timeout=5s;
}

server {
    listen 443 ssl;
    server_name checkout.example.com;

    ssl_certificate     /etc/ssl/certs/checkout.crt;
    ssl_certificate_key /etc/ssl/private/checkout.key;

    # Intercept upstream failures
    proxy_intercept_errors on;
    error_page 500 502 503 504 /gateway_error.html;

    location / {
        proxy_pass http://payment_service;
        proxy_set_header Host $host;
        proxy_connect_timeout 2s;
        proxy_read_timeout 5s;
    }

    location = /gateway_error.html {
        root /var/www/error_pages;
        internal;
    }
}
```

### Example 3: Dynamic Content Negotiation (JSON for APIs, HTML for Browsers)

Routes error responses dynamically: returns structured JSON if the client requested `application/json`, or branded HTML if a web browser requested `text/html`.

```nginx
# /etc/nginx/conf.d/content_negotiated_errors.conf

# Map Accept header to error payload type
map $http_accept $error_format {
    default                  "html";
    "~*application/json"     "json";
}

server {
    listen 80;
    server_name api-and-web.example.com;

    proxy_intercept_errors on;
    error_page 404 /handle_404;
    error_page 500 502 503 504 /handle_50x;

    location / {
        proxy_pass http://backend_pool;
        proxy_set_header Host $host;
    }

    # 404 Content Negotiation Router
    location = /handle_404 {
        internal;
        if ($error_format = "json") {
            return 404 '{"error": {"code": 404, "message": "Resource Not Found"}}\n';
        }
        rewrite ^ /errors/404.html break;
        root /var/www/static;
    }

    # 50x Content Negotiation Router
    location = /handle_50x {
        internal;
        default_type application/json;
        if ($error_format = "json") {
            return 500 '{"error": {"code": 500, "message": "Internal Gateway Failure"}}\n';
        }
        rewrite ^ /errors/50x.html break;
        root /var/www/static;
    }
}
```

### Example 4: Automated Maintenance Mode with 503 Service Unavailable

Triggering site-wide maintenance mode by creating a simple touch-file on disk, returning an RFC-compliant HTTP 503 with a `Retry-After` header.

```nginx
# /etc/nginx/conf.d/maintenance_mode.conf

server {
    listen 80;
    server_name app.example.com;
    root /var/www/app;

    # Define custom 503 page
    error_page 503 @maintenance;

    # Check if maintenance flag file exists on disk
    if (-f /var/www/maintenance.flag) {
        return 503;
    }

    location / {
        proxy_pass http://app_upstream;
    }

    location @maintenance {
        root /var/www/static;
        # Instruct search engines to check back in 1 hour (3600 seconds)
        add_header Retry-After 3600 always;
        add_header Cache-Control "no-store, no-cache, must-revalidate" always;
        rewrite ^(.*)$ /maintenance.html break;
    }
}
```
*To enable maintenance:* `touch /var/www/maintenance.flag`  
*To disable maintenance:* `rm -f /var/www/maintenance.flag`

### Example 5: Upstream Fallback with Named Locations (`@fallback`)

If the primary upstream microservice returns a 500, 502, or 504 error, gracefully fall back to a backup static replica or read-only cache.

```nginx
# /etc/nginx/conf.d/upstream_fallback.conf

upstream primary_api {
    server 10.0.1.10:8000;
}

upstream backup_readonly_api {
    server 10.0.1.20:8000;
}

server {
    listen 80;
    server_name catalog.example.com;

    location / {
        proxy_pass http://primary_api;
        proxy_set_header Host $host;

        # Intercept server errors and reroute to named fallback location
        proxy_intercept_errors on;
        error_page 500 502 503 504 = @fallback_service;
    }

    location @fallback_service {
        # Transparently forward request to read-only backup cluster
        proxy_pass http://backup_readonly_api;
        proxy_set_header Host $host;
        proxy_set_header X-Served-From-Fallback "true";

        add_header X-Gateway-Fallback "active" always;
    }
}
```

---

## 6. Production Best Practices

1. **Always Protect Error Locations with `internal;`**: Never leave error page endpoints publicly callable. Without `internal;`, users or vulnerability scanners can directly browse to `/500.html` or `/errors/json_500`, inflating error metrics.
2. **Always Enable `proxy_intercept_errors on;` for Unified Gateways**: If NGINX is intended to provide a uniform corporate branding or strict JSON error structure, `proxy_intercept_errors on;` must be declared.
3. **Always Include `Retry-After` on 503 Responses**: When taking a site offline for planned maintenance, returning a `503 Service Unavailable` with `Retry-After: 3600` instructs search engine crawlers (Googlebot) not to de-index the site from search results.
4. **Never Rewrite Errors to 200 OK (`=200`) on REST APIs**: Rewriting 404 or 500 errors to 200 OK (the "Soft 404" anti-pattern) breaks client-side error handling, causes indexing issues for SEO, and confuses monitoring tools.
5. **Keep Error Page Assets Self-Contained**: If `/50x.html` references external CSS (`/css/style.css`) or images (`/images/logo.png`) that reside on the crashed upstream server, the error page will render as broken unstyled HTML. Inline all CSS, SVG graphics, and scripts directly inside the error HTML file.
6. **Use Inline Base64 Images in Error Templates**: Embed branding images directly as Base64 `data:image/svg+xml;base64,...` so the error page requires zero secondary HTTP requests.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Omitting `proxy_intercept_errors on;`** | Upstream application crashes (e.g., Python tracebacks or Express 500 pages) leak directly to end users, bypassing NGINX `error_page`. | Set `proxy_intercept_errors on;` in `server` or `location`. |
| **External CSS/Images in `50x.html`** | If upstream is down, requests for `/assets/style.css` also fail, rendering a broken, unstyled error page. | Inline all CSS styles and SVG graphics directly within the error HTML document. |
| **Omitting `internal;` on error locations** | Clients can directly request `https://example.com/50x.html`, returning HTTP 200 with error graphics and confusing uptime monitors. | Mark the error location as `internal;`. |
| **"Soft 404" (`error_page 404 =200 /index.html;`)** | Returns HTTP 200 OK for missing resources. Search engines index duplicate pages and client API libraries fail to catch missing entities. | Return true HTTP 404 status codes. For Single Page Applications, use `try_files $uri $uri/ /index.html;`. |
| **Infinite Error Redirection Loops** | If the `/50x.html` location itself encounters a 404 or 500 error and `recursive_error_pages` is on, NGINX loops until dropping the socket. | Test that the filesystem path in the error location exists and is readable by the `nginx` worker user. |
| **Omitting `always` on headers inside error blocks** | Security headers configured on parent blocks are omitted from custom error pages. | Re-declare security headers with `always` inside error locations. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test 404 Error Page and Verify Status Code
curl -s -I http://example.com/non-existent-page-test
# Verify: HTTP/1.1 404 Not Found

# 2. Test Upstream Error Interception (Simulate 502 Bad Gateway)
# Stop backend service, then query NGINX:
curl -s -i http://example.com/api/
# Verify NGINX returns custom 502 page and not default raw banner

# 3. Verify Internal Protection of Error Locations (Must return 404)
curl -s -i http://example.com/errors/50x.html
# Verify: HTTP/1.1 404 Not Found (blocked by internal directive)

# 4. Test Content-Negotiated Error Response (JSON vs HTML)
# Request JSON:
curl -s -H "Accept: application/json" http://example.com/invalid-path
# Expected: {"error": {"code": 404, "message": "Resource Not Found"}}

# Request HTML:
curl -s -H "Accept: text/html" http://example.com/invalid-path
# Expected: <!DOCTYPE html><html>... branded page ...</html>
```

### Resource Sizing for Error Handlers
Custom error pages should be lightweight. Inlining a clean SVG logo and modern CSS typography typically requires less than 4KB total page size. Because 4KB fits entirely within a single TCP initial congestion window (10 MSS packets $\approx$ 14KB), NGINX delivers the entire error page in a single network round-trip.

---

## 9. Architectural Comparison Table

| Mechanism | `error_page` (URI) | `error_page` (`@named`) | Upstream Direct Error |
| :--- | :--- | :--- | :--- |
| **Context** | `http`, `server`, `location` | `http`, `server`, `location` | Upstream application |
| **Execution Path** | Internal URI redirect | Direct execution of named block | Forwarded through proxy |
| **Content Handler** | Static file or script | Any NGINX module (`proxy_pass`, `return`) | Upstream runtime |
| **Client Visibility** | Transparent (original URL preserved) | Transparent | Transparent |
| **Overhead** | Minimal (single disk read) | Low (internal re-dispatch) | Consumes upstream bandwidth |
| **Best For** | Static branded HTML pages | Dynamic fallback clusters / JSON | Microservice-specific validation errors |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: Why is proxy_intercept_errors off by default in NGINX?</strong></summary>

**Answer:**
By default, NGINX is designed to act as a transparent proxy. In many modern web applications, the backend server (such as a Spring Boot, Express, or Django microservice) generates rich, context-specific error payloads (e.g., JSON validation errors with field-level details, or custom application error pages). 

If `proxy_intercept_errors` were enabled by default, NGINX would blindly intercept every 4xx and 5xx response emitted by the backend and overwrite the detailed application error message with a generic NGINX error page. Therefore, it defaults to `off` so engineers can explicitly opt-in when they want NGINX to standardize error handling at the edge.
</details>

<details>
<summary><strong>Q2: What is the exact function of the internal directive in an NGINX location block?</strong></summary>

**Answer:**
The `internal;` directive specifies that a `location` block cannot be accessed directly by an external client HTTP request. If an external client attempts to connect directly to the URI (e.g., `GET /internal_error.html`), NGINX immediately aborts processing and returns `404 Not Found`.

The location can only be accessed through internal NGINX operations:
1. Internal redirects generated by the `error_page` directive.
2. Internal rewrites executed via `rewrite ... last;`.
3. Subrequests created by the `auth_request` or `addition` modules.
4. Upstream backend internal redirects via the `X-Accel-Redirect` response header.
</details>

<details>
<summary><strong>Q3: What is the "Soft 404" problem, and why should error_page 404 =200 /index.html be avoided?</strong></summary>

**Answer:**
A "Soft 404" occurs when a web server serves an error message or generic home page when a requested URL does not exist, but returns an HTTP status code of `200 OK` instead of `404 Not Found`.

Using `error_page 404 =200 /index.html`:
1. Destroys search engine optimization (SEO): Crawlers (Googlebot) assume the URL is a distinct, valid page, indexing thousands of garbage or non-existent URLs and wasting site crawl budget.
2. Breaks automated API clients and monitoring scripts that rely on HTTP status codes to detect broken links or missing resources.
3. Masks application bugs from site reliability engineers.
For Single Page Applications (SPAs), `try_files $uri $uri/ /index.html;` should be used instead of rewriting 404s.
</details>

<details>
<summary><strong>Q4: How do you configure NGINX to return JSON error responses for API clients while maintaining HTML error pages for desktop browsers?</strong></summary>

**Answer:**
You achieve this through content negotiation using the `map` directive to inspect the client's `Accept` header:
```nginx
map $http_accept $error_type {
    default                 "html";
    "~*application/json"    "json";
}

server {
    error_page 502 /handle_502;
    proxy_intercept_errors on;

    location = /handle_502 {
        internal;
        if ($error_type = "json") {
            default_type application/json;
            return 502 '{"error": {"code": 502, "message": "Bad Gateway"}}\n';
        }
        rewrite ^ /502.html break;
        root /var/www/static;
    }
}
```
</details>

<details>
<summary><strong>Q5: What happens if an error occurs while NGINX is serving an error_page?</strong></summary>

**Answer:**
If NGINX encounters an error while processing an `error_page` (for example, the file configured in `error_page 500 /500.html` does not exist on disk, triggering a 404):
- By default (`recursive_error_pages off;`), NGINX falls back to its built-in hardcoded plain-text error banner to prevent recursive loops.
- If `recursive_error_pages on;` is explicitly set, NGINX will look for a configured `error_page 404` directive and attempt to handle the secondary error. If that also fails or misconfigures, NGINX eventually drops the connection to break the loop.
</details>

<details>
<summary><strong>Q6: Why should error page templates have self-contained inlined CSS and SVG assets?</strong></summary>

**Answer:**
When an NGINX edge gateway returns a `502 Bad Gateway`, `503 Service Unavailable`, or `504 Gateway Timeout`, it is almost always because the upstream application servers or backend infrastructure have crashed or become unreachable.

If the custom `50x.html` page links to external assets (e.g., `<link rel="stylesheet" href="/assets/style.css">` or `<img src="/images/logo.png">`), the browser will attempt to fetch those secondary assets from the same failed origin. Because the backend is dead, those asset requests will also fail (returning 502s), resulting in a broken, flickering page layout. Inlining all CSS rules and SVG graphics guarantees the error page renders perfectly in a single HTTP payload.
</details>

<details>
<summary><strong>Q7: How does a named location (@fallback) differ from a standard URI redirect in error_page?</strong></summary>

**Answer:**
When using a standard URI redirect (`error_page 502 /502.html;`), NGINX modifies the request's internal URI pointer to `/502.html` and restarts the location matching phase.

When using a named location (`error_page 502 = @backup;`), NGINX transfers execution directly to `location @backup { ... }` **without altering the original request URI or query arguments**. This allows the named location to inspect the original request parameters, forward them transparently to an alternate fallback upstream, or execute conditional logging.
</details>

<details>
<summary><strong>Q8: What is the purpose of the Retry-After header during scheduled maintenance?</strong></summary>

**Answer:**
The `Retry-After` header (defined in RFC 7231 / RFC 9110) informs clients and search engine web crawlers how long they should wait before making another request to the service. When returning an HTTP `503 Service Unavailable` during planned maintenance:
```nginx
add_header Retry-After 3600 always; # 1 hour
```
Search engines recognize this as temporary maintenance: they pause crawling without penalizing search rankings or removing pages from their index, and automatically return to re-index the site after the specified interval.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Branded 404 Error Page</strong></summary>

**Goal:** Configure NGINX to serve a custom HTML 404 page from `/var/www/errors/404.html` and verify that direct client access is blocked by `internal`.

**Solution:**
```bash
# 1. Create directory and error page
mkdir -p /var/www/errors
cat << 'EOF' > /var/www/errors/404.html
<!DOCTYPE html>
<html>
<head><title>404 - Page Not Found</title><style>body{font-family:sans-serif;text-align:center;padding:50px;}</style></head>
<body><h1>404</h1><p>The page you requested could not be found.</p></body>
</html>
EOF
```

```nginx
server {
    listen 80;
    server_name test.local;
    root /var/www/html;

    error_page 404 /errors/404.html;

    location / {
        try_files $uri $uri/ =404;
    }

    location ^~ /errors/ {
        root /var/www;
        internal;
    }
}
```
Test:
- Request missing page: `curl -I http://test.local/missing` -> `HTTP/1.1 404 Not Found` with custom HTML.
- Direct request: `curl -I http://test.local/errors/404.html` -> `HTTP/1.1 404 Not Found`.
</details>

<details>
<summary><strong>Exercise 2: Intercept 502 Bad Gateway from a Down Upstream</strong></summary>

**Goal:** Configure `proxy_pass` to an unused local port (`8089`), enable error interception, and serve a custom 502 error message.

**Solution:**
```nginx
server {
    listen 80;
    server_name proxy-error.local;

    proxy_intercept_errors on;
    error_page 502 /502.html;

    location / {
        proxy_pass http://127.0.0.1:8089;
    }

    location = /502.html {
        internal;
        return 502 "Service temporarily unavailable. Please retry shortly.\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Build a Dynamic JSON Error Handler</strong></summary>

**Goal:** Return a structured JSON response `{"error": "not_found", "status": 404}` for missing API routes under `/api/`.

**Solution:**
```nginx
server {
    listen 80;
    server_name api.local;

    location /api/ {
        error_page 404 @json_404;
        try_files $uri =404;
    }

    location @json_404 {
        default_type application/json;
        return 404 '{"error": "not_found", "status": 404, "path": "$uri"}\n';
    }
}
```
</details>

<details>
<summary><strong>Exercise 4: Implement Instant Flag-Based Maintenance Mode</strong></summary>

**Goal:** Configure an endpoint that returns HTTP 503 with a `Retry-After: 300` header when a file `/tmp/offline` is detected.

**Solution:**
```nginx
server {
    listen 80;
    server_name store.local;

    error_page 503 @maint;

    if (-f /tmp/offline) {
        return 503;
    }

    location / {
        return 200 "Store is open for business!\n";
    }

    location @maint {
        add_header Retry-After 300 always;
        return 503 "Store is down for quick scheduled upgrades. Check back in 5 minutes.\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 5: Reroute Upstream Errors to a Read-Only Fallback Cluster</strong></summary>

**Goal:** If the primary upstream cluster fails with 500 or 504, forward the request to an alternate mock server using `@fallback`.

**Solution:**
```nginx
upstream primary {
    server 127.0.0.1:9001; # Dead port
}

upstream backup_mock {
    server 127.0.0.1:9002;
}

server {
    listen 80;
    server_name fallback.local;

    location / {
        proxy_pass http://primary;
        proxy_intercept_errors on;
        error_page 502 504 = @backup;
    }

    location @backup {
        proxy_pass http://backup_mock;
        add_header X-Served-By "Backup-Service" always;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6: Test Recursive Error Handling</strong></summary>

**Goal:** Verify NGINX behavior when the custom error page file itself is deleted or permissions are removed.

**Solution:**
1. Configure `error_page 404 /missing_error_page.html;` without creating the file.
2. Request a non-existent page:
   ```bash
   curl -i http://localhost/any-random-url
   ```
3. Observe that NGINX safely falls back to its built-in hardcoded plain HTML banner rather than crashing or entering an infinite loop.
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production Error Handling Cheat Sheet

# 1. Standard Error Pages
error_page 404             /errors/404.html;
error_page 500 502 503 504 /errors/50x.html;

# 2. Intercept Upstream Failures
proxy_intercept_errors on;

# 3. Protect Error Directory
location ^~ /errors/ {
    root /var/www;
    internal; # Mandatory to prevent direct client access
}

# 4. JSON Error Handling
error_page 404 @json_404;
location @json_404 {
    default_type application/json;
    return 404 '{"error": "Resource not found"}\n';
}

# 5. Maintenance Mode
if (-f /var/www/maintenance.flag) {
    return 503;
}
error_page 503 @maintenance;
location @maintenance {
    add_header Retry-After 3600 always;
    return 503 "Under scheduled maintenance. Please retry in 1 hour.\n";
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_core_module error_page Documentation](https://nginx.org/en/docs/http/ngx_http_core_module.html#error_page)
- [Official NGINX ngx_http_proxy_module proxy_intercept_errors](https://nginx.org/en/docs/http/ngx_http_proxy_module.html#proxy_intercept_errors)
- [RFC 9110: HTTP Semantics - Status Codes & Retry-After](https://datatracker.ietf.org/doc/html/rfc9110)
- [Google Search Central: Soft 404 Errors & SEO Best Practices](https://developers.google.com/search/docs/crawling-indexing/soft-404-errors)

---

[← Previous: Headers, CORS & Security Headers](08-headers-cors-and-security-headers.md) | [Module Overview](README.md) | [Next: Redirects and Canonical URLs →](10-redirects-and-canonical-urls.md)
