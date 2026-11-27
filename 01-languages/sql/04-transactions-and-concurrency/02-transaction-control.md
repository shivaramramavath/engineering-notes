# Transaction Control: BEGIN, COMMIT, ROLLBACK & SAVEPOINT

## Metadata
| Attribute | Details |
|:---|:---|
| **Topic** | Transaction Boundaries & Control Commands |
| **Dialect** | Standard ANSI SQL / PostgreSQL 17+ Baseline |
| **Database Version** | PostgreSQL 17+, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Tested with** | PostgreSQL 17.2, SQLite 3.50.4 |
| **Prerequisites** | `04-transactions-and-concurrency/01-acid.md` |
| **Complexity** | Intermediate |

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
10. [Comparison Matrix: Transaction Control Commands](#comparison-matrix-transaction-control-commands)
11. [Real-World Interview Questions](#real-world-interview-questions)
12. [Practice Exercises](#practice-exercises)
13. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

In default client interactive configurations, relational database management systems operate in **autocommit mode**: every individual SQL statement is treated as a self-contained transaction that is immediately committed to disk upon execution.

While convenient for ad-hoc querying, autocommit fails completely when executing complex business logic. If an operation requires inserting a parent invoice and ten child line items, or transferring balance across two account ledgers, each individual statement committing independently creates a catastrophic vulnerability: an error midway through leaves partial, corrupted data permanently baked into the database.

**Transaction control commands** allow application developers and database administrators to explicitly demarcate boundaries:
- They establish when a transactional context begins (`BEGIN`).
- They make all mutations permanent and durable (`COMMIT`).
- They cleanly undo all modifications if any error occurs (`ROLLBACK`).
- They create fine-grained checkpoints within a transaction to recover from non-fatal partial failures without throwing away the entire unit of work (`SAVEPOINT`).

---

## Core Concepts

### 1. The Transaction Lifecycle
A database transaction progresses through a formal state machine:
1. **Active**: The transaction begins. DML queries execute, acquiring row locks and modifying buffers in RAM.
2. **Partially Committed**: All SQL statements have finished executing successfully. The client issues `COMMIT`. The engine is now flushing WAL records to disk.
3. **Committed**: WAL records are guaranteed to be durable on disk. The transaction has concluded successfully, locks are released, and changes are visible to other transactions.
4. **Failed**: An error (constraint violation, serialization conflict, deadlock, or syntax error) occurs during statement execution.
5. **Aborted / Rolled Back**: The transaction undoes all intermediate writes, releases all held locks, and returns the database to the pre-transaction state.

```
       +--------------+
       |    BEGIN     |
       +--------------+
              |
              v
       +--------------+      Statement Error / Syntax / Constraint
       |    ACTIVE    | -----------------------------------------------+
       +--------------+                                                |
              |                                                        |
              | All statements successful                              |
              v                                                        |
       +-----------------------+                                       |
       |  PARTIALLY COMMITTED  |                                       |
       +-----------------------+                                       |
              |                                                        |
              | WAL flushed (fsync)                                    v
              v                                                +---------------+
       +--------------+                                        |    FAILED     |
       |  COMMITTED   |                                        +---------------+
       +--------------+                                                |
                                                                       | ROLLBACK
                                                                       v
                                                               +---------------+
                                                               |    ABORTED    |
                                                               +---------------+
```

### 2. Autocommit vs Explicit Demarcation
- **Autocommit Mode (`autocommit = ON`)**: Each statement implicitly runs inside an invisible `BEGIN ... COMMIT` block.
- **Explicit Transaction Mode**: Started by issuing `BEGIN` (or `START TRANSACTION`). The engine disables autocommit for that connection until an explicit `COMMIT` or `ROLLBACK` is issued.

### 3. Savepoints and Subtransactions
A **Savepoint** is an intermediate bookmark created inside an open transaction:
- `SAVEPOINT savepoint_name;`
- `ROLLBACK TO SAVEPOINT savepoint_name;`
- `RELEASE SAVEPOINT savepoint_name;`

Unlike a root transaction commit, rolling back to a savepoint only reverses statements executed *after* the savepoint was created. Statements executed before the savepoint remain active and can still be committed.

---

## How It Works Internally

### Transaction Identifiers (XID / TXID)
When a transaction opens in PostgreSQL or MySQL, the engine does not necessarily allocate a full 32-bit or 64-bit Transaction ID immediately.
- **Read-Only Optimization**: If a transaction only executes `SELECT` statements, PostgreSQL assigns a virtual transaction ID (`vxid`) which incurs no catalog locks and consumes no permanent XID counter space.
- **DML Mutation**: The instant the first `INSERT`, `UPDATE`, or `DELETE` executes, the engine assigns a real, permanent, globally unique Transaction ID (`XID`).

### Savepoint Mechanics and Subtransactions
Internally, PostgreSQL implements savepoints as **subtransactions** (`SubTrans`):
- Each savepoint allocates a subtransaction ID (`SubXID`).
- Subtransactions form a parent-child tree hierarchy in shared memory.
- When `ROLLBACK TO SAVEPOINT` executes, the engine marks the SubXID as aborted in the commit log (`pg_xact`), rewinds the command counter, and invalidates any memory tuples written under that SubXID.
- **Performance Trap**: Creating thousands of savepoints in a single transaction causes the subtransaction cache (`PGPROC.subxids`) to overflow (max 64 subtransactions in Postgres). When overflow occurs, every concurrent visibility check must perform heavyweight I/O against `pg_subtrans` on disk, causing catastrophic database-wide latency spikes.

---

## Syntax & Usage

```sql
-- Explicit ANSI SQL transaction demarcation
BEGIN TRANSACTION; -- or simply BEGIN

-- Command 1
UPDATE orders 
SET status = 'PROCESSING' 
WHERE order_id = 4501;

-- Establish a savepoint
SAVEPOINT payment_gateway_attempt;

-- Attempt an operation that might fail
INSERT INTO order_payments (order_id, amount, payment_method)
VALUES (4501, 199.99, 'CREDIT_CARD');

-- If payment fails, revert ONLY the payment insertion:
-- ROLLBACK TO SAVEPOINT payment_gateway_attempt;

-- If payment succeeds, destroy the savepoint marker to free memory:
RELEASE SAVEPOINT payment_gateway_attempt;

-- Finalize and commit entire transaction
COMMIT;
```

---

## Cross-Dialect Differences

```
+----------------------------------------------------------------------------------------------------+
| DIALECT COMPARISON: TRANSACTION CONTROL STATEMENTS                                                 |
+-------------------+--------------------------------------------------------------------------------+
| PostgreSQL 17+    | BEGIN or START TRANSACTION. COMMIT or END. ROLLBACK or ABORT.                  |
|                   | Supports SAVEPOINT, ROLLBACK TO SAVEPOINT, RELEASE SAVEPOINT.                  |
|                   | DDL statements (ALTER TABLE, CREATE TABLE) ARE fully transactional!           |
+-------------------+--------------------------------------------------------------------------------+
| MySQL 8.4 (InnoDB)| START TRANSACTION or BEGIN. COMMIT, ROLLBACK.                                  |
|                   | Supports SAVEPOINT, ROLLBACK TO SAVEPOINT, RELEASE SAVEPOINT.                  |
|                   | DDL statements trigger an AUTOMATIC IMPLICIT COMMIT before and after execution!|
|                   | Nested START TRANSACTION implicitly commits the previous active transaction!   |
+-------------------+--------------------------------------------------------------------------------+
| SQLite 3.50+      | BEGIN [DEFERRED | IMMEDIATE | EXCLUSIVE] TRANSACTION. COMMIT, ROLLBACK.       |
|                   | Supports SAVEPOINT sp_name, ROLLBACK TO sp_name, RELEASE sp_name.              |
|                   | In SQLite, SAVEPOINT can even be used as an alternative syntax to start a tx.  |
+-------------------+--------------------------------------------------------------------------------+
| SQL Server 2022   | BEGIN TRAN / COMMIT TRAN / ROLLBACK TRAN.                                      |
|                   | SAVE TRANSACTION savepoint_name / ROLLBACK TRAN savepoint_name.                |
|                   | Named transactions like BEGIN TRAN T1 exist, but are NOT true nested txns!    |
|                   | @@TRANCOUNT tracks nesting depth; only outermost COMMIT flushes to disk.       |
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Basic Explicit Transaction with Clean Rollback
```sql
-- PostgreSQL & SQLite compatible
BEGIN;

INSERT INTO customer_notifications (customer_id, message_body)
VALUES (104, 'Your account balance is ready to review.');

-- Explicitly undo work if testing or aborted:
ROLLBACK;

-- Verify table is unchanged
SELECT COUNT(*) FROM customer_notifications WHERE customer_id = 104;
-- Result: 0
```

### Example 2: Financial Wire Transfer with Balance Verification
```sql
BEGIN;

-- 1. Deduct funds from sender
UPDATE bank_accounts
SET balance = balance - 500.00
WHERE account_id = 'ACT_SENDER'
  AND balance >= 500.00;

-- 2. Credit funds to receiver
UPDATE bank_accounts
SET balance = balance + 500.00
WHERE account_id = 'ACT_RECEIVER';

-- 3. Log audit journal
INSERT INTO journal_entries (entry_type, from_account, to_account, amount)
VALUES ('WIRE_TRANSFER', 'ACT_SENDER', 'ACT_RECEIVER', 500.00);

COMMIT;
```

### Example 3: Edge Case — Handling Third-Party Payment Failures via Savepoint
```sql
BEGIN;

-- 1. Reserve flight seat
UPDATE flight_seats
SET reservation_status = 'HELD',
    held_by_user_id = 9021
WHERE flight_id = 'FL_884' AND seat_number = '14B' AND reservation_status = 'AVAILABLE';

-- 2. Create checkpoint before external service tracking
SAVEPOINT loyalty_redemption;

-- Attempt to deduct loyalty reward points
UPDATE loyalty_points
SET available_points = available_points - 50000
WHERE user_id = 9021 AND available_points >= 50000;

-- Suppose user did NOT have 50,000 points (rows affected = 0 or constraint violated):
-- Revert the loyalty deduction attempt:
ROLLBACK TO SAVEPOINT loyalty_redemption;

-- Switch to cash charging model instead
INSERT INTO flight_cash_invoices (user_id, flight_id, seat_number, amount_usd)
VALUES (9021, 'FL_884', '14B', 450.00);

-- Seat reservation remains held and valid!
COMMIT;
```

### Example 4: Failure Mode — The PostgreSQL "Aborted Transaction" Lockout
```sql
BEGIN;

INSERT INTO audit_log (event_name) VALUES ('STAGE_1_SUCCESS');

-- Syntax or semantic error: table does not exist
SELECT * FROM non_existent_table_xyz;
-- ERROR: relation "non_existent_table_xyz" does not exist

-- Attempting any subsequent valid query fails immediately:
INSERT INTO audit_log (event_name) VALUES ('STAGE_2_ATTEMPT');
-- ERROR: current transaction is aborted, commands ignored until end of transaction block

-- The only allowed commands are ROLLBACK or COMMIT (which will act as ROLLBACK):
ROLLBACK;
```

### Example 5: Optimized Transactional DDL in PostgreSQL
Demonstrating atomic schema migration with rollback:
```sql
-- In PostgreSQL, DDL is fully transactional!
BEGIN;

-- 1. Create new table
CREATE TABLE feature_flags (
    flag_key VARCHAR(64) PRIMARY KEY,
    is_enabled BOOLEAN NOT NULL DEFAULT FALSE
);

-- 2. Populate defaults
INSERT INTO feature_flags (flag_key, is_enabled)
VALUES ('NEW_CHECKOUT_FLOW', TRUE), ('BETA_SEARCH', FALSE);

-- 3. Add column to existing table
ALTER TABLE user_profiles ADD COLUMN active_flag VARCHAR(64) REFERENCES feature_flags(flag_key);

-- If anything fails, everything rolls back seamlessly.
COMMIT;
```

---

## Best Practices

1. **Avoid the Nested `BEGIN` Myth**: Never issue `BEGIN` inside an already active transaction. In MySQL, this silently commits the outer transaction; in PostgreSQL, it issues a `WARNING: there is already a transaction in progress` and is ignored. Use `SAVEPOINT` for sub-operations.
2. **Always Release or Roll Back Savepoints**: When using savepoints in long procedures, call `RELEASE SAVEPOINT` as soon as the inner block succeeds to prevent subtransaction memory exhaustion.
3. **Handle Errors with Immediate Rollback**: In application frameworks (such as Node.js `pg`, Python `asyncpg` or `psycopg2`, Java JDBC), always wrap transaction execution in a `try ... catch ... finally` block that guarantees `ROLLBACK` executes on exception.
4. **Beware of Implicit Commits**: Never run administrative commands, index creations without concurrently, or DDL inside transactions in MySQL/Oracle, as they trigger non-reversible commits.
5. **Keep Read-Only Transactions Explicit**: If a transaction only reads data, declare it explicitly: `BEGIN READ ONLY;`. This allows the engine to optimize snapshot handling and bypass XID assignment.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Outer transaction unexpectedly commits early** | Issuing a second `BEGIN` statement in MySQL, which triggers an implicit commit of the first transaction. | Use `SAVEPOINT` instead of nested `BEGIN` statements. |
| **Silent database lockups and high CPU** | Creating thousands of savepoints in a loop (e.g., ORM batch inserts with per-row savepoints), blowing the 64-entry `PGPROC` cache. | Batch operations cleanly or avoid savepoints in tight loops. |
| **Unintended partial commits during script failure** | Running a migration script in `psql` without `\set ON_ERROR_STOP on`. By default, `psql` logs errors and keeps executing later lines. | Always include `\set ON_ERROR_STOP on` at the top of PostgreSQL deployment scripts. |
| **`idle in transaction` connection leak** | Opening a transaction in an application, handling an exception, but forgetting to call `ROLLBACK` before returning the connection to the pool. | Ensure connection pool wrappers issue `ROLLBACK` in the error/finally handler. |
| **Lock starvation during massive DDL** | Running `ALTER TABLE` in a transaction without setting a lock timeout. The DDL waits indefinitely behind slow queries, blocking all incoming traffic. | Set `SET LOCAL lock_timeout = '2s';` before DDL in transactions. |
| **Subtransaction ID wraparound hazard** | Long-running processes that generate millions of savepoints, causing rapid consumption of 32-bit transaction IDs. | Commit transactions frequently in small, bounded batches. |

---

## Performance & Security Notes

### The Hidden Overhead of Subtransactions (Savepoints) in PostgreSQL
In PostgreSQL, subtransactions are not free. While a standard transaction is represented by a single entry in memory, each `SAVEPOINT` creates a subtransaction with its own SubXID:
- PostgreSQL caches up to 64 subtransactions per process in memory (`PGPROC`).
- When a transaction exceeds 64 subtransactions (common with reckless ORM nested transaction configurations), the transaction is marked as **overflowed**.
- Once overflowed, every subsequent read operation executed by **any concurrent backend** inspecting these rows must fall back to reading `pg_subtrans` on disk or through buffer lookups to determine commit status. This can degrade query latency across the entire database cluster by 1,000% to 5,000%.

### Read-Only Optimization
Declaring a transaction as read-only:
```sql
BEGIN TRANSACTION READ ONLY;
```
- Informs the engine that no DML will occur.
- Prevents accidental writes (`ERROR: cannot execute INSERT in a read-only transaction`).
- Allows the engine to bypass assigning a heavy Transaction ID (XID), conserving transaction ID space and reducing vacuum maintenance pressure.

---

## Comparison Matrix: Transaction Control Commands

| Command | Action | Locks Released? | Can Be Undone? | Typical Engine Impact |
|:---|:---|:---:|:---:|:---|
| `BEGIN` | Opens transaction context | No | N/A | Allocates virtual XID; takes snapshot |
| `COMMIT` | Persists all changes to disk | Yes | No | Flushes WAL to disk via `fsync()`; releases all locks |
| `ROLLBACK` | Discards all changes | Yes | No | Reverses memory buffers; marks XID aborted |
| `SAVEPOINT name` | Marks checkpoint in transaction | No | Yes | Creates subtransaction; records in SubTrans cache |
| `ROLLBACK TO SAVEPOINT` | Reverts changes back to bookmark | Partial | No | Aborts SubXID; retains pre-savepoint locks |
| `RELEASE SAVEPOINT` | Destroys bookmark name | No | No | Destroys marker; subtransaction commits with parent |

---

## Real-World Interview Questions

<details>
<summary><strong>1. What happens if a client disconnects while a transaction is in the ACTIVE state?</strong></summary>

**Answer:**
When the database server detects that the TCP connection has terminated (via socket hang-up, TCP keep-alive timeout, or OS signal), the backend worker thread or process assigned to that connection immediately aborts the active transaction. 

The engine issues an internal `ROLLBACK`, invalidating all uncommitted writes made in shared memory buffers, releasing all row-level and table-level locks held by that session, and freeing temporary resources. No uncommitted data is ever made permanent.
</details>

<details>
<summary><strong>2. Can you execute DDL statements (like CREATE TABLE or ALTER TABLE) inside a transaction and roll them back?</strong></summary>

**Answer:**
It depends on the database engine:
- **PostgreSQL**: Yes. Almost all DDL statements (including `CREATE TABLE`, `ALTER TABLE`, `DROP TABLE`, `CREATE SCHEMA`, and adding constraints) are fully transactional. If an error occurs or `ROLLBACK` is issued, the schema changes are completely undone. (Only a few commands like `VACUUM`, `CREATE DATABASE`, and `CREATE INDEX CONCURRENTLY` cannot run inside a transaction block).
- **SQLite**: Yes. DDL statements are transactional.
- **MySQL / Oracle**: No. In MySQL (InnoDB), executing any DDL statement triggers an automatic **implicit commit** of any open transaction before running the DDL, and another commit immediately after. They cannot be rolled back.
- **SQL Server**: Yes. DDL is transactional inside `BEGIN TRAN ... ROLLBACK TRAN`.
</details>

<details>
<summary><strong>3. Explain how PostgreSQL implements SAVEPOINT under the hood and why having too many savepoints causes severe performance degradation.</strong></summary>

**Answer:**
PostgreSQL implements each savepoint as a **subtransaction** with its own SubTransaction ID (SubXID). Subtransactions form a tree hierarchy linked to the top-level parent transaction.

Each PostgreSQL backend has a fixed array in shared memory (`PGPROC.subxids`) that can hold up to 64 SubXIDs. If a transaction creates more than 64 active savepoints, it overflows this cache. Once overflowed, other concurrent transactions checking the visibility of rows modified by this transaction can no longer rely on the in-memory array. Instead, they must perform disk or page reads against `pg_subtrans` to look up the subtransaction hierarchy, leading to massive lock contention on the subtransaction SLRU (Simple LRU) cache and severe system-wide throughput drops.
</details>

<details>
<summary><strong>4. What is the difference between ROLLBACK and ROLLBACK TO SAVEPOINT?</strong></summary>

**Answer:**
- `ROLLBACK` completely terminates the entire transaction. All changes made since the initial `BEGIN` are undone, all row and table locks held by the transaction are released, and the connection returns to an idle/autocommit state.
- `ROLLBACK TO SAVEPOINT savepoint_name` only rolls back modifications made **after** that specific savepoint was declared. The parent transaction remains active and open, locks acquired prior to the savepoint are retained, and the application can continue executing new queries and eventually `COMMIT` the remaining work.
</details>

<details>
<summary><strong>5. Why does running BEGIN when a transaction is already active behave differently in PostgreSQL compared to MySQL?</strong></summary>

**Answer:**
- In **MySQL**, issuing a second `START TRANSACTION` or `BEGIN` while a transaction is already active will **silently and implicitly commit** the first transaction, and then open a brand new transaction.
- In **PostgreSQL**, issuing `BEGIN` inside an already active transaction will emit a `WARNING: there is already a transaction in progress` and will simply be ignored; the existing transaction continues unaffected.
</details>

<details>
<summary><strong>6. What is the purpose of the RELEASE SAVEPOINT command?</strong></summary>

**Answer:**
`RELEASE SAVEPOINT savepoint_name` destroys a savepoint bookmark without rolling back the operations performed after it. It retains the changes made since the savepoint was created and merges them into the parent transaction. 

Calling `RELEASE SAVEPOINT` is critical in long-running functions or batch processes to free the subtransaction resources allocated in shared memory and prevent subtransaction cache overflow.
</details>

<details>
<summary><strong>7. How does BEGIN TRANSACTION READ ONLY benefit an engine like PostgreSQL or MySQL?</strong></summary>

**Answer:**
Marking a transaction explicitly as `READ ONLY`:
1. Enforces a safety invariant: any accidental `INSERT`, `UPDATE`, or `DELETE` is immediately rejected.
2. Optimization: PostgreSQL avoids allocating a physical 32-bit Transaction ID (XID), assigning only a virtual XID (`vxid`). This prevents consuming the 2-billion transaction ID limit and eliminates vacuum freeze overhead.
3. In PostgreSQL's `SERIALIZABLE` isolation level, `READ ONLY DEFERRABLE` transactions can wait for a point in time where they are guaranteed to execute without taking any predicate locks and without ever being aborted due to serialization conflicts.
</details>

<details>
<summary><strong>8. What is the danger of setting autocommit = OFF at the database session level?</strong></summary>

**Answer:**
When `autocommit = OFF` (or `SET IMPLICIT_TRANSACTIONS ON` in SQL Server), simply executing a `SELECT` statement implicitly opens a transaction that **never closes** until the client explicitly issues `COMMIT` or `ROLLBACK`.

If an application developer executes `SELECT * FROM users` and does nothing else, the connection remains in `idle in transaction` state indefinitely. This holds open read snapshots, pins vacuum horizons, prevents dead tuple reclamation, causes table bloat, and retains shared locks that block administrative operations and DDL.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1: Safe Multi-Stage Insertion with Rollback Protection</strong></summary>

**Problem:** Write a transaction block that creates a new project record and three task records. If an error occurs inserting the third task, ensure none of the tasks or the project remain in the database.

**Solution:**
```sql
BEGIN;

INSERT INTO projects (project_id, project_name)
VALUES (10, 'Alpha Initiative');

INSERT INTO tasks (task_id, project_id, title)
VALUES (101, 10, 'Task One'), (102, 10, 'Task Two');

-- Suppose task 103 fails a constraint:
-- Simulated error:
-- INSERT INTO tasks (task_id, project_id, title) VALUES (103, 999, 'Orphan Task');

-- If any statement fails, application catches exception and issues:
ROLLBACK;

-- Verify database remains untouched:
SELECT COUNT(*) FROM projects WHERE project_id = 10;
-- Returns 0
```
</details>

<details>
<summary><strong>Exercise 2: Recovering from a Soft Failure with Savepoint</strong></summary>

**Problem:** Using table `coupons (code VARCHAR PRIMARY KEY, discount_pct INT)` and `cart (item_id INT, price NUMERIC)`, write a transaction that inserts an item into the cart, attempts to insert an invalid coupon (`pct = 150` with check `pct <= 100`), recovers via savepoint, inserts a valid coupon (`pct = 15`), and commits.

**Solution:**
```sql
CREATE TEMP TABLE cart (item_id INT PRIMARY KEY, price NUMERIC);
CREATE TEMP TABLE coupons (code VARCHAR PRIMARY KEY, discount_pct INT CHECK (discount_pct <= 100));

BEGIN;

INSERT INTO cart VALUES (1, 49.99);

SAVEPOINT coupon_stage;

-- Invalid coupon attempt (violates check constraint):
-- In application logic, wrap in try/catch:
-- INSERT INTO coupons VALUES ('SUPER_DISCOUNT', 150); -- Fails

-- Application rolls back to savepoint:
ROLLBACK TO SAVEPOINT coupon_stage;

-- Insert valid coupon instead:
INSERT INTO coupons VALUES ('VALID_15', 15);

COMMIT;

SELECT * FROM cart;    -- 1 | 49.99
SELECT * FROM coupons; -- VALID_15 | 15
```
</details>

<details>
<summary><strong>Exercise 3: Identifying Open Idle Transactions in PostgreSQL</strong></summary>

**Problem:** Write a query against PostgreSQL's system catalog `pg_stat_activity` that returns all sessions currently stuck in the `idle in transaction` state for more than 5 minutes, showing their process ID, duration, and query.

**Solution:**
```sql
SELECT 
    pid,
    usename,
    client_addr,
    state,
    CURRENT_TIMESTAMP - state_change AS idle_duration,
    query
FROM pg_stat_activity
WHERE state = 'idle in transaction'
  AND (CURRENT_TIMESTAMP - state_change) > INTERVAL '5 minutes'
ORDER BY idle_duration DESC;
```
</details>

<details>
<summary><strong>Exercise 4: Verifying Transactional DDL in PostgreSQL</strong></summary>

**Problem:** Create a temporary table `demo_table (id INT)`. Open a transaction, add a new column `notes TEXT`, insert a row, and then execute `ROLLBACK`. Inspect `demo_table` to verify whether the added column was rolled back.

**Solution:**
```sql
CREATE TEMP TABLE demo_table (id INT);

BEGIN;
ALTER TABLE demo_table ADD COLUMN notes TEXT;
INSERT INTO demo_table VALUES (1, 'Testing transactional DDL');
ROLLBACK;

-- Inspect column existence:
SELECT column_name, data_type 
FROM information_schema.columns 
WHERE table_name = 'demo_table';

-- Result: Only 'id' is present! The 'notes' column was completely rolled back.
```
</details>

<details>
<summary><strong>Exercise 5: Releasing Savepoints to Reclaim Memory</strong></summary>

**Problem:** Write a loop or multi-step script that executes 3 successive steps. Use a savepoint for each step, and immediately call `RELEASE SAVEPOINT` after each step verifies successfully.

**Solution:**
```sql
BEGIN;

-- Step 1
SAVEPOINT step_1;
INSERT INTO processing_queue (job_id, status) VALUES (1, 'RUNNING');
RELEASE SAVEPOINT step_1;

-- Step 2
SAVEPOINT step_2;
UPDATE processing_queue SET status = 'COMPLETED' WHERE job_id = 1;
RELEASE SAVEPOINT step_2;

-- Step 3
SAVEPOINT step_3;
INSERT INTO queue_history (job_id, final_status) VALUES (1, 'COMPLETED');
RELEASE SAVEPOINT step_3;

COMMIT;
```
</details>

<details>
<summary><strong>Exercise 6: Configuring Session-Level Transaction Timeouts</strong></summary>

**Problem:** In PostgreSQL, write the command to configure the current session so that any transaction that remains idle for longer than 10 seconds is automatically terminated by the server.

**Solution:**
```sql
-- Automatically terminates backend if a transaction stays idle without issuing queries
SET idle_in_transaction_session_timeout = '10s';

-- Also set a statement timeout to kill queries running longer than 30s
SET statement_timeout = '30s';
```
</details>

---

## Summary & Further Reading

- `BEGIN` opens an explicit transaction, disabling autocommit for that session.
- `COMMIT` persists changes to disk via Write-Ahead Log `fsync()`, while `ROLLBACK` completely discards uncommitted modifications.
- `SAVEPOINT` creates a subtransaction marker allowing partial rollback; however, exceeding 64 nested savepoints causes PostgreSQL cache overflow and severe degradation.
- PostgreSQL and SQLite support fully transactional DDL; MySQL and Oracle trigger implicit commits before and after DDL.
- **Next Topic**: [03-isolation-levels-and-anomalies.md](03-isolation-levels-and-anomalies.md) covers ANSI SQL isolation levels, Dirty Reads, Non-Repeatable Reads, Phantom Reads, and Write Skew.
