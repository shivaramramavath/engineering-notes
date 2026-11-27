# 04: Scalar & Correlated Subqueries

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [03 - Set Operations: UNION, INTERSECT & EXCEPT](./03-set-operations.md) |
| **Target Audience** | Software Engineers, Data Engineers, Query Optimization Specialists |
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

A **Subquery** (or nested query) is an SQL query enclosed within parentheses that is embedded inside another outer SQL statement (`SELECT`, `INSERT`, `UPDATE`, or `DELETE`). Subqueries embody the functional composability of relational algebra: because every query operates on relations and produces a relation, queries can be nested arbitrarily inside one another.

Subqueries are used across three distinct syntactic locations:
1. **In the `SELECT` list (Scalar Subqueries)**: Computes a single value dynamically per outer row (e.g., retrieving the department average salary alongside each employee).
2. **In the `FROM` clause (Derived Tables / Inline Views)**: Treats the subquery's tabular output as a virtual table that can be joined, grouped, or filtered.
3. **In the `WHERE` / `HAVING` clause**: Compares candidate rows against dynamically computed thresholds or values from other tables.

Crucially, subqueries divide into two fundamental architectural classes:
- **Uncorrelated Subqueries**: Completely independent of the outer query. Evaluated **once** by the engine at plan execution start.
- **Correlated Subqueries**: Reference columns from the outer query. Conceptually evaluated **once per candidate row** of the outer query.

Understanding how query planners optimize or decorrelate subqueries—and when correlated subqueries degrade into catastrophic $O(N \times M)$ loops—is essential for writing performant SQL.

---

## 2. Core Concepts

### 1. Uncorrelated vs Correlated Subqueries

```
Uncorrelated Subquery (Evaluates Once):
SELECT * FROM products
WHERE retail_price > (SELECT AVG(retail_price) FROM products);
                            │
                            ▼
          Evaluated ONCE: AVG = 788.20
          Transformed to: WHERE retail_price > 788.20 (Fast!)
```

```
Correlated Subquery (Evaluates Per Row):
SELECT p1.product_name, p1.retail_price
FROM products p1
WHERE p1.retail_price > (
    SELECT AVG(p2.retail_price) 
    FROM products p2 
    WHERE p2.category_id = p1.category_id -- References outer row p1!
);
                            │
                            ▼
For EACH row in p1, the subquery calculates the average for THAT row's category!
If p1 has 1,000,000 rows, the subquery is conceptually executed 1,000,000 times!
```

### 2. Scalar Subqueries vs Multi-Row Subqueries
- **Scalar Subquery**: A subquery that returns **exactly one row and one column** (a single scalar value). If it returns 0 rows, it evaluates to `NULL`. If it returns more than 1 row or more than 1 column in a scalar context, the database engine crashes with a fatal runtime exception:
  `ERROR: more than one row returned by a subquery used as an expression`.
- **Derived Table (Table Subquery)**: A subquery located in the `FROM` clause. Represents a relation with arbitrary rows and columns. In standard SQL, **every derived table MUST be assigned a table alias**.

---

## 3. How It Works Internally

### Query Decorrelation & Subplan Flattening

How does modern PostgreSQL optimize correlated subqueries?

```
Naive Conceptual Execution:                      Optimized Plan (Decorrelation):
Outer Loop: 10,000 rows                          Query Planner "pulls up" the subquery!
  └── Runs Inner Subquery 10,000 times!           Converts to a single Hash Join or HashAggregate!
Cost: O(N * M) (Catastrophic latency)            Cost: O(N + M) (Sub-millisecond execution)
```

- When the query planner analyzes a correlated subquery, it attempts **Subquery Flattening / Decorrelation**:
  - It rewrites the correlation into a relational **Semi-Join**, **Anti-Join**, or **Lateral Hash Join**.
  - If the subquery can be successfully decorrelated, it executes in $O(N + M)$ time.
  - If the subquery cannot be decorrelated (e.g., complex aggregate correlations or volatile functions), the engine is forced to execute an un-flattened `SubPlan` operator, executing a loop for every outer row.

---

## 4. Syntax & Usage

### Core Subquery Patterns

```sql
-- 1. Scalar Subquery in SELECT List
SELECT 
    p.product_name,
    p.retail_price,
    (SELECT AVG(retail_price) FROM products) AS global_avg_price,
    p.retail_price - (SELECT AVG(retail_price) FROM products) AS variance_from_avg
FROM products p;

-- 2. Derived Table in FROM Clause (Requires mandatory alias!)
SELECT 
    cat_summary.category_id,
    cat_summary.avg_price
FROM (
    SELECT category_id, AVG(retail_price) AS avg_price
    FROM products
    GROUP BY category_id
) AS cat_summary
WHERE cat_summary.avg_price > 500.00;

-- 3. Correlated Subquery in WHERE Clause
SELECT c.customer_id, c.first_name, c.last_name
FROM customers c
WHERE (
    SELECT COUNT(*) 
    FROM orders o 
    WHERE o.customer_id = c.customer_id
) >= 2;
```

### Multi-Dialect Comparison Matrix

| Subquery Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Mandatory Alias on Derived Table**| Required | Required | Required (since 3.33) | Required |
| **Scalar Subquery Returning 0 Rows**| Evaluates to `NULL` | Evaluates to `NULL` | Evaluates to `NULL` | Evaluates to `NULL` |
| **Scalar Subquery Returning >1 Rows**| Throws Runtime Error| Throws Runtime Error| Throws Runtime Error| Throws Runtime Error |
| **Correlated Decorrelation Engine** | Advanced (GEQO & Subquery Pullup)| Advanced (Hypergraph optimizer)| Moderate (Flattens simple subqueries)| Advanced (Cascades optimizer) |
| **Row-Constructor Subqueries** | `(a, b) IN (SELECT x, y)`| Supported | Supported | Not supported (requires join) |

---

## 5. 5 Practical Progressive Examples

### Example 1: Uncorrelated Subquery (Products Above Average Price)
Finding all products priced above the catalog-wide average:

```sql
SELECT product_id, product_name, retail_price
FROM products
WHERE retail_price > (
    SELECT AVG(retail_price) FROM products
)
ORDER BY retail_price DESC;
```
```text
+------------+---------------------+--------------+
| product_id | product_name        | retail_price |
+------------+---------------------+--------------+
|        201 | Framework Laptop 16 |      1399.00 |
|        202 | MacBook Air M3       |      1099.00 |
|        203 | Pixel 9 Pro         |       999.00 |
+------------+---------------------+--------------+
```
*(The inner `SELECT AVG(...)` calculates once: `$788.20`. The outer query filters `retail_price > 788.20`).*

### Example 2: Correlated Subquery (Highest Earner per Department)
Finding employees who earn the highest salary within their specific department:

```sql
SELECT 
    e1.department_id,
    e1.first_name || ' ' || e1.last_name AS employee_name,
    e1.salary
FROM employees e1
WHERE e1.salary = (
    SELECT MAX(e2.salary)
    FROM employees e2
    WHERE e2.department_id = e1.department_id
)
ORDER BY e1.department_id;
```
```text
+---------------+---------------+-----------+
| department_id | employee_name | salary    |
+---------------+---------------+-----------+
|             1 | Elena Rostova | 145000.00 |
|             2 | Sarah Jenkins | 115000.00 |
+---------------+---------------+-----------+
```
*(For each employee `e1`, the subquery calculates the `MAX(salary)` for `e1.department_id`).*

### Example 3: Edge Case (Scalar Subquery Evaluating to NULL)
What happens when a scalar subquery matches 0 rows:

```sql
-- When a subquery in SELECT finds no matching rows, it emits NULL:
SELECT 
    c.customer_id,
    c.first_name,
    (
        SELECT MAX(o.total_amount) 
        FROM orders o 
        WHERE o.customer_id = c.customer_id 
          AND o.order_date < '2020-01-01' -- Matches zero rows!
    ) AS historical_max_spend
FROM customers c
LIMIT 2;
```
```text
+-------------+------------+----------------------+
| customer_id | first_name | historical_max_spend |
+-------------+------------+----------------------+
|         101 | Alex       |                 NULL |
|         102 | Priya      |                 NULL |
+-------------+------------+----------------------+
```
*(Scalar subqueries safely evaluate to `NULL` when empty).*

### Example 4: Failure Mode (Multiple Rows in Scalar Context)
The single most common subquery runtime failure:

```sql
-- FAILS: The inner subquery returns 2 rows, but the outer query expects 1!
SELECT 
    product_name,
    retail_price - (SELECT retail_price FROM products WHERE category_id = 2) AS diff
FROM products;
```
```text
ERROR: more than one row returned by a subquery used as an expression
```
**Fix**: Ensure uniqueness or apply an aggregation function (`AVG`, `MAX`, `MIN`), or add `LIMIT 1`:
```sql
SELECT 
    product_name,
    retail_price - (SELECT AVG(retail_price) FROM products WHERE category_id = 2) AS diff
FROM products;
```

### Example 5: Optimized Derived Table vs Repeated Scalar Subqueries
Refactoring multiple repetitive scalar subqueries into a single derived table join:

```sql
-- ANTI-PATTERN: Scanning the orders table twice per customer!
SELECT 
    c.customer_id,
    c.first_name,
    (SELECT COUNT(*) FROM orders o WHERE o.customer_id = c.customer_id) AS total_orders,
    (SELECT SUM(total_amount) FROM orders o WHERE o.customer_id = c.customer_id) AS total_spent
FROM customers c;

-- OPTIMIZED: Aggregate once in a derived table!
SELECT 
    c.customer_id,
    c.first_name,
    COALESCE(ord_summary.total_orders, 0) AS total_orders,
    COALESCE(ord_summary.total_spent, 0.00) AS total_spent
FROM customers c
LEFT JOIN (
    SELECT 
        customer_id, 
        COUNT(*) AS total_orders, 
        SUM(total_amount) AS total_spent
    FROM orders
    GROUP BY customer_id
) AS ord_summary ON c.customer_id = ord_summary.customer_id;
```
*(Reduces execution time from $O(N \times M)$ to a single $O(M)$ aggregation plus $O(N + M)$ join).*

---

## 6. Best Practices

1. **Avoid Scalar Subqueries in the `SELECT` List for Large Datasets**: A scalar subquery in `SELECT` often runs once per emitted row if not decorrelated. Replace it with a `LEFT JOIN` against an aggregated derived table or a window function.
2. **Always Provide Explicit Aliases for Derived Tables in `FROM`**: The SQL standard strictly requires every subquery in the `FROM` clause to have an alias: `FROM (SELECT ...) AS derived_name`.
3. **Guard Against Cardinality Violations**: When using a subquery with comparison operators (`WHERE price > (SELECT ...)`), ensure the subquery uses an aggregate function (`MAX`, `MIN`) or `LIMIT 1` to guarantee it never emits multiple rows.
4. **Prefer CTEs (Common Table Expressions) for Complex Subqueries**: If a subquery has multiple nesting levels, refactor it into named CTEs (`WITH ... AS`) to dramatically improve readability and debugging.
5. **Ensure Foreign Keys Are Indexed for Correlated Subqueries**: If a correlated subquery filters `WHERE inner.fk = outer.pk`, an index on `inner.fk` is mandatory to avoid full table scans on every loop iteration.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Multiple Rows Returned in Scalar Context** | Throws `ERROR: more than one row returned by a subquery used as an expression`. | Aggregate the subquery (`AVG`, `MAX`) or use `LIMIT 1`. |
| **Missing Alias on Derived Table** | Throws `ERROR: subquery in FROM must have an alias`. | Always supply an alias: `FROM (SELECT ...) AS t`. |
| **Un-Decorrelated SubPlan Loop** | If the optimizer cannot decorrelate a query, it runs a nested loop for every row, taking minutes. | Rewrite as an explicit `LEFT JOIN` against a grouped subquery. |
| **Referencing Outer Columns Two Levels Deep** | In standard SQL, a subquery can only see identifiers from its immediate enclosing scope, not arbitrary outer ancestors. | Restructure using joins, lateral joins, or CTEs. |
| **Using Subqueries Where Window Functions Apply** | Writing correlated subqueries to find running totals or rank rows is 100x slower than window functions. | Use window functions (`ROW_NUMBER() OVER (...)`). |
| **Modifying Table Inside Self-Subquery (MySQL)** | MySQL historically forbids updating a table while selecting from the same table in a subquery. | In MySQL, wrap the subquery in another derived table layer. |

---

## 8. Performance & Security Considerations

### Performance
- **Subquery Caching**:
  - In PostgreSQL, uncorrelated subqueries are executed as `InitPlan` nodes: the engine computes the value once before the main query starts and substitutes the constant into the executor.
- **Correlated `SubPlan` Execution**:
  - If `EXPLAIN` displays a `SubPlan 1` node inside a sequential scan, it confirms the optimizer failed to decorrelate the query! The subquery will execute for every single row scanned. Immediately refactor into a join.

### Security
- **Blind SQL Injection via Subqueries**:
  - Attackers utilize scalar subqueries to extract character-by-character data via binary search:
    `WHERE (SELECT ascii(substring(password, 1, 1)) FROM admin) > 64`
  - Always use prepared statements with parameterized inputs.

---

## 9. Comparison Matrix

### Subquery Types & Execution Profiles

| Subquery Type | Syntactic Location | Evaluated | Performance Profile | Cardinality Constraint |
|:---|:---|:---:|:---|:---:|
| **Uncorrelated Scalar** | `SELECT`, `WHERE` | **Once** (at plan init) | Extremely fast ($O(1)$) | Strictly 1 row, 1 col |
| **Correlated Scalar** | `SELECT`, `WHERE` | **Per candidate row** | Slow if not decorrelated ($O(N \times M)$) | Strictly 1 row, 1 col |
| **Derived Table** | `FROM` | **Once** (inlined/joined) | Fast ($O(N + M)$ via join) | Arbitrary rows & cols |
| **Quantified Set Subquery**| `WHERE col IN (...)` | **Once** (Semi-Join) | Fast ($O(N + M)$ via hash) | 1 column, arbitrary rows |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: What is the difference between an uncorrelated subquery and a correlated subquery?
<details>
<summary>Answer</summary>
- **Uncorrelated Subquery**: Completely self-contained; it does not reference any columns or aliases from the outer query. The database engine executes it once, caches the result, and uses it throughout the execution of the outer query.
- **Correlated Subquery**: Contains a reference to one or more columns from the outer query (e.g., `WHERE inner.dept_id = outer.dept_id`). Conceptually, the subquery depends on the current outer row and must be re-evaluated for every candidate tuple processed by the outer query.
</details>

#### Q2: What causes the error "more than one row returned by a subquery used as an expression", and how do you resolve it?
<details>
<summary>Answer</summary>
This error occurs when a subquery is placed in a **scalar context** (such as in the `SELECT` projection list or next to a scalar comparison operator like `=`, `>`, `<`), but the subquery execution returns 2 or more rows. 

Resolution:
1. If you need a single summary metric, apply an aggregate function: `SELECT MAX(...)` or `SELECT AVG(...)`.
2. If you only want the top result, append `LIMIT 1` or `FETCH FIRST 1 ROW ONLY`.
3. If multiple values are valid, change the comparison operator from `=` to a set operator like `IN`, `ANY`, or `EXISTS`.
</details>

#### Q3: What is "Subquery Decorrelation" (or Query Unnesting) performed by the query optimizer?
<details>
<summary>Answer</summary>
Subquery decorrelation is an optimizer optimization where a correlated subquery is transformed into an equivalent relational algebra join (such as a Semi-Join, Anti-Join, or Inner Join with an aggregate). 

Instead of executing an $O(N \times M)$ nested loop where the inner query runs $N$ times, decorrelation pulls the subquery into the `FROM` clause, allowing the engine to execute a Hash Join or Merge Join in $O(N + M)$ time.
</details>

#### Q4: Why does standard SQL require every derived table in the `FROM` clause to have an alias?
<details>
<summary>Answer</summary>
A subquery in the `FROM` clause creates an anonymous virtual relation. Because relational algebra operations require named relations and named attributes to resolve column references and prevent ambiguity, the SQL parser requires a table alias to assign an explicit namespace to the resulting columns (e.g., `FROM (SELECT ...) AS my_subquery`).
</details>

### Practical Subquery Puzzles

#### Q5: Write a query to find all products whose price is strictly greater than the average price of products in their own category, using a correlated subquery.
<details>
<summary>Answer</summary>

```sql
SELECT 
    p1.category_id,
    p1.product_id,
    p1.product_name,
    p1.retail_price
FROM products p1
WHERE p1.retail_price > (
    SELECT AVG(p2.retail_price)
    FROM products p2
    WHERE p2.category_id = p1.category_id
)
ORDER BY p1.category_id, p1.retail_price DESC;
```
</details>

#### Q6: Rewrite the correlated query from Q5 into an optimized query using an inline window function or join, eliminating the correlated subquery loop.
<details>
<summary>Answer</summary>

```sql
-- Using Window Functions (Most performant, single-pass over data):
WITH ProductAverages AS (
    SELECT 
        category_id,
        product_id,
        product_name,
        retail_price,
        AVG(retail_price) OVER (PARTITION BY category_id) AS cat_avg_price
    FROM products
)
SELECT category_id, product_id, product_name, retail_price
FROM ProductAverages
WHERE retail_price > cat_avg_price
ORDER BY category_id, retail_price DESC;
```
</details>

#### Q7: Write a query that returns each customer's `first_name` alongside the date of their very first order, using a scalar subquery in the `SELECT` list.
<details>
<summary>Answer</summary>

```sql
SELECT 
    c.customer_id,
    c.first_name,
    (
        SELECT MIN(o.order_date) 
        FROM orders o 
        WHERE o.customer_id = c.customer_id
    ) AS first_order_date
FROM customers c
ORDER BY c.customer_id;
```
</details>

#### Q8: Write an update statement that sets `is_vip = TRUE` for any customer who has placed more than 3 completed orders, using a subquery in the `WHERE` clause.
<details>
<summary>Answer</summary>

```sql
UPDATE customers
SET is_vip = TRUE
WHERE (
    SELECT COUNT(*) 
    FROM orders 
    WHERE orders.customer_id = customers.customer_id 
      AND orders.order_status = 'completed'
) > 3;
```
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Products Priced Below Global Average
Retrieve all products from `products` whose `retail_price` is less than the catalog average, displaying `product_name` and `retail_price`.
<details>
<summary>Solution</summary>

```sql
SELECT product_name, retail_price
FROM products
WHERE retail_price < (SELECT AVG(retail_price) FROM products)
ORDER BY retail_price ASC;
```
</details>

#### Exercise 2: Customer with Highest Single Order
Find the customer name who placed the single most expensive order in the database using a subquery.
<details>
<summary>Solution</summary>

```sql
SELECT first_name || ' ' || last_name AS customer_name
FROM customers
WHERE customer_id = (
    SELECT customer_id 
    FROM orders 
    ORDER BY total_amount DESC 
    LIMIT 1
);
```
</details>

### Intermediate Exercises

#### Exercise 3: Correlated Count in Projection
Write a query selecting `category_id`, `category_name`, and a subquery calculating the count of products in that category. Include categories with 0 products.
<details>
<summary>Solution</summary>

```sql
SELECT 
    cat.category_id,
    cat.category_name,
    (
        SELECT COUNT(*) 
        FROM products p 
        WHERE p.category_id = cat.category_id
    ) AS product_count
FROM categories cat
ORDER BY product_count DESC;
```
</details>

#### Exercise 4: Derived Table Join Filtering
Select all customers whose total lifetime spend exceeds $1,000 by joining against an aggregated derived table in the `FROM` clause.
<details>
<summary>Solution</summary>

```sql
SELECT 
    c.customer_id,
    c.first_name,
    c.email,
    spend.lifetime_spent
FROM customers c
JOIN (
    SELECT customer_id, SUM(total_amount) AS lifetime_spent
    FROM orders
    WHERE order_status = 'completed'
    GROUP BY customer_id
    HAVING SUM(total_amount) > 1000.00
) AS spend ON c.customer_id = spend.customer_id
ORDER BY spend.lifetime_spent DESC;
```
</details>

### Advanced Exercises

#### Exercise 5: Correlated Subquery with Multiple Aggregates Refactored
Given a slow query with multiple scalar subqueries:
```sql
SELECT 
    d.department_id,
    (SELECT COUNT(*) FROM employees e WHERE e.department_id = d.department_id),
    (SELECT AVG(salary) FROM employees e WHERE e.department_id = d.department_id),
    (SELECT MAX(salary) FROM employees e WHERE e.department_id = d.department_id)
FROM departments d;
```
Refactor it into an optimal single-scan query using a `LEFT JOIN`.
<details>
<summary>Solution</summary>

```sql
SELECT 
    d.department_id,
    COALESCE(stats.emp_count, 0) AS emp_count,
    ROUND(stats.avg_salary, 2) AS avg_salary,
    stats.max_salary
FROM departments d
LEFT JOIN (
    SELECT 
        department_id,
        COUNT(*) AS emp_count,
        AVG(salary) AS avg_salary,
        MAX(salary) AS max_salary
    FROM employees
    GROUP BY department_id
) AS stats ON d.department_id = stats.department_id;
```
</details>

#### Exercise 6: Profiling SubPlan Execution via EXPLAIN
Run `EXPLAIN (ANALYZE, BUFFERS)` on a query with an un-decorrelated subquery and inspect the `SubPlan` node metrics to verify loop iterations.
<details>
<summary>Solution</summary>

```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT 
    c.customer_id,
    (SELECT MAX(o.total_amount) FROM orders o WHERE o.customer_id = c.customer_id)
FROM customers c;
```
Look for: `SubPlan 1` with `loops=N`.
</details>

---

## 12. Summary & Quick Reference

### Subqueries Summary
- **Uncorrelated**: Evaluates once; completely independent; fast ($O(1)$ startup).
- **Correlated**: References outer query columns; evaluates per candidate row unless decorrelated by optimizer.
- **Scalar Context**: Strictly requires exactly 1 row, 1 col. Returns `NULL` if 0 rows; crashes if $>1$ rows.
- **Derived Table**: Must reside in `FROM` and **must have an alias**.
- **Optimization Rule**: Replace repetitive scalar subqueries in `SELECT` with a single aggregated derived table join or window function.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [Subqueries](https://www.postgresql.org/docs/17/queries-subqueries.html)
- Markus Winand: *"Decorrelating Subqueries"* (Use The Index, Luke)
- Goetz Graefe: *"Orthogonal Optimization of Subqueries and Aggregation"* (ACM SIGMOD)
- [05 - Subquery Quantifiers: EXISTS, IN, ANY & ALL](./05-exists-in-any-all.md)
