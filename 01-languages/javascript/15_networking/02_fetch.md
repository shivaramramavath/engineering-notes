# The Fetch API

> The WHATWG Fetch API is the modern, Promise-based standard for making HTTP requests across browsers, Node.js, Deno, and edge runtimes, featuring streaming body consumption and unified cancellation.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [Promises](../11_asynchronous-javascript/03_promises.md), [Cancellation & Abort](../11_asynchronous-javascript/08_cancellation-and-abort.md), [HTTP Fundamentals](./01_http-fundamentals.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **Standard** | WHATWG Fetch Living Standard / Node.js 18+ (Undici Engine) / Baseline Widely Available |

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

Before the Fetch API was standardized, JavaScript HTTP networking in browsers relied entirely on `XMLHttpRequest` (XHR), a complex callback-driven API created in 1999 that mixed state machines with event listeners and lacked native Promise integration. In Node.js, developers had to use low-level `http.request()` or install heavy third-party packages like `request` or `axios`.

The **WHATWG Fetch API** unified HTTP client networking across all JavaScript runtimes into a clean, modern standard. Built natively around Promises, the Fetch API treats HTTP streams as first-class citizens via `ReadableStream`, supports cooperative cancellation through `AbortSignal`, and enforces security policies (like CORS and Origin separation) uniformly across client, server, and edge environments.

---

## 2. Core Concepts

### Two-Step Asynchronous Flow
Making a network request with `fetch()` involves two distinct asynchronous stages:
1. **Header Settlement**: The initial `fetch(url)` promise resolves as soon as the HTTP response status line and response headers arrive. The full body payload has **not yet finished downloading**.
2. **Body Consumption**: The developer chooses how to consume the body stream (`.json()`, `.text()`, `.blob()`, `.arrayBuffer()`), returning a second promise that settles once the bytes have been completely downloaded and decoded.

```text
fetch(url) ──> Promise<Response>
                      │
                      ▼ (Headers arrived; Body is a streaming ReadableStream)
             response.json() / response.text() / response.blob()
                      │
                      ▼
             Promise<ParsedData> (Body fully downloaded and parsed)
```

### Response Body Consumption Is Single-Use
A Fetch `Response` body is a **ReadableStream**. Once you consume the body via `.json()`, `.text()`, or `.blob()`, the stream is locked and drained (`response.bodyUsed === true`). Attempting to read it a second time throws `TypeError: Failed to execute 'json' on 'Response': body stream already read`. If you need to read the body multiple times, you must explicitly clone the response (`response.clone()`) *before* consuming it.

---

## 3. How It Works Internally

### The Fetch Internal Pipeline

```
 [ Call: fetch(url, options) ]
               │
               ▼
 [ Normalize Request ]: Validate method, headers, and credentials mode
               │
               ▼
 [ Origin / CORS Preflight Check ]:
   If cross-origin and not simple request -> Send OPTIONS preflight
               │
               ▼
 [ Network Dispatch ]: Transmit HTTP request bytes via OS socket
               │
               ▼
 [ Response Headers Arrive ]:
   - Inspect status code
   - Instantiate native C++ Response object
   - Resolve fetch() Promise with Response instance
               │
               ▼
 [ Body Streaming ]:
   - Bytes flow incrementally into response.body (ReadableStream)
   - Calling response.json() drains bytes, decodes UTF-8, and parses JSON
```

---

## 4. Syntax & API Reference

### Core Request Signature & Init Options

```javascript
const response = await fetch(resource, {
  method: 'POST', // 'GET', 'POST', 'PUT', 'DELETE', 'PATCH', 'HEAD'
  headers: {
    'Content-Type': 'application/json',
    'Authorization': 'Bearer token_123',
  },
  body: JSON.stringify({ name: 'Ada' }), // String, FormData, Blob, BufferSource, URLSearchParams
  mode: 'cors',        // 'cors' (default), 'no-cors', 'same-origin'
  credentials: 'same-origin', // 'same-origin' (default), 'include', 'omit'
  cache: 'default',    // 'default', 'no-store', 'reload', 'no-cache', 'force-cache'
  redirect: 'follow',  // 'follow' (default), 'error', 'manual'
  signal: abortSignal, // AbortSignal for cancellation and timeout
});
```

### Response Methods & Properties

```javascript
response.ok;         // Boolean: true if status is 200-299
response.status;     // Number: e.g. 200, 404, 500
response.statusText; // String: e.g. "OK", "Not Found"
response.headers;    // Headers object: response.headers.get('content-type')
response.url;        // Final resolved URL (after any redirects)
response.bodyUsed;   // Boolean: true if body has already been consumed

// Body Consumption Methods (Each returns a Promise)
await response.json();        // Parses UTF-8 JSON
await response.text();        // Returns raw string
await response.blob();        // Returns binary Blob (images, PDFs)
await response.arrayBuffer(); // Returns raw binary ArrayBuffer
await response.formData();    // Parses multipart/form-data
response.clone();             // Creates identical unread clone
```

---

## 5. Practical Examples

### Example 1: Robust Universal API Client with Error Extraction
```javascript
async function apiRequest(endpoint, options = {}) {
  const { timeoutMs = 8000, ...fetchOpts } = options;

  // Integrate strict timeout signal
  const signal = AbortSignal.timeout(timeoutMs);

  try {
    const response = await fetch(endpoint, {
      ...fetchOpts,
      signal,
      headers: {
        'Accept': 'application/json',
        'Content-Type': 'application/json',
        ...fetchOpts.headers,
      },
    });

    if (!response.ok) {
      let errorMessage = `HTTP ${response.status}: ${response.statusText}`;
      try {
        // Attempt to extract server error details from JSON
        const errorPayload = await response.json();
        errorMessage = errorPayload.message || errorMessage;
      } catch {
        // Fall back to status text if body is not JSON
      }
      throw new Error(errorMessage);
    }

    if (response.status === 204) return null;
    return await response.json();
  } catch (error) {
    if (error.name === 'TimeoutError') {
      throw new Error(`Request to ${endpoint} timed out after ${timeoutMs}ms`);
    }
    throw error;
  }
}
```

### Example 2: Streaming Download with Real-Time Progress Bar
```javascript
async function downloadWithProgress(url, onProgress) {
  const response = await fetch(url);
  if (!response.ok) throw new Error(`HTTP Error: ${response.status}`);

  const contentLength = response.headers.get('Content-Length');
  const totalBytes = contentLength ? parseInt(contentLength, 10) : 0;
  let loadedBytes = 0;

  const reader = response.body.getReader();
  const chunks = [];

  while (true) {
    const { done, value } = await reader.read();
    if (done) break;

    chunks.push(value);
    loadedBytes += value.length;

    if (totalBytes > 0) {
      const percentage = Math.round((loadedBytes / totalBytes) * 100);
      onProgress(percentage, loadedBytes, totalBytes);
    }
  }

  // Combine Uint8Array chunks into single Blob
  return new Blob(chunks);
}
```

### Example 3: Dual-Consuming Response Body with `response.clone()`
```javascript
async function fetchAndAuditLog(url) {
  const response = await fetch(url);

  // CLONE the response before consuming the body!
  const auditClone = response.clone();

  // 1. Consume primary response for application data
  const data = await response.json();

  // 2. Consume cloned response for background telemetry/auditing
  auditClone.text().then(rawText => {
    sendToAuditLogs({
      url,
      status: auditClone.status,
      bodyLength: rawText.length,
      timestamp: Date.now(),
    });
  });

  return data;
}
```

### Example 4: Authenticated Requests with Cookie Credentials
```javascript
// By default in cross-origin requests, fetch OMITS cookies and auth headers.
// Setting credentials: 'include' sends HTTP-only session cookies cross-origin!
async function fetchAuthenticatedCrossDomain(endpoint) {
  const res = await fetch(`https://api.partner.com${endpoint}`, {
    credentials: 'include', // Sends cross-origin cookies (requires server Access-Control-Allow-Credentials: true)
  });
  return res.json();
}
```

### Example 5: Sending URL-Encoded Form Data
```javascript
async function submitUrlEncoded(url, params) {
  // URLSearchParams automatically sets Content-Type: application/x-www-form-urlencoded
  const body = new URLSearchParams(params);

  const res = await fetch(url, {
    method: 'POST',
    body,
  });

  return res.json();
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`fetch()` Does Not Reject on 404 or 500**: The most common beginner bug. `fetch()` only rejects if the network fails completely. Always check `if (!response.ok)`.
2. **Body Already Read Error**: Reading `response.json()` and then `response.text()` throws `TypeError: body stream already read`. Use `response.clone()` beforehand if you need multiple reads.
3. **CORS Preflight Failures**: Adding custom headers (e.g. `X-Api-Key`) or sending `application/json` triggers a browser CORS `OPTIONS` preflight request. If the server does not handle `OPTIONS` with appropriate `Access-Control-Allow-*` headers, the `fetch()` promise rejects with a generic `TypeError: Failed to fetch`.
4. **Credential Mode Defaults**: Modern browsers default `credentials` to `'same-origin'`. If making cross-origin requests that require session cookies, you must explicitly set `credentials: 'include'`.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Assuming 404 Rejects** | `try { await fetch() } catch { showError() }` | Check `if (!response.ok) throw ...` | Fetch resolves on HTTP error status codes. |
| **Double Body Read** | `await res.json(); await res.text();` | Call `res.clone()` before reading | The underlying `ReadableStream` is locked and drained. |
| **Manual JSON Content-Type with FormData**| Setting `'Content-Type': 'multipart/form-data'` | Omit header when passing `FormData` | Omits the required MIME boundary string. |
| **Missing Request Timeout** | `await fetch(url)` without signal | Use `signal: AbortSignal.timeout(ms)` | Request can hang indefinitely on network blackholes. |
| **Parsing JSON on 204 No Content**| `const data = await res.json()` | Check `if (res.status === 204) return null;` | Throws `SyntaxError: Unexpected end of JSON input`. |
| **Hardcoding Auth in URLs** | `fetch('https://user:pass@api.com')` | Use `Authorization` header | Modern Fetch specification disallows credentials in URL strings. |

---

## 8. Performance Characteristics

- **Zero Memory Bloat with Streaming**: Consuming responses via `response.body.getReader()` processes data in small streaming chunks, allowing multi-gigabyte files to be processed without loading them into memory.
- **Node.js Undici Engine**: In Node.js 18+, native `fetch()` is implemented on top of **Undici**, an ultra-fast HTTP/1.1 and HTTP/2 client written in C++ and optimized JavaScript that is up to 3x faster than legacy `http.request`.

---

## 9. Engine Internals

1. **ReadableStream Backpressure**: The `response.body` stream uses native backpressure. If JavaScript slows down reading from `reader.read()`, the underlying TCP socket window is clamped, pausing network byte delivery at the OS level.
2. **Body Teeing in `response.clone()`**: When `response.clone()` is invoked, the engine "tees" the internal C++ `ReadableStream` into two branch streams, duplicating incoming network chunks in memory.

---

## 10. Best Practices & Design Guidelines

1. **Always Enforce Timeouts via `AbortSignal.timeout()`**: Never perform open-ended network calls without timeouts.
2. **Verify `response.ok` Before Parsing**: Check `if (!response.ok)` before consuming the body to avoid JSON parsing crashes on HTML error pages.
3. **Use `Headers` Object for Dynamic Headers**: Use the native `Headers` interface to inspect and append headers safely without casing discrepancies.
4. **Use `ReadableStream` for Large Files**: Never call `response.blob()` or `response.arrayBuffer()` on huge files; stream chunks to prevent memory spikes.

---

## 11. Real-World Case Study

### Resilient Microservice HTTP Client with Token Refresh Interceptor

A single-page application interacts with an OAuth2 backend. When an API call returns `401 Unauthorized`, the client must automatically pause outgoing requests, refresh the authentication token, and replay the original request transparently.

```javascript
class AuthenticatedApiClient {
  #accessToken = null;
  #refreshPromise = null;

  constructor(token) {
    this.#accessToken = token;
  }

  async fetch(url, options = {}) {
    // 1. Initial request with current token
    let response = await this.#executeRequest(url, options);

    // 2. Intercept 401 Unauthorized
    if (response.status === 401) {
      console.warn('Access token expired. Refreshing token...');

      // Single-flight token refresh to prevent duplicate concurrent refreshes
      if (!this.#refreshPromise) {
        this.#refreshPromise = this.#refreshToken().finally(() => {
          this.#refreshPromise = null;
        });
      }

      await this.#refreshPromise;

      // 3. Replay original request with fresh token
      response = await this.#executeRequest(url, options);
    }

    if (!response.ok) {
      throw new Error(`API Request failed with status: ${response.status}`);
    }

    return response.json();
  }

  async #executeRequest(url, options) {
    return fetch(url, {
      ...options,
      headers: {
        'Accept': 'application/json',
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${this.#accessToken}`,
        ...options.headers,
      },
    });
  }

  async #refreshToken() {
    const res = await fetch('/api/auth/refresh', { method: 'POST' });
    if (!res.ok) throw new Error('Session expired. Please log in again.');
    const data = await res.json();
    this.#accessToken = data.accessToken;
    console.log('Token successfully refreshed');
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. Why is <code>fetch()</code> considered a two-step asynchronous operation?</strong></summary>

Because the initial `fetch(url)` promise resolves as soon as the HTTP status line and headers are received from the server. The body payload has not yet finished downloading. Consuming the body (via `.json()`, `.text()`, etc.) returns a second promise that settles only when the streaming bytes have completely downloaded.
</details>

<details>
<summary><strong>2. What happens if you call <code>response.json()</code> twice on the same response?</strong></summary>

It throws a `TypeError: Failed to execute 'json' on 'Response': body stream already read`. The response body is a `ReadableStream` that can only be drained once. To read it multiple times, you must call `response.clone()` before reading.
</details>

<details>
<summary><strong>3. How does <code>fetch()</code> handle CORS errors?</strong></summary>

If a cross-origin request fails the browser's CORS preflight or response header verification (`Access-Control-Allow-Origin`), the browser halts processing and rejects the `fetch()` promise with a generic `TypeError: Failed to fetch`. For security reasons, JavaScript cannot inspect the status code or response headers of a CORS failure.
</details>

<details>
<summary><strong>4. How do you implement a 5-second timeout on a <code>fetch()</code> call?</strong></summary>

By passing an `AbortSignal.timeout(5000)` into the options object:
```javascript
const response = await fetch(url, { signal: AbortSignal.timeout(5000) });
```
If 5 seconds elapse before completion, the promise rejects with a `TimeoutError` DOMException.
</details>

<details>
<summary><strong>5. What is the difference between <code>credentials: 'same-origin'</code> and <code>credentials: 'include'</code>?</strong></summary>

`'same-origin'` (the default) sends cookies and authentication headers only when making requests to the same origin. `'include'` sends cookies on all requests, including cross-origin requests (requiring the server to return `Access-Control-Allow-Credentials: true`).
</details>

<details>
<summary><strong>6. What is the difference between <code>response.blob()</code> and <code>response.arrayBuffer()</code>?</strong></summary>

`response.blob()` returns an immutable, opaque binary `Blob` object with a MIME type, ideal for passing to `URL.createObjectURL()`. `response.arrayBuffer()` returns raw, mutable memory bytes that can be inspected and manipulated via `TypedArray` views (`Uint8Array`, `DataView`).
</details>

<details>
<summary><strong>7. How does the <code>mode: 'no-cors'</code> option work, and what is its limitation?</strong></summary>

`mode: 'no-cors'` allows sending a cross-origin request without CORS headers, but returns an **opaque response** (`response.type === 'opaque'`). The status code is always `0`, headers are empty, and the response body is completely unreadable by JavaScript.
</details>

<details>
<summary><strong>8. Why should you check <code>response.status === 204</code> before calling <code>response.json()</code>?</strong></summary>

An HTTP 204 No Content response has an intentionally empty body. Calling `.json()` on an empty body throws a `SyntaxError: Unexpected end of JSON input`.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an Automatic JSON Fetch Wrapper
Create a utility function `fetchJson(url, options)` that handles JSON serialization of the request body, sets standard headers, enforces a 10-second timeout, verifies `response.ok`, and safely parses the JSON response.

<details>
<summary>View Solution</summary>

```javascript
async function fetchJson(url, options = {}) {
  const { data, headers, timeoutMs = 10000, ...fetchOptions } = options;

  const init = {
    ...fetchOptions,
    signal: AbortSignal.timeout(timeoutMs),
    headers: {
      'Accept': 'application/json',
      ...headers,
    },
  };

  if (data !== undefined) {
    init.body = JSON.stringify(data);
    init.headers['Content-Type'] = 'application/json';
    if (!init.method) init.method = 'POST';
  }

  const response = await fetch(url, init);

  if (!response.ok) {
    const errorBody = await response.text().catch(() => '');
    throw new Error(`HTTP ${response.status}: ${errorBody || response.statusText}`);
  }

  if (response.status === 204) return null;
  return response.json();
}
```
</details>

### Challenge 2: Implement a Streaming Text Line Reader
Write an async generator `fetchLines(url)` that fetches a large text file using `fetch()` and yields each line of text sequentially as chunks arrive over the network, without loading the full file into memory.

<details>
<summary>View Solution</summary>

```javascript
async function* fetchLines(url) {
  const response = await fetch(url);
  if (!response.ok) throw new Error(`HTTP ${response.status}`);

  const reader = response.body.getReader();
  const decoder = new TextDecoder('utf-8');
  let buffer = '';

  try {
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;

      buffer += decoder.decode(value, { stream: true });
      const lines = buffer.split('\n');
      buffer = lines.pop() ?? ''; // Retain trailing partial line

      for (const line of lines) {
        yield line;
      }
    }

    if (buffer.length > 0) {
      yield buffer;
    }
  } finally {
    reader.releaseLock();
  }
}
```
</details>
