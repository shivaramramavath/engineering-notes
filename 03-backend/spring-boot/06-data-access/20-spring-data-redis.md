# Spring Data Redis: Data Structures, Serializers, Lua Scripts & Distributed Locks

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate / Advanced |
| **Prerequisites** | Redis architectural fundamentals (`03-backend/nodejs/07-databases/redis`), Caching strategies |
| **Estimated Reading Time** | 31 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Spring Data Redis 3.4.x, Lettuce 6.4.x, Redisson 3.37+, Redis 7.4 / Valkey |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Production-Ready RedisTemplate Configuration with JSON Serializers](#51-production-ready-redistemplate-configuration-with-json-serializers)
   - [5.2 Redis Data Structures in Action (Strings, Hashes, ZSets)](#52-redis-data-structures-in-action-strings-hashes-zsets)
   - [5.3 Atomic Rate Limiter Implementation with Redis Lua Scripts](#53-atomic-rate-limiter-implementation-with-redis-lua-scripts)
   - [5.4 Anti-Pattern: JdkSerializationRedisSerializer (Security & Compatibility Trap)](#54-anti-pattern-jdkserializationredisserializer-security--compatibility-trap)
   - [5.5 Distributed Locking with Redisson (Watchdog & Deadlock Prevention)](#55-distributed-locking-with-redisson-watchdog--deadlock-prevention)
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

### Problem Statement
Modern microservices face massive concurrency challenges: handling real-time leaderboard calculations, tracking volatile user sessions, distributing shared counters across clusters, implementing distributed locks to prevent double-spending, and caching hot relational database results. Disk-backed databases cannot handle hundreds of thousands of read/write operations per second with sub-millisecond latency. An in-memory, single-threaded, multiplexed data store like Redis (or Valkey) is essential.

### Historical Evolution
Spring Data Redis provides a comprehensive abstraction over low-level Redis drivers (historically Jedis, now predominantly **Lettuce** for non-blocking Netty-based connections). It provides `RedisTemplate`, repository abstractions (`@RedisHash`), Spring `@Cacheable` integration via `RedisCacheManager`, and reactive Redis support via `ReactiveRedisTemplate`. For distributed coordination (locks, semaphores, queues), **Redisson** complements Spring Data by providing robust distributed primitives.

### Real-World Relevance
Redis serves as the operational backbone for caching, rate limiting, session replication, distributed locks, pub/sub messaging, and real-time streaming in enterprise cloud architectures. Properly configuring serializers, TTLs, and atomic Lua scripts prevents production outages such as cache stampedes, serialization vulnerabilities, and distributed deadlocks.

---

## 2. Core Concepts

### RedisTemplate vs StringRedisTemplate
- **`StringRedisTemplate`**: Pre-configured with `StringRedisSerializer` for both keys and values. Ideal for pure string operations, JSON strings, and atomic counters.
- **`RedisTemplate<K, V>`**: Generic abstraction requiring explicit serializer configuration. By default, it uses Java's native `JdkSerializationRedisSerializer` (which creates unreadable binary payloads and introduces critical security risks).

### Redis Data Structures
1. **Strings (`opsForValue()`)**: Caching objects, counters (`increment()`), bitfield operations.
2. **Hashes (`opsForHash()`)**: Storing structured objects with individual field access (`hget`, `hset`).
3. **Lists (`opsForList()`)**: FIFO queues, timeline feeds (`lpush`, `rpop`, `lrange`).
4. **Sets (`opsForSet()`)**: Unique collections, set algebra (unions, intersections, differences).
5. **Sorted Sets / ZSets (`opsForZSet()`)**: Real-time leaderboards, priority queues, time-window rate limiters.

### Lua Scripting
Redis executes Lua scripts **atomically** in a single execution step. During script execution, no other Redis command can run, preventing race conditions (e.g., check-then-act balance deductions or sliding-window rate limits).

### Distributed Locking with Redisson
A distributed lock ensures that only one application node executes a critical section across a distributed cluster. Redisson's lock implementation provides an automatic background **watchdog timer** that periodically renews the lock lease as long as the holding thread is alive, preventing premature lock release during long-running tasks.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         Spring Data Redis Architecture                             |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Application Code                                                                  |
|       |                                                                            |
|       v                                                                            |
|  RedisTemplate<String, Object>                                                     |
|       |                                                                            |
|       |-- Key Serializer: StringRedisSerializer                                    |
|       |   (Converts "user:101" to raw UTF-8 bytes)                                 |
|       |                                                                            |
|       |-- Value Serializer: GenericJackson2JsonRedisSerializer                     |
|       |   (Converts User record to JSON bytes: '{"@class":..., "name":"Alice"}')   |
|       v                                                                            |
|  Lettuce Connection / Channel (Netty Non-Blocking EventLoop)                       |
|       |                                                                            |
|       v (RESP Protocol over TCP)                                                   |
|  Redis Server (Single-Threaded Command Dispatcher)                                 |
|  - Executes SET, HSET, ZADD in sub-millisecond memory store                        |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Maven Dependencies
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-data-redis</artifactId>
</dependency>
<dependency>
    <groupId>org.redisson</groupId>
    <artifactId>redisson-spring-boot-starter</artifactId>
    <version>3.37.0</version>
</dependency>
```

---

## 5. Practical Examples

### 5.1 Production-Ready RedisTemplate Configuration with JSON Serializers
```java
package com.example.redis.config;

import com.fasterxml.jackson.annotation.JsonTypeInfo;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.jsontype.impl.LaissezFaireSubTypeValidator;
import com.fasterxml.jackson.datatype.jsr310.JavaTimeModule;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.data.redis.connection.RedisConnectionFactory;
import org.springframework.data.redis.core.RedisTemplate;
import org.springframework.data.redis.serializer.GenericJackson2JsonRedisSerializer;
import org.springframework.data.redis.serializer.StringRedisSerializer;

@Configuration
public class RedisConfig {

    @Bean
    public RedisTemplate<String, Object> redisTemplate(RedisConnectionFactory connectionFactory) {
        RedisTemplate<String, Object> template = new RedisTemplate<>();
        template.setConnectionFactory(connectionFactory);

        // Keys always stored as readable UTF-8 strings
        StringRedisSerializer stringSerializer = new StringRedisSerializer();
        template.setKeySerializer(stringSerializer);
        template.setHashKeySerializer(stringSerializer);

        // Values serialized as human-readable JSON with polymorphic type information
        ObjectMapper objectMapper = new ObjectMapper();
        objectMapper.registerModule(new JavaTimeModule());
        objectMapper.activateDefaultTyping(
            LaissezFaireSubTypeValidator.instance,
            ObjectMapper.DefaultTyping.NON_FINAL,
            JsonTypeInfo.As.PROPERTY
        );
        GenericJackson2JsonRedisSerializer jsonSerializer = 
            new GenericJackson2JsonRedisSerializer(objectMapper);

        template.setValueSerializer(jsonSerializer);
        template.setHashValueSerializer(jsonSerializer);
        template.afterPropertiesSet();

        return template;
    }
}
```

### 5.2 Redis Data Structures in Action (Strings, Hashes, ZSets)
```java
package com.example.redis.service;

import org.springframework.data.redis.core.RedisTemplate;
import org.springframework.data.redis.core.ZSetOperations;
import org.springframework.stereotype.Service;

import java.time.Duration;
import java.util.Set;

@Service
public class RedisDataStructuresService {

    private final RedisTemplate<String, Object> redisTemplate;

    public RedisDataStructuresService(RedisTemplate<String, Object> redisTemplate) {
        this.redisTemplate = redisTemplate;
    }

    public record UserProfile(String id, String name, String tier) {}

    // 1. Strings with TTL
    public void cacheUserSession(String token, UserProfile profile) {
        redisTemplate.opsForValue().set("session:" + token, profile, Duration.ofMinutes(30));
    }

    // 2. Hashes: Store fields individually
    public void updateUserSetting(String userId, String settingKey, String value) {
        redisTemplate.opsForHash().put("user:settings:" + userId, settingKey, value);
    }

    // 3. Sorted Sets: Real-Time Gaming Leaderboard
    public void submitGameScore(String leaderboardKey, String playerId, double score) {
        redisTemplate.opsForZSet().add(leaderboardKey, playerId, score);
    }

    public Set<ZSetOperations.TypedTuple<Object>> getTopPlayers(String leaderboardKey, int count) {
        // Fetch top N players with highest scores descending
        return redisTemplate.opsForZSet().reverseRangeWithScores(leaderboardKey, 0, count - 1);
    }
}
```

### 5.3 Atomic Rate Limiter Implementation with Redis Lua Scripts
```java
package com.example.redis.limiter;

import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.data.redis.core.script.DefaultRedisScript;
import org.springframework.stereotype.Service;

import java.util.Collections;

@Service
public class RedisRateLimiter {

    private final StringRedisTemplate stringRedisTemplate;
    private final DefaultRedisScript<Long> rateLimitScript;

    public RedisRateLimiter(StringRedisTemplate stringRedisTemplate) {
        this.stringRedisTemplate = stringRedisTemplate;
        
        // Lua script: Increments key; if first request, sets TTL. Returns current count.
        String scriptSource = """
            local current = redis.call('INCR', KEYS[1])
            if current == 1 then
                redis.call('EXPIRE', KEYS[1], ARGV[1])
            end
            return current
            """;
        this.rateLimitScript = new DefaultRedisScript<>(scriptSource, Long.class);
    }

    /**
     * Checks if an IP or API key has exceeded allowable requests within the time window.
     */
    public boolean isAllowed(String clientIdentifier, int maxRequests, int windowSeconds) {
        String key = "rate:limiter:" + clientIdentifier;
        Long currentRequests = stringRedisTemplate.execute(
            rateLimitScript,
            Collections.singletonList(key),
            String.valueOf(windowSeconds)
        );

        return currentRequests != null && currentRequests <= maxRequests;
    }
}
```

### 5.4 Anti-Pattern: JdkSerializationRedisSerializer (Security & Compatibility Trap)
```java
// =========================================================================
// ANTI-PATTERN: Default JdkSerializationRedisSerializer
// =========================================================================
@Configuration
public class BadRedisConfig {
    @Bean
    public RedisTemplate<Object, Object> badTemplate(RedisConnectionFactory factory) {
        RedisTemplate<Object, Object> template = new RedisTemplate<>();
        template.setConnectionFactory(factory);
        // CRITICAL FLAW: Uses JdkSerializationRedisSerializer by default!
        // 1. Payloads in redis-cli appear as unreadable binary garbage: "\xac\xed\x00\x05t\x00..."
        // 2. Class refactoring (renaming fields/packages) causes InvalidClassException deserialization failure!
        // 3. Vulnerable to Remote Code Execution (RCE) via Java deserialization gadgets!
        return template;
    }
}

// =========================================================================
// PRODUCTION FIX: Explicit String and JSON Serializers
// =========================================================================
// Always use StringRedisSerializer for keys and GenericJackson2JsonRedisSerializer for values!
```

### 5.5 Distributed Locking with Redisson (Watchdog & Deadlock Prevention)
```java
package com.example.redis.lock;

import org.redisson.api.RLock;
import org.redisson.api.RedissonClient;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import java.util.concurrent.TimeUnit;

@Service
public class DistributedOrderProcessingService {

    private static final Logger log = LoggerFactory.getLogger(DistributedOrderProcessingService.class);
    private final RedissonClient redissonClient;

    public DistributedOrderProcessingService(RedissonClient redissonClient) {
        this.redissonClient = redissonClient;
    }

    public void processOrderWithDistributedLock(String orderId) {
        String lockKey = "lock:order:" + orderId;
        RLock lock = redissonClient.getLock(lockKey);

        try {
            // Attempt to acquire lock: wait up to 5 seconds, hold for 10 seconds (lease time)
            // If leaseTime is omitted (-1), Redisson's Watchdog automatically renews the lock!
            boolean acquired = lock.tryLock(5, 10, TimeUnit.SECONDS);

            if (!acquired) {
                log.warn("Could not acquire lock for order {}. Another node is processing it.", orderId);
                throw new IllegalStateException("Order processing in progress; please retry.");
            }

            log.info("Acquired distributed lock for order: {}. Executing critical section...", orderId);
            executeCriticalOrderSettlement(orderId);

        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new IllegalStateException("Interrupted while acquiring lock", e);
        } finally {
            // Ensure lock is unlocked ONLY if currently held by this thread!
            if (lock.isHeldByCurrentThread()) {
                lock.unlock();
                log.info("Released distributed lock for order: {}", orderId);
            }
        }
    }

    private void executeCriticalOrderSettlement(String orderId) {
        // Business logic protected across entire distributed cluster
    }
}
```

---

## 6. Best Practices

1. **Always Configure Serializers Explicitly**: Never rely on default JDK serialization. Use `StringRedisSerializer` for keys and `GenericJackson2JsonRedisSerializer` for values.
2. **Always Set Time-To-Live (TTL) on Cached Keys**: Unbounded keys without TTLs slowly exhaust Redis memory, eventually triggering key eviction policies (`allkeys-lru` or OOM errors).
3. **Use Lua Scripts for Multi-Step Atomic Operations**: Replace "check-then-act" sequences with atomic Lua scripts to prevent concurrent race conditions.
4. **Use Redisson for Distributed Locks**: Avoid implementing manual distributed locks with `SETNX` in RedisTemplate; edge cases like process crashes and clock drift are handled natively by Redisson.
5. **Always Verify `isHeldByCurrentThread()` Before Unlocking**: Attempting to call `lock.unlock()` when the lease has expired or the lock is owned by another thread throws `IllegalMonitorStateException`.
6. **Use Key Namespaces with Colons**: Structure Redis keys hierarchically (e.g. `ecommerce:users:sessions:101`) to support clean filtering and logical grouping in monitoring tools.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using Default `RedisTemplate` Without Serializers** | Generates unreadable binary bytes with Java serialization headers, vulnerable to deserialization exploits. | Configure `StringRedisSerializer` and Jackson JSON serializer. |
| **Executing `KEYS *` in Production** | `KEYS` is an $O(N)$ blocking command; on a database with millions of keys, it freezes Redis for seconds. | Use `SCAN` via cursor iteration (`redisTemplate.scan()`). |
| **Omitting TTL on Cached Objects** | Memory continuously grows until Redis exhausts available RAM. | Always provide a `Duration` with `opsForValue().set(k, v, ttl)`. |
| **Manual `SETNX` Locks Without Watchdog** | If a node crashes or a task exceeds the lock timeout, locks either leak permanently or release prematurely. | Use Redisson's `RLock` with automatic watchdog renewal. |
| **Calling `lock.unlock()` Without Ownership Check** | Throws `IllegalMonitorStateException` if another node took over the lock after expiration. | Wrap unlock in `if (lock.isHeldByCurrentThread()) lock.unlock();`. |
| **Huge Bounded Collections (e.g. 500k items in one List)** | Large single-key payloads degrade Redis network throughput and cause memory fragmentation. | Partition lists or use Redis Streams / PostgreSQL. |

---

## 8. Performance & Security Notes

### Performance
- **Connection Multiplexing via Lettuce**: Lettuce shares a single non-blocking Netty TCP connection across all threads for standard commands, reducing connection overhead compared to Jedis thread pooling.
- **Pipelining**: Batch multiple commands together using `redisTemplate.executePipelined(...)` to execute dozens of operations in a single network round-trip.

### Security
- **Authentication & TLS**: Always enable `requirepass` or Redis ACLs, and enforce TLS in transit (`rediss://`) when connecting across cloud infrastructure.
- **Disable Dangerous Redis Commands**: Disable or rename destructive commands (`FLUSHALL`, `FLUSHDB`, `CONFIG`, `KEYS`, `SHUTDOWN`) in `redis.conf`.

---

## 9. Comparison / When to Use What

| Feature | StringRedisTemplate | RedisTemplate<K, V> | RedissonClient |
| :--- | :--- | :--- | :--- |
| **Key/Value Type** | Strict Strings | Configurable Objects | Native Java Objects |
| **Serialization** | String (UTF-8) | Jackson JSON / Binary | Marshalling / Jackson |
| **Distributed Primitives** | None | Basic | **Full (RLock, RSemaphore, RCountDownLatch)** |
| **Best Used For** | Counters, tokens, JSON strings | Domain DTO caching | Distributed coordination, locking |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is Lettuce preferred over Jedis as the default Redis client in Spring Boot?</strong></summary>

Jedis uses a blocking, thread-per-connection architecture requiring a large Apache Commons Pool. Lettuce is built on Netty and is thread-safe and non-blocking, allowing multiple concurrent application threads to share a single TCP connection through command multiplexing. This minimizes connection overhead and supports reactive programming via Project Reactor.
</details>

<details>
<summary><strong>2. Why is using <code>KEYS *</code> dangerous in a production Redis cluster?</strong></summary>

Redis is single-threaded. The `KEYS` command performs a full $O(N)$ synchronous scan of the entire keyspace. On a database with millions of keys, running `KEYS *` will block the Redis event loop for seconds, stalling all other incoming reads and writes across the entire system. `SCAN` should be used instead.
</details>

<details>
<summary><strong>3. How does Redisson's Watchdog mechanism prevent distributed lock expiration issues?</strong></summary>

When a thread acquires an `RLock` without specifying an explicit `leaseTime` (defaults to -1), Redisson starts a background timer (the Watchdog). Every 10 seconds (one-third of the default 30-second lock watchdog timeout), the Watchdog extends the lock expiration by another 30 seconds. If the application server crashes, the Watchdog dies, allowing the lock to naturally expire in 30 seconds and preventing permanent deadlocks.
</details>

<details>
<summary><strong>4. Why should you avoid Java's default serialization in Redis?</strong></summary>

1. **Readability**: Binary payloads cannot be inspected or debugged in `redis-cli`.
2. **Fragility**: Changing a class's package, serialVersionUID, or attributes causes `InvalidClassException` upon deserialization.
3. **Security**: Java deserialization is notoriously vulnerable to Remote Code Execution (RCE) attacks via gadget chains.
</details>

<details>
<summary><strong>5. What is the difference between a Redis Set and a Sorted Set (ZSet)?</strong></summary>

- **Set**: An unordered collection of unique strings with $O(1)$ lookup, insertion, and deletion (`SADD`, `SISMEMBER`).
- **Sorted Set (ZSet)**: Every string member is paired with a numerical floating-point `score`. Members are ordered by score, allowing fast range queries by score or rank ($O(\log N)$ via skip lists and hash tables), making it ideal for leaderboards.
</details>

<details>
<summary><strong>6. How does Lua scripting guarantee atomicity in Redis?</strong></summary>

Redis executes a Lua script as a single atomic transactional unit. The single-threaded Redis engine halts all other command processing until the Lua script completes. This prevents race conditions in multi-step check-and-set operations without requiring complex client-side transactions.
</details>

<details>
<summary><strong>7. How can you prevent a "Cache Stampede" (Thundering Herd) when a key expires?</strong></summary>

1. **Mutex / Distributed Lock**: When the cache misses, acquire a distributed lock; only the first thread queries the database and populates the cache while other threads wait.
2. **Probabilistic Early Expiration (XFetch)**: Re-calculate and refresh the cache in the background slightly before the key expires based on read frequency and computation time.
</details>

<details>
<summary><strong>8. What is Redis Pipelining and when should you use it?</strong></summary>

Normally, Redis follows a request/response model where each command requires a network round-trip. Pipelining allows the client to send dozens of commands without waiting for intermediate responses, and then reads all replies in a single operation. This dramatically increases throughput for batch data loading.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): String Template Key Increment</strong></summary>

**Task**: Implement a method that increments an API key usage counter in Redis and sets a 1-day expiration if it is the first call.

```java
public long incrementUsage(String apiKey) {
    String key = "usage:" + apiKey;
    Long count = stringRedisTemplate.opsForValue().increment(key);
    if (count != null && count == 1L) {
        stringRedisTemplate.expire(key, Duration.ofDays(1));
    }
    return count != null ? count : 0L;
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Real-Time Leaderboard with ZSets</strong></summary>

**Task**: Add a score to a player in a leaderboard and retrieve the player's 0-based rank.

```java
public Long recordScoreAndGetRank(String gameId, String playerId, double score) {
    String key = "leaderboard:" + gameId;
    redisTemplate.opsForZSet().add(key, playerId, score);
    return redisTemplate.opsForZSet().reverseRank(key, playerId);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Spring CacheManager Redis TTL Configuration</strong></summary>

**Task**: Configure `RedisCacheManager` with default 10-minute TTL, and a specific 1-hour TTL for the `"products"` cache.

```java
@Bean
public RedisCacheManager cacheManager(RedisConnectionFactory connectionFactory) {
    RedisCacheConfiguration defaultConfig = RedisCacheConfiguration.defaultCacheConfig()
        .entryTtl(Duration.ofMinutes(10));

    Map<String, RedisCacheConfiguration> regionConfigs = new HashMap<>();
    regionConfigs.put("products", RedisCacheConfiguration.defaultCacheConfig().entryTtl(Duration.ofHours(1)));

    return RedisCacheManager.builder(connectionFactory)
        .cacheDefaults(defaultConfig)
        .withInitialCacheConfigurations(regionConfigs)
        .build();
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Redis Pub/Sub Topic Configuration</strong></summary>

**Task**: Create a `MessageListenerAdapter` and `RedisMessageListenerContainer` listening to channel `"order-events"`.

```java
@Configuration
public class RedisPubSubConfig {
    @Bean
    public RedisMessageListenerContainer container(RedisConnectionFactory factory, MessageListenerAdapter adapter) {
        RedisMessageListenerContainer container = new RedisMessageListenerContainer();
        container.setConnectionFactory(factory);
        container.addMessageListener(adapter, new ChannelTopic("order-events"));
        return container;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Distributed Semaphore with Redisson</strong></summary>

**Task**: Implement a concurrency limiter using Redisson's `RSemaphore` that limits concurrent calls to a third-party API to 5 across the entire cluster.

```java
public void callThrottledApi(Runnable task) throws InterruptedException {
    RSemaphore semaphore = redissonClient.getSemaphore("throttle:external-api");
    semaphore.trySetPermits(5);

    semaphore.acquire();
    try {
        task.run();
    } finally {
        semaphore.release();
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Atomic Balance Deduction Lua Script</strong></summary>

**Task**: Write an atomic Lua script in Java that checks if a user's balance in Redis is $\ge$ amount, deducts the amount, and returns true; otherwise returns false.

```java
public boolean deductBalance(String userId, double amount) {
    String script = """
        local bal = tonumber(redis.call('GET', KEYS[1]) or '0')
        local amt = tonumber(ARGV[1])
        if bal >= amt then
            redis.call('DECRBY', KEYS[1], amt)
            return 1
        else
            return 0
        end
        """;
    DefaultRedisScript<Long> redisScript = new DefaultRedisScript<>(script, Long.class);
    Long result = stringRedisTemplate.execute(
        redisScript, 
        Collections.singletonList("wallet:" + userId), 
        String.valueOf(amount)
    );
    return result != null && result == 1L;
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Serializers:  Use StringRedisSerializer for keys, GenericJackson2Json for values      |
| Data Types:   Strings (Value), Hashes (Hash), Lists (List), Sets (Set), ZSets (ZSet)  |
| Rule 1:       NEVER execute KEYS * in production; use scan() cursor iteration         |
| Rule 2:       Always assign explicit TTLs (entryTtl) to prevent memory exhaustion     |
| Locks:        Use Redisson RLock with watchdog renewal; check isHeldByCurrentThread() |
| Atomicity:    Use Lua scripts (DefaultRedisScript) for check-then-act operations      |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Data Redis Official Reference Documentation](https://docs.spring.io/spring-data/redis/reference/)
- [Redisson Official GitHub & Distributed Objects Guide](https://github.com/redisson/redisson/wiki/Table-of-Content)
- [Redis Command Reference Documentation](https://redis.io/commands/)
- [Martin Kleppmann: How to Do Distributed Locking (Redlock Analysis)](https://martin.kleppmann.com/2016/02/08/how-to-do-distributed-locking.html)
