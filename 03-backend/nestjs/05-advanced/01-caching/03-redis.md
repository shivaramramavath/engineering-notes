# Redis Integration Patterns in NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 01-caching
- **Prerequisites**: [01-caching-fundamentals](./01-caching-fundamentals.md), [02-cache-manager](./02-cache-manager.md), [03-backend/nodejs/07-databases/redis](../../nodejs)

---

## 1. Context & Purpose

While in-memory caching is adequate for single-process development, production enterprise backends run across multiple Kubernetes pods or autoscaling serverless instances. In a distributed environment, in-memory caches result in data desynchronization and low cache hit ratios.

**Redis** (Remote Dictionary Server) is the industry standard in-memory distributed data store. Beyond basic string caching, Redis provides rich data structures (Hashes, Sets, Sorted Sets, Bitmaps, Streams, HyperLogLogs), atomic operations, Pub/Sub channels, and distributed locks. This guide covers direct Redis integration using **ioredis**, cluster topologies, and advanced Redis patterns in NestJS.

---

## 2. Theoretical Foundations

### 2.1 Redis Data Structures
- **Strings**: Basic key-value pairs (caching serialized JSON, HTML pages, session tokens).
- **Hashes**: Object representations (`HSET user:1 name "Alice" age 30`), allowing field-level mutations without re-serializing entire objects.
- **Sorted Sets (ZSET)**: Elements scored by floating-point values, perfect for real-time leaderboards, rate limiters, and sliding window metrics.
- **Bitmaps & HyperLogLogs**: Hyper-compact structures for tracking unique daily active users (DAUs) across millions of visitors.

### 2.2 Redis Topologies
1. **Standalone**: Single Redis instance (local dev / small workloads).
2. **Sentinel**: Master-replica setup with automated failover monitoring.
3. **Cluster**: Sharded across multiple master nodes with automated hash-slot partitioning (`CRC16(key) % 16384`).

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   NestJS Application                   │
│   Controller ──> Service ──> RedisClient (ioredis)    │
└───────────────────────────┬────────────────────────────┘
                            │
              Persistent TCP Socket Connection
              - Auto-Reconnect with Exponential Backoff
              - Pipelining & Command Multiplexing
                            │
                            ▼
              ┌───────────────────────────┐
              │       Redis Server        │
              │   Single-Threaded Engine  │
              │   In-Memory RAM (< 1ms)   │
              │   RDB / AOF Persistence   │
              └───────────────────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Installing `ioredis`

`ioredis` is the preferred, feature-rich Redis client for Node.js, supporting Clusters, Sentinels, and Pipelines natively:
```bash
npm install --save ioredis
npm install --save-dev @types/ioredis
```

---

### 4.2 Building a Dedicated Redis Module

```typescript
// src/redis/redis.module.ts
import { Module, Global, OnApplicationShutdown, Inject } from '@nestjs/common';
import { ConfigModule, ConfigService } from '@nestjs/config';
import Redis from 'ioredis';

export const REDIS_CLIENT = Symbol('REDIS_CLIENT');

@Global()
@Module({
  imports: [ConfigModule],
  providers: [
    {
      provide: REDIS_CLIENT,
      useFactory: (config: ConfigService) => {
        const client = new Redis({
          host: config.get<string>('REDIS_HOST', 'localhost'),
          port: config.get<number>('REDIS_PORT', 6379),
          password: config.get<string>('REDIS_PASSWORD'),
          maxRetriesPerRequest: 3,
          enableReadyCheck: true,
          retryStrategy: (times) => {
            const delay = Math.min(times * 100, 3000);
            return delay;
          },
        });

        client.on('error', (err) => {
          console.error('[Redis Error]', err.message);
        });

        return client;
      },
      inject: [ConfigService],
    },
  ],
  exports: [REDIS_CLIENT],
})
export class RedisModule implements OnApplicationShutdown {
  constructor(@Inject(REDIS_CLIENT) private readonly redis: Redis) {}

  async onApplicationShutdown() {
    await this.redis.quit(); // Clean connection drain on shutdown
  }
}
```

---

### 4.3 Implementing Sliding Window Rate Limiter with Sorted Sets

A production sliding window rate limiter implemented in Redis:

```typescript
// src/common/guards/sliding-window-rate-limit.guard.ts
import {
  Injectable,
  CanActivate,
  ExecutionContext,
  HttpException,
  HttpStatus,
  Inject,
} from '@nestjs/common';
import { Redis } from 'ioredis';
import { REDIS_CLIENT } from '../../redis/redis.module';

@Injectable()
export class SlidingWindowRateLimitGuard implements CanActivate {
  constructor(@Inject(REDIS_CLIENT) private readonly redis: Redis) {}

  async canActivate(context: ExecutionContext): Promise<boolean> {
    const req = context.switchToHttp().getRequest();
    const ip = req.ip || 'anonymous';
    const key = `ratelimit:${ip}`;

    const now = Date.now();
    const windowMs = 60 * 1000; // 1 minute window
    const maxRequests = 100;    // 100 requests per minute

    const clearBefore = now - windowMs;

    // Execute atomic Redis pipeline
    const multi = this.redis.multi();
    // 1. Remove old timestamps outside the current sliding window
    multi.zremrangebyscore(key, 0, clearBefore);
    // 2. Add current request timestamp
    multi.zadd(key, now, `${now}-${Math.random()}`);
    // 3. Count requests within the window
    multi.zcard(key);
    // 4. Refresh key TTL
    multi.expire(key, 60);

    const results = await multi.exec();
    if (!results) {
      return true;
    }

    // Extract count from multi execution results
    const count = results[2][1] as number;

    if (count > maxRequests) {
      throw new HttpException(
        'Too Many Requests. Rate limit exceeded.',
        HttpStatus.TOO_MANY_REQUESTS,
      );
    }

    return true;
  }
}
```

---

### 4.4 Distributed Locks with Redlock Pattern

Preventing race conditions across microservice workers:

```typescript
// src/redis/redis-lock.service.ts
import { Injectable, Inject } from '@nestjs/common';
import { Redis } from 'ioredis';
import { randomUUID } from 'crypto';
import { REDIS_CLIENT } from './redis.module';

@Injectable()
export class RedisLockService {
  constructor(@Inject(REDIS_CLIENT) private readonly redis: Redis) {}

  async acquireLock(resource: string, ttlMs = 10000): Promise<string | null> {
    const lockId = randomUUID();
    // SET resource lockId NX PX ttlMs
    const acquired = await this.redis.set(
      `lock:${resource}`,
      lockId,
      'PX',
      ttlMs,
      'NX',
    );

    return acquired === 'OK' ? lockId : null;
  }

  async releaseLock(resource: string, lockId: string): Promise<boolean> {
    // Lua script: release lock only if the value matches lockId (avoids deleting another's lock)
    const luaScript = `
      if redis.call("get", KEYS[1]) == ARGV[1] then
        return redis.call("del", KEYS[1])
      else
        return 0
      end
    `;

    const result = await this.redis.eval(luaScript, 1, `lock:${resource}`, lockId);
    return result === 1;
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Redis Node Outage
If the Redis server restarts or network partitions, failing to configure `maxRetriesPerRequest` or error handlers can cause pending Node.js promises to hang indefinitely.
- **Handling**: Configure `maxRetriesPerRequest: 3` and use health checks (`@nestjs/terminus`) to report Redis degradation.

### 5.2 Hot Key Saturation
If a single cache key receives millions of requests per second, the single CPU core handling that hash slot becomes a bottleneck.
- **Handling**: Implement local L1 caching (in-memory) in front of Redis or duplicate hot keys across multiple read replicas.

---

## 6. Performance & Memory Implications

- **Pipelining**: Using `redis.pipeline()` or `redis.multi()` sends dozens of commands in a single network packet, reducing RTT overhead by up to 90%.
- **Memory Optimization**: Use Redis Hashes instead of storing thousands of small JSON strings to leverage Redis `ziplist` memory compression.

---

## 7. Anti-Patterns & Common Mistakes

- **Using the `KEYS *` Command in Production**: Executing `KEYS *` blocks the single-threaded Redis event loop for seconds, stalling all production traffic. Always use `SCAN` with cursor iteration!
- **Unbounded Keys Without TTL**: Writing session or cache records without an expiration timestamp, eventually exhausting Redis RAM.

---

## 8. Production Checklist & Operational Readiness

- [ ] `redis.quit()` executed during application shutdown lifecycle.
- [ ] Redis password and TLS encryption enabled for cloud instances (AWS ElastiCache / Redis Cloud).
- [ ] No `KEYS *` calls in codebase; `SCAN` utilized for pattern matching.
- [ ] Redis memory eviction set to `allkeys-lru` or `volatile-lru`.
- [ ] Atomic Lua scripts or Redlock used for distributed locks.

---

## 9. Migration & Version History

- **Redis 6/7**: Introduced ACLs (Access Control Lists) and Redis Functions.
- **ioredis 5.x**: Full TypeScript typings, ESM support, and Node 20/22 runtime stability.

---

## 10. Verification & Testing

Testing Redis lock acquisition:

```typescript
import { Test } from '@nestjs/testing';
import { RedisLockService } from './redis-lock.service';
import { REDIS_CLIENT } from './redis.module';

describe('RedisLockService', () => {
  let lockService: RedisLockService;
  let mockRedis: any;

  beforeEach(async () => {
    mockRedis = {
      set: vi.fn().mockResolvedValue('OK'),
      eval: vi.fn().mockResolvedValue(1),
    };

    const module = await Test.createTestingModule({
      providers: [
        RedisLockService,
        { provide: REDIS_CLIENT, useValue: mockRedis },
      ],
    }).compile();

    lockService = module.get<RedisLockService>(RedisLockService);
  });

  it('acquires lock with NX and PX flags', async () => {
    const lockId = await lockService.acquireLock('order_100', 5000);
    expect(lockId).toBeDefined();
    expect(mockRedis.set).toHaveBeenCalledWith(
      'lock:order_100',
      expect.any(String),
      'PX',
      5000,
      'NX',
    );
  });
});
```

---

## 11. Comprehensive Examples

### Non-Blocking Key Pattern Scanner (`SCAN` over `KEYS`)

```typescript
// src/redis/redis-scanner.util.ts
import { Redis } from 'ioredis';

export async function deleteKeysByPattern(redis: Redis, pattern: string): Promise<number> {
  let cursor = '0';
  let deletedCount = 0;

  do {
    // SCAN iterates keys in small batches without blocking the event loop
    const [nextCursor, keys] = await redis.scan(cursor, 'MATCH', pattern, 'COUNT', 100);
    cursor = nextCursor;

    if (keys.length > 0) {
      await redis.del(...keys);
      deletedCount += keys.length;
    }
  } while (cursor !== '0');

  return deletedCount;
}
```

---

## 12. Cross-References & Further Reading

- [02-cache-manager.md](./02-cache-manager.md) - Keyv Redis caching
- [04-cache-invalidation.md](./04-cache-invalidation.md) - Invalidation strategies
- [03-backend/nodejs/07-databases/redis](../../nodejs) - Node.js Redis fundamentals
- [ioredis Documentation](https://github.com/redis/ioredis)
