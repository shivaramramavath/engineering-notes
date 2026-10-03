# 15 · Networking

Almost every real application talks to a server. This chapter covers how the web's request/response model works (**HTTP**), how to use it from JavaScript (**`fetch`**), how browsers control cross-site access (**headers and CORS**), how to model requests and responses, how to get real-time updates (**WebSockets and Server-Sent Events**), and the reusable **patterns** that make network code reliable.

```
browser / Node ──request──►  server
                ◄─response──
```

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 1 | [HTTP Fundamentals](./01_http-fundamentals.md) | URLs, methods, status codes, HTTPS, HTTP/2 and 3, caching basics |
| 2 | [Fetch](./02_fetch.md) | Making requests, reading bodies, errors, JSON, forms, streaming |
| 3 | [Headers and CORS](./03_headers-and-cors.md) | Important headers, same-origin policy, preflight, security headers |
| 4 | [Request and Response](./04_request-and-response.md) | `Request`/`Response` objects, bodies, files, ranges, error formats |
| 5 | [WebSockets and SSE](./05_websockets-and-sse.md) | Real-time channels, reconnection, protocols compared |
| 6 | [Networking Patterns](./06_networking-patterns.md) | API clients, retries, timeouts, auth refresh, pagination, caching |

## The network is unreliable

Every request can be **slow**, **fail**, **time out**, **return unexpected data**, be **duplicated**, or arrive **out of order**. Good networking code assumes all of this.

| Concern | Typical answer |
|---------|----------------|
| Slow or hanging | timeouts, cancellation (`AbortSignal`) |
| Transient failures | retries with backoff (idempotent requests only) |
| Bad responses | status checks, validation of data |
| Duplicate submissions | idempotency keys, disabling UI |
| Stale responses | abort previous requests, request IDs |
| Offline | queues, caching, clear UI states |

## Goal

By the end you can call APIs correctly with `fetch`, understand and debug CORS and caching, choose between polling, SSE and WebSockets, and build a resilient API client.

## Prerequisites

- [Promises and async/await](../11_asynchronous-javascript/00_README.md)
- [Cancellation and Abort](../11_asynchronous-javascript/08_cancellation-and-abort.md)
- [Error Handling](../10_error-handling/00_README.md)

**Next:** [HTTP Fundamentals](./01_http-fundamentals.md)
