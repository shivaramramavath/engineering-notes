# Caching Abstraction with Caffeine and Redis

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `spring-boot-starter-cache`, `com.github.ben-manes.caffeine:caffeine:3.1.x`, `spring-boot-starter-data-redis` |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
In high-concurrency systems, repeatedly querying relational databases for frequently requested, slow-changing data (user profiles, catalog items, geographic pricing rules, tax rates) results in severe database connection pool saturation, elevated query latency, and high cloud database compute costs. Under high load, repetitive queries create I/O bottlenecks that degrade overall application responsiveness.

### The Evolution
Spring introduced the declarative Caching Abstraction in Spring 3.1. Rather than forcing developers to manually write boilerplate check-and-populate caching logic (`if (cache.contains(key)) ... else ...`), Spring encapsulates caching behind annotations (`@Cacheable`, `@CachePut`, `@CacheEvict`) and a pluggable SPI (`CacheManager`, `Cache`). Applications can transparently switch or combine near-cache in-memory implementations (Caffeine) and distributed remote stores (Redis) without modifying business logic.

### Relevance in Modern Engineering
Modern enterprise applications leverage multi-level caching (L1 in-memory Caffeine + L2 distributed Redis). Caffeine delivers sub-microsecond in-process access, while Redis ensures consistency across horizontally scaled Kubernetes nodes, preventing database thundering herds during cache warm-up.

---

## 2. Core Concepts

- **Cache Abstraction SPI**:
  - `CacheManager`: The registry bean responsible for creating and retrieving named `Cache` instances.
  - `Cache`: Interface defining key-value operations (`get(key)`, `put(key, value)`, `evict(key)`, `clear()`).
- **Core Annotations**:
  - `@Cacheable`: Checks the cache first. If a cached value exists, returns it immediately without executing the target method. If absent, executes the method and caches the result.
  - `@CachePut`: Always executes the method and updates the cache with the newly calculated return value.
  - `@CacheEvict`: Removes one or all entries from the specified cache (e.g., when an entity is deleted or updated).
  - `@Caching`: Allows grouping multiple caching annotations on a single method.
- **Spring Expression Language (SpEL)**: Dynamic evaluation of cache keys, conditions (`condition = "#id > 10"`), and post-execution evaluations (`unless = "#result == null"`).
- **Caffeine Cache (L1)**: Ultra-high performance, in-memory Java caching library providing near-optimal eviction using the Window TinyLFU algorithm.
- **Redis Cache (L2)**: Distributed, out-of-process in-memory data store accessible across multiple server instances.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Spring Cache Interception Pipeline                         |
|                                                                                   |
| Client Request                                                                    |
|    userService.getUserById("usr_101")                                            |
|                   |                                                               |
|                   v                                                               |
| 1. Spring CGLIB Cache Proxy Interceptor                                           |
|    +-----------------------------+                                                |
|    | CacheInterceptor            | ---> Evaluates SpEL key: "usr_101"             |
|    +-----------------------------+                                                |
|                   |                                                               |
|                   v                                                               |
| 2. CacheManager Lookup (e.g., Redis or Caffeine)                                  |
|    Cache cache = cacheManager.getCache("users");                                  |
|    ValueWrapper wrapper = cache.get("usr_101");                                   |
|                   |                                                               |
|         +---------+---------+                                                     |
|         | Cache Hit         | Cache Miss                                          |
|         v                   v                                                     |
|   Return cached DTO   3. Execute Real Target Method (@Transactional DB query)     |
|   (Bypasses DB!)      User user = userRepository.findById("usr_101");             |
|                             |                                                     |
|                             v                                                     |
|                       4. Populate Cache                                           |
|                          cache.put("usr_101", user);                              |
|                             |                                                     |
|                             v                                                     |
|                       Return user to caller                                       |
+-----------------------------------------------------------------------------------+
```

When an application invokes an annotated method, Spring's `CacheInterceptor` intercepts the call via AOP. It evaluates the SpEL key expression (or uses `SimpleKeyGenerator` by default). The interceptor queries the configured `CacheManager`. On a cache hit, the cached object is returned immediately and method execution is skipped. On a cache miss, the actual target method executes, its return value is stored in the cache, and the value is returned to the caller.

---

## 4. Syntax & Usage

### Maven Dependencies
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-cache</artifactId>
</dependency>
<dependency>
    <groupId>com.github.ben-manes.caffeine</groupId>
    <artifactId>caffeine</artifactId>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-data-redis</artifactId>
</dependency>
```

### Enable Caching
```java
@Configuration
@EnableCaching
public class CacheConfig {}
```

---

## 5. Practical Examples

### Example 1: High-Performance Caffeine In-Memory Cache Configuration
```java
package com.example.platform.cache.config;

import com.github.ben-manes.caffeine.cache.Caffeine;
import org.springframework.cache.CacheManager;
import org.springframework.cache.caffeine.CaffeineCacheManager;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import java.time.Duration;

@Configuration
public class CaffeineCacheConfig {

    @Bean
    public CacheManager caffeineCacheManager() {
        CaffeineCacheManager cacheManager = new CaffeineCacheManager("products", "pricingRules");
        cacheManager.setCaffeine(Caffeine.newBuilder()
                .initialCapacity(500)
                .maximumSize(10_000)
                .expireAfterWrite(Duration.ofMinutes(15))
                .recordStats());
        return cacheManager;
    }
}
```

### Example 2: Distributed Redis CacheManager with TTL and JSON Serialization
```java
package com.example.platform.cache.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.data.redis.cache.RedisCacheConfiguration;
import org.springframework.data.redis.cache.RedisCacheManager;
import org.springframework.data.redis.connection.RedisConnectionFactory;
import org.springframework.data.redis.serializer.GenericJackson2JsonRedisSerializer;
import org.springframework.data.redis.serializer.RedisSerializationContext;
import org.springframework.data.redis.serializer.StringRedisSerializer;

import java.time.Duration;
import java.util.HashMap;
import java.util.Map;

@Configuration
public class RedisCacheConfig {

    @Bean
    public RedisCacheManager redisCacheManager(RedisConnectionFactory connectionFactory) {
        // Default configuration: 1-hour TTL, JSON values, string keys
        RedisCacheConfiguration defaultConfig = RedisCacheConfiguration.defaultCacheConfig()
                .entryTtl(Duration.ofHours(1))
                .disableCachingNullValues()
                .serializeKeysWith(RedisSerializationContext.SerializationPair.fromSerializer(new StringRedisSerializer()))
                .serializeValuesWith(RedisSerializationContext.SerializationPair.fromSerializer(new GenericJackson2JsonRedisSerializer()));

        // Custom TTLs per cache name
        Map<String, RedisCacheConfiguration> cacheConfigs = new HashMap<>();
        cacheConfigs.put("userSessions", defaultConfig.entryTtl(Duration.ofMinutes(30)));
        cacheConfigs.put("productMetadata", defaultConfig.entryTtl(Duration.ofDays(1)));

        return RedisCacheManager.builder(connectionFactory)
                .cacheDefaults(defaultConfig)
                .withInitialCacheConfigurations(cacheConfigs)
                .build();
    }
}
```

### Example 3: Declarative Cache Operations with SpEL
```java
package com.example.platform.cache.service;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.cache.annotation.CacheEvict;
import org.springframework.cache.annotation.CachePut;
import org.springframework.cache.annotation.Cacheable;
import org.springframework.stereotype.Service;

import java.util.UUID;

public record ProductDto(String id, String sku, double price, boolean active) {}

@Service
public class ProductService {

    private static final Logger log = LoggerFactory.getLogger(ProductService.class);

    // Cache hit skips DB query. SpEL key: 'prod:123'
    @Cacheable(value = "products", key = "'prod:' + #id", unless = "#result == null || !#result.active()")
    public ProductDto getProductById(String id) {
        log.info("Cache miss! Querying database for product ID: {}", id);
        return new ProductDto(id, "SKU-" + id, 99.99, true);
    }

    // Always executes method and updates cache entry
    @CachePut(value = "products", key = "'prod:' + #product.id()")
    public ProductDto updateProduct(ProductDto product) {
        log.info("Updating product in database and refreshing cache for ID: {}", product.id());
        return product;
    }

    // Evicts specific product key from cache
    @CacheEvict(value = "products", key = "'prod:' + #id")
    public void deleteProduct(String id) {
        log.info("Deleting product and evicting cache key for ID: {}", id);
    }

    // Purges entire 'products' cache namespace
    @CacheEvict(value = "products", allEntries = true)
    public void evictAllProducts() {
        log.info("Flushing entire products cache");
    }
}
```

### Example 4: Caching Null / Default Identity Anti-Pattern and Fix
```java
// ANTI-PATTERN: Default caching configuration caching null values without TTL,
// causing a permanent negative cache (Cache Penetration) if a non-existent ID is requested.
// @Cacheable("users")
// public User getUser(String id) { return repo.findById(id).orElse(null); }

// FIX: Explicitly reject null caching with 'unless = "#result == null"' or configure .disableCachingNullValues()
package com.example.platform.cache.service;

import org.springframework.cache.annotation.Cacheable;
import org.springframework.stereotype.Service;

@Service
public class UserService {

    @Cacheable(value = "users", key = "#id", unless = "#result == null")
    public String findActiveUserEmail(String id) {
        // If method returns null, Spring Cache will NOT store null in Redis/Caffeine
        return null;
    }
}
```

### Example 5: High-Performance Multi-Level Cache (L1 Caffeine + L2 Redis)
```java
package com.example.platform.cache.composite;

import org.springframework.cache.Cache;
import org.springframework.cache.CacheManager;
import org.springframework.stereotype.Service;

import java.util.concurrent.Callable;

@Service
public class TwoLevelCacheService {

    private final CacheManager caffeineCacheManager;
    private final CacheManager redisCacheManager;

    public TwoLevelCacheService(CacheManager caffeineCacheManager, CacheManager redisCacheManager) {
        this.caffeineCacheManager = caffeineCacheManager;
        this.redisCacheManager = redisCacheManager;
    }

    @SuppressWarnings("unchecked")
    public <T> T get(String cacheName, String key, Class<T> targetClass, Callable<T> dbFallback) {
        Cache l1 = caffeineCacheManager.getCache(cacheName);
        Cache l2 = redisCacheManager.getCache(cacheName);

        // 1. Check L1 (Caffeine In-Memory: Sub-microsecond)
        if (l1 != null) {
            T value = l1.get(key, targetClass);
            if (value != null) {
                return value;
            }
        }

        // 2. Check L2 (Redis Distributed: ~1-2ms)
        if (l2 != null) {
            T value = l2.get(key, targetClass);
            if (value != null) {
                if (l1 != null) {
                    l1.put(key, value); // Populate L1 from L2
                }
                return value;
            }
        }

        // 3. Fallback to Database Query
        try {
            T dbValue = dbFallback.call();
            if (dbValue != null) {
                if (l1 != null) l1.put(key, dbValue);
                if (l2 != null) l2.put(key, dbValue);
            }
            return dbValue;
        } catch (Exception e) {
            throw new RuntimeException("Database fallback failed", e);
        }
    }
}
```

---

## 6. Best Practices

1. **Always Configure Time-To-Live (TTL)**: Never create infinite-lived cache entries in Redis; missing TTLs cause unbounded memory consumption and permanent data staleness.
2. **Use JSON Serialization for Redis**: Avoid default Java native serialization (`JdkSerializationRedisSerializer`) in Redis; use `GenericJackson2JsonRedisSerializer` for human-readable, schema-evolution-safe payloads.
3. **Guard Against Cache Stampede (Thundering Herd)**: When hot keys expire under peak traffic, thousands of concurrent threads hit the database simultaneously. Mitigate with distributed locks or probabilistic early expiration.
4. **Avoid Internal Bean Invocations**: Calling `@Cacheable` methods within the same class bypasses the Spring AOP proxy and executes without caching.
5. **Disable Null Caching (`disableCachingNullValues`)**: Prevent negative cache pollution unless negative caching is intentionally required to mitigate cache penetration attacks.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Invoking `@Cacheable` method from within the same class | CGLIB proxy is bypassed; method executes directly without checking cache | Extract cached method to a separate `@Service` bean and inject it. |
| Using default Java serialization in Redis | Incompatible class version UIDs break deserialization on application upgrades | Configure `GenericJackson2JsonRedisSerializer` or custom Jackson serializers. |
| Mutating cached objects in memory | Modifying a returned object mutates the shared L1 cache copy directly | Return immutable Java records or defensive copies from cached methods. |
| Missing `@EnableCaching` annotation | Caching annotations are silently ignored; methods execute on every call | Add `@EnableCaching` on a `@Configuration` class. |
| Cache Stampede / Dog-piling on hot key expiration | Database overwhelmed by thousands of concurrent misses on a single key | Use Redis distributed locks or mutexes during cache refill. |
| Cache Penetration from querying non-existent keys | Attackers repeatedly query non-existent IDs, forcing full DB queries | Use Bloom filters or cache empty placeholders with short 30-second TTLs. |

---

## 8. Performance & Security Notes

- **Cache Breakdown vs Penetration vs Avalanche**:
  - *Breakdown*: Single hot key expires $\to$ use mutual exclusion lock.
  - *Penetration*: Querying keys that never exist $\to$ use Bloom filters or short-lived null entries.
  - *Avalanche*: Many keys expire simultaneously $\to$ add random jitter to TTLs (e.g., $TTL \pm 10\%$).
- **Sensitive Data in Redis**: Encrypt sensitive personally identifiable information (PII) before writing to Redis, and enable TLS on Redis connections.

---

## 9. Comparison / When to Use What

| Dimension | Caffeine (L1 In-Memory) | Redis (L2 Distributed) | Database Query (No Cache) |
|---|---|---|---|
| **Latency** | Sub-microsecond (< 100ns) | 0.5ms - 2ms | 10ms - 200ms+ |
| **Storage Location** | JVM Heap | Out-of-process Redis Cluster | Persistent Disk (RDBMS/NoSQL) |
| **Consistency** | Node-local (drifts across nodes) | Strong consistency across cluster | Absolute transactional truth |
| **Capacity** | Constrained by JVM heap | Constrained by Redis cluster RAM | Terabytes / Petabytes |
| **Optimal For** | High-frequency read-heavy rules | Shared session state, entity cache | Authoritative write mutations |

---

## 10. Interview Questions

<details>
<summary>1. How does Spring's Caching Abstraction work under the hood?</summary>
Spring uses AOP proxies. When an annotated bean method is invoked, the `CacheInterceptor` intercepts the call, evaluates the cache key via SpEL, and checks the configured `CacheManager`. If a hit occurs, the cached value is returned immediately without executing the target method. On a miss, the method executes and the result is stored in the cache.
</details>

<details>
<summary>2. What is the difference between `@Cacheable` and `@CachePut`?</summary>
`@Cacheable` skips method execution if the key is already present in the cache. `@CachePut` always executes the method and updates the cache with the returned result. `@CachePut` is typically used on update/edit operations.
</details>

<details>
<summary>3. What is Cache Penetration, Cache Breakdown, and Cache Avalanche?</summary>
- *Cache Penetration*: Requests query non-existent data that never exists in cache or DB, repeatedly hitting the DB. (Mitigated by Bloom filters or short-lived null caching).
- *Cache Breakdown*: A single ultra-hot key expires under massive concurrency, causing thousands of DB queries simultaneously. (Mitigated by mutex locks).
- *Cache Avalanche*: A vast number of cached keys share identical TTLs and expire simultaneously, overwhelming the DB. (Mitigated by adding random jitter to TTLs).
</details>

<details>
<summary>4. Why is JdkSerializationRedisSerializer discouraged for Redis in production?</summary>
Java native serialization produces bloated binary payloads, requires classes to implement `Serializable`, is vulnerable to Java deserialization security exploits, and breaks completely whenever class definitions or `serialVersionUID` change across deployments.
</details>

<details>
<summary>5. How can you conditionally prevent caching null or invalid values using SpEL?</summary>
Using the `unless` attribute: `@Cacheable(value = "users", unless = "#result == null")`. The `unless` condition is evaluated *after* method execution; if true, the result is discarded and not cached.
</details>

<details>
<summary>6. Why should you avoid calling a `@Cacheable` method from another method in the same class?</summary>
Because Spring's caching mechanism relies on dynamic proxies (CGLIB). Internal calls (`this.method()`) bypass the proxy, meaning the caching interceptor never runs, and the method always executes directly against the database.
</details>

<details>
<summary>7. What eviction algorithm does Caffeine use and why is it superior to standard LRU?</summary>
Caffeine uses **Window TinyLFU (W-TinyLFU)**. Traditional LRU (Least Recently Used) is easily polluted by bursts of one-hit-wonder queries. W-TinyLFU maintains a small admission window using LRU, then evaluates frequency using a compact CountMinSketch frequency table, achieving near-optimal cache hit ratios.
</details>

<details>
<summary>8. How do you implement a multi-level (L1 + L2) cache in Spring Boot?</summary>
By creating a composite cache service or extending `AbstractCacheManager`. When fetching data, the service checks L1 (Caffeine) first. On miss, it checks L2 (Redis) and populates L1. On an L2 miss, it executes the database query and populates both L2 and L1.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure @Cacheable with SpEL key</summary>
Write a service method `UserDto findUser(String username)` cached under name `"users"` using SpEL key `username.toLowerCase()`.
</details>

<details>
<summary>Exercise 2 (Easy): Configure Caffeine CacheManager</summary>
Configure a `CaffeineCacheManager` with maximum size 1000 and expiration of 10 minutes after write.
</details>

<details>
<summary>Exercise 3 (Medium): Implement complete CRUD caching operations</summary>
Write a `ProductService` implementing `@Cacheable` on `get()`, `@CachePut` on `update()`, and `@CacheEvict` on `delete()`.
</details>

<details>
<summary>Exercise 4 (Medium): Configure RedisCacheManager with JSON serializer</summary>
Configure a `RedisCacheManager` using `GenericJackson2JsonRedisSerializer` and custom TTLs for two specific caches.
</details>

<details>
<summary>Exercise 5 (Hard): Implement Cache Stampede protection using Redisson distributed lock</summary>
Write a method that intercepts cache misses, acquires a Redis distributed lock, re-checks the cache, queries the database, and releases the lock.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test testing cache hits and evictions with Testcontainers Redis</summary>
Write a `@SpringBootTest` using Testcontainers Redis that verifies that a second call to a cached method does not increment the database repository call count.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                       Spring Caching Abstraction Summary                        |
+---------------------------------------------------------------------------------+
| Enable:             @EnableCaching                                              |
| Annotations:        @Cacheable(v, k) -> Check cache; populate if absent         |
|                     @CachePut(v, k)  -> Always execute method and update cache  |
|                     @CacheEvict(v, k)-> Invalidate entry or allEntries = true   |
| SpEL Conditions:    condition = "#id > 0" (Pre-execution)                       |
|                     unless = "#result == null" (Post-execution)                 |
| In-Memory (L1):     com.github.ben-manes.caffeine:caffeine (W-TinyLFU)          |
| Distributed (L2):   spring-boot-starter-data-redis + Jackson JSON serializer    |
| Invariant:          Never call cached methods internally within same class      |
| Danger:             Always set TTLs in Redis; protect against Cache Stampedes   |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring Cache Abstraction Documentation](https://docs.spring.io/spring-framework/reference/integration/cache.html)
- [Caffeine Cache GitHub & Benchmarks](https://github.com/ben-manes/caffeine)
- [Spring Data Redis Cache Documentation](https://docs.spring.io/spring-data/redis/reference/redis/redis-cache.html)
