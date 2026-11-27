# Naming Conventions & Schema Vocabulary

| Metadata | Details |
| :--- | :--- |
| **Topic** | Database Naming Conventions, Identifier Case-Folding, Singular vs Plural, Audit Columns, Constraint Schemas |
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

In a small personal project, whether a table is named `tblCustomer`, `tbl_customers`, `Customer`, or `customers` may seem like a matter of personal preference. In an enterprise system accessed by dozens of applications, ORMs, BI tools, and data engineering pipelines, arbitrary or inconsistent naming conventions cause friction, operational errors, and subtle bugs.

Consider the consequences of inconsistent naming:
- An engineer queries `orders.customer_id` on one table, but encounters `client_fk`, `customerId`, and `cust_no` across three other tables.
- A developer creates a table using double quotes `"UserAccount"` in PostgreSQL, inadvertently forcing every subsequent query, migration, and reporting script to wrap the identifier in quotes for the lifetime of the database.
- A MySQL database migrated from Windows (case-insensitive NTFS) to Linux (case-sensitive ext4) crashes on startup because queries written for `Orders` fail to find physical file `orders.ibd`.

A rigorous schema vocabulary establishes an unambiguous contract. By adhering to consistent casing standards (`snake_case`), predictable foreign key conventions, standardized audit timestamps, and deterministic constraint naming rules, database systems remain intuitive, self-documenting, and durable.

---

## Core Concepts

### 1. Identifier Casing Standards
- **`snake_case` (The Open-Source SQL Standard)**:
  - All lowercase letters separated by underscores: `customer_orders`, `billing_address_line_1`.
  - Standard in PostgreSQL, MySQL (Linux), SQLite, and modern Python/Ruby/Go frameworks.
- **`PascalCase` / `camelCase` (The Microsoft & Java Standard)**:
  - `CustomerOrders`, `BillingAddress`.
  - Common in SQL Server (T-SQL) and C# / Java enterprise environments.
- **Golden Rule**: Never mix styles within the same database catalog.

### 2. The Case-Folding Quoting Trap
How database engines handle unquoted vs quoted identifiers:
- **PostgreSQL**: Unquoted identifiers are automatically folded to **lowercase**.
  - `CREATE TABLE Users (...)` creates table `users`.
  - `CREATE TABLE "Users" (...)` creates table `"Users"`. Subsequent queries writing `SELECT * FROM users` will fail with relation not found!
- **SQL Server (T-SQL)**: Preserves case, but matching is case-insensitive by default depending on database collation (`SQL_Latin1_General_CP1_CI_AS`).
- **MySQL (InnoDB)**: Depends on the host operating system and `lower_case_table_names` configuration! On Linux, table names correspond to file paths on disk and are strictly case-sensitive.

### 3. Singular vs Plural Table Names
- **Plural Standard (Recommended in PostgreSQL/Web)**: A table represents a collection or mathematical set of entities (`customers`, `orders`, `invoices`).
- **Singular Standard (Alternative)**: A table represents the entity type definition or class (`customer`, `order`, `invoice`).
- *Consistency Rule*: Choose one across the entire organization. Plural is overwhelmingly dominant in RESTful APIs and PostgreSQL ecosystems.

### 4. Deterministic Foreign Key Naming
A foreign key column should always follow the template:
$$\text{<singular\_parent\_table>}\_\text{<parent\_pk\_column>}$$
- Parent: `customers(customer_id)` -> Child FK: `orders.customer_id`.
- If an entity participates multiple times in different roles, prepend the semantic role:
  - `billing_customer_id` and `shipping_customer_id`.
  - `created_by_user_id` and `approved_by_user_id`.

### 5. Deterministic Constraint & Index Naming
Never allow the database engine to auto-generate constraint names (e.g., `orders_check_1`). Use structured prefixes:
- **Primary Key**: `pk_<table_name>` (e.g., `pk_customers`)
- **Foreign Key**: `fk_<child_table>_<parent_table>[_<column>]` (e.g., `fk_orders_customers`)
- **Unique Constraint**: `uq_<table_name>_<column>` (e.g., `uq_users_email`)
- **Check Constraint**: `chk_<table_name>_<rule_name>` (e.g., `chk_orders_positive_total`)
- **Index**: `idx_<table_name>_<column>` (e.g., `idx_orders_customer_id`)

---

## How It Works Internally

How do database engines store and resolve object identifiers in system catalogs?

```
               [ Client Query: CREATE TABLE "CustomerProfiles" (...) ]
                                         │
                                         ▼
                             [ SQL Lexer & Parser ]
                                         │
                    Is the identifier quoted with double quotes?
                                         │
                    ┌────────────────────┴────────────────────┐
                    ▼                                         ▼
            [ YES: Quoted ]                           [ NO: Unquoted ]
                    │                                         │
      Preserves EXACT casing:                   Engine applies Case-Folding:
      Stores "CustomerProfiles" in pg_class.    - PostgreSQL: Folds to 'customerprofiles'
      Every future query MUST quote it!         - Oracle: Folds to 'CUSTOMERPROFILES'
                    │                           - MySQL: Depends on OS filesystem!
                    │                                         │
                    └────────────────────┬────────────────────┘
                                         ▼
                            [ Written to pg_class Catalog ]
```

### Why Quoted Identifiers Are an Architectural Hazard:
When an ORM generates quoted mixed-case table names (`"UserProfile"`):
- Standard SQL CLI queries (`psql`, `mysql`) fail unless the developer manually types quotes.
- Migration scripts, ETL pipelines, and reporting queries must use clumsy escape sequences in code (`"SELECT * FROM \"UserProfile\""`).
- Third-party BI tools and data warehouses (Snowflake, BigQuery) often uppercase identifiers on ingest, breaking synchronization scripts.

---

## Syntax & Usage

```sql
-- Production Schema Naming Template
CREATE TABLE order_invoices (
    -- 1. Primary Key: <singular_table>_id
    order_invoice_id BIGINT GENERATED ALWAYS AS IDENTITY,
    
    -- 2. Foreign Keys: <singular_parent>_<parent_pk>
    customer_id BIGINT NOT NULL,
    shipping_address_id BIGINT NOT NULL,
    
    -- 3. Business Attributes (snake_case, explicit nouns)
    invoice_number VARCHAR(50) NOT NULL,
    total_amount NUMERIC(10,2) NOT NULL,
    tax_amount NUMERIC(10,2) NOT NULL DEFAULT 0.00,
    
    -- 4. Boolean Flags (is_/has_ prefix)
    is_settled BOOLEAN NOT NULL DEFAULT FALSE,
    has_dispute BOOLEAN NOT NULL DEFAULT FALSE,
    
    -- 5. Standard Audit Metadata Columns
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    deleted_at TIMESTAMPTZ NULL,
    version_id INT NOT NULL DEFAULT 1,
    
    -- 6. Explicitly Named Constraints
    CONSTRAINT pk_order_invoices PRIMARY KEY (order_invoice_id),
    CONSTRAINT fk_order_invoices_customers FOREIGN KEY (customer_id) 
        REFERENCES customers(customer_id) ON DELETE RESTRICT,
    CONSTRAINT uq_order_invoices_number UNIQUE (invoice_number),
    CONSTRAINT chk_order_invoices_positive_amount CHECK (total_amount >= 0.00)
);

-- 7. Explicitly Named Indexes
CREATE INDEX idx_order_invoices_customer_id ON order_invoices (customer_id);
CREATE INDEX idx_order_invoices_created_at ON order_invoices (created_at DESC);
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **Unquoted Case-Folding** | Folds to **lowercase** | Depends on OS & `lower_case_table_names` | Case-preserving; case-insensitive match | Preserves case; matches via Collation (CI) |
| **Identifier Quoting** | Double quotes (`"col"`) | Backticks (`` `col` ``) | Double quotes, backticks, or `[col]` | Square brackets (`[col]`) or double quotes |
| **Max Identifier Length** | 63 bytes (`NAMEDATALEN - 1`) | 64 characters | Unlimited | 128 characters |
| **Identity Standard** | `GENERATED ALWAYS AS IDENTITY` | `AUTO_INCREMENT` | `INTEGER PRIMARY KEY` (rowid) | `IDENTITY(1,1)` |

---

## Practical Examples

### Example 1: Basic — Cleaning Up an Inconsistent Legacy Schema
Refactor a legacy table filled with mixed casing, cryptic abbreviations, and missing constraints into a professional schema.

#### Before: Inconsistent Legacy Schema
```sql
CREATE TABLE tbl_Cust_Ord (
    ID INT PRIMARY KEY,
    C_FK INT,
    dt DATE,
    flg CHAR(1),
    amt FLOAT
);
```

#### After: Clean Enterprise Standards
```sql
CREATE TABLE customer_orders (
    customer_order_id BIGINT GENERATED ALWAYS AS IDENTITY,
    customer_id BIGINT NOT NULL,
    order_date DATE NOT NULL DEFAULT CURRENT_DATE,
    is_priority BOOLEAN NOT NULL DEFAULT FALSE,
    order_amount NUMERIC(10,2) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    CONSTRAINT pk_customer_orders PRIMARY KEY (customer_order_id),
    CONSTRAINT fk_customer_orders_customers FOREIGN KEY (customer_id) 
        REFERENCES customers(customer_id),
    CONSTRAINT chk_customer_orders_positive_amount CHECK (order_amount >= 0.00)
);
CREATE INDEX idx_customer_orders_customer_id ON customer_orders (customer_id);
```

---

### Example 2: Real-World — Role-Prefixed Foreign Keys
Model a workflow where an invoice has multiple associations to the `users` table: the author who drafted it, the manager who approved it, and the client who pays it.

```sql
CREATE TABLE billing_invoices (
    invoice_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    
    -- Same parent entity (users), distinguished by semantic role prefixes:
    creator_user_id BIGINT NOT NULL,
    approver_user_id BIGINT NULL,
    recipient_user_id BIGINT NOT NULL,
    
    invoice_total NUMERIC(10,2) NOT NULL,
    
    CONSTRAINT fk_invoices_creator FOREIGN KEY (creator_user_id) REFERENCES users(user_id),
    CONSTRAINT fk_invoices_approver FOREIGN KEY (approver_user_id) REFERENCES users(user_id),
    CONSTRAINT fk_invoices_recipient FOREIGN KEY (recipient_user_id) REFERENCES users(user_id)
);

CREATE INDEX idx_invoices_creator_id ON billing_invoices (creator_user_id);
CREATE INDEX idx_invoices_approver_id ON billing_invoices (approver_user_id);
CREATE INDEX idx_invoices_recipient_id ON billing_invoices (recipient_user_id);
```

---

### Example 3: Edge Case — Truncation of Long Identifiers in PostgreSQL
PostgreSQL enforces a hard limit of **63 bytes** on identifier names (`NAMEDATALEN = 64`). Any identifier exceeding 63 bytes is silently truncated, which can cause duplicate index name collisions!

```sql
-- DANGEROUS: Identifier is 72 bytes long!
-- CREATE INDEX idx_user_notification_delivery_attempt_history_user_id_created_at ...
-- PostgreSQL silently truncates to 63 bytes:
-- "idx_user_notification_delivery_attempt_history_user_id_created_"

-- CORRECT ARCHITECTURAL PRACTICE:
-- Keep names concise, using standardized abbreviations if necessary:
CREATE INDEX idx_notif_attempts_user_created 
ON user_notification_delivery_attempts (user_id, created_at DESC);
```

---

### Example 4: Failure Mode — The Quoted Identifier Trap
A developer used an ORM that wrapped table names in double quotes. Observe the syntax failures that follow.

```sql
-- DDL executed by ORM:
CREATE TABLE "CustomerAccounts" (
    "AccountId" INT PRIMARY KEY,
    "Balance" NUMERIC(10,2)
);

-- DEVELOPER TRIES TO QUERY VIA PSQL:
-- SELECT * FROM customeraccounts;
-- ERROR: relation "customeraccounts" does not exist!

-- SELECT * FROM CustomerAccounts;
-- ERROR: relation "customeraccounts" does not exist! (Unquoted folded to lowercase!)

-- DEVELOPER IS FORCED TO TYPE DOUBLE QUOTES FOREVER:
SELECT "AccountId", "Balance" FROM "CustomerAccounts";

-- REMEDY: Never quote identifiers in DDL! Stick to pure lowercase snake_case.
```

---

### Example 5: Optimized — Standardized Enterprise Audit Columns
A reusable DDL snippet implementing the enterprise five-pillar audit standard across tables.

```sql
CREATE TABLE business_contracts (
    contract_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    contract_title VARCHAR(150) NOT NULL,
    
    -- THE 5-PILLAR ENTERPRISE AUDIT BLOCK:
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    created_by_user_id BIGINT NULL REFERENCES users(user_id),
    deleted_at TIMESTAMPTZ NULL,          -- NULL means active; timestamp means soft-deleted
    lock_version INT NOT NULL DEFAULT 1  -- Incremented on every update for optimistic locking
);
```

---

## Best Practices

1. **Enforce Lowercase `snake_case` Uniformly**: Never use mixed-case, camelCase, or uppercase in table or column identifiers in PostgreSQL or MySQL.
2. **Never Quote Identifiers in DDL**: Avoid `"TableName"` or `` `ColumnName` `` in schema definitions. Letting identifiers remain unquoted allows case-insensitive querying.
3. **Use Explicit Boolean Prefixes**: Always prefix boolean columns with `is_`, `has_`, or `can_` (`is_active`, `has_discount`, `can_export`).
4. **Follow the Standard Constraint Prefix Matrix**: Name constraints explicitly using `pk_`, `fk_`, `uq_`, `chk_`, and `idx_`.
5. **Beware the 63-Byte PostgreSQL Limit**: Keep index and constraint names under 63 bytes to avoid silent truncation by the parser.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Quoting identifiers in PostgreSQL (`"Users"`) | Forces every future query and migration script to use quotes. | Write identifiers unquoted in pure lowercase `snake_case`. |
| 2 | Inconsistent foreign key naming | Using `cust_id` in orders, `customer` in logs, and `client_fk` in billing breaks cognitive consistency. | Standardize strictly on `<singular_parent>_id` (`customer_id`). |
| 3 | Naming tables with reserved SQL keywords | Creating a table named `order`, `user`, or `group` causes constant syntax errors. | Use domain nouns: `orders`, `app_users`, `user_groups`. |
| 4 | Identifiers exceeding 63 bytes | PostgreSQL silently truncates names longer than 63 bytes, causing name collisions. | Keep identifiers under 63 bytes. |
| 5 | Generic ID columns across multi-table joins | Naming the primary key `id` in every table forces repetitive `ON orders.customer_id = customers.id`. | Use explicit PK names: `customer_id`, `order_id`. |
| 6 | Linux vs Windows MySQL casing crash | Tables named in camelCase on Windows crash when restored on Linux. | Set `lower_case_table_names = 1` or standardize on lowercase `snake_case`. |

---

## Performance & Security Notes

### The Security Danger of Enumerable Sequential Identifiers
Naming columns `id` and exposing them directly in public URLs (`GET /api/v1/users/125`) introduces **Insecure Direct Object Reference (IDOR)** vulnerabilities:
- Attackers easily iterate through sequential integers (`125`, `126`, `127`) to scrape entire customer databases.
- Competitors deduce business volume: observing `order_id = 1000` on Monday and `order_id = 1200` on Tuesday reveals the business processed exactly 200 orders.
- **Naming & Schema Remediation**:
  - Keep `user_id BIGINT` as the internal primary key for fast joins.
  - Expose a public column named `public_uuid` or `public_id UUID DEFAULT gen_random_uuid()` in public routes.

---

## Comparison Matrix

| Convention Standard | Style Example | Ecosystem Fit | Case Sensitivity Hazard |
| :--- | :--- | :--- | :--- |
| **`snake_case` (Plural)** | `customer_orders`, `is_active` | PostgreSQL, Linux MySQL, Python, Ruby | **Zero** (Safe across all OS platforms) |
| **`camelCase` (Mixed)** | `customerOrders`, `isActive` | Node.js / JavaScript ORMs | **High** (Requires quoted DDL in Postgres) |
| **`PascalCase`** | `CustomerOrders`, `IsActive` | SQL Server (T-SQL), C# .NET | **Moderate** (Relies on CI collation) |
| **Hungarian Notation** | `tbl_CustomerOrders`, `int_id`| Legacy 1990s Desktop DBs (Access) | **Deprecated** (Violates modern style guides)|

---

## Interview Questions & Deep Dives

### Q1: What happens in PostgreSQL when you execute `CREATE TABLE "Users"` versus `CREATE TABLE Users`?
<details>
<summary>Click to view answer</summary>

- `CREATE TABLE Users` uses an unquoted identifier. PostgreSQL automatically applies case-folding, converting the identifier to lowercase `users` in the `pg_class` catalog. It can subsequently be queried using any case without quotes: `SELECT * FROM users`, `SELECT * FROM Users`, or `SELECT * FROM USERS`.
- `CREATE TABLE "Users"` uses a quoted identifier. PostgreSQL preserves the exact case and stores `"Users"` in `pg_class`. Any subsequent query that omits quotes (e.g., `SELECT * FROM Users`) folds to lowercase `users` and fails with `ERROR: relation "users" does not exist`. Every single query against this table must explicitly wrap the identifier in double quotes.
</details>

### Q2: Why is the plural form (`customers`, `orders`) generally preferred over singular (`customer`, `order`) for table names?
<details>
<summary>Click to view answer</summary>

1. **Relational Theory**: In relational algebra, a relation (table) is a **set of tuples**. Mathematically and grammatically, a collection containing multiple instances is described using a plural noun.
2. **Reserved Word Avoidance**: Singular nouns frequently collide with reserved SQL keywords (e.g., `ORDER` is a reserved keyword for `ORDER BY`, `USER` is a reserved function for the current session user, `GROUP` is reserved for `GROUP BY`). Plural forms (`orders`, `users`, `groups`) rarely collide with SQL syntax.
3. **RESTful Alignment**: Modern web APIs use plural collection endpoints (`GET /api/v1/orders`), aligning the database schema directly with domain models.
</details>

### Q3: What is the maximum identifier length in PostgreSQL, and how does it handle identifiers that exceed it?
<details>
<summary>Click to view answer</summary>

In PostgreSQL, the maximum identifier length is **63 bytes** (defined by the compile-time constant `NAMEDATALEN - 1 = 64 - 1`). If an engineer supplies an identifier longer than 63 bytes, PostgreSQL **silently truncates** it to 63 bytes and issues a notice. This can lead to critical bugs if two long index names share the same first 63 characters: the second index creation will fail with a duplicate key collision.
</details>

### Q4: How should foreign keys be named when a table references the same parent table multiple times?
<details>
<summary>Click to view answer</summary>

When multiple foreign keys reference the same parent entity, you should prefix the parent identifier with the **functional semantic role**:
$$\text{<semantic\_role>}\_\text{<singular\_parent>}\_\text{<parent\_pk>}$$
For example, in a `shipments` table referencing `addresses`:
- `origin_address_id`
- `destination_address_id`
- `return_address_id`
This eliminates ambiguity while preserving foreign key traceability.
</details>

### Q5: Why is naming primary key columns `id` considered an anti-pattern by many database architects?
<details>
<summary>Click to view answer</summary>

While popular in ORMs, naming primary keys simply `id` creates friction in multi-table queries:
```sql
SELECT orders.id, customers.id, products.id FROM ...
```
1. The developer must repeatedly write manual aliases (`orders.id AS order_id`) to avoid ambiguous column name errors in application code.
2. In relational joins, foreign keys are named `customer_id`. Naming the parent key `customer_id` allows utilizing the standard ANSI `USING` clause:
   `FROM orders JOIN customers USING (customer_id)`
3. Naming primary keys explicitly `<singular_table>_id` eliminates ambiguity across views, CTEs, and reporting pipelines.
</details>

### Q6: What does MySQL's `lower_case_table_names` system variable do?
<details>
<summary>Click to view answer</summary>

It controls whether table names are stored and compared case-sensitively:
- `0`: Table names are stored as specified and comparisons are case-sensitive (default on Linux).
- `1`: Table names are stored in lowercase on disk and comparisons are case-insensitive.
- `2`: Table names are stored as specified but compared in lowercase (default on macOS).
Mismatched settings between development (Windows/Mac) and production (Linux) cause sudden deployment failures when queries use mixed-case names.
</details>

### Q7: What are the standard prefixes for constraint naming and why are they necessary?
<details>
<summary>Click to view answer</summary>

Standard prefixes:
- `pk_`: Primary Key
- `fk_`: Foreign Key
- `uq_`: Unique Constraint
- `chk_`: Check Constraint
- `idx_`: Secondary Index
Explicit names are necessary because when a constraint violation occurs, the database engine returns the constraint name in the error payload (e.g., `violates check constraint "chk_orders_positive_total"`). A clean, deterministic name immediately tells the engineer exactly which business rule was violated.
</details>

### Q8: What is the recommended naming pattern for junction tables in M:N relationships?
<details>
<summary>Click to view answer</summary>

Combine the names of the two participating tables in alphabetical or logical order:
- `students` and `courses` -> `course_enrollments` (preferred domain noun) or `courses_students`.
- `users` and `roles` -> `user_roles`.
- `posts` and `tags` -> `post_tags`.
If a meaningful business noun exists (e.g., `enrollments` instead of `courses_students`), the business noun is preferred.
</details>

---

## Practice Exercises

### Exercise 1: Refactor Legacy Quoted Identifiers
Rewrite the following legacy DDL into standard unquoted PostgreSQL `snake_case`:
```sql
CREATE TABLE "InvoiceHeader" (
    "InvoiceID" INT PRIMARY KEY,
    "CustFK" INT,
    "InvoiceDate" DATE,
    "IsPaid" CHAR(1)
);
```

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE invoice_headers (
    invoice_header_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    customer_id BIGINT NOT NULL,
    invoice_date DATE NOT NULL DEFAULT CURRENT_DATE,
    is_paid BOOLEAN NOT NULL DEFAULT FALSE
);
CREATE INDEX idx_invoice_headers_customer_id ON invoice_headers (customer_id);
```
</details>

---

### Exercise 2: Design Standard Enterprise Constraint Names
Write DDL for `product_inventory` with `product_id INT`, `warehouse_id INT`, `quantity INT`, and `reorder_threshold INT`. Ensure all primary keys, foreign keys, unique constraints, and check constraints follow standard naming conventions.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE product_inventory (
    inventory_id BIGINT GENERATED ALWAYS AS IDENTITY,
    product_id BIGINT NOT NULL,
    warehouse_id BIGINT NOT NULL,
    quantity INT NOT NULL,
    reorder_threshold INT NOT NULL DEFAULT 10,
    
    CONSTRAINT pk_product_inventory PRIMARY KEY (inventory_id),
    CONSTRAINT fk_product_inventory_products FOREIGN KEY (product_id) 
        REFERENCES products(product_id) ON DELETE CASCADE,
    CONSTRAINT fk_product_inventory_warehouses FOREIGN KEY (warehouse_id) 
        REFERENCES warehouses(warehouse_id) ON DELETE CASCADE,
    CONSTRAINT uq_product_inventory_product_warehouse UNIQUE (product_id, warehouse_id),
    CONSTRAINT chk_product_inventory_nonnegative_qty CHECK (quantity >= 0),
    CONSTRAINT chk_product_inventory_positive_threshold CHECK (reorder_threshold > 0)
);

CREATE INDEX idx_product_inventory_product_id ON product_inventory (product_id);
CREATE INDEX idx_product_inventory_warehouse_id ON product_inventory (warehouse_id);
```
</details>

---

### Exercise 3: Role-Based Foreign Key Modeling
Model a `flight_manifest` table where each flight has an origin airport and a destination airport, both referencing `airports(airport_code CHAR(3))`.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE flight_manifest (
    flight_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    flight_number VARCHAR(10) NOT NULL,
    origin_airport_code CHAR(3) NOT NULL,
    destination_airport_code CHAR(3) NOT NULL,
    
    CONSTRAINT fk_flight_origin FOREIGN KEY (origin_airport_code) 
        REFERENCES airports(airport_code),
    CONSTRAINT fk_flight_destination FOREIGN KEY (destination_airport_code) 
        REFERENCES airports(airport_code),
    CONSTRAINT chk_flight_different_airports CHECK (origin_airport_code <> destination_airport_code)
);

CREATE INDEX idx_flight_origin ON flight_manifest (origin_airport_code);
CREATE INDEX idx_flight_destination ON flight_manifest (destination_airport_code);
```
</details>

---

### Exercise 4: Detect Identifier Truncation Risk
Review this proposed PostgreSQL index name and evaluate whether it poses an architectural risk:
`idx_customer_relationship_management_communication_logs_contact_id_created_at`

<details>
<summary>Click to view solution</summary>

**Evaluation**: The identifier string has **77 characters** (77 bytes in UTF-8). 

PostgreSQL's identifier limit is **63 bytes**. The engine will silently truncate the name to:
`idx_customer_relationship_management_communication_logs_contac`

If another index is created on `contact_id` and `updated_at`, it will truncate to the **exact same string**, crashing the migration with a duplicate relation error.

**Fix**: Shorten the identifier to under 63 bytes:
`idx_crm_comm_logs_contact_created` (34 bytes).
</details>

---

### Exercise 5: Standardize Boolean Flag Naming
Review the following column names and refactor them to follow standard boolean prefix conventions:
`active`, `verified`, `premium_user`, `discount_eligible`.

<details>
<summary>Click to view solution</summary>

- `active` -> `is_active`
- `verified` -> `is_verified`
- `premium_user` -> `is_premium`
- `discount_eligible` -> `is_discount_eligible` (or `can_receive_discount`)
</details>

---

### Exercise 6: Fixing the Reserved Keyword Table Bug
An engineer writes `CREATE TABLE user (id INT, group INT);` and encounters a syntax error in PostgreSQL. Explain why and provide the corrected DDL.

<details>
<summary>Click to view solution</summary>

**Cause**: `user` and `group` are reserved SQL keywords in ANSI SQL and PostgreSQL. The parser expects `USER` to refer to the current database session user and `GROUP` to introduce a `GROUP BY` clause.

**Fix**: Use descriptive plural nouns that do not collide with keywords:
```sql
CREATE TABLE app_users (
    user_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_group_id BIGINT NOT NULL
);
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                               NAMING CONVENTIONS CHEAT SHEET                            |
+-------------------+----------------------------+----------------------------------------+
| Object Type       | Convention Pattern         | Production Example                     |
+-------------------+----------------------------+----------------------------------------+
| Tables            | Lowercase plural snake     | customers, order_line_items            |
| Primary Key       | <singular_table>_id        | customer_id, invoice_id                |
| Foreign Key       | <singular_parent>_id       | customer_id, parent_category_id        |
| Role-Prefixed FK  | <role>_<singular_parent>_id| author_user_id, approver_user_id       |
| Booleans          | is_<state> / has_<feature> | is_active, is_verified, has_discount   |
| Primary Key Cstr  | pk_<table>                 | pk_order_invoices                      |
| Foreign Key Cstr  | fk_<child>_<parent>        | fk_orders_customers                    |
| Unique Cstr       | uq_<table>_<col>           | uq_users_email                         |
| Check Cstr        | chk_<table>_<rule>         | chk_orders_positive_total              |
| Index             | idx_<table>_<cols>         | idx_orders_customer_date               |
+-------------------+----------------------------+----------------------------------------+
```

- **Unquoted Mandate**: Always use unquoted lowercase `snake_case` in PostgreSQL.
- **Audit Columns**: Include `created_at`, `updated_at`, and `deleted_at`.
- **Length Boundary**: Keep identifier names strictly under 63 bytes.

---

## Further Reading
- PostgreSQL 17 Documentation: *SQL Syntax - Identifiers and Key Words*
- SQLFluff Documentation: *Rules Reference - Capitalisation and Naming Conventions*
- Joe Celko: *SQL Programming Style (Chapter 3: Naming Standards)*
- Simon Holywell: *SQL Style Guide (Standardizing SQL Conventions)*
