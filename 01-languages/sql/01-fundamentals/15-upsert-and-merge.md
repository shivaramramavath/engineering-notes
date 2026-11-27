# 15: Upsert Patterns: ON CONFLICT & MERGE

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [14 - Data Modification: INSERT, UPDATE, DELETE & RETURNING](./14-insert-update-delete.md) |
| **Target Audience** | Software Engineers, Data Engineers, Distributed Systems Architects |
| **Verification Status** | Verified against SQLite 3.50+ runner / PostgreSQL 17 baseline |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & Usage](#syntax--usage)
5. [5 Practical Progressive Examples](#5-practical-progressive-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance & Security Considerations](#performance--security-considerations)
9. [Comparison Matrix](#comparison-matrix)
10. [8 Real-World Interview Questions](#8-real-world-interview-questions)
11. [6 Graded Practice Exercises](#6-graded-practice-exercises)
12. [Summary & Quick Reference](#summary--quick-reference)
13. [Further Reading](#further-reading)

---

## 1. What & Why

In distributed systems, microservices, and asynchronous message queues (Kafka, RabbitMQ, SQS), network timeouts, retries, and out-of-order deliveries are daily realities. A consumer service receiving a payment webhook or event notification must be able to process that event multiple times without creating duplicate records or throwing fatal primary key collision errors. This property is known as **Idempotency**: $f(f(x)) = f(x)$.

Historically, developers attempted to handle upserts using an imperative "check-then-act" pattern:
```sql
-- DANGEROUS ANTI-PATTERN: Prone to concurrent race conditions!
IF NOT EXISTS (SELECT 1 FROM accounts WHERE user_id = 42) THEN
    INSERT INTO accounts ...
ELSE
    UPDATE accounts ...
END IF;
```
Under concurrent load, two application threads can execute the `SELECT` simultaneously, find zero rows, and both proceed to `INSERT`, causing one to crash with a unique constraint violation.

To solve this concurrency race condition at the storage engine level, relational databases provide atomic **Upsert (Insert or Update)** facilities:
1. **PostgreSQL & SQLite**: `INSERT ... ON CONFLICT (target) DO UPDATE / DO NOTHING`
2. **ANSI Standard & PostgreSQL 15+**: `MERGE INTO ... USING ... ON ...`
3. **MySQL**: `INSERT ... ON DUPLICATE KEY UPDATE`

Mastering atomic upsert patterns eliminates race conditions, ensures fault-tolerant ETL ingestion, and simplifies application logic.

---

## 2. Core Concepts

### 1. The Check-Then-Act Race Condition
Why application-level checking fails under concurrency:

```
Thread A                                         Thread B
   │                                                │
1. SELECT 1 FROM users WHERE id = 42;               │
   (Returns 0 rows: "User does not exist!")         │
   │                                             2. SELECT 1 FROM users WHERE id = 42;
   │                                                (Returns 0 rows: "User does not exist!")
3. INSERT INTO users (id, name) VALUES (42, 'A');   │
   (Acquires lock, writes tuple, COMMITS!)          │
   │                                             4. INSERT INTO users (id, name) VALUES (42, 'B');
   ▼                                                ▼
[Transaction A SUCCEEDS]                         [CRASH: duplicate key value violates unique constraint!]
```

### 2. Atomic Engine-Level Upsert (`ON CONFLICT`)
In an atomic `ON CONFLICT` operation, the database engine handles conflict detection within the same physical write lock on the B-Tree leaf:
- The engine attempts the insert.
- If an existing key conflicts with a specified unique constraint, it immediately pivots to update the existing row in place (or does nothing).
- Zero race condition windows exist between the check and the write.

### 3. The `EXCLUDED` Pseudo-Table
In PostgreSQL and SQLite `ON CONFLICT DO UPDATE` clauses, the special pseudo-table **`EXCLUDED`** represents the row that was *proposed for insertion*:
```sql
INSERT INTO user_scores (user_id, score)
VALUES (42, 95)
ON CONFLICT (user_id) 
DO UPDATE SET 
    score = EXCLUDED.score, -- References the 95 that we tried to insert!
    updated_at = CURRENT_TIMESTAMP;
```

---

## 3. How It Works Internally

### B-Tree Leaf Locking & Speculative Insertion

How does PostgreSQL physically execute `INSERT ... ON CONFLICT` without race conditions?

```
                    [Execute INSERT ON CONFLICT]
                                │
                                ▼
                [Speculative Insertion Phase]
        Attempts to insert new tuple into Table Heap page.
        Sets tuple header flag: HEAP_XMAX_COMMITTED
                                │
                                ▼
            [Insert into Unique B-Tree Index Leaf]
        Does key already exist in the B-Tree leaf page?
                     /                      \
                  NO                          YES
                  /                             \
        [Confirm Insertion]           [Wait for Conflicting Transaction]
    Clear speculative flag.           Is conflicting transaction active?
    Tuple is now permanent!                     /                \
    Execution Complete.                      COMMITS           ABORTS
                                               /                    \
                                     [Pivot to DO UPDATE]     [Retry Insertion]
                                     Acquire Exclusive        Re-attempt
                                     Row Lock on old tuple.   speculative insert.
                                     Mutate existing row!
```

- **Speculative Insertion**: The engine writes a speculative tuple to the heap, then attempts to insert into the unique B-Tree.
- If a conflict is detected, it does not abort. It waits for the conflicting transaction to finish:
  - If the conflicting transaction commits, it pivots to `DO UPDATE` (or drops the speculative tuple for `DO NOTHING`).
  - If the conflicting transaction rolls back, it converts its speculative tuple into a real committed row!

---

## 4. Syntax & Usage

### 1. PostgreSQL & SQLite Standard `ON CONFLICT`
```sql
-- Conflict Target must match a UNIQUE constraint or unique index!
INSERT INTO customer_metrics (customer_id, login_count, last_login_ip)
VALUES (101, 1, '192.168.1.100')
ON CONFLICT (customer_id) 
DO UPDATE SET 
    login_count = customer_metrics.login_count + 1,
    last_login_ip = EXCLUDED.last_login_ip,
    updated_at = CURRENT_TIMESTAMP
RETURNING customer_id, login_count;
```

### 2. Standard ANSI SQL:2008 `MERGE` (PostgreSQL 15+, SQL Server, Oracle)
```sql
MERGE INTO inventory AS target
USING (
    VALUES (201, 50), (202, 30)
) AS source (product_id, add_qty)
ON target.product_id = source.product_id
WHEN MATCHED THEN
    UPDATE SET stock_qty = target.stock_qty + source.add_qty
WHEN NOT MATCHED THEN
    INSERT (product_id, stock_qty) VALUES (source.product_id, source.add_qty);
```

### Multi-Dialect Comparison Matrix

| Upsert Syntax | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **`ON CONFLICT (target) DO UPDATE`** | Native standard | Not supported | Native standard | Not supported |
| **`EXCLUDED` Pseudo-Table** | Supported | Not supported (use `VALUES(col)`) | Supported | Not supported |
| **`ON CONFLICT DO NOTHING`** | Supported | `INSERT IGNORE` | Supported | Not supported |
| **`ON DUPLICATE KEY UPDATE`** | Not supported | Native standard | Not supported | Not supported |
| **ANSI `MERGE` Statement** | Full support (since PG 15)| Not supported | Not supported | Full support |
| **`RETURNING` on Upsert** | Full support | Not supported | Full support | `OUTPUT` clause |

---

## 5. 5 Practical Progressive Examples

### Example 1: Basic Idempotent Ingestion (`DO NOTHING`)
Inserting data from an event stream where duplicate events should be silently ignored:

```sql
CREATE TABLE webhook_events (
    event_id VARCHAR(64) PRIMARY KEY,
    event_type VARCHAR(50) NOT NULL,
    payload JSONB,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL
);

-- First delivery:
INSERT INTO webhook_events (event_id, event_type, payload)
VALUES ('evt_001', 'payment.success', '{"amount": 100}'::jsonb)
ON CONFLICT (event_id) DO NOTHING;
-- INSERT 0 1

-- Redelivery of same event (Network retry):
INSERT INTO webhook_events (event_id, event_type, payload)
VALUES ('evt_001', 'payment.success', '{"amount": 100}'::jsonb)
ON CONFLICT (event_id) DO NOTHING;
-- INSERT 0 0 (Silently ignored, zero error thrown!)
```

### Example 2: Atomic Increment Counter (`DO UPDATE`)
Tracking daily pageviews per URL without pre-checking if the URL row exists:

```sql
CREATE TABLE daily_pageviews (
    view_date DATE NOT NULL,
    url_path VARCHAR(200) NOT NULL,
    view_count BIGINT DEFAULT 1 NOT NULL,
    PRIMARY KEY (view_date, url_path)
);

-- Atomic increment:
INSERT INTO daily_pageviews (view_date, url_path, view_count)
VALUES (CURRENT_DATE, '/blog/sql-internals', 1)
ON CONFLICT (view_date, url_path) 
DO UPDATE SET 
    view_count = daily_pageviews.view_count + 1
RETURNING view_count;
```
```text
+------------+
| view_count |
+------------+
|          2 |
+------------+
```

### Example 3: Edge Case (Conditional Upsert with `WHERE` Clause)
Updating an existing row *only if* the incoming record is newer than the stored record (preventing out-of-order message overwrite):

```sql
CREATE TABLE device_telemetry (
    device_id VARCHAR(50) PRIMARY KEY,
    battery_level INT NOT NULL,
    temperature NUMERIC(5, 2) NOT NULL,
    reading_time TIMESTAMP WITH TIME ZONE NOT NULL
);

-- Suppose stored reading is from 10:00 AM.
-- An out-of-order packet arrives with reading_time = 09:30 AM.
INSERT INTO device_telemetry (device_id, battery_level, temperature, reading_time)
VALUES ('sensor_42', 88, 22.5, '2024-04-01 09:30:00+00')
ON CONFLICT (device_id) 
DO UPDATE SET 
    battery_level = EXCLUDED.battery_level,
    temperature = EXCLUDED.temperature,
    reading_time = EXCLUDED.reading_time
WHERE EXCLUDED.reading_time > device_telemetry.reading_time;
```
*(If the incoming reading is older, the `WHERE` clause prevents the update, preserving the freshest state).*

### Example 4: Failure Mode (Missing Conflict Target Specification)
Attempting an upsert without specifying a unique constraint:

```sql
CREATE TABLE non_unique_table (
    id INT,
    name VARCHAR(50)
);

-- FAILS: No unique constraint exists on 'id'!
INSERT INTO non_unique_table (id, name) VALUES (1, 'Alice')
ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name;
```
```text
ERROR: there is no unique or exclusion constraint matching the ON CONFLICT specification
```
*(The conflict target in `ON CONFLICT (...)` MUST correspond to an active `UNIQUE` constraint, `PRIMARY KEY`, or unique index).*

### Example 5: High-Performance Bulk Sync with ANSI `MERGE`
Synchronizing an entire batch of incoming staging records against a master product catalog:

```sql
MERGE INTO products AS target
USING staging_product_updates AS source
ON target.product_id = source.product_id
WHEN MATCHED AND source.is_deleted = TRUE THEN
    DELETE
WHEN MATCHED THEN
    UPDATE SET 
        retail_price = source.retail_price,
        stock_qty = target.stock_qty + source.added_stock
WHEN NOT MATCHED THEN
    INSERT (product_id, category_id, product_name, retail_price, stock_qty)
    VALUES (source.product_id, source.category_id, source.product_name, source.retail_price, source.added_stock);
```
*(ANSI `MERGE` can execute `UPDATE`, `INSERT`, and `DELETE` in a single unified atomic statement).*

---

## 6. Best Practices

1. **Always Use Atomic Upserts Instead of Check-Then-Act**: Never write `SELECT` followed by `INSERT` in application code. Always use `ON CONFLICT` or `MERGE` to eliminate concurrency race conditions.
2. **Ensure Conflict Targets Have Matching Unique Indexes**: `ON CONFLICT (col)` requires an exact matching unique constraint or unique index. If the index is partial (`WHERE is_active = TRUE`), the conflict target must include the identical partial predicate: `ON CONFLICT (col) WHERE is_active = TRUE`.
3. **Use the `EXCLUDED` Table for Clean Updates**: Reference `EXCLUDED.col_name` to assign the proposed incoming value rather than repeating literal values in the `DO UPDATE` clause.
4. **Guard Upserts with Timestamp Version Checks**: In event-driven architectures, protect against out-of-order message delivery by adding a `WHERE EXCLUDED.updated_at >= target.updated_at` filter to the `DO UPDATE` clause.
5. **Beware of Auto-Increment Sequence Consumption**: In PostgreSQL, a speculative insert that pivots to `DO UPDATE` or `DO NOTHING` **still consumes a sequence number** from identity/serial columns. Gaps in sequences are normal and expected.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Check-Then-Act Anti-Pattern** | `IF NOT EXISTS ... INSERT` fails under concurrent load due to race condition windows. | Use atomic `ON CONFLICT` or `MERGE`. |
| **Missing Unique Constraint on Conflict Target** | Throws `ERROR: there is no unique or exclusion constraint matching the specification`. | Create a `PRIMARY KEY` or `UNIQUE` constraint on target columns. |
| **Sequence Leaks / Gaps on Conflict** | Speculative inserts consume sequence IDs even if they pivot to `DO UPDATE` or `DO NOTHING`. | Accept sequence gaps as normal in high-concurrency systems. |
| **Assuming `DO NOTHING` Returns Generated ID** | If an existing row conflicts and `DO NOTHING` fires, `RETURNING id` returns 0 rows! | If the ID is required, use `DO UPDATE SET id = target.id RETURNING id`. |
| **MySQL `REPLACE INTO` Trap** | `REPLACE INTO` in MySQL deletes the old row and inserts a new one, breaking foreign key cascades and triggers! | Use `INSERT ... ON DUPLICATE KEY UPDATE` instead. |
| **Overwriting Fresh Data with Stale Retries** | Retried webhook deliveries can overwrite newer data with older state. | Add conditional check: `WHERE EXCLUDED.timestamp > target.timestamp`. |

---

## 8. Performance & Security Considerations

### Performance
- **Locking Overhead**: `ON CONFLICT DO UPDATE` acquires an exclusive row lock on the conflicting tuple. If hundreds of concurrent transactions attempt to upsert the *exact same row* (e.g., a single global counter), transactions queue up and throughput degrades.
- **Bulk Upserts**: Passing 1,000 rows in a single batch `INSERT ... VALUES (...), (...) ON CONFLICT ...` is 50x faster than executing 1,000 individual upsert statements.

### Security
- **Data Tampering via Upsert**:
  - Ensure that clients cannot inadvertently overwrite sensitive fields during an upsert. Only include columns intended to be mutable in the `DO UPDATE SET` clause (e.g., do not update `account_owner_id` or `created_at`).

---

## 9. Comparison Matrix

### Upsert Dialect Differences

| Feature / Behavior | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Primary Syntax** | `ON CONFLICT (...) DO UPDATE` | `ON DUPLICATE KEY UPDATE` | `ON CONFLICT (...) DO UPDATE` | `MERGE INTO ... USING` |
| **Conflict Target Specification**| Explicit column or constraint name | Inferred automatically from any PK/Unique index | Explicit column or constraint name | Defined in `ON` join predicate |
| **Proposed Row Reference** | `EXCLUDED.col_name` | `VALUES(col_name)` or `new.col` | `EXCLUDED.col_name` | `source.col_name` |
| **Conditional Update Guard** | `DO UPDATE ... WHERE ...` | Not supported | `DO UPDATE ... WHERE ...` | `WHEN MATCHED AND ...` |
| **ANSI Standard Compliance** | Full (Supports both `ON CONFLICT` and `MERGE`)| Vendor proprietary | `ON CONFLICT` subset | Full `MERGE` support |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: Why is an application-level "SELECT then INSERT or UPDATE" fundamentally broken in concurrent systems?
<details>
<summary>Answer</summary>
The "check-then-act" pattern suffers from a classic Time-of-Check to Time-of-Use (TOCTOU) race condition. 

Under concurrent execution, Thread A runs `SELECT` and finds no matching record. Before Thread A can issue its `INSERT`, the OS context-switches to Thread B, which runs the same `SELECT` and also finds no matching record. Both threads conclude the row does not exist. Both issue an `INSERT`. One thread succeeds; the other crashes with a `duplicate key value violates unique constraint` exception.

Atomic database-level upserts (`ON CONFLICT` or `MERGE`) solve this by performing conflict detection and row locking within the same atomic engine transaction.
</details>

#### Q2: What is the `EXCLUDED` pseudo-table in PostgreSQL, and how does it differ from the target table name?
<details>
<summary>Answer</summary>
In an `ON CONFLICT DO UPDATE` clause:
- The **Target Table Name** (e.g., `users.counter`) refers to the *currently existing row* already stored in the database.
- The **`EXCLUDED` Pseudo-Table** (e.g., `EXCLUDED.counter`) represents the *candidate row proposed for insertion* that triggered the conflict.

Example: `SET counter = users.counter + EXCLUDED.counter` adds the newly incoming increment value to the existing stored count.
</details>

#### Q3: Why is MySQL’s `REPLACE INTO` dangerous compared to `ON DUPLICATE KEY UPDATE`?
<details>
<summary>Answer</summary>
`REPLACE INTO` does not perform an in-place update. It physically executes a **`DELETE` of the existing conflicting row followed by an `INSERT` of the new row**:
1. If child tables reference the conflicting row with foreign keys, `REPLACE` either fails (with a foreign key constraint violation) or silently deletes all child records if `ON DELETE CASCADE` is enabled!
2. It fires `ON DELETE` triggers unexpectedly.
3. It generates a new auto-increment primary key ID if the primary key is omitted from the insert.
`ON DUPLICATE KEY UPDATE` performs a safe, in-place update of specific columns without deleting the row.
</details>

#### Q4: Why do PostgreSQL sequence numbers (Identity / Serial) leak when an `INSERT ... ON CONFLICT DO NOTHING` conflict occurs?
<details>
<summary>Answer</summary>
To achieve high concurrency, database sequences operate outside transaction isolation rollback boundaries: once a sequence value is dispensed (`nextval()`), it is never rolled back, even if the calling transaction aborts.

When `INSERT ... ON CONFLICT` runs, the engine speculatively consumes a sequence value to construct the candidate tuple. If a conflict is detected and the engine pivots to `DO NOTHING` or `DO UPDATE`, the allocated sequence number is abandoned. This is an intentional architectural trade-off to avoid blocking other concurrent transactions on sequence locks.
</details>

### Practical Upsert Puzzles

#### Q5: Write an idempotent upsert query in PostgreSQL that inserts a user session or updates `last_active_at` if the session token already exists, returning the `session_id`.
<details>
<summary>Answer</summary>

```sql
INSERT INTO user_sessions (session_token, user_id, last_active_at)
VALUES ('token_abc123', 42, CURRENT_TIMESTAMP)
ON CONFLICT (session_token) 
DO UPDATE SET 
    last_active_at = EXCLUDED.last_active_at
RETURNING session_id;
```
</details>

#### Q6: Given an out-of-order event stream, write an upsert query that updates an IoT sensor reading only if the incoming `event_timestamp` is strictly greater than the currently stored timestamp.
<details>
<summary>Answer</summary>

```sql
INSERT INTO sensor_readings (sensor_id, temperature, humidity, event_timestamp)
VALUES ('sensor_09', 24.5, 60.2, '2024-04-01 12:00:00+00')
ON CONFLICT (sensor_id) 
DO UPDATE SET 
    temperature = EXCLUDED.temperature,
    humidity = EXCLUDED.humidity,
    event_timestamp = EXCLUDED.event_timestamp
WHERE EXCLUDED.event_timestamp > sensor_readings.event_timestamp;
```
</details>

#### Q7: Write an ANSI SQL:2008 compliant `MERGE` query that synchronizes a master `accounts` table with a `staged_updates` table.
<details>
<summary>Answer</summary>

```sql
MERGE INTO accounts AS target
USING staged_updates AS source
ON target.account_id = source.account_id
WHEN MATCHED THEN
    UPDATE SET 
        balance = target.balance + source.delta_balance,
        updated_at = CURRENT_TIMESTAMP
WHEN NOT MATCHED THEN
    INSERT (account_id, balance, is_active)
    VALUES (source.account_id, source.delta_balance, TRUE);
```
</details>

#### Q8: How can you write an `ON CONFLICT DO NOTHING` statement in PostgreSQL that STILL returns the `id` of the row even if a conflict occurred?
<details>
<summary>Answer</summary>
`ON CONFLICT DO NOTHING RETURNING id` returns nothing when a conflict occurs. 
To guarantee an ID is returned, use a no-op update (`DO UPDATE SET id = table.id`):

```sql
INSERT INTO tags (tag_name)
VALUES ('database-tuning')
ON CONFLICT (tag_name) 
DO UPDATE SET tag_name = tags.tag_name -- Dummy no-op update!
RETURNING tag_id;
```
*(Because a row was technically updated, `RETURNING tag_id` emits the existing ID).*
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Basic Idempotent Insert
Insert a user setting with `user_id = 101`, `setting_key = 'theme'`, and `setting_val = 'dark'`. If the setting already exists, do nothing.
<details>
<summary>Solution</summary>

```sql
INSERT INTO user_settings (user_id, setting_key, setting_val)
VALUES (101, 'theme', 'dark')
ON CONFLICT (user_id, setting_key) DO NOTHING;
```
</details>

#### Exercise 2: Upserting Key-Value Configurations
Write an upsert statement that inserts the setting from Exercise 1, but if it already exists, updates `setting_val` to `'dark'` and updates `updated_at`.
<details>
<summary>Solution</summary>

```sql
INSERT INTO user_settings (user_id, setting_key, setting_val, updated_at)
VALUES (101, 'theme', 'dark', CURRENT_TIMESTAMP)
ON CONFLICT (user_id, setting_key) 
DO UPDATE SET 
    setting_val = EXCLUDED.setting_val,
    updated_at = EXCLUDED.updated_at;
```
</details>

### Intermediate Exercises

#### Exercise 3: Cumulative Counter Upsert
Create a table `word_frequencies (word TEXT PRIMARY KEY, frequency INT)`. Write an upsert query that inserts a word with frequency 1, or increments the existing frequency by 1.
<details>
<summary>Solution</summary>

```sql
INSERT INTO word_frequencies (word, frequency)
VALUES ('postgresql', 1)
ON CONFLICT (word) 
DO UPDATE SET 
    frequency = word_frequencies.frequency + 1
RETURNING frequency;
```
</details>

#### Exercise 4: Partial Index Conflict Target
Given a table `coupons(code TEXT, is_active BOOLEAN)` with a partial unique index on `code WHERE is_active = TRUE`, write an upsert query matching the partial constraint.
<details>
<summary>Solution</summary>

```sql
INSERT INTO coupons (code, discount_pct, is_active)
VALUES ('SUMMER2024', 15, TRUE)
ON CONFLICT (code) WHERE is_active = TRUE
DO UPDATE SET 
    discount_pct = EXCLUDED.discount_pct;
```
</details>

### Advanced Exercises

#### Exercise 5: Multi-Action ANSI `MERGE` with Deletion
Write a `MERGE` query syncing `inventory` with `staging_inventory`:
- If matched and `staging_inventory.discontinued = TRUE`, delete the row.
- If matched, update `stock_qty`.
- If not matched, insert the new product.
<details>
<summary>Solution</summary>

```sql
MERGE INTO inventory AS target
USING staging_inventory AS source
ON target.product_id = source.product_id
WHEN MATCHED AND source.discontinued = TRUE THEN
    DELETE
WHEN MATCHED THEN
    UPDATE SET stock_qty = source.stock_qty
WHEN NOT MATCHED THEN
    INSERT (product_id, product_name, stock_qty)
    VALUES (source.product_id, source.product_name, source.stock_qty);
```
</details>

#### Exercise 6: Verifying Speculative Insertion in Transactions
Write a demonstration script opening two concurrent psql sessions to prove that `ON CONFLICT` waits on an uncommitted transaction rather than failing immediately.
<details>
<summary>Solution</summary>

```sql
-- Session 1:
BEGIN;
INSERT INTO accounts (id, name) VALUES (99, 'Session 1');
-- Do NOT commit yet!

-- Session 2:
INSERT INTO accounts (id, name) VALUES (99, 'Session 2')
ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name;
-- Session 2 BLOCKS and waits for Session 1!

-- Session 1:
COMMIT;
-- Session 2 unblocks immediately and executes DO UPDATE!
```
</details>

---

## 12. Summary & Quick Reference

### Upsert Cheat Sheet
- **PostgreSQL / SQLite**:
  `INSERT INTO tbl (cols) VALUES (...) ON CONFLICT (unique_col) DO UPDATE SET col = EXCLUDED.col [WHERE cond] RETURNING id;`
- **Silent Ignore**:
  `INSERT INTO tbl (cols) VALUES (...) ON CONFLICT (unique_col) DO NOTHING;`
- **ANSI SQL Standard**:
  `MERGE INTO target USING source ON target.id = source.id WHEN MATCHED THEN UPDATE ... WHEN NOT MATCHED THEN INSERT ...;`
- **Conflict Target Rule**: Target columns in `ON CONFLICT (cols)` must have an active matching unique index or constraint.
- **Concurrency Rule**: `ON CONFLICT` provides atomic row-level locking, mathematically eliminating check-then-act race conditions.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [INSERT ON CONFLICT](https://www.postgresql.org/docs/17/sql-insert.html)
- PostgreSQL Documentation: [MERGE Statement](https://www.postgresql.org/docs/17/sql-merge.html)
- Peter Geoghegan: *"How Speculative Insertion Works in PostgreSQL"* (PostgreSQL Development)
- [16 - SQL Comments, Style & Formatting Conventions](./16-comments-style-and-formatting-conventions.md)
