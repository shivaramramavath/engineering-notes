# Providers and Services

A provider is any class (or value) that Nest's container can create and inject into other classes. Services, repositories, factories, and helpers are all providers. The most common kind, the **service**, holds business logic and is injected into controllers and other services. If controllers are the HTTP surface of your application, providers are everything behind it. This file explains what a provider is, how to write a good service, what "singleton by default" means for your code, and when to reach for the other provider forms. The mechanism that wires providers together, dependency injection, is the subject of the next file.

---

## Overview

**What it is.** A class decorated with `@Injectable()` and listed in a module's `providers` array. The decorator tells Nest the class can be managed by its inversion-of-control (IoC) container.

**Why it exists.** To separate responsibilities. Controllers handle HTTP, providers hold logic. Because the container creates providers and injects them, classes do not construct their own collaborators, so they stay decoupled and testable.

**Where it is used.** Business rules, data access, calls to external APIs, caches, mailers, configuration wrappers, schedulers, anything reusable across controllers, message handlers, and scripts.

**Why you should understand it.**

- Providers are **singletons by default**, so what you store on them is shared across all requests.
- "Provider" is broader than "service". Many Nest concepts (guards, pipes, interceptors, filters) are injectable too.
- Most real-world design questions (where does this logic go? how do I test it?) reduce to how you structure providers.

---

## Mental Model

```text
 ┌──────────────┐    needs     ┌──────────────┐    needs     ┌────────────────┐
 │ Controller   │ ───────────► │  Service     │ ───────────► │ Repository /   │
 │ (HTTP)       │              │  (rules)     │              │ API client     │
 └──────────────┘              └──────────────┘              └────────────────┘
        │                              │                              │
        └────── all three are created ONCE by the container ──────────┘
               and handed to whoever declares them in a constructor
```

A provider is **a unit of reusable behavior that the container owns**. Your code declares what it needs. The container supplies it.

---

## Core Concepts

### What Counts as a Provider

| Kind | Example | Notes |
|---|---|---|
| Service | `UsersService` | Business logic. The common case |
| Repository | `UsersRepository` | Data access |
| Factory | `useFactory` provider | Builds a value (a client, a connection) |
| Helper | `PasswordHasher`, `Clock` | Reusable utility with dependencies |
| Value | `{ provide: 'CONFIG', useValue: {...} }` | Constant or pre-built object |
| Client wrapper | `StripeClient` | Third-party SDK behind an injectable class |

Plain functions and static helpers do not need to be providers. Make something a provider when it **has dependencies, holds shared state or resources, or needs to be replaced in tests**.

### The Service

```typescript
import { Injectable } from '@nestjs/common';
import type { Cat } from './interfaces/cat.interface.js';

@Injectable()
export class CatsService {
  private readonly cats: Cat[] = [];

  create(cat: Cat) {
    this.cats.push(cat);
  }

  findAll(): Cat[] {
    return this.cats;
  }
}
```

- `@Injectable()` attaches metadata declaring that `CatsService` is managed by the IoC container (and makes the compiler emit its constructor parameter types, which injection depends on).
- `import type` is right for `Cat` because it is an **interface** used only as a type.
- Generate one with `nest g service cats`.

### Using a Provider

```typescript
@Controller('cats')
export class CatsController {
  constructor(private readonly catsService: CatsService) {}   // injected

  @Get()
  findAll() {
    return this.catsService.findAll();
  }
}
```

### Registration

A provider must be listed in a module's `providers`. Otherwise the container does not know it exists.

```typescript
@Module({
  controllers: [CatsController],
  providers: [CatsService],
})
export class CatsModule {}
```

To use it from another module, also add it to `exports` and import the module (see [Modules](./02-modules.md)).

### Singleton by Default

By default a provider's lifetime matches the application's. Nest creates it once at bootstrap, shares the instance with every class that depends on it, and destroys it at shutdown.

| Scope | Lifetime | Use |
|---|---|---|
| `DEFAULT` (singleton) | One instance for the application | Almost always |
| `REQUEST` | New instance per request | Per-request state (tenant, user context). Has a cost |
| `TRANSIENT` | New instance for each consumer | Rare |

Consequence: **a service is shared by all requests and all users.** Mutable fields on a singleton service are shared state. Details: [Scopes and Request Context](../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md).

```typescript
@Injectable()
export class SessionService {
  private currentUser?: User;      // BUG: shared between all concurrent requests
  setUser(u: User) { this.currentUser = u; }
}
```

Keep per-request data in method arguments, in the request object, or in request-scoped providers (or `AsyncLocalStorage`), never in singleton fields.

### Property-Based Injection

Constructor injection is the default. Property injection exists for cases where passing dependencies through `super()` from every subclass is cumbersome.

```typescript
@Injectable()
export class HttpService<T> {
  @Inject('HTTP_OPTIONS')
  private readonly httpClient: T;
}
```

If the class does not extend another class, prefer constructor injection. The constructor states the dependencies explicitly.

### Optional Providers

```typescript
@Injectable()
export class HttpService<T> {
  constructor(@Optional() @Inject('HTTP_OPTIONS') private httpClient: T) {}
}
```

If nothing is registered under that token, Nest injects `undefined` instead of failing at startup. The class must supply its own fallback.

> **NestJS 12 change:** `@Optional()` markers are read with `Reflect.getOwnMetadata`, so a **subclass no longer inherits** its parent's optional markers. A subclass without its own constructor keeps the parent's parameter types but loses their optional status, and Nest throws `UnknownDependenciesException` where v11 injected `undefined`. Give the subclass its own constructor and redeclare `@Optional()`.

### Custom Providers (Preview)

Not every provider is "a class registered by itself".

```typescript
@Module({
  providers: [
    CatsService,                                                     // shorthand for { provide: CatsService, useClass: CatsService }
    { provide: 'API_URL', useValue: 'https://api.example.com' },     // a value
    { provide: PaymentGateway, useClass: StripeGateway },            // swap an implementation
    { provide: 'DB', useFactory: () => createClient(), inject: [] }, // computed value
  ],
})
export class AppModule {}
```

Details: [Custom Providers](../03-core-concepts/04-modules-and-di/05-custom-providers.md).

### Manual Retrieval

Occasionally you must step outside constructor injection (dynamic resolution, standalone scripts):

- `ModuleRef` retrieves or instantiates providers dynamically ([ModuleRef and Lazy Loading](../03-core-concepts/04-modules-and-di/08-module-ref-and-lazy-loading.md)).
- `app.get(Service)` on a standalone application context in `bootstrap()`.

---

## How It Works

Nest's container handles each provider through the same lifecycle:

```text
 1. Declared    @Injectable() class + listed in Module.providers
        │
 2. Scanned     container records it in the module's provider registry
        │
 3. Resolved    constructor parameter types are read (design:paramtypes)
        │        and each dependency is located (see Dependency Injection)
        │
 4. Created     `new CatsService(...deps)` called ONCE (singleton scope)
        │
 5. Initialized onModuleInit / onApplicationBootstrap hooks run
        │
 6. Injected    the same instance is passed to every consumer
        │
 7. Destroyed   onModuleDestroy / beforeApplicationShutdown / onApplicationShutdown at shutdown
```

Dependency order is respected: a service is created after everything it depends on.

---

## Basic Example

```typescript
// tasks/tasks.service.ts
import { Injectable, NotFoundException } from '@nestjs/common';

export interface Task { id: number; title: string; done: boolean }

@Injectable()
export class TasksService {
  private readonly tasks = new Map<number, Task>();
  private nextId = 1;

  create(title: string): Task {
    const task: Task = { id: this.nextId++, title, done: false };
    this.tasks.set(task.id, task);
    return task;
  }

  findAll(): Task[] {
    return [...this.tasks.values()];
  }

  findOne(id: number): Task {
    const task = this.tasks.get(id);
    if (!task) throw new NotFoundException(`Task ${id} not found`);
    return task;
  }

  complete(id: number): Task {
    const task = this.findOne(id);
    task.done = true;
    return task;
  }
}
```

```typescript
// tasks/tasks.controller.ts
@Controller('tasks')
export class TasksController {
  constructor(private readonly tasks: TasksService) {}

  @Post()   create(@Body('title') title: string) { return this.tasks.create(title); }
  @Get()    findAll() { return this.tasks.findAll(); }
  @Get(':id') findOne(@Param('id', ParseIntPipe) id: number) { return this.tasks.findOne(id); }
}
```

```typescript
// tasks/tasks.module.ts
@Module({ controllers: [TasksController], providers: [TasksService] })
export class TasksModule {}
```

What to notice:

1. The controller contains no rules. It only forwards.
2. The service throws `NotFoundException`, an HTTP-aware exception Nest converts into a `404`. (Throwing HTTP exceptions from services is common and pragmatic, though some teams prefer domain errors mapped by a filter. See [HTTP Exceptions](../03-core-concepts/01-request-pipeline/07-http-exceptions.md).)
3. The in-memory `Map` is **shared state**, which is fine for a learning example and wrong for production (restarts lose data, multiple instances diverge). You replace it with a repository in [Database Foundations](../04-intermediate/02-database-foundations/README.md).

---

## Practical Examples

### 1. Basic: A Service Depending on Another Service

```typescript
@Injectable()
export class NotificationsService {
  send(userId: number, message: string) { /* ... */ }
}

@Injectable()
export class TasksService {
  constructor(private readonly notifications: NotificationsService) {}

  complete(id: number) {
    const task = this.findOne(id);
    task.done = true;
    this.notifications.send(task.ownerId, `Task ${task.title} done`);
    return task;
  }
}
```

Both must be resolvable: either in the same module's `providers`, or one exported from another module that the first imports.

### 2. Common: A Repository Provider Behind a Service

```typescript
@Injectable()
export class UsersRepository {
  constructor(@Inject('DB') private readonly db: Database) {}
  findById(id: number) { return this.db.query('SELECT * FROM users WHERE id = $1', [id]); }
}

@Injectable()
export class UsersService {
  constructor(private readonly repo: UsersRepository) {}
  async get(id: number) {
    const user = await this.repo.findById(id);
    if (!user) throw new NotFoundException();
    return user;
  }
}
```

The service holds rules (not found), the repository holds data access. See [Repository Pattern](../04-intermediate/02-database-foundations/03-repository-pattern.md).

### 3. Common: Wrap a Third-Party SDK

```typescript
@Injectable()
export class StripeClient {
  private readonly stripe = new Stripe(process.env.STRIPE_API_KEY!);   // better: inject validated config
  charge(amountCents: number, customerId: string) { /* ... */ }
}
```

Wrapping the SDK in a provider gives you one place to configure it, mock it in tests, and swap it later.

### 4. Real-World: Swap an Implementation by Token

```typescript
export abstract class PaymentGateway {
  abstract charge(amountCents: number): Promise<string>;
}

@Injectable() export class StripeGateway extends PaymentGateway { /* ... */ }
@Injectable() export class FakeGateway extends PaymentGateway { /* ... */ }

@Module({
  providers: [
    {
      provide: PaymentGateway,
      useClass: process.env.NODE_ENV === 'test' ? FakeGateway : StripeGateway,
    },
  ],
  exports: [PaymentGateway],
})
export class PaymentsModule {}
```

Consumers inject `PaymentGateway` (an abstract **class**, which exists at runtime) and never know which implementation they got.

### 5. Real-World: Provider With Lifecycle Hooks

```typescript
@Injectable()
export class CacheService implements OnModuleInit, OnModuleDestroy {
  private client!: RedisClient;

  async onModuleInit() { this.client = await connect(); }
  async onModuleDestroy() { await this.client.quit(); }
}
```

Hooks manage resources (connections) safely. In NestJS 12, hooks are called by component hierarchy level, so avoid relying on one provider's hook having run before another's. See [Application Lifecycle](../03-core-concepts/04-modules-and-di/09-application-lifecycle.md).

### 6. Edge Case: Instantiating a Service With `new`

```typescript
@Controller('tasks')
export class TasksController {
  private readonly tasks = new TasksService();     // BUG
}
```

This bypasses the container. `TasksService`'s own dependencies are `undefined`, the instance is not shared, and you cannot replace it in tests.

### 7. Edge Case: Unregistered Provider

```typescript
@Controller('tasks')
export class TasksController {
  constructor(private readonly tasks: TasksService) {}   // TasksService not in any providers array
}
```

Startup fails with `Nest can't resolve dependencies of the TasksController (?)`. Register `TasksService` in the module's `providers`.

### 8. Edge Case: A Subclass Loses `@Optional()` (v12)

```typescript
@Injectable()
class Base { constructor(@Optional() protected readonly options?: Options) {} }

@Injectable()
class Child extends Base {}          // v12: optional marker not inherited → UnknownDependenciesException

@Injectable()
class ChildFixed extends Base {
  constructor(@Optional() options?: Options) { super(options); }   // redeclare
}
```

---

## Syntax / API / Commands

| Item | Purpose |
|---|---|
| `@Injectable()` | Mark a class as injectable |
| `providers: [...]` in `@Module()` | Register providers |
| `exports: [...]` | Share a provider with importing modules |
| `@Inject(token)` | Inject by token (non-class dependencies, custom tokens) |
| `@Optional()` | Allow a missing dependency (`undefined` is injected) |
| `{ provide, useClass \| useValue \| useFactory \| useExisting }` | Custom provider forms |
| `@Injectable({ scope: Scope.REQUEST })` | Request-scoped provider |
| `OnModuleInit`, `OnApplicationBootstrap`, `OnModuleDestroy`, `BeforeApplicationShutdown`, `OnApplicationShutdown` | Lifecycle hook interfaces |
| `nest g service <name>` | Generate a service and register it |
| `ModuleRef` | Retrieve or create providers dynamically |

---

## Important Rules

1. **Add `@Injectable()`** to every class that is injected or that has injected dependencies.
2. **Register every provider** in a module's `providers`. Export it to share it.
3. **Never create providers with `new`.** Let the container do it.
4. **Providers are singletons by default.** Do not keep per-request or per-user state in their fields.
5. **Inject classes (or explicit tokens), not interfaces.** Interfaces vanish at runtime.
6. **Prefer constructor injection.** Use property injection mainly for base-class situations.
7. **`@Optional()` only changes behavior when the provider is missing.** The class supplies the fallback.
8. **In NestJS 12, `@Optional()` is not inherited.** Redeclare it in subclass constructors.
9. **Keep services free of HTTP types.** No `Request`/`Response` in services, so they can serve other transports and tests.
10. **A service should have a clear responsibility.** If the constructor lists ten dependencies, the class is doing too much.

---

## Under the Hood

### What `@Injectable()` Actually Does

It attaches a marker and, because it is a decorator on the class, causes TypeScript (with `emitDecoratorMetadata`) to emit `design:paramtypes` for the constructor. The container reads that array to learn the dependencies. Without a decorator, no metadata is emitted and injection fails. See [TypeScript Decorators](../00-prerequisites/02-typescript-decorators.md).

### The Provider Registry

Each module holds a map from **token** to **provider wrapper** (instance, scope, factory, dependencies). The default token of a class provider is the class itself. Custom providers use strings, symbols, or abstract classes as tokens.

### Singleton Instance Lifetime

A singleton wrapper holds one instance after creation. Every injection of that token returns it. The instance lives until `app.close()` or process exit.

### Scope Bubbling

If a request-scoped provider is injected into a singleton, the singleton becomes request-scoped too, as does everything depending on it, up to controllers. One careless request-scoped provider can make a whole subgraph per-request. See [Scopes and Request Context](../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md).

### Standalone Retrieval

`const ctx = await NestFactory.createApplicationContext(AppModule); ctx.get(TasksService)` gives access to the same providers without HTTP.

---

## Common Patterns

### Service per Aggregate or Use Case

`UsersService`, `OrdersService`. Larger domains split into smaller services or use-case classes.

### Repository + Service Split

Repository: queries and persistence. Service: rules and orchestration.

### Adapter/Wrapper for External Systems

One injectable class per external dependency (payments, mail, storage). Easy to mock and replace.

### Token-Based Substitution

An abstract class (or token) plus `useClass` selecting the implementation by environment.

### Stateless Services, Explicit Context

Pass user or tenant context as method arguments, not through singleton fields.

### Lifecycle for Resources

Open connections in `onModuleInit`, close them in `onModuleDestroy`.

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Missing `@Injectable()` | Dependencies `undefined`, resolution errors | No decorator, no metadata | Add it |
| Provider not in `providers` | `Nest can't resolve dependencies` | Not registered | Register in the module |
| Provider not exported | Same error in other modules | Private by default | `exports` + import the module |
| `new Service()` inside a class | Missing dependencies, untestable | Bypasses DI | Inject |
| Per-request data in a singleton field | Data leaks between users | Shared instance | Pass as arguments, use request scope/ALS deliberately |
| Interface as injection type | Resolution error naming `Object` | Interfaces erased | Use a class or token |
| `import type` for an injected class | Dependency unresolved | Import erased | Normal import |
| Huge service (God object) | Hard to test, many dependencies | No separation | Split by responsibility |
| HTTP types in services | Cannot reuse for other transports | `Request`/`Response` leaked | Pass plain data |
| Optional dependency missing in subclass (v12) | `UnknownDependenciesException` | Markers not inherited | Redeclare `@Optional()` in the subclass constructor |
| Request-scoped provider added casually | Performance drop, many instances | Scope bubbles up | Use only where needed |
| Work in the constructor (I/O, async) | Race conditions, unhandled errors | Constructors cannot be async | Use `onModuleInit` or an async factory provider |

---

## Debugging

| Symptom | Check |
|---|---|
| `Nest can't resolve dependencies of X (?)` | Registered? Exported? Module imported? Class not interface? Normal import? |
| Dependency is `undefined` at runtime | Missing `@Injectable()`, circular import, `import type`, metadata flags |
| State appears shared across requests | Singleton fields holding per-request data |
| Provider created more than once | Listed in multiple modules, or request/transient scope |
| Provider never created | Module unreachable from the root, or provider never injected (singletons are created at bootstrap if registered and reachable) |
| Hook order surprises after upgrading | v12 hierarchy-level ordering |

```typescript
@Injectable()
export class TasksService {
  private readonly logger = new Logger(TasksService.name);
  constructor() { this.logger.debug('TasksService created'); }   // how many times do you see this?
}
```

If the line prints once, it is a singleton. Printing per request means request scope.

---

## Performance

- Singleton creation is a one-time cost at startup.
- Request-scoped providers add allocation and resolution cost per request and make dependants request-scoped. Avoid on hot paths.
- Keep constructors cheap. Heavy initialization belongs in lifecycle hooks or factories.
- Services should avoid blocking the event loop (no synchronous CPU-heavy work, no `*Sync` I/O). See [Node.js Async and Event Loop](../00-prerequisites/03-nodejs-async-and-event-loop.md).
- In-memory caches on singletons are fast, but are per process. Behind multiple instances, use a shared cache.

---

## Security

- **Singleton state is shared across users.** Never store user identity, tokens, or request data on a service field.
- **Do not log secrets** from service methods or constructors.
- **Inject validated configuration** rather than reading `process.env` in many places. See [Configuration Basics](../03-core-concepts/03-configuration/01-configuration-basics.md).
- **Wrap secrets-bearing SDK clients** in providers so credentials are configured once.
- **Authorization belongs in guards and in service checks that need data.** Do not assume the controller's guard covers every caller (a service can be called from a message handler or script).

---

## Production Considerations

- **Stateless services scale horizontally.** Keep state in databases, caches, or queues, not in process memory.
- **Resource lifecycle:** open and close connections through hooks. Enable shutdown hooks (`app.enableShutdownHooks()`) so `onModuleDestroy`/`onApplicationShutdown` run on `SIGTERM`.
- **Graceful degradation:** wrap external clients with timeouts and error handling.
- **Testability:** services depend on abstractions you can replace in `Test.createTestingModule`.
- **Observability:** log at service boundaries with context names (`new Logger(Service.name)`).

---

## Best Practices

### Recommended

```typescript
@Injectable()
export class OrdersService {
  constructor(
    private readonly orders: OrdersRepository,
    private readonly payments: PaymentGateway,        // abstract class token
  ) {}

  async place(userId: number, items: Item[]) {
    const total = this.price(items);                  // rules live here
    const paymentId = await this.payments.charge(total);
    return this.orders.create({ userId, items, total, paymentId });
  }
}
```

### Avoid

```typescript
@Injectable()
export class OrdersService {
  private currentUser?: User;                         // shared across all requests
  private readonly db = new Pool();                   // infrastructure built by hand

  async place(req: Request) {                         // HTTP type in the service
    this.currentUser = req.user;
    // ...
  }
}
```

Why: the recommended service is stateless, depends on abstractions, and has no HTTP knowledge. The avoided one leaks state between users, cannot be tested without a database, and is tied to the web layer.

Additional guidance:

- Aim for a handful of constructor dependencies. More is a design signal.
- Name services after what they own (`BillingService`), not generic nouns (`HelperService`, `UtilsService`).
- Make side-effecting collaborators (mail, payments) injectable so tests can replace them.
- Use `readonly` on injected members.

---

## Version / Compatibility Notes

| Item | NestJS 12 | Earlier |
|---|---|---|
| `@Optional()` inheritance | Not inherited. Subclass must redeclare | Inherited |
| Lifecycle hook ordering | By component hierarchy level | Previous ordering |
| `HttpException` `errorCode` option | Available | Not available |
| ESM projects | `.js` extensions on relative imports. Type-only imports use `import type` | CommonJS style |
| Provider declaration forms (`useClass`, `useValue`, `useFactory`, `useExisting`) | Unchanged | Same |
| Docs examples | Show `import type` for interfaces and normal imports for classes (important under `isolatedModules`) | Often a plain import for both |

*Verify against the official providers chapter and migration guide.*

---

## Real-World Use Cases

- **Domain services** encapsulating business rules.
- **Repositories** wrapping ORMs.
- **Integration clients** for payments, email, SMS, storage, search.
- **Caching layers** behind a stable interface.
- **Feature flags and configuration providers.**
- **Shared helpers** (clock, ID generator, password hasher) injected so tests can control them.
- **Schedulers and job producers** triggered from many entry points.

---

## Interview Questions

### Beginner

1. What is a provider in NestJS?
   - A class (or value) the IoC container can create and inject. Services, repositories, factories, and helpers are providers.
2. What does `@Injectable()` do?
   - Marks the class as manageable by the container and enables constructor type metadata for injection.
3. Where do you register a provider?
   - In the `providers` array of a module.
4. What is the difference between a controller and a service?
   - Controllers handle HTTP routing. Services hold business logic.

### Intermediate

1. What does "singleton by default" imply for your code?
   - One shared instance across all requests, so services must not hold per-request state.
2. How do you share a provider with another module?
   - Export it and import the module.
3. Constructor vs property injection?
   - Constructor is explicit and preferred. Property injection is mainly useful with base classes.
4. What does `@Optional()` do?
   - Injects `undefined` instead of failing when the provider is missing. The class provides defaults.
5. Why can't you inject an interface?
   - It is erased at runtime. Use a class (often abstract) or a token with `@Inject()`.

### Advanced

1. What changed about `@Optional()` in NestJS 12?
   - Markers are no longer inherited by subclasses, so a subclass must declare its own constructor and redeclare `@Optional()`.
2. Explain scope bubbling.
   - A provider depending on a request-scoped provider becomes request-scoped too, and so does everything up the chain to the controller.
3. How would you swap implementations per environment?
   - Define an abstract class or token and register `{ provide: Token, useClass: ... }` selecting by environment or config.
4. Why shouldn't services depend on `Request`/`Response`?
   - It couples them to HTTP and the platform, blocking reuse from other entry points and complicating tests.
5. How do you manage resources like connections in providers?
   - Lifecycle hooks (`onModuleInit`, `onModuleDestroy`) or async factory providers, with shutdown hooks enabled.

---

## Quick Reference

```text
@Injectable()                       mark injectable (also emits constructor type metadata)
Module.providers: [Svc]             register  ·  Module.exports: [Svc]  share
constructor(private readonly s: Svc) {}    inject (class type = token)
@Inject('TOKEN')  @Optional()       explicit token · allow missing (v12: not inherited)
Custom              { provide, useClass | useValue | useFactory | useExisting }
Scopes              DEFAULT (singleton) · REQUEST · TRANSIENT  (request scope bubbles up)
Singleton rule      no per-request or per-user state in fields
Never               new Svc() · interfaces as injection types · import type on injected classes
Lifecycle           onModuleInit · onApplicationBootstrap · onModuleDestroy · onApplicationShutdown
CLI                 nest g service <name>
```

---

## Key Takeaways

- A provider is anything the container can create and inject. A service is the common case, holding business logic.
- Register providers in `providers`, share through `exports` and module imports, and never create them with `new`.
- Providers are singletons by default, so they must not hold per-request or per-user state.
- Inject classes or explicit tokens. Interfaces and type-only imports leave Nest nothing to resolve.
- Keep services free of HTTP types and focused on one responsibility.
- Use lifecycle hooks or async factories for resources, and enable shutdown hooks in production.
- In NestJS 12, `@Optional()` is not inherited, and hook ordering follows the component hierarchy.

---

## Related Topics

```text
03 Controllers
      ↓
[04 Providers and Services]
      ↓
05 Dependency Injection  →  03-core-concepts/04 Custom Providers
```

- [Fundamentals Overview](./README.md)
- [Modules](./02-modules.md)
- [Controllers](./03-controllers.md)
- [Dependency Injection](./05-dependency-injection.md)
- [Custom Providers](../03-core-concepts/04-modules-and-di/05-custom-providers.md)
- [Scopes and Request Context](../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md)
- [Application Lifecycle](../03-core-concepts/04-modules-and-di/09-application-lifecycle.md)
- [Repository Pattern](../04-intermediate/02-database-foundations/03-repository-pattern.md)
- [Unit Testing](../04-intermediate/01-testing/02-unit-testing.md)
