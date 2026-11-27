# Long-Running Transactions, Bloat & The Freeze Horizon

## Metadata
| Attribute | Details |
|:---|:---|
| **Topic** | Database Health: Long Transactions, Snapshot Pinning & Vacuum Bloat |
| **Dialect** | Standard ANSI SQL / PostgreSQL 17+ Baseline |
| **Database Version** | PostgreSQL 17+, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Tested with** | PostgreSQL 17.2, SQLite 3.50.4 |
| **Prerequisites** | `04-transactions-and-concurrency/04-mvcc-explained.md`, `04-transactions-and-concurrency/05-locking.md` |
| **Complexity** | Advanced |

---

## Table of Contents
1. [What & Why](#what--why)
2. [The Anatomy of a Long-Running Transaction](#the-anatomy-of-a-long-running-transaction)
3. [The Cascade of Systemic Destruction](#the-cascade-of-systemic-destruction)
4. [The xmin Horizon & Vacuum Starvation](#the-xmin-horizon--vacuum-starvation)
5. [Transaction ID (TXID) Wraparound Emergency](#transaction-id-txid-wraparound-emergency)
6. [Diagnostic Queries & Monitoring](#diagnostic-queries--monitoring)
7. [Cross-Dialect Differences](#cross-dialect-differences)
8. [Practical Examples](#practical-examples)
9. [Remediation: pg_repack vs VACUUM FULL](#remediation-pg_repack-vs-vacuum-full)
10. [Best Practices](#best-practices)
11. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
12. [Performance & Security Notes](#performance--security-notes)
13. [Comparison Matrix: Bloat Characteristics by Engine](#comparison-matrix-bloat-characteristics-by-engine)
14. [Real-World Interview Questions](#real-world-interview-questions)
15. [Practice Exercises](#practice-exercises)
16. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

In multi-tenant, high-throughput database systems, application developers often treat database transactions as harmless local contexts. A developer opens a transaction in their GUI client (like DBeaver or pgAdmin) to run an exploratory query and leaves for lunch; a background worker script opens a transaction, fetches data, and waits for a slow 30-minute third-party HTTP report; an ORM begins a transaction at the start of a request and keeps it open while rendering server-side templates.

To the application, nothing seems wrong: CPU usage is near 0%, and the connection appears completely harmless.

In reality, an open, uncommitted transaction is **the most destructive operational hazard** in a Multi-Version Concurrency Control (MVCC) database engine. A single idle transaction pins the global **`xmin` horizon**, completely paralyzing the database's garbage collection process (`VACUUM` in Postgres, Undo Purge in MySQL). 

Within hours, dead tuples accumulate by the millions, tables and indexes swell to 10x their normal size, the buffer cache degrades into uselessness, and the database drifts toward emergency read-only shutdown due to Transaction ID Wraparound.

---

## The Anatomy of a Long-Running Transaction

In database metrics, long-lived sessions fall into three distinct operational states:

1. **Active Long Query**: A massive analytic report, cross-table join, or full table scan that has been actively burning CPU and I/O for 45 minutes (`state = 'active'`).
2. **Idle in Transaction**: The connection issued `BEGIN` and executed a query, but has since gone silent without issuing `COMMIT` or `ROLLBACK` (`state = 'idle in transaction'`). The client thread is waiting on network I/O, user think-time, or crashed silently.
3. **Abandoned 2PC (Prepared Transaction)**: A distributed two-phase commit transaction (`PREPARE TRANSACTION`) was executed by a coordinator that crashed before issuing `COMMIT PREPARED`. It holds locks and snapshots indefinitely, surviving even server reboots!

---

## The Cascade of Systemic Destruction

```
+-----------------------------------------------------------------------------------------+
|                         THE LONG TRANSACTION CASCADE OF DESTRUCTION                     |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
| 1. Session A opens transaction at 09:00 AM (Assigned XID 10,000).                       |
|    Enters 'idle in transaction' state...                                                |
|                                                                                         |
| 2. LIVE PRODUCTION TRAFFIC (09:00 AM - 12:00 PM):                                       |
|    - Other sessions execute 2,000,000 UPDATE and DELETE statements.                     |
|    - MVCC writes 2,000,000 new tuple versions; 2,000,000 old versions marked DEAD.     |
|                                                                                         |
| 3. AUTOVACUUM RUNS (10:00 AM, 11:00 AM, 12:00 PM):                                      |
|    - Vacuum inspects the dead tuples.                                                   |
|    - Rule: "Cannot remove any dead tuple whose xmax >= oldest active snapshot (10,000)" |
|    - CANNOT RECLAIM A SINGLE BYTE OF SPACE! Vacuum finishes with 0 tuples removed!      |
|                                                                                         |
| 4. SYSTEM DEGRADATION:                                                                  |
|    - Table swell: 200 MB table grows to 8 GB on disk.                                   |
|    - Index bloat: B-Trees swell with dead leaf pointers; sequential scans required.     |
|    - Shared buffers polluted: RAM fills with dead tuples, evicting active cache.        |
|    - Query latency spikes from 2ms to 850ms across the entire database!                 |
|                                                                                         |
| 5. EMERGENCY SHUTDOWN (Worst Case):                                                     |
|    - Active XIDs approach 2 billion transactions past 10,000.                           |
|    - Postgres triggers emergency shutdown to prevent TXID Wraparound data corruption!   |
+-----------------------------------------------------------------------------------------+
```

---

## The xmin Horizon & Vacuum Starvation

In PostgreSQL, the **`xmin` horizon** is the Transaction ID of the oldest active transaction across the entire cluster (including transactions, physical/logical replication slots, and prepared transactions).

```
Timeline:  XID 9,000 ------> XID 10,000 (Session A) ------> XID 15,000 (Current)
                                    ^
                             xmin Horizon
                                    |
  [ SAFE TO RECLAIM BY VACUUM ]     |    [ UNTOUCHABLE BY VACUUM ]
  Dead tuples with xmax < 10,000    |    Dead tuples with xmax >= 10,000
```

Even if a dead tuple was deleted hours ago, if its `xmax` is greater than or equal to Session A's `xmin` (10,000), `VACUUM` **must leave it on disk untouched**, because Session A's snapshot could theoretically issue a query that needs to see the historical tuple.

---

## Transaction ID (TXID) Wraparound Emergency

PostgreSQL assigns 32-bit transaction IDs, providing $2^{32} \approx 4.29 \text{ billion}$ values.
- PostgreSQL compares XIDs using modulo arithmetic: an XID is considered "in the past" if it is within 2 billion transactions behind the current XID, and "in the future" if it is ahead.
- Normal autovacuum periodically **freezes** old row headers (`HEAP_XMIN_FROZEN`), converting their XID into a special permanent marker that is considered older than all numbers.
- **The Wraparound Danger**: If a long-running transaction or replication slot blocks the freeze horizon from advancing for 2 billion transactions, older committed rows would suddenly appear to be in the "future" and disappear from all query results.
- **The Fail-Safe**: If the oldest unfrozen XID reaches `autovacuum_freeze_max_age` (default: 200 million transactions old), PostgreSQL launches aggressive wraparound autovacuum workers. If unaddressed and the gap approaches 2 billion, **PostgreSQL abruptly shuts down and refuses all new client connections**, forcing a DBA into single-user emergency maintenance mode.

---

## Diagnostic Queries & Monitoring

### 1. Identifying Long-Running and Idle Transactions
```sql
SELECT 
    pid,
    usename,
    application_name,
    client_addr,
    state,
    backend_xmin,
    backend_xid,
    CURRENT_TIMESTAMP - xact_start AS transaction_duration,
    CURRENT_TIMESTAMP - state_change AS state_duration,
    query
FROM pg_stat_activity
WHERE state IN ('idle in transaction', 'active')
  AND (CURRENT_TIMESTAMP - xact_start) > INTERVAL '5 minutes'
ORDER BY transaction_duration DESC;
```

### 2. Monitoring the Freeze Horizon & Distance to Wraparound
```sql
SELECT 
    datname,
    age(datfrozenxid) AS xid_age,
    2147483648 - age(datfrozenxid) AS transactions_until_emergency_shutdown
FROM pg_database
ORDER BY xid_age DESC;
```

### 3. Detecting Lagging Replication Slots Blocking Vacuum
```sql
SELECT 
    slot_name,
    plugin,
    slot_type,
    active,
    age(xmin) AS xmin_age,
    age(catalog_xmin) AS catalog_xmin_age
FROM pg_replication_slots
ORDER BY age(xmin) DESC;
```

---

## Cross-Dialect Differences

```
+----------------------------------------------------------------------------------------------------+
| DIALECT COMPARISON: LONG TRANSACTIONS & SYSTEMIC BLOAT IMPACT                                      |
+-------------------+--------------------------------------------------------------------------------+
| PostgreSQL 17+    | Long txns pin xmin horizon; autovacuum cannot clean dead heap tuples. Severe    |
|                   | table and index bloat. Risks 32-bit TXID wraparound emergency shutdown.        |
+-------------------+--------------------------------------------------------------------------------+
| MySQL 8.4 (InnoDB)| Long txns hold read views; InnoDB Purge threads cannot purge undo log segments.|
|                   | History List Length (HLL) explodes into millions; ibdata1 / undo tablespaces   |
|                   | swell permanently on disk; buffer pool thrashing.                              |
+-------------------+--------------------------------------------------------------------------------+
| SQLite 3.50+      | A long-running reader in WAL mode prevents WAL checkpoints from resetting. The |
|                   | -wal file grows to dozens of gigabytes, severely slowing read performance.     |
+-------------------+--------------------------------------------------------------------------------+
| SQL Server 2022   | When RCSI/Snapshot is enabled, long txns prevent version cleanup in tempdb.   |
|                   | tempdb fills disk, causing entire SQL Server instance to halt on out-of-space.|
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Terminating an Abandoned Idle Transaction
When an uncommitted session is pinning the horizon:
```sql
-- Step 1: Gracefully cancel the currently executing query (if active)
SELECT pg_cancel_backend(14205);

-- Step 2: If session remains 'idle in transaction', forcibly terminate the connection socket
SELECT pg_terminate_backend(14205);
```

### Example 2: Configuring Server-Wide Automatic Timeouts
Preventing developers and buggy code from abandoning transactions:
```sql
-- In postgresql.conf or session:
-- Automatically terminate any session that stays 'idle in transaction' for > 60 seconds
ALTER SYSTEM SET idle_in_transaction_session_timeout = '60s';

-- Terminate any single SQL statement executing longer than 15 minutes
ALTER SYSTEM SET statement_timeout = '15min';

-- Reload server configuration without rebooting
SELECT pg_reload_conf();
```

### Example 3: Edge Case — Finding and Dropping Abandoned Prepared Transactions (2PC)
A distributed transaction that was prepared but never committed or rolled back:
```sql
-- Inspect prepared transactions
SELECT gid, prepared, owner, database FROM pg_prepared_xacts;

-- If a transaction has been prepared for hours/days, roll it back explicitly:
ROLLBACK PREPARED 'distributed_tx_order_9981';
```

### Example 4: Failure Mode — The Old Snapshot Threshold
What happens when you configure `old_snapshot_threshold`:
```sql
-- PostgreSQL feature to protect against bloat:
-- If set (e.g. old_snapshot_threshold = '4h'), autovacuum IS ALLOWED to reclaim dead tuples
-- even if an active snapshot is older than 4 hours!

-- If that ancient query eventually attempts to read the reclaimed page:
-- ERROR: snapshot too old
-- SQLSTATE: 72000
-- This sacrifices the single ancient report to protect the health of the entire database!
```

### Example 5: Identifying Table Bloat Percentage
Estimating dead tuple percentage across user tables:
```sql
SELECT 
    schemaname,
    relname AS table_name,
    n_live_tup AS live_rows,
    n_dead_tup AS dead_rows,
    ROUND(n_dead_tup * 100.0 / NULLIF(n_live_tup + n_dead_tup, 0), 2) AS bloat_ratio_pct,
    last_autovacuum
FROM pg_stat_user_tables
WHERE n_dead_tup > 50000
ORDER BY bloat_ratio_pct DESC;
```

---

## Remediation: pg_repack vs VACUUM FULL

Once dead tuples have caused massive disk bloat, how do you reclaim the physical disk space and return it to the operating system?

```
+----------------------------------------------------------------------------------------------------+
| COMPARISON: VACUUM FULL vs PG_REPACK                                                               |
+-------------------------------+----------------------------------+---------------------------------+
| Feature                       | VACUUM FULL                      | pg_repack (Extension)           |
+-------------------------------+----------------------------------+---------------------------------+
| **Lock Required**             | ACCESS EXCLUSIVE on whole table  | Brief AccessExclusive at start/ |
|                               | (Blocks ALL reads and writes!)   | end; full concurrency during run|
+-------------------------------+----------------------------------+---------------------------------+
| **Production Impact**         | Total application downtime       | Safe for live production use    |
|                               | during entire table rebuild.     | without taking downtime.        |
+-------------------------------+----------------------------------+---------------------------------+
| **How It Works**              | Re-writes entire table to a new  | Creates shadow log table, syncs |
|                               | file on disk; drops old file.    | via trigger, swaps catalog OID. |
+-------------------------------+----------------------------------+---------------------------------+
| **Disk Space Needed**         | 2x current table size            | 2x current table size           |
+-------------------------------+----------------------------------+---------------------------------+
| **Installation**              | Native PostgreSQL command        | Third-party extension           |
+-------------------------------+----------------------------------+---------------------------------+
```

### Using pg_repack in Production
```bash
# Rebuild bloated table without blocking reads or writes:
pg_repack -h localhost -U postgres -d production_db -t customer_orders
```

---

## Best Practices

1. **Set `idle_in_transaction_session_timeout` Everywhere**: Set a cluster-wide default of `60s` or `120s`. There is virtually zero legitimate reason in an OLTP web architecture for a transaction to sit idle waiting for queries for minutes.
2. **Never Run Ad-Hoc Analytics on OLTP Primary**: Never allow data analysts or BI reporting tools to run long queries directly on the production primary writer. Route all analytical queries to dedicated read replicas.
3. **Monitor Inactive Replication Slots**: Inactive logical or physical replication slots retain WAL files on disk and hold back the catalog `xmin` horizon. Automatically alert on and drop slots inactive for > 1 hour.
4. **Use `old_snapshot_threshold`**: On large enterprise clusters, configure `old_snapshot_threshold` (e.g., `4h` or `8h`) to allow autovacuum to reclaim space, failing ancient queries with `snapshot too old` rather than letting the entire cluster swell with bloat.
5. **Reindex Periodically with `CONCURRENTLY`**: B-Tree indexes that have experienced high bloat do not easily shrink their internal pages. Run `REINDEX TABLE CONCURRENTLY table_name;` to rebuild bloated indexes online.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Table size swells from 1 GB to 50 GB** | A single forgotten session left in `idle in transaction` blocking autovacuum dead-tuple cleanup. | Enforce `idle_in_transaction_session_timeout = '60s'` and kill the blocking session. |
| **Emergency database shutdown (`wraparound`)** | Transaction freeze horizon blocked for weeks by an abandoned logical replication slot. | Drop abandoned replication slots with `pg_drop_replication_slot()`. |
| **Replication replica lag explodes** | Running massive multi-hour transactions on the primary, generating millions of WAL files that saturate network bandwidth. | Break large batch updates into smaller, bounded chunked transactions. |
| **Running VACUUM FULL during business hours** | Attempting to reclaim bloat using `VACUUM FULL`, which takes an exclusive table lock and freezes the whole application. | Use `pg_repack` or perform `VACUUM FULL` during an authorized maintenance window. |
| **MySQL undo tablespaces consuming 500 GB disk** | A long-running report holding a Read View in MySQL InnoDB, causing Undo Log History List Length (HLL) explosion. | Kill the long-running query and configure dedicated read replicas for reporting. |
| **SQLite -wal file growing to 20 GB** | An open read transaction in SQLite preventing background WAL checkpointing from truncating the log file. | Close read transactions promptly in application code. |

---

## Performance & Security Notes

### Shared Buffers Cache Dilution
The invisible performance penalty of table bloat is **buffer pool dilution**:
- In a healthy table with 0% bloat, an 8 KB page holds ~100 live customer rows. Scanning 10,000 customers requires reading 100 pages (800 KB of RAM).
- In a bloated table with 90% dead tuples, that same 8 KB page holds only 10 live rows and 90 dead rows.
- Scanning those same 10,000 customers now requires loading 1,000 pages (8 MB of RAM) into the buffer pool.
- Result: Active hot data is violently evicted from shared memory, disk I/O skyrockets, and query latency degrades by orders of magnitude across completely unrelated tables!

### Security: Resource Depletion Attacks
An authenticated user with read permissions can intentionally launch a transaction, execute `SELECT 1;`, and refuse to commit. By keeping the session open, the attacker holds the `xmin` horizon hostage, forcing the database to swell with dead tuples until the server exhausts disk space. Strict session-level timeouts are a critical security hardening requirement.

---

## Comparison Matrix: Bloat Characteristics by Engine

| Dimension | PostgreSQL 17+ | MySQL 8.4 (InnoDB) | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Primary Bloat Location** | Table heap and secondary indexes | Undo tablespaces / ibdata1 | `-wal` file on disk | `tempdb` Version Store |
| **Impact on Table Pages** | Pages fill with dead tuples | Clustered index pages stay compact | DB pages stay compact | DB pages stay compact |
| **Catastrophic Failure Mode** | TXID Wraparound emergency halt | Disk full from undo logs | Slow read performance | `tempdb` out of disk space |
| **Online De-bloat Tool** | `pg_repack` (Extension) | `ALTER TABLE ... ENGINE=InnoDB` | `PRAGMA wal_checkpoint(TRUNCATE)` | Automatic background version cleanup |
| **Timeout Protection** | `idle_in_transaction_session_timeout` | `interactive_timeout` / `wait_timeout` | Application level only | Session timeout settings |

---

## Real-World Interview Questions

<details>
<summary><strong>1. Why does an uncommitted transaction in PostgreSQL that only executed a single SELECT statement cause table bloat on completely different tables?</strong></summary>

**Answer:**
When a transaction opens in PostgreSQL and executes a query, it takes a **snapshot** of the system state. Its backend process registers its `xmin` (the oldest active transaction ID) in shared memory (`PGPROC`).

PostgreSQL's garbage collector (`VACUUM`) calculates the global **`xmin` horizon** as the lowest `xmin` across all active backends. To maintain snapshot isolation, `VACUUM` is strictly forbidden from removing any dead tuple created or deleted by a transaction with `XID >= xmin_horizon` anywhere in the database. 

Therefore, even if Session A only read Table X, its pinned snapshot prevents `VACUUM` from reclaiming dead tuples generated by millions of updates on Table Y, Table Z, and every other table in the cluster, causing cluster-wide table and index bloat.
</details>

<details>
<summary><strong>2. What is the difference between VACUUM, VACUUM FULL, and pg_repack?</strong></summary>

**Answer:**
- **`VACUUM`**: Scans table pages, marks dead tuple space as reusable in the Free Space Map (FSM), and updates visibility maps. It does *not* shrink the table file on disk or return space to the OS (except if trailing pages at the very end of the file are empty). It takes a weak `SHARE UPDATE EXCLUSIVE` lock and runs concurrently with reads and writes.
- **`VACUUM FULL`**: Rewrites the entire table into a brand new disk file, eliminating all bloat and returning all empty space to the OS. However, it takes an **`ACCESS EXCLUSIVE`** lock, completely blocking all reads and writes for the duration of the rewrite (causes full application downtime).
- **`pg_repack`**: Rebuilds the table and indexes online without taking an exclusive lock. It creates a shadow table, copies live data, captures concurrent mutations via triggers, and swaps the tables in catalog metadata in a split second, enabling zero-downtime bloat removal.
</details>

<details>
<summary><strong>3. What is Transaction ID Wraparound, and why does PostgreSQL shut down when it approaches?</strong></summary>

**Answer:**
PostgreSQL uses 32-bit transaction IDs, which wrap around modulo $2^{32} \approx 4.29 \text{ billion}$. Because XID comparisons determine row visibility (past vs future), if the current XID advances 2 billion transactions past the creation XID of an unfrozen row, that old row would suddenly appear to be in the "future" and vanish from query results (data loss).

To prevent this, `VACUUM` must "freeze" old tuples. If long transactions or neglected maintenance prevent freezing, and the oldest unfrozen XID approaches 2 billion transactions old, PostgreSQL executes an emergency fail-safe shutdown to protect data integrity, refusing all write traffic until a DBA manually runs `VACUUM FREEZE` in single-user mode.
</details>

<details>
<summary><strong>4. How do abandoned replication slots cause disk space exhaustion?</strong></summary>

**Answer:**
PostgreSQL replication slots guarantee that the primary server will not discard any Write-Ahead Log (WAL) segments or advance catalog xmin horizons until the connected replica has acknowledged receiving and processing them.

If a replication slot is abandoned (e.g., a replica was decommissioned or crashed, but the slot was not dropped on the primary):
1. The primary server retains every WAL file generated in `pg_wal/`, filling disk space until the filesystem runs out of space.
2. For logical replication slots, the slot holds back `catalog_xmin`, preventing `VACUUM` from removing dead tuples from system catalogs, causing severe catalog bloat.
</details>

<details>
<summary><strong>5. What is the purpose of the idle_in_transaction_session_timeout parameter?</strong></summary>

**Answer:**
`idle_in_transaction_session_timeout` is a server configuration parameter introduced in PostgreSQL 9.6. It specifies the maximum duration (e.g., `'60s'`) that a connection can remain in the `idle in transaction` state (i.e., having opened a transaction with `BEGIN` and executed commands, but currently waiting for the client to send the next command).

If the timeout expires, the database server forcibly terminates the connection socket and rolls back the transaction, immediately releasing held locks, unpinning the `xmin` horizon, and protecting the database from vacuum starvation and bloat.
</details>

<details>
<summary><strong>6. What happens in MySQL InnoDB when a transaction remains open for a long time?</strong></summary>

**Answer:**
In MySQL InnoDB, when an `UPDATE` or `DELETE` executes, the old version of the row is written to an **Undo Log** segment in the undo tablespace. 

A long-running transaction with an active read view requires access to older undo records. Because InnoDB's master Purge threads cannot delete any undo log record that is still needed by the oldest active read view, the **History List Length (HLL)** grows to millions of pages. The undo tablespace files swell on disk, the buffer pool becomes clogged with undo pages, and performance degrades severely across the cluster.
</details>

<details>
<summary><strong>7. How does the old_snapshot_threshold parameter protect PostgreSQL from table bloat?</strong></summary>

**Answer:**
`old_snapshot_threshold` allows the database administrator to decouple vacuuming from ancient read snapshots:
- If set (e.g., `old_snapshot_threshold = '4h'`), PostgreSQL permits `VACUUM` to reclaim dead tuples that are older than 4 hours, even if an active transaction has a snapshot that started earlier.
- If that old transaction subsequently attempts to read a data page where dead tuples have been vacuumed away, PostgreSQL raises an error: `ERROR: snapshot too old (SQLSTATE 72000)`.
- This ensures that a single runaway analytics query cannot hold the entire cluster hostage and cause catastrophic bloat.
</details>

<details>
<summary><strong>8. What is "index bloat", and why does regular VACUUM fail to fix it?</strong></summary>

**Answer:**
When rows are updated or deleted, dead pointers accumulate in B-Tree index leaf pages. 
While standard `VACUUM` clears dead pointers from index pages, it **cannot merge or shrink empty index pages back into the filesystem**. 

If a table undergoes millions of updates, B-Tree leaf pages split repeatedly; after vacuuming, the index may consist of thousands of nearly empty, fragmented 8 KB pages. The index size on disk remains bloated, wasting buffer pool memory and increasing tree traversal depth. Resolving index bloat requires rebuilding the index via `REINDEX TABLE CONCURRENTLY` or `pg_repack`.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1: Diagnosing Stalled Vacuum Horizons</strong></summary>

**Problem:** Write a SQL query in PostgreSQL to identify whether the vacuum horizon is currently held back by an active transaction, a prepared transaction, or a replication slot.

**Solution:**
```sql
SELECT 'ACTIVE_SESSION' AS blocker_type, pid::text AS identifier, backend_xmin::text AS held_xmin, 
       CURRENT_TIMESTAMP - xact_start AS duration
FROM pg_stat_activity 
WHERE backend_xmin IS NOT NULL
UNION ALL
SELECT 'PREPARED_TRANSACTION', gid, NULL, CURRENT_TIMESTAMP - prepared
FROM pg_prepared_xacts
UNION ALL
SELECT 'REPLICATION_SLOT', slot_name, xmin::text, NULL
FROM pg_replication_slots
WHERE xmin IS NOT NULL;
```
</details>

<details>
<summary><strong>Exercise 2: Simulating and Observing Dead Tuple Accumulation</strong></summary>

**Problem:** Create a table `churn_demo (id INT PRIMARY KEY, val TEXT)`. Insert 1 row. Update it 1,000 times in a loop. Query `pg_stat_user_tables` to observe the dead tuple count.

**Solution:**
```sql
CREATE TABLE churn_demo (id INT PRIMARY KEY, val TEXT);
INSERT INTO churn_demo VALUES (1, 'Initial');

-- Execute 1,000 updates:
DO $$
BEGIN
    FOR i IN 1..1000 LOOP
        UPDATE churn_demo SET val = 'Update #' || i WHERE id = 1;
    END LOOP;
END $$;

-- Check dead tuples:
SELECT relname, n_live_tup, n_dead_tup 
FROM pg_stat_user_tables 
WHERE relname = 'churn_demo';
-- Result: n_live_tup = 1, n_dead_tup = 1000!
```
</details>

<details>
<summary><strong>Exercise 3: Rebuilding Bloated Indexes Without Downtime</strong></summary>

**Problem:** Table `orders` has a severely bloated index `idx_orders_customer_id`. Write the SQL command to rebuild this index online without blocking concurrent `INSERT` and `UPDATE` traffic.

**Solution:**
```sql
REINDEX INDEX CONCURRENTLY idx_orders_customer_id;
```
</details>

<details>
<summary><strong>Exercise 4: Monitoring MySQL InnoDB History List Length</strong></summary>

**Problem:** Write the SQL query in MySQL to check the InnoDB History List Length and identify if undo logs are purging properly.

**Solution:**
```sql
SELECT count 
FROM information_schema.innodb_metrics 
WHERE name = 'trx_rseg_history_len';
-- Values above 1,000,000 indicate severe undo log purge blockage!
```
</details>

<details>
<summary><strong>Exercise 5: Terminating Idle Transactions via SQL Script</strong></summary>

**Problem:** Write an automated maintenance query that terminates any backend that has been in `idle in transaction` for more than 10 minutes, returning the terminated PIDs.

**Solution:**
```sql
SELECT 
    pid, 
    usename, 
    pg_terminate_backend(pid) AS terminated
FROM pg_stat_activity
WHERE state = 'idle in transaction'
  AND (CURRENT_TIMESTAMP - state_change) > INTERVAL '10 minutes';
```
</details>

<details>
<summary><strong>Exercise 6: Calculating Database Age and Freeze Headroom</strong></summary>

**Problem:** Write a PostgreSQL query that returns the percentage of the 2-billion transaction limit consumed by each database on the server, alerting if any exceeds 60%.

**Solution:**
```sql
SELECT 
    datname,
    age(datfrozenxid) AS current_age,
    ROUND((age(datfrozenxid)::numeric / 2147483648::numeric) * 100, 2) AS pct_of_wraparound_limit,
    CASE 
        WHEN (age(datfrozenxid)::numeric / 2147483648::numeric) > 0.60 THEN 'CRITICAL ALERT: Approaching Wraparound'
        ELSE 'HEALTHY'
    END AS status
FROM pg_database
ORDER BY current_age DESC;
```
</details>

---

## Summary & Further Reading

- An open, uncommitted transaction pins the global `xmin` horizon, preventing `VACUUM` from reclaiming dead tuples across the entire database cluster.
- Vacuum starvation causes massive table and index bloat, evicting active data from the shared buffer cache and destroying query performance.
- Stalled freeze horizons risk catastrophic **Transaction ID (TXID) Wraparound**, forcing emergency database shutdown.
- Always configure `idle_in_transaction_session_timeout` and route analytics queries to read replicas.
- Use `pg_repack` or `REINDEX CONCURRENTLY` to de-bloat tables and indexes online without production downtime.
- **Next Topic**: [11-distributed-transactions-overview.md](11-distributed-transactions-overview.md) explores distributed transactions, Two-Phase Commit (2PC), consensus protocols, and the Saga pattern.
