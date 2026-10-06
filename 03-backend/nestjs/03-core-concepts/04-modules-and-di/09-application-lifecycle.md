# Application Lifecycle

A Nest app goes through three phases: **initializing** (building the module graph, creating providers), **running** (serving traffic), and **terminating** (shutting down). Lifecycle hooks are methods you add to providers, controllers, or modules so Nest calls them at specific moments in that sequence.

They're how you connect to external systems at startup, warm caches, and, equally important, release resources cleanly on shutdown.

Prerequisites: [Feature and shared modules](./01-feature-and-shared-modules.md), [custom providers](./05-custom-providers.md).

## The hooks

| Phase | Hook | Interface | Called when |
|-------|------|-----------|-------------|
| Init | `onModuleInit()` | `OnModuleInit` | The host module's dependencies have been resolved |
| Init | `onApplicationBootstrap()` | `OnApplicationBootstrap` | **All** modules have initialized, before the app starts listening |
| Shutdown | `onModuleDestroy()` | `OnModuleDestroy` | After a termination signal / `app.close()` is received |
| Shutdown | `beforeApplicationShutdown(signal?)` | `BeforeApplicationShutdown` | After all `onModuleDestroy` handlers complete |
| Shutdown | `onApplicationShutdown(signal?)` | `OnApplicationShutdown` | After connections are closed (`app.close()` resolves) |

```text
 ┌─────────── initializing ───────────┐    ┌───── running ─────┐   ┌──────────── terminating ─────────────┐
 resolve providers → onModuleInit →     →   app.listen()        →   onModuleDestroy → beforeApplicationShutdown
 (per module, dependency order)           onApplicationBootstrap       → (close connections) → onApplicationShutdown
```

## Basic example

```ts
import { Injectable, OnModuleInit, OnModuleDestroy, Logger } from '@nestjs/common';

@Injectable()
export class RedisService implements OnModuleInit, OnModuleDestroy {
  private readonly logger = new Logger(RedisService.name);
  private client: Redis;

  async onModuleInit() {
    this.client = new Redis(process.env.REDIS_URL);
    await this.client.ping();
    this.logger.log('Redis connected');
  }

  async onModuleDestroy() {
    await this.client.quit();
    this.logger.log('Redis disconnected');
  }
}
```

Hooks can be `async`; Nest **awaits** them before moving on. If an init hook throws or rejects, **bootstrap fails**, which is what you want for a mandatory dependency.

Hooks can live on providers, controllers, and the module class itself.

## `onModuleInit` vs `onApplicationBootstrap`

- `onModuleInit`: run setup for **this** provider. Its own dependencies exist, but other modules may not have finished initializing.
- `onApplicationBootstrap`: run logic that needs the **whole app ready**: warm a cache using other services, register discovered handlers, kick off schedulers.

```ts
async onApplicationBootstrap() {
  await this.cache.warm(await this.products.findPopular());
}
```

Order within init: Nest initializes modules in dependency order and awaits each hook **sequentially**. Don't rely on ordering between unrelated modules or providers; if A needs B ready, make A depend on B (inject it) so Nest orders them.

## Shutdown hooks need to be enabled

Shutdown hooks run when `app.close()` is called, **or** when the process receives a termination signal **and** you opt in:

```ts
// main.ts
const app = await NestFactory.create(AppModule);
app.enableShutdownHooks();     // listen for SIGTERM, SIGINT, etc.
await app.listen(3000);
```

Without `enableShutdownHooks()`, `SIGTERM` (what Kubernetes and Docker send) terminates the process **without** running `onModuleDestroy`, `beforeApplicationShutdown`, or `onApplicationShutdown`. This is the most common lifecycle bug in production.

The `signal` argument tells you which signal triggered shutdown:

```ts
async onApplicationShutdown(signal?: string) {
  this.logger.log(`Shutting down (${signal ?? 'app.close()'})`);
}
```

Notes:

- `enableShutdownHooks()` registers process signal listeners, which costs a little memory. In tests that create many app instances, you can hit `MaxListenersExceededWarning`, so avoid enabling it in tests (or close each app).
- **NestJS 11 changed the order of termination hooks** (they now run in the reverse order of initialization). Confirm details in the migration guide if your shutdown logic depends on ordering.
- Shutdown hooks run after the HTTP server stops accepting new connections, but long-lived keep-alive connections and in-flight work need your own handling. See [graceful shutdown](../../07-production/04-deployment/02-graceful-shutdown-and-process-management.md).

## What to do in each hook

| Task | Hook |
|------|------|
| Open DB/Redis/broker connection | `onModuleInit` |
| Validate that a required external service is reachable | `onModuleInit` (throw to abort boot) |
| Warm caches, schedule jobs, register discovered handlers | `onApplicationBootstrap` |
| Stop accepting work (pause queue consumers), flag readiness `false` | `onModuleDestroy` or `beforeApplicationShutdown` |
| Flush buffers, finish in-flight jobs | `beforeApplicationShutdown` |
| Close connections, final logging | `onApplicationShutdown` (or `onModuleDestroy` for owned resources) |

A sensible rule: **whoever opens a resource closes it**, ideally in the same class.

## Lifecycle in tests

```ts
const app = moduleRef.createNestApplication();
await app.init();    // triggers onModuleInit and onApplicationBootstrap
// ...
await app.close();   // triggers shutdown hooks
```

Forgetting `app.close()` leaves connections open and Jest hanging. See [e2e testing](../../04-intermediate/01-testing/06-e2e-testing.md). `Test.createTestingModule(...).compile()` alone does **not** run init hooks until you call `init()` on the module or app.

## Important behavior and limits

- **Request-scoped providers don't get lifecycle hooks** ([scopes](./07-scopes-and-request-context.md)). The same is true for lazily loaded modules ([lazy loading](./08-module-ref-and-lazy-loading.md)).
- Hooks are documented for classes Nest instantiates (providers, controllers, modules). Don't rely on them for plain objects registered with `useValue` or objects returned from a `useFactory`; do setup explicitly, or wrap the object in a class.
- **Timing of `app.listen()`:** init hooks complete before the server accepts connections, so slow `onModuleInit` work delays readiness. Keep it fast or run non-essential warmup in the background.
- **Hybrid apps** (HTTP + microservice) share the same lifecycle; hooks run once for the application.
- **Errors in shutdown hooks** don't stop other hooks from being attempted, but they can leave resources half-closed. Catch and log inside your hooks.
- Don't do **request-handling work** in hooks, and don't block shutdown forever (add timeouts); orchestrators will `SIGKILL` after their grace period.

## Common mistakes

- **No `enableShutdownHooks()`**, so cleanup never runs on `SIGTERM`.
- **Doing connection setup in the constructor** instead of `onModuleInit`. Constructors can't be async, and failures there are less clear.
- **Assuming cross-module init order.** Use DI to express ordering.
- **Swallowing errors in `onModuleInit`** for mandatory dependencies. Let it throw so the app doesn't start half-broken.
- **Long-running work in `onModuleInit`**, delaying startup (and health checks).
- **Not closing resources** you opened, causing hanging test runs and slow shutdowns.
- **Relying on hooks for request-scoped providers.**
- **Enabling shutdown hooks in every test app**, triggering listener warnings.

## Debugging

- Cleanup logs missing in production? Check `enableShutdownHooks()` and that your platform sends `SIGTERM` (not just `SIGKILL`) with enough grace time.
- Hook never runs? Check the class is actually registered as a provider (and not request-scoped, lazy-loaded, or a `useValue` object).
- Order surprises? Add `Logger` lines at the top of each hook and compare with the table above.
- App hangs on startup? An `async` init hook is waiting on something that never resolves (network). Add timeouts.
- Jest "did not exit"? Missing `app.close()` / open handles from services you started in hooks.

## Quick Summary

- Init: `onModuleInit` (per module) → `onApplicationBootstrap` (all modules ready). Shutdown: `onModuleDestroy` → `beforeApplicationShutdown` → `onApplicationShutdown`.
- Hooks can be async and are awaited; a failing init hook aborts startup.
- Call `app.enableShutdownHooks()` or signals like `SIGTERM` will skip cleanup.
- Whoever opens a resource closes it; use DI, not hook ordering assumptions, to sequence dependencies.
- Not available for request-scoped or lazily loaded providers; NestJS 11 reversed termination hook order.

## Next

Section complete. Continue with [Testing](../../04-intermediate/01-testing/README.md), or review the [request pipeline](../01-request-pipeline/README.md) next.

← Back to [Modules and DI overview](./README.md)