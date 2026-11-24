# NestJS Technical Glossary

> 60+ authoritative definitions of core NestJS framework concepts, dependency injection mechanisms, request lifecycle components, and distributed architecture patterns.

---

### A
- **Adapter (Platform Adapter)**: Abstraction layer (`HttpServer`) allowing NestJS to run on top of different HTTP engines (such as `@nestjs/platform-express` or `@nestjs/platform-fastify`).
- **ArgumentMetadata**: Interface passed to custom pipes containing metadata about the target argument: `{ type: 'body' | 'query' | 'param' | 'custom', metatype: Type<any>, data: string }`.
- **AsyncLocalStorage (ALS)**: Node.js standard library module (`node:async_hooks`) enabling request-scoped contextual storage across asynchronous boundaries without parameter passing.

### B
- **Barrel File (`index.ts`)**: File that re-exports components from a directory; a common source of circular dependency initialization bugs in NestJS if modules re-export each other.
- **BullMQ (`@nestjs/bullmq`)**: Redis-backed distributed task queue and background job processing engine with support for job parent-child flows, retries, and sandboxed workers.

### C
- **CallHandler**: Interface passed to an interceptor (`next: CallHandler`) providing `handle()`, which returns an RxJS `Observable` representing the route handler execution stream.
- **CanActivate**: Interface implemented by Guards (`canActivate(context: ExecutionContext): boolean | Promise<boolean> | Observable<boolean>`). Returns true if request is permitted.
- **CASL**: Attribute-Based Access Control (ABAC) library integrated into NestJS to evaluate fine-grained user permissions based on entity ownership and subject attributes.
- **Circular Dependency**: Situations where Provider A requires Provider B, which in turn requires Provider A. Resolved via `forwardRef(() => ProviderB)` on both injection sides.
- **ConfigurableModuleBuilder**: Utility class introduced in modern NestJS to automate the generation of dynamic module options and `forRoot` / `forRootAsync` boilerplates.
- **ConsoleLogger**: Built-in logger in `@nestjs/common` supporting formatted string output, log level filtering, and structured JSON output via `json: true`.
- **CQRS (`@nestjs/cqrs`)**: Command Query Responsibility Segregation architecture pattern dividing operations into mutating Commands, read-only Queries, Domain Events, and long-running Sagas.
- **Custom Decorator**: Function returning `createParamDecorator()` or composition using `applyDecorators()` to encapsulate parameter extraction or metadata attachment.

### D
- **DataLoader**: In-memory utility for batching and caching database reads within a single Node.js event loop tick, eliminating N+1 query problems in GraphQL resolvers.
- **Dependency Injection (DI)**: Inversion of Control (IoC) pattern where dependencies are supplied to a class constructor by the framework container rather than created manually with `new`.
- **DiscoveryService**: Utility in `@nestjs/core` that inspects the compiled dependency graph, discovering all registered providers, controllers, and metadata at runtime.
- **Drizzle ORM (`@nestjs/drizzle`)**: High-performance, lightweight TypeScript ORM with zero cold-start overhead, near-raw SQL execution speed, and explicit relational queries.
- **Dynamic Module**: Module whose metadata (`providers`, `exports`) is configured at runtime via static methods like `register()`, `forRoot()`, or `forFeature()`.

### E
- **ExecutionContext**: Context object passed to Guards, Interceptors, and Filters, extending `ArgumentsHost` with `getClass()` and `getHandler()` reflection helpers.
- **ExceptionFilter**: Component implementing `catch(exception: T, host: ArgumentsHost)` to capture exceptions thrown in the request pipeline and serialize custom HTTP error responses.
- **Express 5**: The default underlying HTTP engine in NestJS 11, introducing stricter path matching (named wildcards `*path`) and native rejected promise forwarding.

### F
- **Fastify**: High-throughput HTTP engine available via `@nestjs/platform-fastify`, featuring radix tree routing (`find-my-way`) and JSON schema serialization.
- **ForwardRef**: Utility function creating a circular reference wrapper (`forwardRef(() => OtherModule)`), allowing the Nest compiler to defer resolution until both modules are registered.

### G
- **Global Module (`@Global()`)**: Decorator making a module's exported providers available everywhere in the application graph without needing to re-import it into every feature module.
- **Guard**: Pipeline stage executing after middleware and before interceptors, responsible for authentication and authorization checks (`CanActivate`).

### H
- **Hybrid Application**: Single NestJS process running both an HTTP web server (Express/Fastify) and one or more microservice listeners (Kafka, RabbitMQ, TCP, gRPC) simultaneously.

### I
- **Idempotency Key**: Unique identifier passed via HTTP headers (`Idempotency-Key`) allowing servers to deduplicate retry requests safely without duplicate side-effects.
- **Interceptor**: Component implementing `NestInterceptor` wrapping route handler execution with RxJS operators, enabling pre/post processing, caching, timeouts, and response transformation.
- **IntrinsicException**: New operational exception base class in NestJS 11; built-in exception filters do not auto-log instances of it to prevent log noise from expected client errors.

### K
- **Keyv**: Modular key-value storage engine powering `cache-manager` v6 in NestJS 11+, replacing legacy cache-store drivers with unified `@keyv/*` adapters.

### L
- **Lifecycle Hooks**: Interfaces (`OnModuleInit`, `OnApplicationBootstrap`, `OnModuleDestroy`, `BeforeApplicationShutdown`, `OnApplicationShutdown`) called during app boot and teardown.

### M
- **Metadata Reflection**: Feature relying on `reflect-metadata` allowing TypeScript to emit design-time types and custom decorator values into the runtime memory graph.
- **Microservices (`@nestjs/microservices`)**: Nest module providing transport-level RPC abstractions for TCP, Redis, NATS, MQTT, RabbitMQ, Kafka, and gRPC.
- **MikroORM (`@mikro-orm/nestjs`)**: TypeScript Data Mapper ORM featuring an Identity Map and Unit of Work, favored in enterprise Domain-Driven Design (DDD).
- **ModuleRef**: Internal service enabling dynamic retrieval of providers, transient instantiations, and lazy module loading at runtime.

### N
- **NestCLI**: Command-line interface (`@nestjs/cli`) for scaffolding projects, generating architectural components (`nest g mo`, `nest g co`), and orchestrating builds.
- **`nestjs-zod`**: Ecosystem package combining Zod validation with NestJS DTO generation (`createZodDto`) and OpenAPI Swagger documentation.

### O
- **Onion Architecture**: Layered software design ensuring inner domain rules do not depend on external frameworks or databases (Ports & Adapters).
- **OpenAPI / Swagger (`@nestjs/swagger`)**: Module generating OpenAPI 3.1 specifications from TypeScript DTOs, entity decorators, and the CLI Swagger plugin.
- **Outbox Pattern**: Transactional pattern saving domain events to an outbox table in the same DB transaction as business mutations, ensuring at-least-once message delivery.

### P
- **ParseDatePipe**: Built-in pipe introduced in NestJS 11 validating and parsing date strings into JavaScript `Date` instances.
- **Passkeys (WebAuthn)**: FIDO2 biometric and hardware key authentication standard eliminating shared passwords.
- **Pipe**: Component implementing `PipeTransform` executing immediately before route handler invocation, responsible for parameter transformation and validation.
- **Prisma**: Schema-driven, type-safe database client using `schema.prisma` definitions, wrapped in NestJS via custom `@Injectable()` services.
- **Provider**: Any class decorated with `@Injectable()` that can be injected as a dependency (services, repositories, factories, helpers).
- **Provider Scope**: Lifecycle lifetime of an injected provider: `DEFAULT` (Singleton), `REQUEST` (New instance per request), or `TRANSIENT` (New instance per consumer).

### R
- **Reflector**: Helper service in `@nestjs/core` for extracting metadata attached to route handlers and controller classes via `@SetMetadata()`.
- **Request Pipeline**: Deterministic sequential execution order: `Middleware -> Guards -> Interceptors (Pre) -> Pipes -> Route Handler -> Interceptors (Post) -> Exception Filters`.
- **Row-Level Security (RLS)**: Database-level multi-tenancy enforcement restricting row queries to the tenant ID bound to the active database session.

### S
- **Saga**: Long-running business transaction coordinator in CQRS listening to domain events and dispatching compensating commands on failures.
- **Scope.REQUEST**: Provider scope creating a dedicated instance per inbound HTTP request; propagates request-scope status upward through the entire dependency chain.
- **Single Executable Application (SEA)**: Node.js feature bundling JavaScript applications and runtime into a single native OS binary.
- **SWC (`@swc/core`)**: Rust-based extensible JavaScript/TypeScript compiler providing up to 20x faster compilation than standard `tsc` in development and testing.

### T
- **Terminus (`@nestjs/terminus`)**: Production healthcheck module providing ready-to-use health indicators for databases, Redis, memory heap, and disk storage.
- **Testcontainers**: Testing library spinning up real ephemeral Docker containers (PostgreSQL, Kafka, Redis) for integration and E2E test suites.
- **TransformStream**: Node.js stream reading raw input chunks, transforming data, and piping the output downstream with backpressure management.
- **TypeORM (`@nestjs/typeorm`)**: Object-Relational Mapper supporting Active Record and Data Mapper patterns, decorators, and query runners.

### U
- **Unit of Work**: Design pattern maintaining a list of database transactions and changes, committing them atomically in a single coordinated operation (e.g. MikroORM).
- **Unwrap (`.unwrap()`)**: Transporter method in NestJS 11 exposing the raw underlying driver connection (e.g. native NATS or Kafka client).

### V
- **ValidationPipe**: Global pipe utilizing `class-validator` and `class-transformer` to enforce payload schemas, strip non-whitelisted properties, and transform types.

### Z
- **ZodValidationPipe**: Pipe from `nestjs-zod` validating incoming request bodies against Zod schema definitions with automatic type coercion.
