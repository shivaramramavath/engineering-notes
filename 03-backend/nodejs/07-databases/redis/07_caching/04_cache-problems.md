# Cache Problems

Caches fail in predictable ways, and each failure ends the same way: **a flood of requests hits your database**. This lesson names the problems, shows how to recognize them, and gives working fixes.

## The four problems

| Problem | What happens | Trigger |
|---------|--------------|---------|
| **Cache breakdown** (hot key expiry) | One hot key expires and many requests rebuild it at once | A single popular key's TTL ends |
| **Cache stampede / avalanche** | Many keys expire together, or Redis goes down, so the database is overwhelmed | Same TTLs, cold start, Redis outage |
| **Cache penetration** | Requests for data that doesn't exist **always** miss and hit the database | Invalid IDs, scans, attacks |
| **Cache pollution / thrashing** | Useless entries evict useful ones | No TTLs, low-value data cached |

Terminology varies (people use "stampede", "thundering herd", "dogpile" and "avalanche" loosely). What matters is the fix for each mechanism.

---

## 1. Breakdown: a hot key expires

A key serving 5,000 requests per second expires. For the next 200 ms, every request sees a miss and runs the same expensive query.

```
t=0  key expires
t=0+ 5000 req/s × 0.2s = ~1000 identical DB queries at once
```

### Fix A: single flight inside the process

Concurrent callers **in the same Node.js process** share one loader promise:

```ts
const inflight = new Map<string, Promise<unknown>>();

export function singleFlight<T>(key: string, fn: () => Promise<T>): Promise<T> {
  const existing = inflight.get(key);
  if (existing) return existing as Promise<T>;

  const p = fn().finally(() => inflight.delete(key));
  inflight.set(key, p);
  return p;
}

const product = await singleFlight(`product:${id}`, () =>
  cache.getOrLoad(key, () => db.products.findById(id), { ttl: 600 })
);
```

Cheap and effective. It reduces load from "N requests" to "one per instance". With 20 instances, you still get up to 20 rebuilds, which is often fine.

### Fix B: a distributed lock

Only **one instance across the fleet** rebuilds:

```ts
import { randomUUID } from "node:crypto";

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

export async function getWithLock<T>(
  key: string,
  loader: () => Promise<T>,
  ttl: number
): Promise<T> {
  for (let attempt = 0; attempt < 20; attempt++) {
    const raw = await redis.get(key);
    if (raw !== null) return JSON.parse(raw);

    const token = randomUUID();
    const lockKey = `shop:lock:build:${key}`;
    const locked = await redis.set(lockKey, token, "PX", 10_000, "NX");

    if (locked === "OK") {
      try {
        const again = await redis.get(key);               // double-check after winning
        if (again !== null) return JSON.parse(again);

        const value = await loader();
        await redis.set(key, JSON.stringify(value), "EX", ttl);
        return value;
      } finally {
        await (redis as any).releaseLock(lockKey, token);  // Lua compare-and-delete (module 06)
      }
    }

    await sleep(50 + Math.random() * 50);                  // someone else is building, wait and re-check
  }

  return loader();                                          // gave up waiting, fall back to the source
}
```

Notes:

- Acquire with `SET NX PX`, and release with a token check ([Lua lesson](../06_advanced-commands/03_lua-scripts.md))
- The lock has a **TTL longer than a normal rebuild**, so a crashed builder can't block everyone forever
- Waiters **poll the cache**, not the database
- A **fallback** at the end avoids failing if the builder is slow

### Fix C: stale-while-revalidate (logical expiry)

Keep serving the old value while **one** caller refreshes it. Store a logical expiry inside the value and give the Redis key a longer physical TTL:

```ts
interface Wrapped<T> { v: T; freshUntil: number }

export async function swr<T>(
  key: string,
  loader: () => Promise<T>,
  { fresh = 60, stale = 600 } = {}       // seconds
): Promise<T> {
  const raw = await redis.get(key);

  if (raw !== null) {
    const w = JSON.parse(raw) as Wrapped<T>;
    if (Date.now() < w.freshUntil) return w.v;               // fresh

    // stale: serve it now, refresh in the background (one caller only)
    const got = await redis.set(`${key}:refresh`, "1", "EX", 30, "NX");
    if (got === "OK") {
      loader()
        .then((v) => redis.set(key, JSON.stringify({ v, freshUntil: Date.now() + fresh * 1000 }), "EX", fresh + stale))
        .catch(() => {});
    }
    return w.v;
  }

  const v = await loader();                                  // cold: must load
  await redis.set(key, JSON.stringify({ v, freshUntil: Date.now() + fresh * 1000 }), "EX", fresh + stale);
  return v;
}
```

Users **never wait** for a rebuild except on a fully cold key. The trade-off is that they may see data up to `fresh + refresh time` old.

### Fix D: never expire, refresh on a schedule

For a handful of critical keys (homepage, config), skip the TTL and update them with a scheduled job or on change. Keep an **alert** so a stopped job doesn't leave data stale forever.

---

## 2. Stampede and avalanche: many keys at once

### Cause 1: identical TTLs

A bulk load creates 100,000 keys with `EX 3600`. An hour later they all expire together.

**Fix: TTL jitter.**

```ts
const ttl = (base: number, spread = 0.1) =>
  Math.round(base * (1 + (Math.random() * 2 - 1) * spread));

await redis.set(key, json, "EX", ttl(3600));        // 3240 to 3960 seconds
```

### Cause 2: cold start or a restart

An empty cache after a deploy, failover or flush sends everything to the database.

**Fixes:**

- **Warm the cache** for the hottest keys before taking traffic (a startup script or readiness gate)
- Roll out gradually (canary, slow ramp)
- Persist Redis (RDB/AOF) so a restart reloads data
- Use replicas and Sentinel/Cluster so one node failing doesn't empty everything

### Cause 3: Redis is down or slow

Every request falls through to the database.

**Fixes: protect the database with limits.**

```ts
import pLimit from "p-limit";

const dbLimit = pLimit(50);       // at most 50 concurrent DB loads from this instance

const value = await dbLimit(() => db.products.findById(id));
```

Combine with:

- **Short `commandTimeout`** and low `maxRetriesPerRequest` so slow Redis fails fast (see [Configuration](../03_ioredis-basics/02_configuration.md))
- A **circuit breaker** that stops calling Redis for a few seconds after repeated failures
- **Load shedding**: return a degraded response (or `503`) rather than melting the database
- Serve **stale data from L1** if available

A minimal breaker:

```ts
class Breaker {
  private failures = 0;
  private openUntil = 0;
  constructor(private threshold = 5, private cooldownMs = 10_000) {}

  get open() { return Date.now() < this.openUntil; }
  ok() { this.failures = 0; }
  fail() {
    if (++this.failures >= this.threshold) {
      this.openUntil = Date.now() + this.cooldownMs;
      this.failures = 0;
    }
  }
}

const breaker = new Breaker();

async function safeGet(key: string) {
  if (breaker.open) return null;               // skip Redis, go straight to the database
  try {
    const v = await redis.get(key);
    breaker.ok();
    return v;
  } catch {
    breaker.fail();
    return null;
  }
}
```

---

## 3. Penetration: data that doesn't exist

Requests for IDs that are **not in the database** never populate the cache, so every request reaches the database. Attackers (or buggy clients) can exploit this with random IDs.

### Fix A: negative caching

Cache "not found" with a short TTL:

```ts
const value = await loader();
await redis.set(key, JSON.stringify(value), "EX", value === null ? 30 : 600);
```

(This is built into the [`getOrLoad` helper](./01_cache-aside.md#a-reusable-helper).) It helps against repeated lookups of the same missing ID, but not against **millions of different random IDs**.

### Fix B: validate input first

Reject impossible IDs before touching Redis or the database:

```ts
if (!/^\d{1,10}$/.test(id)) return res.status(400).end();
```

Validation removes the largest share of junk traffic for free.

### Fix C: a Bloom filter for existence

A Bloom filter answers "definitely not present" or "maybe present" in tiny memory. Put every valid ID in it, and reject requests whose ID is definitely absent.

With the RedisBloom module (available in Redis Stack and some managed services):

```ts
await redis.call("BF.RESERVE", "shop:bf:products", "0.001", "1000000");   // error rate, capacity
await redis.call("BF.ADD", "shop:bf:products", "88");

const maybe = await redis.call("BF.EXISTS", "shop:bf:products", id);
if (maybe === 0) return res.status(404).end();                            // definitely doesn't exist
```

Without the module, implement one in-process or with bitmaps (`SETBIT`/`GETBIT` with several hash functions). Rebuild it periodically and add new IDs as rows are created (Bloom filters don't support deletion).

### Fix D: rate limit suspicious traffic

Limit per IP and per user (module `12_rate-limiting`). Random-ID scans are easy to spot as a high miss ratio from one client.

---

## 4. Pollution and thrashing

A cache full of one-time entries pushes out valuable ones. Symptoms: falling hit ratio, rising `evicted_keys`.

**Fixes:**

- **Don't cache** data that is rarely reused (long-tail search queries, one-off reports)
- Give **every key a TTL**
- Use `allkeys-lru` or `allkeys-lfu` for cache-only instances (see [Memory and Eviction](../02_redis-fundamentals/05_memory-and-eviction.md))
- Cache smaller values, and compress large ones
- Watch `evicted_keys` and the hit ratio, and resize before the hit ratio collapses

---

## Choosing fixes

| Symptom | Likely problem | First fix |
|---------|---------------|-----------|
| DB spike exactly when a popular key expires | Breakdown | Single flight, then lock or SWR |
| DB spike on the hour, keys expiring together | Stampede | TTL jitter |
| DB melts after a deploy or Redis restart | Cold cache | Warm-up, gradual rollout, concurrency limits |
| DB melts when Redis is down | Avalanche | Timeouts, breaker, `pLimit`, load shedding |
| DB busy with lookups of missing rows | Penetration | Validation, negative caching, Bloom filter |
| Hit ratio dropping, many evictions | Pollution | Cache less, TTLs, right eviction policy |

## Defense in depth

```
request
  ├─ validate input                       (penetration)
  ├─ rate limit                           (abuse)
  ├─ L1 in-process cache + single flight  (breakdown)
  ├─ Redis (jittered TTL, negative cache) (stampede, penetration)
  ├─ lock or SWR on rebuild               (breakdown)
  ├─ concurrency limit + circuit breaker  (avalanche)
  └─ database
```

No single technique covers everything, so stack the cheap ones first.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Lock without TTL | Always `PX`, longer than the rebuild time |
| Releasing a lock without a token check | Lua compare-and-delete |
| Waiters polling the database | Poll the cache |
| Same TTL for bulk-loaded keys | Jitter |
| Negative cache with a long TTL | 10 to 60 seconds |
| Retrying forever when Redis is down | Low `maxRetriesPerRequest`, circuit breaker |
| Unbounded DB concurrency on misses | `pLimit` / bulkhead |
| Stale-while-revalidate for data that must be exact | Don't use SWR for money or permissions |

## Key takeaways

- **Breakdown**: single flight, lock, or stale-while-revalidate
- **Stampede**: jittered TTLs, warm-up
- **Avalanche**: timeouts, circuit breaker, concurrency limits
- **Penetration**: validate, negative-cache, Bloom filter, rate limit
- Layer the defenses, and always protect the database

**Next:** [Caching API Responses](./05_caching-api-responses.md)
