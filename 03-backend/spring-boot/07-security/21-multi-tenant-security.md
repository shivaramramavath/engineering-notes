# 21. Multi-Tenant Security & Tenant Isolation in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, `07-JWT-resource-server.md`, `06-data-access/22-soft-delete-and-multi-tenancy-in-jpa.md` |
| **Estimated Reading Time** | 45 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, PostgreSQL 16 |

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
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
In modern SaaS architectures, a single application instance serves multiple distinct customer organizations (tenants). The most catastrophic security failure in multi-tenant SaaS is **Cross-Tenant Data Leakage**: Tenant A gaining access to Tenant B's confidential database records, billing information, or user lists.

Naive multi-tenancy designs that rely solely on developers remembering to add `WHERE tenant_id = ?` to every SQL query inevitably fail. As codebases grow, a single missing filter in a complex query, background job, or caching layer exposes all tenant data.

**Multi-Tenant Security** establishes a defense-in-depth perimeter combining **Cryptographic Identity Isolation** (verifying tenant claims inside JWTs), **Context Propagation** (ThreadLocal/ScopedValue tracking), and **Data Layer Enforcement** (Hibernate schema separation or PostgreSQL Row-Level Security).

### Historical Evolution
- **Early SaaS (Single-Tenant Silos)**: Required deploying separate application instances and database servers per customer. Highly isolated, but operationally expensive and unscalable.
- **Spring Framework 3.x - 5.x**: Popularized `AbstractRoutingDataSource` and Hibernate multi-tenancy filters, but context propagation often failed across asynchronous tasks.
- **Modern Spring Boot 3.x/4.x & Java 21+**: Integrates Java 21 `ScopedValue` for lightweight context propagation across Virtual Threads, native JWT claim-based tenant resolution, and database-level security via PostgreSQL Row-Level Security (RLS).

### Real-World Relevance
Flawless multi-tenant isolation is the foundational requirement for passing enterprise vendor security assessments, achieving SOC 2 Type II certification, and complying with global privacy laws (GDPR, HIPAA).

---

## 2. Core Concepts

### Architectural Contracts
- **Tenant Context**: An ambient security context holding the currently executing request's verified `tenant_id`.
- **Tenant Resolution Strategy**: The mechanism used to extract tenant identity from an incoming request:
  1. *Subdomain / Hostname* (`https://tenant-a.saas.com`)
  2. *Cryptographic JWT Claim* (`"tenant_id": "tenant-a"` inside verified token)
  3. *HTTP Request Header* (`X-Tenant-ID: tenant-a`)
  4. *URL Path Segment* (`/api/v1/tenants/tenant-a/...`)
- **Data Isolation Models**:
  - *Database-per-Tenant*: Each tenant has an independent database instance. Maximum isolation, highest cost.
  - *Schema-per-Tenant*: Tenants share a database instance but have isolated PostgreSQL/MySQL schemas. Good balance.
  - *Shared Database, Shared Schema (Discriminator Column)*: All tenants share tables containing a `tenant_id` column. Lowest cost, requires strict automated row-level filtering.

### The Mental Model
```
[ Incoming Request ] (Bearer eyJhbGciOiJSUzI1NiIs... "tenant_id": "ACME_CORP")
         │
         ▼
[ TenantContextFilter / JwtAuthenticationFilter ]
         │
         ├── 1. Validates JWT signature against IdP JWKS
         ├── 2. Extracts and validates 'tenant_id' claim
         ├── 3. Populates TenantContextHolder (ScopedValue / ThreadLocal)
         │
         ▼
[ Service Layer: orderService.getOrders() ]
         │
         ▼
[ Data Layer Isolation ]
         │
         ├─ Model A: Schema-per-Tenant ──► Swaps DataSource / Schema to "schema_acme_corp"
         │
         └─ Model B: Shared Schema + RLS ──► Injects "SET LOCAL app.current_tenant = 'ACME_CORP'"
                                             PostgreSQL Row-Level Security enforces isolation!
```

### Invariants
1. A client must **never** be allowed to specify or override their tenant identity via an untrusted request header or query parameter; tenant identity must be cryptographically extracted from the verified authentication token.
2. The `TenantContext` must be wiped cleanly upon request completion to prevent ThreadLocal cross-request contamination in thread pools.
3. Asynchronous tasks and scheduled background jobs must inherit or explicitly establish the target tenant's security context.

---

## 3. How It Works Internally

### Context Propagation across Java 21 Virtual Threads
In traditional thread-per-request models, `ThreadLocal` holds the tenant ID. However, Virtual Threads and reactive streams can break `ThreadLocal` inheritance if tasks are dispatched across asynchronous executors.
In Java 21+, `ScopedValue` or structured concurrency bridges provide clean context inheritance:
```
Virtual Thread (HTTP Request - Tenant A)
   │
   ├── Sets TenantContextHolder.setTenantId("tenant_a")
   │
   ├── Dispatches child virtual threads or executes JDBC query
   │      └─ Injects tenant_id into SQL connection session variable
   │
   └── Finally block (Filter):
          └─ TenantContextHolder.clear() // Guarantees zero context leakage!
```

---

## 4. Syntax & Usage

### 1. Tenant Context Holder
```java
package com.example.security.multitenant;

public final class TenantContextHolder {

    private static final ThreadLocal<String> CURRENT_TENANT = new ThreadLocal<>();

    private TenantContextHolder() {}

    public static void setTenantId(String tenantId) {
        CURRENT_TENANT.set(tenantId);
    }

    public static String getTenantId() {
        return CURRENT_TENANT.get();
    }

    public static void clear() {
        CURRENT_TENANT.remove();
    }
}
```

### 2. Tenant Resolution Servlet Filter
Extracting tenant identity securely from the verified `JwtAuthenticationToken`.

```java
package com.example.security.multitenant;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.security.oauth2.jwt.Jwt;
import org.springframework.security.oauth2.server.resource.authentication.JwtAuthenticationToken;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;

@Component
public class TenantResolutionFilter extends OncePerRequestFilter {

    private static final String TENANT_CLAIM = "tenant_id";

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {
        try {
            Authentication auth = SecurityContextHolder.getContext().getAuthentication();
            if (auth instanceof JwtAuthenticationToken jwtAuth) {
                Jwt jwt = jwtAuth.getToken();
                String tenantId = jwt.getClaimAsString(TENANT_CLAIM);
                if (tenantId != null && !tenantId.isBlank()) {
                    TenantContextHolder.setTenantId(tenantId);
                }
            }
            filterChain.doFilter(request, response);
        } finally {
            // CRITICAL: Prevent ThreadLocal leakage in thread pools!
            TenantContextHolder.clear();
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: PostgreSQL Row-Level Security (RLS) Tenant Enforcement
The gold standard for multi-tenant data security: enforcing tenant isolation at the database kernel level using PostgreSQL Row-Level Security.

```sql
-- Database Migration (V1__init_rls.sql)
CREATE TABLE documents (
    id BIGSERIAL PRIMARY KEY,
    tenant_id VARCHAR(64) NOT NULL,
    title VARCHAR(255) NOT NULL,
    content TEXT
);

-- Enable RLS on the table
ALTER TABLE documents ENABLE ROW LEVEL SECURITY;

-- Create policy ensuring queries only see rows matching the session's tenant setting
CREATE POLICY tenant_isolation_policy ON documents
    AS RESTRICTIVE
    USING (tenant_id = current_setting('app.current_tenant', true))
    WITH CHECK (tenant_id = current_setting('app.current_tenant', true));
```

```java
package com.example.security.multitenant;

import org.aspectj.lang.annotation.Aspect;
import org.aspectj.lang.annotation.Before;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Component;

@Aspect
@Component
public class PostgresRlsConnectionAspect {

    private final JdbcTemplate jdbcTemplate;

    public PostgresRlsConnectionAspect(JdbcTemplate jdbcTemplate) {
        this.jdbcTemplate = jdbcTemplate;
    }

    // Set PostgreSQL session variable before any repository operation executes
    @Before("execution(* com.example.repository..*(..))")
    public void setPostgresTenantVariable() {
        String tenantId = TenantContextHolder.getTenantId();
        if (tenantId != null) {
            jdbcTemplate.execute("SET LOCAL app.current_tenant = '" + tenantId + "'");
        }
    }
}
```

### Example 2: Schema-per-Tenant Routing DataSource
Dynamically switching PostgreSQL schemas based on `TenantContextHolder`.

```java
package com.example.security.multitenant;

import org.springframework.jdbc.datasource.lookup.AbstractRoutingDataSource;

public class TenantRoutingDataSource extends AbstractRoutingDataSource {

    @Override
    protected Object determineCurrentLookupKey() {
        return TenantContextHolder.getTenantId();
    }
}
```

### Example 3: Edge Case – Cross-Tenant Administrative Impersonation
Allowing authorized platform super-admins to inspect customer tenant data safely with strict audit logging.

```java
package com.example.service;

import com.example.security.multitenant.TenantContextHolder;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.stereotype.Service;

import java.util.function.Supplier;

@Service
public class TenantImpersonationService {

    private static final Logger log = LoggerFactory.getLogger(TenantImpersonationService.class);

    @PreAuthorize("hasRole('SUPER_ADMIN')")
    public <T> T executeInTenantContext(String targetTenantId, String adminUsername, Supplier<T> task) {
        String originalTenant = TenantContextHolder.getTenantId();
        log.warn("AUDIT ALERT: Super-Admin [{}] impersonating Tenant [{}]", adminUsername, targetTenantId);

        try {
            TenantContextHolder.setTenantId(targetTenantId);
            return task.get();
        } finally {
            if (originalTenant != null) {
                TenantContextHolder.setTenantId(originalTenant);
            } else {
                TenantContextHolder.clear();
            }
        }
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Trusting an unauthenticated `X-Tenant-ID` header supplied by the client without verifying it against the authenticated JWT.

```java
// CRITICAL VULNERABILITY: An attacker logged in as Tenant A can send "X-Tenant-ID: Tenant-B"
// and read Tenant B's confidential database!
String tenantId = request.getHeader("X-Tenant-ID");
TenantContextHolder.setTenantId(tenantId); // NEVER TRUST CLIENT-SUPPLIED TENANT HEADERS!
```

**Production Fix**:
Extract tenant identity exclusively from verified JWT claims issued by your trusted Identity Provider.

```java
// SECURE: Tenant ID is cryptographically signed and tamper-proof
Jwt jwt = ((JwtAuthenticationToken) auth).getToken();
String verifiedTenantId = jwt.getClaimAsString("tenant_id");
TenantContextHolder.setTenantId(verifiedTenantId);
```

### Example 5: High-Performance Non-Blocking WebFlux Reactive Tenant Context
Propagating tenant context in Project Reactor using Reactor `ContextView`.

```java
package com.example.security.reactive;

import org.springframework.stereotype.Component;
import org.springframework.web.server.ServerWebExchange;
import org.springframework.web.server.WebFilter;
import org.springframework.web.server.WebFilterChain;
import reactor.core.publisher.Mono;
import reactor.util.context.Context;

@Component
public class ReactiveTenantContextFilter implements WebFilter {

    public static final String TENANT_KEY = "TENANT_ID";

    @Override
    public Mono<Void> filter(ServerWebExchange exchange, WebFilterChain chain) {
        String tenantId = extractTenantFromHeaders(exchange); // Or JWT
        if (tenantId == null) {
            return chain.filter(exchange);
        }

        // Attach tenant ID to Reactor Context (immutable, non-blocking, thread-agnostic)
        return chain.filter(exchange)
            .contextWrite(Context.of(TENANT_KEY, tenantId));
    }

    private String extractTenantFromHeaders(ServerWebExchange exchange) {
        return exchange.getRequest().getHeaders().getFirst("X-Tenant-ID");
    }
}
```

---

## 6. Best Practices

1. **Extract Tenant Identity from Cryptographic Tokens**: Never trust raw HTTP request headers (`X-Tenant-ID`) for tenant isolation; always bind tenant identity to digitally signed JWT claims.
2. **Always Clear `ThreadLocal` in a `finally` Block**: Guarantee that `TenantContextHolder.clear()` executes on every request to prevent thread pool cross-contamination.
3. **Use Database-Level Row-Level Security (RLS)**: Enforce isolation in PostgreSQL RLS so even accidental omissions of `WHERE tenant_id = ?` in Java code cannot leak data.
4. **Isolate Caches by Tenant**: In Redis or local caches, always prefix cache keys with the tenant ID: `orders:{tenant_id}:{order_id}`.
5. **Separate Tenant Storage for Compliance**: High-value enterprise customers (e.g., healthcare/banking) often contractually mandate dedicated database instances (Database-per-Tenant).
6. **Audit Cross-Tenant Actions**: Log all super-admin impersonation events to an immutable audit trail.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Unverified `X-Tenant-ID` header | Attackers alter the header to access arbitrary customer data. | Extract tenant identity strictly from verified JWT claims. |
| Forgetting `ThreadLocal.remove()` | Subsequent requests on the same thread inherit the previous customer's tenant context. | Always invoke `clear()` in a servlet filter `finally` block. |
| Shared Redis cache without prefixes | Tenant A can read Tenant B's cached user profile by guessing the user ID key. | Prefix all cache keys: `tenant:{tenant_id}:users:{id}`. |
| Missing RLS policies on new tables | Developers create a new table without RLS, completely exposing it to all tenants. | Mandate automated CI schema checks ensuring all tables have RLS enabled. |
| Cross-tenant leakage in async tasks | `@Async` methods run on a different thread where `ThreadLocal` is null. | Use `TaskDecorator` to copy tenant context to asynchronous task threads. |
| Hardcoding single connection pools | A single tenant executing a heavy query pool-starves all other tenants. | Implement tenant-scoped connection quotas or rate limits. |

---

## 8. Performance & Security Notes

- **Connection Pool Overhead in Database-per-Tenant**: If you have 500 tenants and each has a 10-connection Hikari pool, the application consumes 5,000 database connections, crashing PostgreSQL. For hundreds of tenants, use **Shared Schema with Row-Level Security** or dynamic connection pooling.
- **PostgreSQL RLS Query Performance**: RLS adds a negligible sub-millisecond filter overhead provided composite indexes exist: `CREATE INDEX idx_tenant_id_id ON documents(tenant_id, id);`.
- **Virtual Threads Integration**: When using Java 21 Virtual Threads, creating thousands of short-lived virtual threads with `ScopedValue` provides near-zero memory overhead compared to heavy OS threads.

---

## 9. Comparison / When to Use What

| Multi-Tenancy Model | Data Isolation Level | Infrastructure Cost | Operational Complexity | Best Use Case |
|---|---|---|---|---|
| **Database-per-Tenant** | **Maximum (Physical)** | High | High (500+ DB instances) | Banking, Government, HIPAA |
| **Schema-per-Tenant** | High (Logical) | Moderate | Moderate (Schema migrations) | Mid-market SaaS |
| **Shared Schema + RLS** | High (DB Kernel Enforced) | **Lowest** | Low (Single schema) | High-volume B2B/B2C SaaS |
| **Shared Schema (Java WHERE)**| Low (Vulnerability Prone) | Lowest | High (Manual vigilance) | Never; replace with RLS |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between Database-per-Tenant, Schema-per-Tenant, and Shared Schema multi-tenancy?</summary>

**Answer:**
- **Database-per-Tenant**: Each customer has a completely separate physical database. Offers maximum isolation and independent backups, but has high infrastructure cost and connection pool overhead.
- **Schema-per-Tenant**: Customers share a single database server but have dedicated schemas (`tenant_a`, `tenant_b`). Offers strong isolation and moderate cost, but schema migrations must run across hundreds of schemas.
- **Shared Schema (Discriminator Column)**: All customers share the same tables, distinguished by a `tenant_id` column. Highly cost-effective and scalable, but requires database-level protections (like PostgreSQL Row-Level Security) to prevent catastrophic data leaks.
</details>

<details>
<summary>2. Why is relying on Java-level `WHERE tenant_id = ?` filters dangerous in enterprise applications?</summary>

**Answer:**
Relying on developers to manually write or remember tenant filters in every query is human-error prone. A single missing `WHERE` clause in a complex JPQL query, native SQL query, reporting tool, or database migration exposes all customer data. Enforcing isolation at the database layer via PostgreSQL Row-Level Security (RLS) guarantees that queries can never return cross-tenant data, regardless of bugs in application code.
</details>

<details>
<summary>3. Why must tenant resolution never trust unauthenticated HTTP headers like `X-Tenant-ID`?</summary>

**Answer:**
HTTP headers are fully controlled by the client. An attacker belonging to Tenant A can set `X-Tenant-ID: Tenant-B` to bypass application logic and read Tenant B's data. Tenant resolution must always be cryptographically bound to a verified identity token (such as a JWT signed by an authorized Identity Provider) containing a verified `tenant_id` claim.
</details>

<details>
<summary>4. How does PostgreSQL Row-Level Security (RLS) work?</summary>

**Answer:**
PostgreSQL RLS intercepts all SQL queries executed against a table and automatically appends a security policy condition (e.g., `USING (tenant_id = current_setting('app.current_tenant'))`). Even if a developer writes `SELECT * FROM orders;`, the PostgreSQL database engine automatically rewrites and evaluates the query as `SELECT * FROM orders WHERE tenant_id = 'acme_corp'`, making cross-tenant data leaks impossible.
</details>

<details>
<summary>5. How do you propagate Tenant Context across `@Async` thread boundaries?</summary>

**Answer:**
By default, `ThreadLocal` values do not propagate to new threads. You must configure a custom `TaskDecorator` on your `ThreadPoolTaskExecutor`:
```java
public class TenantTaskDecorator implements TaskDecorator {
    @Override
    public Runnable decorate(Runnable runnable) {
        String tenantId = TenantContextHolder.getTenantId();
        return () -> {
            try {
                TenantContextHolder.setTenantId(tenantId);
                runnable.run();
            } finally {
                TenantContextHolder.clear();
            }
        };
    }
}
```
</details>

<details>
<summary>6. What happens if a developer forgets to call `TenantContextHolder.clear()`?</summary>

**Answer:**
Servlet containers (Tomcat, Jetty) maintain a thread pool where worker threads are reused across thousands of HTTP requests. If `clear()` is omitted, a thread retains the previous user's tenant ID. When a subsequent request from a completely different tenant is assigned to that thread, it executes under the previous tenant's identity, causing severe cross-tenant data leakage. Always call `clear()` inside a `finally` block.
</details>

<details>
<summary>7. How do you prevent cross-tenant data leakage in Redis caching?</summary>

**Answer:**
Always namespace and prefix all Redis keys with the tenant identifier:
`cache:{tenant_id}:{entity_name}:{id}`.
Ensure your Spring `CacheManager` or custom key generator automatically prepends `TenantContextHolder.getTenantId()` to every generated cache key.
</details>

<details>
<summary>8. How do you handle multi-tenant authentication with multiple independent Identity Providers (IdPs)?</summary>

**Answer:**
Use `JwtIssuerAuthenticationManagerResolver`. The resolver inspects the incoming token's `iss` claim or request hostname, dynamically resolves the corresponding tenant's IdP (e.g., Tenant A's Okta vs Tenant B's Keycloak), verifies the signature against that specific IdP's JWKS, and extracts the tenant context.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic TenantContext Holder (Easy)</summary>

**Task**: Implement a thread-safe `TenantContext` utility with `setTenant()`, `getTenant()`, and `clear()` methods using `ThreadLocal`.

**Solution**:
```java
public final class TenantContext {
    private static final ThreadLocal<String> CONTEXT = new ThreadLocal<>();

    private TenantContext() {}

    public static void setTenant(String tenantId) { CONTEXT.set(tenantId); }
    public static String getTenant() { return CONTEXT.get(); }
    public static void clear() { CONTEXT.remove(); }
}
```
</details>

<details>
<summary>Exercise 2: Safe Tenant Cache Key Generator (Easy)</summary>

**Task**: Implement a Spring `KeyGenerator` that prepends the current tenant ID from `TenantContext` to any cache key.

**Solution**:
```java
@Component("tenantKeyGenerator")
public class TenantAwareKeyGenerator implements KeyGenerator {
    @Override
    public Object generate(Object target, Method method, Object... params) {
        String tenant = TenantContext.getTenant();
        return tenant + ":" + SimpleKeyGenerator.generateKey(params);
    }
}
```
</details>

<details>
<summary>Exercise 3: JWT Tenant Claim Resolution Filter (Medium)</summary>

**Task**: Write a servlet filter that extracts the `tenant_id` claim from an authenticated `JwtAuthenticationToken` and establishes the `TenantContext`, ensuring `clear()` is invoked in `finally`.

**Solution**:
```java
public class JwtTenantFilter extends OncePerRequestFilter {
    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {
        try {
            Authentication auth = SecurityContextHolder.getContext().getAuthentication();
            if (auth instanceof JwtAuthenticationToken jwtAuth) {
                String tenant = jwtAuth.getToken().getClaimAsString("tenant_id");
                if (tenant != null) {
                    TenantContext.setTenant(tenant);
                }
            }
            filterChain.doFilter(request, response);
        } finally {
            TenantContext.clear();
        }
    }
}
```
</details>

<details>
<summary>Exercise 4: PostgreSQL RLS Session Variable Interceptor (Medium)</summary>

**Task**: Write a Spring `@Aspect` that executes `SET LOCAL app.current_tenant = :tenantId` on every JDBC connection before executing repository methods.

**Solution**:
```java
@Aspect
@Component
public class RlsAspect {
    private final JdbcTemplate jdbcTemplate;

    public RlsAspect(JdbcTemplate jdbcTemplate) {
        this.jdbcTemplate = jdbcTemplate;
    }

    @Before("within(@org.springframework.stereotype.Repository *)")
    public void setRlsContext() {
        String tenant = TenantContext.getTenant();
        if (tenant != null) {
            jdbcTemplate.execute("SET LOCAL app.current_tenant = '" + tenant + "'");
        }
    }
}
```
</details>

<details>
<summary>Exercise 5: Multi-Tenant TaskDecorator for Async Executors (Hard)</summary>

**Task**: Implement a `TaskDecorator` that copies the tenant context from the caller thread to the worker thread for `@Async` method invocations.

**Solution**:
```java
public class TenantContextTaskDecorator implements TaskDecorator {
    @Override
    public Runnable decorate(Runnable runnable) {
        String tenantId = TenantContext.getTenant();
        return () -> {
            try {
                TenantContext.setTenant(tenantId);
                runnable.run();
            } finally {
                TenantContext.clear();
            }
        };
    }
}
```
</details>

<details>
<summary>Exercise 6: MockMvc Multi-Tenant Integration Test (Hard)</summary>

**Task**: Write an integration test using MockMvc and `jwt()` verifying that a user from Tenant A cannot see invoices belonging to Tenant B.

**Solution**:
```java
@SpringBootTest
@AutoConfigureMockMvc
class MultiTenantSecurityTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void userFromTenantACannotAccessTenantBInvoice() throws Exception {
        mockMvc.perform(get("/api/invoices/999") // Invoice 999 belongs to Tenant B
                .with(jwt().jwt(jwt -> jwt.claim("tenant_id", "TENANT_A"))))
            .andExpect(status().isNotFound()); // Or 403 Forbidden via RLS
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Never Trust Headers**: Extract tenant identity strictly from verified JWT claims.
- **Always Clean `ThreadLocal`**: Invoke `TenantContextHolder.clear()` inside a `finally` block to prevent thread pool contamination.
- **Enforce at the Database Layer**: Use PostgreSQL Row-Level Security (RLS) to guarantee queries cannot leak data across tenants.
- **Namespace Caches**: Prefix all Redis keys with the tenant ID: `cache:{tenant}:{key}`.
- **Propagate to Async**: Use `TaskDecorator` to bridge tenant context across asynchronous thread pools.

---

## 13. Further Reading
- [Spring Data JPA Multi-Tenancy Reference](https://docs.spring.io/spring-data/jpa/reference/jpa/multi-tenancy.html)
- [PostgreSQL Documentation: Row-Level Security Policies](https://www.postgresql.org/docs/current/ddl-rowsecurity.html)
- [OWASP Multi-Tenancy Security Guidelines](https://cheatsheetseries.owasp.org/cheatsheets/Multitenancy_Cheat_Sheet.html)
- [Hibernate ORM Multi-Tenancy User Guide](https://docs.jboss.org/hibernate/orm/6.5/userguide/html_single/Hibernate_User_Guide.html#multitenacy)
- [RFC 7519: JSON Web Token (JWT) Specifications](https://datatracker.ietf.org/doc/html/rfc7519)
