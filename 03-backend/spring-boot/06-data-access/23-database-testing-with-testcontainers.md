# Database Testing with Testcontainers: @ServiceConnection, Real PostgreSQL & Reusable Containers

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate / Advanced |
| **Prerequisites** | `02-datasource-and-hikari-pooling.md`, `16-flyway-and-liquibase-migrations.md`, Docker & JUnit 5 basics |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Testcontainers 1.20+, Spring Boot 3.4.x, PostgreSQL 16 Alpine, JUnit 5.11 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Modern Spring Boot 3.1+ @ServiceConnection Integration](#51-modern-spring-boot-31-serviceconnection-integration)
   - [5.2 Singleton Container Pattern for Fast Test Suites](#52-singleton-container-pattern-for-fast-test-suites)
   - [5.3 Testing Native JSONB & Custom Dialects Impossible in H2](#53-testing-native-jsonb--custom-dialects-impossible-in-h2)
   - [5.4 Anti-Pattern: Using In-Memory H2 for Production Testing](#54-anti-pattern-using-in-memory-h2-for-production-testing)
   - [5.5 Testing Concurrent Optimistic Locking with Testcontainers](#55-testing-concurrent-optimistic-locking-with-testcontainers)
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
For years, Java developers relied on in-memory databases like **H2** or **HSQLDB** for integration testing. While fast to boot, in-memory databases do not replicate real production database behaviors. They lack support for production-specific SQL dialects (PostgreSQL JSONB operators, CTEs, full-text search, stored procedures), exhibit different transaction locking semantics, handle sequence generators differently, and fail to validate real Flyway/Liquibase migration scripts. Code that passes 100% of H2 unit tests frequently crashes in production.

### Historical Evolution
The **Testcontainers** library revolutionized integration testing by providing lightweight, throwaway instances of real databases running inside Docker containers. In Spring Boot 3.1+, the team introduced **`@ServiceConnection`**, eliminating the need for verbose `@DynamicPropertySource` methods. Spring Boot automatically discovers running Testcontainers instances, extracts their dynamic JDBC URLs, ports, and credentials, and wires them directly into the Spring `DataSource` environment.

### Real-World Relevance
Testing against exact production database engines (e.g., PostgreSQL 16 on Docker) guarantees that SQL queries, migration scripts, constraint violations, and connection pooling behaviors work identically in CI/CD pipelines as they do in live production environments.

---

## 2. Core Concepts

### Testcontainers
A Java library that supports JUnit tests by providing disposable, containerized instances of databases, message brokers, and web servers via Docker.

### @ServiceConnection
A Spring Boot 3.1+ annotation placed on a `@Container` field. It automatically registers the container's dynamic connection properties into the Spring Boot `Environment`, eliminating boilerplate:
- Automatically binds `spring.datasource.url` with dynamic random port.
- Automatically binds `spring.datasource.username` and `password`.
- Automatically executes Flyway/Liquibase migrations against the container.

### Singleton Container Pattern
Spinning up a new Docker container per test class introduces substantial test execution overhead (typically 3–5 seconds per container). The Singleton Container pattern initializes a single shared container instance across the entire test suite, reducing suite runtimes by up to 90%.

### Container Reuse (`.withReuse(true)`)
A local development feature that keeps the Docker container running between multiple JUnit test executions in your IDE, eliminating container boot latency entirely during local development loops.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         @ServiceConnection Test Lifecycle                          |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  1. JUnit 5 Test Suite Starts                                                      |
|     - Testcontainers connects to Docker daemon via Docker socket                   |
|     - Pulls / Starts 'postgres:16-alpine' container with dynamic random port       |
|                                                                                    |
|  2. Spring Boot Test Context Bootstrapping:                                        |
|     - ContainerConnectionDetailsFactory detects @ServiceConnection                 |
|     - Injects:                                                                     |
|       spring.datasource.url = jdbc:postgresql://localhost:49152/test               |
|       spring.datasource.username = test                                            |
|       spring.datasource.password = test                                            |
|                                                                                    |
|  3. Flyway Migration Execution:                                                    |
|     - Flyway auto-configuration connects to container port 49152                   |
|     - Executes all V*.sql and R*.sql migration scripts against real PostgreSQL!    |
|                                                                                    |
|  4. Tests Execute: Real PostgreSQL ACID transactions, JSONB, and locks             |
|                                                                                    |
|  5. Ryuk Container Cleanup:                                                        |
|     - Testcontainers Moby Ryuk sidecar automatically terminates containers on exit |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Maven Dependencies
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-test</artifactId>
    <scope>test</scope>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-testcontainers</artifactId>
    <scope>test</scope>
</dependency>
<dependency>
    <groupId>org.testcontainers</groupId>
    <artifactId>postgresql</artifactId>
    <scope>test</scope>
</dependency>
<dependency>
    <groupId>org.testcontainers</groupId>
    <artifactId>junit-jupiter</artifactId>
    <scope>test</scope>
</dependency>
```

---

## 5. Practical Examples

### 5.1 Modern Spring Boot 3.1+ @ServiceConnection Integration
```java
package com.example.db.test;

import com.example.db.domain.Customer;
import com.example.db.repository.CustomerRepository;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@Testcontainers
class CustomerRepositoryIntegrationTest {

    // Spring Boot 3.1+ auto-wires DataSource properties from this container!
    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private CustomerRepository customerRepository;

    @Test
    @DisplayName("Verify customer entity persistence and retrieval against real PostgreSQL")
    void testSaveAndFindCustomer() {
        Customer customer = new Customer("alice@example.com", "Alice Smith");
        Customer saved = customerRepository.save(customer);

        assertThat(saved.getId()).isNotNull();

        Customer found = customerRepository.findById(saved.getId()).orElseThrow();
        assertThat(found.getEmail()).isEqualTo("alice@example.com");
    }
}
```

### 5.2 Singleton Container Pattern for Fast Test Suites
```java
package com.example.db.test;

import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;

/**
 * Abstract base test class implementing the Singleton Container pattern.
 * All inheriting test classes share the exact same container instance,
 * avoiding the overhead of starting Docker containers repeatedly!
 */
@SpringBootTest
public abstract class AbstractIntegrationTest {

    @ServiceConnection
    protected static final PostgreSQLContainer<?> POSTGRES_CONTAINER;

    static {
        POSTGRES_CONTAINER = new PostgreSQLContainer<>("postgres:16-alpine")
            .withDatabaseName("integration_db")
            .withUsername("test_user")
            .withPassword("test_password")
            .withReuse(true); // Enables local development container reuse

        POSTGRES_CONTAINER.start();
    }
}
```

### 5.3 Testing Native JSONB & Custom Dialects Impossible in H2
```java
package com.example.db.test;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.jdbc.core.simple.JdbcClient;

import static org.assertj.core.api.Assertions.assertThat;

class NativePostgreSqlFeaturesTest extends AbstractIntegrationTest {

    @Autowired
    private JdbcClient jdbcClient;

    public record DeviceMetadata(String os, String appVersion) {}

    @Test
    @DisplayName("Test native PostgreSQL JSONB operators (->>) that would fail in H2")
    void testPostgresJsonbQuery() {
        // Execute real PostgreSQL JSONB extraction: metadata->>'os'
        jdbcClient.sql("""
            INSERT INTO user_devices (user_id, metadata)
            VALUES (101, '{"os": "iOS", "appVersion": "4.2.0"}'::jsonb)
            """).update();

        String extractedOs = jdbcClient.sql("""
            SELECT metadata->>'os' FROM user_devices WHERE user_id = 101
            """)
            .query(String.class)
            .single();

        assertThat(extractedOs).isEqualTo("iOS");
    }
}
```

### 5.4 Anti-Pattern: Using In-Memory H2 for Production Testing
```yaml
# =========================================================================
# ANTI-PATTERN: application-test.yaml configured with H2 for a PostgreSQL App
# =========================================================================
spring:
  datasource:
    url: jdbc:h2:mem:testdb;MODE=PostgreSQL # FALSE SENSE OF SECURITY!
    driver-class-name: org.h2.Driver
  jpa:
    hibernate:
      ddl-auto: create-drop # Masks broken Flyway migration scripts!

/*
 * CATASTROPHIC ISSUES WITH THIS SETUP:
 * 1. H2's "MODE=PostgreSQL" implements only a tiny subset of PostgreSQL syntax.
 * 2. It does not support native JSONB operators (->>, @>, ?|).
 * 3. It does not validate production Flyway scripts; syntax errors in SQL slip into production.
 * 4. Transaction isolation and lock behaviors differ completely from PostgreSQL MVCC.
 */

// =========================================================================
// PRODUCTION FIX: Always test against real PostgreSQL via Testcontainers!
// =========================================================================
```

### 5.5 Testing Concurrent Optimistic Locking with Testcontainers
```java
package com.example.db.test;

import com.example.db.domain.BankAccount;
import com.example.db.repository.BankAccountRepository;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.dao.OptimisticLockingFailureException;
import org.springframework.transaction.PlatformTransactionManager;
import org.springframework.transaction.support.TransactionTemplate;

import java.math.BigDecimal;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;

import static org.assertj.core.api.Assertions.assertThat;

class ConcurrencyIntegrationTest extends AbstractIntegrationTest {

    @Autowired private BankAccountRepository repository;
    @Autowired private PlatformTransactionManager txManager;

    @Test
    @DisplayName("Verify real PostgreSQL MVCC triggers OptimisticLockingFailureException under concurrent updates")
    void testConcurrentUpdatesTriggerOptimisticLock() throws InterruptedException {
        BankAccount account = repository.save(new BankAccount("ACC-100", BigDecimal.valueOf(500)));
        Long accountId = account.getId();

        int threads = 2;
        ExecutorService executor = Executors.newFixedThreadPool(threads);
        CountDownLatch latch = new CountDownLatch(threads);
        AtomicInteger successCount = new AtomicInteger(0);
        AtomicInteger collisionCount = new AtomicInteger(0);

        for (int i = 0; i < threads; i++) {
            executor.submit(() -> {
                try {
                    new TransactionTemplate(txManager).executeWithoutResult(status -> {
                        BankAccount acc = repository.findById(accountId).orElseThrow();
                        acc.debit(BigDecimal.valueOf(50));
                        repository.saveAndFlush(acc);
                    });
                    successCount.incrementAndGet();
                } catch (OptimisticLockingFailureException ex) {
                    collisionCount.incrementAndGet();
                } finally {
                    latch.countDown();
                }
            });
        }

        latch.await();
        executor.shutdown();

        // Exactly one transaction must succeed; the other must throw OptimisticLockingFailureException!
        assertThat(successCount.get()).isEqualTo(1);
        assertThat(collisionCount.get()).isEqualTo(1);
    }
}
```

---

## 6. Best Practices

1. **Use `@ServiceConnection` Instead of `@DynamicPropertySource`**: `@ServiceConnection` is type-safe, eliminates manual property strings, and automatically configures credentials and driver settings.
2. **Adopt the Singleton Container Pattern for Test Suites**: Base all database integration tests on an abstract class that statically initializes a single shared container instance.
3. **Always Run Flyway / Liquibase in Tests**: Never disable migrations in tests (`ddl-auto = create-drop`); running migrations against Testcontainers verifies deployment scripts on every build.
4. **Enable Local Container Reuse (`.withReuse(true)`)**: Add `testcontainers.reuse.enable=true` in `~/.testcontainers.properties` to keep containers running across local IDE test runs.
5. **Use Minimal Alpine Images**: Prefer lightweight tags like `postgres:16-alpine` or `mysql:8.4` to reduce Docker image download times in CI/CD runners.
6. **Clean Test State Deterministically**: Clean up tables between test runs using test execution listeners, `@Sql`, or selective repository deletes rather than restarting the entire Docker container.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using Non-Static Container with `@Container`** | Non-static containers restart for *every single test method*, slowing test execution to a crawl. | Mark container fields `static` so they live for the entire test class. |
| **Using `ddl-auto: create-drop` in Tests** | Bypasses Flyway scripts; schema bugs in migration files are never detected in CI. | Set `spring.jpa.hibernate.ddl-auto = validate` and run Flyway. |
| **Missing Docker Daemon in CI Runner** | Running Testcontainers on CI environments without Docker socket permissions fails at startup. | Ensure CI runner supports Docker-in-Docker or mounts `/var/run/docker.sock`. |
| **Hardcoding Container Ports** | Binding to host port 5432 causes port collision errors if local PostgreSQL is running. | Let Testcontainers assign dynamic random ports (default behavior). |
| **Forgetting to Call `.start()` on Manual Singletons** | If using a static container without `@Testcontainers`, the container must be started explicitly in a static block. | Call `CONTAINER.start()` inside the static initializer. |
| **Leaving Containers Orphaned After Aborted JVMs** | If the test process is forcefully killed (`SIGKILL`), containers could remain running. | Ensure Testcontainers Ryuk sidecar is permitted to run. |

---

## 8. Performance & Security Notes

### Performance
- **RAM Disk / TMPFS Mounting**: Mount PostgreSQL data directories onto tmpfs (`.withTmpFs(Map.of("/var/lib/postgresql/data", "rw"))`) to run tests entirely in memory without writing to physical disk, cutting test database I/O times by 75%.
- **Container Reuse in IDEs**: When `.withReuse(true)` is enabled, re-running a test in IntelliJ takes $<500\text{ ms}$, delivering the speed of H2 with the fidelity of real PostgreSQL.

### Security
- **No Production Credential Exposure**: Testcontainers generates randomized ephemeral passwords, ensuring test credentials never leak into source repositories.

---

## 9. Comparison / When to Use What

| Feature | Testcontainers (Real PostgreSQL) | In-Memory H2 | Mockito Mocks (`@MockBean`) |
| :--- | :--- | :--- | :--- |
| **Fidelity to Production** | **100% (Identical engine & dialect)** | Poor (~60% compatibility) | None (Mocks fake behavior) |
| **Native Feature Testing (JSONB, CTE)** | **Full Support** | Unsupported | Unsupported |
| **Migration Script Verification** | **Yes (Real DDL)** | Poor (Syntax errors) | None |
| **Startup Speed** | Moderate (1–3s initial) | **Instant (~100ms)** | Instant |
| **Best Used For** | **All Repository & Integration Tests** | Fast unit prototypes | Web layer isolation (`@WebMvcTest`) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is testing against in-memory H2 considered an anti-pattern for PostgreSQL applications?</strong></summary>

While H2 is fast, it is a completely different database engine with different syntax, locking models, and capabilities. H2 cannot test native PostgreSQL features (JSONB operators, array columns, full-text search, CTEs, custom stored procedures), handles identity generation and sequences differently, and fails to validate real Flyway DDL migration scripts. Testing against H2 provides a false sense of security while letting dialect bugs escape into production.
</details>

<details>
<summary><strong>2. How does Spring Boot 3.1+ <code>@ServiceConnection</code> improve upon <code>@DynamicPropertySource</code>?</strong></summary>

Previously, developers wrote verbose `@DynamicPropertySource` methods to manually extract the container's dynamic port, URL, username, and password and register them into Spring's property source. `@ServiceConnection` eliminates this boilerplate: decorating a `@Container` field with `@ServiceConnection` automatically registers the container's connection details with Spring Boot's auto-configuration machinery.
</details>

<details>
<summary><strong>3. How does the Singleton Container pattern optimize Testcontainers performance?</strong></summary>

Starting a new Docker container for each test class adds 2–4 seconds of container boot time per class. In a project with 30 integration test classes, this adds over 90 seconds to the build. The Singleton Container pattern initializes a single shared container instance once in a base class static block, allowing all 30 test classes to run against the same container in seconds.
</details>

<details>
<summary><strong>4. What is the role of the Ryuk container in Testcontainers?</strong></summary>

Ryuk is an automated cleanup daemon spawned by Testcontainers. It connects to the Docker socket and monitors the test execution process. When the JVM terminates (even during unexpected aborts or crashes), Ryuk guarantees that all created containers, networks, and volumes are automatically deleted, preventing orphaned Docker containers from accumulating on development machines or CI runners.
</details>

<details>
<summary><strong>5. How can you optimize Testcontainers disk I/O performance during tests?</strong></summary>

By configuring a temporary in-memory filesystem (tmpfs) on the container:
```java
postgres.withTmpFs(Map.of("/var/lib/postgresql/data", "rw"));
```
This forces the database to write all temporary data directly to RAM rather than the host's physical disk, significantly speeding up DDL migrations and write-heavy tests.
</details>

<details>
<summary><strong>6. How does container reuse work in Testcontainers?</strong></summary>

Setting `.withReuse(true)` instructs Testcontainers to compute a hash of the container configuration and check if a matching container is already running on the local Docker daemon. If found, Testcontainers connects to the existing running container rather than starting a new one, reducing test startup latency to under a second during local development.
</details>

<details>
<summary><strong>7. How do you test multiple database dependencies (e.g. PostgreSQL + Redis) together?</strong></summary>

You declare multiple containers decorated with `@ServiceConnection` in the same test or abstract base test class:
```java
@Container @ServiceConnection
static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

@Container @ServiceConnection
static GenericContainer<?> redis = new GenericContainer<>("redis:7.4-alpine").withExposedPorts(6379);
```
Spring Boot automatically wires both DataSources and Redis connection factories simultaneously.
</details>

<details>
<summary><strong>8. Why should you never use static host port bindings in Testcontainers?</strong></summary>

Hardcoding port mappings (e.g., mapping container port 5432 to host port 5432) prevents parallel test execution, fails if a local PostgreSQL instance is running on the host, and causes builds to break in multi-tenant CI environments. Testcontainers automatically maps container ports to random available host ports.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic @ServiceConnection Setup</strong></summary>

**Task**: Write an integration test class with a static PostgreSQL container using `@ServiceConnection` and assert that the `DataSource` bean is properly injected.

```java
@SpringBootTest
@Testcontainers
class BasicDataSourceTest {
    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired private DataSource dataSource;

    @Test
    void testDataSourceInjected() throws SQLException {
        assertThat(dataSource.getConnection().getMetaData().getDatabaseProductName())
            .isEqualTo("PostgreSQL");
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Repository Save Assertion with Testcontainers</strong></summary>

**Task**: Write a test persisting an entity `Item(name, price)` into PostgreSQL and assert its generated ID is greater than zero.

```java
@Test
void testItemPersistence() {
    Item item = itemRepository.save(new Item("Widget", BigDecimal.valueOf(19.99)));
    assertThat(item.getId()).isPositive();
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Flyway Migration Execution Assertion</strong></summary>

**Task**: Write a test verifying that Flyway migrations ran against the Testcontainers database by querying the `flyway_schema_history` table.

```java
@Test
void testMigrationsExecuted() {
    Integer count = jdbcClient.sql("SELECT COUNT(*) FROM flyway_schema_history WHERE success = true")
        .query(Integer.class)
        .single();
    assertThat(count).isGreaterThan(0);
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Shared Singleton Abstract Integration Base Class</strong></summary>

**Task**: Create an abstract base class `BaseIntegrationTest` that starts a singleton PostgreSQL container with tmpfs configuration.

```java
@SpringBootTest
public abstract class BaseIntegrationTest {
    @ServiceConnection
    protected static final PostgreSQLContainer<?> POSTGRES;

    static {
        POSTGRES = new PostgreSQLContainer<>("postgres:16-alpine")
            .withTmpFs(Map.of("/var/lib/postgresql/data", "rw"));
        POSTGRES.start();
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Multi-Container PostgreSQL + Redis Integration Test</strong></summary>

**Task**: Configure both PostgreSQL and Redis containers in an integration test verifying that a cached repository writes to PostgreSQL and populates Redis.

```java
@SpringBootTest
@Testcontainers
class MultiServiceIntegrationTest {
    @Container @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Container @ServiceConnection
    static GenericContainer<?> redis = new GenericContainer<>("redis:7.4-alpine").withExposedPorts(6379);

    @Autowired private ProductService productService;
    @Autowired private StringRedisTemplate redisTemplate;

    @Test
    void testCachedProductFlow() {
        Product p = productService.createProduct("Tablet", BigDecimal.valueOf(399));
        Product cached = productService.getProductCached(p.getId());
        assertThat(cached).isNotNull();
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): PostgreSQL Constraint Violation Verification</strong></summary>

**Task**: Write a test asserting that inserting a duplicate record into a PostgreSQL table with a partial unique index throws `DataIntegrityViolationException`.

```java
@Test
void testPartialUniqueConstraintViolation() {
    customerRepository.saveAndFlush(new Customer("unique@test.com", "Active"));

    assertThatThrownBy(() -> {
        customerRepository.saveAndFlush(new Customer("unique@test.com", "Active"));
    }).isInstanceOf(DataIntegrityViolationException.class);
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Annotation:    @Container @ServiceConnection static PostgreSQLContainer<?> postgres; |
| Zero Config:   @ServiceConnection auto-wires URL, credentials, and Flyway connection! |
| Speed Rule 1:  Use the Singleton Container pattern to avoid container restart lag     |
| Speed Rule 2:  Enable .withTmpFs() to run test database in memory without disk I/O    |
| Local Dev:     Enable .withReuse(true) for near-instant test re-runs in IDE           |
| Golden Rule:   NEVER test PostgreSQL with H2; use real Testcontainers in CI/CD!       |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Testcontainers Official Documentation](https://java.testcontainers.org/)
- [Spring Boot 3.1+ @ServiceConnection Guide](https://docs.spring.io/spring-boot/reference/testing/testcontainers.html#testing.testcontainers.service-connections)
- [AtomicJar: Testcontainers Best Practices](https://testcontainers.com/guides/getting-started-with-testcontainers-for-java/)
- [Why You Should Stop Using In-Memory Databases for Testing](https://phauer.com/2017/dont-use-in-memory-databases-tests-h2/)
