# 05: Filtering Rows with WHERE & Operators

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [04 - The SELECT Statement & Logical Evaluation Order](./04-select-statement.md) |
| **Target Audience** | Software Engineers, Data Engineers, Database Administrators |
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

The `WHERE` clause is the primary row-filtering mechanism in relational algebra, directly representing the **Selection ($\sigma$)** operator. While a database table might contain tens of millions of records, applications typically require only a minuscule fraction of those rows matching precise business criteria: active accounts, orders placed this week, or users in a specific geographic region.

However, writing effective `WHERE` clauses requires far more than chaining comparison operators. It demands a rigorous understanding of:
1. **Three-Valued Logic (3VL)**: Where expressions evaluate to `TRUE`, `FALSE`, or `UNKNOWN`, and how `WHERE` treats unknowns.
2. **Boolean Operator Precedence**: Where `AND` binds more tightly than `OR`, causing catastrophic data leakage if parenthetical grouping is omitted.
3. **Sargability (Search Argument Able)**: The property of a predicate that determines whether the database engine can seek directly into a B-Tree index in $O(\log N)$ time, or is forced to scan every single disk block in an $O(N)$ full table scan.
4. **Non-Deterministic Evaluation Order**: Why SQL does NOT guarantee procedural left-to-right short-circuit evaluation.

Mastering `WHERE` clause mechanics ensures that queries return mathematically correct results while running at maximum hardware efficiency.

---

## 2. Core Concepts

### 1. Comparison & Relational Operators
SQL provides standard relational comparison operators:

| Operator | Meaning | Example | Behavior with NULL |
|:---|:---|:---|:---|
| `=` | Equals | `status = 'active'` | Yields `UNKNOWN` if either side is NULL |
| `<>` or `!=` | Not equals (`<>` is standard ANSI) | `status <> 'archived'` | Yields `UNKNOWN` if either side is NULL |
| `<`, `<=` | Less than, Less than or equal | `price <= 49.99` | Standard numeric/date/text comparison |
| `>`, `>=` | Greater than, Greater than or equal | `stock_qty >= 10` | Standard numeric/date/text comparison |
| `BETWEEN a AND b` | Inclusive range ($a \le x \le b$) | `age BETWEEN 18 AND 65`| Equivalent to `x >= a AND x <= b` |
| `IN (a, b, ...)` | Discrete set membership | `country IN ('USA', 'CAN')` | Equivalent to `x = a OR x = b ...` |
| `IS NULL` | Null identity check | `deleted_at IS NULL` | Evaluates strictly to `TRUE` or `FALSE` |
| `IS NOT NULL` | Non-null identity check | `email IS NOT NULL` | Evaluates strictly to `TRUE` or `FALSE` |
| `IS DISTINCT FROM` | Null-safe equality (Postgres/Standard) | `a IS DISTINCT FROM b` | Returns `TRUE` if values differ, handles NULLs safely |

### 2. Boolean Operator Precedence (`NOT` > `AND` > `OR`)
In boolean algebra, logical conjunction (`AND`) has higher precedence than logical disjunction (`OR`), just as multiplication takes precedence over addition:

$$\text{A OR B AND C} \equiv \text{A OR (B AND C)}$$

Failing to enclose `OR` conditions in parentheses is one of the most frequent causes of silent security leaks and incorrect financial reports:

```sql
-- DANGEROUS: Finds active users in USA, PLUS ALL users in Canada regardless of status!
WHERE status = 'active' AND country = 'USA' OR country = 'Canada'

-- CORRECT: Parentheses enforce grouping
WHERE status = 'active' AND (country = 'USA' OR country = 'Canada')
```

### 3. Non-Deterministic Evaluation & Short-Circuiting
In procedural languages (JavaScript, Python, C++), boolean expressions guarantee left-to-right short-circuiting: in `if (x != null && x.val > 0)`, the second expression is never evaluated if `x` is null.

**In SQL, short-circuiting is NOT guaranteed!** The cost-based optimizer is free to evaluate predicates in whatever order is statistically cheaper:
```sql
-- DANGEROUS IN SQL: The optimizer might evaluate the division FIRST!
SELECT * FROM metrics 
WHERE denominator <> 0 AND (numerator / denominator) > 10;
-- Can throw: ERROR: division by zero!
```
The proper relational idiom is using `NULLIF` or `CASE`:
```sql
SELECT * FROM metrics 
WHERE (numerator / NULLIF(denominator, 0)) > 10;
```

---

## 3. How It Works Internally

### Sargability and Index Seek vs Full Table Scan
A predicate is **Sargable** (Search Argument Able) if the engine's query optimizer can use an index to locate rows via a direct B-Tree range seek.

```
Indexed Column: created_at (B-Tree Index exists)

Non-Sargable Query:                           Sargable Query:
WHERE DATE(created_at) = '2024-04-01'        WHERE created_at >= '2024-04-01'
                                               AND created_at <  '2024-04-02'
               │                                              │
               ▼                                              ▼
+-----------------------------+               +-----------------------------+
| B-Tree Index INOPERABLE!    |               | B-Tree Index SEEK!          |
| Engine cannot know what     |               | Engine navigates B-Tree     |
| DATE(created_at) returns    |               | root -> leaf directly to    |
| without evaluating function |               | '2024-04-01 00:00:00'.      |
| on every single row!        |               | Reads ONLY matching pages.  |
| RESULT: Seq Scan 10,000,000 |               | RESULT: Index Seek (0.1ms)  |
| rows (5 seconds)!           |               +-----------------------------+
+-----------------------------+
```

### Sargable vs Non-Sargable Patterns

| Pattern | Anti-Pattern (Non-Sargable) | Optimized (Sargable) | Why It Matters |
|:---|:---|:---|:---|
| **Arithmetic on Column** | `WHERE salary * 1.10 > 50000` | `WHERE salary > 50000 / 1.10` | Isolate column on one side of operator |
| **Date/Time Functions** | `WHERE EXTRACT(YEAR FROM dt) = 2024` | `WHERE dt >= '2024-01-01' AND dt < '2025-01-01'` | Enables B-Tree date range scan |
| **String Functions** | `WHERE SUBSTRING(code, 1, 3) = 'NYC'` | `WHERE code LIKE 'NYC%'` | Left-anchored wildcard uses B-Tree |
| **Type Coercion** | `WHERE string_id = 12345` (int) | `WHERE string_id = '12345'` | Implicit cast disables index seek |
| **Leading Wildcards** | `WHERE email LIKE '%@gmail.com'` | Full-Text search or Trigram index | Leading `%` cannot seek B-Tree |

---

## 4. Syntax & Usage

### Standard Complex WHERE Clause

```sql
SELECT order_id, customer_id, order_status, total_amount, order_date
FROM orders
WHERE 
    -- 1. Range comparison (Sargable)
    order_date >= '2024-01-01 00:00:00+00' 
    AND order_date < '2025-01-01 00:00:00+00'
    
    -- 2. Discrete set membership
    AND order_status IN ('completed', 'processing')
    
    -- 3. Parenthesized OR logic
    AND (total_amount >= 500.00 OR customer_id IN (101, 102))
    
    -- 4. Null check
    AND cancelled_at IS NULL;
```

### Multi-Dialect Comparison Matrix

| Operator / Behavior | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Not Equal Operator** | `<>` and `!=` | `<>` and `!=` | `<>` and `!=` | `<>` and `!=` |
| **Case-Insensitive Equality**| `ILIKE` or `LOWER(col) =` | Native if collation is `_ci` | `col LIKE '...'` | Native if collation is `_CI` |
| **Null-Safe Equality** | `IS NOT DISTINCT FROM` | `<=>` (Spaceship operator) | `IS` operator | `IS NOT DISTINCT FROM` (since 2022) |
| **Regular Expression Match** | `~` (case-sensitive), `~*` | `REGEXP` or `RLIKE` | `REGEXP` (with extension) | Not native (requires CLR or regex func)|
| **Boolean Literal in Filter** | `WHERE is_active` | `WHERE is_active = 1` | `WHERE is_active = 1` | `WHERE is_active = 1` |

---

## 5. 5 Practical Progressive Examples

### Example 1: Operator Precedence Trap (Parentheses Failure)
Demonstrating how missing parentheses corrupt report data:

```sql
-- Suppose we want to find electronics or computers that cost over $1,000.
-- ANTI-PATTERN: AND binds tighter than OR!
SELECT product_id, category_id, product_name, retail_price
FROM products
WHERE category_id = 1 OR category_id = 2 AND retail_price > 1000.00;
```
Logical interpretation: `category_id = 1 OR (category_id = 2 AND retail_price > 1000.00)`.
This returns EVERY product in category 1 regardless of price, plus products in category 2 over $1,000!

```sql
-- CORRECT: Explicit parenthesization
SELECT product_id, category_id, product_name, retail_price
FROM products
WHERE (category_id = 1 OR category_id = 2) 
  AND retail_price > 1000.00;
```
```text
+------------+-------------+---------------------+--------------+
| product_id | category_id | product_name        | retail_price |
+------------+-------------+---------------------+--------------+
|        201 |           2 | Framework Laptop 16 |      1399.00 |
|        202 |           2 | MacBook Air M3       |      1099.00 |
+------------+-------------+---------------------+--------------+
```

### Example 2: The `IN (subquery)` vs `EXISTS` Performance Pattern
Filtering rows based on presence in another table:

```sql
-- Finding all customers who have placed an order exceeding $1,000
-- Pattern A: IN with subquery
SELECT customer_id, first_name, last_name
FROM customers
WHERE customer_id IN (
    SELECT customer_id FROM orders WHERE total_amount > 1000.00
);

-- Pattern B: Correlated EXISTS (Preferred for complex conditions)
SELECT c.customer_id, c.first_name, c.last_name
FROM customers c
WHERE EXISTS (
    SELECT 1 FROM orders o 
    WHERE o.customer_id = c.customer_id 
      AND o.total_amount > 1000.00
);
```
*(In modern PostgreSQL, the optimizer converts both into identical Semi-Joins. However, if the subquery in `NOT IN` returns even a single `NULL`, the entire query returns 0 rows! `NOT EXISTS` is always immune to this trap).*

### Example 3: Edge Case (`BETWEEN` with Timestamps)
Why `BETWEEN` is dangerous for timestamps:

```sql
-- DANGEROUS: Misses transactions that occurred between 00:00:01 and 23:59:59 on the last day!
WHERE order_date BETWEEN '2024-04-01' AND '2024-04-30'
```
`'2024-04-30'` is cast to `'2024-04-30 00:00:00'`. An order placed on April 30 at 2:00 PM is completely omitted!

**The Production Standard: Half-Open Intervals ($[a, b)$)**:
```sql
WHERE order_date >= '2024-04-01 00:00:00'
  AND order_date <  '2024-05-01 00:00:00'
```
*(Handles microseconds, time zones, and end-of-month boundaries flawlessly).*

### Example 4: Failure Mode (Implicit Type Conversion Disabling Indexes)
What happens when you compare string columns to integer literals:

```sql
-- Table has an index on account_number (VARCHAR):
-- CREATE INDEX idx_acc_num ON accounts(account_number);

-- ANTI-PATTERN: Passing an unquoted numeric literal forces the engine
-- to cast the ENTIRE column: CAST(account_number AS numeric) = 88290012!
EXPLAIN SELECT * FROM accounts WHERE account_number = 88290012;
```
```text
                       QUERY PLAN                       
--------------------------------------------------------
 Seq Scan on accounts  (cost=0.00..25.00 rows=1 width=64)
   Filter: ((account_number)::numeric = 88290012::numeric)
```
*(Sequential scan! The index is useless because the column was typecast).*

**Correct Sargable Syntax**:
```sql
EXPLAIN SELECT * FROM accounts WHERE account_number = '88290012';
```
```text
                              QUERY PLAN                              
----------------------------------------------------------------------
 Index Scan using idx_acc_num on accounts (cost=0.15..8.17 rows=1 width=64)
   Index Cond: ((account_number)::text = '88290012'::text)
```

### Example 5: Optimized Null-Safe Equality (`IS NOT DISTINCT FROM`)
In distributed data synchronization, you frequently want to check if two values are equal, treating two `NULL`s as equal to each other:

```sql
-- Standard equality fails on NULL:
-- NULL = NULL evaluates to UNKNOWN!

-- Standard ANSI SQL & PostgreSQL:
SELECT customer_id, email, phone_number
FROM customers
WHERE phone_number IS NOT DISTINCT FROM NULL;
```
```text
+-------------+--------------------+--------------+
| customer_id | email              | phone_number |
+-------------+--------------------+--------------+
|         103 | david.k@example.com|         NULL |
+-------------+--------------------+--------------+
```

---

## 6. Best Practices

1. **Always Use Half-Open Intervals for Temporal Ranges**: Avoid `BETWEEN` for date and timestamp columns. Use `>= start_date AND < end_date`.
2. **Always Use Explicit Parentheses with `OR`**: Never leave adjacent `AND` and `OR` operators unbracketed. Explicit grouping prevents security leaks and data corruption.
3. **Isolate Indexed Columns to Ensure Sargability**: Keep the indexed column completely bare on one side of comparison operators: `WHERE col > 100 / 2`, never `WHERE col * 2 > 100`.
4. **Never Use `NOT IN` with Subqueries that May Contain NULLs**: If the subquery can return a single `NULL`, the entire `NOT IN` predicate collapses to `UNKNOWN` and returns zero rows. Always use `NOT EXISTS`.
5. **Match Literal Types to Column Types**: Always quote strings and UUIDs (`WHERE id = '123'`). Mismatched types trigger implicit casting that disables B-Tree index scans.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **`AND` / `OR` Precedence Ambiguity** | `AND` executes before `OR`, changing query logic and causing data leaks. | Always wrap `OR` conditions in parentheses: `WHERE cond1 AND (cond2 OR cond3)`. |
| **Using `BETWEEN` on Timestamps** | Omits records occurring on the final day after `00:00:00`. | Use `>= start_date AND < next_month_date`. |
| **Wrapping Indexed Columns in Functions** | `WHERE UPPER(name) = 'ALICE'` prevents index seeks, causing a full table scan. | Use expression indexes (`CREATE INDEX idx ON tbl(UPPER(name))`) or compare bare columns. |
| **`NOT IN (NULL)` Collapsing to Empty Set** | `val NOT IN (1, 2, NULL)` evaluates to `UNKNOWN`, filtering out all rows in the table. | Use `NOT EXISTS` or ensure `WHERE subquery_col IS NOT NULL`. |
| **Assuming Left-to-Right Short-Circuiting** | In `WHERE den != 0 AND num/den > 5`, the engine may evaluate `num/den` first, throwing division by zero. | Use `NULLIF(den, 0)` or `CASE WHEN den != 0 THEN num/den END > 5`. |
| **Using `!= NULL` Instead of `IS NOT NULL`** | `col != NULL` yields `UNKNOWN`, discarding all rows. | Always use `col IS NOT NULL`. |

---

## 8. Performance & Security Considerations

### Performance
- **Selectivity & Index Choice**: A filter on a high-selectivity column (e.g., `user_id = 42`, matching 0.0001% of rows) uses an Index Scan. A filter on a low-selectivity column (e.g., `is_active = TRUE`, matching 90% of rows) will intentionally use a Sequential Scan because sequential disk reads are faster than jumping back and forth across random index pages.
- **Index Intersection (Bitmap Scan)**: When multiple conditions are combined with `AND` (e.g., `WHERE status = 'pending' AND total > 500`), PostgreSQL can scan two separate indexes, construct in-memory bitmaps of matching disk pages, perform a bitwise AND, and fetch only the intersecting pages.

### Security
- **SQL Injection in String Concatenation**:
  - Concatenating user inputs into `WHERE` clauses allows attackers to break the AST:
    ```sql
    -- Malicious input: ' OR '1'='1
    "SELECT * FROM users WHERE username = '" + input + "'"
    -- Becomes: SELECT * FROM users WHERE username = '' OR '1'='1'
    ```
  - Always use parameterized queries (prepared statements).

---

## 9. Comparison Matrix

### Subquery Membership Operators Compared

| Operator | Handles Subqueries | Behavior with NULLs in Subquery | Performance Strategy |
|:---|:---:|:---|:---|
| **`IN`** | Yes | Matches valid non-null rows; ignores NULLs | Semi-join (Hash or Index probe) |
| **`NOT IN`** | Yes | **Returns 0 rows if ANY subquery row is NULL!** | Anti-join (Must check all rows) |
| **`EXISTS`** | Yes | Completely safe against NULLs in subquery | Semi-join (Stops on first match) |
| **`NOT EXISTS`** | Yes | Completely safe against NULLs in subquery | Anti-join (Preferred over `NOT IN`) |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: What does "Sargability" mean, and why is `WHERE created_at + INTERVAL '1 day' > NOW()` considered an anti-pattern?
<details>
<summary>Answer</summary>
"Sargable" stands for **Search Argument Able**. A predicate is sargable if the query optimizer can use an index on the column to locate rows via a direct B-Tree range seek.

In `WHERE created_at + INTERVAL '1 day' > NOW()`, the indexed column `created_at` is wrapped inside an arithmetic expression. The optimizer cannot traverse the B-Tree directly because the index contains keys for `created_at`, not `created_at + 1 day`. The engine is forced to evaluate the addition for every single row in the table via an $O(N)$ Sequential Scan.

Sargable Fix: Isolate the column:
`WHERE created_at > NOW() - INTERVAL '1 day'`
The right-hand side is evaluated once as a constant, allowing an $O(\log N)$ Index Seek.
</details>

#### Q2: Why does `SELECT * FROM tbl WHERE id NOT IN (SELECT parent_id FROM parents)` return 0 rows if `parents` contains even a single NULL `parent_id`?
<details>
<summary>Answer</summary>
The expression `id NOT IN (1, 2, NULL)` is mathematically equivalent to:
`id <> 1 AND id <> 2 AND id <> NULL`.

In Three-Valued Logic:
- `id <> NULL` evaluates to `UNKNOWN`.
- Any boolean expression `TRUE AND TRUE AND UNKNOWN` evaluates to `UNKNOWN`.
- The `WHERE` clause accepts only rows that evaluate strictly to `TRUE`.

Because the expression evaluates to `UNKNOWN` for every candidate row, all rows are discarded and the query returns an empty set! 
Fix: Use `NOT EXISTS` or filter the subquery: `WHERE parent_id IS NOT NULL`.
</details>

#### Q3: Explain why SQL engines do not guarantee left-to-right short-circuit evaluation in `WHERE` clauses.
<details>
<summary>Answer</summary>
SQL is a declarative language based on relational calculus, not an imperative language with strict sequential instruction pointers. The SQL standard explicitly permits query optimizers to evaluate expressions in any order that minimizes computational cost.

For example, in `WHERE expensive_function(col) AND simple_int = 1`, the optimizer will evaluate `simple_int = 1` first to discard 99% of rows before calling `expensive_function()`, reversing the written order. Therefore, code that relies on short-circuiting to prevent runtime exceptions (e.g., division by zero) can fail nondeterministically.
</details>

#### Q4: What is the difference between `IS NULL` and `= NULL`?
<details>
<summary>Answer</summary>
- `= NULL`: Uses the equality operator. In Three-Valued Logic, comparing any value (even another NULL) to NULL evaluates to `UNKNOWN`. Because `WHERE` requires `TRUE`, `= NULL` never matches any row.
- `IS NULL`: A specialized unary predicate defined by the SQL standard to test for the absence of a value. It returns a boolean `TRUE` if the operand is NULL, and `FALSE` if it contains a value.
</details>

### Practical Filtering Puzzles

#### Q5: Rewrite this non-sargable query into an optimal sargable query that can utilize an index on `phone_number`:
```sql
SELECT * FROM contacts WHERE SUBSTRING(phone_number, 1, 3) = '+44';
```
<details>
<summary>Answer</summary>

```sql
SELECT * FROM contacts WHERE phone_number LIKE '+44%';
```
*(Left-anchored string patterns with `LIKE` can use standard B-Tree index range scans).*
</details>

#### Q6: Given a table `sales(id, rep_id, amount)`, write a query to retrieve all sales where `amount` is between $100 and $500, or where `rep_id` is 42 and `amount` is over $1,000, ensuring proper operator precedence.
<details>
<summary>Answer</summary>

```sql
SELECT id, rep_id, amount
FROM sales
WHERE (amount BETWEEN 100.00 AND 500.00)
   OR (rep_id = 42 AND amount > 1000.00);
```
</details>

#### Q7: Write a query that safely divides `revenue` by `units_sold`, filtering for products where the unit price is greater than $50, without throwing a division by zero error if `units_sold` is 0 or NULL.
<details>
<summary>Answer</summary>

```sql
SELECT product_id, revenue, units_sold
FROM products
WHERE (revenue / NULLIF(units_sold, 0)) > 50.00;
```
*(If `units_sold` is 0, `NULLIF` returns `NULL`. Dividing by `NULL` returns `NULL`. `NULL > 50.00` evaluates to `UNKNOWN`, which `WHERE` safely discards without error).*
</details>

#### Q8: How can you write a query in PostgreSQL that tests whether two columns have different values, correctly treating two NULL values as identical?
<details>
<summary>Answer</summary>

```sql
SELECT * FROM inventory_reconciliation
WHERE current_stock IS DISTINCT FROM expected_stock;
```
*(If both are NULL, `IS DISTINCT FROM` returns `FALSE`. If one is NULL and the other is 5, it returns `TRUE`).*
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Range and Status Filtering
Select `order_id`, `customer_id`, and `total_amount` from `orders` for orders placed in April 2024 that are either `'completed'` or `'processing'`.
<details>
<summary>Solution</summary>

```sql
SELECT order_id, customer_id, total_amount
FROM orders
WHERE order_date >= '2024-04-01 00:00:00'
  AND order_date <  '2024-05-01 00:00:00'
  AND order_status IN ('completed', 'processing');
```
</details>

#### Exercise 2: Null Filtering
Retrieve all customers from `customers` who do NOT have a `phone_number` recorded, sorted by `customer_id`.
<details>
<summary>Solution</summary>

```sql
SELECT customer_id, first_name, last_name, email
FROM customers
WHERE phone_number IS NULL
ORDER BY customer_id;
```
</details>

### Intermediate Exercises

#### Exercise 3: Precedence Guarding
Select all products in category 1 or 2 that have `stock_qty > 0` AND `retail_price < 500.00`.
<details>
<summary>Solution</summary>

```sql
SELECT product_id, product_name, category_id, retail_price, stock_qty
FROM products
WHERE (category_id = 1 OR category_id = 2)
  AND stock_qty > 0
  AND retail_price < 500.00;
```
</details>

#### Exercise 4: Sargable Date Search
Write a query to find all orders placed within the last 7 days relative to `CURRENT_TIMESTAMP`, ensuring the predicate allows an index on `order_date` to be used.
<details>
<summary>Solution</summary>

```sql
SELECT order_id, customer_id, total_amount, order_date
FROM orders
WHERE order_date >= CURRENT_TIMESTAMP - INTERVAL '7 days'
ORDER BY order_date DESC;
```
</details>

### Advanced Exercises

#### Exercise 5: Safe Anti-Join Filtering
Find all customers who have NEVER placed an order, using the `NOT EXISTS` operator to guarantee safety against NULL values.
<details>
<summary>Solution</summary>

```sql
SELECT c.customer_id, c.first_name, c.last_name, c.email
FROM customers c
WHERE NOT EXISTS (
    SELECT 1 FROM orders o 
    WHERE o.customer_id = c.customer_id
);
```
</details>

#### Exercise 6: Verifying Sargability via EXPLAIN
Run `EXPLAIN` on a query filtering `WHERE EXTRACT(MONTH FROM order_date) = 4` versus `WHERE order_date >= '2024-04-01' AND order_date < '2024-05-01'` on an indexed table, comparing execution plans.
<details>
<summary>Solution</summary>

```sql
-- Non-sargable (Seq Scan):
EXPLAIN SELECT * FROM orders WHERE EXTRACT(MONTH FROM order_date) = 4;

-- Sargable (Index Scan):
EXPLAIN SELECT * FROM orders 
WHERE order_date >= '2024-04-01' AND order_date < '2024-05-01';
```
Observe the transition from `Seq Scan on orders` to `Index Scan using idx_orders_order_date`.
</details>

---

## 12. Summary & Quick Reference

### Operators Summary
- **Equality**: `=` (Standard), `<>` (Not equal), `IS DISTINCT FROM` (Null-safe).
- **Ranges**: Use `>= a AND < b` rather than `BETWEEN` for timestamps.
- **Membership**: `IN (...)`, `NOT IN (...)` (Beware NULLs! Use `NOT EXISTS`).
- **Null Checks**: `IS NULL`, `IS NOT NULL`.
- **Precedence**: `NOT` evaluates first, then `AND`, then `OR`. Always use parentheses!
- **Sargability**: Keep indexed columns bare on one side of operators (`WHERE col > 10 / 2`).

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [Chapter 9.2: Comparison Functions and Operators](https://www.postgresql.org/docs/17/functions-comparison.html)
- Markus Winand: *"Sargable Queries in SQL"* (Use The Index, Luke)
- C.J. Date: *"SQL and Relational Theory"* (Chapter on Three-Valued Logic and NULLs)
- [06 - Sorting Results with ORDER BY](./06-order-by.md)
