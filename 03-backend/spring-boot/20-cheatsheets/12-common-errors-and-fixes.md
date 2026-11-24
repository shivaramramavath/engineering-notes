# Spring Boot Common Errors & Fixes Cheatsheet

| Metadata | Details |
|---|---|
| **Topic** | Diagnosing Top 20 Spring Boot Runtime Exceptions & Fixes |
| **Track** | Java Spring Boot |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | Java 21, Spring Boot Core, Hibernate, Spring Security |
| **Java Version** | Java 21 / 25 LTS |
| **Spring Boot** | 3.4.x / 4.x |
| **Tested With** | Spring Boot Starter Web, JPA, Security |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Categorized Reference: The Top 20 Errors](#4-syntax--categorized-reference-the-top-20-errors)
5. [5 Practical Production Examples](#5-5-practical-production-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes to Avoid](#7-common-mistakes-to-avoid)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison Table: Exception Categories](#9-comparison-table-exception-categories)
10. [Technical Interview Questions & Answers](#10-technical-interview-questions--answers)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading & References](#13-further-reading--references)

---

## 1. What & Why
Spring Boot runtime exceptions can be daunting when nested deep inside dynamic proxy chains, reflection invocations, or auto-configuration condition evaluations. Understanding the root causes, diagnostic stack trace signatures, and exact remedies for the top 20 Spring runtime errors accelerates debugging, prevents outages, and ensures reliable production deployments.

This cheatsheet provides an immediate lookup diagnostic matrix with copy-paste solutions.

---

## 2. Core Concepts
- **Bean Lifecycle Errors**: Failures during `BeanFactory` dependency resolution, circular dependency detection, or `@PostConstruct` execution.
- **Persistence & JPA Errors**: `LazyInitializationException` from closed persistence contexts, schema mismatches, and optimistic lock collisions.
- **Web & Deserialization Errors**: JSON parsing failures, constraint violations, and missing path variables.
- **Security & Authorization Errors**: Missing credentials (`AuthenticationException`) or insufficient privileges (`AccessDeniedException`).
- **Infrastructure Errors**: Port conflicts (`PortInUseException`), database pool exhaustion, and broker timeouts.

---

## 3. Internals & Architecture
Spring Exception Translation & Handling Hierarchy:

```
[Raw Database / Network / Reflection Driver Error]
                       |
                       v
+-------------------------------------------------------------+
|             PersistenceExceptionTranslationPostProcessor    |
|   (Translates SQLException / HibernateException into        |
|    Spring's DataAccessException hierarchy)                  |
+-------------------------------------------------------------+
                       |
                       v
+-------------------------------------------------------------+
|              Spring DispatcherServlet Pipeline              |
|   - Catches unhandled runtime exceptions                    |
|   - Delegates to HandlerExceptionResolverComposite          |
+-------------------------------------------------------------+
                       |
                       v
+-------------------------------------------------------------+
|           @RestControllerAdvice Global Handler              |
|   - Matches @ExceptionHandler(SpecificException.class)      |
|   - Formats RFC 9457 ProblemDetail response                 |
|   - Logs contextual diagnostics                             |
+-------------------------------------------------------------+
```

---

## 4. Syntax & Categorized Reference: The Top 20 Errors

### 1. `BeanCreationException` / `NoSuchBeanDefinitionException`
- **Cause**: Spring cannot locate a bean matching the requested type or qualifier.
- **Fix**: Verify `@Component`, `@Service`, or `@Repository` annotation exists, package is within `@ComponentScan`, or define `@Bean`.

### 2. `NoUniqueBeanDefinitionException`
- **Cause**: Multiple candidate beans exist for a single injection target.
- **Fix**: Use `@Qualifier("specificName")` at injection point or annotate one candidate with `@Primary`.

### 3. `BeanCurrentlyInCreationException` (Circular Dependency)
- **Cause**: Service A depends on Service B, which depends on Service A (forbidden in Spring Boot 2.6+).
- **Fix**: Refactor shared logic into a distinct Service C, or use `@Lazy` on one constructor parameter as a temporary workaround.

### 4. `LazyInitializationException`
- **Cause**: Accessing an uninitialized `FetchType.LAZY` association outside of an active transaction.
- **Fix**: Use `@EntityGraph(attributePaths = {...})` or JPQL `JOIN FETCH` to load associations in the repository query.

### 5. `HttpMessageNotReadableException`
- **Cause**: Malformed JSON payload or type mismatch during Jackson deserialization (e.g. passing `"abc"` to an `Integer`).
- **Fix**: Validate incoming JSON against DTO structure; customize Jackson deserializers for proprietary types.

### 6. `MethodArgumentNotValidException`
- **Cause**: Incoming `@RequestBody` payload failed `@Valid` JSR-380 constraints.
- **Fix**: Handle in `@RestControllerAdvice` and extract `BindingResult.getFieldErrors()` into RFC 9457 response.

### 7. `DataIntegrityViolationException`
- **Cause**: Database constraint violation (foreign key failure, unique index violation, NOT NULL violation).
- **Fix**: Validate entity state prior to persistence; catch and translate to HTTP 409 Conflict.

### 8. `CannotCreateTransactionException`
- **Cause**: Database unreachable or HikariCP connection pool exhausted.
- **Fix**: Verify database connection URL, network security groups, and increase `hikari.maximum-pool-size`.

### 9. `TransactionSystemException` / `RollbackException`
- **Cause**: Transaction failed during commit phase (often JPA entity validation triggered at flush time).
- **Fix**: Inspect root cause: ensure entity constraints (`@NotNull`) match database schema and DTO validations.

### 10. `AccessDeniedException`
- **Cause**: Authenticated principal lacks necessary authorities for `@PreAuthorize` or request matcher.
- **Fix**: Verify user roles match `ROLE_` prefix expectations and JWT claims converter maps authorities accurately.

### 11. `AuthenticationException` / `BadCredentialsException`
- **Cause**: Missing, expired, or invalid credentials/token.
- **Fix**: Return HTTP 401 Unauthorized via custom `AuthenticationEntryPoint`.

### 12. `PortInUseException`
- **Cause**: Configured port (e.g. 8080) is already bound by another process.
- **Fix**: Set `server.port=0` (random) or kill conflicting process: `netstat -ano | findstr :8080`.

### 13. `PropertyReferenceException`
- **Cause**: Spring Data derived query method references an entity property that does not exist.
- **Fix**: Ensure method name matches entity field names (`findByUserEmail` vs `findByUser_Email`).

### 14. `TransientPropertyValueException`
- **Cause**: Saving an entity referencing a transient (unsaved) related entity.
- **Fix**: Add `cascade = CascadeType.PERSIST` to association or save the related entity before the parent.

### 15. `OptimisticLockingFailureException` / `ObjectOptimisticLockingFailureException`
- **Cause**: Entity `@Version` field updated concurrently by another transaction.
- **Fix**: Catch exception and retry operation, or inform client to re-fetch and re-apply changes.

### 16. `QueryException` / `IllegalArgumentException` in JPQL
- **Cause**: Syntax error or unmapped token in `@Query` string.
- **Fix**: Validate JPQL syntax against entity names and properties (case sensitive), not database table names.

### 17. `CallNotPermittedException` (Resilience4j)
- **Cause**: Circuit Breaker is in `OPEN` state due to previous high downstream failure rates.
- **Fix**: Provide fallback method (`fallbackMethod = "..."`) and inspect downstream service health.

### 18. `ListenerExecutionFailedException` (Kafka/RabbitMQ)
- **Cause**: Uncaught exception inside `@KafkaListener` or `@RabbitListener`.
- **Fix**: Configure `DefaultErrorHandler` with exponential backoff and dead-letter publishing recoverer.

### 19. `MappingException` (Jackson / MongoDB / Redis)
- **Cause**: Failure converting data types into domain models (missing no-arg constructor or incompatible property types).
- **Fix**: Ensure domain models provide appropriate constructors or Jackson `@JsonCreator` annotations.

### 20. `IllegalStateException: Cannot call sendError() after the response has been committed`
- **Cause**: Controller or filter attempted to modify response status/headers after response stream already started flushing.
- **Fix**: Avoid writing directly to `HttpServletResponse.getOutputStream()` before throwing exceptions.

---

## 5. 5 Practical Production Examples

### Example 1: Handling `MethodArgumentNotValidException` with RFC 9457
```java
@ExceptionHandler(MethodArgumentNotValidException.class)
public ProblemDetail handleValidationErrors(MethodArgumentNotValidException ex) {
    ProblemDetail problem = ProblemDetail.forStatusAndDetail(HttpStatus.BAD_REQUEST, "Validation failed");
    problem.setTitle("Invalid Request Content");

    Map<String, String> invalidFields = ex.getBindingResult().getFieldErrors().stream()
            .collect(Collectors.toMap(
                    FieldError::getField,
                    fe -> fe.getDefaultMessage() != null ? fe.getDefaultMessage() : "Invalid",
                    (msg1, msg2) -> msg1 + "; " + msg2));

    problem.setProperty("invalidFields", invalidFields);
    return problem;
}
```

### Example 2: Resolving Circular Dependency with `@Lazy`
```java
@Service
public class OrderService {

    private final BillingService billingService;

    // BillingService depends on OrderService; resolve circularity cleanly
    public OrderService(@Lazy BillingService billingService) {
        this.billingService = billingService;
    }
}
```

### Example 3: Solving `LazyInitializationException` via EntityGraph
```java
public interface UserRepository extends JpaRepository<User, UUID> {

    // Prevents LazyInitializationException when accessing user.getRoles() outside transaction
    @EntityGraph(attributePaths = {"roles"})
    Optional<User> findWithRolesByEmail(String email);
}
```

### Example 4: Handling `OptimisticLockingFailureException` with Spring Retry
```java
@Service
public class InventoryService {

    private final InventoryRepository inventoryRepository;

    public InventoryService(InventoryRepository inventoryRepository) {
        this.inventoryRepository = inventoryRepository;
    }

    @Retryable(
        retryFor = {OptimisticLockingFailureException.class},
        maxAttempts = 3,
        backoff = @Backoff(delay = 100, multiplier = 2.0)
    )
    @Transactional
    public void deductStock(UUID productId, int quantity) {
        Inventory inventory = inventoryRepository.findById(productId).orElseThrow();
        inventory.decreaseQuantity(quantity);
        inventoryRepository.save(inventory);
    }
}
```

### Example 5: Resolving `NoUniqueBeanDefinitionException` via `@Qualifier`
```java
@Service
public class NotificationRouter {

    private final MessageSender emailSender;
    private final MessageSender smsSender;

    public NotificationRouter(
            @Qualifier("emailSender") MessageSender emailSender,
            @Qualifier("smsSender") MessageSender smsSender) {
        this.emailSender = emailSender;
        this.smsSender = smsSender;
    }
}
```

---

## 6. Best Practices
1. **Never Swallow Root Causes**: Always inspect `ex.getCause()` when diagnosing `BeanCreationException` or `TransactionSystemException`.
2. **Translate Persistence Exceptions**: Catch `DataIntegrityViolationException` and return clean 409 Conflict rather than 500 Internal Error.
3. **Use EntityGraphs Over Open-In-View**: Completely avoid `LazyInitializationException` without leaking DB connections into web controllers.
4. **Retry Optimistic Locks**: Transient optimistic locking collisions should be retried automatically via Spring Retry.
5. **Handle Validation Cleanly**: Provide field-level validation feedback via standardized RFC 9457 Problem Details.

---

## 7. Common Mistakes to Avoid

| Mistake | Consequence | Remedy |
|---|---|---|
| Enabling `open-in-view: true` to avoid `LazyInitializationException` | Exhausts connection pool and masks N+1 query performance degradation | Use `@EntityGraph` or DTO projections |
| Ignoring circular dependency warnings | Application fails to boot in modern Spring Boot | Refactor architectural design or extract shared dependencies |
| Exposing raw database SQL errors to API consumers | Discloses internal database schemas, table names, and security flaws | Intercept and mask exceptions in `@RestControllerAdvice` |
| Using `@Primary` everywhere to resolve `NoUniqueBeanDefinitionException` | Masks configuration ambiguities; unpredictable bean injection | Use explicit `@Qualifier` names |
| Manually writing to `HttpServletResponse` in exception handlers | Results in `Cannot call sendError() after response has been committed` | Let Spring MVC return `ResponseEntity` or `ProblemDetail` |
| Catching generic `Throwable` in business code | Traps JVM errors (`OutOfMemoryError`) and prevents graceful shutdown | Catch specific checked or business exception types |

---

## 8. Performance & Security Considerations
- **Stack Trace Leakage**: Never return raw Java exception stack traces in production API responses. Set `server.error.include-stacktrace=never`.
- **Exception Creation Overhead**: Throwing exceptions on normal business flows incurs stack trace generation overhead. Use domain result types for predictable branching.
- **HikariCP Leak Detection**: Configure `spring.datasource.hikari.leak-detection-threshold=3000` to catch unclosed connections before pool exhaustion.

---

## 9. Comparison Table: Exception Categories

| Category | Typical Status Code | Root Cause | Handling Strategy |
|---|---|---|---|
| Bean Definition | Startup Crash | Configuration/Scanning | Fix wiring, add `@Qualifier` or `@ComponentScan` |
| Validation | 400 Bad Request | Invalid Client Input | `@RestControllerAdvice` + field error formatting |
| Authentication | 401 Unauthorized | Missing/Invalid Token | `AuthenticationEntryPoint` |
| Authorization | 403 Forbidden | Insufficient Roles/Scopes | `AccessDeniedHandler` |
| Conflict | 409 Conflict | Unique constraint / Version clash | Retry (optimistic) or return conflict message |
| Infrastructure | 503 Service Unavailable | Database / Broker down | Circuit breaker fallback + alert |

---

## 10. Technical Interview Questions & Answers

<details>
<summary>1. What causes a `LazyInitializationException` and how do you fix it without Open-In-View?</summary>
It occurs when accessing a lazily-loaded JPA association outside an open Hibernate Session. Fix it by fetching required data within the transaction using `JOIN FETCH` in JPQL or declaring an `@EntityGraph` on the repository method.
</details>

<details>
<summary>2. Why does Spring Boot 2.6+ disable circular dependencies by default?</summary>
Circular dependencies often indicate poor architectural design and high coupling, and they complicate bean lifecycle initialization and proxy creation. Disabling them forces cleaner decoupling.
</details>

<details>
<summary>3. How does Spring translate vendor-specific SQL exceptions into DataAccessException?</summary>
Via `PersistenceExceptionTranslationPostProcessor` and `SQLErrorCodeSQLExceptionTranslator`, which parse vendor-specific error codes and map them into Spring's consistent `DataAccessException` hierarchy.
</details>

<details>
<summary>4. What is the difference between `NoSuchBeanDefinitionException` and `NoUniqueBeanDefinitionException`?</summary>
`NoSuchBeanDefinitionException` occurs when 0 matching beans exist. `NoUniqueBeanDefinitionException` occurs when 2 or more candidate beans match an injection point without disambiguation (`@Qualifier`/`@Primary`).
</details>

<details>
<summary>5. How should `OptimisticLockingFailureException` be handled in high-concurrency applications?</summary>
Configure `@Retryable(retryFor = OptimisticLockingFailureException.class)` with exponential backoff so transient collisions automatically reload the latest state and retry the update.
</details>

<details>
<summary>6. What is the danger of returning stack traces in REST API error responses?</summary>
Stack traces expose internal class names, third-party library versions, SQL schemas, and database file paths, providing attackers valuable reconnaissance for exploits.
</details>

<details>
<summary>7. How do you identify the root cause of a nested `BeanCreationException`?</summary>
Inspect the bottom-most `Caused by:` in the stack trace, or run Spring Boot with `--debug` to output the ConditionEvaluationReport.
</details>

<details>
<summary>8. What causes `HttpMessageNotReadableException`?</summary>
Jackson fails to deserialize the HTTP request body due to invalid JSON syntax, unmapped enumeration values, or type mismatches (e.g. passing a string to a numeric field).
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary>Exercise 1: Reproduce and fix a circular dependency</summary>
Create two mutually dependent services and resolve the cycle by introducing a shared intermediary bean.
</details>

<details>
<summary>Exercise 2: Fix LazyInitializationException in a test</summary>
Write a test reproducing `LazyInitializationException` on a detached entity, then fix it using `@EntityGraph`.
</details>

<details>
<summary>Exercise 3: Build an RFC 9457 exception handler</summary>
Write a `@RestControllerAdvice` translating `MethodArgumentNotValidException` into field error details.
</details>

<details>
<summary>Exercise 4: Implement optimistic lock retry</summary>
Simulate concurrent updates on a product entity and use `@Retryable` to handle `OptimisticLockingFailureException`.
</details>

<details>
<summary>Exercise 5: Handle DataIntegrityViolationException</summary>
Catch duplicate email insertion errors and return a clean HTTP 409 Conflict response.
</details>

<details>
<summary>Exercise 6: Resolve NoUniqueBeanDefinitionException</summary>
Define two implementations of a `PaymentGateway` interface and inject both cleanly using `@Qualifier`.
</details>

---

## 12. Summary & Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                    COMMON SPRING BOOT ERRORS CHEAT SHEET                        |
+---------------------------------------------------------------------------------+
| NO SUCH BEAN   | Add @Service/@Component or check @ComponentScan package       |
| NOT UNIQUE     | Use @Qualifier("name") or @Primary                             |
| CIRCULAR DEP   | Refactor shared logic to Service C, or use @Lazy               |
| LAZY INIT EX   | Use @EntityGraph(attributePaths = {...}) or JOIN FETCH         |
| VALIDATION EX  | Handle MethodArgumentNotValidException -> ProblemDetail        |
| INTEGRITY ERR  | Catch DataIntegrityViolationException -> HTTP 409 Conflict     |
| OPTIMISTIC LOCK| Apply @Retryable(retryFor = OptimisticLockingFailureException) |
| PORT CONFLICT  | server.port=0 (random) or kill process using port              |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References
- [Spring Boot Common Application Problems](https://docs.spring.io/spring-boot/docs/current/reference/html/howto.html)
- [Spring DataAccessException Hierarchy](https://docs.spring.io/spring-framework/reference/data-access/dao.html#dao-exceptions)
- [RFC 9457: Problem Details for HTTP APIs](https://www.rfc-editor.org/rfc/rfc9457.html)
- [Vlad Mihalcea: The Best Way to Handle LazyInitializationException](https://vladmihalcea.com/the-best-way-to-handle-the-lazyinitializationexception/)
