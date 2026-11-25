---
title: "HTTP Headers, CORS, and Security Headers in NGINX"
description: "Production guide to HTTP header manipulation, modern security headers (CSP, HSTS, Permissions-Policy), dynamic multi-origin CORS, and navigating the add_header inheritance trap."
module: "02-core-features"
track: "devops-nginx"
order: 8
difficulty: "Intermediate to Advanced"
prerequisites:
  - "01-fundamentals/03-server-and-location"
  - "02-core-features/04-variables-and-rewrites"
tags:
  - nginx
  - security-headers
  - cors
  - add-header
  - csp
  - hsts
  - permissions-policy
last_updated: "2026-03-30"
---

# HTTP Headers, CORS, and Security Headers in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: The Modern Enterprise Security Headers Suite](#example-1-the-modern-enterprise-security-headers-suite)
  - [Example 2: Production Multi-Origin CORS with Preflight `OPTIONS`](#example-2-production-multi-origin-cors-with-preflight-options)
  - [Example 3: Safe Header Inheritance Pattern (The `include` Strategy)](#example-3-safe-header-inheritance-pattern-the-include-strategy)
  - [Example 4: Hiding Backend Fingerprints (`Server`, `X-Powered-By`)](#example-4-hiding-backend-fingerprints-server-x-powered-by)
  - [Example 5: Dynamic Correlation / Request Tracing Headers](#example-5-dynamic-correlation--request-tracing-headers)
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

HTTP headers are the control plane of web security, browser behavior, and distributed observability. When properly configured at the NGINX edge:
1. **Security Headers** protect end users against Cross-Site Scripting (XSS), Clickjacking, MIME-confusion attacks, and protocol downgrades.
2. **CORS (Cross-Origin Resource Sharing)** coordinates secure cross-domain communications between single-page applications and backend microservices.
3. **Observability Headers** inject distributed tracing identifiers (e.g., `X-Request-ID`) across asynchronous service boundaries.

### The Infamous `add_header` Inheritance Trap
The single most frequent security misconfiguration in NGINX revolves around the inheritance model of the `add_header` directive. In NGINX, **header directives do not merge across scopes**. If a parent block (`http` or `server`) defines ten critical security headers, and a child `location` defines just *one* custom header (e.g., `add_header Cache-Control "no-cache";`), **all ten parent security headers are completely discarded** for that location. Understanding this mechanism is vital for maintaining production security posture.

---

## 2. Core Concepts

### The Critical Security Headers
- **`Content-Security-Policy` (CSP)**: Restricts the origins from which browsers can load scripts, stylesheets, images, fonts, and frames, effectively preventing reflected and stored Cross-Site Scripting (XSS).
- **`Strict-Transport-Security` (HSTS)**: Instructs browsers to communicate with the domain exclusively over HTTPS for a defined duration (`max-age`), resisting SSL-stripping man-in-the-middle attacks.
- **`X-Frame-Options`**: Prevents the site from being rendered inside an `<iframe>`, `<frame>`, or `<object>`, mitigating Clickjacking attacks.
- **`X-Content-Type-Options: nosniff`**: Enforces strict MIME typing, preventing browsers from "sniffing" a non-executable MIME type (e.g., `text/plain`) into an executable script.
- **`Referrer-Policy`**: Governs how much referrer information (URL path and query parameters) is sent in the `Referer` header when navigating between origins.
- **`Permissions-Policy`**: Controls browser hardware features and APIs (e.g., `camera=()`, `microphone=()`, `geolocation=()`).

### The `always` Parameter
By default, NGINX's `add_header` only attaches headers to responses with successful status codes (`200, 201, 204, 206, 301, 302, 303, 304, 307, 308`). If a request triggers an error (such as `400 Bad Request`, `403 Forbidden`, `404 Not Found`, or `500 Internal Server Error`), NGINX drops the headers. Adding the `always` parameter forces NGINX to emit the header on **all responses**, including error codes.

### Cross-Origin Resource Sharing (CORS) Mechanics
Under the Same-Origin Policy (SOP), browsers restrict web pages from making cross-origin XMLHttpRequest/Fetch calls. CORS safely loosens this restriction:
- **Simple Requests**: GET/HEAD/POST with basic headers; browser checks `Access-Control-Allow-Origin`.
- **Preflight Requests**: Requests with custom headers (e.g., `Authorization`), non-standard methods (PUT/DELETE/PATCH), or JSON payloads trigger an initial `OPTIONS` request. NGINX must intercept this `OPTIONS` request and return `204 No Content` with appropriate `Access-Control-Allow-*` headers without forwarding the request to the upstream backend.
- **Credentials and Wildcards**: If `Access-Control-Allow-Credentials: true` is set (permitting cookies and authorization headers), the browser **strictly rejects** `Access-Control-Allow-Origin: *`. The origin must be echoed dynamically from an approved whitelist.

---

## 3. How It Works Internally

### The `add_header` Inheritance State Machine

```
+------------------------------------------------------------+
| http {} Context                                            |
| add_header X-Frame-Options "DENY" always;                 |
| add_header X-Content-Type-Options "nosniff" always;       |
+-----------------------------+------------------------------+
                              | Inherited by default
+-----------------------------v------------------------------+
| server {} Context                                          |
| (Inherits both headers from http)                          |
+--------------+------------------------------+--------------+
               |                              |
               | No add_header declared       | Defines 1 local add_header
               v                              v
+-------------------------------+ +---------------------------------------+
| location /api/ {              | | location /static/ {                   |
|   # Inherits BOTH:            | |   add_header Cache-Control "max-age"; |
|   # - X-Frame-Options         | |                                       |
|   # - X-Content-Type-Options  | |   # TRAP TRIGGERED!                   |
| }                             | |   # ALL PARENT HEADERS DISCARDED!     |
+-------------------------------+ |   # ZERO security headers emitted!    |
                                  +---------------------------------------+
```

### Preflight CORS Execution Flow

```
Browser                                               NGINX Edge                                  Upstream
   |                                                      |                                          |
   |--- 1. Preflight HTTP OPTIONS /api/data ------------->|                                          |
   |    Origin: https://frontend.example.com              |                                          |
   |    Access-Control-Request-Method: POST               |                                          |
   |                                                      |                                          |
   |                                          [Match Origin in map]                          |
   |                                          [Is Method == OPTIONS?]                        |
   |                                                      |                                          |
   |<-- 2. HTTP 204 No Content ---------------------------|  (Upstream NEVER contacted)              |
   |    Access-Control-Allow-Origin: https://frontend... |                                          |
   |    Access-Control-Allow-Methods: GET, POST, OPTIONS  |                                          |
   |    Access-Control-Max-Age: 86400                     |                                          |
   |                                                      |                                          |
   |--- 3. Actual HTTP POST /api/data ------------------->|                                          |
   |    Origin: https://frontend.example.com              |                                          |
   |                                                      |--- Forward POST /api/data -------------->|
   |                                                      |<-- Return 200 OK (JSON) -----------------|
   |<-- 4. HTTP 200 OK (with CORS headers) ---------------|                                          |
```

---

## 4. Syntax & Directives Reference

| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `add_header name value [always]` | `http`, `server`, `location`, `if in location` | None | Adds specified header to response. Use `always` to cover error responses. |
| `add_trailer name value [always]` | `http`, `server`, `location`, `if in location` | None | Adds trailing header to chunked responses. |
| `server_tokens on\|off\|build\|string` | `http`, `server`, `location` | `on` | Controls emission of NGINX version string in `Server` header and error pages. |
| `proxy_hide_header field` | `http`, `server`, `location` | None | Strips specified header received from upstream before sending to client. |
| `proxy_pass_header field` | `http`, `server`, `location` | None | Permits forwarding of otherwise hidden hop-by-hop upstream headers. |
| `proxy_set_header field value` | `http`, `server`, `location` | Host, Connection | Sets or modifies request headers forwarded *to* the upstream backend. |

---

## 5. Practical Implementation Examples

### Example 1: The Modern Enterprise Security Headers Suite

Production-ready baseline implementing strict modern browser security standards.

```nginx
# /etc/nginx/snippets/security_headers.conf

# 1. Enforce HTTPS across all subdomains for 2 years (Preload eligible)
add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;

# 2. Prevent Clickjacking
add_header X-Frame-Options "DENY" always;

# 3. Prevent MIME Confusion / Sniffing
add_header X-Content-Type-Options "nosniff" always;

# 4. Strict Referrer Governance (Only send origin on cross-origin HTTPS requests)
add_header Referrer-Policy "strict-origin-when-cross-origin" always;

# 5. Restrict Browser Features and Hardware APIs
add_header Permissions-Policy "camera=(), microphone=(), geolocation=(), payment=(), usb=()" always;

# 6. Modern Content Security Policy (CSP)
add_header Content-Security-Policy "default-src 'self'; script-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net; style-src 'self' 'unsafe-inline'; img-src 'self' data: https:; font-src 'self' data:; connect-src 'self' https://api.example.com; frame-ancestors 'none'; base-uri 'self'; form-action 'self';" always;

# 7. Legacy XSS Protection Header (Explicitly disabled in favor of CSP)
add_header X-XSS-Protection "0" always;
```

```nginx
# /etc/nginx/conf.d/app.conf
server {
    listen 443 ssl;
    http2 on;
    server_name portal.example.com;

    ssl_certificate     /etc/ssl/certs/portal.crt;
    ssl_certificate_key /etc/ssl/private/portal.key;

    # Include security suite
    include /etc/nginx/snippets/security_headers.conf;

    location / {
        proxy_pass http://portal_backend;
    }
}
```

### Example 2: Production Multi-Origin CORS with Preflight `OPTIONS`

Enables dynamic origin validation against an approved whitelist, supports credentials, and handles preflight caching.

```nginx
# /etc/nginx/conf.d/cors_api.conf

# Dynamic Origin Whitelist
map $http_origin $cors_origin {
    default                             "";
    "~^https://(.*\.)?example\.com$"   "$http_origin";
    "https://admin.internal-portal.net" "$http_origin";
    "http://localhost:3000"             "$http_origin"; # Local development
}

server {
    listen 443 ssl;
    server_name api.example.com;

    ssl_certificate     /etc/ssl/certs/api.crt;
    ssl_certificate_key /etc/ssl/private/api.key;

    location /v1/ {
        # Check if origin is allowed
        set $cors_cred "";
        if ($cors_origin != "") {
            set $cors_cred "true";
        }

        # Intercept Preflight OPTIONS requests
        if ($request_method = 'OPTIONS') {
            add_header 'Access-Control-Allow-Origin' $cors_origin always;
            add_header 'Access-Control-Allow-Credentials' $cors_cred always;
            add_header 'Access-Control-Allow-Methods' 'GET, POST, PUT, DELETE, PATCH, OPTIONS' always;
            add_header 'Access-Control-Allow-Headers' 'Authorization, Content-Type, Accept, X-Requested-With, X-Correlation-ID' always;
            add_header 'Access-Control-Max-Age' 86400 always; # Cache preflight for 24 hours
            add_header 'Content-Type' 'text/plain; charset=utf-8';
            add_header 'Content-Length' 0;
            return 204;
        }

        # Actual Requests (GET, POST, etc.)
        add_header 'Access-Control-Allow-Origin' $cors_origin always;
        add_header 'Access-Control-Allow-Credentials' $cors_cred always;
        add_header 'Access-Control-Expose-Headers' 'Content-Length, Content-Range, X-Correlation-ID' always;

        proxy_pass http://127.0.0.1:4000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

### Example 3: Safe Header Inheritance Pattern (The `include` Strategy)

Resolving the inheritance trap by breaking headers into reusable snippets and including them uniformly.

```nginx
# /etc/nginx/snippets/base_security.conf
add_header X-Frame-Options "DENY" always;
add_header X-Content-Type-Options "nosniff" always;
add_header Referrer-Policy "strict-origin" always;
```

```nginx
# /etc/nginx/conf.d/safe_inheritance.conf
server {
    listen 443 ssl;
    server_name secure.example.com;

    ssl_certificate     /etc/ssl/certs/secure.crt;
    ssl_certificate_key /etc/ssl/private/secure.key;

    # Include at server level
    include /etc/nginx/snippets/base_security.conf;

    location /public/ {
        # This location adds NO headers -> safely inherits server-level headers!
        proxy_pass http://public_service;
    }

    location /static/ {
        # WARNING: Because this location defines a custom header,
        # we MUST explicitly re-include base_security.conf here!
        include /etc/nginx/snippets/base_security.conf;

        expires 30d;
        add_header Cache-Control "public, no-transform" always;
        root /var/www/static;
    }
}
```

### Example 4: Hiding Backend Fingerprints (`Server`, `X-Powered-By`)

Prevents information leakage that aids attackers in fingerprinting technology stacks.

```nginx
# /etc/nginx/conf.d/hide_fingerprints.conf

# Hide NGINX version string globally (Server: nginx instead of Server: nginx/1.26.1)
server_tokens off;

server {
    listen 80;
    server_name api.example.com;

    location / {
        proxy_pass http://backend_cluster;

        # Strip headers emitted by backend application runtimes
        proxy_hide_header X-Powered-By;   # Express.js, PHP
        proxy_hide_header X-AspNet-Version; # Microsoft ASP.NET
        proxy_hide_header X-Runtime;      # Ruby on Rails
        proxy_hide_header Server;         # Upstream web server banner

        # Inject unified edge server header
        add_header Server "Protected-Gateway" always;
    }
}
```

### Example 5: Dynamic Correlation / Request Tracing Headers

Injecting a unique cryptographic correlation ID for every request to trace requests across microservices.

```nginx
# /etc/nginx/conf.d/tracing.conf

# Use client-supplied ID if present, otherwise fall back to NGINX generated request_id
map $http_x_correlation_id $correlation_id {
    default   $http_x_correlation_id;
    ""        $request_id;
}

server {
    listen 80;
    server_name trace.example.com;

    location / {
        # Forward tracing identifier to backend microservice
        proxy_set_header X-Correlation-ID $correlation_id;
        proxy_set_header X-Request-ID $correlation_id;

        # Echo tracing identifier back to client in response
        add_header X-Correlation-ID $correlation_id always;

        proxy_pass http://127.0.0.1:8000;
        proxy_set_header Host $host;
    }
}
```

---

## 6. Production Best Practices

1. **Always Append `always`**: Every `add_header` directive should end with `always` unless you specifically want the header excluded on 4xx/5xx responses.
2. **Never Use Wildcards with Credentials in CORS**: The CORS specification forbids `Access-Control-Allow-Origin: *` when `Access-Control-Allow-Credentials: true`. Always validate and echo the incoming origin using `map`.
3. **Audit Location Blocks for Inheritance Leaks**: Whenever an `add_header` is declared inside a `location {}`, verify that all intended security headers are explicitly re-included.
4. **Always Set `server_tokens off;`**: Preventing NGINX from broadcasting its exact version number mitigates automated vulnerability scanners targeting known CVEs.
5. **Set `X-XSS-Protection "0"` in Modern Sites**: The legacy `X-XSS-Protection: 1; mode=block` filter in older browsers introduced security vulnerabilities and false positives. Modern standard practice is to disable it and enforce security via Content Security Policy.
6. **Preflight Caching (`Access-Control-Max-Age`)**: Set preflight caching to at least `86400` (24 hours) to avoid doubling network roundtrips for every cross-origin API call.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Declaring `add_header` in `location` without re-including parent headers** | NGINX completely discards all `http` and `server` level headers for that location; site loses HSTS, CSP, and clickjacking protection on that path. | Use snippet files (`include /etc/nginx/snippets/security.conf;`) and include them inside every `location` that defines local headers. |
| **Omitting `always` on `add_header`** | Error pages (e.g., `404 Not Found` or `500 Server Error`) omit security headers, leaving error pages vulnerable to clickjacking and MIME attacks. | Append `always` to every `add_header` directive. |
| **`Access-Control-Allow-Origin: *` with credentials** | Browsers strictly abort the request and log an error in the developer console: *"The value of 'Access-Control-Allow-Origin' cannot be '*' when credentials flag is true"*. | Use `map $http_origin $cors_origin` to whitelist and echo the valid origin dynamically. |
| **Forwarding `OPTIONS` requests to slow backends** | Upstream application framework must boot request contexts to answer preflights, adding 100-300ms latency to every cross-origin API call. | Intercept `OPTIONS` directly in NGINX and return `204 No Content`. |
| **Incomplete `proxy_hide_header`** | Attackers read `X-Powered-By: Express` or `Server: Apache-Coyote/1.1` to launch framework-specific exploits. | Declare `proxy_hide_header` for all backend signature headers. |
| **Setting CSP `unsafe-eval` without necessity** | Completely compromises the script execution sandbox of CSP, allowing attackers to execute strings as code. | Refactor frontend code to eliminate `eval()` and maintain strict CSP. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Inspect Response Headers from CLI
curl -s -I https://portal.example.com | grep -E -i "x-frame|strict|content-security|x-content"

# 2. Test CORS Preflight Request Handling
curl -s -I -X OPTIONS https://api.example.com/v1/users \
  -H "Origin: https://frontend.example.com" \
  -H "Access-Control-Request-Method: POST" \
  -H "Access-Control-Request-Headers: Authorization, Content-Type"

# Verify that response is HTTP/1.1 204 No Content or HTTP/2 204 with:
# access-control-allow-origin: https://frontend.example.com
# access-control-allow-credentials: true
# access-control-max-age: 86400

# 3. Test That Error Responses Retain Security Headers (Verify 'always' flag)
curl -s -I https://portal.example.com/non-existent-path-404 | grep -i "x-frame-options"
# Must return: X-Frame-Options: DENY

# 4. Check for Hidden Backend Fingerprints
curl -s -I https://api.example.com/ | grep -i -E "x-powered-by|server"
```

### Security Header Grade Verification
You can evaluate your domain's live configuration using industry-standard tools:
- **Security Headers Analyzer**: https://securityheaders.com/ (Target: A+)
- **Mozilla Observatory**: https://observatory.mozilla.org/

---

## 9. Architectural Comparison Table

| Security Header | Primary Threat Mitigated | Enforced By | Recommended Value |
| :--- | :--- | :--- | :--- |
| **`Strict-Transport-Security`** | SSL-Stripping & MITM downgrades | Browser network stack | `max-age=63072000; includeSubDomains; preload` |
| **`Content-Security-Policy`** | Cross-Site Scripting (XSS), Data Injection | Browser DOM engine | `default-src 'self'; script-src 'self' ...` |
| **`X-Frame-Options`** | Clickjacking & UI Redressing | Browser framing engine | `DENY` or `SAMEORIGIN` |
| **`X-Content-Type-Options`** | MIME Confusion & Sniffing | Browser parser | `nosniff` |
| **`Referrer-Policy`** | Sensitive URL Token Leakage | Browser request engine | `strict-origin-when-cross-origin` |
| **`Permissions-Policy`** | Unauthorized Hardware Access | Browser hardware layer | `camera=(), microphone=(), geolocation=()` |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: Explain the add_header inheritance trap in NGINX and how it can lead to critical security vulnerabilities.</strong></summary>

**Answer:**
In NGINX, array directives like `add_header` do not merge when moving from an outer configuration context (like `http` or `server`) to an inner context (like `location`). 

If an engineer defines universal security headers (such as `Strict-Transport-Security`, `X-Frame-Options`, and `Content-Security-Policy`) in the `server` block, and then configures a child block:
```nginx
location /download/ {
    add_header Content-Disposition "attachment";
    proxy_pass http://storage;
}
```
NGINX completely wipes out all inherited headers from the `server` block for requests matching `/download/`. The response will contain *only* `Content-Disposition`, leaving that endpoint entirely stripped of HSTS, clickjacking, and XSS protections. 

To fix this, either avoid declaring `add_header` in child locations or use snippet files (`include /etc/nginx/snippets/security.conf;`) to re-apply the full security header suite whenever a child block declares local headers.
</details>

<details>
<summary><strong>Q2: What is the purpose of the always parameter on the add_header directive?</strong></summary>

**Answer:**
By default, the `add_header` directive only attaches headers if the HTTP response status code is considered successful or a standard redirect (`200, 201, 204, 206, 301, 302, 303, 304, 307, 308`). 

If a request results in an error code (such as `400 Bad Request`, `401 Unauthorized`, `403 Forbidden`, `404 Not Found`, or `500 Internal Server Error`), NGINX will **omit** the header by default. An attacker could exploit this by triggering intentional 404 or 500 error pages to embed the site within a malicious iframe (Clickjacking) or bypass CSP protections. Appending `always` instructs NGINX to emit the header on every single response, regardless of status code.
</details>

<details>
<summary><strong>Q3: Why can't you set Access-Control-Allow-Origin: * when Access-Control-Allow-Credentials: true is present?</strong></summary>

**Answer:**
The W3C CORS specification explicitly prohibits the combination of `Access-Control-Allow-Origin: *` and `Access-Control-Allow-Credentials: true`. 

When credentials (cookies, HTTP basic authentication, or TLS client certificates) are allowed, allowing a wildcard origin `*` would allow any arbitrary website on the internet to make authenticated cross-origin requests on behalf of a logged-in user and read the response data, completely bypassing the Same-Origin Policy. If credentials are true, the server **must** explicitly validate the incoming `Origin` header against an approved whitelist and echo that exact origin string back in `Access-Control-Allow-Origin`.
</details>

<details>
<summary><strong>Q4: How should NGINX handle CORS preflight OPTIONS requests, and why should they not be forwarded to upstream servers?</strong></summary>

**Answer:**
When a browser initiates a complex cross-origin request (e.g., using `PUT`, `DELETE`, `Authorization` header, or `application/json`), it first issues a preflight `OPTIONS` request.

NGINX should intercept this `OPTIONS` request directly at the edge, validate the `Origin`, attach the allowed methods and headers, set a cache duration via `Access-Control-Max-Age: 86400`, and immediately return `204 No Content`. 

Forwarding `OPTIONS` requests to upstream application servers wastes backend database connections and compute cycles, and increases latency by 100–300ms before the client can even initiate the actual payload request.
</details>

<details>
<summary><strong>Q5: What is the difference between proxy_set_header and add_header?</strong></summary>

**Answer:**
- `proxy_set_header`: Modifies the HTTP request headers that NGINX **sends to the upstream backend** server (e.g., setting `Host`, `X-Real-IP`, or `X-Forwarded-For`). These headers are never seen by the client browser.
- `add_header`: Modifies the HTTP response headers that NGINX **sends back to the client browser** (e.g., `Set-Cookie`, `Strict-Transport-Security`, `Content-Security-Policy`).
</details>

<details>
<summary><strong>Q6: Why is X-XSS-Protection: 0 recommended over X-XSS-Protection: 1; mode=block in modern configurations?</strong></summary>

**Answer:**
The legacy `X-XSS-Protection` header controlled the built-in XSS filter in older browsers (Internet Explorer, older Chrome/Safari). Security researchers discovered that the browser's heuristic XSS auditor could be abused by attackers to selectively disable legitimate scripts on a page, effectively creating side-channel vulnerabilities. 

Modern web standards deprecated the filter. The consensus from OWASP and major browser vendors is to set `X-XSS-Protection: 0` to disable the legacy auditor and enforce strict XSS defense exclusively through `Content-Security-Policy`.
</details>

<details>
<summary><strong>Q7: How do you strip the Server header or hide the NGINX version string?</strong></summary>

**Answer:**
1. To hide the version number: Set `server_tokens off;` in the `http {}` or `server {}` context. This changes the header from `Server: nginx/1.26.1` to `Server: nginx`.
2. To remove or replace the `Server` header entirely: In open-source NGINX, you can use `proxy_hide_header Server;` to strip upstream server headers, and override the edge header via `add_header Server "Gateway" always;`. (Completely omitting the `Server` header on static responses requires compilation with the third-party `headers-more-nginx-module` via `more_clear_headers Server;`).
</details>

<details>
<summary><strong>Q8: What is the purpose of the Permissions-Policy header?</strong></summary>

**Answer:**
`Permissions-Policy` (formerly known as `Feature-Policy`) allows site owners to selectively enable, disable, or restrict access to browser hardware APIs and sensitive browser features. For example:
```http
Permissions-Policy: camera=(), microphone=(), geolocation=(), payment=()
```
This guarantees that even if a third-party script or embedded iframe is compromised via XSS, the attacker cannot access the user's webcam, microphone, location coordinates, or payment APIs.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Comprehensive Security Headers on an Existing Server</strong></summary>

**Goal:** Create a security snippet and apply HSTS, X-Frame-Options, X-Content-Type-Options, and Referrer-Policy to an HTTPS server block.

**Solution:**
```bash
# Create snippet file: /etc/nginx/snippets/security.conf
cat << 'EOF' > /etc/nginx/snippets/security.conf
add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;
add_header X-Frame-Options "DENY" always;
add_header X-Content-Type-Options "nosniff" always;
add_header Referrer-Policy "strict-origin-when-cross-origin" always;
EOF
```

```nginx
server {
    listen 443 ssl;
    server_name secure.local;
    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;

    include /etc/nginx/snippets/security.conf;

    location / {
        return 200 "Secured!\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 2: Implement Multi-Domain CORS Whitelist with `map`</strong></summary>

**Goal:** Allow CORS access only to `https://app.example.com` and `https://dashboard.example.com`, rejecting all other origins.

**Solution:**
```nginx
map $http_origin $cors_allowed {
    default                             "";
    "https://app.example.com"          "$http_origin";
    "https://dashboard.example.com"    "$http_origin";
}

server {
    listen 80;
    server_name api.local;

    location /api/ {
        add_header Access-Control-Allow-Origin $cors_allowed always;
        add_header Access-Control-Allow-Methods "GET, POST, OPTIONS" always;
        add_header Access-Control-Allow-Headers "Authorization, Content-Type" always;

        if ($request_method = 'OPTIONS') {
            return 204;
        }

        proxy_pass http://127.0.0.1:3000;
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Diagnose and Fix an `add_header` Inheritance Leak</strong></summary>

**Goal:** Observe security headers disappearing in a child location and repair the configuration.

**Solution:**
1. Broken configuration:
   ```nginx
   server {
       add_header X-Frame-Options "DENY" always;
       location /images/ {
           add_header Cache-Control "max-age=3600"; # Wipes X-Frame-Options!
       }
   }
   ```
2. Repaired configuration:
   ```nginx
   server {
       include /etc/nginx/snippets/security.conf;

       location /images/ {
           include /etc/nginx/snippets/security.conf; # Re-included!
           add_header Cache-Control "max-age=3600" always;
       }
   }
   ```
</details>

<details>
<summary><strong>Exercise 4: Intercept and Cache CORS Preflight OPTIONS Requests</strong></summary>

**Goal:** Return `204 No Content` for `OPTIONS` with a 24-hour preflight cache header.

**Solution:**
```nginx
location /v2/ {
    if ($request_method = 'OPTIONS') {
        add_header Access-Control-Allow-Origin $http_origin always;
        add_header Access-Control-Allow-Methods "GET, POST, PUT, DELETE, OPTIONS" always;
        add_header Access-Control-Allow-Headers "Authorization, Content-Type" always;
        add_header Access-Control-Max-Age 86400 always;
        return 204;
    }
    proxy_pass http://backend_pool;
}
```
</details>

<details>
<summary><strong>Exercise 5: Hide Upstream Node.js and PHP Version Fingerprints</strong></summary>

**Goal:** Configure NGINX to strip `X-Powered-By` and `X-Runtime` headers emitted by backend application servers.

**Solution:**
```nginx
server {
    listen 80;
    server_name proxy.local;
    server_tokens off;

    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_hide_header X-Powered-By;
        proxy_hide_header X-Runtime;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6: Generate and Propagate Distributed Correlation IDs</strong></summary>

**Goal:** Inject an `X-Request-ID` into every incoming request, forward it to upstream, and return it in the client response header.

**Solution:**
```nginx
server {
    listen 80;
    server_name tracing.local;

    location / {
        # $request_id is a 32-char hex string generated by NGINX
        proxy_set_header X-Request-ID $request_id;
        add_header X-Request-ID $request_id always;

        proxy_pass http://127.0.0.1:4000;
    }
}
```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production Security Headers & CORS Quick Reference

# 1. Hide Server Version
server_tokens off;

# 2. Universal Security Headers (Always use 'always')
add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
add_header X-Frame-Options "DENY" always;
add_header X-Content-Type-Options "nosniff" always;
add_header Referrer-Policy "strict-origin-when-cross-origin" always;
add_header Permissions-Policy "camera=(), microphone=(), geolocation=()" always;

# 3. Dynamic CORS
map $http_origin $cors_origin {
    default                 "";
    "https://example.com"   "$http_origin";
}

# 4. Strip Upstream Headers
proxy_hide_header X-Powered-By;

# 5. Inheritance Rule
# If you use add_header inside location {}, ALL server-level headers are LOST.
# Always re-include your security snippet inside the location!
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_headers_module Documentation](https://nginx.org/en/docs/http/ngx_http_headers_module.html)
- [OWASP Secure Headers Project](https://owasp.org/www-project-secure-headers/)
- [Mozilla Developer Network: Cross-Origin Resource Sharing (CORS)](https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS)
- [W3C Content Security Policy Level 3](https://www.w3.org/TR/CSP3/)

---

[← Previous: Compression (Gzip and Brotli)](07-compression-gzip-and-brotli.md) | [Module Overview](README.md) | [Next: Error Handling and Custom Pages →](09-error-handling-and-custom-pages.md)
