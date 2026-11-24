# Pagination & Sorting: Page, Slice & High-Performance Keyset (Seek) Pagination

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate / Advanced |
| **Prerequisites** | `05-repositories-and-derived-queries.md`, `06-jpql-native-and-specifications.md`, SQL Indexing Fundamentals |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | PostgreSQL 16, Spring Data JPA 3.4.x, Hibernate 6.6.x |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Standard Offset Pagination with Pageable and Page](#51-standard-offset-pagination-with-pageable-and-page)
   - [5.2 Count-Free Pagination with Slice for Infinite Scroll](#52-count-free-pagination-with-slice-for-infinite-scroll)
   - [5.3 Production Keyset (Seek/Cursor) Pagination](#53-production-keyset-seekcursor-pagination)
   - [5.4 Anti-Pattern: Deep Offset Scanning on Million-Row Tables](#54-anti-pattern-deep-offset-scanning-on-million-row-tables)
   - [5.5 Composite Keyset Pagination with Multiple Sort Columns](#55-composite-keyset-pagination-with-multiple-sort-columns)
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
When client applications display datasets containing hundreds of thousands or millions of records (e.g., transactions, user lists, log streams), attempting to fetch all records in a single query exhausts server memory, congests database network bandwidth, and freezes client web browsers. While offset-based pagination (`OFFSET 10000 LIMIT 20`) is widely used, it suffers from two critical flaws:
1. **The Count Query Trap**: Calculating total pages via `SELECT COUNT(*)` requires a complete table scan on large unindexed datasets.
2. **Deep Offset Degradation**: Databases must scan and discard all preceding offset rows ($N$) before returning the desired page size ($M$), turning an $O(1)$ query into an $O(N)$ resource drain.

### Historical Evolution
Spring Data standardized pagination across data stores using the `Pageable`, `Sort`, and `Page<T>` abstractions. To address the overhead of total count queries, Spring Data introduced `Slice<T>`, which requests `pageSize + 1` rows to determine whether subsequent pages exist without running `COUNT(*)`. For ultra-high-throughput systems, **Keyset Pagination (Seek/Cursor method)** has become the gold standard, leveraging B-Tree index traversal to achieve constant-time $O(1)$ pagination regardless of page depth.

### Real-World Relevance
Understanding the trade-offs between `Page`, `Slice`, and `Keyset` pagination is essential for building scalable APIs. An e-commerce catalog might require classic `Page` pagination for numbered UI navigation, infinite-scroll mobile feeds rely on `Slice`, and massive financial audit streams require index-backed Keyset pagination to prevent database collapse.

---

## 2. Core Concepts

### Pageable & Sort
- `Pageable`: An abstract request specification containing page index (zero-based), page size, and sorting directives. Implemented primarily via `PageRequest.of(page, size, sort)`.
- `Sort`: Directs query ordering (e.g., `Sort.by(Direction.DESC, "createdAt")`).

### Page<T> vs Slice<T> vs List<T>
1. **`Page<T>`**: Represents a sub-list of a query result along with total element count and total page count. **Executes two queries**: one for data (`LIMIT/OFFSET`) and one for the count (`SELECT COUNT(*)`).
2. **`Slice<T>`**: Knows only if a next slice is available (`hasNext()`). **Executes only one query** with `LIMIT (size + 1)` and zero count queries. Perfect for mobile infinite-scrolling feeds.
3. **`List<T>` with Pageable**: Executes a simple `LIMIT/OFFSET` query, returning raw rows without total page or next-item indicators.

### Keyset (Seek / Cursor) Pagination
Instead of telling the database to skip $N$ rows using `OFFSET`, Keyset pagination remembers the values of the last record seen on the previous page and uses an indexed `WHERE` predicate:
```sql
WHERE id > :lastSeenId ORDER BY id ASC LIMIT 20
```
Because the database directly traverses the B-Tree index to locate `:lastSeenId`, execution time is strictly $O(1)$, even on page 10,000!

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         Offset vs Keyset Execution Mechanics                       |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Scenario A: Offset Pagination (OFFSET 500,000 LIMIT 10)                           |
|  1. Database engine locates index root                                             |
|  2. Engine MUST READ and DISCARD 500,000 index entries / table rows!              |
|  3. Engine returns the 10 target rows                                              |
|  Time Complexity: O(N) where N = offset (High CPU, disk read saturation!)          |
|                                                                                    |
|  Scenario B: Keyset / Seek Pagination (WHERE id > 500,000 ORDER BY id LIMIT 10)    |
|  1. Database engine navigates B-Tree index directly to key 500,000 (O(log K))      |
|  2. Scans forward exactly 10 adjacent leaf nodes in the index                      |
|  3. Returns the 10 target rows                                                     |
|  Time Complexity: O(1) constant time seek! (Zero disk scan overhead)              |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Basic Repository Methods
```java
public interface UserRepository extends JpaRepository<User, Long> {

    // Returns Page with total count (Two queries executed)
    Page<User> findByStatus(String status, Pageable pageable);

    // Returns Slice without total count (One query executed with size + 1)
    Slice<User> findByRole(String role, Pageable pageable);

    // Simple bounded list
    List<User> findTop100ByOrderByCreatedAtDesc();
}
```

---

## 5. Practical Examples

### 5.1 Standard Offset Pagination with Pageable and Page
```java
package com.example.jpa.service;

import com.example.jpa.domain.Order;
import com.example.jpa.dto.OrderSummaryDto;
import com.example.jpa.repository.OrderRepository;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.PageRequest;
import org.springframework.data.domain.Pageable;
import org.springframework.data.domain.Sort;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class OrderPaginationService {

    private final OrderRepository orderRepository;

    public OrderPaginationService(OrderRepository orderRepository) {
        this.orderRepository = orderRepository;
    }

    @Transactional(readOnly = true)
    public Page<OrderSummaryDto> getOrdersPage(int pageNumber, int pageSize, String sortBy, boolean ascending) {
        // Enforce maximum page size to prevent abuse
        int boundedPageSize = Math.min(pageSize, 100);

        Sort sort = ascending ? Sort.by(sortBy).ascending() : Sort.by(sortBy).descending();
        Pageable pageable = PageRequest.of(pageNumber, boundedPageSize, sort);

        // Executes: 
        // 1. SELECT * FROM orders ORDER BY ... LIMIT ? OFFSET ?
        // 2. SELECT COUNT(*) FROM orders
        Page<Order> orderPage = orderRepository.findAll(pageable);

        // Map entities to DTOs while preserving Page metadata
        return orderPage.map(order -> new OrderSummaryDto(
            order.getId(),
            order.getOrderNumber(),
            order.getTotalAmount(),
            order.getOrderDate()
        ));
    }
}
```

### 5.2 Count-Free Pagination with Slice for Infinite Scroll
```java
package com.example.jpa.service;

import com.example.jpa.domain.SocialPost;
import com.example.jpa.repository.SocialPostRepository;
import org.springframework.data.domain.PageRequest;
import org.springframework.data.domain.Pageable;
import org.springframework.data.domain.Slice;
import org.springframework.data.domain.Sort;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class FeedService {

    private final SocialPostRepository postRepository;

    public FeedService(SocialPostRepository postRepository) {
        this.postRepository = postRepository;
    }

    public record FeedResponse<T>(java.util.List<T> content, boolean hasNext, int pageNumber) {}

    @Transactional(readOnly = true)
    public FeedResponse<SocialPost> getTimelineFeed(int pageNumber, int pageSize) {
        Pageable pageable = PageRequest.of(
            pageNumber, 
            Math.min(pageSize, 50), 
            Sort.by(Sort.Direction.DESC, "createdAt")
        );

        // Slice queries LIMIT 51 when pageSize is 50.
        // If 51 rows return, hasNext() = true, and the 51st row is trimmed!
        // ZERO count query overhead!
        Slice<SocialPost> slice = postRepository.findByPublishedTrue(pageable);

        return new FeedResponse<>(
            slice.getContent(),
            slice.hasNext(),
            slice.getNumber()
        );
    }
}
```

### 5.3 Production Keyset (Seek/Cursor) Pagination
```java
package com.example.jpa.repository;

import com.example.jpa.domain.AuditLog;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.util.List;

public interface AuditLogRepository extends JpaRepository<AuditLog, Long> {

    /**
     * High-Performance Keyset Pagination:
     * O(1) Index lookup regardless of how many million records exist.
     */
    @Query("""
        SELECT a FROM AuditLog a
        WHERE (:lastSeenId IS NULL OR a.id > :lastSeenId)
        ORDER BY a.id ASC
        LIMIT :pageSize
        """)
    List<AuditLog> fetchNextKeysetBatch(
        @Param("lastSeenId") Long lastSeenId, 
        @Param("pageSize") int pageSize
    );
}
```

```java
package com.example.jpa.service;

import com.example.jpa.domain.AuditLog;
import com.example.jpa.repository.AuditLogRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Service
public class KeysetExportService {

    private final AuditLogRepository repository;

    public KeysetExportService(AuditLogRepository repository) {
        this.repository = repository;
    }

    public record KeysetPage<T>(List<T> items, Long nextCursor, boolean hasMore) {}

    @Transactional(readOnly = true)
    public KeysetPage<AuditLog> getLogsAfterCursor(Long cursor, int limit) {
        // Request limit + 1 to determine if more items exist
        List<AuditLog> results = repository.fetchNextKeysetBatch(cursor, limit + 1);

        boolean hasMore = results.size() > limit;
        List<AuditLog> pageItems = hasMore ? results.subList(0, limit) : results;

        Long nextCursor = pageItems.isEmpty() ? null : pageItems.get(pageItems.size() - 1).getId();

        return new KeysetPage<>(pageItems, nextCursor, hasMore);
    }
}
```

### 5.4 Anti-Pattern: Deep Offset Scanning on Million-Row Tables
```java
// =========================================================================
// ANTI-PATTERN: Standard PageRequest for deep background batch processing
// =========================================================================
@Service
public class BadBatchSyncService {

    private final TransactionRepository txRepo;

    public BadBatchSyncService(TransactionRepository txRepo) {
        this.txRepo = txRepo;
    }

    public void exportAllTransactions() {
        int page = 0;
        Page<Transaction> currentBatch;

        do {
            // DISASTER AT SCALE:
            // At page 10,000 with size 1,000, query issues:
            // SELECT * FROM transactions LIMIT 1000 OFFSET 10,000,000!
            // Plus an expensive: SELECT COUNT(*) FROM transactions!
            // Queries will time out, lock the database, and crash application threads!
            currentBatch = txRepo.findAll(PageRequest.of(page, 1000));
            processTransactions(currentBatch.getContent());
            page++;
        } while (currentBatch.hasNext());
    }
}

// =========================================================================
// PRODUCTION FIX: Keyset Pagination or Cursor Streaming
// =========================================================================
@Service
public class GoodBatchSyncService {

    private final TransactionRepository txRepo;

    public GoodBatchSyncService(TransactionRepository txRepo) {
        this.txRepo = txRepo;
    }

    public void exportAllTransactions() {
        Long lastId = 0L;
        List<Transaction> batch;

        do {
            // CONSTANT TIME: B-Tree index lookup: WHERE id > :lastId ORDER BY id ASC LIMIT 1000
            batch = txRepo.findNextBatchAfterId(lastId, 1000);
            if (!batch.isEmpty()) {
                processTransactions(batch);
                lastId = batch.get(batch.size() - 1).getId();
            }
        } while (!batch.isEmpty());
    }
}
```

### 5.5 Composite Keyset Pagination with Multiple Sort Columns
```java
package com.example.jpa.repository;

import com.example.jpa.domain.Customer;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.time.Instant;
import java.util.List;

public interface CustomerKeysetRepository extends JpaRepository<Customer, Long> {

    /**
     * Keyset pagination on (createdAt DESC, id DESC).
     * Uses row-value / tuple comparison or equivalent boolean logic:
     * WHERE (createdAt < :lastDate) OR (createdAt = :lastDate AND id < :lastId)
     */
    @Query("""
        SELECT c FROM Customer c
        WHERE (:lastDate IS NULL)
           OR (c.createdAt < :lastDate)
           OR (c.createdAt = :lastDate AND c.id < :lastId)
        ORDER BY c.createdAt DESC, c.id DESC
        LIMIT :limit
        """)
    List<Customer> findNextCustomersByDateAndId(
        @Param("lastDate") Instant lastDate,
        @Param("lastId") Long lastId,
        @Param("limit") int limit
    );
}
```

---

## 6. Best Practices

1. **Never Expose Unbounded Page Sizes**: Always cap client-requested page sizes using `Math.min(requestedSize, MAX_LIMIT)` in service or controller layers.
2. **Use `Slice<T>` for Infinite Scrolling**: If the UI does not require numbered pagination links (e.g., "Page 1, 2, 3 ... 50"), use `Slice` to eliminate the expensive `SELECT COUNT(*)` query.
3. **Use Keyset Pagination for Deep or Continuous Feeds**: For large datasets ($>100,000\text{ rows}$) or high-frequency polling, always use Keyset (Cursor) pagination.
4. **Index All Sorting Columns**: Any column passed to `Sort.by(...)` must have a corresponding database B-Tree index; otherwise, the database performs an expensive in-memory filesort.
5. **Always Include Unique Tie-Breaker in Sorts**: When sorting by non-unique columns (like `createdAt`), always append the primary key as a secondary sort (`createdAt DESC, id DESC`) to guarantee deterministic pagination without skipped rows.
6. **Use Custom Count Queries for Complex Joins**: When using `@Query` with complex joins, define an explicit `countQuery` attribute to avoid generating massive join trees just to count rows.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `Page<T>` on High-Volume Endpoints** | Triggers redundant `SELECT COUNT(*)` on every request, creating massive CPU and lock overhead. | Migrate to `Slice<T>` or Keyset pagination. |
| **Deep Offset Pagination (`OFFSET > 10,000`)** | The database scans and discards thousands of rows, causing high query latency. | Implement Keyset (Seek) pagination using indexed columns. |
| **Non-Deterministic Sorting** | Sorting on a column with duplicate values without a primary key tie-breaker causes rows to jump pages or be skipped. | Always append a unique tie-breaker: `ORDER BY status ASC, id ASC`. |
| **Unindexed Sorting Columns** | Sorting by an unindexed column forces the database to sort results on disk or in memory (Temporary Filesort). | Add a database index matching the exact query sorting order. |
| **Page-1 Based Client Inputs** | `PageRequest.of(page, size)` expects zero-based indexing (`page = 0`); client requests sending `page = 1` skip the first page. | Normalize client inputs: `int pageIndex = Math.max(0, clientPage - 1)`. |
| **Pagination with Collection `JOIN FETCH`** | Hibernate applies pagination in Java memory instead of SQL, threatening memory exhaustion. | Remove `JOIN FETCH` and use `@BatchSize` or two-step querying. |

---

## 8. Performance & Security Notes

### Performance
- **Keyset Index Seek**: Keyset queries execute in $O(1)$ time because the database traverses the B-Tree index directly to the cursor value rather than scanning preceding rows.
- **Count Query Elimination**: Eliminating `COUNT(*)` using `Slice` reduces database query latency by 50% on average, as only a single SQL statement is executed.

### Security
- **Sort Parameter Whitelisting**: Never allow arbitrary client strings to be passed directly into `Sort.by(clientSortProperty)`. Attackers can probe internal entity property names or trigger property evaluation errors. Always whitelist allowable sort fields.
- **Resource Exhaustion Prevention**: Enforce strict caps on `size` (e.g., maximum 100 rows per request) to prevent malicious actors from requesting `?size=1000000` and crashing the application.

---

## 9. Comparison / When to Use What

| Feature | Page<T> | Slice<T> | Keyset (Seek) Pagination |
| :--- | :--- | :--- | :--- |
| **Query Count** | **2 (Data + COUNT)** | 1 (Data with limit + 1) | 1 (Data with limit + 1) |
| **Total Pages / Items** | **Available** | Not Available | Not Available |
| **Deep Page Latency** | $O(N)$ (Slow at high offset) | $O(N)$ (Slow at high offset) | **$O(1)$ (Constant Time)** |
| **Data Drift Resistance** | Poor (Items shift on insert) | Poor (Items shift on insert) | **Immune (Cursor-anchored)** |
| **Best Used For** | Numbered UI grids (Admin) | Infinite scroll feeds | Large streams, exports, high throughput |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental performance difference between <code>Page<T></code> and <code>Slice<T></code>?</strong></summary>

`Page<T>` executes two queries: the actual data query with `LIMIT` and `OFFSET`, and an auxiliary `SELECT COUNT(*)` query to calculate the total number of pages and elements. On large tables, the count query can be extremely slow. `Slice<T>` only executes the data query with a limit of `pageSize + 1`. If $N+1$ items are returned, `hasNext()` is set to `true`, completely avoiding the count query.
</details>

<details>
<summary><strong>2. Why does offset pagination degrade significantly as the offset increases?</strong></summary>

Under SQL offset pagination (`LIMIT 10 OFFSET 500000`), the database engine cannot simply jump to row 500,001. It must read, traverse, and discard all 500,000 preceding rows from the index or storage engine before returning the 10 requested rows. As the offset increases, query execution time increases linearly ($O(N)$), causing severe disk I/O and CPU contention.
</details>

<details>
<summary><strong>3. How does Keyset (Seek) pagination achieve $O(1)$ performance?</strong></summary>

Keyset pagination eliminates the `OFFSET` clause entirely. Instead, it uses an indexed condition such as `WHERE id > :lastSeenId ORDER BY id ASC LIMIT 10`. The database engine performs a direct B-Tree index lookup to locate `:lastSeenId` in $O(\log K)$ time and immediately reads the next 10 consecutive index entries, yielding constant-time performance regardless of how deep into the dataset the client navigates.
</details>

<details>
<summary><strong>4. What is "data drift" in offset pagination and how does Keyset pagination prevent it?</strong></summary>

If a new row is inserted at the top of a table while a user is paginating using offsets, all existing records shift down by one position. When the user requests Page 2 (`OFFSET 10`), the last record from Page 1 appears again as the first record of Page 2. Keyset pagination anchors the query to the specific key of the last seen record, making it completely immune to shifts caused by concurrent insertions or deletions.
</details>

<details>
<summary><strong>5. How does Spring Boot auto-configure Web pagination support?</strong></summary>

Spring Boot auto-configures `PageableHandlerMethodArgumentResolver`. This allows Spring MVC controller methods to accept `Pageable pageable` directly as an argument, automatically binding query parameters such as `?page=0&size=20&sort=createdAt,desc`.
</details>

<details>
<summary><strong>6. Why must you always include a tie-breaker column when sorting for pagination?</strong></summary>

If the sorting column contains non-unique values (e.g., sorting by `status` or `creation_date` where multiple rows share the same timestamp), the database engine does not guarantee a deterministic ordering among tied records. Between consecutive paginated queries, tied rows can return in arbitrary order, causing duplicate or missed records across pages. Appending a unique primary key (`ORDER BY date DESC, id DESC`) guarantees determinism.
</details>

<details>
<summary><strong>7. How can you customize the count query in a Spring Data JPA <code>@Query</code>?</strong></summary>

By specifying the `countQuery` attribute:
```java
@Query(
    value = "SELECT o FROM Order o JOIN FETCH o.customer c WHERE c.active = true",
    countQuery = "SELECT COUNT(o) FROM Order o WHERE o.customer.active = true"
)
Page<Order> findActiveCustomerOrders(Pageable pageable);
```
This avoids executing unnecessary `JOIN FETCH` operations during the count query.
</details>

<details>
<summary><strong>8. What is the limitation of Keyset pagination compared to offset pagination?</strong></summary>

Keyset pagination does not support random page access (e.g., jumping directly to "Page 45"). Clients can only navigate sequentially forward (or backward) from known cursor positions. It also requires the sorting columns to be non-nullable and indexed.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Pageable Controller Endpoint</strong></summary>

**Task**: Implement a controller endpoint returning a `Page<ProductDto>` accepting `Pageable` with a default size of 20 and default sort by `name` ascending.

```java
@GetMapping("/products")
public Page<ProductDto> getProducts(
    @PageableDefault(size = 20, sort = "name", direction = Sort.Direction.ASC) Pageable pageable
) {
    return productService.getProducts(pageable);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Slice Repository Declaration</strong></summary>

**Task**: Declare a repository query in `CommentRepository` that returns a `Slice<Comment>` for a given `postId` ordered by `createdAt` descending.

```java
public interface CommentRepository extends JpaRepository<Comment, Long> {
    Slice<Comment> findByPostId(Long postId, Pageable pageable);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Page Size Enforcement Validator</strong></summary>

**Task**: Write a utility method that sanitizes incoming `Pageable` requests to ensure that `pageSize` never exceeds 50 and `pageIndex` is never negative.

```java
public static Pageable sanitizePageable(Pageable pageable, int maxLimit) {
    int page = Math.max(0, pageable.getPageNumber());
    int size = Math.min(maxLimit, Math.max(1, pageable.getPageSize()));
    return PageRequest.of(page, size, pageable.getSort());
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Count Query for Complex Entity Join</strong></summary>

**Task**: Write a `@Query` method that fetches users joined with their departments, providing an optimized count query that counts users without joining child tables.

```java
@Query(
    value = "SELECT u FROM User u LEFT JOIN FETCH u.department WHERE u.active = true",
    countQuery = "SELECT COUNT(u) FROM User u WHERE u.active = true"
)
Page<User> findActiveUsersWithDepartment(Pageable pageable);
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Keyset Cursor Repository and DTO Pagination</strong></summary>

**Task**: Implement a repository query and service method for Keyset pagination over a `Transaction` entity using `id` as the cursor, returning a cursor-wrapped DTO.

```java
public interface TransactionRepository extends JpaRepository<Transaction, Long> {
    @Query("SELECT t FROM Transaction t WHERE (:cursor IS NULL OR t.id > :cursor) ORDER BY t.id ASC LIMIT :limit")
    List<Transaction> findNextCursorBatch(@Param("cursor") Long cursor, @Param("limit") int limit);
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Multi-Column Keyset Seek Query Implementation</strong></summary>

**Task**: Implement a repository query for Keyset pagination sorting by `(score DESC, id DESC)` handling compound cursor values `(lastScore, lastId)`.

```java
@Query("""
    SELECT p FROM Player p
    WHERE (:lastScore IS NULL)
       OR (p.score < :lastScore)
       OR (p.score = :lastScore AND p.id < :lastId)
    ORDER BY p.score DESC, p.id DESC
    LIMIT :limit
    """)
List<Player> findLeaderboardKeyset(
    @Param("lastScore") Integer lastScore,
    @Param("lastId") Long lastId,
    @Param("limit") int limit
);
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Page<T>:      Two queries (SELECT + COUNT). Use ONLY when UI requires numbered pages. |
| Slice<T>:     One query (LIMIT size + 1). Use for mobile feeds and infinite scrolls.   |
| Keyset/Seek:  WHERE id > :lastId ORDER BY id LIMIT N. Constant O(1) time!             |
| Rule 1:       Always enforce a maximum page size (e.g. Math.min(size, 100))           |
| Rule 2:       Always append a unique primary key tie-breaker when sorting by dates/status|
| Rule 3:       Never use OFFSET pagination for deep exports (>10,000 rows)             |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Data JPA Pagination Documentation](https://docs.spring.io/spring-data/jpa/reference/repositories/query-methods-details.html#repositories.special-parameters)
- [Use The Index, Luke: Paging Through Huge Result Sets](https://use-the-index-luke.com/no-offset)
- [Vlad Mihalcea: Keyset Pagination with Spring Data](https://vladmihalcea.com/keyset-pagination-with-spring-data-jpa/)
- [PostgreSQL Performance Tuning for LIMIT and OFFSET](https://www.postgresql.org/docs/current/queries-limit.html)
