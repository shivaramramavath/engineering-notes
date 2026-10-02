# 12 · Rate Limiting

Rate limiting protects your system from overload, abuse and runaway clients, and keeps shared resources fair. Redis is the standard place to keep the counters, because it is fast, atomic and shared across every app instance.

## Lessons

| # | Lesson | Core idea |
|---|--------|-----------|
| 01 | [Fixed and Sliding Window](./01_fixed-and-sliding-window.md) | Counting requests per window, the boundary-burst problem, sliding log and sliding counter |
| 02 | [Token and Leaky Bucket](./02_token-and-leaky-bucket.md) | Bursts with a steady average, GCRA, traffic shaping, concurrency limits |
| 03 | [Distributed Rate Limiter](./03_distributed-rate-limiter.md) | A production Express middleware: Lua, headers, fallbacks, bans, plans, testing |

## Learning outcomes

After this module you can:

- Implement fixed window, sliding window, token bucket, leaky bucket and GCRA limiters atomically in Lua
- Explain the accuracy, memory and burst trade-offs of each, and pick one on purpose
- Return correct `429`, `Retry-After` and `RateLimit-*` responses
- Decide fail-open or fail-closed per route, and survive Redis outages
- Protect login and other abuse-prone endpoints, and apply per-plan limits
- Test limiters for correctness under concurrency

## Choosing an algorithm

| Algorithm | Allows bursts? | Accuracy | Memory per key | Best for |
|-----------|----------------|----------|----------------|----------|
| **Fixed window** | Up to **2×** at window edges | Low at edges | O(1) | Simple quotas, coarse limits, daily caps |
| **Sliding window log** | No (exact) | Exact | O(limit) | Small limits where exactness matters (login attempts) |
| **Sliding window counter** | Smoothed | Approximate (~good) | O(1) | General API limits at scale |
| **Token bucket** | **Yes**, up to the bucket size | Exact | O(1) | APIs that allow short bursts, with a steady average |
| **GCRA** | Yes (same as token bucket) | Exact | O(1), one number | Precise, memory-light, high volume |
| **Leaky bucket (meter)** | Limited | Exact | O(1) | Smooth, steady intake |
| **Leaky bucket (queue)** | No | n/a | O(queue) | **Shaping** outgoing traffic to an exact rate |
| **Concurrency limit** | n/a | Exact | O(in-flight) | "At most N at the same time" |

```
Need a quota ("1000 per day")?                  ─► fixed window
Need exactness on a small limit?                ─► sliding window log
Need a general API limit, cheap and smooth?     ─► sliding window counter, or GCRA
Need bursts allowed, steady average enforced?   ─► token bucket / GCRA
Need outgoing calls at an exact steady rate?    ─► leaky bucket queue (or BullMQ limiter)
Need a cap on parallel work?                    ─► concurrency limit (semaphore)
```

## The five decisions behind every limiter

| Decision | Options |
|----------|---------|
| **Who** is limited? | IP, user ID, API key, tenant, route, or a combination |
| **How much?** | Requests (or *cost units*) per window or rate, plus burst |
| **What happens when exceeded?** | `429` with `Retry-After`, queue, degrade, temporary ban |
| **What if Redis is down?** | Fail open (availability) or fail closed (protection), per route |
| **Where does it run?** | Edge or CDN, API gateway, application middleware (this module) |

A good design uses **layers**: coarse protection at the edge (CDN or gateway), precise per-user and per-route limits in the app.

## Prerequisites

- Completed [11_distributed-locks](../11_distributed-locks/README.md)
- Comfortable with [Lua scripts](../06_advanced-commands/03_lua-scripts.md), since every limiter here is a script, and [atomic operations](../06_advanced-commands/04_atomic-and-conditional-ops.md)
- The `RateLimiterPort` and Express wiring from [Express Integration](../08_nodejs-integration/06_express-integration.md)

## Next

Continue to [13_session-management](../13_session-management/README.md).
