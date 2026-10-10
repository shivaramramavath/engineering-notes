# Express

Express is the most common Node.js web framework: a thin layer of routing and middleware over Node's `http` module. It is small, flexible, and unopinionated, which makes it a good place to see how a TypeScript backend is put together. Its types (`@types/express`) are helpful but loose in places, so the main skill is knowing what they check, what they only describe, and how to structure an app so it stays testable.

> **Version note.** Express 5 is the current major version and differs from 4 in important ways, notably that rejected promises from async handlers are forwarded to the error middleware automatically, and `req.query` is read-only. Examples work with both unless noted. Check the version and the matching `@types/express`.

**Prerequisites:**
- [Node.js types](./00-node-types.md)
- [Request and response types](../16-type-safe-apis/01-request-response-types.md)
- [Config and environment](./01-config-and-environment.md)

---

## Setup

```bash
npm install express
npm install --save-dev @types/express
```

A minimal typed app:

```ts
import express from "express";

const app = express();
app.use(express.json());

app.get("/health", (req, res) => {
  res.json({ status: "ok" });
});

app.listen(3000, () => console.log("listening on 3000"));
```

`req` and `res` are typed by contextual typing from the route registration, so inline handlers need no annotations.

## Build the app from a function

Do not create the app and start listening in the same place. Export a factory that takes its dependencies, and start the server in a separate entry file. Tests can then create the app without opening a port ([integration testing](../18-testing-and-debugging/01-integration-testing.md), [dependency injection](../17-design-patterns/05-dependency-injection.md)):

```ts
// src/app.ts
import express, { type Express } from "express";

export interface AppDeps {
  userService: UserService;
}

export function createApp({ userService }: AppDeps): Express {
  const app = express();

  app.use(express.json({ limit: "100kb" }));
  app.use("/users", createUserRouter(userService));
  app.use(notFoundHandler);
  app.use(errorHandler);

  return app;
}
```

```ts
// src/server.ts
const app = createApp(buildDependencies(config));
const server = app.listen(config.http.port);
```

## Typing handlers

Express's `Request` and `Response` are generic over the route's parts:

```ts
Request<Params, ResBody, ReqBody, ReqQuery>
Response<ResBody>
```

```ts
import type { Request, Response, RequestHandler } from "express";

type GetUserParams = { id: string };
type CreateUserBody = { email: string; name: string };

const getUser: RequestHandler<GetUserParams, UserDto | ErrorBody> = async (req, res) => {
  const user = await users.find(req.params.id);                  // req.params.id: string
  if (!user) return res.status(404).json({ error: { code: "NOT_FOUND", message: "User not found" } });
  res.json(toUserDto(user));                                    // checked against UserDto | ErrorBody
};

const createUser: RequestHandler<{}, UserDto, CreateUserBody> = async (req, res) => {
  const { email, name } = req.body;                             // typed, but NOT validated
  // ...
};
```

**These generics are claims, not checks.** `req.body` typed as `CreateUserBody` is only true if something validated it. Express parses JSON and hands you whatever the client sent. The correct flow: treat `req.body` as `unknown`, validate with a schema, and use the parsed result ([trust boundaries](../15-runtime-validation/00-trust-boundaries.md), [validation recipes](../15-runtime-validation/04-validation-recipes.md)):

```ts
const CreateUserSchema = z.object({ email: z.string().email(), name: z.string().min(1) });

app.post("/users", async (req, res) => {
  const parsed = CreateUserSchema.safeParse(req.body);
  if (!parsed.success) {
    return res.status(400).json({ error: { code: "VALIDATION", issues: toIssues(parsed.error) } });
  }
  const user = await userService.register(parsed.data);
  res.status(201).json(toUserDto(user));
});
```

The response generic **does** check what you send (`res.json` arguments), which is useful for keeping handlers aligned with the contract.

Path parameters are always `string`, query values are `string | string[] | ParsedQs | ...` (arrays and nested objects are possible), and headers are `string | string[] | undefined`. Validate and convert them like any other input.

## Routers: organizing routes

A `Router` groups routes, usually one per resource:

```ts
// src/users/user.routes.ts
import { Router } from "express";

export function createUserRouter(service: UserService): Router {
  const router = Router();

  router.get("/:id", async (req, res) => {
    const user = await service.getById(req.params.id);
    res.json(toUserDto(user));
  });

  router.post("/", async (req, res) => {
    const input = CreateUserSchema.parse(req.body);          // throws ZodError, handled by the error middleware
    const user = await service.register(input);
    res.status(201).json(toUserDto(user));
  });

  return router;
}
```

Taking the service as a parameter keeps routes testable and free of global imports. Keep handlers **thin**: parse input, call a service, map the result to a DTO and a status code. Business logic belongs in services ([service and repository layers](./04-service-and-repository-layers.md)).

## Async handlers and errors

Handlers are often `async`, and the framework needs to see their failures:

- **Express 5:** a rejected promise (or a thrown error) from an async handler is passed to the error-handling middleware automatically.
- **Express 4:** a rejection from an async handler is **not** caught. The request hangs and the process may report an unhandled rejection. Wrap handlers, or catch and call `next(err)`:

```ts
import type { Request, Response, NextFunction, RequestHandler } from "express";

const asyncHandler =
  <P, ResB, ReqB, Q>(fn: (req: Request<P, ResB, ReqB, Q>, res: Response<ResB>, next: NextFunction) => Promise<unknown>): RequestHandler<P, ResB, ReqB, Q> =>
  (req, res, next) => {
    fn(req, res, next).catch(next);
  };

router.get("/:id", asyncHandler(async (req, res) => { /* may throw */ }));
```

If you are on Express 4 and need this behavior, the wrapper above (or a small package providing it) is the usual fix. On Express 5 it is built in.

### The error-handling middleware

Four parameters mark a function as an error handler. Register it **last**:

```ts
import type { ErrorRequestHandler } from "express";

export const errorHandler: ErrorRequestHandler = (err: unknown, req, res, _next) => {
  if (err instanceof AppError) {
    return res.status(statusFor[err.code]).json({ error: { code: err.code, message: err.message } });
  }
  if (err instanceof ZodError) {
    return res.status(400).json({ error: { code: "VALIDATION", issues: toIssues(err) } });
  }
  logger.error({ err }, "Unhandled error");
  res.status(500).json({ error: { code: "INTERNAL", message: "Internal server error" } });
};
```

The `err` parameter is `any` in Express's types. Annotate it as `unknown` and narrow, as in any `catch` ([catching and narrowing errors](../11-error-handling/00-catching-and-narrowing-errors.md)). Never send stack traces or internal messages for unexpected errors ([error response types](../16-type-safe-apis/04-error-response-types.md), [error handling strategies](../11-error-handling/03-error-handling-strategies.md)).

Add a 404 handler before the error handler for unmatched routes:

```ts
export const notFoundHandler: RequestHandler = (req, res) => {
  res.status(404).json({ error: { code: "NOT_FOUND", message: `Cannot ${req.method} ${req.path}` } });
};
```

## Extending `Request` and `Response.locals`

Middleware often attaches data (the authenticated user, a request id). Describe it by augmenting Express's types ([global and module augmentation](../09-declaration-files/02-global-and-module-augmentation.md)):

```ts
// src/types/express.d.ts
import type { AuthUser } from "../auth/types";

declare global {
  namespace Express {
    interface Request {
      user?: AuthUser;
      requestId?: string;
    }
  }
}

export {};
```

Now `req.user` is `AuthUser | undefined` everywhere. Keep these optional, since the property exists only after the middleware ran. Make sure the file is in the compilation (`include`), and that tests' tsconfig includes it too. The module-augmentation form (`declare module "express-serve-static-core"`) also works. Pick one style for the project.

For values scoped to one request that handlers should read through a typed accessor, `res.locals` is the alternative, but it is loosely typed (`Record<string, any>` unless augmented). A small helper that validates or narrows is safer than reading it directly. See [middleware](./03-middleware.md).

## Production concerns

| Concern | Do |
|---|---|
| **Security headers** | `helmet()` sets sensible defaults |
| **CORS** | `cors({ origin: config.http.corsOrigins })`: an explicit allow-list, never `*` with credentials |
| **Body size** | set limits on `express.json` and `express.urlencoded` |
| **Rate limiting** | a limiter on login and expensive routes ([authentication](./07-authentication-and-authorization.md)) |
| **Trust proxy** | `app.set("trust proxy", ...)` when behind a load balancer, so `req.ip` and `req.protocol` are right |
| **Disable fingerprinting** | `app.disable("x-powered-by")` (helmet does it) |
| **Compression** | usually done at the proxy or CDN |
| **Logging** | structured request logs with request ids ([logging and observability](../21-production-tooling/06-logging-and-observability.md)) |
| **Health checks** | cheap `/health` (liveness) and a `/ready` that checks dependencies |
| **Graceful shutdown** | stop accepting connections, finish in-flight requests, close pools |

Graceful shutdown:

```ts
const server = app.listen(config.http.port);

function shutdown(signal: string) {
  logger.info({ signal }, "shutting down");
  server.close(async (err) => {
    await pool.end();                         // release database connections
    process.exit(err ? 1 : 0);
  });
  setTimeout(() => process.exit(1), 10_000).unref();    // force exit if requests hang
}

process.on("SIGTERM", () => shutdown("SIGTERM"));
process.on("SIGINT", () => shutdown("SIGINT"));
```

## Express and its alternatives

Express is flexible and has an enormous ecosystem, but gives you little structure and only loose types. Alternatives address that: **Fastify** (schema-first, validates with JSON Schema, strong plugin model and typing), **Hono** (small, multi-runtime, typed routing), and **NestJS** (an opinionated framework, often on top of Express or Fastify, see [NestJS](./08-nestjs.md)). The patterns in this section (thin controllers, services, repositories, validated input, central error handling) apply to all of them.

## Important rules and misconceptions

- **Handler generics describe, they do not validate.** `req.body` is untrusted input.
- **Middleware order is behavior.** The error handler goes last. Body parsers go before routes that read bodies.
- **Express 4 does not catch async rejections.** Express 5 does.
- **`res.json` ends the response.** Code after it still runs. Use `return` when branching.
- **Route order matters:** the first matching route handles the request.
- **`req.query` and `req.params` hold strings.**

## Common mistakes

- Typing `req.body` as a DTO and trusting it without validation.
- Starting the server in the same module that builds the app, making tests start real servers.
- Forgetting `return` after sending a response, and then sending a second one ("headers already sent").
- Putting business logic and SQL in route handlers.
- Registering the error handler before routes, so it never runs for them.
- Using Express 4 with async handlers and no wrapper.
- Sending raw errors or stack traces to clients.
- `cors({ origin: true })` or `*` with credentials.
- No body size limit.
- Not closing the server and pool on shutdown.
- Extending `Request` without making the properties optional, and then reading them before the middleware ran.

## Debugging

- "Cannot set headers after they are sent": a handler responds twice. Add `return`s, and check middleware that also responds.
- If a request hangs, a handler never responded or an async error was swallowed (Express 4). Add the wrapper or upgrade.
- If `req.body` is `undefined`, the body parser is missing or placed after the route, or the content type did not match.
- If `req.user` is `undefined` in a handler, the auth middleware did not run for that route (check order and mount path).
- Log `req.method`, `req.originalUrl`, and the request id on entry and the status on exit.
- If types complain in `RequestHandler` generics, check the order (`Params, ResBody, ReqBody, Query`) and use `{}` for unused slots.

## Quick summary

- Express is a thin router and middleware layer. Build the app from a factory that takes dependencies, and start it in a separate entry file.
- Handler generics type params, body, query, and response, but only the response is checked. Validate `req.body`, `req.query`, and `req.params` with a schema before use.
- Keep handlers thin and group routes in routers. Put logic in services.
- Use a four-argument error middleware registered last, annotate `err` as `unknown`, map known errors to responses, and hide internals for unknown ones.
- Extend `Request` through declaration merging for middleware-provided data, and handle async errors (built in with Express 5, a wrapper in Express 4).
- Add security headers, CORS allow-lists, body limits, rate limits, health checks, and graceful shutdown.

**Next:** [Middleware](./03-middleware.md)
