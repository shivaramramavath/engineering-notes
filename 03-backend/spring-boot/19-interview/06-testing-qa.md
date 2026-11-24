# 06: Spring Boot Testing & Testcontainers Technical Interview Q&A

| Metadata Field | Value |
|---|---|
| **Topic** | Automated Testing, Test Slices, Testcontainers `@ServiceConnection` & Mocking |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Level** | Senior / Lead / Staff Backend Engineer |
| **Key Focus Areas** | Test Slices (`@WebMvcTest`, `@DataJpaTest`), `@ServiceConnection`, `@MockitoBean`, Context Caching |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Practical Usage](#4-syntax--practical-usage)
5. [5 Practical Examples](#5-5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison / Decision Matrix](#9-comparison--decision-matrix)
10. [Real-World Interview Q&A](#10-real-world-interview-qa)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference Cheatsheet](#12-summary--quick-reference-cheatsheet)
13. [Further Reading & Official Resources](#13-further-reading--official-resources)

---

## 1. What & Why

Automated testing is the backbone of high-velocity engineering teams. However, poorly structured testing in Spring Boot often results in brittle test suites, slow build times (taking 30+ minutes), and unrealistic H2 in-memory databases that mask PostgreSQL/MySQL dialect bugs.

Senior and staff candidates are expected to understand the full testing pyramid in Spring Boot 3.4:
- When to use focused **Test Slices** (`@WebMvcTest`, `@DataJpaTest`) versus full `@SpringBootTest`.
- How **Testcontainers** with Spring Boot 3.1+ `@ServiceConnection` eliminates manual `@DynamicPropertySource` boilerplate.
- The migration in Spring Boot 3.4 from `@MockBean` to `@MockitoBean`.
- How the **Spring TestContext Framework caches ApplicationContexts** across test classes, and how the anti-pattern `@DirtiesContext` destroys build performance.

---

## 2. Core Concepts

### 2.1 Test Slices Architecture
Rather than bootstrapping the entire application context, Test Slices load only a narrow subset of beans:
- `@WebMvcTest`: Loads `DispatcherServlet`, `@Controller`, `@ControllerAdvice`, filters, and security; mocks service/repository beans. Runs in $< 1\text{ second}$.
- `@DataJpaTest`: Loads `EntityManager`, JPA repositories, Flyway, and DataSource; mocks web/controller layers.
- `@JsonTest`: Tests Jackson serialization/deserialization bindings.

### 2.2 Testcontainers & `@ServiceConnection`
Historically, integration tests required manual wiring:
```java
@DynamicPropertySource
static void props(DynamicPropertyRegistry r) {
    r.add("spring.datasource.url", postgres::getJdbcUrl);
}
```
In modern Spring Boot (3.1+), annotating the container with `@ServiceConnection` automatically discovers container ports, JDBC URLs, and credentials, injecting them directly into Spring's Environment:
```java
@Container
@ServiceConnection
static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");
```

### 2.3 Spring Boot 3.4 `@MockitoBean`
Spring Boot 3.4 deprecated `@MockBean` and `@SpyBean` in favor of `@MockitoBean` and `@MockitoSpyBean`, aligning with standard Mockito annotations while participating cleanly in Spring's test context caching.

---

## 3. Internals & Architecture

```
 ┌────────────────────────────────────────────────────────┐
 │               Spring TestContext Framework             │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │              Test Context Cache Key Lookup             │
 │  Key = (Config Classes, Active Profiles, Mocks...)     │
 └─────────────┬────────────────────────────┬─────────────┘
               │ (Cache Hit)                │ (Cache Miss / First Class)
               ▼                            ▼
 ┌───────────────────────────┐   ┌────────────────────────┐
 │ Reuse Existing Context    │   │ Start New Spring Context│
 │ Execution Time: ~10ms     │   │ Execution Time: ~3-5s  │
 └───────────────────────────┘   └──────────┬─────────────┘
                                            │
                                            ▼
                                 ┌────────────────────────┐
                                 │ Testcontainers Spins Up│
                                 │ Real Docker PostgreSQL │
                                 │ @ServiceConnection     │
                                 └────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 `@MockitoBean` in Spring Boot 3.4
```java
@WebMvcTest(OrderController.class)
public class OrderControllerTest {
    @Autowired
    private MockMvc mockMvc;

    @MockitoBean // Replaced @MockBean in Spring Boot 3.4
    private OrderService orderService;
}
```

---

## 5. 5 Practical Examples

### Example 1: High-Speed Web Slice Test with `@WebMvcTest` and `@MockitoBean`

```java
package com.example.testing.web;

import com.example.testing.service.OrderService;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.http.MediaType;
import org.springframework.test.context.bean.override.mockito.MockitoBean;
import org.springframework.test.web.servlet.MockMvc;

import java.math.BigDecimal;
import java.util.UUID;

import static org.mockito.BDDMockito.given;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

@WebMvcTest(OrderController.class)
public class OrderControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @MockitoBean // Modern Spring Boot 3.4 override
    private OrderService orderService;

    @Test
    void shouldReturnOrderDetails() throws Exception {
        UUID orderId = UUID.randomUUID();
        given(orderService.getOrderTotal(orderId)).willReturn(new BigDecimal("99.99"));

        mockMvc.perform(get("/api/v1/orders/" + orderId).accept(MediaType.APPLICATION_JSON))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.orderId").value(orderId.toString()))
                .andExpect(jsonPath("$.total").value(99.99));
    }
}
```

### Example 2: Data JPA Slice Test with Real PostgreSQL Testcontainer

```java
package com.example.testing.data;

import com.example.testing.domain.Customer;
import com.example.testing.repository.CustomerRepository;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.jdbc.AutoConfigureTestDatabase;
import org.springframework.boot.test.autoconfigure.orm.jpa.DataJpaTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import static org.assertj.core.api.Assertions.assertThat;

@DataJpaTest
@Testcontainers
@AutoConfigureTestDatabase(replace = AutoConfigureTestDatabase.Replace.NONE)
public class CustomerRepositoryTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private CustomerRepository repository;

    @Test
    void shouldPersistAndFindCustomer() {
        Customer customer = new Customer("alice@company.com", "Alice Smith");
        Customer saved = repository.save(customer);

        assertThat(saved.getId()).isNotNull();
        assertThat(repository.findByEmail("alice@company.com")).isPresent();
    }
}
```

### Example 3: Full Integration Test with TestRestTemplate

```java
package com.example.testing;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@Testcontainers
public class FullSystemIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private TestRestTemplate restTemplate;

    @Test
    void shouldCreateAndRetrieveResource() {
        ResponseEntity<Map> response = restTemplate.postForEntity(
                "/api/v1/orders",
                Map.of("sku", "IPHONE-15", "quantity", 1),
                Map.class
        );

        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.CREATED);
        assertThat(response.getBody()).containsKey("orderId");
    }
}
```

### Example 4: Testing Jackson Serialization with `@JsonTest`

```java
package com.example.testing.json;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.json.JsonTest;
import org.springframework.boot.test.json.JacksonTester;

import java.math.BigDecimal;
import java.time.Instant;

import static org.assertj.core.api.Assertions.assertThat;

@JsonTest
public class OrderDtoJsonTest {

    @Autowired
    private JacksonTester<OrderDto> json;

    public record OrderDto(String orderId, BigDecimal amount, Instant createdAt) {}

    @Test
    void shouldSerializeExpectedJson() throws Exception {
        OrderDto dto = new OrderDto("ord-123", new BigDecimal("49.99"), Instant.parse("2026-11-24T12:00:00Z"));

        assertThat(json.write(dto))
                .hasJsonPathStringValue("$.orderId", "ord-123")
                .hasJsonPathNumberValue("$.amount", 49.99)
                .hasJsonPathStringValue("$.createdAt", "2026-11-24T12:00:00Z");
    }
}
```

### Example 5: Shared Singleton Container Base Class for Fast Test Suites

```java
package com.example.testing.base;

import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;

// Reusable singleton container base class: Started ONCE for the entire test run!
public abstract class AbstractIntegrationTestBase {

    @ServiceConnection
    protected static final PostgreSQLContainer<?> POSTGRES;

    static {
        POSTGRES = new PostgreSQLContainer<>("postgres:16-alpine");
        POSTGRES.start();
    }
}
```

---

## 6. Best Practices

- **Never Use In-Memory H2 for PostgreSQL/MySQL Production Tests**: H2 lacks support for native JSONB, window functions, and complex locking semantics. Use Testcontainers with `@ServiceConnection`.
- **Use Singleton Containers Across Tests**: Inherit from a base class with a `static` container (or use the singleton container pattern) so Docker containers start once per test run rather than per class.
- **Avoid `@DirtiesContext`**: Marking tests with `@DirtiesContext` forces Spring to destroy and recreate the `ApplicationContext` for subsequent tests, inflating CI build times from 1 minute to 20 minutes.
- **Use Test Slices for Granular Testing**: Test controllers using `@WebMvcTest` (sub-second execution); test repositories using `@DataJpaTest`. Reserve `@SpringBootTest` for a few end-to-end smoke flows.
- **Migrate to `@MockitoBean` in Spring Boot 3.4**: Replace legacy `@MockBean` with `@MockitoBean`.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **H2 In-Memory Database for JPA Tests** | H2 allows SQL queries and data types that crash in PostgreSQL production. | Test against identical database engines via Testcontainers. |
| **Overusing `@DirtiesContext`** | Evicts the TestContext cache after every test method, increasing build times by $10\times$. | Isolate test state by cleaning database tables in `@BeforeEach` instead. |
| **Using `@SpringBootTest` for Everything** | Starts full Tomcat, scheduled jobs, and messaging brokers for simple controller unit tests. | Use `@WebMvcTest` for controller tests. |
| **Starting Docker Container per Test Class** | Starting 50 PostgreSQL containers in 50 test classes adds minutes of Docker startup lag. | Use the Singleton Container Pattern (start once statically). |
| **Scattering Random `@MockitoBean` Mocks** | Every unique combination of `@MockitoBean` creates a distinct context cache key, breaking context reuse. | Define consistent mock profiles across test classes. |
| **Calling `Thread.sleep()` in Async Tests** | Brittle tests that fail randomly on slow CI servers. | Use `Awaitility.await().atMost(5, SECONDS).untilAsserted(...)`. |

---

## 8. Performance & Security Considerations

### 8.1 Spring TestContext Caching Key Mechanics
Spring caches contexts using a composite key:
- Classpath configuration classes (`@ContextConfiguration`)
- Active profiles (`@ActiveProfiles`)
- Injected Mock beans (`@MockitoBean`)
Keeping these configurations identical across test classes allows dozens of tests to share a single in-memory Spring container.

### 8.2 Awaitility for Asynchronous Verification
Never sleep in tests; use Awaitility:
```java
Awaitility.await()
        .atMost(Duration.ofSeconds(3))
        .untilAsserted(() -> assertThat(repository.count()).isEqualTo(1));
```

---

## 9. Comparison / Decision Matrix

| Test Style | Startup Time | Scope | Real Dependencies |
|---|---|---|---|
| **Unit Test (Plain Mockito)** | $< 50\text{ms}$ | Single class isolation | None (All mocked) |
| **`@WebMvcTest` (Slice)** | $\sim 500\text{ms}$ | Controller + Validation + Web | Mocked services (`@MockitoBean`) |
| **`@DataJpaTest` (Slice)** | $\sim 1-2\text{s}$ | Repositories + DB Schema | Real DB via Testcontainers |
| **`@SpringBootTest` (Full)**| $\sim 4-8\text{s}$ | Full end-to-end container | Real DB + Gateway + Messaging |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. What does `@ServiceConnection` do in Spring Boot 3.1+?</strong></summary>

It automatically detects the service type of a Testcontainer (e.g. `PostgreSQLContainer`, `KafkaContainer`, `RedisContainer`) and injects the corresponding connection properties (`spring.datasource.url`, `spring.kafka.bootstrap-servers`) into Spring's Environment without needing manual `@DynamicPropertySource` methods.
</details>

<details>
<summary><strong>2. Why should developers avoid testing with in-memory H2 databases?</strong></summary>

H2 uses a completely different SQL parser, indexing engine, and transaction locking model than PostgreSQL or MySQL. Features like JSONB columns, CTEs, full-text search, and dialect-specific functions fail in production despite passing in H2 test suites.
</details>

<details>
<summary><strong>3. How does the Spring TestContext Framework caching mechanism work?</strong></summary>

Spring stores initialized `ApplicationContext` instances in an in-memory cache keyed by configuration classes, active profiles, and mock definitions. If a subsequent test class requires the exact same context configuration, Spring reuses the running context in milliseconds instead of bootstrapping a new one.
</details>

<details>
<summary><strong>4. What is the danger of using `@DirtiesContext`?</strong></summary>

`@DirtiesContext` marks the cached `ApplicationContext` as modified, forcing Spring to shut it down and re-instantiate a brand-new context for subsequent tests. Overusing it on multiple test classes balloons CI test execution times from minutes to hours.
</details>

<details>
<summary><strong>5. What is the difference between `@WebMvcTest` and `@SpringBootTest`?</strong></summary>

`@SpringBootTest` starts the full application context with all beans, services, and repositories. `@WebMvcTest` is a focused slice test that only initializes the web layer (`DispatcherServlet`, controllers, filters, validation), mocking the service and data layers to run in sub-second time.
</details>

<details>
<summary><strong>6. What replaced `@MockBean` in Spring Boot 3.4?</strong></summary>

Spring Boot 3.4 introduced `@MockitoBean` (and `@MockitoSpyBean`) under `org.springframework.test.context.bean.override.mockito`, deprecating `@MockBean` to align with the core Spring Framework bean overriding infrastructure.
</details>

<details>
<summary><strong>7. How do you implement the Singleton Container Pattern in Testcontainers?</strong></summary>

Declare the container as `protected static final` in an abstract base test class and start it inside a `static { ... }` initializer block. All test classes extending the base class share the exact same running container without restarting Docker between classes.
</details>

<details>
<summary><strong>8. How do you reliably test asynchronous event-driven code in Spring?</strong></summary>

Avoid `Thread.sleep()`. Use the **Awaitility** library to poll and assert conditions with an explicit timeout: `await().atMost(5, SECONDS).untilAsserted(() -> assertThat(...));`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Write Controller Slice Test with `@MockitoBean`</strong></summary>

**Prompt**: Write a `@WebMvcTest` for `CustomerController` mocking `CustomerService.findCustomer()` using `@MockitoBean`.

**Solution**:
Inspect Example 1: annotate class with `@WebMvcTest(CustomerController.class)` and inject `@MockitoBean CustomerService`.
</details>

<details>
<summary><strong>Exercise 2: Configure Testcontainers with `@ServiceConnection`</strong></summary>

**Prompt**: Declare a PostgreSQL Testcontainer in a `@DataJpaTest` using `@ServiceConnection`.

**Solution**:
Inspect Example 2: annotate static container with `@Container` and `@ServiceConnection`.
</details>

<details>
<summary><strong>Exercise 3: Test JSON DTO Deserialization with `@JsonTest`</strong></summary>

**Prompt**: Verify that Jackson correctly parses an ISO-8601 date string into a Java `Instant` field.

**Solution**:
Inspect Example 4: use `JacksonTester<T>.parse()` and assert field values.
</details>

<details>
<summary><strong>Exercise 4: Test Asynchronous Message Listener with Awaitility</strong></summary>

**Prompt**: Publish a message to a Kafka topic and use Awaitility to verify that the database table count increases to 1 within 5 seconds.

**Solution**:
```java
kafkaTemplate.send("events", "payload");
Awaitility.await().atMost(Duration.ofSeconds(5))
        .untilAsserted(() -> assertThat(repository.count()).isEqualTo(1));
```
</details>

<details>
<summary><strong>Exercise 5: Refactor `@DirtiesContext` to Clean Database Isolation</strong></summary>

**Prompt**: Remove `@DirtiesContext` from a test class and use `@BeforeEach` to clean repository tables cleanly.

**Solution**:
Remove annotation; add `@BeforeEach void cleanup() { repository.deleteAll(); }`. Context is preserved while database tables remain clean.
</details>

<details>
<summary><strong>Exercise 6: Implement Singleton Testcontainer Base Class</strong></summary>

**Prompt**: Create an abstract class starting PostgreSQL and Redis containers once statically.

**Solution**:
Inspect Example 5: declare `static final PostgreSQLContainer` and `static final RedisContainer` with `.start()` in a static block.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   SPRING BOOT TESTING CHEATSHEET                       │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Database: Real Testcontainers via @ServiceConnection (never H2)     │
│ 2. Slices: @WebMvcTest for controllers; @DataJpaTest for repositories  │
│ 3. Mocking: Use @MockitoBean in Spring Boot 3.4 (replaces @MockBean)   │
│ 4. Speed: Avoid @DirtiesContext; leverage Spring TestContext caching   │
│ 5. Pattern: Singleton Testcontainer base class across all test suites  │
│ 6. Async: Awaitility.await() instead of Thread.sleep()                 │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring Boot 3.4 Testing Reference](https://docs.spring.io/spring-boot/reference/testing/index.html)
- [Testcontainers Spring Boot Integration Guide](https://java.testcontainers.org/modules/databases/postgres/)
- [Spring Framework TestContext Caching Internals](https://docs.spring.io/spring-framework/reference/testing/testcontext-framework/ctx-management/caching.html)
- [Awaitility Asynchronous Testing Guide](http://www.awaitility.org/)
