# Multi-Dimensional Grouping: GROUPING SETS, ROLLUP, and CUBE

| Metadata | Details |
| :--- | :--- |
| **Topic** | Multi-Dimensional Aggregation, GROUPING SETS, ROLLUP, CUBE, and the GROUPING() Function |
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

In data warehousing, executive reporting, and financial analytics, users rarely view data at only a single level of aggregation. A sales dashboard requires numbers broken down simultaneously:
- By Region and Product Category
- By Region subtotal
- By Product Category subtotal
- The Grand Total across all regions and categories

Before the introduction of multi-dimensional SQL operators in ANSI SQL:1999, developers produced these composite reports by writing 4 or 5 separate `SELECT ... GROUP BY` queries and concatenating them using `UNION ALL`. This brute-force technique suffers from a crippling flaw: the database engine must scan, filter, and aggregate the underlying base table *once for every single `UNION ALL` branch*, multiplying I/O and CPU costs.

SQL provides three powerful multi-dimensional aggregation primitives: `GROUPING SETS`, `ROLLUP`, and `CUBE`. They allow the database engine to generate multiple hierarchical or cross-tabular aggregations in a **single pass** over the underlying data. Coupled with the `GROUPING()` function to distinguish generated subtotal rows from genuine `NULL` values, multi-dimensional grouping forms the bedrock of modern SQL reporting and OLAP cubes.

---

## Core Concepts

### 1. `GROUPING SETS`
`GROUPING SETS` is the foundational primitive. It allows you to specify the exact combinations of grouping columns you wish to compute:
```sql
GROUP BY GROUPING SETS (
    (region, category),
    (region),
    (category),
    () -- The empty tuple generates the Grand Total
)
```
Each grouping set generates rows aggregated strictly by the columns listed in its tuple. Columns not participating in a particular grouping set evaluate to `NULL` in the output row.

### 2. `ROLLUP` (Hierarchical Aggregation)
`ROLLUP` creates a progressive hierarchy of subtotals and a grand total. Given $N$ columns, `ROLLUP (c1, c2, c3)` generates exactly $N + 1$ grouping sets:
```sql
ROLLUP (c1, c2, c3)
-- Equates to:
GROUPING SETS (
    (c1, c2, c3),
    (c1, c2),
    (c1),
    ()
)
```
`ROLLUP` is ideally suited for natural hierarchies: `(year, quarter, month)`, `(country, state, city)`, or `(division, department, team)`. Order matters: `ROLLUP (a, b)` is **not** the same as `ROLLUP (b, a)`.

### 3. `CUBE` (Cross-Dimensional Power Set)
`CUBE` generates all possible combinations (the mathematical power set) of the specified columns. Given $N$ columns, `CUBE (c1, c2, c3)` generates $2^N$ grouping sets:
```sql
CUBE (c1, c2)
-- Equates to:
GROUPING SETS (
    (c1, c2),
    (c1),
    (c2),
    ()
)
```
`CUBE` is designed for cross-dimensional analysis where dimensions are independent rather than hierarchical (e.g., analyzing sales across `(device_type, customer_tier)`).

### 4. Disambiguating NULLs: The `GROUPING()` Function
When an aggregate row is produced for a subtotal, non-participating columns are populated with `NULL`. However, what if a column already contains genuine `NULL` values in the raw table (e.g., an unassigned department)?
- The standard `GROUPING(column)` function resolves this ambiguity:
  - Returns `0` if the column value in the current row is a genuine value from a participating group.
  - Returns `1` if the column value is `NULL` because the row represents a subtotal/grand total aggregate.
- In PostgreSQL and SQL Server, `GROUPING(c1, c2)` or `GROUPING_ID(c1, c2)` returns a bitmask integer representing the active grouping set.

---

## How It Works Internally

When a database engine processes a query containing `GROUPING SETS`, `ROLLUP`, or `CUBE`, it avoids scanning the base table multiple times.

```
                  [ Base Table: 10,000,000 Sales Rows ]
                                     │
                             (Single Table Scan)
                                     │
                             [ Hash or Sort ]
                                     │
                 ┌───────────────────┴───────────────────┐
                 ▼                                       ▼
        [ PostgreSQL Mixed Planner ]           [ SQL Server / Oracle ]
                 │                                       │
      Sorts once by (c1, c2).                 Sorts by finest grain (c1, c2).
      Rolls up accumulators in memory.        Rolls up intermediate aggregate
      Emits (c1, c2) rows.                    buffers into coarser groups:
      Sums intermediate buckets               (c1, c2) -> (c1) -> ()
      for (c1) and ().                                   │
                 │                                       │
                 └───────────────────┬───────────────────┘
                                     ▼
                     [ Emits Combined Multi-Grain Rows ]
```

1. **Sort/Hash Minimization**: The engine determines the finest granularity required (e.g., `(c1, c2)`). It reads the base table once and performs a single pass aggregation at this finest grain.
2. **Intermediate Accumulator Rollup**: Rather than re-reading the raw rows to compute coarser subtotals, the engine computes the coarser groups (`(c1)` and `()`) by aggregating the already-summarized intermediate records.
3. **Execution Nodes**:
   - In PostgreSQL, the plan displays a `MixedAggregate` or `GroupAggregate` node with multiple `Grouping Sets` listed under it.
   - In SQL Server, it displays a `Stream Aggregate` or `Hash Match (Aggregate)` operating over a table spool or compute scalar operator.

---

## Syntax & Usage

### 1. `GROUPING SETS` Syntax
```sql
SELECT 
    region, 
    product_category, 
    SUM(sale_amount) AS total_sales
FROM sales
GROUP BY GROUPING SETS (
    (region, product_category),
    (region),
    ()
);
```

### 2. `ROLLUP` Hierarchy Syntax
```sql
SELECT 
    sale_year, 
    sale_quarter, 
    SUM(sale_amount) AS revenue
FROM quarterly_sales
GROUP BY ROLLUP (sale_year, sale_quarter);
```

### 3. `CUBE` Syntax
```sql
SELECT 
    channel, 
    customer_segment, 
    COUNT(*) AS total_signups
FROM signups
GROUP BY CUBE (channel, customer_segment);
```

### 4. Labeling Subtotals with `GROUPING()`
```sql
SELECT 
    CASE WHEN GROUPING(region) = 1 THEN 'All Regions' ELSE region END AS display_region,
    CASE WHEN GROUPING(category) = 1 THEN 'All Categories' ELSE category END AS display_category,
    SUM(revenue) AS total_revenue
FROM sales_summary
GROUP BY CUBE (region, category);
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **`GROUPING SETS`** | Fully supported ANSI syntax | **Not Supported** (Must emulate with `UNION ALL`) | **Not Supported** (Must emulate with `UNION ALL`) | Fully supported ANSI syntax |
| **`ROLLUP`** | Standard: `GROUP BY ROLLUP(a, b)` | Proprietary: `GROUP BY a, b WITH ROLLUP` | **Not Supported** | Standard: `GROUP BY ROLLUP(a, b)` |
| **`CUBE`** | Standard: `GROUP BY CUBE(a, b)` | **Not Supported** | **Not Supported** | Standard: `GROUP BY CUBE(a, b)` |
| **`GROUPING()` function**| Fully supported ANSI: `GROUPING(a)` | Supported in MySQL 8.0+: `GROUPING(a)` | **Not Supported** | Fully supported ANSI: `GROUPING(a)` |
| **`GROUPING_ID()`** | Supported (multi-argument `GROUPING`) | Supported as `GROUPING(a, b, ...)` | **Not Supported** | Fully supported: `GROUPING_ID(a, b)` |

---

## Practical Examples

### Example 1: Basic — Hierarchical Sales Breakdown with `ROLLUP`
Calculate annual, quarterly, and all-time revenue for an e-commerce platform.

```sql
CREATE TABLE sales_records (
    order_id INT PRIMARY KEY,
    order_year INT NOT NULL,
    order_quarter INT NOT NULL,
    amount NUMERIC(10,2) NOT NULL
);

INSERT INTO sales_records VALUES
(1, 2025, 1, 1000.00),
(2, 2025, 1, 1500.00),
(3, 2025, 2, 2000.00),
(4, 2026, 1, 3000.00),
(5, 2026, 2, 4000.00);

SELECT 
    order_year,
    order_quarter,
    SUM(amount) AS revenue,
    GROUPING(order_year) AS g_year,
    GROUPING(order_quarter) AS g_quarter
FROM sales_records
GROUP BY ROLLUP (order_year, order_quarter)
ORDER BY order_year NULLS LAST, order_quarter NULLS LAST;
```

```
Result:
 order_year | order_quarter | revenue  | g_year | g_quarter 
------------+---------------+----------+--------+-----------
       2025 |             1 |  2500.00 |      0 |         0
       2025 |             2 |  2000.00 |      0 |         0
       2025 |          NULL |  4500.00 |      0 |         1  <- 2025 Subtotal
       2026 |             1 |  3000.00 |      0 |         0
       2026 |             2 |  4000.00 |      0 |         0
       2026 |          NULL |  7000.00 |      0 |         1  <- 2026 Subtotal
       NULL |          NULL | 11500.00 |      1 |         1  <- Grand Total
(7 rows)
```

---

### Example 2: Real-World — Cross-Tabular Analysis with `CUBE` and Clean Labels
Generate an executive matrix of product category by sales channel with formatted subtotal labels.

```sql
CREATE TABLE retail_transactions (
    txn_id INT PRIMARY KEY,
    channel VARCHAR(20) NOT NULL,
    category VARCHAR(30) NOT NULL,
    gross_amount NUMERIC(10,2) NOT NULL
);

INSERT INTO retail_transactions VALUES
(1, 'Online', 'Electronics', 500.00),
(2, 'Online', 'Apparel', 120.00),
(3, 'Retail Store', 'Electronics', 800.00),
(4, 'Retail Store', 'Apparel', 350.00);

SELECT 
    CASE WHEN GROUPING(channel) = 1 THEN 'All Channels' ELSE channel END AS sales_channel,
    CASE WHEN GROUPING(category) = 1 THEN 'All Categories' ELSE category END AS product_category,
    SUM(gross_amount) AS total_gross
FROM retail_transactions
GROUP BY CUBE (channel, category)
ORDER BY GROUPING(channel), GROUPING(category), channel, category;
```

```
Result:
 sales_channel | product_category | total_gross 
---------------+------------------+-------------
 Online        | Apparel          |      120.00
 Online        | Electronics      |      500.00
 Retail Store  | Apparel          |      350.00
 Retail Store  | Electronics      |      800.00
 Online        | All Categories   |      620.00
 Retail Store  | All Categories   |     1150.00
 All Channels  | Apparel          |      470.00
 All Channels  | Electronics      |     1300.00
 All Channels  | All Categories   |     1770.00
(9 rows)
```

---

### Example 3: Edge Case — Raw `NULL` Values vs Aggregate Subtotal `NULL`s
Demonstrate how the `GROUPING()` function prevents mistaking unassigned raw values for subtotal rows.

```sql
CREATE TABLE employee_work (
    emp_id INT PRIMARY KEY,
    department_code VARCHAR(10) NULL, -- Can be NULL!
    hours_billed NUMERIC(6,2) NOT NULL
);

INSERT INTO employee_work VALUES
(1, 'ENG', 40.00),
(2, NULL, 15.00), -- Unassigned department
(3, 'ENG', 35.00);

SELECT 
    department_code,
    SUM(hours_billed) AS total_hours,
    GROUPING(department_code) AS is_subtotal,
    CASE 
        WHEN GROUPING(department_code) = 1 THEN '*** Grand Total ***'
        WHEN department_code IS NULL THEN 'Unassigned Dept'
        ELSE department_code 
    END AS clean_display
FROM employee_work
GROUP BY ROLLUP (department_code);
```

```
Result:
 department_code | total_hours | is_subtotal |    clean_display    
-----------------+-------------+-------------+---------------------
 ENG             |       75.00 |           0 | ENG
 NULL            |       15.00 |           0 | Unassigned Dept     <- Real NULL
 NULL            |       90.00 |           1 | *** Grand Total *** <- Subtotal NULL
(3 rows)
```

---

### Example 4: Failure Mode — Combinatorial Explosion with `CUBE`
`CUBE` generates $2^N$ grouping sets. A developer inadvertently cubes 10 dimensions, causing query execution to freeze as the engine attempts to generate 1,024 grouping sets.

```sql
-- DANGEROUS PATTERN:
-- 2^8 = 256 grouping sets!
SELECT c1, c2, c3, c4, c5, c6, c7, c8, SUM(metric)
FROM telemetry_events
GROUP BY CUBE (c1, c2, c3, c4, c5, c6, c7, c8);

-- REMEDY: Use explicit GROUPING SETS to specify only the meaningful business views!
SELECT c1, c2, c3, SUM(metric)
FROM telemetry_events
GROUP BY GROUPING SETS (
    (c1, c2, c3),
    (c1, c2),
    (c1),
    ()
);
```

---

### Example 5: Optimized — Selective Multi-Dimensional Aggregation
Using composite columns inside `GROUPING SETS` to treat paired attributes as an indivisible dimensional unit.

```sql
-- In a store chain, store_id and store_city always move together.
-- Treat them as a single composite unit: (store_id, store_city)
SELECT 
    store_id, 
    store_city, 
    department, 
    SUM(sale_amount) AS revenue
FROM chain_sales
GROUP BY GROUPING SETS (
    ((store_id, store_city), department), -- Store department breakdown
    (store_id, store_city),               -- Store subtotal
    ()                                     -- Chain grand total
);
```

---

## Best Practices

1. **Always Use `GROUPING()` to Disambiguate Labels**: Never rely on `COALESCE(col, 'Total')` alone; if the column contains genuine raw `NULL` values, `COALESCE` will incorrectly label the unassigned group as `'Total'`.
2. **Order Matters in `ROLLUP`**: Always arrange `ROLLUP` dimensions from coarsest to finest granularity (e.g., `ROLLUP(year, quarter, month)`).
3. **Beware Combinatorial Explosion in `CUBE`**: Limit `CUBE` to at most 3 or 4 dimensions ($2^4 = 16$ sets). For higher-dimensional models, list exact required tuples using `GROUPING SETS`.
4. **Group Composite Attributes Together**: When two attributes have a 1:1 or functional relationship (e.g., `country_code` and `country_name`), enclose them in inner parentheses `((country_code, country_name), ...)` to avoid generating redundant combinations.
5. **SQLite / Legacy Engine Portability**: If code must execute on SQLite or MySQL 5.6, write a script or view that constructs the subtotals via `UNION ALL`.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Using `COALESCE(col, 'Total')` without `GROUPING()` | Raw `NULL` values in base data are mistakenly labeled as `'Total'`, corrupting report summaries. | Use `CASE WHEN GROUPING(col) = 1 THEN 'Total' ELSE col END`. |
| 2 | Reversing dimension hierarchy in `ROLLUP` | `ROLLUP (city, country)` produces meaningless subtotals (totals per city across all countries). | Order from highest to lowest hierarchy: `ROLLUP (country, city)`. |
| 3 | Cubing high-cardinality dimensions | `CUBE` on 6 dimensions produces 64 aggregate passes, leading to massive memory bloat and timeouts. | Use explicit `GROUPING SETS` with only necessary combinations. |
| 4 | Sorting output without handling subtotal `NULL`s | Standard `ORDER BY col ASC` places subtotal rows (`NULL`) at the top or bottom unexpectedly. | Use `ORDER BY GROUPING(col), col` or `NULLS LAST`. |
| 5 | Writing multiple `UNION ALL` queries instead of `GROUPING SETS` | Base table is scanned and filtered $N$ times instead of a single pass, multiplying I/O cost. | Replace `UNION ALL` boilerplate with a single `GROUP BY GROUPING SETS (...)`. |
| 6 | Attempting to use `GROUPING()` in the `WHERE` clause | `GROUPING()` evaluates aggregate grouping states and cannot be evaluated during row filtering. | Place filtering conditions involving `GROUPING()` in the `HAVING` clause. |

---

## Performance & Security Notes

### Single-Pass vs Multi-Pass Execution Costs
Consider an analytics query aggregating 10 million transactions:
- **`UNION ALL` Approach (4 queries)**:
  - 4 separate sequential scans of the table (40 million tuples read).
  - 4 separate sort/hash operations.
  - High cache eviction pressure on database buffer pool.
- **`GROUPING SETS` Approach (1 query)**:
  - Exactly 1 sequential scan of the table (10 million tuples read).
  - Intermediate hash/stream aggregate rolls up already summarized buffers.
  - Typically 3x to 5x faster with 75% less I/O overhead.

---

## Comparison Matrix

| Dimension | `GROUP BY a, b` | `GROUPING SETS` | `ROLLUP (a, b)` | `CUBE (a, b)` |
| :--- | :--- | :--- | :--- | :--- |
| **Output Sets** | Single grain: `(a, b)` | User-defined list of tuples | Hierarchical: $N + 1$ sets | Full power set: $2^N$ sets |
| **Generates Grand Total** | No | Only if `()` is included | Yes (always the last set) | Yes (always included) |
| **Order Dependent** | No | No | **Yes** (`ROLLUP(a,b) != ROLLUP(b,a)`) | No |
| **Primary Use Case** | Standard relational reporting | Custom reporting dashboards | Hierarchical drilling (Time, Geo) | Cross-dimensional discovery |
| **Relative Cost** | Baseline $1\times$ | $1.2\times - 1.5\times$ baseline | $1.2\times - 1.4\times$ baseline | $1.5\times - 3\times$ baseline |

---

## Interview Questions & Deep Dives

### Q1: How does `ROLLUP (a, b, c)` differ from `CUBE (a, b, c)`?
<details>
<summary>Click to view answer</summary>

- `ROLLUP (a, b, c)` produces $N + 1 = 4$ grouping sets following a strict linear hierarchy:
  1. `(a, b, c)`
  2. `(a, b)`
  3. `(a)`
  4. `()` (Grand Total)
- `CUBE (a, b, c)` produces $2^N = 2^3 = 8$ grouping sets representing all possible mathematical combinations:
  1. `(a, b, c)`
  2. `(a, b)`, `(a, c)`, `(b, c)`
  3. `(a)`, `(b)`, `(c)`
  4. `()` (Grand Total)
</details>

### Q2: Why is the `GROUPING()` function necessary when writing reports with subtotals?
<details>
<summary>Click to view answer</summary>

When a query rolls up data, the column attributes that are not part of the active subtotal are returned as `NULL`. If the underlying column itself contains natural `NULL` values (e.g., an employee without an assigned manager or department), writing `COALESCE(dept, 'Total')` incorrectly transforms genuine data into a subtotal label. The `GROUPING(col)` function returns `1` if and only if the `NULL` was introduced by the rollup mechanism, and `0` if the value is an actual data point.
</details>

### Q3: How do you generate only the subtotals and exclude the finest grain?
<details>
<summary>Click to view answer</summary>

You can specify the exact subtotals using `GROUPING SETS`:
```sql
GROUP BY GROUPING SETS (
    (region),
    ()
)
```
Alternatively, if using `ROLLUP`, you can filter in the `HAVING` clause:
```sql
GROUP BY ROLLUP (region, category)
HAVING GROUPING(category) = 1;
```
</details>

### Q4: Can you combine normal `GROUP BY` columns with a `ROLLUP`?
<details>
<summary>Click to view answer</summary>

Yes. This is called a **partial rollup**:
```sql
SELECT tenant_id, year, month, SUM(amount)
FROM invoices
GROUP BY tenant_id, ROLLUP (year, month);
```
Here, `tenant_id` is always present in every single output row. The rollup is computed independently *within* each tenant partition. Grouping sets generated: `(tenant_id, year, month)`, `(tenant_id, year)`, `(tenant_id)`.
</details>

### Q5: How can you write a query that works like `ROLLUP` in SQLite?
<details>
<summary>Click to view answer</summary>

Because SQLite does not support `ROLLUP` or `GROUPING SETS`, you must construct the result using `UNION ALL`:
```sql
SELECT year, month, SUM(amount) FROM sales GROUP BY year, month
UNION ALL
SELECT year, NULL, SUM(amount) FROM sales GROUP BY year
UNION ALL
SELECT NULL, NULL, SUM(amount) FROM sales;
```
</details>

### Q6: What is `GROUPING_ID()` and how does it encode grouping sets?
<details>
<summary>Click to view answer</summary>

`GROUPING_ID(c1, c2, c3)` concatenates the 1-bit boolean results of `GROUPING(c1)`, `GROUPING(c2)`, and `GROUPING(c3)` into a single binary integer:
- `(0, 0, 0)` -> Integer `0` (Finest grain: all columns active)
- `(0, 0, 1)` -> Integer `1` (`c3` aggregated)
- `(0, 1, 1)` -> Integer `3` (`c2` and `c3` aggregated)
- `(1, 1, 1)` -> Integer `7` (Grand Total)
This integer is frequently used for sorting or programmatic filtering in reporting engines.
</details>

### Q7: Why are composite parentheses used inside `GROUPING SETS (((a, b), c))`?
<details>
<summary>Click to view answer</summary>

Enclosing columns in parentheses like `(a, b)` treats them as a single composite dimension. In `GROUP BY GROUPING SETS (((a, b), c), ())`, the engine never computes `(a)` or `(b)` independently; it only aggregates by the combined pair `(a, b)` together with `c`, or the grand total `()`.
</details>

### Q8: What is the syntax difference between MySQL and PostgreSQL for `ROLLUP`?
<details>
<summary>Click to view answer</summary>

- ANSI / PostgreSQL: `GROUP BY ROLLUP (col1, col2)`
- MySQL: `GROUP BY col1, col2 WITH ROLLUP`
(Note: MySQL does not support standard `GROUPING SETS` or `CUBE`).
</details>

---

## Practice Exercises

### Exercise 1: Multi-Level Regional Sales
Given `orders(order_id, country, state, city, total_amount)`, write a query using `ROLLUP` that produces totals at the Country-State-City level, Country-State level, Country level, and Grand Total.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    country,
    state,
    city,
    SUM(total_amount) AS revenue
FROM orders
GROUP BY ROLLUP (country, state, city)
ORDER BY country NULLS LAST, state NULLS LAST, city NULLS LAST;
```
</details>

---

### Exercise 2: Clean Subtotal Labels
Modify Exercise 1 so that `NULL` subtotal rows are cleanly labeled:
- If `country` is aggregated, show `'All Countries'`
- If `state` is aggregated, show `'All States'`
- If `city` is aggregated, show `'All Cities'`

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    CASE WHEN GROUPING(country) = 1 THEN 'All Countries' ELSE country END AS country,
    CASE WHEN GROUPING(state) = 1 THEN 'All States' ELSE state END AS state,
    CASE WHEN GROUPING(city) = 1 THEN 'All Cities' ELSE city END AS city,
    SUM(total_amount) AS revenue
FROM orders
GROUP BY ROLLUP (country, state, city)
ORDER BY GROUPING(country), country, GROUPING(state), state, GROUPING(city), city;
```
</details>

---

### Exercise 3: Specific Business Reporting Matrix
Given `invoices(client_tier, payment_method, currency, amount)`, write a single query using `GROUPING SETS` that computes:
1. Breakdown by `(client_tier, payment_method)`
2. Breakdown by `(currency)`
3. Grand total

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    client_tier,
    payment_method,
    currency,
    SUM(amount) AS total_invoiced
FROM invoices
GROUP BY GROUPING SETS (
    (client_tier, payment_method),
    (currency),
    ()
);
```
</details>

---

### Exercise 4: Filter Out Grand Total
Using `ROLLUP (department, team)`, write a query that computes department and team subtotals but explicitly filters out the final Grand Total row using the `HAVING` clause.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    department,
    team,
    COUNT(*) AS headcount
FROM employees
GROUP BY ROLLUP (department, team)
HAVING GROUPING(department) = 0;
```
</details>

---

### Exercise 5: Converting MySQL `WITH ROLLUP` to PostgreSQL ANSI
Convert this MySQL query into standard ANSI SQL compliant with PostgreSQL 17:
```sql
-- MySQL code:
SELECT department_id, job_id, SUM(salary) 
FROM employees 
GROUP BY department_id, job_id WITH ROLLUP;
```

<details>
<summary>Click to view solution</summary>

```sql
SELECT department_id, job_id, SUM(salary)
FROM employees
GROUP BY ROLLUP (department_id, job_id);
```
</details>

---

### Exercise 6: Cross-Dimensional Product Analysis with CUBE
Given `sales(store_type, promotion_type, revenue)`, write a query using `CUBE` that returns revenue across all combinations of store type and promotion type, sorted with Grand Total at the bottom.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    store_type,
    promotion_type,
    SUM(revenue) AS total_revenue
FROM sales
GROUP BY CUBE (store_type, promotion_type)
ORDER BY 
    GROUPING(store_type) ASC, 
    GROUPING(promotion_type) ASC, 
    store_type, 
    promotion_type;
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                         MULTI-DIMENSIONAL GROUPING CHEATSHEET                           |
+-------------------+----------------------------+----------------------------------------+
| Syntax            | Grouping Sets Generated    | Ideal Use Case                         |
+-------------------+----------------------------+----------------------------------------+
| GROUPING SETS     | Exactly what is listed     | Custom reports, disparate dimensions   |
| ROLLUP (A, B)     | (A, B), (A), ()            | Hierarchies (Time, Geography, Orgs)    |
| CUBE (A, B)       | (A, B), (A), (B), ()       | Cross-tabs, all combinations (Power set)|
| GROUPING(col)     | Returns 1 if aggregated    | Disambiguate raw NULL vs subtotal NULL |
+-------------------+----------------------------+----------------------------------------+
```

- **Avoid `UNION ALL`**: Replace multiple table scans with single-pass `GROUPING SETS`.
- **Hierarchical Order**: In `ROLLUP (A, B, C)`, list from largest container to smallest (`Country -> State -> City`).
- **Safety**: Keep `CUBE` dimensions under 4 to avoid generating hundreds of grouping sets.

---

## Further Reading
- PostgreSQL 17 Documentation: *GROUP BY and GROUPING SETS Clause*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 7.10 `<group by clause>`
- Microsoft Learn: *Transact-SQL GROUP BY GROUPING SETS, ROLLUP and CUBE*
- Ralph Kimball & Margy Ross: *The Data Warehouse Toolkit: The Definitive Guide to Dimensional Modeling*
