# Node.js Backend Production Reference Projects

> 16 enterprise-grade, end-to-end reference applications architected for Node.js 22 LTS and Node.js 24. Every project provides complete production source files, architectural diagrams, domain designs, data models, and automated test suites directly inside the Markdown specification.

---

## Reference Projects Catalog

| # | Project Name | Tech Stack | Architectural Highlights |
| :---: | :--- | :--- | :--- |
| **01** | **[REST API with Express 5 & PostgreSQL](01-rest-api-with-express-postgres/README.md)** | Express 5, `pg`, Drizzle ORM, Zod | Layered architecture, connection pooling, transactional unit-of-work, RFC 7807 Problem Details, Vitest + Testcontainers integration testing. |
| **02** | **[Production Authentication Service](02-auth-service/README.md)** | Express 5, Argon2, Jose (JWT), Redis | Asymmetric RS256/EdDSA tokens, sliding refresh token rotation with replay-attack detection, Redis session store, secure cookies, brute-force limiting. |
| **03** | **[High-Throughput URL Shortener](03-url-shortener-with-redis/README.md)** | Fastify 5, Redis 7, PostgreSQL | Base62 encoding, sub-millisecond Redis caching, distributed Snowflake IDs, real-time analytics with HyperLogLog, 301/302 redirects. |
| **04** | **[Distributed Job Queue with BullMQ](04-job-queue-service-with-bullmq/README.md)** | BullMQ v5, Redis 7, Express 5 | Exponential backoff with full jitter, Dead Letter Queue (DLQ), priority queues, rate-limited worker pools, parent-child flows, graceful SIGTERM worker shutdown. |
| **05** | **[Real-Time WebSocket Chat Server](05-realtime-chat-server/README.md)** | `ws`, Redis Pub/Sub, Jose | Multi-node horizontal scaling via Redis Pub/Sub, HTTP upgrade JWT authentication, channel multiplexing, backpressure monitoring (`ws.bufferedAmount`), ping/pong heartbeats. |
| **06** | **[Streaming File Upload & Media Processor](06-file-upload-service/README.md)** | Express 5, Busboy, Sharp, File-Type | Constant-memory multipart streaming (<50MB heap under load), magic-number MIME sniffing, automated multi-resolution WebP/AVIF generation, S3/disk target. |
| **07** | **[Multi-Channel Notification Engine](07-notification-service/README.md)** | Express 5, BullMQ, Handlebars, MJML | Email (SES/SendGrid), SMS (Twilio), Web Push (FCM), responsive MJML templates, automatic provider failover, user quiet-hours enforcement. |
| **08** | **[Enterprise E-Commerce & Checkout API](08-ecommerce-api/README.md)** | Express 5, PostgreSQL, Redis, Stripe | Redlock distributed inventory locking, Stripe PaymentIntents with raw cryptographic webhook verification, ACID checkout transactions, BullMQ order fulfillment. |
| **09** | **[Reliable Webhook Receiver & Dispatcher](09-webhook-receiver-and-dispatcher/README.md)** | Express 5, BullMQ, `node:crypto` | HMAC-SHA256 signature verification via `timingSafeEqual`, timestamp replay attack guards, outbound delivery workers with exponential retries and circuit breakers. |
| **10** | **[High-Scale Distributed Rate Limiter](10-rate-limiter-service/README.md)** | Fastify 5, Redis Lua Scripts | Atomic sliding window counter & token bucket via Redis Lua (`EVALSHA`), standardized IETF `RateLimit-*` headers, multi-tiered quotas, fail-open in-memory fallback. |
| **11** | **[Lightweight API Gateway & Reverse Proxy](11-api-gateway-lite/README.md)** | Fastify 5, `@fastify/http-proxy` | Path-based stream proxying, edge JWT token verification, internal header propagation (`X-User-Id`), correlation ID injection (`X-Request-Id`), keep-alive pooling. |
| **12** | **[Production GraphQL API with DataLoader](12-graphql-api/README.md)** | GraphQL Yoga v5, DataLoader, `pg` | N+1 query elimination via DataLoader microtask batching and request-level caching, Relay cursor pagination, query depth limiting against DoS attacks. |
| **13** | **[Event Sourced Banking Ledger & CQRS](13-event-sourced-bank/README.md)** | Express 5, PostgreSQL | Append-only event store, Command Query Responsibility Segregation (CQRS), aggregate rehydration, optimistic concurrency control (`expected_version`), projections. |
| **14** | **[Multi-Tenant SaaS Backend with RLS](14-multi-tenant-saas-backend/README.md)** | Express 5, PostgreSQL RLS | Kernel-level tenant isolation via PostgreSQL Row-Level Security (`SET LOCAL app.current_tenant_id`), automated context propagation with `AsyncLocalStorage`. |
| **15** | **[High-Throughput Log Ingestion Pipeline](15-log-ingestion-pipeline/README.md)** | Express 5, Node Streams, PostgreSQL | NDJSON chunked streaming, stream backpressure handling, micro-batch aggregation flusher (1,000 logs or 250ms), high-speed bulk database insertion. |
| **16** | **[Production Developer CLI Tool](16-cli-tool-in-node/README.md)** | Commander, `@inquirer/prompts`, Ora | Interactive wizard, TTY piped stdin stream support, XDG directory config storage, POSIX exit codes, standalone binary distribution via Node.js Single Executable Applications (SEA). |

---

## Architectural Principles Across All Projects

1. **Strict Markdown Encapsulation**: All configurations, schemas, and runnable code blocks are contained within the `README.md` file of each project directory, adhering to the repository specification.
2. **Standardized Error Responses**: HTTP APIs implement RFC 7807 Problem Details (`application/problem+json`).
3. **Resilience & Graceful Shutdown**: Every service implements explicit signal handling for `SIGTERM` and `SIGINT`, terminating database pools, HTTP listeners, and worker loops without task loss.
4. **Node.js 22 LTS / 24+ Compatibility**: Built with native ECMAScript Modules (`import`/`export`), modern standard globals (`fetch`, `crypto`, `performance`), and TypeScript 5.7+ compatibility.
