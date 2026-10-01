# Locking Fundamentals

## What a lock must guarantee

| Property | Meaning |
|----------|---------|
| **Mutual exclusion** | At most one client holds the lock at a time |
| **Deadlock freedom** | If the holder crashes, the lock eventually frees itself |
| **Safe release** | A client can only release a lock **it** holds |

A single Redis key with an expiry gives you all three, as long as you acquire and release correctly.

## Acquire: one atomic command

```ts
import { randomUUID } from "node:crypto";

const token = randomUUID();                                       // identifies THIS holder
const ok = await redis.set("shop:lock:order:1042", token, "PX", 10_000, "NX");

if (ok === "OK") {
  // we hold the lock for at most 10 seconds
} else {
  // someone else holds it (ok === null)
}
```

| Part | Why |
|------|-----|
| `NX` | Set only if the key does **not** exist, so exactly one client wins |
| `PX 10000` | A TTL in the **same** command, so a crash can't leave the lock forever |
| `token` | A unique value, so only the owner can release it |

### The classic mistake

```ts
// WRONG: two steps
await redis.setnx(key, "1");
await redis.expire(key, 10);      // a crash between the two commands = a lock that never expires
```

Always use `SET key token NX PX ttl`.

## Release: compare the token, then delete

A plain `DEL` is dangerous:

```
A acquires (TTL 10 s) ── A is slow ── lock expires ── B acquires ── A finishes ── A: DEL ──► deletes B's lock!
```

Release only if the stored value is still **your** token, **atomically**, so it needs a Lua script ([why](../06_advanced-commands/03_lua-scripts.md#1-safe-lock-release-compare-and-delete)):

```ts
const RELEASE = `
if redis.call("GET", KEYS[1]) == ARGV[1] then
  return redis.call("DEL", KEYS[1])
end
return 0`;

async function release(key: string, token: string): Promise<boolean> {
  return (await redis.eval(RELEASE, 1, key, token)) === 1;
}
```

`false` means the lock was **not yours anymore** (it expired). That is an important signal, so log it ([next lesson](./02_lock-expiration-and-renewal.md)).

## A small, complete implementation

```ts
// src/locks/simple-lock.ts
import { randomUUID } from "node:crypto";
import type { Redis } from "ioredis";

const RELEASE = `
if redis.call("GET", KEYS[1]) == ARGV[1] then
  return redis.call("DEL", KEYS[1])
end
return 0`;

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

export class LockNotAcquiredError extends Error {
  constructor(public key: string) { super(`could not acquire lock: ${key}`); }
}

export async function tryLock(redis: Redis, key: string, ttlMs: number): Promise<string | null> {
  const token = randomUUID();
  const ok = await redis.set(key, token, "PX", ttlMs, "NX");
  return ok === "OK" ? token : null;
}

export async function unlock(redis: Redis, key: string, token: string): Promise<boolean> {
  return (await redis.eval(RELEASE, 1, key, token)) === 1;
}

export interface LockOptions {
  ttlMs: number;
  retries?: number;          // additional attempts after the first, default 10
  retryDelayMs?: number;     // base delay, default 100
  jitterMs?: number;         // random extra delay, default 100
}

export async function withLock<T>(
  redis: Redis,
  key: string,
  opts: LockOptions,
  fn: () => Promise<T>
): Promise<T> {
  const { ttlMs, retries = 10, retryDelayMs = 100, jitterMs = 100 } = opts;

  let token: string | null = null;
  for (let attempt = 0; attempt <= retries && !token; attempt++) {
    token = await tryLock(redis, key, ttlMs);
    if (!token && attempt < retries) await sleep(retryDelayMs + Math.random() * jitterMs);
  }
  if (!token) throw new LockNotAcquiredError(key);

  try {
    return await fn();
  } finally {
    const released = await unlock(redis, key, token).catch(() => false);
    if (!released) console.warn(`lock ${key} expired or was lost before release`);
  }
}
```

Usage:

```ts
const result = await withLock(redis, keys.lock("order:1042"), { ttlMs: 10_000 }, async () => {
  const order = await loadOrder(1042);
  return process(order);
});
```

Design notes:

| Choice | Reason |
|--------|--------|
| **Jitter** on retry delay | Many waiters retrying in lockstep stampede Redis and each other |
| Release in `finally` | The lock frees even if `fn` throws |
| Errors swallowed on release | If Redis is unreachable, the TTL frees the lock anyway |
| Distinct `LockNotAcquiredError` | Callers decide: skip, retry later, return `409` or `503` |

## Naming and granularity

```ts
keys.lock("order:1042")           // shop:lock:order:1042   per resource
keys.lock("job:nightly-report")   // shop:lock:job:nightly-report
```

- Lock the **smallest resource** that must be exclusive (an order, not "all orders"). A global lock serializes your whole system
- Build keys with your [key builder](../08_nodejs-integration/05_redis-key-builder.md), so `1:admin` can't alter the key structure
- Use a clear prefix (`lock:`) so locks are easy to find, audit and exclude from eviction (keep them out of `allkeys-*` eviction pools, see [Memory and Eviction](../02_redis-fundamentals/05_memory-and-eviction.md))

## Choosing the TTL

The TTL is a **lease**: the maximum time you may hold the lock without renewing.

| TTL too short | TTL too long |
|---------------|--------------|
| Expires **while you are still working**, so two holders ([next lesson](./02_lock-expiration-and-renewal.md)) | After a crash, everyone waits the full TTL |

Guideline: **TTL = expected worst-case duration × safety factor (2 to 3)**, and keep critical sections short. If the work can take unpredictable time, use a **short TTL with automatic renewal**, not a long TTL.

## Try-lock, wait, or skip?

| Strategy | Code | Use for |
|----------|------|---------|
| **Skip** | `tryLock` once, return if `null` | Cron jobs, cache rebuilds (someone else is on it) |
| **Wait with a timeout** | `withLock` with retries | Short critical sections where the request should proceed |
| **Fail fast** | `retries: 0`, respond `409` or `503` | Interactive requests that shouldn't queue |
| **Queue instead** | Don't lock, enqueue the work | Work that can happen later and must be serialized |

Waiting is **not fair**: there's no FIFO, and a waiter can starve. For ordered processing, use a queue or a stream partition, not a lock.

## Pattern: run a scheduled job on one instance

Every instance runs the same cron schedule, but only one should do the work:

```ts
import cron from "node-cron";

cron.schedule("0 * * * *", async () => {
  const key = keys.lock("job:send-digest");
  const token = await tryLock(redis, key, 10 * 60_000);          // longest the job may run
  if (!token) return;                                            // another instance has it

  try {
    await sendDigest();
  } finally {
    await unlock(redis, key, token);
  }
});
```

**There is a subtle bug here.** If the job finishes in 2 seconds and the lock is released, an instance whose clock fires a few seconds later **acquires it again and runs the job a second time**. Clock skew between servers makes this common.

For "run once per window", don't release. Make the lock key **identify the window** and let it expire on its own:

```ts
const windowId = new Date().toISOString().slice(0, 13);                  // "2026-10-01T09" (hourly)
const ran = await redis.set(keys.lock(`job:send-digest:${windowId}`), hostname, "EX", 2 * 3600, "NX");

if (ran === "OK") await sendDigest();                                    // exactly one instance per hour
```

Better still, if you have a real scheduler (BullMQ repeatable jobs, Kubernetes CronJob), use it instead of cron-in-every-instance ([queues module](../14_queues-and-workers/README.md)).

## Pattern: rebuild a cache entry once

```ts
const raw = await redis.get(cacheKey);
if (raw) return JSON.parse(raw);

const token = await tryLock(redis, keys.lock(`build:${cacheKey}`), 15_000);
if (token) {
  try {
    const value = await rebuild();
    await redis.set(cacheKey, JSON.stringify(value), "EX", 300);
    return value;
  } finally {
    await unlock(redis, keys.lock(`build:${cacheKey}`), token);
  }
}
// someone else is rebuilding: wait briefly and re-read the cache (see Cache Problems)
```

This is an **efficiency** lock: if it fails, the worst case is two rebuilds ([Cache Problems](../07_caching/04_cache-problems.md#fix-b-a-distributed-lock)).

## Pattern: serialize access to a rate-limited external API

```ts
await withLock(redis, keys.lock(`payments-api:${merchantId}`), { ttlMs: 30_000, retries: 50 }, async () => {
  await paymentsApi.capture(merchantId, orderId, { idempotencyKey: `capture-${orderId}` });
});
```

Notice the **idempotency key** passed to the external service. The lock reduces concurrency, and the idempotency key makes a duplicate harmless if the lock ever fails. Use both when the stakes are real.

## Reentrancy

Redis locks are **not reentrant**. If the same code path tries to take the same lock twice, it deadlocks until the TTL. Options:

- Structure code so the lock is taken **once, at the outer boundary**, and inner functions assume it is held
- Pass the lock handle down, rather than re-acquiring
- If you truly need reentrancy, keep a counter in the lock's value and increment it for the same token (more Lua, more risk, usually avoidable)

## Cluster and failover: the weak spot

A lock is one key on one primary. Replication to replicas is **asynchronous**:

```
A: SET lock ─► primary (ack)       primary crashes before replicating
                                   replica promoted: it has no lock key
B: SET lock ─► new primary (OK)    ← A and B both think they hold the lock
```

For **efficiency** locks this is usually tolerable (rare, brief duplication). For **correctness** locks it is a real hazard, so you need [fencing tokens](./02_lock-expiration-and-renewal.md#fencing-tokens), checks in the protected system, or a different tool ([Redlock](./03_redlock.md) and the alternatives).

In Redis Cluster, the lock key lives on one shard. If that shard fails over, the same issue applies.

## Alternatives to Redis locks

| Tool | Notes |
|------|-------|
| **Database row lock** (`SELECT ... FOR UPDATE`) | Strong, transactional, scoped to the data you're protecting |
| **Postgres advisory locks** (`pg_try_advisory_lock`) | App-level locks backed by the database, no extra infrastructure. Mind connection pooling (session-level locks need a stable connection) |
| **Unique constraint or conditional update** | Often removes the need for a lock entirely |
| **etcd, ZooKeeper, Consul** | Consensus-based, designed for coordination, with proper leases |
| **Kubernetes `Lease`** | Built-in leader election for pods |
| **Queue with concurrency 1** or partitions | Serialization without locking |

## Testing locks

Mutual exclusion is easy to test with real concurrency:

```ts
it("never lets two holders run at once", async () => {
  let active = 0;
  let maxActive = 0;
  let counter = 0;

  await Promise.all(
    Array.from({ length: 20 }, () =>
      withLock(redis, "test:lock", { ttlMs: 5_000, retries: 200, retryDelayMs: 5, jitterMs: 10 }, async () => {
        active++;
        maxActive = Math.max(maxActive, active);
        const v = counter;
        await new Promise((r) => setTimeout(r, 5));       // a window for a race to show up
        counter = v + 1;
        active--;
      })
    )
  );

  expect(maxActive).toBe(1);
  expect(counter).toBe(20);                              // no lost updates
});

it("a stale release cannot delete someone else's lock", async () => {
  const a = await tryLock(redis, "test:lock2", 50);
  await sleep(80);                                       // A's lease expires
  const b = await tryLock(redis, "test:lock2", 5_000);   // B acquires
  expect(await unlock(redis, "test:lock2", a!)).toBe(false);   // A's release is a no-op
  expect(await redis.get("test:lock2")).toBe(b);               // B still holds it
});
```

Test against a real Redis, because the guarantees come from Redis semantics.

## Observability

Track:

- Acquire **attempts, successes, failures** (contention)
- **Wait time** before acquiring
- **Hold time** (compare with the TTL, where holds near the TTL mean trouble)
- **Release failures** (`false` = the lock expired before release)

Log the lock name, a short token prefix, and timings, never full tokens in shared logs.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| `SETNX` then `EXPIRE` | `SET key token NX PX ttl` in one command |
| `DEL` to release | Compare-and-delete in Lua |
| A shared constant instead of a unique token | `randomUUID()` per acquisition |
| No TTL | Always set one |
| TTL shorter than the work | Short TTL plus renewal, or smaller units of work |
| Releasing a cron lock immediately | Window-based keys that expire on their own |
| One global lock for everything | Lock per resource |
| Ignoring `false` from release | Log it, because the lock expired while you held it |
| Synchronized retries | Jitter |
| Using a lock where an atomic command would do | Re-read [the alternatives](./README.md#first-question-do-you-really-need-a-lock) |
| Treating a Redis lock as proof of exclusivity for critical data | Fencing tokens or database-level checks |

## Key takeaways

- Acquire with **`SET key token NX PX ttl`**, release with a **token-checking Lua script**
- The TTL gives deadlock freedom, but it also means a holder can **lose** the lock while working
- Use jitter, per-resource keys and idempotency keys
- Redis locks are great for **efficiency**, and for **correctness** you need more ([next lesson](./02_lock-expiration-and-renewal.md))

**Next:** [Lock Expiration and Renewal](./02_lock-expiration-and-renewal.md)
