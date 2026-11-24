# Caching Fundamentals in Backend Systems

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 01-caching
- **Prerequisites**: [02-fundamentals/01-architecture-overview](../../02-fundamentals/01-architecture-overview.md), [00-prerequisites/03-nodejs-async-and-event-loop](../../00-prerequisites/03-nodejs-async-and-event-loop.md)

---

## 1. Context & Purpose

In web applications, database queries, external API calls, and complex data computations are often the primary performance bottlenecks. As traffic scales, hitting a relational database for repeated, identical queries degrades response times and can exhaust database connection pools.

**Caching** stores precomputed results in high-speed, volatile storage (in-memory RAM or distributed stores like Redis/Memcached) so that future requests for the same data are served in sub-millisecond time. In NestJS 11, caching is governed by `@nestjs/cache-manager` integrated with **`cache-manager` v6** and **Keyv**, providing a unified storage abstraction across memory and distributed tiers.

---

## 2. Theoretical Foundations

### 2.1 The Cache Hit / Miss Ratio
The efficiency of any cache is measured by the Cache Hit Ratio ($H$):
$$H = \frac{\text{Hits}}{\text{Hits} + \text{Misses}}$$
- **Cache Hit**: Data is located in cache; returned immediately.
- **Cache Miss**: Data is absent from cache; fetched from source-of-truth database, populated into cache, and returned.

### 2.2 Common Caching Patterns
1. **Cache-Aside (Lazy Loading)**: The application code checks the cache first. If a miss occurs, the application queries the database, writes the result to the cache with a TTL, and returns it.
2. **Read-Through**: The application queries the cache layer directly; the cache layer fetches from the DB transparently on misses.
3. **Write-Through**: The application writes to the cache and the database simultaneously.
4. **Write-Behind (Write-Back)**: The application writes to the cache immediately; the cache asynchronously writes back to the database in batches.

---

## 3. Architecture & Internal Mechanics

```
Client Request (GET /products/100)
              │
              ▼
    NestJS CacheInterceptor / Service
              │
      Query Key: `product:100`
              │
      ┌───────┴───────┐
      ▼               ▼
[Cache HIT]     [Cache MISS]
  (< 1ms)             │
  Return RAM/Redis    ├─> Query PostgreSQL Database (25ms)
                      ├─> Store in Cache with TTL (e.g. 60s)
                      │
                      ▼
               Return Response to Client
```

---

## 4. Concrete Implementation

### 4.1 In-Memory Caching Setup in NestJS 11

In NestJS 11, `@nestjs/cache-manager` uses `cache-manager` v6. By default, an in-memory Keyv store is initialized:

```typescript
// src/app.module.ts
import { Module } from '@nestjs/common';
import { CacheModule, CacheInterceptor } from '@nestjs/cache-manager';
import { APP_INTERCEPTOR } from '@nestjs/core';
import { ProductsController } from './products/products.controller';
import { ProductsService } from './products/products.service';

@Module({
  imports: [
    CacheModule.register({
      ttl: 60000, // In NestJS 11 / cache-manager v6, TTL is in MILLISECONDS (60s = 60000)
      max: 1000,  // Maximum number of items in memory cache
      isGlobal: true,
    }),
  ],
  controllers: [ProductsController],
  providers: [
    ProductsService,
    // Optional: enable automatic controller-level caching globally
    {
      provide: APP_INTERCEPTOR,
      useClass: CacheInterceptor,
    },
  ],
})
export class AppModule {}
```

---

### 4.2 Programmatic Caching via Injected `CACHE_MANAGER`

```typescript
// src/products/products.service.ts
import { Injectable, Inject, Logger } from '@nestjs/common';
import { CACHE_MANAGER } from '@nestjs/cache-manager';
import { Cache } from 'cache-manager';

export interface Product {
  id: string;
  name: string;
  price: number;
}

@Injectable()
export class ProductsService {
  private readonly logger = new Logger(ProductsService.name);

  constructor(@Inject(CACHE_MANAGER) private readonly cacheManager: Cache) {}

  async getProductById(id: string): Promise<Product> {
    const cacheKey = `product:${id}`;

    // 1. Check cache
    const cached = await this.cacheManager.get<Product>(cacheKey);
    if (cached) {
      this.logger.log(`Cache HIT for ${cacheKey}`);
      return cached;
    }

    // 2. Cache MISS: query database
    this.logger.log(`Cache MISS for ${cacheKey}. Querying DB...`);
    const product = await this.fetchFromDb(id);

    // 3. Populate cache with 5-minute TTL (300,000 ms)
    await this.cacheManager.set(cacheKey, product, 300000);

    return product;
  }

  private async fetchFromDb(id: string): Promise<Product> {
    // Simulated DB query latency
    await new Promise((resolve) => setTimeout(resolve, 50));
    return { id, name: 'Wireless Headphones', price: 99.99 };
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Cache Stampede (Thundering Herd)
When a hot cache key expires, thousands of concurrent requests all experience a cache miss simultaneously, overloading the database with identical queries.
- **Remedy**: Use Mutex locks, probabilistic early expiration (XFetch algorithm), or pre-warming background crons.

### 5.2 TTL Unit Shift in NestJS 11
In `cache-manager` v4, TTL was defined in **seconds**. In `cache-manager` v5 and v6, TTL is strictly defined in **milliseconds**. Passing `ttl: 60` causes keys to expire in 60 milliseconds instead of 60 seconds!
- **Rule**: Always pass milliseconds (e.g. `60 * 1000`).

---

## 6. Performance & Memory Implications

- **Latency Reduction**: Reading from memory takes ~0.05ms; reading from PostgreSQL takes ~5–50ms.
- **Process Memory Limits**: When using in-memory stores, large cached datasets increase the Node.js V8 heap. Without a strict `max` item limit, cached objects cause Out-Of-Memory crashes.

---

## 7. Anti-Patterns & Common Mistakes

- **Caching Unbounded User Search Results**: Caching every single user query permutation with no LRU eviction limit.
- **Passing Seconds to `cacheManager.set()`**: Setting `ttl: 300` expecting 5 minutes, resulting in instant 300ms expirations.
- **Using In-Memory Caching Across Multi-Pod Clusters**: In Kubernetes, Pod A caches an update while Pod B serves stale data from its independent memory cache.

---

## 8. Production Checklist & Operational Readiness

- [ ] All TTL values explicitly specified in milliseconds.
- [ ] Memory store configured with `max` items to prevent heap overflow.
- [ ] Cache Hit/Miss metrics exported to Prometheus / Datadog.
- [ ] Distributed cache (Redis) used when scaling to multiple server replicas.

---

## 9. Migration & Version History

- **NestJS 9.x**: Deprecated `@nestjs/common` CacheModule and moved to `@nestjs/cache-manager`.
- **NestJS 11.x**: Built on `cache-manager` v6 with **Keyv** storage engine.

---

## 10. Verification & Testing

Unit testing cache service logic:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { CACHE_MANAGER } from '@nestjs/cache-manager';
import { ProductsService } from './products.service';

describe('ProductsService Caching', () => {
  let service: ProductsService;
  let mockCache: any;

  beforeEach(async () => {
    mockCache = {
      get: vi.fn(),
      set: vi.fn(),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        ProductsService,
        { provide: CACHE_MANAGER, useValue: mockCache },
      ],
    }).compile();

    service = module.get<ProductsService>(ProductsService);
  });

  it('returns cached item on cache hit', async () => {
    const item = { id: 'p1', name: 'Item', price: 10 };
    mockCache.get.mockResolvedValue(item);

    const result = await service.getProductById('p1');
    expect(result).toEqual(item);
    expect(mockCache.set).not.toHaveBeenCalled();
  });
});
```

---

## 11. Comprehensive Examples

### Cache-Aside Wrapper Helper

```typescript
// src/common/cache/cache-aside.util.ts
import { Cache } from 'cache-manager';

export async function getOrSetCache<T>(
  cacheManager: Cache,
  key: string,
  ttlMs: number,
  fetcher: () => Promise<T>,
): Promise<T> {
  const cached = await cacheManager.get<T>(key);
  if (cached !== undefined && cached !== null) {
    return cached;
  }

  const fresh = await fetcher();
  await cacheManager.set(key, fresh, ttlMs);
  return fresh;
}
```

---

## 12. Cross-References & Further Reading

- [02-cache-manager.md](./02-cache-manager.md) - Deep dive into CacheModule & Keyv
- [03-redis.md](./03-redis.md) - Distributed caching with Redis
- [04-cache-invalidation.md](./04-cache-invalidation.md) - Cache invalidation strategies
- [NestJS Caching Documentation](https://docs.nestjs.com/techniques/caching)
