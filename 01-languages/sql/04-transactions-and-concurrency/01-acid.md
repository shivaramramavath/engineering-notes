# ACID Properties Deep Dive

## Metadata
| Attribute | Details |
|:---|:---|
| **Topic** | Transactions & Concurrency: ACID Properties |
| **Dialect** | Standard ANSI SQL / PostgreSQL 17+ Baseline |
| **Database Version** | PostgreSQL 17+, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Tested with** | PostgreSQL 17.2, SQLite 3.50.4 |
| **Prerequisites** | `01-fundamentals/14-insert-update-delete.md`, `01-fundamentals/03-constraints.md` |
| **Complexity** | Intermediate to Advanced |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & Usage](#syntax--usage)
5. [Cross-Dialect Differences](#cross-dialect-differences)
6. [Practical Examples](#practical-examples)
7. [Best Practices](#best-practices)
8. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
9. [Performance & Security Notes](#performance--security-notes)
10. [Comparison Matrix: ACID vs BASE](#comparison-matrix-acid-vs-base)
11. [Real-World Interview Questions](#real-world-interview-questions)
12. [Practice Exercises](#practice-exercises)
13. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

In enterprise data management, a business operation rarely corresponds to a single, isolated SQL statement. Transferring funds from an account to another, booking an airline seat with baggage allowances, or checking out an e-commerce shopping cart requires mutating multiple rows across multiple tables.

If the database crashes halfway through such an operation—or if another user reads the database while the balance has been deducted from Account A but not yet credited to Account B—financial balances vanish into thin air, seats are double-booked, and inventory levels drift into corruption.

A **transaction** is a logical unit of work that bundles multiple SQL operations into a single execution boundary. The **ACID** framework defines the four non-negotiable guarantees an engine must provide to preserve enterprise integrity:
- **Atomicity**: The "all-or-nothing" rule. Either every operation in the transaction succeeds and commits, or every modification is completely aborted and rolled back.
- **Consistency**: Invariant preservation. A transaction transitions the database from one valid state satisfying all schema rules, constraints, and relational invariants to another valid state.
- **Isolation**: Concurrent independence. Uncommitted modifications made by one transaction remain invisible to concurrently executing transactions, preventing destructive race conditions.
- **Durability**: Permanent survival. Once a transaction is acknowledged as committed, its changes survive power outages, operating system crashes, and server hardware reboots.

---

## Core Concepts

### 1. Atomicity (All-or-Nothing)
Atomicity guarantees that intermediate or partial states of a transaction are never left behind in the database heap. If an operation encounters an unhandled error, a constraint violation, or a machine shutdown during execution, the database engine executes an **abort** or **rollback**.
- Even if a transaction performs 500,000 row updates and crashes on row 500,001, the database guarantees that zero rows retain the modified data upon recovery.

### 2. Consistency (Invariant Preservation)
Consistency is often misunderstood. In the ACID model, consistency means that a transaction cannot violate any declared integrity constraints:
- **Relational Invariants**: `NOT NULL`, `CHECK`, `UNIQUE`, `FOREIGN KEY`, and user-defined assertion triggers.
- **Application Invariants**: Business logic rules (such as total sum of debits must equal total sum of credits across accounts).
- If any statement violates a declared schema constraint, the entire transaction is rejected, leaving the existing valid state untouched.

### 3. Isolation (Concurrent Independence)
When hundreds of application threads concurrently read and mutate the same data pages, they must not step on each other's feet. Isolation defines the degree to which concurrently executing transactions are shielded from each other's uncommitted intermediate data.
- Absolute isolation is termed **Serializability**: the concurrent execution of multiple transactions produces the exact same observable state as if they had run strictly one after another in serial order.
- To balance concurrency throughput with isolation safety, SQL engines define multiple isolation levels (Read Committed, Repeatable Read, Serializable).

### 4. Durability (Persistence Guarantee)
Once a client receives a `COMMIT` acknowledgment from the database server, the transaction's changes are guaranteed to persist permanently.
- The engine guarantees this not by immediately writing all modified 8 KB data pages to disk (which would be cripplingly slow due to random disk I/O), but by writing a sequential log record to the **Write-Ahead Log (WAL)** and forcing an OS-level `fsync()` to non-volatile storage before sending the commit response.

---

## How It Works Internally

The implementation of ACID relies on the cooperation of three core subsystems: the **Buffer Pool Manager**, the **Write-Ahead Log (WAL) Engine**, and the **Lock/MVCC Engine**.

```
+-----------------------------------------------------------------------------------------+
|                                    TRANSACTION LIFECYCLE                                |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
| 1. BEGIN                                                                                |
|    - Assign Transaction ID (XID / TXID).                                                |
|    - Acquire read snapshot of the database state.                                       |
|                                                                                         |
| 2. EXECUTE DML (UPDATE / INSERT / DELETE)                                               |
|    - Fetch table page into shared buffer pool (in RAM).                                 |
|    - Apply row lock (or record version tuple in heap/undo log).                         |
|    - Mutate the page in RAM -> Page is now marked DIRTY.                                |
|    - Emit a WAL record describing the byte-level diff to the in-memory WAL buffer.      |
|                                                                                         |
| 3. COMMIT REQUESTED                                                                     |
|    - Engine writes WAL buffer to the WAL log file on disk.                              |
|    - Executes fsync() syscall to flush disk controller caches.                          |
|    - Dirty data pages REMAIN in RAM; they are NOT written to table files yet!          |
|    - Engine returns "COMMIT SUCCESS" to client application.                             |
|                                                                                         |
| 4. ASYNCHRONOUS BACKGROUND FLUSH (CHECKPOINTING)                                        |
|    - Checkpointer periodically sweeps dirty pages from shared buffers to table files.   |
|    - WAL records older than the checkpoint can now be safely recycled or archived.      |
+-----------------------------------------------------------------------------------------+
```

### The WAL Rule (Write-Ahead Logging)
The foundational invariant of modern database storage engines is:
> **The WAL record describing a modification must be written and flushed to durable non-volatile disk before the corresponding dirty data page is written to the table storage file.**

If power fails before the checkpoint flushes dirty pages from RAM:
1. On reboot, the engine reads the last checkpoint record in the WAL.
2. **REDO Phase**: It replays all logged modifications from the checkpoint forward, restoring dirty pages that were lost from RAM.
3. **UNDO Phase**: It scans backwards and rolls back any modifications associated with transactions that were active (uncommitted) at the moment of the crash.

This recovery design is known formally as the **ARIES** (Algorithms for Recovery and Isolation Exploiting Semantics) algorithm.

---

## Syntax & Usage

```sql
-- Standard ANSI SQL Transaction Block
BEGIN; -- or BEGIN TRANSACTION / START TRANSACTION

-- Step 1: Deduct from source
UPDATE bank_accounts
SET balance = balance - 250.00,
    updated_at = CURRENT_TIMESTAMP
WHERE account_id = 'ACC_001'
  AND balance >= 250.00;

-- Step 2: Credit to destination
UPDATE bank_accounts
SET balance = balance + 250.00,
    updated_at = CURRENT_TIMESTAMP
WHERE account_id = 'ACC_002';

-- Step 3: Record audit ledger entry
INSERT INTO transfer_ledger (from_account, to_account, amount, executed_at)
VALUES ('ACC_001', 'ACC_002', 250.00, CURRENT_TIMESTAMP);

-- Complete transaction and make changes durable
COMMIT;
```

If an error occurs or a business check fails:
```sql
-- Abort all operations and restore prior state
ROLLBACK;
```

---

## Cross-Dialect Differences

```
+----------------------------------------------------------------------------------------------------+
| DIALECT DIFFERENCES: TRANSACTION INITIALIZATION & DURABILITY BEHAVIOR                              |
+-------------------+--------------------------------------------------------------------------------+
| PostgreSQL 17+    | BEGIN starts transaction. COMMIT commits, ROLLBACK aborts. Autocommit is client |
|                   | driven. WAL written to pg_wal/. synchronous_commit = on (default) forces fsync.|
+-------------------+--------------------------------------------------------------------------------+
| MySQL 8.4 (InnoDB)| START TRANSACTION or BEGIN. Autocommit is ON by default (SET autocommit = 0).  |
|                   | DDL statements (CREATE/ALTER) trigger IMPLICIT COMMIT! innodb_flush_log_at_trx_ |
|                   | commit = 1 guarantees strict ACID durability on commit.                        |
+-------------------+--------------------------------------------------------------------------------+
| SQLite 3.50+      | BEGIN TRANSACTION (DEFERRED, IMMEDIATE, or EXCLUSIVE). WAL mode                |
|                   | (PRAGMA journal_mode = WAL) allows concurrent readers during writes. PRAGMA    |
|                   | synchronous = FULL forces fsync on commit.                                      |
+-------------------+--------------------------------------------------------------------------------+
| SQL Server 2022   | BEGIN TRANSACTION / COMMIT TRANSACTION. SET IMPLICIT_TRANSACTIONS ON/OFF.      |
|                   | DDL statements are fully transactional and roll back inside BEGIN TRAN.        |
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Basic Balance Transfer with Guard Rails
Demonstrating Atomicity and Consistency via an atomic check:
```sql
-- SQLite & PostgreSQL compatible
BEGIN;

UPDATE bank_accounts
SET balance = balance - 100.00
WHERE account_id = 101 AND balance >= 100.00;

-- If rows affected == 0, application issues ROLLBACK immediately.

UPDATE bank_accounts
SET balance = balance + 100.00
WHERE account_id = 102;

COMMIT;
```

### Example 2: E-Commerce Multi-Item Checkout (Real-World)
Atomic order placement, inventory decrement, and coupon consumption:
```sql
BEGIN;

-- 1. Create order record
INSERT INTO customer_orders (order_id, customer_id, total_amount, order_status)
VALUES (95401, 42, 185.50, 'PENDING');

-- 2. Decrement inventory (ensuring stock does not drop below zero)
UPDATE product_inventory
SET stock_count = stock_count - 2,
    updated_at = CURRENT_TIMESTAMP
WHERE product_sku = 'TECH-HEADPHONES-01'
  AND stock_count >= 2;

-- 3. Mark one-time promotional coupon as consumed
UPDATE discount_coupons
SET times_used = times_used + 1,
    is_active = CASE WHEN times_used + 1 >= max_allowed_uses THEN FALSE ELSE is_active END
WHERE coupon_code = 'SUMMER25'
  AND is_active = TRUE;

-- 4. Record order items
INSERT INTO order_line_items (order_id, product_sku, quantity, unit_price)
VALUES (95401, 'TECH-HEADPHONES-01', 2, 92.75);

COMMIT;
```

### Example 3: Edge Case — Partial Rollback via Savepoints
Rolling back a non-critical optional bonus step without abandoning the primary transaction:
```sql
BEGIN;

-- Primary operation: Subscription renewal
UPDATE user_subscriptions
SET status = 'ACTIVE',
    expires_at = CURRENT_TIMESTAMP + INTERVAL '30 days'
WHERE user_id = 7802;

-- Establish checkpoint
SAVEPOINT bonus_perk_savepoint;

-- Attempt optional operation: Grant promotional partner credits
INSERT INTO partner_referral_credits (user_id, partner_id, credit_amount)
VALUES (7802, 99, 15.00);

-- Suppose partner_referral_credits triggers a foreign key error or partner is inactive:
-- Rollback ONLY to the savepoint:
ROLLBACK TO SAVEPOINT bonus_perk_savepoint;

-- The subscription renewal is still pending and can be committed!
COMMIT;
```

### Example 4: Failure Mode — Unhandled Constraint Violation
What happens when a `CHECK` constraint fails:
```sql
-- Schema definition:
-- CREATE TABLE accounts (id INT PRIMARY KEY, balance NUMERIC(10,2) CHECK (balance >= 0.00));

BEGIN;

UPDATE accounts SET balance = balance + 500 WHERE id = 1;

-- This statement violates the CHECK (balance >= 0.00) constraint:
UPDATE accounts SET balance = balance - 2000 WHERE id = 2; -- ERROR: new row for relation "accounts" violates check constraint

-- In PostgreSQL, once an error occurs in a transaction, the entire transaction is in an ABORTED state.
-- Any subsequent query (even SELECT) will fail with:
-- "ERROR: current transaction is aborted, commands ignored until end of transaction block"

-- The only permitted action is:
ROLLBACK;
```

### Example 5: Optimized Batch Insert with Minimized Transaction Duration
Keeping the transaction window as short as possible to prevent lock contention:
```sql
-- Anti-pattern: Doing slow network I/O or data preparation inside BEGIN ... COMMIT
-- Optimized pattern: Stage data in client RAM, open transaction, bulk insert, commit immediately.

BEGIN;

-- Efficient multi-row insert executes in a few milliseconds
INSERT INTO sensor_telemetry (device_id, reading_timestamp, metric_value)
VALUES 
    ('DEV_A1', '2026-03-31 10:00:00', 42.15),
    ('DEV_A2', '2026-03-31 10:00:00', 88.04),
    ('DEV_A3', '2026-03-31 10:00:00', 12.33),
    ('DEV_A4', '2026-03-31 10:00:00', 65.89);

COMMIT;
```

---

## Best Practices

1. **Keep Transactions as Short as Possible**: Never perform remote HTTP calls, file reading, email sending, or heavy cryptographic hashing inside an open transaction block. Lock duration directly degrades database throughput.
2. **Deterministic Locking Order**: When updating multiple rows or tables within a transaction, always update them in a globally consistent order (e.g., sort account IDs in ascending order: `UPDATE accounts WHERE id IN (10, 45) ORDER BY id`) to completely eliminate deadlocks.
3. **Declare Constraints in the Database**: Never rely exclusively on application code for data consistency. Always enforce `CHECK`, `FOREIGN KEY`, and `NOT NULL` constraints in the relational schema.
4. **Use Explicit Transactions for Multi-Statement Operations**: Relying on autocommit for a sequence of related `UPDATE` and `INSERT` statements will leave the database corrupted if the client or server crashes between statements.
5. **Set Transaction Timeouts**: Configure `idle_in_transaction_session_timeout` (in Postgres) to automatically terminate sessions that abandon open transactions.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Partial data corruption after mid-flight crash** | Executing multiple related DML queries without wrapping them in an explicit `BEGIN ... COMMIT` block (autocommit mode). | Wrap all related operations in an explicit `BEGIN ... COMMIT` transaction. |
| **System freezes, connection pool exhaustion** | Holding an open transaction while awaiting third-party HTTP API responses or payment gateways. | Fetch all external API data **before** opening the SQL transaction. |
| **Silent partial commits in MySQL** | Executing DDL statements (`CREATE TABLE`, `ALTER TABLE`) inside a transaction block in MySQL. | Recognize that MySQL InnoDB triggers an implicit, unpreventable commit before and after DDL. |
| **`current transaction is aborted` loop** | In PostgreSQL, continuing to execute SQL statements after a previous statement in the same block threw an error. | Catch the exception in the application, issue an immediate `ROLLBACK`, or use `SAVEPOINT` for handled errors. |
| **Catastrophic WAL exhaustion and disk out-of-space** | Leaving a transaction open in `idle in transaction` state for hours, preventing WAL recycling and vacuuming. | Configure aggressive transaction timeouts and audit connection leakages. |
| **Phantom double-credit race condition** | Reading balance with a plain `SELECT` and updating with `UPDATE ... SET balance = balance + amount` without isolation locks. | Use `SELECT ... FOR UPDATE` or atomic SQL updates (`UPDATE ... SET balance = balance - X WHERE balance >= X`). |

---

## Performance & Security Notes

### Durability vs Performance Trade-offs
A strict ACID commit requires forcing an `fsync()` syscall to physical storage. Rotating HDDs achieve ~100-200 fsyncs/second; NVMe SSDs achieve ~10,000-50,000 fsyncs/second.
- **Group Commit**: Modern engines do not flush disk for every single client query individually. When 100 concurrent sessions issue `COMMIT` at the same time, the engine flushes all 100 transaction records in a single batched `fsync()` call, dramatically increasing throughput.
- **Asynchronous Commit**: For non-critical analytics or log auditing, setting `synchronous_commit = off` (PostgreSQL) or `innodb_flush_log_at_trx_commit = 2` (MySQL) acknowledges the commit as soon as the WAL is in OS RAM cache, risking at most 1-2 seconds of lost transactions upon power failure in exchange for 5x to 10x higher write throughput.

### Security: Transaction Boundary Hijacking
In web applications, connection pooling (e.g., PgBouncer in transaction pooling mode) requires that sessions clean up their state. If an application crashes without issuing `COMMIT` or `ROLLBACK`, and connection pooling assigns that dirty connection to another client, data leakage or unexpected cross-request pollution can occur. Always ensure connection pools run `DISCARD ALL` or configure poolers to enforce clean transaction boundaries.

---

## Comparison Matrix: ACID vs BASE

| Dimension | ACID (Relational Systems) | BASE (Distributed NoSQL Systems) |
|:---|:---|:---|
| **Core Philosophy** | Immediate, strict mathematical consistency | High availability, partition tolerance, eventual consistency |
| **Stands For** | Atomicity, Consistency, Isolation, Durability | Basically Available, Soft state, Eventual consistency |
| **State at Any Instant** | Guaranteed fully consistent across all valid views | May be temporarily divergent across replicas |
| **Failure Handling** | Abort and rollback whole unit of work | Reconcile conflicts asynchronously (Vector Clocks, CRDTs, LWW) |
| **Throughput Scaling** | Vertically scales easily; horizontal sharding requires complex 2PC | Horizontally scales across thousands of nodes |
| **Typical Systems** | PostgreSQL, MySQL InnoDB, SQL Server, Oracle | Cassandra, DynamoDB, MongoDB (default mode), Redis |
| **Primary Domain** | Financial ledgers, ERP, core banking, inventory reservation | Social media feeds, sensor streams, clickstream tracking |

---

## Real-World Interview Questions

<details>
<summary><strong>1. Explain the difference between Atomicity and Durability in database internals.</strong></summary>

**Answer:**
- **Atomicity** dictates that all operations within a transaction either succeed completely or have zero effect on the persistent state of the database ("all-or-nothing"). It is implemented via undo logs (in MySQL/Oracle) or multi-version tuple tracking (`xmin`/`xmax` in Postgres) and rollback mechanics that restore prior state if execution aborts.
- **Durability** guarantees that once a transaction has been acknowledged as committed, its state modifications will survive any subsequent system crash, power loss, or reboot. It is implemented via the Write-Ahead Log (WAL) and the physical forcing of log buffers to disk via the `fsync()` system call before acknowledging the commit to the client.
</details>

<details>
<summary><strong>2. Why does the Write-Ahead Log (WAL) improve database write performance compared to writing data pages directly to disk?</strong></summary>

**Answer:**
Relational tables and indexes are stored in fixed-size pages (typically 8 KB or 16 KB) scattered across disk files. If a transaction updates 5 rows across 5 tables, flushing those modifications directly to data files would require 5 random, scattered write operations, which incurs heavy seek and rotational/block-allocation latency.

By using WAL, the engine writes a compact, sequential append-only stream of byte-level diffs to a single contiguous log file and performs a single sequential `fsync()`. Random page writes to data files are deferred and batched in memory (the shared buffer pool) and flushed lazily in the background by the checkpointer process.
</details>

<details>
<summary><strong>3. What happens if an application crashes while inside an active transaction before calling COMMIT or ROLLBACK?</strong></summary>

**Answer:**
From the engine's perspective, the transaction remains open until the connection socket closes. 
1. When the operating system or connection pool detects that the client socket terminated, the database backend process terminates and triggers an automatic internal `ROLLBACK`.
2. Any uncommitted row modifications or locks held by that transaction are discarded or invalidated.
3. If the server itself crashes while the transaction is open, the engine's crash recovery process scans the WAL on restart: because the transaction has no `COMMIT` record in the log, the UNDO phase rolls back any modifications it wrote to disk pages.
</details>

<details>
<summary><strong>4. In MySQL, why can executing an ALTER TABLE inside a transaction be dangerous?</strong></summary>

**Answer:**
MySQL (InnoDB) does not support transactional DDL. Executing any DDL statement (such as `CREATE TABLE`, `ALTER TABLE`, `DROP TABLE`, or `TRUNCATE TABLE`) triggers an **implicit commit** of any currently active transaction immediately before the DDL statement executes, and another commit immediately after it finishes.

If an application opens a transaction, deducts money, runs an `ALTER TABLE`, and then throws an exception attempting to call `ROLLBACK`, the financial deduction cannot be rolled back because it was committed implicitly by the DDL statement. (In contrast, PostgreSQL supports fully transactional DDL for almost all operations).
</details>

<details>
<summary><strong>5. What is the ARIES algorithm, and what are its three recovery phases?</strong></summary>

**Answer:**
ARIES (Algorithms for Recovery and Isolation Exploiting Semantics) is the canonical crash-recovery algorithm used by modern relational database engines:
1. **Analysis Phase**: Scans the WAL forward from the last known checkpoint to identify all dirty pages in the buffer pool and all active ("loser") transactions that had not committed when the crash occurred.
2. **REDO Phase**: Replays all logged changes forward from the earliest unwritten log sequence number (LSN) to restore the exact state of the database (both committed and uncommitted) up to the exact millisecond of the crash ("repeating history").
3. **UNDO Phase**: Scans backward through the WAL to reverse all changes made by the active "loser" transactions identified in the Analysis Phase, restoring database invariants.
</details>

<details>
<summary><strong>6. Can a database maintain Consistency if an application does not declare foreign keys or check constraints?</strong></summary>

**Answer:**
In the strict relational model, No. The database engine can only enforce Consistency with respect to the constraints and invariants explicitly declared in its catalog (e.g., `PRIMARY KEY`, `FOREIGN KEY`, `CHECK`, `UNIQUE`, `NOT NULL`).

If business invariants (e.g., "an account cannot have negative balance" or "every order must reference a valid customer") are only implemented in application application code, any bug, rogue script, direct SQL update, or race condition will permanently violate data consistency without the database engine raising an error.
</details>

<details>
<summary><strong>7. What is Group Commit, and what problem does it solve?</strong></summary>

**Answer:**
Flushing WAL records to disk requires an `fsync()` syscall, which is constrained by hardware disk latency (e.g., 50 microseconds on fast SSDs, limiting single-threaded commits to ~20,000/sec).

Group Commit is an optimization where the database engine collects `COMMIT` requests from multiple concurrent client sessions over a tiny microsecond window. Instead of executing an `fsync()` for each individual transaction, the engine writes all pending WAL records to disk and executes a single batched `fsync()`. It then wakes up all waiting client sessions simultaneously, increasing write transaction throughput by orders of magnitude under high concurrency.
</details>

<details>
<summary><strong>8. What is the difference between a Savepoint and a nested transaction?</strong></summary>

**Answer:**
Most major relational database engines (PostgreSQL, MySQL, SQLite) **do not support true nested transactions**. If you issue a second `BEGIN` statement inside an active transaction in MySQL, it implicitly commits the outer transaction.

A **Savepoint** is an internal marker established within a single transaction boundary (`SAVEPOINT sp1`). The application can partially revert work done after the marker (`ROLLBACK TO SAVEPOINT sp1`) without aborting or committing the enclosing outer transaction. The entire transaction remains a single atomic unit that is ultimately committed or aborted as a whole.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1: Atomic Ledger Balancing</strong></summary>

**Problem:** Write an explicit transaction that transfers \$75.00 from account ID 10 to account ID 20 in table `accounts (id INT PRIMARY KEY, balance NUMERIC(10,2))`. Ensure the transfer only occurs if account 10 has sufficient funds.

**Solution:**
```sql
BEGIN;

-- Check and deduct atomically
UPDATE accounts
SET balance = balance - 75.00
WHERE id = 10 AND balance >= 75.00;

-- Credit recipient
UPDATE accounts
SET balance = balance + 75.00
WHERE id = 20;

COMMIT;
```
*(Note: In application code, verify that the first UPDATE affected exactly 1 row; if 0 rows were updated, issue `ROLLBACK` immediately).*
</details>

<details>
<summary><strong>Exercise 2: Simulating Rollback on Invariant Failure</strong></summary>

**Problem:** Create a temporary table `wallet (id INT PRIMARY KEY, coins INT CHECK (coins >= 0))`. Insert a row with 10 coins. In a transaction, attempt to subtract 15 coins, handle the resulting error, and verify the original balance is retained.

**Solution:**
```sql
CREATE TEMP TABLE wallet (id INT PRIMARY KEY, coins INT CHECK (coins >= 0));
INSERT INTO wallet VALUES (1, 10);

BEGIN;
-- This will fail with a check constraint violation
UPDATE wallet SET coins = coins - 15 WHERE id = 1;

-- Transaction is now aborted
ROLLBACK;

-- Verify original data is intact
SELECT * FROM wallet WHERE id = 1;
-- Returns: 1 | 10
```
</details>

<details>
<summary><strong>Exercise 3: Implementing Partial Rollback with Savepoint</strong></summary>

**Problem:** In a transaction, insert an order for customer 501. Create a savepoint. Attempt to insert a discount record with a negative percentage (violating a constraint). Roll back to the savepoint and commit the order.

**Solution:**
```sql
CREATE TEMP TABLE orders (order_id INT PRIMARY KEY, customer_id INT);
CREATE TEMP TABLE discounts (order_id INT, pct INT CHECK (pct > 0));

BEGIN;

INSERT INTO orders VALUES (1001, 501);

SAVEPOINT discount_attempt;

-- Violates check constraint
INSERT INTO discounts VALUES (1001, -10); -- Fails

ROLLBACK TO SAVEPOINT discount_attempt;

-- Commit primary order
COMMIT;

SELECT * FROM orders; -- Row 1001 exists
SELECT * FROM discounts; -- Empty
```
</details>

<details>
<summary><strong>Exercise 4: Investigating Autocommit Behavior</strong></summary>

**Problem:** Explain what happens in MySQL vs PostgreSQL when the following commands are run without an explicit `BEGIN`:
```sql
UPDATE inventory SET stock = stock - 1 WHERE item_id = 99;
UPDATE inventory SET stock = stock + 1 WHERE item_id = 100;
```

**Solution:**
In both engines, standard interactive client sessions have `autocommit = ON` by default.
Each statement executes as its own independent single-statement transaction.
If the database or server crashes after the first statement succeeds but before the second statement executes, item 99's stock is permanently decremented and item 100's stock is never incremented, violating atomicity and business consistency.
</details>

<details>
<summary><strong>Exercise 5: Designing a Safe Idempotent Payment Record</strong></summary>

**Problem:** Write a DDL constraint and an INSERT transaction that guarantees that duplicate webhook notifications for charge ID `ch_98765` can never record duplicate payments.

**Solution:**
```sql
CREATE TABLE payments (
    payment_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    idempotency_key VARCHAR(128) NOT NULL UNIQUE,
    amount NUMERIC(10,2) NOT NULL,
    status VARCHAR(32) NOT NULL,
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
);

-- Idempotent insertion
BEGIN;

INSERT INTO payments (idempotency_key, amount, status)
VALUES ('ch_98765', 49.99, 'SUCCEEDED')
ON CONFLICT (idempotency_key) DO NOTHING;

COMMIT;
```
</details>

<details>
<summary><strong>Exercise 6: Measuring Transaction Overhead</strong></summary>

**Problem:** Contrast the performance of inserting 10,000 rows one by one with autocommit enabled versus wrapping all 10,000 inserts inside a single `BEGIN ... COMMIT` block.

**Solution:**
- **With Autocommit (10,000 transactions)**: The database must flush and `fsync()` the WAL log file 10,000 separate times. On typical SSD storage, this takes ~5 to 15 seconds.
- **Inside 1 Explicit Transaction**: The database writes all 10,000 row records into in-memory WAL buffers and executes a single `fsync()` on `COMMIT`. This completes in ~15 to 40 milliseconds (over 100x faster).
</details>

---

## Summary & Further Reading

- **Atomicity** ensures an all-or-nothing outcome for multi-statement operations.
- **Consistency** ensures every transaction preserves all relational integrity constraints (`NOT NULL`, `CHECK`, `FK`).
- **Isolation** determines how concurrent transactions observe each other's intermediate state.
- **Durability** guarantees that committed data survives crashes via sequential Write-Ahead Logging (WAL) and `fsync()`.
- **Next Topic**: [02-transaction-control.md](02-transaction-control.md) details the exact syntax, transaction life-cycle commands, and savepoints across database engines.
