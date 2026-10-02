# Token and Leaky Bucket

Window limiters count requests in a period. **Bucket limiters** model a **rate**: capacity that refills (or drains) continuously. They are the better fit when you want to **allow short bursts while enforcing a steady average**.

## Token bucket

A bucket holds up to `capacity` tokens and **refills at `rate` tokens per second**. Each request removes tokens (its **cost**). If there aren't enough, the request is rejected.

```
        refill: r tokens/sec
              │
        ┌─────▼─────┐
        │ ● ● ● ● ● │  capacity B (max burst)
        │ ● ● ●     │
        └─────┬─────┘
              │  each request takes `cost` tokens
              ▼
        allowed if enough tokens, else 429
```

Behavior:

- A client idle for a while has a **full bucket**, so it can send a burst of up to `capacity` requests at once
- After the burst, it is limited to the **refill rate** on average
- `capacity` controls burstiness, `rate` controls sustained throughput

Example: "100 requests per minute, bursts up to 20" means `rate = 100/60 ≈ 1.67 tokens/s`, `capacity = 20`.

### Lazy refill: no timers

You never run a background job that adds tokens. Instead, when a request arrives, compute how many tokens **would have been added** since the last request:

```
tokens = min(capacity, tokens + elapsed × rate)
```

State per client is just two numbers: `tokens` and the last update `timestamp`.

### Implementation in Lua

```lua
-- token-bucket.lua
-- KEYS[1] = hash {tokens, ts}
-- ARGV[1] = capacity, ARGV[2] = refill tokens per second, ARGV[3] = cost
local t = redis.call("TIME")
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)   -- ms, server clock

local capacity = tonumber(ARGV[1])
local rate = tonumber(ARGV[2]) / 1000                                    -- tokens per ms
local cost = tonumber(ARGV[3])

local data = redis.call("HMGET", KEYS[1], "tokens", "ts")
local tokens = tonumber(data[1])
local ts = tonumber(data[2])
if tokens == nil then                                                    -- first request: start with a full bucket
  tokens = capacity
  ts = now
end

local elapsed = math.max(0, now - ts)
tokens = math.min(capacity, tokens + elapsed * rate)                     -- lazy refill

local allowed = 0
local retryMs = 0
if tokens >= cost then
  tokens = tokens - cost
  allowed = 1
else
  retryMs = math.ceil((cost - tokens) / rate)                            -- time until enough tokens exist
end

redis.call("HSET", KEYS[1], "tokens", tokens, "ts", now)
redis.call("PEXPIRE", KEYS[1], math.ceil(capacity / rate) * 2)           -- an idle bucket is equal to a fresh one
return {allowed, math.floor(tokens), retryMs}
```

```ts
redis.defineCommand("rlTokenBucket", { numberOfKeys: 1, lua: load("token-bucket") });

async function takeTokens(id: string, capacity: number, perSec: number, cost = 1) {
  if (perSec <= 0) throw new Error("refill rate must be positive");
  const [allowed, remaining, retryMs] = await redis.rlTokenBucket(`shop:rate:tb:${id}`, capacity, perSec, cost);
  return { allowed: allowed === 1, remaining, retryMs };
}

// 100 / minute with a burst of 20
const r = await takeTokens(userId, 20, 100 / 60);
```

Design notes:

| Detail | Why |
|--------|-----|
| Refill computed from **Redis server time** | All app instances agree, with no client clock skew |
| `math.max(0, now - ts)` | Guards against a clock step backwards |
| The key **expires** after twice the full-refill time | A bucket idle that long would be full anyway, so deleting it changes nothing, and it saves memory |
| A rejected request **does not consume tokens** | But it does update `ts`, which is harmless because refill is continuous |
| `cost` parameter | Expensive operations take more tokens |
| Rate validated `> 0` in Node | Avoids a divide-by-zero in the script |

Numbers stored in hashes are converted to strings with limited precision (about 14 significant digits), which is plenty for millisecond timestamps (13 digits) today.

### Cost-based limiting

```ts
const COST: Record<string, number> = { "GET /api/products": 1, "GET /api/search": 5, "POST /api/export": 25 };
const cost = COST[`${req.method} ${req.route?.path}`] ?? 1;
await takeTokens(userId, 100, 100 / 60, cost);
```

One bucket then covers a mixed workload fairly, which beats separate counters per endpoint.

### Choosing capacity and rate

| Goal | `capacity` | `rate` |
|------|------------|--------|
| Strict smoothing | Small (1 to 2) | Target rate |
| Normal API, tolerate bursts | 10 to 20% of the per-minute limit | `limit / 60` per second |
| Allow big bursts, enforce a daily average | Large | `daily limit / 86,400` |

Remember `capacity` is also the **cold-start allowance**: every new client starts full.

## GCRA: a token bucket in one number

The **Generic Cell Rate Algorithm** gives the same behavior as a token bucket but stores a single timestamp per client: the **theoretical arrival time** (TAT), when the bucket would be "exactly empty".

- `T` = the emission interval, the time one request "costs" (`1 / rate`)
- `τ` = how far ahead of schedule a client may run = `T × burst`

A request at time `now` is allowed if `max(TAT, now) + cost×T − τ ≤ now`. On success, TAT moves forward by `cost × T`.

```lua
-- gcra.lua
-- KEYS[1] = TAT key
-- ARGV[1] = T (emission interval, ms), ARGV[2] = burst size, ARGV[3] = cost
local t = redis.call("TIME")
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)

local T = tonumber(ARGV[1])
local burst = tonumber(ARGV[2])
local cost = tonumber(ARGV[3])
local tau = T * burst

local tat = tonumber(redis.call("GET", KEYS[1]) or "0")
if tat < now then tat = now end

local newTat = tat + cost * T
local allowAt = newTat - tau

if now < allowAt then
  return {0, 0, math.ceil(allowAt - now)}                                 -- denied: wait this many ms
end

redis.call("SET", KEYS[1], newTat, "PX", math.ceil(newTat - now))         -- expires when the bucket would be empty
return {1, math.floor((now + tau - newTat) / T), 0}
```

```ts
redis.defineCommand("rlGcra", { numberOfKeys: 1, lua: load("gcra") });

// 100 per minute, burst 20  →  T = 600 ms
const T = Math.max(1, Math.round(60_000 / 100));
const [allowed, remaining, retryMs] = await redis.rlGcra(`shop:rate:gcra:${id}`, T, 20, 1);
```

| Strength | Weakness |
|----------|----------|
| **One key, one small string**, minimal memory | Harder to read than a token bucket |
| Exact, atomic, one `GET` and `SET` | Millisecond resolution means rates above about 1,000 per second per key aren't expressible without microsecond units |
| Idle keys expire automatically | The "remaining" value is derived, not stored |

Choose GCRA when you have **millions of keys**, or high request volume, and token-bucket semantics. The `redis-cell` module offers a native `CL.THROTTLE` command with the same algorithm if your Redis supports modules.

## Leaky bucket

The leaky bucket has two meanings. Know which one you want.

### 1. Leaky bucket as a meter

Requests **fill** a bucket and it **drains at a constant rate**. If a request would overflow it, reject it.

```
requests ─► ┌─────────┐
            │░░░░░░░░░│  level rises with each request
            │░░░░     │  drains steadily at `rate`
            └────┬────┘
                 ▼ constant leak
```

This is the **mirror image of a token bucket** (level = capacity − tokens), and GCRA is exactly this model in one number. The practical difference is only in framing: token buckets talk about **allowance**, leaky meters talk about **backlog**. Both permit a burst up to the bucket size, then enforce a steady rate.

### 2. Leaky bucket as a queue (traffic shaping)

Requests are **queued**, and a worker releases them at a **constant rate**. Nothing is rejected until the queue is full. This **smooths** traffic instead of just policing it, which is what you want for **outbound calls to a third party with a hard rate limit**.

```
callers ──push──► [ queue, max N ] ──pop every 1/rate s──► third-party API
                        │
                full? → reject / shed
```

```lua
-- enqueue-bounded.lua
-- KEYS[1] = list; ARGV[1] = max length, ARGV[2] = payload
if redis.call("LLEN", KEYS[1]) >= tonumber(ARGV[1]) then
  return 0                                         -- bucket full: reject
end
return redis.call("LPUSH", KEYS[1], ARGV[2])
```

```ts
// one drain loop (run it on ONE instance: a lock or leader election, see module 11)
const intervalMs = 1000 / 5;                      // 5 per second
while (running) {
  const job = await redis.rpop("shop:q:partner-api");
  if (job) await callPartner(JSON.parse(job));
  await sleep(intervalMs);
}
```

If you already use [BullMQ](../14_queues-and-workers/README.md), its worker **`limiter`** option (maximum jobs per duration) does this for you, with retries, delays and visibility. Prefer it over hand-rolling queue shaping.

## Token bucket vs leaky bucket vs windows

| | Fixed window | Sliding window | Token bucket / GCRA | Leaky queue |
|---|---|---|---|---|
| Allows bursts | 2× at edges | No | **Yes, up to capacity** | **No** (smoothed) |
| Enforces average | Roughly | Exactly | **Yes** | **Exactly** |
| Output is smooth | No | No | No | **Yes** |
| Memory per key | O(1) | O(limit) or O(1) | O(1) | O(queue) |
| Rejects or delays | Rejects | Rejects | Rejects | **Delays**, rejects when full |
| Typical use | Quotas | Login limits | **API limits** | Outbound shaping |

## Concurrency limiting (a different question)

Rate limits cap **how often**. Sometimes you need to cap **how many at once**: "at most 3 concurrent exports per user". Track **in-flight leases** with expiry in a sorted set, so crashed holders free their slot automatically:

```lua
-- concurrency-acquire.lua
-- KEYS[1] = zset of active leases (score = expiry ms)
-- ARGV[1] = max concurrent, ARGV[2] = lease ms, ARGV[3] = holder id
local t = redis.call("TIME")
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)

redis.call("ZREMRANGEBYSCORE", KEYS[1], "-inf", now)           -- drop expired leases (crashed holders)
if redis.call("ZCARD", KEYS[1]) >= tonumber(ARGV[1]) then
  return 0
end
redis.call("ZADD", KEYS[1], now + tonumber(ARGV[2]), ARGV[3])
redis.call("PEXPIRE", KEYS[1], tonumber(ARGV[2]))
return 1
```

```ts
redis.defineCommand("rlAcquireSlot", { numberOfKeys: 1, lua: load("concurrency-acquire") });

async function withSlot<T>(userId: string, max: number, leaseMs: number, fn: () => Promise<T>) {
  const key = `shop:rate:conc:${userId}`;
  const holder = randomUUID();
  if ((await redis.rlAcquireSlot(key, max, leaseMs, holder)) !== 1) throw new TooManyConcurrentError();
  try {
    return await fn();
  } finally {
    await redis.zrem(key, holder);                             // release
  }
}
```

The lease means a crashed worker's slot returns after `leaseMs`. Make it longer than the longest legitimate job, or **renew** it (the same watchdog idea as [lock renewal](../11_distributed-locks/02_lock-expiration-and-renewal.md)). It is a **semaphore**, and the [lock lessons](../11_distributed-locks/README.md) apply.

## Testing buckets

```ts
it("allows a burst up to capacity, then enforces the rate", async () => {
  const id = randomUUID();
  const burst = await Promise.all(Array.from({ length: 30 }, () => takeTokens(id, 20, 10)));
  expect(burst.filter((r) => r.allowed)).toHaveLength(20);          // exactly the capacity

  await sleep(350);                                                  // ~3.5 tokens refill at 10/s
  const after = await Promise.all(Array.from({ length: 10 }, () => takeTokens(id, 20, 10)));
  const ok = after.filter((r) => r.allowed).length;
  expect(ok).toBeGreaterThanOrEqual(3);
  expect(ok).toBeLessThanOrEqual(4);
});

it("reports a usable retry time", async () => {
  const id = randomUUID();
  await takeTokens(id, 1, 1);                                        // empties the bucket
  const denied = await takeTokens(id, 1, 1);
  expect(denied.allowed).toBe(false);
  expect(denied.retryMs).toBeGreaterThan(0);
  expect(denied.retryMs).toBeLessThanOrEqual(1000);
});
```

For GCRA, assert the same properties, and compare the decision sequence against a reference in-memory implementation using random timestamps (a property-style test).

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Huge `capacity` "to be safe" | It is also the burst allowance, so size it deliberately |
| Zero or negative rate | Validate in Node before calling the script |
| Client clocks in the refill math | Redis `TIME` inside the script |
| Keeping idle buckets forever | `PEXPIRE` of about twice the full-refill time |
| Using a token bucket when you need smooth output | Leaky **queue**, or BullMQ's limiter |
| Draining a leaky-bucket queue from many instances at once | One drain loop (lock or leader), or a queue library |
| GCRA at more than ~1,000 req/s per key with ms units | Use microsecond units, or a token bucket |
| Semaphore leases without expiry | Lease TTL plus renewal |
| One cost for all endpoints | Cost-based tokens |

## Key takeaways

- A **token bucket** allows bursts up to its capacity and enforces a steady average, with lazy refill and just two numbers of state
- **GCRA** does the same with **one** value per client
- A **leaky bucket** meter is the mirror of a token bucket, and a leaky **queue** shapes outgoing traffic
- Use a **semaphore with leases** to cap concurrency
- Always keep the logic atomic in Lua, on Redis server time

**Next:** [Distributed Rate Limiter](./03_distributed-rate-limiter.md)
