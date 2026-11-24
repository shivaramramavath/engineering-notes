# Spring Boot Annotations Quick Reference

| Metadata | Details |
|---|---|
| **Topic** | Spring Boot Annotations Quick Reference |
| **Track** | Java Spring Boot |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | Java 21, Core Spring Framework & Spring Boot |
| **Java Version** | Java 21 / 25 LTS |
| **Spring Boot** | 3.4.x / 4.x |
| **Tested With** | Maven 3.9+, JUnit 5.11+ |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Categorized Annotation Reference](#4-syntax--categorized-annotation-reference)
5. [5 Practical Production Examples](#5-5-practical-production-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes to Avoid](#7-common-mistakes-to-avoid)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison Table: Overloaded & Competing Annotations](#9-comparison-table-overloaded--competing-annotations)
10. [Technical Interview Questions & Answers](#10-technical-interview-questions--answers)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading & References](#13-further-reading--references)

---

## 1. What & Why
Spring annotations declaratively govern bean configuration, component scanning, dependency injection, web mapping, transactions, persistence mapping, and testing. Before annotation-driven development, Spring applications relied on verbose XML descriptors. Modern Spring Boot applications rely on composite meta-annotations that reduce boilerplate, validate contracts at build time, and enable ahead-of-time (AOT) reflection hints for GraalVM native images.

A comprehensive reference card prevents annotation misapplication, incorrect lifecycle ordering, and invalid proxy interception.

---

## 2. Core Concepts
- **Stereotype Annotations**: `@Component`, `@Service`, `@Repository`, and `@Controller` categorize application layers and trigger component scanning.
- **Dependency Injection**: `@Autowired`, `@Qualifier`, and `@Primary` guide constructor/field wiring.
- **Conditional Configuration**: `@ConditionalOnProperty`, `@ConditionalOnClass`, and `@ConditionalOnMissingBean` drive modular auto-configuration.
- **Data Persistence**: Jakarta Persistence annotations (`@Entity`, `@Table`, `@Id`) alongside Spring Data (`@Query`, `@Modifying`, `@Param`).
- **Security & Authorization**: `@PreAuthorize`, `@Secured`, and `@RolesAllowed` invoke method-level security interceptors.
- **Observability & Testing**: `@Timed`, `@WebMvcTest`, `@DataJpaTest`, `@MockitoBean`.

---

## 3. Internals & Architecture
Spring annotation resolution operates through BeanPostProcessors and reflection/ASM bytecode inspection:

```
+-------------------------------------------------------------+
|               JVM Class Loading & Scanning                  |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|        ClassPathScanningCandidateComponentProvider          |
|    - Reads class metadata using ASM SimpleMetadataReader    |
|    - Evaluates @Conditional / TypeFilter rules              |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|             BeanDefinitionRegistryPostProcessor             |
|    - Registers RootBeanDefinition for matching classes      |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|           BeanPostProcessor Interception Pipeline           |
|  1. CommonAnnotationBeanPostProcessor (@PostConstruct)      |
|  2. AutowiredAnnotationBeanPostProcessor (@Autowired)      |
|  3. Advisor / Dynamic Proxy Wrappers (@Transactional, AOP)  |
+-------------------------------------------------------------+
```

---

## 4. Syntax & Categorized Annotation Reference

### 4.1 Stereotypes & Core DI
| Annotation | Target | Description |
|---|---|---|
| `@Component` | Type | Generic managed Spring bean |
| `@Service` | Type | Business layer stereotype |
| `@Repository` | Type | Persistence layer stereotype with automated exception translation |
| `@Controller` / `@RestController` | Type | MVC handler / JSON REST controller combining `@ResponseBody` |
| `@Configuration` | Type | Source of `@Bean` definitions with CGLIB proxying (`proxyBeanMethods=true`) |
| `@Autowired` | Constructor / Field | Injects matching bean by type |
| `@Qualifier("name")` | Param / Field | Disambiguates between multiple candidate beans |
| `@Primary` | Type / Method | Designates default bean when multiple candidates exist |
| `@Lazy` | Type / Param | Delays bean instantiation until first invocation |
| `@Value("${property}")` | Param / Field | Resolves environment or configuration property |

### 4.2 Web & HTTP Mapping
| Annotation | Target | Description |
|---|---|---|
| `@GetMapping`, `@PostMapping`, etc. | Method | Maps HTTP verbs to handler methods |
| `@PathVariable` | Param | Extracts URI template variable |
| `@RequestParam` | Param | Extracts HTTP query parameter or form field |
| `@RequestBody` | Param | Deserializes HTTP request body via `HttpMessageConverter` |
| `@ResponseStatus` | Method | Sets HTTP status code on normal completion |
| `@ExceptionHandler` | Method | Declares custom error handler method |
| `@RestControllerAdvice` | Type | Global cross-cutting controller interceptor and error translator |

### 4.3 Data & Transactions
| Annotation | Target | Description |
|---|---|---|
| `@Transactional` | Method / Type | Demarcates ACID transaction boundary with AOP proxy interceptor |
| `@Entity` / `@Table` | Type | Jakarta persistence entity declaration |
| `@Id` / `@GeneratedValue` | Field / Getter | Primary key mapping and sequence/identity generation |
| `@Query` | Method | Custom JPQL or native SQL repository query |
| `@Modifying` | Method | Signals DML update/delete operation in repository |
| `@EntityGraph` | Method | Specifies eager fetch join plan to prevent N+1 queries |

---

## 5. 5 Practical Production Examples

### Example 1: Constructor Injection with Disambiguation
```java
@Service
public class PaymentProcessingService {

    private final PaymentGateway primaryGateway;
    private final String merchantId;

    public PaymentProcessingService(
            @Qualifier("stripePaymentGateway") PaymentGateway primaryGateway,
            @Value("${payment.merchant-id}") String merchantId) {
        this.primaryGateway = Objects.requireNonNull(primaryGateway);
        this.merchantId = Objects.requireNonNull(merchantId);
    }

    public PaymentResult execute(PaymentCommand command) {
        return primaryGateway.charge(merchantId, command.amount(), command.token());
    }
}
```

### Example 2: Conditional Auto-Configuration Bean
```java
@Configuration(proxyBeanMethods = false)
public class AuditLogAutoConfiguration {

    @Bean
    @ConditionalOnProperty(name = "app.audit.enabled", havingValue = "true", matchIfMissing = true)
    @ConditionalOnMissingBean(AuditLogPublisher.class)
    public AuditLogPublisher kafkaAuditLogPublisher(KafkaTemplate<String, AuditEvent> kafkaTemplate) {
        return new KafkaAuditLogPublisher(kafkaTemplate);
    }
}
```

### Example 3: Declarative Validation & Web Binding
```java
@RestController
@RequestMapping("/api/v1/users")
public class UserRegistrationController {

    private final UserService userService;

    public UserRegistrationController(UserService userService) {
        this.userService = userService;
    }

    @PostMapping(consumes = MediaType.APPLICATION_JSON_VALUE, produces = MediaType.APPLICATION_JSON_VALUE)
    @ResponseStatus(HttpStatus.CREATED)
    public UserProfileResponse registerUser(@Valid @RequestBody UserRegistrationRequest request) {
        return userService.register(request);
    }
}
```

### Example 4: Custom Composed Meta-Annotation
```java
@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
@Documented
@Transactional(propagation = Propagation.SUPPORTS, readOnly = true, timeout = 5)
public @interface ReadOnlyTransaction {
}
```

### Example 5: Testing Slice with MockitoBean
```java
@WebMvcTest(UserRegistrationController.class)
class UserRegistrationControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @MockitoBean
    private UserService userService;

    @Test
    void shouldRegisterUserSuccessfully() throws Exception {
        UUID id = UUID.randomUUID();
        when(userService.register(any())).thenReturn(new UserProfileResponse(id, "alex@example.com"));

        mockMvc.perform(post("/api/v1/users")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                        {
                            "email": "alex@example.com",
                            "fullName": "Alex Rivera"
                        }
                        """))
                .andExpect(status().isCreated())
                .andExpect(jsonPath("$.id").value(id.toString()));
    }
}
```

---

## 6. Best Practices
1. **Prefer Constructor Injection**: Eliminates circular references at boot and maintains bean immutability.
2. **Set `proxyBeanMethods = false` in `@Configuration`**: Avoids CGLIB subclass generation when `@Bean` methods don't invoke each other.
3. **Use Composed Meta-Annotations**: Bundle `@Transactional(readOnly=true)` or standard security policies into reusable annotations.
4. **Avoid `@Autowired` on Fields**: Makes unit testing difficult and hides dependency coupling.
5. **Annotate Repositories with `@Repository`**: Enables automated DataAccessException translation.

---

## 7. Common Mistakes to Avoid

| Mistake | Consequence | Remedy |
|---|---|---|
| Placing `@Transactional` on a private method | Spring AOP proxy cannot intercept private invocations; transaction is silently bypassed | Mark method `public` or use AspectJ compile-time weaving |
| Self-invocation of `@Transactional` methods | Internal `this.method()` bypasses proxy; no transaction started | Inject self via `ApplicationContext` or refactor into distinct service bean |
| Using `@Autowired` on static fields | Injected after classloading; field remains `null` | Pass dependency into static utility method or inject via instance setter |
| Missing `@Modifying` on DML `@Query` | Throws `InvalidDataAccessApiUsageException` on UPDATE/DELETE | Always pair `@Modifying` with data-altering repository queries |
| Mixing `@NotNull` from wrong package | Using `com.sun.istack.NotNull` instead of `jakarta.validation.constraints.NotNull` produces no runtime validation | Use canonical `jakarta.validation.constraints` imports |
| Forgetting `proxyBeanMethods = false` in light configs | Incurs unnecessary CGLIB subclass generation and memory overhead | Explicitly set `proxyBeanMethods = false` on modular `@Configuration` classes |

---

## 8. Performance & Security Considerations
- **Reflection Overhead**: Heavy annotation inspection during startup increases warm-up time. Use Spring 3.x AOT generation for production container workloads.
- **Method Security Ordering**: Ensure `@EnableMethodSecurity` is enabled once; placing security annotations without proper advisors exposes endpoints.
- **AOP Proxy Depth**: Deep nesting of `@Transactional`, `@Async`, and `@Retryable` adds proxy call stack frames. Keep AOP chains minimal.

---

## 9. Comparison Table: Overloaded & Competing Annotations

| Feature | `@Component` | `@Service` | `@Repository` | `@Configuration` |
|---|---|---|---|---|
| **Layer** | Generic | Domain / Business | Persistence / Data | Infrastructure / Wiring |
| **Exception Translation** | No | No | Yes (`DataAccessException`) | No |
| **Proxying** | Standard JDK/CGLIB | Standard JDK/CGLIB | Standard JDK/CGLIB | CGLIB full proxy (default) |
| **AOT Hint Gen** | Basic | Basic | Repository interfaces | Full factory method metadata |

---

## 10. Technical Interview Questions & Answers

<details>
<summary>1. How does Spring detect and process annotations at runtime vs AOT?</summary>
At runtime, Spring uses ASM byte-code readers and reflection via `ClassPathBeanDefinitionScanner` to populate `BeanDefinition` metadata. In AOT mode (Spring Boot 3+), annotations are evaluated at build time by `BeanRegistrationCodeGenerator`, generating deterministic Java code that initializes the `ApplicationContext` without dynamic reflection.
</details>

<details>
<summary>2. What is the difference between `@Resource`, `@Inject`, and `@Autowired`?</summary>
`@Autowired` is Spring-specific and resolves by type with optional `@Qualifier`. `@Resource` is Jakarta EE (JSR-250) and resolves primarily by name, then by type. `@Inject` is Jakarta CDI (JSR-330) and resolves by type without `required=false` semantics.
</details>

<details>
<summary>3. Why doesn't `@Async` work when called within the same class?</summary>
Spring `@Async` uses dynamic proxies. When an internal method calls another method on `this`, it invokes the raw target instance, completely bypassing the Spring proxy advice.
</details>

<details>
<summary>4. What does `@Primary` do when used alongside `@Qualifier`?</summary>
`@Qualifier` always takes precedence over `@Primary`. `@Primary` acts as a fallback tie-breaker only when injection points do not specify a qualifier.
</details>

<details>
<summary>5. What is the effect of `@ConditionalOnMissingBean`?</summary>
It registers a bean definition only if no other bean of the matching type or name has already been registered in the `BeanFactory`, allowing application consumers to override auto-configured defaults.
</details>

<details>
<summary>6. How does `@RestController` differ from `@Controller`?</summary>
`@RestController` is a meta-annotation composed of `@Controller` and `@ResponseBody`. It automatically serializes return values into HTTP response bodies via `HttpMessageConverter` instead of resolving view templates.
</details>

<details>
<summary>7. What does `@Transactional(propagation = Propagation.REQUIRES_NEW)` guarantee?</summary>
It suspends any existing active transaction and initiates an independent physical transaction with its own database connection and commit/rollback lifecycle.
</details>

<details>
<summary>8. What replaced `@MockBean` in Spring Boot 3.4+?</summary>
Spring Framework 6.2 and Spring Boot 3.4 introduced `@MockitoBean` and `@MockitoSpyBean`, replacing the legacy `@MockBean` and `@SpyBean` annotations to standardize mock registration within Spring test contexts.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary>Exercise 1: Create a custom audit meta-annotation</summary>
Define an annotation `@AuditOperation` that combines `@Transactional(propagation = Propagation.MANDATORY)` and a custom `@Retention(RUNTIME)` marker for aspect interception.
</details>

<details>
<summary>Exercise 2: Implement a conditional caching configuration</summary>
Write a `@Configuration` class that initializes an in-memory cache if `spring.cache.type=simple` and a Redis cache if `spring.cache.type=redis`.
</details>

<details>
<summary>Exercise 3: Refactor field injection to constructor injection</summary>
Take a legacy service with 4 `@Autowired` private fields and refactor to an immutable Java 21 record or class with a single constructor.
</details>

<details>
<summary>Exercise 4: Test a controller with @WebMvcTest and @MockitoBean</summary>
Write an integration test for an order checkout endpoint validating HTTP 400 Bad Request when request validation fails.
</details>

<details>
<summary>Exercise 5: Build a custom `@Conditional` implementation</summary>
Implement a custom `Condition` class that checks if a specific environment variable exists before registering an integration client bean.
</details>

<details>
<summary>Exercise 6: Resolve a circular dependency using `@Lazy`</summary>
Simulate two interacting services (`ServiceA` and `ServiceB`), inject `@Lazy` on one constructor parameter, and verify context startup.
</details>

---

## 12. Summary & Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                   SPRING BOOT ANNOTATION CHEAT SHEET                            |
+---------------------------------------------------------------------------------+
| CORE DI       | @Component, @Service, @Repository, @Configuration, @Bean        |
| INJECTION     | @Autowired, @Qualifier("id"), @Primary, @Value("${prop}")       |
| CONDITIONS    | @ConditionalOnProperty, @ConditionalOnMissingBean               |
| REST API      | @RestController, @GetMapping, @PostMapping, @PathVariable       |
| PERSISTENCE   | @Entity, @Table, @Id, @Transactional, @Query, @Modifying        |
| TESTING       | @SpringBootTest, @WebMvcTest, @DataJpaTest, @MockitoBean        |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References
- [Spring Framework Core Annotations Reference Documentation](https://docs.spring.io/spring-framework/reference/core/beans/annotation-config.html)
- [Spring Boot Auto-configuration Conditions](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.developing-auto-configuration.condition-annotations)
- [Jakarta Persistence Specification 3.1](https://jakarta.ee/specifications/persistence/3.1/)
- [Spring Testing Reference Guide](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.testing)
