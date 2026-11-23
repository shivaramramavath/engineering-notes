# HTTP Fundamentals for JavaScript Developers

> Modern client and server JavaScript engineering requires understanding HTTP protocols (HTTP/1.1, HTTP/2, HTTP/3), idempotency contracts, status codes, and connection lifecycles.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [Sync vs Async Execution](../11_asynchronous-javascript/01_sync-vs-async.md), [Promises](../11_asynchronous-javascript/03_promises.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **Standard** | IETF RFC 9110 (HTTP Semantics) / RFC 9112 (HTTP/1.1) / RFC 9113 (HTTP/2) / RFC 9114 (HTTP/3) |

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

Hypertext Transfer Protocol (HTTP) is the application-layer foundation of data communication across the World Wide Web. Whether a browser fetches an HTML document via `fetch()`, an SPA connects to an API via REST or GraphQL, or a Node.js microservice communicates with an upstream payment gateway, HTTP governs how requests are formed, transported, and acknowledged.

For JavaScript engineers, treating network calls as simple black-box function calls (`await fetch(url)`) leads to catastrophic production failures: cascading retries crashing recovering databases, unhandled network timeouts locking worker threads, and caching errors serving stale customer data. Understanding HTTP semantics—idempotency, status codes, connection pooling, and protocol transport differences—is vital for building reliable, performant distributed software.

---

## 2. Core Concepts

### Evolution of HTTP Transport Protocols

| Protocol | Transport Layer | Multiplexing | Header Compression | Head-of-Line (HoL) Blocking |
| :--- | :--- | :--- | :--- | :--- |
| **HTTP/1.1** | TCP | No (Pipelining failed) | Plaintext (No compression) | **Yes** (At application & TCP layers) |
| **HTTP/2** | TCP | Yes (Binary streams) | HPACK (Stateful table) | **Yes** (At TCP packet loss level) |
| **HTTP/3** | UDP (via QUIC) | Yes (Independent streams) | QPACK (Out-of-order table) | **No** (Zero TCP HoL blocking!) |

```text
HTTP/1.1 (6 Parallel TCP Connections per Origin):
 Connection 1: [ Req 1 ──> Res 1 ][ Req 2 ──> Res 2 ]
 Connection 2: [ Req 3 ──> Res 3 ]
 (Max 6 concurrent requests; remaining requests queue in browser!)

HTTP/2 & HTTP/3 (Single Multiplexed Connection):
 Single Socket: ├── Stream 1 (Req/Res 1) ──┤
                ├── Stream 3 (Req/Res 2) ──┤ (Binary interleaved frames)
                └── Stream 5 (Req/Res 3) ──┤
```

### Method Safety & Idempotency
- **Safe Methods**: Operations that do not alter server state (`GET`, `HEAD`, `OPTIONS`). They can be called repeatedly and pre-fetched or cached by proxies.
- **Idempotent Methods**: Operations where making $N$ identical requests produces the exact same server side-effect state as making $1$ request (`GET`, `PUT`, `DELETE`, `HEAD`, `OPTIONS`).
- **Non-Idempotent Methods**: Making $N$ requests executes the side-effect $N$ times (`POST`, `PATCH`). Retrying a timed-out `POST` request risks charging a credit card twice!

```text
+--------+--------+------------+--------------------------------------------+
| Method | Safe?  | Idempotent?| Primary Semantic Purpose                   |
+--------+--------+------------+--------------------------------------------+
| GET    | YES    | YES        | Retrieve resource representation           |
| POST   | NO     | NO         | Create new resource / process entity       |
| PUT    | NO     | YES        | Completely replace target resource         |
| PATCH  | NO     | NO*        | Partially mutate target resource           |
| DELETE | NO     | YES        | Remove target resource                     |
| HEAD   | YES    | YES        | Retrieve headers only (no response body)   |
+--------+--------+------------+--------------------------------------------+
(* Note: PATCH can be designed to be idempotent, but the standard does not guarantee it.)
```

---

## 3. How It Works Internally

### The Complete HTTP Request / Response Lifecycle

```
 JavaScript invokes: await fetch('https://api.example.com/data')
                                  │
                                  ▼
 1. DNS Resolution: Query OS cache / DNS server for IP address (e.g. 93.184.216.34)
                                  │
                                  ▼
 2. TCP Handshake: SYN -> SYN-ACK -> ACK (1 Round-Trip Time [RTT])
                                  │
                                  ▼
 3. TLS 1.3 Handshake: ClientHello -> ServerHello + Cert + Keys (1 RTT)
                                  │
                                  ▼
 4. HTTP Request Sent: Request-Line + Headers + Optional Body
                                  │
                                  ▼
 5. Server Processing (Time To First Byte - TTFB)
                                  │
                                  ▼
 6. HTTP Response Received: Status-Line + Response Headers + Chunks
                                  │
                                  ▼
 7. Connection Keep-Alive: Socket returned to pool for reuse!
```

---

## 4. Syntax & API Reference

### HTTP Status Code Taxonomy

```javascript
// 2xx Success
200 OK                    // Standard success with body
201 Created               // Resource successfully created (Check Location header)
204 No Content            // Success with intentionally empty body (e.g. DELETE)

// 3xx Redirection
301 Moved Permanently     // Resource permanently relocated (Cachable)
302 Found                 // Temporary redirect
304 Not Modified          // Conditional GET match (ETag/If-None-Match) -> Use local cache!

// 4xx Client Errors
400 Bad Request           // Malformed JSON or validation error
401 Unauthorized          // Missing or invalid authentication token
403 Forbidden             // Authenticated, but lacks required permissions (RBAC)
404 Not Found             // Resource does not exist
409 Conflict              // State conflict (e.g. duplicate unique key)
422 Unprocessable Content // Semantic validation failure (FastAPI / Rails standard)
429 Too Many Requests     // Rate limit exceeded (Check Retry-After header)

// 5xx Server Errors
500 Internal Server Error // Uncaught server exception
502 Bad Gateway           // Upstream reverse proxy / backend unreachable
503 Service Unavailable   // Temporary overload or maintenance (Retryable)
504 Gateway Timeout       // Upstream microservice took too long to reply
```

---

## 5. Practical Examples

### Example 1: Differentiating Status Codes in Fetch
```javascript
async function fetchUserProfile(userId) {
  const response = await fetch(`/api/users/${userId}`);

  // Fetch only rejects on network failures (DNS, offline), NOT on 4xx/5xx errors!
  if (!response.ok) {
    switch (response.status) {
      case 401:
        redirectToLogin();
        throw new Error('Authentication required');
      case 403:
        throw new Error('Permission denied');
      case 404:
        return null; // Gracefully handle non-existent resource
      case 429: {
        const retryAfter = response.headers.get('Retry-After') || '60';
        throw new Error(`Rate limit exceeded. Retry in ${retryAfter} seconds.`);
      }
      default:
        throw new Error(`HTTP Error ${response.status}: ${response.statusText}`);
    }
  }

  // 204 No Content has no JSON body!
  if (response.status === 204) {
    return null;
  }

  return await response.json();
}
```

### Example 2: Safe Retries Using Idempotency Keys (Stripe Pattern)
```javascript
import crypto from 'node:crypto';

// Protect non-idempotent POST requests against duplicate processing
async function chargeCustomer(customerId, amountInCents, idempotencyKey = crypto.randomUUID()) {
  const response = await fetch('/api/v1/charges', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Idempotency-Key': idempotencyKey, // Server caches result by this key
    },
    body: JSON.stringify({ customerId, amount: amountInCents }),
  });

  if (!response.ok) {
    throw new Error(`Payment failed: ${response.status}`);
  }

  return await response.json();
}
```

### Example 3: Conditional Requests via `ETag` and `If-None-Match`
```javascript
let cachedETag = null;
let cachedData = null;

async function fetchWithEtagCaching(url) {
  const headers = {};
  if (cachedETag) {
    headers['If-None-Match'] = cachedETag;
  }

  const response = await fetch(url, { headers });

  if (response.status === 304) {
    console.log('HTTP 304 Not Modified: Reusing local memory cache (0 bytes transferred!)');
    return cachedData;
  }

  if (response.ok) {
    cachedETag = response.headers.get('ETag');
    cachedData = await response.json();
    return cachedData;
  }

  throw new Error(`Request failed with status ${response.status}`);
}
```

### Example 4: Node.js Custom HTTP Agent Connection Pooling
```javascript
import http from 'node:http';
import https from 'node:https';

// Configure persistent Keep-Alive connection pool in Node.js
const httpsAgent = new https.Agent({
  keepAlive: true,              // Reuse TCP sockets across requests
  maxSockets: 50,               // Maximum concurrent sockets per host
  maxFreeSockets: 10,           // Sockets kept open in free pool
  timeout: 60_000,              // Socket timeout
});

// Used in native fetch (Node 18+) or libraries like Axios
export function makeOptimizedRequest(url) {
  return fetch(url, { agent: httpsAgent });
}
```

### Example 5: Handling Chunked Transfer Streaming
```javascript
async function consumeStreamingEndpoint(url, onChunk) {
  const response = await fetch(url);
  if (!response.ok) throw new Error(`HTTP ${response.status}`);

  const reader = response.body.getReader();
  const decoder = new TextDecoder('utf-8');

  while (true) {
    const { done, value } = await reader.read();
    if (done) break;

    const textChunk = decoder.decode(value, { stream: true });
    onChunk(textChunk);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`fetch()` Does Not Reject on 4xx / 5xx**: The native `fetch()` promise **only rejects if there is a network failure** (such as DNS failure, connection refused, or CORS violation). An HTTP 500 or 404 resolves successfully; developers must manually inspect `response.ok` or `response.status`.
2. **HTTP/1.1 6-Connection Browser Limit**: Browsers restrict concurrent HTTP/1.1 connections to **6 per domain**. If an application fires 20 parallel `fetch()` calls over HTTP/1.1, 14 requests sit stalled in browser queues until earlier connections finish. HTTP/2 and HTTP/3 multiplexing eliminate this limitation.
3. **`204 No Content` JSON Parsing Trap**: Calling `response.json()` on an HTTP 204 response throws `SyntaxError: Unexpected end of JSON input`. Always check `if (response.status === 204)` before parsing.
4. **Retrying Non-Idempotent POST Requests**: If a client sends a `POST /orders` request and the network drops before receiving the response, the server may have successfully processed the order. Retrying naively creates duplicate orders. Use **Idempotency Keys**.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Assuming `fetch` Rejects on 500** | Relying on `try/catch` without checking `res.ok` | Check `if (!response.ok) throw ...` | `fetch` resolves on all HTTP status codes (200, 404, 500). |
| **Retrying `POST` Unconditionally** | Retrying failed checkout `POST` | Use `Idempotency-Key` or retry only `GET`/`PUT` | Causes duplicate charges or duplicate resource creation. |
| **Calling `.json()` on 204** | `const data = await res.json()` | Check `if (res.status === 204) return null;` | Throws `SyntaxError: Unexpected end of JSON input`. |
| **Disabling Keep-Alive in Node** | Instantiating new `Agent({ keepAlive: false })` | Enable persistent Keep-Alive sockets | Incurs 2 RTT (TCP + TLS) handshake penalties on every request. |
| **Ignoring `Retry-After` Header** | Retrying HTTP 429 immediately in tight loop | Parse `Retry-After` header and delay | Violates rate limits, leading to IP bans. |
| **Using `GET` for Mutating Operations** | `GET /api/user/delete?id=5` | Use `DELETE /api/user/5` | Browsers, proxies, and crawlers pre-fetch `GET` requests, deleting data! |

---

## 8. Performance Characteristics

- **Connection Setup Overhead**: Establishing a cold HTTPS connection over TCP requires **2 RTTs** (1 for TCP SYN, 1 for TLS 1.3). Over a 100ms latency mobile connection, connection setup takes 200ms before application data can be sent.
- **HTTP/2 Multiplexing Benefits**: By interleaving multiple streams over a single TCP connection, HTTP/2 eliminates the 6-connection domain sharding bottleneck and header repetition via HPACK compression.
- **HTTP/3 QUIC Advantage**: Because HTTP/3 runs over UDP with independent QUIC streams, losing a single packet on Stream A does **not** stall Stream B, completely resolving TCP Head-of-Line blocking.

---

## 9. Engine Internals

1. **Chromium Network Service**: In Chromium, all networking is handled by an isolated, multi-threaded C++ process (`NetworkService`). It manages the socket pool, HTTP/2 session frames, disk cache, and TLS crypto handshakes.
2. **Socket Pooling in Node.js**: Node's `http.globalAgent` maintains a pool of persistent `net.Socket` instances. When a request ends, the socket is placed in a free list and kept open for up to `keepAliveMsecs`.

---

## 10. Best Practices & Design Guidelines

1. **Always Check `response.ok`**: Treat `if (!response.ok)` as mandatory boilerplate after every `fetch()`.
2. **Implement Idempotency Keys on Payment/Creation Endpoints**: Prevent duplicate processing of retried network calls.
3. **Reuse Keep-Alive Sockets in Backend Services**: In Node.js, always configure a custom `Agent` with `keepAlive: true` for upstream microservice communication.
4. **Leverage `ETag` for Bandwidth Optimization**: Return `304 Not Modified` for unchanged resources to eliminate payload downloads.

---

## 11. Real-World Case Study

### Resilient Payment Processing Client with Backoff and Idempotency

An e-commerce checkout service communicates with an external payment gateway. Network glitches must be retried with exponential backoff while guaranteeing that customers are never double-billed.

```javascript
class PaymentClient {
  #baseUrl;
  #apiKey;

  constructor(baseUrl, apiKey) {
    this.#baseUrl = baseUrl;
    this.#apiKey = apiKey;
  }

  async processPayment(paymentPayload, maxRetries = 3) {
    // Generate unique idempotency key for this specific transaction
    const idempotencyKey = paymentPayload.transactionId || crypto.randomUUID();

    for (let attempt = 0; attempt <= maxRetries; attempt++) {
      try {
        const response = await fetch(`${this.#baseUrl}/v1/charges`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${this.#apiKey}`,
            'Idempotency-Key': idempotencyKey, // Safe retry guarantee!
          },
          body: JSON.stringify(paymentPayload),
        });

        // If rate limited, respect Retry-After
        if (response.status === 429) {
          const retryAfterSec = parseInt(response.headers.get('Retry-After') || '2', 10);
          console.warn(`Rate limited. Waiting ${retryAfterSec}s...`);
          await new Promise(r => setTimeout(r, retryAfterSec * 1000));
          continue;
        }

        // Do not retry 4xx client errors (e.g. 400 Invalid Card, 402 Card Declined)
        if (response.status >= 400 && response.status < 500) {
          const errorDetails = await response.json();
          throw new Error(`Client Payment Error [${response.status}]: ${errorDetails.message}`);
        }

        // Retry 5xx server errors
        if (!response.ok) {
          throw new Error(`Gateway Error: ${response.status}`);
        }

        return await response.json();
      } catch (err) {
        if (attempt === maxRetries || err.message.startsWith('Client Payment Error')) {
          throw err;
        }

        const backoffDelay = (2 ** attempt) * 500 + Math.random() * 200;
        console.warn(`Payment attempt ${attempt + 1} failed. Retrying in ${Math.round(backoffDelay)}ms...`);
        await new Promise(r => setTimeout(r, backoffDelay));
      }
    }
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. Why doesn't <code>fetch()</code> reject its promise on an HTTP 404 or 500 status code?</strong></summary>

According to the WHATWG Fetch standard, the `fetch()` promise only rejects on fatal network errors—such as DNS resolution failures, connection timeouts, refused TCP connections, or CORS violations. Receiving an HTTP 404 or 500 response from an HTTP server represents a successful HTTP request/response exchange, so the promise resolves with `response.ok = false`.
</details>

<details>
<summary><strong>2. What is the difference between an idempotent HTTP method and a safe HTTP method?</strong></summary>

A **safe** method does not modify server state at all (e.g. `GET`, `HEAD`, `OPTIONS`). An **idempotent** method may modify server state, but making $N$ identical requests produces the exact same server state as making 1 request (e.g. `PUT`, `DELETE`). All safe methods are idempotent, but not all idempotent methods are safe.
</details>

<details>
<summary><strong>3. What was the Head-of-Line (HoL) blocking problem in HTTP/1.1, and how did HTTP/2 solve it?</strong></summary>

In HTTP/1.1, a single TCP connection could only process one request/response exchange at a time. If the first request was slow, all subsequent requests queued behind it. HTTP/2 introduced **binary frame multiplexing**, interleaving multiple bidirectional streams over a single TCP connection so requests execute concurrently.
</details>

<details>
<summary><strong>4. How does HTTP/3 improve upon HTTP/2?</strong></summary>

HTTP/2 multiplexes streams over a single TCP connection. If a single TCP packet is dropped, TCP pauses *all* streams on that connection until the missing packet is retransmitted (TCP Head-of-Line blocking). HTTP/3 runs over **QUIC (UDP)** where each stream is tracked independently; a dropped packet on Stream 1 does not pause Stream 2.
</details>

<details>
<summary><strong>5. What is the purpose of an <code>Idempotency-Key</code> header?</strong></summary>

It allows clients to safely retry non-idempotent operations (like `POST /payments`) after network timeouts. The server caches the result of the first request by this unique key. If a retry arrives with the same key, the server returns the cached result without executing the charge a second time.
</details>

<details>
<summary><strong>6. What is the difference between HTTP status 401 Unauthorized and 403 Forbidden?</strong></summary>

`401 Unauthorized` means authentication is missing or invalid (the server does not know who you are). `403 Forbidden` means the client is authenticated, but lacks sufficient permissions to access the requested resource.
</details>

<details>
<summary><strong>7. How does HTTP caching work with <code>ETag</code> and <code>If-None-Match</code>?</strong></summary>

The server sends an `ETag: "hash123"` header representing the resource version. On subsequent requests, the client sends `If-None-Match: "hash123"`. If the resource hasn't changed, the server returns `304 Not Modified` with an empty body, saving bandwidth.
</details>

<details>
<summary><strong>8. Why is connection Keep-Alive essential for Node.js backend performance?</strong></summary>

Without Keep-Alive, Node closes the TCP socket after each request, incurring a 3-way TCP handshake and TLS cryptographic negotiation on every subsequent request. Keep-Alive keeps sockets open in a connection pool, reducing request latency by hundreds of milliseconds.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an HTTP Response Classifier
Write a function `classifyResponse(response)` that inspects a Fetch `Response` object and returns a standardized diagnostic object: `{ success: boolean, retryable: boolean, category: string, message: string }`.

<details>
<summary>View Solution</summary>

```javascript
function classifyResponse(response) {
  const status = response.status;

  if (response.ok) {
    return {
      success: true,
      retryable: false,
      category: 'SUCCESS',
      message: `Request succeeded with status ${status}`,
    };
  }

  // Rate limited
  if (status === 429) {
    return {
      success: false,
      retryable: true,
      category: 'RATE_LIMIT',
      message: 'Rate limit exceeded. Backoff required.',
    };
  }

  // 5xx Server Errors (Temporary outages are retryable)
  if (status >= 500 && status <= 599) {
    const isRetryable = status === 502 || status === 503 || status === 504;
    return {
      success: false,
      retryable: isRetryable,
      category: 'SERVER_ERROR',
      message: `Server failed with status ${status}`,
    };
  }

  // 4xx Client Errors (Generally non-retryable without changes)
  return {
    success: false,
    retryable: false,
    category: 'CLIENT_ERROR',
    message: `Client error with status ${status}`,
  };
}
```
</details>

### Challenge 2: Implement an HTTP Head Check Before Download
Write an async function `getContentLength(url)` that sends an HTTP `HEAD` request to inspect the `Content-Length` header without downloading the actual response body.

<details>
<summary>View Solution</summary>

```javascript
async function getContentLength(url) {
  const response = await fetch(url, { method: 'HEAD' });

  if (!response.ok) {
    throw new Error(`HEAD request failed with status ${response.status}`);
  }

  const lengthStr = response.headers.get('Content-Length');
  if (!lengthStr) return null;

  return parseInt(lengthStr, 10);
}
```
</details>
