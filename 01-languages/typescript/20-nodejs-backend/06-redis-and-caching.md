# Redis and Caching

A **cache** stores the result of expensive work so you can reuse it: a slow query, a remote API call, a computed page. **Redis** is an in-memory data store commonly used as that cache, and also for sessions, rate limiting, pub/sub, and queues. Caching is easy to add and hard to get right: the difficulty is not storing values, it is deciding when they are stale, what happens when the cache is down, and how not to serve one user another user's data. This note covers the common patterns, typing a cache so it does not become an untyped `any` store, and the failure modes to design for.

> **Tool note.** Two popular Node clients are `ioredis` and the official `redis` package (node-redis). Their APIs differ in details (option shapes, promise behavior). Examples use a small interface so the application code does not depend on either. Check the current documentation of the client you choose.

**Prerequisites:**
- [Service and repository layers](./04-service-and-repository-layers.md)
- [Databases](./05-databases.md)
- [Concurrency patterns](../12-async-and-iteration/05-concurrency-patterns.md) (in-flight deduplication)
- [Zod](../15-runtime-validation/02-zod.md)

---

## When to cache

Cache when:

- The same expensive result is requested **repeatedly** (a popular product page, a computed report, an external API response).
- The data is **allowed to be a little stale**.
- Recomputing is **costly** (slow queries, rate-limited or paid APIs).

Do not cache when the data must always be current (account balances at checkout), is rarely reused, or when the underlying operation is already cheap. A cache adds a second copy of the truth, and every copy has to be kept acceptably consistent. The first fix for a slow query is usually an index, not a cache.

## The cache-aside pattern

The most common approach: the application checks the cache, falls back to the source on a miss, and fills the cache.

```text
read(key):
  value = cache.get(key)
  if value exists:        return value        # hit
  value = source.load(key)                    # miss: go to the database or API
  cache.set(key, value, ttl)
  return value
```

```ts
async function getProduct(id: string): Promise<Product> {
  const key = `product:${id}`;

  const cached = await cache.get(key, ProductSchema);
  if (cached) return cached;

  const product = await products.getById(id);          // source of truth
  await cache.set(key, product, 300);                  // 5 minutes
  return product;
}
```

Writes go to the **source of truth first**, then invalidate or update the cache entry.

## A typed cache interface

A raw Redis client returns `string | null`. If you `JSON.parse` it and cast, the cache becomes an untyped back door: old entries in an outdated shape, or corrupted data, flow into your code as if they were correct. Wrap the client in a small interface that **validates on read** ([trust boundaries](../15-runtime-validation/00-trust-boundaries.md)):

```ts
import { z } from "zod";

export interface Cache {
  get<S extends z.ZodType>(key: string, schema: S): Promise<z.output<S> | undefined>;
  set(key: string, value: unknown, ttlSeconds: number): Promise<void>;
  del(key: string): Promise<void>;
}

export class RedisCache implements Cache {
  constructor(private readonly redis: RedisLike) {}

  async get<S extends z.ZodType>(key: string, schema: S): Promise<z.output<S> | undefined> {
    const raw = await this.redis.get(key);               // string | null
    if (raw === null) return undefined;
    try {
      return schema.parse(JSON.parse(raw));              // validated, typed
    } catch {
      await this.redis.del(key);                         // corrupt or outdated entry: drop it
      return undefined;                                  // treat as a miss
    }
  }

  async set(key: string, value: unknown, ttlSeconds: number): Promise<void> {
    await this.redis.set(key, JSON.stringify(value), "EX", ttlSeconds);   // ioredis form; node-redis uses { EX: ... }
  }

  async del(key: string): Promise<void> {
    await this.redis.del(key);
  }
}
```

Why this shape:

- **The schema makes the return type accurate.** A shape change (you deploy a new version with a different `Product`) becomes a cache miss instead of a runtime crash, because old entries fail validation.
- **The application depends on `Cache`,** not on a specific client. Swap Redis for an in-memory implementation in tests ([dependency injection](../17-design-patterns/05-dependency-injection.md)).
- **Failure to parse is a miss,** not an error.

A reusable "get or load" helper removes the boilerplate:

```ts
export async function cached<S extends z.ZodType>(
  cache: Cache,
  key: string,
  schema: S,
  ttlSeconds: number,
  load: () => Promise<z.output<S>>,
): Promise<z.output<S>> {
  const hit = await cache.get(key, schema);
  if (hit !== undefined) return hit;

  const value = await load();
  await cache.set(key, value, ttlSeconds);
  return value;
}

const product = await cached(cache, `product:${id}`, ProductSchema, 300, () => products.getById(id));
```

### Serialization pitfalls

Redis stores strings (or bytes), so your values go through JSON:

- `Date` becomes an ISO string. A schema with `z.coerce.date()` (or a transform) converts it back.
- `undefined` properties disappear, `Map`/`Set` become `{}`, `bigint` throws, and class instances lose their methods ([request and response types](../16-type-safe-apis/01-request-response-types.md)).
- Cache **DTOs or plain data**, not class instances or ORM entities.

## Keys, TTLs, and invalidation

### Key design

- **Namespace and version keys:** `app:v2:product:42`. Changing the version invalidates everything when you change the cached shape.
- **Include everything the value depends on:** the id, the locale, the filters, the page. A key that omits a dependency serves the wrong data (the cached French page to an English user).
- Keep keys predictable and bounded. Do not put unbounded user input in keys without validation (it can fill memory).

```ts
const keys = {
  product: (id: string) => `app:v1:product:${id}`,
  productList: (q: ProductQuery) => `app:v1:products:${hash(q)}`,
};
```

### Expiration

- **Always set a TTL** on cache entries, so mistakes are bounded and memory is reclaimed.
- Choose TTLs by how stale the data may be: seconds for volatile data, minutes or hours for stable data.
- Add a little **jitter** (randomness) to TTLs for many similar keys so they do not all expire at the same instant.

### Invalidation

"There are only two hard things in computer science: cache invalidation and naming things." The usual strategies:

| Strategy | How | Trade-off |
|---|---|---|
| **TTL only** | let entries expire | simple, data may be stale until expiry |
| **Delete on write** | after updating the source, `del` the key | fresh, but you must find every key that depends on the changed data |
| **Write-through** | update the source and the cache together | fresh and simple reads, but writes are slower and both can fail |
| **Versioned keys** | include a version or "updated at" token in the key | no deletes needed, old keys expire by TTL |

Invalidate **after** the source write succeeds, and accept a short window of staleness. Prefer designs where being slightly stale is acceptable, because perfect consistency across a cache and a database is expensive.

## Cache stampede

When a popular entry expires, many concurrent requests miss at once and all hit the database together. Prevent it by **sharing one in-flight load** per key within a process:

```ts
const inflight = new Map<string, Promise<unknown>>();

async function singleFlight<T>(key: string, load: () => Promise<T>): Promise<T> {
  const existing = inflight.get(key);
  if (existing) return existing as Promise<T>;

  const promise = load().finally(() => inflight.delete(key));
  inflight.set(key, promise);
  return promise;
}
```

Combine it with `cached(...)` so concurrent misses for the same key share one load ([concurrency patterns](../12-async-and-iteration/05-concurrency-patterns.md)). Across **multiple instances**, a stampede can still happen. Mitigations include a short lock in Redis, serving stale data while one worker refreshes, and TTL jitter.

## Failure handling: the cache must not take you down

A cache is an optimization, not a dependency of correctness. When Redis is slow or unreachable:

- **Degrade to the source.** Treat a cache error as a miss, log it, and continue.
- **Set short timeouts** on cache calls, so a hanging Redis does not hang every request.
- **Do not let a cache write failure fail the request.** Catch and log.

```ts
async function safeGet<S extends z.ZodType>(key: string, schema: S) {
  try {
    return await cache.get(key, schema);
  } catch (err) {
    logger.warn({ err, key }, "cache read failed");
    return undefined;                      // behave as a miss
  }
}
```

Be careful with the opposite failure too: if every request falls through to the database at once because the cache is cold or down, the database can be overwhelmed. Rate limit, and warm critical entries.

## Security

- **Never cache per-user data under a shared key.** Include the user (or tenant) in the key, or do not cache it. Serving user A's private data to user B is a classic caching vulnerability.
- Do not cache **sensitive data** (tokens, personal data) unless necessary, and give it short TTLs.
- Treat cached data as **untrusted input** on read (the validation above), since the cache can be written by other services or contain older formats.
- Secure Redis itself: require authentication, use TLS across networks, and never expose it to the public internet.

## Other uses of Redis

| Use | How |
|---|---|
| **Sessions** | store a session by id with a TTL, set an HttpOnly cookie with the id ([authentication](./07-authentication-and-authorization.md)) |
| **Rate limiting** | increment a counter per key with an expiry (`INCR` + `EXPIRE`), or use a library |
| **Pub/sub** | broadcast events between instances (fire-and-forget: subscribers miss messages sent while disconnected) |
| **Queues and jobs** | libraries such as BullMQ build reliable job queues on Redis |
| **Distributed locks** | possible, but subtle: use a vetted library and keep locks short, with timeouts |
| **Leaderboards, counters** | sorted sets and atomic increments |

Atomicity: single Redis commands are atomic, so `INCR` is safe under concurrency. Read-modify-write sequences across several commands are not, unless you use a transaction (`MULTI`) or a Lua script.

## In-process caches

For data that is small, hot, and per-instance, an **in-memory cache** (a `Map` with TTL, or an LRU library such as `lru-cache`) is faster than Redis and has no network failure mode:

```ts
import { LRUCache } from "lru-cache";

const configCache = new LRUCache<string, FeatureFlags>({ max: 100, ttl: 30_000 });
```

Caveats: each instance has its own copy (so they may disagree briefly), memory is bounded by your process, and entries vanish on restart. Use an LRU or a size limit so it cannot grow without bound. Many systems use both: a small in-process cache in front of Redis in front of the database.

## HTTP caching

Not all caching needs Redis. HTTP has built-in mechanisms that move caching to browsers, proxies, and CDNs:

- **`Cache-Control`** (`max-age`, `public`, `private`, `no-store`) says who may cache a response and for how long.
- **`ETag` / `If-None-Match`** lets clients revalidate cheaply and receive `304 Not Modified`.

Use `private` or `no-store` for user-specific responses. Set `Vary` when responses depend on request headers. HTTP caching often removes load before it reaches your code.

## Testing

- Unit-test services with an **in-memory `Cache` implementation**, and test the "miss, load, store" and "hit" paths.
- Test the **failure paths**: a throwing cache must not break the request.
- Test that **old or corrupt entries** are treated as misses.
- For the Redis implementation, use a real Redis (a container) in integration tests rather than assuming a mock behaves identically ([integration testing](../18-testing-and-debugging/01-integration-testing.md)).

## Important rules and misconceptions

- **A cache is not a source of truth.** Anything in it can disappear, expire, or be stale.
- **Casting `JSON.parse` output defeats typing.** Validate on read.
- **Redis is single-threaded for commands,** so a slow command (or a huge key scan) blocks every other client. Avoid `KEYS` on large datasets, and use `SCAN`.
- **Caching does not fix a missing index.**
- **`undefined` is not storable.** Decide how to represent "known missing" (a negative cache entry with a short TTL) versus "not cached".
- **TTL is a ceiling on staleness only if every write sets it.** Some commands overwrite and drop the TTL.

## Common mistakes

- No TTL, so stale data lives forever and memory grows.
- Casting cached JSON to a type without validation.
- Keys that omit a dependency (user, locale, filters), serving the wrong data.
- Caching per-user data under a shared key.
- Letting a cache outage fail requests.
- A stampede on popular keys with no single-flight or jitter.
- Invalidating **before** the source write succeeds, then re-caching the old value.
- Caching class instances or ORM entities and getting broken objects back.
- Using `KEYS *` in production.
- Treating Redis as durable storage without configuring persistence or accepting data loss.
- Caching errors or empty results unintentionally for long periods.

## Debugging

- Log hits, misses, and load durations (or expose counters), so you can see the hit rate. A low hit rate means the cache is not helping.
- Inspect values and TTLs with `redis-cli` (`GET`, `TTL`, `SCAN`) to confirm what is stored.
- If users see stale data, check the TTL, whether invalidation runs after writes, and whether a key is missing a dependency.
- If a deploy causes errors from the cache, check for shape changes: version your keys or rely on validation to treat old entries as misses.
- If the database spikes when the cache expires or restarts, look for stampedes and add single-flight, jitter, or warming.
- Check connection settings (timeouts, retry behavior) if Redis problems cause request latency.

## Quick summary

- Cache expensive, reusable, tolerably stale data with **cache-aside**: check the cache, load on a miss, store with a TTL.
- Wrap Redis in a typed `Cache` interface that **validates on read**, so shape changes become misses. Cache plain data, not class instances.
- Namespace and version keys, include every dependency in the key, always set TTLs (with jitter), and invalidate after the source write.
- Prevent stampedes with single-flight and jitter, and make cache failures degrade to the source instead of failing requests.
- Never share per-user data under one key, secure Redis, and treat cached data as untrusted input.
- Redis also serves sessions, rate limiting, pub/sub, and queues. In-process LRU caches and HTTP caching are often simpler and sufficient.

**Next:** [Authentication and authorization](./07-authentication-and-authorization.md)
