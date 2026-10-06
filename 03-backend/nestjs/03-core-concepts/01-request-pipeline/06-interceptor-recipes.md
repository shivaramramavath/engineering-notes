# Interceptor Recipes

Practical, copy-adaptable interceptors. Mechanics are in [Interceptors](./05-interceptors.md); read that first.

Each recipe notes where it breaks down, because the failure cases matter more than the happy path.

## 1. Request logging with duration

Logs method, URL, status, and duration for every request, including failures.

```ts
// logging.interceptor.ts
import { CallHandler, ExecutionContext, Injectable, Logger, NestInterceptor } from '@nestjs/common';
import { Observable, tap } from 'rxjs';

@Injectable()
export class LoggingInterceptor implements NestInterceptor {
  private readonly logger = new Logger('HTTP');

  intercept(context: ExecutionContext, next: CallHandler): Observable<unknown> {
    if (context.getType() !== 'http') return next.handle();

    const req = context.switchToHttp().getRequest();
    const res = context.switchToHttp().getResponse();
    const start = Date.now();
    const label = `${req.method} ${req.originalUrl}`;

    return next.handle().pipe(
      tap({
        next: () => this.logger.log(`${label} ${res.statusCode} +${Date.now() - start}ms`),
        error: (err) =>
          this.logger.warn(`${label} ${err?.status ?? 500} +${Date.now() - start}ms`),
      }),
    );
  }
}
```

Caveats:

- On the `error` path the response status isn't final yet (filters haven't run), so read `err.status` rather than `res.statusCode`.
- Requests rejected earlier by middleware or guards never reach an interceptor, so they won't be logged here. For complete access logs, log from middleware via `res.on('finish')` ([middleware](./02-middleware.md)) or a logger like pino ([request logging](../../07-production/03-observability/03-request-logging-and-correlation-id.md)).

## 2. Response envelope

Wrap successful responses as `{ data, ... }` for a consistent shape.

```ts
// response-envelope.interceptor.ts
import { CallHandler, ExecutionContext, Injectable, NestInterceptor } from '@nestjs/common';
import { Reflector } from '@nestjs/core';
import { map, Observable } from 'rxjs';

export const SKIP_ENVELOPE = 'skipEnvelope';
export const SkipEnvelope = () => SetMetadata(SKIP_ENVELOPE, true);

@Injectable()
export class ResponseEnvelopeInterceptor implements NestInterceptor {
  constructor(private readonly reflector: Reflector) {}

  intercept(context: ExecutionContext, next: CallHandler): Observable<unknown> {
    const skip = this.reflector.getAllAndOverride<boolean>(SKIP_ENVELOPE, [
      context.getHandler(),
      context.getClass(),
    ]);
    if (skip) return next.handle();

    return next.handle().pipe(map((data) => ({ data: data ?? null })));
  }
}
```

(`SetMetadata` comes from `@nestjs/common`.)

Caveats:

- **Opt-out is not optional.** Health checks, file downloads, redirects, and webhooks usually must not be wrapped.
- Paginated results: have the service return `{ items, meta }` and map that to `{ data: items, meta }` deliberately. Don't let the envelope guess.
- **Swagger won't know about the envelope** unless you document it (e.g., a custom decorator with `ApiOkResponse` + `getSchemaPath`). See [responses and examples](../../04-intermediate/09-openapi-and-swagger/05-responses-and-examples.md).
- Errors aren't wrapped by this interceptor; they go through filters. Keep the error format consistent there ([response and error format](../../04-intermediate/08-api-design/05-response-and-error-format.md)).
- Consider whether you need an envelope at all. HTTP status codes plus a plain body is often cleaner.

## 3. Timeout

Fail slow handlers with `408 Request Timeout`.

```ts
// timeout.interceptor.ts
import { CallHandler, ExecutionContext, Injectable, NestInterceptor, RequestTimeoutException } from '@nestjs/common';
import { catchError, Observable, throwError, timeout, TimeoutError } from 'rxjs';

@Injectable()
export class TimeoutInterceptor implements NestInterceptor {
  constructor(private readonly ms = 5000) {}

  intercept(_ctx: ExecutionContext, next: CallHandler): Observable<unknown> {
    return next.handle().pipe(
      timeout(this.ms),
      catchError((err) =>
        err instanceof TimeoutError
          ? throwError(() => new RequestTimeoutException())
          : throwError(() => err),
      ),
    );
  }
}
```

Caveats (important):

- **This does not cancel the work.** RxJS unsubscribes from the stream and the client gets an error, but the handler's promise (DB query, outbound HTTP call) keeps running. For real cancellation, pass an `AbortSignal` to the underlying operations.
- Not suitable for SSE or long-lived streams; the timeout applies to the first emission, not the connection lifetime.
- Because the constructor takes a primitive, bind with an instance: `@UseInterceptors(new TimeoutInterceptor(3000))`.

## 4. Simple cache (short-circuit)

Demonstrates skipping the handler. For production caching use the built-in `CacheInterceptor` from `@nestjs/cache-manager` ([cache-manager](../../05-advanced/01-caching/02-cache-manager.md)).

```ts
@Injectable()
export class SimpleCacheInterceptor implements NestInterceptor {
  private readonly cache = new Map<string, { value: unknown; expires: number }>();

  intercept(context: ExecutionContext, next: CallHandler): Observable<unknown> {
    const req = context.switchToHttp().getRequest();
    if (req.method !== 'GET') return next.handle();

    const key = req.originalUrl;
    const hit = this.cache.get(key);
    if (hit && hit.expires > Date.now()) return of(hit.value); // handler skipped

    return next.handle().pipe(
      tap((value) => this.cache.set(key, { value, expires: Date.now() + 10_000 })),
    );
  }
}
```

Caveats: in-memory means per-process (wrong with multiple instances), unbounded growth, no invalidation, and the key ignores user identity, so **never cache per-user responses under a shared key**.

## 5. Error translation with `catchError`

Convert a library or domain error into an HTTP exception close to where it occurs.

```ts
@Injectable()
export class DomainErrorInterceptor implements NestInterceptor {
  intercept(_ctx: ExecutionContext, next: CallHandler): Observable<unknown> {
    return next.handle().pipe(
      catchError((err) => {
        if (err instanceof InsufficientFundsError) {
          return throwError(() => new UnprocessableEntityException(err.message));
        }
        return throwError(() => err); // always re-throw what you don't handle
      }),
    );
  }
}
```

For app-wide error mapping an [exception filter](./08-exception-filters.md) is usually the better home: it also catches errors from guards and pipes, which interceptor `catchError` cannot see for guards (guards run before interceptors).

## 6. Setting headers from an interceptor

```ts
return next.handle().pipe(
  tap(() => context.switchToHttp().getResponse().setHeader('x-served-by', 'api-1')),
);
```

Headers must be set before the response is sent. `tap` on the success path runs before Nest writes the body, so this works for normal handlers, but not after streaming has started.

## Choosing between recipes and built-ins

| Need | Prefer |
|------|--------|
| Serialize entities (`@Exclude`, `@Expose`) | Built-in `ClassSerializerInterceptor` ([serialization](../02-validation-and-serialization/07-serialization.md)) |
| HTTP caching | `CacheInterceptor` ([cache-manager](../../05-advanced/01-caching/02-cache-manager.md)) |
| Access logs | Middleware or pino-based logging |
| Rate limiting | Throttler guard ([rate limiting](../../07-production/01-security/04-rate-limiting-and-brute-force-protection.md)) |

## Common mistakes

- Envelopes with no opt-out.
- Treating `timeout` as cancellation.
- Caching without accounting for auth, query strings, or multiple instances.
- `catchError` that swallows errors by returning `of(undefined)` instead of re-throwing.
- Using `res.statusCode` in the error path.

## Testing

Interceptors are plain classes: call `intercept()` with a mock context and a `CallHandler` returning `of(value)` or `throwError`, then assert on the resulting observable (`lastValueFrom`). See [testing pipeline components](../../04-intermediate/01-testing/04-testing-pipeline-components.md).

## Quick Summary

- Logging: `tap` with `next` and `error` callbacks; read the status from the error on failures.
- Envelope: `map`, and always provide a skip mechanism.
- Timeout: `timeout` + `catchError` → `RequestTimeoutException`; it doesn't cancel work.
- Cache: return `of(cached)` to skip the handler; prefer the built-in.
- Error translation: `catchError` + re-throw, but filters are usually the better place.

## Next

[HTTP exceptions →](./07-http-exceptions.md)
