# Writing Readable, Maintainable SQL: Refactoring & Architecture

| Metadata | Details |
| :--- | :--- |
| **Topic** | SQL Code Cleanliness, Refactoring Anti-Patterns, Common Table Expression Decomposition, and Style Standards |
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

In software engineering, code is read far more often than it is written. While backend engineers invest heavily in clean object-oriented architecture, SOLID principles, and functional purity in languages like Go, Rust, Java, and Python, SQL is all too frequently treated as a secondary dumping ground.

The result is the dreaded **"SQL Spaghetti Monster"**:
- Monolithic 600-line queries nested 5 levels deep inside derived tables.
- Cryptic one-letter table aliases (`FROM a JOIN b ON a.x = b.y JOIN c ON b.z = c.w`).
- Obscure positional ordering (`ORDER BY 1, 4, 7`).
- Slapping `SELECT DISTINCT` onto a broken query to patch over accidental Cartesian multiplication.
- Zero inline documentation explaining non-obvious business logic or date truncation heuristics.

Unreadable SQL is not merely an aesthetic problem; it is a primary vector for **catastrophic production failures**. When queries are impenetrable, developers cannot spot missing join predicates, optimizer statistics mismatches, or full-table scans. Refactoring becomes terrifying, and bugs hide in plain sight.

Writing clean, maintainable SQL is a core discipline. By structuring queries as linear, single-responsibility pipelines using Common Table Expressions (CTEs), adopting robust formatting rules (e.g., SQLFluff), and documenting business invariants, engineers create database systems that remain performant, audit-proof, and resilient for years.

---

## Core Concepts

### 1. The "SQL Onion" Anti-Pattern vs Linear CTE Pipelines
- **The SQL Onion**: A deeply nested hierarchy of derived subqueries:
  ```sql
  -- THE ONION (Inside-Out Flow)
  SELECT ... FROM (
      SELECT ... FROM (
          SELECT ... FROM base_table
      ) sub1
  ) sub2
  ```
  The human brain must read from the innermost subquery outward, tracking column aliases as they pass through multiple arbitrary layers.
- **The Linear CTE Pipeline**: A modular sequence where each step performs one discrete transformation and names the intermediate business concept:
  ```sql
  WITH 
  raw_active_users AS (...),
  monthly_activity AS (...),
  churn_risk_scores AS (...)
  SELECT * FROM churn_risk_scores;
  ```

### 2. Semantic Table Aliases
Avoid single-letter aliases (`a`, `b`, `c`, `x`, `y`). If a table is named `order_line_items`, alias it as `item` or `line_item`. If `customers` is joined twice (once as billing customer, once as shipping recipient), alias them explicitly as `bill_cust` and `ship_cust`.

### 3. Explicit Projections vs `SELECT *`
In production application code, **never write `SELECT *`**:
- It breaks query coverage in index-only scans, forcing the engine to fetch heap pages.
- It causes subtle application bugs when schema migrations add new columns.
- It transfers unnecessary bandwidth across network sockets and deserialization layers.
Always enumerate the exact columns required by the consumer.

### 4. Explicit Join Syntax vs Implicit Comma Joins
- **Implicit Comma Join (Deprecated ANSI-89)**: `FROM table_a a, table_b b WHERE a.id = b.id`. An omitted `WHERE` condition silently causes a devastating Cartesian cross product!
- **Explicit Join (ANSI-92)**: `FROM table_a a JOIN table_b b ON a.id = b.id`. The compiler enforces that an `ON` condition exists, preventing accidental Cartesian products.

### 5. Documenting Intent vs Restating Syntax
Comments should explain **why** a specific filter exists, not *what* the SQL syntax is doing:
- *Bad Comment*: `-- Filter where status is completed`
- *Good Comment*: `-- Only include COMPLETED orders; CANCELLED and REFUNDED orders do not contribute to revenue per Accounting Rule 404`

---

## How It Works Internally

Does breaking a monolithic query into clean, modular CTEs harm query execution performance?

```
      [ Refactored Query: 3 Chained CTEs ]
                        │
                        ▼
            [ Query Parser & Rewriter ]
                        │
    1. Checks semantic validity and type constraints.
    2. Rewrites CTEs into an internal Abstract Syntax Tree (AST).
    3. Inlines non-materialized, single-reference CTEs directly!
                        │
                        ▼
           [ Cost-Based Optimizer (CBO) ]
                        │
    - Merges CTE definitions into the main relational tree.
    - Pushes WHERE filters across CTE boundaries.
    - Evaluates join order based on table statistics.
                        │
                        ▼
      [ Produces Flat, Highly Optimized Physical Plan ]
```

In modern relational engines (PostgreSQL 12+, MySQL 8+, SQLite 3.35+, SQL Server):
- **CTE Inlining**: The optimizer flattens single-reference CTEs into the primary query tree. The physical execution plan of a 4-stage CTE pipeline is typically **identical** to a deeply nested subquery onion.
- **Maintainability Without Penalty**: You gain 100% of the architectural readability benefits of modular code with 0% runtime performance degradation.

---

## Syntax & Usage

```sql
-- Production SQL Style Standard:
-- 1. Keywords in UPPERCASE
-- 2. Clause keywords aligned
-- 3. Trailing commas for clean git diffs
-- 4. Meaningful, semantic aliases
-- 5. Explicit join conditions

WITH 
eligible_customers AS (
    SELECT 
        cust.customer_id,
        cust.email,
        cust.created_at
    FROM customers cust
    WHERE cust.is_active = TRUE
      AND cust.deleted_at IS NULL
),
customer_order_metrics AS (
    SELECT 
        ord.customer_id,
        COUNT(ord.order_id) AS total_orders,
        COALESCE(SUM(ord.total_amount), 0.00) AS gross_spend,
        MAX(ord.order_date) AS latest_order_date
    FROM orders ord
    WHERE ord.order_status = 'COMPLETED'
    GROUP BY ord.customer_id
)
SELECT 
    ec.customer_id,
    ec.email,
    com.total_orders,
    com.gross_spend,
    com.latest_order_date
FROM eligible_customers ec
LEFT JOIN customer_order_metrics com ON ec.customer_id = com.customer_id
ORDER BY com.gross_spend DESC NULLS LAST;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **CTE Inlining / Optimization**| Automatic inlining (since PG 12) | Inlines non-recursive CTEs | Inlines non-recursive CTEs | Inlines non-recursive CTEs |
| **Leading/Trailing Comma Diff** | Trailing comma standard | Trailing comma standard | Trailing comma standard | Trailing comma standard |
| **Identifier Quoting** | Double quotes (`"my_col"`) | Backticks (`` `my_col` ``) | Double quotes or brackets | Square brackets (`[my_col]`) |
| **Positional GROUP/ORDER BY** | Supported (`GROUP BY 1, 2`) | Supported (`GROUP BY 1, 2`) | Supported (`GROUP BY 1, 2`) | Order By only (Group By positional deprecated) |
| **Linter Ecosystem** | Full SQLFluff / pgFormatter | Full SQLFluff | Full SQLFluff | Full SQLFluff / TSQL-Lint |

---

## Practical Examples

### Example 1: Basic — Refactoring the "SQL Onion"
Transform an unreadable nested derived subquery into an elegant, linear pipeline.

#### Before: The Unreadable Onion
```sql
SELECT c_name, total_val, order_cnt
FROM (
    SELECT c.name AS c_name, SUM(o.val) AS total_val, COUNT(o.id) AS order_cnt
    FROM (SELECT id, name FROM customers WHERE active = 1) c
    JOIN (SELECT id, customer_id, val FROM orders WHERE status = 'PAID') o 
      ON c.id = o.customer_id
    GROUP BY c.name
) sub
WHERE total_val > 1000;
```

#### After: Clean Modular CTE Pipeline
```sql
WITH 
active_customers AS (
    SELECT customer_id, name
    FROM customers
    WHERE is_active = TRUE
),
paid_orders AS (
    SELECT order_id, customer_id, amount
    FROM orders
    WHERE status = 'PAID'
),
customer_revenue AS (
    SELECT 
        cust.name AS customer_name,
        SUM(ord.amount) AS total_revenue,
        COUNT(ord.order_id) AS order_count
    FROM active_customers cust
    JOIN paid_orders ord ON cust.customer_id = ord.customer_id
    GROUP BY cust.name
)
SELECT customer_name, total_revenue, order_count
FROM customer_revenue
WHERE total_revenue > 1000.00
ORDER BY total_revenue DESC;
```

---

### Example 2: Real-World — Eliminating the "JOIN + DISTINCT" Band-Aid
A developer notices duplicate rows when joining authors to books, and slaps `DISTINCT` on top. Refactor to fix the actual cardinality root cause.

```sql
CREATE TABLE authors (
    author_id INT PRIMARY KEY,
    author_name VARCHAR(100) NOT NULL
);

CREATE TABLE publications (
    pub_id INT PRIMARY KEY,
    author_id INT NOT NULL,
    pub_year INT NOT NULL,
    genre VARCHAR(50) NOT NULL
);

INSERT INTO authors VALUES (1, 'Isaac Asimov'), (2, 'Arthur C. Clarke'), (3, 'Jane Austen');
INSERT INTO publications VALUES 
(101, 1, 1951, 'Sci-Fi'),
(102, 1, 1952, 'Sci-Fi'),
(103, 2, 1968, 'Sci-Fi');

-- BAD CODE: JOIN duplicated Asimov 2x; DISTINCT forces a full sort to fix it!
SELECT DISTINCT a.author_id, a.author_name
FROM authors a
JOIN publications p ON a.author_id = p.author_id
WHERE p.pub_year > 1950 AND p.genre = 'Sci-Fi';

-- REFACTORED CODE: Use Semi-Join (EXISTS) to preserve cardinality cleanly!
SELECT 
    auth.author_id,
    auth.author_name
FROM authors auth
WHERE EXISTS (
    SELECT 1 
    FROM publications pub 
    WHERE pub.author_id = auth.author_id 
      AND pub.pub_year > 1950 
      AND pub.genre = 'Sci-Fi'
);
```

```
Result:
 author_id |   author_name    
-----------+------------------
         1 | Isaac Asimov
         2 | Arthur C. Clarke
(2 rows)
```

---

### Example 3: Edge Case — Isolating Multi-Table Aggregation Branches
When a query aggregates across multiple one-to-many child tables (e.g., `orders` and `support_tickets`), joining them together directly triggers Cartesian multiplication.

```sql
-- DANGEROUS CODE (Values inflated by Cartesian fan-out!):
-- If customer has 2 orders and 3 tickets, 6 joined rows are created!
-- SUM(amount) will be multiplied by 3!
-- SUM(ticket_score) will be multiplied by 2!

-- REFACTORED CLEAN ARCHITECTURE:
WITH 
order_summary AS (
    SELECT customer_id, SUM(amount) AS total_order_spend
    FROM orders
    GROUP BY customer_id
),
ticket_summary AS (
    SELECT customer_id, COUNT(*) AS total_tickets_logged
    FROM support_tickets
    GROUP BY customer_id
)
SELECT 
    cust.customer_id,
    cust.name,
    COALESCE(os.total_order_spend, 0.00) AS total_order_spend,
    COALESCE(ts.total_tickets_logged, 0) AS total_tickets_logged
FROM customers cust
LEFT JOIN order_summary os ON cust.customer_id = os.customer_id
LEFT JOIN ticket_summary ts ON cust.customer_id = ts.customer_id;
```

---

### Example 4: Failure Mode — The Fragility of Positional `ORDER BY`
Demonstrating how `ORDER BY 2` causes silent data presentation bugs when a developer modifies column projections.

```sql
-- ORIGINAL INTENT: Sort by 'created_at' (Position 2)
SELECT user_id, created_at, email FROM users ORDER BY 2 DESC;

-- FUTURE DEVELOPER MODIFIES QUERY: Inserts 'username' as column 2!
SELECT user_id, username, created_at, email FROM users ORDER BY 2 DESC;
-- BUG: The query is now sorting alphabetically by USERNAME rather than CREATED_AT!

-- CORRECT ROBUST STANDARD: Always name columns explicitly!
SELECT user_id, username, created_at, email 
FROM users 
ORDER BY created_at DESC;
```

---

### Example 5: Optimized — Self-Documenting Defensive SQL
An executive churn reporting query with inline business documentation and defensive formatting.

```sql
/*
 * AUDIT REPORT: Monthly Active User Churn Risk
 * Owner: Data Engineering Team
 * Update Frequency: Nightly batch ETL
 * SLA: Maximum 2 minutes execution window
 */
WITH 
active_billing_period AS (
    -- Accounting convention: Standard 30-day trailing activity window
    SELECT 
        CURRENT_DATE - INTERVAL '30 days' AS period_start,
        CURRENT_DATE AS period_end
),
trailing_logins AS (
    SELECT 
        l.user_id,
        COUNT(l.login_id) AS login_count,
        MAX(l.login_timestamp) AS most_recent_login
    FROM user_logins l
    CROSS JOIN active_billing_period bp
    WHERE l.login_timestamp >= bp.period_start
    GROUP BY l.user_id
)
SELECT 
    u.user_id,
    u.company_name,
    u.plan_tier,
    COALESCE(tl.login_count, 0) AS logins_last_30_days,
    tl.most_recent_login,
    CASE 
        WHEN tl.login_count IS NULL THEN 'HIGH_RISK_DORMANT'
        WHEN tl.login_count < 3 THEN 'MEDIUM_RISK_INACTIVE'
        ELSE 'HEALTHY_ACTIVE'
    END AS churn_risk_assessment
FROM users u
LEFT JOIN trailing_logins tl ON u.user_id = tl.user_id
WHERE u.account_status = 'ACTIVE'
ORDER BY 
    CASE WHEN tl.login_count IS NULL THEN 1 ELSE 2 END ASC,
    tl.login_count ASC NULLS FIRST;
```

---

## Best Practices

1. **Adopt a Consistent Linter in CI/CD**: Run `sqlfluff lint` or automated formatting in pull requests to enforce uppercase keywords, indentation, and identifier casing.
2. **Never Use `SELECT *` in Application Code**: Explicitly project every required column to maximize index-only scan opportunities and protect against schema migration bugs.
3. **Use Meaningful, Self-Explanatory Aliases**: Replace `t1`, `t2`, `a`, `b` with descriptive nouns (`ord`, `cust`, `line_item`, `event`).
4. **Name Columns in `ORDER BY` and `GROUP BY`**: Never use positional ordinal numbers (`GROUP BY 1, 2`). Positional numbers break silently when projection columns are added or reordered.
5. **Decouple Multi-Table Aggregations**: Pre-aggregate separate child tables in independent CTEs before joining them to avoid catastrophic Cartesian duplication.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Slapping `DISTINCT` on an `INNER JOIN` | Hides underlying join cardinality mistakes and forces massive sorting passes. | Fix join conditions or replace with a `WHERE EXISTS (...)` semi-join. |
| 2 | Positional references (`ORDER BY 1, 3`) | Modifying the `SELECT` list silently changes the sorting order without raising an error. | Always order by explicit column names: `ORDER BY created_at DESC`. |
| 3 | Writing implicit comma joins (`FROM a, b`) | Accidental omission of join criteria produces silent Cartesian cross products. | Always write explicit ANSI joins: `FROM a JOIN b ON a.id = b.id`. |
| 4 | Deeply nested "SQL Onions" | Inner logic is inverted, making debugging, reviewing, and testing nearly impossible. | Decompose into top-down, single-responsibility CTEs. |
| 5 | Writing `SELECT *` in production code | Disables index-only scans, increases network payload, and causes ORM deserialization bugs. | Explicitly enumerate projected columns. |
| 6 | Omitting comments on non-obvious business logic | Future engineers cannot tell whether a filter is intentional or a bug. | Document business rules, accounting standards, and date assumptions. |

---

## Performance & Security Notes

### The Git Diff Problem in SQL Codebases
In large engineering teams, SQL queries undergo frequent version control modifications. Formatting directly affects merge conflict frequency:
```sql
-- POOR FORMATTING (Forces merge conflicts on comma removal/addition):
SELECT id, name, email, created_at
FROM users;

-- OPTIMAL PRODUCTION FORMATTING (Clean 1-line git diffs):
SELECT 
    id,
    name,
    email,
    created_at
FROM users;
```
When a developer adds a new column `account_tier`, putting each column on its own line ensures the Git diff touches **only 1 added line**, eliminating merge conflicts in pull requests.

---

## Comparison Matrix

| Quality Dimension | Monolithic Nested SQL | Refactored CTE Pipeline |
| :--- | :--- | :--- |
| **Cognitive Load** | High (Read inside-out) | Low (Read top-to-bottom) |
| **Unit Testability** | Impossible to isolate inner queries | Sub-CTEs can be tested independently |
| **Execution Speed** | Identical | Identical (Flattened by modern CBOs) |
| **Maintainability** | Degrades exponentially with query length | Scales linearly with query length |
| **Cardinality Bugs** | Frequently hidden behind `DISTINCT` | Explicitly prevented by isolated aggregation |

---

## Interview Questions & Deep Dives

### Q1: Does refactoring a subquery into a CTE degrade query performance in modern PostgreSQL?
<details>
<summary>Click to view answer</summary>

No. In PostgreSQL 8.4 through 11, CTEs were unconditionally materialized (acting as optimization fences). However, beginning with PostgreSQL 12, the cost-based optimizer automatically inlines non-recursive, side-effect-free CTEs that are referenced only once. The resulting physical execution plan and query runtime are **identical** to an inlined subquery, giving you clean code architecture with zero performance penalty.
</details>

### Q2: Why is `SELECT DISTINCT` considered a major code smell when used after a `JOIN`?
<details>
<summary>Click to view answer</summary>

When `DISTINCT` follows a `JOIN`, it usually indicates that the join condition produced unintended Cartesian duplicates (a 1-to-many relationship where only 1-to-1 was expected). Slapping `DISTINCT` on top forces the database engine to perform an expensive hash or external sort operation across all joined rows to remove the duplicates it just created. 
The correct engineering approach is to preserve cardinality at the source using a semi-join (`WHERE EXISTS (...)`) or by pre-aggregating the child table in a CTE.
</details>

### Q3: What is the risk of using positional numbers in `ORDER BY` and `GROUP BY`?
<details>
<summary>Click to view answer</summary>

Positional references (e.g., `SELECT id, email, created_at FROM users ORDER BY 3`) couple ordering semantics to column index position. If another engineer adds a column or reorders the projection (e.g., `SELECT id, username, email, created_at`), the query now silently sorts by `email` instead of `created_at` without throwing any syntax or runtime errors. Always specify explicit column identifiers.
</details>

### Q4: Why should explicit `JOIN ... ON` always be used instead of comma joins (`FROM table1, table2`)?
<details>
<summary>Click to view answer</summary>

Comma joins (ANSI-89 syntax) separate table declarations from filtering logic. If a developer accidentally forgets the join condition in a long `WHERE` clause, the database executes an unintended Cartesian product (multiplying tables together), which can exhaust server memory, lock disk buffers, and cause outages. Explicit `JOIN ... ON` (ANSI-92 syntax) enforces at compile time that an `ON` clause is present.
</details>

### Q5: How should complex multi-table joins involving multiple 1-to-many relationships be structured?
<details>
<summary>Click to view answer</summary>

They should be structured by **decoupling independent branches**:
1. Aggregate each child table separately in its own dedicated CTE (e.g., aggregate orders per customer in `order_summary`, and aggregate tickets per customer in `ticket_summary`).
2. Join the pre-aggregated summary CTEs to the parent table in the final `SELECT`.
This guarantees that child tables never multiply against each other.
</details>

### Q6: What automated tooling exists to maintain SQL formatting standards in CI/CD?
<details>
<summary>Click to view answer</summary>

The industry standard is **SQLFluff**, an open-source, dialect-aware SQL linter and auto-formatter. It can be integrated into GitHub Actions / GitLab CI pipelines, pre-commit hooks, and IDEs to automatically enforce casing, indentation, trailing commas, and anti-pattern rules across thousands of SQL files.
</details>

### Q7: Why is `SELECT *` harmful even if the application discards unused columns?
<details>
<summary>Click to view answer</summary>

1. **Prevents Index-Only Scans**: If all requested columns are in a B-Tree index, the database reads data directly from the index. `SELECT *` forces the engine to perform heap fetches for every row.
2. **Bandwidth & Memory**: Large columns (e.g., `JSONB`, `TEXT`, `BYTEA`) stored in TOAST tables must be retrieved and decompressed, wasting I/O and network bandwidth.
3. **Fragility**: Schema changes (adding/dropping columns) can cause position-based ORMs or tuple mapping drivers to fail.
</details>

### Q8: What is the rule of thumb for commenting SQL code?
<details>
<summary>Click to view answer</summary>

Comment the **business reason and intent**, never the obvious SQL mechanics.
- Do not write: `-- Join customer on id`
- Do write: `-- Exclude test accounts created during automated staging deployments (tenant_id = 999)`
</details>

---

## Practice Exercises

### Exercise 1: Refactor Implicit Comma Joins
Refactor this deprecated ANSI-89 query into clean, explicit ANSI-92 SQL:
```sql
SELECT u.name, o.total, p.status
FROM users u, orders o, payments p
WHERE u.id = o.user_id AND o.id = p.order_id AND p.status = 'SETTLED';
```

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    usr.name,
    ord.total,
    pmt.status
FROM users usr
JOIN orders ord ON usr.id = ord.user_id
JOIN payments pmt ON ord.id = pmt.order_id
WHERE pmt.status = 'SETTLED';
```
</details>

---

### Exercise 2: Eliminate the `DISTINCT` Anti-Pattern
Refactor this query to eliminate `DISTINCT` using a semi-join:
```sql
SELECT DISTINCT c.customer_id, c.company_name
FROM customers c
JOIN invoices i ON c.customer_id = i.customer_id
WHERE i.is_overdue = TRUE;
```

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    cust.customer_id,
    cust.company_name
FROM customers cust
WHERE EXISTS (
    SELECT 1 
    FROM invoices inv 
    WHERE inv.customer_id = cust.customer_id 
      AND inv.is_overdue = TRUE
);
```
</details>

---

### Exercise 3: Remove Fragile Positional Ordering
Fix this fragile query to use explicit, robust column naming in `ORDER BY` and `GROUP BY`:
```sql
SELECT department_id, job_title, AVG(salary)
FROM employees
GROUP BY 1, 2
ORDER BY 3 DESC;
```

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    department_id,
    job_title,
    ROUND(AVG(salary), 2) AS avg_salary
FROM employees
GROUP BY department_id, job_title
ORDER BY avg_salary DESC;
```
</details>

---

### Exercise 4: Refactor Multi-Branch Cartesian Duplication
Given `students(student_id, name)`, `exam_grades(grade_id, student_id, score)`, and `attendance_logs(log_id, student_id, is_present)`. Write a query that computes each student's average exam score and total attended days without Cartesian multiplication.

<details>
<summary>Click to view solution</summary>

```sql
WITH 
exam_stats AS (
    SELECT 
        student_id, 
        ROUND(AVG(score), 2) AS avg_exam_score
    FROM exam_grades
    GROUP BY student_id
),
attendance_stats AS (
    SELECT 
        student_id, 
        COUNT(*) AS days_present
    FROM attendance_logs
    WHERE is_present = TRUE
    GROUP BY student_id
)
SELECT 
    stu.student_id,
    stu.name,
    COALESCE(es.avg_exam_score, 0.00) AS avg_exam_score,
    COALESCE(att.days_present, 0) AS total_days_attended
FROM students stu
LEFT JOIN exam_stats es ON stu.student_id = es.student_id
LEFT JOIN attendance_stats att ON stu.student_id = att.student_id;
```
</details>

---

### Exercise 5: Linear CTE Pipeline Decomposition
Decompose the following nested subquery into a 3-step chained CTE pipeline:
```sql
SELECT org_name, active_members
FROM (
    SELECT o.name AS org_name, COUNT(u.id) AS active_members
    FROM (SELECT id, name FROM orgs WHERE tier = 'ENTERPRISE') o
    JOIN (SELECT id, org_id FROM users WHERE last_login > '2026-01-01') u ON o.id = u.org_id
    GROUP BY o.name
) summary
WHERE active_members >= 10;
```

<details>
<summary>Click to view solution</summary>

```sql
WITH 
enterprise_orgs AS (
    SELECT org_id, name
    FROM orgs
    WHERE tier = 'ENTERPRISE'
),
active_users AS (
    SELECT user_id, org_id
    FROM users
    WHERE last_login > '2026-01-01'
),
org_member_counts AS (
    SELECT 
        eo.name AS org_name,
        COUNT(au.user_id) AS active_members
    FROM enterprise_orgs eo
    JOIN active_users au ON eo.org_id = au.org_id
    GROUP BY eo.name
)
SELECT org_name, active_members
FROM org_member_counts
WHERE active_members >= 10
ORDER BY active_members DESC;
```
</details>

---

### Exercise 6: Clean Up Ambiguous Aliasing
Review this query and rewrite it using semantic, self-documenting aliases:
```sql
SELECT a.id, b.id, c.val
FROM users a
JOIN accounts b ON a.acc_id = b.id
JOIN metrics c ON b.id = c.acc_id;
```

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    usr.user_id,
    acc.account_id,
    met.metric_value
FROM users usr
JOIN accounts acc ON usr.account_id = acc.account_id
JOIN metrics met ON acc.account_id = met.account_id;
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                              CLEAN SQL ARCHITECTURE CHEAT SHEET                         |
+-------------------+----------------------------+----------------------------------------+
| Principle         | Anti-Pattern               | Clean Architecture Standard            |
+-------------------+----------------------------+----------------------------------------+
| Query Structure   | Deeply nested "SQL Onion"  | Linear, top-down chained CTE pipeline  |
| Cardinality Fix   | Slapping DISTINCT on JOIN  | WHERE EXISTS (...) Semi-Join           |
| Join Syntax       | FROM a, b (Comma Join)     | Explicit ANSI JOIN ... ON              |
| Column Projection | SELECT *                   | Explicitly named attribute lists       |
| Sorting / Grouping| GROUP BY 1, 2 (Positional) | Named identifiers (GROUP BY dept_id)   |
| Multi-Aggregations| Joining multiple 1:N childs| Pre-aggregate child tables in CTEs     |
| Table Aliasing    | Single letters (a, b, c)   | Semantic nouns (cust, ord, line_item)  |
+-------------------+----------------------------+----------------------------------------+
```

- **Readability**: Code is read 10x more than written; invest in modular CTE pipelines.
- **Git Hygiene**: Put each column and condition on its own line with trailing commas for 1-line git diffs.
- **Performance Invariance**: Modern query planners inline CTEs seamlessly, yielding zero performance cost for clean code.

---

## Further Reading
- SQLFluff Documentation: *The Dialect-Flexible SQL Linter and Formatter*
- Joe Celko: *SQL Programming Style (The Morgan Kaufmann Series in Data Management)*
- Martin Fowler: *Refactoring: Improving the Design of Existing Code*
- Markus Winand: *Modern SQL - Style Guides and Maintainability*
