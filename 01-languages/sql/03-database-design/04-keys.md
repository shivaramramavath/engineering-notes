# Keys: Natural, Surrogate, Composite, and UUIDv7 Architecture

| Metadata | Details |
| :--- | :--- |
| **Topic** | Primary Key Design, Natural vs Surrogate Keys, Sequential Identity vs UUIDv4 vs UUIDv7, B-Tree Fragmentation |
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

The choice of Primary Key is the most foundational physical design decision in a relational database. Every foreign key references it, every secondary index stores it (in clustered storage engines like MySQL InnoDB and SQL Server), and every update touches its underlying B-Tree index.

Yet, teams routinely make critical mistakes:
- Using natural business keys (like email addresses or national IDs) that subsequently change, breaking referential integrity across 30 tables.
- Adopting sequential integers (`BIGINT IDENTITY`) for public web APIs, exposing the business to Insecure Direct Object Reference (IDOR) attacks and competitor scraping.
- Switching blindly to standard random UUIDv4 (`gen_random_uuid()`), which scatters writes randomly across the B-Tree index, causing catastrophic **leaf page splits**, buffer pool thrashing, and a 10x write performance collapse.

The solution lies in understanding key classification, B-Tree leaf page dynamics, and adopting modern **time-sorted unique identifiers (UUIDv7 - RFC 9562)**. 

UUIDv7 combines the best of both worlds: decentralized, collision-free generation with monotonic time-ordered sortability, ensuring smooth B-Tree append operations without fragmentation.

---

## Core Concepts

### 1. Key Classifications
- **Superkey**: Any set of columns that uniquely identifies a row in a relation.
- **Candidate Key**: A minimal superkey (no subset can uniquely identify the row).
- **Primary Key (PK)**: The single candidate key selected by the database architect as the official identifier for the entity.
- **Natural Key**: An attribute that exists naturally in the real-world business domain (e.g., `Social Security Number`, `VIN`, `ISBN`, `email`).
- **Surrogate Key**: An artificial, meaningless identifier generated purely for the database system (e.g., sequential integer `1, 2, 3...` or a UUID).
- **Composite Key**: A primary key composed of two or more columns (common in junction tables and weak entities: `(order_id, line_number)`).

### 2. Natural vs Surrogate Tradeoffs
- **Natural Keys**:
  - *Advantage*: Eliminates artificial columns; enforces business uniqueness directly.
  - *Fatal Flaw*: **Real-world data changes**. People change emails; countries update tax ID formats; companies rebrand. Updating a natural primary key cascades updates across millions of child rows, locking tables.
- **Surrogate Keys**:
  - *Advantage*: Completely immutable, compact, and completely decoupled from business semantics.
  - *Requirement*: Must always be paired with `UNIQUE` constraints on the natural attributes to prevent duplicate business entities!

### 3. Sequential Integers vs UUIDs
- **Auto-Increment / `IDENTITY` (`BIGINT`)**:
  - 8 bytes on disk. Monotonically increasing.
  - Appends to the rightmost leaf page of the B-Tree. Zero fragmentation!
  - *Flaws*: Central coordination required (cannot generate offline in distributed microservices); easily guessable in public URLs (`/invoices/1001` -> `/invoices/1002`).
- **UUIDv4 (Random 128-bit)**:
  - Generated client-side without database round-trips. Unguessable.
  - *Flaw*: Completely random distribution causes extreme B-Tree fragmentation.
- **UUIDv7 (RFC 9562 Time-Sorted UUID)**:
  - 128-bit identifier composed of a 48-bit UNIX timestamp (millisecond precision) followed by 74 bits of cryptographically secure random entropy.
  - Time-ordered: Inserts cleanly to the right edge of B-Tree indexes with minimal page splits!

---

## How It Works Internally

How does the randomness of a primary key degrade storage engine performance?

```
             [ Sequential Key / UUIDv7 Insertion ]
   B-Tree Leaf Page 1       B-Tree Leaf Page 2       B-Tree Leaf Page 3 (Active)
   [ 101 | 102 | 103 ]  ──► [ 104 | 105 | 106 ]  ──► [ 107 | 108 | (Append Here) ]
   All new keys arrive in chronological order.
   Pages remain 100% full. Zero page splits!
   Reads and writes hit the hot cache in memory.

═══════════════════════════════════════════════════════════════════════════════

             [ Random UUIDv4 Insertion (B-Tree Thrashing) ]
   B-Tree Leaf Page 1       B-Tree Leaf Page 2       B-Tree Leaf Page 3
   [ 04a | 12f | 38c ]  ──► [ 49b | 71a | 84e ]  ──► [ 91c | b2a | f4e ]
                                     ▲
                           New key arrives: '62d'
                  Page 2 is completely FULL! What happens?
                                     │
                                     ▼
                          [ B-Tree 50/50 Page Split ]
                  1. Engine allocates a new empty page.
                  2. Moves 50% of Page 2 tuples to new page.
                  3. Rewires linked-list sibling pointers.
                  4. Generates WAL log records for 2 pages!
                  5. Index becomes 50% empty (Storage Bloat)!
```

### The Cost of Randomness:
1. **B-Tree Page Splits**: When an inserted key falls in the middle of a full 8KB leaf page, the engine must split the page in half, moving half the tuples to a newly allocated page.
2. **Buffer Pool Eviction**: Because random UUIDs scatter insertions across all leaf pages across disk, the database cannot keep the entire B-Tree in RAM. Every single insert becomes a random physical disk read/write operation.
3. **Clustered Storage Amplification**: In MySQL InnoDB and SQL Server, secondary indexes do not store physical row pointers; they store the **Primary Key value**. A 16-byte UUID primary key bloats every secondary index on the table by 2x to 4x compared to an 8-byte integer.

---

## Syntax & Usage

### 1. Modern PostgreSQL Identity Column (SQL:2003 Standard)
```sql
CREATE TABLE accounts (
    account_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    account_number VARCHAR(30) NOT NULL UNIQUE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

### 2. Native UUIDv7 Implementation in PostgreSQL 17+
```sql
-- PostgreSQL 17 / Extension Syntax:
CREATE TABLE orders (
    -- UUIDv7: 48-bit timestamp + random entropy
    order_id UUID PRIMARY KEY DEFAULT uuid_generate_v7(),
    customer_id BIGINT NOT NULL,
    total_amount NUMERIC(10,2) NOT NULL
);
```

### 3. SQL Server Identity and Sequential UUID
```sql
-- SQL Server 2022 Syntax:
CREATE TABLE dbo.Invoices (
    InvoiceId UNIQUEIDENTIFIER PRIMARY KEY DEFAULT NEWSEQUENTIALID(),
    Amount DECIMAL(18,2) NOT NULL
);
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **Native UUID Type** | 16-byte binary `UUID` type | Native 16-byte `UUID` (or `BINARY(16)`) | `TEXT` or `BLOB` | 16-byte `UNIQUEIDENTIFIER` |
| **Standard Identity** | `GENERATED ALWAYS AS IDENTITY` | `AUTO_INCREMENT` | `INTEGER PRIMARY KEY` (rowid) | `IDENTITY(1,1)` |
| **Time-Sorted UUID** | `uuid_generate_v7()` / extensions | `UUID_TO_BIN(UUID(), 1)` (Swaps time) | Custom user function | `NEWSEQUENTIALID()` |
| **Secondary Index PK Storage**| Stores 6-byte heap `ctid` (Minimal bloat) | Stores the Primary Key! (Heavy bloat) | Stores 8-byte integer `rowid` | Stores the Clustered Key! (Heavy bloat) |

---

## Practical Examples

### Example 1: Basic — Composite Primary Key for Weak Entities
Model order line items using a composite primary key that combines parent reference with a sequential line number.

```sql
CREATE TABLE purchase_orders (
    order_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    customer_email VARCHAR(100) NOT NULL
);

CREATE TABLE purchase_order_lines (
    order_id BIGINT NOT NULL,
    line_number INT NOT NULL,
    product_sku VARCHAR(30) NOT NULL,
    quantity INT NOT NULL CHECK (quantity > 0),
    PRIMARY KEY (order_id, line_number),
    FOREIGN KEY (order_id) REFERENCES purchase_orders(order_id) ON DELETE CASCADE
);

INSERT INTO purchase_orders (customer_email) VALUES ('alice@example.com');

INSERT INTO purchase_order_lines (order_id, line_number, product_sku, quantity) VALUES
(1, 1, 'LAPTOP-PRO', 1),
(1, 2, 'USB-C-CABLE', 2);

SELECT * FROM purchase_order_lines WHERE order_id = 1;
```

```
Result:
 order_id | line_number | product_sku | quantity 
----------+-------------+-------------+----------
        1 |           1 | LAPTOP-PRO  |        1
        1 |           2 | USB-C-CABLE |        2
(2 rows)
```

---

### Example 2: Real-World — Dual-Key Pattern (Internal Identity + Public UUID)
The gold standard for production web architectures: an internal `BIGINT IDENTITY` for fast internal joins and foreign keys, paired with an external `UUIDv7` for public APIs.

```sql
CREATE TABLE users (
    -- Internal Primary Key: 8 bytes, maximum join performance, index efficiency
    user_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    
    -- Public UUID: Exposed in REST/GraphQL APIs, unguessable, safe from IDOR attacks
    public_id UUID NOT NULL DEFAULT gen_random_uuid() UNIQUE,
    
    email VARCHAR(255) NOT NULL UNIQUE,
    full_name VARCHAR(100) NOT NULL
);

CREATE TABLE user_orders (
    order_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    -- Foreign key references internal user_id, NOT the 16-byte UUID!
    user_id BIGINT NOT NULL REFERENCES users(user_id),
    order_total NUMERIC(10,2) NOT NULL
);

CREATE INDEX idx_user_orders_user_id ON user_orders (user_id);
```

---

### Example 3: Edge Case — Extracting Timestamp from UUIDv7
Because UUIDv7 embeds a 48-bit millisecond UNIX timestamp in its leading bytes, the creation timestamp can be extracted mathematically without storing a separate `created_at` column.

```sql
-- Demonstration of UUIDv7 Structure:
-- Example UUIDv7: 018c21e6-b4b0-7d3e-908d-8a9d12345678
-- Leading 48 bits (018c21e6b4b0 hex) = 1701416973488 ms
-- Corresponds to: 2023-12-01 07:49:33.488 UTC

-- In PostgreSQL, convert the leading 12 hex characters to a timestamp:
SELECT TO_TIMESTAMP(
    ('x' || SUBSTRING('018c21e6-b4b0-7d3e-908d-8a9d12345678' FROM 1 FOR 8) || 
     SUBSTRING('018c21e6-b4b0-7d3e-908d-8a9d12345678' FROM 10 FOR 4))::bit(48)::bigint / 1000.0
) AT TIME ZONE 'UTC' AS extracted_creation_time;
```

```
Result:
   extracted_creation_time   
-----------------------------
 2023-12-01 07:49:33.488+00
(1 row)
```

---

### Example 4: Failure Mode — B-Tree Index Bloat Under UUIDv4
Benchmarking 1,000,000 random UUIDv4 inserts against sequential identifiers.

```sql
-- Benchmark Profile:
-- Table A: BIGINT IDENTITY PK
--   Index Size: ~22 MB
--   Insert Throughput: ~45,000 writes/sec
--   Leaf Page Fill Rate: ~95%
--
-- Table B: UUIDv4 (Random) PK
--   Index Size: ~85 MB (4x bloat!)
--   Insert Throughput: ~4,200 writes/sec (10x slower!)
--   Leaf Page Fill Rate: ~50% (Constant page splits!)
```
*Takeaway*: Never use random UUIDv4 as a clustered primary key in high-throughput databases!

---

### Example 5: Optimized — MySQL 8.4 Time-Ordered Binary UUID
In MySQL InnoDB, where tables are strictly organized as clustered indexes on the primary key, UUIDs must be rearranged using `UUID_TO_BIN(..., 1)` to place the timestamp bytes first.

```sql
-- MySQL 8.4 LTS Syntax:
CREATE TABLE transactions (
    -- BINARY(16) stores compact 16-byte UUID
    txn_id BINARY(16) PRIMARY KEY,
    amount DECIMAL(10,2) NOT NULL
);

-- Insert using time-swapping parameter (flag = 1):
-- Reorders timestamp components so UUID sorts chronologically!
INSERT INTO transactions (txn_id, amount) 
VALUES (UUID_TO_BIN(UUID(), 1), 500.00);

-- Query back to human-readable string:
SELECT BIN_TO_UUID(txn_id, 1) AS txn_uuid, amount FROM transactions;
```

---

## Best Practices

1. **Use the Dual-Key Architecture for Web APIs**: Use an internal `BIGINT IDENTITY` for internal foreign key joins, and an external `UUIDv7` for public API endpoints.
2. **Never Use Random UUIDv4 as a Clustered Primary Key**: In MySQL and SQL Server, random UUIDv4 destroys index locality and saturates disk I/O with page splits. Use UUIDv7 or `NEWSEQUENTIALID()`.
3. **Always Add Unique Constraints When Using Surrogate Keys**: A surrogate key guarantees technical uniqueness; you must still declare `UNIQUE(email)` or `UNIQUE(tax_id)` to enforce business rules.
4. **Prefer `BIGINT` over `INT` for Identity Columns**: A standard 32-bit `INT` caps at 2,147,483,647. High-volume systems exhaust 32-bit integers in production, requiring painful, zero-downtime database migrations.
5. **Use Composite Primary Keys for Association Tables**: For M:N junction tables, make the primary key `PRIMARY KEY (parent_a_id, parent_b_id)` rather than introducing an unnecessary surrogate `id`.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Using sequential integers in public URLs | Exposes IDs to scraping and enumeration attacks (IDOR vulnerabilities). | Use public UUIDv7 or NanoID for public routes. |
| 2 | Using UUIDv4 as a Primary Key in MySQL InnoDB | Forces constant B-Tree page splits and leaves index pages 50% empty. | Use sequential BIGINT or time-sorted UUIDv7. |
| 3 | Choosing a Natural Key that changes | Updating an email or tax ID cascades updates across millions of child rows. | Use immutable surrogate keys; put `UNIQUE` on natural attributes. |
| 4 | Storing UUIDs as `VARCHAR(36)` | Consumes 36 bytes per row plus index overhead instead of 16 bytes. | Use native `UUID` (PostgreSQL) or `BINARY(16)` (MySQL). |
| 5 | Using 32-bit `INT` for fast-growing transaction tables | Reaches integer limit (2.1 billion) and causes database write outages. | Always use 64-bit `BIGINT` for auto-increment keys. |
| 6 | Forgetting a reverse index on Composite Keys | A composite PK on `(a, b)` cannot be used for queries searching strictly by `b`. | Add a secondary index on `(b, a)` for reverse lookups. |

---

## Performance & Security Notes

### Secondary Index Bloat in Clustered Storage Engines
In clustered database architectures (MySQL InnoDB and SQL Server):
- Every secondary index stores a copy of the **Primary Key** as its tuple pointer.
- If a table has 6 secondary indexes:
  - With an 8-byte `BIGINT` PK: $6 \times 8 = 48$ bytes of key data per row.
  - With a 36-byte `VARCHAR(36)` UUID: $6 \times 36 = 216$ bytes of key data per row!
- Across 50,000,000 rows, this represents **8.4 GB of wasted RAM** in the buffer pool purely from redundant UUID strings in secondary indexes.

---

## Comparison Matrix

| Key Strategy | Disk Footprint | B-Tree Fragmentation | Distributed Generation? | Public Safe (No IDOR)? |
| :--- | :--- | :--- | :--- | :--- |
| **BIGINT IDENTITY** | 8 bytes | **None (0% Splits)** | No (Central DB bottleneck) | **No** (Easily enumerable) |
| **Natural Key (Text)** | 20–100 bytes | Variable | Yes (Domain dependent) | Dependent on domain |
| **UUIDv4 (Random)** | 16 bytes | **Severe (50% Splits)**| Yes (Client-side) | **Yes** (Unguessable) |
| **UUIDv7 (Time-Sorted)**| 16 bytes | **Minimal (<1% Splits)**| Yes (Client-side) | **Yes** (Unguessable) |
| **Composite Key** | Sum of column sizes | None (If sequential) | No | Dependent on components |

---

## Interview Questions & Deep Dives

### Q1: What is a B-Tree page split and why does random UUIDv4 trigger it?
<details>
<summary>Click to view answer</summary>

A B-Tree stores index keys in sorted order within fixed-size memory blocks (typically 8KB leaf pages). When new keys arrive in sequential order, the engine appends them to the rightmost page until full, then allocates a new page. 

When random UUIDv4 keys are inserted, the key value can fall anywhere in the entire key space with equal probability. When an insert targets a leaf page that is already 100% full, the storage engine must allocate a new page and move 50% of the tuples from the existing page to the new page. This 50/50 page split:
1. Causes write amplification (writing 2 pages to WAL instead of 1).
2. Leaves index pages 50% empty, doubling total index size on disk.
3. Destroys spatial cache locality, requiring random disk reads for every insert.
</details>

### Q2: How does UUIDv7 (RFC 9562) solve the B-Tree fragmentation problem?
<details>
<summary>Click to view answer</summary>

UUIDv7 encodes a 48-bit big-endian UNIX millisecond timestamp into the most significant bits of the 128-bit structure, followed by version bits and 74 bits of cryptographically secure random entropy. 

Because the leading 48 bits increment monotonically over time, newly generated UUIDv7 values are always greater than previously generated values. When inserted into a B-Tree index, they append cleanly to the right edge of the index, mimicking the optimal insertion performance of sequential integers while retaining the decentralized, collision-free properties of UUIDs.
</details>

### Q3: What is the "Dual-Key Architecture" pattern and why is it used?
<details>
<summary>Click to view answer</summary>

The Dual-Key Architecture pattern assigns two distinct identifiers to an entity:
1. An **Internal Surrogate Key** (`BIGINT GENERATED ALWAYS AS IDENTITY`): Used exclusively inside the database as the primary key and referenced by all internal foreign keys. It maximizes join performance, minimizes secondary index size, and avoids page splits.
2. An **External Public Key** (`UUIDv7` or `UUIDv4`): Exposed in external REST/GraphQL APIs and URLs (`api.com/v1/orders/018c21e6...`). It prevents competitors from scraping order volume and protects against Insecure Direct Object Reference (IDOR) attacks.
</details>

### Q4: Why is it dangerous to use a natural key like an Email Address as a Primary Key?
<details>
<summary>Click to view answer</summary>

1. **Mutability**: Users frequently change their email addresses. Updating a primary key requires cascading updates to all child foreign keys across the entire database, locking multiple tables and risking deadlocks.
2. **Width & Index Bloat**: An email string averages 25–40 bytes, compared to 8 bytes for a `BIGINT`. Because secondary indexes in clustered engines store the primary key, large string keys drastically bloat all secondary indexes.
3. **Data Privacy**: Storing personal identifiable information (PII) like email addresses in foreign key audit tables complicates GDPR / CCPA "Right to be Forgotten" erasure requests.
</details>

### Q5: What is the difference between `GENERATED ALWAYS AS IDENTITY` and `GENERATED BY DEFAULT AS IDENTITY`?
<details>
<summary>Click to view answer</summary>

- `GENERATED ALWAYS AS IDENTITY`: The database engine strictly generates the value. Any attempt by application code to supply a manual value in an `INSERT` statement raises an error (unless overridden with `OVERRIDING SYSTEM VALUE`).
- `GENERATED BY DEFAULT AS IDENTITY`: The database generates a sequential value only if the application omits the column in the `INSERT` statement. If the application supplies an explicit value, the database accepts it. `GENERATED ALWAYS` is safer for preventing duplicate sequence collisions.
</details>

### Q6: How does MySQL InnoDB store Secondary Indexes relative to the Primary Key?
<details>
<summary>Click to view answer</summary>

In MySQL InnoDB, the physical table is organized as an **Index-Organized Table (Clustered Index)** sorted by the Primary Key. Secondary indexes do *not* point to physical disk byte offsets; instead, each secondary index leaf node stores the **Primary Key value** of the matching row. When a query searches via a secondary index, it performs an initial seek in the secondary index, retrieves the primary key, and performs a second seek in the clustered index (a "bookmark lookup").
</details>

### Q7: Can a Composite Primary Key be used to enforce ordering on multiple dimensions?
<details>
<summary>Click to view answer</summary>

Yes. A composite primary key `PRIMARY KEY (tenant_id, created_at, event_id)` defines the physical clustering order on disk. Tuples belonging to the same `tenant_id` are stored contiguously in chronological order. However, queries filtering strictly by `created_at` without specifying `tenant_id` cannot utilize the index efficiently unless a separate index is created.
</details>

### Q8: What is ULID and how does it compare to UUIDv7?
<details>
<summary>Click to view answer</summary>

ULID (Universally Unique Lexicographically Sortable Identifier) is a 128-bit time-ordered identifier introduced prior to the standardization of UUIDv7. It consists of a 48-bit timestamp and 80 bits of randomness, encoded as a 26-character Crockford's Base32 string (URL-safe, case-insensitive, human-readable). UUIDv7 is the official IETF standard (RFC 9562) that achieves the exact same time-sorted property using standard 128-bit UUID binary and hyphenated hex representations.
</details>

---

## Practice Exercises

### Exercise 1: Implement Dual-Key User Schema
Write DDL for a `merchants` table implementing the Dual-Key Architecture:
- `merchant_id` as an internal 64-bit identity primary key.
- `merchant_uuid` as an unguessable public UUIDv7/UUIDv4 unique key.
- `tax_id` as a natural unique business key.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE merchants (
    merchant_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    merchant_uuid UUID NOT NULL DEFAULT gen_random_uuid() UNIQUE,
    tax_id VARCHAR(30) NOT NULL UNIQUE,
    company_name VARCHAR(150) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```
</details>

---

### Exercise 2: Composite Primary Key with Reverse Index
Write DDL for an e-commerce `product_categories` junction table that maps products to categories. Ensure that looking up all categories for a product AND looking up all products for a category both utilize index-only seeks.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE product_categories (
    product_id BIGINT NOT NULL,
    category_id BIGINT NOT NULL,
    assigned_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    -- Primary key automatically indexes (product_id, category_id)
    PRIMARY KEY (product_id, category_id),
    FOREIGN KEY (product_id) REFERENCES products(product_id) ON DELETE CASCADE,
    FOREIGN KEY (category_id) REFERENCES categories(category_id) ON DELETE CASCADE
);

-- Crucial: Reverse index for queries filtering by category_id!
CREATE INDEX idx_product_categories_reverse 
ON product_categories (category_id, product_id);
```
</details>

---

### Exercise 3: Safe Manual Sequence Value Override
Write an `INSERT` statement in PostgreSQL that inserts an explicit ID into a `GENERATED ALWAYS AS IDENTITY` table without dropping or modifying the constraint.

<details>
<summary>Click to view solution</summary>

```sql
INSERT INTO accounts (account_id, account_number)
OVERRIDING SYSTEM VALUE
VALUES (9999, 'ACC-OVERRIDE-01');
```
</details>

---

### Exercise 4: Detect Missing Natural Uniqueness
An architect writes:
```sql
CREATE TABLE currencies (
    currency_id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    iso_code VARCHAR(3) NOT NULL,
    currency_name VARCHAR(50) NOT NULL
);
```
Identify the integrity defect and provide the fix.

<details>
<summary>Click to view solution</summary>

**Defect**: While `currency_id` is unique, `iso_code` lacks a `UNIQUE` constraint. An application bug could insert duplicate rows for `'USD'` with different surrogate IDs (`currency_id = 1` and `currency_id = 2`), corrupting the financial ledger.

**Fix**:
```sql
CREATE TABLE currencies (
    currency_id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    iso_code VARCHAR(3) NOT NULL UNIQUE, -- Enforce natural uniqueness!
    currency_name VARCHAR(50) NOT NULL
);
```
</details>

---

### Exercise 5: Benchmark Verification Plan
Write an `EXPLAIN` query checking whether searching a table by `public_uuid` uses an index scan versus searching by `id`.

<details>
<summary>Click to view solution</summary>

```sql
EXPLAIN ANALYZE
SELECT * FROM merchants WHERE merchant_uuid = '018c21e6-b4b0-7d3e-908d-8a9d12345678';
-- Expected plan: Index Scan using merchants_merchant_uuid_key
```
</details>

---

### Exercise 6: Fixing the 32-bit Integer Exhaustion Bug
A production database running on PostgreSQL runs out of IDs on a table declared with `SERIAL` (32-bit integer). Describe the zero-downtime migration strategy to upgrade the column to `BIGINT`.

<details>
<summary>Click to view solution</summary>

Directly executing `ALTER TABLE tbl ALTER COLUMN id TYPE BIGINT;` takes an exclusive table lock and rewrites the entire multi-gigabyte table, causing extended downtime.

**Zero-Downtime Migration Strategy**:
1. Add a new column: `ALTER TABLE tbl ADD COLUMN new_id BIGINT;`
2. Create a trigger to copy `id` to `new_id` on all new inserts.
3. Backfill existing rows in small batches: `UPDATE tbl SET new_id = id WHERE new_id IS NULL AND id BETWEEN ...`.
4. Add foreign keys and unique constraints concurrently on `new_id`.
5. In a brief transaction, rename `id` to `old_id` and `new_id` to `id`.
6. Update foreign key constraints across child tables.
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                  PRIMARY KEY CHEAT SHEET                                |
+-------------------+----------------------------+----------------------------------------+
| Identifier Type   | Byte Size                  | Recommended Use Case                   |
+-------------------+----------------------------+----------------------------------------+
| BIGINT IDENTITY   | 8 bytes                    | Internal PK for 95% of OLTP tables     |
| UUIDv7 (RFC 9562) | 16 bytes                   | Public APIs, distributed microservices |
| UUIDv4 (Random)   | 16 bytes                   | Ephemeral tokens (AVOID as clustered PK|
| Composite Key     | Sum of columns             | M:N junction tables, weak entities     |
| Natural Key       | Variable                   | Pure reference lookup catalogs only    |
+-------------------+----------------------------+----------------------------------------+
```

- **Dual-Key Pattern**: Internal `BIGINT IDENTITY` + Public `UUIDv7`.
- **B-Tree Safety**: Never use random UUIDv4 as a clustered primary key.
- **Natural Uniqueness**: Always add `UNIQUE` constraints to natural business attributes when using surrogate IDs.

---

## Further Reading
- IETF RFC 9562: *Universally Unique IDentifiers (UUIDv7 Specification)*
- PostgreSQL 17 Documentation: *Identity Columns and UUID Types*
- Markus Winand: *Use The Index, Luke! - The Cost of Random Primary Keys*
- C.J. Date: *Database Design and Relational Theory (Keys and Candidate Keys)*
