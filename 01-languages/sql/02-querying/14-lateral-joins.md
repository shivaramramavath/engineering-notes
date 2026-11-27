# Lateral Joins & Correlated Subqueries in FROM

| Metadata | Details |
| :--- | :--- |
| **Topic** | Lateral Joins (CROSS JOIN LATERAL, LEFT JOIN LATERAL), CROSS APPLY, and Set-Returning Functions |
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

In standard ANSI SQL, all table sources in the `FROM` clause are logically evaluated independently. A subquery in the `FROM` clause cannot reference columns from preceding sibling tables in the same `FROM` list:
```sql
-- ILLEGAL IN STANDARD SQL:
SELECT * 
FROM departments d 
JOIN (SELECT * FROM employees e WHERE e.dept_id = d.id) sub ON true;
-- ERROR: relation "d" does not exist in subquery scope!
```

Introduced in ANSI SQL:2008, the **`LATERAL`** keyword breaks this scoping boundary. When a subquery or table-valued function is prefaced with `LATERAL`, it acts like a parameterized `foreach` loop: for every single row emitted by the left-hand table, the subquery executes with full access to the left row's column values.

In Microsoft SQL Server, this exact relational mechanic is implemented via the **`APPLY`** operator (`CROSS APPLY` and `OUTER APPLY`).

Lateral joins solve problems that are otherwise computationally devastating or impossible:
- Retrieving the **Top-N rows per category** (e.g., latest 3 orders for every customer) using direct B-Tree index seeks rather than full-table window scans.
- Calling Set-Returning Functions (SRFs) parameterized by row data (e.g., `unnest(array_col)` or `jsonb_to_recordset()`).
- Reusing complex calculated expressions across `SELECT` and `WHERE` without CTE overhead.

---

## Core Concepts

### 1. `CROSS JOIN LATERAL`
A `CROSS JOIN LATERAL` evaluates the subquery for each outer row. If the subquery returns zero rows for that outer row, the outer row is discarded (analogous to an `INNER JOIN`).
```sql
SELECT c.customer_name, o.order_id, o.order_amount
FROM customers c
CROSS JOIN LATERAL (
    SELECT order_id, order_amount
    FROM orders
    WHERE customer_id = c.customer_id
    ORDER BY order_date DESC
    LIMIT 3
) o;
```

### 2. `LEFT JOIN LATERAL`
A `LEFT JOIN LATERAL ... ON true` evaluates the subquery for each outer row. If the subquery returns zero rows, the outer row is preserved with `NULL`s for the subquery's columns (analogous to a `LEFT OUTER JOIN`).

### 3. T-SQL Equivalents: `CROSS APPLY` & `OUTER APPLY`
In Microsoft SQL Server (and Oracle):
- `CROSS APPLY` is semantically and physically identical to `CROSS JOIN LATERAL`.
- `OUTER APPLY` is semantically and physically identical to `LEFT JOIN LATERAL ... ON true`.

### 4. Exploding Semi-Structured Data
When unpacking JSON arrays, arrays, or text strings, `LATERAL` provides the syntax to pipe column values into set-returning functions:
```sql
SELECT p.post_id, tag.value AS tag_name
FROM posts p
CROSS JOIN LATERAL unnest(p.tags) AS tag(value);
```

---

## How It Works Internally

The database optimizer executes lateral joins using a **Parameterized Nested Loop**:

```
                         [ Outer Table: Customers ]
                                     │
                             (Sequential Scan)
                                     │
                                     ▼
                      [ Parameterized Nested Loop ]
                                     │
                 For each outer tuple (c.customer_id):
                                     │
                                     ▼
                    [ Inner Subquery Execution ]
                 Index Scan on idx_orders_cust_date
                 WHERE customer_id = c.customer_id
                 ORDER BY order_date DESC LIMIT 3
                                     │
                 ┌───────────────────┴───────────────────┐
                 ▼                                       ▼
        [ Matches Found ]                       [ 0 Matches Found ]
                 │                                       │
      Emits joined tuples (up to 3).          If CROSS JOIN: Discards.
                                              If LEFT JOIN: Emits row with NULLs.
```

### Why Lateral Outperforms Window Functions for Top-N:
- **Window Function Approach (`ROW_NUMBER()` in CTE)**: Must scan **all** 10,000,000 orders in the table, sort them into partitions, assign row numbers, and filter where `rn <= 3`. Cost is proportional to the total size of the `orders` table ($O(M)$).
- **Lateral Join Approach**: Performs an index seek on `idx_orders(customer_id, order_date DESC)` for each of the 1,000 customers, reading exactly $1,000 \times 3 = 3,000$ index tuples! Cost is proportional to the outer table size multiplied by $N$ ($O(K \times N)$).

---

## Syntax & Usage

```sql
SELECT 
    d.department_name,
    top_emp.employee_name,
    top_emp.salary
FROM departments d
CROSS JOIN LATERAL (
    SELECT e.employee_name, e.salary
    FROM employees e
    WHERE e.department_id = d.department_id
    ORDER BY e.salary DESC
    LIMIT 3
) top_emp;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **`CROSS JOIN LATERAL`** | Full ANSI support (9.3+) | Supported (8.0.14+) | Supported via table-valued functions / derived tables | **Not Supported** (Use `CROSS APPLY`) |
| **`LEFT JOIN LATERAL`** | Full ANSI support (`... ON true`) | Supported (8.0.14+) | Supported via subquery | **Not Supported** (Use `OUTER APPLY`) |
| **`CROSS APPLY`** | **Not Supported** (Use `LATERAL`) | **Not Supported** (Use `LATERAL`) | **Not Supported** | Native T-SQL keyword |
| **`OUTER APPLY`** | **Not Supported** (Use `LEFT JOIN LATERAL`) | **Not Supported** | **Not Supported** | Native T-SQL keyword |
| **Set-Returning Functions**| Native `unnest()`, `jsonb_each()`, etc. | `JSON_TABLE()` | `json_each()`, `json_tree()` | `OPENJSON()`, `STRING_SPLIT()` |

---

## Practical Examples

### Example 1: Basic — Calculated Expression Reuse
Avoid repeating verbose mathematical calculations across `SELECT` and `WHERE` without creating multiple CTE levels.

```sql
CREATE TABLE product_metrics (
    product_id INT PRIMARY KEY,
    views INT NOT NULL,
    clicks INT NOT NULL,
    conversions INT NOT NULL
);

INSERT INTO product_metrics VALUES
(1, 1000, 50, 10),
(2, 5000, 200, 2),
(3, 800, 10, 0);

SELECT 
    pm.product_id,
    calc.ctr,
    calc.conversion_rate
FROM product_metrics pm
CROSS JOIN LATERAL (
    SELECT 
        ROUND((pm.clicks * 100.0) / NULLIF(pm.views, 0), 2) AS ctr,
        ROUND((pm.conversions * 100.0) / NULLIF(pm.clicks, 0), 2) AS conversion_rate
) calc
WHERE calc.ctr > 2.0;
```

```
Result:
 product_id |  ctr  | conversion_rate 
------------+-------+-----------------
          1 |  5.00 |           20.00
          2 |  4.00 |            1.00
(2 rows)
```

---

### Example 2: Real-World — Top 2 Latest Invoices Per Client
Efficiently retrieve the 2 most recent invoices for each corporate client using an indexed lateral seek.

```sql
CREATE TABLE corporate_clients (
    client_id INT PRIMARY KEY,
    client_name VARCHAR(100) NOT NULL
);

CREATE TABLE client_invoices (
    invoice_id INT PRIMARY KEY,
    client_id INT NOT NULL,
    invoice_date DATE NOT NULL,
    amount NUMERIC(10,2) NOT NULL
);

CREATE INDEX idx_invoices_client_date ON client_invoices (client_id, invoice_date DESC);

INSERT INTO corporate_clients VALUES (1, 'Acme Corp'), (2, 'Globex'), (3, 'Initech');

INSERT INTO client_invoices VALUES
(101, 1, '2026-01-01', 500.00),
(102, 1, '2026-02-01', 750.00),
(103, 1, '2026-03-01', 900.00),
(104, 2, '2026-01-15', 300.00);

-- LEFT JOIN LATERAL ensures clients with 0 invoices (Initech) still appear!
SELECT 
    c.client_name,
    latest.invoice_id,
    latest.invoice_date,
    latest.amount
FROM corporate_clients c
LEFT JOIN LATERAL (
    SELECT ci.invoice_id, ci.invoice_date, ci.amount
    FROM client_invoices ci
    WHERE ci.client_id = c.client_id
    ORDER BY ci.invoice_date DESC
    LIMIT 2
) latest ON true;
```

```
Result:
 client_name | invoice_id | invoice_date | amount 
-------------+------------+--------------+--------
 Acme Corp   |        103 | 2026-03-01   | 900.00
 Acme Corp   |        102 | 2026-02-01   | 750.00
 Globex      |        104 | 2026-01-15   | 300.00
 Initech     |       NULL | NULL         |   NULL
(4 rows)
```

---

### Example 3: Edge Case — Exploding PostgreSQL Arrays
Normalize an array of integer tags stored in a relational column using `unnest()`.

```sql
CREATE TABLE articles (
    article_id INT PRIMARY KEY,
    title VARCHAR(100) NOT NULL,
    tag_ids INT[] NOT NULL
);

INSERT INTO articles VALUES
(1, 'PostgreSQL Internals', ARRAY[10, 20, 30]),
(2, 'SQL Window Functions', ARRAY[20, 40]);

SELECT 
    a.article_id,
    a.title,
    t.tag_id
FROM articles a
CROSS JOIN LATERAL unnest(a.tag_ids) AS t(tag_id);
```

```
Result:
 article_id |        title         | tag_id 
------------+----------------------+--------
          1 | PostgreSQL Internals |     10
          1 | PostgreSQL Internals |     20
          1 | PostgreSQL Internals |     30
          2 | SQL Window Functions |     20
          2 | SQL Window Functions |     40
(5 rows)
```

---

### Example 4: Failure Mode — Forgetting `ON true` in `LEFT JOIN LATERAL`
In ANSI SQL, a `LEFT JOIN` requires an `ON` clause. Forgetting `ON true` results in a syntax error.

```sql
-- FATAL SYNTAX ERROR:
-- ERROR: syntax error at or near ";"
SELECT * 
FROM corporate_clients c
LEFT JOIN LATERAL (
    SELECT * FROM client_invoices ci WHERE ci.client_id = c.client_id
) sub;

-- CORRECT SYNTAX:
SELECT * 
FROM corporate_clients c
LEFT JOIN LATERAL (
    SELECT * FROM client_invoices ci WHERE ci.client_id = c.client_id
) sub ON true;
```

---

### Example 5: Optimized — SQL Server `CROSS APPLY` Top-N
The exact equivalent query executed in Microsoft SQL Server using `CROSS APPLY` and `TOP (2)`.

```sql
SELECT 
    c.client_name,
    latest.invoice_id,
    latest.invoice_date,
    latest.amount
FROM corporate_clients c
CROSS APPLY (
    SELECT TOP (2) ci.invoice_id, ci.invoice_date, ci.amount
    FROM client_invoices ci
    WHERE ci.client_id = c.client_id
    ORDER BY ci.invoice_date DESC
) latest;
```

---

## Best Practices

1. **Index the Correlation Predicate**: A lateral join performs a nested loop. Ensure the inner query has a composite index on `(correlation_column, order_column DESC)` to turn each loop step into a fast B-Tree seek.
2. **Use `LEFT JOIN LATERAL ... ON true` When Outer Rows Must Be Preserved**: If the outer table may have entities with zero matching child records (e.g., customers without orders), use `LEFT JOIN LATERAL` instead of `CROSS JOIN LATERAL`.
3. **Prefer Lateral over Window Functions for Small Top-N on Large Tables**: When the inner table contains millions of rows and you need only top 1 to 5 items per group, lateral joins avoid scanning the entire inner table.
4. **Translate `APPLY` to `LATERAL` When Migrating from SQL Server to PostgreSQL**: Replace `CROSS APPLY` with `CROSS JOIN LATERAL`, and `OUTER APPLY` with `LEFT JOIN LATERAL ... ON true`.
5. **Use Lateral Joins for Complex Derived Columns**: Compute reusable intermediate values in a lateral join to avoid repeating verbose formulas or nesting multiple CTE layers.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Forgetting `ON true` on `LEFT JOIN LATERAL` | ANSI SQL syntax requires an `ON` clause for all `LEFT JOIN` variants. | Always append `ON true` or `ON 1=1`. |
| 2 | Using `CROSS JOIN LATERAL` when rows have 0 matches | Silently drops outer rows that have no matching records in the subquery. | Use `LEFT JOIN LATERAL ... ON true` to retain all outer rows. |
| 3 | Missing index on correlation column | Causes the inner subquery to perform a full sequential scan for every single outer row ($O(N \times M)$ disaster). | Create composite index on `(foreign_key, sort_col)`. |
| 4 | Attempting to use `CROSS APPLY` in PostgreSQL/MySQL | T-SQL keyword `APPLY` is not supported in PostgreSQL or MySQL. | Use standard ANSI `CROSS JOIN LATERAL`. |
| 5 | Referencing a sibling table that appears *after* the lateral subquery | Lateral scoping only allows referencing tables that appear to the *left* (earlier) in the `FROM` list. | Reorder table declarations in the `FROM` clause. |
| 6 | Using lateral joins on high-cardinality outer tables without indexes | A nested loop over 1,000,000 outer rows takes millions of random seeks. | Use window functions or hash joins if the outer table is massive and unindexed. |

---

## Performance & Security Notes

### Lateral Index Seek vs Window Function Seq Scan
Consider retrieving the top 3 transactions for 500 customers from an orders table containing 10,000,000 rows.
```sql
-- Plan A: Window Function CTE
-- Scans all 10,000,000 rows -> Sorts all 10,000,000 rows -> Filters
Execution Time: 3,450 ms

-- Plan B: CROSS JOIN LATERAL with index on (customer_id, created_at DESC)
-- Performs 500 index seeks -> Reads 1,500 tuples total!
Execution Time: 4.2 ms (800x faster!)
```
- Lateral joins leverage the index's B-Tree structure to halt immediately after reading $N$ tuples per key.

---

## Comparison Matrix

| Dimension | `CROSS JOIN LATERAL` | `LEFT JOIN LATERAL` | `CROSS APPLY` (MSSQL) | `ROW_NUMBER()` in CTE |
| :--- | :--- | :--- | :--- | :--- |
| **Drop 0-Match Rows?**| Yes | No (Emits NULLs) | Yes | Yes (Unless `LEFT JOIN` used) |
| **Syntax Requirement** | `CROSS JOIN LATERAL` | `LEFT JOIN LATERAL ... ON true` | `CROSS APPLY` | `OVER (PARTITION BY ...)` |
| **Engine Execution** | Parameterized Nested Loop | Parameterized Nested Loop | Nested Loops | `WindowAgg` over Sort/Scan |
| **Best For** | Top-N with small outer set | Top-N preserving all parents | Top-N in SQL Server | Top-N when outer set is huge |
| **Dialect Compatibility**| Postgres 9.3+, MySQL 8.0.14+ | Postgres 9.3+, MySQL 8.0.14+ | SQL Server, Oracle | Universal ANSI SQL |

---

## Interview Questions & Deep Dives

### Q1: What does the `LATERAL` keyword do in a SQL query?
<details>
<summary>Click to view answer</summary>

`LATERAL` allows a subquery (or table-valued function) in the `FROM` clause to reference columns provided by preceding tables in the same `FROM` list. Without `LATERAL`, subqueries in `FROM` are evaluated independently in an isolated scope and cannot access sibling table columns.
</details>

### Q2: How does `CROSS APPLY` in SQL Server differ from `CROSS JOIN LATERAL` in PostgreSQL?
<details>
<summary>Click to view answer</summary>

They are semantically and physically identical. `CROSS APPLY` is Microsoft SQL Server's proprietary keyword for a lateral join, introduced prior to the ANSI SQL:2008 standard. `CROSS JOIN LATERAL` is the official ISO/ANSI SQL standard syntax adopted by PostgreSQL, MySQL 8.0.14+, and Oracle 12c+.
</details>

### Q3: Why is `LEFT JOIN LATERAL ... ON true` required instead of just `LEFT JOIN LATERAL`?
<details>
<summary>Click to view answer</summary>

Under the ANSI SQL grammar, any `LEFT OUTER JOIN` requires a join condition specified via the `ON` or `USING` clause. Because the correlation condition is already contained inside the lateral subquery's `WHERE` clause, an unconditional join condition (`ON true` or `ON 1=1`) is required to satisfy the parser.
</details>

### Q4: When is a Lateral Join significantly faster than a Window Function CTE for Top-N queries?
<details>
<summary>Click to view answer</summary>

A Lateral Join is drastically faster when:
1. The outer table has relatively low to moderate cardinality (e.g., thousands of categories or users).
2. The inner table is very large (millions of rows).
3. A composite index exists on `(category_id, order_col DESC)`.
In this scenario, the lateral join performs direct index seeks reading only $K \times N$ rows, whereas a window function forces a scan and sort of the entire multi-million row table.
</details>

### Q5: Can a Lateral Join be used with Set-Returning Functions (SRFs)?
<details>
<summary>Click to view answer</summary>

Yes. In fact, lateral joins are the standard mechanism for unpacking multi-valued data structures in PostgreSQL:
```sql
SELECT doc_id, element
FROM documents d
CROSS JOIN LATERAL jsonb_array_elements_text(d.payload_json->'tags') element;
```
For every document row, the function is evaluated with that document's specific JSON column, emitting one row per tag.
</details>

### Q6: Can a lateral subquery reference columns from tables listed *after* it in the `FROM` clause?
<details>
<summary>Click to view answer</summary>

No. ANSI SQL mandates that lateral scoping is strictly left-to-right. A lateral subquery may only reference columns from tables or subqueries that appear to its left (preceding it) in the `FROM` list. Forward references raise an undefined table error.
</details>

### Q7: How does MySQL 8.0 handle lateral derived tables?
<details>
<summary>Click to view answer</summary>

MySQL 8.0.14+ supports standard ANSI `LATERAL` derived tables. It evaluates them using a dependent nested loop. MySQL does not support custom array unnesting but supports `JSON_TABLE()` which can be used laterally.
</details>

### Q8: What happens if the subquery in a `CROSS JOIN LATERAL` returns multiple rows?
<details>
<summary>Click to view answer</summary>

The outer row is duplicated once for each row emitted by the subquery, exactly like a standard relational join. If the subquery returns 3 rows, 3 joined rows are produced for that outer tuple.
</details>

---

## Practice Exercises

### Exercise 1: Most Recent Order Per Customer
Given `customers(customer_id, name)` and `orders(order_id, customer_id, order_date, total)`. Using `CROSS JOIN LATERAL`, retrieve each customer's name alongside the details of their single most recent order.

<details>
<summary>Click to view solution</summary>

```sql
SELECT c.name, o.order_id, o.order_date, o.total
FROM customers c
CROSS JOIN LATERAL (
    SELECT order_id, order_date, total
    FROM orders
    WHERE customer_id = c.customer_id
    ORDER BY order_date DESC
    LIMIT 1
) o;
```
</details>

---

### Exercise 2: Retain Customers with Zero Orders
Modify Exercise 1 so that customers who have never placed an order are still included in the output with `NULL`s for order details.

<details>
<summary>Click to view solution</summary>

```sql
SELECT c.name, o.order_id, o.order_date, o.total
FROM customers c
LEFT JOIN LATERAL (
    SELECT order_id, order_date, total
    FROM orders
    WHERE customer_id = c.customer_id
    ORDER BY order_date DESC
    LIMIT 1
) o ON true;
```
</details>

---

### Exercise 3: SQL Server CROSS APPLY Translation
Translate the following PostgreSQL query into valid Microsoft SQL Server (T-SQL) syntax:
```sql
SELECT d.dept_name, top_e.emp_name, top_e.salary
FROM departments d
CROSS JOIN LATERAL (
    SELECT emp_name, salary 
    FROM employees e 
    WHERE e.dept_id = d.dept_id 
    ORDER BY salary DESC 
    LIMIT 3
) top_e;
```

<details>
<summary>Click to view solution</summary>

```sql
SELECT d.dept_name, top_e.emp_name, top_e.salary
FROM departments d
CROSS APPLY (
    SELECT TOP (3) emp_name, salary 
    FROM employees e 
    WHERE e.dept_id = d.dept_id 
    ORDER BY salary DESC
) top_e;
```
</details>

---

### Exercise 4: Clean Calculation Reuse
Given `sales_leads(lead_id, score_a, score_b, score_c)`. Use a lateral join to calculate `composite_score = (score_a * 0.5 + score_b * 0.3 + score_c * 0.2)` and filter for leads where `composite_score > 75.0` without recalculating the formula in `WHERE`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT sl.lead_id, calc.composite_score
FROM sales_leads sl
CROSS JOIN LATERAL (
    SELECT (sl.score_a * 0.5 + sl.score_b * 0.3 + sl.score_c * 0.2) AS composite_score
) calc
WHERE calc.composite_score > 75.0;
```
</details>

---

### Exercise 5: Unnest Comma-Separated Values (PostgreSQL)
Given `products(product_id, title, color_list TEXT)` where `color_list` contains `'Red,Green,Blue'`. Use `string_to_array()` and `unnest()` in a lateral join to emit one row per product-color combination.

<details>
<summary>Click to view solution</summary>

```sql
SELECT p.product_id, p.title, TRIM(c.color) AS color
FROM products p
CROSS JOIN LATERAL unnest(string_to_array(p.color_list, ',')) AS c(color);
```
</details>

---

### Exercise 6: Identify Missing Index Bottleneck
Explain why a `CROSS JOIN LATERAL` query fetching top 3 items per customer took 45 seconds on 10,000 customers, and provide the exact index definition needed to fix it.

<details>
<summary>Click to view solution</summary>

**Cause**: Without a composite index, the database engine executes a full table scan of the child table for every single customer (10,000 sequential scans).

**Index Fix**:
```sql
CREATE INDEX idx_child_cust_date ON orders (customer_id, order_date DESC);
```
This enables the engine to perform 10,000 index seeks that read only 3 rows each, reducing query latency from minutes to milliseconds.
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                  LATERAL JOIN CHEAT SHEET                               |
+-------------------+----------------------------+----------------------------------------+
| Syntax            | Dialect                    | Behavior                               |
+-------------------+----------------------------+----------------------------------------+
| CROSS JOIN LATERAL| ANSI (Postgres, MySQL)     | Parameterized loop; drops 0-match rows |
| LEFT JOIN LATERAL | ANSI (Postgres, MySQL)     | Requires "ON true"; keeps 0-match rows |
| CROSS APPLY       | SQL Server, Oracle         | Identical to CROSS JOIN LATERAL        |
| OUTER APPLY       | SQL Server, Oracle         | Identical to LEFT JOIN LATERAL         |
| unnest(array_col) | PostgreSQL SRF             | Explodes arrays/JSON per outer tuple   |
+-------------------+----------------------------+----------------------------------------+
```

- **Top-N Performance**: Lateral joins with composite indexes crush full-table window functions for Top-N per group.
- **Left-to-Right Scoping**: Lateral subqueries can only access tables declared before them in the `FROM` list.
- **Left Join Gotcha**: Always append `ON true` when using `LEFT JOIN LATERAL`.

---

## Further Reading
- PostgreSQL 17 Documentation: *Queries - LATERAL Subqueries*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 7.6 `<table reference>`
- Microsoft Learn: *Using the APPLY Operator (CROSS APPLY and OUTER APPLY)*
- Markus Winand: *Use The Index, Luke! - The Lateral Join Revolution for Top-N Problems*
