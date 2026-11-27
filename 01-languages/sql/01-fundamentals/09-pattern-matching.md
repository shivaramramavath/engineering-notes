# 09: Pattern Matching: LIKE, ILIKE & Regex

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [08 - Three-Valued Logic & NULL Semantics](./08-null-semantics.md) |
| **Target Audience** | Software Engineers, Data Analysts, Backend Developers |
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

Exact equality lookups (`WHERE username = 'alex'`) satisfy only a portion of real-world string search requirements. Users frequently search for partial names, email domains, phone prefixes, SKU patterns, or complex structured formats (UUIDs, IP addresses, postal codes). 

SQL provides three distinct tiers of pattern matching:
1. **ANSI `LIKE` / `NOT LIKE`**: Simple wildcard matching using `%` (zero or more characters) and `_` (exactly one character).
2. **Case-Insensitive Extensions (`ILIKE`)**: Convenient case-folding search without the performance overhead of wrapping columns in `LOWER()`.
3. **Regular Expression Engines (POSIX `~` and `REGEXP`)**: Full pattern matching supporting character classes, quantifiers, alternations, and capture groups.

Crucially, how you construct pattern-matching predicates dictates whether the database engine can perform an ultra-fast $O(\log N)$ B-Tree index scan or is forced into an expensive $O(N)$ full table scan. A left-anchored prefix (`LIKE 'NYC%'`) is fully sargable; a wildcard prefix (`LIKE '%NYC%'`) destroys standard index utilization unless specialized inverted trigram (GIN) indexes are deployed.

---

## 2. Core Concepts

### 1. The ANSI `LIKE` Wildcards
The SQL standard defines two fundamental wildcard characters:
- **`%` (Percent)**: Matches any sequence of zero or more characters.
  - `'A%'`: Matches `'A'`, `'Alice'`, `'Apple'`.
  - `'%son'`: Matches `'son'`, `'Johnson'`, `'Watson'`.
  - `'%bank%'`: Matches any string containing `'bank'`.
- **`_` (Underscore)**: Matches exactly one single character.
  - `'c_t'`: Matches `'cat'`, `'cut'`, `'cot'`. Does NOT match `'cart'`.
  - `'___'`: Matches any string of exactly three characters.

### 2. Escaping Literal Wildcards
If you need to search for a literal `%` or `_` character in data (e.g., searching for `'100%'` or `'user_id'`), you must escape it using the `ESCAPE` clause:
```sql
-- Search for a literal underscore
SELECT * FROM files WHERE filename LIKE '%\_backup.tar' ESCAPE '\';

-- Search for a literal percent sign
SELECT * FROM discounts WHERE label LIKE '10!% OFF' ESCAPE '!';
```

### 3. POSIX Regular Expressions vs Standard `SIMILAR TO`
- **`SIMILAR TO` (SQL:1999 Standard)**: A hybrid syntax blending `LIKE` wildcards (`%`, `_`) with regex features (`|`, `*`, `+`, `[...]`). Widely criticized as verbose, confusing, and largely superseded by POSIX regex.
- **POSIX Regular Expressions (PostgreSQL Standard)**:
  - `~`: Case-sensitive regex match.
  - `~*`: Case-insensitive regex match.
  - `!~`: Case-sensitive non-match.
  - `!~*`: Case-insensitive non-match.

---

## 3. How It Works Internally

### B-Tree Index Matching Mechanics for Patterns

A standard B-Tree index stores string keys in sorted lexicographical byte order:

```
B-Tree Leaf Page Layout:
["Apple", "Apricot", "Avocado", "Banana", "Blueberry", "Cherry", "Cranberry"]

Query A: WHERE name LIKE 'Ap%' (Left-Anchored Prefix)
- The optimizer navigates B-Tree root -> leaf directly to 'Ap'.
- It scans sequentially until the first key that does not start with 'Ap'.
- Result: Sargable B-Tree Index Range Seek! (Instant)

Query B: WHERE name LIKE '%berry' (Leading Wildcard)
- 'Blueberry' and 'Cranberry' are located on completely opposite sides of the tree!
- The B-Tree cannot eliminate any branch.
- Result: Non-Sargable! B-Tree abandoned -> Full Table Sequential Scan!
```

- **Collation Impact**: In PostgreSQL, standard B-Tree indexes on text columns using locale collations (e.g., `en_US.UTF-8`) cannot optimize `LIKE 'prefix%'` unless the index is created with the specialized **`varchar_pattern_ops`** operator class:
  ```sql
  CREATE INDEX idx_products_name_prefix ON products(product_name varchar_pattern_ops);
  ```

---

## 4. Syntax & Usage

### Pattern Matching Operations Overview

```sql
-- 1. Standard LIKE (Case-sensitive in Postgres)
SELECT * FROM customers WHERE email LIKE '%@company.com';

-- 2. PostgreSQL ILIKE (Case-insensitive)
SELECT * FROM customers WHERE first_name ILIKE 'al%';

-- 3. Literal Escaping
SELECT * FROM audit_logs WHERE log_message LIKE '%ERROR\_404%' ESCAPE '\';

-- 4. POSIX Regular Expressions (PostgreSQL)
SELECT * FROM users WHERE phone_number ~ '^\+1-[0-9]{3}-[0-9]{3}-[0-9]{4}$';

-- 5. Regex Substring Extraction
SELECT SUBSTRING('Order #88219 confirmed' FROM '#([0-9]+)') AS extracted_order_id;
```

### Multi-Dialect Comparison Matrix

| Pattern Capability | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Case-Insensitive `ILIKE`** | Native keyword (`ILIKE`)| Not needed if collation is `_ci` | `LIKE` is case-insensitive for ASCII | Not native (requires `COLLATE` or `LOWER`)|
| **Standard `LIKE` Case Default**| Case-Sensitive | Case-Insensitive (default `_ci` collation)| Case-Insensitive (ASCII only) | Case-Insensitive (default collation) |
| **POSIX Regex Match** | `~`, `~*`, `!~`, `!~*` | `REGEXP` or `RLIKE` | `REGEXP` (requires extension) | Not supported natively |
| **Regex Extraction** | `SUBSTRING(str FROM regex)` or `regexp_substr`| `REGEXP_SUBSTR()` | `regex_extract()` (extension)| Not native |
| **Trigram GIN Indexing** | Native via `pg_trgm` | Full-Text Search (InnoDB FTS) | FTS5 extension | Full-Text Engine |

---

## 5. 5 Practical Progressive Examples

### Example 1: Prefix Matching (Sargable B-Tree Seek)
Finding customers by last name prefix in a phone book:

```sql
SELECT customer_id, first_name, last_name, email
FROM customers
WHERE last_name LIKE 'M%'
ORDER BY last_name, first_name;
```
```text
+-------------+------------+-----------+--------------------+
| customer_id | first_name | last_name | email              |
+-------------+------------+-----------+--------------------+
|         101 | Alex       | Morgan    | alex.m@example.com |
+-------------+------------+-----------+--------------------+
```
*(Left-anchored `LIKE 'M%'` translates directly to an index range scan between `'M'` and `'N'`).*

### Example 2: Fixed-Length Character Matching with Underscore (`_`)
Searching for flight codes or standard 3-character airport codes:

```sql
CREATE TABLE airport_codes (code VARCHAR(5), city VARCHAR(50));
INSERT INTO airport_codes VALUES ('JFK', 'New York'), ('LAX', 'Los Angeles'), ('ORD', 'Chicago'), ('KLAX', 'Military Base');

-- Find all exactly 3-character codes starting with 'L':
SELECT * FROM airport_codes WHERE code LIKE 'L__';
```
```text
+------+-------------+
| code | city        |
+------+-------------+
| LAX  | Los Angeles |
+------+-------------+
```

### Example 3: Edge Case (Searching for Literal `%` and `_` Characters)
Searching for percentage discounts in promo labels:

```sql
CREATE TABLE promotions (promo_id INT, label VARCHAR(50));
INSERT INTO promotions VALUES 
(1, 'SAVE 20% TODAY'),
(2, 'SAVE 200 POINTS'),
(3, 'NEW_USER_SPECIAL');

-- Search for literal '%' character:
SELECT * FROM promotions WHERE label LIKE '%20\%%' ESCAPE '\';
```
```text
+----------+----------------+
| promo_id | label          |
+----------+----------------+
|        1 | SAVE 20% TODAY |
+----------+----------------+
```

### Example 4: Failure Mode (Leading Wildcard Squelching Indexes)
Observing the query execution plan when searching for substrings:

```sql
-- ANTI-PATTERN: Leading wildcard forces Sequential Scan!
EXPLAIN SELECT * FROM customers WHERE email LIKE '%@gmail.com';
```
```text
                         QUERY PLAN                         
------------------------------------------------------------
 Seq Scan on customers  (cost=0.00..25.00 rows=5 width=108)
   Filter: ((email)::text ~~ '%@gmail.com'::text)
```
*(Notice the filter operator `~~`, which is PostgreSQL's internal operator symbol for `LIKE`. Because of the leading `%`, the engine must read every row sequentially).*

### Example 5: Optimized Trigram GIN Search (`pg_trgm`)
In PostgreSQL, you can enable fast index seeks on arbitrary substring searches (`'%substring%'`) using the `pg_trgm` extension:

```sql
-- Step 1: Enable extension
CREATE EXTENSION IF NOT EXISTS pg_trgm;

-- Step 2: Create GIN Trigram index
CREATE INDEX idx_products_name_trgm ON products USING gin (product_name gin_trgm_ops);

-- Step 3: Run arbitrary substring query
EXPLAIN SELECT * FROM products WHERE product_name LIKE '%Laptop%';
```
```text
                                    QUERY PLAN                                    
----------------------------------------------------------------------------------
 Bitmap Heap Scan on products  (cost=8.00..12.01 rows=1 width=48)
   Recheck Cond: ((product_name)::text ~~ '%Laptop%'::text)
   ->  Bitmap Index Scan on idx_products_name_trgm  (cost=0.00..8.00 rows=1 width=0)
         Index Cond: ((product_name)::text ~~ '%Laptop%'::text)
```
*(With trigrams, even middle-wildcard searches use an index in milliseconds!)*

---

## 6. Best Practices

1. **Avoid Leading Wildcards When Possible**: Avoid `LIKE '%term'` unless backed by full-text search or a trigram index (`pg_trgm`). A leading wildcard prevents the optimizer from using standard B-Tree indexes.
2. **Use `ILIKE` in PostgreSQL for Case-Insensitive Searches**: Prefer `col ILIKE 'term%'` over `LOWER(col) LIKE LOWER('term%')`. It is cleaner, more expressive, and avoids unnecessary function overhead.
3. **Always Declare an Explicit `ESCAPE` Character**: When matching user-generated inputs that might contain literal `%` or `_`, always sanitize the string and supply an explicit `ESCAPE` clause.
4. **Use POSIX Regex for Complex Token Validation**: For validating emails, phone numbers, UUIDs, or postal codes, use regex (`~ '^[0-9]{5}(-[0-9]{4})?$'`) instead of fragile nested `LIKE` and `SUBSTRING` chains.
5. **Beware of Regex Denial of Service (ReDoS)**: Avoid evaluating complex unbounded regexes supplied directly by unauthenticated users, as catastrophic backtracking can pin database CPU cores at 100%.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Leading Wildcard (`LIKE '%term'`)** | Prevents B-Tree index seeks, forcing a full table sequential scan. | Use prefix search (`LIKE 'term%'`) or deploy a trigram GIN index. |
| **Assuming `LIKE` Is Always Case-Insensitive** | `LIKE` is case-sensitive in PostgreSQL and SQLite (for non-ASCII), returning zero rows if case mismatches. | Use `ILIKE` in PostgreSQL or `LOWER(col) LIKE LOWER(...)`. |
| **Forgetting to Escape `_` (Underscore)** | `WHERE filename LIKE 'test_file.csv'` matches `test1file.csv`, `test-file.csv`, and `testAfile.csv`! | Escape the underscore: `LIKE 'test\_file.csv' ESCAPE '\'`. |
| **Using `SIMILAR TO` Instead of Regex** | `SIMILAR TO` has bizarre hybrid syntax rules that confuse developers and optimize poorly. | Use standard `LIKE` for wildcards, or POSIX regex (`~`) for advanced patterns. |
| **Ignoring Collation in String Searches** | In PostgreSQL with default C locale, B-Tree indexes on text ignore prefix searches unless `varchar_pattern_ops` is used. | Create index with `CREATE INDEX idx ON tbl(col varchar_pattern_ops);`. |
| **Regex Matching Across NULLs** | `NULL ~ '^[A-Z]+'` yields `UNKNOWN`, discarding the row. | Ensure columns are `NOT NULL` or use `COALESCE`. |

---

## 8. Performance & Security Considerations

### Performance
- **Prefix Sargability**: `LIKE 'prefix%'` is internally rewritten by the query planner as:
  `WHERE col >= 'prefix' AND col < 'prefiy'`
  This allows the B-Tree index to perform a direct index range scan.
- **Trigram Index Overhead**: GIN trigram indexes (`pg_trgm`) allow arbitrary substring lookups, but consume 2x–3x more disk storage than standard B-Trees and slow down write operations.

### Security
- **Wildcard Injection**:
  - If user search terms are directly interpolated into `LIKE` clauses:
    ```sql
    query = "SELECT * FROM users WHERE name LIKE '" + user_input + "%'";
    ```
  - If a user passes `%` or `_`, they can bypass authentication or extract data. Always sanitize wildcards:
    ```python
    sanitized = user_input.replace('\\', '\\\\').replace('%', '\\%').replace('_', '\\_')
    ```

---

## 9. Comparison Matrix

### Pattern Matching Methods Compared

| Method | Syntax | Standard | Case Sensitivity | B-Tree Sargable? | Best Used For |
|:---|:---|:---:|:---:|:---:|:---|
| **ANSI `LIKE`** | `col LIKE 'A%'` | SQL-92 | Dialect-dependent | Yes (Prefix only) | Simple prefixes, suffixes, domain checks |
| **`ILIKE`** | `col ILIKE 'a%'` | Vendor (Postgres) | Strictly Case-Insensitive | Yes (with `pattern_ops`) | User-facing text searches in PostgreSQL |
| **`SIMILAR TO`** | `col SIMILAR TO '(A\|B)%'`| SQL:1999 | Case-Sensitive | Rarely | Legacy standard queries |
| **POSIX Regex** | `col ~ '^[A-Z]{3}'` | POSIX | Configurable (`~` vs `~*`)| No (Requires GIN) | Data validation (emails, phone numbers, UUIDs) |
| **Trigram Search** | `col LIKE '%substr%'` | Extension (`pg_trgm`) | Configurable | Yes (via GIN index) | Typo-tolerant search, autocomplete, substring search |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: Why can `LIKE 'John%'` use a standard B-Tree index, while `LIKE '%John'` cannot?
<details>
<summary>Answer</summary>
A B-Tree index stores strings in sorted lexicographical order from left to right (like words in a physical dictionary). 

- In `LIKE 'John%'`, the left anchor is known. The database planner rewrites this as a bounded range scan: `WHERE col >= 'John' AND col < 'Joho'`. The engine seeks directly to the first leaf node matching `'John'` and scans forward in $O(\log N)$ time.
- In `LIKE '%John'`, the prefix is unknown. Matching values could be `'Elton John'`, `'Papa John'`, or `'Little John'`, which are scattered across completely different pages throughout the entire B-Tree. The engine cannot navigate to any specific branch and must scan every row in $O(N)$ time.
</details>

#### Q2: What is a Trigram index, and how does it enable indexing for arbitrary `LIKE '%substring%'` queries?
<details>
<summary>Answer</summary>
A trigram is a continuous sequence of three consecutive characters extracted from a string (e.g., `'laptop'` breaks into trigrams: `"  l"`, `" la"`, `"lap"`, `"apt"`, `"pto"`, `"top"`, `"op "`).

In PostgreSQL, the `pg_trgm` extension with a Generalized Inverted Index (GIN) indexes every individual trigram. When a query searches for `LIKE '%laptop%'`, the engine decomposes the search string into trigrams, looks up matching row IDs in the GIN index for each trigram, and calculates the intersection, locating matching rows without scanning the entire heap.
</details>

#### Q3: What is the purpose of the `varchar_pattern_ops` operator class in PostgreSQL?
<details>
<summary>Answer</summary>
By default, PostgreSQL creates B-Tree indexes on text using locale-specific collations (e.g., `en_US.UTF-8`), which enforce complex cultural sorting rules (ignoring punctuation, case folding). 

Under non-C locales, the standard B-Tree index cannot be used to satisfy `LIKE 'prefix%'` because byte ordering does not strictly match linguistic collation ordering. Specifying `varchar_pattern_ops` creates the index using raw byte-by-byte comparisons, enabling the optimizer to execute index range seeks for `LIKE` prefix queries under any locale.
</details>

#### Q4: What is the difference between `~` and `~*` in PostgreSQL?
<details>
<summary>Answer</summary>
Both are POSIX regular expression matching operators:
- `~`: Performs a **case-sensitive** regular expression match (`'Alice' ~ '^a'` returns `FALSE`).
- `~*`: Performs a **case-insensitive** regular expression match (`'Alice' ~* '^a'` returns `TRUE`).
Their negations are `!~` (case-sensitive non-match) and `!~*` (case-insensitive non-match).
</details>

### Practical Pattern Puzzles

#### Q5: Write an SQL query to find all users whose email address is strictly from a `.edu` or `.gov` top-level domain.
<details>
<summary>Answer</summary>

```sql
-- Using LIKE:
SELECT user_id, email
FROM users
WHERE email LIKE '%.edu' OR email LIKE '%.gov';

-- Using POSIX Regex (PostgreSQL):
SELECT user_id, email
FROM users
WHERE email ~* '\.(edu|gov)$';
```
</details>

#### Q6: How do you search for table names containing an actual underscore in the `information_schema.tables` catalog?
<details>
<summary>Answer</summary>

```sql
SELECT table_name
FROM information_schema.tables
WHERE table_name LIKE '%\_%' ESCAPE '\'
  AND table_schema = 'public';
```
*(Without `ESCAPE '\'`, the underscore acts as a single-character wildcard, matching all table names with at least one character).*
</details>

#### Q7: Write a regex query to extract the domain name from a URL column (e.g., extracting `'github.com'` from `'https://github.com/torvalds/linux'`).
<details>
<summary>Answer</summary>

```sql
-- In PostgreSQL using substring with regex pattern:
SELECT 
    url,
    SUBSTRING(url FROM 'https?://([^/]+)') AS domain_name
FROM web_links;
```
</details>

#### Q8: Write a query to validate that a `postal_code` column matches either standard US 5-digit format (`12345`) or 9-digit ZIP+4 format (`12345-6789`).
<details>
<summary>Answer</summary>

```sql
SELECT postal_code
FROM addresses
WHERE postal_code ~ '^[0-9]{5}(-[0-9]{4})?$';
```
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Prefix Brand Search
Select all products from `products` where `product_name` begins with `'Sony'` or `'Apple'`.
<details>
<summary>Solution</summary>

```sql
SELECT product_id, product_name, retail_price
FROM products
WHERE product_name LIKE 'Sony%' OR product_name LIKE 'Apple%';
```
</details>

#### Exercise 2: Single-Character Wildcard Search
Find all customer records where the `first_name` has exactly 4 letters, ending in `'am'` (e.g., `'Liam'`, `'Adam'`).
<details>
<summary>Solution</summary>

```sql
SELECT customer_id, first_name, last_name
FROM customers
WHERE first_name LIKE '__am';
```
</details>

### Intermediate Exercises

#### Exercise 3: Case-Insensitive Pattern Match
Write a query to search for products containing the word `'laptop'` anywhere in `product_name`, regardless of casing, using PostgreSQL `ILIKE`.
<details>
<summary>Solution</summary>

```sql
SELECT product_id, product_name, retail_price
FROM products
WHERE product_name ILIKE '%laptop%';
```
</details>

#### Exercise 4: Literal Percent Match
Retrieve all promotion records where the `description` contains a literal `'%'` symbol, using `!` as the escape character.
<details>
<summary>Solution</summary>

```sql
SELECT promo_id, description
FROM promotions
WHERE description LIKE '%!%%' ESCAPE '!';
```
</details>

### Advanced Exercises

#### Exercise 5: Regex Validation Constraint
Add a `CHECK` constraint to a `users` table verifying that `username` is between 3 and 20 alphanumeric characters or underscores, and begins with a letter.
<details>
<summary>Solution</summary>

```sql
ALTER TABLE users 
    ADD CONSTRAINT chk_valid_username 
    CHECK (username ~ '^[A-Za-z][A-Za-z0-9_]{2,19}$');
```
</details>

#### Exercise 6: Profiling Trigram Index Scan
Create a trigram index on `products(product_name)` using `pg_trgm` and run `EXPLAIN` on a middle-wildcard query (`LIKE '%Laptop%'`) to verify that a `Bitmap Index Scan` is used instead of a `Seq Scan`.
<details>
<summary>Solution</summary>

```sql
CREATE EXTENSION IF NOT EXISTS pg_trgm;
CREATE INDEX idx_products_trgm ON products USING gin (product_name gin_trgm_ops);

EXPLAIN SELECT * FROM products WHERE product_name LIKE '%Laptop%';
```
</details>

---

## 12. Summary & Quick Reference

### Wildcards & Regex Cheatsheet
- **`%`**: Zero or more characters (`LIKE 'A%'`).
- **`_`**: Exactly one character (`LIKE 'L__'`).
- **`ESCAPE '\'`**: Escape literal wildcards (`LIKE '100\%%' ESCAPE '\'`).
- **`ILIKE`**: Case-insensitive `LIKE` (PostgreSQL).
- **`~`**: POSIX regex match (case-sensitive).
- **`~*`**: POSIX regex match (case-insensitive).
- **Sargability Rule**: Left-anchored prefix (`'prefix%'`) uses B-Tree index; middle/leading wildcard (`'%suffix'`) requires trigram GIN index.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [Pattern Matching](https://www.postgresql.org/docs/17/functions-matching.html)
- PostgreSQL Documentation: [pg_trgm Extension](https://www.postgresql.org/docs/17/pgtrgm.html)
- Markus Winand: *"Indexing LIKE and regex queries"* (Use The Index, Luke)
- [10 - Scalar Numeric & Mathematical Functions](./10-numeric-functions.md)
