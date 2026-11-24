# 04: Spring Data JPA, Hibernate & Transactions Technical Interview Q&A

| Metadata Field | Value |
|---|---|
| **Topic** | Spring Data JPA, Hibernate 6 SQM, N+1 Traps, Keyset Paging & Transactions |
| **Spring Boot Version** | 3.4.x |
| **Hibernate Version** | 6.6.x |
| **Java Version** | 21 / 25 LTS |
| **Target Level** | Senior / Lead / Staff Backend Engineer |
| **Key Focus Areas** | N+1 Problem, `@EntityGraph`, Keyset Pagination, Transaction Propagation, OSIV Trap |

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

Persistence layer performance is the single most common source of outages and latency spikes in enterprise Spring Boot applications. While Spring Data JPA abstracts SQL behind repository interfaces, misunderstanding the Object-Relational Mapping (ORM) layer leads to catastrophic $N+1$ query traps, memory exhaustion, transaction lock contention, and connection pool starvation.

Senior and staff-level interviews focus heavily on the Hibernate Persistence Context, Hibernate 6's Semantic Query Model (SQM), `JOIN FETCH` vs `@EntityGraph`, Keyset/Window pagination replacing slow SQL `OFFSET`, Spring's declarative transaction propagation semantics (`REQUIRES_NEW`, `REQUIRED`), and why Open Session in View (OSIV) must be disabled in production.

---

## 2. Core Concepts

### 2.1 The $N+1$ Select Problem & Solutions
When querying an entity with a `@OneToMany` collection:
- An initial query fetches $N$ parent records (`SELECT * FROM orders`).
- Accessing the children triggers $N$ additional queries (`SELECT * FROM order_items WHERE order_id = ?`).
- Total: $1 + N$ queries. If $N = 1,000$, 1,001 SQL queries hit the database!
- **Solutions**:
  1. `JOIN FETCH`: Hardcoded inner/left join in JPQL query.
  2. `@EntityGraph`: Dynamic declarative fetch plan overriding lazy loading at runtime.
  3. Hibernate `@BatchSize(size = 50)`: Groups child queries into `WHERE order_id IN (?, ?, ...)`.

### 2.2 Keyset Pagination vs. Offset Pagination
- **Offset Pagination (`OFFSET 100000 LIMIT 20`)**: Database scans and discards 100,000 rows before returning 20. Performance degrades linearly ($O(N)$).
- **Keyset Pagination (`WHERE id > :lastSeenId ORDER BY id ASC LIMIT 20`)**: Uses B-Tree index seek ($O(\log N)$). Query execution is instantaneous regardless of page depth.

### 2.3 Transaction Propagation Semantics
- `REQUIRED` (Default): Joins current transaction if one exists; creates new one if none exists.
- `REQUIRES_NEW`: Suspends current transaction and executes in a completely separate, independent transaction.
- `MANDATORY`: Requires an existing active transaction; throws `IllegalTransactionStateException` if none exists.
- `SUPPORTS`: Executes within transaction if present; executes non-transactionally if none exists.

---

## 3. Internals & Architecture

```
        [ Spring Service Method: @Transactional ]
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │         TransactionInterceptor (AOP Proxy)              │
  │   1. PlatformTransactionManager.getTransaction()        │
  │   2. Borrow connection from HikariCP Pool               │
  │   3. Set autoCommit = false                             │
  └────────────────────────┬────────────────────────────────┘
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │               Hibernate 6 Persistence Context           │
  │   - First-Level (L1) Cache (Entity Identity Map)        │
  │   - Dirty Checking (Snapshot vs Current state)          │
  └────────────────────────┬────────────────────────────────┘
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │         SQL Generation via Semantic Query Model (SQM)   │
  │   - Executes Batch Inserts & JDBC Statements            │
  └────────────────────────┬────────────────────────────────┘
                           │
         ┌─────────────────┴─────────────────┐
         ▼ (Method Success)                  ▼ (Unchecked Exception)
  ┌─────────────────────────┐       ┌────────────────────────┐
  │ Connection.commit()     │       │ Connection.rollback()  │
  │ Release to HikariCP Pool│       │ Release to HikariCP    │
  └─────────────────────────┘       └────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 `@EntityGraph` Dynamic Fetch Plan Syntax
```java
public interface OrderRepository extends JpaRepository<Order, UUID> {
    @EntityGraph(attributePaths = {"items", "customer"})
    Optional<Order> findWithDetailsById(UUID id);
}
```

---

## 5. 5 Practical Examples

### Example 1: Solving $N+1$ Queries with `@EntityGraph`

```java
package com.example.data.repository;

import com.example.data.domain.CustomerOrder;
import org.springframework.data.jpa.repository.EntityGraph;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.util.List;
import java.util.Optional;
import java.util.UUID;

public interface CustomerOrderRepository extends JpaRepository<CustomerOrder, UUID> {

    // Eliminates N+1: Emits a single SQL LEFT JOIN query
    @EntityGraph(attributePaths = {"items", "shippingAddress"})
    @Query("SELECT o FROM CustomerOrder o WHERE o.id = :id")
    Optional<CustomerOrder> findOrderWithDetailsById(@Param("id") UUID id);

    // Dynamic join fetch for batch list query
    @EntityGraph(attributePaths = {"items"})
    List<CustomerOrder> findByCustomerId(UUID customerId);
}
```

### Example 2: High-Performance Keyset (Window) Pagination

```java
package com.example.data.repository;

import com.example.data.domain.AuditLog;
import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.time.Instant;
import java.util.List;
import java.util.UUID;

public interface AuditLogRepository extends JpaRepository<AuditLog, UUID> {

    // Fast O(log N) Keyset Query utilizing composite B-Tree index (created_at, id)
    @Query("""
        SELECT a FROM AuditLog a
        WHERE (a.createdAt < :lastTimestamp) OR (a.createdAt = :lastTimestamp AND a.id < :lastId)
        ORDER BY a.createdAt DESC, a.id DESC
    """)
    List<AuditLog> findNextPageKeyset(
            @Param("lastTimestamp") Instant lastTimestamp,
            @Param("lastId") UUID lastId,
            Pageable pageable
    );
}
```

### Example 3: Transaction Propagation: `REQUIRES_NEW` for Independent Auditing

```java
package com.example.data.service;

import com.example.data.domain.AuditLog;
import com.example.data.repository.AuditLogRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Propagation;
import org.springframework.transaction.annotation.Transactional;

@Service
public class AuditLoggingService {

    private final AuditLogRepository auditRepo;

    public AuditLoggingService(AuditLogRepository auditRepo) {
        this.auditRepo = auditRepo;
    }

    // REQUIRES_NEW suspends the outer business transaction.
    // If the business transaction fails and rolls back, the audit log STILL commits!
    @Transactional(propagation = Propagation.REQUIRES_NEW)
    public void recordSecurityAudit(String action, String principal) {
        AuditLog log = new AuditLog(action, principal);
        auditRepo.save(log);
    }
}
```

### Example 4: Rollback Rules for Checked Exceptions

```java
package com.example.data.service;

import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class PaymentProcessingService {

    public class PaymentDeclinedCheckedException extends Exception {
        public PaymentDeclinedCheckedException(String msg) { super(msg); }
    }

    // By default, Spring ONLY rolls back on RuntimeException and Error!
    // rollbackFor = Exception.class forces rollback on Checked Exceptions as well
    @Transactional(rollbackFor = Exception.class)
    public void executePayment(String orderId) throws PaymentDeclinedCheckedException {
        // Business logic...
        if (false) {
            throw new PaymentDeclinedCheckedException("Card declined");
        }
    }
}
```

### Example 5: Correct Entity Mapping with `equals()` and `hashCode()`

```java
package com.example.data.domain;

import jakarta.persistence.*;
import org.hibernate.proxy.HibernateProxy;

import java.util.Objects;
import java.util.UUID;

@Entity
@Table(name = "customers")
public class Customer {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(nullable = false)
    private String email;

    public Customer() {}

    public Customer(String email) { this.email = email; }

    public UUID getId() { return id; }
    public String getEmail() { return email; }

    // Correct JPA equals & hashCode implementation accounting for Hibernate proxies
    @Override
    public final boolean equals(Object o) {
        if (this == o) return true;
        if (o == null) return false;
        Class<?> oEffectiveClass = o instanceof HibernateProxy hp 
                ? hp.getHibernateLazyInitializer().getPersistentClass() 
                : o.getClass();
        Class<?> thisEffectiveClass = this instanceof HibernateProxy hp 
                ? hp.getHibernateLazyInitializer().getPersistentClass() 
                : this.getClass();
        if (thisEffectiveClass != oEffectiveClass) return false;
        Customer customer = (Customer) o;
        return getId() != null && Objects.equals(getId(), customer.getId());
    }

    @Override
    public final int hashCode() {
        return this instanceof HibernateProxy hp 
                ? hp.getHibernateLazyInitializer().getPersistentClass().hashCode() 
                : getClass().hashCode();
    }
}
```

---

## 6. Best Practices

- **Always Disable Open Session in View (OSIV)**:
  ```yaml
  spring:
    jpa:
      open-in-view: false
  ```
  OSIV keeps database connections open during HTTP response JSON serialization, leading to connection pool exhaustion and hidden query execution in the presentation layer.
- **Annotate Read Queries with `readOnly = true`**: `@Transactional(readOnly = true)` disables Hibernate dirty checking snapshots, saves memory, and routes queries directly to read replicas.
- **Use Keyset Pagination for Infinite Scroll**: Never execute `OFFSET 50000`; use `WHERE (created_at, id) < (:last_time, :last_id)` for sub-millisecond pagination.
- **Configure Batch Fetching**: Set `spring.jpa.properties.hibernate.default_batch_fetch_size: 50` to safeguard against accidental lazy-loading $N+1$ loops.
- **Specify `rollbackFor = Exception.class`**: Ensure transactions roll back on checked exceptions in addition to runtime exceptions.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Leaving OSIV Enabled (`open-in-view: true`)** | Holds database connections open while Jackson serializes JSON, exhausting HikariCP pools. | Set `spring.jpa.open-in-view: false` and fetch required data in service layer. |
| **`FetchType.EAGER` on Collections** | Eager loading executes joins on every query, causing massive Cartesian product explosions. | Always use `FetchType.LAZY` on collections; fetch explicitly via `@EntityGraph`. |
| **Offset Pagination on Large Tables** | `OFFSET 100000` scans 100,000 rows in disk, pegging database CPU at 100%. | Use Keyset (Seek) Pagination based on indexed columns. |
| **Expecting Rollback on Checked Exception** | By default, `@Transactional` only rolls back for `RuntimeException`. Checked exceptions commit silently. | Add `@Transactional(rollbackFor = Exception.class)`. |
| **Using `Lombok @Data` on JPA Entities** | Generates `equals`, `hashCode`, and `toString` including lazy collections, causing `LazyInitializationException` and recursion. | Use `@Getter` and `@Setter` only; implement proxy-safe `equals()` and `hashCode()` manually. |
| **Multiple Bag Fetch Exception** | Fetching two `@OneToMany` `List` collections in a single query creates a Cartesian product error. | Use `Set` collections or split into two separate queries with batch size. |

---

## 8. Performance & Security Considerations

### 8.1 Hibernate 6 Semantic Query Model (SQM)
Hibernate 6 introduced SQM, generating standardized, optimized SQL abstract syntax trees directly, avoiding unnecessary joins and subqueries.

### 8.2 JDBC Batching Configuration
Ensure batch updates and inserts are active:
```yaml
spring:
  jpa:
    properties:
      hibernate:
        jdbc:
          batch_size: 50
        order_inserts: true
        order_updates: true
```

---

## 9. Comparison / Decision Matrix

| Strategy | Performance | Complexity | Best For |
|---|---|---|---|
| **Offset Pagination (`Pageable`)** | $O(N)$ (Degrades with depth) | Zero (Spring built-in) | Small datasets ($< 1,000$ rows), admin UIs |
| **Keyset Pagination (Seek)** | $O(\log N)$ (Constant speed) | Moderate (Custom query) | High-scale infinite scroll, mobile feeds |
| **`JOIN FETCH` in JPQL** | $O(1)$ query | Static JPQL | Fixed queries requiring joined collections |
| **`@EntityGraph`** | $O(1)$ query | Declarative & Dynamic | Reusable repositories with varying fetch plans |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. What is the N+1 query problem, and how do you detect and eliminate it?</strong></summary>

It occurs when querying parent entities triggers $N$ additional child queries for each parent when accessing lazy relationships. Detect it by setting `hibernate.generate_statistics: true` or using Datasource-Proxy/QuickPerf. Eliminate it using `JOIN FETCH`, `@EntityGraph`, or configuring `hibernate.default_batch_fetch_size: 50`.
</details>

<details>
<summary><strong>2. Why is Open Session in View (OSIV) considered an anti-pattern?</strong></summary>

OSIV keeps the Hibernate `Session` and underlying database connection open until the view (or Jackson JSON serializer) finishes writing the HTTP response. If serialization is slow or lazy getters are triggered during JSON generation, queries are executed unexpectedly in the controller/view layer, and connection pool resources are held open for seconds.
</details>

<details>
<summary><strong>3. What happens if a `@Transactional` method throws a checked exception?</strong></summary>

By default, Spring's transaction interceptor ONLY marks transactions for rollback upon receiving unchecked exceptions (`RuntimeException` and `Error`). A checked exception (inheriting from `Exception`) causes the transaction to COMMIT unless explicitly annotated with `@Transactional(rollbackFor = Exception.class)`.
</details>

<details>
<summary><strong>4. Explain the difference between `Propagation.REQUIRED` and `Propagation.REQUIRES_NEW`.</strong></summary>

`REQUIRED` joins an existing active transaction or creates one if none exists. `REQUIRES_NEW` always suspends any existing transaction and starts a completely independent, isolated transaction with its own database connection that commits or rolls back independently of the outer transaction.
</details>

<details>
<summary><strong>5. Why is Keyset Pagination preferred over Offset Pagination for large datasets?</strong></summary>

Offset pagination requires the database to read and discard all offset rows (`OFFSET 100000 LIMIT 20` scans 100,020 rows), resulting in high disk I/O and latency. Keyset pagination uses indexed comparison operators (`WHERE id > :lastSeenId LIMIT 20`), which directly seeks to the target index leaf in $O(\log N)$ time.
</details>

<details>
<summary><strong>6. What is Hibernate's First-Level (L1) Cache?</strong></summary>

The L1 cache is the `PersistenceContext` associated with the current `EntityManager`/`Session`. It acts as an identity map, ensuring that multiple `findById()` calls for the exact same entity within a single transaction return the identical Java instance without re-querying the database.
</details>

<details>
<summary><strong>7. Why should Lombok's `@Data` or `@EqualsAndHashCode` not be used on JPA entities?</strong></summary>

Default Lombok implementations include all entity fields, including lazy-loaded `@OneToMany` collections. Accessing them inside `equals()` or `hashCode()` triggers `LazyInitializationException` outside transactions or loads entire tables into memory. Entities should implement proxy-safe identity comparisons based on their primary key.
</details>

<details>
<summary><strong>8. What does `@Transactional(readOnly = true)` do under the hood?</strong></summary>

It sets the Hibernate flush mode to `FlushMode.MANUAL`, prevents Hibernate from generating snapshot copies of entities for dirty checking (saving significant heap memory), and hints the JDBC driver and connection router to direct queries to database read replicas.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Fix N+1 Query Using `@EntityGraph`</strong></summary>

**Prompt**: Refactor a repository method returning `List<Department>` so that its `employees` collection is fetched in a single query.

**Solution**:
Inspect Example 1: annotate method with `@EntityGraph(attributePaths = {"employees"})`.
</details>

<details>
<summary><strong>Exercise 2: Implement Keyset Pagination Query</strong></summary>

**Prompt**: Write a Spring Data JPA query selecting products sorted by `price ASC, id ASC` using a keyset condition.

**Solution**:
```sql
SELECT p FROM Product p 
WHERE (p.price > :lastPrice) OR (p.price = :lastPrice AND p.id > :lastId)
ORDER BY p.price ASC, p.id ASC
```
</details>

<details>
<summary><strong>Exercise 3: Configure Spring Boot to Disable OSIV</strong></summary>

**Prompt**: Disable Open Session in View in `application.yaml` and verify startup warning disappears.

**Solution**:
```yaml
spring:
  jpa:
    open-in-view: false
```
</details>

<details>
<summary><strong>Exercise 4: Verify Rollback on Checked Exception</strong></summary>

**Prompt**: Write a test verifying that throwing `IOException` rolls back the transaction when `rollbackFor = Exception.class` is declared.

**Solution**:
Inspect Example 4: annotate service method with `@Transactional(rollbackFor = Exception.class)`, throw `IOException`, and assert database table count remains 0.
</details>

<details>
<summary><strong>Exercise 5: Configure Hibernate Batch Fetching</strong></summary>

**Prompt**: Set default batch fetch size to 50 in `application.yaml`.

**Solution**:
```yaml
spring:
  jpa:
    properties:
      hibernate:
        default_batch_fetch_size: 50
```
</details>

<details>
<summary><strong>Exercise 6: Test Isolation with `REQUIRES_NEW`</strong></summary>

**Prompt**: Create an outer transaction that rolls back, and verify an inner `REQUIRES_NEW` audit log persists successfully.

**Solution**:
Inspect Example 3: outer method calls `auditService.recordSecurityAudit()`, then throws `RuntimeException`. Assert business record is rolled back, but audit log record is committed.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   SPRING DATA JPA & TRANSACTIONS CHEATSHEET            │
├────────────────────────────────────────────────────────────────────────┤
│ 1. N+1 Prevention: Use @EntityGraph or batch_fetch_size: 50            │
│ 2. Production Invariant: spring.jpa.open-in-view = false               │
│ 3. Paging Scale: Keyset pagination for deep pages (never OFFSET 100k)  │
│ 4. Read Performance: @Transactional(readOnly = true) disables snapshots│
│ 5. Rollback Rule: Default rolls back on RuntimeException only          │
│ 6. Propagation: REQUIRES_NEW for independent audit logs & metrics      │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring Data JPA Reference Documentation](https://docs.spring.io/spring-data/jpa/reference/)
- [Hibernate ORM 6.6 User Guide](https://docs.jboss.org/hibernate/orm/current/userguide/html_single/Hibernate_User_Guide.html)
- [Vlad Mihalcea: The High-Performance Java Persistence Guide](https://vladmihalcea.com/books/high-performance-java-persistence/)
- [Keyset Pagination Guide (Markus Winand)](https://use-the-index-luke.com/no-offset)
