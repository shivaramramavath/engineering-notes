# Subquery Predicates: EXISTS, IN, ANY, and ALL

| Metadata | Details |
| :--- | :--- |
| **Topic** | Subquery Predicates & Set Quantifiers (EXISTS, NOT EXISTS, IN, NOT IN, ANY, SOME, ALL) |
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

In relational database systems, filtering data based on criteria evaluated against another set of rows is one of the most frequent operations. SQL provides four fundamental set-membership and quantification predicates: `EXISTS`, `IN`, `ANY` (synonym `SOME`), and `ALL`.

While developers intuitively reach for `IN` when testing whether a scalar value matches any value in a subquery or static list, `IN` harbors one of the most treacherous traps in computer science: Three-Valued Logic (3VL) null propagation. When a subquery returns even a single `NULL`, `val NOT IN (subquery)` unconditionally evaluates to `UNKNOWN` or `FALSE` for every candidate row, silently wiping out query results in production.

`EXISTS`, by contrast, operates on two-valued existential logic: it asks simply whether the subquery yields *at least one tuple*. `EXISTS` is immune to null values in subquery columns, short-circuits as soon as the storage engine encounters the first qualifying row, and directly unlocks optimizer transformations into relational semi-joins and anti-joins. Mastering `EXISTS`, `IN`, `ANY`, and `ALL` enables backend engineers to write resilient, highly performant analytical queries without falling prey to silent data omission.

---

## Core Concepts

### 1. The `EXISTS` and `NOT EXISTS` Predicate
`EXISTS` evaluates a subquery and returns boolean `TRUE` if the subquery returns one or more rows, and `FALSE` if it returns zero rows. Because `EXISTS` only checks for tuple existence:
- The projection list of the subquery is completely ignored by the query planner. `SELECT 1`, `SELECT *`, or `SELECT 1/0` inside an `EXISTS` clause do not evaluate the select list expressions or raise division-by-zero errors.
- `EXISTS` never evaluates to `NULL` (`UNKNOWN`). It is strictly two-valued: `TRUE` or `FALSE`.

### 2. The `IN` and `NOT IN` Predicate
`x IN (subquery)` tests whether `x` equals any value returned by the subquery:
```sql
x IN (v1, v2, v3)  <=>  (x = v1 OR x = v2 OR x = v3)
```
Conversely, `x NOT IN (subquery)` translates via De Morgan's Laws to:
```sql
x NOT IN (v1, v2, v3)  <=>  (x <> v1 AND x <> v2 AND x <> v3)
```
If any value `vi` is `NULL`, `x <> NULL` evaluates to `UNKNOWN`. In SQL boolean logic:
```
TRUE AND UNKNOWN => UNKNOWN
FALSE AND UNKNOWN => FALSE
```
Because a `WHERE` clause filters out any row whose predicate does not evaluate to `TRUE`, a single `NULL` in the subquery causes `NOT IN` to reject **every single candidate row**.

### 3. The `ANY` / `SOME` Predicate
`ANY` (which has the exact standard synonym `SOME`) compares a scalar expression against every value returned by a single-column subquery using any comparison operator (`=`, `<>`, `<`, `<=`, `>`, `>=`):
- `x = ANY (subquery)` is semantically identical to `x IN (subquery)`.
- `x > ANY (subquery)` evaluates to `TRUE` if `x` is greater than the *minimum* value in the subquery.
- `x < ANY (subquery)` evaluates to `TRUE` if `x` is less than the *maximum* value in the subquery.

### 4. The `ALL` Predicate
`ALL` compares a scalar value against every row returned by a subquery and returns `TRUE` only if the comparison holds for *all* rows:
- `x > ALL (subquery)` evaluates to `TRUE` if `x` is greater than the *maximum* value in the subquery.
- `x < ALL (subquery)` evaluates to `TRUE` if `x` is less than the *minimum* value in the subquery.
- `x = ALL (subquery)` is only `TRUE` if all values in the subquery are identical to `x` (or if the subquery is empty).
- `x <> ALL (subquery)` is semantically identical to `x NOT IN (subquery)`.
- **The Vacuous Truth Trap**: If the subquery returns zero rows, `x = ALL (empty)` evaluates to `TRUE`!

---

## How It Works Internally

Modern relational engines (PostgreSQL, MySQL, SQL Server) employ sophisticated query optimizers that attempt to unnest or decorrelate subqueries into relational algebra operators before execution.

```
       [ Client Query: WHERE EXISTS (SELECT 1 FROM orders o WHERE o.customer_id = c.id) ]
                                            │
                                            ▼
                           [ Query Parser & Rewriter ]
                                            │
            Is subquery correlated? Can it be flattened into a Semi-Join?
                                            │
                     ┌──────────────────────┴──────────────────────┐
                     ▼                                             ▼
        [ Correlated Nested Loop ]                       [ Relational Semi-Join ]
        (Fallback / Unindexed)                          (Flattened & Cost-Based)
                     │                                             │
      For each row in Customers:                        Hash Join / Merge Join
      Execute Subquery Index Scan                       Inner table deduplicated
      Halt at FIRST tuple matched                       Processes in O(N + M) time
                     │                                             │
                     └──────────────────────┬──────────────────────┘
                                            ▼
                               [ Query Execution Engine ]
```

1. **Subquery Unnesting / Flattening**: The query analyzer inspects the subquery. If it detects `WHERE EXISTS (SELECT ...)` or `WHERE id IN (SELECT ...)`, it attempts to rewrite the query into an internal `Semi-Join` (`JoinType = JOIN_SEMI`).
2. **Semi-Join Execution**: Unlike an `INNER JOIN`, which duplicates outer rows when multiple matching inner rows exist, a `Semi-Join` emits the outer row upon encountering the **first** match and immediately advances to the next outer row. No row duplication occurs, eliminating the need for `DISTINCT`.
3. **Anti-Join Execution**: `NOT EXISTS` is transformed into an `Anti-Join` (`JoinType = JOIN_ANTI`). The engine searches for matching inner tuples; if none exist, the outer row is emitted.
4. **`NOT IN` Inhibition**: When `NOT IN` is used, the optimizer cannot easily rewrite the query to a standard Anti-Join unless the optimizer can prove through `NOT NULL` constraints that the subquery column can never produce a `NULL`. If the column is nullable, the engine is forced to execute an expensive SubPlan or Null-Aware Anti-Join (`NAAJ`), severely hurting performance.

---

## Syntax & Usage

### 1. `EXISTS` & `NOT EXISTS`
```sql
SELECT c.customer_id, c.full_name
FROM customers c
WHERE EXISTS (
    SELECT 1 
    FROM orders o 
    WHERE o.customer_id = c.customer_id 
      AND o.order_status = 'COMPLETED'
);
```

### 2. `IN` with Subquery
```sql
SELECT product_id, product_name, unit_price
FROM products
WHERE category_id IN (
    SELECT category_id 
    FROM categories 
    WHERE is_active = TRUE
);
```

### 3. `ANY` / `SOME` Comparison
```sql
-- Find employees earning more than at least one senior architect
SELECT employee_name, salary
FROM employees
WHERE salary > ANY (
    SELECT salary 
    FROM employees 
    WHERE job_title = 'Senior Architect'
);
```

### 4. `ALL` Comparison
```sql
-- Find employees earning more than ALL junior engineers
SELECT employee_name, salary
FROM employees
WHERE salary > ALL (
    SELECT salary 
    FROM employees 
    WHERE job_title = 'Junior Engineer'
);
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **`EXISTS` / `NOT EXISTS`** | Full ANSI support; decorrelates to Hash/Merge Semi/Anti Join | Full support; transforms to semi-join via materialization or duplicate weedout | Full support; translates to Correlated Subquery or Left Join search | Full support; transforms to Left Semi/Anti Join operator |
| **`IN` (Subquery)** | Flattens to Semi-Join; supports row-constructors `(a,b) IN (...)` | Flattens to Semi-Join; supports row-constructors `(a,b) IN (...)` | Flattens to subquery search; row-constructors supported since 3.15 | Flattens to Semi-Join; **No** row constructor syntax `(a,b) IN` |
| **`NOT IN` with Nulls** | Enforces 3VL; uses `SubPlan` unless column is `NOT NULL` | Enforces 3VL; materialization disabled if nulls possible | Enforces 3VL; scans subquery table per outer row | Enforces 3VL; uses `Null-Aware Anti-Join` operator |
| **`ANY` / `SOME`** | Standard subquery + array support: `x = ANY(arr)` | Supported for single-column subqueries | **Not Supported** (Must rewrite with `MIN`/`MAX` or `EXISTS`) | Supported for single-column subqueries |
| **`ALL`** | Standard single-column subquery + array support | Supported for single-column subqueries | **Not Supported** (Must rewrite with `MIN`/`MAX` or `NOT EXISTS`) | Supported for single-column subqueries |

---

## Practical Examples

### Example 1: Basic — `IN` vs `EXISTS` for Related Records
Retrieve all active customers who have placed at least one purchase order.

```sql
-- Schema setup
CREATE TABLE customers (
    customer_id INTEGER PRIMARY KEY,
    full_name VARCHAR(100) NOT NULL,
    account_status VARCHAR(20) NOT NULL
);

CREATE TABLE orders (
    order_id INTEGER PRIMARY KEY,
    customer_id INTEGER NOT NULL,
    total_amount NUMERIC(10,2) NOT NULL,
    order_date DATE NOT NULL,
    FOREIGN KEY (customer_id) REFERENCES customers(customer_id)
);

INSERT INTO customers (customer_id, full_name, account_status) VALUES
(1, 'Alice Smith', 'ACTIVE'),
(2, 'Bob Jones', 'ACTIVE'),
(3, 'Charlie Brown', 'SUSPENDED'),
(4, 'Diana Prince', 'ACTIVE');

INSERT INTO orders (order_id, customer_id, total_amount, order_date) VALUES
(101, 1, 150.00, '2026-01-10'),
(102, 1, 200.50, '2026-02-15'),
(103, 2, 75.25, '2026-03-01');

-- Query using EXISTS
SELECT c.customer_id, c.full_name
FROM customers c
WHERE c.account_status = 'ACTIVE'
  AND EXISTS (
      SELECT 1 
      FROM orders o 
      WHERE o.customer_id = c.customer_id
  );
```

```
Result:
 customer_id |  full_name  
-------------+-------------
           1 | Alice Smith
           2 | Bob Jones
(2 rows)
```

---

### Example 2: Real-World — Finding Inactive Customers via `NOT EXISTS`
Identify customers who have never placed an order or whose last order was placed more than 365 days ago, for an automated marketing re-engagement campaign.

```sql
SELECT c.customer_id, c.full_name
FROM customers c
WHERE c.account_status = 'ACTIVE'
  AND NOT EXISTS (
      SELECT 1 
      FROM orders o 
      WHERE o.customer_id = c.customer_id
        AND o.order_date >= DATE('2026-11-26', '-365 days')
  );
```

```
Result:
 customer_id |  full_name   
-------------+--------------
           4 | Diana Prince
(1 row)
```

---

### Example 3: Edge Case — Vacuous Truth of `ALL` on Empty Sets
In mathematical set theory, a universal quantifier over an empty domain is vacuously true. Observe what happens when evaluating `val > ALL (subquery)` when the subquery returns zero rows.

```sql
CREATE TABLE salary_benchmarks (
    tier VARCHAR(20) PRIMARY KEY,
    min_salary NUMERIC(10,2) NOT NULL
);

-- Note: Table is empty! Zero rows inserted.

-- Query testing ALL against empty subquery
SELECT c.customer_id, c.full_name
FROM customers c
WHERE 50000 > ALL (SELECT min_salary FROM salary_benchmarks);
```

```
Result:
 customer_id |   full_name   
-------------+---------------
           1 | Alice Smith
           2 | Bob Jones
           3 | Charlie Brown
           4 | Diana Prince
(4 rows)
```
*Key takeaway*: Because `salary_benchmarks` is empty, there is no value `v` such that `50000 > v` is false. Consequently, the condition evaluates to `TRUE` for every row in `customers`!

---

### Example 4: Failure Mode — The Fatal `NOT IN (NULL)` Trap
Demonstration of how a single NULL value inside a nullable column causes `NOT IN` to produce zero rows.

```sql
CREATE TABLE departments (
    dept_id INTEGER PRIMARY KEY,
    dept_name VARCHAR(50) NOT NULL,
    parent_dept_id INTEGER NULL
);

INSERT INTO departments (dept_id, dept_name, parent_dept_id) VALUES
(10, 'Executive', NULL),
(20, 'Engineering', 10),
(30, 'Sales', 10),
(40, 'Special Operations', NULL);

-- GOAL: Find all departments that are NOT a parent to any department
-- BUGGY QUERY: Using NOT IN
SELECT d1.dept_id, d1.dept_name
FROM departments d1
WHERE d1.dept_id NOT IN (
    SELECT d2.parent_dept_id 
    FROM departments d2
);
```

```
Result:
 dept_id | dept_name 
---------+-----------
(0 rows)
```

**Why it failed**: `SELECT parent_dept_id FROM departments` returns `{NULL, 10}`. Evaluating `10 NOT IN (NULL, 10)` becomes `(10 <> NULL) AND (10 <> 10)` -> `UNKNOWN AND FALSE` -> `FALSE`. Evaluating `20 NOT IN (NULL, 10)` becomes `(20 <> NULL) AND (20 <> 10)` -> `UNKNOWN AND TRUE` -> `UNKNOWN`. The query returns 0 rows!

**Corrected Query Using `NOT EXISTS`**:
```sql
SELECT d1.dept_id, d1.dept_name
FROM departments d1
WHERE NOT EXISTS (
    SELECT 1 
    FROM departments d2 
    WHERE d2.parent_dept_id = d1.dept_id
);
```

```
Result:
 dept_id |     dept_name      
---------+--------------------
      20 | Engineering
      30 | Sales
      40 | Special Operations
(3 rows)
```

---

### Example 5: Optimized — Row-Constructor Subquery Matching
Find inventory items whose combined `(warehouse_id, product_sku)` pair exists in the urgent reorder log.

```sql
CREATE TABLE warehouse_stock (
    warehouse_id INT NOT NULL,
    product_sku VARCHAR(30) NOT NULL,
    quantity_on_hand INT NOT NULL,
    PRIMARY KEY (warehouse_id, product_sku)
);

CREATE TABLE urgent_reorders (
    reorder_id INT PRIMARY KEY,
    warehouse_id INT NOT NULL,
    product_sku VARCHAR(30) NOT NULL,
    urgency_level VARCHAR(10) NOT NULL
);

INSERT INTO warehouse_stock VALUES 
(1, 'SKU-RED-1', 45), (1, 'SKU-BLU-2', 12), (2, 'SKU-RED-1', 5);

INSERT INTO urgent_reorders VALUES 
(1001, 1, 'SKU-BLU-2', 'CRITICAL'),
(1002, 2, 'SKU-RED-1', 'HIGH');

-- PostgreSQL / MySQL / SQLite row-constructor IN predicate
SELECT ws.warehouse_id, ws.product_sku, ws.quantity_on_hand
FROM warehouse_stock ws
WHERE (ws.warehouse_id, ws.product_sku) IN (
    SELECT ur.warehouse_id, ur.product_sku 
    FROM urgent_reorders ur 
    WHERE ur.urgency_level = 'CRITICAL'
);
```

```
Result:
 warehouse_id | product_sku | quantity_on_hand 
--------------+-------------+------------------
            1 | SKU-BLU-2   |               12
(1 row)
```

---

## Best Practices

1. **Default to `NOT EXISTS` over `NOT IN`**: `NOT EXISTS` is completely immune to `NULL` values in subquery columns and guarantees consistent two-valued logic.
2. **Use `SELECT 1` in `EXISTS`**: While the query planner ignores the select list inside an `EXISTS` subquery, writing `SELECT 1` clearly signals developer intent that the column projection is irrelevant.
3. **Index Correlated Columns**: Ensure foreign key and correlation columns in the inner table (e.g., `orders.customer_id`) have a B-Tree index. This allows the optimizer to transform `EXISTS` into an index lookup or index-backed semi-join.
4. **Avoid `ANY` / `ALL` for SQLite Portability**: Because SQLite does not support ANSI `ANY`/`ALL` subqueries, use `= ANY(...)` -> `IN (...)` and `> ALL(...)` -> `> (SELECT MAX(...))` if cross-database compatibility is required.
5. **Enforce `NOT NULL` on Foreign Keys**: If you choose to write `IN` or `NOT IN`, ensure the target column is defined with `NOT NULL` so the optimizer knows it can safely employ semi/anti-join transformations without checking for nulls.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Using `NOT IN` on a nullable subquery column | If the subquery yields even one `NULL`, `x <> NULL` evaluates to `UNKNOWN`, returning zero rows across the entire query. | Use `NOT EXISTS` with a correlation condition, or add `WHERE col IS NOT NULL` inside the subquery. |
| 2 | Believing `SELECT *` inside `EXISTS` incurs performance overhead | The SQL parser completely strips the projection list of an `EXISTS` subquery during parse analysis; it never retrieves columns. | `SELECT 1` is preferred for readability, but `SELECT *` carries zero execution penalty. |
| 3 | Misunderstanding `> ALL` on an empty set (vacuous truth) | If the subquery returns zero rows, `val > ALL (empty)` evaluates to `TRUE`, unexpectedly matching all rows. | Check subquery non-emptiness first, or rewrite using `COALESCE((SELECT MAX(...) FROM ...), default)`. |
| 4 | Using `IN` with massive subquery result sets | When the subquery returns millions of rows, `IN` can force materialization of large temporary hash tables. | Use `EXISTS` with an indexed correlation predicate, or perform an explicit `INNER JOIN` on a summarized CTE. |
| 5 | Writing `WHERE EXISTS (...)` without a correlation condition | Forgetting to link outer and inner tables (e.g., `o.customer_id = c.customer_id`) turns `EXISTS` into a static global flag matching all or zero rows. | Always ensure correlated parameters link the inner query to the outer row context. |
| 6 | Assuming `= ALL` matches any element in the list | `= ALL` requires the scalar value to be identical to *every single row* in the subquery. If the subquery has distinct values, it is always `FALSE`. | Use `= ANY` (or `IN`) to test if a scalar value matches *any* element in the list. |

---

## Performance & Security Notes

### Query Planner Semi-Join Transformations
Consider the following comparison in PostgreSQL 17:
```sql
-- Query A: IN Subquery
EXPLAIN ANALYZE
SELECT * FROM customers 
WHERE customer_id IN (SELECT customer_id FROM orders);

-- Query B: EXISTS Subquery
EXPLAIN ANALYZE
SELECT * FROM customers c
WHERE EXISTS (SELECT 1 FROM orders o WHERE o.customer_id = c.customer_id);
```
Both queries produce an identical execution plan:
```
Hash Semi Join  (cost=12.50..35.20 rows=150 width=68) (actual time=0.045..0.120 rows=128 loops=1)
  Hash Cond: (customers.customer_id = orders.customer_id)
  ->  Seq Scan on customers  (cost=0.00..15.50 rows=550 width=68)
  ->  Hash  (cost=10.00..10.00 rows=200 width=4)
        ->  Seq Scan on orders  (cost=0.00..10.00 rows=200 width=4)
```
In both cases, the cost-based optimizer rewrites the predicate into a `Hash Semi Join`. The inner table's keys are loaded into a temporary in-memory hash table. As soon as the outer table finds a hash bucket hit, it emits the customer row immediately without scanning further order records.

### SQL Injection Considerations in Dynamic `IN` Clauses
In application code (Node.js, Python, Java), dynamically constructing `IN (?, ?, ?)` clauses with variable numbers of parameters is a frequent vulnerability vector. 
- In PostgreSQL, pass an array parameter: `WHERE customer_id = ANY($1::int[])`. This requires only a single bind parameter regardless of list size.
- In MySQL / SQL Server, use table-valued parameters or parameterized JSON arrays (`JSON_TABLE` / `OPENJSON`).

---

## Comparison Matrix

| Dimension | `EXISTS` | `IN` | `ANY` / `SOME` | `ALL` |
| :--- | :--- | :--- | :--- | :--- |
| **Logic Model** | 2-Valued (`TRUE` / `FALSE`) | 3-Valued (`TRUE` / `FALSE` / `UNKNOWN`) | 3-Valued (`TRUE` / `FALSE` / `UNKNOWN`) | 3-Valued (`TRUE` / `FALSE` / `UNKNOWN`) |
| **Null Sensitivity** | Completely immune | Broken if subquery contains `NULL` in `NOT IN` | Yields `UNKNOWN` if no match and `NULL` exists | Yields `UNKNOWN` if comparison hits `NULL` |
| **Evaluation Scope** | Tuple existence (Projection ignored) | Value equality in set | Arbitrary operator (`<`, `>`, `=`, `<>`) | Arbitrary operator (`<`, `>`, `=`, `<>`) |
| **Execution Strategy** | Semi-Join / Anti-Join | Semi-Join (if `NOT NULL`) or SubPlan | Hash Join or Filter Scan | SubPlan or Aggregate Transformation |
| **Empty Subquery** | Returns `FALSE` | Returns `FALSE` | Returns `FALSE` | Returns **`TRUE`** (Vacuous Truth) |
| **Composite Keys** | Natural (via `WHERE a.k1 = b.k1 AND a.k2 = b.k2`) | Requires row constructors `(a, b) IN (...)` | Single column only | Single column only |

---

## Interview Questions & Deep Dives

### Q1: Why does `NOT IN` return 0 rows when the subquery contains a `NULL`, while `NOT EXISTS` does not?
<details>
<summary>Click to view answer</summary>

`x NOT IN (SELECT col FROM table)` is logically expanded using De Morgan's laws to:
`(x <> v1) AND (x <> v2) AND ... AND (x <> NULL)`.
Under ANSI Three-Valued Logic (3VL), comparing any value to `NULL` via `<>` yields `UNKNOWN`.
In SQL boolean algebra:
- `TRUE AND UNKNOWN => UNKNOWN`
- `FALSE AND UNKNOWN => FALSE`
Because a `WHERE` clause requires a predicate to evaluate strictly to boolean `TRUE` to keep a row, an evaluation of `UNKNOWN` causes the row to be discarded. Thus, if even a single `NULL` is returned by the subquery, no row can ever evaluate to `TRUE`.

`NOT EXISTS`, conversely, does not compare values; it evaluates the cardinality of the returned set. Even if the subquery returns a row containing `NULL`, that row counts as 1 tuple. If a matching tuple exists, `EXISTS` is `TRUE` and `NOT EXISTS` is `FALSE`. If zero matching tuples exist, `NOT EXISTS` is `TRUE`. `EXISTS` is strictly two-valued.
</details>

### Q2: Is `SELECT 1` faster than `SELECT *` inside an `EXISTS (...)` subquery?
<details>
<summary>Click to view answer</summary>

No, they have identical execution performance across all major enterprise RDBMS engines (PostgreSQL, MySQL, SQL Server, Oracle). During query compilation and semantic analysis, the optimizer identifies the `EXISTS` operator and completely discards the subquery's projection list (`SELECT` clause). The engine only needs to know whether the access path produces at least one tuple; it never constructs or reads the column buffers in the projection list. `SELECT 1` is conventionally preferred purely for readability and stylistic clarity.
</details>

### Q3: What is the semantic difference between `x <> ALL (SELECT ...)` and `x NOT IN (SELECT ...)`?
<details>
<summary>Click to view answer</summary>

They are semantically identical. `x <> ALL (s)` states that `x <> v` must be true for every value `v` in the set `s`. This expands to `(x <> s1 AND x <> s2 AND ...)`, which is the exact definition of `x NOT IN (s)`. Both share the exact same 3VL null-propagation vulnerabilities when `NULL` values exist in the target set.
</details>

### Q4: What happens when evaluating `SELECT * FROM tbl WHERE x > ALL (SELECT y FROM empty_table)`?
<details>
<summary>Click to view answer</summary>

It returns every row from `tbl`. In formal logic, this is known as a **vacuous truth**. Universal quantification (`ALL`) asserts: "for all elements `y` in set `S`, predicate `P(y)` holds." If set `S` is empty, there exists no element `y` that violates `P(y)`. Therefore, the condition evaluates to `TRUE` for all candidates.
</details>

### Q5: How does an optimizer execute a Semi-Join differently from an Inner Join?
<details>
<summary>Click to view answer</summary>

An `INNER JOIN` emits a Cartesian combination of matching rows. If a single customer matches 5 order rows, the customer data is repeated 5 times in the joined result set.
A `Semi-Join` emits the outer row upon finding the **first** match in the inner table and immediately stops scanning the inner table for that outer row. This guarantees:
1. No row duplication (eliminating the need for a costly `DISTINCT` sort/hash step).
2. Early termination (short-circuiting) on each outer tuple.
</details>

### Q6: Can `EXISTS` be used in a `SELECT` projection list instead of a `WHERE` clause?
<details>
<summary>Click to view answer</summary>

Yes. In PostgreSQL, MySQL, and SQLite, boolean expressions can be projected directly:
```sql
SELECT c.customer_id,
       EXISTS (SELECT 1 FROM orders o WHERE o.customer_id = c.customer_id) AS has_placed_orders
FROM customers c;
```
In SQL Server (T-SQL), where boolean is not a valid output data type, it must be wrapped inside a `CASE` expression:
```sql
SELECT c.customer_id,
       CASE WHEN EXISTS (SELECT 1 FROM orders o WHERE o.customer_id = c.customer_id) 
            THEN 1 ELSE 0 END AS has_placed_orders
FROM customers c;
```
</details>

### Q7: When is `IN` preferable to `EXISTS`?
<details>
<summary>Click to view answer</summary>

`IN` is preferable when:
1. Matching against a small, static, hard-coded list of literal constants (e.g., `WHERE status IN ('PAID', 'PENDING', 'SHIPPED')`).
2. The subquery returns a very small, independent, static lookup table where an in-memory hash lookup is cleaner and clearer to read than a correlated subquery.
</details>

### Q8: How can you pass an array of integers into an `IN` condition in PostgreSQL without string interpolation?
<details>
<summary>Click to view answer</summary>

In PostgreSQL, instead of dynamically generating `IN (?, ?, ?)`, use the `= ANY(...)` operator with a parameterized native array:
```sql
SELECT * FROM products WHERE product_id = ANY($1::int[]);
```
The application passes a single native array parameter `[101, 102, 103]`, preventing query plan cache bloat and eliminating SQL injection risks.
</details>

---

## Practice Exercises

### Exercise 1: Filter Customers with No Shipments
Given `customers(customer_id, name)` and `shipments(shipment_id, customer_id, delivery_status)`, write a query using `NOT EXISTS` to find all customers who have no shipments marked `'DELIVERED'`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT c.customer_id, c.name
FROM customers c
WHERE NOT EXISTS (
    SELECT 1 
    FROM shipments s 
    WHERE s.customer_id = c.customer_id 
      AND s.delivery_status = 'DELIVERED'
);
```
</details>

---

### Exercise 2: Identify Products Priced Above All Competitor Averages
Given `products(product_id, category_id, price)` and `competitor_prices(category_id, avg_price)`, find products whose price is strictly greater than `ALL` competitor average prices within their category.

<details>
<summary>Click to view solution</summary>

```sql
SELECT p.product_id, p.category_id, p.price
FROM products p
WHERE p.price > ALL (
    SELECT cp.avg_price 
    FROM competitor_prices cp 
    WHERE cp.category_id = p.category_id
);
```
</details>

---

### Exercise 3: Debug the `NOT IN` Null Bug
The following query is intended to find employees who do not manage any other employee, but it returns zero rows in production. Identify the defect and fix it using `NOT EXISTS`.
```sql
-- Buggy code:
SELECT employee_id, first_name 
FROM employees 
WHERE employee_id NOT IN (SELECT manager_id FROM employees);
```

<details>
<summary>Click to view solution</summary>

```sql
-- Fix: Subquery contains NULL for top-level managers (CEOs).
-- Correct implementation with NOT EXISTS:
SELECT e1.employee_id, e1.first_name
FROM employees e1
WHERE NOT EXISTS (
    SELECT 1 
    FROM employees e2 
    WHERE e2.manager_id = e1.employee_id
);
```
</details>

---

### Exercise 4: Multi-Column Matching
Given `order_items(order_id, product_id, quantity)` and `discontinued_products(product_id, supplier_id)`, find all `order_id` values that contain at least one discontinued product from supplier `99`. Write this using `EXISTS`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT DISTINCT oi.order_id
FROM order_items oi
WHERE EXISTS (
    SELECT 1 
    FROM discontinued_products dp 
    WHERE dp.product_id = oi.product_id 
      AND dp.supplier_id = 99
);
```
</details>

---

### Exercise 5: Equivalent `ANY` Transformation
Rewrite the following query using `ANY` instead of `EXISTS`:
```sql
SELECT p.product_name 
FROM products p 
WHERE EXISTS (
    SELECT 1 
    FROM flash_sale_items fsi 
    WHERE fsi.product_id = p.product_id
);
```

<details>
<summary>Click to view solution</summary>

```sql
SELECT product_name
FROM products
WHERE product_id = ANY (
    SELECT product_id 
    FROM flash_sale_items
);
```
</details>

---

### Exercise 6: Find Departments with at Least Two Active Projects
Given `departments(dept_id, dept_name)` and `projects(proj_id, dept_id, status)`, write an `EXISTS` query that returns departments having at least two distinct active projects without using `GROUP BY` in the outer query.

<details>
<summary>Click to view solution</summary>

```sql
SELECT d.dept_id, d.dept_name
FROM departments d
WHERE EXISTS (
    SELECT 1 
    FROM projects p1
    JOIN projects p2 ON p1.dept_id = p2.dept_id AND p1.proj_id < p2.proj_id
    WHERE p1.dept_id = d.dept_id 
      AND p1.status = 'ACTIVE' 
      AND p2.status = 'ACTIVE'
);
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                              SUBQUERY PREDICATE CHEATSHEET                              |
+-------------------+--------------------+------------------------+-----------------------+
| Predicate         | Logic Type         | Null Handling          | Planner Equivalent    |
+-------------------+--------------------+------------------------+-----------------------+
| EXISTS (sub)      | 2-Valued (Boolean) | Immune to subquery null| Hash / Merge SemiJoin |
| NOT EXISTS (sub)  | 2-Valued (Boolean) | Immune to subquery null| Hash / Merge AntiJoin |
| val IN (sub)      | 3-Valued (3VL)     | OK if match found      | SemiJoin / In-Hash    |
| val NOT IN (sub)  | 3-Valued (3VL)     | FAILS if ANY null exists| Null-Aware AntiJoin   |
| val = ANY (sub)   | 3-Valued (3VL)     | Identical to IN        | SemiJoin              |
| val > ALL (sub)   | 3-Valued (3VL)     | Vacuous TRUE if empty  | > (SELECT MAX(...))   |
+-------------------+--------------------+------------------------+-----------------------+
```

- **Golden Rule**: Always use `NOT EXISTS` instead of `NOT IN` when subquery columns can contain `NULL`.
- **Projection List**: Inside `EXISTS (...)`, the `SELECT` list is ignored by the engine. Write `SELECT 1` for stylistic clarity.
- **Empty Subquery Behavior**: `EXISTS` = `FALSE`; `IN` = `FALSE`; `ANY` = `FALSE`; **`ALL` = `TRUE`**.

---

## Further Reading
- PostgreSQL 17 Documentation: *Subquery Expressions (`EXISTS`, `IN`, `ANY/SOME`, `ALL`)*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 8.8 `Quantified comparison predicate` & Section 8.9 `Exists predicate`
- C.J. Date: *Database Design and Relational Theory: Normal Forms and All That Jazz* (Three-Valued Logic & Null Pitfalls)
- Bruce Momjian: *PostgreSQL Internals: Understanding Subquery Flattening and Semi-Joins*
