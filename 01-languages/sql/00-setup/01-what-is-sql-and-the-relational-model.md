# 01: What Is SQL & The Relational Model

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | None |
| **Target Audience** | Software Engineers, Systems Architects, Database Administrators |
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

In 1970, Dr. Edgar F. Codd published a landmark paper titled *"A Relational Model of Data for Large Shared Data Banks"*. At the time, data management was dominated by hierarchical (IBM IMS) and network (CODASYL) database models. These legacy systems tightly coupled the logical representation of data to its physical storage structures on disk. If an engineer added an index, reorganized pointers, or split a physical file, every application querying that database broke and had to be rewritten.

Codd proposed a radical abstraction: **Physical Data Independence**. Data should be represented purely in terms of mathematical relations (tables) and first-order predicate logic, completely divorcing application logic from memory layouts, B-Trees, or file pointers.

**Structured Query Language (SQL)**—originally developed at IBM by Donald Chamberlin and Raymond Boyce as SEQUEL—became the universal declarative manifestation of Codd's relational model. Unlike imperative programming languages where the developer instructs the computer *how* to navigate memory step-by-step, SQL allows the developer to specify *what* data is desired. The relational database management system (RDBMS) takes responsibility for determining the optimal physical execution strategy.

Understanding the mathematical foundations of the relational model enables engineers to design schemas that guarantee data consistency, avoid update anomalies, and allow query planners to execute at maximum performance.

---

## 2. Core Concepts

### Relational Model Foundations vs SQL Terminology
The relational model has formal mathematical definitions which map directly to SQL constructs:

| Mathematical Term | SQL Equivalent | Definition & Invariants |
|:---|:---|:---|
| **Relation** | Table (or View) | A set of ordered pairs representing a predicate. In pure relational theory, relations contain distinct tuples without duplicate rows and without innate ordering. |
| **Tuple** | Row (or Record) | A single element of a relation representing a concrete fact or entity instance. |
| **Attribute** | Column (or Field) | A named domain component within a relation. |
| **Domain** | Data Type & Constraints | The set of all permissible atomic values for a given attribute (e.g., `INTEGER`, `VARCHAR(100)`). |
| **Cardinality** | Row Count | The total number of tuples currently stored in a relation. |
| **Degree (Arity)** | Column Count | The total number of attributes defining the schema of a relation. |

### Relations vs Multisets (Bags)
In strict mathematical relational algebra, relations are **sets**:
1. Tuples are unordered.
2. Tuples are strictly unique (no two identical tuples can exist).

However, SQL deviates from pure relational theory by operating on **multisets** (also known as bags):
- By default, `SELECT` queries preserve duplicate rows unless `DISTINCT` or `GROUP BY` is explicitly invoked.
- Relational tables can store duplicate rows unless constrained by a `PRIMARY KEY` or `UNIQUE` constraint.
- Sorting (`ORDER BY`) is only meaningful at presentation time; relational storage itself has no inherent order.

### Declarative vs Imperative Data Retrieval
- **Imperative (e.g., C, Python, Java loop)**: "Open file `customers.dat`. Traverse pointer offset 0 to 4096. Check if byte 12 equals 'active'. Append pointer to array."
- **Declarative (SQL)**: `SELECT customer_id FROM customers WHERE status = 'active';`
  - The database engine chooses whether to read from an in-memory cache, scan a B-Tree index, utilize a bitmap scan, or parallelize a sequential disk read.

---

## 3. How It Works Internally

The core architectural promise of the relational model is **Physical Data Independence**, which is realized through the classic ANSI-SPARC Three-Level Database Architecture:

```
+-------------------------------------------------------------------+
|               External Level (User Views & Queries)               |
|   Query A: SELECT id, total   |   View B: Active Customers Only   |
+---------------------------------+---------------------------------+
                                  | Logical Schema Mapping
+---------------------------------v---------------------------------+
|               Conceptual Level (Logical Relational Model)         |
|   Tables, Columns, Primary Keys, Foreign Keys, CHECK Constraints  |
|   Relations: customers(id, name, status), orders(id, cust_id, amt)|
+---------------------------------+---------------------------------+
                                  | Physical Storage Mapping
+---------------------------------v---------------------------------+
|               Internal Level (Physical Storage Engine)            |
|   8KB/16KB Disk Pages, Slotted Page Layout, B-Tree Indexes,       |
|   Write-Ahead Logs (WAL), Buffer Pool Cache, Heap Tables          |
+-------------------------------------------------------------------+
```

### Relational Engine Processing Stages
When a declarative SQL statement is issued to an RDBMS:
1. **Parser & Lexer**: Converts the raw SQL string into an Abstract Syntax Tree (AST) while verifying grammar syntax.
2. **Catalog Resolver / Semantic Analyzer**: Validates table and column identifiers against system catalogs, checks user permissions, and infers data types.
3. **Query Rewriter**: Applies relational equivalence rules (e.g., flattening subqueries, simplifying constant expressions).
4. **Cost-Based Optimizer (CBO)**: Translates the relational expression into physical relational algebra operator trees (e.g., replacing a conceptual join with a physical Hash Join, Merge Join, or Index Nested Loop Scan) using statistical histogram metadata.
5. **Storage Engine Executor**: Interacts with the buffer pool and disk subsystems to retrieve pages and execute tuples.

---

## 4. Syntax & Usage

### Basic Table Definition & Relational Constraints
```sql
-- ANSI SQL:2023 compliant table definition enforcing relational invariants
CREATE TABLE accounts (
    account_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    account_number VARCHAR(34) NOT NULL UNIQUE,
    holder_name VARCHAR(100) NOT NULL,
    balance NUMERIC(15, 2) NOT NULL DEFAULT 0.00,
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT chk_positive_balance CHECK (balance >= 0.00)
);
```

### Multi-Dialect Comparison Matrix

| Feature / Behavior | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Identity / Auto-Increment** | `GENERATED ALWAYS AS IDENTITY` (Standard) or `SERIAL` | `AUTO_INCREMENT` | `INTEGER PRIMARY KEY AUTOINCREMENT` | `IDENTITY(1,1)` |
| **Strict Type Enforcement** | Strict; rejects invalid types and casts explicitly | Strict in standard SQL mode; loose in non-strict modes | Type affinity; dynamic typing allows storing text in int columns | Strict; strong static typing |
| **Boolean Data Type** | Full `BOOLEAN` (`TRUE`, `FALSE`, `NULL`) | `TINYINT(1)` synonym (`1`, `0`) | Numeric integer (`1`, `0`) | `BIT` (`1`, `0`, `NULL`) |
| **CHECK Constraints** | Fully validated and enforced | Enforced (since MySQL 8.0.16) | Enforced (if enabled, default on) | Fully validated and enforced |
| **Physical Storage Paradigm** | Heap tables + secondary indexes | Clustered index (B+ Tree organized by PK) | B-Tree pages per table/index | Clustered index by default, or Heap |

---

## 5. 5 Practical Progressive Examples

### Example 1: Basic Relation Modeling (Enforcing Set Semantics)
In pure relational theory, duplicate rows represent logical redundancy and violate entity integrity. We enforce set uniqueness via primary keys.

```sql
CREATE TABLE departments (
    department_id INTEGER PRIMARY KEY,
    name VARCHAR(50) NOT NULL UNIQUE
);

INSERT INTO departments (department_id, name) VALUES
(1, 'Engineering'),
(2, 'Finance'),
(3, 'Security');

SELECT department_id, name FROM departments ORDER BY department_id;
```
```text
+---------------+-------------+
| department_id | name        |
+---------------+-------------+
|             1 | Engineering |
|             2 | Finance     |
|             3 | Security    |
+---------------+-------------+
```

### Example 2: Relational Composition (Foreign Key Integrity)
Relations are connected by value matching (referential integrity), not by physical pointers.

```sql
CREATE TABLE employees (
    employee_id INTEGER PRIMARY KEY,
    department_id INTEGER NOT NULL REFERENCES departments(department_id),
    first_name VARCHAR(50) NOT NULL,
    last_name VARCHAR(50) NOT NULL,
    salary NUMERIC(10, 2) NOT NULL CHECK (salary > 0)
);

INSERT INTO employees VALUES
(101, 1, 'Elena', 'Rostova', 145000.00),
(102, 1, 'Marcus', 'Vance', 125000.00),
(103, 2, 'Sarah', 'Jenkins', 115000.00);

-- Querying the joined relation
SELECT 
    e.employee_id,
    e.first_name || ' ' || e.last_name AS full_name,
    d.name AS department,
    e.salary
FROM employees e
JOIN departments d ON e.department_id = d.department_id
ORDER BY e.salary DESC;
```
```text
+-------------+---------------+-------------+-----------+
| employee_id | full_name     | department  | salary    |
+-------------+---------------+-------------+-----------+
|         101 | Elena Rostova | Engineering | 145000.00 |
|         102 | Marcus Vance  | Engineering | 125000.00 |
|         103 | Sarah Jenkins | Finance     | 115000.00 |
+-------------+---------------+-------------+-----------+
```

### Example 3: Edge Case (Handling Unknowns in 3-Valued Logic)
The relational model incorporates `NULL` to represent missing or inapplicable information, governed by Three-Valued Logic (3VL: True, False, Unknown).

```sql
CREATE TABLE inventory (
    item_id INTEGER PRIMARY KEY,
    item_name VARCHAR(50) NOT NULL,
    reorder_threshold INTEGER -- NULL means threshold not determined
);

INSERT INTO inventory VALUES 
(1, 'Ball Bearing', 50),
(2, 'O-Ring', NULL),
(3, 'Piston Ring', 20);

-- Comparing against NULL directly evaluates to UNKNOWN, which WHERE treats as FALSE
SELECT item_id, item_name, reorder_threshold
FROM inventory
WHERE reorder_threshold = NULL; -- INCORRECT: Returns 0 rows!
```
```text
+---------+-----------+-------------------+
| item_id | item_name | reorder_threshold |
+---------+-----------+-------------------+
+---------+-----------+-------------------+
(0 rows returned)
```
Correct handling requires `IS NULL` or `IS NOT NULL`:
```sql
SELECT item_id, item_name, reorder_threshold
FROM inventory
WHERE reorder_threshold IS NULL;
```
```text
+---------+-----------+-------------------+
| item_id | item_name | reorder_threshold |
+---------+-----------+-------------------+
|       2 | O-Ring    |              NULL |
+---------+-----------+-------------------+
```

### Example 4: Failure Mode (Violating Relational Referential Constraints)
Attempting to insert a child tuple without a corresponding parent tuple violates referential integrity.

```sql
-- Attempting to assign employee to department 99 (non-existent)
INSERT INTO employees (employee_id, department_id, first_name, last_name, salary)
VALUES (104, 99, 'Tariq', 'Al-Mansoor', 95000.00);
```
```text
ERROR: insert or update on table "employees" violates foreign key constraint "employees_department_id_fkey"
DETAIL: Key (department_id)=(99) is not present in table "departments".
```

### Example 5: Optimized Relational Projection (Avoiding `SELECT *`)
Declarative SQL enables columnar projection optimization. Selecting only needed columns reduces I/O bandwidth, network serialization, and enables index-only scans.

```sql
-- Antipattern: SELECT * forces heap page fetches
-- Optimized: Explicit attribute projection
SELECT 
    d.name AS department_name,
    COUNT(e.employee_id) AS total_staff,
    COALESCE(AVG(e.salary), 0) AS average_salary
FROM departments d
LEFT JOIN employees e ON d.department_id = e.department_id
GROUP BY d.department_id, d.name
ORDER BY total_staff DESC;
```
```text
+-----------------+-------------+----------------+
| department_name | total_staff | average_salary |
+-----------------+-------------+----------------+
| Engineering     |           2 |      135000.00 |
| Finance         |           1 |      115000.00 |
| Security        |           0 |           0.00 |
+-----------------+-------------+----------------+
```

---

## 6. Best Practices

1. **Always Define a Primary Key**: Tables without primary keys violate entity integrity and default to bags rather than relational sets, preventing reliable replication, row identification, and ORM mapping.
2. **Enforce Integrity in the Engine, Not Just in Application Code**: Application validations can be bypassed by manual queries, batch scripts, or race conditions. Use `NOT NULL`, `CHECK`, and `FOREIGN KEY` constraints at the schema layer.
3. **Use Standard Types Over Vendor Synonyms**: Prefer `TIMESTAMP WITH TIME ZONE` over engine-specific timestamps, and standard ANSI types (`NUMERIC`, `VARCHAR`, `BIGINT`).
4. **Avoid Implicit Ordering Assumptions**: In relational algebra, a table is an unordered set. Never assume rows will be returned in insertion order; always supply an explicit `ORDER BY` clause when ordering matters.
5. **Never Use Comma Joins**: Always use explicit ANSI-92 `JOIN ... ON` syntax. Comma joins (`FROM a, b WHERE a.id = b.id`) are error-prone and easily degrade into accidental Cartesian products.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Relying on Default Physical Order** | Without `ORDER BY`, the database engine returns rows in arbitrary physical order based on disk page traversal or parallel worker timing. | Always write `ORDER BY column_name` when presentation order is required. |
| **Testing Equality with `= NULL`** | In 3VL, `column = NULL` yields `UNKNOWN`, which is rejected by `WHERE` clauses. | Use `column IS NULL` or `column IS NOT DISTINCT FROM value`. |
| **Missing Foreign Key Indexes** | While primary keys are automatically indexed, foreign key columns are generally NOT automatically indexed (except in MySQL InnoDB), causing full table scans during child table joins and parent row deletes. | Explicitly create an index on every foreign key column (`CREATE INDEX idx_emp_dept ON employees(department_id);`). |
| **Encoding Multiple Values in One Column** | Storing CSV strings (e.g. `'tag1,tag2,tag3'`) violates First Normal Form (1NF), making indexing, joining, and constraint validation impossible. | Normalize into a dedicated junction table or use typed JSONB/Arrays with GIN indexing. |
| **Using `SELECT *` in Production Code** | Breaks physical data independence: adding or reordering columns breaks positional readers and causes excessive I/O and network payload. | Explicitly enumerate only required columns: `SELECT id, name, created_at FROM ...`. |
| **Treating NULL as Zero or Empty String** | `NULL` represents the *absence* of a value. Math operations with NULL produce NULL (`100 + NULL = NULL`). String concatenations in some dialects produce NULL. | Use `COALESCE(val, 0)` or conditional checks when NULL needs fallback handling. |

---

## 8. Performance & Security Considerations

### Performance
- **Physical Independence Benefits**: Because queries express *intent* rather than *execution path*, database administrators can add B-Trees, partition tables, or rewrite storage layouts without changing a single line of application SQL.
- **Relational Pushdown**: The query optimizer pushes filters (`WHERE`) and projections down the execution tree as close to the disk scan as possible, minimizing the cardinality of intermediate tuples processed in memory.

### Security
- **Declarative Access Control**: Relational tables allow granular permission grants down to the column and row levels (`GRANT SELECT (id, name) ON users TO reporter_role`).
- **SQL Injection Vulnerabilities**: Because SQL blends query logic and data into a single string syntax, user inputs concatenated directly into SQL strings allow attackers to rewrite the AST. Always use parameterized queries (prepared statements).

---

## 9. Comparison Matrix

### Relational Model vs Alternative Data Models

| Dimension | Relational Model (RDBMS) | Document Store (NoSQL) | Graph Database | Key-Value Store |
|:---|:---|:---|:---|:---|
| **Data Structure** | Normalized 2D relations (tables) | Hierarchical JSON/BSON documents | Nodes, edges, and properties | Raw byte arrays / strings mapped to keys |
| **Query Language** | Declarative SQL (ANSI Standard) | Engine-specific APIs / MQL | Declarative graph languages (Cypher, GQL) | Simple key get/put/del operations |
| **Schema Strictness** | Schema-on-write (strictly enforced) | Schema-on-read (flexible / optional) | Schema-optional or flexible | Schemaless |
| **Integrity & ACID** | Strong referential integrity, full ACID transactions | Document-level atomicity, eventual consistency | ACID transactions across graph paths | Single-key atomicity |
| **Best Used For** | Financial systems, transactional ERP/CRM, core business domains | Content management, event logging, dynamic catalog objects | Social networks, fraud detection, identity trees | Caching, session state, fast lookups |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: What is "Physical Data Independence" and why is it the core premise of the relational model?
<details>
<summary>Answer</summary>
Physical Data Independence is the separation of the logical schema (how tables, columns, and relationships are structured conceptually) from the physical storage schema (how data is stored in files, B-Tree indexes, compressed pages, or memory segments). 

It ensures that database administrators can modify physical layouts (e.g., adding an index, partitioning a large table across disks, re-clustering a heap) without requiring developers to rewrite the application queries accessing that data.
</details>

#### Q2: How does SQL's multiset semantics differ from pure relational theory, and what are the practical implications?
<details>
<summary>Answer</summary>
Pure relational theory defines a relation as a mathematical set of unique tuples with no duplicate elements. SQL, however, operates on multisets (bags), meaning a table or query result can contain duplicate rows unless explicitly restricted by a `PRIMARY KEY`, `UNIQUE` constraint, or `DISTINCT` keyword. 

Practical implications:
1. SQL can aggregate non-distinct values directly (e.g., `SUM(amount)`).
2. Forgetting a join predicate can produce duplicate rows without throwing a theoretical set violation.
3. Operations like `UNION` incur deduplication sorting costs, whereas `UNION ALL` maintains multiset behavior without sorting overhead.
</details>

#### Q3: Explain Edgar Codd's Information Rule (Rule 1 of Codd's 12 Rules).
<details>
<summary>Answer</summary>
Codd's Rule 1 (The Information Rule) states: *"All information in a relational database is represented explicitly at the logical level and in exactly one way—by values in tables."* 

This means that metadata, relationship linkages, table structures, and user data must all be stored in tabular form and queried using the same relational query language. System catalogs (like `information_schema` or `pg_catalog`) are themselves regular tables that can be queried with SQL.
</details>

#### Q4: What is Three-Valued Logic (3VL) and why does `SELECT * FROM tbl WHERE col = NULL` never return rows?
<details>
<summary>Answer</summary>
SQL operates on Three-Valued Logic comprising `TRUE`, `FALSE`, and `UNKNOWN`. In 3VL, `NULL` represents an unknown or absent value. Therefore, comparing any value to `NULL` using equality (`=`) evaluates to `UNKNOWN` because the engine cannot know if two unknown values are identical. 

The `WHERE` clause requires a predicate to evaluate strictly to `TRUE` to include a row. Because `col = NULL` evaluates to `UNKNOWN`, the condition is never `TRUE`, and zero rows are returned. The correct syntax is `col IS NULL`.
</details>

### Query Writing & Debugging Puzzles

#### Q5: Given a table without a primary key containing duplicate rows, write a standard SQL query to eliminate duplicates and keep only one instance of each row.
<details>
<summary>Answer</summary>
Using a Common Table Expression (CTE) with the `ROW_NUMBER()` window function:

```sql
WITH RankedRows AS (
    SELECT 
        ctid, -- In PostgreSQL, ctid is the physical tuple location. In standard SQL, use an identity or all columns.
        ROW_NUMBER() OVER (
            PARTITION BY first_name, last_name, email 
            ORDER BY ctid
        ) AS rn
    FROM customers_duplicate
)
DELETE FROM customers_duplicate
WHERE ctid IN (
    SELECT ctid FROM RankedRows WHERE rn > 1
);
```
In SQLite or engines without `ctid`:
```sql
DELETE FROM customers_duplicate
WHERE rowid NOT IN (
    SELECT MIN(rowid)
    FROM customers_duplicate
    GROUP BY first_name, last_name, email
);
```
</details>

#### Q6: Why does `WHERE department_id NOT IN (SELECT department_id FROM departments_with_nulls)` return zero rows if the subquery returns even a single NULL value?
<details>
<summary>Answer</summary>
`v NOT IN (a, b, NULL)` translates to:
`v <> a AND v <> b AND v <> NULL`.

Since `v <> NULL` evaluates to `UNKNOWN`, the entire logical chain becomes:
`TRUE AND TRUE AND UNKNOWN` $\rightarrow$ `UNKNOWN`.

Because `WHERE` filters out any row whose condition evaluates to `UNKNOWN`, the entire query returns empty. To fix this, always filter out NULLs in subqueries used with `NOT IN`, or use `NOT EXISTS`:

```sql
WHERE NOT EXISTS (
    SELECT 1 FROM departments_with_nulls d 
    WHERE d.department_id = employees.department_id
)
```
</details>

#### Q7: Write a query to verify whether an existing column in a relation violates candidate key constraints (has duplicates).
<details>
<summary>Answer</summary>
```sql
SELECT 
    email,
    COUNT(*) AS duplicate_count
FROM users
GROUP BY email
HAVING COUNT(*) > 1;
```
If this query returns zero rows, the column satisfies candidate key uniqueness.
</details>

#### Q8: Construct a query demonstrating relational division: Find all students who have taken EVERY course in the Computer Science curriculum.
<details>
<summary>Answer</summary>
```sql
SELECT s.student_id, s.name
FROM students s
JOIN enrollments e ON s.student_id = e.student_id
JOIN courses c ON e.course_id = c.course_id
WHERE c.department = 'Computer Science'
GROUP BY s.student_id, s.name
HAVING COUNT(DISTINCT c.course_id) = (
    SELECT COUNT(*) 
    FROM courses 
    WHERE department = 'Computer Science'
);
```
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Enforce Unique Entity Representation
Create a table `products` with columns `sku`, `title`, and `retail_price`. Enforce that `sku` cannot be duplicated, `title` cannot be null, and `retail_price` cannot be negative.
<details>
<summary>Solution</summary>

```sql
CREATE TABLE products (
    product_id INTEGER PRIMARY KEY,
    sku VARCHAR(30) NOT NULL UNIQUE,
    title VARCHAR(120) NOT NULL,
    retail_price NUMERIC(10, 2) NOT NULL CHECK (retail_price >= 0.00)
);
```
</details>

#### Exercise 2: Declarative Range Filtering
Given an `orders` table, retrieve all order records created between January 1, 2024 and March 31, 2024 inclusive, with an `order_status` of `'completed'`, sorted by total amount descending.
<details>
<summary>Solution</summary>

```sql
SELECT order_id, customer_id, total_amount, created_at
FROM orders
WHERE order_status = 'completed'
  AND created_at >= '2024-01-01'
  AND created_at < '2024-04-01'
ORDER BY total_amount DESC;
```
</details>

### Intermediate Exercises

#### Exercise 3: Cross-Table Foreign Key Reference & Cascades
Design a `projects` table and an `assigned_tasks` table. If a project is deleted, all its associated tasks must be automatically deleted by the database engine.
<details>
<summary>Solution</summary>

```sql
CREATE TABLE projects (
    project_id INTEGER PRIMARY KEY,
    name VARCHAR(100) NOT NULL
);

CREATE TABLE assigned_tasks (
    task_id INTEGER PRIMARY KEY,
    project_id INTEGER NOT NULL REFERENCES projects(project_id) ON DELETE CASCADE,
    description TEXT NOT NULL,
    is_completed BOOLEAN NOT NULL DEFAULT FALSE
);
```
</details>

#### Exercise 4: 3VL Guarded Calculation
Write a query calculating the profit margin `((selling_price - cost_price) / selling_price) * 100` for items in an `inventory` table. Handle cases where `selling_price` is 0 or `cost_price` is NULL without throwing division by zero or returning errors.
<details>
<summary>Solution</summary>

```sql
SELECT 
    item_id,
    selling_price,
    cost_price,
    CASE 
        WHEN selling_price IS NULL OR selling_price <= 0 OR cost_price IS NULL THEN NULL
        ELSE ROUND(((selling_price - cost_price) / selling_price) * 100.0, 2)
    END AS profit_margin_pct
FROM inventory;
```
Alternative standard idiom:
```sql
SELECT 
    item_id,
    ROUND(((selling_price - cost_price) / NULLIF(selling_price, 0)) * 100.0, 2) AS profit_margin_pct
FROM inventory;
```
</details>

### Advanced Exercises

#### Exercise 5: Relational Set Difference Without `EXCEPT`
Find all customer IDs who registered in 2023 but have never placed an order, without using the `EXCEPT` or `MINUS` keywords.
<details>
<summary>Solution</summary>

```sql
SELECT c.customer_id, c.email
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id
WHERE c.created_at >= '2023-01-01' 
  AND c.created_at < '2024-01-01'
  AND o.order_id IS NULL;

-- Or using NOT EXISTS:
SELECT c.customer_id, c.email
FROM customers c
WHERE c.created_at >= '2023-01-01' 
  AND c.created_at < '2024-01-01'
  AND NOT EXISTS (
      SELECT 1 FROM orders o WHERE o.customer_id = c.customer_id
  );
```
</details>

#### Exercise 6: Verifying Functional Dependencies
Write a query to test if the functional dependency $X \rightarrow Y$ holds on a relation `emp_departments(employee_ssn, department_name)`. The dependency holds if every SSN maps to exactly one department name.
<details>
<summary>Solution</summary>

```sql
SELECT employee_ssn
FROM emp_departments
GROUP BY employee_ssn
HAVING COUNT(DISTINCT department_name) > 1;
```
If this query returns any records, the functional dependency $X \rightarrow Y$ is violated.
</details>

---

## 12. Summary & Quick Reference

### Key Takeaways
- **Codd's Relational Model**: Abstracted physical storage (pointers, disk pages) into mathematical relations (tables) governed by first-order predicate logic.
- **Declarative Power**: You specify *what* data is required; the cost-based optimizer determines *how* to physically retrieve it.
- **Physical Data Independence**: Schemas, indexes, and partitions can change without breaking application queries.
- **Sets vs Bags**: Pure relational algebra uses unique sets; SQL uses multisets (bags) by default. Use `PRIMARY KEY` and `UNIQUE` constraints to enforce entity uniqueness.
- **Three-Valued Logic (3VL)**: Missing values evaluate to `UNKNOWN`. Never compare with `= NULL`; always use `IS NULL`.

### Relational Algebra Operator Mapping
| Relational Algebra Operator | Symbol | SQL Equivalent |
|:---|:---:|:---|
| **Selection** | $\sigma$ | `WHERE` |
| **Projection** | $\pi$ | `SELECT col1, col2` |
| **Cartesian Product** | $\times$ | `CROSS JOIN` |
| **Join** | $\bowtie$ | `INNER JOIN ... ON` |
| **Union** | $\cup$ | `UNION` |
| **Set Difference** | $-$ | `EXCEPT` / `MINUS` |
| **Intersection** | $\cap$ | `INTERSECT` |

---

## 13. Further Reading
- Edgar F. Codd (1970). *"A Relational Model of Data for Large Shared Data Banks"*, Communications of the ACM.
- C.J. Date. *"An Introduction to Database Systems"* (8th Edition), Addison-Wesley.
- PostgreSQL 17 Documentation: [Chapter 1: Relational Databases](https://www.postgresql.org/docs/17/tutorial-arch.html)
- ANSI/ISO/IEC 9075:2023: *Information Technology — Database Languages — SQL*.
- [02 - RDBMS Landscape & Dialects](./02-rdbms-landscape-and-dialects.md)
