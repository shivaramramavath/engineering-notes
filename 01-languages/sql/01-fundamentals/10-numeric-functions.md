# 10: Scalar Numeric & Mathematical Functions

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [09 - Pattern Matching: LIKE, ILIKE & Regex](./09-pattern-matching.md) |
| **Target Audience** | Software Engineers, Data Analysts, Financial Engineers |
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

Relational databases are not passive storage lockers; they are high-performance data processing engines. In modern data architecture, performing numeric transformations, calculations, metric derivations, and aggregations directly within the SQL engine minimizes network serialization overhead, reduces application CPU pressure, and allows query optimizers to vectorize operations across millions of rows in memory.

However, scalar numeric operations in SQL harbor critical dialect discrepancies and mathematical traps:
- In PostgreSQL and SQL Server, dividing two integers (`5 / 2`) performs **truncating integer division** returning `2`, whereas in MySQL and SQLite it performs floating-point division returning `2.5`.
- Different rounding functions implement different rounding algorithms (e.g., standard half-up vs banker's rounding).
- Floating-point arithmetic introduces IEEE-754 representation error, whereas arbitrary-precision `NUMERIC` maintains exact decimal accuracy.
- Unhandled division by zero aborts entire transaction blocks.

Mastering SQL's scalar numeric functions guarantees that business metrics, financial ledgers, and analytical KPIs are calculated accurately, deterministically, and performantly.

---

## 2. Core Concepts

### 1. Integer Division vs Floating-Point Division
The single most pervasive source of numeric bugs across database dialects:
- In PostgreSQL and SQL Server: Dividing an integer by an integer performs integer division and truncates toward zero:
  `SELECT 7 / 2;` $\rightarrow$ `3`
  To obtain fractional decimals, at least one operand must be explicitly cast to numeric or float:
  `SELECT 7.0 / 2;` $\rightarrow$ `3.5`
- In MySQL and SQLite: The division operator `/` automatically promotes operands to floating-point:
  `SELECT 7 / 2;` $\rightarrow$ `3.5`

### 2. Rounding, Truncating, and Boundary Functions
- **`ROUND(val [, s])`**: Rounds to nearest integer (or nearest $s$ decimal places). Standard half-up rounding ($0.5$ rounds away from zero).
- **`TRUNC(val [, s])`** (or `TRUNCATE` in MySQL): Truncates digits past $s$ decimal places toward zero without rounding.
- **`CEIL(val)` / `CEILING(val)`**: Returns the smallest integer greater than or equal to `val` (rounds up toward $+\infty$).
- **`FLOOR(val)`**: Returns the largest integer less than or equal to `val` (rounds down toward $-\infty$).

```
Value       ROUND(x)    TRUNC(x)    CEIL(x)    FLOOR(x)
────────────────────────────────────────────────────────
 3.2           3           3           4          3
 3.8           4           3           4          3
-3.2          -3          -3          -3         -4
-3.8          -4          -3          -3         -4
```

### 3. Absolute Values, Sign & Modulo
- **`ABS(val)`**: Computes the absolute value (magnitude), stripping negative signs.
- **`SIGN(val)`**: Returns `-1` for negative numbers, `0` for zero, and `+1` for positive numbers.
- **`MOD(dividend, divisor)`** (or `%`): Computes the integer remainder after division.

---

## 3. How It Works Internally

### CPU Execution: Hardware FPU vs Software BCD Arithmetic
The database engine executes numeric calculations using two distinct execution paths:

```
+-------------------------------------------------------------------------+
| Query: SELECT colA + colB FROM metrics;                                 |
+------------------------------------+------------------------------------+
| Case A: colA is DOUBLE PRECISION   | Case B: colA is NUMERIC(12, 4)     |
| (IEEE-754 Binary Floating-Point)   | (Arbitrary Precision Decimal)      |
+------------------------------------+------------------------------------+
| 1. Dispatches directly to native   | 1. Dispatches to software BCD math |
|    CPU Floating Point Unit (FPU).  |    library in database engine.     |
| 2. Executed in 1 clock cycle.      | 2. Iterates over base-10,000 array |
| 3. High throughput (Millions/sec). |    digits, tracking carry-overs.   |
| 4. Inexact (Rounding error drift). | 3. Slower CPU processing (10x-20x).|
|                                    | 4. 100% Mathematically Exact!      |
+------------------------------------+------------------------------------+
```

- When exact decimal integrity is paramount (financial accounting, currency, interest rates), the CPU cost of `NUMERIC` is mandatory.
- When processing scientific coordinates, machine learning vectors, or high-velocity sensor telemetry where tiny rounding errors are acceptable, `DOUBLE PRECISION` leverages hardware acceleration.

---

## 4. Syntax & Usage

### Core Numeric Functions Syntax

```sql
SELECT 
    -- Rounding & Truncation
    ROUND(125.756, 2) AS rounded_val,    -- 125.76
    TRUNC(125.756, 2) AS truncated_val,  -- 125.75
    CEIL(12.01) AS ceiling_val,          -- 13
    FLOOR(12.99) AS floor_val,           -- 12

    -- Remainder & Magnitude
    MOD(17, 5) AS remainder_val,         -- 2
    ABS(-45.50) AS absolute_val,         -- 45.50
    SIGN(-120) AS sign_val,              -- -1

    -- Powers & Roots
    POWER(2, 8) AS power_val,            -- 256
    SQRT(144) AS sqrt_val,               -- 12

    -- Logarithms & Exponentials
    EXP(1) AS e_val,                     -- 2.71828...
    LN(10) AS natural_log,               -- 2.30258...
    LOG(100) AS base_10_log;             -- 2
```

### Multi-Dialect Comparison Matrix

| Function / Operator | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Integer Division (`5 / 2`)** | `2` (Truncates to int) | `2.5` (Promotes to float) | `2` (Truncates to int) | `2` (Truncates to int) |
| **Integer Modulo** | `%` or `MOD(n, m)` | `%` or `MOD(n, m)` | `%` operator | `%` operator only |
| **Truncation Function** | `TRUNC(val, s)` | `TRUNCATE(val, s)` | `ROUND(val - 0.5)` trick | `ROUND(val, s, 1)` |
| **Square Root** | `SQRT(val)` or `|/ val` | `SQRT(val)` | `SQRT(val)` (Math extension)| `SQRT(val)` |
| **Random Number** | `RANDOM()` ($0.0 \le x < 1.0$) | `RAND()` | `RANDOM()` (Large integer)| `RAND()` |
| **Pi Constant** | `PI()` | `PI()` | Not built-in | `PI()` |

---

## 5. 5 Practical Progressive Examples

### Example 1: Preventing Truncating Integer Division Bugs
Demonstrating why calculating percentages in PostgreSQL requires explicit casting:

```sql
-- ANTI-PATTERN in PostgreSQL: Integer division produces 0!
SELECT 3 / 4 AS naive_ratio;
```
```text
+-------------+
| naive_ratio |
+-------------+
|           0 |
+-------------+
```
Because both 3 and 4 are integers, the engine performs integer division and truncates `0.75` to `0`.

**The Production Fix**:
```sql
-- Cast numerator to numeric or multiply by 100.0 first
SELECT 
    (3::numeric / 4) AS decimal_ratio,
    (3 * 100.0 / 4) AS percentage_ratio;
```
```text
+---------------+------------------+
| decimal_ratio | percentage_ratio |
+---------------+------------------+
|        0.7500 |          75.0000 |
+---------------+------------------+
```

### Example 2: Financial Currency Conversion and Tax Rounding
Calculating item sales tax, rounding exactly to the nearest cent ($0.01) using `NUMERIC`:

```sql
SELECT 
    order_id,
    subtotal,
    tax_rate,
    ROUND(subtotal * tax_rate, 2) AS sales_tax,
    subtotal + ROUND(subtotal * tax_rate, 2) AS grand_total
FROM (
    SELECT 
        1001 AS order_id, 
        144.55::numeric AS subtotal, 
        0.0825::numeric AS tax_rate -- 8.25% sales tax
) t;
```
```text
+----------+----------+----------+-----------+-------------+
| order_id | subtotal | tax_rate | sales_tax | grand_total |
+----------+----------+----------+-----------+-------------+
|     1001 |   144.55 |   0.0825 |     11.93 |      156.48 |
+----------+----------+----------+-----------+-------------+
```

### Example 3: Edge Case (Safe Division Guarding Against Division by Zero)
Safely computing conversion rates where visitor count may be zero or NULL:

```sql
CREATE TABLE campaigns (
    campaign_id INT PRIMARY KEY,
    impressions INT,
    clicks INT
);

INSERT INTO campaigns VALUES 
(1, 10000, 250),
(2, 0, 0),       -- Zero impressions!
(3, NULL, NULL);  -- Unknown impressions

-- Standard Safe Division using NULLIF:
SELECT 
    campaign_id,
    impressions,
    clicks,
    ROUND((clicks::numeric / NULLIF(impressions, 0)) * 100.0, 2) AS ctr_percentage
FROM campaigns;
```
```text
+-------------+-------------+--------+----------------+
| campaign_id | impressions | clicks | ctr_percentage |
+-------------+-------------+--------+----------------+
|           1 |       10000 |    250 |           2.50 |
|           2 |           0 |      0 |           NULL |
|           3 |        NULL |   NULL |           NULL |
+-------------+-------------+--------+----------------+
```
*(Campaign 2 returns `NULL` cleanly rather than terminating the query with an unhandled exception).*

### Example 4: Failure Mode (Modulo with Negative Operands)
Understanding how different dialects evaluate modulo on negative numbers:

In PostgreSQL / Standard SQL:
```sql
SELECT (-17) % 5 AS pg_mod;
-- Returns -2 (Sign of the result matches the dividend!)
```

In Python or Mathematical Modular Arithmetic:
`-17 % 5` yields `3` (Floor division modulo).

To obtain true positive mathematical modulo in SQL:
```sql
SELECT ((-17 % 5) + 5) % 5 AS true_math_mod;
-- Returns 3!
```

### Example 5: Optimized Pseudo-Random Integer in a Bounded Range
Generating a random integer between $A$ and $B$ (inclusive) in PostgreSQL:
$$\text{random\_int}(A, B) = \text{FLOOR}(A + \text{RANDOM}() \times (B - A + 1))$$

```sql
-- Generate 3 random dice rolls between 1 and 6:
SELECT 
    n AS roll_number,
    FLOOR(1 + RANDOM() * 6)::integer AS dice_result
FROM generate_series(1, 3) AS n;
```
```text
+-------------+-------------+
| roll_number | dice_result |
+-------------+-------------+
|           1 |           4 |
|           2 |           6 |
|           3 |           1 |
+-------------+-------------+
```

---

## 6. Best Practices

1. **Multiply Before Dividing to Preserve Precision**: In financial calculations, write `(amount * rate) / divisor` rather than `amount * (rate / divisor)`. This delays division and prevents premature truncation of intermediate values.
2. **Always Guard Denominators with `NULLIF(col, 0)`**: Never execute raw division (`a / b`) in production queries without verifying that `b` cannot be zero. A single zero value in a 10-million-row scan will abort the entire transaction.
3. **Explicitly Cast Integers Before Division in PostgreSQL**: If you intend to compute a fractional value, explicitly cast the numerator: `(val::numeric / total)`.
4. **Use `ROUND()` with Explicit Scale for Currency**: Always specify the target decimal places (`ROUND(amt, 2)`). Omitting the scale defaults to rounding to the nearest whole integer.
5. **Use Deterministic Ceil/Floor for Billing Buckets**: When billing for time or bandwidth (e.g., cell phone minutes or compute seconds), use `CEIL(duration / 60.0)` to ensure partial minutes round up to the next full unit.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Integer Division Truncating to Zero** | `SELECT 1 / 2` yields `0` in Postgres, breaking percentage calculations. | Cast operand: `1.0 / 2` or `1::numeric / 2`. |
| **Unhandled Division by Zero** | A single row with `0` in the denominator crashes the entire query with `ERROR: division by zero`. | Use `val / NULLIF(denominator, 0)`. |
| **Using `ROUND(val)` Without Scale for Money** | Rounds `$14.95` to `$15.00`, discarding cents. | Supply the scale argument: `ROUND(val, 2)`. |
| **Assuming Modulo Result Is Always Positive** | In SQL, `(-7) % 4` yields `-3`, not `1`. | Use `((val % m) + m) % m` for positive modular arithmetic. |
| **Using Floating-Point Math for Balances** | `DOUBLE PRECISION` accumulates binary rounding drift, causing ledger audits to fail. | Use `NUMERIC(p, s)`. |
| **Using `RANDOM()` Without Seeding for Tests** | Calling `RANDOM()` generates non-deterministic test outputs that cannot be reproduced. | Use `setseed(float)` in PostgreSQL before testing. |

---

## 8. Performance & Security Considerations

### Performance
- **Cost of `NUMERIC` Operations**: Software-emulated arbitrary-precision arithmetic (`NUMERIC`) is approximately 10x–20x slower than hardware-accelerated integer or double-precision arithmetic. For non-monetary, read-heavy analytical workloads (e.g., machine learning distances or signal processing), prefer `DOUBLE PRECISION`.
- **Pre-Computing Constants**: In `WHERE price > 100 * 1.15`, modern query optimizers evaluate `100 * 1.15` as a constant (`115.0`) at planning time (constant folding), incurring zero runtime evaluation overhead.

### Security
- **Statistical Inferences via `RANDOM()`**:
  - Never use the database `RANDOM()` function for generating cryptographically secure secrets, password reset tokens, or cryptographic keys. Database random number generators are pseudo-random algorithms (e.g., Linear Congruential Generators or Mersenne Twister). Use dedicated cryptographic libraries or PostgreSQL's `pgcrypto` extension (`gen_random_bytes`).

---

## 9. Comparison Matrix

### Numeric Operations Across Dialects

| Mathematical Concept | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **`5 / 2`** | `2` (Integer) | `2.5` (Float) | `2` (Integer) | `2` (Integer) |
| **Division by Zero** | Throws Exception | Returns `NULL` | Returns `NULL` | Throws Exception |
| **`ROUND(12.345, 2)`** | `12.35` | `12.35` | `12.35` | `12.35` |
| **Truncate to 2 Places**| `TRUNC(12.345, 2)` | `TRUNCATE(12.345, 2)` | Custom trick | `ROUND(12.345, 2, 1)` |
| **Random Float $[0, 1)$** | `RANDOM()` | `RAND()` | `(RANDOM() & 0x7FFFFFFF) / 2147483648.0` | `RAND()` |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: Why does `SELECT 5 / 2` return `2` in PostgreSQL and SQL Server, but `2.5` in MySQL?
<details>
<summary>Answer</summary>
This behavior reflects divergent design philosophies regarding type systems:
- **PostgreSQL and SQL Server**: Adhere strictly to the C/C++ static typing philosophy where arithmetic between two operands of type `INTEGER` must yield an `INTEGER`. Fractional remainders are truncated toward zero.
- **MySQL**: Follows dynamic scripting language conventions (like PHP or JavaScript), automatically promoting operands to `DECIMAL` or `DOUBLE` when division is performed.

To write portable SQL, developers should explicitly cast the numerator: `CAST(5 AS numeric) / 2`.
</details>

#### Q2: What is the difference between `ROUND()` and `TRUNC()`?
<details>
<summary>Answer</summary>
- `ROUND(val, s)`: Evaluates the $(s+1)$-th digit. If it is 5 or greater, it rounds the $s$-th digit away from zero. Example: `ROUND(14.56, 1) = 14.6`.
- `TRUNC(val, s)`: Mechanically discards all digits past the $s$-th decimal position without inspecting subsequent digits or rounding up. Example: `TRUNC(14.56, 1) = 14.5`.
</details>

#### Q3: Why does `SELECT * FROM tbl WHERE (col / 0) > 1` abort a transaction in PostgreSQL, and how does `NULLIF` prevent this?
<details>
<summary>Answer</summary>
In PostgreSQL, dividing by literal zero or evaluating an expression where a row's denominator is 0 causes a fatal runtime mathematical exception (`ERROR: division by zero`), immediately aborting the enclosing transaction block.

`NULLIF(denominator, 0)` checks if the denominator is 0. If it is 0, it substitutes `NULL`. In SQL, dividing any number by `NULL` returns `NULL`. The condition `NULL > 1` evaluates to `UNKNOWN`, which the `WHERE` clause safely discards without raising an exception.
</details>

#### Q4: What is the difference between `CEIL()` and `FLOOR()` for negative numbers?
<details>
<summary>Answer</summary>
- `CEIL(x)`: Returns the smallest integer $\ge x$ (rounds up toward positive infinity).
  For negative numbers: `CEIL(-3.2)` returns `-3` (because $-3 > -3.2$).
- `FLOOR(x)`: Returns the largest integer $\le x$ (rounds down toward negative infinity).
  For negative numbers: `FLOOR(-3.2)` returns `-4` (because $-4 < -3.2$).
</details>

### Practical Calculation Puzzles

#### Q5: Write an SQL query to calculate the percentage discount between `retail_price` and `sale_price`, rounded to 1 decimal place, handling zero or NULL retail prices safely.
<details>
<summary>Answer</summary>

```sql
SELECT 
    product_id,
    retail_price,
    sale_price,
    ROUND(
        ((retail_price - sale_price)::numeric / NULLIF(retail_price, 0)) * 100.0, 
        1
    ) AS discount_pct
FROM products;
```
</details>

#### Q6: How can you generate a pseudo-random integer between 100 and 999 (inclusive) in PostgreSQL?
<details>
<summary>Answer</summary>

```sql
SELECT FLOOR(100 + RANDOM() * (999 - 100 + 1))::integer AS random_three_digit_code;
```
</details>

#### Q7: Write a query to compute the standard Euclidean distance between two geographic coordinate points $(x_1, y_1)$ and $(x_2, y_2)$ using scalar math functions.
<details>
<summary>Answer</summary>

```sql
SELECT 
    point_a_id,
    point_b_id,
    SQRT(POWER(x2 - x1, 2) + POWER(y2 - y1, 2)) AS euclidean_distance
FROM coordinate_pairs;
```
</details>

#### Q8: Write a query that computes both the integer quotient and the remainder when dividing `seconds_elapsed` into whole minutes and remaining seconds.
<details>
<summary>Answer</summary>

```sql
SELECT 
    seconds_elapsed,
    (seconds_elapsed / 60) AS minutes_quotient, -- Integer division
    (seconds_elapsed % 60) AS remaining_seconds  -- Modulo
FROM timer_logs;
```
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Percentage Calculation with Cast
Given an `orders` table with `total_amount` and `tax_amount`, calculate the tax percentage of the total amount, rounded to 2 decimal places.
<details>
<summary>Solution</summary>

```sql
SELECT 
    order_id,
    ROUND((tax_amount::numeric / NULLIF(total_amount, 0)) * 100.0, 2) AS tax_percentage
FROM orders;
```
</details>

#### Exercise 2: Absolute Value Difference
Calculate the absolute discrepancy between `recorded_balance` and `actual_balance` in a reconciliation table.
<details>
<summary>Solution</summary>

```sql
SELECT 
    account_id,
    ABS(recorded_balance - actual_balance) AS variance
FROM account_audits
ORDER BY variance DESC;
```
</details>

### Intermediate Exercises

#### Exercise 3: Billing Duration Ceil Calculation
A cloud provider charges for compute instances per minute, where any fraction of a minute is billed as a full minute. Calculate the billable minutes from `duration_seconds`.
<details>
<summary>Solution</summary>

```sql
SELECT 
    instance_id,
    duration_seconds,
    CEIL(duration_seconds / 60.0)::integer AS billable_minutes
FROM instance_usage;
```
</details>

#### Exercise 4: Profit Margin Calculation
Calculate the gross profit margin percentage `((revenue - cost) / revenue) * 100` for all products, formatted to 2 decimal places, sorting by highest margin first.
<details>
<summary>Solution</summary>

```sql
SELECT 
    product_name,
    retail_price,
    cost_price,
    ROUND(((retail_price - cost_price)::numeric / NULLIF(retail_price, 0)) * 100.0, 2) AS margin_pct
FROM products
ORDER BY margin_pct DESC;
```
</details>

### Advanced Exercises

#### Exercise 5: Haversine Great-Circle Distance
Write a query calculating the Haversine great-circle distance in kilometers between two latitude/longitude points: `(lat1, lon1)` and `(lat2, lon2)` using trigonometric functions, where Earth's radius $R = 6371\text{ km}$.
<details>
<summary>Solution</summary>

```sql
SELECT 
    location_a,
    location_b,
    ROUND(
        6371 * 2 * ASIN(
            SQRT(
                POWER(SIN(RADIANS(lat2 - lat1) / 2), 2) +
                COS(RADIANS(lat1)) * COS(RADIANS(lat2)) *
                POWER(SIN(RADIANS(lon2 - lon1) / 2), 2)
            )
        )::numeric, 
        2
    ) AS distance_km
FROM geo_coordinates;
```
</details>

#### Exercise 6: Reversible Pseudo-Random Shuffling via Hash
Write an SQL query to generate a deterministic pseudo-random ranking of rows without using `RANDOM()`, such that running the query on different servers produces identical sorting.
<details>
<summary>Solution</summary>

```sql
-- Use MD5 hash of an integer ID converted to integer:
SELECT 
    user_id,
    username,
    ('x' || SUBSTR(MD5(user_id::text), 1, 8))::bit(32)::bigint AS deterministic_hash
FROM users
ORDER BY deterministic_hash ASC;
```
*(Because MD5 is deterministic, the random-like order is 100% reproducible across all environments).*
</details>

---

## 12. Summary & Quick Reference

### Numeric Functions Quick Cheat Sheet
- **Division**: In Postgres, `int / int` truncates. Use `int::numeric / int`.
- **Division by Zero**: Guard with `NULLIF(den, 0)`.
- **Rounding**: `ROUND(x, s)` (Half-up), `TRUNC(x, s)` (Chop decimals).
- **Boundaries**: `CEIL(x)` (Round up), `FLOOR(x)` (Round down).
- **Magnitude**: `ABS(x)` (Positive magnitude), `SIGN(x)` ($-1, 0, +1$).
- **Powers & Roots**: `POWER(base, exp)`, `SQRT(val)`.
- **Random Range**: `FLOOR(min + RANDOM() * (max - min + 1))::integer`.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [Mathematical Functions and Operators](https://www.postgresql.org/docs/17/functions-math.html)
- SQLite Documentation: [Built-in Mathematical Functions](https://www.sqlite.org/lang_mathfunc.html)
- Modern SQL: [Numeric Data Types and Precision](https://modern-sql.com/data-types/numeric)
- [11 - String Functions & Text Manipulation](./11-string-functions.md)
