# Rate Limiting

Every authentication endpoint is a guessing oracle: login accepts password guesses, reset accepts tokens, OTP endpoints accept six-digit codes. Rate limiting caps how fast anyone can ask, turning "billions of guesses" into "a handful per hour". It also protects your CPU: each Argon2 hash is deliberately expensive.

**Prerequisites:** [02-threat-model.md](../01-fundamentals/02-threat-model.md), [01-redis-sessions.md](../04-sessions/01-redis-sessions.md)

---

## What you are defending against

| Attack | Limit that stops it |
|---|---|
| **Brute force** (many passwords, one account) | Per-account failed-attempt limit |
| **Credential stuffing** (leaked pairs across many accounts, from few IPs) | Per-IP limit |
| **Distributed stuffing** (botnet, one try per IP per account) | Per-account limit (IP limits alone don't see it) |
| **OTP/token guessing** | Per-challenge attempt cap ([00-mfa-overview.md](../05-mfa/00-mfa-overview.md)) |
| **Email/SMS bombing** (abusing forgot-password, resend, OTP send) | Per-recipient and per-IP send limits |
| **Resource exhaustion** (Argon2 flood) | Per-IP and global limits, small body sizes |
| **Scraping / enumeration** | Per-IP and per-user API limits |

No single dimension is enough. Combine **IP**, **account** and sometimes **IP + account**.

---

## Choosing a key

```text
login:ip:{ip}                 → 20 attempts / 15 min     (stops one machine hammering many accounts)
login:acct:{sha256(email)}    → 10 failures / 15 min     (stops many machines hammering one account)
login:pair:{ip}:{emailhash}   → 5 failures / 15 min      (strict for the exact pair)
reset:email:{sha256(email)}   → 3 sends / hour
reset:ip:{ip}                 → 10 requests / hour
```

- **Hash identifiers** used in keys (emails, phone numbers). Redis keys appear in logs and monitoring tools, and shouldn't contain personal data.
- **Normalize first** (`lowercase`, `trim`), or attackers evade limits with `Alice@x.com` vs `alice@x.com`.
- **Count unknown accounts too.** If only existing accounts get limited, the limiter itself reveals which accounts exist ([01-enumeration-protection.md](./01-enumeration-protection.md)).
- Limit by **authenticated user id** on logged-in routes, not only by IP: many users share one IP (offices, mobile carriers), and one attacker can rotate IPs.

---

## Algorithms

| Algorithm | How | Good for | Weakness |
|---|---|---|---|
| **Fixed window** | Count per calendar window (`INCR` + `EXPIRE`) | Simple, cheap | Burst of 2x at window boundaries |
| **Sliding window log** | Store each request timestamp (sorted set) | Exact | Memory per request |
| **Sliding window counter** | Weighted blend of current and previous fixed windows | Good approximation, cheap | Slightly approximate |
| **Token bucket** | Bucket refills at a steady rate; each request spends a token | Allows controlled bursts | Slightly more state |
| **Leaky bucket** | Requests drain at a constant rate | Smooth output | Delays/queues |

For authentication endpoints a **fixed window with a modest limit** is usually enough; the boundary burst only matters when limits are very small. Use token bucket for general API traffic where bursts are normal.

---

## Atomic fixed-window limiter in Redis

The naive version has a classic bug:

```js
// ❌ if the process crashes between these two calls, the key never expires → permanent block
const count = await redis.incr(key);
if (count === 1) await redis.expire(key, windowSeconds);
```

A Lua script does both steps atomically:

```lua
-- redis-scripts/rate-limit.lua
-- KEYS[1] = counter key; ARGV[1] = window seconds
local n = redis.call('INCR', KEYS[1])
if n == 1 then
  redis.call('EXPIRE', KEYS[1], ARGV[1])
end
local ttl = redis.call('TTL', KEYS[1])
return { n, ttl }
```

```js
// utils/rate-limit.js
import fs from "fs";
const SCRIPT = fs.readFileSync(new URL("../redis-scripts/rate-limit.lua", import.meta.url), "utf8");

export async function hit(key, limit, windowSeconds) {
  const [count, ttl] = await redis.eval(SCRIPT, { keys: [`rl:${key}`], arguments: [String(windowSeconds)] });
  return { allowed: count <= limit, count, remaining: Math.max(0, limit - count), retryAfter: ttl > 0 ? ttl : windowSeconds };
}
```

(`eval` option names depend on your `node-redis` version, as in [05-atomic-rotation-lua.md](../03-tokens/05-atomic-rotation-lua.md).)

### Middleware factory

```js
export const rateLimit = ({ name, limit, windowSeconds, key }) => async (req, res, next) => {
  try {
    const id = await key(req);                               // e.g. req.ip, or hashed email from body
    if (!id) return next();
    const r = await hit(`${name}:${id}`, limit, windowSeconds);

    res.set("RateLimit-Limit", String(limit));
    res.set("RateLimit-Remaining", String(r.remaining));
    if (!r.allowed) {
      res.set("Retry-After", String(r.retryAfter));
      return res.status(429).json({ message: "Too many requests. Try again later." });
    }
    next();
  } catch (err) {
    // Redis unavailable: see "Failure mode"
    return failureMode(err, req, res, next);
  }
};
```

Usage:

```js
const byIp = (name, limit, windowSeconds) => rateLimit({ name, limit, windowSeconds, key: (req) => req.ip });
const byEmail = (name, limit, windowSeconds) =>
  rateLimit({ name, limit, windowSeconds, key: (req) => hashToken(String(req.body?.email ?? "").trim().toLowerCase()) });

router.post("/login",           byIp("login-ip", 20, 900), byEmail("login-email", 10, 900), login);
router.post("/forgot-password", byIp("forgot-ip", 10, 3600), byEmail("forgot-email", 3, 3600), forgotPassword);
router.post("/refresh",         byIp("refresh-ip", 60, 60), refresh);
```

Note the email limiter must run **after** `express.json()` so the body exists, and must tolerate missing or non-string bodies.

---

## Counting failures vs requests

- **Per-IP:** count all attempts. It limits load and scanning.
- **Per-account:** count **failures** and reset on success, otherwise a legitimate user is punished for logging in normally. Full lockout designs are in [02-account-lockout.md](./02-account-lockout.md).

```js
// after verifying credentials
if (!valid) await hit(`login-fail:${emailHash}`, 10, 900);        // record
else await redis.del(`rl:login-fail:${emailHash}`);                // clear on success
// and check the counter BEFORE doing the expensive hash verification
```

---

## Using a library

`express-rate-limit` with a Redis store covers the common cases:

```js
import { rateLimit } from "express-rate-limit";
import { RedisStore } from "rate-limit-redis";

export const loginLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  limit: 20,                                   // called `max` in older versions
  standardHeaders: "draft-7",
  legacyHeaders: false,
  store: new RedisStore({ sendCommand: (...args) => redis.sendCommand(args) }),
  keyGenerator: (req) => req.ip,
});
```

Option names, header modes and IPv6 key handling have changed between major versions of these packages; check the docs for the versions you install. A library won't know about "hash the email from the body", "count only failures", or "same response for unknown accounts", so custom logic is often still needed for auth.

---

## The client IP problem

`req.ip` is only the real client address if Express is told how many proxies sit in front of it:

```js
app.set("trust proxy", 1);      // e.g. exactly one load balancer in front
```

| Setup | Result |
|---|---|
| No `trust proxy`, behind a load balancer | `req.ip` = the balancer's IP → **everyone shares one limit** (or gets blocked together) |
| `trust proxy: true` (trust all) | Clients can spoof `X-Forwarded-For` and **dodge limits** by sending a different value each request |
| Correct hop count | Real client IP |

Set the exact number of proxy hops (or the proxy's subnet), and make sure your edge overwrites rather than appends client-supplied `X-Forwarded-For`. Prefer a provider header such as `CF-Connecting-IP` only when traffic can reach you **only** through that provider.

**IPv6:** one user may control a whole `/64` (or more). Rate limit IPv6 by prefix (for example the /64), not the full address, or attackers get billions of "different" IPs.

---

## Suggested starting limits

Tune with real traffic; these are starting points, not standards.

| Endpoint | Per IP | Per account / recipient |
|---|---|---|
| `POST /login` | 20 / 15 min | 10 failures / 15 min |
| `POST /register` | 10 / hour | n/a |
| `POST /forgot-password` | 10 / hour | 3 / hour per email |
| `POST /reset-password` | 10 / 15 min | per token (single-use already) |
| `POST /resend-verification` | 10 / hour | 3 / hour per email |
| `POST /mfa/verify` | 20 / 15 min | 5 attempts per challenge |
| `POST /refresh` | 60 / min | per session |
| `POST /step-up` | 10 / 15 min | 5 failures / 15 min |
| Authenticated API | 100-1000 / min | per user id |
| Any request body | `express.json({ limit: "10kb" })` | |

---

## Responses and behavior

- **`429 Too Many Requests`** with a **`Retry-After`** header (seconds). Clients, including your Axios interceptor, can back off.
- Same response body for everyone; don't say whether the account exists.
- Rate-limit headers (`RateLimit-*`) are helpful for honest clients and mostly irrelevant to attackers.
- Log blocked requests as security events ([05-logging-and-audit.md](./05-logging-and-audit.md)); a spike is an attack signal.
- For humans who hit the limit, offer a path (CAPTCHA, contact support).

### Failure mode: when Redis is down

| Choice | Effect |
|---|---|
| **Fail open** (allow) | Availability preserved, but attackers get unlimited guesses during an outage |
| **Fail closed** (deny) | Safe, but nobody can log in |
| **Degrade** to an in-memory per-instance limiter with a stricter limit | Reasonable middle ground for auth endpoints |

Pick deliberately and alert on limiter errors. For general API traffic, failing open is common; for credential endpoints, prefer the degraded stricter fallback.

---

## Layers beyond the app

- **Edge/CDN/WAF** (Cloudflare, AWS WAF, Nginx `limit_req`): drops floods before they reach Node.
- **CAPTCHA / proof-of-work** after N failures: raises the cost per attempt for bots.
- **Bot detection and IP reputation** services.
- **Global concurrency cap on password hashing** (a queue or semaphore): bounds Argon2 CPU/memory under attack.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Limiting only by IP | Distributed attacks sail through |
| Limiting only by account | Attacker locks victims out; unknown accounts unprotected |
| Not normalizing the key | Case/whitespace variants bypass |
| `trust proxy` misconfigured | Shared limit for all users, or trivially spoofed IPs |
| Non-atomic `INCR` then `EXPIRE` | Keys without TTL → permanent blocks |
| Different behavior for existing/nonexistent accounts | Enumeration oracle |
| In-memory limiter with multiple instances | Limit multiplied by instance count; lost on restart |
| Limiting after the Argon2 verify | CPU already spent |
| Same limits for every endpoint | Too strict on APIs, too loose on login |
| Returning `200` with an error body instead of `429` | Clients/bots don't back off |

## Debugging

| Symptom | Cause |
|---|---|
| All users blocked at once | `req.ip` is the proxy's address |
| Limits never trigger | Behind a proxy with `trust proxy: true` and spoofed XFF, key function returns `undefined`, or limiter placed after the handler |
| Key without TTL (`TTL = -1`) | Non-atomic increment/expire |
| Legit users rate-limited behind NAT | Per-IP limits too low; add per-user limits and raise IP limits |
| Limit "resets" unexpectedly | Fixed window expiry, or Redis eviction/flush |

```text
redis-cli> KEYS rl:*          (dev only)
redis-cli> TTL rl:login-ip:203.0.113.7
```

---

## Quick summary

- Rate limit by IP, account and sometimes the pair; hash and normalize keys.
- Make the counter atomic (Lua) and always expiring.
- Count failures per account, all requests per IP; check before the expensive hash.
- Configure `trust proxy` exactly; treat IPv6 by prefix.
- Return `429` + `Retry-After`; decide the Redis-outage behavior in advance.

**Next:** [01-enumeration-protection.md](./01-enumeration-protection.md)
