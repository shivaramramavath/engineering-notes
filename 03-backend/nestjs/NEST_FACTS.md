# NestJS Technical Facts & Ecosystem Reference

> Source of Truth for framework versions, platform adaptations, breaking changes, and runtime behaviors in NestJS 11.

---

## 1. Runtime Baseline & Platform Standards

| Parameter | Specification | Notes / Breaking Changes |
| :--- | :--- | :--- |
| **Current Major** | **NestJS 11.x** | Core packages: `@nestjs/core`, `@nestjs/common`, `@nestjs/platform-express` |
| **Node.js Minimum** | **Node.js >= 20.0.0** | Node.js 18 support completely dropped (EOL April 2025). Node.js 22 LTS is primary target. |
| **Default HTTP Engine** | **Express 5.0** | Express v5 is now the default underlying engine in `@nestjs/platform-express` v11. |
| **Alternative Engine** | **Fastify 5.0** | `@nestjs/platform-fastify` v11 natively supports Fastify v5. |
| **TypeScript Baseline** | **TypeScript 5.3+ / 5.7+** | Supports `NodeNext` module resolution, Stage 3 decorators, and native type stripping. |

---

## 2. NestJS 11 Architectural Shifts & Breaking Changes

### 2.1 Stricter Route Matching in Express 5 (`path-to-regexp` v8)
In Express 5, un-named wildcard asterisks are rejected:
- **Nest 10 (Express 4)**: `@Get('files/*')`
- **Nest 11 (Express 5)**: Must use named wildcards: `@Get('files/*path')` or `@Get('files/*splat')`.
- **Optional Parameters**: Must use curly braces: `@Get('users{/:id}')` instead of `@Get('users/:id?')`.

### 2.2 Termination Lifecycle Hooks Order Reversed
In NestJS 11, the execution order of shutdown lifecycle hooks has been **reversed** to strictly follow reverse-topological order:
- Dependent modules tear down **before** the providers and database connections they depend on.
- Affects: `OnModuleDestroy`, `BeforeApplicationShutdown`, `OnApplicationShutdown`.
- Initialization hooks (`OnModuleInit`, `OnApplicationBootstrap`) remain in standard topological order.

### 2.3 Caching Engine Overhaul: `cache-manager` v6 + Keyv
- `@nestjs/cache-manager` in v11 uses `cache-manager` v6, which migrated its core engine to **Keyv**.
- Legacy store packages (such as `cache-manager-redis-store`) are obsolete and incompatible.
- Key-value stores use `@keyv/*` adapters (e.g. `@keyv/redis`) via the `stores` array:
  ```typescript
  import { CacheModule } from '@nestjs/cache-manager';
  import { createKeyv } from '@keyv/redis';

  CacheModule.registerAsync({
    isGlobal: true,
    useFactory: async () => ({
      stores: [createKeyv('redis://localhost:6379')],
    }),
  });
  ```

### 2.4 Built-in Structured JSON Logging in `ConsoleLogger`
`ConsoleLogger` in `@nestjs/common` now natively supports JSON formatting without third-party loggers:
```typescript
const app = await NestFactory.create(AppModule, {
  logger: new ConsoleLogger({ json: true }),
});
```
When `json: true` is configured, colored output is disabled and each log line is printed as NDJSON with timestamp, log level, context, and message.

### 2.5 `IntrinsicException` (Operational Exception Base Class)
- `@nestjs/common` introduces `IntrinsicException`, the base class for `HttpException`, `WsException`, and `RpcException`.
- The framework's default global exception filters recognize `IntrinsicException` instances as expected operational events and **do not automatically log them as server errors**, keeping server logs clean from client-generated 400/404 noise.

### 2.6 Startup Performance Optimization
Dynamic module instantiation no longer computes hash-based string keys; NestJS 11 uses direct object references internally, delivering up to 30-40% faster boot times for large enterprise modular graphs.

### 2.7 `ParseDatePipe`
A new built-in pipe in `@nestjs/common` for validating and transforming ISO 8601 strings into native JavaScript `Date` instances.

### 2.8 Transporter `.unwrap()`
Microservice clients (NATS, Kafka, RabbitMQ) now expose `.unwrap<T>()` to allow direct access to underlying client connections when driver-specific low-level APIs are needed.

---

## 3. Ecosystem Packages & Versions in NestJS 11

| Package | Version Baseline | Architectural Role |
| :--- | :--- | :--- |
| **`@nestjs/config`** | `^4.0.0` | Environment configuration, validation with Joi/Zod, ConfigService namespaces. |
| **`@nestjs/typeorm` + `typeorm`**| `^11.0.0` / `^0.3.20` | Decorator-driven ORM, Active Record & Data Mapper, subscribers, query runners. |
| **`@prisma/client` + `prisma`**| `^6.0.0` | Schema-driven type-safe client, extended clients ($extends), middleware/extensions. |
| **`@mikro-orm/nestjs`** | `^6.0.0` | TypeScript-first Data Mapper, Unit of Work, Identity Map for DDD architectures. |
| **`@nestjs/drizzle` + `drizzle-orm`**| `^11.0.0` / `^0.38.0` | Lightweight high-performance SQL query builder with zero reflection overhead. |
| **`class-validator` + `class-transformer`**| `^0.14.1` / `^0.5.1` | DTO decorators, whitelist validation, transformation, validation groups. |
| **`nestjs-zod` + `zod`** | `^4.0.0` / `^3.24.0` | `createZodDto`, `ZodValidationPipe`, `patchNestJsSwagger` for single-source typing. |
| **`@nestjs/passport` + `passport`**| `^11.0.0` / `^0.7.0` | Passport strategies (Local, JWT, OAuth2, Refresh Token, Passkeys). |
| **`@nestjs/swagger`** | `^11.0.0` | OpenAPI 3.1 generation, Swagger UI, CLI plugin for automatic DTO property reflection. |
| **`@nestjs/bullmq` + `bullmq`**| `^11.0.0` / `^5.40.0` | Distributed Redis queues, sandboxed processors, flow producers, job observability. |
| **`@nestjs/schedule`** | `^5.0.0` | Cron jobs (`@Cron`), intervals, timeouts, distributed cron lock coordination. |
| **`@nestjs/throttler`** | `^6.0.0` | Multi-tier rate limiting with custom storage providers (Redis/Keyv). |
| **`@nestjs/terminus`** | `^11.0.0` | Production health checks (HTTP, Database, Memory Heap, Disk, Redis ping). |
| **`@nestjs/cqrs`** | `^11.0.0` | Command, Query, Event handlers, and Sagas. |
| **`@nestjs/event-emitter`** | `^3.0.0` | In-process decoupled pub/sub events via Node.js EventEmitter2. |
| **`@nestjs/graphql` + `@apollo/server`**| `^13.0.0` / `^4.11.0` | Code-first and Schema-first GraphQL, DataLoader batching, Apollo Federation. |
| **`@nestjs/microservices`** | `^11.0.0` | Message patterns, Event patterns, gRPC, RabbitMQ, Kafka, Redis, NATS. |
| **`nestjs-pino` + `pino`** | `^4.2.0` / `^9.6.0` | High-performance JSON logging with request correlation IDs (`X-Request-Id`). |
| **`@casl/ability`** | `^6.7.0` | Attribute-Based Access Control (ABAC) and fine-grained rule evaluation. |
