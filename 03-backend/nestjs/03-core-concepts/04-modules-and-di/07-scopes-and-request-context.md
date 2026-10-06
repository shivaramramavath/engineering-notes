# Scopes and Request Context

By default every provider in Nest is a **singleton**: created once at startup and shared by everyone. That's fast and fine for nearly all code. **Scopes** let a provider have a different lifetime, most often *one instance per incoming request*, so it can hold request-specific state.

Scopes are powerful and expensive. Most apps need them rarely, and often there's a cheaper alternative (see [AsyncLocalStorage](#alternative-asynclocalstorage)).

Prerequisites: [Custom providers](./05-custom-providers.md), [request lifecycle](../../02-fundamentals/09-request-lifecycle.md).

## The three scopes

| Scope | Instance lifetime | Shared between |
|-------|-------------------|----------------|
| `DEFAULT` | One for the whole app | Every consumer, every request |
| `REQUEST` | One per incoming request (garbage-collected after) | All consumers **within that request** |
| `TRANSIENT` | One per **consumer** that injects it | Nobody. Each injector gets its own |

```ts
import { Injectable, Scope } from '@nestjs/common';

@Injectable({ scope: Scope.REQUEST })
export class TenantContext {
  tenantId?: string;
}

@Injectable({ scope: Scope.TRANSIENT })
export class RequestLogger { /* each injector gets a fresh instance */ }
```

For a custom provider, set `scope` on the provider object:

```ts
{ provide: 'CTX', useClass: TenantContext, scope: Scope.REQUEST }
```

Controllers can be scoped too: `@Controller({ path: 'cats', scope: Scope.REQUEST })`.

## Injecting the request

Request-scoped providers can inject the current request via the `REQUEST` token:

```ts
import { REQUEST } from '@nestjs/core';
import { Request } from 'express';

@Injectable({ scope: Scope.REQUEST })
export class CurrentTenantService {
  constructor(@Inject(REQUEST) private readonly request: Request) {}

  get tenantId() {
    return this.request.headers['x-tenant-id'] as string;
  }
}
```

Injecting `REQUEST` **implicitly makes the provider request-scoped**; you don't need to declare it. In Fastify, the type is `FastifyRequest`. For non-HTTP transports (microservices, GraphQL), what `REQUEST` contains differs. For GraphQL it's the GraphQL context; check the relevant docs rather than assuming an HTTP request.

## Scope bubbles up

This is the part that surprises people. **If a provider depends on a request-scoped provider, it becomes request-scoped too**, and so does anything depending on *that*, up to the controller.

```text
TenantContext          (REQUEST)
      ▲
OrdersRepository       (becomes REQUEST: depends on TenantContext)
      ▲
OrdersService          (becomes REQUEST)
      ▲
OrdersController       (becomes REQUEST)
```

One request-scoped provider deep in the tree turns the whole chain above it into per-request instances. Nothing in the code says so; it's inferred from dependencies. Consequences:

- **Performance:** each request instantiates the whole chain, and the garbage collector has more to do. For high-throughput APIs this matters.
- **Singleton state is lost** for the affected providers (caches, in-memory counters, connection pools wrapped in them). Don't put a pool in a provider that has become request-scoped.
- **Transient providers** don't bubble the same way: each consumer gets its own instance, but the consumer stays singleton unless it's also scoped.

Keep request-scoped providers **leaf-level and few**, and don't let broad services depend on them.

## What doesn't work well with request scope

- **Lifecycle hooks** (`onModuleInit`, `onModuleDestroy`, etc.) are not triggered for request-scoped providers ([lifecycle](./09-application-lifecycle.md)).
- **Non-HTTP entry points**: cron jobs, event listeners, queue processors and WebSocket gateways have no "request". Check each integration's docs before injecting request-scoped providers there; the safe path is to avoid it.
- **Global enhancers** registered with `useGlobalGuards(new X())` and similar can't depend on scoped providers. Use `APP_GUARD` etc. (which participates in DI), accepting the bubbling cost: a request-scoped global guard makes it per-request.
- **Circular dependencies** among scoped providers are particularly painful. Avoid them ([circular dependencies](./04-circular-dependencies.md)).
- **Unit tests**: `moduleRef.get()` can't retrieve scoped providers. Use `await moduleRef.resolve(Service)` ([ModuleRef](./08-module-ref-and-lazy-loading.md)).

## Practical example: per-request tenant

```ts
@Injectable({ scope: Scope.REQUEST })
export class TenantContext {
  constructor(@Inject(REQUEST) req: Request) {
    this.tenantId = req.headers['x-tenant-id'] as string;
  }
  readonly tenantId: string;
}

@Injectable({ scope: Scope.REQUEST })
export class ProjectsService {
  constructor(private readonly tenant: TenantContext, private readonly db: Db) {}

  list() {
    return this.db.projects.find({ tenantId: this.tenant.tenantId });
  }
}
```

It reads naturally, but every provider on the path is now per-request. Multi-tenancy at scale usually deserves a deliberate design ([multi-tenancy](../../08-architecture-and-patterns/04-real-world-patterns/08-multi-tenancy.md)).

## Alternative: AsyncLocalStorage

Node's `AsyncLocalStorage` carries data across an async call chain **without** changing provider scopes, so all your services stay singletons.

```ts
// request-context.ts
import { AsyncLocalStorage } from 'node:async_hooks';

export interface RequestContext { requestId: string; tenantId?: string }
export const als = new AsyncLocalStorage<RequestContext>();

export const getContext = () => als.getStore();
```

```ts
// context.middleware.ts
@Injectable()
export class ContextMiddleware implements NestMiddleware {
  use(req: Request, _res: Response, next: NextFunction) {
    als.run(
      { requestId: randomUUID(), tenantId: req.headers['x-tenant-id'] as string },
      () => next(),
    );
  }
}
```

```ts
// any singleton service
const tenantId = getContext()?.tenantId;
```

Pros: no scope bubbling, no per-request instantiation, works in any singleton. Cons: implicit global-like state, `getStore()` returns `undefined` outside a request (jobs, startup), and context can be lost if a library breaks async context propagation. The community package `nestjs-cls` wraps this pattern with Nest integration; evaluate it for your needs.

| Need | Prefer |
|------|--------|
| Correlation/request ID, current user/tenant read by many services | `AsyncLocalStorage` |
| A provider that truly holds per-request mutable state or needs `REQUEST` deeply | `Scope.REQUEST` |
| A distinct instance per consumer (e.g., a logger with its own context name) | `Scope.TRANSIENT` |

## Durable providers (multi-tenant optimization)

For tenant-based systems where many requests share the same context (the tenant), Nest supports **durable providers** (`@Injectable({ scope: Scope.REQUEST, durable: true })`) with a custom `ContextIdStrategy`, so the per-tenant subtree is created once per tenant rather than once per request. This is an advanced feature, so read the official docs for your Nest version before using it.

## Common mistakes

- **Using `Scope.REQUEST` for convenience** (reading `req.user`) instead of a param decorator or guard-populated value ([custom decorators](../01-request-pipeline/10-custom-decorators.md)).
- **Not realizing scope bubbles**, then wondering why startup is fine but request latency rose.
- **Putting singleton-worthy state** (cache, pool, rate-limit counters) in a provider that became request-scoped.
- **Expecting `onModuleInit` to run** for request-scoped providers.
- **Using `moduleRef.get()`** to fetch scoped providers; it fails. Use `resolve()`.
- **Injecting request-scoped providers into cron/queue/event code**, where no request exists.
- **Request-scoped global guard/interceptor** without realizing every route is now per-request.

## Debugging

- Latency regression after adding a provider? Search the dependency chain for a new `REQUEST`/`@Inject(REQUEST)`; it silently propagated.
- Log in the constructor: if it prints on every request, the provider is request-scoped (possibly by inheritance).
- `Nest could not find X element (this provider does not exist in the current context)` when using `get()`: switch to `resolve()` for scoped providers.
- State leaking between requests means a provider you assumed per-request is actually singleton. Verify with a constructor log.

## Quick Summary

- Providers are singletons by default; `REQUEST` gives one instance per request, `TRANSIENT` one per consumer.
- Request scope **bubbles up** to every dependent, including controllers, with real performance cost.
- Lifecycle hooks aren't called for request-scoped providers; avoid scoped providers in non-HTTP entry points.
- Prefer `AsyncLocalStorage` (or a guard + param decorator) for request context in most apps.
- Test scoped providers with `moduleRef.resolve()`.

## Next

[ModuleRef and lazy loading →](./08-module-ref-and-lazy-loading.md)