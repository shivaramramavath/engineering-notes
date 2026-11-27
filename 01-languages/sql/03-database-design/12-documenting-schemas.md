# Documenting Schemas: Data Dictionaries, COMMENT ON, and Metadata Introspection

| Metadata | Details |
| :--- | :--- |
| **Topic** | Schema Documentation, Data Dictionaries, COMMENT ON, Information Schema Introspection, ER Generation |
| **Dialect** | ANSI SQL:2023 / PostgreSQL 17+ (Primary), MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Database Version** | PostgreSQL 17.2, MySQL 8.4.1, SQLite 3.50.4, SQL Server 2022 CU14 |
| **Tested On** | Python 3.14 stdlib sqlite3 engine & PostgreSQL 17 execution runner |
| **Date** | 2026-11-26 |
| **Author** | Antigravity SQL Curriculum Lead |
| **Category** | Database Design & Schema Architecture |

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

In large software organizations, tribal knowledge is the enemy of engineering velocity. When a database contains 300 tables and 4,000 columns, new engineers and data analysts spend days deciphering obscure business codes:
- *"Does `status = 4` mean cancelled or refunded?"*
- *"Why is `customer_id` null in 2% of these rows?"*
- *"Is `total_amount` inclusive or exclusive of VAT?"*
- *"Which of these three address columns is the active shipping destination?"*

External documentation—such as Confluence wikis, Notion documents, or Google Docs—inevitably suffers from **Documentation Drift**: developers update the physical database schema during a sprint, but forget to update the wiki. Within six months, the external wiki is inaccurate, misleading, and dangerous.

The solution is **Living In-Database Documentation**. By embedding descriptions directly into the database catalog using standard `COMMENT ON` statements, documentation is version-controlled alongside schema DDL. 

Using standard `information_schema` and system catalog introspection, teams can programmatically generate up-to-date data dictionaries, automated Mermaid ER diagrams, and CI/CD drift-detection alerts directly from the live database.

---

## Core Concepts

### 1. The `COMMENT ON` Statement
ANSI SQL and PostgreSQL provide native commands to attach documentation strings directly to database objects:
```sql
COMMENT ON TABLE customers IS 'Primary enterprise customer accounts and billing identities';
COMMENT ON COLUMN customers.tax_id IS 'National tax registration number; must be encrypted at application rest';
COMMENT ON CONSTRAINT chk_orders_positive_total ON orders IS 'Enforces that order totals cannot be negative';
```
These comments are stored physically within the database's internal system catalogs (`pg_description`), persisting across backups (`pg_dump`), replication streams, and schema dumps.

### 2. The `information_schema` Standard
Standardized in ANSI SQL-92, `information_schema` is an ANSI-compliant set of read-only views present in PostgreSQL, MySQL, and SQL Server that exposes structural metadata about tables, columns, constraints, and data types:
- `information_schema.tables`: Catalogs all tables, views, and foreign tables.
- `information_schema.columns`: Catalogs column names, data types, nullability, and default expressions.
- `information_schema.table_constraints`: Catalogs Primary Key, Foreign Key, Unique, and Check constraints.
- `information_schema.referential_constraints`: Catalogs foreign key relationships and cascade rules.

### 3. PostgreSQL Native System Catalogs
While `information_schema` is portable, it is often slow because it enforces complex permission filtering. PostgreSQL's internal `pg_catalog` provides instant, low-level introspection:
- `pg_class`: Tables, indexes, sequences, views (`relname`, `relkind`).
- `pg_attribute`: Column metadata (`attname`, `atttypid`, `attnotnull`).
- `pg_description`: Stores all comments attached to database objects.
- Functions: `obj_description(oid, 'pg_class')` and `col_description(table_oid, col_number)`.

### 4. Automated Data Dictionaries & ER Generation
Because all metadata lives in the catalog, automated CI/CD tools can query the database to generate:
- **Interactive Web Portals**: SchemaSpy, dbdocs.io, tbls.
- **Visual Diagrams**: Automated extraction of Foreign Keys into Mermaid or PlantUML ER diagrams.
- **Drift Detection**: Comparing staging vs production catalogs using tools like `migra`.

---

## How It Works Internally

How does the database engine store and retrieve schema comments?

```
      [ DDL: COMMENT ON COLUMN customers.email IS 'User primary login email' ]
                                          │
                                          ▼
                             [ PostgreSQL Catalog Engine ]
                                          │
        1. Resolves 'customers' table OID in pg_class (e.g., OID = 16384).
        2. Resolves 'email' column number in pg_attribute (attnum = 2).
        3. Writes comment record to pg_description:
           ┌──────────┬──────────┬─────────┬───────────────────────────────┐
           │ objoid   │ classoid │ objsubid│ description                   │
           ├──────────┼──────────┼─────────┼───────────────────────────────┤
           │ 16384    │ 1259     │ 2       │ User primary login email      │
           └──────────┴──────────┴─────────┴───────────────────────────────┘
                                          │
                                          ▼
     [ What happens when running \d+ customers in psql? ]
     psql calls col_description(16384, 2) and renders comment column automatically!
```

- Schema comments consume zero storage on table data pages. They reside in a small system catalog table (`pg_description`), incurring **zero runtime performance overhead** on standard transactional queries.

---

## Syntax & Usage

### 1. Attaching Living Documentation in DDL
```sql
CREATE TABLE payment_transactions (
    transaction_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    merchant_id BIGINT NOT NULL,
    amount NUMERIC(10,2) NOT NULL,
    currency CHAR(3) NOT NULL,
    payment_status VARCHAR(20) NOT NULL
);

-- Table Comment
COMMENT ON TABLE payment_transactions IS 
'Records real-time credit card and ACH settled payments processed via Stripe and Adyen.';

-- Column Comments
COMMENT ON COLUMN payment_transactions.transaction_id IS 
'Internal 64-bit monotonically increasing surrogate key.';

COMMENT ON COLUMN payment_transactions.amount IS 
'Gross transaction amount in major currency units (e.g., USD dollars, EUR euros).';

COMMENT ON COLUMN payment_transactions.currency IS 
'ISO-4217 three-letter currency code (e.g., USD, EUR, GBP).';

COMMENT ON COLUMN payment_transactions.payment_status IS 
'State machine status: PENDING, SETTLED, DECLINED, or REFUNDED.';
```

### 2. Introspecting Comments via `information_schema` and `pg_description`
```sql
-- Querying a complete living Data Dictionary for table 'payment_transactions':
SELECT 
    cols.ordinal_position,
    cols.column_name,
    cols.data_type,
    cols.is_nullable,
    cols.column_default,
    col_description(c.oid, cols.ordinal_position) AS business_description
FROM information_schema.columns cols
JOIN pg_class c ON c.relname = cols.table_name
JOIN pg_namespace n ON n.oid = c.relnamespace AND n.nspname = cols.table_schema
WHERE cols.table_name = 'payment_transactions'
  AND cols.table_schema = 'public'
ORDER BY cols.ordinal_position;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **Comment DDL Syntax** | Standalone: `COMMENT ON <obj> IS '...'` | Inline: `col INT COMMENT '...'` | **Not Supported** (Must parse SQL text) | Proprietary: `sp_addextendedproperty` |
| **`information_schema`**| Supported (ANSI compliant) | Supported | **Not Supported** (Use `sqlite_master` / PRAGMA) | Supported |
| **Catalog Functions** | `col_description()`, `obj_description()` | Inline in `SHOW FULL COLUMNS` | `PRAGMA table_info(tbl)` | `sys.extended_properties` |
| **Backup Persistence** | Preserved in `pg_dump` | Preserved in `mysqldump` | N/A | Preserved in `.bak` files |

---

## Practical Examples

### Example 1: Basic — Programmatically Generating a Markdown Data Dictionary
Extracting table and column metadata into clean GitHub Flavored Markdown directly from SQL.

```sql
SELECT 
    '| `' || column_name || '` | `' || 
    data_type || '` | ' || 
    is_nullable || ' | ' || 
    COALESCE(col_description(c.oid, ordinal_position), '*No description provided*') || ' |' AS markdown_row
FROM information_schema.columns cols
JOIN pg_class c ON c.relname = cols.table_name
WHERE cols.table_name = 'payment_transactions'
  AND cols.table_schema = 'public'
ORDER BY ordinal_position;
```

```
Resulting Markdown Output:
| `transaction_id` | `bigint` | NO | Internal 64-bit monotonically increasing surrogate key. |
| `merchant_id` | `bigint` | NO | *No description provided* |
| `amount` | `numeric` | NO | Gross transaction amount in major currency units. |
| `currency` | `character` | NO | ISO-4217 three-letter currency code. |
| `payment_status` | `character varying` | NO | State machine status: PENDING, SETTLED, DECLINED, or REFUNDED. |
```

---

### Example 2: Real-World — Automated Mermaid ER Diagram Extraction Query
A SQL query that reads foreign key constraints from the system catalog and outputs a valid Mermaid ER diagram syntax!

```sql
SELECT 
    '    ' || tc.table_name || ' }o--|| ' || ccu.table_name || ' : "' || kcu.column_name || '"' AS mermaid_relationship
FROM information_schema.table_constraints tc
JOIN information_schema.key_column_usage kcu 
    ON tc.constraint_name = kcu.constraint_name 
   AND tc.table_schema = kcu.table_schema
JOIN information_schema.constraint_column_usage ccu 
    ON ccu.constraint_name = tc.constraint_name 
   AND ccu.table_schema = tc.table_schema
WHERE tc.constraint_type = 'FOREIGN KEY'
  AND tc.table_schema = 'public';
```

```
Generated Output:
    orders }o--|| customers : "customer_id"
    order_items }o--|| orders : "order_id"
    order_items }o--|| products : "product_id"
```
*Takeaway*: Tying this query into a CI/CD script automatically regenerates and commits the repository's architecture diagrams on every schema migration!

---

### Example 3: Edge Case — Auditing Undocumented Columns
An automated compliance audit query that fails CI/CD if any production column lacks a descriptive comment.

```sql
SELECT 
    cols.table_name,
    cols.column_name,
    cols.data_type
FROM information_schema.columns cols
JOIN pg_class c ON c.relname = cols.table_name
JOIN pg_namespace n ON n.oid = c.relnamespace AND n.nspname = cols.table_schema
WHERE cols.table_schema = 'public'
  AND c.relkind = 'r' -- Normal physical tables only
  AND col_description(c.oid, cols.ordinal_position) IS NULL
ORDER BY cols.table_name, cols.ordinal_position;
```

```
Result (Flagged Deficiencies):
     table_name      | column_name | data_type 
---------------------+-------------+-----------
 payment_transactions| merchant_id | bigint
(1 row found - Missing documentation!)
```

---

### Example 4: Failure Mode — The Wiki Drift Disaster
A software team maintained database documentation in an external Notion wiki.
```
Year 1: Notion wiki states: 'orders.status can be 1 (Pending), 2 (Shipped), 3 (Cancelled)'.
Year 2: Database team migrates to strings: 'PENDING', 'SHIPPED', 'CANCELLED', 'REFUNDED'.
        Developer does not update Notion wiki.
Year 3: A new data scientist writes ETL queries filtering WHERE status = '1'.
        ETL quietly processes ZERO rows for 6 months! Executive financial metrics corrupted!

REMEDY: Keep documentation IN THE DATABASE using COMMENT ON. The database is the single source of truth.
```

---

### Example 5: Optimized — Microsoft SQL Server Extended Properties
Attaching and querying metadata documentation in Microsoft SQL Server 2022.

```sql
-- SQL Server 2022 Syntax:
-- Add documentation via sp_addextendedproperty:
EXEC sp_addextendedproperty 
    @name = N'MS_Description', 
    @value = N'Primary enterprise customer account entity', 
    @level0type = N'Schema', @level0name = N'dbo', 
    @level1type = N'Table',  @level1name = N'Customers';

-- Read documentation back:
SELECT 
    objname AS table_name,
    value AS table_description
FROM fn_listextendedproperty(
    N'MS_Description', 
    'schema', 'dbo', 
    'table', 'Customers', 
    NULL, NULL
);
```

---

## Best Practices

1. **Document in Migration Scripts**: Never separate documentation from DDL. Every `CREATE TABLE` and `ALTER TABLE ADD COLUMN` statement in a migration script must include a companion `COMMENT ON` statement.
2. **Document the "Why" and Units of Measure**: Always document currency scales, metric units (e.g., `'latency_ms in milliseconds'`, `'distance in meters'`), and state machine transitions.
3. **Automate ER Diagram Generation in CI/CD**: Integrate tools like `tbls` or custom metadata queries into GitHub Actions to regenerate `ARCHITECTURE.md` diagrams automatically when migrations merge.
4. **Enforce Documentation Audits in Pull Requests**: Add an automated linter or SQL test verifying that no new column can be deployed without a non-empty `COMMENT ON`.
5. **Use System Catalogs for High-Speed Introspection**: In high-frequency introspection tools, query PostgreSQL's `pg_catalog` directly rather than `information_schema` to avoid view evaluation overhead.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Maintaining documentation in an external wiki | Inevitably drifts out of date as schema evolves, misleading engineers. | Use `COMMENT ON` directly in database DDL. |
| 2 | Vague / redundant comments | Comments like `-- customer_id: the customer ID` add zero cognitive value. | Document business invariants, relationships, and units. |
| 3 | Forgetting units of measurement | Unspecified time/weight columns (`timeout`, `weight`) lead to seconds-vs-ms bugs. | Explicitly document units (`timeout_seconds`, `weight_grams`). |
| 4 | Attempting to use `COMMENT ON` in SQLite | SQLite does not support the ANSI `COMMENT ON` syntax. | Document schemas in SQL file comments or shadow metadata tables. |
| 5 | Relying on `information_schema` in tight loops | `information_schema` views perform heavy permission checks and can be slow. | Query `pg_catalog` directly for high-speed scripting. |
| 6 | Dropping tables without tracking dependents | Dropping a table referenced by undocumented views causes cascade failures. | Query `information_schema.view_table_usage` prior to DDL. |

---

## Performance & Security Notes

### Security: Redacting Sensitive PII in Data Dictionaries
While data dictionaries are invaluable for developers, they can also expose sensitive architectural information to unauthorized users:
- **PII / Compliance Tagging**: Use comments to tag data sensitivity for automated GDPR compliance tools:
  ```sql
  COMMENT ON COLUMN users.ssn IS 'TAG:PII; RESTRICTED; Compliance=HIPAA';
  ```
- Automated security scanning tools can query `pg_description` to identify all columns containing the `TAG:PII` token and automatically verify that column encryption or masking is active!

---

## Comparison Matrix

| Documentation Strategy | Single Source of Truth? | Version Controlled? | Zero-Drift Guarantee? | Tooling Integration |
| :--- | :--- | :--- | :--- | :--- |
| **In-Database (`COMMENT ON`)**| **Yes (Database catalog)**| **Yes (Inside DDL)**| **100% Yes** | Native (`\d+`, SchemaSpy, tbls) |
| **External Wiki (Notion/Confluence)**| No (Secondary copy) | No (Separate UI) | **0% NO (High drift)** | Poor (Manual copying) |
| **ORM Code Docstrings** | No (Application tier) | Yes (In Git) | Moderate | ORM-dependent only |
| **Markdown in Repo** | Moderate (Git repo) | Yes (In Git) | Moderate (Requires CI check)| High (GitHub preview) |

---

## Interview Questions & Deep Dives

### Q1: What is the `COMMENT ON` statement and where is its metadata stored in PostgreSQL?
<details>
<summary>Click to view answer</summary>

`COMMENT ON <object_type> <object_name> IS '...'` is an ANSI-standard DDL statement that attaches human-readable documentation directly to database objects (tables, columns, indexes, constraints, types, schemas). 

In PostgreSQL, comments are physically stored in the **`pg_description`** system catalog table. Each entry records the object OID (`objoid`), the catalog table OID (`classoid`), the column sub-index (`objsubid`, where 0 represents the table itself and $>0$ represents column numbers), and the description text.
</details>

### Q2: What is the `information_schema` and why is it standardized across relational engines?
<details>
<summary>Click to view answer</summary>

The `information_schema` is an ANSI/ISO SQL-standard collection of views that provides standardized, vendor-neutral metadata introspection across relational database systems. By querying `information_schema.tables` and `information_schema.columns`, database tools, ORMs, and migration frameworks (like Flyway, Liquibase, and Hibernate) can discover database schemas, primary keys, and data types using identical SQL queries regardless of whether the underlying engine is PostgreSQL, MySQL, or SQL Server.
</details>

### Q3: Why is querying `information_schema` significantly slower than querying `pg_catalog` in PostgreSQL?
<details>
<summary>Click to view answer</summary>

The views in `information_schema` are designed for strict ANSI standard conformance and portability, not raw speed. Under the hood, each `information_schema` view executes complex joins across multiple internal catalogs, applies string conversions, and enforces granular SQL access privilege checks (`HAS_TABLE_PRIVILEGE`, `HAS_COLUMN_PRIVILEGE`) for every single row. 

PostgreSQL's internal `pg_catalog` tables (such as `pg_class`, `pg_attribute`) contain raw binary data without security filtering overhead, executing orders of magnitude faster.
</details>

### Q4: How can in-database comments be used for automated GDPR compliance audits?
<details>
<summary>Click to view answer</summary>

Security teams establish structured metadata conventions within column comments (e.g., `COMMENT ON COLUMN users.phone_number IS 'CLASSIFICATION:CONFIDENTIAL; PII:TRUE; RETENTION:3Y'`). 

Automated security scanners query `pg_description` during CI/CD to inspect all columns tagged with `PII:TRUE`. The scanner automatically verifies that those specific columns are included in data anonymization scripts, data export pipelines, and backup retention deletion jobs, eliminating manual compliance audits.
</details>

### Q5: What built-in functions does PostgreSQL provide to retrieve object and column comments?
<details>
<summary>Click to view answer</summary>

PostgreSQL provides specialized catalog lookup functions:
- `col_description(table_oid, column_number)`: Returns the comment for a specific column.
- `obj_description(object_oid, catalog_name)`: Returns the comment for a database-level object (e.g., `obj_description(16384, 'pg_class')` for a table).
- `shobj_description(object_oid, catalog_name)`: Returns comments for shared cluster objects (databases, roles, tablespaces).
</details>

### Q6: How does SQLite support schema documentation given that it lacks `COMMENT ON`?
<details>
<summary>Click to view answer</summary>

SQLite does not support `COMMENT ON` or `sp_addextendedproperty`. However, SQLite stores the literal `CREATE TABLE` DDL statement used to construct each table in the **`sqlite_master`** (or `sqlite_schema`) system table. Engineers document SQLite schemas using inline SQL comments (`-- comment`) inside the DDL; introspection tools parse the raw SQL text from `sqlite_master.sql` to extract column and table documentation.
</details>

### Q7: How can you find all Foreign Keys referencing a specific table in PostgreSQL?
<details>
<summary>Click to view answer</summary>

By querying `information_schema.referential_constraints` joined to `information_schema.table_constraints`:
```sql
SELECT 
    kcu.table_name AS child_table,
    kcu.column_name AS child_foreign_key_column,
    tc.constraint_name
FROM information_schema.table_constraints tc
JOIN information_schema.key_column_usage kcu ON tc.constraint_name = kcu.constraint_name
JOIN information_schema.constraint_column_usage ccu ON ccu.constraint_name = tc.constraint_name
WHERE tc.constraint_type = 'FOREIGN KEY'
  AND ccu.table_name = 'customers';
```
</details>

### Q8: What happens to column comments when a column is renamed or its type is altered?
<details>
<summary>Click to view answer</summary>

Because comments in PostgreSQL are linked by table OID (`objoid`) and physical column position (`objsubid`), **renaming a column or altering its data type preserves the comment perfectly**! The comment remains attached to the column slot in `pg_description`. Only executing `ALTER TABLE ... DROP COLUMN` deletes the associated comment record from `pg_description`.
</details>

---

## Practice Exercises

### Exercise 1: Attach Comprehensive Table and Column Documentation
Write the complete DDL to create `warehouse_locations(location_id, zone_code, shelf_number, max_weight_kg)` and attach explicit business comments explaining units of measure and zone naming conventions.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE warehouse_locations (
    location_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    zone_code VARCHAR(10) NOT NULL,
    shelf_number INT NOT NULL,
    max_weight_kg NUMERIC(8,2) NOT NULL CHECK (max_weight_kg > 0)
);

COMMENT ON TABLE warehouse_locations IS 
'Physical storage bins and shelf positions within regional fulfillment centers.';

COMMENT ON COLUMN warehouse_locations.zone_code IS 
'Building sector code (e.g., ZONE-A, COLD-STORAGE, HAZMAT-1).';

COMMENT ON COLUMN warehouse_locations.shelf_number IS 
'Vertical shelf tier index (1 = Ground level, higher numbers indicate elevated tiers).';

COMMENT ON COLUMN warehouse_locations.max_weight_kg IS 
'Maximum safe structural load limit in kilograms. Enforced by forklift routing algorithms.';
```
</details>

---

### Exercise 2: Introspect Table Constraints Query
Write an SQL query against `information_schema` that returns all constraints defined on table `warehouse_locations`, displaying the constraint name and constraint type.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    constraint_name,
    constraint_type
FROM information_schema.table_constraints
WHERE table_name = 'warehouse_locations'
  AND table_schema = 'public'
ORDER BY constraint_type, constraint_name;
```
</details>

---

### Exercise 3: Read Column Description via Native PostgreSQL Catalog
Write a query using PostgreSQL's `col_description()` function to retrieve the comment attached to column `max_weight_kg` on `warehouse_locations`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT col_description('warehouse_locations'::regclass, attnum) AS column_doc
FROM pg_attribute
WHERE attrelid = 'warehouse_locations'::regclass
  AND attname = 'max_weight_kg';
```
</details>

---

### Exercise 4: Query Tables Lacking Primary Keys
Write a database hygiene query that inspects `information_schema` and returns all physical base tables in the `public` schema that do **not** possess a Primary Key constraint.

<details>
<summary>Click to view solution</summary>

```sql
SELECT t.table_name
FROM information_schema.tables t
WHERE t.table_schema = 'public'
  AND t.table_type = 'BASE TABLE'
  AND NOT EXISTS (
      SELECT 1 
      FROM information_schema.table_constraints tc
      WHERE tc.table_schema = t.table_schema
        AND tc.table_name = t.table_name
        AND tc.constraint_type = 'PRIMARY KEY'
  );
```
</details>

---

### Exercise 5: CI/CD Documentation Enforcement Test
Write a query that counts how many columns in table `warehouse_locations` lack a descriptive comment. (In a CI/CD test, this query asserting `= 0` ensures 100% documentation coverage).

<details>
<summary>Click to view solution</summary>

```sql
SELECT COUNT(*) AS undocumented_column_count
FROM information_schema.columns cols
JOIN pg_class c ON c.relname = cols.table_name
WHERE cols.table_name = 'warehouse_locations'
  AND cols.table_schema = 'public'
  AND col_description(c.oid, cols.ordinal_position) IS NULL;
```
</details>

---

### Exercise 6: Fixing the Lost Documentation Bug
A team runs `CREATE TABLE new_users AS SELECT * FROM users;` during a migration and notices that all column comments, primary keys, and foreign keys have disappeared. Explain why, and provide the correct command to duplicate table structure with comments.

<details>
<summary>Click to view solution</summary>

**Cause**: `CREATE TABLE AS SELECT` (CTAS) only copies the column names and inferred data types of the result set. It deliberately **does not copy** primary keys, foreign keys, unique constraints, default expressions, or comments from the system catalog.

**Fix**: Use `CREATE TABLE ... (LIKE ... INCLUDING ALL)`:
```sql
CREATE TABLE new_users (
    LIKE users INCLUDING DEFAULTS INCLUDING CONSTRAINTS INCLUDING COMMENTS INCLUDING INDEXES
);
```
This instructs the database engine to clone the complete metadata specification, including all `COMMENT ON` documentation.
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                               DOCUMENTING SCHEMAS CHEAT SHEET                           |
+-------------------+----------------------------+----------------------------------------+
| Goal              | SQL Statement              | Catalog Location                       |
+-------------------+----------------------------+----------------------------------------+
| Document Table    | COMMENT ON TABLE t IS '...'| pg_description (objsubid = 0)          |
| Document Column   | COMMENT ON COLUMN t.c IS ''| pg_description (objsubid = col_num)    |
| Read Column Doc   | col_description(oid, num)  | Resolves via pg_class / pg_attribute   |
| Portable Schema   | information_schema.columns | Standard ANSI introspection views      |
| Quick CLI Inspect | \d+ table_name in psql     | Renders comments in CLI table view     |
| Clone with Docs   | LIKE t INCLUDING COMMENTS  | Clones structure + metadata            |
+-------------------+----------------------------+----------------------------------------+
```

- **In-Database Mandate**: Keep documentation in DDL via `COMMENT ON`; avoid wiki drift.
- **CI/CD Hygiene**: Assert zero undocumented columns in pull request test pipelines.
- **Compliance**: Use structured comments (`TAG:PII`) to automate GDPR/HIPAA audits.

---

## Further Reading
- PostgreSQL 17 Documentation: *COMMENT ON & System Catalog Information Functions*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-11) *Information and Definition Schemas (SQL/Schemata)*
- SchemaSpy: *Automated Database Metadata Introspection and Visual Documentation*
- tbls: *CI-friendly Tool for Documenting Databases in Markdown*
