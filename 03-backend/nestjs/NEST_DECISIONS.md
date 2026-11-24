# NestJS Architecture Decision Records (ADRs)

> Engineering and architectural decisions made during the audit, completion, and standardization of the NestJS track.

---

## ADR 001: Baseline Version Standardization (NestJS 11+ / Node.js 22 LTS)
- **Status**: Accepted
- **Context**: NestJS 11 drops support for Node.js 18 (EOL April 2025) and enforces Node.js >= 20.0.0. Express 5 is now the default underlying HTTP platform adapter, introducing stricter path matching (named wildcards `*splat`). Caching has transitioned to Keyv (`cache-manager` v6).
- **Decision**: Standardize on **NestJS 11.x** with **Node.js 22 LTS** as the reference execution baseline. Explicitly tag features and differences between Nest 10 and Nest 11 throughout all notes.

---

## ADR 002: Stray README Normalization
- **Status**: Accepted
- **Context**: 9 folders contained stray duplicate README files (`README (1).md` through `README (7).md`), while several target folders lacked `README.md` or contained a 0-byte placeholder (e.g. `03-configuration`).
- **Decision**: Rename all `README (N).md` files into canonical `README.md` in each respective folder. In cases of duplicate files, preserve existing syllabus content and ensure every folder has an index.

---

## ADR 003: Caching Modernization with Keyv
- **Status**: Accepted
- **Context**: Legacy `@nestjs/cache-manager` examples rely on `cache-manager-redis-store` and `store: redisStore`, which are deprecated and broken in `cache-manager` v6.
- **Decision**: Update all caching notes, interview questions, and reference projects to use the modern Keyv architecture (`stores: [createKeyv('redis://...')]` via `@keyv/redis`), noting the v5 $\rightarrow$ v6 migration differences.

---

## ADR 004: Modern Database ORM Expansion (Drizzle & MikroORM)
- **Status**: Accepted
- **Context**: The existing database track only had notes for TypeORM, Prisma, and Mongoose. Modern enterprise architectures frequently leverage `@nestjs/drizzle` (official in NestJS) and `@mikro-orm/nestjs` for Unit of Work and high-throughput SQL.
- **Decision**: Add dedicated notes for MikroORM (`@mikro-orm/nestjs`) and Drizzle ORM (`@nestjs/drizzle`) with full CRUD, transactions, relations, and repository patterns.

---

## ADR 005: Reference Projects Suite (10-projects)
- **Status**: Accepted
- **Context**: `10-projects/` only contained a high-level README without implemented projects.
- **Decision**: Build out 15 complete, production-grade reference projects adhering to `_templates/project-template.md`:
  1. `01-blog-api`
  2. `02-auth-service` (JWT, Refresh, 2FA, OAuth2)
  3. `03-task-manager-api` (RBAC & CASL)
  4. `04-ecommerce-backend` (Orders, Stripe payments, outbox pattern)
  5. `05-url-shortener` (Fastify, Redis cache, rate limiting)
  6. `06-file-upload-service` (S3 presigned URLs, streaming multipart)
  7. `07-notification-service` (BullMQ, SES, Twilio, FCM, templates)
  8. `08-realtime-chat` (WebSocket Gateway, Redis adapter, presence)
  9. `09-multi-tenant-saas-api` (Row-Level Security, tenant isolation)
  10. `10-graphql-api` (Apollo, DataLoader N+1 batching, federation)
  11. `11-microservices-system` (API Gateway + gRPC / RabbitMQ services)
  12. `12-cqrs-event-sourced-bank` (EventStore, CQRS commands/queries)
  13. `13-ai-rag-api` (OpenAI / Anthropic, LangChain, pgvector embeddings)
  14. `14-booking-system` (Optimistic/pessimistic locking, race condition mitigation)
  15. `15-analytics-ingestion-pipeline` (High-throughput event streaming, ClickHouse/TimescaleDB)

---

## ADR 006: Quick-Reference Consolidation (Module 13)
- **Status**: Accepted
- **Context**: Prompt instructed not to create a duplicate `14-cheatsheets/` folder, but rather to extend `13-quick-reference/` with consecutive numbering.
- **Decision**: Keep and extend `13-quick-reference/` sequentially from 01 through 25, adding cheat sheets for Fastify adapter, MikroORM, Drizzle, CQRS, GraphQL, WebSockets, Throttler, Cron, Terminus, and v10 $\rightarrow$ v11 upgrade.
