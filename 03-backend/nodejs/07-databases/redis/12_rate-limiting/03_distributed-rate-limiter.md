# Distributed Rate Limiter

The algorithms are the easy part. A production limiter also has to identify clients correctly, return useful responses, survive Redis outages, resist abuse, support plans, and be testable. This lesson assembles the Lua scripts from the previous lessons into an Express-ready, Redis-backed limiter.

## Design

```
request ─► identify client ─► choose policy ─► limiter.check(policy, id, cost)
                                                   │
                                      Redis (Lua, atomic, shared by all instances)
                                                   │
                       allowed ◄─── decision ───► denied → 429 + Retry-After
```

Components:

| Component | Responsibility |
|-----------|----------------|
| **Policy** | Algorithm, limit, window, burst |
| **Limiter** | Runs the right Lua script and returns a `Decision` |
| **Middleware** | Maps requests to client IDs and policies, writes headers and `429`s |
| **Fallback** | Behavior when Redis is unavailable |
| **Penalties** | Escalation for repeat offenders |

## Scripts and commands

Keep the Lua from the previous lessons in one directory and register them once, with the typed declarations shown in [Typed Redis Client](../08_nodejs-integration/04_typed-redis-client.md#typing-custom-commands-lua):

```
src/ratelimit/scripts/
  fixed-window.lua           sliding-window-log.lua     sliding-window-counter.lua
  token-bucket.lua           gcra.lua                   concurrency-acquire.lua
```

```ts
// src/ratelimit/commands.ts
declare module "ioredis" {
  interface RedisCommander<Context> {
    rlFixedWindow(key: string, limit: number, windowSec: number): Promise<[number, number, number]>;
    rlSlidingLog(key: string, limit: number, windowMs: number, id: string): Promise<[number, number, number]>;
    rlSlidingCounter(key: string, limit: number, windowMs: number): Promise<[number, number, number]>;
    rlTokenBucket(key: string, capacity: number, perSec: number, cost: number): Promise<[number, number, number]>;
    rlGcra(key: string, intervalMs: number, burst: number, cost: number): Promise<[number, number, number]>;
  }
}

export function registerRateLimitCommands(redis: Redis) {
  redis.defineCommand("rlFixedWindow",    { numberOfKeys: 1, lua: lua("fixed-window") });
  redis.defineCommand("rlSlidingLog",     { numberOfKeys: 1, lua: lua("sliding-window-log") });
  redis.defineCommand("rlSlidingCounter", { numberOfKeys: 1, lua: lua("sliding-window-counter") });
  redis.defineCommand("rlTokenBucket",    { numberOfKeys: 1, lua: lua("token-bucket") });
  redis.defineCommand("rlGcra",           { numberOfKeys: 1, lua: lua("gcra") });
}
```

(The earlier lessons show `{allowed, remaining, retry}`-style returns. Here every script returns the same triple, `[allowed, remaining, retryMs]`. The fixed-window script returns seconds in the third slot, so the limiter normalizes it below.)

## The limiter

```ts
// src/ratelimit/limiter.ts
import { randomUUID } from "node:crypto";
import type { Redis } from "ioredis";
import { keys } from "../redis/keys.js";

export type Algorithm = "fixed" | "sliding-log" | "sliding-counter" | "token-bucket" | "gcra";

export interface Policy {
  name: string;                    // part of the key and of metric labels
  algo: Algorithm;
  limit: number;                   // requests per window
  windowSec: number;
  burst?: number;                  // token-bucket and gcra: maximum burst (default: limit)
}

export interface Decision {
  allowed: boolean;
  limit: number;
  remaining: number;
  retryAfterMs: number;            // 0 when allowed
  resetMs: number;                 // approximate time until the budget is fully restored
  policy: string;
}

export interface Limiter {
  check(policy: Policy, id: string, cost?: number): Promise<Decision>;
}

export class RedisLimiter implements Limiter {
  constructor(private redis: Redis) {}

  async check(p: Policy, id: string, cost = 1): Promise<Decision> {
    const key = keys.rateLimit(`${p.algo}:${p.name}`, id);
    const windowMs = p.windowSec * 1000;
    let allowed: number, remaining: number, retryMs: number;

    switch (p.algo) {
      case "fixed": {
        let retrySec: number;
        [allowed, remaining, retrySec] = await this.redis.rlFixedWindow(key, p.limit, p.windowSec);
        retryMs = retrySec * 1000;
        break;
      }
      case "sliding-log":
        [allowed, remaining, retryMs] = await this.redis.rlSlidingLog(key, p.limit, windowMs, randomUUID());
        break;
      case "sliding-counter":
        [allowed, remaining, retryMs] = await this.redis.rlSlidingCounter(key, p.limit, windowMs);
        break;
      case "token-bucket":
        [allowed, remaining, retryMs] = await this.redis.rlTokenBucket(
          key, p.burst ?? p.limit, p.limit / p.windowSec, cost
        );
        break;
      case "gcra": {
        const intervalMs = Math.max(1, Math.round(windowMs / p.limit));
        [allowed, remaining, retryMs] = await this.redis.rlGcra(key, intervalMs, p.burst ?? p.limit, cost);
        break;
      }
    }

    const ok = allowed === 1;
    return {
      allowed: ok,
      limit: p.limit,
      remaining,
      retryAfterMs: ok ? 0 : retryMs,
      resetMs: ok ? Math.round(windowMs * (1 - remaining / p.limit)) : retryMs,
      policy: p.name,
    };
  }
}
```

The `Limiter` interface is what the rest of the app depends on. It extends the `RateLimiterPort` idea from [Express Integration](../08_nodejs_integration/06_express-integration.md) with policies and cost, and lets tests inject a fake.

## The Express middleware

```ts
// src/http/middleware/rate-limit.ts
import type { Request, RequestHandler } from "express";
import type { Limiter, Policy, Decision } from "../../ratelimit/limiter.js";

export interface RateLimitOptions {
  limiter: Limiter;
  policy: Policy | ((req: Request) => Policy);          // a function lets you vary by plan
  key: (req: Request) => string | null;                  // null = do not limit this request
  cost?: (req: Request) => number;
  failOpen?: boolean;                                    // default true
  onDenied?: (req: Request, d: Decision) => void;        // metrics and logging
}

export function rateLimit(o: RateLimitOptions): RequestHandler {
  return async (req, res, next) => {
    const id = o.key(req);
    if (id === null) return next();

    const policy = typeof o.policy === "function" ? o.policy(req) : o.policy;

    let d: Decision;
    try {
      d = await o.limiter.check(policy, id, o.cost?.(req) ?? 1);
    } catch (err) {
      req.log?.warn?.({ err, policy: policy.name }, "rate limiter unavailable");
      if (o.failOpen ?? true) return next();               // availability over protection
      return res.set("Retry-After", "5").status(503).json({ error: "service_unavailable" });
    }

    res.set("RateLimit-Limit", String(d.limit));
    res.set("RateLimit-Remaining", String(d.remaining));
    res.set("RateLimit-Reset", String(Math.ceil(d.resetMs / 1000)));

    if (d.allowed) return next();

    o.onDenied?.(req, d);
    res.set("Retry-After", String(Math.max(1, Math.ceil(d.retryAfterMs / 1000))));
    return res.status(429).json({ error: "too_many_requests", retryAfterSec: Math.ceil(d.retryAfterMs / 1000) });
  };
}
```

Wire it up:

```ts
const limiter = new RedisLimiter(redis);

// anonymous traffic: by IP
app.use("/api", rateLimit({
  limiter,
  policy: { name: "anon", algo: "sliding-counter", limit: 60, windowSec: 60 },
  key: (req) => (req.user ? null : ipKey(req)),           // authenticated users are handled below
}));

// authenticated traffic: by user, after authentication middleware
app.use("/api", authenticate, rateLimit({
  limiter,
  policy: (req) => PLANS[req.user!.plan] ?? PLANS.free,
  key: (req) => (req.user ? `u:${req.user.id}` : null),
  cost: (req) => COST[`${req.method} ${req.route?.path}`] ?? 1,
}));
```

## Choosing the client identity

```ts
// IPv6 users can own enormous ranges: group by /64
export function ipKey(req: Request): string {
  const ip = req.ip ?? "unknown";                        // correct only if `trust proxy` is configured
  if (ip.includes(":") && !ip.startsWith("::ffff:")) {
    return "ip6:" + ip.split(":").slice(0, 4).join(":"); // approximate /64 (expand "::" in real code)
  }
  return "ip:" + ip.replace("::ffff:", "");
}
```

Use a proper IP library for real IPv6 normalization (compressed `::` forms change the segment count).

Rules:

- Set `app.set("trust proxy", <your proxy hops or subnet>)`, or `req.ip` is the load balancer and **everyone shares one bucket**
- Don't read `X-Forwarded-For` yourself. Let Express resolve it using the trusted proxy setting
- Apply user-level limits **after** authentication, and IP limits **before** it
- For API products, limit by **API key** or **tenant**, and keep the IP limit as an outer guard

## Several limits at once

Check from **strictest to loosest**, and stop at the first denial:

```ts
const LAYERS: Policy[] = [
  { name: "burst",  algo: "token-bucket",    limit: 20,     windowSec: 1,      burst: 20 },
  { name: "minute", algo: "sliding-counter", limit: 300,    windowSec: 60 },
  { name: "day",    algo: "fixed",           limit: 50_000, windowSec: 86_400 },
];

export function layered(limiter: Limiter, layers: Policy[]): Limiter {
  return {
    async check(_p, id, cost = 1) {
      let last!: Decision;
      for (const policy of layers) {
        last = await limiter.check(policy, id, cost);
        if (!last.allowed) return last;                   // report the layer that blocked
      }
      return last;
    },
  };
}
```

A request that passes the first layer and fails the second has still **consumed** from the first. That small over-count is usually acceptable. If you need all-or-nothing, put the layers in **one Lua script** over several keys sharing a hash tag (`{u:42}:rate:burst`, `{u:42}:rate:minute`).

## Per-plan policies

```ts
export const PLANS: Record<string, Policy> = {
  free:       { name: "free",       algo: "sliding-counter", limit: 60,     windowSec: 60 },
  pro:        { name: "pro",        algo: "token-bucket",    limit: 600,    windowSec: 60, burst: 100 },
  enterprise: { name: "enterprise", algo: "gcra",            limit: 6_000,  windowSec: 60, burst: 500 },
};
```

Include the **plan or policy name in the key** so changing a user's plan doesn't mix counters (`rate:token-bucket:pro:u:42`). To change limits without a deploy, keep plan definitions in a Redis hash or your database and **cache them in memory for a few seconds**.

## Failure policy: Redis is down

| Route type | Policy | Reason |
|------------|--------|--------|
| Normal read APIs | **Fail open** | A limiter outage shouldn't become a full outage |
| Login, OTP, password reset, signup | **Fail closed**, or fall back to a local limiter | Protection matters more than availability |
| Expensive or costly endpoints | **Fail closed** or local fallback | Prevents runaway cost |
| Webhooks from trusted partners | Fail open | Dropping them is worse |

### A local fallback

When Redis is unreachable, an **in-memory per-instance** limiter is far better than nothing. Divide the limit by the expected instance count:

```ts
export class LocalTokenBucket {
  private buckets = new Map<string, { tokens: number; ts: number }>();
  constructor(private capacity: number, private perSec: number, private maxKeys = 10_000) {}

  take(id: string, cost = 1): { allowed: boolean; retryMs: number } {
    const now = Date.now();
    const b = this.buckets.get(id) ?? { tokens: this.capacity, ts: now };
    b.tokens = Math.min(this.capacity, b.tokens + ((now - b.ts) / 1000) * this.perSec);
    b.ts = now;

    const allowed = b.tokens >= cost;
    if (allowed) b.tokens -= cost;

    this.buckets.delete(id);                              // re-insert to keep Map ordered by recency
    this.buckets.set(id, b);
    if (this.buckets.size > this.maxKeys) this.buckets.delete(this.buckets.keys().next().value!);

    return { allowed, retryMs: allowed ? 0 : Math.ceil(((cost - b.tokens) / this.perSec) * 1000) };
  }
}
```

```ts
const local = new LocalTokenBucket(10, 100 / 60 / 4);       // assume ~4 instances share the limit
// in the middleware's catch block:
const r = local.take(id);
if (!r.allowed) return res.set("Retry-After", String(Math.ceil(r.retryMs / 1000))).status(429).end();
return next();
```

Pair it with a short `commandTimeout` and a [circuit breaker](../07_caching/04_cache-problems.md#cause-3-redis-is-down-or-slow), so a slow Redis doesn't add latency to every request.

## Protecting Redis from the attacker

A flood of denied requests still costs a Redis round trip each. Remember each client's **denial** in memory and short-circuit it:

```ts
const denyUntil = new Map<string, number>();              // id → epoch ms

function shortCircuit(id: string): number {
  const until = denyUntil.get(id);
  if (!until) return 0;
  if (until <= Date.now()) { denyUntil.delete(id); return 0; }
  return until - Date.now();
}

function rememberDenial(id: string, retryAfterMs: number) {
  if (denyUntil.size > 50_000) denyUntil.clear();          // crude bound, since this is only an optimization
  denyUntil.set(id, Date.now() + retryAfterMs);
}
```

In the middleware, check `shortCircuit(id)` first (respond `429` without touching Redis) and call `rememberDenial` after each denial. Each instance then absorbs repeat offenders locally. The cost is that a client may stay blocked slightly past its true reset, bounded by `retryAfterMs`.

## Escalation: strikes and temporary bans

Persistent offenders deserve a longer penalty than "wait 60 seconds":

```ts
async function recordViolation(id: string) {
  const strikeKey = keys.rateLimit("strikes", id);
  const res = await redis.multi()
    .incr(strikeKey)
    .expire(strikeKey, 3600, "NX")                        // strikes decay after an hour
    .exec();
  const strikes = res![0]![1] as number;

  if (strikes >= 5) await redis.set(keys.rateLimit("ban", id), "1", "EX", 900);    // 15-minute ban
}

async function isBanned(id: string) {
  return (await redis.exists(keys.rateLimit("ban", id))) === 1;
}
```

Check bans **before** the limiter. With [auto pipelining](../06_advanced-commands/01_pipelines-and-auto-pipelining.md#auto-pipelining), the `exists` and the limiter call issued together travel in one round trip. Keep bans **short and automatic**, log them, and give legitimate users a way to recover.

## Protecting login and other abuse targets

Credential stuffing needs **two** limits, because attackers rotate IPs and target single accounts:

```ts
app.post("/login",
  rateLimit({ limiter, failOpen: false,
    policy: { name: "login-ip", algo: "sliding-log", limit: 10, windowSec: 600 },
    key: (req) => ipKey(req) }),

  rateLimit({ limiter, failOpen: false,
    policy: { name: "login-user", algo: "sliding-log", limit: 5, windowSec: 600 },
    key: (req) => `user:${String(req.body?.email ?? "").toLowerCase()}` }),

  loginHandler
);
```

Notes:

- A **per-account** limit protects accounts from guessing. It also lets an attacker **lock a victim out** by hammering their email, so prefer **slowing down** (progressive delays, CAPTCHA, step-up verification) over hard lockout
- **Count only failures** for login if you can: increment the limiter on failed attempts and reset on success
- Return the **same response** for unknown and known accounts so the limiter doesn't become an enumeration oracle
- Use the exact `sliding-log` algorithm here, since the limits are small and precision matters
- Fail **closed** (or use the local fallback) for these routes

## Limiting outbound calls

Use the same limiter to **respect a third party's limit** when calling out. Instead of rejecting, **wait**:

```ts
export async function acquire(limiter: Limiter, policy: Policy, id: string, maxWaitMs = 10_000) {
  const deadline = Date.now() + maxWaitMs;
  for (;;) {
    const d = await limiter.check(policy, id);
    if (d.allowed) return;
    const wait = d.retryAfterMs + Math.random() * 50;     // jitter prevents synchronized wake-ups
    if (Date.now() + wait > deadline) throw new Error("rate limit wait exceeded");
    await sleep(wait);
  }
}

await acquire(limiter, { name: "partner-api", algo: "gcra", limit: 5, windowSec: 1 }, "global");
await callPartner();
```

Because the state is in Redis, **all instances share one budget**, which is the whole point of a distributed limiter.

## Operations

### Latency and cost

- Each check is **one Redis round trip** (a single Lua call), so keep Redis close to the app and use a **short `commandTimeout`** (tens of milliseconds) on the client the limiter uses
- Use a Redis suited to ephemeral keys, with a `maxmemory` policy that tolerates eviction (limiter state is rebuildable, and losing it only resets a client's budget)
- Hot keys (a very popular API key) concentrate load on one slot, so keep that in mind in Cluster

### Cluster

Each script call touches **one key**, so it works unchanged. Multi-key scripts (layered, all-or-nothing) need keys sharing a hash tag ([Key Design](../05_key-management/01_key-design.md#cluster-safe-keys-and-hash-tags)).

### Metrics to track

| Metric | Why |
|--------|-----|
| Allowed and denied counts **per policy** | The basic signal, and spikes show attacks or buggy clients |
| Top denied clients (sampled) | Find offenders |
| Limiter latency | It sits on every request |
| **Fail-open count** | Your protection was off for those requests |
| Local-fallback activations | Redis trouble |
| Ban counts | Escalation health |

Log denials **sampled**, with policy and (hashed) client ID, never full tokens or secrets.

### Configuration hygiene

- Keep limits in **config**, not scattered literals
- Document every limit (who, how much, why) next to the code
- Load-test the limiter itself, because it is on the hot path

## Testing

**Middleware (no Redis):** inject a fake limiter.

```ts
it("returns 429 with Retry-After when denied", async () => {
  const limiter: Limiter = {
    check: async (p) => ({ allowed: false, limit: 10, remaining: 0, retryAfterMs: 7_200, resetMs: 7_200, policy: p.name }),
  };
  const app = makeApp({ limiter });
  const res = await request(app).get("/api/ping");
  expect(res.status).toBe(429);
  expect(res.headers["retry-after"]).toBe("8");                    // rounded up
  expect(res.headers["ratelimit-remaining"]).toBe("0");
});

it("fails open when the limiter throws, if configured", async () => {
  const limiter: Limiter = { check: async () => { throw new Error("redis down"); } };
  const res = await request(makeApp({ limiter })).get("/api/ping");
  expect(res.status).toBe(200);
});

it("fails closed on login routes", async () => {
  const limiter: Limiter = { check: async () => { throw new Error("redis down"); } };
  const res = await request(makeApp({ limiter })).post("/login").send({ email: "a@x.com" });
  expect(res.status).toBe(503);
});
```

**Scripts (real Redis):** concurrency and timing.

```ts
it("never lets concurrent callers exceed the limit", async () => {
  const limiter = new RedisLimiter(redis);
  const policy: Policy = { name: "t", algo: "gcra", limit: 10, windowSec: 60, burst: 10 };
  const id = randomUUID();

  const results = await Promise.all(Array.from({ length: 100 }, () => limiter.check(policy, id)));
  expect(results.filter((r) => r.allowed)).toHaveLength(10);
});
```

Run the same property test for **every algorithm**, and run a **multi-instance** test (two limiter objects, two connections, one Redis) to prove the budget is shared. Also test the **Redis-down** path by disconnecting the client mid-test.

## Production checklist

- [ ] `trust proxy` configured, and the client identity verified behind your real load balancer
- [ ] IPv6 grouped by prefix
- [ ] Limits per **route class**, with **cost** for expensive endpoints
- [ ] `429` with `Retry-After` and `RateLimit-*` headers, documented for API consumers
- [ ] Fail-open versus fail-closed chosen **per route**, with a local fallback for sensitive ones
- [ ] Login and reset endpoints limited by IP **and** account, with progressive friction, not just lockout
- [ ] Short `commandTimeout` on the limiter's Redis client
- [ ] Metrics: allowed, denied, latency, fail-open count
- [ ] Idle keys expire, with a `maxmemory` policy and sane key cardinality
- [ ] Edge or CDN protection in front for volumetric abuse
- [ ] Tests: concurrency, multi-instance, Redis-down, header correctness

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Every user behind a proxy shares one bucket | Configure `trust proxy` |
| Limiter outage takes the whole API down | Fail open (or a local fallback) on non-sensitive routes |
| Failing open on login | Fail closed or use a local limiter |
| A per-account lockout used as the only defense | Progressive delays, CAPTCHA, IP limits, alerts |
| A denied flood still hammers Redis | In-memory short-circuit of known denials |
| Counters mixed across plans or algorithms | Put the policy name in the key |
| `Retry-After` missing or zero | At least 1 second, rounded up, and clients should add jitter |
| Limits spread as magic numbers | One config module, documented |
| Layered limits that over-consume | Accept it, or use one multi-key script with hash tags |
| No tests under concurrency | A parallel-requests test for every algorithm |
| Treating the limiter as the only protection | Layer it with edge protection, timeouts and capacity planning |

## Key takeaways

- One `Limiter` interface, with Lua-backed algorithms behind it, keeps the app testable and the policy flexible
- Identify clients carefully (`trust proxy`, IPv6 prefixes, user versus IP), and layer limits
- Decide **fail-open or fail-closed per route**, and keep a local fallback for sensitive ones
- Protect Redis and your login endpoints from floods, escalate repeat offenders, and monitor denials
- Test for **concurrency correctness**, shared budgets across instances, and the Redis-down path

**Next module:** [13_session-management](../13_session-management/README.md)
