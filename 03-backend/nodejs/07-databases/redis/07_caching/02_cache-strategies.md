# Cache Strategies

Cache-aside puts the caching logic in your application. Other strategies move that logic elsewhere or change **when** data reaches the database. Each is a trade between **consistency, latency, durability and complexity**.

## Overview

| Strategy | Reads | Writes | Consistency | Risk |
|----------|-------|--------|-------------|------|
| **Cache-aside** | App checks cache, loads DB on miss | DB, then delete cache key | Eventual (TTL backstop) | Stale window, stampedes |
| **Read-through** | Cache layer loads DB on miss | Usually paired with another | Eventual | Loader in the cache layer |
| **Write-through** | Reads hit the cache | DB **and** cache written together | Strong-ish | Slower writes, caches unread data |
| **Write-behind** (write-back) | Reads hit the cache | Cache first, DB **later** | Weak | **Data loss** if Redis dies before flush |
| **Write-around** | Cache-aside style | DB only, skip the cache | Eventual | First read after a write is a miss |
| **Refresh-ahead** | Cache refreshed **before** it expires | n/a | Fresher reads | Wasted refreshes |

In practice you often mix: cache-aside for reads plus write-around or delete-on-write for writes.

## Read-through

The caller asks the cache. On a miss, the **cache layer itself** calls the loader. Redis doesn't do this natively, so you build a wrapper. The difference from cache-aside is who owns the loader: **registered once, in one place**, instead of at every call site.

```ts
type Loader<T> = (id: string) => Promise<T | null>;

interface CacheDef<T> {
  prefix: string;            // e.g. "shop:cache:product"
  version: number;
  ttl: number;
  load: Loader<T>;
}

export class ReadThroughCache {
  private defs = new Map<string, CacheDef<any>>();

  constructor(private redis: Redis) {}

  register<T>(name: string, def: CacheDef<T>) {
    this.defs.set(name, def);
  }

  async get<T>(name: string, id: string): Promise<T | null> {
    const def = this.defs.get(name);
    if (!def) throw new Error(`unknown cache: ${name}`);

    const key = `${def.prefix}:${id}:v${def.version}`;

    try {
      const raw = await this.redis.get(key);
      if (raw !== null) return JSON.parse(raw);
    } catch { /* fail open */ }

    const value = await def.load(id);
    const ttl = value === null ? 30 : Math.round(def.ttl * (0.9 + Math.random() * 0.2));
    this.redis.set(key, JSON.stringify(value), "EX", ttl).catch(() => {});
    return value;
  }

  async invalidate(name: string, id: string) {
    const def = this.defs.get(name)!;
    await this.redis.unlink(`${def.prefix}:${id}:v${def.version}`);
  }
}
```

```ts
const cache = new ReadThroughCache(redis);

cache.register<Product>("product", {
  prefix: "shop:cache:product",
  version: 2,
  ttl: 600,
  load: (id) => db.products.findById(id),
});

const product = await cache.get<Product>("product", "88");
```

**Use it when** many call sites read the same data and you want one place for keys, TTLs, loaders and metrics.

## Write-through

Every write goes to the database **and** the cache in the same operation, so reads never see stale data (subject to failures).

```ts
async function saveProduct(p: Product) {
  await db.products.upsert(p);                                        // 1. source of truth
  await redis.set(`shop:cache:product:${p.id}:v2`, JSON.stringify(p), "EX", 600);   // 2. cache
}
```

Order matters: **database first**. If the cache write fails, the entry is missing or stale until the TTL runs out. If you wrote the cache first and the database write then failed, the cache would hold data that doesn't exist.

Concurrent writers can still finish out of order (writer A's DB write, writer B's DB write, writer B's cache write, writer A's cache write leaves A's older value in cache). Mitigations: per-key serialization with a [lock](../11_distributed-locks/README.md), or a version check (see [Cache Invalidation](./03_cache-invalidation.md)).

| Pros | Cons |
|------|------|
| Reads are almost always hits | Write latency includes both systems |
| Little staleness | Caches data that may never be read |
| Simple mental model | Not atomic across Redis and the database |

**Use it for** read-heavy data where freshness matters and writes are modest (profiles, settings, product prices).

## Write-behind (write-back)

Writes go to Redis first (fast), and a background worker persists them to the database **later**, often in batches.

```
client ─► app ─► Redis (ack immediately)
                   │
                   └─► queue ─► worker ─► database (batched)
```

Producer with a Redis Stream as the durable-ish queue:

```ts
async function recordScore(playerId: string, score: number) {
  await redis.multi()
    .zadd("shop:lb", "GT", score, playerId)                       // visible to readers now
    .xadd("shop:wb:scores", "MAXLEN", "~", 1_000_000, "*",
          "player", playerId, "score", String(score))             // to be flushed later
    .exec();
}
```

Worker sketch (details in `10_streams`):

```ts
await redis.xgroup("CREATE", "shop:wb:scores", "flushers", "$", "MKSTREAM").catch(() => {});

while (running) {
  const res = await blocker.xreadgroup(
    "GROUP", "flushers", "worker-1", "COUNT", 500, "BLOCK", 2000,
    "STREAMS", "shop:wb:scores", ">"
  );
  if (!res) continue;

  const entries = (res as any)[0][1] as [string, string[]][];
  await db.scores.bulkUpsert(entries.map(([, f]) => ({ player: f[1], score: Number(f[3]) })));
  await redis.xack("shop:wb:scores", "flushers", ...entries.map(([id]) => id));   // ack after the DB commit
}
```

| Pros | Cons |
|------|------|
| Very fast writes | **Data can be lost** if Redis fails before the flush |
| Absorbs write spikes, batches DB writes | Database lags behind, so other readers see old data |
| Reduces database load | Ordering, retries and duplicates need care (make writes idempotent) |

Safeguards: AOF with `appendfsync everysec` (or `always`), replicas, acknowledge only after the database commit, idempotent bulk upserts, alerts on queue lag.

**Use it for** high-volume, loss-tolerant or reconstructable writes: counters, analytics events, "last seen", game scores. **Avoid it for** payments and anything you cannot afford to lose.

## Write-around

Writes go straight to the database and **skip** the cache. The cached copy is deleted or left to expire, and the next read repopulates it.

```ts
async function updateProduct(id: number, changes: Partial<Product>) {
  await db.products.update(id, changes);
  await redis.unlink(`shop:cache:product:${id}:v2`);      // don't write the new value
}
```

This is what cache-aside does on writes. It avoids polluting the cache with data that may never be read, at the cost of one guaranteed miss after each write.

**Use it for** data that is written often but read rarely.

## Refresh-ahead

Refresh a hot entry **before** it expires, so users never wait for a rebuild.

```ts
async function getWithRefreshAhead<T>(
  key: string,
  loader: () => Promise<T>,
  ttl: number,
  refreshBelow = 0.2          // refresh when less than 20% of the TTL remains
): Promise<T> {
  const [raw, pttl] = await Promise.all([redis.get(key), redis.pttl(key)]);

  if (raw !== null) {
    if (pttl >= 0 && pttl < ttl * 1000 * refreshBelow) {
      // refresh in the background, guarded by a short lock so only one caller does it
      const got = await redis.set(`${key}:refresh`, "1", "EX", 30, "NX");
      if (got === "OK") {
        loader()
          .then((v) => redis.set(key, JSON.stringify(v), "EX", ttl))
          .catch(() => {});
      }
    }
    return JSON.parse(raw);
  }

  const value = await loader();
  await redis.set(key, JSON.stringify(value), "EX", ttl);
  return value;
}
```

**Use it for** a small set of **hot, expensive** keys (home page, config, rankings). Avoid it for a long tail of rarely used keys because you'd refresh things nobody reads.

A related technique, stale-while-revalidate, is in [Cache Problems](./04_cache-problems.md).

## Two-level caching (L1 + L2)

Add an in-process cache in front of Redis to save the network hop for the hottest keys:

```
request ─► L1: in-process LRU (microseconds, per instance)
             └─ miss ─► L2: Redis (sub-millisecond, shared)
                          └─ miss ─► database
```

```ts
import { LRUCache } from "lru-cache";

const l1 = new LRUCache<string, string>({ max: 5000, ttl: 5_000 });   // short TTL

async function get(key: string, loader: () => Promise<unknown>) {
  const hit = l1.get(key);
  if (hit) return JSON.parse(hit);

  const raw = await redis.get(key);
  if (raw !== null) { l1.set(key, raw); return JSON.parse(raw); }

  const value = await loader();
  const s = JSON.stringify(value);
  await redis.set(key, s, "EX", 300);
  l1.set(key, s);
  return value;
}
```

The catch is L1 staleness: each instance has its own copy, so use a **very short L1 TTL** (seconds), or broadcast invalidations over Pub/Sub (next lesson).

## Choosing a strategy

| Question | Answer → strategy |
|----------|-------------------|
| Default for reading DB-backed data? | **Cache-aside** |
| Many call sites and you want central rules? | **Read-through wrapper** |
| Reads must reflect writes almost immediately? | **Write-through** (or delete-on-write) |
| Extreme write rate, some loss acceptable? | **Write-behind** |
| Data written often, read rarely? | **Write-around** |
| A few hot keys with costly rebuilds? | **Refresh-ahead** |
| Ultra-hot keys, network hop matters? | **L1 + L2** |

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Write-behind for critical data | Use a durable database write |
| Writing the cache before the database | Database first |
| Assuming write-through is atomic | It isn't, so keep TTLs as a backstop |
| Unbounded write-behind queue | Cap the stream, alert on lag |
| Non-idempotent write-behind flush | Idempotent upserts, ack after commit |
| Refresh-ahead on cold keys | Restrict to a hot set |
| Long L1 TTL | Seconds, plus invalidation messages |

## Key takeaways

- Strategies differ in who loads data and **when** the database and cache meet
- Default to cache-aside, add read-through wrappers for consistency across code
- Write-through favors freshness, write-behind favors speed and accepts risk
- Every strategy keeps TTLs as the final safety net

**Next:** [Cache Invalidation](./03_cache-invalidation.md)
