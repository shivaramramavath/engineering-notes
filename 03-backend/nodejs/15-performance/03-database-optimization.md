# Database Query Optimization & Caching in Node.js

> Optimizing backend database performance: Connection pool sizing, query pipelining, Multi-level caching (Redis + In-memory), and EXPLAIN ANALYZE interpretation.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Databases** | PostgreSQL, Redis |
| **Level** | High-Scale Performance |

---

## 1. PostgreSQL Connection Pool Sizing Formula

A common misconception is that increasing connection pool size to 100 or 500 improves performance.
In reality, PostgreSQL processes compete for CPU cores, disk I/O, and memory bandwidth:

$$\text{Optimal Pool Size} = (\text{CPU Cores} \times 2) + \text{Disk Spindle Count}$$

For a database server with 4 CPU cores, an optimal pool size is **9–12 connections**.
Excessive connections cause context-switching overhead and CPU cache invalidations, degrading throughput.

---

## 2. Multi-Level Caching (L1 In-Memory + L2 Redis)

```javascript
import { LRUCache } from 'lru-cache';
import Redis from 'ioredis';

const redis = new Redis(process.env.REDIS_URL);

// L1: In-process memory cache (sub-millisecond, zero network hop)
const l1Cache = new LRUCache({
  max: 500,               // Max 500 items
  ttl: 1000 * 10,         // 10 seconds TTL
});

export async function getProductCached(productId) {
  // Check L1 (In-Memory)
  if (l1Cache.has(productId)) {
    return l1Cache.get(productId);
  }

  // Check L2 (Distributed Redis)
  const redisKey = `product:${productId}`;
  const cachedRedis = await redis.get(redisKey);
  if (cachedRedis) {
    const product = JSON.parse(cachedRedis);
    l1Cache.set(productId, product);
    return product;
  }

  // Fallback to Database
  const product = await db.query.products.findFirst({
    where: eq(products.id, productId),
  });

  if (product) {
    l1Cache.set(productId, product);
    await redis.set(redisKey, JSON.stringify(product), 'EX', 3600); // 1 hr TTL
  }

  return product;
}
```
Reduces database load by 95%+ during traffic spikes.
