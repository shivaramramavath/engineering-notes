# Ranking Functions: ROW_NUMBER, RANK, DENSE_RANK, and NTILE

| Metadata | Details |
| :--- | :--- |
| **Topic** | SQL Ranking & Distribution Functions (ROW_NUMBER, RANK, DENSE_RANK, NTILE, PERCENT_RANK, CUME_DIST) |
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

In relational theory, sets have no inherent order. However, real-world business queries constantly demand ordinal evaluation:
- Identifying the top 3 highest-spending customers in each country.
- Deduplicating incoming webhook events by selecting the latest event per UUID.
- Assigning student test percentiles, employee compensation tiers, or quartile buckets.
- Finding the $N^{\text{th}}$ highest salary in a company without hardcoded limit offsets.

SQL provides four primary ranking functions defined in ANSI SQL:2003: `ROW_NUMBER()`, `RANK()`, `DENSE_RANK()`, and `NTILE(k)`, as well as relative distribution functions `PERCENT_RANK()` and `CUME_DIST()`.

The critical challenge lies in **tie-handling**. If two sales representatives achieve the exact same quarterly revenue, should both receive rank 1 and the next person receive rank 2 or rank 3? Does row deduplication produce deterministic results when timestamps collide? Mastering the nuances of ranking functions is essential for writing accurate analytical queries and idempotent ETL pipelines.

---

## Core Concepts

### 1. `ROW_NUMBER()`
`ROW_NUMBER()` assigns a unique, continuous integer starting from 1 to each row within its partition, according to the `ORDER BY` clause:
- Sequence: `1, 2, 3, 4, 5, ...`
- **Ties**: If rows share identical ordering values, `ROW_NUMBER()` assigns distinct numbers arbitrarily (non-deterministically) unless unique tie-breaking columns are added to `ORDER BY`.
- Primary use case: Deduplication, pagination, and picking exactly one deterministic row per group.

### 2. `RANK()`
`RANK()` assigns the same rank to rows that share identical ordering values (peers). However, it leaves gaps in the sequence equal to the number of tied rows:
- Sequence with ties: `1, 2, 2, 4, 5, ...` (Rank 3 is skipped).
- Calculation: Rank is $1 + \text{number of rows strictly preceding the current peer group}$.
- Primary use case: Olympic-style leaderboard ranking (e.g., tied for silver medal means no bronze medal is awarded).

### 3. `DENSE_RANK()`
`DENSE_RANK()` assigns the same rank to rows that share identical ordering values, but **never skips** integers in the sequence:
- Sequence with ties: `1, 2, 2, 3, 4, ...` (No gaps).
- Primary use case: Finding the $N^{\text{th}}$ distinct highest value (e.g., finding all employees who earn the 2nd highest distinct salary).

### 4. `NTILE(k)`
`NTILE(k)` divides the rows of an ordered partition into $k$ roughly equal buckets, assigning bucket numbers from 1 to $k$:
- If the total row count $N$ is not evenly divisible by $k$, the first $N \pmod k$ buckets receive 1 extra row.
- Primary use case: Dividing customers into quartiles (`NTILE(4)`), deciles (`NTILE(10)`), or percentiles (`NTILE(100)`).

### 5. `PERCENT_RANK()` and `CUME_DIST()`
- **`PERCENT_RANK()`**: Evaluates relative rank from 0.0 to 1.0:
  $$\text{PERCENT\_RANK} = \frac{\text{RANK} - 1}{\text{Total Rows in Partition} - 1}$$
- **`CUME_DIST()`** (Cumulative Distribution): Evaluates the fraction of rows with values less than or equal to the current row's value:
  $$\text{CUME\_DIST} = \frac{\text{Number of rows with value } \le \text{ current value}}{\text{Total Rows in Partition}}$$

---

## How It Works Internally

The query execution engine processes ranking functions inside a streaming `WindowAgg` node after sorting the input tuples.

```
                    [ Stream of Tuples from Sort Node ]
                                     │
           (Sorted by PARTITION BY department_id, ORDER BY salary DESC)
                                     │
                                     ▼
                          [ WindowAgg Executor ]
                                     │
    Maintains internal counters:
    - current_row_num     = 1, 2, 3...
    - current_rank        = 1
    - current_dense_rank  = 1
    - previous_val        = salary of prior tuple
                                     │
                 Did the ORDER BY value change from previous_val?
                                     │
                 ┌───────────────────┴───────────────────┐
                 ▼                                       ▼
        [ NO: Tied Peer ]                       [ YES: Value Changed ]
                 │                                       │
      row_number += 1                         row_number += 1
      rank unchanged                          rank = current_row_num
      dense_rank unchanged                    dense_rank += 1
      (No gap formed)                         (Gap forms in RANK!)
```

### Key Internal Invariants:
1. **No Frame Specification Allowed**: Ranking functions strictly operate on the entire ordered partition. Specifying `ROWS` or `RANGE` inside `OVER()` with `ROW_NUMBER()`, `RANK()`, or `DENSE_RANK()` is a syntax violation.
2. **Deterministic Sort Dependency**: If `ORDER BY` contains duplicates and the engine uses an unstable sort (like Quicksort), running the query twice might assign different `ROW_NUMBER()` values to tied rows! Always append the table's Primary Key as a deterministic tie-breaker.

---

## Syntax & Usage

```sql
SELECT 
    department_id,
    employee_name,
    salary,
    -- Continuous row index
    ROW_NUMBER() OVER (PARTITION BY department_id ORDER BY salary DESC, emp_id ASC) AS row_num,
    -- Olympic rank with gaps
    RANK() OVER (PARTITION BY department_id ORDER BY salary DESC) AS standard_rank,
    -- Consecutive rank without gaps
    DENSE_RANK() OVER (PARTITION BY department_id ORDER BY salary DESC) AS dense_rank,
    -- Quartile bucket (1 through 4)
    NTILE(4) OVER (ORDER BY salary DESC) AS salary_quartile
FROM employees;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **`ROW_NUMBER()`** | Full ANSI support | Supported (MySQL 8.0+) | Supported (SQLite 3.25+) | Full ANSI support |
| **`RANK()` & `DENSE_RANK()`** | Full ANSI support | Supported (MySQL 8.0+) | Supported (SQLite 3.25+) | Full ANSI support |
| **`NTILE(k)`** | Full ANSI support | Supported (MySQL 8.0+) | Supported (SQLite 3.25+) | Full ANSI support |
| **`PERCENT_RANK()` & `CUME_DIST()`** | Full ANSI support | Supported (MySQL 8.0+) | Supported (SQLite 3.25+) | Full ANSI support |
| **`QUALIFY` Clause** | **Not Supported** (Must wrap in CTE) | **Not Supported** (Must wrap in CTE) | **Not Supported** (Must wrap in CTE) | **Not Supported** (Teradata / Snowflake / BigQuery only) |

---

## Practical Examples

### Example 1: Basic — Comparison of Ranking Behaviors on Tied Scores
Observe how `ROW_NUMBER()`, `RANK()`, and `DENSE_RANK()` handle tied scores.

```sql
CREATE TABLE exam_scores (
    student_id INT PRIMARY KEY,
    student_name VARCHAR(50) NOT NULL,
    score INT NOT NULL
);

INSERT INTO exam_scores VALUES
(1, 'Alice', 100),
(2, 'Bob', 90),
(3, 'Charlie', 90), -- Tied with Bob
(4, 'Diana', 85),
(5, 'Evan', 70);

SELECT 
    student_name,
    score,
    ROW_NUMBER() OVER (ORDER BY score DESC, student_id ASC) AS r_num,
    RANK() OVER (ORDER BY score DESC) AS r_rank,
    DENSE_RANK() OVER (ORDER BY score DESC) AS r_dense
FROM exam_scores;
```

```
Result:
 student_name | score | r_num | r_rank | r_dense 
--------------+-------+-------+--------+---------
 Alice        |   100 |     1 |      1 |       1
 Bob          |    90 |     2 |      2 |       2
 Charlie      |    90 |     3 |      2 |       2
 Diana        |    85 |     4 |      4 |       3  <- RANK skipped 3! DENSE did not.
 Evan         |    70 |     5 |      5 |       4
(5 rows)
```

---

### Example 2: Real-World — Deduplicating Webhook Events
A common production pattern: an event stream produces duplicate payloads. Select only the latest valid version of each unique entity.

```sql
CREATE TABLE webhook_payloads (
    event_id INT PRIMARY KEY,
    entity_uuid VARCHAR(36) NOT NULL,
    payload_json TEXT NOT NULL,
    received_at TIMESTAMP NOT NULL
);

INSERT INTO webhook_payloads VALUES
(1, 'uuid-001', '{"status": "PENDING"}', '2026-01-01 10:00:00'),
(2, 'uuid-001', '{"status": "SUCCESS"}', '2026-01-01 10:00:05'), -- Latest for uuid-001
(3, 'uuid-002', '{"status": "CREATED"}', '2026-01-01 10:01:00');

-- CTE Deduplication Pattern:
WITH ranked_events AS (
    SELECT 
        event_id,
        entity_uuid,
        payload_json,
        received_at,
        ROW_NUMBER() OVER (
            PARTITION BY entity_uuid 
            ORDER BY received_at DESC, event_id DESC
        ) AS dedup_rank
    FROM webhook_payloads
)
SELECT event_id, entity_uuid, payload_json, received_at
FROM ranked_events
WHERE dedup_rank = 1;
```

```
Result:
 event_id | entity_uuid |     payload_json      |     received_at     
----------+-------------+-----------------------+---------------------
        2 | uuid-001    | {"status": "SUCCESS"} | 2026-01-01 10:00:05
        3 | uuid-002    | {"status": "CREATED"} | 2026-01-01 10:01:00
(2 rows)
```

---

### Example 3: Edge Case — Finding the $N^{\text{th}}$ Highest Salary Using `DENSE_RANK`
Find all employees earning the 2nd highest distinct salary, even if multiple employees share the 1st or 2nd highest salary.

```sql
CREATE TABLE compensation (
    emp_id INT PRIMARY KEY,
    full_name VARCHAR(50) NOT NULL,
    salary NUMERIC(10,2) NOT NULL
);

INSERT INTO compensation VALUES
(1, 'Alice', 150000.00),
(2, 'Bob', 150000.00), -- Tied for 1st
(3, 'Charlie', 120000.00), -- 2nd highest distinct salary
(4, 'Diana', 120000.00), -- Tied for 2nd
(5, 'Evan', 90000.00);

WITH salary_tiers AS (
    SELECT 
        emp_id,
        full_name,
        salary,
        DENSE_RANK() OVER (ORDER BY salary DESC) AS salary_rank
    FROM compensation
)
SELECT emp_id, full_name, salary
FROM salary_tiers
WHERE salary_rank = 2;
```

```
Result:
 emp_id | full_name |  salary   
--------+-----------+-----------
      3 | Charlie   | 120000.00
      4 | Diana     | 120000.00
(2 rows)
```
*Note*: If `RANK()` had been used, Bob and Alice would be rank 1, and Charlie and Diana would be rank 3. Filtering by `salary_rank = 2` would have returned **0 rows**!

---

### Example 4: Failure Mode — Uneven Buckets with `NTILE`
When partitioning 7 rows into 3 buckets using `NTILE(3)`, buckets cannot be equal ($7 / 3 = 2 \text{ remainder } 1$). The remainder is distributed to bucket 1.

```sql
CREATE TABLE scores_seven (
    id INT PRIMARY KEY,
    val INT NOT NULL
);

INSERT INTO scores_seven VALUES 
(1, 10), (2, 20), (3, 30), (4, 40), (5, 50), (6, 60), (7, 70);

SELECT 
    val,
    NTILE(3) OVER (ORDER BY val ASC) AS bucket
FROM scores_seven;
```

```
Result:
 val | bucket 
-----+--------
  10 |      1
  20 |      1
  30 |      1  <- Bucket 1 receives 3 rows
  40 |      2
  50 |      2  <- Bucket 2 receives 2 rows
  60 |      3
  70 |      3  <- Bucket 3 receives 2 rows
(7 rows)
```

---

### Example 5: Optimized — Top 2 Items Per Category with Composite Index
Select the top 2 highest-priced products per category.

```sql
CREATE TABLE catalog_items (
    item_id INT PRIMARY KEY,
    category_id INT NOT NULL,
    item_name VARCHAR(50) NOT NULL,
    price NUMERIC(10,2) NOT NULL
);

-- Index eliminates sort node!
CREATE INDEX idx_catalog_cat_price ON catalog_items (category_id, price DESC);

WITH ranked_items AS (
    SELECT 
        category_id,
        item_name,
        price,
        ROW_NUMBER() OVER (
            PARTITION BY category_id 
            ORDER BY price DESC
        ) AS rank_in_category
    FROM catalog_items
)
SELECT category_id, item_name, price
FROM ranked_items
WHERE rank_in_category <= 2;
```

---

## Best Practices

1. **Always Add a Deterministic Tie-Breaker to `ROW_NUMBER()`**: If sorting by `created_at` which can have ties, always append the primary key: `ORDER BY created_at DESC, id DESC`.
2. **Use `DENSE_RANK()` for $N^{\text{th}}$ Highest Values**: Never use `LIMIT 1 OFFSET (N-1)` or `RANK()` when multiple rows can share the top values; `DENSE_RANK()` correctly handles tied values without skipping.
3. **Never Specify Frames with Ranking Functions**: `ROW_NUMBER()`, `RANK()`, `DENSE_RANK()`, and `NTILE()` disallow `ROWS` or `RANGE` framing clauses.
4. **Use CTEs for Filtering**: Because ANSI SQL does not support `QUALIFY`, always calculate rankings in a CTE and filter on `WHERE rank_col <= N` in the outer query.
5. **Beware of `NTILE` with Small Partitions**: If partition row count is less than the bucket count $k$ (e.g., `NTILE(10)` on 5 rows), buckets 6 through 10 will remain completely empty!

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Attempting to filter `ROW_NUMBER() = 1` in `WHERE` | Window functions are evaluated after `WHERE`; engine raises a syntax error. | Wrap query in a CTE and filter in the outer query. |
| 2 | Using `RANK()` to find the 2nd highest salary | If multiple employees share rank 1, rank 2 does not exist (skips to rank 3). | Use `DENSE_RANK()` which never skips rank integers. |
| 3 | Non-deterministic deduplication with `ROW_NUMBER()` | When ordering keys are tied, the database picks arbitrary rows on each run. | Append a unique column (e.g., `id`) to `ORDER BY`. |
| 4 | Adding `ROWS BETWEEN ...` to `ROW_NUMBER()` | Ranking functions operate strictly on the full partition; framing raises a syntax error. | Remove framing clause from `OVER()`. |
| 5 | Assuming `NTILE(k)` produces equal-sized buckets | If $N$ is not divisible by $k$, earlier buckets receive extra items. | Account for remainder distribution in application logic. |
| 6 | Using `MAX(salary)` subqueries for top-N ranking | Fails to scale beyond $N=1$ and forces multiple table scans. | Use `DENSE_RANK() OVER (ORDER BY salary DESC)` in a CTE. |

---

## Performance & Security Notes

### Index-Assisted Top-N Execution
In PostgreSQL, querying top-N items per group using a CTE with `ROW_NUMBER()` can still perform a full index scan over all groups.
```sql
EXPLAIN ANALYZE
WITH ranked AS (
    SELECT *, ROW_NUMBER() OVER (PARTITION BY category_id ORDER BY price DESC) as rn
    FROM catalog_items
)
SELECT * FROM ranked WHERE rn <= 2;
```
If `catalog_items` contains 1,000,000 rows across 10,000 categories:
- A naive `WindowAgg` scans all 1,000,000 rows.
- **Optimization (Loose Index Scan / Lateral Join)**: If performance is critical, rewrite using `CROSS JOIN LATERAL` over distinct categories:
```sql
SELECT c.category_id, r.*
FROM categories c
CROSS JOIN LATERAL (
    SELECT item_name, price
    FROM catalog_items i
    WHERE i.category_id = c.category_id
    ORDER BY price DESC
    LIMIT 2
) r;
```
This performs an index seek per category, reading only $10,000 \times 2 = 20,000$ index tuples instead of 1,000,000!

---

## Comparison Matrix

| Function | Sequence (With Ties) | Skips Ranks? | Frame Clause Allowed? | Deterministic on Ties? |
| :--- | :--- | :--- | :--- | :--- |
| **`ROW_NUMBER()`** | `1, 2, 3, 4, 5` | Never | No | **No** (Unless tie-breaker added) |
| **`RANK()`** | `1, 2, 2, 4, 5` | **Yes** (Gaps on ties) | No | Yes |
| **`DENSE_RANK()`** | `1, 2, 2, 3, 4` | Never | No | Yes |
| **`NTILE(k)`** | `1, 1, 2, 2, 3` | Never | No | Yes |
| **`PERCENT_RANK()`**| `0.0, 0.25, 0.25, 0.75, 1.0` | Skips values | No | Yes |

---

## Interview Questions & Deep Dives

### Q1: What is the exact difference between `RANK()` and `DENSE_RANK()`?
<details>
<summary>Click to view answer</summary>

Both assign identical rank values to tied rows. However, when moving to the subsequent distinct value:
- `RANK()` leaves a gap equal to the number of tied rows (e.g., `1, 2, 2, 4`), reflecting the total count of preceding rows.
- `DENSE_RANK()` does not leave a gap and assigns the immediately consecutive integer (e.g., `1, 2, 2, 3`).
</details>

### Q2: How do you find the 3rd highest distinct salary in a company without using `LIMIT` or `OFFSET`?
<details>
<summary>Click to view answer</summary>

Use `DENSE_RANK()` inside a CTE:
```sql
WITH ranked_salaries AS (
    SELECT salary, DENSE_RANK() OVER (ORDER BY salary DESC) as rnk
    FROM employees
)
SELECT DISTINCT salary
FROM ranked_salaries
WHERE rnk = 3;
```
`DENSE_RANK()` ensures that if 5 employees share the top salary and 3 share the second, the 3rd distinct salary receives rank 3.
</details>

### Q3: Why is `ROW_NUMBER()` non-deterministic when ties exist in `ORDER BY`?
<details>
<summary>Click to view answer</summary>

If the sorting expression has identical values across multiple rows (e.g., `ORDER BY created_at`), relational theory does not define an order among them. Database sorting algorithms (such as Quicksort or Timsort) may order tied rows in arbitrary sequence based on physical page order, parallel worker scheduling, or vacuum status. To make `ROW_NUMBER()` deterministic, unique columns must be appended (e.g., `ORDER BY created_at DESC, id ASC`).
</details>

### Q4: How does `NTILE(k)` distribute rows when the partition count is not divisible by $k$?
<details>
<summary>Click to view answer</summary>

If there are $N$ rows and $k$ buckets, the engine computes $q = \lfloor N / k \rfloor$ and $r = N \pmod k$.
- The first $r$ buckets will each receive $q + 1$ rows.
- The remaining $k - r$ buckets will each receive $q$ rows.
Thus, extra rows are always distributed one by one to the earliest buckets starting from bucket 1.
</details>

### Q5: What is the `QUALIFY` clause and why doesn't PostgreSQL support it?
<details>
<summary>Click to view answer</summary>

`QUALIFY` is a proprietary clause implemented in Teradata, Snowflake, Databricks, and Google BigQuery that allows filtering the results of window functions directly (analogous to `HAVING` for `GROUP BY`), avoiding CTE boilerplate:
```sql
-- Snowflake / BigQuery syntax:
SELECT * FROM employees QUALIFY ROW_NUMBER() OVER (PARTITION BY dept_id ORDER BY salary DESC) = 1;
```
PostgreSQL, MySQL, SQLite, and SQL Server do not support `QUALIFY` because it is not part of the ISO/IEC SQL standard. In standard SQL, you must wrap the query in a CTE or derived table.
</details>

### Q6: Can you call `ROW_NUMBER()` without an `ORDER BY` clause inside `OVER ()`?
<details>
<summary>Click to view answer</summary>

In ANSI SQL and PostgreSQL, `ROW_NUMBER()` **requires** an `ORDER BY` clause inside `OVER()`; omitting it raises a syntax error:
`ERROR: operator does not exist: OVER () requires an ORDER BY clause for ranking`.
(In SQL Server, `ORDER BY (SELECT NULL)` is a commonly used workaround to bypass this restriction).
</details>

### Q7: What is the formula used by `PERCENT_RANK()`?
<details>
<summary>Click to view answer</summary>

$$\text{PERCENT\_RANK} = \frac{\text{RANK} - 1}{N - 1}$$
Where $\text{RANK}$ is the value computed by `RANK()` and $N$ is the total number of rows in the partition. The first row in any partition always evaluates to `0.0`, and the last row evaluates to `1.0` (unless the partition contains only 1 row, in which case it evaluates to `0.0`).
</details>

### Q8: How can you delete duplicate rows in a table using `ROW_NUMBER()` in PostgreSQL?
<details>
<summary>Click to view answer</summary>

```sql
WITH duplicates AS (
    SELECT ctid,
           ROW_NUMBER() OVER (
               PARTITION BY email 
               ORDER BY created_at DESC, id DESC
           ) AS rn
    FROM users
)
DELETE FROM users
WHERE ctid IN (SELECT ctid FROM duplicates WHERE rn > 1);
```
Using the internal physical row identifier `ctid` allows PostgreSQL to delete specific duplicate tuples without relying on an existing unique primary key.
</details>

---

## Practice Exercises

### Exercise 1: Top 3 Sales Reps per Region
Given `sales_reps(rep_id, region_id, total_sales)`, write a query to return the top 3 sales representatives in each region by sales. If there is a tie for 3rd place, include all tied reps.

<details>
<summary>Click to view solution</summary>

```sql
WITH ranked_reps AS (
    SELECT 
        rep_id,
        region_id,
        total_sales,
        DENSE_RANK() OVER (
            PARTITION BY region_id 
            ORDER BY total_sales DESC
        ) AS sales_rank
    FROM sales_reps
)
SELECT rep_id, region_id, total_sales, sales_rank
FROM ranked_reps
WHERE sales_rank <= 3;
```
</details>

---

### Exercise 2: Deduplicate Customer Phone Numbers
Given `customer_phones(phone_id, customer_id, phone_number, is_verified, updated_at)`, select one phone number per customer, preferring verified numbers first, followed by the most recently updated number.

<details>
<summary>Click to view solution</summary>

```sql
WITH prioritized_phones AS (
    SELECT 
        phone_id,
        customer_id,
        phone_number,
        is_verified,
        updated_at,
        ROW_NUMBER() OVER (
            PARTITION BY customer_id 
            ORDER BY is_verified DESC, updated_at DESC, phone_id DESC
        ) AS preference_rank
    FROM customer_phones
)
SELECT customer_id, phone_number, is_verified, updated_at
FROM prioritized_phones
WHERE preference_rank = 1;
```
</details>

---

### Exercise 3: Customer Spending Quartiles
Given `customers(customer_id, annual_spend)`, segment all customers into 4 spending quartiles (1 = Lowest 25%, 4 = Highest 25%) using `NTILE`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    customer_id,
    annual_spend,
    NTILE(4) OVER (ORDER BY annual_spend ASC) AS spend_quartile
FROM customers;
```
</details>

---

### Exercise 4: Identify the Gap After Ties
Given `tournament_scores(player_id, score)`, compute the player rank with gaps using `RANK()`. Demonstrate that the player following two tied players at rank 2 receives rank 4.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    player_id,
    score,
    RANK() OVER (ORDER BY score DESC) AS leaderboard_rank
FROM tournament_scores;
```
</details>

---

### Exercise 5: Relative Percentile via `PERCENT_RANK`
Calculate the percentile ranking of server CPU spikes from `server_metrics(server_id, metric_time, cpu_usage)`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    server_id,
    metric_time,
    cpu_usage,
    ROUND(PERCENT_RANK() OVER (ORDER BY cpu_usage ASC)::numeric, 4) AS cpu_percentile
FROM server_metrics;
```
</details>

---

### Exercise 6: Deterministic Keyset Pagination Using Row Number
Explain why keyset pagination `WHERE (salary, id) < ($last_salary, $last_id)` is preferred over `ROW_NUMBER() OVER (...) BETWEEN 100000 AND 100020` on multi-million row tables.

<details>
<summary>Click to view solution</summary>

`ROW_NUMBER() OVER (...) BETWEEN 100000 AND 100020` requires the database engine to sort and assign sequential row numbers to all 100,020 tuples from the beginning of the table before discarding the first 100,000, resulting in high CPU and I/O costs. 

Keyset pagination utilizes a B-Tree index on `(salary DESC, id DESC)` to jump directly to the target record in $O(\log N)$ time, reading only the 20 requested rows without materializing or scanning prior tuples.
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                RANKING FUNCTIONS CHEAT SHEET                            |
+-------------------+----------------------------+----------------------------------------+
| Function          | Behavior on Tied Values    | Typical Use Case                       |
+-------------------+----------------------------+----------------------------------------+
| ROW_NUMBER()      | Arbitrary sequence (1,2,3) | Deduplication, single record pick      |
| RANK()            | Identical rank, leaves gaps| Leaderboards, medals, competition      |
| DENSE_RANK()      | Identical rank, no gaps    | Nth highest distinct value             |
| NTILE(k)          | Divides into k buckets     | Quartiles (4), Deciles (10)            |
| PERCENT_RANK()    | (Rank - 1) / (N - 1)       | Normalized distribution [0.0 to 1.0]   |
+-------------------+----------------------------+----------------------------------------+
```

- **Deduplication Golden Rule**: Always append a unique tie-breaker to `ROW_NUMBER()`.
- **$N^{\text{th}}$ Highest Salary**: Always use `DENSE_RANK()`, never `RANK()`.
- **Framing Warning**: Never specify `ROWS` or `RANGE` inside `OVER()` with ranking functions.

---

## Further Reading
- PostgreSQL 17 Documentation: *General-Purpose Window Functions*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 6.10 `<window function>`
- Markus Winand: *Pagination Done the PostgreSQL Way (Keyset Pagination vs Ranking)*
- Itzik Ben-Gan: *T-SQL Fundamentals: Ranking Functions and Solutions to Top-N Problems*
