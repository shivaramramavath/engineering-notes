# 05-01 — Caching

Caching is a fundamental optimization technique for enterprise backends. It reduces database load, mitigates latency, and protects downstream microservices from traffic surges.

In **NestJS 11**, the caching subsystem was overhauled to adopt **`cache-manager` v6** with **Keyv** as the underlying unified engine. This module covers everything from in-memory caching to production Redis clusters, multi-tier L1/L2 architectures, and bulletproof cache invalidation.

---

## Module Index

| # | File | Core Topics |
|---|---|---|
| 01 | [Caching Fundamentals](./01-caching-fundamentals.md) | Cache Hit/Miss ratios, Cache-Aside, Write-Through, millisecond TTLs |
| 02 | [CacheManager & Keyv](./02-cache-manager.md) | `cache-manager` v6, Keyv stores, `@CacheKey`, `@CacheTTL`, custom interceptors |
| 03 | [Redis Integration](./03-redis.md) | `ioredis`, sliding-window rate limiters, Redlock distributed locking |
| 04 | [Cache Invalidation](./04-cache-invalidation.md) | Declarative invalidation interceptors, event-driven purging, surrogate keys |
| 05 | [Distributed Caching](./05-distributed-caching.md) | Redis Cluster, consistent hashing, hash tags, dual-tier L1/L2 synchronization |

---

## Key Learning Outcomes

1. Configure `@nestjs/cache-manager` with Keyv adapters in NestJS 11.
2. Avoid the common millisecond TTL conversion pitfalls.
3. Build atomic distributed locks and rate limiters with Redis and Lua scripts.
4. Synchronize local process memory caches across multi-pod Kubernetes clusters using Redis Pub/Sub.
5. Invalidate caches surgically without triggering database thundering herds.
