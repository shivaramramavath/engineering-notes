# Reactive Data Access with R2DBC in Spring Boot

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Spring WebFlux, Project Reactor, Relational Databases, SQL |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | R2DBC 1.0, PostgreSQL 16, OpenJDK 21.0.2 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Configuration](#4-syntax--configuration)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison: R2DBC vs JDBC / JPA / Hibernate](#9-comparison-r2dbc-vs-jdbc--jpa--hibernate)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Historically, the Achilles' heel of reactive architectures in the Java ecosystem was the database layer. While the web layer (Netty, WebFlux) was completely non-blocking, standard relational database access was bound to **JDBC (Java Database Connectivity)**—an inherently blocking, synchronous specification from 1997.

### The Problem
- Calling blocking JDBC or Hibernate/JPA repositories from WebFlux controllers halts Netty event-loop threads, destroying reactive throughput.
- Offloading JDBC calls to thread pools (`Schedulers.boundedElastic()`) is an awkward workaround that still wastes memory and lacks reactive backpressure.

### The Solution: R2DBC (Reactive Relational Database Connectivity)
R2DBC is an open specification designed from the ground up to bring **reactive programming and non-blocking backpressure to traditional SQL databases** (PostgreSQL, MySQL, Microsoft SQL Server, MariaDB, Oracle). Spring Data R2DBC integrates natively with Spring WebFlux, providing non-blocking reactive repositories (`ReactiveCrudRepository`, `R2dbcEntityTemplate`) and transactional management without a single blocking call.

---

## 2. Core Concepts

### Mental Model: Non-Blocking Database Socket I/O
```
                       WebFlux Controller Method
                                  │
                                  ▼
                    ReactiveCrudRepository.findById(id)
                                  │
                                  ▼
                   Spring Data R2DBC EntityTemplate
                                  │
                                  ▼
                     R2DBC Driver (e.g. r2dbc-postgresql)
              (Uses asynchronous Netty TCP socket channels)
                                  │
                                  ▼
                 PostgreSQL Server executes query:
                 SELECT * FROM orders WHERE id = $1
                                  │
                                  ▼
                 PostgreSQL streams wire packets back
                                  │
                                  ▼
                 R2DBC RowMapper maps rows to Mono<Order>
                                  │
                                  ▼
                 WebFlux returns Mono directly to client!
                 (ZERO platform threads blocked during entire round-trip!)
```

### Invariants
1. **R2DBC is NOT an ORM**: R2DBC does not support complex Hibernate features like dirty checking, lazy loading, session-level caching, or automated entity relationship cascades. It is a lightweight data mapper analogous to `JdbcClient`.
2. **Backpressure to the Wire**: If a query returns 1,000,000 rows, R2DBC requests and pulls records from the database socket only as fast as the downstream HTTP client consumes them.
3. **Reactive Transactions**: Transactions in R2DBC are bound to the **Reactor `Context`**, not to `ThreadLocal`.

---

## 3. How It Works Internally

### The `ConnectionFactory` Abstraction
In R2DBC, `io.r2dbc.spi.ConnectionFactory` replaces the traditional `javax.sql.DataSource`:
1. It establishes non-blocking TCP connections via Netty.
2. Connection pooling is managed by `r2dbc-pool` (`io.r2dbc.pool.ConnectionPool`).
3. Queries execute asynchronously, returning `Publisher<? extends Result>`.
4. Spring Data R2DBC parses `Result.map((row, metadata) -> ...)` into immutable Java records or `@Table` entity beans.

### Reactive Transaction Management
Traditional `@Transactional` relies on `ThreadLocal` storage. In WebFlux, threads change constantly. Spring Data R2DBC uses `ReactiveTransactionManager` (specifically `R2dbcTransactionManager`). The active database connection and transaction state are stored inside the immutable **Reactor `Context`**, propagating cleanly across reactive operators.

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-data-r2dbc</artifactId>
</dependency>
<!-- PostgreSQL R2DBC Driver -->
<dependency>
    <groupId>org.postgresql</groupId>
    <artifactId>r2dbc-postgresql</artifactId>
    <scope>runtime</scope>
</dependency>
<!-- Connection Pooling -->
<dependency>
    <groupId>io.r2dbc</groupId>
    <artifactId>r2dbc-pool</artifactId>
</dependency>
```

### Standard `application.yaml` R2DBC Configuration
```yaml
spring:
  r2dbc:
    url: r2dbc:pool:postgresql://localhost:5432/orders_db
    username: postgres
    password: secretpassword
    pool:
      initial-size: 10
      max-size: 50
      max-idle-time: 30m
      max-acquire-time: 5s
```

---

## 5. Practical Examples

### Example 1: Entity Definition and `ReactiveCrudRepository`

```java
package com.example.r2dbc.model;

import org.springframework.data.annotation.Id;
import org.springframework.data.relational.core.mapping.Table;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

@Table("orders")
public record Order(
        @Id UUID id,
        String customerEmail,
        BigDecimal totalAmount,
        String status,
        Instant createdAt
) {}
```

```java
package com.example.r2dbc.repository;

import com.example.r2dbc.model.Order;
import org.springframework.data.r2dbc.repository.Query;
import org.springframework.data.repository.reactive.ReactiveCrudRepository;
import org.springframework.stereotype.Repository;
import reactor.core.publisher.Flux;
import reactor.core.publisher.Mono;

import java.util.UUID;

@Repository
public interface OrderReactiveRepository extends ReactiveCrudRepository<Order, UUID> {

    Flux<Order> findByCustomerEmail(String email);

    @Query("SELECT * FROM orders WHERE status = :status ORDER BY created_at DESC LIMIT :limit")
    Flux<Order> findRecentOrdersByStatus(String status, int limit);

    Mono<Boolean> existsByCustomerEmail(String email);
}
```

### Example 2: Reactive Transactional Service with `@Transactional`

```java
package com.example.r2dbc.service;

import com.example.r2dbc.model.Order;
import com.example.r2dbc.repository.OrderReactiveRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import reactor.core.publisher.Mono;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

public record PlaceOrderCommand(String email, BigDecimal amount) {}

@Service
public class OrderProcessingService {

    private final OrderReactiveRepository orderRepository;

    public OrderProcessingService(OrderReactiveRepository orderRepository) {
        this.orderRepository = orderRepository;
    }

    /**
     * In R2DBC, @Transactional binds the transaction boundary to the Reactor Context!
     * If an error occurs in the reactive pipeline, the database rolls back automatically.
     */
    @Transactional
    public Mono<Order> placeOrder(PlaceOrderCommand command) {
        Order newOrder = new Order(
                UUID.randomUUID(),
                command.email(),
                command.amount(),
                "PENDING",
                Instant.now()
        );

        return orderRepository.save(newOrder)
                .flatMap(savedOrder -> {
                    if (command.amount().compareTo(new BigDecimal("10000.00")) > 0) {
                        // Error triggers automatic transaction rollback!
                        return Mono.error(new IllegalArgumentException("Order amount exceeds single-transaction limit"));
                    }
                    return Mono.just(savedOrder);
                });
    }
}
```

### Example 3: Low-Level Flexible Queries with `R2dbcEntityTemplate`

When queries require dynamic criteria or complex projections without raw repository methods:

```java
package com.example.r2dbc.template;

import com.example.r2dbc.model.Order;
import org.springframework.data.r2dbc.core.R2dbcEntityTemplate;
import org.springframework.data.relational.core.query.Criteria;
import org.springframework.data.relational.core.query.Query;
import org.springframework.stereotype.Service;
import reactor.core.publisher.Flux;

import java.math.BigDecimal;

@Service
public class OrderQueryService {

    private final R2dbcEntityTemplate entityTemplate;

    public OrderQueryService(R2dbcEntityTemplate entityTemplate) {
        this.entityTemplate = entityTemplate;
    }

    public Flux<Order> searchOrders(String email, BigDecimal minAmount) {
        Criteria criteria = Criteria.where("customer_email").is(email)
                .and("total_amount").greaterThanOrEquals(minAmount);

        return entityTemplate.select(Order.class)
                .matching(Query.query(criteria))
                .all();
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Expecting JPA/Hibernate Entity Relationships in R2DBC
```java
// ANTI-PATTERN: Attempting to use JPA annotations in R2DBC!
@Table("orders")
public class BadOrder {
    @Id private Long id;

    // FATAL MISTAKE: R2DBC does NOT support @OneToMany, @ManyToOne, or lazy fetching!
    @OneToMany(mappedBy = "order", fetch = FetchType.LAZY) 
    private List<OrderItem> items; // Will fail or be completely ignored!
}
```

#### Production Fix: Explicit Relational Modeling with Foreign Keys
```java
// PRODUCTION FIX: Model relational foreign keys explicitly with independent queries
@Table("orders")
public record Order(Long id, String customerName) {}

@Table("order_items")
public record OrderItem(Long id, Long orderId, String productSku, int quantity) {}

// Query items explicitly via non-blocking repository:
public Mono<OrderDetails> getOrderDetails(Long orderId) {
    Mono<Order> orderMono = orderRepo.findById(orderId);
    Flux<OrderItem> itemsFlux = itemRepo.findByOrderId(orderId);

    return Mono.zip(orderMono, itemsFlux.collectList())
            .map(tuple -> new OrderDetails(tuple.getT1(), tuple.getT2()));
}
```

### Example 5: High-Performance Database Streaming Endpoint

```java
package com.example.r2dbc.controller;

import com.example.r2dbc.model.Order;
import com.example.r2dbc.repository.OrderReactiveRepository;
import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import reactor.core.publisher.Flux;

@RestController
@RequestMapping("/api/v1/data-stream")
public class DatabaseStreamingController {

    private final OrderReactiveRepository repository;

    public DatabaseStreamingController(OrderReactiveRepository repository) {
        this.repository = repository;
    }

    /**
     * Streams millions of records from PostgreSQL to the client as NDJSON.
     * Backpressure flows from client network socket all the way down to PostgreSQL cursor!
     */
    @GetMapping(value = "/orders", produces = MediaType.APPLICATION_NDJSON_VALUE)
    public Flux<Order> streamAllOrders() {
        return repository.findAll();
    }
}
```

---

## 6. Best Practices

1. **Always Use Connection Pooling (`r2dbc-pool`)**: Plain R2DBC driver URLs open a new TCP socket per query. Always prefix the URL with `r2dbc:pool:` (e.g. `r2dbc:pool:postgresql://...`) to pool connections.
2. **Remember: R2DBC is NOT JPA**: Do not attempt to use `@OneToMany`, `@ManyToOne`, or entity cascade graphs. Fetch associated rows explicitly using child repositories.
3. **Do Not Block in Mapping**: Keep `Result.map()` and entity mapping functions completely pure and non-blocking.
4. **Use Liquibase or Flyway for Migrations**: R2DBC does not generate database schemas automatically (no `ddl-auto: update`). Use standard Flyway or Liquibase with JDBC drivers for schema migrations during application boot.
5. **Chain Transactions Reactively**: Always return the reactive publisher from `@Transactional` methods. If the pipeline terminates without returning, the transaction commits prematurely or leaks.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Omitting `r2dbc-pool` Dependency** | Opens a new TCP connection on every query, crashing database with `too many connections`. | Include `r2dbc-pool` and use `r2dbc:pool:` connection URLs. |
| **Using JPA Annotations (`@Entity`, `@OneToMany`)** | R2DBC ignores JPA annotations; relationships are null or fail to persist. | Use `@Table`, `@Id`, and query related child records explicitly. |
| **Expecting Flyway/Liquibase to Run over R2DBC** | Database migration tools require standard JDBC drivers; they cannot run over R2DBC. | Add the JDBC driver (`postgresql`) and run migrations via standard Spring Boot Flyway auto-configuration. |
| **Missing Primary Key Detection on `@Id`** | If an entity has an assigned ID (like a UUID) before `save()`, Spring thinks it's an update, not an insert. | Implement `Persistable<ID>` to tell Spring when an entity is new. |
| **`ThreadLocal` Transaction Leak** | Calling non-reactive transaction utilities inside R2DBC does nothing. | Use `R2dbcTransactionManager` and Spring's reactive `@Transactional`. |
| **Blocking Inside R2DBC Operations** | Calling external REST clients or `Thread.sleep()` inside repository callbacks stalls database I/O. | Keep reactive pipelines purely non-blocking. |

---

## 8. Performance & Security Considerations

### Solving the Assigned ID `Persistable` Trap
When using UUIDs as primary keys, if you populate `UUID.randomUUID()` before calling `repository.save()`, Spring Data R2DBC checks if `@Id` is null. Because it is non-null, Spring executes an SQL `UPDATE` instead of an `INSERT`! To fix this:
```java
public record Order(@Id UUID id, String status) implements Persistable<UUID> {
    @Override public boolean isNew() { return true; } // Or check transient flag
}
```

### Connection Pool Sizing
Unlike JDBC where hundreds of connections are needed because threads block, R2DBC multiplexes multiple queries efficiently. A pool of 20-50 R2DBC connections can often handle workloads that previously required 200-500 JDBC connections.

---

## 9. Comparison: R2DBC vs JDBC / JPA / Hibernate

| Feature | Spring Data R2DBC | Spring Data JPA / Hibernate |
| :--- | :--- | :--- |
| **I/O Model** | Pure Non-Blocking Reactive (Netty) | Synchronous Blocking (Thread per Query) |
| **OR-Mapping Depth** | Lightweight Data Mapper (No lazy loading) | Full-featured ORM (Dirty checking, cascades, proxies) |
| **Relationships** | Explicit manual queries | Declarative (`@OneToMany`, `@ManyToMany`) |
| **Schema Generation** | None (External Flyway/Liquibase) | Automated (`hibernate.hbm2ddl.auto`) |
| **Backpressure** | Native End-to-End | None (In-memory buffering) |
| **Best Used For** | WebFlux streaming, high-concurrency microservices | Complex enterprise domain models with deep graphs |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why can't Flyway or Liquibase use an R2DBC connection for database schema migrations?</strong></summary>

**Answer:**
Flyway and Liquibase were designed around the synchronous JDBC specification (`java.sql.Connection`). They execute migration scripts sequentially, locking schema tables synchronously. They have no concept of reactive streams, `Mono`, or non-blocking event loops. In a Spring Boot R2DBC application, you include both the R2DBC driver (for runtime application queries) and the standard JDBC driver (scoped for startup Flyway migrations).
</details>

<details>
<summary><strong>Q2: How does `@Transactional` work in Spring Data R2DBC without `ThreadLocal`?</strong></summary>

**Answer:**
In Spring Data R2DBC, `R2dbcTransactionManager` manages transactions using Reactor's **`Context`**. When a `@Transactional` method is called, Spring intercepts the publisher and attaches a transaction context containing the active R2DBC `Connection`. When repository operations execute downstream, they extract the connection from the subscriber's `ContextView`, ensuring all queries in the reactive chain execute within the same transactional session.
</details>

<details>
<summary><strong>Q3: Why doesn't R2DBC support Lazy Loading or Dirty Checking like Hibernate?</strong></summary>

**Answer:**
Lazy loading in Hibernate relies on transparently intercepting getter methods (e.g. `order.getItems()`) and executing a synchronous SQL query behind the scenes. In a reactive world, every database query is asynchronous and must return a `Mono` or `Flux`. A standard getter cannot return a bare collection lazily without blocking the calling thread. Therefore, R2DBC intentionally avoids ORM complexity and functions as a lightweight data mapper.
</details>

<details>
<summary><strong>Q4: What happens if an entity with a non-null UUID is passed to `repository.save()` in Spring Data R2DBC?</strong></summary>

**Answer:**
Spring Data inspects the entity's `@Id` property. If the ID is null (or primitive 0), Spring assumes the entity is new and executes an SQL `INSERT`. If the ID is already populated (as is common with client-generated UUIDs), Spring assumes the entity already exists in the database and executes an SQL `UPDATE`. If the row does not exist, the update affects 0 rows and fails. To fix this, the entity must implement `Persistable<ID>` and return `true` from `isNew()`.
</details>

<details>
<summary><strong>Q5: What is the purpose of `r2dbc-pool`?</strong></summary>

**Answer:**
The core R2DBC driver specifications do not include connection pooling. Without `r2dbc-pool`, every database operation opens and closes a new physical TCP socket to the database server. `r2dbc-pool` provides a non-blocking connection pool implementation that reuses open sockets, reducing latency and preventing database connection exhaustion.
</details>

<details>
<summary><strong>Q6: How does Backpressure propagate from a WebFlux HTTP response to a PostgreSQL database via R2DBC?</strong></summary>

**Answer:**
When a client requests an NDJSON stream (`GET /orders`), WebFlux requests a batch of items (e.g. 32). R2DBC receives this demand via `Subscription.request(32)` and reads 32 rows from the PostgreSQL socket cursor. If the client network slows down, WebFlux stops requesting items. R2DBC ceases pulling from the socket buffer, and TCP flow control halts the PostgreSQL server from sending more packets until the client catches up.
</details>

<details>
<summary><strong>Q7: Can you write custom SQL queries with parameters in Spring Data R2DBC repositories?</strong></summary>

**Answer:**
Yes. Use the `@Query` annotation:
`@Query("SELECT * FROM users WHERE status = :status AND age > :age")`
`Flux<User> findActiveUsers(String status, int age);`
Spring Data binds parameters non-blockingly using native driver bind markers (`$1`, `$2` in Postgres).
</details>

<details>
<summary><strong>Q8: What is `R2dbcEntityTemplate`?</strong></summary>

**Answer:**
`R2dbcEntityTemplate` is the central low-level programmatic helper in Spring Data R2DBC (analogous to `JdbcTemplate` or `MongoTemplate`). It provides fluent methods (`select()`, `insert()`, `update()`, `delete()`) and `Criteria` builders for executing dynamic queries without repository interfaces.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic R2DBC Entity and Repository</strong></summary>

**Task**: Define a record `Customer(@Id Long id, String name)` and an interface extending `ReactiveCrudRepository<Customer, Long>`.

```java
// Solution:
@Table("customers")
public record Customer(@Id Long id, String name) {}

public interface CustomerRepository extends ReactiveCrudRepository<Customer, Long> {
    Flux<Customer> findByName(String name);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Configure R2DBC Pool in YAML</strong></summary>

**Task**: Configure `application.yaml` for a PostgreSQL database with a connection pool size between 5 and 20.

```yaml
# Solution:
spring:
  r2dbc:
    url: r2dbc:pool:postgresql://localhost:5432/testdb
    username: myuser
    password: mypassword
    pool:
      initial-size: 5
      max-size: 20
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Reactive Transactional Rollback Test</strong></summary>

**Task**: Write a service method with `@Transactional` that saves a customer and then throws a `RuntimeException`, verifying that the customer was not persisted.

```java
// Solution:
@Service
public class CustomerTxService {
    @Autowired CustomerRepository repo;

    @Transactional
    public Mono<Customer> saveAndFail(Customer c) {
        return repo.save(c)
                .flatMap(saved -> Mono.error(new RuntimeException("Simulated Failure")));
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Dynamic Query with `R2dbcEntityTemplate`</strong></summary>

**Task**: Use `R2dbcEntityTemplate` to query customers whose name starts with `"A"` and whose ID is greater than 100.

```java
// Solution:
public Flux<Customer> findSpecialCustomers(R2dbcEntityTemplate template) {
    Criteria criteria = Criteria.where("name").like("A%")
            .and("id").greaterThan(100);
    return template.select(Customer.class)
            .matching(Query.query(criteria))
            .all();
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Solve Assigned UUID `Persistable` Update Bug</strong></summary>

**Task**: Implement an entity `TransactionRecord` with a pre-assigned UUID that implements `Persistable<UUID>`, ensuring `repository.save()` issues an SQL `INSERT` rather than an `UPDATE`.
</details>

<details>
<summary><strong>Exercise 6 (Hard): PostgreSQL Reactive Notification Listener</strong></summary>

**Task**: Use `PostgresqlConnection` from the underlying R2DBC driver to issue `LISTEN order_events` and stream incoming PostgreSQL `NOTIFY` events as a reactive `Flux<String>` to a WebFlux SSE endpoint.
</details>

---

## 12. Summary & Quick Reference

- **Core Paradigm**: Pure non-blocking relational database access via Netty.
- **Connection Factory**: `io.r2dbc.spi.ConnectionFactory` + `r2dbc-pool`.
- **Not an ORM**: No lazy loading, dirty checking, or `@OneToMany` cascades.
- **Transactions**: `@Transactional` binds to Reactor `Context`, not `ThreadLocal`.
- **Migrations**: Always run Flyway/Liquibase via standard JDBC drivers during startup.

---

## 13. Further Reading

- [Spring Data R2DBC Official Reference Guide](https://docs.spring.io/spring-data/r2dbc/reference/)
- [R2DBC Official Specification](https://r2dbc.io/spec/1.0.0.RELEASE/spec/html/)
- [PostgreSQL R2DBC Driver Documentation](https://github.com/pgjdbc/r2dbc-postgresql)
- [Managing Reactive Transactions in Spring](https://spring.io/blog/2019/05/16/reactive-transactions-with-spring)
