# Enterprise Caching Architecture and Patterns in Java

> High-performance caching strategies, eviction algorithms, cache synchronization, stampede mitigation, and multi-tier L1/L2 architectures in enterprise Java systems.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [`08-collections/01_maps.md`](../08-collections/01_maps.md), [`14-concurrency/04_locks-and-synchronizers.md`](../14-concurrency/04_locks-and-synchronizers.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified (Java 8 to 25 LTS) |
| **Standard** | Java SE 21 / 25 LTS |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In distributed enterprise architectures, disk and network I/O represent the primary latency bottlenecks. A relational database query typically incurs 5–50 ms latency, whereas reading from in-process L1 heap memory requires < 100 ns, and fetching from a distributed Redis cache requires ~1 ms.

```
Access Latency Hierarchy:
CPU L1/L2/L3 Cache:    0.5 ns - 15 ns
JVM Heap Memory:       50 ns - 150 ns  (1x baseline)
Local NVMe SSD Read:   10 us - 50 us   (300x slower)
Local Redis (TCP):     500 us - 1 ms   (6,000x slower)
Distributed DB Query:  5 ms - 50 ms    (100,000x slower)
```

Caching stores frequently accessed, computationally expensive data in fast transient storage to minimize response times, preserve database throughput, and decouple downstream systems from spiky read traffic.

---

## 2. Core Concepts

### 2.1 Caching Topologies & Access Patterns

1. **Cache-Aside (Lazy Loading)**:
   - Application queries cache first.
   - On cache miss, application queries database, inserts result into cache, and returns value.
   - Resilience: Cache outage does not halt application (falls back to DB).

2. **Read-Through**:
   - Application treats cache as the primary data store.
   - Cache provider delegates to an underlying `CacheLoader` on miss.

3. **Write-Through**:
   - Application writes directly to cache; cache writes synchronously to database before confirming success.
   - Guarantees high consistency at the expense of write latency.

4. **Write-Behind (Write-Back)**:
   - Application writes to cache; cache asynchronously batches writes to database.
   - Extreme write throughput, but risks data loss during unexpected crashes.

5. **Refresh-Ahead**:
   - Cache automatically reloads hot items prior to expiration based on access frequency.

### 2.2 Eviction Algorithms

| Algorithm | Mechanism | Trade-Off |
| :--- | :--- | :--- |
| **LRU (Least Recently Used)** | Evicts entries that haven't been requested for the longest time | Vulnerable to sequential scan pollution |
| **LFU (Least Frequently Used)** | Evicts entries with lowest access count | Historical frequency bias; slow to adapt to workload shifts |
| **FIFO (First In First Out)** | Evicts oldest entries regardless of usage | Poor hit ratio for hot recurring keys |
| **W-TinyLFU (Caffeine)** | Uses 4-bit Count-Min Sketch filter with window cache | Industry-standard hit ratio; immune to scan pollution |

---

## 3. How It Works Internally

### 3.1 Multi-Tier Caching Architecture (L1 Local + L2 Distributed)

In modern enterprise microservices, an optimal pattern combines in-memory local caching (L1 - Caffeine) with distributed caching (L2 - Redis) backed by pub/sub cache invalidation:

```
+-------------------------------------------------------------------------------+
|                            MULTI-TIER CACHE ARCHITECTURE                      |
|                                                                               |
|   Client Request                                                              |
|        |                                                                      |
|        v                                                                      |
|   +----+------------------+                                                   |
|   | Microservice Node A   |                                                   |
|   |  +------------------+ |                                                   |
|   |  | L1 Cache (Caffeine)| |  L1 Hit (< 100 ns) -> Return Data               |
|   |  +--------+---------+ |                                                   |
|   +-----------|-----------+                                                   |
|               | L1 Miss                                                       |
|               v                                                               |
|   +-----------+-----------+                                                   |
|   | L2 Cache (Redis)      |   L2 Hit (~1 ms) -> Populate L1 -> Return Data    |
|   +-----------+-----------+                                                   |
|               | L2 Miss                                                       |
|               v                                                               |
|   +-----------+-----------+                                                   |
|   | Primary Database      |   DB Hit (~15 ms) -> Populate L2 -> Populate L1   |
|   +-----------------------+                                                   |
|                                                                               |
|   Invalidation Flow on Mutation:                                              |
|   Node A updates DB -> Deletes L2 key -> Publishes Invalidation Event to Redis|
|   Node B, C subscribe to Redis Pub/Sub -> Evict local key from L1 Caffeine    |
+-------------------------------------------------------------------------------+
```

### 3.2 Cache Stampede (Thundering Herd) Prevention

When a hot cache key expires under 10,000 requests/sec, all threads simultaneously miss the cache and overwhelm the database.

Mitigation strategies:
1. **Mutex Lock / Single-Flight Request**: Only one thread acquires a distributed or local lock to recompute the value; others wait or receive stale data.
2. **Probabilistic Early Expiration (XFetch)**:
   $$e^{-\frac{\beta \cdot \delta \cdot \ln(U)}{\text{TTL}}}$$
   Background thread proactively refreshes the key before absolute TTL expiration based on compute delta $\delta$ and random variable $U \in (0, 1]$.

---

## 4. Syntax & Usage

### 4.1 In-Memory LRU Cache with `LinkedHashMap`

Java's `LinkedHashMap` provides an eviction hook `removeEldestEntry(Map.Entry<K,V>)`:

```java
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.locks.ReentrantReadWriteLock;

public class BoundedLruCache<K, V> {
    private final int capacity;
    private final Map<K, V> map;
    private final ReentrantReadWriteLock lock = new ReentrantReadWriteLock();

    public BoundedLruCache(int capacity) {
        this.capacity = capacity;
        this.map = new LinkedHashMap<>(capacity, 0.75f, true) {
            @Override
            protected boolean removeEldestEntry(Map.Entry<K, V> eldest) {
                return size() > BoundedLruCache.this.capacity;
            }
        };
    }

    public V get(K key) {
        lock.writeLock().lock();
        try {
            return map.get(key);
        } finally {
            lock.writeLock().unlock();
        }
    }

    public void put(K key, V value) {
        lock.writeLock().lock();
        try {
            map.put(key, value);
        } finally {
            lock.writeLock().unlock();
        }
    }

    public int size() {
        lock.readLock().lock();
        try {
            return map.size();
        } finally {
            lock.readLock().unlock();
        }
    }
}
```

---

## 5. Practical Examples

### 5.1 Basic Example: Cache-Aside with Single-Flight Mutex Locking

```java
import java.time.Duration;
import java.time.Instant;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.locks.ReentrantLock;
import java.util.function.Function;

public final class CacheAsideManager<K, V> {
    private record CacheEntry<V>(V value, Instant expiresAt) {
        boolean isExpired() {
            return Instant.now().isAfter(expiresAt);
        }
    }

    private final Map<K, CacheEntry<V>> cache = new ConcurrentHashMap<>();
    private final Map<K, ReentrantLock> keyLocks = new ConcurrentHashMap<>();
    private final Duration ttl;

    public CacheAsideManager(Duration ttl) {
        this.ttl = Objects.requireNonNull(ttl);
    }

    public V getOrCompute(K key, Function<K, V> databaseLoader) {
        CacheEntry<V> entry = cache.get(key);
        if (entry != null && !entry.isExpired()) {
            return entry.value();
        }

        ReentrantLock keyLock = keyLocks.computeIfAbsent(key, k -> new ReentrantLock());
        keyLock.lock();
        try {
            entry = cache.get(key);
            if (entry != null && !entry.isExpired()) {
                return entry.value();
            }

            V freshValue = databaseLoader.apply(key);
            if (freshValue != null) {
                cache.put(key, new CacheEntry<>(freshValue, Instant.now().plus(ttl)));
            }
            return freshValue;
        } finally {
            keyLock.unlock();
            keyLocks.remove(key, keyLock);
        }
    }

    public void evict(K key) {
        cache.remove(key);
    }
}
```

### 5.2 Real-World Example: Multi-Tier L1/L2 Invalidation Gateway

```java
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Consumer;

public final class MultiTierCacheGateway<K, V> {
    public interface DistributedCacheClient<K, V> {
        Optional<V> get(K key);
        void set(K key, V value, long ttlSeconds);
        void delete(K key);
        void publishInvalidation(K key);
        void registerInvalidationListener(Consumer<K> listener);
    }

    private final ConcurrentHashMap<K, V> localL1Cache = new ConcurrentHashMap<>();
    private final DistributedCacheClient<K, V> distributedL2;

    public MultiTierCacheGateway(DistributedCacheClient<K, V> distributedL2) {
        this.distributedL2 = distributedL2;
        this.distributedL2.registerInvalidationListener(localL1Cache::remove);
    }

    public Optional<V> fetch(K key) {
        V localValue = localL1Cache.get(key);
        if (localValue != null) {
            return Optional.of(localValue);
        }

        Optional<V> l2Value = distributedL2.get(key);
        if (l2Value.isPresent()) {
            localL1Cache.put(key, l2Value.get());
            return l2Value;
        }

        return Optional.empty();
    }

    public void put(K key, V value, long ttlSeconds) {
        distributedL2.set(key, value, ttlSeconds);
        localL1Cache.put(key, value);
        distributedL2.publishInvalidation(key);
    }

    public void invalidate(K key) {
        localL1Cache.remove(key);
        distributedL2.delete(key);
        distributedL2.publishInvalidation(key);
    }
}
```

### 5.3 Edge Case: Mitigating Cache Penetration with Null Object Caching & Bloom Filters

```java
import java.time.Duration;
import java.util.Optional;
import java.util.function.Function;

public final class CachePenetrationProtector<K, V> {
    private static final Object NULL_SENTINEL = new Object();
    private final CacheAsideManager<K, Object> innerCache;

    public CachePenetrationProtector(Duration negativeCacheTtl) {
        this.innerCache = new CacheAsideManager<>(negativeCacheTtl);
    }

    @SuppressWarnings("unchecked")
    public Optional<V> getProtected(K key, Function<K, Optional<V>> databaseFetcher) {
        Object cached = innerCache.getOrCompute(key, k -> {
            Optional<V> dbResult = databaseFetcher.apply(k);
            return dbResult.isPresent() ? dbResult.get() : NULL_SENTINEL;
        });

        if (cached == NULL_SENTINEL) {
            return Optional.empty();
        }
        return Optional.of((V) cached);
    }
}
```

---

## 6. Best Practices

1. **Always Set TTLs**: Never introduce a cache entry without an explicit Time-To-Live. Unbounded caches lead directly to `OutOfMemoryError`.
2. **Apply Jitter to TTLs**: Avoid setting identical 1-hour TTLs on thousands of items loaded concurrently. Add $\pm 10\%$ uniform random jitter to prevent coordinated expiration spikes.
3. **Handle Cache Down Gracefully**: Always wrap external cache calls in circuit breakers or try-catch blocks to prevent cache outages from taking down the application.
4. **Use High-Performance Libraries**: For JVM-local caching, use **Caffeine** over custom `ConcurrentHashMap` implementations. Caffeine achieves near-optimal W-TinyLFU eviction and outperforms Guava Cache by 3–5x.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Mutable Cached References** | Callers mutate the cached instance directly, corrupting state for all subsequent readers | Store immutable records or return defensive deep copies |
| **Cache Stampede on Cold Startup** | Cache restarts empty, and heavy traffic instantly craters downstream databases | Pre-warm cache during startup or use single-flight loaders |
| **Cache Penetration by Malicious IDs** | Attackers query non-existent IDs (`id=-9999`), bypassing cache and hitting DB directly | Cache `NULL_SENTINEL` with short TTL or use a Bloom filter |
| **Big Keys in Distributed Cache** | Storing 50MB JSON blobs in Redis blocks single-threaded event loop | Chunk large datasets or store references in blob storage (S3) |
| **Lack of Cache Invalidation on Writes**| Stale data served for hours across cluster nodes | Implement transactional outbox or Redis Pub/Sub invalidation |

---

## 8. Performance & Security Notes

### 8.1 Garbage Collection and Off-Heap Memory
Large JVM heap caches ($> 10\text{ GB}$) increase garbage collection pause times under traditional collectors. When maintaining hundreds of gigabytes in-memory on Java:
- Prefer modern low-latency collectors: **ZGC** or **Shenandoah**.
- Use off-heap cache engines (e.g., Chronicle Map or Hazelcast Off-Heap) to store serialized byte buffers outside the JVM garbage collected heap.

### 8.2 Security Considerations
- **Cache Poisoning**: Validate and sanitize input keys prior to serialization.
- **Sensitive Data Exposure**: Encrypt PII (personally identifiable information) and credit card data before storing in shared L2 caches (Redis).

---

## 9. Comparison / When to Use What

| Feature | In-Process Heap (Caffeine) | Distributed In-Memory (Redis) | CDN / Reverse Proxy (Varnish, Cloudflare) |
| :--- | :--- | :--- | :--- |
| **Access Latency** | $< 100\text{ ns}$ | $0.5 - 2.0\text{ ms}$ | $10 - 50\text{ ms}$ |
| **Scope** | Single JVM instance | Shared across microservice cluster | Global edge locations |
| **Data Size** | Megabytes to few Gigabytes | Tens of Gigabytes to Terabytes | Terabytes of static/semi-static assets |
| **Network Overhead** | Zero | TCP/socket serialization | HTTP/HTTPS round-trip |
| **Ideal For** | High-frequency reference tables, config | Session state, shared aggregates, rate limits | Static assets, full HTTP response caching |

---

## 10. Interview Questions

<details>
<summary>1. What is the Cache Stampede problem and how do you prevent it?</summary>
Cache Stampede (Thundering Herd) occurs when a heavily requested cache key expires, causing thousands of concurrent requests to simultaneously experience a cache miss and execute the identical database query. It is resolved using mutex locking (single-flight computation where only the lock owner queries the DB while others wait) or probabilistic early expiration (XFetch algorithm).
</details>

<details>
<summary>2. Why does Caffeine outperform Guava Cache and LinkedHashMap?</summary>
Caffeine uses Window TinyLFU (W-TinyLFU) eviction, which maintains a small window cache to admit new items and a frequency sketch (4-bit Count-Min Sketch) to retain hot items. Additionally, Caffeine performs lock-free reads using ring-buffer event logs that are batched and processed asynchronously, avoiding lock contention.
</details>

<details>
<summary>3. Differentiate between Cache-Aside, Read-Through, and Write-Behind.</summary>
Cache-Aside requires application code to explicitly coordinate reading/writing to cache and database. Read-Through encapsulates the DB loader inside the cache itself. Write-Behind (Write-Back) writes updates to the cache and asynchronously flushes them in batches to the database.
</details>

<details>
<summary>4. How do you resolve cache invalidation across multiple microservice instances?</summary>
Using event-driven invalidation: when an update occurs, the mutator publishes a cache eviction event over a messaging bus or Redis Pub/Sub topic. All subscribing nodes receive the event and evict the corresponding key from their local L1 caches.
</details>

<details>
<summary>5. What is the difference between Cache Penetration and Cache Breakdown?</summary>
Cache Breakdown is when a single hot key expires and traffic overwhelms the DB. Cache Penetration is when queries request non-existent keys (which are never cached), causing every request to bypass cache and hammer the database.
</details>

<details>
<summary>6. How does a Bloom Filter prevent Cache Penetration?</summary>
A Bloom Filter is a space-efficient probabilistic data structure that tests set membership with zero false negatives (if it says an element is absent, it is guaranteed absent). The application checks the Bloom Filter before hitting the cache; non-existent keys are rejected instantly without querying DB.
</details>

<details>
<summary>7. What are the dangers of Write-Behind caching in financial applications?</summary>
Write-Behind poses data loss risk if the cache process crashes before dirty entries are flushed to persistent storage. Financial systems mandate synchronous write-through or transactional commits.
</details>

<details>
<summary>8. Why should you add jitter to cache expiration TTLs?</summary>
If 100,000 keys are loaded at midnight with a static 2-hour TTL, all 100,000 keys will expire simultaneously at 02:00, creating an instantaneous latency spike. Random jitter spreads expirations smoothly across time.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Implement a Thread-Safe Time-Based Expiring Cache from scratch</summary>

```java
import java.time.Instant;
import java.util.concurrent.*;

public class SimpleExpiringCache<K, V> {
    private record TimedValue<V>(V value, Instant expiry) {}
    private final ConcurrentMap<K, TimedValue<V>> store = new ConcurrentHashMap<>();

    public void put(K key, V value, long ttlMillis) {
        store.put(key, new TimedValue<>(value, Instant.now().plusMillis(ttlMillis)));
    }

    public V get(K key) {
        TimedValue<V> tv = store.get(key);
        if (tv == null) return null;
        if (Instant.now().isAfter(tv.expiry())) {
            store.remove(key, tv);
            return null;
        }
        return tv.value();
    }
}
```
</details>

<details>
<summary>Exercise 2: Implement Single-Flight Request Coalescing</summary>

```java
import java.util.concurrent.*;
import java.util.function.Function;

public class SingleFlightCoalescer<K, V> {
    private final ConcurrentMap<K, CompletableFuture<V>> inFlight = new ConcurrentHashMap<>();

    public CompletableFuture<V> execute(K key, Function<K, V> heavyComputation) {
        return inFlight.computeIfAbsent(key, k -> CompletableFuture.supplyAsync(() -> {
            try {
                return heavyComputation.apply(k);
            } finally {
                inFlight.remove(k);
            }
        }));
    }
}
```
</details>

<details>
<summary>Exercise 3: Implement Negative Caching with Sentinel Pattern</summary>

```java
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;

public class NegativeCache<K, V> {
    private static final Object ABSENT = new Object();
    private final Map<K, Object> map = new ConcurrentHashMap<>();

    public void putFound(K key, V value) { map.put(key, value); }
    public void putNotFound(K key) { map.put(key, ABSENT); }

    @SuppressWarnings("unchecked")
    public Optional<V> get(K key) {
        Object val = map.get(key);
        if (val == null) return Optional.empty();
        if (val == ABSENT) return Optional.ofNullable(null);
        return Optional.of((V) val);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **L1 Cache**: In-process heap (Caffeine, $< 100\text{ ns}$). Fastest, but isolated to single node.
- **L2 Cache**: Shared distributed (Redis, $\sim 1\text{ ms}$). Consistent across cluster, requires serialization.
- **Cache-Aside**: Read cache $\to$ Miss $\to$ Read DB $\to$ Populate cache. Resilient default.
- **Stampede Defense**: Mutex locking or single-flight coalescing around DB fetch.
- **Penetration Defense**: Bloom filter or caching null sentinel objects with short TTL.
- **Eviction**: Default to W-TinyLFU (Caffeine) for optimal hit ratios and resistance to scan pollution.

---

## 13. Further Reading

- [Caffeine Cache GitHub & Architecture](https://github.com/ben-manes/caffeine)
- [Redis Caching Best Practices (AWS Whitepapers)](https://aws.amazon.com/caching/best-practices/)
- Martin Kleppmann, *Designing Data-Intensive Applications* (O'Reilly, 2017)
