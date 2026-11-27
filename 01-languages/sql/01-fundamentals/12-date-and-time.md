# 12: Date, Time & Timezone Handling

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [11 - String Functions & Text Manipulation](./11-string-functions.md) |
| **Target Audience** | Software Engineers, Data Engineers, Systems Architects |
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

Temporal data is at the core of nearly every modern relational database system. Whether recording user logins, financial transaction settlements, audit trails, subscription renewals, or analytical cohort retention, operations require precise recording and calculation of time.

Yet, temporal processing is notoriously full of edge cases:
- Daylight Saving Time (DST) shifts cause local wall-clock hours to repeat or vanish completely.
- Leap years, leap seconds, and variable month lengths (28, 29, 30, or 31 days) make naive date math (`+ 30 days`) inaccurate.
- Timezone ambiguity between server location, client location, and UTC creates phantom data discrepancies.
- In PostgreSQL, `NOW()` returns the timestamp at the **start of the transaction**, remaining frozen across all statements within that transaction, whereas `clock_timestamp()` advances in real time.

Understanding how relational database engines store, normalize, convert, and index temporal types is vital for building reliable global systems.

---

## 2. Core Concepts

### 1. The Temporal Data Type Family
- **`DATE`**: Calendar day only (`YYYY-MM-DD`). 4 bytes. Range: 4713 BC to 5874897 AD.
- **`TIME [WITHOUT TIME ZONE]`**: Time of day only (`HH:MI:SS.US`). 8 bytes. No calendar date context.
- **`TIMESTAMP [WITHOUT TIME ZONE]`**: Year, month, day, hour, minute, second, microsecond. Has no timezone offset.
- **`TIMESTAMP WITH TIME ZONE` (`TIMESTAMPTZ`)**: **The production standard**. Input strings are normalized to UTC for physical storage. Output strings are rendered in the client’s requested timezone.
- **`INTERVAL`**: A duration or span of time (e.g., `'1 year 2 months 3 days 4 hours'`).

### 2. Transaction Time (`NOW()`) vs Real Clock Time (`clock_timestamp()`)
In PostgreSQL, temporal functions have precise execution semantics:
- **`CURRENT_TIMESTAMP` / `NOW()` / `transaction_timestamp()`**: Returns the timestamp at the **beginning of the current transaction**. It does NOT change between statements within the same transaction!
- **`statement_timestamp()`**: Returns the timestamp at the beginning of the currently executing SQL statement.
- **`clock_timestamp()`**: Returns the actual current wall-clock time at the microsecond of function execution.

```
BEGIN; -- Time: 10:00:00.000
  SELECT NOW(); -- Returns 10:00:00.000
  SELECT pg_sleep(5);
  SELECT NOW(); -- STILL Returns 10:00:00.000! (Guarantees transaction-level consistency)
  SELECT clock_timestamp(); -- Returns 10:00:05.012! (Actual wall-clock time)
COMMIT;
```

### 3. Truncating & Extracting Time
- **`DATE_TRUNC('unit', timestamp)`**: Truncates a timestamp to the specified precision (e.g., `'day'`, `'month'`, `'year'`, `'hour'`). Critical for grouping time-series analytics.
- **`EXTRACT(field FROM timestamp)`**: Extracts a specific numeric sub-component (e.g., `EXTRACT(YEAR FROM dt)`, `EXTRACT(DOW FROM dt)` for day of week).

---

## 3. How It Works Internally

### Physical Storage and UTC Normalization

```
Client Input: '2024-06-01 14:30:00-04' (New York EDT, UTC-4)
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ Database Engine Parsing & Normalization                     │
│ 14:30:00 - (-04:00) = 18:30:00 UTC                          │
│ Stored on 8KB Disk Page as:                                 │
│ 64-bit integer representing microseconds since 2000-01-01   │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
Client Session: SET timezone = 'Asia/Tokyo' (UTC+9)
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ Presentation Output:                                        │
│ 18:30:00 UTC + 09:00 = '2024-06-02 03:30:00+09'             │
└─────────────────────────────────────────────────────────────┘
```

- In PostgreSQL, `TIMESTAMPTZ` takes exactly 8 bytes on disk. It does NOT store a string or a timezone name. It stores an integer counting microseconds since the Julian epoch (`2000-01-01 00:00:00 UTC`).
- The IANA Time Zone Database (Olson database) is embedded in the engine, providing historical DST transition tables for every geographic region on Earth.

---

## 4. Syntax & Usage

### Core Temporal Operations Syntax

```sql
SELECT 
    -- Current Time Primitives
    CURRENT_DATE AS today_date,
    CURRENT_TIME AS local_time,
    CURRENT_TIMESTAMP AS now_with_tz,

    -- Interval Arithmetic
    CURRENT_TIMESTAMP + INTERVAL '30 days' AS renewal_deadline,
    CURRENT_TIMESTAMP - INTERVAL '1 year 2 months' AS past_event,

    -- Truncation (Grouping bucket)
    DATE_TRUNC('month', CURRENT_TIMESTAMP) AS start_of_current_month,
    DATE_TRUNC('hour', CURRENT_TIMESTAMP) AS start_of_hour,

    -- Field Extraction
    EXTRACT(YEAR FROM CURRENT_TIMESTAMP) AS current_year,
    EXTRACT(DOW FROM CURRENT_TIMESTAMP) AS day_of_week_0_to_6,
    EXTRACT(EPOCH FROM CURRENT_TIMESTAMP) AS unix_epoch_seconds,

    -- Formatting to String & Parsing
    TO_CHAR(CURRENT_TIMESTAMP, 'YYYY-MM-DD HH24:MI:SS TZH:TZM') AS formatted_string,
    TO_TIMESTAMP('2024-04-01 15:30:00', 'YYYY-MM-DD HH24:MI:SS') AS parsed_timestamp;
```

### Multi-Dialect Comparison Matrix

| Temporal Operation | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Current Timestamp** | `CURRENT_TIMESTAMP` or `NOW()`| `CURRENT_TIMESTAMP` or `NOW()`| `CURRENT_TIMESTAMP` or `datetime('now')`| `SYSUTCDATETIME()` or `GETDATE()` |
| **Add 7 Days** | `dt + INTERVAL '7 days'` | `DATE_ADD(dt, INTERVAL 7 DAY)`| `datetime(dt, '+7 days')`| `DATEADD(day, 7, dt)` |
| **Date Truncation** | `DATE_TRUNC('month', dt)` | Not native (`DATE_FORMAT` trick)| `strftime('%Y-%m-01', dt)`| `DATETRUNC(month, dt)` (since 2022)|
| **Epoch Extraction** | `EXTRACT(EPOCH FROM dt)` | `UNIX_TIMESTAMP(dt)` | `unixepoch(dt)` | `DATEDIFF_BIG(second, '1970-01-01', dt)`|
| **Convert Timezone** | `dt AT TIME ZONE 'UTC'` | `CONVERT_TZ(dt, from, to)`| Not built-in | `dt AT TIME ZONE 'UTC'` |

---

## 5. 5 Practical Progressive Examples

### Example 1: Cohort Grouping with `DATE_TRUNC`
Aggregating monthly sales revenue across multi-year transactional datasets:

```sql
SELECT 
    DATE_TRUNC('month', order_date) AS order_month,
    COUNT(order_id) AS total_orders,
    SUM(total_amount) AS monthly_revenue
FROM orders
WHERE order_status = 'completed'
GROUP BY DATE_TRUNC('month', order_date)
ORDER BY order_month ASC;
```
```text
+------------------------+--------------+-----------------+
| order_month            | total_orders | monthly_revenue |
+------------------------+--------------+-----------------+
| 2024-04-01 00:00:00+00 |            3 |         2887.00 |
+------------------------+--------------+-----------------+
```
*(Notice: `DATE_TRUNC` normalizes every order in April to `2024-04-01 00:00:00`, allowing straightforward grouping without string conversion).*

### Example 2: Converting Between Timezones (`AT TIME ZONE`)
Translating a universal timestamp into local operational times:

```sql
SELECT 
    order_id,
    order_date AS utc_timestamp,
    order_date AT TIME ZONE 'America/New_York' AS us_eastern_time,
    order_date AT TIME ZONE 'Asia/Kolkata' AS india_standard_time
FROM orders
LIMIT 1;
```
```text
+----------+------------------------+---------------------+---------------------+
| order_id | utc_timestamp          | us_eastern_time     | india_standard_time |
+----------+------------------------+---------------------+---------------------+
|     1001 | 2024-04-01 10:15:00+00 | 2024-04-01 06:15:00 | 2024-04-01 15:45:00 |
+----------+------------------------+---------------------+---------------------+
```

### Example 3: Edge Case (Calculating Exact Age in Years/Months/Days)
Why naive subtraction (`(dt2 - dt1) / 365`) fails on leap years:

In PostgreSQL:
```sql
SELECT 
    first_name,
    birth_date,
    AGE(CURRENT_DATE, birth_date) AS exact_age_interval,
    EXTRACT(YEAR FROM AGE(CURRENT_DATE, birth_date))::integer AS age_years
FROM employees;
```
```text
+------------+------------+-------------------------+-----------+
| first_name | birth_date | exact_age_interval      | age_years |
+------------+------------+-------------------------+-----------+
| Elena      | 1990-05-15 | 36 years 6 mons 11 days |        36 |
+------------+------------+-------------------------+-----------+
```
*(The `AGE()` function accounts for variable month lengths and leap years).*

### Example 4: Failure Mode (Sargability Loss via `DATE_TRUNC` in WHERE)
Why using `DATE_TRUNC` in a filter prevents index usage:

```sql
-- ANTI-PATTERN: Wrapping order_date in DATE_TRUNC forces a full table scan!
EXPLAIN SELECT * FROM orders WHERE DATE_TRUNC('month', order_date) = '2024-04-01';
```
```text
                       QUERY PLAN                       
--------------------------------------------------------
 Seq Scan on orders  (cost=0.00..25.00 rows=1 width=64)
   Filter: (date_trunc('month'::text, order_date) = '2024-04-01'::timestamp)
```
**Optimized Sargable Equivalent**:
```sql
EXPLAIN SELECT * FROM orders 
WHERE order_date >= '2024-04-01 00:00:00' 
  AND order_date <  '2024-05-01 00:00:00';
```
```text
                              QUERY PLAN                              
----------------------------------------------------------------------
 Index Scan using idx_orders_order_date on orders (cost=0.15..8.17 rows=3 width=64)
```

### Example 5: Optimized Time Interval Calculations (Subscribers Due for Renewal)
Finding recurring subscription accounts due for renewal within the next 48 hours:

```sql
SELECT 
    subscription_id, 
    customer_id, 
    renewal_date
FROM subscriptions
WHERE renewal_date >= CURRENT_TIMESTAMP
  AND renewal_date <  CURRENT_TIMESTAMP + INTERVAL '48 hours'
  AND status = 'active';
```

---

## 6. Best Practices

1. **Always Use `TIMESTAMPTZ` for Points in Time**: Store all events with timezone awareness (`TIMESTAMP WITH TIME ZONE`). Store universal UTC on disk, convert to local user timezone at presentation time.
2. **Use Plain `DATE` for Calendar Days**: For birth dates, national holidays, or corporate accounting days, use `DATE`. Attaching a time or timezone to a birth date can cause it to shift into yesterday or tomorrow when viewed in different timezones!
3. **Use Half-Open Intervals for Date Queries**: Always query ranges with `>= start_date AND < next_month_date`. Never use `BETWEEN` on timestamps.
4. **Use `EXTRACT(EPOCH FROM ...)` for Unix Timestamps**: When integrating with external systems expecting Unix epoch seconds, use `EXTRACT(EPOCH FROM timestamp)`.
5. **Remember `NOW()` Is Frozen Within a Transaction**: If you need real elapsed wall-clock time inside a long-running batch transaction, call `clock_timestamp()`, not `NOW()`.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Using `TIMESTAMP WITHOUT TIME ZONE`** | Drops timezone offsets, causing ambiguous times when Daylight Saving shifts. | Use `TIMESTAMP WITH TIME ZONE` (`TIMESTAMPTZ`). |
| **Using `BETWEEN` on Timestamp Ranges** | Omits events occurring after `00:00:00` on the final date. | Use `>= start_date AND < end_date`. |
| **Using `DATE_TRUNC` in `WHERE` Clauses** | Prevents the query optimizer from using B-Tree indexes on the timestamp column. | Use sargable boundary ranges (`>= '2024-01-01' AND < '2024-02-01'`). |
| **Assuming 1 Month is Always 30 Days** | `dt + INTERVAL '30 days'` drifts across February and 31-day months. | Use `dt + INTERVAL '1 month'`. |
| **Assuming `NOW()` Advances During Transactions** | `NOW()` remains frozen at the transaction start time; polling in a loop sees no change. | Use `clock_timestamp()` for real elapsed time. |
| **Storing Birthdays as `TIMESTAMPTZ`** | Viewing a midnight birthday in a western timezone shifts the birth date to the day before! | Store birthdays strictly as `DATE`. |

---

## 8. Performance & Security Considerations

### Performance
- **BRIN Indexes for Append-Only Time Series**:
  - Tables with millions of append-only chronological records (event logs, metrics) naturally sort by physical disk block.
  - A **BRIN (Block Range Index)** on `created_at` consumes 99% less disk space and memory than a B-Tree while providing comparable range scan speeds.
- **Partitioning by Range**:
  - For massive tables ($> 50$ million rows), partition by `order_date` range (e.g., monthly partitions) to enable partition pruning and instant archival drops.

### Security
- **Time-Based Blind SQL Injection**:
  - Attackers test injection vulnerabilities using artificial sleep functions:
    `SELECT * FROM users WHERE id = 1 AND pg_sleep(5);`
  - Restrict query execution timeouts (`statement_timeout = '15s'`) to prevent attackers from locking up server threads.

---

## 9. Comparison Matrix

### Temporal Types & Precision

| Type | Storage Size | Resolution | Timezone Aware? | Best Used For |
|:---|:---:|:---:|:---:|:---|
| **`DATE`** | 4 bytes | 1 day | No | Birthdays, legal holidays, invoice dates |
| **`TIME`** | 8 bytes | 1 microsecond | No | Recurring daily alarms, store opening hours |
| **`TIMESTAMP`** | 8 bytes | 1 microsecond | No | Wall-clock devices with no network connection |
| **`TIMESTAMPTZ`** | 8 bytes | 1 microsecond | Yes (Normalized to UTC)| All transactional events, audit logs, user actions |
| **`INTERVAL`** | 16 bytes | 1 microsecond | N/A | Expiration periods, recurring schedule spans |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: What is the difference between `NOW()`, `statement_timestamp()`, and `clock_timestamp()` in PostgreSQL?
<details>
<summary>Answer</summary>
- `NOW()` (or `CURRENT_TIMESTAMP`, `transaction_timestamp()`): Returns the time at which the **current transaction began**. It remains completely constant and frozen across all statements within the same transaction block.
- `statement_timestamp()`: Returns the time at which the **current SQL statement began execution**. It updates with each statement within a multi-statement transaction.
- `clock_timestamp()`: Returns the actual **real-time wall-clock time** at the microsecond the function is invoked, advancing continuously even within a single query.
</details>

#### Q2: Why is `TIMESTAMP WITHOUT TIME ZONE` dangerous in a globally distributed application stack?
<details>
<summary>Answer</summary>
`TIMESTAMP WITHOUT TIME ZONE` stores only the raw digits of year, month, day, hour, minute, and second without any geographical reference or UTC offset. 

If a user in New York creates an order at 5:00 PM local time (stored as `17:00:00`), and a user in London creates an order at 5:00 PM local time (stored as `17:00:00`), the database treats both events as having occurred at the exact same instant—even though the London order occurred 5 hours earlier in real physical time. This breaks chronological sequencing, audit trails, and reconciliation.
</details>

#### Q3: How does PostgreSQL handle Daylight Saving Time (DST) transitions when performing interval math?
<details>
<summary>Answer</summary>
PostgreSQL distinguishes between adding calendar days and adding hours:
- Adding `INTERVAL '1 day'` respects wall-clock calendar time. If a DST transition occurs (e.g., losing or gaining an hour), `timestamp + INTERVAL '1 day'` remains at the same wall-clock hour (e.g., 2:00 PM becomes 2:00 PM the next day, which may be 23 or 25 physical hours later).
- Adding `INTERVAL '24 hours'` adds exactly 24 absolute physical hours of elapsed time, which may result in a different local wall-clock hour (e.g., 2:00 PM becomes 3:00 PM or 1:00 PM across a DST shift).
</details>

#### Q4: What does the `AT TIME ZONE` construct do in PostgreSQL?
<details>
<summary>Answer</summary>
The behavior of `AT TIME ZONE` depends on the input type:
1. When applied to `TIMESTAMPTZ`: It converts the UTC timestamp to the specified timezone and returns a `TIMESTAMP WITHOUT TIME ZONE` reflecting local wall-clock time in that region.
2. When applied to `TIMESTAMP WITHOUT TIME ZONE`: It treats the timestamp as a local wall-clock time in that specified timezone and returns a `TIMESTAMPTZ` normalized to UTC.
</details>

### Practical Temporal Puzzles

#### Q5: Write an SQL query to calculate the number of days, hours, and minutes an issue ticket was open between `created_at` and `closed_at`.
<details>
<summary>Answer</summary>

```sql
SELECT 
    ticket_id,
    closed_at - created_at AS elapsed_interval,
    EXTRACT(DAY FROM closed_at - created_at) AS days_open,
    EXTRACT(HOUR FROM closed_at - created_at) AS hours_open,
    EXTRACT(MINUTE FROM closed_at - created_at) AS minutes_open
FROM support_tickets
WHERE closed_at IS NOT NULL;
```
</details>

#### Q6: Write a query to find all users who registered on a weekend (Saturday or Sunday) in PostgreSQL.
<details>
<summary>Answer</summary>

```sql
SELECT user_id, username, created_at
FROM users
WHERE EXTRACT(DOW FROM created_at) IN (0, 6);
-- In PostgreSQL, DOW (Day Of Week): 0 = Sunday, 6 = Saturday
```
</details>

#### Q7: Write a query to find the last day of the current month in PostgreSQL.
<details>
<summary>Answer</summary>

```sql
SELECT (
    DATE_TRUNC('month', CURRENT_DATE) + INTERVAL '1 month' - INTERVAL '1 day'
)::date AS last_day_of_month;
```
</details>

#### Q8: Write an SQL statement that extracts the Unix epoch timestamp in milliseconds (13-digit integer) from a `TIMESTAMPTZ` column.
<details>
<summary>Answer</summary>

```sql
SELECT 
    event_id,
    FLOOR(EXTRACT(EPOCH FROM logged_at) * 1000)::bigint AS epoch_millis
FROM audit_events;
```
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Finding Today's Orders
Select all orders from `orders` that were placed today (since 00:00:00 UTC), sorted by `order_date` descending.
<details>
<summary>Solution</summary>

```sql
SELECT order_id, customer_id, total_amount, order_date
FROM orders
WHERE order_date >= CURRENT_DATE
ORDER BY order_date DESC;
```
</details>

#### Exercise 2: Adding Subscription Expiry
Given a `subscriptions` table with `start_date DATE` and `duration_months INT`, compute `expiration_date` using interval addition.
<details>
<summary>Solution</summary>

```sql
SELECT 
    subscription_id,
    start_date,
    (start_date + (duration_months || ' months')::interval)::date AS expiration_date
FROM subscriptions;
```
</details>

### Intermediate Exercises

#### Exercise 3: Weekly Cohort Aggregation
Group orders by calendar week (Monday through Sunday) using `DATE_TRUNC('week', order_date)`, displaying the week start date, total order count, and revenue.
<details>
<summary>Solution</summary>

```sql
SELECT 
    DATE_TRUNC('week', order_date)::date AS week_start,
    COUNT(order_id) AS orders_count,
    SUM(total_amount) AS revenue
FROM orders
WHERE order_status = 'completed'
GROUP BY DATE_TRUNC('week', order_date)
ORDER BY week_start ASC;
```
</details>

#### Exercise 4: Business Hours Filter
Filter an audit log table to return events that occurred strictly between 09:00:00 and 17:00:00 New York local time.
<details>
<summary>Solution</summary>

```sql
SELECT log_id, logged_at
FROM audit_logs
WHERE (logged_at AT TIME ZONE 'America/New_York')::time >= '09:00:00'
  AND (logged_at AT TIME ZONE 'America/New_York')::time <  '17:00:00';
```
</details>

### Advanced Exercises

#### Exercise 5: Generating Synthetic Date Ranges
Generate a continuous sequence of all calendar dates in the year 2024 using PostgreSQL's `generate_series()` function, left-joining to `orders` to find dates with zero sales.
<details>
<summary>Solution</summary>

```sql
WITH AllDays2024 AS (
    SELECT generate_series(
        '2024-01-01'::date,
        '2024-12-31'::date,
        '1 day'::interval
    )::date AS calendar_date
)
SELECT 
    d.calendar_date,
    COUNT(o.order_id) AS orders_placed,
    COALESCE(SUM(o.total_amount), 0.00) AS revenue
FROM AllDays2024 d
LEFT JOIN orders o 
  ON d.calendar_date = o.order_date::date 
 AND o.order_status = 'completed'
GROUP BY d.calendar_date
ORDER BY d.calendar_date ASC;
```
</details>

#### Exercise 6: Calculating Median Time-to-Resolution
Calculate the median duration (in hours) between ticket creation and ticket resolution using `PERCENTILE_CONT(0.5)`.
<details>
<summary>Solution</summary>

```sql
SELECT 
    PERCENTILE_CONT(0.5) WITHIN GROUP (
        ORDER BY EXTRACT(EPOCH FROM (closed_at - created_at)) / 3600.0
    ) AS median_resolution_hours
FROM support_tickets
WHERE closed_at IS NOT NULL;
```
</details>

---

## 12. Summary & Quick Reference

### Temporal Functions Cheatsheet
- **Types**: `DATE` (Days), `TIMESTAMPTZ` (UTC points in time), `INTERVAL` (Spans).
- **Current Time**: `NOW()` / `CURRENT_TIMESTAMP` (Transaction start), `clock_timestamp()` (Real clock).
- **Interval Arithmetic**: `dt + INTERVAL '14 days'`, `dt - INTERVAL '2 hours'`.
- **Truncation**: `DATE_TRUNC('month', dt)`, `DATE_TRUNC('day', dt)`.
- **Extraction**: `EXTRACT(YEAR FROM dt)`, `EXTRACT(EPOCH FROM dt)`.
- **Timezone**: `dt AT TIME ZONE 'UTC'`.
- **Query Rule**: Query ranges using `>= start AND < end` (never `BETWEEN`).

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [Date/Time Functions and Operators](https://www.postgresql.org/docs/17/functions-datetime.html)
- PostgreSQL Documentation: [Date/Time Types](https://www.postgresql.org/docs/17/datatype-datetime.html)
- IANA Time Zone Database: [Time Zone Database Overview](https://www.iana.org/time-zones)
- [13 - Conditional Expressions: CASE, COALESCE & NULLIF](./13-case-and-conditionals.md)
