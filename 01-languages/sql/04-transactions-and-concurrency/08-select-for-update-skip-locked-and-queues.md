# SELECT FOR UPDATE, SKIP LOCKED & Transactional Job Queues

## Metadata
| Attribute | Details |
|:---|:---|
| **Topic** | Explicit Row Locking, Non-Blocking Dequeuing & Job Queue Architecture |
| **Dialect** | Standard ANSI SQL / PostgreSQL 17+ Baseline |
| **Database Version** | PostgreSQL 17+, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Tested with** | PostgreSQL 17.2, SQLite 3.50.4 |
| **Prerequisites** | `04-transactions-and-concurrency/05-locking.md`, `04-transactions-and-concurrency/07-optimistic-vs-pessimistic-concurrency.md` |
| **Complexity** | Advanced |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Explicit Row Lock Strengths](#explicit-row-lock-strengths)
3. [The NOWAIT and SKIP LOCKED Clauses](#the-nowait-and-skip-locked-clauses)
4. [Building High-Throughput Job Queues in SQL](#building-high-throughput-job-queues-in-sql)
5. [Syntax & Implementation Patterns](#syntax--implementation-patterns)
6. [Cross-Dialect Differences](#cross-dialect-differences)
7. [Practical Examples](#practical-examples)
8. [Best Practices](#best-practices)
9. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
10. [Performance & Security Notes](#performance--security-notes)
11. [Comparison Matrix: Queue Solutions](#comparison-matrix-queue-solutions)
12. [Real-World Interview Questions](#real-world-interview-questions)
13. [Practice Exercises](#practice-exercises)
14. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

In distributed systems, asynchronous task execution and background job processing are ubiquitous. Applications must send emails, process uploaded video files, generate monthly invoices, and sync data to external search engines.

Traditionally, software engineers reached for dedicated message brokers (such as RabbitMQ, Kafka, or Amazon SQS). While powerful, dedicated message brokers introduce significant operational complexity, distributed state desynchronization, and the dreaded **Dual-Write Problem**: if your application saves an order in PostgreSQL and crashes before pushing the message to SQS, your database and message broker permanently diverge.

By utilizing relational database row locking with **`SKIP LOCKED`**, you can build high-performance, strictly transactional job queues directly inside your database:
- State mutations and queue messages are written in the **exact same ACID transaction**.
- Multiple concurrent worker processes can dequeue jobs simultaneously without blocking each other.
- If a worker crashes midway through executing a job, the database automatically rolls back the transaction, releases the row lock, and makes the job immediately available for another worker to process.

---

## Explicit Row Lock Strengths

When querying rows with the intent to modify or protect them, PostgreSQL provides four distinct locking clauses, ordered from strongest to weakest:

```
+-----------------------------------------------------------------------------------------+
|                               ROW LOCK STRENGTH SPECTRUM                                |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  [ FOR UPDATE ]                                                                         |
|    - Strongest lock.                                                                    |
|    - Acquired by DELETE and UPDATE on unique/PK columns.                                |
|    - Conflicts with ALL other row locks (FOR UPDATE, NO KEY UPDATE, SHARE, KEY SHARE).  |
|                                                                                         |
|  [ FOR NO KEY UPDATE ]                                                                  |
|    - Acquired automatically by standard UPDATE on non-key columns.                      |
|    - Blocks other writers, but ALLOWS concurrent FOR KEY SHARE (foreign keys).          |
|                                                                                         |
|  [ FOR SHARE ]                                                                          |
|    - Shared read lock.                                                                  |
|    - Blocks concurrent UPDATE / DELETE, but allows concurrent readers (FOR SHARE).      |
|                                                                                         |
|  [ FOR KEY SHARE ]                                                                      |
|    - Weakest lock. Acquired automatically by foreign key constraint validators.         |
|    - Allows concurrent FOR NO KEY UPDATE! Only blocks FOR UPDATE and DELETE.            |
+-----------------------------------------------------------------------------------------+
```

---

## The NOWAIT and SKIP LOCKED Clauses

By default, when a query encounters a row locked by another transaction, it **blocks and sleeps** until the locking transaction finishes (`COMMIT` or `ROLLBACK`). 

SQL provides two critical modifiers to alter this waiting behavior:

### 1. `NOWAIT`
Informs the engine never to wait. If any matching row is currently locked by another session, the statement aborts immediately with SQLSTATE `55P03` (`lock_not_available`):
```sql
SELECT * FROM seats WHERE seat_id = '14B' FOR UPDATE NOWAIT;
-- If locked by another customer, fails instantly:
-- ERROR: could not obtain lock on row in relation "seats"
```

### 2. `SKIP LOCKED`
Informs the engine to **skip over any rows that are currently locked** by concurrent transactions and return only the unlocked rows satisfying the query predicate:
```sql
SELECT * FROM job_queue WHERE status = 'PENDING' ORDER BY priority DESC LIMIT 5 FOR UPDATE SKIP LOCKED;
-- If worker A has locked jobs 1 and 2, worker B skips 1 and 2 and receives jobs 3 and 4!
-- Neither worker waits. Lock contention is ZERO!
```

---

## Building High-Throughput Job Queues in SQL

```
+-----------------------------------------------------------------------------------------+
|                           SKIP LOCKED WORKER DEQUEUE FLOW                               |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  Queue Table: [ Job 1 (Locked by W1) ] [ Job 2 (Locked by W1) ] [ Job 3 (Available) ]  |
|               [ Job 4 (Available) ]    [ Job 5 (Available) ]                            |
|                                                                                         |
|  Worker 1 (Active)                        Worker 2 (Concurrent Poll)                    |
|  -----------------                        --------------------------                    |
|  BEGIN;                                   BEGIN;                                        |
|  SELECT * FROM job_queue                  SELECT * FROM job_queue                       |
|  WHERE status = 'PENDING'                 WHERE status = 'PENDING'                      |
|  ORDER BY id LIMIT 2                      ORDER BY id LIMIT 2                           |
|  FOR UPDATE SKIP LOCKED;                  FOR UPDATE SKIP LOCKED;                       |
|  -> Acquires Jobs 1 & 2                   -> Skips 1 & 2! Acquires Jobs 3 & 4!          |
|                                                                                         |
|  * Worker 1 processes jobs...             * Worker 2 processes jobs...                  |
|  * UPDATE job_queue SET status='DONE'     * UPDATE job_queue SET status='DONE'          |
|  * COMMIT; (Releases locks)               * COMMIT; (Releases locks)                    |
+-----------------------------------------------------------------------------------------+
```

---

## Syntax & Implementation Patterns

### The Production Atomic Dequeue Pattern (CTE + UPDATE)
In PostgreSQL, you can fetch, lock, and transition jobs to `RUNNING` in a single atomic SQL statement using a writable Common Table Expression (CTE):

```sql
WITH next_jobs AS (
    SELECT job_id
    FROM background_jobs
    WHERE status = 'PENDING'
      AND run_at <= CURRENT_TIMESTAMP
    ORDER BY priority DESC, created_at ASC
    LIMIT 10
    FOR UPDATE SKIP LOCKED
)
UPDATE background_jobs j
SET status = 'RUNNING',
    locked_by_worker = 'worker_node_04',
    started_at = CURRENT_TIMESTAMP,
    attempt_count = attempt_count + 1
FROM next_jobs
WHERE j.job_id = next_jobs.job_id
RETURNING j.job_id, j.task_type, j.payload;
```

---

## Cross-Dialect Differences

```
+----------------------------------------------------------------------------------------------------+
| DIALECT COMPARISON: SKIP LOCKED & NON-BLOCKING LOCKING                                             |
+-------------------+--------------------------------------------------------------------------------+
| PostgreSQL 17+    | Native support since 9.5. Supports NOWAIT and SKIP LOCKED across all row-lock   |
|                   | modes (FOR UPDATE, FOR NO KEY UPDATE, FOR SHARE, FOR KEY SHARE).               |
+-------------------+--------------------------------------------------------------------------------+
| MySQL 8.4 (InnoDB)| Native support since 8.0 for SELECT ... FOR UPDATE / FOR SHARE NOWAIT /        |
|                   | SKIP LOCKED. Does not support FOR NO KEY UPDATE.                               |
+-------------------+--------------------------------------------------------------------------------+
| SQLite 3.50+      | Not supported. SQLite locks at the whole database/WAL level; fine-grained      |
|                   | row-level SKIP LOCKED is physically impossible.                                |
+-------------------+--------------------------------------------------------------------------------+
| SQL Server 2022   | Implemented via table hints: WITH (UPDLOCK, READPAST, ROWLOCK).                |
|                   | READPAST skips locked rows/pages. Common in SQL Server queue implementations.  |
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Basic Seat Reservation with NOWAIT
Booking a movie or flight seat where the user must be informed immediately if another customer is in the checkout flow:
```sql
BEGIN;

-- Attempt to lock the seat without waiting
SELECT seat_number, price 
FROM theater_seats 
WHERE theater_id = 101 AND seat_number = 'G14' 
  AND status = 'AVAILABLE'
FOR UPDATE NOWAIT;

-- If locked, application catches lock_not_available (55P03) and displays:
-- "Seat G14 is currently held by another user. Please choose another seat."

UPDATE theater_seats 
SET status = 'RESERVED', reserved_by = 4059 
WHERE theater_id = 101 AND seat_number = 'G14';

COMMIT;
```

### Example 2: Building an Outbox Pattern Queue (Real-World)
Guaranteeing atomic domain event publishing without dual-write inconsistencies:
```sql
-- Step 1: User creates an order. Both order and outbox event are saved in 1 transaction:
BEGIN;

INSERT INTO customer_orders (order_id, customer_id, total_amount)
VALUES (99001, 452, 129.50);

INSERT INTO transactional_outbox (event_type, aggregate_type, aggregate_id, payload)
VALUES (
    'ORDER_CREATED', 
    'ORDER', 
    '99001', 
    '{"customer_id": 452, "amount": 129.50}'::jsonb
);

COMMIT; -- Atomic! The event CANNOT exist without the order, and vice versa.

-- Step 2: Asynchronous Event Publisher worker polls outbox with SKIP LOCKED:
BEGIN;

WITH event_batch AS (
    SELECT event_id, event_type, payload
    FROM transactional_outbox
    WHERE status = 'PENDING'
    ORDER BY created_at ASC
    LIMIT 100
    FOR UPDATE SKIP LOCKED
)
UPDATE transactional_outbox o
SET status = 'PROCESSING',
    locked_at = CURRENT_TIMESTAMP
FROM event_batch
WHERE o.event_id = event_batch.event_id
RETURNING event_batch.event_id, event_batch.event_type, event_batch.payload;

-- Worker publishes events to Kafka / RabbitMQ...
-- When ACKed:
UPDATE transactional_outbox SET status = 'PUBLISHED' WHERE event_id IN (...);
COMMIT;
```

### Example 3: Edge Case — Handling Worker Node Crashes (Stale Lock Reclaim)
If a worker server suffers a hardware crash or out-of-memory kill while executing a long job, the transaction connection closes and PostgreSQL releases row locks automatically. But what if the worker wrote `status = 'RUNNING'` in a separate commit?
```sql
-- Periodic reaper query: Reclaims jobs abandoned in 'RUNNING' state for > 10 minutes
UPDATE background_jobs
SET status = 'PENDING',
    locked_by_worker = NULL,
    locked_at = NULL,
    error_log = error_log || E'\nReclaimed after worker timeout'
WHERE status = 'RUNNING'
  AND started_at < CURRENT_TIMESTAMP - INTERVAL '10 minutes';
```

### Example 4: Failure Mode — The Index Scan Lock Leak
What happens if you run `FOR UPDATE SKIP LOCKED` without an index matching your `WHERE` and `ORDER BY` clauses?
```sql
-- Anti-Pattern: No index on (status, priority)
-- Table has 5,000,000 rows!
SELECT job_id 
FROM background_jobs 
WHERE status = 'PENDING' 
ORDER BY priority DESC 
LIMIT 1 
FOR UPDATE SKIP LOCKED;

-- FAILURE MECHANISM:
-- Because no index exists, PostgreSQL must execute a Sequential Scan over the entire table.
-- During the sequential scan, the engine evaluates row locks on EVERY tuple it inspects!
-- Result: The query locks thousands of unrelated rows during the scan, defeating SKIP LOCKED!
-- Solution: CREATE INDEX idx_jobs_pending ON background_jobs (priority DESC, created_at ASC) WHERE status = 'PENDING';
```

### Example 5: Optimized Dead-Letter Queue (DLQ) Architecture
Retrying failed jobs up to a maximum threshold and routing to DLQ:
```sql
CREATE TABLE task_queue (
    task_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    task_name VARCHAR(64) NOT NULL,
    payload JSONB NOT NULL,
    status VARCHAR(20) NOT NULL DEFAULT 'PENDING',
    retry_count INT NOT NULL DEFAULT 0,
    max_retries INT NOT NULL DEFAULT 3,
    last_error TEXT,
    run_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Worker marks failure or routes to DEAD_LETTER:
UPDATE task_queue
SET status = CASE 
        WHEN retry_count + 1 >= max_retries THEN 'DEAD_LETTER' 
        ELSE 'PENDING' 
    END,
    retry_count = retry_count + 1,
    run_at = CURRENT_TIMESTAMP + (INTERVAL '10 seconds' * power(2, retry_count)), -- Exponential backoff!
    last_error = 'Connection timeout to payment gateway'
WHERE task_id = 1042;
```

---

## Best Practices

1. **Always Use Partial Indexes on Queue Tables**: Queue tables grow rapidly, but 99% of rows are `COMPLETED`. Indexing completed rows wastes disk and write throughput. Always create a partial index on pending work:
   ```sql
   CREATE INDEX idx_jobs_poll ON background_jobs (priority DESC, run_at ASC)
   WHERE status = 'PENDING';
   ```
2. **Never Omit `LIMIT` with `SKIP LOCKED`**: Always pair `FOR UPDATE SKIP LOCKED` with a small, bounded `LIMIT` (e.g., 5 to 50 items). Without `LIMIT`, a single worker would lock the entire table, starving all other workers.
3. **Keep Lock Holding Times Minimal**: Workers should only hold database row locks during the brief dequeue and state transition phase. Commit the dequeue transaction immediately, execute the heavy background task outside the transaction, and record the result in a new transaction.
4. **Purge Completed Jobs Continuously**: Queue tables suffer from extreme dead-tuple churn. Configure aggressive autovacuum or run periodic partition pruning / truncation on completed jobs.
5. **Use `FOR UPDATE OF table_name` in Complex Joins**: When joining multiple tables inside a locking query, specify which table to lock (`FOR UPDATE OF j`) to avoid taking unintended locks on reference lookup tables.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Workers serialize and throughput collapses** | Using standard `FOR UPDATE` without `SKIP LOCKED`. Worker 2 blocks waiting for Worker 1 to commit. | Always append `SKIP LOCKED` to queue polling queries. |
| **All workers dequeue the exact same row** | Running plain `SELECT ... LIMIT 1` followed by an `UPDATE` in separate transactions without row locks. | Combine locking and update into an atomic CTE with `FOR UPDATE SKIP LOCKED`. |
| **Severe sequential scan lock contention** | Polling a queue table without an index supporting both the `WHERE` filter and `ORDER BY` clause. | Create a targeted partial B-Tree index covering the pending query predicate. |
| **Dual-write data corruption** | Updating user state in the database, but publishing queue events via an external message bus client outside the transaction. | Use the Transactional Outbox Pattern with `SKIP LOCKED`. |
| **Queue table bloat swelling to 100 GB** | Failing to delete or vacuum completed jobs in high-throughput systems processing millions of tasks daily. | Partition queue tables by day/hour and drop old partitions, or delete completed rows immediately. |
| **Stale jobs stuck permanently in 'RUNNING'** | A worker process crashed without transaction rollback, leaving jobs flagged as `RUNNING` in the database. | Implement a background watchdog/reaper query that reclaims jobs exceeding a timeout. |

---

## Performance & Security Notes

### Throughput Benchmarks: SQL Queues vs Dedicated Brokers
Modern PostgreSQL running on NVMe storage with connection pooling can easily achieve:
- **10,000 to 25,000 dequeues per second** using `FOR UPDATE SKIP LOCKED` across 50 concurrent worker threads.
- While Kafka or Pulsar achieve millions of events/sec, most enterprise applications process fewer than 5,000 background jobs/sec. For this volume, a PostgreSQL queue eliminates an entire distributed infrastructure dependency (Zookeeper/Kafka clusters), simplifies operations, and guarantees strict ACID transactional semantics.

### Security: Queue Poisoning & Payload Sanitization
Payloads stored in queue tables are often deserialized and executed by privileged background workers. If users can inject arbitrary JSON into queue payloads (e.g., webhook processing), worker nodes may be vulnerable to Remote Code Execution (RCE) or SQL injection during worker-side execution. Always validate payloads against a strict schema (e.g., JSON Schema or typed protobuf/structs) before execution.

---

## Comparison Matrix: Queue Solutions

| Dimension | PostgreSQL `SKIP LOCKED` | Redis (Streams / Lists) | RabbitMQ | Apache Kafka |
|:---|:---|:---|:---|:---|
| **ACID Transactional with Business DB?** | **Yes (100% Guaranteed)** | No (Separate datastore) | No (Separate broker) | No (Dual-write hazard) |
| **Throughput** | 10k - 30k msgs/sec | 100k+ msgs/sec | 50k - 100k msgs/sec | 1M+ events/sec |
| **Operational Complexity** | Zero (Uses existing DB) | Low (Requires Redis server) | Medium (Clustering, Erlang) | High (Brokers, storage, schema)|
| **Job Prioritization** | Native (`ORDER BY priority`) | Complex | Priority queues supported | Not supported (FIFO per partition)|
| **Scheduled / Delayed Jobs** | Native (`WHERE run_at <= NOW`) | Supported via Sorted Sets | Requires delayed exchange | Requires custom topics/delay logic |
| **Crash Recovery** | Automatic rollback | Requires manual ACK logic | Channel ACK mechanics | Consumer Offset commit |

---

## Real-World Interview Questions

<details>
<summary><strong>1. Explain how SKIP LOCKED eliminates lock contention between concurrent worker threads.</strong></summary>

**Answer:**
Under standard row locking (`SELECT ... FOR UPDATE`), when Worker 1 acquires an exclusive lock on Row 1, any concurrent query from Worker 2 attempting to lock Row 1 will **block and sleep** until Worker 1 commits. If 20 workers poll the queue for the top pending job, all 19 workers serialize behind the first worker, reducing concurrency to a crawl.

With `SKIP LOCKED`, the database engine's row-filtering engine checks whether a candidate row is currently locked by another transaction. If it is locked, the engine simply skips over that row as if it did not match the query predicate and inspects the next matching row. As a result, each concurrent worker instantly acquires a distinct, non-overlapping batch of unlocked rows without blocking, achieving true horizontal scalability.
</details>

<details>
<summary><strong>2. What is the Dual-Write Problem, and how does the Transactional Outbox Pattern solve it using SKIP LOCKED?</strong></summary>

**Answer:**
The **Dual-Write Problem** occurs in distributed architectures when an application must update its database (e.g., insert an Order) and notify external systems via a message broker (e.g., publish to Kafka). Because distributed transactions across disparate systems are unreliable and slow, network drops or application crashes between the database commit and the broker publish leave the system in an inconsistent state (e.g., order created, but payment event never sent).

The **Transactional Outbox Pattern** solves this by inserting the event into an `outbox` table in the *exact same database transaction* that creates the order. The write is 100% atomic. A background worker then polls the `outbox` table using `FOR UPDATE SKIP LOCKED`, publishes the event to the external broker, and marks the event as processed upon receiving an acknowledgment.
</details>

<details>
<summary><strong>3. Why is a partial index critical for performance on a job queue table?</strong></summary>

**Answer:**
In production job queues, millions of tasks are processed over time. If a table contains 10,000,000 historical jobs, but only 200 jobs are currently `PENDING`, a full index on `(priority, created_at)` will contain 10,000,000 entries, consuming hundreds of megabytes of RAM and slowing down index scans.

A **partial index** (`WHERE status = 'PENDING'`) only indexes the 200 pending rows. The index size remains tiny (a few kilobytes), fits entirely in CPU cache, makes worker polling queries lightning fast, and incurs zero maintenance overhead when updating completed jobs.
</details>

<details>
<summary><strong>4. What is the difference between NOWAIT and SKIP LOCKED?</strong></summary>

**Answer:**
- **`NOWAIT`**: If the query encounters a row that is currently locked by another transaction, it **aborts immediately** and raises an error (`SQLSTATE 55P03: could not obtain lock on row`). It returns no rows.
- **`SKIP LOCKED`**: If the query encounters a locked row, it **silently ignores it** and continues scanning for other matching unlocked rows. It succeeds and returns whatever unlocked rows satisfy the query up to the `LIMIT`.
</details>

<details>
<summary><strong>5. Why should a background worker NOT hold an open transaction while executing a 30-second job?</strong></summary>

**Answer:**
Holding an open database transaction while executing external work (like calling a third-party API or transcoding a video) causes severe system issues:
1. It ties up a dedicated database connection from the connection pool for 30 seconds.
2. It holds the transaction's read snapshot open, pinning the `xmin` horizon and blocking `VACUUM` from reclaiming dead tuples across the entire database.
3. If the worker encounters a network hang, the connection remains stuck in `idle in transaction`, risking lock starvation and table bloat.
Workers should dequeue and mark the job as `RUNNING` in a short 5-millisecond transaction, commit immediately, execute the long job, and update the status to `COMPLETED` in a separate short transaction.
</details>

<details>
<summary><strong>6. How does SQL Server implement the equivalent of SKIP LOCKED?</strong></summary>

**Answer:**
SQL Server implements this capability using table query hints:
```sql
SELECT TOP (1) JobID, Payload 
FROM BackgroundJobs WITH (UPDLOCK, READPAST, ROWLOCK)
WHERE Status = 'PENDING'
ORDER BY Priority DESC;
```
- `UPDLOCK`: Takes update locks on rows instead of shared locks.
- `ROWLOCK`: Forces row-level lock granularity rather than page or table locks.
- `READPAST`: Instructs SQL Server to skip past any rows or data pages locked by concurrent transactions.
</details>

<details>
<summary><strong>7. What happens if a worker using SKIP LOCKED encounters an unindexed table during high concurrency?</strong></summary>

**Answer:**
Without a suitable index, the database engine must execute a Sequential Scan to locate matching rows. 
During a sequential scan under `FOR UPDATE`, the engine evaluates locks as it reads pages. This can result in the query testing locks on thousands of rows, generating unnecessary lock manager traffic and causing workers to skip large blocks of eligible rows due to lock conflicts on intermediate scan paths, completely destroying dequeue throughput.
</details>

<details>
<summary><strong>8. In PostgreSQL, what is the difference between FOR UPDATE and FOR NO KEY UPDATE when building queues?</strong></summary>

**Answer:**
`FOR UPDATE` locks the row exclusively against all lock modes, including `FOR KEY SHARE`. 
If other tables reference the queue table via foreign keys, or if concurrent queries inspect the queue's primary key for validation, `FOR UPDATE` will block those foreign key checks. 

`FOR NO KEY UPDATE` indicates that the worker intends to update columns like `status` or `started_at` without mutating the primary key. This allows concurrent transactions to perform foreign key checks (`FOR KEY SHARE`) on that row without blocking, reducing lock contention.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1: Writing an Atomic Dequeue Query in PostgreSQL</strong></summary>

**Problem:** Table `jobs (id BIGINT PRIMARY KEY, status TEXT, priority INT, payload JSONB)`. Write a single SQL statement that dequeues the top 1 highest priority job, changes its status to 'PROCESSING', and returns the payload to the caller.

**Solution:**
```sql
WITH target_job AS (
    SELECT id
    FROM jobs
    WHERE status = 'PENDING'
    ORDER BY priority DESC, id ASC
    LIMIT 1
    FOR UPDATE SKIP LOCKED
)
UPDATE jobs
SET status = 'PROCESSING'
FROM target_job
WHERE jobs.id = target_job.id
RETURNING jobs.id, jobs.priority, jobs.payload;
```
</details>

<details>
<summary><strong>Exercise 2: Simulating Concurrent Workers with SKIP LOCKED</strong></summary>

**Problem:** Create a temporary table `tasks (id INT PRIMARY KEY, name TEXT)`. Insert 4 rows. Open two psql sessions: in Session 1, lock 2 rows with `SKIP LOCKED`. In Session 2, run the same query. Show that Session 2 receives the other 2 rows without blocking.

**Solution:**
```sql
CREATE TEMP TABLE tasks (id INT PRIMARY KEY, name TEXT);
INSERT INTO tasks VALUES (1, 'A'), (2, 'B'), (3, 'C'), (4, 'D');

-- Session 1:
BEGIN;
SELECT * FROM tasks ORDER BY id LIMIT 2 FOR UPDATE SKIP LOCKED;
-- Returns: Rows 1 and 2 (Holds locks)

-- Session 2 (Concurrently):
BEGIN;
SELECT * FROM tasks ORDER BY id LIMIT 2 FOR UPDATE SKIP LOCKED;
-- Returns: Rows 3 and 4 immediately! Zero blocking!

-- Both sessions commit:
COMMIT; -- Session 1
COMMIT; -- Session 2
```
</details>

<details>
<summary><strong>Exercise 3: Creating an Optimal Partial Index for Job Polling</strong></summary>

**Problem:** Write the PostgreSQL DDL statement to create the ideal partial index for a queue table with columns `(job_id BIGINT, status VARCHAR, priority INT, scheduled_at TIMESTAMPTZ)`.

**Solution:**
```sql
CREATE INDEX idx_jobs_ready_to_run 
ON background_jobs (priority DESC, scheduled_at ASC)
WHERE status = 'PENDING';
```
</details>

<details>
<summary><strong>Exercise 4: Handling NOWAIT Failures in PL/pgSQL</strong></summary>

**Problem:** Write a PL/pgSQL function `try_lock_room(room_num INT)` that attempts to lock a hotel room using `FOR UPDATE NOWAIT`. If locked, return `FALSE`; if successfully locked, return `TRUE`.

**Solution:**
```sql
CREATE OR REPLACE FUNCTION try_lock_room(p_room_id INT) 
RETURNS BOOLEAN LANGUAGE plpgsql AS $$
DECLARE
    v_dummy INT;
BEGIN
    SELECT room_id INTO v_dummy
    FROM hotel_rooms
    WHERE room_id = p_room_id
    FOR UPDATE NOWAIT;
    
    RETURN TRUE;
EXCEPTION
    WHEN lock_not_available THEN
        RETURN FALSE;
END;
$$;
```
</details>

<details>
<summary><strong>Exercise 5: Building a Delayed Execution Scheduler</strong></summary>

**Problem:** Write a queue dequeue query that only selects jobs whose `execute_after` timestamp is in the past, ordered by oldest scheduled time first.

**Solution:**
```sql
WITH due_tasks AS (
    SELECT task_id
    FROM scheduled_tasks
    WHERE status = 'SCHEDULED'
      AND execute_after <= CURRENT_TIMESTAMP
    ORDER BY execute_after ASC
    LIMIT 25
    FOR UPDATE SKIP LOCKED
)
UPDATE scheduled_tasks s
SET status = 'IN_FLIGHT',
    picked_up_at = CURRENT_TIMESTAMP
FROM due_tasks
WHERE s.task_id = due_tasks.task_id
RETURNING s.task_id, s.task_type, s.payload;
```
</details>

<details>
<summary><strong>Exercise 6: Measuring Queue Lag and Worker Health</strong></summary>

**Problem:** Write an administrative monitoring query that reports the total number of pending jobs, the age of the oldest pending job in seconds (queue lag), and the count of jobs currently in 'RUNNING' status.

**Solution:**
```sql
SELECT 
    COUNT(*) FILTER (WHERE status = 'PENDING') AS pending_count,
    COUNT(*) FILTER (WHERE status = 'RUNNING') AS running_count,
    COALESCE(
        EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - MIN(created_at) FILTER (WHERE status = 'PENDING'))), 
        0
    ) AS oldest_pending_lag_seconds
FROM background_jobs;
```
</details>

---

## Summary & Further Reading

- Explicit row locking provides fine-grained mutual exclusion (`FOR UPDATE`, `FOR NO KEY UPDATE`, `FOR SHARE`, `FOR KEY SHARE`).
- `NOWAIT` fails immediately with `55P03` if a target row is locked; `SKIP LOCKED` skips locked rows cleanly without blocking.
- `FOR UPDATE SKIP LOCKED` enables building high-throughput, horizontally scalable, ACID-compliant job queues and Transactional Outbox patterns directly inside the database.
- Always use partial indexes (`WHERE status = 'PENDING'`) to keep queue polling lightning-fast and prevent table bloat.
- **Next Topic**: [09-idempotency-and-retries.md](09-idempotency-and-retries.md) covers designing idempotent database transactions and safe application retry patterns under transient network failures.
