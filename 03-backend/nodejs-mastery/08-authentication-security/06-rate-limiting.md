# Rate Limiting

Capping how many requests a client can make in a given time, so brute force, scraping, and accidental overload can't take you down.

## Why rate limit?

- **Brute-force protection** — without a limit, an attacker can try millions of passwords or one-time codes.
- **Credential stuffing** — attackers replay leaked email/password pairs from other sites against your login.
- **Abuse prevention** — stop scraping, spam signups, and SMS/email-bombing (which also costs you money).
- **Fairness and stability** — one noisy client shouldn't starve everyone else.
- **Cost control** — protects expensive endpoints (search, report generation, third-party API calls).

Rate limiting does not replace authentication or validation. It's another layer from the defense-in-depth stack in `00-README.md`.

---

## The common algorithms

| Algorithm | How it works | Trade-off |
|---|---|---|
| **Fixed window** | Count requests per clock window (e.g. 100 per minute, resetting at :00) | Simple and cheap, but allows a burst of 2× at a window boundary |
| **Sliding window** | Count requests over the *last* N seconds, continuously | Smoother and fairer; slightly more storage/computation |
| **Token bucket** | A bucket refills at a steady rate; each request spends a token | Allows controlled bursts; great for APIs |
| **Leaky bucket** | Requests drain from a queue at a constant rate | Smooths traffic into a steady flow |

`express-rate-limit` uses fixed windows. That's good enough for most apps. The system-design version with token buckets is in `19-system-design/01-scalable-api-and-rate-limiter.md`.

---

## Basic setup with `express-rate-limit`

```bash
npm install express-rate-limit
```

```js
import express from "express";
import rateLimit from "express-rate-limit";

const app = express();

const globalLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,      // 15 minutes
  limit: 100,                     // max 100 requests per IP per window
  standardHeaders: "draft-7",     // send RateLimit-* headers
  legacyHeaders: false,           // don't send the old X-RateLimit-* headers
  message: { error: "Too many requests, please try again later." },
});

app.use(globalLimiter);
```

When the limit is exceeded, the client gets **`429 Too Many Requests`**. With `standardHeaders` enabled, the response includes `RateLimit` and `RateLimit-Policy` headers (and `Retry-After` on rejected requests) so well-behaved clients know when to come back.

> In older versions of the library, the option was called `max`. Current versions use `limit` (and still accept `max` for compatibility).

---

## Different limits for different routes

One global number is rarely right. Sensitive and expensive routes need tighter limits than cheap reads.

```js
// very strict: login attempts
const loginLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  limit: 5,
  skipSuccessfulRequests: true,   // only count failures (status < 400 is skipped)
  standardHeaders: "draft-7",
  legacyHeaders: false,
  message: { error: "Too many login attempts. Try again in 15 minutes." },
});

// strict: anything that sends an email or SMS
const otpLimiter = rateLimit({
  windowMs: 60 * 60 * 1000,
  limit: 3,
  standardHeaders: "draft-7",
  legacyHeaders: false,
});

// relaxed: general API
const apiLimiter = rateLimit({
  windowMs: 60 * 1000,
  limit: 120,
  standardHeaders: "draft-7",
  legacyHeaders: false,
});

app.use("/api", apiLimiter);
app.post("/auth/login", loginLimiter, loginHandler);
app.post("/auth/forgot-password", otpLimiter, forgotPasswordHandler);
```

Suggested starting points (tune with real traffic):

| Endpoint | Window | Limit |
|---|---|---|
| Login | 15 min | 5–10 failures per IP **and** per account |
| Registration | 1 hour | 5–10 per IP |
| Password reset / OTP send | 1 hour | 3–5 per IP **and** per email |
| General authenticated API | 1 min | 60–300 per user |
| Expensive endpoints (export, search) | 1 min | 5–20 per user |

---

## Behind a proxy: `trust proxy` is critical

If your app runs behind Nginx, a load balancer, or a platform proxy (see `16-production/04-nginx.md`), every request appears to come from the proxy's IP, so **all users share one bucket** and get blocked together.

```js
// Tell Express how many proxy hops to trust so req.ip is the real client IP
app.set("trust proxy", 1);      // 1 = one proxy in front of the app
```

Get the number right:

- Too low → everyone shares the proxy's IP.
- Too high (or `true`) → a client can spoof `X-Forwarded-For` to get a fresh bucket on every request and bypass the limit entirely.

The same setting affects `secure` cookies (`03-sessions.md`).

---

## Choosing the key: IP is not always enough

By default the limiter keys on `req.ip`. Better keys depend on the endpoint:

```js
// per authenticated user (falls back to IP for anonymous requests)
const userLimiter = rateLimit({
  windowMs: 60 * 1000,
  limit: 60,
  keyGenerator: (req) => req.user?.id ?? req.ip,
});

// per email on login — stops distributed attacks on one account
const accountLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  limit: 5,
  keyGenerator: (req) => String(req.body?.email ?? "").toLowerCase() || req.ip,
});

app.post("/auth/login", loginLimiter, accountLimiter, loginHandler);
```

Why both? An attacker with many IPs (a botnet) defeats a per-IP limit, but they still can't try unlimited passwords against **one account** once you limit by email. Conversely, a per-account-only limit lets an attacker lock a real user out; combining both gives balance.

> Note: if you key on user input like `req.body.email`, make sure `express.json()` runs **before** the limiter, and be careful not to let attackers create unlimited buckets with random emails (the IP limiter above catches that).

---

## Production: use a shared store (Redis)

The default store is **in memory**, which breaks down quickly:

- Each Node process has its own counters. With 4 cluster workers (`02-core-modules/10-cluster-and-worker-threads.md`) or 10 containers, an attacker effectively gets 4× or 10× the limit.
- Counters vanish on every restart or deploy.

Use Redis so every instance shares the same counters:

```bash
npm install rate-limit-redis redis
```

```js
import { createClient } from "redis";
import { RedisStore } from "rate-limit-redis";
import rateLimit from "express-rate-limit";

const redisClient = createClient({ url: process.env.REDIS_URL });
await redisClient.connect();

const limiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  limit: 100,
  standardHeaders: "draft-7",
  legacyHeaders: false,
  store: new RedisStore({
    sendCommand: (...args) => redisClient.sendCommand(args),
    prefix: "rl:api:",          // separate prefix per limiter
  }),
});
```

Give each limiter its own `prefix` so the login limiter and the API limiter don't share counters. Redis basics are in `07-databases/redis/01-basics-and-data-structures.md`.

### What if Redis goes down?

Decide deliberately whether to **fail open** (allow requests; risk abuse) or **fail closed** (reject requests; risk an outage). For a login limiter, failing closed is safer; for a general API limiter, failing open is often acceptable. `express-rate-limit` lets you choose via the `passOnStoreError` option.

---

## How it works under the hood (a DIY limiter)

Understanding the mechanism makes the library less magical. A fixed-window limiter in Redis is two commands:

```js
async function isRateLimited(key, limit, windowSeconds) {
  const count = await redisClient.incr(key);          // atomic increment
  if (count === 1) {
    await redisClient.expire(key, windowSeconds);     // start the window on first hit
  }
  return count > limit;
}

// middleware
function rateLimitByIp(req, res, next) {
  isRateLimited(`rl:${req.ip}`, 100, 60)
    .then((limited) => {
      if (limited) return res.status(429).json({ error: "Too many requests" });
      next();
    })
    .catch(next);
}
```

Caveat: there's a tiny race between `INCR` and `EXPIRE` (a crash in between leaves a key with no expiry). Production code wraps both in a Lua script or a `MULTI` transaction so they happen atomically, which is exactly what `rate-limit-redis` does for you.

---

## Brute-force defenses beyond plain rate limiting

### Progressive delays

Instead of a hard block, slow each attempt down:

```bash
npm install express-slow-down
```

```js
import slowDown from "express-slow-down";

const loginSlowDown = slowDown({
  windowMs: 15 * 60 * 1000,
  delayAfter: 3,                         // first 3 requests are at full speed
  delayMs: (used) => (used - 3) * 500,   // then +500ms per extra request
});

app.post("/auth/login", loginSlowDown, loginLimiter, loginHandler);
```

### Account lockout (use with care)

Temporarily locking an account after N failures stops targeted guessing, but it hands attackers a **denial-of-service tool** (they can lock out anyone by submitting bad passwords). Prefer:

- Short, growing lockouts (1 min → 5 min → 30 min) rather than permanent locks.
- Notifying the real user by email.
- Combining with CAPTCHA or MFA after repeated failures.

### Multi-factor authentication

The strongest answer to credential stuffing is that a stolen password alone is no longer enough. Rate-limit the OTP verification step too, since a 6-digit code has only 1,000,000 possibilities.

---

## Handling 429s on the client side

Return information clients can act on:

```js
const limiter = rateLimit({
  windowMs: 60 * 1000,
  limit: 30,
  handler: (req, res, next, options) => {
    res.status(options.statusCode).json({
      error: "rate_limited",
      message: "Too many requests. Please slow down.",
      retryAfterSeconds: Math.ceil(options.windowMs / 1000),
    });
  },
});
```

A polite client reads `Retry-After` and uses **exponential backoff with jitter** rather than hammering the endpoint. This pattern appears again in `11-async-processing/02-workers-retry-dlq.md`. Error response conventions are in `09-api-development/05-error-responses.md`.

---

## Rate limiting is not just for Express

Layer it:

1. **Edge / CDN / WAF** (Cloudflare, AWS WAF) — absorbs large floods before they reach you.
2. **Reverse proxy** (Nginx `limit_req`) — cheap and fast; see `16-production/04-nginx.md`.
3. **Application** (`express-rate-limit`) — per-user, per-route, business-aware rules.

The application layer is the only one that knows *who the user is* and *which action is expensive*, so keep it even when the outer layers exist.

---

## Common mistakes

```js
// ❌ in-memory store in a multi-instance deployment — the limit is silently multiplied
rateLimit({ windowMs: 60_000, limit: 100 });

// ❌ wrong trust proxy: everyone shares one IP, or attackers spoof X-Forwarded-For
app.set("trust proxy", true);

// ❌ limiter registered AFTER the route it should protect — it never runs
app.post("/login", loginHandler);
app.use(loginLimiter);

// ❌ same generous limit on login as on a health check
app.use(rateLimit({ windowMs: 60_000, limit: 1000 }));

// ❌ rate limiting health-check endpoints — your load balancer gets 429s and marks you unhealthy
```

```js
// ✅ skip health checks
const limiter = rateLimit({
  windowMs: 60_000,
  limit: 100,
  skip: (req) => req.path === "/health",
});
```

## Next

**`07-helmet.md`** covers the security headers that protect browsers interacting with your app — XSS defenses, clickjacking protection, and forced HTTPS.
