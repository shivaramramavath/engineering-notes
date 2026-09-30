# Cache-Aside

Also called **lazy loading**. The application owns the logic: check the cache, on a miss load from the database, then populate the cache. Redis never talks to your database.

## The flow

```
READ                                        WRITE
1. GET key from Redis                       1. Update the database
2. hit  → return it                         2. DELETE the key from Redis
3. miss → load from the database
4. SET key with a TTL
5. return
```

Why it is the default:

- Only data that is actually requested gets cached
- A Redis outage degrades performance instead of breaking the app
- The cache and the database stay loosely coupled
- Simple to reason about and to debug

## A minimal implementation

```ts
import { Redis } from "ioredis";

const redis = new Redis();

async function getUser(id: number) {
  const key = `shop:cache:user:${id}:v1`;

  const cached = await redis.get(key);
  if (cached !== null) return JSON.parse(cached);

  const user = await db.users.findById(id);         // your database call
  if (user) await redis.set(key, JSON.stringify(user), "EX", 300);

  return user;
}
```

That works, but a production version needs more care. The rest of this lesson builds it up.

## A reusable helper

```ts
import { Redis } from "ioredis";

interface CacheOptions {
  ttl: number;            // seconds
  jitter?: number;        // fraction of ttl to randomize, default 0.1
  nullTtl?: number;       // seconds to remember "not found", default 30
}

export function createCache(redis: Redis, log = console) {
  async function getOrLoad<T>(
    key: string,
    loader: () => Promise<T | null>,
    { ttl, jitter = 0.1, nullTtl = 30 }: CacheOptions
  ): Promise<T | null> {
    // 1. read (fail open)
    try {
      const raw = await redis.get(key);
      if (raw !== null) return JSON.parse(raw) as T | null;   // "null" string = cached "not found"
    } catch (err) {
      log.warn("cache read failed", err);
    }

    // 2. load from the source of truth
    const value = await loader();

    // 3. populate (never block the response, never throw)
    const seconds =
      value === null
        ? nullTtl
        : Math.round(ttl * (1 + (Math.random() * 2 - 1) * jitter));

    redis.set(key, JSON.stringify(value), "EX", seconds).catch((err) =>
      log.warn("cache write failed", err)
    );

    return value;
  }

  return { getOrLoad };
}
```

Usage:

```ts
const cache = createCache(redis);

const user = await cache.getOrLoad(
  `shop:cache:user:${id}:v1`,
  () => db.users.findById(id),
  { ttl: 300 }
);
```

Design choices worth noticing:

| Choice | Why |
|--------|-----|
| `try/catch` around the read | A Redis problem falls through to the database instead of failing the request |
| `"null"` stored for missing rows | **Negative caching**: repeated lookups for missing IDs don't hammer the database. Note that `JSON.stringify(null)` is the string `"null"`, which is distinguishable from a real miss (`raw === null`) |
| Short TTL for nulls | The row might be created a moment later |
| TTL jitter | Keys created together don't expire together |
| Fire-and-forget `set` with `.catch` | The user isn't waiting on a cache write, and there's no unhandled rejection |

## Writes: update the database, then delete the key

```ts
async function updateUser(id: number, changes: Partial<User>) {
  const user = await db.users.update(id, changes);   // 1. source of truth first
  await redis.unlink(`shop:cache:user:${id}:v1`);    // 2. drop the stale copy
  return user;
}
```

### Why delete instead of updating the cache?

| Approach | Problem |
|----------|---------|
| **Delete** (recommended) | The next read rebuilds from the database, so the cache can't hold something the database doesn't |
| **Set** the new value | Two concurrent writers can finish their database and cache writes in different orders, leaving the **older** value in the cache |
| Set the value **and** it is expensive to build | Waste if nobody reads it again before the next write |

Deleting is simpler and safer. There is still a small race (see [Cache Invalidation](./03_cache-invalidation.md)), and the TTL is the backstop.

If the delete itself fails (Redis down), log it. The stale entry lives until its TTL runs out, which is exactly why every key has one.

## Caching objects as hashes

If you mostly read a few fields, or update fields individually, cache a hash instead of a JSON string:

```ts
const key = `shop:cache:user:${id}:h1`;
const h = await redis.hgetall(key);
if (Object.keys(h).length) return h;

const user = await db.users.findById(id);
await redis.multi().hset(key, user).expire(key, 300).exec();
```

Use JSON strings for whole-object reads, and hashes when partial reads or writes matter (see [Hashes](../04_data-structures/02_hashes.md)).

## Batch reads

Avoid one round trip per item:

```ts
async function getUsers(ids: number[]) {
  const keys = ids.map((id) => `shop:cache:user:${id}:v1`);
  const raws = await redis.mget(keys);                       // one round trip

  const result = new Map<number, User | null>();
  const missing: number[] = [];

  raws.forEach((raw, i) => {
    if (raw !== null) result.set(ids[i], JSON.parse(raw));
    else missing.push(ids[i]);
  });

  if (missing.length) {
    const rows = await db.users.findByIds(missing);          // one query for all misses
    const p = redis.pipeline();
    for (const u of rows) {
      result.set(u.id, u);
      p.set(`shop:cache:user:${u.id}:v1`, JSON.stringify(u), "EX", 300);
    }
    p.exec().catch(() => {});
  }
  return ids.map((id) => result.get(id) ?? null);
}
```

In Cluster, `MGET` across slots fails with `CROSSSLOT`. Use a pipeline with hash tags, or auto pipelining (see [Pipelines](../06_advanced-commands/01_pipelines-and-auto-pipelining.md)).

## Serialization notes

- `JSON.stringify` turns `Date` into strings and drops `undefined`. Revive dates when reading
- `BigInt` is not JSON-serializable, so convert it to a string first
- Keep cached payloads **small**: cache the fields you need, not entire ORM objects with relations
- Large payloads can be compressed (see `16_performance/04_serialization.md`)
- Include a **version** in the key (`:v1`) so you can change the shape by bumping it

## Choosing a TTL

| Data | Typical TTL | Reasoning |
|------|-------------|-----------|
| Rarely changing reference data (countries, plans) | Hours to a day | Stale data is harmless |
| Product pages, profiles | 1 to 15 minutes | Balance freshness and load |
| Search results, feeds | 10 to 60 seconds | Very read-heavy, tolerant of lag |
| Money, inventory, permissions | Seconds, or **don't cache** | Stale data is expensive |
| "Not found" results | 10 to 60 seconds | Rows may appear soon |

Ask: **"What is the worst outcome if a user sees data this old?"** That answer is your maximum TTL.

## Measuring the hit ratio

Server-wide numbers:

```ts
const stats = await redis.info("stats");
// keyspace_hits:12345 keyspace_misses:678
```

These count **all** reads on the server, not only your cache. For accurate numbers, count in the application:

```ts
await redis.hincrby("shop:metrics:cache:user", raw !== null ? "hit" : "miss", 1);
```

Or export counters to your metrics system (Prometheus, Datadog) and graph hit ratio by cache name.

## When cache-aside is not enough

| Need | Look at |
|------|---------|
| The cache library should load data itself | Read-through ([next lesson](./02_cache-strategies.md)) |
| Cache must always match the database | Write-through |
| Very high write rates | Write-behind |
| Many concurrent misses on hot keys | [Cache problems](./04_cache-problems.md) |

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| No TTL | Always `EX` |
| Same TTL for every key | Add jitter |
| Caching `null` by skipping it | Cache "not found" with a short TTL |
| Letting a Redis error fail the request | `try/catch`, fail open, with a short `commandTimeout` |
| Updating the cache on writes | Delete the key instead |
| Caching before the database commit | Only invalidate **after** the write succeeds |
| Huge cached objects | Cache only what the view needs |
| Cache key without version | Add `:v1` |

## Key takeaways

- Cache-aside: read the cache, on a miss load the database and populate, on a write update the database and **delete** the key
- Always TTL, add jitter, cache "not found", and fail open
- Batch reads with `MGET` or pipelines
- Measure hit ratio in the app

**Next:** [Cache Strategies](./02_cache-strategies.md)
