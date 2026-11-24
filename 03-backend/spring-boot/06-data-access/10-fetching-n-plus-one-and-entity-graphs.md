# Fetching Strategies, N+1 Query Elimination & EntityGraphs

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | `03-spring-data-jpa-fundamentals.md`, `04-entity-mapping-and-relationships.md`, `06-jpql-native-and-specifications.md` |
| **Estimated Reading Time** | 30 minutes |
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
   - [5.1 Diagnosing the N+1 Query Problem with SQL Logging](#51-diagnosing-the-n1-query-problem-with-sql-logging)
   - [5.2 Eliminating N+1 via JPQL JOIN FETCH](#52-eliminating-n1-via-jpql-join-fetch)
   - [5.3 Dynamic Fetch Plans with Spring Data @EntityGraph](#53-dynamic-fetch-plans-with-spring-data-entitygraph)
   - [5.4 Anti-Pattern: MultipleBagFetchException with Two List Collections](#54-anti-pattern-multiplebagfetchexception-with-two-list-collections)
   - [5.5 Mitigating Residual N+1 with Hibernate @BatchSize](#55-mitigating-residual-n1-with-hibernate-batchsize)
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
The **N+1 Query Problem** is the single most prevalent performance flaw in ORM-based applications. It occurs when an initial query fetches $1$ parent record or list of $N$ parent records, and subsequent code iterates through the collection to access an associated child relationship (`post.getComments()`). Because the association is configured with `FetchType.LAZY` (or poorly configured `EAGER`), Hibernate transparently issues $N$ individual secondary `SELECT` statements—one for each parent entity. A simple query intended to fetch 100 posts suddenly executes $1 + 100 = 101$ database queries, causing severe network round-trip latency, connection pool starvation, and database CPU saturation.

### Historical Evolution
In early Hibernate versions, developers attempted to resolve N+1 queries by marking associations `FetchType.EAGER`. This worsened the problem by ensuring the secondary queries were executed *always*, even when the business logic did not need the child data. JPA 2.1 introduced **Entity Graphs**, allowing developers to override static fetch plans dynamically at query execution time. Combined with JPQL `JOIN FETCH` and Hibernate's `@BatchSize` batch-fetching mechanism, modern Spring Data JPA provides granular control to eliminate the N+1 problem completely.

### Real-World Relevance
In production systems, eliminating N+1 queries can drop endpoint response times from 3,500ms to 45ms and reduce database network traffic by 90%. Mastering `JOIN FETCH`, `@EntityGraph`, and batch sizing is a mandatory competency for any senior Spring backend engineer.

---

## 2. Core Concepts

### The N+1 Math
$$\text{Total Queries} = 1 + N$$
Where:
- $1$ is the root query fetching $N$ parent rows.
- $N$ is the number of subsequent queries executed when traversing the relationship.

### JOIN FETCH
A JPQL operator that instructs the query planner to fetch the associated entity or collection in the same SQL `SELECT` statement via an inner or left outer join, immediately populating the child collection without secondary queries.

### @EntityGraph
A declarative annotation in Spring Data JPA that overrides static `FetchType.LAZY` declarations for a specific query:
- `EntityGraphType.FETCH`: Attributes specified in the graph are fetched eagerly (`EAGER`); all other attributes are fetched lazily (`LAZY`).
- `EntityGraphType.LOAD`: Attributes specified in the graph are fetched eagerly; attributes *not* specified retain their static mapping (`LAZY` or `EAGER`).

### Hibernate @BatchSize
A fail-safe optimization that replaces $N$ individual queries with $\lceil N / \text{batchSize} \rceil$ queries using SQL `IN` clauses:
```sql
SELECT * FROM comments WHERE post_id IN (?, ?, ?, ... ?);
```

### MultipleBagFetchException
An exception thrown by Hibernate when attempting to `JOIN FETCH` two or more `java.util.List` ("bag") collections simultaneously in a single JPQL query. Hibernate refuses because joining multiple bags creates a Cartesian product that corrupts collection indexes.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         N+1 Queries vs JOIN FETCH Execution                        |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Scenario A: The N+1 Problem (findAll() followed by loop)                          |
|  Query 1: SELECT * FROM posts; (Returns 3 posts: IDs 1, 2, 3)                      |
|  Iteration 1: post1.getComments() -> Query 2: SELECT * FROM comments WHERE post_id=1|
|  Iteration 2: post2.getComments() -> Query 3: SELECT * FROM comments WHERE post_id=2|
|  Iteration 3: post3.getComments() -> Query 4: SELECT * FROM comments WHERE post_id=3|
|  Total Network Round-Trips = 4                                                     |
|                                                                                    |
|  Scenario B: Single Query Elimination via JOIN FETCH                               |
|  Query 1: SELECT p, c FROM Post p LEFT JOIN FETCH p.comments c;                    |
|  SQL Generated:                                                                    |
|    SELECT p.id, p.title, c.id, c.text                                              |
|    FROM posts p                                                                    |
|    LEFT OUTER JOIN comments c ON p.id = c.post_id;                                 |
|  Total Network Round-Trips = 1! (Hibernate assembles Post object graph in memory)  |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Basic @EntityGraph Usage
```java
public interface PostRepository extends JpaRepository<Post, Long> {

    // Overrides FetchType.LAZY on comments for this query only
    @EntityGraph(attributePaths = {"comments", "author"})
    List<Post> findByStatus(String status);
}
```

### Basic JPQL JOIN FETCH
```java
public interface PostRepository extends JpaRepository<Post, Long> {

    @Query("SELECT DISTINCT p FROM Post p LEFT JOIN FETCH p.comments WHERE p.status = :status")
    List<Post> findPostsWithComments(@Param("status") String status);
}
```

---

## 5. Practical Examples

### 5.1 Diagnosing the N+1 Query Problem with SQL Logging
```yaml
# application-dev.yaml - Exposing the N+1 query loop in logs
spring:
  jpa:
    show-sql: true
    properties:
      hibernate:
        format_sql: true
        # Tracks JDBC query execution count
        generate_statistics: true
```

```java
package com.example.jpa.service;

import com.example.jpa.domain.Post;
import com.example.jpa.repository.PostRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Service
public class VulnerablePostService {

    private final PostRepository postRepository;

    public VulnerablePostService(PostRepository postRepository) {
        this.postRepository = postRepository;
    }

    /**
     * VULNERABLE CODE: Triggers 1 + N queries!
     * If 50 posts are returned, Hibernate executes 51 queries in total!
     */
    @Transactional(readOnly = true)
    public void printPostCommentCounts() {
        // Query 1: SELECT * FROM posts
        List<Post> posts = postRepository.findAll();

        for (Post post : posts) {
            // Queries 2 to 51: SELECT * FROM comments WHERE post_id = ?
            System.out.println("Post " + post.getId() + " has " + post.getComments().size() + " comments");
        }
    }
}
```

### 5.2 Eliminating N+1 via JPQL JOIN FETCH
```java
package com.example.jpa.repository;

import com.example.jpa.domain.Post;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.util.List;

public interface PostRepository extends JpaRepository<Post, Long> {

    /**
     * Eliminates N+1: Fetches posts and comments in ONE single SQL join query!
     * DISTINCT keyword ensures duplicate parent Post instances produced by
     * the SQL join are de-duplicated in Hibernate's identity map.
     */
    @Query("""
        SELECT DISTINCT p FROM Post p
        LEFT JOIN FETCH p.comments c
        LEFT JOIN FETCH p.author a
        WHERE p.published = true
        ORDER BY p.createdAt DESC
        """)
    List<Post> findAllPublishedWithCommentsAndAuthor();
}
```

### 5.3 Dynamic Fetch Plans with Spring Data @EntityGraph
```java
package com.example.jpa.repository;

import com.example.jpa.domain.Order;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.EntityGraph;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;
import java.util.Optional;

public interface OrderRepository extends JpaRepository<Order, Long> {

    // Ad-hoc attribute path entity graph: Eagerly fetches items and customer
    @EntityGraph(
        attributePaths = {"items", "customer", "customer.address"},
        type = EntityGraph.EntityGraphType.FETCH
    )
    Optional<Order> findWithDetailsById(Long id);

    // Entity graph with derived search
    @EntityGraph(attributePaths = {"customer"})
    List<Order> findByStatus(String status);
}
```

### 5.4 Anti-Pattern: MultipleBagFetchException with Two List Collections
```java
// =========================================================================
// ANTI-PATTERN: Multiple JOIN FETCH on two List collections simultaneously
// =========================================================================
public interface BadRepository extends JpaRepository<User, Long> {

    // RUNTIME EXCEPTION: org.hibernate.loader.MultipleBagFetchException: cannot simultaneously fetch multiple bags!
    // A 'bag' (java.util.List without @OrderColumn) joined with another bag creates
    // a Cartesian Product (e.g. 10 roles * 10 permissions = 100 rows)!
    @Query("SELECT u FROM User u JOIN FETCH u.roles JOIN FETCH u.permissions WHERE u.id = :id")
    User findUserWithRolesAndPermissions(Long id);
}

// =========================================================================
// PRODUCTION FIX 1: Use java.util.Set instead of List for Collections
// =========================================================================
@Entity
public class User {
    @OneToMany(mappedBy = "user")
    private Set<Role> roles = new HashSet<>(); // Set has no bag index ambiguity!

    @OneToMany(mappedBy = "user")
    private Set<Permission> permissions = new HashSet<>();
}

// =========================================================================
// PRODUCTION FIX 2: Split into Two Sequential Single-Join Queries
// =========================================================================
@Service
public class UserService {
    @Transactional(readOnly = true)
    public User loadUserFully(Long userId) {
        // Query 1: Loads User and JOIN FETCH roles into First-Level Cache
        User user = userRepository.findWithRoles(userId);
        
        // Query 2: Loads User and JOIN FETCH permissions into same L1 Cache
        // Hibernate re-uses the managed User instance and attaches permissions!
        user = userRepository.findWithPermissions(userId);
        
        return user; // Fully initialized without Cartesian product!
    }
}
```

### 5.5 Mitigating Residual N+1 with Hibernate @BatchSize
```java
package com.example.jpa.domain;

import jakarta.persistence.*;
import org.hibernate.annotations.BatchSize;

import java.util.ArrayList;
import java.util.List;

@Entity
@Table(name = "departments")
public class Department {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    private String name;

    /**
     * Fail-safe optimization: If comments are accessed lazily without a JOIN FETCH,
     * Hibernate will load them in batches of 25 using an SQL IN clause,
     * reducing 100 queries to only 4 queries!
     */
    @BatchSize(size = 25)
    @OneToMany(mappedBy = "department", fetch = FetchType.LAZY)
    private List<Employee> employees = new ArrayList<>();

    public Long getId() { return id; }
    public String getName() { return name; }
    public List<Employee> getEmployees() { return employees; }
}
```

---

## 6. Best Practices

1. **Keep All Associations `FetchType.LAZY` by Default**: Never configure `FetchType.EAGER` in entity mappings; always use query-specific `@EntityGraph` or `JOIN FETCH` to load associations explicitly.
2. **Use `@BatchSize` Globally as a Safety Net**: Configure `spring.jpa.properties.hibernate.default_batch_fetch_size: 25` in `application.yaml` to mitigate accidental N+1 queries across the entire project.
3. **Use `Set` Instead of `List` for Child Collections**: Modeling associations with `Set` prevents `MultipleBagFetchException` and mirrors true relational set semantics.
4. **Use Sequential Two-Step Fetching for Multiple Collections**: If an entity has two large collections, fetch them in two consecutive queries within the same transaction to avoid Cartesian explosion.
5. **Always Audit Generated SQL in Integration Tests**: Enable `generate_statistics` or assert JDBC query counts using test libraries like QuickPerf or Hypersistence Utils.
6. **Prefer DTO Projections for Read-Only Views**: When data is only needed for display, skip entity graphs entirely and project directly to Java records.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Setting `FetchType.EAGER` to "Fix" N+1** | Triggers unexpected queries everywhere the entity is loaded, worsening performance. | Set `FetchType.LAZY` and use `@EntityGraph` or `JOIN FETCH` per query. |
| **Joining Multiple Bags in Single Query** | Throws `MultipleBagFetchException` due to Cartesian product ambiguity. | Change `List` to `Set` or use two separate queries in one transaction. |
| **`JOIN FETCH` on Collection with Offset Pagination** | Hibernate applies pagination in JVM memory instead of SQL `LIMIT`, logging a severe performance warning. | Use Keyset pagination or fetch IDs first, then fetch collections. |
| **Forgetting `DISTINCT` on `JOIN FETCH`** | In older Hibernate versions or complex queries, SQL joins duplicate parent entity references. | Add `SELECT DISTINCT` in JPQL. |
| **Overly Deep Entity Graphs** | Fetching graphs with 4+ levels of nesting creates massive multi-table joins that overwhelm the database query optimizer. | Restrict entity graphs to immediate 1-2 level dependencies. |
| **Calling Size/Iterating Inside Loops Without Pre-fetching** | Transparently triggers N individual SQL queries behind the scenes. | Pre-fetch associations using `@EntityGraph` or JPQL before entering loops. |

---

## 8. Performance & Security Notes

### Performance
- **Cartesian Explosion Prevention**: Joining an entity with two collections of 20 items each produces $20 \times 20 = 400$ joined rows. Joining three collections of 20 items produces $20 \times 20 \times 20 = 8,000$ rows! Always fetch multiple collections in separate query steps.
- **Global Batch Fetching**: Setting `hibernate.default_batch_fetch_size = 25` compresses $N$ queries down to $\lceil N / 25 \rceil$, providing an immediate $90\%$ reduction in database round-trips.

### Security
- **Denial of Service via Unbounded Graphs**: Uncontrolled deep entity graphs exposed to user search queries can cause the database to execute queries that join 10+ tables, exhausting database CPU and memory.

---

## 9. Comparison / When to Use What

| Technique | Where Configured | Dynamic per Query? | Mitigates N+1? | Cartesian Risk? |
| :--- | :--- | :--- | :--- | :--- |
| **`JOIN FETCH`** | JPQL `@Query` | **Yes** | **Yes (1 query)** | High (if multiple lists) |
| **`@EntityGraph`** | Repository method | **Yes** | **Yes (1 query)** | High (if multiple lists) |
| **`@BatchSize`** | Entity or global config | No (Fallback) | **Partially ($\lceil N/B \rceil$ queries)** | **None** |
| **DTO Projection** | Repository / JPQL | **Yes** | **Yes (1 query)** | **None (Flat columns)** |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the N+1 query problem and why is it detrimental to application performance?</strong></summary>

The N+1 problem occurs when an application executes 1 initial query to fetch $N$ parent records, and subsequently executes $N$ additional queries to fetch associated child entities for each parent row. It causes severe network latency due to repetitive round-trips between the application and database, exhausts database connection pool threads, and degrades database CPU throughput.
</details>

<details>
<summary><strong>2. How does <code>JOIN FETCH</code> solve the N+1 problem in JPQL?</strong></summary>

`JOIN FETCH` instructs Hibernate to perform an SQL `LEFT JOIN` or `INNER JOIN` in the initial `SELECT` statement, fetching both the parent and child entity columns in a single database round-trip. Hibernate then maps the joined result set directly into the parent's collection in memory.
</details>

<details>
<summary><strong>3. What is the difference between <code>EntityGraphType.FETCH</code> and <code>EntityGraphType.LOAD</code>?</strong></summary>

- `FETCH`: All attributes specified in the graph are fetched eagerly (`EAGER`). All attributes *not* specified in the graph are treated as `LAZY`, regardless of their entity mapping.
- `LOAD`: Attributes specified in the graph are fetched eagerly. Attributes *not* specified in the graph retain their default static mapping configuration (`LAZY` or `EAGER`).
</details>

<details>
<summary><strong>4. Why does Hibernate throw <code>MultipleBagFetchException</code> and how do you resolve it?</strong></summary>

In Java, a `List` without an `@OrderColumn` is mapped in Hibernate as a "bag" (an unordered collection allowing duplicates). If you attempt to `JOIN FETCH` two bags in a single query, the relational cross-join produces a Cartesian product where duplicate rows cannot be correctly assigned to each list without corrupting collection sizes.
**Solutions**:
1. Change collection types from `List` to `Set`.
2. Fetch the collections in two consecutive queries within the same transaction.
</details>

<details>
<summary><strong>5. How does Hibernate's <code>@BatchSize</code> work?</strong></summary>

`@BatchSize(size = N)` alters how Hibernate initializes lazy collections or proxies. Instead of issuing one `SELECT` per parent when accessing a lazy relationship, Hibernate waits until the first lazy collection is accessed, inspects other uninitialized instances of the same type currently in the Persistence Context, and fetches up to $N$ collections in a single query using an SQL `WHERE parent_id IN (?, ?, ...)` clause.
</details>

<details>
<summary><strong>6. Why is pagination with <code>JOIN FETCH</code> on a collection dangerous?</strong></summary>

Because SQL joins duplicate parent rows for each matching child row, applying SQL `LIMIT` and `OFFSET` would slice the joined result set across parent boundaries, returning incomplete parent entities. To prevent data corruption, Hibernate executes the join without `LIMIT/OFFSET`, loads *all* database rows into the JVM heap, and applies pagination in Java memory, which can trigger an `OutOfMemoryError` on large datasets.
</details>

<details>
<summary><strong>7. How can you set a global default batch fetch size in Spring Boot?</strong></summary>

In `application.yaml`:
```yaml
spring:
  jpa:
    properties:
      hibernate:
        default_batch_fetch_size: 30
```
This applies batch-fetching automatically to all lazy collections and entity proxies across the entire application.
</details>

<details>
<summary><strong>8. When should you use a DTO projection instead of an <code>@EntityGraph</code>?</strong></summary>

When the data is intended solely for read-only consumption (e.g., API responses, search tables, reports) and will not be modified or re-persisted. DTO projections select only the specific columns needed, bypassing entity lifecycle tracking and memory overhead entirely.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic @EntityGraph Declaration</strong></summary>

**Task**: Declare a repository query in `UserRepository` using `@EntityGraph` to fetch users with their `roles` collection eagerly for active users.

```java
public interface UserRepository extends JpaRepository<User, Long> {
    @EntityGraph(attributePaths = {"roles"})
    List<User> findByActiveTrue();
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): JPQL JOIN FETCH with Distinct</strong></summary>

**Task**: Write a JPQL query in `AuthorRepository` that fetches authors along with their `books` using `JOIN FETCH` and de-duplicates parent authors.

```java
public interface AuthorRepository extends JpaRepository<Author, Long> {
    @Query("SELECT DISTINCT a FROM Author a LEFT JOIN FETCH a.books")
    List<Author> findAllWithBooks();
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Global Batch Size Configuration</strong></summary>

**Task**: Configure Spring Boot to globally batch-fetch lazy associations with a batch size of 20 and show formatted SQL in logs.

```yaml
spring:
  jpa:
    show-sql: true
    properties:
      hibernate:
        format_sql: true
        default_batch_fetch_size: 20
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Resolving MultipleBagFetchException with Sets</strong></summary>

**Task**: Refactor a `Course` entity that contains both `students` and `instructors` as `List` collections to `Set` collections, and write a query fetching both associations in a single query.

```java
@Entity
public class Course {
    @Id @GeneratedValue
    private Long id;

    @ManyToMany
    private Set<Student> students = new HashSet<>();

    @ManyToMany
    private Set<Instructor> instructors = new HashSet<>();
}

// In Repository:
@Query("SELECT DISTINCT c FROM Course c LEFT JOIN FETCH c.students LEFT JOIN FETCH c.instructors WHERE c.id = :id")
Optional<Course> findCourseWithParticipants(@Param("id") Long id);
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Two-Step Query Pattern for Multiple Collections</strong></summary>

**Task**: Implement a transactional service method that loads an `Order` with its `items` in the first query and its `statusHistory` in the second query without Cartesian products.

```java
@Transactional(readOnly = true)
public Order loadOrderWithAllDetails(Long orderId) {
    // Step 1: Loads order and items into L1 Cache
    Order order = orderRepository.findWithItemsById(orderId)
        .orElseThrow(() -> new EntityNotFoundException("Order not found"));

    // Step 2: Loads statusHistory into same L1 Cache; Hibernate attaches it to order!
    orderRepository.findWithStatusHistoryById(orderId);

    return order;
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): SQL Execution Count Assertion Test</strong></summary>

**Task**: Write a test verifying that querying 10 departments and accessing their employees executes exactly 1 SQL statement instead of 11.

```java
@Test
@Transactional
void testNoNPlusOneQueries() {
    List<Department> depts = departmentRepository.findAllWithEmployees();

    for (Department dept : depts) {
        // Must NOT trigger secondary queries!
        assertThat(dept.getEmployees()).isNotNull();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Problem:       N+1 queries occur when traversing lazy associations in loops.          |
| Solution 1:    @Query("SELECT DISTINCT p FROM Post p LEFT JOIN FETCH p.comments")     |
| Solution 2:    @EntityGraph(attributePaths = {"comments", "author"})                  |
| Safety Net:    hibernate.default_batch_fetch_size: 25                                 |
| Bag Trap:      Never JOIN FETCH two List collections simultaneously (use Sets)        |
| Golden Rule:   Never use FetchType.EAGER on entity definitions; fetch per query!      |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Data JPA EntityGraph Documentation](https://docs.spring.io/spring-data/jpa/reference/jpa/query-methods.html#jpa.entity-graph)
- [Vlad Mihalcea: The N+1 Query Problem in Hibernate](https://vladmihalcea.com/n-plus-1-query-problem/)
- [Hibernate 6.6 Fetching Strategies and Batch Sizing](https://docs.jboss.org/hibernate/orm/6.6/userguide/html_single/Hibernate_User_Guide.html#fetching)
- [How to Solve MultipleBagFetchException in Hibernate](https://vladmihalcea.com/hibernate-multiplebagfetchexception/)
