# Spring Boot Testing Recipes Cheatsheet

| Metadata | Details |
|---|---|
| **Topic** | Spring Boot Testing Recipes & Slices Cheatsheet |
| **Track** | Java Spring Boot |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | Java 21, JUnit 5, Mockito, Testcontainers |
| **Java Version** | Java 21 / 25 LTS |
| **Spring Boot** | 3.4.x / 4.x |
| **Tested With** | JUnit 5.11+, Mockito 5.14+, Testcontainers 1.20+ |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Categorized Reference](#4-syntax--categorized-reference)
5. [5 Practical Production Examples](#5-5-practical-production-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes to Avoid](#7-common-mistakes-to-avoid)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison Table: Test Slices & Approaches](#9-comparison-table-test-slices--approaches)
10. [Technical Interview Questions & Answers](#10-technical-interview-questions--answers)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading & References](#13-further-reading--references)

---

## 1. What & Why
Testing in Spring Boot balances high test coverage, speed, and real-world fidelity. Spring Boot provides specialized test slices (`@WebMvcTest`, `@DataJpaTest`) that bootstrap only specific subsets of the application context, minimizing execution time. Combined with Spring Boot 3.1+ `@ServiceConnection` and Testcontainers, developers test against real PostgreSQL and Kafka instances without brittle H2 in-memory databases or manual property wrangling.

In Spring Boot 3.4+, standard `@MockitoBean` and `@MockitoSpyBean` annotations replace legacy `@MockBean` for declarative test isolation.

---

## 2. Core Concepts
- **`@SpringBootTest`**: Full context initialization; tests complete end-to-end integration with mock or random HTTP ports.
- **Test Slices**: Focused context bootstrapping (`@WebMvcTest` for web, `@DataJpaTest` for persistence).
- **`@MockitoBean`**: Native Spring 6.2 / Boot 3.4+ annotation registering Mockito mocks directly into the `ApplicationContext`.
- **Testcontainers & `@ServiceConnection`**: Automatically discovers and binds container credentials (JDBC URL, username, password) directly to Spring Boot properties.
- **MockMvc DSL**: Fluent request builder and JSONPath assertions for controller testing.

---

## 3. Internals & Architecture
Spring TestContext Manager Lifecycle & Context Caching:

```
[JUnit 5 Test Execution]
          |
          v
+-------------------------------------------------------------+
|                 TestContextManager Engine                   |
+-------------------------------------------------------------+
          |
          | Evaluates MergedContextConfiguration (Slices, Config)
          v
+-------------------------------------------------------------+
|                     ContextCache                            |
|    - Checks if identical ApplicationContext already exists  |
|    - If HIT: Reuses cached ApplicationContext (Fast!)       |
|    - If MISS: Creates & initializes new context             |
+-------------------------------------------------------------+
          |
          | Applies TestExecutionListeners
          v
+-------------------------------------------------------------+
|         MockitoTestExecutionListener / Slices               |
|    - Injects @MockitoBean mocks into BeanFactory            |
|    - Configures Testcontainers @ServiceConnection           |
|    - Resets mocks between test methods                      |
+-------------------------------------------------------------+
          |
          v
[Execute Test Method (@Test)]
```

---

## 4. Syntax & Categorized Reference

### 4.1 Test Slice Annotations
| Annotation | Loaded Components | Excluded Components |
|---|---|---|
| `@WebMvcTest(Target.class)` | `@Controller`, `@RestController`, filters, converters | Services, repositories, components |
| `@DataJpaTest` | `@Repository`, entities, `TestEntityManager`, Flyway | Web layers, services, controllers |
| `@JsonTest` | Jackson `ObjectMapper`, serializers, deserializers | All Spring web and persistence beans |
| `@RestClientTest` | `RestClient.Builder`, `RestTemplateBuilder` | Repositories, controllers |
| `@SpringBootTest` | Complete application context | None |

### 4.2 Modern Mockito & Testcontainers Annotations
| Modern Annotation | Legacy Replacement | Usage |
|---|---|---|
| `@MockitoBean` | `@MockBean` (Deprecated 3.4+) | Replaces bean in context with Mockito mock |
| `@MockitoSpyBean` | `@SpyBean` (Deprecated 3.4+) | Wraps existing bean in Mockito spy |
| `@ServiceConnection` | `@DynamicPropertySource` | Auto-wires container properties to Spring Boot |

---

## 5. 5 Practical Production Examples

### Example 1: WebMvc Slice Test with MockMvc and @MockitoBean
```java
@WebMvcTest(OrderController.class)
class OrderControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @MockitoBean
    private OrderService orderService;

    @Test
    void shouldCreateOrderAndReturn201() throws Exception {
        UUID orderId = UUID.randomUUID();
        when(orderService.createOrder(any())).thenReturn(new OrderResponse(orderId, BigDecimal.valueOf(99.99)));

        mockMvc.perform(post("/api/v1/orders")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                        {
                            "customerId": "3fa85f64-5717-4562-b3fc-2c963f66afa6",
                            "amount": 99.99
                        }
                        """))
                .andExpect(status().isCreated())
                .andExpect(header().exists("Location"))
                .andExpect(jsonPath("$.id").value(orderId.toString()))
                .andExpect(jsonPath("$.amount").value(99.99));
    }
}
```

### Example 2: DataJpaTest with Real PostgreSQL Testcontainer & @ServiceConnection
```java
@DataJpaTest
@Testcontainers
@AutoConfigureTestDatabase(replace = AutoConfigureTestDatabase.Replace.NONE)
class CustomerRepositoryTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private CustomerRepository customerRepository;

    @Autowired
    private TestEntityManager entityManager;

    @Test
    void shouldSaveAndQueryByEmail() {
        Customer customer = new Customer("test@example.com", "John Doe");
        entityManager.persistAndFlush(customer);

        Optional<Customer> found = customerRepository.findByEmail("test@example.com");

        assertThat(found).isPresent();
        assertThat(found.get().getName()).isEqualTo("John Doe");
    }
}
```

### Example 3: Full Integration Test with Random Port & RestClient
```java
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@Testcontainers
class OrderIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @LocalServerPort
    private int port;

    private RestClient restClient;

    @BeforeEach
    void setUp() {
        this.restClient = RestClient.builder().baseUrl("http://localhost:" + port).build();
    }

    @Test
    void shouldExecuteFullCheckoutFlow() {
        ResponseEntity<OrderResponse> response = restClient.post()
                .uri("/api/v1/orders")
                .contentType(MediaType.APPLICATION_JSON)
                .body(new OrderRequest(UUID.randomUUID(), BigDecimal.valueOf(50.0)))
                .retrieve()
                .toEntity(OrderResponse.class);

        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.CREATED);
        assertThat(response.getBody()).isNotNull();
    }
}
```

### Example 4: Testing Security with @WithMockUser
```java
@WebMvcTest(AdminController.class)
class AdminControllerSecurityTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    @WithMockUser(username = "jane_doe", roles = {"USER"})
    void shouldDenyAccessToNonAdmin() throws Exception {
        mockMvc.perform(delete("/api/v1/admin/users/123"))
                .andExpect(status().isForbidden());
    }

    @Test
    @WithMockUser(username = "admin_user", roles = {"ADMIN"})
    void shouldAllowAccessToAdmin() throws Exception {
        mockMvc.perform(delete("/api/v1/admin/users/123"))
                .andExpect(status().isNoContent());
    }
}
```

### Example 5: JSON Serialization / Deserialization Slice Test
```java
@JsonTest
class OrderJsonTest {

    @Autowired
    private JacksonTester<OrderDto> json;

    @Test
    void shouldSerializeJson() throws IOException {
        OrderDto dto = new OrderDto(UUID.fromString("00000000-0000-0000-0000-000000000001"), "COMPLETED");
        JsonContent<OrderDto> result = json.write(dto);

        assertThat(result).extractingJsonPathStringValue("$.id").isEqualTo("00000000-0000-0000-0000-000000000001");
        assertThat(result).extractingJsonPathStringValue("$.status").isEqualTo("COMPLETED");
    }
}
```

---

## 6. Best Practices
1. **Prefer `@ServiceConnection` over `@DynamicPropertySource`**: Simplifies Testcontainers setup and avoids boilerplate property bindings.
2. **Favor Test Slices (`@WebMvcTest`, `@DataJpaTest`)**: Test slices boot in milliseconds compared to full `@SpringBootTest`.
3. **Use Real Containers Instead of H2**: In-memory databases hide dialect differences (JSONB, full-text search, sequences, locks).
4. **Avoid `@DirtiesContext` When Possible**: `@DirtiesContext` evicts the Spring context cache, forcing full restarts on subsequent tests.
5. **Use Spring Boot 3.4+ `@MockitoBean`**: Replaces legacy `@MockBean` cleanly within the Spring test framework.

---

## 7. Common Mistakes to Avoid

| Mistake | Consequence | Remedy |
|---|---|---|
| Using H2 for JPA tests while using Postgres in production | Production syntax fails due to un-reproduced SQL dialect discrepancies | Use Testcontainers PostgreSQL with `@ServiceConnection` |
| Overusing `@DirtiesContext` | Drastically slows down the test suite by destroying context cache | Write idempotent tests and clean data explicitly |
| Using `@SpringBootTest` for simple unit tests | Unnecessarily boots entire database, security, and web layers | Use plain JUnit 5 + Mockito without Spring runner |
| Missing `@AutoConfigureTestDatabase(replace = NONE)` | Spring replaces Postgres with embedded H2 unexpectedly | Set `replace = Replace.NONE` when using Testcontainers |
| Forgetting to reset mocks in non-slice tests | State leaks between consecutive `@Test` executions | Let `@MockitoBean` handle auto-reset or call `Mockito.reset()` |
| Misconfigured MockMvc CSRF in security tests | Returns 403 Forbidden on POST/PUT requests unexpectedly | Use `.with(csrf())` from `SecurityMockMvcRequestPostProcessors` |

---

## 8. Performance & Security Considerations
- **Test Context Caching**: Keep configuration classes identical across tests so Spring reuses the running context instance.
- **Shared Testcontainers**: Use the singleton container pattern or reusable containers (`.withReuse(true)`) to start Docker instances once per run.
- **Mock Security Tokens**: Use `@WithMockUser` or `@WithMockJwt` instead of minting real signed JWTs in unit/slice tests.

---

## 9. Comparison Table: Test Slices & Approaches

| Test Strategy | Speed | Scope | Docker Dependency | Context Caching |
|---|---|---|---|---|
| Plain Unit Test | Instant (~5ms) | Single Class | None | N/A |
| `@WebMvcTest` | Fast (~300ms) | Controller + Filters | None | Excellent |
| `@DataJpaTest` | Fast (~500ms) | JPA + Repositories | Optional (Testcontainers) | Excellent |
| `@SpringBootTest` | Slow (~3-8s) | Entire Application | Recommended | Depends on `@MockitoBean` variance |

---

## 10. Technical Interview Questions & Answers

<details>
<summary>1. How does Spring Test Context caching work and what invalidates it?</summary>
Spring caches `ApplicationContext` instances keyed by the combination of configuration classes, active profiles, and mock bean definitions. Annotating tests with `@DirtiesContext` or altering bean configurations between tests invalidates the cache, triggering costly reloads.
</details>

<details>
<summary>2. What is the advantage of `@ServiceConnection` introduced in Spring Boot 3.1?</summary>
`@ServiceConnection` automatically extracts connection metadata (host, port, credentials, database name) from a Testcontainers container and binds them to Spring Boot's standard property keys without manual `@DynamicPropertySource`.
</details>

<details>
<summary>3. Why was `@MockBean` deprecated in favor of `@MockitoBean` in Spring Boot 3.4?</summary>
`@MockitoBean` is provided directly by Spring Framework 6.2 core testing support, standardizing mock lifecycle management and context integration across all Spring projects.
</details>

<details>
<summary>4. What components are NOT loaded by `@WebMvcTest`?</summary>
`@WebMvcTest` does not load `@Service`, `@Repository`, `@Component`, or persistence layers (`EntityManager`, DataSource). Only web controllers, interceptors, and converter beans are instantiated.
</details>

<details>
<summary>5. How does `TestEntityManager` differ from `EntityManager`?</summary>
`TestEntityManager` is designed specifically for testing JPA entities, providing helper methods like `persistAndFlush()`, `persistFlushFind()`, and `getId()` without requiring manual transaction management.
</details>

<details>
<summary>6. How do you test authenticated endpoints with CSRF protection enabled?</summary>
Import `SecurityMockMvcRequestPostProcessors.csrf()` and append `.with(csrf())` to your `MockMvc.perform(...)` calls.
</details>

<details>
<summary>7. What does `@AutoConfigureTestDatabase(replace = Replace.NONE)` do?</summary>
It instructs Spring Boot not to override the configured DataSource with an embedded in-memory database (like H2), allowing the test to connect to the real Testcontainers database.
</details>

<details>
<summary>8. How do you verify JSON responses with nested arrays in MockMvc?</summary>
Use JSONPath expressions: `.andExpect(jsonPath("$.items", hasSize(3)))` and `.andExpect(jsonPath("$.items[0].name").value("Widget"))`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary>Exercise 1: Write a WebMvc controller slice test</summary>
Test a GET `/api/v1/products/{id}` endpoint verifying 200 OK and 404 Not Found using `@MockitoBean`.
</details>

<details>
<summary>Exercise 2: Setup a DataJpaTest with PostgreSQL Testcontainers</summary>
Write a test verifying an `@EntityGraph` query against a real PostgreSQL container using `@ServiceConnection`.
</details>

<details>
<summary>Exercise 3: Test role-based access control with MockMvc</summary>
Verify that an endpoint with `@PreAuthorize("hasRole('ADMIN')")` allows `ADMIN` and rejects `USER`.
</details>

<details>
<summary>Exercise 4: Test custom exception handling</summary>
Verify that triggering a `BusinessRuleViolationException` produces an RFC 9457 `ProblemDetail` with status 409.
</details>

<details>
<summary>Exercise 5: Write a JsonTest for Java 21 record serialization</summary>
Verify that Java 21 instant fields are formatted to ISO-8601 strings in JSON.
</details>

<details>
<summary>Exercise 6: Implement an end-to-end test with RestClient</summary>
Boot the application on a random port, call a POST endpoint, and assert data persistence in the database.
</details>

---

## 12. Summary & Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                   SPRING BOOT TESTING CHEAT SHEET                               |
+---------------------------------------------------------------------------------+
| FULL TEST      | @SpringBootTest(webEnvironment = RANDOM_PORT)                  |
| WEB SLICE      | @WebMvcTest(MyController.class)                                |
| DATA SLICE     | @DataJpaTest + @AutoConfigureTestDatabase(replace = NONE)      |
| MOCKS (3.4+)   | @MockitoBean private MyService myService;                      |
| CONTAINERS     | @Container @ServiceConnection static PostgreSQLContainer pg... |
| MOCKMVC        | mockMvc.perform(get("/api")).andExpect(status().isOk())        |
| SECURITY MOCK  | @WithMockUser(username = "admin", roles = {"ADMIN"})           |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References
- [Spring Boot Testing Official Documentation](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.testing)
- [Testcontainers Spring Boot Integration](https://java.testcontainers.org/supported_docker_environment/)
- [Spring Framework 6.2 MockitoBean Support](https://docs.spring.io/spring-framework/reference/testing/annotations/integration-spring/annotation-mockitobean.html)
- [MockMvc Result Matchers Reference](https://docs.spring.io/spring-framework/reference/testing/spring-mvc-test-framework.html)
