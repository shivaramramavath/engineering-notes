# Modules

A module is a class decorated with `@Module()` that groups related controllers and providers and decides what is shared with the rest of the application. Modules are how Nest organizes code and how it builds the dependency graph: Nest starts at the root module, follows `imports`, and registers everything it finds. Most "it can't resolve dependencies" errors are really module wiring mistakes, so this file is worth reading carefully.

---

## Overview

**What it is.** A class with `@Module({ imports, controllers, providers, exports })` metadata. Every application has at least one module (the root, usually `AppModule`).

**Why it exists.** To give code boundaries. A module encapsulates its providers: other modules can use them only if the module **exports** them and the consumer **imports** the module. This makes dependencies explicit and keeps large codebases comprehensible.

**Where it is used.** Everywhere. Each feature (users, orders, billing), each piece of shared infrastructure (database, configuration, logging), and each integration is typically a module.

**Why you should understand it.** Module wiring determines what can be injected where, how many instances of a provider exist, and how an application can later be split into services.

---

## Mental Model

A module is a **box with a public surface**:

```text
            imports (what I use from others)
                     │
                     ▼
 ┌──────────────── UsersModule ────────────────┐
 │                                             │
 │   controllers: [UsersController]            │
 │   providers:   [UsersService, UsersRepo]    │   ← private by default
 │                                             │
 └──────────────────────┬──────────────────────┘
                        │ exports: [UsersService]    ← the public surface
                        ▼
              other modules that import UsersModule
              may inject UsersService (but not UsersRepo)
```

Three rules cover most situations:

1. **Declare** controllers and providers in the module that owns them.
2. **Export** what other modules may use.
3. **Import** the module (not the provider) where you need it.

---

## Core Concepts

### `@Module()` Metadata

| Property | Meaning |
|---|---|
| `providers` | Providers instantiated by the Nest injector. Available inside this module |
| `controllers` | Controllers defined in this module |
| `imports` | Other modules whose **exported** providers this module needs |
| `exports` | Subset of this module's providers (or imported modules) made available to modules that import this one |

```typescript
import { Module } from '@nestjs/common';
import { UsersController } from './users.controller.js';
import { UsersService } from './users.service.js';

@Module({
  controllers: [UsersController],
  providers: [UsersService],
  exports: [UsersService],
})
export class UsersModule {}
```

### Encapsulation

Providers are **scoped to the module** that declares them. `UsersModule`'s `UsersService` is invisible to `OrdersModule` unless `UsersModule` exports it and `OrdersModule` imports `UsersModule`.

```typescript
@Module({ providers: [UsersService, UsersRepository], exports: [UsersService] })
export class UsersModule {}

@Module({ imports: [UsersModule], providers: [OrdersService] })
export class OrdersModule {}
// OrdersService may inject UsersService.
// It may NOT inject UsersRepository (not exported).
```

### The Root Module and the Module Graph

`NestFactory.create(AppModule)` begins at the root. Nest follows every `imports` array transitively. Anything not reachable from the root does not exist.

```text
                 AppModule
                /    |     \
      UsersModule  OrdersModule  ConfigModule
           ▲            │
           └────────────┘   OrdersModule imports UsersModule
```

### Feature Modules

A feature module holds everything for one capability. It is the default unit of organization.

```text
src/users/
├── users.module.ts
├── users.controller.ts
├── users.service.ts
├── dto/
└── entities/
```

Generate with `nest g module users` (or `nest g resource users`). The CLI registers the new module in the nearest parent.

### Shared Modules

Modules are **singletons**: once instantiated, the module and its providers are cached. Any number of modules can import the same module and will receive the **same provider instances**. This is how you share one database connection or one configuration service.

```typescript
@Module({
  providers: [DatabaseService],
  exports: [DatabaseService],
})
export class DatabaseModule {}

@Module({ imports: [DatabaseModule] }) export class UsersModule {}
@Module({ imports: [DatabaseModule] }) export class OrdersModule {}
// Both receive the SAME DatabaseService instance.
```

A provider listed in the `providers` arrays of **two different modules** is two separate instances, one per module. Share through exports and imports, not by repeating the provider.

### Module Re-Exporting

A module can export modules it imports, so consumers get their providers without importing them directly.

```typescript
@Module({
  imports: [DatabaseModule, ConfigModule],
  exports: [DatabaseModule, ConfigModule],
})
export class CoreModule {}
```

Importing `CoreModule` now also gives access to what `DatabaseModule` and `ConfigModule` export.

### Global Modules

`@Global()` makes a module's exports available everywhere without importing it.

```typescript
@Global()
@Module({ providers: [ConfigService], exports: [ConfigService] })
export class AppConfigModule {}
```

Use sparingly. Global modules hide dependencies, which weakens the explicitness modules exist to provide. Reasonable candidates: configuration, logging. See [Global Modules](../03-core-concepts/04-modules-and-di/02-global-modules.md).

### Dynamic Modules

A dynamic module is configured when imported, typically through a static `forRoot()` or `forRootAsync()` method that returns module metadata.

```typescript
@Module({
  imports: [
    TypeOrmModule.forRoot({ /* connection options */ }),
    ConfigModule.forRoot({ isGlobal: true }),
  ],
})
export class AppModule {}
```

You will mostly consume dynamic modules at this stage. Writing your own is covered in [Dynamic Modules](../03-core-concepts/04-modules-and-di/03-dynamic-modules.md).

### Module Classes Can Inject Providers

A module class is itself instantiable and can use constructor injection, for example to run setup code.

```typescript
@Module({ providers: [CatsService] })
export class CatsModule {
  constructor(private readonly catsService: CatsService) {}
}
```

Module classes cannot be injected into providers (circular dependency risk).

---

## How It Works

How Nest turns module metadata into a working graph:

```text
 NestFactory.create(AppModule)
        │
        ▼
 1. Scanner reads AppModule metadata
        │
        ▼
 2. For each entry in `imports`: recurse (modules are cached, so each is processed once)
        │
        ▼
 3. Register per module:
        providers   → provider registry of THAT module
        controllers → controller list of THAT module
        exports     → mark which providers are visible to importers
        │
        ▼
 4. Resolve dependencies per class:
        look in own module's providers
        → then in exports of each imported module
        → not found? throw UnknownDependenciesException
        │
        ▼
 5. Instantiate providers (dependency order), then controllers
        │
        ▼
 6. Run lifecycle hooks (onModuleInit, onApplicationBootstrap)
```

Visibility rule for resolution: **own providers + exports of imported modules (+ global modules' exports)**. Nothing else.

---

## Basic Example

A two-module application where one module uses another's service.

```typescript
// users/users.service.ts
import { Injectable } from '@nestjs/common';

@Injectable()
export class UsersService {
  private readonly users = [{ id: 1, name: 'Ada' }];
  findOne(id: number) {
    return this.users.find((u) => u.id === id);
  }
}
```

```typescript
// users/users.module.ts
import { Module } from '@nestjs/common';
import { UsersService } from './users.service.js';

@Module({
  providers: [UsersService],
  exports: [UsersService],          // public surface
})
export class UsersModule {}
```

```typescript
// orders/orders.service.ts
import { Injectable } from '@nestjs/common';
import { UsersService } from '../users/users.service.js';

@Injectable()
export class OrdersService {
  constructor(private readonly users: UsersService) {}

  describe(userId: number) {
    const user = this.users.findOne(userId);
    return `Order for ${user?.name ?? 'unknown'}`;
  }
}
```

```typescript
// orders/orders.module.ts
import { Module } from '@nestjs/common';
import { UsersModule } from '../users/users.module.js';
import { OrdersService } from './orders.service.js';

@Module({
  imports: [UsersModule],           // makes UsersService visible here
  providers: [OrdersService],
})
export class OrdersModule {}
```

```typescript
// app.module.ts
@Module({ imports: [UsersModule, OrdersModule] })
export class AppModule {}
```

What happens:

1. `UsersModule` exports `UsersService`. Without the export, `OrdersService` fails to resolve it.
2. `OrdersModule` imports `UsersModule`. Without the import, the same failure occurs.
3. `AppModule` imports both so they are reachable from the root. Importing `OrdersModule` alone would still pull in `UsersModule` transitively through `OrdersModule`'s own `imports`.
4. `UsersService` is created once and shared.

---

## Practical Examples

### 1. Basic: A Feature Module

```bash
nest g module tasks
nest g controller tasks
nest g service tasks
```

The CLI adds `TasksController` and `TasksService` to `TasksModule` (generate the module first so they register there), and `TasksModule` to `AppModule`.

### 2. Common: Share Infrastructure With a Module

```typescript
@Module({
  providers: [{ provide: 'REDIS', useFactory: () => createRedisClient() }],
  exports: ['REDIS'],
})
export class RedisModule {}
```

Any module that imports `RedisModule` can inject `@Inject('REDIS')`. Custom providers and factories: [Custom Providers](../03-core-concepts/04-modules-and-di/05-custom-providers.md).

### 3. Common: A Core Module That Re-Exports

```typescript
@Module({
  imports: [ConfigModule.forRoot({ isGlobal: false }), DatabaseModule],
  exports: [ConfigModule, DatabaseModule],
})
export class CoreModule {}
```

Feature modules import only `CoreModule`.

### 4. Real-World: Dependency Direction

```text
        AppModule
       /    |     \
  Users   Orders   Billing        feature modules
      \     |     /
        CoreModule                shared infrastructure
```

Features depend on shared infrastructure, never the reverse. If `CoreModule` imports a feature module, you are heading for a circular dependency.

### 5. Real-World: Module-Level Setup With Lifecycle Hooks

```typescript
@Module({ providers: [SeedService] })
export class SeedModule implements OnModuleInit {
  constructor(private readonly seed: SeedService) {}

  async onModuleInit() {
    if (process.env.NODE_ENV !== 'production') await this.seed.run();
  }
}
```

Lifecycle hooks are covered in [Application Lifecycle](../03-core-concepts/04-modules-and-di/09-application-lifecycle.md).

### 6. Edge Case: Duplicate Providers Create Duplicate Instances

```typescript
@Module({ providers: [CounterService] }) export class AModule {}
@Module({ providers: [CounterService] }) export class BModule {}   // a SECOND instance
```

If `CounterService` holds state or a connection, A and B no longer share it. Put it in one module, export it, and import that module.

### 7. Edge Case: Importing a Provider Instead of a Module

```typescript
@Module({ imports: [UsersService] })    // wrong: imports expects MODULES
export class OrdersModule {}
```

`imports` takes modules. Providers go in `providers`, and cross-module sharing needs `exports`.

### 8. Edge Case: Circular Module Imports

```typescript
@Module({ imports: [OrdersModule] }) export class UsersModule {}
@Module({ imports: [UsersModule] })  export class OrdersModule {}
```

Fails at startup or leaves `undefined` imports. First try to redesign (extract a third module, move shared logic). `forwardRef()` is a last resort. See [Circular Dependencies](../03-core-concepts/04-modules-and-di/04-circular-dependencies.md).

---

## Syntax / API / Commands

| API / Command | Purpose |
|---|---|
| `@Module({ imports, controllers, providers, exports })` | Declare a module |
| `@Global()` | Make a module's exports globally available |
| `Module.forRoot(...)`, `forRootAsync(...)`, `forFeature(...)` | Conventional dynamic module factories |
| `nest g module <name>` | Generate a module and register it in the nearest parent |
| `nest g resource <name>` | Generate a full feature module |
| `forwardRef(() => Module)` | Break a circular import (last resort) |
| `OnModuleInit`, `OnApplicationBootstrap`, `OnModuleDestroy`... | Lifecycle hook interfaces for modules and providers |

| Metadata entry | Accepts |
|---|---|
| `imports` | Modules, dynamic modules, `forwardRef()` |
| `controllers` | Controller classes |
| `providers` | Classes, or custom provider objects (`useClass`, `useValue`, `useFactory`, `useExisting`) |
| `exports` | Provider classes or tokens, or imported modules |

---

## Important Rules

1. **Providers are private to their module until exported.** Exports are the only way to share.
2. **Import modules, not providers.** `imports` takes modules.
3. **A consumer needs both:** the provider exported by its home module, and the home module imported by the consumer (or a global module).
4. **Modules are singletons.** Importing a module from many places yields the same instances.
5. **Listing the same provider in two modules creates two instances.**
6. **Everything must be reachable from the root module.** Unreachable modules are never instantiated.
7. **Keep dependency direction clean.** Feature to shared, not shared to feature.
8. **Use `@Global()` sparingly.** It hides dependencies.
9. **Module classes cannot be injected** into providers (circular dependency).
10. **Generate the module before its controllers and services** with the CLI so registrations land in the right module.
11. **In NestJS 12, lifecycle hooks are called by component hierarchy level.** If your code relies on a specific hook order between related providers or modules, review it when upgrading.

---

## Under the Hood

### Module Tokens and the Container

Nest assigns each module a unique token (derived from the class plus its dynamic metadata, if any). The container stores per-module provider maps. A dynamic module with different `forRoot()` options produces a different token, so it is a **different** module instance. Two `TypeOrmModule.forFeature([...])` calls with different entities are distinct dynamic modules.

### How Exports Are Resolved

When resolving a constructor parameter, the injector searches:

1. The current module's providers.
2. The `exports` of every module in the current module's `imports` (and re-exported modules, recursively).
3. Providers exported by global modules.

If nothing matches, it throws `UnknownDependenciesException`, naming the class, the parameter index, and the module.

### Why Module Order in `imports` Rarely Matters

The graph is resolved after the whole scan, so declaration order is irrelevant for dependency resolution. One exception is **middleware** registered through `configure()`, where module order can influence the order of module-bound middleware. See [Middleware](../03-core-concepts/01-request-pipeline/02-middleware.md).

### Lifecycle Hook Ordering (NestJS 12)

Hooks such as `onModuleInit`, `onApplicationBootstrap`, and the shutdown hooks are now invoked by component hierarchy level. This can change relative order when multiple providers or modules depend on one another. Code that implicitly depended on the old order (for example one provider's `onModuleInit` assuming another's had already run) should be reviewed. `nest upgrade` prints notes about this. See [Application Lifecycle](../03-core-concepts/04-modules-and-di/09-application-lifecycle.md).

---

## Common Patterns

### Feature Module per Capability

The default. One folder, one module, explicit exports.

### Core/Shared Module for Infrastructure

Database, configuration, logging, cache. Imported by features, never importing features.

### Barrel for Public Surface

Expose only what other modules should import (the module and exported providers) from an `index.ts`. Keep internals unexported. Beware that barrels can cause circular file imports, so keep them small.

### Module as a Seam for Extraction

Design modules so their exports form a narrow public API. That makes later extraction into a separate service realistic. See [Modular Monolith](../08-architecture-and-patterns/01-architecture/02-modular-monolith.md).

### Dynamic Module for Configurable Libraries

`forRoot()` for global configuration, `forFeature()` for per-module registrations (as TypeORM and Mongoose do).

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Provider not exported | `Nest can't resolve dependencies of X (?)` | Providers are private to their module | Add to `exports` |
| Module not imported | Same error | Consumer cannot see another module's exports | Add the module to `imports` |
| Putting a provider in `imports` | Startup error or confusing message | `imports` expects modules | Move to `providers` |
| Same provider in two modules | Two instances, split state | Each module owns its own instance | Single home module, export and import |
| Everything in `AppModule` | Giant file, no boundaries | No feature modules | Split by feature |
| Controller not in any module | `404` | Not registered | Add to `controllers` |
| Module not reachable from root | Routes and providers missing | Never imported | Import it (directly or transitively) |
| Overusing `@Global()` | Hidden dependencies, hard to reason about | Convenience | Import explicitly |
| Circular imports | `undefined` modules, bootstrapping errors | Mutual `imports` | Redesign, extract a shared module, `forwardRef` last |
| Shared module that imports features | Cycles and tight coupling | Wrong dependency direction | Move shared pieces down, features up |
| Reusing a dynamic module's options by accident | Different configuration than expected | Each `forRoot` call creates a distinct module | Configure once in the root, or use `isGlobal` where documented |
| Relying on hook order after upgrading to v12 | Initialization order bugs | Hooks called by hierarchy level | Make hooks independent or explicit about order |

---

## Debugging

### Reading the Error

```text
Nest can't resolve dependencies of the OrdersService (?). Please make sure that the argument UsersService at index [0] is available in the OrdersModule context.
```

Read it as: **consumer** `OrdersService`, **missing** `UsersService` at **parameter 0**, looking in **`OrdersModule`**. Checklist:

```text
□ Is UsersService in UsersModule.providers?
□ Is UsersService in UsersModule.exports?
□ Is UsersModule in OrdersModule.imports?
□ Is the import a normal import (not `import type`)? Is it a class (not an interface)?
□ Is there a circular import (the (?) can mean undefined)?
```

### Techniques

- Use `debug()` in the Nest REPL to print the module graph ([Debugging](../01-getting-started/06-debugging.md)).
- Nest Devtools visualizes modules and providers in development.
- Comment out `imports` one by one (bisect) to find the failing branch.
- Log from `onModuleInit` in suspicious modules to confirm they are instantiated.

---

## Performance

- Module and provider count affects **startup time** (scanning and instantiation), not request time.
- Singleton providers are created once. Request-scoped providers are created per request and their scope bubbles up through dependants, which is the main module-related runtime cost. See [Scopes and Request Context](../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md).
- Lazy-loading modules can reduce startup cost in serverless settings. See [ModuleRef and Lazy Loading](../03-core-concepts/04-modules-and-di/08-module-ref-and-lazy-loading.md).

---

## Security

- **Encapsulation is a design aid, not a security boundary.** Anything in the same process can be reached by determined code. Do not rely on `exports` to protect secrets.
- **Global modules widen exposure.** A globally exported provider is injectable anywhere, including code you did not write. Do not make privileged providers global without need.
- **Feature modules are a good unit for applying guards** (module-wide `APP_GUARD` registrations, or per-controller `@UseGuards`). Decide intentionally.

---

## Production Considerations

- **Treat module exports as a contract.** Changing them is an API change inside your codebase.
- **Enforce boundaries** with code review and, where useful, lint rules or architecture tests that forbid deep imports into another module's internals.
- **Keep the root module small.** It should mostly list imports and global configuration.
- **Plan for hook ordering** in v12: initialization logic should not silently depend on another provider's hook having run.
- **Measure startup time** in serverless and container environments, and trim unused modules.

---

## Best Practices

### Recommended

```typescript
@Module({
  imports: [DatabaseModule],
  controllers: [UsersController],
  providers: [UsersService, UsersRepository],
  exports: [UsersService],                  // narrow public surface
})
export class UsersModule {}
```

### Avoid

```typescript
@Module({
  imports: [],
  controllers: [UsersController, OrdersController, BillingController],   // everything in one module
  providers: [UsersService, OrdersService, BillingService, DatabaseService, ConfigService],
})
export class AppModule {}

@Global() @Module({ providers: [UsersService], exports: [UsersService] })   // a business service made global
export class UsersGlobalModule {}
```

Why: feature modules give clear boundaries and a small public API. Monolithic root modules and global business services hide dependencies and invite coupling.

Additional guidance:

- Name modules after capabilities (`BillingModule`), not layers (`ServicesModule`).
- Export interfaces (service classes), not repositories or internals.
- Keep module files declarative. Put logic in providers.
- When two modules need each other, reconsider the boundary before reaching for `forwardRef`.

---

## Version / Compatibility Notes

| Item | NestJS 12 | Earlier |
|---|---|---|
| Lifecycle hook ordering | By component hierarchy level | Previous ordering |
| `@Optional()` inheritance | Not inherited by subclasses | Inherited |
| Module system | ESM-ready: relative imports in ESM projects use `.js` | CommonJS |
| `nest g module` registration | Registers in the nearest parent module | Same |
| Dynamic module conventions (`forRoot`, `forRootAsync`, `forFeature`) | Unchanged | Same |

*`@nestjs/config` v12 moves validation to Standard Schema, which affects `ConfigModule.forRoot({ validationSchema })`. See [Configuration Validation](../03-core-concepts/03-configuration/02-configuration-validation.md).*

---

## Real-World Use Cases

- **Domain modules** (`Users`, `Orders`, `Payments`) with explicit exports forming internal APIs.
- **Shared infrastructure modules** (database, Redis, mail, storage) imported by features.
- **Integration modules** wrapping third-party SDKs behind a provider.
- **Library modules** published as packages with `forRoot()` configuration.
- **Monorepo apps** composing the same shared library modules in several deployables.
- **Feature flags per module** (conditionally importing modules based on configuration).

---

## Interview Questions

### Beginner

1. What is a module in NestJS?
   - A class decorated with `@Module()` that groups controllers and providers and declares what it imports and exports.
2. What are the four properties of `@Module()`?
   - `providers`, `controllers`, `imports`, `exports`.
3. What is the root module?
   - The module passed to `NestFactory.create()`, from which Nest discovers all others.
4. How do you generate a module?
   - `nest g module <name>`.

### Intermediate

1. How do you share a provider between two modules?
   - Export it from its home module and import that module in the consumers.
2. Why are providers private by default?
   - Encapsulation: modules expose a deliberate public surface through `exports`.
3. What happens if two modules both list the same provider in `providers`?
   - Each module gets its own instance.
4. What does `@Global()` do and when should you avoid it?
   - Makes a module's exports available everywhere without importing. Avoid it for business logic because it hides dependencies.
5. What is a dynamic module?
   - A module configured at import time, usually via `forRoot()` or `forRootAsync()` returning module metadata.

### Advanced

1. Describe how Nest resolves a constructor dependency across modules.
   - Own module providers, then exports of imported modules (recursively through re-exports), then global modules' exports. Otherwise `UnknownDependenciesException`.
2. Why does each `forRoot()` call with different options produce a different module?
   - The module token includes the dynamic metadata, so the container treats it as a distinct module with its own providers.
3. How would you break a circular module dependency?
   - Redesign: extract the shared piece into a third module or invert a dependency. Use `forwardRef()` on both sides only as a last resort.
4. How do modules support a path to microservices?
   - Narrow exports form a stable internal API, so a module can be extracted behind a transport with limited change.
5. What changed about lifecycle hooks in NestJS 12 and how could it affect you?
   - Hooks are called by hierarchy level, which can change relative order across dependent providers or modules. Code relying on the old order must be reviewed.

---

## Quick Reference

```text
@Module({
  imports:     [OtherModule],          modules whose EXPORTS I need
  controllers: [XController],          routes owned by this module
  providers:   [XService],             injectables owned by this module (private by default)
  exports:     [XService, OtherModule] public surface (providers or re-exported modules)
})

Share a provider       home module exports it · consumer module imports the home module
Singletons             modules and their providers are cached and shared
Duplicate provider     listed in two modules → two instances
Global                 @Global() → exports available everywhere (use sparingly)
Dynamic                Module.forRoot(...) / forRootAsync(...) / forFeature(...)
Cycle                  redesign first, forwardRef last
CLI                    nest g module <name> → generate module before its controller/service
Error decoder          "can't resolve X at index [n] in YModule context" → X exported? YModule imports X's module?
```

---

## Key Takeaways

- A module groups controllers and providers and exposes a deliberate public surface through `exports`.
- Resolution sees only a module's own providers, the exports of its imports, and global modules' exports.
- Share by exporting and importing. Repeating a provider in two modules creates two instances.
- One module per feature keeps boundaries clear. Keep dependencies flowing from features to shared infrastructure.
- `@Global()` and `forwardRef()` are escape hatches, not defaults.
- Most "can't resolve dependencies" errors are missing exports or missing imports.
- In NestJS 12, check any initialization logic that depended on lifecycle hook order.

---

## Related Topics

```text
01 Architecture Overview
      ↓
[02 Modules]
      ↓
03 Controllers  →  04 Providers and Services  →  05 Dependency Injection
      ↓
03-core-concepts/04-modules-and-di
```

- [Fundamentals Overview](./README.md)
- [Architecture Overview](./01-architecture-overview.md)
- [Controllers](./03-controllers.md)
- [Providers and Services](./04-providers-and-services.md)
- [Dependency Injection](./05-dependency-injection.md)
- [Feature and Shared Modules](../03-core-concepts/04-modules-and-di/01-feature-and-shared-modules.md)
- [Global Modules](../03-core-concepts/04-modules-and-di/02-global-modules.md)
- [Dynamic Modules](../03-core-concepts/04-modules-and-di/03-dynamic-modules.md)
- [Circular Dependencies](../03-core-concepts/04-modules-and-di/04-circular-dependencies.md)
- [Modular Monolith](../08-architecture-and-patterns/01-architecture/02-modular-monolith.md)
