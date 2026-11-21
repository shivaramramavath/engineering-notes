# Backend System Design in Java Interview Mastery

> An exhaustive guide to designing production distributed systems, low-latency architectures, caching strategies, rate limiters, distributed ID generators, and high-throughput microservices using modern Java.

| Metadata | Details |
| :--- | :--- |
| **Level** | Staff / Principal / Architect |
| **Prerequisites** | [14-concurrency](../14-concurrency/README.md), [11-io-and-networking](../11-io-and-networking/README.md), [22-production-engineering](../22-production-engineering/README.md) |
| **Time to Read** | 50 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to 25 LTS |

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

### What It Is
Backend system design in Java covers the architectural patterns, data pipelines, synchronization primitives, and infrastructure building blocks needed to build scalable, fault-tolerant, and high-throughput distributed systems using the Java runtime.

### Why It Matters in Interviews
Interviews for Senior, Lead, Staff, and Principal Engineers evaluate a candidate's ability to translate business requirements into resilient distributed systems. Beyond generic high-level boxes and arrows, top technical interviews probe deep into Java implementation details:
- How to size thread pools for CPU-bound vs. I/O-bound workloads.
- When to migrate from reactive programming (WebFlux/Netty) to Virtual Threads (Project Loom).
- How to implement distributed rate limiting with Token Bucket or Sliding Window Log algorithms.
- How to achieve exactly-once semantics when consuming Apache Kafka topics.
- Multi-tier caching architectures balancing local JVM memory (Caffeine) against distributed stores (Redis).

---

## 2. Core Concepts

### Distributed Building Blocks in Java

1. **Distributed Rate Limiting:**
   - **Token Bucket:** Fixed refill rate adding tokens up to capacity; allows controlled bursts.
   - **Leaky Bucket:** Enforces smooth output flow regardless of input bursts.
   - **Sliding Window Counter:** Blends previous window counters with current window metrics to avoid edge-of-window request doubling.

2. **Distributed ID Generation:**
   - **Snowflake Algorithm:** 64-bit monotonically increasing IDs composed of:
     - 1 bit unused (sign bit)
     - 41 bits timestamp (millisecond precision, ~69 years)
     - 10 bits machine / datacenter ID (1,024 nodes)
     - 12 bits sequence number (4,096 IDs per ms per node)

3. **Multi-Tier Caching & Consistency:**
   - **L1 Cache (In-Memory JVM):** Sub-microsecond reads using Caffeine with W-TinyLFU eviction.
   - **L2 Cache (Distributed):** Low-millisecond reads using Redis Cluster.
   - **Cache-Aside Pattern:** Read from cache -> on miss, read DB -> write back to cache.
   - **Cache Stampede Prevention:** Mutex locking or Probabilistic Early Expiration (XFetch algorithm).

4. **Event-Driven Messaging & Sagas:**
   - Outbox Pattern: Persisting business state changes and domain events within the same atomic database transaction to guarantee reliable message publishing.
   - Orchestrated vs. Choreographed Sagas for distributed transactional consistency across microservices.

---

## 3. How It Works Internally

### Virtual Threads vs Platform Threads Architecture

Prior to Java 21, Java threads were 1:1 wrappers around operating system platform threads. In Java 21+ (JEP 444), Virtual Threads decouple application threads from kernel scheduling.

```
+-------------------------------------------------------------------------+
|                         Java Application Layer                          |
|  [Virtual Thread 1]  [Virtual Thread 2]  ...  [Virtual Thread 1,000,000] |
+-------------------------------------------------------------------------+
                                    |
                 Mounted on Non-blocking I/O or Parking
                                    v
+-------------------------------------------------------------------------+
|                  ForkJoinPool Carrier Platform Threads                  |
|          [Carrier Thread 1]                  [Carrier Thread 2]         |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
|                         Operating System Kernel                         |
|                                [CPU Core]                               |
+-------------------------------------------------------------------------+
```

When a Virtual Thread executes a blocking operation (e.g., `Socket.read()`, `Thread.sleep()`, or database I/O via JDBC), HotSpot **unmounts** the virtual thread from its carrier thread and preserves its stack frame in Java heap memory. The underlying carrier thread is immediately free to execute other virtual threads, allowing single JVM processes to handle millions of concurrent connections without thread exhaustion.

---

## 4. Syntax & Usage

### Core Backend Primitives

#### 1. Virtual Thread Executor (Java 21+)
```java
// Lightweight per-task execution for high-throughput I/O services
try (var executor = Executors.newVirtualThreadPerTaskExecutor()) {
    IntStream.range(0, 10_000).forEach(i -> executor.submit(() -> {
        // Blocking HTTP call or database query
        Thread.sleep(Duration.ofMillis(50));
        return "Processed " + i;
    }));
} // Executor auto-waits for all virtual threads to complete
```

#### 2. Thread Pool Sizing Formula (Classic Platform Threads)
For platform thread pools handling blocking I/O:
$$N_{\text{threads}} = N_{\text{CPUs}} \times U_{\text{CPU}} \times \left(1 + \frac{W}{C}\right)$$
Where:
- $N_{\text{CPUs}}$: Number of available CPU cores.
- $U_{\text{CPU}}$: Target CPU utilization ($0 \le U \le 1$).
- $W/C$: Ratio of Wait time (I/O latency) to Compute time (CPU processing).

---

## 5. Practical Examples

### Example 1: High-Performance Distributed Snowflake ID Generator
```java
package com.engineering.systemdesign;

public class SnowflakeIdGenerator {
    private static final long EPOCH = 1704067200000L; // 2024-01-01 00:00:00 UTC
    private static final long NODE_ID_BITS = 10L;
    private static final long SEQUENCE_BITS = 12L;
    private static final long MAX_NODE_ID = ~(-1L << NODE_ID_BITS);
    private static final long MAX_SEQUENCE = ~(-1L << SEQUENCE_BITS);

    private static final long TIMESTAMP_SHIFT = NODE_ID_BITS + SEQUENCE_BITS;
    private static final long NODE_ID_SHIFT = SEQUENCE_BITS;

    private final long nodeId;
    private long lastTimestamp = -1L;
    private long sequence = 0L;

    public SnowflakeIdGenerator(long nodeId) {
        if (nodeId < 0 || nodeId > MAX_NODE_ID) {
            throw new IllegalArgumentException("Node ID out of range: " + nodeId);
        }
        this.nodeId = nodeId;
    }

    public synchronized long nextId() {
        long currentTimestamp = System.currentTimeMillis();

        if (currentTimestamp < lastTimestamp) {
            throw new IllegalStateException("Clock moved backwards! Refusing to generate ID.");
        }

        if (currentTimestamp == lastTimestamp) {
            sequence = (sequence + 1) & MAX_SEQUENCE;
            if (sequence == 0) {
                // Sequence exhausted for this millisecond; spin until next millisecond
                while (currentTimestamp <= lastTimestamp) {
                    currentTimestamp = System.currentTimeMillis();
                }
            }
        } else {
            sequence = 0L;
        }

        lastTimestamp = currentTimestamp;

        return ((currentTimestamp - EPOCH) << TIMESTAMP_SHIFT)
                | (nodeId << NODE_ID_SHIFT)
                | sequence;
    }
}
```

### Example 2: In-Memory Sliding Window Rate Limiter
```java
package com.engineering.systemdesign;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;

public class SlidingWindowRateLimiter {
    private final int maxRequests;
    private final long windowSizeMillis;
    private final ConcurrentHashMap<String, ConcurrentLinkedQueue<Long>> userRequests = new ConcurrentHashMap<>();

    public SlidingWindowRateLimiter(int maxRequests, long windowSizeMillis) {
        this.maxRequests = maxRequests;
        this.windowSizeMillis = windowSizeMillis;
    }

    public boolean allowRequest(String userId) {
        long now = System.currentTimeMillis();
        long windowBoundary = now - windowSizeMillis;

        ConcurrentLinkedQueue<Long> timestamps = userRequests.computeIfAbsent(userId, k -> new ConcurrentLinkedQueue<>());

        synchronized (timestamps) {
            // Evict outdated request timestamps
            while (!timestamps.isEmpty() && timestamps.peek() < windowBoundary) {
                timestamps.poll();
            }

            if (timestamps.size() < maxRequests) {
                timestamps.add(now);
                return true;
            }
            return false;
        }
    }
}
```

### Example 3: Two-Tier Cache with Caffeine and Distributed Store
```java
package com.engineering.systemdesign;

import java.time.Duration;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;

public class TwoTierCacheService<K, V> {
    // Simulated L1 JVM local cache
    private final ConcurrentHashMap<K, V> l1Cache = new ConcurrentHashMap<>();
    // Simulated L2 Redis distributed cache
    private final ConcurrentHashMap<K, V> l2DistributedCache = new ConcurrentHashMap<>();

    public Optional<V> get(K key, java.util.function.Function<K, V> dbFallback) {
        // 1. Check L1 Cache
        V value = l1Cache.get(key);
        if (value != null) {
            return Optional.of(value);
        }

        // 2. Check L2 Cache
        value = l2DistributedCache.get(key);
        if (value != null) {
            l1Cache.put(key, value); // Populate L1
            return Optional.of(value);
        }

        // 3. Fallback to Database
        value = dbFallback.apply(key);
        if (value != null) {
            l2DistributedCache.put(key, value);
            l1Cache.put(key, value);
            return Optional.of(value);
        }

        return Optional.empty();
    }
}
```

---

## 6. Best Practices

1. **Prevent Cache Penetration with Bloom Filters:**
   - When requests query non-existent keys (e.g., malicious user scanning IDs), requests bypass caches and hit the database. Use a Bloom Filter at the API layer to quickly reject keys guaranteed not to exist.

2. **Idempotent API Design with Idempotency Keys:**
   - Ensure mutation endpoints accept an `Idempotency-Key` header stored in Redis with an atomic `SET key value NX EX 86400`. If duplicate requests arrive during network retries, return the cached initial response.

3. **Graceful Degradation and Circuit Breaking:**
   - Protect downstream dependencies using Resilience4j circuit breakers. Configure fallback behaviors (e.g., return cached degraded recommendations if the recommendation service fails).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Cache Stampede (Thundering Herd)** | When a hot cache key expires, thousands of concurrent requests miss cache simultaneously and overwhelm the database. | Use mutex locks around cache misses or apply probabilistic early expiration (XFetch algorithm). |
| **Pinning Virtual Threads with `synchronized`** | In Java 21–23, blocking inside a `synchronized` block pins the virtual thread to its underlying carrier OS thread, degrading scalability. | Replace `synchronized` with `ReentrantLock` for blocking code paths (mitigated in Java 24+ via JEP 491). |
| **Clock Skew in Distributed Systems** | Relying on system clocks across different servers for sequencing causes causality violations due to NTP sync jumps. | Use Snowflake IDs, Lamport timestamps, or Raft consensus log indexes instead of raw wall-clock time. |
| **Unbounded Thread Pools (`newCachedThreadPool`)** | `Executors.newCachedThreadPool()` creates an unlimited number of platform threads, causing `OutOfMemoryError: unable to create native thread` under load. | Use bounded `ThreadPoolExecutor` with a bounded queue and `CallerRunsPolicy` rejection handler. |

---

## 8. Performance & Security Notes

### Performance Notes
- **Zero-Copy I/O:** When transferring files over network sockets, bypass JVM heap buffers using `FileChannel.transferTo()`. This leverages the Linux `sendfile()` system call to transfer data directly from OS page cache to socket buffers without CPU memory copying.
- **Connection Pool Tuning:** Always size database connection pools (HikariCP) based on database core counts ($N_{\text{connections}} = \text{CPU cores} \times 2 + \text{effective spindle count}$), not application thread counts.

### Security Notes
- **Distributed Denial of Service (DDoS) Mitigation:** Implement rate limiting at API gateways before requests enter application tiers.
- **SQL / NoSQL Injection Prevention:** Enforce parameterized queries (`PreparedStatement`) across all database interactions.

---

## 9. Comparison / When to Use What

### Concurrency Architectures in Modern Java

| Model | Technology | Best For | Trade-offs |
| :--- | :--- | :--- | :--- |
| **Platform Thread Pools** | `ThreadPoolExecutor` | Heavy CPU-bound computation (video encoding, ML inference) | High memory overhead (~1MB stack per thread); limited to ~5,000 threads |
| **Virtual Threads** | Java 21+ Project Loom | High-volume blocking I/O (REST APIs, JDBC database queries) | Minimal memory overhead (~1KB stack); not suitable for long CPU-bound tasks |
| **Reactive Programming** | Spring WebFlux / RxJava / Netty | Event streaming, continuous duplex WebSocket connections | Complex debugging, difficult stack traces, callback hell |

---

## 10. Interview Questions

### Q1: How do you design a distributed URL shortener (e.g., TinyURL) handling 100M daily writes and 1B daily reads in Java?
<details>
<summary>Detailed Answer</summary>

1. **Capacity Estimation:**
   - Writes: $100\text{M} / 86400 \approx 1,160\text{ writes/sec}$.
   - Reads: $1\text{B} / 86400 \approx 11,600\text{ reads/sec}$ (10:1 read-to-write ratio).
   - Storage: 100M URLs/day $\times 500\text{ bytes} \approx 50\text{GB/day} \approx 18\text{TB/year}$.
2. **Short URL Encoding:**
   - Use Base62 encoding (`[a-z, A-Z, 0-9]`). A 7-character Base62 string yields $62^7 \approx 3.5\text{ trillion}$ unique URLs.
3. **ID Generation:**
   - Generate unique 64-bit integer IDs using a distributed Snowflake generator or pre-allocated Redis token ranges.
   - Convert integer ID to Base62 string.
4. **Data Storage & Caching:**
   - Primary Store: NoSQL key-value store (Cassandra or DynamoDB) partitioned by hash of short URL key.
   - Cache: Redis Cluster caching the top 20% most popular URLs (Pareto 80/20 rule), reducing DB read load by 80%.
5. **Redirection Logic:**
   - Return HTTP `302 Found` (temporary redirect) to ensure all click analytics are logged at the URL shortener gateway, or HTTP `301 Moved Permanently` to maximize browser-side caching.
</details>

### Q2: How do you handle database connection pool sizing when using Virtual Threads in Java 21+?
<details>
<summary>Detailed Answer</summary>

A common architectural trap with Virtual Threads is creating 50,000 virtual threads and expecting the database connection pool (e.g., HikariCP) to scale to 50,000 connections.
- **The Reality:** Databases (PostgreSQL, MySQL) cannot handle 50,000 active connections due to context switching, lock contention, and process memory limits.
- **The Solution:** Keep the HikariCP pool sized conservatively (e.g., 50–200 connections based on DB CPU capacity).
- When 10,000 virtual threads attempt to query the database, they will safely wait on HikariCP's connection queue. Virtual threads waiting for a connection are **unmounted** from carrier threads, consuming negligible system resources without starving the application. Use semaphores to rate-limit database access before pool exhaustion.
</details>

### Q3: How do you guarantee exactly-once message processing in a Kafka-based microservice architecture?
<details>
<summary>Detailed Answer</summary>

True end-to-end exactly-once processing requires coordination across Producer, Broker, and Consumer:
1. **Producer Side:**
   - Enable `enable.idempotence=true` to prevent duplicate broker writes on network retries.
   - For multi-topic transactions, use Kafka transactional API (`initTransactions()`, `beginTransaction()`, `commitTransaction()`).
2. **Consumer Side (Application Layer Idempotency):**
   - Consume messages with manual offset commit (`enable.auto.commit=false`).
   - Store processed message IDs (or transaction IDs) in an atomic datastore (e.g., Redis `SET NX` or a relational table `processed_messages`).
   - Wrap business updates and message ID recording in an atomic database transaction (Transactional Outbox Pattern).
   - If a duplicate message arrives after a consumer crash, the unique key constraint rejects the duplicate.
</details>

### Q4: Explain the Cache Stampede (Thundering Herd) problem and how to prevent it in Java.
<details>
<summary>Detailed Answer</summary>

**Problem:**
When a heavily accessed key in a distributed cache expires, hundreds of concurrent requests encounter a cache miss simultaneously. All requests hit the underlying database at the exact same instant, causing query timeouts and potential cascading database failure.

**Prevention Strategies:**
1. **Mutex / Distributed Lock:** The first request to encounter a miss acquires a distributed lock (e.g., Redis Redlock) to query the database and repopulate the cache. All other threads wait or return stale data.
2. **Probabilistic Early Expiration (XFetch Algorithm):**
   $$-\beta \times \delta \times \ln(\text{random}()) > \text{TTL}$$
   As the key approaches its expiration time, background worker threads probabilistically refresh the cache *before* it actually expires.
3. **Soft TTL with Background Refresh:** Set a soft TTL (e.g., 5 mins) and a hard TTL (e.g., 10 mins). When soft TTL expires, the cache returns stale data immediately and submits an asynchronous background task to refresh the value.
</details>

### Q5: What is the Transactional Outbox Pattern and why is it essential in microservices?
<details>
<summary>Detailed Answer</summary>

**Problem:**
In a microservice, updating a local database and publishing a message to a message broker (Kafka/RabbitMQ) cannot be executed in a single atomic two-phase commit (2PC) without severe performance degradation and single-point-of-failure risks. If the DB write succeeds but broker publishing fails, systems fall into inconsistent states.

**Transactional Outbox Solution:**
1. Create an `OUTBOX` table in the service's relational database.
2. When a business entity is updated, insert the event payload into the `OUTBOX` table **within the exact same local ACID transaction**.
3. A separate asynchronous process (e.g., Debezium CDC via database transaction logs, or a polling publisher) reads the `OUTBOX` table and publishes events to Kafka.
4. Once published, the outbox record is marked processed or deleted.
This guarantees **at-least-once message delivery** without dual-write inconsistency.
</details>

### Q6: How do you design a distributed rate limiter that avoids race conditions across multiple nodes?
<details>
<summary>Detailed Answer</summary>

1. **Redis + Lua Scripting:**
   Execute Token Bucket or Sliding Window logic inside an atomic Redis Lua script. Because Redis executes Lua scripts single-threaded on a single shard, read-modify-write operations on token counters are completely atomic without distributed locks.
2. **Sliding Window Log with Redis Sorted Sets (ZSET):**
   - Key: User ID.
   - Member & Score: Request timestamp in milliseconds.
   - Steps in Lua script:
     1. `ZREMRANGEBYSCORE key 0 (current_time - window_size)` (Remove expired timestamps).
     2. `ZCARD key` (Count requests in window).
     3. If count < limit: `ZADD key current_time current_time`, set TTL, return ALLOW.
     4. Else: return REJECT.
</details>

### Q7: What are the trade-offs between Choreography and Orchestration in Distributed Sagas?
<details>
<summary>Detailed Answer</summary>

- **Choreography (Event-Driven):**
  - *Mechanism:* Each service performs its local transaction and publishes domain events. Other services listen to events and trigger their local transactions.
  - *Pros:* Decoupled, no single point of failure, simple for small workflows (2–3 steps).
  - *Cons:* Cyclic dependencies, difficult to trace execution flow, risk of split-brain in complex workflows.
- **Orchestration (Central Coordinator):**
  - *Mechanism:* A centralized orchestrator (e.g., Temporal, Camunda, or a custom state machine) sends command messages to participants and waits for execution status.
  - *Pros:* Clear workflow visualization, centralized state management, simple compensation logic.
  - *Cons:* Orchestrator can become a bottleneck; higher architectural complexity.
</details>

### Q8: How does Consistent Hashing work and why is it preferred over `hash(key) % N` for distributed caches?
<details>
<summary>Detailed Answer</summary>

With naive modulo hashing (`hash(key) % N`), adding or removing a single node from a cluster of $N$ nodes changes the target bucket for nearly all keys ($rac{N-1}{N} \approx 90+\%$ of keys), triggering massive cache misses and database thundering herds.

**Consistent Hashing:**
1. Maps both nodes and keys onto a circular hash ring (e.g., 0 to $2^{32}-1$).
2. A key is stored on the first node encountered by traversing clockwise on the ring.
3. When a node is added or removed, **only $\frac{1}{N}$ of keys** must be remapped to adjacent nodes.
4. **Virtual Nodes:** To prevent hotspot imbalances, each physical server is mapped to multiple (e.g., 100–300) virtual points across the ring, ensuring uniform key distribution.
</details>

---

## 11. Practice Exercises

### Exercise 1: Implement Consistent Hash Ring in Java
**Task:** Implement a thread-safe Consistent Hash Ring supporting virtual nodes and node addition/removal.

<details>
<summary>Solution</summary>

```java
package com.engineering.systemdesign;

import java.util.Collection;
import java.util.SortedMap;
import java.util.TreeMap;
import java.util.concurrent.locks.ReentrantReadWriteLock;

public class ConsistentHashRing<T> {
    private final int numberOfReplicas;
    private final SortedMap<Integer, T> circle = new TreeMap<>();
    private final ReentrantReadWriteLock rwl = new ReentrantReadWriteLock();

    public ConsistentHashRing(int numberOfReplicas, Collection<T> nodes) {
        this.numberOfReplicas = numberOfReplicas;
        for (T node : nodes) {
            addNode(node);
        }
    }

    public void addNode(T node) {
        rwl.writeLock().lock();
        try {
            for (int i = 0; i < numberOfReplicas; i++) {
                circle.put((node.toString() + "#" + i).hashCode(), node);
            }
        } finally {
            rwl.writeLock().unlock();
        }
    }

    public void removeNode(T node) {
        rwl.writeLock().lock();
        try {
            for (int i = 0; i < numberOfReplicas; i++) {
                circle.remove((node.toString() + "#" + i).hashCode());
            }
        } finally {
            rwl.writeLock().unlock();
        }
    }

    public T get(Object key) {
        rwl.readLock().lock();
        try {
            if (circle.isEmpty()) return null;
            int hash = key.hashCode();
            if (!circle.containsKey(hash)) {
                SortedMap<Integer, T> tailMap = circle.tailMap(hash);
                hash = tailMap.isEmpty() ? circle.firstKey() : tailMap.firstKey();
            }
            return circle.get(hash);
        } finally {
            rwl.readLock().unlock();
        }
    }
}
```
</details>

### Exercise 2: Outbox Pattern Database Schema & Poller
**Task:** Design the SQL schema and Java worker thread for polling and publishing transactional outbox records.

<details>
<summary>Solution</summary>

```sql
-- PostgreSQL Outbox Table Schema
CREATE TABLE outbox_events (
    id UUID PRIMARY KEY,
    aggregate_type VARCHAR(255) NOT NULL,
    aggregate_id VARCHAR(255) NOT NULL,
    event_type VARCHAR(255) NOT NULL,
    payload JSONB NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    processed BOOLEAN DEFAULT FALSE
);
CREATE INDEX idx_outbox_unprocessed ON outbox_events(created_at) WHERE processed = FALSE;
```

```java
package com.engineering.systemdesign;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;

public class OutboxPublisher {
    public void processOutbox(Connection conn) throws Exception {
        // Select for update skip locked to allow concurrent worker instances
        String sql = "SELECT id, payload FROM outbox_events WHERE processed = FALSE ORDER BY created_at LIMIT 100 FOR UPDATE SKIP LOCKED";
        try (PreparedStatement ps = conn.prepareStatement(sql);
             ResultSet rs = ps.executeQuery()) {
            while (rs.next()) {
                String id = rs.getString("id");
                String payload = rs.getString("payload");
                
                // Publish to Kafka
                // kafkaProducer.send(new ProducerRecord<>("events", id, payload));

                // Mark processed
                try (PreparedStatement update = conn.prepareStatement("UPDATE outbox_events SET processed = TRUE WHERE id = ?::uuid")) {
                    update.setString(1, id);
                    update.executeUpdate();
                }
            }
        }
    }
}
```
</details>

### Exercise 3: Resilience4j Circuit Breaker Pipeline
**Task:** Configure a programmatic Resilience4j CircuitBreaker with sliding window metrics, failure rate thresholds, and fallback execution.

<details>
<summary>Solution</summary>

```java
package com.engineering.systemdesign;

import io.github.resilience4j.circuitbreaker.CircuitBreaker;
import io.github.resilience4j.circuitbreaker.CircuitBreakerConfig;

import java.time.Duration;
import java.util.function.Supplier;

public class CircuitBreakerDemo {
    public static void main(String[] args) {
        CircuitBreakerConfig config = CircuitBreakerConfig.custom()
                .failureRateThreshold(50.0f)
                .waitDurationInOpenState(Duration.ofSeconds(10))
                .slidingWindowSize(20)
                .minimumNumberOfCalls(5)
                .build();

        CircuitBreaker circuitBreaker = CircuitBreaker.of("backendService", config);

        Supplier<String> decoratedSupplier = CircuitBreaker.decorateSupplier(circuitBreaker, () -> {
            // Simulated downstream network RPC
            if (Math.random() > 0.3) throw new RuntimeException("Downstream timeout");
            return "Service Response";
        });

        for (int i = 0; i < 10; i++) {
            try {
                String result = decoratedSupplier.get();
                System.out.println("Success: " + result);
            } catch (Exception e) {
                System.out.println("Failed or Circuit Open: " + e.getMessage());
            }
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Virtual Threads for I/O:** Use Java 21+ Virtual Threads for massive blocking I/O throughput; avoid pinning with `synchronized` in versions prior to Java 24.
- **Snowflake ID:** 64-bit monotonically increasing ID composed of 41 bits timestamp, 10 bits worker/datacenter ID, and 12 bits sequence number.
- **Cache Invalidation:** Always prefer Cache-Aside with Redis TTL + Outbox event bus for cache eviction. Use Redis Lua scripts for atomic rate limiting.
- **Resilience Triad:** Rate Limiting (protect self) + Circuit Breakers (protect downstream) + Idempotency Keys (protect state from retries).

---

## 13. Further Reading

- [Designing Data-Intensive Applications by Martin Kleppmann](https://dataintensive.net/)
- [JEP 444: Virtual Threads](https://openjdk.org/jeps/444)
- [Kafka: The Definitive Guide by Gwen Shapira et al.](https://www.oreilly.com/library/view/kafka-the-definitive/9781492043072/)
- [Microservices Patterns by Chris Richardson](https://microservices.io/book)
