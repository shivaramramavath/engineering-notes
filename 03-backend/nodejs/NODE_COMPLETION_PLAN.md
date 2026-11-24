# Node.js Track Completion Plan

> **Execution Blueprint**: Systematic batch-by-batch execution schedule to audit, complete, and extend the Node.js curriculum to 100% production readiness.

---

## Batch Breakdown

### Batch 0: Hygiene, Renaming & Root Architectural Setup
1. Standardize naming in `22-project-tooling/`: rename files with underscores (`01_...` $\rightarrow$ `01-...`) and rename `InversifyJS.md` $\rightarrow$ `06-inversifyjs.md`.
2. Standardize `05-http-web/06_status-codes.md` and `06-express/08_http-errors.md`.
3. Consolidate `13-testing/` duplicate files into clean `NN-kebab-case.md` format.
4. Resolve `01-fundamentals/03-module-system.md` vs `02-core-modules/11-module-system.md`.
5. Create root track documents: `INDEX.md`, `GLOSSARY.md`, `README.md`, `ROADMAP.md`, `CONTRIBUTING.md`.

### Batch 1: Setup, Fundamentals & Core Modules (00, 01, 02)
- `00-setup/`: Add native TypeScript type stripping (`strip-types`), Volta/fnm comparisons, and bootstrapping checklist.
- `01-fundamentals/`: Add Web Platform APIs, exit codes, process lifecycle, and error handling fundamentals.
- `02-core-modules/`: Write `02-core-modules/README.md`, expand streams, add `11-node-sqlite-and-test-runner.md`, `12-permissions-model.md`, and `13-single-executable-applications.md`.

### Batch 2: JS for Node, npm, HTTP & Frameworks (03, 04, 05, 06, 06b, 06c, 06d)
- `03-javascript-for-node/`: Add server-side async patterns, EventEmitter patterns, memory leaks.
- `04-npm-ecosystem/`: Add `package.json` deep-dive (`exports`/`imports`/`engines`), pnpm/yarn/bun comparison, and monorepo workspaces.
- `05-http-web/`: Add HTTP/2 & HTTP/3, keep-alive, streaming uploads, and Server-Sent Events (SSE).
- `06-express/`: Update for Express 5.0 (automatic async error propagation, `path-to-regexp` v8, `app.delete()`).
- Add sibling modules:
  - `06b-fastify/` (Fastify v5, schemas, plugins, hooks, high throughput).
  - `06c-hono-and-edge-runtimes/` (Hono, `@hono/node-server`, Web standards).
  - `06d-koa-and-framework-landscape/` (Koa onion model, framework comparison).

### Batch 3: Complete Database Suite (Module 07)
- Fix root files for `07-databases/` and nested `mongodb/` and `redis/` tracks (`README.md`, `ROADMAP.md`, `CONTRIBUTING.md`, `LICENSE`).
- Expand `07-databases/postgresql/`:
  - `01-pg-driver-and-connection-pooling.md`
  - `02-queries-parameterisation-and-sql-injection.md`
  - `03-transactions-and-isolation-levels.md`
  - `04-migrations-with-node-pg-migrate-and-drizzle.md`
  - `05-query-builders-kysely-and-knex.md`
  - `06-orms-prisma-drizzle-typeorm-compared.md`
  - `07-pagination-and-n-plus-one-optimization.md`
  - `08-json-and-full-text-search-in-postgres.md`
  - `09-listen-notify-and-logical-replication.md`
  - `10-testing-with-real-databases-and-testcontainers.md`
  - `README.md`
- Add other database notes in `07-databases/`:
  - `01-mysql-with-node.md`
  - `02-sqlite-with-node.md`
  - `03-dynamodb-from-node.md`
  - `04-elasticsearch-and-opensearch.md`
  - `05-database-selection-guide.md`

### Batch 4: Auth, API Dev, Architecture & Queues (08, 09, 10, 11, 12)
- `08-authentication-security/`: Password hashing (Argon2 vs bcrypt), JWT/jose, Passkeys/WebAuthn, CSRF/SSRF, OWASP for Node.
- `09-api-development/`: OpenAPI-first, GraphQL (Yoga/Pothos), gRPC, tRPC, Idempotency keys, Webhooks.
- `10-architecture/`: Clean Architecture, Hexagonal, CQRS/Event Sourcing, Dependency Injection (InversifyJS/tsyringe).
- `11-async-processing/`: BullMQ v5, RabbitMQ (amqplib), KafkaJS, Outbox pattern, distributed cron schedulers.
- `12-realtime/`: Socket.IO Redis adapter, raw `ws` server, WebRTC signaling, realtime backpressure.

### Batch 5: Testing, Observability & Performance (13, 14, 15, 16, 17, 18, 19)
- `13-testing/`: Standardized suite (`00-setup` through `10-flaky-tests`), `node:test`, Vitest, Supertest, Testcontainers.
- `14-logging-observability/`: Pino structured logging, Correlation IDs, OpenTelemetry tracing, Prometheus metrics.
- `15-performance/`: **Write all 5 unwritten notes** (`01-event-loop-performance`, `02-profiling`, `03-database-opt`, `04-load-testing`, and `README.md`).
- `16-production/`: Graceful shutdown, PM2/systemd, Docker standalone multi-stage builds.
- `17-typescript/`: Node 22 native type stripping, tsconfig bundler resolution, typed ORMs.
- `18-design-patterns/`: Add Facade, Command, State, Circuit Breaker, Retry-Backoff, and Repository patterns.
- `19-system-design/`: Add E-commerce backend, Payment idempotency, Video upload pipeline, and Multi-tenant SaaS.

### Batch 6: AWS SDK Projects (20-aws-sdk/05-projects)
- Build out all 3 projects completely:
  - `01-s3-upload-api`
  - `02-serverless-api`
  - `03-node-app-on-ecs`
- Write `20-aws-sdk/README.md` and `20-aws-sdk/05-projects/README.md`.

### Batch 7: 16 Reference Projects (Module 21-projects Part 1 - Projects 01-08)
- `01-rest-api-with-express-postgres/`
- `02-auth-service/`
- `03-url-shortener-with-redis/`
- `04-job-queue-service-with-bullmq/`
- `05-realtime-chat-server/`
- `06-file-upload-service/`
- `07-notification-service/`
- `08-ecommerce-api/`

### Batch 8: Reference Projects (Module 21-projects Part 2 - Projects 09-16 + Catalog README)
- `09-webhook-receiver-and-dispatcher/`
- `10-rate-limiter-service/`
- `11-api-gateway-lite/`
- `12-graphql-api/`
- `13-event-sourced-bank/`
- `14-multi-tenant-saas-backend/`
- `15-log-ingestion-pipeline/`
- `16-cli-tool-in-node/`
- `21-projects/README.md`

### Batch 9: Interview Prep (Module 23) & Cheatsheets (Module 24)
- `23-interview/`: 11 Senior & Staff interview guides + `23-interview/README.md`.
- `24-cheatsheets/`: 14 high-density quick reference cards + `24-cheatsheets/README.md`.

### Batch 10: Final Verification, Root Tree & Final Report
- Verification check across all notes.
- Regenerate root `tree.txt`.
- Write `NODE_FINAL_REPORT.md`.
