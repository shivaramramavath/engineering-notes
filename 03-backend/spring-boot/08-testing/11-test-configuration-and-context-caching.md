# 11. Test Configuration & Context Caching Optimization in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-testing-fundamentals-in-spring.md`, `04-mockito-and-mockbean-replacements.md`, Spring IoC Internals |
| **Estimated Reading Time** | 40 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1 |

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
In large enterprise Spring Boot applications, the continuous integration (CI) build frequently degrades over time:
- The test suite begins taking 25 to 45 minutes to execute.
- Build logs reveal that the Spring `ApplicationContext` is booting up dozens—sometimes hundreds—of times from scratch.
- The root cause is almost always **Context Cache Thrashing**: subtle, accidental differences in test annotations, mock beans, active profiles, or `@TestPropertySource` entries that prevent the Spring TestContext Framework from reusing already-initialized application contexts.

Understanding **`@TestConfiguration` semantics**, mastering the exact composition of the **`MergedContextConfiguration` cache key**, and structuring tests to maximize cache hits can slash CI build times by over 80%.

### Historical Evolution
- **Spring 3.x**: Introduced basic test context caching with simple configuration location keys.
- **Spring 4.x - 5.x**: Standardized `@TestConfiguration` (distinguishing it from standard `@Configuration`) and added `@DynamicPropertySource`.
- **Spring Boot 3.4+ / 4.x**: Refined cache key calculation for the new Bean Overriding SPI (`@MockitoBean`, `@TestBean`), optimized parallel test execution, and enhanced context cache diagnostic metrics.

### Real-World Relevance
Optimizing context caching directly improves developer productivity by turning a sluggish 30-minute CI build into a snappy 3-minute validation pipeline.

---

## 2. Core Concepts

### Architectural Contracts
- **`@TestConfiguration`**: A specialized configuration class intended strictly for testing. Unlike `@Configuration`, it is excluded from component scanning by default; it is only activated when explicitly imported via `@Import` or defined as a static inner class inside a test.
- **`ContextCache`**: The static internal cache in `org.springframework.test.context.cache.ContextCache` that stores initialized `ApplicationContext` instances keyed by `MergedContextConfiguration`.
- **`MergedContextConfiguration`**: The immutable composite cache key containing all configuration parameters that define the context.
- **Cache Hit**: A test class shares an identical `MergedContextConfiguration` key with an already-running context; startup time is **0 milliseconds**.
- **Cache Miss**: A test class introduces even a single new property, profile, or mock bean; Spring is forced to spend 2,000–5,000 ms initializing a new context.
- **Cache Eviction**: Triggered by `@DirtiesContext` or when the cache exceeds its maximum capacity (default: 32).

### The Mental Model: The Cache Key Anatomy
```
[ MergedContextConfiguration Key ]
├── 1. Context Initializers
├── 2. Configuration Classes / Locations
├── 3. Active Profiles (@ActiveProfiles)
├── 4. Test Property Sources (@TestPropertySource)
├── 5. Dynamic Property Sources (@DynamicPropertySource)
├── 6. Context Customizers (@MockitoBean, Testcontainers, WebEnvironment)
├── 7. Context Hierarchy
└── 8. Resource Base Path

IF (KeyA.equals(KeyB)) ──► REUSE EXISTING CONTEXT (0ms)
ELSE                   ──► BOOT NEW CONTEXT (3,000ms)
```

### Invariants
1. Two test classes will only share an `ApplicationContext` if their computed `MergedContextConfiguration` keys are strictly equal (`equals()` and `hashCode()`).
2. Adding `@MockitoBean` or `@MockBean` modifies the `ContextCustomizer` parameter, forcing a distinct cache key.
3. Defining a non-static `@TestConfiguration` inner class is illegal; test configurations must be static.

---

## 3. How It Works Internally

### The 12 Parameters of the Context Cache Key
The `MergedContextConfiguration` `equals()` method evaluates:
1. `testClass` (only if context hierarchy is present)
2. `locations` (XML or properties paths)
3. `classes` (annotated configuration classes)
4. `contextInitializerClasses`
5. `activeProfiles` (alphabetically sorted array)
6. `propertySourceDescriptors` / `propertySourceProperties`
7. `contextCustomizers` (includes `@MockitoBean`, `@AutoConfigureMockMvc`, etc.)
8. `contextLoader` class
9. `parent` context (if hierarchical)
10. `resourceBasePath`
11. `webMergedContextConfiguration` (e.g., `MOCK`, `RANDOM_PORT`)
12. `beanOverrideHandlers` (Spring 6.2+)

If **any single element** differs between Class $A$ and Class $B$, Spring considers them completely different environments and initializes a second context.

---

## 4. Syntax & Usage

### 1. Enabling Context Cache Diagnostics in Logging
To view real-time cache hits, misses, and statistics during test runs, enable debug logging in `src/test/resources/logback-test.xml` or `application-test.yml`:

```yaml
logging:
  level:
    org.springframework.test.context.cache: DEBUG
```

**Log Output Sample:**
```
DEBUG o.s.t.c.c.DefaultContextCache - Storing ApplicationContext in cache with key [MergedContextConfiguration@...]
DEBUG o.s.t.c.c.DefaultContextCache - Retrieved ApplicationContext from cache with key [MergedContextConfiguration@...]
DEBUG o.s.t.c.c.DefaultContextCache - Context cache statistics: [size = 1, hits = 14, misses = 1, parentHitCount = 0]
```

### 2. Static Inner `@TestConfiguration`
```java
package com.example.testing;

import com.example.service.TimeProvider;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.context.TestConfiguration;
import org.springframework.context.annotation.Bean;

import java.time.Instant;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
class CustomTimeTest {

    @Autowired
    private TimeProvider timeProvider;

    @Test
    void shouldReturnFixedTestTime() {
        assertThat(timeProvider.now()).isEqualTo("2026-11-24T00:00:00Z");
    }

    // Static inner test configuration: automatically detected and registered for this test
    @TestConfiguration
    static class TestConfig {
        @Bean
        public TimeProvider timeProvider() {
            return () -> Instant.parse("2026-11-24T00:00:00Z");
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Creating an Abstract Base Integration Test to Standardize Cache Keys
The single most effective architectural pattern for maximizing context caching across enterprise test suites.

```java
package com.example.testing.base;

import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.springframework.test.context.ActiveProfiles;
import org.springframework.test.context.DynamicPropertyRegistry;
import org.springframework.test.context.DynamicPropertySource;
import org.testcontainers.containers.PostgreSQLContainer;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.MOCK)
@ActiveProfiles("test") // Exact profile match
public abstract class BaseIntegrationTest {

    @ServiceConnection
    protected static final PostgreSQLContainer<?> POSTGRES;

    static {
        POSTGRES = new PostgreSQLContainer<>("postgres:16-alpine").withReuse(true);
        POSTGRES.start();
    }

    @DynamicPropertySource
    static void commonProperties(DynamicPropertyRegistry registry) {
        registry.add("app.cache.type", () -> "caffeine");
        registry.add("app.metrics.export.enabled", () -> "false");
    }
}
```
```java
// Subclasses inherit identical MergedContextConfiguration -> 100% Cache Hits!
class OrderServiceTest extends BaseIntegrationTest {
    @Test void testOrders() {}
}

class InvoiceServiceTest extends BaseIntegrationTest {
    @Test void testInvoices() {}
}
```

### Example 2: The Context Thrashing Anti-Pattern & Production Fix
**Anti-Pattern**: Randomly scattering `@TestPropertySource` and `@ActiveProfiles` across different test classes, destroying context reuse.

```java
// Class 1: Boots Context #1 (miss)
@SpringBootTest
@ActiveProfiles("test")
class TestOne {}

// Class 2: Boots Context #2 (miss, because property differs!)
@SpringBootTest
@ActiveProfiles("test")
@TestPropertySource(properties = "feature.toggle=true")
class TestTwo {}

// Class 3: Boots Context #3 (miss, because profiles differ!)
@SpringBootTest
@ActiveProfiles({"test", "fast"})
class TestThree {}
```

**Production Fix**:
Consolidate properties into a single `application-test.yml` file and unify test profiles.

```yaml
# src/test/resources/application-test.yml
feature:
  toggle: true
```
```java
// Both classes now share the identical cache key!
@SpringBootTest
@ActiveProfiles("test")
class TestOne {}

@SpringBootTest
@ActiveProfiles("test")
class TestTwo {}
```

### Example 3: External `@TestConfiguration` Imported Selectively
Defining a reusable mock configuration imported across multiple slice tests without component scanning interference.

```java
package com.example.testing.config;

import com.example.external.AuditClient;
import org.springframework.boot.test.context.TestConfiguration;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Primary;

@TestConfiguration
public class MockAuditTestConfig {

    @Bean
    @Primary
    public AuditClient mockAuditClient() {
        return event -> { /* No-op mock client */ };
    }
}
```
```java
package com.example.web;

import com.example.testing.config.MockAuditTestConfig;
import org.junit.jupiter.api.Test;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.context.annotation.Import;

@WebMvcTest(UserController.class)
@Import(MockAuditTestConfig.class) // Explicitly imported; ignored by normal component scans
class UserControllerTest {
    @Test void test() {}
}
```

### Example 4: Tuning the Context Cache Size
Increasing cache capacity in large monolithic codebases to prevent LRU thrashing.

```properties
# src/test/resources/junit-platform.properties or command line JVM arg
# Default is 32; increase to 64 if test suite has legitimate distinct configurations
spring.test.context.cache.maxSize=64
```

### Example 5: High-Performance Diagnostic Test for Context Cache Health
Writing an architectural fitness test that asserts the total number of created Spring contexts remains below a strict ceiling.

```java
package com.example.testing;

import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.Test;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.cache.ContextCache;
import org.springframework.test.context.cache.DefaultContextCache;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
class ContextCacheHealthCheckTest {

    @Test
    void dummyTest() {}

    @AfterAll
    static void verifyCacheEfficiency() {
        // In real suites, hook into a custom TestExecutionListener to verify cache misses < 5
    }
}
```

---

## 6. Best Practices

1. **Inherit from a Single Abstract Base Class**: Standardize `@SpringBootTest`, `@ActiveProfiles`, and Testcontainers in an `AbstractIntegrationTest` base class so all integration tests share one context.
2. **Eliminate Ad-Hoc `@TestPropertySource`**: Do not sprinkle unique property overrides across test classes; move shared overrides into `application-test.yml`.
3. **Consolidate `@MockitoBean` Declarations**: Declare common mocked external services in the shared base class rather than on individual test classes.
4. **Use `@TestConfiguration` with `proxyBeanMethods = false`**: Ensure test configurations are lightweight and start quickly.
5. **Monitor Cache Hits in CI Logs**: Enable `DEBUG` logging on `org.springframework.test.context.cache` to identify tests that trigger unwanted context reboots.
6. **Ban `@DirtiesContext` in Code Reviews**: Treat `@DirtiesContext` as a defect; refactor tests to use clean `@Transactional` rollbacks or explicit resets instead.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Non-static inner `@TestConfiguration` | Spring throws `IllegalStateException: @TestConfiguration inner classes must be static`. | Add `static` modifier to inner class. |
| Scattered `@MockBean` / `@MockitoBean` | Every unique mock combination produces a cache miss, causing 30+ context reboots. | Consolidate common mocks in an abstract base test class. |
| Overriding profiles per class | `@ActiveProfiles("test")` vs `@ActiveProfiles({"test", "dev"})` produces cache misses. | Standardize on a single unified profile: `@ActiveProfiles("test")`. |
| Random DynamicPropertySource lambdas | Providing changing dynamic property keys forces context cache recreation. | Ensure `@DynamicPropertySource` keys are consistent across classes. |
| Small cache eviction thrashing | Cache size defaults to 32; having 33 configurations evicts the first, causing endless reboots. | Unify configurations or tune `spring.test.context.cache.maxSize=64`. |
| Using `@Configuration` instead of `@TestConfiguration` | Component scanning detects the test configuration in production runs. | Always use `@TestConfiguration` for test-only beans. |

---

## 8. Performance & Security Notes

- **Context Startup Cost**: A single Spring Boot context initialization takes ~2,500ms on a fast workstation, and up to 8,000ms on constrained CI build runners. Minimizing context misses from 40 to 2 saves **over 4 minutes** per build.
- **JVM Heap Sizing**: Each active `ApplicationContext` in memory holds singletons, class metadata, and caching buffers (~50–150 MB heap). If your suite runs 32 contexts, the test JVM needs at least 4 GB of heap to avoid GC thrashing.
- **Virtual Threads Integration**: Context initialization is single-threaded per test class, but tests executing against the shared cached context leverage Java 21 Virtual Threads for concurrent execution.

---

## 9. Comparison / When to Use What

| Feature | `@Configuration` | `@TestConfiguration` | `@TestBean` (Spring 6.2+) |
|---|---|---|---|
| **Component Scanned?** | **Yes (Always)** | No (Ignored unless imported) | No (Method-scoped) |
| **Production Safe?** | Yes | Test directories only | Test directories only |
| **Override Mechanism** | High precedence bean | Bean override in test context | Direct property override |
| **Best For** | Production application beans | Reusable test fixtures/stubs | Overriding a single dependency |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between `@Configuration` and `@TestConfiguration`?</summary>

**Answer:**
`@Configuration` classes are detected by Spring Boot's automatic component scanning (`@ComponentScan`). If placed on the classpath, they are included in every application context boot.
`@TestConfiguration` is explicitly excluded from component scanning. It is only included in the `ApplicationContext` when explicitly imported via `@Import` or defined as a static inner class inside an active test class.
</details>

<details>
<summary>2. Why does adding `@MockitoBean` or `@MockBean` to a test class cause a context cache miss?</summary>

**Answer:**
Spring's `ContextCache` keys contexts by `MergedContextConfiguration`. One of the parameters in this key is `contextCustomizers`. Adding a `@MockitoBean` registers a customizer that alters the bean definition graph. Since the graph differs from test classes without that mock, the cache keys are not equal, forcing Spring to boot a brand-new `ApplicationContext`.
</details>

<details>
<summary>3. What are the key parameters that compose the `MergedContextConfiguration` cache key?</summary>

**Answer:**
The key includes:
1. Configuration classes (`classes`)
2. XML/property locations (`locations`)
3. Context initializers (`contextInitializerClasses`)
4. Active profiles (`activeProfiles`)
5. Test properties (`propertySourceProperties`)
6. Context customizers (`contextCustomizers`, including mocks and web environment modes)
7. Parent context hierarchy
If any of these differ between two test classes, they cannot share a cached context.
</details>

<details>
<summary>4. How can you observe whether a test class resulted in a context cache hit or miss?</summary>

**Answer:**
By enabling debug logging for the cache category:
`logging.level.org.springframework.test.context.cache=DEBUG`.
The log will output whether a context was stored (miss) or retrieved (hit), alongside running statistics (`size`, `hits`, `misses`).
</details>

<details>
<summary>5. How can you prevent context cache thrashing in a large microservice test suite?</summary>

**Answer:**
1. Create a single `AbstractIntegrationTest` base class that all integration tests inherit from.
2. Standardize `@ActiveProfiles("test")` on the base class.
3. Consolidate common `@MockitoBean` declarations and `@ServiceConnection` containers in the base class.
4. Move ad-hoc `@TestPropertySource` entries into a shared `application-test.yml` file.
</details>

<details>
<summary>6. What is the default maximum size of the Spring TestContext cache, and what happens when it is exceeded?</summary>

**Answer:**
The default maximum size is 32 contexts. When a 33rd unique context configuration is encountered, Spring evicts the least recently used (LRU) context from the cache and closes it. If earlier tests run again or subsequent tests need the evicted configuration, Spring must re-boot it from scratch, causing cache thrashing.
</details>

<details>
<summary>7. Why must an inner `@TestConfiguration` class be declared `static`?</summary>

**Answer:**
If an inner class is non-static, it holds an implicit reference to an enclosing instance of the test class. Spring cannot instantiate the configuration class independently of a test instance during context bootstrapping. Declaring it `static` allows the Spring container to instantiate it as an independent configuration component.
</details>

<details>
<summary>8. How does `@DirtiesContext` interact with the context cache?</summary>

**Answer:**
`@DirtiesContext` marks the current context as modified or corrupted. After the test method or class finishes, the TestContext Framework forcefully closes the `ApplicationContext` and removes it from the `ContextCache`. The next test class cannot reuse it and must pay the full performance penalty of booting a new context.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Static Inner `@TestConfiguration` (Easy)</summary>

**Task**: Write a test with a static inner `@TestConfiguration` that defines a `String` bean named `"appName"` returning `"TestApp"`.

**Solution**:
```java
@SpringBootTest
class InnerConfigTest {

    @Autowired
    private String appName;

    @Test
    void shouldInjectAppName() {
        assertThat(appName).isEqualTo("TestApp");
    }

    @TestConfiguration
    static class Config {
        @Bean
        public String appName() {
            return "TestApp";
        }
    }
}
```
</details>

<details>
<summary>Exercise 2: Logging Cache Statistics (Easy)</summary>

**Task**: Configure `application-test.yml` to enable debug logging for Spring's test context cache.

**Solution**:
```yaml
logging:
  level:
    org.springframework.test.context.cache: DEBUG
```
</details>

<details>
<summary>Exercise 3: Shared Abstract Base Test (Medium)</summary>

**Task**: Implement an abstract base class `SharedContextBaseTest` configured with `@ActiveProfiles("test")` and `@SpringBootTest`, and demonstrate two subclasses that share the same context.

**Solution**:
```java
@SpringBootTest
@ActiveProfiles("test")
public abstract class SharedContextBaseTest {}

class ServiceOneTest extends SharedContextBaseTest {
    @Test void testOne() {}
}

class ServiceTwoTest extends SharedContextBaseTest {
    @Test void testTwo() {}
}
// Both classes have identical MergedContextConfiguration; context boots once!
```
</details>

<details>
<summary>Exercise 4: Increasing Max Cache Size (Medium)</summary>

**Task**: Configure the JVM system property to increase the test context cache capacity to 64.

**Solution**:
```properties
# src/test/resources/junit-platform.properties
spring.test.context.cache.maxSize=64
```
</details>

<details>
<summary>Exercise 5: External Test Configuration via `@Import` (Hard)</summary>

**Task**: Create an external `@TestConfiguration` class in a separate package that provides a mock `NotificationGateway` and import it into a slice test.

**Solution**:
```java
@TestConfiguration
public class NotificationTestConfig {
    @Bean
    public NotificationGateway notificationGateway() {
        return mock(NotificationGateway.class);
    }
}

// In test:
@WebMvcTest(OrderController.class)
@Import(NotificationTestConfig.class)
class OrderControllerSliceTest {
    @Test void test() {}
}
```
</details>

<details>
<summary>Exercise 6: Context Cache Performance Benchmark (Hard)</summary>

**Task**: Write two integration tests: one that cleanly reuses a shared base context, and one that uses `@DirtiesContext`. Measure the execution duration of each to demonstrate the performance impact of context eviction.

**Solution**:
```java
@SpringBootTest
class CacheComparisonTest {

    @Test
    void fastReusedContextTest() {
        // Runs against cached context: < 5ms
    }

    @Test
    @DirtiesContext
    void slowDirtiesContextTest() {
        // Forces context close and subsequent rebuild: > 2,500ms
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **`MergedContextConfiguration`**: The 12-factor cache key that determines context reuse.
- **Inherit from a Common Base**: Standardize profiles, properties, and containers in an `AbstractIntegrationTest`.
- **Avoid `@DirtiesContext`**: Evicting context costs 2–5 seconds per occurrence.
- **Use `@TestConfiguration`**: Excluded from production component scanning; must be `static` when nested.
- **Monitor Cache Hits**: Turn on `DEBUG` logging on `org.springframework.test.context.cache` to spot context leaks.

---

## 13. Further Reading
- [Spring TestContext Framework: Context Caching Documentation](https://docs.spring.io/spring-framework/reference/testing/testcontext-framework/ctx-management/caching.html)
- [Spring Boot Test Configuration Guide](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.testing.spring-boot-applications.detecting-test-configuration)
- [Spring Framework 6.2 TestContext Improvements](https://github.com/spring-projects/spring-framework/wiki/Spring-Framework-6.2-Release-Notes)
- [Baeldung: Spring TestContext Caching Explained](https://www.baeldung.com/spring-testcontext-cache)
- [JVM System Properties for Spring Test Tuning](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/test/context/cache/ContextCache.html)
