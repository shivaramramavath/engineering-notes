# Fetch API & HTTP Networking Cheatsheet

> Quick-reference guide covering Fetch API request options, response streaming, HTTP status codes, CORS configuration headers, and WebSocket protocols.

| Metadata | Details |
| :--- | :--- |
| **Track** | Reference Cheatsheets (`27_cheatsheets`) |
| **Baseline** | ECMAScript 2024 / Node.js 22 LTS |
| **Level** | Reference / Quick Lookup |

---

## 1. Complete Fetch Request Configuration

```javascript
const res = await fetch('https://api.example.com/v1/users', {
  method: 'POST', // GET, POST, PUT, PATCH, DELETE, HEAD
  headers: {
    'Content-Type': 'application/json',
    'Accept': 'application/json',
    'Authorization': 'Bearer ' + token
  },
  body: JSON.stringify({ name: 'Alex' }),
  mode: 'cors', // cors, no-cors, same-origin
  credentials: 'include', // include, same-origin, omit
  cache: 'no-cache', // default, no-store, reload, no-cache, force-cache
  redirect: 'follow', // follow, error, manual
  signal: AbortSignal.timeout(5000) // 5s timeout cancellation
});

if (!res.ok) {
  throw new Error(`HTTP Error ${res.status}: ${res.statusText}`);
}
const data = await res.json();
```

---

## 2. Response Body Consumptions

| Method | Returns | Use Case |
| :--- | :--- | :--- |
| `res.json()` | Parsed JavaScript Object | JSON API responses |
| `res.text()` | UTF-8 String | HTML, plain text, SVG, CSV |
| `res.blob()` | Binary `Blob` object | Images, PDFs, audio/video downloads |
| `res.arrayBuffer()`| Raw binary `ArrayBuffer` | Cryptography, WebAssembly, binary protocols |
| `res.formData()` | `FormData` instance | Multipart form uploads |
| `res.body` | `ReadableStream<Uint8Array>` | Streaming NDJSON, server-sent events, progress |

```javascript
// Consuming ReadableStream directly:
const reader = res.body.getReader();
const decoder = new TextDecoder();
while (true) {
  const { done, value } = await reader.read();
  if (done) break;
  console.log('Stream chunk:', decoder.decode(value, { stream: true }));
}
```

---

## 3. Standard HTTP Status Code Reference

| Status Code | Meaning | Client Behavior |
| :--- | :--- | :--- |
| **200 OK** | Success | Process response body |
| **201 Created** | Resource created | Inspect `Location` header or created object |
| **204 No Content** | Success with empty body | Do not attempt to parse JSON |
| **301 / 308** | Permanent Redirect | Browser updates bookmarks/caches new URL |
| **304 Not Modified** | Conditional GET match | Read response from browser HTTP cache |
| **400 Bad Request** | Validation failed | Check request payload / headers |
| **401 Unauthorized** | Missing or invalid auth | Prompt user to authenticate / refresh token |
| **403 Forbidden** | Authenticated, but not permitted | Deny access; do not re-prompt login |
| **404 Not Found** | Resource missing | Verify URL path |
| **429 Too Many Requests** | Rate limit exceeded | Back off; inspect `Retry-After` header |
| **500 Server Error** | Unhandled backend exception | Log error; schedule retry with jitter |
| **502 / 503 / 504** | Gateway / Unavailable / Timeout | Transient infrastructure fault; retry with backoff |

---

## 4. WebSocket Client Lifecycle

```javascript
const socket = new WebSocket('wss://api.example.com/ws');

socket.addEventListener('open', () => {
  socket.send(JSON.stringify({ type: 'SUBSCRIBE', channel: 'ticker' }));
});

socket.addEventListener('message', (event) => {
  const message = JSON.parse(event.data);
  console.log('Received:', message);
});

socket.addEventListener('close', (event) => {
  console.warn(`Connection closed (code: ${event.code}, reason: ${event.reason})`);
});

socket.addEventListener('error', (err) => {
  console.error('WebSocket encountered error:', err);
});
```
