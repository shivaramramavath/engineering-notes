# Node.js Backend Curriculum Index

> Complete cross-module catalog of concepts, core APIs, architectural patterns, and production systems in the Node.js engineering track.

---

## Master Module Index

| Module | Category | Primary Focus & Deliverables |
| :--- | :--- | :--- |
| **[00-setup](./00-setup/README.md)** | Setup & Environment | Node versions, version managers (nvm/fnm/Volta), native TypeScript strip-types, debugging, bootstrapping. |
| **[01-fundamentals](./01-fundamentals/README.md)** | Core Architecture | Event loop phases (libuv), V8 engine, thread pool, ESM vs CJS, process lifecycle, error handling. |
| **[02-core-modules](./02-core-modules/README.md)** | Standard Library | Streams, Buffer, `fs/promises`, path, HTTP/HTTPS, crypto, cluster, worker threads, `node:sqlite`, SEA, permissions. |
| **[03-javascript-for-node](./03-javascript-for-node/README.md)** | Server JS | Server-side async patterns, EventEmitter, memory leak mitigation, V8 garbage collection. |
| **[04-npm-ecosystem](./04-npm-ecosystem/README.md)** | Package Management | `package.json` (`exports`/`imports`/`engines`), pnpm/yarn/bun, monorepos (Turborepo/Nx), provenance. |
| **[05-http-web](./05-http-web/README.md)** | Web Protocols | HTTP methods, status codes, headers, cookies, CORS, HTTP/2 & 3, keep-alive, streaming uploads, SSE. |
| **[06-express](./06-express/README.md)** | Express Framework | Express 5.0 migration, routing, middleware, async error handling, security headers, Supertest testing. |
| **[06b-fastify](./06b-fastify/README.md)** | Fastify Framework | Fastify v5, JSON schemas, plugins, lifecycle hooks, serialization, high-performance APIs. |
| **[06c-hono-and-edge-runtimes](./06c-hono-and-edge-runtimes/README.md)** | Modern Web Standards | Hono framework, `@hono/node-server`, Web Request/Response, edge runtime portability. |
| **[06d-koa-and-framework-landscape](./06d-koa-and-framework-landscape/README.md)** | Framework Comparison | Koa onion middleware, Restify, Fastify vs Express vs Hono benchmarks. |
| **[07-databases](./07-databases/README.md)** | Data Persistence | PostgreSQL (`pg`, Drizzle, Prisma, Kysely), MongoDB (18 sub-modules), Redis (21 sub-modules), MySQL, SQLite. |
| **[08-authentication-security](./08-authentication-security/README.md)** | Security & Auth | Argon2 vs bcrypt, JWT/jose, Passkeys/WebAuthn, RBAC/ABAC, HMAC, OWASP Top 10 for Node.js, CSP. |
| **[09-api-development](./09-api-development/README.md)** | API Engineering | REST design, OpenAPI, pagination, idempotency keys, Webhooks, GraphQL (Yoga/Pothos), gRPC, tRPC. |
| **[10-architecture](./10-architecture/README.md)** | System Architecture | Layered architecture, Clean Architecture, Hexagonal, CQRS, Event-Driven, InversifyJS/tsyringe DI. |
| **[11-async-processing](./11-async-processing/README.md)** | Queues & Events | BullMQ v5, RabbitMQ (amqplib), KafkaJS, Outbox pattern, idempotent consumers, distributed cron. |
| **[12-realtime](./12-realtime/README.md)** | Realtime & WebSockets | Socket.IO, raw `ws` server, Redis adapter cluster, presence, WebRTC signaling, backpressure. |
| **[13-testing](./13-testing/README.md)** | Testing & Quality | Testing fundamentals, unit & integration, `node:test`, Vitest, Supertest, Testcontainers, mocking. |
| **[14-logging-observability](./14-logging-observability/README.md)** | Observability | Pino structured logging, correlation IDs, OpenTelemetry tracing, Prometheus metrics, health checks. |
| **[15-performance](./15-performance/README.md)** | Performance & Profiling | Event loop lag, clinic/0x profiling, heap snapshots, database query tuning, load testing with autocannon. |
| **[16-production](./16-production/README.md)** | Production Engineering | Graceful shutdown, PM2/systemd, Docker multi-stage builds, Nginx reverse proxy, 12-factor apps. |
| **[17-typescript](./17-typescript/README.md)** | TypeScript with Node | Node 22 native type stripping, tsconfig bundler settings, typed Express/Fastify, typed ORMs. |
| **[18-design-patterns](./18-design-patterns/README.md)** | Design Patterns | Creational, Structural, Behavioral patterns (Factory, Observer, Middleware, Circuit Breaker, Unit of Work). |
| **[19-system-design](./19-system-design/README.md)** | System Design | High-scale backend architectures: URL shortener, Notification engine, E-commerce flash sales, SaaS. |
| **[20-aws-sdk](./20-aws-sdk/README.md)** | Cloud Services | AWS SDK v3, S3, DynamoDB, SQS, SNS, SES, Bedrock, Lambda handlers, 3 reference projects. |
| **[21-projects](./21-projects/README.md)** | Reference Projects | 16 enterprise production applications with complete architectures, tests, and Docker. |
| **[22-project-tooling](./22-project-tooling/README.md)** | Developer Tooling | Project structure, environment variables, git hooks, linting/formatting, InversifyJS. |
| **[23-interview](./23-interview/README.md)** | Technical Interview Prep | 11 Senior & Staff interview guides covering event loop, streams, databases, and system design. |
| **[24-cheatsheets](./24-cheatsheets/README.md)** | Cheatsheets & Reference | 14 high-density syntax and architecture cheat sheets. |
