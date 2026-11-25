---
title: "NGINX Extensibility with njs (NGINX JavaScript)"
description: "Mastering njs for programmatic request routing, JWT validation, dynamic header manipulation, QuickJS integration, and response transformation."
module: "02-core-features"
track: "devops-nginx"
order: 12
difficulty: "Advanced"
prerequisites:
  - "01-fundamentals/02-config-structure"
  - "02-core-features/04-variables-and-rewrites"
tags:
  - nginx
  - njs
  - javascript
  - quickjs
  - extensibility
  - jwt
  - edge-compute
last_updated: "2026-03-30"
---

# NGINX Extensibility with njs (NGINX JavaScript)

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Dynamic Routing and Header Injection with `js_set`](#example-1-dynamic-routing-and-header-injection-with-js_set)
  - [Example 2: Edge JWT Signature and Claim Validation](#example-2-edge-jwt-signature-and-claim-validation)
  - [Example 3: Dynamic Asynchronous Subrequests with `ngx.fetch()`](#example-3-dynamic-asynchronous-subrequests-with-ngx_fetch)
  - [Example 4: Response Body Masking and Transformation (`js_body_filter`)](#example-4-response-body-masking-and-transformation-js_body_filter)
  - [Example 5: Modern ES2023 Scripting with QuickJS Engine (`js_engine qjs`)](#example-5-modern-es2023-scripting-with-quickjs-engine-js_engine-qjs)
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

Historically, extending NGINX with complex business logic required either writing native C modules (which requires deep knowledge of NGINX internals and recompilation) or adopting OpenResty / Lua (which introduces a separate runtime ecosystem and third-party dependencies).

To provide a native, lightweight, and official programmatic extension mechanism, F5/NGINX developed **njs (NGINX JavaScript)**. 

### Why njs?
1. **Official & Mainline Support**: Maintained directly by the core NGINX engineering team. Available as official packages across Ubuntu, Debian, RHEL, and official Docker images.
2. **Ultra-Low Memory Footprint**: Unlike Node.js or V8, which allocate hundreds of megabytes for a long-lived runtime with complex garbage collectors, njs uses an **ephemeral per-request VM model**. A lightweight JavaScript VM is initialized for a request and torn down immediately upon request completion, requiring **zero garbage collection pauses**.
3. **Modern JavaScript Support via QuickJS**: In modern njs (0.8.x+), engineers can switch the underlying execution engine to Fabrice Bellard's **QuickJS** (`js_engine qjs;`), enabling modern ES2023 language features, async/await, native Promises, arrow functions, and destructuring.
4. **Rich Edge Capabilities**: Handle edge authentication (HMAC, JWT), dynamic routing, upstream canary splitting, subrequest aggregation (`ngx.fetch()`), and real-time response body masking.

---

## 2. Core Concepts

### The Per-Request VM Architecture
In Node.js or browser environments, a single JavaScript engine instance runs continuously, managing a global heap, event loop, and garbage collector. 
In njs, **each HTTP request receives its own isolated, lightweight VM instance**. Variables, objects, and memory allocated during request processing reside in the request's internal memory pool. When the HTTP request finishes and the socket is closed or reused, the entire memory pool is freed in one operation. This prevents memory leaks and eliminates non-deterministic GC latency spikes.

### The njs Request Object (`r`)
When an njs function is invoked in the HTTP context, NGINX passes an HTTP Request object (`r`) as the first argument. Key properties include:
- `r.uri`: Current normalized URI string.
- `r.args`: Object containing parsed query parameters.
- `r.headersIn`: Object containing incoming client headers (e.g., `r.headersIn['Authorization']`).
- `r.headersOut`: Object containing outgoing response headers.
- `r.variables`: Dictionary providing read/write access to NGINX built-in variables (e.g., `r.variables.remote_addr`, `r.variables.upstream_target`).
- `r.return(status, [body])`: Completes the request and sends the HTTP response immediately.
- `r.internalRedirect(uri)`: Executes an internal NGINX redirect.

### Execution Hooks in the NGINX Pipeline
njs can be attached at different phases of the HTTP lifecycle:
- **Variable Evaluation (`js_set`)**: Evaluates a JavaScript function lazily when a variable is read.
- **Content Generation (`js_content`)**: Generates the complete HTTP response body programmatically.
- **Header Modification (`js_header_filter`)**: Inspects and mutates response headers before they are sent to the client.
- **Body Transformation (`js_body_filter`)**: Inspects, masks, or modifies streaming response body chunks.

---

## 3. How It Works Internally

### njs Request Execution Lifecycle

```
Client Request arrives at NGINX Worker
               |
               v
+------------------------------------+
| Worker Event Loop (epoll)          |
+--------------+---------------------+
               |
               v
+------------------------------------+
| Request Memory Pool Initialized    |
| (ngx_http_request_t allocated)     |
+--------------+---------------------+
               |
               v
+------------------------------------+
| njs Context Spawned                |
| - Lightweight VM cloned from cache |
| - Global scope pre-compiled        |
| - Request object 'r' populated     |
+--------------+---------------------+
               |
               +-------------------------------------------+
               |                                           |
       js_content handler                           js_set variable read
               |                                           |
+--------------v---------------------+      +--------------v---------------------+
| Execute JS function                |      | Execute getter function            |
| - r.headersIn parsed               |      | - Calculate hash / auth token      |
| - r.return(200, "JSON payload")    |      | - Assign result to $variable       |
+--------------+---------------------+      +--------------+---------------------+
               |                                           |
               +--------------------+----------------------+
                                    |
                                    v
+--------------------------------------------------------+
| Request Terminated                                     |
| - Whole Request Memory Pool freed in 1 cycle           |
| - Zero Garbage Collection overhead                     |
+--------------------------------------------------------+
```

---

## 4. Syntax & Directives Reference

| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `js_import module from file` | `http`, `stream` | None | Imports JavaScript source file into the configuration scope. |
| `js_set $variable module.function` | `http`, `stream` | None | Binds an njs function to an NGINX variable, evaluated on demand. |
| `js_content module.function` | `location`, `limit_except` | None | Sets an njs function as the content handler for the location. |
| `js_header_filter module.function` | `http`, `server`, `location` | None | Sets an njs function as an outgoing response header filter. |
| `js_body_filter module.function` | `http`, `server`, `location` | None | Sets an njs function as an outgoing response body filter. |
| `js_engine njs\|qjs` | `http`, `server`, `location` | `njs` | Selects execution engine (`qjs` enables QuickJS ES2023 support). |
| `js_path path` | `http`, `stream` | None | Sets directory search paths for JavaScript modules (`node_modules`). |
| `js_preload_object name from file` | `http`, `stream` | None | Pre-compiles and shares immutable JS objects across all worker VMs. |

---

## 5. Practical Implementation Examples

### Example 1: Dynamic Routing and Header Injection with `js_set`

Calculate an HMAC-signed token or dynamically compute the target upstream server based on client headers.

#### JavaScript Module (`/etc/nginx/njs/routing.js`):
```javascript
function determineUpstream(r) {
    // Inspect incoming user-agent and custom headers
    const clientTier = r.headersIn['X-Client-Tier'];
    const isInternal = r.variables.remote_addr.startsWith('10.');

    if (isInternal || clientTier === 'premium') {
        return 'upstream_fast_cluster';
    }

    return 'upstream_standard_cluster';
}

function generateSignature(r) {
    const timestamp = Date.now().toString();
    const dataToSign = `${r.remoteAddr}:${timestamp}`;
    
    // Use native njs crypto module
    const hmac = require('crypto').createHmac('sha256', 'super-secret-key');
    hmac.update(dataToSign);
    
    return `${timestamp}:${hmac.digest('hex')}`;
}

export default { determineUpstream, generateSignature };
```

#### NGINX Configuration (`/etc/nginx/conf.d/njs_routing.conf`):
```nginx
# Load njs module (if installed dynamically)
# load_module modules/ngx_http_js_module.so;

js_import router from /etc/nginx/njs/routing.js;

# Bind functions to dynamic variables
js_set $target_cluster router.determineUpstream;
js_set $edge_signature router.generateSignature;

upstream upstream_fast_cluster {
    server 10.0.1.10:8000;
}

upstream upstream_standard_cluster {
    server 10.0.1.20:8000;
}

server {
    listen 80;
    server_name api.example.com;

    location / {
        proxy_pass http://$target_cluster;
        proxy_set_header Host $host;
        proxy_set_header X-Edge-Signature $edge_signature;
        proxy_set_header X-Routing-Pool $target_cluster;
    }
}
```

### Example 2: Edge JWT Signature and Claim Validation

Validate JSON Web Tokens (JWT) at the NGINX edge without needing NGINX Plus or Lua.

#### JavaScript Module (`/etc/nginx/njs/auth.js`):
```javascript
const cr = require('crypto');

function validateJwt(r) {
    const authHeader = r.headersIn['Authorization'];
    if (!authHeader || !authHeader.startsWith('Bearer ')) {
        r.return(401, JSON.stringify({ error: 'Missing or malformed Authorization header' }));
        return;
    }

    const token = authHeader.substring(7);
    const parts = token.split('.');
    if (parts.length !== 3) {
        r.return(401, JSON.stringify({ error: 'Invalid JWT structure' }));
        return;
    }

    const [headerB64, payloadB64, signatureB64] = parts;

    // Verify HMAC SHA-256 signature
    const hmac = cr.createHmac('sha256', 'my_jwt_shared_secret_2026');
    hmac.update(`${headerB64}.${payloadB64}`);
    const calculatedSig = hmac.digest('base64url');

    if (calculatedSig !== signatureB64) {
        r.return(403, JSON.stringify({ error: 'Invalid JWT signature' }));
        return;
    }

    // Inspect claims payload
    try {
        const payloadJson = Buffer.from(payloadB64, 'base64url').toString();
        const claims = JSON.parse(payloadJson);

        // Check expiration
        const now = Math.floor(Date.now() / 1000);
        if (claims.exp && claims.exp < now) {
            r.return(401, JSON.stringify({ error: 'Token has expired' }));
            return;
        }

        // Forward authenticated user identity to upstream via internal variables
        r.variables.authenticated_user = claims.sub || 'anonymous';
        r.variables.user_role = claims.role || 'viewer';

        // Internal redirect to protected proxy location
        r.internalRedirect('@protected_backend');
    } catch (e) {
        r.return(400, JSON.stringify({ error: 'Corrupt JWT claims payload' }));
    }
}

export default { validateJwt };
```

#### NGINX Configuration:
```nginx
js_import auth from /etc/nginx/njs/auth.js;

server {
    listen 80;
    server_name secure.example.com;

    location /api/private/ {
        # Execute njs content handler for verification
        js_content auth.validateJwt;
    }

    # Protected named location reached only via r.internalRedirect
    location @protected_backend {
        proxy_pass http://127.0.0.1:4000;
        proxy_set_header Host $host;
        proxy_set_header X-User-Id $authenticated_user;
        proxy_set_header X-User-Role $user_role;
    }
}
```

### Example 3: Dynamic Asynchronous Subrequests with `ngx.fetch()`

Making non-blocking asynchronous HTTP calls to external authentication or telemetry servers before allowing access.

#### JavaScript Module (`/etc/nginx/njs/auth_fetch.js`):
```javascript
async function authorize(r) {
    const token = r.headersIn['X-API-Key'];
    if (!token) {
        r.return(401, JSON.stringify({ error: "Missing API Key" }));
        return;
    }

    try {
        // Asynchronous non-blocking subrequest via ngx.fetch
        const response = await ngx.fetch('http://127.0.0.1:9090/validate', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ apiKey: token })
        });

        if (response.status === 200) {
            const data = await response.json();
            r.variables.tenant_id = data.tenantId;
            r.internalRedirect('@backend');
        } else {
            r.return(403, JSON.stringify({ error: "Unauthorized API Key" }));
        }
    } catch (err) {
        r.error(`Auth subrequest failed: ${err.message}`);
        r.return(502, JSON.stringify({ error: "Auth service unreachable" }));
    }
}

export default { authorize };
```

### Example 4: Response Body Masking and Transformation (`js_body_filter`)

Sanitizing sensitive data (e.g., masking credit card numbers or Social Security Numbers) in outgoing HTTP responses in real time.

#### JavaScript Module (`/etc/nginx/njs/masking.js`):
```javascript
function maskSensitiveData(r, data, flags) {
    // data is a string/buffer chunk; flags.last is true on final chunk
    if (!data) {
        r.sendBuffer(data, flags);
        return;
    }

    // Mask 16-digit credit card patterns: XXXX-XXXX-XXXX-1234
    const masked = data.replace(/\b(?:\d{4}-){3}(\d{4})\b/g, 'XXXX-XXXX-XXXX-$1');

    // Forward modified buffer downstream to client
    r.sendBuffer(masked, flags);
}

export default { maskSensitiveData };
```

#### NGINX Configuration:
```nginx
js_import filter from /etc/nginx/njs/masking.js;

server {
    listen 80;
    server_name portal.example.com;

    location /reports/ {
        proxy_pass http://reporting_upstream;
        proxy_set_header Host $host;

        # Disable upstream compression so njs can inspect plain text
        proxy_set_header Accept-Encoding "";

        # Stream body chunks through njs filter
        js_body_filter filter.maskSensitiveData;
    }
}
```

### Example 5: Modern ES2023 Scripting with QuickJS Engine (`js_engine qjs`)

Enabling QuickJS in NGINX 0.8.x+ to leverage advanced JavaScript language features.

```nginx
# /etc/nginx/conf.d/quickjs_server.conf

# Enable QuickJS execution engine for modern ECMAScript compliance
js_engine qjs;

js_import modern from /etc/nginx/njs/modern_app.js;

server {
    listen 80;
    server_name es2023.example.com;

    location /aggregate {
        js_content modern.aggregateData;
    }
}
```

#### `/etc/nginx/njs/modern_app.js`:
```javascript
async function aggregateData(r) {
    // Modern ES2023: Destructuring, Object.entries, async/await, Promise.allSettled
    const endpoints = [
        'http://127.0.0.1:8001/status',
        'http://127.0.0.1:8002/metrics'
    ];

    const results = await Promise.allSettled(endpoints.map(url => ngx.fetch(url)));

    const summary = results.map((res, idx) => ({
        endpoint: endpoints[idx],
        status: res.status === 'fulfilled' ? res.value.status : 'failed'
    }));

    r.headersOut['Content-Type'] = 'application/json';
    r.return(200, JSON.stringify({ timestamp: Date.now(), results: summary }));
}

export default { aggregateData };
```

---

## 6. Production Best Practices

1. **Use `export default` Syntax**: Organize njs modules using ES module export syntax (`export default { funcA, funcB };`) and import them cleanly using `js_import module from file.js;`.
2. **Never Block the Event Loop**: njs runs directly on the single-threaded NGINX worker event loop. Never write synchronous infinite loops, long CPU-heavy calculations, or synchronous filesystem reads.
3. **Use `ngx.fetch()` for Network I/O**: When calling external services from njs, always use `ngx.fetch()`. It integrates asynchronously with the NGINX event loop without blocking other client connections.
4. **Disable Upstream Compression for Body Filters**: If using `js_body_filter` to inspect or rewrite response content, you must ensure the upstream response is uncompressed by clearing the header: `proxy_set_header Accept-Encoding "";`. If the response arrives as compressed Gzip bytes, the string regex replacer will fail or corrupt binary data.
5. **Always Handle JSON Parse Exceptions**: Never write bare `JSON.parse(data)` in njs handlers. Malformed client input will throw an uncaught exception, resulting in an unformatted HTTP 500 Internal Server Error. Always wrap in `try ... catch`.
6. **Use `js_engine qjs;` for Modern Syntax**: If your njs version is 0.8.0 or newer and you require ES2020+ features (such as `Promise.allSettled`, optional chaining `?.`, or nullish coalescing `??`), explicitly declare `js_engine qjs;`.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Using Node.js standard libraries (`fs`, `net`, `http`)** | njs is NOT Node.js; standard Node npm packages that depend on OS sockets or the V8 C++ runtime fail to import. | Use njs built-in modules (`crypto`, `fs`, `buffer`) or standalone browser-compatible utility scripts. |
| **CPU-blocking algorithms in njs** | Performing heavy PBKDF2 hashing or complex image processing in njs blocks the worker process, freezing all thousands of concurrent connections. | Offload heavy computation to backend workers or use asynchronous subrequests. |
| **Using `js_body_filter` on compressed responses** | Upstream sends Gzip/Brotli compressed bytes; njs attempts string replacements on raw binary, corrupting the stream. | Add `proxy_set_header Accept-Encoding "";` so upstream returns uncompressed plain text. |
| **Global variable mutation across requests** | Attempting to share state by writing to global variables (`globalThis.counter++`) fails unpredictably because NGINX runs multiple worker processes with separate heaps. | Store shared state in Redis, Memcached, or NGINX shared memory zones (`ngx.shared`). |
| **Missing `export default`** | Functions declared without export cannot be resolved by `js_import`, causing `nginx -t` to fail with `function not found`. | Always export an object containing all callable functions. |
| **Calling `r.return()` inside `js_set`** | `js_set` is strictly a variable getter function and cannot terminate the HTTP request. | Use `js_content` when you need to send an HTTP response or terminate a request. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test NGINX Configuration Syntax with njs Modules
nginx -t

# 2. Test njs JavaScript Code Interactively via the njs CLI
# (Install njs package: apt-get install njs)
njs -c 'console.log(crypto.createHash("sha256").update("test").digest("hex"))'

# 3. Test njs Content Handler Output
curl -i http://localhost/aggregate

# 4. Inspect njs Errors in NGINX Error Log
tail -f /var/log/nginx/error.log | grep -i "js:"

# 5. Measure Latency Overhead of njs Execution
# Compare TTFB of plain proxy_pass vs njs-filtered proxy_pass:
curl -o /dev/null -s -w 'TTFB: %{time_starttransfer}s Total: %{time_total}s\n' http://localhost/reports/
```

### Resource Sizing and Memory Metrics
- Memory per njs VM: $\approx 10\text{KB}$ to $30\text{KB}$.
- Execution overhead: A typical header inspection or variable calculation via njs adds less than **0.05 milliseconds** of latency per request.

---

## 9. Architectural Comparison Table

| Feature / Trait | NGINX JavaScript (njs) | OpenResty (Lua) | Native C Modules |
| :--- | :--- | :--- | :--- |
| **Language** | Modern JavaScript (ES2023 with QuickJS) | Lua / LuaJIT | C |
| **Vendor Support** | Official (F5 / NGINX Core) | Third-party / Community | Custom / NGINX Core |
| **Installation** | Standard APT/RPM package | Requires custom OpenResty binary | Requires compiling from source |
| **VM Architecture** | Ephemeral per-request VM | Long-lived Lua state | No VM (Native machine code) |
| **Garbage Collection** | **None** (Pool freed on request end) | Incremental GC (Can pause) | Manual `malloc`/`free` |
| **Memory Footprint** | Extremely Low (~10-30KB) | Low (~2MB per worker) | Minimal |
| **Ecosystem Packages** | Zero npm native C bindings | Rich LuaRocks ecosystem | None |
| **Best For** | JWT validation, dynamic routing, edge filtering | Full web frameworks at the edge | Maximum packet throughput |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: What is the architectural difference between the njs execution model and the Node.js execution model?</strong></summary>

**Answer:**
- **Node.js**: Runs a persistent V8 JavaScript engine. It initializes a persistent global heap, an event loop, and an active garbage collector. Node.js handles multiple requests concurrently within the same long-lived runtime, which makes it vulnerable to memory leaks and GC latency spikes.
- **njs**: Designed specifically for the NGINX event-driven architecture. It uses an **ephemeral per-request VM model**. When a request arrives, NGINX creates a lightweight VM instance mapped to the request's memory pool. All memory allocated by JavaScript objects is held in that pool. When the HTTP request completes, NGINX destroys the pool in a single CPU cycle. There is no background garbage collector and zero possibility of cross-request memory contamination.
</details>

<details>
<summary><strong>Q2: What is the role of QuickJS in modern njs releases?</strong></summary>

**Answer:**
In earlier versions of njs, the NGINX team maintained a custom, minimal ECMAScript 5.1 parser. While fast, it lacked modern JavaScript features (such as classes, `async`/`await`, Promises, destructuring, and arrow functions).

Starting in njs 0.8.x, NGINX integrated Fabrice Bellard's **QuickJS** engine alongside the native njs engine. By configuring `js_engine qjs;`, engineers can execute modern ES2023 code natively inside NGINX, unlocking standard modern JavaScript syntax while retaining the high performance and low footprint of NGINX.
</details>

<details>
<summary><strong>Q3: Explain the difference between js_set and js_content.</strong></summary>

**Answer:**
- `js_set $var module.func`: Defines a variable calculation hook. The bound function is evaluated lazily when `$var` is read by an NGINX directive (e.g., `proxy_pass http://$var;` or `add_header X-Custom $var;`). The function must return a string and cannot send an HTTP response or terminate the request.
- `js_content module.func`: Defines a content generation handler for a `location`. The bound function is responsible for producing the response. It has access to methods like `r.return(status, body)` or `r.internalRedirect(uri)` to send the response directly to the client or reroute it.
</details>

<details>
<summary><strong>Q4: Why must you disable upstream compression (Accept-Encoding: "") when using js_body_filter?</strong></summary>

**Answer:**
The `js_body_filter` directive intercepts response body chunks as they stream through the NGINX filter chain. If the upstream server compresses the response (e.g., returning Gzip or Brotli compressed binary bytes), the `data` chunk passed into the JavaScript filter is compressed binary. 

Attempting to run string transformations (like regex masking or JSON parsing) on compressed binary bytes will fail or corrupt the stream. By setting `proxy_set_header Accept-Encoding "";`, NGINX forces the upstream backend to return plain uncompressed text, allowing njs to safely inspect and transform the payload.
</details>

<details>
<summary><strong>Q5: Can you install and use arbitrary npm packages in njs?</strong></summary>

**Answer:**
Only partially. You **cannot** use npm packages that rely on:
1. Native Node.js C++ bindings.
2. Node.js built-in core modules like `net`, `http`, `child_process`, or `fs` (unless polyfilled or matched by njs equivalents).
3. Browser-specific DOM APIs (`window`, `document`).

However, you **can** use standalone, pure JavaScript algorithms and utility libraries (such as `lodash`, cryptographic math, or custom validation libraries). These can be bundled into a single file using Webpack, Rollup, or esbuild and imported into NGINX via `js_import`.
</details>

<details>
<summary><strong>Q6: How does ngx.fetch() differ from the standard browser or Node.js fetch()?</strong></summary>

**Answer:**
`ngx.fetch()` implements the Fetch API standard, but is architected internally as an asynchronous subrequest within the NGINX event loop. 

When `await ngx.fetch('http://backend/auth')` is called, NGINX initiates an asynchronous HTTP client socket. It registers the socket with the worker's `epoll` listener and suspends the njs execution context without blocking the worker thread. When the subrequest response arrives, epoll awakens the njs VM and resumes execution of the async function.
</details>

<details>
<summary><strong>Q7: What is the difference between r.internalRedirect() and r.return()?</strong></summary>

**Answer:**
- `r.return(status, [body])`: Terminates request processing and immediately sends the given HTTP status code (e.g., 200, 401, 403) and body string back to the external client.
- `r.internalRedirect(uri)`: Does not send a response to the client. Instead, it alters the request's internal URI pointer and transfers execution to another NGINX location block (such as an `@upstream` named location or `/internal/` path) within the same server.
</details>

<details>
<summary><strong>Q8: How do you handle exceptions in njs to prevent 500 errors?</strong></summary>

**Answer:**
Uncaught exceptions thrown inside an njs handler immediately cause NGINX to abort execution, log a stack trace to `error.log`, and return `500 Internal Server Error` to the client. 

To handle exceptions gracefully, all risky operations (such as `JSON.parse()`, Base64 decoding, regex matching, or `ngx.fetch()`) must be enclosed within standard `try { ... } catch (err) { ... }` blocks. In the `catch` block, you should log the error with `r.error(err.message)` and return a controlled status code (such as `400 Bad Request` or `502 Bad Gateway`) via `r.return()`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Hello World njs Endpoint</strong></summary>

**Goal:** Configure NGINX to return a dynamic JSON greeting displaying the client's IP and current timestamp using `js_content`.

**Solution:**
```bash
# /etc/nginx/njs/hello.js
mkdir -p /etc/nginx/njs
cat << 'EOF' > /etc/nginx/njs/hello.js
function greet(r) {
    r.headersOut['Content-Type'] = 'application/json';
    const payload = {
        message: "Hello from NGINX JavaScript!",
        client_ip: r.remoteAddr,
        timestamp: new Date().toISOString()
    };
    r.return(200, JSON.stringify(payload) + "\n");
}
export default { greet };
EOF
```

```nginx
# /etc/nginx/conf.d/hello.conf
js_import hello from /etc/nginx/njs/hello.js;

server {
    listen 80;
    server_name hello.local;

    location /hello {
        js_content hello.greet;
    }
}
```
Test with `curl http://localhost/hello`.
</details>

<details>
<summary><strong>Exercise 2: Implement Dynamic URL Normalization with `js_set`</strong></summary>

**Goal:** Use njs to convert incoming request paths to lowercase before routing to an upstream server.

**Solution:**
```javascript
// /etc/nginx/njs/normalize.js
function lowerUri(r) {
    return r.uri.toLowerCase();
}
export default { lowerUri };
```

```nginx
js_import norm from /etc/nginx/njs/normalize.js;
js_set $normalized_uri norm.lowerUri;

server {
    listen 80;
    server_name norm.local;

    location / {
        proxy_pass http://127.0.0.1:4000$normalized_uri;
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Build an Edge HMAC Request Validator</strong></summary>

**Goal:** Reject requests lacking a valid HMAC-SHA256 signature calculated over the URI and timestamp header.

**Solution:**
```javascript
// /etc/nginx/njs/hmac.js
const cr = require('crypto');

function checkSig(r) {
    const timestamp = r.headersIn['X-Timestamp'];
    const signature = r.headersIn['X-Signature'];

    if (!timestamp || !signature) {
        r.return(403, "Missing signature headers\n");
        return;
    }

    const expected = cr.createHmac('sha256', 'secret-key')
                       .update(`${r.uri}:${timestamp}`)
                       .digest('hex');

    if (expected !== signature) {
        r.return(403, "Invalid HMAC Signature\n");
        return;
    }

    r.return(200, "HMAC Signature Verified Successfully!\n");
}
export default { checkSig };
```
</details>

<details>
<summary><strong>Exercise 4: Test QuickJS Support with Modern ES2023 Features</strong></summary>

**Goal:** Verify QuickJS integration by writing an njs script that uses optional chaining (`?.`) and nullish coalescing (`??`).

**Solution:**
```nginx
js_engine qjs;
js_import modern from /etc/nginx/njs/modern.js;

server {
    listen 80;
    location /es2023 {
        js_content modern.test;
    }
}
```

```javascript
// /etc/nginx/njs/modern.js
function test(r) {
    const user = r.args?.user ?? "guest";
    r.return(200, `Authenticated as: ${user}\n`);
}
export default { test };
```
</details>

<details>
<summary><strong>Exercise 5: Real-Time Response Body Replacement with `js_body_filter`</strong></summary>

**Goal:** Replace the word `DRAFT` with `RELEASE` in outgoing responses on the fly.

**Solution:**
```javascript
// /etc/nginx/njs/replace.js
function replaceWords(r, data, flags) {
    if (data) {
        data = data.replaceAll("DRAFT", "RELEASE");
    }
    r.sendBuffer(data, flags);
}
export default { replaceWords };
```

```nginx
js_import rep from /etc/nginx/njs/replace.js;

server {
    listen 80;
    location /doc {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Accept-Encoding "";
        js_body_filter rep.replaceWords;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6: Diagnose njs Compilation Failures</strong></summary>

**Goal:** Intentionally introduce a syntax error into an njs module and inspect the NGINX configuration test output.

**Solution:**
1. Insert an unclosed bracket `{` in an njs module.
2. Run `nginx -t`.
3. Observe error output:
   `nginx: [emerg] js_import: ... SyntaxError: Unexpected token in line X`
4. Repair syntax and verify `nginx -t` passes.
</details>

---

## 12. Summary & Quick Reference

```nginx
# NGINX JavaScript (njs) Cheat Sheet

# 1. Enable QuickJS (njs 0.8.x+)
js_engine qjs;

# 2. Import Module
js_import auth from /etc/nginx/njs/auth.js;

# 3. Dynamic Variable Evaluation
js_set $dynamic_user auth.getUser;

# 4. Content Handler
location /api/auth {
    js_content auth.validateToken;
}

# 5. Body Filter
location /stream {
    proxy_pass http://backend;
    proxy_set_header Accept-Encoding ""; # Mandatory!
    js_body_filter auth.maskPayload;
}
```

```javascript
// Standard njs Module Structure
function handler(r) {
    // r.headersIn, r.headersOut, r.args, r.variables
    r.headersOut['Content-Type'] = 'text/plain';
    r.return(200, "OK");
}

export default { handler };
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX JavaScript (njs) Documentation](https://nginx.org/en/docs/njs/)
- [njs Reference (HTTP Request Object)](https://nginx.org/en/docs/njs/reference.html)
- [njs GitHub Repository & Examples](https://github.com/nginx/njs)
- [QuickJS Javascript Engine Specification](https://bellard.org/quickjs/)

---

[← Previous: ACME & Certbot Automation](11-acme-and-certbot-automation.md) | [Module Overview](README.md) | [Next Module: Traffic Management, Caching & Upstreams →](../03-traffic-management/01-caching.md)
