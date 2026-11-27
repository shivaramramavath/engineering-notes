# Schema Migrations & Versioning: Safe DDL and Zero-Downtime Architecture

| Metadata | Details |
| :--- | :--- |
| **Topic** | Production Schema Migrations, DDL Locking Mechanics, Lock Queues, Expand/Contract Pattern, Migration Tooling |
| **Dialect** | ANSI SQL:2023 / PostgreSQL 17+ (Primary), MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Database Version** | PostgreSQL 17.2, MySQL 8.4.1, SQLite 3.50.4, SQL Server 2022 CU14 |
| **Tested On** | Python 3.14 stdlib sqlite3 engine & PostgreSQL 17 execution runner |
| **Date** | 2026-11-26 |
| **Author** | Antigravity SQL Curriculum Lead |
| **Category** | Database Design & Schema Architecture |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & Usage](#syntax--usage)
5. [Practical Examples](#practical-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance & Security Notes](#performance--security-notes)
9. [Comparison Matrix](#comparison-matrix)
10. [Interview Questions & Deep Dives](#interview-questions--deep-dives)
11. [Practice Exercises](#practice-exercises)
12. [Summary & Cheat Sheet](#summary--cheat-sheet)
13. [Further Reading](#further-reading)

---

## What & Why

In modern continuous delivery (CD) pipelines, application code is deployed dozens of times per day with zero downtime using blue-green or rolling canary deployments. However, the database cannot simply be duplicated and swapped out with a load balancer: it maintains state, processes live customer transactions, and holds gigabytes or terabytes of physical data.

Executing Data Definition Language (DDL) in a live production environment is the single highest-risk operational activity in software engineering:
- An `ALTER TABLE` statement requests an **`ACCESS EXCLUSIVE` lock**. If a long-running reporting query is currently reading the table, the migration must wait.
- **The Lock Queue Trap**: While the migration waits in line, it blocks every subsequent incoming query—including simple `SELECT` statements! Within seconds, connection pools are exhausted, web servers crash, and a full application outage ensues.
- Modifying a column type or adding a constraint incorrectly can force the database to rewrite the entire multi-gigabyte table to disk, locking all read/write traffic for hours.

To deploy schema changes safely without downtime, backend engineers must master **DDL locking hierarchies, lock timeouts, the multi-phase Expand/Contract migration pattern**, and production migration tooling.

---

## Core Concepts

### 1. PostgreSQL Lock Hierarchies
PostgreSQL enforces an 8-level table lock hierarchy. A higher-level lock conflicts with lower-level locks:
- **`ACCESS SHARE`**: Acquired by `SELECT`. Conflicts only with `ACCESS EXCLUSIVE`. Multiple queries can read simultaneously.
- **`ROW EXCLUSIVE`**: Acquired by `INSERT`, `UPDATE`, `DELETE`. Permits concurrent readers and writers on different rows.
- **`SHARE UPDATE EXCLUSIVE`**: Acquired by `VACUUM`, `ANALYZE`, and `CREATE INDEX CONCURRENTLY`. Does not block readers or writers!
- **`ACCESS EXCLUSIVE`**: The strongest lock. Acquired by `ALTER TABLE`, `DROP TABLE`, `TRUNCATE`. **Blocks all concurrent readers and writers completely**.

### 2. The Lock Queue Blockage Trap
PostgreSQL processes lock requests via a strict FIFO (First-In, First-Out) queue.
```
Step 1: Long-running analytics query is executing (Holds ACCESS SHARE lock on 'orders').
Step 2: Migration runs: ALTER TABLE orders ADD COLUMN ... (Requests ACCESS EXCLUSIVE lock).
        -> The migration is BLOCKED waiting for the analytics query to finish.
Step 3: New customer checkout arrives: SELECT * FROM orders ... (Requests ACCESS SHARE lock).
        -> PostgreSQL puts checkout in the queue BEHIND the pending ALTER TABLE!
Step 4: All subsequent queries queue up behind the migration. Connection pool maxes out -> OUTAGE!
```

### 3. Defensive Settings: `lock_timeout`
To prevent the lock queue trap, migrations must **never** wait indefinitely for a lock. Every migration transaction must set an aggressive `lock_timeout`:
```sql
SET lock_timeout = '2s';
ALTER TABLE orders ADD COLUMN notes TEXT;
```
If the lock cannot be acquired within 2 seconds, the transaction aborts cleanly, releasing the queue and preventing production traffic from backing up.

### 4. The Expand / Contract (Parallel Run) Pattern
Because application servers run multiple versions simultaneously during rolling deployments, the database schema must remain compatible with both the old and new application code.
- **Phase 1 (Expand)**: Add the new column/table nullable or with a default. Deploy new code that writes to *both* old and new columns, but reads from the old.
- **Phase 2 (Backfill)**: Run an asynchronous background script to copy historical data from the old column to the new column in small batches.
- **Phase 3 (Contract)**: Deploy code that reads and writes exclusively to the new column. Finally, drop the old column in a separate migration.

### 5. Migration Tooling Standards
Production migrations must be tracked in version control as immutable, sequential migration scripts:
- **Flyway / Liquibase**: SQL-first, database-agnostic, tracks versions via a catalog table (`flyway_schema_history`).
- **Prisma / TypeORM / Alembic**: ORM-driven schema diffing tools.

---

## How It Works Internally

How does PostgreSQL 11+ execute `ALTER TABLE ... ADD COLUMN ... DEFAULT` in sub-millisecond time without rewriting the table?

```
      [ Query: ALTER TABLE orders ADD COLUMN status VARCHAR(20) DEFAULT 'PENDING'; ]
                                           │
                                           ▼
                                 [ PostgreSQL 11+ Engine ]
                                           │
    Does the default expression evaluate to a constant value?
                                           │
                    ┌──────────────────────┴──────────────────────┐
                    ▼                                             ▼
       [ YES: Fast Metadata Update ]                 [ NO: Volatile Function (e.g. random()) ]
                    │                                             │
      1. Acquires brief ACCESS EXCLUSIVE.           1. Acquires long ACCESS EXCLUSIVE.
      2. Updates pg_attribute catalog.              2. Scans every 8KB heap page.
      3. Records default value in atthasdef.        3. Rewrites every physical tuple!
      4. Execution time: < 2 ms! (O(1))             4. Execution time: Minutes/Hours! (O(N))
                    │
                    ▼
     [ What happens when old rows are read? ]
     Tuple parser sees attribute is physically missing in old row.
     Engine automatically substitutes default value from catalog in memory!
```

1. **Fast Metadata Alterations**: In PostgreSQL 11+, adding a column with a constant `DEFAULT` or a nullable column is a pure metadata operation ($O(1)$) that updates system catalogs (`pg_attribute`) without modifying existing table pages on disk.
2. **Volatile Defaults Require Full Table Rewrites**: If the default expression calls a volatile function (e.g., `DEFAULT clock_timestamp()`), PostgreSQL cannot optimize it as metadata and must sequentially rewrite every tuple on disk.

---

## Syntax & Usage

### 1. The Production-Safe Migration Wrapper Template
```sql
-- Safe Migration Wrapper in PostgreSQL
BEGIN;

-- 1. Fail fast if a lock cannot be acquired immediately
SET LOCAL lock_timeout = '3s';

-- 2. Fail fast if the DDL execution takes too long
SET LOCAL statement_timeout = '10s';

-- 3. Execute metadata DDL
ALTER TABLE customer_accounts 
ADD COLUMN is_verified BOOLEAN NOT NULL DEFAULT FALSE;

COMMIT;
```

### 2. Safe Index Creation: `CREATE INDEX CONCURRENTLY`
```sql
-- MUST be executed outside a transaction block (cannot be inside BEGIN/COMMIT)
CREATE INDEX CONCURRENTLY idx_customer_accounts_email 
ON customer_accounts (email);
```

### 3. Safe NOT NULL Constraint Addition (Two-Phase)
```sql
-- Step 1: Add constraint without validating existing rows (Instant metadata lock)
ALTER TABLE customer_accounts 
ADD CONSTRAINT chk_accounts_phone_not_null 
CHECK (phone_number IS NOT NULL) NOT VALID;

-- Step 2: Validate existing data in the background (Does NOT block reads or writes!)
ALTER TABLE customer_accounts 
VALIDATE CONSTRAINT chk_accounts_phone_not_null;

-- Step 3: (PostgreSQL 12+) Convert validated check constraint to formal NOT NULL
ALTER TABLE customer_accounts 
ALTER COLUMN phone_number SET NOT NULL;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **Transactional DDL** | **Fully Supported** (DDL rolls back in transactions!) | **Not Supported** (DDL triggers implicit commit!) | Fully Supported | Fully Supported |
| **Non-blocking Indexing** | `CREATE INDEX CONCURRENTLY` | `ALGORITHM=INPLACE, LOCK=NONE` | **Not Supported** (Locks table) | `WITH (ONLINE = ON)` |
| **Fast Default Add Column**| Supported (PG 11+) | Supported (MySQL 8.0 Instant DDL)| Supported | Supported (Enterprise Edition) |
| **Safe Constraint Addition**| `NOT VALID` followed by `VALIDATE` | Not supported (Validates immediately)| Not supported | `WITH NOCHECK` followed by `CHECK` |

---

## Practical Examples

### Example 1: Basic — Reversible Migration Script Structure
A professional migration script incorporating both UP and DOWN logic with version assertions.

```sql
-- V004__add_discount_code_to_orders.sql
-- UP MIGRATION:
BEGIN;
SET LOCAL lock_timeout = '2s';

ALTER TABLE orders 
ADD COLUMN discount_code VARCHAR(30) NULL;

COMMENT ON COLUMN orders.discount_code IS 'Optional marketing coupon code applied at checkout';

COMMIT;

-- DOWN MIGRATION (Rollback Plan):
-- BEGIN;
-- SET LOCAL lock_timeout = '2s';
-- ALTER TABLE orders DROP COLUMN IF EXISTS discount_code;
-- COMMIT;
```

---

### Example 2: Real-World — Zero-Downtime Column Rename (Expand/Contract)
Renaming column `usr_nm` to `username` in a table with 100,000,000 rows without taking down the production API.

#### Step 1: Expand (Add new column & trigger)
```sql
-- Migration 1: Add new column
ALTER TABLE users ADD COLUMN username VARCHAR(100) NULL;

-- Trigger to sync writes from old column to new column during deployment:
CREATE OR REPLACE FUNCTION sync_username_columns()
RETURNS TRIGGER AS $$
BEGIN
    NEW.username = COALESCE(NEW.username, NEW.usr_nm);
    NEW.usr_nm = COALESCE(NEW.usr_nm, NEW.username);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_sync_users_username
BEFORE INSERT OR UPDATE ON users
FOR EACH ROW EXECUTE FUNCTION sync_username_columns();
```

#### Step 2: Backfill Historical Data in Batches
```sql
-- Run in small batches of 5,000 rows to prevent long transactions:
DO $$
DECLARE
    batch_size INT := 5000;
    rows_updated INT;
BEGIN
    LOOP
        UPDATE users 
        SET username = usr_nm 
        WHERE id IN (
            SELECT id FROM users 
            WHERE username IS NULL AND usr_nm IS NOT NULL 
            LIMIT batch_size
        );
        GET DIAGNOSTICS rows_updated = ROW_COUNT;
        EXIT WHEN rows_updated = 0;
        COMMIT; -- Releases lock between batches!
    END LOOP;
END $$;
```

#### Step 3: Contract (Drop old trigger and column)
```sql
-- Executed after application code has fully transitioned to 'username':
DROP TRIGGER trg_sync_users_username ON users;
DROP FUNCTION sync_username_columns();
ALTER TABLE users DROP COLUMN usr_nm;
```

---

### Example 3: Edge Case — Handling Failed `CONCURRENTLY` Indexes
If a `CREATE INDEX CONCURRENTLY` statement fails (e.g., duplicate values encountered, or connection dropped), PostgreSQL leaves the index in an **`INVALID`** state. Invalid indexes continue to consume disk space and slow down writes, but are ignored by the query planner!

```sql
-- 1. Detect invalid indexes in the database:
SELECT indexrelid::regclass AS invalid_index_name
FROM pg_index
WHERE indisvalid = FALSE;

-- 2. Drop the invalid index safely:
DROP INDEX CONCURRENTLY IF EXISTS idx_customer_accounts_email;

-- 3. Re-run index creation after fixing the root cause:
CREATE INDEX CONCURRENTLY idx_customer_accounts_email ON customer_accounts (email);
```

---

### Example 4: Failure Mode — The Implicit Commit Trap in MySQL
Demonstrating how non-transactional DDL in MySQL breaks atomic deployment rollbacks.

```sql
-- IN POSTGRESQL (Atomic):
BEGIN;
ALTER TABLE users ADD COLUMN age INT;
INSERT INTO audit_log VALUES ('Age column added');
-- If anything fails here, ROLLBACK undoes BOTH the column addition and the insert!
ROLLBACK; 
-- Result: Table 'users' has NO 'age' column. Perfectly safe.

-- IN MYSQL (Non-Transactional DDL Disaster):
START TRANSACTION;
INSERT INTO audit_log VALUES ('Starting migration');
ALTER TABLE users ADD COLUMN age INT; -- IMPLICIT COMMIT TRIGGERED IMMEDIATELY!
-- Error occurs here!
ROLLBACK; 
-- Result: In MySQL, ALTER TABLE cannot be rolled back! 
-- The database is left in a broken, half-migrated state!
```

---

### Example 5: Optimized — Batch Purging / Deleting Without Vacuum Lockup
Deleting 10,000,000 expired rows from an audit table without locking the database or exhausting WAL storage.

```sql
-- Delete in small transaction chunks of 10,000 rows:
DO $$
DECLARE
    deleted_count INT;
BEGIN
    LOOP
        DELETE FROM user_logs 
        WHERE log_id IN (
            SELECT log_id FROM user_logs 
            WHERE created_at < NOW() - INTERVAL '1 year' 
            LIMIT 10000
        );
        GET DIAGNOSTICS deleted_count = ROW_COUNT;
        EXIT WHEN deleted_count = 0;
        -- Pause briefly to allow replication and autovacuum to breathe:
        PERFORM pg_sleep(0.1);
        COMMIT;
    END LOOP;
END $$;
```

---

## Best Practices

1. **Always Set `lock_timeout` in Every Migration**: Prevent the lock queue blockage trap by enforcing `SET LOCAL lock_timeout = '2s'`.
2. **Never Build Indexes Synchronously in Production**: Always use `CREATE INDEX CONCURRENTLY` (PostgreSQL) or `ONLINE = ON` (SQL Server).
3. **Follow the Expand / Contract Pattern for Breaking Changes**: Never rename columns, change column data types, or add non-nullable columns without defaults in a single step.
4. **Clean Up Invalid Concurrent Indexes**: Regularly check `pg_index.indisvalid` for failed concurrent index attempts and drop them.
5. **Backfill in Small Batches with Sleep Pauses**: Avoid updating millions of rows in a single monolithic transaction. Process in chunks of 5,000–10,000 rows with explicit `COMMIT` statements to avoid WAL bloat and replication lag.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Missing `lock_timeout` in migration | Sits in lock queue behind slow queries, blocking all application traffic. | Set `SET LOCAL lock_timeout = '2s'`. |
| 2 | Standard `CREATE INDEX` on large tables | Takes `SHARE` lock blocking all concurrent `INSERT`, `UPDATE`, `DELETE` operations. | Use `CREATE INDEX CONCURRENTLY`. |
| 3 | Single-step column rename | Immediately breaks running application instances during rolling deploys. | Use Expand / Contract pattern with multi-version code. |
| 4 | Monolithic multi-million row `UPDATE` | Saturates transaction log (WAL), locks rows for minutes, crashes replication. | Batch update in small chunks of 5,000 rows. |
| 5 | Assuming MySQL DDL is transactional | MySQL DDL triggers an implicit commit; migrations cannot roll back on error. | Use migration tools that verify preconditions and test scripts on staging. |
| 6 | Running `CREATE INDEX CONCURRENTLY` in a transaction | PostgreSQL disallows concurrent index builds inside transaction blocks. | Run outside transaction block (`autocommit = true`). |

---

## Performance & Security Notes

### Replication Lag During Migrations
In databases utilizing streaming physical replication (Primary -> Standby replicas):
- When a migration performs a heavy DDL or updates 1,000,000 rows, it generates gigabytes of WAL records.
- Replicas must replay these WAL records sequentially.
- If the primary executes writes faster than replicas can replay, **replication lag spikes**. Read queries directed to read replicas serve stale data, and failover automation may fail.
- **Rule**: Monitor `pg_stat_replication` during migration execution and throttle batch updates to maintain replication lag under 1 second.

---

## Comparison Matrix

| DDL Operation | PostgreSQL Lock Type | Blocks Reads? | Blocks Writes? | Safe in Production? |
| :--- | :--- | :--- | :--- | :--- |
| **`ADD COLUMN (Nullable)`** | `ACCESS EXCLUSIVE` | Yes (for <2ms) | Yes (for <2ms) | **Yes (Safe)** |
| **`ADD COLUMN DEFAULT const`** | `ACCESS EXCLUSIVE` | Yes (for <2ms) | Yes (for <2ms) | **Yes (PG 11+)** |
| **`CREATE INDEX`** | `SHARE` | No | **Yes (Until done)** | **DANGEROUS** |
| **`CREATE INDEX CONCURRENTLY`**| `SHARE UPDATE EXCL` | **No** | **No** | **Yes (Safe)** |
| **`DROP COLUMN`** | `ACCESS EXCLUSIVE` | Yes (for <5ms) | Yes (for <5ms) | **Yes (If app decoupled)**|
| **`ALTER COLUMN TYPE`** | `ACCESS EXCLUSIVE` | **Yes (Full table rewrite)**| **Yes (Full rewrite)** | **FATAL (Downtime)** |

---

## Interview Questions & Deep Dives

### Q1: What is the "Lock Queue Blockage Trap" and how does setting `lock_timeout` prevent it?
<details>
<summary>Click to view answer</summary>

When a migration issues `ALTER TABLE`, it requests an `ACCESS EXCLUSIVE` lock. If a long-running read query is currently executing, the migration waits in line in the engine's lock queue. Because PostgreSQL processes lock requests in FIFO order, any subsequent `SELECT` queries that arrive *after* the migration are queued behind the migration. Within seconds, hundreds of incoming queries pile up, exhausting the database connection pool and taking down the application.

Setting `SET LOCAL lock_timeout = '2s'` instructs the database to abort the migration transaction if the lock cannot be acquired within 2 seconds. This ejects the migration from the queue, allowing all pending and incoming `SELECT` queries to continue processing normally without interruption.
</details>

### Q2: Why cannot `CREATE INDEX CONCURRENTLY` be executed inside a transaction block (`BEGIN ... COMMIT`)?
<details>
<summary>Click to view answer</summary>

`CREATE INDEX CONCURRENTLY` operates using a **two-phase indexing algorithm**:
1. Phase 1: It acquires a weak `SHARE UPDATE EXCLUSIVE` lock, scans the table, builds the initial index structure, and waits for all active transactions that were writing to the table to commit.
2. Phase 2: It performs a second scan to index any rows that were inserted or modified during Phase 1.
Because it must wait for other transactions to complete and sync internal transaction snapshots across multiple phases, it cannot be contained inside a single atomic transaction block.
</details>

### Q3: What is the Expand / Contract migration pattern?
<details>
<summary>Click to view answer</summary>

The Expand / Contract pattern is a methodology for making breaking schema changes (like renaming a column, changing a type, or splitting a table) with zero application downtime:
1. **Expand**: Add the new schema structure alongside the old structure. Update application code to dual-write to both old and new structures, while reading from the old.
2. **Backfill**: Asynchronously copy historical data from the old structure to the new structure in small, non-blocking batches.
3. **Contract**: Deploy application code that reads and writes strictly to the new structure. In a final migration, safely drop the old structure.
</details>

### Q4: How did PostgreSQL 11 optimize `ALTER TABLE ... ADD COLUMN ... DEFAULT`?
<details>
<summary>Click to view answer</summary>

Prior to PostgreSQL 11, adding a column with a default value required sequentially rewriting every physical page of the table on disk to write the default value into every existing row, taking hours and locking large tables. 

Beginning in PostgreSQL 11, if the default is a non-volatile constant (e.g., `DEFAULT 'PENDING'`), PostgreSQL performs a **metadata-only update** ($O(1)$ time). It records the default value in the `atthasdef` column of the `pg_attribute` catalog. When queries subsequently read existing rows where the column is physically absent, the tuple parser automatically substitutes the default value from memory.
</details>

### Q5: How do you safely add a `NOT NULL` constraint to a column on a 50-million row table in PostgreSQL?
<details>
<summary>Click to view answer</summary>

Executing `ALTER TABLE tbl ALTER COLUMN col SET NOT NULL;` directly takes an exclusive lock and scans all 50 million rows, blocking writes.

**The Safe 2-Step Pattern**:
1. Add a check constraint without validating existing rows:
   ```sql
   ALTER TABLE tbl ADD CONSTRAINT chk_col_not_null CHECK (col IS NOT NULL) NOT VALID;
   ```
   (Takes an instant lock).
2. Validate the constraint in the background:
   ```sql
   ALTER TABLE tbl VALIDATE CONSTRAINT chk_col_not_null;
   ```
   (Validates data without blocking reads or writes).
In PostgreSQL 12+, once the check constraint is validated, executing `ALTER COLUMN col SET NOT NULL;` is an instantaneous metadata-only operation.
</details>

### Q6: What is Transactional DDL and which major database does NOT support it?
<details>
<summary>Click to view answer</summary>

Transactional DDL allows schema modification statements (`CREATE TABLE`, `ALTER TABLE`, `DROP TABLE`) to be executed inside an ACID transaction block (`BEGIN ... COMMIT`). If an error occurs, issuing `ROLLBACK` undoes the entire schema change cleanly. 

PostgreSQL, SQLite, and SQL Server fully support Transactional DDL. **MySQL does NOT support Transactional DDL**: any DDL statement in MySQL triggers an immediate, implicit commit of the current transaction, making atomic rollback impossible.
</details>

### Q7: What causes a PostgreSQL index to become `INVALID`?
<details>
<summary>Click to view answer</summary>

When `CREATE INDEX CONCURRENTLY` encounters an unhandled error during its build phase (such as a unique constraint violation, a timeout, or a client disconnection), PostgreSQL does not drop the partially built index. Instead, it marks the index as `INVALID` (`indisvalid = false` in `pg_index`). The invalid index is never used by queries, but continues to consume disk space and incurs write overhead during inserts until manually dropped via `DROP INDEX CONCURRENTLY`.
</details>

### Q8: How should large data backfills be throttled in production?
<details>
<summary>Click to view answer</summary>

Large data backfills should be throttled by:
1. Chunking updates into batches of 1,000 to 10,000 rows based on primary key ranges (`WHERE id BETWEEN $start AND $end`).
2. Issuing an explicit `COMMIT` after each batch to release row locks and allow transaction logs to truncate.
3. Adding a sleep pause (e.g., `pg_sleep(0.1)`) between batches to prevent saturating disk I/O and allow read replicas to catch up on replication lag.
</details>

---

## Practice Exercises

### Exercise 1: Safe Migration Wrapper Script
Write a complete, production-safe PostgreSQL DDL script that adds a column `reorder_threshold INT NOT NULL DEFAULT 10` to `products`, ensuring that the script aborts if a lock cannot be acquired within 2 seconds.

<details>
<summary>Click to view solution</summary>

```sql
BEGIN;

-- Enforce aggressive timeouts to avoid blocking the queue:
SET LOCAL lock_timeout = '2s';
SET LOCAL statement_timeout = '10s';

-- Fast metadata-only update in PG 11+:
ALTER TABLE products 
ADD COLUMN reorder_threshold INT NOT NULL DEFAULT 10;

COMMIT;
```
</details>

---

### Exercise 2: Query for Invalid Indexes
Write a query against PostgreSQL system catalogs to identify all invalid indexes in the database, including the table name and index size.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    c.relname AS table_name,
    i.relname AS index_name,
    pg_size_pretty(pg_relation_size(i.oid)) AS index_size
FROM pg_index x
JOIN pg_class c ON c.oid = x.indrelid
JOIN pg_class i ON i.oid = x.indexrelid
WHERE x.indisvalid = FALSE;
```
</details>

---

### Exercise 3: Safe NOT NULL Addition via Check Constraint
Write the exact SQL statements to safely enforce that `users.email` is not null on a table with 20 million rows without blocking concurrent traffic.

<details>
<summary>Click to view solution</summary>

```sql
-- Step 1: Add check constraint without validating existing rows (instant):
ALTER TABLE users 
ADD CONSTRAINT chk_users_email_not_null 
CHECK (email IS NOT NULL) NOT VALID;

-- Step 2: Validate existing rows in background (non-blocking):
ALTER TABLE users 
VALIDATE CONSTRAINT chk_users_email_not_null;

-- Step 3: Formalize as standard NOT NULL (instant metadata update in PG 12+):
ALTER TABLE users 
ALTER COLUMN email SET NOT NULL;
```
</details>

---

### Exercise 4: Batched Update Loop
Write a PL/pgSQL script to backfill `is_processed = TRUE` on `telemetry_logs` where `processed_at IS NOT NULL`, updating in batches of 5,000 rows to prevent table lockups.

<details>
<summary>Click to view solution</summary>

```sql
DO $$
DECLARE
    batch_count INT;
BEGIN
    LOOP
        UPDATE telemetry_logs 
        SET is_processed = TRUE 
        WHERE log_id IN (
            SELECT log_id FROM telemetry_logs 
            WHERE is_processed = FALSE AND processed_at IS NOT NULL 
            LIMIT 5000
        );
        GET DIAGNOSTICS batch_count = ROW_COUNT;
        EXIT WHEN batch_count = 0;
        COMMIT;
    END LOOP;
END $$;
```
</details>

---

### Exercise 5: Non-Blocking Index Creation
Write the command to build a unique index on `merchants(tax_id)` without blocking concurrent insert or update queries.

<details>
<summary>Click to view solution</summary>

```sql
-- Must execute outside a transaction block:
CREATE UNIQUE INDEX CONCURRENTLY idx_merchants_tax_id 
ON merchants (tax_id);
```
</details>

---

### Exercise 6: Fixing the Lock Queue Blockage Bug
During a deployment, a migration script hangs for 45 seconds trying to alter `orders`, and web servers report that all customer checkout queries have frozen. Explain the root cause and provide the immediate operational remediation.

<details>
<summary>Click to view solution</summary>

**Root Cause**: The migration requested an `ACCESS EXCLUSIVE` lock on `orders` and is waiting behind an existing long-running query (e.g., an analytical report). All incoming checkout queries (`SELECT` and `INSERT`) are queued up *behind* the migration in the lock queue, exhausting connection pools.

**Immediate Operational Remediation**:
1. Identify the blocking process ID:
   ```sql
   SELECT pid, query, state, age(clock_timestamp(), query_start) 
   FROM pg_stat_activity 
   WHERE wait_event_type = 'Lock';
   ```
2. Immediately terminate the migration process:
   ```sql
   SELECT pg_cancel_backend(migration_pid);
   ```
   (This instantly unblocks the lock queue and restores customer checkouts).
3. Update the migration script to include `SET LOCAL lock_timeout = '2s';` before retrying.
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                               MIGRATION SAFETY CHEAT SHEET                              |
+-------------------+----------------------------+----------------------------------------+
| Operation         | Safe Command               | Why It's Safe                          |
+-------------------+----------------------------+----------------------------------------+
| Acquire Lock      | SET LOCAL lock_timeout='2s'| Aborts if blocked; prevents queue pileup|
| Build Index       | CREATE INDEX CONCURRENTLY  | Builds in 2 passes; zero write blocking|
| Add NOT NULL      | CHECK (...) NOT VALID      | Instant lock; background validation    |
| Add Default (PG11)| ADD COLUMN ... DEFAULT 'x' | Pure metadata update; <2ms execution   |
| Rename Column     | Expand / Contract Pattern  | Allows rolling multi-version deploys   |
| Historical Update | Batch chunks of 5,000 rows | Prevents WAL bloat and replication lag |
+-------------------+----------------------------+----------------------------------------+
```

- **Lock Rule**: Never run DDL without setting `lock_timeout`.
- **Index Rule**: Always use `CONCURRENTLY` in production.
- **Rollback Rule**: Test down migrations on production-sized staging data before deployment.

---

## Further Reading
- PostgreSQL 17 Documentation: *Explicit Locking & Table-Level Locks*
- pg-roll: *Zero-Downtime Schema Migrations for PostgreSQL*
- GitLab Engineering: *PostgreSQL Database Migration Style Guide*
- Flyway Documentation: *Database Migrations in Continuous Delivery Pipelines*
