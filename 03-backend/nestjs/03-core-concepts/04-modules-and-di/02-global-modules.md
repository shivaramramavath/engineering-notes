# Global Modules

By default, a module's exports are only visible to modules that **import** it. A **global module** is visible everywhere without being imported by each consumer. It's a convenience for infrastructure that nearly every module needs, and a trap when used for anything else.

Prerequisite: [Feature and shared modules](./01-feature-and-shared-modules.md).

## Making a module global

```ts
import { Global, Module } from '@nestjs/common';

@Global()
@Module({
  providers: [LoggerService],
  exports: [LoggerService],     // still required
})
export class LoggerModule {}
```

```ts
// app.module.ts: register it ONCE, usually in the root module
@Module({ imports: [LoggerModule, UsersModule] })
export class AppModule {}
```

```ts
// users.service.ts: no LoggerModule import in UsersModule
constructor(private readonly logger: LoggerService) {}
```

Two details people get wrong:

- `@Global()` does **not** remove the need for `exports`. Only exported providers become globally visible.
- The module still has to be **imported once somewhere** so Nest instantiates it. `@Global()` changes visibility, not discovery.

## Global via dynamic modules

Many official packages expose an option instead of a decorator:

```ts
ConfigModule.forRoot({ isGlobal: true });
CacheModule.register({ isGlobal: true });
```

For your own dynamic modules you set `global: true` in the returned `DynamicModule`, or expose an `isGlobal` option ([dynamic modules](./03-dynamic-modules.md)).

## When global is reasonable

Cross-cutting infrastructure that has no domain meaning and is used almost everywhere:

| Good candidates | Why |
|-----------------|-----|
| Configuration (`ConfigModule`) | Needed by nearly every module |
| Logger | Same |
| Database connection / ORM root module | One connection for the whole app |
| Event bus, clock, request-context helpers | Stateless or infrastructure-level |

## When global is a mistake

- **Domain services** (`UsersService`, `OrdersService`). Making them global hides who depends on whom and erodes module boundaries, defeating the purpose of modules.
- **"I'm tired of importing it."** Explicit imports are documentation: they make dependencies visible and the graph analyzable.
- **A global `SharedModule` that exports everything.** It couples the whole app to a grab-bag.

Costs of global modules:

- **Hidden dependencies.** A service can use something its module never mentions, so reading the module tells you less.
- **Harder testing.** Test modules must import or mock the global provider explicitly, because nothing declares the need.
- **Weaker boundaries.** Extracting a module into a library or microservice is harder when it silently relies on globals.
- **Ordering/override surprises.** Two global modules exporting the same token are ambiguous; avoid it.

A decent rule: allow a **handful** of global infrastructure modules, and import everything else explicitly.

## Testing with global modules

```ts
const moduleRef = await Test.createTestingModule({
  imports: [UsersModule],
  providers: [{ provide: LoggerService, useValue: { log: jest.fn() } }], // supply what the global would have
}).compile();
```

If you import the real global module, its own dependencies come with it. Mocking at the provider level is usually lighter. See [mocking](../../04-intermediate/01-testing/03-mocking.md).

## Global *enhancers* are a different thing

`APP_GUARD`, `APP_PIPE`, `APP_INTERCEPTOR`, and `APP_FILTER` register **global request-pipeline components**, not global module visibility. Don't confuse them ([pipeline overview](../01-request-pipeline/01-pipeline-overview-and-execution-order.md)).

## Common mistakes

- **Using `@Global()` and forgetting `exports`**: still "can't resolve dependencies".
- **Never importing the global module anywhere**, so it's never instantiated.
- **Making domain modules global** to avoid import boilerplate.
- **Importing a global module in many places "just in case".** Harmless but noisy; one import is enough.
- **Assuming global means "sees everything".** It only makes **its exports** visible to others; it doesn't give *it* access to other modules.
- **Two global modules providing the same token**, with unclear winners.

## Debugging

- `Can't resolve dependencies of X (?)` with a global module: check that it's imported in `AppModule`, that the provider is in `exports`, and that you didn't accidentally declare the same provider again locally.
- Provider is `undefined` in tests: the test module didn't include the global module or a mock for it.
- Wondering what depends on a global? Search for the injected class name; there's no import to follow, which is exactly the cost of global.

## Quick Summary

- `@Global()` (or `global: true` / `isGlobal`) makes a module's **exports** available everywhere without per-module imports.
- You still need `exports` and one import to register the module.
- Use it for infrastructure (config, logging, DB root), not domain services.
- Global modules hide dependencies and complicate testing, so keep them few.
- Global request-pipeline enhancers (`APP_*`) are unrelated.

## Next

[Dynamic modules →](./03-dynamic-modules.md)
