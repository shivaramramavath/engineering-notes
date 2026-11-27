# Designing for Schema Evolution: Compatibility, Widening, and Deprecation

| Metadata | Details |
| :--- | :--- |
| **Topic** | Schema Evolution, Backward/Forward Compatibility, Column Widening, Table Splitting, Deprecation Pipelines |
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

No database schema remains static. As a software product grows, business models shift, features are added, and data volumes multiply by orders of magnitude. A schema designed for 10,000 users in Year 1 will fail under the load of 10,000,000 users in Year 3.

However, evolving a production database schema is fundamentally different from refactoring application code:
- **Application Code**: Can be refactored, recompiled, and deployed in isolated containers.
- **Database Schema**: Must maintain backwards compatibility with existing deployed code while transitioning to new structures, preserving millions of historical records with zero data loss and zero downtime.

Designing for evolution means anticipating change from Day 1:
- Understanding which schema alterations are **additive and safe** versus which are **destructive and breaking**.
- Knowing which column type widenings (e.g., `VARCHAR(50)` to `VARCHAR(100)`) are instantaneous metadata updates, and which (e.g., `INTEGER` to `BIGINT`) force a catastrophic table rewrite.
- Implementing graceful **deprecation pipelines** using anti-corruption views and usage tracking (`pg_stat_statements`) before dropping legacy structures.

Mastering schema evolution principles guarantees that relational architectures grow smoothly alongside business requirements.

---

## Core Concepts

### 1. The Three Evolution Categories
1. **Additive Changes (Zero-Risk)**:
   - Creating a new table.
   - Adding a nullable column or a column with a constant default (PostgreSQL 11+).
   - Adding a non-unique index concurrently.
   - Creating a new database view or function.
   - *Compatibility*: 100% backward and forward compatible. Existing application code continues running uninterrupted.
2. **Subtractive Changes (High-Risk / Breaking)**:
   - Dropping a column or table.
   - Renaming a column or table.
   - Restricting a constraint (e.g., changing `NULL` to `NOT NULL`).
   - *Compatibility*: Immediately breaks any running application instance that expects the old structure. Requires the multi-phase Expand/Contract pattern.
3. **Mutative Changes (Variable-Risk)**:
   - Changing column data types (e.g., widening text or numeric types).
   - Splitting a table into multiple tables (Vertical Partitioning).

### 2. The Robustness Principle (Postel's Law for Databases)
> *"Be conservative in what you send, be liberal in what you accept."*
- **Application Rule (Tolerant Reader)**: Application code should never execute `SELECT *`. It should request only the explicit columns it understands, ignoring new or unselected attributes returned by the schema.
- **Database Rule (Tolerant Writer)**: New columns should be introduced as nullable or with sensible defaults so that legacy application versions can continue inserting rows without knowing about the new columns.

### 3. Column Type Widening Mechanics
- **Safe Metadata Widening**:
  - `VARCHAR(50)` to `VARCHAR(100)` in PostgreSQL 9.1+: Modifies the length check in `pg_attribute` catalog in $O(1)$ time without touching table data on disk!
  - `VARCHAR(n)` to `TEXT`: Instant metadata update.
- **Dangerous Table-Rewriting Widening**:
  - `INTEGER` (4 bytes) to `BIGINT` (8 bytes): Requires allocating 4 extra physical bytes for every single row on disk. PostgreSQL must rewrite every page of the table, taking an `ACCESS EXCLUSIVE` lock.
  - `NUMERIC(10,2)` to `NUMERIC(15,2)`: Requires rewriting physical numeric headers.

### 4. Database Views as Anti-Corruption Layers
When refactoring legacy tables (e.g., splitting `users` into `user_accounts` and `user_profiles`):
- Rather than forcing all application services to rewrite their queries simultaneously, create a **Database View** bearing the legacy table's original name.
- The view joins the new split tables together, presenting the exact legacy interface to older microservices while new services query the split tables directly.

---

## How It Works Internally

How does PostgreSQL differentiate between a zero-cost metadata type change and a full table rewrite?

```
      [ DDL: ALTER TABLE accounts ALTER COLUMN balance TYPE NUMERIC(15,2); ]
                                          │
                                          ▼
                       [ PostgreSQL Catalog Inspection ]
                                          │
        Does the new type have binary coercion compatibility with the old type?
                                          │
                  ┌───────────────────────┴───────────────────────┐
                  ▼                                               ▼
     [ YES: Binary Coercible ]                       [ NO: Format Incompatible ]
     (e.g., VARCHAR(50) -> VARCHAR(100))             (e.g., INT -> BIGINT, or NUMERIC change)
                  │                                               │
     1. Brief ACCESS EXCLUSIVE lock.                 1. Long ACCESS EXCLUSIVE lock.
     2. Updates pg_attribute.atttypmod.              2. Allocates new physical table file.
     3. No disk pages touched!                       3. Rewrites every row with new 8-byte layout!
     4. Execution: < 2 milliseconds!                 4. Execution: Hours on a 100GB table!
```

- When PostgreSQL evaluates an `ALTER TABLE ... ALTER COLUMN TYPE` command, it checks the internal function `find_coercion_pathway()`.
- If the conversion requires a binary change to the data layout (such as converting 32-bit integer binary to 64-bit integer binary), an in-place metadata update is impossible, forcing an exclusive table rewrite.

---

## Syntax & Usage

### 1. Instant Safe Column Widening (PostgreSQL)
```sql
-- Instant metadata-only update (sub-millisecond execution!):
ALTER TABLE customer_profiles 
ALTER COLUMN phone_number TYPE VARCHAR(50);

-- Widening to unlimited TEXT is also an instant metadata update:
ALTER TABLE customer_profiles 
ALTER COLUMN postal_code TYPE TEXT;
```

### 2. View-Based Anti-Corruption Layer for Table Splitting
```sql
-- Step 1: Split physical table into core accounts and cold profile data
CREATE TABLE user_accounts (
    user_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    email VARCHAR(255) NOT NULL UNIQUE
);

CREATE TABLE user_profiles (
    user_id BIGINT PRIMARY KEY REFERENCES user_accounts(user_id) ON DELETE CASCADE,
    bio TEXT,
    avatar_url TEXT
);

-- Step 2: Create a backward-compatible view with the legacy table name:
CREATE VIEW users AS
SELECT 
    a.user_id,
    a.email,
    p.bio,
    p.avatar_url
FROM user_accounts a
LEFT JOIN user_profiles p ON a.user_id = p.user_id;

-- Step 3: Enable seamless writes to the view via INSTEAD OF triggers:
CREATE OR REPLACE FUNCTION trg_users_view_insert()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO user_accounts (user_id, email)
    VALUES (NEW.user_id, NEW.email);
    
    INSERT INTO user_profiles (user_id, bio, avatar_url)
    VALUES (NEW.user_id, NEW.bio, NEW.avatar_url);
    
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_insert_users_view
INSTEAD OF INSERT ON users
FOR EACH ROW EXECUTE FUNCTION trg_users_view_insert();
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **`VARCHAR` Widening** | Instant metadata update | Instant metadata update (`ALGORITHM=INPLACE`)| Dynamic typing (Instant) | Metadata update (if variable length) |
| **`INT` to `BIGINT`** | **Full table rewrite** | **Full table copy** | Dynamic typing (Instant) | **Full table rewrite** |
| **`INSTEAD OF` Triggers on Views**| Fully supported | **Not Supported** | Supported | Fully supported |
| **Dropping Column Overhead**| Instant metadata mark (`attisdropped`)| Instant (MySQL 8.0.29+) | Table copy (prior to 3.35) | Metadata update |

---

## Practical Examples

### Example 1: Basic — The 4-Phase Deprecation Pipeline
Safely deprecating a legacy column `fax_number` without breaking active application instances.

```sql
-- PHASE 1: Annotation
-- Add schema documentation warning developers that the column is scheduled for removal:
COMMENT ON COLUMN organizations.fax_number IS 'DEPRECATED [Target Drop: 2027-01-01]. Use communication_channels table instead.';

-- PHASE 2: Audit Usage
-- Inspect application telemetry or pg_stat_statements to verify zero queries reference fax_number.

-- PHASE 3: Revoke Access (Soft Deprecation)
-- In a staging environment, make the column invisible or read-only to verify no microservices break:
-- (e.g., update view or security grant).

-- PHASE 4: Hard Removal (Final Migration)
ALTER TABLE organizations DROP COLUMN fax_number;
```

---

### Example 2: Real-World — Zero-Downtime INT to BIGINT Migration
Migrating `orders.order_id` from a 32-bit `INTEGER` to a 64-bit `BIGINT` on a 200GB production table without table-locking downtime.

#### Step 1: Add new 64-bit column & synchronization trigger
```sql
ALTER TABLE orders ADD COLUMN new_order_id BIGINT NULL;

CREATE OR REPLACE FUNCTION sync_order_id()
RETURNS TRIGGER AS $$
BEGIN
    NEW.new_order_id = COALESCE(NEW.new_order_id, NEW.order_id);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_sync_order_id
BEFORE INSERT ON orders
FOR EACH ROW EXECUTE FUNCTION sync_order_id();
```

#### Step 2: Backfill historical rows in throttled chunks
```sql
-- Backfill in small batches based on ID ranges:
UPDATE orders SET new_order_id = order_id WHERE new_order_id IS NULL AND order_id BETWEEN 1 AND 50000;
-- Loop until all historical rows are populated.
```

#### Step 3: Add constraints concurrently
```sql
CREATE UNIQUE INDEX CONCURRENTLY idx_orders_new_id ON orders (new_order_id);
ALTER TABLE orders ADD CONSTRAINT chk_new_id_not_null CHECK (new_order_id IS NOT NULL) NOT VALID;
ALTER TABLE orders VALIDATE CONSTRAINT chk_new_id_not_null;
```

#### Step 4: Swap columns in a brief transaction
```sql
BEGIN;
SET LOCAL lock_timeout = '2s';
-- Drop trigger
DROP TRIGGER trg_sync_order_id ON orders;
DROP FUNCTION sync_order_id();

-- Swap column names
ALTER TABLE orders RENAME COLUMN order_id TO old_order_id;
ALTER TABLE orders RENAME COLUMN new_order_id TO order_id;

-- Re-point primary key
ALTER TABLE orders DROP CONSTRAINT orders_pkey;
ALTER TABLE orders ADD CONSTRAINT orders_pkey PRIMARY KEY USING INDEX idx_orders_new_id;
COMMIT;
```

---

### Example 3: Edge Case — Vertical Partitioning for Buffer Pool Optimization
A `products` table contains high-frequency catalog data (`title`, `price`) and low-frequency massive data (`detailed_specs_markdown`, `high_res_image_bytes`). Splitting them preserves buffer pool memory.

```sql
-- Hot Table: 64 bytes per row. 100,000 rows fit in ~6 MB of RAM!
CREATE TABLE products_hot (
    product_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    title VARCHAR(150) NOT NULL,
    current_price NUMERIC(10,2) NOT NULL,
    is_active BOOLEAN NOT NULL DEFAULT TRUE
);

-- Cold Table: 4,000 bytes per row. Stored on separate cold storage pages.
CREATE TABLE products_cold (
    product_id BIGINT PRIMARY KEY REFERENCES products_hot(product_id) ON DELETE CASCADE,
    specs_markdown TEXT NOT NULL,
    image_blob BYTEA NULL
);
```

---

### Example 4: Failure Mode — Direct Type Conversion Outage
A developer executes `ALTER TABLE transactions ALTER COLUMN amount TYPE NUMERIC(15,2);` directly on a 50-million row table.

```sql
-- FATAL FAILURE:
-- 1. PostgreSQL acquires an ACCESS EXCLUSIVE lock on 'transactions'.
-- 2. It discovers NUMERIC(15,2) has a different physical header format than the previous type.
-- 3. The engine begins rewriting all 50,000,000 rows into a new physical table file.
-- 4. Process takes 45 minutes.
-- 5. All production checkout transactions queue up and fail with connection timeouts!
-- REMEDY: Use the Expand/Contract dual-column migration pattern for incompatible types!
```

---

### Example 5: Optimized — Expanding Enum Values Safely in PostgreSQL
Adding new values to a native PostgreSQL `ENUM` type without table-locking rewrites.

```sql
CREATE TYPE shipment_status AS ENUM ('ORDERED', 'PROCESSING', 'SHIPPED');

-- In PostgreSQL 12+, adding a value to an ENUM is a fast metadata operation:
ALTER TYPE shipment_status ADD VALUE 'DELIVERED';

-- Adding a value BEFORE another value:
ALTER TYPE shipment_status ADD VALUE 'ON_HOLD' BEFORE 'SHIPPED';
-- Note: While ADD VALUE is fast, renaming or removing values requires a full type rebuild!
```

---

## Best Practices

1. **Verify Binary Coercibility Before Altering Types**: Check whether a column type modification is metadata-only or requires a full table rewrite. If a rewrite is required, use the Expand/Contract pattern.
2. **Never Execute `SELECT *` in Production Code**: Always enumerate explicit columns. This allows database teams to add new columns without breaking application deserializers.
3. **Use Views as Transitional Anti-Corruption Layers**: When renaming or splitting tables, preserve the old interface as a view to decouple database deployment from application microservice deployments.
4. **Audit Column Usage with `pg_stat_statements`**: Before dropping a deprecated column, inspect query statistics to verify that zero production queries are accessing it.
5. **Keep Hot Columns Separate from Massive Cold Blobs**: Vertically partition tables so that wide, rarely read text/blob columns do not pollute the buffer pool cache of high-frequency transactional data.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Direct `INT` to `BIGINT` conversion | Takes an exclusive lock and rewrites the entire table on disk. | Use multi-phase Expand/Contract dual-column migration. |
| 2 | Immediate column drop | Breaks running application servers during rolling canary deployments. | Deprecate in code first, verify zero reads, drop in later sprint. |
| 3 | Writing `SELECT *` in application queries | Adding a new column can cause ORM tuple-mapping crashes or network bloat. | Explicitly project only required columns. |
| 4 | Attempting to drop native ENUM values | PostgreSQL does not support `ALTER TYPE ... DROP VALUE`. | Use `VARCHAR` with `CHECK` constraints instead of native enums. |
| 5 | Forgetting to drop sync triggers after migration | Leaves background triggers running indefinitely, slowing down every future insert. | Clean up sync triggers during the Contract phase. |
| 6 | Unindexed foreign keys in split tables | Vertically partitioning a table into 2 tables requires frequent joins; unindexed FKs destroy latency. | Index the 1:1 foreign key in the child table. |

---

## Performance & Security Notes

### Buffer Cache Efficiency via Vertical Partitioning
Consider an e-commerce catalog of 1,000,000 products:
- **Monolithic Table**: Stores product title, price, inventory count, and a 5KB technical specification text blob.
  - Average row width: ~5,200 bytes.
  - Rows per 8KB page: ~1 row.
  - Table size on disk: ~5.2 GB.
  - Buffer pool cache hit ratio: Low (Scanning 10,000 products reads 50 MB into RAM).
- **Vertically Partitioned (Hot vs Cold)**:
  - Hot Table (`title`, `price`, `stock`): Average row width 60 bytes. Rows per page: ~130. Total size: ~60 MB!
  - 100% of the entire catalog's transactional search data fits permanently in a tiny 60 MB RAM buffer. Reads are instantaneous!
  - The 5GB of technical specification text is stored in the cold table and read only on the single-item product detail page.

---

## Comparison Matrix

| Schema Evolution Technique | Migration Complexity | Risk Level | Downtime | Recommended Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Instant Metadata Alter** | Lowest (1 script) | Low | **Zero** | `VARCHAR` widening, nullable column addition |
| **Expand / Contract** | High (3 phases, 2 deploys)| Low | **Zero** | Column rename, `INT` to `BIGINT`, table split |
| **Direct Table Rewrite** | Lowest (1 command) | **EXTREME** | **Hours** | Forbidden in production OLTP systems |
| **Anti-Corruption View** | Moderate (View + Trigger) | Low | **Zero** | Major table restructuring / microservice split |

---

## Interview Questions & Deep Dives

### Q1: Why does changing a column from `VARCHAR(50)` to `VARCHAR(100)` execute instantly, while changing `INTEGER` to `BIGINT` can take hours?
<details>
<summary>Click to view answer</summary>

- `VARCHAR(50)` and `VARCHAR(100)` use the exact same underlying binary storage structure (`varlena`). The length limit is not part of the physical data on disk; it is merely a constraint stored in the `atttypmod` column of the `pg_attribute` catalog. PostgreSQL simply updates this integer in the system catalog ($O(1)$ metadata update).
- `INTEGER` (4-byte fixed binary) and `BIGINT` (8-byte fixed binary) have fundamentally incompatible physical storage layouts. To change an integer to a bigint, the database engine must allocate 4 additional physical bytes for every single tuple in the table, rewrite every 8KB disk page, rebuild all dependent indexes, and rewrite WAL logs ($O(N)$ table rewrite), locking all access during the process.
</details>

### Q2: What is "Tolerant Reader" and why is it essential for zero-downtime database deployments?
<details>
<summary>Click to view answer</summary>

"Tolerant Reader" is an architectural design pattern for service integration. In database systems, an application is a tolerant reader if it:
1. Projects only the explicit columns it needs (`SELECT id, name`) and never issues `SELECT *`.
2. Gracefully handles unmapped or unexpected fields returned by database drivers without throwing deserialization exceptions.
This ensures that when the database team adds new columns or temporary migration columns to a table, existing production application instances continue running without errors.
</details>

### Q3: What is Vertical Partitioning and what performance problem does it solve?
<details>
<summary>Click to view answer</summary>

Vertical Partitioning splits a single table into two or more tables by columns (rather than by rows). Typically, high-frequency, narrow columns (IDs, numbers, short titles) are placed in a "Hot" table, while wide, rarely-accessed columns (large descriptions, JSON blobs, image binaries) are placed in a "Cold" table linked by a 1:1 foreign key. 

This dramatically improves buffer pool cache locality: the hot table packs hundreds of rows per 8KB page, allowing high-frequency transactional and search queries to run entirely in RAM without loading gigabytes of cold text data off disk.
</details>

### Q4: How can an `INSTEAD OF` trigger on a View facilitate a zero-downtime database refactoring?
<details>
<summary>Click to view answer</summary>

When a legacy table is refactored (e.g., split into multiple normalized tables), modifying all application queries simultaneously across 20 microservices is impossible. 

By replacing the legacy table with a View of the same name and attaching an `INSTEAD OF INSERT / UPDATE / DELETE` trigger to the view:
1. Legacy applications continue issuing simple inserts and updates against what they believe is the original table.
2. The trigger transparently intercepts the writes and routes the attributes to the appropriate underlying split tables.
3. Once all application code has migrated to the new schema, the view and trigger can be safely decommissioned.
</details>

### Q5: What is the safest way to deprecate a database column before physically dropping it?
<details>
<summary>Click to view answer</summary>

1. **Document & Annotate**: Add a `COMMENT ON COLUMN` noting the deprecation date and replacement path.
2. **Remove Application Writes**: Deploy code that stops writing to the column.
3. **Remove Application Reads**: Deploy code that stops reading the column.
4. **Audit via System Catalogs**: Query `pg_stat_statements` or database audit logs to verify that zero queries have referenced the column name over a 30-day billing cycle.
5. **Drop Column**: Execute `ALTER TABLE ... DROP COLUMN` within a safe `lock_timeout` wrapper.
</details>

### Q6: Can you drop an individual value from a PostgreSQL native `ENUM`?
<details>
<summary>Click to view answer</summary>

No. As of PostgreSQL 17, PostgreSQL does not support `ALTER TYPE ... DROP VALUE`. To remove an enum value, you must:
1. Create a new enum type without the unwanted value.
2. Alter all dependent table columns to use the new type (requiring table locks or Expand/Contract migration).
3. Drop the old enum type.
Because of this operational rigidity, using `VARCHAR` with a `CHECK` constraint is preferred for rapidly evolving categorical domains.
</details>

### Q7: What is the purpose of `pg_stat_statements` during schema evolution?
<details>
<summary>Click to view answer</summary>

`pg_stat_statements` is a core PostgreSQL extension that tracks execution statistics and normalized SQL query strings across all database connections. Before dropping a column or modifying an index, database engineers inspect `pg_stat_statements` to verify that no active queries are referencing the target identifier, preventing accidental outages caused by forgotten background jobs or legacy reporting scripts.
</details>

### Q8: What is the performance impact of dropping a column in PostgreSQL 11+?
<details>
<summary>Click to view answer</summary>

In PostgreSQL, executing `ALTER TABLE tbl DROP COLUMN col;` is a **sub-millisecond metadata-only operation**. PostgreSQL does not physically delete the column data from existing disk pages; it merely marks the attribute as dropped (`attisdropped = true` in `pg_attribute`) and hides it from queries. The physical disk space occupied by the dropped column is gradually reclaimed as rows are subsequently updated or when the table is rewritten via `VACUUM FULL` or `pg_repack`.
</details>

---

## Practice Exercises

### Exercise 1: Safe Column Widening Check
Write an SQL query to inspect the current maximum length constraint of column `postal_code` on table `addresses` in PostgreSQL's `information_schema`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    table_name,
    column_name,
    data_type,
    character_maximum_length
FROM information_schema.columns
WHERE table_name = 'addresses' 
  AND column_name = 'postal_code';
```
</details>

---

### Exercise 2: Vertical Partitioning Implementation
Given a table `articles(id, title, author_id, published_at, full_body_text, raw_markdown)`. Decompose this into a hot metadata table `articles` and a cold content table `article_contents`.

<details>
<summary>Click to view solution</summary>

```sql
-- Hot Table (Fast scans, compact memory footprint)
CREATE TABLE articles (
    article_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    title VARCHAR(200) NOT NULL,
    author_id BIGINT NOT NULL,
    published_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Cold Table (Heavy text payloads stored out-of-line)
CREATE TABLE article_contents (
    article_id BIGINT PRIMARY KEY REFERENCES articles(article_id) ON DELETE CASCADE,
    full_body_text TEXT NOT NULL,
    raw_markdown TEXT NOT NULL
);
```
</details>

---

### Exercise 3: Anti-Corruption View Creation
Create an updateable view named `legacy_products` that wraps the split tables `products_hot(product_id, title, price)` and `products_cold(product_id, specs)`.

<details>
<summary>Click to view solution</summary>

```sql
CREATE VIEW legacy_products AS
SELECT 
    h.product_id,
    h.title,
    h.price,
    c.specs
FROM products_hot h
LEFT JOIN products_cold c ON h.product_id = c.product_id;
```
</details>

---

### Exercise 4: Deprecation Column Comment Annotation
Add a standardized deprecation comment to column `secondary_phone` on table `user_contacts` indicating that it is scheduled for removal on 2026-12-31.

<details>
<summary>Click to view solution</summary>

```sql
COMMENT ON COLUMN user_contacts.secondary_phone IS 
'DEPRECATED [Drop Target: 2026-12-31]. Contact numbers migrated to user_phones table.';
```
</details>

---

### Exercise 5: Verification of Dropped Column Metadata
Write a query against PostgreSQL's `pg_attribute` catalog to list all dropped columns that are still occupying physical attribute slots on table `orders`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    attrelid::regclass AS table_name,
    attname AS column_alias,
    attnum AS physical_column_number
FROM pg_attribute
WHERE attrelid = 'orders'::regclass
  AND attisdropped = TRUE;
```
</details>

---

### Exercise 6: Fixing the Table Rewrite Outage
An architect needs to widen `company_id` from `INT` to `BIGINT` on a 50GB table `company_events`. Provide the non-blocking Expand/Contract architectural plan.

<details>
<summary>Click to view solution</summary>

1. **Expand**: Add `new_company_id BIGINT NULL` to `company_events`.
2. **Sync**: Create a trigger copying `company_id` to `new_company_id` on all new inserts and updates.
3. **Backfill**: Asynchronously update historical rows in batches of 10,000 using primary key ranges.
4. **Index & Validate**: Create indexes concurrently on `new_company_id` and add `NOT NULL` via validated check constraints.
5. **Contract**: Swap column names in a brief 2-second locked transaction, repoint foreign keys, and drop the old column.
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                               SCHEMA EVOLUTION CHEAT SHEET                              |
+-------------------+----------------------------+----------------------------------------+
| Operation         | Physical Impact            | Safety Strategy                        |
+-------------------+----------------------------+----------------------------------------+
| VARCHAR Widening  | Pure Metadata ($O(1)$)     | Execute directly (PG 9.1+)             |
| INT -> BIGINT     | Full Table Rewrite ($O(N)$)| Expand / Contract dual-column pattern  |
| Table Split       | Vertical Partitioning      | Create backward-compatible View        |
| Column Drop       | Metadata Mark ($O(1)$)     | Deprecate in code first; drop later    |
| Add Enum Value    | Metadata ($O(1)$)          | ALTER TYPE ... ADD VALUE               |
| Drop Enum Value   | Forbidden natively         | Avoid native ENUMs; use VARCHAR+CHECK  |
+-------------------+----------------------------+----------------------------------------+
```

- **Postel's Law**: Be tolerant in queries (`SELECT explicit_cols`), conservative in writes.
- **Cache Locality**: Separate narrow hot columns from massive cold text blobs.
- **Verification Rule**: Use `pg_stat_statements` to verify zero queries access deprecated columns before deletion.

---

## Further Reading
- PostgreSQL 17 Documentation: *ALTER TABLE & Binary Coercion Pathways*
- Martin Fowler: *Evolutionary Database Design (Refactoring Databases)*
- Pramod Sadalage & Scott Ambler: *Refactoring Databases: Evolutionary Database Design*
- GitLab Engineering: *Zero-Downtime Database Migration Best Practices*
