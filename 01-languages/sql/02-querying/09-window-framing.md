# Window Framing: ROWS, RANGE, GROUPS, and Frame Exclusion

| Metadata | Details |
| :--- | :--- |
| **Topic** | SQL Window Framing Specifications (ROWS, RANGE, GROUPS, Frame Bounds, EXCLUDE Clause) |
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

While the `PARTITION BY` clause isolates subsets of data and `ORDER BY` defines row sequence, the **Window Frame** clause determines the exact slice of rows inside the partition that the window function inspects for the current row.

Without explicit framing, developers frequently fall victim to SQL's notorious default framing trap: adding an `ORDER BY` clause quietly changes the frame from the entire partition to `RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW`. This can produce surprising cumulative totals or collapse duplicate values unexpectedly.

Mastering window framing allows you to construct precise moving statistical calculations:
- 7-day or 30-day rolling revenue averages
- Centered 5-point smoothing filters for noisy time-series sensor data
- Year-to-date and month-to-date accumulators
- Moving volatility bands (standard deviation over rolling windows)
- Custom peer exclusions (excluding the current row or tied peers from sibling averages)

Understanding the distinction between physical row offsets (`ROWS`), logical value ranges (`RANGE`), and peer-group offsets (`GROUPS`) is essential for building robust financial, analytical, and telemetry systems.

---

## Core Concepts

### 1. Frame Units: ROWS vs RANGE vs GROUPS
A window frame is declared using one of three framing units:

1. **`ROWS` (Physical Row Offset)**:
   - Measures frame boundaries strictly by physical row counts relative to the current row.
   - `ROWS 3 PRECEDING` means exactly the 3 physically prior rows in the partition, regardless of whether ordering values are duplicated.
2. **`RANGE` (Logical Value Offset)**:
   - Measures frame boundaries by value differences based on the expression in `ORDER BY`.
   - Rows sharing identical ordering values are treated as **peers**.
   - Supports numeric and temporal intervals: `RANGE BETWEEN INTERVAL '7 days' PRECEDING AND CURRENT ROW`.
3. **`GROUPS` (Peer Group Offset)**:
   - Introduced in ANSI SQL:2011. Measures boundaries by the number of peer groups rather than individual rows.
   - If 5 rows share the same timestamp, they form 1 group. `GROUPS 1 PRECEDING` includes the entire preceding peer group regardless of how many rows it contains.

### 2. Frame Boundaries
A frame specification defines a start and end boundary using:
- **`UNBOUNDED PRECEDING`**: The frame starts at the very first row of the partition.
- **`n PRECEDING`**: The frame starts/ends $n$ units before the current row.
- **`CURRENT ROW`**: The frame starts/ends at the current row (for `ROWS`) or the current peer group (for `RANGE` and `GROUPS`).
- **`n FOLLOWING`**: The frame starts/ends $n$ units after the current row.
- **`UNBOUNDED FOLLOWING`**: The frame extends to the very last row of the partition.

### 3. The `EXCLUDE` Clause (ANSI SQL:2011)
The optional `EXCLUDE` clause allows surgical removal of rows from within the evaluated frame:
- **`EXCLUDE CURRENT ROW`**: Excludes the current row from the frame computation.
- **`EXCLUDE GROUP`**: Excludes the current row and all its ordering peers.
- **`EXCLUDE TIES`**: Excludes ordering peers of the current row, but keeps the current row itself.
- **`EXCLUDE NO OTHERS`** (Default): Keeps all rows within the frame boundaries.

---

## How It Works Internally

The query execution engine implements specialized sliding window aggregation algorithms to compute frame statistics efficiently.

```
                  [ Sorted Partition Stream ]
   Row 1 ──► [ Row 2 ──► Row 3 ──► Row 4 ] ──► Row 5 ──► ...
             │                           │
             └───────────┬───────────────┘
                         ▼
              [ Active Sliding Frame ]
              (e.g., ROWS 2 PRECEDING AND CURRENT ROW)
                         │
        ┌────────────────┴────────────────┐
        ▼                                 ▼
[ Naive Aggregator ]             [ Inverse Aggregator (Ring Buffer) ]
Recalculates SUM/AVG             Running accumulator maintains state:
across all K frame rows.         SUM_new = SUM_prev + NewRow - OldRow
Cost: O(N * K)                   Cost: O(1) amortized per row!
```

### 1. The Ring Buffer (Sliding Window Optimization)
For cumulative functions with invertible operations (such as `SUM`, `COUNT`, `AVG`):
- Rather than iterating over all $K$ rows in the frame for every single tuple ($O(N \times K)$ complexity), modern engines (PostgreSQL 11+, SQLite 3.28+) use an **inverse transition function**.
- As the window slides forward by one row, the engine adds the incoming row to the accumulator and subtracts the exiting row. This achieves an optimal **$O(1)$ amortized cost** per row.

### 2. Non-Invertible Aggregates (`MIN`, `MAX`)
For non-invertible functions like `MIN()` and `MAX()`, an exiting value cannot simply be subtracted. Engines utilize monotonic deque or two-stack queue algorithms (e.g., the Ascending Minima algorithm) to maintain moving minima/maxima in $O(1)$ amortized time.

---

## Syntax & Usage

```sql
SELECT 
    metric_date,
    revenue,
    -- 7-day physical moving average (preceding 6 rows + current row)
    AVG(revenue) OVER (
        ORDER BY metric_date 
        ROWS BETWEEN 6 PRECEDING AND CURRENT ROW
    ) AS moving_avg_7_rows,
    
    -- 7-day true calendar moving average (handles missing calendar dates!)
    AVG(revenue) OVER (
        ORDER BY metric_date 
        RANGE BETWEEN INTERVAL '6 days' PRECEDING AND CURRENT ROW
    ) AS moving_avg_7_calendar_days,

    -- Centered smoothing filter (2 rows before, current row, 2 rows after)
    AVG(revenue) OVER (
        ORDER BY metric_date 
        ROWS BETWEEN 2 PRECEDING AND 2 FOLLOWING
    ) AS centered_smoothed_avg,

    -- Leave-one-out average (average of peers excluding current row)
    AVG(revenue) OVER (
        ORDER BY metric_date 
        ROWS BETWEEN 2 PRECEDING AND 2 FOLLOWING 
        EXCLUDE CURRENT ROW
    ) AS avg_excluding_self
FROM daily_metrics;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **`ROWS` Framing** | Full ANSI support (`n PRECEDING / FOLLOWING`) | Full ANSI support | Full ANSI support (3.25+) | Full ANSI support |
| **`RANGE` with Numeric / Intervals** | Fully supported: `RANGE BETWEEN 10 PRECEDING` & `RANGE BETWEEN INTERVAL '7 days'` | Supported for numeric/date offsets | Supported for numeric/date offsets (3.28+) | **Not Supported** (Only supports `UNBOUNDED` or `CURRENT ROW`) |
| **`GROUPS` Unit** | Fully supported (PostgreSQL 11+) | **Not Supported** | Supported (SQLite 3.28+) | **Not Supported** |
| **`EXCLUDE` Clause** | Fully supported (PostgreSQL 11+) | **Not Supported** | Supported (SQLite 3.28+) | **Not Supported** |

---

## Practical Examples

### Example 1: Basic — Physical Moving Average with `ROWS`
Calculate a 3-day trailing moving average of transaction volume.

```sql
CREATE TABLE transaction_volume (
    log_id INT PRIMARY KEY,
    day_num INT NOT NULL,
    volume INT NOT NULL
);

INSERT INTO transaction_volume VALUES
(1, 1, 100),
(2, 2, 120),
(3, 3, 110),
(4, 4, 150),
(5, 5, 140);

SELECT 
    day_num,
    volume,
    -- Frame: 2 preceding + current row = 3 rows total
    ROUND(AVG(volume) OVER (
        ORDER BY day_num 
        ROWS BETWEEN 2 PRECEDING AND CURRENT ROW
    ), 2) AS trailing_3day_avg
FROM transaction_volume;
```

```
Result:
 day_num | volume | trailing_3day_avg 
---------+--------+-------------------
       1 |    100 |            100.00
       2 |    120 |            110.00
       3 |    110 |            110.00
       4 |    150 |            126.67
       5 |    140 |            133.33
(5 rows)
```

---

### Example 2: Real-World — True Calendar-Based Range Framing
When time-series data contains missing weekend or holiday dates, `ROWS 6 PRECEDING` does not represent a true 7-day period. Use `RANGE` with an `INTERVAL` to enforce calendar boundaries.

```sql
CREATE TABLE daily_sales (
    sale_date DATE PRIMARY KEY,
    amount NUMERIC(10,2) NOT NULL
);

-- Note the gap: 2026-01-02 and 2026-01-03 are missing (weekend)
INSERT INTO daily_sales VALUES
('2026-01-01', 1000.00),
('2026-01-04', 2000.00),
('2026-01-05', 1500.00);

-- PostgreSQL / SQLite Date Range Framing:
SELECT 
    sale_date,
    amount,
    -- Looks back strictly 3 calendar days
    SUM(amount) OVER (
        ORDER BY sale_date 
        RANGE BETWEEN INTERVAL '3 days' PRECEDING AND CURRENT ROW
    ) AS revenue_last_3_days
FROM daily_sales;
```

```
Result:
 sale_date  | amount  | revenue_last_3_days 
------------+---------+---------------------
 2026-01-01 | 1000.00 |             1000.00
 2026-01-04 | 2000.00 |             3000.00  (Includes 2026-01-01 + 2026-01-04)
 2026-01-05 | 1500.00 |             3500.00  (Includes 2026-01-04 + 2026-01-05)
(3 rows)
```

---

### Example 3: Edge Case — Frame Peering with `GROUPS`
Demonstrating how `GROUPS` treats duplicate order values as single conceptual units.

```sql
CREATE TABLE sensor_readings (
    reading_id INT PRIMARY KEY,
    batch_cycle INT NOT NULL,
    temperature NUMERIC(5,2) NOT NULL
);

INSERT INTO sensor_readings VALUES
(1, 10, 20.5),
(2, 10, 21.0), -- Batch 10 has 2 rows
(3, 20, 25.0),
(4, 30, 30.0);

SELECT 
    reading_id,
    batch_cycle,
    temperature,
    -- Considers 1 preceding batch group + current batch group
    COUNT(*) OVER (
        ORDER BY batch_cycle 
        GROUPS BETWEEN 1 PRECEDING AND CURRENT ROW
    ) AS readings_in_window
FROM sensor_readings;
```

```
Result:
 reading_id | batch_cycle | temperature | readings_in_window 
------------+-------------+-------------+--------------------
          1 |          10 |       20.50 |                  2
          2 |          10 |       21.00 |                  2
          3 |          20 |       25.00 |                  3  (Batch 10 [2 rows] + Batch 20 [1 row])
          4 |          30 |       30.00 |                  2  (Batch 20 [1 row] + Batch 30 [1 row])
(4 rows)
```

---

### Example 4: Failure Mode — Invalid Frame Boundaries
A frame start boundary cannot logically appear *after* the frame end boundary.

```sql
-- FATAL ERROR:
-- ERROR: frame start cannot be after frame end
SELECT 
    volume,
    SUM(volume) OVER (
        ORDER BY day_num 
        ROWS BETWEEN 1 FOLLOWING AND 2 PRECEDING
    )
FROM transaction_volume;

-- CORRECT USAGE: Order chronologically from past to future
SELECT 
    volume,
    SUM(volume) OVER (
        ORDER BY day_num 
        ROWS BETWEEN 2 PRECEDING AND 1 FOLLOWING
    )
FROM transaction_volume;
```

---

### Example 5: Optimized — Sibling Leave-One-Out Average via `EXCLUDE CURRENT ROW`
Calculate the average metric among team members excluding the employee's own contribution.

```sql
CREATE TABLE team_sales (
    rep_id INT PRIMARY KEY,
    team_name VARCHAR(20) NOT NULL,
    sales_score INT NOT NULL
);

INSERT INTO team_sales VALUES
(1, 'Alpha', 100),
(2, 'Alpha', 80),
(3, 'Alpha', 60);

SELECT 
    rep_id,
    team_name,
    sales_score,
    -- Average of all other teammates in Alpha
    ROUND(AVG(sales_score) OVER (
        PARTITION BY team_name 
        ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING 
        EXCLUDE CURRENT ROW
    ), 2) AS teammates_avg_score
FROM team_sales;
```

```
Result:
 rep_id | team_name | sales_score | teammates_avg_score 
--------+-----------+-------------+---------------------
      1 | Alpha     |         100 |               70.00  ((80 + 60) / 2)
      2 | Alpha     |          80 |               80.00  ((100 + 60) / 2)
      3 | Alpha     |          60 |               90.00  ((100 + 80) / 2)
(3 rows)
```

---

## Best Practices

1. **Explicitly Specify `ROWS` for Row-Count Moving Averages**: Never write `AVG(x) OVER (ORDER BY y)` expecting a moving average; explicitly specify `ROWS BETWEEN n PRECEDING AND CURRENT ROW`.
2. **Use `RANGE` with `INTERVAL` for Incomplete Date Series**: If calendar dates have gaps (weekends, holidays), physical `ROWS` yields incorrect time windows. Use `RANGE BETWEEN INTERVAL 'n days' PRECEDING AND CURRENT ROW`.
3. **Prefer `ROWS` over `RANGE` for Performance in Older Engines**: If ordering values are unique, `ROWS` execution is faster and uses less memory than `RANGE` because it avoids peer detection checks.
4. **Enforce Chronological Frame Sequences**: Ensure that boundary specifications strictly progress forward: `[PRECEDING -> CURRENT ROW -> FOLLOWING]`.
5. **SQL Server Workaround for Calendar Ranges**: Because SQL Server does not support `RANGE BETWEEN INTERVAL`, join against a dedicated `DimDate` calendar table or calculate rolling sums via `CROSS APPLY`.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Assuming `ORDER BY` gives a partition total | `ORDER BY` silently defaults frame to `RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW`. | Omit `ORDER BY` if you need the full partition total. |
| 2 | Using `ROWS 7 PRECEDING` for a weekly average | Includes 8 rows total ($7 + 1$ current row), skewing denominator. | Use `ROWS BETWEEN 6 PRECEDING AND CURRENT ROW` for a 7-day average. |
| 3 | Frame start placed after frame end | `ROWS BETWEEN CURRENT ROW AND 2 PRECEDING` violates chronological sequencing and raises syntax errors. | Write `ROWS BETWEEN 2 PRECEDING AND CURRENT ROW`. |
| 4 | Ignoring missing calendar dates in time series | `ROWS 30 PRECEDING` spans months if transactions occur only once a week. | Use `RANGE BETWEEN INTERVAL '30 days' PRECEDING` or pre-generate date spine. |
| 5 | Using `RANGE` on non-numeric/non-date types | `RANGE 5 PRECEDING` on text columns raises a type error; range offsets require continuous, ordered arithmetic types. | Use `ROWS` for text or composite order keys. |
| 6 | Attempting to use `EXCLUDE` in SQL Server | SQL Server 2022 does not support the ANSI `EXCLUDE` frame clause. | Subtract current value algebraically: `(SUM(x) OVER() - x) / (COUNT(*) OVER() - 1)`. |

---

## Performance & Security Notes

### Ring Buffer Invertible Execution vs Spooling
In PostgreSQL 17, check the execution plan of a sliding frame:
```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT id, AVG(val) OVER (ORDER BY id ROWS BETWEEN 100 PRECEDING AND CURRENT ROW)
FROM metrics;
```
- When using `ROWS BETWEEN ...` with standard aggregates (`SUM`, `COUNT`, `AVG`), PostgreSQL uses an in-memory ring buffer with inverse transition functions (`invagg`).
- Memory usage is tightly bounded to the frame size $K$. Execution time scales linearly as $O(N)$ rather than $O(N \times K)$.
- If using `RANGE` without unique indexes, the engine must buffer and inspect all peer groups to determine frame edges, increasing memory usage.

---

## Comparison Matrix

| Framing Dimension | `ROWS` | `RANGE` | `GROUPS` |
| :--- | :--- | :--- | :--- |
| **Unit of Measurement** | Physical row count | Logical value difference | Logical peer-group count |
| **Duplicate Order Handling** | Distinct rows evaluated individually | Tied values grouped as peers | Tied values grouped as 1 unit |
| **Offset Types** | Integer constant only | Constant, Interval, Numeric | Integer constant only |
| **Temporal Intervals** | Not supported | Supported (`INTERVAL '7 days'`) | Not supported |
| **Engine Portability** | Universal (All 4 engines) | Standard syntax universal; Offsets vary | PostgreSQL 11+, SQLite 3.28+ |

---

## Interview Questions & Deep Dives

### Q1: What is the exact default frame specification when `ORDER BY` is present inside `OVER ()`?
<details>
<summary>Click to view answer</summary>

Under the ANSI SQL standard, when `ORDER BY` is present in a window specification and no explicit frame clause is provided, the frame defaults to:
`RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW`.
This includes all rows from the beginning of the partition up to and including the current row's ordering peers.
</details>

### Q2: Why does `ROWS BETWEEN 6 PRECEDING AND CURRENT ROW` calculate a 7-row window rather than 6?
<details>
<summary>Click to view answer</summary>

The boundary specification includes 6 preceding rows, PLUS the current row itself ($6 + 1 = 7$ rows). To calculate an $N$-element trailing window including the current row, the preceding offset must always be $N - 1$.
</details>

### Q3: What is the semantic difference between `ROWS` and `RANGE` when ordering by a non-unique column?
<details>
<summary>Click to view answer</summary>

- `ROWS` treats every row as a separate physical entity. If three rows share the order date `'2026-01-01'`, `ROWS` processes them one by one.
- `RANGE` treats all rows with the same order value as peers. When the frame boundary specifies `CURRENT ROW`, `RANGE` expands the boundary to include **all peers** that share the same ordering value as the current row. This causes cumulative metrics to jump to the total of the entire tied group simultaneously.
</details>

### Q4: How does `EXCLUDE TIES` differ from `EXCLUDE GROUP`?
<details>
<summary>Click to view answer</summary>

- `EXCLUDE GROUP` removes the current row **and** all its ordering peers from the frame calculation.
- `EXCLUDE TIES` removes only the ordering peers, but keeps the current row inside the frame calculation.
</details>

### Q5: How can a database compute a sliding window `SUM` in $O(1)$ time per row?
<details>
<summary>Click to view answer</summary>

The engine uses an **inverse transition function**. As the window frame slides forward by one row, instead of summing all $K$ rows from scratch:
1. It adds the value of the new incoming row entering the right side of the frame to the running accumulator.
2. It subtracts the value of the old exiting row leaving the left side of the frame from the running accumulator.
This requires only 1 addition and 1 subtraction per row, running in $O(1)$ amortized time.
</details>

### Q6: Can you specify `ROWS BETWEEN CURRENT ROW AND UNBOUNDED FOLLOWING`?
<details>
<summary>Click to view answer</summary>

Yes. This defines a reverse cumulative frame that includes the current row and all subsequent rows until the end of the partition. It is commonly used to calculate "remaining balance" or "future pipeline value".
</details>

### Q7: Why does SQL Server reject `RANGE BETWEEN 5 PRECEDING AND CURRENT ROW`?
<details>
<summary>Click to view answer</summary>

Microsoft SQL Server's T-SQL implementation of `RANGE` only supports `UNBOUNDED PRECEDING`, `CURRENT ROW`, and `UNBOUNDED FOLLOWING`. It does not support explicit numeric or temporal value offsets with `RANGE`. Developers must use `ROWS` or rewrite using joins.
</details>

### Q8: What is the purpose of the `GROUPS` framing unit added in SQL:2011?
<details>
<summary>Click to view answer</summary>

`GROUPS` allows developers to define window frames based on counts of peer groups rather than individual rows or continuous values. For example, `GROUPS BETWEEN 2 PRECEDING AND CURRENT ROW` includes all rows from the previous 2 distinct ordering values plus the current ordering value, regardless of how many individual rows share those values.
</details>

---

## Practice Exercises

### Exercise 1: 5-Point Centered Moving Average
Given `sensor_telemetry(ts TIMESTAMP, temperature NUMERIC)`, write a query to compute a 5-point centered moving average (2 rows before, current row, 2 rows after) ordered by timestamp.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    ts,
    temperature,
    ROUND(AVG(temperature) OVER (
        ORDER BY ts 
        ROWS BETWEEN 2 PRECEDING AND 2 FOLLOWING
    ), 2) AS smoothed_temp
FROM sensor_telemetry;
```
</details>

---

### Exercise 2: Rolling 30-Day Revenue
Given `orders(order_id, order_date, total_amount)`, write a query using PostgreSQL's calendar interval framing to calculate total revenue over a rolling 30-day window (29 days preceding + current date).

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    order_date,
    total_amount,
    SUM(total_amount) OVER (
        ORDER BY order_date 
        RANGE BETWEEN INTERVAL '29 days' PRECEDING AND CURRENT ROW
    ) AS rolling_30day_revenue
FROM orders;
```
</details>

---

### Exercise 3: Remaining Pipeline Revenue
Given `deals(deal_id, closing_date, projected_revenue)`, compute for each deal the total projected revenue of all deals closing on or after that deal's closing date.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    deal_id,
    closing_date,
    projected_revenue,
    SUM(projected_revenue) OVER (
        ORDER BY closing_date ASC 
        ROWS BETWEEN CURRENT ROW AND UNBOUNDED FOLLOWING
    ) AS remaining_pipeline_revenue
FROM deals;
```
</details>

---

### Exercise 4: Leave-Current-Row-Out Sum
Given `audit_scores(auditor_id, department_id, score)`, write a query that calculates the sum of all scores in the department excluding the auditor's own score, using `EXCLUDE CURRENT ROW`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    auditor_id,
    department_id,
    score,
    SUM(score) OVER (
        PARTITION BY department_id 
        ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING 
        EXCLUDE CURRENT ROW
    ) AS other_auditors_total_score
FROM audit_scores;
```
</details>

---

### Exercise 5: Rolling Volatility (Moving Standard Deviation)
Calculate a 14-day trailing sample standard deviation of stock closing prices using `STDDEV_SAMP`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    trade_date,
    closing_price,
    ROUND(STDDEV_SAMP(closing_price) OVER (
        ORDER BY trade_date 
        ROWS BETWEEN 13 PRECEDING AND CURRENT ROW
    ), 4) AS rolling_14day_volatility
FROM daily_stock_prices;
```
</details>

---

### Exercise 6: Cross-Dialect Framing Workaround for SQL Server
SQL Server does not support `RANGE BETWEEN INTERVAL '7 days' PRECEDING`. Rewrite the rolling 7-day revenue calculation using a correlated subquery or `CROSS APPLY` that runs efficiently on SQL Server.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    o1.order_date,
    o1.total_amount,
    ca.rolling_7day_revenue
FROM orders o1
CROSS APPLY (
    SELECT SUM(o2.total_amount) AS rolling_7day_revenue
    FROM orders o2
    WHERE o2.order_date BETWEEN DATEADD(day, -6, o1.order_date) AND o1.order_date
) ca;
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                               WINDOW FRAMING CHEAT SHEET                                |
+-------------------+----------------------------+----------------------------------------+
| Frame Clause      | Range Definition           | Common Use Case                        |
+-------------------+----------------------------+----------------------------------------+
| ROWS BETWEEN ...  | Physical row count offsets | N-row moving average, smoothing        |
| RANGE BETWEEN ... | Value / Interval offsets   | Calendar date rolling totals (7d, 30d) |
| GROUPS BETWEEN ...| Count of tied peer groups  | Batch/Cycle rolling aggregates         |
| EXCLUDE CURRENT   | Drops current row from sum | Leave-one-out benchmarks, peer averages|
| EXCLUDE TIES      | Drops peers, keeps current | Disambiguate tied competitors          |
+-------------------+----------------------------+----------------------------------------+
```

- **Default Frame Warning**: Specifying `ORDER BY` activates `RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW`.
- **Moving Average Rule**: For an $N$-point trailing window, use `ROWS BETWEEN (N-1) PRECEDING AND CURRENT ROW`.
- **Performance**: Invertible aggregates (`SUM`, `COUNT`, `AVG`) slide in $O(1)$ amortized time via ring buffers.

---

## Further Reading
- PostgreSQL 17 Documentation: *Window Function Calls & Framing Syntax*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 7.13 `<window frame clause>`
- Itzik Ben-Gan: *High Performance T-SQL Window Functions and Moving Aggregates*
- SQLite 3.28 Release Notes: *Window Function Support for GROUPS and Frame EXCLUDE*
