# Semi-Joins and Anti-Joins: Relational Algebra & Optimization

| Metadata | Details |
| :--- | :--- |
| **Topic** | Relational Semi-Joins ($\ltimes$), Anti-Joins ($\rhd$), Query Planner Decorrelation, and Deduplication |
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

In relational algebra, filtering records based on whether related records exist (or do not exist) in another table is formalized by two operators: the **Semi-Join** ($R \ltimes S$) and the **Anti-Join** ($R \rhd S$).

Despite their immense importance, SQL has no explicit `SEMI JOIN` or `ANTI JOIN` keyword in the `FROM` clause! Instead, SQL developers invoke them indirectly using declarative constructs:
- **Semi-Join**: Expressed via `EXISTS (subquery)` or `IN (subquery)`.
- **Anti-Join**: Expressed via `NOT EXISTS (subquery)`, `NOT IN (subquery)`, or `LEFT JOIN ... WHERE right.id IS NULL`.

A very common developer anti-pattern is using an `INNER JOIN` to filter rows and then slapping `SELECT DISTINCT` on top because the join duplicated parent rows. This is a severe performance mistake. An inner join forms combinations of tuples, duplicating rows, and `DISTINCT` forces the engine to perform a massive in-memory or on-disk sort/hash to remove those duplicates.

Mastering semi-joins and anti-joins allows you to express set membership natively, enabling query planners to halt inner scans on the first match (short-circuiting), preserve parent cardinality naturally, and eliminate expensive deduplication passes.

---

## Core Concepts

### 1. The Relational Semi-Join ($R \ltimes S$)
A semi-join returns all rows from relation $R$ for which there exists **at least one** matching row in relation $S$, based on a join condition:
- It returns **only** columns from $R$. Columns from $S$ are never projected.
- It **never duplicates** rows from $R$. If an outer row matches 10,000 inner rows, it is emitted exactly once.
- Relational definition: $R \ltimes S = \Pi_R(R \bowtie S)$.

### 2. The Relational Anti-Join ($R \rhd S$)
An anti-join returns all rows from relation $R$ for which there exists **zero** matching rows in relation $S$:
- It returns **only** columns from $R$.
- It is the exact complement of a semi-join: every row in $R$ belongs to either $R \ltimes S$ or $R \rhd S$.
- Relational definition: $R \rhd S = R - (R \ltimes S)$.

### 3. Physical Join Implementation Variants
Relational engines implement semi-joins and anti-joins using three core physical join strategies:
1. **Hash Semi-Join / Anti-Join**: The inner table $S$ is hashed into memory. The outer table $R$ streams past; upon the first hash bucket hit, $R$ is emitted (semi) or dropped (anti).
2. **Merge Semi-Join / Anti-Join**: Both tables are sorted. When a match is encountered, the engine advances the outer table immediately without searching the remainder of the inner table's tied keys.
3. **Nested Loop Semi-Join / Anti-Join**: For each outer row, the engine probes an index on the inner table. The seek terminates immediately upon encountering the first matching key.

---

## How It Works Internally

```
     [ Query: SELECT * FROM customers c WHERE EXISTS (SELECT 1 FROM orders o WHERE o.c_id = c.id) ]
                                            │
                                            ▼
                           [ Query Parser & Rewriter ]
                                            │
                    Subquery Flattening: Converts to JOIN_SEMI
                                            │
                                            ▼
                               [ Cost-Based Optimizer ]
                                            │
                    ┌───────────────────────┼───────────────────────┐
                    ▼                       ▼                       ▼
           [ Hash Semi Join ]      [ Merge Semi Join ]    [ Nested Loop Semi ]
             (Large unindexed)       (Pre-sorted input)      (Indexed foreign key)
                    │                       │                       │
           Inner table hashed.     Scans sorted streams.   Loops through outer.
           Outer row checked.      Emits row on 1st match. Index seek halts on
           Emits row on 1st hit.   Skips to next outer!    1st found tuple!
```

### Contrast with Inner Join + DISTINCT:
```
-- THE EXPENSIVE MISTAKE:
SELECT DISTINCT c.id, c.name
FROM customers c
JOIN orders o ON c.id = o.customer_id;

Execution Plan:
-> Unique / HashAggregate (Cost: 1540.00..1620.00)  <-- Expensive dedup sort!
     -> Hash Join (Cost: 120.00..1250.00)           <-- Emits 1,000,000 duplicated rows!
          -> Seq Scan on orders
          -> Seq Scan on customers

-- THE OPTIMAL SEMI-JOIN:
SELECT c.id, c.name
FROM customers c
WHERE EXISTS (SELECT 1 FROM orders o WHERE o.customer_id = c.id);

Execution Plan:
-> Hash Semi Join (Cost: 120.00..350.00)            <-- Emits only 5,000 unique customers directly!
     -> Seq Scan on customers
     -> Hash (orders)
```

---

## Syntax & Usage

### 1. Semi-Join via `EXISTS` (Preferred Standard)
```sql
SELECT c.customer_id, c.customer_name
FROM customers c
WHERE EXISTS (
    SELECT 1 
    FROM orders o 
    WHERE o.customer_id = c.customer_id 
      AND o.order_status = 'SHIPPED'
);
```

### 2. Semi-Join via `IN`
```sql
SELECT c.customer_id, c.customer_name
FROM customers c
WHERE c.customer_id IN (
    SELECT o.customer_id 
    FROM orders o 
    WHERE o.order_status = 'SHIPPED'
);
```

### 3. Anti-Join via `NOT EXISTS` (Recommended)
```sql
SELECT c.customer_id, c.customer_name
FROM customers c
WHERE NOT EXISTS (
    SELECT 1 
    FROM orders o 
    WHERE o.customer_id = c.customer_id
);
```

### 4. Anti-Join via `LEFT JOIN ... WHERE IS NULL`
```sql
SELECT c.customer_id, c.customer_name
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id
WHERE o.customer_id IS NULL;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **`EXISTS` Decorrelation** | Flattens to `Hash/Merge Semi Join` | Flattens via semi-join transform | Flattens or uses subquery index seek | Flattens to `Left Semi Join` |
| **`NOT EXISTS` Decorrelation**| Flattens to `Hash/Merge Anti Join` | Flattens to anti-join | Flattens to anti-join / index seek | Flattens to `Left Anti Semi Join` |
| **`LEFT JOIN ... IS NULL`** | Optimizer transforms directly to `Anti Join` | Transformed to anti-join | Transformed to anti-join | Transformed to `Left Anti Semi Join` |
| **`NOT IN` with Nullable Keys**| Forced `SubPlan` (3VL null checks) | Disables semi-join materialization | Scans subquery per row | `Null-Aware Anti Join` (NAAJ) |

---

## Practical Examples

### Example 1: Basic — Semi-Join Replacing Join + DISTINCT
Find all departments that currently employ at least one active engineer, without duplicating department rows.

```sql
CREATE TABLE depts (
    dept_id INT PRIMARY KEY,
    dept_name VARCHAR(50) NOT NULL
);

CREATE TABLE staff_members (
    staff_id INT PRIMARY KEY,
    dept_id INT NOT NULL,
    job_role VARCHAR(50) NOT NULL
);

INSERT INTO depts VALUES (1, 'Engineering'), (2, 'Sales'), (3, 'HR');

INSERT INTO staff_members VALUES
(101, 1, 'Senior Engineer'),
(102, 1, 'QA Engineer'),
(103, 1, 'DevOps Engineer'),
(104, 2, 'Account Executive');

-- Semi-Join query: Engineering matches 3 times, but is emitted ONCE.
SELECT d.dept_id, d.dept_name
FROM depts d
WHERE EXISTS (
    SELECT 1 
    FROM staff_members s 
    WHERE s.dept_id = d.dept_id 
      AND s.job_role LIKE '%Engineer%'
);
```

```
Result:
 dept_id |  dept_name  
---------+-------------
       1 | Engineering
(1 row)
```

---

### Example 2: Real-World — Finding Abandoned Shopping Carts via Anti-Join
Find users who created a shopping cart over 24 hours ago but never placed a corresponding checkout order.

```sql
CREATE TABLE user_carts (
    cart_id INT PRIMARY KEY,
    user_id INT NOT NULL,
    created_at TIMESTAMP NOT NULL
);

CREATE TABLE checkout_orders (
    order_id INT PRIMARY KEY,
    cart_id INT NOT NULL,
    payment_status VARCHAR(20) NOT NULL
);

INSERT INTO user_carts VALUES
(1, 1001, '2026-01-01 10:00:00'),
(2, 1002, '2026-01-01 11:00:00'),
(3, 1003, '2026-01-01 12:00:00');

INSERT INTO checkout_orders VALUES
(501, 1, 'COMPLETED'),
(502, 3, 'PENDING');

-- Anti-Join: Cart 2 has zero orders; Cart 3 has a pending order (not completed)
SELECT c.cart_id, c.user_id, c.created_at
FROM user_carts c
WHERE NOT EXISTS (
    SELECT 1 
    FROM checkout_orders o 
    WHERE o.cart_id = c.cart_id 
      AND o.payment_status = 'COMPLETED'
);
```

```
Result:
 cart_id | user_id |     created_at      
---------+---------+---------------------
       2 |    1002 | 2026-01-01 11:00:00
       3 |    1003 | 2026-01-01 12:00:00
(2 rows)
```

---

### Example 3: Edge Case — Anti-Join Performance: `NOT EXISTS` vs `LEFT JOIN ... IS NULL`
Demonstrating why `NOT EXISTS` is superior when columns are nullable.

```sql
CREATE TABLE parent_docs (
    doc_id INT PRIMARY KEY,
    title VARCHAR(50) NOT NULL
);

CREATE TABLE child_revisions (
    rev_id INT PRIMARY KEY,
    doc_id INT NULL, -- Nullable foreign key!
    rev_number INT NOT NULL
);

INSERT INTO parent_docs VALUES (1, 'Doc A'), (2, 'Doc B');
INSERT INTO child_revisions VALUES (10, 1, 1), (11, NULL, 0);

-- Pattern A: NOT EXISTS (Clear, direct, optimizer-friendly)
SELECT p.doc_id, p.title
FROM parent_docs p
WHERE NOT EXISTS (
    SELECT 1 FROM child_revisions r WHERE r.doc_id = p.doc_id
);

-- Pattern B: LEFT JOIN ... IS NULL (Can be ambiguous if right-hand side has natural NULLs)
SELECT p.doc_id, p.title
FROM parent_docs p
LEFT JOIN child_revisions r ON p.doc_id = r.doc_id
WHERE r.doc_id IS NULL;
```
Both queries return `Doc B`. However, in Pattern B, if the developer accidentally checks `WHERE r.rev_id IS NULL` on a column that naturally contains nulls, the logic silently breaks. `NOT EXISTS` never depends on child column nullability.

---

### Example 4: Failure Mode — The 3VL Crash of `NOT IN`
Demonstration of how `NOT IN` fails to act as an anti-join when the inner relation produces a `NULL`.

```sql
-- DANGEROUS / DEFECTIVE ANTI-JOIN:
SELECT p.doc_id, p.title
FROM parent_docs p
WHERE p.doc_id NOT IN (
    SELECT doc_id FROM child_revisions
);
```

```
Result:
 doc_id | title 
--------+-------
(0 rows)
```
**Why it failed**: `child_revisions` contains row `(11, NULL)`. The subquery produces `{1, NULL}`. For Doc 2, `2 NOT IN (1, NULL)` expands to `(2 <> 1) AND (2 <> NULL)` -> `TRUE AND UNKNOWN` -> `UNKNOWN`. The entire result set is discarded!

---

### Example 5: Optimized — Multi-Column Semi-Join
Match transaction line items whose combined `(sku, warehouse_id)` exists in the discounted clearance register.

```sql
CREATE TABLE inventory_items (
    sku VARCHAR(20) NOT NULL,
    warehouse_id INT NOT NULL,
    stock_count INT NOT NULL,
    PRIMARY KEY (sku, warehouse_id)
);

CREATE TABLE clearance_manifest (
    sku VARCHAR(20) NOT NULL,
    warehouse_id INT NOT NULL,
    discount_pct NUMERIC(4,2) NOT NULL,
    PRIMARY KEY (sku, warehouse_id)
);

INSERT INTO inventory_items VALUES ('SKU-1', 10, 50), ('SKU-1', 20, 30), ('SKU-2', 10, 10);
INSERT INTO clearance_manifest VALUES ('SKU-1', 10, 0.40);

-- Multi-column semi-join using EXISTS
SELECT ii.sku, ii.warehouse_id, ii.stock_count
FROM inventory_items ii
WHERE EXISTS (
    SELECT 1 
    FROM clearance_manifest cm 
    WHERE cm.sku = ii.sku 
      AND cm.warehouse_id = ii.warehouse_id
);
```

```
Result:
  sku  | warehouse_id | stock_count 
-------+--------------+-------------
 SKU-1 |           10 |          50
(1 row)
```

---

## Best Practices

1. **Stop Using `JOIN` + `DISTINCT` for Existence Checks**: If you do not need columns from the joined table, replace `JOIN` + `DISTINCT` with `WHERE EXISTS (...)`. It eliminates row duplication and avoids sorting overhead.
2. **Standardize on `NOT EXISTS` for Anti-Joins**: `NOT EXISTS` is immune to null values in child tables, communicates intent clearly, and transforms cleanly into Hash Anti-Joins.
3. **Avoid `NOT IN` for Subqueries**: A single `NULL` in the subquery breaks `NOT IN`. If you must use `NOT IN`, ensure the subquery explicitly filters `WHERE col IS NOT NULL`.
4. **Ensure B-Tree Indexes on Foreign Keys**: An index on the child table's foreign key (`child_table(parent_id)`) allows the optimizer to transform semi-joins into blazing-fast index seeks with early termination.
5. **Use Semi-Joins Before Multi-Table Aggregation**: When calculating parent metrics, filter parents via a semi-join first to avoid aggregating massive child tables unnecessarily.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Slapping `DISTINCT` on an `INNER JOIN` | Forces database to duplicate rows and then sort/hash them to remove duplicates. | Use `WHERE EXISTS (...)` (Semi-Join). |
| 2 | Using `NOT IN` on a nullable column | A single `NULL` in the subquery causes the entire query to return 0 rows. | Use `NOT EXISTS` (Anti-Join). |
| 3 | Projecting child columns in a semi-join | Semi-joins only return parent columns; attempting to select `orders.amount` in outer query fails. | Use a standard `INNER JOIN` if child columns are required. |
| 4 | Writing `LEFT JOIN ... WHERE child.col IS NULL` on nullable column | If `child.col` can naturally be NULL, false positives are returned. | Check the child table's primary key `WHERE child.pk IS NULL`, or use `NOT EXISTS`. |
| 5 | Forgetting the join predicate inside `EXISTS` | Forgetting `s.dept_id = d.dept_id` turns the semi-join into a global static boolean matching all or zero rows. | Always correlate the inner subquery with the outer table. |
| 6 | Believing `LEFT JOIN ... IS NULL` is faster than `NOT EXISTS` | In all modern enterprise engines (PostgreSQL, MySQL, SQL Server), both compile to the exact same Anti-Join execution plan. | Prefer `NOT EXISTS` for clarity and null-safety. |

---

## Performance & Security Notes

### Early Termination (Short-Circuiting)
The primary execution advantage of a Semi-Join is **early termination**:
- In an `INNER JOIN`, if customer ID `42` has 10,000 orders, the engine must scan and join all 10,000 orders.
- In a `Nested Loop Semi-Join`, the engine seeks the index for `customer_id = 42`. The moment it finds the **first** order, it immediately emits customer `42` and moves to the next customer. 9,999 order records are never read!
- In `Hash Semi-Join`, as soon as an outer key finds a match in the hash table, it outputs the row and skips further bucket comparisons.

---

## Comparison Matrix

| Dimension | `INNER JOIN` + `DISTINCT` | `EXISTS` (Semi-Join) | `NOT EXISTS` (Anti-Join) | `LEFT JOIN ... IS NULL` |
| :--- | :--- | :--- | :--- | :--- |
| **Relational Operator**| $\Pi_R(R \bowtie S)$ | $R \ltimes S$ | $R \rhd S$ | $R \rhd S$ |
| **Duplicates Parent Rows?**| Yes (Cleaned up by DISTINCT)| **Never** | **Never** | **Never** |
| **Deduplication Overhead** | High (Sort / HashAggregate) | Zero (Early termination) | Zero (Early termination) | Zero (Anti-Join plan) |
| **Null Safety** | N/A | Fully immune | Fully immune | Vulnerable if wrong column checked |
| **Projection Scope** | Can project from both tables | Parent table only | Parent table only | Parent table only |

---

## Interview Questions & Deep Dives

### Q1: What is the formal difference between a regular join and a semi-join in relational algebra?
<details>
<summary>Click to view answer</summary>

A regular join ($R \bowtie S$) computes the Cartesian product of $R$ and $S$ filtered by a predicate, returning composite tuples containing attributes from both $R$ and $S$; if a row in $R$ matches $M$ rows in $S$, it appears $M$ times in the output. 
A semi-join ($R \ltimes S$) returns only the attributes of relation $R$ for which there is at least one matching row in $S$. It never duplicates rows of $R$, regardless of how many matches exist in $S$.
</details>

### Q2: Why is `WHERE EXISTS (...)` generally preferable to `INNER JOIN ... SELECT DISTINCT`?
<details>
<summary>Click to view answer</summary>

1. **Efficiency**: `INNER JOIN` generates multiple intermediate rows for every match, and `DISTINCT` requires an additional sorting or hashing pass over the entire joined dataset to eliminate the duplicates it just created.
2. **Early Termination**: A semi-join (`EXISTS`) short-circuits on the inner table—the engine halts inspection of the inner table as soon as the first matching tuple is found.
3. **Clarity**: It signals developer intent directly: "filter $R$ based on the existence of records in $S$ without projecting $S$."
</details>

### Q3: How does the query planner execute an Anti-Join?
<details>
<summary>Click to view answer</summary>

An Anti-Join is typically executed as:
- **Hash Anti-Join**: Builds an in-memory hash table of the inner table's join keys. Streams the outer table; if an outer key is **not** present in the hash table, it is emitted.
- **Merge Anti-Join**: Streams both sorted inputs. If an outer key has no corresponding key in the inner stream, it is emitted.
- **Nested Loop Anti-Join**: Probes the inner index for each outer row. If the index seek returns 0 rows, the outer row is emitted.
</details>

### Q4: Is `LEFT JOIN ... WHERE right.id IS NULL` slower than `NOT EXISTS`?
<details>
<summary>Click to view answer</summary>

In modern database engines (PostgreSQL 9+, MySQL 8+, SQL Server 2012+), **no**. The cost-based query optimizer inspects the query tree, recognizes that the `LEFT JOIN` has a `WHERE right.id IS NULL` filter on a non-nullable key, and rewrites both queries into the exact same physical `Anti-Join` operator (`Hash Anti Join` or `Merge Anti Join`). However, `NOT EXISTS` is preferred for readability and null-safety.
</details>

### Q5: What is a Null-Aware Anti-Join (NAAJ)?
<details>
<summary>Click to view answer</summary>

When a developer writes `WHERE val NOT IN (SELECT col FROM ...)`, SQL Three-Valued Logic mandates that if *any* value in the subquery is `NULL`, the condition evaluates to `UNKNOWN` and returns 0 rows. A Null-Aware Anti-Join is a specialized physical operator implemented in engines (like Oracle and SQL Server) that tracks null presence in the inner set to guarantee strict 3VL semantics, often at significant performance penalty compared to a standard Anti-Join.
</details>

### Q6: Can a Semi-Join be performed across composite (multi-column) keys?
<details>
<summary>Click to view answer</summary>

Yes. In `EXISTS`, simply include multiple conditions in the correlation clause:
`WHERE EXISTS (SELECT 1 FROM s WHERE s.k1 = r.k1 AND s.k2 = r.k2)`.
Alternatively, PostgreSQL, MySQL, and SQLite support row constructors with `IN`:
`WHERE (r.k1, r.k2) IN (SELECT s.k1, s.k2 FROM s)`.
</details>

### Q7: What is an Anti-Semi-Join?
<details>
<summary>Click to view answer</summary>

"Anti-Semi-Join" is simply the technical, academic name for an Anti-Join ($R \rhd S$). In query execution plans (such as SQL Server execution plans), it is explicitly labeled as `Left Anti Semi Join`.
</details>

### Q8: What happens to a Semi-Join if the inner subquery returns `NULL` values?
<details>
<summary>Click to view answer</summary>

In `WHERE EXISTS (SELECT 1 FROM s WHERE s.id = r.id)`, if `s.id` contains `NULL`, `s.id = r.id` evaluates to `UNKNOWN`. Because `EXISTS` only matches tuples where the inner `WHERE` condition evaluates to `TRUE`, rows with `NULL` keys simply do not match and are ignored. `EXISTS` is completely immune to null errors.
</details>

---

## Practice Exercises

### Exercise 1: Identify Suppliers with Active Products
Given `suppliers(supplier_id, company_name)` and `products(product_id, supplier_id, is_active)`. Write a semi-join query returning all suppliers that have at least one active product, without using `DISTINCT`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT s.supplier_id, s.company_name
FROM suppliers s
WHERE EXISTS (
    SELECT 1 
    FROM products p 
    WHERE p.supplier_id = s.supplier_id 
      AND p.is_active = TRUE
);
```
</details>

---

### Exercise 2: Identify Unsold Products (Anti-Join)
Given `products(product_id, title)` and `order_items(item_id, product_id)`. Write an anti-join query returning all products that have never been ordered.

<details>
<summary>Click to view solution</summary>

```sql
SELECT p.product_id, p.title
FROM products p
WHERE NOT EXISTS (
    SELECT 1 
    FROM order_items oi 
    WHERE oi.product_id = p.product_id
);
```
</details>

---

### Exercise 3: Refactor Inefficient `DISTINCT` Query
Refactor the following query to eliminate the expensive `DISTINCT` sorting step:
```sql
SELECT DISTINCT a.author_id, a.author_name
FROM authors a
JOIN books b ON a.author_id = b.author_id
WHERE b.genre = 'Science Fiction';
```

<details>
<summary>Click to view solution</summary>

```sql
SELECT a.author_id, a.author_name
FROM authors a
WHERE EXISTS (
    SELECT 1 
    FROM books b 
    WHERE b.author_id = a.author_id 
      AND b.genre = 'Science Fiction'
);
```
</details>

---

### Exercise 4: Users with No Orders in the Last Year
Given `users(user_id, username)` and `orders(order_id, user_id, order_date)`. Find users who have placed zero orders since `'2025-01-01'`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT u.user_id, u.username
FROM users u
WHERE NOT EXISTS (
    SELECT 1 
    FROM orders o 
    WHERE o.user_id = u.user_id 
      AND o.order_date >= '2025-01-01'
);
```
</details>

---

### Exercise 5: Multi-Key Anti-Join
Given `warehouse_inventory(warehouse_id, sku, qty)` and `reorder_exceptions(warehouse_id, sku)`. Select all inventory records whose `(warehouse_id, sku)` pair does not exist in the exceptions table.

<details>
<summary>Click to view solution</summary>

```sql
SELECT wi.warehouse_id, wi.sku, wi.qty
FROM warehouse_inventory wi
WHERE NOT EXISTS (
    SELECT 1 
    FROM reorder_exceptions re 
    WHERE re.warehouse_id = wi.warehouse_id 
      AND re.sku = wi.sku
);
```
</details>

---

### Exercise 6: Explain Plan Verification
Given an execution plan showing:
```
Hash Semi Join  (cost=12.20..35.40 rows=200 width=32)
  Hash Cond: (c.customer_id = o.customer_id)
```
Explain what this operator did with the inner table and why it is faster than an inner join.

<details>
<summary>Click to view solution</summary>

1. **Inner Table Handling**: The engine loaded the unique `customer_id` keys from `orders` into an in-memory hash table.
2. **Execution Advantage**: As outer `customers` rows stream past, the engine probes the hash table. As soon as a matching hash entry is found, the customer row is emitted **immediately**, without scanning further order rows or generating duplicate records. This completely avoids row inflation and subsequent deduplication sorting.
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                               SEMI & ANTI JOIN CHEAT SHEET                              |
+-------------------+----------------------------+----------------------------------------+
| Relational Op     | SQL Idiom                  | Core Rule                              |
+-------------------+----------------------------+----------------------------------------+
| Semi-Join         | WHERE EXISTS (...)         | Emits parent row on 1st match; no dupes|
| Semi-Join         | WHERE id IN (...)          | Flattens to semi-join if not null      |
| Anti-Join         | WHERE NOT EXISTS (...)     | Emits parent if 0 matches; immune to null|
| Anti-Join         | LEFT JOIN ... IS NULL      | Emits parent if 0 matches; checks PK   |
+-------------------+----------------------------+----------------------------------------+
```

- **Stop Using `JOIN` + `DISTINCT`**: Replace with `EXISTS` to eliminate sorting overhead.
- **Early Termination**: Semi-joins halt scanning the inner relation upon finding the first match.
- **Null Safety**: Always use `NOT EXISTS` instead of `NOT IN` to prevent 3VL data loss.

---

## Further Reading
- PostgreSQL 17 Documentation: *Planner Methodologies - Subquery Unnesting and Semi-Joins*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 8.8 `<quantified comparison predicate>`
- Goetz Graefe: *Query Evaluation Techniques for Large Databases (ACM Computing Surveys)*
- Microsoft Learn: *Understanding Semi-Join and Anti-Semi-Join Execution Plans*
