---
title: "Project 06: Public API Gateway with Tiered Rate Limiting & CORS"
description: "Production reference implementation for a developer-facing public API gateway: multi-tiered rate limiting by API key, RFC 6585 HTTP 429 JSON errors, and robust CORS preflight handling."
module: "07-projects"
project_id: "06-rate-limited-public-api-gateway"
status: "completed"
tags: ["nginx", "project", "api-gateway", "rate-limiting", "cors", "api-keys", "json-error", "rfc6585"]
prerequisites: ["03-traffic-management/03-rate-limiting.md", "02-core-features/08-headers-cors-and-security-headers.md"]
estimated_read_time: "35 min"
---

# Project 06: Public API Gateway with Tiered Rate Limiting & CORS

```
+-------------------------------------------------------------------------------+
| Multi-Tiered Public API Gateway Architecture                                  |
|                                                                               |
|  [Developer Client / Frontend Application]                                    |
|        │                                                                      |
|        ├── 1. CORS Preflight Handshake: OPTIONS /v1/data                      |
|        │      Origin: https://webapp.partner.com                              |
|        │      ==> NGINX validates Origin whitelist & returns 204 No Content  |
|        │                                                                      |
|        └── 2. Authenticated API Request: GET /v1/data                         |
|               X-API-Key: "tier_free_xyz123"                                   |
|        │                                                                      |
|        ▼                                                                      |
|  [NGINX API Gateway Core]                                                     |
|        │                                                                      |
|        ├── Tier Evaluation via Hash Map:                                      |
|        │   - Free Tier    --> Limit: 5 req/sec (burst=10, delay=5)            |
|        │   - Premium Tier --> Limit: 50 req/sec (burst=100, nodelay)          |
|        │   - Internal App --> Unthrottled (Empty tracking key)                |
|        │                                                                      |
|        ├── Rate Exceeded?                                                     |
|        │   ├── YES: Intercept & Return HTTP 429 Too Many Requests (JSON)      |
|        │   │        Headers: Retry-After: 1, Content-Type: application/json  |
|        │   │                                                                  |
|        │   └── NO: Pass to Protected API Upstream Cluster                     |
+-------------------------------------------------------------------------------+
```

---

## 1. Architecture & Objective

### Objective
Design and implement a robust, developer-facing public API gateway that enforces service-level agreements (SLAs) through multi-tiered rate limiting keyed on client API tokens, provides production-grade Cross-Origin Resource Sharing (CORS) preflight handling, and emits RFC-compliant structured JSON errors.

### Core Architectural Requirements
1. **Tiered Rate Limiting by API Key**:
   - *Free Tier*: Capped at 5 requests/sec with two-stage smoothing (`burst=10 delay=5`).
   - *Premium Tier*: Capped at 50 requests/sec with immediate burst processing (`burst=100 nodelay`).
   - *Anonymous (Missing Key)*: Strict IP-based throttling at 2 requests/sec.
2. **Standardized RFC 6585 HTTP 429 Errors**: Return an immediate, structured JSON payload accompanied by standard rate-limiting headers (`Retry-After`, `X-RateLimit-Limit`).
3. **Safe CORS Architecture**: Accurately validate incoming `Origin` headers against an approved whitelist, responding to `OPTIONS` preflight checks instantly at the proxy edge without waking up backend microservices.
4. **Header Preservation on Error Responses**: Guarantee that CORS headers (`Access-Control-Allow-Origin`) remain present on HTTP 429 and 500 error responses (`always` flag), preventing frontend browser masking errors.

---

## 2. Complete Config Code

### 1. Unified API Gateway Configuration (`/etc/nginx/conf.d/gateway.example.com.conf`)

```nginx
# ==============================================================================
# 1. CORS Origin Whitelist (http context)
# ==============================================================================
map $http_origin $cors_allowed_origin {
    default                          "";
    "https://example.com"            "$http_origin";
    "https://dashboard.example.com"  "$http_origin";
    "~^https://.*\.partner\.com$"    "$http_origin";
}

# ==============================================================================
# 2. Rate Limiting Tier Mapping by API Key
# ==============================================================================
# Map the API key to a specific rate tier
map $http_x_api_key $rate_tier {
    default                 "anonymous";
    "~^key_free_"           "free";
    "~^key_prem_"           "premium";
    "key_internal_supersecret" "unthrottled";
}

# Map rate tier to the tracking keys
map $rate_tier $key_anonymous {
    "anonymous" $binary_remote_addr;
    default     "";
}

map $rate_tier $key_free {
    "free"  $http_x_api_key;
    default "";
}

map $rate_tier $key_premium {
    "premium" $http_x_api_key;
    default   "";
}

# ==============================================================================
# 3. Shared Memory Rate Limit Zones
# ==============================================================================
limit_req_zone $key_anonymous zone=tier_anon_zone:10m rate=2r/s;
limit_req_zone $key_free      zone=tier_free_zone:20m rate=5r/s;
limit_req_zone $key_premium   zone=tier_prem_zone:20m rate=50r/s;

# Return RFC 6585 status 429
limit_req_status 429;

# Upstream Backend Pool
upstream api_backend_pool {
    server 127.0.0.1:4000;
    keepalive 32;
}

# ==============================================================================
# 4. Master Public Gateway Virtual Host
# ==============================================================================
server {
    listen 443 ssl;
    http2 on;
    server_name api.example.com;

    # TLS 1.3
    ssl_certificate /etc/letsencrypt/live/api.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/api.example.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;

    # Apply All Three Tier Zones (Only the matching key evaluates; empty keys are skipped!)
    limit_req zone=tier_anon_zone burst=5 nodelay;
    limit_req zone=tier_free_zone burst=10 delay=5;
    limit_req zone=tier_prem_zone burst=100 nodelay;

    # Custom RFC 6585 JSON Error Handler for HTTP 429
    error_page 429 = @rate_limit_exceeded;

    # --------------------------------------------------------------------------
    # API Routing Location
    # --------------------------------------------------------------------------
    location /v1/ {
        # Handle CORS Preflight Requests
        if ($request_method = 'OPTIONS') {
            add_header Access-Control-Allow-Origin $cors_allowed_origin always;
            add_header Access-Control-Allow-Methods "GET, POST, PUT, DELETE, OPTIONS" always;
            add_header Access-Control-Allow-Headers "Authorization, Content-Type, X-API-Key, Accept" always;
            add_header Access-Control-Max-Age 86400 always;
            add_header Content-Type "text/plain; charset=utf-8";
            add_header Content-Length 0;
            return 204;
        }

        proxy_pass http://api_backend_pool;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

        # Standard CORS Headers on Success
        add_header Access-Control-Allow-Origin $cors_allowed_origin always;
        add_header Access-Control-Allow-Credentials "true" always;
        add_header X-Content-Type-Options "nosniff" always;
    }

    # --------------------------------------------------------------------------
    # 429 Rate Limit Intercept Handler (RFC 6585 JSON)
    # --------------------------------------------------------------------------
    location @rate_limit_exceeded {
        default_type application/json;
        add_header Access-Control-Allow-Origin $cors_allowed_origin always;
        add_header Retry-After 1 always;
        add_header X-RateLimit-Limit $rate_tier always;

        return 429 '{\n  "error": "TooManyRequests",\n  "message": "API request rate limit exceeded for your tier.",\n  "tier": "$rate_tier",\n  "retry_after_seconds": 1\n}\n';
    }
}
```

---

## 3. Step-by-Step Walkthrough

1. **Multi-Tiered Rate Mapping Architecture**:
   - A single NGINX location cannot easily express dynamic if-else limits. We solve this using **sparse tracking keys**.
   - If an API key starts with `key_free_`, `$rate_tier` evaluates to `"free"`.
   - In turn, `$key_free` receives the API key string, while `$key_anonymous` and `$key_premium` evaluate to an **empty string (`""`)**.
   - *Core Invariant*: NGINX ignores shared memory tracking when the tracking key evaluates to an empty string! Thus, only the `tier_free_zone` throttles the request; the other two zones do not evaluate.

2. **Two-Stage Traffic Smoothing (`delay=5`) on Free Tier**:
   - `limit_req zone=tier_free_zone burst=10 delay=5;` implements two-stage shaping.
   - The first 5 burst requests are admitted instantly with 0ms delay.
   - The subsequent 5 requests (from 6 to 10) are intentionally delayed to smooth out spikes before reaching the backend.
   - Requests exceeding 10 are immediately rejected with HTTP 429.

3. **CORS Preflight Interception at the Edge**:
   - Browsers send an `OPTIONS` preflight before cross-origin POST/PUT calls.
   - The `if ($request_method = 'OPTIONS')` block validates the origin against `$cors_allowed_origin`, writes the permitted headers, and returns **HTTP 204 No Content** directly from NGINX in less than 1ms, completely sparing backend threads.

4. **The `always` Flag on Error Pages**:
   - Notice that every `add_header` directive uses the `always` parameter.
   - Without `always`, NGINX strips custom headers on 4xx/5xx responses. If a browser encounters a 429 error without `Access-Control-Allow-Origin: ...`, the browser reports a generic `CORS Error` in the JavaScript console, masking the actual rate-limiting root cause!

---

## 4. Verification & Testing

### Test 1: Verify Free Tier Rate Limiting & JSON 429 Payload
```bash
for i in {1..12}; do
    curl -s -H "X-API-Key: key_free_developer_01" https://api.example.com/v1/users
done
```
*Expected Output on Requests 11 and 12*:
```json
HTTP/2 429
content-type: application/json
retry-after: 1
x-ratelimit-limit: free
access-control-allow-origin: https://example.com

{
  "error": "TooManyRequests",
  "message": "API request rate limit exceeded for your tier.",
  "tier": "free",
  "retry_after_seconds": 1
}
```

### Test 2: Verify CORS Preflight `OPTIONS` Returns 204
```bash
curl -v -X OPTIONS \
  -H "Origin: https://dashboard.example.com" \
  -H "Access-Control-Request-Method: POST" \
  -H "Access-Control-Request-Headers: X-API-Key, Content-Type" \
  https://api.example.com/v1/users
```
*Expected Output*:
```http
HTTP/2 204
access-control-allow-origin: https://dashboard.example.com
access-control-allow-methods: GET, POST, PUT, DELETE, OPTIONS
access-control-allow-headers: Authorization, Content-Type, X-API-Key, Accept
access-control-max-age: 86400
content-length: 0
```

### Test 3: Verify Unauthorized Origin Rejection
```bash
curl -sI -H "Origin: https://malicious-site.com" https://api.example.com/v1/users | grep -i "Access-Control-Allow-Origin"
# Output is empty (Header not emitted!)
```

---

## 5. Troubleshooting & Edge Cases

### Edge Case 1: Frontend Reports "CORS Error" When Rate Limited
- **Symptom**: Frontend developers see a red CORS error in Chrome DevTools instead of an HTTP 429 response.
- **Root Cause**: `Access-Control-Allow-Origin` was omitted inside the `@rate_limit_exceeded` error location or missing the `always` parameter.
- **Fix**: Ensure `add_header Access-Control-Allow-Origin $cors_allowed_origin always;` is present inside `@rate_limit_exceeded`.

### Edge Case 2: Memory Zone Depletion for Anonymous Tier
- **Symptom**: High volume of unique scrapers exhaust `tier_anon_zone`.
- **Root Cause**: Using `$remote_addr` (15 bytes text) rather than `$binary_remote_addr` (4 bytes binary).
- **Fix**: Always index IP-based zones with `$binary_remote_addr` to maximize shared memory density.

---

[← Previous Project: 05-websocket-and-sse-realtime-proxy.md](05-websocket-and-sse-realtime-proxy.md) | [Back to Module Index](README.md) | [Next Project: 07-tls-termination-and-certbot-automation.md →](07-tls-termination-and-certbot-automation.md)
