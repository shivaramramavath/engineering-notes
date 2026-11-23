# Redis Client Typing and Type-Safe Caching Patterns in TypeScript

> Typing Redis in TypeScript: ioredis vs node-redis, JSON serialization/deserialization, TTL helpers, tagged cache invalidation, and distributed locks with Redlock.

| Metadata | Details |
| :--- | :--- |
| **Category** | Node.js Backend Typing |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [12-async-and-iteration/01-promises.md](../12-async-and-iteration/01-promises.md), [15-runtime-validation/01-schema-validation.md](../15-runtime-validation/01-schema-validation.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Redis is an in-memory key-value data structure store used for caching, session management, rate limiting, and pub/sub messaging.

By default, Redis drivers (`ioredis` or `node-redis`) store and return raw strings or buffers:
```typescript
const raw = await redis.get('user:123'); // raw is string | null
```
When caching complex domain models or DTOs:
1. **JSON Deserialization**: Deserializing `JSON.parse(raw)` produces `any`. A typo like `user.emial` goes undetected during compilation.
2. **Schema Drift in Cache**: When your database model changes, stale cached objects in Redis still reflect the old shape, causing runtime crashes when read.
3. **Cache Invalidation Chaos**: Without structured cache key factories, invalidating related cache entries (e.g. invalidating all user sessions when a password changes) relies on brittle wildcards (`KEYS user:*`), which blocks the single-threaded Redis engine.

A type-safe caching layer encapsulates Redis operations with **generics**, **runtime schema validation**, and **key factories**.

```
+-----------------------------------------------------------+
|               Type-Safe Redis Caching Pipeline            |
|                                                           |
|  cache.getOrSet(key, fetcher, schema, ttlSeconds)         |
|                       |                                   |
|         +-------------+-------------+                     |
|         |                           |                     |
|    (Cache Hit)                 (Cache Miss)               |
|         v                           v                     |
|  redis.get(key)              fetcher() -> DB Query        |
|         v                           v                     |
|  schema.parse(JSON)          redis.set(key, JSON, 'EX')   |
|         v                           v                     |
|  Return Strongly Typed T     Return Strongly Typed T      |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. `ioredis` vs `node-redis` Typing
- **`ioredis`**: Industry favorite for robust cluster support, sentinel failover, and predictable promise-based APIs.
- **`node-redis`**: Official client with native support for Redis Stack, RedisJSON, and search modules.

### 2. Type-Safe Cache Key Factories
Keys should be structured as branded tuples to prevent string typo errors:
```typescript
export const cacheKeys = {
  user: (id: string) => `users:${id}` as const,
  session: (token: string) => `sessions:${token}` as const,
  rateLimit: (ip: string) => `ratelimit:${ip}` as const
};
```

### 3. Generic Get-Or-Set Pattern (Cache-Aside)
Encapsulating cache lookup, miss handling, and storage into a single typed utility:
```typescript
export async function getOrSet<T>(
  redis: Redis,
  key: string,
  ttlSeconds: number,
  fetcher: () => Promise<T>,
  schema?: z.ZodType<T>
): Promise<T> { ... }
```

---

## 3. How It Works Internally

1. `redis.get(key)` executes the RESP (REdis Serialization Protocol) command over a TCP socket.
2. If the key exists, the JSON string is parsed.
3. If a validation schema (Zod) is provided, it validates the cached payload. If the cached payload fails validation (due to schema drift), the cache entry is treated as a miss, evicted, and re-fetched from the database.

---

## 4. Syntax & API Reference

### Type-Safe Cache Manager Class Interface
```typescript
export interface TypedCacheManager {
  get<T>(key: string, schema?: z.ZodType<T>): Promise<T | null>;
  set<T>(key: string, value: T, ttlSeconds?: number): Promise<void>;
  del(key: string): Promise<boolean>;
}
```

---

## 5. Practical Examples

### Example 1: Basic Type-Safe Redis Wrapper
```typescript
import Redis from 'ioredis';

const redis = new Redis(process.env.REDIS_URL || 'redis://localhost:6379');

export async function setJson<T>(key: string, data: T, ttlSeconds: number): Promise<void> {
  const serialized = JSON.stringify(data);
  await redis.set(key, serialized, 'EX', ttlSeconds);
}

export async function getJson<T>(key: string): Promise<T | null> {
  const raw = await redis.get(key);
  if (!raw) return null;
  return JSON.parse(raw) as T;
}
```

### Example 2: Real-World Safe Cache-Aside with Schema Verification
```typescript
import Redis from 'ioredis';
import { z } from 'zod';

export const UserCacheSchema = z.object({
  id: string(),
  email: z.string().email(),
  role: z.enum(['admin', 'user'])
});

export type CachedUser = z.infer<typeof UserCacheSchema>;

export async function getCachedUser(
  redis: Redis,
  userId: string,
  dbFallback: (id: string) => Promise<CachedUser>
): Promise<CachedUser> {
  const key = `user:${userId}`;
  const cached = await redis.get(key);

  if (cached) {
    const parsed = UserCacheSchema.safeParse(JSON.parse(cached));
    if (parsed.success) {
      return parsed.data;
    }
    // Stale schema drift! Evict and re-fetch:
    await redis.del(key);
  }

  const fresh = await dbFallback(userId);
  await redis.set(key, JSON.stringify(fresh), 'EX', 3600);
  return fresh;
}
```

### Example 3: Edge Case - Distributed Locking with Expiration (Redlock Pattern)
```typescript
export async function withLock<T>(
  redis: Redis,
  resourceKey: string,
  ttlMs: number,
  task: () => Promise<T>
): Promise<T> {
  const lockKey = `locks:${resourceKey}`;
  const lockToken = crypto.randomUUID();

  // SET lockKey token NX PX ttlMs (Atomic lock acquisition)
  const acquired = await redis.set(lockKey, lockToken, 'PX', ttlMs, 'NX');
  if (!acquired) {
    throw new Error(`Failed to acquire lock for ${resourceKey}`);
  }

  try {
    return await task();
  } finally {
    // Atomic release using Lua script to verify token ownership:
    const releaseScript = `
      if redis.call("get", KEYS[1]) == ARGV[1] then
        return redis.call("del", KEYS[1])
      else
        return 0
      end
    `;
    await redis.eval(releaseScript, 1, lockKey, lockToken);
  }
}
```

### Example 4: Failure Case - Type Discrepancy on Stored Generic
```typescript
interface ProductDTO { sku: string; priceCents: number; }

declare function getJsonTyped<T>(key: string): Promise<T | null>;

async function test() {
  const p = await getJsonTyped<ProductDTO>('prod:1');
  // @ts-expect-error TS2339: Property 'title' does not exist on type 'ProductDTO'.
  console.log(p?.title);
}
```

### Example 5: Idiomatic Tagged Invalidation via Sets
```typescript
export class TaggedCache {
  constructor(private redis: Redis) {}

  public async setWithTags<T>(key: string, data: T, tags: string[], ttlSeconds = 3600) {
    await this.redis.set(key, JSON.stringify(data), 'EX', ttlSeconds);
    for (const tag of tags) {
      await this.redis.sadd(`tag:${tag}`, key);
    }
  }

  public async invalidateTag(tag: string) {
    const tagKey = `tag:${tag}`;
    const keys = await this.redis.smembers(tagKey);
    if (keys.length > 0) {
      await this.redis.del(...keys);
      await this.redis.del(tagKey);
    }
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Blocking the Redis Event Loop with `KEYS *`**:
   Never execute `redis.keys('pattern*')` in production! Redis is single-threaded; `KEYS` inspects every key in memory synchronously, freezing all database traffic for seconds. Use `SCAN` or Tagged Sets instead.
2. **Cache Stampede (Thundering Herd)**:
   When a popular cache key expires, thousands of concurrent requests miss simultaneously, hammering the underlying PostgreSQL database. Mitigate with distributed locks or probabilistic early expiration (XFetch).
3. **Serialization of `Date` and `bigint`**:
   `JSON.stringify` drops `Date` prototypes (turning them into strings) and crashes on `bigint`. Ensure you parse date strings back to `Date` objects if consumer code expects dates.
4. **Memory Leaks in Redis**:
   Always set a TTL (`EX` or `PX`) on cached keys. Keys without TTLs remain in Redis RAM permanently until evicted by the maxmemory policy.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Using `KEYS *` in Production** | `redis.keys('users:*')` | Tagged Sets or `SCAN` cursor | Freezes Redis process for all users |
| **Untyped JSON Parse** | `const user = JSON.parse(await redis.get(k))` | Parse with Zod schema | Returns `any`; runtime crashes on schema drift |
| **Missing Expiration TTL** | `redis.set(key, val)` without 'EX' | `redis.set(key, val, 'EX', 3600)` | Redis out of memory (OOM) |
| **Unsafe Distributed Lock Release** | `await redis.del(lockKey)` | Lua script checking lock token | Releases someone else's expired lock |
| **Hardcoding Raw String Keys** | `redis.get('user_' + id)` | Centralized `cacheKeys.user(id)` | Inconsistent key naming across services |

---

## 8. Performance & Type-Checker Complexity

- **Throughput**: A single Redis instance processes 100,000+ operations/sec. JSON serialization and Zod validation take ~2-5 microseconds in Node.js.
- **Type Checking**: Key factory templates and generic wrappers check in sub-milliseconds.

---

## 9. Compiler Internals

When using template literal types for keys:
```typescript
type CacheKey = `user:${string}` | `session:${string}`;
```
The compiler verifies that string arguments match the prefix patterns, rejecting arbitrary strings.

---

## 10. Best Practices & Design Guidelines

1. **Always Set a TTL on Cache Keys**: Prevent memory bloat.
2. **Validate Cached Data on Read**: Detect schema drift automatically.
3. **Use Tagged Invalidation Instead of Wildcard Scans**: Protect Redis single-threaded performance.
4. **Use Atomic Lua Scripts for Lock Releases**: Prevent race conditions.

---

## 11. Real-World Case Study

### Distributed API Rate Limiter
Sliding window rate limiter using Redis sorted sets (ZSET):

```typescript
import Redis from 'ioredis';

export class SlidingWindowRateLimiter {
  constructor(private redis: Redis) {}

  public async isAllowed(identifier: string, limit: number, windowSeconds: number): Promise<boolean> {
    const key = `ratelimit:${identifier}`;
    const now = Date.now();
    const clearBefore = now - windowSeconds * 1000;

    const pipeline = this.redis.pipeline();
    pipeline.zremrangebyscore(key, 0, clearBefore);
    pipeline.zadd(key, now, `${now}-${Math.random()}`);
    pipeline.zcard(key);
    pipeline.expire(key, windowSeconds);

    const results = await pipeline.exec();
    const count = results?.[2]?.[1] as number;

    return count <= limit;
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the Cache-Aside pattern and how is it implemented in TypeScript?</summary>

In Cache-Aside, the application first inspects the cache. On a hit, it returns the cached data. On a miss, it queries the primary database, populates the cache with a TTL, and returns the result. A generic function `getOrSet<T>(key, fetcher, ttl)` encapsulates this workflow with strong typing.
</details>

<details>
<summary>2. Why is `redis.keys('*')` dangerous in production systems?</summary>

Redis is single-threaded. The `KEYS` command performs a blocking $O(N)$ scan across all keys in the database. In a database with millions of keys, this blocks the entire Redis engine for seconds, causing timeouts and cascading failures across all backend microservices.
</details>

<details>
<summary>3. What is the Cache Stampede (Thundering Herd) problem and how do you prevent it?</summary>

Cache Stampede occurs when a high-traffic cache key expires. Multiple concurrent requests miss simultaneously and all attempt to query the database and recompute the cache, causing a sudden spike in database CPU load. Solutions include distributed locks (Redlock) or probabilistic early expiration (XFetch algorithm).
</details>

<details>
<summary>4. Why should distributed lock releases always use a Lua script?</summary>

If a process acquires a lock with a 5-second TTL, but its operation takes 6 seconds, the lock expires automatically. Another process acquires the lock. When the first process finishes and blindly calls `DEL lockKey`, it deletes the *second* process's lock! A Lua script checks whether the token in Redis matches the process's token before deleting, ensuring atomic, safe releases.
</details>

<details>
<summary>5. How do you handle schema drift in cached Redis JSON objects?</summary>

Parse cached JSON through a runtime schema parser (e.g. Zod). If the cached data fails schema validation (e.g. missing a newly required field), catch the error, evict the stale cache key, and re-fetch fresh data from the database.
</details>

<details>
<summary>6. How does Redis pipeline improve network performance?</summary>

Instead of sending commands one-by-one and waiting for a round-trip network response for each command ($N \times \text{RTT}$), pipelining packages multiple commands into a single TCP packet, executing them all in a single round-trip.
</details>

<details>
<summary>7. What are Tagged Sets for cache invalidation?</summary>

Rather than scanning for keys using patterns, whenever a key is written, its key name is added to a Redis Set representing a tag (e.g. `tag:user:123`). When invalidating that tag, the application reads the set members and deletes them directly.
</details>

<details>
<summary>8. How do you type Redis Pub/Sub channels in TypeScript?</summary>

By defining a mapped channel interface where keys are channel names and values are payload shapes, wrapping `publish` and `subscribe` in typed methods.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Redis Key Formatter</summary>

**Problem:** Write a function `makeSessionKey(userId: string): string` using template literals.

**Solution:**
```typescript
export function makeSessionKey(userId: string): `session:${string}` {
  return `session:${userId}`;
}
```
</details>

<details>
<summary>Exercise 2 (Easy): TTL Expiration Wrapper</summary>

**Problem:** Write a function `cacheString(redis: any, key: string, value: string, ttlSeconds: number)` that sets a string with expiration.

**Solution:**
```typescript
export async function cacheString(redis: any, key: string, value: string, ttlSeconds: number): Promise<void> {
  await redis.set(key, value, 'EX', ttlSeconds);
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Generic Get-Or-Set Function</summary>

**Problem:** Implement `getOrSet<T>(redis, key, ttl, fetcher)` that caches and retrieves JSON objects.

**Solution:**
```typescript
export async function getOrSet<T>(
  redis: any,
  key: string,
  ttl: number,
  fetcher: () => Promise<T>
): Promise<T> {
  const cached = await redis.get(key);
  if (cached) return JSON.parse(cached) as T;
  const fresh = await fetcher();
  await redis.set(key, JSON.stringify(fresh), 'EX', ttl);
  return fresh;
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Redis Counter with Expiration</summary>

**Problem:** Implement an atomic increment function `incrementCounter(redis, key, ttlSeconds): Promise<number>`.

**Solution:**
```typescript
export async function incrementCounter(redis: any, key: string, ttlSeconds: number): Promise<number> {
  const count = await redis.incr(key);
  if (count === 1) {
    await redis.expire(key, ttlSeconds);
  }
  return count;
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe Redis PubSub Channel Manager</summary>

**Problem:** Build a typed Pub/Sub wrapper where publishing and subscribing to channels enforces matching payload types.

**Solution:**
```typescript
export interface ChannelMap {
  'user:signup': { userId: string; email: string };
  'order:paid': { orderId: string; totalCents: number };
}

export class TypedPubSub {
  constructor(private publisher: any, private subscriber: any) {}

  public async publish<C extends keyof ChannelMap>(channel: C, payload: ChannelMap[C]): Promise<void> {
    await this.publisher.publish(channel, JSON.stringify(payload));
  }

  public subscribe<C extends keyof ChannelMap>(channel: C, handler: (payload: ChannelMap[C]) => void): void {
    this.subscriber.subscribe(channel);
    this.subscriber.on('message', (ch: string, message: string) => {
      if (ch === channel) handler(JSON.parse(message));
    });
  }
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Cache Invalidator with Tag Sets</summary>

**Problem:** Implement a tag-based cache store where `invalidate(tag)` reads members from a Redis set and deletes all associated keys.

**Solution:**
```typescript
export class TaggedStore {
  constructor(private redis: any) {}

  public async set(key: string, data: any, tags: string[], ttl = 3600) {
    await this.redis.set(key, JSON.stringify(data), 'EX', ttl);
    for (const tag of tags) {
      await this.redis.sadd(`tag:${tag}`, key);
    }
  }

  public async invalidate(tag: string) {
    const setKey = `tag:${tag}`;
    const keys = await this.redis.smembers(setKey);
    if (keys.length > 0) {
      await this.redis.del(...keys);
      await this.redis.del(setKey);
    }
  }
}
```
</details>
