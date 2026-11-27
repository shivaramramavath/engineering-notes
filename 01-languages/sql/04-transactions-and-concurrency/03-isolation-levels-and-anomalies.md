# Transaction Isolation Levels & Anomalies

## Metadata
| Attribute | Details |
|:---|:---|
| **Topic** | Isolation Levels, Concurrency Anomalies & ANSI SQL:1992 Critique |
| **Dialect** | Standard ANSI SQL / PostgreSQL 17+ Baseline |
| **Database Version** | PostgreSQL 17+, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Tested with** | PostgreSQL 17.2, SQLite 3.50.4 |
| **Prerequisites** | `04-transactions-and-concurrency/01-acid.md`, `04-transactions-and-concurrency/02-transaction-control.md` |
| **Complexity** | Advanced |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [The Concurrency Phenomena (Anomalies)](#the-concurrency-phenomena-anomalies)
4. [How It Works Internally](#how-it-works-internally)
5. [Syntax & Usage](#syntax--usage)
6. [Cross-Dialect Differences](#cross-dialect-differences)
7. [Practical Examples](#practical-examples)
8. [Best Practices](#best-practices)
9. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
10. [Performance & Security Notes](#performance--security-notes)
11. [Comparison Matrix: ANSI SQL vs Engine Reality](#comparison-matrix-ansi-sql-vs-engine-reality)
12. [Real-World Interview Questions](#real-world-interview-questions)
13. [Practice Exercises](#practice-exercises)
14. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

In an ideal theoretical database, all transactions would execute with **Serializability**: the concurrent interleaving of operations from hundreds of clients would produce the exact same outcome as if they had executed one after another in strict serial sequence.

However, strict serializability historically required locking large swathes of tables and index key ranges (Two-Phase Locking, 2PL), drastically choking database concurrency and throughput. To balance performance with correctness, the ANSI SQL:1992 standard defined **four isolation levels**, categorized by which anomalous data phenomena they permit or prohibit:
1. `READ UNCOMMITTED`
2. `READ COMMITTED`
3. `REPEATABLE READ`
4. `SERIALIZABLE`

Yet, as proven by Berenson et al. in their seminal 1995 paper *"A Critique of ANSI SQL Isolation Levels"*, the ANSI definitions were deeply flawed: they were framed around lock-based database engines and missed critical anomalies like **Write Skew** and **Lost Updates** that plague modern multi-version concurrency control (MVCC) engines. Mastering how isolation levels operate under the hood is critical for avoiding subtle, multi-million-dollar data corruption bugs in production systems.

---

## Core Concepts

### The 4 ANSI SQL Isolation Levels

```
Isolation Level      | Dirty Read | Non-Repeatable Read | Phantom Read
---------------------+------------+---------------------+--------------
READ UNCOMMITTED     | ALLOWED    | ALLOWED             | ALLOWED
READ COMMITTED       | Prevented  | ALLOWED             | ALLOWED
REPEATABLE READ      | Prevented  | Prevented           | ALLOWED
SERIALIZABLE         | Prevented  | Prevented           | Prevented
```

1. **`READ UNCOMMITTED`**: Transactions can read rows that have been modified by another active transaction but not yet committed ("dirty data"). If the modifying transaction rolls back, the reading transaction has acted on data that never officially existed.
2. **`READ COMMITTED`**: Guarantees that any row read was committed at the instant it was read. However, if Transaction A reads a row, and Transaction B updates that row and commits, Transaction A re-reading the exact same row will observe the new mutated value.
3. **`REPEATABLE READ`**: Guarantees that any row read by Transaction A will return the exact same values if re-read during the transaction. In traditional locking engines, phantom rows (new rows inserted by another transaction that match the search condition) could still appear. In MVCC engines like PostgreSQL, Repeatable Read also prevents phantoms!
4. **`SERIALIZABLE`**: The strictest level. Guarantees that concurrent transactions execute with no observable anomalies whatsoever, exactly as if executed serially.

---

## The Concurrency Phenomena (Anomalies)

To understand isolation, one must analyze the exact interleaving of read ($R$) and write ($W$) operations across concurrent transactions $T_1$ and $T_2$:

### 1. Dirty Read ($P_1$)
$T_1$ modifies a row. $T_2$ reads that row *before* $T_1$ commits. $T_1$ then issues `ROLLBACK`. $T_2$ has now operated on an invalid, non-existent state.

```
T1: BEGIN; UPDATE users SET balance = 500 WHERE id = 1;
T2: BEGIN; SELECT balance FROM users WHERE id = 1; -- Sees 500!
T1: ROLLBACK;
T2: -- T2 now acts on 500, but real balance is still 100!
```

### 2. Non-Repeatable (Fuzzy) Read ($P_2$)
$T_1$ reads a row. $T_2$ modifies or deletes that row and issues `COMMIT`. $T_1$ re-reads the row and observes a different value (or finds the row missing).

```
T1: BEGIN; SELECT balance FROM accounts WHERE id = 1; -- Returns 100
T2: BEGIN; UPDATE accounts SET balance = 250 WHERE id = 1; COMMIT;
T1: SELECT balance FROM accounts WHERE id = 1;        -- Returns 250! (Values changed mid-tx)
```

### 3. Phantom Read ($P_3$)
$T_1$ reads a set of rows satisfying a search predicate (e.g., `WHERE department = 'HR'`). $T_2$ inserts a new row satisfying the predicate and commits. $T_1$ repeats the query with the same predicate and discovers new "phantom" rows that were not previously present.

```
T1: BEGIN; SELECT count(*) FROM employees WHERE dept = 'HR'; -- Returns 5
T2: BEGIN; INSERT INTO employees (id, dept) VALUES (99, 'HR'); COMMIT;
T1: SELECT count(*) FROM employees WHERE dept = 'HR';        -- Returns 6! (Phantom appeared)
```

### 4. Lost Update ($P_4$)
$T_1$ and $T_2$ both read the current state of a row. $T_1$ updates the row based on the read value and commits. $T_2$ updates the same row based on its original read value and commits, completely overwriting and obliterating $T_1$'s update without knowing it.

### 5. Write Skew ($A5B$) — The Snapshot Isolation Flaw
Two concurrent transactions read overlapping datasets, verify a global invariant, make disjoint mutations, and both commit. Neither transaction overwrites the same row (so no write lock conflict occurs), but their combined execution violates the global business invariant!

**The Classic "Doctors on Call" Anomaly**:
- Invariant: *At least one doctor must remain on call at all times.*
- Initial State: Dr. Alice and Dr. Bob are both on call.
- $T_1$ (Dr. Alice wants to go off call): Reads table, counts 2 active doctors. Satisfies rule ($2 - 1 \ge 1$). Sets Alice = `FALSE`.
- $T_2$ (Dr. Bob wants to go off call concurrently): Reads table, counts 2 active doctors. Satisfies rule ($2 - 1 \ge 1$). Sets Bob = `FALSE`.
- Both $T_1$ and $T_2$ commit! Result: **Zero doctors on call**, violating the invariant!

```
Initial State: Alice (OnCall=TRUE), Bob (OnCall=TRUE)
T1 (Alice): SELECT COUNT(*) WHERE OnCall=TRUE; -> Returns 2 (Rule: Count >= 1)
T2 (Bob):   SELECT COUNT(*) WHERE OnCall=TRUE; -> Returns 2 (Rule: Count >= 1)
T1 (Alice): UPDATE Doctors SET OnCall=FALSE WHERE Name='Alice'; COMMIT;
T2 (Bob):   UPDATE Doctors SET OnCall=FALSE WHERE Name='Bob'; COMMIT;
Final State: Alice=FALSE, Bob=FALSE! (CRITICAL INVARIANT VIOLATION)
```

---

## How It Works Internally

### MVCC Snapshot Isolation vs Traditional 2PL
How do modern databases implement these isolation guarantees without locking the entire table?

```
+-----------------------------------------------------------------------------------------+
|                                SNAPSHOT ISOLATION MECHANICS                             |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
| 1. READ COMMITTED:                                                                      |
|    - Each STATEMENT acquires a brand NEW snapshot of committed transactions at the      |
|      exact millisecond that individual statement starts executing.                      |
|    - Statement 1 sees DB state at 10:00:00.000.                                         |
|    - Statement 2 sees DB state at 10:00:00.050 (including commits between S1 and S2).   |
|                                                                                         |
| 2. REPEATABLE READ:                                                                     |
|    - The TRANSACTION acquires a SINGLE snapshot at the start of the first statement.    |
|    - Every statement in the transaction sees the database frozen in time at that        |
|      exact initial snapshot timestamp!                                                  |
|    - No non-repeatable reads; no phantom rows visible.                                 |
|                                                                                         |
| 3. SERIALIZABLE SNAPSHOT ISOLATION (SSI) (PostgreSQL):                                  |
|    - Uses SI snapshots (no read locks!).                                               |
|    - Tracks SIREAD locks (Predicate Locks) in memory to detect dependency cycles.       |
|    - If a dependency cycle (rw-antidependencies) forms (e.g. Write Skew), one tx is     |
|      aborted with: "ERROR: could not serialize access due to read/write dependencies".  |
+-----------------------------------------------------------------------------------------+
```

---

## Syntax & Usage

```sql
-- Setting isolation level for a single transaction
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

SELECT balance FROM customer_wallets WHERE customer_id = 42;

COMMIT;

-- Setting isolation level for the entire session
SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL SERIALIZABLE;

-- Querying current isolation level in PostgreSQL:
SHOW transaction_isolation;
```

---

## Cross-Dialect Differences

```
+----------------------------------------------------------------------------------------------------+
| DIALECT COMPARISON: ISOLATION LEVELS & ANOMALY HANDLING                                            |
+-------------------+--------------------------------------------------------------------------------+
| PostgreSQL 17+    | Default: READ COMMITTED. READ UNCOMMITTED is silently mapped to READ COMMITTED |
|                   | (Postgres NEVER allows dirty reads!). REPEATABLE READ prevents phantoms!       |
|                   | SERIALIZABLE uses SSI (no locks, aborts with serialization error on conflict). |
+-------------------+--------------------------------------------------------------------------------+
| MySQL 8.4 (InnoDB)| Default: REPEATABLE READ! READ UNCOMMITTED permits true dirty reads.           |
|                   | REPEATABLE READ prevents phantoms for plain SELECTs (MVCC snapshot), but      |
|                   | locking SELECTs (FOR UPDATE) use Next-Key (Gap) locks to block phantom inserts.|
|                   | SERIALIZABLE implicitly converts all plain SELECTs to SELECT ... FOR SHARE!    |
+-------------------+--------------------------------------------------------------------------------+
| SQLite 3.50+      | In WAL mode, isolation is essentially SERIALIZABLE for writes (single writer)  |
|                   | and SNAPSHOT isolation for readers. Does not support READ UNCOMMITTED in WAL.  |
+-------------------+--------------------------------------------------------------------------------+
| SQL Server 2022   | Default: READ COMMITTED (uses shared locks that block writers by default!).    |
|                   | Must explicitly enable Read Committed Snapshot Isolation (RCSI) or SNAPSHOT    |
|                   | isolation at database level to get non-blocking MVCC behavior.                 |
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Demonstrating Non-Repeatable Read in Read Committed
```sql
-- Session 1 (T1)
BEGIN TRANSACTION ISOLATION LEVEL READ COMMITTED;
SELECT tier FROM loyalty_members WHERE member_id = 100;
-- Output: 'BRONZE'

-- Session 2 (T2) concurrently modifies and commits:
-- BEGIN; UPDATE loyalty_members SET tier = 'GOLD' WHERE member_id = 100; COMMIT;

-- Session 1 (T1) re-reads:
SELECT tier FROM loyalty_members WHERE member_id = 100;
-- Output: 'GOLD' (Non-repeatable read observed!)
COMMIT;
```

### Example 2: Eliminating Non-Repeatable Read with Repeatable Read
```sql
-- Session 1 (T1)
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
SELECT tier FROM loyalty_members WHERE member_id = 100;
-- Output: 'BRONZE'

-- Session 2 (T2) concurrently modifies and commits:
-- BEGIN; UPDATE loyalty_members SET tier = 'GOLD' WHERE member_id = 100; COMMIT;

-- Session 1 (T1) re-reads:
SELECT tier FROM loyalty_members WHERE member_id = 100;
-- Output: 'BRONZE' (Snapshot preserved; consistent view maintained!)
COMMIT;
```

### Example 3: Write Skew Reproducer (The Doctors on Call Bug)
```sql
-- Setup
CREATE TABLE on_call_doctors (
    doctor_id INT PRIMARY KEY,
    doctor_name VARCHAR(64),
    is_on_call BOOLEAN NOT NULL
);
INSERT INTO on_call_doctors VALUES (1, 'Alice', TRUE), (2, 'Bob', TRUE);

-- Session 1 (Alice requests off-call)
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
SELECT COUNT(*) FROM on_call_doctors WHERE is_on_call = TRUE; -- Returns 2 (Valid: > 1)
UPDATE on_call_doctors SET is_on_call = FALSE WHERE doctor_id = 1;

-- Session 2 (Bob concurrently requests off-call)
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
SELECT COUNT(*) FROM on_call_doctors WHERE is_on_call = TRUE; -- Returns 2 (Valid: > 1)
UPDATE on_call_doctors SET is_on_call = FALSE WHERE doctor_id = 2;

-- Both commit!
-- Session 1:
COMMIT;
-- Session 2:
COMMIT;

-- Verification:
SELECT * FROM on_call_doctors WHERE is_on_call = TRUE;
-- 0 rows returned! The hospital has NO doctors on call. Invariant broken under Repeatable Read!
```

### Example 4: Preventing Write Skew with SERIALIZABLE
Repeating the exact scenario under PostgreSQL's Serializable Snapshot Isolation (SSI):
```sql
-- Session 1 (Alice)
BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
SELECT COUNT(*) FROM on_call_doctors WHERE is_on_call = TRUE;
UPDATE on_call_doctors SET is_on_call = FALSE WHERE doctor_id = 1;

-- Session 2 (Bob)
BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
SELECT COUNT(*) FROM on_call_doctors WHERE is_on_call = TRUE;
UPDATE on_call_doctors SET is_on_call = FALSE WHERE doctor_id = 2;

-- Session 1 commits:
COMMIT; -- Succeeded!

-- Session 2 commits:
COMMIT;
-- ERROR: could not serialize access due to read/write dependencies among transactions
-- DETAIL: Reason code: Canceled on identification as a pivot, during commit attempt.
-- HINT: The transaction might succeed if retried.
```

### Example 5: Preventing Lost Update via Explicit Locking or Atomic DML
```sql
-- Anti-Pattern (Vulnerable to Lost Update in Read Committed):
-- T1 reads stock = 10
-- T2 reads stock = 10
-- T1 writes stock = 10 - 2 = 8
-- T2 writes stock = 10 - 3 = 7 (T1's deduction is lost!)

-- Pattern A: Atomic Update (Pushing calculation to the engine)
UPDATE products 
SET stock = stock - 2 
WHERE product_id = 500 AND stock >= 2;

-- Pattern B: Explicit Row Locking (Pessimistic)
BEGIN;
SELECT stock 
FROM products 
WHERE product_id = 500 
FOR UPDATE; -- Acquires exclusive row lock; T2 waits until T1 commits

UPDATE products SET stock = stock - 2 WHERE product_id = 500;
COMMIT;
```

---

## Best Practices

1. **Default to READ COMMITTED for Standard CRUD**: For 90% of web application workloads, `READ COMMITTED` provides optimal balance of throughput and safety, provided write conflicts are handled via atomic SQL updates or `SELECT FOR UPDATE`.
2. **Use SERIALIZABLE for Financial Audits and Cross-Row Invariants**: Whenever a transaction verifies an aggregate rule across multiple rows before inserting or updating (such as credit limit vs total pending invoices), run in `SERIALIZABLE` or lock explicitly.
3. **Always Build Application Retry Loops for SERIALIZABLE**: When using `SERIALIZABLE`, the database engine will intentionally abort transactions with SQLSTATE `40001` (`serialization_failure`). Your application **must** be written to catch this error and automatically retry the transaction with exponential backoff.
4. **Beware of the Repeatable Read Snapshot Illusion**: Remember that in PostgreSQL, an `UPDATE` in `REPEATABLE READ` that targets a row already modified and committed by another transaction will immediately abort with: `ERROR: could not serialize access due to concurrent update`.
5. **Never Use READ UNCOMMITTED**: There is almost no valid use case in modern online transaction processing (OLTP) for reading uncommitted dirty data. In PostgreSQL, `READ UNCOMMITTED` does not even exist (it operates as `READ COMMITTED`).

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Inventory balance overwrites / drift** | Fetching balance via plain `SELECT`, calculating in application code, and running `UPDATE ... SET balance = :val`. | Use atomic updates (`SET balance = balance - :qty`) or `SELECT FOR UPDATE`. |
| **Silent invariant violations (e.g., Doctors on Call)** | Assuming `REPEATABLE READ` prevents all anomalies because it eliminates non-repeatable reads and phantoms. | Use `SERIALIZABLE` or explicit mutual exclusion locks (`pg_advisory_lock` / table locks). |
| **`serialization_failure` crashing web requests** | Switching to `SERIALIZABLE` isolation without implementing retry middleware in the application. | Catch `40001` serialization failures and retry the entire transaction block. |
| **Massive read-blocking in SQL Server** | Running default SQL Server configuration where plain `SELECT` acquires shared locks (`S`), blocking all concurrent `UPDATE` queries. | Enable `READ_COMMITTED_SNAPSHOT` (RCSI) on the database. |
| **MySQL Next-Key Lock unexpected lockups** | In MySQL `REPEATABLE READ`, `SELECT ... FOR UPDATE` locks entire index key gaps, blocking inserts from unrelated tenants. | Ensure queries filter on unique indexed columns, or use PostgreSQL which handles row locks without gap locking. |
| **Stale data across loop iterations** | Opening a single `REPEATABLE READ` transaction and running queries inside a loop expecting to observe changes made by other background workers. | Commit the transaction between loop iterations to refresh the read snapshot. |

---

## Performance & Security Notes

### The Cost of Serializable Snapshot Isolation (SSI)
PostgreSQL's SSI implementation is brilliant because readers never block writers, and writers never block readers. However, SSI maintains in-memory **SIREAD locks** (predicate locks) in shared memory (`max_pred_locks_per_transaction`):
- If memory runs low, the lock manager escalates individual tuple locks to page-level locks, increasing false-positive conflict aborts.
- High-contention workloads on hot rows will experience high abort rates, wasting CPU cycles on discarded work.

### Security Implications of Isolation Leaks
Under `READ UNCOMMITTED`, malicious or buggy code could manipulate sensitive flags (e.g., `is_admin = TRUE`), allow another transaction to read the elevated permission, and then immediately issue `ROLLBACK`. The system acts upon the uncommitted administrative grant, resulting in privilege escalation.

---

## Comparison Matrix: ANSI SQL vs Engine Reality

| Isolation Level | Dirty Read ($P_1$) | Non-Repeatable Read ($P_2$) | Phantom Read ($P_3$) | Write Skew ($A5B$) | Lost Update ($P_4$) |
|:---|:---:|:---:|:---:|:---:|:---:|
| **ANSI Read Uncommitted** | Permitted | Permitted | Permitted | Permitted | Permitted |
| **ANSI Read Committed** | Prevented | Permitted | Permitted | Permitted | Permitted |
| **ANSI Repeatable Read** | Prevented | Prevented | Permitted | Permitted | Permitted |
| **PostgreSQL Repeatable Read (Snapshot)** | Prevented | Prevented | **Prevented** | Permitted | Prevented* |
| **MySQL Repeatable Read (InnoDB)** | Prevented | Prevented | **Prevented** (Snapshots) | Permitted | Prevented* |
| **True Serializable (SSI / 2PL)** | Prevented | Prevented | Prevented | **Prevented** | **Prevented** |

*\*Prevented by raising a serialization update conflict error if both update the exact same row.*

---

## Real-World Interview Questions

<details>
<summary><strong>1. Why is Write Skew possible in Repeatable Read (Snapshot Isolation) but impossible in Serializable?</strong></summary>

**Answer:**
Under Snapshot Isolation (Repeatable Read in Postgres/MySQL), each transaction reads a private, immutable snapshot of the database taken at the start of the transaction. Two concurrent transactions can read the exact same data, evaluate a shared business constraint (e.g., "sum of balances must be $\ge 0$"), and make disjoint updates (Transaction 1 updates Account A, Transaction 2 updates Account B).

Because each transaction updates a **different row**, neither transaction triggers a row-level write-write lock conflict. Both commit cleanly. However, together, their combined writes violate the invariant. 

Serializable isolation (e.g., SSI in PostgreSQL) tracks read-write antidependencies (SIREAD locks) and detects that Transaction 1 read what Transaction 2 wrote, and Transaction 2 read what Transaction 1 wrote, identifying a dangerous cycle in the dependency graph and deliberately aborting one of the transactions.
</details>

<details>
<summary><strong>2. Does PostgreSQL support READ UNCOMMITTED? What happens if you run SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED?</strong></summary>

**Answer:**
PostgreSQL accepts the syntax `SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED`, but it **silently maps it to READ COMMITTED**. 

PostgreSQL's storage engine relies on MVCC heap row headers (`xmin`/`xmax`). A tuple is never considered visible to any transaction until the transaction that created it has recorded its commit status in the commit log (`pg_xact`). Because PostgreSQL's core visibility routines cannot physically read uncommitted heap tuples without severe engine redesign, dirty reads are impossible in PostgreSQL under any configuration.
</details>

<details>
<summary><strong>3. What is the difference between a Non-Repeatable Read and a Phantom Read?</strong></summary>

**Answer:**
- **Non-Repeatable Read (Fuzzy Read)** occurs when a transaction reads a **specific existing row** by ID or primary key, and a concurrent transaction updates or deletes that specific row and commits. If the first transaction re-reads the same row, the column values have changed or the row has disappeared.
- **Phantom Read** occurs when a transaction executes a **range or predicate query** (e.g., `WHERE age > 30` or `WHERE status = 'ACTIVE'`), and a concurrent transaction inserts a **brand new row** that satisfies that predicate and commits. When the first transaction repeats the range query, new "phantom" rows appear in the result set.
</details>

<details>
<summary><strong>4. How does MySQL InnoDB prevent Phantom Reads in REPEATABLE READ?</strong></summary>

**Answer:**
MySQL InnoDB uses a dual mechanism:
1. For **plain `SELECT` (consistent non-locking reads)**: InnoDB uses MVCC snapshots (Read Views). Any row inserted after the snapshot was initialized is simply filtered out by transaction ID visibility rules, preventing phantom rows from being seen.
2. For **locking reads (`SELECT ... FOR UPDATE`, `UPDATE`, `DELETE`)**: InnoDB uses **Next-Key Locking**, which combines an index record lock with a **Gap Lock** on the gap preceding the index record. By locking the gap between index keys, InnoDB physically blocks concurrent transactions from inserting new rows into that range until the locking transaction commits.
</details>

<details>
<summary><strong>5. What is the SQLSTATE error code for a serialization failure in PostgreSQL, and how should applications handle it?</strong></summary>

**Answer:**
The standard SQLSTATE error code is **`40001`** (`serialization_failure`). 

In applications using `SERIALIZABLE` isolation, serialization failures are not application bugs; they are a normal, expected consequence of optimistic concurrency conflict resolution. The application must wrap all serializable transactions in retry middleware (e.g., using exponential backoff with random jitter) that catches error `40001`, re-executes the transaction from the `BEGIN` statement, and attempts execution up to a configured retry limit (e.g., 3-5 attempts).
</details>

<details>
<summary><strong>6. Why did Berenson et al. criticize the ANSI SQL:1992 isolation definitions in their 1995 paper?</strong></summary>

**Answer:**
Berenson et al. demonstrated that the ANSI SQL:1992 standard had fundamental shortcomings:
1. **Lock-Centric Bias**: The definitions assumed that isolation would be implemented via Two-Phase Locking (2PL), failing to account for Multi-Version Concurrency Control (MVCC) engines that were emerging.
2. **Missing Anomalies**: The standard only defined three phenomena (Dirty Read, Non-Repeatable Read, Phantom Read). It completely missed critical anomalies that occur in modern systems, specifically **Lost Update**, **Read Skew**, and **Write Skew**.
3. **Under-specification of Serializable**: The standard implied that eliminating the three phenomena was sufficient to guarantee serializability. Berenson et al. proved that Snapshot Isolation prevents all three ANSI phenomena, yet still suffers from Write Skew and is therefore *not* truly serializable.
</details>

<details>
<summary><strong>7. How does SQL Server's default locking behavior differ from PostgreSQL and MySQL?</strong></summary>

**Answer:**
In SQL Server's default `READ COMMITTED` mode, concurrency is managed using traditional **pessimistic shared locks (`S`)** rather than MVCC:
- When a `SELECT` runs, it acquires shared locks on data pages or rows, which **blocks concurrent `UPDATE` transactions** from modifying those rows.
- Conversely, an `UPDATE` holds exclusive locks (`X`), which **blocks all concurrent `SELECT` readers**.
- To achieve non-blocking reads like PostgreSQL and MySQL, a SQL Server database administrator must explicitly alter the database to enable `READ_COMMITTED_SNAPSHOT` (RCSI), which offloads row versions to `tempdb`.
</details>

<details>
<summary><strong>8. What is Read Skew ($A5A$) and give a concrete financial example.</strong></summary>

**Answer:**
**Read Skew** occurs when Transaction $T_1$ reads row $x$, then concurrent Transaction $T_2$ updates row $x$ and row $y$ and commits, and then $T_1$ reads row $y$. $T_1$ sees the pre-update state of $x$ and the post-update state of $y$, observing an inconsistent, skewed state that never existed simultaneously.

**Example**: Checking account has \$500, Savings has \$500 (Total = \$1,000).
- $T_1$ reads Checking: \$500.
- $T_2$ transfers \$200 from Checking to Savings: updates Checking to \$300, Savings to \$700, and commits.
- $T_1$ reads Savings: \$700.
- $T_1$ calculates total balance as \$500 + \$700 = \$1,200! \$200 of fictitious money appeared due to reading across an un-isolated commit boundary. (Prevented by `REPEATABLE READ`).
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1: Inspecting Database Isolation Level</strong></summary>

**Problem:** Write SQL queries to check the current transaction isolation level in PostgreSQL, MySQL, and SQL Server.

**Solution:**
```sql
-- PostgreSQL:
SHOW transaction_isolation;

-- MySQL:
SELECT @@transaction_isolation;

-- SQL Server:
SELECT CASE transaction_isolation_level 
    WHEN 1 THEN 'ReadUncommitted' 
    WHEN 2 THEN 'ReadCommitted' 
    WHEN 3 THEN 'RepeatableRead' 
    WHEN 4 THEN 'Serializable' 
    WHEN 5 THEN 'Snapshot' 
END AS isolation_level
FROM sys.dm_exec_sessions 
WHERE session_id = @@SPID;
```
</details>

<details>
<summary><strong>Exercise 2: Simulating and Mitigating Lost Update</strong></summary>

**Problem:** Table `inventory (item_id INT PRIMARY KEY, quantity INT)`. Write an atomic single-statement query to reduce quantity by 5 without incurring a lost update, even under concurrent `READ COMMITTED` transactions.

**Solution:**
```sql
UPDATE inventory
SET quantity = quantity - 5
WHERE item_id = 42
  AND quantity >= 5;
```
*(By combining calculation and constraint check inside the `UPDATE`, the engine evaluates the expression using the latest committed row version under the row's exclusive write lock, eliminating lost updates).*
</details>

<details>
<summary><strong>Exercise 3: Preventing Write Skew in Application Design</strong></summary>

**Problem:** Given the Doctors-on-Call problem, provide an explicit locking solution in `READ COMMITTED` mode that prevents the write skew anomaly without switching the whole database to `SERIALIZABLE`.

**Solution:**
```sql
BEGIN;

-- Lock all doctors on call exclusively
SELECT * 
FROM on_call_doctors 
WHERE is_on_call = TRUE 
FOR UPDATE;

-- Evaluate condition safely: count will not change concurrently
-- Application checks: IF count > 1 THEN:
UPDATE on_call_doctors 
SET is_on_call = FALSE 
WHERE doctor_id = 1;

COMMIT;
```
</details>

<details>
<summary><strong>Exercise 4: Identifying Concurrency Conflicts in PostgreSQL Logs</strong></summary>

**Problem:** Write a PostgreSQL query using system catalogs to identify if any transactions are currently waiting on locks held by another transaction, showing the blocked PID and the blocking PID.

**Solution:**
```sql
SELECT 
    blocked_locks.pid     AS blocked_pid,
    blocked_activity.usename  AS blocked_user,
    blocking_locks.pid    AS blocking_pid,
    blocking_activity.usename AS blocking_user,
    blocked_activity.query    AS blocked_statement,
    blocking_activity.query   AS blocking_statement
FROM  pg_catalog.pg_locks         blocked_locks
JOIN pg_catalog.pg_stat_activity blocked_activity ON blocked_activity.pid = blocked_locks.pid
JOIN pg_catalog.pg_locks         blocking_locks 
    ON blocking_locks.locktype = blocked_locks.locktype
    AND blocking_locks.database IS NOT DISTINCT FROM blocked_locks.database
    AND blocking_locks.relation IS NOT DISTINCT FROM blocked_locks.relation
    AND blocking_locks.page IS NOT DISTINCT FROM blocked_locks.page
    AND blocking_locks.tuple IS NOT DISTINCT FROM blocked_locks.tuple
    AND blocking_locks.virtualxid IS NOT DISTINCT FROM blocked_locks.virtualxid
    AND blocking_locks.transactionid IS NOT DISTINCT FROM blocked_locks.transactionid
    AND blocking_locks.classid IS NOT DISTINCT FROM blocked_locks.classid
    AND blocking_locks.objid IS NOT DISTINCT FROM blocked_locks.objid
    AND blocking_locks.objsubid IS NOT DISTINCT FROM blocked_locks.objsubid
    AND blocking_locks.pid != blocked_locks.pid
JOIN pg_catalog.pg_stat_activity blocking_activity ON blocking_activity.pid = blocking_locks.pid
WHERE NOT blocked_locks.granted;
```
</details>

<details>
<summary><strong>Exercise 5: Building an Application Retry Loop (Pseudocode/SQL Logic)</strong></summary>

**Problem:** Write the standard application pseudocode pattern required to execute a transaction safely under `SERIALIZABLE` isolation.

**Solution:**
```python
MAX_RETRIES = 5
for attempt in range(MAX_RETRIES):
    try:
        cursor.execute("BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;")
        cursor.execute("SELECT balance FROM accounts WHERE id = 1;")
        # ... execute business logic ...
        cursor.execute("UPDATE accounts SET balance = balance - 100 WHERE id = 1;")
        cursor.execute("COMMIT;")
        break # Success!
    except DatabaseError as e:
        cursor.execute("ROLLBACK;")
        if e.pgcode == '40001': # serialization_failure
            sleep_with_backoff(attempt)
            continue
        else:
            raise e # Real error, do not retry
```
</details>

<details>
<summary><strong>Exercise 6: Testing Snapshot Isolation Visibility</strong></summary>

**Problem:** In SQLite or PostgreSQL, open transaction A in `REPEATABLE READ`. Insert a row in Transaction B and commit. Does Transaction A observe the new row? Explain why.

**Solution:**
No, Transaction A will not see the new row.
Under `REPEATABLE READ`, Transaction A establishes its active snapshot at the beginning of the transaction. Any row whose creation transaction ID (`xmin`) is higher than the snapshot's upper boundary (or committed after the snapshot timestamp) is considered invisible to all queries executed within Transaction A.
</details>

---

## Summary & Further Reading

- The ANSI SQL:1992 standard defined 4 isolation levels based on Dirty Read, Non-Repeatable Read, and Phantom Read.
- Modern MVCC engines like PostgreSQL implement `REPEATABLE READ` via snapshots, which prevents Phantom Reads out of the box, but still allows **Write Skew**.
- `SERIALIZABLE` is the only isolation level that mathematically eliminates all anomalies, including Write Skew and Read Skew.
- Applications using `SERIALIZABLE` must be designed with automatic retry logic to catch `40001` serialization failures.
- **Next Topic**: [04-mvcc-explained.md](04-mvcc-explained.md) explores the physical engine mechanics of Multi-Version Concurrency Control: row headers, `xmin`, `xmax`, undo logs, and snapshot data structures.
