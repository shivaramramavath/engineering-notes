# Spring Data JPA Fundamentals: EntityManager, Persistence Context & Lifecycle States

| Metadata | Details |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | `01-jdbc-template-and-jdbc-client.md`, Object-Relational Mapping (ORM) basics |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Hibernate 6.6.x, PostgreSQL 16, Spring Data JPA 3.4.x |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Entity Definition and Basic Lifecycle Transitions](#51-entity-definition-and-basic-lifecycle-transitions)
   - [5.2 Dirty Checking in Action Without Explicit Save](#52-dirty-checking-in-action-without-explicit-save)
   - [5.3 Detached Entity Reattachment with Merge](#53-detached-entity-reattachment-with-merge)
   - [5.4 Anti-Pattern: Calling save() Inside an Active Transaction](#54-anti-pattern-calling-save-inside-an-active-transaction)
   - [5.5 Explicit Flush Control and Bulk Detach for High-Throughput Batches](#55-explicit-flush-control-and-bulk-detach-for-high-throughput-batches)
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
In relational databases, data is organized in tables, rows, primary keys, and foreign keys. In object-oriented programming (Java), data is structured in object graphs, inheritance hierarchies, polymorphic references, and encapsulation boundaries. Translating between these two paradigms—known as the **Object-Relational Impedance Mismatch**—requires thousands of lines of manual mapping and state-tracking code when using raw JDBC.

### Historical Evolution
The Java Community Process introduced JPA (Jakarta Persistence API) to standardize object-relational mapping, with **Hibernate** serving as the predominant reference implementation. Spring Data JPA builds on top of JPA, eliminating boilerplate DAO implementations by generating repository proxies at runtime. In modern Spring Boot 3.4+ / Hibernate 6.6+, the entire internal query translation engine has been rewritten to use the **Semantic Query Model (SQM)**, generating tighter, dialect-specific SQL with type-checked AST transformations.

### Real-World Relevance
Understanding the JPA entity lifecycle and the persistence context is the dividing line between developers who write fragile, slow database applications and engineers who build high-throughput enterprise systems. Mastering dirty checking, flush timing, and entity states prevents common production issues such as phantom updates, premature flushes, and memory bloat.

---

## 2. Core Concepts

### EntityManager & Persistence Context
- **Persistence Context**: An in-memory cache (First-Level Cache, L1) that tracks managed entity instances during an active transaction. Each entity has a unique identity (`@Id`) within this context.
- **EntityManager**: The primary interface used to interact with the persistence context (e.g., `persist`, `find`, `merge`, `remove`, `flush`, `detach`).

### Entity Lifecycle States
```
+------------------------------------------------------------------------------------+
|                               Entity Lifecycle States                              |
+------------------------------------------------------------------------------------+
|                                                                                    |
|       [ Transient / New ]  ---- persist() ---->  [ Managed / Persistent ]          |
|                ^                                          |        ^               |
|                |                                    find()|        | merge()       |
|                |                                          v        |               |
|                +----------- clear() / detach() ------- [ Detached ]                |
|                                                           |                        |
|                                                        remove()                    |
|                                                           v                        |
|                                                      [ Removed ]                   |
+------------------------------------------------------------------------------------+
```

1. **Transient (New)**: A newly instantiated Java object (`new User()`). It has no persistent database identity (`@Id` is typically null) and is not associated with any persistence context.
2. **Managed (Persistent)**: The entity has a database identity and is tracked by the persistence context. Any state mutations to its fields are automatically synchronized with the database upon transaction commit.
3. **Detached**: The entity has a database identity, but its persistence context has closed (or it was explicitly evicted via `detach()` or `clear()`). State changes are no longer tracked.
4. **Removed**: Marked for deletion from the database upon the next flush/commit.

### Automatic Dirty Checking
When an entity enters the Managed state, Hibernate takes an initial snapshot of its property values. At flush time, Hibernate compares the current entity state against the snapshot. If any attribute differs, Hibernate automatically generates and executes an SQL `UPDATE` statement.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                        Persistence Context & Flush Pipeline                        |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  1. Transaction Begins (@Transactional)                                            |
|     EntityManager is bound to current thread via TransactionSynchronizationManager |
|                                                                                    |
|  2. Loading Entity: User user = em.find(User.class, 1L);                           |
|     - Checks L1 Cache: If found, returns existing instance immediately (No SQL!)   |
|     - If not found: Executes SELECT SQL, stores entity in IdentityMap,             |
|       and saves an internal object snapshot in EntityEntry                         |
|                                                                                    |
|  3. Modifying Entity: user.setEmail("new@example.com");                            |
|     - Pure in-memory field mutation. NO SQL IS EXECUTED YET!                       |
|                                                                                    |
|  4. Transaction Pre-Commit / Flush Trigger:                                        |
|     - Hibernate executes Dirty Check: compares user state against EntityEntry snapshot|
|     - Detects difference in 'email' field                                          |
|     - Generates UPDATE SQL into ActionQueue                                        |
|     - Executes ActionQueue in JDBC batch order: INSERT -> UPDATE -> DELETE         |
|                                                                                    |
|  5. Transaction Commits: DB Connection commits, Persistence Context is cleared     |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Entity Declaration
```java
@Entity
@Table(name = "users")
public class User {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false, unique = true)
    private String email;

    @Column(nullable = false)
    private String name;

    // Protected or public no-args constructor required by JPA specification
    protected User() {}

    public User(String email, String name) {
        this.email = email;
        this.name = name;
    }

    // Getters and business setters...
}
```

---

## 5. Practical Examples

### 5.1 Entity Definition and Basic Lifecycle Transitions
```java
package com.example.jpa.service;

import com.example.jpa.domain.Customer;
import jakarta.persistence.EntityManager;
import jakarta.persistence.PersistenceContext;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class CustomerLifecycleService {

    @PersistenceContext
    private EntityManager entityManager;

    @Transactional
    public Long demonstrateTransitions() {
        // 1. Transient State: New POJO in heap memory, no database record exists
        Customer customer = new Customer("alice@example.com", "Alice Smith");

        // 2. Transition to Managed State: Assigned to persistence context
        entityManager.persist(customer);
        // Note: Generated ID is available immediately for IDENTITY or SEQUENCE strategies

        // 3. Mutating Managed State: No save() call needed!
        customer.setName("Alice Cooper");

        // 4. Detaching: Evict from persistence context
        entityManager.detach(customer);

        // 5. This mutation will NOT be saved because entity is now detached!
        customer.setName("Alice In Chains");

        // 6. Reattaching: Merge creates a new managed copy with the updated state
        Customer managedCustomer = entityManager.merge(customer);
        
        return managedCustomer.getId();
    } // Transaction commits here: dirty checking flushes updates to DB
}
```

### 5.2 Dirty Checking in Action Without Explicit Save
```java
package com.example.jpa.service;

import com.example.jpa.domain.BankAccount;
import com.example.jpa.repository.BankAccountRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;

@Service
public class BankAccountService {

    private final BankAccountRepository repository;

    public BankAccountService(BankAccountRepository repository) {
        this.repository = repository;
    }

    @Transactional
    public void deposit(Long accountId, BigDecimal amount) {
        // 1. Entity loaded into Managed state
        BankAccount account = repository.findById(accountId)
            .orElseThrow(() -> new IllegalArgumentException("Account not found: " + accountId));

        // 2. Perform business logic mutating internal state
        account.credit(amount);

        // 3. NOTICE: We DO NOT call repository.save(account)!
        // When @Transactional method completes successfully, Hibernate's
        // dirty-checking mechanism detects the balance change and issues an UPDATE SQL!
    }
}
```

### 5.3 Detached Entity Reattachment with Merge
```java
package com.example.jpa.service;

import com.example.jpa.domain.UserProfile;
import jakarta.persistence.EntityManager;
import jakarta.persistence.PersistenceContext;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class ProfileUpdateService {

    @PersistenceContext
    private EntityManager entityManager;

    /**
     * Demonstrates merging a detached entity sent from a REST controller payload.
     */
    @Transactional
    public UserProfile updateProfile(UserProfile detachedProfile) {
        // Warning: merge() does NOT make 'detachedProfile' managed!
        // It copies state into a newly loaded/managed instance and returns it.
        UserProfile managedProfile = entityManager.merge(detachedProfile);

        managedProfile.setLastLoginIp("192.168.1.100");
        return managedProfile; // Return the MANAGED instance
    }
}
```

### 5.4 Anti-Pattern: Calling save() Inside an Active Transaction
```java
// =========================================================================
// ANTI-PATTERN: Redundant repository.save() on Managed Entities
// =========================================================================
@Service
public class BadOrderService {

    private final OrderRepository orderRepository;

    public BadOrderService(OrderRepository orderRepository) {
        this.orderRepository = orderRepository;
    }

    @Transactional
    public void markDelivered(Long orderId) {
        Order order = orderRepository.findById(orderId).orElseThrow();
        order.setStatus(OrderStatus.DELIVERED);

        // REDUNDANT AND MISLEADING:
        // 'order' is ALREADY in the Managed state!
        // Calling save() forces Spring Data JPA to invoke em.merge(order),
        // which performs unnecessary entity state lookups and copies.
        orderRepository.save(order); 
    }
}

// =========================================================================
// PRODUCTION FIX: Rely on Transactional Dirty Checking
// =========================================================================
@Service
public class GoodOrderService {

    private final OrderRepository orderRepository;

    public GoodOrderService(OrderRepository orderRepository) {
        this.orderRepository = orderRepository;
    }

    @Transactional
    public void markDelivered(Long orderId) {
        Order order = orderRepository.findById(orderId).orElseThrow();
        // Domain model state mutation - Hibernate automatically detects this at commit!
        order.markDelivered();
    }
}
```

### 5.5 Explicit Flush Control and Bulk Detach for High-Throughput Batches
```java
package com.example.jpa.service;

import com.example.jpa.domain.AuditRecord;
import jakarta.persistence.EntityManager;
import jakarta.persistence.PersistenceContext;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Service
public class BatchAuditService {

    @PersistenceContext
    private EntityManager entityManager;

    private static final int BATCH_SIZE = 50;

    /**
     * Persists 10,000 entities without exhausting JVM heap memory
     * by periodically flushing and clearing the persistence context.
     */
    @Transactional
    public void ingestAuditBatch(List<AuditRecord> records) {
        for (int i = 0; i < records.size(); i++) {
            entityManager.persist(records.get(i));

            // Flush changes to DB and clear L1 cache every BATCH_SIZE items
            if (i > 0 && i % BATCH_SIZE == 0) {
                entityManager.flush(); // Sends pending SQL INSERTs to database
                entityManager.clear(); // Evicts all managed entities from First-Level Cache!
            }
        }
    }
}
```

---

## 6. Best Practices

1. **Understand Dirty Checking**: Never call `repository.save()` when mutating an entity that was already loaded within an active `@Transactional` method.
2. **Batch Large Ingestions with `flush()` and `clear()`**: If creating or mutating thousands of entities in a single transaction, clear the persistence context periodically to avoid `OutOfMemoryError`.
3. **Use Protected No-Arg Constructors**: JPA requires a no-argument constructor for proxy instantiation; mark it `protected` to discourage manual empty instantiation in business code.
4. **Avoid Modifying Entities in Read-Only Transactions**: Annotate read operations with `@Transactional(readOnly = true)`. Hibernate turns off dirty-check snapshot creation, saving memory and CPU cycles.
5. **Never Use Lombok `@Data` or `@EqualsAndHashCode` on Entities**: Lombok's generated `equals()` and `hashCode()` break JPA identity maps when using lazy associations or changing database IDs. Implement `equals`/`hashCode` based on business keys or class identity.
6. **Prefer Explicit Business Methods Over Setters**: Encapsulate state changes inside expressive domain methods (`order.cancel()`) rather than public setters.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using Lombok `@Data` on `@Entity`** | Generates `toString()`, `equals()`, and `hashCode()` that evaluate lazy relationships, triggering `LazyInitializationException` or infinite loops. | Use `@Getter`, `@Setter`, and implement custom `equals()` and `hashCode()` using immutable business identifiers. |
| **Assuming `save()` Immediately Executes SQL** | `save()` or `persist()` only registers the entity with the persistence context; SQL is deferred until flush or commit. | If immediate database execution is needed, call `saveAndFlush()` or `entityManager.flush()`. |
| **Mutating Detached Entities and Expecting Updates** | State changes to detached entities are ignored because Hibernate is no longer tracking them. | Reattach using `entityManager.merge(entity)` or load a fresh managed copy inside an active transaction. |
| **Omitting `@Transactional` on Updates** | Without an active transaction, the persistence context closes immediately, and dirty checking never triggers. | Ensure update methods are annotated with `@Transactional`. |
| **Keeping Huge Collections in Memory During Long Transactions** | The First-Level Cache holds every loaded entity until the transaction ends, leading to excessive GC pauses. | Partition bulk processing into smaller transactions or periodically invoke `entityManager.clear()`. |
| **Misunderstanding `merge()` Return Value** | `em.merge(detachedEntity)` returns a *new* managed instance; the original detached reference remains detached! | Always reassign the returned instance: `entity = em.merge(entity)`. |

---

## 8. Performance & Security Notes

### Performance
- **Read-Only Optimization**: Marking methods `@Transactional(readOnly = true)` signals Hibernate to set the `FlushMode` to `MANUAL`, skipping the expensive dirty-checking snapshot comparison across all loaded entities.
- **First-Level Cache De-duplication**: Loading the same entity by primary key ten times within a single transaction issues only one SQL query; subsequent lookups are resolved in-memory from the persistence context.

### Security
- **Mass Assignment Vulnerability**: Never bind incoming HTTP request JSON bodies directly to JPA `@Entity` classes via `@RequestBody MyEntity entity`. Attackers can mutate sensitive fields like `role` or `balance`. Always accept distinct DTOs and map them to entities.
- **Tenant Isolation**: When building multi-tenant applications, ensure the persistence context is flushed and cleared between tenant context switches to avoid cross-tenant cache bleeding.

---

## 9. Comparison / When to Use What

| Lifecycle State | Database Representation | Tracked by Persistence Context? | Can Be Mutated Safely? |
| :--- | :--- | :--- | :--- |
| **Transient (New)** | None (No DB row yet) | No | Yes (In-memory only) |
| **Managed** | Exists (or will exist at flush) | **Yes (L1 Cache)** | Yes (Triggers automatic SQL UPDATE) |
| **Detached** | Exists in database | No | Changes ignored until merged |
| **Removed** | Exists in DB until flush | Yes (Scheduled for DELETE) | No |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the JPA Persistence Context and what are its primary responsibilities?</strong></summary>

The Persistence Context is an in-memory first-level cache managed by the `EntityManager`. Its responsibilities include:
1. **Identity Mapping**: Ensuring that within a transaction, each database row corresponds to exactly one unique Java object instance.
2. **First-Level Caching**: Caching entities loaded by primary key to prevent duplicate SQL `SELECT` queries.
3. **Dirty Checking**: Tracking changes to managed entity attributes and automatically writing updates to the database upon transaction commit.
4. **Write-Behind Batching**: Queuing SQL statements into an `ActionQueue` to optimize JDBC batching.
</details>

<details>
<summary><strong>2. How does Hibernate's automatic dirty checking work under the hood?</strong></summary>

When an entity is loaded or persisted into the persistence context, Hibernate creates an internal array snapshot of its state (`EntityEntry`). At flush time, Hibernate iterates over all managed entities, compares their current property values against the cached snapshot, identifies changed fields, and schedules the corresponding SQL `UPDATE` statements into the `ActionQueue`.
</details>

<details>
<summary><strong>3. What is the difference between <code>entityManager.persist()</code> and <code>entityManager.merge()</code>?</strong></summary>

- `persist()` takes a transient entity, transitions it to the managed state, and schedules an SQL `INSERT`. Passing a detached entity throws an `EntityExistsException` or `PersistentObjectException`.
- `merge()` takes a detached (or transient) entity, looks up or loads the corresponding managed entity from the database, copies the state from the detached instance into the managed instance, and returns the managed instance. The original argument remains detached.
</details>

<details>
<summary><strong>4. Why is calling <code>repository.save()</code> unnecessary when updating an entity inside a <code>@Transactional</code> method?</strong></summary>

Any entity loaded via a Spring Data repository method inside an active transaction is already in the **Managed** state. Hibernate automatically tracks changes to managed entities via dirty checking and issues the appropriate `UPDATE` SQL statement at flush/commit time. Calling `save()` is redundant and causes Spring Data to invoke `merge()`, adding unnecessary overhead.
</details>

<details>
<summary><strong>5. What happens during an <code>entityManager.flush()</code> vs a transaction commit?</strong></summary>

- `flush()` synchronizes in-memory persistence context changes with the database by executing pending SQL statements (`INSERT`, `UPDATE`, `DELETE`) over the JDBC connection. However, the database transaction is *not* committed yet; changes can still be rolled back.
- `commit()` triggers a final `flush()` and then sends the database `COMMIT` command, making all changes durable and ending the transaction.
</details>

<details>
<summary><strong>6. Why should you avoid using Lombok's <code>@EqualsAndHashCode</code> on JPA entities?</strong></summary>

Lombok's generated `equals()` and `hashCode()` methods default to evaluating all fields. This introduces critical flaws:
1. If an unpersisted entity has a `null` ID that is subsequently populated by the database, its `hashCode()` changes while stored in a `Set`, violating the hash collection contract.
2. If fields represent lazy associations (`@OneToMany`), evaluating them inside `equals()` triggers unintended database queries or `LazyInitializationException`.
</details>

<details>
<summary><strong>7. How does <code>@Transactional(readOnly = true)</code> optimize Hibernate performance?</strong></summary>

It instructs Hibernate to set the `FlushMode` to `MANUAL`. Hibernate skips creating initial state snapshots for loaded entities, avoiding memory allocations and eliminating the CPU-intensive dirty-checking comparison at transaction completion.
</details>

<details>
<summary><strong>8. What is the purpose of <code>entityManager.clear()</code> and when should it be used?</strong></summary>

`entityManager.clear()` completely evicts all managed entities from the First-Level Cache, transitioning them to the Detached state. It is primarily used during large batch processing (e.g., iterating through thousands of records) to prevent the persistence context from exhausting heap memory.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Entity Declaration with Identity Strategy</strong></summary>

**Task**: Create a JPA Entity `Book` with fields `id` (auto-generated identity), `isbn` (unique, non-null), and `title`, adhering to proper constructor conventions.

```java
@Entity
@Table(name = "books")
public class Book {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false, unique = true)
    private String isbn;

    @Column(nullable = false)
    private String title;

    protected Book() {}

    public Book(String isbn, String title) {
        this.isbn = isbn;
        this.title = title;
    }

    // Getters and business methods...
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Transactional Dirty Checking Test</strong></summary>

**Task**: Write a service test asserting that updating an entity's property within a `@Transactional` boundary persists without calling `repository.save()`.

```java
@Test
@Transactional
void testDirtyChecking() {
    Book book = repository.save(new Book("978-0134685991", "Effective Java"));
    entityManager.flush();
    entityManager.clear();

    // Mutate state without save()
    Book managedBook = repository.findById(book.getId()).orElseThrow();
    managedBook.setTitle("Effective Java 3rd Edition");

    entityManager.flush();
    entityManager.clear();

    Book updatedBook = repository.findById(book.getId()).orElseThrow();
    assertThat(updatedBook.getTitle()).isEqualTo("Effective Java 3rd Edition");
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Safe Detached Entity Merge Pattern</strong></summary>

**Task**: Implement a repository update method that safely merges a detached entity received from an external DTO and returns the updated entity ID.

```java
@Transactional
public Long updateBookFromDetached(Book detachedBook) {
    Book managedBook = entityManager.merge(detachedBook);
    return managedBook.getId();
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Business-Key Equals and HashCode</strong></summary>

**Task**: Implement correct `equals` and `hashCode` methods on a `User` entity based on an immutable business key (`UUID` or unique `email`).

```java
@Entity
public class User {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false, unique = true, updatable = false)
    private UUID publicId = UUID.randomUUID();

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof User other)) return false;
        return publicId != null && publicId.equals(other.publicId);
    }

    @Override
    public int hashCode() {
        return getClass().hashCode();
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Batch Processing with Periodic Flush and Clear</strong></summary>

**Task**: Implement a batch migration method that processes 5,000 pending payments, flushes and clears the persistence context every 100 records, and prevents memory leaks.

```java
@Transactional
public void processBatchPayments(List<PaymentCommand> commands) {
    for (int i = 0; i < commands.size(); i++) {
        Payment payment = new Payment(commands.get(i));
        entityManager.persist(payment);

        if (i > 0 && i % 100 == 0) {
            entityManager.flush();
            entityManager.clear();
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Testing Read-Only Transaction Optimization</strong></summary>

**Task**: Write an integration test asserting that attempting to modify an entity inside a `@Transactional(readOnly = true)` method does not flush updates to the database upon completion.

```java
@Test
void testReadOnlyTransactionIgnoresMutations() {
    Long id = createTestProduct("Widget", BigDecimal.TEN);

    // Call service method marked @Transactional(readOnly = true)
    productService.attemptMutateInReadOnly(id, "Modified Widget");

    // Verify in separate transaction that value was NOT changed
    Product product = repository.findById(id).orElseThrow();
    assertThat(product.getName()).isEqualTo("Widget");
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Transient:  New POJO (No ID, not in L1 cache) -> em.persist()                         |
| Managed:    Tracked by L1 cache. Any mutation automatically writes to DB at commit!   |
| Detached:   L1 cache closed or em.detach() -> Must em.merge() to reattach             |
| Removed:    Scheduled for DELETE -> em.remove()                                       |
| Anti-Pattern: Calling repository.save(entity) on an already managed entity!          |
| Read-Only:  @Transactional(readOnly = true) disables dirty check snapshots for speed  |
| Batching:   em.flush() then em.clear() every N items to prevent OutOfMemoryError      |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Jakarta Persistence Specification (JPA 3.1 / 3.2)](https://jakarta.ee/specifications/persistence/)
- [Hibernate 6.6 User Guide: Persistence Context and Flushing](https://docs.jboss.org/hibernate/orm/6.6/userguide/html_single/Hibernate_User_Guide.html#pc)
- [Vlad Mihalcea: The High-Performance Java Persistence Guide](https://vladmihalcea.com/books/high-performance-java-persistence/)
- [Spring Data JPA Reference Documentation](https://docs.spring.io/spring-data/jpa/reference/)
