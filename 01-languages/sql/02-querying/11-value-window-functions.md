# Value Window Functions: LAG, LEAD, FIRST_VALUE, LAST_VALUE, and NTH_VALUE

| Metadata | Details |
| :--- | :--- |
| **Topic** | SQL Value Window Functions (LAG, LEAD, FIRST_VALUE, LAST_VALUE, NTH_VALUE, IGNORE NULLS) |
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

Time-series analytics, financial modeling, and audit workflows require comparing the current row against preceding or succeeding events:
- Calculating day-over-day revenue percentage growth.
- Detecting user session boundaries by measuring inactivity gaps between successive clicks.
- Identifying when a server CPU transitioned above a warning threshold.
- Extracting the initial onboarding balance (`FIRST_VALUE`) or the final closing price (`LAST_VALUE`) of a trading day.
- Forward-filling missing telemetry values from the most recent non-null sensor reading.

Before window functions, developers accomplished this via expensive quadratic self-joins `JOIN table t2 ON t2.id = t1.id - 1`, which collapsed under scale and failed when sequence keys contained gaps.

Value window functions—`LAG`, `LEAD`, `FIRST_VALUE`, `LAST_VALUE`, and `NTH_VALUE`—enable relational engines to inspect preceding, succeeding, or boundary rows in constant $O(1)$ memory access per tuple. However, they also introduce subtle traps, notably the infamous `LAST_VALUE` default framing trap and dialect variations in `IGNORE NULLS` support.

---

## Core Concepts

### 1. `LAG()` and `LEAD()`
- **`LAG(column, [offset], [default])`**: Looks backward by `offset` rows (default 1) within the partition. If the offset reaches before the start of the partition, returns `default` (or `NULL` if omitted).
- **`LEAD(column, [offset], [default])`**: Looks forward by `offset` rows (default 1) within the partition. If the offset extends past the end of the partition, returns `default` (or `NULL`).
- **Framing Note**: `LAG` and `LEAD` **forbid** explicit `ROWS` or `RANGE` framing clauses; they navigate relative row offsets directly.

### 2. `FIRST_VALUE()` and `LAST_VALUE()`
- **`FIRST_VALUE(column)`**: Returns the value of `column` from the very first row of the window frame.
- **`LAST_VALUE(column)`**: Returns the value of `column` from the **last row of the window frame**.
- **The Infamous `LAST_VALUE` Trap**: Because adding `ORDER BY` defaults the frame to `RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW`, the "last row of the frame" is the **current row**! Hence, `LAST_VALUE(col) OVER (ORDER BY date)` unexpectedly returns the current row's value. You must explicitly specify `ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING` to get the true last value of the partition!

### 3. `NTH_VALUE(column, n)`
- Returns the value of `column` at the $n^{\text{th}}$ row (1-based index) of the window frame.
- If the frame has fewer than $n$ rows, returns `NULL`.
- Like `LAST_VALUE`, if the frame is not extended to `UNBOUNDED FOLLOWING`, `NTH_VALUE(col, 5)` returns `NULL` for rows 1 through 4.

### 4. `RESPECT NULLS` vs `IGNORE NULLS` (ANSI SQL:2011)
- **`RESPECT NULLS`** (Default): Evaluates the offset strictly by row position, even if the value at that row is `NULL`.
- **`IGNORE NULLS`**: Skips over `NULL` values when seeking the preceding or succeeding non-null value (essential for forward-filling sparse time-series data).

---

## How It Works Internally

```
                   [ WindowAgg Execution Engine ]
                                  │
    Sorted input stream: [ t1(10:00) ──► t2(10:05) ──► t3(10:15) ──► t4(10:30) ]
                                  │
                 ┌────────────────┴────────────────┐
                 ▼                                 ▼
         [ LAG Evaluation ]                [ LEAD Evaluation ]
      Keeps prior tuple buffer.         Requires lookahead buffer.
      Pointer to (t_curr - offset).     Buffers incoming tuples until
      Memory footprint: O(offset).      lookahead offset is satisfied.
                                                   │
                 ┌─────────────────────────────────┘
                 ▼
     [ Window Framing Buffer: FIRST_VALUE & LAST_VALUE ]
     If frame has UNBOUNDED FOLLOWING:
     Engine must buffer or spool the entire partition before emitting
     the first tuple, increasing memory consumption!
```

1. **`LAG` Mechanics**: The engine simply maintains a small ring buffer of size `offset` containing previous tuples. As the cursor advances, prior rows are readily available in memory ($O(1)$ CPU, tiny buffer).
2. **`LEAD` Mechanics**: The engine requires a lookahead buffer. It reads `offset` rows ahead from the input stream before emitting the current tuple.
3. **Partition Spooling on `LAST_VALUE`**: When `UNBOUNDED FOLLOWING` is declared, the engine cannot know the partition's final value until it reads all rows in that partition. For large partitions, this forces the database to buffer the entire partition in memory or spill to a temporary disk spool.

---

## Syntax & Usage

```sql
SELECT 
    trade_date,
    ticker,
    close_price,
    -- Prior trading day's price
    LAG(close_price, 1) OVER w AS prev_day_price,
    -- Next trading day's price
    LEAD(close_price, 1) OVER w AS next_day_price,
    -- Opening price of the partition (first row)
    FIRST_VALUE(close_price) OVER w AS opening_price,
    -- Closing price of the partition (MUST expand frame to UNBOUNDED FOLLOWING)
    LAST_VALUE(close_price) OVER (
        PARTITION BY ticker 
        ORDER BY trade_date 
        ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
    ) AS final_closing_price
FROM stock_quotes
WINDOW w AS (PARTITION BY ticker ORDER BY trade_date);
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **`LAG()` & `LEAD()`** | Full ANSI support | Supported (MySQL 8.0+) | Supported (SQLite 3.25+) | Full ANSI support |
| **`FIRST_VALUE()` & `LAST_VALUE()`** | Full ANSI support | Supported (MySQL 8.0+) | Supported (SQLite 3.25+) | Full ANSI support |
| **`NTH_VALUE()`** | Supported | Supported | **Not Supported** | **Not Supported** |
| **`IGNORE NULLS` Clause** | Supported in PostgreSQL 17+ | **Not Supported** | **Not Supported** | Supported in SQL Server 2022 |
| **Frame Forbidden on LAG/LEAD** | Enforced (Syntax error if frame used) | Enforced | Enforced | Enforced |

---

## Practical Examples

### Example 1: Basic — Day-over-Day Revenue Growth
Calculate the revenue change and percentage growth between consecutive trading days.

```sql
CREATE TABLE daily_revenue (
    record_date DATE PRIMARY KEY,
    revenue NUMERIC(10,2) NOT NULL
);

INSERT INTO daily_revenue VALUES
('2026-01-01', 1000.00),
('2026-01-02', 1250.00),
('2026-01-03', 1100.00),
('2026-01-04', 1650.00);

SELECT 
    record_date,
    revenue,
    LAG(revenue, 1) OVER (ORDER BY record_date) AS prev_revenue,
    revenue - LAG(revenue, 1) OVER (ORDER BY record_date) AS net_change,
    ROUND(
        ((revenue - LAG(revenue, 1) OVER (ORDER BY record_date)) * 100.0) / 
        LAG(revenue, 1) OVER (ORDER BY record_date), 
        2
    ) AS pct_growth
FROM daily_revenue;
```

```
Result:
 record_date | revenue | prev_revenue | net_change | pct_growth 
-------------+---------+--------------+------------+------------
 2026-01-01  | 1000.00 |         NULL |       NULL |       NULL
 2026-01-02  | 1250.00 |      1000.00 |     250.00 |      25.00
 2026-01-03  | 1100.00 |      1250.00 |    -150.00 |     -12.00
 2026-01-04  | 1650.00 |      1100.00 |     550.00 |      50.00
(4 rows)
```

---

### Example 2: Real-World — Sessionization (Inactivity Gap Detection)
Identify distinct user browsing sessions by flagging when more than 30 minutes elapsed since the user's previous action.

```sql
CREATE TABLE clickstream (
    click_id INT PRIMARY KEY,
    user_id INT NOT NULL,
    click_time TIMESTAMP NOT NULL
);

INSERT INTO clickstream VALUES
(1, 101, '2026-01-01 10:00:00'),
(2, 101, '2026-01-01 10:15:00'), -- 15 mins (same session)
(3, 101, '2026-01-01 11:00:00'), -- 45 mins (>30 mins: NEW SESSION)
(4, 101, '2026-01-01 11:05:00'); -- 5 mins (same session)

WITH session_flags AS (
    SELECT 
        click_id,
        user_id,
        click_time,
        CASE 
            WHEN click_time - LAG(click_time) OVER (
                PARTITION BY user_id ORDER BY click_time
            ) > INTERVAL '30 minutes' OR LAG(click_time) OVER (
                PARTITION BY user_id ORDER BY click_time
            ) IS NULL 
            THEN 1 ELSE 0 
        END AS is_new_session
    FROM clickstream
)
SELECT 
    click_id,
    user_id,
    click_time,
    is_new_session,
    -- Running sum of session flags creates a unique session identifier!
    SUM(is_new_session) OVER (
        PARTITION BY user_id ORDER BY click_time
    ) AS session_id
FROM session_flags;
```

```
Result:
 click_id | user_id |     click_time      | is_new_session | session_id 
----------+---------+---------------------+----------------+------------
        1 |     101 | 2026-01-01 10:00:00 |              1 |          1
        2 |     101 | 2026-01-01 10:15:00 |              0 |          1
        3 |     101 | 2026-01-01 11:00:00 |              1 |          2
        4 |     101 | 2026-01-01 11:05:00 |              0 |          2
(4 rows)
```

---

### Example 3: Edge Case — Demonstrating the `LAST_VALUE` Framing Trap
Observe how omitting the frame clause causes `LAST_VALUE` to return the current row instead of the true final value of the partition!

```sql
CREATE TABLE audit_log (
    log_id INT PRIMARY KEY,
    audit_date DATE NOT NULL,
    status_msg VARCHAR(20) NOT NULL
);

INSERT INTO audit_log VALUES
(1, '2026-01-01', 'INIT'),
(2, '2026-01-02', 'PROCESSING'),
(3, '2026-01-03', 'COMPLETED');

SELECT 
    audit_date,
    status_msg,
    -- WRONG: Trapped by default RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    LAST_VALUE(status_msg) OVER (ORDER BY audit_date) AS wrong_last_value,
    -- CORRECT: Explicitly expand frame to the end of partition
    LAST_VALUE(status_msg) OVER (
        ORDER BY audit_date 
        ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
    ) AS true_last_value
FROM audit_log;
```

```
Result:
 audit_date | status_msg | wrong_last_value | true_last_value 
------------+------------+------------------+-----------------
 2026-01-01 | INIT       | INIT             | COMPLETED
 2026-01-02 | PROCESSING | PROCESSING       | COMPLETED
 2026-01-03 | COMPLETED  | COMPLETED        | COMPLETED
(3 rows)
```

---

### Example 4: Failure Mode — Adding Frame Clauses to `LAG` / `LEAD`
`LAG` and `LEAD` operate on discrete row offsets and explicitly disallow frame specifications.

```sql
-- FATAL SYNTAX ERROR:
-- ERROR: cannot specify frame for window function lag
SELECT 
    record_date,
    LAG(revenue, 1) OVER (
        ORDER BY record_date 
        ROWS BETWEEN 1 PRECEDING AND CURRENT ROW
    )
FROM daily_revenue;

-- CORRECT SYNTAX:
SELECT 
    record_date,
    LAG(revenue, 1) OVER (ORDER BY record_date)
FROM daily_revenue;
```

---

### Example 5: Optimized — Forward-Filling Sparse Telemetry via `IGNORE NULLS`
Fill missing sensor readings with the most recent observed non-null reading.

```sql
CREATE TABLE iot_telemetry (
    read_time TIMESTAMP PRIMARY KEY,
    reading NUMERIC(5,2) NULL
);

INSERT INTO iot_telemetry VALUES
('2026-01-01 00:00:00', 21.5),
('2026-01-01 00:01:00', NULL), -- Missed packet
('2026-01-01 00:02:00', NULL), -- Missed packet
('2026-01-01 00:03:00', 23.0);

-- PostgreSQL 17+ / SQL Server 2022 IGNORE NULLS Syntax:
SELECT 
    read_time,
    reading,
    LAST_VALUE(reading) IGNORE NULLS OVER (
        ORDER BY read_time 
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) AS forward_filled_reading
FROM iot_telemetry;
```

```
Result:
      read_time      | reading | forward_filled_reading 
---------------------+---------+------------------------
 2026-01-01 00:00:00 |   21.50 |                  21.50
 2026-01-01 00:01:00 |    NULL |                  21.50  (Forward filled from 00:00)
 2026-01-01 00:02:00 |    NULL |                  21.50  (Forward filled from 00:00)
 2026-01-01 00:03:00 |   23.00 |                  23.00
(4 rows)
```

---

## Best Practices

1. **Always Expand the Frame for `LAST_VALUE`**: Always append `ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING` when calling `LAST_VALUE()`.
2. **Provide Default Values to `LAG` and `LEAD`**: Use the 3-argument syntax `LAG(val, 1, 0)` if your calculations cannot tolerate initial `NULL` values.
3. **Never Specify Frames for `LAG` or `LEAD`**: Frame clauses are syntactically forbidden on relative offset functions.
4. **Use `FIRST_VALUE` in Reverse Rather Than `LAST_VALUE`**: Instead of buffering partitions with `LAST_VALUE(...) OVER (ORDER BY col ROWS BETWEEN ... UNBOUNDED FOLLOWING)`, sort descending and use `FIRST_VALUE(...) OVER (ORDER BY col DESC)`. This allows the engine to stream tuples immediately!
5. **Cross-Dialect Forward-Filling**: For engines lacking `IGNORE NULLS` (MySQL, SQLite), use a two-step CTE pattern with `COUNT(reading) OVER (ORDER BY ts)` to create grouping boundaries, followed by `FIRST_VALUE`.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Calling `LAST_VALUE()` without explicit frame | Default frame stops at `CURRENT ROW`, so `LAST_VALUE` returns the current row value. | Specify `ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING`. |
| 2 | Specifying `ROWS` or `RANGE` on `LAG()` or `LEAD()` | Syntactically illegal; raises an execution error across all ANSI engines. | Remove frame clause; specify offset directly inside function parameters. |
| 3 | Division by zero when computing growth with `LAG` | If the previous row's value is 0, `(curr - prev) / prev` crashes with division by zero. | Use `NULLIF(LAG(val), 0)` in the denominator. |
| 4 | Non-deterministic ordering in time-series queries | Sorting only by non-unique timestamps causes `LAG` to pick random peers. | Append unique ID to `ORDER BY` clause. |
| 5 | Assuming `FIRST_VALUE()` ignores `NULL`s | ANSI default is `RESPECT NULLS`; if the first row is `NULL`, `FIRST_VALUE` returns `NULL`. | Use `IGNORE NULLS` (PostgreSQL 17 / SQL Server 2022) or filter nulls. |
| 6 | Heavy memory bloat with `LAST_VALUE` on large tables | `UNBOUNDED FOLLOWING` forces engine to spool the entire partition before emitting row 1. | Reverse `ORDER BY` direction and use `FIRST_VALUE()`. |

---

## Performance & Security Notes

### Memory Optimization: `FIRST_VALUE(DESC)` vs `LAST_VALUE(ASC)`
When calculating the latest record in a partition:
```sql
-- Method A: Heavy Spooling (LAST_VALUE ASC)
SELECT LAST_VALUE(val) OVER (
    PARTITION BY dept_id ORDER BY created_at 
    ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
) FROM metrics;

-- Method B: Zero-Spool Streaming (FIRST_VALUE DESC)
SELECT FIRST_VALUE(val) OVER (
    PARTITION BY dept_id ORDER BY created_at DESC
) FROM metrics;
```
- In Method A, the engine cannot emit row 1 until it reads all 10,000,000 rows in the partition to locate the last value. This forces a massive `Window Spool` into temp disk files.
- In Method B, row 1 *is* the latest value! The engine reads the first row, caches it in memory, and immediately streams all remaining rows without spooling.

---

## Comparison Matrix

| Function | Frame Dependent? | Default Frame Target | Offset Direction | Dialect Support |
| :--- | :--- | :--- | :--- | :--- |
| **`LAG(col, n)`** | **No** (Frame forbidden) | Current Row $- n$ | Backward (Past) | Universal (All 4 engines) |
| **`LEAD(col, n)`** | **No** (Frame forbidden) | Current Row $+ n$ | Forward (Future) | Universal (All 4 engines) |
| **`FIRST_VALUE(col)`** | Yes | First row of frame | Frame start | Universal (All 4 engines) |
| **`LAST_VALUE(col)`** | **Yes** (Trap warning!) | End of frame (`CURRENT ROW` by default!) | Frame end | Universal (All 4 engines) |
| **`NTH_VALUE(col, n)`** | Yes | $n^{\text{th}}$ row of frame | Absolute frame index | PostgreSQL, MySQL (Missing in SQLite/MSSQL) |

---

## Interview Questions & Deep Dives

### Q1: Why does `LAST_VALUE(col) OVER (ORDER BY date)` return the current row instead of the partition's final value?
<details>
<summary>Click to view answer</summary>

Under ANSI SQL, specifying `ORDER BY` inside `OVER()` without an explicit frame clause sets the default frame to:
`RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW`.
Because the frame ends at `CURRENT ROW`, the "last row of the active frame" for any given row is the current row itself. To obtain the true partition-wide last value, the frame must be explicitly expanded:
`ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING`.
</details>

### Q2: How can you optimize a query using `LAST_VALUE()` to avoid disk spooling?
<details>
<summary>Click to view answer</summary>

Reverse the `ORDER BY` sort direction from `ASC` to `DESC` and replace `LAST_VALUE()` with `FIRST_VALUE()`. Because the default frame includes the first row of the partition (`UNBOUNDED PRECEDING`), `FIRST_VALUE()` requires no lookahead and does not force the database engine to spool the entire partition into memory or disk before emitting the first row.
</details>

### Q3: What is the third argument in `LAG(column, offset, default)`?
<details>
<summary>Click to view answer</summary>

The third argument specifies the fallback default value returned when the requested offset falls outside the partition boundary (e.g., calling `LAG` on the very first row). If omitted, it defaults to `NULL`. For example, `LAG(balance, 1, 0.00)` returns `0.00` instead of `NULL` for the initial record.
</details>

### Q4: Which SQL engines support the ANSI `IGNORE NULLS` clause on window functions?
<details>
<summary>Click to view answer</summary>

`IGNORE NULLS` is supported in PostgreSQL 17+ and Microsoft SQL Server 2022. It is currently **not supported** in MySQL 8.4 LTS or SQLite 3.50.4.
</details>

### Q5: Can `LAG()` look backward across partition boundaries?
<details>
<summary>Click to view answer</summary>

No. `LAG()` and `LEAD()` operate strictly within the confines of the current partition defined by `PARTITION BY`. When `LAG()` is evaluated on the first row of a partition, it returns `NULL` (or the default argument), even if rows exist in a preceding partition. To inspect prior rows across the entire dataset, omit the `PARTITION BY` clause.
</details>

### Q6: How would you simulate `IGNORE NULLS` on an engine that does not support it (like SQLite)?
<details>
<summary>Click to view answer</summary>

Use a two-step CTE:
1. Create a grouping cluster using `COUNT(col) OVER (ORDER BY ts)`. Because `COUNT(col)` increments only when `col` is non-null, all subsequent `NULL` rows receive the same count ID as the last non-null row.
2. In the outer query, use `FIRST_VALUE(col) OVER (PARTITION BY count_id ORDER BY ts)`.
</details>

### Q7: What is the difference between `NTH_VALUE(col, 2)` and `LEAD(col, 1)`?
<details>
<summary>Click to view answer</summary>

- `LEAD(col, 1)` is a relative offset: it returns the value of the immediately following row relative to whatever row is currently being processed.
- `NTH_VALUE(col, 2)` is an absolute position: it returns the value of the 2nd row from the beginning of the window frame, regardless of which row is currently being processed.
</details>

### Q8: What happens if `offset` in `LAG(col, offset)` is negative or zero?
<details>
<summary>Click to view answer</summary>

Under ANSI SQL, the offset must be a non-negative integer ($\ge 0$). Passing an offset of `0` returns the current row's value. Passing a negative offset raises an execution error in PostgreSQL (`ERROR: argument to lag() must be a positive integer`).
</details>

---

## Practice Exercises

### Exercise 1: Elapsed Time Between Consecutive Customer Orders
Given `orders(order_id, customer_id, order_time)`, calculate for each order the number of days elapsed since the customer's previous order. For their first order, return `0`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    order_id,
    customer_id,
    order_time,
    COALESCE(
        EXTRACT(DAY FROM (order_time - LAG(order_time) OVER (
            PARTITION BY customer_id ORDER BY order_time
        ))), 
        0
    ) AS days_since_last_order
FROM orders;
```
</details>

---

### Exercise 2: Price Fluctuation from Daily Open
Given `stock_ticks(tick_id, ticker, tick_time, price)`, compute the price difference between each trade and the day's opening trade price for that ticker.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    tick_id,
    ticker,
    tick_time,
    price,
    FIRST_VALUE(price) OVER (
        PARTITION BY ticker 
        ORDER BY tick_time ASC
    ) AS opening_price,
    price - FIRST_VALUE(price) OVER (
        PARTITION BY ticker 
        ORDER BY tick_time ASC
    ) AS price_delta_from_open
FROM stock_ticks;
```
</details>

---

### Exercise 3: Next Event Timestamp
Given `iot_events(event_id, device_id, event_time, event_code)`, project each event along with the timestamp of the device's next subsequent event. If no subsequent event exists, label it `'9999-12-31 23:59:59'`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    event_id,
    device_id,
    event_time,
    event_code,
    LEAD(event_time, 1, '9999-12-31 23:59:59'::timestamp) OVER (
        PARTITION BY device_id 
        ORDER BY event_time ASC
    ) AS next_event_time
FROM iot_events;
```
</details>

---

### Exercise 4: Detect State Transitions
Given `machine_state_log(log_id, machine_id, log_time, operational_state)`, write a query that filters to return only the rows where `operational_state` changed from its immediately prior recorded state.

<details>
<summary>Click to view solution</summary>

```sql
WITH state_transitions AS (
    SELECT 
        log_id,
        machine_id,
        log_time,
        operational_state,
        LAG(operational_state) OVER (
            PARTITION BY machine_id 
            ORDER BY log_time ASC
        ) AS prev_state
    FROM machine_state_log
)
SELECT log_id, machine_id, log_time, prev_state, operational_state
FROM state_transitions
WHERE prev_state IS NULL OR prev_state <> operational_state;
```
</details>

---

### Exercise 5: 2nd Earliest Sign-In IP Address
Given `user_logins(login_id, user_id, login_time, ip_address)`, find the IP address used on the user's 2nd login attempt using `NTH_VALUE`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT DISTINCT
    user_id,
    NTH_VALUE(ip_address, 2) OVER (
        PARTITION BY user_id 
        ORDER BY login_time ASC 
        ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
    ) AS second_login_ip
FROM user_logins;
```
</details>

---

### Exercise 6: Fixing the `LAST_VALUE` Null Bug
A reporting analyst writes:
```sql
SELECT employee_id, dept_id, hire_date,
       LAST_VALUE(hire_date) OVER (PARTITION BY dept_id ORDER BY hire_date)
FROM employees;
```
Explain why this returns `hire_date` identical to the current row, and write the corrected version using both `LAST_VALUE` and `FIRST_VALUE`.

<details>
<summary>Click to view solution</summary>

**Cause**: The default frame is `RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW`, so `LAST_VALUE` stops evaluating at the current row.

**Fix 1 (LAST_VALUE with explicit frame)**:
```sql
SELECT employee_id, dept_id, hire_date,
       LAST_VALUE(hire_date) OVER (
           PARTITION BY dept_id ORDER BY hire_date 
           ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
       ) AS most_recent_hire
FROM employees;
```

**Fix 2 (FIRST_VALUE with reversed sort - Preferred for performance)**:
```sql
SELECT employee_id, dept_id, hire_date,
       FIRST_VALUE(hire_date) OVER (
           PARTITION BY dept_id ORDER BY hire_date DESC
       ) AS most_recent_hire
FROM employees;
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                            VALUE WINDOW FUNCTIONS CHEAT SHEET                           |
+-------------------+----------------------------+----------------------------------------+
| Function          | Frame Behavior             | Key Purpose                            |
+-------------------+----------------------------+----------------------------------------+
| LAG(col, n, def)  | Frame forbidden            | Looks back n rows (Period-over-Period) |
| LEAD(col, n, def) | Frame forbidden            | Looks ahead n rows (Next event ts)     |
| FIRST_VALUE(col)  | Uses frame (Unbounded Prec)| Earliest value in window partition     |
| LAST_VALUE(col)   | MUST use UNBOUNDED FOLLOW  | Latest value (Trap: stops at curr row!)|
| NTH_VALUE(col, n) | MUST use UNBOUNDED FOLLOW  | Nth value from window start            |
| IGNORE NULLS      | Skips null entries         | Forward-fills missing telemetry data   |
+-------------------+----------------------------+----------------------------------------+
```

- **Avoid LAST_VALUE Spooling**: Sort descending and use `FIRST_VALUE(...) DESC` to enable streaming execution.
- **Sessionization Trick**: Sum boolean threshold flags computed via `LAG()` to generate contiguous session IDs.
- **Division Safety**: Always wrap `LAG()` denominators in `NULLIF(LAG(val), 0)`.

---

## Further Reading
- PostgreSQL 17 Documentation: *Value Window Functions & IGNORE NULLS Clause*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 6.10 `<window function>`
- Itzik Ben-Gan: *Solving Gaps and Islands Problems with Window Functions*
- Mode Analytics: *Using SQL Window Functions for Sessionization and Growth Analysis*
