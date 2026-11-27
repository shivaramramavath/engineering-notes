# 06: Sorting Results with ORDER BY

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [05 - Filtering Rows with WHERE & Operators](./05-where-filtering.md) |
| **Target Audience** | Software Engineers, Data Analysts, Database Administrators |
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

In relational database theory, a relation is defined as a mathematical set of tuples. By definition, **sets have no inherent order**. When a database returns rows without an explicit `ORDER BY` clause, the returned order is non-deterministic: it depends on disk block layout, buffer pool cache hits, parallel scan worker scheduling, or concurrent write operations. A query without `ORDER BY` might return rows in one order at 9:00 AM and in a completely different order at 9:01 AM.

The `ORDER BY` clause is the only mechanism in SQL that guarantees deterministic row presentation. However, sorting is computationally expensive:
- It requires comparing every candidate tuple.
- In-memory sorting consumes precious session memory (`work_mem`).
- When result sets exceed memory limits, the engine must spill tuples to temporary disk files, executing slow multi-pass external merge sorts.
- Divergent dialect defaults for `NULL` sorting (`NULLS FIRST` vs `NULLS LAST`) and string collations can introduce subtle discrepancies in cross-database applications.

Understanding physical sort algorithms, collation mechanics, and how B-Tree indexes can eliminate sorting overhead entirely is essential for building scalable, predictable database applications.

---

## 2. Core Concepts

### 1. Directional Sorting & Multi-Column Precedence
Sorting can be configured per column in ascending (`ASC`, default) or descending (`DESC`) direction:
```sql
ORDER BY department_id ASC, salary DESC;
```
- The engine first sorts all rows by `department_id` from lowest to highest.
- For rows sharing the identical `department_id`, it breaks ties by sorting `salary` from highest to lowest.

### 2. The `NULLS FIRST` vs `NULLS LAST` Standard
In relational databases, `NULL` represents an unknown value. How should an unknown value compare against concrete numbers or strings during sorting?
- **Standard SQL (PostgreSQL & Oracle)**:
  - `ASC` defaults to `NULLS LAST` (Nulls are treated as greater than all values).
  - `DESC` defaults to `NULLS FIRST` (Nulls appear at the top).
  - Can be explicitly overridden: `ORDER BY score DESC NULLS LAST`.
- **MySQL, SQLite & SQL Server**:
  - Treat `NULL` as lower than all non-null values.
  - In `ASC`, NULLs appear at the very beginning.
  - In `DESC`, NULLs appear at the very end.

### 3. Collations & Character Sorting
String sorting is determined by **Collations** (rules defining character sort order and equality):
- `C` / `POSIX`: Pure binary byte-order sort based on ASCII/UTF-8 byte values (uppercase `'A'` sorts before lowercase `'a'`).
- `en_US.UTF-8`: Linguistic collation based on natural language alphabetical rules.
- `und-x-icu`: Modern International Components for Unicode (ICU) collations providing consistent sorting across operating systems.

---

## 3. How It Works Internally

### Physical Sorting Strategies in the Executor

When a query planner encounters an `ORDER BY` clause, it chooses between three physical execution paths:

```
               [Query with ORDER BY Clause]
                            │
          Is there an Index matching the sort order?
                     /              \
                 YES                  NO
                 /                      \
        [B-Tree Index Scan]        [Physical Sort Operator]
    Traverse B-Tree leaf pages               │
    in order. ZERO sorting CPU!      Does data fit in work_mem?
    Startup Cost: 0.00!                     /          \
                                         YES             NO
                                         /                 \
                          [quicksort / top-N heap]   [external merge sort]
                          In-memory sort in RAM.     Spills to temporary
                          Fast!                      disk files! Slow!
```

1. **B-Tree Index Scan (Zero Sorting Overhead)**:
   - If an index exists on `(department_id, salary DESC)`, the engine simply walks the index leaves in order. Sorting cost is mathematically eliminated!
2. **Top-N Heap Sort (In-Memory Optimization)**:
   - When `ORDER BY` is paired with `LIMIT k` (e.g., `LIMIT 10`), the executor does not sort all 1,000,000 rows. It maintains an in-memory priority queue of size $k$, discarding non-qualifying rows as it streams tuples.
3. **Quicksort (In-Memory)**:
   - Used when all candidate tuples fit entirely within `work_mem`.
4. **External Merge Sort (Disk Spill)**:
   - When tuples exceed `work_mem`, the engine divides the dataset into chunks, sorts each chunk in memory, writes them to temporary disk files, and merges them in multiple passes. This introduces severe disk I/O latency.

---

## 4. Syntax & Usage

### Standard ANSI ORDER BY Syntax

```sql
SELECT 
    customer_id, 
    first_name, 
    last_name, 
    credit_score, 
    created_at
FROM customers
ORDER BY 
    -- 1. Explicit NULL positioning
    credit_score DESC NULLS LAST,
    
    -- 2. Secondary tie-breaker column
    last_name ASC,
    
    -- 3. Expression-based sort
    created_at DESC;
```

### Multi-Dialect Comparison Matrix

| Sorting Capability | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **`NULLS FIRST / LAST` Syntax** | Supported natively | Not supported (emulated via `ISNULL`)| Supported natively (since 3.30)| Not supported (emulated via `CASE`) |
| **Default `ASC` NULL Placement**| `NULLS LAST` | `NULLS FIRST` | `NULLS FIRST` | `NULLS FIRST` |
| **Default `DESC` NULL Placement**| `NULLS FIRST` | `NULLS LAST` | `NULLS LAST` | `NULLS LAST` |
| **Sort by Ordinal Position** | Supported (`ORDER BY 1, 2`)| Supported (`ORDER BY 1, 2`) | Supported (`ORDER BY 1, 2`) | Supported (`ORDER BY 1, 2`) |
| **Sort by SELECT Column Alias**| Supported | Supported | Supported | Supported |
| **ICU Collations** | Supported natively | Supported | Supported | Supported |

---

## 5. 5 Practical Progressive Examples

### Example 1: Multi-Column Tie-Breaking
Sorting products by category ascending, then by retail price descending, and breaking price ties by product name ascending:

```sql
SELECT product_id, category_id, product_name, retail_price
FROM products
ORDER BY category_id ASC, retail_price DESC, product_name ASC;
```
```text
+------------+-------------+---------------------------------------+--------------+
| product_id | category_id | product_name                          | retail_price |
+------------+-------------+---------------------------------------+--------------+
|        201 |           2 | Framework Laptop 16                   |      1399.00 |
|        202 |           2 | MacBook Air M3                        |      1099.00 |
|        203 |           3 | Pixel 9 Pro                           |       999.00 |
|        204 |           4 | Sony WH-1000XM5                       |       399.00 |
|        205 |           5 | Designing Data-Intensive Applications |        45.00 |
+------------+-------------+---------------------------------------+--------------+
```

### Example 2: Cross-Dialect NULL Ordering Emulation
How to achieve identical NULL-last descending order in engines lacking native `NULLS LAST` syntax (MySQL and SQL Server):

```sql
-- Standard PostgreSQL & SQLite (Native):
SELECT customer_id, first_name, credit_score
FROM customer_profiles
ORDER BY credit_score DESC NULLS LAST;

-- Universal Emulation (Works on MySQL, SQL Server, Postgres, SQLite):
SELECT customer_id, first_name, credit_score
FROM customer_profiles
ORDER BY 
    CASE WHEN credit_score IS NULL THEN 1 ELSE 0 END ASC,
    credit_score DESC;
```
*(The `CASE` expression assigns `0` to valid values and `1` to NULLs, ensuring valid numbers sort first while NULLs are pushed to the bottom).*

### Example 3: Edge Case (Sorting by Positional Ordinals)
SQL permits sorting by the numeric index of the column in the `SELECT` list:

```sql
-- ORDER BY 2 DESC sorts by the 2nd projected column (total_spent)
SELECT 
    c.customer_id, 
    SUM(o.total_amount) AS total_spent
FROM customers c
JOIN orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id
ORDER BY 2 DESC;
```
*(While supported by the standard, positional ordering like `ORDER BY 2` is considered an anti-pattern in production code because reordering columns in `SELECT` silently changes the sort order).*

### Example 4: Failure Mode (Sorting with Collations & Case Sensitivity)
Why string sorting behaves differently across operating systems and collations:

```sql
CREATE TABLE collation_demo (name VARCHAR(20));
INSERT INTO collation_demo VALUES ('apple'), ('Banana'), ('cherry'), ('Apple');

-- Binary C Collation: All Uppercase letters sort before lowercase!
SELECT name FROM collation_demo ORDER BY name COLLATE "C";
```
```text
+--------+
| name   |
+--------+
| Apple  |
| Banana |
| apple  |
| cherry |
+--------+
```
```sql
-- Natural Linguistic Collation (en_US): Case-folded natural alphabetical sorting
SELECT name FROM collation_demo ORDER BY name COLLATE "en_US.UTF-8";
```
```text
+--------+
| name   |
+--------+
| Apple  |
| apple  |
| Banana |
| cherry |
+--------+
```

### Example 5: Optimized Index-Backed Sorting (Eliminating Sort Node)
Inspecting the execution plan when an index completely eliminates the need for physical sorting:

```sql
-- Given an index on orders:
-- CREATE INDEX idx_orders_customer_date ON orders(customer_id, order_date DESC);

EXPLAIN 
SELECT * FROM orders 
WHERE customer_id = 101 
ORDER BY order_date DESC;
```
```text
                                       QUERY PLAN                                       
----------------------------------------------------------------------------------------
 Index Scan using idx_orders_customer_date on orders  (cost=0.15..8.17 rows=5 width=48)
   Index Cond: (customer_id = 101)
```
*(Notice there is **NO `Sort` node** in the query plan! The engine traverses the pre-sorted B-Tree directly, achieving $O(1)$ startup time).*

---

## 6. Best Practices

1. **Never Rely on Implicit Insertion Order**: In relational databases, rows have no guaranteed order without an explicit `ORDER BY`. If your application requires ordered data, always specify `ORDER BY`.
2. **Back High-Frequency Sorts with B-Tree Indexes**: If an API endpoint frequently queries `WHERE user_id = ? ORDER BY created_at DESC LIMIT 20`, create a composite index on `(user_id, created_at DESC)` to eliminate physical in-memory sorting.
3. **Explicitly Specify `NULLS FIRST` or `NULLS LAST`**: Do not rely on engine defaults for NULL sorting. Dialects disagree fundamentally on default NULL placement; explicit declarations prevent cross-database migration bugs.
4. **Avoid Positional References (`ORDER BY 1, 2`) in Production**: Positional sorting is brittle; adding or rearranging a column in the `SELECT` list silently changes which column is sorted. Always reference explicit column names or aliases.
5. **Add Deterministic Tie-Breakers for Pagination**: When sorting by non-unique columns (like `order_date` or `status`), always append a unique column as the final tie-breaker (e.g., `ORDER BY order_date DESC, order_id DESC`). Without a unique tie-breaker, pagination can display duplicate rows or skip rows across pages.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Non-Deterministic Pagination Without Unique Tie-Breakers** | If 50 rows share the exact same `created_at`, sorting by `created_at LIMIT 10 OFFSET 10` can return the same row across multiple pages! | Always add a unique tie-breaker: `ORDER BY created_at DESC, id DESC`. |
| **Assuming SQLite/MySQL Sort NULLs Like PostgreSQL** | Postgres defaults `ASC` to `NULLS LAST`; MySQL and SQLite default to `NULLS FIRST`. | Explicitly specify `NULLS LAST` or use `CASE` emulation. |
| **Relying on Subquery `ORDER BY`** | The SQL standard specifies that `ORDER BY` inside an un-limited subquery or view is ignored by the outer query optimizer. | Place `ORDER BY` in the outermost query. |
| **Using `ORDER BY RANDOM()` on Large Tables** | `ORDER BY RANDOM() LIMIT 1` requires generating a random number for every single row and sorting the entire 10-million-row table! | Use reservoir sampling or sample indexed IDs. |
| **Exceeding `work_mem` Spilling to Disk** | Complex sorts on large tables exceed `work_mem`, triggering multi-pass disk merge sorts that slow queries by 50x. | Increase `work_mem` for analytical queries or back the sort with an index. |
| **Using `ORDER BY` with Positional Numbers** | Changing `SELECT a, b` to `SELECT b, a` with `ORDER BY 1` silently changes the sort column. | Use explicit column names: `ORDER BY a`. |

---

## 8. Performance & Security Considerations

### Performance
- **Top-N Heapsort vs Full Sort**: When an `ORDER BY` is paired with a small `LIMIT`, PostgreSQL uses a Top-N Heapsort. Instead of sorting 1,000,000 rows, it allocates a small binary heap of 10 items in RAM, completing in milliseconds.
- **Bi-Directional Index Scans**: PostgreSQL B-Tree indexes can be scanned forward or backward. An index on `(created_at ASC)` can satisfy `ORDER BY created_at DESC` with zero penalty. However, for multi-column sorts with mixed directions (e.g., `col1 ASC, col2 DESC`), you must define the index with matching directions: `CREATE INDEX idx ON tbl(col1 ASC, col2 DESC)`.

### Security
- **SQL Injection in Dynamic `ORDER BY`**:
  - Developers frequently parameterize `WHERE` clauses but dynamically concatenate user input into `ORDER BY`:
    ```sql
    "SELECT * FROM users ORDER BY " + userInput;
    ```
  - Attackers can exploit this via blind SQL injection:
    ```sql
    -- Malicious input: (CASE WHEN (SELECT 1)=1 THEN id ELSE name END)
    ```
  - Always validate sorting column names against a strict whitelist of permitted identifiers.

---

## 9. Comparison Matrix

### Sorting Execution Methods

| Execution Method | Memory Usage | Disk I/O | Performance | When Chosen by Optimizer |
|:---|:---|:---:|:---:|:---|
| **Index Scan** | Zero ($O(1)$) | None (Reads pages in order) | Ultra-Fast | B-Tree index matches sort column(s) |
| **Top-N Heap Sort**| Small ($O(K)$ bounded heap) | None | Very Fast | `ORDER BY` paired with small `LIMIT K` |
| **Quicksort** | Moderate ($O(N)$ within `work_mem`) | None | Fast | Dataset fits in `work_mem` |
| **External Merge** | High (Exceeds `work_mem`) | Heavy (Disk spill) | Slow | Dataset exceeds `work_mem` |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: Why does a query without an `ORDER BY` clause have no guaranteed order, even if the table has an auto-incrementing primary key?
<details>
<summary>Answer</summary>
Relational theory defines tables as unordered mathematical sets. 

The physical order in which rows are returned depends on the physical scan path chosen by the engine:
1. Sequential Scans read blocks in physical disk order, which may not match insertion order if rows were deleted, updated (MVCC creates new versions at the end of the heap), or vacuumed.
2. Parallel Workers scan different block ranges simultaneously, returning tuples in unpredictable arrival order.
3. Bitmap Heap Scans read pages in physical disk block order rather than index key order.

Without `ORDER BY`, any assumption of order is an illusion that breaks under production load.
</details>

#### Q2: How does a composite B-Tree index on `(department_id, salary DESC)` allow the engine to completely eliminate a `Sort` node?
<details>
<summary>Answer</summary>
A B-Tree index is physically stored in pre-sorted order on disk. In a composite index on `(department_id, salary DESC)`, the index leaf pages are arranged first by `department_id` ascending, and within each department, entries are arranged by `salary` descending.

When executing `WHERE department_id = 5 ORDER BY salary DESC`, the executor seeks directly to the first entry for department 5 and reads the leaf nodes sequentially. Because the data is already sorted by the physical structure of the B-Tree, the engine streams tuples directly to the client with zero CPU sorting cost and zero memory allocation.
</details>

#### Q3: What is the difference between `NULLS FIRST` and `NULLS LAST` across different database engines?
<details>
<summary>Answer</summary>
- **PostgreSQL & Oracle**: Follow the ANSI SQL standard convention where NULL is treated as larger than any known value. In `ASC`, NULLs sort last by default (`NULLS LAST`). In `DESC`, NULLs sort first by default (`NULLS FIRST`). Both engines allow explicit `NULLS FIRST` / `NULLS LAST` syntax.
- **MySQL, SQLite & SQL Server**: Treat NULL as smaller than any known value. In `ASC`, NULLs sort first by default. In `DESC`, NULLs sort last by default. In MySQL and SQL Server, achieving NULLS LAST in ascending order requires custom `CASE` expressions.
</details>

#### Q4: Why is `ORDER BY RANDOM() LIMIT 1` an anti-pattern on large tables, and how can it be optimized?
<details>
<summary>Answer</summary>
`ORDER BY RANDOM()` requires the database engine to:
1. Scan every single row in the table.
2. Call the pseudo-random generator function for every row.
3. Sort the entire table in memory or on disk based on the random float values.
4. Emit the top row.
On a 10-million-row table, this takes several seconds and saturates disk and CPU.

Optimized alternatives:
- Generate a random integer ID within `[min_id, max_id]` in application code and execute `WHERE id >= random_id LIMIT 1`.
- Use `TABLESAMPLE SYSTEM (1)` in PostgreSQL to read a random 8KB disk page.
</details>

### Practical Sorting Puzzles

#### Q5: Write an ANSI-compliant query that sorts employees by `commission_pct` descending, ensuring that employees with NULL commissions appear at the very bottom, in a way that works identically across PostgreSQL, MySQL, and SQL Server.
<details>
<summary>Answer</summary>

```sql
SELECT employee_id, first_name, commission_pct
FROM employees
ORDER BY 
    CASE WHEN commission_pct IS NULL THEN 1 ELSE 0 END ASC,
    commission_pct DESC;
```
</details>

#### Q6: Given a query with pagination `WHERE status = 'active' ORDER BY created_at DESC LIMIT 10 OFFSET 20`, explain why users might see duplicate rows on page 2 and page 3, and provide the fix.
<details>
<summary>Answer</summary>
If multiple records share the exact same `created_at` timestamp, the sort order between those duplicate rows is non-deterministic. When page 1 is fetched, the engine might return row A; when page 2 is fetched with `OFFSET 10`, concurrent inserts or varying execution timing can cause row A to appear again.

Fix: Always append a guaranteed unique column (such as the Primary Key) as the final tie-breaker:
```sql
ORDER BY created_at DESC, order_id DESC
LIMIT 10 OFFSET 20;
```
</details>

#### Q7: How can you force PostgreSQL to sort strings using a case-insensitive collation without modifying the underlying table schema?
<details>
<summary>Answer</summary>
Use the `COLLATE` clause in the `ORDER BY` expression:

```sql
SELECT username FROM users ORDER BY username COLLATE "und-x-icu";
-- Or standard lowercase expression:
SELECT username FROM users ORDER BY LOWER(username) ASC;
```
</details>

#### Q8: In `EXPLAIN ANALYZE`, you observe `Sort Method: external merge Disk: 14520kB`. How do you optimize this query?
<details>
<summary>Answer</summary>
This indicates the sort exceeded available memory and spilled 14.5MB of data to temporary disk files.
Optimizations:
1. **Increase `work_mem`** for the session: `SET work_mem = '64MB';`. This allows the sort to complete in RAM via Quicksort.
2. **Create an index** matching the `ORDER BY` and `WHERE` criteria to eliminate the sort operator entirely.
3. **Select fewer columns**: Avoid `SELECT *`, reducing the byte width of each tuple stored in the sort buffer.
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Multi-Directional Product Sort
Select `product_id`, `product_name`, `category_id`, and `retail_price` from `products`. Sort first by `category_id` ascending, and then by `retail_price` descending.
<details>
<summary>Solution</summary>

```sql
SELECT product_id, product_name, category_id, retail_price
FROM products
ORDER BY category_id ASC, retail_price DESC;
```
</details>

#### Exercise 2: Sorting with Aliases
Write a query calculating each customer's total spending (`SUM(total_amount) AS total_spent`). Sort the output by the `total_spent` alias descending.
<details>
<summary>Solution</summary>

```sql
SELECT 
    customer_id, 
    SUM(total_amount) AS total_spent
FROM orders
WHERE order_status = 'completed'
GROUP BY customer_id
ORDER BY total_spent DESC;
```
</details>

### Intermediate Exercises

#### Exercise 3: Cross-Engine Null Positioning
Write a query sorting the `inventory` table by `reorder_threshold` ascending. Ensure items with NULL thresholds appear at the very end, using portable syntax.
<details>
<summary>Solution</summary>

```sql
SELECT item_id, item_name, reorder_threshold
FROM inventory
ORDER BY 
    CASE WHEN reorder_threshold IS NULL THEN 1 ELSE 0 END ASC,
    reorder_threshold ASC;
```
*(In PostgreSQL/SQLite, `ORDER BY reorder_threshold ASC NULLS LAST` also works).*
</details>

#### Exercise 4: Custom Domain Status Ordering
Sort an `orders` table so that statuses appear in a specific custom business order: `'pending'` first, `'processing'` second, `'completed'` third, and `'cancelled'` last.
<details>
<summary>Solution</summary>

```sql
SELECT order_id, customer_id, order_status, total_amount
FROM orders
ORDER BY 
    CASE order_status
        WHEN 'pending'    THEN 1
        WHEN 'processing' THEN 2
        WHEN 'completed'  THEN 3
        WHEN 'cancelled'  THEN 4
        ELSE 5
    END ASC,
    order_id DESC;
```
</details>

### Advanced Exercises

#### Exercise 5: Deterministic Keyset Ordering Specification
Construct an `ORDER BY` specification for a social feed query displaying posts by `score` descending, breaking ties by `created_at` descending, and breaking secondary ties by `post_id` descending, ensuring zero duplicate rows across paginated requests.
<details>
<summary>Solution</summary>

```sql
SELECT post_id, score, created_at, content
FROM posts
ORDER BY score DESC, created_at DESC, post_id DESC
LIMIT 20;
```
</details>

#### Exercise 6: Profiling Sort Memory with EXPLAIN
Execute `EXPLAIN (ANALYZE, BUFFERS)` on an unindexed sort of 50,000 rows. Inspect whether the sort method used was `quicksort`, `top-N heapsort`, or `external merge`.
<details>
<summary>Solution</summary>

```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM orders 
ORDER BY total_amount DESC 
LIMIT 10;
```
Inspect the output:
`Sort Method: top-N heapsort  Memory: 25kB`
</details>

---

## 12. Summary & Quick Reference

### ORDER BY Cheatsheet
- **Syntax**: `ORDER BY col1 [ASC|DESC] [NULLS FIRST|LAST] [, ...]`
- **Default Direction**: `ASC` (Ascending, lowest to highest).
- **Default NULLs in Postgres**: `ASC` = `NULLS LAST`, `DESC` = `NULLS FIRST`.
- **Default NULLs in MySQL/SQLite**: `ASC` = `NULLS FIRST`, `DESC` = `NULLS LAST`.
- **Performance**:
  - Pre-sorted B-Tree index scan $\rightarrow$ $O(1)$ zero-cost sort.
  - Paired with `LIMIT K` $\rightarrow$ In-memory Top-N Heapsort ($O(N \log K)$).
  - Within `work_mem` $\rightarrow$ In-memory Quicksort.
  - Exceeds `work_mem` $\rightarrow$ External merge disk spill.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [Sorting Rows (ORDER BY)](https://www.postgresql.org/docs/17/queries-order.html)
- PostgreSQL Documentation: [Collation Support](https://www.postgresql.org/docs/17/collation.html)
- Markus Winand: *"Indexing for ORDER BY"* (Use The Index, Luke)
- [07 - Pagination: LIMIT, OFFSET & FETCH FIRST](./07-limit-offset.md)
