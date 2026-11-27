# 04: The SELECT Statement & Logical Evaluation Order

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [03 - Data Integrity & Constraints](./03-constraints.md) |
| **Target Audience** | Software Engineers, Data Analysts, Database Administrators |
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

The `SELECT` statement is the core retrieval command of SQL. However, one of the most confounding hurdles for engineers transitioning from imperative languages (Python, Java, Go, C++) to declarative SQL is the stark disparity between the **Lexical (Written) Order** of clauses and the **Logical Evaluation Order** enforced by relational algebra.

In an imperative language, code executes sequentially from top to bottom. But in SQL:
- You write `SELECT` first, followed by `FROM`, `WHERE`, `GROUP BY`, and `HAVING`.
- Conceptually, the database engine executes `FROM` first, followed by `WHERE`, `GROUP BY`, `HAVING`, and does NOT evaluate `SELECT` until almost the very end!

This fundamental disconnect explains why you cannot reference a column alias defined in the `SELECT` clause inside the `WHERE` clause, why `HAVING` filters aggregated values while `WHERE` filters raw tuples, and how window functions evaluate relative to group-by reductions. Mastering the logical evaluation pipeline is the single most important conceptual milestone in becoming proficient in SQL.

---

## 2. Core Concepts

### Lexical Order (How You Write It) vs Logical Order (How It Evaluates)

```
Lexical / Written Order:                 Logical Evaluation Order (Relational Pipeline):
+-------------------------------+        +-----------------------------------------------+
| 1. SELECT [DISTINCT]          |        | 1. FROM & JOINs (Build virtual dataset)       |
| 2. FROM & JOINs               |        | 2. WHERE (Filter individual base rows)        |
| 3. WHERE                      |        | 3. GROUP BY (Group rows into buckets)         |
| 4. GROUP BY                   |        | 4. HAVING (Filter aggregated group buckets)   |
| 5. HAVING                     | ───►   | 5. WINDOW (Calculate window functions)        |
| 6. WINDOW                     |        | 6. SELECT (Project expressions, assign aliases|
| 7. ORDER BY                   |        | 7. DISTINCT (Deduplicate resulting tuples)    |
| 8. LIMIT / OFFSET             |        | 8. ORDER BY (Sort final projected rows)       |
+-------------------------------+        | 9. LIMIT / OFFSET (Slice final output page)   |
                                         +-----------------------------------------------+
```

### Detailed Breakdown of the 9 Logical Evaluation Phases

1. **Phase 1: `FROM` & `JOIN`**:
   - The engine identifies all participating relations, evaluates Cartesian products, and applies `ON` / `USING` join predicates to build the working virtual relation.
2. **Phase 2: `WHERE`**:
   - Evaluates filter predicates on each individual candidate row. Rows evaluating to `FALSE` or `UNKNOWN` (NULL) are immediately discarded. *Notice: Aggregations do not exist yet!*
3. **Phase 3: `GROUP BY`**:
   - Collapses the filtered rows into distinct group buckets based on matching grouping keys.
4. **Phase 4: `HAVING`**:
   - Filters the grouped buckets. Predicates here can reference aggregate functions (`COUNT(*) > 5`).
5. **Phase 5: `WINDOW`**:
   - Evaluates window calculations (`ROW_NUMBER()`, `RANK()`, `SUM() OVER ()`) across partitions of the grouped rows without collapsing them.
6. **Phase 6: `SELECT`**:
   - Projects the requested attributes, evaluates scalar calculations, and assigns **column aliases** (e.g., `AS full_name`).
7. **Phase 7: `DISTINCT`**:
   - Removes duplicate projected tuples from the result set.
8. **Phase 8: `ORDER BY`**:
   - Sorts the final output rows. *Because `SELECT` evaluated in Phase 6, `ORDER BY` CAN see and sort by column aliases!*
9. **Phase 9: `LIMIT` / `OFFSET` (`FETCH FIRST`)**:
   - Slices the sorted rows, skipping `OFFSET` rows and returning up to `LIMIT` rows to the client.

---

## 3. How It Works Internally

### Logical vs Physical Execution Order

It is crucial to understand that **Logical Evaluation Order** describes the conceptual semantics of the language, NOT necessarily the exact physical order the database engine's optimizer executes on CPU hardware:

```
Logical Semantics:                               Physical Optimization (Cost-Based):
FROM -> WHERE -> GROUP BY -> SELECT               Filter Pushdown & Index Seek
                                                  +----------------------------------+
In theory: Read entire table into memory,         | Planner reads index directly,    |
then apply WHERE filters, then aggregate.         | pushing WHERE filter into index  |
                                                  | seek BEFORE touching the heap!   |
                                                  +----------------------------------+
```

- **Predicate Pushdown**: The query optimizer pushes `WHERE` filters down into the storage scan layer so that non-matching rows are discarded before ever loading into memory.
- **Projection Pruning**: The engine ignores columns not mentioned in `SELECT`, `WHERE`, or `JOIN`, fetching only required bytes from disk.

---

## 4. Syntax & Usage

### The Complete SELECT Syntax Anatomy

```sql
SELECT [ ALL | DISTINCT ]
    expression [[ AS ] column_alias ] [, ...]
FROM from_item [, ...]
[ WHERE condition ]
[ GROUP BY grouping_element [, ...] ]
[ HAVING condition ]
[ WINDOW window_name AS ( window_definition ) [, ...] ]
[ ORDER BY expression [ ASC | DESC ] [ NULLS { FIRST | LAST } ] [, ...] ]
[ LIMIT { count | ALL } ]
[ OFFSET start [ ROW | ROWS ] ];
```

### Multi-Dialect Comparison Matrix

| Query Evaluation Behavior | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Referencing SELECT Aliases in WHERE** | Strictly Forbidden (Standard) | Strictly Forbidden | Strictly Forbidden | Strictly Forbidden |
| **Referencing SELECT Aliases in GROUP BY** | Forbidden (Standard) | Permitted (Vendor extension) | Permitted (Vendor extension) | Strictly Forbidden |
| **Referencing SELECT Aliases in HAVING** | Strictly Forbidden (Standard) | Permitted (Vendor extension) | Permitted (Vendor extension) | Strictly Forbidden |
| **Referencing SELECT Aliases in ORDER BY** | Permitted (Standard) | Permitted | Permitted | Permitted |
| **`SELECT` Without `FROM` (Dual table)** | Supported (`SELECT 1;`) | Supported (`SELECT 1;`) | Supported (`SELECT 1;`) | Supported (`SELECT 1;`) |
| **`DISTINCT ON (col)` Syntax** | Supported (Postgres-specific) | Not supported | Not supported | Not supported |

---

## 5. 5 Practical Progressive Examples

### Example 1: Demonstrating Why Aliases Fail in `WHERE`
Understanding why referring to a column alias in `WHERE` causes a parser failure:

```sql
-- FAILS: 'gross_profit' does not exist when WHERE evaluates in Phase 2!
SELECT 
    product_name,
    retail_price - 20.00 AS gross_profit
FROM products
WHERE gross_profit > 100.00;
-- ERROR: column "gross_profit" does not exist
```

**The Correct Relational Solutions**:
```sql
-- Solution A: Repeat the expression (Optimizer evaluates it only once)
SELECT 
    product_name,
    retail_price - 20.00 AS gross_profit
FROM products
WHERE (retail_price - 20.00) > 100.00;

-- Solution B: Use a Common Table Expression (CTE) or Subquery
WITH ComputedProfit AS (
    SELECT 
        product_name,
        retail_price - 20.00 AS gross_profit
    FROM products
)
SELECT product_name, gross_profit
FROM ComputedProfit
WHERE gross_profit > 100.00;
```

### Example 2: `WHERE` vs `HAVING` in the Aggregation Lifecycle
Filtering individual transaction rows (`WHERE`) versus filtering aggregated group totals (`HAVING`).

```sql
-- We want customers who placed orders in 2024 (WHERE filters raw orders),
-- but ONLY if their TOTAL spend exceeds $1,000 (HAVING filters group sum).
SELECT 
    customer_id,
    COUNT(order_id) AS total_orders_2024,
    SUM(total_amount) AS total_spent_2024
FROM orders
WHERE order_date >= '2024-01-01' 
  AND order_date <  '2025-01-01'
  AND order_status = 'completed'
GROUP BY customer_id
HAVING SUM(total_amount) > 1000.00
ORDER BY total_spent_2024 DESC;
```
```text
+-------------+-------------------+------------------+
| customer_id | total_orders_2024 | total_spent_2024 |
+-------------+-------------------+------------------+
|         101 |                 2 |          1489.00 |
|         102 |                 1 |          1398.00 |
+-------------+-------------------+------------------+
```

### Example 3: Edge Case (Sorting by Expressions Not in `SELECT`)
In standard SQL, you can sort by columns and expressions that are NOT projected in the `SELECT` clause, as long as `DISTINCT` is not used.

```sql
-- Valid: Ordering by stock_qty even though it's omitted from the output!
SELECT product_id, product_name
FROM products
ORDER BY stock_qty ASC;
```

However, if `DISTINCT` is invoked, sorting by an unprojected column is strictly illegal!
```sql
-- FAILS in Standard SQL and PostgreSQL:
SELECT DISTINCT product_name
FROM products
ORDER BY stock_qty ASC;
```
```text
ERROR: for SELECT DISTINCT, ORDER BY expressions must appear in select list
LINE 3: ORDER BY stock_qty ASC;
```
*(Why? In Phase 7, `DISTINCT` collapses rows with identical `product_name`. If two rows have the same `product_name` but different `stock_qty`, the engine cannot determine which `stock_qty` value to use for ordering in Phase 8!)*

### Example 4: Failure Mode (Aggregating Without `GROUP BY`)
Attempting to select both scalar row columns and aggregate functions simultaneously:

```sql
-- FAILS: Ontological mismatch between row-level and group-level data!
SELECT 
    department_id, 
    first_name, 
    AVG(salary)
FROM employees;
```
```text
ERROR: column "employees.department_id" must appear in the GROUP BY clause or be used in an aggregate function
```
**Fix using Window Functions (Preserves individual rows while calculating aggregates)**:
```sql
SELECT 
    department_id, 
    first_name, 
    salary,
    AVG(salary) OVER (PARTITION BY department_id) AS avg_dept_salary
FROM employees;
```

### Example 5: Optimized Projection (PostgreSQL `DISTINCT ON`)
PostgreSQL provides a powerful proprietary extension: `DISTINCT ON (expression)`, which returns the first row for each group based on an `ORDER BY` clause.

```sql
-- Find the single most recent order placed by each customer:
SELECT DISTINCT ON (customer_id)
    customer_id,
    order_id,
    order_date,
    total_amount
FROM orders
WHERE order_status = 'completed'
ORDER BY customer_id, order_date DESC;
```
```text
+-------------+----------+---------------------+--------------+
| customer_id | order_id | order_date          | total_amount |
+-------------+----------+---------------------+--------------+
|         101 |     1003 | 2024-04-05 15:30:00 |        45.00 |
|         102 |     1002 | 2024-04-02 12:00:00 |      1398.00 |
+-------------+----------+---------------------+--------------+
```
*(The leftmost columns of `ORDER BY` must match the expressions in `DISTINCT ON`).*

---

## 6. Best Practices

1. **Internalize the 9-Phase Evaluation Pipeline**: Whenever a query fails with an alias error or unexpected aggregate result, trace the query through the pipeline: `FROM` $\rightarrow$ `WHERE` $\rightarrow$ `GROUP BY` $\rightarrow$ `HAVING` $\rightarrow$ `WINDOW` $\rightarrow$ `SELECT` $\rightarrow$ `DISTINCT` $\rightarrow$ `ORDER BY` $\rightarrow$ `LIMIT`.
2. **Never Put Non-Aggregated Filters in `HAVING`**: If a filter condition does not involve an aggregate function (e.g., `status = 'active'`), place it in `WHERE`, not `HAVING`. Filtering rows in `WHERE` discards tuples *before* the expensive grouping phase, saving memory and CPU.
3. **Use CTEs to Make Complex Aliases Available**: If you have an expensive calculation needed across `WHERE` and `GROUP BY`, encapsulate it in a Common Table Expression (`WITH ... AS`) rather than repeating the formula.
4. **Avoid `SELECT *` in Production Systems**: Explicitly enumerate required columns. `SELECT *` breaks caching, increases network serialization payloads, defeats index-only scans, and causes application breakages when schemas evolve.
5. **Always Pair `LIMIT` with an Explicit `ORDER BY`**: Without `ORDER BY`, `LIMIT 10` returns an arbitrary, non-deterministic subset of rows that can change from one second to the next based on disk block traversal order.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Using Column Aliases in `WHERE`** | `WHERE` evaluates in Phase 2, long before `SELECT` aliases are assigned in Phase 6. | Repeat the expression in `WHERE`, or wrap the query in a CTE or subquery. |
| **Filtering Base Columns in `HAVING`** | Placing `HAVING status = 'active'` forces the engine to group all rows first before filtering, wasting CPU and memory. | Move base row filters to `WHERE status = 'active'`. |
| **Using `SELECT DISTINCT` with Unprojected `ORDER BY`** | Once rows are collapsed by `DISTINCT`, unprojected sorting values become ambiguous, causing engine errors. | Include the sorting column in the `SELECT DISTINCT` list. |
| **Assuming `LIMIT` Runs Before `ORDER BY`** | The database cannot return the "top 5" rows until it has evaluated and sorted the entire candidate result set. | Recognize that `ORDER BY` sorts the entire set before `LIMIT` truncates it. |
| **Relying on MySQL's Permissive Group By** | In non-strict mode, MySQL historically allowed selecting unaggregated columns not in `GROUP BY`, returning arbitrary row values. | Enable `ONLY_FULL_GROUP_BY` and write standard-compliant SQL. |
| **Confusing `COUNT(*)` with `COUNT(col)`** | `COUNT(*)` counts total rows; `COUNT(col)` ignores rows where `col IS NULL`. | Use `COUNT(*)` for row counts; use `COUNT(col)` only when counting non-null values. |

---

## 8. Performance & Security Considerations

### Performance
- **Order of Evaluation vs Sargability**: While `WHERE` logically precedes `SELECT`, the query planner uses indexes to satisfy `WHERE` and `ORDER BY` simultaneously if a composite index matches both clauses (`CREATE INDEX idx_status_date ON orders(status, order_date DESC);`).
- **Early Filtering**: The earlier rows are eliminated in the pipeline (via `JOIN ON` and `WHERE`), the smaller the intermediate dataset that must be kept in `work_mem` for `GROUP BY` and `SORT`.

### Security
- **Projection Privilege Hardening**: PostgreSQL allows granting permissions on specific columns rather than whole tables (`GRANT SELECT (id, username) ON users TO reporter;`). Attempting to `SELECT password_hash` fails at Phase 2 (catalog resolution) with a permissions error.

---

## 9. Comparison Matrix

### Filtering Stages Compared

| Dimension | `ON` (Join Condition) | `WHERE` | `HAVING` |
|:---|:---|:---|:---|
| **Pipeline Phase** | Phase 1 (`FROM`) | Phase 2 (`WHERE`) | Phase 4 (`HAVING`) |
| **Operates On** | Row pairs during table join | Individual candidate rows | Grouped summary buckets |
| **Aggregates Allowed?** | No | No | Yes (`COUNT`, `SUM`, `AVG`) |
| **Impact on Outer Joins**| Determines which rows match; does NOT filter out unmatched left rows | Filters out rows *after* the join is complete (converts outer join to inner!) | Filters summary buckets after aggregation |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: Walk through the exact logical evaluation order of an SQL query from `FROM` to `LIMIT`.
<details>
<summary>Answer</summary>
The 9 logical evaluation phases occur in this order:
1. `FROM` (+ `JOIN ... ON`): Identifies relations and computes joined sets.
2. `WHERE`: Filters individual candidate rows.
3. `GROUP BY`: Aggregates remaining rows into distinct grouping keys.
4. `HAVING`: Filters aggregated group buckets based on aggregate conditions.
5. `WINDOW`: Evaluates window functions (`OVER`) across partitions.
6. `SELECT`: Evaluates projected expressions and assigns column aliases.
7. `DISTINCT`: Eliminates duplicate projected tuples.
8. `ORDER BY`: Sorts the final projected rows.
9. `LIMIT` / `OFFSET`: Slices the final sorted result set.
</details>

#### Q2: Why is it illegal in standard SQL to use a column alias declared in the `SELECT` clause within a `WHERE` clause?
<details>
<summary>Answer</summary>
Because `WHERE` executes in **Phase 2**, whereas `SELECT` does not evaluate until **Phase 6**. 

When the query engine processes the `WHERE` clause to filter raw table rows, the projection expressions in the `SELECT` clause have not been evaluated yet, and their assigned aliases do not exist in the catalog execution scope.
</details>

#### Q3: Why CAN an alias defined in `SELECT` be referenced in an `ORDER BY` clause?
<details>
<summary>Answer</summary>
Because `ORDER BY` executes in **Phase 8**, strictly *after* the `SELECT` clause has completed in Phase 6. By the time the engine begins sorting rows, all column aliases have been computed and bound to the projected output tuple.
</details>

#### Q4: What happens if you place a filter condition on a right-hand table in the `WHERE` clause of a `LEFT OUTER JOIN`?
<details>
<summary>Answer</summary>
Placing a filter on the right-hand table in the `WHERE` clause (e.g., `WHERE right_table.status = 'active'`) inadvertently **converts the `LEFT JOIN` into an `INNER JOIN`**!

Unmatched rows produced by the `LEFT JOIN` will have `right_table.status IS NULL`. In Three-Valued Logic, `NULL = 'active'` evaluates to `UNKNOWN`, which the `WHERE` clause rejects. Thus, all unmatched left rows are filtered out. 

To preserve the `LEFT JOIN` behavior, the condition must be placed in the `ON` clause: `LEFT JOIN right_table ON left.id = right.id AND right.status = 'active'`.
</details>

### Practical Query Analysis Puzzles

#### Q5: Identify the syntax error in this query and explain why it occurs:
```sql
SELECT department_id, AVG(salary) AS avg_sal
FROM employees
WHERE avg_sal > 50000
GROUP BY department_id;
```
<details>
<summary>Answer</summary>
Two errors exist:
1. `avg_sal` is a `SELECT` alias, which cannot be referenced in `WHERE`.
2. More fundamentally, `AVG(salary)` is an aggregate function. Aggregates cannot be evaluated in the `WHERE` clause because grouping has not taken place yet.

Fix: Move the aggregate filter to the `HAVING` clause:
```sql
SELECT department_id, AVG(salary) AS avg_sal
FROM employees
GROUP BY department_id
HAVING AVG(salary) > 50000;
```
</details>

#### Q6: Why does `SELECT DISTINCT name FROM users ORDER BY age;` fail in standard SQL, while `SELECT name FROM users ORDER BY age;` succeeds?
<details>
<summary>Answer</summary>
Without `DISTINCT`, every row has a 1-to-1 correspondence with a specific `age` value, so the engine can easily sort the projected `name` values by their corresponding `age`.

With `DISTINCT`, if two users share the same name (e.g., two people named "Alice", one aged 20 and one aged 45), `DISTINCT` collapses them into a single row: `"Alice"`. When `ORDER BY age` runs subsequently, the engine cannot determine whether `"Alice"` should be sorted as 20 or 45. Standard SQL forbids this ambiguity.
</details>

#### Q7: Write an SQL query that demonstrates how a window function can be filtered, explaining why it cannot be filtered directly in `WHERE` or `HAVING`.
<details>
<summary>Answer</summary>
Window functions evaluate in **Phase 5** (after `WHERE` in Phase 2 and `HAVING` in Phase 4). Therefore, a window function cannot be filtered in `WHERE` or `HAVING`. To filter on a window function, wrap it in a CTE:

```sql
WITH RankedSales AS (
    SELECT 
        salesperson_id,
        amount,
        ROW_NUMBER() OVER (ORDER BY amount DESC) AS sales_rank
    FROM sales
)
SELECT salesperson_id, amount, sales_rank
FROM RankedSales
WHERE sales_rank <= 3;
```
</details>

#### Q8: Does `LIMIT 5` stop the execution of an `ORDER BY` clause early? Explain.
<details>
<summary>Answer</summary>
No, not generally. In order to determine which 5 rows belong to the top of the sorted list, the database engine must evaluate the sort criteria across the **entire candidate dataset**. 

However, modern query planners can optimize this using a **Top-N Heap Sort** in memory: instead of sorting all 1,000,000 rows, the engine maintains a small bounded priority queue (heap) of 5 elements, updating it as it scans candidate rows, saving significant memory.
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Multi-Clause Query Construction
Write a query on the `orders` table that returns `customer_id` and the total sum of their completed orders, for customers who have placed at least 2 completed orders, sorted by total spent descending, returning only the top 3 customers.
<details>
<summary>Solution</summary>

```sql
SELECT 
    customer_id, 
    SUM(total_amount) AS total_spent
FROM orders
WHERE order_status = 'completed'
GROUP BY customer_id
HAVING COUNT(order_id) >= 2
ORDER BY total_spent DESC
LIMIT 3;
```
</details>

#### Exercise 2: Alias Reuse via CTE
Create a query on `products` that calculates `discounted_price = retail_price * 0.85`. Filter for products where `discounted_price < 500.00` and sort by `discounted_price` ascending, using a CTE so the formula is not duplicated.
<details>
<summary>Solution</summary>

```sql
WITH DiscountedCatalog AS (
    SELECT 
        product_id,
        product_name,
        retail_price * 0.85 AS discounted_price
    FROM products
)
SELECT product_id, product_name, discounted_price
FROM DiscountedCatalog
WHERE discounted_price < 500.00
ORDER BY discounted_price ASC;
```
</details>

### Intermediate Exercises

#### Exercise 3: Correcting Outer Join Predicate Placement
Given two tables `departments` and `employees`, write a query displaying all departments (including those with no employees), but only listing employees whose `salary > 80000`. Ensure departments with no high earners are still displayed.
<details>
<summary>Solution</summary>

```sql
-- The salary filter MUST be in the ON clause, NOT the WHERE clause!
SELECT 
    d.name AS department_name,
    e.first_name,
    e.salary
FROM departments d
LEFT JOIN employees e 
  ON d.department_id = e.department_id 
 AND e.salary > 80000
ORDER BY d.name, e.salary DESC;
```
</details>

#### Exercise 4: Aggregating with Conditional Expressions
Write a query showing `customer_id`, total orders placed, and the percentage of those orders that were `'completed'`.
<details>
<summary>Solution</summary>

```sql
SELECT 
    customer_id,
    COUNT(*) AS total_orders,
    ROUND(
        (COUNT(CASE WHEN order_status = 'completed' THEN 1 END)::numeric / COUNT(*)) * 100.0, 
        2
    ) AS completion_pct
FROM orders
GROUP BY customer_id
ORDER BY total_orders DESC;
```
</details>

### Advanced Exercises

#### Exercise 5: PostgreSQL `DISTINCT ON` Emulation
Using standard ANSI SQL (without using PostgreSQL's proprietary `DISTINCT ON`), write a query that returns the most recent order for each customer.
<details>
<summary>Solution</summary>

```sql
WITH RankedOrders AS (
    SELECT 
        customer_id,
        order_id,
        order_date,
        total_amount,
        ROW_NUMBER() OVER (
            PARTITION BY customer_id 
            ORDER BY order_date DESC
        ) AS rn
    FROM orders
    WHERE order_status = 'completed'
)
SELECT customer_id, order_id, order_date, total_amount
FROM RankedOrders
WHERE rn = 1
ORDER BY customer_id;
```
</details>

#### Exercise 6: Profiling Evaluation Order via EXPLAIN
Write a query filtering on a calculated expression and observe via `EXPLAIN` whether the query planner evaluated the expression during the sequential scan or after an index seek.
<details>
<summary>Solution</summary>

```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT customer_id, SUM(total_amount)
FROM orders
WHERE total_amount > 100.00
GROUP BY customer_id
HAVING COUNT(*) > 1
ORDER BY SUM(total_amount) DESC;
```
Inspect the plan tree:
1. `Filter: (total_amount > 100.00)` on the Scan node.
2. `HashAggregate` with `Filter: (count(*) > 1)`.
3. `Sort` on `SUM(total_amount)`.
</details>

---

## 12. Summary & Quick Reference

### The 9-Phase Logical Query Evaluation Pipeline
1. **`FROM` & `JOIN`**: Assembles the working virtual table.
2. **`WHERE`**: Filters individual base rows.
3. **`GROUP BY`**: Collapses rows into group buckets.
4. **`HAVING`**: Filters group buckets using aggregates.
5. **`WINDOW`**: Computes window functions over partitions.
6. **`SELECT`**: Evaluates projections and assigns column aliases.
7. **`DISTINCT`**: Deduplicates projected rows.
8. **`ORDER BY`**: Sorts final result rows (can see `SELECT` aliases!).
9. **`LIMIT` / `OFFSET`**: Slices final output rows for pagination.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [Chapter 7: Queries](https://www.postgresql.org/docs/17/queries.html)
- Itzik Ben-Gan: *"T-SQL Querying"* (Microsoft Press - The definitive guide to Logical Query Processing)
- Markus Winand: *"SQL Order of Operations"* (Modern SQL)
- [05 - Filtering Rows with WHERE & Operators](./05-where-filtering.md)
