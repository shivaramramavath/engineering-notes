# Recursive CTEs: Hierarchies, Graphs, and Cycle Detection

| Metadata | Details |
| :--- | :--- |
| **Topic** | Recursive Common Table Expressions (WITH RECURSIVE), Trees, Graphs, Cycle Detection |
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

Relational tables naturally model flat, two-dimensional structures. However, real-world data is inherently hierarchical, nested, or graph-structured:
- Corporate organizational charts (CEOs, managers, individual contributors).
- Bill of Materials (BOM) in manufacturing (a vehicle contains assemblies, which contain sub-assemblies, which contain individual bolts and nuts).
- File systems (nested folders and files).
- Social networks (friends of friends, degrees of separation).
- Logistics and routing networks (flight connections, road networks).

Before recursive CTEs, traversing structures of unknown depth required either recursive application code issuing dozens of queries (the $N+1$ query problem), denormalized nested-set / closure-table schemas, or vendor-specific proprietary extensions (such as Oracle's `CONNECT BY`).

Introduced in ANSI SQL:1999, **Recursive Common Table Expressions** (`WITH RECURSIVE`) enable the relational engine to perform iterative fixed-point traversal natively within a single SQL statement. Understanding recursive CTEs—along with loop termination and cycle detection—is essential for data engineers and backend architects.

---

## Core Concepts

### 1. Anatomy of a Recursive CTE
A recursive CTE consists of three core components joined by `UNION ALL` (or `UNION`):
1. **The Anchor Member**: A non-recursive query that selects the base rows (e.g., the root of the tree, depth 0). It executes exactly once.
2. **The Recursive Member**: A query that references the CTE itself, joining the results of the previous iteration to the base table to discover the next level of children.
3. **The Termination Condition**: An implicit or explicit `WHERE` clause in the recursive member. When an iteration produces 0 new rows, the engine halts.

```sql
WITH RECURSIVE org_chart AS (
    -- 1. Anchor Member (Root CEO)
    SELECT emp_id, emp_name, manager_id, 1 AS depth
    FROM employees
    WHERE manager_id IS NULL

    UNION ALL

    -- 2. Recursive Member (Subordinates)
    SELECT e.emp_id, e.emp_name, e.manager_id, oc.depth + 1
    FROM employees e
    JOIN org_chart oc ON e.manager_id = oc.emp_id
)
SELECT * FROM org_chart;
```

### 2. `UNION ALL` vs `UNION`
- **`UNION ALL`** (Standard & Fast): Appends rows produced by each iteration without checking for duplicates. Recommended for trees and directed acyclic graphs (DAGs).
- **`UNION`** (Deduplicating): Deduplicates rows between iterations. Slower because it maintains an internal hash or sort set, but acts as a safeguard against cycles in simple graphs.

### 3. Path Tracking and Depth
To visualize hierarchies and prevent cycles, recursive queries typically maintain two synthetic columns:
- **`depth`**: An integer incremented by 1 at each level (`depth + 1`).
- **`path`**: An array or concatenated string tracking visited nodes:
  - PostgreSQL Array: `ARRAY[emp_id]`, extended with `path || e.emp_id`.
  - String Path: `'/' || emp_id`, extended with `path || '/' || e.emp_id`.

### 4. Cycle Detection in Graphs
In cyclic graphs (where node A connects to node B, and node B connects back to node A), a recursive query will loop indefinitely until it exhausts memory or hits database recursion limits.
- **ANSI SQL:2016 / PostgreSQL 14+ `CYCLE` Clause**:
  ```sql
  CYCLE emp_id SET is_cycle USING path
  ```
  Automatically halts traversal when a duplicate ID is encountered along the current path.
- **Array Search Check**:
  ```sql
  WHERE NOT (e.emp_id = ANY(oc.path))
  ```

---

## How It Works Internally

The SQL execution engine implements recursive CTEs using an **iterative fixed-point algorithm** operating on three internal temporary buffers:

```
                            [ 1. Execute Anchor Query ]
                                         │
                                         ▼
                            [ Load into Working Table ]
                                         │
                 ┌───────────────────────┴───────────────────────┐
                 │                                               │
                 ▼                                               ▼
     [ Append to Result Set ]                         [ Is Working Table Empty? ]
                                                                 │
                                                    ┌────────────┴────────────┐
                                                    ▼                         ▼
                                                 [ YES ]                   [ NO ]
                                                    │                         │
                                                 TERMINATE                    │
                                                                              ▼
                                                                [ Execute Recursive Member ]
                                                                (Joins Base Table with Working Table)
                                                                              │
                                                                              ▼
                                                                [ Load Matches into Intermediate Table ]
                                                                              │
                                                                              ▼
                                                                [ Swap Intermediate -> Working Table ]
                                                                (Repeat Loop)
```

1. **Step 1**: The engine executes the Anchor Query. Its output is written to the cumulative **Result Table** and copied into the **Working Table** (the delta queue).
2. **Step 2**: The engine checks the Working Table. If it is empty, execution terminates.
3. **Step 3**: The engine evaluates the Recursive Member, replacing the CTE reference with the current contents of the Working Table.
4. **Step 4**: The matches are collected in the **Intermediate Table** and appended to the cumulative Result Table.
5. **Step 5**: The Working Table is cleared, the Intermediate Table is moved into the Working Table, and the engine loops back to Step 2.

---

## Syntax & Usage

```sql
WITH RECURSIVE category_tree AS (
    -- Anchor: Top-level root categories
    SELECT 
        category_id, 
        category_name, 
        parent_id,
        1 AS level,
        category_name::text AS breadcrumb_path
    FROM categories
    WHERE parent_id IS NULL

    UNION ALL

    -- Recursive Step: Child categories
    SELECT 
        c.category_id, 
        c.category_name, 
        c.parent_id,
        ct.level + 1,
        ct.breadcrumb_path || ' > ' || c.category_name
    FROM categories c
    JOIN category_tree ct ON c.parent_id = ct.category_id
)
SELECT * FROM category_tree ORDER BY breadcrumb_path;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **`WITH RECURSIVE` Syntax** | Requires `RECURSIVE` keyword | Requires `RECURSIVE` keyword | Requires `RECURSIVE` keyword | Disallows `RECURSIVE`! Uses plain `WITH cte AS` |
| **Native `CYCLE` Clause** | Fully supported (PostgreSQL 14+) | **Not Supported** | **Not Supported** | **Not Supported** |
| **Array-Based Path Tracking** | Native: `ARRAY[id]` & `= ANY(path)` | Emulated via `CONCAT(',', id, ',')` | Emulated via string concatenation | Emulated via string concatenation |
| **Recursion Depth Safeguard** | `statement_timeout` | `cte_max_recursion_depth` (Default 1000) | `sqlite3_limit(SQLITE_LIMIT_RECURSIVE_DEPTH)` | `OPTION (MAXRECURSION 100)` (Default 100) |

---

## Practical Examples

### Example 1: Basic — Generating a Number Series (1 to 5)
Generate a synthetic sequence of integers without physical tables.

```sql
WITH RECURSIVE numbers AS (
    SELECT 1 AS n -- Anchor
    UNION ALL
    SELECT n + 1 FROM numbers WHERE n < 5 -- Recursive member + termination
)
SELECT n FROM numbers;
```

```
Result:
 n 
---
 1
 2
 3
 4
 5
(5 rows)
```

---

### Example 2: Real-World — Organizational Hierarchy with Breadcrumb Paths
Traverse a corporate management hierarchy to display reporting lines and depth.

```sql
CREATE TABLE company_org (
    emp_id INT PRIMARY KEY,
    emp_name VARCHAR(50) NOT NULL,
    manager_id INT NULL,
    title VARCHAR(50) NOT NULL
);

INSERT INTO company_org VALUES
(1, 'Sarah Chen', NULL, 'CEO'),
(2, 'Marcus Vance', 1, 'VP Engineering'),
(3, 'Elena Rostova', 1, 'VP Sales'),
(4, 'Liam O Connor', 2, 'Principal Architect'),
(5, 'Zoe Washington', 2, 'Engineering Manager'),
(6, 'David Kim', 5, 'Senior Software Engineer');

WITH RECURSIVE org_hierarchy AS (
    SELECT 
        emp_id, 
        emp_name, 
        manager_id, 
        title, 
        0 AS depth,
        emp_name::text AS path
    FROM company_org
    WHERE manager_id IS NULL

    UNION ALL

    SELECT 
        e.emp_id, 
        e.emp_name, 
        e.manager_id, 
        e.title, 
        oh.depth + 1,
        oh.path || ' -> ' || e.emp_name
    FROM company_org e
    JOIN org_hierarchy oh ON e.manager_id = oh.emp_id
)
SELECT depth, emp_name, title, path
FROM org_hierarchy
ORDER BY path;
```

```
Result:
 depth |    emp_name    |          title           |                            path                            
-------+----------------+--------------------------+------------------------------------------------------------
     0 | Sarah Chen     | CEO                      | Sarah Chen
     1 | Elena Rostova  | VP Sales                 | Sarah Chen -> Elena Rostova
     1 | Marcus Vance   | VP Engineering           | Sarah Chen -> Marcus Vance
     2 | Liam O Connor  | Principal Architect      | Sarah Chen -> Marcus Vance -> Liam O Connor
     2 | Zoe Washington | Engineering Manager      | Sarah Chen -> Marcus Vance -> Zoe Washington
     3 | David Kim      | Senior Software Engineer | Sarah Chen -> Marcus Vance -> Zoe Washington -> David Kim
(6 rows)
```

---

### Example 3: Edge Case — Bill of Materials (BOM) Quantity Rollup
Calculate the total quantity of raw components needed to manufacture 1 completed bicycle.

```sql
CREATE TABLE bill_of_materials (
    parent_part_id INT NULL,
    child_part_id INT NOT NULL,
    quantity_needed INT NOT NULL
);

-- Bike (100) needs 2 Wheels (200) and 1 Frame (300).
-- Each Wheel (200) needs 32 Spokes (400) and 1 Tire (500).
INSERT INTO bill_of_materials VALUES
(NULL, 100, 1),
(100, 200, 2),
(100, 300, 1),
(200, 400, 32),
(200, 500, 1);

WITH RECURSIVE bom_calc AS (
    SELECT 
        child_part_id, 
        quantity_needed AS total_qty
    FROM bill_of_materials
    WHERE parent_part_id IS NULL

    UNION ALL

    SELECT 
        b.child_part_id, 
        bc.total_qty * b.quantity_needed
    FROM bill_of_materials b
    JOIN bom_calc bc ON b.parent_part_id = bc.child_part_id
)
SELECT child_part_id, SUM(total_qty) AS final_quantity_required
FROM bom_calc
GROUP BY child_part_id
ORDER BY child_part_id;
```

```
Result:
 child_part_id | final_quantity_required 
---------------+-------------------------
           100 |                       1
           200 |                       2
           300 |                       1
           400 |                      64  (2 wheels * 32 spokes)
           500 |                       2  (2 wheels * 1 tire)
(5 rows)
```

---

### Example 4: Failure Mode — Infinite Loop via Graph Cycle
Demonstrating how a cyclic relationship causes an infinite recursion crash without cycle detection.

```sql
CREATE TABLE flight_routes (
    departure_city VARCHAR(10) NOT NULL,
    arrival_city VARCHAR(10) NOT NULL
);

INSERT INTO flight_routes VALUES
('JFK', 'LHR'),
('LHR', 'CDG'),
('CDG', 'JFK'); -- Cycle back to JFK!

-- DANGEROUS QUERY: Infinite loop!
-- Runs until memory is exhausted or max recursion is hit.
-- WITH RECURSIVE routes AS (...)

-- SAFE IMPLEMENTATION with Array Path Cycle Detection:
WITH RECURSIVE safe_routes AS (
    SELECT 
        departure_city, 
        arrival_city, 
        1 AS stops, 
        ARRAY[departure_city, arrival_city] AS visited_cities
    FROM flight_routes
    WHERE departure_city = 'JFK'

    UNION ALL

    SELECT 
        f.departure_city, 
        f.arrival_city, 
        sr.stops + 1, 
        sr.visited_cities || f.arrival_city
    FROM flight_routes f
    JOIN safe_routes sr ON f.departure_city = sr.arrival_city
    WHERE NOT (f.arrival_city = ANY(sr.visited_cities)) -- HALTS CYCLE!
)
SELECT departure_city, arrival_city, stops, visited_cities
FROM safe_routes;
```

```
Result:
 departure_city | arrival_city | stops | visited_cities 
----------------+--------------+-------+----------------
 JFK            | LHR          |     1 | {JFK,LHR}
 LHR            | CDG          |     2 | {JFK,LHR,CDG}
(2 rows - stops before repeating JFK!)
```

---

### Example 5: Optimized — Native `CYCLE` Clause in PostgreSQL 14+
Using PostgreSQL's native SQL:2016 `CYCLE` clause for clean, automated cycle handling.

```sql
WITH RECURSIVE safe_routes AS (
    SELECT departure_city, arrival_city, 1 AS stops
    FROM flight_routes
    WHERE departure_city = 'JFK'

    UNION ALL

    SELECT f.departure_city, f.arrival_city, sr.stops + 1
    FROM flight_routes f
    JOIN safe_routes sr ON f.departure_city = sr.arrival_city
)
CYCLE arrival_city SET is_cycle USING path_history
SELECT departure_city, arrival_city, stops, is_cycle, path_history
FROM safe_routes;
```

---

## Best Practices

1. **Always Implement Cycle Protection**: When querying user-generated or graph data, always track visited IDs in an array/string path or use the `CYCLE` clause to prevent infinite loops.
2. **Cap Maximum Recursion Depth**: Add an explicit safeguard `WHERE depth < 50` to guarantee query termination in case of malformed data.
3. **Prefer `UNION ALL` over `UNION`**: Use `UNION ALL` unless duplicate elimination between iterations is explicitly required. `UNION` adds heavy hash/sort overhead at every single iteration.
4. **Index Foreign Keys**: Ensure the join column in the recursive member (e.g., `manager_id` or `parent_id`) has a B-Tree index. This allows the engine to perform index seeks from the Working Table.
5. **Remember SQL Server Keyword Difference**: SQL Server uses `WITH cte AS (...)` without the `RECURSIVE` keyword.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Adding `RECURSIVE` keyword in SQL Server | T-SQL syntax rejects `WITH RECURSIVE`; raises an immediate syntax error. | Use plain `WITH cte AS (...)` in SQL Server. |
| 2 | Forgetting loop termination condition | If the recursive member lacks a halting condition, query enters an infinite loop. | Ensure recursive join eventually matches 0 rows, or add `WHERE depth < max`. |
| 3 | Traversing cyclic data without path tracking | Graph edges that loop back cause stack exhaustion or statement timeouts. | Use `NOT (id = ANY(path))` or the `CYCLE` clause. |
| 4 | Referencing the CTE multiple times in recursive query | ANSI SQL permits referencing the recursive CTE **only once** in the recursive member. | Perform self-joins outside the recursive CTE or in subsequent chained CTEs. |
| 5 | Using `GROUP BY` or `DISTINCT` in the recursive member | ANSI SQL forbids `GROUP BY`, `HAVING`, and window functions directly inside the recursive member. | Pre-aggregate data before recursion or aggregate results in the outer query. |
| 6 | Data type mismatch between Anchor and Recursive members | If the anchor column is `INT` and recursive column evaluates to `BIGINT`, query fails. | Explicitly cast columns in the anchor member (e.g., `1::bigint`). |

---

## Performance & Security Notes

### Working Table Memory Scaling
- In PostgreSQL, the recursive engine evaluates the recursive step using a `WorkTableScan` operator:
```
Recursive Union  (cost=0.00..32.50 rows=101 width=64)
  ->  Seq Scan on company_org  (cost=0.00..1.05 rows=1 width=64)
  ->  Hash Join  (cost=0.15..3.05 rows=10 width=64)
        Hash Cond: (e.manager_id = oh.emp_id)
        ->  Seq Scan on company_org e
        ->  WorkTableScan on org_hierarchy oh
```
- Because the `WorkTable` holds only the delta tuples from the *immediately prior iteration*, memory usage per step is bounded by the branching factor at that specific depth.
- **Denial-of-Service Risk**: Malicious users capable of inserting cyclic foreign keys into parent-child tables can trigger infinite loops that consume 100% database CPU. Enforce `statement_timeout` or use `CYCLE` clauses.

---

## Comparison Matrix

| Approach | Native Recursive CTE | Closure Table (Bridge) | Materialized Path | Adjacency List + Nested Queries |
| :--- | :--- | :--- | :--- | :--- |
| **Schema Complexity** | Minimal (Standard PK/FK) | High (Separate ancestor-descendant table) | Medium (Text column `1/4/9`) | Minimal |
| **Write Cost** | Cheap $O(1)$ insert | Expensive $O(D)$ inserts | Expensive on subtree moves | Cheap $O(1)$ insert |
| **Query Flexibility** | Arbitrary graphs & trees | Strictly directed trees/DAGs | Strictly hierarchical trees | Limited to fixed depth |
| **Engine Portability** | Universal (All 4 engines) | Universal | Universal | Universal |
| **Cycle Resilience** | Requires cycle logic | Schema prevents cycles | Impossible to cycle | Handled in app code |

---

## Interview Questions & Deep Dives

### Q1: What are the three essential components of a recursive CTE?
<details>
<summary>Click to view answer</summary>

1. **The Anchor Member**: A non-recursive query that produces the base starting set (e.g., root nodes of a hierarchy).
2. **The Recursive Member**: A query that references the CTE itself, joining the results of the previous step to discover the next generation of child rows.
3. **The Set Operator**: `UNION ALL` (or `UNION`) that connects the anchor and recursive members.
</details>

### Q2: Why does ANSI SQL forbid using `GROUP BY` or window functions inside the recursive member?
<details>
<summary>Click to view answer</summary>

The recursive algorithm operates on a streaming delta queue (the Working Table) one level at a time. Evaluating `GROUP BY` or window functions requires visibility over the entire cumulative dataset, which is not yet complete while iterations are still running. Permitting aggregation would violate the monotonicity requirements of fixed-point relational algebra.
</details>

### Q3: How does the `CYCLE` clause work in PostgreSQL 14+?
<details>
<summary>Click to view answer</summary>

The `CYCLE col SET is_cycle USING path` clause automatically tracks the sequence of `col` values visited along each traversal branch. If an iteration encounters a `col` value that already exists in its path:
1. It sets the boolean flag `is_cycle` to `TRUE`.
2. It stops expanding the recursive member for that specific branch.
This cleanly prevents infinite loops without requiring manual array manipulation.
</details>

### Q4: What is the default recursion depth limit in Microsoft SQL Server, and how is it modified?
<details>
<summary>Click to view answer</summary>

By default, SQL Server sets `MAXRECURSION` to 100. If a recursive CTE exceeds 100 iterations, the query terminates with error 530. You can modify this limit by appending query hints:
- `OPTION (MAXRECURSION 500)` (increases limit to 500).
- `OPTION (MAXRECURSION 0)` (removes the limit completely; use with extreme caution!).
</details>

### Q5: Can you reference the recursive CTE more than once inside the recursive member?
<details>
<summary>Click to view answer</summary>

No. ANSI SQL mandates that the recursive query specification may contain **at most one** reference to the recursive CTE, and that reference must be in the `FROM` clause. Multiple self-references (such as joining the recursive CTE to itself) are strictly forbidden by database parsers.
</details>

### Q6: What happens if you use `UNION` instead of `UNION ALL` in a recursive CTE?
<details>
<summary>Click to view answer</summary>

Using `UNION` causes the engine to deduplicate tuples across iterations. While this can automatically break simple cycles (since returning an already-seen row produces zero new rows), it incurs substantial performance overhead because the engine must sort or hash the cumulative result set at every single step. `UNION ALL` is strongly preferred.
</details>

### Q7: How do you build a breadcrumb trail (e.g., `'Electronics > Computers > Laptops'`) using recursion?
<details>
<summary>Click to view answer</summary>

In the anchor member, project the root name as text:
`category_name::text AS breadcrumb`.
In the recursive member, concatenate the parent's breadcrumb with the current category:
`ct.breadcrumb || ' > ' || c.category_name`.
</details>

### Q8: How does SQLite handle recursive CTE termination?
<details>
<summary>Click to view answer</summary>

SQLite enforces a compile-time recursion limit (by default 1,000 iterations). If recursion exceeds this limit, SQLite aborts with `parser stack overflow` or `recursion limit exceeded`. SQLite supports standard `WITH RECURSIVE` syntax.
</details>

---

## Practice Exercises

### Exercise 1: Generate Calendar Date Spine
Generate a table of every calendar date in January 2026 using a recursive CTE.

<details>
<summary>Click to view solution</summary>

```sql
WITH RECURSIVE jan_dates AS (
    SELECT '2026-01-01'::date AS cal_date
    UNION ALL
    SELECT (cal_date + INTERVAL '1 day')::date
    FROM jan_dates
    WHERE cal_date < '2026-01-31'::date
)
SELECT cal_date FROM jan_dates;
```
</details>

---

### Exercise 2: Find All Subordinates of a Specific Manager
Given `employees(emp_id, emp_name, manager_id)`. Write a recursive query that finds all direct and indirect subordinates reporting to manager ID `10`.

<details>
<summary>Click to view solution</summary>

```sql
WITH RECURSIVE subordinates AS (
    SELECT emp_id, emp_name, manager_id, 1 AS depth
    FROM employees
    WHERE manager_id = 10

    UNION ALL

    SELECT e.emp_id, e.emp_name, e.manager_id, s.depth + 1
    FROM employees e
    JOIN subordinates s ON e.manager_id = s.emp_id
)
SELECT * FROM subordinates;
```
</details>

---

### Exercise 3: File System Folder Depth
Given `folders(folder_id, parent_id, folder_name)`. Calculate the nesting depth of each folder, where root folders (parent_id IS NULL) have depth 0.

<details>
<summary>Click to view solution</summary>

```sql
WITH RECURSIVE folder_depths AS (
    SELECT folder_id, parent_id, folder_name, 0 AS depth
    FROM folders
    WHERE parent_id IS NULL

    UNION ALL

    SELECT f.folder_id, f.parent_id, f.folder_name, fd.depth + 1
    FROM folders f
    JOIN folder_depths fd ON f.parent_id = fd.folder_id
)
SELECT * FROM folder_depths ORDER BY depth, folder_name;
```
</details>

---

### Exercise 4: Network Degrees of Separation
Given `social_connections(user_a, user_b)`. Find all users connected to user `1` within at most 3 degrees of separation, preventing infinite loops.

<details>
<summary>Click to view solution</summary>

```sql
WITH RECURSIVE network AS (
    SELECT user_b AS friend_id, 1 AS degree, ARRAY[1, user_b] AS visited
    FROM social_connections
    WHERE user_a = 1

    UNION ALL

    SELECT sc.user_b, n.degree + 1, n.visited || sc.user_b
    FROM social_connections sc
    JOIN network n ON sc.user_a = n.friend_id
    WHERE n.degree < 3 AND NOT (sc.user_b = ANY(n.visited))
)
SELECT DISTINCT friend_id, MIN(degree) AS min_degree
FROM network
GROUP BY friend_id
ORDER BY min_degree;
```
</details>

---

### Exercise 5: Ancestry Path Search
Given `categories(category_id, parent_id, category_name)`. For a specific leaf category (`category_id = 99`), traverse *upwards* to list all ancestor categories up to the root.

<details>
<summary>Click to view solution</summary>

```sql
WITH RECURSIVE ancestors AS (
    SELECT category_id, parent_id, category_name, 0 AS level
    FROM categories
    WHERE category_id = 99

    UNION ALL

    SELECT c.category_id, c.parent_id, c.category_name, a.level + 1
    FROM categories c
    JOIN ancestors a ON c.category_id = a.parent_id
)
SELECT * FROM ancestors ORDER BY level ASC;
```
</details>

---

### Exercise 6: SQL Server Portability Adjustment
Identify why the following query fails on Microsoft SQL Server and provide the corrected query:
```sql
WITH RECURSIVE nums AS (
    SELECT 1 AS val
    UNION ALL
    SELECT val + 1 FROM nums WHERE val < 10
)
SELECT * FROM nums;
```

<details>
<summary>Click to view solution</summary>

**Cause**: SQL Server does not support the `RECURSIVE` keyword.

**Corrected SQL Server Query**:
```sql
WITH nums AS (
    SELECT 1 AS val
    UNION ALL
    SELECT val + 1 FROM nums WHERE val < 10
)
SELECT * FROM nums;
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                RECURSIVE CTE CHEAT SHEET                                |
+-------------------+----------------------------+----------------------------------------+
| Clause / Step     | Role                       | Details                                |
+-------------------+----------------------------+----------------------------------------+
| Anchor Member     | Base Case                  | Executes once; seeds the working table |
| UNION ALL         | Set Concatenation          | Connects anchor to recursive step      |
| Recursive Member  | Inductive Step             | References CTE; joins to working table |
| Termination       | Halting Condition          | WHERE clause that eventually returns 0 |
| CYCLE Clause      | Loop Prevention            | Halts traversal when duplicate ID seen |
+-------------------+----------------------------+----------------------------------------+
```

- **Algorithm**: Working Table (delta queue) iteratively evaluated until empty.
- **Cycle Safety**: Always maintain a path array/string or use the ANSI `CYCLE` clause.
- **Dialect Gotcha**: SQL Server rejects the `RECURSIVE` keyword (`WITH cte AS`).

---

## Further Reading
- PostgreSQL 17 Documentation: *WITH Queries - Recursive Common Table Expressions*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 7.15 `<with clause>` & `<cycle clause>`
- Joe Celko: *Trees and Hierarchies in SQL for Smarties*
- Markus Winand: *Modern SQL - The Cycle Clause in PostgreSQL 14*
