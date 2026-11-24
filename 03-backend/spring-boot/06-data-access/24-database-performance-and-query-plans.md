# Database Performance, Query Execution Plans & Index Optimization

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | `01-jdbc-template-and-jdbc-client.md`, `06-jpql-native-and-specifications.md`, `08-pagination-and-sorting.md`, B-Tree storage internals |
| **Estimated Reading Time** | 32 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | PostgreSQL 16 `EXPLAIN (ANALYZE, BUFFERS)`, Hibernate 6.6.x |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Diagnosing Slow Queries with EXPLAIN (ANALYZE, BUFFERS)](#51-diagnosing-slow-queries-with-explain-analyze-buffers)
   - [5.2 Transforming a Seq Scan to an Index Only Scan via Covering Indexes](#52-transforming-a-seq-scan-to-an-index-only-scan-via-covering-indexes)
   - [5.3 Composite Index Tuning: Equality First, Range Second Rule](#53-composite-index-tuning-equality-first-range-second-rule)
   - [5.4 Anti-Pattern: Function Wrapping and Implicit Type Conversions](#54-anti-pattern-function-wrapping-and-implicit-type-conversions)
   - [5.5 Monitoring Query Performance with DataSource-Proxy and Hibernate Statistics](#55-monitoring-query-performance-with-datasource-proxy-and-hibernate-statistics)
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
In production backend engineering, slow database queries are the primary cause of latency degradation, thread pool exhaustion, and server outages. Developers frequently assume that adding an index to a column automatically fixes query performance, only to discover that the database continues to perform expensive full table scans. Without understanding the database query planner, cost estimations, join algorithms, and execution plan operators, query optimization is reduced to blind guesswork.

### Historical Evolution
Modern relational databases (PostgreSQL, MySQL, Oracle) utilize sophisticated **Cost-Based Query Optimizers (CBO)**. The optimizer analyzes table statistics, page distributions, and index selectivity to calculate cost estimates for various execution paths. In PostgreSQL, executing `EXPLAIN (ANALYZE, BUFFERS)` reveals the exact execution tree chosen by the engine, the actual execution time per node, and the physical disk buffer hits versus disk reads.

### Real-World Relevance
Optimizing a single high-frequency query from a Sequential Scan to an Index Only Scan can reduce query execution time from 850ms to 0.4ms and eliminate gigabytes of unnecessary disk I/O, allowing database clusters to scale effortlessly without requiring hardware upgrades.

---

## 2. Core Concepts

### Access Methods (PostgreSQL)
1. **Sequential Scan (`Seq Scan`)**: Reads every data block (page) of the table sequentially from disk/RAM. Optimal for small tables or when a query selects $>20\text{--}30\%$ of the entire table.
2. **Index Scan (`Index Scan`)**: Traverses the B-Tree index to locate row pointers (TIDs: Tuple IDs), then fetches the corresponding physical table heap pages.
3. **Index Only Scan (`Index Only Scan`)**: Traverses the B-Tree index to retrieve both the search criteria and all projected columns *without touching the table heap pages at all* (requires a **Covering Index**). Maximum possible performance!
4. **Bitmap Index Scan / Bitmap Heap Scan**: Scans the index to build an in-memory bitmap of target table pages, sorts the page references by physical disk location, and reads the table pages sequentially to minimize random disk head movements.

### Join Algorithms
- **Nested Loop Join**: Iterates over an outer relation and probes an index on the inner relation for each row. Ideal for small datasets with indexed joins.
- **Hash Join**: Builds an in-memory hash table of the smaller relation, then scans the larger relation to find matches. Ideal for large, unindexed equality joins.
- **Merge Join**: Sorts both relations by the join key (if not already sorted by an index) and merges them in a single pass. Ideal for large, sorted datasets.

### Composite Index Ordering Rule: Equality First, Range Second
When indexing multiple columns (`WHERE status = 'ACTIVE' AND created_at >= '2026-01-01'`):
- **Equality columns** (`status`) must come **FIRST** in the composite index definition.
- **Range / Inequality columns** (`created_at`) must come **LAST**.
Once a B-Tree search encounters a range inequality, it cannot use subsequent index columns for binary searching!

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         PostgreSQL Query Planner Pipeline                          |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  1. SQL Statement Parsed: SELECT name FROM users WHERE age > 30 AND status = 'A'   |
|                                                                                    |
|  2. Query Optimizer (Cost-Based Optimizer - CBO):                                  |
|     - Inspects pg_statistic / pg_stats tables (data distribution, null fraction)   |
|     - Estimates candidate path costs:                                              |
|       * Path A (Seq Scan): Cost = 4500.00                                          |
|       * Path B (Bitmap Scan on idx_age): Cost = 1200.00                            |
|       * Path C (Index Only Scan on idx_status_age_name): Cost = 12.50              |
|                                                                                    |
|  3. Execution Engine executes winning Path C:                                      |
|     +----------------------------------------------------------------------------+ |
|     | Index Only Scan using idx_status_age_name on users (Cost=0.42..12.50)      | |
|     | Index Cond: ((status = 'A') AND (age > 30))                                | |
|     | Buffers: shared hit=4 read=0 (Fetched directly from OS buffer cache!)      | |
|     +----------------------------------------------------------------------------+ |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Running EXPLAIN in PostgreSQL
```sql
-- Comprehensive execution plan with runtime metrics and buffer hit statistics
EXPLAIN (ANALYZE, BUFFERS, COSTS, VERBOSE, FORMAT TEXT)
SELECT id, order_number, total_amount 
FROM orders 
WHERE customer_id = 101 AND status = 'COMPLETED';
```

---

## 5. Practical Examples

### 5.1 Diagnosing Slow Queries with EXPLAIN (ANALYZE, BUFFERS)
```sql
-- Query under test:
SELECT * FROM orders WHERE customer_id = 45000 AND order_date >= '2026-01-01';

-- Output of EXPLAIN (ANALYZE, BUFFERS):
Seq Scan on orders  (cost=0.00..18450.00 rows=12 width=72) (actual time=42.120..85.340 rows=8 loops=1)
  Filter: ((customer_id = 45000) AND (order_date >= '2026-01-01'::date))
  Rows Removed by Filter: 999992
  Buffers: shared hit=1200 read=7250
Planning Time: 0.180 ms
Execution Time: 85.410 ms

/*
 * DIAGNOSIS:
 * 1. 'Seq Scan' indicates a full table scan across 1,000,000 rows!
 * 2. 'Rows Removed by Filter: 999992': 99.9% of rows were read from disk and immediately discarded!
 * 3. 'shared read=7250': 7,250 physical disk blocks read (huge I/O penalty).
 * 4. FIX: Create an index on (customer_id, order_date).
 */
```

### 5.2 Transforming a Seq Scan to an Index Only Scan via Covering Indexes
```sql
-- Goal: Optimize high-frequency endpoint:
-- SELECT order_number, total_amount FROM orders WHERE customer_id = ?;

-- 1. Standard Index:
CREATE INDEX idx_orders_customer ON orders(customer_id);

-- Plan with Standard Index:
-- Result: 'Index Scan' (traverses index, then must read heap table pages to get order_number and total_amount)
-- Buffers: shared hit=15 (index pages + table heap pages)

-- 2. COVERING INDEX using PostgreSQL 'INCLUDE' clause:
CREATE INDEX idx_orders_customer_covering 
ON orders (customer_id) 
INCLUDE (order_number, total_amount);

-- Plan with Covering Index:
-- Result: 'Index Only Scan'!
-- The database retrieves order_number and total_amount directly from the index leaf nodes!
-- Zero table heap reads required! Execution time drops to 0.08ms!
```

### 5.3 Composite Index Tuning: Equality First, Range Second Rule
```sql
-- Query under test:
SELECT * FROM audit_logs 
WHERE tenant_id = 'acme' 
  AND status = 'FAILED' 
  AND created_at >= '2026-11-01';

-- SUB-OPTIMAL INDEX (Range first):
CREATE INDEX idx_bad ON audit_logs (created_at, tenant_id, status);
-- FAILS OPTIMIZATION: B-Tree must scan ALL rows matching created_at >= '2026-11-01',
-- filtering tenant_id and status sequentially in memory!

-- PRODUCTION OPTIMAL INDEX (Equality first, Range second):
CREATE INDEX idx_good ON audit_logs (tenant_id, status, created_at);
-- EXCELLENT: B-Tree performs exact binary search for ('acme', 'FAILED'),
-- and only scans the exact contiguous range of created_at >= '2026-11-01'!
```

### 5.4 Anti-Pattern: Function Wrapping and Implicit Type Conversions
```sql
-- =========================================================================
-- ANTI-PATTERN 1: Function wrapping an indexed column in the WHERE clause
-- =========================================================================
-- Even if an index exists on 'created_at', wrapping it in DATE() PREVENTS index usage!
-- Forces a full Sequential Scan across the entire table!
SELECT * FROM orders WHERE DATE(created_at) = '2026-11-24';

-- PRODUCTION FIX: Rewrite query using an sargable range condition:
SELECT * FROM orders 
WHERE created_at >= '2026-11-24 00:00:00+00' 
  AND created_at <  '2026-11-25 00:00:00+00';

-- =========================================================================
-- ANTI-PATTERN 2: Implicit Type Conversion (e.g. string vs varchar/numeric)
-- =========================================================================
-- If phone_number is VARCHAR, passing an integer forces PostgreSQL to cast
-- every single row: WHERE phone_number::integer = 1234567890 (bypasses index)!
SELECT * FROM customers WHERE phone_number = 1234567890; -- BAD!

-- PRODUCTION FIX: Pass matching types:
SELECT * FROM customers WHERE phone_number = '1234567890'; -- GOOD!
```

### 5.5 Monitoring Query Performance with DataSource-Proxy and Hibernate Statistics
```yaml
# application-dev.yaml - Query Performance Monitoring
spring:
  jpa:
    properties:
      hibernate:
        # Collects query counts, cache hits, execution times
        generate_statistics: true
```

```java
package com.example.db.config;

import net.ttddyy.dsproxy.listener.logging.SLF4JLogLevel;
import net.ttddyy.dsproxy.support.ProxyDataSourceBuilder;
import org.springframework.beans.BeansException;
import org.springframework.beans.factory.config.BeanPostProcessor;
import org.springframework.context.annotation.Configuration;

import javax.sql.DataSource;

/**
 * Intercepts the Spring DataSource and logs any query executing longer than 200ms.
 */
@Configuration
public class SlowQueryLoggingBeanPostProcessor implements BeanPostProcessor {

    @Override
    public Object postProcessAfterInitialization(Object bean, String beanName) throws BeansException {
        if (bean instanceof DataSource dataSource && !beanName.contains("proxy")) {
            return ProxyDataSourceBuilder.create(dataSource)
                .name("SlowQueryLogger")
                .logSlowQueryBySlf4j(200, java.util.concurrent.TimeUnit.MILLISECONDS, SLF4JLogLevel.WARN)
                .countQuery()
                .build();
        }
        return bean;
    }
}
```

---

## 6. Best Practices

1. **Always Profile with `EXPLAIN (ANALYZE, BUFFERS)`**: Never rely on plain `EXPLAIN` (which only shows estimates); `ANALYZE` executes the query and reports actual elapsed times and disk buffer hits.
2. **Follow the Equality-First, Range-Second Rule**: In composite indexes, place exact-equality columns (`=`) first, followed by range/inequality columns (`<`, `>`, `BETWEEN`).
3. **Use Covering Indexes (`INCLUDE`) for High-Throughput Read Queries**: Store additional projected columns in the index leaf nodes to enable **Index Only Scans** without expanding B-Tree index key size.
4. **Keep Queries "Sargable" (Search Argument Able)**: Never wrap indexed columns in functions (e.g., `WHERE LOWER(email) = ?` or `WHERE EXTRACT(YEAR FROM date) = ?`). Use range queries or create an expression index (`CREATE INDEX ON users (LOWER(email))`).
5. **Use Partial Indexes for Skewed Boolean Flags**: Never index a standard boolean column with a full index. Use a partial index (`CREATE INDEX ... WHERE active = true` or `WHERE is_deleted = false`).
6. **Set Slow Query Thresholds in Production**: Configure PostgreSQL `log_min_duration_statement = 200` to automatically log any query exceeding 200ms to your observability stack.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Over-Indexing (Index on every column)** | Every index slows down `INSERT`, `UPDATE`, and `DELETE` operations and wastes database memory. | Only index columns used in frequent `WHERE`, `JOIN`, and `ORDER BY` clauses. |
| **Indexing Low-Cardinality Columns (e.g. Gender, Boolean)** | If an index matches 50% of the table, the optimizer ignores it and chooses a `Seq Scan`. | Use partial indexes or combine with high-cardinality columns in a composite index. |
| **Function Wrapping on Indexed Columns** | `WHERE UPPER(username) = ?` cannot use an index on `username`. | Create a functional index: `CREATE INDEX ON users(UPPER(username))`. |
| **Wrong Column Order in Composite Indexes** | Putting a range column before an equality column prevents the index from filtering subsequent columns. | Place equality columns first: `(status, created_at)`. |
| **Ignoring Table Statistics (`ANALYZE`)** | Stale table statistics cause the optimizer to estimate row counts incorrectly and choose poor query plans. | Run `ANALYZE table_name;` after large data migrations. |
| **Relying on H2 for Query Tuning** | H2 lacks cost-based query optimization, buffer metrics, and PostgreSQL join algorithms. | Always optimize queries using real PostgreSQL with Testcontainers. |

---

## 8. Performance & Security Notes

### Performance
- **Buffer Cache Hit Ratio**: Aim for a database cache hit ratio $>99\%$. A low cache hit ratio indicates that working sets exceed available RAM, forcing the engine to read table pages from slow physical SSDs.
- **Index Only Scan Speedup**: Eliminating table heap lookups reduces I/O from multiple random disk page reads to a single contiguous index page read.

### Security
- **Denial of Service via Unindexed Wildcard Queries**: Queries with leading wildcards (`LIKE '%search%'`) force full table scans. When executed concurrently by multiple users, they can saturate 100% of database CPU, taking down the entire service.

---

## 9. Comparison / When to Use What

| Scan / Join Operator | Speed | Disk I/O | Best Use Case |
| :--- | :--- | :--- | :--- |
| **Index Only Scan** | **Fastest (~0.1ms)** | **Zero Table Heap Reads** | Covering index covering all query fields |
| **Index Scan** | Very Fast (~1ms) | Moderate (Reads index + heap) | Highly selective queries ($<5\%$ of table) |
| **Bitmap Index Scan** | Fast (~5ms) | Low (Batches heap reads) | Medium selectivity ($5\text{--}20\%$ of table) |
| **Sequential Scan** | Slow on large tables | Very High (Reads entire table) | Small tables ($<1,000$ rows) or bulk reads |
| **Nested Loop Join** | Fast for small data | Low (Index-driven) | Small outer table joined to indexed inner table |
| **Hash Join** | Fast for large sets | High RAM usage | Joining large unindexed tables on equality |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the difference between an Index Scan and an Index Only Scan in PostgreSQL?</strong></summary>

- An **Index Scan** searches the B-Tree index to locate row pointers (Tuple IDs), and then reads the corresponding physical heap pages from table storage to fetch the required column values.
- An **Index Only Scan** finds the required row pointers in the index and extracts *all* requested column values directly from the index leaf nodes (via a covering index with `INCLUDE`) without reading the physical table heap pages at all.
</details>

<details>
<summary><strong>2. What does "sargable" mean in database query optimization?</strong></summary>

A query condition is "sargable" (Search Argument Able) if the database engine can utilize an index to execute the search via a direct B-Tree seek. Wrapping an indexed column in a function (e.g., `WHERE YEAR(date) = 2026` or `WHERE SUBSTRING(name, 1, 3) = 'ABC'`) makes it non-sargable, forcing a full table scan. Rewriting it as a range (`WHERE date >= '2026-01-01' AND date < '2027-01-01'`) restores sargability.
</details>

<details>
<summary><strong>3. How does the "Equality First, Range Second" rule dictate composite index column ordering?</strong></summary>

B-Tree indexes maintain sorted order across columns from left to right. Once the index navigation encounters an inequality/range comparison (`>`, `<`, `BETWEEN`), the sorted order of subsequent columns in the index cannot be used for further binary searching; subsequent columns can only be evaluated as filter checks. Therefore, columns tested for exact equality (`=`) must always precede range columns in the index definition.
</details>

<details>
<summary><strong>4. Why does PostgreSQL sometimes choose a Sequential Scan even when an index exists on the searched column?</strong></summary>

The Cost-Based Optimizer (CBO) estimates the cost of an index scan (random I/O to read index blocks plus random I/O to read table heap blocks) versus a sequential scan (sequential I/O reading blocks sequentially). If the query matches a large percentage of the table (typically $>15\text{--}25\%$), random I/O becomes more expensive than reading the entire table sequentially, so the optimizer intentionally chooses a Sequential Scan.
</details>

<details>
<summary><strong>5. What is the purpose of the <code>INCLUDE</code> clause when creating a PostgreSQL index?</strong></summary>

The `INCLUDE` clause defines non-key attributes in an index. These columns are stored only in the leaf nodes of the B-Tree and do not participate in the index key sorting tree. This keeps the index structure small and fast while providing a "covering index" that enables **Index Only Scans** for queries selecting those additional columns.
</details>

<details>
<summary><strong>6. What is a Bitmap Index Scan in PostgreSQL?</strong></summary>

A Bitmap Index Scan is an intermediate access method used when a query matches too many rows for an Index Scan, but not enough to justify a Sequential Scan. It scans the index, builds a memory bitmap where bits represent physical database pages containing matching rows, sorts the page references by physical disk order, and reads the table pages sequentially (Bitmap Heap Scan), minimizing random disk seek latency.
</details>

<details>
<summary><strong>7. How do stale database statistics degrade query performance?</strong></summary>

The query optimizer relies on statistics gathered in `pg_statistic` (row counts, null fractions, distinct values, histograms) to estimate query costs. If a table undergoes large batch insertions or updates and statistics are not updated, the optimizer's cost estimates will be wildly inaccurate, causing it to choose disastrous execution plans (e.g., selecting a Nested Loop Join instead of a Hash Join).
</details>

<details>
<summary><strong>8. What is a Functional (Expression) Index?</strong></summary>

A functional index is an index built on the result of an expression or function rather than a raw column value (e.g., `CREATE INDEX idx_user_lower_email ON users (LOWER(email))`). It allows queries containing that exact function (`WHERE LOWER(email) = ?`) to utilize the index directly.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Identifying Non-Sargable Query</strong></summary>

**Task**: Refactor the non-sargable query `SELECT * FROM users WHERE LOWER(email) = 'alice@example.com'` into a sargable condition or write the required PostgreSQL functional index.

```sql
-- Solution: Functional Index
CREATE INDEX idx_users_lower_email ON users (LOWER(email));
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Covering Index Definition</strong></summary>

**Task**: Write the SQL DDL statement to create a covering index on `orders` for `customer_id` that includes `order_date` and `total_amount`.

```sql
CREATE INDEX idx_orders_customer_covering 
ON orders (customer_id) 
INCLUDE (order_date, total_amount);
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Composite Index Ordering</strong></summary>

**Task**: For query `SELECT * FROM tasks WHERE assignee_id = 42 AND priority = 'HIGH' AND due_date <= NOW()`, determine the optimal composite index column order.

```sql
-- Equality first (assignee_id, priority), Range second (due_date)
CREATE INDEX idx_tasks_optimal ON tasks (assignee_id, priority, due_date);
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Partial Index for Soft-Deleted Active Rows</strong></summary>

**Task**: Write an index on `products` indexing `sku` only for non-deleted records (`is_deleted = false`).

```sql
CREATE INDEX idx_products_active_sku ON products (sku) WHERE is_deleted = false;
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Execution Plan Node Assertion in Tests</strong></summary>

**Task**: Write a test executing `EXPLAIN` via `JdbcClient` and assert that the query execution plan contains `"Index Only Scan"`.

```java
@Test
void testQueryUsesIndexOnlyScan() {
    String plan = jdbcClient.sql("""
        EXPLAIN SELECT order_number, total_amount 
        FROM orders WHERE customer_id = 101
        """)
        .query(String.class)
        .list()
        .toString();

    assertThat(plan).contains("Index Only Scan");
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Slow Query Logging Filter Setup</strong></summary>

**Task**: Configure `datasource-proxy` programmatically to log a warning whenever any SQL query takes longer than 150 milliseconds.

```java
@Bean
public DataSource dataSourceProxy(DataSource realDataSource) {
    return ProxyDataSourceBuilder.create(realDataSource)
        .name("PerformanceProxy")
        .logSlowQueryBySlf4j(150, TimeUnit.MILLISECONDS, SLF4JLogLevel.WARN)
        .build();
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Golden Rule 1: Index Only Scan = Fastest query possible (requires Covering Index)      |
| Golden Rule 2: Composite index rule: Equality columns FIRST, Range columns LAST       |
| Golden Rule 3: NEVER wrap indexed columns in functions (e.g. DATE(col), LOWER(col))   |
| Diagnostic:    Always use EXPLAIN (ANALYZE, BUFFERS) to verify actual execution times |
| Partial Index: Index only rows matching WHERE condition (saves 90% index memory)      |
| Production:    Set log_min_duration_statement = 200ms to monitor slow queries         |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Use The Index, Luke: A Guide to Database Performance for Developers](https://use-the-index-luke.com/)
- [PostgreSQL EXPLAIN Documentation](https://www.postgresql.org/docs/current/using-explain.html)
- [Vlad Mihalcea: The Anatomy of a High-Performance SQL Query Execution Plan](https://vladmihalcea.com/execution-plan-anatomy/)
- [The Internals of PostgreSQL: Query Processing and Optimization](https://www.interdb.jp/pg/pgsql03.html)
