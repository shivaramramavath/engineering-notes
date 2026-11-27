# Idempotency, Transient Failures & Safe Application Retries

## Metadata
| Attribute | Details |
|:---|:---|
| **Topic** | Resilient Transactions: Idempotency Keys, Error Classification & Retry Algorithms |
| **Dialect** | Standard ANSI SQL / PostgreSQL 17+ Baseline |
| **Database Version** | PostgreSQL 17+, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Tested with** | PostgreSQL 17.2, SQLite 3.50.4 |
| **Prerequisites** | `04-transactions-and-concurrency/01-acid.md`, `04-transactions-and-concurrency/06-deadlocks-and-diagnosis.md` |
| **Complexity** | Advanced |

---

## Table of Contents
1. [What & Why](#what--why)
2. [The Network Dilemma & The Two Generals Paradox](#the-network-dilemma--the-two-generals-paradox)
3. [Core Concepts of Idempotency](#core-concepts-of-idempotency)
4. [SQL Implementation Patterns for Idempotency](#sql-implementation-patterns-for-idempotency)
5. [Classifying Database Errors: Retriable vs Permanent](#classifying-database-errors-retriable-vs-permanent)
6. [Cross-Dialect Differences](#cross-dialect-differences)
7. [Practical Examples](#practical-examples)
8. [Retry Algorithms: Backoff & Full Jitter](#retry-algorithms-backoff--full-jitter)
9. [Best Practices](#best-practices)
10. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
11. [Performance & Security Notes](#performance--security-notes)
12. [Comparison Matrix: Idempotency Techniques](#comparison-matrix-idempotency-techniques)
13. [Real-World Interview Questions](#real-world-interview-questions)
14. [Practice Exercises](#practice-exercises)
15. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

In computer systems, network connections are inherently unreliable. When an application sends a `COMMIT` command across a network socket to a database server, three distinct outcomes can occur:
1. **Clear Success**: The database commits the transaction and sends an acknowledgment packet back to the client.
2. **Clear Failure**: The database encounters an error (e.g., deadlock or constraint failure), aborts the transaction, and sends an error packet back to the client.
3. **Ambiguity (The Black Hole)**: The database successfully commits the transaction and writes changes to disk, but the network switch drops the connection or the client times out *before* the acknowledgment packet arrives.

From the client application's perspective, Outcome 3 is indistinguishable from a network drop that happened *before* the database ever received the query. 

If the client application assumes the operation failed and blindly retries a payment or order insertion, the customer is billed twice, duplicate inventory is reserved, and database integrity is destroyed.

An operation is **Idempotent** if executing it multiple times produces the exact same observable state and outcome as executing it once:
$$f(f(x)) = f(x)$$
Designing idempotent database transactions and pairing them with mathematically sound retry policies is the cornerstone of building fault-tolerant production distributed systems.

---

## The Network Dilemma & The Two Generals Paradox

```
+-----------------------------------------------------------------------------------------+
|                                  THE AMBIGUOUS NETWORK DROP                             |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|   APPLICATION CLIENT                                        DATABASE SERVER             |
|   ==================                                        ===============             |
|           |                                                        |                    |
|           | 1. BEGIN; INSERT INTO payments ...; COMMIT;            |                    |
|           |------------------------------------------------------->|                    |
|           |                                                        |                    |
|           |                                                        | 2. Flushes WAL     |
|           |                                                        |    Writes to disk  |
|           |                                                        |    TRANSACTION OK  |
|           |                                                        |                    |
|           |            3. TCP ACK Packet Dropped by Network!       |                    |
|           |                     X <--------------------------------|                    |
|           |                                                        |                    |
|           | 4. Client Socket Timeout!                              |                    |
|           |    "Did the payment commit or not?!"                   |                    |
|           v                                                        v                    |
|                                                                                         |
|  Naive Retry -> Duplicate Payment Charged! (Catastrophic Bug)                           |
|  Idempotent Retry -> Recognized existing key; returns prior success! (Correct)          |
+-----------------------------------------------------------------------------------------+
```

---

## Core Concepts of Idempotency

### 1. Natural Idempotency
Certain operations in mathematics and SQL are inherently idempotent:
- `SET balance = 100.00` is naturally idempotent: running it 100 times leaves `balance = 100.00`.
- In contrast, `SET balance = balance + 100.00` is **non-idempotent**: running it twice adds \$200.00.
- `DELETE FROM users WHERE id = 42` is naturally idempotent: whether run once or ten times, user 42 is absent.

### 2. Idempotency Keys
An **Idempotency Key** is a unique, client-generated identifier (typically a UUIDv4 or UUIDv7) attached to an operation. The database enforces a `UNIQUE` constraint on this key.
- If the first attempt commits, the key is recorded in the database.
- If a subsequent retry arrives with the exact same idempotency key, the database detects the collision and either silently skips the insert (`DO NOTHING`) or returns the previously computed result without re-executing business logic.

---

## SQL Implementation Patterns for Idempotency

### Pattern 1: Dedicated Idempotency Table with Result Caching
The gold standard for enterprise payment gateways (e.g., Stripe's idempotency engine):
```sql
CREATE TABLE idempotency_records (
    idempotency_key VARCHAR(128) PRIMARY KEY,
    user_id BIGINT NOT NULL,
    request_hash VARCHAR(64) NOT NULL, -- SHA-256 of request payload
    response_status INT NOT NULL,      -- HTTP status code (e.g., 200, 201)
    response_body JSONB NOT NULL,       -- Cached JSON returned to client
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    locked_until TIMESTAMPTZ            -- For concurrent in-flight deduplication
);
```

### Pattern 2: Atomic Upsert with ON CONFLICT
Executing an operation idempotently using PostgreSQL's `INSERT ... ON CONFLICT`:
```sql
INSERT INTO user_ledger_entries (entry_id, account_id, amount, idempotency_key)
VALUES (DEFAULT, 104, 250.00, 'req_pay_9a8b7c6d5e')
ON CONFLICT (idempotency_key) DO NOTHING;
```

### Pattern 3: State Machine Invariant Transitions
Preventing duplicate state advances in order processing:
```sql
-- Idempotent transition: Only transitions if current state is strictly 'PENDING'
UPDATE customer_orders
SET status = 'PAID',
    paid_at = CURRENT_TIMESTAMP
WHERE order_id = 9021
  AND status = 'PENDING';

-- If retried: rows_affected == 0, because status is already 'PAID'!
```

---

## Classifying Database Errors: Retriable vs Permanent

When an application database driver catches an exception, it must classify the SQLSTATE error code before deciding whether to retry:

```
+-----------------------------------------------------------------------------------------+
|                                DATABASE ERROR CLASSIFICATION                            |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  [ RETRIABLE ERRORS ]                               [ PERMANENT ERRORS ]                |
|  (Transient: Safe & necessary to retry)             (Deterministic: Will NEVER succeed) |
|                                                                                         |
|  * 40P01: deadlock_detected                         * 23505: unique_violation           |
|  * 40001: serialization_failure                     * 23503: foreign_key_violation      |
|  * 57P01: admin_shutdown (failover in progress)     * 23502: not_null_violation         |
|  * 53300: too_many_connections (pool drop)         * 23514: check_violation            |
|  * 08006: connection_failure                        * 42P01: undefined_table            |
|  * 55P03: lock_not_available (NOWAIT)              * 42703: undefined_column           |
|  * Socket Hang-Up / Timeout during connect          * 22P02: invalid_text_representation|
+-----------------------------------------------------------------------------------------+
```

---

## Cross-Dialect Differences

```
+----------------------------------------------------------------------------------------------------+
| DIALECT COMPARISON: IDEMPOTENCY & RETRY PRIMITIVES                                                 |
+-------------------+--------------------------------------------------------------------------------+
| PostgreSQL 17+    | Standardized SQLSTATE codes. Full INSERT ... ON CONFLICT (key) DO NOTHING /    |
|                   | DO UPDATE support. Transactional DDL ensures failed migrations roll back clean.|
+-------------------+--------------------------------------------------------------------------------+
| MySQL 8.4 (InnoDB)| Supports INSERT ... ON DUPLICATE KEY UPDATE. Error codes are engine-specific   |
|                   | (e.g. 1213 for deadlock, 1062 for duplicate entry) alongside SQLSTATE.         |
+-------------------+--------------------------------------------------------------------------------+
| SQLite 3.50+      | Supports INSERT OR IGNORE / INSERT OR REPLACE. Returns SQLITE_BUSY /           |
|                   | SQLITE_LOCKED on write contention.                                             |
+-------------------+--------------------------------------------------------------------------------+
| SQL Server 2022   | Uses MERGE or IF NOT EXISTS ... INSERT. Uses @@ERROR numbers (e.g. 1205 for    |
|                   | deadlock, 2627 for primary key violation).                                     |
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Webhook Ingestion with Idempotency Key
Guaranteeing that duplicate webhook deliveries from Stripe or PayPal only process once:
```sql
CREATE TABLE webhook_events (
    event_id VARCHAR(128) PRIMARY KEY, -- Provided by webhook provider (e.g. evt_123abc)
    event_type VARCHAR(64) NOT NULL,
    payload JSONB NOT NULL,
    processed_at TIMESTAMPTZ,
    status VARCHAR(20) NOT NULL DEFAULT 'RECEIVED'
);

-- Ingestion query:
BEGIN;

INSERT INTO webhook_events (event_id, event_type, payload)
VALUES ('evt_3M4o9281', 'invoice.paid', '{"amount": 5000}'::jsonb)
ON CONFLICT (event_id) DO NOTHING;

-- If rows affected == 0, the event was already received! We acknowledge HTTP 200 immediately.

COMMIT;
```

### Example 2: Financial Wallet Credit with Idempotency Token
```sql
BEGIN;

-- 1. Insert transaction token; fails immediately if already attempted
INSERT INTO wallet_transactions (
    transaction_id, 
    wallet_id, 
    idempotency_token, 
    amount, 
    created_at
) VALUES (
    DEFAULT, 
    5001, 
    'tok_transfer_user88_uuid7', 
    150.00, 
    CURRENT_TIMESTAMP
)
ON CONFLICT (idempotency_token) DO NOTHING;

-- 2. Only increment wallet balance if the token was actually inserted!
UPDATE user_wallets
SET balance = balance + 150.00
WHERE wallet_id = 5001
  AND EXISTS (
      SELECT 1 FROM wallet_transactions 
      WHERE idempotency_token = 'tok_transfer_user88_uuid7' 
        AND created_at >= CURRENT_TIMESTAMP - INTERVAL '5 seconds'
  );

COMMIT;
```

### Example 3: Edge Case — Concurrent In-Flight Idempotency Race
What happens if two identical requests arrive at the exact same millisecond?
```sql
-- Solution: Atomic In-Flight Reservation
-- Insert with status = 'PROCESSING'. If conflict occurs, read existing status:
INSERT INTO idempotency_keys (idempotency_key, status, locked_at)
VALUES ('req_unique_99', 'PROCESSING', CURRENT_TIMESTAMP)
ON CONFLICT (idempotency_key) DO NOTHING;

-- In application code:
-- If insert succeeded (1 row): proceed with business logic, then update status = 'COMPLETED'.
-- If insert failed (0 rows): 
-- Query the existing row:
SELECT status, response_body 
FROM idempotency_keys 
WHERE idempotency_key = 'req_unique_99';

-- If status is 'PROCESSING', another thread is actively running! Return HTTP 409 or wait.
-- If status is 'COMPLETED', return cached response_body immediately!
```

### Example 4: Failure Mode — Non-Idempotent DDL Scripts
What happens when a deployment script fails halfway and is retried without guard rails:
```sql
-- Anti-Pattern (Fails on retry!):
ALTER TABLE users ADD COLUMN phone_number VARCHAR(32);
-- If the script crashes on the next line and is retried:
-- ERROR: column "phone_number" of relation "users" already exists

-- Idempotent Pattern (Safe for infinite retries):
ALTER TABLE users ADD COLUMN IF NOT EXISTS phone_number VARCHAR(32);
CREATE INDEX IF NOT EXISTS idx_users_phone ON users(phone_number);
```

### Example 5: Optimized Atomic Invariant Guard
Preventing overdrafts and duplicate requests simultaneously:
```sql
UPDATE checking_accounts
SET balance = balance - 75.00,
    last_processed_tx_id = 'tx_88194'
WHERE account_id = 9901
  AND balance >= 75.00
  AND (last_processed_tx_id IS DISTINCT FROM 'tx_88194');
-- If retried: last_processed_tx_id IS 'tx_88194', so 0 rows are updated!
```

---

## Retry Algorithms: Backoff & Full Jitter

When a retriable error occurs (e.g., deadlock or serialization failure), naively retrying immediately in a tight loop causes all contending threads to collide repeatedly.

The mathematically optimal retry strategy is **Exponential Backoff with Full Jitter** (formalized by AWS Architecture Labs):

### The Formula
For retry attempt $i \in \{0, 1, 2, \dots\}$:
$$\text{MaxWait} = \text{BaseDelay} \times 2^i$$
$$\text{SleepDuration} = \text{UniformRandom}(0, \min(\text{MaxDelay}, \text{MaxWait}))$$

```
Contending Sessions
   |
   +--- Retry immediately without jitter -> Periodic collisions (Retry Spikes)
   |
   +--- Retry with Full Jitter -> Uniformly distributed across time window (Smooth load)
```

### Python Implementation Reference
```python
import time, random

def execute_with_retry(db_pool, sql_operation, max_retries=5, base_delay=0.05, max_delay=2.0):
    for attempt in range(max_retries):
        try:
            with db_pool.transaction() as tx:
                return sql_operation(tx)
        except RetriableDatabaseError as err:
            if attempt == max_retries - 1:
                raise err # Exhausted retries; bubble up
            
            # Calculate Full Jitter sleep duration
            temp = min(max_delay, base_delay * (2 ** attempt))
            sleep_time = random.uniform(0, temp)
            time.sleep(sleep_time)
```

---

## Best Practices

1. **Require Idempotency Keys on All Mutating APIs**: Require client applications to generate and submit an `Idempotency-Key: <UUID>` HTTP header for all `POST` and `PATCH` endpoints.
2. **Strictly Differentiate Retriable vs Non-Retriable Errors**: Never implement a blind `catch (Exception) { retry(); }` block. Only retry verified transient errors (`40P01`, `40001`, connection resets).
3. **Set Hard Limits on Retries**: Always enforce a maximum retry count (e.g., 3 to 5 attempts) and a maximum timeout deadline.
4. **Always Combine Exponential Backoff with Jitter**: Never retry on a fixed periodic interval (e.g., every 500ms). Jitter is mandatory to desynchronize concurrent worker threads.
5. **Use `IF NOT EXISTS` in Migration Scripts**: Ensure all database migration scripts (`CREATE TABLE`, `CREATE INDEX`, `ALTER TABLE ADD COLUMN`) use `IF NOT EXISTS` clauses so failed CI/CD pipelines can be retried safely.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Customers billed twice during payment timeouts** | Retrying a non-idempotent `INSERT INTO payments` after a client socket timeout. | Require an idempotency key with a `UNIQUE` constraint or use `ON CONFLICT DO NOTHING`. |
| **Thundering herd retry spikes taking down DB** | Retrying failed transactions with a fixed sleep interval (e.g., `sleep(1s)`), causing all workers to hit the DB simultaneously. | Implement Exponential Backoff with Full Jitter. |
| **Retrying on unique constraint violations** | Treating SQLSTATE `23505` (`unique_violation`) as a retriable error. | Catch `23505` as a permanent logic failure and return an immediate error. |
| **Holding an open transaction across retry backoff** | Sleeping inside an active transaction block during backoff (`sleep()` between statements). | Roll back the transaction immediately, sleep outside any transaction, and open a brand new `BEGIN` on retry. |
| **Idempotency table growing indefinitely** | Inserting idempotency keys without a TTL or retention policy, swelling table to hundreds of millions of rows. | Set a retention window (e.g., 24-72 hours) and truncate/partition old idempotency records. |
| **Payload mutation with reused idempotency key** | A client sends the same idempotency key with completely different financial amounts. | Store SHA-256 hash of the request payload; reject request if key matches but payload hash differs. |

---

## Performance & Security Notes

### Storage and Pruning of Idempotency Records
In high-throughput architectures (e.g., 10,000 orders/second), an `idempotency_records` table accumulates ~864 million rows per day.
- **Partitioning**: Partition the table by day using `PARTITION BY RANGE (created_at)`.
- **Retention**: Drop partitions older than 72 hours via an automated cron job:
  ```sql
  DROP TABLE idempotency_records_y2026_m03_d28;
  ```
- Dropping a partition is an instantaneous metadata-only operation taking 2 milliseconds and generating zero WAL bloat, unlike running a slow, locking `DELETE WHERE created_at < NOW() - INTERVAL '3 days'`.

### Security: Idempotency Key Hijacking
An attacker could guess or forge an idempotency key belonging to another user to view their cached transaction response or cause denial of service. Always scope idempotency lookups to the authenticated tenant/user ID:
```sql
SELECT response_body 
FROM idempotency_records 
WHERE idempotency_key = :key 
  AND user_id = :authenticated_user_id;
```

---

## Comparison Matrix: Idempotency Techniques

| Pattern | Mechanism | Best Use Case | Performance Overhead |
|:---|:---|:---|:---|
| **Natural Idempotency** | Inherent property of query (`SET val = 5`) | Absolute state sets, status updates | Zero overhead |
| **Unique Idempotency Token** | `UNIQUE` constraint on business table | Direct DML insertions, ledger rows | Very low (1 B-Tree index) |
| **Dedicated Idempotency Table** | Separate table caching payload + response | External HTTP APIs, Stripe webhooks | Medium (1 extra write) |
| **Conditional State Machine** | `WHERE status = 'EXPECTED'` | Workflow phase transitions | Zero overhead |
| **Atomic Upsert** | `ON CONFLICT DO NOTHING / UPDATE` | Bulk syncs, cache pre-warming | Very low |

---

## Real-World Interview Questions

<details>
<summary><strong>1. Why is an ambiguous network timeout during a COMMIT the most dangerous scenario in transactional systems?</strong></summary>

**Answer:**
When a client sends a `COMMIT` request to the database server and the network connection drops or times out before the response arrives, the client is left in an **indeterminate state**:
1. The database may have processed and flushed the commit successfully to disk before the socket died.
2. The database may have dropped the connection or rebooted before committing, causing the transaction to abort.

If the operation was a non-idempotent balance deduction or payment charge, blindly retrying the operation will result in a double charge if the original transaction actually succeeded. Blindly failing the operation will leave the customer charged while informing them that the purchase failed. The only architectural resolution is to enforce **Idempotency Keys** so retries are safe.
</details>

<details>
<summary><strong>2. Explain why retrying transactions with Full Jitter is superior to simple exponential backoff.</strong></summary>

**Answer:**
Under standard exponential backoff without jitter, all client threads that collide at time $T$ will calculate identical wait intervals (e.g., $100\text{ms}, 200\text{ms}, 400\text{ms}$) and sleep for the exact same amount of time. When they awaken, they hit the database concurrently in synchronized waves ("retry spikes" or "thundering herds"), repeatedly causing collisions and deadlocks.

**Full Jitter** selects a random uniform duration between 0 and the exponential upper bound ($\text{UniformRandom}(0, \text{Base} \times 2^i)$). This uniformly disperses retry attempts smoothly across the entire time window, breaking the synchronization loop and dramatically increasing system throughput under high contention.
</details>

<details>
<summary><strong>3. Which PostgreSQL SQLSTATE error codes represent retriable transient failures?</strong></summary>

**Answer:**
The primary retriable SQLSTATE error codes in PostgreSQL are:
- **`40P01`** (`deadlock_detected`): The transaction was selected as a victim to break a deadlock cycle.
- **`40001`** (`serialization_failure`): A concurrent conflict occurred under `REPEATABLE READ` or `SERIALIZABLE` isolation.
- **`55P03`** (`lock_not_available`): A lock could not be obtained under `NOWAIT`.
- **`57P01`** (`admin_shutdown`): Occurs during high-availability database failovers when the primary server restarts or steps down.
- **`08006`** (`connection_failure`) / **`53300`** (`too_many_connections`): Network drops or pool saturation.
</details>

<details>
<summary><strong>4. How does the Stripe Idempotency architecture work at the database layer?</strong></summary>

**Answer:**
Stripe requires an `Idempotency-Key` HTTP header on all mutating requests:
1. When a request arrives, the application begins a database transaction and attempts to insert the idempotency key along with a hash of the request parameters into an `idempotency_keys` table.
2. If the key already exists and has `status = 'COMPLETED'`, the database returns the cached HTTP response body and status code immediately, bypassing all payment processing.
3. If the key already exists with `status = 'PROCESSING'`, another server is currently handling the request; the server returns HTTP 409 or polls until complete.
4. If the key is new, the server inserts it with `status = 'PROCESSING'`, executes the payment, writes the result to the cache, updates `status = 'COMPLETED'`, and commits.
</details>

<details>
<summary><strong>5. Why should an application never sleep or wait for backoff inside an active SQL transaction?</strong></summary>

**Answer:**
Sleeping inside an active transaction keeps database locks, connections, and snapshots open:
- It holds row and table locks, blocking other concurrent sessions and increasing system-wide deadlock frequency.
- It pins the database's `xmin` horizon, preventing `VACUUM` from reclaiming dead tuples and causing severe table bloat.
- It consumes a connection pool slot that sits idle, risking connection pool exhaustion.
An application must always issue `ROLLBACK` immediately upon error, execute the backoff sleep outside any transaction context, and issue a fresh `BEGIN` on retry.
</details>

<details>
<summary><strong>6. How can an application detect if an ON CONFLICT DO NOTHING statement actually inserted a row or was skipped?</strong></summary>

**Answer:**
By checking the **Rows Affected / Tuples Returned** count:
- In SQL, if the row was inserted, the driver reports `1` row affected (or `RETURNING` produces 1 row).
- If the row conflicted with an existing key and was skipped, the driver reports `0` rows affected (or `RETURNING` produces an empty result set).
Alternatively, using `RETURNING xmax`: if `xmax = 0`, the row was freshly inserted; if `xmax != 0`, the row already existed.
</details>

<details>
<summary><strong>7. What is the difference between an at-least-once delivery system and an exactly-once delivery system, and how does the database bridge the two?</strong></summary>

**Answer:**
- Most distributed messaging systems (Kafka, SQS, RabbitMQ) only guarantee **At-Least-Once Delivery**: messages are guaranteed not to be lost, but network retries and duplicate acknowledgments mean consumers will occasionally receive duplicate messages.
- True distributed **Exactly-Once Delivery** is theoretically impossible across independent networks without coordination.
- The relational database bridges this gap by acting as an **Idempotent Consumer**: by processing incoming messages inside an ACID transaction that checks an `idempotency_key` or event ID before mutating business state, the system achieves **effective exactly-once processing semantics** despite receiving duplicates.
</details>

<details>
<summary><strong>8. What is the "Request In-Flight" race condition in idempotency design and how is it resolved?</strong></summary>

**Answer:**
The "Request In-Flight" race occurs when a client fires Request 1, encounters a client-side timeout at 2 seconds, and fires identical Request 2 with the same idempotency key while Request 1 is *still actively executing* on the server.

Resolution: The idempotency record must have an atomic in-flight lock state:
- Request 1 inserts the key with `status = 'IN_FLIGHT'` and a lease expiration (`locked_until = NOW() + INTERVAL '30s'`).
- When Request 2 arrives, it attempts to insert or acquire the key: seeing `IN_FLIGHT` with an active lease, Request 2 rejects execution with HTTP `409 Conflict` ("Concurrent request in flight with same idempotency key").
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1: Designing an Idempotent Ledger Insertion</strong></summary>

**Problem:** Table `ledger (entry_id BIGINT PRIMARY KEY, user_id INT, amount NUMERIC, ref_token UUID UNIQUE)`. Write the SQL query that inserts a \$50 credit for user 10 idempotently using token `'a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d'`.

**Solution:**
```sql
INSERT INTO ledger (entry_id, user_id, amount, ref_token)
VALUES (DEFAULT, 10, 50.00, 'a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d')
ON CONFLICT (ref_token) DO NOTHING;
```
</details>

<details>
<summary><strong>Exercise 2: Detecting Duplicate Processing via RETURNING</strong></summary>

**Problem:** Using PostgreSQL, execute an idempotent insert and return a boolean column `is_new_record` indicating whether the row was newly created or already present.

**Solution:**
```sql
WITH ins AS (
    INSERT INTO idempotency_tokens (token, created_at)
    VALUES ('order_tok_9981', CURRENT_TIMESTAMP)
    ON CONFLICT (token) DO NOTHING
    RETURNING token
)
SELECT 
    'order_tok_9981' AS token,
    EXISTS(SELECT 1 FROM ins) AS is_new_record;
```
</details>

<details>
<summary><strong>Exercise 3: Building an Idempotent DDL Migration Script</strong></summary>

**Problem:** Write a database migration script that adds an enum type, creates a table, adds a column, and builds an index, ensuring the script can run 10 times consecutively without failing.

**Solution:**
```sql
-- 1. Idempotent type creation
DO $$ 
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_type WHERE typname = 'order_status_enum') THEN
        CREATE TYPE order_status_enum AS ENUM ('DRAFT', 'SUBMITTED', 'FULFILLED');
    END IF;
END $$;

-- 2. Idempotent table creation
CREATE TABLE IF NOT EXISTS orders (
    order_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    status order_status_enum NOT NULL DEFAULT 'DRAFT'
);

-- 3. Idempotent column addition
ALTER TABLE orders ADD COLUMN IF NOT EXISTS tracking_code VARCHAR(64);

-- 4. Idempotent index creation
CREATE INDEX IF NOT EXISTS idx_orders_tracking ON orders(tracking_code);
```
</details>

<details>
<summary><strong>Exercise 4: Idempotent State Machine Update</strong></summary>

**Problem:** Table `shipments (id INT PRIMARY KEY, status TEXT)`. Write an idempotent query to mark shipment 400 as 'DELIVERED', ensuring that if it is already 'DELIVERED', the update is a no-op that reports 0 rows changed.

**Solution:**
```sql
UPDATE shipments
SET status = 'DELIVERED',
    delivered_at = CURRENT_TIMESTAMP
WHERE id = 400
  AND status = 'IN_TRANSIT';
```
</details>

<details>
<summary><strong>Exercise 5: Implementing Full Jitter Formula in SQL</strong></summary>

**Problem:** Write a PostgreSQL query that calculates the randomized sleep duration in seconds for attempt number 3, with base delay 0.1s and max delay 2.0s, using the Full Jitter formula.

**Solution:**
```sql
SELECT random() * LEAST(2.0, 0.1 * power(2, 3)) AS jittered_sleep_seconds;
-- power(2,3) = 8; 0.1 * 8 = 0.8; returns uniform random between 0.0 and 0.8 seconds!
```
</details>

<details>
<summary><strong>Exercise 6: Idempotency Table Cleanup Strategy</strong></summary>

**Problem:** Write the SQL query to prune all idempotency records older than 48 hours without holding table locks that block active writers.

**Solution:**
```sql
-- Batch deletion pattern to avoid long table locks:
WITH deleted_batch AS (
    SELECT idempotency_key
    FROM idempotency_records
    WHERE created_at < CURRENT_TIMESTAMP - INTERVAL '48 hours'
    LIMIT 5000
)
DELETE FROM idempotency_records
WHERE idempotency_key IN (SELECT idempotency_key FROM deleted_batch);
```
</details>

---

## Summary & Further Reading

- Network ambiguity makes transient timeouts inevitable; without idempotency, retrying operations causes catastrophic duplicate state corruption.
- An operation is idempotent if executing it multiple times produces the exact same state as executing it once ($f(f(x)) = f(x)$).
- Idempotency is implemented via client-supplied **Idempotency Keys**, `UNIQUE` constraints, `ON CONFLICT DO NOTHING`, and conditional state transitions.
- Errors must be classified: retrying permanent errors (`23505`, syntax) is futile; retrying transient errors (`40P01`, `40001`, timeouts) requires **Exponential Backoff with Full Jitter**.
- **Next Topic**: [10-long-transactions-and-bloat.md](10-long-transactions-and-bloat.md) examines the devastating systemic consequences of long-running transactions, snapshot holding, table bloat, and transaction ID wraparound.
