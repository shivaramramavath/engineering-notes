# Denormalisation: Architectural Tradeoffs, Materialized Views, and Integrity

| Metadata | Details |
| :--- | :--- |
| **Topic** | Pragmatic Denormalisation, Read-vs-Write Asymmetry, Materialized Rollups, Cache Invalidation, Data Divergence |
| **Dialect** | ANSI SQL:2023 / PostgreSQL 17+ (Primary), MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Database Version** | PostgreSQL 17.2, MySQL 8.4.1, SQLite 3.50.4, SQL Server 2022 CU14 |
| **Tested On** | Python 3.14 stdlib sqlite3 engine & PostgreSQL 17 execution runner |
| **Date** | 2026-11-26 |
| **Author** | Antigravity SQL Curriculum Lead |
| **Category** | Database Design & Schema Architecture |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & Usage](#syntax--usage)
5. [Practical Examples](#practical-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance & Security Notes](#performance--security-notes)
9. [Comparison Matrix](#comparison-matrix)
10. [Interview Questions & Deep Dives](#interview-questions--deep-dives)
11. [Practice Exercises](#practice-exercises)
12. [Summary & Cheat Sheet](#summary--cheat-sheet)
13. [Further Reading](#further-reading)

---

## What & Why

While formal normalisation (3NF/BCNF) is the gold standard for transactional data integrity, real-world systems exhibit extreme **Read/Write Asymmetry**. In high-scale web platforms, read operations frequently outnumber write operations by ratios of 100:1 or 1,000:1 (e.g., social media feeds, catalog browsing, analytics dashboards).

In such systems, forcing every read query to join 6 or 7 normalized tables and compute on-the-fly aggregations (`COUNT(*)`, `SUM()`) can saturate database CPUs, exhaust connection pools, and exceed strict latency SLAs.

**Denormalisation** is the deliberate, strategic introduction of redundancy into a normalized schema to accelerate read performance. 

However, denormalisation is not a license to design sloppy tables. It is an architectural contract: by duplicating data or pre-computing aggregates, you trade off cheap disk space and faster reads for **increased write latency, lock amplification, and the ever-present danger of data divergence (split-brain state)**. 

Mastering pragmatic denormalisation—leveraging database triggers, Materialized Views, and asynchronous Change Data Capture (CDC)—allows engineers to achieve sub-millisecond read latency without sacrificing transactional consistency.

---

## Core Concepts

### 1. The Core Architectural Tradeoff
```
[ 3NF Normalized Schema ] ◄─────────────────────────► [ Denormalized Schema ]
• Single source of truth                              • Replicated attributes & pre-computed sums
• Zero update anomalies                               • Risk of data divergence
• Fast writes (single-row update)                     • Slower writes (multi-row updates & locks)
• Slower reads (multi-table joins & aggs)             • Blazing reads (single-table index lookups)
• Minimal buffer pool footprint                       • Larger rows, table bloat, cache dilution
```

### 2. Common Denormalisation Patterns
1. **Pre-computed Rollup Columns**: Storing `order_count` and `total_spend` directly on the `customers` table, updated via transactional triggers or background jobs.
2. **Replicated Attributes**: Copying `customer_name` or `product_title` into the `orders` or `order_items` table so that reporting queries never need to join the parent master tables.
3. **Materialized Views**: A database-managed physical table storing the pre-computed result of a complex query, refreshed on demand or on a schedule.
4. **Table Collapse (Merging 1:1 Tables)**: Merging `user_accounts` and `user_profiles` into a single table to eliminate join overhead when both sets of attributes are always read together.

### 3. Synchronization & Invalidation Strategies
When duplicated or pre-computed data exists, keeping it in sync with the primary source of truth requires a synchronization mechanism:
- **Synchronous Triggers**: A database trigger updates the summary column within the same ACID transaction. Guarantees 100% real-time consistency, but increases write latency and lock contention.
- **Materialized View Refresh**: Periodically recalculating the view (`REFRESH MATERIALIZED VIEW CONCURRENTLY`). Provides eventual consistency with zero write-path overhead.
- **Asynchronous CDC (Change Data Capture)**: Streaming database WAL logs via tools like Debezium/Kafka to asynchronously update read-optimized materialized tables.

---

## How It Works Internally

How does denormalisation impact database buffer pool memory and disk I/O?

```
                  [ Buffer Pool Memory Page (8KB) ]
                                 │
     ┌───────────────────────────┴───────────────────────────┐
     ▼                                                       ▼
[ 3NF Normalized Table ]                           [ Wide Denormalized Table ]
Row Width: 64 bytes                                Row Width: 512 bytes
Rows per 8KB page: ~125 rows                       Rows per 8KB page: ~15 rows
                                                             │
                  Table Scan Comparison (100,000 Rows):
   - 3NF Table: Scans 800 pages (6.4 MB)              - Denormalized: Scans 6,666 pages (53.3 MB)
   - Cache Eviction: Minimal                          - Cache Eviction: HIGH (Flushes hot cache!)
```

1. **Cache Dilution**: Storing replicated strings across millions of child rows bloats the average row size. Because database storage engines read data in fixed pages (typically 8KB), fewer rows fit into memory. This forces more frequent disk reads and evicts hot indexes from RAM.
2. **Write Amplification**: Updating an author's name in a normalized schema updates 1 row and 1 index page. In a denormalized schema where `author_name` is duplicated across 50,000 book rows, that single name change writes 50,000 row versions to the Write-Ahead Log (WAL), generates massive MVCC table bloat, and invalidates hundreds of index pages.

---

## Syntax & Usage

### 1. PostgreSQL Concurrent Materialized View
```sql
-- 1. Create Materialized View
CREATE MATERIALIZED VIEW mv_customer_spending_summary AS
SELECT 
    c.customer_id,
    c.email,
    COUNT(o.order_id) AS total_completed_orders,
    COALESCE(SUM(o.total_amount), 0.00) AS total_gross_spend,
    MAX(o.order_date) AS latest_order_date
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id AND o.order_status = 'COMPLETED'
GROUP BY c.customer_id, c.email;

-- 2. Create Unique Index (REQUIRED for concurrent refresh!)
CREATE UNIQUE INDEX idx_mv_customer_id ON mv_customer_spending_summary (customer_id);

-- 3. Non-blocking Background Refresh (Does NOT lock queries reading the view!)
REFRESH MATERIALIZED VIEW CONCURRENTLY mv_customer_spending_summary;
```

### 2. Transactional Pre-computed Summary via Triggers
```sql
-- Auto-update customer total_spent on order insertion
CREATE OR REPLACE FUNCTION update_customer_spend_trigger()
RETURNS TRIGGER AS $$
BEGIN
    UPDATE customers
    SET total_spent = total_spent + NEW.total_amount,
        order_count = order_count + 1
    WHERE customer_id = NEW.customer_id;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_order_inserted
AFTER INSERT ON orders
FOR EACH ROW
WHEN (NEW.order_status = 'COMPLETED')
EXECUTE FUNCTION update_customer_spend_trigger();
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **Materialized Views** | Supported natively (`REFRESH MATERIALIZED VIEW`) | **Not Supported** (Must emulate via tables + triggers) | **Not Supported** (Must emulate via tables + triggers) | Supported natively as **Indexed Views** (`WITH SCHEMABINDING`) |
| **Concurrent Refresh** | Supported (`CONCURRENTLY` with unique index) | N/A | N/A | Automatic background incremental updates! |
| **Generated Stored Columns**| Supported (`GENERATED ALWAYS AS (...) STORED`) | Supported (`STORED` and `VIRTUAL`) | Supported (`STORED` and `VIRTUAL`) | Supported (`PERSISTED` computed columns) |
| **Trigger Execution** | Row-level & Statement-level triggers | Row-level triggers | Row-level triggers | Statement-level triggers (Using `INSERTED`/`DELETED` pseudo-tables) |

---

## Practical Examples

### Example 1: Basic — Read Acceleration via Stored Computed Column
Avoid repeating expensive string concatenation or mathematical calculations on every read.

```sql
CREATE TABLE inventory_parts (
    part_id INT PRIMARY KEY,
    unit_cost NUMERIC(10,2) NOT NULL,
    tax_rate NUMERIC(4,2) NOT NULL,
    markup_multiplier NUMERIC(4,2) NOT NULL,
    -- Pre-calculated stored column: Evaluated ONCE at write time, reads are free!
    retail_price NUMERIC(10,2) GENERATED ALWAYS AS (
        ROUND((unit_cost * (1.0 + tax_rate)) * markup_multiplier, 2)
    ) STORED
);

INSERT INTO inventory_parts (part_id, unit_cost, tax_rate, markup_multiplier)
VALUES (1, 100.00, 0.08, 1.40);

SELECT part_id, retail_price FROM inventory_parts;
```

```
Result:
 part_id | retail_price 
---------+--------------
       1 |       151.20
(1 row)
```

---

### Example 2: Real-World — High-Traffic Dashboard Rollup Table
In an analytics platform processing 10,000 events per second, calculating daily active users on-the-fly causes massive query latency. Maintain an hourly summary table.

```sql
CREATE TABLE raw_telemetry_events (
    event_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_id VARCHAR(50) NOT NULL,
    event_type VARCHAR(30) NOT NULL,
    occurred_at TIMESTAMPTZ NOT NULL
);

-- Denormalized Hourly Rollup Table:
CREATE TABLE hourly_event_rollups (
    rollup_hour TIMESTAMPTZ NOT NULL,
    event_type VARCHAR(30) NOT NULL,
    total_event_count INT NOT NULL DEFAULT 0,
    unique_devices_count INT NOT NULL DEFAULT 0,
    PRIMARY KEY (rollup_hour, event_type)
);

-- Batch aggregation ETL run every hour:
INSERT INTO hourly_event_rollups (rollup_hour, event_type, total_event_count, unique_devices_count)
SELECT 
    date_trunc('hour', occurred_at) AS hr,
    event_type,
    COUNT(*) AS total_events,
    COUNT(DISTINCT device_id) AS unique_devs
FROM raw_telemetry_events
WHERE occurred_at >= NOW() - INTERVAL '1 hour'
GROUP BY date_trunc('hour', occurred_at), event_type
ON CONFLICT (rollup_hour, event_type) 
DO UPDATE SET 
    total_event_count = EXCLUDED.total_event_count,
    unique_devices_count = EXCLUDED.unique_devices_count;
```

---

### Example 3: Edge Case — Data Divergence via Out-of-Band Updates
Demonstrating how direct updates bypass synchronization mechanisms, causing silent split-brain corruption.

```sql
CREATE TABLE customers_denorm (
    customer_id INT PRIMARY KEY,
    customer_name VARCHAR(100) NOT NULL,
    total_orders INT NOT NULL DEFAULT 0 -- Denormalized counter!
);

CREATE TABLE orders_denorm (
    order_id INT PRIMARY KEY,
    customer_id INT NOT NULL REFERENCES customers_denorm(customer_id)
);

INSERT INTO customers_denorm VALUES (1, 'Alice', 1);
INSERT INTO orders_denorm VALUES (101, 1);

-- DISASTER: A DBA manually purges old orders using DELETE without updating the parent counter:
DELETE FROM orders_denorm WHERE order_id = 101;

-- Querying the database reveals divergent reality:
SELECT c.customer_id, c.total_orders, COUNT(o.order_id) AS actual_orders
FROM customers_denorm c
LEFT JOIN orders_denorm o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.total_orders;
```

```
Result:
 customer_id | total_orders | actual_orders 
-------------+--------------+---------------
           1 |            1 |             0  <-- DATA DIVERGENCE!
(1 row)
```

---

### Example 4: Failure Mode — Lock Escalation Under Synchronous Triggers
When 1,000 concurrent workers insert orders for the same customer (e.g., a corporate account), a trigger updating `customers.total_spent` forces all 1,000 workers to serialize on a single row lock!

```sql
-- High-concurrency transaction:
-- Worker A inserts order for Customer 1 -> Locks Customer 1 row
-- Worker B inserts order for Customer 1 -> BLOCKED waiting for Worker A to commit!
-- Worker C inserts order for Customer 1 -> BLOCKED!
-- Result: Massive database connection pileup, timeout crashes, and deadlocks.

-- REMEDY: Decouple synchronization!
-- Insert orders without trigger locks; aggregate totals asynchronously in a background worker
-- or Materialized View!
```

---

### Example 5: Optimized — SQL Server Indexed View (Real-Time Auto-Updating Materialization)
Microsoft SQL Server supports **Indexed Views**: views with a unique clustered index that the storage engine updates automatically and incrementally in real time as underlying tables change.

```sql
-- SQL Server 2022 Syntax:
CREATE VIEW vw_OrderSummary WITH SCHEMABINDING AS
SELECT 
    customer_id,
    COUNT_BIG(*) AS total_orders,
    SUM(total_amount) AS total_gross_spend
FROM dbo.orders
GROUP BY customer_id;
GO

-- Creating a unique clustered index physically materializes the view on disk!
CREATE UNIQUE CLUSTERED INDEX idx_vw_ordersummary ON vw_OrderSummary (customer_id);
GO

-- When queried, SQL Server reads directly from the pre-computed clustered index!
SELECT * FROM vw_OrderSummary WHERE customer_id = 42;
```

---

## Best Practices

1. **Benchmark Before Denormalizing**: Never denormalize based on intuition. Measure baseline 3NF query latency using `EXPLAIN (ANALYZE, BUFFERS)`. Only denormalize if join latency violates explicit SLA thresholds.
2. **Prefer Materialized Views Over Manual Table Columns**: Materialized views keep the primary schema clean and normalized, isolating the denormalized representation to a dedicated structure.
3. **Always Create Unique Indexes on Materialized Views**: In PostgreSQL, `REFRESH MATERIALIZED VIEW CONCURRENTLY` strictly requires a unique index on the view; without it, refreshes block all readers.
4. **Avoid Triggers on High-Concurrency Master Rows**: Updating parent balance counters via triggers creates severe row-lock serialization bottlenecks. Use append-only delta tables or background rollups.
5. **Implement Reconciliation Audits**: If you duplicate columns or maintain summary fields, schedule a nightly automated audit script to verify that `denormalized_val = actual_computed_val` and alert on drift.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Premature Denormalisation | Introduces complexity and data integrity risks before performance bottlenecks exist. | Start with strict 3NF; denormalize only after profiling production traffic. |
| 2 | Forgetting Unique Index on Materialized View | Disables `REFRESH CONCURRENTLY`; refreshing the view takes an exclusive table lock blocking all reads. | Create a unique index on the materialized view's primary key. |
| 3 | Single-Row Trigger Lock Bottlenecks | Concurrent child inserts serialize on updating the parent row lock, causing thread pool starvation. | Use asynchronous rollups or append-only ledger entries. |
| 4 | Duplicating High-Frequency Mutable Text | Replicating `user_name` into 10 tables means changing a name requires updating millions of rows across the database. | Only replicate immutable or extremely stable attributes. |
| 5 | Cache Invalidation Race Conditions | Application updates the base table and invalidates Redis/summary table out-of-order, leaving stale data. | Use database transactions, CDC, or single-source invalidation pipelines. |
| 6 | Ignoring Table Bloat & Cache Eviction | Denormalized wide rows fill buffer pool pages with redundant text, evicting critical indexes from RAM. | Limit denormalisation to numeric counters or narrow codes. |

---

## Performance & Security Notes

### Buffer Pool Eviction Under Extreme Denormalisation
When wide attributes (such as user descriptions, profile JSONs, and addresses) are duplicated into every transaction row:
- A table of 5,000,000 transactions balloons from 500 MB to 6 GB.
- PostgreSQL's `shared_buffers` (e.g., 4 GB) can no longer hold the table and its indexes in memory.
- Read operations trigger constant physical disk I/O, causing average query latency to increase by 500%—the exact opposite of the intended optimization!
- **Rule**: If an attribute is large (>100 bytes) and only needed in 10% of queries, keep it in a separate normalized table.

---

## Comparison Matrix

| Mechanism | Update Frequency | Consistency Model | Write Overhead | Read Speed |
| :--- | :--- | :--- | :--- | :--- |
| **3NF On-The-Fly Join** | Every read | 100% Real-Time ACID | Zero | Moderate ($O(\log N)$ seeks) |
| **Synchronous Trigger Rollup**| Every write | 100% Real-Time ACID | High (Row locks & WAL bloat) | Instant ($O(1)$ scalar read) |
| **PostgreSQL Materialized View**| Periodic / Scheduled | Eventual Consistency | Zero during writes; Batch I/O | Instant (Pre-computed) |
| **SQL Server Indexed View** | Real-time incremental | 100% Real-Time ACID | Moderate (Engine-managed) | Instant (Pre-computed) |
| **CDC + Read Replica** | Sub-second stream | Eventual Consistency | Zero on primary database | Instant (Read replica) |

---

## Interview Questions & Deep Dives

### Q1: When is denormalisation justified in an enterprise relational architecture?
<details>
<summary>Click to view answer</summary>

Denormalisation is justified only when:
1. The system exhibits extreme read/write asymmetry (reads heavily dominate writes).
2. Rigorous profiling (`EXPLAIN ANALYZE`) demonstrates that multi-table joins or on-the-fly aggregations represent the primary bottleneck violating application SLAs.
3. All indexing, hardware, and query tuning optimizations on the normalized schema have been exhausted.
4. A robust, automated synchronization or reconciliation mechanism exists to prevent data divergence.
</details>

### Q2: How does `REFRESH MATERIALIZED VIEW CONCURRENTLY` work in PostgreSQL?
<details>
<summary>Click to view answer</summary>

Standard `REFRESH MATERIALIZED VIEW` acquires an `ACCESS EXCLUSIVE` lock on the view, blocking all concurrent `SELECT` queries until the entire view is regenerated. 

When `CONCURRENTLY` is specified:
1. The view must possess at least one `UNIQUE` index.
2. PostgreSQL builds a temporary new version of the view.
3. It performs a diff between the old and new versions using the unique index, applying incremental `INSERT`, `UPDATE`, and `DELETE` operations.
4. Concurrent readers are never blocked and continue reading from the view without interruption.
</details>

### Q3: What is "Write Amplification" in the context of denormalisation?
<details>
<summary>Click to view answer</summary>

Write amplification refers to the phenomenon where a single logical update by an application results in multiple physical write operations across disk files and the Write-Ahead Log (WAL). If an attribute (such as `company_name`) is normalized, changing it updates 1 row. If it has been denormalized across 100,000 order records, changing the name forces the database to write 100,000 modified rows, update 100,000 index entries, and generate megabytes of WAL traffic, locking tables and saturating disk I/O.
</details>

### Q4: How does a SQL Server Indexed View differ from a PostgreSQL Materialized View?
<details>
<summary>Click to view answer</summary>

- **PostgreSQL Materialized View**: It is a snapshot in time. It is **not** automatically updated when base tables change; it must be explicitly refreshed via `REFRESH MATERIALIZED VIEW` (either manually or via a scheduled cron job).
- **SQL Server Indexed View**: It is updated **automatically, incrementally, and synchronously** by the database engine within the same transaction that modifies the underlying base tables.
</details>

### Q5: What is a "Reconciliation Job" and why is it necessary in denormalized databases?
<details>
<summary>Click to view answer</summary>

A reconciliation job is an automated background audit process that verifies whether denormalized summary fields match the true calculated values of the underlying normalized data (e.g., verifying that `customers.total_spent = SUM(orders.amount)`). It is necessary because direct manual updates, bugs in application code, trigger failures, or interrupted transactions can cause the denormalized data to drift out of sync (data divergence).
</details>

### Q6: What is the difference between a `VIRTUAL` and a `STORED` generated column?
<details>
<summary>Click to view answer</summary>

- **`VIRTUAL` (Default in MySQL/SQLite)**: The column is not written to disk; it is calculated dynamically on-the-fly whenever a query reads the row. It consumes zero disk space, but costs CPU on every read.
- **`STORED` (PostgreSQL / MySQL / SQLite)**: The column is calculated once at write time (`INSERT` or `UPDATE`) and physically stored on disk like any standard column. It consumes disk space, but reads are completely free.
</details>

### Q7: Why does adding a synchronous trigger to update summary counters degrade concurrency?
<details>
<summary>Click to view answer</summary>

When multiple concurrent database connections insert records into a child table referencing the same parent (e.g., 50 workers inserting orders for Customer 1), each trigger attempts to execute `UPDATE customers SET total = total + ... WHERE customer_id = 1`. 

Because relational databases require an exclusive row-level lock (`X-lock`) to update a row, only one worker can update Customer 1 at a time. The remaining 49 workers are forced to wait in a serialized lock queue, drastically degrading transaction throughput.
</details>

### Q8: What role does Change Data Capture (CDC) play in modern denormalisation?
<details>
<summary>Click to view answer</summary>

CDC tools (such as Debezium) tail the database Write-Ahead Log (WAL) asynchronously without locking or placing write overhead on the primary transactional database. The log stream is published to message brokers (like Apache Kafka), where microservices or stream processors consume the events and maintain denormalized read-optimized stores (such as Elasticsearch, Redis, or read replica tables) with sub-second eventual consistency.
</details>

---

## Practice Exercises

### Exercise 1: Materialized View with Concurrent Refresh
Create a materialized view named `mv_monthly_sales_summary` that aggregates `orders(order_id, customer_id, order_date, total_amount)` by `customer_id` and calendar month. Include the unique index required to support concurrent refreshing.

<details>
<summary>Click to view solution</summary>

```sql
CREATE MATERIALIZED VIEW mv_monthly_sales_summary AS
SELECT 
    customer_id,
    date_trunc('month', order_date)::date AS sale_month,
    COUNT(order_id) AS total_orders,
    SUM(total_amount) AS total_revenue
FROM orders
GROUP BY customer_id, date_trunc('month', order_date)::date;

-- Unique index on the composite primary key of the aggregation:
CREATE UNIQUE INDEX idx_mv_cust_month 
ON mv_monthly_sales_summary (customer_id, sale_month);
```
</details>

---

### Exercise 2: Stored Generated Tax Calculation
Write the DDL for an `invoices` table containing `subtotal NUMERIC(10,2)` and `tax_rate NUMERIC(4,2)`, with a stored generated column `tax_amount` and a stored generated column `total_amount` that automatically sums subtotal and tax.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE invoices (
    invoice_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    subtotal NUMERIC(10,2) NOT NULL CHECK (subtotal >= 0),
    tax_rate NUMERIC(4,2) NOT NULL CHECK (tax_rate >= 0),
    tax_amount NUMERIC(10,2) GENERATED ALWAYS AS (ROUND(subtotal * tax_rate, 2)) STORED,
    total_amount NUMERIC(10,2) GENERATED ALWAYS AS (subtotal + ROUND(subtotal * tax_rate, 2)) STORED
);
```
</details>

---

### Exercise 3: Audit Reconciliation Query
Write a reconciliation query that identifies any customer in `customers_denorm(customer_id, cached_order_count)` whose `cached_order_count` does not match the actual count of rows in `orders(order_id, customer_id)`.

<details>
<summary>Click to view solution</summary>

```sql
WITH actual_counts AS (
    SELECT customer_id, COUNT(*) AS real_count
    FROM orders
    GROUP BY customer_id
)
SELECT 
    c.customer_id,
    c.cached_order_count,
    COALESCE(ac.real_count, 0) AS actual_order_count
FROM customers_denorm c
LEFT JOIN actual_counts ac ON c.customer_id = ac.customer_id
WHERE c.cached_order_count <> COALESCE(ac.real_count, 0);
```
</details>

---

### Exercise 4: Safe Trigger Implementation for Child Delete
Write a PostgreSQL trigger function that decrements `customer_departments(dept_id, employee_count)` whenever an employee is deleted from `employees(emp_id, dept_id)`.

<details>
<summary>Click to view solution</summary>

```sql
CREATE OR REPLACE FUNCTION trg_decrement_dept_headcount()
RETURNS TRIGGER AS $$
BEGIN
    UPDATE customer_departments
    SET employee_count = employee_count - 1
    WHERE dept_id = OLD.dept_id;
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_employee_deleted
AFTER DELETE ON employees
FOR EACH ROW
EXECUTE FUNCTION trg_decrement_dept_headcount();
```
</details>

---

### Exercise 5: Compare Table Size and Page Footprint
Explain why an unindexed query scanning a denormalized table with 20 columns takes 5 times longer than scanning a normalized 3-column table containing the same number of rows.

<details>
<summary>Click to view solution</summary>

Because relational engines perform I/O in fixed 8KB pages. 
- A 3-column table has a small row width (~30 bytes), packing ~250 rows per 8KB page.
- A 20-column denormalized table with redundant text has a wide row width (~300 bytes), packing only ~25 rows per page.
Scanning the denormalized table forces the database engine to read **10 times more 8KB pages** off disk or buffer cache, creating massive I/O overhead and evicting other active tables from memory.
</details>

---

### Exercise 6: Concurrency Lock Resolution
A team reports that their payment system experiences deadlocks when updating customer balances in a synchronous trigger. Propose two alternative architectural designs to eliminate the locking bottleneck.

<details>
<summary>Click to view solution</summary>

1. **Append-Only Ledger Architecture**: Stop updating a single balance column. Instead, insert every payment as an immutable append-only ledger transaction (`ledger(account_id, amount, created_at)`). Appending new rows never locks existing rows. Balances are calculated via indexed summary rollups.
2. **Asynchronous Batch Rollup**: Insert payments without triggers. A background worker (or Kafka consumer) aggregates batches of transactions every 10 seconds and updates balances in bulk, eliminating real-time transactional lock contention.
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                DENORMALISATION CHEAT SHEET                              |
+-------------------+----------------------------+----------------------------------------+
| Pattern           | When to Use                | Risk / Disadvantage                    |
+-------------------+----------------------------+----------------------------------------+
| Stored Generated  | Immutable calculations     | Consumes disk space on row             |
| Materialized View | Heavy analytical rollups   | Eventual consistency (requires refresh)|
| Pre-computed Col  | High-frequency read metric | Lock contention during updates         |
| Append-Only Ledger| High-concurrency balances  | Requires aggregate read or rollup table|
| Table Collapse    | 1:1 entities always read   | Wider rows, reduced buffer efficiency  |
+-------------------+----------------------------+----------------------------------------+
```

- **Rule 1**: Normalize for write integrity; denormalize for proven read bottlenecks.
- **Rule 2**: Always prefer Materialized Views over manually maintained columns.
- **Rule 3**: Never use synchronous triggers to update summary counters on high-concurrency master tables.

---

## Further Reading
- PostgreSQL 17 Documentation: *Materialized Views & Concurrent Refresh*
- Martin Fowler: *Patterns of Enterprise Application Architecture (Read/Write Separation)*
- Ralph Kimball: *The Data Warehouse Toolkit (Denormalized Dimensional Modeling)*
- Kleppmann, Martin: *Designing Data-Intensive Applications (Reliability, Scalability, Maintainability)*
