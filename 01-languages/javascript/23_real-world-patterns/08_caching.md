# Caching Strategies & Stale-While-Revalidate in JavaScript

> A production-grade implementation and architectural guide to caching strategies in JavaScript, featuring Cache-Aside, Stale-While-Revalidate (SWR), multi-tiered memory/distributed hierarchies, and cache stampede defenses.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced / Production-Grade |
| **Prerequisites** | [05_memoization.md](file:///e:/Learning/engineering-notes/01-languages/javascript/23_real-world-patterns/05_memoization.md), [15_networking/01_fetch-api.md](file:///e:/Learning/engineering-notes/01-languages/javascript/15_networking/01_fetch-api.md) |
| **Standard Baseline** | ECMAScript 2024 / Node.js 20+ LTS / Browser Evergreen |
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

In distributed web applications, data retrieval is often the primary bottleneck for latency and throughput. Reading data from relational databases, running complex aggregations, or querying third-party APIs incurs network Round Trip Times (RTT) and heavy CPU overhead.

A **Cache** is a high-speed data storage layer that holds a subset of data so that future requests can be served significantly faster than querying the primary storage tier.
- **Latency Reduction**: Serves data in sub-millisecond timeframes.
- **Database Offloading**: Absorbs repetitive read volume, shielding databases from saturation.
- **Resilience**: Shields client applications from transient downstream failures by serving cached fallbacks.

---

## 2. Core Concepts

### Architectural Caching Patterns
1. **Cache-Aside (Lazy Loading)**: The application code directly queries the cache. On a cache miss, it reads from the primary database, populates the cache with the fetched record, and returns the result.
2. **Read-Through / Write-Through**: The application treats the cache as the authoritative data store. The cache layer automatically reads from or writes to the underlying database synchronously.
3. **Write-Behind (Write-Back)**: The application writes to the cache immediately; the cache asynchronously flushes changes to the primary database in queued batches.
4. **Stale-While-Revalidate (SWR)**: Returns cached data immediately (even if slightly stale), while triggering an asynchronous background fetch to refresh the cache with the latest authoritative version.

```
[ Client Request ]
        │
        ▼
[ Cache Check ]
        │
        ├── HIT (Fresh) ─────────────────────────────► [ Return Data (<1ms) ]
        │
        ├── HIT (Stale, within SWR window) ──────────► [ Return Stale Data Immediately ]
        │                                                     │
        │                                                     ▼
        │                                           (Background Revalidation Async)
        │
        └── MISS ─────────────────────────────────────► [ Read Primary DB ]
                                                              │
                                                              ▼
                                                        [ Populate Cache ]
                                                              │
                                                              ▼
                                                        [ Return Fresh Data ]
```

### The Cache Stampede (Thundering Herd) Hazard
When a heavily accessed key expires (or on cold start), hundreds of concurrent client requests observe a cache miss at the same millisecond. All requests simultaneously query the underlying database, overwhelming it and causing service degradation. This is mitigated using **SingleFlight Promise Deduplication** or **Mutex Locks**.

---

## 3. How It Works Internally

The Stale-While-Revalidate pattern maintains three distinct lifecycle thresholds:
1. **Fresh Window ($0 \to T_{\text{fresh}}$)**: Served directly from cache; no network call is initiated.
2. **Stale Window ($T_{\text{fresh}} \to T_{\text{expire}}$)**: Served immediately from cache to ensure zero UI delay, while simultaneously firing a background fetch to revalidate the cache.
3. **Expired Window ($> T_{\text{expire}}$)**: Cache miss; execution must wait synchronously for fresh data.

```
0s                  10s (fresh)                 60s (stale window)            >60s (expired)
├────────────────────┼───────────────────────────┼──────────────────────────────►
│   FRESH HIT        │   SERVE STALE + REVALIDATE│          CACHE MISS          │
│   (Zero Network)   │   (Instant + Async Fetch) │          (Sync Network Fetch)│
```

---

## 4. Syntax & API Reference

### Production Stale-While-Revalidate Cache Manager
```javascript
/**
 * Production-grade in-memory cache with Stale-While-Revalidate (SWR)
 * and Cache Stampede protection (SingleFlight lock).
 */
export class SWRCache {
  constructor(options = {}) {
    this.cache = new Map();
    this.inFlight = new Map(); // SingleFlight lock map
    this.maxSize = options.maxSize ?? 500;
  }

  async getOrFetch(key, fetcher, { freshMs = 5000, staleMs = 60000 } = {}) {
    const now = Date.now();
    const entry = this.cache.get(key);

    // 1. Fresh Cache Hit: Return immediately without revalidation
    if (entry && now - entry.cachedAt < freshMs) {
      return entry.data;
    }

    // 2. Stale Cache Hit: Return cached data immediately, revalidate in background
    if (entry && now - entry.cachedAt < staleMs) {
      this.revalidateInBackground(key, fetcher);
      return entry.data;
    }

    // 3. Cache Miss or Expired: Fetch fresh data synchronously with Stampede Lock
    return this.fetchWithStampedeLock(key, fetcher);
  }

  fetchWithStampedeLock(key, fetcher) {
    if (this.inFlight.has(key)) {
      // Existing in-flight request: join pending promise
      return this.inFlight.get(key);
    }

    const promise = (async () => {
      try {
        const data = await fetcher();
        this.set(key, data);
        return data;
      } finally {
        this.inFlight.delete(key);
      }
    })();

    this.inFlight.set(key, promise);
    return promise;
  }

  revalidateInBackground(key, fetcher) {
    if (this.inFlight.has(key)) return; // Revalidation already in progress

    const promise = (async () => {
      try {
        const data = await fetcher();
        this.set(key, data);
      } catch (err) {
        console.warn(`[SWRCache] Background revalidation failed for '${key}':`, err.message);
      } finally {
        this.inFlight.delete(key);
      }
    })();

    this.inFlight.set(key, promise);
  }

  set(key, data) {
    if (this.cache.size >= this.maxSize) {
      const oldestKey = this.cache.keys().next().value;
      this.cache.delete(oldestKey);
    }
    this.cache.set(key, { data, cachedAt: Date.now() });
  }

  invalidate(key) {
    this.cache.delete(key);
    this.inFlight.delete(key);
  }

  clear() {
    this.cache.clear();
    this.inFlight.clear();
  }
}
```

---

## 5. Practical Examples

### Example 1: Multi-Tier Cache (L1 Memory + L2 Distributed Redis)
```javascript
export class MultiTierCache {
  constructor(l1MemoryCache, l2RedisClient) {
    this.l1 = l1MemoryCache; // Fast process-local Map
    this.l2 = l2RedisClient; // Shared network Redis instance
  }

  async get(key, fetcher, ttlSeconds = 300) {
    // 1. Check L1 Memory Cache (sub-microsecond)
    const l1Value = this.l1.get(key);
    if (l1Value !== undefined) {
      return l1Value;
    }

    // 2. Check L2 Redis Cache (~1-2ms)
    const l2Raw = await this.l2.get(key);
    if (l2Raw) {
      const data = JSON.parse(l2Raw);
      this.l1.set(key, data); // Populate L1
      return data;
    }

    // 3. Cache Miss: Query Database
    const freshData = await fetcher();

    // Populate both cache tiers
    this.l1.set(key, freshData);
    await this.l2.set(key, JSON.stringify(freshData), 'EX', ttlSeconds);

    return freshData;
  }
}
```

### Example 2: Client-Side SWR Hook for Web Applications
```javascript
import { SWRCache } from './SWRCache.js';

const globalSWR = new SWRCache();

export function useSWR(url, onDataUpdate) {
  let isMounted = true;

  async function load() {
    const data = await globalSWR.getOrFetch(
      url,
      async () => {
        const res = await fetch(url);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return res.json();
      },
      { freshMs: 10000, staleMs: 120000 }
    );

    if (isMounted) {
      onDataUpdate(data);
    }
  }

  load();

  return () => {
    isMounted = false;
  };
}
```

### Example 3: Cache Penetration Defense via Null Object Caching
```javascript
export class NullDefendedCache {
  constructor(cacheClient) {
    this.cache = cacheClient;
    this.NULL_SENTINEL = '__NULL_RECORD__';
  }

  async findUserById(userId, dbQueryFn) {
    const key = `user:${userId}`;
    const cached = await this.cache.get(key);

    if (cached === this.NULL_SENTINEL) {
      return null; // Record proven non-existent in database
    }
    if (cached) {
      return JSON.parse(cached);
    }

    const user = await dbQueryFn(userId);

    if (!user) {
      // Cache non-existent record with short TTL (60s) to prevent DB hammering
      await this.cache.set(key, this.NULL_SENTINEL, 'EX', 60);
      return null;
    }

    await this.cache.set(key, JSON.stringify(user), 'EX', 3600);
    return user;
  }
}
```

### Example 4: ETag & Conditional Revalidation HTTP Middleware
```javascript
import crypto from 'node:crypto';

export function etagCacheMiddleware(req, res, next) {
  const originalJson = res.json.bind(res);

  res.json = (body) => {
    const payload = JSON.stringify(body);
    const hash = crypto.createHash('md5').update(payload).digest('hex');
    const etag = `"${hash}"`;

    res.setHeader('ETag', etag);
    res.setHeader('Cache-Control', 'public, max-age=60, stale-while-revalidate=300');

    // Check If-None-Match header
    if (req.headers['if-none-match'] === etag) {
      return res.status(304).end(); // Not Modified (0 bytes transferred)
    }

    return originalJson(body);
  };

  next();
}
```

### Example 5: Write-Through Cache with Transactional Integrity
```javascript
export class WriteThroughRepository {
  constructor(db, redis) {
    this.db = db;
    this.redis = redis;
  }

  async updateAccountBalance(accountId, newBalance) {
    // 1. Authoritative write to SQL Database
    await this.db.query('UPDATE accounts SET balance = $1 WHERE id = $2', [newBalance, accountId]);

    // 2. Synchronous update to cache tier
    const key = `account:${accountId}:balance`;
    await this.redis.set(key, String(newBalance), 'EX', 1800);

    return newBalance;
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Cache Stampede (Dogpiling)**:
   When an item accessed by thousands of concurrent requests expires from the cache, all requests miss simultaneously and flood the database. Mitigate by using in-flight SingleFlight promise locks or probabilistic early expiration (XFetch algorithm).
2. **Cache Penetration**:
   Attackers querying random, non-existent entity IDs (`/users/9999999999`) bypass the cache completely because the keys never exist in the cache, repeatedly hitting the primary database. Mitigate by caching `null` values with a short TTL or using a Bloom filter.
3. **Cache Breakdown**:
   Occurs when a single hot spot key expires and the sudden surge of traffic overwhelms the database. Use mutex locks so only one thread revalidates the hot key while others wait or receive stale data.
4. **V8 Heap Contention from In-Memory Caching**:
   Storing hundreds of thousands of large JavaScript objects in process-level memory causes V8 Garbage Collection pauses (mark-compact) to increase dramatically. For datasets exceeding 100MB, store data in Redis or use off-heap Node.js `Buffer` arrays.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Risk | Production Fix |
| :--- | :--- | :--- |
| Unbounded in-memory cache | Process crashes with Out Of Memory (OOM). | Enforce LRU eviction with a strict `maxSize` capacity limit. |
| Missing stampede locks | Cold-start traffic crushes database on cache expiration. | Use SingleFlight promise deduplication on cache misses. |
| Inadequate cache invalidation on writes | Stale data served indefinitely to users after mutations. | Implement cache eviction/invalidation on write/update paths. |
| Storing mutable objects in cache | Mutating cached object modifies data for all subsequent callers. | Freeze objects (`Object.freeze`) or clone them upon return. |
| Ignoring cache penetration on non-existent keys | Heavy query traffic degrades DB when querying fake IDs. | Cache null sentinels with short TTL or apply Bloom filters. |
| Identical TTLs across all cached items | Simultaneous expiration creates cyclical thundering herds. | Add random jitter to cache TTLs ($\text{TTL} \pm 10\%$). |

---

## 8. Performance Characteristics

- **Latency Comparisons**:
  - L1 In-Memory `Map` lookup: $\approx 15\text{ns}$ to $40\text{ns}$.
  - L2 Local Redis cache lookup: $\approx 0.5\text{ms}$ to $1.5\text{ms}$.
  - Primary Database SQL Query: $\approx 10\text{ms}$ to $150\text{ms}$.
- **Throughput Multiplier**: Adding a cache-aside layer with a 95% hit ratio typically increases system read throughput by 10x to 20x while decreasing database CPU load by 90%.

---

## 9. Engine Internals

### V8 Memory Architecture & JSON Stringification
- **In-Memory Object Overhead**: In V8, each small JavaScript object carries an internal Map pointer (Hidden Class) and prototype references ($\approx 40$ bytes base overhead per object). Storing 1,000,000 small objects in memory consumes over 60MB just in structural pointers.
- **Off-Heap Buffers**: Storing serialized data in Node.js `Buffer` instances places bytes outside the V8 V8 garbage-collected heap. This allows storing hundreds of megabytes of cached data without impacting V8 GC pause times.

---

## 10. Best Practices & Design Guidelines

1. **Add TTL Jitter**: Always inject $\pm 10\%$ jitter into cache expiration times to prevent simultaneous key expiration.
2. **Defend with SingleFlight**: Guarantee that only one in-flight fetcher executes for a given key at any moment.
3. **Prefer Stale-While-Revalidate for Reads**: Delivering stale data in $1\text{ms}$ while refreshing in the background provides vastly superior user experience compared to blocking for fresh queries.
4. **Log Cache Hit Ratios**: Continuously monitor and alert on cache hit ratios ($\text{Hits} / (\text{Hits} + \text{Misses})$) in production dashboards.

---

## 11. Real-World Case Study

### High-Volume Digital Media Publisher
**Scenario**: A major news website experiences traffic surges of 250,000 requests/minute during breaking news events. The homepage renders articles retrieved from a PostgreSQL database.
**Problem**: During a breaking news event, the article cache expired simultaneously for 50,000 concurrent visitors. All requests missed and hit PostgreSQL, driving database CPU to 100% and taking the site offline.

**Solution Architecture**:
- Replaced basic TTL caching with a **Stale-While-Revalidate (SWR)** architecture:
  - `freshMs: 15,000` (15s fresh).
  - `staleMs: 300,000` (5 minutes stale allowance).
  - SingleFlight promise deduplication on revalidation.
- When the 15-second fresh window expired, the very first visitor received the cached homepage instantly ($2\text{ms}$) while triggering a single background revalidation query to PostgreSQL.
- The other 49,999 visitors during that window received the stale version in $<2\text{ms}$ without a single database query.

**Outcome**:
- Database CPU dropped from 100% to 4%.
- P99 latency dropped from $3,400\text{ms}$ to $8\text{ms}$.
- Cache hit ratio achieved 99.98% during breaking news traffic spikes.

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between Cache-Aside and Stale-While-Revalidate?</summary>

In Cache-Aside, on a cache miss the caller synchronously waits for the database query to complete before receiving a response and populating the cache. In Stale-While-Revalidate (SWR), if the cached data is past its "fresh" threshold but within its "stale" threshold, the cache returns the stale data immediately (zero wait time for the user) while asynchronously initiating a background network fetch to refresh the cache for future callers.
</details>

<details>
<summary>2. What is a Cache Stampede (or Dogpiling), and how is it prevented?</summary>

A cache stampede occurs when a high-traffic cache key expires, causing hundreds or thousands of concurrent requests to experience a cache miss simultaneously. All these requests query the underlying database at the exact same moment, causing database saturation and failure. It is prevented using **SingleFlight promise deduplication** (locking), probabilistic early expiration (XFetch algorithm), or serving stale data while a single worker revalidates in the background.
</details>

<details>
<summary>3. What is Cache Penetration, and how do you protect against it?</summary>

Cache penetration occurs when requests query keys that do not exist in either the cache or the database (e.g., automated vulnerability scanners querying random non-existent IDs). Because the keys never exist, every request bypasses the cache and hits the database. Mitigations include:
1. **Caching Null Sentinels**: Store a null indicator in the cache with a short TTL (e.g. 60 seconds).
2. **Bloom Filters**: Test for key existence with an in-memory Bloom filter before querying the cache or database.
</details>

<details>
<summary>4. Why should you add randomized jitter to cache TTLs?</summary>

If an application pre-populates or updates 10,000 records simultaneously with a fixed TTL of 1 hour, all 10,000 records will expire at the exact same second one hour later. This creates massive periodic spikes in database load. Adding random jitter (e.g. $\text{TTL} = 3600 \pm \text{rand}(0, 300)$ seconds) spreads out the expiration timestamps evenly across time.
</details>

<details>
<summary>5. How does HTTP ETag caching work with 304 Not Modified?</summary>

The server generates a content hash (ETag) for the response and returns it in the `ETag` header. On subsequent requests, the client sends this hash in the `If-None-Match` header. The server computes the current resource hash; if it matches the client's `If-None-Match` value, the server returns an empty `HTTP 304 Not Modified` status code, saving bandwidth and serialization time.
</details>

<details>
<summary>6. What are the trade-offs of an in-memory process cache vs. a distributed cache like Redis?</summary>

- **In-Memory (Process Local)**: Ultra-fast ($<1\mu\text{s}$), zero network hops, but consumes local Node.js process heap memory, is not shared across multi-core cluster processes, and is lost on process restart.
- **Distributed (Redis)**: Shared across all server instances and processes, survives app restarts, supports massive datasets, but incurs network latency ($\approx 1\text{ms}$ RTT) and requires managing serialization/deserialization.
</details>

<details>
<summary>7. What is Write-Behind (Write-Back) caching, and what is its primary risk?</summary>

In Write-Behind caching, write operations update only the cache immediately, returning success to the client instantly. The cache then writes the mutations to the primary database asynchronously in background batches. While this achieves ultra-high write throughput, its primary risk is **data loss**: if the cache crashes or loses power before background writes flush to persistent disk, unwritten data is permanently lost.
</details>

<details>
<summary>8. How does object mutability in JavaScript threaten in-memory caches?</summary>

If an in-memory cache returns a direct reference to a cached JavaScript object, any caller that modifies a property on that returned object mutates the cached record in place. All other parts of the application reading from the cache will immediately receive the mutated, corrupted state. To prevent this, in-memory caches must either `Object.freeze()` cached entities or return deep copies (`structuredClone(cached)`).
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement a Basic TTL-Based Cache
**Specification**: Implement a `TTLCache` class supporting `set(key, val, ttlMs)` and `get(key)` that evicts expired entries on read.

<details>
<summary>Solution</summary>

```javascript
export class TTLCache {
  constructor() {
    this.store = new Map();
  }

  set(key, value, ttlMs) {
    const expiresAt = Date.now() + ttlMs;
    this.store.set(key, { value, expiresAt });
  }

  get(key) {
    const entry = this.store.get(key);
    if (!entry) return undefined;

    if (Date.now() > entry.expiresAt) {
      this.store.delete(key);
      return undefined;
    }

    return entry.value;
  }

  has(key) {
    return this.get(key) !== undefined;
  }
}
```
</details>

### Exercise 2: Build a SingleFlight Promise Deduplicator
**Specification**: Write `singleFlight(fetcher)` that ensures concurrent calls with the same key share a single active Promise.

<details>
<summary>Solution</summary>

```javascript
export function createSingleFlight() {
  const inFlight = new Map();

  return async function execute(key, task) {
    if (inFlight.has(key)) {
      return inFlight.get(key);
    }

    const promise = (async () => {
      try {
        return await task();
      } finally {
        inFlight.delete(key);
      }
    })();

    inFlight.set(key, promise);
    return promise;
  };
}
```
</details>
