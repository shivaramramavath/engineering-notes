# Cheatsheet 05: HTTP Status Codes & Security Headers Reference Card

> Production reference card for HTTP status codes, RFC 7807 Problem Details schemas, and OWASP-recommended security headers.

---

### 1. HTTP Status Code Quick Reference

#### 2xx Success
- `200 OK`: Request succeeded with payload.
- `201 Created`: Resource successfully created (returns `Location` header).
- `202 Accepted`: Request accepted for asynchronous processing (e.g. queue jobs, webhooks).
- `204 No Content`: Action succeeded; response body intentionally empty (e.g. `DELETE`).

#### 3xx Redirection
- `301 Moved Permanently`: Permanent redirect (cached by browsers).
- `302 Found`: Temporary redirect (common for URL shorteners).
- `304 Not Modified`: Cached copy matches server `ETag` or `If-Modified-Since`.
- `308 Permanent Redirect`: Permanent redirect preserving HTTP request method and body.

#### 4xx Client Errors
- `400 Bad Request`: Malformed syntax or invalid request format.
- `401 Unauthorized`: Authentication missing or invalid credentials.
- `403 Forbidden`: Authenticated, but user lacks permission (RBAC).
- `404 Not Found`: Resource does not exist.
- `409 Conflict`: Business invariant collision (e.g. duplicate email, optimistic lock conflict).
- `422 Unprocessable Content`: Valid JSON syntax, but semantic validation failed (e.g. Zod error).
- `429 Too Many Requests`: Rate limit exceeded (returns `Retry-After`).

#### 5xx Server Errors
- `500 Internal Server Error`: Unhandled server exception.
- `502 Bad Gateway`: Upstream microservice returned invalid or unreachable response.
- `503 Service Unavailable`: Server shutting down, overloaded, or maintenance mode.
- `504 Gateway Timeout`: Upstream service failed to respond within timeout window.

---

### 2. RFC 7807 Problem Details Format (`application/problem+json`)
Standardized JSON error payload structure:
```json
{
  "type": "https://api.example.com/errors/validation-failed",
  "title": "Validation Failed",
  "status": 422,
  "detail": "The request body failed schema validation.",
  "instance": "/api/v1/orders/123",
  "invalidParams": [
    { "field": "amountCents", "message": "Must be greater than 0" }
  ]
}
```

---

### 3. Helmet & OWASP Security Headers

| Header | Production Setting | Security Purpose |
| :--- | :--- | :--- |
| **`Content-Security-Policy`** | `default-src 'self'` | Prevents Cross-Site Scripting (XSS) and code injection. |
| **`Strict-Transport-Security`** | `max-age=31536000; includeSubDomains` | Enforces HTTPS exclusively (HSTS). |
| **`X-Content-Type-Options`** | `nosniff` | Prevents browser MIME-type sniffing attacks. |
| **`X-Frame-Options`** | `DENY` or `SAMEORIGIN` | Mitigates Clickjacking attacks in iframes. |
| **`Referrer-Policy`** | `strict-origin-when-cross-origin` | Protects privacy by omitting URL paths in referrers. |

---

### 4. CORS Headers
```text
Access-Control-Allow-Origin: https://app.example.com
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization, X-Request-Id
Access-Control-Allow-Credentials: true
Access-Control-Max-Age: 86400
```
