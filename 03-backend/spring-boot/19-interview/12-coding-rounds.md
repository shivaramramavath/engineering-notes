# 12: Live Coding Rounds & Implementation Challenges

| Metadata Field | Value |
|---|---|
| **Topic** | Live Coding Interviews, Clean Code, Concurrency & Data Structures |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Level** | Senior / Lead / Staff Backend Engineer |
| **Key Challenges** | Token Bucket Limiter, Custom JWT Filter, LRU Cache, Outbox Publisher |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Practical Usage](#4-syntax--practical-usage)
5. [5 Practical Examples](#5-5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison / Decision Matrix](#9-comparison--decision-matrix)
10. [Real-World Interview Q&A](#10-real-world-interview-qa)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference Cheatsheet](#12-summary--quick-reference-cheatsheet)
13. [Further Reading & Official Resources](#13-further-reading--official-resources)

---

## 1. What & Why

In senior live coding rounds (45–60 minutes), interviewers evaluate a candidate's ability to translate abstract requirements into production-quality, compilable, and thread-safe Java code under time pressure. Candidates are judged on:
- **Clean Architecture & SOLID principles**
- **Thread Safety & Concurrency Primitives** (e.g. `AtomicInteger`, `ReentrantLock`, `ConcurrentHashMap`)
- **Proper Idiomatic Spring Boot usage** (Constructor injection, records, DTO separation)
- **Comprehensive Unit Testing** with JUnit 5 and AssertJ.

This guide provides fully implemented solutions to the 5 most common live coding challenges asked at top tech companies.

---

## 2. Core Concepts

### 2.1 Concurrency & Mutex Primitives
- `AtomicLong` / `AtomicReference`: Lock-free atomic state mutations using CPU Compare-And-Swap (CAS).
- `ReentrantLock`: Fine-grained mutual exclusion supporting timeout acquisition (`tryLock()`) without carrier thread pinning in Java 21.
- `ConcurrentHashMap.compute()`: Atomic read-modify-write operations on map keys.

### 2.2 Testing & Edge Cases
Live coding interviewers expect candidates to write tests addressing edge cases:
- Null inputs and malformed headers.
- Concurrent burst traffic (using `CountDownLatch` or `ExecutorService`).
- Expired tokens and boundary time arithmetic.

---

## 3. Internals & Architecture

```
 ┌────────────────────────────────────────────────────────┐
 │           Live Coding Problem Evaluation Canvas        │
 ├────────────────────────────────────────────────────────┤
 │ 1. Understand Problem & Invariants (Ask Questions)     │
 │ 2. Define Clean Domain Models (Records / Immutability) │
 │ 3. Implement Thread-Safe Core Algorithm (Atomic / Lock)│
 │ 4. Expose Clean Spring Web / Service API               │
 │ 5. Write JUnit 5 Concurrency Test Suite                │
 └────────────────────────────────────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Live Coding Test Setup Pattern
```java
@Test
void shouldHandleConcurrentAccess() throws Exception {
    int threads = 20;
    var executor = Executors.newFixedThreadPool(threads);
    var latch = new CountDownLatch(threads);
    // Execute concurrently...
}
```

---

## 5. 5 Practical Examples

### Challenge 1: Thread-Safe In-Memory Token Bucket Rate Limiter

```java
package com.example.coding.ratelimit;

import java.time.Instant;
import java.util.concurrent.ConcurrentHashMap;

public class InMemoryTokenBucketRateLimiter {

    private final int capacity;
    private final double refillRatePerSecond;
    private final ConcurrentHashMap<String, TokenBucket> buckets = new ConcurrentHashMap<>();

    public InMemoryTokenBucketRateLimiter(int capacity, double refillRatePerSecond) {
        this.capacity = capacity;
        this.refillRatePerSecond = refillRatePerSecond;
    }

    public boolean tryConsume(String key) {
        TokenBucket bucket = buckets.computeIfAbsent(key, k -> new TokenBucket(capacity, refillRatePerSecond));
        return bucket.tryConsume();
    }

    private static class TokenBucket {
        private final int capacity;
        private final double refillRate;
        private double tokens;
        private long lastRefillTimestamp;

        public TokenBucket(int capacity, double refillRate) {
            this.capacity = capacity;
            this.refillRate = refillRate;
            this.tokens = capacity;
            this.lastRefillTimestamp = System.currentTimeMillis();
        }

        public synchronized boolean tryConsume() {
            refill();
            if (tokens >= 1.0) {
                tokens -= 1.0;
                return true;
            }
            return false;
        }

        private void refill() {
            long now = System.currentTimeMillis();
            double secondsPassed = (now - lastRefillTimestamp) / 1000.0;
            double tokensToAdd = secondsPassed * refillRate;
            tokens = Math.min(capacity, tokens + tokensToAdd);
            lastRefillTimestamp = now;
        }
    }
}
```

### Challenge 2: Custom JWT Authentication Servlet Filter

```java
package com.example.coding.security;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpStatus;
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;
import java.util.List;

public class CustomJwtTokenFilter extends OncePerRequestFilter {

    private final SimpleJwtParser tokenParser;

    public CustomJwtTokenFilter(SimpleJwtParser tokenParser) {
        this.tokenParser = tokenParser;
    }

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {

        String header = request.getHeader(HttpHeaders.AUTHORIZATION);
        if (header == null || !header.startsWith("Bearer ")) {
            filterChain.doFilter(request, response);
            return;
        }

        String token = header.substring(7);
        try {
            var claims = tokenParser.parseToken(token);
            var authorities = List.of(new SimpleGrantedAuthority("ROLE_" + claims.role().toUpperCase()));
            var auth = new UsernamePasswordAuthenticationToken(claims.username(), null, authorities);

            SecurityContextHolder.getContext().setAuthentication(auth);
            filterChain.doFilter(request, response);

        } catch (Exception ex) {
            SecurityContextHolder.clearContext();
            response.setStatus(HttpStatus.UNAUTHORIZED.value());
            response.setContentType("application/json");
            response.getWriter().write("{\"error\":\"Invalid or expired token\"}");
        }
    }

    public record TokenClaims(String username, String role) {}
    public interface SimpleJwtParser {
        TokenClaims parseToken(String token) throws Exception;
    }
}
```

### Challenge 3: Transactional Outbox Event Publisher

```java
package com.example.coding.outbox;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;
import java.util.UUID;

@Service
public class OutboxEventPublisherService {

    private static final Logger log = LoggerFactory.getLogger(OutboxEventPublisherService.class);
    private final JdbcClient jdbcClient;
    private final KafkaTemplate<String, String> kafkaTemplate;

    public OutboxEventPublisherService(JdbcClient jdbcClient, KafkaTemplate<String, String> kafkaTemplate) {
        this.jdbcClient = jdbcClient;
        this.kafkaTemplate = kafkaTemplate;
    }

    public record OutboxRecord(UUID id, String topic, String eventKey, String payload) {}

    @Scheduled(fixedDelay = 1000) // Poll every 1 second
    @Transactional
    public void publishPendingEvents() {
        // Fetch up to 50 uncommitted outbox events
        List<OutboxRecord> records = jdbcClient.sql("""
                SELECT id, topic, event_key, payload FROM outbox_events 
                WHERE status = 'PENDING' 
                ORDER BY created_at ASC LIMIT 50 
                FOR UPDATE SKIP LOCKED
                """)
                .query(OutboxRecord.class)
                .list();

        for (OutboxRecord rec : records) {
            log.info("Publishing outbox event {} to Kafka topic {}", rec.id(), rec.topic());
            kafkaTemplate.send(rec.topic(), rec.eventKey(), rec.payload());

            jdbcClient.sql("UPDATE outbox_events SET status = 'PUBLISHED' WHERE id = ?")
                    .param(rec.id())
                    .update();
        }
    }
}
```

### Challenge 4: Thread-Safe LRU Cache with Max Capacity

```java
package com.example.coding.cache;

import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.locks.ReentrantReadWriteLock;

public class ThreadSafeLruCache<K, V> {

    private final int capacity;
    private final LinkedHashMap<K, V> map;
    private final ReentrantReadWriteLock rwLock = new ReentrantReadWriteLock();

    public ThreadSafeLruCache(int capacity) {
        this.capacity = capacity;
        // accessOrder = true enforces LRU order
        this.map = new LinkedHashMap<>(capacity, 0.75f, true) {
            @Override
            protected boolean removeEldestEntry(Map.Entry<K, V> eldest) {
                return size() > ThreadSafeLruCache.this.capacity;
            }
        };
    }

    public V get(K key) {
        rwLock.writeLock().lock(); // accessOrder mutation requires writeLock
        try {
            return map.get(key);
        } finally {
            rwLock.writeLock().unlock();
        }
    }

    public void put(K key, V value) {
        rwLock.writeLock().lock();
        try {
            map.put(key, value);
        } finally {
            rwLock.writeLock().unlock();
        }
    }

    public int size() {
        rwLock.readLock().lock();
        try {
            return map.size();
        } finally {
            rwLock.readLock().unlock();
        }
    }
}
```

### Challenge 5: Multi-Threaded Live Coding Unit Test

```java
package com.example.coding.ratelimit;

import org.junit.jupiter.api.Test;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;

import static org.assertj.core.api.Assertions.assertThat;

public class TokenBucketTest {

    @Test
    void shouldAllowInitialBurstAndRejectExcess() throws Exception {
        // Bucket: capacity 5, refill 1/sec
        var limiter = new InMemoryTokenBucketRateLimiter(5, 1.0);

        int threads = 10;
        var executor = Executors.newFixedThreadPool(threads);
        var startGate = new CountDownLatch(1);
        var doneGate = new CountDownLatch(threads);

        var allowedCount = new AtomicInteger(0);
        var rejectedCount = new AtomicInteger(0);

        for (int i = 0; i < threads; i++) {
            executor.submit(() -> {
                try {
                    startGate.await();
                    if (limiter.tryConsume("user-1")) {
                        allowedCount.incrementAndGet();
                    } else {
                        rejectedCount.incrementAndGet();
                    }
                } catch (Exception ignored) {
                } finally {
                    doneGate.countDown();
                }
            });
        }

        startGate.countDown(); // Fire simultaneously
        doneGate.await();

        // Exactly 5 requests must succeed, and 5 must be rejected
        assertThat(allowedCount.get()).isEqualTo(5);
        assertThat(rejectedCount.get()).isEqualTo(5);
    }
}
```

---

## 6. Best Practices

- **Communicate Thought Process Out Loud**: Walk the interviewer through your approach, edge cases, and time/space complexity before writing code.
- **Use Modern Java 21 Features**: Leverage `record`, pattern matching, and `var` where appropriate to demonstrate modern language mastery.
- **Favor Immutability**: Make fields `final` wherever possible.
- **Validate Inputs Defensively**: Throw `IllegalArgumentException` on null or invalid inputs.
- **Write Compilable Tests Early**: End the live coding round by writing a fast JUnit test proving your code works.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Ignoring Thread Safety** | Using non-thread-safe `HashMap` in concurrent challenges leads to data corruption or infinite loops. | Use `ConcurrentHashMap` or synchronization locks. |
| **Silent Swallowing of InterruptedException** | Catching `InterruptedException` without restoring interrupt status breaks thread pooling. | Call `Thread.currentThread().interrupt()`. |
| **Jumping to Code without Clarifying Requirements** | Implementing the wrong requirements wastes 25 minutes of interview time. | Spend the first 5 minutes clarifying edge cases, scale, and constraints. |
| **Using `new Thread()` Directly** | Spawning raw unmanaged threads is an anti-pattern. | Use `ExecutorService` or `Executors.newVirtualThreadPerTaskExecutor()`. |
| **Missing `FOR UPDATE SKIP LOCKED` in Outbox** | Multiple worker pods lock the same outbox rows, causing lock contention and duplicate events. | Use `FOR UPDATE SKIP LOCKED` in outbox queries. |
| **No Edge Case Testing** | Interviewer spots that passing `0` or negative values crashes the algorithm. | Write tests for 0, negative, and null inputs. |

---

## 8. Performance & Security Considerations

### 8.1 Read/Write Lock Granularity
When implementing an LRU cache with `LinkedHashMap(capacity, 0.75f, true)`, calling `get()` mutates the internal doubly-linked list ordering. Therefore, `get()` requires a **write lock** rather than a read lock.

### 8.2 Safe Database Polling
Always use `FOR UPDATE SKIP LOCKED` when polling database queue tables across multiple worker pods to prevent row locking contention.

---

## 9. Comparison / Decision Matrix

| Challenge Topic | Core Data Structure | Time Complexity | Concurrency Mechanism |
|---|---|---|---|
| **Token Bucket Limiter** | Atomic timestamp + double tokens | $O(1)$ | `synchronized` / CAS |
| **LRU Cache** | `LinkedHashMap` (Hash + Doubly Linked) | $O(1)$ | `ReentrantReadWriteLock` |
| **Outbox Poller** | Relational DB Queue Table | $O(N)$ batch | `FOR UPDATE SKIP LOCKED` |
| **JWT Filter** | OncePerRequestFilter | $O(1)$ | Stateless (ThreadLocal context) |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. Why is `ConcurrentHashMap.computeIfAbsent()` preferred over `if (!map.containsKey()) map.put()`?</strong></summary>

`containsKey()` followed by `put()` is a compound check-then-act operation that is NOT atomic. Two threads could simultaneously determine the key is missing and both call `put()`, overwriting state. `computeIfAbsent()` executes atomically on the bucket level.
</details>

<details>
<summary><strong>2. How does `FOR UPDATE SKIP LOCKED` work in a database job queue?</strong></summary>

It locks a batch of selected rows for the current transaction and automatically skips any rows currently locked by other concurrent worker transactions. This allows multiple workers to drain the queue in parallel without blocking each other.
</details>

<details>
<summary><strong>3. Why does `LinkedHashMap` require a write lock on `get()` when `accessOrder = true`?</strong></summary>

When `accessOrder = true`, calling `get(key)` moves the accessed node to the end of the doubly linked list to track recency. Since `get()` modifies the internal pointers of the data structure, it is a mutating operation that requires exclusive write locking.
</details>

<details>
<summary><strong>4. How do you implement a token bucket without a background refill timer thread?</strong></summary>

Use lazy calculation. When a request arrives, calculate the time elapsed since the last request: $\text{tokensToAdd} = \Delta t \times \text{refillRate}$. Add the tokens up to capacity, update the timestamp, and decrement 1 token. This avoids running idle background threads.
</details>

<details>
<summary><strong>5. What is the difference between `CountDownLatch` and `CyclicBarrier` in concurrency testing?</strong></summary>

A `CountDownLatch` is a one-shot gate where threads wait until a counter reaches zero (cannot be reset). A `CyclicBarrier` allows a set of threads to repeatedly wait for each other at a common barrier point and can be reused after being released.
</details>

<details>
<summary><strong>6. How do you ensure Spring's `OncePerRequestFilter` is executed only once per request?</strong></summary>

It sets a unique request attribute flag (`alreadyFilteredAttributeName`) on the `HttpServletRequest` during its first execution. If the request is forwarded internally or redispatched, the filter detects the attribute and skips re-execution.
</details>

<details>
<summary><strong>7. Why is `Thread.currentThread().interrupt()` necessary when catching `InterruptedException`?</strong></summary>

Catching `InterruptedException` clears the thread's interrupt status flag. If you do not call `Thread.currentThread().interrupt()`, higher-level callers and thread pools will never know that an interrupt was requested, preventing graceful thread shutdown.
</details>

<details>
<summary><strong>8. What is the time complexity of the Token Bucket rate limiter?</strong></summary>

$O(1)$ time complexity and $O(1)$ space complexity per tracked client key.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Implement Thread-Safe Counter with AtomicLong</strong></summary>

**Prompt**: Write a class that tracks request counts per tenant ID using `ConcurrentHashMap<String, AtomicLong>`.

**Solution**:
```java
public class TenantCounter {
    private final ConcurrentHashMap<String, AtomicLong> counts = new ConcurrentHashMap<>();
    public long increment(String tenant) {
        return counts.computeIfAbsent(tenant, t -> new AtomicLong(0)).incrementAndGet();
    }
}
```
</details>

<details>
<summary><strong>Exercise 2: Write Token Bucket Live Coding Test</strong></summary>

**Prompt**: Implement Example 1 and write a JUnit 5 test verifying that 5 requests succeed and the 6th is rejected.

**Solution**:
Inspect Example 5: configure bucket with capacity 5, invoke 6 times, and assert 5 true and 1 false.
</details>

<details>
<summary><strong>Exercise 3: Build Custom JWT Claims Extractor</strong></summary>

**Prompt**: Write a utility extracting `sub` and `roles` claims from a signed JWT string.

**Solution**:
Use Nimbus or JJWT library: `Jwts.parser().verifyWith(key).build().parseSignedClaims(jwt).getPayload()`.
</details>

<details>
<summary><strong>Exercise 4: Implement LRU Cache with Max 3 Items</strong></summary>

**Prompt**: Put keys 1, 2, 3 into `ThreadSafeLruCache(3)`. Read key 1. Put key 4. Verify key 2 was evicted.

**Solution**:
Inspect Example 4: assert `cache.get(2)` returns null, while `cache.get(1)` and `cache.get(4)` exist.
</details>

<details>
<summary><strong>Exercise 5: Implement Outbox Worker with Mockito</strong></summary>

**Prompt**: Write a unit test asserting `OutboxEventPublisherService` publishes records and updates database status to `PUBLISHED`.

**Solution**:
Mock `JdbcClient` and `KafkaTemplate`. Call `publishPendingEvents()`. Verify `kafkaTemplate.send()` was invoked once per row.
</details>

<details>
<summary><strong>Exercise 6: Test Concurrent Token Bucket Refill</strong></summary>

**Prompt**: Consume all 5 tokens. Sleep for 2 seconds (refill rate: 1 token/sec). Assert that exactly 2 new tokens can be consumed.

**Solution**:
Call `tryConsume()` 5 times (all return true). Sleep 2000ms. Call `tryConsume()` twice (both return true). 3rd call returns false.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   LIVE CODING ROUNDS CHEATSHEET                        │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Clarification: First 5 mins to confirm inputs, outputs & edge cases │
│ 2. Thread-Safety: ConcurrentHashMap.compute() for atomic map updates   │
│ 3. Lazy Refill: Calculate tokens on request arrival; avoid timers      │
│ 4. LRU Cache: LinkedHashMap with accessOrder=true (needs write lock)   │
│ 5. Outbox Poller: SELECT ... FOR UPDATE SKIP LOCKED to avoid pod locks │
│ 6. Verification: Always write JUnit 5 test with CountDownLatch         │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Java Concurrency in Practice (Brian Goetz)](https://jcip.net/)
- [Spring Web OncePerRequestFilter API](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/web/filter/OncePerRequestFilter.html)
- [PostgreSQL: The SELECT FOR UPDATE SKIP LOCKED Feature](https://www.2ndquadrant.com/en/blog/what-is-select-skip-locked-in-postgresql-9-5/)
- [Token Bucket Algorithm (Wikipedia)](https://en.wikipedia.org/wiki/Token_bucket)
