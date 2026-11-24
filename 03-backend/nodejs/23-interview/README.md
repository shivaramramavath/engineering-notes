# Node.js Technical Interview Master Preparation Guide

> Comprehensive Senior & Staff-level Node.js technical interview guide covering runtime internals, libuv event loop mechanics, memory management, stream pipelines, database optimization, distributed architecture, output prediction, and live coding challenges.

---

## Curriculum Structure

| Module | Title | Target Topics & Engineering Depth |
| :---: | :--- | :--- |
| **00** | **[Fundamentals Q&A](00-fundamentals-qa.md)** | Single-threaded vs multi-threaded, V8 engine internals, CommonJS vs ESM, process lifecycle, environment configuration, Node.js vs other runtimes (Deno, Bun). |
| **01** | **[Event Loop & Asynchronous Mechanics Q&A](01-event-loop-and-async-qa.md)** | Libuv phases (Timers, Pending, Poll, Check, Close), microtasks (`process.nextTick` vs `queueMicrotask` vs Promise), thread pool (`UV_THREADPOOL_SIZE`), non-blocking I/O. |
| **02** | **[Streams & Buffers Q&A](02-streams-and-buffers-qa.md)** | `Readable`, `Writable`, `Transform`, `Duplex`, backpressure handling (`highWaterMark`, `drain`), `pipeline()` vs `pipe()`, Buffer allocation (`Buffer.alloc` vs `Buffer.allocUnsafe`). |
| **03** | **[Express, Fastify & HTTP Q&A](03-express-and-http-qa.md)** | Middleware onion execution, error-handling middleware signatures, Express 5 vs 4 breaking changes, Fastify schema compilation, HTTP/2 multiplexing, keep-alive. |
| **04** | **[Databases & Caching Q&A](04-databases-qa.md)** | Connection pooling mechanics, N+1 query problem, transaction isolation levels, SQL vs NoSQL trade-offs, Redis caching patterns (Cache-Aside, Write-Through), distributed locks. |
| **05** | **[Authentication & Security Q&A](05-auth-and-security-qa.md)** | JWT vs session cookies, asymmetric key signing (RS256 vs HS256), refresh token rotation and replay detection, CORS, CSRF, ReDoS, prototype pollution, SSRF mitigation. |
| **06** | **[Performance, Memory & Scaling Q&A](06-performance-and-scaling-qa.md)** | V8 garbage collection (Scavenge, Mark-Sweep), diagnosing heap memory leaks with heap snapshots, CPU profiling, Node.js clustering vs worker threads vs containers. |
| **07** | **[Architecture & System Design Q&A](07-architecture-and-system-design-qa.md)** | Hexagonal architecture, event-driven design, CQRS and Event Sourcing, API Gateway patterns, circuit breakers, idempotency key implementation in distributed systems. |
| **08** | **[Output-Based Questions & Code Tracing](08-output-based-questions.md)** | 15 tricky output prediction challenges testing microtask ordering, event loop phases, closure scope, unhandled promise rejections, and EventEmitter lifecycle. |
| **09** | **[Coding Challenges & Implementation Problems](09-coding-problems.md)** | Hand-coding an EventEmitter from scratch, asynchronous concurrency limiter, custom Transform stream, LRU cache with TTL, and promise retry with exponential backoff. |
| **10** | **[System Design & Behavioural Case Studies](10-behavioural-and-project-discussion.md)** | Staff-level architectural deep-dives: designing a real-time collaborative doc backend, scaling WebSocket clusters, handling high-volume webhooks, and post-mortem incident discussions. |
