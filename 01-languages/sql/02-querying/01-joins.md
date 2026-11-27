# 01: Relational Joins: INNER, OUTER & CROSS

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [01-fundamentals/04-select-statement.md](../01-fundamentals/04-select-statement.md) |
| **Target Audience** | Software Engineers, Data Engineers, Database Architects |
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

In relational database design, data is normalized across discrete, specialized tables (e.g., `customers`, `orders`, `order_items`, `products`) to eliminate redundancy, enforce integrity, and avoid update anomalies. However, business queries rarely need data from a single isolated table; they require answering multi-dimensional questions like: *"Which customers in Germany purchased laptops last quarter?"*

The **JOIN** operation is the relational operator that reconstructs composite facts across related tables by matching attributes on common keys. In relational algebra, a join is defined as a filtered **Cartesian Product**: combining tuples from relation $R$ and relation $S$ that satisfy a join predicate $\theta$ ($R \bowtie_\theta S$).

Understanding the behavioral distinctions between join types:
- **`INNER JOIN`**: Retains only matching rows from both relations.
- **`LEFT OUTER JOIN`**: Retains all rows from the left relation, null-padding non-matching right rows.
- **`RIGHT OUTER JOIN`**: Retains all rows from the right relation, null-padding non-matching left rows.
- **`FULL OUTER JOIN`**: Retains all rows from both relations, matching where possible and null-padding elsewhere.
- **`CROSS JOIN`**: Produces the unfiltered Cartesian product ($N \times M$ rows).

Mastering joins—along with how `ON` predicates differ from `WHERE` filters and how physical join algorithms execute in memory—is the defining prerequisite for writing analytical and transactional SQL.

---

## 2. Core Concepts

### 1. Visualizing Relational Joins

```
Table A: Customers                   Table B: Orders
+----+-------+                       +----------+---------+--------+
| id | name  |                       | order_id | cust_id | amount |
+----+-------+                       +----------+---------+--------+
|  1 | Alice |                       |      101 |       1 |  50.00 |
|  2 | Bob   |                       |      102 |       1 |  75.00 |
|  3 | Carol |                       |      103 |       4 |  90.00 |
+----+-------+                       +----------+---------+--------+

1. INNER JOIN (Only matches between A and B):
   Returns: Alice (101, 50.00), Alice (102, 75.00). [Bob and Carol omitted; Order 103 omitted].

2. LEFT OUTER JOIN (All A + matching B):
   Returns: Alice (101), Alice (102), Bob (NULL), Carol (NULL).

3. RIGHT OUTER JOIN (All B + matching A):
   Returns: Alice (101), Alice (102), NULL (103).

4. FULL OUTER JOIN (All A + All B):
   Returns: Alice (101), Alice (102), Bob (NULL), Carol (NULL), NULL (103).

5. CROSS JOIN (Cartesian product: 3 * 3 = 9 rows):
   Pairs every row in A with every row in B.
```

### 2. The `ON` Predicate vs The `WHERE` Filter
Understanding the execution timing difference between `ON` and `WHERE` is critical:
- **`ON` clause**: Evaluated *during Phase 1 (`FROM`)* to determine which rows in the right relation match the left relation.
  - In a `LEFT JOIN`, if the `ON` condition evaluates to false, the left row is **still emitted**, padded with `NULL`s for the right columns.
- **`WHERE` clause**: Evaluated *in Phase 2*, **after** the join has already been formed.
  - If a condition on the right table is placed in the `WHERE` clause (e.g., `WHERE b.status = 'active'`), any unmatched row that was null-padded by the `LEFT JOIN` will evaluate to `NULL = 'active'` $\rightarrow$ `UNKNOWN`, which the `WHERE` clause **discards**.
  - **Placing right-table filters in `WHERE` silently converts a `LEFT JOIN` into an `INNER JOIN`!**

### 3. Join Syntax: `ON` vs `USING` vs `NATURAL`
- **`ON a.id = b.cust_id`**: The universal, explicit standard. Flexible across differently named columns.
- **`USING (customer_id)`**: Concise standard syntax when both tables share the exact same column name. Merges the two columns into a single unified output column.
- **`NATURAL JOIN` (Anti-Pattern)**: Implicitly joins across *all* columns sharing identical names. Extremely fragile in production: adding a common metadata column (like `created_at` or `status`) to both tables silently alters the join condition, corrupting query results!

---

## 3. How It Works Internally

### Physical Join Algorithms in the Query Engine

The query optimizer evaluates costs to choose between three physical join strategies:

```
+--------------------------------------------------------------------------+
|                  Physical Join Algorithms in Query Engines               |
+--------------------+-----------------------------------------------------+
| 1. Nested Loop Join| For each row in outer table R:                      |
|                    |     Scan inner table S (ideally using B-Tree index) |
|                    |     for matching join keys.                         |
|                    | Cost: O(R * log S) with index. Ultra-fast for small |
|                    | outer tables. Catastrophic O(R * S) if unindexed!   |
+--------------------+-----------------------------------------------------+
| 2. Hash Join       | Phase 1 (Build): Read smaller table R, build        |
|                    |                  in-memory hash table in work_mem.  |
|                    | Phase 2 (Probe): Stream larger table S, hashing join|
|                    |                  key and probing the hash table.    |
|                    | Cost: O(R + S). Memory-intensive. Equality joins    |
|                    | only (=).                                           |
+--------------------+-----------------------------------------------------+
| 3. Merge Join      | Pre-condition: Both relations MUST be sorted on     |
|                    |                the join key!                        |
|                    | Step: Sequentially step through both sorted streams |
|                    |       in tandem, emitting matches.                  |
|                    | Cost: O(R + S). Low memory overhead. Ideal when data|
|                    | is pre-sorted by B-Tree indexes.                    |
+--------------------+-----------------------------------------------------+
```

---

## 4. Syntax & Usage

### Standard ANSI SQL Join Statements

```sql
-- 1. Explicit INNER JOIN
SELECT 
    c.customer_id,
    c.first_name || ' ' || c.last_name AS customer_name,
    o.order_id,
    o.total_amount
FROM customers c
INNER JOIN orders o 
  ON c.customer_id = o.customer_id;

-- 2. LEFT OUTER JOIN with USING syntax
SELECT 
    customer_id,
    c.email,
    o.order_id,
    o.order_status
FROM customers c
LEFT JOIN orders o USING (customer_id);

-- 3. FULL OUTER JOIN
SELECT 
    p.product_id,
    p.product_name,
    oi.order_id,
    oi.quantity
FROM products p
FULL OUTER JOIN order_items oi 
  ON p.product_id = oi.product_id;

-- 4. CROSS JOIN
SELECT 
    s.size_name,
    c.color_name
FROM sizes s
CROSS JOIN colors c;
```

### Multi-Dialect Comparison Matrix

| Join Capability | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **`INNER JOIN`** | Full support | Full support | Full support | Full support |
| **`LEFT OUTER JOIN`** | Full support | Full support | Full support | Full support |
| **`RIGHT OUTER JOIN`** | Full support | Full support | Full support (since 3.39)| Full support |
| **`FULL OUTER JOIN`** | Full support | Not supported (emulated via UNION)| Full support (since 3.39)| Full support |
| **`CROSS JOIN`** | Full support | Full support | Full support | Full support |
| **`USING (col)` Syntax** | Full support | Full support | Full support | Not supported (use `ON`)|
| **Hash Join Engine** | Full support | Full support (since 8.0) | Supported (Bloom filters) | Full support |

---

## 5. 5 Practical Progressive Examples

### Example 1: Multi-Table Inner Join (E-Commerce Order Detail)
Navigating a 4-table relational path: `customers` $\rightarrow$ `orders` $\rightarrow$ `order_items` $\rightarrow$ `products`:

```sql
SELECT 
    c.first_name || ' ' || c.last_name AS customer_name,
    o.order_id,
    o.order_date,
    p.product_name,
    oi.quantity,
    oi.unit_price,
    (oi.quantity * oi.unit_price) AS line_total
FROM customers c
INNER JOIN orders o 
  ON c.customer_id = o.customer_id
INNER JOIN order_items oi 
  ON o.order_id = oi.order_id
INNER JOIN products p 
  ON oi.product_id = p.product_id
WHERE o.order_status = 'completed'
ORDER BY o.order_id, oi.item_id;
```
```text
+---------------+----------+---------------------+---------------------------------------+----------+------------+------------+
| customer_name | order_id | order_date          | product_name                          | quantity | unit_price | line_total |
+---------------+----------+---------------------+---------------------------------------+----------+------------+------------+
| Alex Morgan   |     1001 | 2024-04-01 10:15:00 | Framework Laptop 16                   |        1 |    1399.00 |    1399.00 |
| Alex Morgan   |     1001 | 2024-04-01 10:15:00 | Designing Data-Intensive Applications |        1 |      45.00 |      45.00 |
| Priya Sharma  |     1002 | 2024-04-02 12:00:00 | Pixel 9 Pro                           |        1 |     999.00 |     999.00 |
| Priya Sharma  |     1002 | 2024-04-02 12:00:00 | Sony WH-1000XM5                       |        1 |     399.00 |     399.00 |
| Alex Morgan   |     1003 | 2024-04-05 15:30:00 | Designing Data-Intensive Applications |        1 |      45.00 |      45.00 |
+---------------+----------+---------------------+---------------------------------------+----------+------------+------------+
```

### Example 2: The `ON` vs `WHERE` Left Join Trap
Demonstrating how predicate placement alters outer join semantics:

```sql
-- Scenario: Find ALL customers and their completed orders in 2024.
-- Customers without completed orders should still appear with NULL order data!

-- CORRECT: Filter placed in ON clause
SELECT 
    c.customer_id, 
    c.first_name, 
    o.order_id, 
    o.order_status
FROM customers c
LEFT JOIN orders o 
  ON c.customer_id = o.customer_id 
 AND o.order_status = 'completed';
```
```text
+-------------+------------+----------+--------------+
| customer_id | first_name | order_id | order_status |
+-------------+------------+----------+--------------+
|         101 | Alex       |     1001 | completed    |
|         101 | Alex       |     1003 | completed    |
|         102 | Priya      |     1002 | completed    |
|         103 | David      |     NULL | NULL         |
|         104 | Maria      |     NULL | NULL         |
|         105 | Liam       |     NULL | NULL         |
+-------------+------------+----------+--------------+
```
*(Notice: David, Maria, and Liam are preserved).*

Now observe what happens if you place that condition in `WHERE`:
```sql
-- INCORRECT: Moving condition to WHERE clause
SELECT 
    c.customer_id, 
    c.first_name, 
    o.order_id, 
    o.order_status
FROM customers c
LEFT JOIN orders o 
  ON c.customer_id = o.customer_id
WHERE o.order_status = 'completed';
-- Discards David, Maria, and Liam because NULL = 'completed' evaluates to UNKNOWN!
-- The LEFT JOIN has been degraded into an INNER JOIN.
```

### Example 3: Edge Case (Full Outer Join Reconciliation)
Reconciling records between a legacy billing system and a modern payment gateway:

```sql
CREATE TABLE legacy_billing (account_id INT PRIMARY KEY, billed_amt NUMERIC(10, 2));
CREATE TABLE gateway_payments (account_id INT PRIMARY KEY, paid_amt NUMERIC(10, 2));

INSERT INTO legacy_billing VALUES (1, 100.00), (2, 250.00), (3, 75.00);
INSERT INTO gateway_payments VALUES (2, 250.00), (3, 50.00), (4, 120.00);

-- FULL OUTER JOIN detects: Matches, Unbilled Payments, and Unpaid Bills!
SELECT 
    COALESCE(b.account_id, p.account_id) AS account_id,
    b.billed_amt,
    p.paid_amt,
    CASE 
        WHEN b.account_id IS NULL THEN 'Payment without Bill'
        WHEN p.account_id IS NULL THEN 'Billed but Unpaid'
        WHEN b.billed_amt <> p.paid_amt THEN 'Amount Mismatch'
        ELSE 'Reconciled'
    END AS reconciliation_status
FROM legacy_billing b
FULL OUTER JOIN gateway_payments p 
  ON b.account_id = p.account_id
ORDER BY account_id;
```
```text
+------------+------------+----------+-----------------------+
| account_id | billed_amt | paid_amt | reconciliation_status |
+------------+------------+----------+-----------------------+
|          1 |     100.00 |     NULL | Billed but Unpaid     |
|          2 |     250.00 |   250.00 | Reconciled            |
|          3 |      75.00 |    50.00 | Amount Mismatch       |
|          4 |       NULL |   120.00 | Payment without Bill  |
+------------+------------+----------+-----------------------+
```

### Example 4: Failure Mode (Accidental Comma Join Cartesian Product)
What happens when a legacy comma join omits the `WHERE` condition:

```sql
-- DANGEROUS ANTI-PATTERN: Comma join without WHERE condition
SELECT c.customer_id, o.order_id
FROM customers c, orders o;
-- Produces 5 customers * 5 orders = 25 rows!
-- On production tables with 10,000 customers and 100,000 orders:
-- Generates 1,000,000,000 rows, exhausting memory and locking server CPUs!
```
*(Always use explicit SQL-92 `JOIN ... ON` syntax. It forces the developer to supply join predicates).*

### Example 5: Optimized Join Execution Plan Profiling
Inspecting how the query planner chooses a physical Hash Join for unindexed relations:

```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT c.first_name, o.order_id
FROM customers c
INNER JOIN orders o ON c.customer_id = o.customer_id;
```
```text
                                                   QUERY PLAN                                                   
----------------------------------------------------------------------------------------------------------------
 Hash Join  (cost=1.11..2.22 rows=5 width=40) (actual time=0.021..0.025 rows=5 loops=1)
   Hash Cond: (o.customer_id = c.customer_id)
   Buffers: shared hit=2
   ->  Seq Scan on orders o  (cost=0.00..1.05 rows=5 width=8) (actual time=0.003..0.004 rows=5 loops=1)
         Buffers: shared hit=1
   ->  Hash  (cost=1.05..1.05 rows=5 width=36) (actual time=0.009..0.009 rows=5 loops=1)
         Buckets: 1024  Batches: 1  Memory Usage: 9kB
         Buffers: shared hit=1
         ->  Seq Scan on customers c  (cost=0.00..1.05 rows=5 width=36) (actual time=0.002..0.003 rows=5 loops=1)
               Buffers: shared hit=1
 Planning Time: 0.085 ms
 Execution Time: 0.052 ms
```
- `Hash` node: Read `customers` into an in-memory hash table in `work_mem`.
- `Hash Join` node: Streamed `orders` and probed the hash table in $O(1)$ time per row.

---

## 6. Best Practices

1. **Always Use Explicit ANSI SQL-92 `JOIN ... ON` Syntax**: Never use legacy comma joins (`FROM a, b WHERE a.id = b.id`). Explicit joins cleanly separate join predicates from row filters and prevent catastrophic Cartesian products.
2. **Always Index Foreign Key Columns**: Failure to create a B-Tree index on child table foreign key columns prevents the optimizer from choosing fast **Index Nested Loop Joins**, forcing slow sequential scans or hash builds on every query.
3. **Keep Left-Table Filters in `WHERE`, Right-Table Filters in `ON` in Outer Joins**:
   - Condition on left table $\rightarrow$ Put in `WHERE`.
   - Condition on right table $\rightarrow$ Put in `ON` (to preserve outer join null-padding).
4. **Never Use `NATURAL JOIN` in Production**: A schema migration adding an unrelated column with a common name (e.g., `updated_at`, `status`) silently breaks every `NATURAL JOIN` in your codebase without throwing a syntax error.
5. **Always Qualify All Column Names**: In multi-table queries, always prefix every column with its table alias (`c.customer_id`, `o.total_amount`). Unqualified column references break if an identical column name is added to another table in the future.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Putting Right-Table Filters in `WHERE` of a `LEFT JOIN`** | Silently converts the `LEFT JOIN` into an `INNER JOIN`, discarding unmatched left rows. | Move the right-table filter into the `ON` clause. |
| **Using `NATURAL JOIN`** | Joins across *all* columns sharing identical names, silently breaking when schemas evolve. | Use explicit `JOIN ... ON` or `JOIN ... USING (col)`. |
| **Missing Foreign Key B-Tree Indexes** | Forces the query engine to perform Hash Joins or Full Table Scans instead of Index Nested Loop seeks. | Create explicit indexes: `CREATE INDEX idx_fk ON child(parent_id);`. |
| **Accidental Comma Join Cartesian Product** | Omitting a join predicate in `FROM a, b` returns $N \times M$ rows, saturating server memory. | Use explicit `INNER JOIN a ON ...`. |
| **Joining on Floating-Point Columns** | Due to IEEE-754 binary rounding errors, `float_col1 = float_col2` frequently evaluates to false. | Join on exact integers, UUIDs, or exact numeric types. |
| **Duplicating Aggregate Values via Join Multiplicity** | Joining a 1-to-N relationship (e.g., Orders to Order Items) duplicates order totals when aggregating! | Aggregate line items in a subquery/CTE *before* joining to orders. |

---

## 8. Performance & Security Considerations

### Performance
- **Join Ordering by the Optimizer**:
  - The query optimizer does not join tables in the lexical order you write them. For $N$ tables, it searches the combinatorial space of join permutations to determine the cheapest path (starting with the smallest filtered relation).
  - In queries with $> 12$ tables, PostgreSQL automatically switches from exhaustive dynamic programming to the **Genetic Query Optimizer (GEQO)** to prevent planning time explosion.
- **Hash Join Memory Spilling**:
  - If the inner table hash table exceeds `work_mem`, PostgreSQL splits the hash table into multiple batches on disk, degrading performance. Ensure `work_mem` is tuned appropriately.

### Security
- **Multi-Tenant Row Leakage via Joins**:
  - In multi-tenant databases, every joined table must be explicitly partitioned or filtered by `tenant_id` to prevent cross-tenant data leakage:
    ```sql
    FROM orders o 
    JOIN customers c ON o.customer_id = c.customer_id AND c.tenant_id = o.tenant_id
    WHERE o.tenant_id = $current_tenant;
    ```

---

## 9. Comparison Matrix

### Relational Join Types Summary

| Join Type | Left Rows Emitted | Right Rows Emitted | Unmatched Rows Handling | Algebraic Symbol |
|:---|:---:|:---:|:---|:---:|
| **`INNER JOIN`** | Matching only | Matching only | Discarded | $\bowtie_\theta$ |
| **`LEFT OUTER JOIN`** | **All** | Matching only | Right columns null-padded | $\ltimes\!\!\bowtie$ |
| **`RIGHT OUTER JOIN`** | Matching only | **All** | Left columns null-padded | $\bowtie\!\!\rtimes$ |
| **`FULL OUTER JOIN`** | **All** | **All** | Both sides null-padded where unmatched | $\bowtie\!\!\!\bowtie$ |
| **`CROSS JOIN`** | **All** | **All** | Cartesian Product ($N \times M$) | $\times$ |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: What is the exact difference between placing a filter condition in the `ON` clause versus the `WHERE` clause in a `LEFT OUTER JOIN`?
<details>
<summary>Answer</summary>
- **In the `ON` clause**: The predicate is evaluated during the join formation phase. If the condition evaluates to false, the left-hand row is **not discarded**; instead, it is emitted with `NULL` values for all right-hand columns.
- **In the `WHERE` clause**: The predicate is evaluated after the join has been completely formed. If an unmatched left-hand row was null-padded by the `LEFT JOIN`, evaluating any condition on the right table (e.g., `WHERE right.status = 'active'`) results in `NULL = 'active'` $\rightarrow$ `UNKNOWN`. Because `WHERE` requires `TRUE`, the null-padded row is discarded.

Therefore, putting a right-table filter in the `WHERE` clause converts the `LEFT OUTER JOIN` into an `INNER JOIN`.
</details>

#### Q2: Explain the three physical join algorithms used by relational database engines (Nested Loop, Hash Join, Merge Join) and when each is optimal.
<details>
<summary>Answer</summary>
1. **Nested Loop Join**: Iterates over each row of an outer table and looks up matching rows in an inner table. Optimal when the outer set is small ($< 100$ rows) and the inner table has a B-Tree index on the join key ($O(R \log S)$).
2. **Hash Join**: Scans the smaller table, builds an in-memory hash table on the join key in `work_mem`, then streams the larger table, probing the hash table. Optimal for large unsorted datasets with equality predicates ($O(R + S)$).
3. **Merge Join**: Requires both relations to be pre-sorted on the join key. It walks through both sorted streams sequentially in tandem. Optimal when inputs are already sorted by an index or prior sort, requiring minimal memory ($O(R + S)$).
</details>

#### Q3: Why is `NATURAL JOIN` considered an anti-pattern in enterprise schema design?
<details>
<summary>Answer</summary>
A `NATURAL JOIN` automatically joins two tables on **all columns that share identical names**. 
This creates tight, invisible coupling:
1. If a developer runs a migration adding an audit column like `updated_at` or `created_by` to both tables, the `NATURAL JOIN` automatically adds `AND a.updated_at = b.updated_at` to the join condition!
2. The query will suddenly return 0 rows or missing data in production without throwing any syntax or runtime error.
Always use explicit `JOIN ... ON` or `JOIN ... USING`.
</details>

#### Q4: How does MySQL 8.4 LTS emulate a `FULL OUTER JOIN` since it does not support the native keyword?
<details>
<summary>Answer</summary>
MySQL emulates a `FULL OUTER JOIN` by combining a `LEFT JOIN` and a `RIGHT JOIN` using the `UNION` set operator:

```sql
SELECT c.customer_id, o.order_id
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id
UNION
SELECT c.customer_id, o.order_id
FROM customers c
RIGHT JOIN orders o ON c.customer_id = o.customer_id;
```
`UNION` automatically deduplicates the overlapping inner matched rows emitted by both queries.
</details>

### Practical Join Puzzles

#### Q5: Write a query to find all customers who have NEVER placed an order, using a `LEFT JOIN`.
<details>
<summary>Answer</summary>

```sql
SELECT c.customer_id, c.first_name, c.last_name, c.email
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id
WHERE o.order_id IS NULL;
```
*(This is the classic anti-join pattern: unmatched left rows are null-padded; filtering for `o.order_id IS NULL` captures customers with zero orders).*
</details>

#### Q6: Explain what happens when a query joins Table A (1,000 rows) with Table B (2,000 rows), where every row in both tables has `status = 'active'`, and the join condition is `ON a.status = b.status`.
<details>
<summary>Answer</summary>
Because every row in Table A matches every row in Table B on `'active'`, the join degrades into a full **Cartesian Product**:
$$\text{Total Rows} = 1,000 \times 2,000 = 2,000,000\text{ rows!}$$
Joining on low-cardinality non-unique columns creates massive row multiplication, saturating buffer caches and crashing client connections.
</details>

#### Q7: Write a query that computes the total spending of each customer, including customers who spent $0.00, avoiding duplicate addition when line items exist.
<details>
<summary>Answer</summary>

```sql
SELECT 
    c.customer_id,
    c.first_name,
    c.last_name,
    COALESCE(SUM(o.total_amount), 0.00) AS total_spent
FROM customers c
LEFT JOIN orders o 
  ON c.customer_id = o.customer_id 
 AND o.order_status = 'completed'
GROUP BY c.customer_id, c.first_name, c.last_name
ORDER BY total_spent DESC;
```
</details>

#### Q8: How can you write a cross-join query that generates all combinations of days of the week (Monday–Sunday) and shifts (Morning, Afternoon, Night) for shift scheduling?
<details>
<summary>Answer</summary>

```sql
WITH Days AS (
    SELECT unnest(ARRAY['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday']) AS day_name
),
Shifts AS (
    SELECT unnest(ARRAY['Morning', 'Afternoon', 'Night']) AS shift_name
)
SELECT d.day_name, s.shift_name
FROM Days d
CROSS JOIN Shifts s
ORDER BY d.day_name, s.shift_name;
```
*(Generates exactly $7 \times 3 = 21$ scheduling slots).*
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Basic Two-Table Inner Join
Retrieve all orders with `order_status = 'completed'`, showing `order_id`, `order_date`, customer `email`, and `total_amount`.
<details>
<summary>Solution</summary>

```sql
SELECT 
    o.order_id,
    o.order_date,
    c.email,
    o.total_amount
FROM orders o
INNER JOIN customers c ON o.customer_id = c.customer_id
WHERE o.order_status = 'completed'
ORDER BY o.order_date DESC;
```
</details>

#### Exercise 2: Unmatched Row Left Join
List all categories from `categories` and their associated products. Include categories that currently have zero products.
<details>
<summary>Solution</summary>

```sql
SELECT 
    cat.category_id,
    cat.category_name,
    p.product_id,
    p.product_name
FROM categories cat
LEFT JOIN products p ON cat.category_id = p.category_id
ORDER BY cat.category_id, p.product_id;
```
</details>

### Intermediate Exercises

#### Exercise 3: Three-Way Outer Join
Retrieve all customers, their orders (if any), and the items within those orders (if any). Ensure customers who never ordered are still displayed.
<details>
<summary>Solution</summary>

```sql
SELECT 
    c.customer_id,
    c.first_name,
    o.order_id,
    oi.product_id,
    oi.quantity
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id
LEFT JOIN order_items oi ON o.order_id = oi.order_id
ORDER BY c.customer_id, o.order_id, oi.item_id;
```
</details>

#### Exercise 4: Generating a Size/Color Matrix via Cross Join
Given a table of `products` and a virtual table of `t_shirt_sizes ('S', 'M', 'L', 'XL')`, generate a row for every possible product-size combination for products in category 4.
<details>
<summary>Solution</summary>

```sql
SELECT 
    p.product_id,
    p.product_name,
    s.size_code
FROM products p
CROSS JOIN (
    VALUES ('S'), ('M'), ('L'), ('XL')
) AS s(size_code)
WHERE p.category_id = 4
ORDER BY p.product_id, s.size_code;
```
</details>

### Advanced Exercises

#### Exercise 5: Full Outer Join Data Migration Diff
Two tables `users_v1` and `users_v2` represent a user database before and after a migration. Write a `FULL OUTER JOIN` query that outputs `user_id`, status (`'Added in V2'`, `'Deleted in V2'`, `'Unchanged'`, `'Email Modified'`).
<details>
<summary>Solution</summary>

```sql
SELECT 
    COALESCE(v1.user_id, v2.user_id) AS user_id,
    v1.email AS old_email,
    v2.email AS new_email,
    CASE 
        WHEN v1.user_id IS NULL THEN 'Added in V2'
        WHEN v2.user_id IS NULL THEN 'Deleted in V2'
        WHEN v1.email <> v2.email THEN 'Email Modified'
        ELSE 'Unchanged'
    END AS diff_status
FROM users_v1 v1
FULL OUTER JOIN users_v2 v2 ON v1.user_id = v2.user_id
ORDER BY user_id;
```
</details>

#### Exercise 6: Forcing Join Algorithms via Session Flags
Using PostgreSQL, run `EXPLAIN` on a two-table join. Disable Hash Joins (`SET enable_hashjoin = OFF;`) and observe how the planner switches to a `Merge Join` or `Nested Loop Join`.
<details>
<summary>Solution</summary>

```sql
-- Initial plan (Default: Hash Join)
EXPLAIN SELECT * FROM customers c JOIN orders o ON c.customer_id = o.customer_id;

-- Force Merge Join
SET enable_hashjoin = OFF;
EXPLAIN SELECT * FROM customers c JOIN orders o ON c.customer_id = o.customer_id;

-- Force Nested Loop Join
SET enable_mergejoin = OFF;
EXPLAIN SELECT * FROM customers c JOIN orders o ON c.customer_id = o.customer_id;

-- Reset defaults
RESET enable_hashjoin;
RESET enable_mergejoin;
```
</details>

---

## 12. Summary & Quick Reference

### Joins Quick Cheat Sheet
- **`INNER JOIN`**: Rows where join condition evaluates to `TRUE`.
- **`LEFT JOIN`**: All left rows + matching right rows (unmatched right columns are `NULL`).
- **`RIGHT JOIN`**: All right rows + matching left rows (unmatched left columns are `NULL`).
- **`FULL JOIN`**: All rows from both sides (unmatched columns are `NULL`).
- **`CROSS JOIN`**: Cartesian product ($N \times M$ rows).
- **Golden Rule**: In a `LEFT JOIN`, put right-table filters in the **`ON`** clause, not the **`WHERE`** clause.
- **Performance Rule**: Always index foreign key columns to enable fast Index Nested Loop joins.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [Table Expressions - Joined Tables](https://www.postgresql.org/docs/17/queries-table-expressions.html#QUERIES-FROM)
- Markus Winand: *"Join Operations in SQL"* (Use The Index, Luke)
- Goetz Graefe: *"Query Evaluation Techniques for Large Databases"* (ACM Computing Surveys)
- [02 - Self Joins & Hierarchies](./02-self-joins.md)
