# Scalable API and Rate Limiter

Two linked problems: how do you make an API handle growing traffic, and how do you stop any single client from overwhelming it? The answers lean heavily on earlier files — `15-performance/04-load-balancing-and-testing.md`, `08-authentication-security/06-rate-limiting.md`, and `07-databases/redis/`.

---

# Part 1 — Designing a scalable API

## Requirements

**Functional:** a REST API serving authenticated clients (`09-api-development/`).

**Non-functional:**
- Handle ~5,000 requests/second at peak, growing over time
- p95 latency under 200 ms
- High availability — a single machine failure must not cause an outage
- Safe to deploy without downtime

## Scale estimate

Say 50 million requests/day:
- Average: 50,000,000 ÷ 86,400 ≈ **~580 requests/second**
- Peak at ~8× average: **~4,600 requests/second**
- Read/write ratio assumed 90/10 → reads dominate, so caching helps a lot

One Node.js process can serve a lot of simple requests, but not 5,000/sec of database-backed work with headroom. We need multiple instances.

## High-level design

```
              ┌──────────┐
 Clients ───► │   CDN    │  (static assets, cacheable GETs)
              └────┬─────┘
                   ▼
            ┌──────────────┐
            │ Load balancer│  (Nginx / ALB — health checks, TLS)
            └──┬────┬────┬─┘
               ▼    ▼    ▼
            ┌────┐┌────┐┌────┐
            │API ││API ││API │   stateless Node.js instances
            └─┬──┘└─┬──┘└─┬──┘
              │     │     │
        ┌─────┴─────┴─────┴──────┐
        ▼                        ▼
   ┌─────────┐             ┌───────────┐
   │  Redis  │             │ Database  │
   │ (cache, │             │ primary + │
   │ sessions│             │ replicas  │
   │ limits) │             └───────────┘
   └─────────┘
```

## The key principle: stateless instances

Horizontal scaling only works if **any instance can handle any request**. That means no per-user state in process memory:

| State | Wrong place | Right place |
|-------|-------------|-------------|
| Sessions | In-memory `Map` | Redis, or stateless JWTs (`08-authentication-security/02-jwt-and-tokens.md`, `03-sessions.md`) |
| Cache | Per-process object | Redis (shared) |
| Uploaded files | Local disk | Object storage like S3 (`05-file-upload-system.md`) |
| Scheduled jobs | `setInterval` in each instance | A single scheduler / queue (`06-job-processing-system.md`) |
| Rate-limit counters | In-memory counter | Redis (see Part 2) |

Test of statelessness: kill any instance at random — does any user notice?

## Scaling each layer

**Compute — scale out.** Run N identical instances behind the load balancer. Within a machine, use the `cluster` module, PM2, or one container per core (`02-core-modules/10-cluster-and-worker-threads.md`). Scale on CPU or request-rate metrics.

**Load balancing.** Round-robin or least-connections. The balancer runs health checks (`16-production/02-graceful-shutdown-and-health-checks.md`) and stops sending traffic to an unhealthy instance. With stateless instances, **no sticky sessions** are needed (WebSockets are the exception — see `03-chat-system.md`).

**Caching — cut the load before it hits the database.**
- CDN for static assets and public cacheable GETs (`05-http-web/05-caching-and-compression.md`)
- Redis for hot data (cache-aside pattern):

```js
async function getProduct(id) {
  const key = `product:${id}`;
  const cached = await redis.get(key);
  if (cached) return JSON.parse(cached);

  const product = await db.products.findById(id);
  await redis.set(key, JSON.stringify(product), { EX: 300 });  // TTL bounds staleness
  return product;
}
```

**Database — scale reads first.**
1. Add proper indexes (`07-databases/*/`, `15-performance/03-database-optimization.md`) — usually the biggest win
2. Use a connection pool and size it against `instances × poolSize ≤ database max connections`
3. Add **read replicas** for read-heavy traffic (accept replica lag for non-critical reads)
4. Only then consider **sharding** (splitting data across databases) — it adds large complexity

**Move slow work off the request path.** Emails, report generation, and webhooks go to a queue and return `202 Accepted` immediately (`11-async-processing/`).

**Protect downstream dependencies.** Set timeouts on every outbound call; use retries with backoff; add circuit breakers so one slow service doesn't cascade into a full outage.

## Failure modes to discuss

- **Instance dies** — load balancer removes it; others absorb the load (keep headroom, e.g. run at ≤ 60% capacity)
- **Redis down** — decide the fallback: serve from the database (slower but correct) or degrade features. Don't let a cache outage become an API outage.
- **Cache stampede** — a popular key expires and thousands of requests hit the database at once. Mitigate with jittered TTLs and a lock so only one request rebuilds the value.
- **Database failover** — plan for a brief window of errors; clients should retry idempotent requests

---

# Part 2 — Designing a distributed rate limiter

## Why rate limit?

- Protect against abuse and accidental floods
- Ensure fairness between clients
- Control costs of expensive operations
- Defend login and other sensitive endpoints against brute force (`08-authentication-security/06-rate-limiting.md`)

## Requirements

- Limit per client (API key, user ID, or IP) — e.g. **100 requests/minute**
- Add **very little latency** to each request (it runs on every call)
- Work **correctly across many API instances**
- Return `429 Too Many Requests` with a `Retry-After` header

## Why in-memory limiting breaks when you scale

If each of 4 instances keeps its own counter, a client can make 100 requests to *each* — an effective limit of 400. State must be **shared**, which is why Redis is the standard choice: fast, atomic operations, and built-in expiry.

## Choosing an algorithm

| Algorithm | How it works | Pros | Cons |
|-----------|--------------|------|------|
| **Fixed window** | Counter per time window (e.g. per minute), reset at the boundary | Simplest, very cheap | Boundary burst: 100 at 12:00:59 + 100 at 12:01:00 = 200 in 2 seconds |
| **Sliding window log** | Store a timestamp per request; count those in the last N seconds | Accurate | Memory-heavy per client |
| **Sliding window counter** | Weighted blend of current and previous window counts | Good accuracy, low memory | Approximate |
| **Token bucket** | Bucket refills at a steady rate; each request spends a token | Allows controlled bursts; smooth average rate | Slightly more state |
| **Leaky bucket** | Requests drain at a fixed rate (queue-like) | Smooth output rate | Doesn't favor legitimate bursts |

Token bucket is the common default: it allows short bursts while enforcing an average rate.

## Fixed window in Redis — the simple version

```js
async function fixedWindowLimit(clientId, limit = 100, windowSec = 60) {
  const windowId = Math.floor(Date.now() / 1000 / windowSec);
  const key = `rl:${clientId}:${windowId}`;

  const count = await redis.incr(key);
  if (count === 1) await redis.expire(key, windowSec);   // first hit sets expiry

  return { allowed: count <= limit, remaining: Math.max(0, limit - count) };
}
```

`INCR` is atomic, so concurrent requests from different instances never lose counts.

**Subtle bug:** `INCR` then `EXPIRE` are two separate commands. If the process dies between them, the key never expires and the client is blocked forever. Fix it by making both steps one atomic unit — a Lua script (below) or a `MULTI` transaction.

## Token bucket with an atomic Lua script

The check-and-update must be **atomic**: read tokens, refill, subtract, write back. Doing that as separate round trips creates a race between instances. Redis runs a Lua script as a single atomic operation:

```js
const TOKEN_BUCKET_LUA = `
local key       = KEYS[1]
local capacity  = tonumber(ARGV[1])
local refillPerSec = tonumber(ARGV[2])
local now       = tonumber(ARGV[3])   -- milliseconds
local cost      = tonumber(ARGV[4])

local data   = redis.call("HMGET", key, "tokens", "ts")
local tokens = tonumber(data[1])
local ts     = tonumber(data[2])

if tokens == nil then
  tokens = capacity
  ts = now
end

-- refill based on elapsed time
local elapsed = math.max(0, now - ts) / 1000
tokens = math.min(capacity, tokens + elapsed * refillPerSec)

local allowed = 0
if tokens >= cost then
  tokens = tokens - cost
  allowed = 1
end

redis.call("HSET", key, "tokens", tokens, "ts", now)
redis.call("PEXPIRE", key, math.ceil(capacity / refillPerSec * 1000) * 2)

return { allowed, math.floor(tokens) }
`;

export async function tokenBucket(clientId, capacity = 100, refillPerSec = 100 / 60) {
  const [allowed, remaining] = await redis.eval(TOKEN_BUCKET_LUA, {
    keys: [`tb:${clientId}`],
    arguments: [String(capacity), String(refillPerSec), String(Date.now()), "1"],
  });
  return { allowed: allowed === 1, remaining };
}
```

(The `eval` call signature differs between Redis client libraries — the shape above matches `node-redis` v4+; check your client's docs. In production, load scripts once with `SCRIPT LOAD`/`EVALSHA` rather than sending the source each time.)

Note on the clock: using the app server's `Date.now()` assumes instance clocks are close. For stricter correctness, use Redis's own `TIME` inside the script so all instances share one clock.

## Express middleware

```js
export function rateLimit({ capacity = 100, refillPerSec = 100 / 60 } = {}) {
  return async (req, res, next) => {
    const clientId = req.user?.id ?? req.ip;       // prefer identity over IP
    try {
      const { allowed, remaining } = await tokenBucket(clientId, capacity, refillPerSec);

      res.set("X-RateLimit-Limit", String(capacity));
      res.set("X-RateLimit-Remaining", String(remaining));

      if (!allowed) {
        res.set("Retry-After", String(Math.ceil(1 / refillPerSec)));
        return res.status(429).json({
          error: { code: "RATE_LIMITED", message: "Too many requests" },
        });
      }
      next();
    } catch (err) {
      // Redis unavailable: fail OPEN (allow) or CLOSED (block)? — a deliberate decision
      logger.error({ err }, "Rate limiter unavailable");
      next();
    }
  };
}
```

The catch block holds a real design decision:

- **Fail open** (let traffic through) — keeps the API available but unprotected during a Redis outage. Usual choice for general APIs.
- **Fail closed** (reject) — safer for sensitive endpoints like login or payment, at the cost of availability.

## Details that matter

- **Identify the client carefully.** Behind a proxy or load balancer, `req.ip` is the proxy's IP unless you configure `app.set("trust proxy", ...)` correctly — otherwise everyone shares one limit. But trusting `X-Forwarded-For` blindly lets clients spoof their IP.
- **Different limits for different routes.** Login: strict (5/min). Reads: generous. Expensive endpoints (exports): lower still.
- **Tiered limits** — free vs paid plans, set via the client's plan.
- **Return helpful headers** so well-behaved clients can back off (`Retry-After`, remaining count).
- **Limit at the edge too** — put coarse limits at the load balancer/CDN/API gateway so abusive traffic never reaches Node.
- **Hot keys** — a single heavily-used key concentrates load on one Redis shard; usually fine, but worth knowing at very large scale.

## Trade-offs summary

| Choice | Trade-off |
|--------|-----------|
| Fixed window vs token bucket | Simplicity vs burst handling/accuracy |
| Centralized Redis vs local counters | Accuracy vs added latency + a new dependency |
| Fail open vs fail closed | Availability vs protection |
| Per-IP vs per-user | Easy but shared IPs (offices, mobile carriers) vs requires authentication |

## Common mistakes

- **Per-instance in-memory counters** — the effective limit multiplies with instance count.
- **Non-atomic read-modify-write** — races between instances; use `INCR` or Lua.
- **`INCR` without guaranteed expiry** — a crash between commands creates a permanent block.
- **Trusting `X-Forwarded-For` blindly, or not configuring `trust proxy`** — spoofable, or everyone looks like one IP.
- **No decision on Redis failure** — the limiter silently becomes a single point of failure.
- **Sticky sessions or local state in "stateless" services** — breaks horizontal scaling.
- **Scaling the app before fixing indexes** — most API slowness is a missing index.
- **Pool size × instances exceeding database connection limits** — autoscaling then takes the database down.

## Quick summary

- Scale an API by making instances **stateless**, adding a load balancer, caching aggressively, and scaling the database reads-first
- Move slow work to queues; add timeouts and circuit breakers around dependencies
- A rate limiter must use **shared state (Redis)** and **atomic operations (Lua/INCR)** to work across instances
- Token bucket is a good default; fixed window is simplest; know the boundary-burst flaw
- Decide explicitly whether the limiter fails open or closed
- Return `429` with `Retry-After`; identify clients carefully

## Next

**`02-url-shortener.md`** designs a read-heavy system where ID generation and caching are the interesting problems.
