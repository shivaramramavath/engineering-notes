# Atomic and Conditional Operations

Most concurrency bugs in Redis code have one shape: **check, then act, in two steps**. Between the steps, another client changes the world. This lesson shows how to collapse those steps into one atomic operation.

## The core idea

**Every single Redis command is atomic.** The trick is choosing a command (or an option on a command) that performs the check *and* the write together.

```ts
// Race: two clients can both see "no lock" and both proceed
if (!(await redis.get("lock"))) {
  await redis.set("lock", "me");
}

// Atomic: one command decides
const ok = await redis.set("lock", "me", "NX");   // "OK" or null
```

## Conditional writes on strings

```ts
await redis.set("k", "v", "NX");             // set only if the key does NOT exist
await redis.set("k", "v", "XX");             // set only if the key EXISTS
await redis.set("k", "v", "NX", "EX", 60);   // conditional + expiry, atomically
await redis.set("k", "v", "KEEPTTL");        // overwrite, keep the TTL
await redis.msetnx({ a: 1, b: 2 });          // set all only if none exist
await redis.setnx("k", "v");                 // legacy, prefer SET ... NX
```

Results:

| Call | Success | Condition not met |
|------|---------|-------------------|
| `SET ... NX` / `XX` | `"OK"` | `null` |
| `MSETNX` | `1` | `0` |
| `SETNX` | `1` | `0` |

### `SET ... NX GET` (Redis 7.0+)

Set if absent **and** return the existing value if it was already there:

```ts
const previous = await redis.set("cfg:owner", "node-1", "NX", "GET");
// null      → we set it
// "node-2"  → someone else had it, we changed nothing
```

## Conditional writes on other types

| Goal | Command |
|------|---------|
| Set a hash field once | `HSETNX key field value` |
| Add a sorted-set member only if new | `ZADD key NX score member` |
| Update only if it exists | `ZADD key XX score member` |
| Raise a score only if higher ("best score") | `ZADD key GT score member` |
| Lower a score only if lower | `ZADD key LT score member` |
| Set a TTL only if none exists | `EXPIRE key seconds NX` (7.0+) |
| Extend a TTL only if longer | `EXPIRE key seconds GT` (7.0+) |
| Rename only if the target is free | `RENAMENX old new` |
| Add to a set and learn if it was new | `SADD` (returns 1 or 0) |
| Move between sets | `SMOVE src dst member` |
| Move between lists | `LMOVE src dst RIGHT LEFT` |

```ts
await redis.hsetnx("user:1", "createdAt", Date.now());        // only the first call wins
await redis.zadd("lb", "GT", newScore, playerId);             // never lowers a best score
await redis.expire("session:abc", 3600, "GT");                // only extends
```

## Atomic read-and-modify commands

Reach for these instead of `GET` then `SET`:

| Command | What it does atomically |
|---------|------------------------|
| `INCR`, `INCRBY`, `DECR`, `INCRBYFLOAT` | Read, add, write |
| `HINCRBY`, `HINCRBYFLOAT` | Same, per hash field |
| `ZINCRBY` | Same, for scores |
| `GETDEL` (6.2+) | Read and delete (one-time tokens) |
| `GETEX` (6.2+) | Read and set/extend TTL (sliding sessions) |
| `SET ... GET` | Write and return the old value |
| `LPOP`/`RPOP`, `SPOP`, `ZPOPMIN`/`ZPOPMAX` | Remove and return |
| `LMOVE` / `BLMOVE` | Move a list element between lists |
| `APPEND` | Append and return the new length |

```ts
const token = await redis.getdel(`otp:${phone}`);      // usable exactly once
const data  = await redis.getex(`session:${sid}`, "EX", 1800);   // read and slide the TTL
```

## Patterns

### 1. Distributed lock (acquire)

```ts
const token = crypto.randomUUID();
const acquired = (await redis.set(`lock:${name}`, token, "PX", 10_000, "NX")) === "OK";
```

Release needs [compare-and-delete in Lua](./03_lua-scripts.md#1-safe-lock-release-compare-and-delete). Full treatment in `11_distributed-locks`.

### 2. Idempotency key

Make a repeated request (client retry, webhook redelivery) do its work only once:

```ts
async function once<T>(key: string, ttlSec: number, work: () => Promise<T>) {
  const first = (await redis.set(`idem:${key}`, "pending", "EX", ttlSec, "NX")) === "OK";
  if (!first) return { duplicate: true as const };

  try {
    const result = await work();
    await redis.set(`idem:${key}`, JSON.stringify(result), "EX", ttlSec);   // store outcome
    return { duplicate: false as const, result };
  } catch (err) {
    await redis.unlink(`idem:${key}`);       // allow a retry after failure
    throw err;
  }
}
```

A second caller sees `duplicate: true` and can read the stored result if `"pending"` has been replaced. More in `20_real-world-patterns/01_idempotency.md`.

### 3. Fixed-window counter with a single atomic step

```ts
const [[, count]] = (await redis.multi()
  .incr(`rate:${ip}`)
  .expire(`rate:${ip}`, 60, "NX")           // TTL is set once, on the first hit
  .exec())!;
if ((count as number) > 100) { /* limited */ }
```

### 4. Reserve inventory with `DECRBY`

```ts
async function reserve(sku: string, qty: number) {
  const left = await redis.decrby(`stock:${sku}`, qty);
  if (left < 0) {
    await redis.incrby(`stock:${sku}`, qty);   // give it back
    return false;
  }
  return true;
}
```

This works, but the counter is briefly negative, and a crash between `DECRBY` and the refund loses stock. For strict correctness, use the Lua script in [Lua Scripts](./03_lua-scripts.md#3-reserve-stock-only-if-enough-is-available).

### 5. Claim work exactly once

```ts
// many workers race for the same job id, only one sees 1
const mine = (await redis.sadd("jobs:claimed", jobId)) === 1;
// or: (await redis.zrem("jobs:delayed", jobId)) === 1
```

### 6. Compare-and-set (CAS)

Change a value only if it still equals what you read:

```lua
-- KEYS[1] = key, ARGV[1] = expected, ARGV[2] = new value
if redis.call("GET", KEYS[1]) == ARGV[1] then
  redis.call("SET", KEYS[1], ARGV[2])
  return 1
end
return 0
```

For records, keep a **version** field and update only when it matches:

```lua
-- KEYS[1] = hash, ARGV[1] = expected version, ARGV[2] = field, ARGV[3] = value
local v = redis.call("HGET", KEYS[1], "version")
if v == ARGV[1] then
  redis.call("HSET", KEYS[1], ARGV[2], ARGV[3])
  redis.call("HINCRBY", KEYS[1], "version", 1)
  return 1
end
return 0
```

### 7. One-time cache warm ("single flight")

Only one caller rebuilds an expensive value:

```ts
const gotLock = (await redis.set(`build:${key}`, "1", "EX", 30, "NX")) === "OK";
if (gotLock) { /* rebuild and store */ } else { /* wait briefly, or serve stale */ }
```

This is the seed of the stampede protection covered in `07_caching`.

## Choosing the right tool

| Situation | Use |
|-----------|-----|
| One conditional write | A single command with `NX`/`XX`/`GT`/`LT` |
| Read-modify-write on a number | `INCR` family |
| One-time read | `GETDEL` |
| Group of writes, no logic | `MULTI`/`EXEC` |
| Read, decide, write | Lua |
| Rarely contended CAS across keys | `WATCH` on a dedicated connection |
| Cross-system consistency (Redis + a database) | Idempotency keys, outbox patterns, or a real transaction elsewhere |

## Anti-patterns

```ts
// 1. GET then SET for counters
const n = Number(await redis.get("c")); await redis.set("c", n + 1);      // lost updates

// 2. EXISTS then SET
if (!(await redis.exists("k"))) await redis.set("k", "v");                // race

// 3. GET then DEL to release a lock
if ((await redis.get("lock")) === token) await redis.del("lock");         // may delete another owner's lock

// 4. SET then EXPIRE
await redis.set("k", "v"); await redis.expire("k", 60);                   // crash between = immortal key
//    Fix: await redis.set("k", "v", "EX", 60)

// 5. INCR then EXPIRE, unprotected
await redis.incr("c"); await redis.expire("c", 60);                       // TTL reset every time / lost on a crash
//    Fix: multi + EXPIRE ... NX, or Lua
```

## What is *not* atomic across systems

Redis atomicity covers commands **inside Redis**. It says nothing about a database write plus a Redis write:

```ts
await db.orders.insert(order);          // succeeds
await redis.unlink(`cache:order:${id}`); // process crashes before this
```

Mitigate with short TTLs on caches, idempotent retries, and (for strict needs) an outbox or change-data-capture flow.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Check-then-act in two commands | Use a conditional command or Lua |
| Ignoring the `null` from `SET NX` | It means you did **not** get the lock/claim |
| Assuming `SETNX` sets a TTL | Use `SET ... NX EX` |
| Negative stock windows with `DECRBY` + refund | Lua reservation |
| No TTL on claim/idempotency keys | Always expire them |
| Using `NX ... GET` on Redis older than 7.0 | Check the server version |
| Assuming Redis + database updates are atomic together | Design for retries and idempotency |

## Key takeaways

- Single commands are atomic, so pick the one whose options fold the check into the write
- `SET NX EX`, `ZADD GT`, `EXPIRE NX`, `GETDEL` and the `INCR` family remove most races
- Use Lua when you must read, decide and write, and `MULTI` when you only need a grouped batch
- Atomicity ends at Redis's edge, so use idempotency for cross-system work

**Next module:** [07_caching](../07_caching/README.md)
