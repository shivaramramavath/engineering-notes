# Architecture Overview

NestJS is an opinionated framework for building server-side applications in TypeScript. It does not replace Express or Fastify. It sits on top of one of them and adds an architecture: modules to organize code, controllers to receive requests, providers to hold logic, a dependency-injection container to connect them, and a request pipeline (middleware, guards, interceptors, pipes, exception filters) to handle cross-cutting concerns. This file gives you the map. The remaining files in this stage explore each territory.

---

## Overview

**What it is.** A framework that provides structure (an architecture) for Node.js applications. You write classes decorated with metadata. Nest reads that metadata at startup, builds a graph of components, and runs your application through a fixed request pipeline.

**Why it exists.** Express and Fastify give you routing and middleware but no opinion on how to organize a large codebase, share logic, or test it. Teams reinvent structure differently every time. Nest standardizes it, borrowing ideas from Angular (modules, decorators, dependency injection) and from enterprise frameworks like Spring.

**Where it is used.** REST and GraphQL APIs, WebSocket servers, microservices over message brokers and gRPC, background workers, CLI tools, and standalone scripts. All can share the same modules and providers.

**Why you should understand the map first.** Every later topic is a specific building block or a specific part of the request journey. Learning them without the map leads to cargo-cult code (adding decorators until it works).

---

## Mental Model

Three building blocks, one container, one pipeline:

```text
                        ┌────────────────────── Application (Nest IoC container) ──────────────────────┐
                        │                                                                                │
   HTTP request         │   ┌──────────────── Module: UsersModule ─────────────────┐                     │
  ──────────────►  Adapter  │                                                      │                     │
   (Express/Fastify)    │   │  Controller ──depends on──► Provider (Service)       │                     │
                        │   │  (routes)                    │   ──depends on──► Provider (Repository)    │
                        │   │                              ▼                       │                     │
                        │   │                       Provider (any injectable)      │                     │
                        │   └──────────────────────────────┬───────────────────────┘                     │
                        │                  imports / exports│                                             │
                        │   ┌──────────────────────────────▼───────────────────────┐                     │
                        │   │ Module: DatabaseModule (exports a provider)          │                     │
                        │   └──────────────────────────────────────────────────────┘                     │
                        └────────────────────────────────────────────────────────────────────────────────┘

   Request pipeline around every controller call:
   Middleware → Guards → Interceptors (before) → Pipes → Handler → Interceptors (after) → Exception filters (on error)
```

The simplest way to remember it:

| Piece | Question it answers |
|---|---|
| **Module** | Which pieces belong together, and what do they share? |
| **Controller** | Which URL and method maps to which function? |
| **Provider** | Where does the actual work (logic, data access, integrations) live? |
| **Dependency injection** | Who creates the objects and hands them to whoever needs them? |
| **Pipeline components** | What happens before and after my handler (auth, validation, logging, errors)? |

---

## Core Concepts

### The Building Blocks

| Block | Declared with | Role | Covered in |
|---|---|---|---|
| **Module** | `@Module({...})` | Groups controllers and providers, controls what is shared | [Modules](./02-modules.md) |
| **Controller** | `@Controller()` + `@Get()`, `@Post()`... | Maps requests to handler methods | [Controllers](./03-controllers.md) |
| **Provider** | `@Injectable()` | Anything injectable: services, repositories, factories, helpers | [Providers and Services](./04-providers-and-services.md) |
| **DI container** | Built in | Creates providers, resolves dependencies, manages lifetimes | [Dependency Injection](./05-dependency-injection.md) |
| **Decorators** | `@Something()` | Attach metadata that Nest reads | [Decorators](./06-decorators.md) |

### The Request Pipeline Components

Each is a class (or function) that plugs into the request journey at a defined point.

| Component | Runs | Typical job |
|---|---|---|
| **Middleware** | First, before routing resolves the handler context | Logging, request IDs, body parsing, CORS |
| **Guard** | After middleware | Authentication and authorization (allow or deny) |
| **Interceptor** | Before and after the handler | Logging, timing, caching, response mapping |
| **Pipe** | Just before the handler, on its parameters | Validation and transformation |
| **Exception filter** | When an exception is thrown | Turn errors into responses |

Details: [Request Lifecycle](./09-request-lifecycle.md) now, then [Request Pipeline](../03-core-concepts/01-request-pipeline/README.md).

### Platform Agnosticism: Express and Fastify

Nest does not implement an HTTP server. It delegates to an **HTTP adapter**.

| Package | Platform | Notes |
|---|---|---|
| `@nestjs/platform-express` | Express | **Default.** Largest middleware ecosystem. Registration order affects route matching |
| `@nestjs/platform-fastify` | Fastify | Higher throughput, schema-oriented. Its router ranks routes by specificity, so declaration order does not affect matching |

Your controllers, providers, guards, and pipes are written against Nest abstractions, so you can usually switch platforms without rewriting application logic. You lose portability only when you reach for platform objects (`@Req()`, `@Res()`) or platform-specific middleware.

```typescript
// Express (default)
const app = await NestFactory.create(AppModule);

// Fastify
import { FastifyAdapter, type NestFastifyApplication } from '@nestjs/platform-fastify';
const fastifyApp = await NestFactory.create<NestFastifyApplication>(AppModule, new FastifyAdapter());
await fastifyApp.listen(3000, '0.0.0.0');   // bind all interfaces, needed inside containers
```

Details: [Platform Adapters](../06-internals/05-platform-adapters.md).

### Application Types

One programming model, several kinds of entry point.

| Type | Created with | Use for |
|---|---|---|
| **HTTP application** | `NestFactory.create(AppModule)` | REST APIs, GraphQL, server-rendered pages |
| **Microservice** | `NestFactory.createMicroservice(AppModule, options)` | Message-driven services (TCP, Redis, NATS, MQTT, Kafka, RabbitMQ, gRPC) |
| **Hybrid application** | HTTP app plus `connectMicroservice()` | An API that also consumes messages |
| **Standalone application** | `NestFactory.createApplicationContext(AppModule)` | Scripts, CLIs, cron jobs, workers: the DI container with no network listener |
| **WebSocket server** | A gateway inside an HTTP application | Real-time communication |

Because providers are independent of the transport, the same `UsersService` can be called from an HTTP controller, a message handler, and a script.

### The Typical Layering

Nest does not enforce layers, but the conventional shape is:

```text
 Controller          translate HTTP ↔ method calls (thin)
     │
     ▼
 Service             business rules and orchestration
     │
     ▼
 Repository / client data access or external API calls
     │
     ▼
 Database / external system
```

Cross-cutting concerns (auth, validation, logging, error format) live in pipeline components, not in every controller.

### Convention Over Configuration

Nest relies on conventions that the CLI encodes: one folder and one module per feature, `*.controller.ts` / `*.service.ts` / `*.module.ts` file suffixes, DTO classes, `*.spec.ts` tests. The conventions make unfamiliar codebases readable, which is a large part of the framework's value.

---

## How It Works

What happens at startup and per request, at the highest level:

```text
 Startup  (once)
 ───────────────
 NestFactory.create(AppModule)
    1. Read @Module metadata starting at the root module; discover all imported modules
    2. Register providers and controllers per module
    3. Resolve each class's constructor dependencies (DI graph)
    4. Instantiate providers (singletons by default), then controllers
    5. Register routes with the HTTP adapter
    6. Run lifecycle hooks (onModuleInit, onApplicationBootstrap)
 app.listen(port)

 Per request
 ───────────
 adapter receives request
    → middleware
    → guards
    → interceptors (before)
    → pipes (transform and validate arguments)
    → controller handler → service → repository
    → interceptors (after)
    → serialize return value into the response
    (any thrown exception → exception filters → error response)
```

Everything in step 1 to 5 is driven by **metadata written by decorators**. See [TypeScript Decorators](../00-prerequisites/02-typescript-decorators.md) and [How Nest Boots](../06-internals/01-how-nest-boots.md).

---

## Basic Example

The smallest complete Nest application, in one file for clarity (real projects split it):

```typescript
// main.ts
import 'reflect-metadata';
import { Controller, Get, Injectable, Module } from '@nestjs/common';
import { NestFactory } from '@nestjs/core';

@Injectable()
class GreetingService {
  greet(name: string) {
    return `Hello, ${name}!`;
  }
}

@Controller('greetings')
class GreetingController {
  constructor(private readonly greetingService: GreetingService) {}

  @Get()
  hello() {
    return { message: this.greetingService.greet('world') };
  }
}

@Module({
  controllers: [GreetingController],
  providers: [GreetingService],
})
class AppModule {}

const app = await NestFactory.create(AppModule);
await app.listen(3000);
```

```bash
curl http://localhost:3000/greetings
# {"message":"Hello, world!"}
```

What this shows:

1. `AppModule` declares which controllers and providers exist.
2. `GreetingController` declares a dependency on `GreetingService` in its constructor. Nest supplies it.
3. `@Controller('greetings')` plus `@Get()` creates `GET /greetings`.
4. Returning an object produces a JSON response.
5. You never wrote `new GreetingService()` or touched Express.

(In a normal project, `reflect-metadata` is imported by Nest itself. Classes in separate files need regular imports, and ESM relative imports need `.js` extensions.)

---

## Practical Examples

### 1. Basic: Same Service, Different Entry Points

```typescript
// An HTTP controller and a standalone script share one provider
@Injectable()
export class ReportService { build() { return 'report'; } }

// HTTP
@Controller('reports')
export class ReportController {
  constructor(private readonly reports: ReportService) {}
  @Get() get() { return this.reports.build(); }
}

// Standalone script (no HTTP server)
const ctx = await NestFactory.createApplicationContext(AppModule);
console.log(ctx.get(ReportService).build());
await ctx.close();
```

This is why logic belongs in providers, not controllers: providers are reusable across entry points.

### 2. Common: A Feature Module That Exposes a Service

```typescript
@Module({
  controllers: [UsersController],
  providers: [UsersService],
  exports: [UsersService],        // other modules can inject UsersService
})
export class UsersModule {}

@Module({
  imports: [UsersModule],         // now OrdersService may inject UsersService
  providers: [OrdersService],
})
export class OrdersModule {}
```

### 3. Common: Choosing Fastify

```typescript
const app = await NestFactory.create<NestFastifyApplication>(AppModule, new FastifyAdapter());
```

Choose Fastify for throughput-sensitive services after checking that the middleware you need exists for it. Choose Express when you rely on its ecosystem. Measure before deciding.

### 4. Real-World: A Hybrid API and Worker

```typescript
const app = await NestFactory.create(AppModule);
app.connectMicroservice({ transport: Transport.RMQ, options: { urls: [process.env.RABBITMQ_URL!], queue: 'orders' } });
await app.startAllMicroservices();
await app.listen(3000);
```

One process serves HTTP and consumes queue messages, using the same providers. See [Hybrid Apps and API Gateway](../05-advanced/06-microservices/05-hybrid-apps-and-api-gateway.md).

### 5. Edge Case: Business Logic in a Controller

```typescript
@Controller('orders')
export class OrdersController {
  @Post()
  create(@Body() dto: CreateOrderDto) {
    if (dto.items.length === 0) throw new BadRequestException();
    const total = dto.items.reduce((s, i) => s + i.price * i.qty, 0);   // pricing logic here
    // ...talks to the database directly
  }
}
```

This works until you need the same logic from a message handler, a cron job, or a test without HTTP. Move rules into a service.

---

## Syntax / API / Commands

### Core Packages

| Package | Provides |
|---|---|
| `@nestjs/common` | Decorators (`@Module`, `@Controller`, `@Injectable`, `@Get`...), pipes, `HttpException` classes, `Logger` |
| `@nestjs/core` | `NestFactory`, the DI container, router, `Reflector`, lifecycle machinery |
| `@nestjs/platform-express` / `@nestjs/platform-fastify` | HTTP adapters |
| `@nestjs/testing` | `Test.createTestingModule` and helpers |
| `@nestjs/microservices` | Transports, `@MessagePattern`, `@EventPattern` |
| `@nestjs/websockets` + platform package | Gateways |
| `@nestjs/config`, `@nestjs/swagger`, `@nestjs/typeorm`, `@nestjs/mongoose`, `@nestjs/bullmq`, ... | Official integration packages |
| `reflect-metadata`, `rxjs` | Runtime dependencies of Nest itself |

### `NestFactory`

| Method | Returns | Purpose |
|---|---|---|
| `NestFactory.create(Module, options?)` | `INestApplication` | HTTP application |
| `NestFactory.createMicroservice(Module, options)` | `INestMicroservice` | Message-driven service |
| `NestFactory.createApplicationContext(Module)` | `INestApplicationContext` | DI container only |

Common `create` options: `logger`, `cors`, `bodyParser`, `abortOnError`, `routeConflictPolicy`, `routeResolutionStrategy` (v12).

---

## Important Rules

1. **Everything that Nest manages must belong to a module.** Unregistered controllers and providers do not exist as far as Nest is concerned.
2. **Controllers handle HTTP. Providers hold logic.** Keep controllers thin.
3. **Let the container create your objects.** Do not instantiate providers with `new`.
4. **Providers are singletons by default.** They are shared across all requests.
5. **Modules encapsulate.** A provider is private to its module unless exported, and consumers must import the exporting module.
6. **The pipeline order is fixed.** You choose which components to attach, not the order between types.
7. **Decorators describe, components act.** `@UseGuards()` records a guard. The framework runs it.
8. **Portability ends at `@Req()` and `@Res()`.** Prefer Nest abstractions for code that must work on both Express and Fastify.
9. **Transport-independent logic goes in providers**, so it can serve HTTP, messages, and scripts.

---

## Under the Hood

### The IoC Container

Nest's container holds a registry of providers per module, builds a dependency graph from constructor metadata, and instantiates in dependency order. It also manages lifetimes (singleton, request, transient) and calls lifecycle hooks. See [DI Internals](../06-internals/02-dependency-injection-internals.md).

### Module Graph

Modules form a directed graph: `imports` are edges. Nest walks it from the root module. A module cached once is reused, which is why importing the same module in many places does not create many instances.

### Adapter Boundary

```text
 Nest router + pipeline  ◄──►  HttpAdapter interface  ◄──►  Express  |  Fastify
```

Nest registers one handler per route with the adapter. When a request matches, the adapter calls Nest's internal handler, which runs guards, interceptors, pipes, and your method.

### Where Metadata Lives

`@Module`, `@Controller`, `@Get` and friends store metadata on classes and methods using `reflect-metadata`. Startup scanning reads it. This is why decorators are required and why they must be loaded before `NestFactory.create`. See [Metadata and Reflection](../06-internals/03-metadata-and-reflection.md).

---

## Common Patterns

### Feature Modules

One module per business capability (`UsersModule`, `OrdersModule`), imported into `AppModule`.

### Shared Module

A module that exports common providers (for example `DatabaseModule`) imported by feature modules.

### Thin Controller, Rich Service

Controllers adapt HTTP. Services own rules. Repositories own data access.

### Cross-Cutting Concerns in the Pipeline

Authentication in a guard, validation in a pipe, error format in a filter, timing in an interceptor. Each written once and applied globally or selectively.

### Standalone Context for Scripts

`createApplicationContext` for migrations, seed scripts, and one-off jobs that reuse providers.

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Everything in `AppModule` | Huge module, unclear boundaries | No feature modules | One module per feature |
| Business logic in controllers | Hard to test or reuse | Controller doing the service's job | Move to a service |
| `new SomeService()` in a class | Missing dependencies, untestable | Bypasses the container | Inject via constructor |
| Provider not registered | `Nest can't resolve dependencies` | Not in any `providers` array | Register it in the owning module |
| Provider not exported | Same error across modules | Providers are private by default | Export it and import the module |
| Assuming per-request instances | Shared state leaks between users | Providers are singletons | Keep services stateless, or use request scope deliberately |
| Mixing Express objects everywhere | Cannot switch to Fastify, hard to test | `@Req()` / `@Res()` leak platform types | Use Nest decorators and DTOs |
| Treating decorators as magic | Behavior unexplained | Decorators only record metadata | Learn what reads each one ([Decorators](./06-decorators.md)) |
| Adding guards/pipes without understanding order | Surprising execution order | Fixed pipeline ordering | Read [Request Lifecycle](./09-request-lifecycle.md) |
| Choosing Fastify without checking middleware | Missing Express-only middleware | Different ecosystems | Verify required middleware before switching |

---

## Debugging

| Question | Where to look |
|---|---|
| Is my controller/provider registered? | The module's `controllers` / `providers` arrays |
| Why can't X inject Y? | Is Y provided and exported, and is its module imported? |
| Which routes exist? | Startup route logs (`debug`/`verbose` logger levels), Nest Devtools, `app.getHttpAdapter().getInstance()` router inspection |
| Which platform am I on? | `app.getHttpAdapter().getType()` returns `'express'` or `'fastify'` |
| What does the module graph look like? | REPL `debug()`, Nest Devtools (development only) |

```typescript
const app = await NestFactory.create(AppModule, { logger: ['error', 'warn', 'log', 'debug', 'verbose'] });
console.log(app.getHttpAdapter().getType());
```

See [Debugging](../01-getting-started/06-debugging.md).

---

## Performance

- Nest's overhead per request is small. The adapter and your own code dominate.
- Fastify generally handles more requests per second than Express in raw benchmarks. Measure your own workload, because database and downstream calls usually dominate.
- Singleton providers avoid per-request construction. Request-scoped providers add cost (a new instance per request, and the scope bubbles up to dependants). Use them only when necessary.
- Startup time grows with the number of modules and providers. It is rarely a problem for long-running servers, but matters for serverless.

---

## Security

- The architecture gives you **places** to put security (guards, pipes, filters, middleware), but nothing is enforced by default. A fresh app has no authentication, validation, or rate limiting.
- Prefer **global** guards and pipes with explicit opt-outs, so a forgotten decorator does not leave a route open.
- Shared singleton state is shared across users. Never store per-user data on a singleton provider.

See [Security Fundamentals](../07-production/01-security/01-security-fundamentals.md).

---

## Production Considerations

- Treat module boundaries as your internal architecture. They decide what can later be extracted into a separate service. See [Modular Monolith](../08-architecture-and-patterns/01-architecture/02-modular-monolith.md).
- Keep transport concerns (HTTP, messages) out of providers so a service can move between deployment shapes.
- Standardize cross-cutting concerns (error format, logging, validation) globally, so every endpoint behaves the same.
- Pin the platform choice and document it. Switching adapters late is possible but not free.

---

## Best Practices

### Recommended

```typescript
@Controller('orders')
export class OrdersController {
  constructor(private readonly orders: OrdersService) {}

  @Post()
  create(@Body() dto: CreateOrderDto) {
    return this.orders.create(dto);           // one call, no rules here
  }
}
```

### Avoid

```typescript
@Controller('orders')
export class OrdersController {
  private readonly db = new Pool();           // infrastructure created by hand

  @Post()
  async create(@Req() req: Request, @Res() res: Response) {   // platform objects, manual response
    // validation, pricing, persistence, and formatting all inline
    res.status(201).send(/* ... */);
  }
}
```

Why: the recommended controller is a thin adapter that works with any platform and can be tested without HTTP. The avoided one is bound to Express, creates its own infrastructure, and mixes concerns.

Additional guidance:

- Draw the module graph before the code grows. Clear dependency direction (feature to shared, never the reverse) prevents circular dependencies later.
- Follow the CLI's file naming. Tooling and teammates depend on it.
- Prefer global defaults for cross-cutting behavior and local overrides for exceptions.

---

## Version / Compatibility Notes

| Item | NestJS 12 | Earlier |
|---|---|---|
| Core packages | ESM (consumable from CommonJS via `require(esm)`) | CommonJS |
| Default HTTP platform | Express (v5 line since Nest 11) | Express 4 in Nest 10 and earlier. *Verify your installed `@nestjs/platform-express` and `express` versions* |
| New project defaults | ESM or CommonJS prompt, Vitest/Jest, oxlint | CommonJS, Jest, ESLint |
| Route conflict handling | Opt-in diagnostics and `specificity` resolution | Declaration order only |
| Standard Schema | Supported in route decorators, config, serialization | Not available |
| Observability | Official `@nestjs/observe` SDK (opt-in) | Third-party APM only |
| Node.js | 20.19+ or 22.12+ to run | 20+ (v11) |

*Verify against the official migration guide for your versions.*

---

## Real-World Use Cases

- **REST backends** for web and mobile apps.
- **API gateways** composing several services.
- **Microservice fleets** with a shared project structure and conventions.
- **Event consumers and workers** using standalone or microservice applications.
- **Real-time features** (chat, notifications) using gateways beside HTTP controllers.
- **Internal tooling and scripts** using the DI container without a server.
- **Enterprise teams** that need a consistent structure across many developers and services.

---

## Interview Questions

### Beginner

1. What is NestJS, and how does it relate to Express?
   - A TypeScript framework that provides architecture (modules, controllers, providers, DI, a request pipeline) on top of an HTTP library, Express by default or Fastify.
2. What are the three main building blocks?
   - Modules, controllers, and providers.
3. What does a controller do versus a service?
   - Controllers map requests to handler methods. Services hold business logic.
4. Why is NestJS called opinionated?
   - It prescribes structure and conventions instead of leaving them to each team.

### Intermediate

1. List the request pipeline components in order.
   - Middleware, guards, interceptors (before), pipes, handler, interceptors (after), with exception filters handling thrown errors.
2. What is the IoC container responsible for?
   - Building the dependency graph, instantiating providers, injecting dependencies, managing scopes, and calling lifecycle hooks.
3. What are the application types Nest can create?
   - HTTP, microservice, hybrid, standalone application context, and WebSocket servers (as gateways).
4. Why can you usually switch between Express and Fastify?
   - Application code targets Nest abstractions and adapters hide the platform. Platform-specific objects and middleware reduce portability.
5. Why should providers not depend on HTTP types?
   - So the same logic can serve HTTP, messages, and scripts, and can be tested without a server.

### Advanced

1. Describe what happens during `NestFactory.create()`.
   - Scan module metadata, build the dependency graph, instantiate providers and controllers, register routes on the adapter, and run lifecycle hooks.
2. What are the trade-offs of Nest's singleton-by-default providers?
   - Efficient and simple, but state is shared across requests, so services must be stateless or use request scope deliberately (which adds cost and bubbles up).
3. How do module boundaries relate to later microservice extraction?
   - Well-defined modules with explicit exports are natural seams. A modular monolith can split along module lines.
4. When would you pick `createApplicationContext`?
   - Scripts, migrations, workers, and tasks that need providers but no network server.
5. What are the downsides of a framework this opinionated?
   - Learning curve, decorator and metadata coupling, and some indirection, in exchange for consistency and testability.

---

## Quick Reference

```text
Building blocks     Module · Controller · Provider (+ DI container)
Pipeline            Middleware → Guards → Interceptors(before) → Pipes → Handler → Interceptors(after)
                    (errors → Exception filters)
Platforms           Express (default) · Fastify
App types           create · createMicroservice · hybrid (connectMicroservice) · createApplicationContext
Layering            Controller → Service → Repository
Singletons          providers are shared across requests by default
Visibility          private to module unless exported + module imported
Rule                logic in providers, HTTP in controllers, cross-cutting in the pipeline
```

---

## Key Takeaways

- Nest adds architecture on top of Express or Fastify: modules, controllers, providers, dependency injection, and a fixed request pipeline.
- Modules organize and encapsulate. Controllers route. Providers hold logic. The container wires them together.
- Cross-cutting concerns live in pipeline components, written once and attached globally or selectively.
- One programming model serves HTTP, messages, WebSockets, and standalone scripts.
- Decorators record metadata at definition time. Startup scanning turns it into a running application.
- Providers are singletons by default, so keep them stateless and keep per-request data out of them.
- Nest gives you the places for security, validation, and consistency, but you must fill them.

---

## Related Topics

```text
01-getting-started
      ↓
[01 Architecture Overview]
      ↓
02 Modules  →  03 Controllers  →  04 Providers and Services  →  05 Dependency Injection
```

- [Fundamentals Overview](./README.md)
- [Modules](./02-modules.md)
- [Controllers](./03-controllers.md)
- [Providers and Services](./04-providers-and-services.md)
- [Request Lifecycle](./09-request-lifecycle.md)
- [How Nest Boots](../06-internals/01-how-nest-boots.md)
- [Platform Adapters](../06-internals/05-platform-adapters.md)
- [Modular Monolith](../08-architecture-and-patterns/01-architecture/02-modular-monolith.md)
