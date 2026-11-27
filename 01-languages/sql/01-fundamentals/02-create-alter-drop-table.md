# 02: Table Definition: CREATE, ALTER, DROP & TRUNCATE

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [01 - Relational Data Types](./01-data-types.md) |
| **Target Audience** | Software Engineers, Database Administrators, DevOps / SRE |
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

Data Definition Language (DDL) is the subset of SQL responsible for creating, modifying, and tearing down the structural schema of a relational database. While writing queries (DML) touches existing data within predefined boundaries, executing DDL commands alters the actual blueprint of the database catalog, acquires heavyweight table locks, and can trigger massive physical table rewrites on disk.

In modern continuous delivery and zero-downtime architectures, executing DDL naively can take down an entire production system. Running `ALTER TABLE ADD COLUMN ... DEFAULT ...` on a 500-million-row table in an older database version might lock the table exclusively for hours, queuing up all incoming application traffic until connection pools saturate and services crash.

Understanding the mechanics of `CREATE TABLE`, `ALTER TABLE`, `DROP TABLE`, and `TRUNCATE TABLE`—including catalog mutations, lock modes, metadata-only fast alters, and foreign key cascades—is essential for any engineer deploying database migrations to live environments.

---

## 2. Core Concepts

### 1. The DDL Lifecycle Operations
- **`CREATE TABLE`**: Instantiates a new relational table definition in system catalogs and allocates initial disk pages.
- **`CREATE TABLE AS SELECT` (CTAS)**: Creates a new table and immediately populates it with the result of a query in a single atomic operation. Does not automatically copy indexes or constraints!
- **`ALTER TABLE`**: Modifies the structure of an existing table: adding/dropping columns, changing data types, setting defaults, and attaching/detaching constraints.
- **`DROP TABLE`**: Removes the table definition from system catalogs and marks physical heap files for deletion on disk.
- **`TRUNCATE TABLE`**: Fast, non-logged or minimally logged removal of *all* rows from a table while preserving the table schema, indexes, and constraints.

### 2. Lock Hierarchy: Why DDL Can Block Production
Relational engines protect schema integrity using lock hierarchies. While queries (`SELECT`) acquire lightweight `AccessShareLock` (which do not conflict with each other), DDL commands generally require an **`AccessExclusiveLock`**:
- An `AccessExclusiveLock` conflicts with *all other lock modes*, including simple `SELECT` queries!
- Even a fast metadata-only `ALTER TABLE` must wait for any long-running `SELECT` query to finish before acquiring the lock.
- While the `ALTER TABLE` is waiting in the lock queue, all subsequent incoming queries are blocked behind it!

```
Incoming Traffic Queue:
[SELECT #1 (Running: 30s)] ──► Holds AccessShareLock
        ▲
[ALTER TABLE ADD COLUMN]   ──► Waiting for AccessExclusiveLock (BLOCKED!)
        ▲
[SELECT #2 (Incoming)]     ──► BLOCKED behind ALTER TABLE!
[SELECT #3 (Incoming)]     ──► BLOCKED behind ALTER TABLE! (Connection pool exhausts!)
```

### 3. Metadata-Only Alter vs Physical Table Rewrite
- **Metadata-Only Alter (Instant)**: Modifies only rows in `pg_attribute` or `pg_class`. Takes milliseconds regardless of whether the table has 10 rows or 1 billion rows.
  - Examples in PostgreSQL 11+: Adding a column with a constant `DEFAULT`, adding a nullable column, renaming a column or table.
- **Table Rewrite (Slow & Heavy)**: The engine must create a brand-new physical file on disk, read every existing tuple, convert it to the new binary format, and copy it over.
  - Examples: Changing a column data type from `INT` to `BIGINT`, adding a column with a volatile default (`DEFAULT random()`).

---

## 3. How It Works Internally

### `TRUNCATE` vs `DELETE` Internal Mechanics

Understanding why `TRUNCATE` takes 5 milliseconds while `DELETE FROM table;` can take 20 minutes on a 50-million-row table:

```
DELETE Operation:                                TRUNCATE Operation:
+------------------------------------+           +------------------------------------+
| 1. Scan every 8KB disk page        |           | 1. Acquire AccessExclusiveLock     |
| 2. For each tuple:                 |           | 2. Create brand-new empty file     |
|    - Write dead tuple header (xmax)|           |    relfilenode on disk             |
|    - Generate WAL record per row   |           | 3. Update pg_class catalog pointer |
|    - Fire ON DELETE triggers       |           | 4. Old data files unlinked by OS   |
| 3. Heap pages remain on disk       |           | 5. Indexes recreated empty         |
|    (Causes massive table bloat!)   |           | Fast, atomic, zero bloat!          |
+------------------------------------+           +------------------------------------+
```

- In `DELETE`, the engine must maintain MVCC guarantees: it cannot physically destroy data until all concurrent transactions finish. It marks rows as dead tuples, generates millions of WAL entries, and leaves empty space that requires `VACUUM` to reclaim.
- In `TRUNCATE`, the engine allocates a new disk file identifier (`relfilenode`), updates the catalog to point to it, and instructs the operating system to unlink the old storage files.

---

## 4. Syntax & Usage

### Standard ANSI DDL Statements

```sql
-- 1. Create Table with complete constraint specifications
CREATE TABLE IF NOT EXISTS inventory_locations (
    location_id INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    warehouse_code VARCHAR(20) NOT NULL UNIQUE,
    aisle_number SMALLINT NOT NULL CHECK (aisle_number > 0),
    is_refrigerated BOOLEAN DEFAULT FALSE NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL
);

-- 2. Alter Table: Safe Column Additions and Modifications
ALTER TABLE inventory_locations 
    ADD COLUMN max_weight_kg NUMERIC(10, 2) DEFAULT 1000.00 NOT NULL;

-- 3. Alter Table: Renaming
ALTER TABLE inventory_locations 
    RENAME COLUMN aisle_number TO aisle_idx;

-- 4. Truncate Table with Identity Reset
TRUNCATE TABLE inventory_locations RESTART IDENTITY;

-- 5. Drop Table with Safety Checks
DROP TABLE IF EXISTS inventory_locations CASCADE;
```

### Multi-Dialect Comparison Matrix

| DDL Capability | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Transactional DDL** | Full (DDL statements run inside `BEGIN ... ROLLBACK`)| No (DDL causes implicit auto-commit!) | Full (DDL can be rolled back in transactions)| Full (DDL runs in transactions) |
| **Instant Add Column with Default** | Instant (since Postgres 11 via catalog metadata) | Instant (via `ALGORITHM=INSTANT`) | Instant | Instant (since SQL Server 2012) |
| **Identity Reset on Truncate** | `TRUNCATE tbl RESTART IDENTITY;` | Automatically resets `AUTO_INCREMENT` | `DELETE FROM tbl; DELETE FROM sqlite_sequence WHERE name='tbl';` | Automatically resets `IDENTITY` |
| **`CREATE TABLE AS SELECT`** | Full support (`WITH DATA` / `WITH NO DATA`) | Full support | Full support | `SELECT ... INTO new_table FROM ...` |
| **Drop Column Support** | Full | Full | Supported (since SQLite 3.35) | Full |
| **Drop Table Cascade** | `DROP TABLE tbl CASCADE;` | Drops foreign keys if specified | Foreign keys drop automatically | Must drop foreign key constraints first |

---

## 5. 5 Practical Progressive Examples

### Example 1: Creating a Table from Query Results (CTAS)
Create a summary reporting table populated directly from an existing transactional dataset.

```sql
-- Create an analytics snapshot table
CREATE TABLE top_selling_products AS
SELECT 
    p.product_id,
    p.product_name,
    COUNT(oi.item_id) AS times_ordered,
    SUM(oi.quantity * oi.unit_price) AS gross_revenue
FROM products p
JOIN order_items oi ON p.product_id = oi.product_id
GROUP BY p.product_id, p.product_name
ORDER BY gross_revenue DESC;

-- Verify created schema and data
SELECT * FROM top_selling_products LIMIT 3;
```
```text
+------------+---------------------------------------+---------------+---------------+
| product_id | product_name                          | times_ordered | gross_revenue |
+------------+---------------------------------------+---------------+---------------+
|        201 | Framework Laptop 16                   |             1 |       1399.00 |
|        202 | MacBook Air M3                        |             1 |       1099.00 |
|        203 | Pixel 9 Pro                           |             1 |        999.00 |
+------------+---------------------------------------+---------------+---------------+
```
*(Warning: `CREATE TABLE AS SELECT` copies column names and types, but does NOT copy primary keys, indexes, foreign keys, or defaults! Those must be added explicitly).*

### Example 2: Transactional DDL (Rolling Back a Schema Migration)
PostgreSQL and SQLite support fully transactional DDL. If a migration script fails, the entire schema modification rolls back atomically!

```sql
BEGIN;

-- 1. Create a new table
CREATE TABLE feature_flags (
    flag_id INT PRIMARY KEY,
    flag_key VARCHAR(50) NOT NULL UNIQUE
);

-- 2. Add an audit column
ALTER TABLE feature_flags ADD COLUMN enabled BOOLEAN DEFAULT FALSE;

-- 3. Simulate an error or rollback
ROLLBACK;

-- Verify table was NOT created:
SELECT * FROM feature_flags;
-- ERROR: relation "feature_flags" does not exist
```
*(In MySQL, executing `CREATE TABLE` or `ALTER TABLE` immediately issues an invisible `COMMIT`, making DDL rollbacks impossible).*

### Example 3: Edge Case (Safe Zero-Downtime Column Addition with Lock Timeouts)
In high-throughput systems, waiting indefinitely for an exclusive table lock queues up queries and brings down the application. We use `lock_timeout`:

```sql
-- Step 1: Set a strict lock timeout so we don't queue traffic if blocked
SET lock_timeout = '2s';

-- Step 2: Add column with constant default (instant in Postgres 11+)
ALTER TABLE orders 
    ADD COLUMN is_priority BOOLEAN DEFAULT FALSE NOT NULL;

-- Reset lock timeout
RESET lock_timeout;
```
If another transaction holds a lock on `orders` for more than 2 seconds, the `ALTER TABLE` aborts with an error instead of blocking the entire site.

### Example 4: Failure Mode (Foreign Key Truncation Error)
Attempting to truncate a parent table referenced by child foreign keys without cascading:

```sql
-- Attempting to truncate parent table 'categories' while 'products' has foreign keys
TRUNCATE TABLE categories;
```
```text
ERROR: cannot truncate a table referenced in a foreign key constraint
DETAIL: Table "products" references "categories".
HINT: Truncate table "products" at the same time, or use TRUNCATE ... CASCADE.
```
To safely truncate both atomically:
```sql
TRUNCATE TABLE categories, products RESTART IDENTITY;
```
Or with automatic child cascade:
```sql
TRUNCATE TABLE categories RESTART IDENTITY CASCADE;
```

### Example 5: Optimized Two-Step Constraint Validation
Adding a `CHECK` constraint to a 100-million-row table requires scanning every row to verify data. Doing this in a single `ALTER TABLE ADD CONSTRAINT` holds an `AccessExclusiveLock` for minutes.

**The Production Zero-Downtime Pattern**:
```sql
-- Step 1: Add the constraint NOT VALID (Instant metadata-only lock!)
ALTER TABLE orders 
    ADD CONSTRAINT chk_positive_total 
    CHECK (total_amount >= 0.00) NOT VALID;

-- At this point, new INSERTs and UPDATEs are strictly enforced,
-- but historical rows haven't been scanned yet. Lock is released instantly.

-- Step 2: Validate the constraint in the background
-- This acquires only a ShareUpdateExclusiveLock (allows concurrent SELECT, INSERT, UPDATE, DELETE!)
ALTER TABLE orders 
    VALIDATE CONSTRAINT chk_positive_total;
```
This pattern allows safely validating constraints on billion-row tables without downtime!

---

## 6. Best Practices

1. **Always Set `lock_timeout` in Migration Scripts**: Never run raw DDL on a production database without setting `SET lock_timeout = '3s';`. If the lock cannot be acquired within 3 seconds, fail fast rather than creating a connection pool pile-up.
2. **Use Two-Phase Constraint Addition for Large Tables**: Add constraints with `NOT VALID` first, then validate them with `VALIDATE CONSTRAINT` to prevent long-running table locks.
3. **Always Include `IF EXISTS` / `IF NOT EXISTS` in Automated Migrations**: Idempotent DDL scripts (`CREATE TABLE IF NOT EXISTS`, `DROP TABLE IF EXISTS`) can be safely re-run without blowing up if partially applied.
4. **Never Use `TRUNCATE` in Multi-Table Applications Without Reviewing Cascades**: `TRUNCATE ... CASCADE` will recursively wipe all child tables referencing the target table.
5. **Remember CTAS Does Not Copy Constraints**: When using `CREATE TABLE AS SELECT`, explicitly declare primary keys, foreign keys, and indexes on the newly created table.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Expecting DDL to Rollback in MySQL** | MySQL issues an implicit `COMMIT` before and after every DDL statement. If statement 3 of 5 fails, the first 2 remain committed! | In MySQL, write idempotent defensive scripts and plan for manual rollback compensation. |
| **Adding a Column with a Volatile Default** | Adding a column with `DEFAULT clock_timestamp()` or `DEFAULT random()` forces a full table rewrite on disk because values differ per row. | Use constant defaults (`DEFAULT 'active'`) which are instant metadata-only updates. |
| **Forgetting Indexes on CTAS Tables** | `CREATE TABLE ... AS SELECT` creates bare heaps without primary keys or indexes, resulting in slow full table scans later. | Run `ALTER TABLE new_tbl ADD PRIMARY KEY (id);` immediately after CTAS. |
| **Dropping a Table Without `CASCADE` Handling** | `DROP TABLE parent;` throws an error if child tables have foreign keys pointing to it. | Evaluate dependent child tables, then use `DROP TABLE parent CASCADE;` cautiously. |
| **Running Heavy Alters Without `statement_timeout`** | An unchecked migration script can lock tables indefinitely if blocked by an uncommitted transaction. | Set `SET lock_timeout = '5s'; SET statement_timeout = '60s';` before running migrations. |
| **Using `DELETE` Instead of `TRUNCATE` to Clear Tables** | `DELETE FROM big_table;` scans every row, logs millions of WAL entries, and leaves dead tuple bloat. | Use `TRUNCATE TABLE big_table;` for bulk table purges. |

---

## 8. Performance & Security Considerations

### Performance
- **Instant DDL in Modern Engines**:
  - In PostgreSQL 11+, `ALTER TABLE tbl ADD COLUMN col INT DEFAULT 0;` does not touch table heap pages. It records the default value in `pg_attribute`. When reading older rows that lack the column, the executor transparently substitutes the catalog default value!
- **Table Bloat After `TRUNCATE`**:
  - `TRUNCATE` immediately resets disk allocations and frees space back to the operating system, unlike `DELETE` which requires a vacuum.

### Security
- **DDL Permissions Separation**:
  - Never grant `CREATE`, `DROP`, or `ALTER` privileges to standard application database users. Application connection roles should only possess `SELECT`, `INSERT`, `UPDATE`, and `DELETE`. DDL must be restricted to a specialized migration deployment role.

---

## 9. Comparison Matrix

### Table Purging Strategies Compared

| Strategy | Speed on 10M Rows | Transaction Safety | Resets Auto-Increment | Fires Triggers | Storage Reclaimed |
|:---|:---:|:---:|:---:|:---:|:---:|
| **`DELETE FROM table;`** | Minutes (Slow) | Full (Rollbackable) | No | Yes (`ON DELETE`) | No (Creates dead tuples) |
| **`TRUNCATE TABLE;`** | Milliseconds (Instant) | Full (in Postgres/SQLite) | Yes (`RESTART IDENTITY`) | No (Fires truncate trigger only) | Yes (Unlinks old disk files) |
| **`DROP TABLE;`** | Milliseconds (Instant) | Full (in Postgres/SQLite) | Schema destroyed | No | Yes (Files unlinked by OS) |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: Why does executing an `ALTER TABLE` statement on a busy production table cause an outage even if the alter itself only takes 10 milliseconds?
<details>
<summary>Answer</summary>
`ALTER TABLE` requires an `AccessExclusiveLock`. This lock cannot be granted until *all* currently active queries (which hold `AccessShareLock`) finish executing.

If a long-running read query is currently active (e.g., taking 30 seconds), the `ALTER TABLE` enters the lock queue waiting for it. However, the database lock manager queues all subsequent incoming queries behind the waiting `ALTER TABLE` to prevent lock starvation. 

Consequently, hundreds of incoming application requests are blocked, exhausting the connection pool and causing service timeouts—even though the alter itself takes only 10ms once acquired!
</details>

#### Q2: What is the difference between a metadata-only DDL operation and a physical table rewrite?
<details>
<summary>Answer</summary>
- **Metadata-Only DDL**: The database updates its internal system catalogs (e.g., modifying `pg_attribute` or `pg_class`) without reading or modifying the underlying 8KB data pages in the table heap. The operation completes in milliseconds regardless of table size.
- **Physical Table Rewrite**: The database engine must create a new physical backing file on disk, iterate over every row of the existing table, copy/convert the data to the new tuple format, update all secondary indexes, and swap the file pointers. This incurs massive disk I/O, generates heavy WAL volume, and locks the table for minutes or hours.
</details>

#### Q3: Which major relational database engines support Transactional DDL, and which do not?
<details>
<summary>Answer</summary>
- **PostgreSQL**: Full Transactional DDL. Almost all DDL statements (`CREATE TABLE`, `ALTER TABLE`, `DROP TABLE`, `CREATE INDEX CONCURRENTLY` excepted) can run inside `BEGIN ... ROLLBACK`.
- **SQLite**: Full Transactional DDL. Schema changes can be tested inside a transaction and rolled back safely.
- **SQL Server**: Full Transactional DDL within standard transactions.
- **MySQL (InnoDB)**: **No Transactional DDL**. Executing any DDL statement causes an implicit `COMMIT` before and after the statement, preventing rollbacks if a subsequent statement fails.
</details>

#### Q4: How does `TRUNCATE TABLE` differ from `DELETE FROM table` with respect to WAL generation and MVCC?
<details>
<summary>Answer</summary>
- `DELETE FROM table`: Operates under standard MVCC rules. Every individual row is flagged as dead by writing the current transaction ID to its `xmax` header. Every row deletion generates a WAL record. If the table has 10 million rows, 10 million WAL records are generated, and the table pages remain on disk filled with dead tuples until vacuumed.
- `TRUNCATE TABLE`: Bypasses row-by-row MVCC. It acquires an `AccessExclusiveLock`, allocates a new empty storage file on disk (`relfilenode`), updates the system catalog pointer, and unlinks the old file. It generates a single WAL record representing the truncation event, completing in milliseconds with zero tuple bloat.
</details>

### Practical DDL Writing Puzzles

#### Q5: Write the production-safe sequence of SQL commands to add a `NOT NULL` column with a default value to a 100-million-row table in PostgreSQL without locking out reads.
<details>
<summary>Answer</summary>

```sql
-- Step 1: Set a short lock timeout to protect traffic
SET lock_timeout = '2s';

-- Step 2: In PostgreSQL 11+, adding a column with a constant default is metadata-only!
ALTER TABLE large_events 
    ADD COLUMN event_source VARCHAR(50) DEFAULT 'system' NOT NULL;

RESET lock_timeout;
```
*(In PostgreSQL 11+, constant defaults update `pg_attribute` without rewriting the heap. In older versions or other engines, one would add the column as nullable first, backfill in batches, and then attach a `NOT NULL` constraint).*
</details>

#### Q6: How do you safely rename a column in a production database without breaking active application code that references the old column name?
<details>
<summary>Answer</summary>
A direct `ALTER TABLE RENAME COLUMN` breaks running application instances that expect the old column name. 

The Expand/Contract (Parallel Run) Pattern:
1. **Expand**: Add the new column (`ALTER TABLE users ADD COLUMN full_name TEXT;`).
2. **Dual-Write**: Deploy application code that reads from `old_col` but writes to both `old_col` and `new_col` (or use a database trigger).
3. **Backfill**: Migrate historical data in background batches (`UPDATE users SET full_name = old_col WHERE full_name IS NULL;`).
4. **Switch**: Deploy application code that reads and writes strictly to `new_col`.
5. **Contract**: Drop the old column after verifying telemetry (`ALTER TABLE users DROP COLUMN old_col;`).
</details>

#### Q7: Write an SQL statement that creates an exact structural copy of an existing table's schema (including defaults and constraints) without copying any data.
<details>
<summary>Answer</summary>

```sql
-- PostgreSQL syntax:
CREATE TABLE orders_archive (LIKE orders INCLUDING ALL);

-- Standard SQL syntax:
CREATE TABLE orders_archive AS 
SELECT * FROM orders WHERE 1 = 0;
```
*(Note: `LIKE ... INCLUDING ALL` in Postgres copies constraints, indexes, and defaults. The standard `WHERE 1 = 0` only copies column names and types).*
</details>

#### Q8: Write a query to find all foreign key constraints referencing a specific table before running `DROP TABLE`.
<details>
<summary>Answer</summary>

```sql
SELECT 
    tc.table_name AS referencing_table,
    kcu.column_name AS referencing_column,
    tc.constraint_name
FROM information_schema.table_constraints tc
JOIN information_schema.key_column_usage kcu 
  ON tc.constraint_name = kcu.constraint_name
JOIN information_schema.constraint_column_usage ccu 
  ON ccu.constraint_name = tc.constraint_name
WHERE tc.constraint_type = 'FOREIGN KEY'
  AND ccu.table_name = 'target_table_name';
```
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Creating a Table with Constraints
Create a table `suppliers` with an auto-incrementing integer PK, a supplier name (max 100 chars, required, unique), a contact email, and an active status defaulting to `TRUE`.
<details>
<summary>Solution</summary>

```sql
CREATE TABLE suppliers (
    supplier_id INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    supplier_name VARCHAR(100) NOT NULL UNIQUE,
    contact_email VARCHAR(150),
    is_active BOOLEAN DEFAULT TRUE NOT NULL
);
```
</details>

#### Exercise 2: Altering Column Definitions
Alter the `suppliers` table to add a `rating` column (integer between 1 and 5), and drop the `contact_email` column.
<details>
<summary>Solution</summary>

```sql
ALTER TABLE suppliers 
    ADD COLUMN rating SMALLINT CHECK (rating BETWEEN 1 AND 5),
    DROP COLUMN contact_email;
```
</details>

### Intermediate Exercises

#### Exercise 3: Creating a Summary Table via CTAS
Create an aggregate table `category_metrics` containing `category_id`, `product_count`, and `avg_price` computed from the `products` table, with zero rows copied initially.
<details>
<summary>Solution</summary>

```sql
CREATE TABLE category_metrics AS
SELECT 
    category_id,
    COUNT(*) AS product_count,
    AVG(retail_price) AS avg_price
FROM products
WHERE 1 = 0
GROUP BY category_id;
```
</details>

#### Exercise 4: Atomic Table Truncation with Identity Reset
Write a command to truncate the `audit_events` table, resetting its primary key identity sequence back to 1.
<details>
<summary>Solution</summary>

```sql
TRUNCATE TABLE audit_events RESTART IDENTITY;
```
</details>

### Advanced Exercises

#### Exercise 5: Zero-Downtime Check Constraint Validation
Given an existing table `user_accounts` with 5 million rows, write the two-step migration to enforce `balance >= 0` without blocking concurrent reads or writes during the row scan.
<details>
<summary>Solution</summary>

```sql
-- Step 1: Add constraint as NOT VALID (Instant metadata lock)
ALTER TABLE user_accounts 
    ADD CONSTRAINT chk_non_negative_balance 
    CHECK (balance >= 0) NOT VALID;

-- Step 2: Validate without blocking concurrent reads/writes
ALTER TABLE user_accounts 
    VALIDATE CONSTRAINT chk_non_negative_balance;
```
</details>

#### Exercise 6: Swapping Tables via Atomic Rename
In a data warehousing ETL process, a new table `sales_staging` has been populated. Write a transactional script to atomically replace the production `sales_current` table with `sales_staging` using table renames.
<details>
<summary>Solution</summary>

```sql
BEGIN;

-- Atomically swap tables under an exclusive lock
ALTER TABLE sales_current RENAME TO sales_old;
ALTER TABLE sales_staging RENAME TO sales_current;

-- Optionally drop the old table
DROP TABLE sales_old;

COMMIT;
```
*(Because this runs in a single transaction in PostgreSQL, queries never observe a moment where `sales_current` does not exist!)*
</details>

---

## 12. Summary & Quick Reference

### DDL Commands Syntax Summary
- **Create**: `CREATE TABLE [IF NOT EXISTS] name (columns, constraints);`
- **CTAS**: `CREATE TABLE name AS SELECT ...;`
- **Add Column**: `ALTER TABLE name ADD COLUMN col type [DEFAULT val] [NOT NULL];`
- **Drop Column**: `ALTER TABLE name DROP COLUMN col [CASCADE];`
- **Add Constraint**: `ALTER TABLE name ADD CONSTRAINT cname CHECK (...) [NOT VALID];`
- **Validate Constraint**: `ALTER TABLE name VALIDATE CONSTRAINT cname;`
- **Truncate**: `TRUNCATE TABLE name [RESTART IDENTITY] [CASCADE];`
- **Drop**: `DROP TABLE [IF EXISTS] name [CASCADE];`

---

## 13. Further Reading
- PostgreSQL 17 DDL Documentation: [Chapter 5: Data Definition](https://www.postgresql.org/docs/17/ddl.html)
- PostgreSQL Wiki: [Safe Schema Updates](https://wiki.postgresql.org/wiki/Safe_schema_updates)
- MySQL 8.4 Reference Manual: [InnoDB Instant DDL Operations](https://dev.mysql.com/doc/refman/8.4/en/innodb-online-ddl-operations.html)
- [03 - Data Integrity & Constraints](./03-constraints.md)
