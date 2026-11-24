# Node.js Technical Cheatsheets & Quick References

> High-density, production-ready syntax reference cards for backend engineers working with Node.js 22 LTS, Node.js 24, databases, containerization, and modern cloud deployment.

---

## Cheatsheets Catalog

| # | Cheatsheet | Description & Key Topics |
| :---: | :--- | :--- |
| **00** | **[Node.js Core Modules](00-core-modules.md)** | `node:fs`, `node:path`, `node:crypto`, `node:os`, `node:util`, `node:sqlite`, `node:test`, `node:async_hooks`. |
| **01** | **[npm & package.json Deep Dive](01-npm-and-package-json.md)** | `exports`/`imports` subpath imports (`#`), dependency types, semantic versioning, lockfiles, workspace commands. |
| **02** | **[Express 5.0 Reference](02-express.md)** | Route definitions, async error propagation, built-in middleware, router composition, request/response APIs. |
| **03** | **[Fastify 5.0 Reference](03-fastify.md)** | Plugin lifecycle, `find-my-way` routing, JSON Schema validation (`ajv`), lifecycle hooks (`onRequest`, `preHandler`). |
| **04** | **[Streams & Buffers](04-streams-and-buffers.md)** | `pipeline()`, `Readable.from()`, `Transform`, `Buffer.alloc()`, `Buffer.concat()`, backpressure handling. |
| **05** | **[HTTP Status Codes & Security Headers](05-http-status-and-headers.md)** | Standard status codes, RFC 7807 Problem Details, Helmet security headers (CSP, HSTS, X-Content-Type-Options). |
| **06** | **[MongoDB & Mongoose](06-mongodb.md)** | Connection options, schema definitions, indexing, aggregation pipeline stages, transactions. |
| **07** | **[PostgreSQL with Node.js (`pg` & Drizzle)](07-postgres-with-node.md)** | Connection pooling (`pg.Pool`), parameterized queries, transactions, Drizzle ORM schema and queries. |
| **08** | **[Redis with ioredis](08-redis-with-ioredis.md)** | Data structures (Strings, Hashes, Lists, Sets, Sorted Sets), Pub/Sub, Lua script execution, pipelining. |
| **09** | **[Authentication, Cookies & JWT](09-auth-and-jwt.md)** | Argon2 hashing, `jose` token signing and verification, secure HTTP-only cookie configuration, CORS options. |
| **10** | **[Docker & Compose for Node.js](10-docker-and-compose.md)** | Multi-stage Dockerfile, Distroless images, `.dockerignore`, Docker Compose services and healthchecks. |
| **11** | **[Testing (node:test & Vitest)](11-testing.md)** | Test runner syntax, assertions, mock functions, mocking modules, lifecycle hooks (`before`, `after`). |
| **12** | **[Debugging & Profiling](12-debugging-and-profiling.md)** | Chrome DevTools inspector (`--inspect`), memory snapshots, CPU flamegraphs, event loop monitoring. |
| **13** | **[Common Node.js Errors & Fixes](13-common-errors.md)** | `ERR_MODULE_NOT_FOUND`, `ERR_REQUIRE_ESM`, `EADDRINUSE`, `ECONNRESET`, `ERR_HTTP_HEADERS_SENT`, unhandled rejections. |
