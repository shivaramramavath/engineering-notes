# Multi-Tenancy Architecture in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | Hibernate 6.x, PostgreSQL (Row-Level Security / Schemas), HikariCP |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
When building Software-as-a-Service (SaaS) platforms, multiple distinct client organizations (**tenants**) must share the same application infrastructure. Deploying a separate, dedicated application server and database cluster for each tenant leads to astronomical cloud hosting costs, complex deployment automation, and severe operational overhead. Conversely, pooling all customer data into a shared application tier introduces the catastrophic risk of **cross-tenant data leakage** (e.g., Tenant A accidentally viewing Tenant B's confidential financial orders).

### The Evolution
Modern SaaS applications solve this dilemma through **Multi-Tenancy Architectures**. A single running instance of a Spring Boot microservice securely serves requests for thousands of tenants while enforcing strict data isolation at the routing, persistence, and database layers.

### Production Relevance
Selecting and implementing the correct multi-tenancy model is one of the most critical foundational architecture decisions for any B2B SaaS startup or enterprise cloud service. It impacts database schema migration complexity, tenant onboarding velocity, compliance certifications (SOC2, HIPAA, GDPR), and infrastructure cost efficiency.

---

## 2. Core Concepts

### The Three Multi-Tenancy Models

```
+-----------------------------------------------------------------------------------------+
|                                 MULTI-TENANCY MODELS                                    |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  MODEL 1: DISCRIMINATOR COLUMN (SHARED DATABASE, SHARED TABLE)                          |
|  +-----------------------------------------------------------------------------------+  |
|  |  PostgreSQL Database: saas_db                                                      |  |
|  |  Table: orders                                                                    |  |
|  |  +----+-----------+-------------+--------+                                        |  |
|  |  | id | tenant_id | customer_id | amount |  <-- All tenants share identical table |  |
|  |  +----+-----------+-------------+--------+                                        |  |
|  |  | 1  | "tenant_a"| "cust-1"    | $50.00 |  <-- Isolation enforced via WHERE / RLS|  |
|  |  | 2  | "tenant_b"| "cust-9"    | $90.00 |                                        |  |
|  |  +----+-----------+-------------+--------+                                        |  |
|  +-----------------------------------------------------------------------------------+  |
|                                                                                         |
|  MODEL 2: SCHEMA-PER-TENANT (SHARED DATABASE, SEPARATE SCHEMAS)                         |
|  +-----------------------------------------------------------------------------------+  |
|  |  PostgreSQL Database: saas_db                                                      |  |
|  |  +---------------------------+       +---------------------------+                |  |
|  |  | Schema: tenant_a_schema   |       | Schema: tenant_b_schema   |                |  |
|  |  |   Table: orders           |       |   Table: orders           |                |  |
|  |  +---------------------------+       +---------------------------+                |  |
|  +-----------------------------------------------------------------------------------+  |
|                                                                                         |
|  MODEL 3: DATABASE-PER-TENANT (SEPARATE DATABASES)                                      |
|  +---------------------------------+    +---------------------------------+             |  |
|  | Database: tenant_a_prod_db      |    | Database: tenant_b_prod_db      |             |  |
|  |   (Dedicated Connection Pool)   |    |   (Dedicated Connection Pool)   |             |  |
|  +---------------------------------+    +---------------------------------+             |  |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

### Key Architectural Invariants
- **Zero Cross-Tenant Leakage**: Under no circumstances may a SQL query execute without a valid tenant context filter.
- **Tenant Context Thread-Safety**: The tenant identifier must be securely extracted at the HTTP/gRPC boundary, stored in a thread-safe context (`ThreadLocal`), and reliably cleaned up after request execution to prevent thread-pool pollution.
- **Automated Tenant Onboarding**: Creating a new tenant must be an automated, scriptable API operation without requiring manual database administrator intervention.

---

## 3. How It Works Internally

### Request Interception & Dynamic Routing Lifecycle

```
[ Incoming HTTP Request: Header "X-Tenant-ID: acme_corp" ]
                         |
                         v
[ TenantInterceptor (HandlerInterceptor) ]
   1. Extracts "acme_corp" from Header / JWT / Subdomain
   2. Validates tenant existence in TenantRegistryCache
   3. Sets TenantContext.setCurrentTenant("acme_corp")
                         |
                         v
[ Spring Service / JpaRepository Method Execution ]
                         |
                         v
[ Hibernate MultiTenancyConnectionProvider / AbstractRoutingDataSource ]
   1. Reads TenantContext.getCurrentTenant()
   2. Selects target DataSource or sets SQL search path:
      "SET search_path TO acme_corp, public;" OR switches Connection Pool
                         |
                         v
[ PostgreSQL Execution (Isolated Tenant Data) ]
                         |
                         v
[ TenantInterceptor.afterCompletion() ]
   - CRITICAL: TenantContext.clear() (Prevents ThreadLocal leak into thread pool!)
```

1. **Resolution**: The tenant identifier is extracted from the incoming request (via HTTP header `X-Tenant-ID`, subdomain `acme.myplatform.com`, or signed JWT claim).
2. **Context Storage**: Stored in a `TenantContext` backed by Java `ThreadLocal` or Virtual Thread scoped values.
3. **Hibernate 6 Routing**: Hibernate provides native multi-tenancy SPI contracts:
   - `CurrentTenantIdentifierResolver`: Resolves the tenant ID string for the active session.
   - `MultiTenancyConnectionProvider`: Provides JDBC connections matching the resolved tenant.
4. **Context Teardown**: An interceptor `afterCompletion()` callback always calls `TenantContext.clear()` to prevent tenant identity leakage when worker threads are returned to the Tomcat/Virtual thread pool.

---

## 4. Syntax & Usage

### 1. Thread-Safe Tenant Context Holder

```java
package com.example.multitenant.context;

public final class TenantContext {

    private static final ThreadLocal<String> CURRENT_TENANT = new ThreadLocal<>();

    private TenantContext() {}

    public static void setCurrentTenant(String tenantId) {
        CURRENT_TENANT.set(tenantId);
    }

    public static String getCurrentTenant() {
        return CURRENT_TENANT.get();
    }

    public static void clear() {
        CURRENT_TENANT.remove();
    }
}
```

### 2. HTTP Request Tenant Extraction Interceptor

```java
package com.example.multitenant.web;

import com.example.multitenant.context.TenantContext;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.stereotype.Component;
import org.springframework.web.servlet.HandlerInterceptor;

@Component
public class TenantInterceptor implements HandlerInterceptor {

    public static final String TENANT_HEADER = "X-Tenant-ID";

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) {
        String tenantId = request.getHeader(TENANT_HEADER);
        if (tenantId == null || tenantId.isBlank()) {
            response.setStatus(HttpServletResponse.SC_BAD_REQUEST);
            return false;
        }
        TenantContext.setCurrentTenant(tenantId);
        return true;
    }

    @Override
    public void afterCompletion(HttpServletRequest request, HttpServletResponse response, Object handler, Exception ex) {
        // Prevent ThreadLocal memory and identity leaks!
        TenantContext.clear();
    }
}
```

---

## 5. Practical Examples

### Example 1: Model 1 - Discriminator Column with PostgreSQL Row-Level Security (RLS)

PostgreSQL native **Row-Level Security (RLS)** provides mathematically unbreakable multi-tenancy at the database engine level, eliminating developer error where someone forgets a `WHERE tenant_id = ?` clause!

#### PostgreSQL DDL:
```sql
-- Enable RLS on the table
ALTER TABLE orders ENABLE ROW LEVEL SECURITY;

-- Create policy enforcing tenant match against session setting
CREATE POLICY tenant_isolation_policy ON orders
    FOR ALL
    USING (tenant_id = current_setting('app.current_tenant', true));
```

#### Spring Connection Interceptor:
```java
package com.example.multitenant.rls;

import com.example.multitenant.context.TenantContext;
import org.springframework.stereotype.Component;

import javax.sql.DataSource;
import java.sql.Connection;
import java.sql.SQLException;
import java.sql.Statement;

@Component
public class RlsConnectionCustomizer {

    public static void applyTenantSetting(Connection connection) throws SQLException {
        String tenantId = TenantContext.getCurrentTenant();
        if (tenantId != null && !tenantId.isBlank()) {
            try (Statement stmt = connection.createStatement()) {
                // Sets session variable checked by PostgreSQL RLS policy
                stmt.execute("SET LOCAL app.current_tenant = '" + tenantId + "';");
            }
        }
    }
}
```

### Example 2: Model 2 - Schema-per-Tenant via Hibernate Connection Provider

```java
package com.example.multitenant.schema;

import com.example.multitenant.context.TenantContext;
import org.hibernate.context.spi.CurrentTenantIdentifierResolver;
import org.hibernate.engine.jdbc.connections.spi.MultiTenantConnectionProvider;
import org.springframework.stereotype.Component;

import javax.sql.DataSource;
import java.sql.Connection;
import java.sql.SQLException;

@Component
public class SchemaTenantIdentifierResolver implements CurrentTenantIdentifierResolver<String> {

    private static final String DEFAULT_TENANT = "public";

    @Override
    public String resolveCurrentTenantIdentifier() {
        String tenantId = TenantContext.getCurrentTenant();
        return (tenantId != null && !tenantId.isBlank()) ? tenantId : DEFAULT_TENANT;
    }

    @Override
    public boolean validateExistingCurrentSessions() {
        return true;
    }
}

@Component
public class SchemaConnectionProvider implements MultiTenantConnectionProvider<String> {

    private final DataSource dataSource;

    public SchemaConnectionProvider(DataSource dataSource) {
        this.dataSource = dataSource;
    }

    @Override
    public Connection getConnection(String tenantIdentifier) throws SQLException {
        Connection connection = dataSource.getConnection();
        // Switch schema via PostgreSQL search_path
        connection.createStatement().execute("SET search_path TO " + tenantIdentifier + ", public;");
        return connection;
    }

    @Override
    public void releaseConnection(String tenantIdentifier, Connection connection) throws SQLException {
        connection.createStatement().execute("SET search_path TO public;");
        connection.close();
    }

    @Override public boolean supportsAggressiveRelease() { return false; }
    @Override public boolean isUnwrappableAs(Class<?> unwrapType) { return false; }
    @Override public <T> T unwrap(Class<T> unwrapType) { return null; }
    @Override public Connection getAnyConnection() throws SQLException { return dataSource.getConnection(); }
    @Override public void releaseAnyConnection(Connection connection) throws SQLException { connection.close(); }
}
```

### Example 3: Model 3 - Database-per-Tenant via Spring `AbstractRoutingDataSource`

```java
package com.example.multitenant.database;

import com.example.multitenant.context.TenantContext;
import org.springframework.jdbc.datasource.lookup.AbstractRoutingDataSource;

public class DynamicTenantRoutingDataSource extends AbstractRoutingDataSource {

    @Override
    protected Object determineCurrentLookupKey() {
        // Returns the tenant key; Spring automatically routes to the registered DataSource
        return TenantContext.getCurrentTenant();
    }
}
```

Configuration wiring:
```java
@Configuration
public class MultiDbConfig {

    @Bean
    public DataSource dataSource(Map<String, DataSource> tenantDataSources) {
        DynamicTenantRoutingDataSource routingDs = new DynamicTenantRoutingDataSource();
        
        Map<Object, Object> targetDataSources = new HashMap<>(tenantDataSources);
        routingDs.setTargetDataSources(targetDataSources);
        routingDs.setDefaultTargetDataSource(tenantDataSources.get("default_db"));
        routingDs.afterPropertiesSet();

        return routingDs;
    }
}
```

### Example 4: Anti-Pattern (Leaking TenantContext into Thread Pool) vs Safe Cleanup

#### The Anti-Pattern: Uncleaned ThreadLocal
```java
// BAD: Filter sets TenantContext on worker thread, but never cleans it up!
@Component
public class BadTenantFilter implements Filter {
    @Override
    public void doFilter(ServletRequest req, ServletResponse res, FilterChain chain) throws IOException, ServletException {
        String tenant = ((HttpServletRequest) req).getHeader("X-Tenant-ID");
        TenantContext.setCurrentTenant(tenant);

        chain.doFilter(req, res);
        // CRITICAL BUG: No finally block! 
        // If an exception occurs, or when the request finishes, the Tomcat worker thread
        // retains "tenant" in ThreadLocal! When the thread is reused for another customer,
        // CATASTROPHIC DATA LEAKAGE OCCURS!
    }
}
```

#### Production Fix: Guaranteed `try-finally` Teardown
```java
// GOOD: Guaranteed cleanup in finally block
@Component
public class CleanTenantFilter implements Filter {
    @Override
    public void doFilter(ServletRequest req, ServletResponse res, FilterChain chain) throws IOException, ServletException {
        String tenant = ((HttpServletRequest) req).getHeader("X-Tenant-ID");
        try {
            TenantContext.setCurrentTenant(tenant);
            chain.doFilter(req, res);
        } finally {
            // ALWAYS executed even if an unhandled exception or error occurs
            TenantContext.clear();
        }
    }
}
```

### Example 5: Automated Tenant Onboarding with Flyway Schema Migrations

```java
package com.example.multitenant.onboarding;

import org.flywaydb.core.Flyway;
import org.springframework.stereotype.Service;

import javax.sql.DataSource;
import java.sql.Connection;
import java.sql.Statement;

@Service
public class TenantProvisioningService {

    private final DataSource dataSource;

    public TenantProvisioningService(DataSource dataSource) {
        this.dataSource = dataSource;
    }

    public void provisionNewTenant(String tenantSchemaName) {
        // Step 1: Create isolated PostgreSQL schema
        try (Connection conn = dataSource.getConnection();
             Statement stmt = conn.createStatement()) {
            stmt.execute("CREATE SCHEMA IF NOT EXISTS " + tenantSchemaName);
        } catch (Exception ex) {
            throw new RuntimeException("Failed to create tenant schema: " + tenantSchemaName, ex);
        }

        // Step 2: Execute Flyway migrations exclusively within the new schema
        Flyway flyway = Flyway.configure()
                .dataSource(dataSource)
                .schemas(tenantSchemaName)
                .locations("classpath:db/migration/tenant")
                .load();

        flyway.migrate();
    }
}
```

---

## 6. Best Practices

1. **Always Use `try-finally` to Clear `TenantContext`**: Prevent thread-pool contamination by removing tenant IDs in an interceptor `afterCompletion()` or servlet filter `finally` block.
2. **Favor PostgreSQL Row-Level Security (RLS) for Shared-Table Models**: Eliminates the risk of developers forgetting `WHERE tenant_id = ?` in custom JPQL or native SQL queries.
3. **Enforce Tenant Validation at the Gateway / Filter**: Never trust raw `X-Tenant-ID` headers without validating cryptographic JWT signatures or checking an active tenant cache.
4. **Automate Schema Migrations for Schema-per-Tenant**: Write automated migration loops running Flyway/Liquibase across all tenant schemas during deployment pipelines.
5. **Set Maximum Connection Limits for Database-per-Tenant**: If you have 500 tenants, you cannot run 500 Hikari pools with 10 connections each (5,000 connections will overwhelm the database server). Use shared pools or connection pool proxies (PgBouncer).
6. **Include `tenant_id` in Distributed Tracing Logs**: Inject the tenant ID into the SLF4J MDC (`MDC.put("tenantId", tenantId)`) so all production logs are searchable by tenant.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **ThreadLocal Pollution in Thread Pools** | Reused Tomcat worker threads retain previous user's tenant identity, causing cross-tenant data leakage. | Always execute `TenantContext.clear()` in a `finally` block or `HandlerInterceptor.afterCompletion()`. |
| **Connection Starvation in Database-per-Tenant** | Creating a separate connection pool per tenant quickly exhausts maximum database connection limits. | Use schema-per-tenant, or use dynamic connection proxies like AWS RDS Proxy or PgBouncer. |
| **Relying on Application-Level WHERE Clauses** | A single missing `WHERE tenant_id = :tenantId` in a native SQL query leaks all tenant records. | Enforce database-level PostgreSQL Row-Level Security (RLS) or Hibernate `@TenantId`. |
| **Trusting Client-Supplied Tenant Headers Blindly** | Attackers forge `X-Tenant-ID: victim_corp` in HTTP headers to access another organization's data. | Extract tenant ID strictly from cryptographically signed OAuth2/OIDC JWT tokens. |
| **Running Migrations Sequentially on 1,000 Schemas** | Sequential Flyway migrations take 4 hours during deployment, prolonging downtime windows. | Run migrations concurrently across schemas using Java parallel streams or Kubernetes batch jobs. |
| **Ignoring Asynchronous Background Threads** | `@Async` or `@Scheduled` tasks execute on new threads where `TenantContext` is null, throwing NPEs. | Use `TaskDecorator` to propagate tenant context across thread boundaries. |

---

## 8. Performance & Security Notes

### Performance
- **Connection Pool Overhead**: In the Database-per-Tenant model, idle pools consume database RAM and thread handles. Prefer Schema-per-Tenant with connection pool sharing via `search_path`.
- **Index Optimization**: In the Discriminator Column model, always make `tenant_id` the **leading column** in compound composite indexes: `CREATE INDEX idx_orders_tenant_cust ON orders (tenant_id, customer_id)`.

### Security
- **Defense in Depth**: Combine application-level token verification with database engine Row-Level Security (RLS). Even if application code has a severe SQL injection vulnerability, PostgreSQL RLS prevents returning rows belonging to another tenant.

---

## 9. Comparison / When to Use What

| Architectural Dimension | Model 1: Shared Table (Discriminator) | Model 2: Schema-per-Tenant | Model 3: Database-per-Tenant |
|---|---|---|---|
| **Data Isolation** | Lowest (Logical via RLS / SQL) | High (Separate DB Schemas) | Maximum (Physical DB Isolation)|
| **Infrastructure Cost** | Lowest (Highest density) | Low-to-Medium | High (Requires many DB instances)|
| **Migration Complexity**| Simplest (1 single table migration) | Complex (Run on N schemas)| Highest (Run on N databases) |
| **Max Scale** | 100,000+ Tenants | 1,000–5,000 Tenants (Schema limits)| Unlimited (Sharded across DBs) |
| **Regulatory Compliance**| Harder for HIPAA/banking | Standard for B2B Enterprise| Mandatory for government/banks |
| **Tenant Backup/Restore**| Complex (Requires row-level dump) | Simple (`pg_dump -n schema`)| Trivial (Full DB snapshot/restore)|

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the difference between Schema-per-Tenant and Discriminator Column multi-tenancy?</strong></summary>

- **Discriminator Column**: All tenants share the exact same database tables. Every table includes a `tenant_id` column. Queries filter by `WHERE tenant_id = ?` (or PostgreSQL RLS). It provides maximum infrastructure density and easiest schema migrations, but requires absolute discipline to avoid data leakage bugs.
- **Schema-per-Tenant**: Tenants share a single database server, but each tenant has their own isolated PostgreSQL schema (`tenant_a.orders`, `tenant_b.orders`). Switching tenants is done by changing the connection's `search_path`. It provides strong separation, easy individual tenant backup/restore, but schema migrations must run across all schemas on every deployment.
</details>

<details>
<summary><strong>2. How does Hibernate 6 native `@TenantId` annotation work?</strong></summary>

Hibernate 6 introduced native multi-tenancy support via the `@TenantId` annotation:
```java
@Entity
public class Order {
    @Id private Long id;

    @TenantId
    @Column(name = "tenant_id", nullable = false, updatable = false)
    private String tenantId;
}
```
When configured with a `CurrentTenantIdentifierResolver`:
- Hibernate automatically populates `tenantId` during `persist()` / `save()`.
- Hibernate automatically appends `WHERE tenant_id = ?` to every generated JPQL, Criteria, and HQL query, eliminating manual filtering boilerplate.
</details>

<details>
<summary><strong>3. How does PostgreSQL Row-Level Security (RLS) enforce multi-tenancy at the database engine level?</strong></summary>

PostgreSQL RLS intercepts all incoming SQL queries at the database engine parser level.
When a table has RLS enabled:
`ALTER TABLE orders ENABLE ROW LEVEL SECURITY;`
And a policy is declared:
`CREATE POLICY tenant_isolation ON orders USING (tenant_id = current_setting('app.current_tenant'));`
When a query `SELECT * FROM orders;` arrives:
- The database engine automatically rewrites the query internally to:
  `SELECT * FROM orders WHERE tenant_id = current_setting('app.current_tenant');`
Even if a compromised or buggy Spring Boot service runs `SELECT * FROM orders` without a WHERE clause, the database physically refuses to return any rows outside the active session tenant.
</details>

<details>
<summary><strong>4. How do you propagate `TenantContext` across asynchronous `@Async` and Virtual Threads?</strong></summary>

By default, Java `ThreadLocal` variables are not inherited by new worker threads spawned by `@Async` executors.
**Solution: Spring `TaskDecorator`**:
```java
public class TenantAwareTaskDecorator implements TaskDecorator {
    @Override
    public Runnable decorate(Runnable runnable) {
        String tenantId = TenantContext.getCurrentTenant();
        return () -> {
            try {
                TenantContext.setCurrentTenant(tenantId);
                runnable.run();
            } finally {
                TenantContext.clear();
            }
        };
    }
}
```
Register this decorator on the `ThreadPoolTaskExecutor`.
</details>

<details>
<summary><strong>5. What is the "Noisy Neighbor" problem in multi-tenant systems and how do you mitigate it?</strong></summary>

The Noisy Neighbor problem occurs when one heavy tenant executes massive batch jobs, consuming 95% of database CPU, memory, or connection pool bandwidth, causing degraded performance or outages for all other tenants sharing that database.
**Mitigation**:
- Rate limiting per tenant (Token Bucket algorithm via Redis).
- Connection pool quotas or dedicated pools for premium VIP tenants.
- Migrating high-volume noisy tenants to dedicated databases (Hybrid Multi-Tenancy model).
</details>

<details>
<summary><strong>6. Why is `TenantContext.clear()` critical when using connection pooling and thread pooling?</strong></summary>

Modern web servers (Tomcat, Jetty) maintain a pool of reusable worker threads.
If a worker thread handles a request for `Tenant A` and the `ThreadLocal` is not cleared:
- The thread is returned to the pool with `Tenant A` still bound in memory.
- When an unauthenticated or public request arrives on that same thread, it executes under `Tenant A`'s identity!
- Calling `clear()` in an interceptor's `finally` block guarantees complete memory sanitization.
</details>

<details>
<summary><strong>7. How do you implement tenant-specific branding or feature flags in a multi-tenant architecture?</strong></summary>

- Store tenant configuration metadata in a global `tenants` table (or cached in Redis).
- During request resolution, load the `TenantConfig` object containing customized themes, logo URLs, and enabled feature flag sets.
- Bind the configuration to the request scope so web controllers and services can tailor behavior dynamically.
</details>

<details>
<summary><strong>8. What is the limit of schemas in PostgreSQL for the Schema-per-Tenant model?</strong></summary>

While PostgreSQL does not enforce an arbitrary hard limit, exceeding 2,000–5,000 schemas on a single database causes severe degradation:
- Database catalogs (`pg_class`, `pg_attribute`) become bloated, slowing down query planning.
- Memory consumption increases as internal metadata caches swell.
- Automated migrations (Flyway) take hours to execute across thousands of schemas.
For platforms exceeding 5,000 tenants, the Discriminator Column model or Database Sharding is required.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create a Tenant Context Holder with Try-With-Resources AutoCloseable</strong></summary>

**Objective**: Implement an `AutoCloseable` wrapper for `TenantContext` to make scoping in tests and background jobs clean and leak-proof.

**Solution**:
```java
public class TenantScope implements AutoCloseable {

    public TenantScope(String tenantId) {
        TenantContext.setCurrentTenant(tenantId);
    }

    @Override
    public void close() {
        TenantContext.clear();
    }
}

// Usage:
try (var scope = new TenantScope("tenant_alpha")) {
    orderService.processPendingOrders();
} // Automatically clears TenantContext upon block exit!
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Extract Tenant from Signed JWT Claims</strong></summary>

**Objective**: Write a Spring Security authentication converter that extracts the `tenant_id` claim from a validated JWT and sets it in `TenantContext`.

**Solution**:
```java
@Component
public class JwtTenantExtractionFilter extends OncePerRequestFilter {

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain chain) 
            throws ServletException, IOException {
        
        Authentication auth = SecurityContextHolder.getContext().getAuthentication();
        if (auth instanceof JwtAuthenticationToken jwtAuth) {
            String tenantId = jwtAuth.getToken().getClaimAsString("tenant_id");
            if (tenantId != null) {
                TenantContext.setCurrentTenant(tenantId);
            }
        }
        try {
            chain.doFilter(request, response);
        } finally {
            TenantContext.clear();
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Configure Hibernate 6 `@TenantId` on JPA Entities</strong></summary>

**Objective**: Create a `Customer` entity that automatically populates and filters by tenant using Hibernate 6.

**Solution**:
```java
@Entity
@Table(name = "customers")
public class Customer {

    @Id @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    private String name;

    @TenantId
    @Column(name = "tenant_id", nullable = false, updatable = false)
    private String tenantId;

    protected Customer() {}
    public Customer(String name) { this.name = name; }

    public UUID getId() { return id; }
    public String getName() { return name; }
    public String getTenantId() { return tenantId; }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Build an Async TaskDecorator for Tenant Propagation</strong></summary>

**Objective**: Configure an `AsyncConfigurer` ensuring all `@Async` methods receive the caller's tenant context.

**Solution**:
```java
@Configuration
@EnableAsync
public class AsyncMultiTenantConfig implements AsyncConfigurer {

    @Override
    public Executor getAsyncExecutor() {
        ThreadPoolTaskExecutor executor = new ThreadPoolTaskExecutor();
        executor.setCorePoolSize(10);
        executor.setMaxPoolSize(50);
        executor.setTaskDecorator(runnable -> {
            String tenantId = TenantContext.getCurrentTenant();
            return () -> {
                try {
                    TenantContext.setCurrentTenant(tenantId);
                    runnable.run();
                } finally {
                    TenantContext.clear();
                }
            };
        });
        executor.initialize();
        return executor;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Spring `AbstractRoutingDataSource` with Hikari Pools</strong></summary>

**Objective**: Implement a dynamic multi-tenant data source that lazily initializes a HikariCP pool for a new tenant on their first request.

**Solution**:
```java
public class LazyTenantRoutingDataSource extends AbstractRoutingDataSource {

    private final Map<Object, Object> targetDataSources = new ConcurrentHashMap<>();
    private final DataSource defaultDataSource;

    public LazyTenantRoutingDataSource(DataSource defaultDataSource) {
        this.defaultDataSource = defaultDataSource;
        setDefaultTargetDataSource(defaultDataSource);
    }

    @Override
    protected Object determineCurrentLookupKey() {
        return TenantContext.getCurrentTenant();
    }

    @Override
    protected DataSource determineTargetDataSource() {
        String tenant = (String) determineCurrentLookupKey();
        if (tenant == null) return defaultDataSource;

        return (DataSource) targetDataSources.computeIfAbsent(tenant, this::createTenantPool);
    }

    private DataSource createTenantPool(String tenantId) {
        HikariConfig config = new HikariConfig();
        config.setJdbcUrl("jdbc:postgresql://localhost:5432/tenant_" + tenantId);
        config.setUsername("tenant_user");
        config.setPassword("secret");
        config.setMaximumPoolSize(5);
        return new HikariDataSource(config);
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Integration Test Verifying Cross-Tenant Isolation</strong></summary>

**Objective**: Write an integration test asserting that Tenant A cannot query or read records created by Tenant B.

**Solution**:
```java
@SpringBootTest
class MultiTenantDataIsolationTest {

    @Autowired private CustomerRepository customerRepository;

    @Test
    void assertTenantIsolationIsStrictlyEnforced() {
        // Create customer under Tenant A
        try (var scopeA = new TenantScope("tenant_alpha")) {
            customerRepository.save(new Customer("Alice Alpha"));
            assertEquals(1, customerRepository.count());
        }

        // Verify Tenant B sees 0 customers
        try (var scopeB = new TenantScope("tenant_beta")) {
            assertEquals(0, customerRepository.count());
            customerRepository.save(new Customer("Bob Beta"));
            assertEquals(1, customerRepository.count());
        }

        // Verify Tenant A still sees ONLY Alice
        try (var scopeA = new TenantScope("tenant_alpha")) {
            assertEquals(1, customerRepository.count());
            assertEquals("Alice Alpha", customerRepository.findAll().get(0).getName());
        }
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| MULTI-TENANCY IN SPRING BOOT CHEAT SHEET                                                           |
+----------------------------------------------------------------------------------------------------+
| Model               | Spring Implementation Strategy            | Trade-offs                       |
+---------------------+-------------------------------------------+----------------------------------+
| Discriminator Col   | Hibernate 6 @TenantId, PostgreSQL RLS     | Lowest cost; easiest schema      |
|                     | leading column on composite DB indexes    | migrations; requires strict RLS  |
+---------------------+-------------------------------------------+----------------------------------+
| Schema-per-Tenant   | MultiTenancyConnectionProvider switching  | High isolation; individual schema|
|                     | connection search_path (PostgreSQL)       | backups; complex multi-migrations|
+---------------------+-------------------------------------------+----------------------------------+
| Database-per-Tenant | Spring AbstractRoutingDataSource with     | Maximum isolation; compliance;   |
|                     | dynamic Hikari connection pools           | high cost; pool limits           |
+---------------------+-------------------------------------------+----------------------------------+
| Context Management  | ThreadLocal<String> TenantContext         | MUST clear() in finally block to |
|                     | HandlerInterceptor preHandle / afterCompl | avoid thread-pool pollution      |
+---------------------+-------------------------------------------+----------------------------------+
| Async Propagation   | TaskDecorator copying TenantContext to    | Essential for @Async and Virtual |
|                     | background worker threads                 | Threads                          |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Hibernate ORM 6.x Multi-Tenancy Guide](https://docs.jboss.org/hibernate/orm/current/userguide/html_single/Hibernate_User_Guide.html#multitenacy)
- [PostgreSQL Documentation: Row-Level Security (RLS)](https://www.postgresql.org/docs/current/ddl-rowsecurity.html)
- [Spring Framework: AbstractRoutingDataSource](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/jdbc/datasource/lookup/AbstractRoutingDataSource.html)
- [Baeldung: Multitenancy with Spring Data JPA](https://www.baeldung.com/multitenancy-with-spring-data-jpa)
- [AWS SaaS Factory: Multi-Tenant Architecture Patterns](https://aws.amazon.com/solutions/saas/)
