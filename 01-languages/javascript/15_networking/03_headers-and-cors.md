# Headers & Cross-Origin Resource Sharing (CORS)

> Cross-Origin Resource Sharing (CORS) relaxes the browser's fundamental Same-Origin Policy, using standardized HTTP headers and preflight handshakes to govern secure cross-domain resource access.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [HTTP Fundamentals](./01_http-fundamentals.md), [The Fetch API](./02_fetch.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **Standard** | W3C CORS / WHATWG Fetch Standard / RFC 6454 (Same-Origin Policy) |

---

## Table of Contents

1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

The **Same-Origin Policy (SOP)** is the cornerstone of web application security. Under the SOP, a web browser strictly isolates documents and scripts loaded from one origin from accessing data or resources belonging to another origin. Without the SOP, visiting a malicious website (`evil.com`) in one tab would allow that site to execute JavaScript fetching your bank balance and account details from `mybank.com` in another tab, silently utilizing your active login cookies.

However, modern cloud architectures demand cross-origin data exchanges: frontends hosted on CDNs (`https://app.example.com`) regularly communicate with backend APIs on separate subdomains (`https://api.example.com`) or third-party SaaS services (`https://auth.provider.com`).

**Cross-Origin Resource Sharing (CORS)** is the W3C/WHATWG mechanism that allows servers to selectively relax the Same-Origin Policy. Using dedicated HTTP response headers, a server explicitly grants permission to specific external origins to read its responses.

---

## 2. Core Concepts

### What Defines an Origin?
An origin is defined by the exact combination of three components:
$$\text{Origin} = \text{Scheme} + \text{Host (Domain)} + \text{Port}$$

Comparing against `https://example.com:443`:

| Target URL | Same Origin? | Reason |
| :--- | :---: | :--- |
| `https://example.com/about` | **YES** | Identical scheme, host, and default port (443) |
| `http://example.com` | **NO** | Different scheme (`http` vs `https`) |
| `https://api.example.com` | **NO** | Different subdomain (`api.example.com`) |
| `https://example.com:8080` | **NO** | Different port (`8080` vs `443`) |

### Simple Requests vs Preflighted Requests

The browser categorizes cross-origin requests into two types:

1. **Simple Requests**: Requests that historically could be made via HTML forms without JavaScript. The browser sends the request directly:
   - Methods: `GET`, `HEAD`, or `POST`.
   - Headers: Only standard CORS-safe headers (`Accept`, `Accept-Language`, `Content-Language`, `Content-Type`).
   - `Content-Type`: Restricted to `application/x-www-form-urlencoded`, `multipart/form-data`, or `text/plain`.
2. **Preflighted Requests**: Any request that mutates data or uses custom headers (e.g. `POST` with `Content-Type: application/json`, `PUT`, `DELETE`, or custom auth headers like `Authorization`).
   - The browser automatically sends an **HTTP `OPTIONS`** preflight request to the server *before* sending the actual request to verify permission.

```text
Browser Client (https://app.com)            API Server (https://api.com)
       │                                                 │
       │ 1. OPTIONS /api/data (Preflight)                │
       ├────────────────────────────────────────────────>│
       │    Origin: https://app.com                      │
       │    Access-Control-Request-Method: DELETE        │
       │    Access-Control-Request-Headers: Authorization│
       │                                                 │
       │ 2. 204 No Content (Preflight Approval)          │
       │<────────────────────────────────────────────────┤
       │    Access-Control-Allow-Origin: https://app.com │
       │    Access-Control-Allow-Methods: GET, DELETE    │
       │    Access-Control-Allow-Headers: Authorization  │
       │    Access-Control-Max-Age: 86400                │
       │                                                 │
       │ 3. DELETE /api/data (Actual Request)            │
       ├────────────────────────────────────────────────>│
       │    Authorization: Bearer xyz                    │
       │                                                 │
       │ 4. 200 OK (Response with Data)                  │
       │<────────────────────────────────────────────────┤
       │    Access-Control-Allow-Origin: https://app.com │
```

---

## 3. How It Works Internally

### Browser CORS Enforcement Pipeline

```
 JavaScript calls fetch('https://api.com/users', { headers: { 'Authorization': '...' }})
                                  │
                                  ▼
 Is target URL same-origin with active window?
             /                  \
       YES  /                    \ NO
           v                      v
 [ Send Request Directly ]   Does it qualify as a "Simple Request"?
                                  /                   \
                            YES  /                     \ NO
                                v                       v
                    [ Send Request Directly ]     [ Send HTTP OPTIONS Preflight ]
                                │                               │
                                v                               v
                    Did server return valid         Did server return 2xx and valid
                    Access-Control-Allow-Origin?    Access-Control-Allow-* headers?
                                /         \                     /         \
                          YES  /           \ NO           YES  /           \ NO
                              v             v                 v             v
                       [ Deliver Data ] [ CORS Error ]  [ Send Actual ] [ Block & Reject ]
                                                        [ Request ]     [ Promise with TypeError ]
```

Crucially: **CORS is enforced by the BROWSER, not by the server.** The server receives and processes the request; the browser inspects the response headers and blocks JavaScript from reading the data if the origin is not approved.

---

## 4. Syntax & API Reference

### The `Headers` Interface

```javascript
const headers = new Headers();
headers.append('Accept', 'application/json');
headers.set('Content-Type', 'application/json'); // Overwrite

// Inspection (Headers are case-insensitive!)
headers.get('content-type'); // 'application/json'
headers.has('authorization');

// Guard checks
headers.delete('cache-control');

// Passing to Fetch
fetch('/api', { headers });
```

### Essential CORS Response Headers

```http
Access-Control-Allow-Origin: https://app.example.com
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With
Access-Control-Allow-Credentials: true
Access-Control-Expose-Headers: Content-Length, X-Kms-Version
Access-Control-Max-Age: 86400
```

---

## 5. Practical Examples

### Example 1: Handling CORS Preflight in Node.js (Vanilla HTTP)
```javascript
import http from 'node:http';

const server = http.createServer((req, res) => {
  const origin = req.headers.origin;
  const allowedOrigins = ['https://dashboard.example.com', 'http://localhost:3000'];

  if (allowedOrigins.includes(origin)) {
    res.setHeader('Access-Control-Allow-Origin', origin);
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');
    res.setHeader('Access-Control-Allow-Credentials', 'true');
    res.setHeader('Access-Control-Max-Age', '86400'); // Cache preflight for 24 hours
  }

  // Handle preflight OPTIONS request immediately!
  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  // Standard API business logic
  if (req.url === '/api/user' && req.method === 'GET') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ username: 'AdaLovelace' }));
  }
});

server.listen(4000);
```

### Example 2: Exposing Custom Response Headers to JavaScript
```javascript
// By default, JavaScript can only read 7 CORS-safelisted response headers:
// Cache-Control, Content-Language, Content-Length, Content-Type, Expires, Last-Modified, Pragma.
// To allow client JS to read a custom header like 'X-Total-Count':

// SERVER SENDS:
// Access-Control-Expose-Headers: X-Total-Count, X-RateLimit-Remaining

// CLIENT JAVASCRIPT:
async function fetchPaginatedUsers(page) {
  const res = await fetch(`https://api.example.com/users?page=${page}`);

  // Without Access-Control-Expose-Headers, this returns null!
  const totalCount = res.headers.get('X-Total-Count');
  const data = await res.json();

  return { users: data, total: parseInt(totalCount, 10) };
}
```

### Example 3: Cross-Origin Authenticated Cookies
```javascript
// When sending cookies cross-origin, TWO conditions must be met:
// 1. Client specifies credentials: 'include'
// 2. Server MUST specify exact origin in Access-Control-Allow-Origin (NOT wildcard '*')
//    AND must return Access-Control-Allow-Credentials: true

async function fetchUserSession() {
  const response = await fetch('https://auth.company.com/session', {
    method: 'GET',
    credentials: 'include', // Sends cross-origin session cookies
  });

  return response.json();
}
```

### Example 4: Node.js Express CORS Middleware Configuration
```javascript
import express from 'express';
import cors from 'cors';

const app = express();

const corsOptions = {
  origin: (origin, callback) => {
    const whitelist = ['https://app.mysite.com', 'https://admin.mysite.com'];
    if (!origin || whitelist.includes(origin)) {
      callback(null, true);
    } else {
      callback(new Error('Blocked by CORS policy'));
    }
  },
  methods: ['GET', 'POST', 'PUT', 'DELETE'],
  allowedHeaders: ['Content-Type', 'Authorization', 'X-Requested-With'],
  credentials: true,
  maxAge: 3600, // Preflight cache in seconds
};

app.use(cors(corsOptions));
```

### Example 5: Inspecting Request Headers in Frontend
```javascript
function inspectRequestHeaders(headers) {
  for (const [name, value] of headers.entries()) {
    console.log(`${name}: ${value}`);
  }
}

const reqHeaders = new Headers({
  'Content-Type': 'application/json',
  'Accept': 'application/json',
});
inspectRequestHeaders(reqHeaders);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Wildcard `*` Origin with Credentials Crash**: Setting `Access-Control-Allow-Origin: *` while simultaneously setting `Access-Control-Allow-Credentials: true` **is strictly prohibited by the CORS specification**. The browser will reject the response with a fatal CORS error. You must echo the client's explicit origin.
2. **Generic `TypeError: Failed to fetch`**: When a request fails due to a CORS violation, the browser intentionally conceals the failure reason from JavaScript to prevent origin reconnaissance. The `fetch()` promise rejects with a generic `TypeError`. You must inspect the browser DevTools Console to read the underlying CORS error message.
3. **Server Side Effects Still Execute on CORS Failure**: A common misconception is that CORS blocks the request from reaching the server. For simple requests or after a successful preflight, the server **receives, processes, and commits the database transaction**! The browser only blocks the JavaScript client from *reading the response*.
4. **Localhost Port Mismatches**: `http://localhost:3000` and `http://localhost:5000` are **different origins** because their port numbers differ. Cross-port requests during local development require full CORS configuration.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Wildcard with Credentials** | `Allow-Origin: *` with `Allow-Credentials: true` | Echo specific Origin: `Allow-Origin: https://app.com` | Spec forbids wildcard origins when credentials are included. |
| **Missing Preflight Handler** | Only handling `POST` route in server | Implement handler for HTTP `OPTIONS` | Browser cancels actual request if OPTIONS preflight fails. |
| **Using `no-cors` to Bypass CORS** | Setting `{ mode: 'no-cors' }` to read JSON | Configure proper CORS headers on server | Returns opaque response; body cannot be read by JS. |
| **Trying to Fix CORS in Client JS**| Adding CORS headers inside `fetch()` call | Configure CORS headers on the **server** | CORS is a server-enforced response policy; clients cannot grant permissions. |
| **Unexposed Custom Headers** | Reading custom `X-Total-Count` without config | Add `Access-Control-Expose-Headers: X-Total-Count` | Browser conceals non-safelisted headers from client JavaScript. |
| **Missing `Vary: Origin`** | Caching dynamic CORS response without `Vary` | Add `Vary: Origin` response header | Reverse proxies serve cached origin headers to different clients. |

---

## 8. Performance Characteristics

- **Preflight Latency Cost**: An un-cached preflight request adds a full extra round trip (**1 RTT**) before the actual request can be dispatched.
- **Preflight Caching with `Access-Control-Max-Age`**: Setting `Access-Control-Max-Age: 86400` instructs the browser to cache preflight approvals for 24 hours (clamped to 2 hours in Chromium, 24 hours in Firefox), eliminating preflight latency for subsequent requests.

---

## 9. Engine Internals

1. **Chromium CORS in Network Service**: In Chromium, CORS is validated directly in the C++ `services/network/cors/` subsystem. If a check fails, the network process cancels the response stream before it passes IPC buffers into the untrusted Renderer Process.
2. **Header Case Normalization**: The `Headers` interface normalizes all header names to lowercase ASCII strings internally, matching HTTP/2 and HTTP/3 byte serialization contracts.

---

## 10. Best Practices & Design Guidelines

1. **Set `Access-Control-Max-Age`**: Always return `Access-Control-Max-Age: 7200` on preflight responses to eliminate unnecessary `OPTIONS` round trips.
2. **Always Include `Vary: Origin`**: Whenever `Access-Control-Allow-Origin` is dynamically generated based on the request origin, include `Vary: Origin` so intermediate CDNs do not poison caches across domains.
3. **Keep Whitelists Strict**: Never echo arbitrary origins dynamically in production; validate against a strictly defined domain whitelist.
4. **Use Backend Reverse Proxies (BFF)**: In single-page architectures, consider placing a reverse proxy (like NGINX, Cloudflare, or Next.js rewrites) in front of APIs to make all calls same-origin, eliminating CORS complexity entirely.

---

## 11. Real-World Case Study

### High-Throughput Microservice Gateway with CORS Preflight Optimization

An analytics dashboard displays live charts by polling 20 microservices cross-origin. Each poll triggered an `OPTIONS` preflight request, doubling traffic to 200,000 requests per minute and increasing latency by 120ms.

```javascript
// NGINX / Gateway Optimization Configuration:
// 1. Efficient Preflight Caching & Dynamic Whitelisting
export function corsGatewayMiddleware(req, res, next) {
  const origin = req.headers.origin;
  const allowedPattern = /^https:\/\/(.*\.)?mycompany\.com$/;

  if (origin && allowedPattern.test(origin)) {
    res.setHeader('Access-Control-Allow-Origin', origin);
    res.setHeader('Access-Control-Allow-Credentials', 'true');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Request-ID');
    res.setHeader('Access-Control-Expose-Headers', 'X-Total-Count, X-Request-ID');
    
    // CRITICAL: Cache preflight for 2 hours (Chromium maximum cap)
    res.setHeader('Access-Control-Max-Age', '7200');
    // Ensure CDNs do not serve cached origin to wrong caller
    res.setHeader('Vary', 'Origin');
  }

  if (req.method === 'OPTIONS') {
    // Return fast 204 immediately without executing backend queries
    res.status(204).end();
    return;
  }

  next();
}

// Outcome:
// - Gateway preflight request volume dropped by 94%
// - Average API latency dropped from 180ms to 62ms
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is the Same-Origin Policy (SOP), and what components define an origin?</strong></summary>

The Same-Origin Policy is a fundamental browser security mechanism that restricts scripts on one origin from accessing or reading resources from another origin. An origin is defined by the exact combination of three components: **Scheme (protocol)**, **Host (domain name)**, and **Port number**.
</details>

<details>
<summary><strong>2. Can you fix a CORS error by changing code in client-side JavaScript?</strong></summary>

No. CORS is a policy enforced by the browser based on HTTP response headers sent by the **server**. Client-side JavaScript cannot bypass or grant itself CORS permissions. The server hosting the requested resource must be configured to return valid `Access-Control-Allow-*` response headers.
</details>

<details>
<summary><strong>3. What triggers a CORS Preflight (OPTIONS) request?</strong></summary>

A preflight request is triggered if the request does not qualify as a "Simple Request". This includes:
- HTTP methods other than `GET`, `HEAD`, or `POST` (e.g. `PUT`, `DELETE`, `PATCH`).
- Setting custom request headers (such as `Authorization` or `X-Api-Key`).
- A `Content-Type` other than `application/x-www-form-urlencoded`, `multipart/form-data`, or `text/plain` (e.g. `application/json`).
</details>

<details>
<summary><strong>4. Why is setting <code>Access-Control-Allow-Origin: *</code> alongside <code>Access-Control-Allow-Credentials: true</code> invalid?</strong></summary>

The CORS specification explicitly prohibits wildcard `*` origins when credentials (cookies or HTTP authentication) are permitted. Allowing wildcards with credentials would permit any malicious website in the world to steal authenticated user data via CSRF/XSS. When credentials are true, the server must specify the exact, verified origin.
</details>

<details>
<summary><strong>5. What does the <code>Access-Control-Max-Age</code> header do?</strong></summary>

It specifies how many seconds the browser is permitted to cache the result of an `OPTIONS` preflight response. During this duration, subsequent cross-origin requests with the same method and headers skip the preflight step, saving a full network round-trip.
</details>

<details>
<summary><strong>6. Does a CORS failure mean the server never executed the request?</strong></summary>

No. For simple requests or once preflight passes, the browser sends the actual request to the server. The server executes the logic and sends back a response. If the response headers lack valid CORS permissions, the **browser blocks the client-side JavaScript from reading the response**. The server side-effects have already taken place!
</details>

<details>
<summary><strong>7. Why must a server include the <code>Vary: Origin</code> header when using dynamic CORS origins?</strong></summary>

If the server dynamically sets `Access-Control-Allow-Origin` based on the incoming `Origin` header, intermediate caching proxies or CDNs might cache the response for `siteA.com` and serve it to `siteB.com`, triggering a CORS failure for `siteB.com`. `Vary: Origin` instructs caches to store separate copies per origin.
</details>

<details>
<summary><strong>8. What is the purpose of the <code>Access-Control-Expose-Headers</code> header?</strong></summary>

By default, the browser only permits JavaScript to access 7 basic CORS-safelisted response headers. If the server sends custom headers (like `X-Total-Count` or `ETag`) that the client needs to read, the server must list them in `Access-Control-Expose-Headers`.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an Origin Matcher with Wildcard Subdomains
Write a Node.js utility function `isAllowedOrigin(requestOrigin, allowedPatterns)` that validates whether an incoming origin matches a list of allowed origins, supporting wildcard subdomains (e.g. `*.mycompany.com`).

<details>
<summary>View Solution</summary>

```javascript
function isAllowedOrigin(requestOrigin, allowedPatterns) {
  if (!requestOrigin) return false;

  return allowedPatterns.some((pattern) => {
    // Exact match
    if (pattern === requestOrigin) return true;

    // Wildcard subdomain matching (e.g. "https://*.example.com")
    if (pattern.includes('*')) {
      const escaped = pattern
        .replace(/[.+?^${}()|[\]\\]/g, '\\$&') // Escape regex specials
        .replace('\\*', '[a-zA-Z0-9-]+');     // Convert * to subdomain regex
      const regex = new RegExp(`^${escaped}$`);
      return regex.test(requestOrigin);
    }

    return false;
  });
}

// Verification:
const whitelist = ['https://*.company.com', 'https://admin.portal.org'];
console.log(isAllowedOrigin('https://app.company.com', whitelist)); // true
console.log(isAllowedOrigin('https://evil.com', whitelist));        // false
```
</details>

### Challenge 2: Client-Side CORS Diagnostic Helper
Write a wrapper function `safeCorsFetch(url, options)` that attempts a `fetch()` call. If it throws a `TypeError: Failed to fetch`, it performs diagnostic heuristics (detecting protocol, origin mismatch, and offline status) to throw a clear, human-readable error.

<details>
<summary>View Solution</summary>

```javascript
async function safeCorsFetch(url, options = {}) {
  try {
    return await fetch(url, options);
  } catch (err) {
    if (err instanceof TypeError && err.message.includes('fetch')) {
      if (!navigator.onLine) {
        throw new Error('Network error: Device is currently offline.');
      }

      const targetUrl = new URL(url, window.location.href);
      const isCrossOrigin = targetUrl.origin !== window.location.origin;

      if (isCrossOrigin) {
        throw new Error(
          `CORS Error: Failed to fetch cross-origin resource at ${targetUrl.origin}. ` +
          `Verify that the server handles OPTIONS preflight and sets Access-Control-Allow-Origin.`
        );
      }
    }
    throw err;
  }
}
```
</details>
