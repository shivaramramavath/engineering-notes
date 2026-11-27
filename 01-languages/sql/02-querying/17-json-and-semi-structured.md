# JSON and Semi-Structured Data in Relational Databases

| Metadata | Details |
| :--- | :--- |
| **Topic** | JSON vs JSONB Storage, Navigation Operators (`->`, `->>`, `@>`), SQL/JSON Path, and GIN Indexing |
| **Dialect** | ANSI SQL:2023 / PostgreSQL 17+ (Primary), MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Database Version** | PostgreSQL 17.2, MySQL 8.4.1, SQLite 3.50.4, SQL Server 2022 CU14 |
| **Tested On** | Python 3.14 stdlib sqlite3 engine & PostgreSQL 17 execution runner |
| **Date** | 2026-11-26 |
| **Author** | Antigravity SQL Curriculum Lead |
| **Category** | Advanced Querying & Analytical SQL |

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

Relational normalization demands fixed schemas, typed columns, and discrete tables linked by foreign keys. However, modern software engineering regularly encounters data that defies rigid tabular modeling:
- Ingesting arbitrary third-party webhook payloads (Stripe, GitHub, Shopify).
- Multi-tenant software where each tenant defines custom user attributes.
- E-commerce catalogs where a laptop has `{"cpu": "M3", "ram": "32GB"}` while a shoe has `{"size": 10.5, "color": "Navy"}`.
- Fast-evolving event streams where schema migrations cannot precede feature releases.

Historically, teams abandoned relational databases for dedicated Document Stores (MongoDB, Couchbase) to handle JSON. However, modern RDBMS engines (led by PostgreSQL 9.4+ and codified in ANSI SQL:2016 / SQL:2023) bridge this divide.

By combining relational guarantees (ACID transactions, foreign keys, secondary indexes) with native **binary JSON (`JSONB`)** storage, JSON Path query expressions, and **Generalized Inverted Indexes (GIN)**, engineers can run hybrid queries: joining relational primary keys to semi-structured documents and querying nested attributes in sub-millisecond time.

---

## Core Concepts

### 1. `JSON` vs `JSONB` in PostgreSQL
PostgreSQL offers two distinct JSON data types:
- **`JSON` (Text Storage)**: Stores an exact textual replica of the input string, including whitespace, indentation, duplicate object keys, and key ordering. It requires reparsing the entire text on every query read. It does **not** support GIN indexing.
- **`JSONB` (Decomposed Binary Storage)**: Parses the JSON upon insertion and stores it in a decomposed binary hierarchy. Whitespace is stripped, duplicate keys are eliminated (last value wins), and object keys are sorted. Reads and nested lookups are blazing fast, and documents can be indexed with GIN.
- **Rule of thumb**: Always use `JSONB` unless you have a strict requirement to preserve exact formatting or duplicate keys.

### 2. Core Extraction Operators
- **`->` (Extract as JSON/JSONB)**: Navigates by object key or array index; returns a `JSONB` object or primitive.
  ```sql
  payload -> 'address' -> 'zip' -- Returns jsonb "94107"
  ```
- **`->>` (Extract as Text)**: Navigates by object key or array index; returns the value unquoted as raw `TEXT`.
  ```sql
  payload -> 'address' ->> 'zip' -- Returns text '94107'
  ```
- **`#>` & `#>>` (Path Extraction)**: Traverses nested structures using a text array path:
  ```sql
  payload #>> '{address, location, coordinates, 0}'
  ```

### 3. Containment & Existence Operators
- **`@>` (Contains)**: Tests whether the left JSONB document contains the right JSONB structure:
  ```sql
  payload @> '{"role": "admin", "active": true}'
  ```
- **`?` (Key Exists)**: Tests whether a specific top-level string key exists:
  ```sql
  payload ? 'tax_id'
  ```
- **`?|` and `?&`**: Tests whether *any* or *all* keys in a string array exist.

### 4. SQL/JSON Path (ANSI SQL:2016 / SQL:2023)
PostgreSQL 12+ implements standard SQL/JSON Path expressions (`jsonpath`):
- `jsonb_path_exists(data, '$.items[*] ? (@.price > 100)')`
- `jsonb_path_query(data, '$.users[*].email')`

### 5. GIN Indexing for JSONB
- **`jsonb_ops` (Default)**: Indexes every key, value, and array element. Supports `@>`, `?`, `?|`, `?&`.
- **`jsonb_path_ops`**: Creates hash signatures of root-to-leaf paths. Consumes up to 50% less disk space and executes containment queries (`@>`) faster, but does **not** support key-existence (`?`).

---

## How It Works Internally

```
                   [ Input: '{"name": "Alice", "age": 30}' ]
                                       │
                                       ▼
                       [ PostgreSQL JSONB Ingestion ]
                                       │
    1. Validates syntax.
    2. Strips whitespace and duplicate keys.
    3. Serializes into binary memory header:
       [ Header | JEntry Keys: (name, age) | JEntry Values: (Alice, 30) ]
                                       │
                                       ▼
                     [ Storage on Disk: TOAST Engine ]
                     If document exceeds 2KB, compressed and
                     stored in out-of-line TOAST table.
                                       │
       ┌───────────────────────────────┴───────────────────────────────┐
       ▼                                                               ▼
[ B-Tree Index on Expression ]                          [ GIN Index on Document ]
CREATE INDEX ON tbl ((data->>'name'))                  CREATE INDEX ON tbl USING GIN (data)
Only indexes the extracted scalar value.               Deconstructs JSON into tokens:
Fastest for single predictable lookups.                - "name": "Alice"
                                                       - "age": 30
                                                       Supports arbitrary containment (@>)!
```

---

## Syntax & Usage

```sql
SELECT 
    event_id,
    -- Extract nested text
    payload -> 'user' ->> 'email' AS user_email,
    -- Extract numeric value with explicit cast
    (payload -> 'transaction' ->> 'amount')::numeric AS txn_amount,
    -- Check key existence
    payload ? 'discount_code' AS has_discount,
    -- SQL/JSON path evaluation
    jsonb_path_exists(payload, '$.items[*] ? (@.quantity > 5)') AS has_bulk_item
FROM webhook_events
WHERE payload @> '{"event_type": "CHECKOUT_COMPLETED"}';
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **Native Storage Type** | `JSON` (Text) & `JSONB` (Binary) | Native binary `JSON` | Text (with `JSON` check constraints) | Text / `NVARCHAR(MAX)` (Native `JSON` in 2024 preview) |
| **Extraction Operators** | `->` (json), `->>` (text) | `->` (json), `->>` (unquoted) | `->` (json), `->>` (text) (since 3.38+) | No native operators (Use `JSON_VALUE`, `JSON_QUERY`) |
| **Containment Operator** | `@>` (with native GIN support) | `MEMBER OF()`, `JSON_CONTAINS()` | **Not Supported** | **Not Supported** |
| **Inverted Indexing (GIN)**| Native `USING GIN (col)` | Multi-Valued Indexes on `CAST(... AS ARRAY)`| **Not Supported** (Generated cols only) | Full-Text Index or Computed Columns |
| **SQL/JSON Standard Path** | Full ANSI `jsonpath` support | `JSON_EXTRACT(doc, '$.path')` | `json_extract(doc, '$.path')` | `JSON_VALUE(doc, '$.path')` |

---

## Practical Examples

### Example 1: Basic — Storing and Querying JSONB Attributes
Store flexible customer profile settings and filter by nested attributes.

```sql
CREATE TABLE customer_profiles (
    customer_id INT PRIMARY KEY,
    full_name VARCHAR(100) NOT NULL,
    metadata JSONB NOT NULL
);

INSERT INTO customer_profiles VALUES
(1, 'Alice Smith', '{"tier": "GOLD", "preferences": {"newsletter": true, "theme": "dark"}}'),
(2, 'Bob Jones', '{"tier": "SILVER", "preferences": {"newsletter": false, "theme": "light"}}'),
(3, 'Charlie Brown', '{"tier": "GOLD", "preferences": {"newsletter": false, "theme": "system"}}');

-- Query using -> and ->>
SELECT 
    customer_id,
    full_name,
    metadata ->> 'tier' AS tier,
    (metadata -> 'preferences' ->> 'newsletter')::boolean AS subscribed_to_newsletter
FROM customer_profiles
WHERE metadata ->> 'tier' = 'GOLD';
```

```
Result:
 customer_id |   full_name   | tier | subscribed_to_newsletter 
-------------+---------------+------+--------------------------
           1 | Alice Smith   | GOLD | t
           3 | Charlie Brown | GOLD | f
(2 rows)
```

---

### Example 2: Real-World — Containment Filtering with GIN Index
Filter e-commerce catalog items by dynamic specification attributes using the `@>` operator.

```sql
CREATE TABLE product_catalog (
    sku VARCHAR(20) PRIMARY KEY,
    title VARCHAR(100) NOT NULL,
    specs JSONB NOT NULL
);

-- Create a GIN index on specs
CREATE INDEX idx_product_specs_gin ON product_catalog USING GIN (specs);

INSERT INTO product_catalog VALUES
('SKU-MAC-01', 'MacBook Pro 16', '{"brand": "Apple", "ram_gb": 32, "ports": ["usb-c", "hdmi"]}'),
('SKU-DEL-02', 'Dell XPS 15', '{"brand": "Dell", "ram_gb": 32, "ports": ["usb-c"]}'),
('SKU-THK-03', 'ThinkPad X1', '{"brand": "Lenovo", "ram_gb": 16, "ports": ["usb-c", "hdmi"]}');

-- Query using containment @> (Can use the GIN index!)
SELECT sku, title, specs ->> 'brand' AS brand
FROM product_catalog
WHERE specs @> '{"ram_gb": 32, "brand": "Apple"}';
```

```
Result:
    sku     |     title      | brand 
------------+----------------+-------
 SKU-MAC-01 | MacBook Pro 16 | Apple
(1 row)
```

---

### Example 3: Edge Case — Unpacking JSON Arrays into Relational Rows
Convert a JSON array of line items stored inside a webhook payload into discrete relational rows using `jsonb_to_recordset()`.

```sql
CREATE TABLE webhook_payloads (
    webhook_id INT PRIMARY KEY,
    raw_payload JSONB NOT NULL
);

INSERT INTO webhook_payloads VALUES
(101, '{
    "order_ref": "ORD-9901", 
    "items": [
        {"item_name": "Mechanical Keyboard", "qty": 1, "price": 120.00},
        {"item_name": "Desk Mat", "qty": 2, "price": 25.00}
    ]
}');

SELECT 
    wp.webhook_id,
    wp.raw_payload ->> 'order_ref' AS order_ref,
    item.item_name,
    item.qty,
    item.price
FROM webhook_payloads wp
CROSS JOIN LATERAL jsonb_to_recordset(wp.raw_payload -> 'items') AS item(
    item_name TEXT, 
    qty INT, 
    price NUMERIC(10,2)
);
```

```
Result:
 webhook_id | order_ref |      item_name      | qty | price  
------------+-----------+---------------------+-----+--------
        101 | ORD-9901  | Mechanical Keyboard |   1 | 120.00
        101 | ORD-9901  | Desk Mat            |   2 |  25.00
(2 rows)
```

---

### Example 4: Failure Mode — B-Tree Index Misalignment on JSONB Operators
A developer creates an index using `->>` (Text extraction) but writes the query using `@>` (JSONB containment), causing the engine to miss the index completely and perform a full sequential scan!

```sql
-- INDEX DEFINED:
CREATE INDEX idx_user_role ON customer_profiles ((metadata ->> 'role'));

-- FAILING QUERY (Cannot use idx_user_role! Performs Seq Scan):
EXPLAIN ANALYZE
SELECT * FROM customer_profiles WHERE metadata @> '{"role": "admin"}';

-- CORRECT USAGE:
-- Either use the operator that matches the B-Tree index:
SELECT * FROM customer_profiles WHERE metadata ->> 'role' = 'admin';

-- Or create a GIN index that natively supports @>:
CREATE INDEX idx_metadata_gin ON customer_profiles USING GIN (metadata);
```

---

### Example 5: Optimized — SQL Server `OPENJSON` Transformation
Unpack JSON documents into relational columns in Microsoft SQL Server 2022.

```sql
-- SQL Server 2022 Syntax:
DECLARE @json NVARCHAR(MAX) = N'{"order_id": 101, "customer": "Alice", "items": [{"sku": "A1", "qty": 2}]}';

SELECT 
    order_id, 
    customer, 
    sku, 
    qty
FROM OPENJSON(@json)
WITH (
    order_id INT '$.order_id',
    customer NVARCHAR(50) '$.customer',
    items NVARCHAR(MAX) '$.items' AS JSON
)
CROSS APPLY OPENJSON(items)
WITH (
    sku NVARCHAR(20) '$.sku',
    qty INT '$.qty'
);
```

---

## Best Practices

1. **Always Choose `JSONB` over `JSON` in PostgreSQL**: `JSONB` decomposes documents into binary format, stripping whitespace, enabling fast indexing, and eliminating repetitive parsing costs.
2. **Use Expression B-Tree Indexes for Known Query Paths**: If you frequently filter by one specific path (e.g., `WHERE metadata ->> 'email' = $1`), an expression B-Tree index `((metadata ->> 'email'))` is 5x smaller and faster than a global GIN index.
3. **Use GIN Indexes for Dynamic Containment**: Use `USING GIN (col)` when users query arbitrary combinations of tags or attributes using `@>`.
4. **Cast Extracted Text Explicitly**: `->>` returns text. If extracting numbers or booleans, explicitly cast them: `(metadata ->> 'amount')::numeric`.
5. **Normalize Core Relational Keys**: Never put foreign keys, primary keys, or audit timestamps inside a JSON document. Store relational metadata in first-class typed columns, and reserve JSON for optional or dynamic attributes.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Using `->` instead of `->>` in string comparisons | `data -> 'status'` returns jsonb `"ACTIVE"`; comparing `"ACTIVE" = 'ACTIVE'` evaluates to `FALSE`! | Use `->>` to extract raw, unquoted text. |
| 2 | Forgetting explicit casts on numeric JSON fields | Extracted text compared to numbers (`->> 'age' > 21`) performs alphabetical text sorting (`'100' < '21'`). | Cast explicitly: `(data ->> 'age')::int > 21`. |
| 3 | Dumping relational foreign keys into JSON | Destroys referential integrity, breaks cascading deletes, and hurts join performance. | Model relations as standard foreign key columns. |
| 4 | Storing massive binary blobs in JSONB | Large JSON documents (>2KB) spill to out-of-line TOAST tables, incurring decompression I/O. | Store files in S3/blob storage and reference URLs. |
| 5 | Using `jsonb_path_ops` when key existence (`?`) is needed | `jsonb_path_ops` only hashes path-value pairs and cannot evaluate `? 'key'`. | Use default `jsonb_ops` if key existence checks are required. |
| 6 | Believing JSON validation prevents application bugs | Schemaless JSON columns can accumulate malformed or inconsistent key names (`userID` vs `user_id`). | Use `CHECK (jsonb_typeof(data) = 'object')` or Postgres JSON Schema validators. |

---

## Performance & Security Notes

### Storage Bloat and the TOAST Mechanism
- In PostgreSQL, rows cannot exceed the page size (8KB). Any JSON document larger than 2KB is automatically compressed and moved out-of-line to a **TOAST (The Oversized-Attribute Storage Technique)** table.
- When querying a row, if the query selects `SELECT *`, PostgreSQL must retrieve and decompress the entire TOASTed JSON document, even if the application only wanted the `id` column!
- **Remedy**: Never write `SELECT *` on tables with large JSONB columns; explicitly project only the scalar fields required: `SELECT id, metadata ->> 'name' FROM tbl`.

---

## Comparison Matrix

| Dimension | PostgreSQL `JSON` | PostgreSQL `JSONB` | MySQL `JSON` | SQL Server `NVARCHAR` |
| :--- | :--- | :--- | :--- | :--- |
| **Storage Engine** | Raw UTF-8 Text | Decomposed Binary | Internal Binary Format | Unicode Text |
| **Parsing Overhead** | On every query read | Once on insert | Once on insert | On every query read |
| **GIN Indexing** | Not Supported | Native `USING GIN` | Multi-Valued B-Tree | Full-Text Index |
| **Whitespace Preserved?** | Yes | No | No | Yes |
| **Duplicate Keys** | Preserved | Eliminated (Last wins)| Eliminated (Last wins)| Preserved |
| **Containment (`@>`)** | Not Supported | Supported | Via `MEMBER OF()` | Not Supported |

---

## Interview Questions & Deep Dives

### Q1: What is the fundamental difference between `JSON` and `JSONB` in PostgreSQL?
<details>
<summary>Click to view answer</summary>

- `JSON` stores an exact textual replica of the input string. Whitespace, indentation, duplicate keys, and key ordering are preserved. It must be reparsed from scratch on every single query read, making processing slow. It cannot be indexed with GIN.
- `JSONB` parses the JSON upon ingestion into a decomposed binary format. Whitespace is stripped, duplicate keys are discarded (last value wins), and keys are sorted alphabetically. Parsing is done once at write time, allowing fast binary lookups at read time. It supports native GIN and GiST indexing.
</details>

### Q2: What is the difference between `->` and `->>` in PostgreSQL?
<details>
<summary>Click to view answer</summary>

- `->` extracts a JSON sub-element (object, array, or primitive) and returns it as a `JSONB` data type (e.g., `'{"a": "hello"}'::jsonb -> 'a'` returns `"hello"` with quotes).
- `->>` extracts the element and converts it into a standard SQL `TEXT` data type without quotes (e.g., `'{"a": "hello"}'::jsonb ->> 'a'` returns `'hello'`).
</details>

### Q3: How does a GIN index on a JSONB column work internally?
<details>
<summary>Click to view answer</summary>

A GIN (Generalized Inverted Index) decomposes the JSONB document into individual key-value tokens (e.g., `"tier": "GOLD"` becomes a posting key). The index structure maintains a B-Tree of these distinct tokens, where each token points to a posting list of heap tuple IDs (`TID`s) that contain that token. When a query searches via `WHERE metadata @> '{"tier": "GOLD"}'`, the engine seeks the token directly in the GIN index and returns the matching row IDs without scanning the heap.
</details>

### Q4: When should you create an Expression B-Tree index instead of a GIN index on JSONB?
<details>
<summary>Click to view answer</summary>

You should use an Expression B-Tree index `CREATE INDEX ON tbl ((metadata ->> 'user_id'))` when:
1. Queries filter on a specific, known, highly selective path.
2. You need B-Tree features such as range scans (`<`, `>`, `BETWEEN`), sorting (`ORDER BY`), or unique constraints (`CREATE UNIQUE INDEX`).
Expression B-Tree indexes are significantly smaller and faster to maintain during inserts than full GIN indexes.
</details>

### Q5: What is the difference between `jsonb_ops` and `jsonb_path_ops`?
<details>
<summary>Click to view answer</summary>

- `jsonb_ops` (default GIN operator class) indexes every individual key, value, and array element. It supports containment (`@>`), key existence (`?`), any-key existence (`?|`), and all-key existence (`?&`).
- `jsonb_path_ops` hashes entire root-to-leaf paths (e.g., hash of `address.city.San Francisco`). It is typically 30%–50% smaller on disk and executes containment queries (`@>`) faster, but it **only** supports the `@>` operator and cannot be used for key existence checks (`?`).
</details>

### Q6: How do you update a single nested key in a JSONB document in PostgreSQL?
<details>
<summary>Click to view answer</summary>

Use the `jsonb_set()` function:
```sql
UPDATE customer_profiles
SET metadata = jsonb_set(metadata, '{preferences, theme}', '"dark"')
WHERE customer_id = 1;
```
Or use the concatenation operator `||` to merge/overwrite top-level keys:
```sql
UPDATE customer_profiles
SET metadata = metadata || '{"tier": "PLATINUM"}'::jsonb
WHERE customer_id = 1;
```
</details>

### Q7: Why does `'{"age": 100}'::jsonb ->> 'age' > 21` produce unexpected results?
<details>
<summary>Click to view answer</summary>

`->>` returns a `TEXT` string `'100'`. When comparing text with text, string collation is used: `'100'` is alphabetically smaller than `'21'` because `'1'` comes before `'2'`. Consequently, `'100' > '21'` evaluates to `FALSE`! You must explicitly cast the extracted text to an integer:
`((metadata ->> 'age')::int > 21)`.
</details>

### Q8: What does the `?` operator do in PostgreSQL JSONB?
<details>
<summary>Click to view answer</summary>

The `?` operator tests whether a top-level string key exists in a JSONB object or array:
`'{"a": 1, "b": 2}'::jsonb ? 'a'` evaluates to `TRUE`.
(Note: In application frameworks using JDBC or Python DB-API where `?` represents a bind parameter, the `?` operator must be escaped as `??`).
</details>

---

## Practice Exercises

### Exercise 1: Extract Nested Property with Type Casting
Given `devices(device_id, telemetry JSONB)`. Write a query extracting `battery_level` (as integer) and `firmware_version` (as text) from path `telemetry -> 'hardware' -> 'battery'` and `telemetry -> 'system' -> 'version'`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    device_id,
    (telemetry -> 'hardware' ->> 'battery')::integer AS battery_level,
    telemetry -> 'system' ->> 'version' AS firmware_version
FROM devices;
```
</details>

---

### Exercise 2: JSON Containment Filtering
Given `catalog(item_id, attributes JSONB)`. Write a query using the `@>` containment operator to find all items that have `color = 'Black'` and `in_stock = true`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT item_id
FROM catalog
WHERE attributes @> '{"color": "Black", "in_stock": true}';
```
</details>

---

### Exercise 3: Filter by Key Existence
Given `users(user_id, profile JSONB)`. Find all users whose profile contains the top-level key `'phone_verified'`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT user_id
FROM users
WHERE profile ? 'phone_verified';
```
</details>

---

### Exercise 4: Expression B-Tree Index Creation
Create a unique B-Tree index on the extracted `tax_id` property from `companies(company_id, tax_info JSONB)` to guarantee tax ID uniqueness across all companies.

<details>
<summary>Click to view solution</summary>

```sql
CREATE UNIQUE INDEX idx_unique_company_tax_id 
ON companies ((tax_info ->> 'tax_id'));
```
</details>

---

### Exercise 5: Unnest JSON Array Elements
Given `blog_posts(post_id, comments JSONB)` where `comments` is a JSON array of strings `["Great post!", "Helpful article"]`. Write a query unnesting each comment as an individual row using `jsonb_array_elements_text()`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    bp.post_id,
    c.comment_text
FROM blog_posts bp
CROSS JOIN LATERAL jsonb_array_elements_text(bp.comments) AS c(comment_text);
```
</details>

---

### Exercise 6: Fix the Bind Parameter Escaping Bug
In a Node.js or Python backend, an engineer writes:
`cursor.execute("SELECT * FROM events WHERE payload ? %s", ['auth_token'])`
Explain why this throws a syntax error and how to fix it.

<details>
<summary>Click to view solution</summary>

**Cause**: Drivers interpret `?` as a positional bind parameter rather than the PostgreSQL JSONB key-existence operator, causing parameter count mismatch errors.

**Fix**: Escape the operator using double question marks `??` (or use the equivalent function `jsonb_exists(payload, 'auth_token')`):
```sql
SELECT * FROM events WHERE payload ?? %s;
-- Or:
SELECT * FROM events WHERE jsonb_exists(payload, %s);
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                    JSONB CHEAT SHEET                                    |
+-------------------+----------------------------+----------------------------------------+
| Operator / Func   | Return Type                | Meaning                                |
+-------------------+----------------------------+----------------------------------------+
| -> 'key'          | JSONB                      | Extracts nested element as JSONB       |
| ->> 'key'         | TEXT                       | Extracts nested element as plain text  |
| #> '{a, b}'       | JSONB                      | Extracts path as JSONB                 |
| #>> '{a, b}'      | TEXT                       | Extracts path as text                  |
| @> '{"a": 1}'     | BOOLEAN                    | Containment check (Left contains Right)|
| ? 'key'           | BOOLEAN                    | Top-level key existence                |
| jsonb_set(...)    | JSONB                      | Updates nested attribute value         |
| jsonb_array_elem  | SET OF JSONB               | Unnests JSON array elements into rows  |
+-------------------+----------------------------+----------------------------------------+
```

- **Storage**: Always choose `JSONB` over `JSON` in PostgreSQL.
- **Indexing**: Use B-Tree expression indexes for single known keys; use GIN for dynamic containment.
- **Type Safety**: Always explicitly cast extracted text (`(data ->> 'price')::numeric`).

---

## Further Reading
- PostgreSQL 17 Documentation: *JSON Types, Functions, and Operators*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 4.31 `<JSON data>`
- Bruce Momjian: *PostgreSQL JSONB Internals & GIN Index Architecture*
- Markus Winand: *Modern SQL - JSON in Relational Databases*
