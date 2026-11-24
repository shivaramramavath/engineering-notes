# 02: Designing Scalable REST APIs to 100k QPS with Spring Boot

| Metadata Field | Value |
|---|---|
| **Topic** | High-Throughput REST Architecture & Scaling Patterns |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Scale** | 100,000 QPS, $p99 < 25\text{ms}$ |
| **Key Middleware** | Nginx/HAProxy, Redis Cluster, PostgreSQL Read Replicas, Spring Boot Virtual Threads |

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

Serving 100,000 Queries Per Second (QPS) with sub-30 millisecond $p99$ response times is a common benchmark for large-scale enterprise backends (e.g., e-commerce product catalogs, social media timelines, payment verification). 

A single Spring Boot instance running on standard cloud hardware (e.g., 4 vCPU, 8 GB RAM) cannot handle 100k QPS alone; it typically saturates around 2,000–8,000 QPS depending on database I/O and payload serialization costs. Reaching 100k QPS requires horizontal scaling, multi-tier caching (Edge CDN, distributed Redis Cluster, and local JVM Guava/Caffeine caches), read-replica database splitting, non-blocking thread scheduling (Java 21 Virtual Threads), and HTTP connection pooling optimization.

---

## 2. Core Concepts

### 2.1 The Multi-Tier Caching Funnel
To protect backend databases, traffic must be absorbed at outer tiers:
1. **Edge CDN (CloudFront / Cloudflare)**: Absorbs 60–80% of cacheable read requests ($p99 \sim 5\text{ms}$).
2. **L7 Reverse Proxy / Ingress (Nginx / Envoy / AWS ALB)**: Manages TLS termination, HTTP/2 multiplexing, keep-alive connections, and round-robin load balancing.
3. **Local In-Memory Cache (Caffeine in JVM)**: Absorbs ultra-hot metadata ($p99 < 1\mu\text{s}$) with 5–60 second TTLs.
4. **Distributed Cache (Redis Cluster)**: Centralized, shared session and entity state ($p99 \sim 1\text{ms}$).
5. **Database Read Replicas (PostgreSQL / MySQL Aurora)**: Distributed read queries routed via Spring `@Transactional(readOnly = true)`.

### 2.2 Horizontal Pod Autoscaling (HPA) Math
If a single optimized Spring Boot node comfortably sustains 4,000 QPS at 65% CPU utilization:
$$\text{Required Pods} = \frac{100,000\text{ QPS}}{4,000\text{ QPS/Pod}} = 25\text{ Pods}$$
With a $1.5\times$ safety margin for sudden traffic spikes: **38 to 40 Pods**.

---

## 3. Internals & Architecture

```
                       [ 100,000 Inbound HTTP Requests / sec ]
                                          │
                                          ▼
                       ┌──────────────────────────────────────┐
                       │       CloudFront / Fastly CDN        │
                       │   (Cached Static & Product Catalog)  │
                       └──────────────────┬───────────────────┘
                                          │ (Misses: ~30k QPS)
                                          ▼
                       ┌──────────────────────────────────────┐
                       │       AWS ALB / Nginx Ingress        │
                       │   - TLS 1.3 Termination, HTTP/2      │
                       │   - Round-robin across K8s cluster   │
                       └──────────────────┬───────────────────┘
                                          │
                  ┌───────────────────────┴───────────────────────┐
                  ▼                                               ▼
   ┌─────────────────────────────┐                 ┌─────────────────────────────┐
   │ Spring Boot Pod 1           │  . . . . . . .  │ Spring Boot Pod 40          │
   │ - Java 21 Virtual Threads   │                 │ - Java 21 Virtual Threads   │
   │ - Caffeine L1 Hot Cache     │                 │ - Caffeine L1 Hot Cache     │
   │ - HikariCP Pool (30 conns)  │                 │ - HikariCP Pool (30 conns)  │
   └──────────────┬──────────────┘                 └──────────────┬──────────────┘
                  │                                               │
                  ├──────────────────────┬────────────────────────┤
                  ▼                      ▼                        ▼
   ┌─────────────────────────────┐ ┌────────────────────────┐ ┌──────────────────┐
   │ Redis Cluster (L2 Cache)    │ │ PostgreSQL Primary DB  │ │ PostgreSQL Read  │
   │ - 3 Masters, 3 Replicas     │ │ - Writes only          │ │ Replicas (x3)    │
   │ - 150k Ops/sec throughput   │ │ - Logical Replication ─┼─> - Read queries   │
   └─────────────────────────────┘ └────────────────────────┘ └──────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Spring Dynamic Read/Write Routing DataSource
Spring provides `AbstractRoutingDataSource` to determine at runtime whether to direct SQL queries to the Primary DB or the Read Replicas based on transaction context:

```java
public class ReadWriteRoutingDataSource extends AbstractRoutingDataSource {
    @Override
    protected Object determineCurrentLookupKey() {
        return TransactionSynchronizationManager.isCurrentTransactionReadOnly() 
                ? DataSourceType.READ_REPLICA 
                : DataSourceType.PRIMARY;
    }
}
```

---

## 5. 5 Practical Examples

### Example 1: Multi-Tier Cache with Caffeine (L1) and Redis (L2)

```java
package com.example.scaling.cache;

import com.github.benmanes.caffeine.cache.Caffeine;
import org.springframework.cache.CacheManager;
import org.springframework.cache.annotation.EnableCaching;
import org.springframework.cache.caffeine.CaffeineCacheManager;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Primary;
import org.springframework.data.redis.cache.RedisCacheConfiguration;
import org.springframework.data.redis.cache.RedisCacheManager;
import org.springframework.data.redis.connection.RedisConnectionFactory;

import java.time.Duration;

@Configuration
@EnableCaching
public class TwoTierCacheConfig {

    @Bean
    @Primary
    public CacheManager caffeineCacheManager() {
        CaffeineCacheManager manager = new CaffeineCacheManager("l1-hot-products");
        manager.setCaffeine(Caffeine.newBuilder()
                .maximumSize(10_000)
                .expireAfterWrite(Duration.ofSeconds(30))
                .recordStats());
        return manager;
    }

    @Bean
    public CacheManager redisCacheManager(RedisConnectionFactory connectionFactory) {
        RedisCacheConfiguration config = RedisCacheConfiguration.defaultCacheConfig()
                .entryTtl(Duration.ofMinutes(15))
                .disableCachingNullValues();
        return RedisCacheManager.builder(connectionFactory)
                .cacheDefaults(config)
                .build();
    }
}
```

### Example 2: Dynamic Primary/Replica DataSource Router

```java
package com.example.scaling.datasource;

import org.springframework.jdbc.datasource.lookup.AbstractRoutingDataSource;
import org.springframework.transaction.support.TransactionSynchronizationManager;

import javax.sql.DataSource;
import java.util.Map;

public class ReplicaRoutingDataSource extends AbstractRoutingDataSource {

    public enum RouteTarget { PRIMARY, REPLICA }

    public ReplicaRoutingDataSource(DataSource primary, DataSource replica) {
        setTargetDataSources(Map.of(
                RouteTarget.PRIMARY, primary,
                RouteTarget.REPLICA, replica
        ));
        setDefaultTargetDataSource(primary);
    }

    @Override
    protected Object determineCurrentLookupKey() {
        boolean isReadOnly = TransactionSynchronizationManager.isCurrentTransactionReadOnly();
        return isReadOnly ? RouteTarget.REPLICA : RouteTarget.PRIMARY;
    }
}
```

### Example 3: High-Performance Product Catalog Service

```java
package com.example.scaling.service;

import com.example.scaling.domain.Product;
import com.example.scaling.repository.ProductRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.cache.annotation.Cacheable;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.UUID;

@Service
public class ProductCatalogService {

    private static final Logger log = LoggerFactory.getLogger(ProductCatalogService.class);
    private final ProductRepository productRepository;

    public ProductCatalogService(ProductRepository productRepository) {
        this.productRepository = productRepository;
    }

    // L1 cache absorbs repeated hot reads within 30 seconds
    @Cacheable(value = "l1-hot-products", key = "#id")
    @Transactional(readOnly = true)
    public Product getProductById(UUID id) {
        log.debug("L1 cache miss for product {}. Querying database read replica...", id);
        return productRepository.findById(id)
                .orElseThrow(() -> new IllegalArgumentException("Product not found: " + id));
    }
}
```

### Example 4: Non-Blocking WebMvc Virtual Threads Configuration

```yaml
# application.yaml
server:
  port: 8080
  tomcat:
    threads:
      max: 200 # Platform worker pool for virtual thread scheduling
    accept-count: 1000
    max-connections: 20000
spring:
  threads:
    virtual:
      enabled: true
  datasource:
    hikari:
      maximum-pool-size: 30
      minimum-idle: 10
      connection-timeout: 1500
```

### Example 5: HTTP Response Cache-Control Header Filter

```java
package com.example.scaling.filter;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.http.HttpHeaders;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;

@Component
public class CacheControlFilter extends OncePerRequestFilter {

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {

        if ("GET".equalsIgnoreCase(request.getMethod()) && request.getRequestURI().startsWith("/api/v1/products")) {
            // Instruct downstream CDN and Browser to cache response for 60 seconds
            response.setHeader(HttpHeaders.CACHE_CONTROL, "public, max-age=60, s-maxage=120, stale-while-revalidate=30");
        }
        filterChain.doFilter(request, response);
    }
}
```

---

## 6. Best Practices

- **Leverage HTTP/2 Multiplexing**: Ensure Edge proxies (ALB/Nginx) speak HTTP/2 or HTTP/3 to client apps, multiplexing multiple REST calls over a single persistent TCP connection.
- **Split Read and Write Transactions**: Annotate every read query with `@Transactional(readOnly = true)` to ensure Spring routes it directly to PostgreSQL read replicas.
- **Keep Application Containers Stateless**: Never store user sessions in local JVM memory. Use Redis session stores or stateless JWTs.
- **Set Up Gzip/Brotli Compression**: Compress payloads larger than 1 KB (`server.compression.enabled: true`) to reduce network transit time.
- **Configure Graceful Shutdown**: Set `server.shutdown: graceful` with a 30-second timeout so Kubernetes rolling updates drain in-flight requests without dropping connections.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Database Pool Oversizing** | Setting `maximumPoolSize=500` per pod across 40 pods = 20,000 DB connections, crashing the database engine. | Cap pool at 20–30 per pod, and use PgBouncer / RDS Proxy for connection multiplexing. |
| **Missing HTTP Cache-Control Headers** | Clients hit origin servers for unchanged resources, multiplying QPS unnecessarily. | Set `Cache-Control: public, max-age=60` for public catalog endpoints. |
| **Unindexed Foreign Key Queries** | At 100k QPS, unindexed table scans lock tables and exhaust database CPU instantly. | Ensure all join and filter columns have appropriate B-Tree or Hash indexes. |
| **Unbounded Query Result Sets** | `SELECT * FROM items` returns 50,000 rows, exhausting JVM heap and triggering GC pauses. | Enforce hard pagination (`Pageable` limit $\le 100$). |
| **Synchronous Logging to Disk** | Synchronous file I/O blocks request threads under heavy concurrency. | Use Logback asynchronous appenders (`AsyncAppender`). |
| **No Circuit Breaker on Third Parties** | A slow 3rd-party tax/shipping API causes all 40 pods to exhaust their worker threads. | Wrap external calls in Resilience4j circuit breakers with 1s timeouts. |

---

## 8. Performance & Security Considerations

### 8.1 Logback Asynchronous Appender
Prevent disk I/O latency from stalling request threads:
```xml
<!-- logback-spring.xml -->
<configuration>
    <appender name="ASYNC_FILE" class="ch.qos.logback.classic.AsyncAppender">
        <queueSize>10000</queueSize>
        <discardingThreshold>20</discardingThreshold>
        <appender-ref ref="FILE" />
    </appender>
</configuration>
```

### 8.2 Rate Limiting at the Edge
At 100k QPS, enforce client IP and API key rate limits at Nginx or Cloudflare (e.g., 50 req/sec per token) to prevent volumetric DDoS.

---

## 9. Comparison / Decision Matrix

| Strategy | Implementation Complexity | Latency | Database Relief |
|---|---|---|---|
| **Edge CDN Caching** | Low (DNS/CDN configuration) | $5\text{ms}$ | 80% traffic offload |
| **JVM Caffeine (L1)** | Low (Spring Cache annotation) | $< 1\mu\text{s}$ | Eliminates network hop to Redis |
| **Redis Cluster (L2)** | Medium (Cluster setup, serialization) | $1-2\text{ms}$ | Centralized, synchronized across all pods |
| **DB Read Replicas** | High (Replication lag management) | $5-15\text{ms}$ | Offloads read queries from Primary DB |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. How many Spring Boot instances do you need to handle 100,000 QPS?</strong></summary>

Assuming each 4 vCPU container handles 3,000–4,000 QPS with Virtual Threads and L1/L2 caching, you need approximately 25 to 35 pods. Accounting for a $1.5\times$ peak traffic headroom, plan for 40 pods managed via Kubernetes Horizontal Pod Autoscaler (HPA).
</details>

<details>
<summary><strong>2. How does `AbstractRoutingDataSource` determine whether to send a query to primary or replica?</strong></summary>

It queries Spring's `TransactionSynchronizationManager.isCurrentTransactionReadOnly()`. If `@Transactional(readOnly = true)` is active, it returns the lookup key for the read-replica DataSource; otherwise, it returns the primary DataSource.
</details>

<details>
<summary><strong>3. What is replication lag, and how do you handle it after a user writes data?</strong></summary>

Replication lag is the delay between a write hitting the primary database and being reflected on read replicas. To prevent "read-your-own-writes" anomalies, serve user profile or checkout reads from the primary database for 5–10 seconds following a write (or use a cookie/header tracking recent write timestamps).
</details>

<details>
<summary><strong>4. Why is PgBouncer or AWS RDS Proxy necessary when scaling Spring Boot horizontally?</strong></summary>

When scaling to 50 pods with 30 connections each, 1,500 persistent database connections are established. PostgreSQL allocates memory per backend process ($\sim 5-10\text{ MB}$). A connection pooler like PgBouncer multiplexes thousands of application connections over a small pool of active database connections.
</details>

<details>
<summary><strong>5. What is the benefit of Java 21 Virtual Threads over traditional thread-per-request?</strong></summary>

Traditional threads map 1:1 to OS kernel threads, consuming $\sim 1\text{ MB}$ stack memory. When threads block on DB/network I/O, OS threads remain idle. Virtual Threads are managed by the JVM, consume only a few hundred bytes, and unmount from carrier threads during blocking I/O, allowing tens of thousands of concurrent connections.
</details>

<details>
<summary><strong>6. How do you prevent a cache stampede when a hot product record expires?</strong></summary>

Use a distributed mutex lock in Redis (`SETNX`) so only one pod queries the database to re-populate the cache while other pods wait or return slightly stale data.
</details>

<details>
<summary><strong>7. How does HTTP/2 improve REST API performance compared to HTTP/1.1?</strong></summary>

HTTP/2 allows multiple bidirectional requests and responses over a single TCP connection (multiplexing), eliminating head-of-line blocking and reducing TCP handshake latency.
</details>

<details>
<summary><strong>8. What metrics indicate a Spring Boot application is bottlenecked?</strong></summary>

Key indicators: HikariCP `ConnectionTimeoutException`, elevated JVM garbage collection pause times ($>100\text{ms}$), Tomcat `currentThreadsBusy` reaching max capacity, and database CPU exceeding 80%.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Dual DataSource with Spring Boot</strong></summary>

**Prompt**: Implement a configuration class defining `primaryDataSource`, `replicaDataSource`, and a `ReplicaRoutingDataSource` bean.

**Solution**:
Inspect Example 2: register both HikariDataSources, wrap them in `ReplicaRoutingDataSource`, and expose it as `@Primary DataSource`.
</details>

<details>
<summary><strong>Exercise 2: Write a Cache-Aside Method with Jitter</strong></summary>

**Prompt**: Write a Java service method fetching a user from Redis, calculating a random jitter between 1 and 5 minutes on cache miss, and persisting back to Redis.

**Solution**:
```java
public User getUser(String id) {
    User u = redis.get(id);
    if (u != null) return u;
    u = db.find(id);
    int jitter = ThreadLocalRandom.current().nextInt(60, 300);
    redis.set(id, u, Duration.ofSeconds(900 + jitter));
    return u;
}
```
</details>

<details>
<summary><strong>Exercise 3: Configure Spring Boot Graceful Shutdown</strong></summary>

**Prompt**: Configure `application.yaml` to gracefully handle SIGTERM signals in Kubernetes without dropping active requests.

**Solution**:
```yaml
server:
  shutdown: graceful
spring:
  lifecycle:
    timeout-per-shutdown-phase: 30s
```
</details>

<details>
<summary><strong>Exercise 4: Benchmark Virtual Threads vs Platform Threads</strong></summary>

**Prompt**: Use `wrk` or `hey` to benchmark a Spring Boot endpoint that performs a 50ms simulated database sleep.

**Solution**:
Run: `hey -n 50000 -c 1000 http://localhost:8080/api/v1/test`. Virtual threads will sustain 1,000 concurrent connections with negligible memory overhead compared to platform threads.
</details>

<details>
<summary><strong>Exercise 5: Implement Response Compression</strong></summary>

**Prompt**: Enable Gzip compression in Spring Boot for JSON responses larger than 2KB.

**Solution**:
```yaml
server:
  compression:
    enabled: true
    mime-types: application/json,text/plain
    min-response-size: 2048
```
</details>

<details>
<summary><strong>Exercise 6: Configure Kubernetes HPA Manifest</strong></summary>

**Prompt**: Write a Kubernetes HorizontalPodAutoscaler YAML targeting 65% average CPU utilization between 10 and 50 replicas.

**Solution**:
```yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: spring-boot-api-hpa
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: spring-boot-api
  minReplicas: 10
  maxReplicas: 50
  metrics:
  - type: Resource
    resource:
      name: cpu
      target:
        type: Utilization
        averageUtilization: 65
```
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   SCALING TO 100k QPS CHEATSHEET                       │
├────────────────────────────────────────────────────────────────────────┤
│ 1. CDN (CloudFront): Absorbs 70%+ cacheable traffic                    │
│ 2. L1/L2 Cache: Caffeine (in-JVM) + Redis Cluster (Distributed)        │
│ 3. Database: Primary (Writes) + 3-4 Read Replicas via RoutingDataSource│
│ 4. Pooling: PgBouncer / RDS Proxy to multiplex database connections    │
│ 5. Concurrency: Spring Boot 3.4 + Java 21 Virtual Threads              │
│ 6. Pod Sizing: ~40 Pods across K8s cluster with HPA (min 10, max 50)   │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring Boot 3 Web Applications Reference](https://docs.spring.io/spring-boot/reference/web/index.html)
- [Caffeine High-Performance Caching Library](https://github.com/ben-manes/caffeine)
- [HikariCP Connection Pool Best Practices](https://github.com/brettwooldridge/HikariCP)
- [AWS Aurora Read Replica Scaling Guide](https://aws.amazon.com/rds/aurora/)
