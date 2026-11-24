# Node.js Backend Engineering Mastery Roadmap

> Structured career learning paths for engineering progression: from JavaScript transition to Staff / Principal Software Architect on Node.js.

---

## Visual Learning Trajectories

```text
  Level 1: Core Runtime & Standard Library
  [00-setup] ───► [01-fundamentals] ───► [02-core-modules] ───► [03-javascript-for-node]
                                                                          │
  Level 2: Package Ecosystem, Protocols & Web Frameworks                  ▼
  [06d-koa] ◄─── [06c-hono] ◄─── [06b-fastify] ◄─── [06-express] ◄─── [05-http-web] ◄─── [04-npm]
        │
        ▼
  Level 3: Multi-Database Persistence, Auth & APIs
  [07-databases] ───► [08-auth-security] ───► [09-api-dev] ───► [10-architecture]
                                                                        │
  Level 4: Queues, Realtime, Quality & Observability                    ▼
  [14-observability] ◄─── [13-testing] ◄─── [12-realtime] ◄─── [11-async-processing]
        │
        ▼
  Level 5: Performance, Production, Patterns & Cloud
  [15-performance] ───► [16-production] ───► [17-typescript] ───► [18-patterns] ───► [19-system-design] ───► [20-aws-sdk]
                                                                                                                   │
  Level 6: Reference Projects, Interview Prep & Cheatsheets                                                        ▼
  [24-cheatsheets] ◄─── [23-interview] ◄─── [22-project-tooling] ◄─── [21-projects]
```

---

## Track 1: Foundations (Junior / Transitioning from Frontend)
*Goal: Master asynchronous runtime mechanics, event loop phases, and core standard modules without third-party frameworks.*

1. **Environment & Tooling**: Node version managers, native TypeScript type stripping (`node app.ts`), and bootstrapping ([00-setup](./00-setup/README.md)).
2. **Event Loop Internals**: Libuv event loop phases (Timers, Poll, Check), threadpool, process lifecycle, and exit codes ([01-fundamentals](./01-fundamentals/README.md)).
3. **Core Standard Library**: Streams, Buffer, `fs/promises`, path, crypto, cluster, worker threads, and `node:sqlite` ([02-core-modules](./02-core-modules/README.md)).
4. **Server JS Mechanics**: EventEmitter, server-side async patterns, memory leak diagnosis ([03-javascript-for-node](./03-javascript-for-node/README.md)).
5. **NPM & Dependency Hygiene**: Modern `package.json` (`exports`/`imports`/`engines`), pnpm workspaces, and package security ([04-npm-ecosystem](./04-npm-ecosystem/README.md)).

---

## Track 2: Backend Job-Ready (Mid-Level API Engineer)
*Goal: Build scalable, secure HTTP APIs with Express 5, Fastify, and PostgreSQL.*

1. **Web Protocols**: HTTP status codes, headers, cookies, CORS, keep-alive, streaming uploads, and SSE ([05-http-web](./05-http-web/README.md)).
2. **Express 5 & Fastify**: Express 5 async error handling, Fastify v5 JSON schemas, and Hono Web standards ([06-express](./06-express/README.md), [06b-fastify](./06b-fastify/README.md), [06c-hono-and-edge-runtimes](./06c-hono-and-edge-runtimes/README.md)).
3. **PostgreSQL & Database Persistence**: Connection pooling with `pg`, parameterized queries, Drizzle ORM, Prisma v6, Kysely, and Redis caching ([07-databases](./07-databases/README.md)).
4. **Authentication & Security**: Argon2 password hashing, Jose JWTs, Passkeys/WebAuthn, RBAC, and OWASP Top 10 defenses ([08-authentication-security](./08-authentication-security/README.md)).
5. **API Design Standards**: REST best practices, OpenAPI schemas, idempotency keys, and Webhooks ([09-api-development](./09-api-development/README.md)).

---

## Track 3: Realtime, Queues & Distributed Systems (Senior Engineer)
*Goal: Build high-concurrency event-driven architectures, distributed queues, and resilient background jobs.*

1. **Architecture & Patterns**: Clean Architecture, Hexagonal boundaries, CQRS/Event Sourcing, and InversifyJS DI ([10-architecture](./10-architecture/README.md)).
2. **Asynchronous Processing**: Distributed job queues with BullMQ v5, RabbitMQ (amqplib), KafkaJS, and Outbox pattern ([11-async-processing](./11-async-processing/README.md)).
3. **Realtime WebSockets**: Socket.IO with Redis cluster adapter, raw `ws` server, WebRTC signaling, and backpressure ([12-realtime](./12-realtime/README.md)).
4. **Testing Rigor**: Native `node:test`, Vitest, Supertest, Testcontainers, and mocking with MSW ([13-testing](./13-testing/README.md)).
5. **Cloud Services with AWS SDK v3**: S3 uploads, DynamoDB, SQS, SNS, SES, Bedrock, and Lambda handlers ([20-aws-sdk](./20-aws-sdk/README.md)).

---

## Track 4: Performance, Production & Staff Architecture
*Goal: Keep event loop lag < 10ms, optimize memory, self-host containers, and pass staff technical interviews.*

1. **Observability & Tracing**: Pino structured logging, OpenTelemetry distributed tracing, and Prometheus metrics ([14-logging-observability](./14-logging-observability/README.md)).
2. **Profiling & Performance**: Event loop profiling, clinic/0x flamegraphs, heap snapshots, and autocannon load tests ([15-performance](./15-performance/README.md)).
3. **Production Engineering**: Graceful shutdown, PM2, multi-stage Docker builds, and zero-downtime deploys ([16-production](./16-production/README.md)).
4. **System Design Scenarios**: High-throughput URL shortener, Notification engine, Flash sale inventory, and Video processing pipeline ([19-system-design](./19-system-design/README.md)).
5. **16 Production Reference Projects**: Fullstack portfolio applications ([21-projects](./21-projects/README.md)).
6. **Technical Interview Mastery**: Senior & Staff interview question guides and output debugging challenges ([23-interview](./23-interview/README.md)).
