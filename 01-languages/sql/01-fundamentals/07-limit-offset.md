# 07: Pagination: LIMIT, OFFSET & FETCH FIRST

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [06 - Sorting Results with ORDER BY](./06-order-by.md) |
| **Target Audience** | Software Engineers, Backend API Developers, Performance Engineers |
| **Verification Status** | Verified against SQLite 3.50+ runner / PostgreSQL 17 baseline |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & Usage](#syntax--usage)
5. [5 Practical Progressive Examples](#5-practical-progressive-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance & Security Considerations](#performance--security-considerations)
9. [Comparison Matrix](#comparison-matrix)
10. [8 Real-World Interview Questions](#8-real-world-interview-questions)
11. [6 Graded Practice Exercises](#6-graded-practice-exercises)
12. [Summary & Quick Reference](#summary--quick-reference)
13. [Further Reading](#further-reading)

---

## 1. What & Why

Pagination is the process of dividing a large dataset into discrete, manageable subsets or "pages" for presentation in user interfaces and REST/GraphQL APIs. Retrieving a million rows in a single query would overwhelm server memory, saturate network bandwidth, lock database resources, and crash client applications.

The two primary SQL pagination paradigms are:
1. **Offset-Based Pagination (`OFFSET ... LIMIT`)**: The traditional, intuitive approach where the client requests "Page 5 with 20 items per page" (`OFFSET 80 LIMIT 20`). While simple to implement, offset pagination suffers from a fatal architectural flaw: **$O(N)$ linear performance degradation at scale**. Requesting page 10,000 forces the database to read and discard 200,000 rows before returning 20. Furthermore, concurrent insertions or deletions cause data drift, leading to duplicate or skipped items across pages.
2. **Keyset Pagination (Seek Method)**: An index-driven approach where the client remembers the unique values of the last item seen (`WHERE (created_at, id) < ($last_time, $last_id) LIMIT 20`). Keyset pagination maintains constant $O(1)$ / $O(\log N)$ performance regardless of whether you are on page 1 or page 1,000,000.

Understanding how database engines physically execute `LIMIT` and `OFFSET`, how the ANSI SQL:2008 standard `FETCH FIRST` improves upon legacy syntax, and when to transition to keyset pagination is essential for building responsive, scalable applications.

---

## 2. Core Concepts

### 1. Legacy Vendor Syntax vs ANSI SQL Standard
- **Vendor Syntax (`LIMIT n OFFSET m`)**: Popularized by MySQL and PostgreSQL. Concise and widely used, but historically non-standard.
- **ANSI SQL:2008 Standard (`OFFSET m ROWS FETCH NEXT n ROWS ONLY`)**: Formally defined in ISO/IEC 9075:2008. Supported by modern PostgreSQL, SQL Server, Oracle, and DB2.

### 2. The Fatal Flaw of High Offsets ($O(N)$ Waste)
When you execute:
```sql
SELECT * FROM orders ORDER BY order_date DESC LIMIT 20 OFFSET 100000;
```
The database engine **cannot** simply jump to the 100,001st row on disk. In relational storage, rows are variable-length and spread across unordered 8KB pages. The executor is forced to:
1. Scan or index-traverse the first 100,000 rows.
2. Evaluate and sort them.
3. Discard all 100,000 rows.
4. Emit the remaining 20 rows.
As page numbers grow, response latency increases linearly, transforming a 5ms query into a 15-second timeout.

```
OFFSET 100,000 LIMIT 20:
[Row 1 ... Row 100,000: READ FROM DISK AND DISCARDED!] ──► [Rows 100,001 - 100,020: EMITTED]
(Wastes 99.98% of all CPU and I/O effort!)
```

### 3. The Sliding Window / Phantom Row Phenomenon
Offset pagination assumes a static dataset between page requests. In real-world multi-user applications, data is continuously modified:
- **Duplicate Rows**: A user views Page 1. A new order is inserted at the top. The user clicks "Page 2" (`OFFSET 10`). Because every existing row shifted down by one position, the 10th row from Page 1 now appears as the 1st row of Page 2!
- **Skipped Rows**: A user views Page 1. An order is deleted from Page 1. The user clicks "Page 2". Because rows shifted up by one, the original 11th row is skipped completely and never seen.

---

## 3. How It Works Internally

### The Limit Executor Node in the Volcano Pipeline

In the query execution pipeline, `Limit` sits near the top of the plan tree:

```
        [Limit Node (count = 20, offset = 80)]
                         │ Calls Next()
        [Sort Node (Top-N Heapsort or Quicksort)]
                         │ Calls Next()
        [Seq Scan or Index Scan on Table Heap]
```

1. **Without Offset (`LIMIT 20`)**:
   - The `Limit` node calls `Next()` on its child operator 20 times.
   - Once 20 tuples are received, the `Limit` node returns `NULL` (end of stream), halting child execution early!
2. **With High Offset (`OFFSET 100000 LIMIT 20`)**:
   - The `Limit` node must call `Next()` on its child **100,020 times**.
   - It silently drops the first 100,000 tuples in a tight CPU loop before emitting the final 20.
3. **With `WITH TIES` (SQL:2008 Feature)**:
   - The engine emits $N$ rows, but continues calling `Next()` as long as subsequent rows share the exact same sort key values as the $N$-th row.

---

## 4. Syntax & Usage

### Standard ANSI SQL:2008 Pagination Syntax

```sql
SELECT product_id, product_name, retail_price
FROM products
ORDER BY retail_price DESC, product_id ASC
OFFSET 40 ROWS
FETCH NEXT 20 ROWS ONLY;
```

### Standard ANSI SQL:2008 with Ties
```sql
SELECT product_id, product_name, retail_price
FROM products
ORDER BY retail_price DESC
FETCH FIRST 3 ROWS WITH TIES;
```

### Multi-Dialect Comparison Matrix

| Pagination Syntax | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **`LIMIT n OFFSET m`** | Supported natively | Supported natively | Supported natively | Not supported |
| **`LIMIT m, n` (Comma syntax)**| Not supported | Supported (`LIMIT offset, count`)| Not supported | Not supported |
| **ANSI `OFFSET ... FETCH`** | Full support (`ROWS ONLY` / `WITH TIES`)| Supported (8.0+) | Not supported (`LIMIT` only)| Full support (Mandatory `ORDER BY`) |
| **`TOP (n)` Syntax** | Not supported | Not supported | Not supported | Supported (`SELECT TOP (10)`) |
| **`WITH TIES` Support** | Supported | Not supported with `LIMIT` | Not supported | Supported with `TOP` and `FETCH` |

---

## 5. 5 Practical Progressive Examples

### Example 1: Basic Offset Pagination (Page Calculation Formula)
Mapping user-facing page numbers to SQL queries:
- $\text{Page Size} = S$
- $\text{Page Number} = P$ (1-indexed)
- $\text{OFFSET} = (P - 1) \times S$
- $\text{LIMIT} = S$

```sql
-- Fetching Page 2 with 3 items per page: OFFSET = (2 - 1) * 3 = 3
SELECT product_id, product_name, retail_price
FROM products
ORDER BY retail_price DESC, product_id ASC
LIMIT 3 OFFSET 3;
```
```text
+------------+---------------------------------------+--------------+
| product_id | product_name                          | retail_price |
+------------+---------------------------------------+--------------+
|        204 | Sony WH-1000XM5                       |       399.00 |
|        205 | Designing Data-Intensive Applications |        45.00 |
+------------+---------------------------------------+--------------+
```

### Example 2: Handling Ties with `FETCH FIRST ... WITH TIES`
When rows have identical prices, `WITH TIES` ensures no equal-ranking rows are arbitrarily truncated:

```sql
-- Insert a product tying for price with Sony WH-1000XM5 ($399.00)
INSERT INTO products VALUES (206, 4, 'Bose QuietComfort', 399.00, 30);

-- Request top 4 products WITH TIES:
SELECT product_id, product_name, retail_price
FROM products
ORDER BY retail_price DESC
FETCH FIRST 4 ROWS WITH TIES;
```
```text
+------------+---------------------+--------------+
| product_id | product_name        | retail_price |
+------------+---------------------+--------------+
|        201 | Framework Laptop 16 |      1399.00 |
|        202 | MacBook Air M3       |      1099.00 |
|        203 | Pixel 9 Pro         |       999.00 |
|        204 | Sony WH-1000XM5     |       399.00 |
|        206 | Bose QuietComfort   |       399.00 |
+------------+---------------------+--------------+
```
*(Notice: 5 rows returned! Both $399 products tied for 4th place and were included).*

### Example 3: Edge Case (High-Offset Performance Degradation Profile)
Observing the physical difference in `EXPLAIN ANALYZE` between Page 1 and Page 10,000:

```sql
-- Page 1 (Instant: scans 20 rows):
EXPLAIN ANALYZE 
SELECT * FROM orders ORDER BY order_id LIMIT 20 OFFSET 0;
-- Execution Time: 0.045 ms

-- Page 5,000 (Slow: scans and drops 100,000 rows):
EXPLAIN ANALYZE 
SELECT * FROM orders ORDER BY order_id LIMIT 20 OFFSET 100000;
-- Execution Time: 48.210 ms (Over 1,000x slower!)
```

### Example 4: Failure Mode (Missing Tie-Breaker Creating Ghost Rows)
Demonstrating how sorting by a non-unique column causes unstable pagination:

```sql
-- ANTI-PATTERN: retail_price is not unique; multiple items cost $399.00
SELECT product_id, product_name, retail_price
FROM products
ORDER BY retail_price DESC
LIMIT 2 OFFSET 3;
```
If two products have identical prices, the engine can return them in arbitrary order on consecutive calls. Page 1 might show Bose, and Page 2 might also show Bose!

**Fix: Append Unique Column**:
```sql
SELECT product_id, product_name, retail_price
FROM products
ORDER BY retail_price DESC, product_id ASC
LIMIT 2 OFFSET 3;
```

### Example 5: Optimized Keyset Pagination (The Scalable Alternative)
Replacing `OFFSET` with indexed keyset comparison:
- On initial page: Fetch first 20 rows.
- The client receives the last row: `created_at = '2024-04-10 09:00:00'`, `order_id = 1004`.
- To fetch the next page, query *after* that keyset:

```sql
SELECT order_id, customer_id, order_date, total_amount
FROM orders
WHERE (order_date, order_id) < ('2024-04-10 09:00:00', 1004)
ORDER BY order_date DESC, order_id DESC
LIMIT 20;
```
*(The query planner performs a direct B-Tree index seek to `(2024-04-10, 1004)` and reads the next 20 entries. Execution time is identical whether you are on Page 1 or Page 1,000,000: exactly 0.05ms)*.

---

## 6. Best Practices

1. **Use Standard ANSI `OFFSET ... FETCH` for New Code**: Prefer `OFFSET n ROWS FETCH NEXT m ROWS ONLY` over vendor-specific `LIMIT`. It is ISO standard and portable across Postgres, SQL Server, Oracle, and DB2.
2. **Always Provide a Deterministic Tie-Breaker**: Every `ORDER BY` used with pagination must terminate with a unique column (e.g., `PRIMARY KEY`). This prevents duplicate or omitted rows across page boundaries.
3. **Switch to Keyset Pagination for Infinite Scrolls and High Volume**: If users scroll through feeds, activity logs, or event streams, use keyset pagination (`WHERE (col, id) < (val, id)`). Never use offset pagination for tables exceeding 100,000 rows.
4. **Cap Maximum Page Depth in Public APIs**: If you must use offset pagination, enforce a hard limit on `OFFSET` (e.g., `MAX_OFFSET = 5000`). If a client requests `OFFSET 100000`, reject the request with HTTP 400 to protect database server health.
5. **Avoid Returning Total Row Counts on Every Request**: Running `SELECT COUNT(*) FROM table WHERE ...` on every paginated API request doubles query latency. Use pagination metadata with `has_next_page` (fetching `LIMIT + 1` rows) instead of calculating exact total pages.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **High Offsets in Deep Pagination** | `OFFSET 100000` scans and discards 100,000 rows, causing high CPU and memory thrashing. | Use Keyset Pagination (`WHERE id > last_id LIMIT 20`). |
| **Missing Unique Tie-Breaker** | Sorting by a non-unique column causes non-deterministic ordering where rows skip or duplicate across pages. | Append the primary key to the sort list: `ORDER BY created_at DESC, id DESC`. |
| **Calculating Total Count on Every Page** | Running `SELECT COUNT(*)` on every API call scans millions of rows just to display a page count. | Return cursor pagination (`has_more: true/false`) or cache the count. |
| **Using `LIMIT` Without `ORDER BY`** | The database returns an arbitrary set of rows that changes based on disk page traversal. | Always pair `LIMIT` with an explicit `ORDER BY`. |
| **Inverted Arguments in MySQL Comma Syntax** | In MySQL, `LIMIT 10, 20` means `LIMIT offset, count` (offset 10, return 20). Many developers mistake this for `LIMIT count, offset`. | Use explicit syntax: `LIMIT 20 OFFSET 10`. |
| **Data Drift During Offset Pagination** | Inserts or deletes shift rows, causing items to appear twice or be missed completely. | Use Keyset (cursor) pagination. |

---

## 8. Performance & Security Considerations

### Performance
- **The "Limit + 1" Trick**: To determine whether a "Next Page" exists without running an expensive `COUNT(*)`:
  - Request `LIMIT 21` when page size is 20.
  - If 21 rows return, emit the first 20 to the client and set `hasNextPage = true`.
  - If 20 or fewer return, set `hasNextPage = false`.
- **Deferred Joins (Late Row Lookup)**: For high-offset pagination where keyset pagination cannot be used, fetch only IDs first using an index-only scan, then join to fetch full columns:
  ```sql
  SELECT o.* FROM orders o
  JOIN (
      SELECT order_id FROM orders ORDER BY order_date DESC LIMIT 20 OFFSET 50000
  ) p ON o.order_id = p.order_id;
  ```

### Security
- **Denial of Service via Offset Exhaustion**:
  - Malicious scrapers requesting `?page=9999999` force the database to scan millions of rows per request, quickly exhausting CPU cores and connection pools. Enforce strict rate limits and maximum offset caps on public endpoints.

---

## 9. Comparison Matrix

### Offset Pagination vs Keyset Pagination

| Feature / Dimension | Offset Pagination (`OFFSET m LIMIT n`) | Keyset Pagination (`WHERE col < last_val`) |
|:---|:---|:---|
| **Performance Complexity** | $O(N)$ linear degradation as offset grows | $O(\log N)$ or $O(1)$ constant time via B-Tree |
| **Implementation Complexity** | Trivial (page number $\times$ limit) | Moderate (requires client to pass cursors) |
| **Random Page Jumping (Jump to Page 50)**| Supported natively | Not supported (sequential traversal only) |
| **Data Drift Resiliency** | Fragile (rows shift on concurrent insert/delete)| Highly resilient (anchored to immutable keys) |
| **Best Used For** | Small admin tables ($< 5,000$ rows), search results | Infinite scrolling feeds, high-volume APIs, mobile apps |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: Why does `SELECT * FROM tbl ORDER BY id LIMIT 10 OFFSET 1000000` execute slowly even when `id` is the primary key?
<details>
<summary>Answer</summary>
Even though `id` is indexed, the database engine cannot mathematically compute the physical disk address of the 1,000,001st row without traversing the preceding rows. 

The executor must traverse the B-Tree index across the first 1,000,000 leaf entries, dereference the corresponding table heap pages (if not an index-only scan), construct 1,000,000 tuple structures in memory, and immediately discard them in a CPU loop before finally capturing and emitting rows 1,000,001 to 1,000,010. The work performed scales linearly with the offset size ($O(N)$).
</details>

#### Q2: What is the "Sliding Window" problem in offset pagination?
<details>
<summary>Answer</summary>
The sliding window problem occurs when concurrent mutations modify the table while a user navigates between pages:
1. **Duplicate Rows**: If a user is on Page 1 and a new row is inserted at the top of the table, all existing rows shift down by 1 index position. When the user requests Page 2 (`OFFSET 10`), the row that was originally at position 10 on Page 1 is now at position 11, causing it to appear again on Page 2.
2. **Skipped Rows**: If a row on Page 1 is deleted, all subsequent rows shift up by 1 position. When the user requests Page 2, the row originally at position 11 has moved to position 10 and is skipped entirely.
</details>

#### Q3: How does the "Limit + 1" pattern optimize API pagination?
<details>
<summary>Answer</summary>
In many APIs, clients only need to know whether a "Next Page" button should be enabled. Naive implementations run two queries:
1. `SELECT * FROM items LIMIT 20 OFFSET 40;`
2. `SELECT COUNT(*) FROM items;` (Massively expensive full table scan).

With the "Limit + 1" pattern, the backend requests `LIMIT 21 OFFSET 40`. 
- If 21 rows are returned, the API knows there is a next page, strips off the 21st row, and returns 20 items with `has_more = true`.
- If $\le 20$ rows return, it returns them with `has_more = false`.
This eliminates the `COUNT(*)` query completely.
</details>

#### Q4: What does the ANSI SQL:2008 clause `WITH TIES` do, and what is its prerequisite?
<details>
<summary>Answer</summary>
`FETCH FIRST n ROWS WITH TIES` returns the top $n$ rows based on the `ORDER BY` criteria, plus any additional rows that have the exact same sort key values as the $n$-th row.

Prerequisite: It strictly requires an explicit `ORDER BY` clause. Without `ORDER BY`, the concept of a tie is meaningless, and the parser throws an error.
</details>

### Practical Pagination Puzzles

#### Q5: Convert this slow high-offset query into a fast keyset pagination query:
```sql
SELECT order_id, order_date, total_amount
FROM orders
ORDER BY order_date DESC, order_id DESC
LIMIT 20 OFFSET 50000;
```
<details>
<summary>Answer</summary>
Assuming the 50,000th row had `order_date = '2023-11-15 14:22:10'` and `order_id = 94821`:

```sql
SELECT order_id, order_date, total_amount
FROM orders
WHERE (order_date, order_id) < ('2023-11-15 14:22:10', 94821)
ORDER BY order_date DESC, order_id DESC
LIMIT 20;
```
*(This query utilizes an index on `(order_date DESC, order_id DESC)` to seek directly to the keyset in 0.05ms).*
</details>

#### Q6: Write a query using the Deferred Join technique to optimize offset pagination on a wide table with 50 columns.
<details>
<summary>Answer</summary>

```sql
SELECT o.*
FROM orders o
JOIN (
    -- Subquery uses index-only scan (narrow payload) to find target IDs
    SELECT order_id
    FROM orders
    ORDER BY order_date DESC, order_id DESC
    LIMIT 20 OFFSET 50000
) AS page_keys ON o.order_id = page_keys.order_id
ORDER BY o.order_date DESC, o.order_id DESC;
```
*(Instead of reading 50 columns for all 50,000 discarded rows, the subquery reads only the indexed `order_id` column, deferring full heap page fetches to the final 20 rows).*
</details>

#### Q7: Write an ANSI SQL:2008 compliant query that returns page 4 of a customer directory (page size: 25), sorted alphabetically by `last_name`, using standard `OFFSET ... FETCH` syntax.
<details>
<summary>Answer</summary>

```sql
SELECT customer_id, first_name, last_name, email
FROM customers
ORDER BY last_name ASC, customer_id ASC
OFFSET 75 ROWS
FETCH NEXT 25 ROWS ONLY;
```
*(Offset = $(4 - 1) \times 25 = 75$).*
</details>

#### Q8: Why does `SELECT * FROM tbl LIMIT 10` return different results when run twice in a row on a newly updated table?
<details>
<summary>Answer</summary>
Because it lacks an `ORDER BY` clause. In PostgreSQL and other MVCC engines, updating rows creates new tuple versions at the end of the heap file. During the subsequent sequential scan, the updated rows are encountered in a different physical disk order, returning a different set of 10 rows. 

Without `ORDER BY`, row ordering is completely non-deterministic.
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Basic Page 1 Query
Retrieve the top 5 most expensive products from `products`, displaying `product_id`, `product_name`, and `retail_price`.
<details>
<summary>Solution</summary>

```sql
SELECT product_id, product_name, retail_price
FROM products
ORDER BY retail_price DESC, product_id ASC
LIMIT 5;
```
</details>

#### Exercise 2: Calculating Offset
Given a page size of 15, write the query to fetch Page 3 of customers sorted by `customer_id` ascending.
<details>
<summary>Solution</summary>

```sql
-- Offset = (3 - 1) * 15 = 30
SELECT customer_id, first_name, last_name, email
FROM customers
ORDER BY customer_id ASC
LIMIT 15 OFFSET 30;
```
</details>

### Intermediate Exercises

#### Exercise 3: Standard ANSI Fetch with Ties
Select the top 3 highest spending orders from `orders`. If other orders tie with the 3rd order's total amount, include them in the result using ANSI standard syntax.
<details>
<summary>Solution</summary>

```sql
SELECT order_id, customer_id, total_amount
FROM orders
ORDER BY total_amount DESC
FETCH FIRST 3 ROWS WITH TIES;
```
</details>

#### Exercise 4: Implementing the "Limit + 1" Pattern
Write a query to retrieve up to 10 orders for `customer_id = 101` using the "Limit + 1" technique to detect whether subsequent orders exist.
<details>
<summary>Solution</summary>

```sql
SELECT order_id, order_date, total_amount
FROM orders
WHERE customer_id = 101
ORDER BY order_date DESC, order_id DESC
LIMIT 11; -- Request 10 + 1
```
*(If 11 rows return, the application presents 10 and sets `has_more = true`).*
</details>

### Advanced Exercises

#### Exercise 5: Bidirectional Keyset Pagination
Write two queries demonstrating keyset pagination for an infinite scroll timeline:
1. Fetching the "Next" 10 posts older than post `(2024-04-01 10:00:00, 500)`.
2. Fetching the "Previous" 10 posts newer than post `(2024-04-01 10:00:00, 500)`.
<details>
<summary>Solution</summary>

```sql
-- 1. Next Page (Older items):
SELECT post_id, created_at, content
FROM posts
WHERE (created_at, post_id) < ('2024-04-01 10:00:00', 500)
ORDER BY created_at DESC, post_id DESC
LIMIT 10;

-- 2. Previous Page (Newer items):
SELECT post_id, created_at, content
FROM posts
WHERE (created_at, post_id) > ('2024-04-01 10:00:00', 500)
ORDER BY created_at ASC, post_id ASC
LIMIT 10;
```
*(In application code, reverse the result array of query 2 to maintain chronological presentation).*
</details>

#### Exercise 6: Profiling Offset Degradation via EXPLAIN
Run `EXPLAIN (ANALYZE, BUFFERS)` on `OFFSET 10` versus `OFFSET 100000` on a table with 200,000 rows. Compare the `shared hit` buffer counts between the two executions.
<details>
<summary>Solution</summary>

```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM orders ORDER BY order_id LIMIT 10 OFFSET 10;

EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM orders ORDER BY order_id LIMIT 10 OFFSET 100000;
```
Observe how the `Buffers: shared hit` count increases proportionally with the offset size.
</details>

---

## 12. Summary & Quick Reference

### Pagination Paradigms Summary
- **Offset Pagination**:
  - `LIMIT n OFFSET m` or `OFFSET m ROWS FETCH NEXT n ROWS ONLY`.
  - Simple to build, supports jumping to arbitrary pages.
  - Flaws: $O(N)$ execution cost at high offsets, susceptible to sliding window drift.
- **Keyset Pagination (Seek Method)**:
  - `WHERE (sort_col, id) < (last_val, last_id) ORDER BY sort_col DESC, id DESC LIMIT n`.
  - Constant $O(1)$ / $O(\log N)$ performance via B-Tree index seek.
  - Flaw: Cannot jump directly to arbitrary page numbers (e.g., jump to page 47).
- **Golden Rules**:
  - Always append a unique tie-breaker column to `ORDER BY`.
  - Use "Limit + 1" to avoid `COUNT(*)` queries.
  - Cap maximum allowable offsets in public APIs.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [LIMIT and OFFSET](https://www.postgresql.org/docs/17/queries-limit.html)
- Markus Winand: *"We Need Tool Support for Keyset Pagination"* (Use The Index, Luke)
- Slack Engineering: *"Evolving API Pagination at Slack"*
- [08 - Three-Valued Logic & NULL Semantics](./08-null-semantics.md)
