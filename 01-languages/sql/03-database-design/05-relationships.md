# Relationships: 1:1, 1:N, M:N, Trees, and the Polymorphic Anti-Pattern

| Metadata | Details |
| :--- | :--- |
| **Topic** | Relational Relationship Modeling, 1:1, 1:N, M:N Bridges, Self-Referential Trees, Polymorphic Associations |
| **Dialect** | ANSI SQL:2023 / PostgreSQL 17+ (Primary), MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Database Version** | PostgreSQL 17.2, MySQL 8.4.1, SQLite 3.50.4, SQL Server 2022 CU14 |
| **Tested On** | Python 3.14 stdlib sqlite3 engine & PostgreSQL 17 execution runner |
| **Date** | 2026-11-26 |
| **Author** | Antigravity SQL Curriculum Lead |
| **Category** | Database Design & Schema Architecture |

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

Relational database management systems (RDBMS) derive their very name from the mathematical concept of relations. In the real world, entities never exist in isolation: customers place orders, orders contain products, employees manage departments, and files reside in directories.

How these associations are translated into physical SQL tables determines the integrity, query performance, and long-term viability of the software architecture.

While modeling One-to-Many (1:N) relationships using foreign keys is straightforward, complex real-world requirements introduce perilous traps:
- Incorrectly modeling One-to-One (1:1) relationships, allowing child rows to multiply uncontrollably.
- Struggling with recursive tree hierarchies and self-referential cycles.
- Falling victim to the **Polymorphic Associations Anti-Pattern** (popularized by web application frameworks like Ruby on Rails and Laravel), which deliberately destroys foreign key integrity, introduces silent orphan records, and defeats query optimizers.

Understanding how to model clean 1:1, 1:N, M:N junction tables, hierarchical trees, and architecturally sound alternatives to polymorphic associations is essential for building robust relational schemas.

---

## Core Concepts

### 1. One-to-One (1:1) Relationships
In a 1:1 relationship, exactly one instance of Entity A is associated with at most one instance of Entity B.
- **Implementation Pattern A (Shared Primary Key - Preferred)**:
  ```sql
  CREATE TABLE user_profiles (
      user_id BIGINT PRIMARY KEY REFERENCES users(user_id) ON DELETE CASCADE,
      avatar_url TEXT
  );
  ```
  Here, `user_id` is simultaneously the Primary Key and the Foreign Key. It is physically impossible for a user to have more than one profile.
- **Implementation Pattern B (Unique Foreign Key)**:
  A separate primary key exists, but a `UNIQUE` constraint is placed on the foreign key column: `FOREIGN KEY (user_id) REFERENCES users(user_id) UNIQUE`.

### 2. One-to-Many (1:N) Relationships
The standard workhorse of relational databases:
- The "Many" child table contains a foreign key pointing to the "One" parent table.
- Cardinality enforcement: The foreign key column is marked `NOT NULL` if the child must strictly belong to a parent (mandatory participation), or left nullable if optional.

### 3. Many-to-Many (M:N) Relationships
Because relational columns must be scalar (1NF), an M:N relationship cannot be stored directly. It must be resolved via an intermediate **associative entity (junction table)**:
- **Pure Bridge Table**: Contains only the two foreign keys, combined into a composite primary key: `PRIMARY KEY (student_id, course_id)`.
- **Rich Associative Entity**: Contains relationship-specific attributes (e.g., `enrolled_at`, `final_grade`, `payment_status`).

### 4. Self-Referential (Recursive) Relationships
An entity is associated with other instances of itself:
- Examples: Employee manager hierarchy (`manager_id REFERENCES employees(emp_id)`), nested category trees (`parent_category_id REFERENCES categories(category_id)`).
- Requires careful handling of root nodes (`manager_id IS NULL`) and cycle prevention.

### 5. The Polymorphic Associations Anti-Pattern
In object-oriented frameworks, developers often want a single `comments` table that can attach to `posts`, `photos`, or `videos`:
```sql
-- THE POLYMORPHIC ANTI-PATTERN (DO NOT USE IN PRODUCTION!):
CREATE TABLE comments (
    comment_id BIGINT PRIMARY KEY,
    commentable_id BIGINT NOT NULL,       -- Could be post_id, photo_id, or video_id!
    commentable_type VARCHAR(20) NOT NULL, -- 'Post', 'Photo', 'Video'
    body TEXT NOT NULL
);
```
**Why it is a relational disaster**:
1. **Zero Referential Integrity**: You cannot declare a `FOREIGN KEY` constraint on `commentable_id` because it points to different tables conditionally.
2. **Orphan Records**: Deleting a post or photo leaves orphaned comment records floating in the database.
3. **Broken Joins**: To query comments, the database engine cannot perform clean join seeks and must resort to dynamic branching.

---

## How It Works Internally

How does a database engine enforce relational integrity across parent and child tables?

```
                 [ Foreign Key Referential Actions ]
                                  │
          Child Table: orders (customer_id REFERENCES customers)
                                  │
         What happens when: DELETE FROM customers WHERE customer_id = 42?
                                  │
         ┌────────────────────────┼────────────────────────┐
         ▼                        ▼                        ▼
    [ RESTRICT ]             [ CASCADE ]              [ SET NULL ]
    (Default ANSI)           Engine finds all         Engine sets
    Engine checks FK.        matching child rows      orders.customer_id
    If child rows exist,     in 'orders' and deletes  to NULL. (Requires FK
    aborts transaction       them automatically in    to be nullable!).
    immediately!             the same transaction!
```

### Relational Alternatives to Polymorphic Associations
To model multi-entity attachments while preserving strict relational integrity, architects use three proven patterns:

```
Pattern 1: Exclusive Arc (Check Constraint)
   Table: comments
   Columns: post_id FK, photo_id FK, video_id FK
   Constraint: CHECK (num_nonnulls(post_id, photo_id, video_id) = 1)
   -> 100% Referential Integrity! Foreign keys verified on every table.

Pattern 2: Shared Supertype (Table Inheritance)
   Table: commentable_entities (entity_id PK, entity_type)
   Table: posts  (post_id PK REFERENCES commentable_entities)
   Table: photos (photo_id PK REFERENCES commentable_entities)
   Table: comments (comment_id PK, entity_id FK REFERENCES commentable_entities)
   -> Elegant, normalized, scales to dozens of entity types!

Pattern 3: Discrete Junction Tables
   Table: post_comments (post_id FK, comment_id FK)
   Table: photo_comments (photo_id FK, comment_id FK)
   -> Strict separation, zero nullable columns.
```

---

## Syntax & Usage

### 1. Clean Relational Alternative: The Exclusive Arc Pattern
```sql
CREATE TABLE posts (
    post_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    title VARCHAR(150) NOT NULL
);

CREATE TABLE photos (
    photo_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    image_url TEXT NOT NULL
);

CREATE TABLE comments (
    comment_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    body TEXT NOT NULL,
    post_id BIGINT NULL REFERENCES posts(post_id) ON DELETE CASCADE,
    photo_id BIGINT NULL REFERENCES photos(photo_id) ON DELETE CASCADE,
    -- Enforce that EXACTLY ONE foreign key is populated!
    CONSTRAINT chk_exclusive_comment_target CHECK (
        num_nonnulls(post_id, photo_id) = 1
    )
);

CREATE INDEX idx_comments_post_id ON comments (post_id) WHERE post_id IS NOT NULL;
CREATE INDEX idx_comments_photo_id ON comments (photo_id) WHERE photo_id IS NOT NULL;
```

### 2. Cascading Referential Actions
```sql
CREATE TABLE audit_records (
    record_id BIGINT PRIMARY KEY,
    author_id BIGINT NULL,
    -- If author is deleted, keep the audit log but set author to NULL!
    CONSTRAINT fk_audit_author FOREIGN KEY (author_id) 
        REFERENCES users(user_id) 
        ON DELETE SET NULL
);
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **`num_nonnulls()` Helper** | Supported natively | **Not Supported** (Use `CASE + CASE = 1`) | **Not Supported** (Use `(c1 IS NOT NULL) + ...`) | **Not Supported** |
| **Shared PK (1:1)** | Fully supported | Fully supported | Fully supported | Fully supported |
| **Cascade Actions** | `CASCADE`, `RESTRICT`, `NO ACTION`, `SET NULL`, `SET DEFAULT` | `CASCADE`, `RESTRICT`, `NO ACTION`, `SET NULL` | `CASCADE`, `RESTRICT`, `NO ACTION`, `SET NULL` | `CASCADE`, `NO ACTION`, `SET NULL`, `SET DEFAULT` (Cycles throw error!) |
| **Self-Referential FKs** | Supported | Supported | Supported | Supported (Cannot cascade cycles) |

---

## Practical Examples

### Example 1: Basic — Strict 1:1 Shared Primary Key
Model user settings ensuring that a settings row can only exist if the user exists, and each user can have at most one settings record.

```sql
CREATE TABLE user_accounts (
    user_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    email VARCHAR(255) NOT NULL UNIQUE
);

CREATE TABLE user_settings (
    user_id BIGINT PRIMARY KEY, -- Primary Key AND Foreign Key!
    theme VARCHAR(20) NOT NULL DEFAULT 'SYSTEM',
    email_notifications BOOLEAN NOT NULL DEFAULT TRUE,
    CONSTRAINT fk_settings_user FOREIGN KEY (user_id) 
        REFERENCES user_accounts(user_id) 
        ON DELETE CASCADE
);

INSERT INTO user_accounts (email) VALUES ('alice@example.com');
INSERT INTO user_settings (user_id, theme) VALUES (1, 'DARK');

-- Attempting to insert a duplicate settings row fails with PK collision!
-- INSERT INTO user_settings (user_id) VALUES (1); -> ERROR: duplicate key value violates unique constraint
```

---

### Example 2: Real-World — Rich M:N Course Registration with Grade Tracking
An academic registration system tracking enrollment date, completion status, and semester.

```sql
CREATE TABLE students (
    student_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    student_name VARCHAR(100) NOT NULL
);

CREATE TABLE courses (
    course_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    course_code VARCHAR(10) NOT NULL UNIQUE,
    credit_hours INT NOT NULL CHECK (credit_hours > 0)
);

CREATE TABLE enrollments (
    student_id BIGINT NOT NULL,
    course_id BIGINT NOT NULL,
    semester VARCHAR(10) NOT NULL, -- e.g., '2026-SPRING'
    enrollment_status VARCHAR(20) NOT NULL DEFAULT 'ENROLLED',
    final_grade NUMERIC(3,2) NULL CHECK (final_grade BETWEEN 0.0 AND 4.0),
    PRIMARY KEY (student_id, course_id, semester),
    FOREIGN KEY (student_id) REFERENCES students(student_id) ON DELETE CASCADE,
    FOREIGN KEY (course_id) REFERENCES courses(course_id) ON DELETE RESTRICT
);

-- Crucial: Index the foreign keys for fast joins!
CREATE INDEX idx_enrollments_course ON enrollments (course_id);
CREATE INDEX idx_enrollments_student ON enrollments (student_id);
```

---

### Example 3: Edge Case — Recursive Adjacency Tree with Hierarchy Safeguard
Model department management where each department reports to a parent department, and root departments have `parent_dept_id IS NULL`.

```sql
CREATE TABLE departments (
    dept_id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    dept_name VARCHAR(100) NOT NULL,
    parent_dept_id INT NULL,
    CONSTRAINT fk_dept_parent FOREIGN KEY (parent_dept_id) 
        REFERENCES departments(dept_id) 
        ON DELETE RESTRICT,
    -- Prevent an entity from being its own immediate parent!
    CONSTRAINT chk_no_self_parent CHECK (dept_id <> parent_dept_id)
);

CREATE INDEX idx_dept_parent ON departments (parent_dept_id);

INSERT INTO departments (dept_name, parent_dept_id) VALUES ('Executive', NULL);
INSERT INTO departments (dept_name, parent_dept_id) VALUES ('Engineering', 1);
INSERT INTO departments (dept_name, parent_dept_id) VALUES ('DevOps Team', 2);
```

---

### Example 4: Failure Mode — The Polymorphic Association Disaster
Demonstrating how polymorphic associations silently produce orphaned records.

```sql
CREATE TABLE raw_posts (post_id INT PRIMARY KEY, content TEXT);
CREATE TABLE raw_comments (
    comment_id INT PRIMARY KEY,
    commentable_id INT NOT NULL, -- NO FOREIGN KEY!
    commentable_type VARCHAR(20) NOT NULL,
    body TEXT NOT NULL
);

INSERT INTO raw_posts VALUES (10, 'Post about database architecture');
INSERT INTO raw_comments VALUES (1001, 10, 'Post', 'Insightful read!');

-- DISASTER: Post 10 is deleted from the system:
DELETE FROM raw_posts WHERE post_id = 10;

-- What happened to the comment?
SELECT * FROM raw_comments WHERE commentable_id = 10;
-- Comment 1001 still exists, pointing to a non-existent post! Orphan record created!
```

---

### Example 5: Optimized — Shared Supertype Architecture (Clean Polymorphic Alternative)
Model multiple entities (Articles, Products, Events) sharing a single tagging or commenting subsystem using a shared supertype table.

```sql
-- Supertype Table: Base Entity
CREATE TABLE content_entities (
    entity_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    entity_type VARCHAR(20) NOT NULL CHECK (entity_type IN ('ARTICLE', 'PRODUCT')),
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Subtype A: Articles
CREATE TABLE articles (
    entity_id BIGINT PRIMARY KEY REFERENCES content_entities(entity_id) ON DELETE CASCADE,
    article_title VARCHAR(200) NOT NULL,
    article_body TEXT NOT NULL
);

-- Subtype B: Products
CREATE TABLE products (
    entity_id BIGINT PRIMARY KEY REFERENCES content_entities(entity_id) ON DELETE CASCADE,
    product_sku VARCHAR(30) NOT NULL UNIQUE,
    price NUMERIC(10,2) NOT NULL
);

-- Unified Comments Table: Fully Protected by Foreign Keys!
CREATE TABLE entity_comments (
    comment_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    entity_id BIGINT NOT NULL REFERENCES content_entities(entity_id) ON DELETE CASCADE,
    author_name VARCHAR(50) NOT NULL,
    comment_text TEXT NOT NULL
);

CREATE INDEX idx_entity_comments_entity ON entity_comments (entity_id);
```

---

## Best Practices

1. **Avoid Polymorphic Associations in RDBMS**: Never use `commentable_id` + `commentable_type`. Use the **Exclusive Arc** pattern or **Shared Supertype** pattern to preserve foreign key constraints.
2. **Use Shared Primary Keys for 1:1 Extensions**: For 1:1 extension tables (e.g., `user_profiles`), use the parent's primary key as the child's primary key.
3. **Always Index Foreign Keys in Junction Tables**: Ensure both legs of a junction table are indexed: the primary key indexes `(a, b)`, and an explicit secondary index indexes `(b, a)`.
4. **Choose Delete Actions Carefully**: Use `ON DELETE CASCADE` for dependent weak entities (`order_items`), but use `ON DELETE RESTRICT` for master reference tables (`customers`, `courses`).
5. **Add Self-Parenting Check Constraints**: On recursive adjacency tables, always add `CHECK (id <> parent_id)` to catch immediate 1-hop circular loops.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Polymorphic associations (`item_id` + `item_type`)| Cannot declare foreign keys; causes orphaned data and broken joins. | Use Exclusive Arc (`num_nonnulls`) or Shared Supertype. |
| 2 | Forgetting `UNIQUE` on 1:1 foreign keys | Child table permits multiple rows per parent, quietly mutating 1:1 into 1:N. | Use shared primary key or add `UNIQUE` to foreign key. |
| 3 | Missing reverse index on junction tables | Queries searching by the second foreign key perform full table scans. | Add a secondary index on `(child_b, child_a)`. |
| 4 | SQL Server cascade cycle limitations | SQL Server throws error 1785 if multiple cascade paths exist from one table. | Use `ON DELETE NO ACTION` with explicit triggers in T-SQL. |
| 5 | Using `CASCADE` on critical financial ledgers | Deleting a merchant account accidentally deletes all historical invoices. | Use `ON DELETE RESTRICT`. |
| 6 | Recursive cycles in parent-child tables | A child is assigned as the parent of its own ancestor, crashing queries. | Use recursive CTE cycle checks or database validation triggers. |

---

## Performance & Security Notes

### Cascade Lock Contention in High-Scale Systems
When executing `DELETE FROM parent WHERE id = 1` with `ON DELETE CASCADE`:
- The database engine locks the parent row.
- It then seeks and locks all matching rows in the child table.
- If the child table also cascades to grandchildren, locks propagate across dozens of tables in a single transaction.
- In high-throughput OLTP systems, cascading deletes of high-volume parent records can lock thousands of rows, causing widespread connection timeouts and deadlocks.
- **Architectural Solution**: For high-volume systems, perform bulk cascading deletions in small, controlled background batches (e.g., deleting 1,000 children at a time) rather than relying on automatic relational cascades.

---

## Comparison Matrix

| Relationship Pattern | Implementation | Foreign Key Protected? | Scalability |
| :--- | :--- | :--- | :--- |
| **Strict 1:1** | Shared Primary Key (`PK REFERENCES parent(PK)`) | **100% Yes** | Optimal |
| **1:N Parent-Child** | `NOT NULL FK` in child + B-Tree index | **100% Yes** | Optimal |
| **M:N Junction** | Associative table with Composite PK | **100% Yes** | Optimal |
| **Polymorphic (Anti-Pattern)**| `id INT, type VARCHAR` | **0% NO (Forbidden)** | Worst (Orphans, slow joins) |
| **Exclusive Arc** | Multi-column FKs + `num_nonnulls() = 1` | **100% Yes** | High (Up to 4–5 targets) |
| **Shared Supertype** | Base entity table + Subtype tables | **100% Yes** | Highest (Scales to N targets)|

---

## Interview Questions & Deep Dives

### Q1: Why is the "Polymorphic Association" considered an anti-pattern in relational databases?
<details>
<summary>Click to view answer</summary>

A polymorphic association stores an entity identifier (`commentable_id`) and an entity name (`commentable_type`) in place of a standard foreign key. This violates foundational relational algebra principles:
1. **No Referential Integrity**: Relational engines cannot enforce foreign keys to dynamic table targets. If a target row is deleted, the database cannot cascade or prevent the deletion, leaving orphan records.
2. **Optimizer Degradation**: The query planner cannot deduce selectivity, check constraints, or use foreign key join elimination.
3. **Data Type Collisions**: If `posts.id` is a `BIGINT` and `photos.id` is a `UUID`, storing both in a single column forces the use of strings, causing index degradation.
</details>

### Q2: How does the "Exclusive Arc" pattern solve the polymorphic requirement cleanly?
<details>
<summary>Click to view answer</summary>

The Exclusive Arc pattern replaces the dynamic ID/Type columns with explicit, strongly-typed foreign keys for every possible target table, accompanied by a `CHECK` constraint:
```sql
post_id BIGINT NULL REFERENCES posts(post_id),
photo_id BIGINT NULL REFERENCES photos(photo_id),
CONSTRAINT chk_exclusive CHECK (num_nonnulls(post_id, photo_id) = 1)
```
Every target is fully protected by referential integrity, foreign key cascades work perfectly, and indexes are optimal.
</details>

### Q3: What is the difference between `ON DELETE RESTRICT` and `ON DELETE NO ACTION`?
<details>
<summary>Click to view answer</summary>

In standard ANSI SQL:
- `RESTRICT` checks for foreign key violations **immediately** when the `DELETE` statement executes. It rejects the deletion before any other triggers or cascade operations run.
- `NO ACTION` checks for foreign key violations at the **end of the statement** (or at `COMMIT` time if the constraint is `DEFERRABLE`). This allows intermediate triggers or cascade scripts within the same statement to clean up child rows before the check is evaluated.
In PostgreSQL, `NO ACTION` is the default behavior.
</details>

### Q4: How should a 1-to-1 relationship be modeled if both sides are strictly mandatory?
<details>
<summary>Click to view answer</summary>

If both sides are strictly mandatory (Entity A cannot exist without Entity B, and Entity B cannot exist without Entity A), the ideal physical design is to **merge them into a single table**. If they must remain separate (for security, column limits, or physical isolation), they must use mutual foreign keys configured as `DEFERRABLE INITIALLY DEFERRED` so that both rows can be inserted within a single transaction before validation occurs at `COMMIT`.
</details>

### Q5: Why is a reverse index necessary on a Many-to-Many junction table?
<details>
<summary>Click to view answer</summary>

A junction table has a composite primary key `PRIMARY KEY (student_id, course_id)`. The B-Tree index created for this key sorts primarily by `student_id`.
- Queries searching by `student_id` (e.g., "Find all courses for Alice") perform an instant index seek.
- However, queries searching by `course_id` (e.g., "Find all students in Math 101") cannot utilize the index because `course_id` is the second column in the composite index.
A secondary index on `(course_id, student_id)` is required to accelerate reverse lookups.
</details>

### Q6: What is the "Shared Supertype" pattern?
<details>
<summary>Click to view answer</summary>

The Shared Supertype (or Class Table Inheritance) pattern models multiple related entities by introducing a common root table (the supertype) that generates a unified primary key. Specific entity tables (subtypes) use that same ID as their primary key and foreign key. Other tables (like comments or audit logs) reference the supertype ID, preserving complete referential integrity across all subtypes.
</details>

### Q7: Why does SQL Server throw Error 1785 on multiple cascading foreign keys?
<details>
<summary>Click to view answer</summary>

Microsoft SQL Server's storage engine disallows multiple cascade paths or self-referential cascade cycles to prevent infinite loops and non-deterministic delete sequences. If Table A cascades to Table B and Table C, and both Table B and Table C cascade to Table D, SQL Server rejects the DDL. Developers must change one of the paths to `ON DELETE NO ACTION` and handle child cleanup via stored procedures or triggers.
</details>

### Q8: How can you prevent an employee from being assigned as their own manager?
<details>
<summary>Click to view answer</summary>

Add a table-level check constraint:
```sql
CONSTRAINT chk_no_self_manage CHECK (emp_id <> manager_id)
```
This prevents 1-hop circular management loops directly in the database engine.
</details>

---

## Practice Exercises

### Exercise 1: Implement the Exclusive Arc Pattern
Create a `support_tickets` table that can be opened either by a `registered_user` or an `organization`, but never both and never neither, ensuring complete foreign key protection.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE registered_users (user_id INT PRIMARY KEY, email VARCHAR(100) NOT NULL);
CREATE TABLE organizations (org_id INT PRIMARY KEY, name VARCHAR(100) NOT NULL);

CREATE TABLE support_tickets (
    ticket_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    ticket_subject VARCHAR(200) NOT NULL,
    user_id INT NULL REFERENCES registered_users(user_id) ON DELETE CASCADE,
    org_id INT NULL REFERENCES organizations(org_id) ON DELETE CASCADE,
    CONSTRAINT chk_ticket_owner CHECK (
        (user_id IS NOT NULL AND org_id IS NULL) OR 
        (user_id IS NULL AND org_id IS NOT NULL)
    )
);

CREATE INDEX idx_tickets_user ON support_tickets(user_id) WHERE user_id IS NOT NULL;
CREATE INDEX idx_tickets_org ON support_tickets(org_id) WHERE org_id IS NOT NULL;
```
</details>

---

### Exercise 2: Strict 1:1 Employee Security Clearance
Write the DDL for `employee_clearances` such that each employee can have at most one clearance record, and deleting an employee automatically removes their clearance.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE employees (
    emp_id INT PRIMARY KEY,
    full_name VARCHAR(100) NOT NULL
);

CREATE TABLE employee_clearances (
    emp_id INT PRIMARY KEY, -- Shared PK enforces 1:1!
    clearance_level VARCHAR(20) NOT NULL,
    granted_at DATE NOT NULL DEFAULT CURRENT_DATE,
    CONSTRAINT fk_clearance_emp FOREIGN KEY (emp_id) 
        REFERENCES employees(emp_id) ON DELETE CASCADE
);
```
</details>

---

### Exercise 3: Complete M:N Doctor-Hospital Association
Write the DDL for doctors practicing at multiple hospitals, with doctors holding specific department titles at each hospital. Include all necessary performance indexes.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE doctors (doctor_id INT PRIMARY KEY, name VARCHAR(100) NOT NULL);
CREATE TABLE hospitals (hospital_id INT PRIMARY KEY, name VARCHAR(100) NOT NULL);

CREATE TABLE doctor_hospital_affiliations (
    doctor_id INT NOT NULL,
    hospital_id INT NOT NULL,
    department_title VARCHAR(50) NOT NULL,
    PRIMARY KEY (doctor_id, hospital_id),
    FOREIGN KEY (doctor_id) REFERENCES doctors(doctor_id) ON DELETE CASCADE,
    FOREIGN KEY (hospital_id) REFERENCES hospitals(hospital_id) ON DELETE CASCADE
);

-- Reverse index for hospital seeks:
CREATE INDEX idx_affiliations_hospital ON doctor_hospital_affiliations (hospital_id, doctor_id);
```
</details>

---

### Exercise 4: Prevent Cascade Deletion of Bank Customers
A financial schema has `bank_accounts(account_id, customer_id, balance)`. Configure the foreign key so that attempting to delete a customer who still has open accounts is rejected by the database.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE bank_accounts (
    account_id BIGINT PRIMARY KEY,
    customer_id BIGINT NOT NULL,
    balance NUMERIC(15,2) NOT NULL,
    CONSTRAINT fk_accounts_customer FOREIGN KEY (customer_id) 
        REFERENCES customers(customer_id) 
        ON DELETE RESTRICT -- Rejects parent deletion if accounts exist!
);
```
</details>

---

### Exercise 5: Self-Referential Category Tree
Write the DDL for an e-commerce `product_categories` table supporting infinite nested subcategories, with protection against immediate self-parenting.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE product_categories (
    category_id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    category_name VARCHAR(100) NOT NULL,
    parent_category_id INT NULL,
    CONSTRAINT fk_cat_parent FOREIGN KEY (parent_category_id) 
        REFERENCES product_categories(category_id) ON DELETE RESTRICT,
    CONSTRAINT chk_no_self_parent CHECK (category_id <> parent_category_id)
);

CREATE INDEX idx_categories_parent ON product_categories (parent_category_id);
```
</details>

---

### Exercise 6: Fix the Missing Reverse Index Bug
An application team queries `SELECT * FROM enrollments WHERE course_id = 101` and discovers that it performs a full table scan across 10,000,000 rows despite `(student_id, course_id)` being the primary key. Explain the cause and provide the fix.

<details>
<summary>Click to view solution</summary>

**Cause**: The composite primary key index is ordered by `(student_id, course_id)`. In a B-Tree, searching by the second column (`course_id`) without specifying the leading column (`student_id`) prevents an index seek.

**Fix**: Create a secondary index with `course_id` in the leading position:
```sql
CREATE INDEX idx_enrollments_course_id ON enrollments (course_id, student_id);
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                  RELATIONSHIPS CHEAT SHEET                              |
+-------------------+----------------------------+----------------------------------------+
| Relationship Type | Physical Implementation    | Critical Best Practice                 |
+-------------------+----------------------------+----------------------------------------+
| Strict 1:1        | Shared PK (`user_id PK,FK`)| Guaranteed zero duplicate child rows   |
| Standard 1:N      | Child FK + B-Tree index    | Always manually index the FK column    |
| Many-to-Many      | Junction table (A, B)      | Add reverse index on (B, A)            |
| Tree / Hierarchy  | parent_id REFERENCES self  | Add CHECK (id <> parent_id)            |
| Polymorphic Alt   | Exclusive Arc CHECK        | num_nonnulls(t1_id, t2_id) = 1         |
| Supertype Alt     | Base Entity + Subtypes     | Unified PK across all polymorphic types|
+-------------------+----------------------------+----------------------------------------+
```

- **Say NO to Polymorphic Associations**: Use Exclusive Arc or Shared Supertype.
- **Cascading Caution**: Use `ON DELETE RESTRICT` for master and financial data.
- **Junction Rule**: Always index both directions of an M:N bridge table.

---

## Further Reading
- PostgreSQL 17 Documentation: *Constraints - Foreign Keys and Check Constraints*
- Bill Karwin: *SQL Antipatterns: Avoiding the Pitfalls of Database Programming (Polymorphic Associations)*
- Martin Fowler: *Patterns of Enterprise Application Architecture (Class Table Inheritance)*
- Joe Celko: *SQL for Smarties: Advanced SQL Programming (Trees and Hierarchies)*
