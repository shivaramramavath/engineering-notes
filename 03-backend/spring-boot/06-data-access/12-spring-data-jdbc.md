# Spring Data JDBC: Opinionated DDD Persistence Without ORM Complexity

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | `01-jdbc-template-and-jdbc-client.md`, `03-spring-data-jpa-fundamentals.md`, Domain-Driven Design (DDD) basics |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Spring Data JDBC 3.4.x, PostgreSQL 16 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Domain-Driven Design Aggregate Root with @MappedCollection](#51-domain-driven-design-aggregate-root-with-mappedcollection)
   - [5.2 Value Objects with @Embedded in Spring Data JDBC](#52-value-objects-with-embedded-in-spring-data-jdbc)
   - [5.3 Immutable Aggregates Using Java 21 Records](#53-immutable-aggregates-using-java-21-records)
   - [5.4 Anti-Pattern: Attempting Bidirectional Relationships or Lazy Loading](#54-anti-pattern-attempting-bidirectional-relationships-or-lazy-loading)
   - [5.5 Custom SQL Queries and Aggregate Event Publishing](#55-custom-sql-queries-and-aggregate-event-publishing)
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
Hibernate and Spring Data JPA provide powerful abstractions, but they come with significant conceptual and operational complexity: First-Level Caching, ByteBuddy proxies, automatic dirty checking, `LazyInitializationException`, detached entity merging, and N+1 query surprises. In many microservices and domain-driven systems, developers spend more time debugging Hibernate lifecycle quirks than writing business features.

### Historical Evolution
The Spring Data team introduced **Spring Data JDBC** to provide a fundamentally simpler persistence alternative. Unlike JPA, Spring Data JDBC has **no session, no proxy generation, no lazy loading, and no dirty checking**. It adheres strictly to the Domain-Driven Design (DDD) concept of **Aggregate Roots**: an aggregate is loaded completely in memory, mutated via pure domain logic, and persisted explicitly by saving the root.

### Real-World Relevance
For transactional microservices, event-driven architectures, and domain-driven projects that want the productivity of declarative repositories without the unpredictable caching and proxy behaviors of Hibernate, Spring Data JDBC offers the ideal middle ground: the simplicity of SQL with the convenience of Spring Data repositories.

---

## 2. Core Concepts

### Aggregate Root Principle
In DDD, an Aggregate is a cluster of associated objects treated as a single unit for data changes. Every aggregate has an **Aggregate Root** (the only entity external code is allowed to hold a reference to). In Spring Data JDBC:
1. Only Aggregate Roots have repositories (`OrderRepository`, but *not* `OrderItemRepository`).
2. Saving the Aggregate Root saves the entire tree of child entities.
3. Deleting the Aggregate Root deletes all associated child rows.

### Zero Magic Guarantees
- **No Session / Persistence Context**: There is no L1 cache; entities are never "attached" or "managed".
- **No Lazy Loading**: When an aggregate is loaded, its entire object tree is fetched immediately.
- **No Automatic Dirty Checking**: Changes to entities are never flushed implicitly; you must explicitly call `repository.save(aggregate)`.
- **No Bidirectional Associations**: References flow strictly downwards from the root to children.

### Core Annotations
- `@Id`: Marks the primary key field.
- `@Table`: Configures the target database table name.
- `@Column`: Configures the column name.
- `@MappedCollection`: Configures child entity collections (specifying `idColumn` and `keyColumn`).
- `@Embedded`: Maps value objects into columns of the parent table.
- `@Version`: Supports optimistic concurrency control.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         Spring Data JDBC Aggregate Lifecycle                       |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  1. Loading: orderRepository.findById(42L)                                         |
|     - Executes SELECT FROM orders WHERE id = 42                                    |
|     - Immediately executes SELECT FROM order_items WHERE order_id = 42             |
|     - Instantiates clean, immutable POJOs or Java Records (NO Proxies!)            |
|                                                                                    |
|  2. Mutation: order.addItem("SKU-100", 2)                                          |
|     - Pure in-memory Java state change. ZERO database connection involvement!       |
|                                                                                    |
|  3. Persisting: orderRepository.save(order)                                        |
|     - Issues UPDATE orders SET ... WHERE id = 42                                   |
|     - Synchronizes children: Deletes removed items, inserts/updates current items   |
|     - Clean SQL executed immediately without deferred action queues                |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-data-jdbc</artifactId>
</dependency>
```

### Basic Aggregate Definition
```java
@Table("orders")
public class Order {

    @Id
    private Long id;

    private String customerEmail;

    @MappedCollection(idColumn = "order_id", keyColumn = "item_index")
    private List<OrderItem> items = new ArrayList<>();

    @Version
    private Long version;
}
```

---

## 5. Practical Examples

### 5.1 Domain-Driven Design Aggregate Root with @MappedCollection
```java
package com.example.jdbc.domain;

import org.springframework.data.annotation.Id;
import org.springframework.data.annotation.Version;
import org.springframework.data.relational.core.mapping.Column;
import org.springframework.data.relational.core.mapping.MappedCollection;
import org.springframework.data.relational.core.mapping.Table;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

@Table("orders")
public class Order {

    @Id
    private Long id;

    @Column("order_number")
    private String orderNumber;

    @Column("created_at")
    private Instant createdAt = Instant.now();

    @Version
    private Long version;

    // Direct 1-to-N mapping: order_id is foreign key; item_index tracks list ordering
    @MappedCollection(idColumn = "order_id", keyColumn = "item_index")
    private List<OrderItem> items = new ArrayList<>();

    public Order(String orderNumber) {
        this.orderNumber = orderNumber;
    }

    public void addItem(String sku, int quantity, BigDecimal unitPrice) {
        this.items.add(new OrderItem(sku, quantity, unitPrice));
    }

    public Long getId() { return id; }
    public String getOrderNumber() { return orderNumber; }
    public List<OrderItem> getItems() { return Collections.unmodifiableList(items); }
}
```

```java
package com.example.jdbc.domain;

import org.springframework.data.relational.core.mapping.Table;
import java.math.BigDecimal;

@Table("order_items")
public class OrderItem {

    // Note: Child entities do NOT require an @Id in Spring Data JDBC if tracked by parent list!
    private String sku;
    private int quantity;
    private BigDecimal unitPrice;

    public OrderItem(String sku, int quantity, BigDecimal unitPrice) {
        this.sku = sku;
        this.quantity = quantity;
        this.unitPrice = unitPrice;
    }

    public String getSku() { return sku; }
    public int getQuantity() { return quantity; }
    public BigDecimal getUnitPrice() { return unitPrice; }
}
```

### 5.2 Value Objects with @Embedded in Spring Data JDBC
```java
package com.example.jdbc.domain;

import org.springframework.data.relational.core.mapping.Column;
import org.springframework.data.relational.core.mapping.Embedded;
import org.springframework.data.relational.core.mapping.Table;
import org.springframework.data.annotation.Id;

public record ShippingAddress(
    @Column("ship_street") String street,
    @Column("ship_city") String city,
    @Column("ship_zip") String zipCode
) {}

@Table("deliveries")
public class Delivery {

    @Id
    private Long id;

    private String recipientName;

    // Inlined directly into the 'deliveries' table columns
    @Embedded(onEmpty = Embedded.OnEmpty.USE_NULL)
    private ShippingAddress address;

    public Delivery(String recipientName, ShippingAddress address) {
        this.recipientName = recipientName;
        this.address = address;
    }

    public Long getId() { return id; }
    public ShippingAddress getAddress() { return address; }
}
```

### 5.3 Immutable Aggregates Using Java 21 Records
```java
package com.example.jdbc.domain;

import org.springframework.data.annotation.Id;
import org.springframework.data.annotation.Version;
import org.springframework.data.relational.core.mapping.MappedCollection;
import org.springframework.data.relational.core.mapping.Table;

import java.time.Instant;
import java.util.Set;

/**
 * Spring Data JDBC natively supports 100% immutable Java 21 Records as Aggregates!
 * (Impossible in JPA/Hibernate due to proxy and no-arg constructor requirements).
 */
@Table("surveys")
public record Survey(
    @Id Long id,
    String title,
    Instant publishedAt,
    @MappedCollection(idColumn = "survey_id") Set<Question> questions,
    @Version Long version
) {
    public Survey withTitle(String newTitle) {
        return new Survey(id, newTitle, publishedAt, questions, version);
    }
}

@Table("survey_questions")
public record Question(String prompt, boolean mandatory) {}
```

### 5.4 Anti-Pattern: Attempting Bidirectional Relationships or Lazy Loading
```java
// =========================================================================
// ANTI-PATTERN: Attempting to treat Spring Data JDBC like JPA
// =========================================================================
@Table("order_items")
public class BadOrderItem {
    @Id
    private Long id;

    // CATASTROPHIC DESIGN FLAW IN SPRING DATA JDBC:
    // Spring Data JDBC strictly prohibits child-to-parent entity references!
    // There is no @ManyToOne! Putting an Order reference here creates a circular aggregate!
    private Order parentOrder; // Throws InvalidDataAccessApiUsageException at startup!
}

// =========================================================================
// PRODUCTION FIX: Unidirectional Ownership or AggregateReference<T, ID>
// =========================================================================
import org.springframework.data.jdbc.core.mapping.AggregateReference;

@Table("shipments")
public class Shipment {
    @Id
    private Long id;

    // Cross-aggregate link using weak typed reference: Stores only order_id foreign key!
    private AggregateReference<Order, Long> orderRef;

    public void assignOrder(Long orderId) {
        this.orderRef = AggregateReference.to(orderId);
    }
}
```

### 5.5 Custom SQL Queries and Aggregate Event Publishing
```java
package com.example.jdbc.repository;

import com.example.jdbc.domain.Order;
import org.springframework.data.jdbc.repository.query.Modifying;
import org.springframework.data.jdbc.repository.query.Query;
import org.springframework.data.repository.ListCrudRepository;
import org.springframework.data.repository.query.Param;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.util.Optional;

@Repository
public interface OrderJdbcRepository extends ListCrudRepository<Order, Long> {

    // Derived queries operate identically to Spring Data JPA
    Optional<Order> findByOrderNumber(String orderNumber);

    // Native SQL with named parameters
    @Query("SELECT * FROM orders WHERE created_at >= :since ORDER BY id DESC")
    List<Order> findRecentOrders(@Param("since") java.time.Instant since);

    @Modifying
    @Query("UPDATE orders SET status = :status WHERE id = :id")
    int updateOrderStatus(@Param("id") Long id, @Param("status") String status);
}
```

---

## 6. Best Practices

1. **Model Aggregates Strictly Along Transactional Boundaries**: An aggregate should encapsulate only entities that must change together to enforce business invariants.
2. **Use `AggregateReference<T, ID>` for Cross-Aggregate Links**: Never nest one Aggregate Root inside another Aggregate Root; reference external aggregates solely by ID.
3. **Prefer Immutable Java 21 Records**: Leverage Spring Data JDBC's native record support to build thread-safe, immutable domain models with zero boilerplate.
4. **Always Call `repository.save(aggregate)` Explicitly**: There is no dirty checking; omitting `save()` means changes will simply be discarded.
5. **Keep Aggregates Small**: Because saving an aggregate synchronizes all child collections, large aggregates with thousands of children create excessive delete-insert SQL operations.
6. **Use `@Version` for Concurrency**: Always include `@Version private Long version` to protect against lost updates.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Expecting Automatic Dirty Checking** | Mutating an aggregate property without invoking `repository.save()` does not write to the DB. | Always invoke `repository.save(aggregate)` after state changes. |
| **Attempting Bidirectional Child-to-Parent Links** | Spring Data JDBC does not support `@ManyToOne` back-references to parents. | Keep child entities pointing strictly forward or use `AggregateReference`. |
| **Creating Repositories for Child Entities** | Only Aggregate Roots are allowed repositories; creating a repository for child tables breaks DDD boundaries. | Access and mutate children exclusively through the Aggregate Root repository. |
| **Oversized Aggregates (e.g. 10,000 items in a list)** | Spring Data JDBC deletes and re-inserts child collections upon update, killing performance on huge lists. | Break into separate independent aggregate roots using `AggregateReference`. |
| **Expecting Lazy Loading** | Spring Data JDBC loads the entire aggregate immediately. | Design small, tightly focused aggregates. |
| **Mixing JPA and Spring Data JDBC Annotations** | Using `jakarta.persistence.*` annotations (`@Entity`, `@OneToMany`) has zero effect in Spring Data JDBC. | Use `org.springframework.data.relational.core.mapping.*` annotations. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Proxy / Reflection Overhead**: Direct mapping into constructors or records achieves raw JDBC throughput without Hibernate proxy initialization or bytecode interception.
- **Predictable SQL**: Eliminates hidden queries; every database operation corresponds directly to a method invoked on the repository.

### Security
- **Domain Invariant Enforcement**: Because child entities can only be accessed through the Aggregate Root, invalid domain states (e.g. adding items to a cancelled order) can be rejected deterministically by domain logic.

---

## 9. Comparison / When to Use What

| Feature | Spring Data JDBC | Spring Data JPA (Hibernate) |
| :--- | :--- | :--- |
| **Session / L1 Cache** | **None** | Yes (Persistence Context) |
| **Lazy Loading** | **None (Loads full aggregate)** | Yes (`FetchType.LAZY`) |
| **Dirty Checking** | **None (Must call `save()`)** | Automatic at transaction commit |
| **Java 21 Records Support** | **Full Native Support** | Limited (Projections only; entities cannot be records) |
| **Mental Overhead** | Very Low (Plain SQL semantics) | High (Complex lifecycle & proxy traps) |
| **Best Used For** | Microservices, DDD, Event-Driven, Simple CRUD | Complex enterprise legacy graphs, polymorphics |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental design philosophy of Spring Data JDBC compared to Spring Data JPA?</strong></summary>

Spring Data JDBC is designed around **simplicity and Domain-Driven Design (DDD)**. It deliberately excludes the Hibernate Persistence Context (no First-Level Cache), eliminates lazy loading, removes runtime bytecode proxies, and abandons automatic dirty checking. Data is loaded and saved strictly as complete, self-contained aggregates with predictable SQL execution.
</details>

<details>
<summary><strong>2. How does Spring Data JDBC handle updates to child collections?</strong></summary>

When an Aggregate Root is saved via `repository.save()`, Spring Data JDBC issues an `UPDATE` for the root row, deletes all existing child rows belonging to that aggregate in the database, and re-inserts the current list/set of child entities. This is why aggregates must remain reasonably small.
</details>

<details>
<summary><strong>3. Why can Java 21 Records be used as entities in Spring Data JDBC but not in Spring Data JPA?</strong></summary>

JPA requires entities to have mutable state, a no-argument constructor, and non-final classes/methods so that Hibernate can create runtime ByteBuddy proxies for lazy loading and dirty checking. Java Records are final and immutable, violating JPA rules. Spring Data JDBC does not use proxies or dirty checking, allowing it to instantiate records directly from query result sets.
</details>

<details>
<summary><strong>4. How do you model relationships between two different Aggregate Roots in Spring Data JDBC?</strong></summary>

You never hold a direct object reference to another Aggregate Root. Instead, you use `AggregateReference<TargetRoot, IdType>` or store the raw foreign ID (`Long targetId`). This maintains strict aggregate boundaries and prevents loading massive transitive object graphs.
</details>

<details>
<summary><strong>5. What happens if you forget to call <code>repository.save(aggregate)</code> after modifying an entity in Spring Data JDBC?</strong></summary>

Unlike JPA, which automatically writes mutations to the database upon transaction commit via dirty checking, Spring Data JDBC has no tracking mechanism. If you do not explicitly invoke `repository.save()`, your changes remain in memory and are discarded when the method completes.
</details>

<details>
<summary><strong>6. What is the purpose of the <code>@MappedCollection</code> annotation?</strong></summary>

`@MappedCollection` configures a one-to-many relationship in Spring Data JDBC. It specifies the `idColumn` (the foreign key column name in the child table pointing to the parent) and optionally the `keyColumn` (which stores the array index or map key for ordered collections).
</details>

<details>
<summary><strong>7. Does Spring Data JDBC support optimistic concurrency control?</strong></summary>

Yes. By adding a field annotated with `@Version` (`Long` or `Integer`) to the Aggregate Root, Spring Data JDBC appends `WHERE version = ?` to `UPDATE` and `DELETE` statements, throwing `OptimisticLockingFailureException` if a concurrent modification occurs.
</details>

<details>
<summary><strong>8. Why is there no repository for child entities in Spring Data JDBC?</strong></summary>

In Domain-Driven Design, external clients must interact with the aggregate exclusively through the Aggregate Root to ensure business invariants are respected. Exposing independent repositories for child entities bypasses the root and violates aggregate boundary integrity.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Spring Data JDBC Aggregate Root</strong></summary>

**Task**: Define an Aggregate Root `BlogPost` with fields `id`, `title`, and `content` using Spring Data JDBC annotations.

```java
@Table("blog_posts")
public record BlogPost(
    @Id Long id,
    String title,
    String content
) {}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): CRUD Repository Declaration</strong></summary>

**Task**: Declare a repository for `BlogPost` extending `ListCrudRepository` and add a derived query method to find posts by title.

```java
public interface BlogPostRepository extends ListCrudRepository<BlogPost, Long> {
    List<BlogPost> findByTitleContaining(String keyword);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Parent-Child Collection Mapping</strong></summary>

**Task**: Map a `Department` aggregate with a `Set<Employee>` using `@MappedCollection(idColumn = "department_id")`.

```java
@Table("departments")
public class Department {
    @Id
    private Long id;
    private String name;

    @MappedCollection(idColumn = "department_id")
    private Set<Employee> employees = new HashSet<>();

    public Department(String name) { this.name = name; }
    public void addEmployee(String name) { employees.add(new Employee(name)); }
}

@Table("employees")
public record Employee(String name) {}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Optimistic Locking Test in Spring Data JDBC</strong></summary>

**Task**: Write an integration test asserting that saving a stale aggregate version throws `OptimisticLockingFailureException`.

```java
@Test
void testOptimisticLocking() {
    BlogPost post = repository.save(new BlogPost(null, "Title", "Content", 0L));

    BlogPost copy1 = repository.findById(post.id()).orElseThrow();
    BlogPost copy2 = repository.findById(post.id()).orElseThrow();

    repository.save(copy1.withTitle("Updated Title"));

    assertThatThrownBy(() -> repository.save(copy2.withTitle("Conflicting Title")))
        .isInstanceOf(OptimisticLockingFailureException.class);
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): AggregateReference Cross-Aggregate Linking</strong></summary>

**Task**: Implement a `Customer` aggregate and an `Account` aggregate, where `Account` references `Customer` using `AggregateReference<Customer, Long>`.

```java
@Table("customers")
public record Customer(@Id Long id, String name) {}

@Table("accounts")
public class Account {
    @Id
    private Long id;
    private String accountNumber;
    private AggregateReference<Customer, Long> customer;

    public Account(String accountNumber, Long customerId) {
        this.accountNumber = accountNumber;
        this.customer = AggregateReference.to(customerId);
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Aggregate Domain Event Publishing</strong></summary>

**Task**: Implement an aggregate root that extends `AbstractAggregateRoot` to automatically publish a `CustomerRegisteredEvent` upon being saved by the repository.

```java
@Table("customers")
public class RegisteredCustomer extends AbstractAggregateRoot<RegisteredCustomer> {
    @Id
    private Long id;
    private String email;

    public RegisteredCustomer(String email) {
        this.email = email;
        registerEvent(new CustomerRegisteredEvent(email));
    }
}
public record CustomerRegisteredEvent(String email) {}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Philosophy:   Strict DDD Aggregate Roots, Plain SQL, Zero ORM magic                   |
| Invariants:   NO L1 cache, NO lazy loading, NO proxies, NO automatic dirty checking    |
| Persistence:  Always call repository.save(aggregate) explicitly!                      |
| Collections:  @MappedCollection(idColumn = "parent_id", keyColumn = "idx")            |
| Relations:    Use AggregateReference<Root, ID> for cross-aggregate boundaries         |
| Records:      100% native support for Java 21 immutable Records as Entities           |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Data JDBC Official Reference Documentation](https://docs.spring.io/spring-data/jdbc/reference/)
- [Jens Schauder: Spring Data JDBC - Do We Need Another ORM?](https://spring.io/blog/2018/09/17/introducing-spring-data-jdbc)
- [Domain-Driven Design: Tackling Complexity in the Heart of Software (Eric Evans)](https://www.domainlanguage.com/ddd/)
- [Domain Events with Spring Data AbstractAggregateRoot](https://docs.spring.io/spring-data/commons/reference/repositories/core-domain-events.html)
