# 01: System Design Framework for Spring Boot Engineers

| Metadata Field | Value |
|---|---|
| **Topic** | System Design Interview & Architecture Framework |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Audience** | Senior / Lead / Staff Backend Engineers |
| **Prerequisites** | Distributed Systems Fundamentals, Microservices, RDBMS, Caching |

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

In modern software engineering, designing robust backend systems requires balancing functional capabilities with non-functional constraints: throughput, latency, fault tolerance, consistency, security, and developer ergonomics. 

For Java and Spring Boot engineers, system design is not merely drawing boxes and arrows on a whiteboard; it is understanding how the framework's threading models (Virtual Threads vs Platform Threads), connection pooling (HikariCP), persistence layer (Hibernate/JPA vs JDBC vs R2DBC), caching abstractions, and messaging adapters behave under real-world loads exceeding 100,000 queries per second (QPS).

A structured, 4-step framework ensures engineers cover functional requirements, back-of-the-envelope calculations, high-level API and database schema design, and deep-dive failure mode analyses without missing critical bottlenecks.

---

## 2. Core Concepts

### 2.1 The 4-Step System Design Blueprint
1. **Requirements Clarification & Scope (5-7 mins)**:
   - *Functional Requirements*: What operations must the system execute? (e.g., Shorten URL, Redirect URL, View Analytics).
   - *Non-Functional Requirements (NFRs)*: High availability vs strict consistency (CAP theorem), $p99$ latency targets, durability, data retention.
2. **Back-of-the-Envelope Capacity Estimations (5 mins)**:
   - Read vs Write ratios (e.g., 100:1 read-heavy).
   - QPS calculations (Daily active users $\times$ requests / 86,400 seconds). Peak QPS = $2\times$ to $5\times$ average.
   - Storage volume (5-year disk projections).
   - Memory caching capacity (80/20 Pareto rule: cache top 20% hot data in RAM).
3. **High-Level Design & Core Data Contracts (10-15 mins)**:
   - REST/gRPC API contracts.
   - Database schema (Relational vs NoSQL, primary keys, indexing strategies).
   - Core end-to-end data flow with architecture diagrams.
4. **Deep Dive & Failure Mode Analysis (15-20 mins)**:
   - Bottlenecks, single points of failure (SPOF), partition tolerance, database connection exhaustion, cache stampedes, retry storms, and resilience patterns (Circuit Breakers, Bulkheads, Outbox pattern).

---

## 3. Internals & Architecture

```
                  ┌─────────────────────────────────────────┐
                  │   DNS / Global Anycast Load Balancer    │
                  └────────────────────┬────────────────────┘
                                       │
                                       ▼
                  ┌─────────────────────────────────────────┐
                  │         Edge CDN / CloudFront           │
                  │   - Static assets & Edge Caching        │
                  └────────────────────┬────────────────────┘
                                       │
                                       ▼
                  ┌─────────────────────────────────────────┐
                  │    L7 API Gateway / Reverse Proxy       │
                  │   (Spring Cloud Gateway / Envoy)        │
                  │   - TLS termination, JWT validation     │
                  │   - Rate limiting, Correlation ID       │
                  └────────────────────┬────────────────────┘
                                       │
                ┌──────────────────────┴──────────────────────┐
                ▼                                             ▼
 ┌─────────────────────────────┐               ┌─────────────────────────────┐
 │  Spring Boot Node 1         │               │  Spring Boot Node 2         │
 │  (Java 21 Virtual Threads)  │               │  (Java 21 Virtual Threads)  │
 │  ┌───────────────────────┐  │               │  ┌───────────────────────┐  │
 │  │ HikariCP Connection   │  │               │  │ HikariCP Connection   │  │
 │  │ Pool (Max: 50)        │  │               │  │ Pool (Max: 50)        │  │
 │  └───────────┬───────────┘  │               │  └───────────┬───────────┘  │
 └──────────────┼──────────────┘               └──────────────┼──────────────┘
                │                                             │
                ├──────────────────────┬──────────────────────┤
                ▼                      ▼                      ▼
 ┌─────────────────────────────┐ ┌──────────────┐ ┌───────────────────────────┐
 │ Distributed Cache           │ │ Primary DB   │ │ Async Event Broker        │
 │ (Redis Cluster 7.x)         │ │ (PostgreSQL) │ │ (Apache Kafka / RabbitMQ) │
 │ - Hot read entities (TTL)   │ │ - Writes     │ │ - Event-driven decoupling │
 └─────────────────────────────┘ └──────┬───────┘ └───────────────────────────┘
                                        │ Replication
                                        ▼
                                 ┌──────────────┐
                                 │ Read Replica │
                                 │ - Queries    │
                                 └──────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Capacity Estimation Formulas
$$\text{Average QPS} = \frac{\text{Daily Active Users (DAU)} \times \text{Requests per User}}{86,400\text{ seconds}}$$
$$\text{Peak QPS} = \text{Average QPS} \times \text{Peak Factor (usually } 2\text{ to } 5)$$
$$\text{5-Year Storage} = \text{Daily Writes} \times \text{Payload Size (bytes)} \times 365 \times 5 \times \text{Replication Factor (3)}$$
$$\text{Cache RAM Required} = \text{Daily Read Requests} \times 0.20 \times \text{Payload Size}$$

---

## 5. 5 Practical Examples

### Example 1: Back-of-the-Envelope Capacity Calculator in Java

```java
public class CapacityCalculator {

    public record EstimationResult(long avgQps, long peakQps, long dailyStorageBytes, long cacheRamBytes) {}

    public static EstimationResult calculate(long dau, int reqsPerUser, int payloadBytes, double readWriteRatio) {
        long totalDailyReqs = dau * reqsPerUser;
        long avgQps = totalDailyReqs / 86_400;
        long peakQps = avgQps * 3;

        long dailyWrites = (long) (totalDailyReqs / (1.0 + readWriteRatio));
        long dailyStorageBytes = dailyWrites * payloadBytes;

        // 80/20 Pareto rule for caching top 20% hot daily read entities
        long dailyReads = totalDailyReqs - dailyWrites;
        long cacheRamBytes = (long) (dailyReads * 0.20 * payloadBytes);

        return new EstimationResult(avgQps, peakQps, dailyStorageBytes, cacheRamBytes);
    }

    public static void main(String[] args) {
        EstimationResult res = calculate(50_000_000, 10, 500, 9.0);
        System.out.printf("Avg QPS: %,d | Peak QPS: %,d%n", res.avgQps(), res.peakQps());
        System.out.printf("Daily Storage: %,d MB | Cache RAM: %,d MB%n",
                res.dailyStorageBytes() / (1024 * 1024),
                res.cacheRamBytes() / (1024 * 1024));
    }
}
```

### Example 2: Non-Blocking WebMvc Virtual Threads Configuration

```java
package com.example.design.config;

import org.springframework.boot.autoconfigure.task.TaskExecutionAutoConfiguration;
import org.springframework.boot.web.embedded.tomcat.TomcatProtocolHandlerCustomizer;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.task.AsyncTaskExecutor;
import org.springframework.core.task.support.TaskExecutorAdapter;

import java.util.concurrent.Executors;

@Configuration
public class VirtualThreadConfig {

    @Bean(TaskExecutionAutoConfiguration.APPLICATION_TASK_EXECUTOR_BEAN_NAME)
    public AsyncTaskExecutor asyncTaskExecutor() {
        return new TaskExecutorAdapter(Executors.newVirtualThreadPerTaskExecutor());
    }

    @Bean
    public TomcatProtocolHandlerCustomizer<?> protocolHandlerVirtualThreadCustomizer() {
        return protocolHandler -> protocolHandler.setExecutor(Executors.newVirtualThreadPerTaskExecutor());
    }
}
```

### Example 3: Resilient Fallback Pattern with Resilience4j

```java
package com.example.design.service;

import io.github.resilience4j.circuitbreaker.annotation.CircuitBreaker;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

@Service
public class ExternalUserProfileClient {

    private static final Logger log = LoggerFactory.getLogger(ExternalUserProfileClient.class);
    private final RestClient restClient;

    public ExternalUserProfileClient(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("https://api.internal.corp").build();
    }

    @CircuitBreaker(name = "userProfileService", fallbackMethod = "fallbackProfile")
    public String fetchUserProfile(String userId) {
        return restClient.get()
                .uri("/users/{id}", userId)
                .retrieve()
                .body(String.class);
    }

    public String fallbackProfile(String userId, Throwable ex) {
        log.warn("Downstream user service failed for userId {}. Invoking degraded fallback: {}", userId, ex.getMessage());
        return "{\"userId\":\"" + userId + "\",\"status\":\"ANONYMOUS_DEGRADED\"}";
    }
}
```

### Example 4: Distributed Idempotency Filter

```java
package com.example.design.filter;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;
import java.time.Duration;

@Component
public class IdempotencyFilter extends OncePerRequestFilter {

    private final StringRedisTemplate redisTemplate;

    public IdempotencyFilter(StringRedisTemplate redisTemplate) {
        this.redisTemplate = redisTemplate;
    }

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {

        if ("POST".equalsIgnoreCase(request.getMethod()) || "PUT".equalsIgnoreCase(request.getMethod())) {
            String idempotencyKey = request.getHeader("Idempotency-Key");
            if (idempotencyKey != null && !idempotencyKey.isBlank()) {
                String redisKey = "idempotency:" + idempotencyKey;
                Boolean isNew = redisTemplate.opsForValue().setIfAbsent(redisKey, "IN_PROGRESS", Duration.ofMinutes(10));

                if (Boolean.FALSE.equals(isNew)) {
                    response.setStatus(HttpStatus.CONFLICT.value());
                    response.setContentType("application/json");
                    response.getWriter().write("{\"error\":\"Concurrent or duplicate request detected\"}");
                    return;
                }
            }
        }
        filterChain.doFilter(request, response);
    }
}
```

### Example 5: HikariCP High-Throughput Production Sizing

```yaml
spring:
  datasource:
    hikari:
      pool-name: ProductionHikariPool
      minimum-idle: 10
      maximum-pool-size: 50
      idle-timeout: 300000 # 5 minutes
      max-lifetime: 1800000 # 30 minutes
      connection-timeout: 2000 # 2 seconds
      leak-detection-threshold: 5000 # 5 seconds
      connection-test-query: SELECT 1
```

---

## 6. Best Practices

- **Clarify non-functional constraints before architecture**: Ask about consistency tolerance, availability targets (e.g. 99.99%), geographical distribution, and audit requirements.
- **Size pools by disk/CPU limits, not thread count**: With Java 21 Virtual Threads, never allocate 10,000 DB connections; scale HikariCP to:
  $$\text{connections} = ((\text{core\_count} \times 2) + \text{effective\_spindle\_count})$$
- **Embrace Stateless Application Nodes**: Keep session and ephemeral state in Redis or JWTs so Spring Boot containers can scale out horizontally behind load balancers.
- **Implement Cache-Aside with Jitter**: Avoid cache stampedes by appending a random jitter ($10\%\text{ of TTL}$) to cached items.
- **Guard Downstream Boundaries**: Never invoke an external network dependency without an explicit connect timeout (e.g., 500ms), read timeout (e.g., 2000ms), and circuit breaker.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Infinite Database Pools** | Thousands of threads opening connections overwhelm DB CPU context switching and exhaust RAM. | Restrict HikariCP pool size (e.g. 30-50) and use fast read-replicas. |
| **Synchronous Distributed Transactions (2PC)** | Two-phase commit blocks all participating microservices, destroying availability and latency. | Use Sagas (Choreographed or Orchestrated) with compensation logic. |
| **Cache Stampede (Dogpile Effect)** | Thousands of concurrent requests hit the DB simultaneously when a popular cache key expires. | Use Redis distributed locks or mutex keys during cache misses, or soft TTL refresh. |
| **Synchronous Inter-Service Calls** | Deep chains ($A \to B \to C \to D$) multiply latencies and cascade failures ($0.99^4 = 0.96$ uptime). | Decouple non-essential flows using Kafka or RabbitMQ event streams. |
| **Missing Idempotency in Payment/Order APIs** | Network retries trigger double payments or duplicate order submissions. | Mandate client-generated `Idempotency-Key` headers stored in Redis/DB with unique constraints. |
| **Unbounded In-Memory Queues** | Placing jobs in JVM `LinkedBlockingQueue` causes `OutOfMemoryError` during downstream outages. | Back queues with durable brokers (RabbitMQ/Kafka) with backpressure and DLQs. |

---

## 8. Performance & Security Considerations

### 8.1 Virtual Threads Pinning Awareness
Under Java 21, avoid `synchronized` blocks around blocking I/O (e.g., JDBC or socket read). Use `ReentrantLock` instead to prevent carrier thread pinning:
```java
private final ReentrantLock lock = new ReentrantLock();

public void performSafeVirtualThreadIo() {
    lock.lock();
    try {
        // Safe blocking call without pinning carrier thread
    } finally {
        lock.unlock();
    }
}
```

### 8.2 Zero-Trust Security at the Edge
- Validate JWT cryptographic signatures at the API Gateway; pass normalized user metadata downstream via mutual TLS (mTLS) headers.
- Sanitize all inbound strings to prevent SQLi and Path Traversal.

---

## 9. Comparison / Decision Matrix

| Dimension | Monolithic Architecture | Modular Monolith (Modulith) | Microservices Architecture |
|---|---|---|---|
| **Development Velocity** | Fast initially; degrades with team growth | High; strict package boundaries | High for independent distributed teams |
| **Deployment Complexity** | Low (Single JAR) | Low (Single JAR) | High (Kubernetes, Helm, Istio, CI/CD pipelines) |
| **Network Latency** | In-process JVM calls (nanoseconds) | In-process JVM calls (nanoseconds) | Network overhead (HTTP/gRPC/Kafka: 2-50ms) |
| **Data Consistency** | ACID transactions supported | ACID across modules supported | Eventual consistency; Sagas required |
| **Fault Isolation** | Low (Crash takes down process) | Low (Crash takes down process) | High (Individual services fail independently) |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. How do you approach calculating capacity in a system design interview?</strong></summary>

Start with Daily Active Users (DAU), estimate actions per user per day, and calculate average QPS ($\text{DAU} \times \text{actions} / 86400$). Multiply by 2 to 5 for peak QPS. Derive storage by multiplying daily writes by average payload size, and project across 3 to 5 years. For caching, apply the 80/20 rule: top 20% of read volume held in Redis RAM.
</details>

<details>
<summary><strong>2. Why should HikariCP maximumPoolSize not be set to a high number (e.g., 500)?</strong></summary>

A database server has fixed physical CPU cores and disk spindles. When 500 connections query simultaneously, the OS and database engine waste more time context switching and managing disk/memory contention than executing queries. A smaller pool (e.g., 20-50) yields higher throughput and lower queue times.
</details>

<details>
<summary><strong>3. How do Virtual Threads change system design in Spring Boot 3.4+?</strong></summary>

Virtual Threads eliminate the 1:1 binding between JVM threads and OS carrier threads. A Spring Boot server can handle millions of concurrent idle or I/O-blocked connections without reactive programming (WebFlux) complexity, while maintaining simple synchronous coding styles (`RestClient`, Spring MVC).
</details>

<details>
<summary><strong>4. How do you prevent cascading failures in a distributed microservice topology?</strong></summary>

Use circuit breakers (Resilience4j), bulkheads to isolate thread pools per downstream service, strict network timeouts (connect $\le 500$ms, read $\le 2$s), exponential backoff with jitter, and graceful fallback responses.
</details>

<details>
<summary><strong>5. What is the difference between Strong Consistency and Eventual Consistency in distributed data?</strong></summary>

Strong consistency ensures any read immediately following a write returns the newest value (e.g., via consensus protocols like Raft or primary RDBMS). Eventual consistency allows temporary replication lag across replicas or microservices, guaranteeing convergence after a short interval.
</details>

<details>
<summary><strong>6. How do you implement distributed idempotency for financial APIs?</strong></summary>

Require an `Idempotency-Key` HTTP header. Check Redis via atomic `SETNX` (set if not exists) with a TTL. If present, return HTTP 409 or the cached response. Store the key in a unique constraint table in the database alongside the transaction.
</details>

<details>
<summary><strong>7. When would you choose Kafka over RabbitMQ?</strong></summary>

Choose Kafka for high-throughput, ordered, append-only event streaming, event replayability, and analytics pipelines. Choose RabbitMQ for complex message routing (topic, headers, fanout), low-latency point-to-point task queues, and per-message acknowledgement/priority.
</details>

<details>
<summary><strong>8. How do you handle cache invalidation at scale?</strong></summary>

Common strategies: Cache-Aside with TTL, Write-Through, or Change Data Capture (CDC via Debezium) streaming database write-ahead logs to Kafka to asynchronously invalidate Redis keys.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Compute Capacity for a Global Photo Sharing Service</strong></summary>

**Prompt**: 200M DAU, each user views 20 photos and posts 1 photo daily. Average photo metadata = 1 KB, photo binary = 2 MB. Calculate QPS, write bandwidth, and 3-year metadata storage.

**Solution**:
- Read requests: $200\text{M} \times 20 = 4\text{ billion reads/day} \approx 46,296\text{ Read QPS}$ (Peak $\approx 100,000$ QPS).
- Write requests: $200\text{M} \times 1 = 200\text{ million writes/day} \approx 2,315\text{ Write QPS}$ (Peak $\approx 6,000$ QPS).
- Daily photo binary storage: $200\text{M} \times 2\text{ MB} = 400\text{ TB/day}$.
- 3-Year Metadata Storage: $200\text{M} \times 1\text{ KB} \times 365 \times 3 \approx 219\text{ TB}$.
</details>

<details>
<summary><strong>Exercise 2: Design an Idempotent Payment Service</strong></summary>

**Prompt**: Implement a Spring Boot filter and service layer ensuring that duplicate charge requests with the same transaction token are never billed twice.

**Solution**:
Inspect Example 4: check Redis for `idempotency:{key}` using `setIfAbsent()`. If key exists, return HTTP 409 Conflict. If new, proceed with billing and save the completed transaction record in PostgreSQL with a unique constraint on `idempotency_key`.
</details>

<details>
<summary><strong>Exercise 3: Configure Spring Boot 3.4 for Virtual Threads</strong></summary>

**Prompt**: Enable Virtual Threads in `application.yaml` and verify Tomcat thread naming.

**Solution**:
In `application.yaml`:
```yaml
spring:
  threads:
    virtual:
      enabled: true
```
Thread names will print as `VirtualThread[#...]/runnable@ForkJoinPool-1-worker-...`.
</details>

<details>
<summary><strong>Exercise 4: Implement Cache Stampede Defense</strong></summary>

**Prompt**: Write a Java service method that locks cache misses using Redis to prevent dogpiling.

**Solution**:
```java
public String getCachedValue(String key) {
    String val = redis.opsForValue().get(key);
    if (val != null) return val;

    String lockKey = "lock:" + key;
    Boolean acquired = redis.opsForValue().setIfAbsent(lockKey, "locked", Duration.ofSeconds(5));
    if (Boolean.TRUE.equals(acquired)) {
        try {
            val = db.fetch(key);
            redis.opsForValue().set(key, val, Duration.ofMinutes(15));
            return val;
        } finally {
            redis.delete(lockKey);
        }
    } else {
        Thread.sleep(100);
        return getCachedValue(key);
    }
}
```
</details>

<details>
<summary><strong>Exercise 5: Design a Dead Letter Queue (DLQ) Consumer</strong></summary>

**Prompt**: Configure Spring Kafka container factory with an exponential backoff error handler routing failed records to `events.DLT`.

**Solution**:
```java
@Bean
public DefaultErrorHandler errorHandler(KafkaTemplate<String, Object> template) {
    var recoverer = new DeadLetterPublishingRecoverer(template,
            (r, e) -> new TopicPartition(r.topic() + ".DLT", r.partition()));
    var backOff = new ExponentialBackOffWithMaxRetries(3);
    backOff.setInitialInterval(1000L);
    backOff.setMultiplier(2.0);
    return new DefaultErrorHandler(recoverer, backOff);
}
```
</details>

<details>
<summary><strong>Exercise 6: Design a Rate-Limited Client Interceptor</strong></summary>

**Prompt**: Create a Spring `ClientHttpRequestInterceptor` that respects `Retry-After` HTTP headers from downstream services.

**Solution**:
Inspect response status: if `429 Too Many Requests`, read the `Retry-After` header, sleep for that duration, and retry once.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│               SYSTEM DESIGN INTERVIEW CHECKLIST                        │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Requirements: Functional (User APIs) vs Non-Functional (QPS, SLA)   │
│ 2. Math: Read/Write ratio, Peak QPS = Avg * 3, 5-Yr Storage, 80/20 RAM │
│ 3. API & DB: REST verbs, JSON schemas, Primary Keys, Sharding Keys    │
│ 4. Resilience: Circuit Breaker, Idempotency-Key, Virtual Threads, DLQ  │
│ 5. DB Pool: HikariCP = (Cores * 2) + Effective Spindles                │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring Boot 3 Reference Documentation](https://docs.spring.io/spring-boot/reference/)
- [Designing Data-Intensive Applications (Martin Kleppmann)](https://dataintensive.net/)
- [System Design Interview by Alex Xu (Volume 1 & 2)](https://bytebytego.com/)
- [HikariCP About Pool Sizing Guide](https://github.com/brettwooldridge/HikariCP/wiki/About-Pool-Sizing)
- [Resilience4j Official Documentation](https://resilience4j.readme.io/)
