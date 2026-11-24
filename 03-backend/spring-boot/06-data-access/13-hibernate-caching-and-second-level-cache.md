# Hibernate Caching & Second-Level Cache (L2C): Architecture, Redisson & Query Cache

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | `03-spring-data-jpa-fundamentals.md`, Redis architecture (`03-backend/nodejs/07-databases/redis`), Caching strategies |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Hibernate 6.6.x, Redisson 3.37+, Redis 7.4 / Valkey, PostgreSQL 16 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Configuring Hibernate Second-Level Cache with Redisson and Redis](#51-configuring-hibernate-second-level-cache-with-redisson-and-redis)
   - [5.2 Entity Caching with Cache Concurrency Strategies](#52-entity-caching-with-cache-concurrency-strategies)
   - [5.3 Query Result Caching with StandardQueryCache](#53-query-result-caching-with-standardquerycache)
   - [5.4 Anti-Pattern: Caching High-Write Volatile Entities](#54-anti-pattern-caching-high-write-volatile-entities)
   - [5.5 Cache Eviction and Metrics Monitoring with Micrometer](#55-cache-eviction-and-metrics-monitoring-with-micrometer)
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
In read-heavy enterprise applications, repeatedly querying the database for static or infrequently modified data (e.g., country codes, product catalogs, permissions, tenant configurations) consumes substantial database CPU cycles, exhausts connection pools, and introduces unnecessary network latency. While Spring's `@Cacheable` operates at the service method level, it caches coarse business objects and does not synchronize with low-level JPA entity mutations or handle fine-grained relational invalidations.

### Historical Evolution
Hibernate introduced a two-tier caching architecture:
1. **First-Level Cache (L1)**: Bound strictly to the thread-local `Session` / `EntityManager`. It cannot share data across transactions or application nodes.
2. **Second-Level Cache (L2C)**: Process- or cluster-scoped cache shared across all sessions and application instances (via providers like Ehcache, Infinispan, or **Redisson / Redis**).
In Hibernate 6.x, the caching SPI was overhauled to support the Semantic Query Model (SQM) and non-blocking cluster invalidation, allowing distributed microservices to share cached entities with strict consistency.

### Real-World Relevance
For globally distributed or high-throughput microservices reading reference data thousands of times per second, L2C cuts database load by up to 95%. When an entity is modified anywhere in the cluster, Hibernate automatically invalidates or updates the distributed cache entry, eliminating stale read inconsistencies.

---

## 2. Core Concepts

### First-Level vs Second-Level Cache
- **First-Level Cache (L1)**: Mandatory, in-memory, session-scoped. Automatically enabled for every transaction. Destroyed when the transaction or session ends.
- **Second-Level Cache (L2C)**: Optional, shared across sessions and nodes. Persists beyond individual transaction lifecycles.

### Cache Concurrency Strategies
Configured via `@org.hibernate.annotations.Cache(usage = ...)`:
1. `READ_ONLY`: For immutable data that never changes. Fastest performance, no locking.
2. `NONSTRICT_READ_WRITE`: For data that is updated rarely. Does not guarantee strict isolation; small window of stale reads during concurrent writes.
3. `READ_WRITE`: Uses soft locks to maintain read-committed isolation. Recommended for read-heavy entities with occasional updates.
4. `TRANSACTIONAL`: Supports JTA distributed transactions (typically with Infinispan).

### Query Cache
Stores the results of JPQL queries (specifically, a list of matching entity primary keys). When the query runs again with the same parameters, Hibernate fetches the IDs from the query cache and then hydrates the entities from the L2C entity cache. **Warning**: If any row in the queried table is updated, Hibernate invalidates the entire query cache region for that table.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         Hibernate Two-Tier Caching Pipeline                        |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  EntityManager.find(User.class, 1L)                                                |
|         |                                                                          |
|         v                                                                          |
|  [ 1. Check First-Level Cache (L1) ] ----------------> Found? -> Return Entity     |
|         | Not Found                                                                |
|         v                                                                          |
|  [ 2. Check Second-Level Cache (L2C) ] --------------> Found? -> Hydrate & Return  |
|     (In-Memory / Distributed Redis via Redisson)                                   |
|         | Not Found                                                                |
|         v                                                                          |
|  [ 3. Execute SQL Query on Database ]                                              |
|     SELECT id, name, email FROM users WHERE id = 1;                                 |
|         |                                                                          |
|         +---> Store in L1 Cache (Session Scope)                                    |
|         +---> Store in L2C Cache (Cluster Scope via Redisson Region)               |
|         v                                                                          |
|  Return Entity to Application                                                      |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

1. **Hydration**: The L2C does not store live Java object instances with active proxies. It stores **disassembled state arrays** (primitive values, strings, IDs). When retrieved from L2C, Hibernate "hydrates" a new entity instance and places it into the calling session's L1 cache.
2. **Invalidation**: When an entity is updated via `em.merge()` or dirty checking, Hibernate sends an eviction command to the L2C provider (e.g., Redis `DEL` or soft-lock message) to ensure other nodes invalidate their local copies.

---

## 4. Syntax & Usage

### Maven Dependencies for Hibernate L2C with Redisson
```xml
<dependency>
    <groupId>org.redisson</groupId>
    <artifactId>redisson-hibernate-6</artifactId>
    <version>3.37.0</version>
</dependency>
```

### Entity Cache Annotation
```java
@Entity
@Table(name = "countries")
@org.hibernate.annotations.Cache(
    usage = CacheConcurrencyStrategy.READ_ONLY, 
    region = "reference-data"
)
public class Country {
    @Id
    private String code;
    private String name;
}
```

---

## 5. Practical Examples

### 5.1 Configuring Hibernate Second-Level Cache with Redisson and Redis
```yaml
# application.yaml - Production Hibernate L2C Configuration
spring:
  jpa:
    properties:
      hibernate:
        # Enable Second-Level and Query Cache
        cache:
          use_second_level_cache: true
          use_query_cache: true
          region:
            factory_class: org.redisson.hibernate.RedissonRegionFactory
        # Redisson configuration file path
        javax:
          persistence:
            sharedCache:
              mode: ENABLE_SELECTIVE # Only cache entities annotated with @Cacheable / @Cache
      redisson:
        file: classpath:redisson.yaml
```

```yaml
# redisson.yaml - Redis Connection Settings for Hibernate L2C
singleServerConfig:
  address: "redis://127.0.0.1:6379"
  connectionMinimumIdleSize: 10
  connectionPoolSize: 32
  idleConnectionTimeout: 10000
  timeout: 3000
```

### 5.2 Entity Caching with Cache Concurrency Strategies
```java
package com.example.jpa.domain;

import jakarta.persistence.*;
import org.hibernate.annotations.Cache;
import org.hibernate.annotations.CacheConcurrencyStrategy;

import java.math.BigDecimal;
import java.time.Instant;

@Entity
@Table(name = "products")
@Cacheable
@Cache(usage = CacheConcurrencyStrategy.READ_WRITE, region = "product-cache")
public class Product {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false, unique = true)
    private String sku;

    @Column(nullable = false)
    private String name;

    @Column(nullable = false)
    private BigDecimal basePrice;

    @Column(nullable = false)
    private Instant lastUpdated = Instant.now();

    protected Product() {}

    public Product(String sku, String name, BigDecimal basePrice) {
        this.sku = sku;
        this.name = name;
        this.basePrice = basePrice;
    }

    public void updatePrice(BigDecimal newPrice) {
        this.basePrice = newPrice;
        this.lastUpdated = Instant.now();
    }

    public Long getId() { return id; }
    public String getSku() { return sku; }
    public BigDecimal getBasePrice() { return basePrice; }
}
```

### 5.3 Query Result Caching with StandardQueryCache
```java
package com.example.jpa.repository;

import com.example.jpa.domain.Product;
import jakarta.persistence.QueryHint;
import org.hibernate.jpa.HibernateHints;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.jpa.repository.QueryHints;
import org.springframework.data.repository.query.Param;

import java.math.BigDecimal;
import java.util.List;

public interface ProductRepository extends JpaRepository<Product, Long> {

    /**
     * Caches the list of Product IDs matching this query in the Query Cache.
     * Subsequent invocations with the same maxPrice skip SQL execution!
     */
    @QueryHints({
        @QueryHint(name = HibernateHints.HINT_CACHEABLE, value = "true"),
        @QueryHint(name = HibernateHints.HINT_CACHE_REGION, value = "product-query-cache")
    })
    @Query("SELECT p FROM Product p WHERE p.basePrice <= :maxPrice ORDER BY p.name ASC")
    List<Product> findAffordableProductsCached(@Param("maxPrice") BigDecimal maxPrice);
}
```

### 5.4 Anti-Pattern: Caching High-Write Volatile Entities
```java
// =========================================================================
// ANTI-PATTERN: Second-Level Caching on Frequently Updated Entities
// =========================================================================
@Entity
@Table(name = "stock_tickers")
@Cacheable
// DISASTER: This entity is updated 500 times per second!
// Every update triggers Redis eviction messages across all cluster nodes!
// Cache hit ratio approaches 0%, while network saturation and CPU spike 10x!
@Cache(usage = CacheConcurrencyStrategy.READ_WRITE)
public class VolatileStockTicker {
    @Id private String symbol;
    private BigDecimal currentPrice;
    private long volume;
}

// =========================================================================
// PRODUCTION FIX: Restrict L2C to Read-Mostly Reference Data
// =========================================================================
@Entity
@Table(name = "tax_jurisdictions")
@Cacheable
// EXCELLENT: Tax rates change once a year; read on every checkout!
// Hit ratio > 99.9%, saving millions of database queries!
@Cache(usage = CacheConcurrencyStrategy.READ_ONLY, region = "static-tax-rules")
public class TaxJurisdiction {
    @Id private String stateCode;
    private BigDecimal standardTaxRate;
}
```

### 5.5 Cache Eviction and Metrics Monitoring with Micrometer
```java
package com.example.jpa.monitoring;

import jakarta.persistence.EntityManagerFactory;
import org.hibernate.SessionFactory;
import org.hibernate.stat.CacheRegionStatistics;
import org.hibernate.stat.Statistics;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

@Component
public class HibernateCacheMonitor {

    private static final Logger log = LoggerFactory.getLogger(HibernateCacheMonitor.class);
    private final SessionFactory sessionFactory;

    public HibernateCacheMonitor(EntityManagerFactory entityManagerFactory) {
        this.sessionFactory = entityManagerFactory.unwrap(SessionFactory.class);
        // Enable statistics collection in production monitoring profiles
        this.sessionFactory.getStatistics().setStatisticsEnabled(true);
    }

    @Scheduled(fixedRate = 60000)
    public void logCachePerformance() {
        Statistics stats = sessionFactory.getStatistics();

        long hitCount = stats.getSecondLevelCacheHitCount();
        long missCount = stats.getSecondLevelCacheMissCount();
        long putCount = stats.getSecondLevelCachePutCount();

        double hitRatio = (hitCount + missCount == 0) ? 0.0 : 
            ((double) hitCount / (hitCount + missCount)) * 100.0;

        log.info("Hibernate L2C Stats: Hits={}, Misses={}, Puts={}, HitRatio={}%",
            hitCount, missCount, putCount, String.format("%.2f", hitRatio));

        CacheRegionStatistics productStats = stats.getDomainDataRegionStatistics("product-cache");
        if (productStats != null) {
            log.info("Product Region: Hits={}, Misses={}, ElementsInMemory={}",
                productStats.getHitCount(), 
                productStats.getMissCount(), 
                productStats.getElementCountInMemory());
        }
    }

    /**
     * Programmatic eviction when external bulk updates occur outside Hibernate.
     */
    public void evictProductRegion() {
        sessionFactory.getCache().evictRegion("product-cache");
        log.warn("Evicted 'product-cache' region from Hibernate L2C.");
    }
}
```

---

## 6. Best Practices

1. **Only Cache Read-Mostly Entities**: Apply L2C only to entities with read-to-write ratios of $50:1$ or higher (e.g., categories, geographic data, plan tiers).
2. **Use `READ_ONLY` for Immutable Data**: If an entity is never modified after insertion, use `CacheConcurrencyStrategy.READ_ONLY` to bypass distributed lock overhead.
3. **Always Set `sharedCache.mode = ENABLE_SELECTIVE`**: Never cache all entities indiscriminately; require explicit `@Cacheable` or `@Cache` declarations.
4. **Be Cautious with the Query Cache**: The query cache is fragile; any `INSERT`, `UPDATE`, or `DELETE` on a table invalidates *all* cached queries referencing that table. Use it only for static lookup queries.
5. **Always Configure Time-To-Live (TTL) in Distributed Caches**: In Redis/Redisson, set explicit TTLs (e.g., 1 hour) on cache regions to prevent stale data accumulation if an eviction message is lost.
6. **Monitor Hit Ratios via Micrometer / JMX**: An L2C hit ratio below $70\%$ indicates that an entity is being updated too frequently or the cache is undersized, wasting CPU.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Caching High-Update Entities** | Cache invalidation messages saturate network links and drop hit ratios to near-zero. | Remove `@Cache` from write-heavy transactional entities. |
| **Enabling Query Cache Without Entity Cache** | Query cache returns IDs, but Hibernate issues individual SQL queries to fetch each entity, creating an N+1 storm! | Always cache the underlying entity when enabling query caching. |
| **Updating Database via Direct SQL or Stored Procedures** | Hibernate L2C is unaware of out-of-band database changes, resulting in stale cached data. | Explicitly evict cache regions using `sessionFactory.getCache().evictRegion(...)`. |
| **Using `NONSTRICT_READ_WRITE` on High-Concurrency Updates** | Concurrency race conditions cause stale data to remain cached permanently. | Use `READ_WRITE` concurrency strategy. |
| **Storing Live Entity References in Cache** | Entities contain active session-bound proxies; serializing them causes `SerializationException`. | Ensure L2C provider serializes raw entity state arrays. |
| **Omitting Region Names** | All cached entities default to the global default region, making fine-grained eviction and TTL tuning impossible. | Always define explicit region names (`region = "product-cache"`). |

---

## 8. Performance & Security Notes

### Performance
- **Sub-Millisecond L2C Retrieval**: Reading an entity from an in-memory L2C (or local Redis cluster) takes under $0.5\text{ ms}$, compared to $10\text{--}50\text{ ms}$ for a relational database query.
- **Disassembled State Storage**: Hibernate strips entity instances down to Object arrays of primitives, keeping the memory footprint in Redis or heap memory minimal.

### Security
- **Multi-Tenant Cache Bleeding**: In multi-tenant systems using schema-based multi-tenancy, verify that the L2C provider prefixes cache keys with the tenant ID to prevent cross-tenant data leaks.
- **Cache Encryption**: Encrypt sensitive personal information (PII) before storage in distributed Redis cache nodes.

---

## 9. Comparison / When to Use What

| Caching Mechanism | Scope | Invalidation Strategy | Best Use Case |
| :--- | :--- | :--- | :--- |
| **First-Level Cache (L1)** | Single Session / Thread | Automatic on transaction end | Identity mapping within a single request |
| **Second-Level Cache (L2C)** | Application / Cluster | Automatic on Hibernate entity mutation | Read-heavy JPA entities (Products, Categories) |
| **Spring `@Cacheable`** | Service Method | Explicit annotations (`@CacheEvict`) | Complex service DTOs, external API results |
| **Query Cache** | Application / Cluster | Invalidation on any table modification | Repetitive queries with identical parameter sets |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between Hibernate's First-Level and Second-Level Cache?</strong></summary>

- **First-Level Cache (L1)** is bound strictly to the `Session` / `EntityManager` lifecycle. It is mandatory, cannot be disabled, and is destroyed when the transaction finishes.
- **Second-Level Cache (L2C)** is process- or cluster-wide. It is optional, lives across transaction and session boundaries, and can be distributed across multiple microservice nodes via providers like Redisson (Redis) or Hazelcast.
</details>

<details>
<summary><strong>2. What does the Hibernate Query Cache store?</strong></summary>

The Query Cache does *not* store fully materialized entity instances. It stores query parameter hashes paired with an array of matching entity primary keys (`[101, 102, 103]`). To hydrate the final entities, Hibernate looks up those IDs in the Second-Level Entity Cache. If the entities are not in the L2C, Hibernate executes a query to load them from the database.
</details>

<details>
<summary><strong>3. Why is the Query Cache considered fragile in write-heavy environments?</strong></summary>

Hibernate invalidates the entire query cache region for a table whenever *any* entity in that table is inserted, updated, or deleted. If a table experiences frequent writes, cached query results are constantly discarded before they can be reused, resulting in wasted memory and zero performance benefit.
</details>

<details>
<summary><strong>4. How does the <code>READ_WRITE</code> cache concurrency strategy maintain consistency during updates?</strong></summary>

`READ_WRITE` uses a "soft lock" mechanism. When a transaction updates an entity, Hibernate places a soft lock on the entity's cache key. Concurrent transactions attempting to read from L2C see the soft lock and fall back directly to reading from the database with read-committed isolation. Upon transaction commit, the updated state is written to the cache and the soft lock is released.
</details>

<details>
<summary><strong>5. What happens if a database row is updated directly via raw SQL or an external system?</strong></summary>

Hibernate's L2C is managed entirely within the application tier. If data is modified via direct SQL scripts, database stored procedures, or another application that does not coordinate with Hibernate, the L2C will serve stale data. The cache must be explicitly evicted using `SessionFactory.getCache().evictRegion(...)`.
</details>

<details>
<summary><strong>6. What is the difference between Spring's <code>@Cacheable</code> and Hibernate's <code>@Cache</code>?</strong></summary>

- Spring's `@Cacheable` operates as an AOP proxy around service methods, caching the returned business DTO or result. It is completely agnostic of JPA or database tables.
- Hibernate's `@Cache` operates directly inside the ORM engine, intercepting `em.find()` and relationship loading. It understands entity identity and automatically invalidates entries when the entity is mutated via JPA.
</details>

<details>
<summary><strong>7. What does <code>sharedCache.mode = ENABLE_SELECTIVE</code> do?</strong></summary>

It configures JPA to cache only entities that are explicitly annotated with `@Cacheable` or `@org.hibernate.annotations.Cache`. Entities without these annotations will never be placed in the Second-Level Cache, preventing accidental memory bloat.
</details>

<details>
<summary><strong>8. Why should you avoid caching entities with mutable child collections?</strong></summary>

Caching collections (e.g., `@Cache` on `@OneToMany`) requires Hibernate to store collection role entries. When children are added, removed, or modified, maintaining collection cache consistency introduces significant distributed coordination overhead and lock contention. It is generally safer to cache root entities only.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Read-Only Entity Cache</strong></summary>

**Task**: Annotate a `Currency` entity with proper JPA and Hibernate annotations to configure it for read-only Second-Level Caching in region `"currencies"`.

```java
@Entity
@Table(name = "currencies")
@Cacheable
@Cache(usage = CacheConcurrencyStrategy.READ_ONLY, region = "currencies")
public class Currency {
    @Id private String code;
    private String name;
    private int decimalPlaces;
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Enabling Query Cache on Repository Query</strong></summary>

**Task**: Write a repository method with `@QueryHints` to cache query results in the `"active-plans-cache"` region.

```java
public interface SubscriptionPlanRepository extends JpaRepository<SubscriptionPlan, Long> {
    @QueryHints({
        @QueryHint(name = HibernateHints.HINT_CACHEABLE, value = "true"),
        @QueryHint(name = HibernateHints.HINT_CACHE_REGION, value = "active-plans-cache")
    })
    List<SubscriptionPlan> findByActiveTrue();
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Programmatic Region Eviction Service</strong></summary>

**Task**: Implement a service method that takes a region name and completely clears it from the Hibernate Second-Level Cache.

```java
@Service
public class CacheManagementService {
    private final SessionFactory sessionFactory;

    public CacheManagementService(EntityManagerFactory emf) {
        this.sessionFactory = emf.unwrap(SessionFactory.class);
    }

    public void evictRegion(String regionName) {
        sessionFactory.getCache().evictRegion(regionName);
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Cache Hit Ratio Health Indicator</strong></summary>

**Task**: Build a Spring Boot Actuator `HealthIndicator` that reports `DOWN` if the L2C hit ratio drops below 50% after at least 1,000 requests.

```java
@Component
public class HibernateCacheHealthIndicator implements HealthIndicator {
    private final SessionFactory sessionFactory;

    public HibernateCacheHealthIndicator(EntityManagerFactory emf) {
        this.sessionFactory = emf.unwrap(SessionFactory.class);
    }

    @Override
    public Health health() {
        Statistics stats = sessionFactory.getStatistics();
        long total = stats.getSecondLevelCacheHitCount() + stats.getSecondLevelCacheMissCount();
        if (total > 1000) {
            double hitRatio = ((double) stats.getSecondLevelCacheHitCount() / total) * 100.0;
            if (hitRatio < 50.0) {
                return Health.down().withDetail("hitRatio", hitRatio).build();
            }
        }
        return Health.up().build();
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Distributed Redisson Second-Level Cache Integration Test</strong></summary>

**Task**: Write an integration test using Testcontainers (Redis + PostgreSQL) asserting that loading an entity twice in two separate transactions executes only 1 SQL `SELECT` statement.

```java
@Test
@Transactional(propagation = Propagation.NOT_SUPPORTED)
void testSecondLevelCacheHits() {
    Product prod = productRepository.save(new Product("SKU-1", "Laptop", BigDecimal.valueOf(999)));

    // Transaction 1: Cache Miss -> Loads from DB, puts into L2C
    productService.findById(prod.getId());

    // Transaction 2: Cache Hit -> Loads from Redisson Redis L2C!
    productService.findById(prod.getId());

    Statistics stats = entityManagerFactory.unwrap(SessionFactory.class).getStatistics();
    assertThat(stats.getSecondLevelCacheHitCount()).isGreaterThanOrEqualTo(1);
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Cache Concurrency Lock Verification</strong></summary>

**Task**: Verify that modifying an entity configured with `READ_WRITE` invalidates the L2C entry and subsequent reads reflect updated values.

```java
@Test
void testCacheUpdateInvalidation() {
    Long id = createProduct("Widget", BigDecimal.TEN);
    productService.findById(id); // Populate cache

    // Update entity
    productService.updatePrice(id, BigDecimal.valueOf(15.00));

    Product updated = productService.findById(id);
    assertThat(updated.getBasePrice()).isEqualByComparingTo("15.00");
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| L1 Cache:     Session-scoped, mandatory, automatic within @Transactional              |
| L2C Cache:    Cluster-scoped via Redisson / Redis; shared across all sessions/nodes   |
| Read-Only:    @Cache(usage = CacheConcurrencyStrategy.READ_ONLY) for immutable data  |
| Read-Write:   @Cache(usage = CacheConcurrencyStrategy.READ_WRITE) with soft locks    |
| Query Cache:  Caches entity IDs matching a query; pair ALWAYS with L2C entity cache   |
| Selective:    javax.persistence.sharedCache.mode = ENABLE_SELECTIVE                   |
| Golden Rule:  Never cache volatile, high-frequency update entities (write-heavy data) |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Hibernate 6.6 User Guide: Caching Architecture](https://docs.jboss.org/hibernate/orm/6.6/userguide/html_single/Hibernate_User_Guide.html#caching)
- [Redisson Hibernate Second-Level Cache Documentation](https://github.com/redisson/redisson/wiki/14.-Integration-with-frameworks#143-hibernate-cache-provider)
- [Vlad Mihalcea: How Does Hibernate Second-Level Cache Work](https://vladmihalcea.com/how-does-hibernate-second-level-cache-work/)
- [Martin Fowler: Two Hard Things in Computer Science (Cache Invalidation)](https://martinfowler.com/bliki/TwoHardThings.html)
