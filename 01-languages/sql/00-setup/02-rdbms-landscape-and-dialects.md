# 02: RDBMS Landscape & Dialects

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022, Oracle 23ai |
| **Prerequisites** | [01 - What Is SQL & The Relational Model](./01-what-is-sql-and-the-relational-model.md) |
| **Target Audience** | Software Engineers, Solution Architects, Infrastructure Engineers |
| **Verification Status** | Verified against engine official release specifications |

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

While the SQL standard defines a theoretical and syntactic baseline, real-world database systems diverge significantly in their physical architectures, storage engines, concurrency algorithms, and dialect idiosyncrasies. Choosing a database engine is one of the most consequential architectural decisions in software engineering: it impacts system latency, concurrency limits, data integrity guarantees, operational complexity, and infrastructure cost.

Developers often treat SQL as a single monolithic language, leading to painful surprises when migrating between systems or attempting to write cross-platform database logic. For example:
- PostgreSQL organizes data in unordered heap pages with multiversion tuple copies and background vacuuming.
- MySQL’s InnoDB engine forces data into a clustered B+ Tree organized strictly by primary key.
- SQLite embeds an entire serverless, zero-configuration engine directly into the application process memory space.
- Microsoft SQL Server combines Transact-SQL (T-SQL), clustered indexes, and query store telemetry.

Understanding the strengths, trade-offs, and dialect boundaries of these engines allows engineers to choose the right tool for their workload and write robust, engine-aware SQL.

---

## 2. Core Concepts

### 1. Client-Server vs Embedded Architectures
- **Client-Server RDBMS (PostgreSQL, MySQL, SQL Server)**:
  - The database runs as an independent daemon/service.
  - Clients communicate over TCP sockets or Unix domain sockets using proprietary wire protocols (e.g., PostgreSQL Frontend/Backend protocol, MySQL Client/Server protocol).
  - Handles concurrent connections from hundreds or thousands of clients via connection pooling and multi-process/multi-threaded workers.
- **Embedded RDBMS (SQLite, DuckDB)**:
  - The database engine is a compiled C library linked directly into the host application process.
  - No network overhead, zero configuration, zero daemon processes.
  - Database is a single ordinary disk file. Concurrent writes are coordinated via file-system locks.

### 2. Storage Engine Paradigms: Heap Tables vs Clustered Indexes
The physical layout of table rows on disk determines I/O patterns for lookups and insertions:

```
Heap Table (PostgreSQL)                  Clustered Index Table (MySQL InnoDB)
+-----------------------------------+    +-----------------------------------+
| Heap File (Pages of Tuples)       |    | Clustered B+ Tree (Root / Internal|
| [Tup 1] [Tup 2] [Tup 3] [Tup 4]   |    |         /              \          |
|                                   |    |   [Node A]            [Node B]    |
| Secondary Index (B-Tree)          |    |      /   \             /    \     |
| [Key -> Points to Heap Block/Off] |    | [Leaf Pages: Holds Full Row Data] |
+-----------------------------------+    +-----------------------------------+
```

- **Heap-Organized Tables (PostgreSQL)**:
  - Rows are inserted into the first available space in 8KB disk pages.
  - Rows do not move unless updated. Secondary indexes store physical tuple pointers (`ctid` = block number + offset).
  - Fast insertions; secondary index lookups require an extra dereference to the heap page.
- **Clustered Index-Organized Tables (MySQL InnoDB, SQL Server)**:
  - The table *is* the primary key B+ Tree index. Leaf pages contain the actual table columns.
  - Primary key point lookups and range scans are extremely fast because data is co-located with the index.
  - Secondary indexes do not point to physical disk addresses; they store the Primary Key value, requiring a "double lookup" (secondary index scan followed by clustered index traversal).

### 3. Open Source vs Proprietary Engines
- **PostgreSQL**: Permissive PostgreSQL License (similar to BSD/MIT). No commercial licensing fees, massive community and extension ecosystem (PostGIS, pgvector, TimescaleDB).
- **MySQL**: Dual-licensed (GPLv2 and Commercial by Oracle Corporation). Powers massive web platforms (WordPress, Meta, Shopify).
- **SQLite**: Dedicated to the Public Domain. The most widely deployed software engine in human history (billions of smartphones, browsers, IoT devices).
- **SQL Server**: Proprietary commercial software (Microsoft). Deep integration with Azure, Active Directory, and .NET enterprise ecosystems.
- **Oracle Database**: Proprietary enterprise tier. Industry-standard for Fortune 500 financial and legacy ERP infrastructure.

---

## 3. How It Works Internally

### Process Models vs Thread Models

The underlying operating system execution model dictates how each engine allocates memory and scales concurrent connections:

| Dimension | PostgreSQL | MySQL (InnoDB) | SQLite | SQL Server |
|:---|:---|:---|:---|:---|
| **Concurrency Model** | Multi-Process (`fork()` per client connection) | Multi-Threaded (one thread per client connection) | In-Process (same thread as caller) | Multi-Threaded (SQLOS thread pooling) |
| **Memory Allocation** | Shared Memory (shared buffers) + per-process `work_mem` | Global Buffer Pool + per-thread session memory | Process heap memory (`sqlite3_malloc`) | Buffer Pool managed dynamically by SQLOS |
| **Scaling Mechanism** | Requires external connection pooler (PgBouncer, Odyssey) for 1000+ conns | Handles hundreds of threads; thread pool plugin available | Single-writer lock (WAL allows concurrent readers) | Internal scheduler handles thousands of connections |
| **Extension Model** | Pluggable shared libraries (C, Rust via `pgrx`) | Storage Engine API, Component Architecture | Loadable C extensions (`sqlite3_load_extension`) | CLR Integration, External Scripts |

---

## 4. Syntax & Usage

### Dialect Peculiarities: String Concatenation, Auto-Increment & Dates

Each engine uses divergent syntax for common operational tasks:

```sql
-- PostgreSQL: ANSI Standard concatenation and timestamp arithmetic
SELECT 'User: ' || username AS label,
       created_at + INTERVAL '7 days' AS expiry
FROM users;

-- MySQL: Function-based concatenation and date add
SELECT CONCAT('User: ', username) AS label,
       DATE_ADD(created_at, INTERVAL 7 DAY) AS expiry
FROM users;

-- SQLite: Standard concatenation and datetime modifier
SELECT 'User: ' || username AS label,
       datetime(created_at, '+7 days') AS expiry
FROM users;

-- SQL Server (T-SQL): Plus operator and DATEADD function
SELECT 'User: ' + username AS label,
       DATEADD(day, 7, created_at) AS expiry
FROM users;
```

### Multi-Dialect Comparison Matrix

| Architectural Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Primary Storage Engine** | Heap + MVCC | InnoDB (Clustered B+ Tree) | B-Tree file pages | Clustered Index / Heap |
| **Transaction Model** | ACID, True Serializable (SSI), MVCC | ACID, MVCC via Undo Logs | ACID (Rollback Journal or WAL) | ACID, Lock-based or Snapshot |
| **String Quoting** | Single quotes `'text'` for literals, double `"` for identifiers | Single/Double for literals, backticks `` `id` `` for identifiers | Single for literals, double/square/backticks for identifiers | Single `'text'` for literals, `[id]` or `"` for identifiers |
| **Limit / Offset Syntax** | `LIMIT n OFFSET m` or `FETCH FIRST n ROWS` | `LIMIT n OFFSET m` | `LIMIT n OFFSET m` | `OFFSET m ROWS FETCH NEXT n ROWS ONLY` |
| **Upsert Syntax** | `ON CONFLICT (...) DO UPDATE` | `ON DUPLICATE KEY UPDATE` | `ON CONFLICT (...) DO UPDATE` | `MERGE INTO ... USING` |
| **Returning Clause** | `RETURNING col1, col2` | Not supported (requires secondary query or triggers) | `RETURNING col1, col2` (since 3.35) | `OUTPUT INSERTED.col1` |
| **Window Functions** | Full ANSI support with frames and exclusion | Full ANSI support (since 8.0) | Supported (since 3.25) | Supported |
| **JSON Support** | Native binary `JSONB` with GIN indexing | Native binary JSON with generated columns | JSON functions on text | Native JSON functions and JSON type |

---

## 5. 5 Practical Progressive Examples

### Example 1: Basic Portable DDL (Universal Subset)
Writing SQL that executes identically across all four major engines requires sticking to the ANSI-92 lowest common denominator.

```sql
CREATE TABLE product_categories (
    category_id INTEGER NOT NULL PRIMARY KEY,
    category_name VARCHAR(50) NOT NULL UNIQUE,
    is_active INTEGER DEFAULT 1 NOT NULL
);

INSERT INTO product_categories (category_id, category_name) VALUES
(1, 'Electronics'),
(2, 'Books'),
(3, 'Apparel');

SELECT category_id, category_name, is_active 
FROM product_categories 
ORDER BY category_id;
```
```text
+-------------+---------------+-----------+
| category_id | category_name | is_active |
+-------------+---------------+-----------+
|           1 | Electronics   |         1 |
|           2 | Books         |         1 |
|           3 | Apparel       |         1 |
+-------------+---------------+-----------+
```

### Example 2: Dialect Divergence in Upserts (Atomic Insert or Update)
Handling duplicate key conflicts across engines:

```sql
-- PostgreSQL 17 & SQLite 3.50+ Standard ON CONFLICT
INSERT INTO user_logins (user_id, login_count, last_login)
VALUES (42, 1, CURRENT_TIMESTAMP)
ON CONFLICT (user_id) 
DO UPDATE SET 
    login_count = user_logins.login_count + 1,
    last_login = CURRENT_TIMESTAMP;

-- MySQL 8.4 LTS ON DUPLICATE KEY UPDATE
INSERT INTO user_logins (user_id, login_count, last_login)
VALUES (42, 1, NOW())
ON DUPLICATE KEY UPDATE 
    login_count = login_count + 1,
    last_login = NOW();

-- SQL Server 2022 MERGE
MERGE user_logins AS target
USING (SELECT 42 AS user_id) AS source
ON target.user_id = source.user_id
WHEN MATCHED THEN
    UPDATE SET login_count = target.login_count + 1, last_login = CURRENT_TIMESTAMP
WHEN NOT MATCHED THEN
    INSERT (user_id, login_count, last_login) VALUES (42, 1, CURRENT_TIMESTAMP);
```

### Example 3: Edge Case (Type Affinity in SQLite vs Strict Typing)
Demonstrating SQLite's unique dynamic typing (type affinity) compared to PostgreSQL's strict type verification.

In SQLite (Permissive):
```sql
CREATE TABLE loose_types (
    id INTEGER PRIMARY KEY,
    numeric_val INTEGER
);

-- SQLite happily accepts text into an INTEGER column!
INSERT INTO loose_types (id, numeric_val) VALUES (1, 'Not a number');
SELECT id, numeric_val, typeof(numeric_val) FROM loose_types;
```
```text
+----+--------------+--------------------+
| id | numeric_val  | typeof(numeric_val)|
+----+--------------+--------------------+
|  1 | Not a number | text               |
+----+--------------+--------------------+
```

In PostgreSQL 17 (Strict):
```sql
CREATE TABLE strict_types (
    id INTEGER PRIMARY KEY,
    numeric_val INTEGER
);
INSERT INTO strict_types (id, numeric_val) VALUES (1, 'Not a number');
```
```text
ERROR: invalid input syntax for type integer: "Not a number"
LINE 1: ...VALUES (1, 'Not a number');
```
*(PostgreSQL strictly guarantees domain integrity at write time).*

### Example 4: Failure Mode (Identifier Case Sensitivity & Quoting)
How engines treat unquoted identifiers:

- **PostgreSQL**: Lowercases unquoted identifiers. `SELECT MyColumn FROM MyTable;` searches for `mycolumn` in table `mytable`. If created with double quotes (`CREATE TABLE "MyTable" ("MyColumn" INT)`), it can ONLY be queried with double quotes.
- **MySQL (Linux)**: Table names are case-sensitive because they match physical files on the Linux file system! `users` and `Users` are different tables. Column names are case-insensitive.
- **SQL Server & SQLite**: Case-insensitive by default.

```sql
-- In PostgreSQL:
CREATE TABLE "SensitiveTable" (
    id INT PRIMARY KEY
);

-- Fails in Postgres:
SELECT * FROM SensitiveTable;
-- ERROR: relation "sensitivetable" does not exist
-- Fix:
SELECT * FROM "SensitiveTable";
```

### Example 5: Optimized Execution Pattern (PostgreSQL `RETURNING` vs Workarounds)
Returning the newly generated primary key from an insert statement:

```sql
-- PostgreSQL & SQLite: Atomic single-round-trip return
INSERT INTO orders (customer_id, total_amount)
VALUES (101, 299.95)
RETURNING order_id, created_at;
```
```text
+----------+----------------------------+
| order_id | created_at                 |
+----------+----------------------------+
|     5001 | 2026-11-26 11:55:00+00     |
+----------+----------------------------+
```

In MySQL (Requires two round trips):
```sql
INSERT INTO orders (customer_id, total_amount) VALUES (101, 299.95);
SELECT LAST_INSERT_ID(); -- Bound to connection session
```

---

## 6. Best Practices

1. **Default to PostgreSQL for New Projects**: Unless you have specific enterprise constraints (e.g., legacy .NET shop requiring SQL Server) or embedded requirements (SQLite), PostgreSQL offers the most robust standard compliance, advanced indexing (GIN, BRIN), and rich extension ecosystem (pgvector, PostGIS).
2. **Use SQLite for Testing, CLI Tools, and Edge Devices**: SQLite is unbeatable for unit tests, local development sandboxes, desktop software, and embedded mobile applications.
3. **Always Lowercase Identifiers with Snake_Case**: Avoid quoted identifiers (`"CamelCase"`) to ensure your DDL and queries remain portable across case-folding engines.
4. **Never Rely on Auto-Casting in Production**: Explicitly cast data types (`CAST(val AS INTEGER)` or `val::integer`) rather than expecting the engine to perform implicit type coercion.
5. **Architect for Connection Pooling**: If deploying PostgreSQL in high-concurrency environments (Node.js, serverless lambdas), always put a connection pooler (PgBouncer, Supabase Supavisor) in front of the database to mitigate its multi-process memory overhead.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Assuming MySQL Table Names Are Case-Insensitive** | On Linux, MySQL table names correspond to file paths on the ext4/xfs filesystem, making `orders` and `Orders` two completely distinct tables, breaking code deployed from macOS/Windows to Linux. | Always standardize on lowercase `snake_case` table and column names across all environments. |
| **Using SQLite for Highly Concurrent Multi-Writer Applications** | SQLite uses database-level locking for writes; concurrent writes queue up and trigger `SQLITE_BUSY` errors under high write load. | Use PostgreSQL or MySQL when your workload requires dozens of simultaneous writer processes. |
| **Assuming Primary Keys Clustered by Default Everywhere** | MySQL InnoDB always clusters by PK; PostgreSQL uses an unordered heap where secondary indexes require heap page lookups. | In PostgreSQL, use covering indexes (`INCLUDE` clause) or `CLUSTER` if sequential physical ordering is required. |
| **Using `SELECT LAST_INSERT_ID()` Across Pooled Connections** | If an application thread releases a connection between the `INSERT` and the `LAST_INSERT_ID()` call, another thread's insert can be read. | Use `RETURNING` in PostgreSQL/SQLite, or ensure both statements execute within the same unbroken transaction session in MySQL. |
| **Using Vendor Non-Standard Operators** | Operators like `!=` vs `<>`, `+` for string concatenation, or `IFNULL()` instead of `COALESCE()` break portability across engines. | Use ANSI standard SQL: `<>` for not-equal, `||` for string concatenation, `COALESCE()` for null fallback. |
| **Ignoring PostgreSQL `work_mem` Defaults** | PostgreSQL defaults to a modest `work_mem` (4MB); complex joins and sorts spill to temporary disk files, degrading performance dramatically. | Tune `work_mem` appropriately for analytical queries (`SET work_mem = '64MB';`). |

---

## 8. Performance & Security Considerations

### Performance
- **Connection Overhead**:
  - PostgreSQL forks a new OS process per connection (~5MB–10MB memory per idle connection). Spawning 1,000 direct connections can exhaust server RAM.
  - MySQL uses one thread per connection (~256KB stack), handling higher raw connection counts with lower base memory.
- **Vacuuming & Bloat in PostgreSQL**:
  - Because PostgreSQL updates write a new tuple version to the heap, frequent updates create dead tuples. Proper autovacuum tuning is mandatory to prevent table and index bloat.
- **InnoDB Buffer Pool in MySQL**:
  - Set `innodb_buffer_pool_size` to 70–80% of total system RAM on dedicated database servers to keep hot B+ Tree pages in memory.

### Security
- **Default Superuser Accounts**:
  - PostgreSQL uses `postgres`; MySQL uses `root`; SQL Server uses `sa`. Always rename or lock down default administrator accounts in production.
- **Network Listening**:
  - By default, modern installations bind only to `localhost` (`127.0.0.1`). Exposing ports `5432` or `3306` to `0.0.0.0` without TLS certificates and firewall restrictions invites automated brute-force attacks.

---

## 9. Comparison Matrix

### Comprehensive RDBMS Architecture Comparison

| Dimension | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **License** | Open Source (PostgreSQL) | GPLv2 / Commercial (Oracle) | Public Domain | Commercial (Microsoft) |
| **Process Model** | Multi-process | Multi-threaded | In-process library | Multi-threaded (SQLOS) |
| **Storage Architecture** | Heap + Secondary B-Trees | Clustered Index (InnoDB) | B-Tree pages | Clustered Index / Heap |
| **ANSI Standard Adherence** | Very High (> 170 SQL:2023 features) | Moderate | High (Core SQL-92/2003) | High (T-SQL extensions) |
| **Full-Text Search** | Built-in `tsvector` & `tsquery` | Built-in Full-Text Indexes | FTS5 Extension | Full-Text Engine |
| **AI / Vector Support** | `pgvector` extension | Native Vector (8.4+) | `sqlite-vec` extension | Azure / T-SQL Vector Support |
| **JSON Indexing** | Deep (`JSONB` + GIN) | Generated columns + functional index | JSON1 functions | JSON path expressions |
| **Ideal Workload** | High-concurrency OLTP, analytics, geospatial, AI | High-scale web applications, e-commerce | Mobile, desktop, embedded, IoT, dev/test | Windows enterprise, Azure cloud, large ERP |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: Why does MySQL InnoDB enforce a clustered primary key while PostgreSQL uses heap tables, and what are the trade-offs?
<details>
<summary>Answer</summary>
In MySQL InnoDB, data is stored directly in the leaf pages of the primary key's B+ Tree.
- **Advantage**: Lookups by primary key or range scans on the primary key require zero heap dereferencing; data is immediately available at the index leaf.
- **Disadvantage**: Secondary indexes cannot store physical disk pointers (because rows move when pages split); they store the primary key, forcing secondary lookups to perform two traversals.

In PostgreSQL, tables are unordered heaps.
- **Advantage**: Insertions can be appended anywhere with free space. Secondary indexes point directly to physical heap tuples (`ctid`), making secondary index lookups fast without re-traversing the primary key.
- **Disadvantage**: Reading rows in primary key order requires hopping across arbitrary heap pages unless data is explicitly reorganized via `CLUSTER`.
</details>

#### Q2: What causes SQLite to return an `SQLITE_BUSY` error, and how should an application architect mitigate it?
<details>
<summary>Answer</summary>
SQLite coordinates concurrency via file-level locks. While multiple readers can read simultaneously in WAL mode, only a single writer can execute at any instant. If a transaction attempts to write while another write transaction holds the lock, SQLite will wait for a configured timeout (`busy_timeout`) and then return `SQLITE_BUSY`.

Mitigations:
1. Enable WAL mode (`PRAGMA journal_mode = WAL;`).
2. Configure a sensible busy timeout (`PRAGMA busy_timeout = 5000;`).
3. In multi-threaded applications, route all write operations through a single dedicated write thread or queue.
</details>

#### Q3: Contrast PostgreSQL's multi-process model with MySQL's multi-threaded model in terms of memory usage and scalability.
<details>
<summary>Answer</summary>
PostgreSQL forks a distinct operating system process for each client connection. Each process has its own address space, requiring 5MB–10MB of base OS memory overhead per connection. Scaling to thousands of connections without an external connection pooler (e.g., PgBouncer) can cause memory exhaustion and excessive OS context switching.

MySQL uses a single multi-threaded process where each client connection is assigned a lightweight thread. Threads share the process address space and buffer pool, requiring significantly less memory per connection (~256KB stack). MySQL can natively handle higher raw connection counts than PostgreSQL before requiring an external pooler.
</details>

#### Q4: What is the semantic difference between `VARCHAR(100)` in PostgreSQL and MySQL?
<details>
<summary>Answer</summary>
In modern versions of both engines, `VARCHAR(100)` specifies a maximum length of 100 *characters* (not bytes), properly handling multi-byte UTF-8 characters. 

However, differences exist:
- In PostgreSQL, trailing spaces are significant in comparisons: `'abc '` does NOT equal `'abc'`.
- In MySQL, trailing whitespace in `VARCHAR` comparisons depends on the collation padding attribute (e.g., `PAD SPACE` vs `NO PAD`), where standard collations historically ignore trailing spaces in comparisons.
</details>

### Practical Dialect Translation & Puzzles

#### Q5: Translate this PostgreSQL query to MySQL 8.4 LTS:
```sql
SELECT user_id, 
       email,
       created_at + INTERVAL '30 days' AS renewal_date
FROM users
WHERE is_active = TRUE
ORDER BY created_at DESC
LIMIT 10 OFFSET 20;
```
<details>
<summary>Answer</summary>

```sql
SELECT user_id, 
       email,
       DATE_ADD(created_at, INTERVAL 30 DAY) AS renewal_date
FROM users
WHERE is_active = 1
ORDER BY created_at DESC
LIMIT 20, 10; -- Or: LIMIT 10 OFFSET 20; (MySQL supports both)
```
Key changes: `DATE_ADD` function used, boolean converted to integer `1` (or `TRUE` synonym), and interval syntax adjusted.
</details>

#### Q6: Given a scenario where an application frequently updates non-primary key columns, which engine experiences more index write amplification: PostgreSQL or MySQL? Why?
<details>
<summary>Answer</summary>
PostgreSQL generally experiences higher index write amplification unless HOT (Heap-Only Tuples) optimization triggers. 
- In PostgreSQL, an `UPDATE` creates a new tuple in the heap. Because the tuple has a new physical address (`ctid`), every secondary index on that table must be updated to point to the new location, unless the updated column is unindexed and space exists on the same page for HOT.
- In MySQL InnoDB, updating an unindexed column only modifies the clustered index leaf page. Because secondary indexes store the Primary Key value (which did not change), secondary indexes require zero updates!
</details>

#### Q7: How do you implement a case-insensitive search in PostgreSQL vs MySQL without altering column collation?
<details>
<summary>Answer</summary>
In PostgreSQL:
```sql
-- Using the ILIKE operator (PostgreSQL specific)
SELECT * FROM users WHERE email ILIKE 'john%';

-- Or standard lower() function (can be indexed via expression index):
SELECT * FROM users WHERE LOWER(email) = LOWER('John@Example.com');
```

In MySQL:
By default, MySQL's standard collations (e.g., `utf8mb4_0900_ai_ci`) are already Case-Insensitive (`_ci`):
```sql
-- Already case-insensitive by default in MySQL:
SELECT * FROM users WHERE email = 'john@example.com';
```
</details>

#### Q8: Write an ANSI SQL query that safely computes the division of two columns, returning 0 if the denominator is 0 or NULL, that works identically on Postgres, MySQL, SQLite, and SQL Server.
<details>
<summary>Answer</summary>

```sql
SELECT 
    order_id,
    COALESCE(revenue / NULLIF(units_sold, 0), 0) AS avg_unit_price
FROM sales_summary;
```
`NULLIF(units_sold, 0)` returns `NULL` when `units_sold` is 0. Dividing by `NULL` returns `NULL`. `COALESCE` replaces `NULL` with `0`. This expression is 100% portable across all engines.
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Standard Dialect DDL
Write a table creation statement for an `audit_log` table containing `log_id` (auto-incrementing integer PK), `event_name` (string up to 100 chars), and `logged_at` (current timestamp) that compiles without error on both PostgreSQL 17 and SQLite 3.50+.
<details>
<summary>Solution</summary>

```sql
CREATE TABLE audit_log (
    log_id INTEGER PRIMARY KEY,
    event_name VARCHAR(100) NOT NULL,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL
);
```
*(In SQLite, `INTEGER PRIMARY KEY` automatically acts as an auto-incrementing alias for `rowid`. In PostgreSQL, standard integer PK can be populated via sequences or identity, but SQLite accepts this exact DDL).*
</details>

#### Exercise 2: Portable Date Formatting
Write an SQL statement that extracts the year from a column `birth_date` across standard databases.
<details>
<summary>Solution</summary>

```sql
-- Standard ANSI SQL:
SELECT EXTRACT(YEAR FROM birth_date) AS birth_year FROM persons;

-- Also supported across most engines:
-- PostgreSQL: EXTRACT(YEAR FROM birth_date)
-- MySQL: YEAR(birth_date) or EXTRACT(YEAR FROM birth_date)
-- SQLite: CAST(strftime('%Y', birth_date) AS INTEGER)
```
</details>

### Intermediate Exercises

#### Exercise 3: Cross-Engine String Concatenation
Construct a query that concatenates `first_name`, a single space, and `last_name` into `full_name`, ensuring that if `last_name` is NULL, the result still displays the `first_name` rather than returning NULL.
<details>
<summary>Solution</summary>

```sql
SELECT 
    first_name || ' ' || COALESCE(last_name, '') AS full_name
FROM contacts;
```
*(In PostgreSQL and SQLite, `||` is standard. In MySQL, ensure `PIPES_AS_CONCAT` SQL mode is enabled, or use `CONCAT(first_name, ' ', COALESCE(last_name, ''))`).*
</details>

#### Exercise 4: Conditional Aggregation
Write a query to count how many orders are `'pending'` vs `'completed'` in a single row without using dialect-specific `FILTER` clauses.
<details>
<summary>Solution</summary>

```sql
SELECT 
    COUNT(CASE WHEN status = 'pending' THEN 1 END) AS pending_orders,
    COUNT(CASE WHEN status = 'completed' THEN 1 END) AS completed_orders
FROM orders;
```
</details>

### Advanced Exercises

#### Exercise 5: Emulating `RETURNING` in MySQL
Given an application executing an `INSERT` into a table with an auto-incrementing primary key in MySQL, write the transactional sequence of commands required to safely capture the inserted row's ID in a multi-threaded application.
<details>
<summary>Solution</summary>

```sql
START TRANSACTION;

INSERT INTO customer_orders (customer_id, order_total)
VALUES (101, 450.00);

-- Captures the exact ID generated on THIS connection thread:
SET @new_order_id = LAST_INSERT_ID();

INSERT INTO order_items (order_id, product_id, quantity)
VALUES (@new_order_id, 999, 2);

COMMIT;
```
</details>

#### Exercise 6: Portable Pagination
Write an ANSI SQL:2008 compliant query that returns the 3rd page of products (page size: 25 items), ordered by `price` descending, using `OFFSET ... FETCH` syntax instead of the vendor-specific `LIMIT` clause.
<details>
<summary>Solution</summary>

```sql
SELECT product_id, title, price
FROM products
ORDER BY price DESC, product_id ASC
OFFSET 50 ROWS
FETCH NEXT 25 ROWS ONLY;
```
*(Standard SQL requires an explicit `ORDER BY` with deterministic tie-breakers when using offset/fetch pagination).*
</details>

---

## 12. Summary & Quick Reference

### Engine Selection Decision Tree
```
Is the database embedded directly in an app, mobile client, or CLI tool?
├── YES ──► Use SQLite
└── NO  ──► Client-Server RDBMS
             ├── Do you need rich extensibility, geospatial, vector AI, or strict ANSI compliance?
             │    └── YES ──► Use PostgreSQL 17+
             ├── Is your stack built on a legacy LAMP/WordPress architecture or MySQL-specialized tooling?
             │    └── YES ──► Use MySQL 8.4 LTS
             └── Is your organization standardized on Microsoft .NET / Azure Enterprise?
                  └── YES ──► Use Microsoft SQL Server 2022
```

### Key Architectural Differences
- **PostgreSQL**: Heap tables + MVCC vacuuming + multi-process architecture. Best for complex queries, analytics, and rich data types.
- **MySQL (InnoDB)**: Clustered primary key B+ Tree + undo log MVCC + multi-threaded architecture. Best for high-velocity key-based web transactions.
- **SQLite**: Single-file B-Tree + in-process engine + zero-configuration. Best for edge, testing, local caching, and desktop/mobile software.
- **SQL Server**: Clustered index default + T-SQL + deep enterprise integration.

---

## 13. Further Reading
- PostgreSQL 17 Architecture: [PostgreSQL Official Documentation](https://www.postgresql.org/docs/17/)
- MySQL 8.4 Reference Manual: [InnoDB Storage Engine Architecture](https://dev.mysql.com/doc/refman/8.4/en/innodb-storage-engine.html)
- SQLite Architecture: [How SQLite Works Under the Hood](https://www.sqlite.org/arch.html)
- Microsoft SQL Server Architecture: [SQL Server Technical Documentation](https://learn.microsoft.com/en-us/sql/sql-server/)
- [03 - Installing PostgreSQL, MySQL & SQLite](./03-installing-postgresql-mysql-sqlite.md)
