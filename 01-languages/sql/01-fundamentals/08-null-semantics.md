# 08: Three-Valued Logic & NULL Semantics

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [07 - Pagination: LIMIT, OFFSET & FETCH FIRST](./07-limit-offset.md) |
| **Target Audience** | Software Engineers, QA / Automation Engineers, Database Administrators |
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

In classical Boolean logic, every proposition is either strictly `TRUE` or strictly `FALSE`. However, real-world data systems frequently encounter missing, unavailable, unrecorded, or inapplicable information. To address this reality, Edgar F. Codd incorporated the concept of **`NULL`** into the relational model, accompanied by **Three-Valued Logic (3VL)** consisting of `TRUE`, `FALSE`, and `UNKNOWN`.

`NULL` is not a data value; it is a **marker** indicating the absence of a value. It is not equivalent to an empty string (`""`), a zero (`0`), or a boolean `FALSE`.

Because `NULL` represents an unknown, it introduces subtle logical complexities that trip up developers of all experience levels:
- Comparing any value to `NULL` using equality (`=`) yields `UNKNOWN`.
- Evaluating `NOT IN` against a subquery containing a single `NULL` silently collapses the entire query to 0 rows.
- The `WHERE` clause accepts only `TRUE` (rejecting `UNKNOWN`), whereas a `CHECK` constraint accepts anything that is *not `FALSE`* (accepting `UNKNOWN`!).
- Aggregate functions silently ignore `NULL`s, altering statistical distributions.

Mastering Three-Valued Logic is essential for preventing silent data corruption, logic bugs, and catastrophic false reporting.

---

## 2. Core Concepts

### 1. The 3VL Truth Tables
In SQL, boolean expressions evaluate to `TRUE`, `FALSE`, or `UNKNOWN`. Below are the formal truth tables:

```
+---------------+---------------+---------------+---------------+
|      AND      |     TRUE      |     FALSE     |    UNKNOWN    |
+---------------+---------------+---------------+---------------+
| TRUE          | TRUE          | FALSE         | UNKNOWN       |
| FALSE         | FALSE         | FALSE         | FALSE         |
| UNKNOWN       | UNKNOWN       | FALSE         | UNKNOWN       |
+---------------+---------------+---------------+---------------+

+---------------+---------------+---------------+---------------+
|      OR       |     TRUE      |     FALSE     |    UNKNOWN    |
+---------------+---------------+---------------+---------------+
| TRUE          | TRUE          | TRUE          | TRUE          |
| FALSE         | TRUE          | FALSE         | UNKNOWN       |
| UNKNOWN       | TRUE          | UNKNOWN       | UNKNOWN       |
+---------------+---------------+---------------+---------------+

+---------------+---------------+
|               |      NOT      |
+---------------+---------------+
| TRUE          | FALSE         |
| FALSE         | TRUE          |
| UNKNOWN       | UNKNOWN       |
+---------------+---------------+
```

Notice the critical edge cases:
- `FALSE AND UNKNOWN` is **`FALSE`** (Because regardless of what the unknown value is, false and anything is always false).
- `TRUE OR UNKNOWN` is **`TRUE`** (Because true or anything is always true).
- `NOT UNKNOWN` is **`UNKNOWN`**! Negating an unknown does not make it known.

### 2. How `WHERE` vs `CHECK` Constraints Treat UNKNOWN
This is one of the most critical and least understood gotchas in database engineering:

```
Predicate Evaluates to:        WHERE Clause Accepts?        CHECK Constraint Accepts?
───────────────────────────────────────────────────────────────────────────────────────
TRUE                                    YES                            YES
FALSE                                   NO                             NO (Throws Error)
UNKNOWN                                 NO                             YES! (Silent Pass!)
```

- In a `WHERE` clause: A row is returned **ONLY if the condition evaluates to `TRUE`**. Rows evaluating to `FALSE` or `UNKNOWN` are filtered out.
- In a `CHECK` constraint: A row is rejected **ONLY if the condition evaluates to `FALSE`**. If a check constraint evaluates to `UNKNOWN`, the database engine **accepts the insert/update**!

---

## 3. How It Works Internally

### Physical Storage: The Null Bitmap

How does a storage engine like PostgreSQL store `NULL` values on an 8KB disk page?

```
Slotted Page Tuple Structure (PostgreSQL Heap Tuple)
+--------------------------------------------------------------------------+
| Tuple Header (23 bytes)                                                  |
| ├── t_xmin, t_xmax (MVCC Transaction IDs)                                |
| ├── t_infomask (Status flags: HEAP_HASNULL, etc.)                        |
| └── Null Bitmap: [1, 1, 0, 1, 0, 0, 0, 0] (1 bit per column attribute!)  |
+--------------------------------------------------------------------------+
| Column Data Area                                                         |
| ├── Col 1 Data: 42                                                       |
| ├── Col 2 Data: 'Engineering'                                            |
| ├── Col 3: NULL (0 bytes consumed in data payload!)                      |
| └── Col 4 Data: '2024-01-01'                                             |
+--------------------------------------------------------------------------+
```

- When a table row contains nullable columns, the tuple header includes a **Null Bitmap** (1 bit per column).
- If bit $i$ is `1`, the attribute has a value. If bit $i$ is `0`, the attribute is `NULL`.
- For `NULL` attributes, the engine stores **zero payload bytes** in the column data area!
- Thus, nullable columns do not consume storage space when set to `NULL`, other than the 1-bit allocation in the tuple header bitmap.

---

## 4. Syntax & Usage

### Checking and Handling NULLs

```sql
-- 1. Correct Null Testing Predicates
SELECT * FROM users WHERE phone_number IS NULL;
SELECT * FROM users WHERE phone_number IS NOT NULL;

-- 2. Null-Safe Equality (Treats NULL as equal to NULL)
SELECT * FROM users WHERE phone_number IS NOT DISTINCT FROM NULL;

-- 3. Fallback and Coalescing Functions
SELECT 
    username,
    COALESCE(phone_number, 'No Phone Provided') AS contact_phone,
    NULLIF(login_attempts, 0) AS safe_denominator
FROM users;
```

### Multi-Dialect Comparison Matrix

| Null Handling Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Null-Safe Equality Operator** | `IS [NOT] DISTINCT FROM` | `<=>` (Spaceship) | `IS` / `IS NOT` | `IS [NOT] DISTINCT FROM` |
| **Primary Fallback Function** | `COALESCE(a, b)` | `IFNULL(a, b)` or `COALESCE` | `IFNULL(a, b)` or `COALESCE` | `ISNULL(a, b)` or `COALESCE` |
| **`NULL` in Unique Indexes** | Multiple NULLs allowed (`NULLS NOT DISTINCT` option)| Multiple NULLs allowed | Multiple NULLs allowed | Only ONE NULL allowed unless filtered |
| **Concat with NULL** | Returns NULL (`'a' \|\| NULL = NULL`)| Returns NULL (`CONCAT('a', NULL) = NULL`)| Returns NULL (`'a' \|\| NULL = NULL`)| Depends on `CONCAT_NULL_YIELDS_NULL` |
| **String Null Substitution** | `COALESCE(str, '')` | `IFNULL(str, '')` | `IFNULL(str, '')` | `ISNULL(str, '')` |

---

## 5. 5 Practical Progressive Examples

### Example 1: The Classic `= NULL` Trap
Demonstrating why comparing with `= NULL` never matches any row:

```sql
CREATE TABLE tasks (
    task_id INT PRIMARY KEY,
    title VARCHAR(100),
    completed_at TIMESTAMP
);

INSERT INTO tasks VALUES 
(1, 'Deploy API', '2024-04-01 10:00:00'),
(2, 'Write Tests', NULL);

-- ANTI-PATTERN: Comparing with = NULL
SELECT * FROM tasks WHERE completed_at = NULL;
```
```text
+---------+-------+--------------+
| task_id | title | completed_at |
+---------+-------+--------------+
+---------+-------+--------------+
(0 rows returned!)
```
`completed_at = NULL` evaluates to `UNKNOWN`. The `WHERE` clause filters out any row where the condition is not `TRUE`.

**The Correct Standard Syntax**:
```sql
SELECT * FROM tasks WHERE completed_at IS NULL;
```
```text
+---------+-------------+--------------+
| task_id | title       | completed_at |
+---------+-------------+--------------+
|       2 | Write Tests |         NULL |
+---------+-------------+--------------+
```

### Example 2: The Infamous `NOT IN (NULL)` Disaster
How a single NULL in a subquery silently wipes out all results:

```sql
CREATE TABLE departments_demo (dept_id INT);
INSERT INTO departments_demo VALUES (1), (2), (NULL); -- Contains NULL!

CREATE TABLE employees_demo (emp_id INT, dept_id INT);
INSERT INTO employees_demo VALUES (101, 1), (102, 5);

-- We want employees whose department is NOT in departments_demo (Employee 102 should match!)
SELECT * FROM employees_demo 
WHERE dept_id NOT IN (SELECT dept_id FROM departments_demo);
```
```text
+--------+---------+
| emp_id | dept_id |
+--------+---------+
+--------+---------+
(0 rows returned - SILENT LOGIC DISASTER!)
```
**Why did this happen?**
The condition expanded to:
`dept_id <> 1 AND dept_id <> 2 AND dept_id <> NULL`.
Because `dept_id <> NULL` is `UNKNOWN`, the whole expression evaluates to `UNKNOWN`, discarding every employee!

**The Production Solution: Always Use `NOT EXISTS`**:
```sql
SELECT e.* 
FROM employees_demo e
WHERE NOT EXISTS (
    SELECT 1 FROM departments_demo d 
    WHERE d.dept_id = e.dept_id
);
```
```text
+--------+---------+
| emp_id | dept_id |
+--------+---------+
|    102 |       5 |
+--------+---------+
```

### Example 3: Edge Case (Check Constraint Allowing NULLs)
Demonstrating how a check constraint quietly allows invalid data through if a column is NULL:

```sql
CREATE TABLE account_limits (
    account_id INT PRIMARY KEY,
    credit_limit NUMERIC(10, 2),
    -- Rule: Credit limit must be positive
    CONSTRAINT chk_positive_limit CHECK (credit_limit > 0)
);

-- Insert NULL:
INSERT INTO account_limits VALUES (1, NULL);
```
```text
INSERT 0 1 (SUCCESS!)
```
**Why was NULL accepted?**
`credit_limit > 0` evaluated to `NULL > 0`, which is **`UNKNOWN`**. 
Because a `CHECK` constraint rejects a row *only if the expression evaluates to `FALSE`*, `UNKNOWN` evaluates as a pass!
To forbid NULLs, you must explicitly combine with `NOT NULL`:
```sql
credit_limit NUMERIC(10, 2) NOT NULL CHECK (credit_limit > 0)
```

### Example 4: Failure Mode (Aggregate Functions Silently Dropping NULLs)
Demonstrating how `AVG()` computes averages across non-null rows rather than total rows:

```sql
CREATE TABLE exam_scores (
    student_id INT PRIMARY KEY,
    score NUMERIC(5, 2) -- NULL indicates student was absent
);

INSERT INTO exam_scores VALUES 
(1, 100.00),
(2, 50.00),
(3, NULL); -- Absent

SELECT 
    AVG(score) AS raw_avg,
    AVG(COALESCE(score, 0)) AS penalized_avg,
    COUNT(score) AS count_scores,
    COUNT(*) AS total_students
FROM exam_scores;
```
```text
+---------+---------------+--------------+----------------+
| raw_avg | penalized_avg | count_scores | total_students |
+---------+---------------+--------------+----------------+
|   75.00 |         50.00 |            2 |              3 |
+---------+---------------+--------------+----------------+
```
- `AVG(score)` yields `(100 + 50) / 2 = 75.00` (Ignores the 3rd student!).
- If absent students should count as 0, failing to use `COALESCE` inflates the average score by 50%!

### Example 5: Optimized Sorting with `NULLS LAST`
Ensuring high scores appear at the top while unranked players are pushed to the end:

```sql
CREATE TABLE leaderboards (
    player_id INT PRIMARY KEY,
    score INT
);

INSERT INTO leaderboards VALUES (1, 500), (2, NULL), (3, 850);

-- In PostgreSQL:
SELECT player_id, score
FROM leaderboards
ORDER BY score DESC NULLS LAST;
```
```text
+-----------+-------+
| player_id | score |
+-----------+-------+
|         3 |   850 |
|         1 |   500 |
|         2 |  NULL |
+-----------+-------+
```

---

## 6. Best Practices

1. **Never Use `= NULL` or `<> NULL`**: Always use standard predicates: `IS NULL` and `IS NOT NULL`.
2. **Never Use `NOT IN` with Nullable Columns**: Always prefer `NOT EXISTS` or ensure subquery columns are guarded with `WHERE col IS NOT NULL`.
3. **Always Couple `CHECK` Constraints with `NOT NULL`**: Check constraints evaluate `UNKNOWN` as a pass. If an attribute must strictly adhere to a positive range, declare it `NOT NULL CHECK (col > 0)`.
4. **Be Intentional with Aggregates**: Know whether you want the average of *reporting* rows (`AVG(val)`) or the average of *all* rows (`AVG(COALESCE(val, 0))`).
5. **Use `COALESCE` for Safe String Concatenation**: In standard SQL, concatenating a string with NULL produces NULL (`'Hello ' || NULL = NULL`). Wrap nullable strings with `COALESCE(str, '')`.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Querying with `WHERE col = NULL`** | Returns zero rows because equality with NULL evaluates to `UNKNOWN`. | Use `WHERE col IS NULL`. |
| **`NOT IN` with Subquery Containing NULL** | Collapses the entire boolean condition to `UNKNOWN`, returning 0 rows. | Use `NOT EXISTS` or `WHERE col IS NOT NULL` in subquery. |
| **Assuming Check Constraints Reject NULL** | Check constraints reject ONLY `FALSE`. `UNKNOWN` passes silently! | Add explicit `NOT NULL` to column definition. |
| **String Concatenation with NULL** | `'Hello ' || NULL` evaluates to `NULL`, erasing the entire greeting string! | Use `CONCAT()` in MySQL/Postgres or `COALESCE(str, '')`. |
| **Assuming `COUNT(col)` Equals `COUNT(*)`** | `COUNT(col)` ignores NULL values, returning a lower count than `COUNT(*)`. | Use `COUNT(*)` when counting total physical rows. |
| **Dividing by Nullable Columns Without Guard** | If denominator is NULL, result is NULL; if 0, throws error. | Use `val / NULLIF(denominator, 0)`. |

---

## 8. Performance & Security Considerations

### Performance
- **Partial Indexes on Non-Null Values**: If 95% of a table has `deleted_at IS NULL`, creating an index on all rows wastes 95% of index space. Create a partial index:
  ```sql
  CREATE INDEX idx_active_users ON users(user_id) WHERE deleted_at IS NULL;
  ```
- **Null Bitmap CPU Overhead**: Checking null bitmaps incurs negligible CPU overhead compared to the disk savings achieved by omitting null data payloads.

### Security
- **Bypassing Check Constraints via NULL**:
  - If a security constraint enforces `CHECK (user_age >= 18)`, an attacker passing `NULL` for `user_age` bypasses the check because `NULL >= 18` is `UNKNOWN` (accepted by `CHECK`). Always add `NOT NULL` to security-sensitive columns.

---

## 9. Comparison Matrix

### Behavior of NULL Across SQL Operations

| SQL Operation | Behavior with NULL | Example | Result |
|:---|:---|:---|:---|
| **Equality (`=`)** | Evaluates to `UNKNOWN` | `NULL = NULL` | `UNKNOWN` |
| **Not Equal (`<>`)** | Evaluates to `UNKNOWN` | `5 <> NULL` | `UNKNOWN` |
| **Arithmetic (`+`, `-`, `*`)** | Propagates NULL | `100 + NULL` | `NULL` |
| **String Concatenation (`\|\|`)** | Propagates NULL | `'Hello ' \|\| NULL` | `NULL` |
| **`COUNT(col)`** | Discards NULLs | `COUNT(NULL)` | `0` |
| **`SUM(col)` / `AVG(col)`** | Discards NULLs | `SUM(10, NULL)` | `10` |
| **`DISTINCT` / `GROUP BY`** | Groups all NULLs together | `GROUP BY status` | Single group for NULL |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: What is Three-Valued Logic (3VL), and why does SQL employ it instead of two-valued Boolean logic?
<details>
<summary>Answer</summary>
Three-Valued Logic is a logical system comprising three truth values: `TRUE`, `FALSE`, and `UNKNOWN`. 

SQL employs 3VL to model real-world database states where information is missing, unrecorded, or inapplicable. Because an absent value (`NULL`) is unknown, evaluating a proposition involving it (such as "Is this person's age greater than 21?") cannot be proven true or false; it is fundamentally `UNKNOWN`. 3VL provides formal mathematical rules for how unknowns propagate through logical conjunctions (`AND`), disjunctions (`OR`), and negations (`NOT`).
</details>

#### Q2: Explain the exact mechanism by which a single NULL value in a `NOT IN` subquery causes the outer query to return zero rows.
<details>
<summary>Answer</summary>
The predicate `x NOT IN (val1, val2, NULL)` is mathematically expanded by the query parser into a conjunction of inequality comparisons:
`(x <> val1) AND (x <> val2) AND (x <> NULL)`.

In Three-Valued Logic:
1. `x <> NULL` evaluates to `UNKNOWN` for every candidate row.
2. In boolean algebra, `TRUE AND TRUE AND UNKNOWN` evaluates to `UNKNOWN`.
3. The `WHERE` clause filters out any row whose predicate does not evaluate strictly to `TRUE`.

Because the expression evaluates to `UNKNOWN` for every row in the candidate relation, all rows are discarded and the query returns empty.
</details>

#### Q3: Why does a `CHECK` constraint permit a row with a NULL value to be inserted, whereas a `WHERE` clause discards it?
<details>
<summary>Answer</summary>
This stems from the formal SQL standard definitions:
- In a `WHERE` clause, the rule is: **Include row if predicate is TRUE**. If it evaluates to `FALSE` or `UNKNOWN`, discard it.
- In a `CHECK` constraint, the rule is: **Reject row if predicate is FALSE**. If it evaluates to `TRUE` or `UNKNOWN`, accept it!

Because comparing `NULL > 0` evaluates to `UNKNOWN`, it is not `FALSE`, so the `CHECK` constraint accepts the row.
</details>

#### Q4: What is the difference between `COUNT(*)`, `COUNT(1)`, and `COUNT(column_name)`?
<details>
<summary>Answer</summary>
- `COUNT(*)`: Counts the total number of physical rows in the relation, regardless of whether any or all column values are NULL.
- `COUNT(1)`: Semantically and physically identical to `COUNT(*)`. The engine counts candidate tuples without inspecting individual columns.
- `COUNT(column_name)`: Counts only the rows where `column_name` is strictly **NOT NULL**. Rows containing `NULL` in that specific column are ignored.
</details>

### Practical 3VL Puzzles

#### Q5: What does the following query return?
```sql
SELECT 
    CASE 
        WHEN NULL = NULL THEN 'Equal'
        WHEN NULL <> NULL THEN 'Not Equal'
        ELSE 'Unknown'
    END AS result;
```
<details>
<summary>Answer</summary>
It returns `'Unknown'`. 
Both `NULL = NULL` and `NULL <> NULL` evaluate to `UNKNOWN`. Because neither `WHEN` condition evaluates to `TRUE`, the `CASE` statement falls through to the `ELSE` branch.
</details>

#### Q6: Given the expression `SELECT * FROM tbl WHERE NOT (status = 'active')`, will this return rows where `status` is NULL? Explain.
<details>
<summary>Answer</summary>
**No**, it will NOT return rows where `status` is NULL.
If `status` is NULL:
1. `status = 'active'` evaluates to `UNKNOWN`.
2. `NOT (UNKNOWN)` evaluates to `UNKNOWN`.
3. The `WHERE` clause discards all rows that evaluate to `UNKNOWN`.

To include NULL rows, you must explicitly write:
`WHERE status <> 'active' OR status IS NULL`.
</details>

#### Q7: Write a query that computes the total payroll cost of employees, adding `salary` and `commission`, handling cases where either or both are NULL.
<details>
<summary>Answer</summary>

```sql
SELECT 
    employee_id,
    COALESCE(salary, 0.00) + COALESCE(commission, 0.00) AS total_compensation
FROM employees;
```
*(Without `COALESCE`, adding `salary + commission` where commission is NULL produces NULL!)*
</details>

#### Q8: Write an ANSI SQL query to find all orders where `discount` is different from `promotional_discount`, treating two NULLs as identical.
<details>
<summary>Answer</summary>

```sql
SELECT order_id, discount, promotional_discount
FROM orders
WHERE discount IS DISTINCT FROM promotional_discount;
```
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Basic Null Filtering
Select all customers who have not verified their email address (`email_verified_at` is NULL).
<details>
<summary>Solution</summary>

```sql
SELECT customer_id, email
FROM customers
WHERE email_verified_at IS NULL;
```
</details>

#### Exercise 2: Null Fallback Display
Write a query displaying `product_name` and `reorder_threshold`. If `reorder_threshold` is NULL, display the number `10` as a default.
<details>
<summary>Solution</summary>

```sql
SELECT 
    product_name,
    COALESCE(reorder_threshold, 10) AS reorder_threshold
FROM inventory;
```
</details>

### Intermediate Exercises

#### Exercise 3: Defending Against `NOT IN` Traps
Rewrite this dangerous query to guarantee safety against NULLs without changing the business goal:
```sql
SELECT * FROM users WHERE user_id NOT IN (SELECT supervisor_id FROM departments);
```
<details>
<summary>Solution</summary>

```sql
SELECT u.* 
FROM users u
WHERE NOT EXISTS (
    SELECT 1 FROM departments d 
    WHERE d.supervisor_id = u.user_id
);
```
</details>

#### Exercise 4: Accurate Average Calculation
Given a `reviews(review_id, product_id, rating)` table where `rating` can be NULL (user viewed but did not score), calculate:
1. The average rating of users who submitted a score.
2. The average rating treating unsubmitted scores as 0.
<details>
<summary>Solution</summary>

```sql
SELECT 
    product_id,
    ROUND(AVG(rating), 2) AS scored_avg,
    ROUND(AVG(COALESCE(rating, 0)), 2) AS zero_penalized_avg
FROM reviews
GROUP BY product_id;
```
</details>

### Advanced Exercises

#### Exercise 5: Multi-Column Null-Safe Join
Join two tables `staging_inventory` and `production_inventory` on `sku` and `warehouse_location`, where `warehouse_location` can be NULL, matching rows where both locations are NULL.
<details>
<summary>Solution</summary>

```sql
SELECT s.sku, s.quantity AS staging_qty, p.quantity AS prod_qty
FROM staging_inventory s
JOIN production_inventory p 
  ON s.sku = p.sku 
 AND s.warehouse_location IS NOT DISTINCT FROM p.warehouse_location;
```
</details>

#### Exercise 6: Three-Valued Logic Invariant Verification
Construct a query that demonstrates the truth value of `UNKNOWN AND FALSE` versus `UNKNOWN AND TRUE` using SQL conditional expressions.
<details>
<summary>Solution</summary>

```sql
SELECT 
    -- 1. UNKNOWN AND FALSE: Evaluates to FALSE
    CASE WHEN (NULL = 1) AND (1 = 2) THEN 'TRUE' ELSE 'NOT TRUE (FALSE)' END AS unknown_and_false,
    
    -- 2. UNKNOWN AND TRUE: Evaluates to UNKNOWN
    CASE 
        WHEN (NULL = 1) AND (1 = 1) THEN 'TRUE'
        WHEN NOT ((NULL = 1) AND (1 = 1)) THEN 'FALSE'
        ELSE 'UNKNOWN'
    END AS unknown_and_true;
```
</details>

---

## 12. Summary & Quick Reference

### 3VL Truth Table Quick Rules
- **AND**: Any `FALSE` makes it `FALSE`. Otherwise, any `UNKNOWN` makes it `UNKNOWN`.
- **OR**: Any `TRUE` makes it `TRUE`. Otherwise, any `UNKNOWN` makes it `UNKNOWN`.
- **NOT**: `NOT UNKNOWN` is `UNKNOWN`.
- **WHERE**: Accepts only `TRUE`.
- **CHECK**: Accepts `TRUE` and `UNKNOWN` (rejects only `FALSE`!).
- **Comparisons**: `= NULL` is always `UNKNOWN`. Use `IS NULL` or `IS NOT DISTINCT FROM`.
- **Anti-Joins**: Never use `NOT IN` with nullable subqueries; always use `NOT EXISTS`.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [Chapter 9.2: Comparison Functions and Operators](https://www.postgresql.org/docs/17/functions-comparison.html)
- Edgar F. Codd: *"Extending the Database Relational Model to Capture More Meaning"* (ACM TODS)
- Modern SQL: [Three-Valued Logic in SQL](https://modern-sql.com/concept/three-valued-logic)
- [09 - Pattern Matching: LIKE, ILIKE & Regex](./09-pattern-matching.md)
