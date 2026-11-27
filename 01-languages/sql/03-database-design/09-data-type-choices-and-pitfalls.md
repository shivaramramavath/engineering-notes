# Data Type Choices, Physical Storage, and Common Pitfalls

| Metadata | Details |
| :--- | :--- |
| **Topic** | SQL Data Types, Physical Storage Footprint, Exact vs Float, VARCHAR vs TEXT, TIMESTAMPTZ, ENUMs |
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

Selecting data types during schema design is often treated as a casual, routine task. Yet, data type selection governs the physical reality of the database engine: how many bytes are written to disk, how many tuples fit into an 8KB memory page, whether index searches perform fast integer comparisons or slow string collations, and whether calculations suffer from floating-point rounding corruption.

Poor data type choices introduce subtle, insidious failures in production:
- Using `FLOAT` or `DOUBLE PRECISION` to store financial balances, causing fractional penny accounting discrepancies (`0.10 + 0.20 = 0.30000000000000004`).
- Storing timestamps as `TIMESTAMP WITHOUT TIME ZONE`, causing daylight saving time shifts to record events that occurred in the future before events that occurred in the past.
- Blindly applying `VARCHAR(255)` everywhere due to outdated MySQL 5.0 tutorials, rather than understanding that `TEXT` in modern PostgreSQL has zero performance penalty and eliminates arbitrary truncation errors.
- Adopting native database `ENUM` types for categories that change frequently, leading to locked table migrations when dropping or renaming values.

Mastering data types from a physical storage, mathematical precision, and migration-safety perspective ensures that tables remain lean, performant, and immune to data corruption.

---

## Core Concepts

### 1. Numeric Types: Exact vs Inexact
- **Exact Numeric Types (`NUMERIC`, `DECIMAL`, `BIGINT`, `INTEGER`, `SMALLINT`)**:
  - Store numbers with exact mathematical precision. Zero rounding error.
  - **Golden Rule**: Financial currency, ledgers, and billing amounts **must always** be stored as `NUMERIC(p, s)` (or integer cents in `BIGINT`).
- **Inexact Numeric Types (`FLOAT`, `REAL`, `DOUBLE PRECISION`)**:
  - Implemented using IEEE 754 binary floating-point representation.
  - Fast in CPU hardware, but inherently cannot represent fractional decimal values (like $0.1$ or $0.05$) accurately in binary.
  - Reserved strictly for scientific, geographic, or machine learning applications where fractional rounding error is acceptable.

### 2. String Types: The `VARCHAR(255)` Myth
- **`CHAR(n)` (Fixed Length)**: Pads values with trailing spaces to fill $n$ characters. Wastes storage on disk and introduces string comparison bugs. Almost never appropriate in modern design.
- **`VARCHAR(n)` (Variable Length with Limit)**: Stores the string plus a 1- or 4-byte length header. Enforces a maximum length check.
- **`TEXT` (Arbitrary Variable Length)**: Stores the string plus length header without an arbitrary length limit.
- **The PostgreSQL Myth**: In PostgreSQL, `VARCHAR(n)` and `TEXT` use the **exact same underlying storage structure** (`varlena`). A `VARCHAR(255)` provides zero performance, storage, or indexing advantage over `TEXT`—it merely adds a check constraint on string length.

### 3. Temporal Types: `TIMESTAMPTZ` vs `TIMESTAMP`
- **`TIMESTAMPTZ` (`TIMESTAMP WITH TIME ZONE`)**:
  - The name is a misnomer: it does **not** store a timezone string!
  - It converts any input timestamp to **UTC** and stores it as an 8-byte integer (microseconds since 2000-01-01 UTC).
  - When queried, it converts the stored UTC timestamp to the client session's local timezone.
  - **Mandatory** for 99% of business events (order dates, audit logs, user signups).
- **`TIMESTAMP` (`WITHOUT TIME ZONE`)**:
  - Stores a local "wall-clock" time without any UTC normalization.
  - Only appropriate for abstract local concepts: "Every store opens at 09:00 local time regardless of country."

### 4. Categorical Data: Native `ENUM` vs Lookup Tables
- **Native `ENUM` (`CREATE TYPE status AS ENUM (...)`)**:
  - Compact: Stores values internally as a 4-byte integer OID.
  - *Migration Trap*: In PostgreSQL, you can append values (`ALTER TYPE ... ADD VALUE`), but you **cannot drop or rename enum values** without rebuilding the entire type and rewriting dependent tables!
- **Lookup Tables (`FOREIGN KEY`)**:
  - Highly flexible: values can be renamed, disabled, and enriched with metadata (e.g., descriptions, display order).
- **Simple `CHECK` Constraint (`CHECK (status IN ('ACTIVE', 'PENDING'))`)**:
  - Lightweight, easy to modify in migrations, perfectly suited for static state machines.

---

## How It Works Internally

How do data types determine physical row layout and page storage?

```
                     [ Physical Row Tuple on Disk ]
 ┌──────────────────────┬─────────────────────────┬─────────────────────────┐
 │ Tuple Header         │ Fixed-Length Attributes │ Variable-Length Attributes
 │ (23 bytes in PG)     │ (INT: 4B, BIGINT: 8B)   │ (TEXT / VARCHAR / JSONB)│
 └──────────────────────┴─────────────────────────┴─────────────────────────┘
                                                       │
                                      Does text exceed 2KB?
                                                       │
                                   ┌───────────────────┴───────────────────┐
                                   ▼                                       ▼
                          [ In-Line Storage ]                     [ TOAST Out-of-Line ]
                          Prefixed by 1-byte                      Compressed and moved
                          or 4-byte length header.                to separate TOAST table!
```

### 1. Data Alignment and Padding
CPU architectures fetch memory more efficiently when data is aligned to word boundaries (e.g., 8-byte boundaries for `BIGINT` and `TIMESTAMPTZ`).
- If you declare `(char(1), bigint, char(1), bigint)`, the storage engine inserts padding bytes between columns, bloating row size.
- Ordering columns by alignment (largest types first: `bigint`, `timestamptz`, `int`, `smallint`, `char`) can reduce physical table footprint by 10%–20%.

### 2. The TOAST Engine (Oversized Attributes)
PostgreSQL storage pages are strictly 8KB. Columns whose length exceeds ~2KB (`TEXT`, `JSONB`, `BYTEA`) are automatically compressed and moved out-of-line into a **TOAST (The Oversized-Attribute Storage Technique)** table. Reading rows that do not select the TOASTed column avoids touching the out-of-line storage pages entirely.

---

## Syntax & Usage

```sql
CREATE TABLE product_ledger (
    entry_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    
    -- Exact Financial Math (10 digits total, 2 decimal places)
    unit_amount NUMERIC(10,2) NOT NULL CHECK (unit_amount >= 0.00),
    
    -- Inexact Scientific Metric (Acceptable for sensor floats)
    storage_temp_celsius DOUBLE PRECISION NOT NULL,
    
    -- Text: Natural length without arbitrary 255 cutoff
    product_description TEXT NOT NULL,
    
    -- Temporal: Always TIMESTAMPTZ for real-world events
    transaction_time TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    -- Categorical: Check constraint over rigid ENUM
    ledger_status VARCHAR(20) NOT NULL DEFAULT 'PENDING'
        CONSTRAINT chk_ledger_status CHECK (
            ledger_status IN ('PENDING', 'SETTLED', 'DISPUTED', 'CANCELLED')
        )
);
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **Exact Money** | `NUMERIC(p,s)` | `DECIMAL(p,s)` | `NUMERIC` / Integer cents | `DECIMAL(p,s)` (Avoid legacy `MONEY`)|
| **`VARCHAR` vs `TEXT`**| Identical performance | `TEXT` cannot have default; off-page | Dynamic typing (`TEXT`) | `VARCHAR(MAX)` vs `VARCHAR(n)` |
| **Timestamp with Zone**| `TIMESTAMPTZ` (UTC 8B) | `TIMESTAMP` (Converts to UTC; 2038 bug!)| Text ISO-8601 strings | `DATETIMEOFFSET` (Stores literal offset)|
| **Native ENUM Type** | `CREATE TYPE ... AS ENUM` | `ENUM('a', 'b')` inline on column | **Not Supported** (Use `CHECK`) | **Not Supported** (Use `CHECK` / FK) |

---

## Practical Examples

### Example 1: Basic — The Floating-Point Accounting Disaster
Demonstrating why storing currency in `FLOAT` or `DOUBLE PRECISION` causes financial calculation discrepancies.

```sql
-- DANGEROUS: Float arithmetic
SELECT 
    (0.1::float + 0.2::float) AS float_sum,
    ((0.1::float + 0.2::float) = 0.3::float) AS float_is_equal,
    -- CORRECT: Exact Numeric arithmetic
    (0.1::numeric + 0.2::numeric) AS exact_sum,
    ((0.1::numeric + 0.2::numeric) = 0.3::numeric) AS exact_is_equal;
```

```
Result:
     float_sum      | float_is_equal | exact_sum | exact_is_equal 
--------------------+----------------+-----------+----------------
 0.30000000000000004| f              |       0.3 | t
(1 row)
```
*Takeaway*: Floating-point equality fails. In financial applications, float arithmetic causes accounting balances to fail reconciliation audits. Always use `NUMERIC`.

---

### Example 2: Real-World — TIMESTAMPTZ vs TIMESTAMP Across Timezones
Demonstrating how `TIMESTAMP WITHOUT TIME ZONE` corrupts temporal reality when users log in from different continents.

```sql
-- Setup test table
CREATE TABLE event_tz_demo (
    id INT PRIMARY KEY,
    plain_ts TIMESTAMP,
    tz_ts TIMESTAMPTZ
);

-- Client session in New York (UTC-5)
SET timezone = 'America/New_York';
INSERT INTO event_tz_demo VALUES 
(1, '2026-06-01 12:00:00', '2026-06-01 12:00:00');

-- Client session in Tokyo (UTC+9) reads the data:
SET timezone = 'Asia/Tokyo';
SELECT 
    plain_ts AS tokyo_sees_plain,
    tz_ts AS tokyo_sees_timestamptz
FROM event_tz_demo;
```

```
Result:
  tokyo_sees_plain   |   tokyo_sees_timestamptz   
---------------------+----------------------------
 2026-06-01 12:00:00 | 2026-06-02 01:00:00+09
(1 row)
```
*Takeaway*: `plain_ts` incorrectly reports that the event occurred at noon Tokyo time! `tz_ts` accurately converts the moment to 1:00 AM the next day Tokyo time (exact 14-hour difference).

---

### Example 3: Edge Case — The `CHAR(n)` Trailing Space Trap
Demonstrating how fixed-length `CHAR(n)` padding causes string comparison bugs and pattern-matching failures.

```sql
CREATE TABLE state_codes (
    char_code CHAR(5),
    varchar_code VARCHAR(5)
);

INSERT INTO state_codes VALUES ('CA', 'CA');

SELECT 
    LENGTH(char_code) AS char_len,       -- Padded to 5: 'CA   '
    LENGTH(varchar_code) AS varchar_len, -- Exact 2: 'CA'
    char_code = 'CA' AS direct_equal,    -- True (SQL standard strips spaces for =)
    char_code LIKE 'CA' AS like_equal    -- FALSE! LIKE does not strip spaces!
FROM state_codes;
```

```
Result:
 char_len | varchar_len | direct_equal | like_equal 
----------+-------------+--------------+------------
        2 |           2 | t            | f
(1 row)
```
*Takeaway*: `char_code LIKE 'CA'` evaluates to `FALSE` because `LIKE` matches the literal trailing padded spaces. Avoid `CHAR(n)`.

---

### Example 4: Failure Mode — The 32-Bit Integer Counter Overflow
Demonstrating the fatal failure when an auto-incrementing integer reaches the 2.14 billion limit.

```sql
CREATE TABLE click_counter (
    id INT PRIMARY KEY -- 32-bit signed integer (-2,147,483,648 to 2,147,483,647)
);

INSERT INTO click_counter VALUES (2147483647);

-- ATTEMPTING NEXT INSERT:
-- INSERT INTO click_counter VALUES (2147483648);
-- ERROR: integer out of range!
-- Any production application inserting at this moment crashes with 500 Internal Server Errors!
-- FIX: Always declare primary keys and sequences as BIGINT (64-bit)!
```

---

### Example 5: Optimized — Modifying Categorical Status Without DDL Lock Downtime
Using a `CHECK` constraint with `NOT VALID` allows adding or changing status categories instantly on multi-million row tables without taking an exclusive table lock.

```sql
CREATE TABLE subscription_orders (
    order_id BIGINT PRIMARY KEY,
    status VARCHAR(30) NOT NULL
);

-- Add flexible check constraint:
ALTER TABLE subscription_orders 
ADD CONSTRAINT chk_sub_status 
CHECK (status IN ('ACTIVE', 'TRIAL', 'EXPIRED', 'CANCELLED'));

-- When adding a new status 'PAUSED' in the future:
-- 1. Drop old constraint (instant metadata update):
ALTER TABLE subscription_orders DROP CONSTRAINT chk_sub_status;

-- 2. Add new constraint with NOT VALID (instant, zero downtime!):
ALTER TABLE subscription_orders 
ADD CONSTRAINT chk_sub_status 
CHECK (status IN ('ACTIVE', 'TRIAL', 'EXPIRED', 'CANCELLED', 'PAUSED')) NOT VALID;
```

---

## Best Practices

1. **Use `NUMERIC(precision, scale)` for All Monetary Values**: Never use floating-point types for currency, discounts, taxes, or balances.
2. **Default to `TIMESTAMPTZ` for Event Timestamps**: Always store timestamps with timezone awareness. The engine normalizes to UTC on disk and displays in the client's local session time.
3. **Prefer `TEXT` over `VARCHAR(255)` in PostgreSQL**: Do not invent arbitrary length limits. Use `TEXT` unless business logic strictly mandates a length limit (e.g., `VARCHAR(2)` for US state codes).
4. **Use `BIGINT` for All Primary Keys and Counters**: The 4 extra bytes per row are cheap insurance against catastrophic 32-bit integer overflow outages.
5. **Prefer `CHECK` Constraints over Native Database `ENUM`s**: Native `ENUM` types are difficult to alter or delete in PostgreSQL. A `VARCHAR` with a `CHECK` constraint is flexible, safe, and zero-downtime friendly.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Storing currency in `FLOAT` / `REAL` | Binary floating-point representation causes fractional penny calculation errors. | Use `NUMERIC(15,2)` or integer cents in `BIGINT`. |
| 2 | Using `TIMESTAMP WITHOUT TIME ZONE` | Loses UTC normalization; events across different timezones appear out of order. | Always use `TIMESTAMPTZ`. |
| 3 | Arbitrary `VARCHAR(255)` everywhere | Historical MySQL superstition; adds length check overhead with zero storage benefit in Postgres. | Use `TEXT` (or specific domain lengths like `VARCHAR(50)`). |
| 4 | Using `CHAR(n)` for short strings | Pads trailing spaces, breaking `LIKE` pattern matching and wasting disk space. | Use `VARCHAR(n)` or `TEXT`. |
| 5 | Using 32-bit `INT` for primary keys | High-churn tables exhaust 2.1 billion rows, causing catastrophic production write outages. | Always use 64-bit `BIGINT`. |
| 6 | Native `ENUM` for fast-changing states | Cannot drop or easily rename enum values in PostgreSQL without rebuilding types. | Use `VARCHAR` with `CHECK` constraints or a lookup table. |

---

## Performance & Security Notes

### Column Alignment & Row Packing Optimization
In PostgreSQL, attributes are physically aligned to their natural byte size boundaries:
- `BIGINT`, `TIMESTAMPTZ`, `DOUBLE PRECISION` align to **8-byte** boundaries.
- `INTEGER`, `DATE` align to **4-byte** boundaries.
- `SMALLINT` aligns to **2-byte** boundaries.
- `BOOLEAN`, `CHAR` align to **1-byte** boundaries.

Consider this inefficient table definition:
```sql
CREATE TABLE poorly_packed (
    col_a SMALLINT,  -- 2 bytes + 6 bytes padding!
    col_b BIGINT,    -- 8 bytes
    col_c SMALLINT,  -- 2 bytes + 6 bytes padding!
    col_d BIGINT     -- 8 bytes
); -- Row payload size: 32 bytes (12 bytes wasted in padding!)
```
Rearranging columns by alignment size:
```sql
CREATE TABLE well_packed (
    col_b BIGINT,    -- 8 bytes
    col_d BIGINT,    -- 8 bytes
    col_a SMALLINT,  -- 2 bytes
    col_c SMALLINT   -- 2 bytes + 4 bytes padding at end
); -- Row payload size: 24 bytes (8 bytes saved per row!)
```
Across 100,000,000 rows, proper column ordering saves **800 MB of disk space and RAM**.

---

## Comparison Matrix

| Data Type | Storage Size | Precision | Best Used For | Anti-Pattern For |
| :--- | :--- | :--- | :--- | :--- |
| **`BIGINT`** | 8 bytes | Exact integer ($\pm 9 \times 10^{18}$)| Primary keys, high-volume counters | Small static enum codes |
| **`INTEGER`** | 4 bytes | Exact integer ($\pm 2.1 \times 10^9$) | Foreign keys with bounded sizes | Auto-increment keys for high churn |
| **`NUMERIC(p,s)`**| Variable (Numeric)| Exact decimal | Currency, financial balances | High-speed scientific simulation |
| **`DOUBLE PRECISION`**| 8 bytes | Inexact (~15 digits)| Physics, ML weights, GIS coordinates| Money, accounting ledgers |
| **`TIMESTAMPTZ`** | 8 bytes | Microsecond UTC | Real-world event timestamps | Local abstract store opening hours |
| **`TEXT`** | 1B/4B + Data | Variable | General descriptions, titles, notes | Short fixed-length codes |

---

## Interview Questions & Deep Dives

### Q1: Why should money NEVER be stored using floating-point types (`FLOAT`, `DOUBLE PRECISION`)?
<details>
<summary>Click to view answer</summary>

Floating-point types implement the IEEE 754 standard, which represents fractional values in base-2 (binary). Just as the fraction $1/3$ cannot be represented finitely in base-10 decimal ($0.3333...$), fractions like $1/10$ ($0.10$) and $1/20$ ($0.05$) cannot be represented finitely in base-2 binary. 

As arithmetic operations accumulate, small binary rounding errors propagate, resulting in values like `$100.00000000000001` or `$99.99999999999999`. In financial systems, balance checks (`credit = debit`), tax calculations, and reconciliation audits fail. Financial currency must strictly be stored in exact decimal types (`NUMERIC` / `DECIMAL`) or as integer cents in `BIGINT`.
</details>

### Q2: What is the difference between `VARCHAR(255)` and `TEXT` in PostgreSQL?
<details>
<summary>Click to view answer</summary>

Under the hood in PostgreSQL, `VARCHAR(n)` and `TEXT` are physically identical: both use the `varlena` internal storage format (a variable-length structure with a 1-byte or 4-byte header depending on length). 

`VARCHAR(255)` provides **zero performance, storage, or indexing advantage** over `TEXT`. The only difference is that `VARCHAR(255)` introduces an implicit check constraint validating that string length does not exceed 255 characters. The widespread use of `VARCHAR(255)` is a historical relic from early versions of MySQL where lengths over 255 required separate off-page storage.
</details>

### Q3: What does `TIMESTAMPTZ` actually store on disk in PostgreSQL?
<details>
<summary>Click to view answer</summary>

Contrary to its name, `TIMESTAMPTZ` does **not** store a timezone string or offset on disk! It physically stores an **8-byte integer representing microseconds since 2000-01-01 00:00:00 UTC**. 

When a client inserts a timestamp with an offset (e.g., `'2026-06-01 10:00:00-05'`), PostgreSQL converts the time to UTC before writing to disk. When queried, PostgreSQL inspects the current client connection's `timezone` configuration setting, converts the stored UTC value into that local timezone, and outputs the formatted string.
</details>

### Q4: When would you use `TIMESTAMP WITHOUT TIME ZONE` instead of `TIMESTAMPTZ`?
<details>
<summary>Click to view answer</summary>

`TIMESTAMP WITHOUT TIME ZONE` is appropriate only when representing an abstract, local "wall-clock" concept that is deliberately decoupled from any specific point in physical time:
1. "The company holiday party is at 18:00 local time in every regional office."
2. "Store opening hours are 09:00 to 21:00 regardless of the store's geographic location."
For recording actual real-world occurrences (when a customer purchased an item or when a server threw an exception), `TIMESTAMPTZ` is always required.
</details>

### Q5: What is the architectural downside of using native PostgreSQL `ENUM` types?
<details>
<summary>Click to view answer</summary>

Native PostgreSQL `ENUM` types are rigid catalog objects:
1. **Cannot Remove or Rename Values Easily**: Prior to PostgreSQL 12, renaming was impossible; dropping an enum value requires creating a new type, updating all dependent tables, and dropping the old type, taking heavy table locks.
2. **Schema Coupling**: Enum definitions are global catalog types in `pg_type`, which complicates multi-tenant schema isolation and migration tooling.
A `VARCHAR` column with a `CHECK` constraint or a separate foreign key lookup table is significantly more flexible for evolving business domains.
</details>

### Q6: How does column order impact the physical size of a PostgreSQL table?
<details>
<summary>Click to view answer</summary>

PostgreSQL aligns column values on physical memory boundaries (8 bytes for `BIGINT`/`TIMESTAMPTZ`, 4 bytes for `INT`, 2 bytes for `SMALLINT`, 1 byte for `CHAR`). If smaller columns are interleaved between larger columns, the engine must insert empty padding bytes to maintain alignment. 

By grouping 8-byte columns first, followed by 4-byte, 2-byte, and 1-byte columns, alignment padding is minimized, reducing physical table size on disk and fitting more tuples into each 8KB buffer pool page.
</details>

### Q7: What is the Year 2038 Problem and which SQL data types are vulnerable?
<details>
<summary>Click to view answer</summary>

The Year 2038 problem occurs in systems that represent timestamps as a **signed 32-bit integer** counting seconds since the Unix Epoch (1970-01-01). On January 19, 2038, the integer overflows into negative values. 
- In MySQL, the legacy `TIMESTAMP` data type is a 32-bit integer and **will overflow in 2038**! (MySQL `DATETIME` is safe).
- PostgreSQL's `TIMESTAMP` and `TIMESTAMPTZ` use signed **64-bit integers** with microsecond precision, which are safe for the next 292,000 years.
</details>

### Q8: What is the difference between `UUID` and `TEXT` for storing identifiers?
<details>
<summary>Click to view answer</summary>

- `UUID` stores identifiers in **16 bytes** of binary storage (128 bits). Index comparisons are fast 64-bit integer comparisons in hardware.
- `TEXT` stores UUIDs as a **36-byte string** (e.g., `'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'`) plus a 1-byte length header. Index comparisons require slow character-by-character string collation.
Storing UUIDs as `TEXT` wastes more than 2x the disk space and severely degrades B-Tree search performance.
</details>

---

## Practice Exercises

### Exercise 1: Safe Currency Column DDL
Write the DDL for an e-commerce `invoices` table storing `subtotal`, `tax_rate`, and `total_amount` ensuring absolute mathematical precision and preventing negative amounts.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE invoices (
    invoice_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    subtotal NUMERIC(12,2) NOT NULL CHECK (subtotal >= 0.00),
    tax_rate NUMERIC(5,4) NOT NULL CHECK (tax_rate BETWEEN 0.0000 AND 1.0000),
    total_amount NUMERIC(12,2) NOT NULL CHECK (total_amount >= subtotal),
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```
</details>

---

### Exercise 2: Timezone Conversion Query
Given a table `flight_departures(flight_id INT, departure_time TIMESTAMPTZ)`. A flight departs at `'2026-07-04 14:00:00+00'`. Write a query displaying the departure time formatted for passengers in `'America/Chicago'` (UTC-5) and `'Europe/Paris'` (UTC+2).

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    flight_id,
    departure_time AT TIME ZONE 'America/Chicago' AS chicago_local_time,
    departure_time AT TIME ZONE 'Europe/Paris' AS paris_local_time
FROM flight_departures;
```
</details>

---

### Exercise 3: Optimize Column Alignment Order
Reorder the columns of this table to achieve optimal physical memory alignment and minimize padding bloat in PostgreSQL:
```sql
CREATE TABLE messy_table (
    flag_a BOOLEAN,       -- 1 byte
    amount NUMERIC(12,2), -- Variable
    flag_b BOOLEAN,       -- 1 byte
    user_id BIGINT,       -- 8 bytes
    status_code SMALLINT, -- 2 bytes
    created_at TIMESTAMPTZ-- 8 bytes
);
```

<details>
<summary>Click to view solution</summary>

```sql
-- Optimal packing: 8-byte types first, then smaller fixed types, then variable:
CREATE TABLE optimized_table (
    user_id BIGINT,        -- 8 bytes
    created_at TIMESTAMPTZ,-- 8 bytes
    status_code SMALLINT,  -- 2 bytes
    flag_a BOOLEAN,        -- 1 byte
    flag_b BOOLEAN,        -- 1 byte (2+1+1 = 4 bytes; packs cleanly!)
    amount NUMERIC(12,2)   -- Variable length (varlena) at end
);
```
</details>

---

### Exercise 4: Flexible Status Check Constraint
Write a DDL statement for `support_tickets(ticket_id, status)` that avoids rigid native `ENUM` types while strictly enforcing valid statuses: `'OPEN'`, `'IN_PROGRESS'`, `'PENDING_USER'`, `'RESOLVED'`, `'CLOSED'`.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE support_tickets (
    ticket_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    status VARCHAR(20) NOT NULL DEFAULT 'OPEN',
    CONSTRAINT chk_ticket_status CHECK (
        status IN ('OPEN', 'IN_PROGRESS', 'PENDING_USER', 'RESOLVED', 'CLOSED')
    )
);
```
</details>

---

### Exercise 5: Spot the Precision Bug
An engineer writes:
```sql
CREATE TABLE sensor_readings (
    reading_id INT PRIMARY KEY,
    latitude FLOAT NOT NULL,
    longitude FLOAT NOT NULL,
    voltage REAL NOT NULL
);
```
Evaluate whether `FLOAT` and `REAL` are appropriate for this domain.

<details>
<summary>Click to view solution</summary>

**Evaluation**: In this domain (physical sensor telemetry and geographic coordinates), `FLOAT` and `REAL` are **acceptable and appropriate**. 

Physical sensor readings (voltage, temperature) and spatial coordinates naturally exhibit continuous analog variance where binary floating-point rounding errors (e.g., $0.000001$ variation) are completely within acceptable tolerance thresholds. Unlike financial currency, sensor telemetry does not require exact decimal arithmetic.
</details>

---

### Exercise 6: Fix the Missing UTC Normalization
An application logs user logins using `CURRENT_TIMESTAMP::timestamp`. Explain what happens when daylight saving time occurs in London, and provide the fix.

<details>
<summary>Click to view solution</summary>

**Defect**: Casting to plain `timestamp` (`WITHOUT TIME ZONE`) strips the UTC offset. When London transitions from British Summer Time (UTC+1) to Greenwich Mean Time (UTC+0) in autumn, the local clock is set backward by 1 hour. Any logins occurring during that hour will appear to have occurred *before* logins that actually happened 45 minutes earlier, corrupting security audit timelines.

**Fix**: Never cast away the timezone; store as pure `TIMESTAMPTZ`:
```sql
login_timestamp TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                  DATA TYPES CHEAT SHEET                                 |
+-------------------+----------------------------+----------------------------------------+
| Data Type         | Physical Storage           | Golden Rule                            |
+-------------------+----------------------------+----------------------------------------+
| NUMERIC(p,s)      | Variable exact decimal     | Mandatory for money, taxes, balances   |
| BIGINT            | 8 bytes exact integer      | Mandatory for primary keys & counters  |
| FLOAT / REAL      | 4B / 8B binary inexact     | Acceptable for sensors/science; NO MONEY|
| TIMESTAMPTZ       | 8 bytes UTC integer        | Mandatory for 99% of event timestamps  |
| TEXT              | 1B/4B + UTF-8 payload      | Preferred over VARCHAR(255) in Postgres|
| VARCHAR + CHECK   | Variable + check rule      | Superior to rigid native database ENUMs|
+-------------------+----------------------------+----------------------------------------+
```

- **Money Rule**: `NUMERIC(p,s)` only. Never `FLOAT`.
- **Time Rule**: `TIMESTAMPTZ` only. Normalized to UTC on disk.
- **Key Rule**: `BIGINT` only. Avoid 32-bit `INT` overflow.

---

## Further Reading
- PostgreSQL 17 Documentation: *Data Types - Chapter 8*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 4.4 `<numeric types>`
- David Wheeler: *PostgreSQL Alignment Padding and Physical Row Layout*
- Markus Winand: *Modern SQL - The Truth About Timestamps with Time Zone*
