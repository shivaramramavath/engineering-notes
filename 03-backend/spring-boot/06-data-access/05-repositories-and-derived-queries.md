# Spring Data Repositories & Derived Queries: Contracts, Parser & Proxy Mechanics

| Metadata | Details |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | `03-spring-data-jpa-fundamentals.md`, `04-entity-mapping-and-relationships.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Spring Data JPA 3.4.x, Hibernate 6.6.x |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Comprehensive Derived Query Method Repository](#51-comprehensive-derived-query-method-repository)
   - [5.2 Limiting Query Results with First / Top Keywords](#52-limiting-query-results-with-first--top-keywords)
   - [5.3 Case-Insensitive and Complex Boolean Predicate Matching](#53-case-insensitive-and-complex-boolean-predicate-matching)
   - [5.4 Anti-Pattern: Unbounded Derived Method Names and Deep Traversal](#54-anti-pattern-unbounded-derived-method-names-and-deep-traversal)
   - [5.5 Streaming Large Datasets via Java 8+ Stream Return Types](#55-streaming-large-datasets-via-java-8-stream-return-types)
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
In traditional data access patterns, developers write repetitive boilerplate classes implementing CRUD methods (`save`, `delete`, `findById`) and hand-craft SQL strings for simple filtering (`SELECT * FROM users WHERE email = ?`). Writing and maintaining these repetitive DAO implementations for dozens of entities consumes substantial engineering time, invites copy-paste errors, and obscures application domain logic.

### Historical Evolution
Spring Data pioneered the concept of declarative repository interfaces. Instead of writing concrete DAO classes, engineers declare an interface extending `JpaRepository` or `ListCrudRepository`. At application bootstrap, Spring Data inspects interface method signatures, parses domain-specific keywords (`findByEmail`, `findTop5ByStatusOrderByCreatedAtDesc`), and generates high-performance proxy implementations using dynamic proxies and Hibernate's query engine. In Spring Data 3.x, `ListCrudRepository` was introduced to return `List<T>` directly, removing the awkward `Iterable<T>` cast from legacy `CrudRepository`.

### Real-World Relevance
Derived queries allow engineers to implement 80% of common business query requirements in seconds with zero boilerplate and compile-time validation. When paired with modern return types like `Optional<T>` and `Stream<T>`, Spring Data repositories provide an expressive, type-safe data access layer.

---

## 2. Core Concepts

### Repository Interface Hierarchy
```
+------------------------------------------------------------------------------------+
|                         Spring Data Repository Hierarchy                           |
+------------------------------------------------------------------------------------+
|                                                                                    |
|                                Repository<T, ID>                                   |
|                                       ^                                            |
|                                       |                                            |
|                               CrudRepository<T, ID>                                |
|                               /                   \                                |
|                              /                     \                               |
|        ListCrudRepository<T, ID>         PagingAndSortingRepository<T, ID>         |
|               (Returns List<T>)                            ^                       |
|                             \                             /                        |
|                              \                           /                         |
|                       ListPagingAndSortingRepository<T, ID>                        |
|                                       ^                                            |
|                                       |                                            |
|                              JpaRepository<T, ID>                                  |
|          (Adds flush, batch operations, pagination, and JPA specifications)         |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

### Derived Query Anatomy
Derived queries follow a strict syntactic grammar:
$$\text{[Prefix]} + \text{[Subject/Limiter]} + \text{By} + \text{[Predicate Conditions]}$$
- **Prefix**: `find`, `read`, `query`, `count`, `exists`, `delete`, `remove`.
- **Limiter**: `First`, `Top`, `Distinct`, `First5`, `Top10`.
- **Criteria**: Entity property names combined with keywords (`And`, `Or`, `Between`, `LessThan`, `GreaterThan`, `Like`, `StartingWith`, `EndingWith`, `Containing`, `IgnoreCase`, `True`, `False`, `In`, `NotIn`).
- **Ordering**: `OrderBy` followed by the property name and direction (`Asc` / `Desc`).

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                       Spring Data Proxy Generation Mechanics                       |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  1. Spring Container Bootstrapping:                                                |
|     - JpaRepositoriesRegistrar scans @EnableJpaRepositories packages               |
|     - Identifies UserRepository interface extending JpaRepository                  |
|                                                                                    |
|  2. Bean Creation via JpaRepositoryFactoryBean:                                    |
|     - Invokes ProxyFactory with JdkDynamicAopProxy                                 |
|     - Registers SimpleJpaRepository as the default target class                    |
|     - Injects QueryExecutorMethodInterceptor as the advice                         |
|                                                                                    |
|  3. Query Method Analysis:                                                         |
|     - PartTree parser analyzes method name "findByEmailAndActiveTrue"              |
|     - Validates tokens against JPA Entity Metamodel (User.email, User.active)      |
|     - If invalid property: FAILS AT STARTUP with PropertyReferenceException!       |
|     - Compiles tokens into an AST and generates an executable JPQL query string    |
|                                                                                    |
|  4. Runtime Execution:                                                             |
|     - Caller invokes userRepository.findByEmail("alice@test.com")                  |
|     - Proxy intercepts call, binds parameters, and delegates to EntityManager      |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Basic Repository Definition
```java
public interface UserRepository extends JpaRepository<User, Long> {

    // Derived Query returning Optional
    Optional<User> findByEmail(String email);

    // Boolean derived query
    boolean existsByUsername(String username);

    // Count query
    long countByStatus(UserStatus status);

    // Derived deletion query
    void deleteByStatusAndCreatedAtBefore(UserStatus status, Instant cutoff);
}
```

---

## 5. Practical Examples

### 5.1 Comprehensive Derived Query Method Repository
```java
package com.example.jpa.repository;

import com.example.jpa.domain.Customer;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.math.BigDecimal;
import java.time.LocalDate;
import java.util.Collection;
import java.util.List;
import java.util.Optional;

@Repository
public interface CustomerRepository extends JpaRepository<Customer, Long> {

    // Single item lookups
    Optional<Customer> findByEmailIgnoreCase(String email);

    // Multiple criteria with AND / OR
    List<Customer> findByLastNameAndActiveTrue(String lastName);

    List<Customer> findByCountryOrVipStatusTrue(String country);

    // Comparison operators
    List<Customer> findByCreditLimitGreaterThanEqual(BigDecimal minCredit);

    List<Customer> findByRegistrationDateBetween(LocalDate start, LocalDate end);

    // String matching
    List<Customer> findByLastNameStartingWith(String prefix);

    List<Customer> findByEmailContaining(String keyword);

    // Collection membership
    List<Customer> findByStatusIn(Collection<String> statuses);

    // Nullability checks
    List<Customer> findByPhoneNumberIsNull();
}
```

### 5.2 Limiting Query Results with First / Top Keywords
```java
package com.example.jpa.repository;

import com.example.jpa.domain.Order;
import org.springframework.data.domain.Sort;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;
import java.util.Optional;

public interface OrderRepository extends JpaRepository<Order, Long> {

    // Fetch the single most recent order for a customer
    Optional<Order> findFirstByCustomerIdOrderByOrderDateDesc(Long customerId);

    // Fetch the top 5 highest-value completed orders
    List<Order> findTop5ByStatusOrderByTotalAmountDesc(String status);

    // Dynamic sorting applied to Top-N derived query
    List<Order> findTop10ByStatus(String status, Sort sort);
}
```

### 5.3 Case-Insensitive and Complex Boolean Predicate Matching
```java
package com.example.jpa.repository;

import com.example.jpa.domain.Article;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;

public interface ArticleRepository extends JpaRepository<Article, Long> {

    // All case-insensitive: WHERE UPPER(title) = UPPER(?1) AND published = true
    List<Article> findByTitleIgnoreCaseAndPublishedTrue(String title);

    // Partial case-insensitive: WHERE UPPER(title) LIKE UPPER(?1) AND views > ?2
    List<Article> findByTitleContainingIgnoreCaseAndViewCountGreaterThan(
        String keyword, 
        long minViews
    );
}
```

### 5.4 Anti-Pattern: Unbounded Derived Method Names and Deep Traversal
```java
// =========================================================================
// ANTI-PATTERN: Monster Derived Method Name (Unreadable & Brittle)
// =========================================================================
public interface BadUserRepository extends JpaRepository<User, Long> {

    // UNMAINTAINABLE CODE:
    // A 120-character method name traversing 3 nested relationships!
    // Triggers multiple hidden SQL joins and breaks silently during entity refactoring.
    List<User> findByDepartmentCompanyHeadquartersAddressCityAndDepartmentActiveTrueAndRolesNameInOrderByProfileLastNameAsc(
        String city, 
        Collection<String> roleNames
    );
}

// =========================================================================
// PRODUCTION FIX: Explicit JPQL / Native Query with @Query
// =========================================================================
public interface GoodUserRepository extends JpaRepository<User, Long> {

    // CLEAN AND MAINTAINABLE:
    // Explicit, readable JPQL with named binds and optimal JOIN FETCH
    @org.springframework.data.jpa.repository.Query("""
        SELECT u FROM User u
        JOIN u.department d
        JOIN d.company c
        JOIN u.roles r
        WHERE c.headquartersAddress.city = :city
          AND d.active = true
          AND r.name IN :roleNames
        ORDER BY u.profile.lastName ASC
        """)
    List<User> findUsersInCityWithRoles(
        @org.springframework.data.repository.query.Param("city") String city,
        @org.springframework.data.repository.query.Param("roleNames") Collection<String> roleNames
    );
}
```

### 5.5 Streaming Large Datasets via Java 8+ Stream Return Types
```java
package com.example.jpa.service;

import com.example.jpa.domain.Transaction;
import com.example.jpa.repository.TransactionRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.io.Writer;
import java.util.stream.Stream;

@Service
public class TransactionExportService {

    private final TransactionRepository repository;

    public TransactionExportService(TransactionRepository repository) {
        this.repository = repository;
    }

    /**
     * Consumes thousands of records via a database cursor without loading
     * the entire result set into the JVM heap.
     * CRITICAL: Must be wrapped in @Transactional(readOnly = true) to hold the connection!
     */
    @Transactional(readOnly = true)
    public void exportTransactions(Writer writer) {
        try (Stream<Transaction> stream = repository.findByStatus("SETTLED")) {
            stream.forEach(tx -> {
                try {
                    writer.write(tx.toCsvRow() + "\n");
                } catch (Exception e) {
                    throw new RuntimeException("Export failed", e);
                }
            });
        }
    }
}
```

---

## 6. Best Practices

1. **Use `ListCrudRepository` for List Returns**: When you do not need full JPA features like manual flushing or Specification support, prefer `ListCrudRepository` to return `List<T>` instead of `Iterable<T>`.
2. **Cap Derived Query Complexity at 2–3 Conditions**: If a derived query method name exceeds 3 conditions or traverses nested objects, switch to `@Query` with JPQL for readability.
3. **Always Use `Optional<T>` for Nullable Single-Item Lookups**: Never declare single-entity return types as `T findByEmail(String email)`. Returning `Optional<T>` forces callers to handle absence explicitly.
4. **Leverage Compile-Time Startup Verification**: Spring Data parses and validates all derived query method names during application context bootstrap. Treat startup validation failures as test gate failures.
5. **Always Close Result Streams Inside `try-with-resources`**: Java `Stream<T>` backed by JPA retains an open `ResultSet`. Always manage streams within a `try-with-resources` block inside an active transaction.
6. **Prefer Derived `existsBy` Over `countBy > 0`**: `existsBy` generates an optimized `SELECT 1 FROM table WHERE ... LIMIT 1`, whereas `countBy` can trigger a full table index scan to count all matching rows.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `findBy` for Large Unpaged Datasets** | Loading 500,000 entities into a `List<T>` exhausts the heap and crashes the application with `OutOfMemoryError`. | Use pagination (`Pageable`), keyset pagination, or cursor streaming (`Stream<T>`). |
| **Returning Raw `T` Instead of `Optional<T>`** | If the row does not exist, `null` is returned, leading to `NullPointerException`s downstream. | Use `Optional<T>` for all unique lookups. |
| **Consuming `Stream<T>` Outside a Transaction** | Closing the connection closes the underlying JDBC `ResultSet`, throwing `TransactionSystemException` during iteration. | Always annotate caller service methods with `@Transactional(readOnly = true)`. |
| **Deep Property Traversal (e.g. `findByUser_Profile_Address_City`)** | Generates hidden, unoptimized SQL cross-joins that bypass query indexes. | Write explicit JPQL queries with explicit inner or left joins via `@Query`. |
| **Using `countBy` to Check Existence** | Counts all matching rows in the table instead of stopping at the first match. | Use `existsBy...` which issues a fast `EXISTS (SELECT 1 ...)` query. |
| **Modifying Derived Method Return Types Incompatibly** | Declaring a method returning `User` when multiple matching rows exist throws `IncorrectResultSizeDataAccessException`. | Return `List<User>` or refine criteria to guarantee uniqueness. |

---

## 8. Performance & Security Notes

### Performance
- **Startup Time Validation**: Spring Data parses all repository methods at boot time. While this incurs a small startup cost ($100\text{--}500\text{ ms}$), it eliminates runtime query compilation overhead.
- **Top / First Query Optimization**: Specifying `findFirst10By` translates directly to the database dialect's `LIMIT 10` (or `FETCH FIRST 10 ROWS ONLY`), preventing excessive network data transfer.

### Security
- **No SQL Injection Surface**: Derived queries are completely immune to SQL injection. The method parser binds all method arguments strictly as JDBC PreparedStatement parameters.
- **Data Leak Prevention via Limiting**: Using `findTop100By` guards against accidental denial-of-service from runaway queries that return millions of rows.

---

## 9. Comparison / When to Use What

| Query Technique | Complexity Supported | Type Safety | Refactoring Resistance | Performance |
| :--- | :--- | :--- | :--- | :--- |
| **Derived Queries** | Low (1–3 conditions) | **High (Verified at boot)** | High (Fails fast if property changes) | Excellent |
| **JPQL (`@Query`)** | Medium–High | High (Verified at boot) | Moderate (Requires query string audit) | Excellent |
| **Native SQL (`@Query`)** | Very High (DB-specific) | Low (No verification) | Low (Breaks silently on schema changes) | Maximum |
| **Specifications (Criteria)** | Dynamic Multi-filter | High (Type-safe Metamodel) | High | Good |

---

## 10. Interview Questions

<details>
<summary><strong>1. How does Spring Data generate repository implementations without concrete classes?</strong></summary>

Spring Data uses the Spring AOP `ProxyFactory` and Java's `JdkDynamicAopProxy`. At context startup, it scans repository interfaces, creates a dynamic proxy implementing the interface, registers `SimpleJpaRepository` as the default target for CRUD methods, and registers `QueryExecutorMethodInterceptor` to route custom and derived query methods to compiled JPA query executions.
</details>

<details>
<summary><strong>2. What happens if a derived query method contains a typo in a property name?</strong></summary>

Spring Data parses method names against the JPA Entity Metamodel during application context initialization. If a property cannot be resolved (e.g., `findByEmal` instead of `findByEmail`), Spring throws a `PropertyReferenceException` and aborts application startup, guaranteeing fail-fast behavior.
</details>

<details>
<summary><strong>3. What is the difference between <code>CrudRepository</code> and <code>ListCrudRepository</code> in Spring Data 3+?</strong></summary>

`CrudRepository` returns `Iterable<T>` for bulk operations like `findAll()`, requiring manual type-casting to `List<T>`. Spring Data 3.0 introduced `ListCrudRepository`, which overrides these methods to return `List<T>` directly, eliminating casting boilerplate while maintaining contract clarity.
</details>

<details>
<summary><strong>4. Why is <code>existsBy...</code> more efficient than <code>countBy... > 0</code>?</strong></summary>

`existsBy` generates SQL using the `EXISTS (SELECT 1 FROM table WHERE ...)` construct (or `LIMIT 1`), which causes the database engine to halt scanning immediately upon finding the first matching record. `countBy` scans all matching rows to calculate the exact count, consuming significantly more I/O and CPU on large tables.
</details>

<details>
<summary><strong>5. How does Spring Data handle <code>deleteBy...</code> derived methods?</strong></summary>

By default, derived `deleteBy` methods in `JpaRepository` load matching entities into the persistence context and invoke `entityManager.remove()` on each instance one by one to ensure JPA entity lifecycle callbacks (`@PreRemove`, `@PostRemove`) and cascades are honored. For high-performance single-statement bulk deletion, an explicit `@Modifying @Query("DELETE FROM ...")` is required.
</details>

<details>
<summary><strong>6. Can derived query methods accept sorting and pagination parameters dynamically?</strong></summary>

Yes. Any derived query method can accept `org.springframework.data.domain.Pageable` or `org.springframework.data.domain.Sort` as its final argument (e.g., `findByStatus(String status, Pageable pageable)`), allowing dynamic sorting and offset/limit pagination without hardcoding orders in the method name.
</details>

<details>
<summary><strong>7. What is the prerequisite for returning a Java <code>Stream<T></code> from a repository method?</strong></summary>

The calling service method must be annotated with `@Transactional(readOnly = true)`. Because a stream lazily fetches records from the database cursor during iteration, closing the transaction prematurely terminates the database connection, rendering the stream unconsumable.
</details>

<details>
<summary><strong>8. What is the difference between <code>findFirstBy</code> and <code>findTopBy</code>?</strong></summary>

In Spring Data, `First` and `Top` are completely synonymous keywords. Both instruct the query generator to append a `LIMIT 1` clause to the resulting SQL statement. Specifying a number (e.g., `findTop5By` or `findFirst5By`) sets the limit to that specific count.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Derived Lookup and Existence</strong></summary>

**Task**: Declare repository methods for `Product` to find by `sku` returning `Optional<Product>` and check if a product exists by `sku`.

```java
public interface ProductRepository extends JpaRepository<Product, Long> {
    Optional<Product> findBySku(String sku);
    boolean existsBySku(String sku);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Date Range and Status Filtering</strong></summary>

**Task**: Write a derived query method to find all orders with status `"PENDING"` created after a specified `Instant`.

```java
public interface OrderRepository extends JpaRepository<Order, Long> {
    List<Order> findByStatusAndCreatedAtAfter(String status, Instant timestamp);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Case-Insensitive Prefix Search with Ordering</strong></summary>

**Task**: Implement a repository query to find users whose last name starts with a given prefix (case-insensitive), ordered by first name ascending.

```java
public interface UserRepository extends JpaRepository<User, Long> {
    List<User> findByLastNameStartingWithIgnoreCaseOrderByFirstNameAsc(String prefix);
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Dynamic Limiting with Sort</strong></summary>

**Task**: Declare a derived query to fetch the top 3 highest-rated reviews for a product by passing a dynamic `Sort` parameter.

```java
public interface ReviewRepository extends JpaRepository<Review, Long> {
    List<Review> findTop3ByProductId(Long productId, Sort sort);
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Streaming Cursor Consumer with Read-Only Transaction</strong></summary>

**Task**: Write a service method that queries an audit log stream via `Stream<AuditLog>` and aggregates an in-memory security report within a `@Transactional(readOnly = true)` boundary.

```java
@Transactional(readOnly = true)
public long countSuspiciousEvents(String ipAddress) {
    try (Stream<AuditLog> stream = repository.findByIpAddress(ipAddress)) {
        return stream.filter(AuditLog::isFailure).count();
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Repository Extension Interface</strong></summary>

**Task**: Extend `JpaRepository` with a custom interface `CustomUserRepository` implemented by `UserRepositoryImpl` that executes dynamic query filtering via `EntityManager`.

```java
public interface CustomUserRepository {
    List<User> searchByCriteria(String name, String role);
}

@Repository
public class UserRepositoryImpl implements CustomUserRepository {
    @PersistenceContext
    private EntityManager em;

    @Override
    public List<User> searchByCriteria(String name, String role) {
        return em.createQuery("SELECT u FROM User u WHERE u.name = :name", User.class)
            .setParameter("name", name)
            .getResultList();
    }
}

public interface UserRepository extends JpaRepository<User, Long>, CustomUserRepository {}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Repository:   Use ListCrudRepository<T, ID> or JpaRepository<T, ID>                   |
| Grammar:      find[TopN]By[Property][Operator]And[Property][Operator]                 |
| Operators:    IgnoreCase, Between, LessThan, GreaterThan, StartingWith, Containing, In|
| Nulls:        IsNull, IsNotNull, True, False                                          |
| Existence:    existsBy[Property] (Much faster than countBy > 0)                       |
| Limiting:     findFirstBy, findTop5By, findDistinctBy                                 |
| Streams:      Stream<T> must run inside @Transactional(readOnly = true)               |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Data JPA Query Methods Reference](https://docs.spring.io/spring-data/jpa/reference/jpa/query-methods.html)
- [Spring Data Repository Interface Hierarchy (Spring Blog)](https://spring.io/blog/2022/02/22/announcing-spring-data-2022-0-0-m2)
- [Project Reactor & Spring Data Stream Repositories](https://docs.spring.io/spring-data/commons/reference/repositories/query-methods-details.html#repositories.query-streaming)
- [Hibernate 6.6 Semantic Query Model (SQM) Overview](https://in.relation.to/2022/04/04/hibernate-orm-60-final/)
