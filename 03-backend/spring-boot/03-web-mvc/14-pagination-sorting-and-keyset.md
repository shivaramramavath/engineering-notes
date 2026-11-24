# Pagination, Sorting & Keyset (Cursor) Pagination in Spring Boot

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Spring Data JPA / JDBC, SQL Execution Plans, Spring MVC Controllers |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Spring Data 3.4, PostgreSQL 16, OpenJDK 21.0.2 |

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
9. [Comparison: Offset vs Keyset (Cursor) Pagination](#9-comparison-offset-vs-keyset-cursor-pagination)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In enterprise data stores containing millions or billions of rows, returning all records in a single HTTP request is catastrophic. It exhausts database connection memory, saturates network bandwidth, and crashes client browsers.

### The Problem
Naive pagination using SQL `OFFSET` causes critical production bottlenecks:
1. **The Offset Performance Cliff**: `SELECT * FROM orders ORDER BY id LIMIT 20 OFFSET 1000000` forces the database engine to scan and discard 1,000,000 index entries or table pages, taking seconds or minutes of CPU time.
2. **Result Drift**: If records are inserted or deleted while a user navigates between pages, items duplicate or are skipped entirely across page boundaries.
3. **Expensive Total Counts**: Executing `SELECT COUNT(*)` on huge tables blocks database threads and prevents caching.

### The Solution: Offset & Keyset (Cursor) Pagination in Spring
Spring provides two pagination mechanisms:
1. **Standard Offset Pagination**: Built into Spring Data via `Pageable`, `Page<T>`, and `Slice<T>` for UI tables requiring random page jumping (`page=3`).
2. **Keyset / Cursor Pagination**: Using index-seek predicates (`WHERE id > :cursor ORDER BY id ASC LIMIT 20`) via Spring Data 3.1+ `ScrollPosition` and `Window<T>`, providing $O(1)$ query time regardless of dataset depth.

---

## 2. Core Concepts

### Mental Model: Offset vs Keyset (Cursor) Pagination
```
OFFSET-BASED PAGINATION:
Query: SELECT * FROM orders ORDER BY id LIMIT 10 OFFSET 500000;
Database Engine Execution:
[ 0 .. 499,999 ] ──► Reads & Discards 500,000 rows (High I/O & CPU!)
[ 500,000 .. 500,009 ] ──► Returns 10 rows
Complexity: O(OFFSET + LIMIT) -> Degrades linearly as offset increases!

───────────────────────────────────────────────────────────────────

KEYSET (CURSOR) PAGINATION:
Query: SELECT * FROM orders WHERE id > '018f2b3c-4d5e' ORDER BY id ASC LIMIT 10;
Database Engine Execution:
B-Tree Index Seek directly to '018f2b3c-4d5e' ──► O(log N)
Reads next 10 consecutive index entries ──► O(LIMIT)
Complexity: O(1) relative to total table size! Consistently < 5ms.
```

### Invariants
1. **Keyset Determinism Invariant**: Keyset pagination requires an `ORDER BY` clause consisting of a **strictly unique, indexed column** (or combination of columns, e.g. `(created_at, id)`). Non-unique sort keys cause missed rows.
2. **Slice vs Page**: `Page<T>` executes a secondary `SELECT COUNT(*)` query to compute total elements and pages. `Slice<T>` fetches `LIMIT + 1` rows to determine whether a next page exists without ever running `COUNT(*)`.
3. **Cursor Opaqueness**: Keyset cursors should be encoded as opaque base64 strings so clients cannot tamper with internal column representations.

---

## 3. How It Works Internally

### Spring MVC Parameter Resolution: `PageableHandlerMethodArgumentResolver`
1. When a controller method declares `Pageable pageable`, Spring's `PageableHandlerMethodArgumentResolver` intercepts request parameters (`?page=2&size=25&sort=name,desc`).
2. It constructs a `PageRequest` object with configured limits (e.g., maximum allowed page size).
3. The repository executes the query:
   - If returning `Page<T>`: Runs main query + count query, returning `PageImpl`.
   - If returning `Slice<T>`: Appends `+1` to limit, checking if additional elements exist.
   - If returning `Window<T>`: Leverages `ScrollPosition` for cursor-based seek navigation.

---

## 4. Syntax & Configuration

### Standard `application.yaml` Pagination Defaults
```yaml
spring:
  data:
    web:
      pageable:
        default-page-size: 20
        max-page-size: 100 # Prevents clients from requesting ?size=1000000
        page-parameter-name: page
        size-parameter-name: size
        one-indexed-parameters: false # 0-indexed: page 0 is first page
      sort:
        sort-parameter-name: sort
```

---

## 5. Practical Examples

### Example 1: Standard Offset Pagination with `Page<T>` and Records

```java
package com.example.web.pagination;

import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.data.domain.Sort;
import org.springframework.data.web.PageableDefault;
import org.springframework.data.web.SortDefault;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.math.BigDecimal;
import java.util.UUID;

public record TransactionSummary(UUID id, String description, BigDecimal amount) {}

@RestController
@RequestMapping("/api/v1/transactions")
public class TransactionOffsetController {

    private final TransactionService transactionService;

    public TransactionOffsetController(TransactionService transactionService) {
        this.transactionService = transactionService;
    }

    @GetMapping
    public Page<TransactionSummary> getTransactions(
            @PageableDefault(size = 25)
            @SortDefault(sort = "createdAt", direction = Sort.Direction.DESC)
            Pageable pageable) {
        
        // Pageable safely encapsulates ?page=0&size=25&sort=createdAt,desc
        return transactionService.findAll(pageable);
    }
}
```

### Example 2: High-Performance `Slice<T>` (Eliminating Expensive `COUNT(*)` Queries)

When infinite scrolling on mobile apps does not require showing total page count ("Page 1 of 500"), using `Slice<T>` eliminates the slow `COUNT(*)` query entirely.

```java
package com.example.web.pagination;

import org.springframework.data.domain.Pageable;
import org.springframework.data.domain.Slice;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

public record FeedItem(Long id, String headline) {}

@RestController
@RequestMapping("/api/v1/feed")
public class InfiniteScrollFeedController {

    private final FeedService feedService;

    public InfiniteScrollFeedController(FeedService feedService) {
        this.feedService = feedService;
    }

    @GetMapping
    public Slice<FeedItem> getSocialFeed(Pageable pageable) {
        // Only executes: SELECT * FROM feed LIMIT 21 (for size=20)
        // Zero COUNT(*) query executed!
        return feedService.getFeedSlice(pageable);
    }
}
```

### Example 3: Edge Case — Keyset (Cursor) Pagination with Spring Data `ScrollPosition`

```java
package com.example.web.pagination;

import org.springframework.data.domain.ScrollPosition;
import org.springframework.data.domain.Window;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.List;
import java.util.Map;

public record CustomerAuditLog(Long id, String action, String timestamp) {}
public record KeysetResponse<T>(List<T> data, String nextCursor, boolean hasMore) {}

@RestController
@RequestMapping("/api/v1/audit-logs")
public class KeysetCursorController {

    private final AuditRepository auditRepository;

    public KeysetCursorController(AuditRepository auditRepository) {
        this.auditRepository = auditRepository;
    }

    @GetMapping
    public KeysetResponse<CustomerAuditLog> getAuditLogs(
            @RequestParam(required = false) String cursor,
            @RequestParam(defaultValue = "50") int limit) {

        ScrollPosition scrollPosition = ScrollPosition.keyset();
        if (cursor != null && !cursor.isBlank()) {
            String decodedId = new String(Base64.getUrlDecoder().decode(cursor), StandardCharsets.UTF_8);
            // Reconstruct keyset position from opaque cursor token
            scrollPosition = ScrollPosition.forward(Map.of("id", Long.parseLong(decodedId)));
        }

        // Window returns elements + enables extracting the next keyset position
        Window<CustomerAuditLog> window = auditRepository.findTop50ByOrderByIdAsc(scrollPosition);

        String nextCursor = null;
        if (!window.isEmpty() && window.hasNext()) {
            CustomerAuditLog lastItem = window.getContent().get(window.getContent().size() - 1);
            nextCursor = Base64.getUrlEncoder().encodeToString(String.valueOf(lastItem.id()).getBytes(StandardCharsets.UTF_8));
        }

        return new KeysetResponse<>(window.getContent(), nextCursor, window.hasNext());
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Unbounded Page Size and Deep Offset
```java
// ANTI-PATTERN: Client can request size=1000000 and page=500000
@GetMapping("/bad-orders")
public List<Order> getOrdersBad(@RequestParam int page, @RequestParam int size) {
    // Uncapped size causes OutOfMemoryError; deep offset crashes DB CPU
    PageRequest pr = PageRequest.of(page, size);
    return orderRepository.findAll(pr).getContent();
}
```

#### Production Fix: Capped Pageable with Global Limits
```yaml
# application.yaml: Enforce hard ceiling
spring:
  data:
    web:
      pageable:
        max-page-size: 100 # Client requesting ?size=10000 gets 100 max
```

```java
// PRODUCTION FIX: Use Spring Pageable with default and max constraints
@GetMapping("/good-orders")
public Page<OrderDto> getOrdersGood(
        @PageableDefault(size = 20) Pageable pageable) {
    return orderService.findOrders(pageable);
}
```

### Example 5: High-Performance Composite Keyset Pagination (Timestamp + ID)

When sorting by a non-unique field (e.g. `created_at`), you must break ties with a unique column (`id`) to ensure deterministic pagination.

```sql
-- Optimal PostgreSQL index supporting composite keyset query
CREATE INDEX idx_orders_created_at_id ON orders (created_at DESC, id DESC);

-- Query executed by repository:
SELECT * FROM orders
WHERE (created_at, id) < (:lastCreatedAt, :lastId)
ORDER BY created_at DESC, id DESC
LIMIT 20;
```

```java
package com.example.web.pagination;

import org.springframework.stereotype.Repository;

import java.time.Instant;
import java.util.List;
import java.util.UUID;

public record OrderCursor(Instant createdAt, UUID id) {}

@Repository
public interface OrderCompositeKeysetRepository {
    // Spring Data JPA query leveraging composite tuple comparison
    List<OrderDto> findNextPage(Instant createdAt, UUID id, int limit);
}
```

---

## 6. Best Practices

1. **Always Cap `max-page-size`**: Enforce `spring.data.web.pageable.max-page-size: 100` so malicious or naive clients cannot execute `?size=10000000` to trigger out-of-memory errors.
2. **Use `Slice<T>` for Infinite Scrolling**: If the client doesn't need "Page X of Y" or total record count, use `Slice<T>` to avoid running expensive `SELECT COUNT(*)` queries.
3. **Use Keyset Pagination for Deep Datasets**: For datasets exceeding 100,000 rows (e.g. audit logs, event feeds, message queues), use Keyset/Cursor pagination for constant $O(1)$ query latency.
4. **Always Back Sort Columns with Database Indexes**: Sorting on an unindexed column forces the database to perform an in-memory or on-disk external sort, degrading performance.
5. **Opaque Cursor Tokens**: Encode keyset markers as base64 tokens (`cursor=ZXlK...`) so clients treat them as black-box continuation tokens.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Uncapped Page Size (`?size=50000`)** | Pulls thousands of entities into JVM heap, triggering GC stalls or `OutOfMemoryError`. | Configure `max-page-size: 100` in YAML or validate `size <= 100`. |
| **Using Deep OFFSET in High-Volume APIs** | Query latency grows linearly with offset (`OFFSET 500000`), tying up database connections. | Switch to Keyset (Cursor) pagination using index seeks. |
| **Non-Unique Sort Keys in Keyset Pagination** | Rows sharing identical sort values (e.g. identical `created_at`) are skipped or duplicated. | Append a tie-breaking unique column (`ORDER BY created_at DESC, id DESC`). |
| **Executing `Page<T>` on Huge Partitioned Tables** | `COUNT(*)` performs a full index or table scan across partitions, taking seconds to execute. | Return `Slice<T>` or Keyset `Window<T>` to skip the count query. |
| **Sorting on Unindexed Columns** | Triggers expensive database sort operations (`Sort` / `Temp Table` in EXPLAIN plan). | Only permit sorting on columns supported by B-Tree indexes. |
| **Offset Drift During Pagination** | Inserting a row on page 1 shifts all subsequent records down, showing duplicate items on page 2. | Use Keyset pagination; newly inserted records do not shift the cursor position. |

---

## 8. Performance & Security Considerations

### SQL Injection via Sort Parameters
Spring Data's `Sort` abstraction protects against SQL injection when using standard property names. However, if using native SQL queries with dynamic string concatenation (`"ORDER BY " + sortParam`), an attacker can execute SQL injection. Always pass `Sort` or `Pageable` directly into Spring Data repository methods.

### Count Query Caching
If your UI strictly requires total counts on huge tables:
1. Cache the total count in Redis with a 5-minute TTL.
2. Maintain approximate row counts via PostgreSQL's `pg_class.reltuples` or database statistics.

---

## 9. Comparison: Offset vs Keyset (Cursor) Pagination

| Feature | Standard Offset Pagination (`Pageable`) | Keyset (Cursor) Pagination (`ScrollPosition`) |
| :--- | :--- | :--- |
| **Query Mechanism** | `LIMIT x OFFSET y` | `WHERE id > :cursor LIMIT x` |
| **Query Complexity** | $O(\text{offset} + \text{limit})$ (Degrades over depth) | $O(1)$ (Constant via B-Tree index seek) |
| **Total Count (`COUNT(*)`)** | Yes (`Page<T>`) | No |
| **Random Page Jumping** | Yes (e.g., jump directly to page 42) | No (Sequential navigation: next/previous only) |
| **Result Drift Resistance** | Poor (Insertions shift items between pages) | Excellent (Cursor position is immune to shifts) |
| **Best Used For** | Admin dashboards, search results (<1000 items) | High-volume APIs, mobile feeds, audit logs, ETL |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why does `SELECT * FROM orders ORDER BY id LIMIT 20 OFFSET 1000000` perform poorly in relational databases?</strong></summary>

**Answer:**
Relational databases cannot instantly jump to row 1,000,000. Even with an index on `id`, the database must traverse the B-Tree index, count and discard the first 1,000,000 entries, and then read the 20 target rows. If columns are not in a covering index, it may also perform 1,000,000 random disk page lookups to retrieve table tuples, generating massive disk I/O and CPU utilization.
</details>

<details>
<summary><strong>Q2: What is the architectural difference between `Page<T>` and `Slice<T>` in Spring Data?</strong></summary>

**Answer:**
- `Page<T>`: Executes **two** SQL queries: the data query (`SELECT ... LIMIT x OFFSET y`) and an additional count query (`SELECT COUNT(*) FROM ...`). It knows the total number of elements and total pages.
- `Slice<T>`: Executes only **one** SQL query, fetching `limit + 1` records. If the result contains `limit + 1` items, Spring knows a next slice exists without ever running a count query.
</details>

<details>
<summary><strong>Q3: How does Keyset (Cursor) Pagination achieve $O(1)$ performance?</strong></summary>

**Answer:**
Keyset pagination replaces `OFFSET` with an indexed `WHERE` filter (e.g. `WHERE id > :cursor`). The database engine performs an $O(\log N)$ B-Tree index lookup directly to the cursor position and sequentially reads the next `LIMIT` entries. Regardless of whether you are reading the 1st page or the 10,000,000th page, execution time remains identical (< 5ms).
</details>

<details>
<summary><strong>Q4: Why must the sort criteria in Keyset pagination be strictly unique?</strong></summary>

**Answer:**
If the sort column contains duplicate values (e.g. sorting by `status` or non-unique `created_at`), the filter `WHERE created_at > :cursor` will skip all subsequent rows sharing the exact same timestamp as the cursor. To prevent missed rows, you must combine the non-unique column with a unique tie-breaker (e.g. `ORDER BY created_at DESC, id DESC`).
</details>

<details>
<summary><strong>Q5: What is "Result Drift" in pagination?</strong></summary>

**Answer:**
Result drift occurs when the underlying dataset is modified while a user paginates. For example, if a user views page 1 and a new record is inserted at the top of the table, all existing records shift down by one. When the user navigates to page 2 using `OFFSET 20`, the last record from page 1 is shown again as the first item on page 2. Keyset pagination prevents drift because the cursor anchors to a specific record value, not an offset position.
</details>

<details>
<summary><strong>Q6: How do you customize default pagination size globally in Spring Boot?</strong></summary>

**Answer:**
Configure `spring.data.web.pageable.default-page-size` and `spring.data.web.pageable.max-page-size` in `application.yaml`, or register a custom `PageableHandlerMethodArgumentResolverCustomizer` bean.
</details>

<details>
<summary><strong>Q7: What is Spring Data's `Window<T>` interface introduced in Spring Data 3.1?</strong></summary>

**Answer:**
`Window<T>` is the abstraction for scrollable/keyset results. It holds the current batch of items and provides methods like `positionAt(index)` to extract `ScrollPosition` markers, enabling seamless forward and backward cursor-based scrolling across JPA and NoSQL repositories.
</details>

<details>
<summary><strong>Q8: Can you combine sorting and pagination using multiple properties in Spring MVC?</strong></summary>

**Answer:**
Yes. A client can supply multiple sort query parameters: `?page=0&size=20&sort=status,asc&sort=createdAt,desc`. Spring's `PageableHandlerMethodArgumentResolver` binds these into a composite `Sort` object containing multiple `Order` elements.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Configure Pageable Defaults via Annotation</strong></summary>

**Task**: Annotate a controller endpoint parameter so that requests without pagination default to page 0, size 15, sorted by `id` ascending.

```java
// Solution:
@GetMapping("/api/items")
public List<Item> getItems(
        @PageableDefault(page = 0, size = 15)
        @SortDefault(sort = "id", direction = Sort.Direction.ASC)
        Pageable pageable) {
    return itemRepository.findAll(pageable).getContent();
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Return `Slice<T>` to Avoid Count Query</strong></summary>

**Task**: Write a repository method and controller endpoint returning a `Slice<Article>` for infinite scroll.

```java
// Solution:
public interface ArticleRepository extends Repository<Article, Long> {
    Slice<Article> findByStatus(String status, Pageable pageable);
}

@RestController
public class ArticleController {
    @Autowired ArticleRepository repo;

    @GetMapping("/api/articles")
    public Slice<Article> getArticles(Pageable pageable) {
        return repo.findByStatus("PUBLISHED", pageable);
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Safe Base64 Cursor Serializer</strong></summary>

**Task**: Implement helper methods `String encodeCursor(Long id)` and `Long decodeCursor(String cursor)` to encode and decode keyset cursor tokens safely.

```java
// Solution:
public class CursorUtils {
    public static String encodeCursor(Long id) {
        if (id == null) return null;
        return Base64.getUrlEncoder().withoutPadding().encodeToString(String.valueOf(id).getBytes(StandardCharsets.UTF_8));
    }

    public static Long decodeCursor(String cursor) {
        if (cursor == null || cursor.isBlank()) return null;
        try {
            String decoded = new String(Base64.getUrlDecoder().decode(cursor), StandardCharsets.UTF_8);
            return Long.parseLong(decoded);
        } catch (IllegalArgumentException e) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Malformed cursor token");
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Page Limit Validator</strong></summary>

**Task**: Write a custom validator or pre-handle check ensuring that incoming `Pageable.getPageSize()` does not exceed 50, throwing `ResponseStatusException(BAD_REQUEST)` if exceeded.

```java
// Solution:
@GetMapping("/api/enforced")
public Page<Data> getData(Pageable pageable) {
    if (pageable.getPageSize() > 50) {
        throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Page size cannot exceed 50");
    }
    return service.findAll(pageable);
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Dual-Column Keyset Paginator Repository</strong></summary>

**Task**: Write a native SQL repository query implementing composite keyset pagination on `(created_at, id)` with descending order, returning exactly `limit + 1` items to detect `hasNext`.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Keyset MethodArgumentResolver</strong></summary>

**Task**: Build a `HandlerMethodArgumentResolver` that automatically binds incoming HTTP request parameter `?cursor=...` into a typed `CursorRequest<Long>` record for controller methods.
</details>

---

## 12. Summary & Quick Reference

- **`Page<T>`**: Executes count query; use for traditional page-number navigation.
- **`Slice<T>`**: No count query; use for infinite scrolling feeds.
- **Keyset / Cursor**: $O(1)$ index seek (`WHERE id > :cursor`); use for massive datasets (>100k rows).
- **Security Cap**: Always set `spring.data.web.pageable.max-page-size: 100`.
- **Determinism**: Keyset sorting must always include a unique tie-breaking column.

---

## 13. Further Reading

- [Spring Data Reference: Pagination and Sorting](https://docs.spring.io/spring-data/commons/reference/repositories/query-methods-details.html#repositories.special-parameters)
- [Spring Data Commons: Scrolling and Keyset Pagination](https://docs.spring.io/spring-data/commons/reference/repositories/scrolling.html)
- [Use The Index, Luke: Paging Through Huge Result Sets](https://use-the-index-luke.com/no-offset)
- [PostgreSQL Indexing Best Practices for Pagination](https://www.postgresql.org/docs/current/indexes-ordering.html)
