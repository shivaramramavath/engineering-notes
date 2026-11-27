# 05: Sample Databases Used in This Track

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [04 - Clients & Developer Tools](./04-clients-and-tools.md) |
| **Target Audience** | Software Engineers, Data Analysts, Database Architects |
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

Learning SQL without realistic, richly populated sample databases is like learning surgery without anatomical models. Trivial synthetic tables with three rows (`id`, `name`, `age`) fail to reveal the complexities of real-world database engineering: multi-table joins, outer join null-padding, recursive hierarchical graphs, transactional race conditions, index selectivity, and analytical window framing.

Throughout this curriculum, we use four standard, production-modeled reference schemas:
1. **Retail E-Commerce**: Multi-tier order processing, inventory reservations, customer profiles, product catalogs, and categorical hierarchies.
2. **Pagila (DVD / Media Rental)**: The industry-standard relational benchmark schema featuring actors, films, categories, film-actor junction tables, inventory, rentals, and payments.
3. **HR & Payroll Analytics**: Organizational hierarchies, department managers, temporal salary history, job titles, and compensation bands.
4. **Double-Entry Financial Ledger**: Strict accounting balances, immutable journal entries, balanced debit/credit postings, and audit trails.

Understanding these schemas, their entity relationships, and their data constraints provides the common vocabulary for all subsequent query examples, performance tuning benchmarks, and project capstones.

---

## 2. Core Concepts

### Entity-Relationship Architecture Overview

```
                      +-------------------+
                      |    categories     |
                      +-------------------+
                                | 1
                                |
                                | N
+-------------------+ 1       N +-------------------+
|     customers     |--------<|      orders       |
+-------------------+         +-------------------+
                                | 1
                                |
                                | N
+-------------------+ 1       N +-------------------+
|     products      |--------<|    order_items    |
+-------------------+         +-------------------+
```

### 1. The Retail E-Commerce Schema
- `categories`: Hierarchical product taxonomy (self-referencing `parent_id`).
- `products`: Catalog items with stock quantities, retail prices, cost prices, and category references.
- `customers`: User accounts with registration timestamps, email addresses, and geographical regions.
- `orders`: Transactional headers with order status (`pending`, `completed`, `cancelled`), timestamps, and total amounts.
- `order_items`: Order line items with historical snapshot prices, quantities, and item-level discounts.

### 2. The Pagila / Media Rental Schema
- `film`: 1,000 films with rental rates, replacement costs, ratings (`G`, `PG`, `PG-13`, `R`, `NC-17`), and length.
- `actor`: Actors starring in films.
- `film_actor`: Many-to-many junction relation linking actors to films.
- `inventory`: Physical copies of films stocked across store locations.
- `rental`: Rental checkouts and returns with timestamps.
- `payment`: Monetary customer transactions linked to rentals.

### 3. The HR & Payroll Schema
- `departments`: Functional organizational business units.
- `employees`: Core employee records with hire dates, birth dates, and managers.
- `dept_emp`: Historical department assignments with date ranges (`from_date`, `to_date`).
- `dept_manager`: Department leadership assignments.
- `salaries`: Temporal compensation records tracking raises over time.

### 4. The Double-Entry Financial Ledger Schema
- `accounts`: Assets, Liabilities, Equity, Revenue, and Expense balance sheet accounts.
- `journal_entries`: Immutable accounting transaction headers.
- `postings`: Balanced lines; the sum of debits must equal the sum of credits per entry.

---

## 3. How It Works Internally

### Referential Integrity Invariants
The reference schemas enforce relational invariants using database constraints:
- **Foreign Key Cascading Rules**:
  - `ON DELETE RESTRICT` (Default): Prevents deleting a parent category or product if active order items reference it.
  - `ON DELETE CASCADE`: Deleting a draft order automatically cascades to delete all child `order_items`.
- **Check Constraints for Domain Integrity**:
  - `retail_price >= 0.00`
  - `quantity > 0`
  - `discount BETWEEN 0.00 AND 1.00`

---

## 4. Syntax & Usage

### Complete DDL: Retail E-Commerce Reference Schema

```sql
-- Category Hierarchy
CREATE TABLE categories (
    category_id INTEGER PRIMARY KEY,
    category_name VARCHAR(100) NOT NULL UNIQUE,
    parent_id INTEGER REFERENCES categories(category_id)
);

-- Customers
CREATE TABLE customers (
    customer_id INTEGER PRIMARY KEY,
    first_name VARCHAR(50) NOT NULL,
    last_name VARCHAR(50) NOT NULL,
    email VARCHAR(100) NOT NULL UNIQUE,
    country VARCHAR(50) NOT NULL DEFAULT 'USA',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL
);

-- Products
CREATE TABLE products (
    product_id INTEGER PRIMARY KEY,
    category_id INTEGER NOT NULL REFERENCES categories(category_id),
    product_name VARCHAR(150) NOT NULL,
    retail_price NUMERIC(10, 2) NOT NULL CHECK (retail_price >= 0.00),
    stock_qty INTEGER NOT NULL DEFAULT 0 CHECK (stock_qty >= 0)
);

-- Orders
CREATE TABLE orders (
    order_id INTEGER PRIMARY KEY,
    customer_id INTEGER NOT NULL REFERENCES customers(customer_id),
    order_status VARCHAR(20) NOT NULL CHECK (order_status IN ('pending', 'processing', 'completed', 'cancelled')),
    order_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL,
    total_amount NUMERIC(12, 2) NOT NULL DEFAULT 0.00
);

-- Order Items (Line Items)
CREATE TABLE order_items (
    item_id INTEGER PRIMARY KEY,
    order_id INTEGER NOT NULL REFERENCES orders(order_id) ON DELETE CASCADE,
    product_id INTEGER NOT NULL REFERENCES products(product_id),
    quantity INTEGER NOT NULL CHECK (quantity > 0),
    unit_price NUMERIC(10, 2) NOT NULL CHECK (unit_price >= 0.00)
);
```

### Multi-Dialect Comparison Matrix

| DDL Construct | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Identity Generator** | `GENERATED ALWAYS AS IDENTITY` | `AUTO_INCREMENT` | `INTEGER PRIMARY KEY` | `IDENTITY(1,1)` |
| **Check Constraints** | Enforced | Enforced | Enforced | Enforced |
| **Foreign Key Enforcement** | Enforced by default | Enforced (InnoDB) | Requires `PRAGMA foreign_keys = ON;` | Enforced |
| **Boolean Enum Emulation** | Native `BOOLEAN` or `CHECK` | `ENUM(...)` or `CHECK` | `CHECK (col IN (0, 1))` | `BIT` or `CHECK` |

---

## 5. 5 Practical Progressive Examples

### Example 1: Seeding Deterministic Baseline Data
Let's populate the retail database with realistic data to serve as our verifiable testbed.

```sql
-- Seed Categories
INSERT INTO categories (category_id, category_name, parent_id) VALUES
(1, 'Electronics', NULL),
(2, 'Computers', 1),
(3, 'Smartphones', 1),
(4, 'Audio', 1),
(5, 'Books', NULL);

-- Seed Customers
INSERT INTO customers (customer_id, first_name, last_name, email, country, created_at) VALUES
(101, 'Alex', 'Morgan', 'alex.m@example.com', 'USA', '2024-01-15 10:00:00'),
(102, 'Priya', 'Sharma', 'priya.s@example.com', 'India', '2024-02-01 11:30:00'),
(103, 'David', 'Kim', 'david.k@example.com', 'Canada', '2024-02-10 14:15:00'),
(104, 'Maria', 'Garcia', 'maria.g@example.com', 'Spain', '2024-03-05 09:45:00'),
(105, 'Liam', 'OConnor', 'liam.o@example.com', 'Ireland', '2024-03-20 16:20:00');

-- Seed Products
INSERT INTO products (product_id, category_id, product_name, retail_price, stock_qty) VALUES
(201, 2, 'Framework Laptop 16', 1399.00, 25),
(202, 2, 'MacBook Air M3', 1099.00, 40),
(203, 3, 'Pixel 9 Pro', 999.00, 50),
(204, 4, 'Sony WH-1000XM5', 399.00, 75),
(205, 5, 'Designing Data-Intensive Applications', 45.00, 120);

-- Seed Orders
INSERT INTO orders (order_id, customer_id, order_status, order_date, total_amount) VALUES
(1001, 101, 'completed', '2024-04-01 10:15:00', 1444.00),
(1002, 102, 'completed', '2024-04-02 12:00:00', 1398.00),
(1003, 101, 'completed', '2024-04-05 15:30:00', 45.00),
(1004, 103, 'pending', '2024-04-10 09:00:00', 1099.00),
(1005, 104, 'cancelled', '2024-04-11 14:00:00', 399.00);

-- Seed Order Items
INSERT INTO order_items (item_id, order_id, product_id, quantity, unit_price) VALUES
(1, 1001, 201, 1, 1399.00),
(2, 1001, 205, 1, 45.00),
(3, 1002, 203, 1, 999.00),
(4, 1002, 204, 1, 399.00),
(5, 1003, 205, 1, 45.00),
(6, 1004, 202, 1, 1099.00),
(7, 1005, 204, 1, 399.00);
```

### Example 2: Multi-Table Relational Join
Verify customer order history combining 4 tables: `customers`, `orders`, `order_items`, and `products`.

```sql
SELECT 
    c.first_name || ' ' || c.last_name AS customer_name,
    o.order_id,
    o.order_date,
    p.product_name,
    oi.quantity,
    oi.unit_price,
    (oi.quantity * oi.unit_price) AS line_total
FROM customers c
JOIN orders o ON c.customer_id = o.customer_id
JOIN order_items oi ON o.order_id = oi.order_id
JOIN products p ON oi.product_id = p.product_id
WHERE o.order_status = 'completed'
ORDER BY o.order_id, oi.item_id;
```
```text
+---------------+----------+---------------------+---------------------------------------+----------+------------+------------+
| customer_name | order_id | order_date          | product_name                          | quantity | unit_price | line_total |
+---------------+----------+---------------------+---------------------------------------+----------+------------+------------+
| Alex Morgan   |     1001 | 2024-04-01 10:15:00 | Framework Laptop 16                   |        1 |    1399.00 |    1399.00 |
| Alex Morgan   |     1001 | 2024-04-01 10:15:00 | Designing Data-Intensive Applications |        1 |      45.00 |      45.00 |
| Priya Sharma  |     1002 | 2024-04-02 12:00:00 | Pixel 9 Pro                           |        1 |     999.00 |     999.00 |
| Priya Sharma  |     1002 | 2024-04-02 12:00:00 | Sony WH-1000XM5                       |        1 |     399.00 |     399.00 |
| Alex Morgan   |     1003 | 2024-04-05 15:30:00 | Designing Data-Intensive Applications |        1 |      45.00 |      45.00 |
+---------------+----------+---------------------+---------------------------------------+----------+------------+------------+
```

### Example 3: Edge Case (Outer Join to Detect Non-Purchasing Customers)
Detect registered customers who have never completed an order.

```sql
SELECT 
    c.customer_id,
    c.first_name || ' ' || c.last_name AS full_name,
    c.country,
    COUNT(o.order_id) AS orders_placed
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id AND o.order_status = 'completed'
GROUP BY c.customer_id, c.first_name, c.last_name, c.country
HAVING COUNT(o.order_id) = 0
ORDER BY c.customer_id;
```
```text
+-------------+---------------+---------+---------------+
| customer_id | full_name     | country | orders_placed |
+-------------+---------------+---------+---------------+
|         103 | David Kim     | Canada  |             0 |
|         104 | Maria Garcia  | Spain   |             0 |
|         105 | Liam OConnor  | Ireland |             0 |
+-------------+---------------+---------+---------------+
```
*(Notice David Kim placed an order, but its status is `pending`; Maria Garcia's was `cancelled`; Liam placed none. All correctly show 0 completed orders).*

### Example 4: Failure Mode (Cascading Deletion Accident)
Demonstrating why cascading foreign keys must be configured intentionally:

```sql
-- Because order_items has ON DELETE CASCADE from orders:
DELETE FROM orders WHERE order_id = 1001;

-- Notice what happened to order_items:
SELECT * FROM order_items WHERE order_id = 1001;
```
```text
+---------+----------+------------+----------+------------+
| item_id | order_id | product_id | quantity | unit_price |
+---------+----------+------------+----------+------------+
+---------+----------+------------+----------+------------+
(0 rows returned - child rows deleted automatically!)
```
*(If `order_items` had `ON DELETE RESTRICT`, the engine would have safely rejected the deletion of the order, preventing accidental data loss).*

### Example 5: Optimized Analytical Aggregation (Revenue by Category)
Calculate total revenue and average transaction size by product category hierarchy.

```sql
SELECT 
    cat.category_name,
    COUNT(DISTINCT o.order_id) AS total_orders,
    SUM(oi.quantity * oi.unit_price) AS total_revenue
FROM categories cat
JOIN products p ON cat.category_id = p.category_id
JOIN order_items oi ON p.product_id = oi.product_id
JOIN orders o ON oi.order_id = o.order_id
WHERE o.order_status = 'completed'
GROUP BY cat.category_id, cat.category_name
ORDER BY total_revenue DESC;
```
```text
+---------------+--------------+---------------+
| category_name | total_orders | total_revenue |
+---------------+--------------+---------------+
| Computers     |            1 |       1399.00 |
| Smartphones   |            1 |        999.00 |
| Audio         |            1 |        399.00 |
| Books         |            2 |         90.00 |
+---------------+--------------+---------------+
```

---

## 6. Best Practices

1. **Snapshot Historical Values in Junction Tables**: Notice that `order_items` includes `unit_price`. Never rely on `products.retail_price` to calculate historical sales; if a product's price changes tomorrow, historical accounting records would be corrupted!
2. **Use Natural Primary Keys Sparingly**: Primary keys should be immutable integers or time-ordered UUIDv7s. Natural keys like emails or phone numbers change and cause heavy foreign key update cascades.
3. **Always Add Index on Foreign Keys**: Foreign keys in child tables (`order_items.order_id`, `orders.customer_id`) should always have explicit B-Tree indexes created to optimize join traversals and delete checks.
4. **Enforce Referential Integrity with `RESTRICT` for Financial Records**: Avoid `CASCADE` on core business transactions. Deleting a customer should never silently erase their invoice history.
5. **Separate Status from Soft Deletes**: Use domain status columns (`order_status = 'cancelled'`) rather than abusing soft delete flags (`is_deleted`) to represent business lifecycle states.

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Calculating Total from Live Product Prices** | Joining `order_items` to `products` to sum revenue produces inaccurate historical totals when product catalog prices change. | Store the exact executed `unit_price` on `order_items` at transaction time. |
| **Missing Check Constraints on Financial Numbers** | Storing `retail_price = -50.00` or `quantity = -5` breaks business logic and allows fraudulent records. | Add `CHECK (price >= 0.00)` and `CHECK (quantity > 0)`. |
| **Using `FLOAT` / `REAL` for Money** | Floating-point numbers introduce IEEE-754 rounding errors (e.g., `$0.10 + $0.20 = $0.30000000000000004`). | Always use `NUMERIC(p, s)` or `DECIMAL(p, s)` for monetary values. |
| **Assuming Parent Deletes Are Blocked Automatically** | If you accidentally set `ON DELETE CASCADE`, deleting a customer deletes all their orders and audit history without warning. | Use `ON DELETE RESTRICT` or `ON DELETE NO ACTION` for transactional tables. |
| **Inconsistent Case in Status Columns** | Storing `'Completed'`, `'completed'`, and `'COMPLETED'` leads to missing rows in case-sensitive filters. | Normalize statuses with `LOWER()` or enforce standardized enum values. |
| **Not Null Missing on Foreign Keys** | If an `order_items.order_id` is nullable, orphaned items can exist without belonging to any order. | Explicitly mark foreign keys `NOT NULL` unless optionality is required by the domain. |

---

## 8. Performance & Security Considerations

### Performance
- **Foreign Key Indexing**: In PostgreSQL, creating a foreign key does NOT automatically index the child column. Always create explicit indexes:
  ```sql
  CREATE INDEX idx_order_items_order_id ON order_items(order_id);
  CREATE INDEX idx_order_items_product_id ON order_items(product_id);
  CREATE INDEX idx_orders_customer_id ON orders(customer_id);
  ```
- **Join Cardinality Estimates**: The query optimizer relies on accurate table statistics (`ANALYZE`) to choose between Nested Loop and Hash Joins across these tables.

### Security
- **Role-Based Schema Isolation**: In production, application clients should never connect with schema-owning users. Restrict application database users to DML operations (`SELECT`, `INSERT`, `UPDATE`) while denying DDL rights (`DROP`, `ALTER`).

---

## 9. Comparison Matrix

### Reference Schemas Used Across the Track

| Schema Domain | Tables Count | Complexity | Core Topics Tested | Primary Use Case in Curriculum |
|:---|:---:|:---|:---|:---|
| **Retail E-Commerce** | 5 | Moderate | Joins, Aggregation, Subqueries, Constraints | Fundamentals, Querying, Indexing |
| **Pagila (DVD Rental)**| 15 | High | Complex multi-way joins, CTEs, Window functions | Analytical SQL, Deep Querying Labs |
| **HR & Payroll** | 6 | High | Temporal data, hierarchical self-joins, ranges | Window functions, Advanced CTEs |
| **Financial Ledger** | 3 | Strict | ACID, Concurrency, Locking, Idempotency | Transactions, MVCC, Real-World Patterns |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: Why must an e-commerce schema snapshot product prices in `order_items` rather than reading them from `products`?
<details>
<summary>Answer</summary>
In relational modeling, an order is an immutable legal and financial record of a historical transaction. 
If an e-commerce platform merely joins `order_items` to `products` to calculate historical totals, any future price increase or decrease in the `products` catalog will retroactively alter the historical revenue, customer receipts, tax filings, and ledger balances. Storing `unit_price` directly on `order_items` preserves the historical fact at transaction time.
</details>

#### Q2: What is the difference between `ON DELETE RESTRICT` and `ON DELETE NO ACTION` in standard SQL?
<details>
<summary>Answer</summary>
While both prevent the deletion of a parent row if referencing child rows exist, they differ in *when* the check is evaluated:
- `RESTRICT`: Evaluates immediately when the `DELETE` statement executes. It cannot be deferred.
- `NO ACTION`: Evaluates at the end of the statement or, if configured as `DEFERRABLE INITIALLY DEFERRED`, at transaction `COMMIT` time. This allows intermediate statements within the transaction to create new matching parents or clean up children before the check runs.
</details>

#### Q3: How do you model a hierarchical product category taxonomy with arbitrary depth in a single relational table?
<details>
<summary>Answer</summary>
Using an Adjacency List pattern: a self-referencing foreign key on the `categories` table:
```sql
CREATE TABLE categories (
    category_id INT PRIMARY KEY,
    category_name VARCHAR(100) NOT NULL,
    parent_id INT REFERENCES categories(category_id)
);
```
Root categories have `parent_id IS NULL`. Subcategories point to their immediate parent. Traversal of arbitrary depths is performed using Recursive Common Table Expressions (`WITH RECURSIVE`).
</details>

#### Q4: Why should financial amounts never be stored using `FLOAT` or `DOUBLE PRECISION` data types?
<details>
<summary>Answer</summary>
`FLOAT` and `DOUBLE` use binary floating-point representation (IEEE 754), which cannot represent base-10 fractions (like 0.10 or 0.05) exactly. Arithmetic operations accumulate rounding errors, leading to balancing discrepancies in accounting and reconciliation. 

Financial amounts must always use exact arbitrary-precision numeric types: `NUMERIC(precision, scale)` or `DECIMAL(precision, scale)` (e.g., `NUMERIC(15, 2)`).
</details>

### Practical Query Writing Puzzles

#### Q5: Write a query to find the top 2 highest revenue products within each category using window functions.
<details>
<summary>Answer</summary>

```sql
WITH ProductRevenue AS (
    SELECT 
        p.category_id,
        p.product_id,
        p.product_name,
        COALESCE(SUM(oi.quantity * oi.unit_price), 0) AS total_revenue,
        DENSE_RANK() OVER (
            PARTITION BY p.category_id 
            ORDER BY COALESCE(SUM(oi.quantity * oi.unit_price), 0) DESC
        ) AS rank_in_cat
    FROM products p
    LEFT JOIN order_items oi ON p.product_id = oi.product_id
    GROUP BY p.category_id, p.product_id, p.product_name
)
SELECT category_id, product_name, total_revenue, rank_in_cat
FROM ProductRevenue
WHERE rank_in_cat <= 2
ORDER BY category_id, rank_in_cat;
```
</details>

#### Q6: Detect any orders whose recorded `total_amount` does not match the sum of their `order_items` line totals.
<details>
<summary>Answer</summary>

```sql
SELECT 
    o.order_id,
    o.total_amount AS recorded_total,
    SUM(oi.quantity * oi.unit_price) AS calculated_items_total,
    o.total_amount - SUM(oi.quantity * oi.unit_price) AS discrepancy
FROM orders o
JOIN order_items oi ON o.order_id = oi.order_id
GROUP BY o.order_id, o.total_amount
HAVING o.total_amount <> SUM(oi.quantity * oi.unit_price);
```
</details>

#### Q7: Write a query using `categories` to generate the complete breadcrumb path for each subcategory (e.g., "Electronics > Computers > Laptops").
<details>
<summary>Answer</summary>

```sql
WITH RECURSIVE CategoryPath AS (
    -- Anchor: root categories
    SELECT 
        category_id, 
        category_name, 
        CAST(category_name AS TEXT) AS breadcrumb
    FROM categories
    WHERE parent_id IS NULL

    UNION ALL

    -- Recursive member: child categories
    SELECT 
        c.category_id, 
        c.category_name, 
        cp.breadcrumb || ' > ' || c.category_name
    FROM categories c
    JOIN CategoryPath cp ON c.parent_id = cp.category_id
)
SELECT category_id, breadcrumb
FROM CategoryPath
ORDER BY breadcrumb;
```
</details>

#### Q8: Write an atomic SQL statement that cancels an order and restores the reserved product stock quantities.
<details>
<summary>Answer</summary>

```sql
BEGIN;

-- 1. Update order status
UPDATE orders 
SET order_status = 'cancelled' 
WHERE order_id = 1004 AND order_status = 'pending';

-- 2. Restore inventory quantities
UPDATE products p
SET stock_qty = p.stock_qty + oi.quantity
FROM order_items oi
WHERE oi.order_id = 1004 AND p.product_id = oi.product_id;

COMMIT;
```
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Finding Out-of-Stock Products
Retrieve all products where `stock_qty` is less than 10, showing `product_name`, `stock_qty`, and `retail_price`, sorted by `stock_qty` ascending.
<details>
<summary>Solution</summary>

```sql
SELECT product_name, stock_qty, retail_price
FROM products
WHERE stock_qty < 10
ORDER BY stock_qty ASC;
```
</details>

#### Exercise 2: Customer Order Count Summary
Write a query displaying `customer_id`, `email`, and the total count of orders placed (including customers who have placed 0 orders).
<details>
<summary>Solution</summary>

```sql
SELECT 
    c.customer_id, 
    c.email, 
    COUNT(o.order_id) AS total_orders
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.email
ORDER BY total_orders DESC;
```
</details>

### Intermediate Exercises

#### Exercise 3: Average Order Value (AOV) by Country
Calculate the Average Order Value (AOV) for completed orders, grouped by customer `country`. Only include countries with at least 2 completed orders.
<details>
<summary>Solution</summary>

```sql
SELECT 
    c.country,
    COUNT(o.order_id) AS completed_orders,
    ROUND(AVG(o.total_amount), 2) AS average_order_value
FROM customers c
JOIN orders o ON c.customer_id = o.customer_id
WHERE o.order_status = 'completed'
GROUP BY c.country
HAVING COUNT(o.order_id) >= 2
ORDER BY average_order_value DESC;
```
</details>

#### Exercise 4: Products Never Ordered
Find all products that have never appeared in any order item, without using subqueries.
<details>
<summary>Solution</summary>

```sql
SELECT p.product_id, p.product_name, p.retail_price
FROM products p
LEFT JOIN order_items oi ON p.product_id = oi.product_id
WHERE oi.item_id IS NULL;
```
</details>

### Advanced Exercises

#### Exercise 5: Customer Retention / Repeat Purchase Rate
Calculate the percentage of customers who have placed more than one completed order.
<details>
<summary>Solution</summary>

```sql
WITH CustomerOrderCounts AS (
    SELECT 
        c.customer_id,
        COUNT(o.order_id) AS order_count
    FROM customers c
    LEFT JOIN orders o ON c.customer_id = o.customer_id AND o.order_status = 'completed'
    GROUP BY c.customer_id
)
SELECT 
    COUNT(*) AS total_customers,
    COUNT(CASE WHEN order_count > 1 THEN 1 END) AS repeat_customers,
    ROUND(
        (COUNT(CASE WHEN order_count > 1 THEN 1 END)::numeric / NULLIF(COUNT(*), 0)) * 100.0, 
        2
    ) AS repeat_buyer_percentage
FROM CustomerOrderCounts;
```
</details>

#### Exercise 6: Double-Entry Ledger Invariant Verification
Given a `postings(posting_id, entry_id, account_id, debit_amount, credit_amount)` table, write a query that returns any `entry_id` where total debits do not equal total credits.
<details>
<summary>Solution</summary>

```sql
SELECT 
    entry_id,
    SUM(debit_amount) AS total_debits,
    SUM(credit_amount) AS total_credits,
    SUM(debit_amount) - SUM(credit_amount) AS imbalance
FROM postings
GROUP BY entry_id
HAVING SUM(debit_amount) <> SUM(credit_amount);
```
*(In a valid double-entry system, this query must always return zero rows).*
</details>

---

## 12. Summary & Quick Reference

### Core Schema Entity Summary
- **Customers**: Root entity for accounts and geographical telemetry.
- **Orders & Order Items**: Classical Master-Detail (Header-Line) relational design pattern.
- **Products & Categories**: Catalog taxonomy with self-referential adjacency hierarchy.
- **Financial Ledger**: Exact-balance double-entry posting ensuring systemic accounting integrity.

---

## 13. Further Reading
- PostgreSQL Sample Databases: [Pagila Database GitHub](https://github.com/devrimgunduz/pagila)
- Martin Fowler: *"Accounting Patterns"* (Analysis Patterns for Double-Entry Ledgers)
- Ralph Kimball: *"The Data Warehouse Toolkit"* (Dimensional Modeling)
- [06 - SQL Standard & How to Read the Docs](./06-sql-standard-and-how-to-read-the-docs.md)
