# JPQL, Native Queries & Specifications: Declarative & Dynamic Querying

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate / Advanced |
| **Prerequisites** | `03-spring-data-jpa-fundamentals.md`, `05-repositories-and-derived-queries.md`, SQL Joins & Aggregations |
| **Estimated Reading Time** | 32 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | PostgreSQL 16, Hibernate 6.6.x, Spring Data JPA 3.4.x |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Expressive JPQL with JOIN FETCH and Named Parameters](#51-expressive-jpql-with-join-fetch-and-named-parameters)
   - [5.2 Native PostgreSQL Query with Common Table Expressions (CTEs) and JSONB](#52-native-postgresql-query-with-common-table-expressions-ctes-and-jsonb)
   - [5.3 Bulk Updates with @Modifying and Persistence Context Synchronization](#53-bulk-updates-with-modifying-and-persistence-context-synchronization)
   - [5.4 Anti-Pattern: Native Queries Without Parameter Binding](#54-anti-pattern-native-queries-without-parameter-binding)
   - [5.5 Production Dynamic Filtering with Specification and CriteriaBuilder](#55-production-dynamic-filtering-with-specification-and-criteriabuilder)
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
While Spring Data's derived query methods (`findBy...`) handle simple lookups cleanly, they become unreadable and brittle when queries require multi-table joins, subqueries, group aggregations, database-specific capabilities (e.g., PostgreSQL JSONB operators, Common Table Expressions, window functions), or dynamic multi-attribute search screens where filters can be applied in arbitrary combinations.

### Historical Evolution
JPA introduced the Java Persistence Query Language (JPQL)—an object-oriented dialect operating over entity classes and properties rather than physical tables and columns. For dynamic query assembly, JPA standardized the Criteria API. Spring Data JPA introduced `Specification<T>` (wrapping Criteria API into composable functional interfaces) and the `@Query` annotation. In Spring Boot 3.4+ / Hibernate 6.6+, both JPQL and Criteria queries are compiled via the unified Semantic Query Model (SQM) directly into optimal database-specific SQL.

### Real-World Relevance
Production enterprise applications require both type-safe entity querying and high-performance database execution. Using JPQL prevents coupling to specific SQL dialects, while native queries unlock vendor-specific analytics and bulk performance. Composable `Specification` predicates power modern multi-filter e-commerce product catalogs and administrative search portals.

---

## 2. Core Concepts

### JPQL (Java Persistence Query Language)
A declarative query language operating on entity classes, attributes, and relationships. It is database-agnostic; Hibernate translates JPQL into dialect-specific SQL (e.g., handling pagination differences across Oracle, PostgreSQL, and SQL Server automatically).

### Native SQL Queries
Configured via `@Query(value = "...", nativeQuery = true)`. Bypasses JPQL translation to execute raw SQL directly on the database engine. Essential for utilizing vendor-specific features like `JSON_EXTRACT`, recursive CTEs (`WITH RECURSIVE`), full-text search, and window functions (`ROW_NUMBER() OVER (...)`).

### @Modifying Queries
JPA `@Query` methods default to `SELECT` operations. To execute `UPDATE` or `DELETE` statements, methods must be annotated with `@Modifying`. Attributes `clearAutomatically = true` and `flushAutomatically = true` control persistence context synchronization.

### Specification<T>
A functional interface from Spring Data JPA based on the JPA Criteria API:
```java
@FunctionalInterface
public interface Specification<T> extends Serializable {
    Predicate toPredicate(Root<T> root, CriteriaQuery<?> query, CriteriaBuilder criteriaBuilder);
}
```
Specifications can be composed dynamically using `and()`, `or()`, and `where()`.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         Query Execution Internals in JPA 3+                        |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Caller Invocation: userRepository.findActiveInCity("NYC")                         |
|         |                                                                          |
|         v                                                                          |
|  JpaQueryExecution / QueryExecutorMethodInterceptor                                |
|         |                                                                          |
|         +---> Is nativeQuery = true?                                               |
|         |     |                                                                    |
|         |     +-- YES --> Prepares native SQL directly via Connection              |
|         |                 Maps ResultSet using NativeQueryImplementor              |
|         |                                                                          |
|         +-- NO (JPQL / Specification Criteria)                                     |
|               |                                                                    |
|               v                                                                    |
|         Hibernate 6.6 Semantic Query Model (SQM)                                   |
|         - Parses JPQL / Criteria into Abstract Syntax Tree (SQM Tree)              |
|         - Type-checks entity properties against Entity Metamodel                   |
|         - Dialect SQL Translator generates optimized target SQL:                   |
|           SELECT u.id, u.email FROM users u WHERE u.city = ? AND u.status = 'ACTIVE'|
|                                                                                    |
|         Action Execution:                                                          |
|         - Binds @Param arguments to PreparedStatement placeholders                 |
|         - Executes query, populates L1 Cache, and returns mapped entities/DTOs     |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Basic JPQL Query
```java
public interface UserRepository extends JpaRepository<User, Long> {

    @Query("SELECT u FROM User u WHERE u.status = :status AND u.email LIKE %:domain%")
    List<User> findUsersByStatusAndDomain(
        @Param("status") String status, 
        @Param("domain") String domain
    );
}
```

### Basic Native Query
```java
public interface UserRepository extends JpaRepository<User, Long> {

    @Query(value = "SELECT * FROM users WHERE metadata->>'tier' = :tier", nativeQuery = true)
    List<User> findByMetadataTier(@Param("tier") String tier);
}
```

---

## 5. Practical Examples

### 5.1 Expressive JPQL with JOIN FETCH and Named Parameters
```java
package com.example.jpa.repository;

import com.example.jpa.domain.Order;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.List;
import java.util.Optional;

public interface OrderRepository extends JpaRepository<Order, Long> {

    /**
     * Resolves N+1 query problem by fetching order items and customer in a single query.
     */
    @Query("""
        SELECT DISTINCT o FROM Order o
        JOIN FETCH o.items i
        JOIN FETCH o.customer c
        WHERE o.totalAmount >= :minTotal
          AND o.orderDate BETWEEN :startDate AND :endDate
        ORDER BY o.orderDate DESC
        """)
    List<Order> findHighValueOrdersWithDetails(
        @Param("minTotal") BigDecimal minTotal,
        @Param("startDate") Instant startDate,
        @Param("endDate") Instant endDate
    );

    @Query("SELECT o FROM Order o WHERE o.id = :id AND o.customer.id = :customerId")
    Optional<Order> findByIdAndCustomerId(
        @Param("id") Long id, 
        @Param("customerId") Long customerId
    );
}
```

### 5.2 Native PostgreSQL Query with Common Table Expressions (CTEs) and JSONB
```java
package com.example.jpa.repository;

import com.example.jpa.domain.AnalyticsEvent;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.util.List;

public interface AnalyticsEventRepository extends JpaRepository<AnalyticsEvent, Long> {

    public record DailyEventSummary(String eventType, long eventCount, double avgDurationMs) {}

    /**
     * Native query using PostgreSQL CTE (WITH clause) and aggregate window calculations.
     */
    @Query(value = """
        WITH RecentEvents AS (
            SELECT 
                event_type,
                (payload->>'duration')::numeric AS duration_ms
            FROM analytics_events
            WHERE created_at >= NOW() - INTERVAL '7 DAYS'
              AND payload->>'tenant_id' = :tenantId
        )
        SELECT 
            event_type AS eventType,
            COUNT(*) AS eventCount,
            ROUND(AVG(duration_ms), 2) AS avgDurationMs
        FROM RecentEvents
        GROUP BY event_type
        ORDER BY eventCount DESC
        """, nativeQuery = true)
    List<DailyEventSummary> summarizeWeeklyEventsByTenant(@Param("tenantId") String tenantId);
}
```

### 5.3 Bulk Updates with @Modifying and Persistence Context Synchronization
```java
package com.example.jpa.repository;

import com.example.jpa.domain.Subscription;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Modifying;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;

public interface SubscriptionRepository extends JpaRepository<Subscription, Long> {

    /**
     * Bulk update: Modifies thousands of records in a single SQL statement.
     * CRITICAL: clearAutomatically = true evicts stale entities from First-Level Cache!
     */
    @Modifying(clearAutomatically = true, flushAutomatically = true)
    @Transactional
    @Query("""
        UPDATE Subscription s
        SET s.status = 'EXPIRED', s.updatedAt = NOW()
        WHERE s.status = 'ACTIVE'
          AND s.validUntil < :now
        """)
    int expireOverdueSubscriptions(@Param("now") Instant now);

    @Modifying(clearAutomatically = true)
    @Transactional
    @Query("DELETE FROM Subscription s WHERE s.status = 'CANCELLED' AND s.updatedAt < :cutoff")
    int purgeOldCancelledSubscriptions(@Param("cutoff") Instant cutoff);
}
```

### 5.4 Anti-Pattern: Native Queries Without Parameter Binding
```java
// =========================================================================
// ANTI-PATTERN: Dynamic Native SQL via String Concatenation (SQL Injection Risk)
// =========================================================================
public interface BadRepository extends JpaRepository<User, Long> {

    // SEVERE SECURITY FLAW:
    // String concatenation inside custom repository implementations or @Query!
    // Never concatenate user inputs into native or JPQL strings!
    @Query(value = "SELECT * FROM users WHERE email = '" + "#{input}" + "'", nativeQuery = true)
    User findByDangerousConcatenation(String input);
}

// =========================================================================
// PRODUCTION FIX: Explicit Named Parameter Bindings
// =========================================================================
public interface GoodRepository extends JpaRepository<User, Long> {

    // SAFE: Database driver binds value securely via prepared statement parameter
    @Query(value = "SELECT * FROM users WHERE email = :email", nativeQuery = true)
    Optional<User> findBySafeBinding(@Param("email") String email);
}
```

### 5.5 Production Dynamic Filtering with Specification and CriteriaBuilder
```java
package com.example.jpa.specification;

import com.example.jpa.domain.Product;
import jakarta.persistence.criteria.Predicate;
import org.springframework.data.jpa.domain.Specification;

import java.math.BigDecimal;
import java.util.ArrayList;
import java.util.List;

public class ProductSpecifications {

    public record ProductFilterCriteria(
        String nameQuery,
        BigDecimal minPrice,
        BigDecimal maxPrice,
        String category,
        Boolean inStockOnly
    ) {}

    public static Specification<Product> withDynamicFilter(ProductFilterCriteria criteria) {
        return (root, query, cb) -> {
            List<Predicate> predicates = new ArrayList<>();

            if (criteria.nameQuery() != null && !criteria.nameQuery().isBlank()) {
                predicates.add(cb.like(
                    cb.lower(root.get("name")), 
                    "%" + criteria.nameQuery().toLowerCase() + "%"
                ));
            }

            if (criteria.minPrice() != null) {
                predicates.add(cb.greaterThanOrEqualTo(root.get("price"), criteria.minPrice()));
            }

            if (criteria.maxPrice() != null) {
                predicates.add(cb.lessThanOrEqualTo(root.get("price"), criteria.maxPrice()));
            }

            if (criteria.category() != null && !criteria.category().isBlank()) {
                predicates.add(cb.equal(root.get("category").get("name"), criteria.category()));
            }

            if (Boolean.TRUE.equals(criteria.inStockOnly())) {
                predicates.add(cb.greaterThan(root.get("stockQuantity"), 0));
            }

            // Combine all predicates using AND
            return cb.and(predicates.toArray(new Predicate[0]));
        };
    }
}
```

```java
package com.example.jpa.service;

import com.example.jpa.domain.Product;
import com.example.jpa.repository.ProductRepository;
import com.example.jpa.specification.ProductSpecifications;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class ProductCatalogService {

    private final ProductRepository productRepository;

    public ProductCatalogService(ProductRepository productRepository) {
        this.productRepository = productRepository;
    }

    @Transactional(readOnly = true)
    public Page<Product> searchCatalog(ProductSpecifications.ProductFilterCriteria filter, Pageable pageable) {
        // Compose specification and execute paginated search
        Specification<Product> spec = ProductSpecifications.withDynamicFilter(filter);
        return productRepository.findAll(spec, pageable);
    }
}
```

---

## 6. Best Practices

1. **Prefer JPQL Over Native Queries**: JPQL preserves database portability, leverages entity cache management, and validates types against the JPA metamodel at startup.
2. **Always Use `JOIN FETCH` to Prevent N+1 Problems**: In JPQL, normal `JOIN` does not initialize lazy collections. Use `JOIN FETCH` to load parent and child associations in one query.
3. **Always Set `clearAutomatically = true` on `@Modifying`**: Bulk `UPDATE` and `DELETE` queries bypass the persistence context. Clearing the L1 cache ensures subsequently loaded entities reflect updated database values.
4. **Use Java Records for Native Query Projections**: Map multi-column native aggregate queries directly to Java 21 records using column alias matching.
5. **Always Explicitly Name Parameters via `@Param`**: While modern Java compilers preserve parameter names with `-parameters`, explicit `@Param("name")` guarantees resilience across different build environments.
6. **Compose Specifications Using Lambdas**: Avoid legacy verbose criteria classes; write compact, reusable static methods returning `Specification<T>`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Omitting `@Modifying` on UPDATE/DELETE** | Spring Data assumes `@Query` is a `SELECT` statement, throwing `InvalidDataAccessApiUsageException`. | Annotate mutation queries with `@Modifying`. |
| **Omitting `clearAutomatically = true` on Bulk Updates** | The persistence context retains stale entity instances in memory, causing subsequent code to read outdated data. | Always specify `@Modifying(clearAutomatically = true)`. |
| **Referencing Database Column Names in JPQL** | JPQL operates on Java entity property names (`u.firstName`), not physical database columns (`u.first_name`). | Use entity field names in JPQL statements. |
| **Using `JOIN FETCH` with Offset Pagination on Collections** | In-memory pagination warning: Hibernate fetches all rows into memory to apply pagination in Java, threatening memory exhaustion. | Use Keyset pagination or fetch child entity IDs in a two-step query. |
| **Passing `null` into Native Queries Without NULL Handling** | In native SQL, `:param IS NULL` may fail or require explicit cast in PostgreSQL (`:param::text IS NULL`). | Use `COALESCE` or dynamic Specifications for nullable filters. |
| **Writing Native Queries in Lowercase Table Names on Case-Sensitive DBs** | Native queries bypass JPA naming strategies. If the schema uses uppercase names (Oracle/DB2), the query fails. | Match exact physical database identifiers in native SQL. |

---

## 8. Performance & Security Notes

### Performance
- **Bulk Update Throughput**: Executing `@Modifying @Query("UPDATE ...")` executes a single SQL statement in the database, avoiding the overhead of loading thousands of entities into heap memory and dirty checking each one.
- **SQM Compilation Optimization**: Hibernate 6.6 caches compiled SQM AST trees; subsequent executions of the same JPQL statement skip parsing overhead entirely.

### Security
- **Strict Parameter Isolation**: Both JPQL and Native queries using `:param` or `?1` bind values strictly via JDBC `PreparedStatement.setObject()`, completely neutralizing SQL injection risks.
- **Specification Injection Safety**: When building dynamic criteria, using `cb.equal()` and `cb.like()` with parameter arguments maintains total protection against injection attacks.

---

## 9. Comparison / When to Use What

| Feature | JPQL (`@Query`) | Native SQL (`nativeQuery=true`) | Specification<T> |
| :--- | :--- | :--- | :--- |
| **Target Syntax** | Entity & Property names | Physical Tables & Columns | Criteria API Root & Path |
| **Portability** | Dialect Agnostic | Vendor Specific | Dialect Agnostic |
| **Dynamic Filters** | Clumsy (requires SPEL) | Difficult | **Best (Composable Functional API)** |
| **L1 Cache Aware** | Yes | No (Bypasses L1) | Yes |
| **Advanced SQL Features** | Basic to Intermediate | **Full (CTEs, Window funcs, JSONB)** | Intermediate |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the difference between JPQL and Native SQL?</strong></summary>

JPQL (Java Persistence Query Language) operates on abstract entities and their object properties (e.g., `SELECT u FROM User u`), making it completely portable across different database vendors. Native SQL executes directly on the physical database engine using actual table and column names (`SELECT * FROM users`), unlocking database-specific features (e.g., PostgreSQL JSONB, window functions) at the expense of portability.
</details>

<details>
<summary><strong>2. Why is <code>clearAutomatically = true</code> critical on <code>@Modifying</code> update queries?</strong></summary>

Bulk `UPDATE` and `DELETE` queries executed via `@Modifying` are translated directly into JDBC SQL statements and sent to the database without updating the entities currently held in the Hibernate First-Level Cache (Persistence Context). Setting `clearAutomatically = true` clears the persistence context immediately after the query executes, forcing subsequent reads to fetch the fresh, updated state from the database.
</details>

<details>
<summary><strong>3. How does <code>JOIN FETCH</code> differ from a standard <code>JOIN</code> in JPQL?</strong></summary>

A standard `JOIN` in JPQL filters the query based on the association but does not initialize or populate the associated child collection. A `JOIN FETCH` instructs Hibernate to include all attributes of the joined entity in the `SELECT` clause, populating the parent and child entities in a single SQL query and eliminating the N+1 query problem.
</details>

<details>
<summary><strong>4. What is a Spring Data <code>Specification</code> and how does it work?</strong></summary>

A `Specification` is an implementation of the Domain-Driven Design Specification pattern built on top of the JPA Criteria API. It provides a single method `toPredicate(Root, CriteriaQuery, CriteriaBuilder)` that generates a JPA `Predicate`. Specifications can be chained using `.and()`, `.or()`, and `.not()` to dynamically construct complex WHERE clauses based on user search inputs.
</details>

<details>
<summary><strong>5. Why does Hibernate log a warning when applying pagination to a JPQL query with <code>JOIN FETCH</code> on a collection?</strong></summary>

Relational SQL joins on one-to-many relationships produce duplicate parent rows for each child row. If SQL `LIMIT` and `OFFSET` are applied directly to the joined result set, parent records will be truncated prematurely. To avoid returning corrupted entities, Hibernate loads *all* matching rows into JVM memory and performs pagination in Java memory, which can lead to high memory consumption on large tables.
</details>

<details>
<summary><strong>6. Can native queries return mapped JPA entities?</strong></summary>

Yes. If a native query selects all columns of the underlying entity table (e.g., `SELECT * FROM users`), Spring Data JPA and Hibernate can automatically map the resulting rows into managed `@Entity` instances. If the query returns a partial subset of columns or aggregates, it must be mapped to a projection interface or Java record instead.
</details>

<details>
<summary><strong>7. How do you pass a collection parameter into a JPQL query?</strong></summary>

You define the JPQL query using the `IN :collectionParam` syntax:
```java
@Query("SELECT u FROM User u WHERE u.status IN :statuses")
List<User> findByStatuses(@Param("statuses") Collection<String> statuses);
```
Hibernate expands the collection into comma-separated SQL positional parameters automatically.
</details>

<details>
<summary><strong>8. What is the role of <code>JpaSpecificationExecutor</code>?</strong></summary>

`JpaSpecificationExecutor<T>` is an interface that your repository extends alongside `JpaRepository`. It provides methods like `findOne(Specification)`, `findAll(Specification)`, and `findAll(Specification, Pageable)` to execute dynamically constructed Criteria queries.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic JPQL Projection Query</strong></summary>

**Task**: Write a JPQL query in `EmployeeRepository` to find all employees with a salary greater than a specified threshold, ordered by hire date descending.

```java
public interface EmployeeRepository extends JpaRepository<Employee, Long> {
    @Query("SELECT e FROM Employee e WHERE e.salary > :minSalary ORDER BY e.hireDate DESC")
    List<Employee> findHighEarners(@Param("minSalary") BigDecimal minSalary);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Native SQL Count Query</strong></summary>

**Task**: Write a native query that counts active users in a PostgreSQL table using a raw native statement.

```java
@Query(value = "SELECT COUNT(*) FROM users WHERE is_active = TRUE", nativeQuery = true)
long countActiveUsersNative();
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Bulk Status Deactivation with @Modifying</strong></summary>

**Task**: Implement a repository method that bulk-updates all users who have not logged in since a cutoff timestamp to status `"INACTIVE"`, clearing the persistence context.

```java
@Modifying(clearAutomatically = true)
@Transactional
@Query("UPDATE User u SET u.status = 'INACTIVE' WHERE u.lastLoginAt < :cutoff")
int deactivateInactiveUsers(@Param("cutoff") Instant cutoff);
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Composing Specifications with AND/OR</strong></summary>

**Task**: Write two simple specifications (`hasRole(role)` and `isCreatedAfter(date)`) and combine them dynamically in a service method.

```java
Specification<User> spec = Specification
    .where(UserSpecs.hasRole("ADMIN"))
    .and(UserSpecs.isCreatedAfter(Instant.now().minus(30, ChronoUnit.DAYS)));

List<User> recentAdmins = userRepository.findAll(spec);
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Dynamic Multi-Filter Specification Implementation</strong></summary>

**Task**: Implement a Specification for an `Order` entity that dynamically filters by customer email (string like), status list (IN clause), and min/max order total.

```java
public static Specification<Order> searchOrders(String email, List<String> statuses, BigDecimal minTotal) {
    return (root, query, cb) -> {
        List<Predicate> predicates = new ArrayList<>();
        if (email != null && !email.isBlank()) {
            predicates.add(cb.like(cb.lower(root.get("customer").get("email")), "%" + email.toLowerCase() + "%"));
        }
        if (statuses != null && !statuses.isEmpty()) {
            predicates.add(root.get("status").in(statuses));
        }
        if (minTotal != null) {
            predicates.add(cb.greaterThanOrEqualTo(root.get("totalAmount"), minTotal));
        }
        return cb.and(predicates.toArray(new Predicate[0]));
    };
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Native Recursive Common Table Expression (CTE) Query</strong></summary>

**Task**: Write a native PostgreSQL query in `CategoryRepository` using a recursive CTE to fetch all child categories under a given root parent ID.

```java
@Query(value = """
    WITH RECURSIVE CategoryHierarchy AS (
        SELECT id, name, parent_id
        FROM categories
        WHERE id = :rootId
        UNION ALL
        SELECT c.id, c.name, c.parent_id
        FROM categories c
        INNER JOIN CategoryHierarchy ch ON c.parent_id = ch.id
    )
    SELECT * FROM CategoryHierarchy
    """, nativeQuery = true)
List<Category> findAllDescendants(@Param("rootId") Long rootId);
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| JPQL:         @Query("SELECT u FROM User u WHERE u.email = :email")                   |
| Native SQL:   @Query(value = "SELECT * FROM users WHERE ...", nativeQuery = true)     |
| Fetching:     Use JOIN FETCH to initialize lazy associations and eliminate N+1 queries|
| Bulk Updates: @Modifying(clearAutomatically = true) for UPDATE/DELETE queries         |
| Dynamic:      Extend JpaSpecificationExecutor<T> and compose Specification<T> filters |
| Metamodel:    JPQL uses Entity and Field names; Native uses Tables and Columns        |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Data JPA @Query Reference](https://docs.spring.io/spring-data/jpa/reference/jpa/query-methods.html#jpa.query-methods.at-query)
- [JPA Criteria API & Spring Data Specifications](https://docs.spring.io/spring-data/jpa/reference/jpa/specifications.html)
- [Hibernate 6.6 Semantic Query Model (SQM) Architecture](https://docs.jboss.org/hibernate/orm/6.6/userguide/html_single/Hibernate_User_Guide.html#hql)
- [Vlad Mihalcea: The Best Way to Use Bulk Updates with Spring Data JPA](https://vladmihalcea.com/spring-data-jpa-modifying-annotation/)
