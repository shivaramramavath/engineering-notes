# 14: Data Modification: INSERT, UPDATE, DELETE & RETURNING

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [13 - Conditional Expressions: CASE, COALESCE & NULLIF](./13-case-and-conditionals.md) |
| **Target Audience** | Software Engineers, Backend Developers, Database Administrators |
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

Data Modification Language (DML) is the operational engine of every transaction processing (OLTP) application. Reading data (`SELECT`) is passive, but modifying data (`INSERT`, `UPDATE`, `DELETE`) changes the permanent state of the database, writes Write-Ahead Log (WAL) records, invalidates buffer caches, acquires row-level locks, and triggers integrity constraint validations.

Writing production-grade DML requires navigating critical engine mechanics:
- Executing an un-indexed `UPDATE` or `DELETE` can escalate into table-wide locking, freezing all application traffic.
- Updating rows in high-concurrency environments without proper transaction boundaries leads to lost updates and race conditions.
- Inserting rows one-by-one in an application loop introduces massive network latency, whereas multi-row batch inserts improve throughput by 100x.
- Capturing newly generated surrogate IDs or inspecting modified values historically required messy secondary queries (`SELECT LAST_INSERT_ID()`), but modern SQL provides the atomic **`RETURNING`** clause.

Mastering DML operations ensures that applications mutate state safely, atomically, and at peak throughput.

---

## 2. Core Concepts

### 1. The DML Operations
- **`INSERT`**: Appends new tuples into a relation. Supports single-row literals, multi-row batches, and bulk inserts derived from queries (`INSERT INTO ... SELECT`).
- **`UPDATE`**: Mutates existing row attributes in place. In MVCC engines (PostgreSQL), an update is physically implemented as an atomic **`DELETE` of the old tuple version followed by an `INSERT` of a new tuple version**.
- **`DELETE`**: Removes rows satisfying a `WHERE` predicate. In MVCC engines, it flags the row as dead (`xmax = current_xid`) without immediately freeing disk space.
- **`RETURNING`**: An ANSI/vendor extension that instructs the engine to return modified, inserted, or deleted column values directly to the client in the same round trip.

### 2. The Power of `RETURNING`
Prior to `RETURNING`, retrieving a generated primary key or inspecting modified values required two round trips:
1. `INSERT INTO orders ...;`
2. `SELECT LAST_INSERT_ID();` (Fragile across connection pools and concurrent threads!).

With `RETURNING`, the database returns the generated data atomically:
```sql
INSERT INTO orders (customer_id, total_amount) 
VALUES (101, 250.00) 
RETURNING order_id, created_at;
```

`RETURNING` can also capture data during updates and deletions:
- `UPDATE ... RETURNING new_balance`: Eliminates a separate read.
- `DELETE ... RETURNING *`: Moves deleted rows into application memory or archive tables in a single atomic step.

---

## 3. How It Works Internally

### MVCC Tuple Mutation (PostgreSQL Heap Updates)

What actually happens on disk when you run `UPDATE accounts SET balance = balance + 50 WHERE id = 1;`?

```
Before UPDATE (Page Block 42):
+--------------------------------------------------------------------------+
| Tuple 1: [xmin: 500, xmax: 0] id: 1, balance: 100.00                     |
+--------------------------------------------------------------------------+

After UPDATE (Current Transaction XID = 505):
+--------------------------------------------------------------------------+
| Tuple 1 (Old Version):                                                   |
|   xmin: 500, xmax: 505 (Dead to any transaction starting after XID 505)   |
|   t_ctid -> Points to (Block 42, Offset 2) [The New Tuple Version!]      |
|                                                                          |
| Tuple 2 (New Version at Offset 2):                                       |
|   xmin: 505, xmax: 0                                                     |
|   id: 1, balance: 150.00                                                 |
+--------------------------------------------------------------------------+
```

1. **Write Amplification**: The engine does NOT overwrite the old numbers in place (which would break concurrent transactions reading under Snapshot Isolation). It writes a completely new tuple to disk!
2. **Locking**: The engine acquires an **Exclusive Row Lock** on the tuple. Any concurrent transaction attempting to `UPDATE` or `DELETE` the same row is blocked until transaction 505 commits or rolls back.
3. **WAL Logging**: Before the modified buffer page is flushed to disk, the mutation is appended to the Write-Ahead Log (WAL) to guarantee durability.

---

## 4. Syntax & Usage

### Core DML Syntax

```sql
-- 1. Multi-Row Batch Insert with RETURNING
INSERT INTO customers (first_name, last_name, email)
VALUES 
    ('Siddharth', 'Nair', 'siddharth@example.com'),
    ('Chloe', 'Dubois', 'chloe@example.com')
RETURNING customer_id, created_at;

-- 2. Insert from Query (Bulk Population)
INSERT INTO customer_archives (customer_id, email, archived_at)
SELECT customer_id, email, CURRENT_TIMESTAMP
FROM customers
WHERE created_at < '2020-01-01';

-- 3. Update with Calculations and Filters
UPDATE accounts
SET 
    balance = balance - 150.00,
    updated_at = CURRENT_TIMESTAMP
WHERE account_id = 42 AND balance >= 150.00
RETURNING balance AS new_balance;

-- 4. Delete with RETURNING
DELETE FROM cart_items
WHERE cart_id = 901
RETURNING item_id, product_id, quantity;
```

### Multi-Dialect Comparison Matrix

| DML Capability | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Multi-Row `INSERT ... VALUES`** | Supported | Supported | Supported | Supported |
| **`RETURNING` Clause** | Full support (`INSERT`, `UPDATE`, `DELETE`)| Not supported (use `LAST_INSERT_ID()`)| Supported (since 3.35)| `OUTPUT INSERTED.col / DELETED.col` |
| **`UPDATE ... FROM` (Join Update)**| Supported (`UPDATE a SET ... FROM b WHERE ...`)| Supported (`UPDATE a JOIN b ON ... SET ...`)| Supported (3.33+)| Supported (`UPDATE a SET ... FROM a JOIN b`)|
| **`DELETE ... USING` (Join Delete)**| Supported (`DELETE FROM a USING b WHERE ...`)| Supported (`DELETE a FROM a JOIN b ...`)| Not supported (use subquery)| Supported (`DELETE a FROM a JOIN b ...`)|
| **`LIMIT` in `UPDATE` / `DELETE`** | Not supported (use subquery or ctid)| Supported (`UPDATE ... LIMIT 10`)| Supported (if compiled with options)| Supported (`TOP (10)`) |

---

## 5. 5 Practical Progressive Examples

### Example 1: Multi-Row Batch Insertion with Atomic ID Capture
Inserting multiple orders and capturing generated keys in a single network round-trip:

```sql
INSERT INTO products (category_id, product_name, retail_price, stock_qty)
VALUES 
    (2, 'UltraBook Pro 14', 1299.00, 15),
    (3, 'Pixel 9a', 499.00, 30),
    (4, 'AirPods Max 2', 549.00, 20)
RETURNING product_id, product_name, retail_price;
```
```text
+------------+------------------+--------------+
| product_id | product_name     | retail_price |
+------------+------------------+--------------+
|        207 | UltraBook Pro 14 |      1299.00 |
|        208 | Pixel 9a         |       499.00 |
|        209 | AirPods Max 2    |       549.00 |
+------------+------------------+--------------+
```

### Example 2: Update with Join (`UPDATE ... FROM`)
Updating product inventory based on line items in a newly completed order:

```sql
-- In PostgreSQL & SQLite 3.33+:
UPDATE products p
SET stock_qty = p.stock_qty - oi.quantity
FROM order_items oi
WHERE oi.order_id = 1001 
  AND p.product_id = oi.product_id
RETURNING p.product_id, p.product_name, p.stock_qty AS remaining_stock;
```
```text
+------------+---------------------------------------+-----------------+
| product_id | product_name                          | remaining_stock |
+------------+---------------------------------------+-----------------+
|        201 | Framework Laptop 16                   |              24 |
|        205 | Designing Data-Intensive Applications |             119 |
+------------+---------------------------------------+-----------------+
```

### Example 3: Edge Case (Safe Idempotent Currency Transfer)
Transferring $100 between two bank accounts atomically with balance checks to prevent overdrafts:

```sql
BEGIN;

-- 1. Deduct from Source Account (Only if balance is sufficient!)
UPDATE accounts
SET balance = balance - 100.00
WHERE account_id = 101 AND balance >= 100.00
RETURNING balance;

-- In application code: If 0 rows updated, ROLLBACK and abort!

-- 2. Credit Destination Account
UPDATE accounts
SET balance = balance + 100.00
WHERE account_id = 202;

COMMIT;
```
*(The `balance >= 100.00` check inside the `UPDATE WHERE` acts as an atomic optimistic concurrency guard).*

### Example 4: Failure Mode (Accidental Table-Wide UPDATE / DELETE)
What happens when you forget the `WHERE` clause:

```sql
-- DISASTER: Forgetting WHERE updates EVERY row in the entire table!
UPDATE accounts SET balance = 0.00;
-- UPDATE 10000000 (Wipes every customer balance!)
```
In `psql`, protect against this by setting:
```sql
\set AUTOCOMMIT off
```
Or configuring server-level safety or client flags. In MySQL:
```sql
SET sql_safe_updates = 1;
-- Prevents UPDATE or DELETE statements that lack a WHERE clause using an indexed key!
```

### Example 5: Moving Rows Atomically via Data-Modifying CTE
PostgreSQL allows combining `DELETE ... RETURNING` with `INSERT` inside a Common Table Expression to move archived records in a single atomic transaction:

```sql
WITH DeletedEvents AS (
    -- 1. Delete rows older than 1 year and capture them in memory
    DELETE FROM audit_logs
    WHERE logged_at < CURRENT_TIMESTAMP - INTERVAL '1 year'
    RETURNING log_id, event_name, logged_at
)
-- 2. Insert the deleted rows directly into the archive table!
INSERT INTO audit_logs_cold_archive (log_id, event_name, logged_at, archived_at)
SELECT log_id, event_name, logged_at, CURRENT_TIMESTAMP
FROM DeletedEvents;
```
*(100% atomic: records cannot be deleted without being inserted into the archive, and zero duplicate rows can exist).*

---

## 6. Best Practices

1. **Always Use Batch Inserts in Application Code**: Never execute single-row `INSERT` statements inside a loop (`for item in items: db.execute(...)`). Batch them into multi-row inserts (`INSERT INTO ... VALUES (...), (...)`) in chunks of 500 to 1,000 rows.
2. **Leverage `RETURNING` to Eliminate Round Trips**: Always append `RETURNING id` to `INSERT` statements to capture generated primary keys, avoiding separate `SELECT` queries.
3. **Always Include Optimistic Concurrency Checks on `UPDATE`**: In high-concurrency environments, guard balance deductions and state updates: `WHERE id = ? AND balance >= ? AND version = ?`.
4. **Wrap Multi-Statement Mutations in Explicit Transactions**: Never execute related updates without `BEGIN ... COMMIT`. If the second update fails, the first update remains committed, causing financial imbalances.
5. **Always Test `DELETE` with `SELECT` First**: Before running `DELETE FROM tbl WHERE <complex_condition>`, run `SELECT COUNT(*) FROM tbl WHERE <complex_condition>` to verify the exact scope of affected rows.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Omitting `WHERE` in `UPDATE` or `DELETE`** | Wipes or alters every single row in the table permanently. | Use `WHERE id = ...`, enable `sql_safe_updates`, or test inside a rolled-back transaction. |
| **Iterative Single-Row Inserts in Loops** | Causes $N$ separate network round trips, slowing ingestion by 100x. | Use multi-row batch inserts (`INSERT INTO ... VALUES (...), (...)`). |
| **Using `SELECT LAST_INSERT_ID()` Across Pools** | Connection pooling threads can swap connections, returning the wrong thread's generated ID. | Use `RETURNING id` in PostgreSQL/SQLite or capture within an unbroken session. |
| **Updating Primary Keys** | Triggers expensive cascading updates across all foreign key tables and splits B-Tree index pages. | Treat primary keys as immutable surrogate IDs. |
| **Missing Indexes on `UPDATE` / `DELETE` Predicates** | An `UPDATE` with an unindexed `WHERE` clause acquires exclusive row locks on every page it scans, blocking concurrent readers. | Ensure columns used in `WHERE` clauses of updates/deletes are indexed. |
| **Assuming `UPDATE` Modifies Data in Place** | In PostgreSQL MVCC, an update appends a brand-new tuple, creating dead tuple bloat if not vacuumed. | Tune autovacuum for write-heavy tables. |

---

## 8. Performance & Security Considerations

### Performance
- **Write Amplification in Indexes**: When an `UPDATE` modifies an indexed column, the database must write a new row to the table heap AND insert new index entries into every index covering that column. Only update columns that have genuinely changed.
- **Batching Large Deletions**: Deleting 10,000,000 rows in a single `DELETE` statement generates massive WAL spikes and holds locks for minutes. Delete in batches of 10,000:
  ```sql
  DELETE FROM events WHERE id IN (
      SELECT id FROM events WHERE created_at < '2023-01-01' LIMIT 10000
  );
  ```

### Security
- **Data Modification Injection**:
  - String concatenation in `UPDATE` statements is catastrophic:
    ```sql
    "UPDATE users SET email = '" + email + "' WHERE id = " + userId;
    -- Injected email: admin@test.com', is_admin = TRUE WHERE id = 1; --
    ```
  - Always use prepared statements with parameterized inputs.

---

## 9. Comparison Matrix

### Update & Delete Join Syntax Across Dialects

| Action | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Join Update Syntax** | `UPDATE a SET col = b.val FROM b WHERE a.id = b.id` | `UPDATE a JOIN b ON a.id = b.id SET a.col = b.val` | `UPDATE a SET col = b.val FROM b WHERE a.id = b.id` | `UPDATE a SET col = b.val FROM a JOIN b ON a.id = b.id` |
| **Atomic Returning** | `RETURNING col` | Not supported | `RETURNING col` | `OUTPUT INSERTED.col` |
| **Delete Join Syntax** | `DELETE FROM a USING b WHERE a.id = b.id` | `DELETE a FROM a JOIN b ON a.id = b.id` | Use `WHERE id IN (SELECT ...)` | `DELETE a FROM a JOIN b ON a.id = b.id` |
| **Transactional DML** | Fully ACID rollbackable | Fully ACID rollbackable | Fully ACID rollbackable | Fully ACID rollbackable |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: How does PostgreSQL physically execute an `UPDATE` statement under MVCC?
<details>
<summary>Answer</summary>
In PostgreSQL's MVCC implementation, an `UPDATE` is physically executed as an atomic **`DELETE` followed by an `INSERT`**:
1. The engine locates the existing tuple and sets its `xmax` header field to the current transaction ID ($XID$), marking it dead to any transaction starting after this one.
2. It constructs a brand-new tuple with the updated values, sets its `xmin` header field to the current $XID$, and writes it to an available page in the table heap.
3. The old tuple’s `t_ctid` pointer is updated to point to the physical block and offset of the new tuple.
4. Secondary indexes are updated to point to the new tuple (unless the Heap-Only Tuples / HOT optimization applies).
</details>

#### Q2: What is the Heap-Only Tuple (HOT) optimization in PostgreSQL updates?
<details>
<summary>Answer</summary>
HOT is a critical optimization designed to eliminate index write amplification during updates.
When a row is updated:
1. If the updated column is **NOT indexed**, and
2. If sufficient free space exists within the **exact same 8KB disk page** as the old tuple:
PostgreSQL places the new tuple on the same page and chains it from the old tuple. 

Because the row did not move to a different page and indexed columns did not change, **zero secondary indexes require updates**! The index continues pointing to the old tuple, which immediately redirects the executor to the new version, cutting write I/O drastically.
</details>

#### Q3: Why is `DELETE FROM table WHERE ...` significantly slower than `TRUNCATE TABLE`?
<details>
<summary>Answer</summary>
- `DELETE`: Iterates row-by-row through the table. It evaluates constraints, fires triggers, updates `xmax` headers, writes a WAL entry for every single deleted row, and leaves dead tuples on disk requiring subsequent vacuuming.
- `TRUNCATE`: Operates at the file system level. It acquires an `AccessExclusiveLock`, allocates a new empty storage file (`relfilenode`), updates the system catalog pointer, and instructs the operating system to unlink the old file. It bypasses row-by-row logging and completes in milliseconds.
</details>

#### Q4: How does the `RETURNING` clause work, and what are its performance advantages?
<details>
<summary>Answer</summary>
The `RETURNING` clause instructs the query executor to project specified columns from the modified or generated tuple directly into the output stream during the DML execution phase.

Advantages:
1. **Eliminates Network Latency**: Combines write and read into a single round-trip.
2. **Guarantees Atomicity**: Returns the exact state generated by that specific transaction, eliminating race conditions across connection pools.
3. **Captures Default & Computed Values**: Immediately returns values generated by database sequences, identity columns, and default expressions.
</details>

### Practical DML Puzzles

#### Q5: Write an SQL query that atomically deletes inactive users who haven't logged in for 2 years, archiving them into an `inactive_users_archive` table in a single atomic step.
<details>
<summary>Answer</summary>

```sql
WITH PurgedUsers AS (
    DELETE FROM users
    WHERE last_login < CURRENT_TIMESTAMP - INTERVAL '2 years'
    RETURNING user_id, username, email, last_login
)
INSERT INTO inactive_users_archive (user_id, username, email, last_login, archived_at)
SELECT user_id, username, email, last_login, CURRENT_TIMESTAMP
FROM PurgedUsers;
```
</details>

#### Q6: Write an `UPDATE` statement that increases the price of all products in the `'Electronics'` category by 5%, using a join against the `categories` table.
<details>
<summary>Answer</summary>

```sql
-- In PostgreSQL & SQLite 3.33+:
UPDATE products p
SET retail_price = ROUND(p.retail_price * 1.05, 2)
FROM categories c
WHERE p.category_id = c.category_id
  AND c.category_name = 'Electronics';
```
</details>

#### Q7: Write an atomic SQL statement that claims the next unprocessed job from a `job_queue` table, marking it as `'processing'`, ensuring no two concurrent workers claim the same job.
<details>
<summary>Answer</summary>

```sql
UPDATE job_queue
SET 
    status = 'processing',
    locked_by = 'worker_01',
    locked_at = CURRENT_TIMESTAMP
WHERE job_id = (
    SELECT job_id
    FROM job_queue
    WHERE status = 'queued'
    ORDER BY priority DESC, created_at ASC
    LIMIT 1
    FOR UPDATE SKIP LOCKED
)
RETURNING job_id, payload;
```
*(Notice `FOR UPDATE SKIP LOCKED`, which allows multiple concurrent workers to lock disjoint jobs simultaneously without blocking each other).*
</details>

#### Q8: How can you delete 5 million old rows from a high-traffic table without causing lock escalation or exhausting replication log space?
<details>
<summary>Answer</summary>
Delete in iterative bounded chunks inside a procedural loop or batch script:

```sql
-- Execute repeatedly until 0 rows are deleted:
DELETE FROM audit_logs
WHERE log_id IN (
    SELECT log_id
    FROM audit_logs
    WHERE logged_at < '2023-01-01'
    LIMIT 5000
);
```
*(Pausing for 100ms between batches allows replication lag to clear and prevents lock starvation).*
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Multi-Row Insert with Returning
Insert three new categories (`'Gardening'`, `'Toys'`, `'Automotive'`) into the `categories` table, capturing their generated `category_id` values.
<details>
<summary>Solution</summary>

```sql
INSERT INTO categories (category_name)
VALUES ('Gardening'), ('Toys'), ('Automotive')
RETURNING category_id, category_name;
```
</details>

#### Exercise 2: Targeted Conditional Update
Update all `'pending'` orders placed before `'2024-01-01'` to status `'cancelled'`, updating the `updated_at` timestamp.
<details>
<summary>Solution</summary>

```sql
UPDATE orders
SET 
    order_status = 'cancelled',
    updated_at = CURRENT_TIMESTAMP
WHERE order_status = 'pending' 
  AND order_date < '2024-01-01';
```
</details>

### Intermediate Exercises

#### Exercise 3: Safe Account Balance Deduction
Write an update that subtracts $50.00 from an account's balance, but only if the balance is at least $50.00 and the account is active. Return the new balance.
<details>
<summary>Solution</summary>

```sql
UPDATE accounts
SET balance = balance - 50.00
WHERE account_id = 101 
  AND is_active = TRUE 
  AND balance >= 50.00
RETURNING balance AS updated_balance;
```
</details>

#### Exercise 4: Clean Up Orphaned Rows via Delete Join
Delete all rows from an `order_items` table where the associated `order_id` no longer exists in the `orders` table.
<details>
<summary>Solution</summary>

```sql
DELETE FROM order_items
WHERE order_id NOT IN (SELECT order_id FROM orders);

-- Or using NOT EXISTS:
DELETE FROM order_items oi
WHERE NOT EXISTS (
    SELECT 1 FROM orders o WHERE o.order_id = oi.order_id
);
```
</details>

### Advanced Exercises

#### Exercise 5: Atomic Delete and Re-Insert Cycle
Write a CTE that deletes all tags associated with article 42 and immediately inserts three new tags (`'sql'`, `'performance'`, `'indexing'`) for article 42 in a single atomic transaction.
<details>
<summary>Solution</summary>

```sql
WITH DeletedTags AS (
    DELETE FROM article_tags
    WHERE article_id = 42
)
INSERT INTO article_tags (article_id, tag_name)
VALUES 
    (42, 'sql'),
    (42, 'performance'),
    (42, 'indexing');
```
</details>

#### Exercise 6: Profiling Heap-Only Tuple (HOT) Updates
Create a table with one indexed column and one unindexed column. Perform 100 updates on the unindexed column and inspect `pg_stat_user_tables` to verify that `n_tup_hot_upd` equals 100.
<details>
<summary>Solution</summary>

```sql
CREATE TABLE hot_test (id INT PRIMARY KEY, counter INT);
INSERT INTO hot_test VALUES (1, 0);

-- Perform updates on unindexed column 'counter':
DO $$
BEGIN
  FOR i IN 1..100 LOOP
    UPDATE hot_test SET counter = counter + 1 WHERE id = 1;
  END LOOP;
END $$;

-- Inspect HOT update count:
SELECT relname, n_tup_upd, n_tup_hot_upd 
FROM pg_stat_user_tables 
WHERE relname = 'hot_test';
```
*(Notice `n_tup_hot_upd = 100`, confirming zero index write amplification).*
</details>

---

## 12. Summary & Quick Reference

### DML Operations Cheatsheet
- **Batch Insert**: `INSERT INTO tbl (cols) VALUES (...), (...) RETURNING ids;`
- **Update with Join**: `UPDATE a SET col = b.val FROM b WHERE a.id = b.id RETURNING a.col;`
- **Delete with Returning**: `DELETE FROM tbl WHERE cond RETURNING *;`
- **MVCC Invariant**: Updates in Postgres create a new tuple version and mark the old one dead (`xmax = xid`).
- **HOT Update**: Updates on unindexed columns on the same page bypass secondary index writes.
- **Safety**: Always test deletes/updates with `SELECT` first; avoid unbound operations without `WHERE`.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [Data Manipulation (INSERT, UPDATE, DELETE)](https://www.postgresql.org/docs/17/dml.html)
- PostgreSQL Documentation: [The RETURNING Clause](https://www.postgresql.org/docs/17/dml-returning.html)
- Markus Winand: *"How to Update and Delete Efficiently"* (Use The Index, Luke)
- [15 - Upsert Patterns: ON CONFLICT & MERGE](./15-upsert-and-merge.md)
