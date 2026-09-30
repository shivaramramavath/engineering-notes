# Redis Service

Raw ioredis calls scattered through business code create problems: repeated JSON handling, inconsistent error policies, no metrics, and tests that need a real Redis. A **service** solves this: a small class that wraps the client, encodes your conventions, and is **injected** wherever it is needed.

## What belongs in the service

| Belongs | Doesn't belong |
|---------|----------------|
| JSON get/set, TTL and jitter rules | Business rules ("premium users get X") |
| Failure policy (fail open for reads) | Domain-specific key names (use the [key builder](./05_redis-key-builder.md)) |
| Metrics and logging hooks | Entity mapping (use [repositories](./03_redis-repository.md)) |
| Small primitives (`remember`, counters, locks) | HTTP concerns |

Keep it **thin**. If it grows a method per feature, split it.

## Depend on a port, not on ioredis

Define what your application needs as an interface:

```ts
// src/redis/ports.ts
export interface CachePort {
  get<T>(key: string): Promise<T | null>;
  set<T>(key: string, value: T, ttlSec: number): Promise<void>;
  del(...keys: string[]): Promise<void>;
  remember<T>(key: string, ttlSec: number, loader: () => Promise<T | null>): Promise<T | null>;
}

export interface CounterPort {
  hit(key: string, windowSec: number): Promise<number>;
}
```

Business code imports the port. It never imports `ioredis`, so tests can pass a fake.

## The implementation

```ts
// src/redis/service.ts
import type { Redis } from "ioredis";
import type { CachePort, CounterPort } from "./ports.js";

export interface RedisServiceOptions {
  jitter?: number;                                   // fraction, default 0.1
  nullTtlSec?: number;                               // negative-cache TTL, default 30
  onEvent?: (e: { op: string; key: string; ok: boolean; hit?: boolean; ms: number }) => void;
  log?: { warn: (obj: unknown, msg?: string) => void };
}

export class RedisService implements CachePort, CounterPort {
  private jitter: number;
  private nullTtl: number;

  constructor(private redis: Redis, private opts: RedisServiceOptions = {}) {
    this.jitter = opts.jitter ?? 0.1;
    this.nullTtl = opts.nullTtlSec ?? 30;
  }

  // ---- reads fail OPEN: a Redis problem behaves like a miss ----
  async get<T>(key: string): Promise<T | null> {
    const t = performance.now();
    try {
      const raw = await this.redis.get(key);
      this.emit("get", key, true, raw !== null, t);
      return raw === null ? null : (JSON.parse(raw) as T);
    } catch (err) {
      this.opts.log?.warn({ err, key }, "cache get failed");
      this.emit("get", key, false, false, t);
      return null;
    }
  }

  // ---- writes are best effort: never break the caller ----
  async set<T>(key: string, value: T, ttlSec: number): Promise<void> {
    const t = performance.now();
    try {
      await this.redis.set(key, JSON.stringify(value), "EX", this.withJitter(ttlSec));
      this.emit("set", key, true, undefined, t);
    } catch (err) {
      this.opts.log?.warn({ err, key }, "cache set failed");
      this.emit("set", key, false, undefined, t);
    }
  }

  async del(...keys: string[]): Promise<void> {
    if (keys.length === 0) return;
    try {
      await this.redis.unlink(...keys);
    } catch (err) {
      // an undeleted key stays stale until its TTL: log loudly
      this.opts.log?.warn({ err, keys }, "cache invalidation failed");
    }
  }

  /** Cache-aside with negative caching. */
  async remember<T>(key: string, ttlSec: number, loader: () => Promise<T | null>): Promise<T | null> {
    const t = performance.now();
    try {
      const raw = await this.redis.get(key);
      if (raw !== null) {
        this.emit("remember", key, true, true, t);
        return JSON.parse(raw) as T | null;          // "null" = cached "not found"
      }
    } catch (err) {
      this.opts.log?.warn({ err, key }, "cache read failed");
    }

    const value = await loader();
    const ttl = value === null ? this.nullTtl : this.withJitter(ttlSec);
    this.redis.set(key, JSON.stringify(value), "EX", ttl).catch((err) =>
      this.opts.log?.warn({ err, key }, "cache write failed")
    );
    this.emit("remember", key, true, false, t);
    return value;
  }

  // ---- counters: TTL is set once, atomically ----
  async hit(key: string, windowSec: number): Promise<number> {
    const res = await this.redis.multi().incr(key).expire(key, windowSec, "NX").exec();
    const [err, count] = res![0];
    if (err) throw err;
    return count as number;
  }

  // ---- helpers ----
  private withJitter(ttl: number) {
    return Math.max(1, Math.round(ttl * (1 + (Math.random() * 2 - 1) * this.jitter)));
  }

  private emit(op: string, key: string, ok: boolean, hit: boolean | undefined, t: number) {
    this.opts.onEvent?.({ op, key, ok, hit, ms: performance.now() - t });
  }
}
```

Note the deliberate **policy differences**:

| Method | On Redis failure | Why |
|--------|------------------|-----|
| `get`, `remember` reads | Treated as a miss | Availability over freshness |
| `set`, `del` | Logged, swallowed | The caller already has its answer |
| `hit` (counter) | **Throws** | The caller must decide (rate limiters choose fail open or closed) |

If you have a use case where a Redis failure must stop the operation (locks, idempotency), give it **its own class or method that throws**, don't overload the cache methods.

`expire(..., "NX")` needs Redis 7.0+. On older servers, use a Lua script for the counter (see [Lua Scripts](../06_advanced-commands/03_lua-scripts.md)).

## Metrics hook

The service reports events without knowing about your metrics library:

```ts
const service = new RedisService(redis, {
  log: logger,
  onEvent: ({ op, ok, hit, ms }) => {
    metrics.histogram("redis_op_ms", ms, { op });
    if (!ok) metrics.increment("redis_errors", { op });
    if (hit !== undefined) metrics.increment("redis_cache", { op, result: hit ? "hit" : "miss" });
  },
});
```

Track **hit ratio**, **error count** and **latency** per operation. Don't put raw keys into metric labels (unbounded cardinality).

## Composition root

Build everything in one place and pass dependencies down:

```ts
// src/container.ts
import { RedisConnections } from "./redis/connections.js";
import { RedisService } from "./redis/service.js";
import { UserRepository } from "./repositories/user.repository.js";
import { UserService } from "./services/user.service.js";

export function buildContainer() {
  const connections = new RedisConnections();
  const redis = connections.create("app");

  const cache = new RedisService(redis, { log: logger });
  const users = new UserRepository(redis);
  const userService = new UserService(users, cache);

  return {
    redis, cache, users, userService,
    close: () => connections.close(),
    health: () => connections.health(),
  };
}

export type Container = ReturnType<typeof buildContainer>;
```

```ts
// src/services/user.service.ts: no ioredis import anywhere
export class UserService {
  constructor(private users: UserRepositoryPort, private cache: CachePort) {}

  getProfile(id: string) {
    return this.cache.remember(`profile:${id}`, 300, () => this.users.findById(id));
  }
}
```

That is dependency injection with **plain constructors**. No framework required.

## Testing with a fake

```ts
export class InMemoryCache implements CachePort {
  private store = new Map<string, string>();
  calls = { get: 0, set: 0, del: 0 };

  async get<T>(key: string) {
    this.calls.get++;
    const raw = this.store.get(key);
    return raw === undefined ? null : (JSON.parse(raw) as T);
  }
  async set<T>(key: string, value: T) { this.calls.set++; this.store.set(key, JSON.stringify(value)); }
  async del(...keys: string[]) { this.calls.del++; keys.forEach((k) => this.store.delete(k)); }
  async remember<T>(key: string, _ttl: number, loader: () => Promise<T | null>) {
    const hit = await this.get<T>(key);
    if (hit !== null) return hit;
    const v = await loader();
    await this.set(key, v);
    return v;
  }
}

it("loads from the repository once, then serves from the cache", async () => {
  const repo = { findById: vi.fn().mockResolvedValue({ id: "1" }) };
  const svc = new UserService(repo as any, new InMemoryCache());

  await svc.getProfile("1");
  await svc.getProfile("1");

  expect(repo.findById).toHaveBeenCalledTimes(1);
});
```

Use fakes for **unit tests**, and a real Redis (Testcontainers) for **integration tests** of the service itself, because TTLs, Lua and atomicity can't be faked faithfully.

## DI frameworks

Plain constructor injection scales further than most people expect. If you use a framework, the pattern is the same: **create the client once via a factory provider, and close it on shutdown**.

### NestJS

```ts
// redis.module.ts
import { Global, Module, Inject, OnApplicationShutdown } from "@nestjs/common";
import { Redis } from "ioredis";

export const REDIS = Symbol("REDIS");

@Global()
@Module({
  providers: [
    { provide: REDIS, useFactory: () => createClient("nest") },
    RedisService,
  ],
  exports: [REDIS, RedisService],
})
export class RedisModule implements OnApplicationShutdown {
  constructor(@Inject(REDIS) private readonly redis: Redis) {}

  async onApplicationShutdown() {
    await this.redis.quit().catch(() => this.redis.disconnect());
  }
}
```

Call `app.enableShutdownHooks()` in `main.ts` so Nest runs `onApplicationShutdown` on `SIGTERM`.

### Other containers (awilix, tsyringe, InversifyJS)

Register the client as a **singleton** built by a factory, register services as singletons depending on it, and add a dispose hook that calls `quit()`.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| A giant service with a method per feature | Keep the service generic, and put feature logic in feature services |
| Business code importing `ioredis` | Depend on ports |
| One failure policy for everything | Per-method policy (cache fails open, locks fail closed) |
| Swallowing errors silently | Log with context, and emit metrics |
| Keys built inside the service from domain concepts | Take keys from the key builder |
| Faking Redis for atomicity tests | Use a real Redis in integration tests |
| Creating clients inside constructors | Inject the client |

## Key takeaways

- A thin, injected service centralizes serialization, TTL/jitter, metrics and failure policy
- Business code depends on small **ports**, which makes tests trivial
- Build the object graph once in a composition root
- Plain constructor DI is enough. Frameworks just add a factory and a shutdown hook

**Next:** [Redis Repository](./03_redis-repository.md)
