# Feature and Shared Modules

A module is a class decorated with `@Module()` that groups related controllers and providers and controls their **visibility**. The [fundamentals note](../../02-fundamentals/02-modules.md) introduces the syntax; this one covers how to structure a real app and the rules that cause most DI errors.

## The four metadata fields

```ts
@Module({
  imports: [PaymentsModule],     // modules whose EXPORTED providers I want
  controllers: [OrdersController],
  providers: [OrdersService],    // providers instantiated and owned by this module
  exports: [OrdersService],      // subset of providers/modules visible to importers
})
export class OrdersModule {}
```

Visibility rules, which explain nearly every injection error:

1. A provider is private to its module unless listed in `exports`.
2. To inject a provider from another module, that module must be in your `imports` **and** must export it.
3. A module can re-export a module it imported, so importers get that module's exports too.

```ts
@Module({
  imports: [CommonModule],
  exports: [CommonModule],   // re-export: importers of this module also get CommonModule's exports
})
export class CoreModule {}
```

## Feature modules

A **feature module** owns one slice of the domain: its controllers, services, repositories, DTOs.

```text
src/
├── app.module.ts
├── users/
│   ├── users.module.ts
│   ├── users.controller.ts
│   ├── users.service.ts
│   ├── users.repository.ts
│   └── dto/
├── orders/
│   ├── orders.module.ts
│   └── ...
└── payments/
    └── ...
```

```ts
// users.module.ts
@Module({
  controllers: [UsersController],
  providers: [UsersService, UsersRepository],
  exports: [UsersService],        // the module's public API; the repository stays private
})
export class UsersModule {}
```

Think of `exports` as the module's **public interface**. Exporting only the service and hiding the repository means other modules can't bypass your business rules and touch persistence directly. This is the foundation of a [modular monolith](../../08-architecture-and-patterns/01-architecture/02-modular-monolith.md).

```ts
// orders.module.ts
@Module({
  imports: [UsersModule, PaymentsModule],
  providers: [OrdersService],
})
export class OrdersModule {}

// orders.service.ts: UsersService is injectable because UsersModule exports it
constructor(private readonly users: UsersService) {}
```

When to create a new module: when the code represents a cohesive domain concept with its own data and rules. When **not** to: for every file type (a `ControllersModule`, a `ServicesModule`). Group by feature, not by technical layer.

## Shared modules

A **shared module** bundles providers needed by many feature modules (a hashing service, a date utility, a mailer wrapper) and exports them.

```ts
// shared.module.ts
@Module({
  providers: [HashingService, DateService],
  exports: [HashingService, DateService],
})
export class SharedModule {}

// any feature module
@Module({ imports: [SharedModule], providers: [UsersService] })
export class UsersModule {}
```

`SharedModule` is nothing special to Nest; it's a feature module whose purpose is to be imported. Keep it small and cohesive. A `SharedModule` that exports 40 unrelated things becomes a dumping ground that couples everything to everything.

## Modules are singletons

This is the point people miss: **a module and its providers are instantiated once**, no matter how many modules import it.

```text
UsersModule ─┐
             ├──► SharedModule (one instance, one HashingService instance)
OrdersModule ┘
```

Both importers receive the **same** `HashingService` instance, so shared state (caches, counters, connection pools) is genuinely shared.

The inverse mistake creates duplicates:

```ts
// ❌ HashingService declared in two modules → two separate instances
@Module({ providers: [HashingService] }) export class AModule {}
@Module({ providers: [HashingService] }) export class BModule {}
```

If you need one instance, declare it in **one** module, export it, and import that module elsewhere. Duplicated declarations are a common source of "why is my cache empty in this service" bugs.

## Module classes can have constructors

A module is also a provider, so it can inject services (useful for init logic), though lifecycle hooks are usually cleaner ([application lifecycle](./09-application-lifecycle.md)):

```ts
export class UsersModule {
  constructor(private readonly config: ConfigService) {}
}
```

Don't inject a **provider that belongs to the same module into the module class** to wire other things together; that's a sign the logic belongs in a provider.

## Reading the classic error

```text
Nest can't resolve dependencies of the OrdersService (?). Please make sure that the argument
UsersService at index [0] is available in the OrdersModule context.
```

It names the consumer (`OrdersService`), the missing dependency (`UsersService`, index 0), and the module context (`OrdersModule`). Checklist:

1. Is `UsersService` in `providers` of the module that owns it?
2. Does that module `export` it?
3. Does `OrdersModule` `import` that module?
4. Is it a typo or a wrong import path (two classes with the same name)?
5. Is the dependency an interface or primitive? Then it needs `@Inject(TOKEN)` ([tokens](./06-injection-tokens-and-optional-dependencies.md)).
6. Are you in a test? The testing module must declare the provider or mock it ([mocking](../../04-intermediate/01-testing/03-mocking.md)).

## Practical guidelines

- **Export the minimum.** Services yes, repositories and internal helpers no.
- **Import modules, not providers.** Never list another module's service in your own `providers` to "make it work": that creates a second instance.
- **Keep the dependency graph acyclic.** If A imports B and B imports A, redesign ([circular dependencies](./04-circular-dependencies.md)).
- **`AppModule` is a composition root**: it imports feature modules and global infrastructure, and contains little else.
- **One module per bounded context** in larger apps; a module that needs 15 imports is probably doing too much.
- **Don't reach into other modules' files** (`import { UsersRepository } from '../users/users.repository'`) to bypass exports. If you need it, export it or reconsider the boundary.

## Common mistakes

- **Forgetting `exports`**, leading to the unresolved dependency error.
- **Providing the same class in several modules**, creating several instances.
- **Importing a service class directly into `providers` of another module** instead of importing its module.
- **Technical-layer modules** (`ServicesModule`) instead of feature modules.
- **A giant `SharedModule`** or a `CoreModule` that exports everything.
- **Controllers listed in several modules**, producing duplicate routes.
- **Assuming `imports` is transitive.** If A imports B and B imports C, A does **not** see C's exports unless B re-exports C.

## Debugging

- Start from the error: consumer, missing token, module context. Check the four visibility rules in order.
- Log whether two places receive the same instance: add `console.log(Math.random())` in a provider's constructor and see how many times it prints.
- A visual graph helps for large apps. Nest Devtools (`@nestjs/devtools-integration`) can display the module/provider graph; check its docs for your version.

## Quick Summary

- Modules group controllers and providers and control visibility via `imports`/`exports`.
- Inject across modules only when the owner **exports** it and you **import** that module.
- Organize by feature; shared modules are just small, cohesive, imported modules.
- Modules and their providers are singletons; declare a provider in exactly one module.
- Treat `exports` as the module's public API and keep internals private.

## Next

[Global modules →](./02-global-modules.md)
