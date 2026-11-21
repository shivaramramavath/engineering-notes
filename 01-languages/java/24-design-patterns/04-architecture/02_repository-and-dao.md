# Repository and Data Access Object (DAO) Patterns in Modern Java

> Decoupling domain logic from persistence mechanisms: DAO vs. Repository, Spring Data interfaces, in-memory test repositories, and mapping SQL ResultSets to domain aggregates.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Architecture / Structural |
| **Prerequisites** | [16_jdbc-fundamentals](../../16-jdbc-and-databases/01_jdbc-fundamentals.md), [01_layered-architecture](01_layered-architecture.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

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
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What are DAO and Repository Patterns?
Both DAO (Data Access Object) and Repository are architectural patterns that isolate the domain and business layer from the details of underlying data access technologies (JDBC, JPA, Hibernate, MongoDB).

While frequently used interchangeably in informal conversations, they have distinct architectural intents:
- **DAO (Data Access Object)**: Table-centric. Provides direct CRUD mappings to database tables (`insert`, `update`, `delete`, `findByPrimaryKey`). Closer to the database schema.
- **Repository**: Domain-centric. Originating in Domain-Driven Design (DDD), a Repository mimics an in-memory collection of domain aggregates (`add()`, `remove()`, `findActiveCustomers()`). It speaks the language of the domain, completely concealing whether data is stored in SQL, NoSQL, or cache.

---

## 2. Core Concepts

### DAO vs. Repository Comparison
- **DAO**:
  - Focuses on database operations and SQL mapping.
  - 1-to-1 mapping with database tables (`CustomerDao` $\to$ `customers` table).
  - Exposes database concepts (cursors, pagination, sorting).
- **Repository**:
  - Focuses on Domain Aggregates (Eric Evans DDD).
  - Operates on Aggregate Roots (e.g. `OrderRepository` saves an `Order` and its constituent `OrderLine` items as a single atomic boundary).
  - Acts conceptually as an in-memory collection (`Set<Order>`).

---

## 3. How It Works Internally

### Architectural Placement
```
+--------------------------------------------------------------------------+
|                  REPOSITORY & DAO ARCHITECTURAL BOUNDARY                 |
|                                                                          |
|  [ Domain Service ]                                                      |
|         |                                                                |
|         | calls orderRepository.add(order)                               |
|         v                                                                |
|  [ OrderRepository (Interface) ] <--- Belongs to Domain Layer            |
|         ^                                                                |
|         | implements                                                     |
|  [ PostgresOrderRepository ]     <--- Belongs to Infrastructure Layer    |
|         |                                                                |
|         +--> Uses OrderDao / PreparedStatement / HikariDataSource        |
|         +--> Maps SQL ResultSets -> Rich Domain Aggregate                |
|         v                                                                |
|  [ PostgreSQL Database ]                                                 |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Domain Repository Interface
```java
import java.util.Optional;

public interface CustomerRepository {
    void add(Customer customer);
    Optional<Customer> findById(CustomerId id);
    void remove(Customer customer);
}
```

---

## 5. Practical Examples

### Example 1: Pure JDBC Repository Implementation with In-Memory Test Alternate
**Goal:** Implement a production JDBC repository alongside a 100% in-memory implementation for blazing-fast unit tests.

```java
import java.sql.*;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

// Domain Aggregate
public record CustomerId(long value) {}
public record Customer(CustomerId id, String email, String name) {}

// Repository Contract
public interface CustomerRepository {
    void save(Customer customer);
    Optional<Customer> findById(CustomerId id);
    List<Customer> findAll();
}

// Production Implementation 1: Pure JDBC
public class JdbcCustomerRepository implements CustomerRepository {
    private final javax.sql.DataSource dataSource;

    public JdbcCustomerRepository(javax.sql.DataSource ds) {
        this.dataSource = Objects.requireNonNull(ds);
    }

    @Override
    public void save(Customer customer) {
        String sql = "INSERT INTO customers (id, email, name) VALUES (?, ?, ?) "
                   + "ON CONFLICT (id) DO UPDATE SET email = EXCLUDED.email, name = EXCLUDED.name";
        try (Connection conn = dataSource.getConnection();
             PreparedStatement pstmt = conn.prepareStatement(sql)) {
            pstmt.setLong(1, customer.id().value());
            pstmt.setString(2, customer.email());
            pstmt.setString(3, customer.name());
            pstmt.executeUpdate();
        } catch (SQLException e) {
            throw new RuntimeException("Database error saving customer", e);
        }
    }

    @Override
    public Optional<Customer> findById(CustomerId id) {
        String sql = "SELECT id, email, name FROM customers WHERE id = ?";
        try (Connection conn = dataSource.getConnection();
             PreparedStatement pstmt = conn.prepareStatement(sql)) {
            pstmt.setLong(1, id.value());
            try (ResultSet rs = pstmt.executeQuery()) {
                if (rs.next()) {
                    return Optional.of(new Customer(
                        new CustomerId(rs.getLong("id")),
                        rs.getString("email"),
                        rs.getString("name")
                    ));
                }
                return Optional.empty();
            }
        } catch (SQLException e) {
            throw new RuntimeException("Database error finding customer", e);
        }
    }

    @Override
    public List<Customer> findAll() { return List.of(); }
}

// Test Implementation 2: In-Memory (Zero DB required for unit tests!)
public class InMemoryCustomerRepository implements CustomerRepository {
    private final Map<CustomerId, Customer> map = new ConcurrentHashMap<>();

    @Override public void save(Customer c) { map.put(c.id(), c); }
    @Override public Optional<Customer> findById(CustomerId id) { return Optional.ofNullable(map.get(id)); }
    @Override public List<Customer> findAll() { return List.copyOf(map.values()); }
}
```

---

## 6. Best Practices

1. **Keep Repository Interfaces in Domain Layer**: The repository interface belongs to the domain; the concrete implementation belongs to the infrastructure layer (DIP).
2. **Operate on Aggregate Roots**: Do not create a separate repository for every single database table. Only create repositories for Aggregate Roots (e.g. `Order`, not `OrderItem`).
3. **Always Return `Optional<T>` for Single Lookups**: Never return `null` when a find-by-id lookup misses.
4. **Use `List.copyOf()` for Multiple Returns**: Return immutable lists from query methods.
5. **Implement In-Memory Repositories for Unit Testing**: Having an in-memory map-backed repository enables comprehensive unit testing without database latency.
6. **Encapsulate SQL completely**: No SQL keywords or table names should leak out of repository methods.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Returning raw `ResultSet` to services** | Leaks open cursor, unclosed connection, and SQL coupling. | Map ResultSet to domain objects inside repository. |
| **Creating a repository for every table** | Violates Aggregate Root boundaries in DDD. | Only create repositories for Aggregate Roots. |
| **String concatenation in SQL queries** | Exposes repository to critical SQL injection. | Always use `PreparedStatement` bind parameters (`?`). |
| **Ignoring transactions during aggregate save** | Saving root succeeds but child line items fail, leaving database corrupted. | Execute multi-table updates in a single atomic transaction. |

---

## 8. Performance & Security Notes

### Zero Leaks via PreparedStatement Mapping
Repositories are the primary defense against SQL injection.
- 100% of data parameters must be bound using `pstmt.setString()`, `pstmt.setLong()`.
- Use cursor streaming or pagination (`LIMIT / OFFSET` or keyset pagination) to prevent OutOfMemoryErrors when querying large tables.

---

## 9. Comparison / When to Use What

| Pattern | Mental Model | Boundary | Primary Vocabulary |
| :--- | :--- | :--- | :--- |
| **DAO** | Database Table Wrapper | Table-level CRUD | `insert()`, `update()`, `delete()` |
| **Repository** | In-Memory Collection of Aggregates | Domain Aggregate Root | `add()`, `findById()`, `findActive()` |
| **Active Record** | Object holds data + its own SQL | Anti-pattern in DDD | `user.save()`, `user.delete()` |

---

## 10. Interview Questions & Answers

### Q1: What is the architectural difference between a DAO and a Repository?
<details>
<summary>View Answer</summary>
A DAO is data-centric: it directly abstracts a database table or storage system, offering low-level CRUD operations (`insert()`, `update()`, `delete()`). A Repository is domain-centric: originating in Domain-Driven Design (DDD), it manages an entire Aggregate Root and mimics an in-memory collection of domain objects (`add()`, `remove()`, `findSpec()`). A single Repository might use multiple DAOs internally to persist a complete aggregate graph across multiple database tables.
</details>

### Q2: Why should Repository methods return `Optional<T>` instead of `null`?
<details>
<summary>View Answer</summary>
When querying an entity by identifier (`findById`), the record may legitimately not exist. Returning `null` forces callers to remember explicit null checks; if forgotten, it leads to `NullPointerException`. Returning `java.util.Optional<T>` explicitly documents that the value might be absent and enables clean functional handling via `.map()`, `.orElseThrow()`, or `.ifPresent()`.
</details>

### Q3: What is an Aggregate Root in Domain-Driven Design and how does it relate to Repositories?
<details>
<summary>View Answer</summary>
An Aggregate is a cluster of associated domain objects that are treated as a unit for the purpose of data changes. The Aggregate Root is the single parent entity that guards access to the internal cluster (e.g. `Order` is the root; `OrderItem` and `DeliveryAddress` are internal children). In DDD, **Repositories are created only for Aggregate Roots**. External code never saves an `OrderItem` directly; it saves the `Order`, and the repository persists the entire boundary atomically.
</details>

### Q4: How does the Repository pattern facilitate unit testing?
<details>
<summary>View Answer</summary>
Because the business service depends on the `Repository` interface rather than a concrete database driver, developers can provide an `InMemoryRepository` backed by a `ConcurrentHashMap` during unit tests. This eliminates the need to spin up Docker containers or databases for unit tests, allowing entire test suites to execute in sub-seconds.
</details>

### Q5: What is the Active Record pattern and why is it generally discouraged in enterprise Java?
<details>
<summary>View Answer</summary>
Active Record (popularized by Ruby on Rails) combines database persistence methods directly on the entity itself (`user.save()`, `user.delete()`). In enterprise Java, Active Record is discouraged because it tightly couples the domain model to the database schema, breaks the Single Responsibility Principle, and makes mocking and testing difficult. The Repository pattern cleanly separates the domain model from persistence mechanics.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Generic CrudRepository Interface
**Task:** Define a generic repository interface for ID-based lookups and persistence.
<details>
<summary>View Solution</summary>

```java
import java.util.List;
import java.util.Optional;

public interface CrudRepository<T, ID> {
    void save(T entity);
    Optional<T> findById(ID id);
    List<T> findAll();
    void deleteById(ID id);
}
```
</details>

### Exercise 2: Map-Backed Generic Test Repository
**Task:** Implement `CrudRepository` using an in-memory map.
<details>
<summary>View Solution</summary>

```java
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Function;

public class InMemoryCrudRepository<T, ID> implements CrudRepository<T, ID> {
    private final Map<ID, T> storage = new ConcurrentHashMap<>();
    private final Function<T, ID> idExtractor;

    public InMemoryCrudRepository(Function<T, ID> idExtractor) {
        this.idExtractor = idExtractor;
    }

    @Override public void save(T entity) { storage.put(idExtractor.apply(entity), entity); }
    @Override public Optional<T> findById(ID id) { return Optional.ofNullable(storage.get(id)); }
    @Override public List<T> findAll() { return List.copyOf(storage.values()); }
    @Override public void deleteById(ID id) { storage.remove(id); }
}
```
</details>

### Exercise 3: ResultSet Column Extractor Helper
**Task:** Write a safe JDBC extractor method for nullable columns.
<details>
<summary>View Solution</summary>

```java
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.Optional;

public class JdbcExtractor {
    public static Optional<String> getOptionalString(ResultSet rs, String col) throws SQLException {
        String val = rs.getString(col);
        return rs.wasNull() ? Optional.empty() : Optional.of(val);
    }
}
```
</details>

---


### 11.4 Unit of Work Pattern and Specification Pattern with Repositories

In enterprise data access, the Repository pattern frequently pairs with two essential architectural companions:

#### 1. Unit of Work Pattern
The Unit of Work maintains a list of business objects affected by a business transaction and coordinates writing out changes and resolving concurrency problems:

```java
import java.util.*;

public interface UnitOfWork {
    void registerNew(Object entity);
    void registerDirty(Object entity);
    void registerRemoved(Object entity);
    void commit();
    void rollback();
}

public final class JpaUnitOfWork implements UnitOfWork {
    private final List<Object> newEntities = new ArrayList<>();
    private final List<Object> dirtyEntities = new ArrayList<>();
    private final List<Object> removedEntities = new ArrayList<>();

    @Override
    public void registerNew(Object entity) {
        Objects.requireNonNull(entity);
        if (!newEntities.contains(entity)) newEntities.add(entity);
    }

    @Override
    public void registerDirty(Object entity) {
        Objects.requireNonNull(entity);
        if (!dirtyEntities.contains(entity) && !newEntities.contains(entity)) {
            dirtyEntities.add(entity);
        }
    }

    @Override
    public void registerRemoved(Object entity) {
        Objects.requireNonNull(entity);
        if (newEntities.remove(entity)) return;
        dirtyEntities.remove(entity);
        if (!removedEntities.contains(entity)) removedEntities.add(entity);
    }

    @Override
    public void commit() {
        try {
            // Persist all new
            // Flush all dirty updates
            // Delete all removed
            System.out.println("Unit of Work committed: " + newEntities.size() + " new, " 
                + dirtyEntities.size() + " modified, " + removedEntities.size() + " deleted.");
            clear();
        } catch (Exception ex) {
            rollback();
            throw new RuntimeException("Commit failed", ex);
        }
    }

    @Override
    public void rollback() {
        clear();
        System.out.println("Unit of Work rolled back.");
    }

    private void clear() {
        newEntities.clear();
        dirtyEntities.clear();
        removedEntities.clear();
    }
}
```

#### 2. Specification Pattern for Dynamic Filtering
Instead of creating dozens of repository methods like `findByStatusAndRegionAndMinBalance(...)`, encapsulate query criteria into composable specifications:

```java
import java.util.function.Predicate;

public interface Specification<T> {
    boolean isSatisfiedBy(T candidate);

    default Specification<T> and(Specification<T> other) {
        return candidate -> this.isSatisfiedBy(candidate) && other.isSatisfiedBy(candidate);
    }

    default Specification<T> or(Specification<T> other) {
        return candidate -> this.isSatisfiedBy(candidate) || other.isSatisfiedBy(candidate);
    }

    default Specification<T> not() {
        return candidate -> !this.isSatisfiedBy(candidate);
    }
}

public final class AccountSpecs {
    public static Specification<Account> isActive() {
        return account -> "ACTIVE".equalsIgnoreCase(account.status());
    }

    public static Specification<Account> hasBalanceGreaterThan(double threshold) {
        return account -> account.balance() > threshold;
    }

    public static Specification<Account> inRegion(String region) {
        return account -> region.equalsIgnoreCase(account.region());
    }
}
```

## 12. Summary / Cheat Sheet

- **DAO**: Table-centric CRUD database access.
- **Repository**: Domain-centric aggregate root collection abstraction.
- **Domain Purity**: Repository interfaces reside in Domain; implementations in Infrastructure.
- **Optional**: Always return `Optional<T>` from single entity lookups.
- **Aggregate Roots Only**: Do not create repositories for dependent child tables.
- **Testing**: Implement in-memory repositories for millisecond unit testing.

---

## 13. Further Reading
- Eric Evans: [Domain-Driven Design: Tackling Complexity in the Heart of Software](https://www.oreilly.com/library/view/domain-driven-design-tackling/0321125215/)
- Martin Fowler: [Repository Pattern (Catalog of Enterprise Architecture Patterns)](https://martinfowler.com/eaaCatalog/repository.html)
- Spring Data: [Spring Data Repository Documentation](https://docs.spring.io/spring-data/commons/reference/repositories/core-concepts.html)
