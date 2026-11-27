# Database Locking Mechanisms & Lock Hierarchies

## Metadata
| Attribute | Details |
|:---|:---|
| **Topic** | Concurrency Control: Physical Locks, Modes & Hierarchies |
| **Dialect** | Standard ANSI SQL / PostgreSQL 17+ Baseline |
| **Database Version** | PostgreSQL 17+, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Tested with** | PostgreSQL 17.2, SQLite 3.50.4 |
| **Prerequisites** | `04-transactions-and-concurrency/01-acid.md`, `04-transactions-and-concurrency/04-mvcc-explained.md` |
| **Complexity** | Advanced |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [The Lock Hierarchy & Granularity](#the-lock-hierarchy--granularity)
4. [PostgreSQL Table Lock Modes](#postgresql-table-lock-modes)
5. [Row-Level Lock Modes](#row-level-lock-modes)
6. [How the Lock Manager Works Internally](#how-the-lock-manager-works-internally)
7. [The Lock Queue Blockage Trap](#the-lock-queue-blockage-trap)
8. [Cross-Dialect Differences](#cross-dialect-differences)
9. [Practical Examples](#practical-examples)
10. [Best Practices](#best-practices)
11. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
12. [Performance & Security Notes](#performance--security-notes)
13. [Comparison Matrix: Lock Modes Conflict Matrix](#comparison-matrix-lock-modes-conflict-matrix)
14. [Real-World Interview Questions](#real-world-interview-questions)
15. [Practice Exercises](#practice-exercises)
16. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

While Multi-Version Concurrency Control (MVCC) allows concurrent readers to query snapshots without blocking writers, **write-write conflicts** and **schema mutations (DDL)** cannot be resolved with snapshots alone. If two concurrent transactions attempt to debit the exact same bank balance row simultaneously, or if a DBA attempts to drop a column while thousands of queries are actively reading it, the database engine must enforce physical serialization.

**Locking** is the foundational synchronization mechanism used by database engines to arbitrate concurrent access to shared data structures. Understanding lock granularity, lock modes, and lock queues is essential for database engineers: misunderstanding them leads directly to catastrophic production incidents—connection pool exhaustion, cascading query timeouts, and complete application lockups caused by a single simple migration script.

---

## Core Concepts

### 1. Lock Granularity
Locks can be acquired at different levels of the physical storage hierarchy:
- **Database Level**: Global lock on the entire database catalog (e.g., SQLite default write lock, or `DROP DATABASE`).
- **Table Level**: Heavyweight locks protecting table schemas and physical heap files during DDL or bulk operations.
- **Page / Block Level**: Shared or exclusive locks on an 8 KB physical disk page. Common in SQL Server and DB2 during index traversal; rare in PostgreSQL user transactions.
- **Row / Tuple Level**: Fine-grained locks on an individual row within a table page, protecting concurrent mutations against the exact same record.

### 2. Lock Modes: Shared vs Exclusive
The fundamental locking rule:
- **Shared (`S`) Lock**: Acquired when multiple sessions want to read data concurrently without modifying it. Multiple transactions can hold shared locks on the same object simultaneously.
- **Exclusive (`X`) Lock**: Acquired when a session wants to modify or delete data. Only one transaction can hold an exclusive lock on an object, blocking all other transactions from holding either shared or exclusive locks.

### 3. Intent Locks
When locking an individual row, how does the database prevent another transaction from locking the entire table?
- Scanning all 10 million rows in a table to check if any single row is locked before granting a table lock would be computationally prohibitive.
- Instead, the engine uses **Intent Locks** (e.g., Intent Shared `IS`, Intent Exclusive `IX`).
- Before acquiring a row-level exclusive lock, the transaction must first place an **Intent Exclusive (`IX`)** lock on the parent table. Any transaction wishing to acquire an exclusive lock on the entire table simply checks the table's intent lock flags, immediately detecting that child rows are locked!

---

## The Lock Hierarchy & Granularity

```
+-----------------------------------------------------------------------------------------+
|                                    LOCK HIERARCHY TREE                                  |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|                                     DATABASE LEVEL                                      |
|                                           |                                             |
|                                           v                                             |
|                                      TABLE LEVEL                                        |
|                       (AccessShare, RowExclusive, AccessExclusive)                      |
|                                     /             \                                     |
|                                    v               v                                    |
|                              PAGE LEVEL         PAGE LEVEL                              |
|                              (Buffer Locks)    (Buffer Locks)                           |
|                                  /                     \                                |
|                                 v                       v                               |
|                             ROW LEVEL               ROW LEVEL                           |
|                        (FOR UPDATE, SHARE)     (FOR UPDATE, SHARE)                      |
+-----------------------------------------------------------------------------------------+
```

---

## PostgreSQL Table Lock Modes

PostgreSQL defines an elegant hierarchy of **8 distinct table-level lock modes**, ordered from weakest to strongest:

1. **`ACCESS SHARE`**: Acquired automatically by `SELECT` queries. Conflicts *only* with `ACCESS EXCLUSIVE`.
2. **`ROW SHARE`**: Acquired by `SELECT ... FOR UPDATE` and `SELECT ... FOR SHARE`. Conflicts with `EXCLUSIVE` and `ACCESS EXCLUSIVE`.
3. **`ROW EXCLUSIVE`**: Acquired by `INSERT`, `UPDATE`, and `DELETE`. Conflicts with `SHARE`, `SHARE ROW EXCLUSIVE`, `EXCLUSIVE`, and `ACCESS EXCLUSIVE`. Allows concurrent readers and concurrent writers on *different* rows!
4. **`SHARE UPDATE EXCLUSIVE`**: Acquired by `VACUUM` (without FULL), `ANALYZE`, `CREATE INDEX CONCURRENTLY`, and `ALTER TABLE VALIDATE CONSTRAINT`. Protects against concurrent schema changes while allowing concurrent reads and writes.
5. **`SHARE`**: Acquired by `CREATE INDEX` (standard). Allows concurrent reads, but blocks concurrent writes.
6. **`SHARE ROW EXCLUSIVE`**: Acquired by `CREATE TRIGGER` and certain forms of `ALTER TABLE`. Blocks concurrent writes and index builds; allows reads.
7. **`EXCLUSIVE`**: Acquired by `REFRESH MATERIALIZED VIEW CONCURRENTLY`. Blocks concurrent writes and other exclusive queries; allows reads.
8. **`ACCESS EXCLUSIVE`**: The heaviest lock. Acquired by `DROP TABLE`, `TRUNCATE`, `ALTER TABLE`, and `VACUUM FULL`. Conflicts with **every other lock mode** (blocks all reads and all writes).

---

## Row-Level Lock Modes

In PostgreSQL, row locks are represented by four distinct strengths:

1. **`FOR KEY SHARE`**: Weakest. Used by foreign key integrity checks. Multiple sessions can hold it. Allows concurrent updates to non-key columns!
2. **`FOR SHARE`**: Acquired by `SELECT ... FOR SHARE`. Blocks any transaction from updating or deleting the row, but permits other transactions to read or acquire `FOR SHARE`.
3. **`FOR NO KEY UPDATE`**: Acquired by `UPDATE` statements that **do not modify unique or primary key columns**. Allows concurrent transactions to acquire `FOR KEY SHARE`!
4. **`FOR UPDATE`**: Strongest row lock. Acquired by `DELETE` and `UPDATE` on primary/unique keys, or explicit `SELECT ... FOR UPDATE`. Blocks all other row locks on that tuple.

---

## How the Lock Manager Works Internally

The database Lock Manager maintains two primary data structures in shared memory:
1. **Lock Hash Table (`LockMethodData`)**: A hash map indexed by `LockTag` (Database OID + Relation OID + Tuple ID / Page ID). Each lock tag points to a `PROCLOCK` structure tracking granted locks and waiting transactions.
2. **The Wait Queue (`waitExec` / Wait Queue FIFO)**: A doubly linked list of transactions waiting to acquire a lock on that resource.

---

## The Lock Queue Blockage Trap

The most common cause of database outages during application deployments is the **Lock Queue Head-of-Line Blocking Trap**.

```
Time  | Session 1 (Slow Analytics)     | Session 2 (Migration Script)   | Session 3 (Live Web Traffic)
------+--------------------------------+--------------------------------+-----------------------------
T1    | SELECT * FROM orders; (10 min) |                                |
      | (Holds ACCESS SHARE)           |                                |
------+--------------------------------+--------------------------------+-----------------------------
T2    | (Running...)                   | ALTER TABLE orders ADD col ... |
      |                                | (Requests ACCESS EXCLUSIVE)   |
      |                                | BLOCKED behind Session 1!      |
      |                                | Enters Lock Wait Queue!        |
------+--------------------------------+--------------------------------+-----------------------------
T3    | (Running...)                   | (Waiting in Queue...)          | SELECT * FROM orders WHERE ...
      |                                |                                | (Requests ACCESS SHARE)
      |                                |                                | BLOCKED behind Session 2!
------+--------------------------------+--------------------------------+-----------------------------
T4    | Result: Session 2 is waiting for Session 1.                                                    |
      | Because Session 2 requests an EXCLUSIVE lock, the Lock Manager FIFO blocks ALL subsequent      |
      | incoming queries (Session 3, 4, 5...) behind Session 2 to prevent starvation!                  |
      | Within 10 seconds, all 500 connection pool slots are exhausted. The entire website goes DOWN! |
```

### The Solution: Mandatory Lock Timeouts
Always set a strict lock timeout before any DDL in migration scripts:
```sql
SET LOCAL lock_timeout = '2s';
ALTER TABLE orders ADD COLUMN loyalty_tier VARCHAR(32);
```

---

## Cross-Dialect Differences

```
+----------------------------------------------------------------------------------------------------+
| DIALECT COMPARISON: LOCKING BEHAVIOR & ROW LOCK IMPLEMENTATIONS                                    |
+-------------------+--------------------------------------------------------------------------------+
| PostgreSQL 17+    | Row locks are NOT stored in RAM! They are stored in the tuple header on disk   |
|                   | (xmax and infomask bits). Millions of row locks consume ZERO lock manager RAM! |
|                   | Table locks are tracked in the shared memory Lock Table.                       |
+-------------------+--------------------------------------------------------------------------------+
| MySQL 8.4 (InnoDB)| Row locks ARE stored in RAM in the InnoDB Lock Manager (lock_sys). Uses        |
|                   | Record Locks, Gap Locks, and Next-Key Locks. Locking millions of rows consumes |
|                   | significant RAM. Supports table intention locks (IS, IX).                      |
+-------------------+--------------------------------------------------------------------------------+
| SQLite 3.50+      | Database-level locking! Uses 5 states: UNLOCKED, SHARED, RESERVED, PENDING,    |
|                   | EXCLUSIVE. In WAL mode, single writer locks WAL; readers take shared memory lock.|
+-------------------+--------------------------------------------------------------------------------+
| SQL Server 2022   | Locks tracked in memory. Features Lock Escalation: when a transaction acquires |
|                   | > 5,000 row locks on a table, the engine escalates them to a full TABLE LOCK!   |
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Observing Table Locks in PostgreSQL
```sql
-- Query active locks held across tables
SELECT 
    l.pid,
    d.datname,
    c.relname AS table_name,
    l.mode AS lock_mode,
    l.granted
FROM pg_locks l
JOIN pg_class c ON l.relation = c.oid
JOIN pg_database d ON l.database = d.oid
WHERE c.relkind = 'r'
ORDER BY l.pid;
```

### Example 2: Explicit Table Locking
When running an offline maintenance migration that requires total isolation:
```sql
BEGIN;

-- Explicitly request exclusive lock, with 3-second timeout protection
SET LOCAL lock_timeout = '3s';

LOCK TABLE daily_metrics IN ACCESS EXCLUSIVE MODE;

-- Perform bulk operations safely
TRUNCATE daily_metrics_staging;
INSERT INTO daily_metrics SELECT * FROM daily_metrics_staging;

COMMIT;
```

### Example 3: Edge Case — Safe DDL Deployment with Retry Loop
Deploying a non-null column addition safely without locking production:
```sql
DO $$
DECLARE
    attempts INT := 0;
BEGIN
    LOOP
        BEGIN
            -- Attempt migration with a 1-second timeout
            SET LOCAL lock_timeout = '1s';
            ALTER TABLE customers ADD COLUMN is_verified BOOLEAN DEFAULT FALSE;
            EXIT; -- Succeeded, exit loop
        EXCEPTION
            WHEN lock_not_available THEN
                attempts := attempts + 1;
                IF attempts > 10 THEN
                    RAISE EXCEPTION 'Could not acquire lock after 10 attempts; aborting.';
                END IF;
                PERFORM pg_sleep(0.5); -- Backoff before retry
        END;
    END LOOP;
END $$;
```

### Example 4: Failure Mode — Lock Escalation in SQL Server
What happens in SQL Server when updating many rows:
```sql
-- SQL Server T-SQL
BEGIN TRAN;

-- Updating 15,000 rows acquires row locks initially...
-- Once lock count exceeds threshold (~5,000), SQL Server triggers LOCK ESCALATION:
-- The lock manager converts 5,000 row locks into ONE EXCLUSIVE TABLE LOCK (TABLOCKX)!
UPDATE CustomerAccounts 
SET Status = 'AUDITED' 
WHERE Region = 'MIDWEST';

-- Result: All other readers querying ANY row in CustomerAccounts are now BLOCKED!
COMMIT TRAN;
```

### Example 5: Optimized Row-Level Locking with FOR NO KEY UPDATE
```sql
-- Session 1: Modifying order notes (non-key column)
BEGIN;
SELECT * FROM orders WHERE order_id = 901 FOR NO KEY UPDATE;
UPDATE orders SET internal_notes = 'Reviewed by supervisor' WHERE order_id = 901;

-- Session 2: Concurrent payment insertion referencing orders(order_id)
-- Checks foreign key via FOR KEY SHARE:
INSERT INTO payments (order_id, amount) VALUES (901, 45.00);
-- In PostgreSQL, FOR NO KEY UPDATE does NOT conflict with FOR KEY SHARE!
-- Both transactions succeed concurrently without blocking!
COMMIT; -- Session 1
```

---

## Best Practices

1. **Always Set `lock_timeout` Before DDL**: Never run `ALTER TABLE`, `DROP TABLE`, or `CREATE INDEX` in production without explicitly setting `SET LOCAL lock_timeout = '2s';`. If the lock cannot be acquired within 2 seconds, fail fast instead of queuing up and taking down live traffic.
2. **Use `CREATE INDEX CONCURRENTLY`**: Standard `CREATE INDEX` takes a `SHARE` lock that blocks all writes (`INSERT`, `UPDATE`, `DELETE`). Always use `CONCURRENTLY` in production PostgreSQL, which only acquires `SHARE UPDATE EXCLUSIVE` (permitting full concurrent read and write traffic).
3. **Keep Transactions Containing Writes Short**: Locks are held until the transaction finishes (`COMMIT` or `ROLLBACK`). Minimize the time between your first write operation and `COMMIT`.
4. **Use `FOR NO KEY UPDATE` When Possible**: When explicitly locking a row to update non-unique data, prefer `FOR NO KEY UPDATE` over `FOR UPDATE` to avoid blocking concurrent foreign key checks.
5. **Beware of Lock Escalation in SQL Server**: In SQL Server, batch large updates in chunks of 4,000 rows (`TOP (4000)`) to stay below the lock escalation threshold, or disable escalation explicitly: `ALTER TABLE ... SET (LOCK_ESCALATION = DISABLE);`.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Whole website hangs during a simple ALTER TABLE** | DDL requested `ACCESS EXCLUSIVE`, was queued behind a slow query, and blocked all subsequent `SELECT`s. | Set `lock_timeout = '2s'` and kill long-running queries before migration. |
| **Out-of-memory errors in MySQL lock system** | Locking millions of rows inside a single transaction in MySQL InnoDB (`lock_sys` exhausted). | Batch row modifications in chunks of 5,000-10,000 rows. |
| **Standard CREATE INDEX blocks write API traffic** | Running `CREATE INDEX` instead of `CREATE INDEX CONCURRENTLY` in PostgreSQL. | Always append `CONCURRENTLY` to production index creations. |
| **Foreign key insertions unexpectedly blocked** | Using `SELECT ... FOR UPDATE` instead of `FOR NO KEY UPDATE` on parent records. | Use `FOR NO KEY UPDATE` when primary key / unique columns are not mutated. |
| **Lock contention on SQLite database** | Multiple application threads attempting to write concurrently in default rollback journal mode. | Enable WAL mode (`PRAGMA journal_mode = WAL`) and use a single writer thread/queue. |
| **Deadlock between parent and child tables** | Updating child records and parent foreign keys in non-deterministic order across transactions. | Establish and enforce a strict parent-then-child or primary-key sorted update order. |

---

## Performance & Security Notes

### Why PostgreSQL Row Locks Don't Consume RAM
In engines like SQL Server and MySQL InnoDB, every locked row creates an in-memory lock structure in RAM. Locking 10 million rows consumes gigabytes of RAM.
- PostgreSQL solves this elegantly: row locks are **written directly into the 8 KB table data page** inside the tuple header's `xmax` and `t_infomask` fields.
- Locking 10 million rows in PostgreSQL requires **zero additional Lock Manager RAM**.
- Only table-level locks and wait-queue allocations are tracked in the shared memory lock table.

### Security: Denial of Service via Lock Starvation
An attacker with basic read privileges could intentionally launch a long-running, slow query:
```sql
SELECT count(*) FROM large_table WHERE pg_sleep(300) IS NOT NULL;
```
If an administrative deployment runs `ALTER TABLE` shortly after, the administrative DDL enters the queue behind the attacker, and the lock manager blocks *all* subsequent production traffic, achieving complete denial of service. Strict `statement_timeout` and `lock_timeout` parameters must be enforced at the role or database level.

---

## Comparison Matrix: Lock Modes Conflict Matrix

PostgreSQL Table Lock Conflict Matrix:

| Requested Mode | AccessShare | RowShare | RowExclusive | ShareUpdateExclusive | Share | ShareRowExclusive | Exclusive | AccessExclusive |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **AccessShare** (`SELECT`) | - | - | - | - | - | - | - | **CONFLICT** |
| **RowShare** (`FOR SHARE`) | - | - | - | - | - | - | **CONFLICT** | **CONFLICT** |
| **RowExclusive** (`UPDATE`) | - | - | - | - | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** |
| **ShareUpdateExclusive** (`VACUUM`) | - | - | - | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** |
| **Share** (`CREATE INDEX`) | - | - | **CONFLICT** | **CONFLICT** | - | **CONFLICT** | **CONFLICT** | **CONFLICT** |
| **ShareRowExclusive** | - | - | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** |
| **Exclusive** | - | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** |
| **AccessExclusive** (`ALTER`) | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** | **CONFLICT** |

---

## Real-World Interview Questions

<details>
<summary><strong>1. Explain the "Lock Queue Head-of-Line Blocking Trap" and how a 2-millisecond ALTER TABLE can cause a total site outage.</strong></summary>

**Answer:**
When an `ALTER TABLE` statement executes, it requests an `ACCESS EXCLUSIVE` lock on the table.
1. If a slow `SELECT` is currently running, the `ALTER TABLE` cannot acquire the lock immediately and enters the table's Lock Wait Queue.
2. The database Lock Manager enforces FIFO fairness to prevent the DDL from starving indefinitely: **it blocks all newly arriving queries** (even simple `SELECT` queries requiring only `ACCESS SHARE`) behind the pending `ALTER TABLE`.
3. Within seconds, hundreds of incoming web requests accumulate in the wait queue, exhausting the application server's connection pool. The entire application goes down until either the slow query finishes or the DDL is killed.
4. Prevention: Always set `SET LOCAL lock_timeout = '2s';` before executing DDL.
</details>

<details>
<summary><strong>2. How does PostgreSQL store row-level locks without exhausting database shared memory?</strong></summary>

**Answer:**
Unlike MySQL or SQL Server which maintain in-memory lock structures for every locked row, PostgreSQL stores row locks directly **on disk inside the tuple header**:
- When a row is locked via `SELECT FOR UPDATE` or `UPDATE`, the transaction writes its Transaction ID into the row's `xmax` header field and flips specific bit flags in `t_infomask` (e.g., `HEAP_XMAX_EXCL_LOCK` or `HEAP_XMAX_KEYSHR_LOCK`).
- Concurrent transactions check the tuple header directly. If `xmax` is an active transaction holding an exclusive lock, the concurrent transaction sleeps on the locking transaction's XID.
- Because row locks reside inside the data pages, locking a billion rows consumes zero bytes in the shared memory Lock Table.
</details>

<details>
<summary><strong>3. What is the difference between FOR UPDATE and FOR NO KEY UPDATE in PostgreSQL?</strong></summary>

**Answer:**
- `FOR UPDATE` acquires the strongest row-level lock. It blocks all other row-level locks on that tuple, including `FOR KEY SHARE` (foreign key validations). It is used when modifying primary keys, unique keys, or deleting rows.
- `FOR NO KEY UPDATE` acquires a weaker exclusive lock. It is used automatically by `UPDATE` statements that modify non-key columns. It allows concurrent transactions to acquire `FOR KEY SHARE` locks on the same row. This prevents updates to non-key attributes from blocking foreign key insert validations from child tables.
</details>

<details>
<summary><strong>4. What is Lock Escalation in Microsoft SQL Server, and why doesn't PostgreSQL have it?</strong></summary>

**Answer:**
- In **SQL Server**, every row lock consumes a discrete 96-byte memory structure. If a single query acquires more than ~5,000 row locks, SQL Server automatically **escalates** those row locks into a single table lock (`TABLOCK`) to prevent running out of lock memory. This can unexpectedly block concurrent readers across the entire table.
- **PostgreSQL does not have lock escalation** because row locks do not consume memory (they are stored in tuple headers on disk). A transaction in PostgreSQL can lock 100 million rows without ever escalating to a table lock.
</details>

<details>
<summary><strong>5. What is an Intent Lock, and why is it necessary in a hierarchical locking system?</strong></summary>

**Answer:**
An Intent Lock (such as Intent Shared `IS` or Intent Exclusive `IX`) is placed at a higher level of the lock hierarchy (e.g., at the table level) before a transaction acquires a lock on a lower-level resource (e.g., an individual row).

Without intent locks, whenever a transaction requested an exclusive lock on an entire table (e.g., for DDL), the database engine would have to scan every single page and row in the table to verify that no transaction held a row-level lock. With intent locks, the engine simply checks if the table has an `IX` lock; if it does, it immediately knows a child row is locked, making conflict detection $O(1)$.
</details>

<details>
<summary><strong>6. Why is CREATE INDEX CONCURRENTLY slower than CREATE INDEX, and what locks does it take?</strong></summary>

**Answer:**
- Standard `CREATE INDEX` acquires a `SHARE` lock on the table. It blocks all concurrent writes (`INSERT`, `UPDATE`, `DELETE`) but builds the index in a single fast sequential scan.
- `CREATE INDEX CONCURRENTLY` acquires only `SHARE UPDATE EXCLUSIVE` lock, allowing normal concurrent reads and writes to proceed unhindered.
- However, it is much slower because it must perform **two full table scans** and wait for all concurrent transactions holding snapshots to finish, resolving any concurrent modifications that occurred during the build.
</details>

<details>
<summary><strong>7. How does SQLite handle locking, and what is the difference between SHARED and RESERVED locks?</strong></summary>

**Answer:**
In SQLite's traditional rollback journal mode, locks operate at the file/database level across five states:
- **SHARED**: Any number of connections can hold SHARED locks to read from the database.
- **RESERVED**: Acquired when a connection intends to write in the future. Only one connection can hold a RESERVED lock at a time. Other connections can continue to read (SHARED), but no other connection can begin writing.
- **EXCLUSIVE**: Acquired when the writer flushes dirty pages to the database file. Blocks all readers and writers.
</details>

<details>
<summary><strong>8. What is a Gap Lock in MySQL InnoDB, and what purpose does it serve?</strong></summary>

**Answer:**
A **Gap Lock** in InnoDB locks the open "gap" between two adjacent index records (or before the first record, or after the last record), rather than the record itself. 

Its purpose is to prevent other transactions from inserting new rows into that gap. In `REPEATABLE READ` isolation, gap locks (combined with record locks into **Next-Key Locks**) prevent the Phantom Read anomaly for locking queries like `SELECT ... FOR UPDATE WHERE id BETWEEN 10 AND 20`.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1: Detecting Active Lock Blockages</strong></summary>

**Problem:** Write a SQL query in PostgreSQL to identify which sessions are currently blocked waiting for a lock, and the exact query being executed by the blocking session.

**Solution:**
```sql
SELECT 
    blocked.pid AS blocked_pid,
    blocked.usename AS blocked_user,
    blocked.query AS blocked_query,
    blocking.pid AS blocking_pid,
    blocking.usename AS blocking_user,
    blocking.query AS blocking_query
FROM pg_stat_activity blocked
JOIN pg_locks bl ON bl.pid = blocked.pid AND NOT bl.granted
JOIN pg_locks kl ON kl.locktype = bl.locktype
    AND kl.database IS NOT DISTINCT FROM bl.database
    AND kl.relation IS NOT DISTINCT FROM bl.relation
    AND kl.page IS NOT DISTINCT FROM bl.page
    AND kl.tuple IS NOT DISTINCT FROM bl.tuple
    AND kl.virtualxid IS NOT DISTINCT FROM bl.virtualxid
    AND kl.transactionid IS NOT DISTINCT FROM bl.transactionid
    AND kl.classid IS NOT DISTINCT FROM bl.classid
    AND kl.objid IS NOT DISTINCT FROM bl.objid
    AND kl.objsubid IS NOT DISTINCT FROM bl.objsubid
    AND kl.pid != bl.pid AND kl.granted
JOIN pg_stat_activity blocking ON blocking.pid = kl.pid;
```
</details>

<details>
<summary><strong>Exercise 2: Simulating Lock Queue Head-of-Line Blocking</strong></summary>

**Problem:** Describe the steps to reproduce the Lock Queue blockage in two psql terminals, showing how a third terminal running a simple `SELECT` gets blocked.

**Solution:**
- **Terminal 1**: `BEGIN; SELECT * FROM pg_sleep(60);` (Holds `ACCESS SHARE` on whatever table it touches or runs a slow scan).
- **Terminal 2**: `ALTER TABLE my_table ADD COLUMN test_col INT;` (Requests `ACCESS EXCLUSIVE`, blocks behind T1).
- **Terminal 3**: `SELECT * FROM my_table LIMIT 1;` (Requests `ACCESS SHARE`, but gets queued behind T2).
- Result: Terminal 3 hangs until Terminal 1 finishes or is canceled!
</details>

<details>
<summary><strong>Exercise 3: Safe Non-Blocking Constraint Validation</strong></summary>

**Problem:** Adding a `CHECK` constraint to a 50-million-row table normally locks the table with `ACCESS EXCLUSIVE` for minutes while scanning rows. Write the PostgreSQL two-step pattern to add and validate the constraint without long lock duration.

**Solution:**
```sql
-- Step 1: Add constraint NOT VALID (takes ACCESS EXCLUSIVE for a few milliseconds, does NOT scan data)
ALTER TABLE user_accounts 
ADD CONSTRAINT check_positive_balance 
CHECK (balance >= 0) NOT VALID;

-- Step 2: Validate constraint (takes SHARE UPDATE EXCLUSIVE; scans all rows without blocking reads or writes!)
ALTER TABLE user_accounts 
VALIDATE CONSTRAINT check_positive_balance;
```
</details>

<details>
<summary><strong>Exercise 4: Inspecting Row Lock Waiters</strong></summary>

**Problem:** Using PostgreSQL extension `pgrowlocks`, how can an administrator inspect exactly which rows in table `orders` are currently locked by which transaction ID?

**Solution:**
```sql
CREATE EXTENSION IF NOT EXISTS pgrowlocks;

SELECT 
    locked_row,
    locker,
    multi,
    modes,
    pids
FROM pgrowlocks('orders');
```
</details>

<details>
<summary><strong>Exercise 5: Setting Dynamic Session Lock Timeouts</strong></summary>

**Problem:** Write a PostgreSQL wrapper that sets a lock timeout of 1.5 seconds, performs an `ALTER TABLE`, and resets the setting back to the default.

**Solution:**
```sql
BEGIN;
-- Limit lock acquisition wait time to 1500 milliseconds
SET LOCAL lock_timeout = '1500ms';

ALTER TABLE invoices ADD COLUMN payment_reference VARCHAR(64);

COMMIT;
-- Setting automatically reverts to default upon COMMIT/ROLLBACK
```
</details>

<details>
<summary><strong>Exercise 6: Demonstrating Non-Conflicting Row Locks</strong></summary>

**Problem:** Show that Session 1 holding `FOR NO KEY UPDATE` on an account does not block Session 2 from validating a foreign key reference (`FOR KEY SHARE`) on the same account.

**Solution:**
```sql
-- Session 1
BEGIN;
SELECT * FROM accounts WHERE id = 100 FOR NO KEY UPDATE;

-- Session 2 (Concurrently):
BEGIN;
-- This acquires FOR KEY SHARE on accounts where id = 100
INSERT INTO account_audit_logs (account_id, action) VALUES (100, 'LOGIN');
-- Seduces zero wait time! Executes immediately!
COMMIT;

-- Session 1
COMMIT;
```
</details>

---

## Summary & Further Reading

- Database locks prevent destructive write-write conflicts and serialize schema mutations.
- PostgreSQL implements 8 distinct table lock modes; `ACCESS EXCLUSIVE` blocks all operations.
- The Lock Queue operates in FIFO order: a pending exclusive lock blocks all subsequent shared locks (the Lock Queue Blockage Trap). Always enforce `lock_timeout`.
- PostgreSQL row locks are stored on disk in tuple headers, consuming zero RAM; SQL Server and MySQL InnoDB maintain row locks in RAM and risk lock escalation or memory pressure.
- **Next Topic**: [06-deadlocks-and-diagnosis.md](06-deadlocks-and-diagnosis.md) covers deadlock mechanics, wait-for graphs, cycle detection algorithms, and diagnostic strategies.
