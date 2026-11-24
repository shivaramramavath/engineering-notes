# Cache Invalidation Strategies

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 01-caching
- **Prerequisites**: [01-caching-fundamentals](./01-caching-fundamentals.md), [02-cache-manager](./02-cache-manager.md), [03-core-concepts/01-request-pipeline/04-interceptors](../../03-core-concepts/01-request-pipeline/04-interceptors.md)

---

## 1. Context & Purpose

*"There are only two hard things in Computer Science: cache invalidation and naming things."* — Phil Karlton.

Caching data is straightforward; keeping that cached data synchronized with database mutations without serving stale records or dropping overall cache efficiency is complex. When an entity is updated via `PATCH /users/1`, multiple cached entries immediately become stale:
- The individual entity cache: `user:1`.
- Paginated listing caches: `users:page:1`, `users:page:2`.
- Aggregation caches: `users:count`, `tenant:42:users`.

This guide explores production-grade cache invalidation architectures in NestJS: **Time-To-Live (TTL)** expiration, **Explicit Event-Driven Invalidation**, **Tag-Based Invalidation (Surrogate Keys)**, and declarative **Invalidation Interceptors**.

---

## 2. Theoretical Foundations

### 2.1 The Invalidation Approaches
1. **TTL-Based (Passive)**: Keys expire automatically after a set duration. Suitable when data changes infrequently or clients tolerate eventual consistency.
2. **Explicit / Write-Through (Active)**: Immediately upon a database update, the application triggers `cache.del(key)`. Guarantees immediate consistency.
3. **Event-Driven**: Services emit domain events (`UserUpdatedEvent`). Dedicated cache subscribers listen to the event bus and invalidate affected keys asynchronously.
4. **Surrogate Keys / Tagging**: Cache entries are tagged with metadata tags (`tag:user-1`, `tag:users-list`). Invalidating `tag:user-1` purges all related query permutations atomically.

---

## 3. Architecture & Internal Mechanics

```
Client sends PATCH /articles/42 (Title changed)
                   │
                   ▼
        ArticlesController.update()
                   │
                   ▼
           ArticlesService
                   │
         1. Update PostgreSQL DB
                   │
                   ▼
     Emit Event: ArticleUpdatedEvent(42)
                   │
         ┌─────────┴─────────┐
         ▼                   ▼
    Cache Listener       Search Indexer
         │
    Invalidate Keys:
    - article:42
    - articles:feed:*
    - author:10:articles
                   │
                   ▼
        Cache Cleared in Redis
```

---

## 4. Concrete Implementation

### 4.1 Custom Declarative Invalidation Decorator & Interceptor

Let's build a declarative decorator `@InvalidateCache(['users:*', 'user:{id}'])`:

#### Step 1: Define Invalidation Decorator

```typescript
// src/common/cache/invalidate-cache.decorator.ts
import { SetMetadata } from '@nestjs/common';

export const INVALIDATE_CACHE_KEY = 'INVALIDATE_CACHE_METADATA';

export const InvalidateCache = (...patterns: string[]) =>
  SetMetadata(INVALIDATE_CACHE_KEY, patterns);
```

---

#### Step 2: Implement Invalidation Interceptor

```typescript
// src/common/cache/invalidate-cache.interceptor.ts
import {
  Injectable,
  NestInterceptor,
  ExecutionContext,
  CallHandler,
  Inject,
} from '@nestjs/common';
import { Reflector } from '@nestjs/core';
import { Observable } from 'rxjs';
import { tap } from 'rxjs/operators';
import { Redis } from 'ioredis';
import { INVALIDATE_CACHE_KEY } from './invalidate-cache.decorator';
import { REDIS_CLIENT } from '../../redis/redis.module';

@Injectable()
export class InvalidateCacheInterceptor implements NestInterceptor {
  constructor(
    private readonly reflector: Reflector,
    @Inject(REDIS_CLIENT) private readonly redis: Redis,
  ) {}

  intercept(context: ExecutionContext, next: CallHandler): Observable<any> {
    const patterns = this.reflector.get<string[]>(
      INVALIDATE_CACHE_KEY,
      context.getHandler(),
    );

    if (!patterns || patterns.length === 0) {
      return next.handle();
    }

    const req = context.switchToHttp().getRequest();

    return next.handle().pipe(
      tap({
        next: async () => {
          // Resolve template parameters e.g. 'user:{id}' -> 'user:123'
          for (const rawPattern of patterns) {
            const resolvedPattern = this.interpolate(rawPattern, req.params);
            await this.purgePattern(resolvedPattern);
          }
        },
      }),
    );
  }

  private interpolate(pattern: string, params: Record<string, string>): string {
    return pattern.replace(/{(\w+)}/g, (_, key) => params[key] || '*');
  }

  private async purgePattern(pattern: string) {
    let cursor = '0';
    do {
      const [nextCursor, keys] = await this.redis.scan(
        cursor,
        'MATCH',
        pattern,
        'COUNT',
        100,
      );
      cursor = nextCursor;
      if (keys.length > 0) {
        await this.redis.del(...keys);
      }
    } while (cursor !== '0');
  }
}
```

---

#### Step 3: Decorating Mutation Handlers

```typescript
// src/articles/articles.controller.ts
import { Controller, Patch, Delete, Param, Body, UseInterceptors } from '@nestjs/common';
import { InvalidateCache } from '../common/cache/invalidate-cache.decorator';
import { InvalidateCacheInterceptor } from '../common/cache/invalidate-cache.interceptor';
import { ArticlesService } from './articles.service';

@Controller('articles')
@UseInterceptors(InvalidateCacheInterceptor)
export class ArticlesController {
  constructor(private readonly articlesService: ArticlesService) {}

  @Patch(':id')
  @InvalidateCache('article:{id}', 'articles:feed:*', 'articles:page:*')
  async updateArticle(@Param('id') id: string, @Body() dto: any) {
    return this.articlesService.update(id, dto);
  }

  @Delete(':id')
  @InvalidateCache('article:{id}', 'articles:*')
  async deleteArticle(@Param('id') id: string) {
    return this.articlesService.delete(id);
  }
}
```

---

### 4.2 Event-Driven Cache Invalidation (Decoupled Pattern)

Using NestJS `EventEmitter2` for asynchronous, resilient invalidation:

```typescript
// src/articles/events/article-updated.event.ts
export class ArticleUpdatedEvent {
  constructor(public readonly articleId: string, public readonly authorId: string) {}
}
```

```typescript
// src/common/cache/cache-event-listener.service.ts
import { Injectable, Inject } from '@nestjs/common';
import { OnEvent } from '@nestjs/event-emitter';
import { CACHE_MANAGER } from '@nestjs/cache-manager';
import { Cache } from 'cache-manager';
import { ArticleUpdatedEvent } from '../../articles/events/article-updated.event';

@Injectable()
export class CacheEventListener {
  constructor(@Inject(CACHE_MANAGER) private readonly cache: Cache) {}

  @OnEvent('article.updated', { async: true })
  async handleArticleUpdated(event: ArticleUpdatedEvent) {
    // Purge entity cache
    await this.cache.del(`article:${event.articleId}`);
    // Purge author's article list
    await this.cache.del(`author:${event.authorId}:articles`);
    console.log(`[Cache Invalidation] Purged caches for article ${event.articleId}`);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Invalidation Race Conditions
If a database write completes, invalidation fires, but a concurrent slow read from an uncommitted transaction reads stale data and immediately repopulates the cache.
- **Handling**: Use **Delayed Invalidation (Double Deletion)**: Purge the cache immediately, then schedule a secondary purge 500ms later to clear any concurrent stale writes.

### 5.2 Failure of Cache Deletion
If the database write succeeds but the Redis call fails (e.g. network blip), the cache remains stale until the TTL expires.
- **Remedy**: Always pair active invalidation with reasonable TTL safety boundaries (e.g. 10–30 minutes) as a fallback.

---

## 6. Performance & Memory Implications

- **Scan vs Keys**: Never use `KEYS *` for pattern matching; it freezes the Redis event loop. Always use `SCAN` with cursor iteration.
- **Selective Purging**: Purging only affected keys (`article:42`) maintains high cache hit ratios, whereas calling `cache.clear()` wipes all unrelated caches, triggering database thundering herds.

---

## 7. Anti-Patterns & Common Mistakes

- **Calling `cache.clear()` on Every Update**: Clearing the entire application cache whenever a user edits their bio.
- **Relying Exclusively on TTL for Critical Data**: Giving sensitive inventory or authorization permission records a 24-hour TTL with zero active invalidation.
- **Deleting Before Database Commit**: Purging the cache before the SQL transaction commits. If the transaction rolls back, valid data was purged.

---

## 8. Production Checklist & Operational Readiness

- [ ] Cache invalidation executes **after** database transactions successfully commit.
- [ ] Pattern deletions use non-blocking `SCAN` iteration.
- [ ] Fallback TTL configured on every cached key to prevent permanent staleness.
- [ ] Event-driven invalidation uses asynchronous queues for high-volume systems.

---

## 9. Migration & Version History

- **NestJS 10/11**: Full support for reactive RxJS pipe interceptors and Keyv multi-store invalidation.

---

## 10. Verification & Testing

Testing cache invalidation interceptor:

```typescript
import { Test } from '@nestjs/testing';
import { InvalidateCacheInterceptor } from './invalidate-cache.interceptor';
import { Reflector } from '@nestjs/core';
import { of } from 'rxjs';
import { REDIS_CLIENT } from '../../redis/redis.module';

describe('InvalidateCacheInterceptor', () => {
  let interceptor: InvalidateCacheInterceptor;
  let mockRedis: any;

  beforeEach(async () => {
    mockRedis = {
      scan: vi.fn().mockResolvedValue(['0', ['article:100']]),
      del: vi.fn().mockResolvedValue(1),
    };

    const module = await Test.createTestingModule({
      providers: [
        InvalidateCacheInterceptor,
        { provide: Reflector, useValue: { get: vi.fn().mockReturnValue(['article:{id}']) } },
        { provide: REDIS_CLIENT, useValue: mockRedis },
      ],
    }).compile();

    interceptor = module.get<InvalidateCacheInterceptor>(InvalidateCacheInterceptor);
  });

  it('purges matched keys after handler completes', async () => {
    const mockContext: any = {
      getHandler: () => {},
      switchToHttp: () => ({ getRequest: () => ({ params: { id: '100' } }) }),
    };

    const callHandler: any = { handle: () => of({ success: true }) };

    await new Promise((resolve) => {
      interceptor.intercept(mockContext, callHandler).subscribe({
        complete: () => resolve(true),
      });
    });

    expect(mockRedis.del).toHaveBeenCalledWith('article:100');
  });
});
```

---

## 11. Comprehensive Examples

### Surrogate Key Tagging Pattern

```typescript
// src/common/cache/tag-cache.service.ts
import { Injectable, Inject } from '@nestjs/common';
import { Redis } from 'ioredis';
import { REDIS_CLIENT } from '../../redis/redis.module';

@Injectable()
export class TagCacheService {
  constructor(@Inject(REDIS_CLIENT) private readonly redis: Redis) {}

  async setWithTags(key: string, value: any, tags: string[], ttlSeconds: number) {
    const pipeline = this.redis.pipeline();
    pipeline.set(key, JSON.stringify(value), 'EX', ttlSeconds);

    // Track which keys belong to which tags
    for (const tag of tags) {
      pipeline.sadd(`tag:${tag}`, key);
      pipeline.expire(`tag:${tag}`, ttlSeconds + 3600);
    }

    await pipeline.exec();
  }

  async invalidateTag(tag: string) {
    const tagKey = `tag:${tag}`;
    const keys = await this.redis.smembers(tagKey);

    if (keys.length > 0) {
      const pipeline = this.redis.pipeline();
      pipeline.del(...keys);
      pipeline.del(tagKey);
      await pipeline.exec();
    }
  }
}
```

---

## 12. Cross-References & Further Reading

- [01-caching-fundamentals.md](./01-caching-fundamentals.md) - Cache hit/miss dynamics
- [03-redis.md](./03-redis.md) - Redis pipelines and data structures
- [05-advanced/03-events-and-messaging/01-event-emitter.md](../03-events-and-messaging/01-event-emitter.md) - In-process event bus
