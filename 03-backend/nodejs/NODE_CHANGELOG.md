# Node.js Track Changelog

All notable changes, structural migrations, note completions, and project implementations across `03-backend/nodejs` are documented in this file.

---

## [2026-11-24] - Full Curriculum Overhaul & 100% Completion Delivery

### Batch 0: Hygiene, Renamings, Templates & Root Files
- **Renamed**: Standardized files in `22-project-tooling/`: `01_...` $\rightarrow$ `01-...`, `InversifyJS.md` $\rightarrow$ `06-inversifyjs.md`.
- **Renamed**: Harmonized `05-http-web/06_status-codes.md` $\rightarrow$ `06-status-codes-reference.md`.
- **Renamed**: Harmonized `06-express/08_http-errors.md` $\rightarrow$ `08-http-errors-and-problem-details.md`.
- **Resolved Module System Duplication**: Transformed `01-fundamentals/03-module-system.md` into `03-esm-vs-commonjs-in-node.md`; repurposed `02-core-modules/11-module-system.md` into `11-node-sqlite-and-test-runner.md`.
- **Testing Consolidation**: Consolidated `13-testing/` duplicate prefixes into clean sequence (01 to 10).
- **Templates**: Authoritative `_templates/concept-template.md` (13 sections) and `_templates/project-template.md` (5 sections).
- **Root Files**: Created `README.md`, `INDEX.md`, `GLOSSARY.md`, `ROADMAP.md`, `CONTRIBUTING.md`, `LICENSE`, `NODE_FACTS.md`, `NODE_DECISIONS.md`, `NODE_AUDIT.md`, `NODE_COMPLETION_PLAN.md`, `NODE_PROGRESS.md`.
- **Commit**: `569666a`

### Batch 1: Modules 00-setup, 01-fundamentals, 02-core-modules
- **00-setup**: Added `03-node-versions-and-managers.md`, `04-running-typescript-in-node.md` (native type stripping), `05-editor-debugger-and-inspector.md`, `06-project-bootstrapping-checklist.md`, and `README.md`.
- **01-fundamentals**: Added `05-process-lifecycle-and-exit-codes.md`, `06-error-handling-fundamentals.md`, and `README.md`.
- **02-core-modules**: Added `11-node-sqlite-and-test-runner.md`, `12-permissions-model.md`, `13-single-executable-applications.md`, `14-diagnostics-channel-and-asynclocalstorage.md`, and `README.md`.
- **Commit**: `41dad4b`

### Batch 2: Modules 03, 04, 05, 06, 06b-fastify, 06c-hono, 06d-koa
- **03-javascript-for-node**: Added `07-server-async-patterns-and-event-emitter.md`, `08-memory-leaks-and-heap-profiling.md`, and `README.md`.
- **04-npm-ecosystem**: Added `04-package-json-deep-dive-exports-imports.md` (subpath imports `#`), `05-npm-pnpm-yarn-bun-and-workspaces.md`, and `README.md`.
- **05-http-web**: Added `07-http2-http3-and-keepalive.md`, `08-server-sent-events-and-streaming.md`, and `README.md`.
- **06-express**: Added `09-express-5-migration-and-internals.md` (GA Oct 2024), `10-express-security-middleware-and-clustering.md`, and `README.md`.
- **06b-fastify**: Created complete module: `01-getting-started-and-plugins.md`, `02-schemas-validation-and-serialization.md`, `03-lifecycle-hooks-and-decorators.md`, and `README.md`.
- **06c-hono-and-edge-runtimes**: Created complete module: `01-hono-fundamentals-and-node-server.md`, `02-web-standards-and-edge-deployment.md`, and `README.md`.
- **06d-koa-and-framework-landscape**: Created complete module: `01-koa-onion-architecture.md`, `02-framework-selection-and-benchmarks.md`, and `README.md`.
- **Commit**: `1f73155`

### Batch 3: Module 07 Databases & Sub-tracks
- **mongodb / redis**: Created missing root documentation suites (`README.md`, `ROADMAP.md`, `CONTRIBUTING.md`, `LICENSE`).
- **postgresql**: Expanded empty track into 10 production notes:
  - `01-connection-and-queries.md` (`pg.Pool`), `02-transactions-and-indexing.md`, `03-migrations-with-node-pg-migrate-and-drizzle.md`, `04-query-builders-kysely-and-knex.md`, `05-orms-prisma-drizzle-typeorm-compared.md`, `06-pagination-and-n-plus-one.md`, `07-json-and-full-text-search.md`, `08-listen-notify-and-replication.md`, `09-testing-with-real-databases-and-testcontainers.md`, `README.md`.
- **07-databases**: Added `01-mysql-with-node.md`, `02-sqlite-with-node.md` (`node:sqlite`), `03-dynamodb-from-node.md`, `04-elasticsearch-and-opensearch.md`, `05-database-selection-guide.md`, and `README.md`.
- **Commit**: `d4ba3d7`

### Batch 4: Modules 08 Auth, 09 API Dev, 10 Architecture, 11 Queues, 12 Realtime
- **08-authentication-security**: Added `09-passkeys-and-webauthn.md`, `10-rbac-abac-and-policy-enforcement.md`, `11-api-keys-and-hmac-signatures.md`, and `README.md`.
- **09-api-development**: Added `08-graphql-with-node-apollo-and-yoga.md`, `09-trpc-and-grpc-in-node.md`, `10-openapi-first-and-sdk-generation.md`, and `README.md`.
- **10-architecture**: Added `06-hexagonal-architecture-and-ports-adapters.md`, `07-cqrs-and-event-sourcing.md`, `08-domain-driven-design-with-node.md`, and `README.md`.
- **11-async-processing**: Added `05-rabbitmq-and-amqplib.md`, `06-outbox-and-saga-patterns.md`, `07-idempotent-consumers-and-stream-processing.md`, and `README.md`.
- **12-realtime**: Added `04-ws-library-from-scratch.md`, `05-webrtc-signalling-server.md`, `06-backpressure-in-realtime-systems.md`, and `README.md`.
- **Commit**: `5150bd6`

### Batch 5: Modules 13 Testing, 15 Performance, 18 Patterns, 19 System Design
- **13-testing**: Added `10-node-test-runner.md` (`node:test`) and consolidated `README.md`.
- **15-performance**: Wrote all 5 previously 0-byte notes: `01-event-loop-performance.md`, `02-profiling-and-memory-leaks.md`, `03-database-optimization.md`, `04-load-balancing-and-testing.md`, and `README.md`.
- **18-design-patterns**: Added `10-circuit-breaker-and-retry.md`, `11-repository-and-unit-of-work.md`, `12-facade-and-command-patterns.md`, and `README.md`.
- **19-system-design**: Added `08-ecommerce-flash-sale-system.md`, `09-payment-system-and-idempotency.md`, `10-multi-tenant-saas-backend.md`, and `README.md`.
- **Commit**: `a361c9b`

### Batch 6: Module 20 AWS SDK (Projects Skeletons & READMEs)
- **20-aws-sdk**: Updated module `README.md` and `05-projects/README.md`.
- **Cleaned Non-MD Files**: Removed 0-byte `.json` and `.gitkeep` files from project directories to adhere strictly to Markdown-only repository constraint.
- **Project 01**: `05-projects/01-s3-upload-api/README.md` (Express 5, S3 presigned URLs, streaming multipart upload via `@aws-sdk/lib-storage`, LocalStack, Vitest).
- **Project 02**: `05-projects/02-serverless-api/README.md` (AWS Lambda `nodejs22.x`, API Gateway HTTP API v2, DynamoDB DocumentClient Single-Table Design, SAM IaC template).
- **Project 03**: `05-projects/03-node-app-on-ecs/README.md` (ECS Fargate microservice, ALB dual healthchecks, graceful SIGTERM shutdown with socket draining, Secrets Manager with TTL cache, Terraform HCL, Distroless Docker).
- **Commit**: `bd6d635`

### Batch 7: Module 21 Reference Projects Part 1 (Projects 01 - 08)
- Created `21-projects/` folder filling track numbering gap.
- **Project 01**: `01-rest-api-with-express-postgres/README.md` (Express 5, `pg`, Drizzle ORM, Zod, Testcontainers).
- **Project 02**: `02-auth-service/README.md` (Argon2, `jose` RS256/EdDSA, Redis sliding token rotation & replay detection).
- **Project 03**: `03-url-shortener-with-redis/README.md` (Fastify 5, Base62, sub-millisecond Redis caching, HyperLogLog analytics).
- **Project 04**: `04-job-queue-service-with-bullmq/README.md` (BullMQ v5, Redis 7, exponential retries, DLQ, rate limiting).
- **Project 05**: `05-realtime-chat-server/README.md` (`ws`, Redis Pub/Sub cluster, HTTP upgrade JWT auth, backpressure checks).
- **Project 06**: `06-file-upload-service/README.md` (Busboy streaming, Sharp WebP/AVIF generation, magic-number MIME sniffing).
- **Project 07**: `07-notification-service/README.md` (Multi-channel dispatch: SES, Twilio, FCM, MJML templates, failover).
- **Project 08**: `08-ecommerce-api/README.md` (Stripe PaymentIntents, raw webhook signature verification, Redlock inventory locking).
- **Commit**: `4425d14`

### Batch 8: Module 21 Reference Projects Part 2 (Projects 09 - 16 + Catalog)
- **Project 09**: `09-webhook-receiver-and-dispatcher/README.md` (HMAC-SHA256 constant-time verification, timestamp replay tolerance, BullMQ retry dispatcher).
- **Project 10**: `10-rate-limiter-service/README.md` (Fastify 5, Redis Lua sliding window counter, IETF `RateLimit-*` headers).
- **Project 11**: `11-api-gateway-lite/README.md` (Fastify 5 `@fastify/http-proxy`, edge JWT auth, `X-Request-Id` correlation).
- **Project 12**: `12-graphql-api/README.md` (GraphQL Yoga v5, DataLoader N+1 batching & caching, query complexity limits).
- **Project 13**: `13-event-sourced-bank/README.md` (Append-only PostgreSQL event store, CQRS, optimistic locking via expected version).
- **Project 14**: `14-multi-tenant-saas-backend/README.md` (PostgreSQL Row-Level Security `SET LOCAL app.current_tenant_id`, `AsyncLocalStorage`).
- **Project 15**: `15-log-ingestion-pipeline/README.md` (NDJSON chunked streaming, backpressure handling, micro-batch aggregation).
- **Project 16**: `16-cli-tool-in-node/README.md` (Commander, Inquirer prompts, Ora spinners, Single Executable Application SEA packaging).
- **Catalog**: `21-projects/README.md` (Exhaustive index of all 16 projects).
- **Commit**: `7b1b2d8`

### Batch 9: Technical Interview Prep (Module 23) & Cheatsheets (Module 24)
- **Module 23 (`23-interview/`)**:
  - `00-fundamentals-qa.md`, `01-event-loop-and-async-qa.md`, `02-streams-and-buffers-qa.md`, `03-express-and-http-qa.md`, `04-databases-qa.md`, `05-auth-and-security-qa.md`, `06-performance-and-scaling-qa.md`, `07-architecture-and-system-design-qa.md`, `08-output-based-questions.md`, `09-coding-problems.md`, `10-behavioural-and-project-discussion.md`, `README.md`.
  - **Commit**: `7cd00ff`
- **Module 24 (`24-cheatsheets/`)**:
  - `00-core-modules.md`, `01-npm-and-package-json.md`, `02-express.md`, `03-fastify.md`, `04-streams-and-buffers.md`, `05-http-status-and-headers.md`, `06-mongodb.md`, `07-postgres-with-node.md`, `08-redis-with-ioredis.md`, `09-auth-and-jwt.md`, `10-docker-and-compose.md`, `11-testing.md`, `12-debugging-and-profiling.md`, `13-common-errors.md`, `README.md`.
  - **Commit**: `7945bd0`

### Batch 10: Final Verification, Root Tree & Final Report
- Regenerated root `tree.txt` reflecting the complete track additions.
- Created `NODE_FINAL_REPORT.md` documenting architecture, metrics, and verification results.
