# 13: Conditional Expressions: CASE, COALESCE & NULLIF

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [12 - Date, Time & Timezone Handling](./12-date-and-time.md) |
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

In declarative SQL, queries express what data is needed, but business logic is rarely uniform. Systems need to categorize accounts into tiers, apply variable tax rates based on jurisdiction, guard calculations against divide-by-zero errors, pivot relational rows into summary columns, and substitute sensible defaults for missing attributes.

The `CASE` expression is SQL’s universal control-flow structure. Unlike procedural `if/else` statements that execute code imperatively, `CASE` is a **scalar expression**: it evaluates conditions sequentially and returns a single concrete scalar value. 

Together with specialized standard functions:
- **`COALESCE`**: Chained fallback for `NULL` handling.
- **`NULLIF`**: Converts specific values (such as zero or empty strings) to `NULL` to prevent exceptions.
- **`GREATEST` / `LEAST`**: Multi-column scalar min/max evaluation.

Mastering conditional expressions allows engineers to write expressive, resilient SQL that handles edge cases cleanly without resorting to awkward client-side post-processing.

---

## 2. Core Concepts

### 1. Simple CASE vs Searched CASE
SQL supports two distinct forms of `CASE`:

1. **Simple `CASE` (Equality Matching)**:
   Compares an expression against discrete literal values:
   ```sql
   CASE status
       WHEN 'pending'   THEN 'Under Review'
       WHEN 'completed' THEN 'Fulfilled'
       ELSE 'Unknown'
   END
   ```
   *Limitation: Evaluates only strict equality (`=`). Cannot perform range checks (`>`), null checks (`IS NULL`), or complex boolean combinations.*

2. **Searched `CASE` (Boolean Predicates - Recommended Standard)**:
   Evaluates arbitrary boolean conditions sequentially from top to bottom:
   ```sql
   CASE 
       WHEN total_amount >= 1000.00 THEN 'VIP Tier'
       WHEN total_amount >= 500.00  THEN 'Preferred Tier'
       WHEN total_amount IS NULL    THEN 'Unassessed'
       ELSE 'Standard Tier'
   END
   ```
   *Searched `CASE` is far more powerful, flexible, and explicit.*

### 2. Sequential Evaluation & Short-Circuit Guarantee
Unlike `WHERE` clauses (where the query planner may reorder predicates for cost reasons), the SQL standard **strictly guarantees that `CASE` evaluates its `WHEN` branches in written top-to-bottom order**.

The engine stops evaluating as soon as it encounters the first branch whose condition evaluates to `TRUE`. Subsequent branches are never evaluated:
```sql
-- SAFE: If denominator is 0, Branch 1 matches and Branch 2 is NEVER evaluated!
CASE 
    WHEN denominator = 0 THEN 0.00
    ELSE numerator / denominator
END
```

### 3. Return Type Homogeneity
Every `THEN` branch and the `ELSE` branch in a `CASE` expression must return data types that are mutually compatible. The database engine determines a single common data type for the entire expression.
- Returning `100` (integer) in branch 1 and `'N/A'` (text) in branch 2 throws a compilation type error in strict engines like PostgreSQL:
  `ERROR: invalid input syntax for type integer: "N/A"`.

---

## 3. How It Works Internally

### Logical Compilation of COALESCE and NULLIF

Behind the scenes, `COALESCE` and `NULLIF` are syntactic sugar formally defined in the SQL standard as macro translations into searched `CASE` expressions:

```
COALESCE(a, b, c) Translation:
CASE 
    WHEN a IS NOT NULL THEN a 
    WHEN b IS NOT NULL THEN b 
    ELSE c 
END

NULLIF(a, b) Translation:
CASE 
    WHEN a = b THEN NULL 
    ELSE a 
END
```

- When the query parser encounters `COALESCE` or `NULLIF`, it expands the AST into a standard `CaseExpr` node.
- The executor evaluates expressions lazily: in `COALESCE(colA, expensive_func())`, `expensive_func()` is executed **only if `colA` is NULL**.

---

## 4. Syntax & Usage

### Core Conditional Expressions

```sql
SELECT 
    -- 1. Searched CASE with range checks
    CASE 
        WHEN score >= 90 THEN 'A'
        WHEN score >= 80 THEN 'B'
        WHEN score >= 70 THEN 'C'
        ELSE 'F'
    END AS letter_grade,

    -- 2. COALESCE (First non-null value)
    COALESCE(phone_work, phone_mobile, phone_home, 'No Contact') AS best_contact_phone,

    -- 3. NULLIF (Zero-division defense)
    revenue / NULLIF(units_sold, 0) AS unit_price,

    -- 4. GREATEST and LEAST (Scalar Multi-Column Min/Max)
    GREATEST(start_salary, bonus, commission) AS highest_comp_item,
    LEAST(current_stock, max_capacity) AS effective_stock;
```

### Multi-Dialect Comparison Matrix

| Conditional Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Searched `CASE`** | Full support | Full support | Full support | Full support |
| **`COALESCE()`** | Full support | Full support | Full support | Full support |
| **`NULLIF()`** | Full support | Full support | Full support | Full support |
| **`GREATEST()` / `LEAST()`** | Supported (Ignores NULL unless all null)| Supported (Returns NULL if any arg is NULL!)| Supported (3.38+)| Supported (since 2022) |
| **`IF()` Function** | Not supported (use `CASE`)| Supported (`IF(cond, a, b)`)| Supported (`IIF(cond, a, b)`)| Supported (`IIF(cond, a, b)`)|
| **`IFNULL()` / `ISNULL()`** | Use standard `COALESCE` | `IFNULL(a, b)` | `IFNULL(a, b)` | `ISNULL(a, b)` |

---

## 5. 5 Practical Progressive Examples

### Example 1: Conditional Aggregation (Pivoting Data)
Pivoting rows into columns without complex external transformation:

```sql
-- Count orders by status across geographical regions in a single query:
SELECT 
    country,
    COUNT(*) AS total_orders,
    COUNT(CASE WHEN order_status = 'completed' THEN 1 END) AS completed_orders,
    COUNT(CASE WHEN order_status = 'pending'   THEN 1 END) AS pending_orders,
    COUNT(CASE WHEN order_status = 'cancelled' THEN 1 END) AS cancelled_orders
FROM customers c
JOIN orders o ON c.customer_id = o.customer_id
GROUP BY country
ORDER BY total_orders DESC;
```
```text
+---------+--------------+------------------+----------------+------------------+
| country | total_orders | completed_orders | pending_orders | cancelled_orders |
+---------+--------------+------------------+----------------+------------------+
| USA     |            2 |                2 |              0 |                0 |
| India   |            1 |                1 |              0 |                0 |
| Canada  |            1 |                0 |              1 |                0 |
| Spain   |            1 |                0 |              0 |                1 |
+---------+--------------+------------------+----------------+------------------+
```
*(Notice how `COUNT(CASE ... END)` works: when status doesn't match, `CASE` returns `NULL`. `COUNT()` ignores `NULL`s, counting only matches!)*

### Example 2: Dynamic Custom Sorting with `CASE`
Sorting by a custom business status hierarchy:

```sql
SELECT order_id, customer_id, order_status, total_amount
FROM orders
ORDER BY 
    CASE order_status
        WHEN 'pending'    THEN 1
        WHEN 'processing' THEN 2
        WHEN 'completed'  THEN 3
        WHEN 'cancelled'  THEN 4
        ELSE 5
    END ASC,
    total_amount DESC;
```
```text
+----------+-------------+--------------+--------------+
| order_id | customer_id | order_status | total_amount |
+----------+-------------+--------------+--------------+
|     1004 |         103 | pending      |      1099.00 |
|     1001 |         101 | completed    |      1444.00 |
|     1002 |         102 | completed    |      1398.00 |
|     1003 |         101 | completed    |        45.00 |
|     1005 |         104 | cancelled    |       399.00 |
+----------+-------------+--------------+--------------+
```

### Example 3: Edge Case (Type Mismatch Across Result Branches)
What happens when different `THEN` branches return incompatible types:

```sql
-- FAILS in strict PostgreSQL:
SELECT 
    product_name,
    CASE 
        WHEN stock_qty > 0 THEN stock_qty -- integer
        ELSE 'Out of Stock'               -- text
    END AS inventory_display
FROM products;
```
```text
ERROR: invalid input syntax for type integer: "Out of Stock"
```
**Fix: Explicit Casting to Common Supertype (TEXT)**:
```sql
SELECT 
    product_name,
    CASE 
        WHEN stock_qty > 0 THEN stock_qty::text
        ELSE 'Out of Stock'
    END AS inventory_display
FROM products;
```

### Example 4: Failure Mode (Missing `ELSE` Branch Returning Silent NULLs)
What happens if no `WHEN` branch matches and `ELSE` is omitted:

```sql
-- Omitting ELSE defaults to ELSE NULL:
SELECT 
    product_name,
    CASE 
        WHEN retail_price > 1000 THEN 'Expensive'
        WHEN retail_price < 50   THEN 'Budget'
        -- No ELSE!
    END AS price_tier
FROM products;
```
```text
+---------------------------------------+------------+
| product_name                          | price_tier |
+---------------------------------------+------------+
| Framework Laptop 16                   | Expensive  |
| MacBook Air M3                        | Expensive  |
| Pixel 9 Pro                           | NULL       |
| Sony WH-1000XM5                       | NULL       |
| Designing Data-Intensive Applications | Budget     |
+---------------------------------------+------------+
```
*(Pixel 9 Pro and Sony Headphones cost between $50 and $1,000, falling through to `NULL`). Always specify an explicit `ELSE` branch).*

### Example 5: Optimized Safe Multi-Tier Calculation
Calculating progressive shipping fees based on order weight and destination:

```sql
SELECT 
    order_id,
    weight_kg,
    destination_country,
    CASE 
        WHEN destination_country = 'USA' THEN 
            CASE 
                WHEN weight_kg <= 2.0 THEN 5.00
                WHEN weight_kg <= 10.0 THEN 12.00
                ELSE 12.00 + (weight_kg - 10.0) * 1.50
            END
        ELSE 
            CASE 
                WHEN weight_kg <= 2.0 THEN 15.00
                ELSE 15.00 + (weight_kg - 2.0) * 3.50
            END
    END AS shipping_cost
FROM shipment_packages;
```

---

## 6. Best Practices

1. **Always Provide an Explicit `ELSE` Clause**: Never omit `ELSE`. If no branch matches and `ELSE` is missing, SQL silently evaluates to `NULL`. Supplying an explicit `ELSE 'default'` prevents unexpected nulls.
2. **Use Searched `CASE` by Default**: Avoid simple `CASE` (`CASE col WHEN ...`). Searched `CASE` (`CASE WHEN col = ...`) handles ranges, nulls, and multiple conditions with complete clarity.
3. **Use `NULLIF(col, 0)` for All Divisions**: Wrap every denominator in `NULLIF` unless guaranteed positive by a schema `CHECK` constraint.
4. **Use `COALESCE` Over Vendor Functions**: Avoid proprietary functions like `IFNULL` (MySQL/SQLite) or `NVL` (Oracle). `COALESCE` is standard ANSI SQL and accepts arbitrary argument lengths.
5. **Ensure Type Consistency Across Branches**: Explicitly cast numeric, boolean, or date values to text if any branch returns text.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Type Mismatch in Branches** | Returning integer in Branch 1 and text in Branch 2 throws a compilation type error. | Cast all branches to a common type: `THEN col::text`. |
| **Omitting `ELSE`** | Unmatched rows quietly evaluate to `NULL`, corrupting downstream calculations. | Always write an explicit `ELSE` fallback. |
| **Using Simple CASE for NULL Checks** | `CASE col WHEN NULL THEN 'x'` never matches because `col = NULL` evaluates to `UNKNOWN`. | Use searched `CASE WHEN col IS NULL THEN 'x'`. |
| **MySQL `GREATEST` Returning NULL** | In MySQL, if *any* argument in `GREATEST(a, b, c)` is NULL, the entire function returns `NULL`. | Use `COALESCE(a, 0)` on arguments in MySQL. |
| **Overlapping Range Branches** | `WHEN age >= 18 THEN 'Adult'` placed before `WHEN age >= 65 THEN 'Senior'` prevents seniors from ever being matched! | Order specific conditions from most restrictive to least restrictive. |
| **Procedural `IF` in Query Bodies** | `IF (cond) ...` is a procedural statement valid only in stored routines, not in raw queries. | Use the scalar `CASE` expression. |

---

## 8. Performance & Security Considerations

### Performance
- **Short-Circuit Optimization**: Because `CASE` evaluates sequentially and halts on the first `TRUE` condition, place the **most frequent condition first** (e.g., if 90% of rows are `'completed'`, evaluate that first) to save CPU cycles on subsequent branch evaluations.
- **Expensive Subqueries in `CASE`**: Avoid putting expensive scalar subqueries in branches that evaluate frequently. Wrap subqueries in CTEs or join tables instead.

### Security
- **Bypassing Row-Level Logic with Unhandled Nulls**:
  - If a authorization branch evaluates:
    `CASE WHEN role = 'admin' THEN 'FULL_ACCESS' ELSE 'RESTRICTED' END`
  - Ensure the fallback branch defaults to the least-privilege state (`RESTRICTED`).

---

## 9. Comparison Matrix

### Conditional Functions Compared

| Function / Expression | SQL Standard | Number of Arguments | Return Value | Purpose |
|:---|:---:|:---:|:---|:---|
| **`CASE ... END`** | ANSI SQL-92 | Arbitrary branches | Value of first matching branch | General conditional branching |
| **`COALESCE(a, b, ...)`**| ANSI SQL-92 | $\ge 2$ arguments | First non-null argument | Fallback default substitution |
| **`NULLIF(a, b)`** | ANSI SQL-92 | Exactly 2 | `NULL` if $a = b$, else $a$ | Zero-division guard, null normalization |
| **`GREATEST(a, b, ...)`**| ANSI SQL:2008 | $\ge 2$ arguments | Largest scalar value | Cross-column horizontal maximum |
| **`LEAST(a, b, ...)`** | ANSI SQL:2008 | $\ge 2$ arguments | Smallest scalar value | Cross-column horizontal minimum |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: What is the difference between a Simple `CASE` expression and a Searched `CASE` expression?
<details>
<summary>Answer</summary>
- **Simple `CASE`**: Has the syntax `CASE expression WHEN value THEN ... END`. It compares the initial `expression` for exact equality against each candidate `value`. It cannot test ranges (`<`, `>`), check for `IS NULL`, or evaluate multi-column predicates.
- **Searched `CASE`**: Has the syntax `CASE WHEN boolean_condition THEN ... END`. Each `WHEN` clause evaluates an independent boolean predicate. It supports range comparisons, `IS NULL`, pattern matching, and complex boolean combinations (`AND`/`OR`).
</details>

#### Q2: Why does `CASE status WHEN NULL THEN 'Unknown' END` never evaluate to 'Unknown'?
<details>
<summary>Answer</summary>
Simple `CASE` performs an equality comparison (`=`) between the operand and the `WHEN` value. 
The expression expands internally to: `status = NULL`.

Under Three-Valued Logic, comparing any value to `NULL` via equality evaluates to `UNKNOWN`, never `TRUE`. Because a `CASE` branch executes only when its condition evaluates strictly to `TRUE`, the branch never matches. 
Fix: Use searched `CASE`:
`CASE WHEN status IS NULL THEN 'Unknown' END`.
</details>

#### Q3: Does the SQL standard guarantee short-circuit evaluation for `CASE` expressions?
<details>
<summary>Answer</summary>
**Yes.** Unlike the `WHERE` clause (where the optimizer may evaluate predicates in any order based on cost statistics), the SQL standard explicitly specifies that `CASE` expressions must be evaluated in written lexical order from top to bottom. Once a condition evaluates to `TRUE`, subsequent conditions must not be evaluated. 

This makes `CASE WHEN den = 0 THEN 0 ELSE num / den END` guaranteed safe against division by zero across all compliant engines.
</details>

#### Q4: What is the difference between `COALESCE` and `NULLIF`?
<details>
<summary>Answer</summary>
- `COALESCE(a, b, c)` returns the **first non-null** value among its arguments. If all are NULL, it returns NULL. It is used to replace NULLs with default fallbacks.
- `NULLIF(a, b)` compares two arguments. If $a = b$, it returns **`NULL`**; otherwise, it returns $a$. It is used to turn specific sentinel values (like 0 or empty strings) into NULL.
</details>

### Practical Conditional Puzzles

#### Q5: Write a query to classify customer spending into 3 tiers: 'Platinum' ($\ge \$1,000$), 'Gold' ($\ge \$500$), and 'Silver' ($< \$500$), handling customers with 0 orders.
<details>
<summary>Answer</summary>

```sql
SELECT 
    c.customer_id,
    c.first_name || ' ' || c.last_name AS customer_name,
    COALESCE(SUM(o.total_amount), 0.00) AS total_spent,
    CASE 
        WHEN SUM(o.total_amount) >= 1000.00 THEN 'Platinum'
        WHEN SUM(o.total_amount) >= 500.00  THEN 'Gold'
        ELSE 'Silver'
    END AS spending_tier
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id AND o.order_status = 'completed'
GROUP BY c.customer_id, c.first_name, c.last_name
ORDER BY total_spent DESC;
```
</details>

#### Q6: How do you safely compute profit margin percentage `((revenue - cost) / revenue) * 100` returning `0.00` if revenue is zero or cost is NULL?
<details>
<summary>Answer</summary>

```sql
SELECT 
    product_id,
    COALESCE(
        ROUND(((revenue - COALESCE(cost, 0)) / NULLIF(revenue, 0)) * 100.0, 2),
        0.00
    ) AS profit_margin_pct
FROM sales_records;
```
</details>

#### Q7: Write an SQL query that pivots quarterly sales data from rows into four separate columns (`Q1_Sales`, `Q2_Sales`, `Q3_Sales`, `Q4_Sales`) for each salesperson.
<details>
<summary>Answer</summary>

```sql
SELECT 
    salesperson_id,
    SUM(CASE WHEN EXTRACT(QUARTER FROM sale_date) = 1 THEN amount ELSE 0 END) AS q1_sales,
    SUM(CASE WHEN EXTRACT(QUARTER FROM sale_date) = 2 THEN amount ELSE 0 END) AS q2_sales,
    SUM(CASE WHEN EXTRACT(QUARTER FROM sale_date) = 3 THEN amount ELSE 0 END) AS q3_sales,
    SUM(CASE WHEN EXTRACT(QUARTER FROM sale_date) = 4 THEN amount ELSE 0 END) AS q4_sales
FROM sales
GROUP BY salesperson_id
ORDER BY salesperson_id;
```
</details>

#### Q8: Write an update statement that gives a 10% raise to employees in Department 1, a 5% raise to Department 2, and a 2% raise to all other departments in a single atomic statement.
<details>
<summary>Answer</summary>

```sql
UPDATE employees
SET salary = salary * CASE department_id
    WHEN 1 THEN 1.10
    WHEN 2 THEN 1.05
    ELSE 1.02
END;
```
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Stock Status Categorization
Write a query classifying products into `'High Stock'` ($> 30$), `'Moderate Stock'` ($10–30$), `'Low Stock'` ($1–9$), or `'Out of Stock'` ($0$).
<details>
<summary>Solution</summary>

```sql
SELECT 
    product_name,
    stock_qty,
    CASE 
        WHEN stock_qty > 30 THEN 'High Stock'
        WHEN stock_qty >= 10 THEN 'Moderate Stock'
        WHEN stock_qty > 0 THEN 'Low Stock'
        ELSE 'Out of Stock'
    END AS stock_status
FROM products
ORDER BY stock_qty DESC;
```
</details>

#### Exercise 2: Null Fallback Selection
Select `customer_id` and contact info, returning `phone_number` if available, or `email` if phone is NULL, or `'Unreachable'` if both are NULL.
<details>
<summary>Solution</summary>

```sql
SELECT 
    customer_id,
    COALESCE(phone_number, email, 'Unreachable') AS primary_contact
FROM customers;
```
</details>

### Intermediate Exercises

#### Exercise 3: Zero-Division Defensive Metric
Calculate conversion rate (`orders / visits * 100`) for traffic channels, displaying `0.00` if `visits` is 0 or NULL.
<details>
<summary>Solution</summary>

```sql
SELECT 
    channel_name,
    COALESCE(ROUND((orders::numeric / NULLIF(visits, 0)) * 100.0, 2), 0.00) AS conversion_rate_pct
FROM web_traffic;
```
</details>

#### Exercise 4: Conditional Aggregation Filter
Count how many products cost more than $500 vs $500 or less in each category.
<details>
<summary>Solution</summary>

```sql
SELECT 
    category_id,
    COUNT(CASE WHEN retail_price > 500.00 THEN 1 END) AS premium_count,
    COUNT(CASE WHEN retail_price <= 500.00 THEN 1 END) AS standard_count
FROM products
GROUP BY category_id;
```
</details>

### Advanced Exercises

#### Exercise 5: Effective Date Range Bounding via GREATEST/LEAST
Given an employee contract with `contract_start` and `contract_end`, and a reporting window `window_start` and `window_end`, compute the effective overlapping start and end dates.
<details>
<summary>Solution</summary>

```sql
SELECT 
    employee_id,
    GREATEST(contract_start, '2024-01-01'::date) AS effective_start,
    LEAST(contract_end, '2024-12-31'::date) AS effective_end
FROM contracts
WHERE contract_start <= '2024-12-31'::date 
  AND contract_end >= '2024-01-01'::date;
```
</details>

#### Exercise 6: State Machine Validation in UPDATE
Write an `UPDATE` statement that transitions ticket status from `'open'` to `'in_progress'`, or `'in_progress'` to `'resolved'`, but throws an error or aborts if the ticket is already `'closed'`.
<details>
<summary>Solution</summary>

```sql
UPDATE support_tickets
SET status = CASE status
    WHEN 'open' THEN 'in_progress'
    WHEN 'in_progress' THEN 'resolved'
    ELSE status -- Unchanged
END
WHERE ticket_id = 42 AND status <> 'closed';
```
</details>

---

## 12. Summary & Quick Reference

### Conditional Expressions Summary
- **Searched CASE**: `CASE WHEN cond1 THEN res1 WHEN cond2 THEN res2 ELSE res3 END`.
- **Short-Circuiting**: Guaranteed top-to-bottom evaluation; stops on first `TRUE`.
- **Type Rule**: All result branches must evaluate to a mutually compatible data type.
- **COALESCE**: `COALESCE(v1, v2, ...)` returns the first non-null argument.
- **NULLIF**: `NULLIF(a, b)` returns `NULL` if $a = b$, otherwise returns $a$.
- **Conditional Aggregation**: `COUNT(CASE WHEN cond THEN 1 END)` or `SUM(CASE WHEN cond THEN amt ELSE 0 END)`.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [Conditional Expressions](https://www.postgresql.org/docs/17/functions-conditional.html)
- SQLite Documentation: [Conditional Expressions](https://www.sqlite.org/lang_expr.html#case_expressions)
- Modern SQL: [CASE Expressions and Conditional Aggregation](https://modern-sql.com/feature/case)
- [14 - Data Modification: INSERT, UPDATE, DELETE & RETURNING](./14-insert-update-delete.md)
