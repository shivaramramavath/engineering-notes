# Rate Limiting & Traffic Shaping in JavaScript

> A production-grade implementation and architectural guide to rate-limiting algorithms in JavaScript, featuring Token Bucket, Leaky Bucket, Sliding Window Counter, and Redis atomic Lua scripts.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced / Production-Grade |
| **Prerequisites** | [08_caching.md](file:///e:/Learning/engineering-notes/01-languages/javascript/23_real-world-patterns/08_caching.md), [22_security/06_security-checklist.md](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security/06_security-checklist.md) |
| **Standard Baseline** | ECMAScript 2024 / Node.js 20+ LTS / RFC 6585 & IETF Draft |
| **Status** | Production Ready |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Unconstrained web applications and public APIs are vulnerable to volumetric abuse:
- **DDoS & Resource Exhaustion**: Malicious or buggy clients can flood servers with thousands of requests per second, starving legitimate users.
- **Credential Stuffing & Brute Force**: Automated bots target authentication endpoints (`/login`, `/reset-password`) trying millions of leaked password combinations.
- **Scraping & Quota Abuse**: Free-tier users exceeding agreed consumption thresholds, impacting platform multi-tenant stability.

**Rate Limiting** is a traffic management strategy that monitors incoming request frequency per client (identified by IP address, user account, or API key) and rejects or delays requests that exceed defined limits, responding with `HTTP 429 Too Many Requests`.

---

## 2. Core Concepts

### Rate Limiting Algorithms
1. **Fixed Window Counter**: Divides time into fixed discrete windows (e.g. 1 minute). Counts requests within the window.
   - *Flaw*: Vulnerable to boundary burst attacks (2x the limit at the boundary).
2. **Sliding Window Log**: Stores exact timestamps of every request in a sorted set. Accurate but memory-intensive ($O(N)$ memory per client).
3. **Sliding Window Counter**: Computes a weighted average between the previous window and the current window:
   $$\text{Rate} = \text{Count}_{\text{current}} + \text{Count}_{\text{prev}} \times \left(1 - \frac{t_{\text{elapsed}}}{\text{WindowSize}}\right)$$
4. **Token Bucket**: Tokens are continuously added to a bucket at a fixed rate up to a max capacity. Each request consumes one token.
   - *Advantage*: Naturally accommodates short legitimate traffic bursts while maintaining a steady-state average rate.
5. **Leaky Bucket (Traffic Shaping)**: Requests enter a FIFO queue and leak out at a constant, steady rate. Smooths bursty traffic into a predictable constant stream.

```
       [ Request Arrives ]
               │
               ▼
     [ Token in Bucket? ]
         │          │
        YES         NO
         │          │
         ▼          ▼
    Consume 1    HTTP 429
      Token     Too Many Requests
         │      Retry-After: 5s
         ▼
    Forward to
   Route Handler
```

### Standard IETF HTTP Rate Limit Headers
Production rate limiters attach standardized response headers:
- `RateLimit-Limit`: Maximum requests permitted per quota window.
- `RateLimit-Remaining`: Number of requests remaining in the current window.
- `RateLimit-Reset`: Seconds remaining until the quota window resets.
- `Retry-After`: Returned on `429` indicating how long the client must wait.

---

## 3. How It Works Internally

The **Token Bucket** algorithm calculates token replenishment mathematically on demand rather than running active background timers for every user:

$$\text{Tokens}_{\text{new}} = \min\left(\text{Capacity}, \text{Tokens}_{\text{last}} + \frac{(\text{Time}_{\text{now}} - \text{Time}_{\text{last}}) \times \text{Capacity}}{\text{WindowMs}}\right)$$

```
Client Key: "ip:203.0.113.195"
  Capacity: 10 tokens
  Refill: 1 token / second

State: { tokens: 4, lastRefill: 1700000000000 }
New Request at 1700000003000 (3 seconds later):
  Tokens added: 3 * 1 = 3 tokens
  Updated tokens: min(10, 4 + 3) = 7 tokens
  Can consume 1 token? YES -> Tokens become 6. Allow request!
```

---

## 4. Syntax & API Reference

### Production-Grade Token Bucket Rate Limiter
```javascript
/**
 * In-memory Token Bucket rate limiter with automatic LRU cleanup.
 */
export class TokenBucketLimiter {
  constructor(options = {}) {
    this.capacity = options.capacity ?? 60; // Max burst size
    this.refillRatePerSec = options.refillRatePerSec ?? 1; // Tokens added per second
    this.buckets = new Map();
    this.maxBuckets = options.maxBuckets ?? 10000;
  }

  getBucket(key) {
    let bucket = this.buckets.get(key);
    const now = Date.now();

    if (!bucket) {
      if (this.buckets.size >= this.maxBuckets) {
        // Evict oldest bucket to prevent memory leaks
        const oldest = this.buckets.keys().next().value;
        this.buckets.delete(oldest);
      }
      bucket = { tokens: this.capacity, lastUpdated: now };
      this.buckets.set(key, bucket);
      return bucket;
    }

    // Refresh LRU position
    this.buckets.delete(key);
    this.buckets.set(key, bucket);

    // Calculate token replenishment
    const elapsedSeconds = (now - bucket.lastUpdated) / 1000;
    const tokensToAdd = elapsedSeconds * this.refillRatePerSec;
    bucket.tokens = Math.min(this.capacity, bucket.tokens + tokensToAdd);
    bucket.lastUpdated = now;

    return bucket;
  }

  consume(key, tokens = 1) {
    const bucket = this.getBucket(key);

    if (bucket.tokens >= tokens) {
      bucket.tokens -= tokens;
      return {
        allowed: true,
        remaining: Math.floor(bucket.tokens),
        resetSeconds: Math.ceil((this.capacity - bucket.tokens) / this.refillRatePerSec),
      };
    }

    const deficit = tokens - bucket.tokens;
    const retryAfter = Math.ceil(deficit / this.refillRatePerSec);

    return {
      allowed: false,
      remaining: 0,
      retryAfter,
      resetSeconds: retryAfter,
    };
  }

  clear() {
    this.buckets.clear();
  }
}
```

---

## 5. Practical Examples

### Example 1: Express.js Rate Limiting Middleware
```javascript
import { TokenBucketLimiter } from './TokenBucketLimiter.js';

export function createRateLimitMiddleware(options = {}) {
  const limiter = new TokenBucketLimiter(options);

  return function rateLimitMiddleware(req, res, next) {
    // Identify client by authenticated user ID or real client IP
    const clientId = req.user?.id || req.ip || req.socket.remoteAddress;
    const result = limiter.consume(clientId, 1);

    // Set standard IETF RateLimit headers
    res.setHeader('RateLimit-Limit', String(limiter.capacity));
    res.setHeader('RateLimit-Remaining', String(result.remaining));
    res.setHeader('RateLimit-Reset', String(result.resetSeconds));

    if (!result.allowed) {
      res.setHeader('Retry-After', String(result.retryAfter));
      return res.status(429).json({
        error: 'Too Many Requests',
        message: `Rate limit exceeded. Please retry after ${result.retryAfter} seconds.`,
      });
    }

    next();
  };
}
```

### Example 2: Sliding Window Counter Rate Limiter
```javascript
export class SlidingWindowCounter {
  constructor(limit = 100, windowMs = 60000) {
    this.limit = limit;
    this.windowMs = windowMs;
    this.windows = new Map(); // key -> { currentCount, prevCount, currentWindowStart }
  }

  consume(key) {
    const now = Date.now();
    const currentWindowStart = Math.floor(now / this.windowMs) * this.windowMs;

    let state = this.windows.get(key);

    if (!state || state.currentWindowStart < currentWindowStart - this.windowMs) {
      // More than 1 full window elapsed: reset completely
      state = { currentCount: 0, prevCount: 0, currentWindowStart };
      this.windows.set(key, state);
    } else if (state.currentWindowStart < currentWindowStart) {
      // Transitioned to next window
      state.prevCount = state.currentCount;
      state.currentCount = 0;
      state.currentWindowStart = currentWindowStart;
    }

    // Calculate weighted request count
    const timeIntoCurrentWindow = now - currentWindowStart;
    const weight = 1 - (timeIntoCurrentWindow / this.windowMs);
    const estimatedCount = (state.prevCount * weight) + state.currentCount;

    if (estimatedCount >= this.limit) {
      const resetSeconds = Math.ceil((this.windowMs - timeIntoCurrentWindow) / 1000);
      return { allowed: false, remaining: 0, resetSeconds };
    }

    state.currentCount++;
    return {
      allowed: true,
      remaining: Math.max(0, Math.floor(this.limit - estimatedCount - 1)),
      resetSeconds: Math.ceil((this.windowMs - timeIntoCurrentWindow) / 1000),
    };
  }
}
```

### Example 3: Atomic Redis Sliding Window Limiter (Lua Script)
```javascript
// Distributed atomic rate limiter executing in Redis via Lua
export const REDIS_SLIDING_WINDOW_LUA = `
local key = KEYS[1]
local now = tonumber(ARGV[1])
local window = tonumber(ARGV[2])
local limit = tonumber(ARGV[3])

local clearBefore = now - window
redis.call('ZREMRANGEBYSCORE', key, 0, clearBefore)

local currentRequests = redis.call('ZCARD', key)

if currentRequests < limit then
    redis.call('ZADD', key, now, now)
    redis.call('PEXPIRE', key, window)
    return {1, limit - currentRequests - 1}
else
    return {0, 0}
end
`;

export async function checkDistributedRateLimit(redisClient, clientKey, limit = 100, windowMs = 60000) {
  const now = Date.now();
  const res = await redisClient.eval(
    REDIS_SLIDING_WINDOW_LUA,
    1,
    `ratelimit:${clientKey}`,
    now,
    windowMs,
    limit
  );

  return {
    allowed: res[0] === 1,
    remaining: res[1],
  };
}
```

### Example 4: Multi-Tier Endpoint Rate Limiting
```javascript
import express from 'express';
import { createRateLimitMiddleware } from './rateLimiterMiddleware.js';

const app = express();

// 1. Global Baseline: 100 requests per minute
const globalLimiter = createRateLimitMiddleware({ capacity: 100, refillRatePerSec: 1.66 });
app.use(globalLimiter);

// 2. Strict Authentication Protection: 5 login attempts per minute (brute-force defense)
const authLimiter = createRateLimitMiddleware({ capacity: 5, refillRatePerSec: 0.083 });
app.post('/api/auth/login', authLimiter, (req, res) => {
  res.json({ message: 'Authentication successful' });
});

// 3. Generous Internal API: 500 requests per minute
const internalApiLimiter = createRateLimitMiddleware({ capacity: 500, refillRatePerSec: 8.33 });
app.use('/api/internal', internalApiLimiter);
```

### Example 5: Client-Side Rate-Limited Request Dispatcher
```javascript
export class RateLimitedClient {
  constructor(requestsPerSecond = 5) {
    this.delayMs = 1000 / requestsPerSecond;
    this.lastRequestTime = 0;
  }

  async fetch(url, options = {}) {
    const now = Date.now();
    const elapsed = now - this.lastRequestTime;
    const waitTime = Math.max(0, this.delayMs - elapsed);

    if (waitTime > 0) {
      await new Promise((resolve) => setTimeout(resolve, waitTime));
    }

    this.lastRequestTime = Date.now();
    return fetch(url, options);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **The Boundary Burst Problem in Fixed Windows**:
   With a fixed 1-minute window allowing 100 requests, an attacker can send 100 requests at 00:59 and another 100 requests at 01:00 (200 requests within a 2-second window). Use **Token Bucket** or **Sliding Window** to eliminate boundary spikes.
2. **Reverse Proxy IP Spoofing**:
   If an Express server reads `req.headers['x-forwarded-for']` directly without configuring `app.set('trust proxy', 1)`, an attacker can send a spoofed `X-Forwarded-For: 1.2.3.4` header with every request, resetting their rate limit key and bypassing defenses.
3. **Redis Multi-Node Race Conditions**:
   Performing `redis.get()` followed by `redis.set()` from Node.js is non-atomic. Under high concurrency, multiple Node instances read identical counts, allowing clients to exceed limits. Always execute rate checks atomically inside **Redis Lua scripts** or use Redis `INCR` + `EXPIRE`.
4. **Memory Leaks from Dynamic IP Sets**:
   Storing bucket states in a JavaScript `Map` where each IP is a new key will cause the map to grow indefinitely as millions of unique web crawlers visit the site. Always enforce LRU eviction or TTL on in-memory rate-limiter maps.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Hazard | Production Fix |
| :--- | :--- | :--- |
| Fixed window with naive reset | Vulnerable to 2x boundary spikes (bursts at window transitions). | Adopt Token Bucket or Sliding Window Counter algorithms. |
| Relying on untrusted `X-Forwarded-For` | Attackers spoof IPs to completely evade rate limits. | Configure `trust proxy` and validate reverse proxy hop counts. |
| Non-atomic distributed counters | Race conditions allow clients to exceed limits during concurrency. | Execute rate limit evaluations in atomic Redis Lua scripts. |
| Unbounded in-memory IP tables | Rapid heap memory accumulation from ephemeral bot IPs. | Enforce LRU eviction (`maxBuckets`) and automatic TTL cleanup. |
| Missing `Retry-After` headers on 429 | Clients retry immediately in aggressive loops, exacerbating load. | Return standard `Retry-After` header specifying wait duration in seconds. |
| Applying identical limits to all routes | Heavy APIs starved or login endpoints exposed to brute force. | Implement multi-tiered limits tailored to endpoint risk profiles. |

---

## 8. Performance Characteristics

- **In-Memory Token Calculation**: Calculating tokens mathematically during incoming requests requires sub-microsecond CPU time ($\approx 80\text{ns}$ in V8) and zero background timer scheduling.
- **Redis Lua Script Overhead**: An atomic sliding-window Lua script in Redis typically executes in $0.4\text{ms}$ to $0.8\text{ms}$, capable of handling over 40,000 checks per second on a single Redis core.
- **Memory Consumption**: A Token Bucket state requires only 24 bytes of memory per client key.

---

## 9. Engine Internals

### V8 Context & Atomic Lua Execution
- **Redis Single-Threaded Isolation**: Redis executes Lua scripts as a single atomic unit. No other command or script can run concurrently while the Lua script is evaluating. This guarantees that checking the current count and recording the timestamp occur atomically without distributed lock contention.
- **V8 Map Memory Footprint**: In V8, keeping rate-limiting keys formatted as compact strings (`ip:127.0.0.1`) minimizes memory allocation and preserves fast hash lookup times.

---

## 10. Best Practices & Design Guidelines

1. **Use Token Bucket for User APIs**: Permits natural user burstiness (loading multiple assets) while maintaining a strict long-term average.
2. **Use Sliding Window Counter for Strict SLAs**: Delivers high precision with minimal memory overhead compared to sliding window logs.
3. **Always Include Standard Headers**: Emit `RateLimit-Limit`, `RateLimit-Remaining`, and `RateLimit-Reset` on every response.
4. **Degrade Gracefully on Redis Failure**: If the distributed Redis rate-limiting cluster becomes unreachable, fail open (allow requests) or fall back to local in-memory limiting rather than breaking production traffic.

---

## 11. Real-World Case Study

### SaaS Authentication API Under Distributed Brute Force
**Incident**: A SaaS platform suffered a distributed credential-stuffing attack targeting `/api/v1/auth/login`. Attackers used a botnet of 40,000 rotating residential proxy IP addresses, ensuring that no single IP sent more than 2 requests per minute, bypassing traditional per-IP rate limits.
**Impact**: Millions of authentication queries saturated database CPU, and multiple accounts with weak passwords were breached.

**Remediation**:
- Introduced a **Composite Rate Limiting Strategy**:
  1. **Per-IP Limit**: 10 requests/minute.
  2. **Per-Target-Account Limit**: Maximum 5 failed attempts per target username/email per 15 minutes, irrespective of client IP.
  3. **Global Endpoint Limit**: Maximum 5,000 total login attempts per minute across the entire platform.
- Implemented in Redis using atomic Lua sliding-window scripts.
- If an account hits 5 failed attempts, the account is temporarily locked with CAPTCHA verification required.

**Result**:
- Credential stuffing was completely mitigated within minutes of deployment.
- Database CPU dropped from 94% back down to 12%.

---

## 12. Interview Questions

<details>
<summary>1. How does the Token Bucket algorithm differ from the Leaky Bucket algorithm?</summary>

- **Token Bucket**: Tokens are added to a bucket at a fixed rate up to a maximum capacity. Incoming requests consume tokens. If tokens are available, requests are processed immediately. This allows short bursts of traffic (up to the bucket capacity) while maintaining a long-term average rate.
- **Leaky Bucket**: Requests enter a queue (the bucket) and are discharged/processed at a strictly constant rate, smoothing out bursts into a steady, uniform flow (traffic shaping). If the queue overflows, new requests are rejected.
</details>

<details>
<summary>2. Why does the Fixed Window Counter algorithm suffer from boundary burst vulnerabilities?</summary>

In Fixed Window, time is sliced into discrete intervals (e.g., 01:00 to 01:01). If the limit is 100 requests per minute, an attacker can send 100 requests at 01:00:59 (the end of the first window) and another 100 requests at 01:01:01 (the beginning of the second window). Within a 2-second timeframe, the server receives 200 requests—twice the intended maximum rate—without violating either window's limit.
</details>

<details>
<summary>3. How does the Sliding Window Counter algorithm eliminate boundary bursts without high memory usage?</summary>

Instead of storing individual timestamps (as in Sliding Window Log), Sliding Window Counter estimates requests using a weighted formula based on the previous and current windows:
$$\text{Estimated} = \text{Count}_{\text{current}} + \text{Count}_{\text{prev}} \times \left(1 - \frac{t}{\text{window}}\right)$$
This provides a smooth rate approximation across sliding boundaries while requiring storage of only two integer numbers per client key, making it memory-efficient and resilient to boundary spikes.
</details>

<details>
<summary>4. Why must distributed rate limiting in Redis use Lua scripts or transactions?</summary>

If an application performs `redis.get(key)` in Node.js, checks the limit in JavaScript, and then runs `redis.incr(key)`, a race condition exists. Under high concurrency, 50 concurrent requests across multiple Node.js server instances can read the same count simultaneously before any of them increments it, allowing the limit to be exceeded by 50x. Executing the check-and-increment logic inside an atomic Redis Lua script ensures complete atomicity because Redis executes the script without interruption.
</details>

<details>
<summary>5. What headers should be returned when an HTTP request is rate-limited?</summary>

The server should return `HTTP 429 Too Many Requests` along with:
- `Retry-After`: Number of seconds the client must wait before retrying.
- `RateLimit-Limit`: The request quota ceiling in the current window.
- `RateLimit-Remaining`: The number of requests remaining (0 when blocked).
- `RateLimit-Reset`: Number of seconds until the quota window resets.
</details>

<details>
<summary>6. How can an attacker bypass an IP-based rate limiter, and how do you prevent it?</summary>

Attackers bypass IP limits by:
1. Spoofing `X-Forwarded-For` headers: Prevent this by configuring `trust proxy` correctly to only trust headers from known, trusted reverse proxies.
2. Using distributed botnets or proxy pools with rotating IPs: Prevent this by using **Composite Rate Limiting**—rate limiting not just by IP, but also by user account ID, API key, device fingerprint, or global endpoint thresholds.
</details>

<details>
<summary>7. How do you prevent in-memory rate limiters from causing memory leaks in Node.js?</summary>

Because millions of unique IP addresses can visit a public web server, an unbounded `Map` storing rate-limit states will continually grow, leading to V8 heap exhaustion and process crashes. Mitigate by implementing an **LRU eviction strategy** (capping the map size to e.g. 10,000 entries) or running periodic cleanup routines that evict buckets whose reset timers have expired.
</details>

<details>
<summary>8. What should happen if the centralized rate-limiting store (e.g. Redis) goes down?</summary>

Applications should implement a **Fail-Open** policy: if the Redis rate-limiting call times out or throws an error, catch the error, log an alert, and allow the request through (optionally falling back to local process-level in-memory limiting). Failing closed (blocking all requests) converts a rate-limiter outage into a complete site-wide denial of service.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement a Simple In-Memory Fixed Window Limiter
**Specification**: Implement `isAllowed(ip, limit, windowMs)` that enforces a fixed window counter using a JavaScript `Map`.

<details>
<summary>Solution</summary>

```javascript
export class FixedWindowLimiter {
  constructor(limit = 10, windowMs = 60000) {
    this.limit = limit;
    this.windowMs = windowMs;
    this.counters = new Map();
  }

  isAllowed(ip) {
    const now = Date.now();
    const windowKey = Math.floor(now / this.windowMs);
    const key = `${ip}:${windowKey}`;

    const currentCount = this.counters.get(key) || 0;

    if (currentCount >= this.limit) {
      return false;
    }

    this.counters.set(key, currentCount + 1);
    return true;
  }
}
```
</details>

### Exercise 2: Add Leak-Free Token Bucket with `consume()`
**Specification**: Implement a minimal `TokenBucket` class that replenishes tokens over time without running active background timers.

<details>
<summary>Solution</summary>

```javascript
export class SimpleTokenBucket {
  constructor(capacity, refillRatePerSec) {
    this.capacity = capacity;
    this.refillRate = refillRatePerSec;
    this.tokens = capacity;
    this.lastRefill = Date.now();
  }

  consume(amount = 1) {
    const now = Date.now();
    const elapsedSeconds = (now - this.lastRefill) / 1000;

    // Refill tokens
    this.tokens = Math.min(this.capacity, this.tokens + elapsedSeconds * this.refillRate);
    this.lastRefill = now;

    if (this.tokens >= amount) {
      this.tokens -= amount;
      return true;
    }

    return false;
  }
}
```
</details>
