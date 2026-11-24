# JPA and Hibernate Performance Patterns in Spring Boot

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
Spring Data JPA and Hibernate provide immense developer productivity by mapping relational tables directly to object-oriented domain models. However, when deployed to production without deep performance discipline, JPA abstractions frequently produce catastrophic bottlenecks:
- The **N+1 Query Problem**: Fetching a list of 1,000 orders triggers 1,001 individual roundtrips to the database to load customer records, degrading API latency from 20ms to 4 seconds.
- Inefficient entity loading: Loading complete managed entity graphs when only 3 columns are needed burns JVM heap memory and forces expensive dirty checking tracking.
- Row-by-row insertion: Saving 10,000 records row-by-row generates 10,000 network roundtrips instead of a single batched JDBC insert.
- **Dirty Checking Overhead**: Every managed entity loaded into the Hibernate Persistence Context allocates a duplicate snapshot array for change detection, doubling memory consumption.

### Historical Evolution
Hibernate was originally designed for transactional CRUD operations in enterprise applications. 

With **Hibernate 6.x** in **Spring Boot 3**, the internal query translation engine was completely rewritten around the **Semantic Query Model (SQM)**, enabling significantly faster HQL/JPQL-to-SQL translation, native window functions, and enhanced tuple projections. 

Simultaneously, features like `StatelessSession`, interface/record DTO projections, and batching optimizations allow developers to achieve native JDBC speeds while retaining Spring Data's declarative convenience.

### Production Relevance
Mastering JPA performance patterns ensures that:
- Database roundtrips are reduced by up to 95% via `JOIN FETCH` and `@EntityGraph`.
- Batch insertion operations execute at $> 20,000$ records/sec using JDBC batching and sequence allocation.
- Read-only endpoints consume zero dirty-checking memory overhead via `@Transactional(readOnly = true)`.

---

## 2. Core Concepts

### Foundational JPA Performance Pillars
1. **The N+1 Query Problem**: Occurs when an entity association (`@ManyToOne`, `@OneToMany`) is configured with `FetchType.LAZY` or `EAGER` and an initial query (`1`) triggers $N$ secondary queries for each parent record.
2. **DTO Projections (Constructor & Interface)**: Bypassing Hibernate managed entity tracking entirely by selecting only required columns directly into Java records (`SELECT new com.example.OrderSummary(...)`).
3. **`@Transactional(readOnly = true)`**: Informs Hibernate and the JDBC driver that the transaction is read-only. Hibernate sets the `FlushMode` to `MANUAL` and skips creating entity dirty-checking snapshot copies, saving massive heap memory.
4. **JDBC Batching**: Grouping multiple `INSERT`, `UPDATE`, or `DELETE` statements into a single network transmission to the database server.
5. **Hibernate `StatelessSession`**: A lightweight, non-caching persistence session that operates without a First-Level Cache (Persistence Context), dirty checking, or lifecycle event interceptors, ideal for high-volume bulk batch processing.
6. **Open-Session-In-View (OSIV)**: A legacy Spring pattern that keeps the database connection open during HTTP view rendering. **Must be disabled in production** (`spring.jpa.open-in-view=false`).

---

## 3. How It Works Internally

### Persistence Context vs. DTO Projection Memory Mechanics

```
PATH A: MANAGED ENTITY QUERY (SELECT o FROM Order o)
  1. Executes SQL: SELECT * FROM orders;
  2. Allocates Order entity instances on JVM heap.
  3. Hibernate Persistence Context (First-Level Cache):
     - Stores entity reference in Session map.
     - DUP-ALLOCATES a snapshot copy of all entity fields for dirty checking.
  4. At transaction commit:
     - Iterates through ALL loaded entities.
     - Compares current state against snapshot copy field-by-field (Dirty Checking).
     - Issues UPDATE SQL for any modified fields.
  --> Heavy CPU & Heap Allocation Overhead!

PATH B: DTO PROJECTION QUERY (SELECT new OrderDTO(o.id, o.amount) FROM Order o)
  1. Executes SQL: SELECT id, amount FROM orders;
  2. Allocates lightweight immutable Java Record OrderDTO.
  3. NO First-Level Cache retention.
  4. NO snapshot array allocated (Zero Dirty Checking overhead).
  5. Transaction commits instantly without entity graph comparison!
  --> 10x faster, minimal memory allocation!
```

---

## 4. Syntax & Configuration Reference

### Production Performance Properties (`application.yml`)

```yaml
spring:
  jpa:
    open-in-view: false # CRITICAL: Release DB connection immediately after service layer!
    properties:
      hibernate:
        # JDBC Batching configuration
        jdbc:
          batch_size: 50 # Batch statements in groups of 50
          batch_versioned_data: true
        order_inserts: true # Groups insert statements by entity to allow batching
        order_updates: true # Groups update statements by entity to allow batching
        # Query plan cache optimizations
        query:
          plan_cache_max_size: 2048
          in_clause_parameter_padding: true # Reuses query plans by padding IN (?) clauses to powers of 2
        # Logging: Disable SQL formatting in production!
        format_sql: false
        generate_statistics: false # Enable only during performance audits
```

---

## 5. Practical Examples

### Example 1: Eliminating N+1 Queries with `@EntityGraph` and `JOIN FETCH`
```java
package com.example.performance.jpa;

import org.springframework.data.jpa.repository.EntityGraph;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.stereotype.Repository;

import java.util.List;

@Repository
public interface OrderRepository extends JpaRepository<Order, Long> {

    // ANTI-PATTERN: Standard findAll() produces 1 query for orders + N queries for customers!
    // List<Order> findAll();

    // SOLUTION 1: Explicit JPQL JOIN FETCH loads Order and Customer in a single SQL query!
    @Query("SELECT o FROM Order o JOIN FETCH o.customer WHERE o.status = :status")
    List<Order> findAllWithCustomerFetch(String status);

    // SOLUTION 2: Declarative @EntityGraph (Overrides lazy loading for specific query)
    @EntityGraph(attributePaths = { "customer", "orderItems" })
    List<Order> findByStatus(String status);
}
```

### Example 2: Ultra-Fast Read-Only Projections with Java Records
```java
package com.example.performance.jpa;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.stereotype.Repository;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.List;

public record OrderSummaryDto(Long orderId, String customerName, BigDecimal totalAmount, Instant createdAt) {}

@Repository
public interface OrderSummaryRepository extends JpaRepository<Order, Long> {

    // Direct constructor expression: zero Hibernate entity tracking, zero dirty checking
    @Transactional(readOnly = true)
    @Query("""
        SELECT new com.example.performance.jpa.OrderSummaryDto(
            o.id, c.name, o.totalAmount, o.createdAt
        )
        FROM Order o
        JOIN o.customer c
        WHERE o.status = 'COMPLETED'
        """)
    List<OrderSummaryDto> findCompletedOrderSummaries();
}
```

### Example 3: High-Performance Bulk Batch Insertion
To enable batch inserts in Hibernate, the entity **must not use `GenerationType.IDENTITY`** because `IDENTITY` forces an immediate `INSERT` execution per row to retrieve the database-generated ID, completely disabling JDBC batching! Use `GenerationType.SEQUENCE` with pooled optimizers:

```java
package com.example.performance.jpa;

import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.SequenceGenerator;
import jakarta.persistence.Table;

import java.math.BigDecimal;

@Entity
@Table(name = "audit_logs")
public class AuditLog {

    @Id
    @GeneratedValue(strategy = GenerationType.SEQUENCE, generator = "audit_seq_gen")
    @SequenceGenerator(
        name = "audit_seq_gen",
        sequenceName = "audit_log_seq",
        allocationSize = 50 // Matches hibernate.jdbc.batch_size: 50!
    )
    private Long id;

    private String action;
    private BigDecimal amount;

    public AuditLog() {}
    public AuditLog(String action, BigDecimal amount) {
        this.action = action;
        this.amount = amount;
    }
    // Getters and setters omitted
}
```

**Batch Insertion Service with Periodic Flushes**:
```java
package com.example.performance.jpa;

import jakarta.persistence.EntityManager;
import jakarta.persistence.PersistenceContext;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;
import java.util.List;

@Service
public class BulkIngestionService {

    @PersistenceContext
    private EntityManager entityManager;

    private static final int BATCH_SIZE = 50;

    @Transactional
    public void saveAllInBatches(List<String> actions) {
        for (int i = 0; i < actions.size(); i++) {
            AuditLog log = new AuditLog(actions.get(i), BigDecimal.TEN);
            entityManager.persist(log);

            // Flush and clear First-Level Cache to prevent OutOfMemoryError!
            if (i > 0 && i % BATCH_SIZE == 0) {
                entityManager.flush(); // Sends 50 inserts as a single JDBC batch!
                entityManager.clear(); // Evicts managed entities from Persistence Context
            }
        }
    }
}
```

### Example 4: Bulk Operations with Hibernate `StatelessSession`
When processing hundreds of thousands of records, `StatelessSession` bypasses the persistence context completely:

```java
package com.example.performance.jpa;

import org.hibernate.SessionFactory;
import org.hibernate.StatelessSession;
import org.hibernate.Transaction;
import org.springframework.stereotype.Service;

import java.util.List;

@Service
public class BigDataProcessorService {

    private final SessionFactory sessionFactory;

    public BigDataProcessorService(SessionFactory sessionFactory) {
        this.sessionFactory = sessionFactory;
    }

    public void archiveOldOrders(List<Long> orderIds) {
        // StatelessSession has NO First-Level Cache, NO dirty checking, NO cascade operations
        try (StatelessSession session = sessionFactory.openStatelessSession()) {
            Transaction tx = session.beginTransaction();
            try {
                for (Long orderId : orderIds) {
                    Order order = session.get(Order.class, orderId);
                    if (order != null) {
                        order.setStatus("ARCHIVED");
                        session.update(order); // Direct SQL UPDATE without snapshot tracking
                    }
                }
                tx.commit();
            } catch (Exception ex) {
                tx.rollback();
                throw ex;
            }
        }
    }
}
```

### Example 5: Anti-Pattern vs. Production Fix (In-Memory Pagination on Joined Collections)
```java
// ANTI-PATTERN: Paginated query joining a collection association
// Hibernate logs: "HHH000104: firstResult/maxResults specified with collection fetch; applying in memory!"
// Result: Hibernate downloads ALL 1,000,000 rows into JVM memory and paginates in Java heap!
@Repository
public interface BadRepository extends JpaRepository<Order, Long> {
    @Query("SELECT o FROM Order o JOIN FETCH o.orderItems")
    Page<Order> findOrdersWithItems(Pageable pageable); // CATASTROPHIC HEAP EXHAUSTION!
}

// PRODUCTION FIX: Two-phase query or DTO projection
// Phase 1: Fetch paginated Order IDs using clean pagination
// Phase 2: Fetch full entity graphs for ONLY the 20 paginated IDs
@Repository
public interface ScalableOrderRepository extends JpaRepository<Order, Long> {

    @Query("SELECT o.id FROM Order o WHERE o.status = :status")
    Page<Long> findOrderIdsByStatus(String status, Pageable pageable);

    @Query("SELECT o FROM Order o JOIN FETCH o.orderItems WHERE o.id IN :ids")
    List<Order> findOrdersWithItemsByIds(List<Long> ids);
}
```

---

## 6. Best Practices & Production Guidelines

1. **Always Disable Open-Session-In-View (OSIV)**: Set `spring.jpa.open-in-view=false`. With OSIV enabled, database connections remain checked out during the entire HTTP response rendering, depleting the HikariCP pool.
2. **Use `@Transactional(readOnly = true)` for Queries**: Instructs Hibernate to bypass dirty checking snapshot allocations and sets the JDBC connection to read-only mode, reducing memory footprint by ~50%.
3. **Never Use `GenerationType.IDENTITY` for High-Volume Entities**: `IDENTITY` disables JDBC batching because Hibernate must execute each `INSERT` immediately to retrieve the generated ID. Always use `GenerationType.SEQUENCE` with `allocationSize=50`.
4. **Prefer DTO Projections over Entities for Read Operations**: When reading data for APIs or reports, query Java records or interface projections directly. Managed entities should only be loaded when they will be mutated within the transaction.
5. **Always Set Batch Ordering Flags**: Enable `hibernate.order_inserts=true` and `hibernate.order_updates=true`. Without these flags, interleaved statements (e.g. `Insert Order`, `Insert Item`, `Insert Order`) break JDBC batch grouping.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Leaving OSIV enabled (`open-in-view: true`) | Holds database connections open while serializing JSON, causing HikariCP pool exhaustion under load. | Set `spring.jpa.open-in-view: false` in `application.yml`. |
| Using `GenerationType.IDENTITY` with batching | JDBC batching is silently disabled; Hibernate inserts rows one-by-one to get IDs. | Switch to `GenerationType.SEQUENCE` with `allocationSize=50`. |
| Paginating over `JOIN FETCH` collection | Hibernate executes unpaged query and performs pagination in JVM memory, exhausting heap. | Page on root IDs first, then fetch collection by ID list. |
| Forgetting `entityManager.clear()` in batch loops | All 50,000 persisted entities remain managed in the First-Level Cache, causing `OutOfMemoryError`. | Flush and clear `EntityManager` every `batch_size` iterations. |
| The N+1 query trap | Calling getters on lazy associations inside loops fires hundreds of individual queries. | Use `JOIN FETCH`, `@EntityGraph`, or DTO projections. |
| Missing `in_clause_parameter_padding` | Variable-length `IN (?)` queries flood the database query plan cache with un-reusable execution plans. | Set `hibernate.query.in_clause_parameter_padding=true`. |

---

## 8. Performance & Security Notes

### Query Plan Cache Optimization
- In Hibernate 6, dynamic queries compile to internal SQM AST plans.
- When queries use `WHERE o.id IN (:ids)`, querying with 3 IDs, 4 IDs, or 5 IDs compiles separate plans.
- Setting `hibernate.query.in_clause_parameter_padding=true` pads lists to powers of 2 (e.g. 1, 2, 4, 8, 16), reducing distinct compiled query plans by $> 90\%$.

### SQL Injection in Dynamic JPQL
- Always use parameterized queries or Spring Data query methods. Never concatenate user input directly into JPQL strings:
  ```java
  // VULNERABLE:
  entityManager.createQuery("SELECT u FROM User u WHERE u.name = '" + input + "'");
  // SECURE:
  entityManager.createQuery("SELECT u FROM User u WHERE u.name = :name").setParameter("name", input);
  ```

---

## 9. Comparison / When to Use What

| Query Pattern | Memory Overhead | Database Roundtrips | Best Used For |
|---|---|---|---|
| **Managed Entity (`findById`)** | High (Snapshot + PC) | 1 | Single-row read-modify-write transactional CRUD |
| **`JOIN FETCH` / `@EntityGraph`** | Medium-High | 1 (Solves N+1) | Loading parent with child collections to mutate |
| **Record DTO Projection** | Minimal (Zero PC) | 1 (Selective columns) | Read-only APIs, dashboards, search results |
| **`StatelessSession`** | Zero PC overhead | 1 per batch | High-volume batch ingestion, ETL jobs (> 100k rows) |
| **Spring Data JDBC / jOOQ** | Zero ORM overhead | Exact SQL control | Complex multi-table reporting queries |

---

## 10. Interview Questions & Answers

### Q1: What is the N+1 Query Problem in JPA, and how do you resolve it?
<details>
<summary>Click to view answer</summary>

The N+1 problem occurs when an application executes 1 initial query to fetch $N$ parent entities, and then, while accessing a lazy or eager association on each entity, Hibernate executes an additional query per entity ($N$ queries), totaling $N+1$ database roundtrips.  
Resolution:
1. Use `JOIN FETCH` in JPQL to join and fetch parent and children in a single SQL query.
2. Use Spring Data `@EntityGraph(attributePaths = {"children"})`.
3. Select DTO projections directly.
</details>

### Q2: Why does `GenerationType.IDENTITY` disable Hibernate JDBC batching?
<details>
<summary>Click to view answer</summary>

Under the `IDENTITY` generation strategy, the primary key ID is generated by the database column (e.g. auto-increment) upon row insertion. To know the entity's ID immediately upon `persist()`, Hibernate must execute the `INSERT` SQL statement instantly and call `getGeneratedKeys()`. Because statements must be executed one-by-one to retrieve IDs sequentially, Hibernate cannot group them into a deferred JDBC batch. With `GenerationType.SEQUENCE`, Hibernate pre-allocates a block of IDs in memory (e.g. 50 IDs), allowing inserts to be batched cleanly.
</details>

### Q3: What performance optimizations occur when using `@Transactional(readOnly = true)` in Spring?
<details>
<summary>Click to view answer</summary>

1. **Hibernate Snapshot Elimination**: Hibernate sets `FlushMode.MANUAL` on the persistence context and skips creating snapshot copies of entity field arrays, saving heap memory.
2. **Skipped Dirty Checking**: At commit time, Hibernate skips scanning the persistence context for changes.
3. **Database Driver Optimization**: Spring sets the JDBC connection to read-only (`Connection.setReadOnly(true)`), allowing database engines (like MySQL/Oracle) to route queries to read replicas and skip transaction locks.
</details>

### Q4: Why is Open-Session-In-View (OSIV) considered an anti-pattern in production?
<details>
<summary>Click to view answer</summary>

OSIV binds a Hibernate `Session` to the entire lifecycle of an HTTP request. This keeps the physical database connection checked out from the HikariCP connection pool while the servlet controller renders views or serializes JSON payloads. If network serialization is slow, database connections are held idle, starving the connection pool for other threads. Setting `spring.jpa.open-in-view=false` ensures connections are returned to the pool the instant the `@Transactional` service layer completes.
</details>

### Q5: What is Hibernate's `StatelessSession` and when should you use it?
<details>
<summary>Click to view answer</summary>

`StatelessSession` is a command-oriented persistence interface that bypasses the First-Level Cache (Persistence Context) entirely. It does not perform dirty checking, manage entity states, cascade operations, or maintain snapshot arrays. It is designed for bulk operations (batch processing, ETL, archiving) where managing millions of entities in a standard session would cause severe memory exhaustion.
</details>

### Q6: What does `hibernate.query.in_clause_parameter_padding` do?
<details>
<summary>Click to view answer</summary>

When executing queries with dynamic `IN (?)` clauses, differing parameter counts produce different SQL strings (e.g., `IN (?)`, `IN (?, ?)`, `IN (?, ?, ?)`), forcing the database and Hibernate to compile and cache separate query execution plans. Enabling padding rounds parameter list sizes up to powers of 2 (1, 2, 4, 8, 16...), drastically reducing the number of distinct query plans and maximizing plan cache hit rates.
</details>

### Q7: Why does paginating a query with `JOIN FETCH` on a collection cause an OutOfMemoryError?
<details>
<summary>Click to view answer</summary>

When an SQL `JOIN` is executed against a `@OneToMany` collection, the result set duplicates parent rows for every child record. An SQL `LIMIT` clause would limit the joined rows, not the parent entities, corrupting parent entity pagination. Therefore, Hibernate emits a warning (`HHH000104`) and downloads the **entire unpaged result set into JVM memory**, applying pagination in the Java heap. With large datasets, this exhausts heap memory immediately.
</details>

### Q8: What is the difference between an Interface Projection and a Class/Record (DTO) Projection in Spring Data JPA?
<details>
<summary>Click to view answer</summary>

- **Interface Projection**: Spring Data creates a runtime JDK dynamic proxy that intercepts getter calls to read column values from an underlying object array. It incurs small proxy invocation overhead.
- **Class/Record Projection**: Uses constructor expressions (`SELECT new com.example.MyRecord(...)`). Hibernate instantiates the record directly via bytecode constructor invocation with zero proxy overhead, delivering the highest performance.
</details>

---

## 11. Practice Exercises

### Exercise 1: Convert an N+1 Query to DTO Projection (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Refactor a repository query that fetches all `Product` entities with their `Category` names into an immutable `ProductDto` record projection.

**Solution**:
```java
package com.example.performance.jpa;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.stereotype.Repository;

import java.math.BigDecimal;
import java.util.List;

public record ProductDto(Long id, String name, String categoryName, BigDecimal price) {}

@Repository
public interface ProductRepository extends JpaRepository<Product, Long> {

    @Query("""
        SELECT new com.example.performance.jpa.ProductDto(
            p.id, p.name, c.name, p.price
        )
        FROM Product p
        JOIN p.category c
        """)
    List<ProductDto> findAllProductSummaries();
}
```
</details>

### Exercise 2: Configure Safe Two-Phase Pagination for One-to-Many Collections (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Implement a two-phase query in a service layer to safely paginate `Customer` entities while eagerly fetching their `orders` collection without in-memory pagination warnings.

**Solution**:
```java
package com.example.performance.jpa;

import org.springframework.data.domain.Page;
import org.springframework.data.domain.PageImpl;
import org.springframework.data.domain.Pageable;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Service
public class CustomerQueryService {

    private final CustomerRepository customerRepository;

    public CustomerQueryService(CustomerRepository customerRepository) {
        this.customerRepository = customerRepository;
    }

    @Transactional(readOnly = true)
    public Page<Customer> getPaginatedCustomersWithOrders(Pageable pageable) {
        // Phase 1: Paginate root IDs only (Clean SQL LIMIT/OFFSET)
        Page<Long> customerIdsPage = customerRepository.findCustomerIds(pageable);

        // Phase 2: Fetch full entity graphs for ONLY the 20 paginated customer IDs
        List<Customer> customersWithOrders = customerRepository.findWithOrdersByIdIn(customerIdsPage.getContent());

        return new PageImpl<>(customersWithOrders, pageable, customerIdsPage.getTotalElements());
    }
}
```
</details>

### Exercise 3: High-Throughput Batch Upsert with StatelessSession (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a service using Hibernate `StatelessSession` that processes a list of 50,000 `InventoryUpdate` records, inserting them into PostgreSQL in batches of 100 with manual transaction commits.

**Solution**:
```java
package com.example.performance.jpa;

import org.hibernate.SessionFactory;
import org.hibernate.StatelessSession;
import org.hibernate.Transaction;
import org.springframework.stereotype.Service;

import java.util.List;

@Service
public class FastInventoryBatchService {

    private final SessionFactory sessionFactory;
    private static final int CHUNK_SIZE = 100;

    public FastInventoryBatchService(SessionFactory sessionFactory) {
        this.sessionFactory = sessionFactory;
    }

    public void bulkInsertInventory(List<InventoryItem> items) {
        try (StatelessSession session = sessionFactory.openStatelessSession()) {
            Transaction tx = session.beginTransaction();
            try {
                for (int i = 0; i < items.size(); i++) {
                    session.insert(items.get(i)); // Direct batched SQL insert without First-Level Cache

                    if (i > 0 && i % CHUNK_SIZE == 0) {
                        tx.commit(); // Flushes batch and frees DB transaction locks
                        tx = session.beginTransaction();
                    }
                }
                tx.commit();
            } catch (Exception ex) {
                if (tx.isActive()) tx.rollback();
                throw ex;
            }
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Feature / Configuration                     | Description & Performance Impact                            |
+---------------------------------------------+-------------------------------------------------------------+
| spring.jpa.open-in-view: false              | Releases DB connections immediately after service layer     |
| @Transactional(readOnly = true)             | Bypasses dirty-checking snapshot allocations (~50% memory)  |
| JOIN FETCH / @EntityGraph                   | Solves N+1 query problem by joining relationships in SQL    |
| DTO Projections (Records)                   | Queries selective columns directly into immutable records   |
| hibernate.jdbc.batch_size: 50               | Groups multiple insert/update statements into 1 network call|
| hibernate.order_inserts / order_updates     | Groups statements by entity type to allow JDBC batching     |
| GenerationType.SEQUENCE                     | Mandatory for batching; avoid GenerationType.IDENTITY       |
| StatelessSession                            | Non-caching persistence session for bulk batch processing   |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- High-Performance Java Persistence by Vlad Mihalcea
- Hibernate 6 User Guide: Performance Tuning & SQM Architecture
- Spring Data JPA Reference Documentation: Projections & EntityGraphs
- PostgreSQL Performance Optimization Guide: JDBC Batching