# Express Integration

This lesson wires everything from the module into a real Express app: a composition root, routes that use services (not Redis), a Redis-backed rate limiter, health and readiness endpoints, error mapping, and a clean shutdown.

The examples target **Express 5**, which forwards rejected promises from async handlers to the error middleware. On **Express 4**, wrap handlers in a small helper (shown below).

## The shape of the app

```
main.ts                 start, signals, shutdown
  └─ buildContainer()   connections, Redis service, repositories, services   (lesson 02)
       └─ createApp(deps)
            ├─ middleware: json, timing, rate limit
            ├─ routes: /healthz, /readyz, /api/users
            ├─ notFound
            └─ errorHandler
```

`createApp` takes **only what it needs** (an interface, not the whole container), so tests can build the app with fakes and no Redis.

## Dependencies the app needs

```ts
// src/http/deps.ts
import type { UserService } from "../services/user.service.js";

export interface RateLimiterPort {
  check(id: string): Promise<{ allowed: boolean; retryAfterSec: number }>;
}

export interface AppDeps {
  userService: UserService;
  limiter: RateLimiterPort;
  health: () => Promise<Record<string, boolean>>;
  isShuttingDown: () => boolean;
}
```

## A Redis-backed rate limiter

Uses the atomic `rateLimit` Lua command from [Lua Scripts](../06_advanced-commands/03_lua-scripts.md) and the typed registration from [lesson 04](./04_typed-redis-client.md):

```ts
// src/redis/rate-limiter.ts
import type { Redis } from "ioredis";
import { keys } from "./keys.js";
import type { RateLimiterPort } from "../http/deps.js";

export class RedisRateLimiter implements RateLimiterPort {
  constructor(
    private redis: Redis,
    private limit: number,
    private windowSec: number,
    private failOpen = true,
    private log: { warn: (o: unknown, m?: string) => void } = console
  ) {}

  async check(id: string) {
    try {
      const [allowed, ttl] = await this.redis.rateLimit(keys.rateLimit("ip", id), this.limit, this.windowSec);
      return { allowed: allowed === 1, retryAfterSec: Math.max(ttl, 1) };
    } catch (err) {
      this.log.warn({ err }, "rate limiter unavailable");
      return { allowed: this.failOpen, retryAfterSec: 1 };     // a deliberate policy decision
    }
  }
}
```

Choose `failOpen` per route group: **open** for a general API (availability first), **closed** for abuse-prone endpoints like login or OTP. The full algorithms are in `12_rate-limiting`.

```ts
// src/http/middleware/rate-limit.ts
import type { RequestHandler } from "express";
import type { RateLimiterPort } from "../deps.js";

export const rateLimit = (limiter: RateLimiterPort): RequestHandler => async (req, res, next) => {
  const { allowed, retryAfterSec } = await limiter.check(req.ip ?? "unknown");
  if (allowed) return next();
  res.set("Retry-After", String(retryAfterSec)).status(429).json({ error: "too_many_requests" });
};
```

Behind a load balancer, set `app.set("trust proxy", <hops or subnet>)` so `req.ip` is the real client and not the proxy, otherwise **all users share one bucket**.

## Routes that talk to services, not to Redis

```ts
// src/http/routes/users.ts
import { Router } from "express";
import { z } from "zod";
import type { AppDeps } from "../deps.js";

const CreateUser = z.object({ email: z.string().email(), name: z.string().min(1) });
const UpdateUser = z.object({ name: z.string().min(1).optional(), active: z.boolean().optional() });

export function usersRouter({ userService }: AppDeps) {
  const r = Router();

  r.get("/:id", async (req, res) => {
    const user = await userService.getProfile(req.params.id);
    if (!user) return res.status(404).json({ error: "not_found" });

    res.set("ETag", `"${user.version}"`);
    res.json(user);
  });

  r.post("/", async (req, res) => {
    const input = CreateUser.parse(req.body);                     // ZodError → 400 in the error handler
    const user = await userService.register(input);               // ConflictError → 409
    res.status(201).location(`/api/users/${user.id}`).json(user);
  });

  r.patch("/:id", async (req, res) => {
    const changes = UpdateUser.parse(req.body);
    const ifMatch = req.header("If-Match");
    const expectedVersion = ifMatch ? Number(ifMatch.replaceAll('"', "")) : undefined;

    const user = await userService.update(req.params.id, changes, expectedVersion);   // VersionConflictError → 412
    if (!user) return res.status(404).json({ error: "not_found" });

    res.set("ETag", `"${user.version}"`);
    res.json(user);
  });

  return r;
}
```

The `ETag` plus `If-Match` pair exposes the repository's **optimistic concurrency** ([version checks](./03_redis-repository.md)) to API clients: two editors can't silently overwrite each other.

On Express 4, wrap async handlers:

```ts
export const ah = (fn: RequestHandler): RequestHandler => (req, res, next) =>
  Promise.resolve(fn(req, res, next)).catch(next);

r.get("/:id", ah(async (req, res) => { /* ... */ }));
```

## Health and readiness

Two different questions:

| Endpoint | Question | Should it check Redis? |
|----------|----------|------------------------|
| `/healthz` (liveness) | Is the process alive? | **No** |
| `/readyz` (readiness) | Should I receive traffic now? | **Yes, if Redis is required** |

```ts
app.get("/healthz", (_req, res) => res.json({ status: "ok" }));

app.get("/readyz", async (_req, res) => {
  if (deps.isShuttingDown()) return res.status(503).json({ status: "shutting_down" });

  const checks = await deps.health();                      // { app: true, events: true }
  const ok = Object.values(checks).every(Boolean);
  res.status(ok ? 200 : 503).json({ status: ok ? "ready" : "degraded", checks });
});
```

Why liveness must **not** depend on Redis: if Redis has a blip, the orchestrator would restart every app instance at once, turning a small problem into an outage. Readiness only removes the instance from the load balancer until Redis returns.

If Redis is **optional** (pure cache), leave it out of readiness or report it as informational.

## Error handling

Turn domain and Redis errors into sensible HTTP responses in one place:

```ts
// src/http/middleware/error-handler.ts
import type { ErrorRequestHandler } from "express";
import { ZodError } from "zod";
import { ConflictError, VersionConflictError } from "../../repositories/user.repository.js";

const isRedisUnavailable = (e: any) =>
  e?.name === "MaxRetriesPerRequestError" ||
  /Command timed out|Connection is closed|Stream isn't writeable/i.test(e?.message ?? "") ||
  /^(LOADING|BUSY|OOM|CLUSTERDOWN|READONLY)/.test(e?.message ?? "");

export const errorHandler: ErrorRequestHandler = (err, req, res, _next) => {
  if (err instanceof ZodError)
    return res.status(400).json({ error: "validation_failed", issues: err.issues });

  if (err instanceof ConflictError)
    return res.status(409).json({ error: "conflict", message: err.message });

  if (err instanceof VersionConflictError)
    return res.status(412).json({ error: "version_conflict" });

  if (isRedisUnavailable(err)) {
    req.log?.error({ err }, "redis unavailable");
    return res.set("Retry-After", "5").status(503).json({ error: "service_unavailable" });
  }

  req.log?.error({ err }, "unhandled error");               // NOAUTH, WRONGTYPE ... are bugs or config problems
  res.status(500).json({ error: "internal_error" });        // never leak internals to clients
};
```

| Situation | Response |
|-----------|----------|
| Validation failed | `400` |
| Unique constraint (email taken) | `409` |
| Version mismatch (`If-Match`) | `412` |
| Redis down, timing out, loading, OOM | `503` + `Retry-After` |
| `NOAUTH`, `WRONGPASS`, `WRONGTYPE` | `500` and **alert** (config or code bug) |

Matching on message text is pragmatic but brittle. Centralize it in this one function so a change is a one-line fix.

## Assembling the app

```ts
// src/http/app.ts
import express from "express";
import type { AppDeps } from "./deps.js";
import { rateLimit } from "./middleware/rate-limit.js";
import { errorHandler } from "./middleware/error-handler.js";
import { usersRouter } from "./routes/users.js";

export function createApp(deps: AppDeps) {
  const app = express();
  app.disable("x-powered-by");
  app.set("trust proxy", 1);                                   // adjust to your proxy setup
  app.use(express.json({ limit: "100kb" }));

  app.get("/healthz", (_req, res) => res.json({ status: "ok" }));
  app.get("/readyz", async (_req, res) => { /* as above */ });

  app.use("/api", rateLimit(deps.limiter));
  app.use("/api/users", usersRouter(deps));

  app.use((_req, res) => res.status(404).json({ error: "not_found" }));
  app.use(errorHandler);                                        // must be last
  return app;
}
```

### Caching a route

The middleware from [Caching API Responses](../07_caching/05_caching_api-responses.md) takes the client as a parameter instead of importing a global:

```ts
app.get(
  "/api/products",
  cacheResponse(redis, { ttl: 60, vary: ["Accept-Language"], tags: () => ["products"] }),
  listProducts
);
```

Place it **after** authentication so `req.user` exists for `private: true` routes. Sessions (`express-session` with a Redis store) are covered in `13_session-management`.

## `main.ts`: start and shut down cleanly

```ts
// src/main.ts
import { buildContainer } from "./container.js";
import { createApp } from "./http/app.js";
import { RedisRateLimiter } from "./redis/rate-limiter.js";

const container = buildContainer();
let shuttingDown = false;

// Required Redis: fail fast at boot
await container.redis.ping();

const app = createApp({
  userService: container.userService,
  limiter: new RedisRateLimiter(container.redis, 100, 60),
  health: container.health,
  isShuttingDown: () => shuttingDown,
});

const server = app.listen(Number(process.env.PORT ?? 3000), () => console.log("listening"));

async function shutdown(signal: string) {
  if (shuttingDown) return;
  shuttingDown = true;                                          // readiness now returns 503
  console.log(`${signal}: shutting down`);

  const force = setTimeout(() => process.exit(1), 10_000);     // keep below the platform grace period
  force.unref();

  try {
    await new Promise<void>((resolve, reject) => server.close((e) => (e ? reject(e) : resolve())));
    server.closeIdleConnections?.();                            // don't wait on idle keep-alive sockets
    await container.close();                                    // subscribers, blocking, then commands
    process.exit(0);
  } catch (err) {
    console.error(err);
    process.exit(1);
  }
}

process.on("SIGTERM", () => shutdown("SIGTERM"));
process.on("SIGINT", () => shutdown("SIGINT"));
```

Order matters: flip readiness, stop accepting connections, let in-flight requests finish, **then** close Redis. Give the load balancer a moment to notice the failing readiness check before you stop listening (a short delay of a few seconds is common). Details in [Graceful Shutdown](../03_ioredis-basics/06_graceful-shutdown.md).

## Testing the HTTP layer

Build the app with fakes, so tests are fast and need no Redis:

```ts
import request from "supertest";
import { createApp } from "../src/http/app.js";

const allowAll = { check: async () => ({ allowed: true, retryAfterSec: 0 }) };

function makeApp(overrides: Partial<AppDeps> = {}) {
  return createApp({
    userService: new UserService(new InMemoryUserRepository(), new InMemoryCache()),
    limiter: allowAll,
    health: async () => ({ app: true }),
    isShuttingDown: () => false,
    ...overrides,
  });
}

it("returns 429 with Retry-After when limited", async () => {
  const app = makeApp({ limiter: { check: async () => ({ allowed: false, retryAfterSec: 7 }) } });
  const res = await request(app).get("/api/users/1");
  expect(res.status).toBe(429);
  expect(res.headers["retry-after"]).toBe("7");
});

it("maps a duplicate email to 409", async () => {
  const app = makeApp();
  const body = { email: "a@x.com", name: "A" };
  await request(app).post("/api/users").send(body).expect(201);
  await request(app).post("/api/users").send(body).expect(409);
});

it("readiness fails while shutting down", async () => {
  const res = await request(makeApp({ isShuttingDown: () => true })).get("/readyz");
  expect(res.status).toBe(503);
});
```

Keep a **small set of integration tests** against a real Redis (Testcontainers) for the rate limiter script and repository, where atomicity and TTL behavior matter (`18_testing-and-debugging`).

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Routes importing a global Redis client | Inject services through `createApp(deps)` |
| Liveness probe that pings Redis | Liveness is process-only, readiness checks dependencies |
| `req.ip` is the proxy's address | Set `trust proxy` correctly |
| Rate limiter failing the same way everywhere | Choose fail-open or fail-closed per route group |
| Redis errors becoming `500`s | Map them to `503` with `Retry-After` |
| Leaking error details to clients | Generic message, detailed server log |
| Error middleware registered before routes | It must be last |
| Closing Redis before the HTTP server drains | Stop listening, drain, then close Redis |
| Async handlers on Express 4 without a wrapper | `ah()` helper, or upgrade to Express 5 |
| Cache middleware placed before auth | Put it after, so per-user keys work |

## Key takeaways

- Build objects once in a composition root and pass **narrow interfaces** into `createApp`
- Routes call services, services call ports, and only the edges know about Redis
- Liveness is cheap and dependency-free, readiness reflects whether you can serve traffic
- Map domain and Redis errors to HTTP in one place, and shut down in order

**Next module:** [09_pub-sub](../09_pub-sub/README.md)
