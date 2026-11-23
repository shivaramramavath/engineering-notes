# The Request and Response Interfaces

> The `Request` and `Response` interfaces represent the fundamental primitives of the WHATWG Fetch specification, powering Service Worker caching, synthetic responses, and isomorphic HTTP pipelines.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [The Fetch API](./02_fetch.md), [Headers & CORS](./03_headers-and-cors.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **Standard** | WHATWG Fetch Living Standard / Baseline Widely Available |

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

When making network requests, developers typically pass simple strings and options objects to `fetch('https://api.com', { method: 'POST' })`. Internally, the browser or Node.js runtime converts these inputs into first-class **`Request`** and **`Response`** objects.

Understanding these objects as standalone, constructible primitives is essential for modern web architecture:
- **Service Workers**: Intercepting network requests (`event.respondWith()`) and serving cached responses requires instantiating, cloning, and inspecting `Request` and `Response` objects directly.
- **Modern Web Frameworks**: Frameworks like Next.js App Router, Remix, Astro, and edge servers (Cloudflare Workers, Bun) use standard WHATWG `Request` and `Response` objects as their native HTTP request/response handler signatures.
- **Synthetic Testing & Mocking**: Creating realistic mock network pipelines without spinning up live HTTP test servers.

---

## 2. Core Concepts

### Autonomous Constructibility
Both `Request` and `Response` are fully constructible via `new Request()` and `new Response()`. You do not need an active network connection to create them:

```javascript
// Creating a synthetic response in memory
const mockResponse = new Response(JSON.stringify({ status: 'ok' }), {
  status: 200,
  headers: { 'Content-Type': 'application/json' },
});
```

### Modern Static Factory Methods
Modern specifications provide high-performance static constructors:
- **`Response.json(data, init)`**: Creates a JSON response with automatic `Content-Type: application/json` headers and serialization (ES2022+ / Baseline Widely Available).
- **`Response.error()`**: Creates a synthetic network error response (used in Service Workers to simulate network dropouts).
- **`Response.redirect(url, status)`**: Creates a redirect response (status 301, 302, 307, or 308).

```text
               +-------------------------------------------+
               |              new Request()                |
               |  - Method, URL, Headers                   |
               |  - Body: ReadableStream                   |
               |  - Cache mode, Credentials, Signal        |
               +-------------------------------------------+
                                     │
                                     ▼
                                [ fetch() ]
                                     │
                                     ▼
               +-------------------------------------------+
               |              new Response()               |
               |  - Status, StatusText, Headers            |
               |  - Body: ReadableStream                   |
               |  - Methods: .json(), .text(), .blob()     |
               +-------------------------------------------+
```

---

## 3. How It Works Internally

### Request / Response Cloning and Stream Teeing

When code calls `request.clone()` or `response.clone()`:

```
 Original Response Object:
   +-------------------------------------------------------+
   | Status: 200 | Headers: ... | Body: ReadableStream [A] |
   +-------------------------------------------------------+
                              │
               response.clone() invoked
                              │
                              ▼
   Internal Stream "Tee" Operation:
   Underlying byte chunks arriving from network socket are
   duplicated in memory into two independent ring buffers.
                              │
             ┌────────────────┴────────────────┐
             ▼                                 ▼
   Original Response                Cloned Response
   Body: ReadableStream [Branch 1]  Body: ReadableStream [Branch 2]
```

Both streams can now be read independently. However, cloning increases memory consumption because chunks must be retained in memory until *both* streams consume them.

---

## 4. Syntax & API Reference

### 1. The `Request` Constructor

```javascript
// Signature: new Request(input [, init])
const req = new Request('https://api.example.com/items', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ item: 'Widget' }),
  mode: 'cors',
  credentials: 'include',
  cache: 'no-cache', // 'default' | 'no-store' | 'reload' | 'no-cache' | 'force-cache'
  redirect: 'follow',
});

console.log(req.method); // 'POST'
console.log(req.url);    // 'https://api.example.com/items'
```

### 2. The `Response` Constructor & Static Helpers

```javascript
// 1. Manual constructor
const res = new Response('Hello World', {
  status: 200,
  statusText: 'OK',
  headers: { 'Content-Type': 'text/plain' },
});

// 2. High-performance JSON helper
const jsonRes = Response.json({ user: 'Ada' }, {
  status: 201,
  headers: { 'X-Custom-Header': 'val' },
});

// 3. Error and Redirect helpers
const errRes = Response.error();
const redirRes = Response.redirect('https://example.com/login', 302);
```

---

## 5. Practical Examples

### Example 1: Service Worker Offline Cache Interceptor
```javascript
// Inside a Service Worker (sw.js)
self.addEventListener('fetch', (event) => {
  event.respondWith(
    (async () => {
      // 1. Check local Cache Storage for matching Request
      const cachedResponse = await caches.match(event.request);
      if (cachedResponse) {
        return cachedResponse; // Return cached Response directly!
      }

      try {
        // 2. Fall back to network
        const networkResponse = await fetch(event.request);

        // 3. Cache must receive a CLONE because bodies are single-use!
        const cache = await caches.open('v1-runtime-cache');
        cache.put(event.request, networkResponse.clone());

        return networkResponse;
      } catch (err) {
        // 4. Offline fallback response
        return new Response('<h1>Offline - Network Unavailable</h1>', {
          status: 503,
          headers: { 'Content-Type': 'text/html' },
        });
      }
    })()
  );
});
```

### Example 2: Edge API Route Handler (Next.js / Cloudflare Workers Pattern)
```javascript
// Standard isomorphic web standard handler signature:
// export default function handler(request: Request): Promise<Response>
export async function handleRequest(request) {
  if (request.method !== 'POST') {
    return new Response('Method Not Allowed', { status: 405 });
  }

  const contentType = request.headers.get('content-type') || '';
  if (!contentType.includes('application/json')) {
    return Response.json(
      { error: 'Expected application/json' },
      { status: 415 }
    );
  }

  const payload = await request.json();
  const processedData = {
    received: payload,
    timestamp: Date.now(),
    server: 'Edge-Region-US',
  };

  return Response.json(processedData, {
    status: 200,
    headers: {
      'Cache-Control': 'public, max-age=300',
    },
  });
}
```

### Example 3: Reusable Request Template Factory
```javascript
class ApiRequestBuilder {
  #baseUrl;
  #authToken;

  constructor(baseUrl, token) {
    this.#baseUrl = baseUrl;
    this.#authToken = token;
  }

  createRequest(path, method = 'GET', body = null) {
    const url = new URL(path, this.#baseUrl);
    const headers = new Headers({
      'Authorization': `Bearer ${this.#authToken}`,
      'Accept': 'application/json',
    });

    const init = { method, headers };

    if (body) {
      headers.set('Content-Type', 'application/json');
      init.body = JSON.stringify(body);
    }

    return new Request(url, init);
  }
}

// Usage
const builder = new ApiRequestBuilder('https://api.crm.com', 'secret_jwt');
const userReq = builder.createRequest('/users/10', 'GET');
const updateReq = builder.createRequest('/users/10', 'PUT', { role: 'admin' });

const userRes = await fetch(userReq);
```

### Example 4: Mocking API Responses in Unit Tests
```javascript
function mockFetch(handler) {
  const originalFetch = globalThis.fetch;

  globalThis.fetch = async (input, init) => {
    const request = input instanceof Request ? input : new Request(input, init);
    const mockedResponse = await handler(request);
    if (mockedResponse) return mockedResponse;
    return originalFetch(input, init);
  };

  return () => {
    globalThis.fetch = originalFetch; // Restore original
  };
}

// In test file:
const restore = mockFetch((req) => {
  if (req.url.endsWith('/api/status')) {
    return Response.json({ healthy: true, uptime: 99.9 });
  }
});
```

### Example 5: Streaming Synthetic Response
```javascript
function createStreamingTimeResponse() {
  const stream = new ReadableStream({
    start(controller) {
      let count = 0;
      const interval = setInterval(() => {
        const text = `Tick ${++count}: ${new Date().toISOString()}\n`;
        controller.enqueue(new TextEncoder().encode(text));

        if (count >= 5) {
          clearInterval(interval);
          controller.close();
        }
      }, 500);
    },
  });

  return new Response(stream, {
    headers: { 'Content-Type': 'text/plain; charset=utf-8' },
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`GET` or `HEAD` Requests with Body**: Passing a `body` to a `Request` with method `GET` or `HEAD` throws a fatal `TypeError: Request with GET/HEAD method cannot have body`.
2. **Cloning Consumed Streams Fails**: Calling `.clone()` on a `Request` or `Response` whose body has already been consumed (`bodyUsed === true`) throws `TypeError: Cannot clone a disturbed or locked Response`.
3. **Immutable Headers Guard**: Certain `Request` and `Response` objects have immutable header guards (e.g. responses returned by `fetch()` inside a Service Worker). Calling `response.headers.set()` on them throws a `TypeError`. You must construct a new `Response` to alter headers.
4. **Body Re-use Across Fetch Calls**: Once a `Request` instance with a stream body is passed to `fetch()`, its body is locked. Passing the same `Request` object into a second `fetch()` call will fail.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Passing Body to `GET` Request** | `new Request(url, { method: 'GET', body: json })` | Pass query parameters in URL | Spec explicitly forbids bodies on GET/HEAD requests. |
| **Manual JSON Response Construction**| `new Response(JSON.stringify(d), { headers: ... })` | Use `Response.json(d)` | Manually specifying Content-Type is verbose and error-prone. |
| **Cloning After `.json()`** | `await res.json(); const c = res.clone();` | Call `res.clone()` *before* `.json()` | Once a stream is disturbed/consumed, it cannot be cloned. |
| **Mutating Immutable Headers** | `res.headers.set('X-Key', 'v')` on fetch result | Create new `Response(res.body, res)` | Network fetch responses have immutable header guards. |
| **Reusing Request Object** | Calling `fetch(req)` twice in a row | Clone request first: `fetch(req.clone())` | The request body stream is drained upon first invocation. |
| **Ignoring Redirect Status Rules** | `Response.redirect(url, 200)` | Must use 301, 302, 303, 307, or 308 | Status must be a valid HTTP redirect code. |

---

## 8. Performance Characteristics

- **Memory Cost of Response Cloning**: `response.clone()` duplicates stream chunks in memory. If a 100MB video response is cloned, both branches buffer incoming chunks until consumed, temporarily doubling memory usage.
- **`Response.json()` Optimization**: V8 optimizes `Response.json()` by passing the native C++ string buffer directly to the JSON parser without intermediate JavaScript string object allocations.

---

## 9. Engine Internals

1. **Chromium `blink::FetchRequestData`**: In Blink, `Request` is a C++ wrapper around `blink::FetchRequestData`, storing URL, method, and internal stream pipeline pipes.
2. **Body Locking**: The WHATWG Streams standard defines the concept of **Disturbed** and **Locked**. When `response.json()` is called, it acquires a `ReadableStreamDefaultReader`, setting the internal `[[disturbed]]` slot to true.

---

## 10. Best Practices & Design Guidelines

1. **Use `Response.json()` for API Handlers**: Adopt `Response.json(data)` for clean, standardized JSON serialization across edge and serverless environments.
2. **Clone Early in Service Workers**: If storing a network response in Cache Storage while returning it to the page, invoke `response.clone()` immediately upon receipt.
3. **Use the `Request` Object as an Entity**: Encapsulate complex authentication and headers logic inside a `Request` factory rather than scattering options objects across your codebase.
4. **Inspect `bodyUsed` Before Reading**: Guard conditional body reads with `if (!response.bodyUsed)`.

---

## 11. Real-World Case Study

### Progressive Service Worker Network Interceptor with Stale-While-Revalidate

An offline-capable Progressive Web App serves cached catalog data instantly while fetching fresh data in the background and updating Cache Storage.

```javascript
// sw.js - Stale-While-Revalidate Pattern
self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);

  // Only apply strategy to API catalog queries
  if (url.pathname.startsWith('/api/catalog')) {
    event.respondWith(
      (async () => {
        const cache = await caches.open('catalog-cache-v1');

        // 1. Check cache first
        const cachedResponse = await cache.match(event.request);

        // 2. Initiate background network fetch
        const networkFetchPromise = fetch(event.request)
          .then((networkResponse) => {
            if (networkResponse.ok) {
              // Clone and update cache asynchronously in background
              cache.put(event.request, networkResponse.clone());
            }
            return networkResponse;
          })
          .catch((err) => {
            console.warn('Background fetch failed (device offline):', err);
          });

        // 3. Return cached response immediately if available; otherwise wait for network
        return cachedResponse || (await networkFetchPromise);
      })()
    );
  }
});
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is the advantage of using <code>Response.json(data)</code> over <code>new Response(JSON.stringify(data))</code>?</strong></summary>

`Response.json(data)` automatically serializes the data to a JSON string and sets the `Content-Type: application/json` header, eliminating boilerplate code and reducing memory allocations.
</details>

<details>
<summary><strong>2. Can a <code>Request</code> with a method of <code>GET</code> or <code>HEAD</code> contain a body?</strong></summary>

No. According to the WHATWG Fetch specification, constructing a `Request` with method `GET` or `HEAD` that includes a `body` will throw a `TypeError`.
</details>

<details>
<summary><strong>3. Why is calling <code>response.clone()</code> strictly required in Service Workers before caching?</strong></summary>

Because a `Response` body is a `ReadableStream` that can only be read once. If you pass the response to `cache.put(request, response)` without cloning, the cache drains the stream, making it impossible to return the response body to the waiting web page.
</details>

<details>
<summary><strong>4. What does <code>response.bodyUsed</code> indicate?</strong></summary>

It is a boolean property indicating whether the response body stream has already been read or locked. If `bodyUsed` is `true`, calling `.json()`, `.text()`, or `.clone()` will throw a `TypeError`.
</details>

<details>
<summary><strong>5. What are the allowed HTTP status codes for <code>Response.redirect(url, status)</code>?</strong></summary>

The status must be an HTTP redirect code: `301` (Moved Permanently), `302` (Found), `303` (See Other), `307` (Temporary Redirect), or `308` (Permanent Redirect).
</details>

<details>
<summary><strong>6. How can you modify headers on a Response returned by <code>fetch()</code>?</strong></summary>

Responses returned by `fetch()` have immutable header guards. To modify them, you must create a new `Response` object, passing the original body stream and a new headers configuration:
```javascript
const newRes = new Response(res.body, { ...res, headers: newHeaders });
```
</details>

<details>
<summary><strong>7. What does the <code>cache: 'no-store'</code> option on a <code>Request</code> do?</strong></summary>

It instructs the browser to bypass its local HTTP cache completely for the request and guarantees that the browser does not update its cache with the resulting response.
</details>

<details>
<summary><strong>8. What is the return value of <code>Response.error()</code>?</strong></summary>

It returns a synthetic network error response with a status of `0`, an empty body, and empty headers. It is used in Service Workers to simulate complete network dropouts or block unauthorized requests.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an Immutable Request Header Appender
Write a function `withAuthToken(request, token)` that takes an existing `Request` object and returns a new cloned `Request` containing an updated `Authorization: Bearer <token>` header, preserving the original method, body, and options.

<details>
<summary>View Solution</summary>

```javascript
function withAuthToken(request, token) {
  const headers = new Headers(request.headers);
  headers.set('Authorization', `Bearer ${token}`);

  // When cloning requests with bodies, we must check if a body exists
  const hasBody = !['GET', 'HEAD'].includes(request.method);

  const init = {
    method: request.method,
    headers,
    mode: request.mode,
    credentials: request.credentials,
    cache: request.cache,
    redirect: request.redirect,
  };

  if (hasBody) {
    init.body = request.clone().body;
    // In Node/Chromium duplex streaming:
    init.duplex = 'half';
  }

  return new Request(request.url, init);
}
```
</details>

### Challenge 2: Implement an In-Memory Response Caching Layer
Create a class `ResponseCache` that stores `Response` objects in memory keyed by URL string and returns cloned copies to callers, ensuring that cached responses can be read multiple times.

<details>
<summary>View Solution</summary>

```javascript
class ResponseCache {
  #cache = new Map();

  async set(url, response) {
    // Clone and buffer body as ArrayBuffer so it can be re-instantiated cleanly
    const cloned = response.clone();
    const buffer = await cloned.arrayBuffer();
    const headers = {};
    cloned.headers.forEach((val, key) => { headers[key] = val; });

    this.#cache.set(url, {
      buffer,
      status: cloned.status,
      statusText: cloned.statusText,
      headers,
    });
  }

  get(url) {
    const entry = this.#cache.get(url);
    if (!entry) return null;

    // Instantiate fresh Response instance each time
    return new Response(entry.buffer.slice(0), {
      status: entry.status,
      statusText: entry.statusText,
      headers: entry.headers,
    });
  }
}
```
</details>
