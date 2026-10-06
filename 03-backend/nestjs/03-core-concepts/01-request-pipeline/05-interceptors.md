# Interceptors

An interceptor **wraps the handler**. It runs code before the handler, then gets a chance to work on the handler's result (or error) as an RxJS stream. That makes it the right tool for timing, response mapping, caching, timeouts, and error translation.

This note covers the mechanics. Ready-to-use patterns are in [Interceptor recipes](./06-interceptor-recipes.md).

## Prerequisites

You need a working grasp of RxJS `Observable` and a few operators (`map`, `tap`, `catchError`, `timeout`). You don't need more than that.

## Core concept

```ts
export interface NestInterceptor<T = any, R = any> {
  intercept(context: ExecutionContext, next: CallHandler<T>): Observable<R> | Promise<Observable<R>>;
}
```

- `context` is the [`ExecutionContext`](./09-execution-context.md) (same one guards get).
- `next` is a `CallHandler`. Calling `next.handle()` **invokes the route handler** and returns an `Observable` of its result.
- What you return from `intercept()` becomes the response.

```text
request ──► intercept() ──► [pre-logic] ──► next.handle() ──► handler runs
                                                │
response ◄── [post-logic via RxJS operators] ◄──┘
```

The crucial detail: **the handler runs only when `next.handle()` is called**. Not calling it skips the handler entirely, which is how cache interceptors work.

## Basic example

```ts
// timing.interceptor.ts
import { CallHandler, ExecutionContext, Injectable, NestInterceptor } from '@nestjs/common';
import { Observable, tap } from 'rxjs';

@Injectable()
export class TimingInterceptor implements NestInterceptor {
  intercept(context: ExecutionContext, next: CallHandler): Observable<unknown> {
    const start = Date.now();                       // pre: runs before the handler
    return next.handle().pipe(
      tap(() => console.log(`took ${Date.now() - start}ms`)), // post: runs when the handler returns
    );
  }
}
```

## Binding

```ts
@UseInterceptors(TimingInterceptor)      // route or controller
@Get() findAll() {}

app.useGlobalInterceptors(new TimingInterceptor());        // no DI
{ provide: APP_INTERCEPTOR, useClass: TimingInterceptor }  // global, DI-aware
```

Order on the way in: global → controller → route. On the way out the stack unwinds in reverse (route → controller → global), like nested function calls.

## What you can do with the stream

| Goal | Operator | Notes |
|------|----------|-------|
| Run side effects, don't change the value | `tap` | Logging, metrics |
| Change the response body | `map` | Envelope, field renaming |
| Handle or translate errors | `catchError` | Return `throwError(() => new X())` to re-throw as another exception |
| Enforce a deadline | `timeout(ms)` | Throws `TimeoutError` you should translate |
| Skip the handler | return `of(value)` without calling `next.handle()` | Caching, feature flags |
| Change behavior based on handler metadata | `Reflector` | Same pattern as guards |

```ts
// map: transform what the handler returned
return next.handle().pipe(map((data) => ({ data })));

// short-circuit: handler never runs
if (cached) return of(cached);
return next.handle();
```

## How it works with async code

`intercept()` can be `async` (it may return `Promise<Observable>`). Pre-logic can `await` something, then return `next.handle().pipe(...)`.

```ts
async intercept(ctx: ExecutionContext, next: CallHandler) {
  await this.audit.begin(ctx.getHandler().name);
  return next.handle();
}
```

Awaiting only delays the handler. It doesn't change how the stream behaves.

## Important behavior

- **Interceptors run before pipes.** Pre-logic sees raw, unvalidated input. If you need validated data, work in the handler or a pipe.
- **`map` receives whatever the handler returned**, including `undefined` for handlers that return nothing. Handle that deliberately.
- **Errors skip `map`/`tap` success callbacks.** To observe errors use `catchError` or `tap({ error })`. If you re-throw from `catchError`, filters see the new error.
- **Streaming / SSE**: the stream may emit multiple values; `map` applies to each. See [SSE and streaming](../../05-advanced/04-realtime/06-server-sent-events-and-streaming.md).
- **`@Res()` disables response mapping.** When a handler injects the response object without `passthrough: true`, Nest stops handling the response for that route, so interceptors that map the return value have no effect.
- **Request-scoped providers** inside an interceptor make it request-scoped too, with a per-request instantiation cost. See [scopes](../04-modules-and-di/07-scopes-and-request-context.md).
- **Interceptors run for any transport** (HTTP, WebSocket, microservice, GraphQL). If your logic touches `req`/`res`, check `context.getType()` first.

## Interceptors vs the alternatives

| Need | Prefer |
|------|--------|
| Run before/after the handler and touch the result | Interceptor |
| Allow/deny | Guard |
| Convert one argument | Pipe |
| Raw `req/res` hook, no handler knowledge | Middleware |
| Reshape errors into responses | [Exception filter](./08-exception-filters.md) (or `catchError` for local translation) |

## Common mistakes

- **Forgetting to return `next.handle()`** (or a replacement observable). The request hangs or errors.
- **Mutating the result inside `tap`.** `tap` is for side effects; use `map` to produce a new value.
- **Subscribing manually** (`next.handle().subscribe(...)`) instead of returning the piped observable. The response won't reflect your changes and you risk running the handler twice.
- **Calling `next.handle()` more than once.** Each call re-runs the handler.
- **Putting blocking or slow work in pre-logic** (it delays *every* request).
- **Wrapping every response in an envelope without a way to opt out**: file downloads, health checks, and webhooks usually shouldn't be wrapped. Use metadata and `Reflector` to skip.

## Debugging

- "Post" logic never runs? An exception was thrown, or the handler uses `@Res()`.
- Response unchanged after `map`? You used `tap`, or the route bypasses Nest's response handling.
- Interceptor runs on unexpected routes? Check global registration (`APP_INTERCEPTOR`) and controller-level `@UseInterceptors`.
- Order confusion? Log pre/post with a label per interceptor and compare with [the execution order](./01-pipeline-overview-and-execution-order.md).

## Quick Summary

- An interceptor wraps the handler: pre-logic, `next.handle()`, then RxJS operators for post-logic.
- The handler runs only if you call `next.handle()`.
- `tap` = side effects, `map` = change the value, `catchError` = translate errors, `timeout` = deadline.
- Runs after guards, before pipes; unwinds in reverse order on the way out.
- `@Res()` without passthrough bypasses response mapping.

## Next

[Interceptor recipes →](./06-interceptor-recipes.md)
