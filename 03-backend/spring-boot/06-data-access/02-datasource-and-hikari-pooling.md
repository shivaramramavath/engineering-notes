# DataSource & HikariCP: High-Performance Connection Pooling

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | `01-jdbc-template-and-jdbc-client.md`, Database architecture & OS threading |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | HikariCP 5.1.x / 6.x, PostgreSQL 16, MySQL 8.4 LTS |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Production-Ready HikariCP Configuration in YAML](#51-production-ready-hikaricp-configuration-in-yaml)
   - [5.2 Programmatic DataSource Customization & Metric Binding](#52-programmatic-datasource-customization--metric-binding)
   - [5.3 Detecting Connection Leaks in Test & Production](#53-detecting-connection-leaks-in-test--production)
   - [5.4 Anti-Pattern: The Massive Connection Pool Myth](#54-anti-pattern-the-massive-connection-pool-myth)
   - [5.5 Graceful Shutdown and Pool Eviction Handling](#55-graceful-shutdown-and-pool-eviction-handling)
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
Establishing a relational database connection is one of the most expensive operations in modern computing. A physical connection requires a multi-step TCP handshake, SSL/TLS cryptographic negotiation, database user authentication, authorization checks, and server-side process/thread allocation. Doing this per HTTP request causes severe latency spikes and quickly exhausts database resources. Conversely, poorly configured connection pools trigger thread starvation, database lock contention, memory leaks, and cascading outages when the database server saturates.

### Historical Evolution
Early Java applications relied on pools like Apache Commons DBCP and C3P0. While functional, they suffered from synchronization bottlenecks, complex locking hierarchies, and high memory allocations under concurrency. In 2013, Brett Wooldridge introduced **HikariCP**—a zero-overhead, hyper-optimized connection pool engineered using lock-free data structures (`ConcurrentBag`), custom micro-collections (`FastList`), and direct bytecode generation. HikariCP became the default connection pool in Spring Boot 2.0 and remains the industry gold standard.

### Real-World Relevance
At high traffic volumes, database performance is bounded by physical disk I/O and CPU cores, not pool size. Properly tuning HikariCP ensures that hundreds of application threads gracefully queue at the application tier rather than overwhelming the database server with thousands of concurrent context switches.

---

## 2. Core Concepts

### HikariCP
A lightweight, high-performance JDBC connection pool designed to operate close to bare-metal hardware speeds through lock-free concurrency and custom bytecode compilation.

### The PostgreSQL / Hardware Pool Sizing Formula
A proven heuristic established by the PostgreSQL development team:
$$\text{pool\_size} = (\text{core\_count} \times 2) + \text{effective\_spindle\_count}$$
Where $\text{core\_count}$ is the number of CPU cores on the *database server*, and $\text{effective\_spindle\_count}$ is the number of disks participating in RAID or concurrent NVMe channels. For example, a 4-core database server with an SSD can rarely handle more than $(4 \times 2) + 1 = 9$ or $10$ truly concurrent queries before disk/CPU thrashing degrades overall throughput.

### Connection Lifecycle States
1. **Idle**: Available in the pool, ready to be borrowed.
2. **In-Use (Active)**: Borrowed by an application thread executing SQL.
3. **Pending (Awaiting)**: Application threads waiting for an idle connection when the pool is saturated.
4. **Evicted / Dead**: Closed due to exceeding `maxLifetime`, network timeouts, or health check failures.

### Key HikariCP Tuning Parameters
- `maximumPoolSize`: Maximum number of actual physical connections in the pool.
- `minimumIdle`: Minimum number of idle connections HikariCP attempts to maintain.
- `connectionTimeout`: Maximum milliseconds an application thread will wait for a connection before throwing a `SQLTransientConnectionException`.
- `idleTimeout`: Maximum milliseconds a connection may sit idle before being eligible for retirement.
- `maxLifetime`: Maximum lifespan of a connection in the pool (prevents stale database memory/leak retention).
- `leakDetectionThreshold`: Milliseconds a connection can be borrowed before logging an alert that a connection leak was detected.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                               HikariCP Architecture                                |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Application Threads Borrowing Connection:                                         |
|      Thread 1                  Thread 2                   Thread 3                 |
|         |                         |                          |                     |
|         v                         v                          v                     |
|  +-------------------------------------------------------------------------------+ |
|  |                                ConcurrentBag                                  | |
|  |  1. ThreadLocal Cache: Check if current thread previously borrowed connection | |
|  |  2. Shared Queue (Lock-Free): Borrow from shared list if unassigned           | |
|  |  3. SynchronousQueue Handoff: Wait for returning connection                   | |
|  +-------------------------------------------------------------------------------+ |
|         |                                                                          |
|         v                                                                          |
|  FastList Proxy Wrapper                                                            |
|  - Tracks Statement instances without ArrayList index-checking overhead            |
|  - Intercepts close() to return connection to pool instead of physical socket close|
|                                                                                    |
|  Housekeeper Thread (runs every 30s)                                               |
|  - Enforces maxLifetime (retires connections gradually with random jitter)         |
|  - Detects leaked connections exceeding leakDetectionThreshold                     |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

1. **ThreadLocal Borrow Optimization**: HikariCP inspects the thread's `ThreadLocal` cache. If the thread previously used a connection that is now idle, it reacquires it with zero contention.
2. **Lock-Free Borrow via ConcurrentBag**: If no cached connection exists, it scans the shared list using compare-and-swap (CAS) primitives.
3. **Synchronous Handoff**: If all connections are active, the thread parks on a `SynchronousQueue` waiting for an active connection to call `close()`.
4. **Gradual Eviction via Jitter**: When connections reach `maxLifetime`, HikariCP retires them with negative variance (jitter) to prevent a "stampede" where all connections drop simultaneously.

---

## 4. Syntax & Usage

### Standard Spring Boot Configuration (`application.yaml`)
```yaml
spring:
  datasource:
    url: jdbc:postgresql://localhost:5432/orders_db
    username: app_user
    password: secret_password
    driver-class-name: org.postgresql.Driver
    hikari:
      pool-name: OrdersHikariPool
      maximum-pool-size: 10
      minimum-idle: 10
      connection-timeout: 30000
      idle-timeout: 600000
      max-lifetime: 1800000
      leak-detection-threshold: 2000
```

---

## 5. Practical Examples

### 5.1 Production-Ready HikariCP Configuration in YAML
```yaml
# =========================================================================
# PRODUCTION HIKARICP CONFIGURATION FOR HIGH-THROUGHPUT POSTGRESQL
# =========================================================================
spring:
  datasource:
    url: jdbc:postgresql://postgres.internal:5432/ecommerce_prod?ssl=true&sslmode=verify-full
    username: app_service
    password: ${DB_PASSWORD}
    type: com.zaxxer.hikari.HikariDataSource
    hikari:
      pool-name: ProdOrderServicePool
      # Sizing: 2 * 4 DB Cores + 2 = 10 connections
      maximum-pool-size: 10
      # Fixed-size pool: matching minimum-idle to maximum-pool-size prevents connection churn
      minimum-idle: 10
      # Wait up to 5 seconds before failing fast
      connection-timeout: 5000
      # Maximum connection lifespan: 30 minutes (must be < DB wait_timeout)
      max-lifetime: 1800000
      # Detect unclosed connections held for > 3 seconds
      leak-detection-threshold: 3000
      # PostgreSQL-specific driver optimizations
      data-source-properties:
        cachePrepStmts: true
        prepStmtCacheSize: 250
        prepStmtCacheSqlLimit: 2048
        useServerPrepStmts: true
        reWriteBatchedInserts: true
```

### 5.2 Programmatic DataSource Customization & Metric Binding
```java
package com.example.db.config;

import com.zaxxer.hikari.HikariConfig;
import com.zaxxer.hikari.HikariDataSource;
import io.micrometer.core.instrument.MeterRegistry;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Primary;

import javax.sql.DataSource;

@Configuration
public class DataSourceConfiguration {

    @Value("${spring.datasource.url}")
    private String jdbcUrl;

    @Value("${spring.datasource.username}")
    private String username;

    @Value("${spring.datasource.password}")
    private String password;

    @Bean
    @Primary
    public DataSource dataSource(MeterRegistry meterRegistry) {
        HikariConfig config = new HikariConfig();
        config.setPoolName("CustomMicrometerPool");
        config.setJdbcUrl(jdbcUrl);
        config.setUsername(username);
        config.setPassword(password);
        
        // Strict Pool Boundary Settings
        config.setMaximumPoolSize(12);
        config.setMinimumIdle(12);
        config.setConnectionTimeout(3000); // 3 seconds
        config.setMaxLifetime(1800000);     // 30 minutes
        config.setLeakDetectionThreshold(2500); // 2.5 seconds

        // Bind Micrometer metrics for Prometheus monitoring
        config.setMetricRegistry(meterRegistry);

        return new HikariDataSource(config);
    }
}
```

### 5.3 Detecting Connection Leaks in Test & Production
```java
package com.example.db.service;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import javax.sql.DataSource;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

@Service
public class LeakyReportingService {

    private static final Logger log = LoggerFactory.getLogger(LeakyReportingService.class);
    private final DataSource dataSource;

    public LeakyReportingService(DataSource dataSource) {
        this.dataSource = dataSource;
    }

    /**
     * ANTI-PATTERN DEMONSTRATION:
     * Manual connection acquired without try-with-resources.
     * When leakDetectionThreshold is set to 2000ms, HikariCP's housekeeper thread
     * outputs a detailed stack trace showing where this connection was borrowed!
     */
    public void leakyOperation() throws SQLException {
        Connection conn = dataSource.getConnection(); // LEAK: Never closed!
        PreparedStatement ps = conn.prepareStatement("SELECT pg_sleep(4)");
        ps.execute();
        log.warn("Query finished, but connection was intentionally not closed!");
    }

    /**
     * PRODUCTION FIX:
     * Idiomatic try-with-resources guarantees connection is returned to HikariCP pool.
     */
    public void safeOperation() {
        try (Connection conn = dataSource.getConnection();
             PreparedStatement ps = conn.prepareStatement("SELECT 1");
             ResultSet rs = ps.executeQuery()) {
            if (rs.next()) {
                log.info("Query successfully executed and connection cleanly closed.");
            }
        } catch (SQLException e) {
            log.error("Database error occurred", e);
        }
    }
}
```

### 5.4 Anti-Pattern: The Massive Connection Pool Myth
```java
// =========================================================================
// ANTI-PATTERN: Setting maximumPoolSize to 100+ on a typical database instance
// =========================================================================
# application-bad.yaml
spring:
  datasource:
    hikari:
      maximum-pool-size: 200 # DISASTER: Causes severe database context switching!
      connection-timeout: 60000

/*
 * WHY THIS FAILS UNDER LOAD:
 * 1. A PostgreSQL / MySQL server running on 4 or 8 cores can only execute 4-8 queries concurrently.
 * 2. 200 concurrent active connections force the OS kernel to constantly swap CPU registers,
 *    thrashing the L1/L2/L3 caches and saturating disk queues.
 * 3. Overall system throughput plummets, latency spikes from 10ms to 5000ms, and queries time out.
 */

// =========================================================================
// PRODUCTION FIX: Right-sized pool backed by application-tier queuing
// =========================================================================
# application-good.yaml
spring:
  datasource:
    hikari:
      # Formula: (DB_CORES * 2) + DISK_COUNT = (4 * 2) + 1 = 9 ~ 10
      maximum-pool-size: 10
      minimum-idle: 10
      connection-timeout: 2500 # Fail fast at the application tier!
```

### 5.5 Graceful Shutdown and Pool Eviction Handling
```java
package com.example.db.lifecycle;

import com.zaxxer.hikari.HikariDataSource;
import com.zaxxer.hikari.HikariPoolMXBean;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.SmartLifecycle;
import org.springframework.stereotype.Component;

import javax.sql.DataSource;

@Component
public class HikariPoolMonitor implements SmartLifecycle {

    private static final Logger log = LoggerFactory.getLogger(HikariPoolMonitor.class);
    private final DataSource dataSource;
    private boolean running = false;

    public HikariPoolMonitor(DataSource dataSource) {
        this.dataSource = dataSource;
    }

    @Override
    public void start() {
        this.running = true;
        log.info("HikariCP lifecycle monitor started.");
    }

    @Override
    public void stop() {
        if (dataSource instanceof HikariDataSource hikari) {
            HikariPoolMXBean poolMxBean = hikari.getHikariPoolMXBean();
            if (poolMxBean != null) {
                log.info("Shutting down HikariCP pool [{}]. Active: {}, Idle: {}, Awaiting: {}",
                    hikari.getPoolName(),
                    poolMxBean.getActiveConnections(),
                    poolMxBean.getIdleConnections(),
                    poolMxBean.getThreadsAwaitingConnection());
            }
            // Closes pool and cleanly interrupts threads
            hikari.close();
        }
        this.running = false;
    }

    @Override
    public boolean isRunning() {
        return this.running;
    }
}
```

---

## 6. Best Practices

1. **Keep `maximumPoolSize` Small**: Size according to database CPU core counts, not web application thread counts. Ten connections can easily support thousands of requests per second with fast queries.
2. **Set `minimumIdle` Equal to `maximumPoolSize`**: Avoid dynamic pool resizing. Establishing new connections on demand introduces latency spikes during traffic surges.
3. **Set `maxLifetime` Less than Database Timeouts**: Ensure `maxLifetime` is 2–5 minutes shorter than database server timeouts (e.g., MySQL `wait_timeout` or firewall TCP idle connection droppers).
4. **Always Enable `leakDetectionThreshold`**: Set between 2000ms and 5000ms to immediately identify unclosed connections in staging and production logs.
5. **Set an Explicit `poolName`**: Naming your pool (e.g., `OrderServiceHikariPool`) clarifies thread dumps, JMX metrics, and log outputs.
6. **Enable Driver-Level Prepared Statement Caching**: For PostgreSQL and MySQL, always enable `cachePrepStmts=true` and `prepStmtCacheSize=250` via `data-source-properties`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Oversizing the Pool (e.g., 100+ connections)** | Causes database CPU thrashing, severe lock contention, and cache pollution. | Calculate size using: $(\text{cores} \times 2) + \text{spindles}$. |
| **`maxLifetime` Greater than Firewall / DB Timeout** | Firewalls drop idle TCP sockets silently; application attempts to use dead sockets, causing connection reset errors. | Set `maxLifetime` at least 60 seconds shorter than the firewall/DB timeout. |
| **`connectionTimeout` Set Too High (e.g., 30s+)** | Application threads block for 30s before failing, causing thread pool exhaustion in web servers (Tomcat/Jetty). | Reduce `connectionTimeout` to 2000ms–5000ms to fail fast. |
| **Disabling Leak Detection in Production** | Leaked connections slowly drain the pool until the application hangs completely without diagnostic breadcrumbs. | Set `leak-detection-threshold: 3000` (3 seconds). |
| **Using `minimumIdle: 0` in Serverless / Cold Starts** | The first incoming request suffers massive latency penalty establishing fresh TCP/SSL connections. | Set `minimumIdle` equal to `maximumPoolSize`. |
| **Manual `Connection.close()` in Finally Without Null Check** | If `getConnection()` throws an exception, the finally block throws a secondary NPE, masking the root cause. | Always use modern `try-with-resources` syntax. |

---

## 8. Performance & Security Notes

### Performance
- **ConcurrentBag Micro-Optimization**: HikariCP avoids synchronized locks on borrow; threads attempt lock-free CAS operations, yielding sub-microsecond connection checkout times.
- **FastList Statement Tracking**: Standard `ArrayList` checks indexes on remove; HikariCP's `FastList` removes statements from head-to-tail without boundary checks, shaving CPU cycles.

### Security
- **Credential Rotation**: HikariCP supports programmatic credential rotation via `HikariConfig.setPassword()` without restarting the entire Spring Boot application.
- **SSL/TLS Validation**: Always enforce `sslmode=verify-full` in production JDBC URLs to prevent man-in-the-middle attacks on internal network traffic.

---

## 9. Comparison / When to Use What

| Feature | HikariCP | Apache Commons DBCP2 | Tomcat JDBC Pool | C3P0 |
| :--- | :--- | :--- | :--- | :--- |
| **Spring Boot Default** | **Yes (Since 2.0)** | No | Legacy | No |
| **Locking Mechanism** | Lock-Free (`ConcurrentBag`) | Synchronized locks | Reentrant locks | Heavy synchronization |
| **Micro-Allocations** | Near Zero (`FastList`) | Moderate | Moderate | High |
| **Bytecode Optimization** | Direct Javassist / Generated | Standard Java | Standard Java | Standard Java |
| **Leak Detection** | Built-in stack trace logger | Manual interceptor | Fairly basic | Unreliable |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is HikariCP significantly faster than older pools like DBCP or C3P0?</strong></summary>

HikariCP achieves its performance through several key engineering innovations:
1. **ConcurrentBag**: A lock-free collection utilizing `ThreadLocal` caching and compare-and-swap (CAS) primitives, eliminating lock contention.
2. **FastList**: A custom replacement for `ArrayList` that eliminates range checking and performs $O(1)$ removals for recently added statements.
3. **Bytecode Generation**: Pre-compiled connection wrappers that eliminate runtime reflection and synthetic bridge methods.
4. **Minimalist Codebase**: Shrunk down to a few thousand lines of highly cache-friendly CPU assembly instructions.
</details>

<details>
<summary><strong>2. What is the recommended formula for sizing a database connection pool?</strong></summary>

The formula popularized by the PostgreSQL team is:
$$\text{Pool Size} = (\text{DB CPU Cores} \times 2) + \text{Effective Spindles}$$
A database is bounded by its disk and CPU resources. Running more simultaneous queries than physical cores causes context switching overhead, which degrades total transactions per second.
</details>

<details>
<summary><strong>3. What does <code>leakDetectionThreshold</code> do in HikariCP?</strong></summary>

It configures a background schedule where, if a connection remains checked out from the pool longer than the specified threshold, HikariCP logs a warning containing a stack trace of the thread that originally checked out the connection. This pinpoints code paths that forgot to close connections.
</details>

<details>
<summary><strong>4. Why should <code>minimumIdle</code> generally match <code>maximumPoolSize</code> in production?</strong></summary>

Setting them to the same value creates a fixed-size pool. This eliminates the latency penalty of establishing new TCP/TLS connections during sudden traffic surges, prevents connection churn, and provides predictable resource consumption on both the application and database tiers.
</details>

<details>
<summary><strong>5. What is the purpose of <code>maxLifetime</code> and what rule must it follow?</strong></summary>

`maxLifetime` controls the maximum age of a connection in the pool before HikariCP closes and replaces it. It prevents resource leaks, clears stale database server state, and evicts broken sockets. It must always be set several minutes *shorter* than any database or firewall idle connection timeout.
</details>

<details>
<summary><strong>6. How does HikariCP avoid "connection storms" when multiple connections reach their <code>maxLifetime</code> simultaneously?</strong></summary>

HikariCP applies a negative random variance (jitter) to `maxLifetime`. Each connection's actual retirement time varies slightly, preventing all connections from expiring at the exact same moment.
</details>

<details>
<summary><strong>7. What exception is thrown when an application thread times out waiting for a connection?</strong></summary>

HikariCP throws `java.sql.SQLTransientConnectionException` (wrapped in Spring's `CannotGetJdbcConnectionException`) when the `connectionTimeout` duration elapses without a connection becoming available.
</details>

<details>
<summary><strong>8. How does HikariCP integrate with Micrometer in Spring Boot?</strong></summary>

HikariCP natively supports Micrometer via `HikariConfig.setMetricRegistry()`. Spring Boot auto-configuration automatically binds the active `MeterRegistry` to HikariCP, exposing metrics like `hikaricp.connections.active`, `hikaricp.connections.idle`, and `hikaricp.connections.acquire.seconds`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic HikariCP YAML Configuration</strong></summary>

**Task**: Configure HikariCP in `application.yaml` with a pool name of `OrderPool`, maximum pool size of 8, minimum idle of 8, and a connection timeout of 2000ms.

```yaml
spring:
  datasource:
    hikari:
      pool-name: OrderPool
      maximum-pool-size: 8
      minimum-idle: 8
      connection-timeout: 2000
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Validating Active Connections via JMX / MXBean</strong></summary>

**Task**: Inject `DataSource` and write a utility method that checks whether the pool currently has any active connections.

```java
public boolean hasActiveConnections(DataSource dataSource) {
    if (dataSource instanceof HikariDataSource hikari) {
        HikariPoolMXBean mxBean = hikari.getHikariPoolMXBean();
        return mxBean != null && mxBean.getActiveConnections() > 0;
    }
    return false;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Leak Detection Unit Test</strong></summary>

**Task**: Write an integration test configuring a low `leak-detection-threshold` (500ms), hold a connection for 800ms, and verify that a warning is logged.

```java
@Test
void testConnectionLeakDetection() throws Exception {
    HikariConfig config = new HikariConfig();
    config.setJdbcUrl("jdbc:h2:mem:leaktest");
    config.setLeakDetectionThreshold(500);
    
    try (HikariDataSource ds = new HikariDataSource(config)) {
        Connection conn = ds.getConnection();
        Thread.sleep(800); // Triggers leak detection housekeeper warning
        conn.close();
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom HikariCP Metrics Prometheus Exposer</strong></summary>

**Task**: Expose a custom health indicator checking if the number of awaiting threads in HikariCP exceeds a safe threshold of 5.

```java
@Component
public class HikariPoolHealthIndicator implements HealthIndicator {
    private final HikariDataSource dataSource;

    public HikariPoolHealthIndicator(DataSource dataSource) {
        this.dataSource = (HikariDataSource) dataSource;
    }

    @Override
    public Health health() {
        HikariPoolMXBean poolBean = dataSource.getHikariPoolMXBean();
        if (poolBean != null && poolBean.getThreadsAwaitingConnection() > 5) {
            return Health.down()
                .withDetail("awaitingThreads", poolBean.getThreadsAwaitingConnection())
                .build();
        }
        return Health.up().build();
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Dynamic Database Password Rotation Without Downtime</strong></summary>

**Task**: Implement a scheduled service that fetches renewed database credentials from Vault or AWS Secrets Manager and updates the live HikariCP pool.

```java
@Service
public class CredentialRotationService {

    private final HikariDataSource hikariDataSource;

    public CredentialRotationService(DataSource dataSource) {
        this.hikariDataSource = (HikariDataSource) dataSource;
    }

    public synchronized void updateCredentials(String newUsername, String newPassword) {
        // HikariCP allows updating username and password on the live pool config!
        hikariDataSource.setUsername(newUsername);
        hikariDataSource.setPassword(newPassword);
        
        // Evict idle connections so they re-connect with the new credentials
        HikariPoolMXBean poolBean = hikariDataSource.getHikariPoolMXBean();
        if (poolBean != null) {
            poolBean.softEvictConnections();
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Multi-Pool Router Based on Read/Write Separation</strong></summary>

**Task**: Configure two distinct HikariCP DataSources (one for Primary read-write, one for Read-Replica) using Spring's `AbstractRoutingDataSource`.

```java
public class ReadWriteRoutingDataSource extends AbstractRoutingDataSource {
    @Override
    protected Object determineCurrentLookupKey() {
        return TransactionSynchronizationManager.isCurrentTransactionReadOnly()
            ? "REPLICA" : "PRIMARY";
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Sizing Formula: Pool Size = (DB_CORES * 2) + SPINDLES (Keep it small, e.g. 10-20)     |
| Fixed Pool:     minimum-idle = maximum-pool-size (Prevents connection churn)          |
| Fail Fast:      connection-timeout: 2000-5000ms (Fail quickly rather than hang threads)|
| Lifespan:       max-lifetime < DB wait_timeout (e.g., 30 mins)                        |
| Leak Hunting:   leak-detection-threshold: 2000-3000ms (Logs stack trace of leak site) |
| Architecture:   ConcurrentBag (Lock-Free) + FastList (Zero-boundary check array)      |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [HikariCP Official Documentation & Configuration Guide](https://github.com/brettwooldridge/HikariCP)
- [About Pool Sizing (HikariCP Wiki)](https://github.com/brettwooldridge/HikariCP/wiki/About-Pool-Sizing)
- [PostgreSQL Connection Pooling Best Practices](https://wiki.postgresql.org/wiki/Number_Of_Database_Connections)
- [Spring Boot DataSource Auto-Configuration](https://docs.spring.io/spring-boot/docs/current/reference/html/data.html#data.sql.datasource)
