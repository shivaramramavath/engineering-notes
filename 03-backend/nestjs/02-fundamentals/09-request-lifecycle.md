# Request Lifecycle

Every HTTP request that reaches a NestJS application travels through the same sequence of components: **middleware**, **guards**, **interceptors**, **pipes**, your **handler**, interceptors again on the way out, and **exception filters** if anything throws. Knowing the order, and where each component can stop the request, explains why authentication happens before validation, why a guard cannot see a transformed body, why a filter can catch a pipe's error, and why some behaviors are impossible in the wrong place. This file is the high-level journey. The detailed treatment of each component, with decision tables, is in [Pipeline Overview and Execution Order](../03-core-concepts/01-request-pipeline/01-pipeline-overview-and-execution-order.md).

---

## Overview

**What it is.** The fixed processing pipeline Nest applies around every controller call. You choose which components to attach and at which level (global, controller, route). Nest decides the order between component types.

**Why it exists.** Cross-cutting concerns (authentication, validation, logging, error formatting) must apply uniformly without being copied into every handler. A fixed pipeline makes their interaction predictable.

**Where it is used.** Every request in every HTTP application. A similar pipeline (without middleware, and with pre-request hooks in microservices) applies to WebSocket gateways, GraphQL resolvers, and message handlers.

**Why you should understand it.** Most "why did X run before Y?" and "why can't my guard see Z?" questions are answered by the order. It also tells you where to put each kind of logic.

---

## Mental Model

A request passes inward through layers, reaches your handler, and the response passes outward through the interceptors. Exceptions jump to the filters.

```text
 Client
   │  HTTP request
   ▼
 ┌──────────────────────────────────────────────────────────────────────────────┐
 │ Adapter: parse body/query, match route, global prefix/versioning             │
 │ ┌──────────────────────────────────────────────────────────────────────────┐ │
 │ │ 1. MIDDLEWARE        (global, then module-bound)                         │ │
 │ │ ┌──────────────────────────────────────────────────────────────────────┐ │ │
 │ │ │ 2. GUARDS         (global → controller → route)    allow / deny      │ │ │
 │ │ │ ┌──────────────────────────────────────────────────────────────────┐ │ │ │
 │ │ │ │ 3. INTERCEPTORS: before   (global → controller → route)          │ │ │ │
 │ │ │ │ ┌──────────────────────────────────────────────────────────────┐ │ │ │ │
 │ │ │ │ │ 4. PIPES  (global → controller → route → parameter)          │ │ │ │ │
 │ │ │ │ │ ┌──────────────────────────────────────────────────────────┐ │ │ │ │ │
 │ │ │ │ │ │ 5. HANDLER  controller method → service → repository     │ │ │ │ │ │
 │ │ │ │ │ └──────────────────────────────────────────────────────────┘ │ │ │ │ │
 │ │ │ │ └──────────────────────────────────────────────────────────────┘ │ │ │ │
 │ │ │ │ 6. INTERCEPTORS: after   (route → controller → global)  REVERSED │ │ │ │
 │ │ │ └──────────────────────────────────────────────────────────────────┘ │ │ │
 │ │ └──────────────────────────────────────────────────────────────────────┘ │ │
 │ └──────────────────────────────────────────────────────────────────────────┘ │
 │   any exception thrown anywhere inside ──► 7. EXCEPTION FILTERS              │
 │                                              (route → controller → global)   │
 └──────────────────────────────────────────────────────────────────────────────┘
   │  HTTP response
   ▼
 Client
```

Two directions to remember: **inbound** is global to specific (except filters), **outbound** through interceptors is the reverse.

---

## Core Concepts

### The Components, in Order

| # | Component | Job | Can short-circuit? | Has access to |
|---|---|---|---|---|
| 1 | **Middleware** | Preprocess the raw request: logging, request IDs, body/cookie parsing, CORS | Yes (respond or error) | `req`, `res`, `next` (platform objects) |
| 2 | **Guard** | Decide whether the request may proceed: authentication, authorization | Yes (return `false` or throw) | `ExecutionContext` (knows the handler and class, so can read metadata) |
| 3 | **Interceptor (before)** | Wrap the handler: logging, timing, caching, set up context | Yes (return a different observable, for example a cache hit) | `ExecutionContext`, `CallHandler` |
| 4 | **Pipe** | Transform and validate each handler argument | Yes (throw) | The argument value and its metadata |
| 5 | **Handler** | Your controller method calling services | n/a | Everything you inject |
| 6 | **Interceptor (after)** | Map the result, add headers, measure, cache, handle errors from the stream | n/a (transforms result or errors) | The result stream |
| 7 | **Exception filter** | Convert thrown exceptions into responses | n/a | The exception and `ArgumentsHost` |

### Which Components See What

| Component | Knows which handler will run? | Sees the validated DTO? |
|---|---|---|
| Middleware | **No** (runs before the execution context exists) | No |
| Guard | **Yes** (via `ExecutionContext`) | No (pipes have not run) |
| Interceptor (before) | Yes | No |
| Pipe | Yes (argument metadata) | It *creates* it |
| Handler | n/a | Yes |

That is why **authorization happens before validation**: unauthenticated clients are rejected cheaply, and a guard cannot rely on a transformed DTO. If a guard needs the body, it reads the raw `req.body`, and treats it as unvalidated.

### Binding Levels

Guards, interceptors, pipes, and filters can be attached at several levels:

| Level | How |
|---|---|
| **Global** | `app.useGlobalGuards(...)` / `useGlobalPipes` / `useGlobalInterceptors` / `useGlobalFilters`, **or** (preferred when DI is needed) a provider using `APP_GUARD`, `APP_PIPE`, `APP_INTERCEPTOR`, `APP_FILTER` in any module |
| **Controller** | `@UseGuards()`, `@UsePipes()`, `@UseInterceptors()`, `@UseFilters()` on the class |
| **Route (handler)** | The same decorators on a method |
| **Parameter** | Pipes only: `@Param('id', ParseIntPipe)` |

`app.useGlobal...()` registers outside any module, so those instances **cannot inject providers**. Registering with the `APP_*` token inside a module gives the component dependency injection:

```typescript
@Module({
  providers: [{ provide: APP_GUARD, useClass: AuthGuard }],   // global AND injectable
})
export class AppModule {}
```

### Execution Order Across Levels

| Component | Order across levels |
|---|---|
| Middleware | Global (`app.use()`), then module-bound (`configure()`), in the order determined by module imports and registration |
| Guards | Global → controller → route |
| Interceptors (before) | Global → controller → route |
| Pipes | Global → controller → route → **parameter** (and left to right within a decorator) |
| Interceptors (after) | **Route → controller → global** (reverse) |
| Exception filters | **Route → controller → global.** Filters are the one component that does *not* resolve global-first. The lowest matching level handles the exception first |

Within one level, components run in the order listed (`@UseGuards(A, B)` runs `A` then `B`).

### Short-Circuiting

| Where | How a request stops there | Result |
|---|---|---|
| Middleware | Respond without `next()`, or `next(err)` | Response sent, or error to filters (global only, see below) |
| Guard | `return false` or `throw` | `403 Forbidden` for `false` (or your exception), error to filters |
| Interceptor | Return an observable that does not call `next.handle()` | e.g. cached response |
| Pipe | `throw` (usually `BadRequestException`) | `400` via filters. Handler never runs |
| Handler | `throw` | Filters |

Once a stage stops the request, **later stages do not run**. A guard rejecting a request means no pipes, no handler, and no interceptor "after" logic.

### Errors and Filters

Exceptions thrown from guards, interceptors, pipes, and the handler are caught by the exceptions zone and routed to filters (route, then controller, then global). The default filter maps `HttpException` to its status and body, and anything else to a generic `500`.

Exceptions thrown in **middleware** are handled by global filters. Route- and controller-level filters are not yet in context at that point. *Verify the exact behavior for your version if you depend on it.*

Interceptors see errors from the handler as an error on the observable (use `catchError` to handle or map them). They run "after" logic on success via `tap`/`map`, which does not run on the error path.

### Before the Pipeline: The Adapter

Before Nest's components run, the **adapter** (Express or Fastify) has already:

- received the connection and parsed the HTTP message,
- run its own parsers (JSON/URL-encoded body, query string, cookies if configured),
- matched the route (including global prefix and versioning),
- applied middleware registered at the platform level.

A request that matches no route produces a `404` through Nest's exception handling.

### Request Scope and the Lifecycle

If any provider in the chain is **request-scoped**, Nest builds a fresh dependency subtree for each request (controller, services, and request-scoped guards/interceptors/pipes as well). Singletons are shared. See [Scopes and Request Context](../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md).

### Not Every Transport Has Every Component

| Transport | Middleware | Guards, interceptors, pipes, filters |
|---|---|---|
| HTTP | Yes | Yes |
| GraphQL | Platform middleware, plus field middleware | Yes (adapted context) |
| WebSockets | No (adapter-level) | Yes |
| Microservices | No | Yes (+ pre-request hooks) |

`ExecutionContext` abstracts the transport so guards and interceptors can serve all of them. See [Execution Context](../03-core-concepts/01-request-pipeline/09-execution-context.md).

---

## How It Works

For one request, with components at all levels:

```text
 1  adapter parses request and matches route:  GET /demo/42
 2  middleware:           global middleware → module-bound middleware
 3  guards:               global guards → controller guards → route guards
                              any returns false / throws → STOP (filters)
 4  interceptors (before) global → controller → route      (each calls next.handle() to continue)
 5  pipes:                for each argument: global → controller → route → parameter pipes
                              any throws → STOP (filters)
 6  handler:              controller.method(args) → services → ...
 7  interceptors (after): route → controller → global      (result flows back out)
 8  response controller:  status, headers, serialization → adapter writes the response

 any throw in 3–6  →  exception filters: route → controller → global → response
```

Mechanically, Nest wraps your handler in a "route handler" created by the router explorer. For each request it creates an `ExecutionContext`, runs the guards consumer, then the interceptors consumer (which wraps a function that runs pipes and then your handler), then the response controller. The interceptor nesting is why "after" logic runs in reverse order: each interceptor's `next.handle()` observable is wrapped by the next.

---

## Basic Example

Make the order visible. This example attaches one component of each kind at the route or controller level, and logs from each.

```typescript
// demo.ts
import {
  ArgumentsHost, CallHandler, CanActivate, Catch, Controller, ExceptionFilter, ExecutionContext,
  Get, HttpException, Injectable, MiddlewareConsumer, Module, NestInterceptor, NestModule,
  Param, PipeTransform, UseFilters, UseGuards, UseInterceptors,
} from '@nestjs/common';
import type { NextFunction, Request, Response } from 'express';
import { Observable, tap } from 'rxjs';

const log = (s: string) => console.log(s);

function loggerMiddleware(_req: Request, _res: Response, next: NextFunction) {
  log('1 middleware');
  next();
}

@Injectable()
class DemoGuard implements CanActivate {
  canActivate(_ctx: ExecutionContext) {
    log('2 guard');
    return true;
  }
}

@Injectable()
class DemoInterceptor implements NestInterceptor {
  intercept(_ctx: ExecutionContext, next: CallHandler): Observable<unknown> {
    log('3 interceptor (before)');
    return next.handle().pipe(tap(() => log('6 interceptor (after)')));
  }
}

@Injectable()
class DemoPipe implements PipeTransform {
  transform(value: unknown) {
    log('4 pipe');
    return value;
  }
}

@Catch(HttpException)
class DemoFilter implements ExceptionFilter {
  catch(exception: HttpException, host: ArgumentsHost) {
    log('7 filter');
    host.switchToHttp().getResponse<Response>().status(exception.getStatus()).json({ handled: true });
  }
}

@Controller('demo')
@UseGuards(DemoGuard)
@UseInterceptors(DemoInterceptor)
@UseFilters(DemoFilter)
class DemoController {
  @Get(':id')
  find(@Param('id', DemoPipe) id: string) {
    log('5 handler');
    if (id === 'boom') throw new HttpException('boom', 400);
    return { id };
  }
}

@Module({ controllers: [DemoController] })
export class DemoModule implements NestModule {
  configure(consumer: MiddlewareConsumer) {
    consumer.apply(loggerMiddleware).forRoutes('demo');
  }
}
```

```bash
curl localhost:3000/demo/42
```

```text
1 middleware
2 guard
3 interceptor (before)
4 pipe
5 handler
6 interceptor (after)
```

```bash
curl localhost:3000/demo/boom
```

```text
1 middleware
2 guard
3 interceptor (before)
4 pipe
5 handler
7 filter
```

What this shows:

1. The sequence is middleware, guard, interceptor (before), pipe, handler, interceptor (after).
2. On the error path the "after" `tap` does **not** run (the observable errored). Control jumps to the filter.
3. Pipes run *after* the interceptor's "before" code, and guards run before both.
4. If the guard returned `false`, you would see only `1` and `2`.

---

## Practical Examples

### 1. Basic: Where to Put Common Concerns

| Concern | Component | Why |
|---|---|---|
| Request ID, access logging, `cookie-parser`, `helmet` | Middleware | Needs raw request/response, no handler knowledge |
| Authentication, roles, permissions | Guard | Needs to know the handler and its metadata, must run before validation |
| Timing, caching, response envelope, timeout | Interceptor | Wraps handler execution, can transform the result |
| Param conversion, DTO validation | Pipe | Operates on arguments just before the handler |
| Error to response mapping | Exception filter | Catches everything thrown |

### 2. Common: Multiple Levels, Observed Order

Suppose guards `G` (global), `C` (controller), `R` (route), and interceptors `Ig`, `Ic`, `Ir`, and pipes `Pg`, `Pc`, `Pr`, `Pp` (parameter) are all bound:

```text
inbound   G → C → R           (guards)
          Ig → Ic → Ir        (interceptors, before)
          Pg → Pc → Pr → Pp   (pipes)
handler
outbound  Ir → Ic → Ig        (interceptors, after: reversed)
errors    route filter → controller filter → global filter
```

### 3. Common: Secure by Default With a Global Guard

```typescript
@Module({
  providers: [
    { provide: APP_GUARD, useClass: AuthGuard },     // applies to every route, can inject services
    { provide: APP_GUARD, useClass: RolesGuard },    // runs after AuthGuard (registration order)
  ],
})
export class AppModule {}
```

Combine with a `@Public()` metadata decorator that the guard reads, to opt routes out. See [Decorators](./06-decorators.md).

### 4. Common: Global Validation Pipe

```typescript
app.useGlobalPipes(new ValidationPipe({ whitelist: true, transform: true }));
// or, to allow DI inside the pipe:
{ provide: APP_PIPE, useValue: new ValidationPipe({ whitelist: true, transform: true }) }
```

### 5. Real-World: A Timing Interceptor Across the Whole Pipeline

```typescript
@Injectable()
export class TimingInterceptor implements NestInterceptor {
  intercept(_ctx: ExecutionContext, next: CallHandler) {
    const start = Date.now();
    return next.handle().pipe(tap(() => console.log(`handler+pipes: ${Date.now() - start}ms`)));
  }
}
```

It measures pipes plus handler (not guards or middleware, which run earlier). To include everything, measure in middleware or at the adapter.

### 6. Real-World: Cache Hit Short-Circuit

```typescript
@Injectable()
export class SimpleCacheInterceptor implements NestInterceptor {
  private readonly cache = new Map<string, unknown>();

  intercept(ctx: ExecutionContext, next: CallHandler) {
    const key = ctx.switchToHttp().getRequest().url;
    if (this.cache.has(key)) return of(this.cache.get(key));      // handler and pipes never run
    return next.handle().pipe(tap((v) => this.cache.set(key, v)));
  }
}
```

Guards already ran, so authorization is still enforced for cache hits.

### 7. Real-World: Error Mapping in One Place

```typescript
@Catch()
export class AllExceptionsFilter implements ExceptionFilter {
  catch(exception: unknown, host: ArgumentsHost) {
    const res = host.switchToHttp().getResponse<Response>();
    const status = exception instanceof HttpException ? exception.getStatus() : 500;
    res.status(status).json({ statusCode: status, message: status === 500 ? 'Internal server error' : (exception as HttpException).message });
  }
}
// registered globally with { provide: APP_FILTER, useClass: AllExceptionsFilter }
```

### 8. Edge Case: Guard Needs the Body

```typescript
canActivate(ctx: ExecutionContext) {
  const body = ctx.switchToHttp().getRequest().body;   // raw, UNVALIDATED, NOT a DTO instance
  return typeof body?.ownerId === 'string';
}
```

Pipes have not run, so the body is untrusted and untransformed. Prefer checking ownership **in the service** after validation.

### 9. Edge Case: Middleware Cannot Know the Handler

```typescript
// Middleware cannot read @Roles() metadata: no ExecutionContext exists yet.
// Authorization based on handler metadata belongs in a guard.
```

### 10. Edge Case: Throwing in Middleware

Errors from middleware are handled by global exception filters only, because route and controller context is not available yet. A controller-level `@UseFilters()` will not catch them.

### 11. Edge Case: Interceptor "After" Not Running

```typescript
return next.handle().pipe(tap(() => log('after')));   // runs on success only
```

Use `catchError` (to map or log failures) and `finalize` (to run on both success and error) when "after" logic must always execute.

---

## Syntax / API / Commands

| Item | Purpose |
|---|---|
| `NestMiddleware` / functional middleware + `MiddlewareConsumer.apply(...).forRoutes(...)` | Middleware registration (`configure()` in a module implementing `NestModule`) |
| `app.use(fn)` | Global platform-level middleware |
| `CanActivate#canActivate(context)` | Guard contract |
| `NestInterceptor#intercept(context, next)` | Interceptor contract. Return an `Observable` |
| `PipeTransform#transform(value, metadata)` | Pipe contract |
| `ExceptionFilter#catch(exception, host)` + `@Catch(...)` | Filter contract |
| `@UseGuards`, `@UseInterceptors`, `@UsePipes`, `@UseFilters` | Controller or route binding |
| `app.useGlobalGuards/Interceptors/Pipes/Filters(...)` | Global binding (no DI) |
| `APP_GUARD`, `APP_INTERCEPTOR`, `APP_PIPE`, `APP_FILTER` | Global binding via providers (with DI) |
| `ExecutionContext`, `ArgumentsHost` | Transport-agnostic context |
| `Reflector` | Read handler/class metadata in guards/interceptors |

---

## Important Rules

1. **The type order is fixed:** middleware → guards → interceptors (before) → pipes → handler → interceptors (after) → filters (on error).
2. **Inbound is global to specific, interceptors' after-phase reverses it, and filters go specific to global.**
3. **Guards run before pipes.** They cannot rely on validated or transformed input.
4. **Middleware does not know the handler.** Anything depending on handler metadata is a guard or interceptor.
5. **A stage that stops the request prevents all later stages.**
6. **`useGlobal*()` registrations cannot inject providers.** Use `APP_*` tokens when you need DI.
7. **`@UseGuards(A, B)` runs `A` before `B`.** Order within a level is the list order.
8. **Interceptor "after" code runs on success.** Use `catchError`/`finalize` for the error path.
9. **Exceptions from guards, interceptors, pipes, and handlers go to filters,** route level first.
10. **Exceptions from middleware reach global filters only.**
11. **Route matching happens before any of this.** A pipe or guard cannot fix a wrongly routed request.
12. **Request-scoped providers rebuild the dependency subtree per request.**

---

## Under the Hood

### How Nest Builds the Chain

At startup, for each route the router explorer:

1. Collects guards, interceptors, pipes, and filters from global registrations, controller metadata, and handler metadata.
2. Creates "context creators" that instantiate them (resolving DI) once for singleton components.
3. Builds a proxy around the handler: `guardsConsumer → interceptorsConsumer(pipes → handler) → response controller`, all wrapped in the exceptions handler.

At request time the proxy builds the `ExecutionContext` and runs this chain. Because it is built once at startup, attaching components is cheap per request.

### Why Interceptors Nest

Each interceptor receives `next: CallHandler` whose `handle()` returns an observable of the rest of the chain. Interceptors compose like function wrappers. The outermost (global) runs its "before" first and its "after" last, which gives the reversed outbound order.

### Where Pipes Run Relative to Interceptors

Pipes are invoked **inside** the interceptor chain, right before the handler is called. That is why an interceptor's "before" code sees raw arguments, and why an error thrown by a pipe is an error on the interceptor's observable (a `catchError` in an interceptor can see validation failures).

### The Exceptions Zone

Nest runs route handlers inside a try/catch-like "exceptions zone". The router proxy catches thrown errors and passes them to the exception filter chain built for that route (route filters, controller filters, global filters, then the built-in base filter as the final fallback).

### Platform-Level Order

Everything the adapter does (for example Express middleware registered with `app.use()` before Nest's routes, or body parsers) happens before Nest's route handler is invoked. Express middleware order is registration order. Fastify uses hooks and content-type parsers with their own lifecycle. See [Platform Adapters](../06-internals/05-platform-adapters.md) and [Request Lifecycle Internals](../06-internals/04-request-lifecycle-internals.md).

### Shutdown Interaction (NestJS 12)

On Express, the adapter now drains in-flight requests when the application shuts down. Requests already inside the pipeline are allowed to finish before the server closes. Enable shutdown hooks (`app.enableShutdownHooks()`) and test the behavior under your signal handling.

---

## Common Patterns

### Authentication in a Global Guard, Opt-Outs With Metadata

`APP_GUARD` + `@Public()`. New routes are protected by default.

### Validation in a Global Pipe

One `ValidationPipe` for the whole app, configured in `main.ts` and in e2e test setup.

### Observability in Middleware and Interceptors

Request IDs and access logs in middleware (earliest point). Business timing and result logging in interceptors.

### Uniform Errors via a Global Filter

One filter maps exceptions to your API's error format, with stable codes.

### Caching and Timeouts as Interceptors

`CacheInterceptor`-style short circuits after guards, and `timeout()` operators that convert slow handlers to errors.

### Per-Route Overrides

Global defaults plus `@UseGuards()`/`@UsePipes()`/`@UseFilters()` for exceptions to the rule.

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Authorization in middleware needing `@Roles()` | Metadata is unavailable | Middleware has no execution context | Move to a guard |
| Guard reading a DTO | Body is raw/untyped, validation not applied | Pipes run after guards | Check raw input carefully, or authorize in the service after validation |
| Expecting interceptor "after" code on errors | Logging missing for failures | `tap` runs on success only | `catchError` / `finalize` |
| `app.useGlobalGuards(new AuthGuard(svc))` needing DI | Manual wiring pain, no injection of other deps | Global registration is outside modules | `{ provide: APP_GUARD, useClass: AuthGuard }` |
| Assuming global-first for filters | A global filter does not handle the exception first | Filters resolve route → controller → global | Design with the reversed order in mind |
| Pipe expected to fix a wrong route | Wrong handler still runs | Routing precedes pipes | Fix route order/specificity |
| Same concern in middleware and guard | Duplicate work or conflicting responses | Unclear ownership | One owner per concern |
| Controller filter not catching middleware errors | Unhandled-looking errors | Middleware errors reach global filters only | Use a global filter |
| `@UseGuards(B)` above `@UseGuards(A)` assumption | Confusing order | Stacked decorator evaluation is bottom to top | One `@UseGuards(A, B)` |
| Slow guards doing DB hits on every request | Latency | Guards run for every request | Cache, narrow, or use token claims |
| Request-scoped guard/interceptor added casually | Per-request instantiation cost, scope bubbling | Scope contagion | Keep singletons, pass context explicitly |
| Returning `false` from a guard and expecting a custom message | Generic `403` | `false` maps to Forbidden | Throw a specific exception |
| Heavy work in interceptors "before" for every request | Latency | Runs even for cache hits above | Order interceptors, short-circuit early |
| Not testing the pipeline wiring | Guards/pipes silently not applied | Registered nowhere or in test setup only | E2E tests for 401/403/400 with real bootstrap config |

---

## Debugging

### Make the Order Visible

Temporarily log from each component (as in the Basic Example), or enable debug logging. For quick checks, a global middleware, guard, interceptor, and pipe that log a numbered line is the fastest way to confirm what runs and when.

### Symptom Table

| Symptom | Check |
|---|---|
| Guard never runs | Is it registered (decorator or `APP_GUARD`)? On the right controller/route? Does the module with `APP_GUARD` get imported? |
| Pipe never runs | `ValidationPipe` registered globally? Parameter metatype a class? Route in library mode (`@Res()`)? |
| Interceptor "after" missing | Request errored or was short-circuited before the handler |
| Filter not applied | `@Catch()` type does not match the exception? Registered at a level that applies to this route? Exception thrown in middleware (global only)? |
| Order differs from expectation | Binding levels (global/controller/route). Filter order is reversed. Stacked `@UseGuards` |
| Response sent twice or hangs | `@Res()` usage mixed with an interceptor or filter writing the response |
| `req.user` undefined in a guard | Authentication guard registered after the one reading `user` |

### Techniques

- Number your log lines and compare with the documented order.
- Add a request ID in middleware and log it from each component to follow one request.
- Use Nest Devtools or the REPL to inspect registered enhancers for a route (development only). See [Debugging](../01-getting-started/06-debugging.md).
- Write e2e tests that assert: unauthenticated → `401`, unauthorized → `403`, invalid body → `400`, valid → `2xx`. They verify the entire chain. See [E2E Testing](../04-intermediate/01-testing/06-e2e-testing.md).

---

## Performance

- Components are instantiated once at startup (singletons), so the **per-request cost** is just their execution.
- **Guards run on every request.** Keep them cheap (verify a signed token locally, cache permission lookups). A database query per request in a guard is a throughput ceiling.
- **Interceptors add observable plumbing.** Keep the stack small on hot paths.
- **Validation cost** scales with DTO size and nesting. See [Request Data](./07-request-data.md).
- **Order for early exit:** reject cheaply and early (guards before expensive pipes, caching interceptors before expensive handlers).
- **Request-scoped components** rebuild per request and make dependants request-scoped. Avoid on hot paths.
- **Middleware runs even for routes that will 404** if bound broadly (`forRoutes('*')`). Bind narrowly.
- **Measure by phase:** record timestamps in middleware, guard, interceptor, and handler to see where latency lives.

---

## Security

- **Authentication and authorization belong early** (guards), before validation and business logic.
- **Secure by default:** register authentication as a global `APP_GUARD` and mark public routes explicitly. A forgotten per-route decorator then cannot expose a route.
- **Validate after authorization**, but never trust the body inside a guard. It is raw input.
- **Do not rely on middleware for per-route authorization.** It cannot see handler metadata and is easy to mis-scope.
- **Filters and error leakage:** the last stage decides what clients see. Ensure unexpected errors are generic, and log details privately.
- **Cache interceptors:** make sure cached responses are keyed per user or only used for public data. Guards run before interceptors, so authorization is enforced, but cache keys must still include whatever varies the response (user, tenant, query).
- **Order matters for rate limiting:** apply throttling guards early so abusive traffic is rejected before expensive work.
- **Test negative paths.** A passing happy path says nothing about enforcement.

---

## Production Considerations

- **Document the pipeline** for your service: which global guards, pipes, interceptors, and filters exist, in `main.ts` and `APP_*` providers, so new engineers do not guess.
- **Keep global wiring in one place** (a `CoreModule` using `APP_*` tokens) and mirror it in your e2e test bootstrap.
- **Standardize** error format (filter), logging and request IDs (middleware/interceptor), validation (pipe), authentication (guard).
- **Observe per phase** with correlation IDs and metrics (guard rejections, validation failures, handler latency). See [Request Logging and Correlation ID](../07-production/03-observability/03-request-logging-and-correlation-id.md).
- **Graceful shutdown:** enable shutdown hooks and verify in-flight requests complete (NestJS 12 drains Express in-flight requests).
- **Review ordering when upgrading:** NestJS 12 changed lifecycle hook ordering and reworked adapter error mapping. Re-run your e2e suite and review custom filters.
- **Avoid per-request heavy dependencies** in global components.

---

## Best Practices

### Recommended

```typescript
// core.module.ts: global wiring in one place, injectable
@Module({
  providers: [
    { provide: APP_GUARD, useClass: AuthGuard },
    { provide: APP_GUARD, useClass: RolesGuard },
    { provide: APP_PIPE, useValue: new ValidationPipe({ whitelist: true, forbidNonWhitelisted: true, transform: true }) },
    { provide: APP_INTERCEPTOR, useClass: TimingInterceptor },
    { provide: APP_FILTER, useClass: AllExceptionsFilter },
  ],
})
export class CoreModule {}
```

### Avoid

```typescript
// main.ts: scattered, non-injectable, easy to forget in tests
app.useGlobalGuards(new AuthGuard());                   // cannot inject services
// ...validation configured only on some controllers
// ...each handler formats its own errors
```

Why: one visible, injectable global configuration makes behavior uniform and testable. Scattered setup drifts, is not mirrored in tests, and leaves gaps.

Additional guidance:

- One owner per concern: authentication → guard, validation → pipe, formatting errors → filter.
- Put cheap, broad checks early. Put expensive, specific checks late.
- Prefer global defaults with explicit per-route overrides.
- Keep each component small and single-purpose.
- Use the same bootstrap configuration function in `main.ts` and in e2e tests.

---

## Version / Compatibility Notes

| Item | NestJS 12 | Earlier |
|---|---|---|
| Pipeline order (middleware → guards → interceptors → pipes → handler → interceptors → filters) | Unchanged | Same |
| Lifecycle hook ordering (`onModuleInit`, etc.) | By component hierarchy level, may change relative order | Previous ordering |
| HTTP adapter error mapping | Reworked in v12. *Review custom filters tied to adapter-specific errors* | Previous behavior |
| Express shutdown | Drains in-flight requests | Did not drain |
| Route conflict diagnostics | Opt-in `routeConflictPolicy` / `routeResolutionStrategy` | Not available |
| `APP_*` tokens | Available | Available |
| Standard Schema in route decorators | Attached as metadata, enforced by `StandardSchemaValidationPipe` | Not available |
| Microservice pre-request hooks | Available (microservices) | Available |

*Verify against the official request lifecycle FAQ and migration guide for your version.*

---

## Real-World Use Cases

- **API platforms** with uniform auth, validation, logging, and error formats applied globally.
- **Multi-tenant services** with a tenant-resolving middleware or guard feeding downstream components.
- **Caching layers** implemented as interceptors after authentication.
- **Rate limiting and abuse protection** as early guards.
- **Observability:** correlation IDs in middleware, timings in interceptors.
- **Internal gateways** that normalize requests and errors before reaching services.
- **Compliance logging** (audit trails) via interceptors that run after the handler with the user and result.

---

## Interview Questions

### Beginner

1. In what order do middleware, guards, interceptors, and pipes run?
   - Middleware, guards, interceptors (before), pipes, then the handler.
2. What does an exception filter do?
   - Catches thrown exceptions and turns them into responses.
3. What is the difference between a guard and middleware?
   - Middleware runs first on the raw request without knowing the handler. A guard runs after, knows the handler via `ExecutionContext`, and decides whether to proceed.
4. Where do validation and transformation of parameters happen?
   - In pipes, just before the handler.

### Intermediate

1. Why do guards run before pipes?
   - Unauthorized requests should be rejected before spending effort on validation or running business logic, and guards must not depend on transformed input.
2. In what order do interceptors run on the way out?
   - Reverse of the way in: route, controller, global.
3. In what order do exception filters apply?
   - Route, then controller, then global: the opposite of the global-first rule for other components.
4. How do you register a global guard that can inject services?
   - Provide it with `APP_GUARD` in a module (`{ provide: APP_GUARD, useClass: AuthGuard }`). `app.useGlobalGuards()` instances cannot use DI.
5. If a guard returns `false`, which later components run?
   - None of the pipeline. The request ends with a `403` (via filters).

### Advanced

1. Where does an interceptor's "after" logic not run, and how do you cover that?
   - On errors, because `tap` only runs on success. Use `catchError` and `finalize`.
2. Why can't middleware implement role-based authorization based on `@Roles()`?
   - No execution context exists yet, so handler and class metadata are not available. A guard has that access via `Reflector`.
3. How are pipes related to the interceptor chain?
   - They run inside it, right before the handler, so interceptors' "before" code sees raw arguments and can observe pipe errors.
4. How would you design the pipeline for a secure-by-default API?
   - Global authentication guard with `@Public()` opt-outs, global validation pipe with whitelisting, a global filter for uniform errors, and interceptors for timing and logging, all registered via `APP_*` tokens and mirrored in e2e tests.
5. How can a cache interceptor stay safe with authentication?
   - Guards run before interceptors, so authorization is enforced on hits. The cache key must still include user or tenant when responses vary.
6. What changed in NestJS 12 that could affect behavior of the lifecycle?
   - Lifecycle hooks are called by hierarchy level, the adapter's error mapping was reworked, and Express drains in-flight requests at shutdown. Re-test custom filters and initialization order.

---

## Quick Reference

```text
Order (in)    Adapter (parse, route) → Middleware → Guards → Interceptors(before) → Pipes → Handler
Order (out)   Handler → Interceptors(after) → Response            (errors → Exception filters)
Across levels
   guards / interceptors(before) / pipes     global → controller → route (→ parameter for pipes)
   interceptors(after)                       route → controller → global      (reversed)
   exception filters                         route → controller → global      (not global-first)
   middleware                                global (app.use) → module-bound
Know the handler?   middleware NO · guard YES · interceptor YES · pipe YES (argument)
Short-circuits      anything that throws/returns early skips all later stages
Global + DI         { provide: APP_GUARD | APP_PIPE | APP_INTERCEPTOR | APP_FILTER, useClass }   (app.useGlobal*() → no DI)
Guard + body        body is raw and unvalidated inside guards
After + errors      tap = success only → use catchError / finalize
Middleware errors   global filters only
Secure by default   global auth guard + @Public() opt-out
```

---

## Key Takeaways

- The pipeline is fixed: middleware, guards, interceptors (before), pipes, handler, interceptors (after), with exception filters catching errors.
- Inbound order is global to specific. The interceptors' after-phase reverses it. Filters are specific to global.
- Guards decide access and run before pipes, so they see raw, unvalidated input. Middleware cannot see handler metadata at all.
- Any stage that stops the request prevents every later stage.
- Use `APP_*` provider tokens for global components that need dependency injection. `useGlobal*()` cannot inject.
- Interceptor "after" logic runs only on success unless you add `catchError` or `finalize`.
- Put each concern in the component whose position fits it: auth in guards, validation in pipes, error format in filters, cross-cutting wrapping in interceptors, raw request preprocessing in middleware.
- Test the negative paths end to end. They are the only proof the pipeline enforces what you meant.

---

## Related Topics

```text
08 Response Handling
      ↓
[09 Request Lifecycle]
      ↓
03-core-concepts/01 Request Pipeline (Pipeline Overview, Middleware, Pipes, Guards, Interceptors, Filters)
      ↓
06-internals/04 Request Lifecycle Internals
```

- [Fundamentals Overview](./README.md)
- [Controllers](./03-controllers.md)
- [Decorators](./06-decorators.md)
- [Request Data](./07-request-data.md)
- [Response Handling](./08-response-handling.md)
- [Pipeline Overview and Execution Order](../03-core-concepts/01-request-pipeline/01-pipeline-overview-and-execution-order.md)
- [Middleware](../03-core-concepts/01-request-pipeline/02-middleware.md)
- [Guards](../03-core-concepts/01-request-pipeline/04-guards.md)
- [Interceptors](../03-core-concepts/01-request-pipeline/05-interceptors.md)
- [Exception Filters](../03-core-concepts/01-request-pipeline/08-exception-filters.md)
- [Request Lifecycle Internals](../06-internals/04-request-lifecycle-internals.md)