# Production Schema Patterns: Soft Deletes, Auditing, Temporal Tables, and Multi-Tenancy

| Metadata | Details |
| :--- | :--- |
| **Topic** | Enterprise Schema Patterns, Soft Deletes (deleted_at), Audit Logging, SCD Types 1-4, Multi-Tenancy |
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

Relational theory models data as the current, timeless state of truth. However, real-world enterprise software lives in a complex, regulatory-heavy reality:
- Users accidentally click "Delete" and demand immediate data recovery.
- Compliance frameworks (SOC 2, HIPAA, GDPR, PCI-DSS) mandate that every modification to sensitive financial or patient data must be immutably audited with a forensic trail ("Who changed what, when, and from what value?").
- Insurance, tax, and employment systems require **Temporal Validity**: evaluating what a customer's address or contract status was on a specific historical date (Slowly Changing Dimensions).
- SaaS business models require isolating thousands of independent corporate customers (**Multi-Tenancy**) within a single infrastructure footprint.

When software teams implement these requirements ad-hoc—scattering boolean `is_deleted` flags across tables without index support, or writing fragile application-level audit scripts—databases suffer from index bloat, data corruption, and catastrophic security breaches where Tenant A reads Tenant B's data.

Mastering production schema patterns equips engineers with battle-tested architectural templates for soft deletes, JSONB audit shadow logging, temporal validity, and Row-Level Security (RLS) multi-tenancy.

---

## Core Concepts

### 1. Soft Deletes (`deleted_at` vs `is_deleted`)
Soft deletion preserves deleted records in the database rather than issuing physical `DELETE` statements.
- **`is_deleted BOOLEAN` (The Anti-Pattern)**: Contains minimal information (lacks timestamp of deletion), and forces boolean filtering that defeats standard B-Tree indexing.
- **`deleted_at TIMESTAMPTZ NULL` (The Enterprise Standard)**:
  - If `NULL`: The record is active.
  - If populated: Records the exact millisecond the entity was tombstoned.
  - Enforced via **Partial Indexes**: `CREATE UNIQUE INDEX ... WHERE deleted_at IS NULL`.

### 2. Audit Trails & Change Data Capture (CDC)
Audit logging records every `INSERT`, `UPDATE`, and `DELETE` operation.
- **Shadow / History Table Pattern**: A mirror table `audit_logs` storing:
  - Table name, record PK, operation type (`INSERT`, `UPDATE`, `DELETE`), actor user ID, timestamp, and the `OLD` and `NEW` row snapshots stored as `JSONB`.
- **Database Trigger Implementation**: Captures all changes unconditionally, even if updates are made by DBAs via direct `psql` scripts.

### 3. Temporal Validity & Slowly Changing Dimensions (SCD)
How changes to dimension attributes (e.g., employee salary, customer address) are tracked over time:
- **SCD Type 1 (Overwrite)**: Old value is replaced. Zero history preserved.
- **SCD Type 2 (Versioned Rows)**: Each change inserts a new row with validity date boundaries: `valid_from TIMESTAMPTZ`, `valid_to TIMESTAMPTZ`, and `is_current BOOLEAN`.
- **SCD Type 3 (Previous Column)**: Adds a `previous_val` column. Only preserves 1 level of history.
- **SCD Type 4 (Historical Audit Table)**: Base table holds current state; changes are archived to a dedicated historical table.
- **System-Versioned Temporal Tables (ANSI SQL:2011)**: Native database-managed temporal versioning (supported natively in Microsoft SQL Server).

### 4. Multi-Tenancy Architecture Models
1. **Shared Database, Shared Schema (Row-Level Isolation)**:
   - Every table contains a `tenant_id` column.
   - Enforced automatically at the database engine layer via PostgreSQL **Row-Level Security (RLS)**.
   - Lowest infrastructure cost, highest scalability.
2. **Shared Database, Separate Schema per Tenant**:
   - Each tenant has its own isolated schema (`tenant_101.orders`, `tenant_102.orders`).
   - Tenant isolation managed via `SET search_path = tenant_101`.
3. **Separate Database per Tenant**:
   - Complete physical isolation; highest cost, connection pool overhead.

---

## How It Works Internally

How does PostgreSQL Row-Level Security (RLS) enforce multi-tenant isolation?

```
    [ Client App sets Tenant Context: SET app.current_tenant_id = 'org_99'; ]
                                       │
                                       ▼
                   [ User Query: SELECT * FROM documents; ]
                                       │
                                       ▼
                         [ Query Parser & Rewriter ]
                                       │
                         Is RLS enabled on 'documents'?
                                       │
                                       ▼
               [ RLS Policy Injection (The Invisible Filter) ]
          Policy: USING (tenant_id = current_setting('app.current_tenant_id')::bigint)
                                       │
                                       ▼
                      [ Rewritten Abstract Syntax Tree ]
          SELECT * FROM documents 
          WHERE tenant_id = 99;  <-- Automatically injected by engine!
                                       │
                                       ▼
                         [ B-Tree Index Seek on tenant_id ]
          Engine guarantees Tenant 99 can NEVER read or write Tenant 100 tuples!
```

1. **Security Invariant**: Even if a developer accidentally omits `WHERE tenant_id = ?` in application code, or a SQL injection vulnerability exposes the query, the database engine **rewrites the AST internally** to append the tenant predicate.
2. **B-Tree Optimization**: All queries on RLS tables utilize composite indexes starting with `tenant_id` (`idx_docs_tenant_created ON documents (tenant_id, created_at DESC)`), ensuring that the tenant filter acts as an instant B-Tree partition seek.

---

## Syntax & Usage

### 1. Robust Soft Deletes with Partial Index
```sql
CREATE TABLE projects (
    project_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    project_name VARCHAR(100) NOT NULL,
    slug VARCHAR(100) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    deleted_at TIMESTAMPTZ NULL -- Soft delete tombstone
);

-- Active uniqueness: Slugs must be unique among ACTIVE projects only!
CREATE UNIQUE INDEX idx_projects_active_slug 
ON projects (slug) 
WHERE deleted_at IS NULL;

-- Soft Delete Query:
UPDATE projects 
SET deleted_at = CURRENT_TIMESTAMP 
WHERE project_id = 42 AND deleted_at IS NULL;
```

### 2. Universal Trigger-Based Audit Shadow Table (PostgreSQL)
```sql
CREATE TABLE audit_logs (
    audit_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    target_table VARCHAR(63) NOT NULL,
    record_id BIGINT NOT NULL,
    action_type VARCHAR(10) NOT NULL, -- 'INSERT', 'UPDATE', 'DELETE'
    old_data JSONB NULL,
    new_data JSONB NULL,
    actor_id VARCHAR(50) NULL,
    occurred_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_audit_logs_lookup ON audit_logs (target_table, record_id);
```

### 3. Multi-Tenancy via PostgreSQL Row-Level Security (RLS)
```sql
CREATE TABLE tenant_invoices (
    invoice_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    tenant_id BIGINT NOT NULL,
    amount NUMERIC(10,2) NOT NULL,
    status VARCHAR(20) NOT NULL
);

CREATE INDEX idx_tenant_invoices_tenant ON tenant_invoices (tenant_id);

-- 1. Enable RLS on the table
ALTER TABLE tenant_invoices ENABLE ROW LEVEL SECURITY;

-- 2. Define Tenant Isolation Policy
CREATE POLICY tenant_isolation_policy ON tenant_invoices
    AS RESTRICTIVE
    USING (tenant_id = current_setting('app.current_tenant_id')::bigint)
    WITH CHECK (tenant_id = current_setting('app.current_tenant_id')::bigint);
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **Row-Level Security (RLS)** | Native `CREATE POLICY` | Emulate via Views / App logic | **Not Supported** | Native `CREATE SECURITY POLICY` |
| **Temporal Tables** | Extension `temporal_tables` or manual SCD2 | Manual SCD2 | Manual SCD2 | **Native System-Versioned Temporal Tables** (`SYSTEM_VERSIONING = ON`)|
| **Audit JSON Logging** | Native `to_jsonb(OLD)` in triggers | Triggers + `JSON_OBJECT()` | Triggers + `json_object()` | Change Data Capture (CDC) or Temporal |
| **Partial Unique Index** | Supported (`WHERE deleted_at IS NULL`) | **Not Supported** (Use generated cols) | Supported | Supported |

---

## Practical Examples

### Example 1: Basic — Universal JSONB Audit Trigger
A production-ready trigger that automatically records `INSERT`, `UPDATE`, and `DELETE` deltas as JSONB.

```sql
CREATE OR REPLACE FUNCTION log_entity_audit()
RETURNS TRIGGER AS $$
BEGIN
    IF (TG_OP = 'DELETE') THEN
        INSERT INTO audit_logs (target_table, record_id, action_type, old_data, new_data)
        VALUES (TG_TABLE_NAME, OLD.id, 'DELETE', to_jsonb(OLD), NULL);
        RETURN OLD;
    ELSIF (TG_OP = 'UPDATE') THEN
        INSERT INTO audit_logs (target_table, record_id, action_type, old_data, new_data)
        VALUES (TG_TABLE_NAME, NEW.id, 'UPDATE', to_jsonb(OLD), to_jsonb(NEW));
        RETURN NEW;
    ELSIF (TG_OP = 'INSERT') THEN
        INSERT INTO audit_logs (target_table, record_id, action_type, old_data, new_data)
        VALUES (TG_TABLE_NAME, NEW.id, 'INSERT', NULL, to_jsonb(NEW));
        RETURN NEW;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

-- Attach trigger to any table with an 'id' primary key
CREATE TABLE sensitive_documents (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    title VARCHAR(100) NOT NULL,
    content TEXT NOT NULL
);

CREATE TRIGGER trg_audit_sensitive_docs
AFTER INSERT OR UPDATE OR DELETE ON sensitive_documents
FOR EACH ROW EXECUTE FUNCTION log_entity_audit();
```

---

### Example 2: Real-World — SCD Type 2 Employee Compensation History
Track historical salary changes with validity date ranges and enforce that an employee can have at most one active salary record at any time.

```sql
CREATE TABLE employee_compensation_history (
    comp_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    employee_id BIGINT NOT NULL,
    annual_salary NUMERIC(10,2) NOT NULL,
    valid_from DATE NOT NULL,
    valid_to DATE NULL, -- NULL indicates current active salary!
    is_current BOOLEAN NOT NULL DEFAULT TRUE,
    
    CONSTRAINT chk_valid_date_range CHECK (valid_to IS NULL OR valid_to >= valid_from)
);

-- Enforce exactly ONE current salary per employee!
CREATE UNIQUE INDEX idx_single_current_salary_per_emp 
ON employee_compensation_history (employee_id) 
WHERE is_current = TRUE;

-- Insert initial salary
INSERT INTO employee_compensation_history (employee_id, annual_salary, valid_from, is_current)
VALUES (101, 80000.00, '2024-01-01', TRUE);

-- Promotion on 2026-01-01: Archive previous row and insert new row in a transaction:
BEGIN;
UPDATE employee_compensation_history
SET valid_to = '2026-01-01', is_current = FALSE
WHERE employee_id = 101 AND is_current = TRUE;

INSERT INTO employee_compensation_history (employee_id, annual_salary, valid_from, is_current)
VALUES (101, 105000.00, '2026-01-01', TRUE);
COMMIT;
```

---

### Example 3: Edge Case — Querying Point-in-Time State in SCD2
Find what employee 101's salary was on `'2025-06-15'` (retroactive tax audit).

```sql
SELECT employee_id, annual_salary
FROM employee_compensation_history
WHERE employee_id = 101
  AND valid_from <= '2025-06-15'
  AND (valid_to IS NULL OR valid_to > '2025-06-15');
```

```
Result:
 employee_id | annual_salary 
-------------+---------------
         101 |      80000.00
(1 row)
```

---

### Example 4: Failure Mode — The Soft-Delete Table Bloat Collapse
Demonstrating why leaving soft-deleted rows in high-churn tables ruins query performance.

```sql
-- Scenario: Queue jobs table processes 1,000,000 tasks per day.
-- Developer implements soft-deleting: UPDATE queue_tasks SET deleted_at = NOW();
--
-- After 90 days:
-- Table size: 90,000,000 rows (89,950,000 soft-deleted, 50,000 active).
-- Query: SELECT * FROM queue_tasks WHERE deleted_at IS NULL;
--
-- Problem: Without a partial index, the database scans tens of gigabytes of dead tuples!
-- Autovacuum struggles to reclaim space because rows are physically present.
--
-- REMEDY: Archival Partitioning or Hard Deletes with Shadow Tables!
-- Move soft-deleted rows out of the hot OLTP table to a cold archive table after 7 days!
```

---

### Example 5: Optimized — Native System-Versioned Temporal Tables in SQL Server
Microsoft SQL Server maintains the entire historical ledger automatically at the storage engine level.

```sql
-- SQL Server 2022 Syntax:
CREATE TABLE dbo.EmployeeContracts (
    ContractId INT PRIMARY KEY,
    EmployeeName NVARCHAR(100) NOT NULL,
    HourlyRate DECIMAL(10,2) NOT NULL,
    -- System versioning period columns
    SysStartTime DATETIME2 GENERATED ALWAYS AS ROW START NOT NULL,
    SysEndTime DATETIME2 GENERATED ALWAYS AS ROW END NOT NULL,
    PERIOD FOR SYSTEM_TIME (SysStartTime, SysEndTime)
)
WITH (SYSTEM_VERSIONING = ON (HISTORY_TABLE = dbo.EmployeeContractsHistory));

-- Time Travel Query: Point-in-time state as of 2025-01-01!
SELECT * FROM dbo.EmployeeContracts 
FOR SYSTEM_TIME AS OF '2025-01-01 12:00:00';
```

---

## Best Practices

1. **Always Use Partial Indexes for Soft-Delete Tables**: Index only active records (`WHERE deleted_at IS NULL`) to keep B-Tree sizes small and fast.
2. **Implement Automated Archival for High-Churn Soft Deletes**: In tables where soft-deleted rows outnumber active rows by 10:1, run a nightly job to move soft-deleted rows to cold archival storage.
3. **Use Row-Level Security (RLS) for Multi-Tenancy**: Never rely on developer discipline to append `WHERE tenant_id = ?`. Enforce tenant isolation via declarative database RLS policies.
4. **Use JSONB for Audit Logs**: Store pre- and post-update row snapshots as `JSONB` in the audit table. This decouples the audit log schema from changes to the base table schema.
5. **Always Set `app.current_tenant_id` at Connection Acquisition**: In web application connection pools, set the tenant context immediately when borrowing a connection from the pool to prevent cross-tenant data leakage.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Boolean `is_deleted` flags | Disables partial indexes and lacks forensic timestamp of when deletion occurred. | Use `deleted_at TIMESTAMPTZ NULL`. |
| 2 | Normal `UNIQUE` constraint with soft deletes | Soft-deleted rows block re-registration of the same email or username. | Use partial unique index: `WHERE deleted_at IS NULL`. |
| 3 | Forgetting `tenant_id` in composite index | Queries filter by `tenant_id` on every query; missing index causes full table scans. | Put `tenant_id` as the leading column in composite indexes. |
| 4 | Unbounded SCD2 updates without transactions | Updating `valid_to` and inserting a new version separately risks race conditions. | Execute both updates inside a single ACID transaction block. |
| 5 | Trigger audit log infinite loop | Trigger on audit log table auditing itself causes stack exhaustion. | Attach audit triggers strictly to business tables, never the audit table. |
| 6 | Superuser RLS bypass | PostgreSQL table owners and superusers bypass RLS policies by default! | Use `FORCE ROW LEVEL SECURITY` or connect as an unprivileged application role. |

---

## Performance & Security Notes

### The PostgreSQL `FORCE ROW LEVEL SECURITY` Invariant
By default in PostgreSQL, table owners, administrators, and superusers **bypass Row-Level Security**:
```sql
ALTER TABLE tenant_invoices ENABLE ROW LEVEL SECURITY;
```
If an application connects to PostgreSQL using the table owner role (e.g., role `postgres`), **RLS is silently bypassed**, exposing all tenant data!
- **Mandatory Production Security Fix**:
  ```sql
  ALTER TABLE tenant_invoices FORCE ROW LEVEL SECURITY;
  ```
  `FORCE` mandates that even the table owner is subject to the security policy. Application connection pools should always connect as a non-superuser role.

---

## Comparison Matrix

| Multi-Tenancy Strategy | Isolation Level | Storage Overhead | Operational Complexity | Cost per Tenant |
| :--- | :--- | :--- | :--- | :--- |
| **Shared DB / Shared Schema (RLS)**| High (Cryptographic/DB)| Lowest (Single pool)| Low (Single migration) | **Lowest** |
| **Shared DB / Separate Schema** | High (Namespace separation)| Moderate (Catalog bloat)| High (N migrations) | Moderate |
| **Separate DB per Tenant** | Maximum (Physical DB file)| Highest (Connection pool exhaustion)| Highest (N database instances)| **Highest** |

---

## Interview Questions & Deep Dives

### Q1: What is the architectural difference between `deleted_at TIMESTAMPTZ` and `is_deleted BOOLEAN`?
<details>
<summary>Click to view answer</summary>

1. **Information Content**: `is_deleted` only conveys a binary state. `deleted_at` conveys both state (NULL vs populated) and the exact temporal audit timestamp of when the deletion occurred.
2. **Index Optimization**: In PostgreSQL and SQL Server, `deleted_at IS NULL` can be used to construct a **Partial Index**. Active records (where `deleted_at IS NULL`) are indexed, while tombstoned records are completely excluded from the index, keeping index trees compact and fast.
3. **Data Retention & Compliance**: Automated data pruning jobs can easily query `WHERE deleted_at < NOW() - INTERVAL '90 days'` to execute permanent GDPR/retention purges.
</details>

### Q2: How does PostgreSQL Row-Level Security (RLS) prevent cross-tenant data leakage?
<details>
<summary>Click to view answer</summary>

PostgreSQL RLS operates at the Query Rewriter stage of the execution engine. When an RLS policy is attached to a table:
1. The application sets the active tenant context using `SET app.current_tenant_id = '123'`.
2. Whenever any query (`SELECT`, `UPDATE`, `DELETE`, `INSERT`) targets the table, the query rewriter transparently injects the policy condition into the query's Abstract Syntax Tree.
3. The query is physically executed as if the developer wrote `WHERE tenant_id = 123`.
Because the filter is injected by the core database engine itself, application bugs, omitted `WHERE` clauses, and SQL injection payloads cannot bypass tenant boundaries.
</details>

### Q3: What is the difference between SCD Type 1 and SCD Type 2?
<details>
<summary>Click to view answer</summary>

- **SCD Type 1 (Overwrite)**: Updates the attribute in place, overwriting previous data. No history is kept. Ideal for correcting typographical errors or non-historical attributes (e.g., correcting a misspelled customer name).
- **SCD Type 2 (Add New Row)**: Preserves complete historical auditability. When an attribute changes, the current active row is closed out (by setting `valid_to = NOW()` and `is_current = FALSE`), and a new row is inserted with `valid_from = NOW()` and `is_current = TRUE`. Ideal for historical tracking of compensation, insurance coverage, and billing addresses.
</details>

### Q4: Why must `ALTER TABLE ... FORCE ROW LEVEL SECURITY` be used in production?
<details>
<summary>Click to view answer</summary>

In PostgreSQL, executing `ALTER TABLE ... ENABLE ROW LEVEL SECURITY` enforces policies on regular users, but **table owners and superusers bypass RLS by default**. If the web application connects to the database using the database owner credentials, RLS policies will be silently ignored, creating a severe multi-tenant data exposure vulnerability. Adding `FORCE ROW LEVEL SECURITY` explicitly strips the owner bypass and forces the engine to apply security policies to all connections.
</details>

### Q5: How do you prevent unique constraint collisions when soft-deleting rows?
<details>
<summary>Click to view answer</summary>

By using a **Partial Unique Index** instead of a table-level unique constraint:
```sql
CREATE UNIQUE INDEX idx_users_active_email ON users (email) WHERE deleted_at IS NULL;
```
The unique index only indexes rows where `deleted_at IS NULL`. If a user with `email = 'alice@example.com'` is soft-deleted, their row is dropped from the index, allowing a new user to register with that exact email address without violating uniqueness.
</details>

### Q6: What is a System-Versioned Temporal Table in ANSI SQL:2011?
<details>
<summary>Click to view answer</summary>

A System-Versioned Temporal Table (standardized in ANSI SQL:2011 and natively implemented in Microsoft SQL Server 2016+ and MariaDB) automatically maintains a complete versioned history of every row change. The engine manages two timestamp columns (`SysStartTime` and `SysEndTime`) and automatically moves replaced row versions to a linked history table on every update or delete. Queries can use the `FOR SYSTEM_TIME AS OF <timestamp>` clause to travel back in time and query historical database snapshots natively.
</details>

### Q7: What are the performance hazards of soft deletes in high-write transactional systems?
<details>
<summary>Click to view answer</summary>

1. **Table & Index Bloat**: Dead rows are never physically reclaimed by VACUUM, causing tables and B-Tree indexes to grow indefinitely.
2. **Buffer Pool Eviction**: Scans of active data must read through millions of tombstoned dead rows on disk pages, evicting active data from the database cache.
3. **Foreign Key Integrity**: Deleting a parent row requires setting soft-delete flags on children, which cannot be enforced via standard declarative `ON DELETE CASCADE`.
</details>

### Q8: How can you capture the application User ID inside a PostgreSQL database trigger?
<details>
<summary>Click to view answer</summary>

Because PostgreSQL runs as a database user (e.g., `app_user`), it does not naturally know which end-user triggered the web request. Application code sets a transaction-local session variable before executing queries:
`SET LOCAL app.current_user_id = 'usr_abc123';`
The trigger function then inspects this setting:
`current_setting('app.current_user_id', true)`
and records it in the `audit_logs.actor_id` column.
</details>

---

## Practice Exercises

### Exercise 1: Implement Soft-Delete Partial Index
Create an `organizations` table with `org_id`, `subdomain VARCHAR(50)`, and `deleted_at TIMESTAMPTZ`. Create a partial unique index ensuring that active subdomains are strictly unique, but permitting multiple soft-deleted organizations to have identical subdomains.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE organizations (
    org_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    org_name VARCHAR(100) NOT NULL,
    subdomain VARCHAR(50) NOT NULL,
    deleted_at TIMESTAMPTZ NULL
);

CREATE UNIQUE INDEX idx_uq_active_org_subdomain 
ON organizations (subdomain) 
WHERE deleted_at IS NULL;
```
</details>

---

### Exercise 2: Point-in-Time Address Query
Given `customer_address_history(address_id, customer_id, street, city, valid_from DATE, valid_to DATE)`. Write a query returning the valid street address for `customer_id = 42` on `'2023-04-15'`.

<details>
<summary>Click to view solution</summary>

```sql
SELECT street, city
FROM customer_address_history
WHERE customer_id = 42
  AND valid_from <= '2023-04-15'
  AND (valid_to IS NULL OR valid_to > '2023-04-15');
```
</details>

---

### Exercise 3: Complete Row-Level Security Configuration
Given table `client_documents(doc_id, tenant_id, file_path)`. Enable RLS, enforce `FORCE ROW LEVEL SECURITY`, and write a policy matching `tenant_id` against the session variable `app.active_tenant`.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE client_documents (
    doc_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    tenant_id BIGINT NOT NULL,
    file_path TEXT NOT NULL
);

CREATE INDEX idx_client_docs_tenant ON client_documents (tenant_id);

-- Enable & Force RLS
ALTER TABLE client_documents ENABLE ROW LEVEL SECURITY;
ALTER TABLE client_documents FORCE ROW LEVEL SECURITY;

-- Define Policy
CREATE POLICY client_docs_tenant_isolation ON client_documents
    FOR ALL
    USING (tenant_id = current_setting('app.active_tenant')::bigint)
    WITH CHECK (tenant_id = current_setting('app.active_tenant')::bigint);
```
</details>

---

### Exercise 4: Trigger Audit Logging Delta Capture
Write a trigger function that writes to `price_audit_log(product_id, old_price, new_price, modified_at)` whenever `products(product_id, price)` is updated, but **only** if the price actually changed.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE price_audit_log (
    audit_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    product_id BIGINT NOT NULL,
    old_price NUMERIC(10,2) NOT NULL,
    new_price NUMERIC(10,2) NOT NULL,
    modified_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE OR REPLACE FUNCTION trg_log_price_change()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO price_audit_log (product_id, old_price, new_price)
    VALUES (NEW.product_id, OLD.price, NEW.price);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_price_updated
AFTER UPDATE OF price ON products
FOR EACH ROW
WHEN (OLD.price IS DISTINCT FROM NEW.price)
EXECUTE FUNCTION trg_log_price_change();
```
</details>

---

### Exercise 5: Single Active Version Constraint
Write the DDL for `software_licenses(license_id, company_id, license_key, is_active)` ensuring that each company can have at most **one active license**, but unlimited expired licenses.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE software_licenses (
    license_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    company_id BIGINT NOT NULL,
    license_key VARCHAR(100) NOT NULL,
    is_active BOOLEAN NOT NULL DEFAULT TRUE
);

CREATE UNIQUE INDEX idx_single_active_license_per_company 
ON software_licenses (company_id) 
WHERE is_active = TRUE;
```
</details>

---

### Exercise 6: Fixing the Application-Level Multi-Tenancy Vulnerability
A junior developer writes:
`SELECT * FROM invoices WHERE invoice_id = $1`
Explain why this code introduces a multi-tenant security vulnerability, and how database-level RLS completely eliminates the risk.

<details>
<summary>Click to view solution</summary>

**Vulnerability**: The query lacks a tenant filter (`AND tenant_id = current_tenant`). If a malicious user on Tenant A guesses or iterates the `invoice_id` belonging to Tenant B, the application returns Tenant B's confidential invoice data (IDOR vulnerability).

**RLS Solution**: When Row-Level Security is enabled on `invoices`, the database engine automatically rewrites the query internally to:
`SELECT * FROM invoices WHERE invoice_id = $1 AND tenant_id = current_setting('app.current_tenant_id')`.
If the requested `invoice_id` belongs to another tenant, the engine returns **0 rows**, rendering IDOR attacks mathematically impossible.
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                               SCHEMA PATTERNS CHEAT SHEET                               |
+-------------------+----------------------------+----------------------------------------+
| Pattern           | Implementation             | Core Benefit                           |
+-------------------+----------------------------+----------------------------------------+
| Soft Delete       | deleted_at TIMESTAMPTZ NULL| Recoverable data; partial unique index |
| Audit Trail       | Trigger to JSONB log table | Full historical forensic traceability  |
| SCD Type 2        | valid_from, valid_to cols  | Time-travel point-in-time state queries|
| Multi-Tenancy RLS | CREATE POLICY ... USING    | Engine-enforced tenant isolation       |
| Active Uniqueness | WHERE deleted_at IS NULL   | Solves unique soft-delete collision    |
+-------------------+----------------------------+----------------------------------------+
```

- **Soft Delete Rule**: Always use `deleted_at TIMESTAMPTZ`, never `is_deleted BOOLEAN`.
- **RLS Safety**: Always add `FORCE ROW LEVEL SECURITY` to prevent owner bypass.
- **Audit Rule**: Capture `OLD` and `NEW` row snapshots as `JSONB` in shadow tables.

---

## Further Reading
- PostgreSQL 17 Documentation: *Row Security Policies (Row-Level Security)*
- ANSI/ISO SQL:2011 Standard: *Temporal Tables and System-Versioning*
- Ralph Kimball: *The Data Warehouse Toolkit (Slowly Changing Dimensions Types 1-4)*
- Microsoft Learn: *Temporal Tables in SQL Server and Azure SQL Database*
