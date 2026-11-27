# Arrays and Composite Types: Schema Modeling & Operations

| Metadata | Details |
| :--- | :--- |
| **Topic** | Native PostgreSQL Arrays, Operators (`&&`, `@>`, `||`), `unnest()`, `array_agg()`, and Composite Types (UDTs) |
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

In strict First Normal Form (1NF), every relational attribute must contain atomic (indivisible) values. When modeling one-to-many relationships (e.g., tags on a blog post, phone numbers for a contact, or permissions for a security role), standard relational modeling mandates a dedicated child table and a foreign key join.

However, strict normalization introduces overhead:
- A simple query requires joining across 3 tables.
- Foreign keys require index maintenance and vacuuming.
- Denormalized application views must repeatedly group and aggregate rows.

PostgreSQL natively supports **Array Types** and **Composite Types (User-Defined Types)**. Arrays allow storing ordered collections of homogeneous values directly inside a table column, backed by GIN indexes that deliver sub-millisecond set-overlap searches (`&&`) and containment searches (`@>`). Composite types allow bundling related fields (such as `(street, city, zip_code)`) into a single structured unit.

Understanding when to leverage native arrays versus when to enforce strict 1NF relational tables is a hallmark of senior database engineering.

---

## Core Concepts

### 1. PostgreSQL Array Syntax & 1-Based Indexing
Arrays can be declared for any built-in or user-defined data type:
```sql
tags TEXT[],
phone_numbers VARCHAR(20)[],
matrix INT[][]
```
- **1-Based Indexing**: PostgreSQL arrays start at index `1`, not `0`! Accessing `tags[0]` returns `NULL` unless custom array bounds were explicitly defined.
- **Array Constructors**:
  - Literal string syntax: `'{ "sql", "postgres", "database" }'`
  - SQL Constructor: `ARRAY['sql', 'postgres', 'database']`

### 2. Array Slicing & Cardinality
- Slicing: `tags[1:2]` returns an array containing the first two elements.
- `cardinality(array)`: Returns the total number of elements across all dimensions.
- `array_length(array, dimension)`: Returns the length of the specified array dimension.

### 3. Core Array Operators
- **`&&` (Overlap)**: Evaluates to `TRUE` if the two arrays share **at least one common element**:
  ```sql
  tags && ARRAY['fastapi', 'python']
  ```
- **`@>` (Contains)**: Evaluates to `TRUE` if the left array contains every element of the right array.
- **`<@` (Is Contained By)**: Evaluates to `TRUE` if every element of the left array exists in the right array.
- **`||` (Concatenation)**: Appends an element to an array or merges two arrays.

### 4. Relational Bridge: `unnest()` and `array_agg()`
- **`unnest(array)`**: Expands an array into a set of relational rows (one row per element).
- **`array_agg(expression [ORDER BY ...])`**: Aggregate function that collects values from multiple rows into a single ordered array.

### 5. Composite Types (User-Defined Types)
A composite type represents a structured record layout that can be used as a column data type:
```sql
CREATE TYPE money_amount AS (
    currency CHAR(3),
    amount NUMERIC(15,2)
);
```
- Accessing fields requires parentheses around the column identifier: `(user_account.balance).amount`.

---

## How It Works Internally

```
                   [ Input: ARRAY[101, 102, 103]::int[] ]
                                      │
                                      ▼
                        [ PostgreSQL Array Header ]
     ┌─────────────────────────────────────────────────────────────────┐
     │ ndim: 1 | hasnull: 0 | elemtype: 23 (int4) | dim[1]: 3 | lBound: 1│
     ├─────────────────────────────────────────────────────────────────┤
     │ Raw Values: [ 101, 102, 103 ]                                   │
     └─────────────────────────────────────────────────────────────────┘
                                      │
         ┌────────────────────────────┴────────────────────────────┐
         ▼                                                         ▼
[ GIN Index on Array ]                                    [ Unnest Operation ]
CREATE INDEX ON tbl USING GIN (tags)                     SELECT unnest(tags)
Decomposes array into inverted index                     Streaming iterator emits
posting tree:                                            1 tuple per element
- 101 -> [TID 1, TID 4]                                  without copying base row.
- 102 -> [TID 1, TID 2]
Accelerates && and @> in O(log N)!
```

1. **Physical Binary Storage**: PostgreSQL stores arrays as contiguous binary memory structures prefixed by an array header containing dimensionality (`ndim`), flags, element type OID, and lower bounds.
2. **GIN Index Integration**: When a GIN index is built on an array column, each distinct array element is indexed as an independent key. Queries checking `WHERE tags && ARRAY['db', 'sql']` perform a fast union lookup in the GIN B-Tree.

---

## Syntax & Usage

```sql
-- Table definition with arrays
CREATE TABLE blog_posts (
    post_id SERIAL PRIMARY KEY,
    title VARCHAR(150) NOT NULL,
    tags TEXT[] NOT NULL DEFAULT '{}',
    scores INT[] NOT NULL
);

-- GIN Index on Array
CREATE INDEX idx_blog_tags_gin ON blog_posts USING GIN (tags);

-- Query: Check if tags share any target category
SELECT post_id, title
FROM blog_posts
WHERE tags && ARRAY['database', 'architecture'];

-- Query: Unnest array to relational rows
SELECT post_id, unnest(tags) AS individual_tag
FROM blog_posts;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **Native Array Types** | Full support (`INT[]`, `TEXT[]`) | **Not Supported** (Must use JSON or 1NF) | **Not Supported** (Must use JSON or 1NF) | **Not Supported** (Must use JSON or TVP) |
| **Overlap Operator (`&&`)**| Native `arr1 && arr2` with GIN | Emulate via `JSON_OVERLAPS()` | Emulate via `json_each()` | Emulate via `OPENJSON` joins |
| **Aggregation Function** | `ARRAY_AGG(col [ORDER BY col])` | `JSON_ARRAYAGG(col)` | `json_group_array(col)` | `STRING_AGG` (or JSON array) |
| **Unnest Function** | `unnest(array)` | `JSON_TABLE()` | `json_each()` | `OPENJSON()` / `STRING_SPLIT()` |
| **Composite Types (UDTs)** | `CREATE TYPE ... AS (...)` | **Not Supported** | **Not Supported** | CLR UDTs (Complex .NET compilation) |

---

## Practical Examples

### Example 1: Basic — Storing and Querying Tagged Entities
Model article categories using native PostgreSQL arrays and check for specific tags.

```sql
CREATE TABLE knowledge_articles (
    article_id INT PRIMARY KEY,
    title VARCHAR(100) NOT NULL,
    tags TEXT[] NOT NULL
);

INSERT INTO knowledge_articles VALUES
(1, 'PostgreSQL 17 Overview', ARRAY['sql', 'database', 'postgres']),
(2, 'Docker Container Networking', ARRAY['devops', 'docker', 'linux']),
(3, 'FastAPI Async Architecture', ARRAY['python', 'api', 'backend']);

-- 1. Find articles containing 'sql'
SELECT article_id, title 
FROM knowledge_articles 
WHERE 'sql' = ANY(tags);

-- 2. Find articles overlapping with ['postgres', 'devops']
SELECT article_id, title, tags
FROM knowledge_articles
WHERE tags && ARRAY['postgres', 'devops'];
```

```
Result:
 article_id |            title            |          tags           
------------+-----------------------------+-------------------------
          1 | PostgreSQL 17 Overview      | {sql,database,postgres}
          2 | Docker Container Networking | {devops,docker,linux}
(2 rows)
```

---

## Example 2: Real-World — Aggregating and Deduplicating User Permissions
Group user permissions into an ordered array without performing repetitive subqueries.

```sql
CREATE TABLE user_roles (
    user_id INT NOT NULL,
    role_name VARCHAR(50) NOT NULL
);

INSERT INTO user_roles VALUES
(1, 'BILLING_ADMIN'),
(1, 'USER_MANAGER'),
(1, 'VIEWER'),
(2, 'VIEWER');

SELECT 
    user_id,
    ARRAY_AGG(role_name ORDER BY role_name ASC) AS assigned_roles,
    cardinality(ARRAY_AGG(role_name)) AS role_count
FROM user_roles
GROUP BY user_id;
```

```
Result:
 user_id |            assigned_roles            | role_count 
---------+--------------------------------------+------------
       1 | {BILLING_ADMIN,USER_MANAGER,VIEWER}  |          3
       2 | {VIEWER}                             |          1
(2 rows)
```

---

### Example 3: Edge Case — 1-Based Indexing and Out-of-Bounds Behavior
Demonstrating that PostgreSQL arrays are 1-based and return `NULL` (without throwing an error) when indexed out-of-bounds.

```sql
CREATE TABLE array_demo (
    id INT PRIMARY KEY,
    vals INT[] NOT NULL
);

INSERT INTO array_demo VALUES (1, ARRAY[10, 20, 30]);

SELECT 
    vals[0] AS zero_index,      -- Returns NULL!
    vals[1] AS first_element,    -- Returns 10
    vals[3] AS third_element,    -- Returns 30
    vals[99] AS out_of_bounds    -- Returns NULL without raising exception
FROM array_demo;
```

```
Result:
 zero_index | first_element | third_element | out_of_bounds 
------------+---------------+---------------+---------------
       NULL |            10 |            30 |          NULL
(1 row)
```

---

### Example 4: Failure Mode — Syntax Error Accessing Composite Type Attributes
When querying fields of a composite type, PostgreSQL syntax mandates that the column name must be enclosed in parentheses to prevent ambiguity with schema/table names.

```sql
CREATE TYPE geo_point AS (
    latitude NUMERIC(9,6),
    longitude NUMERIC(9,6)
);

CREATE TABLE locations (
    loc_id INT PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    coords geo_point NOT NULL
);

INSERT INTO locations VALUES (1, 'Office HQ', (37.774929, -122.419416));

-- FAILING QUERY (Syntax error):
-- ERROR: schema "coords" does not exist
SELECT coords.latitude FROM locations;

-- CORRECT SYNTAX (Enclose column in parentheses):
SELECT (coords).latitude, (coords).longitude FROM locations;
```

```
Result:
 latitude  |  longitude  
-----------+-------------
 37.774929 | -122.419416
(1 row)
```

---

### Example 5: Optimized — Cross-Dialect Array Simulation in MySQL / SQLite
Since MySQL 8.4 and SQLite 3.50 lack native array types, use JSON arrays and `JSON_OVERLAPS()` or `json_each()`.

```sql
-- MySQL 8.4 LTS:
CREATE TABLE articles_mysql (
    id INT PRIMARY KEY,
    tags JSON NOT NULL
);

INSERT INTO articles_mysql VALUES (1, '["sql", "database", "postgres"]');

-- Check overlap in MySQL 8.0+:
SELECT id 
FROM articles_mysql 
WHERE JSON_OVERLAPS(tags, '["postgres", "devops"]');

-- Unnesting in SQLite 3.50+:
SELECT id, value AS tag
FROM articles_mysql, json_each(tags);
```

---

## Best Practices

1. **Use Arrays for Low-Cardinality Attributes**: Arrays are ideal for small, finite lists (e.g., up to 20 tags, flags, or status codes). Do not use arrays for entities that grow into thousands of items.
2. **Always Index Array Columns with GIN**: An unindexed `tags && ARRAY[...]` query forces a full sequential scan. Always create a `USING GIN (col)` index.
3. **Remember 1-Based Indexing**: Always start array indexing at 1: `arr[1]`.
4. **Use Parentheses for Composite Fields**: When accessing attributes of a composite type, write `(column_name).attribute_name`.
5. **Prefer Relational Tables for Foreign Keys**: Never store foreign key IDs inside an array (e.g., `user_ids INT[]`) if referential integrity (ON DELETE CASCADE) is required. PostgreSQL cannot enforce foreign key constraints on elements inside an array!

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Assuming 0-based array indexing | `arr[0]` evaluates to `NULL` in standard PostgreSQL arrays. | Use `arr[1]` for the first element. |
| 2 | Missing parentheses on composite attributes | `col.attr` is parsed as `schema.table` or `table.column`, raising syntax errors. | Write `(col).attr`. |
| 3 | Storing Foreign Keys in arrays | Foreign key constraints cannot reference elements inside an array. | Use a normalized child join table. |
| 4 | Attempting B-Tree indexing on arrays | B-Trees can only sort the whole array; they cannot index individual elements. | Use a GIN index: `CREATE INDEX ... USING GIN (array_col)`. |
| 5 | Modifying single array elements in high-concurrency tables | Updating `arr[2] = 5` rewrites the entire row and array tuple, causing MVCC bloat. | Normalize into rows if individual elements update frequently. |
| 6 | Using native arrays when cross-dialect portability is required | MySQL, SQLite, and SQL Server do not support native array types. | Use JSON columns (`JSON` / `JSONB`) for cross-database portability. |

---

## Performance & Security Notes

### GIN Indexing vs Sequential Scan on Arrays
Consider an articles table with 2,000,000 rows where each article has 5 tags:
```sql
-- Query checking array overlap:
EXPLAIN ANALYZE
SELECT article_id FROM knowledge_articles WHERE tags && ARRAY['database', 'cloud'];
```
- **Without GIN Index**: Performs a sequential scan reading all 2,000,000 tuples, checking arrays in CPU memory. Latency: ~380 ms.
- **With GIN Index (`CREATE INDEX idx_gin ON knowledge_articles USING GIN (tags)`)**:
  ```
  Bitmap Heap Scan on knowledge_articles  (cost=12.20..145.30 rows=50 width=4)
    ->  Bitmap Index Scan on idx_gin  (cost=0.00..12.20 rows=50)
          Index Cond: (tags && '{database,cloud}'::text[])
  ```
  Latency: ~0.8 ms (475x faster!). The GIN index seeks only the pointers for `'database'` and `'cloud'`.

---

## Comparison Matrix

| Dimension | Native Arrays (`INT[]`) | Normalized Join Table | JSONB Array (`["a", "b"]`) |
| :--- | :--- | :--- | :--- |
| **Storage Engine** | Native PostgreSQL Binary | Normalized Relational Heap | Decomposed Binary JSON |
| **Referential Integrity (FK)**| **Not Supported** | Fully Enforced | **Not Supported** |
| **Index Mechanism** | GIN (`array_ops`) | B-Tree on Foreign Key | GIN (`jsonb_ops`) |
| **Cross-Dialect Portability** | PostgreSQL Only | Universal (100% ANSI) | Universal (Modern engines) |
| **Join Overhead** | Zero (Embedded in row) | Joins required | Zero (Embedded in row) |
| **Best Used For** | Fast tag searches, permissions | Formal 1:N relations with FKs | Dynamic/polymorphic payloads |

---

## Interview Questions & Deep Dives

### Q1: What is the difference between `= ANY(array)` and `array && ARRAY[...]`?
<details>
<summary>Click to view answer</summary>

- `= ANY(array)` compares a single scalar value against an array, returning `TRUE` if the scalar matches any element:
  `'postgres' = ANY(tags)`
- `array1 && array2` compares two arrays, returning `TRUE` if the two arrays have **at least one common element** (set overlap):
  `tags && ARRAY['postgres', 'sql']`
Both can utilize a GIN index on PostgreSQL.
</details>

### Q2: Why can't PostgreSQL enforce a foreign key constraint on an array column?
<details>
<summary>Click to view answer</summary>

In relational theory and SQL engine architecture, foreign key constraints operate on discrete, scalar table columns. A foreign key triggers check routines and row locks on the referenced table. Relational storage engines do not unpack multi-element collection containers like arrays during tuple validation. If you require `ON DELETE CASCADE` or referential verification, you must normalize the relationship into a bridge join table.
</details>

### Q3: What is the starting index of a PostgreSQL array by default?
<details>
<summary>Click to view answer</summary>

PostgreSQL arrays are **1-based** by default. The first element is accessed using `array_col[1]`. Accessing `array_col[0]` evaluates to `NULL` (it does not throw an out-of-bounds error).
</details>

### Q4: How does `unnest()` interact with multiple array columns in PostgreSQL?
<details>
<summary>Click to view answer</summary>

In PostgreSQL 8.4+, calling `unnest(arr1, arr2)` in the `SELECT` list performs a parallel unnesting: it pairs elements from `arr1` and `arr2` by index. If the arrays have unequal lengths, shorter arrays are padded with `NULL`s. Best practice is to use `CROSS JOIN LATERAL unnest(arr)` in the `FROM` clause to avoid confusing set-returning function semantics in `SELECT`.
</details>

### Q5: How do you add an element to an existing array in PostgreSQL?
<details>
<summary>Click to view answer</summary>

Use the concatenation operator `||` or the `array_append()` function:
```sql
UPDATE blog_posts 
SET tags = tags || 'performance' 
WHERE post_id = 1;
-- Or:
UPDATE blog_posts 
SET tags = array_append(tags, 'performance') 
WHERE post_id = 1;
```
</details>

### Q6: What is a Composite Type and how does it differ from a Table?
<details>
<summary>Click to view answer</summary>

A composite type defines a structured record specification (a set of named fields with data types) without allocating physical storage on disk. A table is a composite type that also allocates an underlying heap storage file, constraints, indexes, and write-ahead log structures. In fact, in PostgreSQL, whenever you create a table `CREATE TABLE users (...)`, PostgreSQL automatically creates a composite type with the same name `users` in `pg_type`.
</details>

### Q7: What does the `cardinality()` function return?
<details>
<summary>Click to view answer</summary>

The `cardinality(array)` function returns the total number of elements across all dimensions of the array. For a 1D array of 3 elements, it returns 3. For a 2D array of $3 \times 4$ elements, it returns 12. If the array is empty or `NULL`, it returns 0 or `NULL` respectively.
</details>

### Q8: How can you remove duplicate elements from a PostgreSQL array?
<details>
<summary>Click to view answer</summary>

Unnest the array, select distinct values, and aggregate them back using `ARRAY_AGG`:
```sql
SELECT ARRAY(SELECT DISTINCT unnest(tags) FROM blog_posts WHERE post_id = 1);
```
</details>

---

## Practice Exercises

### Exercise 1: Filter by Multiple Required Tags (Containment)
Given `products(product_id, title, features TEXT[])`. Write a query returning products that contain **both** `'bluetooth'` and `'wireless'` features using the `@>` operator.

<details>
<summary>Click to view solution</summary>

```sql
SELECT product_id, title
FROM products
WHERE features @> ARRAY['bluetooth', 'wireless'];
```
</details>

---

### Exercise 2: Unnest and Count Tag Frequency
Given `articles(article_id, tags TEXT[])`. Write a query to find the top 5 most frequently used tags across all articles and their occurrence count.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    t.tag,
    COUNT(*) AS usage_count
FROM articles a
CROSS JOIN LATERAL unnest(a.tags) AS t(tag)
GROUP BY t.tag
ORDER BY usage_count DESC
LIMIT 5;
```
</details>

---

### Exercise 3: Aggregate Child Rows into Sorted Array
Given `orders(order_id, customer_id)` and `order_items(item_id, order_id, sku)`. For each order, produce an array of all purchased `sku` values sorted alphabetically.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    order_id,
    ARRAY_AGG(sku ORDER BY sku ASC) AS sku_list
FROM order_items
GROUP BY order_id;
```
</details>

---

### Exercise 4: Composite Address Type Creation & Usage
Create a composite type `street_address` with fields `street`, `city`, and `postal_code`. Create a table `branches` using this type, insert a row, and write a query selecting only the `city`.

<details>
<summary>Click to view solution</summary>

```sql
-- 1. Create Type
CREATE TYPE street_address AS (
    street TEXT,
    city TEXT,
    postal_code VARCHAR(10)
);

-- 2. Create Table
CREATE TABLE branches (
    branch_id INT PRIMARY KEY,
    location street_address NOT NULL
);

-- 3. Insert Row
INSERT INTO branches VALUES 
(1, ('100 Market St', 'San Francisco', '94105'));

-- 4. Select City (Must enclose in parentheses!)
SELECT (location).city FROM branches;
```
</details>

---

### Exercise 5: Array Overlap with GIN Index Creation
Write the DDL to create a GIN index on `users(user_id, assigned_permissions TEXT[])`, and write a query checking if the user has any of `['SUPERADMIN', 'SECURITY_OFFICER']`.

<details>
<summary>Click to view solution</summary>

```sql
-- DDL
CREATE INDEX idx_users_perms_gin ON users USING GIN (assigned_permissions);

-- Query
SELECT user_id
FROM users
WHERE assigned_permissions && ARRAY['SUPERADMIN', 'SECURITY_OFFICER'];
```
</details>

---

### Exercise 6: Fix the Array Slicing Bug
An engineer writes:
`SELECT tags[0:2] FROM articles;`
Explain what this query returns in PostgreSQL and why it is misleading.

<details>
<summary>Click to view solution</summary>

In PostgreSQL, array indexing is 1-based. Slicing with `[0:2]` requests elements from index 0 through index 2. Because index 0 does not exist, PostgreSQL gracefully ignores it and returns elements at index 1 and index 2 (an array of length 2). 

While it appears to work, writing index 0 reflects an incorrect assumption about array bounds. The correct idiomatic syntax is `tags[1:2]`.
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                    ARRAY CHEAT SHEET                                    |
+-------------------+----------------------------+----------------------------------------+
| Operator / Func   | Return Type                | Description                            |
+-------------------+----------------------------+----------------------------------------+
| arr[1]            | Element                    | 1-based indexing; NULL if missing      |
| arr[1:3]          | Array Slice                | Extracts sub-array from index 1 to 3   |
| arr1 && arr2      | BOOLEAN                    | Overlap: TRUE if sharing ANY element   |
| arr1 @> arr2      | BOOLEAN                    | Contains: TRUE if arr1 has ALL arr2    |
| arr || element    | Array                      | Appends element to array               |
| unnest(arr)       | SET OF elements            | Expands array into relational rows     |
| ARRAY_AGG(col)    | Array                      | Aggregates relational rows into array  |
| cardinality(arr)  | INTEGER                    | Total count of elements in array       |
+-------------------+----------------------------+----------------------------------------+
```

- **Index Starting Point**: 1-based (`arr[1]`), never 0-based.
- **Fast Search**: Accelerate `&&` and `@>` queries with `USING GIN (col)`.
- **Composite Parentheses**: Always write `(composite_col).field`.

---

## Further Reading
- PostgreSQL 17 Documentation: *Arrays & Array Functions and Operators*
- PostgreSQL 17 Documentation: *Composite Types & User-Defined Types*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 4.10 `<collection types>`
- Bruce Momjian: *PostgreSQL Internals: Understanding Array Storage and GIN Indexing*
