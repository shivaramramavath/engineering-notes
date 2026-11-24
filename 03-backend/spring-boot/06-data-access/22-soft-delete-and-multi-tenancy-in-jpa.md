# Soft Delete & Multi-Tenancy in JPA: Hibernate 6.3+ Annotations & Isolation Models

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | `03-spring-data-jpa-fundamentals.md`, `04-entity-mapping-and-relationships.md`, `17-multiple-datasources.md` |
| **Estimated Reading Time** | 31 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Hibernate 6.6.x, PostgreSQL 16, Spring Boot 3.4.x |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Modern Soft Delete with @SQLDelete and @SQLRestriction](#51-modern-soft-delete-with-sqldelete-and-sqlrestriction)
   - [5.2 Resolving Unique Constraint Collisions with PostgreSQL Partial Indexes](#52-resolving-unique-constraint-collisions-with-postgresql-partial-indexes)
   - [5.3 Schema-Per-Tenant Multi-Tenancy with CurrentTenantIdentifierResolver](#53-schema-per-tenant-multi-tenancy-with-currenttenantidentifierresolver)
   - [5.4 Anti-Pattern: Using Deprecated Hibernate @Where](#54-anti-pattern-using-deprecated-hibernate-where)
   - [5.5 Row-Level Discriminator Multi-Tenancy with Hibernate Filters](#55-row-level-discriminator-multi-tenancy-with-hibernate-filters)
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
In enterprise applications, physical `DELETE` statements are rarely acceptable: historical data must be preserved for compliance audits, accident recovery ("undo"), and referential integrity with past invoices or transactions. Simultaneously, Software-as-a-Service (SaaS) platforms require serving hundreds or thousands of tenant organizations from a unified backend while guaranteeing strict data isolation.

### Historical Evolution
Historically, developers implemented soft deletion using Hibernate's proprietary `@Where(clause = "deleted = false")`. In Hibernate 6.3+, `@Where` was deprecated and replaced by **`@SQLRestriction`** and **`@SQLDelete`**, integrating with the Semantic Query Model (SQM) to ensure filter predicates are applied consistently across polymorphic hierarchies and joins. For multi-tenancy, Hibernate standardized SPI interfaces—`CurrentTenantIdentifierResolver` and `MultiTenantConnectionProvider`—to support database, schema, and row-level tenant isolation natively.

### Real-World Relevance
Regulatory standards (GDPR, SOC2, HIPAA) demand data provenance and strict tenant boundaries. Implementing automated soft deletion preserves historical associations, while multi-tenant isolation ensures one tenant's queries can never access or corrupt another tenant's proprietary data.

---

## 2. Core Concepts

### Soft Delete Mechanics
Instead of issuing a physical `DELETE FROM users WHERE id = ?`, Hibernate intercepts the delete operation and executes an `UPDATE`:
- `@SQLDelete(sql = "UPDATE users SET deleted = true WHERE id = ?")`: Overrides the delete command.
- `@SQLRestriction("deleted = false")`: Automatically appends `AND deleted = false` to all generated `SELECT` statements, derived queries, and relationship joins.

### Multi-Tenancy Architecture Models
1. **Database-Per-Tenant**: Each tenant has a completely separate physical database instance. Maximum isolation, highest hardware cost.
2. **Schema-Per-Tenant**: All tenants share a database instance, but each tenant has an isolated SQL schema (`tenant_a.users`, `tenant_b.users`). Strong isolation with shared memory.
3. **Discriminator Column (Row-Level)**: All tenants share the same tables; every row includes a `tenant_id` foreign key. Low cost, but requires rigorous filtering to prevent cross-tenant data leaks.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                      Hibernate Multi-Tenancy & Soft Deletion                       |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  1. Incoming Request with Tenant Header (X-Tenant-ID: "acme_corp")                 |
|     - TenantFilter sets TenantContext.setTenant("acme_corp")                       |
|                                                                                    |
|  2. Transaction Begins (@Transactional)                                            |
|     - CurrentTenantIdentifierResolver resolves "acme_corp"                         |
|     - MultiTenantConnectionProvider checks out connection from pool                |
|     - Executes: connection.setSchema("acme_corp");                                 |
|                                                                                    |
|  3. Entity Query Execution: userRepository.findAll()                               |
|     - Hibernate SQM compiles query:                                                |
|       SELECT u.id, u.name FROM acme_corp.users u WHERE u.deleted = false;          |
|       (Automatically applies @SQLRestriction!)                                     |
|                                                                                    |
|  4. Entity Deletion: userRepository.deleteById(10L)                                |
|     - Hibernate intercepts DELETE action:                                          |
|       UPDATE acme_corp.users SET deleted = true WHERE id = 10;                      |
|       (Executes @SQLDelete override!)                                              |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Modern Hibernate 6.3+ Soft Delete Annotations
```java
@Entity
@Table(name = "users")
@SQLDelete(sql = "UPDATE users SET deleted = true, deleted_at = NOW() WHERE id = ?")
@SQLRestriction("deleted = false")
public class User {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false)
    private boolean deleted = Boolean.FALSE;

    private Instant deletedAt;
}
```

---

## 5. Practical Examples

### 5.1 Modern Soft Delete with @SQLDelete and @SQLRestriction
```java
package com.example.jpa.domain;

import jakarta.persistence.*;
import org.hibernate.annotations.SQLDelete;
import org.hibernate.annotations.SQLRestriction;

import java.time.Instant;

@Entity
@Table(name = "articles")
// Override delete statement
@SQLDelete(sql = "UPDATE articles SET is_deleted = true, deleted_at = NOW() WHERE id = ?")
// Automatic filter on all queries and joins
@SQLRestriction("is_deleted = false")
public class Article {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false)
    private String title;

    @Column(name = "is_deleted", nullable = false)
    private boolean deleted = false;

    @Column(name = "deleted_at")
    private Instant deletedAt;

    protected Article() {}

    public Article(String title) {
        this.title = title;
    }

    public Long getId() { return id; }
    public String getTitle() { return title; }
    public boolean isDeleted() { return deleted; }
}
```

### 5.2 Resolving Unique Constraint Collisions with PostgreSQL Partial Indexes
```sql
-- Problem: A user with email 'alice@example.com' is soft-deleted.
-- A new user attempts to register with 'alice@example.com'.
-- A standard UNIQUE (email) constraint throws a duplicate key error on the deleted row!

-- PRODUCTION REMEDY: PostgreSQL Partial Unique Index
-- Only enforces uniqueness across ACTIVE (non-deleted) records!
CREATE UNIQUE INDEX idx_unique_active_email 
ON users (email) 
WHERE is_deleted = false;
```

### 5.3 Schema-Per-Tenant Multi-Tenancy with CurrentTenantIdentifierResolver
```java
package com.example.jpa.multitenant;

import org.hibernate.context.spi.CurrentTenantIdentifierResolver;
import org.springframework.stereotype.Component;

@Component
public class HeaderTenantIdentifierResolver implements CurrentTenantIdentifierResolver<String> {

    private static final String DEFAULT_TENANT = "public";

    @Override
    public String resolveCurrentTenantIdentifier() {
        String tenant = TenantContext.getTenant();
        return (tenant != null && !tenant.isBlank()) ? tenant : DEFAULT_TENANT;
    }

    @Override
    public boolean validateExistingCurrentSessions() {
        return true;
    }
}
```

```java
package com.example.jpa.multitenant;

import org.hibernate.engine.jdbc.connections.spi.MultiTenantConnectionProvider;
import org.springframework.stereotype.Component;

import javax.sql.DataSource;
import java.sql.Connection;
import java.sql.SQLException;

@Component
public class SchemaMultiTenantConnectionProvider implements MultiTenantConnectionProvider<String> {

    private final DataSource dataSource;

    public SchemaMultiTenantConnectionProvider(DataSource dataSource) {
        this.dataSource = dataSource;
    }

    @Override
    public Connection getAnyConnection() throws SQLException {
        return dataSource.getConnection();
    }

    @Override
    public void releaseAnyConnection(Connection connection) throws SQLException {
        connection.close();
    }

    @Override
    public Connection getConnection(String tenantIdentifier) throws SQLException {
        Connection connection = getAnyConnection();
        // Dynamically switch the active PostgreSQL schema for this connection!
        connection.setSchema(tenantIdentifier);
        return connection;
    }

    @Override
    public void releaseConnection(String tenantIdentifier, Connection connection) throws SQLException {
        // Reset schema before returning connection to HikariCP pool
        connection.setSchema("public");
        connection.close();
    }

    @Override
    public boolean supportsAggressiveRelease() { return true; }
    @Override
    public boolean isUnwrappableAs(Class<?> unwrapType) { return false; }
    @Override
    public <T> T unwrap(Class<T> unwrapType) { return null; }
}
```

### 5.4 Anti-Pattern: Using Deprecated Hibernate @Where
```java
// =========================================================================
// ANTI-PATTERN: Using deprecated @Where in Hibernate 6.x
// =========================================================================
@Entity
// DEPRECATED IN HIBERNATE 6.3:
// @Where uses legacy string injection that often fails during complex HQL/SQM joins,
// cannot be disabled per query, and causes syntax errors in joined-subclass inheritance.
@org.hibernate.annotations.Where(clause = "deleted = false")
public class BadSoftDeleteEntity {
    @Id private Long id;
    private boolean deleted;
}

// =========================================================================
// PRODUCTION FIX: Modern @SQLRestriction Annotation
// =========================================================================
@Entity
// SAFE: Fully integrated into the Semantic Query Model (SQM) AST
@org.hibernate.annotations.SQLRestriction("deleted = false")
public class GoodSoftDeleteEntity {
    @Id private Long id;
    private boolean deleted;
}
```

### 5.5 Row-Level Discriminator Multi-Tenancy with Hibernate Filters
```java
package com.example.jpa.domain;

import jakarta.persistence.*;
import org.hibernate.annotations.Filter;
import org.hibernate.annotations.FilterDef;
import org.hibernate.annotations.ParamDef;

@Entity
@Table(name = "orders")
@FilterDef(
    name = "tenantFilter", 
    parameters = @ParamDef(name = "tenantId", type = String.class)
)
@Filter(name = "tenantFilter", condition = "tenant_id = :tenantId")
public class MultiTenantOrder {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "tenant_id", nullable = false, updatable = false)
    private String tenantId;

    private String orderNumber;

    @PrePersist
    public void assignTenant() {
        if (this.tenantId == null) {
            this.tenantId = com.example.jpa.multitenant.TenantContext.getTenant();
        }
    }
}
```

---

## 6. Best Practices

1. **Use `@SQLRestriction` Instead of `@Where`**: Migrate all soft deletion clauses to Hibernate 6.3+ `@SQLRestriction`.
2. **Use Partial Unique Indexes for Soft-Deleted Entities**: In PostgreSQL, always use `CREATE UNIQUE INDEX ... WHERE is_deleted = false` to allow re-registration of soft-deleted emails or unique codes.
3. **Always Reset Connection State in `releaseConnection`**: When using schema-per-tenant connection routing, always reset `connection.setSchema("public")` before releasing to the HikariCP pool.
4. **Use Partial Filtering with `@Filter` for Row-Level Multi-Tenancy**: If all tenants share a table, enforce tenant boundaries via Hibernate `@FilterDef` and `@Filter`, enabling the filter in a transactional interceptor.
5. **Mark `@Column(name = "tenant_id", updatable = false)`**: Prevent malicious or accidental updates to the tenant discriminator column.
6. **Provide Administrative Bypass for Soft Deletion**: When database cleanup or GDPR compliance requires physical deletion, use native queries or raw `JdbcClient` to bypass `@SQLDelete`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using Standard UNIQUE Constraint with Soft Deletes** | Deleting a record leaves the unique column occupied, preventing new records with that value. | Create a database partial unique index with `WHERE deleted = false`. |
| **Forgetting to Reset Schema on Connection Release** | Connection is returned to HikariCP with the previous tenant's schema, causing cross-tenant leaks on subsequent requests. | Execute `connection.setSchema("public")` in `releaseConnection`. |
| **ThreadLocal Memory Leak in TenantContext** | ThreadLocal retaining tenant identifier bleeds into subsequent worker tasks in thread pools. | Always invoke `TenantContext.clear()` in servlet filter `finally` blocks. |
| **Direct Native Queries Bypassing `@SQLRestriction`** | Native SQL queries (`nativeQuery = true`) bypass Hibernate SQM, exposing soft-deleted rows! | Explicitly append `AND is_deleted = false` in native SQL statements. |
| **Soft-Deleting Parent Without Cascading Children** | Soft-deleting an `Order` leaves its `OrderItem` records visible to queries. | Soft delete children or add `@SQLRestriction` to child entities as well. |
| **Missing Parameter Placeholder in `@SQLDelete`** | The custom SQL string must contain an exact parameter placeholder `?` for the entity primary key. | Verify `@SQLDelete(sql = "UPDATE ... WHERE id = ?")`. |

---

## 8. Performance & Security Notes

### Performance
- **Partial Index Efficiency**: Partial indexes (`WHERE is_deleted = false`) are significantly smaller than full indexes because they exclude millions of historical soft-deleted records, keeping index leaf pages memory-resident.
- **Connection Schema Switching**: In PostgreSQL, `connection.setSchema()` executes an internal `SET search_path` command, incurring negligible overhead ($<0.1\text{ ms}$).

### Security
- **Cross-Tenant Data Leak Prevention**: Strict schema separation provides hard relational boundaries. Even if a developer forgets a `WHERE` clause in a custom query, the database engine physically isolates access to the current tenant schema.
- **GDPR "Right to be Forgotten"**: Soft deletion does *not* satisfy GDPR erasure mandates. Applications must provide an audited "Permanent Purge" feature that executes physical DDL `DELETE` statements when requested by data subjects.

---

## 9. Comparison / When to Use What

| Architecture Model | Isolation Level | Hardware Cost | Schema Complexity | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Database-Per-Tenant** | **Highest (Physical DB)** | High | Low | Enterprise banks, government contracts |
| **Schema-Per-Tenant** | **High (SQL Schema)** | Moderate | Moderate | B2B SaaS platforms |
| **Discriminator Column** | Low (Application level) | **Lowest** | High (Requires filters everywhere) | Massive B2C consumer platforms |
| **Soft Delete** | Logical flag | None | Low | Entities requiring audit/recovery |

---

## 10. Interview Questions

<details>
<summary><strong>1. How does Hibernate 6.3+ handle soft deletion compared to older versions?</strong></summary>

Older Hibernate versions relied on `@SQLDelete` paired with `@Where(clause = "deleted = false")`. Hibernate 6.3 deprecated `@Where` in favor of `@SQLRestriction`. `@SQLRestriction` integrates directly into Hibernate's Semantic Query Model (SQM) Abstract Syntax Tree, ensuring that the filter predicate is correctly applied to polymorphic inheritance hierarchies, subqueries, and join fetch criteria without string-injection side effects.
</details>

<details>
<summary><strong>2. How do you resolve unique constraint conflicts when soft deletion is enabled?</strong></summary>

In relational databases like PostgreSQL, you replace standard table-level unique constraints with **Partial Unique Indexes**:
```sql
CREATE UNIQUE INDEX idx_user_email_active ON users (email) WHERE is_deleted = false;
```
This enforces uniqueness exclusively across active rows, allowing soft-deleted historical records to retain duplicate values.
</details>

<details>
<summary><strong>3. What are the three primary multi-tenancy models supported by Hibernate?</strong></summary>

1. **Database-per-tenant**: Separate physical database per tenant.
2. **Schema-per-tenant**: Separate SQL schema per tenant within a single shared database.
3. **Discriminator column (Row-level)**: Shared database and tables with a `tenant_id` column distinguishing rows.
</details>

<details>
<summary><strong>4. What is the role of <code>CurrentTenantIdentifierResolver</code> in Hibernate?</strong></summary>

It is an SPI interface implemented by the application to inform Hibernate which tenant context is currently active for the calling thread (typically extracting the tenant identifier from a `ThreadLocal` populated by an incoming HTTP header).
</details>

<details>
<summary><strong>5. What is the role of <code>MultiTenantConnectionProvider</code>?</strong></summary>

It provides database connections configured for the specific tenant identifier resolved by `CurrentTenantIdentifierResolver`. In schema-per-tenant setups, it checks out a connection from the pool and invokes `connection.setSchema(tenantId)` before returning it to Hibernate.
</details>

<details>
<summary><strong>6. Do Spring Data native queries honor <code>@SQLRestriction</code>?</strong></summary>

No. Native SQL queries (`@Query(nativeQuery = true)`) bypass Hibernate's SQM compiler and execute raw SQL directly on the JDBC driver. They do not inherit `@SQLRestriction`. Developers must explicitly include `WHERE is_deleted = false` in all native SQL statements.
</details>

<details>
<summary><strong>7. How can you perform a physical hard delete on an entity annotated with <code>@SQLDelete</code>?</strong></summary>

Since `entityManager.remove()` triggers the custom `@SQLDelete` update, a physical deletion requires executing a direct JPQL/Native `@Modifying` query (`DELETE FROM User u WHERE u.id = :id`) or using Spring's `JdbcClient` to bypass Hibernate entity lifecycle overrides.
</details>

<details>
<summary><strong>8. Why is resetting the schema in <code>releaseConnection</code> mandatory for connection pools?</strong></summary>

HikariCP connection pools reuse physical connections across multiple threads and requests. If a connection is returned to the pool while bound to `tenant_a`, and the next HTTP request for `tenant_b` borrows that connection, any un-scoped query could execute in `tenant_a`'s schema, resulting in a critical security breach.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Soft-Deleted Entity</strong></summary>

**Task**: Create an entity `Customer` with `@SQLDelete` setting `deleted = true` and `@SQLRestriction` filtering on `deleted = false`.

```java
@Entity
@SQLDelete(sql = "UPDATE customers SET deleted = true WHERE id = ?")
@SQLRestriction("deleted = false")
public class Customer {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;
    private String name;
    private boolean deleted = false;
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Tenant Context ThreadLocal Holder</strong></summary>

**Task**: Implement a thread-safe `TenantStorage` class with `set(String id)`, `get()`, and `clear()` methods.

```java
public final class TenantStorage {
    private static final ThreadLocal<String> STORAGE = new ThreadLocal<>();
    public static void set(String id) { STORAGE.set(id); }
    public static String get() { return STORAGE.get(); }
    public static void clear() { STORAGE.remove(); }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Soft-Delete Unit Verification Test</strong></summary>

**Task**: Write an integration test asserting that calling `repository.delete(customer)` executes an update, and subsequent `repository.findAll()` excludes the deleted customer.

```java
@Test
@Transactional
void testSoftDelete() {
    Customer customer = repository.save(new Customer("Acme"));
    repository.delete(customer);
    entityManager.flush();
    entityManager.clear();

    assertThat(repository.findAll()).isEmpty();
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Tenant Filter Servlet Configuration</strong></summary>

**Task**: Create a Spring filter that extracts `X-Tenant-Slug` and sets it in `TenantStorage`, ensuring cleanup in `finally`.

```java
@Component
public class TenantInterceptorFilter extends OncePerRequestFilter {
    @Override
    protected void doFilterInternal(HttpServletRequest req, HttpServletResponse res, FilterChain chain)
            throws ServletException, IOException {
        String slug = req.getHeader("X-Tenant-Slug");
        if (slug != null) TenantStorage.set(slug);
        try {
            chain.doFilter(req, res);
        } finally {
            TenantStorage.clear();
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Hard Delete Administrative Repository Method</strong></summary>

**Task**: Implement a repository method that executes an un-intercepted physical `DELETE` for GDPR compliance on a soft-deleted entity.

```java
public interface CustomerRepository extends JpaRepository<Customer, Long> {
    @Modifying
    @Transactional
    @Query(value = "DELETE FROM customers WHERE id = :id", nativeQuery = true)
    int physicalPurgeForGdpr(@Param("id") Long id);
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Hibernate CurrentTenantIdentifierResolver Implementation</strong></summary>

**Task**: Write a complete `CurrentTenantIdentifierResolver` that resolves the active tenant, falling back to `"public"` when unauthenticated.

```java
@Component
public class DynamicTenantResolver implements CurrentTenantIdentifierResolver<String> {
    @Override
    public String resolveCurrentTenantIdentifier() {
        String tenant = TenantStorage.get();
        return (tenant != null && !tenant.isBlank()) ? tenant : "public";
    }

    @Override
    public boolean validateExistingCurrentSessions() {
        return true;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Soft Delete:     @SQLDelete(sql = "UPDATE table SET deleted = true WHERE id = ?")     |
| Auto Filter:     @SQLRestriction("deleted = false") (Modern replacement for @Where)   |
| Unique Trap:     Use PostgreSQL Partial Unique Indexes (WHERE deleted = false)        |
| Multi-Tenancy:   CurrentTenantIdentifierResolver + MultiTenantConnectionProvider      |
| Schema Switch:   connection.setSchema(tenantId) -> Reset to "public" on release!      |
| Native Caveat:   Native queries bypass @SQLRestriction; add manual WHERE clauses!     |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Hibernate 6.6 User Guide: Soft Deletion and Filters](https://docs.jboss.org/hibernate/orm/6.6/userguide/html_single/Hibernate_User_Guide.html#filters)
- [Vlad Mihalcea: The Best Way to Soft Delete with Hibernate @SQLDelete and @SQLRestriction](https://vladmihalcea.com/the-best-way-to-soft-delete-with-hibernate/)
- [Hibernate 6.6 Multi-Tenancy Reference Guide](https://docs.jboss.org/hibernate/orm/6.6/userguide/html_single/Hibernate_User_Guide.html#multitenancy)
- [PostgreSQL Partial Indexes Documentation](https://www.postgresql.org/docs/current/indexes-partial.html)
