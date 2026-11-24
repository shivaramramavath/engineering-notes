# Module 05: HTTP Protocols & Web Standards in Node.js

> Deep dive into HTTP status codes, headers, cookie security, CORS, caching, HTTP/2 multiplexing, and Server-Sent Events.

---

## Module Overview

Node.js backends power the modern web through direct manipulation of HTTP requests and responses. Understanding HTTP protocol semantics, connection reuse, content negotiation, and real-time streaming unlocks high-performance, fault-tolerant backend architectures.

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-http-methods-and-status-codes.md](./01-http-methods-and-status-codes.md) | Idempotent vs safe methods, RESTful resource mapping, 2xx/4xx/5xx status codes. |
| **02** | [02-headers-and-content-negotiation.md](./02-headers-and-content-negotiation.md) | Standard headers, MIME types, `Accept`, `Content-Type`, compression negotiation. |
| **03** | [03-cookies.md](./03-cookies.md) | Cookie attributes (`HttpOnly`, `Secure`, `SameSite=Lax/Strict`), session cookies. |
| **04** | [04-cors.md](./04-cors.md) | Cross-Origin Resource Sharing, preflight `OPTIONS` requests, credentials. |
| **05** | [05-caching-and-compression.md](./05-caching-and-compression.md) | `Cache-Control`, `ETag`, gzip/Brotli compression with `zlib`. |
| **06** | [06-status-codes-reference.md](./06-status-codes-reference.md) | Exhaustive reference card for HTTP 2xx, 3xx, 4xx, and 5xx status codes. |
| **07** | [07-http2-http3-and-keepalive.md](./07-http2-http3-and-keepalive.md) | Keep-Alive connection pooling (`http.Agent`), HTTP/2 multiplexing (`node:http2`). |
| **08** | [08-server-sent-events-and-streaming.md](./08-server-sent-events-and-streaming.md) | Server-Sent Events (`text/event-stream`), chunked streaming uploads to S3. |

---

## Next Steps
Proceed to [06-express](../06-express/README.md) to explore the Express 5 framework, routing, middleware, and its modern alternatives (Fastify and Hono).
