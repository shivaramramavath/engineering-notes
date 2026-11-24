# Database Schema Migrations: Flyway, Liquibase & Zero-Downtime Deployments

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate / Advanced |
| **Prerequisites** | `01-jdbc-template-and-jdbc-client.md`, SQL DDL (`CREATE`, `ALTER`, `INDEX`), DevOps deployment lifecycles |
| **Estimated Reading Time** | 31 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Flyway 10.x / 11.x, Liquibase 4.30+, PostgreSQL 16 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Standard Flyway Configuration in Spring Boot](#51-standard-flyway-configuration-in-spring-boot)
   - [5.2 Repeatable Migrations for Views, Stored Functions & Triggers](#52-repeatable-migrations-for-views-stored-functions--triggers)
   - [5.3 Resolving Checksum Mismatches via Flyway Repair](#53-resolving-checksum-mismatches-via-flyway-repair)
   - [5.4 Anti-Pattern: Using ddl-auto=update in Production](#54-anti-pattern-using-ddl-autoupdate-in-production)
   - [5.5 Zero-Downtime Expand-and-Contract Migration Workflow](#55-zero-downtime-expand-and-contract-migration-workflow)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
In professional software delivery, application code evolves continuously alongside database schemas. Manually applying SQL DDL scripts (`ALTER TABLE`, `CREATE INDEX`) via database administrator terminals during deployment creates operational bottlenecks, human error risks, deployment drift across development, staging, and production environments, and silent schema divergence. Furthermore, relying on Hibernate's automatic schema generator (`spring.jpa.hibernate.ddl-auto=update`) in production is notoriously dangerous—it cannot safely drop columns, alter column types, handle data migrations, or enforce database-specific index constraints.

### Historical Evolution
Database migration tools emerged to treat database schemas as version-controlled, auditable, and automated code artifacts. **Flyway** pioneered SQL-first migrations, using plain SQL scripts organized by strict version prefixes (`V1__...sql`). **Liquibase** introduced format-agnostic changesets (XML, YAML, JSON, or SQL) with preconditions and automatic rollbacks. In modern cloud-native architectures, these tools integrate seamlessly with Spring Boot to ensure databases migrate deterministically before application connection pools open.

### Real-World Relevance
Modern continuous delivery (CI/CD) pipelines require automated, idempotent schema migrations. Understanding zero-downtime migration patterns like **Expand-and-Contract (Parallel Run)** ensures that schema modifications never lock tables or break running instances during rolling deployments.

---

## 2. Core Concepts

### Migration Types (Flyway)
1. **Versioned Migrations (`V`)**: Strictly ordered by version numbers (e.g., `V1__init.sql`, `V2_1__add_users.sql`). Executed exactly once. Once applied, their file checksums are locked; modifying an applied script fails subsequent deployments.
2. **Repeatable Migrations (`R`)**: Executed whenever their file checksum changes. Ideal for re-creating database views, stored procedures, functions, and triggers. Always executed *after* all versioned migrations.
3. **Undo Migrations (`U`)**: Optional rollback scripts in Flyway Teams/Enterprise edition.

### The Metadata Table
Flyway automatically creates and maintains a dedicated metadata table (default: `flyway_schema_history`). It records:
- Migration version, description, type, and script name.
- CRC32 checksum of the SQL file content.
- Execution timestamp and execution duration.
- Success or failure status.

### Zero-Downtime Expand-and-Contract Pattern
Modifying database columns in-place (e.g. renaming `full_name` to `first_name` and `last_name`) breaks running applications during rolling updates. The Expand-and-Contract pattern decouples changes across multiple releases:
- **Phase 1 (Expand)**: Add the new column/table without touching the old one. Application writes to both columns.
- **Phase 2 (Backfill)**: Asynchronously backfill historical data from old column to new column.
- **Phase 3 (Switchover)**: Update application to read exclusively from the new column.
- **Phase 4 (Contract)**: Drop the deprecated old column in a subsequent release.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         Flyway Bootstrapping Pipeline                              |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  1. Spring Boot Application Startup                                                |
|     - Spring Boot detects 'flyway-core' on classpath                               |
|     - Post-processes DataSource before EntityManagerFactory initializes!           |
|                                                                                    |
|  2. Metadata Table Acquisition:                                                    |
|     - Locks 'flyway_schema_history' table (or creates it if missing)               |
|     - Reads list of already applied migrations and their recorded checksums        |
|                                                                                    |
|  3. Filesystem Scanning:                                                           |
|     - Scans classpath:db/migration for V*.sql and R*.sql                           |
|     - Verifies checksums of previously applied scripts                             |
|     - If checksum differs: FAILS BOOT with FlywayException!                        |
|                                                                                    |
|  4. Migration Execution:                                                           |
|     - Sorts pending migrations in numerical version order                          |
|     - For each script:                                                             |
|       a. Begins database DDL transaction                                           |
|       b. Executes SQL statements                                                   |
|       c. Inserts record into 'flyway_schema_history'                               |
|       d. Commits transaction                                                       |
|                                                                                    |
|  5. Releases lock and hands initialized DataSource to JPA/Hibernate                |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.flywaydb</groupId>
    <artifactId>flyway-core</artifactId>
</dependency>
<dependency>
    <groupId>org.flywaydb</groupId>
    <artifactId>flyway-database-postgresql</artifactId>
</dependency>
```

### Script Naming Convention
```
V{VERSION}__{DESCRIPTION}.sql
Examples:
  V1__initial_schema.sql
  V2__create_orders_table.sql
  V2_1__add_order_discount_index.sql
  R__recreate_order_summary_view.sql
```
*Note: The separator between version and description is strictly a double underscore (`__`).*

---

## 5. Practical Examples

### 5.1 Standard Flyway Configuration in Spring Boot
```yaml
# =========================================================================
# PRODUCTION FLYWAY AND JPA CONFIGURATION
# =========================================================================
spring:
  jpa:
    # STRICT PRODUCTION RULE: Never allow Hibernate to touch the schema!
    hibernate:
      ddl-auto: validate
    properties:
      hibernate:
        default_schema: public
  flyway:
    enabled: true
    # Classpath directory containing SQL scripts
    locations: classpath:db/migration
    # Baseline existing un-versioned databases on first run
    baseline-on-migrate: true
    baseline-version: "0"
    # Fails immediately if a previously applied migration script is altered
    validate-on-migrate: true
    # Allows applying V3 even if V4 was already merged in parallel branches
    out-of-order: false
```

```sql
-- src/main/resources/db/migration/V1__initial_schema.sql
CREATE TABLE customers (
    id BIGSERIAL PRIMARY KEY,
    email VARCHAR(255) NOT NULL UNIQUE,
    full_name VARCHAR(255) NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE TABLE orders (
    id BIGSERIAL PRIMARY KEY,
    customer_id BIGINT NOT NULL REFERENCES customers(id),
    order_number VARCHAR(64) NOT NULL UNIQUE,
    total_amount NUMERIC(12, 2) NOT NULL,
    status VARCHAR(32) NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_orders_customer_id ON orders(customer_id);
```

### 5.2 Repeatable Migrations for Views, Stored Functions & Triggers
```sql
-- src/main/resources/db/migration/R__customer_order_summary_view.sql
-- This script executes every time its content is edited!
CREATE OR REPLACE VIEW view_customer_order_summary AS
SELECT 
    c.id AS customer_id,
    c.email,
    c.full_name,
    COUNT(o.id) AS total_orders,
    COALESCE(SUM(o.total_amount), 0) AS lifetime_value
FROM customers c
LEFT JOIN orders o ON c.id = o.customer_id
GROUP BY c.id, c.email, c.full_name;
```

### 5.3 Resolving Checksum Mismatches via Flyway Repair
```bash
# Problem: A developer accidentally edited V1__initial_schema.sql after it was applied to Staging.
# Flyway startup error:
# org.flywaydb.core.api.exception.FlywayValidateException: Validate failed: 
# Migration checksum mismatch for migration version 1:
#   DB: -142981249
#   Classpath: 894318721

# PRODUCTION REMEDY 1: Revert the file changes in Git to match the original checksum.

# PRODUCTION REMEDY 2: If the change was an innocuous comment fix, execute Flyway repair
# Maven command:
mvn flyway:repair

# Programmatic repair during emergency startup bean:
```

```java
package com.example.db.config;

import org.flywaydb.core.Flyway;
import org.springframework.boot.autoconfigure.flyway.FlywayMigrationStrategy;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Profile;

@Configuration
public class FlywayRepairConfig {

    @Bean
    @Profile("emergency-repair")
    public FlywayMigrationStrategy repairAndMigrateStrategy() {
        return flyway -> {
            // Re-aligns flyway_schema_history checksums with current classpath files
            flyway.repair();
            flyway.migrate();
        };
    }
}
```

### 5.4 Anti-Pattern: Using ddl-auto=update in Production
```yaml
# =========================================================================
# ANTI-PATTERN: spring.jpa.hibernate.ddl-auto=update in Production
# =========================================================================
spring:
  jpa:
    hibernate:
      ddl-auto: update # CATASTROPHIC IN PRODUCTION!

/*
 * WHY THIS FAILS IN PRODUCTION:
 * 1. Hibernate cannot DROP columns: Renaming a property creates a new column
 *    and leaves the old column with stale data.
 * 2. It cannot perform data migrations: Splitting 'name' into 'firstName' and 'lastName'
 *    leaves the new columns NULL.
 * 3. It cannot alter column types: Changing an INT to BIGINT is ignored or fails.
 * 4. It holds exclusive locks during app bootstrap, causing downtime.
 * 5. Multiple server instances starting concurrently execute ALTER TABLE simultaneously,
 *    causing schema deadlocks.
 */
```

### 5.5 Zero-Downtime Expand-and-Contract Migration Workflow
```sql
-- =========================================================================
-- ZERO-DOWNTIME STEP 1: EXPAND (Version V2__expand_phone_country_code.sql)
-- Goal: Split phone_number into phone_country_code and phone_national_number
-- =========================================================================
-- Add new columns as NULLABLE so older running application instances can continue writing!
ALTER TABLE customers 
    ADD COLUMN phone_country_code VARCHAR(8),
    ADD COLUMN phone_national_number VARCHAR(32);

-- =========================================================================
-- ZERO-DOWNTIME STEP 2: BACKFILL (Asynchronous background script or V2_1)
-- =========================================================================
-- Populate new columns from legacy column for existing rows:
UPDATE customers 
SET phone_country_code = '+1', 
    phone_national_number = SUBSTRING(legacy_phone FROM 3)
WHERE legacy_phone IS NOT NULL 
  AND phone_national_number IS NULL;

-- =========================================================================
-- ZERO-DOWNTIME STEP 3: CONTRACT (Applied 2 releases later in V4__contract.sql)
-- Once all application instances only read/write new columns:
-- =========================================================================
-- 1. Enforce NOT NULL constraints safely:
ALTER TABLE customers ALTER COLUMN phone_country_code SET NOT NULL;
ALTER TABLE customers ALTER COLUMN phone_national_number SET NOT NULL;

-- 2. Drop the deprecated legacy column:
ALTER TABLE customers DROP COLUMN legacy_phone;
```

---

## 6. Best Practices

1. **Set `spring.jpa.hibernate.ddl-auto = validate` in Production**: Hibernate should verify that the JPA entities match the database schema, but must never attempt to modify the schema.
2. **Never Modify an Applied Migration Script**: Once a migration script has run in any environment (including CI or staging), it is immutable. Create a new `V{N+1}__...` script for fixes.
3. **Always Add New Columns as Nullable Initially**: Adding non-nullable columns without default values causes table rewrites and breaks older running application instances during rolling blue/green deployments.
4. **Create Indexes Concurrently in Production (PostgreSQL)**: Use `CREATE INDEX CONCURRENTLY` in non-transactional migrations to avoid locking tables during index creation on large tables.
5. **Keep Repeatable Migrations Idempotent**: Always use `CREATE OR REPLACE VIEW` or `DROP ... IF EXISTS` inside `R__` repeatable migrations.
6. **Integrate Flyway with Testcontainers in CI**: Run all migration scripts against real PostgreSQL/MySQL Testcontainers instances in pull request automated test suites.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Editing an Already-Applied Migration File** | Changes the file's CRC32 checksum; Flyway detects validation failure and refuses to start. | Revert changes or create a new forward migration script. |
| **Single Underscore Instead of Double Underscore** | Naming a file `V1_init.sql` instead of `V1__init.sql` causes Flyway to ignore the script completely. | Adhere strictly to the double-underscore convention: `V1__description.sql`. |
| **Adding a `NOT NULL` Column on a Large Table Without Default** | In older databases, forces a full table rewrite, acquiring an exclusive lock and freezing production traffic. | Add column as nullable, backfill data, then add `NOT NULL` in a subsequent release. |
| **Creating Indexes Inside Standard DDL Transactions** | Standard `CREATE INDEX` locks the table for writes until the index completes. | Use `CREATE INDEX CONCURRENTLY` in PostgreSQL (requires `flyway.mixed=true`). |
| **Branch Merge Conflicts with Migration Versions** | Two developers merge branches both containing `V3__...`, causing duplicate version errors in CI. | Use timestamp-based versions (e.g. `V202611241200__...`) or set `out-of-order = true`. |
| **Forgetting to Install Database-Specific Flyway Dependency** | In Flyway 10+, database engines were split into separate modules (e.g. `flyway-database-postgresql`). | Add the appropriate `flyway-database-{dialect}` dependency. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Startup Lag on Clean State**: Flyway checks the `flyway_schema_history` table in milliseconds ($<10\text{ ms}$). If no pending scripts exist, startup proceeds with zero overhead.
- **Locking Overhead on High-Volume Tables**: DDL operations that acquire table access exclusive locks (`ACCESS EXCLUSIVE`) stall incoming read/write queries. Always inspect query execution locks before running migrations on tables with $>10,000,000$ rows.

### Security
- **Restricted Migration User**: In mature enterprise environments, the database user executing Flyway migrations (with `ALTER` and `CREATE` privileges) should be distinct from the day-to-day application user (which should have only `SELECT`, `INSERT`, `UPDATE`, `DELETE` grants).

---

## 9. Comparison / When to Use What

| Feature | Flyway | Liquibase | Hibernate ddl-auto=update |
| :--- | :--- | :--- | :--- |
| **Primary Format** | **Plain SQL (`.sql`)** | XML, YAML, JSON, or SQL | Generated from Java Annotations |
| **Learning Curve** | **Near Zero (Standard SQL)** | Moderate | Low |
| **Rollback Support** | Commercial only (or manual) | **Built-in (Community)** | None |
| **Checksum Validation** | Yes (Strict) | Yes | None |
| **Production Safe?** | **YES (Industry Standard)** | **YES (Industry Standard)** | **NEVER (Prohibited)** |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is <code>spring.jpa.hibernate.ddl-auto=update</code> strictly prohibited in production environments?</strong></summary>

`ddl-auto=update` has several catastrophic drawbacks:
1. It never drops columns or constraints, leaving legacy garbage columns and indexes behind.
2. It cannot execute data transformations (e.g., migrating a single column into two normalized columns).
3. It cannot alter column data types safely.
4. Concurrent startup of multiple application instances in a cluster causes simultaneous competing DDL statements, leading to database schema deadlocks.
5. It lacks auditability and version control.
</details>

<details>
<summary><strong>2. What is the difference between a Versioned migration and a Repeatable migration in Flyway?</strong></summary>

- **Versioned Migrations (`V`)**: Have unique, ordered version numbers (e.g., `V1__init.sql`). They are executed exactly once. Modifying an applied versioned script causes a checksum validation error.
- **Repeatable Migrations (`R`)**: Do not have version numbers (e.g., `R__views.sql`). They execute after all versioned migrations and are re-executed whenever their file checksum changes. They are used for views, procedures, and functions.
</details>

<details>
<summary><strong>3. What does <code>baseline-on-migrate: true</code> do in Flyway?</strong></summary>

When introducing Flyway to an existing, non-empty database that was created without Flyway, Flyway would normally fail because the schema history table does not exist. `baseline-on-migrate: true` tells Flyway to create the `flyway_schema_history` table and mark an initial "baseline" version (e.g., `0` or `1`), treating the existing schema as already applied and running only subsequent scripts.
</details>

<details>
<summary><strong>4. How does the Expand-and-Contract pattern achieve zero-downtime schema migrations?</strong></summary>

It breaks breaking changes into non-breaking, incremental phases:
1. **Expand**: Add new columns or tables as nullable. The old application and new application can both run simultaneously.
2. **Backfill**: Asynchronously copy and convert historical data to the new structure.
3. **Switchover**: Deploy the application version that reads exclusively from the new structure.
4. **Contract**: Once the old application instances are retired, drop the deprecated old column or table.
</details>

<details>
<summary><strong>5. How does Flyway detect that an applied migration file was modified?</strong></summary>

When Flyway executes a script, it calculates a CRC32 checksum of the file contents and stores it in the `flyway_schema_history` table. On subsequent application boots, it re-calculates the checksums of all classpath files and compares them against the database. If any checksum does not match, it throws a `FlywayValidateException` and halts startup.
</details>

<details>
<summary><strong>6. What is the purpose of the <code>flyway repair</code> command?</strong></summary>

`flyway repair` performs two functions:
1. Re-aligns checksums in the `flyway_schema_history` table with the current versions of files on the classpath (used to repair accidental comment/whitespace edits).
2. Removes metadata entries for failed migrations in databases that do not support DDL transactions.
</details>

<details>
<summary><strong>7. Why should you avoid executing <code>CREATE INDEX</code> without <code>CONCURRENTLY</code> in PostgreSQL production migrations?</strong></summary>

A standard `CREATE INDEX` acquires a `SHARE` lock on the target table, blocking all concurrent `INSERT`, `UPDATE`, and `DELETE` transactions until the entire index is built. On a table with millions of rows, this can block writes for minutes. `CREATE INDEX CONCURRENTLY` builds the index without blocking writes.
</details>

<details>
<summary><strong>8. What is the difference between Flyway and Liquibase?</strong></summary>

Flyway is SQL-centric; migrations are written in native, vendor-specific SQL scripts. Liquibase is format-agnostic; changesets can be written in XML, YAML, JSON, or SQL, allowing abstract database operations that Liquibase translates into vendor-specific dialects. Liquibase also has superior free-tier rollback support.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Initial Flyway Migration Script</strong></summary>

**Task**: Write a `V1__init_user_table.sql` script creating a `users` table with `id`, `username`, `email`, and `created_at` fields in PostgreSQL dialect.

```sql
-- src/main/resources/db/migration/V1__init_user_table.sql
CREATE TABLE users (
    id BIGSERIAL PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    email VARCHAR(255) NOT NULL UNIQUE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW() NOT NULL
);
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Adding an Index via Versioned Migration</strong></summary>

**Task**: Write `V2__add_users_email_index.sql` to add a secondary B-Tree index on `email`.

```sql
-- src/main/resources/db/migration/V2__add_users_email_index.sql
CREATE INDEX idx_users_email ON users(email);
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Repeatable Migration for Stored Function</strong></summary>

**Task**: Create a repeatable migration `R__calculate_discount_function.sql` that defines or replaces a PostgreSQL stored function calculating discounts.

```sql
-- src/main/resources/db/migration/R__calculate_discount_function.sql
CREATE OR REPLACE FUNCTION calculate_discount(amount NUMERIC, rate NUMERIC)
RETURNS NUMERIC AS $$
BEGIN
    RETURN amount * (rate / 100.0);
END;
$$ LANGUAGE plpgsql;
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Flyway Baseline and Validate Configuration</strong></summary>

**Task**: Configure `application.yaml` to enable Flyway with `baseline-on-migrate: true`, `validate-on-migrate: true`, and Hibernate `ddl-auto: validate`.

```yaml
spring:
  jpa:
    hibernate:
      ddl-auto: validate
  flyway:
    enabled: true
    baseline-on-migrate: true
    validate-on-migrate: true
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Zero-Downtime Column Rename Simulation</strong></summary>

**Task**: Write the three sequential SQL migration scripts (`V3__expand.sql`, `V3_1__backfill.sql`, and `V4__contract.sql`) to rename `name` to `display_name` without zero downtime.

```sql
-- V3__expand.sql
ALTER TABLE users ADD COLUMN display_name VARCHAR(100);

-- V3_1__backfill.sql
UPDATE users SET display_name = name WHERE display_name IS NULL;

-- V4__contract.sql (Applied after app deployment)
ALTER TABLE users ALTER COLUMN display_name SET NOT NULL;
ALTER TABLE users DROP COLUMN name;
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Automated Testcontainers Migration Integration Test</strong></summary>

**Task**: Write a JUnit 5 test using Testcontainers PostgreSQL that starts a clean database, runs Flyway migrations, and asserts all tables and views exist.

```java
@SpringBootTest
@Testcontainers
class FlywayMigrationIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private JdbcClient jdbcClient;

    @Test
    void testAllMigrationsAppliedSuccessfully() {
        // Assert flyway_schema_history has records and success status
        Integer successCount = jdbcClient.sql("SELECT COUNT(*) FROM flyway_schema_history WHERE success = true")
            .query(Integer.class)
            .single();

        assertThat(successCount).isGreaterThanOrEqualTo(1);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Golden Rule 1: Set spring.jpa.hibernate.ddl-auto = validate in all production setups  |
| Golden Rule 2: NEVER modify an applied migration file (checksum validation will fail) |
| File Naming:   V{Version}__{Description}.sql (Strict double underscore '__')          |
| Repeatable:    R__{Description}.sql (Re-runs automatically when content changes)      |
| Zero-Downtime: Expand (add nullable) -> Backfill -> Switchover -> Contract (drop old)  |
| Baseline:      flyway.baseline-on-migrate: true (for existing legacy databases)       |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Flyway Official Documentation](https://documentation.red-gate.com/fd/)
- [Martin Fowler: Evolutionary Database Design](https://martinfowler.com/articles/evodb.html)
- [Zero Downtime Database Migrations with Spring Boot and Flyway](https://spring.io/blog/2016/05/31/zero-downtime-deployment-with-a-database)
- [PostgreSQL Index Creation Concurrency Guidelines](https://www.postgresql.org/docs/current/sql-createindex.html#SQL-CREATEINDEX-CONCURRENTLY)
