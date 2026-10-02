# Fixed and Sliding Window

Window-based limiters answer: **"how many requests has this client made recently?"** They differ in how they define *recently*.

## Fixed window

Divide time into equal windows (for example one minute). Count requests per window. Reject once the count passes the limit.

```
window 12:00–12:01      window 12:01–12:02
 ████████ (limit 100)    ██░░░░░░░░
```

### Implementation (clock-aligned windows)

Include the window number in the key, so each window has its own counter that expires on its own:

```ts
async function fixedWindow(id: string, limit: number, windowSec: number) {
  const windowId = Math.floor(Date.now() / (windowSec * 1000));
  const key = `shop:rate:fw:${id}:${windowId}`;

  const res = await redis.multi()
    .incr(key)
    .expire(key, windowSec, "NX")          // set the TTL once, on the first hit (Redis 7.0+)
    .exec();

  const count = res![0]![1] as number;
  return { allowed: count <= limit, remaining: Math.max(0, limit - count), count };
}
```

`INCR` and `EXPIRE ... NX` run inside one `MULTI`, so a crash can't leave a counter without a TTL ([Transactions](../06_advanced-commands/02_transactions.md)). On Redis older than 7.0, use the Lua version below.

### Implementation (anchored window, in Lua)

The window starts at the client's **first request** and lasts `windowSec`. There is no clock involved, and the script is atomic and returns the time to reset:

```lua
-- fixed-window.lua
-- KEYS[1] = counter key; ARGV[1] = limit, ARGV[2] = window seconds
local count = redis.call("INCR", KEYS[1])
if count == 1 then
  redis.call("EXPIRE", KEYS[1], tonumber(ARGV[2]))
end
local ttl = redis.call("TTL", KEYS[1])
if ttl < 0 then                                  -- safety: a key without TTL would never reset
  redis.call("EXPIRE", KEYS[1], tonumber(ARGV[2]))
  ttl = tonumber(ARGV[2])
end
local limit = tonumber(ARGV[1])
if count > limit then
  return {0, 0, ttl}                             -- denied, nothing remaining, seconds until reset
end
return {1, limit - count, ttl}
```

```ts
redis.defineCommand("rlFixedWindow", { numberOfKeys: 1, lua: load("fixed-window") });
const [allowed, remaining, resetSec] = await redis.rlFixedWindow(`shop:rate:fw:${id}`, 100, 60);
```

(Typing the command is covered in [Typed Redis Client](../08_nodejs-integration/04_typed-redis-client.md#typing-custom-commands-lua).)

### The boundary burst problem

Fixed windows have a flaw: a client can send the full limit **at the end of one window** and again **at the start of the next**.

```
limit 100 / minute

   12:00:30 ─────── 12:00:59 | 12:01:00 ─────── 12:01:30
                     100 reqs | 100 reqs
                      └── 200 requests within about 2 seconds ──┘
```

Up to **twice the limit** can pass in a short span. For quotas ("1000 per day") that is acceptable. For protecting a fragile backend it isn't.

| Strength | Weakness |
|----------|----------|
| One key, one `INCR`, tiny memory | Edge bursts up to 2× |
| Trivial to explain and debug | Counts reset abruptly, so clients learn to sync with the reset |

## Sliding window log

Remember the **timestamp of every request** in a sorted set, and count those inside the last `windowMs`. This is exact: any window of the given length contains at most `limit` requests.

```lua
-- sliding-window-log.lua
-- KEYS[1] = zset
-- ARGV[1] = limit, ARGV[2] = window ms, ARGV[3] = unique request id
local t = redis.call("TIME")
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)
local limit = tonumber(ARGV[1])
local windowMs = tonumber(ARGV[2])

redis.call("ZREMRANGEBYSCORE", KEYS[1], 0, now - windowMs)     -- forget requests outside the window
local count = redis.call("ZCARD", KEYS[1])

if count < limit then
  redis.call("ZADD", KEYS[1], now, ARGV[3])                    -- record this request
  redis.call("PEXPIRE", KEYS[1], windowMs)                     -- idle keys clean themselves up
  return {1, limit - count - 1, 0}
end

local oldest = redis.call("ZRANGE", KEYS[1], 0, 0, "WITHSCORES")
local retryMs = tonumber(oldest[2]) + windowMs - now           -- when the oldest request leaves the window
return {0, 0, retryMs}
```

```ts
import { randomUUID } from "node:crypto";

redis.defineCommand("rlSlidingLog", { numberOfKeys: 1, lua: load("sliding-window-log") });

const [allowed, remaining, retryMs] = await redis.rlSlidingLog(
  `shop:rate:sl:${id}`, 5, 60_000, randomUUID()
);
```

Details worth noticing:

| Detail | Why |
|--------|-----|
| **Server time** (`TIME`) | All app instances share one clock, so clock skew between servers can't distort windows |
| Unique member id (`ARGV[3]`) | Two requests in the same millisecond must not overwrite each other |
| **Only allowed requests are recorded** | A client that hammers while blocked isn't punished forever. Record denied requests too if you *want* penalizing |
| `retryMs` | The exact time until a slot frees, ideal for `Retry-After` |

| Strength | Weakness |
|----------|----------|
| **Exact** | Memory is O(limit) per client (a limit of 10,000/hour keeps 10,000 entries) |
| Precise `Retry-After` | More work per request than a counter |

Use it for **small limits where precision matters**: login attempts, password resets, OTP sends.

## Sliding window counter (the practical middle)

Keep only **two counters** (the current and the previous fixed window) and estimate the sliding count by weighting the previous window by how much of it still overlaps:

```
estimate = previous × (1 − elapsedFractionOfCurrentWindow) + current
```

Example: limit 100/minute. The previous minute had 80 requests, the current minute has 30 so far, and we're 25% into the current minute:

```
estimate = 80 × 0.75 + 30 = 90   → 10 more allowed
```

It smooths the boundary burst while using O(1) memory. One hash holds everything:

```lua
-- sliding-window-counter.lua
-- KEYS[1] = hash {s: window start, c: current count, p: previous count}
-- ARGV[1] = limit, ARGV[2] = window ms
local t = redis.call("TIME")
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)
local limit = tonumber(ARGV[1])
local win = tonumber(ARGV[2])

local start = tonumber(redis.call("HGET", KEYS[1], "s") or "0")
local curr  = tonumber(redis.call("HGET", KEYS[1], "c") or "0")
local prev  = tonumber(redis.call("HGET", KEYS[1], "p") or "0")

local aligned = now - (now % win)                   -- start of the current window
if aligned > start then                             -- we moved into a newer window
  if aligned - start >= 2 * win then prev = 0 else prev = curr end
  curr = 0
  start = aligned
end

local elapsed = now - start
local estimate = prev * (1 - elapsed / win) + curr

if estimate + 1 > limit then
  redis.call("HSET", KEYS[1], "s", start, "c", curr, "p", prev)
  redis.call("PEXPIRE", KEYS[1], win * 2)
  return {0, 0, win - elapsed}                      -- upper bound: time to the next window
end

curr = curr + 1
redis.call("HSET", KEYS[1], "s", start, "c", curr, "p", prev)
redis.call("PEXPIRE", KEYS[1], win * 2)
return {1, math.floor(limit - (estimate + 1)), 0}
```

```ts
redis.defineCommand("rlSlidingCounter", { numberOfKeys: 1, lua: load("sliding-window-counter") });
const [allowed, remaining, retryMs] = await redis.rlSlidingCounter(`shop:rate:sc:${id}`, 100, 60_000);
```

| Strength | Weakness |
|----------|----------|
| O(1) memory, one small hash | An **approximation**: assumes requests in the previous window were evenly spread |
| Smooth, no 2× edge burst | `retryMs` is an upper bound, not exact |

Widely used in practice because the error is small and bounded, and the cost is tiny. Choose it as the **default general-purpose window limiter**.

## Comparison

| | Fixed | Sliding log | Sliding counter |
|---|-------|-------------|-----------------|
| Memory per client | 1 counter | `limit` entries | 1 hash (3 fields) |
| Accuracy | Edge bursts to 2× | **Exact** | Close approximation |
| `Retry-After` | Exact (TTL) | Exact | Upper bound |
| CPU per request | Lowest | Highest | Low |
| Complexity | Lowest | Medium | Medium |
| Good for | Quotas, coarse limits | Login, OTP, small limits | General API limits |

## Responses and headers

Tell clients what happened so they can back off correctly:

```ts
res.set("RateLimit-Limit", String(limit));
res.set("RateLimit-Remaining", String(remaining));
res.set("RateLimit-Reset", String(Math.ceil(resetMs / 1000)));         // seconds until the window resets

if (!allowed) {
  res.set("Retry-After", String(Math.ceil(retryMs / 1000)));
  return res.status(429).json({ error: "too_many_requests", retryAfterSec: Math.ceil(retryMs / 1000) });
}
```

| Header | Meaning |
|--------|---------|
| `429 Too Many Requests` | The right status for rate limiting (not `403` or `503`) |
| `Retry-After` | Seconds to wait before trying again, which well-behaved clients honor |
| `RateLimit-Limit`, `RateLimit-Remaining`, `RateLimit-Reset` | Standardization drafts exist, and many APIs still use `X-RateLimit-*`. Pick one convention and document it |

On the client side, retry after `Retry-After` **plus jitter**, or all rejected clients return at the same instant.

## Counting more than requests

Not every request costs the same. Charge more for expensive operations by adding a **cost**:

```ts
await redis.incrby(key, cost);                       // fixed window: a search costs 5, a read costs 1
```

For the Lua limiters, pass `cost` and compare `count + cost` against the limit. The [bucket limiters](./02_token-and-leaky-bucket.md) take a cost parameter directly.

## Multiple limits together

Real APIs layer limits (a short burst limit, a sustained limit, a daily quota):

```ts
const policies = [
  { name: "burst",  limit: 10,     windowSec: 1 },
  { name: "minute", limit: 100,    windowSec: 60 },
  { name: "day",    limit: 10_000, windowSec: 86_400 },
];
```

Check them from strictest to loosest and reject on the first failure ([Distributed Rate Limiter](./03_distributed-rate-limiter.md#several-limits-at-once)).

## Who to limit

| Identity | Notes |
|----------|-------|
| **IP address** | Easy, but shared NATs and offices make many users look like one. Behind a proxy you **must** configure `trust proxy`, or everyone shares the proxy's IP |
| **IPv6** | One user can own a huge range. Limit by a **/64** prefix instead of the full address |
| **User ID** | Best for authenticated APIs. Apply **after** authentication |
| **API key / tenant** | The natural unit for paid plans |
| **Route** | Include it in the key to give expensive endpoints their own budget |

Never trust `X-Forwarded-For` blindly. Take the client IP from your **trusted proxy chain** only.

## Memory and keys

- Each client gets one key, and **idle keys expire** (TTL equal to the window)
- Attackers rotating identities (random IPs, user IDs) create **many keys**. Keep windows short, keep the keys small, and use a cache-class Redis with a sane `maxmemory` policy ([Memory and Eviction](../02_redis-fundamentals/05_memory-and-eviction.md))
- Build keys with your [key builder](../08_nodejs-integration/05_redis-key-builder.md), and include the algorithm in the name (`rate:sc:…`) so you can change algorithms without key collisions

## Testing windows

```ts
it("allows exactly `limit` requests per window", async () => {
  const results = await Promise.all(
    Array.from({ length: 50 }, () => redis.rlSlidingCounter("test:rate:1", 10, 60_000))
  );
  const allowed = results.filter(([a]) => a === 1).length;
  expect(allowed).toBe(10);                               // concurrency does not leak extra requests
});

it("frees capacity after the window passes", async () => {
  for (let i = 0; i < 3; i++) await redis.rlSlidingLog("test:rate:2", 3, 200, randomUUID());
  const [blocked] = await redis.rlSlidingLog("test:rate:2", 3, 200, randomUUID());
  expect(blocked).toBe(0);

  await sleep(250);
  const [again] = await redis.rlSlidingLog("test:rate:2", 3, 200, randomUUID());
  expect(again).toBe(1);
});
```

Use **short windows** and generous sleeps to avoid flaky tests, and always test with **parallel** calls, because races are the whole point of doing this in Lua.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| `INCR` then a separate `EXPIRE` | Lua, or `MULTI` with `EXPIRE ... NX` |
| Window computed from each app server's clock | Redis `TIME` inside Lua, or one clock source |
| Using fixed windows to protect a fragile backend | Sliding counter, or token bucket |
| Counting rejected requests forever (self-inflicted lockouts) | Record only allowed requests unless penalizing on purpose |
| Limiting by IP behind a proxy without `trust proxy` | Configure the proxy chain |
| Limiting before authentication, then by user | Layer both: IP for anonymous, user ID after login |
| Same cost for cheap and expensive endpoints | Cost-based limits |
| No `Retry-After`, so clients retry immediately | Return it, and add jitter on the client |
| Unbounded key growth from rotating identities | Short TTLs, and a capped cache-class Redis |

## Key takeaways

- **Fixed window** is cheapest, but allows up to 2× bursts at the edges
- **Sliding log** is exact but memory-hungry. **Sliding counter** is the cheap, smooth default
- Always do the check-and-count in **one Lua script**, using Redis server time
- Return `429` with `Retry-After`, and identify clients carefully

**Next:** [Token and Leaky Bucket](./02_token-and-leaky-bucket.md)
