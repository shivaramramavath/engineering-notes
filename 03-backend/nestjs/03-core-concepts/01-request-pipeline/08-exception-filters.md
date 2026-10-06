# Exception Filters

An exception filter is the **last stop for errors**: it receives whatever was thrown during request handling and decides what response to send. Nest ships a built-in global filter that handles `HttpException` and turns everything else into a generic 500. You write your own when you need to:

- unify the error response shape,
- map non-HTTP errors (domain errors, ORM errors) to proper status codes,
- add logging, error tracking, or correlation IDs.

Prerequisite: [HTTP exceptions](./07-http-exceptions.md).

## Core concept

```ts
export interface ExceptionFilter<T = any> {
  catch(exception: T, host: ArgumentsHost): any;
}
```

- `@Catch(X, Y)` declares which exception types the filter handles. `@Catch()` with no arguments catches **everything**.
- `host` is an [`ArgumentsHost`](./09-execution-context.md), used to reach the underlying request/response.

## Basic example

Handle `HttpException` and produce a consistent body.

```ts
// http-exception.filter.ts
import { ArgumentsHost, Catch, ExceptionFilter, HttpException } from '@nestjs/common';
import { Request, Response } from 'express';

@Catch(HttpException)
export class HttpExceptionFilter implements ExceptionFilter {
  catch(exception: HttpException, host: ArgumentsHost) {
    const ctx = host.switchToHttp();
    const res = ctx.getResponse<Response>();
    const req = ctx.getRequest<Request>();
    const status = exception.getStatus();
    const body = exception.getResponse();

    res.status(status).json({
      statusCode: status,
      path: req.url,
      timestamp: new Date().toISOString(),
      error: typeof body === 'string' ? { message: body } : body,
    });
  }
}
```

## Binding

```ts
@UseFilters(HttpExceptionFilter)            // route
@Post() create() {}

@UseFilters(HttpExceptionFilter)            // controller
@Controller('cats') export class CatsController {}

app.useGlobalFilters(new HttpExceptionFilter());           // global, no DI
{ provide: APP_FILTER, useClass: HttpExceptionFilter }     // global, DI-aware
```

Pass the **class** to `@UseFilters` where possible so Nest can inject dependencies. Passing an instance works but gets no DI.

Resolution: the most specific scope is checked first (route → controller → global). The first filter whose `@Catch` type matches handles the exception and the rest are skipped.

## Practical usage: a catch-all filter

Handles both `HttpException` and unknown errors, platform-agnostically (works with Express or Fastify) via `HttpAdapterHost`.

```ts
// all-exceptions.filter.ts
import { ArgumentsHost, Catch, ExceptionFilter, HttpException, HttpStatus, Logger } from '@nestjs/common';
import { HttpAdapterHost } from '@nestjs/core';

@Catch()
export class AllExceptionsFilter implements ExceptionFilter {
  private readonly logger = new Logger(AllExceptionsFilter.name);

  constructor(private readonly adapterHost: HttpAdapterHost) {}

  catch(exception: unknown, host: ArgumentsHost) {
    const { httpAdapter } = this.adapterHost;
    const ctx = host.switchToHttp();

    const isHttp = exception instanceof HttpException;
    const status = isHttp ? exception.getStatus() : HttpStatus.INTERNAL_SERVER_ERROR;
    const payload = isHttp ? exception.getResponse() : { message: 'Internal server error' };

    if (!isHttp) {
      this.logger.error(exception instanceof Error ? exception.stack : String(exception));
    }

    httpAdapter.reply(
      ctx.getResponse(),
      {
        statusCode: status,
        path: httpAdapter.getRequestUrl(ctx.getRequest()),
        timestamp: new Date().toISOString(),
        ...(typeof payload === 'string' ? { message: payload } : payload),
      },
      status,
    );
  }
}
```

```ts
// app.module.ts. Needs DI, so use APP_FILTER, not useGlobalFilters(new ...)
providers: [{ provide: APP_FILTER, useClass: AllExceptionsFilter }]
```

Notes:

- `HttpAdapterHost` is how a filter gets the platform adapter. It's resolved at runtime, so use it from `catch()`, not the constructor body.
- Spreading `payload` into the body can overwrite your `statusCode` if the exception body already has one. Decide your canonical shape deliberately.
- Never include stack traces or raw error messages for unknown errors in the response.

## Mapping domain and ORM errors

Filters are the right place to translate library errors into HTTP semantics, keeping services free of HTTP concerns.

```ts
// domain error
export class OrderAlreadyPaidError extends Error {}

@Catch(OrderAlreadyPaidError)
export class OrderErrorFilter implements ExceptionFilter {
  catch(err: OrderAlreadyPaidError, host: ArgumentsHost) {
    const res = host.switchToHttp().getResponse();
    res.status(409).json({ statusCode: 409, code: 'ORDER_ALREADY_PAID', message: err.message });
  }
}
```

Prisma example (known request error codes):

```ts
import { Prisma } from '@prisma/client';

@Catch(Prisma.PrismaClientKnownRequestError)
export class PrismaExceptionFilter implements ExceptionFilter {
  catch(err: Prisma.PrismaClientKnownRequestError, host: ArgumentsHost) {
    const res = host.switchToHttp().getResponse();
    switch (err.code) {
      case 'P2002': // unique constraint
        return res.status(409).json({ statusCode: 409, message: 'Resource already exists' });
      case 'P2025': // record not found
        return res.status(404).json({ statusCode: 404, message: 'Resource not found' });
      default:
        return res.status(500).json({ statusCode: 500, message: 'Internal server error' });
    }
  }
}
```

See [database errors](../../04-intermediate/02-database-foundations/07-database-errors.md) for the ORM side.

## Extending the built-in filter

To add behavior (e.g., logging) while keeping Nest's default responses, extend `BaseExceptionFilter` and delegate:

```ts
import { BaseExceptionFilter } from '@nestjs/core';

@Catch()
export class LoggingFilter extends BaseExceptionFilter {
  catch(exception: unknown, host: ArgumentsHost) {
    // log / report to error tracker here
    super.catch(exception, host);
  }
}
```

When registering a `BaseExceptionFilter` subclass globally via `useGlobalFilters`, it needs the `HttpAdapterHost`: `app.useGlobalFilters(new LoggingFilter(app.get(HttpAdapterHost).httpAdapter))`. Registering through `APP_FILTER` avoids this wiring.

## Important behavior

- A filter **ends the request**: it must send a response (or delegate to `super.catch`).
- **Order of declaration matters** when you combine a catch-all (`@Catch()`) with specific filters at the same level. List the catch-all first and specific ones after, since Nest evaluates filters from the end of the list, so specific filters get priority. Check by testing, not assumption.
- Filters handle exceptions from **guards, interceptors, pipes, and handlers**. They do not cover bootstrap errors, background jobs, queue consumers, or event-emitter listeners.
- **Hybrid apps:** an HTTP filter doesn't apply to WebSocket gateways or microservice handlers. Gateways use `BaseWsExceptionFilter` / `WsException`; microservices use `RpcException` and their own filters. Branch on `host.getType()` in shared filters ([execution context](./09-execution-context.md)).
- With `@Res()` handlers, responses you've already started may conflict with a filter's reply. Avoid sending twice.
- The validation errors array (`message: string[]`) passes through unchanged unless your filter reshapes it.

## Common mistakes

- **`@Catch()` catching everything and swallowing it** (no log, no response, hung request).
- **Registering via `useGlobalFilters(new X())` when `X` injects services.** Use `APP_FILTER`.
- **Exposing internal messages or stack traces** for unknown errors.
- **Forgetting that `@Catch(Error)` also catches `HttpException`** (it extends `Error`). Be intentional about what matches.
- **Using `res.status().json()` directly in a Fastify app.** Use `httpAdapter.reply()` for portability.
- **Duplicating error shaping** in filters and interceptors. Pick filters for errors, interceptors for success bodies.

## Debugging

- Filter never called? Check the `@Catch` type matches what's thrown (log `exception.constructor.name`), and that the exception actually originates in the request pipeline.
- Wrong filter wins? A more specific scope (route/controller) takes precedence over global.
- Getting the default body despite your filter? It's probably registered but the thrown class isn't covered by `@Catch(...)`.
- Request hangs after an error? Your filter didn't send a response.

## Quick Summary

- Filters turn thrown exceptions into responses; `@Catch(Type)` selects what they handle, `@Catch()` handles everything.
- Bind at route, controller, or global scope; use `APP_FILTER` when DI is needed.
- Use `HttpAdapterHost` + `httpAdapter.reply()` for platform-independent catch-alls.
- Map domain/ORM errors here so services stay HTTP-agnostic.
- Filters cover the request pipeline only, not background jobs or bootstrap.

## Next

[Execution context →](./09-execution-context.md)
