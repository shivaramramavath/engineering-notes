# 16. Flaky Tests: Root Causes, Detection & Remediation in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-testing-fundamentals-in-spring.md`, `08-testing-transactions-and-database-state.md`, `12-testing-async-and-scheduled-code.md` |
| **Estimated Reading Time** | 35 minutes |
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
A **flaky test** is an automated test that exhibits non-deterministic behavior: it passes or fails intermittently on the exact same commit hash without any changes to the codebase.

Flaky tests inflict devastating organizational damage on engineering teams:
- **Erosion of Trust**: Developers stop treating test failures as real regressions, dismissing them as "just that flaky test again" and re-running the CI pipeline until it turns green by chance. Real bugs slip into production unnoticed.
- **Wasted Developer Time**: Engineers spend hours re-running pipelines, diagnosing false positives, and waiting for blocked merge queues.
- **Velocity Collapse**: Teams hesitate to deploy frequently because CI pipelines are unpredictable.

Eliminating test flakiness requires understanding the **top root causes** in the Spring Boot ecosystem—such as async race conditions, unrolled database transactions, system clock drift, and port collisions—and applying systematic remediation patterns.

### Historical Evolution
- **Early Java Testing**: Flakiness was commonly resolved by re-running failed tests automatically (`rerunFailingTestsCount`), sweeping the underlying defects under the rug.
- **Modern CI/CD (Google/Meta Research, 2016-Present)**: Industry research proved that test retries mask severe concurrency and architectural bugs. Organizations established strict **Quarantine Policies** and deterministic testing standards.
- **Spring Boot 3.x/4.x**: Java 21 Virtual Threads, Awaitility, Testcontainers `@ServiceConnection`, and pluggable `Clock` beans provide the foundation for 100% deterministic test suites.

### Real-World Relevance
Zero-flakiness test suites are the prerequisite for True Continuous Deployment, where commits automatically deploy to production upon passing CI without manual human intervention.

---

## 2. Core Concepts: The Top 7 Causes of Flakiness in Spring Boot

| Rank | Root Cause | Description | Systematic Fix |
|---|---|---|---|
| **#1** | **Async Timing & Race Conditions** | Using `Thread.sleep()` or asserting before background threads finish | Replace with `Awaitility.await().untilAsserted(...)` |
| **#2** | **Test Order Dependency** | Test $A$ leaves data in the database or cache that breaks Test $B$ | Enforce `@Transactional` rollback or isolate test data |
| **#3** | **Timezone & System Clock Drift** | Hardcoding `Instant.now()` or assuming UTC in local machine tests | Inject a mockable `java.time.Clock` bean across all services |
| **#4** | **Hardcoded Network Ports** | Two tests bind port 8080/5432 simultaneously in parallel runs | Use dynamic ports: `RANDOM_PORT`, Testcontainers |
| **#5** | **Shared Mutable Static State** | Singletons or static variables modified by one test bleed into another | Avoid static state; reset explicitly in `@AfterEach` |
| **#6** | **Unpinned Container/Dependency Tags** | Using `postgres:latest` pulls upstream changes that break queries | Pin exact semantic tags: `postgres:16.4-alpine` |
| **#7** | **Non-Deterministic Collections** | Asserting order on `HashSet` or queries without `ORDER BY` | Enforce explicit SQL sorting and use `containsExactlyInAnyOrder()` |

---

## 3. How It Works Internally: Clock Drift & Deterministic Time

### The System Clock Trap
When a service relies on direct system time:
```java
// Production Code:
public boolean isCouponValid(Coupon coupon) {
    return Instant.now().isBefore(coupon.getExpiresAt());
}
```
If a test creates a coupon expiring in 10 milliseconds:
- On a developer's fast MacBook, the test executes in 2ms $\implies$ **PASSES**.
- On an overloaded shared CI runner with CPU throttling, JVM initialization takes 15ms $\implies$ **FAILS**!

### The Solution: Injectable `java.time.Clock`
Spring Boot services must inject `java.time.Clock`:
```java
@Service
public class CouponService {
    private final Clock clock; // Injectable!

    public CouponService(Clock clock) {
        this.clock = clock;
    }

    public boolean isCouponValid(Coupon coupon) {
        return clock.instant().isBefore(coupon.getExpiresAt());
    }
}
```
In tests, freeze time deterministically using `Clock.fixed(instant, zone)`:
```java
Clock fixedClock = Clock.fixed(Instant.parse("2026-11-24T12:00:00Z"), ZoneOffset.UTC);
CouponService service = new CouponService(fixedClock);
// Time is frozen! Zero flakiness regardless of CI runner speed!
```

---

## 4. Syntax & Usage

### 1. Surefire Test Quarantine Configuration (`pom.xml`)
Quarantining flaky tests by tagging them with `@Tag("flaky")` and excluding them from mandatory PR merge gates while running them in a non-blocking diagnostic build.

```xml
<plugin>
    <groupId>org.apache.maven.plugins</groupId>
    <artifactId>maven-surefire-plugin</artifactId>
    <version>3.5.0</version>
    <configuration>
        <!-- Exclude quarantined flaky tests from blocking the main PR build -->
        <excludedGroups>flaky</excludedGroups>
        <!-- Limit retries: Only retry once to capture diagnostic logs, NEVER to hide bugs! -->
        <rerunFailingTestsCount>1</rerunFailingTestsCount>
    </configuration>
</plugin>
```

---

## 5. Practical Examples

### Example 1: Eliminating Collection Ordering Flakiness
Asserting on collections returned without explicit sorting guarantees.

```java
// Production Repository:
public List<User> findActiveUsers() {
    // Notice: NO 'ORDER BY' clause! Database engine order is non-deterministic!
    return jdbcTemplate.query("SELECT * FROM users WHERE active = true", rowMapper);
}
```

```java
// FLAKY TEST:
@Test
void shouldReturnActiveUsersFlaky() {
    List<User> users = repository.findActiveUsers();
    // FAILS intermittently if the database returns [Bob, Alice] instead of [Alice, Bob]!
    assertThat(users).extracting(User::getName).containsExactly("Alice", "Bob");
}

// DETERMINISTIC TEST (FIXED):
@Test
void shouldReturnActiveUsersDeterministic() {
    List<User> users = repository.findActiveUsers();
    // Passes regardless of database row order!
    assertThat(users).extracting(User::getName).containsExactlyInAnyOrder("Alice", "Bob");
}
```

### Example 2: Eliminating Timezone Flakiness
Tests that pass in London (UTC) but fail in New York (EST) or Tokyo (JST).

```java
package com.example.time;

import org.junit.jupiter.api.Test;

import java.time.Clock;
import java.time.Instant;
import java.time.LocalDate;
import java.time.ZoneId;

import static org.assertj.core.api.Assertions.assertThat;

class TimezoneDeterministicTest {

    @Test
    void shouldCalculateBillingDateConsistentlyAcrossAllTimezones() {
        // Freeze clock at a specific UTC instant
        Instant fixedInstant = Instant.parse("2026-11-24T23:30:00Z");
        Clock fixedClock = Clock.fixed(fixedInstant, ZoneId.of("UTC"));

        BillingCalculator calculator = new BillingCalculator(fixedClock);

        LocalDate billingDate = calculator.calculateCurrentBillingCycleDate();

        // Always 2026-11-24, regardless of whether the CI agent runs in Tokyo or California!
        assertThat(billingDate).isEqualTo(LocalDate.of(2026, 11, 24));
    }
}
```

### Example 3: Eliminating Async Race Condition Flakiness with Awaitility
Replacing fragile `Thread.sleep()` with dynamic assertion polling.

```java
package com.example.async;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;

import java.time.Duration;

import static org.assertj.core.api.Assertions.assertThat;
import static org.awaitility.Awaitility.await;

@SpringBootTest
class AsyncWorkerTest {

    @Autowired
    private AsyncWorker worker;

    @Autowired
    private JobRepository jobRepository;

    @Test
    void shouldCompleteJobAsynchronouslyWithoutFlakiness() {
        String jobId = worker.submitJob("PROCESS_DATA");

        // Awaitility dynamically polls until condition is met or 5s timeout expires
        await().atMost(Duration.ofSeconds(5))
            .pollInterval(Duration.ofMillis(50))
            .untilAsserted(() -> {
                Job job = jobRepository.findById(jobId).orElseThrow();
                assertThat(job.getStatus()).isEqualTo("SUCCESS");
            });
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Sharing mutable static variables across tests in parallel test runs.

```java
// SEVERE FLAKINESS ANTI-PATTERN:
public class SharedStateService {
    public static int executionCount = 0; // Shared across all tests!
}

@Test
void testA() {
    SharedStateService.executionCount++;
    assertThat(SharedStateService.executionCount).isEqualTo(1); // FAILS if testB runs concurrently!
}

@Test
void testB() {
    SharedStateService.executionCount++;
    assertThat(SharedStateService.executionCount).isEqualTo(1); // FAILS!
}
```

**Production Fix**:
Eliminate static mutable fields entirely, or use instance-scoped beans managed by Spring or test fixtures.

```java
// DETERMINISTIC: Thread-safe and instance-scoped
class IsolatedStateTest {
    private int executionCount = 0; // Local to test instance!

    @Test
    void testA() {
        executionCount++;
        assertThat(executionCount).isEqualTo(1);
    }
}
```

### Example 5: Systematic Quarantine Pattern via `@Tag("flaky")`
Quarantining an intermittently failing test until root cause analysis is complete.

```java
package com.example.testing;

import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Test;

class PaymentGatewayFlakyIntegrationTest {

    @Test
    @Tag("flaky") // Quarantined: Won't block the main CI build gate!
    void testThirdPartyWebhookUnderLoad() {
        // Known flaky test awaiting fix in JIRA-4092
    }
}
```

---

## 6. Best Practices

1. **Inject `java.time.Clock` Everywhere**: Never call `Instant.now()`, `LocalDate.now()`, or `new Date()` directly in domain services; inject a `Clock` bean to allow frozen test times.
2. **Never Use `Thread.sleep()`**: Always synchronize async tasks using Awaitility's `untilAsserted()`.
3. **Use Dynamic Random Ports**: Always configure `webEnvironment = RANDOM_PORT` and let Testcontainers assign dynamic host ports to prevent port collisions.
4. **Assert Collections Independently of Order**: Use `containsExactlyInAnyOrder()` unless the SQL query explicitly includes an `ORDER BY` clause.
5. **Pin Exact Docker Image Versions**: Never use `latest` tags in Testcontainers; pin exact versions (e.g., `postgres:16.4-alpine`).
6. **Quarantine Flaky Tests Immediately**: Tag flaky tests with `@Tag("flaky")` so they do not block main developer PRs while root-cause investigation is underway.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Relying on `Thread.sleep()` | Fails intermittently when CI agent experiences CPU throttling. | Use `Awaitility.await().untilAsserted(...)`. |
| Unpinned Docker `latest` tag | Overnight Docker image rebuild breaks tests unexpectedly. | Pin exact tags: `redis:7.2.4-alpine`. |
| Hardcoded local timezone assumption | Passes locally, fails on CI server configured to UTC. | Inject `Clock.fixed(...)` with explicit UTC zone. |
| Missing `ORDER BY` in queries | Database engine returns rows in non-deterministic order. | Add `ORDER BY` or assert with `containsExactlyInAnyOrder()`. |
| Using auto-retries to ignore flakiness | Retries hide real concurrency bugs and race conditions. | Treat flakiness as a production bug; quarantine and fix root cause. |
| Shared mutable static singletons | Tests running in parallel corrupt each other's state. | Eliminate mutable static variables. |

---

## 8. Performance & Security Notes

- **The Cost of Test Retries**: Automatically retrying failed tests 3 times multiplies CI compute costs and masks real concurrency race conditions that will eventually crash production under load.
- **Clock Mocking Security**: Injecting fixed clocks allows engineers to test edge-case certificate expirations, token expiration timestamps (`exp`), and leap-year bugs safely.
- **Virtual Threads Integration**: Eliminating `Thread.sleep()` in favor of Awaitility ensures Java 21 Virtual Threads unmount cleanly without carrier thread starvation.

---

## 9. Comparison / When to Use What

| Technique | Solves Which Flakiness? | Implementation Complexity | Impact on Test Speed |
|---|---|---|---|
| **Awaitility** | Async timing & race conditions | Low | **Massive speedup vs sleep** |
| **Clock Injection** | Timezone and calendar drift | Low | Instant (Deterministic) |
| **`containsExactlyInAnyOrder`** | Non-deterministic SQL order | Low | Instant |
| **Dynamic Ports (`RANDOM_PORT`)**| Socket port collisions in parallel runs | Low | Eliminates bind errors |
| **Quarantine Tagging (`@Tag("flaky")`)**| CI build blockages during diagnosis | Low | Keeps master green |

---

## 10. Interview Questions

<details>
<summary>1. What are the three most common root causes of flaky tests in Spring Boot applications?</summary>

**Answer:**
1. **Asynchronous Race Conditions**: Using `Thread.sleep()` instead of polling libraries like Awaitility, causing tests to assert before background tasks complete.
2. **Test Order Dependencies & Shared State**: Tests leaving unrolled database rows, dirty Redis caches, or modified static singletons that break subsequent tests.
3. **Time and Timezone Assumptions**: Hardcoding `Instant.now()` or assuming the JVM runs in a specific local timezone rather than UTC.
</details>

<details>
<summary>2. Why is configuring automated test retries (`rerunFailingTestsCount`) considered a harmful practice if used permanently?</summary>

**Answer:**
Automated retries treat the symptom rather than the disease. They mask severe underlying defects (such as thread safety issues, race conditions, memory leaks, and unrolled transactions) that will manifest in production under high concurrency. Retries also lengthen CI build times and encourage developers to ignore test suite health.
</details>

<details>
<summary>3. How does injecting `java.time.Clock` eliminate time-based test flakiness?</summary>

**Answer:**
By replacing direct calls to `Instant.now()` or `LocalDate.now()` with `clock.instant()`, production services decouple themselves from the hardware system clock. In unit and integration tests, developers can supply a `Clock.fixed(instant, zone)` bean, effectively freezing time at a known, predictable moment. Tests become 100% deterministic regardless of execution duration, timezones, or leap years.
</details>

<details>
<summary>4. What is a Test Quarantine Strategy?</summary>

**Answer:**
A quarantine strategy is an engineering policy where any test identified as flaky is immediately tagged (e.g., `@Tag("flaky")`) and excluded from blocking the main pull request merge build. The quarantined tests are executed in a secondary non-blocking diagnostic CI job. A bug ticket is created, and the test is fixed and verified before being admitted back into the primary build pipeline.
</details>

<details>
<summary>5. How can database test flakiness occur even when `@Transactional` is used?</summary>

**Answer:**
1. The test runs against an embedded server (`webEnvironment = RANDOM_PORT`), meaning the HTTP request runs on a separate worker thread that does not participate in the test's rollback transaction.
2. The code uses database sequences or auto-increment IDs; sequences do not roll back, causing tests that assert on hardcoded IDs (e.g., `id == 1`) to fail.
3. In-memory caches (Redis/Caffeine) populated during the transaction do not roll back with the database.
</details>

<details>
<summary>6. Why should you avoid using `containsExactly()` on collection assertions unless the query has an `ORDER BY`?</summary>

**Answer:**
Relational databases do not guarantee any specific row order unless an explicit `ORDER BY` clause is present in the SQL query. Depending on database index usage, page fragmentation, or parallel scan worker allocation, rows may return in different sequences. Using `containsExactly()` asserts strict index ordering and will fail intermittently. Use `containsExactlyInAnyOrder()` instead.
</details>

<details>
<summary>7. How does parallel test execution trigger port collision flakiness?</summary>

**Answer:**
When JUnit 5 parallel test execution is enabled across multiple CPU cores, if two test classes hardcode the same server port (e.g., `server.port=8080`) or container port (e.g., PostgreSQL port 5432), one test will succeed and the other will fail with `java.net.BindException: Address already in use`. The fix is to use dynamic ports (`RANDOM_PORT` and Testcontainers dynamic port mapping).
</details>

<details>
<summary>8. How do you reproduce a flaky test that only fails intermittently in CI?</summary>

**Answer:**
1. Run the test in an endless loop locally using tools like `@RepeatedTest(100)` or Maven command line `mvn test -Dtest=MyTest -Dsurefire.rerunFailingTestsCount=0`.
2. Simulate CI environment constraints: run tests inside a Docker container with CPU limits (`--cpus=1`) and memory constraints (`--memory=1g`) to expose race conditions.
3. Run tests in random order using JUnit 5's `junit.jupiter.execution.order.random.seed` to detect order dependencies.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Tagging a Flaky Test for Quarantine (Easy)</summary>

**Task**: Decorate a test method with JUnit 5's `@Tag` annotation to mark it as `"flaky"` for CI exclusion.

**Solution**:
```java
@Test
@Tag("flaky")
void unstableLegacyNetworkTest() {
    // Quarantined test
}
```
</details>

<details>
<summary>Exercise 2: Non-Deterministic Collection Assertion Fix (Easy)</summary>

**Task**: Fix this fragile assertion so it passes regardless of order:
```java
assertThat(names).containsExactly("Alpha", "Beta", "Gamma");
```

**Solution**:
```java
assertThat(names).containsExactlyInAnyOrder("Alpha", "Beta", "Gamma");
```
</details>

<details>
<summary>Exercise 3: Freezing Time with Clock.fixed (Medium)</summary>

**Task**: Write a test that provides a fixed `Clock` set to `2026-11-24T10:00:00Z` to a `TokenValidator` and asserts that a token expiring at `2026-11-24T10:05:00Z` is recognized as valid.

**Solution**:
```java
@Test
void shouldValidateTokenWithFrozenClock() {
    Instant now = Instant.parse("2026-11-24T10:00:00Z");
    Clock fixedClock = Clock.fixed(now, ZoneOffset.UTC);

    TokenValidator validator = new TokenValidator(fixedClock);
    boolean valid = validator.isTokenActive(Instant.parse("2026-11-24T10:05:00Z"));

    assertThat(valid).isTrue();
}
```
</details>

<details>
<summary>Exercise 4: Replacing Thread.sleep with Awaitility (Medium)</summary>

**Task**: Refactor this flaky sleep-based test to use Awaitility:
```java
service.startTask();
Thread.sleep(2000);
assertThat(service.isCompleted()).isTrue();
```

**Solution**:
```java
service.startTask();
await().atMost(Duration.ofSeconds(5))
    .untilAsserted(() -> assertThat(service.isCompleted()).isTrue());
```
</details>

<details>
<summary>Exercise 5: Dynamic Port Injection for Parallel Isolation (Hard)</summary>

**Task**: Configure a `@SpringBootTest` that binds to a dynamic random port and injects the port into an HTTP client to guarantee parallel test isolation without port conflicts.

**Solution**:
```java
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class IsolatedParallelTest {

    @LocalServerPort
    private int port;

    @Autowired
    private RestClient.Builder clientBuilder;

    @Test
    void shouldExecuteWithoutPortCollisions() {
        RestClient client = clientBuilder.baseUrl("http://localhost:" + port).build();
        String response = client.get().uri("/health").retrieve().body(String.class);
        assertThat(response).contains("UP");
    }
}
```
</details>

<details>
<summary>Exercise 6: Repeated Stress Test for Concurrency Flakiness (Hard)</summary>

**Task**: Use JUnit 5's `@RepeatedTest` to run a concurrent service method 50 times in a test suite to detect race conditions before merging.

**Solution**:
```java
@RepeatedTest(50)
void shouldIncrementCounterConcurrentlyWithoutRaceConditions() {
    ConcurrentCounter counter = new ConcurrentCounter();
    IntStream.range(0, 100).parallel().forEach(i -> counter.increment());
    assertThat(counter.getValue()).isEqualTo(100);
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Never Sleep**: Replace `Thread.sleep()` with Awaitility polling.
- **Freeze Time with `Clock`**: Inject `java.time.Clock` into production services to eliminate timezone and timing flakiness.
- **Dynamic Ports Everywhere**: Use `RANDOM_PORT` and dynamic container ports to allow parallel test execution.
- **Order-Independent Assertions**: Use `containsExactlyInAnyOrder()` unless queries explicitly sort via `ORDER BY`.
- **Quarantine, Don't Mask**: Use `@Tag("flaky")` to isolate flaky tests while fixing root causes; never rely on permanent retries.

---

## 13. Further Reading
- [Google Testing Blog: Where Do Our Flaky Tests Come From?](https://testing.googleblog.com/2016/05/flaky-tests-at-google-fact-or-fiction.html)
- [Microsoft Research: An Empirical Analysis of Flaky Tests](https://www.microsoft.com/en-us/research/publication/an-empirical-analysis-of-flaky-tests/)
- [Awaitility Documentation](http://www.awaitility.org/)
- [JUnit 5 User Guide: Test Execution Order](https://junit.org/junit5/docs/current/user-guide/#writing-tests-test-execution-order)
- [Martin Fowler: Eradicating Non-Determinism in Tests](https://martinfowler.com/articles/non-determinism.html)
