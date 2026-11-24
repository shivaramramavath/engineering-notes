# 01. Testing Fundamentals & The Spring TestContext Framework

| Metadata | Value |
|---|---|
| **Level** | Beginner |
| **Prerequisites** | JUnit 5 Basics, Core Spring IoC & DI (`01-core-spring-framework/`) |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, JUnit 5.11 |

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
In enterprise Java development, automated tests frequently degrade into an unmanageable, slow quagmire:
- Developers annotate every test with `@SpringBootTest`, forcing the framework to initialize database connection pools, message brokers, caching infrastructure, and security filters for simple domain logic tests.
- Build times balloon from 30 seconds to 45 minutes, destroying developer feedback loops and slowing CI/CD pipelines.
- Improper test setup triggers subtle test pollution where test $A$ modifies an in-memory bean or database row, causing an unrelated test $B$ to fail intermittently depending on test execution order.

Understanding the **Testing Pyramid** and mastering the internal mechanics of the **Spring TestContext Framework (TCF)** is essential for building test suites that are both thorough and blisteringly fast.

### Historical Evolution
- **Spring 1.x - 2.x**: Relied on heavy inheritance (`AbstractDependencyInjectionSpringContextTests`). Tests manually tore down contexts, and startup times were excruciating.
- **Spring 3.x - 4.x**: Introduced the annotation-driven TestContext Framework, pluggable `TestExecutionListener` contracts, and automated context caching.
- **Spring Boot 1.x - 2.x**: Introduced specialized test slices (`@WebMvcTest`, `@DataJpaTest`) and `@SpringBootTest(webEnvironment = ...)`.
- **Spring Boot 3.x / 4.x**: Optimized for Java 21, introducing `@MockitoBean` / `@MockitoSpyBean` to supersede legacy `@MockBean`, first-class Testcontainers integration with `@ServiceConnection`, and parallel test execution support.

### Real-World Relevance
High-performing engineering teams maintain a healthy testing pyramid: 70% pure unit tests (sub-millisecond execution), 20% integration test slices (fast focused Spring context), and 10% full-stack containerized tests.

---

## 2. Core Concepts

### Architectural Contracts
- **`org.springframework.test.context.TestContextManager`**: The main entry point into the Spring TestContext Framework. Manages a single `TestContext` instance and coordinates lifecycle events across test execution.
- **`org.springframework.test.context.TestContext`**: Holds the context of the currently executing test (the test class, method, exception, and the backing `ApplicationContext`).
- **`org.springframework.test.context.TestExecutionListener`**: Lifecycle SPI reacting to test events: `beforeTestClass`, `prepareTestInstance`, `beforeTestMethod`, `afterTestMethod`, and `afterTestClass`.
- **ApplicationContext Caching**: The mechanism by which the Spring TCF reuses already-initialized `ApplicationContext` instances across different test classes with identical configuration.
- **`@SpringBootTest`**: Meta-annotation that searches upward for `@SpringBootConfiguration` (typically `@SpringBootApplication`) and boots the full application context.
- **`@DirtiesContext`**: Annotation signaling that a test corrupted or modified the Spring context, instructing the framework to evict it from the cache and close it.

### The Mental Model: The Testing Pyramid
```
                   ▲
                  / \
                 /   \     E2E / Full App Tests (~10%)
                /     \    @SpringBootTest + Testcontainers (Slowest, 2-10s)
               /───────\
              /         \   Integration Slice Tests (~20%)
             /           \  @WebMvcTest, @DataJpaTest, @JsonTest (Medium, 100-500ms)
            /─────────────\
           /               \ Pure Unit Tests (~70%)
          /                 \ Plain JUnit 5 + Mockito (Fastest, < 5ms)
         /───────────────────\
```

### Invariants
1. A test should run in the lowest feasible layer of the pyramid: if plain Java logic can verify the requirement, never boot an `ApplicationContext`.
2. The Spring TestContext cache key is derived from the unique combination of configuration parameters (active profiles, property overrides, context initializers, mock beans). Any alteration forces a new context startup.
3. Tests must be completely deterministic and idempotent: running tests in isolation, in parallel, or in arbitrary sequences must yield identical results.

---

## 3. How It Works Internally

### The Test Context Caching Algorithm
When a test class annotated with `@SpringBootTest` or a slice annotation runs:
```
Test Class Execution Begins
   │
   ├── 1. TestContextManager computes MergedContextConfiguration Key:
   │      - Context Initializers
   │      - Config Locations / Classes
   │      - Active Profiles (@ActiveProfiles)
   │      - Property Source Overrides (@TestPropertySource)
   │      - Context Customizers (MockBeans, DynamicPropertySources)
   │
   ├── 2. Queries Static ContextCache:
   │      ├─ Cache HIT: Reuses running ApplicationContext! (0 ms startup)
   │      │
   │      └─ Cache MISS:
   │            ├─ Boots new Spring ApplicationContext (1,500 - 5,000 ms)
   │            └─ Stores in ContextCache (default max size = 32)
   │
   ├── 3. Executes TestExecutionListeners:
   │      ├─ DependencyInjectionTestExecutionListener: Injects @Autowired fields
   │      ├─ DirtiesContextBeforeModesTestExecutionListener
   │      ├─ TransactionalTestExecutionListener: Starts DB transaction
   │      └─ ...
   │
   ├── 4. Executes @Test method
   │
   └── 5. Cleanup & Eviction:
          ├─ If @DirtiesContext present: Closes and removes context from cache!
          └─ Transaction rolls back automatically (if @Transactional)
```

---

## 4. Syntax & Usage

### 1. Web Environment Modes in `@SpringBootTest`
```java
package com.example.testing;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.boot.test.web.server.LocalServerPort;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class RandomPortIntegrationTest {

    @LocalServerPort
    private int port;

    @Autowired
    private TestRestTemplate restTemplate;

    @Test
    void shouldReturnHealthStatusOverRealHttp() {
        String response = restTemplate.getForObject("http://localhost:" + port + "/actuator/health", String.class);
        assertThat(response).contains("UP");
    }
}
```

### WebEnvironment Options:
- `MOCK` (Default): Configures a mock servlet environment; embedded server (Tomcat/Netty) is **not** started. Used with `MockMvc`.
- `RANDOM_PORT`: Starts a real embedded web container on an available dynamic OS port. Ideal for `TestRestTemplate` or `WebTestClient`.
- `DEFINED_PORT`: Starts the embedded container on the configured `server.port` (default 8080). Prone to port collisions in CI.
- `NONE`: Boots standard `ApplicationContext` without any servlet/reactive web infrastructure (ideal for batch jobs and message consumers).

---

## 5. Practical Examples

### Example 1: Pure Unit Test Without Spring (The Pyramid Foundation)
Testing complex business domain rules in less than 2 milliseconds without booting Spring.

```java
package com.example.service;

import com.example.domain.DiscountEngine;
import com.example.domain.Order;
import org.junit.jupiter.api.Test;

import java.math.BigDecimal;

import static org.assertj.core.api.Assertions.assertThat;

class DiscountEngineTest {

    private final DiscountEngine engine = new DiscountEngine();

    @Test
    void shouldApplyVipDiscountWhenTotalExceedsThreshold() {
        Order order = new Order("VIP", new BigDecimal("150.00"));

        BigDecimal finalPrice = engine.calculateFinalPrice(order);

        assertThat(finalPrice).isEqualByComparingTo("120.00");
    }
}
```

### Example 2: Integration Slice Test (`@JsonTest`)
Validating custom Jackson serialization and deserialization in milliseconds without loading repositories or controllers.

```java
package com.example.dto;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.json.JsonTest;
import org.springframework.boot.test.json.JacksonTester;

import java.time.Instant;

import static org.assertj.core.api.Assertions.assertThat;

@JsonTest
class OrderEventJsonTest {

    @Autowired
    private JacksonTester<OrderEvent> json;

    @Test
    void shouldSerializeInstantToIso8601() throws Exception {
        OrderEvent event = new OrderEvent("EVT-100", Instant.parse("2026-11-24T12:00:00Z"));

        assertThat(json.write(event))
            .hasJsonPathStringValue("@.eventId", "EVT-100")
            .hasJsonPathStringValue("@.timestamp", "2026-11-24T12:00:00Z");
    }

    @Test
    void shouldDeserializeFromJsonPayload() throws Exception {
        String content = "{\"eventId\":\"EVT-200\",\"timestamp\":\"2026-11-24T15:30:00Z\"}";

        OrderEvent event = json.parse(content).getObject();

        assertThat(event.eventId()).isEqualTo("EVT-200");
    }

    public record OrderEvent(String eventId, Instant timestamp) {}
}
```

### Example 3: Context Cache Preservation Across Test Classes
Ensuring multiple integration test classes share the exact same cached `ApplicationContext` by inheriting from a common base configuration.

```java
package com.example.testing.base;

import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.ActiveProfiles;
import org.springframework.test.context.DynamicPropertyRegistry;
import org.springframework.test.context.DynamicPropertySource;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.MOCK)
@ActiveProfiles("test")
public abstract class AbstractIntegrationTest {

    @DynamicPropertySource
    static void commonProperties(DynamicPropertyRegistry registry) {
        registry.add("spring.cloud.vault.enabled", () -> "false");
        registry.add("app.cache.ttl-seconds", () -> "30");
    }
}
```
```java
package com.example.testing;

import com.example.testing.base.AbstractIntegrationTest;
import org.junit.jupiter.api.Test;

class OrderIntegrationTest extends AbstractIntegrationTest {
    @Test
    void testOrder() { /* Context is reused! 0ms startup */ }
}

class InvoiceIntegrationTest extends AbstractIntegrationTest {
    @Test
    void testInvoice() { /* Context is reused! 0ms startup */ }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Carelessly decorating tests with `@DirtiesContext` because of state leakage, completely wiping the context cache and forcing slow reboots for every subsequent test class.

```java
// SEVERE PERFORMANCE ANTI-PATTERN:
// Forces Spring to destroy the ApplicationContext and re-initialize the entire app from scratch!
// 100 tests * 4s reboot = 400 seconds (almost 7 minutes) build time!
@SpringBootTest
@DirtiesContext(classMode = DirtiesContext.ClassMode.AFTER_EACH_TEST_METHOD)
class SlowTest {
    @Test void testA() {}
    @Test void testB() {}
}
```

**Production Fix**:
Use `@Transactional` to roll back database modifications automatically, or clean up mock states explicitly in `@AfterEach` rather than destroying the entire container.

```java
// FAST & REUSABLE: Context remains cached; database transaction rolls back cleanly
@SpringBootTest
@Transactional
class FastTest {
    @Test void testA() {}
    @Test void testB() {}
}
```

### Example 5: High-Performance Parallel Test Execution Configuration
Leveraging JUnit 5 parallel execution while preserving Spring context caching safely.

```properties
# src/test/resources/junit-platform.properties
junit.jupiter.execution.parallel.enabled=true
junit.jupiter.execution.parallel.mode.default=concurrent
junit.jupiter.execution.parallel.mode.classes.default=concurrent
junit.jupiter.execution.parallel.config.strategy=dynamic
junit.jupiter.execution.parallel.config.dynamic.factor=1.0
```

---

## 6. Best Practices

1. **Adhere to the 70/20/10 Testing Pyramid**: Maximize pure JUnit 5 tests. Use test slices for web/data layers, and reserve `@SpringBootTest` for end-to-end integration flows.
2. **Standardize Test Configurations to Maximize Cache Hits**: Ensure test classes share identical `@ActiveProfiles`, property overrides, and configuration classes so Spring reuses the running context.
3. **Avoid `@DirtiesContext`**: Treat `@DirtiesContext` as a last resort code smell. Investigate why a test pollutes shared state (e.g., mutable static singletons or unrolled DB transactions) and fix the root cause.
4. **Use `webEnvironment = RANDOM_PORT` for Real HTTP Tests**: Prevents port conflicts when running tests in parallel or on shared CI build agents.
5. **Inherit from an `AbstractIntegrationTest` Base Class**: Centralize test annotations and container configuration to guarantee identical context cache keys.
6. **Prefer `@Transactional` on Test Classes**: Automatically rolls back database mutations at the end of each test method, keeping the database clean for the next test.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Inadvertent context cache busting | Slight differences in `@MockBean` or `@TestPropertySource` force Spring to create new contexts. | Standardize mock beans and properties on a shared abstract base test class. |
| Overusing `@SpringBootTest` | Running the full context to test a single utility class slows builds drastically. | Use plain JUnit 5 unit tests without Spring. |
| Hardcoded ports (`DEFINED_PORT`) | Parallel builds fail with `BindException: Address already in use`. | Use `webEnvironment = RANDOM_PORT` and `@LocalServerPort`. |
| Uncleaned static state | Modifying static variables pollutes downstream tests that share the same JVM. | Avoid static state or reset it explicitly in `@AfterEach`. |
| Missing `@Transactional` on repository tests | Data inserted in Test A remains in the database, causing Test B to fail with duplicate key errors. | Annotate integration tests with `@Transactional` for auto-rollback. |
| Running tests against H2 instead of production DB | H2 lacks PostgreSQL-specific types (JSONB, UUID, full-text search), hiding production bugs. | Use Testcontainers with real PostgreSQL/MySQL instances. |

---

## 8. Performance & Security Notes

- **Context Cache Memory Limits**: By default, the Spring TestContext cache stores up to 32 contexts (`spring.test.context.cache.maxSize=32`). In huge microservices, 32 contexts will consume several gigabytes of JVM heap, triggering GC thrashing. Keep unique test configurations below 5.
- **Virtual Threads in Testing**: When parallel test execution is enabled alongside Java 21 Virtual Threads, integration tests waiting on HTTP or database I/O scale effortlessly without thread pool saturation.
- **Test Secrets Isolation**: Ensure test profiles disable external cloud integrations (Vault, AWS Secrets Manager) and supply isolated mock credentials.

---

## 9. Comparison / When to Use What

| Test Style | Annotation | Context Boot Time | Database State | Best Use Case |
|---|---|---|---|---|
| **Pure Unit Test** | None (Plain JUnit 5) | 0 ms | None (Mocked) | Domain logic, pricing rules, algorithms |
| **Web Slice Test** | `@WebMvcTest` | 100 - 300 ms | None | Controller routing, validation, HTTP mapping |
| **Data Slice Test**| `@DataJpaTest` | 200 - 500 ms | Auto-rollback | Custom JPQL/SQL queries, entity mappings |
| **Full Integration**| `@SpringBootTest` | 1,500 - 5,000 ms | Real or Testcontainers | Cross-layer end-to-end integration verification |

---

## 10. Interview Questions

<details>
<summary>1. How does Spring's ApplicationContext caching work during a test suite run?</summary>

**Answer:**
Spring's TestContext Framework calculates a cache key (the `MergedContextConfiguration`) based on test class metadata: configuration classes, initializers, active profiles, property sources, and customizers. When a new test class executes, Spring checks its static context cache. If a context matching that exact configuration already exists, it is reused immediately (0 ms startup). If not, a new context is initialized and cached.
</details>

<details>
<summary>2. What is the performance impact of `@DirtiesContext`?</summary>

**Answer:**
`@DirtiesContext` informs Spring that a test corrupted the application context (e.g., modified bean state, closed a socket, or altered singleton definitions). Spring is forced to evict that context from the cache and close it. The next test class must pay the full CPU and I/O penalty of booting a brand-new `ApplicationContext` from scratch, which drastically inflates total test suite execution time.
</details>

<details>
<summary>3. What is the difference between `@SpringBootTest(webEnvironment = MOCK)` and `RANDOM_PORT`?</summary>

**Answer:**
- `MOCK` (default): Does not start a real embedded servlet container (Tomcat). Instead, it simulates the web layer using a mock servlet environment, executed via `MockMvc`.
- `RANDOM_PORT`: Starts a real embedded web server listening on a randomly assigned, available port. It enables testing full network I/O, filters, WebSocket handshakes, and serialization using real HTTP clients (`TestRestTemplate` or `RestClient`).
</details>

<details>
<summary>4. Why should developers avoid testing domain business logic with `@SpringBootTest`?</summary>

**Answer:**
Booting a full `@SpringBootTest` takes seconds because it initializes the entire dependency injection graph, database pools, and auto-configurations. Domain business logic should be self-contained and free of framework dependencies. Testing domain logic with plain JUnit 5 unit tests executes in microseconds, allowing hundreds of tests to run in less than a second.
</details>

<details>
<summary>5. What is a `TestExecutionListener` and how does Spring use it?</summary>

**Answer:**
`TestExecutionListener` is an SPI that hooks into test lifecycle phases. Spring uses built-in listeners for:
- Injecting dependencies into test instances (`DependencyInjectionTestExecutionListener`).
- Managing test database transactions and automatic rollback (`TransactionalTestExecutionListener`).
- Handling context dirtiness (`DirtiesContextTestExecutionListener`).
- Setting up security contexts (`WithSecurityContextTestExecutionListener`).
</details>

<details>
<summary>6. How does `@Transactional` behave differently on a test class versus a production service class?</summary>

**Answer:**
On a production service class, `@Transactional` commits the database transaction upon successful method completion.
On a test class (or test method), `@Transactional` starts a transaction before the test executes, but **automatically rolls it back** at the end of the test method by default. This ensures the database remains clean without leaving test data behind.
</details>

<details>
<summary>7. How can you override a single property in a Spring Boot test without breaking context caching?</summary>

**Answer:**
You can use `@TestPropertySource(properties = "feature.toggle=true")` or `@DynamicPropertySource`. However, if only one test class has this property override, Spring must create and cache a distinct context for it. To preserve caching across multiple tests, extract the property override to a shared base class or shared `@ActiveProfiles("test")` file.
</details>

<details>
<summary>8. What is the maximum size of Spring's internal TestContext cache, and how can it be tuned?</summary>

**Answer:**
The default maximum size is 32 contexts. If the number of unique context configurations exceeds 32, Spring evicts the least recently used (LRU) context. It can be tuned via the JVM system property `-Dspring.test.context.cache.maxSize=64`.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Minimal JUnit 5 Unit Test (Easy)</summary>

**Task**: Write a pure JUnit 5 unit test for an `EmailValidator` utility class without any Spring annotations.

**Solution**:
```java
class EmailValidatorTest {
    private final EmailValidator validator = new EmailValidator();

    @Test
    void shouldRejectInvalidEmail() {
        assertThat(validator.isValid("invalid-email")).isFalse();
    }

    @Test
    void shouldAcceptValidEmail() {
        assertThat(validator.isValid("user@example.com")).isTrue();
    }
}
```
</details>

<details>
<summary>Exercise 2: Basic `@SpringBootTest` with Random Port (Easy)</summary>

**Task**: Create a test using `@SpringBootTest` on a random port and inject the port using `@LocalServerPort`.

**Solution**:
```java
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class ServerPortTest {

    @LocalServerPort
    private int port;

    @Test
    void shouldAssignDynamicPort() {
        assertThat(port).isPositive();
    }
}
```
</details>

<details>
<summary>Exercise 3: Test Property Override (Medium)</summary>

**Task**: Configure a test using `@TestPropertySource` that sets `app.notifications.enabled=false` and verify the injected property value.

**Solution**:
```java
@SpringBootTest
@TestPropertySource(properties = "app.notifications.enabled=false")
class PropertyOverrideTest {

    @Value("${app.notifications.enabled}")
    private boolean notificationsEnabled;

    @Test
    void shouldHonorPropertyOverride() {
        assertThat(notificationsEnabled).isFalse();
    }
}
```
</details>

<details>
<summary>Exercise 4: Context Caching Verification (Medium)</summary>

**Task**: Create two separate test classes inheriting from a shared base integration test class and verify via log output that the Spring application context starts only once.

**Solution**:
```java
@SpringBootTest
@ActiveProfiles("test")
public abstract class SharedBaseTest {}

class FeatureATest extends SharedBaseTest {
    @Test void testFeatureA() {}
}

class FeatureBTest extends SharedBaseTest {
    @Test void testFeatureB() {}
}
// Both classes share the same MergedContextConfiguration, booting the context only once.
```
</details>

<details>
<summary>Exercise 5: Dynamic Property Source Configuration (Hard)</summary>

**Task**: Write a test using `@DynamicPropertySource` to programmatically supply a dynamic database URL generated at runtime.

**Solution**:
```java
@SpringBootTest
class DynamicPropertiesTest {

    @DynamicPropertySource
    static void configureDynamicProperties(DynamicPropertyRegistry registry) {
        registry.add("spring.datasource.url", () -> "jdbc:h2:mem:dynamic_db;DB_CLOSE_DELAY=-1");
        registry.add("spring.datasource.username", () -> "sa");
    }

    @Test
    void contextLoadsSuccessfully() {}
}
```
</details>

<details>
<summary>Exercise 6: Custom TestExecutionListener (Hard)</summary>

**Task**: Implement a custom `TestExecutionListener` that prints the total execution time of each test method and register it on a test class.

**Solution**:
```java
public class ExecutionTimeTestListener implements TestExecutionListener {
    private long startTime;

    @Override
    public void beforeTestMethod(TestContext testContext) {
        startTime = System.currentTimeMillis();
    }

    @Override
    public void afterTestMethod(TestContext testContext) {
        long duration = System.currentTimeMillis() - startTime;
        System.out.println("Test " + testContext.getTestMethod().getName() + " took " + duration + " ms");
    }
}

// On Test Class:
@SpringBootTest
@TestExecutionListeners(listeners = ExecutionTimeTestListener.class, mergeMode = TestExecutionListeners.MergeMode.MERGE_WITH_DEFAULTS)
class TimedIntegrationTest {
    @Test void testWorkload() {}
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Testing Pyramid**: Maximize pure unit tests (70%), use slices for boundaries (20%), use `@SpringBootTest` for end-to-end integration (10%).
- **Context Caching is Essential**: Identical test configurations reuse the cached `ApplicationContext` across classes.
- **Never Use `@DirtiesContext` Lightly**: Evicting context destroys test suite performance by forcing slow context reboots.
- **`RANDOM_PORT` Avoids Conflicts**: Prevents port binding errors in parallel CI test jobs.
- **Use `@Transactional` on Tests**: Rolls back database state automatically after each test method.

---

## 13. Further Reading
- [Spring Framework Testing Documentation](https://docs.spring.io/spring-framework/reference/testing.html)
- [Spring Boot Testing Features](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.testing)
- [JUnit 5 User Guide: Parallel Execution](https://junit.org/junit5/docs/current/user-guide/#writing-tests-parallel-execution)
- [Martin Fowler: Practical Test Pyramid](https://martinfowler.com/articles/practical-test-pyramid.html)
- [Spring TestContext Framework Reference](https://docs.spring.io/spring-framework/reference/testing/testcontext-framework.html)
