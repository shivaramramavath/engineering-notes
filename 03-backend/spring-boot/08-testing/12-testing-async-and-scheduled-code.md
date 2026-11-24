# 12. Testing Asynchronous & Scheduled Code (Awaitility)

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | `01-testing-fundamentals-in-spring.md`, Java Concurrency, `@Async`, `@Scheduled` |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, Awaitility 4.2 |

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
Testing code that executes asynchronously presents a major concurrency challenge in automated testing:
- **`@Async` methods** return control immediately to the calling thread (often returning `void` or an uncompleted `CompletableFuture`), causing test assertions to execute before the background thread has finished processing.
- **`@Scheduled` tasks** execute on periodic background timers (e.g., cron triggers or fixed delays). Waiting for a cron trigger to fire during test execution is slow and unpredictable.
- Developers frequently resort to **`Thread.sleep(5000)`**, which is a severe anti-pattern:
  - If the sleep duration is too short, tests fail intermittently on slower CI runners (flaky tests).
  - If the sleep duration is too long, test suite execution times balloon, wasting thousands of developer hours over a project's lifecycle.

**Awaitility** is an expressive, asynchronous assertion library designed to synchronize tests with background threads. It polls assertions at configurable intervals, succeeding the millisecond the condition is met and failing gracefully with a timeout if it is not.

### Historical Evolution
- **Early Java Testing**: Relied on manual `CountDownLatch`, `Thread.sleep()`, or busy-wait `while (!condition)` loops with timeout counters.
- **Awaitility (2010-Present)**: Standardized fluent asynchronous DSL (`await().atMost(...).until(...)`), providing rich polling intervals, condition evaluations, and exception suppression.
- **Modern Spring Boot 3.x/4.x**: Full compatibility with Java 21 Virtual Threads, Project Reactor, and Spring's `ThreadPoolTaskExecutor`.

### Real-World Relevance
Reliably testing background email dispatchers, payment webhooks, async event processors, and periodic database cleanup tasks is mandatory for maintaining zero-flakiness test suites.

---

## 2. Core Concepts

### Architectural Contracts
- **`org.awaitility.Awaitility`**: The static entry point providing fluent methods (`await()`, `given()`, `waitAtMost()`).
- **Condition**: A boolean supplier (`until(Callable<Boolean>)`) or an AssertJ assertion block (`untilAsserted(ThrowingRunnable)`).
- **Timeout**: The maximum duration Awaitility will wait before failing the test with a `ConditionTimeoutException` (default: 10 seconds).
- **Poll Interval**: How frequently Awaitility evaluates the condition (default: 100 milliseconds).
- **Poll Delay**: An optional initial delay before the first polling cycle begins.
- **Exception Ignoring**: Ability to suppress expected transient exceptions (e.g., `EmptyResultDataAccessException`) while waiting for asynchronous state to converge.

### The Mental Model
```
[ JUnit 5 Test Thread ]                      [ Background Worker Thread ]
          │                                                │
          ├── 1. Invokes service.processAsync() ──────────►│ Starts long task
          │                                                │
          ├── 2. await().atMost(5s).untilAsserted(...)     │
          │      ├─ Poll at 0ms: Condition FALSE ◄─────────┼─ In progress...
          │      ├─ Poll at 100ms: Condition FALSE ◄───────┼─ In progress...
          │      ├─ Poll at 200ms: Condition TRUE! ◄───────┼─ COMPLETED!
          │
          └── 3. Test succeeds instantly at 200ms!
                 (No need to wait for a fixed 5-second sleep!)
```

### Invariants
1. `Thread.sleep()` should **never** be used in automated tests; always use Awaitility's polling mechanism.
2. The maximum timeout should be generous (e.g., 5 to 10 seconds) to avoid flakiness on overloaded CI runners, while the poll interval should be short (e.g., 50 to 100ms) for fast completion.
3. Assertions evaluated inside `untilAsserted()` must be idempotent and side-effect free.

---

## 3. How It Works Internally

### The Polling Engine
When `await().atMost(timeout).untilAsserted(assertionBlock)` is executed:
1. Records the start timestamp.
2. Waits for any configured initial poll delay.
3. Executes the assertion block on the current test thread.
4. If an `AssertionError` is caught:
   - Checks if elapsed time exceeds the configured timeout.
   - If timed out, throws `ConditionTimeoutException` with diagnostic details.
   - If not timed out, parks the thread for the configured poll interval (default 100ms).
   - Repeats polling.
5. If the assertion block succeeds without throwing an exception, Awaitility terminates immediately and the test passes.

---

## 4. Syntax & Usage

### 1. Minimal Maven Dependency (`pom.xml`)
```xml
<dependency>
    <groupId>org.awaitility</groupId>
    <artifactId>awaitility</artifactId>
    <version>4.2.2</version>
    <scope>test</scope>
</dependency>
```

### 2. Standard `untilAsserted` Syntax with AssertJ
```java
package com.example.async;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;

import java.time.Duration;

import static org.assertj.core.api.Assertions.assertThat;
import static org.awaitility.Awaitility.await;

@SpringBootTest
class AsyncNotificationTest {

    @Autowired
    private NotificationDispatcher dispatcher;

    @Autowired
    private NotificationRepository repository;

    @Test
    void shouldDispatchNotificationAsynchronously() {
        dispatcher.dispatchAsync("user@example.com", "Welcome!");

        // Polls every 100ms until condition succeeds or 5s timeout elapses
        await().atMost(Duration.ofSeconds(5))
            .pollInterval(Duration.ofMillis(100))
            .untilAsserted(() -> {
                var notifications = repository.findByRecipient("user@example.com");
                assertThat(notifications).hasSize(1);
                assertThat(notifications.get(0).getStatus()).isEqualTo("SENT");
            });
    }
}
```

---

## 5. Practical Examples

### Example 1: Testing `@Async` Service Methods
Testing a background calculation method that writes results to an in-memory or database store.

```java
package com.example.service;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;

import java.time.Duration;

import static org.assertj.core.api.Assertions.assertThat;
import static org.awaitility.Awaitility.await;

@SpringBootTest
class ReportGenerationServiceTest {

    @Autowired
    private ReportService reportService;

    @Autowired
    private ReportRepository reportRepository;

    @Test
    void shouldGenerateReportInBackgroundThread() {
        String reportId = reportService.triggerAsyncReportGeneration("Q4-FINANCIALS");

        await().atMost(Duration.ofSeconds(3))
            .untilAsserted(() -> {
                Report report = reportRepository.findById(reportId).orElse(null);
                assertThat(report).isNotNull();
                assertThat(report.isFinished()).isTrue();
                assertThat(report.getRecordCount()).isPositive();
            });
    }
}
```

### Example 2: Testing Scheduled Background Tasks (`@Scheduled`)
Testing a periodic cleanup task that purges expired sessions.

```java
package com.example.scheduler;

import com.example.repository.SessionRepository;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.TestPropertySource;

import java.time.Duration;

import static org.assertj.core.api.Assertions.assertThat;
import static org.awaitility.Awaitility.await;

@SpringBootTest
@TestPropertySource(properties = {
    // Override production cron with rapid fixed delay for fast test execution!
    "app.scheduler.cleanup.fixed-delay=200"
})
class SessionPurgeSchedulerTest {

    @Autowired
    private SessionRepository sessionRepository;

    @Test
    void shouldPurgeExpiredSessionsPeriodically() {
        sessionRepository.createExpiredSession("SESS-OLD");

        await().atMost(Duration.ofSeconds(2))
            .untilAsserted(() -> {
                assertThat(sessionRepository.existsById("SESS-OLD")).isFalse();
            });
    }
}
```

### Example 3: Edge Case – Suppressing Transient Exceptions
Handling cases where the database or query throws a temporary exception while the background worker is writing data.

```java
package com.example.async;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.dao.EmptyResultDataAccessException;

import java.time.Duration;

import static org.assertj.core.api.Assertions.assertThat;
import static org.awaitility.Awaitility.await;

@SpringBootTest
class TransientExceptionHandlingTest {

    @Autowired
    private AsyncDataSink dataSink;

    @Test
    void shouldIgnoreTransientExceptionsDuringPolling() {
        dataSink.processDataAsync("item-42");

        await().atMost(Duration.ofSeconds(4))
            .pollInterval(Duration.ofMillis(50))
            // Suppress transient query exception while row is being inserted
            .ignoreExceptionsInstanceOf(EmptyResultDataAccessException.class)
            .untilAsserted(() -> {
                assertThat(dataSink.queryProcessedItem("item-42")).isNotNull();
            });
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Using `Thread.sleep(5000)` in tests to wait for asynchronous processing.

```java
// SEVERE ANTI-PATTERN:
// 1. Slows down test suite by 5,000ms every time!
// 2. Still flaky if CI agent lags and takes 5,001ms!
@Test
void testAsync() throws InterruptedException {
    service.doAsync();
    Thread.sleep(5000); // NEVER DO THIS!
    assertThat(repo.isDone()).isTrue();
}
```

**Production Fix**:
Use Awaitility's `untilAsserted()`. Passes in 50ms if the task finishes quickly, and only waits up to the timeout if it takes longer.

```java
// FAST & RELIABLE: Finishes in 50ms on fast machines, waits up to 5s on slow CI!
@Test
void testAsync() {
    service.doAsync();
    await().atMost(Duration.ofSeconds(5))
        .untilAsserted(() -> assertThat(repo.isDone()).isTrue());
}
```

### Example 5: High-Performance Virtual Thread Async Testing
Testing asynchronous task execution backed by Java 21 Virtual Threads.

```java
package com.example.async;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.TestPropertySource;

import java.time.Duration;
import java.util.concurrent.atomic.AtomicInteger;

import static org.assertj.core.api.Assertions.assertThat;
import static org.awaitility.Awaitility.await;

@SpringBootTest
@TestPropertySource(properties = "spring.threads.virtual.enabled=true")
class VirtualThreadAsyncTest {

    private final AtomicInteger counter = new AtomicInteger(0);

    @Test
    void shouldExecuteOnVirtualThreadWithoutCarrierPinning() {
        Thread.startVirtualThread(() -> {
            try {
                Thread.sleep(50);
                counter.incrementAndGet();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        });

        await().atMost(Duration.ofSeconds(1))
            .untilAsserted(() -> assertThat(counter.get()).isEqualTo(1));
    }
}
```

---

## 6. Best Practices

1. **Always Use `untilAsserted` with AssertJ**: Prefer `untilAsserted(() -> assertThat(...))` over boolean `until(() -> ...)` because AssertJ prints the exact failing values on timeout.
2. **Never Use `Thread.sleep()`**: Awaitility finishes as soon as the condition is satisfied, making tests run orders of magnitude faster.
3. **Override Production Cron Schedules for Tests**: Use `@TestPropertySource` to replace 24-hour cron expressions with rapid fixed delays (e.g., 200ms) during test runs.
4. **Configure Reasonable Poll Intervals**: A poll interval of 50–100ms provides fast reaction times without saturating CPU cores.
5. **Set Generous Timeouts for CI**: Use 5 to 10 seconds for timeouts. Fast machines will finish in milliseconds; the high timeout prevents false-positive timeouts on busy CI build runners.
6. **Ignore Expected Transient Exceptions**: Use `.ignoreExceptionsInstanceOf(...)` to prevent early termination when polling resources that are temporarily absent during creation.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Using `Thread.sleep()` | Wasteful test latency and inevitable flakiness on slower CI runners. | Replace with `await().atMost(...).untilAsserted(...)`. |
| Boolean `until()` without diagnostic | On timeout, error message only says `Condition was not fulfilled`, with no clue what value was found. | Use `untilAsserted()` with AssertJ; outputs expected vs actual values. |
| Overly tight timeouts (e.g., 200ms) | Tests pass locally but fail intermittently on slow CI build agents. | Set timeouts to at least 3–5 seconds; tests still finish in milliseconds when fast. |
| Polling without interval throttling | Loops continuously, burning 100% CPU on polling. | Set `.pollInterval(Duration.ofMillis(100))`. |
| Modifying state inside the assertion block | Re-evaluating the assertion mutates data repeatedly, causing non-deterministic tests. | Ensure assertion blocks are strictly read-only. |
| Testing cron tasks with production schedules | Tests wait forever because cron runs once at midnight. | Override cron with fixed delay via `@TestPropertySource`. |

---

## 8. Performance & Security Notes

- **Zero Unnecessary Waiting**: If an async task finishes in 15ms, Awaitility terminates the wait at 15ms. In a suite of 200 async tests, this saves tens of minutes compared to fixed sleeps.
- **CPU Overhead of Polling**: With a 100ms poll interval, Awaitility executes at most 10 checks per second, creating near-zero CPU overhead.
- **Virtual Threads Integration**: Awaitility's internal thread parking leverages `LockSupport.parkNanos()`, which unmounts Java 21 Virtual Threads cleanly without carrier thread starvation.

---

## 9. Comparison / When to Use What

| Synchronization Tool | Dynamic Waiting? | Diagnostic Output | Thread Safe? | Best Use Case |
|---|---|---|---|---|
| **Awaitility** | **Yes (Finishes immediately)** | **Rich (AssertJ failure details)**| **Yes** | Asynchronous tests, scheduled tasks, queues |
| **`Thread.sleep()`** | No (Waits full duration) | None | N/A | Never in tests |
| **`CountDownLatch`** | Yes (Via latch count) | Poor | Yes | Parallel thread coordination |
| **`CompletableFuture.get()`**| Yes (Blocks until done) | Poor | Yes | Direct future return values |

---

## 10. Interview Questions

<details>
<summary>1. Why is `Thread.sleep()` considered a severe anti-pattern in automated testing?</summary>

**Answer:**
`Thread.sleep()` introduces two major flaws:
1. **Wasted Execution Time**: It forces the test runner to wait for the entire sleep duration even if the background operation completed within 5 milliseconds, drastically slowing down build pipelines.
2. **Test Flakiness**: If an overloaded CI build runner takes 1 millisecond longer than the hardcoded sleep duration, the test fails intermittently, creating false-positive build failures. Awaitility solves both problems by polling dynamically.
</details>

<details>
<summary>2. What is the advantage of `untilAsserted()` over `until()` in Awaitility?</summary>

**Answer:**
When using `until(Callable<Boolean>)`, if the condition times out, Awaitility only reports that the boolean condition was not satisfied, providing no information about why it failed.
When using `untilAsserted(ThrowingRunnable)`, you write standard AssertJ assertions. If it times out, Awaitility prints the exact assertion failure (e.g., `expected: "COMPLETED" but was: "IN_PROGRESS"`), making debugging instantaneous.
</details>

<details>
<summary>3. How do you test a Spring `@Scheduled` task that is configured to run once per day in production?</summary>

**Answer:**
Externalize the schedule configuration in `application.yml` using a property (e.g., `app.cron.schedule=${SCHEDULE_CRON:0 0 0 * * ?}`). In your test class, use `@TestPropertySource(properties = "app.cron.schedule=* * * * * ?")` or replace it with a rapid fixed delay (e.g., `app.scheduler.fixed-delay=100`) so the scheduled task fires immediately during test execution.
</details>

<details>
<summary>4. What happens if an exception is thrown inside an Awaitility `untilAsserted` block?</summary>

**Answer:**
By default, any `AssertionError` is caught and treated as a non-fatal failure; Awaitility parks the thread for the poll interval and retries until the timeout. If an unexpected `RuntimeException` (such as `NullPointerException`) is thrown, Awaitility fails immediately unless that exception type was explicitly configured via `.ignoreExceptionsInstanceOf(...)`.
</details>

<details>
<summary>5. How can you configure a global default timeout for Awaitility across an entire project?</summary>

**Answer:**
You can set global defaults statically before test execution (e.g., in a base class or `@BeforeAll` block):
```java
Awaitility.setDefaultTimeout(Duration.ofSeconds(5));
Awaitility.setDefaultPollInterval(Duration.ofMillis(50));
```
All subsequent calls to `await()` inherit these defaults.
</details>

<details>
<summary>6. How does Awaitility interact with Java 21 Virtual Threads?</summary>

**Answer:**
Awaitility is fully compatible with Virtual Threads. Its polling pauses utilize `LockSupport.parkNanos()`, which cleanly unmounts virtual threads from their carrier worker threads. This allows thousands of concurrent asynchronous tests to run in parallel without thread starvation.
</details>

<details>
<summary>7. Can Awaitility verify that a condition NEVER happens (e.g., verifying an event is not fired)?</summary>

**Answer:**
Yes, using `during()`:
```java
await().during(Duration.ofSeconds(2))
    .atMost(Duration.ofSeconds(3))
    .until(() -> eventQueue.isEmpty());
```
This asserts that the queue remains empty continuously throughout the 2-second monitoring window.
</details>

<details>
<summary>8. How do you test an `@Async` method that returns `CompletableFuture<T>`?</summary>

**Answer:**
If the method returns a `CompletableFuture`, you can assert on the future directly using AssertJ's `CompletableFuture` support:
```java
CompletableFuture<String> future = service.computeAsync();
assertThat(future).succeedsWithin(Duration.ofSeconds(3)).isEqualTo("Expected Result");
```
For methods returning `void` (where no future handle exists), use Awaitility to poll the resulting side effect.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic Awaitility Assertion (Easy)</summary>

**Task**: Write an Awaitility check verifying that an `AtomicBoolean isReady` becomes `true` within 3 seconds.

**Solution**:
```java
AtomicBoolean isReady = new AtomicBoolean(false);
// Trigger background thread...
await().atMost(Duration.ofSeconds(3))
    .untilTrue(isReady);
```
</details>

<details>
<summary>Exercise 2: AssertJ untilAsserted Verification (Easy)</summary>

**Task**: Poll an in-memory list `messageQueue` until it contains at least 3 items using `untilAsserted`.

**Solution**:
```java
await().atMost(Duration.ofSeconds(2))
    .untilAsserted(() -> assertThat(messageQueue).hasSizeGreaterThanOrEqualTo(3));
```
</details>

<details>
<summary>Exercise 3: Ignoring Specific Exceptions (Medium)</summary>

**Task**: Configure Awaitility to ignore `IndexOutOfBoundsException` while polling a list that is being populated by a worker thread.

**Solution**:
```java
await().atMost(Duration.ofSeconds(3))
    .ignoreExceptionsInstanceOf(IndexOutOfBoundsException.class)
    .untilAsserted(() -> assertThat(list.get(0)).isEqualTo("First Item"));
```
</details>

<details>
<summary>Exercise 4: Custom Polling Interval (Medium)</summary>

**Task**: Configure Awaitility to wait at most 4 seconds, polling every 50 milliseconds, until a database record has status `"ACTIVE"`.

**Solution**:
```java
await().atMost(Duration.ofSeconds(4))
    .pollInterval(Duration.ofMillis(50))
    .untilAsserted(() -> {
        Account account = accountRepository.findById(1L).orElseThrow();
        assertThat(account.getStatus()).isEqualTo("ACTIVE");
    });
```
</details>

<details>
<summary>Exercise 5: Testing `@Scheduled` Task Execution (Hard)</summary>

**Task**: Write an integration test for a Spring `@Scheduled` task that increments a metric counter, verifying that the counter reaches 2 within 2 seconds.

**Solution**:
```java
@SpringBootTest
@TestPropertySource(properties = "app.cron.interval=200")
class ScheduledTaskTest {

    @Autowired
    private MetricService metricService;

    @Test
    void shouldIncrementMetricCounterPeriodically() {
        await().atMost(Duration.ofSeconds(2))
            .untilAsserted(() -> assertThat(metricService.getExecutionCount()).isGreaterThanOrEqualTo(2));
    }
}
```
</details>

<details>
<summary>Exercise 6: Asserting Condition Remains True Over Duration (Hard)</summary>

**Task**: Write an Awaitility test verifying that a dead-letter queue remains completely empty continuously for at least 1.5 seconds.

**Solution**:
```java
@Test
void deadLetterQueueShouldRemainEmpty() {
    await().during(Duration.ofMillis(1500))
        .atMost(Duration.ofSeconds(2))
        .until(() -> deadLetterQueue.isEmpty());
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Banish `Thread.sleep()`**: Always synchronize asynchronous tests with Awaitility.
- **Use `untilAsserted` with AssertJ**: Provides rich diagnostics and prints actual vs expected values upon timeout.
- **Tune Poll Intervals**: 50–100ms intervals provide instant reaction times with minimal CPU overhead.
- **Generous Timeouts for CI**: Use 5-second timeouts to prevent false-positive failures on slow build runners.
- **Override Scheduled Timers**: Replace production cron expressions with rapid fixed delays in test properties.

---

## 13. Further Reading
- [Awaitility Official User Guide](http://www.awaitility.org/)
- [Spring Framework Reference: Asynchronous Execution](https://docs.spring.io/spring-framework/reference/integration/scheduling.html#scheduling-annotation-support-async)
- [Spring Framework Reference: Scheduled Tasks](https://docs.spring.io/spring-framework/reference/integration/scheduling.html#scheduling-annotation-support-scheduled)
- [Baeldung: Guide to Awaitility](https://www.baeldung.com/awaitility-testing)
- [Java Concurrency in Practice: Testing Concurrent Programs](https://jcip.net/)
