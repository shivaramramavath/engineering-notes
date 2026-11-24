# CacheManager and Keyv Architecture in NestJS 11

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 01-caching
- **Prerequisites**: [01-caching-fundamentals](./01-caching-fundamentals.md), [03-core-concepts/04-modules-and-di/01-dependency-injection](../../03-core-concepts/04-modules-and-di/01-dependency-injection.md)

---

## 1. Context & Purpose

In NestJS 11, the caching subsystem underwent a major architectural overhaul. Previous versions of NestJS relied on `cache-manager` v4 and v5 with fragmented, bespoke store adapters (e.g. `cache-manager-redis-store`, `cache-manager-ioredis`).

In NestJS 11, `@nestjs/cache-manager` adopts **`cache-manager` v6**, which standardizes all storage backends on **Keyv**. Keyv provides a unified, highly optimized key-value storage engine supporting tiered multi-caching, unified TTL handling (in milliseconds), native Buffer and JSON serialization, and pluggable storage adapters (`@keyv/redis`, `@keyv/memcache`, `@keyv/sqlite`, `@keyv/mongo`).

---

## 2. Theoretical Foundations

Keyv abstracts storage drivers behind a consistent Promise-based interface:
- **Unified Serialization**: Serializes JavaScript values using standard JSON or fast binary serializers.
- **Namespacing**: Automatically prefixes keys (`namespace:key`) to prevent key collisions across environments or multi-tenant domains.
- **Tiered Multi-Caching (L1/L2)**: Keyv allows layering a fast L1 in-memory cache in front of an L2 distributed Redis cache. When reading, L1 is queried first (< 0.1ms). On miss, L2 is queried (1ms) and populates L1 automatically.

---

## 3. Architecture & Internal Mechanics

```
NestJS Service / CacheInterceptor
               │
               ▼
   CACHE_MANAGER (cache-manager v6)
               │
               ▼
     Keyv Unified Engine
               │
       ┌───────┴───────┐
       ▼               ▼
  L1 Cache        L2 Cache
(In-Memory Keyv)  (@keyv/redis)
   < 0.05ms          ~1.5ms
```

In `cache-manager` v6, the legacy `store` option passing a package function is deprecated. Stores are passed directly via Keyv instances or the `stores` array.

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies for NestJS 11

Install `@nestjs/cache-manager` and the appropriate Keyv adapter:
```bash
npm install --save @nestjs/cache-manager cache-manager@^6.0.0 @keyv/redis
```

### 4.2 Configuring Asynchronous CacheModule with Keyv

```typescript
// src/common/cache/cache.module.ts
import { Module } from '@nestjs/common';
import { CacheModule } from '@nestjs/cache-manager';
import { ConfigModule, ConfigService } from '@nestjs/config';
import { createKeyv } from '@keyv/redis';
import Keyv from 'keyv';

@Module({
  imports: [
    CacheModule.registerAsync({
      imports: [ConfigModule],
      useFactory: (config: ConfigService) => {
        const redisUri = config.getOrThrow<string>('REDIS_URI');

        // Multi-tier cache: L1 In-Memory + L2 Redis via Keyv
        const memoryStore = new Keyv({ namespace: 'app-l1' });
        const redisStore = createKeyv(redisUri);

        return {
          stores: [memoryStore, redisStore],
          ttl: 60000, // 60 seconds (in milliseconds)
        };
      },
      inject: [ConfigService],
      isGlobal: true,
    }),
  ],
})
export class AppCacheModule {}
```

---

### 4.3 Using Automatic `CacheInterceptor` on Controllers

```typescript
// src/users/users.controller.ts
import {
  Controller,
  Get,
  Param,
  UseInterceptors,
} from '@nestjs/common';
import { CacheInterceptor, CacheKey, CacheTTL } from '@nestjs/cache-manager';
import { UsersService } from './users.service';

@Controller('users')
@UseInterceptors(CacheInterceptor) // Automatically caches GET requests
export class UsersController {
  constructor(private readonly usersService: UsersService) {}

  @Get()
  @CacheKey('all_active_users') // Custom static cache key
  @CacheTTL(120000)             // 2 minutes in milliseconds
  async findAll() {
    return this.usersService.findAll();
  }

  @Get(':id')
  // By default, CacheInterceptor generates key from HTTP URL path: /users/123
  async findOne(@Param('id') id: string) {
    return this.usersService.findById(id);
  }
}
```

---

### 4.4 Programmatic Keyv Operations

```typescript
// src/catalog/catalog.service.ts
import { Injectable, Inject } from '@nestjs/common';
import { CACHE_MANAGER } from '@nestjs/cache-manager';
import { Cache } from 'cache-manager';

@Injectable()
export class CatalogService {
  constructor(@Inject(CACHE_MANAGER) private readonly cache: Cache) {}

  async setPrice(productId: string, price: number) {
    // Write to cache with 1-hour TTL
    await this.cache.set(`price:${productId}`, price, 3600000);
  }

  async getPrice(productId: string): Promise<number | null> {
    const price = await this.cache.get<number>(`price:${productId}`);
    return price ?? null;
  }

  async invalidatePrice(productId: string) {
    await this.cache.del(`price:${productId}`);
  }

  async clearAllCatalog() {
    // Clears the entire cache store
    await this.cache.clear();
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Using Deprecated `cache-manager-redis-store`
If upgrading from NestJS 10, retaining `import * as redisStore from 'cache-manager-redis-store'` causes startup exceptions in NestJS 11:
`TypeError: store is not a function / adapter incompatible with cache-manager v6`.
- **Fix**: Replace with `@keyv/redis` and pass `stores: [createKeyv('redis://...')]`.

### 5.2 Non-GET Requests Bypassing Interceptor
By default, `CacheInterceptor` intercepts only HTTP `GET` requests. `POST`, `PUT`, `PATCH`, and `DELETE` requests automatically bypass the interceptor without caching.

---

## 6. Performance & Memory Implications

- **L1/L2 Tiering**: Serving 90% of requests from L1 RAM reduces Redis network I/O to a fraction of normal load, eliminating Redis connection bottlenecks during traffic spikes.
- **Serialization Costs**: Deeply nested, circular JavaScript objects will fail during Keyv serialization. Cache flat DTOs or pre-serialized JSON strings.

---

## 7. Anti-Patterns & Common Mistakes

- **Caching User-Specific Data under Static Keys**: Putting `@CacheKey('profile')` on `@Get('profile')`. Every user receives the cached profile of the first user who hit the route!
- **Not Handling Redis Connection Drops**: Omitting error listeners on Keyv Redis stores, causing unhandled promise rejections if the Redis cluster restarts.

---

## 8. Production Checklist & Operational Readiness

- [ ] `cache-manager` v6 and `@keyv/redis` used in NestJS 11.
- [ ] User-specific endpoints generate dynamic keys or bypass global interceptors.
- [ ] Redis URI loaded from secure secret managers.
- [ ] Redis cluster configured with maxmemory and eviction policies (`volatile-lru`).

---

## 9. Migration & Version History

- **NestJS 8/9/10**: `cache-manager` v4/v5 with custom stores.
- **NestJS 11.x**: Standardized on `cache-manager` v6 with **Keyv** storage engines and mandatory millisecond TTLs.

---

## 10. Verification & Testing

Testing cache operations programmatically:

```typescript
import { Test } from '@nestjs/testing';
import { CacheModule, CACHE_MANAGER } from '@nestjs/cache-manager';
import { Cache } from 'cache-manager';

describe('Keyv Cache Verification', () => {
  let cache: Cache;

  beforeAll(async () => {
    const module = await Test.createTestingModule({
      imports: [CacheModule.register({ ttl: 5000 })],
    }).compile();

    cache = module.get<Cache>(CACHE_MANAGER);
  });

  it('stores and retrieves values correctly', async () => {
    await cache.set('test_key', { foo: 'bar' }, 5000);
    const value = await cache.get('test_key');
    expect(value).toEqual({ foo: 'bar' });
  });

  it('deletes values correctly', async () => {
    await cache.set('delete_me', 123, 5000);
    await cache.del('delete_me');
    const value = await cache.get('delete_me');
    expect(value).toBeUndefined();
  });
});
```

---

## 11. Comprehensive Examples

### Custom Dynamic Cache Key Generator

```typescript
// src/common/interceptors/user-specific-cache.interceptor.ts
import { Injectable, ExecutionContext } from '@nestjs/common';
import { CacheInterceptor } from '@nestjs/cache-manager';

@Injectable()
export class UserSpecificCacheInterceptor extends CacheInterceptor {
  trackBy(context: ExecutionContext): string | undefined {
    const req = context.switchToHttp().getRequest();
    const userId = req.user?.id || 'anonymous';
    const path = req.originalUrl || req.url;

    // Cache key dynamically partitioned by user ID!
    return `cache:user:${userId}:${path}`;
  }
}
```

---

## 12. Cross-References & Further Reading

- [01-caching-fundamentals.md](./01-caching-fundamentals.md) - Caching theory
- [03-redis.md](./03-redis.md) - Dedicated Redis patterns
- [04-cache-invalidation.md](./04-cache-invalidation.md) - Invalidation techniques
- [Keyv Official GitHub Repository](https://github.com/jaredwray/keyv)
