# NestJS Curriculum Completion & Standardization Plan

> Detailed execution plan for auditing, completing, and expanding the NestJS learning track (`03-backend/nestjs`).

---

## Batch Structure & Milestones

### Batch 0: Hygiene, Stray README Renaming & Root Documentation Suite
1. Rename all 9 stray `README (N).md` files into canonical `README.md`.
2. Write root documentation suite matching Java/Node.js track conventions:
   - `README.md`: Curriculum overview, architecture pillars, prerequisites.
   - `INDEX.md`: Master alphabetical and modular index of all topics.
   - `GLOSSARY.md`: 60+ core NestJS and enterprise architectural terms.
   - `ROADMAP.md`: Structured learning roadmaps (Beginner, API Dev, Enterprise/DDD, Microservices, Performance).
   - `CONTRIBUTING.md`: Contribution guidelines, style standards, PR checklist.
   - `LICENSE`: Curriculum attribution and usage notice.

### Batch 1: Extension of Modules 00, 01, 02
- **00-prerequisites**: Add `05-nodejs-modules-esm-and-cjs.md`, `06-reflect-metadata-and-decorator-mechanics.md`, `07-oop-and-solid-primer.md`, and update `README.md`.
- **01-getting-started**: Add `07-nestjs-versions-and-upgrade-guide.md` (v8 to v11 migration), `08-nest-cli-workspaces-and-monorepos.md`, `09-swc-and-vitest-setup.md`, `10-devtools-and-repl.md`, `11-standalone-applications-and-cli-apps.md`, and update `README.md`.
- **02-fundamentals**: Add `10-routing-and-wildcards-express5-vs-fastify.md`, `11-platform-express-vs-fastify.md`, `12-versioning-uri-header-media-type.md`, `13-serving-static-and-sse.md`, `14-streaming-files.md`, `15-http-module-and-axios.md`, `16-raw-body-and-body-parsers.md`, `17-cookies-and-sessions.md`, `18-hybrid-applications-intro.md`, and update `README.md`.

### Batch 2: Completion & Extension of Modules 03 & 04
- **03-core-concepts**:
  - `02-validation-and-serialization`: Add `05-zod-based-validation-with-nestjs-zod.md`.
  - `04-modules-and-di`: Add `05-configurable-module-builder.md`, `06-provider-scopes-and-performance-impact.md`, `07-discovery-service-and-explorer.md`.
- **04-intermediate**:
  - Fill 0-byte notes: `08-api-design/06-idempotency.md`, `07-webhooks.md`, and `09-openapi-and-swagger/06-client-sdk-generation.md`.
  - `02-database-foundations`: Add `03-mikro-orm.md`, `04-drizzle-with-nestjs.md`, `05-kysely-and-typed-sql.md`.
  - `06-authentication`: Add `11-passkeys-and-webauthn.md`.
  - `07-authorization`: Add `05-casl-advanced-and-policy-guards.md`.

### Batch 3: Module 05 Advanced Part 1 (Caching, Queues & Events)
- **01-caching**: Write all 5 notes + README (`01-caching-fundamentals.md`, `02-cache-manager.md`, `03-redis.md` with Keyv, `04-cache-invalidation.md`, `05-distributed-caching.md`).
- **02-background-processing**: Write all 9 notes + README (`01-queues-fundamentals.md` to `09-dynamic-scheduling-and-timezones.md`).
- **03-events-and-messaging**: Write all 4 notes + CQRS note + README (`01-event-emitter.md`, `02-messaging-fundamentals.md`, `03-kafka.md`, `04-rabbitmq.md`, `05-cqrs-module.md`).

### Batch 4: Module 05 Advanced Part 2 (Realtime, GraphQL, Microservices, Integrations, AI)
- **04-realtime**: Write all 6 notes + README (`01-gateways-and-events.md` to `06-server-sent-events-and-streaming.md`).
- **05-graphql**: Write all 6 notes + README (`01-graphql-fundamentals.md` to `06-apollo-federation.md`).
- **06-microservices**: Write all 6 notes + README (`01-microservices-overview.md` to `06-resilience-and-circuit-breaker.md`).
- **07-integrations**: Write all 6 notes + README (`01-stripe-payments.md` to `06-webhooks-system.md`).
- **08-ai-and-llm**: Write all 6 notes + README (`01-openai-integration.md` to `06-rate-limits-and-cost-control.md`).

### Batch 5: Module 06 Internals & Module 07 Production
- **06-internals**: Write all 8 notes + README (`01-how-nest-boots.md` to `08-writing-custom-platform-adapter.md`).
- **07-production**: Write all notes across `01-security` (6 notes), `02-performance` (6 notes), `03-observability` (6 notes), `04-deployment` (6 notes), `05-operations` (6 notes) + READMEs.

### Batch 6: Module 08 Architecture & Patterns & Module 09 Best Practices
- **08-architecture-and-patterns**: Write notes in `01-architecture` (6 notes), `02-design-patterns` (6 notes), `03-clean-code` (5 notes), `04-real-world-patterns` (7 notes) + READMEs.
- **09-best-practices**: Write all 10 notes + README (`01-project-structure.md` to `10-troubleshooting-common-errors.md`).

### Batch 7: Module 10 Reference Projects (Projects 01 - 08)
Build 8 complete production reference projects in `10-projects/`:
1. `01-blog-api`
2. `02-auth-service`
3. `03-task-manager-api`
4. `04-ecommerce-backend`
5. `05-url-shortener`
6. `06-file-upload-service`
7. `07-notification-service`
8. `08-realtime-chat`

### Batch 8: Module 10 Reference Projects (Projects 09 - 15) & Catalog
Build remaining 7 production reference projects in `10-projects/`:
9. `09-multi-tenant-saas-api`
10. `10-graphql-api`
11. `11-microservices-system`
12. `12-cqrs-event-sourced-bank`
13. `13-ai-rag-api`
14. `14-booking-system`
15. `15-analytics-ingestion-pipeline`
- `10-projects/README.md` (Full 15-project catalog).

### Batch 9: Module 11 System Design & Module 12 Interview Preparation
- **11-system-design**: Write 19 notes + README (scalable REST, auth, notifications, uploads, queues, realtime chat, ecommerce, shortener, rate limiter, microservices, social feed, payments, search, booking, collaborative editing, video processing, analytics, API gateway/BFF).
- **12-interview-preparation**: Write 16 notes + README (fundamentals, modules/DI, request pipeline, auth, databases, testing, microservices, architecture, performance, security, output questions, scenario questions, Nest 11 changes, DI scopes deep questions, coding rounds, project discussions).

### Batch 10: Module 13 Quick Reference, Tree Regeneration & Final Report
- **13-quick-reference**: Write 25 high-density cheat sheets + README.
- Regenerate root `tree.txt`.
- Write `NEST_FINAL_REPORT.md`.
