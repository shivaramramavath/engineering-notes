# 07 · Caching

Caching is the most common reason to run Redis, and the easiest thing to get subtly wrong. This module covers the patterns, the trade-offs, and the failure modes, with production-shaped Node.js code.

## Lessons

| # | Lesson | Core idea |
|---|--------|-----------|
| 01 | [Cache-Aside](./01_cache-aside.md) | The default pattern: the app checks the cache, then the database |
| 02 | [Cache Strategies](./02_cache-strategies.md) | Read-through, write-through, write-behind, write-around, refresh-ahead |
| 03 | [Cache Invalidation](./03_cache-invalidation.md) | TTL, explicit deletes, tags, versions, races |
| 04 | [Cache Problems](./04_cache-problems.md) | Stampede, penetration, breakdown, avalanche |
| 05 | [Caching API Responses](./05_caching-api-responses.md) | An Express middleware with keys, tags, and headers |

## Learning outcomes

After this module you can:

- Implement cache-aside with TTL jitter, negative caching and fail-open behavior
- Choose a write strategy based on consistency and durability needs
- Invalidate caches without leaving stale data behind
- Protect the database from stampedes, penetration and outages
- Cache HTTP responses safely, including per-user and per-language variants

## The mental model

```
                 ┌────────────┐   hit    ┌──────────┐
 request ───────►│   Cache    │─────────►│ response │
                 │  (Redis)   │          └──────────┘
                 └─────┬──────┘
                  miss │  ▲ populate
                       ▼  │
                 ┌────────────┐
                 │  Database  │   the source of truth
                 └────────────┘
```

Three rules keep caches healthy:

1. **The cache is disposable.** Everything in it must be rebuildable from the source of truth
2. **Every cache key has a TTL.** It is your safety net when invalidation fails
3. **A cache failure must not become an outage.** Fail open, and protect the database

## Measure before and after

Track the **hit ratio** and the **load on the database**. A cache with a 30% hit ratio may cost more than it saves.

```
hit ratio = hits / (hits + misses)
```

## Prerequisites

- Completed [06_advanced-commands](../06_advanced-commands/README.md), especially [atomic ops](../06_advanced-commands/04_atomic-and-conditional-ops.md) and [Lua](../06_advanced-commands/03_lua-scripts.md)
- Comfort with [expiration](../02_redis-fundamentals/03_expiration-and-ttl.md) and [eviction](../02_redis-fundamentals/05_memory-and-eviction.md)

## Next

Continue to [08_nodejs-integration](../08_nodejs-integration/README.md).
