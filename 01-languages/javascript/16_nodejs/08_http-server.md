# HTTP Server and Networking in Node.js

> High-throughput HTTP server architecture, request and response streams, routing primitives, keep-alive connections, and TLS security in Node.js.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Node.js Streams](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/06_streams.md), [Buffers](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/07_buffers.md), [Event Loop](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/02_node-event-loop.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | Node.js v18 LTS - v24+, HTTP/1.1 & HTTP/2 |

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

The `node:http` module provides an abstraction over TCP sockets (`node:net`) that understands the HTTP/1.1 protocol. Rather than exposing raw packet buffers, it parses wire bytes into structured `IncomingMessage` objects and provides an `ServerResponse` stream to construct responses.

Every major Node.js web framework—Express, Fastify, Koa, NestJS—either builds directly on top of `node:http` or models its pipeline after it. Understanding native HTTP mechanics is essential for diagnosing socket hangs, memory backpressure during file uploads, TLS handshake bottlenecks, and connection leaks in production microservices.

---

## 2. Core Concepts

### Server Lifecycle and Sockets
An HTTP server in Node.js is an instance of `http.Server`, which inherits from `net.Server`. When a client connects:
1. A TCP connection is established via libuv socket listener.
2. The server binds an HTTP parser (`llhttp` written in C) to the incoming TCP stream.
3. Once headers are completely parsed, the server emits a `'request'` event with `(req, res)`.
4. `req` is a `ReadableStream` providing the HTTP body chunks.
5. `res` is a `WritableStream` allowing buffered chunks to be sent back across the TCP socket.

### Persistent Connections (Keep-Alive)
By default in HTTP/1.1, connections remain open across requests unless explicitly terminated with `Connection: close`. Node.js maintains keep-alive sockets in an idle pool until `server.keepAliveTimeout` (default 5000ms) or `server.headersTimeout` expires.

### Chunked Transfer Encoding
When responses are streamed without a known `Content-Length` header upfront, Node.js automatically emits `Transfer-Encoding: chunked`. Each buffer write becomes a chunk prefixed with its hexadecimal length and followed by `\r\n`.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------------------+
| Client Browser / HTTP Client                                            |
+-------------------------------------------------------------------------+
                                   | TCP Syn / Syn-Ack / Handshake
                                   v
+-------------------------------------------------------------------------+
| libuv TCP Socket Listener (Poll Phase)                                  |
|   - Allocates memory buffer via uv_alloc_cb                             |
|   - Reads raw bytes from OS network stack via uv_read_cb                |
+-------------------------------------------------------------------------+
                                   | Raw TCP Buffer
                                   v
+-------------------------------------------------------------------------+
| llhttp Parser (C / Assembly optimized)                                 |
|   - Parses HTTP method, URL, and header key-value pairs                 |
|   - Emits on_headers_complete -> Creates IncomingMessage & ServerResponse|
|   - Emits on_body -> Pushes Buffer chunks into req ReadableStream       |
|   - Emits on_message_complete -> req emits 'end'                        |
+-------------------------------------------------------------------------+
                                   | Emits ('request', req, res)
                                   v
+-------------------------------------------------------------------------+
| User Request Handler Callback                                           |
|   - req (stream.Readable): body consumption, backpressure handling      |
|   - res (stream.Writable): writeHead(), write(), end()                  |
+-------------------------------------------------------------------------+
                                   | res.write(chunk)
                                   v
+-------------------------------------------------------------------------+
| OutgoingMessage Internal Output Queue -> TCP Socket Write Buffer        |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Creating an HTTP Server
```javascript
import http from 'node:http';

const server = http.createServer({
  keepAlive: true,
  keepAliveTimeout: 5000,
  maxHeaderSize: 16384, // 16 KB header limit
}, (req, res) => {
  // req: http.IncomingMessage (ReadableStream)
  // res: http.ServerResponse (WritableStream)
});

server.listen(3000, '0.0.0.0', () => {
  console.log('HTTP server listening on http://0.0.0.0:3000');
});
```

### Core Server Properties and Timeouts
- `server.timeout`: Socket inactivity timeout before emitting `'timeout'` (default 0 / disabled in modern Node).
- `server.keepAliveTimeout`: Inactivity time on an idle keep-alive socket before destroying it (default 5000ms).
- `server.headersTimeout`: Time allowed for a client to send complete HTTP headers (default 60000ms). Prevents slowloris attacks.
- `server.requestTimeout`: Maximum time allowed between receiving the first byte of request headers and receiving the entire request body.

### Response Methods
- `res.writeHead(statusCode[, statusMessage][, headers])`: Writes the status line and headers immediately to the wire.
- `res.setHeader(name, value)`: Queues a header to be sent when headers flush.
- `res.write(chunk[, encoding][, callback])`: Sends a body chunk over the wire. Returns `false` if internal buffer is full (backpressure).
- `res.end([data][, encoding][, callback])`: Signals the end of the response stream.

---

## 5. Practical Examples

### Example 1: Minimal Robust HTTP Router
```javascript
import http from 'node:http';

const routes = new Map();

function addRoute(method, pathname, handler) {
  routes.set(`${method.toUpperCase()}:${pathname}`, handler);
}

addRoute('GET', '/api/health', (req, res) => {
  res.writeHead(200, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({ status: 'ok', uptime: process.uptime() }));
});

addRoute('POST', '/api/echo', async (req, res) => {
  const chunks = [];
  for await (const chunk of req) {
    chunks.push(chunk);
  }
  const payload = Buffer.concat(chunks).toString('utf8');

  res.writeHead(200, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({ received: payload, bytes: Buffer.byteLength(payload) }));
});

const server = http.createServer((req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`);
  const handler = routes.get(`${req.method}:${url.pathname}`);

  if (handler) {
    handler(req, res);
  } else {
    res.writeHead(404, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ error: 'Not Found', path: url.pathname }));
  }
});

server.listen(8080);
```

### Example 2: Streaming File Download with Content-Disposition
```javascript
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { pipeline } from 'node:stream/promises';

const server = http.createServer(async (req, res) => {
  if (req.method === 'GET' && req.url === '/download') {
    const filePath = path.resolve('sample.pdf');

    try {
      const stats = await fs.promises.stat(filePath);

      res.writeHead(200, {
        'Content-Type': 'application/pdf',
        'Content-Length': stats.size,
        'Content-Disposition': 'attachment; filename="document.pdf"',
      });

      const fileStream = fs.createReadStream(filePath);
      await pipeline(fileStream, res);
    } catch (err) {
      if (!res.headersSent) {
        res.writeHead(err.code === 'ENOENT' ? 404 : 500);
        res.end(err.code === 'ENOENT' ? 'File Not Found' : 'Server Error');
      }
    }
  }
});
```

### Example 3: Enforcing Request Body Size Limits to Thwart DoS
```javascript
import http from 'node:http';

const MAX_BODY_BYTES = 1 * 1024 * 1024; // 1 MB limit

function readBody(req, limit = MAX_BODY_BYTES) {
  return new Promise((resolve, reject) => {
    let received = 0;
    const chunks = [];

    function onData(chunk) {
      received += chunk.length;
      if (received > limit) {
        req.removeListener('data', onData);
        req.removeListener('end', onEnd);
        req.destroy(); // Terminate abusive socket immediately
        reject(new Error('PAYLOAD_TOO_LARGE'));
      } else {
        chunks.push(chunk);
      }
    }

    function onEnd() {
      resolve(Buffer.concat(chunks));
    }

    req.on('data', onData);
    req.on('end', onEnd);
    req.on('error', reject);
  });
}

const server = http.createServer(async (req, res) => {
  if (req.method === 'POST') {
    try {
      const bodyBuffer = await readBody(req);
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ receivedBytes: bodyBuffer.length }));
    } catch (err) {
      if (err.message === 'PAYLOAD_TOO_LARGE') {
        res.writeHead(413, { 'Content-Type': 'text/plain' });
        res.end('Payload Too Large: Maximum allowed is 1MB');
      } else {
        res.writeHead(400);
        res.end('Malformed Request');
      }
    }
  }
});
```

### Example 4: Custom HTTP Agent with Connection Pooling
```javascript
import http from 'node:http';

const agent = new http.Agent({
  keepAlive: true,
  maxSockets: 50,       // Max simultaneous sockets per origin
  maxFreeSockets: 10,   // Max idle sockets retained in pool
  timeout: 10000,       // Socket inactive timeout (10s)
});

function postMetric(data) {
  return new Promise((resolve, reject) => {
    const payload = JSON.stringify(data);

    const req = http.request({
      hostname: 'metrics.internal',
      port: 8080,
      path: '/v1/ingest',
      method: 'POST',
      agent, // Reuse sockets through pooled agent
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(payload),
      },
    }, (res) => {
      res.resume(); // Discard body chunks to free the socket back to agent pool
      res.on('end', () => resolve(res.statusCode));
    });

    req.on('error', reject);
    req.write(payload);
    req.end();
  });
}
```

### Example 5: Handling HTTP Upgrades (WebSockets Integration)
```javascript
import http from 'node:http';
import crypto from 'node:crypto';

const server = http.createServer((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Normal HTTP endpoint');
});

server.on('upgrade', (req, socket, head) => {
  if (req.headers['upgrade']?.toLowerCase() !== 'websocket') {
    socket.destroy();
    return;
  }

  const clientKey = req.headers['sec-websocket-key'];
  const GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';
  const acceptHash = crypto
    .createHash('sha1')
    .update(clientKey + GUID)
    .digest('base64');

  const responseHeaders = [
    'HTTP/1.1 101 Switching Protocols',
    'Upgrade: websocket',
    'Connection: Upgrade',
    `Sec-WebSocket-Accept: ${acceptHash}`,
    '\r\n',
  ];

  socket.write(responseHeaders.join('\r\n'));
  console.log('Socket successfully upgraded to WebSocket protocol');

  // Socket is now detached from HTTP parser and can stream WebSocket frames
  socket.on('data', (rawFrame) => {
    // Process frame
  });
});

server.listen(3000);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`headersTimeout` vs Reverse Proxy Idle Timeouts**: If Node's `keepAliveTimeout` is shorter than AWS ALB or Nginx's proxy timeout, the client/proxy will attempt to reuse a socket right as Node is closing it, causing spurious `ECONNRESET` errors. Always set `server.keepAliveTimeout` slightly higher than downstream load balancer timeouts (e.g., 65s if AWS ALB is 60s).
2. **Missing `res.end()`**: If an error handler executes or a branch misses `res.end()`, the client TCP connection remains open until timeout, exhausting server socket handles.
3. **`ERR_HTTP_HEADERS_SENT`**: Attempting to call `res.setHeader()` or `res.writeHead()` after response bytes have already been flushed to the network socket throws this uncatchable runtime error if not guarded with `if (!res.headersSent)`.
4. **Failing to Consume Response Streams in Client Requests**: When making an outgoing `http.request`, if you do not attach a `'data'` listener or call `res.resume()`, the underlying socket cannot emit `'end'`, and cannot return to the `http.Agent` pool, leaking socket descriptors.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| `Buffer.from(str).length` in `Content-Length` | Inefficient double allocation for large strings | Use `Buffer.byteLength(str, 'utf8')` |
| Calling `res.writeHead()` twice | Throws `ERR_HTTP_HEADERS_SENT` on second call | Check `if (!res.headersSent)` before setting headers |
| Discarding incoming client chunks without `req.resume()` | Pauses incoming socket stream; garbage collection cannot reap socket | Call `req.resume()` if body is ignored |
| Setting `agent: false` on frequent outgoing requests | Creates a fresh TCP handshake & TLS negotiation for every request | Use a singleton `http.Agent({ keepAlive: true })` |
| Serving large files with `fs.readFile()` | Buffers entire file in V8 heap, causing Out-Of-Memory spikes | Stream via `pipeline(fs.createReadStream(), res)` |
| Omitting `headersTimeout` configuration | Vulnerable to slowloris attacks sending headers 1 byte every few seconds | Rely on default `headersTimeout: 60_000` or lower |

---

## 8. Performance Characteristics

- **Socket Allocations**: Each active TCP socket allocates roughly 20–40 KB in kernel space plus ~2 KB of Node.js stream objects.
- **llhttp Parser**: Written in C and compiled into Node core. Processes headers at multi-gigabit speeds using single-pass pointer increments.
- **CJS/ESM Overhead**: Zero overhead; `node:http` delegates to C++ native bindings (`HttpParser`, `StreamBase`).

---

## 9. Engine Internals

1. **`llhttp` Execution Loop**:
   - `uv_read_cb` receives raw bytes into a pooled libuv slab.
   - Pointers are handed to `llhttp__execute()`. It parses the method enum, URL byte offsets, and header tokens without copying strings until necessary.
2. **Backpressure Propagation**:
   - When calling `res.write(chunk)`, if the kernel socket send buffer is full, `StreamBase::Write` returns an unsatisfied promise and Node's JavaScript buffer queues the chunk.
   - `res.write()` returns `false`. When the kernel drains bytes across the wire, libuv fires `uv_write_cb`, which drains Node's queue and emits `'drain'`.

---

## 10. Best Practices & Design Guidelines

- **Always configure timeouts**: Set `server.keepAliveTimeout`, `server.headersTimeout`, and `server.requestTimeout` explicitly.
- **Graceful Shutdown**: Intercept `SIGINT`/`SIGTERM`, call `server.close()` to stop accepting new connections, and use `server.closeIdleConnections()` to terminate idle keep-alive sockets immediately.
- **Validate Content-Length**: Reject requests whose declared `Content-Length` exceeds server body size thresholds immediately before streaming chunks.

---

## 11. Real-World Case Study

### High-Availability Graceful Shutdown Handler
```javascript
import http from 'node:http';

const server = http.createServer((req, res) => {
  res.writeHead(200);
  res.end('Processed safely\n');
});

server.listen(3000);

let isShuttingDown = false;

function shutdown(signal) {
  if (isShuttingDown) return;
  isShuttingDown = true;
  console.log(`Received ${signal}. Initiating graceful shutdown...`);

  // Stop accepting new TCP connections
  server.close((err) => {
    if (err) {
      console.error('Error during server close:', err);
      process.exit(1);
    }
    console.log('All active HTTP connections completed. Exiting cleanly.');
    process.exit(0);
  });

  // Close idle keep-alive sockets immediately so event loop can exit
  if (typeof server.closeIdleConnections === 'function') {
    server.closeIdleConnections();
  }

  // Force kill if connections hang longer than 15 seconds
  const forceKillTimeout = setTimeout(() => {
    console.error('Graceful shutdown timeout exceeded. Forcing termination.');
    process.exit(1);
  }, 15000);

  forceKillTimeout.unref(); // Do not prevent clean process exit if server closes first
}

process.on('SIGTERM', () => shutdown('SIGTERM'));
process.on('SIGINT', () => shutdown('SIGINT'));
```

---

## 12. Interview Questions

<details>
<summary>1. What causes the error "ERR_HTTP_HEADERS_SENT"?</summary>
It occurs when application code calls <code>res.setHeader()</code>, <code>res.writeHead()</code>, or any method that attempts to modify response status or headers after the HTTP response headers have already been transmitted over the socket to the client.
</details>

<details>
<summary>2. Why must keepAliveTimeout on Node.js be higher than the load balancer timeout?</summary>
If Node.js closes an idle keep-alive socket before the load balancer does, there is a race condition where the load balancer forwards an incoming client request over that socket just as Node is sending a TCP FIN packet. The load balancer receives a connection reset (ECONNRESET) and fails the client request with a 502 Bad Gateway.
</details>

<details>
<summary>3. What is the difference between res.write() and res.end()?</summary>
<code>res.write()</code> emits a body chunk into the writable stream without closing the response, whereas <code>res.end()</code> signals that all headers and body chunks have been completely transmitted, terminating the HTTP response message and flushing trailing chunks.
</details>

<details>
<summary>4. How does Node.js handle HTTP keep-alive connections?</summary>
By default in HTTP/1.1, the TCP connection remains open after a response completes. Node stores the socket in an idle state. If a new request arrives within <code>keepAliveTimeout</code>, it reuses the same socket and parser instance without requiring a new TCP three-way handshake.
</details>

<details>
<summary>5. What is llhttp and what role does it play?</summary>
<code>llhttp</code> is a C library embedded in Node.js that parses raw HTTP request/response bytes. It replaces the legacy <code>http_parser</code>, offering significant performance gains through optimized state-machine parsing and memory safety.
</details>

<details>
<summary>6. How can an HTTP server protect itself against Slowloris attacks?</summary>
By configuring <code>server.headersTimeout</code>. Slowloris attacks send headers extremely slowly (e.g., 1 byte every few seconds) to tie up server sockets. <code>headersTimeout</code> forces the server to destroy sockets that do not submit complete headers within the allotted window (default 60s).
</details>

<details>
<summary>7. Why should you call res.resume() on unhandled HTTP client responses?</summary>
If response data is not consumed, the underlying socket remains paused waiting for the stream to drain. Calling <code>res.resume()</code> discards incoming bytes, allowing the stream to reach <code>'end'</code> so the socket can return to the <code>http.Agent</code> pool for reuse.
</details>

<details>
<summary>8. How does Transfer-Encoding: chunked work in Node.js?</summary>
When a response is streamed without a predetermined <code>Content-Length</code> header, Node.js automatically sets <code>Transfer-Encoding: chunked</code>. Each call to <code>res.write()</code> outputs a chunk preceded by its size in hexadecimal notation and ended with <code>\r\n</code>, concluding with a <code>0\r\n\r\n</code> chunk when <code>res.end()</code> is called.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build an In-Memory Rate Limiter Middleware
Implement a native HTTP server middleware that tracks IP request counts using a sliding window and returns HTTP 429 when a client exceeds 10 requests per second.

<details>
<summary>View Solution</summary>

```javascript
import http from 'node:http';

const tracker = new Map(); // ip -> Array<timestamp>
const WINDOW_MS = 1000;
const MAX_REQUESTS = 10;

function isRateLimited(ip) {
  const now = Date.now();
  const timestamps = tracker.get(ip) || [];
  const validTimestamps = timestamps.filter(t => now - t < WINDOW_MS);

  if (validTimestamps.length >= MAX_REQUESTS) {
    tracker.set(ip, validTimestamps);
    return true;
  }

  validTimestamps.push(now);
  tracker.set(ip, validTimestamps);
  return false;
}

const server = http.createServer((req, res) => {
  const ip = req.socket.remoteAddress || 'unknown';

  if (isRateLimited(ip)) {
    res.writeHead(429, {
      'Content-Type': 'application/json',
      'Retry-After': '1',
    });
    res.end(JSON.stringify({ error: 'Too Many Requests' }));
    return;
  }

  res.writeHead(200, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({ message: 'Request accepted' }));
});

server.listen(3000);
```
</details>

### Exercise 2: Chunked Live Progress Streamer
Create an HTTP endpoint `/progress` that streams 5 sequential JSON progress updates (0%, 25%, 50%, 75%, 100%) spaced 200ms apart using `Transfer-Encoding: chunked` and Newline Delimited JSON (NDJSON).

<details>
<summary>View Solution</summary>

```javascript
import http from 'node:http';

const server = http.createServer((req, res) => {
  if (req.url === '/progress') {
    res.writeHead(200, {
      'Content-Type': 'application/x-ndjson',
      'Transfer-Encoding': 'chunked',
      'Cache-Control': 'no-cache',
    });

    const percentages = [0, 25, 50, 75, 100];
    let index = 0;

    const interval = setInterval(() => {
      if (index < percentages.length) {
        const payload = JSON.stringify({ progress: percentages[index], timestamp: Date.now() }) + '\n';
        res.write(payload);
        index++;
      } else {
        clearInterval(interval);
        res.end();
      }
    }, 200);

    req.on('close', () => {
      clearInterval(interval);
    });
  } else {
    res.writeHead(404);
    res.end();
  }
});

server.listen(3000);
```
</details>
