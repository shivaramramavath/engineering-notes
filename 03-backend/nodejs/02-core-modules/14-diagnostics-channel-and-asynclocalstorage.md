# `AsyncLocalStorage` & Diagnostics Channel in Node.js

> Comprehensive guide to managing request-scoped context across asynchronous boundaries with `AsyncLocalStorage` and decoupled instrumentation using `diagnostics_channel`.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Modules** | `node:async_hooks`, `node:diagnostics_channel` |
| **Use Cases** | Request IDs, Multi-tenancy, Distributed Tracing (OpenTelemetry), Query Logging |

---

## 1. The Context Propagation Problem

In traditional multi-threaded runtimes (Java, C#), threads maintain thread-local storage (`ThreadLocal`).
In Node.js, thousands of concurrent requests interleave asynchronously on a single thread.
Passing a `requestId`, user session, or database transaction down through every intermediate function parameter ("prop drilling") creates massive code pollution.

**`AsyncLocalStorage` solves this natively**: It attaches a storage object to the current asynchronous execution chain, making it available anywhere downstream without parameter passing.

---

## 2. Implementing Request-Scoped Context with `AsyncLocalStorage`

```javascript
import { AsyncLocalStorage } from 'node:async_hooks';
import http from 'node:http';
import { randomUUID } from 'node:crypto';

// 1. Instantiate the storage container
export const requestContext = new AsyncLocalStorage();

// Helper to retrieve current request ID anywhere in codebase
export function getRequestId() {
  const store = requestContext.getStore();
  return store ? store.requestId : 'system';
}

// Simulated downstream database query function
async function queryDatabase() {
  // Can access request ID without receiving it as a parameter!
  console.log(`[DB Query] Executing for Request ID: ${getRequestId()}`);
  await new Promise(resolve => setTimeout(resolve, 50));
  return { status: 'ok' };
}

// 2. HTTP Server middleware wrapper
const server = http.createServer((req, res) => {
  const requestId = req.headers['x-request-id'] || randomUUID();
  const store = { requestId, startTime: Date.now() };

  // Run the rest of the request within the async storage scope
  requestContext.run(store, async () => {
    await queryDatabase();
    res.setHeader('X-Request-Id', requestId);
    res.end(JSON.stringify({ success: true, requestId: getRequestId() }));
  });
});

server.listen(3000);
```

---

## 3. Decoupled Pub/Sub with `diagnostics_channel`

`node:diagnostics_channel` provides a high-performance, in-process pub/sub system designed specifically for observability libraries (like OpenTelemetry or APMs) to subscribe to internal runtime events without monkey-patching:

```javascript
import diagnostics_channel from 'node:diagnostics_channel';

// 1. Create a named channel
const channel = diagnostics_channel.channel('http.server.request');

// 2. Subscribe to channel events (e.g. In an APM agent)
diagnostics_channel.subscribe('http.server.request', (message, name) => {
  console.log(`[Diagnostic Event] ${name}: ${message.method} ${message.url}`);
});

// 3. Publish from HTTP server
if (channel.hasSubscribers) {
  channel.publish({ method: 'GET', url: '/api/users', timestamp: Date.now() });
}
```
Built-in Node.js modules like `node:sqlite`, Undici (`fetch`), and HTTP servers publish diagnostic channels automatically.
