# Clean Code & Code Review Checklist for Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | Spotless, SonarQube, ArchUnit, ErrorProne |

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
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
In enterprise engineering organizations, code quality degrades over time without deliberate architectural discipline. Peer code reviews often devolve into bike-shedding over minor formatting discrepancies (tabs vs spaces, curly brace placement), while critical production hazards—such as transactions holding connections across third-party REST calls, unclosed streams, missing database indexes on foreign keys, thread-safety violations, and mass-assignment vulnerabilities—slip silently into production.

### The Evolution
Modern engineering teams use codified, automated **Production Code Review Checklists** combined with static analysis linters (Spotless, ArchUnit, ErrorProne). By automating routine formatting and style checks in CI/CD pipelines, human peer reviewers can focus 100% of their cognitive bandwidth on high-severity architectural invariants, transaction boundaries, performance bottlenecks, and security attack vectors.

### Production Relevance
A standardized Spring Boot code review checklist ensures consistent team velocity, prevents recurring production outages (OOMs, connection pool exhaustion, deadlocks), and aligns the entire engineering team with modern Java 21+ idioms (Records, Pattern Matching, Sealed Types, Virtual Threads).

---

## 2. Core Concepts

### The 6 Pillars of Spring Boot Code Reviews

```
+-----------------------------------------------------------------------------------------+
|                        SPRING BOOT CODE REVIEW PILLARS                                  |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  1. ARCHITECTURE & ENCAPSULATION                                                        |
|     - Are layers respected? (No Repository in Controller, No Web DTO in Entity)         |
|     - Are classes package-private unless explicitly designed for public consumption?    |
|                                                                                         |
|  2. CONCURRENCY & VIRTUAL THREADS (Java 21)                                             |
|     - Are synchronized blocks avoided in hot paths to prevent Virtual Thread pinning?   |
|     - Are shared states immutable or thread-safe (AtomicReference, ConcurrentHashMap)?  |
|                                                                                         |
|  3. TRANSACTIONAL INTEGRITY & DATABASE EFFICIENCY                                       |
|     - Is @Transactional confined to the Service layer?                                  |
|     - Is external network I/O (REST, S3) strictly excluded from transactions?           |
|     - Are queries free from the N+1 problem? (JOIN FETCH / EntityGraph used)            |
|                                                                                         |
|  4. SECURITY & DATA PRIVACY (OWASP)                                                     |
|     - Are inputs validated via @Valid? (Protection against Mass Assignment)             |
|     - Are sensitive credentials and PII masked from logs and exception messages?        |
|                                                                                         |
|  5. ERROR HANDLING & OBSERVABILITY                                                      |
|     - Are exceptions mapped to RFC 7807 ProblemDetail?                                  |
|     - Are distributed trace IDs propagated? Are SLF4J log levels appropriate?           |
|                                                                                         |
|  6. TESTING QUALITY & TESTCONTAINERS                                                    |
|     - Are tests using real database containers (Testcontainers) instead of in-memory H2?|
|     - Are edge cases (null, timeouts, constraint violations) covered?                   |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### Automated Static Analysis & Review Pipeline

```
[ Developer Git Push (Pull Request) ]
                 |
                 v
+-----------------------------------------------------------------------------------------+
|                           AUTOMATED CI/CD GATES (0 Human Effort)                        |
|                                                                                         |
|   1. Spotless (Code Formatting & License Header Verification)                           |
|   2. Checkstyle / ErrorProne (Static Bug Detection & Dead Code Analysis)                |
|   3. ArchUnit (Architectural Layering & Forbidden Import Verification)                  |
|   4. Testcontainers (Unit & Integration Tests with Real PostgreSQL/Kafka)               |
+-----------------------------------------------------------------------------------------+
                 |
                 v (Build Passes Green)
+-----------------------------------------------------------------------------------------+
|                           HUMAN PEER REVIEW (Cognitive Effort)                          |
|                                                                                         |
|   - Does this solve the business problem correctly?                                     |
|   - Are transaction scopes minimal?                                                     |
|   - Is there potential for high-volume performance degradation?                         |
|   - Are failure modes, fallbacks, and recovery mechanisms robust?                       |
+-----------------------------------------------------------------------------------------+
                 |
                 v
[ Merge to Main & Continuous Deployment ]
```

1. **Pre-Commit / Pre-Push**: Automated formatters (Spotless / Google Java Format) format code automatically, eliminating style discussions from code reviews.
2. **CI Gates**: Compiles with `-Werror` (warnings as errors) and runs ArchUnit checks.
3. **Peer Review**: Focuses solely on business logic correctness, transaction sizing, edge-case resilience, and security.

---

## 4. Syntax & Usage

### 1. Spotless Maven Plugin Configuration (`pom.xml`)

```xml
<plugin>
    <groupId>com.diffplug.spotless</groupId>
    <artifactId>spotless-maven-plugin</artifactId>
    <version>2.44.0</version>
    <configuration>
        <java>
            <googleJavaFormat>
                <version>1.22.0</version>
                <style>GOOGLE</style>
            </googleJavaFormat>
            <removeUnusedImports />
            <trimTrailingWhitespace />
            <endWithNewline />
        </java>
    </configuration>
    <executions>
        <execution>
            <goals><goal>check</goal></goals>
            <phase>compile</phase>
        </execution>
    </executions>
</plugin>
```

Run `mvn spotless:apply` to automatically format all files.

---

## 5. Practical Examples

### Example 1: Transactional Scope Review (Anti-Pattern vs Clean Code)

#### Anti-Pattern: External Network I/O Inside `@Transactional`
```java
// REJECT THIS PR!
@Service
public class BadOrderReviewService {

    @Autowired private OrderRepository orderRepository;
    @Autowired private PaymentGatewayClient paymentClient; // External REST client

    @Transactional // DANGER: Acquires JDBC connection before calling external network!
    public void processPayment(Long orderId) {
        Order order = orderRepository.findById(orderId).orElseThrow();

        // If Stripe hangs for 30s, the DB connection is held open for 30s!
        // 20 concurrent requests will completely EXHAUST HikariCP connection pool!
        PaymentResult res = paymentClient.charge(order.getAmount());

        order.markPaid(res.transactionId());
        orderRepository.save(order);
    }
}
```

#### Approved Clean Code: Non-Transactional External I/O
```java
// APPROVE THIS PR!
@Service
public class CleanOrderReviewService {

    private final OrderRepository orderRepository;
    private final PaymentGatewayClient paymentClient;

    public CleanOrderReviewService(OrderRepository orderRepository, PaymentGatewayClient paymentClient) {
        this.orderRepository = orderRepository;
        this.paymentClient = paymentClient;
    }

    public void processPayment(Long orderId) {
        // Step 1: Read-only transaction to load amount
        OrderSummary summary = getOrderSummary(orderId);

        // Step 2: External HTTP call outside ANY database transaction!
        PaymentResult res = paymentClient.charge(summary.amount());

        // Step 3: Fast, tightly-scoped transaction for persistence only (<5ms DB hold)
        completeOrderTransactionally(orderId, res.transactionId());
    }

    @Transactional(readOnly = true)
    public OrderSummary getOrderSummary(Long orderId) {
        return orderRepository.findSummaryById(orderId).orElseThrow();
    }

    @Transactional
    public void completeOrderTransactionally(Long orderId, String txnId) {
        Order order = orderRepository.findById(orderId).orElseThrow();
        order.markPaid(txnId);
    }
}
```

### Example 2: Eliminating Virtual Thread Pinning (`synchronized` vs `ReentrantLock`)

```java
// REJECT THIS PR: Pins Virtual Thread Carrier!
public class PinningRateLimiter {
    private int counter = 0;

    // BAD: synchronized block wrapping a blocking operation pins the OS carrier thread!
    public synchronized boolean tryAcquire() {
        if (counter > 100) {
            try {
                Thread.sleep(100); // Blocking socket/sleep pins carrier!
            } catch (InterruptedException e) { /* ... */ }
        }
        counter++;
        return true;
    }
}

// APPROVE THIS PR: Virtual-Thread Friendly Lock
public class CleanVirtualRateLimiter {
    private final ReentrantLock lock = new ReentrantLock();
    private int counter = 0;

    public boolean tryAcquire() {
        lock.lock(); // ReentrantLock unmounts Virtual Thread safely!
        try {
            if (counter > 100) {
                Thread.sleep(Duration.ofMillis(100)); // Unmounts carrier cleanly
            }
            counter++;
            return true;
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return false;
        } finally {
            lock.unlock();
        }
    }
}
```

### Example 3: JPA N+1 Detection Review

```java
// REJECT THIS PR: Classic N+1 Query Cascade
public interface BadCustomerRepository extends JpaRepository<Customer, Long> {
    // When calling customer.getOrders(), executes 1 SELECT for customer + N SELECTs for orders!
    @Query("SELECT c FROM Customer c WHERE c.active = true")
    List<Customer> findActiveCustomers();
}

// APPROVE THIS PR: Eager Join Fetch or EntityGraph
public interface CleanCustomerRepository extends JpaRepository<Customer, Long> {
    // Executes 1 single SQL JOIN query fetching customers and orders simultaneously!
    @Query("SELECT c FROM Customer c JOIN FETCH c.orders WHERE c.active = true")
    List<Customer> findActiveCustomersWithOrders();
}
```

### Example 4: Security Review - Parameter Tampering & Mass Assignment

```java
// REJECT THIS PR: Direct Entity Binding in Web Controller
@PostMapping("/users/profile")
public ResponseEntity<Void> updateProfile(@RequestBody UserEntity entity) {
    // Attacker sends: {"id": 1, "email": "evil@hacker.org", "role": "SUPERADMIN"}
    // Jackson writes "SUPERADMIN" directly into entity!
    userRepository.save(entity);
    return ResponseEntity.ok().build();
}

// APPROVE THIS PR: Explicit Whitelisted DTO Record
public record UpdateProfileRequest(
    @NotBlank @Size(max = 50) String displayName,
    @Size(max = 200) String bio
) {}

@PostMapping("/users/profile")
public ResponseEntity<Void> updateProfile(@Valid @RequestBody UpdateProfileRequest dto) {
    // Attacker cannot overwrite id, email, or role!
    userService.updateProfile(dto);
    return ResponseEntity.noContent().build();
}
```

### Example 5: ArchUnit Quality Enforcement Guard

```java
package com.example.review;

import com.tngtech.archunit.junit.AnalyzeClasses;
import com.tngtech.archunit.junit.ArchTest;
import com.tngtech.archunit.lang.ArchRule;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import static com.tngtech.archunit.lang.syntax.ArchRuleDefinition.classes;
import static com.tngtech.archunit.lang.syntax.ArchRuleDefinition.methods;

@AnalyzeClasses(packages = "com.example")
public class CodeReviewArchTest {

    @ArchTest
    public static final ArchRule services_should_be_annotated_with_service =
        classes().that().haveSimpleNameEndingWith("Service")
            .should().beAnnotatedWith(Service.class);

    @ArchTest
    public static final ArchRule transactional_must_not_be_on_controllers =
        methods().that().areDeclaredInClassesThat().resideInAPackage("..web..")
            .should().notBeAnnotatedWith(Transactional.class);
}
```

---

## 6. Best Practices

1. **Automate Formatting and Linting via Spotless in CI**: Never spend human review time debating code style. If the build fails formatting, the developer runs `mvn spotless:apply` locally.
2. **Keep Pull Requests Under 400 Lines of Code**: Studies show that review quality plummets exponentially when a PR exceeds 400 lines. Break large epics into smaller, independently testable feature-flagged PRs.
3. **Verify Constructor Injection Exclusively**: Reject any PR using field injection (`@Autowired private MyService myService;`). Require constructor injection with `final` fields.
4. **Mandate `@Transactional(readOnly = true)` for Query Methods**: Ensures query operations do not incur dirty checking snapshot penalties.
5. **Verify Exception Logging Hygiene**: Never write `log.error("Error: " + e.getMessage())` without passing the exception `e` as the final parameter, which preserves the full stacktrace.
6. **Enforce Testcontainers for Database and Messaging Tests**: Reject PRs using H2 for JPA tests. Ensure integration tests run against genuine PostgreSQL and Kafka container instances.

---

## 7. Common Mistakes & Pitfalls (The Peer Review Checklist)

| Area | What to Look For in PR (Red Flags) | Why It Fails | Practical Remedy |
|---|---|---|---|
| **Transactions** | REST/HTTP or S3 calls inside `@Transactional` | Long-lived network call holds DB connection open, starving HikariCP. | Split method: perform network I/O outside, persistence inside. |
| **Concurrency** | `synchronized` blocks in Java 21 | Pins Virtual Thread carrier thread, reducing throughput to zero. | Replace with `ReentrantLock` or `AtomicReference`. |
| **JPA / SQL** | Accessing lazy relations inside loops without `JOIN FETCH` | N+1 Query cascade executes hundreds of queries per second. | Use `@EntityGraph` or `JOIN FETCH` in JPQL. |
| **Security** | Returning `@Entity` from `@RestController` | Jackson serializes uninitialized proxies; leaks sensitive columns. | Map entity to immutable Java 21 `record` DTO. |
| **Validation** | Missing `@Valid` on `@RequestBody` parameters | Invalid or malicious inputs bypass validation constraints. | Add `@Valid` to controller parameters. |
| **Observability** | Swallowing exceptions with empty `catch` blocks | Errors fail silently; impossible to diagnose in production. | Log error with stacktrace and rethrow domain exception. |

---

## 8. Performance & Security Notes

### Performance
- **Connection Leak Prevention**: Verify that all `InputStream`, `OutputStream`, and JDBC `ResultSet` allocations use `try-with-resources`.
- **String Concatenation in Logging**: Verify developers use SLF4J parameterized logging (`log.info("Order: {}", id)`) instead of string concatenation (`log.info("Order: " + id)`), which avoids string allocations when the log level is disabled.

### Security
- **PII Scrubbing**: Ensure credit card numbers, passwords, and authorization headers are never logged to `stdout`.
- **SQL Injection Prevention**: Verify all dynamic database queries use parameterized `JdbcClient` or Spring Data derived methods, never raw string concatenation.

---

## 9. Comparison / When to Use What

| Quality Gate | Tooling | Execution Phase | What It Protects |
|---|---|---|---|
| **Formatting** | Spotless / Google Java Format | Pre-Commit / Build | Code consistency, whitespace, import ordering |
| **Static Bug Analysis**| ErrorProne, SonarQube | CI Build | Null pointers, unclosed streams, dead code |
| **Architectural Rules**| ArchUnit | JUnit Tests | Layering boundaries, forbidden cross-imports |
| **Contract Testing** | Spring Cloud Contract / Pact | Integration Tests | API contract compatibility across teams |
| **Human Peer Review** | GitHub / GitLab Pull Request | Human Gate | Business correctness, transaction scope, resilience |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. Why is field injection (`@Autowired private MyService service;`) widely condemned in modern Spring Boot code reviews?</strong></summary>

1. **Immutability**: Field injection prevents making dependencies `final`, allowing fields to be re-assigned or left null.
2. **Hidden Dependencies**: Constructor injection makes dependencies explicit. If a class has 9 constructor parameters, it clearly violates the Single Responsibility Principle (SRP). Field injection hides this bloat.
3. **Unit Testing Difficulty**: Classes with field injection cannot be instantiated in standard JUnit unit tests without using heavy Spring test contexts or fragile reflection hacks (`ReflectionTestUtils`).
4. **Circular Dependencies**: Field injection masks circular dependencies until runtime, whereas constructor injection fails fast at startup.
</details>

<details>
<summary><strong>2. How do you detect an N+1 query problem during a code review before it reaches production?</strong></summary>

Look for:
- One-to-many (`@OneToMany`) or many-to-one (`@ManyToOne`) associations marked with `FetchType.LAZY` being accessed inside a loop or during DTO mapping.
- JPQL queries without `JOIN FETCH` (e.g., `SELECT o FROM Order o` followed by accessing `o.getCustomer().getName()`).
- Automated detection: Look for tests utilizing QuickPerf or Datasource-Proxy asserting `assertSqlStatementCount(select = 1)`.
</details>

<details>
<summary><strong>3. What is Virtual Thread "Pinning" and how should reviewers catch it in Java 21 pull requests?</strong></summary>

A Virtual Thread becomes pinned to its underlying OS carrier thread if it performs a blocking operation (I/O, sleep, lock acquisition) while holding a monitor lock via a `synchronized` block or method.
When pinned:
- The carrier thread cannot yield to execute other virtual threads.
- High-concurrency throughput drops to traditional platform-thread levels.
**Reviewer Action**: Check for `synchronized(this)` or `synchronized` methods that wrap blocking network or sleep calls. Insist on replacing them with `java.util.concurrent.locks.ReentrantLock`.
</details>

<details>
<summary><strong>4. Why should `System.currentTimeMillis()` or `new Date()` never be used directly in enterprise business logic?</strong></summary>

Calling `System.currentTimeMillis()` or `new Date()` directly binds the code to the physical hardware clock of the machine running it.
This makes testing time-dependent logic (e.g., subscription expiry, trial periods, grace days) virtually impossible without complex bytecode manipulation.
**Remedy**: Always inject a `java.time.Clock` bean into the service. In unit tests, inject a `Clock.fixed(instant, zone)` to deterministically test past, present, and future time states.
</details>

<details>
<summary><strong>5. What is the danger of returning `null` collections from service methods?</strong></summary>

Returning `null` instead of an empty collection (`List.of()`, `Collections.emptyList()`) forces every caller to write defensive null checks:
`if (orders != null && !orders.isEmpty())`
If a developer forgets the check, the application crashes with `NullPointerException`.
**Rule**: Always return empty collections or `Optional<T>` for single items. Never return null collections.
</details>

<details>
<summary><strong>6. How do you review a PR to ensure it complies with OWASP Top 10 Mass Assignment rules?</strong></summary>

Inspect all `@RestController` methods accepting `@RequestBody`:
- Verify that the parameter is a dedicated Java 21 `record` or DTO containing only user-editable fields.
- If the parameter is an `@Entity` class, reject the PR immediately.
- Verify that `@Valid` is present on the `@RequestBody` parameter.
</details>

<details>
<summary><strong>7. Why should you verify that `MDC.clear()` is called in multi-threaded filters?</strong></summary>

SLF4J Mapped Diagnostic Context (MDC) binds logging context (like `traceId` or `userId`) to a `ThreadLocal`.
In container runtimes with thread pooling (Tomcat), failing to clear MDC leaves residual context on worker threads. Subsequent unrelated log messages will be tagged with the wrong user's ID or trace ID, corrupting production log analysis.
</details>

<details>
<summary><strong>8. What is the "Boolean Trap" in method design and how should it be refactored?</strong></summary>

The Boolean Trap occurs when a method takes a boolean parameter whose meaning is invisible at the call site:
`userService.updateUser(user, true, false);`
What do `true` and `false` mean? A developer reading this has no idea without opening the method definition.
**Refactoring**:
- Use descriptive enums: `userService.updateUser(user, NotificationPreference.SEND_EMAIL, SyncMode.ASYNC);`
- Or use builder objects with clear fluent methods.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Refactor Field Injection to Constructor Injection</strong></summary>

**Objective**: Refactor an anti-pattern class using `@Autowired` field injection to clean, testable constructor injection with `final` fields.

**Given**:
```java
@Service
public class UserService {
    @Autowired private UserRepository userRepository;
    @Autowired private NotificationClient notificationClient;
}
```

**Solution**:
```java
@Service
public class UserService {
    private final UserRepository userRepository;
    private final NotificationClient notificationClient;

    public UserService(UserRepository userRepository, NotificationClient notificationClient) {
        this.userRepository = userRepository;
        this.notificationClient = notificationClient;
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Fix Bad Logging Concatenation</strong></summary>

**Objective**: Refactor string concatenation in logging to efficient parameterized logging.

**Solution**:
```java
// BAD: Concatenates strings even if DEBUG is disabled
log.debug("Processing order " + orderId + " for customer " + customerId);

// GOOD: Zero allocation if DEBUG is disabled
log.debug("Processing order {} for customer {}", orderId, customerId);
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Write a Spotless Formatting Execution Script</strong></summary>

**Objective**: Create a Maven command that verifies formatting in CI and formats files locally.

**Solution**:
```bash
# In CI Pipeline (fails if unformatted):
mvn spotless:check

# In Local Developer Environment (automatically fixes all formatting):
mvn spotless:apply
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Eliminate the N+1 Query Using `@EntityGraph`</strong></summary>

**Objective**: Optimize a Spring Data JPA repository query so fetching an Author automatically eagerly fetches their Books in a single SQL query.

**Solution**:
```java
public interface AuthorRepository extends JpaRepository<Author, Long> {

    @EntityGraph(attributePaths = {"books"})
    @Query("SELECT a FROM Author a WHERE a.active = true")
    List<Author> findAllActiveAuthorsWithBooks();
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement an Automated ArchUnit Architecture Guard Suite</strong></summary>

**Objective**: Write a comprehensive ArchUnit test enforcing:
1. No field injection anywhere in the codebase.
2. `@Transactional` only allowed on methods in `@Service` classes.
3. No usage of `java.util.Date` or `Calendar` (mandating `java.time`).

**Solution**:
```java
@AnalyzeClasses(packages = "com.example")
public class ProductionHygieneTest {

    @ArchTest
    public static final ArchRule no_field_injection = 
        noFields().should().beAnnotatedWith(Autowired.class);

    @ArchTest
    public static final ArchRule no_legacy_dates = 
        noClasses().should().dependOnClassesThat().belongToAnyOf(java.util.Date.class, java.util.Calendar.class);

    @ArchTest
    public static final ArchRule transactional_only_in_service = 
        methods().that().areAnnotatedWith(Transactional.class)
            .should().beDeclaredInClassesThat().areAnnotatedWith(Service.class);
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build a Reusable Pull Request Verification Checklist</strong></summary>

**Objective**: Write a markdown checklist template (`.github/pull_request_template.md`) that enforces Spring Boot quality gates for every PR.

**Solution**:
```markdown
## Pull Request Quality Verification Checklist

### 1. Architecture & Boundaries
- [ ] No direct repository calls from controllers.
- [ ] DTO records used for all API inputs and outputs (no entities exposed).
- [ ] Constructor injection used with `final` fields (no `@Autowired` fields).

### 2. Transactions & Persistence
- [ ] No external REST/HTTP, email, or S3 network calls inside `@Transactional`.
- [ ] Queries inspected for N+1 issues (`JOIN FETCH` or `@EntityGraph` used).
- [ ] Read-only query methods annotated with `@Transactional(readOnly = true)`.

### 3. Concurrency & Performance
- [ ] No `synchronized` blocks wrapping blocking I/O (Virtual Thread safe).
- [ ] All streams, connections, and readers wrapped in `try-with-resources`.

### 4. Security & Error Handling
- [ ] `@Valid` applied on all `@RequestBody` parameters.
- [ ] No sensitive credentials, tokens, or PII printed in logs.
- [ ] Exceptions bubble to `@RestControllerAdvice` and return RFC 7807 `ProblemDetail`.
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| SPRING BOOT CODE REVIEW QUICK REFERENCE CHEAT SHEET                                                |
+----------------------------------------------------------------------------------------------------+
| Review Checkpoint   | Approve Criteria                          | Reject Immediately If...         |
+---------------------+-------------------------------------------+----------------------------------+
| Injection           | Constructor injection with final fields   | @Autowired on private fields     |
+---------------------+-------------------------------------------+----------------------------------+
| Transactions        | Tightly scoped to persistence methods     | External HTTP/S3 inside @Tx      |
+---------------------+-------------------------------------------+----------------------------------+
| DTOs vs Entities    | Java 21 Records for all web endpoints     | JPA @Entity returned from REST   |
+---------------------+-------------------------------------------+----------------------------------+
| Concurrency         | ReentrantLock / Virtual Thread safe       | synchronized wrapping I/O/sleep  |
+---------------------+-------------------------------------------+----------------------------------+
| Database Queries    | JOIN FETCH / @EntityGraph / Projections   | N+1 loops in services            |
+---------------------+-------------------------------------------+----------------------------------+
| Exceptions          | ProblemDetail (RFC 7807); traceId mapped  | HTTP 200 OK with error payload   |
+---------------------+-------------------------------------------+----------------------------------+
| Logging             | Parameterized (log.info("{}", id))        | String concatenation in logs     |
+---------------------+-------------------------------------------+----------------------------------+
| Time                | java.time.Clock injected as a bean        | System.currentTimeMillis()       |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Clean Code: A Handbook of Agile Software Craftsmanship (Robert C. Martin)](https://www.oreilly.com/library/view/clean-code-a/9780136083238/)
- [Effective Java (Joshua Bloch, 3rd Edition)](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- [Spotless Code Formatter Plugin for Maven & Gradle](https://github.com/diffplug/spotless)
- [ArchUnit User Guide: Architecture Verification as Code](https://www.archunit.org/)
- [Google Java Style Guide](https://google.github.io/styleguide/javaguide.html)
