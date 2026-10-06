# ModuleRef and Lazy Loading

Constructor injection is static: the dependencies are declared up front and resolved at startup. Sometimes you need to resolve a provider **at runtime**, chosen by a key, created on demand, or fetched from a request-scoped context. `ModuleRef` is Nest's programmatic handle on the DI container for that purpose. `LazyModuleLoader` goes one step further and loads whole modules on demand.

Both are escape hatches. Reach for them only when constructor injection genuinely can't express what you need.

Prerequisites: [Custom providers](./05-custom-providers.md), [scopes](./07-scopes-and-request-context.md).

## `ModuleRef`

Inject it like any other provider:

```ts
import { ModuleRef } from '@nestjs/core';

@Injectable()
export class HandlerRegistry {
  constructor(private readonly moduleRef: ModuleRef) {}
}
```

### `get()`: retrieve an existing singleton

```ts
const users = this.moduleRef.get(UsersService);                         // current module only
const anyUsers = this.moduleRef.get(UsersService, { strict: false });   // search the whole app
```

- By default `get` is **strict**: it looks only in the module that injected `ModuleRef`. Use `{ strict: false }` to search globally.
- `get()` works for **static (singleton) providers only**. For request-scoped or transient providers it throws; use `resolve()`.
- It returns a synchronous result and throws if the provider isn't found.

### `resolve()`: scoped and transient providers

```ts
const logger = await this.moduleRef.resolve(TransientLogger);         // a NEW instance each call
const logger2 = await this.moduleRef.resolve(TransientLogger);
logger === logger2;   // false
```

`resolve()` is async and, for transient providers, returns a fresh instance per call. To share one instance across several resolutions, pass the same **context id**:

```ts
import { ContextIdFactory } from '@nestjs/core';

const contextId = ContextIdFactory.create();
const a = await this.moduleRef.resolve(RequestScopedService, contextId);
const b = await this.moduleRef.resolve(RequestScopedService, contextId);
a === b;   // true: same context
```

### Resolving request-scoped providers from a singleton

Inside a request, reuse the request's own context id so you get the same instances the rest of the request sees:

```ts
@Injectable()
export class CatsService {
  constructor(
    private readonly moduleRef: ModuleRef,
    @Inject(REQUEST) private readonly request: unknown,   // makes CatsService request-scoped
  ) {}

  async run() {
    const contextId = ContextIdFactory.getByRequest(this.request);
    const svc = await this.moduleRef.resolve(TenantService, contextId);
  }
}
```

Outside HTTP, or when you're creating your own context, register the "request" for that id with `registerRequestByContextId(...)` so `REQUEST` injection works for it. See the official docs for the exact flow in your version.

### `create()`: instantiate a class on demand

```ts
const instance = await this.moduleRef.create(SomeClass);
```

Instantiates a class with the module's DI context (its constructor dependencies get injected) **without registering it** as a provider. Useful for plug-in style code where classes are discovered dynamically.

## Practical use cases

### Strategy lookup by key

```ts
@Injectable()
export class PaymentService {
  constructor(private readonly moduleRef: ModuleRef) {}

  pay(provider: 'stripe' | 'paypal', amount: number) {
    const gateway = this.moduleRef.get<PaymentGateway>(`${provider}Gateway`, { strict: false });
    return gateway.charge(amount);
  }
}
```

Works, but it's a **service locator**: the dependency is invisible in the constructor and string-keyed. A usually better version injects a map built by a factory:

```ts
{
  provide: GATEWAYS,
  useFactory: (stripe: StripeGateway, paypal: PaypalGateway) => ({ stripe, paypal }),
  inject: [StripeGateway, PaypalGateway],
}
// then: constructor(@Inject(GATEWAYS) private gateways: Record<string, PaymentGateway>) {}
```

See [custom providers](./05-custom-providers.md) and the [strategy pattern](../../08-architecture-and-patterns/02-design-patterns/02-strategy.md).

### Breaking a dependency cycle at call time

Fetch the other service inside a method instead of injecting it in the constructor ([circular dependencies](./04-circular-dependencies.md)). It works, but it hides the cycle rather than removing it.

### Discovering providers by metadata

If you need to find all providers carrying a decorator (plug-in registries, event handlers), `ModuleRef` isn't the right tool. Nest's `DiscoveryService` (from `@nestjs/core`) is designed for scanning providers and metadata; check its documentation for current usage.

## Drawbacks to keep in mind

- **Hidden dependencies**: reading the constructor no longer tells you what the class uses.
- **Runtime failures** instead of startup failures: a wrong token throws when the code path runs, not at boot.
- **Harder testing**: you must stub `ModuleRef` or register real providers.
- **String tokens** invite typos; prefer symbols or class tokens.

Good rule: if you can list the dependencies at construction time, do. Use `ModuleRef` for dynamic selection, scoped resolution, and on-demand instantiation.

## Lazy-loading modules

Normally the entire module graph is built at startup. For environments where **cold start time** dominates (serverless functions, CLIs), you can defer loading rarely-used modules with `LazyModuleLoader`.

```ts
import { LazyModuleLoader } from '@nestjs/core';

@Injectable()
export class ReportsFacade {
  constructor(private readonly lazyModuleLoader: LazyModuleLoader) {}

  async generate() {
    const { ReportsModule } = await import('./reports/reports.module');
    const moduleRef = await this.lazyModuleLoader.load(() => ReportsModule);

    const { ReportsService } = await import('./reports/reports.service');
    const service = moduleRef.get(ReportsService);
    return service.generate();
  }
}
```

How it works:

- `import()` defers loading the JavaScript; `load()` instantiates the module and its providers on first call and **caches** it, so later calls reuse the same module.
- You get a `ModuleRef` for the lazy module and use `get()` to retrieve its providers.

Limitations (important):

- **Controllers, GraphQL resolvers, and WebSocket gateways can't be lazy-loaded.** Routes and handlers are registered at bootstrap, so a lazily loaded module can't add them afterward.
- **Lifecycle hooks aren't invoked** for lazily loaded modules and providers. Do initialization explicitly after loading.
- It only helps if the module's dependencies are not already pulled in elsewhere. Importing a lazy module's file at the top of another file defeats the purpose.
- Benefit depends on the app. Measure cold start before and after; for a long-running server it's rarely worth the complexity.

## Common mistakes

- **Using `get()` for scoped/transient providers**; use `resolve()`.
- **Forgetting that `get()` is strict by default** and failing to find providers in other modules.
- **Resolving without a shared context id** and getting a different instance each time.
- **Replacing normal injection with `ModuleRef.get('someString')` everywhere** (service locator sprawl).
- **Lazy-loading modules that contain controllers** and expecting routes to appear.
- **Statically importing a "lazy" module** elsewhere, so it's loaded at startup anyway.
- **Expecting lifecycle hooks** in lazy modules.

## Debugging

- `Nest could not find X element (this provider does not exist in the current context)`: wrong module context (try `{ strict: false }`), the provider isn't registered, or it's scoped (use `resolve`).
- Scoped service has the wrong request data: you resolved with a new context id instead of `ContextIdFactory.getByRequest(request)`.
- Lazy module has no effect on startup time: check for a static import of it, and measure the actual cold-start path.

## Quick Summary

- `ModuleRef` gives runtime access to the DI container: `get()` for singletons, `resolve()` for scoped/transient, `create()` for unregistered classes.
- `get()` is strict (current module) by default; pass `{ strict: false }` to search globally.
- Share scoped instances via a common `contextId` (`ContextIdFactory`).
- It's a service-locator escape hatch; prefer constructor injection and factory-built maps.
- `LazyModuleLoader` defers loading modules for faster cold starts, but not controllers/resolvers/gateways, and lifecycle hooks don't run.

## Next

[Application lifecycle →](./09-application-lifecycle.md)