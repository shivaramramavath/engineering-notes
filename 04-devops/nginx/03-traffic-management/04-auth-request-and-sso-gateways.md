---
title: "Auth Request and SSO Gateways in NGINX"
description: "Zero-trust edge security with ngx_http_auth_request_module: OAuth2-Proxy, Keycloak/Authelia SSO integration, auth_request_set header forwarding, and auth caching."
module: "03-traffic-management"
track: "devops-nginx"
order: 4
difficulty: "Advanced"
prerequisites:
  - "01-fundamentals/03-server-and-location"
  - "02-core-features/02-reverse-proxy"
  - "02-core-features/09-error-handling-and-custom-pages"
tags:
  - nginx
  - auth-request
  - sso
  - oauth2-proxy
  - zero-trust
  - keycloak
  - authentication
last_updated: "2026-03-30"
---

# Auth Request and SSO Gateways in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: The Canonical `auth_request` Architecture](#example-1-the-canonical-auth_request-architecture)
  - [Example 2: Enterprise SSO with OAuth2-Proxy (Google / GitHub / OIDC)](#example-2-enterprise-sso-with-oauth2-proxy-google--github--oidc)
  - [Example 3: Passing User Context Headers via `auth_request_set`](#example-3-passing-user-context-headers-via-auth_request_set)
  - [Example 4: Caching Authentication Decisions for Extreme Performance](#example-4-caching-authentication-decisions-for-extreme-performance)
  - [Example 5: Automated SSO Redirection and Return-To Deep Linking](#example-5-automated-sso-redirection-and-return-to-deep-linking)
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

In legacy enterprise setups, every individual backend microservice (Node.js, Spring Boot, Python FastAPI, Go) implemented its own bespoke authentication, session management, and OAuth2/OIDC token verification logic. This caused multiple architectural headaches:
1. **Security Vulnerabilities**: Inconsistent token verification implementations across teams left endpoints exposed.
2. **Duplicated Code & Maintenance**: Updating JWT validation algorithms or rotating keys required updating and redeploying dozens of services.
3. **No Centralized Access Control**: Internal legacy tools (Prometheus, Grafana, internal admin portals) lacked native SSO and were left exposed or secured with weak HTTP basic auth.

The **NGINX Auth Request Module (`ngx_http_auth_request_module`)** implements an **Edge Identity Gateway (Zero-Trust Proxy)**. Before NGINX forwards any client request to a protected backend, it generates an internal, asynchronous subrequest to an upstream authentication service (such as OAuth2-Proxy, Keycloak, or Authelia). 

If the authentication service returns an HTTP `2xx` response, NGINX forwards the request to the backend along with authenticated user identity headers. If the service returns `401` or `403`, NGINX halts processing immediately and redirects the user to the corporate login portal.

---

## 2. Core Concepts

### The Subrequest Mechanism
Unlike a full HTTP redirect that round-trips through the client browser, an **internal subrequest** is initiated entirely within NGINX's C event loop:
1. Client sends: `GET /dashboard HTTP/1.1` with a session cookie or Bearer token.
2. NGINX freezes processing of the primary request and fires an internal subrequest to the auth daemon (e.g., `GET /oauth2/auth`).
3. The auth daemon inspects the cookie/token:
   - Returns `202 Accepted` or `200 OK`: Access granted.
   - Returns `401 Unauthorized`: Invalid or expired session.
   - Returns `403 Forbidden`: Authenticated, but lacks required role.
4. NGINX acts on the status code:
   - On `2xx`: NGINX unfreezes the primary request and proxies it to the backend.
   - On `401/403`: NGINX cancels the backend request, returning the error or redirecting the user to the SSO login page.

### The `auth_request_set` Directive
When the auth service verifies a user, it often returns user identity metadata in HTTP response headers:
```http
HTTP/1.1 200 OK
X-Auth-Request-User: alice
X-Auth-Request-Email: alice@example.com
X-Auth-Request-Groups: devops,admin
```
The `auth_request_set $variable value;` directive captures these headers from the auth subrequest context and maps them into NGINX variables. NGINX can then inject them into the primary request sent to the backend using `proxy_set_header`.

### Preventing Subrequest Body Transmission
Authentication subrequests only need to verify headers, cookies, or tokens. If a client sends a 50MB file upload (`POST /upload`), transmitting that 50MB body to the auth service would waste bandwidth and cause high latency. In the auth location, you must always set:
```nginx
proxy_pass_request_body off;
proxy_set_header Content-Length "";
```

---

## 3. How It Works Internally

### The Complete Auth Request Protocol Exchange

```
Client Browser                         NGINX Reverse Proxy                     Auth Service (OAuth2-Proxy)       Backend Service
      |                                         |                                         |                             |
 1. GET /private/data (Cookie: _oauth2=...) --->|                                         |                             |
      |                                         |--- 2. Internal Subrequest ------------->|                             |
      |                                         |    GET /oauth2/auth                     |                             |
      |                                         |    (Cookie: _oauth2=...)                |                             |
      |                                         |                                         |                             |
      |                                         |                                   [Validate Session]                  |
      |                                         |                                   [Extract User Claims]               |
      |                                         |                                         |                             |
      |                                         |<-- 3. HTTP 200 OK ----------------------|                             |
      |                                         |    X-Auth-User: alice                   |                             |
      |                                         |    X-Auth-Email: alice@corp.com         |                             |
      |                                         |                                                                       |
      |                                         |-- 4. auth_request_set captures headers                                |
      |                                         |-- 5. Forward Primary Request ---------------------------------------->|
      |                                         |      GET /private/data                                                |
      |                                         |      X-User: alice                                                    |
      |                                         |      X-Email: alice@corp.com                                          |
      |                                         |                                                                       |
      |                                         |<-- 6. HTTP 200 OK (App Data) -----------------------------------------|
 <--- 7. HTTP 200 OK (App Data) ----------------|
```

### The 401 Redirection to SSO Flow

```
Client Browser                         NGINX Reverse Proxy                     Auth Service (OAuth2-Proxy)
      |                                         |                                         |
 1. GET /private/data (No Cookie) ------------->|                                         |
      |                                         |--- 2. Internal Subrequest ------------->|
      |                                         |    GET /oauth2/auth                     |
      |                                         |<-- 3. HTTP 401 Unauthorized ------------|
      |                                         |
      |                                   [error_page 401 = @login]
      |                                         |
 <--- 4. HTTP 302 Found (Redirect) -------------|
      Location: /oauth2/start?rd=/private/data
```

---

## 4. Syntax & Directives Reference

| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `auth_request uri\|off` | `http`, `server`, `location` | `off` | Enables authentication based on the result of an internal subrequest to `uri`. |
| `auth_request_set $variable value` | `http`, `server`, `location` | None | Sets an NGINX variable to the specified value after the auth subrequest completes. |
| `proxy_pass_request_body on\|off` | `http`, `server`, `location` | `on` | Must be `off` in auth location to avoid forwarding request bodies to auth service. |
| `proxy_set_header Content-Length ""` | `http`, `server`, `location` | None | Clears `Content-Length` header for subrequests with dropped bodies. |

---

## 5. Practical Implementation Examples

### Example 1: The Canonical `auth_request` Architecture

A minimal, production-ready configuration verifying a custom Python or Go authentication service.

```nginx
# /etc/nginx/conf.d/auth_basic.conf

server {
    listen 80;
    server_name internal.example.com;

    # Protected Application Endpoint
    location / {
        # Enable subrequest authentication
        auth_request /auth_verify;

        # Forward authenticated user identity
        auth_request_set $auth_user $upstream_http_x_user_id;
        proxy_set_header X-Authenticated-User $auth_user;

        proxy_pass http://backend_app;
        proxy_set_header Host $host;
    }

    # Internal Authentication Subrequest Location
    location = /auth_verify {
        internal; # Crucial: Prevent direct external access!

        proxy_pass http://127.0.0.1:9001/verify;
        
        # Don't forward client body to auth service
        proxy_pass_request_body off;
        proxy_set_header Content-Length "";

        # Pass original URI so auth service can evaluate path-based RBAC
        proxy_set_header X-Original-URI $request_uri;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

### Example 2: Enterprise SSO with OAuth2-Proxy (Google / GitHub / OIDC)

Protects internal administrative dashboards (Grafana, Prometheus, Portainer) with corporate Single Sign-On using OAuth2-Proxy.

```nginx
# /etc/nginx/conf.d/oauth2_proxy.conf

server {
    listen 443 ssl;
    http2 on;
    server_name monitoring.corp.internal;

    ssl_certificate     /etc/ssl/certs/corp.crt;
    ssl_certificate_key /etc/ssl/private/corp.key;

    # 1. Protected Dashboards
    location / {
        auth_request /oauth2/auth;
        error_page 401 = /oauth2/sign_in;

        # Capture user profile from OAuth2-Proxy
        auth_request_set $user   $upstream_http_x_auth_request_user;
        auth_request_set $email  $upstream_http_x_auth_request_email;
        auth_request_set $token  $upstream_http_x_auth_request_access_token;

        # Inject into protected backend
        proxy_set_header X-User  $user;
        proxy_set_header X-Email $email;
        proxy_set_header Authorization "Bearer $token";

        proxy_pass http://127.0.0.1:3000; # Grafana
        proxy_set_header Host $host;
    }

    # 2. OAuth2-Proxy Endpoints (Publicly reachable for callbacks)
    location /oauth2/ {
        proxy_pass       http://127.0.0.1:4180;
        proxy_set_header Host                    $host;
        proxy_set_header X-Real-IP               $remote_addr;
        proxy_set_header X-Scheme                $scheme;
        proxy_set_header X-Auth-Request-Redirect $request_uri;
    }

    # 3. Dedicated Auth Check Location
    location = /oauth2/auth {
        internal;
        proxy_pass       http://127.0.0.1:4180;
        proxy_pass_request_body off;
        proxy_set_header Content-Length "";
    }
}
```

### Example 3: Passing User Context Headers via `auth_request_set`

Resolving the variable scoping trap: when using `auth_request_set`, assigning directly to `$upstream_http_*` inside the main location fails unless mapped to a local variable first.

```nginx
# /etc/nginx/conf.d/context_forwarding.conf

server {
    listen 80;
    server_name portal.example.com;

    location /api/ {
        auth_request /auth_check;

        # Step 1: Assign upstream headers from the subrequest to local variables
        auth_request_set $auth_id     $upstream_http_x_user_id;
        auth_request_set $auth_roles  $upstream_http_x_user_roles;
        auth_request_set $auth_tenant $upstream_http_x_tenant_id;

        # Step 2: Forward local variables to the primary upstream microservice
        proxy_set_header X-User-ID     $auth_id;
        proxy_set_header X-User-Roles  $auth_roles;
        proxy_set_header X-Tenant-ID   $auth_tenant;

        proxy_pass http://api_backend_cluster;
        proxy_set_header Host $host;
    }

    location = /auth_check {
        internal;
        proxy_pass http://auth_daemon:8080/introspect;
        proxy_pass_request_body off;
        proxy_set_header Content-Length "";
        proxy_set_header Cookie $http_cookie;
    }
}
```

### Example 4: Caching Authentication Decisions for Extreme Performance

Under high traffic (e.g., loading an HTML page with 40 embedded API requests), hitting the auth daemon 40 times adds severe latency. Caching the subrequest response for 60 seconds eliminates this overhead.

```nginx
# /etc/nginx/conf.d/cached_auth.conf

# Define a shared memory cache specifically for auth tokens
proxy_cache_path /var/cache/nginx/auth_cache
                 levels=1:2
                 keys_zone=AUTH_CACHE:10m
                 max_size=1g
                 inactive=10m
                 use_temp_path=off;

server {
    listen 80;
    server_name fast.example.com;

    location / {
        auth_request /auth_cached;
        auth_request_set $user $upstream_http_x_user;
        proxy_set_header X-User $user;

        proxy_pass http://app_cluster;
    }

    location = /auth_cached {
        internal;
        proxy_pass http://127.0.0.1:8080/verify;
        proxy_pass_request_body off;
        proxy_set_header Content-Length "";

        # Enable caching of auth decisions
        proxy_cache AUTH_CACHE;

        # Cache key based on client Cookie or Bearer Token
        proxy_cache_key "$http_authorization$cookie_session_id";

        # Cache successful 200 OK verifications for 60 seconds
        proxy_cache_valid 200 60s;
        proxy_cache_valid 401 403 0s; # Never cache rejections!

        proxy_cache_lock on;
        proxy_cache_use_stale updating;
    }
}
```

### Example 5: Automated SSO Redirection and Return-To Deep Linking

Intercepts 401 Unauthorized errors and redirects the browser to the corporate identity provider (Keycloak), encoding the originally requested URL so the user is returned to the exact page after authenticating.

```nginx
# /etc/nginx/conf.d/keycloak_sso.conf

server {
    listen 443 ssl;
    server_name enterprise.example.com;

    ssl_certificate     /etc/ssl/certs/enterprise.crt;
    ssl_certificate_key /etc/ssl/private/enterprise.key;

    # Protect entire domain
    location / {
        auth_request /sso_validate;

        # Catch 401 and redirect to Keycloak login URL with return-to parameter
        error_page 401 = @sso_redirect;

        proxy_pass http://internal_service;
        proxy_set_header Host $host;
    }

    # Named location for SSO login redirection
    location @sso_redirect {
        return 302 https://sso.corp.com/realms/enterprise/protocol/openid-connect/auth?client_id=nginx_edge&response_type=code&scope=openid&redirect_uri=https://$host/sso/callback&state=$request_uri;
    }

    location = /sso_validate {
        internal;
        proxy_pass http://authelia_node:9091/api/verify;
        proxy_pass_request_body off;
        proxy_set_header Content-Length "";
        proxy_set_header X-Original-URL $scheme://$http_host$request_uri;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

---

## 6. Production Best Practices

1. **Always Mark the Auth Location as `internal;`**: The location handling `auth_request` (e.g., `/auth_verify`) must be marked `internal;`. Without `internal;`, external attackers can directly query the auth verification route, mapping internal usernames or bypassing controls.
2. **Always Turn Off Request Bodies (`proxy_pass_request_body off;`)**: In the auth location, always disable body forwarding and clear the `Content-Length` header. Forwarding multi-megabyte file uploads to an authentication service wastes memory, bandwidth, and CPU.
3. **Always Pass Original Context Headers**: Always pass `$request_uri` and `$remote_addr` to the auth service:
   ```nginx
   proxy_set_header X-Original-URI $request_uri;
   proxy_set_header X-Real-IP $remote_addr;
   ```
   This allows the authentication service to enforce path-based Role-Based Access Control (RBAC) and IP reputation checks.
4. **Cache Auth Decisions Carefully**: If caching auth subrequests (`proxy_cache`), ensure the cache key contains the exact session cookie or authorization token, and set TTL to no more than 30–60 seconds. **Never cache 401 or 403 responses**.
5. **Always Map via Local Variables**: When extracting headers with `auth_request_set`, always map to an intermediate variable (e.g., `auth_request_set $user $upstream_http_x_user;`) before using it in `proxy_set_header`. Using `$upstream_http_x_user` directly in `proxy_set_header` references the *primary* upstream's context, not the subrequest's context!
6. **Set Aggressive Timeouts on Auth Subrequests**: If the authentication daemon slows down, every single request to your application will hang. Set short timeouts (`proxy_connect_timeout 2s; proxy_read_timeout 3s;`).

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Omitting `internal;` on the auth location** | External users can browse directly to `https://example.com/oauth2/auth`, probing internal authentication responses. | Always mark auth check locations as `internal;`. |
| **Omitting `proxy_pass_request_body off;`** | When clients upload files (e.g., 20MB POST), NGINX buffers and transmits the 20MB payload to the auth service, causing timeouts and high latency. | Set `proxy_pass_request_body off;` and `proxy_set_header Content-Length "";`. |
| **Directly referencing `$upstream_http_*` in `proxy_set_header`** | `$upstream_http_x_user` evaluates against the *primary* backend rather than the auth subrequest, resulting in an empty or corrupt header. | Use `auth_request_set $saved_user $upstream_http_x_user;` and pass `$saved_user`. |
| **Caching 401 or 403 status codes in auth cache** | A user who mistypes their password once is blocked for the entire cache TTL, even after entering the correct password. | Explicitly configure `proxy_cache_valid 401 403 0s;`. |
| **Long timeouts on auth subrequests** | If the identity provider crashes, every user connection freezes for the default 60s timeout, causing 504 Gateway Timeouts. | Configure `proxy_connect_timeout 2s; proxy_read_timeout 3s;`. |
| **Forgetting to forward Cookies / Auth headers to auth service** | Auth daemon never receives client credentials; every request is rejected with 401. | Ensure `proxy_set_header Cookie $http_cookie;` or `Authorization` is passed. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test Protected Endpoint Without Credentials (Must return 401 or 302 Redirect)
curl -s -I http://internal.example.com/dashboard
# Expected: HTTP/1.1 401 Unauthorized or HTTP/1.1 302 Found (Location: /oauth2/sign_in)

# 2. Test Protected Endpoint with Valid Bearer Token
curl -s -I -H "Authorization: Bearer valid_token_here" http://internal.example.com/dashboard
# Expected: HTTP/1.1 200 OK

# 3. Verify Internal Protection of Auth Location (Must return 404)
curl -s -I http://internal.example.com/oauth2/auth
# Expected: HTTP/1.1 404 Not Found (Direct client access refused by internal directive)

# 4. Debug Auth Subrequest Handshake in Error Log
# Add to server block: error_log /var/log/nginx/error.log debug;
tail -f /var/log/nginx/error.log | grep -E "auth request|subrequest"
```

### Latency Overhead of `auth_request`
- Calling an auth subrequest over a local TCP loopback (`127.0.0.1:4180`) typically adds **0.5ms to 1.5ms** of latency per request.
- Enabling auth caching reduces this overhead to **under 0.05ms** for subsequent requests from the same session.

---

## 9. Architectural Comparison Table

| Architecture | `auth_request` Module | NGINX Plus OpenID Connect | In-App Middleware (Express/Spring) |
| :--- | :--- | :--- | :--- |
| **Licensing** | 100% Free / Open Source | Commercial (NGINX Plus) | Free |
| **Token Verification Location** | External Daemon (OAuth2-Proxy)| Native NGINX Plus Lua/C | Application Runtime |
| **Language Agnostic** | **Yes** (Protects any backend) | Yes | No (Language specific) |
| **Legacy App Protection** | **Yes** (Grafana, static files) | Yes | No |
| **Memory Footprint** | Extremely Low | Low | High (Per service VM) |
| **Complexity** | Low (Standard HTTP proxying) | Medium | High (Duplicated across services) |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: How does the ngx_http_auth_request_module work under the hood?</strong></summary>

**Answer:**
When `auth_request /auth_check;` is configured in a location block:
1. When a client request arrives, NGINX pauses the primary request in the `NGX_HTTP_ACCESS_PHASE`.
2. NGINX generates an internal asynchronous subrequest to `/auth_check`.
3. The subrequest communicates with an upstream authentication daemon over HTTP or Unix socket.
4. NGINX inspects only the HTTP status code of the subrequest response:
   - If the code is `2xx` (`200 OK`, `202 Accepted`), NGINX unpauses the primary request and proceeds to the content/proxy phase.
   - If the code is `401 Unauthorized` or `403 Forbidden`, NGINX aborts the primary request and returns that status code to the client (or executes an `error_page` redirect).
   - Any other code (e.g., `500`, `502`) is converted to an HTTP 500 error.
</details>

<details>
<summary><strong>Q2: Why must you avoid using $upstream_http_* directly in proxy_set_header when using auth_request?</strong></summary>

**Answer:**
The `$upstream_http_<header>` variables represent response headers received from the *current* upstream server. 

When a request executes `proxy_pass http://backend;` in the primary location, `$upstream_http_*` is in the scope of the `backend` upstream, not the auth subrequest. If you write:
```nginx
proxy_set_header X-User $upstream_http_x_user;
```
NGINX will look for `X-User` in the response from `http://backend` (which hasn't even been contacted yet!), resulting in an empty string.

The correct approach is to use `auth_request_set`:
```nginx
auth_request_set $saved_user $upstream_http_x_user;
proxy_set_header X-User $saved_user;
```
`auth_request_set` executes immediately when the subrequest completes, capturing the header into a local variable before the primary upstream request begins.
</details>

<details>
<summary><strong>Q3: Why is proxy_pass_request_body off mandatory in an auth subrequest location?</strong></summary>

**Answer:**
By default, NGINX forwards the client's HTTP request body to upstream proxies. 

If a client sends an HTTP `POST` or `PUT` request containing a large payload (e.g., a 100MB video or a 5MB JSON document), leaving `proxy_pass_request_body on;` forces NGINX to stream the entire 100MB body to the authentication service. The authentication service only needs to inspect cookies or headers (a few hundred bytes); streaming a 100MB body wastes network bandwidth, exhausts memory buffers, and causes severe request timeouts. 

Setting `proxy_pass_request_body off;` and clearing `Content-Length` guarantees the auth check is instantaneous.
</details>

<details>
<summary><strong>Q4: How do you implement Single Sign-On (SSO) login redirection using error_page 401?</strong></summary>

**Answer:**
When an unauthenticated client connects, the auth service returns `401 Unauthorized`. To redirect the user to the SSO login portal instead of showing a raw 401 error:
```nginx
location / {
    auth_request /oauth2/auth;
    error_page 401 = @sso_login;
    proxy_pass http://backend;
}

location @sso_login {
    return 302 https://sso.example.com/login?rd=$scheme://$host$request_uri;
}
```
NGINX catches the 401 response internally and redirects the user's browser to the identity provider with the original URL encoded in the `rd` (return directory) query parameter.
</details>

<details>
<summary><strong>Q5: Can auth_request be used to enforce Role-Based Access Control (RBAC)?</strong></summary>

**Answer:**
Yes. To enforce RBAC:
1. In NGINX, forward the target URI and HTTP method to the auth service:
   ```nginx
   proxy_set_header X-Original-URI $request_uri;
   proxy_set_header X-Original-Method $request_method;
   ```
2. The auth service (e.g., Authelia or custom Go service) inspects the user's session token to determine their roles (e.g., `viewer`, `editor`, `admin`).
3. The auth service evaluates whether the user's role permits the requested method on that URI:
   - If an `editor` attempts `DELETE /api/users`, the auth service returns `403 Forbidden`.
   - NGINX intercepts the 403 and immediately blocks the request from reaching the backend.
</details>

<details>
<summary><strong>Q6: Why is it safe to use auth_request with caching, and what precautions must be taken?</strong></summary>

**Answer:**
Caching auth responses (`proxy_cache`) is safe and dramatically improves throughput, provided three precautions are strictly enforced:
1. **Cache Key Uniqueness**: The `proxy_cache_key` must include the client's unique credential:
   `proxy_cache_key "$http_authorization$cookie_session_id$request_uri";`
2. **Short TTL**: The cache validity should be short (e.g., 30–60 seconds) so that token revocations take effect quickly.
3. **Never Cache Rejections**: Only HTTP 200 responses should be cached (`proxy_cache_valid 200 60s;`). You must explicitly set `proxy_cache_valid 401 403 0s;` so that authentication rejections are never cached.
</details>

<details>
<summary><strong>Q7: What happens if the authentication service behind auth_request crashes?</strong></summary>

**Answer:**
If the authentication service is unreachable, crashes, or times out, the internal subrequest will fail with an HTTP 502 Bad Gateway or 504 Gateway Timeout. 

Under the `ngx_http_auth_request_module` specification, **any status code other than 2xx, 401, or 403 is treated as an internal server error**. NGINX will abort the request and return an **HTTP 500 Internal Server Error** to the client. This enforces a secure fail-closed posture: access is denied by default if the identity provider is offline.
</details>

<details>
<summary><strong>Q8: How does OAuth2-Proxy integrate with NGINX auth_request?</strong></summary>

**Answer:**
OAuth2-Proxy acts as a companion daemon listening on `127.0.0.1:4180`:
1. It handles OAuth2/OIDC handshakes with external providers (Google, GitHub, Azure AD, Okta).
2. It sets an encrypted session cookie (`_oauth2_proxy`) on the client browser.
3. In NGINX, `auth_request /oauth2/auth;` points to OAuth2-Proxy's `/oauth2/auth` endpoint.
4. When NGINX subrequests `/oauth2/auth`, OAuth2-Proxy decrypts the cookie, validates token expiration with the IdP, and returns `202 Accepted` along with `X-Auth-Request-User` and `X-Auth-Request-Email` headers.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure a Basic Mock Auth Subrequest</strong></summary>

**Goal:** Configure NGINX to protect `/admin/` by sending an auth subrequest to an internal endpoint that returns HTTP 200 or 401.

**Solution:**
```nginx
server {
    listen 80;
    server_name auth.local;

    location /admin/ {
        auth_request /auth_check;
        return 200 "Welcome Admin! Access Granted.\n";
    }

    # Protected subrequest location
    location = /auth_check {
        internal;
        if ($http_x_token = "secret123") {
            return 200;
        }
        return 401;
    }
}
```
Test with:
- Unauthorized: `curl -i http://auth.local/admin/` -> `HTTP/1.1 401 Unauthorized`
- Authorized: `curl -i -H "X-Token: secret123" http://auth.local/admin/` -> `HTTP/1.1 200 OK`
</details>

<details>
<summary><strong>Exercise 2: Capture and Pass User ID via auth_request_set</strong></summary>

**Goal:** Extract a custom header `X-User-Account` emitted by the auth service and pass it to a mock backend.

**Solution:**
```nginx
server {
    listen 80;
    server_name pass.local;

    location / {
        auth_request /auth;
        auth_request_set $user_account $upstream_http_x_user_account;
        proxy_set_header X-Logged-User $user_account;
        proxy_pass http://127.0.0.1:8080;
    }

    location = /auth {
        internal;
        add_header X-User-Account "john_doe" always;
        return 200;
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Configure 401 Redirection to Login Page</strong></summary>

**Goal:** Intercept 401 errors from `auth_request` and issue an HTTP 302 redirect to `/login?return_to=<URI>`.

**Solution:**
```nginx
server {
    listen 80;
    server_name sso.local;

    location /secure/ {
        auth_request /auth_check;
        error_page 401 = @login_redirect;
        return 200 "Secure content\n";
    }

    location @login_redirect {
        return 302 /login?return_to=$request_uri;
    }

    location = /auth_check {
        internal;
        return 401; # Force unauthorized for testing
    }

    location /login {
        return 200 "Login Page Placeholder\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 4: Verify Dropped Request Body in Auth Location</strong></summary>

**Goal:** Verify that a large POST body is not forwarded to the auth service.

**Solution:**
```nginx
location = /auth_check {
    internal;
    proxy_pass http://127.0.0.1:9090;
    proxy_pass_request_body off;
    proxy_set_header Content-Length "";
}
```
Send a large payload:
```bash
curl -i -X POST http://localhost/secure/ -d @large_file.iso
```
Verify via tcpdump or mock listener on port 9090 that zero body bytes are received.
</details>

<details>
<summary><strong>Exercise 5: Cache Auth Decisions for 30 Seconds</strong></summary>

**Goal:** Configure a dedicated shared memory cache zone for auth decisions to avoid querying the auth service on repeat requests.

**Solution:**
```nginx
proxy_cache_path /tmp/auth_cache keys_zone=AUTH:5m;

location = /auth_cached {
    internal;
    proxy_pass http://127.0.0.1:9090/check;
    proxy_pass_request_body off;
    proxy_set_header Content-Length "";

    proxy_cache AUTH;
    proxy_cache_key $http_authorization;
    proxy_cache_valid 200 30s;
    proxy_cache_valid 401 0s;
}
```
</details>

<details>
<summary><strong>Exercise 6: Diagnose Auth Timeout Failures</strong></summary>

**Goal:** Simulate an auth daemon freeze and observe that NGINX safely returns an HTTP 500 error within configured timeout limits.

**Solution:**
1. Configure `proxy_read_timeout 2s;` in the auth location pointing to a non-responsive IP.
2. Query the protected endpoint.
3. Observe that after exactly 2 seconds, NGINX logs `upstream timed out` and returns `500 Internal Server Error`, maintaining a secure fail-closed posture.
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production Auth Request Cheat Sheet

server {
    listen 443 ssl;
    server_name internal.example.com;

    # 1. Protected Application Route
    location / {
        # Trigger internal auth subrequest
        auth_request /auth_verify;

        # Redirect unauthenticated clients to SSO login
        error_page 401 = @sso_redirect;

        # Capture headers from auth subrequest
        auth_request_set $user  $upstream_http_x_auth_user;
        auth_request_set $email $upstream_http_x_auth_email;

        # Forward user identity to backend
        proxy_set_header X-User-ID    $user;
        proxy_set_header X-User-Email $email;

        proxy_pass http://backend_upstream;
    }

    # 2. SSO Login Redirection
    location @sso_redirect {
        return 302 https://sso.example.com/login?rd=$scheme://$host$request_uri;
    }

    # 3. Dedicated Auth Check Location
    location = /auth_verify {
        internal; # Crucial: Prevent direct external calls!

        proxy_pass http://oauth2_proxy_cluster;
        
        # Performance & Security
        proxy_pass_request_body off;
        proxy_set_header Content-Length "";
        proxy_set_header X-Original-URI $request_uri;

        proxy_connect_timeout 2s;
        proxy_read_timeout 3s;
    }
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_auth_request_module Documentation](https://nginx.org/en/docs/http/ngx_http_auth_request_module.html)
- [OAuth2-Proxy Official Documentation](https://oauth2-proxy.github.io/oauth2-proxy/)
- [Authelia NGINX Integration Guide](https://www.authelia.com/integration/proxies/nginx/)
- [RFC 6749: The OAuth 2.0 Authorization Framework](https://datatracker.ietf.org/doc/html/rfc6749)

---

[← Previous: Rate Limiting & Traffic Shaping](03-rate-limiting.md) | [Module Overview](README.md) | [Next: Geo and Map-Based Routing →](05-geo-and-map-based-routing.md)
