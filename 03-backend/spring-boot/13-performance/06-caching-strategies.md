# Caching Strategies: Multi-Level Caching with Caffeine and Redis

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 13-performance |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Configuration Reference](#4-syntax--configuration-reference)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
In high-throughput microservices, repeated database queries for relatively static data (e.g. product catalogs, exchange rates, user authorization roles) severely degrade latency and saturate database connection pools:
- Querying a database over JDBC incurs 2ms to 20ms of latency per request.
- Relying purely on an external distributed cache (Redis/Memcached) over the network introduces 0.5ms to 3ms network roundtrip overhead and network I/O serialization costs.
- Relying purely on an in-memory local cache (Caffeine/Ehcache) causes data inconsistency across horizontal container replicas and leads to JVM memory bloat.
- When cached items expire simultaneously, hundreds of concurrent threads flood the database in a catastrophic **Cache Stampede (Thundering Herd)**.

### Historical Evolution
Spring Framework introduced the declarative caching abstraction (`@Cacheable`, `@CachePut`, `@CacheEvict`) in Spring 3.1. 

In modern cloud-native systems, enterprise architectures standardize on **Multi-Level Caching (L1/L2)**:
- **L1 Cache (In-Memory)**: Caffeine cache running directly within JVM memory, serving requests in nanoseconds ($< 50\text{ns}$).
- **L2 Cache (Distributed)**: Redis cluster shared across all container instances, serving cache misses in 1ms.
- **Cache Invalidation Sync**: Coordinated via Redis Pub/Sub topic broadcasts.

### Production Relevance
A properly architected two-tier caching strategy enables:
- 99% of requests to be resolved directly from in-memory L1 cache without network hops.
- Complete protection of primary databases during viral traffic spikes.
- Near real-time cache consistency across horizontal pod replicas via pub/sub invalidation.

---

## 2. Core Concepts

### Multi-Level Cache Architecture
1. **L1 Local Cache (Caffeine)**: In-process, lock-free JVM memory cache utilizing **W-TinyLFU** (Window Tiny Least Frequently Used) eviction algorithms, delivering maximum hit ratios and near-zero latency.
2. **L2 Distributed Cache (Redis)**: Shared out-of-process distributed cache preserving state across rolling restarts, pod scaling events, and microservice replicas.
3. **Cache-Aside Pattern (Lazy Loading)**: Application code checks the cache first; on a miss, loads from the database, populates the cache, and returns.
4. **Cache Stampede (Thundering Herd)**: Occurs when a high-traffic cache key expires, causing thousands of concurrent requests to experience a cache miss simultaneously and hammer the database.
5. **Probabilistic Early Expiration (XFetch)**: Recomputing cache values in the background slightly before their hard expiration time based on access frequency.
6. **Cache Invalidation Broadcast**: When a service updates an entity, it writes to the DB, updates L2 Redis, and broadcasts a message over Redis Pub/Sub to invalidate L1 Caffeine entries across all other pods.

---

## 3. How It Works Internally

### Multi-Level (L1/L2) Lookup Pipeline & Invalidation Flow

```
+------------------------------------------------------------------------------------+
|                                  CLIENT REQUEST                                     |
|                             GET /api/products/sku-9921                             |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
| 1. L1 LOCAL CACHE CHECK (Caffeine in JVM Heap)                                     |
|    --> HIT: Return Product immediately (< 50 nanoseconds, Zero Network I/O!)       |
|    --> MISS: Proceed to L2 Cache                                                   |
+------------------------------------------------------------------------------------+
                                         | (On L1 Miss)
                                         v
+------------------------------------------------------------------------------------+
| 2. L2 DISTRIBUTED CACHE CHECK (Redis Cluster)                                      |
|    --> HIT: Return Product (~1ms network hop)                                      |
|             Backfills L1 Caffeine cache for subsequent local requests              |
|    --> MISS: Proceed to Database                                                   |
+------------------------------------------------------------------------------------+
                                         | (On L2 Miss)
                                         v
+------------------------------------------------------------------------------------+
| 3. DATABASE QUERY & CACHE RE-POPULATION                                            |
|    - Queries PostgreSQL / MySQL via JDBC (~15ms)                                   |
|    - Populates L2 Redis with TTL (e.g., 1 hour)                                    |
|    - Populates L1 Caffeine with short TTL (e.g., 5 minutes)                         |
|    - Returns Product to Client                                                     |
+------------------------------------------------------------------------------------+

======================================================================================
UPDATE / MUTATION FLOW & POD SYNCHRONIZATION
======================================================================================
POD A (Executes Update Product 9921):
  1. Updates PostgreSQL Database
  2. Evicts key from Redis L2
  3. Publishes event on Redis Channel: 'cache:evict:products:9921'

POD B, POD C, POD D (Subscribed to Redis Channel):
  - Receive invalidation message
  - Invalidate key 'products:9921' in local L1 Caffeine cache instantly!
```

---

## 4. Syntax & Configuration Reference

### Dependencies (`pom.xml`)
```xml
<!-- Spring Boot Cache Starter -->
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-cache</artifactId>
</dependency>

<!-- Caffeine High-Performance In-Memory Cache -->
<dependency>
    <groupId>com.github.ben-manes.caffeine</groupId>
    <artifactId>caffeine</artifactId>
</dependency>

<!-- Spring Data Redis for Distributed L2 -->
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-data-redis</artifactId>
</dependency>
```

### Application Configuration (`application.yml`)
```yaml
spring:
  cache:
    type: caffeine # Default primary cache
    cache-names:
      - products
      - exchangeRates
    caffeine:
      spec: maximumSize=10000,expireAfterWrite=5m,recordStats
  data:
    redis:
      host: redis-cluster.internal
      port: 6379
      timeout: 1000ms
```

---

## 5. Practical Examples

### Example 1: Standard Declarative Spring Caching
```java
package com.example.performance.caching;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.cache.annotation.CacheEvict;
import org.springframework.cache.annotation.CachePut;
import org.springframework.cache.annotation.Cacheable;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;

@Service
public class ProductCatalogService {

    private static final Logger log = LoggerFactory.getLogger(ProductCatalogService.class);
    private final ProductRepository productRepository;

    public ProductCatalogService(ProductRepository productRepository) {
        this.productRepository = productRepository;
    }

    // Cached on read
    @Cacheable(value = "products", key = "#sku", unless = "#result == null")
    public Product getProductBySku(String sku) {
        log.info("CACHE MISS: Fetching product [{}] from relational database...", sku);
        return productRepository.findBySku(sku).orElse(null);
    }

    // Updates cache when entity is modified
    @CachePut(value = "products", key = "#product.sku")
    public Product updateProduct(Product product) {
        log.info("Updating product [{}] in database and refreshing cache", product.getSku());
        return productRepository.save(product);
    }

    // Evicts cache on deletion
    @CacheEvict(value = "products", key = "#sku")
    public void deleteProduct(String sku) {
        log.info("Deleting product [{}] and evicting from cache", sku);
        productRepository.deleteBySku(sku);
    }
}
```

### Example 2: Implementing a Custom Two-Tier (L1 Caffeine + L2 Redis) CacheManager
```java
package com.example.performance.caching;

import com.github.benmanes.caffeine.cache.Caffeine;
import org.springframework.cache.Cache;
import org.springframework.cache.support.AbstractValueAdaptingCache;
import org.springframework.data.redis.core.RedisTemplate;

import java.time.Duration;
import java.util.concurrent.Callable;

public class MultiLevelCache extends AbstractValueAdaptingCache {

    private final String name;
    private final com.github.benmanes.caffeine.cache.Cache<Object, Object> l1Cache;
    private final RedisTemplate<Object, Object> l2RedisTemplate;
    private final Duration l2Ttl;

    public MultiLevelCache(String name, RedisTemplate<Object, Object> l2RedisTemplate, Duration l2Ttl) {
        super(true); // Allow null values
        this.name = name;
        this.l2RedisTemplate = l2RedisTemplate;
        this.l2Ttl = l2Ttl;
        this.l1Cache = Caffeine.newBuilder()
            .maximumSize(5000)
            .expireAfterWrite(Duration.ofMinutes(3))
            .recordStats()
            .build();
    }

    @Override
    public String getName() {
        return this.name;
    }

    @Override
    public Object getNativeCache() {
        return this.l1Cache;
    }

    @Override
    protected Object lookup(Object key) {
        // Step 1: Check in-memory L1 Caffeine
        Object value = l1Cache.getIfPresent(key);
        if (value != null) {
            return value;
        }

        // Step 2: Check distributed L2 Redis
        value = l2RedisTemplate.opsForValue().get(buildRedisKey(key));
        if (value != null) {
            // Backfill L1
            l1Cache.put(key, value);
            return value;
        }

        return null;
    }

    @Override
    public void put(Object key, Object value) {
        l1Cache.put(key, toStoreValue(value));
        l2RedisTemplate.opsForValue().set(buildRedisKey(key), toStoreValue(value), l2Ttl);
    }

    @Override
    public void evict(Object key) {
        l1Cache.invalidate(key);
        l2RedisTemplate.delete(buildRedisKey(key));
    }

    @Override
    public void clear() {
        l1Cache.invalidateAll();
    }

    @Override
    public <T> T get(Object key, Callable<T> valueLoader) {
        Object value = lookup(key);
        if (value != null) {
            return (T) fromStoreValue(value);
        }
        try {
            T loaded = valueLoader.call();
            put(key, loaded);
            return loaded;
        } catch (Exception ex) {
            throw new ValueRetrievalException(key, valueLoader, ex);
        }
    }

    private String buildRedisKey(Object key) {
        return this.name + ":" + key.toString();
    }
}
```

### Example 3: Mitigating Cache Stampede via `sync = true`
```java
package com.example.performance.caching;

import org.springframework.cache.annotation.Cacheable;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;

@Service
public class ExchangeRateService {

    // sync = true tells Spring Cache to use an internal mutex lock!
    // If 1,000 threads request USD-EUR simultaneously on a cache miss,
    // ONLY ONE thread calls the expensive remote API; the remaining 999 threads
    // wait and read the computed value directly from cache once populated!
    @Cacheable(value = "exchangeRates", key = "#pair", sync = true)
    public BigDecimal getLiveRate(String pair) {
        return fetchFromForeignExchangeProvider(pair);
    }

    private BigDecimal fetchFromForeignExchangeProvider(String pair) {
        // Expensive HTTP API call with rate limits
        return BigDecimal.valueOf(1.085);
    }
}
```

### Example 4: Anti-Pattern vs. Production Fix (Cache Penetration / Null Caching)
```java
// ANTI-PATTERN: Not caching null or missing entity results!
// Attack scenario: Attacker requests /api/users/999999999 with millions of random IDs.
// Result: Every single request misses cache and hits PostgreSQL -> Database crashes under DOS!
@Service
public class VulnerableUserService {
    @Cacheable(value = "users", key = "#id", unless = "#result == null") // ANTI-PATTERN: Omits nulls!
    public User findUser(Long id) {
        return userRepository.findById(id).orElse(null);
    }
}

// PRODUCTION FIX: Cache sentinel null values with short TTL to prevent Cache Penetration
@Service
public class HardenedUserService {
    // Caches null values so subsequent queries for non-existent IDs return immediately from cache!
    @Cacheable(value = "users", key = "#id")
    public Optional<User> findUser(Long id) {
        return userRepository.findById(id);
    }
}
```

### Example 5: Cross-Pod Cache Synchronization via Redis Pub/Sub
```java
package com.example.performance.caching;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.cache.CacheManager;
import org.springframework.data.redis.connection.Message;
import org.springframework.data.redis.connection.MessageListener;
import org.springframework.stereotype.Component;

import java.nio.charset.StandardCharsets;

@Component
public class CacheInvalidationSubscriber implements MessageListener {

    private static final Logger log = LoggerFactory.getLogger(CacheInvalidationSubscriber.class);
    private final CacheManager cacheManager;

    public CacheInvalidationSubscriber(CacheManager cacheManager) {
        this.cacheManager = cacheManager;
    }

    @Override
    public void onMessage(Message message, byte[] pattern) {
        String payload = new String(message.getBody(), StandardCharsets.UTF_8);
        // Payload format: "cacheName:key" (e.g. "products:SKU-100")
        String[] parts = payload.split(":", 2);
        if (parts.length == 2) {
            String cacheName = parts[0];
            String key = parts[1];

            var cache = cacheManager.getCache(cacheName);
            if (cache != null) {
                cache.evict(key);
                log.info("L1 Cache evicted via Redis Pub/Sub broadcast: [{}] in [{}]", key, cacheName);
            }
        }
    }
}
```

---

## 6. Best Practices & Production Guidelines

1. **Always Set TTLs (Time-To-Live)**: An in-memory or distributed cache without a TTL is a memory leak waiting to happen. Always configure `expireAfterWrite` on Caffeine and explicit expiration on Redis keys.
2. **Use `sync = true` for Hot Keys**: Prevent Cache Stampede (Thundering Herd) on high-traffic keys by specifying `@Cacheable(..., sync = true)`. This serializes concurrent cache-miss executions to a single database query.
3. **Cache Null Values to Prevent Cache Penetration**: Ensure queries for missing entities are cached with a short TTL (e.g. 60 seconds) to prevent malicious users or crawlers from bypassing the cache with random IDs.
4. **Monitor Cache Hit Ratios**: Enable Caffeine statistics (`recordStats()`) and export hit/miss rates to Prometheus via Micrometer (`cache.gets{result="hit"}`, `cache.gets{result="miss"}`). A cache with a hit ratio $< 60\%$ is consuming memory without delivering value.
5. **Never Store Unbounded Large Payloads in L1**: Storing large 10MB payloads in Caffeine exhausts JVM heap and triggers Full GC STW pauses. Store large objects in Redis or compress them before caching.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Missing `@EnableCaching` on `@Configuration` | Spring ignores all `@Cacheable` annotations; all queries hit database directly without errors. | Add `@EnableCaching` to a main configuration class. |
| Self-invocation of `@Cacheable` methods | Calling `this.myCachedMethod()` bypasses the Spring AOP proxy; cache interception is skipped. | Inject the service proxy or move method to a separate bean. |
| Forgetting `Serializable` for Redis values | Default Redis serializers fail with `SerializationException` if DTOs do not implement `Serializable`. | Use `GenericJackson2JsonRedisSerializer` for JSON serialization. |
| Unbounded Caffeine cache without `maximumSize` | Heap memory grows indefinitely until the JVM crashes with `OutOfMemoryError: Java heap space`. | Always configure `maximumSize` or `maximumWeight` in Caffeine specs. |
| Cache stampede on synchronized expiration | All keys in a batch expire at the exact same second, overloading the database. | Add random jitter to key TTLs (e.g. $300\text{s} \pm 30\text{s}$). |
| Stale L1 cache on multi-pod Kubernetes clusters | Pod A updates data; Pod B serves stale data for 10 minutes from its local JVM memory. | Use Redis Pub/Sub to broadcast L1 invalidation events across pods. |

---

## 8. Performance & Security Notes

### Caffeine W-TinyLFU Algorithm
- Traditional LRU (Least Recently Used) caches suffer when a temporary sequential scan (e.g. batch job) flushes high-frequency hot items out of memory.
- Caffeine's **Window TinyLFU** uses a small admission window to filter recency and a CountMin-Sketch frequency filter to preserve long-term popular items, achieving near-optimal hit ratios with $< 1\%$ memory overhead.

### Redis Cache Poisoning & Injection
- Never construct cache keys using unvalidated user inputs with special characters (`:`, `*`, `\r\n`).
- Sanitize keys to prevent key-namespace collisions or unintended wildcard evictions in Redis clusters.

---

## 9. Comparison / When to Use What

| Caching Solution | Access Latency | Scope | Max Storage Capacity | Network Overhead |
|---|---|---|---|---|
| **Caffeine (L1)** | **$< 50\text{ nanoseconds}$** | Single JVM Heap | Hundreds of MBs | **Zero** |
| **Redis (L2)** | **$0.5\text{ -- }2\text{ ms}$** | Multi-Pod Distributed | Gigabytes / Terabytes | TCP Network Hop |
| **Multi-Level (L1+L2)**| **$< 50\text{ns}$ (99% Hit)** | Coordinated Distributed | Terabytes | Low (Misses & Sync only) |
| **HTTP Reverse Proxy (Varnish/CDN)**| $5\text{ -- }20\text{ ms}$ | Edge / Ingress | Petabytes | Network Edge |

---

## 10. Interview Questions & Answers

### Q1: What is a Cache Stampede (Thundering Herd) and how does Spring Boot mitigate it?
<details>
<summary>Click to view answer</summary>

A cache stampede occurs when a high-traffic cache key expires under heavy load. Hundreds or thousands of concurrent requests experience a cache miss simultaneously and all attempt to query the database and recompute the value, causing database connection exhaustion and latency spikes.  
In Spring Boot, setting `@Cacheable(value = "data", sync = true)` enforces a local mutex lock. The first thread acquires the lock, queries the database, and populates the cache. The remaining threads wait for the lock and then read the newly populated value directly from the cache.
</details>

### Q2: What is the difference between `@Cacheable`, `@CachePut`, and `@CacheEvict`?
<details>
<summary>Click to view answer</summary>

- `@Cacheable`: Checks the cache first. If a cached value exists, returns it immediately without executing the method. If missing, executes the method and stores the result in the cache.
- `@CachePut`: Always executes the method body and updates the cache with the method's return value. Used for update mutations.
- `@CacheEvict`: Removes one or more entries from the cache. Used for deletions or invalidations.
</details>

### Q3: Why is a multi-level (L1/L2) caching architecture superior to standalone Redis?
<details>
<summary>Click to view answer</summary>

Even with a dedicated 10Gbps network, calling Redis over TCP takes 0.5ms to 2ms per query due to network serialization and socket I/O. In high-concurrency systems handling 50,000 req/sec, Redis CPU and network interfaces can become a bottleneck. By placing Caffeine (L1) in-memory in front of Redis (L2), 95–99% of requests are resolved in memory in under 50 nanoseconds, eliminating network hops and reducing Redis cluster load by 90%.
</details>

### Q4: How does Caffeine's W-TinyLFU eviction policy work?
<details>
<summary>Click to view answer</summary>

Window TinyLFU divides the cache into:
1. **Window Cache (20%)**: Employs LRU eviction to retain newly arrived items and give them a chance to accumulate popularity.
2. **Main Cache (80%)**: Employs segmented LRU (SLRU) protected by a **CountMin Sketch** (a 4-bit probabilistic frequency filter).
When an item is evicted from the window, TinyLFU compares its access frequency against the victim in the main cache: only if the new item is accessed more frequently does it replace the main cache entry, preventing one-off batch scans from polluting the cache.
</details>

### Q5: What is Cache Penetration and how do you protect against it?
<details>
<summary>Click to view answer</summary>

Cache Penetration occurs when queries request keys that do not exist in the database (e.g. querying random non-existent User IDs). Because the data is missing, the cache never stores it, causing every single request to bypass the cache and hit the database directly.  
Protection:
1. Cache `null` or empty `Optional` values with a short TTL (e.g. 60 seconds).
2. Use a **Bloom Filter** in front of the cache to immediately reject keys that definitely do not exist in the dataset.
</details>

### Q6: Why does calling a `@Cacheable` method from within the same class fail to hit the cache?
<details>
<summary>Click to view answer</summary>

Spring Caching is implemented via Spring AOP (Aspect-Oriented Programming) proxies. When a method is invoked from outside the bean, the invocation passes through the proxy, which executes the caching interceptor. When a method calls another method inside the same class (`this.method()`), the call bypasses the Spring proxy, invoking the raw target object directly without triggering cache lookup.
</details>

### Q7: What is the purpose of `recordStats()` in Caffeine?
<details>
<summary>Click to view answer</summary>

`recordStats()` enables internal instrumentation in Caffeine that tracks hit counts, miss counts, eviction counts, and load times. Spring Boot Actuator and Micrometer automatically discover these stats and export them to Prometheus under `cache.gets{result="hit"}`, `cache.evictions`, etc., enabling real-time Grafana dashboard monitoring.
</details>

### Q8: How do you handle cache invalidation across 50 microservice pod replicas?
<details>
<summary>Click to view answer</summary>

Use a **Publish/Subscribe bus** (such as Redis Pub/Sub, Kafka, or RabbitMQ). Whenever any pod mutates an entity, it updates the database, invalidates the distributed L2 Redis cache, and publishes an invalidation message (e.g. `product-evict:SKU-100`). All 50 microservice instances subscribe to this channel and evict `SKU-100` from their local in-memory L1 Caffeine cache upon message receipt.
</details>

---

## 11. Practice Exercises

### Exercise 1: Configure Caffeine Cache with Spec String (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Configure a Caffeine `CacheManager` bean in JavaConfig with a maximum size of 5,000 entries, an expiration of 10 minutes after write, and statistics recording enabled.

**Solution**:
```java
package com.example.performance.caching;

import com.github.benmanes.caffeine.cache.Caffeine;
import org.springframework.cache.CacheManager;
import org.springframework.cache.caffeine.CaffeineCacheManager;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import java.util.concurrent.TimeUnit;

@Configuration
public class CacheConfiguration {

    @Bean
    public CacheManager cacheManager() {
        CaffeineCacheManager manager = new CaffeineCacheManager("users", "roles");
        manager.setCaffeine(Caffeine.newBuilder()
            .maximumSize(5000)
            .expireAfterWrite(10, TimeUnit.MINUTES)
            .recordStats());
        return manager;
    }
}
```
</details>

### Exercise 2: Declarative Caching with Key SpEL Expressions (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a cached method where the cache key is composed of `tenantId` and `userId` using SpEL, and results are only cached if `user.active == true`.

**Solution**:
```java
package com.example.performance.caching;

import org.springframework.cache.annotation.Cacheable;
import org.springframework.stereotype.Service;

@Service
public class TenantUserService {

    @Cacheable(
        value = "tenantUsers",
        key = "#tenantId + ':' + #userId",
        condition = "#tenantId != null",
        unless = "#result == null || !#result.isActive()"
    )
    public UserProfile getUser(String tenantId, Long userId) {
        return loadUserFromDatabase(tenantId, userId);
    }

    private UserProfile loadUserFromDatabase(String tenantId, Long userId) {
        return new UserProfile(userId, tenantId, true);
    }
}
```
</details>

### Exercise 3: Complete Two-Tier Cache with Redis Invalidation Publisher (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Implement a service method that updates an account balance in the database and publishes an invalidation event over a Redis topic `cache:evict:accounts` so all other instances clear their local L1 caches.

**Solution**:
```java
package com.example.performance.caching;

import org.springframework.cache.CacheManager;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;

@Service
public class AccountBalanceService {

    private final AccountRepository accountRepository;
    private final CacheManager cacheManager;
    private final StringRedisTemplate redisTemplate;
    private static final String TOPIC = "cache:evict:accounts";

    public AccountBalanceService(AccountRepository accountRepository,
                                 CacheManager cacheManager,
                                 StringRedisTemplate redisTemplate) {
        this.accountRepository = accountRepository;
        this.cacheManager = cacheManager;
        this.redisTemplate = redisTemplate;
    }

    @Transactional
    public void updateBalance(Long accountId, BigDecimal newBalance) {
        // Step 1: Update PostgreSQL Database
        accountRepository.updateBalance(accountId, newBalance);

        // Step 2: Evict local L1 Caffeine cache
        var localCache = cacheManager.getCache("accounts");
        if (localCache != null) {
            localCache.evict(accountId);
        }

        // Step 3: Broadcast invalidation event to all sibling pods via Redis Pub/Sub
        redisTemplate.convertAndSend(TOPIC, String.valueOf(accountId));
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Feature / Annotation                        | Description & Architecture Role                             |
+---------------------------------------------+-------------------------------------------------------------+
| @Cacheable(value="c", sync=true)            | Caches read result; mutex lock prevents Cache Stampede      |
| @CachePut(value="c", key="#k")              | Always executes method and updates cache with return value  |
| @CacheEvict(value="c", key="#k")            | Removes entry from cache on mutation/deletion               |
| Caffeine (L1)                               | Ultra-fast JVM memory cache (<50ns access, W-TinyLFU)       |
| Redis (L2)                                  | Distributed shared cache (0.5-2ms access, persists on reboot|
| Multi-Level Caching                         | L1 in-memory + L2 distributed + Redis Pub/Sub invalidation   |
| Cache Penetration                           | Mitigated by caching null values with short TTL             |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Spring Boot Official Documentation: Cache Abstraction
- Ben Manes: Design of the Caffeine Caching Library & W-TinyLFU
- High-Performance Caching Patterns: Cache-Aside, Write-Through, Refresh-Ahead
- Redis Documentation: Pub/Sub Messaging Patterns