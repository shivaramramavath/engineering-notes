# 03: Set Operations: UNION, INTERSECT & EXCEPT

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [02 - Self Joins & Hierarchical Relations](./02-self-joins.md) |
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

Relational joins combine tables **horizontally** by appending attributes side-by-side based on relationship keys. In contrast, **Set Operations** combine queries **vertically** by stacking rows from two or more result sets on top of one another, applying classical mathematical set theory to relations.

SQL provides three fundamental set operations:
1. **`UNION`**: Computes the set union of two queries, combining all rows and removing duplicates.
2. **`INTERSECT`**: Computes the set intersection, returning only rows that exist in *both* queries.
3. **`EXCEPT` (or `MINUS`)**: Computes the set difference, returning rows present in the first query that do *not* exist in the second query.

Furthermore, SQL distinguishes between **Set Semantics** (which deduplicate tuples, incurring sorting and hashing overhead) and **Multiset (Bag) Semantics** (`UNION ALL`, `INTERSECT ALL`, `EXCEPT ALL`), which preserve duplicates with zero deduplication penalty.

Understanding set operators, their strict type-compatibility rules, precedence order, and execution costs is essential for consolidating partitioned data, reconciling datasets, and building multi-source data pipelines.

---

## 2. Core Concepts

### 1. Visualizing Set Operations

```
Query A: {1, 2, 2, 3}              Query B: {2, 3, 4}

1. UNION (Distinct):               {1, 2, 3, 4}
   - Combines both sets, eliminates all duplicate rows.

2. UNION ALL (Multiset Bag):       {1, 2, 2, 3, 2, 3, 4}
   - Pure concatenation. Preserves all duplicate instances.

3. INTERSECT (Distinct):           {2, 3}
   - Retains only elements common to both sets.

4. EXCEPT (A EXCEPT B):            {1}
   - Elements present in A that do not exist in B.
```

### 2. Mandatory Structural Invariants
For any set operation to be valid, participating queries must satisfy strict rules:
1. **Identical Column Count (Arity)**: Both queries must return the exact same number of columns.
2. **Compatible Data Types**: Corresponding columns must have compatible data types (e.g., `INT` and `BIGINT`, or `VARCHAR` and `TEXT`). The database engine determines the resulting data type using implicit type resolution.
3. **Column Names Determined by the First Query**: The column names and aliases in the final result set are governed **strictly by the first `SELECT` query**. Aliases defined in subsequent queries are completely ignored!

### 3. Precedence Order: `INTERSECT` Binds First
In mathematical set algebra:
$$\text{Intersection } (\cap) \text{ takes precedence over Union } (\cup) \text{ and Difference } (-)$$

In SQL:
`Query A UNION Query B INTERSECT Query C`
is evaluated as:
`Query A UNION (Query B INTERSECT Query C)`.
To alter evaluation order, you must use explicit parentheses:
`(Query A UNION Query B) INTERSECT Query C`.

---

## 3. How It Works Internally

### Physical Execution: Hash Aggregate vs Append

The execution difference between `UNION ALL` and `UNION` is one of the most significant performance gaps in SQL:

```
UNION ALL Execution Plan:                       UNION (Distinct) Execution Plan:
+------------------------------------+           +------------------------------------+
| Append Node                        |           | Unique / HashAggregate Node        |
| ├── Seq Scan on Table A            |           | └── Append Node                    |
| └── Seq Scan on Table B            |           |     ├── Seq Scan on Table A        |
+------------------------------------+           |     └── Seq Scan on Table B        |
                                                 +------------------------------------+
Cost: O(A + B) (Zero sorting or hashing!)        Cost: O(A + B) + O(N log N) sorting!
Emits rows immediately as streamed.              Must consume ALL rows to deduplicate.
```

- **`UNION ALL` (Append)**: The engine executes an `Append` operator. It reads rows from Query A and streams them directly to the client, then reads rows from Query B and streams them. Memory consumption is negligible ($O(1)$).
- **`UNION` (Unique / Deduplication)**: The engine streams rows into an in-memory Hash Table or performs an external disk Quicksort to locate and eliminate duplicates before emitting any rows.

---

## 4. Syntax & Usage

### Core Set Operations Syntax

```sql
-- 1. UNION (Deduplicated)
SELECT customer_id, email FROM active_customers
UNION
SELECT customer_id, email FROM legacy_customers;

-- 2. UNION ALL (Fast Concatenation)
SELECT event_id, event_type, created_at FROM web_events
UNION ALL
SELECT event_id, event_type, created_at FROM mobile_events;

-- 3. INTERSECT (Common rows)
SELECT customer_id FROM orders WHERE order_year = 2023
INTERSECT
SELECT customer_id FROM orders WHERE order_year = 2024;

-- 4. EXCEPT (Difference: in 2023 but NOT in 2024)
SELECT customer_id FROM orders WHERE order_year = 2023
EXCEPT
SELECT customer_id FROM orders WHERE order_year = 2024;
```

### Multi-Dialect Comparison Matrix

| Set Operation | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **`UNION` & `UNION ALL`** | Full support | Full support | Full support | Full support |
| **`INTERSECT` (Distinct)** | Full support | Supported (since 8.0.31)| Full support | Full support |
| **`INTERSECT ALL` (Bag)** | Supported | Not supported | Not supported | Not supported |
| **`EXCEPT` Keyword** | Full support | Supported (since 8.0.31)| Full support | Full support |
| **`MINUS` Keyword Synonym** | Not native (use `EXCEPT`)| Supported (Oracle mode) | Not native | Not native |
| **`EXCEPT ALL` (Bag)** | Supported | Not supported | Not supported | Not supported |
| **`ORDER BY` Scope** | Allowed only on final query or in parentheses | Allowed only on final query | Allowed only on final query | Allowed only on final query |

---

## 5. 5 Practical Progressive Examples

### Example 1: Combining Historical and Current Partitions (`UNION ALL`)
Querying across active and archived order tables efficiently:

```sql
SELECT 
    order_id, 
    customer_id, 
    total_amount, 
    order_date, 
    'active' AS source_partition
FROM orders
WHERE order_date >= '2024-01-01'

UNION ALL

SELECT 
    order_id, 
    customer_id, 
    total_amount, 
    order_date, 
    'archived' AS source_partition
FROM orders_archive
WHERE order_date >= '2024-01-01'
ORDER BY order_date DESC;
```
*(Because active and archived orders are guaranteed to have disjoint IDs, using `UNION ALL` eliminates deduplication sorting costs completely).*

### Example 2: Finding Repeat Buyers Across Years (`INTERSECT`)
Finding customers who purchased in 2023 AND also purchased in 2024:

```sql
SELECT customer_id FROM orders 
WHERE order_date >= '2023-01-01' AND order_date < '2024-01-01' AND order_status = 'completed'

INTERSECT

SELECT customer_id FROM orders 
WHERE order_date >= '2024-01-01' AND order_date < '2025-01-01' AND order_status = 'completed'

ORDER BY customer_id;
```
```text
+-------------+
| customer_id |
+-------------+
|         101 |
+-------------+
```

### Example 3: Customer Churn Detection (`EXCEPT`)
Finding customers who registered in 2023 but have never placed an order:

```sql
SELECT customer_id FROM customers WHERE created_at < '2024-01-01'

EXCEPT

SELECT customer_id FROM orders WHERE order_status = 'completed';
```
```text
+-------------+
| customer_id |
+-------------+
|         103 |
|         104 |
|         105 |
+-------------+
```
*(Returns all registered customer IDs except those present in completed orders).*

### Example 4: Failure Mode (Column Count & Type Mismatch)
What happens when set branches have mismatched arity or incompatible types:

```sql
-- FAILS: Arity mismatch (Query 1 returns 2 columns, Query 2 returns 3!)
SELECT customer_id, email FROM customers
UNION
SELECT order_id, customer_id, total_amount FROM orders;
```
```text
ERROR: each UNION query must have the same number of columns
LINE 3: SELECT order_id, customer_id, total_amount FROM orders;
```

```sql
-- FAILS: Incompatible type conversion (Cannot cast date to integer)
SELECT customer_id FROM customers -- integer
UNION
SELECT order_date FROM orders;    -- timestamp
```
```text
ERROR: UNION types integer and timestamp without time zone cannot be matched
```

### Example 5: Optimized Sorting & Limiting per Branch
If you want to take the top 3 newest orders and combine them with the top 3 oldest orders:

```sql
-- Individual branch ORDER BY and LIMIT must be enclosed in parentheses!
(
    SELECT order_id, total_amount, order_date, 'newest' AS label
    FROM orders
    ORDER BY order_date DESC
    LIMIT 3
)
UNION ALL
(
    SELECT order_id, total_amount, order_date, 'oldest' AS label
    FROM orders
    ORDER BY order_date ASC
    LIMIT 3
)
ORDER BY order_date DESC;
```
```text
+----------+--------------+---------------------+--------+
| order_id | total_amount | order_date          | label  |
+----------+--------------+---------------------+--------+
|     1005 |       399.00 | 2024-04-11 14:00:00 | newest |
|     1004 |      1099.00 | 2024-04-10 09:00:00 | newest |
|     1003 |        45.00 | 2024-04-05 15:30:00 | newest |
|     1003 |        45.00 | 2024-04-05 15:30:00 | oldest |
|     1002 |      1398.00 | 2024-04-02 12:00:00 | oldest |
|     1001 |      1444.00 | 2024-04-01 10:15:00 | oldest |
+----------+--------------+---------------------+--------+
```

---

## 6. Best Practices

1. **Default to `UNION ALL` Instead of `UNION`**: Only use `UNION` if you genuinely require duplicate elimination. `UNION` forces the database to sort or hash the entire combined dataset. If you know the two sets are disjoint, `UNION ALL` is significantly faster.
2. **Column Names Are Defined by the First Query**: Name and alias your columns in the top `SELECT`. Defining aliases in the second query is ignored:
   ```sql
   SELECT id AS user_id FROM staff  -- 'user_id' becomes the result header
   UNION ALL
   SELECT customer_id AS cust_id FROM clients;
   ```
3. **Use Parentheses When Mixing Set Operators**: Because `INTERSECT` takes precedence over `UNION` and `EXCEPT`, always use parentheses to make your logical grouping explicit: `(A UNION B) INTERSECT C`.
4. **Parenthesize Queries When Applying Local `ORDER BY` / `LIMIT`**: A global `ORDER BY` applies to the final combined result. To apply `ORDER BY` or `LIMIT` to an individual branch, enclose that branch in parentheses.
5. **Ensure Type Alignment**: Explicitly cast mismatched column types in the query (`SELECT id::text`) rather than relying on engine-specific type coercion rules.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Using `UNION` When `UNION ALL` Suffices** | Incurs an unnecessary in-memory or on-disk deduplication sort on millions of rows. | Use `UNION ALL` if datasets are already disjoint or duplicates are allowed. |
| **Assuming Aliases from the Second Query Apply** | The second query's aliases are ignored, confusing developers debugging downstream column names. | Assign desired aliases in the first `SELECT` statement. |
| **Misunderstanding `INTERSECT` Precedence** | `A UNION B INTERSECT C` evaluates `B INTERSECT C` first! | Use parentheses: `(A UNION B) INTERSECT C`. |
| **Column Count / Type Mismatch** | Query throws an immediate error if column counts or data types do not align. | Pad missing columns with `NULL::type` and cast mismatched types. |
| **Using `MINUS` in PostgreSQL** | `MINUS` is the Oracle synonym; PostgreSQL strictly uses standard `EXCEPT`. | Use ANSI standard `EXCEPT`. |
| **Global `ORDER BY` Referencing Discarded Aliases** | An `ORDER BY` at the end of the query cannot reference table aliases from individual branches (`ORDER BY a.col`). | Order by the output column name or ordinal position (`ORDER BY col_name`). |

---

## 8. Performance & Security Considerations

### Performance
- **HashAggregate vs Sort for `UNION`**:
  - PostgreSQL uses a HashAggregate node if the unique rows fit in `work_mem`. If the unique cardinality exceeds `work_mem`, it falls back to an external merge sort.
- **Pushing Down Predicates Across `UNION ALL`**:
  - The query optimizer can push `WHERE` conditions down into each individual branch of a `UNION ALL`. If you query a view built from `UNION ALL`, conditions like `WHERE date >= '2024-01-01'` are pushed into each underlying table scan, enabling index seeks.

### Security
- **Data Leakage in UNION-Based SQL Injection**:
  - Attackers exploit `UNION` to extract sensitive tables via vulnerable search inputs:
    `' UNION SELECT user_id, password_hash FROM admin_users --`
  - Always use parameterized queries to prevent input text from being parsed as set operators.

---

## 9. Comparison Matrix

### Set Operations Summary

| Operator | Duplicates Preserved? | Performance Cost | Algebraic Concept | Dialect Note |
|:---|:---:|:---:|:---:|:---|
| **`UNION ALL`** | Yes (Multiset) | Lowest ($O(N)$ streaming) | Bag Union ($A \uplus B$) | Universal |
| **`UNION`** | No (Distinct) | High ($O(N \log N)$ sort/hash) | Set Union ($A \cup B$) | Universal |
| **`INTERSECT`** | No (Distinct) | High (Hash/Sort comparison) | Set Intersection ($A \cap B$) | Universal (MySQL 8.0.31+) |
| **`INTERSECT ALL`**| Yes (Count min) | Moderate | Bag Intersection | PostgreSQL native |
| **`EXCEPT`** | No (Distinct) | High (Hash/Sort comparison) | Set Difference ($A - B$) | MySQL 8.0.31+, Oracle uses `MINUS` |
| **`EXCEPT ALL`** | Yes (Count diff) | Moderate | Bag Difference | PostgreSQL native |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: What is the physical execution difference between `UNION` and `UNION ALL`?
<details>
<summary>Answer</summary>
- **`UNION ALL`**: Implemented using an `Append` plan node. The engine simply reads tuples from the first query and streams them to the client, then reads tuples from the second query and streams them. It requires zero sorting, zero hashing, and minimal memory overhead ($O(N)$ streaming).
- **`UNION`**: Requires deduplication. The engine appends rows from both queries and then passes them into a `Unique` (Sort-based) or `HashAggregate` operator. Every row must be hashed or sorted in memory (`work_mem`). If the dataset exceeds `work_mem`, it spills to temporary disk files, executing orders of magnitude slower.
</details>

#### Q2: What are the three structural invariants required by the SQL standard for set operations?
<details>
<summary>Answer</summary>
1. **Equal Arity (Column Count)**: Both queries must select the exact same number of columns.
2. **Type Compatibility**: Corresponding columns from left to right must have compatible data types that can be implicitly cast to a common supertype.
3. **Column Header Determination**: The resulting column names and aliases are determined solely by the first `SELECT` query.
</details>

#### Q3: How does operator precedence work when combining `UNION`, `INTERSECT`, and `EXCEPT` in a single query?
<details>
<summary>Answer</summary>
According to ANSI SQL standards, **`INTERSECT` has higher precedence than `UNION` and `EXCEPT`**. 
`UNION` and `EXCEPT` share equal precedence and evaluate from left to right.

Therefore, `A UNION B INTERSECT C` is evaluated as `A UNION (B INTERSECT C)`. To force the union to occur first, parentheses are mandatory: `(A UNION B) INTERSECT C`.
</details>

#### Q4: What is the difference between `EXCEPT` and `EXCEPT ALL`?
<details>
<summary>Answer</summary>
- **`EXCEPT` (Distinct)**: Removes all instances of a row from the result if it appears in the second set. If row X appears 5 times in A and 1 time in B, `A EXCEPT B` returns zero rows for X.
- **`EXCEPT ALL` (Multiset Bag Difference)**: Subtracts the count of occurrences in B from the count of occurrences in A. If row X appears 5 times in A and 2 times in B, `A EXCEPT ALL B` returns row X exactly 3 times ($5 - 2 = 3$).
</details>

### Practical Set Operator Puzzles

#### Q5: Write an SQL query that produces a combined, unified directory of all user email addresses from three separate tables: `employees`, `contractors`, and `customers`, with duplicates removed.
<details>
<summary>Answer</summary>

```sql
SELECT email, 'Employee' AS user_type FROM employees
UNION
SELECT email, 'Contractor' AS user_type FROM contractors
UNION
SELECT email, 'Customer' AS user_type FROM customers
ORDER BY email;
```
*(If you want to deduplicate by email regardless of type, omit `user_type` from the select list).*
</details>

#### Q6: Given two queries, how can you test if two tables `table_a` and `table_b` have 100% identical contents using set operators?
<details>
<summary>Answer</summary>

```sql
(SELECT * FROM table_a EXCEPT SELECT * FROM table_b)
UNION ALL
(SELECT * FROM table_b EXCEPT SELECT * FROM table_a);
```
If both tables have identical rows, `A EXCEPT B` returns empty and `B EXCEPT A` returns empty. If the result of this query returns **zero rows**, the tables are identical.
</details>

#### Q7: Rewrite this `INTERSECT` query using an `INNER JOIN`:
```sql
SELECT customer_id FROM orders_2023
INTERSECT
SELECT customer_id FROM orders_2024;
```
<details>
<summary>Answer</summary>

```sql
SELECT DISTINCT o23.customer_id
FROM orders_2023 o23
INNER JOIN orders_2024 o24 ON o23.customer_id = o24.customer_id;
```
</details>

#### Q8: Rewrite this `EXCEPT` query using a `NOT EXISTS` anti-join:
```sql
SELECT product_id FROM products
EXCEPT
SELECT product_id FROM order_items;
```
<details>
<summary>Answer</summary>

```sql
SELECT p.product_id
FROM products p
WHERE NOT EXISTS (
    SELECT 1 FROM order_items oi WHERE oi.product_id = p.product_id
);
```
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Consolidating Contact Names
Select all distinct names from `customers` (first + last) and `employees` (first + last) into a single unified sorted list.
<details>
<summary>Solution</summary>

```sql
SELECT first_name || ' ' || last_name AS full_name FROM customers
UNION
SELECT first_name || ' ' || last_name AS full_name FROM employees
ORDER BY full_name ASC;
```
</details>

#### Exercise 2: Fast Multi-Source Log Union
Combine all log messages from `auth_logs` and `api_logs` without deduplicating rows, sorted by `logged_at` descending.
<details>
<summary>Solution</summary>

```sql
SELECT log_id, message, logged_at FROM auth_logs
UNION ALL
SELECT log_id, message, logged_at FROM api_logs
ORDER BY logged_at DESC;
```
</details>

### Intermediate Exercises

#### Exercise 3: Common Products Across Stores
Find all `product_id` values that are stocked in Store 1 AND also stocked in Store 2 using `INTERSECT`.
<details>
<summary>Solution</summary>

```sql
SELECT product_id FROM store_inventory WHERE store_id = 1
INTERSECT
SELECT product_id FROM store_inventory WHERE store_id = 2;
```
</details>

#### Exercise 4: Products With No Sales
Find all products that have never appeared in any order using `EXCEPT`.
<details>
<summary>Solution</summary>

```sql
SELECT product_id FROM products
EXCEPT
SELECT product_id FROM order_items;
```
</details>

### Advanced Exercises

#### Exercise 5: Symmetric Difference of Two Relations
Write a query to find the **Symmetric Difference** $(A \setminus B) \cup (B \setminus A)$ of `skills_required` and `skills_possessed`, identifying all skills that are either missing or extra.
<details>
<summary>Solution</summary>

```sql
(
    SELECT skill_name, 'Missing Skill' AS status FROM skills_required
    EXCEPT
    SELECT skill_name, 'Missing Skill' AS status FROM skills_possessed
)
UNION ALL
(
    SELECT skill_name, 'Extra Skill' AS status FROM skills_possessed
    EXCEPT
    SELECT skill_name, 'Extra Skill' AS status FROM skills_required
);
```
</details>

#### Exercise 6: Profiling `UNION` vs `UNION ALL` Performance
Run `EXPLAIN (ANALYZE, BUFFERS)` on `UNION` vs `UNION ALL` combining two 50,000-row tables. Observe the difference in execution time and the presence of `HashAggregate` or `Sort` nodes.
<details>
<summary>Solution</summary>

```sql
-- Fast Append (No Sort):
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM orders_archive UNION ALL SELECT * FROM orders;

-- Slow Deduplication (HashAggregate / Sort):
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM orders_archive UNION SELECT * FROM orders;
```
</details>

---

## 12. Summary & Quick Reference

### Set Operators Cheatsheet
- **`UNION ALL`**: Stacks rows directly; preserves duplicates; fastest ($O(N)$).
- **`UNION`**: Stacks rows and deduplicates; requires sort or hash ($O(N \log N)$).
- **`INTERSECT`**: Emits rows common to both queries.
- **`EXCEPT`**: Emits rows in the first query that are absent from the second query.
- **Precedence**: `INTERSECT` evaluates before `UNION` and `EXCEPT`.
- **Invariants**: Must have identical column counts and compatible data types. Headers determined by the first query.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [Combining Queries (UNION, INTERSECT, EXCEPT)](https://www.postgresql.org/docs/17/queries-union.html)
- Modern SQL: [Set Operations](https://modern-sql.com/feature/set-operators)
- [04 - Scalar & Correlated Subqueries](./04-subqueries.md)
