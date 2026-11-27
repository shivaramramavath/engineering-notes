# 16: SQL Comments, Style & Formatting Conventions

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [15 - Upsert Patterns: ON CONFLICT & MERGE](./15-upsert-and-merge.md) |
| **Target Audience** | Software Engineers, Data Architects, Technical Leads |
| **Verification Status** | Verified against SQLFluff / PostgreSQL engineering style guides |

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

SQL code is written once but read, reviewed, debugged, and refactored hundreds of times throughout its operational lifetime. In large software organizations, unformatted, inconsistent, and undocumented SQL scripts are major sources of technical debt. When a query is written on a single 500-character line with arbitrary casing, single-letter aliases (`a`, `b`, `c`), unquoted ambiguous identifiers, and zero comments, troubleshooting a production incident at midnight becomes an exercise in frustration.

Adopting rigorous SQL styling and formatting conventions provides:
1. **Cognitive Clarity**: Structured indentation and keyword casing immediately reveal the logical shape of the query (joins, CTEs, filters, aggregations).
2. **Version Control Friendliness**: Putting each projected column and join condition on its own line isolates diffs in Git pull requests, eliminating noisy merge conflicts.
3. **Deterministic Portability**: Standardized `snake_case` naming prevents subtle cross-platform bugs caused by operating system filesystem case sensitivity (e.g., Linux vs macOS in MySQL).
4. **Automated Governance**: Enforcing style guides via automated formatters and linters (such as **SQLFluff**) ensures uniform code quality across engineering teams.

---

## 2. Core Concepts

### 1. Comment Syntax in SQL
SQL supports two primary comment formats:
- **Single-Line Comments (`-- comment`)**: Everything from the double dash `--` to the end of the physical line is ignored by the parser.
  *(Note: MySQL requires a space immediately following the double dash: `-- comment`).*
- **Multi-Line Block Comments (`/* comment */`)**: Encloses arbitrary blocks spanning multiple lines.
  - In PostgreSQL and SQLite, **block comments can be nested**:
    ```sql
    /* Outer comment /* Inner nested comment */ still commented */
    ```
  - In MySQL and SQL Server, nesting block comments is illegal and triggers a syntax error.

### 2. Identifier Naming Conventions
A professional relational schema adheres to strict naming rules:

| Entity Type | Convention | Example | Anti-Pattern |
|:---|:---|:---|:---|
| **Tables** | Lowercase `snake_case`, plural | `customers`, `order_items` | `Customer`, `tbl_OrderItems` |
| **Primary Key** | Singular table name + `_id` | `customer_id`, `order_id` | `id`, `ID`, `pk_cust` |
| **Foreign Key** | Exact referenced primary key name | `customer_id` | `cust_fk`, `user` |
| **Junction Tables** | Both entity names concatenated | `student_courses`, `film_actors` | `links`, `map_table` |
| **Unique Indexes** | `uq_<table>_<column>` | `uq_users_email` | `unique_1`, `idx1` |
| **B-Tree Indexes** | `idx_<table>_<column>` | `idx_orders_created_at` | `orders_idx`, `my_index` |
| **Check Constraints** | `chk_<table>_<rule>` | `chk_products_positive_price`| `check_1`, `c_price` |

---

## 3. How It Works Internally

### The Lexer & Parser Treatment of Whitespace and Comments

```
SQL String Input:
"SELECT\n    id, -- Get ID\n    name\nFROM users;"
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│ Lexical Scanner (Flex / Lexer)                              │
│ 1. Strips out single-line comments (-- ...)                 │
│ 2. Strips out block comments (/* ... */)                    │
│ 3. Normalizes whitespace (tabs, newlines, multiple spaces)  │
│    into single token delimiters.                            │
│ 4. Folds unquoted identifiers to lowercase (in PostgreSQL)  │
│    or uppercase (in Oracle).                                │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ Abstract Syntax Tree (AST)                                  │
│ Identical AST produced regardless of formatting, spaces,    │
│ or comment placement!                                       │
└─────────────────────────────────────────────────────────────┘
```

- Because comments and whitespace are completely discarded by the lexical scanner during Phase 1 of query processing, **formatting, indentation, and comments have exactly 0.00% impact on query execution performance**.

---

## 4. Syntax & Usage

### The Canonical Production Formatting Template

```sql
-- ============================================================================
-- Description: Monthly customer revenue cohort summary for VIP accounts
-- Author: Engineering Team
-- Target Dialect: PostgreSQL 17+
-- ============================================================================

WITH MonthlyCustomerSpend AS (
    SELECT 
        o.customer_id,
        DATE_TRUNC('month', o.order_date)::date AS order_month,
        COUNT(o.order_id) AS total_orders,
        SUM(o.total_amount) AS gross_spend
    FROM orders o
    WHERE o.order_status = 'completed'
      AND o.order_date >= '2024-01-01 00:00:00+00'
      AND o.order_date <  '2025-01-01 00:00:00+00'
    GROUP BY 
        o.customer_id,
        DATE_TRUNC('month', o.order_date)
)

SELECT 
    c.customer_id,
    c.first_name || ' ' || c.last_name AS customer_name,
    c.email,
    mcs.order_month,
    mcs.total_orders,
    mcs.gross_spend,
    CASE 
        WHEN mcs.gross_spend >= 1000.00 THEN 'VIP Tier'
        WHEN mcs.gross_spend >= 500.00  THEN 'Preferred Tier'
        ELSE 'Standard Tier'
    END AS customer_tier
FROM MonthlyCustomerSpend mcs
JOIN customers c 
  ON mcs.customer_id = c.customer_id
WHERE mcs.gross_spend >= 500.00
ORDER BY 
    mcs.order_month ASC,
    mcs.gross_spend DESC,
    c.customer_id ASC
LIMIT 50;
```

### Multi-Dialect Comparison Matrix

| Formatting & Casing Behavior | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Unquoted Identifier Case** | Folds to lowercase | Lowercase / Case-Sensitive (Linux filesystem)| Case-Insensitive | Case-Insensitive (default) |
| **Quoted Identifier Delimiter** | Double quotes (`"my_col"`) | Backticks (`` `my_col` ``) or double quotes | Double quotes, backticks, or square brackets | Square brackets (`[my_col]`) or double quotes |
| **Nested Block Comments (`/* /* */ */`)**| Supported | Not supported (Syntax error)| Supported | Not supported |
| **Single-Line Comment Requirement** | `--` followed by anything | `-- ` (Space required after `--`!)| `--` followed by anything | `--` followed by anything |

---

## 5. 5 Practical Progressive Examples

### Example 1: Trailing Commas vs Leading Commas
In SQL, columns in the `SELECT` list are separated by commas. Two layout philosophies exist:

**Pattern A: Trailing Commas (Standard Industry Best Practice)**:
```sql
SELECT 
    customer_id,
    first_name,
    last_name,
    email
FROM customers;
```
*Clean, readable, natural. Automated formatters (SQLFluff) default to trailing commas.*

**Pattern B: Leading Commas (Legacy DBA Style)**:
```sql
SELECT 
    customer_id
    , first_name
    , last_name
    , email
FROM customers;
```
*Advantage in legacy text editors: commenting out the last line does not cause a trailing comma syntax error. However, modern IDEs and linters make this unnecessary, and trailing commas are preferred in modern codebases.*

### Example 2: Meaningful Table Aliases vs Cryptic Letters
Why single-letter aliases degrade maintainability:

```sql
-- ANTI-PATTERN: Cryptic single-letter aliases
SELECT a.id, b.name, c.total, d.sku
FROM cust a
JOIN ord c ON a.id = c.cust_id
JOIN ord_item d ON c.id = d.ord_id
JOIN prod b ON d.prod_id = b.id;

-- PRODUCTION STANDARD: Descriptive, self-documenting abbreviations
SELECT 
    cust.customer_id,
    prod.product_name,
    ord.total_amount,
    item.sku
FROM customers cust
JOIN orders ord 
  ON cust.customer_id = ord.customer_id
JOIN order_items item 
  ON ord.order_id = item.order_id
JOIN products prod 
  ON item.product_id = prod.product_id;
```

### Example 3: Edge Case (Case Folding Discrepancy Across Dialects)
What happens when you create tables with mixed case identifiers:

In PostgreSQL:
```sql
CREATE TABLE MyTable (MyColumn INT);
-- Stored in pg_catalog as 'mytable' and 'mycolumn'!
SELECT mycolumn FROM mytable; -- Works!
SELECT MyColumn FROM MyTable; -- Works!
```
In MySQL on Linux:
```sql
CREATE TABLE MyTable (MyColumn INT);
-- Stored as physical file 'MyTable.ibd' on Linux ext4!
SELECT * FROM mytable; -- FAILS: Table 'mytable' doesn't exist!
SELECT * FROM MyTable; -- Works!
```
*(Standardizing strictly on lowercase `snake_case` eliminates this entire class of bugs).*

### Example 4: Failure Mode (Accidental Comment Hiding Code)
How incorrect single-line comment usage in string interpolation causes syntax errors:

```sql
-- DANGEROUS: Putting a single line comment inside a single-line string
SELECT id, name -- Get user details FROM users WHERE active = true;
```
The text `FROM users WHERE active = true;` is treated as part of the comment! The query parser sees only `SELECT id, name` and crashes with a syntax error.

### Example 5: Automated Linting with SQLFluff Configuration (`.sqlfluff`)
A production configuration file for automated linting in CI/CD pipelines:

```ini
[sqlfluff]
dialect = postgres
templater = raw
max_line_length = 100
indent_unit = space
indent_size = 4

[sqlfluff:rules:capitalisation.keywords]
capitalisation_policy = upper

[sqlfluff:rules:capitalisation.identifiers]
capitalisation_policy = lower

[sqlfluff:rules:aliasing.table]
aliasing = explicit

[sqlfluff:rules:layout.commas]
line_position = trailing
```

---

## 6. Best Practices

1. **Always Use UPPERCASE for SQL Keywords**: Write `SELECT`, `FROM`, `WHERE`, `JOIN`, `GROUP BY`, `ORDER BY`, `AND`, `OR`, `CASE`, `WHEN`, `END` in full uppercase. This provides instant visual separation between SQL syntax and schema identifiers.
2. **Always Use Lowercase `snake_case` for Identifiers**: Name tables, columns, views, and schemas using lowercase letters separated by underscores (`order_items`, `first_name`). Avoid CamelCase, kebab-case, or spaces.
3. **Never Use Reserved Words as Identifiers**: Avoid naming columns `user`, `order`, `group`, `date`, `type`, or `key`. While delimiters allow it (`"order"`), it causes constant friction across query tools, ORMs, and migration scripts.
4. **Always Qualify Column Names in Multi-Table Joins**: When joining two or more tables, always prefix every column with its table alias (`c.customer_id`, `o.order_date`). This eliminates column ambiguity errors if an identical column name is added to another table in the future.
5. **Put Each Projected Column and Join Condition on Its Own Line**: Place every attribute in `SELECT` on its own line indented by 4 spaces. Place each `JOIN ... ON` condition on its own line. This ensures clean Git pull request diffs.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Mixed-Case Quoted Identifiers** | Creating `"MyColumn"` in PostgreSQL forces you to wrap it in double quotes in *every single query forever*. | Use unquoted lowercase `my_column`. |
| **Cryptic Single-Letter Aliases** | Queries with 6 tables aliased as `a, b, c, d, e, f` are unreadable in production code reviews. | Use descriptive aliases: `cust, ord, item, prod`. |
| **Missing Space in MySQL Comments** | In MySQL, `--comment` without a trailing space is a syntax error! | Always include a space: `-- comment`. |
| **Relying on Nested Block Comments Portably** | `/* /* */ */` is valid in Postgres/SQLite, but throws an immediate syntax error in MySQL and SQL Server. | Avoid nesting block comments in cross-dialect code. |
| **Indenting with Inconsistent Tabs and Spaces** | Mixed tabs and spaces create ragged, misaligned queries across different developer editors. | Standardize on 4 spaces (or 2 spaces) across the team. |
| **Using Ambiguous Column Names in Joins** | `SELECT id, name FROM users JOIN departments ...` breaks if both tables have `id` or `name`. | Always prefix: `SELECT u.user_id, d.dept_name`. |

---

## 8. Performance & Security Considerations

### Performance
- **Zero Runtime Overhead**: Comments and whitespace formatting are stripped by the lexical analyzer before parsing. Cleanly formatted SQL with extensive comments executes at the exact same microsecond speed as a minified single-line query.
- **Prepared Statement Cache Key**: In database engines that cache query plans (like PostgreSQL prepared statements or SQL Server plan cache), the hash of the query text is used as the cache key. Standardizing query formatting across application instances maximizes query plan cache hit ratios.

### Security
- **Comment Injection Attacks in WAFs**:
  - Attackers sometimes insert inline SQL comments to bypass Web Application Firewalls (WAFs):
    `UNION/**/SELECT/**/password/**/FROM/**/users`
  - WAFs inspect tokens, but relational engines decode `/**/` as whitespace. Parameterized queries eliminate this vulnerability.

---

## 9. Comparison Matrix

### SQL Formatting Styles Compared

| Style Dimension | Industry Standard (Recommended) | Legacy DBA Style | Minified Style (Anti-Pattern) |
|:---|:---|:---|:---|
| **Keyword Casing** | UPPERCASE (`SELECT`, `FROM`) | UPPERCASE (`SELECT`) | Lowercase (`select`, `from`) |
| **Identifier Casing** | lowercase `snake_case` | Mixed or Uppercase | Mixed or CamelCase |
| **Comma Placement** | Trailing (`col1,`, `col2,`) | Leading (`, col1`, `, col2`)| Compact / single-line |
| **Indentation** | 4 spaces per clause depth | River-aligned clauses | 0 spaces (single line) |
| **Readability Score** | 10/10 | 7/10 | 1/10 |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: Why should database tables always be named using lowercase `snake_case` and plural nouns?
<details>
<summary>Answer</summary>
1. **Lowercase `snake_case`**: Eliminates cross-platform case-sensitivity bugs. On Linux, MySQL table names match physical filesystem files (case-sensitive), while on Windows and macOS they are case-insensitive. Standardizing on lowercase prevents code deployed to Linux servers from failing.
2. **Plural Nouns (`customers`, `orders`)**: A relational table represents a mathematical relation or set holding **multiple entity instances** (tuples). Naming a table `customers` accurately describes its contents, reserving the singular `customer` for application domain models representing a single entity object.
</details>

#### Q2: How does PostgreSQL handle identifier case folding compared to Oracle and MySQL?
<details>
<summary>Answer</summary>
- **PostgreSQL**: Folds all unquoted identifiers to **lowercase**. `SELECT MyColumn` searches for `mycolumn`. If defined with double quotes (`"MyColumn"`), it preserves mixed case and can only be queried with exact quotes.
- **Oracle**: Folds all unquoted identifiers to **uppercase**. `SELECT my_column` searches for `MY_COLUMN`.
- **MySQL**: Case folding depends on the operating system filesystem and the `lower_case_table_names` system variable. Table names are case-sensitive on Linux by default, but column names are always case-insensitive.
</details>

#### Q3: Why is putting each column on its own line with trailing commas preferred in modern Git-based engineering workflows?
<details>
<summary>Answer</summary>
Placing each column on its own line isolates version control diffs in Git. 

If all columns are on a single line, adding or modifying one column marks the entire line as changed, making pull request reviews noisy and triggering merge conflicts if two developers edit different columns. With one column per line, Git highlights the exact single-line addition, deletion, or modification cleanly.
</details>

#### Q4: What is the risk of using database reserved keywords as column names, even if quoting mechanisms exist?
<details>
<summary>Answer</summary>
Naming columns with reserved words (`user`, `order`, `group`, `table`, `check`):
1. Forces every query, view, and migration to wrap the identifier in quotes (`"order"`, `[order]`, `` `order` ``).
2. Quotes differ across database engines (double quotes in Postgres vs backticks in MySQL vs brackets in SQL Server), breaking query portability.
3. Forgetting quotes triggers obscure parser syntax errors.
4. Many ORMs and query builders generate unquoted SQL by default, causing runtime failures.
</details>

### Practical Formatting & Refactoring Puzzles

#### Q5: Refactor this poorly written query into the production standard format:
```sql
select a.id,b.name,count(c.id) from customers a join orders c on a.id=c.customer_id join order_items d on c.id=d.order_id join products b on d.product_id=b.id where c.order_status='completed' group by a.id,b.name having count(c.id)>1 order by count(c.id) desc limit 5;
```
<details>
<summary>Answer</summary>

```sql
SELECT 
    cust.customer_id,
    prod.product_name,
    COUNT(ord.order_id) AS total_orders
FROM customers cust
JOIN orders ord 
  ON cust.customer_id = ord.customer_id
JOIN order_items item 
  ON ord.order_id = item.order_id
JOIN products prod 
  ON item.product_id = prod.product_id
WHERE ord.order_status = 'completed'
GROUP BY 
    cust.customer_id,
    prod.product_name
HAVING COUNT(ord.order_id) > 1
ORDER BY total_orders DESC
LIMIT 5;
```
</details>

#### Q6: Explain why this MySQL single-line comment throws a syntax error:
```sql
SELECT * FROM users;--Check active users
```
<details>
<summary>Answer</summary>
In MySQL, the single-line comment sequence `--` **strictly requires at least one whitespace character** (space, tab, or newline) immediately following the second dash:
`-- Check active users`.

In the snippet above, `--Check` has no space after `--`. MySQL treats `--Check` as an expression or invalid operator sequence, throwing a syntax error.
</details>

#### Q7: Write a standardized SQL header comment block for a production migration script.
<details>
<summary>Answer</summary>

```sql
-- ============================================================================
-- Migration: 20240401_add_user_preferences.sql
-- Author: Database Infrastructure Team
-- Ticket: JIRA-4821
-- Description: Adds user preferences JSONB column with schema validation
-- Rollback: ALTER TABLE users DROP COLUMN IF EXISTS preferences;
-- ============================================================================
```
</details>

#### Q8: What automated tools can be integrated into CI/CD pipelines to enforce SQL formatting and linting?
<details>
<summary>Answer</summary>
1. **SQLFluff**: The industry-standard open-source linter and auto-formatter for SQL. Supports all major dialects (Postgres, MySQL, Snowflake, BigQuery, T-SQL).
2. **pgFormatter**: Specialized high-performance CLI formatter for PostgreSQL.
3. **Prettier with SQL Plugin**: Auto-formatting within IDEs (VS Code, JetBrains).
4. **Liquibase / Flyway Lint Checks**: Quality checks validating that migration files comply with naming conventions and safety rules.
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Format a Simple Query
Format this query following standard production conventions (UPPERCASE keywords, lowercase identifiers, 4-space indent, one column per line):
`select user_id,email,created_at from users where is_active=true order by created_at desc;`
<details>
<summary>Solution</summary>

```sql
SELECT 
    user_id,
    email,
    created_at
FROM users
WHERE is_active = TRUE
ORDER BY created_at DESC;
```
</details>

#### Exercise 2: Standardizing Constraint Names
Write the `ALTER TABLE` statements to add a primary key on `account_id`, a foreign key on `user_id`, and a unique constraint on `account_number` for table `bank_accounts`, using standardized naming conventions (`pk_`, `fk_`, `uq_`).
<details>
<summary>Solution</summary>

```sql
ALTER TABLE bank_accounts
    ADD CONSTRAINT pk_bank_accounts PRIMARY KEY (account_id),
    ADD CONSTRAINT fk_bank_accounts_users FOREIGN KEY (user_id) REFERENCES users(user_id),
    ADD CONSTRAINT uq_bank_accounts_number UNIQUE (account_number);
```
</details>

### Intermediate Exercises

#### Exercise 3: Refactoring Join Aliases
Refactor this query replacing single-letter aliases with descriptive prefixes, and qualifying all projected columns:
```sql
SELECT a.id, b.order_id, c.sku, d.name
FROM customers a
JOIN orders b ON a.id = b.cust_id
JOIN order_items c ON b.order_id = c.order_id
JOIN products d ON c.prod_id = d.id;
```
<details>
<summary>Solution</summary>

```sql
SELECT 
    cust.customer_id,
    ord.order_id,
    item.sku,
    prod.product_name
FROM customers cust
JOIN orders ord 
  ON cust.customer_id = ord.customer_id
JOIN order_items item 
  ON ord.order_id = item.order_id
JOIN products prod 
  ON item.product_id = prod.product_id;
```
</details>

#### Exercise 4: Clean CTE Layout
Write a query using two Common Table Expressions (`ActiveUsers` and `RecentOrders`), formatting them with clear indentation and structure.
<details>
<summary>Solution</summary>

```sql
WITH ActiveUsers AS (
    SELECT user_id, username
    FROM users
    WHERE is_active = TRUE
),

RecentOrders AS (
    SELECT order_id, customer_id, total_amount
    FROM orders
    WHERE order_date >= CURRENT_DATE - INTERVAL '30 days'
)

SELECT 
    u.user_id,
    u.username,
    COUNT(ro.order_id) AS orders_last_30_days,
    COALESCE(SUM(ro.total_amount), 0.00) AS spend_last_30_days
FROM ActiveUsers u
LEFT JOIN RecentOrders ro 
  ON u.user_id = ro.customer_id
GROUP BY 
    u.user_id,
    u.username
ORDER BY spend_last_30_days DESC;
```
</details>

### Advanced Exercises

#### Exercise 5: SQLFluff Configuration File
Write a complete `.sqlfluff` configuration file enforcing PostgreSQL dialect, 4-space indentation, trailing commas, uppercase keywords, and lowercase identifiers.
<details>
<summary>Solution</summary>

```ini
[sqlfluff]
dialect = postgres
max_line_length = 100
indent_unit = space
indent_size = 4

[sqlfluff:rules:capitalisation.keywords]
capitalisation_policy = upper

[sqlfluff:rules:capitalisation.identifiers]
capitalisation_policy = lower

[sqlfluff:rules:layout.commas]
line_position = trailing
```
</details>

#### Exercise 6: Automated Formatting Verification Script
Write a Python script that reads an SQL file, verifies that all SQL keywords (`SELECT`, `FROM`, `WHERE`, `JOIN`) are strictly uppercase, and flags any lines exceeding 100 characters.
<details>
<summary>Solution</summary>

```python
import sys, re

KEYWORDS = ["select", "from", "where", "join", "group by", "order by", "having"]

def lint_sql(file_path):
    with open(file_path, "r", encoding="utf-8") as f:
        for idx, line in enumerate(f, 1):
            if len(line.rstrip()) > 100:
                print(f"Line {idx}: Exceeds 100 characters ({len(line.rstrip())})")
            for kw in KEYWORDS:
                # Match standalone lowercase keyword
                if re.search(r'\b' + kw + r'\b', line) and not line.strip().startswith("--"):
                    print(f"Line {idx}: Keyword '{kw}' must be UPPERCASE.")

if __name__ == "__main__":
    lint_sql("query.sql")
```
</details>

---

## 12. Summary & Quick Reference

### SQL Style Guide Checklist
- **Keywords**: Always `UPPERCASE` (`SELECT`, `INSERT`, `UPDATE`, `DELETE`, `JOIN`, `WHERE`).
- **Identifiers**: Always lowercase `snake_case` (`customer_id`, `order_items`).
- **Tables**: Plural nouns (`users`, `products`, `invoices`).
- **Columns**: Singular attributes (`first_name`, `created_at`).
- **Primary Keys**: Singular table name + `_id` (`user_id`).
- **Foreign Keys**: Exact referenced PK name (`user_id`).
- **Indentation**: 4 spaces per nesting level.
- **Commas**: Trailing commas, one column per line.
- **Aliases**: Explicit and descriptive (`cust`, `ord`, `prod`). Never single letters.
- **Safety**: Never use `SELECT *` in production; qualify all columns in joins.

---

## 13. Further Reading
- SQLFluff: [The SQL Linter for Humans](https://docs.sqlfluff.com/)
- GitLab Data Team: [SQL Style Guide](https://handbook.gitlab.com/handbook/business-technology/data-team/platform/sql-style-guide/)
- Simon Holywell: [SQL Style Guide](https://www.sqlstyle.guide/)
- [Module 02: Advanced Querying & Analytical SQL](../02-querying/README.md)
