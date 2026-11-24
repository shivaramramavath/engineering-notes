# Spring Data JPA & Query Execution Cheatsheet

| Metadata | Details |
|---|---|
| **Topic** | Spring Data JPA & Query Syntax Cheatsheet |
| **Track** | Java Spring Boot |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | Java 21, SQL, Jakarta Persistence (Hibernate) |
| **Java Version** | Java 21 / 25 LTS |
| **Spring Boot** | 3.4.x / 4.x |
| **Tested With** | PostgreSQL 16+, Testcontainers |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Categorized Reference](#4-syntax--categorized-reference)
5. [5 Practical Production Examples](#5-5-practical-production-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes to Avoid](#7-common-mistakes-to-avoid)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison Table: JPQL vs Native vs Specifications](#9-comparison-table-jpql-vs-native-vs-specifications)
10. [Technical Interview Questions & Answers](#10-technical-interview-questions--answers)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading & References](#13-further-reading--references)

---

## 1. What & Why
Spring Data JPA abstracts boilerplate persistence operations by generating runtime repository proxies from interface declarations. It unifies derived method names, explicit JPQL/HQL queries, native SQL, typed criteria Specifications, and constructor projections.

A precise reference card ensures queries avoid the infamous N+1 problem, execute bounded pagination efficiently, and utilize database indexes correctly.

---

## 2. Core Concepts
- **Derived Query Methods**: Method signatures parsed into JPQL queries based on naming conventions (`findByStatusAndCreatedAtAfter`).
- **Explicit JPQL `@Query`**: Object-oriented queries operating against entity models rather than database tables.
- **`@EntityGraph`**: Declarative fetch joins dynamically overriding `FetchType.LAZY` to prevent N+1 queries.
- **DTO Projections**: Querying specific column subsets directly into Java 21 records.
- **Keyset (Seek) Pagination**: O(1) performance pagination replacing offset-based `LIMIT ... OFFSET`.

---

## 3. Internals & Architecture
Spring Data JPA Repository Proxy Generation & Execution:

```
[Repository Interface Call: userRepository.findByEmail(email)]
                             |
                             v
+-------------------------------------------------------------+
|              RepositoryFactorySupport Proxy                 |
|             (JdkDynamicAopProxy / SimpleJpaRepository)     |
+-------------------------------------------------------------+
                             |
                             | Looks up PartTreeJpaQuery / SimpleJpaQuery
                             v
+-------------------------------------------------------------+
|                     Query Execution Engine                  |
|    - Evaluates query method parameters (@Param)             |
|    - Applies EntityGraph fetch plans to EntityManager        |
|    - Generates typed JPA / Hibernate Query instance         |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|                     Hibernate ORM Session                   |
|    - Translates JPQL / Criteria into dialect SQL            |
|    - First-Level Cache (Persistence Context) lookup         |
|    - Executes PreparedStatement over JDBC connection        |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|                   Result Mapping Engine                     |
|    - Entity hydration or Constructor Expression -> Record   |
+-------------------------------------------------------------+
```

---

## 4. Syntax & Categorized Reference

### 4.1 Derived Method Keyword Directory
| Keyword | Method Name Example | Generated JPQL Condition |
|---|---|---|
| `And` | `findByLastNameAndFirstName` | `where x.lastName = ?1 and x.firstName = ?2` |
| `Or` | `findByStatusOrRole` | `where x.status = ?1 or x.role = ?2` |
| `Between` | `findByPriceBetween` | `where x.price between ?1 and ?2` |
| `LessThan` / `GreaterThan` | `findByAgeLessThan` | `where x.age < ?1` |
| `In` | `findByStatusIn(Collection)` | `where x.status in ?1` |
| `Like` / `Containing` | `findByNameContaining` | `where x.name like %?1%` |
| `OrderBy` | `findByActiveTrueOrderByCreatedAtDesc` | `where x.active = true order by x.createdAt desc` |
| `Exists` | `existsByEmail` | `select count(x) > 0 from Entity x where x.email = ?1` |
| `Count` | `countByStatus` | `select count(x) from Entity x where x.status = ?1` |

### 4.2 Explicit JPQL & Native Queries
```java
// JPQL with Named Parameters and Fetch Join
@Query("SELECT o FROM Order o JOIN FETCH o.items WHERE o.customer.id = :customerId")
List<Order> findAllByCustomerIdWithItems(@Param("customerId") UUID customerId);

// Native SQL Query with SpEL
@Query(value = "SELECT * FROM orders WHERE status = :status AND created_at > NOW() - INTERVAL '7 days'", nativeQuery = true)
List<Order> findRecentOrdersByStatus(@Param("status") String status);

// DML Update Query
@Modifying(clearAutomatically = true, flushAutomatically = true)
@Query("UPDATE Order o SET o.status = :newStatus WHERE o.status = :oldStatus")
int transitionOrderStatus(@Param("oldStatus") OrderStatus oldStatus, @Param("newStatus") OrderStatus newStatus);
```

---

## 5. 5 Practical Production Examples

### Example 1: DTO Projection into Java 21 Record
```java
public record UserSummary(UUID id, String email, String fullName) {}

public interface UserRepository extends JpaRepository<User, UUID> {

    @Query("SELECT new com.example.dto.UserSummary(u.id, u.email, u.fullName) FROM User u WHERE u.active = true")
    List<UserSummary> findActiveUserSummaries();
}
```

### Example 2: Preventing N+1 with Dynamic `@EntityGraph`
```java
public interface OrderRepository extends JpaRepository<Order, UUID> {

    @EntityGraph(attributePaths = {"customer", "items", "items.product"})
    @Query("SELECT o FROM Order o WHERE o.status = :status")
    List<Order> findOrdersWithFullGraph(@Param("status") OrderStatus status);
}
```

### Example 3: Keyset / Seek-Based Pagination for High-Scale Feeds
```java
public interface AuditLogRepository extends JpaRepository<AuditLog, UUID> {

    @Query("""
        SELECT a FROM AuditLog a
        WHERE a.createdAt < :lastTimestamp
           OR (a.createdAt = :lastTimestamp AND a.id < :lastId)
        ORDER BY a.createdAt DESC, a.id DESC
        LIMIT :limit
        """)
    List<AuditLog> fetchNextPage(
            @Param("lastTimestamp") Instant lastTimestamp,
            @Param("lastId") UUID lastId,
            @Param("limit") int limit);
}
```

### Example 4: Dynamic Query Filtering with Specifications
```java
public class OrderSpecifications {
    public static Specification<Order> hasStatus(OrderStatus status) {
        return (root, query, cb) -> status == null ? cb.conjunction() : cb.equal(root.get("status"), status);
    }

    public static Specification<Order> createdAfter(Instant date) {
        return (root, query, cb) -> date == null ? cb.conjunction() : cb.greaterThanOrEqualTo(root.get("createdAt"), date);
    }
}

// In Service
Specification<Order> spec = Specification.where(OrderSpecifications.hasStatus(status))
                                         .and(OrderSpecifications.createdAfter(cutoff));
List<Order> orders = orderRepository.findAll(spec);
```

### Example 5: Locking Modes (Pessimistic vs Optimistic)
```java
public interface AccountRepository extends JpaRepository<Account, UUID> {

    @Lock(LockModeType.PESSIMISTIC_WRITE)
    @QueryHints({@QueryHint(name = "jakarta.persistence.lock.timeout", value = "3000")})
    @Query("SELECT a FROM Account a WHERE a.id = :id")
    Optional<Account> findByIdForUpdate(@Param("id") UUID id);
}
```

---

## 6. Best Practices
1. **Always Use Projections for Read-Only APIs**: Avoid fetching entire entity graphs when only 3 columns are needed.
2. **Beware of Offset Pagination on Deep Tables**: `PageRequest.of(1000, 50)` triggers costly table scans (`OFFSET 50000`). Use keyset pagination.
3. **Always Add `clearAutomatically = true` to `@Modifying`**: Prevents the Hibernate First-Level Cache from retaining stale entities.
4. **Use `@EntityGraph` for Read Queries**: Solves N+1 queries declaratively without mutating entity-level default fetch types.
5. **Keep FetchType.LAZY on All `@ManyToOne` and `@OneToOne`**: `@ManyToOne` defaults to `EAGER`, which causes silent N+1 queries.

---

## 7. Common Mistakes to Avoid

| Mistake | Consequence | Remedy |
|---|---|---|
| Leaving `@ManyToOne` with default `FetchType.EAGER` | Generates unexpected secondary SELECT queries on every load | Explicitly specify `@ManyToOne(fetch = FetchType.LAZY)` |
| Modifying query without `@Modifying` | Throws `InvalidDataAccessApiUsageException` | Add `@Modifying` on all DML UPDATE/DELETE repository methods |
| Executing DML `@Query` without clearing persistence context | Subsequent entity lookups return stale uncommitted memory state | Set `@Modifying(clearAutomatically = true)` |
| Deep pagination using `Page<T>` with `findAll(Pageable)` | Executes two queries: expensive offset SELECT and costly `COUNT(*)` | Use `Slice<T>` or keyset seek queries |
| Unsanitized string concatenation in native `@Query` | Exposes application to SQL Injection | Always use parameterized bindings (`:param`) |
| In-memory filtering on unbounded database queries | Out-of-memory errors on production data volumes | Filter within SQL/JPQL `WHERE` clauses |

---

## 8. Performance & Security Considerations
- **Index Alignment**: Ensure columns queried in derived methods (`findByEmailAndTenantId`) match multi-column composite database indexes.
- **Statement Batching**: Configure `spring.jpa.properties.hibernate.jdbc.batch_size=50` and `order_inserts=true` to turn single INSERTs into batches.
- **SQL Injection Prevention**: Spring Data named parameters (`:param`) automatically use JDBC `PreparedStatement` parameter bindings.

---

## 9. Comparison Table: JPQL vs Native vs Specifications

| Feature | JPQL `@Query` | Native SQL `@Query` | JPA Specifications |
|---|---|---|---|
| **Portability** | High (DB independent) | Low (Tied to DB dialect) | High (DB independent) |
| **Type Safety** | Evaluated at startup | Runtime only | Full compile-time safety |
| **Complex Joins / CTEs** | Limited | Full database capability | Moderate |
| **Dynamic Filtering** | Clunky (SpEL logic) | Complex | Excellent (Composable predicates) |

---

## 10. Technical Interview Questions & Answers

<details>
<summary>1. How does Spring Data JPA detect the N+1 problem and how do you resolve it?</summary>
The N+1 problem occurs when fetching an entity with lazy relationships triggers 1 initial query plus N additional queries for each child association. It is resolved using `JOIN FETCH` in JPQL or `@EntityGraph(attributePaths = {...})`.
</details>

<details>
<summary>2. What is the difference between `Page<T>` and `Slice<T>`?</summary>
`Page<T>` executes an extra `COUNT(*)` query to determine total element count and total pages, which can be very expensive. `Slice<T>` only fetches `limit + 1` rows to check if a next slice exists without a count query.
</details>

<details>
<summary>3. Why should `@ManyToOne` default fetching be changed to LAZY?</summary>
JPA specifications define `@ManyToOne` default fetching as `EAGER`. This causes Hibernate to automatically fetch related entities via left outer joins or subsequent queries, degrading performance.
</details>

<details>
<summary>4. What does `@Modifying(clearAutomatically = true)` do?</summary>
It forces Hibernate to clear the persistence context (First-Level Cache) after executing the bulk update or delete query so subsequent queries fetch updated database state.
</details>

<details>
<summary>5. How does Keyset Pagination outperform Offset Pagination?</summary>
Offset pagination forces the database to scan and discard rows (`OFFSET 100000`). Keyset pagination utilizes indexed `WHERE id > :lastId ORDER BY id ASC LIMIT 50` to jump directly to the target record in O(1) time.
</details>

<details>
<summary>6. Can Spring Data JPA map queries directly to Java records?</summary>
Yes. Using JPQL constructor expressions (`SELECT new com.example.UserDto(u.id, u.email) FROM User u`) or Spring Data interface-based / record-based projections.
</details>

<details>
<summary>7. What is the role of `JpaRepository` vs `CrudRepository`?</summary>
`CrudRepository` provides basic CRUD operations. `JpaRepository` extends `ListPagingAndSortingRepository` and provides JPA-specific methods like `flush()`, `saveAllAndFlush()`, and batch deletes.
</details>

<details>
<summary>8. How do you implement pessimistic locking in Spring Data JPA?</summary>
Annotate the repository query method with `@Lock(LockModeType.PESSIMISTIC_WRITE)`, which instructs the database dialect to append `FOR UPDATE` to the SQL query.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary>Exercise 1: Create a DTO constructor projection</summary>
Write a repository method that fetches order status and total amount directly into an immutable Java 21 record.
</details>

<details>
<summary>Exercise 2: Fix an N+1 query using @EntityGraph</summary>
Take an entity `Customer` with `orders` and `addresses` and write a repository query using `@EntityGraph` to fetch both in a single query.
</details>

<details>
<summary>Exercise 3: Implement Keyset pagination</summary>
Write a seek pagination query for a Product catalog sorted by `price DESC, id DESC`.
</details>

<details>
<summary>Exercise 4: Write a dynamic specification filter</summary>
Create a specification combining multiple optional search filters: `name`, `status`, and `minRating`.
</details>

<details>
<summary>Exercise 5: Implement bulk soft-delete with @Modifying</summary>
Write a repository method updating `deleted = true` for inactive users older than a specified date.
</details>

<details>
<summary>Exercise 6: Implement Pessimistic Locking with timeout</summary>
Write an inventory deduction query with `PESSIMISTIC_WRITE` and a 2000ms query timeout hint.
</details>

---

## 12. Summary & Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                 SPRING DATA JPA QUERY CHEAT SHEET                               |
+---------------------------------------------------------------------------------+
| DERIVED       | findByEmail, findByStatusAndAgeGreaterThan, existsByUsername    |
| JPQL          | @Query("SELECT u FROM User u JOIN FETCH u.roles WHERE u.id=:id")|
| PROJECTION    | @Query("SELECT new com.app.Dto(u.id, u.name) FROM User u")      |
| GRAPH         | @EntityGraph(attributePaths = {"roles", "department"})          |
| BULK DML      | @Modifying(clearAutomatically = true) @Query("UPDATE ...")      |
| LOCKING       | @Lock(LockModeType.PESSIMISTIC_WRITE)                           |
| PAGINATION    | PageRequest.of(page, size), Slice<T>, Keyset pagination        |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References
- [Spring Data JPA Official Reference Documentation](https://docs.spring.io/spring-data/jpa/reference/)
- [Hibernate ORM 6.6 User Guide](https://docs.jboss.org/hibernate/orm/current/userguide/html_single/Hibernate_User_Guide.html)
- [Vlad Mihalcea - High-Performance Java Persistence](https://vladmihalcea.com/)
- [Keyset Pagination Guide (Use The Index, Luke!)](https://use-the-index-luke.com/no-offset)
