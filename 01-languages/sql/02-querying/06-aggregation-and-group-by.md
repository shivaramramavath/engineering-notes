# Aggregation and GROUP BY

| Metadata | Details |
| :--- | :--- |
| **Topic** | SQL Aggregates, GROUP BY Clause, HAVING Filtering, and Aggregation Internals |
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

Relational databases store microscopic transactions—individual clicks, invoices, stock movements, and ledger entries. However, businesses make decisions on macroscopic summaries: total monthly revenue, average order value, customer counts by region, and peak concurrency hours.

SQL aggregation transforms sets of individual tuples into concise summary statistics. The `GROUP BY` clause partitions a relation into disjoint equivalence classes based on grouping keys, and aggregate functions compute scalar results over each partition. The `HAVING` clause then filters these aggregated groups.

Understanding how database engines execute aggregation—specifically distinguishing memory-efficient streaming aggregation from memory-intensive hash aggregation—is critical for software engineers. Improper aggregate design can exhaust database memory (`work_mem`), spill gigabytes of hash buckets to disk, or introduce subtle accounting errors due to the way `NULL` values interact with `COUNT`, `SUM`, and `AVG`.

---

## Core Concepts

### 1. Standard Aggregate Functions
- **`COUNT(*)`**: Returns the total number of rows in the group, including rows containing `NULL` values or duplicates.
- **`COUNT(column)`**: Returns the count of rows where `column` is strictly non-`NULL`.
- **`COUNT(DISTINCT column)`**: Returns the number of unique, non-`NULL` values in `column`.
- **`SUM(column)`**: Computes the arithmetic sum of non-`NULL` values. If the group contains only `NULL`s (or 0 rows), `SUM` evaluates to `NULL` (not `0`).
- **`AVG(column)`**: Computes `SUM(col) / COUNT(col)`. **Crucial**: It divides by the count of non-`NULL` values, *not* the total number of rows.
- **`MIN(column)` / `MAX(column)`**: Returns the minimum or maximum non-`NULL` value based on column collation and sorting order.

### 2. The `GROUP BY` Clause and Equivalence Classes
The `GROUP BY` clause collapses all rows sharing identical values across the specified grouping expressions into a single output row. 
Under ANSI SQL:
- Any column appearing in the `SELECT` list that is **not** enclosed inside an aggregate function **must** appear in the `GROUP BY` clause.
- *ANSI SQL:1999 Functional Dependency Exception*: If the primary key of a table appears in the `GROUP BY` clause, other non-key columns of that same table are functionally dependent on the key and may be selected without explicit grouping (supported in PostgreSQL 9.1+ and MySQL 5.7+).

### 3. The `HAVING` Clause vs `WHERE` Clause
- **`WHERE`**: Filters individual base rows *before* grouping and aggregation occur. It cannot reference aggregate functions.
- **`HAVING`**: Filters summarized groups *after* aggregation has taken place. It evaluates group-level predicates (e.g., `HAVING SUM(amount) > 1000`).
- Filtering rows as early as possible in `WHERE` dramatically reduces the working set that `GROUP BY` must process.

---

## How It Works Internally

Relational database engines implement two primary physical algorithms for executing `GROUP BY`:

```
                 [ Incoming Filtered Tuples from WHERE ]
                                    │
               Does an index exist on the GROUP BY columns?
                                    │
               ┌────────────────────┴────────────────────┐
               ▼                                         ▼
      [ YES: Pre-Sorted Stream ]             [ NO: Unsorted Stream ]
               │                                         │
        GroupAggregate                            HashAggregate
               │                                         │
    Reads tuples in order.                    Builds hash table in memory
    When grouping key changes,                Key = Grouping Columns
    emits aggregate row and resets.           Accumulator = Running stats
    O(1) memory consumption.                             │
                                              Does hash table exceed work_mem?
                                                         │
                                              ┌──────────┴──────────┐
                                              ▼                     ▼
                                          [ In-Memory ]         [ Spill to Disk ]
                                          Fast O(N)             Partitions dumped to
                                                                temporary files
```

1. **`GroupAggregate` (Streaming Aggregation)**:
   - Requires input rows to be pre-sorted by the grouping keys (either via an existing B-Tree index scan or an explicit `Sort` node).
   - The engine iterates through the sorted tuples sequentially. As long as the grouping key is unchanged, it updates internal aggregate accumulators. The moment the key changes, it emits the aggregated row and resets the accumulator.
   - Memory footprint is constant $O(1)$ regardless of table size.

2. **`HashAggregate` (Hash Aggregation)**:
   - Used when input data is unsorted.
   - The engine allocates an in-memory hash table in database memory (`work_mem` in PostgreSQL). For each incoming row, it computes the hash of the grouping keys and looks up the bucket.
   - If the key exists, it updates the accumulators (e.g., adds to sum, increments count). If it does not exist, a new hash entry is inserted.
   - If the number of distinct groups exceeds the allocated `work_mem`, the engine spills partitions to temporary disk files, degrading performance.

---

## Syntax & Usage

```sql
SELECT 
    department_id,
    job_title,
    COUNT(*) AS total_employees,
    COUNT(commission_pct) AS employees_with_commission,
    AVG(salary) AS avg_salary,
    SUM(salary) AS total_payroll
FROM employees
WHERE termination_date IS NULL
GROUP BY department_id, job_title
HAVING COUNT(*) >= 5 AND AVG(salary) > 65000.00
ORDER BY total_payroll DESC;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **String Concatenation Aggregate** | `STRING_AGG(col, delimiter [ORDER BY col])` | `GROUP_CONCAT(col [ORDER BY col] SEPARATOR delimiter)` | `GROUP_CONCAT(col, delimiter)` (No native `ORDER BY`) | `STRING_AGG(col, delimiter) WITHIN GROUP (ORDER BY col)` |
| **`FILTER` Clause for Aggregates** | Fully supported: `COUNT(*) FILTER (WHERE status = 'PAID')` | **Not Supported** (Use `SUM(CASE WHEN ... THEN 1 ELSE 0 END)`) | Supported since SQLite 3.30+ | **Not Supported** (Use `CASE` inside aggregate) |
| **Functional Dependency in `GROUP BY`** | Full support (omit table columns if PK in `GROUP BY`) | Full support (with `ONLY_FULL_GROUP_BY` enabled) | Relaxed / Permissive (Bare columns return arbitrary row) | **Not Supported** (Must list every non-aggregated column) |
| **Default `GROUP BY` Algorithm** | Cost-based choice: `HashAggregate` or `GroupAggregate` | Cost-based choice: `hash` or `sort` | In-memory B-Tree sorting or hash table | Stream Aggregate or Hash Match Aggregate |

---

## Practical Examples

### Example 1: Basic — Customer Order Statistics
Compute total orders, total expenditure, and average order value per customer.

```sql
CREATE TABLE customers (
    customer_id INT PRIMARY KEY,
    customer_name VARCHAR(100) NOT NULL
);

CREATE TABLE orders (
    order_id INT PRIMARY KEY,
    customer_id INT NOT NULL,
    order_amount NUMERIC(10,2) NOT NULL,
    FOREIGN KEY (customer_id) REFERENCES customers(customer_id)
);

INSERT INTO customers VALUES 
(1, 'Alice Smith'), (2, 'Bob Jones'), (3, 'Charlie Brown');

INSERT INTO orders VALUES 
(101, 1, 150.00), (102, 1, 350.00), (103, 2, 80.00);

SELECT 
    c.customer_id,
    c.customer_name,
    COUNT(o.order_id) AS total_orders,
    COALESCE(SUM(o.order_amount), 0.00) AS total_spent,
    COALESCE(ROUND(AVG(o.order_amount), 2), 0.00) AS avg_order_value
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.customer_name
ORDER BY total_spent DESC;
```

```
Result:
 customer_id | customer_name | total_orders | total_spent | avg_order_value 
-------------+---------------+--------------+-------------+-----------------
           1 | Alice Smith   |            2 |      500.00 |          250.00
           2 | Bob Jones     |            1 |       80.00 |           80.00
           3 | Charlie Brown |            0 |        0.00 |            0.00
(3 rows)
```

---

### Example 2: Real-World — E-Commerce Category Performance with `HAVING`
Identify product categories with at least 2 distinct orders and total gross revenue exceeding \$200.

```sql
CREATE TABLE products (
    product_id INT PRIMARY KEY,
    category VARCHAR(50) NOT NULL,
    unit_price NUMERIC(10,2) NOT NULL
);

CREATE TABLE order_items (
    order_id INT NOT NULL,
    product_id INT NOT NULL,
    quantity INT NOT NULL,
    PRIMARY KEY (order_id, product_id)
);

INSERT INTO products VALUES 
(1, 'Electronics', 120.00),
(2, 'Electronics', 250.00),
(3, 'Stationery', 5.00),
(4, 'Furniture', 300.00);

INSERT INTO order_items VALUES 
(101, 1, 1),
(102, 1, 2),
(103, 2, 1),
(104, 3, 10),
(105, 4, 1);

SELECT 
    p.category,
    COUNT(DISTINCT oi.order_id) AS distinct_orders,
    SUM(oi.quantity * p.unit_price) AS gross_revenue
FROM products p
JOIN order_items oi ON p.product_id = oi.product_id
GROUP BY p.category
HAVING COUNT(DISTINCT oi.order_id) >= 2 
   AND SUM(oi.quantity * p.unit_price) > 200.00;
```

```
Result:
  category   | distinct_orders | gross_revenue 
-------------+-----------------+---------------
 Electronics |               3 |        610.00
(1 row)
```

---

### Example 3: Edge Case — `COUNT(*)` vs `COUNT(col)` with NULLs
Observe the behavior of `COUNT(*)`, `COUNT(bonus)`, and `AVG(bonus)` when rows contain `NULL`s.

```sql
CREATE TABLE sales_agents (
    agent_id INT PRIMARY KEY,
    agent_name VARCHAR(50) NOT NULL,
    bonus NUMERIC(10,2) NULL
);

INSERT INTO sales_agents VALUES 
(1, 'Agent 1', 1000.00),
(2, 'Agent 2', NULL),
(3, 'Agent 3', 2000.00),
(4, 'Agent 4', NULL);

SELECT 
    COUNT(*) AS row_count,
    COUNT(bonus) AS non_null_bonus_count,
    SUM(bonus) AS total_bonus,
    AVG(bonus) AS avg_bonus_ignoring_nulls,
    AVG(COALESCE(bonus, 0)) AS avg_bonus_treating_null_as_zero
FROM sales_agents;
```

```
Result:
 row_count | non_null_bonus_count | total_bonus | avg_bonus_ignoring_nulls | avg_bonus_treating_null_as_zero 
-----------+----------------------+-------------+--------------------------+---------------------------------
         4 |                    2 |     3000.00 |                  1500.00 |                          750.00
(1 row)
```
*Key takeaway*: `AVG(bonus)` yields `3000 / 2 = 1500.00`. If you intended to average across all 4 agents, you must explicitly use `AVG(COALESCE(bonus, 0))`, which yields `3000 / 4 = 750.00`.

---

### Example 4: Failure Mode — Misusing `HAVING` Instead of `WHERE`
Filtering unaggregated rows in `HAVING` forces the database engine to group every single row in the table first before discarding entire groups, leading to massive memory bloat.

```sql
-- POOR / SLOW PATTERN:
-- Scans all 10,000,000 orders across history, hashes all customers into memory,
-- and then discards 99% of them in HAVING!
SELECT customer_id, SUM(amount)
FROM orders
GROUP BY customer_id
HAVING MAX(order_date) >= '2026-01-01';

-- OPTIMIZED PATTERN:
-- Push non-aggregate filters into WHERE!
SELECT customer_id, SUM(amount)
FROM orders
WHERE order_date >= '2026-01-01'
GROUP BY customer_id;
```

---

### Example 5: Optimized — Index-Backed Streaming `GroupAggregate`
When a composite B-Tree index exists on `(department_id, salary)`, the database avoids the hash table entirely and performs a blazing-fast streaming aggregate.

```sql
CREATE TABLE departmental_salaries (
    dept_id INT NOT NULL,
    employee_id INT NOT NULL,
    salary NUMERIC(10,2) NOT NULL,
    PRIMARY KEY (dept_id, employee_id)
);

CREATE INDEX idx_dept_salary ON departmental_salaries (dept_id, salary);

-- Execution Plan in PostgreSQL:
-- GroupAggregate (cost=0.15..28.50 rows=10 width=40)
--   -> Index Only Scan using idx_dept_salary on departmental_salaries
SELECT dept_id, AVG(salary), MAX(salary)
FROM departmental_salaries
GROUP BY dept_id;
```

---

## Best Practices

1. **Always Filter Early in `WHERE`**: Never put conditions that do not involve aggregate functions into `HAVING`. Filtering early shrinks the hash table and eliminates sorting overhead.
2. **Handle `NULL` in `SUM` and `AVG`**: Remember that `SUM` of zero rows returns `NULL`, not `0`. Always wrap sums in `COALESCE(SUM(val), 0)` when zero is expected.
3. **Be Cautious with `COUNT(DISTINCT)`**: `COUNT(DISTINCT col)` requires tracking unique sets per group, disabling simple running accumulators and consuming significant CPU and memory.
4. **Leverage Functional Dependencies**: When grouping by a table with many columns, group only by the primary key `GROUP BY c.customer_id` (in PostgreSQL/MySQL) instead of listing all 15 customer attributes.
5. **Tune `work_mem` for Large Aggregations**: If query plans show `HashAggregate` spilling to disk (`Batch: 2, Disk: 1420kB`), increase `work_mem` for the analytical session.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Selecting unaggregated column not in `GROUP BY` | Violates relational algebra; non-deterministic in permissive modes, syntax error in ANSI SQL. | Add the column to `GROUP BY` or wrap it in an aggregate function like `MIN()`, `MAX()`, or `ARRAY_AGG()`. |
| 2 | Placing base row filters in `HAVING` | Forces database to aggregate millions of irrelevant rows before discarding them, wasting memory. | Move row-level predicates to the `WHERE` clause. |
| 3 | Assuming `AVG(col)` divides by total rows | `AVG` ignores `NULL` rows, which can inflate metric averages (e.g., bonus calculation). | Use `AVG(COALESCE(col, 0))` if `NULL` represents zero. |
| 4 | Expecting `SUM` on empty table to return `0` | ANSI standard defines `SUM` of an empty set as `NULL`. Application code expecting a number will crash with NullPointerExceptions. | Wrap with `COALESCE(SUM(col), 0)`. |
| 5 | Overusing `COUNT(DISTINCT)` across multiple columns | Forces multi-pass aggregation or cross-product hash structures, severely degrading query latency. | Use CTEs or HyperLogLog (`pg_partman`/`datasketches`) for approximate distinct counting. |
| 6 | Forgetting to `GROUP BY` foreign key when doing `LEFT JOIN` | Aggregating parent table metrics joined to child tables inflates counts and sums due to Cartesian duplication. | Aggregate child table in a CTE or subquery first before joining to parent. |

---

## Performance & Security Notes

### HashAggregate vs GroupAggregate Memory Scaling
- When PostgreSQL plans an aggregation:
  - If expected hash table size < `work_mem`: It chooses `HashAggregate` ($O(N)$ CPU, single pass).
  - If expected hash table size > `work_mem`: It either uses multi-batch disk-spilling HashAggregate or introduces a `Sort` node to feed a `GroupAggregate`.
- Spilling hash buckets to disk triggers massive temporary I/O write operations. You can identify this in `EXPLAIN (ANALYZE, BUFFERS)`:
```
HashAggregate  (cost=1450.00..1820.00 rows=5000 width=32)
  Batches: 4  Memory Usage: 4096kB  Disk Usage: 18432kB
```
Remedy: Increase `work_mem` (e.g., `SET work_mem = '64MB';`) or create an index matching the `GROUP BY` columns to unlock in-memory streaming `GroupAggregate`.

---

## Comparison Matrix

| Dimension | `WHERE` Clause | `HAVING` Clause |
| :--- | :--- | :--- |
| **Execution Order** | Phase 2 (Immediately after `FROM` / `JOIN`) | Phase 5 (Immediately after `GROUP BY`) |
| **Predicate Scope** | Evaluates individual rows | Evaluates summarized group equivalence classes |
| **Aggregate Functions**| **Forbidden** (`WHERE SUM(val) > 10` raises error) | **Permitted** (`HAVING SUM(val) > 10`) |
| **Index Utilization** | Direct index scan / seek | Operates on aggregated accumulator buffers |
| **Primary Purpose** | Minimize input dataset size | Filter resulting metrics based on business thresholds |

---

## Interview Questions & Deep Dives

### Q1: What is the exact difference between `COUNT(*)`, `COUNT(1)`, and `COUNT(column)`?
<details>
<summary>Click to view answer</summary>

- `COUNT(*)` counts the total number of rows in the table/group. It does not inspect individual column values and includes rows even if all their columns are `NULL`.
- `COUNT(1)` is semantically and physically identical to `COUNT(*)`. The query optimizer treats constant literals identically to `*`.
- `COUNT(column)` inspects the specified column for every row and increments the counter **only** if the column value is non-`NULL`. It incurs a slight overhead because it must read the column value or check the null bitmap.
</details>

### Q2: What does ANSI SQL:1999 say about functional dependency in `GROUP BY`?
<details>
<summary>Click to view answer</summary>

Under SQL:1999 (and implemented in PostgreSQL 9.1+ and MySQL 5.7+), if a query groups by the primary key of a table, any other column from that same table is functionally dependent on the primary key (since a primary key uniquely determines all column values in that row). Therefore, the database engine allows developers to omit the dependent columns from the `GROUP BY` clause without raising a syntax error.
</details>

### Q3: Why does `SELECT dept_id, AVG(salary) FROM employees WHERE AVG(salary) > 50000 GROUP BY dept_id` fail?
<details>
<summary>Click to view answer</summary>

In the logical query processing pipeline, the `WHERE` clause is evaluated *before* grouping occurs. At the time `WHERE` executes, groups have not been formed and aggregate functions like `AVG()` have not been computed. Filtering on aggregate expressions must be placed in the `HAVING` clause, which executes *after* `GROUP BY`.
</details>

### Q4: How does a database engine execute `GROUP BY` when memory is insufficient?
<details>
<summary>Click to view answer</summary>

If the engine uses `HashAggregate` and the hash table exceeds `work_mem`, it spills data to temporary files using a partitioned hash join algorithm: it hashes keys into separate disk partitions, sorts/aggregates each partition independently, and merges the results. Alternatively, the planner will choose a `Sort` node (external disk sort) followed by a streaming `GroupAggregate`, which streams sorted data and requires only $O(1)$ memory.
</details>

### Q5: What is the result of `SUM()` on an empty set or on a column where all rows are `NULL`?
<details>
<summary>Click to view answer</summary>

It returns `NULL`, **not** `0`. ANSI SQL defines all aggregate functions (except `COUNT`, which returns 0) as returning `NULL` when applied to an empty set or a set containing solely `NULL` values. Application code must use `COALESCE(SUM(val), 0)` to guarantee numeric zero.
</details>

### Q6: Can you use aliases defined in the `SELECT` clause inside the `GROUP BY` clause?
<details>
<summary>Click to view answer</summary>

Under strict ANSI SQL, **no**, because `GROUP BY` is logically evaluated before `SELECT`. However:
- PostgreSQL allows referencing output column names or positions (`GROUP BY 1`) as an extension.
- MySQL and SQLite allow aliases in `GROUP BY` and `HAVING`.
- SQL Server strictly enforces the ANSI rule and rejects column aliases in `GROUP BY`.
Best practice: write the underlying expression or wrap the query in a CTE/subquery.
</details>

### Q7: What is the "Fan-Out Problem" when aggregating across multiple joined tables?
<details>
<summary>Click to view answer</summary>

When joining a parent table (e.g., `orders`) to multiple one-to-many child tables (e.g., `order_items` and `order_payments`), a Cartesian product occurs. If an order has 3 items and 2 payments, 6 joined rows are produced. Calculating `SUM(order_items.price)` or `SUM(order_payments.amount)` on the joined set will duplicate values 2x and 3x respectively, producing wildly incorrect totals. The solution is to aggregate each child table independently in separate CTEs before joining to the parent.
</details>

### Q8: How does PostgreSQL's `FILTER (WHERE ...)` clause improve conditional aggregation?
<details>
<summary>Click to view answer</summary>

PostgreSQL supports the standard ANSI `FILTER (WHERE ...)` syntax:
```sql
SELECT 
    COUNT(*) AS total_orders,
    COUNT(*) FILTER (WHERE status = 'PAID') AS paid_orders,
    SUM(amount) FILTER (WHERE status = 'PAID') AS paid_amount
FROM orders;
```
This is far more readable and concise than legacy `SUM(CASE WHEN status = 'PAID' THEN amount ELSE 0 END)` syntax, and avoids subtle null-handling mistakes.
</details>

---

## Practice Exercises

### Exercise 1: Department Payroll Analysis
Given `employees(emp_id, dept_id, salary, is_active)`, write a query to return the `dept_id`, count of active employees, and average salary of active employees for all departments with at least 3 active employees.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    dept_id,
    COUNT(*) AS active_employee_count,
    ROUND(AVG(salary), 2) AS avg_active_salary
FROM employees
WHERE is_active = TRUE
GROUP BY dept_id
HAVING COUNT(*) >= 3;
```
</details>

---

### Exercise 2: Identify Duplicate Email Registrations
Given a table `users(user_id, email, created_at)`, write a query to find all emails that appear more than once, along with the total count of registrations and the earliest creation timestamp.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    email,
    COUNT(*) AS registration_count,
    MIN(created_at) AS first_registered_at
FROM users
GROUP BY email
HAVING COUNT(*) > 1;
```
</details>

---

### Exercise 3: Safe Total Revenue Computation
Write a query calculating total revenue from `invoices(invoice_id, client_id, amount, status)`. Ensure that clients with zero paid invoices display a total revenue of `0.00` rather than `NULL`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    client_id,
    COALESCE(SUM(amount), 0.00) AS total_revenue
FROM invoices
WHERE status = 'PAID'
GROUP BY client_id;
```
</details>

---

### Exercise 4: Comma-Separated Role Listing
Given `users(user_id, username)` and `user_roles(user_id, role_name)`, write a query that lists each `username` and a comma-separated list of their assigned roles sorted alphabetically. Provide the PostgreSQL syntax.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    u.username,
    STRING_AGG(ur.role_name, ', ' ORDER BY ur.role_name) AS assigned_roles
FROM users u
JOIN user_roles ur ON u.user_id = ur.user_id
GROUP BY u.user_id, u.username;
```
</details>

---

### Exercise 5: Customer Retention Cohort Count
Given `orders(order_id, customer_id, order_date)`, count how many unique customers placed orders in each calendar year.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    EXTRACT(YEAR FROM order_date) AS order_year,
    COUNT(DISTINCT customer_id) AS unique_active_customers
FROM orders
GROUP BY EXTRACT(YEAR FROM order_date)
ORDER BY order_year ASC;
```
</details>

---

### Exercise 6: Solving the Multi-Join Aggregate Duplication Trap
Given `orders(order_id, customer_id)`, `order_items(item_id, order_id, price)`, and `order_discounts(discount_id, order_id, discount_val)`. Write a query returning each `order_id`, total item price, and total discount value without Cartesian duplication.

<details>
<summary>Click to view solution</summary>

```sql
WITH item_totals AS (
    SELECT order_id, SUM(price) AS total_items_price
    FROM order_items
    GROUP BY order_id
),
discount_totals AS (
    SELECT order_id, SUM(discount_val) AS total_discount_amount
    FROM order_discounts
    GROUP BY order_id
)
SELECT 
    o.order_id,
    COALESCE(it.total_items_price, 0.00) AS total_items_price,
    COALESCE(dt.total_discount_amount, 0.00) AS total_discount_amount
FROM orders o
LEFT JOIN item_totals it ON o.order_id = it.order_id
LEFT JOIN discount_totals dt ON o.order_id = dt.order_id;
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                 AGGREGATION CHEAT SHEET                                 |
+-------------------+-----------------------------------+---------------------------------+
| Clause / Function | Logical Timing                    | Primary Rule                    |
+-------------------+-----------------------------------+---------------------------------+
| WHERE             | Before Grouping (Phase 2)         | Discards base tuples; No aggs!  |
| GROUP BY          | Equivalence Class Split (Phase 4) | Collapses rows into partitions  |
| HAVING            | Post-Aggregation Filter (Phase 5) | Filters summarized groups       |
| COUNT(*)          | Aggregation Accumulator           | Counts all rows (includes NULL) |
| COUNT(col)        | Aggregation Accumulator           | Counts strictly non-NULL rows   |
| SUM(col)          | Aggregation Accumulator           | Returns NULL on empty set!      |
| AVG(col)          | Aggregation Accumulator           | SUM(col) / COUNT(col)           |
+-------------------+-----------------------------------+---------------------------------+
```

- **Physical Execution**:
  - `GroupAggregate`: Requires sorted input ($O(1)$ memory, streaming).
  - `HashAggregate`: Builds in-memory hash table ($O(N)$ memory, spills to disk if > `work_mem`).
- **Defensive SQL**: Always wrap `SUM` and `AVG` in `COALESCE` if zero is the desired default when matching rows are absent.

---

## Further Reading
- PostgreSQL 17 Documentation: *Aggregate Functions and GROUP BY Processing*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 7.10 `group by clause` & Section 7.11 `having clause`
- Markus Winand: *Use The Index, Luke! - Indexing for Group By*
- Joe Celko: *SQL for Smarties: Advanced SQL Programming (Aggregates & Grouping)*
