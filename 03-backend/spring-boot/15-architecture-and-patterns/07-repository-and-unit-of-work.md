# Repository and Unit of Work Patterns in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | Spring Data JPA, Hibernate 6.x, PostgreSQL, Testcontainers |

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
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
When building data access logic in enterprise Java applications, developers frequently mix persistence mechanics (opening SQL connections, managing JDBC statement handles, issuing atomic commit/rollback calls) with business rules. Without proper abstraction, queries are duplicated across services, changes to database schemas break business logic, and coordinating multiple database writes into a single atomic transactional transaction becomes error-prone.

### The Evolution
To solve these concerns, Martin Fowler codified two complementary architectural patterns in *Patterns of Enterprise Application Architecture (PoEAA)*:
1. **The Repository Pattern**: Mediates between the domain and data mapping layers, acting like an in-memory collection of domain objects.
2. **The Unit of Work Pattern**: Maintains a list of objects affected by a business transaction and coordinates the writing out of changes and the resolution of concurrency problems.

In the modern Spring ecosystem, **Spring Data (`JpaRepository`, `CrudRepository`)** provides the Repository implementation, while **Hibernate's `Session` / JPA `EntityManager`** managed by Spring's `@Transactional` declarative boundary provides the Unit of Work.

### Production Relevance
Understanding the relationship between Repositories, Entity Managers, persistence contexts, and transaction boundaries is critical for preventing subtle data corruption, avoiding connection pool starvation, and eliminating unexpected SQL query cascades (such as the N+1 problem or unwanted dirty check flushes).

---

## 2. Core Concepts

### Architectural Topology: Repository & Unit of Work in Spring

```
+-----------------------------------------------------------------------------------------+
|                    REPOSITORY & UNIT OF WORK INTERACTION MODEL                          |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  [ Business Service Layer ] (@Transactional Boundary)                                  |
|            |                                                                            |
|            | 1. Starts Unit of Work (TransactionInterceptor)                            |
|            v                                                                            |
|  +-----------------------------------------------------------------------------------+  |
|  | UNIT OF WORK: JPA PersistenceContext (Hibernate Session)                          |  |
|  |   - Tracks dirty entities (Dirty Checking)                                        |  |
|  |   - First-Level Cache (Identity Map: ensures 1 object instance per DB row)         |  |
|  |   - Defers SQL generation until Flush time                                        |  |
|  +-----------------------------------------------------------------------------------+  |
|            ^                                                                            |
|            | Reads / Registers Entities                                                 |
|            |                                                                            |
|  +-----------------------------------------------------------------------------------+  |
|  | REPOSITORY LAYER: Spring Data JpaRepository                                       |  |
|  |   - Encapsulates queries and storage behind collection-like API                   |  |
|  |   - findById(), save(), delete(), findByEmail()                                   |  |
|  |   - Delegates directly to the underlying EntityManager                            |  |
|  +-----------------------------------------------------------------------------------+  |
|            |                                                                            |
|            | 2. Flushes SQL Statements at Commit Time                                   |
|            v                                                                            |
|  [ JDBC Driver & HikariCP Connection ] ---> [ PostgreSQL Database ]                     |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

### Key Architectural Definitions
1. **Repository**: An abstraction over a collection of domain objects. It encapsulates the retrieval and persistence logic, allowing the domain layer to treat database objects as if they were in an in-memory `List` or `Set`.
2. **Unit of Work**: An object that keeps track of everything you do during a business transaction that can affect the database. In Spring, the **JPA `EntityManager`** (backed by Hibernate's `Session`) acts as the Unit of Work.
3. **Identity Map (First-Level Cache)**: Ensures that each database row is loaded into exactly one Java object instance within the active `PersistenceContext`. This prevents conflicting updates and avoids duplicate queries for the same primary key.
4. **Dirty Checking**: At the end of the transaction, the Unit of Work automatically inspects tracked entities, detects mutated fields by comparing with initial snapshots, and issues minimal SQL `UPDATE` statements automatically without requiring an explicit `repository.save()`.

---

## 3. How It Works Internally

### The Transactional Persistence Context Lifecycle

```
[ Service Method Entry: @Transactional ]
       |
       v
1. TransactionInterceptor binds new EntityManager to ThreadLocal (TransactionSynchronizationManager)
       |
       v
2. repository.findById(101L)
       |
       +---> Checks First-Level Cache (Identity Map)
       |     - If present: returns cached instance immediately (0 SQL queries)
       |     - If absent: executes "SELECT ...", stores snapshot copy in PersistenceContext
       v
3. Business logic mutates entity:
       order.changeStatus(OrderStatus.CONFIRMED); // Memory-only change!
       // Notice: NO repository.save(order) called!
       |
       v
4. Service Method Exit
       |
       v
5. Transaction Commit Phase:
       - Flush Triggered: Hibernate runs Dirty Checking across all tracked entities.
       - Generates "UPDATE orders SET status = 'CONFIRMED' WHERE id = 101"
       - Sends batch SQL to JDBC connection
       - Issues physical "COMMIT" to PostgreSQL
       - Closes PersistenceContext and clears First-Level Cache
```

1. **Transactional Proxy**: When calling a `@Transactional` method, Spring's proxy obtains a database connection and associates a new `EntityManager` with the current thread.
2. **Entity Registration**: Any entity fetched via a Spring Data repository becomes **managed** within that `EntityManager`'s `PersistenceContext`.
3. **Automatic Change Detection**: Hibernate creates an in-memory snapshot of the entity's properties when first loaded. During the commit phase (`flush`), Hibernate compares current values with the snapshot. If any value changed, an `UPDATE` is automatically scheduled.
4. **Connection Release**: The transaction commits, the JDBC connection is returned to HikariCP, and the `PersistenceContext` is destroyed.

---

## 4. Syntax & Usage

### 1. Spring Data Repository Interface

```java
package com.example.repository;

import com.example.domain.Customer;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.util.Optional;
import java.util.UUID;

// Repository provides collection-like semantics
public interface CustomerRepository extends JpaRepository<Customer, UUID> {

    Optional<Customer> findByEmail(String email);

    @Query("SELECT c FROM Customer c JOIN FETCH c.orders WHERE c.id = :id")
    Optional<Customer> findByIdWithOrders(@Param("id") UUID id);
}
```

### 2. Leveraging the Unit of Work (Dirty Checking without `.save()`)

```java
package com.example.service;

import com.example.domain.Customer;
import com.example.repository.CustomerRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.UUID;

@Service
public class CustomerService {

    private final CustomerRepository customerRepository;

    public CustomerService(CustomerRepository customerRepository) {
        this.customerRepository = customerRepository;
    }

    @Transactional
    public void updateCustomerEmail(UUID customerId, String newEmail) {
        // 1. Entity loaded into PersistenceContext (Unit of Work)
        Customer customer = customerRepository.findById(customerId)
                .orElseThrow(() -> new IllegalArgumentException("Customer not found"));

        // 2. State mutated directly on domain object
        customer.updateEmail(newEmail);

        // 3. NOTICE: customerRepository.save(customer) is completely UNNECESSARY!
        // At transaction commit, Hibernate's Unit of Work automatically detects the change
        // and issues the SQL UPDATE.
    }
}
```

---

## 5. Practical Examples

### Example 1: Unit of Work Coordinating Multiple Repositories in a Single ACID Boundary

```java
package com.example.order;

import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;
import java.util.UUID;

@Service
public class CheckoutService {

    private final OrderRepository orderRepository;
    private final InventoryRepository inventoryRepository;
    private final WalletRepository walletRepository;

    public CheckoutService(OrderRepository orderRepository, 
                           InventoryRepository inventoryRepository, 
                           WalletRepository walletRepository) {
        this.orderRepository = orderRepository;
        this.inventoryRepository = inventoryRepository;
        this.walletRepository = walletRepository;
    }

    @Transactional // The Unit of Work spans all 3 repository operations atomically
    public UUID completeCheckout(UUID customerId, UUID productId, int count, BigDecimal totalCost) {
        // 1. Reserve inventory
        InventoryItem item = inventoryRepository.findByProductId(productId)
                .orElseThrow(() -> new OutOfStockException(productId));
        item.reserve(count); // Dirty checking will update inventory

        // 2. Deduct funds from wallet
        Wallet wallet = walletRepository.findByCustomerId(customerId)
                .orElseThrow(() -> new WalletNotFoundException(customerId));
        wallet.deduct(totalCost); // Dirty checking will update wallet balance

        // 3. Create new order
        Order order = new Order(customerId, productId, count, totalCost);
        Order savedOrder = orderRepository.save(order); // Insert statement queued

        // All 3 updates & 1 insert commit in a single atomic database transaction!
        return savedOrder.getId();
    }
}
```

### Example 2: DAO Pattern vs Repository Pattern (The Evolution)

#### Traditional Data Access Object (DAO) Pattern (Table-centric)
```java
// Traditional DAO: tightly coupled to individual database tables and CRUD SQL
public class OrderDaoImpl implements OrderDao {
    private final DataSource dataSource;
    public OrderDaoImpl(DataSource ds) { this.dataSource = ds; }

    @Override
    public void insert(OrderEntity entity) {
        // Raw JDBC SQL INSERT INTO orders ...
    }
    @Override
    public OrderEntity getById(long id) {
        // Raw JDBC SELECT * FROM orders WHERE id = ?
    }
}
```

#### Modern DDD Repository Pattern (Domain Collection-centric)
```java
// Repository: treats data as an in-memory collection of domain Aggregate Roots
public interface OrderRepository extends Repository<Order, OrderId> {
    void save(Order order);
    Optional<Order> findById(OrderId id);
    List<Order> findPendingOrdersCreatedBefore(Instant cutoff);
}
```

### Example 3: Explicit Flush Control via `EntityManager`

In high-volume batch processing, the Unit of Work memory graph can exhaust heap memory if not flushed periodically:

```java
package com.example.batch;

import jakarta.persistence.EntityManager;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Service
public class BatchCustomerProcessor {

    private final EntityManager entityManager;

    public BatchCustomerProcessor(EntityManager entityManager) {
        this.entityManager = entityManager;
    }

    @Transactional
    public void bulkSanitizeRecords(List<CustomerRecordDto> batch) {
        int batchSize = 50;
        for (int i = 0; i < batch.size(); i++) {
            CustomerEntity entity = new CustomerEntity(batch.get(i));
            entityManager.persist(entity);

            if (i > 0 && i % batchSize == 0) {
                // Flush sends accumulated SQL INSERTs to the DB socket
                entityManager.flush();
                // Clear releases managed entities from First-Level Cache to free JVM RAM!
                entityManager.clear();
            }
        }
    }
}
```

### Example 4: Anti-Pattern (Redundant `save()` and Premature Flush) vs Clean Unit of Work

#### The Anti-Pattern: Redundant `.save()` everywhere and Manual SQL
```java
// BAD: Treating Spring Data JPA as a procedural DAO with redundant save calls
@Service
public class BadUserService {
    @Autowired private UserRepository userRepo;

    @Transactional
    public void verifyEmail(Long userId) {
        User user = userRepo.findById(userId).orElseThrow();
        user.setEmailVerified(true);
        
        // REDUNDANT & HARMFUL: Hibernate already tracks this entity!
        // Calling save() forces unnecessary merge() semantics inside JPA!
        userRepo.save(user); 
    }
}
```

#### Production Fix: Clean Idiomatic Unit of Work
```java
// GOOD: Let the PersistenceContext handle mutation tracking naturally
@Service
public class CleanUserService {
    private final UserRepository userRepo;

    public CleanUserService(UserRepository userRepo) {
        this.userRepo = userRepo;
    }

    @Transactional
    public void verifyEmail(Long userId) {
        User user = userRepo.findById(userId)
                .orElseThrow(() -> new UserNotFoundException(userId));
        
        user.verifyEmail(); // Invariant handled by domain model
        // Transaction commit automatically triggers dirty check and executes UPDATE
    }
}
```

### Example 5: Read-Only Optimization for Unit of Work

Instructing Hibernate's Unit of Work to disable dirty checking snapshots for high-speed queries:

```java
package com.example.reporting;

import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Service
public class CatalogQueryService {

    private final ProductRepository productRepository;

    public CatalogQueryService(ProductRepository productRepository) {
        this.productRepository = productRepository;
    }

    // @Transactional(readOnly = true) optimizes the Unit of Work:
    // 1. Sets JDBC connection to read-only (allows DB to route to read replicas)
    // 2. Tells Hibernate to skip dirty check snapshot creation (saving 50% RAM and CPU!)
    // 3. Sets FlushMode.MANUAL so Hibernate never flushes
    @Transactional(readOnly = true)
    public List<ProductDto> searchActiveProducts(String category) {
        return productRepository.findByCategory(category)
                .stream()
                .map(ProductDto::fromEntity)
                .toList();
    }
}
```

---

## 6. Best Practices

1. **Rely on Dirty Checking**: Do not invoke `repository.save()` when updating existing managed entities inside a `@Transactional` method. Hibernate's Unit of Work manages updates automatically.
2. **Always Use `@Transactional(readOnly = true)` for Query Methods**: Disables memory-heavy entity dirty snapshot tracking and allows relational databases to optimize query execution and leverage read replicas.
3. **Keep Repositories Aggregate-Specific**: Never create repositories for child entities or value objects. Only Aggregate Roots deserve a Repository interface.
4. **Flush and Clear in Large Batch Operations**: Periodically call `entityManager.flush()` and `entityManager.clear()` to prevent the Unit of Work's First-Level Cache from consuming gigabytes of JVM heap memory.
5. **Never Expose `EntityManager` in Web Controllers**: Confine all persistence operations and `EntityManager` access strictly behind the `@Service` layer.
6. **Use Specific Query Projections for High-Volume Reads**: When data is only needed for display, use Spring Data Interface Projections or Record DTO projections (`SELECT new com.example.OrderView(...)`) to bypass the Unit of Work entity lifecycle entirely.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Calling `repository.save()` on Managed Entities** | Triggers unnecessary `EntityManager.merge()` overhead and confuses developers regarding JPA lifecycle. | Mutate entity properties directly; let Hibernate's dirty checking generate the `UPDATE`. |
| **Omitting `@Transactional` on Update Logic** | Without a transaction boundary, the `EntityManager` closes immediately after `findById()`; changes made to the entity are silently discarded! | Always annotate modifying service methods with `@Transactional`. |
| **Large Batches Without `entityManager.clear()`** | The First-Level Cache retains every managed entity in RAM until the transaction ends, causing `OutOfMemoryError`. | Flush and clear the `EntityManager` every 50–100 iterations. |
| **Confusing Repository with DAO** | Creating DAOs for every database table leads to anemic domain models and fragmented business logic. | Create Repositories strictly for Domain Aggregate Roots. |
| **Modifying Entities in `readOnly = true` Transactions** | Behavior is dialect-dependent; some drivers ignore updates while others log warnings or fail unexpectedly. | Never perform data mutation inside a `readOnly = true` method. |
| **Ignoring the Identity Map in Multi-Threaded Scenarios** | The JPA `EntityManager` is **not thread-safe**. Sharing an `EntityManager` across threads causes state corruption and race conditions. | Rely on Spring's thread-local `EntityManager` binding per transaction. |

---

## 8. Performance & Security Notes

### Performance
- **First-Level Cache Benefits**: The Identity Map prevents redundant SQL queries. If an application service fetches `customerRepository.findById(101L)` five times within the same `@Transactional` method, Hibernate executes exactly **one** SQL `SELECT` statement and returns the same in-memory object instance for the subsequent four calls.
- **Dirty Checking Overhead**: For huge transactions tracking 10,000 entities, Hibernate must inspect every field of all 10,000 entities at flush time. Use `@Transactional(readOnly = true)` or stateless sessions (`StatelessSession`) to eliminate this overhead.

### Security
- **SQL Injection Prevention**: Spring Data JPA repositories automatically parameterize all generated queries and derived finder methods (`findByEmail`), preventing SQL injection attacks out-of-the-box.
- **Tenant Scoping in Repositories**: When implementing multi-tenant data access, ensure repository finder methods automatically filter by tenant ID or enforce PostgreSQL Row-Level Security (RLS).

---

## 9. Comparison / When to Use What

| Dimension | Data Access Object (DAO) Pattern | Repository Pattern | Unit of Work Pattern |
|---|---|---|---|
| **Core Abstraction** | Database table / SQL rows | In-memory collection of Aggregates | Change tracking & transaction coordinator |
| **Coupling** | Tightly coupled to database schema | Coupled to Domain Model | Managed by ORM framework (Hibernate) |
| **Scope** | Typically 1 DAO per DB table | 1 Repository per Aggregate Root | 1 Unit of Work per HTTP Request / Transaction |
| **Change Tracking** | Manual `update()` calls required | Handled via Unit of Work | Automatic Dirty Checking at Commit |
| **Spring Implementation**| `JdbcTemplate` DAO classes | Spring Data `JpaRepository` | JPA `EntityManager` / Hibernate `Session` |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the exact difference between a DAO and a Repository?</strong></summary>

- **Data Access Object (DAO)**: A low-level, database-centric pattern. DAOs typically map 1:1 to database tables or views. They expose basic CRUD operations directly tied to SQL semantics (`insert`, `update`, `delete`). Business logic must explicitly coordinate DAOs and transactions.
- **Repository**: A high-level, domain-centric pattern from Domain-Driven Design (DDD). A Repository acts like an in-memory collection of Domain Aggregate Roots (`OrderRepository`). It hides storage mechanics completely. You only create repositories for Aggregate Roots, never for dependent child tables.
</details>

<details>
<summary><strong>2. How does Hibernate's Unit of Work implement "Dirty Checking"?</strong></summary>

When an entity is loaded from the database:
1. Hibernate creates the managed entity instance and gives it to the application.
2. Simultaneously, Hibernate stores an internal snapshot array containing the original field values inside the `PersistenceContext`.
3. When the transaction flushes (usually at method completion), Hibernate iterates through all managed entities and compares their current field values against the stored snapshot values.
4. If any field differs, Hibernate queues an optimized SQL `UPDATE` statement containing only the modified columns (or all columns, depending on `@DynamicUpdate`).
</details>

<details>
<summary><strong>3. Why is calling `repository.save()` on an entity that is already managed considered redundant in Spring Data JPA?</strong></summary>

When an entity is retrieved via `repository.findById()` inside a `@Transactional` method, it is already attached to the active `EntityManager` (`PersistenceContext`).
Calling `repository.save(entity)`:
- Checks `entityInformation.isNew(entity)`. Because it already has an ID, it assumes it is an existing entity.
- It delegates to `entityManager.merge(entity)`.
- `merge()` creates a copy of the state and returns the managed instance. But since the entity was *already* managed, this operation performs redundant checks.
Hibernate will perform dirty checking and issue the SQL `UPDATE` regardless of whether `save()` was called.
</details>

<details>
<summary><strong>4. What is the First-Level Cache (Identity Map) and what problem does it solve?</strong></summary>

The First-Level Cache is the in-memory cache associated with the JPA `EntityManager` (Unit of Work).
It serves two critical functions:
1. **Performance**: Avoids duplicate SQL roundtrips when the same entity is accessed multiple times within the same transaction.
2. **Identity Guarantee**: Ensures that if two queries fetch row ID `101`, both references point to the exact same Java object reference (`entity1 == entity2`). Without this, modifying `entity1` while another method reads `entity2` would result in conflicting updates and split-brain state.
</details>

<details>
<summary><strong>5. What does `@Transactional(readOnly = true)` do behind the scenes?</strong></summary>

1. **Spring Transaction Manager**: Calls `connection.setReadOnly(true)` on the JDBC connection, signaling to the database that no writes will occur (enabling routing to read replicas in AWS Aurora / PostgreSQL).
2. **Hibernate Session**: Sets the session's flush mode to `FlushMode.MANUAL`. Hibernate will never automatically flush dirty entities.
3. **Dirty Checking Optimization**: Hibernate disables snapshot creation for entities loaded during this transaction, reducing CPU utilization and memory allocations by up to 50%.
</details>

<details>
<summary><strong>6. What happens if you call `entityManager.clear()` inside a `@Transactional` method?</strong></summary>

`entityManager.clear()` completely detaches all currently managed entities from the `PersistenceContext`.
- Any subsequent modifications made to those entities will **NOT** be detected by dirty checking.
- No SQL `UPDATE` statements will be executed for them upon transaction commit.
- It is primarily used during large batch operations to free up JVM memory.
</details>

<details>
<summary><strong>7. How does the Unit of Work pattern handle optimistic locking conflicts?</strong></summary>

By utilizing a `@Version` field on the entity:
- When generating the SQL `UPDATE`, Hibernate appends the version to the WHERE clause:
  `UPDATE orders SET status = ?, version = 3 WHERE id = ? AND version = 2;`
- If another concurrent transaction already committed and updated the version to 3, the SQL update statement affects `0` rows.
- Hibernate detects that `rowCount == 0` and throws an `OptimisticLockException` (translated by Spring into `OptimisticLockingFailureException`), rolling back the entire Unit of Work.
</details>

<details>
<summary><strong>8. Can you implement the Unit of Work pattern without an ORM like Hibernate?</strong></summary>

Yes. In lightweight frameworks or pure JDBC/jOOQ applications:
- Spring's `TransactionTemplate` or `@Transactional` provides the physical database transaction boundary.
- A custom Unit of Work class maintains lists of pending operations: `List<Order> newOrders`, `List<Order> dirtyOrders`, `List<OrderId> deletedOrders`.
- At commit time, the Unit of Work iterates through the lists and executes batched JDBC `PreparedStatement` operations before calling `connection.commit()`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Demonstrate Dirty Checking in a Test Case</strong></summary>

**Objective**: Write a Spring Boot integration test proving that mutating an entity property updates the database without calling `repository.save()`.

**Solution**:
```java
@SpringBootTest
@Transactional
class DirtyCheckingIntegrationTest {

    @Autowired private CustomerRepository customerRepository;
    @Autowired private EntityManager entityManager;

    @Test
    void shouldUpdateEntityWithoutCallingSave() {
        Customer customer = customerRepository.save(new Customer("Alice", "alice@example.com"));
        UUID id = customer.getId();

        // Mutate property directly inside transaction
        customer.setName("Alice Updated");

        // Flush unit of work to database
        entityManager.flush();
        entityManager.clear(); // Clear 1st level cache to force DB read

        Customer reloaded = customerRepository.findById(id).orElseThrow();
        assertEquals("Alice Updated", reloaded.getName());
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Configure Read-Only Transaction on Search Service</strong></summary>

**Objective**: Annotate a search query service method to disable dirty checking snapshots.

**Solution**:
```java
@Service
public class ProductCatalogService {
    private final ProductRepository productRepo;

    public ProductCatalogService(ProductRepository productRepo) {
        this.productRepo = productRepo;
    }

    @Transactional(readOnly = true)
    public List<ProductDto> searchByName(String keyword) {
        return productRepo.findByNameContainingIgnoreCase(keyword)
                .stream()
                .map(ProductDto::from)
                .toList();
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement Batch Processing with Explicit Flush & Clear</strong></summary>

**Objective**: Write a service that inserts 10,000 log records in batches of 50 without running out of memory.

**Solution**:
```java
@Service
public class AuditLogBatchService {

    @PersistenceContext
    private EntityManager em;

    @Transactional
    public void insertBatchLogs(List<String> logMessages) {
        for (int i = 0; i < logMessages.size(); i++) {
            AuditLogEntity log = new AuditLogEntity(logMessages.get(i), Instant.now());
            em.persist(log);

            if (i > 0 && i % 50 == 0) {
                em.flush(); // Sends batch INSERTs to PostgreSQL
                em.clear(); // Discards managed entities from 1st-level cache
            }
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Repository Implementation using `EntityManager`</strong></summary>

**Objective**: Extend a Spring Data repository with a custom method using `EntityManager` directly.

**Solution**:
```java
// 1. Custom interface
public interface CustomOrderRepository {
    long bulkCancelStaleOrders(Instant cutoff);
}

// 2. Implementation matching naming convention <Interface>Impl
@Repository
public class CustomOrderRepositoryImpl implements CustomOrderRepository {

    @PersistenceContext
    private EntityManager em;

    @Override
    public long bulkCancelStaleOrders(Instant cutoff) {
        return em.createQuery("UPDATE Order o SET o.status = 'CANCELLED' WHERE o.createdAt < :cutoff AND o.status = 'PENDING'")
                .setParameter("cutoff", cutoff)
                .executeUpdate();
    }
}

// 3. Main repository extending both
public interface OrderRepository extends JpaRepository<Order, UUID>, CustomOrderRepository {}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Custom In-Memory Unit of Work for JDBC</strong></summary>

**Objective**: Implement a pure Java Unit of Work pattern coordinating JDBC batch statements without Hibernate.

**Solution**:
```java
public class JdbcUnitOfWork {
    private final Connection connection;
    private final List<User> registeredUsers = new ArrayList<>();
    private final List<User> modifiedUsers = new ArrayList<>();

    public JdbcUnitOfWork(Connection connection) { this.connection = connection; }

    public void registerNew(User user) { registeredUsers.add(user); }
    public void registerDirty(User user) { modifiedUsers.add(user); }

    public void commit() throws SQLException {
        try {
            connection.setAutoCommit(false);
            
            // Execute batch inserts
            try (PreparedStatement insertStmt = connection.prepareStatement("INSERT INTO users VALUES (?, ?)")) {
                for (User u : registeredUsers) {
                    insertStmt.setString(1, u.id());
                    insertStmt.setString(2, u.email());
                    insertStmt.addBatch();
                }
                insertStmt.executeBatch();
            }

            // Execute batch updates
            try (PreparedStatement updateStmt = connection.prepareStatement("UPDATE users SET email = ? WHERE id = ?")) {
                for (User u : modifiedUsers) {
                    updateStmt.setString(1, u.email());
                    updateStmt.setString(2, u.id());
                    updateStmt.addBatch();
                }
                updateStmt.executeBatch();
            }

            connection.commit();
        } catch (SQLException ex) {
            connection.rollback();
            throw ex;
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build an ArchUnit Guard for Repository Layer Hygiene</strong></summary>

**Objective**: Write an ArchUnit test asserting that:
1. Only Aggregate Root classes have corresponding Spring Data repositories.
2. Controllers never directly access `EntityManager` or Repositories.

**Solution**:
```java
@AnalyzeClasses(packages = "com.example")
public class RepositoryHygieneArchitectureTest {

    @ArchTest
    public static final ArchRule controllers_must_never_access_repositories_or_entity_manager =
        noClasses().that().resideInAPackage("..web..")
            .should().accessClassesThat().belongToAnyOf(JpaRepository.class, EntityManager.class);
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| REPOSITORY & UNIT OF WORK IN SPRING BOOT QUICK REFERENCE                                           |
+----------------------------------------------------------------------------------------------------+
| Concept             | Implementation Component                  | Key Characteristic               |
+---------------------+-------------------------------------------+----------------------------------+
| Repository          | Spring Data JpaRepository                 | Collection-like abstraction over |
|                     |                                           | domain Aggregate Roots           |
+---------------------+-------------------------------------------+----------------------------------+
| Unit of Work        | JPA EntityManager / Hibernate Session     | Tracks dirty entities, identity  |
|                     | bounded by @Transactional                 | map, coordinates atomic flush    |
+---------------------+-------------------------------------------+----------------------------------+
| Dirty Checking      | Hibernate snapshot comparison             | Automatically generates SQL      |
|                     |                                           | UPDATE without calling .save()   |
+---------------------+-------------------------------------------+----------------------------------+
| Identity Map        | First-Level Cache in PersistenceContext   | Ensures 1 object instance per ID |
+---------------------+-------------------------------------------+----------------------------------+
| Read-Only Tuning    | @Transactional(readOnly = true)           | Skips dirty check snapshots,     |
|                     |                                           | saves 50% CPU and memory         |
+---------------------+-------------------------------------------+----------------------------------+
| Batch Processing    | entityManager.flush() + clear()           | Prevents First-Level Cache from  |
|                     | every N iterations                        | overflowing JVM heap memory      |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Patterns of Enterprise Application Architecture (Martin Fowler)](https://martinfowler.com/books/eaa.html)
- [Spring Data JPA Reference Documentation](https://docs.spring.io/spring-data/jpa/reference/)
- [Vlad Mihalcea: High-Performance Java Persistence](https://vladmihalcea.com/books/high-performance-java-persistence/)
- [Hibernate ORM User Guide: Persistence Context and Flushes](https://docs.jboss.org/hibernate/orm/current/userguide/html_single/Hibernate_User_Guide.html)
- [Thorben Janssen: Why You Shouldn't Call save() on an Existing Entity](https://thorben-janssen.com/dont-call-save-on-existing-entities/)
