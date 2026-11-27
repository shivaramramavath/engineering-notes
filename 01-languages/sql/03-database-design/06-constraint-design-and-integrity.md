# Constraint Design & Relational Integrity

| Metadata | Details |
| :--- | :--- |
| **Topic** | Declarative Data Integrity, CHECK, UNIQUE, Partial Constraints, PostgreSQL Exclusion Constraints, Deferred Constraints |
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

In modern software development, application code is ephemeral, polyglot, and distributed. An enterprise database is accessed by dozens of microservices, background Celery/Sidekiq workers, administrative scripts, BI reporting tools, and direct migrations.

Relying on application-level validation (e.g., ORM validations in Rails, Django, or Prisma) to safeguard data integrity is an architectural fallacy:
- Race conditions bypass application checks (two users booking the same hotel room simultaneously).
- Background scripts and direct database updates bypass application models completely.
- A single bug in one microservice can corrupt tables accessed by 10 other services.

**Declarative Relational Constraints** push business invariants directly into the database engine. Enforced at the physical storage layer within ACID transaction boundaries, constraints guarantee that no invalid state can ever be committed to disk.

From fundamental `CHECK` and `UNIQUE` constraints to advanced **PostgreSQL Exclusion Constraints (`EXCLUDE USING gist`)** that mathematically eliminate double-booking bugs, constraint design is the ultimate defensive perimeter of software engineering.

---

## Core Concepts

### 1. The Hierarchy of Relational Integrity
- **Entity Integrity**: Every table must have a primary key; primary key attributes must be unique and strictly non-null.
- **Referential Integrity**: Foreign keys must reference valid, existing parent tuples (`REFERENCES parent(id)`).
- **Domain Integrity**: Column values must adhere to defined types, ranges, formats, and non-nullability (`NOT NULL`, `CHECK`).
- **User-Defined / Business Integrity**: Multi-column rules, temporal non-overlap, and conditional uniqueness (`EXCLUDE`, Partial Indexes).

### 2. The `CHECK` Constraint
A `CHECK` constraint specifies a boolean condition that every row must satisfy:
```sql
CHECK (unit_price >= 0.00)
CHECK (end_date >= start_date)
CHECK (email ~* '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$')
```
- **Null Semantics in CHECK**: A `CHECK` constraint passes if the expression evaluates to `TRUE` **OR** `UNKNOWN` (`NULL`). To disallow nulls, the column must be marked `NOT NULL`!

### 3. `UNIQUE` Constraints & The Null Trap
Under traditional ANSI SQL, `NULL <> NULL`. Consequently, a standard `UNIQUE` constraint permits multiple rows containing `NULL`!
- **ANSI SQL:2023 / PostgreSQL 15+ Feature**:
  ```sql
  UNIQUE NULLS NOT DISTINCT
  ```
  Treats `NULL` as a single value, permitting at most one `NULL` in the unique column.

### 4. Partial Unique Constraints (Conditional Indexes)
When soft deletes are implemented, a standard `UNIQUE(email)` constraint fails: once a user is soft-deleted, another user cannot register with that same email!
- **Solution: Partial Unique Index**:
  ```sql
  CREATE UNIQUE INDEX idx_unique_active_email ON users (email) WHERE deleted_at IS NULL;
  ```
  Uniqueness is strictly enforced among active users; historical soft-deleted users can exist with duplicate emails.

### 5. PostgreSQL Exclusion Constraints (`EXCLUDE`)
A generalized extension of uniqueness: while `UNIQUE` tests equality (`=`), `EXCLUDE` allows arbitrary comparison operators (such as **overlaps `&&`** on ranges).
- Primary use case: **Preventing overlapping bookings/reservations** directly in the database:
  ```sql
  EXCLUDE USING gist (room_id WITH =, reservation_period WITH &&)
  ```
  If an incoming booking overlaps in time for the same room, the database automatically rejects the transaction with an exclusion violation.

### 6. Deferrable Constraints
Constraints are evaluated immediately after each statement by default.
- **`DEFERRABLE INITIALLY DEFERRED`**: Delays validation until the transaction issues `COMMIT`. Essential for resolving circular dependencies or intermediate batch updates.

---

## How It Works Internally

```
                   [ Incoming TUPLE: INSERT INTO reservations ]
                                         │
                                         ▼
                            [ 1. NOT NULL Verification ]
                            Inspects null bitmap in tuple header.
                                         │
                                         ▼
                             [ 2. Domain CHECK Validation ]
                             Executes row expression in CPU register.
                                         │
                                         ▼
                         [ 3. B-Tree Unique Index Check ]
                         Probes B-Tree for key collisions.
                                         │
                                         ▼
                      [ 4. GiST Exclusion Constraint Seek ]
                      (If EXCLUDE is defined)
                      Probes R-Tree / GiST index for bounding box overlaps:
                      (room_id = 101 AND time_period && '[2026-06-01, 2026-06-05)')
                                         │
                         ┌───────────────┴───────────────┐
                         ▼                               ▼
                 [ Collision Found ]             [ Clean Space ]
                 Aborts transaction              Writes tuple to WAL
                 Error: 23P01 exclusion_violation and commits!
```

1. **In-Memory Check Evaluation**: `NOT NULL` and `CHECK` constraints are evaluated directly in CPU memory by the tuple parser before writing to the Write-Ahead Log (WAL). Their performance overhead is negligible (nanoseconds).
2. **Index-Backed Constraints**: `UNIQUE` and `EXCLUDE` constraints physically construct underlying indexes (B-Tree for Unique, GiST for Exclude). The validation check occurs as a natural consequence of the index insertion seek.

---

## Syntax & Usage

```sql
-- Comprehensive Constraint Demonstration
CREATE TABLE hotel_reservations (
    reservation_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    guest_email VARCHAR(255) NOT NULL,
    room_number INT NOT NULL,
    stay_dates TSRANGE NOT NULL, -- Timestamp range [start, end)
    nightly_rate NUMERIC(10,2) NOT NULL,
    discount_pct NUMERIC(4,2) NOT NULL DEFAULT 0.00,
    
    -- 1. Domain Range Checks
    CONSTRAINT chk_positive_rate CHECK (nightly_rate > 0.00),
    CONSTRAINT chk_discount_range CHECK (discount_pct BETWEEN 0.00 AND 1.00),
    
    -- 2. Email Format Check (Regex)
    CONSTRAINT chk_valid_email CHECK (guest_email ~* '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$'),
    
    -- 3. PostgreSQL Exclusion Constraint (Guarantees zero double-booking!)
    CONSTRAINT no_overlapping_room_bookings 
        EXCLUDE USING gist (room_number WITH =, stay_dates WITH &&)
);
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **`CHECK` Constraints** | Fully enforced | Enforced (since 8.0.16) | Fully enforced | Fully enforced |
| **`UNIQUE NULLS NOT DISTINCT`**| Supported (PG 15+) | **Not Supported** | Supported (SQLite 3.39+) | Supported natively |
| **Exclusion Constraints (`EXCLUDE`)**| Native `EXCLUDE USING gist` | **Not Supported** (Requires trigger or lock)| **Not Supported** | **Not Supported** |
| **Partial Unique Indexes** | `CREATE UNIQUE INDEX ... WHERE`| **Not Supported** (Use generated cols)| Supported | Supported (`CREATE UNIQUE ... WHERE`)|
| **`DEFERRABLE` Constraints** | Supported on PK, FK, Unique | **Not Supported** | Supported on FKs | **Not Supported** |

---

## Practical Examples

### Example 1: Basic — Domain Integrity via CHECK Constraints
Ensure employee salary and status rules are mathematically enforced.

```sql
CREATE TABLE company_employees (
    emp_id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    emp_name VARCHAR(100) NOT NULL,
    employment_status VARCHAR(20) NOT NULL,
    base_salary NUMERIC(10,2) NOT NULL,
    bonus NUMERIC(10,2) NULL,
    
    -- Status whitelist
    CONSTRAINT chk_emp_status CHECK (
        employment_status IN ('FULL_TIME', 'PART_TIME', 'CONTRACTOR', 'TERMINATED')
    ),
    -- Salary boundary
    CONSTRAINT chk_min_salary CHECK (base_salary >= 30000.00),
    -- Cross-column rule: Bonus cannot exceed base salary
    CONSTRAINT chk_bonus_cap CHECK (bonus IS NULL OR bonus <= base_salary)
);

INSERT INTO company_employees (emp_name, employment_status, base_salary, bonus)
VALUES ('Sarah Connor', 'FULL_TIME', 85000.00, 15000.00);

-- Fails check constraint chk_min_salary:
-- INSERT INTO company_employees (emp_name, employment_status, base_salary) VALUES ('Bad', 'FULL_TIME', 10000.00);
-- ERROR: new row for relation "company_employees" violates check constraint "chk_min_salary"
```

---

### Example 2: Real-World — Solving the Double-Booking Bug via PostgreSQL `EXCLUDE`
Two users attempt to book Room 101 for overlapping dates. Demonstrate that the database guarantees zero double-booking without application-level distributed locks.

```sql
-- Requires btree_gist extension for mixed scalar and range exclusion
CREATE EXTENSION IF NOT EXISTS btree_gist;

CREATE TABLE conference_rooms (
    booking_id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    room_id INT NOT NULL,
    booking_window TSRANGE NOT NULL,
    CONSTRAINT no_double_booking 
        EXCLUDE USING gist (room_id WITH =, booking_window WITH &&)
);

-- First booking: June 1 to June 5
INSERT INTO conference_rooms (room_id, booking_window) VALUES
(101, tsrange('2026-06-01 10:00:00', '2026-06-05 12:00:00', '[)'));

-- Second booking: June 4 to June 8 (Overlaps on June 4-5!)
-- Attempted insert throws exclusion violation:
-- INSERT INTO conference_rooms (room_id, booking_window) VALUES
-- (101, tsrange('2026-06-04 09:00:00', '2026-06-08 12:00:00', '[)'));
-- ERROR: conflicting key value violates exclusion constraint "no_double_booking"
-- DETAIL: Key (room_id, booking_window)=(101, ["2026-06-04 09:00:00","2026-06-08 12:00:00")) conflicts with existing key...
```

---

### Example 3: Edge Case — Null Semantics in CHECK Constraints
Demonstrating that `CHECK` evaluates to `TRUE` if any operand yields `NULL` (`UNKNOWN`).

```sql
CREATE TABLE account_limits (
    account_id INT PRIMARY KEY,
    credit_score INT NULL,
    -- DANGEROUS ASSUMPTION:
    -- Developer intends credit_score to be >= 650
    CONSTRAINT chk_good_credit CHECK (credit_score >= 650)
);

-- SURPRISE: This INSERT succeeds!
INSERT INTO account_limits (account_id, credit_score) VALUES (1, NULL);

-- Why? Because (NULL >= 650) evaluates to UNKNOWN!
-- SQL CHECK constraints only reject rows when the result is strictly FALSE.
-- FIX: Explicitly enforce NOT NULL alongside CHECK:
ALTER TABLE account_limits ALTER COLUMN credit_score SET NOT NULL;
```

---

### Example 4: Failure Mode — The Soft-Delete Unique Collision
Demonstrating how soft deletes break standard unique constraints, and fixing it with a partial unique index.

```sql
CREATE TABLE app_users (
    user_id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    email VARCHAR(100) NOT NULL,
    deleted_at TIMESTAMPTZ NULL
);

-- WRONG: Standard unique constraint includes soft-deleted rows!
-- ALTER TABLE app_users ADD CONSTRAINT uq_email UNIQUE (email);
-- If Alice deletes her account, she can never register again with that email!

-- CORRECT: Partial Unique Index in PostgreSQL / SQLite / SQL Server:
CREATE UNIQUE INDEX idx_unique_active_user_email 
ON app_users (email) 
WHERE deleted_at IS NULL;

-- 1. Alice registers
INSERT INTO app_users (email) VALUES ('alice@example.com');

-- 2. Alice deletes account (soft delete)
UPDATE app_users SET deleted_at = CURRENT_TIMESTAMP WHERE email = 'alice@example.com';

-- 3. Alice re-registers with the same email! (SUCCEEDS!)
INSERT INTO app_users (email) VALUES ('alice@example.com');
```

```
Verify app_users:
 user_id |       email       |          deleted_at           
---------+-------------------+-------------------------------
       1 | alice@example.com | 2026-11-26 12:00:00.000000+00
       2 | alice@example.com | NULL
(2 rows)
```

---

### Example 5: Optimized — Deferring Unique Constraint During Row Swapping
Swapping the ordinal ranks of two items within a single transaction without violating uniqueness.

```sql
CREATE TABLE playlist_tracks (
    track_id INT PRIMARY KEY,
    position_rank INT NOT NULL,
    CONSTRAINT uq_rank UNIQUE (position_rank) DEFERRABLE INITIALLY DEFERRED
);

INSERT INTO playlist_tracks VALUES (101, 1), (102, 2);

-- Swap ranks 1 and 2 in a transaction:
BEGIN;
UPDATE playlist_tracks SET position_rank = 2 WHERE track_id = 101; -- Temporarily two rank 2s!
UPDATE playlist_tracks SET position_rank = 1 WHERE track_id = 102;
COMMIT; -- Engine validates uniqueness here: SUTISFIES CONSTRAINT!
```

---

## Best Practices

1. **Pair `CHECK` with `NOT NULL`**: Because `CHECK` conditions pass on `NULL` (`UNKNOWN`), always mark columns `NOT NULL` if values must be present.
2. **Use Exclusion Constraints for Temporal Allocations**: Never attempt to prevent overlapping bookings via application queries (`SELECT ... WHERE end > start`). Use PostgreSQL `EXCLUDE USING gist` with range types (`TSRANGE`, `DATERANGE`).
3. **Use Partial Unique Indexes for Soft-Delete Tables**: Enforce uniqueness only where `deleted_at IS NULL` to permit deleted entities to be re-created.
4. **Name Every Constraint Explicitly**: Never let database engines auto-generate constraint names (`table_check_1`). Use standard conventions: `chk_<table>_<field>`, `fk_<table>_<parent>`, `uq_<table>_<field>`.
5. **Use Deferrable Constraints for Batch Updates**: When swapping ranks, positions, or resolving circular foreign keys, declare constraints as `DEFERRABLE INITIALLY DEFERRED`.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Assuming `CHECK` forbids `NULL` | `CHECK (val > 0)` passes if `val` is `NULL` (evaluates to UNKNOWN). | Add `NOT NULL` constraint to the column. |
| 2 | Standard `UNIQUE` on soft-delete tables | Prevents re-registration of emails or usernames that were deleted. | Use a partial unique index: `WHERE deleted_at IS NULL`. |
| 3 | Application-level booking validation | Race conditions allow two concurrent transactions to book the same room. | Use PostgreSQL Exclusion Constraints (`EXCLUDE USING gist`). |
| 4 | Unnamed constraints | Makes migration rollbacks and error handling difficult when referencing auto-generated names. | Explicitly name all constraints: `CONSTRAINT chk_...`. |
| 5 | Cross-table CHECK constraints | `CHECK` can only inspect the current row; subqueries are forbidden in `CHECK`. | Use Foreign Keys, Materialized Views, or Triggers. |
| 6 | Forgetting `btree_gist` extension | PostgreSQL raises an error when mixing scalar equality (`room_id WITH =`) with range overlaps in `EXCLUDE`. | Execute `CREATE EXTENSION btree_gist;`. |

---

## Performance & Security Notes

### The Cost of Constraint Validation
- **`NOT NULL` and `CHECK`**: Incur **zero measurable overhead**. The engine validates conditions in CPU registers during tuple construction before writing to disk.
- **`UNIQUE` and `EXCLUDE`**: Require B-Tree or GiST index lookups. The index seek occurs during insertion anyway; adding the constraint imposes less than 1% overhead compared to an unconstrained indexed write.
- **Security Invariant**: Constraints are the ultimate line of defense against SQL Injection and data tampering. Even if malicious application code injects invalid values, the database engine aborts the transaction at the storage boundary.

---

## Comparison Matrix

| Constraint Type | Physical Implementation | Enforces | Failure Severity |
| :--- | :--- | :--- | :--- |
| **`NOT NULL`** | Null Bitmap Bit | Presence of data | Transaction Abort |
| **`CHECK`** | Boolean CPU expression | Domain rules, value ranges, regex | Transaction Abort |
| **`UNIQUE`** | B-Tree Index | Cardinality, single instance | Transaction Abort |
| **`EXCLUDE`** | GiST / SP-GiST Index | Generalized overlaps (ranges, geometry)| Transaction Abort |
| **`FOREIGN KEY`** | Parent B-Tree Index Probe | Referential integrity | Transaction Abort |

---

## Interview Questions & Deep Dives

### Q1: Why does a `CHECK (salary > 0)` constraint allow `NULL` values to be inserted?
<details>
<summary>Click to view answer</summary>

Under the ANSI SQL standard, a `CHECK` constraint validates whether the expression evaluates to `FALSE`. If the expression evaluates to `TRUE` **or** `UNKNOWN`, the constraint is considered satisfied. When `salary` is `NULL`, `NULL > 0` evaluates to `UNKNOWN` under Three-Valued Logic (3VL). Because `UNKNOWN` is not `FALSE`, the database accepts the row. To disallow nulls, you must explicitly mark the column `NOT NULL`.
</details>

### Q2: What is an Exclusion Constraint in PostgreSQL and what problem does it solve?
<details>
<summary>Click to view answer</summary>

An Exclusion Constraint (`EXCLUDE USING gist (...)`) is a generalized constraint that guarantees that if any two rows are compared on specified columns using specified operators, at least one operator comparison will return `FALSE`. 

It solves the classic **temporal overlap and scheduling problem** (preventing double-bookings of rooms, equipment, or doctor appointments). While a standard `UNIQUE` constraint can only enforce equality (`=`), an exclusion constraint can enforce that ranges do not overlap (`&&`), completely eliminating race conditions at the database layer.
</details>

### Q3: What is the difference between `UNIQUE NULLS DISTINCT` and `UNIQUE NULLS NOT DISTINCT`?
<details>
<summary>Click to view answer</summary>

- `UNIQUE NULLS DISTINCT` (Historical SQL standard default): Treats each `NULL` as distinct (`NULL <> NULL`). As a result, a table with a unique constraint on `tax_id` can store an infinite number of rows with `tax_id = NULL`.
- `UNIQUE NULLS NOT DISTINCT` (ANSI SQL:2023 / PostgreSQL 15+): Treats `NULL` values as identical for uniqueness purposes. At most **one row** with `NULL` is permitted in the unique column.
</details>

### Q4: How does a Partial Unique Index solve the Soft-Delete problem?
<details>
<summary>Click to view answer</summary>

When a user deletes their account in a soft-delete architecture, `deleted_at` is populated with a timestamp rather than physically deleting the row. If a standard `UNIQUE(email)` constraint is applied, the soft-deleted row occupies the unique slot, preventing any future user from registering with that email. 

A partial unique index `CREATE UNIQUE INDEX ON users(email) WHERE deleted_at IS NULL` restricts uniqueness checking exclusively to active rows, allowing multiple historical soft-deleted records to share the same email.
</details>

### Q5: What is a Deferrable Constraint and why is it useful?
<details>
<summary>Click to view answer</summary>

A deferrable constraint allows constraint validation to be postponed from statement execution time to transaction `COMMIT` time (`DEFERRABLE INITIALLY DEFERRED`). It is useful when:
1. Two tables have mutual foreign keys (circular dependency), requiring both rows to be inserted before foreign keys are validated.
2. Swapping unique ordinal positions (e.g., swapping priority 1 and priority 2 in a playlist) where an intermediate step temporarily creates duplicate values.
</details>

### Q6: Can a `CHECK` constraint reference columns in other tables?
<details>
<summary>Click to view answer</summary>

No. Under ANSI SQL, a `CHECK` constraint can only reference attributes of the **current row** in the current table; it cannot contain subqueries, calls to volatile functions (like `random()` or `NOW()`), or references to other tables. To enforce cross-table business rules, engineers must use foreign keys, PostgreSQL exclusion constraints, or database validation triggers.
</details>

### Q7: What index type is required to support PostgreSQL Exclusion Constraints on range types?
<details>
<summary>Click to view answer</summary>

Exclusion constraints require a **GiST (Generalized Search Tree)** or **SP-GiST** index because standard B-Trees only support linear ordering (`<`, `=`, `>`) and cannot evaluate spatial or temporal overlap operators (`&&`). To mix scalar equality (`=`) with range overlap (`&&`) in the same exclusion constraint, the `btree_gist` extension must be enabled.
</details>

### Q8: What happens if you add a `CHECK` constraint to a table with 10 million existing rows?
<details>
<summary>Click to view answer</summary>

Adding `ALTER TABLE tbl ADD CONSTRAINT chk_val CHECK (val > 0);` takes an `ACCESS EXCLUSIVE` lock and performs a full sequential scan of all 10 million rows to validate existing data, blocking all reads and writes. 

**Zero-Downtime Safe Pattern**:
1. Add the constraint without validation:
   `ALTER TABLE tbl ADD CONSTRAINT chk_val CHECK (val > 0) NOT VALID;` (Takes instant lock).
2. Validate existing data in the background without blocking concurrent traffic:
   `ALTER TABLE tbl VALIDATE CONSTRAINT chk_val;`
</details>

---

## Practice Exercises

### Exercise 1: Multi-Column Age & Driving License Constraint
Create a `drivers` table with `age INT` and `license_number VARCHAR(30)`. Enforce that:
1. `age` must be between 16 and 100.
2. If `age < 18`, `license_number` must start with `'PROV-'` (provisional license).

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE drivers (
    driver_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    age INT NOT NULL CHECK (age BETWEEN 16 AND 100),
    license_number VARCHAR(30) NOT NULL UNIQUE,
    CONSTRAINT chk_provisional_license CHECK (
        age >= 18 OR license_number LIKE 'PROV-%'
    )
);
```
</details>

---

### Exercise 2: Prevent Overlapping Doctor Appointments
Using PostgreSQL's `EXCLUDE` constraint and `TSRANGE`, create a `doctor_schedules` table ensuring that a doctor cannot be scheduled for two overlapping appointments.

<details>
<summary>Click to view solution</summary>

```sql
CREATE EXTENSION IF NOT EXISTS btree_gist;

CREATE TABLE doctor_schedules (
    schedule_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    doctor_id BIGINT NOT NULL,
    appointment_window TSRANGE NOT NULL,
    CONSTRAINT no_overlapping_appointments 
        EXCLUDE USING gist (doctor_id WITH =, appointment_window WITH &&)
);
```
</details>

---

### Exercise 3: Single Active Subscription Per User
In a subscription billing platform `subscriptions(sub_id, user_id, plan_id, is_active)`, ensure that a user can have at most **one active subscription** at any time, but can have unlimited historical inactive subscriptions.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE subscriptions (
    sub_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id BIGINT NOT NULL,
    plan_id VARCHAR(50) NOT NULL,
    is_active BOOLEAN NOT NULL DEFAULT TRUE
);

-- Partial Unique Index:
CREATE UNIQUE INDEX idx_single_active_sub_per_user 
ON subscriptions (user_id) 
WHERE is_active = TRUE;
```
</details>

---

### Exercise 4: Enforce Exactly One Null in ANSI SQL:2023
Write DDL for `tenants(tenant_id, subdomain)` in PostgreSQL 15+ ensuring that `subdomain` is unique, but unlike traditional SQL, permits **at most one** row where `subdomain IS NULL`.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE tenants (
    tenant_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    subdomain VARCHAR(50) NULL,
    CONSTRAINT uq_subdomain UNIQUE NULLS NOT DISTINCT (subdomain)
);
```
</details>

---

### Exercise 5: Zero-Downtime Check Constraint Addition
Write the two-step PostgreSQL SQL commands to add a `CHECK (amount > 0)` constraint to a table with 50 million rows without taking an extended exclusive table lock.

<details>
<summary>Click to view solution</summary>

```sql
-- Step 1: Add constraint without validating existing rows (instant lock):
ALTER TABLE orders 
ADD CONSTRAINT chk_positive_amount CHECK (amount > 0.00) NOT VALID;

-- Step 2: Validate existing data in background without blocking concurrent traffic:
ALTER TABLE orders 
VALIDATE CONSTRAINT chk_positive_amount;
```
</details>

---

### Exercise 6: Fix the Nullable CHECK Bug
An engineer writes:
```sql
CREATE TABLE inventory (
    item_id INT PRIMARY KEY,
    quantity INT CHECK (quantity >= 0)
);
```
Explain why `INSERT INTO inventory VALUES (1, NULL);` succeeds, and provide the fix.

<details>
<summary>Click to view solution</summary>

**Cause**: `NULL >= 0` evaluates to `UNKNOWN`. SQL `CHECK` constraints only fail when the condition evaluates to `FALSE`. An evaluation of `UNKNOWN` allows the row to be inserted.

**Fix**: Add the `NOT NULL` constraint to the column:
```sql
CREATE TABLE inventory (
    item_id INT PRIMARY KEY,
    quantity INT NOT NULL CHECK (quantity >= 0)
);
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                  CONSTRAINTS CHEAT SHEET                                |
+-------------------+----------------------------+----------------------------------------+
| Constraint        | Purpose                    | Critical Gotcha                        |
+-------------------+----------------------------+----------------------------------------+
| NOT NULL          | Mandates value presence    | Most important constraint in RDBMS     |
| CHECK             | Domain validation, regex   | Passes on NULL! Pair with NOT NULL     |
| UNIQUE            | Unique key enforcement     | Traditional UNIQUE allows multiple NULL|
| NULLS NOT DISTINCT| Enforces single NULL limit | PostgreSQL 15+, SQL Server native      |
| Partial Unique    | WHERE deleted_at IS NULL   | Solves the soft-delete uniqueness trap |
| EXCLUDE           | Prevents temporal overlaps | Eliminates double-booking race conds   |
| DEFERRABLE        | Validates on COMMIT        | Resolves circular FKs & rank swapping  |
+-------------------+----------------------------+----------------------------------------+
```

- **Defensive Rule**: Push data invariants into the database; never trust application layer checks alone.
- **Booking Rule**: Use `EXCLUDE USING gist` for scheduling and temporal ranges.
- **Migration Rule**: Use `NOT VALID` followed by `VALIDATE CONSTRAINT` on large production tables.

---

## Further Reading
- PostgreSQL 17 Documentation: *Constraints - Check, Unique, Foreign Key, Exclusion*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 11.7 `<table constraint definition>`
- Markus Winand: *Modern SQL - UNIQUE NULLS NOT DISTINCT in PostgreSQL 15*
- Bruce Momjian: *PostgreSQL Exclusion Constraints: Solving Complex Overlap Problems*
