# Window Functions: Core Concepts & the OVER Clause

| Metadata | Details |
| :--- | :--- |
| **Topic** | SQL Window Functions, OVER Clause, PARTITION BY, ORDER BY, and Window Execution Mechanics |
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

Standard SQL aggregate functions (`SUM`, `AVG`, `COUNT`) have a fundamental limitation: they **collapse** multiple input rows into a single summary row. If you need to display an employee's individual salary alongside the average salary of their department, standard aggregation forces you to write a complex, multi-pass query involving subqueries or self-joins.

Introduced in ANSI SQL:2003, **Window Functions** (also known as analytic functions) perform calculations across a set of table rows that are related to the current row, but **without collapsing the rows**. Every individual row retains its separate identity and column values while gaining access to partition-level or running statistics.

Window functions revolutionize analytical SQL. Calculations such as running bank balances, year-to-date sales totals, moving averages, percent-of-total distributions, and row rankings that once required hundreds of lines of procedural code or quadratic self-joins can now be expressed declaratively, cleanly, and executed in linear $O(N \log N)$ or $O(N)$ time.

---

## Core Concepts

### 1. The `OVER` Clause
The `OVER` clause is the syntactic marker that turns an aggregate or ranking function into a window function:
```sql
SELECT employee_name, salary, 
       AVG(salary) OVER () AS company_avg_salary
FROM employees;
```
An empty `OVER ()` creates a single window spanning every row emitted by the query's `WHERE` and `FROM` clauses.

### 2. `PARTITION BY`
The `PARTITION BY` clause divides the result set into distinct partitions (groups) that are processed independently:
```sql
AVG(salary) OVER (PARTITION BY department_id)
```
- The aggregate function is evaluated separately for each partition.
- Unlike `GROUP BY`, rows within the partition are **not** collapsed; the resulting department average is broadcast onto every employee row belonging to that department.

### 3. `ORDER BY` Inside the `OVER` Clause
When an `ORDER BY` clause is added inside `OVER (...)`, it controls the order in which rows within the partition are processed:
```sql
SUM(amount) OVER (PARTITION BY customer_id ORDER BY transaction_date)
```
- **The Critical Default Framing Trap**:
  - Without `ORDER BY`: The window frame covers **the entire partition**.
  - With `ORDER BY`: The window frame defaults to `RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW`!
  - Therefore, adding `ORDER BY` silently converts a total partition sum into a **cumulative running total**.

### 4. Named Windows (`WINDOW` Clause)
When multiple window functions share identical partitioning and ordering specifications, repeating the `OVER (...)` definition adds boilerplate. ANSI SQL allows defining a reusable named window via the `WINDOW` clause placed after `HAVING`:
```sql
SELECT 
    employee_name,
    SUM(salary) OVER w AS running_payroll,
    AVG(salary) OVER w AS running_avg_salary
FROM employees
WINDOW w AS (PARTITION BY dept_id ORDER BY hire_date);
```

---

## How It Works Internally

Window functions are evaluated late in the query execution pipeline—strictly after base tables are joined, filtered (`WHERE`), grouped (`GROUP BY`), and filtered again (`HAVING`).

```
                    [ 1. FROM & JOIN ]
                            │
                    [ 2. WHERE Filtering ]
                            │
                    [ 3. GROUP BY Aggregations ]
                            │
                    [ 4. HAVING Filtering ]
                            │
                    ▼════════════════════════════▼
                    ║ 5. WINDOW FUNCTIONS        ║  <-- Evaluated Here!
                    ║    - Sort by Partition/Ord ║      Row identity preserved!
                    ║    - WindowAgg operator    ║
                    ▲════════════════════════════▲
                            │
                    [ 6. SELECT Projection ]
                            │
                    [ 7. DISTINCT ]
                            │
                    [ 8. ORDER BY (Query Output) ]
                            │
                    [ 9. LIMIT / OFFSET ]
```

### Physical Execution Engine Mechanics:
1. **Sorting (`Sort` Node)**: The engine must arrange tuples so that rows sharing the same partition keys are contiguous, and ordered according to the window's `ORDER BY`. If a suitable B-Tree index exists, the sort step is eliminated; otherwise, the engine performs an in-memory or disk sort.
2. **`WindowAgg` Node**:
   - The engine streams sorted tuples through a dedicated `WindowAgg` executor node.
   - For partition-level aggregates (no `ORDER BY`), it buffers the partition, computes the scalar aggregate, and attaches it to every row in the partition buffer.
   - For running aggregates (with `ORDER BY`), it updates running accumulators sequentially tuple by tuple and emits each row immediately.

---

## Syntax & Usage

```sql
SELECT 
    department_id,
    employee_name,
    salary,
    -- Partition-wide aggregate (no row collapse)
    AVG(salary) OVER (PARTITION BY department_id) AS dept_avg_salary,
    -- Running total within partition
    SUM(salary) OVER (
        PARTITION BY department_id 
        ORDER BY hire_date ASC
    ) AS running_dept_payroll,
    -- Percent of department total
    ROUND(
        salary * 100.0 / SUM(salary) OVER (PARTITION BY department_id), 
        2
    ) AS pct_of_dept_payroll
FROM employees;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **Standard Window Aggregates** | Full ANSI support (`SUM`, `AVG`, `COUNT`, `MIN`, `MAX`) | Full support (MySQL 8.0+) | Full support (SQLite 3.25+) | Full support |
| **Named `WINDOW` Clause** | Fully supported (`WINDOW w AS (...)`) | Fully supported (`WINDOW w AS (...)`) | Fully supported (`WINDOW w AS (...)`) | **Not Supported** (Must duplicate `OVER (...)` specs) |
| **`FILTER` Clause on Window Aggs** | Supported: `SUM(val) FILTER (...) OVER (...)` | **Not Supported** | Supported since SQLite 3.30+ | **Not Supported** |
| **Execution Operator** | `WindowAgg` | `Window` stream iterator | `Window` B-Tree / Ephemeral | `Segment` + `Sequence Project` + `Window Spool` |

---

## Practical Examples

### Example 1: Basic — Comparing Individual Salary Against Company Average
Show each employee's salary and how much it deviates from the overall company average.

```sql
CREATE TABLE staff (
    emp_id INT PRIMARY KEY,
    emp_name VARCHAR(50) NOT NULL,
    department VARCHAR(50) NOT NULL,
    salary NUMERIC(10,2) NOT NULL
);

INSERT INTO staff VALUES
(1, 'Alice', 'Engineering', 90000.00),
(2, 'Bob', 'Engineering', 110000.00),
(3, 'Charlie', 'Marketing', 70000.00),
(4, 'Diana', 'Marketing', 80000.00);

SELECT 
    emp_name,
    department,
    salary,
    ROUND(AVG(salary) OVER (), 2) AS company_avg,
    ROUND(salary - AVG(salary) OVER (), 2) AS diff_from_avg
FROM staff;
```

```
Result:
 emp_name | department  |  salary   | company_avg | diff_from_avg 
----------+-------------+-----------+-------------+---------------
 Alice    | Engineering |  90000.00 |    87500.00 |       2500.00
 Bob      | Engineering | 110000.00 |    87500.00 |      22500.00
 Charlie  | Marketing   |  70000.00 |    87500.00 |     -17500.00
 Diana    | Marketing   |  80000.00 |    87500.00 |      -7500.00
(4 rows)
```

---

### Example 2: Real-World — Running Bank Balance and Daily Spend
Calculate a customer's cumulative running ledger balance over time.

```sql
CREATE TABLE bank_ledger (
    entry_id INT PRIMARY KEY,
    account_id INT NOT NULL,
    entry_date DATE NOT NULL,
    amount NUMERIC(10,2) NOT NULL -- positive for deposit, negative for withdrawal
);

INSERT INTO bank_ledger VALUES
(1, 101, '2026-01-01', 500.00),
(2, 101, '2026-01-03', -120.00),
(3, 101, '2026-01-05', 250.00),
(4, 102, '2026-01-01', 1000.00),
(5, 102, '2026-01-04', -300.00);

SELECT 
    entry_id,
    account_id,
    entry_date,
    amount,
    SUM(amount) OVER (
        PARTITION BY account_id 
        ORDER BY entry_date ASC, entry_id ASC
    ) AS running_balance
FROM bank_ledger
ORDER BY account_id, entry_date;
```

```
Result:
 entry_id | account_id | entry_date | amount  | running_balance 
----------+------------+------------+---------+-----------------
        1 |        101 | 2026-01-01 |  500.00 |          500.00
        2 |        101 | 2026-01-03 | -120.00 |          380.00
        3 |        101 | 2026-01-05 |  250.00 |          630.00
        4 |        102 | 2026-01-01 | 1000.00 |         1000.00
        5 |        102 | 2026-01-04 | -300.00 |          700.00
(5 rows)
```

---

### Example 3: Edge Case — Duplicate Order Keys in Default `RANGE` Framing
Demonstrating how identical order keys cause running totals to aggregate tied rows simultaneously when using the default `RANGE` frame!

```sql
CREATE TABLE stock_trades (
    trade_id INT PRIMARY KEY,
    trade_date DATE NOT NULL,
    shares INT NOT NULL
);

INSERT INTO stock_trades VALUES
(1, '2026-01-10', 100),
(2, '2026-01-10', 150), -- Tied date!
(3, '2026-01-11', 200);

SELECT 
    trade_id,
    trade_date,
    shares,
    -- Default frame: RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    SUM(shares) OVER (ORDER BY trade_date) AS running_range,
    -- Explicit frame: ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    SUM(shares) OVER (ORDER BY trade_date, trade_id ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS running_rows
FROM stock_trades;
```

```
Result:
 trade_id | trade_date | shares | running_range | running_rows 
----------+------------+--------+---------------+--------------
        1 | 2026-01-10 |    100 |           250 |          100
        2 | 2026-01-10 |    150 |           250 |          250
        3 | 2026-01-11 |    200 |           450 |          450
(3 rows)
```
*Key takeaway*: In `running_range`, trades 1 and 2 share the same date. Because `RANGE` treats tied ordering values as peers, both rows evaluate to `100 + 150 = 250`! To achieve true row-by-row progression, use `ROWS` or include a tie-breaking unique column (like `trade_id`) in `ORDER BY`.

---

### Example 4: Failure Mode — Attempting to Filter Window Functions in `WHERE`
Window functions are evaluated in Phase 5 (after `WHERE`). Placing a window function in the `WHERE` clause triggers a fatal syntax error.

```sql
-- FATAL SYNTAX ERROR:
-- ERROR: window functions are not allowed in WHERE
SELECT emp_name, salary
FROM staff
WHERE AVG(salary) OVER (PARTITION BY department) > 80000.00;

-- CORRECT PATTERN: Wrap in a CTE or Subquery!
WITH ranked_staff AS (
    SELECT 
        emp_name, 
        department, 
        salary,
        AVG(salary) OVER (PARTITION BY department) AS dept_avg
    FROM staff
)
SELECT emp_name, department, salary, dept_avg
FROM ranked_staff
WHERE dept_avg > 80000.00;
```

---

### Example 5: Optimized — Reusable Named Window Specification
Using the `WINDOW` clause to eliminate repetition and share execution state across multiple metrics.

```sql
SELECT 
    emp_id,
    department,
    salary,
    MIN(salary) OVER w AS dept_min_salary,
    MAX(salary) OVER w AS dept_max_salary,
    ROUND(AVG(salary) OVER w, 2) AS dept_avg_salary,
    COUNT(*) OVER w AS dept_headcount
FROM staff
WINDOW w AS (PARTITION BY department);
```

---

## Best Practices

1. **Be Aware of the Default Frame Trap**: Adding `ORDER BY` inside `OVER()` silently switches the frame from the entire partition to `RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW`. If you want a partition-wide aggregate alongside an ordered ranking, write two separate window specs.
2. **Break Ordering Ties**: When calculating running totals, always append a unique column (e.g., `ORDER BY created_at, id`) or explicitly specify `ROWS BETWEEN ...` to prevent tied rows from aggregating simultaneously.
3. **Filter via CTEs**: Never attempt to filter window function outputs in `WHERE` or `HAVING`. Always compute the window in a CTE and filter in the outer query.
4. **Index the Partition and Order Keys**: Create a composite B-Tree index on `(partition_col, order_col)`. This allows the engine to stream rows directly through `WindowAgg` without an expensive `Sort` step.
5. **Use Named Windows**: When applying 3 or more window functions with the same specification, define a named `WINDOW w AS (...)` clause for cleaner, more maintainable code.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Using a window function in the `WHERE` clause | Window functions are evaluated after `WHERE`; engine raises a syntax error. | Wrap the query in a CTE or subquery and filter outside. |
| 2 | Unexpected running total instead of partition total | Adding `ORDER BY` inside `OVER()` activates default `RANGE` framing, producing a cumulative total. | Remove `ORDER BY` from `OVER()` if you want the partition-wide total. |
| 3 | Tied rows causing jumpy running totals | Default `RANGE` frame groups rows with equal sort keys together as peers. | Specify `ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW` or add unique tie-breaker to `ORDER BY`. |
| 4 | Expecting window functions to collapse rows like `GROUP BY` | Window functions never reduce cardinality; they append calculations to every input row. | Use `GROUP BY` if you want a collapsed, single summary row per category. |
| 5 | Using window functions inside `GROUP BY` | Window functions operate on already-grouped results and cannot define grouping keys. | Pre-calculate window metrics in a CTE first before grouping. |
| 6 | Duplicating complex `OVER (...)` specs across 10 columns | Bloats query text and risks inconsistent partition clauses during maintenance. | Use the standard `WINDOW w AS (...)` clause. |

---

## Performance & Security Notes

### Memory & Sorting Overhead
Window functions require data to be partitioned and sorted. Consider this execution plan in PostgreSQL:
```sql
EXPLAIN ANALYZE
SELECT emp_id, dept_id, salary,
       SUM(salary) OVER (PARTITION BY dept_id ORDER BY hire_date)
FROM employees;
```
If no index exists on `(dept_id, hire_date)`, the plan reveals:
```
WindowAgg  (cost=120.50..185.00 rows=1000 width=48)
  ->  Sort  (cost=120.50..123.00 rows=1000 width=48)
        Sort Key: dept_id, hire_date
        Sort Method: quicksort  Memory: 98kB
        ->  Seq Scan on employees  (cost=0.00..25.00 rows=1000 width=48)
```
- If the table contains 10,000,000 rows and cannot fit in `work_mem`, `Sort` spills to temporary disk files, degrading performance.
- **Index Optimization**: A composite B-Tree index `CREATE INDEX idx_emp_dept_hire ON employees (dept_id, hire_date);` eliminates the `Sort` node entirely! The database performs an `Index Scan` and pipes sorted tuples directly into `WindowAgg` with minimal memory usage.

---

## Comparison Matrix

| Dimension | Standard Aggregate (`GROUP BY`) | Window Aggregate (`OVER`) |
| :--- | :--- | :--- |
| **Output Cardinality** | Collapsed: 1 row per group | Unaltered: Exactly 1 row per input tuple |
| **Available Columns** | Only grouping columns + aggregates | All original columns + computed window values |
| **Logical Execution** | Phase 3 (Early) | Phase 5 (Late, after `HAVING`) |
| **Filtering Mechanism**| Filtered via `HAVING` | Must be wrapped in CTE/subquery to filter |
| **Cumulative Totals** | Impossible without recursive CTE or self-join | Trivially computed via `OVER (ORDER BY ...)` |
| **Multiple Granularities**| Only one grouping grain per query | Multiple different `PARTITION BY` grains in the same query |

---

## Interview Questions & Deep Dives

### Q1: What is the exact difference between an aggregate function and a window function?
<details>
<summary>Click to view answer</summary>

An aggregate function collapses a set of rows into a single scalar value per group (reducing the total number of rows returned by the query). A window function performs a calculation across a set of rows related to the current row, but does **not** collapse the rows—each original row preserves its identity and appears in the final result set.
</details>

### Q2: Why cannot window functions be used directly in a `WHERE` or `HAVING` clause?
<details>
<summary>Click to view answer</summary>

Under the ANSI SQL logical query processing pipeline, `WHERE` (Phase 2), `GROUP BY` (Phase 3), and `HAVING` (Phase 4) are evaluated before window functions (Phase 5). At the time `WHERE` and `HAVING` are executed, window partitions have not been formed and window calculations have not been performed. To filter on the result of a window function, you must wrap it in a derived table or Common Table Expression (CTE).
</details>

### Q3: What is the default window frame when an `ORDER BY` is specified inside `OVER ()` versus when it is omitted?
<details>
<summary>Click to view answer</summary>

- When `ORDER BY` is **omitted**: The default frame is `RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING` (the entire partition).
- When `ORDER BY` is **present**: The default frame changes to `RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW` (from the start of the partition up to the current row and its peers).
</details>

### Q4: What does the term "peers" mean in window framing?
<details>
<summary>Click to view answer</summary>

Peers are rows within the same partition that have identical values for the ordering expression specified in `ORDER BY`. When using `RANGE` framing, all peer rows are considered part of the same frame boundary and evaluate to identical aggregate values. When using `ROWS` framing, rows are distinguished individually regardless of tied ordering values.
</details>

### Q5: Can you compute two window functions with different `PARTITION BY` clauses in the same `SELECT` statement?
<details>
<summary>Click to view answer</summary>

Yes. A single query can project multiple window functions with completely independent partitioning and ordering criteria:
```sql
SELECT 
    emp_name,
    AVG(salary) OVER (PARTITION BY department_id) AS dept_avg,
    AVG(salary) OVER (PARTITION BY location_id) AS location_avg,
    AVG(salary) OVER () AS global_avg
FROM employees;
```
The query planner will sort and evaluate each distinct window specification sequentially.
</details>

### Q6: What is a named window and which database dialect does NOT support it?
<details>
<summary>Click to view answer</summary>

A named window is defined in the `WINDOW` clause of a query (placed between `HAVING` and `ORDER BY`) and can be referenced by name in multiple `OVER` specifications (e.g., `OVER w`). It is fully supported in PostgreSQL, MySQL 8.0+, and SQLite 3.25+, but is **not supported** in Microsoft SQL Server (T-SQL), where developers must repeat the full window specification in each `OVER (...)` clause.
</details>

### Q7: How does creating a composite B-Tree index optimize window function execution?
<details>
<summary>Click to view answer</summary>

A window function requires input rows to be ordered by `(partition_keys, order_keys)`. If an index matching this exact column sequence exists, the storage engine can perform an Index Scan that delivers rows in the exact order required by `WindowAgg`. This completely eliminates the need for an expensive in-memory or disk-spilling `Sort` operator.
</details>

### Q8: What happens if you run `SUM(1) OVER (ORDER BY id)`?
<details>
<summary>Click to view answer</summary>

It acts as a running row counter, functionally equivalent to `ROW_NUMBER() OVER (ORDER BY id)` (assuming `id` is unique). For each row, it adds `1` to the accumulator of preceding rows.
</details>

---

## Practice Exercises

### Exercise 1: Percentage of Departmental Budget
Given `employees(emp_id, emp_name, dept_id, salary)`, write a query that outputs each employee's name, department ID, salary, and the percentage their salary represents of their department's total payroll. Round to 2 decimal places.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    emp_name,
    dept_id,
    salary,
    ROUND(
        (salary * 100.0) / SUM(salary) OVER (PARTITION BY dept_id), 
        2
    ) AS pct_of_dept_budget
FROM employees;
```
</details>

---

### Exercise 2: Cumulative Daily Signups
Given `signups(signup_id, signup_date)`, write a query that returns each `signup_date`, the count of signups on that specific date, and the cumulative running total of signups up to and including that date.

<details>
<summary>Click to view solution</summary>

```sql
WITH daily_counts AS (
    SELECT 
        signup_date, 
        COUNT(*) AS daily_signups
    FROM signups
    GROUP BY signup_date
)
SELECT 
    signup_date,
    daily_signups,
    SUM(daily_signups) OVER (ORDER BY signup_date ASC) AS cumulative_signups
FROM daily_counts
ORDER BY signup_date ASC;
```
</details>

---

### Exercise 3: Filter High Earners Without Row Collapse
Find all employees who earn more than their department's average salary. Return `emp_name`, `dept_id`, `salary`, and `dept_avg_salary`.

<details>
<summary>Click to view solution</summary>

```sql
WITH departmental_stats AS (
    SELECT 
        emp_name,
        dept_id,
        salary,
        ROUND(AVG(salary) OVER (PARTITION BY dept_id), 2) AS dept_avg_salary
    FROM employees
)
SELECT emp_name, dept_id, salary, dept_avg_salary
FROM departmental_stats
WHERE salary > dept_avg_salary;
```
</details>

---

### Exercise 4: Clean Named Window Refactor
Refactor the following query to use a named `WINDOW` clause to avoid duplicating the partition specification:
```sql
SELECT 
    product_id, category_id, price,
    MIN(price) OVER (PARTITION BY category_id ORDER BY price) AS min_p,
    MAX(price) OVER (PARTITION BY category_id ORDER BY price) AS max_p
FROM products;
```

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    product_id, 
    category_id, 
    price,
    MIN(price) OVER w AS min_p,
    MAX(price) OVER w AS max_p
FROM products
WINDOW w AS (PARTITION BY category_id ORDER BY price);
```
</details>

---

### Exercise 5: Compare Against Regional Max and Min
Given `stores(store_id, region, annual_sales)`, write a query that displays each store's sales alongside the difference between their sales and the highest-performing store in their region.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    store_id,
    region,
    annual_sales,
    MAX(annual_sales) OVER (PARTITION BY region) AS regional_best,
    MAX(annual_sales) OVER (PARTITION BY region) - annual_sales AS gap_to_best
FROM stores;
```
</details>

---

### Exercise 6: Fixing the Tied-Row Running Total Bug
Given `stock_movements(movement_id, item_id, movement_date, qty)`, explain why `SUM(qty) OVER (PARTITION BY item_id ORDER BY movement_date)` produces identical values for items moved on the same date, and provide the fix.

<details>
<summary>Click to view solution</summary>

```sql
-- Cause: Default RANGE frame considers duplicate movement_date rows as peers.
-- Fix 1: Add movement_id to ORDER BY as a unique tie-breaker.
SELECT 
    movement_id, item_id, movement_date, qty,
    SUM(qty) OVER (
        PARTITION BY item_id 
        ORDER BY movement_date, movement_id
    ) AS accurate_running_qty
FROM stock_movements;

-- Fix 2: Explicitly specify ROWS framing.
SELECT 
    movement_id, item_id, movement_date, qty,
    SUM(qty) OVER (
        PARTITION BY item_id 
        ORDER BY movement_date 
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) AS accurate_running_qty
FROM stock_movements;
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                              WINDOW FUNCTIONS CHEAT SHEET                               |
+-------------------+----------------------------+----------------------------------------+
| Syntax            | Frame Scope                | Behavior                               |
+-------------------+----------------------------+----------------------------------------+
| OVER ()           | Entire query result set    | Global aggregate on every row          |
| OVER (PARTITION)  | Entire partition           | Independent aggregate per partition    |
| OVER (ORDER BY)   | UNBOUNDED PRECEDING to CUR | Running cumulative calculation         |
| WINDOW w AS (...) | Reusable window definition | Avoids duplicate OVER specifications   |
+-------------------+----------------------------+----------------------------------------+
```

- **Execution Order**: Phase 5 (After `HAVING`, before `DISTINCT`, `ORDER BY`, `LIMIT`).
- **Never in `WHERE`**: Window functions cannot be filtered in `WHERE` or `HAVING`; wrap in a CTE.
- **Index Tip**: Composite index on `(PARTITION BY cols, ORDER BY cols)` eliminates sorting overhead.

---

## Further Reading
- PostgreSQL 17 Documentation: *Tutorial - Window Functions & Syntax*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 7.12 `<window clause>`
- Itzik Ben-Gan: *T-SQL Window Functions: For Data Analysis and Beyond*
- SQLite Documentation: *SQLite Window Functions Mechanics*
