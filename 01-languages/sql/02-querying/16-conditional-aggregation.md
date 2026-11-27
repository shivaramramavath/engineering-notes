# Conditional Aggregation: FILTER, CASE in Aggregates, and Pivoting

| Metadata | Details |
| :--- | :--- |
| **Topic** | Conditional Aggregation (ANSI FILTER Clause, CASE in Aggregates, Matrix Pivoting and Unpivoting) |
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

Dashboards and analytical reports frequently require displaying multiple distinct metrics side-by-side:
- Total orders, Completed orders, and Cancelled orders.
- US Revenue vs European Revenue vs Asian Revenue.
- Percentage conversion rate (Paid Users / Total Signups).

A naive developer attempts to solve this by writing 3 or 4 separate queries and joining them together, or by executing multiple subqueries in the `SELECT` list. Each join forces the database engine to scan the base table again, multiplying I/O and CPU costs.

**Conditional Aggregation** allows a database engine to compute dozens of selective metrics across different dimensions in a **single sequential pass** over the table. 

SQL achieves this via two primary mechanisms:
1. **The ANSI Standard `FILTER (WHERE ...)` Clause** (supported in PostgreSQL and SQLite).
2. **The Universal `CASE WHEN` Expression inside Aggregates** (supported across all relational dialects, including MySQL and SQL Server).

Mastering conditional aggregation is also the key to manual **pivoting** (transforming normalized rows into cross-tabular summary columns) and **unpivoting** without relying on proprietary dialect extensions.

---

## Core Concepts

### 1. The ANSI `FILTER (WHERE ...)` Clause
Introduced in ANSI SQL:2003, the `FILTER` clause attaches directly to an aggregate function:
```sql
SELECT 
    COUNT(*) AS total_orders,
    COUNT(*) FILTER (WHERE order_status = 'COMPLETED') AS completed_orders,
    SUM(total_amount) FILTER (WHERE order_status = 'COMPLETED') AS completed_revenue
FROM orders;
```
For each scanned tuple, the engine evaluates the boolean predicate inside `FILTER`. If `TRUE`, the tuple is fed into the aggregate function's accumulator; if `FALSE` or `UNKNOWN`, the tuple is simply skipped.

### 2. The `CASE WHEN` Aggregate Idiom
In engines that do not support the `FILTER` clause (e.g., MySQL and Microsoft SQL Server), developers embed a `CASE` expression inside the aggregate function:
- **Counting Rows Conditionally**:
  ```sql
  -- Method A (Summing 1s and 0s)
  SUM(CASE WHEN status = 'PAID' THEN 1 ELSE 0 END)
  
  -- Method B (Counting Non-Nulls: ELSE omitted defaults to NULL!)
  COUNT(CASE WHEN status = 'PAID' THEN 1 END)
  ```
- **Summing Values Conditionally**:
  ```sql
  SUM(CASE WHEN status = 'PAID' THEN amount ELSE 0 END)
  ```
- **Averaging Conditionally**:
  ```sql
  -- Crucial: Do NOT put ELSE 0 in AVG! 0 will skew the average.
  AVG(CASE WHEN status = 'PAID' THEN score END) -- Unmatched rows evaluate to NULL and are ignored
  ```

### 3. Matrix Pivoting (Row-to-Column)
Pivoting transforms row values into columnar headers:
```
Raw Rows:                      Pivoted Result:
Quarter | Category    | Sales  Year | Q1_Sales | Q2_Sales | Q3_Sales | Q4_Sales
Q1      | Electronics | 100    2026 | 100      | 150      | 200      | 250
Q2      | Electronics | 150
```
Conditional aggregation is the cleanest, most portable, and most performant way to pivot data across any SQL dialect.

### 4. Unpivoting (Column-to-Row)
Unpivoting reverses the transformation, flattening multiple column metrics into normalized attribute-value rows using `CROSS JOIN LATERAL` or `VALUES`.

---

## How It Works Internally

```
               [ Incoming Row from Sequential Table Scan ]
                                    │
           ┌────────────────────────┼────────────────────────┐
           ▼                        ▼                        ▼
    [ Accumulator 1 ]        [ Accumulator 2 ]        [ Accumulator 3 ]
    Global Total             FILTER (WHERE status='PAID') FILTER (WHERE status='REFUND')
           │                        │                        │
    Always updates:          Evaluates predicate.     Evaluates predicate.
    Count += 1               If TRUE:                 If FALSE:
    Sum += amount            Count += 1               Skip tuple!
                             Sum += amount            Accumulator unchanged!
```

### Physical Efficiency:
1. **Single Table Scan**: Base data is read from disk/buffer cache exactly **once**.
2. **Zero Join Overhead**: No temporary hash tables, sort nodes, or nested loops are generated.
3. **Branch Elimination**: The CPU evaluates simple in-memory branch conditions on each tuple in register memory, achieving blazing throughput (millions of rows per second).

---

## Syntax & Usage

```sql
SELECT 
    department_id,
    COUNT(*) AS total_staff,
    -- ANSI FILTER Syntax (PostgreSQL / SQLite)
    COUNT(*) FILTER (WHERE salary > 100000) AS high_earners_count,
    AVG(salary) FILTER (WHERE employment_type = 'FULL_TIME') AS avg_ft_salary,
    -- Universal CASE Syntax (MySQL / SQL Server / Postgres)
    SUM(CASE WHEN bonus IS NOT NULL THEN bonus ELSE 0 END) AS total_bonus_paid,
    ROUND(
        (COUNT(*) FILTER (WHERE employment_type = 'REMOTE') * 100.0) / COUNT(*), 
        2
    ) AS pct_remote
FROM employees
GROUP BY department_id;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **`FILTER (WHERE ...)` Clause** | Full ANSI support (9.4+) | **Not Supported** | Supported (SQLite 3.30+) | **Not Supported** |
| **`CASE` Inside Aggregates** | Fully supported | Fully supported | Fully supported | Fully supported |
| **Native `PIVOT` Operator** | Optional `tablefunc` extension (`crosstab`) | **Not Supported** | **Not Supported** | Proprietary `PIVOT` operator |
| **Native `UNPIVOT` Operator**| Cross Join Lateral / `unnest` | Cross Join Lateral / `JSON_TABLE` | `VALUES` clause | Proprietary `UNPIVOT` operator |

---

## Practical Examples

### Example 1: Basic — Executive KPI Summary Dashboard
Compute a high-level summary of transaction volume and status breakdowns in a single table scan.

```sql
CREATE TABLE transaction_records (
    txn_id INT PRIMARY KEY,
    amount NUMERIC(10,2) NOT NULL,
    status VARCHAR(20) NOT NULL -- 'SUCCESS', 'FAILED', 'DISPUTED'
);

INSERT INTO transaction_records VALUES
(1, 150.00, 'SUCCESS'),
(2, 200.00, 'SUCCESS'),
(3, 50.00, 'FAILED'),
(4, 300.00, 'DISPUTED'),
(5, 100.00, 'SUCCESS');

SELECT 
    COUNT(*) AS total_transactions,
    SUM(amount) AS gross_volume,
    -- ANSI FILTER
    COUNT(*) FILTER (WHERE status = 'SUCCESS') AS successful_txns,
    SUM(amount) FILTER (WHERE status = 'SUCCESS') AS settled_revenue,
    -- Universal CASE
    COUNT(CASE WHEN status = 'FAILED' THEN 1 END) AS failed_txns,
    SUM(CASE WHEN status = 'DISPUTED' THEN amount ELSE 0.00 END) AS disputed_volume
FROM transaction_records;
```

```
Result:
 total_transactions | gross_volume | successful_txns | settled_revenue | failed_txns | disputed_volume 
--------------------+--------------+-----------------+-----------------+-------------+-----------------
                  5 |       800.00 |               3 |          450.00 |           1 |          300.00
(1 row)
```

---

### Example 2: Real-World — Cross-Tabular Matrix Pivoting
Transform normalized monthly regional sales into a horizontal executive matrix.

```sql
CREATE TABLE monthly_sales (
    region VARCHAR(20) NOT NULL,
    sale_month VARCHAR(7) NOT NULL, -- '2026-01', '2026-02', '2026-03'
    revenue NUMERIC(10,2) NOT NULL
);

INSERT INTO monthly_sales VALUES
('North America', '2026-01', 10000.00),
('North America', '2026-02', 12000.00),
('North America', '2026-03', 15000.00),
('Europe', '2026-01', 8000.00),
('Europe', '2026-02', 9500.00),
('Europe', '2026-03', 11000.00);

-- Pivoted Report:
SELECT 
    region,
    COALESCE(SUM(revenue) FILTER (WHERE sale_month = '2026-01'), 0.00) AS jan_revenue,
    COALESCE(SUM(revenue) FILTER (WHERE sale_month = '2026-02'), 0.00) AS feb_revenue,
    COALESCE(SUM(revenue) FILTER (WHERE sale_month = '2026-03'), 0.00) AS mar_revenue,
    SUM(revenue) AS q1_total_revenue
FROM monthly_sales
GROUP BY region
ORDER BY q1_total_revenue DESC;
```

```
Result:
    region     | jan_revenue | feb_revenue | mar_revenue | q1_total_revenue 
---------------+-------------+-------------+-------------+------------------
 North America |    10000.00 |    12000.00 |    15000.00 |         37000.00
 Europe        |     8000.00 |     9500.00 |    11000.00 |         28500.00
(2 rows)
```

---

### Example 3: Edge Case — Averaging with Conditional CASE Expressions
Demonstrating why putting `ELSE 0` inside an `AVG()` expression produces disastrously incorrect math.

```sql
CREATE TABLE student_grades (
    student_id INT PRIMARY KEY,
    subject VARCHAR(20) NOT NULL,
    score INT NOT NULL
);

INSERT INTO student_grades VALUES
(1, 'Math', 90),
(2, 'History', 80),
(3, 'Math', 100),
(4, 'History', 70);

-- GOAL: Compute average Math score (Expected: (90 + 100) / 2 = 95.0)
SELECT 
    -- WRONG: ELSE 0 treats non-math students as having scored 0 in math!
    -- Computes (90 + 0 + 100 + 0) / 4 = 47.50!
    ROUND(AVG(CASE WHEN subject = 'Math' THEN score ELSE 0 END), 2) AS wrong_avg,
    -- CORRECT 1: Omitted ELSE evaluates to NULL, which AVG skips!
    -- Computes (90 + 100) / 2 = 95.00
    ROUND(AVG(CASE WHEN subject = 'Math' THEN score END), 2) AS correct_case_avg,
    -- CORRECT 2: ANSI FILTER syntax
    ROUND(AVG(score) FILTER (WHERE subject = 'Math'), 2) AS correct_filter_avg
FROM student_grades;
```

```
Result:
 wrong_avg | correct_case_avg | correct_filter_avg 
-----------+------------------+--------------------
     47.50 |            95.00 |              95.00
(1 row)
```

---

### Example 4: Failure Mode — Multi-Pass `UNION ALL` Slowdown
A developer writes 4 queries joined together instead of conditional aggregation:

```sql
-- DANGEROUS / INEFFICEINT MULTI-PASS PATTERN:
-- Scans orders table 3 times!
SELECT 
    (SELECT COUNT(*) FROM orders WHERE status = 'PENDING') AS pending_cnt,
    (SELECT COUNT(*) FROM orders WHERE status = 'SHIPPED') AS shipped_cnt,
    (SELECT COUNT(*) FROM orders WHERE status = 'DELIVERED') AS delivered_cnt;

-- OPTIMAL SINGLE-PASS PATTERN:
-- Scans orders table exactly ONCE!
SELECT 
    COUNT(*) FILTER (WHERE status = 'PENDING') AS pending_cnt,
    COUNT(*) FILTER (WHERE status = 'SHIPPED') AS shipped_cnt,
    COUNT(*) FILTER (WHERE status = 'DELIVERED') AS delivered_cnt
FROM orders;
```

---

### Example 5: Optimized — Unpivoting Columns into Rows via `CROSS JOIN LATERAL`
Transform a denormalized table with columns `phone_home`, `phone_mobile`, `phone_work` into normalized phone records.

```sql
CREATE TABLE contact_directory (
    contact_id INT PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    phone_home VARCHAR(20) NULL,
    phone_mobile VARCHAR(20) NULL,
    phone_work VARCHAR(20) NULL
);

INSERT INTO contact_directory VALUES
(1, 'Alice', '555-0100', '555-0101', NULL),
(2, 'Bob', NULL, '555-0202', '555-0203');

-- Unpivot using CROSS JOIN LATERAL and VALUES (Portable across Postgres, MSSQL, MySQL 8+)
SELECT 
    c.contact_id,
    c.name,
    u.phone_type,
    u.phone_number
FROM contact_directory c
CROSS JOIN LATERAL (
    VALUES 
        ('HOME', c.phone_home),
        ('MOBILE', c.phone_mobile),
        ('WORK', c.phone_work)
) AS u(phone_type, phone_number)
WHERE u.phone_number IS NOT NULL;
```

```
Result:
 contact_id | name  | phone_type | phone_number 
------------+-------+------------+--------------
          1 | Alice | HOME       | 555-0100
          1 | Alice | MOBILE     | 555-0101
          2 | Bob   | MOBILE     | 555-0202
          2 | Bob   | WORK       | 555-0203
(4 rows)
```

---

## Best Practices

1. **Use `FILTER (WHERE ...)` in PostgreSQL and SQLite**: It is standard ANSI, cleaner to read, and prevents accidental null errors in averages.
2. **Never Put `ELSE 0` Inside `AVG()`**: An explicit `ELSE 0` adds unwanted zeros to the denominator; omit `ELSE` so unmatched tuples evaluate to `NULL` and are ignored.
3. **Use `COUNT(CASE WHEN ... THEN 1 END)`**: When counting conditionally in MySQL or SQL Server, omit `ELSE` so non-matching rows produce `NULL`, which `COUNT()` ignores.
4. **Prefer Conditional Aggregation over Proprietary `PIVOT`**: The `SUM(CASE ...)` pattern is standard SQL, runs identically on all engines, and avoids proprietary syntax quirks like SQL Server's `PIVOT` or Oracle's `crosstab`.
5. **Handle Empty Result Sets with `COALESCE`**: When pivoting categories that might not exist in the dataset, wrap the aggregate in `COALESCE(SUM(...), 0.00)` to display clean zeros rather than `NULL`.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | `AVG(CASE WHEN cond THEN val ELSE 0 END)` | Skews the average by counting unselected rows as zero. | Omit `ELSE` so unmatched rows evaluate to `NULL` and are skipped. |
| 2 | `COUNT(CASE WHEN cond THEN 0 ELSE 1 END)` | `COUNT(0)` counts the row because 0 is non-null! | Use `COUNT(CASE WHEN cond THEN 1 END)` (omitted ELSE is NULL). |
| 3 | Writing multiple subqueries in `SELECT` | Scans the base table $N$ times instead of once. | Combine metrics into a single table scan using conditional aggregation. |
| 4 | Attempting to use `FILTER` in MySQL or SQL Server | `FILTER` is not supported in T-SQL or MySQL 8.4; throws a syntax error. | Use `SUM(CASE WHEN ... THEN 1 ELSE 0 END)`. |
| 5 | Forgetting `COALESCE` on pivoted metric columns | Columns with zero occurrences appear as blank `NULL` instead of `0`. | Wrap with `COALESCE(SUM(...), 0)`. |
| 6 | Using `WHERE` instead of `FILTER` for single metrics | A global `WHERE` clause filters out base rows, removing them from all other aggregate columns! | Keep global `WHERE` minimal; filter specific metrics inside `FILTER`. |

---

## Performance & Security Notes

### Single-Pass vs Multiple Subquery Execution
Consider generating an invoice summary dashboard across 5,000,000 invoices:
- **Multiple Subqueries Approach**:
  ```sql
  SELECT 
      (SELECT SUM(amt) FROM inv WHERE status = 'DRAFT'),
      (SELECT SUM(amt) FROM inv WHERE status = 'SENT'),
      (SELECT SUM(amt) FROM inv WHERE status = 'PAID');
  ```
  Forces 3 independent table/index scans reading 15,000,000 tuples total.
- **Conditional Aggregation Approach**:
  ```sql
  SELECT 
      SUM(amt) FILTER (WHERE status = 'DRAFT'),
      SUM(amt) FILTER (WHERE status = 'SENT'),
      SUM(amt) FILTER (WHERE status = 'PAID')
  FROM inv;
  ```
  Reads exactly 5,000,000 tuples in a single pass. Execution time is typically 3x faster with 66% less disk buffer contention.

---

## Comparison Matrix

| Dimension | ANSI `FILTER (WHERE ...)` | `CASE` Inside Aggregate | Proprietary `PIVOT` | Multiple Subqueries |
| :--- | :--- | :--- | :--- | :--- |
| **Dialect Portability**| Postgres 9.4+, SQLite 3.30+ | Universal (All 4 engines) | SQL Server, Oracle only | Universal |
| **Passes Over Data** | Exactly 1 pass | Exactly 1 pass | Exactly 1 pass | $N$ passes |
| **Readability** | High | Medium | Medium (Cryptic syntax) | Low |
| **Average (`AVG`) Safety**| Immune to null trap | Must omit `ELSE` | N/A | Safe |
| **Performance** | Optimal | Optimal | Optimal | Worst |

---

## Interview Questions & Deep Dives

### Q1: What is the semantic difference between `COUNT(*) FILTER (WHERE status = 'PAID')` and `COUNT(CASE WHEN status = 'PAID' THEN 1 END)`?
<details>
<summary>Click to view answer</summary>

They are semantically and physically equivalent. 
- The ANSI `FILTER` clause explicitly instructs the engine to evaluate the tuple against the filter predicate before feeding it to the aggregate accumulator.
- The `CASE` expression evaluates to `1` when the condition is met and `NULL` when it is not (since an omitted `ELSE` clause defaults to `NULL`). Because `COUNT(column)` ignores `NULL` values, only rows matching the condition are counted.
</details>

### Q2: Why does `COUNT(CASE WHEN status = 'FAILED' THEN 0 END)` return the wrong count?
<details>
<summary>Click to view answer</summary>

The aggregate function `COUNT(expression)` increments its internal counter for **any non-null value**. Because integer `0` is a valid, non-null value, `COUNT(0)` increments the counter for every single row where `status = 'FAILED'`. If the developer wrote `COUNT(CASE WHEN status = 'FAILED' THEN 1 ELSE 0 END)`, it would count **every row in the table**! To count conditionally, you must ensure unmatched rows evaluate to `NULL`.
</details>

### Q3: How do you perform a pivot without using the vendor-specific `PIVOT` keyword?
<details>
<summary>Click to view answer</summary>

By combining `GROUP BY` with conditional aggregation:
1. `GROUP BY` the row identifier (e.g., `year` or `customer_id`).
2. For each desired column header, project an aggregate function with a conditional `CASE` or `FILTER` matching that header's value:
   ```sql
   SUM(sales) FILTER (WHERE region = 'US') AS us_sales,
   SUM(sales) FILTER (WHERE region = 'EU') AS eu_sales
   ```
This pattern is 100% ANSI portable across every SQL engine.
</details>

### Q4: How do you calculate a percentage metric in SQL without dividing by zero?
<details>
<summary>Click to view answer</summary>

Use `NULLIF` in the denominator:
```sql
ROUND(
    (COUNT(*) FILTER (WHERE is_converted = TRUE) * 100.0) / 
    NULLIF(COUNT(*), 0), 
    2
) AS conversion_rate
```
If `COUNT(*)` is 0, `NULLIF` transforms the denominator into `NULL`, causing the entire division to evaluate safely to `NULL` rather than crashing with a division-by-zero error.
</details>

### Q5: Can the `FILTER` clause be used with Window Functions?
<details>
<summary>Click to view answer</summary>

Yes, in PostgreSQL and SQLite:
```sql
SUM(amount) FILTER (WHERE status = 'PAID') OVER (PARTITION BY customer_id ORDER BY order_date)
```
This computes a running total that includes only paid orders while preserving all original rows in the output.
</details>

### Q6: How does the `VALUES` clause enable unpivoting in modern SQL?
<details>
<summary>Click to view answer</summary>

A `CROSS JOIN LATERAL (VALUES ('colA', colA), ('colB', colB)) AS u(col_name, col_value)` takes the columns of the parent row and generates multiple rows on the fly. It is completely portable and avoids proprietary `UNPIVOT` clauses.
</details>

### Q7: What is the execution difference between a global `WHERE` clause and a `FILTER` clause?
<details>
<summary>Click to view answer</summary>

A global `WHERE` clause eliminates base tuples before they enter any aggregation node; those rows are unavailable to all aggregate functions in the query. A `FILTER` clause allows base tuples to enter the query and participate in other aggregate calculations, applying the condition only to the specific accumulator it is attached to.
</details>

### Q8: Does MySQL 8.4 support the ANSI `FILTER (WHERE ...)` clause?
<details>
<summary>Click to view answer</summary>

No. MySQL 8.4 LTS does not support the `FILTER` clause. Developers writing queries for MySQL must use `SUM(CASE WHEN ... THEN 1 ELSE 0 END)` or `AVG(CASE WHEN ... THEN val END)`.
</details>

---

## Practice Exercises

### Exercise 1: Single-Pass Order Status Breakdown
Given `orders(order_id, customer_id, total_amount, status)`. Write a single query returning:
- Total order count
- Total revenue of completed orders
- Count of refunded orders
- Count of pending orders

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    COUNT(*) AS total_orders,
    COALESCE(SUM(total_amount) FILTER (WHERE status = 'COMPLETED'), 0.00) AS completed_revenue,
    COUNT(*) FILTER (WHERE status = 'REFUNDED') AS refunded_orders,
    COUNT(*) FILTER (WHERE status = 'PENDING') AS pending_orders
FROM orders;
```
</details>

---

### Exercise 2: Cross-Dialect CASE Fallback
Rewrite Exercise 1 using `CASE` expressions so it runs natively on MySQL 8.4 and SQL Server 2022.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    COUNT(*) AS total_orders,
    COALESCE(SUM(CASE WHEN status = 'COMPLETED' THEN total_amount ELSE 0.00 END), 0.00) AS completed_revenue,
    COUNT(CASE WHEN status = 'REFUNDED' THEN 1 END) AS refunded_orders,
    COUNT(CASE WHEN status = 'PENDING' THEN 1 END) AS pending_orders
FROM orders;
```
</details>

---

### Exercise 3: User Conversion Rate
Given `user_traffic(session_id, user_id, device_type, has_purchased BOOLEAN)`. For each `device_type`, compute the total session count, the purchased session count, and the conversion percentage (safely handling 0-session devices).

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    device_type,
    COUNT(*) AS total_sessions,
    COUNT(*) FILTER (WHERE has_purchased = TRUE) AS converted_sessions,
    ROUND(
        (COUNT(*) FILTER (WHERE has_purchased = TRUE) * 100.0) / 
        NULLIF(COUNT(*), 0), 
        2
    ) AS conversion_rate_pct
FROM user_traffic
GROUP BY device_type;
```
</details>

---

### Exercise 4: Pivot Quarterly Revenue
Given `quarterly_revenue(company_id, fiscal_year, quarter, revenue)` where `quarter` is `'Q1'`, `'Q2'`, `'Q3'`, or `'Q4'`. Write a query pivoting revenue by company and year so each quarter has its own column.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    company_id,
    fiscal_year,
    COALESCE(SUM(revenue) FILTER (WHERE quarter = 'Q1'), 0.00) AS q1_revenue,
    COALESCE(SUM(revenue) FILTER (WHERE quarter = 'Q2'), 0.00) AS q2_revenue,
    COALESCE(SUM(revenue) FILTER (WHERE quarter = 'Q3'), 0.00) AS q3_revenue,
    COALESCE(SUM(revenue) FILTER (WHERE quarter = 'Q4'), 0.00) AS q4_revenue,
    SUM(revenue) AS annual_total
FROM quarterly_revenue
GROUP BY company_id, fiscal_year
ORDER BY company_id, fiscal_year;
```
</details>

---

### Exercise 5: Unpivot Multi-Metric Columns
Given `employee_bonuses(emp_id, bonus_q1, bonus_q2, bonus_q3, bonus_q4)`. Unpivot these columns into rows showing `emp_id`, `quarter_name`, and `bonus_amount` using `CROSS JOIN LATERAL` and `VALUES`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    eb.emp_id,
    u.quarter_name,
    u.bonus_amount
FROM employee_bonuses eb
CROSS JOIN LATERAL (
    VALUES 
        ('Q1', eb.bonus_q1),
        ('Q2', eb.bonus_q2),
        ('Q3', eb.bonus_q3),
        ('Q4', eb.bonus_q4)
) AS u(quarter_name, bonus_amount)
WHERE u.bonus_amount IS NOT NULL;
```
</details>

---

### Exercise 6: Fix the Skewed Average Bug
A software engineer writes:
```sql
SELECT AVG(CASE WHEN is_premium = TRUE THEN latency_ms ELSE 0 END) 
FROM api_metrics;
```
Explain why this calculation is mathematically invalid and provide the fix.

<details>
<summary>Click to view solution</summary>

**Cause**: The `ELSE 0` clause assigns `0` to every non-premium request. The `AVG()` function sums the latencies and divides by the **total number of all requests** (both premium and free), artificially diluting and drastically lowering the reported premium latency.

**Fix**:
```sql
-- Fix with CASE (Omit ELSE so non-premium requests evaluate to NULL):
SELECT AVG(CASE WHEN is_premium = TRUE THEN latency_ms END) AS avg_premium_latency
FROM api_metrics;

-- Fix with ANSI FILTER:
SELECT AVG(latency_ms) FILTER (WHERE is_premium = TRUE) AS avg_premium_latency
FROM api_metrics;
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                           CONDITIONAL AGGREGATION CHEAT SHEET                           |
+-------------------+----------------------------+----------------------------------------+
| Goal              | ANSI Standard (Postgres)   | Universal Fallback (MySQL / MSSQL)     |
+-------------------+----------------------------+----------------------------------------+
| Count Matching    | COUNT(*) FILTER (WHERE c)  | COUNT(CASE WHEN c THEN 1 END)          |
| Sum Matching      | SUM(val) FILTER (WHERE c)  | SUM(CASE WHEN c THEN val ELSE 0 END)   |
| Average Matching  | AVG(val) FILTER (WHERE c)  | AVG(CASE WHEN c THEN val END) [NO ELSE]|
| Safe Division     | (A * 100.0) / NULLIF(B, 0) | (A * 100.0) / NULLIF(B, 0)             |
| Pivot Columns     | SUM(v) FILTER (WHERE k='X')| SUM(CASE WHEN k='X' THEN v ELSE 0 END) |
+-------------------+----------------------------+----------------------------------------+
```

- **Single Table Pass**: Eliminates multi-query `UNION ALL` or derived join overhead.
- **Average Safety Rule**: NEVER put `ELSE 0` inside an `AVG()` expression!
- **Zero Handling**: Always use `NULLIF(denominator, 0)` to prevent runtime crashes.

---

## Further Reading
- PostgreSQL 17 Documentation: *Aggregate Expressions - The FILTER Clause*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 10.9 `<aggregate function>`
- Modern SQL: *The FILTER Clause - Selective Aggregates Made Clean*
- Itzik Ben-Gan: *Pivoting and Unpivoting Data in Transact-SQL*
