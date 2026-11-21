# SQL and Indexing Essentials

> High-performance Java data access fundamentally depends on database schema design, relational integrity constraints, and B-Tree indexing strategies.

| Metadata | Details |
|---|---|
| **Level** | Core / Intermediate |
| **Prerequisites** | Relational database basics, Java data types |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: B-Tree Indexes & Execution Plans](#3-how-it-works-internally-b-tree-indexes--execution-plans)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why
JDBC is merely a protocol conduit connecting the Java Virtual Machine to relational database engines. Regardless of whether an application executes queries using raw JDBC, jOOQ, Spring JdbcClient, or Hibernate, query latency and throughput limits are governed directly by relational table structures and index access paths.

A poorly indexed relational query scanning millions of disk blocks can degrade an ultra-fast Java microservice from 50,000 ops/sec to 10 ops/sec. Relational database engines cannot magically optimize queries if the storage schema lacks foreign keys, primary keys, or appropriate composite indexes. Mastering schema design, composite index column order, keyset pagination, and execution plan analysis is indispensable for every enterprise Java backend engineer.

---

## 2. Core Concepts
Relational storage relies on structured entities, declarative constraints, and predictable access paths:

- **Primary & Foreign Keys**: Primary keys uniquely identify tuples and automatically generate clustered or primary B-Tree indexes. Foreign keys enforce referential integrity across relational boundaries.
- **Constraints**: Declarative guarantees (`NOT NULL`, `UNIQUE`, `CHECK`) enforced directly in the database engine kernel to eliminate data corruption.
- **B-Tree Indexes**: Balanced multi-way search trees that reduce lookups from O(N) sequential scans to O(log N) tree descents.
- **Leftmost Prefix Rule**: Composite indexes `(A, B, C)` can satisfy queries on `(A)`, `(A, B)`, and `(A, B, C)`, but cannot support lookups starting with `(B)` or `(C)` alone.
- **Covering Index**: An index containing all columns requested by a `SELECT` projection, completely bypassing secondary heap table lookups.
- **Sargability**: Search-Argument-Able query predicates that enable direct index range scans without evaluating expressions over every row.

---

## 3. How It Works Internally: B-Tree Indexes & Execution Plans

Relational database engines store table rows in fixed-size disk blocks (e.g., 8 KB in PostgreSQL, 16 KB in MySQL InnoDB). Without indexes, finding a record requires a Sequential Scan (full table scan), reading every block into the database buffer cache.

```
       [ Root Node: Page 100 ]
             /          \
            /            \
 [ Intermediate: 101 ]   [ Intermediate: 102 ]
     /        \             /         \
[Leaf: 201]  [Leaf: 202]  [Leaf: 203]  [Leaf: 204]  <-- Doubly-linked leaf nodes
     |            |            |            |
  (Heap TID)   (Heap TID)   (Heap TID)   (Heap TID)
```

### B-Tree Node Structure & Navigation
1. **Root & Branch Nodes**: Contain sorted keys and disk page pointers directing descent.
2. **Leaf Nodes**: Contain the indexed key values paired with row identifiers (TID / ctid in PostgreSQL, clustered primary key in InnoDB). Leaf nodes are linked sequentially via bidirectional pointers to facilitate fast range scans.
3. **Execution Plan Generation**:
   The query optimizer models multiple execution plans:
   - `Index Scan`: Traverses B-tree to leaf node, then fetches corresponding table block from heap.
   - `Index Only Scan`: Reads data entirely from the index leaf pages because all selected columns are indexed (covering index).
   - `Bitmap Index Scan`: Collects matching heap block addresses into an in-memory bitmap, then reads heap blocks sequentially to minimize random I/O.
   - `Seq Scan`: Directly reads all heap blocks sequentially; chosen by the optimizer when retrieving > 15-20% of total table rows.

---

## 4. Syntax & Usage

### 4.1 Production Schema DDL with Constraints
```sql
CREATE TABLE accounts (
    id            BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    account_number VARCHAR(32) NOT NULL UNIQUE,
    holder_name   VARCHAR(128) NOT NULL,
    balance       NUMERIC(14, 2) NOT NULL DEFAULT 0.00 CHECK (balance >= 0.00),
    status        VARCHAR(16) NOT NULL DEFAULT 'ACTIVE' CHECK (status IN ('ACTIVE', 'FROZEN', 'CLOSED')),
    created_at    TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE account_transactions (
    id             BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    account_id     BIGINT NOT NULL REFERENCES accounts(id) ON DELETE RESTRICT,
    amount         NUMERIC(14, 2) NOT NULL CHECK (amount <> 0),
    tx_type        VARCHAR(16) NOT NULL CHECK (tx_type IN ('DEPOSIT', 'WITHDRAWAL', 'TRANSFER')),
    reference_code VARCHAR(64) NOT NULL UNIQUE,
    tx_timestamp   TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

### 4.2 Composite & Partial Index Creation
```sql
-- Composite index respecting access pattern: filter by account, order by timestamp
CREATE INDEX idx_tx_account_time ON account_transactions(account_id, tx_timestamp DESC);

-- Partial index targeting only active high-value accounts
CREATE INDEX idx_accounts_high_balance ON accounts(balance) WHERE status = 'ACTIVE' AND balance > 100000;

-- Covering index using INCLUDE clause (PostgreSQL / SQL Server)
CREATE INDEX idx_tx_covering ON account_transactions(account_id, tx_timestamp DESC) INCLUDE (amount, tx_type);
```

---

## 5. Practical Examples

### Example 1: Basic Keyset Pagination vs Offset Pagination
```sql
-- ANTI-PATTERN: Deep offset pagination forces scanning and discarding 50,000 rows
SELECT id, account_id, amount, tx_timestamp
FROM account_transactions
WHERE account_id = 1042
ORDER BY tx_timestamp DESC, id DESC
LIMIT 20 OFFSET 50000;

-- OPTIMIZED: Keyset (Seek) Pagination utilizing composite index (account_id, tx_timestamp DESC, id DESC)
SELECT id, account_id, amount, tx_timestamp
FROM account_transactions
WHERE account_id = 1042
  AND (tx_timestamp, id) < ('2026-11-20 14:30:00+00', 98234)
ORDER BY tx_timestamp DESC, id DESC
LIMIT 20;
```
**Explanation**: In keyset pagination, the database navigates directly down the B-tree to the exact key tuple in O(log N) steps. It never reads or discards preceding records, maintaining constant execution time regardless of whether page 1 or page 50,000 is requested.

### Example 2: Inspecting Query Execution Plan with EXPLAIN ANALYZE
```sql
EXPLAIN (ANALYZE, BUFFERS, COSTS, VERBOSE)
SELECT t.id, t.amount, t.tx_timestamp
FROM account_transactions t
WHERE t.account_id = 1042
  AND t.tx_timestamp >= CURRENT_TIMESTAMP - INTERVAL '7 days'
ORDER BY t.tx_timestamp DESC;
```
Expected execution output showing an efficient Index Scan:
```text
Index Scan using idx_tx_account_time on account_transactions t
  Output: id, amount, tx_timestamp
  Index Cond: ((t.account_id = 1042) AND (t.tx_timestamp >= (CURRENT_TIMESTAMP - '7 days'::interval)))
  Buffers: shared hit=4
  Planning Time: 0.124 ms
  Execution Time: 0.082 ms
```

### Example 3: Edge Case - Non-Sargable Predicate Breakdown
```sql
-- ANTI-PATTERN: Wrapping column in function prevents B-Tree index utilization
SELECT id, email FROM users WHERE LOWER(email) = 'engineer@example.com';
-- Result: Forces Seq Scan on users table because index is on email, not LOWER(email).

-- SOLUTION A: Functional (Expression) Index
CREATE INDEX idx_users_lower_email ON users(LOWER(email));

-- SOLUTION B: Normalize at insertion time and query directly
SELECT id, email FROM users WHERE email = 'engineer@example.com';
```

### Example 4: Edge Case - Nullable Foreign Key and Three-Valued Logic
```sql
-- Demonstrating three-valued logic failure in subqueries:
SELECT name FROM departments 
WHERE id NOT IN (SELECT department_id FROM employees);
-- If a single employee has department_id IS NULL, the NOT IN expression yields UNKNOWN for all rows!
-- The query returns 0 rows unexpectedly.

-- CORRECT FIX:
SELECT d.name FROM departments d
WHERE NOT EXISTS (SELECT 1 FROM employees e WHERE e.department_id = d.id);
```

### Example 5: High-Performance Batch Insertion Pattern
```sql
-- Executing multi-row batch inserts to amortize network round-trips and transaction logging
INSERT INTO account_transactions (account_id, amount, tx_type, reference_code)
VALUES 
    (101, 150.00, 'DEPOSIT', 'REF-001'),
    (102, 320.50, 'DEPOSIT', 'REF-002'),
    (103, -45.00, 'WITHDRAWAL', 'REF-003')
ON CONFLICT (reference_code) DO NOTHING;
```

---

## 6. Best Practices
1. **Always Index Foreign Key Columns**: Relational engines do not automatically index foreign keys; omitting them leads to table locks and sequential scans during parent deletions and joins.
2. **Respect the Leftmost Prefix Rule**: Order composite index columns: equality filters first, range filters second, sort columns third.
3. **Use NUMERIC/BigDecimal for Financial Amounts**: Never store monetary balances or currencies as `FLOAT` or `DOUBLE` due to IEEE-754 precision loss.
4. **Prefer Keyset Pagination over OFFSET**: `OFFSET N` reads and discards N rows, producing O(N) latency degradation on deep pages.
5. **Enforce Referential Integrity in Schema**: Never delegate constraints exclusively to application code.
6. **Store Timestamps with Time Zone**: Always use `TIMESTAMP WITH TIME ZONE` (`timestamptz`) in PostgreSQL or UTC epoch timestamps.
7. **Use Explicit Column Lists**: Avoid `SELECT *`; explicit projections allow covering index optimizations (`Index Only Scan`).
8. **Partition Very Large Tables**: When tables exceed 100 million rows, implement declarative range or list partitioning by date or tenant.
9. **Regularly Vacuum and Analyze**: Maintain table statistics so the query cost optimizer makes accurate index vs sequential scan choices.
10. **Avoid Implicit Type Conversions**: Ensure host variables in Java precisely match database column types (e.g., String to VARCHAR, Long to BIGINT).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Querying `WHERE col = NULL` | SQL uses three-valued logic; comparison with NULL always yields `UNKNOWN` | Use `WHERE col IS NULL` or `IS NOT NULL` |
| Over-indexing every single column | Every additional index adds overhead to `INSERT`, `UPDATE`, and `DELETE` | Index only columns used in frequent predicates and joins |
| Function call on indexed column | Prevents index traversal (non-sargable expression) | Index the expression or rewrite predicate |
| Implicit type coercion | Comparing `VARCHAR` column with integer literal prevents index usage | Match literal data type strictly to column type |
| Unbounded `SELECT *` queries | Retrieves unnecessary columns, prevents covering index optimizations | Explicitly project required columns only |
| Missing constraints on unique columns | Application-level checks suffer from race conditions under concurrency | Add declarative `UNIQUE` constraints in schema |
| Using `NOT IN` with nullable subqueries | Any NULL in the subquery causes the entire condition to evaluate to UNKNOWN | Use `NOT EXISTS` or `WHERE col IS NOT NULL` |
| Deep `OFFSET` pagination in public APIs | Scans and discards millions of records, exhausting database CPU | Implement keyset (seek) pagination using cursor tokens |

---

## 8. Performance & Security Notes
- **Index Selectivity**: An index is most effective when column selectivity is high (many distinct values). Indexes on low-cardinality boolean or status columns are rarely selected by the optimizer unless partial indexing is applied.
- **SQL Injection Prevention**: Dynamic string concatenation in SQL queries allows malicious syntax injection. Always bind query parameters through parameterized statements (`PreparedStatement`).
- **Disk I/O vs Buffer Cache**: Database engines cache pages in shared memory buffers. High "shared hit" rates in `EXPLAIN` indicate cache hits, while "read" denotes physical disk I/O.
- **Write Amplification**: Updating an indexed column requires updating both the heap page and all corresponding index trees, increasing disk write amplification and transaction log volume.
- **Dead Tuples & Vacuuming**: In MVCC engines like PostgreSQL, `UPDATE` statements write new versions of tuples. Without routine autovacuuming, table bloat slows down sequential scans and index traversals.

---

## 9. Comparison / When to Use What

| Index Strategy | Target Query Pattern | Pros | Cons |
|---|---|---|---|
| **B-Tree Single Column** | Equality and range lookups on single column | Universal, compact, fast updates | Cannot optimize multi-column sorting |
| **B-Tree Composite** | Multi-attribute filters, sorting, keyset pagination | Satisfies filter + sort simultaneously | Rigid column order; size overhead |
| **Covering Index (`INCLUDE`)** | Read-heavy queries with specific projections | Eliminates table heap lookups | Increases index leaf page size |
| **Partial Index** | Queries targeting subsets (e.g., `status = 'ACTIVE'`) | Minimal disk footprint, fast maintenance | Only applicable when filter matches index predicate |
| **Hash Index** | Strictly exact equality lookups (`=`) | Minimal memory overhead | Does not support range queries (`<`, `>`, `BETWEEN`) |
| **GIN Index** | JSONB, arrays, and full-text search documents | Ultra-fast containment and search queries | Higher memory and write overhead |

---

## 10. Interview Questions

<details>
<summary>1. Why does an index not get used when a function is applied to the column in the WHERE clause?</summary>
The B-Tree index stores the original, unaltered values of the column in sorted order. When a function like <code>LOWER(email)</code> or <code>YEAR(created_at)</code> is applied, the database engine cannot map the sorted index keys to the transformed output without computing the function for every row, forcing a full table scan unless a functional/expression index is created.
</details>

<details>
<summary>2. Explain the Leftmost Prefix Rule in composite indexing.</summary>
A composite index on <code>(A, B, C)</code> is sorted first by <code>A</code>, then by <code>B</code> within matching <code>A</code>s, and finally by <code>C</code> within matching <code>B</code>s. The index can satisfy queries on <code>(A)</code>, <code>(A, B)</code>, or <code>(A, B, C)</code>, but cannot be used for queries filtering on <code>(B)</code> or <code>(C)</code> alone because their values are not sorted globally across the index.
</details>

<details>
<summary>3. What is the performance difference between OFFSET pagination and Keyset pagination?</summary>
<code>OFFSET N LIMIT M</code> requires the database engine to fetch, sort, and discard all preceding $N$ rows before returning $M$ rows, resulting in $O(N)$ execution time that becomes prohibitively slow on deep pages. Keyset pagination uses an index seek (<code>WHERE (id, timestamp) < (last_id, last_ts)</code>) to jump directly to the target record in $O(\log N)$ time regardless of page depth.
</details>

<details>
<summary>4. What is a covering index, and what is the benefit of the SQL INCLUDE clause?</summary>
A covering index contains all columns requested by a query's projections and predicates. The <code>INCLUDE</code> clause adds non-key payload columns strictly to the leaf pages of the B-tree without including them in branch search nodes, keeping the index tree compact while allowing the query to complete via an Index Only Scan without touching table heap pages.
</details>

<details>
<summary>5. How does SQL three-valued logic affect NULL comparisons and aggregations?</summary>
SQL evaluates boolean expressions as <code>TRUE</code>, <code>FALSE</code>, or <code>UNKNOWN</code>. Comparing any value to <code>NULL</code> using <code>=</code> or <code><></code> yields <code>UNKNOWN</code>. In <code>WHERE</code> clauses, only rows evaluating strictly to <code>TRUE</code> are returned. Additionally, standard aggregations like <code>COUNT(col)</code>, <code>AVG(col)</code>, and <code>SUM(col)</code> ignore <code>NULL</code> values completely, whereas <code>COUNT(*)</code> counts all rows regardless of NULLs.
</details>

<details>
<summary>6. When might a database optimizer choose a Full Table Scan even if an index exists?</summary>
The optimizer calculates cost based on estimated page I/Os. If a query matches a large percentage of total table rows (typically > 15-20%), reading the index and performing random I/O heap lookups for each row is more costly than scanning consecutive table blocks sequentially. Outdated table statistics can also cause the optimizer to misjudge selectivity.
</details>

<details>
<summary>7. What is the difference between clustered and non-clustered indexes?</summary>
In a clustered index (e.g., InnoDB primary key), table data rows are physically ordered and stored directly within the leaf nodes of the B-tree. A table can have only one clustered index. In a non-clustered (secondary) index, leaf pages contain indexed key values paired with row pointers (TIDs or primary key values) pointing to the actual data location.
</details>

<details>
<summary>8. How do database locks interact with unindexed foreign keys during DELETE operations?</summary>
When a row in a parent table is deleted or updated, the database must verify that no referencing rows exist in the child table. If the foreign key column on the child table is not indexed, the database may be forced to perform a full table scan or lock the entire child table, causing catastrophic concurrency bottlenecks and deadlocks.
</details>

<details>
<summary>9. What is write amplification in the context of database indexes?</summary>
Write amplification occurs because each INSERT or DELETE must modify not only the base table page, but also every single secondary B-Tree index page referencing that row. A table with 10 indexes requires 11 separate page write operations per insert, causing severe I/O bottlenecks in write-intensive workloads.
</details>

<details>
<summary>10. How does PostgreSQL MVCC handle row updates under the hood?</summary>
PostgreSQL treats every <code>UPDATE</code> as an insert of a new row version with an incremented <code>xmin</code> transaction ID and marks the older row's <code>xmax</code> with the current transaction ID. The old row becomes a dead tuple once all concurrent transactions that could observe it terminate, requiring the autovacuum daemon to reclaim its space.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Write an optimized schema with strict constraints for an eCommerce Order system.</summary>

```sql
CREATE TABLE customers (
    id            BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    email         VARCHAR(255) NOT NULL UNIQUE,
    full_name     VARCHAR(128) NOT NULL,
    created_at    TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE orders (
    id            BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    customer_id   BIGINT NOT NULL REFERENCES customers(id) ON DELETE RESTRICT,
    order_status  VARCHAR(24) NOT NULL CHECK (order_status IN ('PENDING', 'PAID', 'SHIPPED', 'CANCELLED')),
    total_amount  NUMERIC(12, 2) NOT NULL CHECK (total_amount >= 0.00),
    order_date    TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_orders_customer_date ON orders(customer_id, order_date DESC);
```
</details>

<details>
<summary>Exercise 2: Convert a deep offset pagination query to keyset pagination.</summary>

```sql
-- Original slow query:
-- SELECT id, order_date, total_amount FROM orders WHERE customer_id = 50 ORDER BY order_date DESC, id DESC LIMIT 10 OFFSET 10000;

-- Optimized Keyset Query:
SELECT id, order_date, total_amount
FROM orders
WHERE customer_id = 50
  AND (order_date, id) < ('2026-11-20 18:45:00+00', 84129)
ORDER BY order_date DESC, id DESC
LIMIT 10;
```
</details>

<details>
<summary>Exercise 3: Design a partial covering index to optimize active order dispatch queries.</summary>

```sql
-- Query:
-- SELECT id, total_amount FROM orders WHERE order_status = 'PAID' ORDER BY order_date ASC;

CREATE INDEX idx_orders_paid_dispatch
ON orders(order_date ASC)
INCLUDE (total_amount)
WHERE order_status = 'PAID';
```
</details>

<details>
<summary>Exercise 4: Explain why NOT EXISTS is preferred over NOT IN for nullable columns.</summary>

```sql
-- Demonstrating the safe NOT EXISTS pattern:
SELECT c.id, c.email
FROM customers c
WHERE NOT EXISTS (
    SELECT 1 
    FROM orders o 
    WHERE o.customer_id = c.id
);
-- NOT EXISTS uses two-valued Boolean logic (record either exists or it does not),
-- avoiding the three-valued UNKNOWN trap inherent to NOT IN when nulls are present.
```
</details>

<details>
<summary>Exercise 5: Design a schema table with audit timestamp triggers and optimistic locking version column.</summary>

```sql
CREATE TABLE inventory_items (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    sku         VARCHAR(64) NOT NULL UNIQUE,
    quantity    INT NOT NULL CHECK (quantity >= 0),
    version     BIGINT NOT NULL DEFAULT 0,
    updated_at  TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Optimistic lock update in application:
-- UPDATE inventory_items 
-- SET quantity = quantity - 1, version = version + 1, updated_at = CURRENT_TIMESTAMP
-- WHERE id = :id AND version = :version;
```
</details>

<details>
<summary>Exercise 6: Diagnose why an index on a VARCHAR column is ignored when querying with an integer literal.</summary>

```sql
-- Query: SELECT * FROM users WHERE phone_number = 1234567890;
-- Table definition: phone_number VARCHAR(20)

-- Why it fails: The database engine promotes the VARCHAR column to NUMERIC/INT 
-- by wrapping the column in an implicit type cast: CAST(phone_number AS BIGINT) = 1234567890.
-- This function conversion destroys index sargability.
-- Solution: Pass the parameter as a string: WHERE phone_number = '1234567890';
```
</details>

---

## 12. Summary / Cheat Sheet
- **Indexes**: Speed up reads at the cost of disk space and write latency (`INSERT`/`UPDATE`/`DELETE`).
- **Composite Index Order**: `(Equality_Columns, Range_Columns, Sort_Columns)`.
- **Sargable Predicates**: Avoid functions, conversions, and leading wildcards (`LIKE '%abc'`) on indexed columns.
- **Keyset Pagination**: Seek using `WHERE (col1, id) < (val1, last_id)` instead of `OFFSET`.
- **Constraints**: Enforce integrity declaratively (`PRIMARY KEY`, `FOREIGN KEY`, `UNIQUE`, `CHECK`, `NOT NULL`).
- **Money Handling**: Always use `NUMERIC`/`DECIMAL`, never `FLOAT` or `DOUBLE`.
- **Index Selectivity**: An index is most effective when column selectivity is high (many distinct values).
- **Buffer Cache**: Monitor shared hit vs read in `EXPLAIN (ANALYZE, BUFFERS)` to gauge disk I/O pressure.

---

## 13. Further Reading
- PostgreSQL Documentation: Indexes and EXPLAIN Command Architecture
- MySQL Performance Blog: The Leftmost Prefix Rule and Composite Index Optimization
- *Use The Index, Luke!* by Markus Winand (Comprehensive SQL Indexing Guide)
- *Designing Data-Intensive Applications* by Martin Kleppmann (Storage Engines and B-Trees)
- *Database Internals* by Alex Petrov (B-Tree Data Structures and Storage Engines)
