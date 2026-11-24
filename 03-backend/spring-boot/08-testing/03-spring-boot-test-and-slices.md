# 03. Spring Boot Test Slices: Deep Dive & Architecture

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | `01-testing-fundamentals-in-spring.md`, `02-unit-testing-without-spring.md`, Spring Boot Auto-configuration |
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
In testing, developers are often caught between two extremes:
1. **Pure Unit Tests**: Extremely fast (< 5ms), but unable to verify Spring framework integrations (HTTP routing, JSON serialization, validation constraints, security annotations, and SQL generation).
2. **Full `@SpringBootTest`**: Verifies all integrations, but initializes the complete application context (database pools, Kafka brokers, security chains, schedulers), taking 2 to 5 seconds per class.

**Spring Boot Test Slices** solve this by initializing only a targeted, isolated subset of the Spring `ApplicationContext`:
- Testing a REST controller loads **only** the web layer (`@WebMvcTest`).
- Testing a repository loads **only** the JPA and DataSource layer (`@DataJpaTest`).
- Testing JSON serialization loads **only** the Jackson configuration (`@JsonTest`).
- Testing an outbound HTTP client loads **only** the REST client infrastructure (`@RestClientTest`).

### Historical Evolution
- **Spring Boot 1.3 and earlier**: Testing required either pure unit tests or full `@SpringApplicationConfiguration` runs.
- **Spring Boot 1.4**: Introduced the `@...Test` slice architecture (`@WebMvcTest`, `@DataJpaTest`, `@JsonTest`, `@RestClientTest`).
- **Spring Boot 2.x - 3.x**: Added reactive slices (`@WebFluxTest`), NoSQL slices (`@DataMongoTest`, `@DataRedisTest`), and streamlined `@AutoConfigureTestDatabase`.
- **Spring Boot 3.4+ / 4.x**: Optimized for Java 21 Virtual Threads, decoupled `@MockitoBean` from slice internals, and integrated `@ServiceConnection` for Testcontainers.

### Real-World Relevance
Test slices occupy the crucial middle tier of the testing pyramid (~20%). They execute in 100 to 300 milliseconds while giving engineers complete confidence in framework-level integration boundaries.

---

## 2. Core Concepts

### Architectural Contracts: The Major Test Slices
- **`@WebMvcTest`**: Boots Spring MVC infrastructure: `@Controller`, `@ControllerAdvice`, `@JsonComponent`, `Converter`, `Filter`, `WebMvcConfigurer`, and `HandlerMethodArgumentResolver`. Does **not** load `@Service`, `@Repository`, or `@Component` beans.
- **`@WebFluxTest`**: The reactive counterpart to `@WebMvcTest`. Boots reactive controllers, routers, and `WebFilter` instances without starting an HTTP server.
- **`@DataJpaTest`**: Boots Spring Data JPA: entities, repositories, `EntityManager`, and an in-memory/embedded `DataSource`. Executes tests inside an auto-rollback transaction by default.
- **`@DataJdbcTest`**: Boots Spring Data JDBC without JPA/Hibernate overhead.
- **`@JsonTest`**: Boots only the JSON serialization infrastructure (`ObjectMapper`, Jackson modules, `JacksonTester`).
- **`@RestClientTest`**: Boots infrastructure for testing outbound REST clients (`RestClient`, `RestTemplate`), auto-configuring `MockRestServiceServer`.
- **`@DataMongoTest` / `@DataRedisTest`**: Boots specialized NoSQL repositories and connection templates.

### The Mental Model
```
Full Application Context (@SpringBootTest)
┌────────────────────────────────────────────────────────────────────────┐
│  [@Controller]    [@Service]    [@Repository]    [DataSource / JPA]    │
│  [SecurityFilters] [MessageListeners] [Schedulers] [CacheManager]      │
└────────────────────────────────────────────────────────────────────────┘

Targeted Test Slices:
┌────────────────────────┐  ┌────────────────────────┐  ┌────────────────┐
│      @WebMvcTest       │  │      @DataJpaTest      │  │   @JsonTest    │
│  - Controllers         │  │  - Repositories        │  │  - Jackson     │
│  - MockMvc             │  │  - EntityManager       │  │  - DTO Mapper  │
│  - JSON converters     │  │  - DataSource (Rollback│  │  - Date parsing│
└────────────────────────┘  └────────────────────────┘  └────────────────┘
```

### Invariants
1. A test slice only boots the beans registered in its slice configuration; any business service required by the SUT must be supplied via a mock (e.g., `@MockitoBean`).
2. Slices that touch relational data (`@DataJpaTest`, `@DataJdbcTest`) are transactional by default and automatically roll back at the end of each test method.
3. By default, `@DataJpaTest` replaces real production data sources with an embedded database (H2). To test against real databases (PostgreSQL/MySQL), configure `@AutoConfigureTestDatabase(replace = AutoConfigureTestDatabase.Replace.NONE)`.

---

## 3. How It Works Internally

### The `@TypeExcludeFilter` Slice Isolation Engine
When a slice annotation (e.g., `@WebMvcTest`) is encountered:
1. Spring Boot applies `@BootstrapWith(SpringBootTestContextBootstrapper.class)`.
2. Inspects `@AutoConfigurationPackage` and registers a specialized **`TypeExcludeFilter`** (e.g., `WebMvcTypeExcludeFilter`).
3. During component scanning, the exclude filter intercepts every candidate class:
   - If candidate is `@Controller` or `@ControllerAdvice` $\implies$ **Include**.
   - If candidate is `@Service`, `@Repository`, or generic `@Component` $\implies$ **Exclude** from context.
4. Auto-configuration imports are restricted strictly to relevant slice configurations listed in `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`.
5. The resulting lean `ApplicationContext` initializes in a fraction of the time of a full application boot.

---

## 4. Syntax & Usage

### 1. `@WebMvcTest` with `@MockitoBean` (Spring Boot 3.4+)
```java
package com.example.web;

import com.example.service.OrderService;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.http.MediaType;
import org.springframework.test.context.bean.override.mockito.MockitoBean;
import org.springframework.test.web.servlet.MockMvc;

import static org.mockito.BDDMockito.given;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

@WebMvcTest(OrderController.class) // Scoped strictly to OrderController
class OrderControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @MockitoBean // Modern Spring Boot 3.4+ replacement for @MockBean
    private OrderService orderService;

    @Test
    void shouldReturnOrderDetails() throws Exception {
        given(orderService.getOrder("ORD-101"))
            .willReturn(new OrderResponse("ORD-101", 99.99));

        mockMvc.perform(get("/api/orders/ORD-101")
                .accept(MediaType.APPLICATION_JSON))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.id").value("ORD-101"))
            .andExpect(jsonPath("$.amount").value(99.99));
    }
}
```

---

## 5. Practical Examples

### Example 1: `@DataJpaTest` with Real Database (Testcontainers Integration)
Testing JPA repositories, custom queries, and auditing using PostgreSQL via Testcontainers instead of H2.

```java
package com.example.repository;

import com.example.domain.Product;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.jdbc.AutoConfigureTestDatabase;
import org.springframework.boot.test.autoconfigure.orm.jpa.DataJpaTest;
import org.springframework.boot.test.autoconfigure.orm.jpa.TestEntityManager;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import java.math.BigDecimal;
import java.util.List;

import static org.assertj.core.api.Assertions.assertThat;

@DataJpaTest
@Testcontainers
@AutoConfigureTestDatabase(replace = AutoConfigureTestDatabase.Replace.NONE) // Do NOT use embedded H2!
class ProductRepositoryTest {

    @Container
    @ServiceConnection // Auto-configures DataSource properties for the slice
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private ProductRepository repository;

    @Autowired
    private TestEntityManager entityManager;

    @Test
    void shouldFindProductsByCategoryWithStock() {
        entityManager.persist(new Product("Laptop", "ELECTRONICS", 10, new BigDecimal("1200.00")));
        entityManager.persist(new Product("Phone", "ELECTRONICS", 0, new BigDecimal("800.00")));
        entityManager.flush();

        List<Product> available = repository.findByCategoryAndStockQuantityGreaterThan("ELECTRONICS", 0);

        assertThat(available).hasSize(1);
        assertThat(available.get(0).getName()).isEqualTo("Laptop");
    }
}
```

### Example 2: `@RestClientTest` for Outbound HTTP Client Testing
Testing an HTTP client service using `MockRestServiceServer` without starting a real external HTTP server.

```java
package com.example.client;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.client.RestClientTest;
import org.springframework.http.MediaType;
import org.springframework.test.web.client.MockRestServiceServer;

import static org.assertj.core.api.Assertions.assertThat;
import static org.springframework.test.web.client.match.MockRestRequestMatchers.requestTo;
import static org.springframework.test.web.client.response.MockRestResponseCreators.withSuccess;

@RestClientTest(CurrencyExchangeClient.class)
class CurrencyExchangeClientTest {

    @Autowired
    private CurrencyExchangeClient client;

    @Autowired
    private MockRestServiceServer server;

    @Test
    void shouldFetchExchangeRate() {
        server.expect(requestTo("https://api.exchangerate.com/latest?base=USD"))
            .andRespond(withSuccess("{\"rates\":{\"EUR\":0.92}}", MediaType.APPLICATION_JSON));

        Double rate = client.getRate("USD", "EUR");

        assertThat(rate).isEqualTo(0.92);
        server.verify();
    }
}
```

### Example 3: Importing Custom Beans into Slices via `@Import`
When a test slice excludes a necessary utility or security component, use `@Import` to inject it selectively.

```java
package com.example.web;

import com.example.security.SecurityConfig;
import com.example.util.CustomDateFormatter;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.context.annotation.Import;
import org.springframework.test.web.servlet.MockMvc;

import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

@WebMvcTest(ReportController.class)
@Import({SecurityConfig.class, CustomDateFormatter.class}) // Explicitly import excluded components
class ReportControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void shouldRenderFormattedReport() throws Exception {
        mockMvc.perform(get("/api/reports/daily"))
            .andExpect(status().isOk());
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Using `@SpringBootTest` to test a simple REST controller, causing database connections, message brokers, and background schedulers to boot up unnecessarily.

```java
// SLOW & BLOATED: 3,500ms startup for a simple controller validation check!
@SpringBootTest
@AutoConfigureMockMvc
class SlowControllerTest {
    @Autowired private MockMvc mockMvc;
    // Context initializes entire application graph!
}
```

**Production Fix**:
Use `@WebMvcTest(ControllerName.class)` to isolate the web layer.

```java
// FAST & FOCUSED: 150ms startup!
@WebMvcTest(SimpleController.class)
class FastControllerTest {
    @Autowired private MockMvc mockMvc;
}
```

### Example 5: High-Performance Non-Blocking `@WebFluxTest`
Testing reactive endpoints and functional routing without blocking threads.

```java
package com.example.reactive;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.reactive.WebFluxTest;
import org.springframework.test.context.bean.override.mockito.MockitoBean;
import org.springframework.test.web.reactive.server.WebTestClient;
import reactor.core.publisher.Mono;

import static org.mockito.BDDMockito.given;

@WebFluxTest(controllers = ReactiveUserController.class)
class ReactiveUserControllerTest {

    @Autowired
    private WebTestClient webTestClient;

    @MockitoBean
    private ReactiveUserService userService;

    @Test
    void shouldReturnUserStream() {
        given(userService.findUsername(42L)).willReturn(Mono.just("Alice"));

        webTestClient.get().uri("/users/42")
            .exchange()
            .expectStatus().isOk()
            .expectBody(String.class).isEqualTo("Alice");
    }
}
```

---

## 6. Best Practices

1. **Scope `@WebMvcTest` to Specific Controllers**: Always specify the target controller class: `@WebMvcTest(OrderController.class)`. Omitting the class forces Spring to load all controllers in the project into the slice.
2. **Disable Embedded DB Replacement when Testing Real Dialects**: Use `@AutoConfigureTestDatabase(replace = AutoConfigureTestDatabase.Replace.NONE)` on `@DataJpaTest` when verifying native SQL, JSONB, or PostgreSQL-specific extensions.
3. **Use `@RestClientTest` for Outbound APIs**: Avoid running real WireMock servers when simple mock response matching is sufficient.
4. **Use `@JsonTest` for Custom Serializers**: Test date/time formats, custom deserializers, and null-handling rules in isolation without mock MVC overhead.
5. **Keep Slices Focused**: Avoid adding dozens of `@Import` beans to a slice; if a slice requires half the application graph to function, refactor the SUT or transition to a full `@SpringBootTest`.
6. **Migrate to `@MockitoBean` in Spring Boot 3.4+**: Replace deprecated `@MockBean` with `@MockitoBean` for cleaner bean override semantics.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Unscoped `@WebMvcTest` | Loads every controller in the project, requiring mocks for all services across the entire app. | Specify target: `@WebMvcTest(SpecificController.class)`. |
| Missing `@AutoConfigureTestDatabase(replace=NONE)` | Spring overrides your PostgreSQL Testcontainer with an in-memory H2 database. | Add `replace = Replace.NONE` to test against the real container. |
| Forgetting services are excluded in `@WebMvcTest` | Test fails on startup with `NoSuchBeanDefinitionException` for missing service dependencies. | Supply `@MockitoBean` for required service dependencies. |
| Expecting transactions to commit in `@DataJpaTest` | Test verifies database state, but data was rolled back automatically at method completion. | Use `@Commit` or test in a separate non-transactional integration test. |
| Testing security without importing security config | `@WebMvcTest` auto-configures default security, ignoring custom filters, OAuth2 converters, and paths. | Explicitly `@Import(WebSecurityConfig.class)`. |
| Overusing `@DataJpaTest` for trivial CRUD | Testing standard `repository.save()` tests Hibernate, not your code. | Test only custom JPQL, native queries, specifications, and auditing. |

---

## 8. Performance & Security Notes

- **Slice Boot Times**: While `@SpringBootTest` takes 2,000–5,000ms, test slices boot in 100–300ms. Running 50 slice tests saves minutes over full integration runs.
- **Context Caching with Slices**: Spring caches slice contexts too. Ten test classes annotated with `@WebMvcTest(OrderController.class)` sharing identical properties will share a single cached slice context.
- **Test Security Isolation**: `@WebMvcTest` includes Spring Security auto-configuration by default. Unsecured routes will remain guarded unless explicit `@WithMockUser` or permit rules are supplied.

---

## 9. Comparison / When to Use What

| Test Slice | Focus Layer | Included Components | Excluded Components |
|---|---|---|---|
| **`@WebMvcTest`** | Web / HTTP | `@Controller`, `@ControllerAdvice`, Jackson, MockMvc | `@Service`, `@Repository`, DataSource |
| **`@DataJpaTest`** | Data / Persistence | `@Entity`, `@Repository`, `EntityManager`, Test DB | Controllers, Services, Web Filters |
| **`@JsonTest`** | Serialization | `ObjectMapper`, `JacksonTester`, Modules | Repositories, Controllers, Services |
| **`@RestClientTest`** | HTTP Clients | `RestClient`, `RestTemplate`, `MockRestServiceServer` | Everything else |
| **`@DataRedisTest`** | Cache / NoSQL | `RedisTemplate`, Repositories | Web layer, JPA |

---

## 10. Interview Questions

<details>
<summary>1. How does Spring Boot exclude `@Service` beans from a `@WebMvcTest` slice?</summary>

**Answer:**
Spring Boot applies a specialized `TypeExcludeFilter` (specifically `WebMvcTypeExcludeFilter`) during the component scanning phase. The filter inspects candidate classes and excludes any class annotated with `@Service`, `@Repository`, or generic `@Component`, including only `@Controller`, `@ControllerAdvice`, and web-related beans.
</details>

<details>
<summary>2. Why does `@DataJpaTest` replace your configured database with H2 by default?</summary>

**Answer:**
By default, `@DataJpaTest` includes `@AutoConfigureTestDatabase`, which assumes developers want fast, zero-configuration in-memory testing. If H2 or another embedded database is on the classpath, it automatically replaces the real `DataSource`. To prevent this and test against real database containers, you must configure `@AutoConfigureTestDatabase(replace = AutoConfigureTestDatabase.Replace.NONE)`.
</details>

<details>
<summary>3. What is the difference between `@MockBean` (legacy) and `@MockitoBean` in Spring Boot 3.4+?</summary>

**Answer:**
`@MockBean` was a Spring Boot-specific annotation with complex lifecycle interactions that frequently caused context caching issues. In Spring Framework 6.2 and Spring Boot 3.4+, bean overriding was standardized into core Spring via `@MockitoBean` (and `@MockitoSpyBean`), providing clearer semantics, unified lifecycle management, and better compatibility with the TestContext Framework.
</details>

<details>
<summary>4. Why are `@DataJpaTest` methods transactional by default?</summary>

**Answer:**
To guarantee test isolation. By running each test method inside a database transaction that rolls back automatically upon completion, tests leave no residual state in the database. This prevents test pollution where one test's inserted rows cause subsequent tests to fail.
</details>

<details>
<summary>5. When should you use `@JsonTest` instead of `@WebMvcTest`?</summary>

**Answer:**
Use `@JsonTest` when you only need to verify serialization and deserialization details (such as custom Jackson serializers, date/time formatting, field naming strategies, or JSON Schema validation). `@JsonTest` boots in ~50ms without the overhead of servlet filters, controllers, or MockMvc.
</details>

<details>
<summary>6. How does `@RestClientTest` verify outbound API calls?</summary>

**Answer:**
`@RestClientTest` configures the SUT's `RestTemplate` or `RestClient` to bind to an internal `MockRestServiceServer`. The mock server intercepts outbound HTTP requests, matches them against expected URLs, HTTP methods, and headers, and returns pre-configured mock response bodies without making real network connections.
</details>

<details>
<summary>7. What components are loaded by `@WebFluxTest` compared to `@WebMvcTest`?</summary>

**Answer:**
`@WebFluxTest` loads reactive web infrastructure (Netty-based reactive controllers, functional `RouterFunction` beans, and reactive `WebFilter` instances) and provides a `WebTestClient`. `@WebMvcTest` loads traditional servlet-based MVC controllers, `Filter` beans, and provides a `MockMvc` instance.
</details>

<details>
<summary>8. How do you test validation annotations (e.g., `@Valid`, `@NotNull`) on a controller request body?</summary>

**Answer:**
Using `@WebMvcTest`. Because `@WebMvcTest` includes validation infrastructure (`LocalValidatorFactoryBean`), posting invalid JSON payloads via `MockMvc` will trigger validation failure and return HTTP 400 Bad Request, allowing you to test validation errors without booting services.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic `@WebMvcTest` Validation Check (Easy)</summary>

**Task**: Write a `@WebMvcTest` verifying that sending an invalid request body (empty name) to `POST /api/users` returns HTTP 400 Bad Request.

**Solution**:
```java
@WebMvcTest(UserController.class)
class UserControllerValidationTest {

    @Autowired
    private MockMvc mockMvc;

    @MockitoBean
    private UserService userService;

    @Test
    void shouldReturn400WhenNameIsBlank() throws Exception {
        mockMvc.perform(post("/api/users")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"name\":\"\"}")) // Invalid payload
            .andExpect(status().isBadRequest());
    }
}
```
</details>

<details>
<summary>Exercise 2: Simple `@JsonTest` DTO Serialization (Easy)</summary>

**Task**: Test that an `InvoiceDTO` object serializes its `amount` field to a JSON number formatted with two decimal places.

**Solution**:
```java
@JsonTest
class InvoiceJsonTest {

    @Autowired
    private JacksonTester<InvoiceDTO> json;

    @Test
    void shouldSerializeInvoiceAmount() throws Exception {
        InvoiceDTO dto = new InvoiceDTO("INV-1", new BigDecimal("49.99"));
        assertThat(json.write(dto))
            .hasJsonPathNumberValue("@.amount")
            .extractingJsonPathNumberValue("@.amount").isEqualTo(49.99);
    }
}
```
</details>

<details>
<summary>Exercise 3: `@DataJpaTest` with Custom Derived Query (Medium)</summary>

**Task**: Write a `@DataJpaTest` verifying that a custom repository method `findByStatusOrderByCreatedAtDesc()` returns records in the expected chronological order.

**Solution**:
```java
@DataJpaTest
class OrderRepositorySliceTest {

    @Autowired
    private OrderRepository repository;

    @Autowired
    private TestEntityManager entityManager;

    @Test
    void shouldReturnOrdersSortedByDate() {
        entityManager.persist(new Order("ORD-1", "PENDING", Instant.now().minusSeconds(100)));
        entityManager.persist(new Order("ORD-2", "PENDING", Instant.now()));
        entityManager.flush();

        List<Order> orders = repository.findByStatusOrderByCreatedAtDesc("PENDING");

        assertThat(orders).hasSize(2);
        assertThat(orders.get(0).getId()).isEqualTo("ORD-2");
    }
}
```
</details>

<details>
<summary>Exercise 4: `@RestClientTest` Error Simulation (Medium)</summary>

**Task**: Write a test using `@RestClientTest` ensuring that when the external service returns HTTP 500 Internal Server Error, the client throws a custom `ExternalServiceUnavailableException`.

**Solution**:
```java
@RestClientTest(PaymentClient.class)
class PaymentClientErrorTest {

    @Autowired
    private PaymentClient client;

    @Autowired
    private MockRestServiceServer server;

    @Test
    void shouldThrowExceptionOnExternalServerError() {
        server.expect(requestTo("https://api.payment.com/charge"))
            .andRespond(withServerError());

        assertThatThrownBy(() -> client.charge(new ChargeRequest(100)))
            .isInstanceOf(ExternalServiceUnavailableException.class);
    }
}
```
</details>

<details>
<summary>Exercise 5: Real Database Testing with `@DataJpaTest` and Testcontainers (Hard)</summary>

**Task**: Configure a `@DataJpaTest` that uses Testcontainers PostgreSQL with `@ServiceConnection` and disables embedded H2 database replacement.

**Solution**:
```java
@DataJpaTest
@Testcontainers
@AutoConfigureTestDatabase(replace = AutoConfigureTestDatabase.Replace.NONE)
class RealPostgresRepositoryTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private CustomerRepository repository;

    @Test
    void shouldPersistAndRetrieveCustomerInRealPostgres() {
        Customer customer = repository.save(new Customer("Bob", "bob@example.com"));
        assertThat(customer.getId()).isNotNull();
    }
}
```
</details>

<details>
<summary>Exercise 6: Testing WebMvc Slice with Custom Filter and Security (Hard)</summary>

**Task**: Write a `@WebMvcTest` that imports a custom `ApiKeyFilter` via `@Import` and verifies that requests without the `X-API-KEY` header receive HTTP 401 Unauthorized.

**Solution**:
```java
@WebMvcTest(SecureApiController.class)
@Import(ApiKeyFilter.class)
class SecureApiSliceTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void shouldRejectMissingApiKey() throws Exception {
        mockMvc.perform(get("/api/secure/data"))
            .andExpect(status().isUnauthorized());
    }

    @Test
    void shouldAcceptValidApiKey() throws Exception {
        mockMvc.perform(get("/api/secure/data")
                .header("X-API-KEY", "secret-test-key"))
            .andExpect(status().isOk());
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Test Slices are Focused**: Load only the specific layer under test (`@WebMvcTest`, `@DataJpaTest`, `@JsonTest`, `@RestClientTest`).
- **Use `@MockitoBean` (Boot 3.4+)**: Replaces `@MockBean` for injecting mocks into slice contexts.
- **`replace = Replace.NONE`**: Add to `@DataJpaTest` when testing against real Testcontainers rather than H2.
- **Auto-Rollback by Default**: Database slices roll back transactions after every test method.
- **Use `@Import` Sparingly**: Selectively pull in excluded beans (like security or converters) into the slice.

---

## 13. Further Reading
- [Spring Boot Testing Reference: Auto-configured Tests](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.testing.spring-boot-applications.autoconfigured-tests)
- [Spring Boot 3.4 Release Notes: MockitoBean and MockitoSpyBean](https://github.com/spring-projects/spring-boot/wiki/Spring-Boot-3.4-Release-Notes)
- [Testcontainers ServiceConnection Guide](https://java.testcontainers.org/modules/databases/postgres/)
- [Spring Framework Testing: WebMvcTest Details](https://docs.spring.io/spring-framework/reference/testing/spring-mvc-test-framework.html)
- [JacksonTester User Guide](https://docs.spring.io/spring-boot/docs/current/api/org/springframework/boot/test/json/JacksonTester.html)
