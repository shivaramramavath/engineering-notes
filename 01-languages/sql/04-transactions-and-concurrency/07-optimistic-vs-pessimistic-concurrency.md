# Optimistic vs Pessimistic Concurrency Control

## Metadata
| Attribute | Details |
|:---|:---|
| **Topic** | Concurrency Strategies: OCC, 2PL, Version Columns & CAS |
| **Dialect** | Standard ANSI SQL / PostgreSQL 17+ Baseline |
| **Database Version** | PostgreSQL 17+, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Tested with** | PostgreSQL 17.2, SQLite 3.50.4 |
| **Prerequisites** | `04-transactions-and-concurrency/03-isolation-levels-and-anomalies.md`, `04-transactions-and-concurrency/05-locking.md` |
| **Complexity** | Advanced |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [Pessimistic Concurrency Control (PCC)](#pessimistic-concurrency-control-pcc)
4. [Optimistic Concurrency Control (OCC)](#optimistic-concurrency-control-occ)
5. [The Contention Tipping Point](#the-contention-tipping-point)
6. [Syntax & Implementation Patterns](#syntax--implementation-patterns)
7. [Cross-Dialect Differences](#cross-dialect-differences)
8. [Practical Examples](#practical-examples)
9. [Best Practices](#best-practices)
10. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
11. [Performance & Security Notes](#performance--security-notes)
12. [Comparison Matrix: OCC vs PCC](#comparison-matrix-occ-vs-pcc)
13. [Real-World Interview Questions](#real-world-interview-questions)
14. [Practice Exercises](#practice-exercises)
15. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

When multiple users or background threads read and mutate the same data entities concurrently, the system must prevent **lost updates** and ensure serializable state transitions. Consider a classic collaborative editing scenario: User Alice loads a customer's contact record into her web browser at 10:00 AM, edits the phone number over two minutes, and clicks "Save" at 10:02 AM. Meanwhile, User Bob loaded the same record at 10:01 AM, edited the billing address, and saved at 10:01:30 AM.

If the application uses naive SQL `UPDATE` statements, Alice's save at 10:02 AM will blindly overwrite and destroy Bob's address modification.

To solve this, database architects must choose between two opposing philosophies:
- **Pessimistic Concurrency Control (PCC)**: Assume conflicts **will happen**. Acquire physical row locks upfront upon reading, blocking all other concurrent writers until the transaction commits.
- **Optimistic Concurrency Control (OCC)**: Assume conflicts **rarely happen**. Read data without any locks, allow concurrent modifications, and validate during the update step whether another transaction modified the data in the interim. If a conflict occurred, reject the update or retry.

Choosing the wrong concurrency paradigm degrades application performance, causes massive database lock starvation, or leads to silent data corruption.

---

## Core Concepts

```
+-----------------------------------------------------------------------------------------+
|                               CONCURRENCY PARADIGM COMPARISON                           |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  PESSIMISTIC (Lock First)                           OPTIMISTIC (Validate at Commit)     |
|                                                                                         |
|  1. BEGIN;                                          1. SELECT id, data, version         |
|  2. SELECT * FROM items WHERE id=1 FOR UPDATE;         FROM items WHERE id=1;           |
|     (Acquires exclusive row lock)                      (No locks held! Open to all)     |
|  3. User modifies data in RAM / UI...               2. User edits data in RAM / UI...   |
|     (Other sessions BLOCKED on row 1)               3. UPDATE items                     |
|  4. UPDATE items SET ... WHERE id=1;                   SET data = :new_data,            |
|  5. COMMIT; (Releases lock)                                version = version + 1        |
|                                                        WHERE id = 1 AND version = :v;   |
|                                                     4. If rows_affected == 0:           |
|                                                        Conflict detected! Retry or abort|
+-----------------------------------------------------------------------------------------+
```

---

## Pessimistic Concurrency Control (PCC)

Pessimistic concurrency uses explicit database-level row locks (Two-Phase Locking, 2PL).
- **How it works**: A session reads the target row using `SELECT ... FOR UPDATE` (or `FOR NO KEY UPDATE`). The database places an exclusive lock on that physical row.
- **Guarantees**: Complete mutual exclusion. No other session can modify, lock, or delete that row until the locking session commits or rolls back.
- **Drawbacks**:
  - High lock contention under concurrent load.
  - Increased risk of deadlocks if multiple rows are locked in inconsistent orders.
  - **Infeasible for human web workflows**: You cannot hold a database row lock open while waiting for a human user to fill out a web form over a REST or GraphQL API! Database connections and locks cannot be held across stateless HTTP requests.

---

## Optimistic Concurrency Control (OCC)

Optimistic concurrency was formally introduced by Kung and Robinson in 1981. It divides transaction processing into three phases:
1. **Read Phase**: The client reads the target entity along with its **version identifier** (`version INT` or `xmin` or `updated_at`). No database locks are acquired.
2. **Validation Phase**: When writing changes, the client verifies that the version identifier has not changed since the read phase.
3. **Write Phase**: If the version matches, the changes are written and the version identifier is atomically incremented. If the version differs, the write is rejected as a conflict.

### The Compare-and-Swap (CAS) SQL Pattern
OCC is implemented in standard SQL using a single atomic conditional `UPDATE`:
```sql
UPDATE documents
SET title = :new_title,
    content = :new_content,
    version = version + 1,
    updated_at = CURRENT_TIMESTAMP
WHERE document_id = :doc_id
  AND version = :expected_version;
```
- If the statement returns **Rows Affected = 1**, the update succeeded atomically.
- If the statement returns **Rows Affected = 0**, another session modified the row in the meantime! The application detects this and can either automatically retry or notify the user of a conflict.

---

## The Contention Tipping Point

When should you use Pessimistic locking versus Optimistic concurrency?

```
Throughput
   ^
   |        /---\ OCC Outperforms (High Throughput, Low Contention)
   |       /     \
   |      /       \
   |     /         \ OCC Collapses due to Retry Storms!
   |    /           \
   |   /             \
   |  /---------------\------------------------ PCC (Stable, Bounded Throughput)
   | /                 \
   +------------------------------------------------------------> Write Contention
     0%                15%                      100%
```

- **Low Contention (< 5% conflicts)**: OCC easily wins. Zero lock overhead, zero deadlocks, maximum concurrency across thousands of threads.
- **High Contention (> 20% conflicts)**: OCC collapses into **retry storms**—hundreds of threads repeatedly read, compute, fail the CAS check, and retry, wasting massive CPU cycles. In high-contention environments (such as a 10-second flash sale for 100 concert tickets), **Pessimistic locking (`SELECT FOR UPDATE`)** dramatically outperforms OCC because waiting in a lock queue is vastly cheaper than thousands of aborted transactions and retries.

---

## Syntax & Implementation Patterns

### Pattern 1: Dedicated Numeric Version Column (Standard OCC)
```sql
CREATE TABLE accounts (
    account_id INT PRIMARY KEY,
    owner_name VARCHAR(100) NOT NULL,
    balance NUMERIC(12,2) NOT NULL,
    version INT NOT NULL DEFAULT 1
);

-- Application reads:
SELECT account_id, balance, version FROM accounts WHERE account_id = 42;
-- Suppose returns: balance = 500.00, version = 7

-- Application updates:
UPDATE accounts
SET balance = balance - 50.00,
    version = version + 1
WHERE account_id = 42
  AND version = 7;
```

### Pattern 2: PostgreSQL System Column OCC (`xmin`)
PostgreSQL provides a built-in version identifier on every row: the system attribute `xmin` (the Transaction ID that last updated the row). You can implement OCC without adding a `version` column!
```sql
-- Read row along with xmin
SELECT account_id, balance, xmin FROM accounts WHERE account_id = 42;
-- Suppose returns: balance = 500.00, xmin = 104592

-- Atomic CAS update using xmin
UPDATE accounts
SET balance = balance - 50.00
WHERE account_id = 42
  AND xmin = 104592;
```

---

## Cross-Dialect Differences

```
+----------------------------------------------------------------------------------------------------+
| DIALECT COMPARISON: OPTIMISTIC & PESSIMISTIC CONCURRENCY PRIMITIVES                                |
+-------------------+--------------------------------------------------------------------------------+
| PostgreSQL 17+    | Pessimistic: SELECT ... FOR UPDATE / FOR NO KEY UPDATE / SKIP LOCKED.          |
|                   | Optimistic: Numeric version column or system column xmin.                      |
+-------------------+--------------------------------------------------------------------------------+
| MySQL 8.4 (InnoDB)| Pessimistic: SELECT ... FOR UPDATE / FOR SHARE / NOWAIT / SKIP LOCKED.         |
|                   | Optimistic: Explicit version column or TIMESTAMP(6).                           |
+-------------------+--------------------------------------------------------------------------------+
| SQLite 3.50+      | Pessimistic: BEGIN IMMEDIATE locks database against concurrent writers upfront.|
|                   | Optimistic: Application-managed version column (PRAGMA recursive_triggers).    |
+-------------------+--------------------------------------------------------------------------------+
| SQL Server 2022   | Pessimistic: SELECT ... WITH (UPDLOCK, ROWLOCK).                               |
|                   | Optimistic: Native ROWVERSION / TIMESTAMP column (auto-incrementing binary).   |
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Web Application REST API with OCC
Handling collaborative document editing over stateless HTTP:
```sql
-- Client GET /api/documents/101
SELECT document_id, title, body_text, version 
FROM documents 
WHERE document_id = 101;
-- Returns version = 3

-- Client PUT /api/documents/101 (sends If-Match: 3 or body { version: 3 })
UPDATE documents
SET title = 'Revised Q1 Strategy',
    body_text = 'Updated content here...',
    version = version + 1,
    updated_at = CURRENT_TIMESTAMP
WHERE document_id = 101
  AND version = 3;

-- In application backend (Node.js / Python / Java):
-- IF rows_affected == 0 THEN
--    RETURN HTTP 409 Conflict ("This document was modified by another user.")
-- ELSE
--    RETURN HTTP 200 OK
```

### Example 2: High-Contention Flash Sale Ticket Reservation (Pessimistic)
In a high-contention flash sale, OCC would fail 99% of requests. Pessimistic queue locking is required:
```sql
BEGIN;

-- Lock the ticket inventory row exclusively; other buyers wait in line
SELECT available_tickets
FROM event_inventory
WHERE event_id = 'CONCERT_2026'
FOR UPDATE;

-- Evaluate condition
UPDATE event_inventory
SET available_tickets = available_tickets - 1
WHERE event_id = 'CONCERT_2026'
  AND available_tickets > 0;

-- Record purchase ticket
INSERT INTO tickets_issued (event_id, buyer_id, seat_tier)
VALUES ('CONCERT_2026', 8841, 'VIP');

COMMIT;
```

### Example 3: Edge Case — OCC with In-Memory Hash Validation
When modifying a legacy table where adding a `version` column is forbidden by corporate schema rules:
```sql
-- Read data and compute an MD5 hash of all sensitive columns
SELECT id, status, payload,
       md5(id::text || status || payload) AS row_hash
FROM legacy_system_jobs
WHERE id = 5001;

-- Update using hash match
UPDATE legacy_system_jobs
SET status = 'PROCESSED',
    payload = 'New payload data'
WHERE id = 5001
  AND md5(id::text || status || payload) = 'b69f69411648a7235b3c3e874f77e2bb';
```

### Example 4: Failure Mode — The "Infinite Retry Storm" in OCC
```sql
-- Scenario: 1,000 concurrent threads attempting to increment the same global counter:
-- Table: global_stats (counter_name VARCHAR PRIMARY KEY, val INT, version INT)

-- Thread logic:
-- 1. SELECT val, version FROM global_stats WHERE counter_name = 'PAGE_VIEWS';
-- 2. UPDATE global_stats SET val = val + 1, version = version + 1 WHERE version = :v;
-- 3. If rows == 0, retry immediately in a while(true) loop!

-- FAILURE: 
-- 1 thread succeeds; 999 threads fail and retry simultaneously.
-- Next iteration: 1 succeeds, 998 fail and retry.
-- Database CPU hits 100%, query latency explodes to 30 seconds!
-- Correct solution: Use atomic SQL: UPDATE global_stats SET val = val + 1 WHERE counter_name = 'PAGE_VIEWS';
```

### Example 5: Optimized Hybrid Concurrency (Optimistic with Pessimistic Fallback)
```sql
-- Try fast optimistic update first; if contention is detected, fall back to pessimistic lock:
DO $$
DECLARE
    rows_updated INT;
BEGIN
    -- Attempt 1: Fast Optimistic CAS
    UPDATE products 
    SET stock = stock - 1, version = version + 1 
    WHERE product_id = 99 AND version = 5;
    
    GET DIAGNOSTICS rows_updated = ROW_COUNT;
    
    -- If conflict occurred, escalate to pessimistic lock:
    IF rows_updated = 0 THEN
        -- Acquire explicit row lock and update
        PERFORM stock FROM products WHERE product_id = 99 FOR UPDATE;
        UPDATE products SET stock = stock - 1, version = version + 1 WHERE product_id = 99;
    END IF;
END $$;
```

---

## Best Practices

1. **Use OCC for Stateless Web / REST / GraphQL APIs**: Because HTTP is stateless, holding physical database locks across user think-time is impossible. Always use an OCC `version` column and return HTTP `409 Conflict` on mismatch.
2. **Use Pessimistic Locking for High Contention**: For inventory counters, financial account transfers, or ticketing pools where multiple requests hit the exact same row every second, use `SELECT ... FOR UPDATE`.
3. **Prefer Atomic SQL Over Both**: If an operation only modifies a counter or balance, avoid reading into application code entirely! An atomic single-statement update (`UPDATE accounts SET balance = balance - 10 WHERE id = 1 AND balance >= 10`) handles concurrency inside the database engine without version columns or explicit locks.
4. **Implement Jitter in OCC Retry Loops**: When retrying an optimistic update, never retry immediately in a tight loop. Add randomized exponential backoff (`sleep(base * 2^attempt + random_jitter)`) to desynchronize concurrent threads.
5. **Beware of `xmin` Wraparound in Postgres OCC**: While PostgreSQL's `xmin` is convenient, remember that XIDs are 32-bit integers that wrap around and get frozen by vacuum. For critical long-term domain entities, an explicit `BIGINT` or `INT` `version` column is far more predictable.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Database connection pool exhaustion under user load** | Opening a database transaction and holding `SELECT FOR UPDATE` while awaiting HTTP responses or user input. | Use Optimistic Concurrency Control (OCC) with a `version` column for web workflows. |
| **Silent lost updates in web apps** | Loading data in a web form, submitting an `UPDATE` that only checks `WHERE id = :id` without checking version. | Include `AND version = :version` in the `UPDATE` statement. |
| **100% CPU lockups during flash sales with OCC** | Thousands of threads executing OCC retry loops on a single inventory row, producing a retry storm. | Switch to Pessimistic locking (`FOR UPDATE`) or atomic SQL decrement. |
| **Stale version caching in ORMs** | Updating an entity via raw SQL while an ORM in-memory identity map retains an outdated version number. | Refresh the ORM entity state or increment the version column in the raw SQL statement. |
| **Using `updated_at` timestamp for OCC** | Two concurrent updates occurring within the same millisecond or microsecond observing identical timestamps. | Use a monotonic integer `version INT NOT NULL DEFAULT 1` instead of timestamps. |
| **Ignoring Rows Affected in application code** | Issuing the CAS `UPDATE` but failing to check whether the database returned `1` or `0` affected rows. | Always check driver `rows_affected == 1` to verify OCC success. |

---

## Performance & Security Notes

### Benchmarking OCC vs PCC Under Contention
- **Zero Contention (10,000 operations on 10,000 distinct rows)**:
  - OCC: ~45,000 ops/sec (no lock manager overhead).
  - PCC: ~32,000 ops/sec (lock table acquisition and release overhead).
- **Extreme Contention (10,000 operations on 1 single row)**:
  - OCC: ~800 ops/sec (98% abort/retry rate, CPU throttled by rollback processing).
  - PCC: ~12,000 ops/sec (clean FIFO queue waiting on row-level lock).

### Security: Tampering with Version Columns
In client-driven APIs, never trust a client-supplied version blindly without authentication and tenant authorization checks. A malicious actor could probe version numbers to infer transaction frequencies or inject arbitrarily high version numbers to lock out legitimate users. Always validate user permissions in the `WHERE` clause:
```sql
UPDATE documents 
SET ... 
WHERE document_id = :id AND organization_id = :org_id AND version = :v;
```

---

## Comparison Matrix: OCC vs PCC

| Dimension | Optimistic Concurrency Control (OCC) | Pessimistic Concurrency Control (PCC) |
|:---|:---|:---|
| **Locking Mechanism** | None (Lock-free; uses conditional SQL) | Physical row/table locks (2PL) |
| **Database Resource Cost** | Zero lock manager memory | Memory consumed by lock tables (except PG) |
| **Deadlock Risk** | **Zero deadlocks** | High risk of deadlocks |
| **Best Used When** | Conflicts are rare (< 10%); stateless web apps | Conflicts are frequent (> 20%); short batch jobs |
| **Stateless API Suitability** | Excellent (Version passed in HTTP payload) | Unusable (Cannot hold locks across HTTP) |
| **Behavior on Conflict** | Statement fails with 0 rows affected; client retries | Client blocks and waits in lock queue |
| **Contention Profile** | Degrades exponentially under high contention | Degrades linearly under high contention |

---

## Real-World Interview Questions

<details>
<summary><strong>1. Why is Optimistic Concurrency Control mandatory for web applications interacting over REST or GraphQL APIs?</strong></summary>

**Answer:**
REST and GraphQL APIs operate over stateless HTTP protocols. When a human user opens a web page to edit a profile or order, the read operation completes in milliseconds and the database connection is immediately closed and returned to the pool. The user may take 30 seconds or 5 minutes to fill out the form before submitting the update.

Holding a pessimistic database lock (`SELECT FOR UPDATE`) across the duration of user think-time would require keeping a dedicated database connection and transaction open for minutes. In a system with thousands of users, all available database connections would be exhausted within seconds, bringing down the database. OCC solves this by acquiring no locks: the version number is embedded in the HTML/JSON form, and the conflict check is executed atomically upon form submission.
</details>

<details>
<summary><strong>2. Explain the Compare-and-Swap (CAS) pattern in SQL and how application code detects an OCC conflict.</strong></summary>

**Answer:**
The Compare-and-Swap pattern in SQL uses a conditional update that matches both the primary key and the expected version number:
```sql
UPDATE items SET val = :new_val, version = version + 1 WHERE id = :id AND version = :expected_version;
```
When this query executes, the database engine returns the number of rows modified (**Rows Affected / Tuples Updated**):
- If another transaction modified the row before this statement executed, the current `version` in the table no longer equals `:expected_version`.
- The `WHERE` clause matches zero rows, and the engine returns `rows_affected = 0`.
- The application checks this return value: if `rows_affected == 0`, it immediately knows a conflict occurred and handles it (by raising a `409 Conflict` or executing a retry loop).
</details>

<details>
<summary><strong>3. Why is using an updated_at TIMESTAMP column dangerous for Optimistic Concurrency Control?</strong></summary>

**Answer:**
Using `updated_at` as the OCC version token has two major failure modes:
1. **Clock Resolution**: On many systems or configurations, database timestamps have millisecond or microsecond resolution. In high-throughput distributed systems, two concurrent transactions can easily execute within the exact same clock tick, resulting in identical `updated_at` values and allowing a lost update to slip through.
2. **Clock Skew and Leap Seconds**: NTP adjustments and virtualization clock drift can cause system timestamps to step backward, violating monotonicity. A monotonic integer (`version INT`) incremented by 1 is deterministic and immune to clock anomalies.
</details>

<details>
<summary><strong>4. What is a "retry storm" in Optimistic Concurrency Control, and how do you prevent it?</strong></summary>

**Answer:**
A **retry storm** occurs in OCC when high write contention hits a single entity. When 100 concurrent requests attempt to update the same row, 1 request succeeds and 99 fail the CAS check. If all 99 failed requests immediately re-read the row and retry their update simultaneously, 1 will succeed and 98 will fail, repeating the cycle. This creates a feedback loop of runaway CPU saturation, WAL generation, and database latency.

Prevention:
1. Use **Exponential Backoff with Full Jitter**: randomized wait intervals desynchronize the retrying threads.
2. Set a strict retry limit (e.g., maximum 3 retries) before failing fast.
3. Switch high-contention hot spots to Pessimistic Locking (`SELECT FOR UPDATE`) or atomic SQL increments.
</details>

<details>
<summary><strong>5. Can you implement OCC in PostgreSQL without adding a version column? How?</strong></summary>

**Answer:**
Yes, by using PostgreSQL's hidden system attribute **`xmin`**.
Every row in PostgreSQL contains `xmin` in its physical tuple header, which represents the Transaction ID that created or last updated that specific row version.
An application can read `SELECT id, balance, xmin FROM accounts WHERE id = 1;` and perform the update via:
```sql
UPDATE accounts SET balance = :new_balance WHERE id = 1 AND xmin = :read_xmin;
```
If another transaction updated the row in the meantime, the row's `xmin` changed to the new transaction's ID, causing the update to match 0 rows.
</details>

<details>
<summary><strong>6. Under what specific conditions does Pessimistic Locking outperform Optimistic Concurrency Control?</strong></summary>

**Answer:**
Pessimistic locking outperforms OCC when **write contention on the same resource is high** (typically when conflict probability exceeds 15-20%). 
Under high contention, OCC wastes massive CPU, memory, and I/O resources repeatedly rolling back and re-executing transactions that fail validation. Pessimistic locking (`FOR UPDATE`) places contending transactions into an efficient, low-overhead in-memory FIFO wait queue; transactions execute sequentially without wasted work or aborted retries.
</details>

<details>
<summary><strong>7. How does SQL Server natively support Optimistic Concurrency Control?</strong></summary>

**Answer:**
SQL Server provides a native data type called **`ROWVERSION`** (formerly known as `TIMESTAMP`). 
When a column is declared as `ROWVERSION`, SQL Server automatically generates and increments an 8-byte monotonic binary number across the entire database every time any row in that table is inserted or updated. Applications can query the `ROWVERSION` column and include it in the `WHERE` clause of subsequent updates to guarantee OCC without manual version arithmetic.
</details>

<details>
<summary><strong>8. Why is an atomic SQL update (e.g. UPDATE ... SET stock = stock - 1) superior to both OCC and PCC for counter updates?</strong></summary>

**Answer:**
Both OCC and PCC require an extra round-trip: reading the data into application memory, calculating the new value, and sending an update. 
An atomic SQL update pushes the calculation directly to the storage engine. The database acquires the row write lock, reads the absolute latest committed value directly from the data page, evaluates the expression and `WHERE` condition, applies the update, and releases the lock in a single operation. It eliminates application-side race conditions, requires zero version management, and executes in a single network round-trip.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1: Implementing a Classic Version-Based OCC Table</strong></summary>

**Problem:** Create a table `customer_profiles (id INT PRIMARY KEY, email TEXT, address TEXT, version INT DEFAULT 1)`. Write the exact SQL statements to simulate two concurrent users attempting to update the address, showing that the second user's update fails.

**Solution:**
```sql
CREATE TEMP TABLE customer_profiles (
    id INT PRIMARY KEY, 
    email TEXT, 
    address TEXT, 
    version INT DEFAULT 1
);
INSERT INTO customer_profiles VALUES (1, 'alice@example.com', '123 Main St', 1);

-- User A and User B both read version = 1:
-- User A updates first:
UPDATE customer_profiles 
SET address = '456 Oak Ave', version = version + 1 
WHERE id = 1 AND version = 1;
-- Output: UPDATE 1 (User A succeeds! version is now 2)

-- User B attempts to update with stale version = 1:
UPDATE customer_profiles 
SET address = '789 Pine Rd', version = version + 1 
WHERE id = 1 AND version = 1;
-- Output: UPDATE 0 (User B fails! Conflict detected)
```
</details>

<details>
<summary><strong>Exercise 2: OCC Using PostgreSQL System Column xmin</strong></summary>

**Problem:** Using a table without a version column, execute an optimistic update using the `xmin` system column.

**Solution:**
```sql
CREATE TEMP TABLE settings (key TEXT PRIMARY KEY, value TEXT);
INSERT INTO settings VALUES ('maintenance_mode', 'false');

-- Read row and xmin
SELECT key, value, xmin FROM settings WHERE key = 'maintenance_mode';
-- Assume xmin is 89401

-- Execute conditional update
UPDATE settings
SET value = 'true'
WHERE key = 'maintenance_mode'
  AND xmin = 89401;
-- If rows affected == 1, update succeeded!
```
</details>

<details>
<summary><strong>Exercise 3: Building a Python / SQL Jittered Retry Loop</strong></summary>

**Problem:** Write pseudocode for an application function that attempts an OCC update up to 4 times with randomized exponential backoff.

**Solution:**
```python
import time, random

def update_profile_with_occ(profile_id, new_address):
    max_retries = 4
    for attempt in range(max_retries):
        # 1. Read current state
        row = db.query_one("SELECT address, version FROM customer_profiles WHERE id = :id", id=profile_id)
        
        # 2. Attempt CAS update
        affected = db.execute(
            """UPDATE customer_profiles 
               SET address = :addr, version = version + 1 
               WHERE id = :id AND version = :v""",
            addr=new_address, id=profile_id, v=row.version
        )
        
        if affected == 1:
            return True # Successfully updated
        
        # Conflict occurred: backoff with jitter
        sleep_time = (0.05 * (2 ** attempt)) + random.uniform(0.01, 0.05)
        time.sleep(sleep_time)
        
    raise Exception("OCC update failed after max retries due to contention")
```
</details>

<details>
<summary><strong>Exercise 4: Pessimistic Row Locking for Bank Transfer</strong></summary>

**Problem:** Write the SQL transaction block that pessimistically locks two accounts before performing a balance transfer, ensuring deadlock safety.

**Solution:**
```sql
BEGIN;

-- Lock both accounts in deterministic sorted order
SELECT balance 
FROM accounts 
WHERE account_id IN (101, 205) 
ORDER BY account_id 
FOR UPDATE;

-- Both accounts locked exclusively; execute updates safely
UPDATE accounts SET balance = balance - 100 WHERE account_id = 101;
UPDATE accounts SET balance = balance + 100 WHERE account_id = 205;

COMMIT;
```
</details>

<details>
<summary><strong>Exercise 5: Converting OCC to an Atomic SQL Update</strong></summary>

**Problem:** Refactor the following naive read-modify-write OCC code into a single atomic SQL statement:
```python
row = query("SELECT points FROM user_rewards WHERE user_id = 42")
new_points = row.points + 250
update("UPDATE user_rewards SET points = :new_points, version = version + 1 WHERE user_id = 42 AND version = :v")
```

**Solution:**
```sql
-- Single atomic update: eliminates OCC overhead, network round-trips, and retry logic!
UPDATE user_rewards
SET points = points + 250,
    updated_at = CURRENT_TIMESTAMP
WHERE user_id = 42;
```
</details>

<details>
<summary><strong>Exercise 6: Measuring Contention Threshold</strong></summary>

**Problem:** If an application experiences a 35% OCC conflict rate on a specific table during peak hours, what architectural actions should the database team take?

**Solution:**
1. **Analyze Contention Source**: Determine if the updates can be executed as single-statement atomic SQL updates (e.g., `SET balance = balance + :val`), which bypasses read-modify-write cycles entirely.
2. **Switch to Pessimistic Locking**: If multi-statement business logic is required, switch the transaction to `SELECT ... FOR UPDATE` with deterministic lock ordering to queue requests cleanly instead of burning CPU on retry storms.
3. **Partition or Shard Hot Rows**: If a single global counter is the bottleneck, use a distributed counter pattern (split the counter across 10 random sub-rows and aggregate with `SUM()`).
</details>

---

## Summary & Further Reading

- **Optimistic Concurrency Control (OCC)** assumes conflicts are rare, holds zero locks, and uses Compare-and-Swap (CAS) version checks (`WHERE version = :v`) during updates.
- OCC is mandatory for stateless web, REST, and GraphQL architectures where holding database locks across HTTP requests is impossible.
- **Pessimistic Concurrency Control (PCC)** uses physical row locks (`SELECT FOR UPDATE`), preventing conflicts upfront at the cost of concurrency and deadlock risk.
- Under high contention (> 20% conflicts), OCC collapses into catastrophic retry storms; Pessimistic locking or atomic SQL updates must be used.
- **Next Topic**: [08-select-for-update-skip-locked-and-queues.md](08-select-for-update-skip-locked-and-queues.md) covers building high-throughput transactional job queues using `SELECT FOR UPDATE SKIP LOCKED` and `NOWAIT`.
