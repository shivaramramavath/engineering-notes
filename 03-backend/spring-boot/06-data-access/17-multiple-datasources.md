# Multiple DataSources: Configuration, Dynamic Routing & Read-Write Splitting

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | `02-datasource-and-hikari-pooling.md`, `03-spring-data-jpa-fundamentals.md`, `14-transactions.md` |
| **Estimated Reading Time** | 33 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Spring Boot 3.4.x, PostgreSQL 16 (Primary & Replica) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Static Multi-DataSource Configuration (Primary & Secondary)](#51-static-multi-datasource-configuration-primary--secondary)
   - [5.2 Dynamic Read-Write Splitting via AbstractRoutingDataSource](#52-dynamic-read-write-splitting-via-abstractroutingdatasource)
   - [5.3 Transaction Synchronization Routing Key Resolver](#53-transaction-synchronization-routing-key-resolver)
   - [5.4 Anti-Pattern: Assuming Distributed XA Across Multiple Standard Transaction Managers](#54-anti-pattern-assuming-distributed-xa-across-multiple-standard-transaction-managers)
   - [5.5 Multi-Tenant Database-per-Tenant Dynamic Connection Routing](#55-multi-tenant-database-per-tenant-dynamic-connection-routing)
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
Standard Spring Boot applications configure a single database via `spring.datasource.*`. However, enterprise architectures frequently require interacting with multiple physical databases:
1. **Read-Write CQRS Splitting**: Routing heavy read-only traffic (`@Transactional(readOnly = true)`) to read-replicas while directing mutations to the primary master database.
2. **Multi-Database Integration**: Communicating simultaneously with a modern operational PostgreSQL database and a legacy Oracle/SQL Server billing database.
3. **Multi-Tenancy**: Routing each tenant's queries to an isolated, dedicated database schema or cluster.

### Historical Evolution
By default, Spring Boot's `DataSourceAutoConfiguration` backs off as soon as a custom `DataSource` bean is defined. Configuring multiple databases historically required hundreds of lines of boilerplate XML or configuration code to set up duplicate `EntityManagerFactory`, `PlatformTransactionManager`, and package-scanned `JpaRepository` beans. Spring Framework 6.x and Spring Boot 3.x provide clean abstractions—specifically `AbstractRoutingDataSource` and modular configuration classes—to streamline multi-datasource architectures.

### Real-World Relevance
High-scale applications cannot scale writes and reads on a single database node forever. Offloading read queries to replicas reduces load on the primary writer database by 80%, improves query responsiveness, and isolates analytic/reporting queries from core customer-facing transactions.

---

## 2. Core Concepts

### Static Multi-DataSource Architecture
Defines two or more distinct, independent persistence stacks within the same Spring Boot application:
- Each database has its own `DataSource` (with its own HikariCP pool).
- Each database has its own `LocalContainerEntityManagerFactoryBean`.
- Each database has its own `PlatformTransactionManager`.
- Each database binds to repositories in distinct Java package namespaces via `@EnableJpaRepositories`.

### Dynamic Routing (`AbstractRoutingDataSource`)
A Spring `DataSource` implementation that acts as a proxy over a map of target DataSources. At the exact moment a database connection is borrowed, it invokes:
```java
protected abstract Object determineCurrentLookupKey();
```
Based on context (such as whether the current transaction is read-only or which tenant is logged in), it routes the connection checkout to the appropriate underlying pool.

### Primary DataSource Marker (`@Primary`)
When multiple beans of type `DataSource` or `PlatformTransactionManager` exist, Spring requires one to be decorated with `@Primary` to serve as the default injection candidate for framework utilities, Actuator health indicators, and Flyway migration runners.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                   AbstractRoutingDataSource Execution Mechanics                    |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Caller: @Transactional(readOnly = true)                                           |
|       |                                                                            |
|       v                                                                            |
|  TransactionInterceptor / JpaTransactionManager                                    |
|       |                                                                            |
|       |-- Sets TransactionSynchronizationManager.isCurrentTransactionReadOnly(true)|
|       |                                                                            |
|       v                                                                            |
|  AbstractRoutingDataSource.getConnection()                                         |
|       |                                                                            |
|       +---> Calls determineCurrentLookupKey()                                      |
|       |     (Inspects TransactionSynchronizationManager: Returns "REPLICA")        |
|       |                                                                            |
|       +---> Resolves "REPLICA" from targetDataSources map                          |
|       |                                                                            |
|       v                                                                            |
|  Replica HikariDataSource.getConnection()                                          |
|       |                                                                            |
|       v                                                                            |
|  Physical Socket Connection borrowed from PostgreSQL Read-Replica Pool             |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Property Structure (`application.yaml`)
```yaml
app:
  datasource:
    primary:
      url: jdbc:postgresql://primary-db:5432/orders
      username: app_user
      password: secret_password
    replica:
      url: jdbc:postgresql://replica-db:5432/orders
      username: app_user
      password: secret_password
```

---

## 5. Practical Examples

### 5.1 Static Multi-DataSource Configuration (Primary & Secondary)
```java
package com.example.db.config;

import com.zaxxer.hikari.HikariDataSource;
import org.springframework.boot.autoconfigure.jdbc.DataSourceProperties;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Primary;
import org.springframework.data.jpa.repository.config.EnableJpaRepositories;
import org.springframework.orm.jpa.JpaTransactionManager;
import org.springframework.orm.jpa.LocalContainerEntityManagerFactoryBean;
import org.springframework.boot.orm.jpa.EntityManagerFactoryBuilder;
import org.springframework.transaction.PlatformTransactionManager;

import javax.sql.DataSource;

@Configuration
@EnableJpaRepositories(
    basePackages = "com.example.primary.repository",
    entityManagerFactoryRef = "primaryEntityManagerFactory",
    transactionManagerRef = "primaryTransactionManager"
)
public class PrimaryDataSourceConfig {

    @Bean
    @Primary
    @ConfigurationProperties("app.datasource.primary")
    public DataSourceProperties primaryDataSourceProperties() {
        return new DataSourceProperties();
    }

    @Bean
    @Primary
    public DataSource primaryDataSource() {
        return primaryDataSourceProperties()
            .initializeDataSourceBuilder()
            .type(HikariDataSource.class)
            .build();
    }

    @Bean
    @Primary
    public LocalContainerEntityManagerFactoryBean primaryEntityManagerFactory(
        EntityManagerFactoryBuilder builder
    ) {
        return builder
            .dataSource(primaryDataSource())
            .packages("com.example.primary.domain")
            .persistenceUnit("primary")
            .build();
    }

    @Bean
    @Primary
    public PlatformTransactionManager primaryTransactionManager(
        LocalContainerEntityManagerFactoryBean primaryEntityManagerFactory
    ) {
        return new JpaTransactionManager(primaryEntityManagerFactory.getObject());
    }
}
```

```java
package com.example.db.config;

import com.zaxxer.hikari.HikariDataSource;
import org.springframework.boot.autoconfigure.jdbc.DataSourceProperties;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.data.jpa.repository.config.EnableJpaRepositories;
import org.springframework.orm.jpa.JpaTransactionManager;
import org.springframework.orm.jpa.LocalContainerEntityManagerFactoryBean;
import org.springframework.boot.orm.jpa.EntityManagerFactoryBuilder;
import org.springframework.transaction.PlatformTransactionManager;

import javax.sql.DataSource;

@Configuration
@EnableJpaRepositories(
    basePackages = "com.example.secondary.repository",
    entityManagerFactoryRef = "secondaryEntityManagerFactory",
    transactionManagerRef = "secondaryTransactionManager"
)
public class SecondaryDataSourceConfig {

    @Bean
    @ConfigurationProperties("app.datasource.secondary")
    public DataSourceProperties secondaryDataSourceProperties() {
        return new DataSourceProperties();
    }

    @Bean
    public DataSource secondaryDataSource() {
        return secondaryDataSourceProperties()
            .initializeDataSourceBuilder()
            .type(HikariDataSource.class)
            .build();
    }

    @Bean
    public LocalContainerEntityManagerFactoryBean secondaryEntityManagerFactory(
        EntityManagerFactoryBuilder builder
    ) {
        return builder
            .dataSource(secondaryDataSource())
            .packages("com.example.secondary.domain")
            .persistenceUnit("secondary")
            .build();
    }

    @Bean
    public PlatformTransactionManager secondaryTransactionManager(
        LocalContainerEntityManagerFactoryBean secondaryEntityManagerFactory
    ) {
        return new JpaTransactionManager(secondaryEntityManagerFactory.getObject());
    }
}
```

### 5.2 Dynamic Read-Write Splitting via AbstractRoutingDataSource
```java
package com.example.db.routing;

import org.springframework.jdbc.datasource.lookup.AbstractRoutingDataSource;
import org.springframework.transaction.support.TransactionSynchronizationManager;

public class TransactionRoutingDataSource extends AbstractRoutingDataSource {

    public enum DataSourceType {
        PRIMARY,
        REPLICA
    }

    @Override
    protected Object determineCurrentLookupKey() {
        // Inspects if Spring's active transaction is marked readOnly = true
        boolean isReadOnly = TransactionSynchronizationManager.isCurrentTransactionReadOnly();
        return isReadOnly ? DataSourceType.REPLICA : DataSourceType.PRIMARY;
    }
}
```

### 5.3 Transaction Synchronization Routing Key Resolver
```java
package com.example.db.config;

import com.example.db.routing.TransactionRoutingDataSource;
import com.example.db.routing.TransactionRoutingDataSource.DataSourceType;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Primary;
import org.springframework.jdbc.datasource.LazyConnectionDataSourceProxy;

import javax.sql.DataSource;
import java.util.HashMap;
import java.util.Map;

@Configuration
public class RoutingDataSourceConfig {

    @Bean
    @Primary
    public DataSource routingDataSource(
        @Qualifier("primaryDataSource") DataSource primary,
        @Qualifier("replicaDataSource") DataSource replica
    ) {
        TransactionRoutingDataSource routingDataSource = new TransactionRoutingDataSource();

        Map<Object, Object> dataSourceMap = new HashMap<>();
        dataSourceMap.put(DataSourceType.PRIMARY, primary);
        dataSourceMap.put(DataSourceType.REPLICA, replica);

        routingDataSource.setTargetDataSources(dataSourceMap);
        routingDataSource.setDefaultTargetDataSource(primary);
        routingDataSource.afterPropertiesSet();

        /**
         * CRITICAL: LazyConnectionDataSourceProxy defers borrowing physical connection
         * until the first SQL statement actually executes!
         * Without this, Spring borrows a connection BEFORE @Transactional is inspected,
         * making read/write routing completely inoperative!
         */
        return new LazyConnectionDataSourceProxy(routingDataSource);
    }
}
```

### 5.4 Anti-Pattern: Assuming Distributed XA Across Multiple Standard Transaction Managers
```java
// =========================================================================
// ANTI-PATTERN: Attempting Multi-Database Transactions with Standard Managers
// =========================================================================
@Service
public class DangerousCrossDbService {

    @Autowired private OrderRepository orderRepo;     // Uses PrimaryTransactionManager
    @Autowired private BillingRepository billingRepo; // Uses SecondaryTransactionManager

    // SEVERE INTEGRITY RISK:
    // @Transactional defaults to PrimaryTransactionManager!
    // If billingRepo fails, orderRepo DOES NOT ROLL BACK!
    // Using ChainedTransactionManager (deprecated) suffers from the "Last-Resource-Commit" flaw!
    @Transactional("primaryTransactionManager")
    public void createOrderAndBill(Order order, Bill bill) {
        orderRepo.save(order);
        billingRepo.save(bill); // Executes in separate, non-coordinated transaction!
    }
}

// =========================================================================
// PRODUCTION FIX: Event-Driven Sagas or Outbox Pattern
// =========================================================================
@Service
public class SafeCrossDbService {

    @Autowired private OrderRepository orderRepo;
    @Autowired private OutboxEventRepository outboxRepo;

    // Writes order and outbox event to PRIMARY database in a single local ACID transaction!
    @Transactional
    public void createOrderWithOutbox(Order order) {
        orderRepo.save(order);
        outboxRepo.save(new OutboxEvent("ORDER_CREATED", order.getId()));
        // Asynchronous worker reads outbox and reliably calls the billing system
    }
}
```

### 5.5 Multi-Tenant Database-per-Tenant Dynamic Connection Routing
```java
package com.example.db.multitenant;

import org.springframework.jdbc.datasource.lookup.AbstractRoutingDataSource;

public class TenantRoutingDataSource extends AbstractRoutingDataSource {

    private static final ThreadLocal<String> CURRENT_TENANT = new ThreadLocal<>();

    public static void setTenantId(String tenantId) {
        CURRENT_TENANT.set(tenantId);
    }

    public static void clear() {
        CURRENT_TENANT.remove();
    }

    @Override
    protected Object determineCurrentLookupKey() {
        String tenant = CURRENT_TENANT.get();
        return (tenant != null) ? tenant : "DEFAULT";
    }
}
```

---

## 6. Best Practices

1. **Always Wrap `AbstractRoutingDataSource` in `LazyConnectionDataSourceProxy`**: Without lazy connection acquisition, Spring acquires a physical database connection at transaction start *before* transaction attributes (`readOnly`) are evaluated, rendering read-replica routing useless.
2. **Always Mark Default Candidate with `@Primary`**: Clearly designate one `DataSource` and `PlatformTransactionManager` as `@Primary` to avoid injection ambiguity in Spring Boot auto-configurations and Actuator health endpoints.
3. **Segregate Repository Packages Cleanly**: Never mix repositories belonging to different databases in the same package directory; use distinct package trees (e.g. `com.example.db1.repo` and `com.example.db2.repo`).
4. **Avoid Distributed XA Transactions**: Two-Phase Commit (2PC) / XA protocols introduce severe locking latency, coordinator single-points-of-failure, and poor cloud elasticity. Prefer local ACID transactions paired with the Outbox Pattern and event-driven messaging.
5. **Clear Tenant ThreadLocal Variables in `finally` Blocks**: When using `ThreadLocal` for tenant routing, always invoke `clear()` in an HTTP filter's `finally` block to prevent tenant context leaks across pooled worker threads.
6. **Tune HikariCP Pools Independently**: Size the primary pool according to write volume, and size replica pools according to query concurrency and read replica hardware capacity.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Omitting `LazyConnectionDataSourceProxy` on Routing DataSource** | Spring borrows connection at transaction boundary before `isCurrentTransactionReadOnly` is set; all queries route to default primary. | Wrap routing datasource in `new LazyConnectionDataSourceProxy(routingDataSource)`. |
| **Assuming `@Transactional` Coordinates Multiple Databases** | Standard Spring transaction managers bind to one DataSource only; the secondary database commits or rolls back independently. | Use Outbox Pattern or Sagas for cross-database consistency. |
| **Repository Package Scanning Overlap** | Multiple `@EnableJpaRepositories` scanning the same package causes repository bean definition collisions. | Assign strict, disjoint `basePackages` strings to each `@EnableJpaRepositories`. |
| **Forgetting to Configure Flyway for Secondary DataSource** | Flyway auto-configures only for the primary DataSource, leaving secondary schemas un-migrated. | Configure a dedicated programmatic `Flyway` bean for the secondary database. |
| **Read-Your-Own-Writes Replication Lag** | Immediately reading a newly written entity from a replica can return 404 due to asynchronous replication delay. | Force queries immediately following a write to execute against the Primary DataSource. |
| **ThreadLocal Memory Leaks in Multi-Tenant Routing** | Failing to clean up thread locals causes subsequent unrelated requests on that thread to execute against the wrong tenant database. | Always execute `TenantContext.clear()` in a servlet filter `finally` block. |

---

## 8. Performance & Security Notes

### Performance
- **Primary Database CPU Relief**: Offloading analytical, search, and reporting queries to read replicas frees up transaction log buffers and CPU cores on the primary write database.
- **Connection Pool Isolation**: An overloaded reporting query on the replica pool will never starve the primary pool, ensuring transactional order checkout remains responsive.

### Security
- **Tenant Data Isolation**: Multi-tenant database-per-tenant routing provides physical boundary isolation, guaranteeing that an SQL injection or un-scoped query can never access another customer's database.
- **Credential Segregation**: Use separate database users with restricted permissions for the replica pool (e.g., granting read-only privileges `GRANT SELECT ON ALL TABLES`).

---

## 9. Comparison / When to Use What

| Pattern | Connection Acquisition | Complexity | Consistency Guarantee | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Static Multi-DataSource** | Independent pools per package | Moderate | Independent per DB | Distinct operational & legacy databases |
| **Dynamic Routing (`readOnly`)** | Via `LazyConnectionProxy` | High | Eventual (Replication lag) | High-volume read/write CQRS scaling |
| **Multi-Tenant Routing** | Via `ThreadLocal` lookup | High | Strong per tenant | Enterprise B2B SaaS (DB per tenant) |
| **Distributed XA (2PC)** | Two-Phase Commit coordinator | Extremely High | Immediate across DBs | Strict legacy banking (Avoid if possible) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is <code>LazyConnectionDataSourceProxy</code> required when implementing read-write splitting with <code>AbstractRoutingDataSource</code>?</strong></summary>

Spring's `JpaTransactionManager` normally borrows a JDBC connection as soon as a transaction starts, *before* executing the target method. At that point, the transaction attributes (such as `readOnly = true`) have not been evaluated or bound to the `TransactionSynchronizationManager`. `LazyConnectionDataSourceProxy` wraps the DataSource and defers borrowing a physical connection until the application actually executes its first SQL query, ensuring that the lookup key correctly identifies the transaction as read-only.
</details>

<details>
<summary><strong>2. How does <code>AbstractRoutingDataSource</code> work internally?</strong></summary>

`AbstractRoutingDataSource` extends `javax.sql.DataSource` and maintains a map of target DataSources (`targetDataSources`) keyed by arbitrary objects. When `getConnection()` is called, it invokes the template method `determineCurrentLookupKey()`, resolves the target DataSource from the map using the returned key, and delegates the physical connection checkout to that resolved DataSource.
</details>

<details>
<summary><strong>3. Why is <code>ChainedTransactionManager</code> deprecated in Spring?</strong></summary>

`ChainedTransactionManager` attempted to coordinate transactions across multiple databases using the "Last-Resource-Commit" heuristic without a true Two-Phase Commit (2PC) protocol. If Transaction 1 commits, and Transaction 2 fails during commit, Transaction 1 cannot be rolled back, leaving the system in an inconsistent state. Spring deprecated it, advising developers to use modern local transactions with the Outbox Pattern or formal XA managers if strictly necessary.
</details>

<details>
<summary><strong>4. How do you configure Spring Data repositories to use a specific DataSource?</strong></summary>

You use the `@EnableJpaRepositories` annotation, specifying `basePackages` to isolate the repository interfaces, and explicitly wire `entityManagerFactoryRef` and `transactionManagerRef` to point to the specific configuration beans for that database.
</details>

<details>
<summary><strong>5. What is the "read-your-own-writes" problem in primary-replica architectures and how do you mitigate it?</strong></summary>

Because replication from a primary database to a read-replica is asynchronous, there is a small replication lag (typically 5ms to 500ms). If a user creates an order and is immediately redirected to a page that reads from the replica, the query may return "Order Not Found".
**Mitigations**:
1. Route reads occurring within 2 seconds of a write strictly to the Primary database.
2. Store temporary write state in an in-memory cache (Redis).
3. Transmit the newly created entity state directly in the API response.
</details>

<details>
<summary><strong>6. Why is `@Primary` required when configuring multiple DataSources?</strong></summary>

Spring Boot auto-configures various services (Actuator database health indicators, Flyway, JdbcTemplate, Batch) that inject a `DataSource` by type. Without `@Primary` on one of the DataSources, Spring fails to start with a `NoUniqueBeanDefinitionException`.
</details>

<details>
<summary><strong>7. How do you handle database migrations with Flyway across multiple DataSources?</strong></summary>

Disable Flyway auto-configuration and declare two explicit `Flyway` beans in your configuration classes, each configured with its respective `DataSource` and specific migration folder location (e.g. `classpath:db/migration/primary` and `classpath:db/migration/secondary`).
</details>

<details>
<summary><strong>8. How does multi-tenant database routing handle connection pool sizing?</strong></summary>

If an application serves 500 tenants using database-per-tenant routing, creating a 10-connection HikariCP pool for every tenant would result in 5,000 idle database connections, exhausting database memory. Solutions include using dynamic lazy pools that evict idle tenant datasources after 10 minutes, or consolidating small tenants into a shared multi-tenant database using schema separation.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): AbstractRoutingDataSource Key Resolver</strong></summary>

**Task**: Implement an `AbstractRoutingDataSource` that returns `"WRITE"` when the current transaction is read-write, and `"READ"` when it is read-only.

```java
public class ReadWriteSplittingDataSource extends AbstractRoutingDataSource {
    @Override
    protected Object determineCurrentLookupKey() {
        return TransactionSynchronizationManager.isCurrentTransactionReadOnly() 
            ? "READ" : "WRITE";
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): TenantContext ThreadLocal Utility</strong></summary>

**Task**: Create a thread-safe `TenantContext` utility with static methods `setTenant(String id)`, `getTenant()`, and `clear()`.

```java
public final class TenantContext {
    private static final ThreadLocal<String> CURRENT = new ThreadLocal<>();

    public static void setTenant(String tenant) { CURRENT.set(tenant); }
    public static String getTenant() { return CURRENT.get(); }
    public static void clear() { CURRENT.remove(); }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Packaging and Repo Isolation Setup</strong></summary>

**Task**: Declare `@EnableJpaRepositories` for an analytics database scanning package `com.example.analytics.repo` pointing to `analyticsEntityManagerFactory` and `analyticsTxManager`.

```java
@Configuration
@EnableJpaRepositories(
    basePackages = "com.example.analytics.repo",
    entityManagerFactoryRef = "analyticsEntityManagerFactory",
    transactionManagerRef = "analyticsTxManager"
)
public class AnalyticsJpaConfig {}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Tenant Cleanup Servlet Filter</strong></summary>

**Task**: Write a Spring Web `OncePerRequestFilter` that extracts the `X-Tenant-ID` header, sets it in `TenantContext`, and guarantees cleanup in a `finally` block.

```java
@Component
public class TenantHeaderFilter extends OncePerRequestFilter {
    @Override
    protected void doFilterInternal(HttpServletRequest req, HttpServletResponse res, FilterChain chain) 
            throws ServletException, IOException {
        String tenantId = req.getHeader("X-Tenant-ID");
        if (tenantId != null) {
            TenantContext.setTenant(tenantId);
        }
        try {
            chain.doFilter(req, res);
        } finally {
            TenantContext.clear();
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Read-After-Write Consistency Context Bridge</strong></summary>

**Task**: Implement an aspect or filter that forces reads for 3 seconds following a write to execute on the Primary database to prevent replication lag anomalies.

```java
public class ReplicationLagAwareRoutingDataSource extends AbstractRoutingDataSource {
    private static final ThreadLocal<Long> LAST_WRITE_TIME = new ThreadLocal<>();

    public static void recordWrite() { LAST_WRITE_TIME.set(System.currentTimeMillis()); }

    @Override
    protected Object determineCurrentLookupKey() {
        Long lastWrite = LAST_WRITE_TIME.get();
        if (lastWrite != null && (System.currentTimeMillis() - lastWrite) < 3000) {
            return "PRIMARY"; // Force primary for 3 seconds after write
        }
        return TransactionSynchronizationManager.isCurrentTransactionReadOnly() ? "REPLICA" : "PRIMARY";
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Multi-Flyway Programmatic Migration Runner</strong></summary>

**Task**: Configure two programmatic Flyway runners that migrate the primary and secondary databases from distinct migration directories upon startup.

```java
@Configuration
public class MultiDatabaseFlywayConfig {

    @Bean
    public FlywayMigrationInitializer primaryFlywayInitializer(
        @Qualifier("primaryDataSource") DataSource primaryDs
    ) {
        Flyway flyway = Flyway.configure()
            .dataSource(primaryDs)
            .locations("classpath:db/migration/primary")
            .load();
        return new FlywayMigrationInitializer(flyway);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Static Multi-DB: Separate package trees, EntityManagerFactories, & TransactionManagers |
| Dynamic Routing: AbstractRoutingDataSource.determineCurrentLookupKey()                |
| Golden Rule:     ALWAYS wrap routing DataSource in LazyConnectionDataSourceProxy      |
| Default Marker:  Annotate exactly one DataSource and TxManager with @Primary          |
| Multi-Tenancy:   Use TenantContext ThreadLocal and ALWAYS clear() in filter finally   |
| Transactions:    Never assume @Transactional coordinates across multiple DataSources! |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Framework AbstractRoutingDataSource Documentation](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/jdbc/datasource/lookup/AbstractRoutingDataSource.html)
- [Baeldung: Spring Read-Write Database Routing](https://www.baeldung.com/spring-abstract-routing-data-source)
- [Vlad Mihalcea: Read-Write Splitting with Spring Boot and HikariCP](https://vladmihalcea.com/database-read-write-splitting-spring/)
- [Martin Fowler: Saga Pattern for Distributed Transactions](https://martinfowler.com/articles/patterns-of-distributed-systems/saga.html)
