# Security Headers & CORS Snippets

> **Cheatsheet ID:** `NGX-SHT-006`  
> **Topic:** Production-Grade Security Headers, CSP Configurations, and Robust CORS Implementations  
> **Target Version:** NGINX 1.30+ (Stable) / 1.31+ (Mainline)  
> **Related Concept Notes:** [08-headers-cors-and-security-headers.md](../02-core-features/08-headers-cors-and-security-headers.md), [02-security-hardening.md](../05-security-performance/02-security-hardening.md)

---

## 1. The Critical `add_header` Inheritance Rule

> **NGINX Golden Rule:** If a child context (`server` or `location`) declares **even one** `add_header` directive, it **completely disables** all `add_header` directives inherited from parent scopes (`http` or `server`).
>
> **Best Practice Solution:** Store standard security headers in a modular snippet file (e.g., `/etc/nginx/snippets/security-headers.conf`) and `include` it inside **every** `server` and `location` block that adds headers. Always append the `always` flag so headers are returned on error codes (4xx/5xx) as well as 2xx/3xx.

---

## 2. Hardened Security Headers Snippet

Save this file as `/etc/nginx/snippets/security-headers.conf`:

```nginx
# ==============================================================================
# /etc/nginx/snippets/security-headers.conf
# OWASP / Mozilla Modern Baseline Security Headers
# ==============================================================================

# 1. HTTP Strict Transport Security (HSTS)
# 2-year duration (63072000s), include subdomains, eligible for browser preload list
add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;

# 2. Prevent MIME-sniffing
add_header X-Content-Type-Options "nosniff" always;

# 3. Clickjacking Protection (DENY or SAMEORIGIN)
add_header X-Frame-Options "DENY" always;

# 4. Strict Referrer Policy
add_header Referrer-Policy "strict-origin-when-cross-origin" always;

# 5. Cross-Origin Resource Isolation Policies
add_header Cross-Origin-Opener-Policy "same-origin" always;
add_header Cross-Origin-Resource-Policy "same-origin" always;
add_header Cross-Origin-Embedder-Policy "require-corp" always;

# 6. Granular Hardware & Browser API Permissions Policy
add_header Permissions-Policy "camera=(), microphone=(), geolocation=(), payment=(), usb=(), screen-wake-lock=()" always;

# 7. Content Security Policy (Baseline REST API or Strict SPA)
add_header Content-Security-Policy "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: https:; font-src 'self'; connect-src 'self' https://api.example.com; object-src 'none'; frame-ancestors 'none'; base-uri 'self'; form-action 'self';" always;
```

---

## 3. Specialized Content Security Policy (CSP) Profiles

### 3.1 Strict Modern Single Page Application (SPA / React / Vue)
```nginx
add_header Content-Security-Policy "default-src 'none'; script-src 'self'; style-src 'self' https://fonts.googleapis.com; font-src 'self' https://fonts.gstatic.com; img-src 'self' data: blob:; connect-src 'self' https://api.example.com wss://ws.example.com; manifest-src 'self'; frame-ancestors 'none'; base-uri 'self'; form-action 'self';" always;
```

### 3.2 Report-Only Mode (Zero-Risk Audit & Rollout)
```nginx
add_header Content-Security-Policy-Report-Only "default-src 'self'; script-src 'self' https://trustedscripts.example.com; report-uri /api/v1/csp-violations; report-to csp-endpoint" always;
```

### 3.3 Pure REST API Backend (No HTML, No Scripts)
```nginx
add_header Content-Security-Policy "default-src 'none'; frame-ancestors 'none'" always;
```

---

## 4. Production-Grade CORS Architecture

Handling Cross-Origin Resource Sharing (CORS) correctly in NGINX without insecure wildcards (`*`) when credentials are used (`Access-Control-Allow-Credentials: true`).

### 4.1 Modular CORS Configuration via `map` (`http` context)
```nginx
# 1. Allowed Origins Whitelist
map $http_origin $cors_origin {
    default                          "";
    "~^https://(www\.)?example\.com$"  $http_origin;
    "~^https://admin\.example\.com$"   $http_origin;
    "~^https://dev\.example\.com:[0-9]+$" $http_origin; # Local dev support
}

# 2. Identify OPTIONS Preflight
map $request_method $is_options {
    OPTIONS 1;
    default 0;
}
```

### 4.2 Application in Virtual Host (`server` context)
```nginx
server {
    listen 443 ssl;
    http2 on;
    server_name api.example.com;

    # Shared SSL, logs, etc.
    include /etc/nginx/snippets/security-headers.conf;

    location /api/ {
        # Preflight handling
        if ($request_method = 'OPTIONS') {
            add_header Access-Control-Allow-Origin $cors_origin always;
            add_header Access-Control-Allow-Credentials 'true' always;
            add_header Access-Control-Allow-Methods 'GET, POST, PUT, PATCH, DELETE, OPTIONS' always;
            add_header Access-Control-Allow-Headers 'Authorization, Content-Type, Accept, X-Requested-With, Origin' always;
            add_header Access-Control-Max-Age 86400 always; # 24 hour cache
            add_header Content-Type 'text/plain; charset=utf-8';
            add_header Content-Length 0;
            return 204;
        }

        # Actual Request CORS Headers
        add_header Access-Control-Allow-Origin $cors_origin always;
        add_header Access-Control-Allow-Credentials 'true' always;
        add_header Access-Control-Expose-Headers 'Content-Length, Content-Range, X-Request-ID' always;

        # Must re-include security headers because add_header was called in this location!
        include /etc/nginx/snippets/security-headers.conf;

        proxy_pass http://api_backend;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

---

## 5. Security Headers Verification Matrix

| Header | Target Value | Protects Against |
| :--- | :--- | :--- |
| `Strict-Transport-Security` | `max-age=63072000; includeSubDomains; preload` | SSL-stripping, MITM, downgraded HTTP connections |
| `X-Content-Type-Options` | `nosniff` | MIME-type confusion attacks and executable script injection |
| `X-Frame-Options` | `DENY` or `SAMEORIGIN` | Clickjacking, unauthorized UI redressing |
| `Content-Security-Policy` | Strict script-src, frame-ancestors 'none' | Cross-Site Scripting (XSS), data exfiltration, clickjacking |
| `Referrer-Policy` | `strict-origin-when-cross-origin` | Sensitive URL path / token leakage in HTTP Referer |
| `Permissions-Policy` | `camera=(), microphone=(), ...` | Unauthorized device feature & sensor access by 3rd-party frames |
| `Cross-Origin-Opener-Policy` | `same-origin` | Spectre side-channel attacks across browsing contexts |
| `Cross-Origin-Resource-Policy`| `same-origin` | Unauthorized reads of sensitive static assets across origins |

---

## 6. Verification and Diagnostic CLI Commands

```bash
# 1. Inspect all returned response headers using curl
curl -s -I https://api.example.com/api/v1/health

# 2. Test preflight OPTIONS request
curl -s -I -X OPTIONS https://api.example.com/api/v1/users \
  -H "Origin: https://admin.example.com" \
  -H "Access-Control-Request-Method: POST" \
  -H "Access-Control-Request-Headers: Authorization, Content-Type"

# 3. Test CORS rejection against an untrusted origin
curl -s -I https://api.example.com/api/v1/users \
  -H "Origin: https://malicious-site.com" | grep -i "access-control-allow-origin"
# (Output should be empty or omitted)

# 4. Check online security rating with Mozilla Observatory or securityheaders.com
curl -s https://api.securityheaders.com/analyze?q=https://example.com | jq .
```
