# Middleware

Middleware is how a web framework handles everything that is not specific to one route: parsing bodies, logging, authentication, validation, rate limiting, error formatting. Each piece is a small function that sees the request, can change it or end it, and passes control along. The pattern is simple, but the details (ordering, async errors, passing typed data downstream) are where bugs hide. This note uses Express, and the ideas carry over to Koa, Fastify hooks, Hono, and NestJS.

**Prerequisites:**
- [Express](./02-express.md)
- [Higher-order functions and closures](../02-functions/00-function-types.md)
- [Custom errors](../11-error-handling/01-custom-errors.md)

---

## What middleware is

A middleware function receives the request, the response, and a `next` function:

```ts
import type { Request, Response, NextFunction } from "express";

function logRequests(req: Request, res: Response, next: NextFunction) {
  const start = Date.now();
  res.on("finish", () => {
    console.log(`${req.method} ${req.originalUrl} ${res.statusCode} ${Date.now() - start}ms`);
  });
  next();                                  // hand control to the next middleware
}

app.use(logRequests);
```

It can do one of four things:

1. **Do some work and call `next()`** (logging, parsing).
2. **Change `req` or `res`, then call `next()`** (attach a user, set a header).
3. **End the request** by sending a response (reject an unauthorized call). Do **not** call `next()` afterwards.
4. **Pass an error** with `next(err)`, which skips ahead to error-handling middleware.

## The pipeline and ordering

Middleware run **in the order they are registered**, and a request flows through every matching one until something responds:

```text
request
  |
  v
[ request id ] -> [ logger ] -> [ helmet / cors ] -> [ body parser ]
  |
  v
[ rate limit ] -> [ authenticate ] -> [ validate ] -> [ route handler ]
  |                                                        |
  |<------------- error? next(err) -----------------------+
  v
[ 404 handler ] -> [ error handler ] -> response
```

Order is behavior:

- The **body parser** must come before anything that reads `req.body`.
- **Authentication** comes before routes that require a user.
- **Rate limiting** usually comes early, so it protects the expensive parts.
- The **404 handler** goes after all routes, and the **error handler** is last.
- A middleware mounted with a path (`app.use("/admin", requireAdmin)`) only runs for that prefix.

Scope middleware to the routes that need it:

```ts
router.get("/me", requireAuth, getProfile);                 // per route
adminRouter.use(requireAuth, requireRole("admin"));         // per router
app.use("/api", apiRouter);
```

## Writing typed middleware

### Factories: configurable middleware

Most useful middleware is created by a function that takes options and returns the middleware (a closure):

```ts
import type { RequestHandler } from "express";

export function requireRole(...roles: Role[]): RequestHandler {
  return (req, res, next) => {
    const user = req.user;
    if (!user) return res.status(401).json(unauthorized());
    if (!roles.includes(user.role)) return res.status(403).json(forbidden());
    next();
  };
}

router.delete("/:id", requireAuth, requireRole("admin"), deleteUser);
```

`Role` is a union type, so `requireRole("superadmin")` fails to compile. The factory pattern lets you pass dependencies in too ([dependency injection](../17-design-patterns/05-dependency-injection.md)):

```ts
export const authenticate = (tokens: TokenService): RequestHandler => async (req, res, next) => {
  const header = req.headers.authorization;
  if (!header?.startsWith("Bearer ")) return res.status(401).json(unauthorized());
  try {
    req.user = await tokens.verify(header.slice("Bearer ".length));
    next();
  } catch {
    res.status(401).json(unauthorized());
  }
};
```

### Passing typed data downstream

Middleware often produces something later code needs. Options:

**1. Augment `Request`** (simple, global). Declare the property once and keep it optional, since it exists only after the middleware ran:

```ts
declare global {
  namespace Express {
    interface Request { user?: AuthUser; requestId?: string }
  }
}
```

Handlers then check it (`if (!req.user) ...`). To avoid repeating checks, write a small accessor:

```ts
export function requireUser(req: Request): AuthUser {
  if (!req.user) throw new UnauthorizedError("Authentication required");
  return req.user;
}
```

**2. Validated input on `res.locals`** (explicit and scoped). A validation middleware stores the parsed result, and the handler reads it through a typed helper. In Express 5, `req.query` is read-only, so do not try to overwrite it with parsed data.

**3. Parse inside the handler** with a shared helper. For many APIs this is the simplest and most type-safe: no hidden coupling between middleware and handler.

```ts
const input = parseOrThrow(CreatePostSchema, req.body);   // returns the typed value, throws a ValidationError
```

### A validation middleware

```ts
import type { RequestHandler } from "express";
import { z } from "zod";

type Source = "body" | "query" | "params";

export const validate =
  <S extends z.ZodType>(source: Source, schema: S): RequestHandler =>
  (req, res, next) => {
    const result = schema.safeParse(req[source]);
    if (!result.success) return next(new ValidationError(result.error.issues));
    res.locals[source] = result.data;
    next();
  };
```

It hands failures to the central error handler (`next(err)`), so the response format lives in one place. See [validation recipes](../15-runtime-validation/04-validation-recipes.md).

## Async middleware and errors

Async middleware have the same error rules as async handlers:

- **Express 5:** a rejection or thrown error is forwarded to the error handler.
- **Express 4:** you must catch and call `next(err)` yourself, or the request hangs.

```ts
export const loadProject: RequestHandler = async (req, res, next) => {
  try {
    res.locals.project = await projects.getById(req.params.projectId);
    next();
  } catch (err) {
    next(err);                              // explicit, works in both versions
  }
};
```

Calling `next(err)` explicitly is always safe, and it makes the flow clear.

## Error-handling middleware

Four parameters, registered last. It turns errors into responses and logs unexpected ones:

```ts
import type { ErrorRequestHandler } from "express";

export const errorHandler: ErrorRequestHandler = (err: unknown, req, res, _next) => {
  const requestId = req.requestId;

  if (err instanceof AppError) {
    return res.status(statusFor[err.code]).json({ error: { code: err.code, message: err.message, requestId } });
  }

  logger.error({ err, requestId }, "Unhandled error");
  res.status(500).json({ error: { code: "INTERNAL", message: "Internal server error", requestId } });
};
```

If headers were already sent, delegate to Express's default handler (`if (res.headersSent) return next(err)`) rather than trying to write another response. See [error response types](../16-type-safe-apis/04-error-response-types.md) and [error handling strategies](../11-error-handling/03-error-handling-strategies.md).

## Common middleware

| Middleware | Purpose |
|---|---|
| **Request id** | a unique id per request, echoed in logs and responses |
| **Logger** | structured access logs (`pino-http` and similar) |
| **`helmet`** | secure HTTP headers |
| **`cors`** | cross-origin policy with an explicit allow-list |
| **Body parsers** | `express.json`, `express.urlencoded`, with size limits |
| **Rate limiter** | throttle by IP or user, stricter on login |
| **Authentication** | identify the caller, attach `req.user` |
| **Authorization** | check roles and permissions |
| **Validation** | parse and check input |
| **Compression, caching headers** | often better at the proxy or CDN |
| **Timeout** | bound how long a request may run |

### A request id with `AsyncLocalStorage`

A request id is valuable only if every log line carries it. `AsyncLocalStorage` lets code far from the middleware read the id without passing it through every function ([Node.js types](./00-node-types.md)):

```ts
import { AsyncLocalStorage } from "node:async_hooks";
import { randomUUID } from "node:crypto";

export const requestContext = new AsyncLocalStorage<{ requestId: string }>();

export const requestId: RequestHandler = (req, res, next) => {
  const id = req.header("x-request-id") ?? randomUUID();
  req.requestId = id;
  res.setHeader("x-request-id", id);
  requestContext.run({ requestId: id }, next);        // everything downstream runs inside this context
};

export function currentRequestId(): string | undefined {
  return requestContext.getStore()?.requestId;
}
```

A logger can then add `currentRequestId()` to every entry ([logging and observability](../21-production-tooling/06-logging-and-observability.md)). Only trust an incoming `x-request-id` if you control the upstream, and validate its format, since it ends up in your logs.

## Testing middleware

Test a middleware **in isolation** with fake `req`, `res`, and `next`, or through the whole app with HTTP:

```ts
it("rejects a missing token", async () => {
  const req = { headers: {} } as Request;
  const res = { status: vi.fn().mockReturnThis(), json: vi.fn() } as unknown as Response;
  const next = vi.fn();

  await authenticate(fakeTokens)(req, res, next);

  expect(res.status).toHaveBeenCalledWith(401);
  expect(next).not.toHaveBeenCalled();
});
```

Hand-built fake `req`/`res` objects need casts, which is acceptable in tests but brittle. Testing through the app with `supertest` exercises real ordering and error handling, which is where middleware bugs live ([integration testing](../18-testing-and-debugging/01-integration-testing.md), [mocking](../18-testing-and-debugging/02-mocking.md)).

## Important rules and misconceptions

- **Middleware order is part of your application's behavior.** Reordering can break auth or error handling.
- **Always either respond or call `next()`, never both.** Doing neither leaves the request hanging.
- **Calling `next()` after `res.json()`** continues the chain and can cause a second response.
- **`next(err)` skips to error handlers,** bypassing remaining normal middleware.
- **Types on `req.user` are a claim** that depends on the middleware having run.
- **Mounting path matters:** `app.use("/api", mw)` runs only for requests under `/api`.

## Common mistakes

- Forgetting `next()`, so requests hang.
- Calling `next()` after sending a response ("headers already sent").
- Registering the error handler before routes, or omitting the fourth parameter so Express does not treat it as an error handler.
- Putting authentication after the routes it should protect.
- Putting the body parser after a route that reads the body.
- Using Express 4 with async middleware and no `try/catch`.
- Mutating `req.query` in Express 5.
- Reading `req.user` without handling `undefined`.
- Swallowing errors with an empty `catch`.
- Trusting client-provided headers (`x-request-id`, `x-forwarded-for`) without a trusted proxy configuration.
- Doing heavy synchronous work inside middleware, blocking the event loop.

## Debugging

- Log entry and exit of each middleware (temporarily) to see the real order and where a request stops.
- If a route behaves as if auth is missing, list the registered stack in order and check mount paths.
- If a request never completes, find the middleware that neither calls `next()` nor responds.
- If you see "headers already sent", find the middleware or handler that responds twice, and look for a missing `return`.
- Use `app._router.stack` (Express 4) or a debugging helper library to print the middleware order, for inspection only.
- Reproduce with `curl -i`, including the headers the middleware depends on.

## Quick summary

- Middleware are functions of `(req, res, next)` that run in registration order. Each continues the chain, ends it with a response, or fails with `next(err)`.
- Order is behavior: body parsing before reading bodies, auth before protected routes, 404 then the error handler last.
- Write middleware as typed factories (`requireRole("admin")`), and inject dependencies through the factory.
- Pass data downstream via augmented `Request` fields (optional), `res.locals`, or by parsing in the handler. Do not overwrite read-only request fields.
- Centralize error formatting in one error-handling middleware, and make async failures reach it explicitly (`next(err)`) or via Express 5.
- Use `AsyncLocalStorage` for request-scoped context such as request ids, and test through the whole app where ordering matters.

**Next:** [Service and repository layers](./04-service-and-repository-layers.md)
