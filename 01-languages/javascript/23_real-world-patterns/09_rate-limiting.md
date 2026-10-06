# Rate Limiting

Rate limiting caps how many requests a client may make in a time window. On a **server** it protects you from abuse, brute-force attacks, and accidental overload. On a **client** it keeps you within an API provider's quota.

It differs from [concurrency control](./06_concurrency-control.md): concurrency limits *how many at once*; rate limiting limits *how many per unit of time*. [Throttle](./02_throttle.md) is a small client-side relative that spaces function calls.

## Prerequisites

- [HTTP fundamentals](../15_networking/01_http-fundamentals.md) (status codes, headers)
- [HTTP server](../16_nodejs/08_http-server.md)
- [Closures](../06_closures/02_closure-use-cases.md), [Map and Set](../09_built-in-objects/06_map-and-set.md)

---

## Why It Matters

- **Security:** slows credential stuffing and password guessing ([Security checklist](../22_security/06_security-checklist.md)).
- **Reliability:** one noisy client can't starve everyone else.
- **Cost:** protects expensive endpoints (search, exports, third-party calls).
- **Fairness:** shares capacity across users.

---

## Algorithms

| Algorithm | Idea | Pros | Cons |
|---|---|---|---|
| **Fixed window** | Count requests per window (e.g. 100 per minute, resetting on the minute) | Trivial, cheap | **Boundary burst**: 100 at 12:00:59 + 100 at 12:01:00 = 200 in 2 seconds |
| **Sliding window log** | Keep timestamps of recent requests; count those within the last N seconds | Accurate | Memory per request |
| **Sliding window counter** | Weighted blend of current and previous fixed windows | Cheap, smooths bursts | Approximate |
| **Token bucket** | Bucket holds up to `capacity` tokens, refills at a steady rate; each request spends one | Allows **bursts** up to capacity, enforces average rate | Slightly more state |
| **Leaky bucket** | Requests queue and drain at a constant rate | Smooth, constant output | Adds queueing delay |

Token bucket is the usual default: it permits short bursts (good for real users) while keeping the long-run average bounded.

---

## Token Bucket Implementation

```js
class TokenBucket {
  constructor({ capacity, refillPerSec }) {
    this.capacity = capacity;
    this.refillPerSec = refillPerSec;
    this.tokens = capacity;
    this.last = Date.now();
  }

  #refill() {
    const now = Date.now();
    const elapsedSec = (now - this.last) / 1000;
    this.tokens = Math.min(this.capacity, this.tokens + elapsedSec * this.refillPerSec);
    this.last = now;
  }

  take(n = 1) {
    this.#refill();
    if (this.tokens >= n) {
      this.tokens -= n;
      return true;
    }
    return false;
  }

  /** seconds until `n` tokens are available */
  retryAfter(n = 1) {
    this.#refill();
    return Math.max(0, (n - this.tokens) / this.refillPerSec);
  }
}
```

No timers are needed: tokens are computed **lazily** from elapsed time on each call. That keeps it cheap and easy to test.

---

## Server Middleware (Express)

One bucket per client key:

```js
function rateLimit({ capacity = 20, refillPerSec = 5, keyFn = (req) => req.ip } = {}) {
  const buckets = new Map();   // key → TokenBucket

  // Periodically drop idle buckets so the Map doesn't grow forever
  setInterval(() => {
    const now = Date.now();
    for (const [key, b] of buckets) {
      if (now - b.last > 10 * 60_000) buckets.delete(key);
    }
  }, 60_000).unref();

  return (req, res, next) => {
    const key = keyFn(req);
    let bucket = buckets.get(key);
    if (!bucket) buckets.set(key, (bucket = new TokenBucket({ capacity, refillPerSec })));

    if (bucket.take()) return next();

    res.set('Retry-After', String(Math.ceil(bucket.retryAfter())));
    res.status(429).json({ error: 'Too many requests' });
  };
}

app.use('/api', rateLimit());
app.post('/login', rateLimit({ capacity: 5, refillPerSec: 1 / 60 }), loginHandler);   // stricter on sensitive routes
```

Points to note:

- Respond with **`429 Too Many Requests`** and a **`Retry-After`** header (seconds) so well-behaved clients know when to come back.
- `.unref()` on the cleanup interval prevents it from keeping the Node process alive.
- Apply **stricter limits to sensitive endpoints** (login, password reset, OTP, expensive search).
- Some APIs also send informational `RateLimit-*` headers; those follow an evolving standard, so check the current spec/provider docs before relying on exact names.

For production Express apps, the `express-rate-limit` package covers this with stores, headers, and options, so prefer a maintained library over custom middleware unless you have special needs.

---

## Choosing the Client Key

| Key | Notes |
|---|---|
| IP address | Easy; unfair behind NAT/shared IPs, evaded by rotating IPs |
| User ID / API key | Best for authenticated traffic |
| IP + route, or user + route | Per-endpoint limits |
| Composite (user, else IP) | Common: authenticated limit per user, anonymous limit per IP |

**Behind a proxy or load balancer**, `req.ip` is the proxy's address unless you configure trust correctly (`app.set('trust proxy', ...)` in Express). Trusting `X-Forwarded-For` blindly lets clients spoof it and dodge limits, so only trust it from proxies you control, and configure the exact hop count.

---

## Going Distributed

An in-process `Map` limits **per server instance**. With 4 instances behind a load balancer, a client can effectively get 4× the limit, and counters vanish on restart.

For a shared limit, keep counters in a shared store such as Redis. Operations must be **atomic** to avoid race conditions between instances (e.g. `INCR` + `EXPIRE` for fixed windows, or a Lua script for token bucket/sliding window). Prefer an established library or a gateway/CDN-level limiter (API gateway, reverse proxy, WAF) for this.

Decide your **failure mode**: if the limiter store is down, do you fail open (allow traffic) or fail closed (reject)? Fail open for general traffic; consider fail closed for security-critical routes.

---

## Rate Limiting as a Client

When calling an API with a quota (say 10 requests/second), pace your own requests so you don't get `429`s:

```js
function createPacer(ratePerSec) {
  const interval = 1000 / ratePerSec;
  let nextSlot = 0;
  return async function pace() {
    const now = Date.now();
    const wait = Math.max(0, nextSlot - now);
    nextSlot = Math.max(now, nextSlot) + interval;
    if (wait) await new Promise((r) => setTimeout(r, wait));
  };
}

const pace = createPacer(10);
for (const id of ids) {
  await pace();
  await callApi(id);
}
```

Combine with a concurrency limit when requests are slow, and with [retry](./03_retry.md) that honors `Retry-After` when you do get a `429`.

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| Fixed windows only, leaving boundary bursts | Token bucket or sliding window |
| Limiting by IP behind a proxy without configuring trust | Set trust-proxy properly; avoid trusting spoofable headers |
| Per-instance in-memory limits in a multi-instance deploy | Shared store / gateway-level limiting |
| Unbounded `Map` of client keys | Evict idle entries or use TTL'd store |
| Returning `500`/`403` instead of `429` | Use `429` with `Retry-After` |
| Same limit for every endpoint | Tighter limits on login/OTP/expensive routes |
| Non-atomic read-modify-write in a shared store | Use atomic commands/scripts |
| Clients hammering again immediately after `429` | Document backoff; clients should use [retry with backoff + jitter](./03_retry.md) |

---

## Testing

Because the bucket is time-driven but timer-free, fake time makes it trivial.

```js
import { vi, it, expect, beforeEach, afterEach } from 'vitest';

beforeEach(() => vi.useFakeTimers());
afterEach(() => vi.useRealTimers());

it('allows a burst up to capacity, then rejects', () => {
  const b = new TokenBucket({ capacity: 3, refillPerSec: 1 });
  expect([b.take(), b.take(), b.take(), b.take()]).toEqual([true, true, true, false]);
});

it('refills over time', () => {
  const b = new TokenBucket({ capacity: 1, refillPerSec: 1 });
  expect(b.take()).toBe(true);
  expect(b.take()).toBe(false);

  vi.advanceTimersByTime(1000);
  expect(b.take()).toBe(true);
});
```

For the middleware, use an integration test that fires N+1 requests and asserts the last gets `429` with a `Retry-After` header ([Integration testing](../21_testing/03_integration-testing.md)).

---

## Quick Summary

- Rate limiting caps **requests per time**; concurrency control caps **simultaneous** work.
- **Token bucket** is the practical default: bursts allowed, average rate enforced, lazy refill with no timers.
- Respond with **`429` + `Retry-After`**; limit sensitive endpoints more strictly.
- Pick the client key carefully (user > IP), and configure proxy trust correctly.
- In-memory limits are per-instance; use a shared store or gateway for distributed systems.
- As a client, pace your calls and back off on `429`.

**Next:** Back to the [Real-World Patterns README](./README.md).
