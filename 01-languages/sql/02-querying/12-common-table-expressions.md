# Common Table Expressions (CTEs) & Materialization

| Metadata | Details |
| :--- | :--- |
| **Topic** | Common Table Expressions (WITH Clause), Query Modularity, Optimizer Inlining vs Materialization |
| **Dialect** | ANSI SQL:2023 / PostgreSQL 17+ (Primary), MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Database Version** | PostgreSQL 17.2, MySQL 8.4.1, SQLite 3.50.4, SQL Server 2022 CU14 |
| **Tested On** | Python 3.14 stdlib sqlite3 engine & PostgreSQL 17 execution runner |
| **Date** | 2026-11-26 |
| **Author** | Antigravity SQL Curriculum Lead |
| **Category** | Advanced Querying & Analytical SQL |

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

As analytical and business requirements expand, SQL queries naturally become complex. Traditionally, developers tackled multi-stage transformations by nesting derived tables inside `FROM` clauses. A query nested 4 or 5 levels deep becomes an unmaintainable "SQL onion": the logical flow is inverted (inner subqueries must be read from the inside out), aliases clash, and identical subqueries must be repeated verbatim.

Introduced in ANSI SQL:1999, **Common Table Expressions (CTEs)**—declared via the `WITH` clause—transform SQL from deeply nested procedural spaghetti into a clean, top-down linear pipeline. Each CTE acts as a named, temporary result set that exists solely within the execution scope of the primary statement.

Beyond sheer readability, CTEs raise critical architectural questions regarding **Query Optimization**:
- Does the engine inline the CTE into the main query tree (allowing index scans and predicate pushdown)?
- Or does the engine materialize the CTE into a temporary in-memory or on-disk spool (acting as an optimization barrier)?
- Can CTEs modify data (`INSERT/UPDATE/DELETE ... RETURNING`) to execute complex transactional workflows in a single round-trip?

Mastering CTE mechanics and materialization controls enables engineers to write modular, elegant SQL without sacrificing execution speed.

---

## Core Concepts

### 1. Basic CTE Anatomy
A CTE defines one or more auxiliary statements preceding a primary `SELECT`, `INSERT`, `UPDATE`, or `DELETE`:
```sql
WITH customer_orders AS (
    SELECT customer_id, COUNT(*) AS order_count, SUM(total_amount) AS gross_spend
    FROM orders
    GROUP BY customer_id
)
SELECT c.full_name, co.order_count, co.gross_spend
FROM customers c
JOIN customer_orders co ON c.customer_id = co.customer_id
WHERE co.gross_spend > 500.00;
```
The expression reads naturally from top to bottom, mirroring how human software engineers think about data pipelines.

### 2. Chaining Multiple CTEs
Multiple CTEs can be defined in a single `WITH` block, separated by commas. Later CTEs can reference earlier CTEs defined in the same block:
```sql
WITH 
raw_sales AS (
    SELECT * FROM sales WHERE sale_date >= '2026-01-01'
),
monthly_summary AS (
    SELECT DATE_TRUNC('month', sale_date) AS mth, SUM(amount) AS total_revenue
    FROM raw_sales
    GROUP BY DATE_TRUNC('month', sale_date)
)
SELECT mth, total_revenue FROM monthly_summary;
```

### 3. Inlining vs Materialization
- **Inlining (Subquery Flattening)**: The query optimizer treats the CTE as a macro or inline view, pasting its definition directly into the parent query tree. This allows the optimizer to push predicates down (`WHERE id = 42`), pass outer join constraints, and utilize indexes on base tables.
- **Materialization (Optimization Barrier)**: The engine executes the CTE independently, writes its entire result set into a temporary work table (`CTE Scan` node), and reads from this temporary table in subsequent nodes. Outer filters are **not** pushed down into the materialized CTE.

### 4. Explicit Materialization Controls (PostgreSQL 12+)
Prior to PostgreSQL 12, PostgreSQL *always* materialized CTEs, acting as a strict optimization barrier. Beginning with PostgreSQL 12:
- CTEs are **inlined by default** if they are referenced only once and contain no side-effects or outer joins that prevent flattening.
- Developers can explicitly control behavior:
  - `WITH cte AS MATERIALIZED (...)`: Forces the engine to compute and cache the CTE exactly once.
  - `WITH cte AS NOT MATERIALIZED (...)`: Forces the engine to inline the CTE, ensuring predicate pushdown.

### 5. Writable CTEs (Data-Modifying Statements)
PostgreSQL and SQL Server allow `INSERT`, `UPDATE`, and `DELETE` statements inside CTEs using the `RETURNING` clause (or `OUTPUT` in T-SQL). This allows chaining mutations: e.g., deleting expired orders and archiving them into an `audit_orders` table in a single atomic transaction.

---

## How It Works Internally

```
      [ Query with CTE: WITH regional_sales AS (SELECT * FROM sales) SELECT ... ]
                                           │
                                [ Query Analyzer ]
                                           │
                         Is CTE referenced multiple times?
                         Is explicit MATERIALIZED specified?
                                           │
                    ┌──────────────────────┴──────────────────────┐
                    ▼                                             ▼
       [ Inlining: Flattened Plan ]                 [ Materialized: CTE Scan ]
                    │                                             │
      CTE merged into main query tree.              Executes CTE independently.
      Predicates pushed down to base table.         Writes tuples into memory buffer.
      Utilizes indexes on 'sales'.                  Subsequent joins perform Seq Scan
      No temporary spool allocated.                 on temporary CTE buffer.
```

### Execution Node Profiles:
- **Inlined Plan**:
  ```
  Hash Join  (cost=15.20..42.10 rows=100 width=64)
    ->  Index Scan using idx_sales_date on sales  (cost=0.28..12.50)  <-- Filter pushed down!
    ->  Seq Scan on customers
  ```
- **Materialized Plan**:
  ```
  CTE Scan on regional_sales  (cost=120.00..140.00 rows=1000 width=64)
    CTE regional_sales
      ->  Seq Scan on sales  (cost=0.00..120.00 rows=5000 width=64)  <-- Scans all 5,000 rows!
  ```

---

## Syntax & Usage

### 1. Standard Chained CTEs
```sql
WITH 
active_users AS (
    SELECT user_id, email, organization_id
    FROM users
    WHERE is_active = TRUE
),
org_metrics AS (
    SELECT organization_id, COUNT(*) AS active_member_count
    FROM active_users
    GROUP BY organization_id
)
SELECT o.org_name, om.active_member_count
FROM organizations o
JOIN org_metrics om ON o.org_id = om.organization_id;
```

### 2. Explicit Materialization in PostgreSQL
```sql
-- Force caching when CTE is expensive and referenced 3 times
WITH heavy_calculation AS MATERIALIZED (
    SELECT complex_simulation_udf(param) AS metric, category_id
    FROM large_sensor_log
)
SELECT * FROM heavy_calculation WHERE category_id = 1
UNION ALL
SELECT * FROM heavy_calculation WHERE category_id = 2;
```

### 3. Writable Archival CTE (PostgreSQL)
```sql
WITH archived_rows AS (
    DELETE FROM active_sessions
    WHERE last_activity < NOW() - INTERVAL '30 days'
    RETURNING session_id, user_id, last_activity
)
INSERT INTO expired_session_history (session_id, user_id, last_activity, archived_at)
SELECT session_id, user_id, last_activity, NOW()
FROM archived_rows;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **Standard CTEs (`WITH`)** | Fully supported ANSI syntax | Supported (MySQL 8.0+) | Supported (SQLite 3.8.3+) | Fully supported ANSI syntax |
| **Explicit `[NOT] MATERIALIZED`**| Fully supported (`WITH c AS MATERIALIZED`) | **Not Supported** | Supported (SQLite 3.35+) | **Not Supported** |
| **Default Optimization** | Inlined if referenced once; Materialized if $>1$ | Inlined or Materialized based on cost | Inlined by default | Inlined by default (No materialization hint) |
| **Writable CTEs (`INSERT/UPDATE`)**| Fully supported with `RETURNING` | **Not Supported** | **Not Supported** | Supported with `OUTPUT INTO` |
| **Statement Scope** | Valid only for the immediately attached query | Valid only for the query | Valid only for the query | Must terminate preceding statement with `;`! |

---

## Practical Examples

### Example 1: Basic — Modular Aggregation vs Nested Onion
Compute the percentage contribution of each department to total corporate payroll.

```sql
CREATE TABLE company_staff (
    emp_id INT PRIMARY KEY,
    dept_name VARCHAR(50) NOT NULL,
    salary NUMERIC(10,2) NOT NULL
);

INSERT INTO company_staff VALUES
(1, 'Engineering', 120000.00),
(2, 'Engineering', 100000.00),
(3, 'Sales', 80000.00),
(4, 'HR', 60000.00);

WITH 
dept_payroll AS (
    SELECT dept_name, SUM(salary) AS dept_total
    FROM company_staff
    GROUP BY dept_name
),
total_payroll AS (
    SELECT SUM(salary) AS company_total
    FROM company_staff
)
SELECT 
    dp.dept_name,
    dp.dept_total,
    ROUND((dp.dept_total * 100.0) / tp.company_total, 2) AS pct_of_company_payroll
FROM dept_payroll dp
CROSS JOIN total_payroll tp
ORDER BY dp.dept_total DESC;
```

```
Result:
  dept_name  | dept_total | pct_of_company_payroll 
-------------+------------+------------------------
 Engineering |  220000.00 |                  61.11
 Sales       |   80000.00 |                  22.22
 HR          |   60000.00 |                  16.67
(3 rows)
```

---

### Example 2: Real-World — Chained Deduplication & Enrichment Pipeline
Ingest raw server telemetry, deduplicate logs, identify error spikes, and join against metadata.

```sql
CREATE TABLE raw_logs (
    log_id INT PRIMARY KEY,
    server_id INT NOT NULL,
    log_level VARCHAR(10) NOT NULL,
    recorded_at TIMESTAMP NOT NULL
);

INSERT INTO raw_logs VALUES
(1, 101, 'INFO', '2026-01-01 12:00:00'),
(2, 101, 'ERROR', '2026-01-01 12:00:01'),
(3, 101, 'ERROR', '2026-01-01 12:00:01'), -- Duplicate log entry
(4, 102, 'INFO', '2026-01-01 12:00:02');

WITH 
deduped_logs AS (
    SELECT server_id, log_level, recorded_at,
           ROW_NUMBER() OVER (
               PARTITION BY server_id, log_level, recorded_at 
               ORDER BY log_id
           ) AS rn
    FROM raw_logs
),
filtered_errors AS (
    SELECT server_id, COUNT(*) AS error_count
    FROM deduped_logs
    WHERE rn = 1 AND log_level = 'ERROR'
    GROUP BY server_id
)
SELECT fe.server_id, fe.error_count
FROM filtered_errors fe;
```

```
Result:
 server_id | error_count 
-----------+-------------
       101 |           1
(1 row)
```

---

### Example 3: Edge Case — Preventing Predicate Pushdown with `MATERIALIZED`
Demonstrating how `MATERIALIZED` acts as an optimization fence, preventing the engine from pushing volatile or throwing functions into unselected rows.

```sql
CREATE TABLE untrusted_inputs (
    id INT PRIMARY KEY,
    raw_str VARCHAR(20) NOT NULL
);

INSERT INTO untrusted_inputs VALUES 
(1, '100'), 
(2, '250'), 
(3, 'BAD_DATA_NOT_INT');

-- If inlined, a naive optimizer might evaluate cast(raw_str as int) on row 3 before filtering!
-- Using MATERIALIZED guarantees the regex filter executes FIRST:
WITH sanitized_data AS MATERIALIZED (
    SELECT id, raw_str
    FROM untrusted_inputs
    WHERE raw_str ~ '^[0-9]+$' -- Keep only pure numeric strings
)
SELECT id, raw_str::integer AS valid_number
FROM sanitized_data;
```

```
Result:
 id | valid_number 
----+--------------
  1 |          100
  2 |          250
(2 rows)
```

---

### Example 4: Failure Mode — Forgetting the Semicolon in SQL Server
In Microsoft SQL Server (T-SQL), the statement immediately preceding a `WITH` clause **must** terminate with a semicolon. Failing to do so throws a syntax error.

```sql
-- SQL SERVER SYNTAX ERROR:
-- Incorrect syntax near the keyword 'WITH'. If this statement is a common table expression...
SELECT 1
WITH my_cte AS (SELECT 2 AS val)
SELECT * FROM my_cte

-- CORRECT SYNTAX IN SQL SERVER:
SELECT 1;  -- Semicolon required!
WITH my_cte AS (SELECT 2 AS val)
SELECT * FROM my_cte;
```

---

### Example 5: Optimized — Writable CTE Archive and Purge
Move inactive accounts into an archive table and delete them from the production table in a single atomic statement.

```sql
CREATE TABLE active_accounts (
    account_id INT PRIMARY KEY,
    status VARCHAR(20) NOT NULL
);

CREATE TABLE archived_accounts (
    account_id INT PRIMARY KEY,
    archived_at TIMESTAMP NOT NULL
);

INSERT INTO active_accounts VALUES (1, 'ACTIVE'), (2, 'CANCELLED'), (3, 'CANCELLED');

-- Atomic Purge & Archive in PostgreSQL:
WITH purged AS (
    DELETE FROM active_accounts
    WHERE status = 'CANCELLED'
    RETURNING account_id
)
INSERT INTO archived_accounts (account_id, archived_at)
SELECT account_id, CURRENT_TIMESTAMP
FROM purged;
```

```
Verify active_accounts remaining:
 account_id | status 
------------+--------
          1 | ACTIVE
(1 row)

Verify archived_accounts inserted:
 account_id |        archived_at         
------------+----------------------------
          2 | 2026-11-26 12:00:00.000000
          3 | 2026-11-26 12:00:00.000000
(2 rows)
```

---

## Best Practices

1. **Use CTEs for Readability and Linear Structure**: Replace subqueries nested $>2$ levels deep with sequentially named CTEs.
2. **Be Mindful of CTE Reuse**: If a CTE is referenced multiple times in PostgreSQL, it is materialized by default. If it is large, use `WITH cte AS NOT MATERIALIZED` if you want filters to push down into each branch.
3. **Terminate Preceding Statements with Semicolons**: Always end previous statements with `;` to ensure cross-dialect compatibility with SQL Server.
4. **Do Not Abuse CTEs for Trivial Lookups**: Avoid wrapping simple single-table scans in CTEs when a standard `WHERE` clause is more concise.
5. **Use Writable CTEs for Atomic ETL**: When moving rows between tables, leverage `DELETE ... RETURNING` in a CTE piped into `INSERT` to guarantee atomic consistency without distributed transaction overhead.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Omitted semicolon before `WITH` in SQL Server | T-SQL parser requires the preceding statement to end with `;`. | Always terminate SQL statements with a semicolon. |
| 2 | Assuming CTEs are always cached/materialized | In PostgreSQL 12+, MySQL 8+, and SQL Server, single-reference CTEs are inlined by default. | Use `WITH cte AS MATERIALIZED` if you specifically want to force single evaluation. |
| 3 | Forcing materialization on huge unindexed tables | Writing `AS MATERIALIZED` prevents index lookups and predicate pushdown, forcing a full table scan. | Allow the query planner to inline (`NOT MATERIALIZED`) unless caching an expensive UDF. |
| 4 | Attempting to reference a later CTE in an earlier CTE | CTE definitions are evaluated sequentially; forward references raise undefined table errors. | Reorder CTE definitions so prerequisites appear earlier. |
| 5 | Trying to query a CTE outside its statement scope | A CTE exists strictly for the single query it precedes; it cannot be queried in subsequent statements. | Use a temporary table (`CREATE TEMP TABLE`) if persistence across multiple queries is needed. |
| 6 | Duplicating `WITH` keyword in chained CTEs | Writing `WITH a AS (...), WITH b AS (...)` is a syntax error. | Use a single `WITH` followed by comma-separated expressions: `WITH a AS (...), b AS (...)`. |

---

## Performance & Security Notes

### The PostgreSQL CTE Optimization Fence History
- **PostgreSQL 8.4 through 11**: CTEs were strictly materialized. The engine executed the CTE query, stored the result in a temporary tuplestore, and read from it. This acted as a strict **optimization fence**:
  ```sql
  WITH big_data AS (SELECT * FROM huge_table)
  SELECT * FROM big_data WHERE id = 123;
  ```
  In PG 11, the engine sequentially scanned the entire `huge_table`, materialized millions of rows, and then filtered by `id = 123`!
- **PostgreSQL 12+ Fix**: CTEs are inlined unless they contain volatile functions or are referenced multiple times. In PG 17, the above query uses the primary key index on `huge_table.id` automatically.
- **When to force `MATERIALIZED`**: If a CTE executes an expensive computation (e.g., calling an external ML model or cryptographic hash) and is joined 3 times, `AS MATERIALIZED` guarantees it executes only once.

---

## Comparison Matrix

| Dimension | Common Table Expression (CTE) | Derived Table (Subquery) | Temporary Table | Database View |
| :--- | :--- | :--- | :--- | :--- |
| **Scope** | Single statement | Single statement | Session / Connection | Global (Database catalog) |
| **Readability** | High (Top-down linear flow) | Low (Inside-out nested flow) | Medium (Procedural script) | High |
| **Persistence** | Discarded immediately | Discarded immediately | Until session disconnect | Permanent |
| **Indexing** | Cannot create secondary indexes | Cannot create indexes | Can create custom B-Trees | Inherits underlying indexes |
| **Recursion** | **Supported** (`WITH RECURSIVE`)| Not supported | Manual loop required | Not directly supported |
| **Statistics** | Inlined or dynamically estimated | Inlined into plan tree | Has full engine stats (ANALYZE) | Has full engine stats |

---

## Interview Questions & Deep Dives

### Q1: What changed regarding CTE optimization in PostgreSQL 12?
<details>
<summary>Click to view answer</summary>

In PostgreSQL 8.4 through 11, CTEs were unconditionally materialized and treated as optimization fences—predicates from the outer query were never pushed down into the CTE, preventing index scans. Beginning with PostgreSQL 12, the planner inlines non-recursive, side-effect-free CTEs that are referenced only once into the main query tree, allowing predicate pushdown and index utilization. Explicit keywords `AS MATERIALIZED` and `AS NOT MATERIALIZED` were added to allow developers to manually override optimizer decisions.
</details>

### Q2: What is the difference between a CTE and a Temporary Table?
<details>
<summary>Click to view answer</summary>

- **Scope & Lifetime**: A CTE exists only during the execution of the single SQL statement it is attached to. A Temporary Table persists across multiple statements for the duration of the client database session or transaction.
- **Indexing & Statistics**: You cannot create explicit indexes or run `ANALYZE` on a CTE. A temporary table can have primary keys, unique constraints, secondary B-Tree/GIN indexes, and explicit optimizer statistics.
- **Overhead**: CTEs require zero catalog management overhead, whereas temporary tables write metadata to system catalogs and generate tempdb/disk WAL overhead.
</details>

### Q3: How do writable CTEs work in PostgreSQL?
<details>
<summary>Click to view answer</summary>

In PostgreSQL, a CTE can contain data-modifying statements (`INSERT`, `UPDATE`, `DELETE`) with a `RETURNING` clause. The rows emitted by `RETURNING` can then be read by subsequent CTEs or the main query (which can itself perform an `INSERT`, `UPDATE`, or `SELECT`). All modifications within the CTE block occur within the same transaction snapshot and commit or rollback atomically.
</details>

### Q4: Why must statements preceding a CTE end with a semicolon in SQL Server?
<details>
<summary>Click to view answer</summary>

Microsoft SQL Server's T-SQL grammar requires the statement preceding a `WITH` clause to terminate with a semicolon `;`. This is because `WITH` is an ambiguous keyword: it is used for table hints (e.g., `FROM tbl WITH (NOLOCK)`) as well as Common Table Expressions. The semicolon disambiguates the grammar for the parser.
</details>

### Q5: When should you explicitly specify `WITH cte AS MATERIALIZED` in PostgreSQL?
<details>
<summary>Click to view answer</summary>

You should explicitly specify `AS MATERIALIZED` when:
1. The CTE performs an expensive, volatile calculation or calls a slow User-Defined Function (UDF) that is referenced multiple times in the outer query, and you want to ensure it executes exactly once.
2. The CTE acts as an intentional optimization barrier to prevent the planner from pushing down expressions that might cause runtime exceptions (e.g., filtering out malformed strings before casting to integers).
</details>

### Q6: Can a CTE reference a table that has the same name as the CTE itself?
<details>
<summary>Click to view answer</summary>

Inside the CTE definition, referring to the CTE's own name is only legal if `RECURSIVE` is declared. If not recursive, referencing the same name inside the CTE's `FROM` clause resolves to the underlying physical table in the database catalog. In the outer query, the CTE shadows (overrides) any physical table sharing the same name.
</details>

### Q7: Can a CTE be referenced in multiple statements?
<details>
<summary>Click to view answer</summary>

No. A CTE exists strictly within the syntactic boundary of the single statement that introduces it. Attempting to query the same CTE in a subsequent `SELECT` statement results in an "undefined table / relation does not exist" error.
</details>

### Q8: Does MySQL 8.4 support explicit `MATERIALIZED` / `NOT MATERIALIZED` hints on CTEs?
<details>
<summary>Click to view answer</summary>

No. MySQL 8.4 does not support the ANSI/PostgreSQL `AS MATERIALIZED` syntax. However, MySQL provides query-level optimizer hints such as `/*+ MERGE(cte_name) */` to force inlining and `/*+ NO_MERGE(cte_name) */` to force materialization.
</details>

---

## Practice Exercises

### Exercise 1: Clean Up a Nested Subquery
Refactor the following nested subquery into a clean, two-stage chained CTE:
```sql
SELECT customer_name, total_spent
FROM (
    SELECT c.customer_name, SUM(o.amount) AS total_spent
    FROM customers c
    JOIN orders o ON c.customer_id = o.customer_id
    GROUP BY c.customer_name
) sub
WHERE total_spent > 1000.00;
```

<details>
<summary>Click to view solution</summary>

```sql
WITH customer_spending AS (
    SELECT 
        c.customer_name, 
        SUM(o.amount) AS total_spent
    FROM customers c
    JOIN orders o ON c.customer_id = o.customer_id
    GROUP BY c.customer_name
)
SELECT customer_name, total_spent
FROM customer_spending
WHERE total_spent > 1000.00;
```
</details>

---

### Exercise 2: Multi-CTE Financial Summary
Given `invoices(invoice_id, client_id, amount, issued_date)` and `payments(payment_id, invoice_id, paid_amount)`. Using chained CTEs, calculate each client's total invoiced amount, total paid amount, and outstanding balance.

<details>
<summary>Click to view solution</summary>

```sql
WITH 
client_invoices AS (
    SELECT client_id, SUM(amount) AS total_invoiced
    FROM invoices
    GROUP BY client_id
),
client_payments AS (
    SELECT i.client_id, SUM(p.paid_amount) AS total_paid
    FROM payments p
    JOIN invoices i ON p.invoice_id = i.invoice_id
    GROUP BY i.client_id
)
SELECT 
    ci.client_id,
    ci.total_invoiced,
    COALESCE(cp.total_paid, 0.00) AS total_paid,
    ci.total_invoiced - COALESCE(cp.total_paid, 0.00) AS outstanding_balance
FROM client_invoices ci
LEFT JOIN client_payments cp ON ci.client_id = cp.client_id;
```
</details>

---

### Exercise 3: PostgreSQL Writable CTE Migration
Given `user_sessions(session_id, user_id, expires_at)` and `expired_sessions_archive(session_id, user_id, deleted_at)`. Write a single atomic PostgreSQL query using a writable CTE that deletes all sessions where `expires_at < NOW()` and inserts them into the archive table.

<details>
<summary>Click to view solution</summary>

```sql
WITH deleted_sessions AS (
    DELETE FROM user_sessions
    WHERE expires_at < NOW()
    RETURNING session_id, user_id
)
INSERT INTO expired_sessions_archive (session_id, user_id, deleted_at)
SELECT session_id, user_id, NOW()
FROM deleted_sessions;
```
</details>

---

### Exercise 4: Enforce Materialization on Volatile Calculation
Write a PostgreSQL query defining a CTE named `heavy_math` that calls `random_expensive_function(val)` on table `data_points`. Force the optimizer to materialize the result to guarantee the expensive function runs only once even when queried multiple times in a `UNION ALL`.

<details>
<summary>Click to view solution</summary>

```sql
WITH heavy_math AS MATERIALIZED (
    SELECT id, random_expensive_function(val) AS computed_metric
    FROM data_points
)
SELECT id, computed_metric FROM heavy_math WHERE computed_metric > 100
UNION ALL
SELECT id, computed_metric FROM heavy_math WHERE computed_metric < -100;
```
</details>

---

### Exercise 5: Chained CTE Deduplication
Given `raw_events(id, device_id, event_type, created_at)`. Write a CTE that assigns a `ROW_NUMBER()` partitioned by `device_id` and `event_type` ordered by `created_at DESC`, followed by a second CTE that filters to the latest event per device/type combination.

<details>
<summary>Click to view solution</summary>

```sql
WITH 
ranked_events AS (
    SELECT 
        id, 
        device_id, 
        event_type, 
        created_at,
        ROW_NUMBER() OVER (
            PARTITION BY device_id, event_type 
            ORDER BY created_at DESC, id DESC
        ) AS rn
    FROM raw_events
),
latest_events AS (
    SELECT id, device_id, event_type, created_at
    FROM ranked_events
    WHERE rn = 1
)
SELECT * FROM latest_events;
```
</details>

---

### Exercise 6: CTE Cross-Dialect Portability Bug
Explain why this query fails on SQL Server 2022 and provide the fix:
```sql
UPDATE inventory SET stock = 100 WHERE id = 1
WITH low_stock AS (SELECT * FROM inventory WHERE stock < 10)
SELECT * FROM low_stock;
```

<details>
<summary>Click to view solution</summary>

**Cause**: SQL Server requires the statement preceding a `WITH` clause to terminate with a semicolon `;` to avoid ambiguity with table hints.

**Fix**:
```sql
UPDATE inventory SET stock = 100 WHERE id = 1; -- Added semicolon
WITH low_stock AS (SELECT * FROM inventory WHERE stock < 10)
SELECT * FROM low_stock;
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                    CTE CHEAT SHEET                                      |
+-------------------+----------------------------+----------------------------------------+
| Syntax            | Engine Handling            | Best Used For                          |
+-------------------+----------------------------+----------------------------------------+
| WITH cte AS (...) | Inlined if referenced 1x   | Readability, modular query pipelines   |
| AS MATERIALIZED   | Forced temporary cache     | Expensive UDFs, optimization fences    |
| AS NOT MATERIALIZED| Forced inlining           | Pushing predicates & indexes into CTE  |
| Writable CTE      | DELETE/INSERT RETURNING    | Atomic ETL, audit archiving            |
+-------------------+----------------------------+----------------------------------------+
```

- **Execution Order**: CTEs execute top-down in the order declared; later CTEs can read earlier CTEs.
- **Scope Limit**: A CTE is destroyed immediately after its statement finishes.
- **SQL Server Semicolon**: Always put `;` before `WITH` when scripting for T-SQL.

---

## Further Reading
- PostgreSQL 17 Documentation: *WITH Queries (Common Table Expressions) & Materialization Hints*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 7.15 `<with clause>`
- Craig Kerstiens: *PostgreSQL CTEs: No Longer An Optimization Barrier in PG 12*
- Microsoft Learn: *WITH common_table_expression (Transact-SQL)*
