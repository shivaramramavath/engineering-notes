# 06: SQL Standard & How to Read the Docs

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [05 - Sample Databases Used in This Track](./05-sample-databases-used-in-this-track.md) |
| **Target Audience** | Software Engineers, Systems Architects, Technical Writers |
| **Verification Status** | Verified against ISO/IEC 9075:2023 specifications |

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

SQL is not proprietary to any single corporation; it is an international standard formally maintained by the **International Organization for Standardization (ISO)** and the **International Electrotechnical Commission (IEC)** under standard designation **ISO/IEC 9075**. First standardized by ANSI in 1986 (SQL-86), the language has evolved across nine major revisions over nearly four decades, culminating in **ISO/IEC 9075:2023 (SQL:2023)**.

Yet, no commercial or open-source relational database implements 100% of the ISO standard, nor does any database implement standard features in an identical syntactic manner. Vendors selectively implement Core features, adopt optional packages, introduce proprietary extensions (e.g., PostgreSQL `ILIKE`, MySQL `LIMIT offset, count`, T-SQL `CROSS APPLY`), and occasionally implement syntax that directly contradicts the standard.

To navigate this landscape, senior engineers must develop two critical skills:
1. **Understanding Standard Feature Conformance**: Knowing what is universal ANSI SQL versus vendor dialect extensions.
2. **Reading Technical Documentation and Syntax Grammars**: Decoding Backus-Naur Form (BNF) notation, railroad diagrams, and engine compatibility appendices to extract precise syntactic rules without trial-and-error guessing.

---

## 2. Core Concepts

### Chronological Evolution of the SQL Standard

```
1986: SQL-86 (First ANSI standard; basic SELECT, INSERT, UPDATE, DELETE)
  │
1989: SQL-89 (Integrity enhancements: PRIMARY KEY, FOREIGN KEY constraints)
  │
1992: SQL-92 (Landmark release: explicit JOIN ON/USING, CASE, subqueries, schemas)
  │
1999: SQL:1999 (Regex, Recursive CTEs, Triggers, User-Defined Types)
  │
2003: SQL:2003 (Window Functions OVER/PARTITION, XML integration, MERGE)
  │
2006: SQL:2006 (XML Query specifications)
  │
2008: SQL:2008 (FETCH FIRST, TRUNCATE, improved MERGE)
  │
2011: SQL:2011 (Temporal tables, time-slice queries SYSTEM_TIME)
  │
2016: SQL:2016 (JSON functions, row pattern recognition MATCH_RECOGNIZE)
  │
2023: SQL:2023 (Property Graph Queries / SQL/PGQ, Any Value, JSON extensions)
```

### Core SQL vs Optional Packages & Feature IDs
ISO/IEC 9075 is split into multiple parts (Part 1: Framework, Part 2: Foundation, Part 3: CLI, Part 4: PSM/Routines, Part 16: Property Graphs).

Features are formally designated by **Feature Codes**:
- `E011`: Numeric data types (Core SQL).
- `F031`: Basic subquery support.
- `F611`: Window functions.
- `T611`: Elementary OLAP facilities.
- `T111`: Updatable joins.

A database claiming "Core SQL Conformance" must implement all designated Core features, while advanced analytical features are classified as optional packages.

### How to Read Backus-Naur Form (BNF) Syntax
Engine documentation uses standardized typographic notation to specify valid SQL syntax:

| Notation | Meaning | Example | Explanation |
|:---|:---|:---|:---|
| `UPPERCASE` | SQL keywords (literal) | `SELECT`, `FROM` | Must be typed exactly as shown. |
| `lowercase` | Syntactic variables / tokens | `table_name`, `expression` | Replaced by user-supplied identifiers. |
| `[ ... ]` | Optional element | `[ WHERE condition ]` | May be included or omitted. |
| `{ ... }` | Required grouped element | `{ ASC \| DESC }` | Must select one of the grouped choices. |
| `\|` | Alternation (OR) | `LEFT \| RIGHT \| FULL` | Choose exactly one option. |
| `...` | Repeatable element | `column_name [, ...]` | May repeat preceding item separated by commas. |

---

## 3. How It Works Internally

### Standard Conformance Architecture in PostgreSQL
PostgreSQL tracks its standards compliance explicitly in its documentation and system catalogs. In Appendix D of the official documentation, the PostgreSQL Global Development Group maintains an exhaustive catalog of every feature in ISO/IEC 9075:2023:

```
+--------------------------------------------------------------------------+
|                  PostgreSQL Standards Conformance Engine                 |
|                                                                          |
|  Standard Core Features (Supported: > 170 / 179 Core features)           |
|  [E011-01] INTEGER Type          ──► Supported natively                  |
|  [E021-01] CHARACTER Type        ──► Supported natively                  |
|  [F611-01] Window Functions      ──► Supported (Full ANSI Frames)        |
|  [T611]    GROUPING SETS         ──► Supported natively                  |
|                                                                          |
|  Vendor Extensions (Non-Standard but Documented)                         |
|  - ILIKE (Case-insensitive pattern matching)                             |
|  - DISTINCT ON (col1)                                                    |
|  - RETURNING Clause (Standard uses OUTPUT or data change delta tables)   |
+--------------------------------------------------------------------------+
```

You can inspect supported standard features directly by querying the `information_schema.sql_features` catalog!

---

## 4. Syntax & Usage

### Querying the Standard SQL Features Catalog
The ANSI standard defines an `information_schema` view named `sql_features` that lists every standard feature code and whether the running database supports it.

```sql
-- Querying standards conformance directly from PostgreSQL 17
SELECT 
    feature_id,
    feature_name,
    is_supported,
    comments
FROM information_schema.sql_features
WHERE feature_id IN ('E011-01', 'F611', 'T611', 'F591')
ORDER BY feature_id;
```
```text
+------------+---------------------------+--------------+----------+
| feature_id | feature_name              | is_supported | comments |
+------------+---------------------------+--------------+----------+
| E011-01    | INTEGER and SMALLINT      | YES          |          |
| F591       | Derived tables            | YES          |          |
| F611       | Elementary OLAP/Window    | YES          |          |
| T611       | Extended grouping sets    | YES          |          |
+------------+---------------------------+--------------+----------+
```

### Multi-Dialect Comparison Matrix

| Standard Feature & Code | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **SQL-92 Explicit Joins (`JOIN ON`)** | Full | Full | Full | Full |
| **SQL:1999 Recursive CTEs (`T121`)** | Full (`WITH RECURSIVE`) | Full (`WITH RECURSIVE`) | Full (`WITH RECURSIVE`) | Supported (`WITH`, no `RECURSIVE` kw) |
| **SQL:2003 Window Functions (`F611`)**| Full with frames & filter | Full with frames | Supported (since 3.25) | Supported (limited framing) |
| **SQL:2008 `FETCH FIRST` (`F851`)** | Full (`FETCH FIRST n ROWS`) | Supported | Not supported (`LIMIT` only)| Full (`FETCH NEXT n ROWS ONLY`) |
| **SQL:2016 JSON Path (`T831`)** | Full (`jsonb_path_query`) | Supported | Supported (`json_extract`) | Supported (`JSON_QUERY`) |
| **SQL:2023 Property Graphs (SQL/PGQ)**| Planned / Extensions | Not supported | Not supported | Graph extensions |

---

## 5. 5 Practical Progressive Examples

### Example 1: Reading and Translating BNF Documentation
Consider the official PostgreSQL documentation for `SELECT`:

```text
SELECT [ ALL | DISTINCT [ ON ( expression [, ...] ) ] ]
    * | expression [ [ AS ] output_name ] [, ...]
    [ FROM from_item [, ...] ]
    [ WHERE condition ]
    [ GROUP BY [ ALL | DISTINCT ] grouping_element [, ...] ]
    [ HAVING condition ]
    [ WINDOW window_name AS ( window_definition ) [, ...] ]
    [ { UNION | INTERSECT | EXCEPT } [ ALL | DISTINCT ] select ]
    [ ORDER BY expression [ ASC | DESC | USING operator ] [ NULLS { FIRST | LAST } ] [, ...] ]
    [ LIMIT { count | ALL } ]
    [ OFFSET start [ ROW | ROWS ] ]
    [ FETCH { FIRST | NEXT } [ count ] { ROW | ROWS } { ONLY | WITH TIES } ]
```
Decoding this BNF grammar reveals:
1. `ALL | DISTINCT` are mutually exclusive alternatives (`|`).
2. `[ ON ( ... ) ]` is an optional clause that is *only* valid if `DISTINCT` is selected.
3. `expression [ [ AS ] output_name ]` shows `AS` is optional: `SELECT col AS label` and `SELECT col label` are both syntactically valid.
4. `FETCH ... { ONLY | WITH TIES }` requires choosing either `ONLY` or `WITH TIES`.

### Example 2: Standard ANSI `FETCH FIRST` vs Proprietary `LIMIT`
Demonstrating standard SQL:2008 syntax vs legacy vendor `LIMIT`:

```sql
-- Standard SQL:2008 Syntax (PostgreSQL, SQL Server, Oracle)
SELECT product_id, product_name, retail_price
FROM products
ORDER BY retail_price DESC
OFFSET 0 ROWS
FETCH FIRST 3 ROWS ONLY;
```
```text
+------------+---------------------+--------------+
| product_id | product_name        | retail_price |
+------------+---------------------+--------------+
|        201 | Framework Laptop 16 |      1399.00 |
|        202 | MacBook Air M3       |      1099.00 |
|        203 | Pixel 9 Pro         |       999.00 |
+------------+---------------------+--------------+
```

```sql
-- Legacy Vendor Syntax (MySQL, SQLite, PostgreSQL extension)
SELECT product_id, product_name, retail_price
FROM products
ORDER BY retail_price DESC
LIMIT 3 OFFSET 0;
```

### Example 3: Edge Case (`WITH TIES` in Standard SQL)
The standard SQL:2008 `WITH TIES` clause solves the problem of arbitrary cutoffs when rows share identical sorting values:

```sql
-- Returns the top 2 highest priced products, PLUS any ties with the 2nd product!
SELECT product_id, product_name, retail_price
FROM products
ORDER BY retail_price DESC
FETCH FIRST 2 ROWS WITH TIES;
```
*(If products 2 and 3 both cost $1,099.00, both are included, returning 3 rows total).*

### Example 4: Failure Mode (Vendor Keyword Collision)
Attempting to use a standard SQL keyword as a column name without quoting causes a parser failure:

```sql
-- FAILS: 'user' is a reserved standard SQL function returning current user
CREATE TABLE system_events (
    event_id INT,
    user VARCHAR(50) -- SYNTAX ERROR
);
```
```text
ERROR: syntax error at or near "user"
LINE 3:     user VARCHAR(50)
```
Fix: Use descriptive non-reserved identifiers (`username`, `actor_name`), or quote standard identifiers:
```sql
CREATE TABLE system_events (
    event_id INT,
    "user" VARCHAR(50)
);
```

### Example 5: Optimized Standard Standards-Compliant `CASE` Expression
Using ANSI-92 searched `CASE` expression vs engine-specific functions (`IFNULL`, `NVL`, `IF`).

```sql
-- Anti-Pattern: Non-portable MySQL IF() or Oracle NVL()
-- SELECT IF(stock_qty > 0, 'In Stock', 'Out of Stock') FROM products;

-- Production Best Practice: 100% ANSI Standard Searched CASE
SELECT 
    product_name,
    stock_qty,
    CASE 
        WHEN stock_qty > 20 THEN 'Well Stocked'
        WHEN stock_qty > 0  THEN 'Low Inventory'
        ELSE 'Out of Stock'
    END AS inventory_status
FROM products
ORDER BY stock_qty ASC;
```
```text
+---------------------------------------+-----------+------------------+
| product_name                          | stock_qty | inventory_status |
+---------------------------------------+-----------+------------------+
| Framework Laptop 16                   |        25 | Well Stocked     |
| MacBook Air M3                        |        40 | Well Stocked     |
| Pixel 9 Pro                           |        50 | Well Stocked     |
| Sony WH-1000XM5                       |        75 | Well Stocked     |
| Designing Data-Intensive Applications |       120 | Well Stocked     |
+---------------------------------------+-----------+------------------+
```

---

## 6. Best Practices

1. **Write to the Standard When Portability Matters**: Use `COALESCE` instead of `IFNULL` (MySQL) or `NVL` (Oracle). Use `||` for concatenation instead of `+` (SQL Server) or `CONCAT()`.
2. **Favor `OFFSET ... FETCH FIRST` Over `LIMIT` for New Applications**: `FETCH FIRST` is ISO/IEC 9075:2008 standard and supported across modern PostgreSQL, SQL Server, Oracle, and DB2.
3. **Consult the Engine’s SQL Conformance Appendix**: When in doubt about whether a query is standard, check PostgreSQL’s [Appendix D (SQL Conformance)](https://www.postgresql.org/docs/17/features.html).
4. **Never Use Reserved Words for Schema Identifiers**: Avoid naming columns `user`, `order`, `group`, `table`, `check`, or `primary`. While delimiters (`"order"`) allow it, it introduces friction in queries, ORMs, and migration tooling.
5. **Understand the Limits of Standards**: While standard SQL guarantees syntactic portability, it does NOT guarantee identical performance or isolation semantics across different storage engines.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Assuming `LIMIT` is ANSI Standard** | `LIMIT` is a vendor extension (originating in MySQL and Postgres); it is not part of the ISO SQL standard and fails in SQL Server or Oracle. | Use standard `OFFSET m ROWS FETCH NEXT n ROWS ONLY`. |
| **Using Comma Joins (`FROM a, b`)** | SQL-86 comma joins lack explicit join conditions and separate filters from predicates, leading to accidental Cartesian products. | Use SQL-92 explicit `JOIN a ON ...` syntax. |
| **Assuming Standard `INTERVAL` Math Works Universally** | `created_at + INTERVAL '7 days'` works in Postgres, but fails in MySQL (requires `INTERVAL 7 DAY`) and SQL Server (requires `DATEADD`). | Check engine datetime syntax when writing cross-dialect queries. |
| **Misreading Optional Brackets in BNF** | Assuming `[ AS label ]` means the whole clause is required, or failing to recognize nested alternatives. | Break down BNF brackets systematically: `[` = optional, `{` = required group. |
| **Relying on Non-Standard `REPLACE INTO` (MySQL)** | `REPLACE INTO` deletes the conflicting row and inserts a new one, breaking foreign key cascades and triggers. | Use standard ANSI SQL:2008 `MERGE` or standard `ON CONFLICT DO UPDATE`. |
| **Ignoring SQL Revision Year** | Using features introduced in SQL:2016 on older database versions (e.g., PostgreSQL 9.4 or MySQL 5.7) causes syntax errors. | Verify the target engine release against the required SQL standard version. |

---

## 8. Performance & Security Considerations

### Performance
- **Standard Syntax Optimization**: Modern cost-based optimizers parse ANSI SQL:92 joins (`JOIN ... ON`) and ANSI SQL:1999 CTEs (`WITH`) into canonical relational algebra operator trees with equal or superior optimization compared to legacy comma joins or subqueries.
- **Portability Layer Overhead**: Relying on database abstraction layers (e.g., Hibernate, Knex) to translate non-standard SQL can sometimes generate suboptimal physical queries. Knowing the native dialect allows writing fine-tuned native queries when needed.

### Security
- **Information Schema Exposure**: System tables like `information_schema.tables` and `information_schema.columns` expose the entire schema topology. Ensure non-administrative application users do not have global metadata inspection rights in multi-tenant environments.

---

## 9. Comparison Matrix

### Major SQL Standard Revisions & Milestones

| Standard Milestone | Year | Landmark Capabilities Introduced | Industry Impact |
|:---|:---:|:---|:---|
| **SQL-92** | 1992 | `INNER/LEFT/RIGHT/FULL JOIN`, `CASE`, `CAST`, subqueries in `FROM` | Foundation of all modern relational querying |
| **SQL:1999** | 1999 | `WITH RECURSIVE` CTEs, Triggers, Boolean type, Roles | Made complex graph & hierarchical querying possible |
| **SQL:2003** | 2003 | Window Functions (`OVER`, `PARTITION BY`), Sequence generators, `MERGE` | Revolutionized analytical reporting and ETL pipelines |
| **SQL:2008** | 2008 | `FETCH FIRST / OFFSET`, `TRUNCATE TABLE`, `INSTEAD OF` triggers | Standardized pagination and bulk table clearing |
| **SQL:2011** | 2011 | Temporal Tables (`PERIOD FOR`, `SYSTEM_TIME`), Enhanced window framing | Native bitemporal audit and financial time-travel queries |
| **SQL:2016** | 2016 | Native JSON operators, `MATCH_RECOGNIZE` complex event processing | Allowed storing and querying semi-structured documents |
| **SQL:2023** | 2023 | SQL/PGQ (Property Graph Queries), multi-character string functions | Blends graph database traversal directly into SQL |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: What is the architectural difference between SQL-89 and SQL-92 join syntax, and why was the change made?
<details>
<summary>Answer</summary>
In SQL-89, joins were specified implicitly in the `FROM` clause separated by commas, with join conditions placed in the `WHERE` clause:
```sql
SELECT * FROM orders o, customers c WHERE o.customer_id = c.customer_id AND o.status = 'active';
```
In SQL-92, explicit join syntax was introduced:
```sql
SELECT * FROM orders o JOIN customers c ON o.customer_id = c.customer_id WHERE o.status = 'active';
```
Reasons for the change:
1. Separation of concerns: Join logic (`ON`) is cleanly separated from row filtering logic (`WHERE`).
2. Introduction of Outer Joins (`LEFT`, `RIGHT`, `FULL`), which were impossible to express portably in SQL-89.
3. Prevention of accidental Cartesian products caused by omitting a join condition in the `WHERE` clause.
</details>

#### Q2: What is the purpose of `information_schema` in the SQL standard?
<details>
<summary>Answer</summary>
The `information_schema` is an ANSI standard collection of read-only views providing access to database metadata (catalog information). 

Prior to `information_schema`, each engine used proprietary system tables (e.g., `pg_catalog` in Postgres, `sys` tables in SQL Server). The `information_schema` provides a standardized, portable interface to query database metadata (`TABLES`, `COLUMNS`, `CHECK_CONSTRAINTS`, `ROUTINES`) using regular SQL queries.
</details>

#### Q3: What is the difference between `FETCH FIRST n ROWS ONLY` and `FETCH FIRST n ROWS WITH TIES`?
<details>
<summary>Answer</summary>
- `FETCH FIRST n ROWS ONLY`: Returns strictly at most $n$ rows, cutting off the result set even if the $(n+1)$-th row has the exact same sorting values as the $n$-th row according to the `ORDER BY` clause.
- `FETCH FIRST n ROWS WITH TIES`: Returns the top $n$ rows plus any additional rows that have identical sort values to the $n$-th row based on the `ORDER BY` criteria. (Requires an explicit `ORDER BY` clause).
</details>

#### Q4: Why is Backus-Naur Form (BNF) used in database documentation, and how do square brackets `[...]` differ from curly braces `{...}`?
<details>
<summary>Answer</summary>
BNF is a formal metasyntax used to unambiguously describe the grammar of computer languages.
- `[ ... ]` (Square Brackets) denote an **optional** syntactic element. The developer can include or completely omit it.
- `{ ... }` (Curly Braces) denote a **mandatory** grouped element, typically combined with the pipe symbol (`|`) to indicate that the developer *must* select exactly one of the enclosed options (e.g., `{ ASC | DESC }`).
</details>

### Practical Puzzles & Standards Conversion

#### Q5: Rewrite this proprietary MySQL query to be 100% compliant with ANSI SQL:2008 standards:
```sql
SELECT IFNULL(department_name, 'Unassigned') AS dept, 
       COUNT(*) AS total_staff
FROM employees
GROUP BY department_name
LIMIT 5 OFFSET 10;
```
<details>
<summary>Answer</summary>

```sql
SELECT COALESCE(department_name, 'Unassigned') AS dept, 
       COUNT(*) AS total_staff
FROM employees
GROUP BY department_name
ORDER BY department_name
OFFSET 10 ROWS
FETCH NEXT 5 ROWS ONLY;
```
Changes: `IFNULL` replaced with ANSI `COALESCE`, and `LIMIT / OFFSET` replaced with ANSI `OFFSET ... FETCH NEXT`. Added deterministic `ORDER BY` as required by the standard.
</details>

#### Q6: Write a query against `information_schema.columns` to find all columns named `email` across all user tables in the database.
<details>
<summary>Answer</summary>

```sql
SELECT 
    table_schema,
    table_name,
    column_name,
    data_type,
    is_nullable
FROM information_schema.columns
WHERE column_name = 'email'
  AND table_schema NOT IN ('pg_catalog', 'information_schema')
ORDER BY table_schema, table_name;
```
</details>

#### Q7: Why does the standard specify that `SELECT col1, SUM(col2) FROM tbl` is invalid without a `GROUP BY` clause, even if `tbl` contains only one row?
<details>
<summary>Answer</summary>
The SQL standard requires syntactic validation at parse/compile time, completely independent of the runtime row count. 

`SUM(col2)` is an aggregate function that produces a single scalar summary tuple, whereas `col1` is an unaggregated column that produces one value per underlying input tuple. Combining an unaggregated column with an aggregate function without a `GROUP BY` clause creates an ontological conflict between row-level and group-level dimensions, which the parser rejects.
</details>

#### Q8: Write an ANSI SQL query demonstrating the `FILTER` clause on aggregate functions (SQL:2003 Feature T612).
<details>
<summary>Answer</summary>

```sql
SELECT 
    department_id,
    COUNT(*) AS total_employees,
    COUNT(*) FILTER (WHERE salary > 100000) AS high_earners,
    AVG(salary) FILTER (WHERE salary > 100000) AS avg_high_salary
FROM employees
GROUP BY department_id;
```
*(PostgreSQL and SQLite support the standard `FILTER (WHERE ...)` clause; engines lacking it must fall back to `CASE` inside `COUNT`).*
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Querying Information Schema
Write an ANSI SQL query to list all tables in the `public` schema from `information_schema.tables`, showing `table_name` and `table_type`.
<details>
<summary>Solution</summary>

```sql
SELECT table_name, table_type
FROM information_schema.tables
WHERE table_schema = 'public'
ORDER BY table_name;
```
</details>

#### Exercise 2: Standard Pagination
Write a query to retrieve rows 11 through 20 from an `invoices` table sorted by `invoice_date` ascending, using ANSI SQL:2008 syntax.
<details>
<summary>Solution</summary>

```sql
SELECT invoice_id, customer_id, invoice_date, total_amount
FROM invoices
ORDER BY invoice_date ASC, invoice_id ASC
OFFSET 10 ROWS
FETCH NEXT 10 ROWS ONLY;
```
</details>

### Intermediate Exercises

#### Exercise 3: Inspecting Primary Keys Dynamically
Using `information_schema.table_constraints` and `information_schema.key_column_usage`, write a query that lists every table in your database alongside its defined Primary Key column(s).
<details>
<summary>Solution</summary>

```sql
SELECT 
    tc.table_name,
    kcu.column_name,
    kcu.ordinal_position
FROM information_schema.table_constraints tc
JOIN information_schema.key_column_usage kcu 
  ON tc.constraint_name = kcu.constraint_name 
 AND tc.table_schema = kcu.table_schema
WHERE tc.constraint_type = 'PRIMARY KEY'
  AND tc.table_schema = 'public'
ORDER BY tc.table_name, kcu.ordinal_position;
```
</details>

#### Exercise 4: ANSI Coalesce vs Dialect NVL
Given a table `employees(emp_id, bonus, commission)`, write an expression that returns `bonus`, or if `bonus` is NULL returns `commission`, or if both are NULL returns `0.00`, using standard ANSI SQL.
<details>
<summary>Solution</summary>

```sql
SELECT 
    emp_id,
    COALESCE(bonus, commission, 0.00) AS total_extra_comp
FROM employees;
```
</details>

### Advanced Exercises

#### Exercise 5: Standards Feature Introspection
Write a query against `information_schema.sql_features` that calculates the total percentage of ISO/IEC 9075:2023 features supported by your current database engine.
<details>
<summary>Solution</summary>

```sql
SELECT 
    COUNT(*) AS total_standard_features,
    COUNT(CASE WHEN is_supported = 'YES' THEN 1 END) AS supported_features,
    ROUND(
        (COUNT(CASE WHEN is_supported = 'YES' THEN 1 END)::numeric / NULLIF(COUNT(*), 0)) * 100.0,
        2
    ) AS standard_conformance_pct
FROM information_schema.sql_features;
```
</details>

#### Exercise 6: Multi-Level GROUPING SETS (SQL:1999 Feature T611)
Write an analytical query on `orders(customer_id, order_year, region, total_amount)` that computes the sum of `total_amount` across four explicit grouping dimensions simultaneously using `GROUPING SETS`:
1. `(customer_id, order_year)`
2. `(region, order_year)`
3. `(order_year)`
4. Global grand total `()`
<details>
<summary>Solution</summary>

```sql
SELECT 
    customer_id,
    region,
    order_year,
    SUM(total_amount) AS revenue
FROM orders
GROUP BY GROUPING SETS (
    (customer_id, order_year),
    (region, order_year),
    (order_year),
    ()
)
ORDER BY order_year NULLS LAST, region NULLS LAST, customer_id NULLS LAST;
```
</details>

---

## 12. Summary & Quick Reference

### SQL Standard Evolution Cheatsheet
- **SQL-92**: Foundation (`JOIN ... ON`, `CASE`, subqueries, data types).
- **SQL:1999**: CTEs (`WITH RECURSIVE`), triggers, user-defined functions.
- **SQL:2003**: Window functions (`OVER (PARTITION BY ...)`), `MERGE`.
- **SQL:2008**: `OFFSET ... FETCH FIRST ... ONLY / WITH TIES`.
- **SQL:2016**: Native JSON functions, `MATCH_RECOGNIZE`.
- **SQL:2023**: SQL/PGQ (Property Graph Queries), Any Value.

### BNF Syntax Legend
- `KEYWORD`: Literal command.
- `[ optional ]`: May be omitted.
- `{ required | choices }`: Pick one option.
- `item [, ...]`: Comma-separated list.

---

## 13. Further Reading
- ISO/IEC 9075:2023: [Official ISO Information Technology Database Languages SQL](https://www.iso.org/standard/76583.html)
- PostgreSQL Documentation: [Appendix D: SQL Conformance](https://www.postgresql.org/docs/17/features.html)
- Modern SQL: [Modern SQL Evolution & Features by Markus Winand](https://modern-sql.com/)
- SQLite Standard Conformance: [SQL As Understood By SQLite](https://www.sqlite.org/lang.html)
- [07 - How a Query Runs: The Internal Lifecycle](./07-how-a-query-runs.md)
