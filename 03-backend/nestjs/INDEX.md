# NestJS Curriculum Master Index

> Exhaustive index of all topics, concepts, modules, design patterns, and reference architectures across the NestJS learning track.

---

## 1. Master Modules Table

| Module Code | Module Directory | Primary Focus |
| :--- | :--- | :--- |
| **00** | **[00-prerequisites](00-prerequisites/README.md)** | TypeScript decorators, metadata reflection, async mechanics, ESM/CJS, HTTP standards, SOLID. |
| **01** | **[01-getting-started](01-getting-started/README.md)** | CLI, project structure, dev environment, debugging, Nest 11 upgrade guide, SWC, Vitest, REPL. |
| **02** | **[02-fundamentals](02-fundamentals/README.md)** | Core building blocks: Modules, Controllers, Providers, Dependency Injection, Express 5 vs Fastify. |
| **03** | **[03-core-concepts](03-core-concepts/README.md)** | Request pipeline (Pipes, Guards, Interceptors, Filters), Validation (Zod & class-validator), Config, DI Scopes. |
| **04** | **[04-intermediate](04-intermediate/README.md)** | Testing with Testcontainers, ORMs (TypeORM, Prisma, Mongoose, MikroORM, Drizzle), Auth, Swagger. |
| **05** | **[05-advanced](05-advanced/README.md)** | Caching (Keyv), Background Queues (BullMQ), Events, WebSockets, GraphQL, Microservices, AI/LLM. |
| **06** | **[06-internals](06-internals/README.md)** | Bootstrap sequence, DI resolution engine, metadata storage, compiler graph, platform adapters. |
| **07** | **[07-production](07-production/README.md)** | Security hardening, performance tuning, OpenTelemetry, Pino logging, Terminus, Docker, Kubernetes. |
| **08** | **[08-architecture-and-patterns](08-architecture-and-patterns/README.md)** | Clean/Hexagonal, Modular Monolith, DDD, CQRS, GoF design patterns, transactional outbox. |
| **09** | **[09-best-practices](09-best-practices/README.md)** | Architecture conventions, naming, DTO/Entity separation, testing strategies, troubleshooting. |
| **10** | **[10-projects](10-projects/README.md)** | 15 fully specified enterprise production applications with complete architectures and tests. |
| **11** | **[11-system-design](11-system-design/README.md)** | Large-scale systems: Scalable REST, Auth, Notifications, Real-time chat, Social feed, E-commerce. |
| **12** | **[12-interview-preparation](12-interview-preparation/README.md)** | Senior & Staff interview guides, output prediction, scenario questions, timed coding rounds. |
| **13** | **[13-quick-reference](13-quick-reference/README.md)** | 25 high-density quick reference cards covering all major NestJS modules and CLI flags. |

---

## 2. Topic Cross-Reference Index (A-Z)

### A
- **ABAC (Attribute-Based Access Control)**: `04-intermediate/07-authorization/03-abac-and-policies.md`
- **Active Record vs Data Mapper**: `04-intermediate/03-typeorm/03-entities.md`
- **AI & LLM Integration (LangChain / OpenAI)**: `05-advanced/08-ai-and-llm/01-openai-integration.md`
- **API Versioning (URI, Header, Media Type)**: `02-fundamentals/12-versioning-uri-header-media-type.md`
- **Apollo Federation**: `05-advanced/05-graphql/06-apollo-federation.md`
- **AsyncLocalStorage in NestJS**: `04-intermediate/08-api-design/08-request-context-with-cls.md`

### B
- **Backpressure in WebSockets**: `05-advanced/04-realtime/05-scaling-websockets.md`
- **Bootstrap Sequence**: `06-internals/01-how-nest-boots.md`
- **BullMQ Distributed Queues**: `05-advanced/02-background-processing/02-bullmq.md`

### C
- **Cache-Manager v6 & Keyv**: `05-advanced/01-caching/02-cache-manager.md`
- **CASL Authorization**: `04-intermediate/07-authorization/04-casl-integration.md`
- **Circular Dependencies (`forwardRef`)**: `03-core-concepts/04-modules-and-di/04-circular-dependency.md`
- **Clean Architecture & Ports/Adapters**: `08-architecture-and-patterns/01-architecture/03-clean-architecture.md`
- **ConfigurableModuleBuilder**: `03-core-concepts/04-modules-and-di/05-configurable-module-builder.md`
- **CQRS (Command Query Responsibility Segregation)**: `05-advanced/03-events-and-messaging/05-cqrs-module.md`

### D
- **DataLoader in GraphQL**: `05-advanced/05-graphql/03-dataloader.md`
- **Dependency Injection Container**: `02-fundamentals/05-dependency-injection.md`, `06-internals/02-dependency-injection-internals.md`
- **DiscoveryService & MetadataScanner**: `03-core-concepts/04-modules-and-di/07-discovery-service-and-explorer.md`
- **Drizzle ORM Integration**: `04-intermediate/02-database-foundations/04-drizzle-with-nestjs.md`
- **Dynamic Modules (`forRoot`, `forFeature`)**: `03-core-concepts/04-modules-and-di/02-dynamic-modules.md`

### E
- **Error Handling & Exception Filters**: `03-core-concepts/01-request-pipeline/08-exception-filters.md`
- **EventEmitter2 in NestJS**: `05-advanced/03-events-and-messaging/01-event-emitter.md`
- **Express 5 Migration & Named Wildcards**: `01-getting-started/07-nestjs-versions-and-upgrade-guide.md`, `02-fundamentals/10-routing-and-wildcards-express5-vs-fastify.md`

### F
- **Fastify Platform Adapter**: `02-fundamentals/11-platform-express-vs-fastify.md`, `07-production/02-performance/01-fastify-adapter.md`
- **File Uploads & Streaming (S3)**: `04-intermediate/08-api-design/09-file-handling-and-uploads.md`

### G
- **Guards & CanActivate**: `03-core-concepts/01-request-pipeline/04-guards.md`
- **gRPC Microservices**: `05-advanced/06-microservices/04-grpc.md`

### I
- **Idempotency Keys**: `04-intermediate/08-api-design/06-idempotency.md`
- **Interceptors & CallHandler**: `03-core-concepts/01-request-pipeline/05-interceptors.md`
- **`IntrinsicException`**: `03-core-concepts/01-request-pipeline/07-http-exceptions.md`

### K
- **Kafka Transporter**: `05-advanced/03-events-and-messaging/03-kafka.md`
- **Kysely Typed SQL**: `04-intermediate/02-database-foundations/05-kysely-and-typed-sql.md`

### M
- **Microservices & Transporter `.unwrap()`**: `05-advanced/06-microservices/01-microservices-overview.md`
- **MikroORM Integration**: `04-intermediate/02-database-foundations/03-mikro-orm.md`
- **Modular Monolith**: `08-architecture-and-patterns/01-architecture/02-modular-monolith.md`

### P
- **`ParseDatePipe`**: `03-core-concepts/01-request-pipeline/03-pipes.md`
- **Passkeys & WebAuthn**: `04-intermediate/06-authentication/11-passkeys-and-webauthn.md`
- **Passport Strategies (JWT & Local)**: `04-intermediate/06-authentication/02-passport-and-local-strategy.md`
- **Pipes & ArgumentMetadata**: `03-core-concepts/01-request-pipeline/03-pipes.md`
- **Prisma Client Extensions**: `04-intermediate/04-prisma/03-prisma-service.md`
- **Provider Scopes (DEFAULT, REQUEST, TRANSIENT)**: `03-core-concepts/04-modules-and-di/06-provider-scopes-and-performance-impact.md`

### R
- **RabbitMQ Transporter**: `05-advanced/03-events-and-messaging/04-rabbitmq.md`
- **Rate Limiting with Throttler**: `04-intermediate/08-api-design/10-rate-limiting-with-throttler.md`
- **Reflector API**: `03-core-concepts/01-request-pipeline/10-custom-decorators.md`
- **Row-Level Security (RLS)**: `04-intermediate/04-prisma/08-row-level-security.md`

### S
- **Shutdown Lifecycle Order**: `03-core-concepts/04-modules-and-di/03-module-lifecycle.md`
- **Swagger / OpenAPI Generation**: `04-intermediate/09-openapi-and-swagger/01-swagger-setup.md`
- **SWC & Vitest Compilation**: `01-getting-started/09-swc-and-vitest-setup.md`

### T
- **Terminus Health Checks**: `07-production/03-observability/04-terminus-health-checks.md`
- **Testcontainers for E2E Tests**: `04-intermediate/01-testing/06-testcontainers.md`
- **Transactional Outbox Pattern**: `08-architecture-and-patterns/04-real-world-patterns/03-transactional-outbox.md`
- **TypeORM Subscriptions & QueryRunners**: `04-intermediate/03-typeorm/07-transactions.md`

### Z
- **Zod & `nestjs-zod`**: `03-core-concepts/02-validation-and-serialization/05-zod-based-validation-with-nestjs-zod.md`
