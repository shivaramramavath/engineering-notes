# Zero-Downtime Deployments and Database Migrations in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 14-production-and-deployment |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. The Expand and Contract Pattern](#3-the-expand-and-contract-pattern)
- [4. How It Works Internally](#4-how-it-works-internally)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / Migration Strategy Matrix](#9-comparison--migration-strategy-matrix)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Achieving zero-downtime deployments for stateless compute in Kubernetes is straightforward with rolling updates. However, **stateful relational databases (PostgreSQL, MySQL, Oracle) present the primary obstacle to continuous delivery**:
- During a rolling update, **Version 1.0 (old code) and Version 2.0 (new code) run concurrently side-by-side** for several minutes, reading and writing to the exact same shared database.
- If Version 2.0 executes a destructive migration on startup (e.g., dropping a column, renaming a column, or adding a `NOT NULL` constraint without a default), Version 1.0 pods immediately crash with `PSQLException: column "x" does not exist`.
- Conversely, if a deployment fails and rolls back, the database must remain fully functional for the older version without data loss.

### Historical Evolution
In legacy software architectures, database upgrades required scheduled "maintenance windows" (e.g. Sunday at 2:00 AM) where the entire system was shut down, users were shown a "Down for Maintenance" page, migration scripts were run, and the new application version was deployed.

Modern continuous deployment eliminates maintenance windows using the **Expand and Contract Pattern** (also known as the **Parallel Run** or **Phased Migration** pattern) managed via **Flyway** or **Liquibase**.

### Production Relevance
Mastering zero-downtime migrations enables teams to:
- Deploy software to production multiple times a day without user disruption.
- Safely roll back application releases without breaking database integrity.
- Execute large-scale schema refactorings across millions of rows without locking tables.

---

## 2. Core Concepts

### Architectural Foundations
1. **Backward and Forward Compatibility**: At any moment during deployment, the database schema must simultaneously support both the old version ($V_{1.0}$) and the new version ($V_{2.0}$) of the application.
2. **Flyway / Liquibase**: Version-controlled database migration frameworks that execute deterministic, incremental SQL change scripts during the delivery pipeline.
3. **Database Locks**: Understanding DDL lock levels (e.g. `ACCESS EXCLUSIVE` in PostgreSQL) that can lock entire tables and cause connection pool timeouts.
4. **Decoupled Migration Runner**: Running Flyway migrations via an isolated Kubernetes `Job` or Helm `pre-upgrade` hook rather than having 50 Spring Boot pod replicas simultaneously run migrations on boot.

---

## 3. The Expand and Contract Pattern

The Expand and Contract pattern breaks every breaking schema change into **three distinct, non-breaking deployment phases**:

```
+------------------------------------------------------------------------------------+
|                      THE EXPAND AND CONTRACT PATTERN LIFECYCLE                     |
+------------------------------------------------------------------------------------+

PHASE 1: EXPAND (Deploy V1.1)
  - Database: Add the new column/table alongside the old column (make it NULLABLE).
  - Application: Old code (V1.0) continues using the old column.
  - New Code (V1.1): Writes to BOTH old and new columns (Dual-Writing); reads from old.
  --> Database supports both V1.0 and V1.1 simultaneously!

PHASE 2: MIGRATE & BACKFILL (Background Data Sync)
  - Execute an asynchronous background migration to copy historical data:
    UPDATE users SET full_name = first_name || ' ' || last_name WHERE full_name IS NULL;
  - Both V1.0 and V1.1 remain completely operational.

PHASE 3: SWITCH (Deploy V1.2)
  - Application (V1.2): Reads and writes exclusively from the NEW column.
  - Old column is no longer read or referenced by application code.

PHASE 4: CONTRACT (Deploy V1.3 - Cleanup)
  - Database: Safely drop the old column or trigger:
    ALTER TABLE users DROP COLUMN first_name, DROP COLUMN last_name;
  --> Migration is complete with ZERO downtime!
```

---

## 4. How It Works Internally

### Concurrent Dual-State Execution During Kubernetes Rolling Update

```
KUBERNETES CLUSTER (Rolling Update In Progress)
+------------------------------------------------------------------------------------+
|  SHARED POSTGRESQL DATABASE (Phase 1: EXPANDED SCHEMA)                             |
|                                                                                    |
|  TABLE: customers                                                                  |
|  - id: bigint                                                                      |
|  - phone_number: varchar(20)          [OLD COLUMN - Kept active]                   |
|  - contact_phone: varchar(20) NULL    [NEW COLUMN - Added by Flyway migration]    |
+------------------------------------------------------------------------------------+
          ^                                           ^
          | (Reads/Writes phone_number)               | (Reads phone_number, writes BOTH!)
          |                                           |
+--------------------------+               +--------------------------+
|  OLD REPLICA (V1.0 Pod)  |               |  NEW REPLICA (V2.0 Pod)  |
|  - Query: SELECT phone...|               |  - Dual-writes to both   |
|  - Succeeded!            |               |  - Succeeded!            |
+--------------------------+               +--------------------------+
```

---

## 5. Practical Examples

### Example 1: Non-Destructive Column Renaming (Step-by-Step)
Suppose we want to rename `customer.phone_number` to `customer.contact_phone`.

#### Step 1: Flyway Migration `V2__expand_contact_phone.sql`
```sql
-- Step 1: Add new column as NULLABLE (Never add NOT NULL directly in Phase 1!)
ALTER TABLE customers ADD COLUMN contact_phone VARCHAR(30);

-- Create a temporary database trigger to dual-write from old to new (Optional if handled in Java)
CREATE OR REPLACE FUNCTION sync_customer_phone()
RETURNS TRIGGER AS $$
BEGIN
    NEW.contact_phone = NEW.phone_number;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_sync_phone
BEFORE INSERT OR UPDATE ON customers
FOR EACH ROW EXECUTE FUNCTION sync_customer_phone();
```

#### Step 2: Spring Boot JPA Dual-Writing Entity (Version 2.0)
```java
package com.example.production.migration;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.Id;
import jakarta.persistence.Table;

@Entity
@Table(name = "customers")
public class Customer {

    @Id
    private Long id;

    // OLD column: Kept active for backward compatibility with V1.0 pods
    @Column(name = "phone_number")
    private String phoneNumber;

    // NEW column: Populated during dual-writing
    @Column(name = "contact_phone")
    private String contactPhone;

    public void updatePhone(String newPhone) {
        // Dual-write to BOTH fields!
        this.phoneNumber = newPhone;
        this.contactPhone = newPhone;
    }

    public String getPhone() {
        // Read from new column if present, fallback to old column
        return this.contactPhone != null ? this.contactPhone : this.phoneNumber;
    }
}
```

#### Step 3: Backfill Background Script
```sql
-- Backfill existing historical records in batches of 5000 to avoid locking the table
UPDATE customers 
SET contact_phone = phone_number 
WHERE contact_phone IS NULL;
```

#### Step 4: Contract Migration `V3__contract_drop_old_phone.sql` (Deployed with Version 3.0)
```sql
-- Step 4: Drop trigger and drop obsolete column
DROP TRIGGER IF EXISTS trg_sync_phone ON customers;
DROP FUNCTION IF EXISTS sync_customer_phone();
ALTER TABLE customers DROP COLUMN phone_number;
```

### Example 2: Adding a `NOT NULL` Column to a 10-Million Row Table
In PostgreSQL, executing `ALTER TABLE orders ADD COLUMN status VARCHAR(20) NOT NULL DEFAULT 'PENDING'` on an active table can acquire an exclusive table lock and rewrite the table, stalling all production queries.

**Safe Zero-Downtime Approach**:
```sql
-- Step 1: Add column as NULLABLE with default (Instant metadata-only change in Postgres 11+)
ALTER TABLE orders ADD COLUMN status VARCHAR(20) DEFAULT 'PENDING';

-- Step 2: Add NOT NULL constraint without validating historical rows (Instant!)
ALTER TABLE orders ADD CONSTRAINT check_status_not_null CHECK (status IS NOT NULL) NOT VALID;

-- Step 3: Validate constraint concurrently in background (Does NOT hold exclusive table lock!)
ALTER TABLE orders VALIDATE CONSTRAINT check_status_not_null;
```

### Example 3: Decoupled Flyway Runner via Kubernetes Job
Never allow 50 autoscaled Spring Boot pods to run Flyway concurrently on startup. In `application-prod.yml`, disable Flyway on the web pods:

```yaml
# application-prod.yml (Web Service Pods)
spring:
  flyway:
    enabled: false # Disabled on live application replicas!
```

Run Flyway as an isolated, single-instance **Kubernetes Pre-Upgrade Job**:
```yaml
apiVersion: batch/v1
kind: Job
metadata:
  name: order-service-migration-v2
spec:
  backoffLimit: 0 # Fail fast if migration fails
  template:
    spec:
      restartPolicy: Never
      containers:
        - name: flyway-runner
          image: flyway/flyway:10-alpine
          command: ["flyway", "migrate"]
          env:
            - name: FLYWAY_URL
              value: "jdbc:postgresql://db-primary.internal:5432/orders"
            - name: FLYWAY_USER
              value: "migration_admin"
            - name: FLYWAY_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: db-credentials
                  key: admin-password
            - name: FLYWAY_LOCATIONS
              value: "filesystem:/flyway/sql"
          volumeMounts:
            - name: sql-migrations
              mountPath: /flyway/sql
      volumes:
        - name: sql-migrations
          configMap:
            name: order-service-sql-scripts
```

### Example 4: Anti-Pattern vs. Production Fix (Immediate Column Deletion)
```sql
-- ANTI-PATTERN: Dropping or renaming a column in a single migration alongside code update
ALTER TABLE users RENAME COLUMN email TO contact_email;
-- CATASTROPHIC DISASTER:
-- During rolling update, existing V1.0 pods execute: SELECT email FROM users;
-- Instant 500 error on 100% of user requests handled by V1.0 pods!
-- Rollback is impossible because V1.0 code cannot run on the renamed table!

-- PRODUCTION FIX: The Expand/Contract Pattern (4-step phased migration)
-- 1. Add contact_email as NULLABLE
-- 2. Dual-write to email and contact_email
-- 3. Backfill data
-- 4. Switch reads to contact_email
-- 5. Drop email column in next release
```

---

## 6. Best Practices & Production Guidelines

1. **Every Schema Change Must Be Backward Compatible**: Assume that old application pods will run against the new schema for at least 15 to 30 minutes during rolling updates.
2. **Never Add `NOT NULL` Columns Without Defaults**: Adding a `NOT NULL` column without a default will immediately break inserts executed by older application replicas that do not know about the new column.
3. **Execute DDL Migrations in Dedicated Kubernetes Jobs**: Do not let application pods execute DDL on startup. A slow migration on pod 1 will block all other 49 pods, tripping readiness probes.
4. **Use Lock Timeouts in PostgreSQL**: Set `SET lock_timeout = '2s';` before executing DDL migrations. If an exclusive lock cannot be acquired within 2 seconds, fail the migration rather than waiting in a queue that blocks all subsequent application queries.
5. **Create Indexes Concurrently**: In PostgreSQL, never run `CREATE INDEX`. Always use `CREATE INDEX CONCURRENTLY` to avoid taking an exclusive write lock on the table.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Renaming a column directly | Older running pods fail instantly with `column does not exist`. | Use the Expand/Contract pattern with dual-writing. |
| Running Flyway on 50 pod replicas | Pods contend for the `flyway_schema_history` table lock, causing startup timeouts. | Run Flyway in a single-instance Kubernetes Job before rollout. |
| Adding indexes without `CONCURRENTLY` | PostgreSQL locks the entire table for writes while scanning rows, taking APIs offline. | Always specify `CREATE INDEX CONCURRENTLY`. |
| Adding `NOT NULL` without default | Inserts from older application replicas fail immediately with constraint violations. | Add column as NULLABLE; enforce constraint in a subsequent release. |
| Modifying already-executed migration scripts | Flyway detects checksum mismatch on boot and throws `FlywayValidateException`. | Never modify existing migrations; always append a new `V{n}__...sql` script. |
| Inadequate lock timeouts | DDL statement queues behind a slow query, blocking all subsequent incoming application queries. | Set `SET lock_timeout = '3s'` in migration scripts. |

---

## 8. Performance & Security Notes

### Table Rewrites and Exclusive Locks
- In PostgreSQL and MySQL, altering column data types (e.g. `VARCHAR(50)` $\rightarrow$ `INTEGER`) requires rewriting every page of the table on disk, holding an `ACCESS EXCLUSIVE` lock.
- For tables exceeding 10 million rows, use zero-downtime schema tools like **pg-osc** or GitHub's **gh-ost** (for MySQL).

### Migration Admin Credentials
- The database user used for migrations (`migration_admin`) requires DDL privileges (`CREATE`, `ALTER`, `DROP`).
- The runtime application user (`app_user`) should strictly have DML privileges (`SELECT`, `INSERT`, `UPDATE`, `DELETE`) with zero DDL permissions to prevent accidental schema modifications at runtime.

---

## 9. Comparison / Migration Strategy Matrix

| Migration Pattern | Downtime Required? | Rollback Capability | Complexity |
|---|---|---|---|
| **Expand / Contract (Parallel Run)**| **Zero Downtime** | **Seamless (Old code remains compatible)**| Medium (Phased releases) |
| **Maintenance Window (Cold)** | High (30m – 4h) | Risky (Requires full DB restore) | Low (Single script) |
| **Shadow Database / CDC Replication** | Zero Downtime | Instant (Reverse replication) | Very High (Requires Kafka/Debezium)|

---

## 10. Interview Questions & Answers

### Q1: What is the Expand and Contract pattern and why is it necessary for zero-downtime deployments?
<details>
<summary>Click to view answer</summary>

During a zero-downtime rolling update in Kubernetes, old and new application versions run simultaneously for a period of time, accessing the same database. If a database change immediately removes or renames a column, the old pods will crash.  
The Expand and Contract pattern splits breaking schema changes into non-breaking phases:
1. **Expand**: Add the new schema structure alongside the old structure (as nullable). Both old and new code can run concurrently.
2. **Migrate/Backfill**: Sync historical data between structures.
3. **Contract**: In a subsequent release, decommission and drop the obsolete old schema structure.
</details>

### Q2: Why should `spring.flyway.enabled` be set to `false` in production Kubernetes pods?
<details>
<summary>Click to view answer</summary>

If 50 pod replicas boot simultaneously with `spring.flyway.enabled=true`, all 50 pods will attempt to acquire the lock on the `flyway_schema_history` table at the exact same moment. If a migration takes 30 seconds to execute, 49 pods will block waiting for the lock, exceeding their Kubernetes startup probe timeouts and entering `CrashLoopBackOff`. Migrations should be executed by a single-instance Kubernetes Job or Helm pre-upgrade hook before application pods are rolled out.
</details>

### Q3: How do you add an index to a large production table in PostgreSQL without causing downtime?
<details>
<summary>Click to view answer</summary>

Use `CREATE INDEX CONCURRENTLY`. Standard `CREATE INDEX` acquires a `SHARE` lock on the table, which blocks all concurrent `INSERT`, `UPDATE`, and `DELETE` operations until the index build completes (which can take hours on large tables). `CREATE INDEX CONCURRENTLY` builds the index without acquiring an exclusive write lock, allowing live transactional traffic to continue uninterrupted.
</details>

### Q4: What happens if a Flyway migration script fails halfway through execution?
<details>
<summary>Click to view answer</summary>

In databases that support transactional DDL (such as PostgreSQL), the entire migration script rolls back automatically. In databases that do not support transactional DDL (such as MySQL or Oracle), DDL statements cannot be rolled back; the schema is left in a partially migrated state and Flyway records a failed entry in `flyway_schema_history`. An engineer must manually resolve the schema discrepancy and run `flyway repair` before deployment can proceed.
</details>

### Q5: How do you safely remove a column from a high-traffic table without downtime?
<details>
<summary>Click to view answer</summary>

Never drop the column immediately. Follow these steps:
1. Release V1: Remove all references to the column from Java code (remove JPA `@Column` mapping). Old and new pods both ignore the column.
2. Verify: Ensure all running pods are on V1 and no queries reference the column.
3. Release V2: Execute `ALTER TABLE tablename DROP COLUMN column_name;` via Flyway.
</details>

### Q6: What is a `lock_timeout` in PostgreSQL and why is it critical in migrations?
<details>
<summary>Click to view answer</summary>

When a DDL migration runs (e.g. `ALTER TABLE`), it requests an exclusive lock. If a long-running read query is active, the DDL waits in the lock queue. In PostgreSQL, any *new* incoming queries queue behind the DDL request, effectively locking the entire table and crashing the application. Setting `SET lock_timeout = '2s'` ensures that if the DDL cannot acquire the lock within 2 seconds, it aborts immediately, protecting active application traffic.
</details>

### Q7: What is the difference between Flyway and Liquibase?
<details>
<summary>Click to view answer</summary>

- **Flyway**: Prefers plain, native SQL migration scripts (`V1__init.sql`). It is lightweight, straightforward, and leverages database-specific native features.
- **Liquibase**: Uses database-agnostic abstractions formatted in XML, YAML, or JSON (changesets). It supports automated rollbacks and can generate SQL across multiple database vendors.
</details>

### Q8: How does dual-writing work in Java code during the Expand phase?
<details>
<summary>Click to view answer</summary>

During dual-writing, the Java entity maps both the legacy field and the new field. Whenever an `INSERT` or `UPDATE` occurs, the service layer populates both fields with the same value, guaranteeing that whichever version of the application reads the record, the data is consistent.
</details>

---

## 11. Practice Exercises

### Exercise 1: Write a Non-Blocking Concurrent Index Migration in Flyway (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a PostgreSQL Flyway migration script `V4__add_customer_email_idx.sql` that creates an index on `customers(email)` concurrently without blocking table writes.

**Solution**:
```sql
-- Flyway requires disabling transaction for CONCURRENT index creation
-- @formatter:off
-- @flyway:transactional=false

CREATE INDEX CONCURRENTLY IF NOT EXISTS idx_customers_email 
ON customers (email);
```
</details>

### Exercise 2: Safely Add a Column with Check Constraint (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a PostgreSQL migration script to add a `tier` column with a default of `'STANDARD'`, ensuring no table lock occurs and adding a `NOT NULL` check constraint without validating historical rows immediately.

**Solution**:
```sql
SET lock_timeout = '3s';

-- Step 1: Fast metadata add
ALTER TABLE accounts ADD COLUMN tier VARCHAR(20) DEFAULT 'STANDARD';

-- Step 2: Unvalidated check constraint (Takes zero lock time)
ALTER TABLE accounts ADD CONSTRAINT chk_accounts_tier_not_null 
CHECK (tier IS NOT NULL) NOT VALID;

-- Step 3: Validate constraint concurrently
ALTER TABLE accounts VALIDATE CONSTRAINT chk_accounts_tier_not_null;
```
</details>

### Exercise 3: Complete Pre-Upgrade Helm Hook Manifest for Flyway (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a complete Kubernetes `Job` YAML file that acts as a Helm `pre-upgrade` hook to run Flyway migrations, ensuring it deletes itself upon success.

**Solution**:
```yaml
apiVersion: batch/v1
kind: Job
metadata:
  name: {{ .Release.Name }}-db-migrate-{{ .Release.Revision }}
  annotations:
    "helm.sh/hook": pre-upgrade,pre-install
    "helm.sh/hook-weight": "-1"
    "helm.sh/hook-delete-policy": before-hook-creation,hook-succeeded
spec:
  backoffLimit: 0
  template:
    spec:
      restartPolicy: Never
      containers:
        - name: flyway
          image: flyway/flyway:10-alpine
          args: ["-url=$(DB_URL)", "-user=$(DB_USER)", "-password=$(DB_PASSWORD)", "migrate"]
          env:
            - name: DB_URL
              value: "jdbc:postgresql://postgres.internal:5432/production"
            - name: DB_USER
              value: "flyway_admin"
            - name: DB_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: db-admin-secrets
                  key: password
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Principle / Pattern                         | Production Implementation Rule                              |
+---------------------------------------------+-------------------------------------------------------------+
| Expand and Contract                         | 1. Add new (nullable) -> 2. Dual-write -> 3. Switch -> Drop |
| Never Drop or Rename in One Step            | Instant 500 error for older running pods during rollout     |
| CREATE INDEX CONCURRENTLY                   | Mandatory in PostgreSQL to avoid exclusive table locks      |
| spring.flyway.enabled: false                | Disable Flyway on live web pods; run in Kubernetes Job     |
| SET lock_timeout = '2s'                     | Aborts DDL if lock queue exceeds 2s to protect user traffic |
| Decoupled Migration Job                     | Use Helm pre-upgrade hook to migrate DB before rolling pods |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Martin Fowler: Evolutionary Database Design & Parallel Run
- Vlad Mihalcea: How to Rename a Database Column with Zero Downtime
- PostgreSQL Documentation: Concurrently Building Indexes
- Flyway Documentation: Working with Zero-Downtime Deployments