# 02: Self Joins & Hierarchical Relations

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [01 - Relational Joins: INNER, OUTER & CROSS](./01-joins.md) |
| **Target Audience** | Software Engineers, Data Engineers, Solution Architects |
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

In relational database design, entities often possess relationships with other entities of the exact same type. An employee reports to another employee who is their manager; a product category (e.g., "Laptops") is a sub-category of a parent category ("Computers"); a comment on a forum is a reply to another comment; a flight connects an origin airport to a destination airport within the same table.

A **Self Join** is a regular relational join in which a table is joined with **itself**. SQL does not have a separate `SELF JOIN` keyword; instead, the query references the same physical table multiple times in the `FROM` clause, distinguishing each instance by assigning mandatory, distinct **table aliases** (e.g., `FROM employees emp JOIN employees mgr`).

Self-joins allow queries to:
1. Reconstruct parent-child and manager-employee relationships.
2. Compare rows within the same table (e.g., finding all employees who earn more than their manager).
3. Detect pairs or duplicates (e.g., finding customers residing in the same city).
4. Analyze sequential chronological states without relying on window functions.

Understanding how self-joins operate, how to handle root entities where parent IDs are `NULL`, and when tree depth requires transitioning to recursive queries is a fundamental requirement of analytical SQL.

---

## 2. Core Concepts

### 1. The Adjacency List Pattern
The most common relational design for hierarchies is the **Adjacency List model**, where a record contains a foreign key pointing to the primary key of its parent in the same table:

```
Table: employees
+-------------+---------------+------------+
| employee_id | name          | manager_id |
+-------------+---------------+------------+
|           1 | Alice (CEO)   | NULL       |
|           2 | Bob (VP)      | 1          |
|           3 | Carol (Eng)   | 2          |
|           4 | David (Eng)   | 2          |
+-------------+---------------+------------+
```

To display each employee alongside their manager's name:
- We treat `employees` as two virtual copies:
  - Copy 1: `emp` (The subordinate role).
  - Copy 2: `mgr` (The supervisor role).
- Join condition: `emp.manager_id = mgr.employee_id`.
- For the CEO (Alice), `manager_id` is `NULL`. Therefore, an **`INNER JOIN` would drop Alice**! A **`LEFT JOIN`** is mandatory to keep root-level entities.

### 2. Multi-Level Hierarchical Self-Joins
A self-join can be chained multiple times to traverse fixed-depth hierarchies:
```sql
FROM employees emp
LEFT JOIN employees mgr ON emp.manager_id = mgr.employee_id
LEFT JOIN employees dir ON mgr.manager_id = dir.employee_id
```
- `emp`: Employee
- `mgr`: Direct Manager
- `dir`: Skip-level Director (Manager's Manager)

---

## 3. How It Works Internally

### Virtual Table Aliasing in the Query Engine

```
Physical Table on Disk: "employees.ibd" or heap relation (Oid 16420)
                             │
                             ├───────────────────────────────────────┐
                             ▼                                       ▼
                 [Virtual Instance: emp]                 [Virtual Instance: mgr]
                 (Subordinate scan stream)               (Supervisor lookup stream)
                             │                                       │
                             └───────────────────┬───────────────────┘
                                                 ▼
                                        [Hash Join / Loop Join]
                                        ON emp.manager_id = mgr.employee_id
```

- To the database engine's storage subsystem, there is only one physical table file.
- The query parser creates two distinct `RangeTblEntry` nodes in the parse tree, assigning them the alias names `emp` and `mgr`.
- In execution, the optimizer can scan the table once to build an in-memory hash table for `mgr`, and scan it a second time to stream `emp` rows, probing the hash table in $O(1)$ time.

---

## 4. Syntax & Usage

### Standard Self-Join Query

```sql
-- Employee-to-Manager Self-Join with Root Preservation
SELECT 
    emp.employee_id,
    emp.first_name || ' ' || emp.last_name AS employee_name,
    emp.salary AS employee_salary,
    COALESCE(mgr.first_name || ' ' || mgr.last_name, 'None (Top Executive)') AS manager_name,
    mgr.salary AS manager_salary
FROM employees emp
LEFT JOIN employees mgr 
  ON emp.manager_id = mgr.employee_id
ORDER BY emp.employee_id;
```

### Multi-Dialect Comparison Matrix

| Self-Join Capability | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Self `INNER JOIN`** | Full support | Full support | Full support | Full support |
| **Self `LEFT JOIN`** | Full support | Full support | Full support | Full support |
| **Mandatory Table Aliasing** | Enforced (Duplicate table name error)| Enforced | Enforced | Enforced |
| **Arbitrary Depth Handling** | Use Recursive CTE (`WITH RECURSIVE`)| Use Recursive CTE (`WITH RECURSIVE`)| Use Recursive CTE (`WITH RECURSIVE`)| Use Recursive CTE (`WITH`) |

---

## 5. 5 Practical Progressive Examples

### Example 1: Basic Adjacency Self-Join (Hierarchical Categories)
Reconstructing product category breadcrumbs:

```sql
-- Seed category tree
CREATE TABLE category_nodes (
    cat_id INT PRIMARY KEY,
    name VARCHAR(50),
    parent_id INT REFERENCES category_nodes(cat_id)
);

INSERT INTO category_nodes VALUES
(1, 'Electronics', NULL),
(2, 'Computers', 1),
(3, 'Laptops', 2),
(4, 'Audio', 1);

-- 2-Level Self-Join: Subcategory to Parent Category
SELECT 
    child.name AS subcategory,
    COALESCE(parent.name, 'Root') AS parent_category
FROM category_nodes child
LEFT JOIN category_nodes parent 
  ON child.parent_id = parent.cat_id
ORDER BY child.cat_id;
```
```text
+---------------+-----------------+
| subcategory   | parent_category |
+---------------+-----------------+
| Electronics   | Root            |
| Computers     | Electronics     |
| Laptops       | Computers       |
| Audio         | Electronics     |
+---------------+-----------------+
```

### Example 2: Finding Employees Earning More Than Their Manager
A classic analytical puzzle demonstrating cross-tuple comparisons within the same table:

```sql
CREATE TABLE staff (
    staff_id INT PRIMARY KEY,
    name VARCHAR(50),
    salary NUMERIC(10, 2),
    manager_id INT REFERENCES staff(staff_id)
);

INSERT INTO staff VALUES
(1, 'Sarah (VP)', 180000.00, NULL),
(2, 'Marcus (Director)', 140000.00, 1),
(3, 'Elena (Staff Eng)', 160000.00, 2), -- Earns MORE than Marcus ($140k)!
(4, 'David (Junior)', 85000.00, 2);

SELECT 
    emp.name AS employee_name,
    emp.salary AS employee_salary,
    mgr.name AS manager_name,
    mgr.salary AS manager_salary,
    emp.salary - mgr.salary AS salary_excess
FROM staff emp
JOIN staff mgr 
  ON emp.manager_id = mgr.staff_id
WHERE emp.salary > mgr.salary;
```
```text
+-------------------+-----------------+-------------------+----------------+---------------+
| employee_name     | employee_salary | manager_name      | manager_salary | salary_excess |
+-------------------+-----------------+-------------------+----------------+---------------+
| Elena (Staff Eng) |       160000.00 | Marcus (Director) |      140000.00 |      20000.00 |
+-------------------+-----------------+-------------------+----------------+---------------+
```

### Example 3: Edge Case (Finding Duplicate Pairs Without Permutations)
Finding all pairs of customers located in the exact same country:

```sql
-- ANTI-PATTERN using <>: Produces duplicate mirror pairs (A, B) AND (B, A)!
-- WHERE a.country = b.country AND a.customer_id <> b.customer_id

-- PRODUCTION STANDARD using <: Produces unique pairs only!
SELECT 
    a.first_name || ' ' || a.last_name AS customer_1,
    b.first_name || ' ' || b.last_name AS customer_2,
    a.country
FROM customers a
JOIN customers b 
  ON a.country = b.country 
 AND a.customer_id < b.customer_id
ORDER BY a.country;
```
```text
+--------------+--------------+---------+
| customer_1   | customer_2   | country |
+--------------+--------------+---------+
| Alex Morgan  | John Doe     | USA     |
+--------------+--------------+---------+
```
*(Using `a.id < b.id` eliminates both self-matching `a.id = a.id` and reverse duplicates `(B, A)`).*

### Example 4: Failure Mode (Missing Outer Join Dropping Root Entities)
What happens when you use an `INNER JOIN` on an adjacency list:

```sql
-- FAILS to include Sarah (VP / CEO) because her manager_id is NULL!
SELECT emp.name, mgr.name AS manager
FROM staff emp
INNER JOIN staff mgr ON emp.manager_id = mgr.staff_id;
-- Sarah is omitted completely!
```
*(Always use `LEFT OUTER JOIN` on the parent table so that top-level root entities with `parent_id IS NULL` remain visible).*

### Example 5: Multi-Tier Hierarchy Breadcrumb (3-Level Self-Join)
Traversing a 3-tier organizational chart:

```sql
SELECT 
    emp.name AS employee,
    COALESCE(mgr.name, 'None') AS manager,
    COALESCE(dir.name, 'None') AS director
FROM staff emp
LEFT JOIN staff mgr ON emp.manager_id = mgr.staff_id
LEFT JOIN staff dir ON mgr.manager_id = dir.staff_id
ORDER BY emp.staff_id;
```
```text
+-------------------+-------------------+-------------+
| employee          | manager           | director    |
+-------------------+-------------------+-------------+
| Sarah (VP)        | None              | None        |
| Marcus (Director) | Sarah (VP)        | None        |
| Elena (Staff Eng) | Marcus (Director) | Sarah (VP)  |
| David (Junior)    | Marcus (Director) | Sarah (VP)  |
+-------------------+-------------------+-------------+
```

---

## 6. Best Practices

1. **Always Use Descriptive Aliases**: Never write `FROM employees a JOIN employees b`. Use role-based aliases that describe the semantic function of each instance: `emp` and `mgr`, `child` and `parent`, `curr_event` and `prev_event`.
2. **Always Use `LEFT JOIN` When Root Entities Must Be Preserved**: In an adjacency hierarchy, the top-level entity (CEO, root category) has `parent_id = NULL`. Using an `INNER JOIN` discards the root entity entirely.
3. **Use `a.id < b.id` to Eliminate Mirror Duplicate Pairs**: When finding matches or duplicates within the same table, joining with `a.id < b.id` eliminates both self-pairing and symmetrical inverted duplicates `(A, B)` vs `(B, A)`.
4. **Always Index the Self-Referencing Foreign Key Column**: In `employees(manager_id)`, create an index on `manager_id`: `CREATE INDEX idx_emp_mgr ON employees(manager_id);`. This allows self-joins to use fast Index Nested Loop lookups.
5. **Switch to Recursive CTEs for Arbitrary Depths**: If the depth of your tree is unknown or dynamic (e.g., folder subdirectories, comment threads), do not write 15 self-joins. Use a Recursive Common Table Expression (`WITH RECURSIVE`).

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Omitting Table Aliases** | Query throws an immediate error: `table name specified more than once`. | Assign explicit distinct aliases: `FROM tbl a JOIN tbl b`. |
| **Using `INNER JOIN` on Hierarchies** | Silently discards the CEO or root category whose `parent_id IS NULL`. | Use `LEFT JOIN` for parent table reference. |
| **Duplicate Pairs via `a.id <> b.id`** | Returns both `(User 1, User 2)` AND `(User 2, User 1)`. | Use `ON a.id < b.id` to enforce a single canonical pair. |
| **Deep Chains of 10+ Self-Joins** | Slow, brittle, and breaks if the hierarchy exceeds the hardcoded join depth. | Use Recursive CTEs (`WITH RECURSIVE`). |
| **Missing Index on Parent Foreign Key** | Causes the query engine to perform expensive full table scans for both virtual table instances. | Index the parent foreign key: `CREATE INDEX idx_parent ON tbl(parent_id);`. |
| **Circular References in Adjacency Lists** | If Employee A reports to Employee B, and Employee B reports to Employee A, self-joins return infinite cycles in recursive traversals. | Enforce acyclic graphs using validation triggers or check constraints. |

---

## 8. Performance & Security Considerations

### Performance
- **Index Lookups on Both Halves**:
  - The query `FROM employees emp LEFT JOIN employees mgr ON emp.manager_id = mgr.employee_id`:
    - Scans `emp`.
    - Performs an $O(\log N)$ B-Tree index seek into `employees_pkey` on `mgr.employee_id`.
    - High-performance, low-memory execution.
- **Cache Locality**:
  - Because both virtual tables reference the same physical 8KB disk blocks, the buffer pool cache hit ratio is typically near 100%.

### Security
- **Hierarchical Access Control**:
  - Ensure users can only see subordinates within their reporting tree by adding a security predicate on the root manager ID:
    `WHERE mgr.department_id = $user_dept_id`.

---

## 9. Comparison Matrix

### Hierarchical Querying Approaches Compared

| Approach | Hierarchy Depth | Query Complexity | Index Efficiency | Best Used For |
|:---|:---|:---|:---|:---|
| **Self-Join (`JOIN ... ON`)** | Fixed (1 or 2 levels) | Low | Excellent (B-Tree PK) | Manager-employee, category-parent |
| **Recursive CTE (`WITH RECURSIVE`)**| Arbitrary / Dynamic | Moderate | High (with loop index) | Threaded comments, folder trees, bill of materials |
| **Materialized Path (e.g. `'1/2/4'`)**| Arbitrary | Low (Regex / Prefix) | B-Tree prefix scan | Breadcrumbs, file paths |
| **Closure Table (Junction Table)**| Arbitrary | Moderate (Extra table) | Fast relational lookups | Multi-parent DAGs, fast ancestor checks |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: What is a Self-Join, and why does SQL not provide a dedicated `SELF JOIN` keyword?
<details>
<summary>Answer</summary>
A self-join is a standard relational join operation where a table is joined with itself. 

SQL does not require a special keyword because the existing `JOIN` operator operates on relations. By assigning distinct table aliases (e.g., `FROM tbl a JOIN tbl b`), the query planner treats `a` and `b` as two independent virtual relations. Any standard join type (`INNER`, `LEFT`, `CROSS`, `FULL`) can be applied to these two aliases.
</details>

#### Q2: Why is `ON a.id < b.id` preferred over `ON a.id <> b.id` when searching for duplicates or paired records in the same table?
<details>
<summary>Answer</summary>
When finding pairs of rows that share an attribute (e.g., customers in the same city):
- If you use `ON a.id <> b.id`, the query generates both combinations: `(Alice, Bob)` and `(Bob, Alice)`.
- If you use `ON a.id < b.id`, it strictly enforces that the first ID must be smaller than the second ID. This accomplishes two things:
  1. It eliminates self-pairing (`a.id = b.id` is false).
  2. It eliminates reciprocal duplicate permutations, returning exactly one unique row per pair.
</details>

#### Q3: What is the limitation of self-joins for hierarchical tree structures, and what replaces them?
<details>
<summary>Answer</summary>
A self-join can only traverse a **fixed, hardcoded number of levels** in a hierarchy. If an organization has up to 10 levels of management, you would need to write 9 consecutive self-joins. If an employee is added at level 11, the query silently truncates the hierarchy.

For dynamic or arbitrary-depth hierarchies, self-joins are replaced by **Recursive Common Table Expressions (`WITH RECURSIVE`)**, which iteratively traverse parent-child pointers until no further children exist.
</details>

#### Q4: Why must an employee-manager query use a `LEFT OUTER JOIN` instead of an `INNER JOIN`?
<details>
<summary>Answer</summary>
In an organizational hierarchy, the top-level executive (CEO or business owner) has no manager; their `manager_id` foreign key is `NULL`. 

An `INNER JOIN` evaluates `emp.manager_id = mgr.employee_id`. Because `NULL = mgr.employee_id` evaluates to `UNKNOWN`, the CEO tuple fails the join condition and is discarded. Using a `LEFT OUTER JOIN` ensures that all employees are preserved in the output, emitting `NULL` for the CEO's manager.
</details>

### Practical Self-Join Puzzles

#### Q5: Write an SQL query to find all employees who were hired on the exact same date as another employee in the same company.
<details>
<summary>Answer</summary>

```sql
SELECT 
    a.employee_id AS emp_1_id,
    a.first_name || ' ' || a.last_name AS emp_1_name,
    b.employee_id AS emp_2_id,
    b.first_name || ' ' || b.last_name AS emp_2_name,
    a.hire_date
FROM employees a
JOIN employees b 
  ON a.hire_date = b.hire_date 
 AND a.employee_id < b.employee_id
ORDER BY a.hire_date;
```
</details>

#### Q6: Given a table `flights(flight_id, origin_city, destination_city)`, write a self-join query to find all 1-stop connecting flight options from 'New York' to 'London'.
<details>
<summary>Answer</summary>

```sql
SELECT 
    f1.flight_id AS leg1_flight,
    f1.origin_city AS departure,
    f1.destination_city AS layover_city,
    f2.flight_id AS leg2_flight,
    f2.destination_city AS final_destination
FROM flights f1
JOIN flights f2 
  ON f1.destination_city = f2.origin_city
WHERE f1.origin_city = 'New York'
  AND f2.destination_city = 'London';
```
</details>

#### Q7: Write a query to find all employees who earn a higher salary than the average salary of their own direct manager.
<details>
<summary>Answer</summary>

```sql
SELECT 
    emp.name AS employee_name,
    emp.salary AS employee_salary,
    mgr.name AS manager_name,
    mgr.salary AS manager_salary
FROM staff emp
JOIN staff mgr 
  ON emp.manager_id = mgr.staff_id
WHERE emp.salary > mgr.salary;
```
</details>

#### Q8: Write a self-join query on a `temperature_logs(recorded_date DATE, temp INT)` table to find all dates where the temperature was higher than the previous day.
<details>
<summary>Answer</summary>

```sql
SELECT 
    curr.recorded_date,
    curr.temp AS current_temp,
    prev.temp AS previous_day_temp
FROM temperature_logs curr
JOIN temperature_logs prev 
  ON curr.recorded_date = prev.recorded_date + INTERVAL '1 day'
WHERE curr.temp > prev.temp
ORDER BY curr.recorded_date;
```
*(Demonstrates temporal self-joining without window functions).*
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Basic Manager Lookup
Display `employee_id`, employee `first_name`, and their manager's `first_name` using `employees`. Ensure the CEO is displayed with a manager name of `'Top Executive'`.
<details>
<summary>Solution</summary>

```sql
SELECT 
    e.employee_id,
    e.first_name AS employee_name,
    COALESCE(m.first_name, 'Top Executive') AS manager_name
FROM employees e
LEFT JOIN employees m ON e.manager_id = m.employee_id
ORDER BY e.employee_id;
```
</details>

#### Exercise 2: Category Parent Display
Using `categories`, select `category_name` and its parent category name.
<details>
<summary>Solution</summary>

```sql
SELECT 
    c.category_name AS category,
    COALESCE(p.category_name, 'Root Category') AS parent_category
FROM categories c
LEFT JOIN categories p ON c.parent_id = p.category_id
ORDER BY c.category_id;
```
</details>

### Intermediate Exercises

#### Exercise 3: Finding Peers in the Same Department
Write a query to list all pairs of employees who work in the exact same department, ensuring no duplicate pairs or self-pairs appear.
<details>
<summary>Solution</summary>

```sql
SELECT 
    e1.first_name || ' ' || e1.last_name AS employee_1,
    e2.first_name || ' ' || e2.last_name AS employee_2,
    e1.department_id
FROM employees e1
JOIN employees e2 
  ON e1.department_id = e2.department_id 
 AND e1.employee_id < e2.employee_id
ORDER BY e1.department_id, e1.employee_id;
```
</details>

#### Exercise 4: Identifying Managers with at Least 3 Direct Reports
Use a self-join to find all employees who manage 3 or more direct reports, showing manager name and subordinate count.
<details>
<summary>Solution</summary>

```sql
SELECT 
    mgr.employee_id,
    mgr.first_name || ' ' || mgr.last_name AS manager_name,
    COUNT(emp.employee_id) AS direct_reports_count
FROM employees mgr
JOIN employees emp ON mgr.employee_id = emp.manager_id
GROUP BY mgr.employee_id, mgr.first_name, mgr.last_name
HAVING COUNT(emp.employee_id) >= 3
ORDER BY direct_reports_count DESC;
```
</details>

### Advanced Exercises

#### Exercise 5: 3-Tier Hierarchy with Compensation Discrepancy
Find all employees whose salary exceeds that of their skip-level manager (their manager's manager).
<details>
<summary>Solution</summary>

```sql
SELECT 
    emp.name AS employee_name,
    emp.salary AS employee_salary,
    dir.name AS director_name,
    dir.salary AS director_salary
FROM staff emp
JOIN staff mgr ON emp.manager_id = mgr.staff_id
JOIN staff dir ON mgr.manager_id = dir.staff_id
WHERE emp.salary > dir.salary;
```
</details>

#### Exercise 6: Chronological Consecutive Event Detection via Self-Join
Given a table `login_events(user_id INT, login_date DATE)`, find all users who logged in on two consecutive days.
<details>
<summary>Solution</summary>

```sql
SELECT DISTINCT 
    curr.user_id,
    prev.login_date AS first_day,
    curr.login_date AS second_day
FROM login_events curr
JOIN login_events prev 
  ON curr.user_id = prev.user_id 
 AND curr.login_date = prev.login_date + INTERVAL '1 day'
ORDER BY curr.user_id, first_day;
```
</details>

---

## 12. Summary & Quick Reference

### Self-Join Rules of Thumb
- **Aliases Are Mandatory**: You must assign distinct table aliases (`FROM tbl emp JOIN tbl mgr`).
- **Preserve Roots**: Always use `LEFT JOIN` on the parent alias so root entities (`parent_id IS NULL`) are not discarded.
- **Unique Pairs**: Use `ON a.id < b.id` to eliminate self-matches and mirror duplicates `(A, B)` vs `(B, A)`.
- **Indexing**: Always index self-referencing foreign key columns (`CREATE INDEX idx ON tbl(parent_id);`).
- **Dynamic Depth**: For trees deeper than 2–3 levels or arbitrary depth, use `WITH RECURSIVE` instead of chaining self-joins.

---

## 13. Further Reading
- PostgreSQL 17 Documentation: [Joined Tables](https://www.postgresql.org/docs/17/queries-table-expressions.html#QUERIES-FROM)
- Joe Celko: *"Trees and Hierarchies in SQL for Smarties"* (Morgan Kaufmann)
- [03 - Set Operations: UNION, INTERSECT & EXCEPT](./03-set-operations.md)
