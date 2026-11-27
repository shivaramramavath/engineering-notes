# 11: String Functions & Text Manipulation

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [10 - Scalar Numeric & Mathematical Functions](./10-numeric-functions.md) |
| **Target Audience** | Software Engineers, Data Engineers, Backend Developers |
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

Text is the universal medium of human communication. In relational databases, string attributes represent names, email addresses, descriptions, URLs, categorical tags, JSON payloads, and unstructured logs. While relational databases are optimized for structured numerical and relational operations, real-world data pipelines require extensive string cleaning, sanitization, parsing, formatting, and restructuring directly within the database tier.

However, string manipulation in SQL introduces subtle traps:
- In SQL, **string indexing is 1-based**, not 0-based as in Python, JavaScript, or C.
- Concatenating a string with `NULL` using the standard `||` operator returns `NULL`, erasing entire strings.
- In multi-byte UTF-8 character sets, a character's length (`CHAR_LENGTH`) differs fundamentally from its physical storage size (`OCTET_LENGTH`), creating buffer truncation bugs.
- Dialect naming differences (`SUBSTRING` vs `SUBSTR`, `LENGTH` vs `LEN`, `STRING_AGG` vs `GROUP_CONCAT`) complicate cross-engine portability.

Mastering SQL’s rich suite of scalar string functions ensures clean data transformation, robust sanitization, and high-performance text handling.

---

## 2. Core Concepts

### 1. 1-Based Indexing
In standard SQL, the first character of a string is located at **index 1**, NOT index 0:
```sql
SELECT SUBSTRING('PostgreSQL', 1, 4); -- Returns 'Post'
```
If you pass `0` as the starting position, standard SQL treats index 0 as existing before the string, resulting in counter-intuitive sliced lengths:
```sql
SELECT SUBSTRING('PostgreSQL', 0, 4); -- Returns 'Pos' (3 characters!)
```

### 2. Character Length vs Byte Length (Octets)
In modern databases utilizing UTF-8 encoding:
- `CHAR_LENGTH(str)` / `LENGTH(str)`: Returns the count of human-readable Unicode characters.
- `OCTET_LENGTH(str)`: Returns the count of physical bytes stored on disk.
  - Standard ASCII letters (`'A'`) consume 1 byte.
  - Accented Latin characters (`'é'`) consume 2 bytes.
  - Asian characters (`'中'`) consume 3 bytes.
  - Emojis (`'🚀'`) consume 4 bytes.

```
String: 'Rocket 🚀'
CHAR_LENGTH: 8 characters ('R','o','c','k','e','t',' ','🚀')
OCTET_LENGTH: 11 bytes (6 ASCII + 1 Space + 4 UTF-8 bytes for emoji)
```

### 3. Concatenation and Null Safety: `||` vs `CONCAT()` vs `CONCAT_WS()`
- **Standard `||` Operator**:
  - ANSI SQL standard concatenation.
  - If any operand is `NULL`, the entire result is **`NULL`**!
  - `'Hello ' || NULL || 'World'` $\rightarrow$ `NULL`.
- **`CONCAT(a, b, ...)`**:
  - Treats `NULL` as an empty string (`''`), preventing null propagation.
  - `CONCAT('Hello ', NULL, 'World')` $\rightarrow$ `'Hello World'`.
- **`CONCAT_WS(separator, a, b, ...)` (Concatenate With Separator)**:
  - Joins non-null arguments using the first parameter as a delimiter, automatically skipping nulls.
  - `CONCAT_WS(', ', 'New York', NULL, 'USA')` $\rightarrow$ `'New York, USA'`.

---

## 3. How It Works Internally

### Variable-Length Text Storage and TOAST

```
Text Column Payload Structure (PostgreSQL varlena header):
+--------------------------------------------------------------------------+
| 1-Byte or 4-Byte Length Header (encodes byte length & compression flag)  |
+--------------------------------------------------------------------------+
| UTF-8 Encoded Byte Array                                                 |
| ['P', 'o', 's', 't', 'g', 'r', 'e', 'S', 'Q', 'L']                      |
+--------------------------------------------------------------------------+
```

- In PostgreSQL, text columns use the `varlena` (variable-length array) memory structure.
- Short strings ($\le 126$ bytes) use a compact 1-byte header.
- Longer strings use a 4-byte header.
- Because strings are variable-length arrays, operations like `LENGTH()` on multi-byte UTF-8 strings require the engine to traverse the byte stream, decoding UTF-8 multi-byte sequences into character code points.

---

## 4. Syntax & Usage

### Core String Functions Syntax

```sql
SELECT 
    -- Concatenation
    'Postgre' || 'SQL' AS pipe_concat,
    CONCAT('User: ', username, ' (', status, ')') AS safe_concat,
    CONCAT_WS(', ', street, city, state, zip) AS formatted_address,

    -- Case Alteration
    UPPER('database') AS upper_str,      -- 'DATABASE'
    LOWER('DATABASE') AS lower_str,      -- 'database'
    INITCAP('edgar f. codd') AS title_str, -- 'Edgar F. Codd'

    -- Slicing & Extraction
    SUBSTRING('Engineering' FROM 1 FOR 6) AS sub_str, -- 'Engine'
    LEFT('Framework', 5) AS left_str,                 -- 'Frame'
    RIGHT('Framework', 4) AS right_str,               -- 'work'

    -- Trimming & Padding
    TRIM(BOTH ' ' FROM '  padded  ') AS trimmed_str,  -- 'padded'
    LPAD('42', 6, '0') AS padded_code,                -- '000042'
    RPAD('Title', 10, '.') AS dotted_line,            -- 'Title.....'

    -- Search & Replace
    POSITION('cat' IN 'certificate') AS pos_val,      -- 0 (Not found)
    STRPOS('certificate', 'cat') AS strpos_val,       -- 0
    REPLACE('2024-04-01', '-', '/') AS replaced_str,  -- '2024/04/01'
    TRANSLATE('123-ABC', '123', '789') AS trans_str;  -- '789-ABC'
```

### Multi-Dialect Comparison Matrix

| Function / Capability | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Concatenation Operator** | `||` | `||` (if `PIPES_AS_CONCAT` enabled) | `||` | `+` (Plus operator) |
| **`CONCAT()` Function** | Supported (Ignores NULL) | Supported (Returns NULL if any arg is NULL!)| Supported | Supported (Ignores NULL) |
| **`CONCAT_WS()`** | Supported | Supported | Not native | Supported (since 2017) |
| **Character Length** | `LENGTH()` or `CHAR_LENGTH()`| `CHAR_LENGTH()` | `LENGTH()` | `LEN()` (Ignores trailing spaces!) |
| **Byte Length** | `OCTET_LENGTH()` | `LENGTH()` | `OCTET_LENGTH()` (3.44+) | `DATALENGTH()` |
| **Title Case (`INITCAP`)** | `INITCAP()` | Not built-in | Not built-in | Not built-in |
| **String Slicing** | `SUBSTRING()` or `SUBSTR()`| `SUBSTRING()` or `SUBSTR()`| `SUBSTR()` only | `SUBSTRING()` only |

---

## 5. 5 Practical Progressive Examples

### Example 1: Robust Full Name Formatting with `CONCAT_WS`
Formatting a customer's display name cleanly when `middle_name` or `suffix` may be NULL:

```sql
CREATE TABLE name_formatting (
    first_name VARCHAR(50),
    middle_name VARCHAR(50),
    last_name VARCHAR(50),
    suffix VARCHAR(10)
);

INSERT INTO name_formatting VALUES
('Arthur', 'Conan', 'Doyle', NULL),
('Alex', NULL, 'Morgan', 'Jr.');

-- ANTI-PATTERN using ||:
SELECT first_name || ' ' || middle_name || ' ' || last_name FROM name_formatting;
-- Returns NULL for Alex Morgan!

-- PRODUCTION STANDARD using CONCAT_WS:
SELECT 
    CONCAT_WS(' ', first_name, middle_name, last_name, suffix) AS full_display_name
FROM name_formatting;
```
```text
+-----------------------+
| full_display_name     |
+-----------------------+
| Arthur Conan Doyle    |
| Alex Morgan Jr.       |
+-----------------------+
```

### Example 2: Parsing Structured Tokens with `SPLIT_PART`
Extracting the domain name from email addresses in PostgreSQL:

```sql
SELECT 
    email,
    SPLIT_PART(email, '@', 1) AS local_account,
    SPLIT_PART(email, '@', 2) AS email_domain
FROM customers;
```
```text
+--------------------+---------------+--------------+
| email              | local_account | email_domain |
+--------------------+---------------+--------------+
| alex.m@example.com | alex.m        | example.com  |
| priya.s@example.com| priya.s       | example.com  |
| david.k@example.com| david.k       | example.com  |
+--------------------+---------------+--------------+
```

### Example 3: Edge Case (SQL Server `LEN()` Truncating Trailing Whitespace)
Demonstrating the infamous SQL Server `LEN()` gotcha:

In SQL Server:
```sql
-- SQL Server's LEN() SILENTLY IGNORES trailing spaces!
SELECT LEN('Test   '); -- Returns 4, NOT 7!
SELECT DATALENGTH('Test   '); -- Returns 7
```

In PostgreSQL, MySQL, and SQLite:
```sql
SELECT LENGTH('Test   '); -- Correctly returns 7!
```

### Example 4: Failure Mode (0-Based Slicing Bug)
Demonstrating how passing index 0 creates off-by-one errors:

```sql
-- Desired: The first 4 characters of 'Database'
SELECT 
    SUBSTRING('Database', 1, 4) AS correct_1_based,
    SUBSTRING('Database', 0, 4) AS buggy_0_based;
```
```text
+-----------------+---------------+
| correct_1_based | buggy_0_based |
+-----------------+---------------+
| Data            | Dat           |
+-----------------+---------------+
```
*(Passing `0` as the start index causes the slice to cover positions 0, 1, 2, and 3. Since position 0 does not exist, only 3 characters `'Dat'` are emitted).*

### Example 5: Optimized Fixed-Width Code Generation via `LPAD`
Generating standardized zero-padded accounting identifiers (e.g., `'INV-000042'`):

```sql
SELECT 
    order_id,
    'INV-' || LPAD(order_id::text, 6, '0') AS formatted_invoice_number
FROM orders;
```
```text
+----------+--------------------------+
| order_id | formatted_invoice_number |
+----------+--------------------------+
|     1001 | INV-001001               |
|     1002 | INV-001002               |
|     1003 | INV-001003               |
+----------+--------------------------+
```

---

## 6. Best Practices

1. **Always Use 1-Based Indexing**: Remember that SQL string indexes start at 1. Slicing with `SUBSTRING(str, 1, len)` is correct; using 0 drops one character.
2. **Prefer `CONCAT_WS()` for Joining Multiple Nullable Columns**: Avoid messy chains of `COALESCE(col || ' ', '')`. `CONCAT_WS(' ', ...)` automatically skips nulls without leaving dangling spaces.
3. **Use `CHAR_LENGTH()` When Character Count Matters**: Never rely on `OCTET_LENGTH()` to validate string length constraints (e.g., tweet length or password bounds) in multi-byte UTF-8 environments.
4. **Clean Leading/Trailing Spaces on Ingestion**: Use `TRIM()` or `BTRIM()` during `INSERT` or in `CHECK` constraints to prevent invisible whitespace from corrupting username or email lookups.
5. **Use Functional Indexes for Case-Insensitive Lookups**: If querying `WHERE LOWER(email) = ?`, create an expression index: `CREATE INDEX idx_lower_email ON users(LOWER(email));`.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **0-Based Indexing Assumption** | `SUBSTRING('text', 0, 3)` returns only 2 characters because index 0 is out-of-bounds. | Start indexing at 1: `SUBSTRING('text', 1, 3)`. |
| **Concatenating with NULL using `||`** | Any NULL operand converts the entire concatenated string to `NULL`. | Use `CONCAT()` or `CONCAT_WS()`. |
| **Confusing Characters with Bytes** | In UTF-8, an emoji or accented character takes up to 4 bytes; checking length with `OCTET_LENGTH` can reject valid short strings. | Use `CHAR_LENGTH()` or `LENGTH()`. |
| **SQL Server `LEN()` Ignoring Spaces** | `LEN('code ')` returns `4`, failing whitespace validation checks. | In SQL Server, use `DATALENGTH()`. |
| **Using `REPLACE` for Single-Character Translation** | Chaining multiple `REPLACE(REPLACE(s, 'a', 'x'), 'b', 'y')` is slow and inefficient. | Use `TRANSLATE(s, 'ab', 'xy')`. |
| **Case Mismatches in Comparisons** | `'Alice'` does not equal `'alice'` in PostgreSQL. | Use `LOWER(a) = LOWER(b)` or `ILIKE`. |

---

## 8. Performance & Security Considerations

### Performance
- **Expression Indexes on Manipulated Text**:
  - Functions in `WHERE` clauses (e.g., `WHERE LOWER(username) = 'admin'`) defeat standard B-Tree indexes.
  - Fix: Build a functional index:
    ```sql
    CREATE INDEX idx_users_lower_user ON users (LOWER(username));
    ```
- **String Memory Overhead in Sorts**:
  - Wide text columns (`TEXT`) inflate tuple byte sizes in `work_mem`. Project only necessary string attributes to prevent in-memory sorts from spilling to disk.

### Security
- **Cross-Site Scripting (XSS) Sanitization**:
  - While databases can escape text using `REPLACE()`, HTML sanitization is best performed in the application presentation layer. Storing raw data unescaped in the database preserves its canonical semantic representation.

---

## 9. Comparison Matrix

### String Functions Across Dialects

| Action | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Concatenation** | `||` or `CONCAT()` | `CONCAT()` or `||` | `||` | `+` or `CONCAT()` |
| **Concat with Delimiter** | `CONCAT_WS(sep, ...)` | `CONCAT_WS(sep, ...)` | Custom extension | `CONCAT_WS(sep, ...)` |
| **Substring** | `SUBSTRING(s, start, len)`| `SUBSTRING(s, start, len)`| `SUBSTR(s, start, len)`| `SUBSTRING(s, start, len)`|
| **Character Length** | `LENGTH(s)` / `CHAR_LENGTH(s)`| `CHAR_LENGTH(s)` | `LENGTH(s)` | `LEN(s)` |
| **Left / Right Trim** | `TRIM(s)`, `LTRIM(s)`, `RTRIM(s)`| `TRIM(s)`, `LTRIM(s)`, `RTRIM(s)`| `TRIM(s)`, `LTRIM(s)`, `RTRIM(s)`| `TRIM(s)`, `LTRIM(s)`, `RTRIM(s)`|
| **String Aggregation** | `STRING_AGG(col, sep)` | `GROUP_CONCAT(col SEPARATOR sep)`| `GROUP_CONCAT(col, sep)`| `STRING_AGG(col, sep)` |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: Why is string indexing 1-based in SQL instead of 0-based like most programming languages?
<details>
<summary>Answer</summary>
SQL is a declarative mathematical language based on relational calculus and set theory, not an imperative language tied to hardware memory addressing. 

In low-level systems languages (C, assembly), index 0 represents the memory pointer offset (`pointer + 0 * sizeof(char)`). SQL abstracts away hardware memory pointers; its ordinal numbers represent human mathematical positions: the 1st character, 2nd character, and 3rd character.
</details>

#### Q2: What is the difference between `LENGTH()` and `OCTET_LENGTH()` in PostgreSQL when working with UTF-8 data?
<details>
<summary>Answer</summary>
- `LENGTH(str)` (or `CHAR_LENGTH`): Measures the count of logical Unicode characters (code points), regardless of how many physical bytes each character consumes.
- `OCTET_LENGTH(str)`: Measures the exact count of physical bytes (octets) required to encode and store the string on disk.

For ASCII strings, both return identical numbers. For multi-byte strings (e.g., `'Café ☕'`), `LENGTH()` returns 6 characters, while `OCTET_LENGTH()` returns 9 bytes (4 bytes for `'Caf'`, 2 bytes for `'é'`, 1 byte for space, and 3 bytes for `'☕'`).
</details>

#### Q3: How does standard SQL concatenation (`||`) differ from MySQL's default `CONCAT()` function when handling NULLs?
<details>
<summary>Answer</summary>
- In standard SQL (and PostgreSQL/SQLite), `||` follows Three-Valued Logic: concatenating any string with NULL yields `NULL` (`'Hello' || NULL` $\rightarrow$ `NULL`).
- In MySQL, the `CONCAT()` function historically also returns `NULL` if any argument is NULL.
- In PostgreSQL, the `CONCAT()` function deviates from `||` by treating NULLs as empty strings: `CONCAT('Hello', NULL)` yields `'Hello'`.
</details>

#### Q4: What is the purpose of `TRANSLATE()` and how does it differ from `REPLACE()`?
<details>
<summary>Answer</summary>
- `REPLACE(str, search_str, replacement_str)`: Replaces full substring occurrences. It matches the complete `search_str` as a contiguous block.
- `TRANSLATE(str, from_chars, to_chars)`: Performs character-by-character mapping. It replaces each single character in `from_chars` with the corresponding character at the same ordinal position in `to_chars`.
Example: `TRANSLATE('123-456', '123', 'ABC')` converts 1 to A, 2 to B, and 3 to C, returning `'ABC-456'`.
</details>

### Practical String Puzzles

#### Q5: Write an SQL query to extract the top-level domain (TLD) from a website column (e.g., extracting `'org'` from `'wikipedia.org'`).
<details>
<summary>Answer</summary>

```sql
-- Using SPLIT_PART with reverse indexing trick, or regex:
SELECT 
    domain,
    -- Extract characters after the last dot:
    SUBSTRING(domain FROM '\.([^\.]+)$') AS tld
FROM websites;

-- Or using standard split:
SELECT 
    domain,
    REVERSE(SPLIT_PART(REVERSE(domain), '.', 1)) AS tld
FROM websites;
```
</details>

#### Q6: How do you mask a sensitive credit card number in SQL so that only the last 4 digits are visible (e.g., `'************1234'`)?
<details>
<summary>Answer</summary>

```sql
SELECT 
    card_number,
    REPEAT('*', LENGTH(card_number) - 4) || RIGHT(card_number, 4) AS masked_card
FROM payment_methods;
```
</details>

#### Q7: Write a query to capitalize the first letter of every word in a title column in PostgreSQL.
<details>
<summary>Answer</summary>

```sql
SELECT INITCAP('the quick brown fox jumps over the lazy dog') AS title_cased;
-- Returns: 'The Quick Brown Fox Jumps Over The Lazy Dog'
```
</details>

#### Q8: Write an SQL query that formats a list of tags associated with an article into a single comma-separated string ordered alphabetically.
<details>
<summary>Answer</summary>

```sql
-- In PostgreSQL:
SELECT 
    article_id,
    STRING_AGG(tag_name, ', ' ORDER BY tag_name ASC) AS tags_list
FROM article_tags
GROUP BY article_id;

-- In SQLite:
SELECT 
    article_id,
    GROUP_CONCAT(tag_name, ', ') AS tags_list
FROM (SELECT article_id, tag_name FROM article_tags ORDER BY tag_name)
GROUP BY article_id;
```
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Uppercase Email Normalization
Select `customer_id` and normalized lowercase email addresses from `customers`, removing any leading or trailing whitespace.
<details>
<summary>Solution</summary>

```sql
SELECT 
    customer_id,
    LOWER(TRIM(email)) AS clean_email
FROM customers;
```
</details>

#### Exercise 2: Left Substring Extraction
Extract the 3-letter area code from US phone numbers formatted as `'(555) 123-4567'`.
<details>
<summary>Solution</summary>

```sql
SELECT 
    phone_number,
    SUBSTRING(phone_number, 2, 3) AS area_code
FROM contacts;
```
</details>

### Intermediate Exercises

#### Exercise 3: Zero-Padded Sequence Code
Given a sequence number integer `seq_num`, format it as a fixed 8-character string prefixed with `'TX-'` and zero-padded to 5 digits (e.g., 42 becomes `'TX-00042'`).
<details>
<summary>Solution</summary>

```sql
SELECT 
    seq_num,
    'TX-' || LPAD(seq_num::text, 5, '0') AS formatted_tx_code
FROM transaction_sequences;
```
</details>

#### Exercise 4: Clean Name Construction
Using `CONCAT_WS`, construct a mailing label combining `first_name`, `last_name`, `company_name`, `address_line1`, `city`, and `postal_code`, ensuring no dangling double spaces appear when `company_name` is NULL.
<details>
<summary>Solution</summary>

```sql
SELECT 
    CONCAT_WS(' | ', 
        first_name || ' ' || last_name, 
        company_name, 
        address_line1, 
        city || ', ' || postal_code
    ) AS mailing_label
FROM shipping_addresses;
```
</details>

### Advanced Exercises

#### Exercise 5: Parsing Key-Value Strings into Tabular Columns
Given an unstructured logging string `log_data = 'user=42;status=success;ip=192.168.1.1;'`, extract the IP address using standard string functions.
<details>
<summary>Solution</summary>

```sql
SELECT 
    log_data,
    SUBSTRING(
        SUBSTRING(log_data FROM 'ip=([^;]+)'), 
        1, 50
    ) AS ip_address
FROM system_logs;

-- Or using pure split_part:
SELECT 
    log_data,
    SPLIT_PART(SPLIT_PART(log_data, 'ip=', 2), ';', 1) AS ip_address
FROM system_logs;
```
</details>

#### Exercise 6: Verifying Multi-Byte Length Discrepancy
Write a query displaying strings with emojis or Asian characters, showing both the character count and physical byte size.
<details>
<summary>Solution</summary>

```sql
SELECT 
    sample_text,
    CHAR_LENGTH(sample_text) AS characters,
    OCTET_LENGTH(sample_text) AS bytes,
    OCTET_LENGTH(sample_text) - CHAR_LENGTH(sample_text) AS multi_byte_overhead
FROM (
    VALUES 
        ('Hello World'),
        ('PostgreSQL 🐘'),
        ('東京 (Tokyo)')
) AS t(sample_text);
```
</details>

---

## 12. Summary & Quick Reference

### String Functions Quick Reference
- **Concatenation**: `||` (Propagates NULL), `CONCAT(a, b)` (Ignores NULL), `CONCAT_WS(sep, a, b)` (With separator).
- **Indexing**: 1-based (`SUBSTRING('SQL', 1, 1) = 'S'`).
- **Length**: `CHAR_LENGTH(s)` (Characters), `OCTET_LENGTH(s)` (Bytes).
- **Case**: `UPPER(s)`, `LOWER(s)`, `INITCAP(s)`.
- **Trimming**: `TRIM(s)` (Both ends), `LTRIM(s)`, `RTRIM(s)`.
- **Padding**: `LPAD(s, len, fill)`, `RPAD(s, len, fill)`.
- **Replacing**: `REPLACE(s, from, to)`, `TRANSLATE(s, from_chars, to_chars)`.
- **Splitting**: `SPLIT_PART(s, delimiter, position)`.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [String Functions and Operators](https://www.postgresql.org/docs/17/functions-string.html)
- SQLite Documentation: [Core String Functions](https://www.sqlite.org/lang_corefunc.html)
- Modern SQL: [String Concatenation in SQL](https://modern-sql.com/feature/concatenation)
- [12 - Date, Time & Timezone Handling](./12-date-and-time.md)
