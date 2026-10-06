# Pipeline Overview and Execution Order

Nest gives you five hooks into a request's life: **middleware, guards, interceptors, pipes, exception filters**. Knowing exactly *when* each runs, and *what each can see*, tells you where any piece of cross-cutting logic belongs.

The short version lives in [Request lifecycle](../../02-fundamentals/09-request-lifecycle.md). This note is the detailed reference.

## The full order

```text
Incoming request
   │
   ▼
1. Middleware            global (app.use) → module-bound
   │
   ▼
2. Guards                global → controller → route
   │
   ▼
3. Interceptors (pre)    global → controller → route
   │
   ▼
4. Pipes                 global → controller → route → route parameter
   │
   ▼
5. Controller handler  →  service  →  ...
   │
   ▼
6. Interceptors (post)   route → controller → global   (reverse of pre)
   │
   ▼
7. Exception filters     route → controller → global   (only if something threw)
   │
   ▼
Response
```

Two rules cover most of it:

- **Guards, interceptors, pipes**: outer scope runs first (global → controller → route) on the way in.
- **Interceptors on the way out** unwind in reverse, like nested function calls. **Exception filters** are resolved from the most specific scope outward.

## What each block can see

| Block | Has `ExecutionContext`? | Knows the target handler? | Can short-circuit? | Typical job |
|-------|------------------------|---------------------------|--------------------|-------------|
| Middleware | No (`req, res, next`) | No | Yes (respond or skip `next()`) | Raw request tweaks, request IDs, body parser config |
| Guard | Yes | Yes | Yes (`false` / throw) | Authn / authz |
| Interceptor | Yes | Yes | Yes (don't call `next.handle()`) | Logging, mapping, caching, timeout |
| Pipe | No (value + metadata) | No | By throwing | Validate / transform one argument |
| Exception filter | `ArgumentsHost` | No | Always ends the request | Error → response |

The "knows the handler" column is why guards do authorization and middleware does not: a guard can read metadata like `@Roles('admin')` from the handler via `Reflector`; middleware cannot, because it runs before Nest has decided which handler will run.

## Seeing the order yourself

The fastest way to internalize this is a throwaway demo.

```ts
// demo.ts
import {
  CallHandler, CanActivate, Controller, ExecutionContext, Get, Injectable,
  MiddlewareConsumer, Module, NestInterceptor, NestMiddleware, Param,
  PipeTransform, UseGuards, UseInterceptors,
} from '@nestjs/common';
import { Request, Response, NextFunction } from 'express';
import { tap } from 'rxjs';

@Injectable()
export class DemoMiddleware implements NestMiddleware {
  use(req: Request, res: Response, next: NextFunction) {
    console.log('1. middleware');
    next();
  }
}

@Injectable()
export class DemoGuard implements CanActivate {
  canActivate() {
    console.log('2. guard');
    return true;
  }
}

@Injectable()
export class DemoInterceptor implements NestInterceptor {
  intercept(_ctx: ExecutionContext, next: CallHandler) {
    console.log('3. interceptor (before)');
    return next.handle().pipe(tap(() => console.log('6. interceptor (after)')));
  }
}

@Injectable()
export class DemoPipe implements PipeTransform {
  transform(value: unknown) {
    console.log('4. pipe');
    return value;
  }
}

@Controller('demo')
@UseGuards(DemoGuard)
@UseInterceptors(DemoInterceptor)
export class DemoController {
  @Get(':id')
  find(@Param('id', DemoPipe) id: string) {
    console.log('5. handler');
    return { id };
  }
}

@Module({ controllers: [DemoController] })
export class DemoModule {
  configure(consumer: MiddlewareConsumer) {
    consumer.apply(DemoMiddleware).forRoutes(DemoController);
  }
}
```

`GET /demo/42` logs:

```text
1. middleware
2. guard
3. interceptor (before)
4. pipe
5. handler
6. interceptor (after)
```

Now throw an exception inside the handler: step 6 disappears (`tap`'s success callback never fires) and the exception goes to the filters instead.

## Where exceptions can come from

An exception thrown **anywhere** after routing (guard, interceptor, pipe, handler, service) ends up in the exception layer. That layer picks the matching filter, or the built-in one, and builds the response. Related behavior:

- A request that matches no route gets a `NotFoundException` from the router, which is also handled by the exception layer (so a global filter typically sees 404s too).
- A guard returning `false` produces a `ForbiddenException` (403).
- Because interceptors wrap the handler, they can also catch errors with `catchError` *before* filters see them (see [Interceptor recipes](./06-interceptor-recipes.md)).

## Choosing the right block

```text
Need to read/modify the raw req/res before routing matters?   → Middleware
Need to allow or deny based on identity/role/metadata?        → Guard
Need to run code around the handler (timing, mapping, cache)? → Interceptor
Need to validate or convert one input value?                  → Pipe
Need to shape how errors become HTTP responses?               → Exception filter
```

Common borderline cases:

| Scenario | Use | Why |
|----------|-----|-----|
| Attach request ID for logging | Middleware | Needed before everything else, no handler knowledge required |
| Verify JWT and attach `req.user` | Guard | It's an allow/deny decision; needs handler metadata (`@Public()`) |
| Wrap all responses in `{ data, meta }` | Interceptor | Operates on the handler's return value |
| Convert `:id` to a number | Pipe | Single-argument transformation |
| Map DB unique-constraint errors to 409 | Exception filter | Error-to-response translation |
| Parse raw body for webhook signature | Middleware / bootstrap option | Must happen before body parsing consumes the stream |

## Registering globally: `useGlobal*` vs `APP_*`

```ts
// main.ts: simple, but NO dependency injection
app.useGlobalGuards(new AuthGuard());
```

```ts
// app.module.ts: DI-aware, preferred when the enhancer has dependencies
@Module({
  providers: [
    { provide: APP_GUARD, useClass: AuthGuard },
    { provide: APP_INTERCEPTOR, useClass: LoggingInterceptor },
    { provide: APP_PIPE, useClass: ValidationPipe },
    { provide: APP_FILTER, useClass: AllExceptionsFilter },
  ],
})
export class AppModule {}
```

`useGlobal*` runs outside any module context, so the instance can't inject providers. `APP_*` registers the class as a normal provider inside a module, so DI works. Use `APP_*` whenever the class has constructor dependencies. Global enhancers also aren't picked up by e2e tests that build a module without `main.ts` setup, which is another reason to prefer `APP_*`.

If multiple `APP_*` providers of the same kind exist, they run in the order their modules/providers are registered; don't depend on that ordering across modules. Put ordering-sensitive logic in one place.

## Common mistakes and misconceptions

- **"Guards run before middleware."** No, middleware first, always.
- **"Pipes run before interceptors."** Interceptors' *before* logic runs first. Pipes run right before the handler, so an interceptor sees the *unvalidated* raw input.
- **Doing authorization in middleware.** Middleware can't read handler metadata, so you end up hard-coding route paths. Use guards.
- **Using `@Res()` and expecting interceptors/filters to behave.** Injecting the response object switches that handler to library-specific mode: Nest no longer sends the return value, so response-mapping interceptors have no effect. Use `@Res({ passthrough: true })` if you only need to set headers or cookies.
- **Global pipes/filters via `useGlobal*` that need injected services** silently get no DI. Use `APP_*`.
- **Registering the same filter at several levels** and wondering why the error is handled once: the first matching filter handles it and the rest don't run.

## Debugging order problems

- Add a `console.log` / `Logger.debug` to each layer, as in the demo above.
- If middleware doesn't run: check `forRoutes` paths, `exclude`, and the global prefix (see [Middleware](./02-middleware.md)).
- If a guard doesn't run: confirm it's bound (`@UseGuards`, `APP_GUARD`) and the route actually matched.
- If an interceptor's *after* logic doesn't run: an exception was thrown, or the handler uses `@Res()`.
- Internals of how Nest builds this chain per route: [Request lifecycle internals](../../06-internals/04-request-lifecycle-internals.md).

## Quick Summary

- Order: middleware → guards → interceptors (pre) → pipes → handler → interceptors (post) → filters (on error).
- Global → controller → route on the way in; interceptors unwind in reverse; filters resolve most-specific first.
- Guards/interceptors have `ExecutionContext` and know the handler; middleware and pipes don't.
- Use `APP_*` tokens when a global enhancer needs DI.
- Pick the block by *job*: allow/deny → guard, wrap → interceptor, convert → pipe, error → filter.

## Next

[Middleware →](./02-middleware.md)
