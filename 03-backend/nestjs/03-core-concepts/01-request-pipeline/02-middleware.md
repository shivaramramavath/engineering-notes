# Middleware

Middleware is a function that runs **before the route handler** and has access to the raw `req`, `res`, and `next()`. It's the same concept as Express middleware, with Nest-style registration and optional dependency injection.

Use it for things that don't depend on *which handler* will run: request IDs, raw-body handling, simple header manipulation, legacy Express/Connect middleware (helmet, cors, compression, morgan).

> Middleware does **not** know the target controller or handler. For allow/deny decisions that depend on handler metadata, use a [guard](./04-guards.md).

## Two ways to write it

### Functional middleware

Best when you have no dependencies.

```ts
// request-id.middleware.ts
import { randomUUID } from 'node:crypto';
import { Request, Response, NextFunction } from 'express';

export function requestId(req: Request, res: Response, next: NextFunction) {
  const id = (req.headers['x-request-id'] as string) ?? randomUUID();
  req.headers['x-request-id'] = id;
  res.setHeader('x-request-id', id);
  next();
}
```

### Class middleware

Use when you need injected providers. It's a normal provider-like class, so DI works.

```ts
// logger.middleware.ts
import { Injectable, Logger, NestMiddleware } from '@nestjs/common';
import { Request, Response, NextFunction } from 'express';

@Injectable()
export class LoggerMiddleware implements NestMiddleware {
  private readonly logger = new Logger('HTTP');

  use(req: Request, res: Response, next: NextFunction) {
    const start = Date.now();
    res.on('finish', () => {
      this.logger.log(`${req.method} ${req.originalUrl} ${res.statusCode} ${Date.now() - start}ms`);
    });
    next();
  }
}
```

Note the `res.on('finish')`: middleware runs *before* the handler, so to log status code or duration you hook into the response lifecycle. (If you only need the handler's result, an [interceptor](./06-interceptor-recipes.md) is simpler.)

## Binding module-level middleware

Middleware isn't registered in `@Module({...})`. The module implements `NestModule` and uses `configure()`.

```ts
import { MiddlewareConsumer, Module, NestModule, RequestMethod } from '@nestjs/common';

@Module({ controllers: [CatsController] })
export class CatsModule implements NestModule {
  configure(consumer: MiddlewareConsumer) {
    consumer
      .apply(requestId, LoggerMiddleware)          // runs in this order
      .exclude({ path: 'cats/health', method: RequestMethod.GET })
      .forRoutes(CatsController);                   // or a path string, or { path, method }
  }
}
```

`forRoutes()` accepts:

```ts
.forRoutes('cats')                                   // path string
.forRoutes({ path: 'cats', method: RequestMethod.GET })
.forRoutes(CatsController)                           // all routes of a controller
.forRoutes(CatsController, DogsController)
```

Multiple middleware passed to `apply()` run in the order listed.

## Binding globally

```ts
// main.ts
app.use(requestId);   // functional only; class middleware needs DI so use a module
```

`app.use()` runs for every request, including those matching no route. It's also where you plug Express-ecosystem middleware:

```ts
import helmet from 'helmet';
app.use(helmet());
```

If you need a **class** middleware (with DI) applied everywhere, bind it in the root module: `consumer.apply(LoggerMiddleware).forRoutes('*')`. See the wildcard note below.

## Route matching details

- Paths are matched against the route path **including the global prefix** set with `app.setGlobalPrefix()`. Nest applies the prefix to routes bound with `forRoutes`; if you use `exclude` or bind by path, verify the actual URL in your logs when behavior surprises you.
- Binding by controller class (`forRoutes(CatsController)`) is the least error-prone: it follows the controller's own path.
- **NestJS 11 (Express 5):** wildcard route syntax changed. Bare `*` is no longer valid in Express 5 path patterns; wildcards must be named, e.g. `'*splat'`, and `'{*splat}'` also matches the root path. Nest attempts to convert legacy patterns and logs a warning, but update them. Check the official migration guide for exact rules for your platform.
- With **Fastify**, `req` and `res` are the raw Node objects inside middleware (not Fastify's request/reply wrappers). Type them accordingly.

## Practical usage

### Raw body for webhook signature verification

Stripe-style webhooks sign the *raw* bytes. Nest's default body parser consumes the stream first.

```ts
// main.ts
const app = await NestFactory.create<NestExpressApplication>(AppModule, {
  rawBody: true,
});
```

```ts
// controller
@Post('webhook')
handle(@Req() req: RawBodyRequest<Request>) {
  const raw = req.rawBody; // Buffer, available because rawBody: true
}
```

If you need full control, use `{ bodyParser: false }` in `NestFactory.create` and register your own parsers with `app.use(...)`.

### Third-party middleware

```ts
configure(consumer: MiddlewareConsumer) {
  consumer.apply(cookieParser()).forRoutes('*');   // see wildcard note for v11
}
```

### Short-circuiting

Middleware can end the request itself by responding and **not** calling `next()`:

```ts
export function maintenance(req: Request, res: Response, next: NextFunction) {
  if (process.env.MAINTENANCE === '1') return res.status(503).send('Down for maintenance');
  next();
}
```

Responses sent this way bypass guards, interceptors, and filters entirely.

## Common mistakes

- **Forgetting `next()`.** The request hangs with no error. Always call it, or send a response.
- **Putting authorization here.** No handler metadata is available, so you end up matching URLs by string. Use a guard.
- **Expecting `@Injectable()` middleware to work when registered via `app.use(new X())`.** `app.use` takes an instance with no DI. Bind class middleware in a module.
- **Throwing `HttpException` in an Express callback chain you wrote yourself** (e.g., async code after `next()`): errors outside Nest's wrapped call won't reach filters. Keep throws in the synchronous `use()` path or pass them with `next(err)`.
- **Assuming it runs for unmatched routes.** Module-bound middleware only runs for the routes it's bound to; only `app.use` runs on everything.

## Debugging

- Not running at all? Log inside `use()`, then check `forRoutes` path, `exclude` rules, global prefix, and HTTP method.
- Runs twice? You probably bound it both globally and in a module.
- Request hangs? Missing `next()` or an unhandled promise rejection in async middleware.
- Prefer a quick sanity check with `curl -i` to see headers your middleware sets.

## Quick Summary

- Middleware = raw `req/res/next`, runs first, no knowledge of the handler.
- Functional for stateless logic; class (`@Injectable`) when you need DI.
- Bind per module via `configure(consumer)`; globally via `app.use()` (no DI).
- Use `res.on('finish')` for post-response work like timing.
- Authorization belongs in guards; response mapping belongs in interceptors.
- v11/Express 5: use named wildcards in `forRoutes`.

## Next

[Pipes →](./03-pipes.md)
