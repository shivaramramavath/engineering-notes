# Deadlocks: Mechanics, Prevention & Production Diagnosis

## Metadata
| Attribute | Details |
|:---|:---|
| **Topic** | Deadlock Detection, Wait-For Graphs & Diagnostic Root-Cause Analysis |
| **Dialect** | Standard ANSI SQL / PostgreSQL 17+ Baseline |
| **Database Version** | PostgreSQL 17+, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Tested with** | PostgreSQL 17.2, SQLite 3.50.4 |
| **Prerequisites** | `04-transactions-and-concurrency/05-locking.md` |
| **Complexity** | Advanced |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts & Coffman Conditions](#core-concepts--coffman-conditions)
3. [How It Works Internally: Wait-For Graphs](#how-it-works-internally-wait-for-graphs)
4. [The Anatomy of a Deadlock Log](#the-anatomy-of-a-deadlock-log)
5. [Syntax & Diagnostic Queries](#syntax--diagnostic-queries)
6. [Cross-Dialect Differences](#cross-dialect-differences)
7. [Practical Examples](#practical-examples)
8. [Deadlock Prevention Strategies](#deadlock-prevention-strategies)
9. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
10. [Performance & Security Notes](#performance--security-notes)
11. [Comparison Matrix: Deadlock Handling by Engine](#comparison-matrix-deadlock-handling-by-engine)
12. [Real-World Interview Questions](#real-world-interview-questions)
13. [Practice Exercises](#practice-exercises)
14. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

In concurrent relational databases, transactions acquire locks dynamically as they execute SQL statements. If Transaction A holds a lock on Row 1 and attempts to acquire a lock on Row 2, while Transaction B simultaneously holds a lock on Row 2 and attempts to acquire a lock on Row 1, neither transaction can proceed. Both sessions freeze, waiting indefinitely for the other to release the required lock.

This circular dependency is a **Deadlock**.

A deadlock is not a software crash or an engine bug: it is a mathematical property of concurrent execution with mutual exclusion. Because neither transaction can make forward progress on its own, the database engine's **Deadlock Detector** must intervene, actively identify the circular wait condition, choose one of the transactions as a **victim**, abort it, and roll back its modifications so the other transaction can complete.

Understanding how deadlocks occur, how engines detect them, and how to write deadlock-free transactional code is a mandatory competency for production database engineering.

---

## Core Concepts & Coffman Conditions

In computer science, a deadlock can occur if and only if all four **Coffman Conditions** hold simultaneously:

1. **Mutual Exclusion**: At least one resource (table, page, or row) must be held in a non-shareable, exclusive mode (`X` lock).
2. **Hold and Wait**: A transaction must currently hold at least one resource while actively requesting another resource currently held by another transaction.
3. **No Preemption**: Resources cannot be forcibly taken away from a transaction while it is active; locks are only released voluntarily when the transaction issues `COMMIT` or `ROLLBACK`.
4. **Circular Wait**: A closed chain of transactions exists such that $T_1$ waits for a resource held by $T_2$, $T_2$ waits for $T_3$, ..., and $T_n$ waits for a resource held by $T_1$.

Eliminating **even one** of these four conditions mathematically guarantees that deadlocks are impossible. In database programming, we eliminate **Circular Wait** by enforcing strict, deterministic **Lock Ordering**.

---

## How It Works Internally: Wait-For Graphs

The database engine models transaction dependencies internally using a directed graph called a **Wait-For Graph (WFG)**:
- **Vertices ($V$)**: Represent active transactions ($T_1, T_2, \dots$).
- **Directed Edges ($E$)**: An edge $T_1 \rightarrow T_2$ indicates that transaction $T_1$ is currently blocked waiting for a lock held by transaction $T_2$.

```
+-----------------------------------------------------------------------------------------+
|                                    WAIT-FOR GRAPH CYCLE                                 |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|       Holds Lock on Row A                             Holds Lock on Row B               |
|    +-----------------------+                       +-----------------------+            |
|    |     Transaction 1     |                       |     Transaction 2     |            |
|    |      (PID: 10452)     |                       |      (PID: 10488)     |            |
|    +-----------------------+                       +-----------------------+            |
|           ^          \                                   /          ^                   |
|           |           \                                 /           |                   |
|           |            \  Requests Lock on Row B       /            |                   |
|  Requests |             \   (Blocked by T2)           /             |                   |
|   Lock on |              v                           v              |                   |
|    Row A  +---------------------------------------------------------+                   |
|           |               CIRCULAR DEPENDENCY DETECTED!             |                   |
|           |                 Cycle: T1 -> T2 -> T1                   |                   |
|           +---------------------------------------------------------+                   |
|                                                                                         |
| Resolution: Engine aborts T1 (Victim). T2 acquires Row A and commits.                   |
+-----------------------------------------------------------------------------------------+
```

### The Detection Algorithm
Running cycle detection continuously on every lock request would cause severe CPU overhead. Engines use an optimization:
1. When a transaction is blocked on a lock, it simply goes to sleep.
2. The engine starts a timer: `deadlock_timeout` (in PostgreSQL, default is **1 second**; in SQL Server, 5 seconds).
3. If the transaction is still blocked when the timer expires, the engine awakens the **Deadlock Detection Worker**.
4. The worker builds the Wait-For Graph and executes a cycle-detection algorithm (e.g., Depth-First Search with back-edge detection or Tarjan's strongly connected components).
5. If a cycle is detected, the engine selects a victim, raises SQLSTATE `40P01` (`deadlock_detected`), and terminates that transaction's query.

---

## The Anatomy of a Deadlock Log

When a deadlock occurs in PostgreSQL, the server log outputs a comprehensive forensic report:

```
ERROR:  deadlock detected
DETAIL:  Process 14201 waits for ShareLock on transaction 89104; blocked by process 14205.
Process 14205 waits for ExclusiveLock on tuple (42, 18) of relation 16384 of database 12001; blocked by process 14201.
HINT:  See server log for query details.
CONTEXT:  while updating tuple (42, 18) in relation "bank_accounts"
STATEMENT:  UPDATE bank_accounts SET balance = balance - 100 WHERE account_id = 'ACC_02';
```

### Deconstructing the Forensic Evidence:
1. **PIDs**: Process 14201 ($T_1$) and Process 14205 ($T_2$).
2. **Blocked by**: Process 14201 was waiting for transaction 89104 (held by 14205). Process 14205 was waiting for a tuple lock held by 14201.
3. **Relation**: OID 16384 corresponds to `bank_accounts`.
4. **Tuple**: Block 42, Offset 18.
5. **Victim**: Process 14201 was chosen as the victim and aborted with `40P01`.

---

## Syntax & Diagnostic Queries

### Inspecting Historical Deadlock Counts
```sql
-- In PostgreSQL, view cumulative deadlock counts across databases
SELECT datname, deadlocks FROM pg_stat_database WHERE datname = current_database();
```

### Diagnosing Active Blockages
```sql
SELECT 
    blocked.pid AS blocked_pid,
    blocked.usename AS blocked_user,
    blocked.query AS blocked_statement,
    blocking.pid AS blocking_pid,
    blocking.usename AS blocking_user,
    blocking.query AS blocking_statement,
    CURRENT_TIMESTAMP - blocked.query_start AS waiting_duration
FROM pg_stat_activity blocked
JOIN pg_locks bl ON bl.pid = blocked.pid AND NOT bl.granted
JOIN pg_locks kl ON kl.locktype = bl.locktype
    AND kl.database IS NOT DISTINCT FROM bl.database
    AND kl.relation IS NOT DISTINCT FROM bl.relation
    AND kl.page IS NOT DISTINCT FROM bl.page
    AND kl.tuple IS NOT DISTINCT FROM bl.tuple
    AND kl.transactionid IS NOT DISTINCT FROM bl.transactionid
    AND kl.pid != bl.pid AND kl.granted
JOIN pg_stat_activity blocking ON blocking.pid = kl.pid;
```

---

## Cross-Dialect Differences

```
+----------------------------------------------------------------------------------------------------+
| DIALECT COMPARISON: DEADLOCK DETECTION & RESOLUTION BEHAVIOR                                       |
+-------------------+--------------------------------------------------------------------------------+
| PostgreSQL 17+    | Configured via deadlock_timeout (default 1s). Cycle detected via WFG in shared  |
|                   | memory. Aborts with SQLSTATE 40P01. Server logs full query context and queries.|
+-------------------+--------------------------------------------------------------------------------+
| MySQL 8.4 (InnoDB)| Continuous deadlock detection enabled by default (innodb_deadlock_detect = ON).|
|                   | Automatically rolls back the transaction that modified the FEWEST rows (victim)|
|                   | View latest report with: SHOW ENGINE INNODB STATUS;                            |
+-------------------+--------------------------------------------------------------------------------+
| SQLite 3.50+      | Returns SQLITE_BUSY when database is locked. busy_timeout sets how long to wait|
|                   | before failing. Does not construct WFG; uses retry backoff loops.              |
+-------------------+--------------------------------------------------------------------------------+
| SQL Server 2022   | Deadlock Monitor runs every 5s (drops to 100ms when deadlocks found). Chooses  |
|                   | victim based on SET DEADLOCK_PRIORITY (LOW, NORMAL, HIGH) or rollback cost.    |
|                   | Emits error 1205. Extended Events trace system_health captures XML deadlock graph.|
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Classic AB-BA Deadlock Reproduction
Demonstrating the exact sequence that triggers a deadlock:
```sql
-- Session 1 (T1)
BEGIN;
UPDATE accounts SET balance = balance - 50 WHERE id = 1; -- Holds lock on id = 1

-- Session 2 (T2) (Run in separate terminal)
BEGIN;
UPDATE accounts SET balance = balance - 30 WHERE id = 2; -- Holds lock on id = 2

-- Session 1 (T1) attempts to update id = 2:
UPDATE accounts SET balance = balance + 50 WHERE id = 2;
-- Session 1 BLOCKS, waiting for Session 2...

-- Session 2 (T2) attempts to update id = 1:
UPDATE accounts SET balance = balance + 30 WHERE id = 1;
-- Both sessions are now waiting on each other!

-- Within 1 second (deadlock_timeout), PostgreSQL outputs:
-- ERROR: deadlock detected
-- DETAIL: Process 8901 waits for ShareLock on transaction ...; blocked by process 8905.
```

### Example 2: Resolving the AB-BA Deadlock via Ordered Locking
The definitive, mathematical solution: **always lock resources in ascending primary key order**:
```sql
-- Transfer between Account 1 and Account 2:
-- Application rule: Always sort IDs before issuing queries!
-- Min ID: 1, Max ID: 2

BEGIN;

-- Lock or update in deterministic sorted order
UPDATE accounts SET balance = balance - 50 WHERE id = 1;
UPDATE accounts SET balance = balance + 50 WHERE id = 2;

COMMIT;

-- Even if another transaction transfers from Account 2 to Account 1:
-- It MUST ALSO update id = 1 FIRST, and id = 2 SECOND!
-- Result: The second transaction simply waits in line on id = 1. NO DEADLOCK CAN OCCUR!
```

### Example 3: Edge Case — Deadlock Caused by Foreign Key Checks
Parent-child table deadlocks without obvious query overlap:
```sql
-- Table A: orders (order_id PK)
-- Table B: order_items (item_id PK, order_id FK REFERENCES orders)

-- Session 1: Inserts into child table (acquires FOR KEY SHARE on orders row 50)
BEGIN;
INSERT INTO order_items (item_id, order_id, price) VALUES (1, 50, 19.99);

-- Session 2: Updates orders row 60, then attempts to update orders row 50
BEGIN;
UPDATE orders SET status = 'PROCESSING' WHERE order_id = 60;
UPDATE orders SET status = 'PROCESSING' WHERE order_id = 50; -- BLOCKED by Session 1!

-- Session 1 now attempts to insert another item for orders row 60:
INSERT INTO order_items (item_id, order_id, price) VALUES (2, 60, 29.99);
-- DEADLOCK OCCURS!
```

### Example 4: Failure Mode — Deadlock Induced by Unindexed Foreign Keys
In older engines or specific locking profiles, updating a parent row requires a full table scan or share lock on the child table if the foreign key column lacks an index:
```sql
-- Anti-Pattern: Missing index on foreign key
CREATE TABLE parent (id INT PRIMARY KEY);
CREATE TABLE child (id INT PRIMARY KEY, parent_id INT REFERENCES parent(id));
-- NO INDEX ON child(parent_id)!

-- Session 1: DELETE FROM parent WHERE id = 10;
-- Must lock or scan the ENTIRE child table to verify no children exist!
-- Result: Conflicts with concurrent inserts into unrelated parent records, creating deadlocks!
```

### Example 5: Optimized Deadlock Retry Wrapper in Application Code
```sql
-- Application pseudocode / transaction execution pattern
-- Handle SQLSTATE 40P01 cleanly
DO $$
DECLARE
    retries INT := 0;
    max_retries INT := 3;
BEGIN
    LOOP
        BEGIN
            -- Attempt transaction
            PERFORM execute_balance_transfer(10, 20, 100.00);
            EXIT; -- Success, leave loop
        EXCEPTION
            WHEN deadlock_detected THEN
                retries := retries + 1;
                IF retries > max_retries THEN
                    RAISE EXCEPTION 'Transaction failed after % deadlock retries', max_retries;
                END IF;
                -- Exponential backoff with jitter
                PERFORM pg_sleep(random() * 0.1 * power(2, retries));
        END;
    END LOOP;
END $$;
```

---

## Deadlock Prevention Strategies

1. **Deterministic Lock Ordering**: The single most effective deadlock prevention technique. Always acquire locks and update rows in the exact same global order across all application services (e.g., sort IDs numerically or lexicographically: `SELECT ... WHERE id IN (1, 4, 12) ORDER BY id FOR UPDATE`).
2. **Batch Atomic Updates**: Instead of looping through an array of items in application code and updating rows one-by-one, execute a single set-based SQL update:
   ```sql
   UPDATE inventory 
   SET stock = stock - delta.qty
   FROM unnest($1::int[], $2::int[]) AS delta(item_id, qty)
   WHERE inventory.item_id = delta.item_id;
   ```
3. **Index All Foreign Keys**: Always create indexes on all foreign key columns (`CREATE INDEX idx_child_fk ON child(parent_id);`). This prevents parent updates and deletes from acquiring broad table-level locks on the child table.
4. **Use Explicit Lock Timeouts**: Set `statement_timeout` and `lock_timeout`. If a query waits longer than the timeout, it aborts before creating complex multi-node deadlock chains.
5. **Minimize Transaction Scope**: Do not hold transactions open across remote network calls, message bus publishing, or user interaction.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Frequent `40P01 deadlock_detected` in payment APIs** | Updating source and destination accounts in random order based on request parameters (`from_account` vs `to_account`). | Sort account IDs before updating: always lock `LEAST(acc1, acc2)` first, then `GREATEST(acc1, acc2)`. |
| **Deadlock spikes during batch import jobs** | Parallel worker threads inserting/updating rows into the same table in arbitrary batch orders. | Partition batch inputs into disjoint sets, or sort items by primary key before bulk ingestion. |
| **Silent application crashes on deadlock** | Failing to catch SQLSTATE `40P01` in the database client driver, treating it as an unrecoverable server fault. | Implement automated retry logic with exponential backoff for transient deadlock exceptions. |
| **Cascading deadlocks during cascading DELETE** | Deleting parent rows where child tables have unindexed foreign keys, forcing table-level lock conflicts. | Add explicit B-Tree indexes on every foreign key column in all child tables. |
| **MySQL deadlocks on INSERT of duplicate keys** | Concurrent transactions inserting duplicate keys trigger gap locks in InnoDB that deadlock on rollback. | Use `INSERT ... ON DUPLICATE KEY UPDATE` instead of `INSERT` followed by exception catch. |
| **Deadlock detector CPU saturation** | Setting `deadlock_timeout` to an excessively low value (e.g., `10ms`), forcing continuous WFG cycle scans under high concurrency. | Keep `deadlock_timeout` between `500ms` and `1000ms`. |

---

## Performance & Security Notes

### The Cost of Deadlock Detection
While deadlock detection is efficient, running it in high-contention systems has a real cost:
- The deadlock detector must take a shared lock on the global Lock Manager shared memory structures to traverse the Wait-For Graph.
- If hundreds of transactions are blocked simultaneously, searching for cycles in large graphs ($O(V + E)$) increases lock manager spinlock contention, causing CPU spikes.
- Tuning `deadlock_timeout` too low (e.g., 20ms) causes the detector to run unnecessarily when transactions would have naturally unlocked themselves within 50ms.

### Security: Intentional Denial of Service via Distributed Deadlocks
A malicious tenant with query access could intentionally construct cross-table circular wait chains across long transactions, consuming connection pool slots and triggering heavy deadlock detection passes. Setting per-user `lock_timeout` and restricting transaction duration prevents intentional resource exhaustion.

---

## Comparison Matrix: Deadlock Handling by Engine

| Feature | PostgreSQL 17+ | MySQL 8.4 (InnoDB) | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Detection Mechanism** | Wait-For Graph (WFG) | Wait-For Graph (WFG) | None (Timeout based) | Wait-For Graph (WFG) |
| **Detection Interval** | Configured by `deadlock_timeout` (default 1s) | Immediate upon lock wait | Waits for `busy_timeout` | Runs every 5s (drops to 100ms) |
| **Victim Selection** | Aborts the transaction that completed the cycle | Aborts transaction with lowest undo log weight | Fails the statement attempting the lock | Lowest `DEADLOCK_PRIORITY` or lowest log size |
| **Error Code** | `40P01` | `1213` (ER_LOCK_DEADLOCK) | `SQLITE_BUSY` (Code 5) | `1205` |
| **Diagnostic View** | Server logs; `pg_stat_database.deadlocks` | `SHOW ENGINE INNODB STATUS` | N/A | Extended Events (`system_health`) |

---

## Real-World Interview Questions

<details>
<summary><strong>1. What are the four Coffman conditions, and which one is easiest to eliminate in application SQL design?</strong></summary>

**Answer:**
The four Coffman conditions are:
1. Mutual Exclusion
2. Hold and Wait
3. No Preemption
4. Circular Wait

The easiest condition to eliminate in software design is **Circular Wait**. 
By establishing a strict **global lock ordering rule**—ensuring that every transaction in the system acquires locks on shared resources in the exact same sequence (e.g., always sorting primary keys in ascending order before updating or locking)—it is mathematically impossible for a cycle to form in the Wait-For Graph, completely eliminating deadlocks.
</details>

<details>
<summary><strong>2. How does PostgreSQL decide which transaction to abort as the deadlock victim?</strong></summary>

**Answer:**
In PostgreSQL, when the deadlock detector traverses the Wait-For Graph and identifies a directed cycle, it selects as the victim the **transaction that completed the cycle** (the one whose lock request closed the loop) or the transaction whose cancellation requires the least disruption to unblock the cycle. 

Unlike MySQL InnoDB (which inspects the number of rows modified and aborts the smallest transaction), PostgreSQL does not compute rollback costs; it aborts the current querying process that triggered the cycle detection check.
</details>

<details>
<summary><strong>3. Why does unindexed foreign keys cause deadlocks in high-throughput applications?</strong></summary>

**Answer:**
When a row in a parent table is updated or deleted, the database engine must check child tables to verify referential integrity (e.g., ensuring no orphaned child records exist, or applying `ON DELETE CASCADE`).

If the foreign key column in the child table lacks an index, the engine cannot perform an efficient index lookup. In many engines (or under specific lock strengths), this forces a full scan of the child table or broad table-level share locks. If concurrent transactions are inserting or updating other rows in the child table, these broad locks conflict, causing circular wait conditions and severe deadlocks.
</details>

<details>
<summary><strong>4. How should an application distinguish between a transient deadlock error and a permanent SQL error?</strong></summary>

**Answer:**
An application must inspect the database driver's standardized SQLSTATE error code:
- **Transient Deadlock**: SQLSTATE **`40P01`** (in PostgreSQL) or MySQL error **`1213`** / SQL Server error **`1205`**. This indicates a concurrency race condition, not a logic flaw. The transaction should be automatically retried with exponential backoff and jitter.
- **Permanent Errors**: Constraint violations (e.g., `23505` unique violation, `23503` foreign key violation) or syntax/schema errors (`42P01` undefined table). These will never succeed upon retry and must immediately bubble up as application exceptions.
</details>

<details>
<summary><strong>5. What is the difference between lock_timeout and deadlock_timeout in PostgreSQL?</strong></summary>

**Answer:**
- **`deadlock_timeout`** (default: 1 second): Specifies the amount of time a transaction will sleep while waiting for a lock *before* the engine triggers the Deadlock Detector to check if the session is part of a circular wait condition. If no deadlock exists, the transaction continues waiting indefinitely (unless `lock_timeout` is set).
- **`lock_timeout`** (default: 0 / disabled): Specifies the absolute maximum duration a statement will wait to acquire any lock. If the lock is not granted within this threshold, the statement is aborted immediately with `55P03` (`lock_not_available`), regardless of whether a deadlock exists.
</details>

<details>
<summary><strong>6. Explain how Next-Key Locks in MySQL InnoDB can cause deadlocks on concurrent INSERT operations.</strong></summary>

**Answer:**
In MySQL InnoDB, when a transaction attempts an `INSERT` that encounters a unique key collision or executes `SELECT ... FOR UPDATE` on a non-existent range, InnoDB acquires **Gap Locks** or **Next-Key Locks**.

If Transaction 1 inserts a value and holds a lock, and Transaction 2 and Transaction 3 both attempt to insert the same key, both T2 and T3 acquire shared next-key locks waiting for T1. If T1 rolls back, both T2 and T3 wake up and simultaneously attempt to upgrade their shared locks to exclusive locks to perform the insert. Neither can proceed because each blocks the other's lock upgrade, producing an immediate deadlock.
</details>

<details>
<summary><strong>7. How can you inspect the most recent deadlock in MySQL InnoDB?</strong></summary>

**Answer:**
By running the diagnostic command:
```sql
SHOW ENGINE INNODB STATUS;
```
Under the section header `------------------------ LATEST DETECTED DEADLOCK ------------------------`, InnoDB outputs the exact SQL statements, transaction IDs, held locks, requested locks, and indicates which transaction was chosen as the victim and rolled back.
</details>

<details>
<summary><strong>8. What is the "Hold and Wait" condition, and how can an application eliminate it?</strong></summary>

**Answer:**
"Hold and Wait" occurs when a transaction holds locks on some resources while actively waiting to acquire locks on additional resources.

An application can eliminate Hold and Wait by **acquiring all necessary locks upfront in a single atomic operation** at the beginning of the transaction. For example, instead of locking rows one by one throughout a 5-second business flow, the transaction issues a single `SELECT ... WHERE id IN (...) FOR UPDATE` at the very start. If all locks cannot be acquired simultaneously, it does not hold partial locks.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1: Reproducing a Two-Session Deadlock in PostgreSQL</strong></summary>

**Problem:** Using table `accounts (id INT PRIMARY KEY, balance NUMERIC)`, write the exact sequence of SQL commands across two psql sessions that triggers a `40P01` deadlock error.

**Solution:**
```sql
-- Setup:
CREATE TABLE accounts (id INT PRIMARY KEY, balance NUMERIC);
INSERT INTO accounts VALUES (1, 100), (2, 200);

-- Terminal 1:
BEGIN;
UPDATE accounts SET balance = balance + 10 WHERE id = 1;

-- Terminal 2:
BEGIN;
UPDATE accounts SET balance = balance + 20 WHERE id = 2;

-- Terminal 1:
UPDATE accounts SET balance = balance + 10 WHERE id = 2; -- Hangs waiting for Terminal 2

-- Terminal 2:
UPDATE accounts SET balance = balance + 20 WHERE id = 1; -- Triggers deadlock!
-- Terminal 2 (or 1) aborts with: ERROR: deadlock detected (40P01)
```
</details>

<details>
<summary><strong>Exercise 2: Implementing Deterministic Lock Ordering</strong></summary>

**Problem:** Rewrite the transfer procedure so that transferring from Account $X$ to Account $Y$ is completely deadlock-free regardless of which direction funds move.

**Solution:**
```sql
CREATE OR REPLACE PROCEDURE transfer_funds(from_id INT, to_id INT, amount NUMERIC)
LANGUAGE plpgsql AS $$
DECLARE
    first_id INT := LEAST(from_id, to_id);
    second_id INT := GREATEST(from_id, to_id);
BEGIN
    -- Deterministic locking order: lowest ID first
    PERFORM balance FROM accounts WHERE id = first_id FOR UPDATE;
    PERFORM balance FROM accounts WHERE id = second_id FOR UPDATE;

    -- Execute transfer
    UPDATE accounts SET balance = balance - amount WHERE id = from_id;
    UPDATE accounts SET balance = balance + amount WHERE id = to_id;
END;
$$;
```
</details>

<details>
<summary><strong>Exercise 3: Tracking Deadlock Metrics via Prometheus / SQL</strong></summary>

**Problem:** Write a query that computes the total number of deadlocks encountered by the current database since server statistics were last reset.

**Solution:**
```sql
SELECT 
    datname,
    deadlocks AS total_deadlocks,
    stats_reset
FROM pg_stat_database 
WHERE datname = current_database();
```
</details>

<details>
<summary><strong>Exercise 4: Setting Fine-Grained Deadlock Timeouts</strong></summary>

**Problem:** For a batch processing session that frequently experiences contention, set `deadlock_timeout` to 250 milliseconds so deadlocks are resolved quickly, and set `lock_timeout` to 5 seconds to prevent indefinite waits.

**Solution:**
```sql
SET deadlock_timeout = '250ms';
SET lock_timeout = '5s';
```
</details>

<details>
<summary><strong>Exercise 5: Automated Retrying of Serialization and Deadlock Failures</strong></summary>

**Problem:** Write a PL/pgSQL block that catches both deadlock (`40P01`) and serialization failure (`40001`) errors and retries the operation up to 3 times.

**Solution:**
```sql
DO $$
DECLARE
    retry_count INT := 0;
    success BOOLEAN := FALSE;
BEGIN
    WHILE NOT success AND retry_count < 3 LOOP
        BEGIN
            -- Transactional logic here:
            UPDATE accounts SET balance = balance - 10 WHERE id = 1;
            UPDATE accounts SET balance = balance + 10 WHERE id = 2;
            success := TRUE;
        EXCEPTION
            WHEN deadlock_detected OR serialization_failure THEN
                retry_count := retry_count + 1;
                IF retry_count >= 3 THEN
                    RAISE EXCEPTION 'Failed after 3 retries due to concurrency conflicts';
                END IF;
                PERFORM pg_sleep(0.05 * retry_count);
        END;
    END LOOP;
END $$;
```
</details>

<details>
<summary><strong>Exercise 6: Auditing Foreign Keys Missing B-Tree Indexes</strong></summary>

**Problem:** Write a query against PostgreSQL's system catalogs (`pg_constraint`, `pg_index`) to list all foreign key constraints that do not have a matching index on the referencing columns.

**Solution:**
```sql
SELECT 
    conrelid::regclass AS child_table,
    conname AS foreign_key_name,
    pg_get_constraintdef(c.oid) AS constraint_definition
FROM pg_constraint c
WHERE contype = 'f'
  AND NOT EXISTS (
      SELECT 1 
      FROM pg_index i 
      WHERE i.indrelid = c.conrelid 
        AND conkey::int[] = (string_to_array(indkey::text, ' '))[1:array_length(conkey, 1)]::int[]
  );
```
</details>

---

## Summary & Further Reading

- Deadlocks occur when two or more transactions form a circular dependency in the Wait-For Graph (WFG).
- The engine's Deadlock Detector sleeps for `deadlock_timeout` before initiating cycle detection and aborting a victim.
- Enforcing **deterministic lock ordering** (e.g., sorting primary keys before locking or updating) mathematically eliminates circular wait conditions.
- Always index foreign key columns to prevent cascading table lock conflicts.
- **Next Topic**: [07-optimistic-vs-pessimistic-concurrency.md](07-optimistic-vs-pessimistic-concurrency.md) compares Optimistic Concurrency Control (OCC) with Pessimistic Locking patterns in production architectures.
