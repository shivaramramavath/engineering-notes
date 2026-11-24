# Top 10 Performance Pitfalls in Spring Boot Enterprise Applications

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
- [2. The Top 10 Performance Pitfalls Overview](#2-the-top-10-performance-pitfalls-overview)
- [3. Deep-Dive: Pitfalls 1 through 5](#3-deep-dive-pitfalls-1-through-5)
- [4. Deep-Dive: Pitfalls 6 through 10](#4-deep-dive-pitfalls-6-through-10)
- [5. Diagnostic Architecture & Detection Pipeline](#5-diagnostic-architecture--detection-pipeline)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Mistakes & Anti-Patterns Matrix](#7-common-mistakes--anti-patterns-matrix)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / Architectural Impact Matrix](#9-comparison--architectural-impact-matrix)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
In enterprise software engineering, performance degradation rarely stems from complex algorithmic flaws in business logic. Instead, 90% of production outages, latency spikes, and cloud infrastructure over-expenditure are caused by a recurring set of **subtle architectural anti-patterns** embedded within Spring Boot configurations, ORM mappings, thread management, and network I/O:
- Database connection pools are starved because connections are held during slow JSON serialization.
- Services collapse under moderate load because an outbound HTTP client lacks read timeouts, locking all worker threads indefinitely.
- Memory leaks slowly accumulate in static data structures, triggering catastrophic GC thrashing during peak business hours.

### Historical Evolution
As Spring Boot abstracted away complex low-level plumbing (auto-configuring datasources, thread pools, serialization, and transactions), it introduced "magic" defaults optimized for developer convenience rather than high-throughput production resilience. 

Recognizing these traps, modern **Spring Boot 3.4** on **Java 21** provides strict guardrails, warning logs (such as OSIV startup warnings), and observational tools to identify and eradicate these pitfalls before they reach production.

### Production Relevance
Systematically auditing and resolving these 10 pitfalls transforms sluggish, brittle microservices into resilient, sub-millisecond systems capable of sustaining 50,000+ requests/second with minimal cloud infrastructure costs.

---

## 2. The Top 10 Performance Pitfalls Overview

```
+------------------------------------------------------------------------------------+
|                      THE TOP 10 SPRING BOOT PERFORMANCE PITFALLS                   |
+------------------------------------------------------------------------------------+
|  1. Open-Session-In-View (OSIV) Connection Starvation                              |
|  2. The JPA N+1 Query Trap & Missing DTO Projections                               |
|  3. Unbounded HTTP Client Timeouts (Cascading Thread Exhaustion)                   |
|  4. HikariCP Pool Sizing Fallacy (Over-allocation & CPU Context Switching)         |
|  5. Virtual Thread Carrier Pinning (synchronized blocks around blocking I/O)       |
|  6. Eager String Concatenation & Inefficient Logging in Hot Paths                  |
|  7. Unbounded In-Memory Collections (Silent Heap Memory Leaks)                     |
|  8. Self-Invocation Bypassing Spring Proxies (@Cacheable, @Async, @Transactional) |
|  9. Unbounded @Async Thread Pools Without Backpressure / Rejection Policies        |
| 10. Large Multipart File Uploads Buffering Synchronously into JVM Heap             |
+------------------------------------------------------------------------------------+
```

---

## 3. Deep-Dive: Pitfalls 1 through 5

### Pitfall 1: Open-Session-In-View (OSIV) Connection Starvation
- **The Pitfall**: Spring Boot enables `spring.jpa.open-in-view=true` by default in web starters. This keeps the database connection checked out during the entire HTTP response lifecycle (including JSON serialization).
- **Why It Fails**: If client network serialization is slow, database connections sit completely idle, starving the HikariCP pool.
- **The Fix**: Explicitly disable OSIV in `application.yml`:
  ```yaml
  spring:
    jpa:
      open-in-view: false
  ```

### Pitfall 2: The JPA N+1 Query Trap & Entity Over-Fetching
- **The Pitfall**: Fetching parent entities and accessing lazy relationships inside loops, or loading entire entity graphs when only 3 columns are needed.
- **Why It Fails**: Generates hundreds of secondary SQL roundtrips, and forces Hibernate to maintain dirty-checking snapshots on all loaded entities.
- **The Fix**: Use `JOIN FETCH`, Spring Data `@EntityGraph`, or constructor DTO record projections:
  ```java
  @Query("SELECT new com.example.OrderSummary(o.id, c.name, o.total) FROM Order o JOIN o.customer c")
  List<OrderSummary> findSummaries();
  ```

### Pitfall 3: Missing Outbound HTTP Timeouts (Cascading Failure)
- **The Pitfall**: Instantiating `RestClient`, `RestTemplate`, or `WebClient` without configuring explicit connect and read timeouts.
- **Why It Fails**: If an external partner experiences latency or network blackholes, incoming request threads hang indefinitely waiting for TCP socket packets. Within minutes, all 200 Tomcat worker threads are exhausted, crashing the entire service.
- **The Fix**: Always configure strict timeouts via `ClientHttpRequestFactories`:
  ```java
  @Bean
  public RestClient partnerClient(RestClient.Builder builder) {
      return builder
          .baseUrl("https://partner.api")
          .requestFactory(ClientHttpRequestFactories.get(
              ClientHttpRequestFactorySettings.DEFAULTS
                  .withConnectTimeout(Duration.ofSeconds(1))
                  .withReadTimeout(Duration.ofSeconds(3))
          ))
          .build();
  }
  ```

### Pitfall 4: The HikariCP Pool Sizing Fallacy
- **The Pitfall**: Setting `maximum-pool-size: 200` under the false assumption that more connections equal higher throughput.
- **Why It Fails**: The database server's physical CPU cores spend more time context-switching between 200 competing queries than executing SQL, degrading overall throughput.
- **The Fix**: Apply the empirical formula:
  $$\text{Pool Size} = (\text{CPU Cores} \times 2) + \text{Effective Spindles}$$
  Set a fixed-size pool (`minimum-idle == maximum-pool-size`) between 10 and 30 connections.

### Pitfall 5: Virtual Thread Carrier Pinning
- **The Pitfall**: Running blocking I/O (JDBC, REST, `Thread.sleep`) inside `synchronized` blocks or methods when using Java 21 Virtual Threads.
- **Why It Fails**: `synchronized` pins the virtual thread to its underlying OS carrier thread. If all carrier threads become pinned, the entire JVM stalls.
- **The Fix**: Replace `synchronized` with `java.util.concurrent.locks.ReentrantLock`, which permits clean unmounting during blocking calls.

---

## 4. Deep-Dive: Pitfalls 6 through 10

### Pitfall 6: Eager String Concatenation in Hot Logging Paths
- **The Pitfall**: Writing `log.debug("Processing order " + order.getId() + " items: " + order.getItems())`.
- **Why It Fails**: String concatenation evaluates eagerly even when DEBUG logging is disabled in production, allocating millions of unnecessary `String` and `StringBuilder` objects on the heap.
- **The Fix**: Use SLF4J parameterized logging:
  ```java
  log.debug("Processing order {} items: {}", order.getId(), order.getItems());
  ```

### Pitfall 7: Unbounded In-Memory Collections (Silent Heap Leaks)
- **The Pitfall**: Storing data in static `Map<String, Object>` or unconstrained lists without an eviction policy.
- **Why It Fails**: Objects are promoted to the Tenured (Old Generation) space, accumulating until the JVM suffers from severe GC thrashing and eventually crashes with `OutOfMemoryError: Java heap space`.
- **The Fix**: Always use bounded caches like **Caffeine** with `maximumSize` and `expireAfterWrite`.

### Pitfall 8: Self-Invocation Bypassing Spring AOP Proxies
- **The Pitfall**: Calling `@Cacheable`, `@Async`, or `@Transactional` methods directly from another method inside the exact same class (`this.processOrder()`).
- **Why It Fails**: Spring intercepts annotations using dynamic proxies. Self-invocation calls the raw `this` reference directly, completely bypassing proxy interception; transactions, caches, and async executions are silently skipped!
- **The Fix**: Move the annotated method to a separate Spring `@Component` or inject the self-bean proxy.

### Pitfall 9: Unbounded `@Async` Thread Pools Without Backpressure
- **The Pitfall**: Using `@Async` without configuring a custom `ThreadPoolTaskExecutor` (relying on `SimpleAsyncTaskExecutor` or default unbounded `LinkedBlockingQueue`).
- **Why It Fails**: Under sudden load surges, the queue expands indefinitely until JVM heap memory is exhausted, or spawns thousands of unmanaged platform threads.
- **The Fix**: Configure a bounded executor with a `CallerRunsPolicy` rejection handler:
  ```java
  @Bean
  public ThreadPoolTaskExecutor taskExecutor() {
      ThreadPoolTaskExecutor executor = new ThreadPoolTaskExecutor();
      executor.setCorePoolSize(10);
      executor.setMaxPoolSize(25);
      executor.setQueueCapacity(500);
      executor.setRejectedExecutionHandler(new ThreadPoolExecutor.CallerRunsPolicy());
      executor.initialize();
      return executor;
  }
  ```

### Pitfall 10: Synchronous In-Memory Multipart File Buffering
- **The Pitfall**: Accepting large file uploads (`@RequestParam("file") MultipartFile file`) without stream processing or storage limits.
- **Why It Fails**: If 50 concurrent users upload 20MB files, Tomcat buffers gigabytes of data into heap/disk memory simultaneously, causing immediate OutOfMemory crashes.
- **The Fix**: Cap upload limits (`server.tomcat.max-swallow-size`, `spring.servlet.multipart.max-file-size`) or stream directly to Amazon S3 via reactive chunked streams.

---

## 5. Diagnostic Architecture & Detection Pipeline

```
+------------------------------------------------------------------------------------+
|                         CONTINUOUS PERFORMANCE AUDIT PIPELINE                      |
+------------------------------------------------------------------------------------+
                   |
                   v
+------------------------------------------------------------------------------------+
| 1. CODE REVIEW & STATIC ANALYSIS                                                   |
|    - SpotBugs / SonarQube: Detects OSIV, synchronized blocks, self-invocation      |
|    - Checkstyle: Flags string concatenation inside log calls                       |
+------------------------------------------------------------------------------------+
                   |
                   v
+------------------------------------------------------------------------------------+
| 2. CI/CD INTEGRATION & AUTOMATED PERFORMANCE GATES                                 |
|    - k6 / Gatling: Validates P99 latency under concurrency                         |
|    - QuickPerf: Automated unit test assertions verifying exact SQL query counts    |
+------------------------------------------------------------------------------------+
                   |
                   v
+------------------------------------------------------------------------------------+
| 3. PRODUCTION OBSERVABILITY & METRICS                                              |
|    - HikariCP Metrics: hikaricp_connections_pending > 0 (Pool starvation)         |
|    - Actuator / Prometheus: tomcat.threads.busy == tomcat.threads.config.max       |
|    - JFR Events: jdk.VirtualThreadPinned (Detects carrier thread pinning)          |
+------------------------------------------------------------------------------------+
```

---

## 6. Best Practices & Production Guidelines

1. **Always Set `spring.jpa.open-in-view=false`**: Eliminate the single largest database connection leak vector in Spring Boot applications.
2. **Enforce Timeouts on Every Network Client**: No HTTP, gRPC, database, or cache client should ever exist without explicit connect, socket, and read timeouts.
3. **Use Read-Only Transactions for Queries**: Annotate read endpoints with `@Transactional(readOnly = true)` to save 50% memory by eliminating Hibernate snapshot allocations.
4. **Prefer Records and DTOs Over Managed Entities**: Fetch only the exact fields your API requires; never serialize raw Hibernate entities directly to JSON.
5. **Standardize on ReentrantLock in Java 21**: Avoid `synchronized` blocks in any code path that can execute on a Virtual Thread to eliminate carrier thread pinning.

---

## 7. Common Mistakes & Anti-Patterns Matrix

| Pitfall | Root Cause | Production Symptom | Architectural Remedy |
|---|---|---|---|
| **OSIV Connection Leaks** | `spring.jpa.open-in-view: true` | HikariCP pool exhausted while serializing JSON | Set `spring.jpa.open-in-view: false` |
| **JPA N+1 Queries** | Lazy/Eager collection traversal | Database CPU spike; latency $> 2\text{s}$ | Use `JOIN FETCH` or DTO projections |
| **Missing HTTP Timeouts** | Default un-timeouted client | All Tomcat worker threads hang (504s) | Configure strict 1s connect / 3s read timeouts |
| **Oversized DB Pool** | `maximum-pool-size: 200` | Database server CPU 100% context switching | Apply $(\text{Cores} \times 2) + \text{Spindles}$ formula |
| **Virtual Thread Pinning**| `synchronized` around I/O | Carrier threads blocked; virtual threads freeze | Replace with `ReentrantLock` |
| **Eager String Concatenation**| `log.debug("A" + b)` | Rapid heap allocation churn and GC pauses | Use SLF4J parameterized syntax `{}` |
| **Static Memory Leaks** | Unbounded `Map` or `List` | Slow heap creep; eventual OOMKilled 137 | Use bounded Caffeine cache with TTL |
| **Proxy Self-Invocation** | Direct `this.method()` call | Transactions or caches silently ignored | Inject proxy or extract to separate bean |
| **Unbounded @Async** | Unconstrained thread queue | OutOfMemoryError under sudden load bursts | Use bounded queue with `CallerRunsPolicy` |
| **Heap File Uploads** | Large `MultipartFile` in memory | Memory spike during concurrent uploads | Cap upload limits; stream directly to S3 |

---

## 8. Performance & Security Notes

### Cascade Failure Amplification
- A single un-timeouted client call inside a service can cascade across the entire enterprise topology.
- When Service A blocks waiting on Service B, Service A's worker threads saturate. Upstream Service C then blocks waiting on Service A, causing a complete cluster-wide domino collapse.
- Always pair timeouts with **Resilience4j Circuit Breakers** to fail fast when downstream dependencies degrade.

---

## 9. Comparison / Architectural Impact Matrix

| Pitfall | Latency Impact | Memory Impact | Availability Risk | Difficulty to Fix |
|---|---|---|---|---|
| **1. OSIV Connection Leaks** | High (50ms–500ms) | Low | **Critical (Pool Starvation)** | Very Easy (1 config line) |
| **2. JPA N+1 Queries** | **Extreme (1s–10s)** | High (Heap churn) | High (DB Saturation) | Medium (Refactor queries) |
| **3. Missing HTTP Timeouts**| **Catastrophic (Hang)** | High (Thread stacks) | **Fatal (Total Service Outage)**| Easy (Client customizer) |
| **4. Oversized DB Pool** | High (Jitter/p99) | High (DB server) | High (DB Thrashing) | Easy (Formula sizing) |
| **5. Virtual Thread Pinning**| High (Thread stall) | Low | High (JVM Freeze) | Easy (`ReentrantLock`) |
| **6. Eager String Logging** | Low (Microseconds) | **High (GC Churn)** | Low (GC Pauses) | Very Easy (Syntax cleanup) |
| **7. Static Memory Leaks** | None initially | **Catastrophic (OOM)** | **Fatal (Crash exit 137)** | Medium (Heap dump / MAT) |
| **8. Proxy Self-Invocation** | Variable | Low | High (Data Corruption) | Easy (Extract Bean) |
| **9. Unbounded @Async** | Variable | **Catastrophic (OOM)** | **Fatal (Crash under load)**| Easy (Bounded Executor) |
| **10. Heap File Uploads** | Variable | **Extreme (Heap Spike)**| **Critical (OOM on burst)** | Medium (Stream / S3) |

---

## 10. Interview Questions & Answers

### Q1: What is Open-Session-In-View (OSIV) and why is it considered a major anti-pattern?
<details>
<summary>Click to view answer</summary>

OSIV is a Spring Boot pattern that binds a Hibernate `Session` to the entire lifecycle of an HTTP request. It was created so developers could access lazy-loaded entity associations inside JSP or Thymeleaf views without encountering `LazyInitializationException`.  
In REST microservices, OSIV keeps physical database connections checked out from HikariCP while the response is serialized to JSON. Under slow network clients or high concurrency, database connections are held idle, starving the pool and crashing the service. It should always be disabled via `spring.jpa.open-in-view=false`.
</details>

### Q2: Why does calling a `@Transactional` or `@Cacheable` method from within the same class fail to work?
<details>
<summary>Click to view answer</summary>

Spring implements declarative annotations using Aspect-Oriented Programming (AOP) dynamic proxies. When an external caller invokes a bean, the call passes through the proxy, which begins the transaction or checks the cache before delegating to the target method. When a method calls another method on `this`, the invocation bypasses the proxy entirely, executing the raw Java method without any transactional or caching interception.
</details>

### Q3: How do missing HTTP timeouts cause cascading service failures in microservices?
<details>
<summary>Click to view answer</summary>

If an outbound HTTP client (like `RestClient` or `RestTemplate`) lacks connect and read timeouts, it defaults to waiting indefinitely for TCP packets. If a downstream dependency hangs or experiences network degradation, each calling worker thread will block indefinitely. As incoming traffic continues, all container worker threads (e.g. 200 Tomcat threads) become stuck waiting. Upstream callers then time out, cascading the failure through the entire microservice ecosystem.
</details>

### Q4: How does `CallerRunsPolicy` provide backpressure in Spring's `ThreadPoolTaskExecutor`?
<details>
<summary>Click to view answer</summary>

When a thread pool's worker threads are all busy and its task queue is completely full, a rejection policy must handle newly submitted tasks. The default policy (`AbortPolicy`) throws a `RejectedExecutionException`. `CallerRunsPolicy` forces the **calling thread** (e.g. the web request thread) to execute the task itself. This naturally throttles the incoming request rate because the web thread cannot accept new requests while it is busy executing the rejected task, creating natural backpressure.
</details>

### Q5: Why is parameter logging (`{}`) faster than string concatenation even if the log statement is NOT printed?
<details>
<summary>Click to view answer</summary>

String concatenation (`"User: " + user.getId()`) allocates a `StringBuilder` and intermediate string objects in memory before the method is invoked, incurring memory allocation and CPU overhead regardless of log level. Parameterized logging (`log.debug("User: {}", user.getId())`) checks whether the logger level is enabled *first*. If disabled, it returns immediately without allocating strings or calling `toString()`.
</details>

### Q6: What is the N+1 problem and how can you prove it is happening during integration testing?
<details>
<summary>Click to view answer</summary>

The N+1 problem occurs when fetching $N$ parent entities triggers $N$ additional separate database queries to fetch child relationships. In testing, it can be detected by:
1. Enabling `spring.jpa.properties.hibernate.generate_statistics=true` and checking query counts.
2. Using automated testing libraries like **QuickPerf** with `@ExpectSelect(1)` annotations.
3. Enabling SQL logging in test environments and counting emitted `SELECT` statements.
</details>

### Q7: How do you identify an in-memory memory leak caused by an unbounded collection?
<details>
<summary>Click to view answer</summary>

Monitor the JVM heap metric `jvm_memory_used_bytes{area="heap", id="*Old Gen*"}` in Grafana. If the baseline memory usage after major GC cycles steadily climbs over days without stabilizing, capture a heap dump (`jcmd <PID> GC.heap_dump /tmp/dump.hprof`) and open it in Eclipse Memory Analyzer (MAT). Run the **Leak Suspects Report**; the dominator tree will immediately point to the static collection holding retained references.
</details>

### Q8: What happens if a Virtual Thread encounters a `synchronized` block in Java 21?
<details>
<summary>Click to view answer</summary>

If a Virtual Thread enters a `synchronized` block or method and performs a blocking operation (like socket read or sleep), it cannot unmount from its underlying OS carrier thread. This is called **carrier thread pinning**. The carrier thread remains frozen and unable to execute other virtual threads until the blocking operation completes. Replacing `synchronized` with `ReentrantLock` allows the virtual thread to unmount cleanly.
</details>

---

## 11. Practice Exercises

### Exercise 1: Disable OSIV and Verify Repository Fetches (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Disable OSIV in `application.yml` and write a repository method using `JOIN FETCH` to prevent `LazyInitializationException` when accessing customer orders in a controller.

**Solution**:
```yaml
# application.yml
spring:
  jpa:
    open-in-view: false
```
```java
@Repository
public interface CustomerRepository extends JpaRepository<Customer, Long> {
    // Eagerly fetches orders in a single SQL query
    @Query("SELECT c FROM Customer c LEFT JOIN FETCH c.orders WHERE c.id = :id")
    Optional<Customer> findByIdWithOrders(@Param("id") Long id);
}
```
</details>

### Exercise 2: Fix Self-Invocation Cache Bug (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: The following service contains a self-invocation bug where `@Cacheable` is ignored. Refactor the class to ensure caching is properly intercepted.

```java
// Buggy Code
@Service
public class OrderService {
    public Order getOrder(Long id) {
        return fetchFromDb(id); // BUG: Self-invocation bypasses Spring proxy!
    }

    @Cacheable(value = "orders", key = "#id")
    public Order fetchFromDb(Long id) {
        // Slow DB call
        return new Order(id);
    }
}
```

**Solution**:
```java
// Corrected Code: Extract cached query to dedicated component
@Component
public class OrderCacheReader {

    @Cacheable(value = "orders", key = "#id")
    public Order fetchFromDb(Long id) {
        // Intercepted properly by Spring AOP proxy
        return new Order(id);
    }
}

@Service
public class OrderService {

    private final OrderCacheReader cacheReader;

    public OrderService(OrderCacheReader cacheReader) {
        this.cacheReader = cacheReader;
    }

    public Order getOrder(Long id) {
        // Passes through proxy cleanly!
        return cacheReader.fetchFromDb(id);
    }
}
```
</details>

### Exercise 3: Complete Resilient Outbound RestClient with Timeouts and Circuit Breaker (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Configure a production-grade `RestClient` bean with a 1-second connect timeout, 2-second read timeout, connection pool of 50, and wrapped in a Resilience4j Circuit Breaker.

**Solution**:
```java
package com.example.performance.pitfalls;

import io.github.resilience4j.circuitbreaker.annotation.CircuitBreaker;
import org.apache.hc.client5.http.impl.classic.CloseableHttpClient;
import org.apache.hc.client5.http.impl.classic.HttpClients;
import org.apache.hc.client5.http.impl.io.PoolingHttpClientConnectionManager;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.http.client.HttpComponentsClientHttpRequestFactory;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

import java.time.Duration;

@Configuration
public class ResilientClientConfig {

    @Bean
    public RestClient resilientPartnerClient() {
        PoolingHttpClientConnectionManager connectionManager = new PoolingHttpClientConnectionManager();
        connectionManager.setMaxTotal(50);
        connectionManager.setDefaultMaxPerRoute(20);

        CloseableHttpClient httpClient = HttpClients.custom()
            .setConnectionManager(connectionManager)
            .build();

        HttpComponentsClientHttpRequestFactory requestFactory = 
            new HttpComponentsClientHttpRequestFactory(httpClient);
        requestFactory.setConnectTimeout(Duration.ofSeconds(1));
        requestFactory.setConnectionRequestTimeout(Duration.ofSeconds(2));

        return RestClient.builder()
            .requestFactory(requestFactory)
            .baseUrl("https://partner.api")
            .build();
    }
}

@Service
public class PartnerCallingService {

    private final RestClient partnerClient;

    public PartnerCallingService(RestClient resilientPartnerClient) {
        this.partnerClient = resilientPartnerClient;
    }

    @CircuitBreaker(name = "partnerService", fallbackMethod = "fallbackResponse")
    public String executeRemoteCall() {
        return partnerClient.get()
            .uri("/status")
            .retrieve()
            .body(String.class);
    }

    public String fallbackResponse(Throwable t) {
        return "FALLBACK_CACHED_STATUS";
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Pitfall                                     | Golden Production Rule                                      |
+---------------------------------------------+-------------------------------------------------------------+
| 1. OSIV Connection Leaks                    | spring.jpa.open-in-view: false (Always disable)             |
| 2. JPA N+1 Queries                          | Use JOIN FETCH, @EntityGraph, or Java Record DTO projections|
| 3. Missing HTTP Timeouts                    | Never create clients without explicit connect & read timeouts|
| 4. HikariCP Pool Sizing Fallacy             | Size pool close to (Cores * 2) + Spindles (10-30 conns)     |
| 5. Virtual Thread Pinning                   | Replace synchronized blocks with ReentrantLock in Java 21   |
| 6. Eager String Concatenation               | Log with SLF4J parameterized placeholders: log.info("{}", v)|
| 7. Unbounded In-Memory Collections          | Always use bounded caches (Caffeine) with max size and TTL  |
| 8. Self-Invocation Proxy Bypass             | Never call @Cacheable/@Async/@Transactional on 'this'       |
| 9. Unbounded @Async Thread Pools            | Bounded queue capacity with CallerRunsPolicy backpressure   |
| 10. In-Memory File Buffering                | Stream large uploads directly to object storage (S3)        |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Vlad Mihalcea: High-Performance Java Persistence Pitfalls
- Brett Wooldridge: HikariCP Connection Pool Best Practices
- Spring Framework Documentation: Aspect Oriented Programming & Self-Invocation
- Netflix Technology Blog: Fault Tolerance & Resilient Architecture Patterns