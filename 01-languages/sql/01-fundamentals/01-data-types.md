# 01: Relational Data Types

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [00-setup/01-what-is-sql-and-the-relational-model.md](../00-setup/01-what-is-sql-and-the-relational-model.md) |
| **Target Audience** | Software Engineers, Data Architects, Systems Programmers |
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

In relational database systems, a data type defines the domain of valid values an attribute can hold, the physical binary encoding used to persist it to disk pages, the CPU instructions used to compare and sort it, and the mathematical operations permitted upon it. Choosing appropriate data types is the single most critical structural decision during schema design.

A poorly chosen data type creates irreversible cascading problems:
- Using `VARCHAR(255)` for an IP address wastes 80% of storage and defeats fast bitwise subnet lookups.
- Using `FLOAT` for financial balances causes compounding IEEE-754 decimal rounding errors that throw accounting balances off by pennies.
- Using `TIMESTAMP WITHOUT TIME ZONE` in a distributed global system creates ambiguous timestamps that make cross-timezone chronological analysis impossible.
- Oversizing integer types (`BIGINT` where `SMALLINT` suffices) bloats table pages and B-Tree indexes, cutting buffer cache hit ratios in half.

Understanding how database engines physically encode integers, arbitrary-precision decimals, variable-length text, and timestamps allows engineers to design lean, high-throughput, and logically sound relational schemas.

---

## 2. Core Concepts

### 1. Integer Domains & Range Limits
Relational engines provide fixed-width two's-complement binary integer types:

| Type | Storage Size | Minimum Value | Maximum Value | Recommended Use Case |
|:---|:---:|:---|:---|:---|
| **`SMALLINT`** (`INT2`) | 2 bytes (16 bits) | $-32,768$ | $+32,767$ | Status codes, country codes, small enum mappings |
| **`INTEGER`** (`INT4`) | 4 bytes (32 bits) | $-2,147,483,648$ | $+2,147,483,647$ (~2.14 Billion) | Normal surrogate IDs, quantities, counts |
| **`BIGINT`** (`INT8`) | 8 bytes (64 bits) | $-9.22 \times 10^{18}$ | $+9.22 \times 10^{18}$ (~9.22 Quintillion) | High-volume primary keys, audit logs, financial ledger IDs |

### 2. Exact Numeric (`NUMERIC`/`DECIMAL`) vs Inexact Floating-Point (`REAL`/`DOUBLE PRECISION`)
- **Exact Arbitrary-Precision (`NUMERIC(p, s)` / `DECIMAL(p, s)`)**:
  - `p` (Precision): Total count of significant digits across the entire number (before and after decimal).
  - `s` (Scale): Exact count of fractional digits following the decimal point.
  - Stored as base-10,000 or binary-coded decimal digits. Zero rounding error. Mandatory for monetary currency, interest rates, and tax calculations.
- **Inexact Binary Floating Point (`REAL`, `DOUBLE PRECISION`, `FLOAT`)**:
  - Encoded using IEEE-754 standard binary floating-point representation (sign, mantissa, exponent).
  - Approximates fractional numbers in binary base-2. Fast hardware-accelerated CPU floating-point unit (FPU) math, but subject to representation inaccuracy (e.g., $0.1 + 0.2 \neq 0.3$). Ideal for scientific telemetry, geographical coordinates, and machine learning vectors.

### 3. Character & Text Types (`CHAR`, `VARCHAR`, `TEXT`)
- **`CHAR(n)` (Fixed Length)**: Always pads stored strings with spaces up to length $n$. Rarely useful in modern applications except for fixed-length hashes or ISO codes (e.g., `CHAR(2)` for ISO country codes).
- **`VARCHAR(n)` (Variable Length with Limit)**: Stores strings up to maximum $n$ characters with a 1-byte or 4-byte length prefix. Trailing spaces are preserved.
- **`TEXT` / `VARCHAR` (Unbounded)**: In PostgreSQL and SQLite, `TEXT` and unbounded `VARCHAR` have identical underlying physical implementations. Variable-length strings of arbitrary size up to 1GB.

### 4. Temporal Types & Timezones
- **`TIMESTAMP WITH TIME ZONE` (`TIMESTAMPTZ`)**: Standard practice. The engine normalizes incoming timestamps to UTC for physical storage. When retrieved, it formats the timestamp into the client session’s local timezone.
- **`TIMESTAMP WITHOUT TIME ZONE` (`TIMESTAMP`)**: Stores the literal year/month/day/hour/minute/second without any geographical or UTC offset reference. Fragile in distributed architectures.
- **`DATE`**: 4-byte calendar date (`YYYY-MM-DD`).
- **`TIME`**: Time of day without date.
- **`INTERVAL`**: Duration of elapsed time (e.g., `'1 year 2 months 3 days 4 hours'`).

---

## 3. How It Works Internally

### Tuple Disk Layout & Data Alignment
In storage engines like PostgreSQL, each tuple on an 8KB disk page consists of a **Tuple Header** (23 bytes in Postgres) followed by column attributes aligned to memory boundaries:

```
Slotted Page Layout (PostgreSQL 8KB Page)
+--------------------------------------------------------------------------+
| Page Header (24 bytes: LSN, checksum, free space offsets)                |
| [Line Pointer 1] [Line Pointer 2] [Line Pointer 3] ...                   |
|                      ▼ FREE SPACE (grows inward) ▲                       |
| ... [Tuple 3 Payload] [Tuple 2 Payload] [Tuple 1 Payload]                |
+--------------------------------------------------------------------------+

Tuple 1 Internal Byte Layout:
+--------------------------------------------------------------------------+
| Header (23B) | Pad | id (INT4: 4B) | Pad | created_at (TIMESTAMPTZ: 8B)  |
|              |     | offset 24-27  | 4B  | offset 32-39 (8-byte aligned) |
+--------------------------------------------------------------------------+
```

- **CPU Data Alignment**: 8-byte types (`BIGINT`, `TIMESTAMPTZ`, `DOUBLE PRECISION`) must begin at byte offsets divisible by 8. Placing a 2-byte `SMALLINT` immediately before a `BIGINT` forces the engine to insert 6 bytes of empty padding! Structuring table columns in descending size order (`8-byte` $\rightarrow$ `4-byte` $\rightarrow$ `2-byte` $\rightarrow$ `1-byte` $\rightarrow$ `variable`) can reduce table size on disk by up to 20%.

### TOAST (The Oversized-Attribute Storage Technique)
PostgreSQL physical page size is 8KB, and tuples cannot cross page boundaries. When variable-length columns (`TEXT`, `JSONB`, `BYTEA`) exceed ~2KB, PostgreSQL triggers **TOAST**:
1. First, it attempts in-line compression (using LZ4 or pglz).
2. If still too large, it moves the data out of the main table heap into an auxiliary TOAST table, splitting it into 2KB chunks, leaving behind a 18-byte TOAST pointer in the original row.

---

## 4. Syntax & Usage

### Canonical DDL Exhibiting Strict Typing

```sql
CREATE TABLE comprehensive_types_demo (
    -- Integer Types
    small_id SMALLINT NOT NULL,
    standard_id INTEGER NOT NULL,
    big_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,

    -- Exact Monetary & Numeric
    unit_cost NUMERIC(12, 4) NOT NULL,
    retail_price NUMERIC(10, 2) NOT NULL,

    -- Inexact Scientific Floating Point
    temperature REAL NOT NULL,
    latitude DOUBLE PRECISION NOT NULL,

    -- Textual Types
    iso_country CHAR(2) NOT NULL,
    slug VARCHAR(100) NOT NULL,
    description TEXT,

    -- Temporal Types
    scheduled_date DATE NOT NULL,
    active_window INTERVAL NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL,

    -- Boolean & Binary
    is_verified BOOLEAN DEFAULT FALSE NOT NULL,
    raw_payload BYTEA
);
```

### Multi-Dialect Comparison Matrix

| Abstract Data Type | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **64-bit Integer** | `BIGINT` | `BIGINT` | `INTEGER` (64-bit signed) | `BIGINT` |
| **Exact Monetary** | `NUMERIC(p,s)` | `DECIMAL(p,s)` | `REAL` (or stored as text/int) | `DECIMAL(p,s)` or `MONEY` |
| **Double Precision** | `DOUBLE PRECISION` | `DOUBLE` | `REAL` | `FLOAT(53)` |
| **Variable Text** | `VARCHAR(n)` or `TEXT` | `VARCHAR(n)` or `TEXT` | `TEXT` | `VARCHAR(n)` or `NVARCHAR(n)` |
| **Timestamp with TZ** | `TIMESTAMPTZ` (UTC normalized)| `TIMESTAMP` (auto converts to UTC)| Text ISO-8601 string | `DATETIMEOFFSET` |
| **Boolean** | `BOOLEAN` (`TRUE`/`FALSE`)| `TINYINT(1)` synonym | `INTEGER` (`1`/`0`) | `BIT` (`1`/`0`) |
| **Binary BLOB** | `BYTEA` | `BLOB` or `LONGBLOB` | `BLOB` | `VARBINARY(MAX)` |
| **UUID** | Native `UUID` (16 bytes) | `BINARY(16)` or text | Text representation | `UNIQUEIDENTIFIER` |

---

## 5. 5 Practical Progressive Examples

### Example 1: Demonstrating IEEE-754 Floating Point Inaccuracy
Why exact decimal types must be used for money instead of floating-point numbers:

```sql
CREATE TABLE currency_comparison (
    id INTEGER PRIMARY KEY,
    float_val DOUBLE PRECISION,
    numeric_val NUMERIC(10, 2)
);

INSERT INTO currency_comparison VALUES
(1, 0.1, 0.10),
(2, 0.2, 0.20);

-- Query the sum
SELECT 
    SUM(float_val) AS float_sum,
    SUM(numeric_val) AS numeric_sum,
    CASE WHEN SUM(float_val) = 0.3 THEN 'EQUAL' ELSE 'NOT EQUAL' END AS float_check,
    CASE WHEN SUM(numeric_val) = 0.30 THEN 'EQUAL' ELSE 'NOT EQUAL' END AS numeric_check
FROM currency_comparison;
```
```text
+---------------------+-------------+-------------+---------------+
| float_sum           | numeric_sum | float_check | numeric_check |
+---------------------+-------------+-------------+---------------+
| 0.30000000000000004 |        0.30 | NOT EQUAL   | EQUAL         |
+---------------------+-------------+-------------+---------------+
```
*(Notice the float sum accumulates IEEE-754 binary representation error: `0.30000000000000004`, causing direct equality comparisons to fail! `NUMERIC` remains exact).*

### Example 2: Timezone Normalization with `TIMESTAMPTZ`
Demonstrating how `TIMESTAMPTZ` normalizes incoming timestamps across different global clients:

```sql
CREATE TABLE audit_events (
    event_id INTEGER PRIMARY KEY,
    event_name VARCHAR(50),
    logged_at TIMESTAMP WITH TIME ZONE
);

-- Insert from New York (UTC-5)
INSERT INTO audit_events VALUES 
(1, 'Deploy Start', '2024-06-01 10:00:00-05');

-- Insert from Tokyo (UTC+9)
INSERT INTO audit_events VALUES 
(2, 'Deploy End', '2024-06-01 23:30:00+09');

-- Set client session timezone to UTC and query
SET timezone = 'UTC';
SELECT event_id, event_name, logged_at FROM audit_events ORDER BY logged_at;
```
```text
+----------+--------------+------------------------+
| event_id | event_name   | logged_at              |
+----------+--------------+------------------------+
|        2 | Deploy End   | 2024-06-01 14:30:00+00 |
|        1 | Deploy Start | 2024-06-01 15:00:00+00 |
+----------+--------------+------------------------+
```
*(Deploy End actually occurred 30 minutes BEFORE Deploy Start in real universal time, which `TIMESTAMPTZ` correctly reflects by normalizing both to UTC).*

### Example 3: Edge Case (Integer Overflow Failures)
What happens when a 4-byte `INTEGER` primary key exceeds its capacity:

```sql
CREATE TABLE counter (
    id INTEGER PRIMARY KEY
);

INSERT INTO counter VALUES (2147483647); -- Max 32-bit signed integer

-- Attempting to increment by 1
UPDATE counter SET id = id + 1 WHERE id = 2147483647;
```
```text
ERROR: integer out of range
```
*(High-volume tables such as event logs, financial transactions, or user metrics must always use `BIGINT` for primary keys to avoid running out of IDs in production).*

### Example 4: Failure Mode (SQLite Dynamic Type Affinity Surprise)
Demonstrating SQLite's unique type affinity where invalid data is silently accepted unless `STRICT` mode is declared:

In SQLite (Standard Table):
```sql
CREATE TABLE loose_users (
    user_id INTEGER PRIMARY KEY,
    age INTEGER,
    registered_at DATE
);

-- SQLite allows strings and floats into INTEGER/DATE columns!
INSERT INTO loose_users VALUES (1, 'twenty-five', 'INVALID_DATE');
SELECT user_id, age, typeof(age), registered_at, typeof(registered_at) FROM loose_users;
```
```text
+---------+-------------+-------------+---------------+----------------------+
| user_id | age         | typeof(age) | registered_at | typeof(registered_at)|
+---------+-------------+-------------+---------------+----------------------+
|       1 | twenty-five | text        | INVALID_DATE  | text                 |
+---------+-------------+-------------+---------------+----------------------+
```

In SQLite (Modern `STRICT` Table):
```sql
CREATE TABLE strict_users (
    user_id INTEGER PRIMARY KEY,
    age INTEGER
) STRICT;

INSERT INTO strict_users VALUES (1, 'twenty-five');
-- ERROR: cannot store TEXT value in INT column strict_users.age
```

### Example 5: Optimized Column Ordering (Eliminating Padding Waste)
Restructuring table columns to minimize alignment padding:

```sql
-- Suboptimal layout: 32 bytes per row due to padding alignment gaps
CREATE TABLE unaligned_table (
    col1 SMALLINT, -- 2 bytes + 6 bytes padding
    col2 BIGINT,   -- 8 bytes
    col3 SMALLINT, -- 2 bytes + 6 bytes padding
    col4 BIGINT    -- 8 bytes
);

-- Optimized layout: 24 bytes per row (8-byte types first, then 2-byte types)
CREATE TABLE aligned_table (
    col2 BIGINT,   -- 8 bytes
    col4 BIGINT,   -- 8 bytes
    col1 SMALLINT, -- 2 bytes
    col3 SMALLINT  -- 2 bytes (packed together, 4 bytes padding at end)
);
```
On a 100,000,000-row table, saving 8 bytes per tuple saves 800MB of RAM buffer pool space and reduces disk I/O.

---

## 6. Best Practices

1. **Always Use `BIGINT` for Auto-Incrementing Primary Keys**: A 32-bit `INTEGER` caps out at 2.14 billion rows. Migrating a live production table from `INT` to `BIGINT` locks the table for hours. Start with `BIGINT`.
2. **Always Use `TIMESTAMPTZ` Over `TIMESTAMP`**: Store every point in time with timezone awareness. Let the database persist UTC internally and convert to the user’s timezone at query time.
3. **Use `NUMERIC(p, s)` for Monetary Values**: Never use `FLOAT`, `DOUBLE`, or `REAL` for financial accounting. Always declare explicit precision and scale (e.g., `NUMERIC(14, 2)` for dollars and cents).
4. **Prefer `TEXT` over `VARCHAR(255)` in PostgreSQL**: The mythical 255-character limit is a historical MySQL 5.x artifact. In PostgreSQL, `VARCHAR(n)` and `TEXT` have identical storage performance; `VARCHAR(n)` merely adds a check constraint that must be evaluated on every insert.
5. **Use Native `BOOLEAN`**: Avoid integer flags (`is_active INT`) when your engine supports true `BOOLEAN`. Booleans simplify query logic (`WHERE is_active`) and prevent storing invalid values like `2` or `-1`.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Using `FLOAT` / `REAL` for Currency** | Binary IEEE-754 floating point cannot represent base-10 fractions accurately, accumulating rounding drift. | Use `NUMERIC(p, s)` or `DECIMAL(p, s)`. |
| **Using `INT` for Phone Numbers or Zip Codes** | Phone numbers have leading zeros (`01234`) and can exceed 32-bit integers; ZIP codes can contain hyphens (`10001-1234`). | Store phone numbers and postal codes as `VARCHAR(20)`. |
| **Using `TIMESTAMP WITHOUT TIME ZONE`** | Strips daylight saving time and UTC offset context, causing ambiguous historical times when daylight saving shifts. | Use `TIMESTAMP WITH TIME ZONE` (`TIMESTAMPTZ`). |
| **Using `CHAR(n)` for Variable Data** | `CHAR(50)` pads short strings with spaces, consuming unnecessary disk space and causing trailing space comparison surprises. | Use `VARCHAR(n)` or `TEXT`. |
| **Storing JSON Strings in Raw `TEXT`** | Plain `TEXT` cannot be indexed internally, validated for JSON syntax, or queried with path operators. | Use native `JSONB` in PostgreSQL or `JSON` with generated columns in MySQL. |
| **Ignoring Column Order in High-Volume Tables** | Interleaving 2-byte and 8-byte types creates alignment padding waste in every row. | Group 8-byte types together first, followed by 4-byte, 2-byte, and variable types. |

---

## 8. Performance & Security Considerations

### Performance
- **Index Size vs Column Width**: The narrower the data type, the more index entries fit into a single 8KB B-Tree page. A B-Tree on `BIGINT` (8 bytes) has higher fan-out and fewer tree levels than a B-Tree on `UUID` stored as text (36 bytes).
- **TOAST Overhead**: Reading rows with large TOAST attributes that are not needed in the query forces extra disk seeks. Always avoid `SELECT *` on tables containing wide text or JSONB columns.

### Security
- **Domain Validation as Defense-in-Depth**: Strict data types prevent many classes of injection attacks. An `INTEGER` or `DATE` parameter in a prepared statement physically rejects strings containing SQL fragments before the query reaches the planner.

---

## 9. Comparison Matrix

### Numeric & String Type Trade-Offs

| Type | Storage Overhead | Math Speed | Precision Guarantee | Best Used For |
|:---|:---:|:---:|:---:|:---|
| **`INTEGER` / `BIGINT`** | 4 / 8 bytes | Ultra-fast (native CPU ALU) | Exact integer | Primary keys, foreign keys, row counts, discrete units |
| **`NUMERIC(p,s)`** | Variable (~2B per 4 digits) | Moderate (software BCD math)| Exact decimal | Currency, financial ledgers, tax computations |
| **`DOUBLE PRECISION`** | 8 bytes | Ultra-fast (hardware FPU) | Inexact binary float | Scientific simulations, GPS coordinates, machine learning |
| **`VARCHAR(n)`** | String bytes + 1-4B header | Fast string comparison | Exact character limit | Bounded strings (slugs, usernames, emails) |
| **`TEXT`** | String bytes + 1-4B header | Fast string comparison | Unbounded (up to 1GB) | Long articles, markdown notes, unbounded comments |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: Why does `0.1 + 0.2 = 0.3` evaluate to false when using `DOUBLE PRECISION`, and how should monetary calculations be structured in SQL?
<details>
<summary>Answer</summary>
Computers represent `DOUBLE PRECISION` using IEEE-754 binary floating-point numbers. Just as the fraction $1/3$ cannot be represented as a terminating decimal in base-10 ($0.3333...$), fractions like $1/10$ ($0.1$) cannot be represented as terminating fractions in binary base-2 ($0.00011001100..._2$). When stored in 64 bits, binary truncation occurs. Adding $0.1 + 0.2$ results in $0.3000000000000000444...$, which does not equal $0.3$.

Monetary calculations must always use exact numeric types: `NUMERIC(precision, scale)` or `DECIMAL(precision, scale)`. These types store digits in base-10 (or base-10,000), eliminating binary representation drift.
</details>

#### Q2: What is the physical difference between `TIMESTAMP WITH TIME ZONE` (`TIMESTAMPTZ`) and `TIMESTAMP WITHOUT TIME ZONE` in PostgreSQL?
<details>
<summary>Answer</summary>
Both types take exactly 8 bytes of storage on disk. Contrary to common belief, `TIMESTAMPTZ` does NOT store a timezone offset alongside the timestamp!

- `TIMESTAMPTZ`: When an input string with a timezone offset (e.g., `'2024-01-01 12:00:00-05'`) arrives, PostgreSQL converts it immediately to universal UTC and stores the 8-byte integer representing microseconds since 2000-01-01 UTC. When outputting, it converts UTC to the client session's `TimeZone` setting.
- `TIMESTAMP WITHOUT TIME ZONE`: Stores the literal values given without timezone conversion. If inserted as `'12:00:00'`, it remains `'12:00:00'` regardless of what timezone the viewing client is in.
</details>

#### Q3: What is TOAST in PostgreSQL, and under what conditions does the engine move a column out-of-line?
<details>
<summary>Answer</summary>
TOAST (The Oversized-Attribute Storage Technique) is PostgreSQL's mechanism for storing large field values that exceed the physical capacity of an 8KB disk block. Because tuples cannot cross page boundaries, when a row exceeds `TOAST_TUPLE_THRESHOLD` (normally 2KB), the engine takes action on variable-length columns:
1. It attempts in-line compression (using LZ4 or pglz).
2. If the row is still too wide, it moves large column values out-of-line into an auxiliary TOAST table, splitting the data into 2KB chunks.
3. The original row in the heap table retains only an 18-byte pointer (containing the TOAST table OID and chunk ID).
</details>

#### Q4: What is data alignment in database storage, and how can column ordering impact disk footprint?
<details>
<summary>Answer</summary>
CPUs access memory more efficiently when data types are aligned to memory addresses that are multiples of their size (e.g., an 8-byte `BIGINT` must start at an address divisible by 8). Storage engines insert padding bytes between tuple attributes to ensure proper alignment.

If a table schema alternates between 2-byte `SMALLINT` and 8-byte `BIGINT` columns, the engine must insert 6 bytes of empty padding before every `BIGINT`. Ordering columns from largest to smallest (8-byte $\rightarrow$ 4-byte $\rightarrow$ 2-byte $\rightarrow$ 1-byte $\rightarrow$ variable) ensures natural alignment with zero padding bytes, saving significant disk and memory buffer space.
</details>

### Practical Type Conversion & Schema Puzzles

#### Q5: Write an SQL query to convert a Unix epoch timestamp integer (e.g., `1717243200`) into a human-readable `TIMESTAMPTZ` in UTC.
<details>
<summary>Answer</summary>

```sql
-- Standard PostgreSQL:
SELECT to_timestamp(1717243200) AT TIME ZONE 'UTC' AS converted_time;

-- Standard ANSI SQL / SQLite:
SELECT datetime(1717243200, 'unixepoch') AS converted_time;

-- MySQL:
SELECT FROM_UNIXTIME(1717243200) AS converted_time;
```
</details>

#### Q6: How do you safely alter a column from `VARCHAR(100)` to `INTEGER` in PostgreSQL when some rows contain numeric strings?
<details>
<summary>Answer</summary>
Use the `USING` clause with explicit casting:

```sql
ALTER TABLE user_data 
ALTER COLUMN legacy_code TYPE INTEGER 
USING legacy_code::integer;
```
If dirty text (e.g., `'N/A'`) might exist, use a regex guard:
```sql
ALTER TABLE user_data 
ALTER COLUMN legacy_code TYPE INTEGER 
USING (
    CASE 
        WHEN legacy_code ~ '^[0-9]+$' THEN legacy_code::integer 
        ELSE NULL 
    END
);
```
</details>

#### Q7: Write a query to detect the byte size of data stored in a variable-length column vs its character length.
<details>
<summary>Answer</summary>

```sql
SELECT 
    product_name,
    LENGTH(product_name) AS character_count,
    OCTET_LENGTH(product_name) AS byte_size
FROM products;
```
*(In UTF-8, multi-byte characters like emojis or Cyrillic/Asian scripts have `OCTET_LENGTH > LENGTH`).*
</details>

#### Q8: Why should UUIDs be stored as native 16-byte `UUID` types instead of 36-character `VARCHAR(36)` strings?
<details>
<summary>Answer</summary>
A UUID is a 128-bit number. 
- Stored as native `UUID`: It requires exactly 16 bytes on disk and in memory.
- Stored as `VARCHAR(36)` (e.g., `'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'`): It requires 36 bytes of text plus string header overhead (37–40 bytes total).

Storing UUIDs as text more than doubles the storage size of primary key B-Tree indexes, drastically reducing cache efficiency and slowing down join comparisons.
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Strict Monetary Column Definition
Define a table `invoices` with an auto-incrementing 64-bit ID, an invoice number (up to 30 chars, required), a subtotal exact to 2 decimal places, and an issue date.
<details>
<summary>Solution</summary>

```sql
CREATE TABLE invoices (
    invoice_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    invoice_number VARCHAR(30) NOT NULL UNIQUE,
    subtotal NUMERIC(12, 2) NOT NULL CHECK (subtotal >= 0.00),
    issue_date DATE DEFAULT CURRENT_DATE NOT NULL
);
```
</details>

#### Exercise 2: Date Math Operations
Retrieve all records from `invoices` issued in the last 90 days relative to the current timestamp.
<details>
<summary>Solution</summary>

```sql
SELECT invoice_id, invoice_number, issue_date, subtotal
FROM invoices
WHERE issue_date >= CURRENT_DATE - INTERVAL '90 days'
ORDER BY issue_date DESC;
```
</details>

### Intermediate Exercises

#### Exercise 3: Timezone Conversion Across Regions
Write a query converting a stored `TIMESTAMPTZ` column into three distinct local times: New York (`America/New_York`), London (`Europe/London`), and Tokyo (`Asia/Tokyo`).
<details>
<summary>Solution</summary>

```sql
SELECT 
    event_name,
    logged_at AT TIME ZONE 'UTC' AS time_utc,
    logged_at AT TIME ZONE 'America/New_York' AS time_new_york,
    logged_at AT TIME ZONE 'Europe/London' AS time_london,
    logged_at AT TIME ZONE 'Asia/Tokyo' AS time_tokyo
FROM audit_events;
```
</details>

#### Exercise 4: Explicit Cast Error Prevention
Write a query that parses a column `raw_input TEXT` into a numeric value, returning `NULL` instead of throwing an error if the string is not a valid number.
<details>
<summary>Solution</summary>

```sql
-- In PostgreSQL:
SELECT 
    raw_input,
    CASE 
        WHEN raw_input ~ '^-?[0-9]+(\.[0-9]+)?$' THEN raw_input::numeric 
        ELSE NULL 
    END AS safe_numeric
FROM raw_payloads;
```
</details>

### Advanced Exercises

#### Exercise 5: Calculating Table Storage Footprint by Type
Write a query inspecting `pg_column_size` to determine the physical byte size on disk of individual attributes within a row.
<details>
<summary>Solution</summary>

```sql
SELECT 
    product_name,
    pg_column_size(product_id) AS id_bytes,
    pg_column_size(retail_price) AS price_bytes,
    pg_column_size(product_name) AS name_bytes,
    pg_column_size(t.*) AS total_row_bytes
FROM products t
LIMIT 5;
```
</details>

#### Exercise 6: Micro-Optimizing Column Layout
Given the following unaligned table:
```sql
CREATE TABLE messy (
    c1 BOOLEAN,
    c2 BIGINT,
    c3 SMALLINT,
    c4 DOUBLE PRECISION,
    c5 INTEGER
);
```
Rewrite the `CREATE TABLE` statement reordering columns to minimize alignment padding.
<details>
<summary>Solution</summary>

```sql
-- Reorder from largest alignment to smallest:
CREATE TABLE optimized_clean (
    c2 BIGINT,           -- 8 bytes (align 8)
    c4 DOUBLE PRECISION, -- 8 bytes (align 8)
    c5 INTEGER,          -- 4 bytes (align 4)
    c3 SMALLINT,         -- 2 bytes (align 2)
    c1 BOOLEAN           -- 1 byte  (align 1)
);
```
</details>

---

## 12. Summary & Quick Reference

### Data Types Cheat Sheet
- **Integers**: `SMALLINT` (2B, $\pm32k$), `INTEGER` (4B, $\pm2.1B$), `BIGINT` (8B, $\pm9.22Q$).
- **Money / Precision**: `NUMERIC(p, s)` (Exact decimal, no float drift).
- **Floats / Science**: `DOUBLE PRECISION` (8B, IEEE-754 fast binary float).
- **Strings**: `TEXT` (Unbounded, TOASTable), `VARCHAR(n)` (Bounded limit).
- **Dates & Times**: `TIMESTAMPTZ` (UTC normalized), `DATE` (Calendar day), `INTERVAL` (Duration).
- **Identifiers**: `UUID` (Native 16 bytes).

---

## 13. Further Reading
- PostgreSQL Documentation: [Chapter 8: Data Types](https://www.postgresql.org/docs/17/datatype.html)
- What Every Computer Scientist Should Know About Floating-Point Arithmetic (David Goldberg, ACM)
- PostgreSQL Wiki: [Calculating Table and Column Size](https://wiki.postgresql.org/wiki/Calculating_Page_and_Tuple_Sizes)
- [02 - Table Definition: CREATE, ALTER, DROP & TRUNCATE](./02-create-alter-drop-table.md)
