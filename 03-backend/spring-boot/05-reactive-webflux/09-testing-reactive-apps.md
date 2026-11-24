# Testing Reactive Applications: StepVerifier, WebTestClient & PublisherProbe

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | `01-reactive-programming-and-reactor.md`, `02-webflux-architecture.md`, `08-webclient-deep-dive.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | `reactor-test:3.6.x`, `spring-boot-starter-test:3.4.x`, JUnit 5.11 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Basic Unit Testing with StepVerifier](#51-basic-unit-testing-with-stepverifier)
   - [5.2 Enterprise Integration Testing with WebTestClient](#52-enterprise-integration-testing-with-webtestclient)
   - [5.3 Virtual Time Testing for Long-Running Delays](#53-virtual-time-testing-for-long-running-delays)
   - [5.4 Anti-Pattern: Blocking in Reactive Tests](#54-anti-pattern-blocking-in-reactive-tests)
   - [5.5 Production Mocking with PublisherProbe](#55-production-mocking-with-publisherprobe)
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
Standard imperative unit test assertions (`assertEquals`, `assertThat`) rely on synchronous return values. In reactive architectures built on Project Reactor and Spring WebFlux, methods return non-blocking containers (`Mono<T>`, `Flux<T>`). Testing asynchronous publishers by calling `.block()` or `Thread.sleep()` introduces severe flakiness, breaks virtual thread schedulers, masks backpressure bugs, and causes unbounded CI test suite execution times when validating time-based operators like `delayElements`, `windowTimeout`, or `retryWhen`.

### Historical Evolution
In early reactive frameworks, engineers used `CountDownLatch` or manual `Subscriber` stubs with concurrent latch timeouts to assert emitted values. Project Reactor introduced `reactor-test` with `StepVerifier` and `PublisherProbe`, offering declarative, event-driven step assertions that simulate real subscriber backpressure contracts. Spring WebFlux complemented this with `WebTestClient`, a reactive test client that binds directly to WebFlux server pipelines or mock controllers without spinning up physical HTTP sockets.

### Real-World Relevance
Production microservices operating at high throughput require deterministic testing of backpressure, emission ordering, transient network retry policies, and error recovery sequences without delaying build pipelines. `StepVerifier` with virtual time manipulation turns a 24-hour delayed stream into a millisecond-duration test execution.

---

## 2. Core Concepts

### StepVerifier
A fluent DSL that acts as a `Subscriber` to the target `Publisher`. It defines expected lifecycle signals in chronological order: `expectSubscription()`, `expectNext()`, `expectError()`, `expectComplete()`, and terminates with `verify()`.

### Virtual Time
`StepVerifier.withVirtualTime()` installs Reactor's `VirtualTimeScheduler`, substituting the real-time clock with a virtual virtual-clock manipulated programmatically via `thenAwait(Duration)` to advance time instantaneously.

### PublisherProbe
A diagnostic wrapper around a `Mono` or `Flux` that tracks execution mechanics: whether it was subscribed to, how many times it was cancelled, and whether it completed empty or emitted items. It is essential for verifying fallback branches such as `switchIfEmpty()` without mutating downstream observers.

### WebTestClient
A non-blocking HTTP testing client designed for WebFlux. It can bind to:
1. Live running servers via `@SpringBootTest(webEnvironment = RANDOM_PORT)`.
2. Router functions or `@Controller` instances via mock server binds (`bindToController()`, `bindToRouterFunction()`).
3. ApplicationContext via `bindToApplicationContext()`.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                                StepVerifier Workflow                               |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Test Execution Thread          VirtualTimeScheduler             Target Publisher  |
|         |                                |                              |          |
|         | StepVerifier.create(flux)      |                              |          |
|         |-------------------------------------------------------------->|          |
|         |                                |                              | subscribe|
|         | expectNext("data")             |                              |          |
|         |------------------------------->|                              |          |
|         |                                |                              | onNext   |
|         |                                |<-----------------------------|          |
|         | thenAwait(Duration.ofDays(7))  |                              |          |
|         |------------------------------->| Advance Virtual Clock +7d    |          |
|         |                                | Trigger Timed Operators ---> |          |
|         |                                |                              | onNext   |
|         |                                |<-----------------------------|          |
|         | verifyComplete()               |                              | onComplete
|         |-------------------------------------------------------------->|          |
|         | Assert assertions succeeded    |                              |          |
|         v                                v                              v          |
+------------------------------------------------------------------------------------+
```

1. **Step Accumulation**: `StepVerifier.create()` records expected signal assertions into an internal verification queue.
2. **Subscription Initiation**: Calling `.verify()` triggers `publisher.subscribe(stepVerifierSubscriber)`.
3. **Signal Interception**: As the publisher publishes signals (`onSubscribe`, `onNext`, `onError`, `onComplete`), the internal subscriber dequeues the corresponding step assertion and executes assertion logic.
4. **Virtual Clock Interception**: `withVirtualTime()` sets `Schedulers.setFactory()` to delegate all timed schedulers (`parallel()`, `single()`, `boundedElastic()`) to `VirtualTimeScheduler`.

---

## 4. Syntax & Usage

### Maven Dependency Configuration
```xml
<dependency>
    <groupId>io.projectreactor</groupId>
    <artifactId>reactor-test</artifactId>
    <scope>test</scope>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-test</artifactId>
    <scope>test</scope>
</dependency>
```

### Essential StepVerifier DSL Operators
```java
StepVerifier.create(publisher)
    .expectSubscription()
    .expectNext(expectedItem)
    .expectNextMatches(item -> item.getId() != null)
    .expectNextCount(10)
    .expectError(CustomBusinessException.class)
    .expectErrorMessage("Account not found")
    .verify(Duration.ofSeconds(5)); // Always enforce an explicit assertion timeout
```

---

## 5. Practical Examples

### 5.1 Basic Unit Testing with StepVerifier
```java
package com.example.reactive.service;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import reactor.core.publisher.Flux;
import reactor.test.StepVerifier;

import java.time.Duration;

public class OrderStreamServiceTest {

    private Flux<String> getOrderStatuses() {
        return Flux.just("PENDING", "PROCESSING", "COMPLETED");
    }

    @Test
    @DisplayName("Verify sequential emission and completion of order statuses")
    void testOrderStatuses() {
        Flux<String> statusFlux = getOrderStatuses();

        StepVerifier.create(statusFlux)
            .expectSubscription()
            .expectNext("PENDING")
            .expectNext("PROCESSING")
            .expectNext("COMPLETED")
            .expectComplete()
            .verify(Duration.ofSeconds(2));
    }
}
```

### 5.2 Enterprise Integration Testing with WebTestClient
```java
package com.example.reactive.web;

import com.example.reactive.domain.Product;
import com.example.reactive.repository.ProductRepository;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.mockito.Mockito;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.reactive.WebFluxTest;
import org.springframework.boot.test.mock.mockito.MockBean;
import org.springframework.http.MediaType;
import org.springframework.test.web.reactive.server.WebTestClient;
import reactor.core.publisher.Flux;
import reactor.core.publisher.Mono;

import java.math.BigDecimal;

import static org.mockito.ArgumentMatchers.any;

@WebFluxTest(controllers = ProductController.class)
public class ProductControllerIntegrationTest {

    @Autowired
    private WebTestClient webTestClient;

    @MockBean
    private ProductRepository productRepository;

    @Test
    @DisplayName("GET /api/v1/products returns SSE stream of products")
    void testStreamProducts() {
        Product prod1 = new Product("101", "Mechanical Keyboard", BigDecimal.valueOf(149.99));
        Product prod2 = new Product("102", "Ergonomic Mouse", BigDecimal.valueOf(89.50));

        Mockito.when(productRepository.findAll()).thenReturn(Flux.just(prod1, prod2));

        webTestClient.get()
            .uri("/api/v1/products")
            .accept(MediaType.APPLICATION_NDJSON)
            .exchange()
            .expectStatus().isOk()
            .expectHeader().contentTypeCompatibleWith(MediaType.APPLICATION_NDJSON)
            .expectBodyList(Product.class)
            .hasSize(2)
            .contains(prod1, prod2);
    }

    @Test
    @DisplayName("POST /api/v1/products creates product and returns 201")
    void testCreateProduct() {
        Product newProd = new Product(null, "Desk Mat", BigDecimal.valueOf(25.00));
        Product savedProd = new Product("103", "Desk Mat", BigDecimal.valueOf(25.00));

        Mockito.when(productRepository.save(any(Product.class))).thenReturn(Mono.just(savedProd));

        webTestClient.post()
            .uri("/api/v1/products")
            .contentType(MediaType.APPLICATION_JSON)
            .bodyValue(newProd)
            .exchange()
            .expectStatus().isCreated()
            .expectBody()
            .jsonPath("$.id").isEqualTo("103")
            .jsonPath("$.name").isEqualTo("Desk Mat")
            .jsonPath("$.price").isEqualTo(25.00);
    }
}
```

### 5.3 Virtual Time Testing for Long-Running Delays
```java
package com.example.reactive.service;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import reactor.core.publisher.Flux;
import reactor.test.StepVerifier;

import java.time.Duration;

public class DelayedReminderServiceTest {

    public Flux<String> getReminderHeartbeat() {
        return Flux.interval(Duration.ofDays(1))
            .take(3)
            .map(tick -> "Heartbeat #" + tick);
    }

    @Test
    @DisplayName("Test 3-day emission sequence in 100 milliseconds using VirtualTime")
    void testVirtualTimeHeartbeat() {
        StepVerifier.withVirtualTime(this::getReminderHeartbeat)
            .expectSubscription()
            .expectNoEvent(Duration.ofDays(1))
            .thenAwait(Duration.ofDays(1))
            .expectNext("Heartbeat #0")
            .thenAwait(Duration.ofDays(1))
            .expectNext("Heartbeat #1")
            .thenAwait(Duration.ofDays(1))
            .expectNext("Heartbeat #2")
            .expectComplete()
            .verify(Duration.ofSeconds(1));
    }
}
```

### 5.4 Anti-Pattern: Blocking in Reactive Tests
```java
// =========================================================================
// ANTI-PATTERN: Calling .block() or .toStream() in Unit / Integration Tests
// =========================================================================
@Test
void antiPattern_blockingTest() {
    Mono<User> userMono = userService.findById("usr-123");
    
    // FAILS TO TEST: Backpressure contracts, cancellation signals, error channel semantics.
    // BLOCKS: Can deadlock tests if the publisher executes on non-blocking event loops.
    User user = userMono.block(); 
    assertEquals("Alice", user.getName());
}

// =========================================================================
// PRODUCTION FIX: Declarative Assertion with StepVerifier
// =========================================================================
@Test
void productionFix_stepVerifierTest() {
    Mono<User> userMono = userService.findById("usr-123");

    StepVerifier.create(userMono)
        .assertNext(user -> {
            assertThat(user.getId()).isEqualTo("usr-123");
            assertThat(user.getName()).isEqualTo("Alice");
        })
        .verifyComplete();
}
```

### 5.5 Production Mocking with PublisherProbe
```java
package com.example.reactive.service;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import reactor.core.publisher.Mono;
import reactor.test.StepVerifier;
import reactor.test.publisher.PublisherProbe;

import static org.assertj.core.api.Assertions.assertThat;

public class CacheFallbackServiceTest {

    static class DataService {
        private final CacheService cacheService;
        private final DatabaseService dbService;

        public DataService(CacheService cacheService, DatabaseService dbService) {
            this.cacheService = cacheService;
            this.dbService = dbService;
        }

        public Mono<String> resolveData(String key) {
            return cacheService.get(key)
                .switchIfEmpty(dbService.fetch(key));
        }
    }

    interface CacheService { Mono<String> get(String key); }
    interface DatabaseService { Mono<String> fetch(String key); }

    @Test
    @DisplayName("Verify fallback publisher is triggered when cache misses")
    void testCacheMissExecutesFallback() {
        PublisherProbe<String> cacheProbe = PublisherProbe.empty();
        PublisherProbe<String> dbProbe = PublisherProbe.of(Mono.just("Payload from DB"));

        DataService dataService = new DataService(
            key -> cacheProbe.mono(),
            key -> dbProbe.mono()
        );

        StepVerifier.create(dataService.resolveData("item-789"))
            .expectNext("Payload from DB")
            .verifyComplete();

        // Assert probe interactions
        cacheProbe.assertWasSubscribed();
        dbProbe.assertWasSubscribed();
        assertThat(dbProbe.subscribers()).isEqualTo(1);
    }

    @Test
    @DisplayName("Verify fallback publisher is NOT triggered when cache hits")
    void testCacheHitBypassesFallback() {
        PublisherProbe<String> cacheProbe = PublisherProbe.of(Mono.just("Cached Payload"));
        PublisherProbe<String> dbProbe = PublisherProbe.of(Mono.just("DB Payload"));

        DataService dataService = new DataService(
            key -> cacheProbe.mono(),
            key -> dbProbe.mono()
        );

        StepVerifier.create(dataService.resolveData("item-789"))
            .expectNext("Cached Payload")
            .verifyComplete();

        cacheProbe.assertWasSubscribed();
        dbProbe.assertWasNotSubscribed();
    }
}
```

---

## 6. Best Practices

1. **Always Supply an Explicit Timeout to `.verify()`**: Invoking `.verify()` with no parameters blocks until completion, which can hang CI pipelines indefinitely if a publisher leaks or stalls. Always pass `.verify(Duration.ofSeconds(5))`.
2. **Use `Supplier<Publisher<T>>` with Virtual Time**: In `StepVerifier.withVirtualTime(() -> targetPublisher)`, always wrap creation in a lambda so the publisher is constructed *after* virtual schedulers are initialized.
3. **Verify Exact Element Counts**: When verifying large streams, use `expectNextCount(n)` instead of repeating `expectNext()` $n$ times.
4. **Assert Consumer Cancellation**: Use `thenCancel().verify()` to assert that infinite streams (`Flux.interval()`) properly clean up resources when consumers cancel subscriptions.
5. **Enforce Error Type and Message Matchers**: Never assert just `expectError()`; specify `expectError(ExpectedException.class)` or `expectErrorMatches(t -> t.getMessage().contains("expected"))`.
6. **Prefer `WebTestClient.bindToController()` for Unit Slices**: Avoid the overhead of starting complete Netty/Tomcat server containers when testing isolated controller mapping and serialization logic.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Forgetting to call `.verify()`** | `StepVerifier` defines assertions lazily; without `.verify()`, no subscriber attaches and the test silently passes. | Always terminate chains with `.verify()`, `.verifyComplete()`, or `.verifyError()`. |
| **Instantiating Publisher Outside `withVirtualTime`** | If `Flux.interval()` is created before `withVirtualTime()`, it binds to system schedulers rather than `VirtualTimeScheduler`. | Pass publisher instantiation inside the `Supplier` argument: `StepVerifier.withVirtualTime(() -> myService.stream())`. |
| **Using `block()` inside `WebTestClient` calls** | `WebTestClient` is inherently non-blocking; mixing `.block()` creates thread synchronization contention. | Use `WebTestClient` assertions: `.expectStatus()`, `.expectBody()`, `.returnResult()`. |
| **Failing to test backpressure requests** | Stream publishers may emit uncontrollably when subscribers request `Long.MAX_VALUE`. | Use `StepVerifier.create(flux, initialRequest)` and `thenRequest(n)` to test flow control. |
| **Overlapping Virtual Time increments** | Calling `thenAwait()` without verifying intermediate signals can cause events to fire in unexpected batch groupings. | Step through emission increments sequentially with `expectNoEvent()` followed by `thenAwait()`. |
| **Mocking `Mono`/`Flux` return types as `null`** | Returning `null` from service methods violates WebFlux reactive contracts and causes NPEs. | Always mock empty states with `Mono.empty()` or `Flux.empty()`. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Socket Overhead**: `WebTestClient.bindToApplicationContext()` bypasses network I/O, loopback interfaces, and TCP handshakes, executing controller pipelines in-memory via mock reactive exchange instances.
- **Microsecond Test Suites**: Virtual time testing replaces physical thread park/sleep cycles with deterministic state transitions, shortening multi-hour integration sequences to sub-second test executions.

### Security
- **Reactive Security Context Testing**: When validating authorization with `WebTestClient`, pass credentials via `.mutateWith(SecurityMockServerConfigurers.csrf())` and `.mutateWith(SecurityMockServerConfigurers.mockUser())`.
- **Payload Sanitization Verification**: Test error response pipelines to ensure that stack traces and internal identifiers are suppressed in `ProblemDetail` structures.

---

## 9. Comparison / When to Use What

| Feature / Tool | StepVerifier | WebTestClient | TestRestTemplate |
| :--- | :--- | :--- | :--- |
| **Primary Scope** | Reactive publishers (`Mono`, `Flux`) | WebFlux controllers, routes, HTTP streams | Blocking Spring MVC endpoints |
| **Execution Model** | In-process Subscriber assertions | Non-blocking HTTP / In-memory pipeline | Synchronous HTTP socket connection |
| **Streaming (SSE/NDJSON)** | Direct event stream consumption | Supported via `returnResult(Class)` | Unsuitable for long-lived streams |
| **Time Manipulation** | Supported (`withVirtualTime`) | Not supported | Not supported |
| **Typical Target** | Service, Repository, Client layers | Controller, Router, Filter layers | Full-stack E2E black-box testing |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does calling <code>.block()</code> in reactive unit tests constitute an anti-pattern?</strong></summary>

Calling `.block()` forces the asynchronous execution onto the current test thread, violating the Reactive Streams specification. It conceals backpressure contract violations, prevents verification of cancellation handling, masks concurrency deadlocks, and prevents tests from running against non-blocking event-loop schedulers. Furthermore, if the code internally schedules work on the same thread, `.block()` can trigger permanent deadlocks.
</details>

<details>
<summary><strong>2. How does <code>VirtualTimeScheduler</code> work in Project Reactor?</strong></summary>

`VirtualTimeScheduler` replaces standard Reactor schedulers (`Schedulers.parallel()`, etc.) by registering a custom factory with `Schedulers.setFactory()`. It implements an internal virtual clock that does not tick automatically. Time is manually advanced by invoking `thenAwait(Duration)`, which iterates through scheduled delayed tasks and triggers their execution immediately without real-world latency.
</details>

<details>
<summary><strong>3. How do you test backpressure behavior using <code>StepVerifier</code>?</strong></summary>

`StepVerifier.create(publisher, initialRequest)` allows developers to define the initial subscriber demand (e.g., `0` or `1`). By pairing `thenRequest(n)` with `expectNext()`, tests assert that the publisher respects downstream consumer demand and does not emit signals before they are requested.
</details>

<details>
<summary><strong>4. What is the difference between <code>expectNext()</code> and <code>assertNext()</code>?</strong></summary>

`expectNext(T expected)` performs equality comparison (`equals()`) against the emitted item. `assertNext(Consumer<T> assertionConsumer)` provides the emitted item to a consumer, enabling flexible assertions (such as AssertJ assertions, checking nested properties, or verifying non-deterministic IDs).
</details>

<details>
<summary><strong>5. What purpose does <code>PublisherProbe</code> serve when mocking reactive dependencies?</strong></summary>

`PublisherProbe` wraps a publisher to monitor its lifecycle without modifying stream output. It tracks whether the probe was subscribed to, how many subscribers connected, whether it was cancelled, and whether it completed. It is critical for asserting whether execution branches like `switchIfEmpty()` or fallback error paths were triggered.
</details>

<details>
<summary><strong>6. How can you test Server-Sent Events (SSE) or NDJSON streaming using <code>WebTestClient</code>?</strong></summary>

You invoke `.accept(MediaType.TEXT_EVENT_STREAM)` or `.accept(MediaType.APPLICATION_NDJSON)`, call `.exchange()`, and verify the body using `.returnResult(TargetClass.class).getResponseBody()`. The resulting `Flux<TargetClass>` can then be passed into `StepVerifier.create()` to assert streamed elements as they arrive.
</details>

<details>
<summary><strong>7. Why must publishers be supplied via a lambda in <code>StepVerifier.withVirtualTime()</code>?</strong></summary>

If the publisher is instantiated before `withVirtualTime()` is called, time-based operators (`delayElements`, `interval`) capture the system default `Schedulers.parallel()` instance during assembly time. Supplying a `Supplier<Publisher<T>>` ensures that the publisher is assembled *after* `VirtualTimeScheduler` is globally installed.
</details>

<details>
<summary><strong>8. How do you mock authenticated security contexts when testing WebFlux controllers?</strong></summary>

Using `spring-security-test`, mutate the request chain with `.mutateWith(SecurityMockServerConfigurers.mockUser("admin").roles("ADMIN"))` or `.mutateWith(SecurityMockServerConfigurers.mockJwt())`. This injects a mock `SecurityContext` directly into the reactive `ServerWebExchange`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Flux StepVerifier Assertion</strong></summary>

**Task**: Write a unit test using `StepVerifier` to verify a method that returns a `Flux<Integer>` emitting numbers 1 through 5, asserting each element and verifying normal completion within 2 seconds.

```java
@Test
void verifyNumberFlux() {
    Flux<Integer> flux = Flux.range(1, 5);

    StepVerifier.create(flux)
        .expectNext(1, 2, 3, 4, 5)
        .expectComplete()
        .verify(Duration.ofSeconds(2));
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Assert Error Signal</strong></summary>

**Task**: Create a test asserting that a `Mono` emitting an `IllegalArgumentException` with the message `"Invalid ID"` correctly triggers the error channel.

```java
@Test
void verifyErrorSignal() {
    Mono<String> errorMono = Mono.error(new IllegalArgumentException("Invalid ID"));

    StepVerifier.create(errorMono)
        .expectErrorMatches(throwable -> 
            throwable instanceof IllegalArgumentException && 
            "Invalid ID".equals(throwable.getMessage()))
        .verify(Duration.ofSeconds(1));
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Virtual Time Interval Verification</strong></summary>

**Task**: Write a unit test verifying a service emitting items every 10 seconds (`Flux.interval(Duration.ofSeconds(10))`), taking 3 elements, executing in under 200 milliseconds using `withVirtualTime`.

```java
@Test
void verifyVirtualTimeInterval() {
    StepVerifier.withVirtualTime(() -> Flux.interval(Duration.ofSeconds(10)).take(3))
        .expectSubscription()
        .thenAwait(Duration.ofSeconds(10))
        .expectNext(0L)
        .thenAwait(Duration.ofSeconds(10))
        .expectNext(1L)
        .thenAwait(Duration.ofSeconds(10))
        .expectNext(2L)
        .expectComplete()
        .verify(Duration.ofMillis(500));
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Testing Reactive Backpressure Request Batches</strong></summary>

**Task**: Test backpressure on a range flux of 10 items by requesting 2 items, verifying them, then requesting 8 items and verifying stream completion.

```java
@Test
void verifyBackpressureBatches() {
    Flux<Integer> flux = Flux.range(1, 10);

    StepVerifier.create(flux, 0)
        .expectSubscription()
        .thenRequest(2)
        .expectNext(1, 2)
        .thenRequest(8)
        .expectNext(3, 4, 5, 6, 7, 8, 9, 10)
        .expectComplete()
        .verify(Duration.ofSeconds(2));
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Mocking SwitchIfEmpty with PublisherProbe</strong></summary>

**Task**: Implement a test for a resilient account loader that checks Redis cache first, falls back to Postgres on cache miss, and verify using `PublisherProbe` that the database probe was subscribed to only when the cache probe is empty.

```java
@Test
void verifyFallbackWithPublisherProbe() {
    PublisherProbe<Account> cacheProbe = PublisherProbe.empty();
    PublisherProbe<Account> dbProbe = PublisherProbe.of(Mono.just(new Account("acc-1", "SAVINGS")));

    AccountService service = new AccountService(id -> cacheProbe.mono(), id -> dbProbe.mono());

    StepVerifier.create(service.findAccount("acc-1"))
        .assertNext(acc -> assertThat(acc.type()).isEqualTo("SAVINGS"))
        .verifyComplete();

    cacheProbe.assertWasSubscribed();
    dbProbe.assertWasSubscribed();
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Streaming WebTestClient Controller Test with Dynamic StepVerifier</strong></summary>

**Task**: Write an integration test using `WebTestClient` against an SSE endpoint `/prices/stream`, extracting the response body as a `Flux<Price>` and asserting 3 incoming prices with `StepVerifier`.

```java
@Test
void verifySseEndpointStream() {
    Flux<Price> priceFlux = webTestClient.get()
        .uri("/prices/stream")
        .accept(MediaType.TEXT_EVENT_STREAM)
        .exchange()
        .expectStatus().isOk()
        .returnResult(Price.class)
        .getResponseBody();

    StepVerifier.create(priceFlux.take(3))
        .expectNextMatches(p -> p.amount().compareTo(BigDecimal.ZERO) > 0)
        .expectNextMatches(p -> p.amount().compareTo(BigDecimal.ZERO) > 0)
        .expectNextMatches(p -> p.amount().compareTo(BigDecimal.ZERO) > 0)
        .expectComplete()
        .verify(Duration.ofSeconds(5));
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| StepVerifier: StepVerifier.create(pub).expectNext(v).verifyComplete()                 |
| Virtual Time: StepVerifier.withVirtualTime(() -> pub).thenAwait(d).verifyComplete()   |
| Probing:      PublisherProbe.of(mono); probe.assertWasSubscribed()                    |
| Flow Control: StepVerifier.create(pub, 0).thenRequest(n).expectNext(...)              |
| Web Client:   webTestClient.get().uri(...).exchange().expectStatus().isOk()          |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Project Reactor Testing Reference Documentation](https://projectreactor.io/docs/core/release/reference/#testing)
- [Spring Framework WebTestClient Guide](https://docs.spring.io/spring-framework/reference/testing/webtestclient.html)
- [Reactive Streams TCK Specification](https://github.com/reactive-streams/reactive-streams-jvm/tree/master/tck)
- [Testing Asynchronous Architectures: Project Reactor In-Depth](https://spring.io/blog/2017/04/18/reactor-testing-with-stepverifier)
