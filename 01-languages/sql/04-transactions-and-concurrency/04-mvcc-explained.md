# Multi-Version Concurrency Control (MVCC) Explained

## Metadata
| Attribute | Details |
|:---|:---|
| **Topic** | Storage Engine Internals: MVCC Mechanics & Visibility |
| **Dialect** | Standard ANSI SQL / PostgreSQL 17+ Baseline |
| **Database Version** | PostgreSQL 17+, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Tested with** | PostgreSQL 17.2, SQLite 3.50.4 |
| **Prerequisites** | `04-transactions-and-concurrency/01-acid.md`, `04-transactions-and-concurrency/03-isolation-levels-and-anomalies.md` |
| **Complexity** | Advanced |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [How It Works Internally](#how-it-works-internally)
4. [PostgreSQL Heap MVCC vs MySQL InnoDB Undo Logs](#postgresql-heap-mvcc-vs-mysql-innodb-undo-logs)
5. [Syntax & Inspection Tools](#syntax--inspection-tools)
6. [Cross-Dialect Differences](#cross-dialect-differences)
7. [Practical Examples](#practical-examples)
8. [Best Practices](#best-practices)
9. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
10. [Performance & Security Notes](#performance--security-notes)
11. [Comparison Matrix: MVCC Implementations](#comparison-matrix-mvcc-implementations)
12. [Real-World Interview Questions](#real-world-interview-questions)
13. [Practice Exercises](#practice-exercises)
14. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

In early relational database architectures (like original SQL Server and Informix), concurrency was managed exclusively using **Two-Phase Locking (2PL)**. If Session A wanted to read a million rows for an analytics report, it acquired shared read locks (`S`) across those rows. If Session B arrived to update a single user's address, it was blocked until Session A finished. Conversely, a writer modifying data blocked all readers from accessing that data.

This lock contention created unbearable bottlenecks in high-throughput web and OLTP applications.

**Multi-Version Concurrency Control (MVCC)** solves this fundamental dilemma with a revolutionary design principle:
> **Readers never block writers, and writers never block readers.**

Under MVCC, when a transaction updates or deletes a row, the database engine does not overwrite the existing data in-place. Instead, it creates a new version of the row while retaining the older versions. When concurrent readers query the database, the engine inspects the reader's transaction snapshot and dynamically serves the specific historical version of the row that was committed at the instant the reader's snapshot was taken.

---

## Core Concepts

### 1. Row Versions (Tuples)
Under MVCC, a logical row in a table may consist of multiple physical versions simultaneously stored in the database:
- **Active / Visible Version**: The current committed state seen by new transactions.
- **Historical Versions**: Previous states required by concurrently running long queries or transactions that opened their snapshots in the past.
- **Dead Versions**: Outdated versions created by old transactions that have committed or rolled back, which are no longer visible to any active transaction in the entire system.

### 2. Snapshots (`SnapshotData`)
A **Snapshot** is an in-memory data structure representing a consistent point in time. In PostgreSQL, a snapshot consists of three key elements:
- `xmin`: The earliest transaction ID that was still active when this snapshot was created. Any transaction with `XID < xmin` is guaranteed to have committed or aborted.
- `xmax`: The highest transaction ID assigned so far $+ 1$. Any transaction with `XID >= xmax` was not yet started and is completely invisible.
- `xip_list`: An array of specific active transaction IDs between `xmin` and `xmax` at the moment the snapshot was taken.

### 3. Visibility Checking
When a query scans a table page, it evaluates each physical row version against its snapshot:
- Was the row created by a transaction that committed before our snapshot? **Yes $\rightarrow$ Potentially visible.**
- Was the row deleted or updated by a transaction that committed before our snapshot? **Yes $\rightarrow$ Invisible.**
- Was the row created or modified by an active uncommitted transaction or a transaction that started after our snapshot? **Yes $\rightarrow$ Invisible.**

---

## How It Works Internally

### PostgreSQL Heap-Based MVCC Architecture
In PostgreSQL, all row versions live directly inside the regular 8 KB table data pages (the Heap):
- An `INSERT` creates a physical tuple in a page and sets `xmin = CurrentXID`, `xmax = 0`.
- A `DELETE` does not delete the bytes; it sets `xmax = CurrentXID` on the existing tuple.
- An `UPDATE` performs an atomic `DELETE` + `INSERT`: it sets `xmax = CurrentXID` on the old tuple, creates a **brand new physical tuple** with `xmin = CurrentXID`, and points the old tuple's `t_ctid` pointer to the new tuple's block and offset.

```
+-----------------------------------------------------------------------------------------+
|                         POSTGRESQL HEAP-BASED TUPLE STRUCTURE                           |
+-----------------------------------------------------------------------------------------+
| Page Offset ItemPointer -> [ Tuple Header (23 bytes) ] [ User Data Columns ... ]        |
|                                                                                         |
| Header Fields:                                                                          |
| - xmin:        Transaction ID that inserted this tuple version                          |
| - xmax:        Transaction ID that deleted or replaced this tuple version (0 if alive)  |
| - t_ctid:      Physical location (page, offset) of this tuple OR the newer version      |
| - t_infomask:  Bitmask flags (HEAP_XMIN_COMMITTED, HEAP_XMAX_INVALID, etc.)             |
+-----------------------------------------------------------------------------------------+

Example: UPDATE accounts SET balance = 300 WHERE id = 1 (Executed by Transaction 105)

Old Tuple Version (Block 0, Offset 1):
+------------------------------------------------------------------------------------+
| xmin: 100 | xmax: 105 | t_ctid: (0, 2) | Data: id=1, balance=100 (OLD / DEAD)      |
+------------------------------------------------------------------------------------+
                               |
                               | t_ctid pointer
                               v
New Tuple Version (Block 0, Offset 2):
+------------------------------------------------------------------------------------+
| xmin: 105 | xmax: 0   | t_ctid: (0, 2) | Data: id=1, balance=300 (NEW / ACTIVE)    |
+------------------------------------------------------------------------------------+
```

---

## PostgreSQL Heap MVCC vs MySQL InnoDB Undo Logs

The two most popular open-source databases chose completely opposite physical architectures to implement MVCC:

```
+----------------------------------------------------------------------------------------------------+
| ARCHITECTURAL COMPARISON: POSTGRESQL HEAP vs MYSQL INNODB UNDO LOGS                                |
+-------------------------------+----------------------------------+---------------------------------+
| Dimension                     | PostgreSQL (Heap MVCC)           | MySQL InnoDB (Undo Log MVCC)    |
+-------------------------------+----------------------------------+---------------------------------+
| **Where row versions live**   | All versions stored in the table | Only newest version in clustered|
|                               | heap data pages.                 | index; older versions in Undo.  |
+-------------------------------+----------------------------------+---------------------------------+
| **How UPDATE works**          | Writes a NEW row to the heap.    | In-place update in B-Tree page; |
|                               | Leaves old tuple marked dead.    | writes inverse diff to Undo log.|
+-------------------------------+----------------------------------+---------------------------------+
| **Index maintenance**         | Must update ALL secondary indexes| Secondary indexes point to PK;  |
|                               | unless HOT optimization applies. | no secondary index rewrite!     |
+-------------------------------+----------------------------------+---------------------------------+
| **Rollback cost**             | Instantaneous (marks XID aborted | Costly: Must physically replay  |
|                               | in pg_xact; leaves dead tuples). | undo log to revert in-place mod.|
+-------------------------------+----------------------------------+---------------------------------+
| **Garbage collection**        | Requires VACUUM / autovacuum to  | Master Purge thread purges undo |
|                               | sweep and reclaim dead heap space| log segments once oldest reader |
|                               | (prone to table bloat).          | finishes (no table heap bloat). |
+-------------------------------+----------------------------------+---------------------------------+
```

---

## Syntax & Inspection Tools

### Inspecting Tuple Headers in PostgreSQL (`pageinspect`)
PostgreSQL provides the official extension `pageinspect` to view raw physical tuple headers on disk:

```sql
-- Enable the extension
CREATE EXTENSION IF NOT EXISTS pageinspect;

-- Inspect raw tuples in page 0 of 'accounts'
SELECT 
    lp AS item_offset,
    t_xmin AS xmin,
    t_xmax AS xmax,
    t_ctid AS ctid,
    t_data
FROM heap_page_items(get_raw_page('accounts', 0));
```

### Inspecting Active Snapshots
```sql
-- In PostgreSQL, view your session's current transaction snapshot
SELECT pg_current_snapshot();
-- Example output: "1050:1055:1051,1053"
-- Format: xmin:xmax:xip_list
```

---

## Cross-Dialect Differences

```
+----------------------------------------------------------------------------------------------------+
| DIALECT COMPARISON: MVCC IMPLEMENTATION DETAILS                                                    |
+-------------------+--------------------------------------------------------------------------------+
| PostgreSQL 17+    | Append-only heap MVCC. Generates dead tuples on UPDATE/DELETE. Requires        |
|                   | autovacuum. HOT (Heap-Only Tuple) optimizes updates if indexed cols unchanged. |
+-------------------+--------------------------------------------------------------------------------+
| MySQL 8.4 (InnoDB)| In-place update + Undo Log segment MVCC. Clustered index contains DB_TRX_ID   |
|                   | and DB_ROLL_PTR (7-byte pointer to undo record). Purge threads clean undo logs.|
+-------------------+--------------------------------------------------------------------------------+
| SQLite 3.50+      | In WAL mode (PRAGMA journal_mode = WAL), readers read from main DB file and WAL|
|                   | file using an in-memory WAL index (wal-index). Allows concurrent readers + 1 w.|
+-------------------+--------------------------------------------------------------------------------+
| SQL Server 2022   | When RCSI or SNAPSHOT isolation is on, row versions are copied into tempdb     |
|                   | Version Store. Cleaned up by asynchronous background version cleanup task.     |
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Demonstrating Non-Blocking Readers and Writers
Showing how a long reader is not blocked by a heavy concurrent writer:
```sql
-- Session 1: Large report reading table
BEGIN;
SELECT count(*), avg(sale_amount) FROM sales_records;
-- Session 1 is scanning 10 million rows...

-- Session 2 (Concurrently):
BEGIN;
UPDATE sales_records SET sale_amount = sale_amount * 1.05 WHERE region = 'US_EAST';
COMMIT;
-- Session 2 commits instantly! It is NOT blocked by Session 1.
-- Session 1 finishes and sees the exact sales values as they existed when its transaction opened!
```

### Example 2: Inspecting Hidden System Columns in PostgreSQL
PostgreSQL exposes physical MVCC metadata as hidden system attributes on every table:
```sql
CREATE TEMP TABLE inventory (item_id INT PRIMARY KEY, qty INT);

INSERT INTO inventory VALUES (1, 100);

-- Query the hidden columns: xmin, xmax, and ctid (page block, item index)
SELECT ctid, xmin, xmax, item_id, qty FROM inventory;
-- Output:
-- ctid  | xmin | xmax | item_id | qty
-- (0,1) | 501  | 0    | 1       | 100

-- Now update the row
UPDATE inventory SET qty = 150 WHERE item_id = 1;

-- Query again
SELECT ctid, xmin, xmax, item_id, qty FROM inventory;
-- Output:
-- ctid  | xmin | xmax | item_id | qty
-- (0,2) | 502  | 0    | 1       | 150
-- Notice: ctid changed from (0,1) to (0,2)! A new physical tuple was written to block 0!
```

### Example 3: Edge Case — Heap-Only Tuple (HOT) Optimization
When an `UPDATE` does not modify any column covered by an index, and the new tuple fits inside the exact same 8 KB page:
```sql
-- Table with index on primary key
CREATE TABLE users (id INT PRIMARY KEY, bio TEXT);
INSERT INTO users VALUES (1, 'Software Engineer');

-- Update non-indexed column 'bio'
UPDATE users SET bio = 'Senior Software Engineer' WHERE id = 1;

-- If HOT is triggered:
-- 1. No new index entry is added to users_pkey!
-- 2. The index pointer still points to (0,1).
-- 3. The engine follows the internal page chain from (0,1) to (0,2).
-- 4. Result: Zero index write I/O, massive write acceleration!
```

### Example 4: Failure Mode — Long-Running Transaction Causing Catastrophic Bloat
```sql
-- Scenario: Developer opens a transaction in DBeaver/pgAdmin and leaves for lunch:
BEGIN;
SELECT * FROM orders WHERE order_id = 1;
-- Session remains in 'idle in transaction' for 3 hours...

-- Meanwhile, background worker processes execute 500,000 updates:
-- UPDATE order_items SET status = 'SHIPPED' WHERE ...

-- Because Session 1's snapshot requires seeing tuples as they were 3 hours ago:
-- AUTOVACUUM CANNOT REMOVE ANY OF THE 500,000 DEAD TUPLES!
-- The table swells from 50 MB to 4 GB! Disk space exhausts, queries slow down!
```

### Example 5: Optimized Vacuum Tuning to Mitigate MVCC Bloat
```sql
-- Configuring aggressive per-table autovacuum for a high-churn table
ALTER TABLE active_jobs SET (
    autovacuum_vacuum_scale_factor = 0.05, -- Vacuum when 5% of rows are dead (default is 20%)
    autovacuum_vacuum_threshold = 50,      -- Minimum dead rows
    autovacuum_vacuum_cost_limit = 1000    -- Allocate more I/O budget to vacuum workers
);
```

---

## Best Practices

1. **Avoid Idle Transactions**: Never leave connections in `idle in transaction` state. Configure `idle_in_transaction_session_timeout = '60s'` to terminate abandoned transactions automatically.
2. **Favor HOT (Heap-Only Tuple) Updates**: Keep indexes narrow. If a frequently updated column (like `updated_at` or `status`) is included in indexes unnecessarily, every update generates duplicate index entries across every secondary index, defeating HOT optimization.
3. **Tune Autovacuum Proactively**: Do not disable autovacuum. On write-heavy tables, decrease `autovacuum_vacuum_scale_factor` from the default `0.20` to `0.05` or `0.02` to clean dead tuples continuously before table pages fragment.
4. **Monitor Snapshot Horizons**: Regularly inspect `pg_stat_activity` and `pg_stat_database` to detect long-lived transactions holding back the global `xmin` horizon.
5. **Beware of Large In-Memory Undo in MySQL**: Long-running transactions in MySQL cause the InnoDB Undo log history list length (HLL) to grow into millions of pages, severely degrading buffer pool cache efficiency.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Table size multiplies 10x despite constant row count** | Frequent `UPDATE` statements creating dead tuples faster than autovacuum can reclaim them. | Tune autovacuum scale factors, optimize for HOT updates, or run `pg_repack`. |
| **`autovacuum: VACUUM ... (to prevent wraparound)` high CPU** | Running a single uncommitted transaction for days, blocking the global frozen transaction ID horizon. | Identify and kill long-running idle transactions immediately. |
| **Index scan latency steadily degrades over time** | Non-HOT updates generating millions of orphaned index leaf node pointers that must be traversed. | Run `REINDEX TABLE CONCURRENTLY` and remove unnecessary indexes on mutated columns. |
| **MySQL `history list length` spikes to millions** | Open transactions holding read views in MySQL, preventing the master purge thread from discarding undo segments. | Close transactions promptly; avoid analytics queries on primary OLTP instances. |
| **Assuming DELETE immediately frees disk space** | Expecting the OS file size to shrink after running `DELETE FROM large_table`. | MVCC marks rows dead; space is reused internally for future inserts. Use `VACUUM FULL` or `pg_repack` to shrink files. |
| **Vacuum worker starved of I/O** | Leaving `autovacuum_vacuum_cost_limit` at the default conservative 200, throttling vacuum on modern NVMe SSDs. | Increase `autovacuum_vacuum_cost_limit` to 2000+ on SSD hardware. |

---

## Performance & Security Notes

### The Freeze Horizon and Transaction ID Wraparound (PostgreSQL)
PostgreSQL's `xmin` and `xmax` are stored as 32-bit unsigned integers. This means there are approximately $2^{32} \approx 4.29 \text{ billion}$ possible transaction IDs:
- To compare transaction IDs across time, PostgreSQL uses modular arithmetic ($XID_1 - XID_2 < 2^{31}$).
- Every row version must eventually be **frozen** (`HEAP_XMIN_FROZEN` bit set in `t_infomask`), declaring that the row was committed so far in the past that it is permanently visible to all current and future transactions.
- If a database fails to freeze rows before the transaction counter approaches 2 billion transactions, PostgreSQL initiates a fail-safe shutdown and **refuses all write connections** to prevent catastrophic data corruption.

### Security: Residual Data in Dead Tuples
Because MVCC retains old row versions in data pages until vacuumed, sensitive data (such as passwords, credit card numbers, or PII) that has been `DELETED` or `UPDATED` physically remains on disk in plain text within dead tuples until vacuumed and overwritten. Systems handling PCI-DSS or GDPR compliance must take into account that `DELETE` does not perform a secure cryptographic wipe of disk blocks.

---

## Comparison Matrix: MVCC Implementations

| Feature | PostgreSQL 17+ | MySQL 8.4 (InnoDB) | SQLite 3.50+ | SQL Server 2022 (RCSI) |
|:---|:---|:---|:---|:---|
| **Storage Architecture** | Append-only heap pages | Clustered Index + Undo Segments | WAL file + DB file | In-place + `tempdb` Version Store |
| **UPDATE Mechanism** | Dead tuple + New tuple in heap | In-place overwrite + Undo write | Appends new frame to WAL | In-place + writes old version to tempdb |
| **Secondary Indexes** | Point to physical heap TID (`ctid`) | Point to Primary Key value | Point to rowid | Point to clustered key / RID |
| **Garbage Collector** | Autovacuum background worker | Purge threads (InnoDB Purge) | Checkpointer (WAL checkpoint) | Ghost Cleanup & Version Cleanup |
| **HOT Optimization** | Yes (Heap-Only Tuples) | Not needed (clustered index) | N/A | N/A |
| **Risk of Bloat** | Table heap and index bloat | Tablespace / undo tablespace bloat | WAL file bloat | `tempdb` disk space exhaustion |

---

## Real-World Interview Questions

<details>
<summary><strong>1. Explain the mantra "Readers never block writers, and writers never block readers" in the context of MVCC.</strong></summary>

**Answer:**
Under traditional Two-Phase Locking (2PL), a reading transaction acquires shared locks (`S`) that block modifying transactions (`X`), and a writing transaction acquires exclusive locks (`X`) that block reading transactions.

Under MVCC:
- When a writer updates a row, it does not overwrite the existing data; it creates a new version while leaving the old version intact.
- When a reader queries the row, it evaluates its private snapshot against the row's version metadata (`xmin`/`xmax` in Postgres, or undo chain in InnoDB) and reads the older committed version.
- Therefore, the reader never waits for the writer's exclusive lock, and the writer never waits for the reader's shared lock. (Writers only block other writers attempting to mutate the exact same row).
</details>

<details>
<summary><strong>2. How does an UPDATE statement work under PostgreSQL's heap-based MVCC, and why does it differ from MySQL InnoDB?</strong></summary>

**Answer:**
- In **PostgreSQL**, an `UPDATE` does not modify the existing row in place. It performs an atomic `DELETE` + `INSERT`: it writes the current transaction's ID into the existing row's `xmax` header (marking it dead to future transactions) and writes a brand new physical tuple into a data page with `xmin = CurrentXID`. Both versions physically reside in the table heap.
- In **MySQL InnoDB**, an `UPDATE` modifies the row **in-place** directly within the clustered index leaf page. Before overwriting the columns, InnoDB writes the previous column values into an **Undo Log** record and sets the row's internal `DB_ROLL_PTR` pointer to reference that undo record. Readers needing the historical version reconstruct it dynamically by traversing the undo log chain backwards.
</details>

<details>
<summary><strong>3. What is Heap-Only Tuple (HOT) optimization in PostgreSQL, and what conditions are required for it to activate?</strong></summary>

**Answer:**
Under standard PostgreSQL updates, creating a new tuple in the heap requires adding new index entries in *every single secondary index* on that table, pointing to the new tuple's `ctid`.

**HOT (Heap-Only Tuple)** optimization activates if two conditions are met:
1. The `UPDATE` statement does not modify any column that is covered by an index on that table.
2. The page containing the old tuple has enough free space to store the new tuple version.

When HOT activates, no new index entries are created. The index continues pointing to the old tuple, and the engine follows an internal intra-page link (`t_ctid`) from the old tuple to the new tuple. This eliminates secondary index write I/O and drastic index bloat.
</details>

<details>
<summary><strong>4. What is a "dead tuple" in PostgreSQL, and why doesn't running DELETE free up disk space to the operating system?</strong></summary>

**Answer:**
A **dead tuple** is a row version in a PostgreSQL table page that has been updated or deleted, whose deleting transaction (`xmax`) has committed, and which is older than the `xmin` horizon of any currently active transaction in the entire database.

Running `DELETE` does not shrink the database file or return space to the OS because:
1. Reallocating, truncating, or moving 8 KB pages in physical OS files requires expensive table rewrites and exclusive locks.
2. PostgreSQL marks the dead space within the 8 KB page as reusable in its internal **Free Space Map (FSM)**. Subsequent `INSERT` or `UPDATE` operations will reuse these empty slots inside the existing pages before allocating new disk blocks.
</details>

<details>
<summary><strong>5. Explain the concept of Transaction ID (XID) Wraparound in PostgreSQL and how the engine prevents it.</strong></summary>

**Answer:**
PostgreSQL assigns 32-bit transaction IDs, allowing roughly 4.29 billion unique values. Because numbers wrap around modulo $2^{32}$, an XID generated today would eventually appear to be in the "future" relative to an older XID, which would make older committed rows suddenly invisible (catastrophic data loss).

To prevent this, PostgreSQL uses **Row Freezing**:
- When a row is sufficiently old (controlled by `vacuum_freeze_min_age`), `VACUUM` sets a special flag (`HEAP_XMIN_FROZEN`) in the tuple's `t_infomask`.
- A frozen tuple is permanently considered committed and older than all possible current or future transactions.
- If autovacuum fails to keep up and un-frozen rows approach 2 billion transactions old, PostgreSQL triggers aggressive autovacuum wraparound workers; if unaddressed, it shuts down into read-only mode to prevent corruption.
</details>

<details>
<summary><strong>6. What is the "xmin horizon" and what holds it back?</strong></summary>

**Answer:**
The **xmin horizon** is the lowest `xmin` (oldest active transaction ID or snapshot) across all currently active sessions and replication slots in the database.

It represents the safety boundary for garbage collection: `VACUUM` cannot reclaim any dead tuple whose `xmax` is greater than or equal to the xmin horizon, because an active session might still need to view that historical tuple. 

The horizon is held back by:
1. Long-running transactions or queries.
2. Sessions stuck in `idle in transaction`.
3. Inactive or lagging logical/physical replication slots (`pg_replication_slots`).
4. Abandoned prepared transactions (`2PC`).
</details>

<details>
<summary><strong>7. How does SQLite handle MVCC in WAL (Write-Ahead Logging) mode?</strong></summary>

**Answer:**
In SQLite's WAL mode (`PRAGMA journal_mode = WAL`), modifications are appended as sequential frames to a separate `-wal` file instead of overwriting the main `.db` file. 
- An in-memory index (`-shm` shared-memory file) maps page numbers to the latest frame numbers in the WAL file.
- When a reader begins a transaction, it records the current end-of-WAL frame. It reads pages from the WAL file up to that recorded frame, and falls back to the main `.db` file for unmodified pages.
- This allows multiple concurrent readers to query snapshots while a single writer appends new frames to the WAL, achieving MVCC read concurrency.
</details>

<details>
<summary><strong>8. What are "hint bits" in PostgreSQL tuple headers?</strong></summary>

**Answer:**
When a tuple is read, PostgreSQL must determine whether the creating transaction (`xmin`) or deleting transaction (`xmax`) committed. Checking this requires consulting the commit status log (`pg_xact`), which requires shared memory or disk lookups.

To optimize this, the first transaction that inspects the tuple and discovers that `xmin` has committed sets **hint bits** (`HEAP_XMIN_COMMITTED` or `HEAP_XMIN_INVALID`) directly inside the tuple's header flag (`t_infomask`). All subsequent transactions inspecting that tuple can instantly see that it is committed by reading the hint bits in the page itself, completely bypassing lookups to `pg_xact`.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1: Inspecting Tuple Versions with Hidden Columns</strong></summary>

**Problem:** Create a temporary table `test_mvcc (id INT, val TEXT)`. Insert a row, inspect `xmin`, `xmax`, and `ctid`. Update the row twice, and observe how `xmin` and `ctid` change across updates.

**Solution:**
```sql
CREATE TEMP TABLE test_mvcc (id INT, val TEXT);
INSERT INTO test_mvcc VALUES (1, 'Initial');

-- Step 1: Initial state
SELECT ctid, xmin, xmax, * FROM test_mvcc;
-- Result: ctid = (0, 1), xmin = XID_A, xmax = 0

-- Step 2: First Update
UPDATE test_mvcc SET val = 'Second' WHERE id = 1;
SELECT ctid, xmin, xmax, * FROM test_mvcc;
-- Result: ctid = (0, 2), xmin = XID_B, xmax = 0

-- Step 3: Second Update
UPDATE test_mvcc SET val = 'Third' WHERE id = 1;
SELECT ctid, xmin, xmax, * FROM test_mvcc;
-- Result: ctid = (0, 3), xmin = XID_C, xmax = 0
```
</details>

<details>
<summary><strong>Exercise 2: Detecting Table Bloat and Dead Tuples</strong></summary>

**Problem:** Write a query against PostgreSQL's system statistics catalog `pg_stat_user_tables` to identify tables with the highest number of dead tuples and their ratio to live tuples.

**Solution:**
```sql
SELECT 
    schemaname,
    relname AS table_name,
    n_live_tup AS live_tuples,
    n_dead_tup AS dead_tuples,
    ROUND(n_dead_tup * 100.0 / NULLIF(n_live_tup + n_dead_tup, 0), 2) AS dead_tuple_ratio_pct,
    last_vacuum,
    last_autovacuum
FROM pg_stat_user_tables
WHERE n_dead_tup > 1000
ORDER BY n_dead_tup DESC;
```
</details>

<details>
<summary><strong>Exercise 3: Identifying the Oldest Active Transaction Holding the xmin Horizon</strong></summary>

**Problem:** Write a query to pinpoint the exact session in PostgreSQL that is holding back the database-wide vacuum horizon.

**Solution:**
```sql
SELECT 
    pid,
    usename,
    state,
    backend_xmin,
    backend_xid,
    CURRENT_TIMESTAMP - xact_start AS transaction_age,
    query
FROM pg_stat_activity
WHERE backend_xmin IS NOT NULL OR backend_xid IS NOT NULL
ORDER BY COALESCE(backend_xmin, backend_xid) ASC
LIMIT 1;
```
</details>

<details>
<summary><strong>Exercise 4: Evaluating HOT Update Eligibility</strong></summary>

**Problem:** Table `metrics (id INT PRIMARY KEY, server_id VARCHAR(32), cpu_usage NUMERIC, recorded_at TIMESTAMPTZ)`.
If an index exists on `(server_id, recorded_at)`, will an update to `cpu_usage` be eligible for HOT optimization? Explain why or why not.

**Solution:**
Yes, it is eligible!
Because `cpu_usage` is not included in the primary key index (`id`) or the composite index `(server_id, recorded_at)`, modifying `cpu_usage` does not require updating any index keys. As long as the page containing the old tuple has sufficient free space (fillfactor allows room), PostgreSQL will execute a Heap-Only Tuple update.
</details>

<details>
<summary><strong>Exercise 5: Verifying Non-Blocking Concurrent Reads in SQLite</strong></summary>

**Problem:** Write SQLite commands to enable WAL mode, verify the journal mode, and explain how it allows concurrent readers during active write transactions.

**Solution:**
```sql
-- Check current mode:
PRAGMA journal_mode;

-- Enable WAL mode:
PRAGMA journal_mode = WAL;
-- Returns: 'wal'

-- Verify checkpoint status:
PRAGMA wal_checkpoint;
```
*(In WAL mode, SQLite appends modifications to the `-wal` file. Readers read from the database and wal files concurrently using memory-mapped shared memory without waiting for the writer's exclusive lock).*
</details>

<details>
<summary><strong>Exercise 6: Configuring Fillfactor for HOT Updates</strong></summary>

**Problem:** Write the DDL to create a table `session_tokens (token_id UUID PRIMARY KEY, last_seen_at TIMESTAMPTZ)` configured with an 80% fillfactor to leave 20% free space in each page for HOT updates.

**Solution:**
```sql
CREATE TABLE session_tokens (
    token_id UUID PRIMARY KEY,
    last_seen_at TIMESTAMPTZ NOT NULL
) WITH (fillfactor = 80);
```
*(By reserving 20% headroom in each 8 KB page, frequent updates to `last_seen_at` can write new tuple versions into the same page, ensuring nearly 100% HOT update success rate).*
</details>

---

## Summary & Further Reading

- MVCC allows readers and writers to execute concurrently without blocking each other.
- PostgreSQL implements MVCC by appending new tuple versions directly into table heap pages and using `xmin`/`xmax` headers.
- MySQL InnoDB updates rows in-place and writes historical versions to Undo Log rollback segments.
- PostgreSQL requires `VACUUM` to clean dead tuples; uncontrolled long transactions freeze the xmin horizon and cause catastrophic table bloat.
- HOT (Heap-Only Tuple) optimization avoids index maintenance when non-indexed columns are updated.
- **Next Topic**: [05-locking.md](05-locking.md) examines physical locking mechanisms: shared vs exclusive locks, table vs row locking hierarchies, and lock queue management.
