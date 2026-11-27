# 03: Data Integrity & Constraints

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [02 - Table Definition: CREATE, ALTER, DROP & TRUNCATE](./02-create-alter-drop-table.md) |
| **Target Audience** | Software Engineers, Data Architects, Database Administrators |
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

In relational database theory, data integrity is the mathematical guarantee that all data stored within a database remains accurate, valid, consistent, and semantically sound across all transactional operations. While application developers frequently attempt to enforce business rules exclusively in application code (via ORMs, validation libraries, or API middleware), application-level validation alone is fundamentally insufficient.

Application validations can be bypassed:
- A developer runs an ad-hoc `UPDATE` or `INSERT` script directly via the CLI.
- A bug or unhandled edge case in a background worker creates orphaned child records.
- Concurrent race conditions between two application instances allow duplicate entries or negative inventory balances to slip through.

Relational constraints—**PRIMARY KEY**, **FOREIGN KEY**, **UNIQUE**, **NOT NULL**, and **CHECK**—are enforced by the database engine at the physical storage layer. No matter how many application microservices connect, what language they are written in, or what concurrent transactions they run, the database engine mathematically prevents corrupt or inconsistent states from ever being written to disk.

---

## 2. Core Concepts

### The Five Relational Integrity Constraints

```
┌────────────────────────────────────────────────────────────────────────┐
│                   Relational Integrity Constraint Model                │
├───────────────────┬────────────────────────────────────────────────────┤
│ 1. PRIMARY KEY    │ Entity Integrity: Uniquely identifies each tuple;  │
│                   │ strictly NOT NULL and UNIQUE. Backed by B-Tree.    │
├───────────────────┼────────────────────────────────────────────────────┤
│ 2. FOREIGN KEY    │ Referential Integrity: Enforces relationship valid-│
│                   │ ity between child and parent tables.               │
├───────────────────┼────────────────────────────────────────────────────┤
│ 3. UNIQUE         │ Candidate Key Uniqueness: Prevents duplicate       │
│                   │ non-null values across specified column(s).        │
├───────────────────┼────────────────────────────────────────────────────┤
│ 4. NOT NULL       │ Completeness Integrity: Forbids UNKNOWN / NULL     │
│                   │ values from being stored in the attribute.         │
├───────────────────┼────────────────────────────────────────────────────┤
│ 5. CHECK          │ Domain Integrity: Evaluates arbitrary boolean      │
│                   │ expressions (e.g., price >= 0, end_date > start).  │
└───────────────────┴────────────────────────────────────────────────────┘
```

### 1. Referential Actions on Foreign Keys
When a row in a parent table is deleted or updated, the foreign key constraint dictates what happens to referencing child rows:
- **`ON DELETE RESTRICT` (Default)**: Rejects the parent deletion immediately if any child row references it.
- **`ON DELETE NO ACTION`**: Similar to RESTRICT, but can be deferred until the end of the transaction.
- **`ON DELETE CASCADE`**: Automatically deletes all matching child rows when the parent row is deleted.
- **`ON DELETE SET NULL`**: Sets the foreign key column in all child rows to `NULL` (requires child column to be nullable).
- **`ON DELETE SET DEFAULT`**: Resets the foreign key column in all child rows to its defined column `DEFAULT`.

### 2. Deferrable Constraints (`DEFERRABLE`)
By default, constraints are checked immediately at statement execution time (`NOT DEFERRABLE`). 
However, for complex multi-table circular references (e.g., Table A references Table B, and Table B references Table A):
- Constraints can be declared `DEFERRABLE INITIALLY DEFERRED`.
- The database engine delays evaluating constraint compliance until `COMMIT` time, allowing temporary intermediate inconsistent states within the boundary of an active transaction.

---

## 3. How It Works Internally

### Constraint Validation & Physical Index Backing
- **Primary Key & Unique Constraints**:
  - The database engine does not scan the table to check uniqueness. Instead, it automatically instantiates an underlying **Unique B-Tree Index**.
  - On every `INSERT` or `UPDATE`, the engine traverses the B-Tree in $O(\log N)$ time. If an identical key value already exists in an index leaf page, the engine aborts the write with a constraint violation.
- **Foreign Key Indexing Gotcha**:
  - While parent primary keys are indexed, **foreign key columns on child tables are NOT automatically indexed** in PostgreSQL, SQLite, or SQL Server (only MySQL InnoDB auto-indexes them).
  - Without an index on `child_table(parent_id)`, deleting a single row from the parent table forces the engine to perform a **Full Table Sequential Scan** on the entire child table to verify that no child rows reference it!

```
Parent Table (departments)                   Child Table (employees)
+---------------------------+                +-------------------------------+
| dept_id (PK: Indexed)     |                | emp_id (PK)                   |
| 1: Engineering            |<───────────────| dept_id (FK: Unindexed!)      |
| 2: Marketing              |                +-------------------------------+
+---------------------------+                               ▲
              │                                             │
      DELETE FROM dept WHERE id = 1;                        │
              ▼                                             │
      Database engine must scan EVERY page of employees ────┘
      (Seq Scan 1,000,000 rows just to verify no reference exists!)
```

---

## 4. Syntax & Usage

### Comprehensive Schema with Explicit Constraints

```sql
CREATE TABLE authors (
    author_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    author_name VARCHAR(100) NOT NULL,
    email VARCHAR(150) NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL,
    -- Table-level unique constraint
    CONSTRAINT uq_author_email UNIQUE (email)
);

CREATE TABLE articles (
    article_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    author_id BIGINT NOT NULL,
    slug VARCHAR(120) NOT NULL,
    title VARCHAR(200) NOT NULL,
    body TEXT NOT NULL,
    view_count INTEGER DEFAULT 0 NOT NULL,
    status VARCHAR(20) DEFAULT 'draft' NOT NULL,
    published_at TIMESTAMP WITH TIME ZONE,

    -- Foreign Key with Referential Action
    CONSTRAINT fk_articles_author 
        FOREIGN KEY (author_id) 
        REFERENCES authors(author_id) 
        ON DELETE RESTRICT 
        ON UPDATE CASCADE,

    -- Multi-Column Unique Constraint
    CONSTRAINT uq_author_slug UNIQUE (author_id, slug),

    -- Multi-Column Domain Check Constraints
    CONSTRAINT chk_positive_views CHECK (view_count >= 0),
    CONSTRAINT chk_valid_status CHECK (status IN ('draft', 'review', 'published', 'archived')),
    CONSTRAINT chk_published_consistency CHECK (
        (status = 'published' AND published_at IS NOT NULL) OR
        (status <> 'published' AND published_at IS NULL)
    )
);

-- MANDATORY: Explicit B-Tree index on child foreign key column!
CREATE INDEX idx_articles_author_id ON articles(author_id);
```

### Multi-Dialect Comparison Matrix

| Constraint Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **`CHECK` Constraints** | Fully supported | Supported (since 8.0.16)| Supported | Fully supported |
| **Deferrable Constraints** | `DEFERRABLE INITIALLY DEFERRED` | Not supported | Supported (`DEFERRABLE`) | Not supported |
| **`UNIQUE` with NULLs** | Standard: multiple NULLs allowed (`NULLS NOT DISTINCT` option in 15+)| Multiple NULLs allowed | Multiple NULLs allowed | Only ONE NULL allowed in unique index! |
| **Foreign Key Auto-Indexing**| Not indexed (must create manually) | Automatically indexed by InnoDB | Not indexed | Not indexed |
| **Foreign Key Enforcement** | Enforced by default | Enforced (InnoDB) | Disabled by default (`PRAGMA foreign_keys = ON;`)| Enforced by default |

---

## 5. 5 Practical Progressive Examples

### Example 1: Basic Column vs Table Constraints
Simple column-level constraints versus multi-column compound constraints:

```sql
CREATE TABLE conference_rooms (
    room_id INT PRIMARY KEY,
    building_code VARCHAR(10) NOT NULL,
    room_number INT NOT NULL,
    capacity INT NOT NULL CHECK (capacity > 0),
    -- Multi-column compound unique constraint
    CONSTRAINT uq_building_room UNIQUE (building_code, room_number)
);

INSERT INTO conference_rooms VALUES 
(1, 'BLD-A', 101, 20),
(2, 'BLD-A', 102, 10),
(3, 'BLD-B', 101, 15); -- Same room number, different building: VALID!

SELECT * FROM conference_rooms ORDER BY room_id;
```
```text
+---------+---------------+-------------+----------+
| room_id | building_code | room_number | capacity |
+---------+---------------+-------------+----------+
|       1 | BLD-A         |         101 |       20 |
|       2 | BLD-A         |         102 |       10 |
|       3 | BLD-B         |         101 |       15 |
+---------+---------------+-------------+----------+
```

### Example 2: Handling NULLs in UNIQUE Constraints (`NULLS NOT DISTINCT`)
In the ANSI SQL standard and historical PostgreSQL, `NULL <> NULL`. Therefore, a standard `UNIQUE` constraint permits multiple rows with `NULL`:

```sql
CREATE TABLE standard_unique (
    id INT PRIMARY KEY,
    national_id VARCHAR(20) UNIQUE
);

INSERT INTO standard_unique VALUES (1, NULL), (2, NULL);
-- Works! Both rows accepted because NULLs are treated as distinct.
```

In PostgreSQL 15+, you can enforce that NULLs are treated as identical duplicate values using `NULLS NOT DISTINCT`:

```sql
CREATE TABLE strict_unique (
    id INT PRIMARY KEY,
    national_id VARCHAR(20) UNIQUE NULLS NOT DISTINCT
);

INSERT INTO strict_unique VALUES (1, NULL);
INSERT INTO strict_unique VALUES (2, NULL);
-- ERROR: duplicate key value violates unique constraint "strict_unique_national_id_key"
-- DETAIL: Key (national_id)=(null) already exists.
```

### Example 3: Edge Case (Circular Dependencies with Deferrable Constraints)
When two tables reference each other, inserting either one first normally violates foreign key constraints:

```sql
CREATE TABLE teams (
    team_id INT PRIMARY KEY,
    team_name VARCHAR(50) NOT NULL,
    captain_id INT -- References players
);

CREATE TABLE players (
    player_id INT PRIMARY KEY,
    team_id INT NOT NULL REFERENCES teams(team_id),
    name VARCHAR(50) NOT NULL
);

-- Add circular foreign key as DEFERRABLE
ALTER TABLE teams 
    ADD CONSTRAINT fk_team_captain 
    FOREIGN KEY (captain_id) 
    REFERENCES players(player_id) 
    DEFERRABLE INITIALLY DEFERRED;

-- Atomic insertion inside transaction
BEGIN;
  -- Insert team with non-existent captain (allowed because deferred!)
  INSERT INTO teams VALUES (1, 'Apex Predators', 101);
  -- Insert player referencing team
  INSERT INTO players VALUES (101, 1, 'Sarah Conner');
-- At COMMIT, both constraints are satisfied!
COMMIT;
```

### Example 4: Failure Mode (Violating Multi-Column Check Constraints)
Demonstrating how cross-column check constraints prevent illegal state transitions:

```sql
-- Attempting to set status = 'published' without a published_at timestamp
INSERT INTO articles (author_id, slug, title, body, status, published_at)
VALUES (1, 'my-first-post', 'Welcome', 'Hello world', 'published', NULL);
```
```text
ERROR: new row for relation "articles" violates check constraint "chk_published_consistency"
DETAIL: Failing row contains (1, 1, my-first-post, Welcome, Hello world, 0, published, null).
```
*(The check constraint guarantees that no draft article can be marked published without a timestamp).*

### Example 5: Optimized Foreign Key with `ON DELETE SET NULL`
Designing resilient parent-child relationships where deleting a user preserves their historical audit logs:

```sql
CREATE TABLE support_tickets (
    ticket_id BIGINT PRIMARY KEY,
    subject VARCHAR(200) NOT NULL,
    assigned_agent_id BIGINT, -- Nullable!
    CONSTRAINT fk_assigned_agent 
        FOREIGN KEY (assigned_agent_id) 
        REFERENCES employees(employee_id) 
        ON DELETE SET NULL
);

-- If employee 102 leaves the company:
DELETE FROM employees WHERE employee_id = 102;

-- The support ticket remains intact, with assigned_agent_id set to NULL:
SELECT ticket_id, subject, assigned_agent_id FROM support_tickets;
```
```text
+-----------+-------------------------+-------------------+
| ticket_id | subject                 | assigned_agent_id |
+-----------+-------------------------+-------------------+
|     88001 | Cannot reset 2FA token  |              NULL |
+-----------+-------------------------+-------------------+
```

---

## 6. Best Practices

1. **Always Index Foreign Key Columns**: In PostgreSQL, SQL Server, and SQLite, foreign key columns are not automatically indexed. Failing to index foreign keys causes full table scans on parent deletes and slows down joins.
2. **Name All Constraints Explicitly**: Never rely on auto-generated constraint names (like `articles_pkey` or `table_chk_1`). Name them with meaningful conventions: `fk_child_parent`, `uq_table_column`, `chk_table_rule`. Clear names make production error logs readable.
3. **Use Domain `CHECK` Constraints Instead of Enums**: Database `ENUM` types can be difficult to alter and drop across migrations. A `CHECK (status IN ('active', 'pending', 'cancelled'))` constraint is easy to inspect and update via standard `ALTER TABLE`.
4. **Prefer `ON DELETE RESTRICT` for Financial Entities**: Avoid `CASCADE` on core business transactions (invoices, ledgers, customer orders). If an administrative user accidentally deletes a customer, cascading delete could wipe millions of dollars of financial audit history.
5. **Use Multi-Column Check Constraints for State Machines**: Enforce status-dependent column rules (e.g., `end_date >= start_date`, `cancellation_reason IS NOT NULL` only when `status = 'cancelled'`).

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Assuming Unique Constraints Reject Multiple NULLs** | Standard SQL treats each `NULL` as distinct, allowing 1,000 duplicate rows with `NULL` in a `UNIQUE` column. | In Postgres 15+, use `UNIQUE NULLS NOT DISTINCT` or a partial unique index (`WHERE col IS NOT NULL`). |
| **Omitting Indexes on Foreign Key Columns** | When a parent row is deleted, the engine must perform a sequential scan of the entire child table to verify no references exist. | Always create an explicit B-Tree index on every foreign key column (`CREATE INDEX idx_child_fk ON child(parent_id);`). |
| **Using `CHECK` with Non-Deterministic Functions** | Using `CHECK (created_at <= now())` fails during backup restore because `now()` evaluates to the restore date, rejecting valid historical rows! | Check constraints must be strictly deterministic functions of the row’s own columns. |
| **Accidental Cascade Deletions** | Setting `ON DELETE CASCADE` on `orders` referencing `customers` means deleting a customer account permanently destroys all historical order records. | Use `ON DELETE RESTRICT` or soft-deletes (`deleted_at`). |
| **SQL Server Unique Index Failing on Second NULL** | Unlike Postgres and MySQL, standard SQL Server unique indexes treat `NULL` as a single unique value, rejecting a second `NULL`. | In SQL Server, create a filtered unique index: `CREATE UNIQUE INDEX idx ON tbl(col) WHERE col IS NOT NULL;`. |
| **Forgetting to Enable SQLite Foreign Keys** | SQLite silently accepts invalid foreign keys without error unless activated per connection. | Always run `PRAGMA foreign_keys = ON;` in SQLite connections. |

---

## 8. Performance & Security Considerations

### Performance
- **Validation Overhead on Bulk Writes**:
  - Every constraint must be evaluated on every `INSERT` and `UPDATE`. A table with 15 complex check constraints and 8 foreign keys will experience slower write throughput than a bare table.
  - For massive bulk loads (millions of rows), consider temporarily dropping constraints or adding them as `NOT VALID` and validating them in batch afterwards.

### Security
- **Data Tampering Protection**:
  - Constraints act as physical firewalls against compromised application code. If a vulnerability in an API endpoint attempts to set an account balance to `-10,000`, a database `CHECK (balance >= 0)` rejects the update at the storage layer regardless of application privileges.

---

## 9. Comparison Matrix

### Constraint Types & Invariants

| Constraint | Scope | Index Required | Modifies Child Rows | Evaluated At |
|:---|:---|:---:|:---:|:---|
| **`PRIMARY KEY`** | Entity identity | Yes (Unique B-Tree) | No | Statement (or Commit if deferred) |
| **`FOREIGN KEY`** | Cross-table relation | No (Recommended!) | Yes (via Cascade/Set Null) | Statement (or Commit if deferred) |
| **`UNIQUE`** | Column / Compound | Yes (Unique B-Tree) | No | Statement (or Commit if deferred) |
| **`NOT NULL`** | Single column | No (Catalog flag) | No | Statement |
| **`CHECK`** | Row expressions | No | No | Statement |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: Why does deleting a single row from a parent table cause severe database locking and high CPU when foreign key columns are unindexed?
<details>
<summary>Answer</summary>
When a row in a parent table is deleted, the database must verify that referential integrity is not violated. It must check the child table to confirm that no child row references the deleted parent's primary key.

If the child table's foreign key column is unindexed:
1. The engine cannot perform an $O(\log N)$ B-Tree index lookup.
2. It is forced to execute a **Full Table Sequential Scan** across the entire child table.
3. If the child table contains 10 million rows, every single parent deletion triggers a 10-million-row scan, holding table locks and saturating disk I/O.
</details>

#### Q2: How does the SQL standard handle multiple `NULL` values in a `UNIQUE` constraint, and how does SQL Server deviate from this?
<details>
<summary>Answer</summary>
In the ANSI SQL standard (and PostgreSQL, MySQL, SQLite), `NULL` represents an unknown value. In Three-Valued Logic, `NULL = NULL` is `UNKNOWN`, not `TRUE`. Therefore, two `NULL` values are not considered equal to each other, allowing multiple rows with `NULL` in a unique column.

**SQL Server Deviation**: Historically, Microsoft SQL Server implements unique constraints strictly such that `NULL` is treated as a distinct concrete value. Consequently, SQL Server permits only **one single row** with `NULL` in a unique column; inserting a second `NULL` throws a unique constraint violation! 
Workaround in SQL Server: Create a filtered unique index:
`CREATE UNIQUE NONCLUSTERED INDEX idx_col ON tbl(col) WHERE col IS NOT NULL;`
</details>

#### Q3: What is the purpose of `DEFERRABLE INITIALLY DEFERRED` constraints?
<details>
<summary>Answer</summary>
By default, constraints are checked immediately after each SQL statement executes. If an intermediate statement temporarily violates a constraint, the transaction is immediately aborted.

A `DEFERRABLE INITIALLY DEFERRED` constraint instructs the database engine to delay checking the constraint until transaction `COMMIT` time. This is critical for:
1. Circular dependencies (e.g., Table A references Table B, and Table B references Table A).
2. Swapping unique ranking numbers (e.g., reordering ranks 1, 2, 3 without triggering unique collisions on intermediate steps).
3. Cascading updates that temporarily unbalance double-entry ledgers before the offsetting entry is posted.
</details>

#### Q4: Why are non-deterministic functions (like `CURRENT_TIMESTAMP` or `RANDOM()`) forbidden inside `CHECK` constraints?
<details>
<summary>Answer</summary>
A `CHECK` constraint must be a deterministic mathematical invariant of the tuple's own attribute values. 

If a check constraint allowed `CHECK (created_at <= CURRENT_TIMESTAMP)`, the condition might evaluate to `TRUE` when inserted. However, if the database is restored from a backup 5 years later, or if a table row is updated or re-indexed in the future, re-evaluating the constraint against a different `CURRENT_TIMESTAMP` could suddenly fail, rendering valid committed data unreadable or un-dumpable.
</details>

### Practical Constraint Puzzles

#### Q5: Write an SQL table definition for an `events` table that enforces that `end_time` must always be strictly greater than `start_time`.
<details>
<summary>Answer</summary>

```sql
CREATE TABLE scheduled_events (
    event_id INT PRIMARY KEY,
    title VARCHAR(100) NOT NULL,
    start_time TIMESTAMP WITH TIME ZONE NOT NULL,
    end_time TIMESTAMP WITH TIME ZONE NOT NULL,
    CONSTRAINT chk_valid_time_range CHECK (end_time > start_time)
);
```
</details>

#### Q6: How do you swap the unique rank numbers of two rows (e.g., swapping rank 1 and rank 2) in a table with a `UNIQUE(rank)` constraint without violating uniqueness?
<details>
<summary>Answer</summary>
In PostgreSQL with a deferrable constraint:
```sql
ALTER TABLE items ADD CONSTRAINT uq_rank UNIQUE (rank) DEFERRABLE INITIALLY DEFERRED;

BEGIN;
  UPDATE items SET rank = 2 WHERE id = 'A'; -- rank was 1
  UPDATE items SET rank = 1 WHERE id = 'B'; -- rank was 2
COMMIT;
```
Without a deferrable constraint, use an intermediate temporary placeholder value (e.g., `-1` or `NULL`):
```sql
BEGIN;
  UPDATE items SET rank = -1 WHERE id = 'A';
  UPDATE items SET rank = 1 WHERE id = 'B';
  UPDATE items SET rank = 2 WHERE id = 'A';
COMMIT;
```
</details>

#### Q7: Write a check constraint that validates that an `email` column contains at least one `@` symbol and one `.` character.
<details>
<summary>Answer</summary>

```sql
ALTER TABLE users 
    ADD CONSTRAINT chk_basic_email_format 
    CHECK (email LIKE '%@%.%');

-- Or using standard POSIX regex in PostgreSQL:
ALTER TABLE users 
    ADD CONSTRAINT chk_regex_email 
    CHECK (email ~* '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$');
```
</details>

#### Q8: Write a query to find all unindexed foreign keys in a PostgreSQL database.
<details>
<summary>Answer</summary>

```sql
SELECT 
    c.conrelid::regclass AS table_name,
    c.conname AS foreign_key_name,
    string_agg(a.attname, ', ') AS fk_columns
FROM pg_constraint c
JOIN pg_attribute a ON a.attnum = ANY(c.conkey) AND a.attrelid = c.conrelid
WHERE c.contype = 'f'
  AND NOT EXISTS (
      SELECT 1 FROM pg_index i
      WHERE i.indrelid = c.conrelid
        AND i.indkey[0:array_length(c.conkey, 1) - 1] = c.conkey
  )
GROUP BY c.conrelid, c.conname;
```
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Multi-Column Primary Key Definition
Create an `enrollments` junction table linking `student_id` and `course_id` where the combination of both forms a composite primary key, with an enrollment date defaulting to today.
<details>
<summary>Solution</summary>

```sql
CREATE TABLE enrollments (
    student_id INTEGER NOT NULL,
    course_id INTEGER NOT NULL,
    enrolled_at DATE DEFAULT CURRENT_DATE NOT NULL,
    PRIMARY KEY (student_id, course_id)
);
```
</details>

#### Exercise 2: Positive Number Check Constraint
Create a table `bank_accounts` with an `account_id`, `holder_name`, and `balance` where `balance` cannot be negative.
<details>
<summary>Solution</summary>

```sql
CREATE TABLE bank_accounts (
    account_id BIGINT PRIMARY KEY,
    holder_name VARCHAR(100) NOT NULL,
    balance NUMERIC(15, 2) NOT NULL CHECK (balance >= 0.00)
);
```
</details>

### Intermediate Exercises

#### Exercise 3: Cascading Foreign Key Setup
Create a `parent_posts` table and a `child_comments` table. Configure the relationship such that if a post is deleted, all its comments are deleted automatically, but if a post ID is updated, the change cascades to comments.
<details>
<summary>Solution</summary>

```sql
CREATE TABLE parent_posts (
    post_id BIGINT PRIMARY KEY,
    title VARCHAR(200) NOT NULL
);

CREATE TABLE child_comments (
    comment_id BIGINT PRIMARY KEY,
    post_id BIGINT NOT NULL,
    comment_text TEXT NOT NULL,
    CONSTRAINT fk_post_comment 
        FOREIGN KEY (post_id) 
        REFERENCES parent_posts(post_id) 
        ON DELETE CASCADE 
        ON UPDATE CASCADE
);

CREATE INDEX idx_comments_post_id ON child_comments(post_id);
```
</details>

#### Exercise 4: Conditional Constraint (Soft Delete Check)
Add a check constraint to a `users` table with columns `is_deleted BOOLEAN` and `deleted_at TIMESTAMPTZ` enforcing that `deleted_at` must be NOT NULL if `is_deleted = TRUE`, and must be NULL if `is_deleted = FALSE`.
<details>
<summary>Solution</summary>

```sql
ALTER TABLE users 
    ADD CONSTRAINT chk_soft_delete_consistency 
    CHECK (
        (is_deleted = TRUE AND deleted_at IS NOT NULL) OR
        (is_deleted = FALSE AND deleted_at IS NULL)
    );
```
</details>

### Advanced Exercises

#### Exercise 5: Mutual Exclusive Arc (Polymorphic FK Guard)
In an audit system, an `audit_log` row must reference EITHER a `user_id` OR an `api_key_id`, but never both and never neither. Enforce this via a table check constraint.
<details>
<summary>Solution</summary>

```sql
CREATE TABLE audit_logs (
    log_id BIGINT PRIMARY KEY,
    user_id BIGINT REFERENCES users(user_id),
    api_key_id BIGINT REFERENCES api_keys(key_id),
    action VARCHAR(50) NOT NULL,
    -- Exactly one must be NOT NULL:
    CONSTRAINT chk_xor_actor CHECK (
        (user_id IS NOT NULL AND api_key_id IS NULL) OR
        (user_id IS NULL AND api_key_id IS NOT NULL)
    )
);
```
</details>

#### Exercise 6: Deferrable Circular Dependency Resolution
Write a self-contained transaction that creates a `countries` table and a `cities` table where each country has a `capital_city_id` pointing to `cities`, and each city has a `country_id` pointing to `countries`. Insert 'France' and 'Paris' atomically without constraint violations.
<details>
<summary>Solution</summary>

```sql
CREATE TABLE countries (
    country_id INT PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    capital_city_id INT
);

CREATE TABLE cities (
    city_id INT PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    country_id INT NOT NULL REFERENCES countries(country_id)
);

ALTER TABLE countries 
    ADD CONSTRAINT fk_capital 
    FOREIGN KEY (capital_city_id) 
    REFERENCES cities(city_id) 
    DEFERRABLE INITIALLY DEFERRED;

BEGIN;
  INSERT INTO countries VALUES (1, 'France', 101);
  INSERT INTO cities VALUES (101, 'Paris', 1);
COMMIT;
```
</details>

---

## 12. Summary & Quick Reference

### Constraints Summary
- **`PRIMARY KEY`**: Unique + Not Null; one per table; automatically backed by unique B-Tree.
- **`FOREIGN KEY`**: Enforces referential linkage. Always index the child column!
- **`UNIQUE`**: Candidate key uniqueness. Multiple NULLs allowed unless `NULLS NOT DISTINCT` is specified.
- **`NOT NULL`**: Forbids missing values.
- **`CHECK`**: Enforces deterministic domain expressions and state-machine transitions.

---

## 13. Further Reading
- PostgreSQL 17 Constraints: [Chapter 5.4: Constraints](https://www.postgresql.org/docs/17/ddl-constraints.html)
- PostgreSQL Wiki: [Why Index Foreign Keys](https://wiki.postgresql.org/wiki/Index_maintenance)
- ISO/IEC 9075:2023: Foundation (SQL/Foundation) Integrity Specifications
- [04 - The SELECT Statement & Logical Evaluation Order](./04-select-statement.md)
